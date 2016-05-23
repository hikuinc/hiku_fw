/*
 *  Copyright (C)2008 -2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

/**
 *  @brief This is OTP Memory Programming Application
 */
#include <wm_os.h>
#include <mdev_uart.h>
#include <mdev_sdio.h>
#include <lowlevel_drivers.h>
#include <cli.h>
#include <wlan.h>
#include <wmstdio.h>
#include <crc32.h>
#include "secure_config.h"
#include "wlan_fw.h"

/* Enable below for debug packet dumps */
/* #define DEBUG_LOG */
#ifdef DEBUG_LOG
#define dbg(...) wmprintf("[otp_app_dbg] %s\r\n", ##__VA_ARGS__);
#else
#define dbg(...)
#define dump_hex(...)
#endif

#define appl(_fmt_, ...) wmprintf("[otp_app] "_fmt_"", ##__VA_ARGS__);

#define BUF_SIZE 512
#define MAX_OTP_USER_DATA 252

#define MLAN_TYPE_CMD 0x1
#define INTF_HEADER_LEN 4

#define HostCmd_CMD_OTP_READ_USER_DATA 0x0114

extern int sd_wifi_init(enum wlan_type type,
			enum wlan_fw_storage_type st,
			flash_desc_t *fl, uint8_t *fw_ram_start_addr);
extern int wifi_raw_packet_send(const uint8_t *packet, uint32_t length);
extern int wifi_raw_packet_recv(uint8_t **data, uint32_t *pkt_type);

typedef enum {
	OTP_SIGN_BOOT_CONFIG = 0x0d,
	OTP_SIGN_AES_KEY = 0x0a,
	OTP_SIGN_RSA_HASH = 0x0e,
	OTP_USER_DATA_SIGN = 0x49,
} sign_type_t;

typedef struct fw_cmd_pkt {
	uint16_t pkt_size;
	uint16_t pkt_type;
	uint16_t cmd;
	uint16_t size;
	uint16_t seq_no;
	uint16_t result;
	uint16_t action;
	uint16_t usersign;
	uint16_t userdlen;
	uint8_t  userdata;
} __attribute__((packed)) fw_cmd_pkt_t;

static uint8_t user_data_buf[MAX_OTP_USER_DATA];
static uint8_t cmd_resp[BUF_SIZE];
static uint8_t cmd_buf[BUF_SIZE];
static uint16_t seq_no;

static int get_crc32(const void *data_i, uint32_t len, uint32_t *result)
{
	const uint8_t *data = (const uint8_t *) data_i;
	uint8_t *buf;
	uint32_t crc, i;

	buf = (uint8_t *) os_mem_alloc(len);
	if (buf == NULL) {
		appl("Allocation failure\r\n");
		return -1;
	}

	for (i = 0; i < len; i += 4) {
		buf[i] = data[i + 3];
		buf[i + 1] = data[i + 2];
		buf[i + 2] = data[i + 1];
		buf[i + 3] = data[i];
	}

	crc32_init();
	crc = crc32(buf, len, 0);

	uint8_t *s = (uint8_t *) &crc;
	uint8_t *d = (uint8_t *) result;
	d[0] = s[3];
	d[1] = s[2];
	d[2] = s[1];
	d[3] = s[0];

	os_mem_free(buf);
	return 0;
}

int static wifi_raw_cmd_process(uint16_t cmd, const uint8_t *cmd_buf,
					uint32_t len, uint8_t *cmd_resp)
{
	int ret;
	uint32_t pkt_type;
	fw_cmd_pkt_t *resp;

	dbg("===> Packet sent to WLAN FW");
	dump_hex(cmd_buf, len);
	ret = wifi_raw_packet_send(cmd_buf, len);
	if (ret == WM_SUCCESS) {
		ret = wifi_raw_packet_recv((uint8_t **)&resp, &pkt_type);
		dbg("<=== Response received from WLAN FW");
		dump_hex(resp, resp->pkt_size);
		if (ret != WM_SUCCESS) {
			appl("Receive response failed %d\r\n", ret);
			return ret;
		} else {
			if (pkt_type == MLAN_TYPE_CMD &&
					resp->cmd == (0x8000 | cmd)) {
				memcpy(cmd_resp, resp, resp->pkt_size);
				if (resp->result)
					appl("Command failed\r\n");
			} else {
				appl("Response for invalid pkt type"
						" %d\r\n", pkt_type);
			}
		}
	}
	return ret;
}

