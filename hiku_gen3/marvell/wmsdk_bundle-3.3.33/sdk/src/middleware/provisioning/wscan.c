/*
 *  Copyright 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

/** wscan.c: Enqueue wlan_scan in system queue for periodic scan results
 */

#include <work-queue.h>
#include <system-work-queue.h>
#include <wlan.h>

#include "provisioning_int.h"

#define PERIODIC_SCAN_TIME       5000
#define DEFAULT_SCAN_PROTECTION_TIMEOUT 1000

static wq_handle_t wq_handle;
static job_handle_t job_handle;
static os_semaphore_t wscan_protection_sem;
/* This instance is used to hold scan results */
static struct site_survey wscan_survey;

/* WLAN Connection Manager scan results callback */
static int wscan_handle_scan_results(unsigned int count)
{
	int i, j = 0;

	/* WLAN firmware does not provide Noise Floor (NF) value with
	 * every scan result. We call the function below to update a
	 * global NF value so that /sys/scan can provide the latest NF value */
	wlan_get_current_nf();

	/* lock the scan results */
	if (os_mutex_get(&wscan_survey.lock, OS_WAIT_FOREVER) != WM_SUCCESS)
		return 0;

	/* clear out and refresh the site survey table */
	memset(wscan_survey.sites, 0, sizeof(wscan_survey.sites));
	/*
	 * Loop through till we have results to process or
	 * run out of space in survey.sites
	 */
	for (i = 0; i < count && j < MAX_SITES; i++) {
		if (wlan_get_scan_result(i, &wscan_survey.sites[j]))
			break;

		if (wscan_survey.sites[j].ssid[0] != 0)
			j++;
	}
	wscan_survey.num_sites = j;

	/* unlock the site survey table */
	os_mutex_put(&wscan_survey.lock);
	/* Give up the semaphore to process next scan/prov_stop request */
	os_semaphore_put(&wscan_protection_sem);
	return 0;
}

static int sys_wq_job_wscan()
{
	if (os_semaphore_get(&wscan_protection_sem,
			     os_msec_to_ticks(DEFAULT_SCAN_PROTECTION_TIMEOUT))
	    != WM_SUCCESS)
		prov_e("Scan protection timeout occurred. Failed to get the "
		       "scan protection semaphore.");

	return wlan_scan(wscan_handle_scan_results);
}

int wscan_periodic_start()
{
	wq_job_t job = {
		.job_func = sys_wq_job_wscan,
		.param = NULL,
		.periodic_ms = PERIODIC_SCAN_TIME,
		.initial_delay_ms = 0,
	};
	job.owner[0] = 0;

	if (sys_work_queue_init() != WM_SUCCESS) {
		prov_e("Failed to create system work queue\r\n");
		return -WM_FAIL;
	}

	wq_handle = sys_work_queue_get_handle();
	if (!wq_handle) {
		prov_e("Failed to get system work queue "
		      "handler\r\n");
		return -WM_FAIL;
	}

	if (os_mutex_create(&wscan_survey.lock, "site-survey",
			    OS_MUTEX_INHERIT)) {
		prov_e("Failed to create site-survey semaphore\r\n");
		goto out;
	}

	if (os_semaphore_create(&wscan_protection_sem,
				"wscan_protection_sem")) {
		prov_e("Failed to create wscan protection semaphore\r\n");
		goto out;
	}

	if (work_enqueue(wq_handle, &job, &job_handle)) {
		prov_e("Failed to enqueue periodic wscan job to "
		      "system work queue\r\n");
		goto out;
	}
	return WM_SUCCESS;

out:
	if (wscan_survey.lock) {
		os_mutex_delete(&wscan_survey.lock);
		wscan_survey.lock = NULL;
	}
	if (wscan_protection_sem) {
		os_semaphore_delete(&wscan_protection_sem);
		wscan_protection_sem = NULL;
	}
	return -WM_FAIL;
}

int wscan_periodic_stop()
{
	/* return success if wscan_protection_sem does not exist
	 * indicating task is not enqueued
	 */
	if (!wscan_protection_sem)
		return WM_SUCCESS;

	if (os_semaphore_get(&wscan_protection_sem, OS_WAIT_FOREVER) !=
	    WM_SUCCESS) {
		prov_e("Scan protection timeout occurred. Failed to get the "
		       "scan protection semaphore in periodic wscan.");
		return -WM_FAIL;
	}

	if (wq_handle && job_handle) {
		if (work_dequeue(wq_handle, &job_handle) != WM_SUCCESS) {
			prov_e("Failed to dequeue periodic wscan");
			return -WM_FAIL;
		}
	}
	if (wscan_survey.lock) {
		os_mutex_put(&wscan_survey.lock);
		os_mutex_delete(&wscan_survey.lock);
		wscan_survey.lock = NULL;
	}
	if (wscan_protection_sem) {
		os_semaphore_delete(&wscan_protection_sem);
		wscan_protection_sem = NULL;
	}
	return WM_SUCCESS;
}

int wscan_get_scan_result(struct wlan_scan_result **sptr, unsigned int index)
{
	if (wscan_protection_sem) {
		if (index > wscan_survey.num_sites)
			return -WM_FAIL;

		*sptr = &(wscan_survey.sites[index]);

		return WM_SUCCESS;
	}
	return -WM_FAIL;
}

int wscan_get_scan_count()
{
	int i;

	if (wscan_protection_sem) {
		for (i = wscan_survey.num_sites - 1; i >= 0; i--) {
			if (wscan_survey.sites[i].ssid[0] == 0)
				wscan_survey.num_sites--;
			else
				/* Found valid entry, exit */
				break;
		}
		return wscan_survey.num_sites;
	}
	return -WM_FAIL;
}

void wscan_put_lock_scan_results()
{
	if (wscan_protection_sem)
		os_mutex_put(&wscan_survey.lock);
}

int wscan_get_lock_scan_results()
{
	if (wscan_protection_sem) {
		if (os_mutex_get(&wscan_survey.lock, OS_WAIT_FOREVER)
		    == WM_SUCCESS)
			return WM_SUCCESS;
	}
	return -WM_FAIL;
}
