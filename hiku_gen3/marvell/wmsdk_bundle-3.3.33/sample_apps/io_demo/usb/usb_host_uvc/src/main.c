/*
 *  Copyright (C) 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 *
 *  Simple Wireless video streaming application using Micro-AP and USB Host
 *  Stack.
 *
 *  Summary:
 *
 *  A USB camera is connected to the device over the USB-OTG interface. The
 *  application starts a Micro-AP network and a Web Server. The video stream
 *  read from the USB camera is streamed over this Web Server.
 *
 *  Description:
 *
 *  The application is written using Application Framework that simplifies
 *  development of WLAN networking applications.
 *
 *  WLAN Initialization:
 *
 *  When the application framework is started, it starts up the WLAN
 *  sub-system and initializes the network stack. We receive the event
 *  when the WLAN subsystem has been started and initialized.
 *
 *  We start a Micro-AP Network along with a DHCP server. This will
 *  create a WLAN network and also creates a IP network interface
 *  associated with that WLAN network. The DHCP server allows devices
 *  to associate and connect to this network and establish an IP level
 *  connection. We also initialize usb host driver for video class by
 *  providing appropriate format and resolution settings.
 *
 *  When the network is initialized and ready, we will receive a
 *  Micro-AP started event.
 *
 *  Micro-AP Started:
 *
 *  The Micro-AP network is now up and ready.
 *
 *  We start the web-server which will listen to incoming http connections (e.g,
 *  from a browser).
 *
 *  We register a HTTP handler that is accessible at
 *  http://192.168.10.1/video. This handler reads the data stream from the USB
 *  camera and sends this stream to the connected clients (e.g. browser).
 *
 *  /video HTTP Handler:
 *
 *  When invoked, this handler performs the initial enumeration and setup phase
 *  for the USB video class device. Once data, the handler reads the data stream
 *  from the USB camera and sends this data to the connected web client. The
 *  data streaming continuous until the connection is closed by the client.
 *
 *  /capture HTTP Handler:
 *
 *  When invoked, this handler captures a single image and sends
 *  this image to the connected web client. Once done, this handler exits.
 *  Client side can invoke this as follows:
 *  curl http://<ip_address_of_DUT>/capture -o img.jpeg
 *
 *  For more details on usage, please refer to README.usb_host_uvc at same
 *  level.
 */

#include <wmstdio.h>
#include <wm_os.h>
#include <wm_net.h>
#include <mdev_usb_host.h>
#include <wmsysinfo.h>
#include <wlan.h>
#include <dhcp-server.h>
#include <httpd.h>
#include <http_parse.h>
#include <http-strings.h>
#include <app_framework.h>
#include <wmtime.h>
#include <appln_dbg.h>
#include <appln_cb.h>
#include <cli.h>
#include <mrvlperf.h>
#include <snc292a_xu.h>

/* The frame number that is to be captured through /capture WSGI */
#define FRAME_NO 15

/* Buffer to be used for sending http chunk responses */
static uint8_t buf[1024];

/* Buffer passed to USB UVC Driver to be used as circular buffer to store
 * video data.
 */
#ifdef CONFIG_XIP_ENABLE
#define VID_BUF_LEN (200 * 1024)
#else
#define VID_BUF_LEN (64 * 1024)
#endif

/* For MC200, heap must be in SRAM1 which is small compared to SRAM0.
 * Hence, video_buffer cannot be dynamically allocated from heap.
 * Instead, it will be statically allocated from .bss which is in SRAM0.
 */
#ifdef CONFIG_CPU_MC200
static uint8_t video_buffer[VID_BUF_LEN];
#else
static uint8_t *video_buffer;
#endif

static mdev_t *p;

/*-----------------------Global declarations----------------------*/
appln_config_t appln_cfg = {
	.ssid = "Marvell Demo",
	.passphrase = "marvellwm",
	.hostname = "mdemo"
};

static unsigned int frame_cnt;
static unsigned char hdrs_sent, incomplete_frame;

#define START_INDICATOR 0xd8
#define END_INDICATOR 0xd9

unsigned char *get_image_indicator(unsigned char *b, uint32_t size, unsigned
			char c)
{
	int i = 0;

	while (i < size) {
		if (*b == 0xff) {
			if (((i + 1) < size) && (*(b + 1) == c))
				return b;
		}
		b++;
		i++;
	}
	return NULL;
}

