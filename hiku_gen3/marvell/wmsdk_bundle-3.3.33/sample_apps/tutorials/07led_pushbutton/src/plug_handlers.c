/*
 *  Copyright (C) 2008-2016 Marvell International Ltd.
 *  All Rights Reserved.
 */
#include <wmstdio.h>
#include <stdio.h>
#include <string.h>
#include <httpd.h>
#include <json_parser.h>
#include <push_button.h>
#include <led_indicator.h>
#include <psm.h>
#include <pwrmgr.h>

#define RESET_TO_FACTORY_TIMEOUT 5000

bool plug_state = true;

/* The GPIO numbers below are valid only for mw300_rd board.
 * Please use appropriate numbers as per the connections
 * on your board.
 */
/* LED to indicate the plug's state */
static output_gpio_cfg_t plug_led = {
	.gpio = GPIO_40,
	.type = GPIO_ACTIVE_LOW,
};

/* Multi-function push button */
static input_gpio_cfg_t pushbutton = {
	.gpio = GPIO_24,
	.type = GPIO_ACTIVE_LOW
};

/* Indicate the plug state on LED */
static void indicate_plug_state(bool plug_state)
{
	wmprintf("Plug state set to %d\r\n", plug_state);
	if (plug_state)
		led_on(plug_led);
	else
		led_off(plug_led);
}

/* Push button callback for momentary press.
 * Toggles plug state.
 */
static void set_plug_state_cb(int pin, void *data)
{
	plug_state = !plug_state;
	indicate_plug_state(plug_state);
}
/* Push button calback for press and hold of 5+ secs.
 * Does a Reset to factory.
 */
static void reset_to_factory_cb(int pin, void *data)
{
	wmprintf("Resetting to factory defaults\r\n");
	/* Erase the PSM */
	psm_erase_and_init();
	/* Reboot the SoC */
	pm_reboot_soc();
}
/* GPIO_24 is configured as a push button to perform 2
 * different actions
 * 1. Momentory push: Toggle plug state.
 * 2. Press and hold for more than 5 sec: Reset to factory.
 */
void configure_push_button()
{
	push_button_set_cb(pushbutton,
			set_plug_state_cb,
			0, 0, NULL);
	push_button_set_cb(pushbutton,
			reset_to_factory_cb,
			RESET_TO_FACTORY_TIMEOUT, 0, NULL);
	indicate_plug_state(plug_state);
}

static int plug_http_get_power_state(httpd_request_t *req)
{
	char content[30];
	snprintf(content, sizeof(content), "{ \"power\": %d }",
		 plug_state ? 1 : 0);
	return httpd_send_response(req, HTTP_RES_200, content,
				   strlen(content), HTTP_CONTENT_JSON_STR);
}

#define NUM_TOKENS 6
static int plug_http_set_power_state(httpd_request_t *req)
{
	char plug_data[30];

	int ret = httpd_get_data(req, plug_data, sizeof(plug_data));
	if (ret < 0) {
		wmprintf("Failed to get post request data\r\n");
		return ret;
	}

	/* Parse the JSON data */
	jsontok_t json_tokens[NUM_TOKENS];
	jobj_t jobj;
	ret = json_init(&jobj, json_tokens, NUM_TOKENS, plug_data,
			req->body_nbytes);
	if (ret != WM_SUCCESS)
		return -WM_FAIL;

	int state;
	if (json_get_val_int(&jobj, "power", &state) != WM_SUCCESS)
		return -1;

	plug_state = state;

	indicate_plug_state(plug_state);

	return httpd_send_response(req, HTTP_RES_200, HTTPD_JSON_SUCCESS,
				   strlen(HTTPD_JSON_SUCCESS),
				   HTTP_CONTENT_JSON_STR);
}

/* Define the JSON handler */
struct httpd_wsgi_call plug_wsgi_handlers[] = {
	{"/plug", HTTPD_DEFAULT_HDR_FLAGS | HTTPD_HDR_ADD_PRAGMA_NO_CACHE,
	 0, plug_http_get_power_state, plug_http_set_power_state, NULL, NULL},
};

static int plug_wsgi_handlers_no =
	sizeof(plug_wsgi_handlers) / sizeof(struct httpd_wsgi_call);

/*
 * Register Web-Service handlers
 *
 */
int register_httpd_plug_handlers()
{
	return httpd_register_wsgi_handlers(plug_wsgi_handlers,
					    plug_wsgi_handlers_no);
}
