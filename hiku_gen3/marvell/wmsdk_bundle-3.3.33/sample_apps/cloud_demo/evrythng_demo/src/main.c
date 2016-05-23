/*
 *  Copyright (C) 2008-2016, Marvell International Ltd.
 *  All Rights Reserved.
 */
/*
 * Evrythng Cloud demo application
 *
 * Summary:
 *
 * Sample Application demonstrating the use of Evrythng Cloud
 *
 * Description:
 *
 * This application communicates with Evrythng cloud once the device is
 * provisioned. Device can be provisioned using the psm CLIs as mentioned below.
 * After that, it periodically gets/updates (toggles) the state of board_led_1()
 * and board_led_2() from/to the Evrythng cloud.
 *
 */
#include <wm_os.h>
#include <app_framework.h>
#include <httpc.h>
#include <wmtime.h>
#include <cli.h>
#include <wmstdio.h>
#include <board.h>
#include <wmtime.h>
#include <psm.h>
#include <mdev_gpio.h>
#include <evrythng.h>
#include <led_indicator.h>
#include <critical_error.h>
#include "evrythng_tls_certificate.h"


/* Thread handle */
static os_thread_t app_thread;
/* Buffer to be used as stack */
static os_thread_stack_define(app_stack, 8 * 1024);

#define EVRY_PROPERTY_BUF_SIZE   64
#define EVRY_ACTION_BUF_SIZE   100
#define MAX_RETRY_CNT 3
#define EVRYTHNG_URL_SSL "ssl://mqtt.evrythng.com:443"

#define THNG_ID_LEN 25
#define API_KEY_LEN 81

#define EVRYTHNG_GET_TIME_URL "http://api.evrythng.com/time"
#define MAX_DOWNLOAD_DATA 150
#define MAX_URL_LEN 128


#define THNG_ID_VAL "UWh8pD5t8VpRkXAsFHKgSGce"
#define API_KEY_VAL "4fpFsYVO7EwZXjQb3ioNv8M0DRaQU1EKofrD5erKq9akYOcUehyCF5EWgLWJMZH7W9Ik2nkwikK8xcPj"

static char thng_id[THNG_ID_LEN];


/*------------------Global Variable Definitions ---------*/

/* The various properties of this specific device type which was defined by the
 * Evrythng Configurator */
char led1[] = "led1";
char target_led1[] = "target_led1";

char button_a_property[] = "button1";


/* These hold the LED gpio pin numbers */
static output_gpio_cfg_t led_1;
static output_gpio_cfg_t led_2;
/* These hold each pushbutton's count, updated in the callback ISR */
volatile uint32_t pushbutton_a_count;
volatile uint32_t pushbutton_a_count_prev = -1;
uint32_t led_1_state = 0;
static void evrythng_dummy_cb(void)
{
}


static void make_str(int val, char buff_out[], int buff_len)
{
	if (!buff_out)
		return;
	memset(buff_out, 0, buff_len);

	snprintf(buff_out, buff_len,
		 "[{\"value\":%d}]", val);
}
static void property_sub_cb(jobj_t *json_arr)
{
	char buff_send[EVRY_ACTION_BUF_SIZE];
	if (json_array_get_composite_object(json_arr, 0) != WM_SUCCESS) {
		wmprintf("Wrong json array\r\n");
		return;
	}
	char key[32];
	if (json_get_val_str(json_arr, "key",
			     key, sizeof(key)) != WM_SUCCESS) {
		wmprintf("Key doesn't exist\r\n");
		return;
	}

	int value_in = 0;
	if (json_get_val_int(json_arr, "value", &value_in)
	    != WM_SUCCESS) {
		wmprintf("Value doesn't exist\r\n");
		return;
	}

	if (strcmp(key, target_led1) == 0) {
		wmprintf("Value : %d\r\n", value_in);
		led_1_state = value_in;
		if (led_1_state)
			led_on(led_1);
		else
			led_off(led_1);
		led_1_state ^= 1;


		char *property = led1;
		/* Update to new state and inform cloud */
		led_1_state ^= 1;
		make_str(led_1_state, buff_send, sizeof(buff_send));
		wmprintf("Sending '%s %s' to "
			 "Evrythng\r\n",
			 property, buff_send);
		EvrythngPubThngProperty(thng_id,
					property,
					buff_send,
					1,
					evrythng_dummy_cb);


	}
}


