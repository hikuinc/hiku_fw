/*
 *  Copyright (C) 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* main.c
 * Entry point into the application specific code.
 */

/* System includes */
#include <wmstdio.h>
#include <string.h>
#include <wmtime.h>
#include <wm_os.h>
#include <wm_wlan.h>
#include <rfget.h>
#include <wmsysinfo.h>
#include <diagnostics.h>
#include <mdev_pm.h>
#include <cli.h>
#include <stdlib.h>
#include <psm.h>
#include <wm_net.h>
#include <ttcp.h>


/* Generic Application includes */
#include <app_framework.h>
/* Application specific includes */
#include <app.h>

extern void pm_pm0_in_idle();
static void pm_mcu_state_pm1()
{
	os_disable_all_interrupts();
	PMU_SetSleepMode(PMU_PM1);
	__asm volatile ("wfi");
}

/* Get low power configuration of wlan */
psm_hnd_t psm_get_handle();

#define MAX_PSM_VAL		65
#define VAR_NET_LOWPOWER	"network.lowpower"
#define WLAN_LOW_POWER		1
#define WLAN_FULL_POWER		0

#ifdef CONFIG_CPU_MW300
/* We need to initialize psm_module explicitly as it has to be accessed
 * before app framework is started */
static psm_hnd_t demo_app_psm_hnd;

static int pm_mcu_wifi_demo_psm_init(void)
{
	int ret;
	struct partition_entry *p;
	flash_desc_t fl;

	ret = part_init();
	if (ret != WM_SUCCESS) {
		PM_MCU_WIFI_DEMOAPP_LOG("Could not read partition table\r\n");
		return ret;
	}
	p = part_get_layout_by_id(FC_COMP_PSM, NULL);
	if (!p) {
		PM_MCU_WIFI_DEMOAPP_LOG("No psm partition found\r\n");
		return -WM_FAIL;
	}

	part_to_flash_desc(p, &fl);
	ret = psm_module_init(&fl, &demo_app_psm_hnd, NULL);
	if (ret) {
		PM_MCU_WIFI_DEMOAPP_LOG("Failed to init PSM,"
			"status code %d\r\n", ret);
		return -WM_FAIL;
	}

	return WM_SUCCESS;
}

static int wlan_get_low_power_config()
{
	int var_size;
	char psm_val[MAX_PSM_VAL];

	if (pm_mcu_wifi_demo_psm_init() != WM_SUCCESS)
		return WLAN_FULL_POWER;

	var_size = psm_get_variable_str(demo_app_psm_hnd, VAR_NET_LOWPOWER,
			       psm_val, sizeof(psm_val));

	psm_module_deinit(&demo_app_psm_hnd);

	if (var_size > 0)
		return atoi(psm_val) & 0x1;

	/* Default is full power */
	return WLAN_FULL_POWER;
}
#endif
char pm_mcu_wifi_demo_hostname[30];
char pm_mcu_wifi_demo_ssid[30];

#ifdef CONFIG_CPU_MW300
static void wlan_poweroff_usage_help()
{
	wmprintf("Pre-conditions :\r\n");
	wmprintf(" UAP network should be stopped"
	" using command wlan-stop-network\r\n");
	wmprintf(" Station should be disconnected using "
	"command wlan-disconnect\r\n");
	wmprintf(" Use command wlan-stat to see status\r\n");
	wmprintf("For mcu (PM2) + wifi (pdn):\r\n");
	wmprintf("Use pm-mcu-wifi-pdn 2 \r\n");

	wmprintf("For mcu (PM3) + wifi (pdn):\r\n");
	wmprintf("Use pm-mcu-wifi-pdn 3 \r\n");

	wmprintf("For mcu (PM4) + wifi (pdn):\r\n");
	wmprintf("Use pm-mcu-wifi-pdn 4 \r\n");
}