static int dump_single_image(httpd_request_t *req)
{
	int err, ret;
	uint32_t cnt, len;

	cnt = 1024, len = 0;
	hdrs_sent = 0;
	incomplete_frame = 0;
	frame_cnt = 0;

	ret = usb_host_drv_start(p);
	if (ret != WM_SUCCESS) {
		wmprintf("Failed to start host driver: %d\r\n", ret);
		err = httpd_send_chunk(req->sock, NULL, 0);
		err = WM_SUCCESS;
		return err;
	}

	while (1) {
		ret = usb_host_drv_read(p, buf, cnt);
		if (ret <= 0) {
			/* Backoff ?*/
			os_thread_sleep(2);
			continue;
		}

		unsigned char *p1 = (unsigned char *) buf;
		unsigned char *start = p1;
		uint32_t size = ret;
		unsigned char *end = p1 + ret;

		for (;;) {
			if (!incomplete_frame) {
				p1 = get_image_indicator(p1, end - p1,
					START_INDICATOR);
				if (!p1) {
					break;
				}
				start = p1;
				size = end - start;

				incomplete_frame = 1;
				frame_cnt++;

				if ((p1 + 2) < end)
					p1 += 2;
				else
					break;
			}

			p1 = get_image_indicator(p1, end - p1,
				END_INDICATOR);
			if (!p1) {
				break;
			}
			incomplete_frame = 0;
			p1 += 2;
			size -= end - p1;
			if (frame_cnt == FRAME_NO)
				break;
		}

		if (frame_cnt != FRAME_NO)
			continue;
		len += ret;
		if (!hdrs_sent) {
			/* SOI indicator seen: send headers */
			/* Purge HTTP headers from incoming GET
			 * request */
			err = httpd_purge_headers(req->sock);
			if (err != WM_SUCCESS)
				goto out;

			err = httpd_send_hdr_from_code(req->sock,
				0, HTTP_CONTENT_JPEG);
			if (err != WM_SUCCESS)
				goto out;

			err = httpd_send_default_headers(req->sock,
				req->wsgi->hdr_fields);
			if (err != WM_SUCCESS)
				goto out;

			/* Done with setting http headers */
			httpd_send(req->sock, "\r\n", 2);
			hdrs_sent = 1;
		}
		err = httpd_send_chunk(req->sock, (char *)start, size);
		if (err < 0)
			goto out;
		if (!incomplete_frame) {
			/* Send last chunk "\r\n" so that client
			 * understands end of data */
			err = httpd_send_chunk(req->sock, NULL, 0);
			err = WM_SUCCESS;
			goto out;
		}
	}
out:
	usb_host_drv_stop(p);
	return err;
}
static int dump_img_data(httpd_request_t *req)
{
	int err, ret;
	uint32_t cnt, len;

	/* Purge HTTP headers from incoming GET request */
	err = httpd_purge_headers(req->sock);
	if (err != WM_SUCCESS)
		goto out;

	err = httpd_send_hdr_from_code(req->sock, 0, HTTP_CONTENT_JPEG);
	if (err != WM_SUCCESS)
		goto out;

	err = httpd_send_default_headers(req->sock, req->wsgi->hdr_fields);
	if (err != WM_SUCCESS)
		goto out;

	/* Done with setting http headers */
	httpd_send(req->sock, "\r\n", 2);

	cnt = 1024, len = 0;

	ret = usb_host_drv_start(p);
	if (ret != WM_SUCCESS) {
		wmprintf("Failed to start host driver: %d\r\n", ret);
		err = httpd_send_chunk(req->sock, NULL, 0);
		err = WM_SUCCESS;
		return err;
	}

	/* Read out data from usb uvc device and send as chunked encoded over
	 * http as infinite stream
	 */
	while (1) {
		ret = usb_host_drv_read(p, buf, cnt);
		if (ret <= 0) {
			/* Backoff ?*/
			os_thread_sleep(2);
			continue;
		}
		err = httpd_send_chunk(req->sock, (char *)buf, ret);
		if (err < 0)
			goto out;
		len += ret;
	}

	httpd_send(req->sock, "\r\n", 2);
	/* Send last chunk "\r\n" so that client understands end of data */
	err = httpd_send_chunk(req->sock, NULL, 0);
	err = WM_SUCCESS;
out:
	usb_host_drv_stop(p);
	return err;
}