static void set_device_time()
{
	http_session_t handle;
	http_resp_t *resp = NULL;
	char buf[MAX_DOWNLOAD_DATA];
	int64_t timestamp, offset;
	int size = 0;
	int status = 0;
	char url_name[MAX_URL_LEN];

	memset(url_name, 0, sizeof(url_name));
	strncpy(url_name, EVRYTHNG_GET_TIME_URL, strlen(EVRYTHNG_GET_TIME_URL));

	wmprintf("Get time from : %s\r\n", url_name);
	status = httpc_get(url_name, &handle, &resp, NULL);
	if (status != WM_SUCCESS) {
		wmprintf("Getting URL failed");
		return;
	}
	size = http_read_content(handle, buf, MAX_DOWNLOAD_DATA);
	if (size <= 0) {
		wmprintf("Reading time failed\r\n");
		goto out_time;
	}
	/*
	  If timezone is present in PSM
	  Get on http://api.evrythng.com/time?tz=<timezone>
	  The data will look like this
	  {
	  "timestamp":1429514751927,
	  "offset":-18000000,
	  "localTime":"2015-04-20T02:25:51.927-05:00",
	  "nextChange":1446361200000
	  }
	  If timezone is not presentin PSM
	  Get on http://api.evrythng.com/time
	  The data will look like this
	  {
	  "timestamp":1429514751927
	  }
	*/
	jsontok_t json_tokens[9];
	jobj_t json_obj;
	if (json_init(&json_obj, json_tokens, 10, buf, size) != WM_SUCCESS) {
		wmprintf("Wrong json string\r\n");
		goto out_time;
	}

	if (json_get_val_int64(&json_obj, "timestamp", &timestamp)
	    == WM_SUCCESS) {
		if (json_get_val_int64(&json_obj, "offset", &offset)
		    != WM_SUCCESS) {
			offset = 0;
		}
		timestamp = timestamp + offset;
	}
	wmtime_time_set_posix(timestamp/1000);
 out_time:
	http_close_session(&handle);
	return;
}


static int evrythng_configure(void)
{
	char client_id[10];
	int ret = WM_SUCCESS;

	memset(thng_id, 0, THNG_ID_LEN);

	strcpy(thng_id, THNG_ID_VAL);

	int i;
	for (i = 0; i < 9; i++) {
		client_id[i] = '0' + rand() % 10;
	}
	client_id[9] = '\0';

	evrythng_config_t config;
	memset(&config, 0, sizeof(config));
	config.url = EVRYTHNG_URL_SSL;
	config.api_key = API_KEY_VAL;
	config.client_id = client_id;
	config.tls_server_uri = EVRYTHNG_URL_SSL;
	config.cert_buffer = cert_buffer;
	config.cert_buffer_size = sizeof(cert_buffer) - 1;
	config.enable_ssl = 1;

	ret = -WM_FAIL;

	uint32_t cloud_connect_retry_cnt = MAX_RETRY_CNT;
	while (ret  != WM_SUCCESS && cloud_connect_retry_cnt--) {
		ret =  EvrythngConfigure(&config);
		wmprintf("Retrying .... \r\n");
		os_thread_sleep(2000);
	}
	return ret;
}

/* App defined critical error */
enum app_crit_err {
	CRIT_ERR_APP = CRIT_ERR_LAST + 1,
};

/* This function is defined for handling critical error.
 * For this application, we just stall and do nothing when
 * a critical error occurs.
 */

void critical_error(int crit_errno, void *data)
{
	wmprintf("Critical Error %d: %s\r\n", crit_errno,
			critical_error_msg(crit_errno));
	while (1)
		;
	/* do nothing -- stall */
}

/* This is the main evrythng  demo routine with polling loop.
 * This routine assumes the board and GPIOs are configured and a
 * network connection is established.
 */
static void evrythng_demo_main(os_thread_arg_t data)
{
	char *property = NULL;
	char buff_send[EVRY_ACTION_BUF_SIZE];
	wmprintf("EvrythngConfigure called\r\n");
	int ret = evrythng_configure();
	if (ret != WM_SUCCESS) {
		wmprintf("evrythng_configure failed : %d\r\n", ret);
		goto out;
	}
	wmprintf("Cloud Started\r\n");

	/* Subscribe for changes from Cloud */
	/* This also gets initial value from Cloud */

	EvrythngSubThngProperty(thng_id,
				target_led1,
				1,
				1,
				property_sub_cb);

	do {
		/* Any pushbutton A events */
		if (pushbutton_a_count_prev != pushbutton_a_count) {
			/* Toggle LED on board, if state=on, turn it off */
			if (led_1_state)
				led_on(led_1);
			else
				led_off(led_1);


			property = button_a_property;

			make_str(pushbutton_a_count_prev, buff_send, sizeof(buff_send));

			wmprintf("Sending '%s %s' to Evrythng Cloud\r\n",
				 property, buff_send);

			ret = EvrythngPubThngProperty(thng_id,
						      property,
						      buff_send,
						      1,
						      evrythng_dummy_cb);

			if (ret != WM_SUCCESS) {
				wmprintf("Sending property failed\r\n");
			} else {
				property = led1;
				/* Update to new state and inform cloud */
				led_1_state ^= 1;
				make_str(led_1_state, buff_send, sizeof(buff_send));
				wmprintf("Sending '%s %s' to "
					 "Evrythng\r\n",
					 property, buff_send);
				ret = EvrythngPubThngProperty(thng_id,
							      property,
							      buff_send,
							      1,
							      evrythng_dummy_cb);
				
				if (ret) {
					wmprintf("Sending property failed\r\n");
					/* Reset the state since
					 * the server didn't get it */
					led_1_state ^= 1;
				} else {
					pushbutton_a_count_prev
						= pushbutton_a_count;
				}
			}
		}
		os_thread_sleep(os_msec_to_ticks(2000));
	} while (1);
 out :
	os_thread_self_complete(NULL);
	return;
}

