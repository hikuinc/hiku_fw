/*
 * aws_client.c
 *
 *  Created on: May 24, 2016
 *      Author: rajan-work
 */

#include "aws_client.h"
#include "aws_root_ca_cert.h"

/** forward declarations */

static int aws_starter_load_configuration(ShadowParameters_t *sp);
void shadow_update_status_cb(const char *pThingName, ShadowActions_t action,
			     Shadow_Ack_Status_t status,
			     const char *pReceivedJsonDocument,
			     void *pContextData);
static void aws_shadow_yield(os_thread_arg_t data);
static int aws_create_shadow_yield_thread();
void led_indicator_cb(const char *p_json_string,
		      uint32_t json_string_datalen,
		      jsonStruct_t *p_context);
int aws_publish_property_state(ShadowParameters_t *sp);
static void aws_thread(os_thread_arg_t data);
/** global variable declarations */

static MQTTClient_t mqtt_client;
static enum state device_state = AWS_DISCONNECTED;

/* Thread handle */
static os_thread_t aws_starter_thread;
/* Buffer to be used as stack */
static os_thread_stack_define(aws_starter_stack, 8 * 1024);
/* Thread handle */
static os_thread_t aws_shadow_yield_thread;
/* Buffer to be used as stack */
static os_thread_stack_define(aws_shadow_yield_stack, 2 * 1024);
/* aws iot url */
static char url[128];

#define AMAZON_ACTION_BUF_SIZE  100
#define VAR_LED_1_PROPERTY      "led"
#define VAR_BUTTON_A_PROPERTY   "pb"
#define RESET_TO_FACTORY_TIMEOUT 5000
#define BUFSIZE                  128



static char client_cert_buffer[AWS_PUB_CERT_SIZE] = {};
static char private_key_buffer[AWS_PRIV_KEY_SIZE] = {};
#define THING_LEN 126
#define REGION_LEN 16
static char thing_name[THING_LEN] = {};


/** implementations */

/* populate aws shadow configuration details */
static int aws_starter_load_configuration(ShadowParameters_t *sp)
{
	int ret = WM_SUCCESS;
	char region[REGION_LEN];
	memset(region, 0, sizeof(region));

	/* read configured thing name from the persistent memory */
	ret = read_aws_thing(thing_name, THING_LEN);
	if (ret == WM_SUCCESS) {
		sp->pMyThingName = thing_name;
	} else {
		/* if not found in memory, take the default thing name */
		sp->pMyThingName = AWS_IOT_MY_THING_NAME;
	}
	sp->pMqttClientId = AWS_IOT_MQTT_CLIENT_ID;

	/* read configured region name from the persistent memory */
	ret = read_aws_region(region, REGION_LEN);
	if (ret == WM_SUCCESS) {
		snprintf(url, sizeof(url), "data.iot.%s.amazonaws.com",
			 region);
	} else {
		snprintf(url, sizeof(url), "data.iot.%s.amazonaws.com",
			 AWS_IOT_MY_REGION_NAME);
	}
	sp->pHost = url;
	sp->port = AWS_IOT_MQTT_PORT;
	sp->pRootCA = rootCA;

	/* read configured certificate from the persistent memory */
	ret = read_aws_certificate(client_cert_buffer, AWS_PUB_CERT_SIZE);
	if (ret != WM_SUCCESS) {
		hiku_a("Failed to configure certificate. Returning!\r\n");
		return -WM_FAIL;
	}
	sp->pClientCRT = client_cert_buffer;

	/* read configured private key from the persistent memory */
	ret = read_aws_key(private_key_buffer, AWS_PRIV_KEY_SIZE);
	if (ret != WM_SUCCESS) {
		hiku_a("Failed to configure key. Returning!\r\n");
		return -WM_FAIL;
	}
	sp->pClientKey = private_key_buffer;

	return ret;
}

void shadow_update_status_cb(const char *pThingName, ShadowActions_t action,
			     Shadow_Ack_Status_t status,
			     const char *pReceivedJsonDocument,
			     void *pContextData) {

	if (status == SHADOW_ACK_TIMEOUT) {
		hiku_a("Shadow publish state change timeout occurred\r\n");
	} else if (status == SHADOW_ACK_REJECTED) {
		hiku_a("Shadow publish state change rejected\r\n");
	} else if (status == SHADOW_ACK_ACCEPTED) {
		hiku_a("Shadow publish state change accepted\r\n");
	}
}

/* shadow yield thread which periodically checks for data */
static void aws_shadow_yield(os_thread_arg_t data)
{
	while (1) {
		/* periodically check if any data is received on socket */
		aws_iot_shadow_yield(&mqtt_client, 500);
	}
}

/* create shadow yield thread */
static int aws_create_shadow_yield_thread()
{
	int ret;
	ret = os_thread_create(
		/* thread handle */
		&aws_shadow_yield_thread,
		/* thread name */
		"awsShadowYield",
		/* entry function */
		aws_shadow_yield,
		/* argument */
		0,
		/* stack */
		&aws_shadow_yield_stack,
		/* priority */
		OS_PRIO_3);
	if (ret != WM_SUCCESS) {
		hiku_a("Failed to create shadow yield thread: %d\r\n", ret);
		return -WM_FAIL;
	}
	return WM_SUCCESS;
}

/* This function will get invoked when led state change request is received */
void led_indicator_cb(const char *p_json_string,
		      uint32_t json_string_datalen,
		      jsonStruct_t *p_context) {
	int state;
/*	if (p_context != NULL) {
		state = *(int *)(p_context->pData);
		if (state) {
			led_on(led_1);
			led_requested_state = 1;
		} else {
			led_off(led_1);
			led_requested_state = 0;
		}
	}*/
}