static int wlan_validate_connection_state()
{
	if (is_uap_started() || is_sta_connected())
		return -WM_FAIL;

	enum wlan_ps_mode ps_state;
	int ret = wlan_get_ps_mode(&ps_state);
	if (ret != WM_SUCCESS)
		return -WM_FAIL;
	if (ps_state != WLAN_ACTIVE)
		return -WM_FAIL;

	return WM_SUCCESS;
}

static void wifi_poweroff(int argc, char **argv)
{
	if (argc == 2) {
		if (!strcmp(argv[1], "-h")) {
			wlan_poweroff_usage_help();
			return;
		}
	}
	int state = atoi(argv[1]);
	if (state < 2 || state > 4) {
		wlan_poweroff_usage_help();
		return;
	}
	int retcode = WM_SUCCESS;
	retcode = wlan_validate_connection_state();

	if (retcode == -WM_FAIL) {
		wlan_poweroff_usage_help();
		return;
	}
	/* Deinit Wlan */
	wlan_deinit();

	/* Disable Interrupts */
	PMU_WakeupSrcIntMask(PMU_WAKEUP_WLAN, MASK);
	NVIC_DisableIRQ(WIFIWKUP_IRQn);

	/* Change System Clock source to RC32M */
	CLK_SystemClkSrc(CLK_RC32M);

	/* On Wifi Poweroff system, CPU runs @32M
	* SFLL not required */
	/* FIXME: If required keep board_cpu_freq() unchanged */
	CLK_SfllDisable();

	/* Switch off XTAL */
	CLK_RefClkDisable(CLK_XTAL_REF);
       /* Reconfigure SysTick config to run at 32M */
	SysTick->LOAD =  (32000000 / configTICK_RATE_HZ) - 1UL;
	SysTick->VAL = 0;

	/* Wifi Poweroff */
	PMU_PowerDownWLAN();
	/* MCU state can be PM2/PM3/PM4  */
	pm_mcu_state(state, 0);
}
#endif

static struct cli_command pm_mcu_wifi_demo_cmds[] = {
	{"pm-mcu-state-pm0",
	"<NONE>",
	 pm_pm0_in_idle},
	{ "pm-mcu-state-pm1",
	   "<NONE>",
	   pm_mcu_state_pm1},
#ifdef CONFIG_CPU_MW300
	{"pm-mcu-wifi-pdn", "Use pm-mcu-wifi-pdn -h for help", wifi_poweroff},
#endif
};



static void pm_mcu_wifi_demo_configure_defaults()
{
	uint8_t my_mac[6];

	wlan_get_mac_address(my_mac);
	/* Provisioning SSID */
	snprintf(pm_mcu_wifi_demo_ssid, sizeof(pm_mcu_wifi_demo_ssid),
		 "wmdemo-%02X%02X", my_mac[4], my_mac[5]);
	/* Default hostname */
	snprintf(pm_mcu_wifi_demo_hostname,
		 sizeof(pm_mcu_wifi_demo_hostname),
		 "wmdemo-%02X%02X", my_mac[4], my_mac[5]);
}