/* This function is called when push button A is pressed*/
static void pushbutton_cb_a()
{
	/* Little debounce with previous, only count if the server
	 * has been updated */
	if (pushbutton_a_count_prev == pushbutton_a_count) {
		pushbutton_a_count++;
	}
}


/* Configure GPIO pins to be used as LED and push button */
static void configure_gpios()
{
	mdev_t *gpio_dev;
	uint32_t pushbutton_a;

	/* Get the corresponding pin numbers using the board specific calls */
	/* also configures the gpio accordingly for LED */
	led_1 = board_led_1();
	led_2 = board_led_2();
	pushbutton_a = board_button_1();

	/* Initialize & Open handle to GPIO driver for push button
	 * configuration */
	gpio_drv_init();
	gpio_dev = gpio_drv_open("MDEV_GPIO");

	/* Register a callback for each push button interrupt */
	gpio_drv_set_cb(gpio_dev, pushbutton_a, GPIO_INT_FALLING_EDGE,
			NULL, pushbutton_cb_a);
	/* Close driver */
	gpio_drv_close(gpio_dev);
}



/* This is the main event handler for this project. The application framework
 * calls this function in response to the various events in the system.
 */
int common_event_handler(int event, void *data)
{
	int ret;
	static bool is_cloud_started;
	switch (event) {
	case AF_EVT_WLAN_INIT_DONE:
		ret = psm_cli_init();
		if (ret != WM_SUCCESS)
			wmprintf("Error: psm_cli_init failed\r\n");
		int i = (int) data;

		if (i == APP_NETWORK_NOT_PROVISIONED) {
			wmprintf("\r\nPlease provision the device "
				 "and then reboot it:\r\n\r\n");
			wmprintf("psm-set network ssid <ssid>\r\n");
			wmprintf("psm-set network security <security_type>"
				 "\r\n");
			wmprintf("    where: security_type: 0 if open,"
				 " 3 if wpa, 4 if wpa2\r\n");
			wmprintf("psm-set network passphrase <passphrase>"
				 " [valid only for WPA and WPA2 security]\r\n");
			wmprintf("psm-set network configured 1\r\n");
			wmprintf("pm-reboot\r\n\r\n");
		} else
			app_sta_start();

		break;
	case AF_EVT_NORMAL_CONNECTED:
		set_device_time();
		if (!is_cloud_started) {
			configure_gpios();
			os_thread_sleep(2000);
			ret = os_thread_create(
					       /* thread handle */
					       &app_thread,
					       /* thread name */
					       "evrythng_demo_thread",
					       /* entry function */
					       evrythng_demo_main,
					       /* argument */
					       0,
					       /* stack */
					       &app_stack,
					       /* priority - medium low */
					       OS_PRIO_3);
			is_cloud_started = true;
		}
		break;
	default:
		break;
	}

	return 0;
}

static void modules_init()
{
	int ret;

	ret = wmstdio_init(UART0_ID, 0);
	if (ret != WM_SUCCESS) {
		wmprintf("Error: wmstdio_init failed\r\n");
		critical_error(-CRIT_ERR_APP, NULL);
	}

	ret = cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("Error: cli_init failed\r\n");
		critical_error(-CRIT_ERR_APP, NULL);
	}
	ret = wlan_cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("Error: wlan_cli_init failed\r\n");
		critical_error(-CRIT_ERR_APP, NULL);
	}

	ret = pm_cli_init();
	if (ret != WM_SUCCESS) {
		wmprintf("Error: pm_cli_init failed\r\n");
		critical_error(-CRIT_ERR_APP, NULL);
	}
	/* Initialize time subsystem.
	 *
	 * Initializes time to 1/1/1970 epoch 0.
	 */
	ret = wmtime_init();
	if (ret != WM_SUCCESS) {
		wmprintf("Error: wmtime_init failed\r\n");
		critical_error(-CRIT_ERR_APP, NULL);
	}
	return;
}

int main()
{
	modules_init();

	wmprintf("Build Time: " __DATE__ " " __TIME__ "\r\n");

	/* Start the application framework */
	if (app_framework_start(common_event_handler) != WM_SUCCESS) {
		wmprintf("Failed to start application framework\r\n");
		critical_error(-CRIT_ERR_APP, NULL);
	}
	return 0;
}