/* Publish thing state to shadow */
int aws_publish_property_state(ShadowParameters_t *sp)
{
	char buf_out[BUFSIZE];
	int ret = WM_SUCCESS;

	/* On receiving led state change notification from cloud, change
	 * the state of the led on the board in callback function and
	 * publish updated state on configured topic.
	 */
/*	if (led_requested_state != led_state) {
		led_state = led_requested_state;
		snprintf(buf_out, BUFSIZE, "{\"state\": {\"reported\":{"
			 "\"%s\":%d}}}", VAR_LED_1_PROPERTY, led_state);
		ret = aws_iot_shadow_update(&mqtt_client,
					    sp->pMyThingName,
					    buf_out,
					    shadow_update_status_cb,
					    NULL,
					    10, true);
		if (ret != WM_SUCCESS) {
			hiku_a("Failed to publish requested state of "
				 "the led\r\n");
			return ret;
		}
	}*/
	return ret;
}

/* application thread */
static void aws_thread(os_thread_arg_t data)
{
	int led_delta_state = 0, ret;
	jsonStruct_t led_indicator;
	ShadowParameters_t sp;

	aws_iot_mqtt_init(&mqtt_client);

	ret = aws_starter_load_configuration(&sp);
	if (ret != WM_SUCCESS) {
		hiku_a("aws shadow configuration failed : %d\r\n", ret);
		goto out;
	}

	ret = aws_iot_shadow_init(&mqtt_client);
	if (ret != WM_SUCCESS) {
		hiku_a("aws shadow init failed : %d\r\n", ret);
		goto out;
	}

	ret = aws_iot_shadow_connect(&mqtt_client, &sp);
	if (ret != WM_SUCCESS) {
		hiku_a("aws shadow connect failed : %d\r\n", ret);
		goto out;
	}

	/* indication that device is connected and cloud is started */
	led_on(board_led_2());
	hiku_a("Cloud Started\r\n");

	/* configures property of a thing */
	led_indicator.cb = led_indicator_cb;
	led_indicator.pData = &led_delta_state;
	led_indicator.pKey = "led";
	led_indicator.type = SHADOW_JSON_INT8;

	/* subscribes to delta topic of the configured thing */
	ret = aws_iot_shadow_register_delta(&mqtt_client, &led_indicator);
	if (ret != WM_SUCCESS) {
		hiku_a("Failed to subscribe to shadow delta %d\r\n", ret);
		goto out;
	}

	/* creates a thread which will wait for incoming messages, ensuring the
	 * connection is kept alive with the AWS Service
	 */
	aws_create_shadow_yield_thread();

	while (1) {
		/* Implement application logic here */

		if (device_state == AWS_RECONNECTED) {
			ret = aws_iot_shadow_connect(&mqtt_client, &sp);
			if (ret != WM_SUCCESS) {
				hiku_a("aws shadow reconnect failed: "
					 "%d\r\n", ret);
				goto out;
			} else {
				device_state = AWS_CONNECTED;
				led_on(board_led_2());
			}
		}

		ret = aws_publish_property_state(&sp);
		if (ret != WM_SUCCESS)
			hiku_a("Sending property failed\r\n");

		os_thread_sleep(1000);
	}

	ret = aws_iot_shadow_disconnect(&mqtt_client);
	if (NONE_ERROR != ret) {
		hiku_a("aws iot shadow error %d\r\n", ret);
	}

out:
	os_thread_self_complete(NULL);
	return;
}

void aws_wlan_event_normal_link_lost(void *data)
{
	/* led indication to indicate link loss */
	aws_iot_shadow_disconnect(&mqtt_client);
	device_state = AWS_DISCONNECTED;
}

void aws_wlan_event_normal_connect_failed(void *data)
{
	/* led indication to indicate connect failed */
	aws_iot_shadow_disconnect(&mqtt_client);
	device_state = AWS_DISCONNECTED;
}

/* This function gets invoked when station interface connects to home AP.
 * Network dependent services can be started here.
 */
void aws_wlan_event_normal_connected(void *data)
{
	int ret;
	/* Default time set to 1 October 2015 */
	time_t time = 1443657600;

	hiku_a("Connected successfully to the configured network\r\n");

	if (!device_state) {
		/* set system time */
		wmtime_time_set_posix(time);

		/* create cloud thread */
		ret = os_thread_create(
			/* thread handle */
			&aws_starter_thread,
			/* thread name */
			"awsStarterDemo",
			/* entry function */
			aws_thread,
			/* argument */
			0,
			/* stack */
			&aws_starter_stack,
			/* priority */
			OS_PRIO_3);
		if (ret != WM_SUCCESS) {
			hiku_a("Failed to start cloud_thread: %d\r\n", ret);
			return;
		}
	}

	if (!device_state)
		device_state = AWS_CONNECTED;
	else if (device_state == AWS_DISCONNECTED)
		device_state = AWS_RECONNECTED;
}

int aws_client_init(void)
{
	hiku_a("Build Time: " __DATE__ " " __TIME__ "\r\n");
	hiku_a("AWS Client Started!! \r\n");

	/* configure pushbutton on device to perform reset to factory */
	//configure_reset_to_factory();
	/* configure led and pushbutton to communicate with cloud */
	//configure_led_and_button();

	/* This api adds aws iot configuration support in web application.
	 * Configuration details are then stored in persistent memory.
	 */
	enable_aws_config_support();

	return 0;
}
