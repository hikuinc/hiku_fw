/*
 * Copyright 2008-2015, Marvell International Ltd.
 * All Rights Reserved.
 */

/** write_wifi_firmware.c: Write WiFi firmware images to flash
 */

#include <string.h>

#include <wmstdio.h>
#include <rfget.h>
#include <crc32.h>
#include <wm_os.h>
#include <flash.h>
#include <partition.h>
#include <xz.h>

#include "firmware.h"

#define WIFI_FIRMWARE_UPDATE_BUFF_SIZE 256
extern void purge_stream_bytes(data_fetch data_fetch_cb, void *priv,
		size_t length, unsigned char *scratch_buf,
		const int scratch_buflen);

int get_sizeof_wlan_fw_header(void)
{
	return sizeof(struct wlan_fw_header);
}

/* Write the Wlan FW Header */
int write_wlan_fw_header(size_t firmware_length, update_desc_t ud)
{
	struct wlan_fw_header wf_header;

	wf_header.magic = WLAN_FW_MAGIC;
	wf_header.length = firmware_length;
	return flash_drv_write(ud.dev, (uint8_t *)&wf_header,
		sizeof(wf_header), ud.addr);
}

/*
 * Description	: Update the wifi firmware
 * Input	: void
 * Output	: WM_SUCCESS  => ok
 *                -1 => Fail
 */
int update_and_validate_wifi_firmware(data_fetch data_fetch_cb, void *priv,
		    size_t firmware_length, struct partition_entry *f,
		    data_validate data_validate_cb)
{
	uint32_t filesize, read_length;
	int32_t size_read;
	int error;
	update_desc_t ud;
	unsigned char buf[WIFI_FIRMWARE_UPDATE_BUFF_SIZE];
	rf_d(" Updating wifi firmware at address 0x%0x"
		       , f->start);
	rf_d("@ address 0x%x len: %d", f->start, f->size);

	if (firmware_length == 0) {
		rf_e("WiFi Firmware binary zero length");
		return -WM_E_RFGET_INVLEN;
	}

	/* Verify firmware length first before proceeding further */
	if (firmware_length  + get_sizeof_wlan_fw_header() > f->size) {
		rf_e("WiFi Firmware binary too large %d > %d",
			      firmware_length, f->size);
		purge_stream_bytes(data_fetch_cb, priv, firmware_length,
				   buf, WIFI_FIRMWARE_UPDATE_BUFF_SIZE);
		return -WM_E_RFGET_INVLEN;
	}

	size_read = data_fetch_cb(priv, buf, WIFI_FIRMWARE_UPDATE_BUFF_SIZE);
	if (size_read <= 16) {
		rf_e("Failed to get firmware data for initial validation");
		error = -WM_FAIL;
		goto out;
	}

#if defined(CONFIG_XZ_DECOMPRESSION)
	rf_d("Validating wifi firmware..");
	uint8_t buffer[6];
	memcpy(&buffer, buf, sizeof(buffer));

	if (verify_xz_header(buffer) != WM_SUCCESS) {
		rf_e("Expected compressed WiFi firmware image.. "
			"Found incorrect or uncompressed image");
		return -WM_FAIL;
	}
#endif /* CONFIG_XZ_DECOMPRESSION */
	rfget_init();
	error = rfget_update_begin(&ud, f);
	if (error) {
		rf_e("Failed to erase wifi firmware area");
		purge_stream_bytes(data_fetch_cb, priv,
				   firmware_length,
				   buf, WIFI_FIRMWARE_UPDATE_BUFF_SIZE);
		goto out;
	}

	/* Write the Wlan FW Header */
	write_wlan_fw_header(firmware_length, ud);

	filesize = 0;

	ud.addr += get_sizeof_wlan_fw_header();

	read_length = WIFI_FIRMWARE_UPDATE_BUFF_SIZE;
	while (filesize < firmware_length) {
		/* First block of data has already been fetched above
		 * and size_read and buf have been set there */
		if (filesize != 0) {
			if ((firmware_length - filesize) < sizeof(buf))
				read_length = firmware_length - filesize;
			size_read = data_fetch_cb(priv, buf, read_length);
		}
		if (size_read <= 0)
			break;
		error = rfget_update_data(&ud, (char *)buf, size_read);
		if (error) {
			rf_e("Failed to write firmware data");
			goto out;
		}
		filesize += size_read;
	}
	if (filesize != firmware_length) {
		rf_e("Invalid size of receievd file size.");
		return -WM_FAIL;
	}
	if (data_validate_cb) {
		rf_d("Validating received data");
		if (data_validate_cb(priv) != WM_SUCCESS)
			return -WM_FAIL;
	}
	rfget_update_complete(&ud);

	return WM_SUCCESS;
out:
	return error;
}
int update_wifi_firmware(data_fetch data_fetch_cb, void *priv,
		    size_t firmware_length, struct partition_entry *f)
{
	return update_and_validate_wifi_firmware(data_fetch_cb, priv,
			firmware_length, f, NULL);
}
