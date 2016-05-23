#ifndef APP_PROVISIONING_H_
#define APP_PROVISIONING_H_

#include <provisioning.h>
#include <app_framework.h>

typedef enum {
	AF_EVT_EZCONN_PROV_START = APP_CTRL_EVT_INTERNAL + 1,
	AF_EVT_SNIFFER_NW_SET,
	AF_EVT_SNIFFER_MICRO_UP_REQUESTED,
	AF_EVT_RESET_TO_SMC,
} app_ezconnect_event_t;

/** Interrupt the provisioning process */
void app_interrupt_provisioning();

void display_provisioning_pin();
int app_prov_httpd_init(void);
int app_prov_wps_session_req(char *pin);

int app_set_scan_config_psm(struct prov_scan_config *scancfg);

int app_load_scan_config(struct prov_scan_config *scancfg);

int app_provisioning_get_type();

#endif				/*APP_PROVISIONING_H_ */
