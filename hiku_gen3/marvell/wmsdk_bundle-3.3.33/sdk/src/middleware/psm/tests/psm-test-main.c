#include <string.h>
#include <stdlib.h>
#include <psm-v2.h>
#include <errno.h>
#include <wmlist.h>
#include <wmerrno.h>
#include <wmtime.h>
#include <wm_os.h>
#include <partition.h>
#include "../psm-internal.h"
#include <cli.h>
#include <mdev_pm.h>
#include <wm_utils.h>
#include <pwrmgr.h>
#include <wmtime.h>
#include <psm-utils.h>

#include <work-queue.h>
#include <system-work-queue.h>

/* #define TEST_DEBUG */

#define psmt_e(__fmt__, ...)					\
	wmprintf("[psmt] Error: "__fmt__"\r\n", ##__VA_ARGS__)
#define psmt_w(__fmt__, ...)					\
	wmprintf("[psmt] Warn: "__fmt__"\r\n", ##__VA_ARGS__)

#ifdef TEST_DEBUG
#define psmt_d(__fmt__, ...)					\
	wmprintf("[psmt]: "__fmt__"\r\n", ##__VA_ARGS__)
#else
#define psmt_d(...)
#endif

static bool rotator_on;

#define PM4_MARKER 0xA5A5
#define PM4_MARKER1 0xA55A

#define BIGGEST_SIZE 2048
#define MAX_NAME_LEN 256
#define MAX_OBJ_SIZE 512
#define MAX_OBJECTS 17
#define MAX_OBJ_VAL 255

#define TIME_DUR 3000

uint64_t  bytes_read, bytes_written, swap_erases, data_section_erases;

flash_desc_t fdesc;
static psm_hnd_t cached_phandle;
static uint32_t total_part_size;

os_timer_t pm4_timer;

uint32_t nvram_cli_no __attribute__((section(".nvram")));
uint32_t nvram_iter __attribute__((section(".nvram")));
uint32_t nvram_test_marker __attribute__((section(".nvram")));
uint32_t nvram_object_size __attribute__((section(".nvram")));
uint32_t nvram_test_markr1 __attribute__((section(".nvram")));

static uint32_t rand_cb_to_put_in_pm4;
static uint32_t is_timer_started;


typedef enum {
	CLI_0,
	CLI_1,
	CLI_2,
} cli_no_t;

void psm_secure_state(bool state);

static uint32_t get_rand_value(uint32_t mod)
{
	uint32_t rand_val = 0;
	get_random_sequence((uint8_t *)&rand_val, 4);
	rand_val = rand() % mod;
	while (rand_val == 0)
		rand_val = rand() % mod;
	return rand_val;
}

#define MAX_TIME_INTERVAL 5

static void psmcb_handler(psm_event_t event, void *data, void *data1)
{
	switch (event) {
	case PSM_EVENT_READ: {
		psmt_d("READ event");
		uint32_t bytes = (uint32_t) data1;
		bytes_read += bytes;
	}
		break;
	case  PSM_EVENT_WRITE: {
		psmt_d("WRITE event");
		uint32_t bytes = (uint32_t) data1;
		bytes_written += bytes;
	}
		break;
	case PSM_EVENT_ERASE: {
		psmt_d("ERASE event");
		uint32_t bytes = (uint32_t) data1;
		uint32_t address = (uint32_t) data;
		if (address == fdesc.fl_start && bytes != total_part_size)
			data_section_erases++;
		if ((address + bytes) == (fdesc.fl_start + total_part_size)
		    && bytes != total_part_size)
			swap_erases++;
	}
		break;
	case PSM_EVENT_COMPACTION: {
		if (nvram_object_size != 0) {
			uint32_t cb_no = (uint32_t) data;
			uint32_t total_cb = (uint32_t) data1;
			wmprintf("Cb : %d\r\n", cb_no);
			if (!is_timer_started && !rand_cb_to_put_in_pm4)
				rand_cb_to_put_in_pm4
					= get_rand_value(total_cb);

			if (rand_cb_to_put_in_pm4 == cb_no) {
				wmprintf("Trigger : %d\r\n",
					 rand_cb_to_put_in_pm4);
				uint32_t time_interval =
					get_rand_value(MAX_TIME_INTERVAL);
				wmprintf("Timer Interval : %d\r\n",
					 time_interval);
				os_timer_change
					(&pm4_timer,
					 time_interval,
					 0);
				os_timer_activate(&pm4_timer);
				rand_cb_to_put_in_pm4 = 0;
				is_timer_started = 1;
			}
		}
	}
		break;
	default:
		break;
	}
}


int psm_test_init(psm_cfg_t *psm_cfg)
{
	srand(wmtime_time_get_posix());

	int rv = WM_SUCCESS;

	flash_desc_t fdesc;
	rv = part_get_desc_from_id(FC_COMP_PSM, &fdesc);
	if (rv != WM_SUCCESS) {
		wmprintf("Unable to get fdesc from id\r\n");
		return rv;
	}

	flash_drv_init();

	/* Open the flash */
	mdev_t *fl_dev = flash_drv_open(fdesc.fl_dev);
	if (fl_dev == NULL) {
		wmprintf("Flash driver open failed\r\n");
		return -WM_FAIL;
	}
	total_part_size = fdesc.fl_size;

	wmprintf(" fdesc->fl_size ----   %d\r\n", fdesc.fl_size);

	rv = psm_module_init(&fdesc, &cached_phandle, psm_cfg);
	if (rv != WM_SUCCESS) {
		wmprintf("%s: init failed\r\n", __func__);
		return rv;
	}
	psm_register_event_callback(psmcb_handler);
	return rv;
}

int psm_test_deinit()
{
	psm_module_deinit(&cached_phandle);
	cached_phandle = 0;
	return 0;
}

/* ---------------------------------------------------------------- */
/* ---------------------------------------------------------------- */
static void pm4_timer_cb1(os_timer_arg_t arg)
{
	nvram_test_markr1 = PM4_MARKER1;
	pm_mcu_state(PM4, TIME_DUR);
}


int psm_test_compaction(uint32_t test_size);

static int fill_write_objects(uint32_t test_size)
{
	int rv = WM_SUCCESS;

	rv = psm_format(cached_phandle);
	if (rv != 0) {
		wmprintf("Format Failed \r\n");
		return rv;
	}
	uint8_t data_byte = test_size % 10;
	uint8_t *obj_buf;

	while (1) {
		obj_buf = os_mem_alloc(test_size);
		if (!obj_buf) {
			wmprintf(" No memory\r\n");
			return -WM_E_NOMEM;
		}
		memset(obj_buf, data_byte++, test_size);
		rv = psm_set_variable(cached_phandle,
				      "A",
				      obj_buf,
				      test_size);
		if (rv != 0) {
			wmprintf(" %s: Set Obj1 Failed:%d\r\n", __func__, rv);
			rv = -WM_FAIL;
			goto out_fn;
		}
		obj_buf = os_mem_realloc(obj_buf, test_size / 10);
		if (!obj_buf) {
			wmprintf(" No memory\r\n");
			return -WM_E_NOMEM;
		}
		memset(obj_buf, data_byte++, test_size / 10);
		rv = psm_set_variable(cached_phandle,
				      "B",
				      obj_buf,
				      test_size / 10);
		if (rv != 0) {
			wmprintf(" %s: Set Obj2 Failed:%d\r\n", __func__, rv);
			rv = -WM_FAIL;
			goto out_fn;
		}
		obj_buf = os_mem_realloc(obj_buf, test_size / 3);
		if (!obj_buf) {
			wmprintf(" No memory\r\n");
			return -WM_E_NOMEM;
		}
		memset(obj_buf, data_byte++, test_size / 3);
		rv = psm_set_variable(cached_phandle,
				      "C",
				      obj_buf,
				      test_size / 3);
		if (rv != 0) {
			wmprintf(" %s: Set Obj2 Failed:%d\r\n", __func__, rv);
			rv = -WM_FAIL;
			goto out_fn;
		}
		data_byte = test_size % 10;
		os_mem_free(obj_buf);
	}
 out_fn:
	os_mem_free(obj_buf);
	return rv;
}

static int read_verify_objects(uint32_t test_size)
{
	int rv = WM_SUCCESS;
	nvram_iter--;

	uint8_t data_byte = test_size % 10;
	uint8_t *obj_buf, *old_obj_buf;

	obj_buf = os_mem_alloc(test_size);
	if (!obj_buf) {
		wmprintf(" No memory 1\r\n");
		return -WM_E_NOMEM;
	}

	old_obj_buf = os_mem_alloc(test_size);
	if (!obj_buf) {
		wmprintf(" No memory 2\r\n");
		os_mem_free(obj_buf);
		return -WM_E_NOMEM;
	}
	memset(old_obj_buf, data_byte++, test_size);
	rv = psm_get_variable(cached_phandle,
			      "A",
			      obj_buf,
			      test_size);
	if (rv < 0) {
		wmprintf(" %s: Get Obj1 Failed:%d\r\n", __func__, rv);
		rv = -WM_FAIL;
		goto out_fn;
	}
	if (memcmp(obj_buf, old_obj_buf, test_size)) {
		wmprintf("Data1 in PSM not correct after reboot\r\n");
		rv = -WM_FAIL;
		goto out_fn;
	}

	obj_buf = os_mem_realloc(obj_buf, test_size / 10);
	if (!obj_buf) {
		wmprintf(" No memory 3\r\n");
		return -WM_E_NOMEM;
	}
	old_obj_buf = os_mem_realloc(old_obj_buf, test_size / 10);
	if (!old_obj_buf) {
		os_mem_free(obj_buf);
		wmprintf(" No memory 4\r\n");
		return -WM_E_NOMEM;
	}
	memset(old_obj_buf, data_byte++, test_size / 10);
	rv = psm_get_variable(cached_phandle,
			      "B",
			      obj_buf,
			      test_size / 10);
	if (rv < 0) {
		wmprintf(" %s: Get Obj2 Failed:%d\r\n", __func__, rv);
		rv = -WM_FAIL;
		goto out_fn;
	}
	if (memcmp(obj_buf, old_obj_buf, test_size / 10)) {
		wmprintf("Data 2 in PSM not correct after reboot\r\n");
		rv = -WM_FAIL;
		goto out_fn;
	}

	obj_buf = os_mem_realloc(obj_buf, test_size / 3);
	if (!obj_buf) {
		wmprintf(" No memory 5\r\n");
		return -WM_E_NOMEM;
	}

	old_obj_buf = os_mem_realloc(old_obj_buf, test_size / 3);
	if (!old_obj_buf) {
		os_mem_free(obj_buf);
		wmprintf(" No memory 6\r\n");
		return -WM_E_NOMEM;
	}

	memset(old_obj_buf, data_byte++, test_size / 3);
	rv = psm_get_variable(cached_phandle,
			      "C",
			      obj_buf,
			      test_size / 3);
	if (rv < 0) {
		wmprintf(" %s: Get Obj2 Failed:%d\r\n", __func__, rv);
		rv = -WM_FAIL;
		goto out_fn;
	}
	if (memcmp(obj_buf, old_obj_buf, test_size / 3)) {
		wmprintf("Data 3 in PSM not correct after reboot\r\n");
		rv = -WM_FAIL;
		goto out_fn;
	}


 out_fn:
	os_mem_free(obj_buf);
	os_mem_free(old_obj_buf);
	if (rv < 0) {
		wmprintf(" Error in Read\r\n");
	}
	psm_util_dump(cached_phandle);

	/* psm_test_compaction calls psm_tesT_int so
	 * deinit should be done here */
	psm_test_deinit();

	/* go for next iteration */
	if (nvram_iter)
		psm_test_compaction(nvram_object_size);

	return rv;
}

int psm_test_compaction(uint32_t test_size)
{

	int rv = psm_test_init(NULL);
	if (rv != 0) {
		wmprintf("PSM test init failed\r\n");
		return rv;
	}
	wmtime_init();
	pm_init();

	if (nvram_test_markr1 == PM4_MARKER1) {
		wmprintf("Read objects after PM4 \r\n");
		nvram_test_markr1 = 0;
		return read_verify_objects(nvram_object_size);
	} else {
		int rv = os_timer_create(&pm4_timer,
					 "pm4-timer",
					 1,
					 pm4_timer_cb1,
					 NULL,
					 OS_TIMER_ONE_SHOT,
					 OS_TIMER_NO_ACTIVATE);
		if (rv != WM_SUCCESS)
			return rv;

		return fill_write_objects(test_size);
	}
}

/* ---------------------------------------------------------------- */
#define MAX_OBJ1_SIZE 12
#define MAX_OBJ2_SIZE 25
#define MAX_OBJ3_SIZE 111
#define MAX_OBJ4_SIZE 2024
#define MAX_OBJ5_SIZE 4

/* ---------------------------------------------------------------- */
static void pm4_timer_cb(os_timer_arg_t arg)
{
	nvram_test_marker = PM4_MARKER;
	wmprintf(" Press key to wakeup\r\n");
	pm_mcu_state(PM4, TIME_DUR);
}
static int pm4_pre_test()
{
	int  rv =  WM_SUCCESS;
	rv = psm_format(cached_phandle);
	if (rv != 0) {
		wmprintf("Format  Failed \r\n");
		return rv;
	}

	uint8_t *obj_buf = os_mem_alloc(MAX_OBJ1_SIZE);
	if (!obj_buf) {
		wmprintf(" No memory\r\n");
		return -WM_E_NOMEM;
	}
	memset(obj_buf, 0x1, MAX_OBJ1_SIZE);
	rv = psm_set_variable(cached_phandle,
				    "A",
				    obj_buf,
				    MAX_OBJ1_SIZE);
	if (rv != 0) {
		wmprintf(" %s : Set Obj Failed : %d \r\n", __func__, rv);
		rv = -WM_FAIL;
		goto out_fn;
	}

	obj_buf = os_mem_realloc(obj_buf, MAX_OBJ2_SIZE);
	if (!obj_buf) {
		wmprintf(" No memory\r\n");
		return -WM_E_NOMEM;
	}
	memset(obj_buf, 0x2, MAX_OBJ2_SIZE);
	rv = psm_set_variable(cached_phandle,
			      "B",
			      obj_buf,
			      MAX_OBJ2_SIZE);
	if (rv != 0) {
		wmprintf(" %s : Set Obj Failed : %d \r\n", __func__, rv);
		rv = -WM_FAIL;
		goto out_fn;
	}

	obj_buf = os_mem_realloc(obj_buf, MAX_OBJ3_SIZE);
	if (!obj_buf) {
		wmprintf(" No memory\r\n");
		return -WM_E_NOMEM;
	}
	memset(obj_buf, 0x3, MAX_OBJ3_SIZE);

	rv = psm_set_variable(cached_phandle,
			      "C",
			      obj_buf,
			      MAX_OBJ3_SIZE);
	if (rv != 0) {
		wmprintf(" %s : Set Obj Failed : %d \r\n", __func__, rv);
		rv = -WM_FAIL;
		goto out_fn;
	}

	obj_buf = os_mem_realloc(obj_buf, MAX_OBJ4_SIZE);
	if (!obj_buf) {
		wmprintf(" No memory\r\n");
		return -WM_E_NOMEM;
	}
	memset(obj_buf, 0x4, MAX_OBJ4_SIZE);

	os_timer_activate(&pm4_timer);

	rv = psm_set_variable(cached_phandle,
			      "D",
			      obj_buf,
			      MAX_OBJ4_SIZE);
	if (rv != 0) {
		wmprintf(" %s : Set Obj Failed : %d \r\n", __func__, rv);
		rv = -WM_FAIL;
		goto out_fn;
	}
 out_fn:
	os_mem_free(obj_buf);
	return rv;
}
/* ---------------------------------------------------------------- */
static int pm4_post_test()
{
	psm_util_dump(cached_phandle);

	uint8_t *robj_buf = os_mem_alloc(MAX_OBJ1_SIZE);
	if (!robj_buf) {
		wmprintf(" No memory\r\n");
		return -WM_E_NOMEM;
	}

	uint8_t *obj_buf = os_mem_alloc(MAX_OBJ1_SIZE);
	if (!obj_buf) {
		wmprintf(" No memory\r\n");
		return -WM_E_NOMEM;
	}
	memset(obj_buf, 0x1, MAX_OBJ1_SIZE);
	memset(robj_buf, 0, MAX_OBJ1_SIZE);

	int  rv =  WM_SUCCESS;
	rv = psm_get_variable(cached_phandle,
			      "A",
			      robj_buf,
			      MAX_OBJ1_SIZE);
	if (rv < 0) {
		wmprintf(" %s : Get Obj1 Failed : %d \r\n", __func__, rv);
		rv = -WM_FAIL;
		goto out_fn1;
	}
	if (memcmp(robj_buf, obj_buf, MAX_OBJ1_SIZE)) {
		wmprintf("Data1 in PSM not correct after reboot\r\n");
		rv = -WM_FAIL;
		goto out_fn1;
	}

	robj_buf = os_mem_realloc(robj_buf, MAX_OBJ2_SIZE);
	if (!robj_buf) {
		wmprintf(" No memory\r\n");
		return -WM_E_NOMEM;
	}

	obj_buf = os_mem_realloc(obj_buf, MAX_OBJ2_SIZE);
	if (!obj_buf) {
		wmprintf(" No memory\r\n");
		return -WM_E_NOMEM;
	}
	memset(obj_buf, 0x2, MAX_OBJ2_SIZE);

	memset(robj_buf, 0, MAX_OBJ2_SIZE);

	rv = psm_get_variable(cached_phandle,
				    "B",
				    robj_buf,
				    MAX_OBJ2_SIZE);
	if (rv < 0) {
		wmprintf(" %s : Get Obj2 Failed : %d \r\n", __func__, rv);
		rv = -WM_FAIL;
		goto out_fn1;
	}

	if (memcmp(robj_buf, obj_buf, MAX_OBJ2_SIZE)) {
		wmprintf("Data2 in PSM not correct after reboot\r\n");
		rv = -WM_FAIL;
		goto out_fn1;
	}

	robj_buf = os_mem_realloc(robj_buf, MAX_OBJ3_SIZE);
	if (!robj_buf) {
		wmprintf(" No memory\r\n");
		return -WM_E_NOMEM;
	}

	obj_buf = os_mem_realloc(obj_buf, MAX_OBJ3_SIZE);
	if (!obj_buf) {
		wmprintf(" No memory\r\n");
		return -WM_E_NOMEM;
	}
	memset(obj_buf, 0x3, MAX_OBJ3_SIZE);

	memset(robj_buf, 0, MAX_OBJ3_SIZE);
	rv = psm_get_variable(cached_phandle,
				    "C",
				    robj_buf,
				    MAX_OBJ3_SIZE);
	if (rv < 0) {
		wmprintf(" %s : Get Obj3 Failed : %d \r\n", __func__, rv);
		rv = -WM_FAIL;
		goto out_fn1;
	}
	if (memcmp(robj_buf, obj_buf, MAX_OBJ3_SIZE)) {
		wmprintf("Data3 in PSM not correct after reboot\r\n");
		rv = -WM_FAIL;
		goto out_fn1;
	}

	obj_buf = os_mem_realloc(obj_buf, MAX_OBJ4_SIZE);

	if (!obj_buf) {
		wmprintf(" No memory\r\n");
		return -WM_E_NOMEM;
	}
	memset(obj_buf, 0x0, MAX_OBJ4_SIZE);
	rv = psm_get_variable(cached_phandle,
			      "D",
			      obj_buf,
			      MAX_OBJ4_SIZE);
	if (rv < 0) {
		wmprintf(" %s : Get Obj 4  Failed as Expected: %d\r\n"
			 , __func__, rv);
	}
	uint32_t obj5 = 0x55555555, robj5 = 0;

	rv = psm_set_variable(cached_phandle,
			      "E",
			      &obj5,
			      MAX_OBJ5_SIZE);
	if (rv != 0) {
		wmprintf(" %s : Set Obj5 Failed : %d \r\n", __func__, rv);
		rv = -WM_FAIL;
		goto out_fn1;
	}

	rv = psm_get_variable(cached_phandle,
			      "E",
			      &robj5,
			      MAX_OBJ5_SIZE);
	if (rv < 0) {
		wmprintf(" %s : Get Obj5 Failed : %d \r\n", __func__, rv);
		rv = -WM_FAIL;
		goto out_fn1;
	}
	if (robj5 != obj5) {
		wmprintf(" Data mis match obj 5\r\n");
		rv = -WM_FAIL;
	}

	psm_util_dump(cached_phandle);
 out_fn1:

	os_mem_free(obj_buf);
	os_mem_free(robj_buf);
	return rv;
}
/* ---------------------------------------------------------------- */
int  test_power_safe_psm()
{
	int rv = 0;
	if (nvram_test_marker == PM4_MARKER) {
		nvram_test_marker = 0;
		rv = pm4_post_test();
	} else {
		rv = os_timer_create(&pm4_timer,
				     "pm4-timer",
				     1,
				     pm4_timer_cb,
				     NULL,
				     OS_TIMER_ONE_SHOT,
				     OS_TIMER_NO_ACTIVATE);
		rv = pm4_pre_test();
	}
	return rv;
}
/* ---------------------------------------------------------------- */
/* ---------------------------------------------------------------- */
/* ---------------------------------------------------------------- */
/*
 * Test case : psm_test_case_01
 * Test Type : Unit
 * Test Description : A basic test case that does Set and Get of a single
 *                    PSM object.
 * Expected Result :  Set and Get should not return error, variable's value
 *                    which was Set should match with value received from Get.
 */
static int psm_test_case_01()
{
	psmt_d("%s :Started", __func__);
	int rv = 0;

	rv = psm_format(cached_phandle);
	if (rv != 0) {
		wmprintf("Format  Failed \r\n");
		return rv;
	}
	psmt_d("Format PSM partition successful");

	unsigned int obj_size = 4;
	unsigned int obj_val = 0xA5A5A5A5;
	unsigned int read_obj_val = 0;

	psmt_d("Object val : %x", obj_val);
	rv = psm_set_variable(cached_phandle,
				    "Onj1",
				    &obj_val,
				    obj_size);
	if (rv != 0) {
		wmprintf(" %s : Set Obj Failed : %d \r\n", __func__, rv);
		return rv;
	}
	psmt_d("Set Obj Successful");
	rv = psm_get_variable(cached_phandle,
				    "Onj1",
				    &read_obj_val,
				    obj_size);
	if (rv <  0) {
		wmprintf("%s: Get Obj Failed: %d \r\n", __func__, rv);
		return rv;
	}
	rv = WM_SUCCESS;
	psmt_d("%s Get Obj Successful", __func__);
	if (read_obj_val != obj_val) {
		wmprintf("%s: Verification Failed \r\n", __func__);
		return rv;
	}
	psmt_d("%s: Object Set  Get Successful", __func__);
	return rv;
}
/*----------------------------------------------------------------*/
/*
 * Test case : psm_test_case_02
 * Test Type : Functional
 * Test Description : A test case that does Set and Get of a single
 *                    PSM object random number of times. This may or may not
 *                    trigger compaction.
 * Expected Result :  Set and Get should not return error, variable's value
 *                    which was Set should match with value received from Get.
 */
static int psm_test_case_02()
{
	psmt_d("%s: Started", __func__);
	int rv = 0;
	rv = psm_format(cached_phandle);
	if (rv != 0) {
		wmprintf(" %s:Format Failed\r\n", __func__);
		return rv;
	}
	psmt_d("Format PSM partition successful");


	unsigned int obj_size = 4;
	unsigned int obj_val = 0, read_obj_val = 0;
	unsigned int cnt = 0;

	unsigned int max_cnt = get_rand_value(MAX_OBJ_VAL);
	psmt_d("Max Iterations: %d", max_cnt);

	for (cnt = 0; cnt <= max_cnt; cnt++) {
		rotator_on = true;
		psmt_d("Iteration: %d", cnt);
		int rv = psm_format(cached_phandle);
		if (rv != 0) {
			wmprintf(" %s:Format  Failed \r\n", __func__);
			return rv;
		}
		psmt_d("Format PSM partition successful");

		obj_val = rand() % 1024;
		psmt_d("Object Size: %d", obj_size);
		psmt_d("Object Val: %d", obj_val);

		rv = psm_set_variable(cached_phandle,
					    "Onj1",
					    &obj_val,
					    obj_size);
		if (rv != 0) {
			wmprintf(" %s: Set Obj Failed:%d\r\n", __func__, rv);
			return rv;
		}
		psmt_d("%s Set Obj Successful", __func__);
		rv = psm_get_variable(cached_phandle,
					    "Onj1",
					    &read_obj_val,
					    obj_size);
		if (rv <  0) {
			wmprintf("%s: Get Obj Failed: %d\r\n",  __func__, rv);
			return rv;
		}
		rv = WM_SUCCESS;
		psmt_d("%s Get Obj Successful", __func__);

		if (read_obj_val != obj_val) {
			wmprintf("%s:Verification Failed \r\n", __func__);
			return rv;
		}

		psmt_d("%s: Verification Successful", __func__);
	}
	psmt_d("%s: Object Set  Get Successful", __func__);
	return rv;
}
/* ---------------------------------------------------------------- */
/*
 * Test case : psm_test_case_03
 * Test Type : Functional
 * Test Description : A test case that does Set and Get of a single
 *                    PSM object of variable size
 *                    The size starts from PSM SIZE to 1 byte object
 * Expected Result :  An object of size greater than half of PSM Size
 *                    cannot be SET so error should be returned.
 *                    But for lesser size objects,  Set and Get should not
 *                    return error, variable's value
 *                    which was Set should match with value received from Get.

 */
static int psm_test_case_03()
{
	psmt_d("%s: Started", __func__);
	int rv = 0;
	rv = psm_format(cached_phandle);
	if (rv != 0) {
		wmprintf(" %s:Format  Failed \r\n", __func__);
		return rv;
	}
	psmt_d("Format PSM partition successful");

	uint8_t *buf = NULL;
	uint8_t *rbuf = NULL;

	unsigned int os_mem_free_psm_size = total_part_size;
	psmt_d(" %s :PSM Free Space : %d", __func__, os_mem_free_psm_size);
	buf = os_mem_alloc((os_mem_free_psm_size >> 1));
	if (!buf) {
		wmprintf(" %s No memory \r\n", __func__);
		return -1;
	}
	rbuf = os_mem_alloc((os_mem_free_psm_size >> 1));
	if (!buf) {
		wmprintf(" %s No memory \r\n", __func__);
		os_mem_free(buf);
		return -1;
	}

	memset(buf, 0, (os_mem_free_psm_size >> 1));
	memset(rbuf, rand() % MAX_OBJ_VAL, (os_mem_free_psm_size >> 1));

	rv = psm_set_variable(cached_phandle,
				    "Onj1",
				    buf,
				    (os_mem_free_psm_size >> 1));
	if (rv == 0) {
		wmprintf(" %s : Set Obj Failed : %d \r\n", __func__, rv);
		os_mem_free(buf);
		os_mem_free(rbuf);
		return -1;
	}
	psmt_d("%s Set Obj Failed as Expected", __func__);

	uint32_t cntr = 0, new_size = 0, shifter = 3;

	do {
		rotator_on = true;
		new_size = os_mem_free_psm_size >> shifter;
		memset(rbuf, rand() % MAX_OBJ_VAL, new_size);
		psmt_d("Size of Object : %d", new_size);
		shifter++;
		uint32_t max_iter = (os_mem_free_psm_size / new_size);
		for (cntr = 2; cntr <= max_iter; cntr++) {
			rotator_on = true;
			psmt_d("Iteration : %d", cntr);
			rv = psm_set_variable(cached_phandle,
						    "Onj1",
						    buf,
						    new_size);
			if (rv != 0) {
				wmprintf(" %s : Set Obj Failed : %d \r\n",
				       __func__, rv);
				rv = -1;
				goto out;

			}
			psmt_d("%s Set Obj Successful", __func__);
			rv = psm_get_variable(cached_phandle,
						    "Onj1",
						    rbuf,
						    new_size);
			if (rv <  0) {
				wmprintf("%s: Get Obj Failed: %d \r\n",
				       __func__, rv);
				rv = -1;
				goto out;

			}
			rv = WM_SUCCESS;
			psmt_d("%s Get Obj Successful", __func__);
			int res = memcmp(buf, rbuf, new_size);
			if (res != 0) {
				wmprintf("%s:Verification Failed @ %d \r\n",
				       __func__, res);
				rv = -1;
				goto out;
			}
			psmt_d("%s: Verification Successful", __func__);
		}
	} while (new_size != 1);
 out:
	os_mem_free(buf);
	os_mem_free(rbuf);
	return rv;
}
/* ---------------------------------------------------------------- */
/*
 * Test case : psm_test_case_04
 * Test Type : Unit
 * Test Description : A basic test case that does Set and Get of 2
 *                    PSM objects of different fixed size.
 *                    It even checks for invalid parameters.
 * Expected Result :  Set and Get should not return error, variable's value
 *                    which was Set should match with value received from Get.
 */

static int psm_test_case_04()
{
	psmt_d("%s: Started", __func__);
	int rv = 0;
	rv = psm_format(cached_phandle);
	if (rv != 0) {
		wmprintf("%s :Format  Failed \r\n", __func__);
		return rv;
	}
	psmt_d("Format PSM partition successful");


	unsigned int obj1_size = 4;
	unsigned int obj1_val = 0xA5A5A5A5;
	unsigned int read_obj1_val = 0;


	unsigned int obj2_size = 1;
	unsigned int obj2_val = get_rand_value(MAX_OBJ_VAL);
	unsigned int read_obj2_val = 0;

	psmt_d(" Object1 val : %x", obj1_val);
	rv = psm_set_variable(cached_phandle,
				    "Onj1",
				    &obj1_val,
				    obj1_size);
	if (rv != 0) {
		wmprintf(" %s : Set Obj1 Failed : %d \r\n", __func__, rv);
		return rv;
	}
	psmt_d("%s Set Obj1 Successful", __func__);
	psmt_d(" Object2 val : %x", obj2_val);
	rv = psm_set_variable(cached_phandle,
				    "Onj2",
				    &obj2_val,
				    obj2_size);
	if (rv != 0) {
		wmprintf(" %s : Set Obj2 Failed : %d \r\n", __func__, rv);
		return rv;
	}
	psmt_d("%s Set Obj2 Successful", __func__);

	rv = psm_get_variable(cached_phandle,
				    "Onj2",
				    &read_obj2_val,
				    obj2_size);
	if (rv <  0) {
		wmprintf("%s: Get Obj2 Failed: %d \r\n", __func__, rv);
		return rv;
	}
	rv = WM_SUCCESS;

	psmt_d("%s Get Obj2 Successful", __func__);


	rv = psm_get_variable(cached_phandle,
				    "Onj1",
				    &read_obj1_val,
				    obj1_size);
	if (rv < 0) {
		wmprintf("%s: Get Obj1 Failed: %d \r\n", __func__, rv);
		return rv;
	}

	rv = WM_SUCCESS;
	psmt_d("%s Get Obj1 Successful", __func__);

	if (read_obj1_val != obj1_val) {
		wmprintf("%s: Verification Failed Onject1 \r\n", __func__);
		return rv;
	}

	if (read_obj2_val != obj2_val) {
		wmprintf("%s: Verification Failed Objetct2 \r\n", __func__);
		return rv;
	}
	psmt_d("%s: Object Set  Get Successful", __func__);
	return rv;
}

/* ---------------------------------------------------------------- */
/*
 * Test case : psm_test_case_05
 * Test Type : Functional
 * Test Description : A test case that does Set and Get of a single
 *                    PSM object fixed number of times. This will surely
 *                    trigger compaction.
 *                    CRC check of PSM object is also tested when Variable
 *                    Get is performed.
 * Expected Result :  Set and Get should not return error, variable's value
 *                    which was Set should match with value received from Get.
 *                    Even after compaction correct data value should be read
 *                    from PSM.
 */
static int psm_test_case_05()
{
	psmt_d("%s: Started", __func__);
	int rv = 0;
	rv = psm_format(cached_phandle);
	if (rv != 0) {
		wmprintf(" %s:Format  Failed \r\n", __func__);
		return rv;
	}
	psmt_d("Format PSM partition successful");

	unsigned int os_mem_free_psm_size = total_part_size;
	psmt_d(" %s :PSM Free Space : %d", __func__, os_mem_free_psm_size);

	unsigned int obj_size = 511;

	uint8_t *buf = os_mem_alloc(obj_size);
	if (!buf) {
		wmprintf(" %s No memory \r\n", __func__);
		return -1;
	}
	uint8_t *rbuf = os_mem_alloc(obj_size);
	if (!rbuf) {
		wmprintf(" %s No memory \r\n", __func__);
		return -1;
	}
	memset(buf, 0, obj_size);
	memset(rbuf, rand() % MAX_OBJ_VAL, obj_size);

	unsigned int cnt = 0;

	unsigned int max_cnt = ((os_mem_free_psm_size / obj_size) + 2);
	psmt_d("Max Iterations: %d", max_cnt);

	for (cnt = 0; cnt <= max_cnt; cnt++) {
		rotator_on = true;
		psmt_d(" Iteration: %d", cnt);
		psmt_d(" Object Size: %d", obj_size);
		rv = psm_set_variable(cached_phandle,
					    "Onj1",
					    buf,
					    obj_size);
		if (rv != 0) {
			wmprintf(" %s : Set Obj Failed : %d \r\n",
				 __func__, rv);
			rv = -1;
			goto out;
		}
		psmt_d("%s Set Obj Successful", __func__);
		rv = psm_get_variable(cached_phandle,
					    "Onj1",
					    rbuf,
					    obj_size);
		if (rv < 0) {
			wmprintf("%s: Get Obj Failed: %d \r\n",  __func__, rv);
			rv = -1;
			goto out;
		}
		rv = WM_SUCCESS;
		psmt_d("%s Get Obj Successful", __func__);
		int i = 0;
		for (i = 0; i < obj_size; i++) {
			if (rbuf[i] != buf[i]) {
				wmprintf("%s:Verification Failed \r\n",
					 __func__);
				goto out;
			}
		}
		psmt_d("%s: Verification Successful", __func__);
	}
 out:
	os_mem_free(buf);
	os_mem_free(rbuf);
	return rv;
}
/* ---------------------------------------------------------------- */
/*
 * Test case : psm_test_case_06
 * Test Type : Functional
 * Test Description : A test case that does Set and Get of 2 PSM objects
 *                    fixed number of times. This will surely
 *                    trigger compaction.
 *                    CRC check of PSM object is also tested when Variable
 *                    Get is performed.
 * Expected Result :  Set and Get should not return eror, variable's value
 *                    which was Set should match with value received from Get.
 *                    Even after compaction correct data value should be read
 *                    from PSM.
 */
static int psm_test_case_06()
{
	psmt_d("%s: Started", __func__);
	int rv = 0;
	rv = psm_format(cached_phandle);
	if (rv != 0) {
		wmprintf(" %s:Format  Failed \r\n", __func__);
		return rv;
	}
	psmt_d("Format PSM partition successful");

	unsigned int obj1_size = 4;
	unsigned int obj1_val = 0xA5A5A5A5;
	unsigned int read_obj1_val = 0;


	unsigned int obj2_size = 1;
	unsigned int obj2_val = get_rand_value(MAX_OBJ_VAL);
	unsigned int read_obj2_val = 0;

	uint32_t os_mem_free_psm_size = total_part_size;
	uint32_t max_cnt = (os_mem_free_psm_size / ((sizeof(obj1_size) +
					      sizeof(obj2_size)))) + 2;
	uint32_t iter = 0;
	for (iter = 0; iter <= max_cnt; iter++) {
		rotator_on = true;
		obj1_val = rand() % 32787;
		psmt_d(" Object1 val : %x", obj1_val);
		rv = psm_set_variable(cached_phandle,
					    "Onj1",
					    &obj1_val,
					    obj1_size);
		if (rv != 0) {
			wmprintf("%s: Set Obj1 Failed : %d \r\n",
				 __func__, rv);
			return rv;
		}
		psmt_d("%s Set Obj1 Successfuln", __func__);

		obj2_val = get_rand_value(MAX_OBJ_VAL);
		psmt_d("Object2 val : %x", obj2_val);
		rv = psm_set_variable(cached_phandle,
					    "Onj2",
					    &obj2_val,
					    obj2_size);
		if (rv != 0) {
			wmprintf(" %s : Set Obj2 Failed : %d \r\n",
				 __func__, rv);
			return rv;
		}
		psmt_d("%s Set Obj2 Successful", __func__);

		rv = psm_get_variable(cached_phandle,
					    "Onj2",
					    &read_obj2_val,
					    obj2_size);
		if (rv < 0) {
			wmprintf("%s: Get Obj2 Failed: %d \r\n", __func__, rv);
			return rv;
		}
		rv = WM_SUCCESS;
		psmt_d("%s Get Obj2 Successful ", __func__);

		rv = psm_get_variable(cached_phandle,
					    "Onj1",
					    &read_obj1_val,
					    obj1_size);
		if (rv < 0) {
			wmprintf("%s: Get Obj1 Failed: %d \r\n",
			       __func__, rv);
			return rv;
		}
		rv = WM_SUCCESS;
		psmt_d("%s Get Obj1 Successful \r\n", __func__);

		if (read_obj1_val != obj1_val) {
			wmprintf("%s: Verification Failed Onject1 \r\n",
			       __func__);
			return rv;
		}

		if (read_obj2_val != obj2_val) {
			wmprintf("%s: Verification Failed Objetct2 \r\n",
			       __func__);
			return rv;
		}
	}
	psmt_d("%s: Object Set  Get Successful \r\n", __func__);
	return rv;
}
/* ---------------------------------------------------------------- */
/*
 * Test case : psm_test_case_07
 * Test Type : Stress
 * Test Description : This is an extended version of case 6 but it
 *                    acts like a stress test case triggering compaction
 *                    many times.
 *                    A test case that does Set and Get of 2 PSM objects
 *                    fixed number of times. This will surely
 *                    trigger compaction.
 *                    CRC check of PSM object is also tested when Variable
 *                    Get is performed.
 * Expected Result :  Set and Get should not return eror, variable's value
 *                    which was Set should match with value received from Get.
 *                    Even after compaction correct data value should be read
 *                    from PSM.
 */
static int psm_test_case_07()
{
	psmt_d("%s: Started", __func__);
	int rv = 0;
	rv = psm_format(cached_phandle);
	if (rv != 0) {
		wmprintf(" %s:Format  Failed \r\n", __func__);
		return rv;
	}
	psmt_d("Format PSM partition successful");

	unsigned int obj1_size = 4;
	unsigned int obj1_val = 0xA5A5A5A5;
	unsigned int read_obj1_val = 0;

	unsigned int obj2_size = 1;
	unsigned int obj2_val = get_rand_value(MAX_OBJ_VAL);
	unsigned int read_obj2_val = 0;

	uint32_t os_mem_free_psm_size = total_part_size;
	uint32_t max_cnt = (os_mem_free_psm_size / ((sizeof(obj1_size) +
					      sizeof(obj2_size)))) + 2;
	uint32_t iter = 0, cntr = 0;

	uint32_t max_lopp = get_rand_value(3);
	for (cntr = 0 ; cntr < max_lopp; cntr++) {
		for (iter = 0; iter <= max_cnt; iter++) {
			rotator_on = true;
			obj1_val = get_rand_value(32767);
			psmt_d(" Object1 val : %x", obj1_val);
			rv = psm_set_variable(cached_phandle,
						    "Onj1",
						    &obj1_val,
						    obj1_size);
			if (rv != 0) {
				wmprintf(" %s : Set Obj1 Failed : %d \r\n",
				       __func__, rv);
				return rv;
			}
			psmt_d("%s Set Obj1 Successful ", __func__);

			obj2_val = get_rand_value(MAX_OBJ_VAL);
			psmt_d(" Object2 val : %x\r\n", obj2_val);
			rv = psm_set_variable(cached_phandle,
						    "Onj2",
						    &obj2_val,
						    obj2_size);
			if (rv != 0) {
				wmprintf(" %s : Set Obj2 Failed : %d ",
				       __func__, rv);
				return rv;
			}
			psmt_d("%s Set Obj2 Successful ", __func__);
			rv = psm_get_variable(cached_phandle,
						    "Onj2",
						    &read_obj2_val,
						    obj2_size);
			if (rv < 0) {
				wmprintf("%s: Get Obj2 Failed: %d \r\n",
				       __func__, rv);
				return rv;
			}
			rv = WM_SUCCESS;
			psmt_d("%s Get Obj2 Successful ", __func__);
			rv = psm_get_variable(cached_phandle,
						    "Onj1",
						    &read_obj1_val,
						    obj1_size);
			if (rv < 0) {
				wmprintf("%s: Get Obj1 Failed: %d \r\n",
				       __func__, rv);
				return rv;
			}
			psmt_d("%s Get Obj1 Successful ", __func__);

			if (read_obj1_val != obj1_val) {
				wmprintf("%s: Verification Failed Onject1 \r\n",
				       __func__);
				return rv;
			}
			if (read_obj2_val != obj2_val) {
				wmprintf("%s: Verification Failed"
					 "Objetct2 \r\n",
				       __func__);
				return rv;
			}
		}
	}
	psmt_d("%s: Object Set  Get Successful ", __func__);
	return rv;
}
/* ---------------------------------------------------------------- */
/*
 * Test case : psm_test_case_08
 * Test Type : Functional
 * Test Description : A test case that does Set and Get of 2 PSM objects
 *                    One obbject is written in PSM till to fill PSM
 *                    partition. Then write a new object.
 *                    This checks compaction.
 * Expected Result :  Set and Get should not return eror, variable's value
 *                    which was Set should match with value received from Get.
 *                    Even after compaction correct data value should be read
 *                    from PSM.
 */
static int psm_test_case_08()
{
	psmt_d("%s: Started", __func__);
	int rv = 0;
	rv = psm_format(cached_phandle);
	if (rv != 0) {
		wmprintf(" %s:Format  Failed \r\n", __func__);
		return rv;
	}
	psmt_d("Format PSM partition successful");

	unsigned int obj_size = 4;
	unsigned int obj_val = 0, read_obj_val = 0;
	unsigned int cnt = 0;

	unsigned int max_cnt = (total_part_size / obj_size) - 1;
	psmt_d("Max Iterations : %d", max_cnt);

	for (cnt = 0; cnt <= max_cnt; cnt++) {
		rotator_on = true;
		psmt_d(" Iteration: %d", cnt);
		int rv = psm_format(cached_phandle);
		if (rv != 0) {
			wmprintf(" %s:Format  Failed \r\n", __func__);
			return rv;
		}
		psmt_d("Format PSM partition successful\r\n");

		obj_val = get_rand_value(1024);
		psmt_d(" Object Size: %d", obj_size);
		psmt_d(" Object Val: %d\r\n", obj_val);

		rv = psm_set_variable(cached_phandle,
					    "Onj1",
					    &obj_val,
					    obj_size);
		if (rv != 0) {
			wmprintf(" %s : Set Obj Failed : %d \r\n",
				 __func__, rv);
			return rv;
		}
		psmt_d("%s Set Obj Successful ", __func__);
		rv = psm_get_variable(cached_phandle,
					    "Onj1",
					    &read_obj_val,
					    obj_size);
		if (rv < 0) {
			wmprintf("%s: Get Obj Failed: %d \r\n",  __func__, rv);
			return rv;
		}
		rv = WM_SUCCESS;
		psmt_d("%s Get Obj Successful ", __func__);

		if (read_obj_val != obj_val) {
			wmprintf("%s:Verification Failed ", __func__);
			return rv;
		}

		psmt_d("%s: Verification Successful ", __func__);
	}
	unsigned int obj2_size = 1;
	unsigned char obj2_val = get_rand_value(MAX_OBJ_VAL), read_obj2_val = 0;
	psmt_d(" Object2 Size: %d", obj2_size);
	psmt_d(" Object2 Val: %x", obj2_val);

	rv = psm_set_variable(cached_phandle,
			      "Onj2",
			      &obj2_val,
			      obj2_size);
	if (rv != 0) {
		wmprintf(" %s : Set Obj Failed : %d \r\n", __func__, rv);
		return rv;
	}
	psmt_d("%s Set Obj2 Successful ", __func__);
	rv = psm_get_variable(cached_phandle,
			      "Onj2",
			      &read_obj2_val,
			      obj2_size);
	if (rv < 0) {
		wmprintf("%s: Get Obj Failed: %d \r\n",  __func__, rv);
		return rv;
	}
	rv = WM_SUCCESS;
	psmt_d("%s Get Obj2 Successful ", __func__);

	if (read_obj2_val != obj2_val) {
		wmprintf("%s:Verification Failed \r\n", __func__);
		return rv;
	}
	psmt_d("%s: Object Set Get Successful ", __func__);

	rv = psm_set_variable(cached_phandle,
			      "Onj1",
			      &obj_val,
			      obj_size);
	if (rv != 0) {
		wmprintf(" %s : Set Obj Failed : %d \r\n", __func__, rv);
		return rv;
	}
	psmt_d("%s Set Obj Successful ", __func__);
	rv = psm_get_variable(cached_phandle,
			      "Onj1",
			      &read_obj_val,
			      obj2_size);
	if (rv ==  -WM_E_NOSPC) {
		wmprintf("%s: Get Obj Failed as Expected: %d \r\n",
		       __func__, rv);
		return 0;
	}
	return -WM_FAIL;
}

/* ---------------------------------------------------------------- */
/*
 * Test case : psm_test_case_09
 * Test Type : Unit
 * Test Description : A test case that tests APIs
 *                    psm_object_open, psm_object_write
 *                    psm_object_read, psm_object_close
 * Expected Result :  Open an object and write it.
 *                    Close the object and read it back to verify data.
 *                    Data should match and all APIS should work fine.
 */
static int psm_test_case_09()
{
	uint8_t obj[128];
	uint8_t robj[128];
	memset(obj, get_rand_value(MAX_OBJ_VAL / 2) , sizeof(obj));
	memset(robj, 0, sizeof(robj));

	int rv = 0;

	rv = psm_format(cached_phandle);
	if (rv != 0) {
		wmprintf(" %s:Format  Failed \r\n", __func__);
		return rv;
	}

	psm_object_handle_t oHandle;
	rv = psm_object_open(cached_phandle, "object1", PSM_MODE_WRITE,
			     BIGGEST_SIZE,
			     NULL, &oHandle);
	if (rv != WM_SUCCESS) {
		wmprintf("psm_object_open failed : %d \r\n", rv);
		return rv;
	}
	rv = psm_object_write(oHandle, obj, sizeof(obj));

	if (rv != WM_SUCCESS) {
		wmprintf("psm_object_write failed : %d \r\n", rv);
		return rv;
	}
	rv = psm_object_close(&oHandle);
	if (rv != WM_SUCCESS) {
		wmprintf("psm_object_close failed : %d \r\n", rv);
		return rv;
	}

	rv = psm_object_open(cached_phandle, "object1", PSM_MODE_READ,
			     BIGGEST_SIZE,
			     NULL, &oHandle);
	if (rv < WM_SUCCESS) {
		wmprintf("psm_object_open failed : %d \r\n", rv);
		return rv;
	}
	rv = psm_object_read(oHandle, robj, sizeof(robj));

	if (rv != sizeof(robj)) {
		wmprintf("psm_object_read failed : %d \r\n", rv);
		return rv;
	}
	rv = psm_object_close(&oHandle);
	if (rv != WM_SUCCESS) {
		wmprintf("psm_object_close failed : %d \r\n", rv);
		return rv;
	}
	if ((rv = memcmp(robj, obj, sizeof(obj)))) {
		wmprintf("Verification failed: %d \r\n", rv);
		rv = -WM_FAIL;
	} else {
		rv = WM_SUCCESS;

	}
	return rv;
}

/* ---------------------------------------------------------------- */
/*
 * Test case : psm_test_case_10
 * Test Type : Unit
 * Test Description : A test case that tests APIs
 *                    psm_object_open, psm_object_write
 *                    psm_object_read, psm_object_close
 * Expected Result :  Open an object and close it.
 *                    Read should return lenght 0.
 */
static int psm_test_case_10()
{
	int rv = 0;
	rv = psm_format(cached_phandle);
	if (rv != 0) {
		wmprintf(" %s:Format  Failed \r\n", __func__);
		return rv;
	}

	psm_object_handle_t oHandle;
	rv = psm_object_open(cached_phandle, "object1", PSM_MODE_WRITE,
				 BIGGEST_SIZE,
				 NULL, &oHandle);
	if (rv != WM_SUCCESS) {
		wmprintf("psm_object_open failed : %d \r\n", rv);
		return rv;
	}
	rv = psm_object_close(&oHandle);
	if (rv != WM_SUCCESS) {
		wmprintf("psm_object_close failed : %d \r\n", rv);
		return rv;
	}
	rv = psm_object_open(cached_phandle, "object1", PSM_MODE_WRITE,
			     BIGGEST_SIZE,
			     NULL, &oHandle);
	if (rv != WM_SUCCESS) {
		wmprintf("psm_object_open failed : %d \r\n", rv);
		return rv;
	}
	uint32_t obj = 0;
	rv = psm_object_read(oHandle, &obj, MAX_OBJ_SIZE);
	if (rv != 0) {
		return -WM_FAIL;
	}
	rv = psm_object_close(&oHandle);
	if (rv != WM_SUCCESS) {
		wmprintf("psm_object_close failed : %d \r\n", rv);
		return rv;
	}
	return rv;
}
/* ---------------------------------------------------------------- */
/*
 * Test case : psm_test_case_11
 * Test Type : Unit
 * Test Description : A test case that tests API
 *                    psm_object_open with large object name length.
 * Expected Result :  Open should return error.

 */
static int psm_test_case_11()
{
	int rv = 0;
	rv = psm_format(cached_phandle);
	if (rv != 0) {
		wmprintf(" %s:Format  Failed \r\n", __func__);
		return rv;
	}
	char *name = os_mem_alloc(MAX_NAME_LEN + 2);
	if (!name) {
		return -WM_E_NOMEM;
	}
	memset(name, get_rand_value(MAX_OBJ_VAL), MAX_NAME_LEN + 1);

	name[MAX_NAME_LEN + 1] = '\0';
	psm_object_handle_t oHandle;
	rv = psm_object_open(cached_phandle, name, PSM_MODE_WRITE,
			     BIGGEST_SIZE,
			     NULL, &oHandle);
	if (rv == WM_SUCCESS) {
		wmprintf("psm_object_open test failed\r\n");
		rv = -WM_FAIL;
	} else
		rv = WM_SUCCESS;
	os_mem_free(name);
	return rv;
}
/* ---------------------------------------------------------------- */
typedef struct psm_test_obj {
	char  *name;
	uint32_t name_len;
	uint8_t *obj_buf;
	uint32_t obj_len;
	uint32_t burst_size;
	bool state;
	list_head_t l;
} test_obj_t;

static void free_obj(test_obj_t *trav)
{
	if (trav->obj_buf)
		os_mem_free(trav->obj_buf);
	if (trav->name)
		os_mem_free(trav->name);
	os_mem_free(trav);
}

static void free_all_links(list_head_t *anchor)
{
	test_obj_t *trav;
	list_head_t *link = anchor->next;

	while (!list_empty(anchor)) {
		trav = list_entry(link, test_obj_t, l);
		link->prev->next = link->next;
		link->next->prev = link->prev;

		link = link->next;

		free_obj(trav);
	}
	os_mem_free(anchor);
}

static int  write_obj(test_obj_t *link, psm_objattrib_t *atrib)
{
	psm_object_handle_t oHandle;
	int rv = psm_object_open(cached_phandle, link->name, PSM_MODE_WRITE,
				 2048,
				 atrib, &oHandle);
	if (rv != WM_SUCCESS) {
		psmt_d("psm_object_open test failed : %d \r\n", rv);
		free_obj(link);
		return rv;
	}

	uint32_t write_len = link->obj_len;
	uint32_t op_len = link->burst_size;
	while (write_len) {
		rv = psm_object_write(oHandle, link->obj_buf, op_len);
		write_len -= op_len;
		if (write_len > link->burst_size)
			op_len = link->burst_size;
		else
			op_len = write_len;
	}
	rv = psm_object_close(&oHandle);
	if (rv != WM_SUCCESS) {
		wmprintf("psm_object_close failed : %d \r\n", rv);
		return rv;
	}
	link->state = 1;
	return WM_SUCCESS;
}


static int verify_objects(list_head_t *anchor, uint32_t *total_obj,
			  uint32_t *fail_cnt)
{
	int rv = 0;
	test_obj_t *trav;
	uint32_t totla_cnt = 0, failure_cnt = 0;
	uint8_t *ver_buf1;

	list_for_each_entry(trav, anchor, l) {
		if (trav->state) {
			totla_cnt++;
			ver_buf1 = os_mem_alloc(trav->obj_len);
			if (!ver_buf1) {
				failure_cnt++;
				continue;
			}
			rv = psm_get_variable(cached_phandle, trav->name,
					      ver_buf1, trav->obj_len);
			if (rv < 0) {
				wmprintf("Get Variable failed \r\n");
				os_mem_free(ver_buf1);
				failure_cnt++;
				totla_cnt++;
				continue;
			}
			rv = WM_SUCCESS;
			if (memcmp(ver_buf1, trav->obj_buf, trav->obj_len)) {
				failure_cnt++;
			}
			os_mem_free(ver_buf1);
		}
	}
	*fail_cnt = failure_cnt;
	*total_obj = totla_cnt;
	return 0;
}

static void add_link(test_obj_t *link, list_head_t *anchor)
{
	psmt_d("----------Object Contents--------------");
	psmt_d(" Name : %s", link->name);
	psmt_d(" Name Len  : %d", link->name_len);
	psmt_d(" Object Len  : %d", link->obj_len);


	INIT_LIST_HEAD(&link->l);
	list_add(&link->l, anchor);
}

static int populate_test_obj(test_obj_t *link, list_head_t *anchor,
			     psm_objattrib_t *atrib)
{
	link->name_len = get_rand_value(MAX_NAME_LEN);

	/* This is done to make space for '\0'*/
	if (link->name_len == 1)
		link->name_len = 2;
	link->name = os_mem_alloc(link->name_len);
	if (!link->name) {
		return  -WM_E_NOMEM;
	}

	memset(link->name, 'B', link->name_len);
	link->name[link->name_len - 1] = '\0';

	link->obj_len = get_rand_value(MAX_OBJ_SIZE);

	link->obj_buf = os_mem_alloc(link->obj_len);
	if (!link->obj_buf) {
		os_mem_free(link->name);
		return  -WM_E_NOMEM;
	}
	link->burst_size = get_rand_value((link->obj_len));

	memset(link->obj_buf, rand() % MAX_NAME_LEN, link->obj_len);

	test_obj_t *trav;

	list_for_each_entry(trav, anchor, l) {
		if (trav->name_len == link->name_len) {
			trav->state = 0;
		}
	}
	int rv = WM_SUCCESS;
	rv =  write_obj(link, atrib);
	if (rv == -WM_E_NOSPC) {
		/* psmt_d("Write_obj failed"); */
		return rv;
	}
	if (rv != WM_SUCCESS) {
		free_obj(link);
		return rv;
	}
	add_link(link, anchor);
	return rv;
}
/* ---------------------------------------------------------------- */
/*
 * Test case : psm_test_case_12
 * Test Type : Stress
 * Test Description : A complex test case which stressfully
 *                    tests PSM module APIS.
 *                    Random number of objects are created.
 *                    This objects have random lenght name,
 *                    random size of data and random burst size.
 *                    Burst size is the size of chunk in which
 *                    object is written in PSM.
 *                    Fill PSM till no space is left.
 *                    Then start verifying data from PSM
 *                    APIs tested:
 *                    psm_object_close, psm_get_variable,
 *                    psm_object_write, psm_object_open
 * Expected Result :  Set and Get should not return eror, variable's value
 *                    which was Set should match with value received from Get.
 *                    Even after compaction correct data value should be read
 *                    from PSM.
 */

static int psm_test_case_12()
{
	int rv = 0;

	rv = psm_format(cached_phandle);
	if (rv != 0) {
		wmprintf(" %s:Format  Failed \r\n", __func__);
		return rv;
	}
	/* Initialize the anchor */
	list_head_t *anchor = os_mem_alloc(sizeof(list_head_t));
	INIT_LIST_HEAD(anchor);

	unsigned int max_cnt = get_rand_value(MAX_OBJECTS);
	uint32_t cnt;

	psmt_d("Max Objects : %d", max_cnt);

	for (cnt = 0; cnt < max_cnt; cnt++) {
		test_obj_t *obj_link = NULL;
		obj_link = os_mem_alloc(sizeof(test_obj_t));
		if (!obj_link) {
			rv = -WM_E_NOMEM;
			goto out;
		}
		rv = populate_test_obj(obj_link, anchor, NULL);
		if (rv != WM_SUCCESS)
			break;
	}
	psmt_d("Start Verification ........");

	uint32_t total_cnt = 0, fail_cnt = 0;

	if (rv == -WM_E_NOSPC || rv == WM_SUCCESS) {
		rv = verify_objects(anchor, &total_cnt, &fail_cnt);
		psmt_d("Total Objects : %d", total_cnt);
		psmt_d("Failures : %d", fail_cnt);
		if (fail_cnt)
			rv = -WM_FAIL;
	}
 out:
	free_all_links(anchor);
	return rv;
}
/* ---------------------------------------------------------------- */
/* Test Case : psm_test_case_13
 * Test Type : unit
 * Test Description : This test case sets and gets an object
 *                    and then deletes the object and tries to get it.
 *                    API tested psm_object_delete
 * Expected Result :  Get after deletion of object should return error.
 */

static int psm_test_case_13()
{
	int rv = 0;

	rv = psm_format(cached_phandle);
	if (rv != 0) {
		wmprintf(" %s:Format  Failed \r\n", __func__);
		return rv;
	}
	uint32_t obj_size = get_rand_value(MAX_OBJ_SIZE);
	uint8_t *obj_ptr = os_mem_alloc(obj_size);
	uint8_t *robj_ptr = os_mem_alloc(obj_size);

	if (!obj_ptr || !robj_ptr) {
		return -WM_E_NOMEM;
	}
	rv = psm_set_variable(cached_phandle,
			      "#@!%^&*&",
			      obj_ptr,
			      obj_size);
	if (rv !=  WM_SUCCESS) {
		wmprintf(" %s : Set Obj Failed : %d \r\n", __func__, rv);
		rv = -WM_FAIL;
		goto out_13;
	}

	rv = psm_get_variable(cached_phandle,
			      "#@!%^&*&",
			      robj_ptr,
			      obj_size);

	if (rv < 0) {
		wmprintf(" %s : Get Obj Failed : %d \r\n", __func__, rv);
		rv = -WM_FAIL;
		goto out_13;
	}
	rv = WM_SUCCESS;
	if (!memcmp(robj_ptr, obj_ptr, obj_size)) {
		psmt_d("Verification Successful");
	} else {
		wmprintf("Verification Failed\r\n");
		rv = -WM_FAIL;
		goto out_13;
	}

	rv = psm_object_delete(cached_phandle, "#@!%^&*&");

	if (rv !=  WM_SUCCESS) {
		wmprintf(" %s : Delete Obj Failed : %d \r\n", __func__, rv);
		rv = -WM_FAIL;
		goto out_13;
	}
	rv = psm_get_variable(cached_phandle,
			      "#@!%^&*&",
			      robj_ptr,
			      obj_size);
	if (rv != -WM_FAIL) {
		wmprintf("%s :Detected a deleted object: %d \r\n",
			 __func__, rv);
		rv = -WM_FAIL;
		goto out_13;
	} else {
		rv = WM_SUCCESS;
		goto out_13;
	}

#define MAX_ITER 3
	uint32_t iter;
	for (iter = 0; iter < MAX_ITER; iter++) {
		memset(robj_ptr, 0, obj_size);
		memset(obj_ptr, 0, obj_size);
		memset(obj_ptr, get_rand_value(MAX_NAME_LEN), obj_size);
		rv = psm_set_variable(cached_phandle,
				      "#@!%^&*&",
				      obj_ptr,
				      obj_size);
		if (rv !=  WM_SUCCESS) {
			wmprintf(" %s : Set Obj Failed : %d \r\n",
				 __func__, rv);
			rv = -WM_FAIL;
			goto out_13;
		}

		rv = psm_get_variable(cached_phandle,
				      "#@!%^&*&",
				      robj_ptr,
				      obj_size);

		if (rv !=  WM_SUCCESS) {
			wmprintf(" %s : Set Obj Failed : %d \r\n",
				 __func__, rv);
			rv = -WM_FAIL;
			goto out_13;
		}
		if (memcmp(robj_ptr, obj_ptr, obj_size)) {
			wmprintf("%s: Verification failed \r\n", __func__);
			goto out_13;
		}
	}

 out_13:
	os_mem_free(obj_ptr);
	os_mem_free(robj_ptr);
	return rv;
}
/* ---------------------------------------------------------------- */
#define MAX_LIST 20

static int psm_test_case_14()
{
	int rv = 0;

	rv = psm_format(cached_phandle);
	if (rv != 0) {
		wmprintf(" %s:Format  Failed \r\n", __func__);
		return rv;
	}
	/* Initialize the anchor */
	list_head_t *anchor = os_mem_alloc(sizeof(list_head_t));
	INIT_LIST_HEAD(anchor);

	unsigned int max_cnt = MAX_LIST;
	uint32_t cnt;

	psmt_d("Max Objects : %d", max_cnt);

	for (cnt = 0; cnt < max_cnt; cnt++) {
		test_obj_t *obj_link = NULL;
		obj_link = os_mem_alloc(sizeof(test_obj_t));
		if (!obj_link) {
			rv = -WM_E_NOMEM;
			goto out;
		}
		psm_objattrib_t atrib = {get_rand_value(2)};

		rv = populate_test_obj(obj_link, anchor,
				       &atrib);
		if (rv != WM_SUCCESS)
			break;
	}
	psmt_d("Start Verification ........");

	uint32_t total_cnt = 0, fail_cnt = 0;

	if (rv == -WM_E_NOSPC || rv == WM_SUCCESS) {
		rv = verify_objects(anchor, &total_cnt, &fail_cnt);
		psmt_d("Total Objects : %d", total_cnt);
		psmt_d("Failures : %d", fail_cnt);
		if (fail_cnt)
			rv = -WM_FAIL;
	}
 out:
	free_all_links(anchor);
	return rv;
}

/* ---------------------------------------------------------------- */
static int psm_test_case_15()
{
	int rv = 0;

	rv = psm_format(cached_phandle);
	if (rv != 0) {
		wmprintf(" %s:Format  Failed \r\n", __func__);
		return rv;
	}
	/* Initialize the anchor */
	list_head_t *anchor = os_mem_alloc(sizeof(list_head_t));
	INIT_LIST_HEAD(anchor);

	unsigned int max_cnt = MAX_LIST;
	uint32_t cnt;

	psmt_d("Max Objects : %d", max_cnt);

	for (cnt = 0; cnt < max_cnt; cnt++) {
		test_obj_t *obj_link = NULL;
		obj_link = os_mem_alloc(sizeof(test_obj_t));
		if (!obj_link) {
			rv = -WM_E_NOMEM;
			goto out;
		}
		psm_objattrib_t atrib = {get_rand_value(3)};

		rv = populate_test_obj(obj_link, anchor,
				       &atrib);
		if (rv != WM_SUCCESS)
			break;
	}
	uint32_t total_cnt = 0, fail_cnt = 0;

	if (rv == -WM_E_NOSPC || rv == WM_SUCCESS) {
		psmt_d("Start Verification ........");
		rv = verify_objects(anchor, &total_cnt, &fail_cnt);
		psmt_d("Total Objects : %d", total_cnt);
		psmt_d("Failures : %d", fail_cnt);
		if (fail_cnt)
			rv = -WM_FAIL;
	}
 out:
	free_all_links(anchor);
	return rv;
}
/* ---------------------------------------------------------------- */
static int psm_test_case_16()
{
	uint8_t obj[8];
	uint8_t robj[8];
	memset(obj, get_rand_value(MAX_OBJ_VAL / 2) , sizeof(obj));
	memset(robj, 0, sizeof(robj));

	int rv = 0;

	rv = psm_format(cached_phandle);
	if (rv != 0) {
		wmprintf(" %s:Format  Failed \r\n", __func__);
		return rv;
	}

	psm_object_handle_t oHandle;
	psm_objattrib_t attrib = {PSM_CACHING_ENABLED};
	rv = psm_object_open(cached_phandle, "object1", PSM_MODE_WRITE,
			     BIGGEST_SIZE,
			     &attrib, &oHandle);
	if (rv != WM_SUCCESS) {
		wmprintf("%s: psm_object_open failed : %d \r\n", __func__,  rv);
		return rv;
	}
	rv = psm_object_write(oHandle, obj, sizeof(obj));

	if (rv != WM_SUCCESS) {
		wmprintf("%s: psm_object_write failed : %d \r\n", __func__, rv);
		return rv;
	}
	rv = psm_object_close(&oHandle);
	if (rv != WM_SUCCESS) {
		wmprintf("psm_object_close failed : %d \r\n", rv);
		return rv;
	}

	uint64_t old_read_bytes = bytes_read;
	rv = psm_object_open(cached_phandle, "object1", PSM_MODE_READ,
			     BIGGEST_SIZE,
			     &attrib, &oHandle);
	if (rv < WM_SUCCESS) {
		wmprintf("psm_object_open failed : %d \r\n", rv);
		return rv;
	}
	rv = psm_object_read(oHandle, robj, sizeof(robj));

	if (rv != sizeof(robj)) {
		wmprintf("psm_object_read failed : %d \r\n", rv);
		return rv;
	}
	rv = psm_object_close(&oHandle);
	if (rv != WM_SUCCESS) {
		wmprintf("psm_object_close failed : %d \r\n", rv);
		return rv;
	}
	if ((rv = memcmp(robj, obj, sizeof(obj)))) {
		wmprintf("Verification failed: %d \r\n", rv);
		rv = -WM_FAIL;
		goto out_16;
	} else {
		rv = WM_SUCCESS;
		if (bytes_read != old_read_bytes) {
			wmprintf("Caching enabled but READ called \r\n");
			rv = -WM_FAIL;
		}
	}
	memset(robj, 0, sizeof(robj));
	rv = psm_get_variable(cached_phandle,
			      "object1",
			      robj,
			      sizeof(robj));
	if (rv <  0) {
		wmprintf("%s: Get Obj Failed: %d \r\n", __func__, rv);
		rv = -WM_FAIL;
		goto out_16;
	}

	if ((rv = memcmp(robj, obj, sizeof(obj)))) {
		wmprintf("Verification failed: %d \r\n", rv);
		rv = -WM_FAIL;
		goto out_16;
	} else {
		rv = WM_SUCCESS;
		if (bytes_read != old_read_bytes) {
			wmprintf("Caching enabled but READ called \r\n");
			rv = -WM_FAIL;
		}
	}
 out_16:

	return rv;
}
/* ---------------------------------------------------------------- */
/* ---------------------------------------------------------------- */
static void print_test_status(int rv, const char *test_name)
{
	if (rv < 0)
		wmprintf("Test Case : %s Failed \r\n", test_name);
	else
		wmprintf("Test Case : %s Successful OK \r\n", test_name);
}

static  void run_test(int option)
{
	int rv = 0;
	switch (option) {
	case 1:
		/* Test to write only one object continuously */
		rv = psm_test_case_01();
		print_test_status(rv, "psm_test_case_01");
		break;
	case 2:
		/* Test to write an object of the size bigger than
		   half of entire PSM partition size */
		rv = psm_test_case_02();
		print_test_status(rv, "psm_test_case_02");
		break;
	case 3:
		/* To read and write one object
		 * till entire PSM partition is consumed
		 */
		rv = psm_test_case_03();
		print_test_status(rv, "psm_test_case_03");
		break;
	case 4:
		rv = psm_test_case_04();
		print_test_status(rv, "psm_test_case_04");
		break;
	case 5:
		rv = psm_test_case_05();
		print_test_status(rv, "psm_test_case_05");
		break;
	case 6:
		rv = psm_test_case_06();
		print_test_status(rv, "psm_test_case_06");
		break;
	case 7:
		rv = psm_test_case_07();
		print_test_status(rv, "psm_test_case_07");
		break;
	case 8:
		rv = psm_test_case_08();
		print_test_status(rv, "psm_test_case_08");
		break;
	case 9:
		rv = psm_test_case_09();
		print_test_status(rv, "psm_test_case_09");
		break;

	case 10:
		rv = psm_test_case_10();
		print_test_status(rv, "psm_test_case_10");
		break;
	case 11:
		rv = psm_test_case_11();
		print_test_status(rv, "psm_test_case_11");
		break;
	case 12:
		rv = psm_test_case_12();
		print_test_status(rv, "psm_test_case_12");
		break;
	case 13:
		rv = psm_test_case_13();
		print_test_status(rv, "psm_test_case_13");
		break;
	case 14:
		rv = psm_test_case_14();
		print_test_status(rv, "psm_test_case_14");
		break;
	case 15:
		rv = psm_test_case_15();
		print_test_status(rv, "psm_test_case_15");
		break;
	case 16:
		rv = psm_test_case_16();
		print_test_status(rv, "psm_test_case_16");
		break;
	default:
		break;
	}
}

#define MAX_TEST_CNT 16
void test_operations(int option)
{
	unsigned int count = 0;

	if (option == 0) {
		for (count = MAX_TEST_CNT; count > 0; count--) {
			run_test(count);
		}
	} else {
		run_test(option);
	}
}

static const char *get_stats(uint64_t bytes, uint32_t *out_bytes)
{
	if (bytes / (1 << 30)) {
		*out_bytes = bytes / (1 << 30);
		return "GB";
	} else if (bytes / (1 <<  20)) {
		*out_bytes = bytes / (1 << 20);
		return "MB";
	} else if (bytes / (1 << 10))  {
		*out_bytes = bytes / (1 << 10);
		return "KB";
	} else {
		*out_bytes = bytes;
		return "bytes";
	}
}

int psm_test_main(int test_no)
{
	int rv = 0;
	uint32_t bytes = 0;

	swap_erases = 0;
	data_section_erases = 0;
	bytes_read = 0;
	bytes_written = 0;
	rv = psm_test_init(NULL);
	if (rv != 0) {
		wmprintf("PSM test init failed\r\n");
		return rv;
	}
	srand(wmtime_time_get_posix());

	test_operations(test_no);

	wmprintf("\r\n-----------------Statistics---------------------\r\n");
	const char *s = get_stats(bytes_read, &bytes);
	wmprintf(" Total Bytes Read : %u %s (%llu) \r\n", bytes,
		 s, bytes_read);
	s = get_stats(bytes_written, &bytes);
	wmprintf(" Total Bytes Written : %u %s (%llu) \r\n", bytes,
		 s, bytes_written);
	wmprintf(" Data Erase count : %lld\r\n", data_section_erases);
	wmprintf(" Swap Erase count : %lld\r\n", swap_erases);
	wmprintf("------------------------------------------------\r\n");
	psm_test_deinit();
	return 0;
}

/*----------------------------------------------------------------*/
/*---------------------CLI Functions-----------------------------------*/
/*----------------------------------------------------------------*/

static void print_usage()
{
	wmprintf("Usage : \r\n");
	wmprintf("psm-test [test_no] \r\n");
	wmprintf("         if test_no = 0 all test"
		 "cases will be executed\r\n");
	wmprintf("psm-power-safe-test \r\n");
	wmprintf("psm-compaction [size of object] [iteration]\r\n");
}

static void cmd_psm_test(int argc, char *argv[])
{
	uint32_t test_no = 0;
	if (argc < 2) {
		print_usage();
		return;
	}
	test_no = atoi(argv[1]);
	psm_test_main(test_no);
}

static void cmd_psm_power_safe_test(int argc, char *argv[])
{
	pm_init();
	int rv = psm_test_init(NULL);
	if (rv != 0) {
		wmprintf("PSM test init failed\r\n");
		return;
	}
	test_power_safe_psm();
	psm_test_deinit();
}

static void cmd_psm_read_only_test(int argc, char *argv[])
{
	/*
	  Algo:
	  - Open in r/w mode.
	  - Format
	  - Write single object.
	  - close psm
	  - open in ro mode.
	  - read-verify object.
	  - try writing different value to same object. Should fail.
	  - read-verify original value.
	*/

	int rv = psm_test_init(NULL);
	if (rv != 0) {
		wmprintf("%s: PSM r/w test init failed\r\n",
			 __func__);
		return;
	}

	rv = psm_format(cached_phandle);
	if (rv != 0) {
		wmprintf("Format Failed: %d \r\n", rv);
		return;
	}

	uint16_t obj_size = fdesc.fl_size / 3;
	uint8_t *data = os_mem_alloc(obj_size);
	if (!data) {
		wmprintf("%s: malloc failed\r\n",
			 __func__);
		return;
	}

	const int pattern = 0x5A;
	memset(data, pattern, obj_size);

	const char var[] = "var-test-ro";
	rv = psm_set_variable(cached_phandle, var, data, obj_size);
	if (rv != WM_SUCCESS) {
		wmprintf("%s: Failed to set var %s\n\r", __func__, var);
		os_mem_free(data);
		return;
	}

	psm_test_deinit();

	/***********************/

	psm_cfg_t psm_cfg = {
		.read_only = true,
	};

	rv = psm_test_init(&psm_cfg);
	if (rv != 0) {
		wmprintf("%s: PSM ro test init failed\r\n",
			 __func__);
		return;
	}

	/* Read verify */
	rv = psm_get_variable(cached_phandle, var, data, obj_size);
	if (rv != obj_size) {
		wmprintf("%s: (First) Failed to get var %s\n\r", __func__, var);
		os_mem_free(data);
		return;
	}

	int j;
	for (j = 0; j < obj_size; j++) {
		if (data[j] != pattern) {
			wmprintf("%s: (First) Verify %s failed @ %d\n\r",
				 __func__, var, j);
			os_mem_free(data);
			return;
		}
	}

	/* try to write */

	/* This should fail */
	rv = psm_set_variable(cached_phandle, var, data, obj_size);
	if (rv == WM_SUCCESS) {
		wmprintf("%s: Failed: Was able to write to var %s\n\r",
			 __func__, var);
		os_mem_free(data);
		return;
	}

	/* Second Read verify */
	rv = psm_get_variable(cached_phandle, var, data, obj_size);
	if (rv != obj_size) {
		wmprintf("%s: (Second) Failed to get var %s\n\r",
			 __func__, var);
		os_mem_free(data);
		return;
	}

	for (j = 0; j < obj_size; j++) {
		if (data[j] != pattern) {
			wmprintf("%s: (Second) Verify %s failed @ %d\n\r",
				 __func__, var, j);
			os_mem_free(data);
			return;
		}
	}

	os_mem_free(data);
	wmprintf("%s: Success\r\n", __func__);
	psm_test_deinit();
}

static void
cmd_psm_test_compaction(int argc, char *argv[])
{
	if (argc < 3) {
		print_usage();
		return;
	}
	uint32_t test_size = 0;
	test_size = atoi(argv[1]);

	uint32_t iter = 0;
	iter = atoi(argv[2]);

	nvram_iter = iter;
	nvram_object_size = test_size;
	nvram_cli_no = (uint32_t) CLI_2;
	if (!test_size) {
		wmprintf(" Give non zero value\r\n");
		print_usage();
		return;
	}
	nvram_object_size = test_size;

	psm_test_compaction(test_size);
	os_timer_delete(&pm4_timer);
}

static void cmd_psm_objects_dump()
{
	psm_test_init(NULL);
	psm_util_dump(cached_phandle);
	psm_test_deinit();
}

enum {
	ROT_INVALID,
	ROT_0,
	ROT_45,
	ROT_90,
	ROT_135,
};

static int rotator(void *param)
{
	if (!rotator_on)
		return WM_SUCCESS;

	static int count;

	if (count >= ROT_0)
		wmprintf("\b");

	switch (count) {
	case ROT_0:
		wmprintf("|");
		break;
	case ROT_45:
		wmprintf("/");
		break;
	case ROT_90:
		wmprintf("-");
		break;
	case ROT_135:
		wmprintf("\\");
		break;
	}

	count++;
	if (count == 5)
		count = ROT_0;

	rotator_on = false;

	return WM_SUCCESS;
}

static void cmd_psm_enable_rotator(int argc, char **argv)
{
	if (argc != 2) {
		wmprintf("No option given (Pass 1/0)\r\n");
		return;
	}

	int value = strtol(argv[1], NULL, 10);
	if (value != 0 &&
	    value != 1) {
		wmprintf("Invalid option\r\n");
		return;
	}

	sys_work_queue_init();
	static wq_handle_t wq_handle;

	if (value && !wq_handle) {
		wq_handle = sys_work_queue_get_handle();

		wq_job_t wq_job;
		memset(&wq_job, 0x00, sizeof(wq_job_t));
		wq_job.job_func = rotator;
		strcpy(wq_job.owner, "psmt");
		wq_job.periodic_ms = 2000;
		wq_job.initial_delay_ms = 5000;

		work_enqueue(wq_handle, &wq_job, NULL);
	} else if (!value) {
		work_dequeue_owner_all(wq_handle, "psmt");
		wq_handle = 0;
	}
}

static void cmd_psm_enable_secure(int argc, char **argv)
{
	if (argc != 2) {
		wmprintf("No option given (Pass 1/0)\r\n");
		return;
	}

	int value = strtol(argv[1], NULL, 10);
	if (value != 0 &&
	    value != 1) {
		wmprintf("Invalid option\r\n");
		return;
	}

	psm_secure_state(value);
}

static struct cli_command commands[] = {
	{"psm-test", "[test_no]", cmd_psm_test},
	{"psm-read-only-test", NULL, cmd_psm_read_only_test},
	{"psm-dump", NULL, cmd_psm_objects_dump},
	{"psm-power-safe-test", NULL, cmd_psm_power_safe_test},
	{"psm-compaction", "[object size] [iteration]",
	 cmd_psm_test_compaction},
	{"psm-enable-rotator", NULL, cmd_psm_enable_rotator},
	{"psm-enable-secure", NULL, cmd_psm_enable_secure}
};

int psm_test_cli_init(void)
{
	int i;

	for (i = 0; i < sizeof(commands) /
		     sizeof(struct cli_command); i++)
		if (cli_register_command(&commands[i]))
			return 1;
	if (nvram_cli_no == 2 && nvram_iter != 0) {
		psm_test_compaction(nvram_object_size);

	} else {
		nvram_object_size = 0;
		nvram_cli_no = 0;
		nvram_iter = 0;
	}

	return 0;
}

/*----------------------------------------------------------------*/