static int otp_set_sign_field(sign_type_t type, const uint8_t *data,
				uint16_t len)
{
	int ret;
	uint16_t orig_len = len;

	fw_cmd_pkt_t *pkt = (fw_cmd_pkt_t *) cmd_buf;
	fw_cmd_pkt_t *resp = (fw_cmd_pkt_t *) cmd_resp;
	memset(pkt, 0, BUF_SIZE);
	memset(resp, 0, BUF_SIZE);

	pkt->pkt_type = MLAN_TYPE_CMD;
	pkt->cmd = HostCmd_CMD_OTP_READ_USER_DATA;
	pkt->seq_no = seq_no++;
	pkt->action = 0x0;
	pkt->pkt_size = sizeof(*pkt) - 1;
	pkt->size = pkt->pkt_size - INTF_HEADER_LEN;
	pkt->usersign = 0x0;
	pkt->userdlen = 8;
	ret = wifi_raw_cmd_process(pkt->cmd, (const uint8_t *) pkt,
				pkt->pkt_size, (uint8_t *) resp);
	if (ret != WM_SUCCESS)
		return ret;
	else
		appl("OTP Free Lines = %d\r\n", resp->usersign);

	if ((resp->usersign * 8) < (len + 8)) {
		appl("Err...Insufficient OTP lines %d\r\n",
						resp->usersign);
		return -WM_FAIL;
	}

	memset(pkt, 0, BUF_SIZE);
	memset(resp, 0, BUF_SIZE);

	pkt->pkt_type = MLAN_TYPE_CMD;
	pkt->cmd = HostCmd_CMD_OTP_READ_USER_DATA;
	pkt->seq_no = seq_no++;
	pkt->action = 0x1;

	if (type == OTP_SIGN_AES_KEY || type == OTP_SIGN_RSA_HASH) {
		uint32_t crc;
		ret = get_crc32(data, len, &crc);
		if (ret)
			return ret;
		memcpy(&pkt->userdata, data, len);
		memcpy(&pkt->userdata + len, &crc, sizeof(crc));
		len += sizeof(crc);
	} else {
		memcpy(&pkt->userdata, data, len);
	}

	pkt->pkt_size = sizeof(*pkt) + len - 1;
	pkt->size = pkt->pkt_size - INTF_HEADER_LEN;
	pkt->userdlen = len;
	pkt->usersign = type;

	ret = wifi_raw_cmd_process(pkt->cmd, (const uint8_t *) pkt,
				pkt->pkt_size, (uint8_t *) resp);
	if (ret == WM_SUCCESS) {
		if (!memcmp(data, &resp->userdata, orig_len)) {
			appl("Programming OK!\r\n");
			dump_hex(data, orig_len);
		} else {
			appl("Programming Failure!\r\n");
		}
	}

	return ret;
}

static void do_otp_write(int argc, char **argv)
{
	int ret;

	appl("Start OTP Programming...\r\n");

	uint8_t boot_cfg = 0;
	if (JTAG_STATUS == 0) {
		appl("JTAG Disable = Yes\r\n");
		boot_cfg |= (1 << 0);
	}

	if (UART_USB_BOOT == 0) {
		appl("UART/USB Boot Disable = Yes\r\n");
		boot_cfg |= (1 << 1);
	}

	if (ENCRYPTED_BOOT == 1) {
		appl("Encrypted Boot = Yes\r\n");
		boot_cfg |= (1 << 2);

		if (strlen(AES_CCM256_KEY) != 64) {
			appl("Invalid AES-CCM key length\r\n");
			return;
		}
	}

	if (SIGNED_BOOT == 1) {
		appl("Signed Boot = Yes\r\n");
		boot_cfg |= (1 << 3);

		if (strlen(RSA_PUB_KEY_SHA256) != 64) {
			appl("Invalid RSA signature length\r\n");
			return;
		}
	}

	uint8_t sign_buf[32];
	if (ENCRYPTED_BOOT == 1) {
		hex2bin((const uint8_t *) AES_CCM256_KEY, sign_buf,
							sizeof(sign_buf));
		ret = otp_set_sign_field(OTP_SIGN_AES_KEY,
					sign_buf, sizeof(sign_buf));
		if (ret != WM_SUCCESS) {
			appl("Failed to program aes config %d\r\n", ret);
			return;
		}
	}

	if (SIGNED_BOOT == 1) {
		hex2bin((const uint8_t *) RSA_PUB_KEY_SHA256, sign_buf,
							sizeof(sign_buf));
		ret = otp_set_sign_field(OTP_SIGN_RSA_HASH,
					sign_buf, sizeof(sign_buf));
		if (ret != WM_SUCCESS) {
			appl("Failed to program rsa config %d\r\n", ret);
			return;
		}
	}

	if (USER_DATA == 1) {
		int ud_len = hex2bin((const uint8_t *) OTP_USER_DATA,
				user_data_buf, sizeof(user_data_buf));
		ret = otp_set_sign_field(OTP_USER_DATA_SIGN, user_data_buf,
								ud_len);
		if (ret != WM_SUCCESS) {
			appl("Failed to program user data %d\r\n", ret);
			return;
		}
	}

	ret = otp_set_sign_field(OTP_SIGN_BOOT_CONFIG, &boot_cfg,
			sizeof(boot_cfg));
	if (ret != WM_SUCCESS) {
		appl("Failed to program boot config %d\r\n", ret);
		return;
	}

	appl("OTP Programming Done!\r\n");
}

static struct cli_command cli[] = {
	{"otp_write", "(write OTP memory)", do_otp_write},
};

/**
 * All application specific initialization is performed here
 */
int main()
{
	wmstdio_init(UART0_ID, 0);
	cli_init();
	cli_register_commands(&cli[0],
			sizeof(cli) / sizeof(struct cli_command));

	int ret = sd_wifi_init(WLAN_TYPE_WIFI_CALIB,
				WLAN_FW_IN_RAM, NULL, wlan_fw_data);
	if (ret) {
		appl("Failed to init WLAN %d\r\n", ret);
		return ret;
	}
	appl("WLAN Firmware download successful\r\n");
	appl("##############################\r\n");
	appl("OTP Programming Application v" APP_VERSION " Started\r\n");

	/* Busy wait and let user run some commands */
	for (;;)
		os_thread_sleep(5000);

	return ret;
}