int pm_mcu_wifi_demo_app_event_handler(int event, void *data)
{
	char ip[16];
	int i;
	PM_MCU_WIFI_DEMOAPP_LOG
		("Received event %d\r\n", event);

	switch (event) {
	case AF_EVT_WLAN_INIT_DONE:
		i = (int) data;
		if (i == APP_NETWORK_NOT_PROVISIONED) {
			/* Include indicators if any */
			wmprintf("\r\n Please provision the device "
				 "using PSM CLI \r\n");
			wmprintf("psm-set network ssid  <network_name>\r\n");
			wmprintf("psm-set network security <security_type>"
				 "\r\n");
			wmprintf("security_type: 0: open, 3: WPA-PSK, "
				 "4: WPA2-PSK, 5: WPA/WPA2 PSK mixed mode\r\n");
			wmprintf(" If  security type is 3 (wpa) or 4 (wpa2)"
				 "\r\n");
			wmprintf("psm-set network passphrase <passphrase>\r\n");
			wmprintf("psm-set network lan DYNAMIC\r\n");
			wmprintf("psm-set network configured 1\r\n");
			wmprintf(" Optionally set the low power mode of"
				 " wlan\r\n");
			wmprintf("psm-set network lowpower <enable_flag>"
				 "\r\n");
			wmprintf(" enable_flag: 1: enable, 0: disable"
				 "\r\n\r\n");
		} else
			app_sta_start();
		break;
	case AF_EVT_NORMAL_CONNECTING:
		/* Connecting attempt is in progress */
		net_dhcp_hostname_set(pm_mcu_wifi_demo_hostname);
		break;
	case AF_EVT_NORMAL_CONNECTED:
		/* We have successfully connected to the network. Note that
		 * we can get here after normal bootup or after successful
		 * provisioning.
		 */
		app_network_ip_get(ip);
		wmprintf("\r\n Connected with IP:%s\r\n", ip);
		ttcp_init();
		break;
	case AF_EVT_NORMAL_CONNECT_FAILED:
		/* One connection attempt to the network has failed. Note that
		 * we can get here after normal bootup or after an unsuccessful
		 * provisioning.
		 */
		break;
	case AF_EVT_NORMAL_LINK_LOST:
		/* We were connected to the network, but the link was lost
		 * intermittently.
		 */
	case AF_EVT_NORMAL_USER_DISCONNECT:
		break;
	case AF_EVT_PS_ENTER:
		wmprintf("Power save enter\r\n");
		break;
	case AF_EVT_PS_EXIT:
		wmprintf("Power save exit\r\n");
		break;
	default:
		PM_MCU_WIFI_DEMOAPP_DBG
			("Not handling event %d\r\n", event);
		break;
	}
	return 0;
}


int main()
{
	int ret;

	/* Initialize console on uart0 */
	wmstdio_init(UART0_ID, 0);

#ifdef CONFIG_CPU_MW300
	int low_pwr;
	/* if low power, enable it before wlan_init() */
	low_pwr = wlan_get_low_power_config();
	if (low_pwr == WLAN_LOW_POWER) {
		PM_MCU_WIFI_DEMOAPP_LOG("Enabling wlan low power mode\r\n");
		wlan_enable_low_pwr_mode();
	} else
		PM_MCU_WIFI_DEMOAPP_LOG("Enabling wlan full power mode\r\n");
#endif
	/* Initialize WM core modules */
	ret = wm_core_and_wlan_init();

	if (ret) {
		wmprintf("Error initializing WLAN subsystem. Reason: %d\r\n",
				ret);
		/* System core init failed, just wait forever here */
		while (1)
			;
	}
	ret = psm_cli_init();
	if (ret) {
		wmprintf("Error registering PSM commands. Reason: %d\r\n",
				ret);
		/* System core init failed, just wait forever here */
		while (1)
			;
	}
	wlan_cli_init();
	wlan_pm_cli_init();
	pm_mcu_cli_init();
	wlan_iw_cli_init();
	wmprintf("[pm_mcu_wifi_demo] Build Time: "__DATE__" "__TIME__"\r\n");
	sysinfo_init();
	pm_mcu_wifi_demo_configure_defaults();
	/* Disable sending null packets */
	wlan_configure_null_pkt_interval(-1);
	int i;
	for (i = 0; i < sizeof(pm_mcu_wifi_demo_cmds) /
			sizeof(struct cli_command); i++)
		cli_register_command(&pm_mcu_wifi_demo_cmds[i]);
	/* Start the application framework */
	if (app_framework_start(pm_mcu_wifi_demo_app_event_handler)
	    != WM_SUCCESS) {
		wmprintf("Failed to start application framework\r\n");
		while (1)
			;
	}
	return 0;
}
