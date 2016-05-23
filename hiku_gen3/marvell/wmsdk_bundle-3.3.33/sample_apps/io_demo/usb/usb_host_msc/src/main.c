/*
 *  Copyright (C) 2008-2015 Marvell International Ltd.
 *  All Rights Reserved.
 *
 *  Simple Wireless usb mass storage streaming application using Micro-AP and
 *  USB Host Stack.
 *
 *  Summary:
 *
 *  A USB mass storage disk/stick is connected to the device over the USB-OTG
 *  interface. The application starts a Micro-AP network and a Web Server.
 *  The data stream read from the USB disk is streamed over this Web Server.
 *
 *  The application reads raw USB data from the usb disk (no file system).
 *  It prints USB statistics after http connection is broken.
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
 *  connection.
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
 *  http://192.168.10.1/data. This handler reads the data stream from the USB
 *  disk and sends this stream to the connected clients (e.g. browser).
 *
 *  /data HTTP Handler:
 *
 *  When invoked, this handler performs the initial enumeration and setup phase
 *  for the USB mass storage class device. Once data, the handler reads the
 *  data stream from the USB disk and sends this data to the connected web
 *  client. The data streaming continuous until the connection is closed by
 *  the client.
 *
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

/* Mdev pointer to be used for the USB driver */
static mdev_t *p;

/* Buffer passed to USB MSC Driver to read data from usb disk.
 * Throughput dependes upon how much data is read at once, more the
 * buffer size, more is the throughput.
 */
#ifdef CONFIG_XIP_ENABLE
#define DATA_BUF_LEN (96 * 1024)
#else
#define DATA_BUF_LEN (32 * 1024)
#endif

/* For MC200, heap must be in SRAM1 which is small compared to SRAM0.
 * Hence, data_buffer cannot be dynamically allocated from heap.
 * Instead, it will be statically allocated from .bss which is in SRAM0.
 */
#ifdef CONFIG_CPU_MC200
static uint8_t data_buffer[DATA_BUF_LEN];
#else
static uint8_t *data_buffer;
#endif

/* Flag to notify the usb disk detection/enumeration event
 */
static uint8_t enable_streaming;

/*-----------------------Global declarations----------------------*/
appln_config_t appln_cfg = {
	.ssid = "Marvell Demo",
	.passphrase = "marvellwm",
	.hostname = "mdemo"
};

static int dump_img_data(httpd_request_t *req)
{
	int err, ret;
	uint32_t len, sector, sectors;
	uint32_t last_time, total_time;
	struct usb_msc_drv_info disk_info;

	len = 0, sector = 0, sectors = DATA_BUF_LEN / 512;
	total_time = 0;

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

	/* Wait till USB disk is enumerated */
	while (enable_streaming == false) {
		os_thread_sleep(10);
		continue;
	}

	ret = usb_host_drv_info(p, &disk_info);
	if (ret != WM_SUCCESS)
		goto out;

	sectors = DATA_BUF_LEN / disk_info.sector_size;

	/* Read out data from usb msc device and send as chunked encoded over
	 * http as infinite stream
	 */
	while (1) {
		if (enable_streaming == false) {
			os_thread_sleep(10);
			continue;
		}

		last_time = os_get_timestamp();
		ret = usb_host_drv_block_read(p, data_buffer, sector, sectors);
		if (ret != sectors * 512) {
			wmprintf("Disk Read failed ret = %d sector = %u\r\n",
				ret, sector);
			continue;
		}
		total_time += os_get_timestamp() - last_time;
		len += ret;
		sector += sectors;
		err = httpd_send_chunk(req->sock, (char *)data_buffer, ret);
		if (err < 0)
			goto out;
	}

	httpd_send(req->sock, "\r\n", 2);
	/* Send last chunk "\r\n" so that client understands end of data */
	err = httpd_send_chunk(req->sock, NULL, 0);
	err = WM_SUCCESS;
out:
	wmprintf("Total Data = %u KB Read Time = %u us\r\n",
		(len / 1024), total_time);
	wmprintf("Total Sectors = %u  USB Throughput = %u.%u MB/s\r\n",
		sector, len / total_time, len % total_time);
	return err;
}

static struct httpd_wsgi_call dump_img = {"/data",
	HTTPD_DEFAULT_HDR_FLAGS, 0, dump_img_data,
	NULL, NULL, NULL};

/*
 * Register Web-Service handler for data streaming
 */
int register_httpd_handlers()
{
	return httpd_register_wsgi_handlers(&dump_img, 1);
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

/* Event handler invoked when usb event is detected
 */

int disk_event_handler(usb_host_event_t event, void *data)
{
	switch (event) {
	case USB_DEVICE_ENUMERATED:
		enable_streaming = true;
		break;
	case USB_DEVICE_DISCONNECTED:
		enable_streaming = false;
		break;
	default:
		break;
	}

	return 0;
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
 * In addition we also initialize usb host driver for mass storage
 * class in this handler.
 */
void event_wlan_init_done(void *data)
{
	int ret;
	struct usb_msc_drv_data drv;

#ifndef CONFIG_CPU_MC200
	/* Allocate memory for data buffer.
	 * For MC200 it is statically allocated. */
	data_buffer = os_mem_alloc(DATA_BUF_LEN);
	if (data_buffer == NULL) {
		wmprintf("Failed to allocate data buffer\r\n");
		return;
	}
#endif
	/* Initialize usb host driver */
	memset(&drv, 0, sizeof(drv));
	drv.event_handler = disk_event_handler;
	ret = usb_host_msc_drv_init(&drv);
	if (ret != WM_SUCCESS) {
		wmprintf("Failed to init usb host driver\r\n");
		return;
	}
	p = usb_host_drv_open("MDEV_USB_HOST");
	if (p == NULL) {
		wmprintf("Failed to get mdev handle\r\n");
		return;
	}
	ret = usb_host_drv_start(p);
	if (ret != WM_SUCCESS) {
		wmprintf("Failed to start host driver: %d\r\n", ret);
		return;
	}
	app_uap_start_with_dhcp(appln_cfg.ssid, appln_cfg.passphrase);

	ret = wlan_cli_init();
	if (ret != WM_SUCCESS)
		dbg("Error: wlan_cli_init failed");
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
	wmprintf("[usb-disk-app] USB Mass Storage Class demo application "
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