static struct httpd_wsgi_call usb_http_handlers[] = {
{"/video", HTTPD_DEFAULT_HDR_FLAGS, 0, dump_img_data,
	NULL, NULL, NULL},
{"/capture", HTTPD_DEFAULT_HDR_FLAGS, 0, dump_single_image,
	NULL, NULL, NULL},
};

int g_usb_handlers_no = sizeof(usb_http_handlers) /
	sizeof(struct httpd_wsgi_call);

/*
 * Register Web-Service handler for video streaming
 */
int register_httpd_handlers()
{
	return httpd_register_wsgi_handlers(usb_http_handlers,
			g_usb_handlers_no);
}

/*
 * Handler invoked when the Micro-AP Network interface
 * is ready.
 */
void event_uap_started(void *data)
{
	int ret;

	/* Start http server */
	ret = app_httpd_start();
	if (ret != WM_SUCCESS)
		dbg("Failed to start HTTPD");

	ret = register_httpd_handlers();
	if (ret != WM_SUCCESS)
		dbg("Failed to register HTTPD handlers");
}

void event_uap_stopped(void *data)
{
	dbg("uap interface stopped");
}

/*
 * Handler invoked when WLAN subsystem is ready.
 *
 * The app-framework tells the handler whether there is
 * valid network information stored in persistent memory.
 *
 * The handler can then chose to connect to the network.
 *
 * We ignore the data and just start a Micro-AP network
 * with DHCP service. This will allow a client device
 * to connect to us and receive a valid IP address via
 * DHCP.
 *
 * In addition we also initialize usb host driver for video
 * class in this handler.
 */
void event_wlan_init_done(void *data)
{
	int ret;
	struct usb_uvc_drv_data drv;

	app_uap_start_with_dhcp(appln_cfg.ssid, appln_cfg.passphrase);

	ret = wlan_cli_init();
	if (ret != WM_SUCCESS)
		dbg("Error: wlan_cli_init failed");

#ifndef CONFIG_CPU_MC200
	/* Allocate memory for video buffer.
	 * For MC200 it is statically allocated. */
	video_buffer = os_mem_alloc(VID_BUF_LEN);
	if (video_buffer == NULL) {
		wmprintf("Failed to allocate video buffer\r\n");
		return;
	}
#endif
	/* Initialize usb host driver */
	drv.buf = video_buffer;
	drv.len = VID_BUF_LEN;
	drv.format = STREAM_MJPEG;
	drv.frame_interval = FPS_30;
	drv.xres = 320;
	drv.yres = 240;
	ret = usb_host_uvc_drv_init(&drv);
	if (ret != WM_SUCCESS) {
		wmprintf("Failed to init usb host driver\r\n");
		return;
	}

	p = usb_host_drv_open("MDEV_USB_HOST");
	if (p == NULL) {
		wmprintf("Failed to get mdev handle\r\n");
		return;
	}
}

/* This is the main event handler for this project. The application framework
 * calls this function in response to the various events in the system.
 */
int common_event_handler(int event, void *data)
{
	switch (event) {
	case AF_EVT_WLAN_INIT_DONE:
		event_wlan_init_done(data);
		break;
	case AF_EVT_UAP_STARTED:
		event_uap_started(data);
		break;
	case AF_EVT_UAP_STOPPED:
		event_uap_stopped(data);
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
		while (1)
			;
		/* do nothing -- stall */
	}

	ret = cli_init();
	if (ret != WM_SUCCESS) {
		dbg("Error: cli_init failed");
		while (1)
			;
		/* do nothing -- stall */
	}

	return;
}

/*
 * The application entry point is main(). At this point, minimal
 * initialization of the hardware is done, and also the RTOS is
 * initialized.
 */
int main()
{
	modules_init();

	/* Set WiFi driver level TX retry count limit, needed to improve
	 * streaming experience over TCP */
	wifi_set_packet_retry_count(50);

	dbg("Build Time: " __DATE__ " " __TIME__ "");
	wmprintf("[usb-video-app] USB Video Class demo application "
				"started\r\n");

	/* Start the application framework */
	if (app_framework_start(common_event_handler) != WM_SUCCESS) {
		dbg("Failed to start application framework");
		while (1)
			;
		/* do nothing -- stall */
	}
	return 0;
}
