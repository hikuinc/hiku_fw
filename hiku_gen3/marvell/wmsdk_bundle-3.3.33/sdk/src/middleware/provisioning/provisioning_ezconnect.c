/*
 *  Copyright 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

/** provisioning_ezconnect.c: The EZConnect provisioning state machine
 */

/* Protocol description:
 * In this provisioning method, the WLAN radio starts sniffing each channel
 * for CHANNEL_SWITCH_INTERVAL. It looks for multicast MAC frames destined to
 * different IP addresses. It attempts to extract information from the least
 * significant 23 bits of multicast destination MAC address. Each 23 bits are
 * organized in following format
 *
 *	|       7 bits         |           16 bits      |
 *	|      <metadata>      |         <data>         |
 *	| Frame Type |  Seq no |   data1    |  data0    |
 *
 * Preamble -
 * Frame Type	= 0b11110
 * Seq no	= 0 - 2
 * data		= preamble bytes (0, 1), (2, 3), (4, 5) in (data0, data1)
 *
 * SSID -
 * Frame Type   = 0b10
 * Seq no       = 0
 * data		= (len_ssid, len_ssid) in (data0, data1)
 * Seq no       = 1 - 2
 * data		= CRC of the SSID byte by byte
 * Seq no       = 3 onwards
 * data		= SSID byte by byte (LSB in data0)
 *
 * Passphrasse - Encrypted or plaintext passphrase
 * Frame Type	= 0b0
 * Seq no       = 0
 * data		= (len_pass, len_pass) in (data0, data1)
 * Seq no       = 1 - 2
 * data		= CRC of the Passphrase byte by byte
 * Seq no	= 3 onwards
 * data		= Encrypted or plaintext passphrase byte by byte (LSB in data0)
 *
 */

#include <stdio.h>
#include <string.h>

#include <wmtypes.h>
#include <wmstdio.h>
#include <wm_os.h>
#include <wm_net.h>
#include <provisioning.h>
#include <app_framework.h>
#include <mdev_aes.h>
#include <wlan_smc.h>
#include <sha.h>
#include <pwdbased.h>
#include <internal.h>
#include "provisioning_int.h"

#define SSID_MAX_LENGTH                 33

extern struct prov_gdata prov_g;

struct ezconn_state {
	uint8_t state;
	uint8_t substate;
	uint8_t pass_len;
	uint8_t ssid_len;
	char cipher_pass[WLAN_PSK_MAX_LENGTH];
	char plain_pass[WLAN_PSK_MAX_LENGTH];
	uint32_t crc_ssid;
	uint32_t crc_passphrase;
	uint8_t BSSID[6];
	uint8_t source[6];
	char ssid[SSID_MAX_LENGTH];
	int8_t security;
};

static struct ezconn_state *es, *es1;

typedef struct {
	uint8_t bssid[MLAN_MAC_ADDR_LENGTH];
	uint8_t channel;
	uint8_t dtim_count;
	uint8_t dtim_period;
} network_info_t;

static network_info_t *beacon_info;
/* Maximum number of beacon information cached */
#define MAX_BEACON_COUNT 40
/* In beacon frame, we get 2 channels. First one indicates the channel on which
 * beacon is sniffed and second one is the actual channel on which beacon frame
 * was sent. TLV with type 3 gives actual channel */
#define BEACON_CHANNEL_ELE_ID 3
/* In beacon frame, TLV with type 5 gives DTIM information */
#define BEACON_DTIM_ELE_ID 5

/* Number of beacon information cached */
static int count_beacon_info;

/* Channel information of home network */
static uint8_t network_channel;

/* After successful provisioning, below pointer contains correct state
   structure */
struct ezconn_state *g_esp;

/* In SMC mode based EZConnect provisioning support, start uAP on receiving
 * AUTH request or directed probe request or start EZConnect state machine on
 * receiving multicast packets */
enum packet_type {
	NULL_TYPE = 0,
	EZ_MULTICAST,
	SMC_AUTH,
	SMC_PROBE_REQ,
};

static enum packet_type received_packet_type;

/* preamble as EZPR22 */
static char preamble[] = {0x45, 0x5a, 0x50, 0x52, 0x32, 0x32};

struct wlan_network uap_network;

uint8_t ezconn_device_key[PROV_DEVICE_KEY_MAX_LENGTH];
static uint8_t ezconn_device_key_len;
uint8_t ezconn_derived_secret[AES_256_KEY_SIZE];

static uint8_t iv[16];

static os_semaphore_t smc_cs_sem;
static os_timer_t restricted_chan_sniff_timer;
#define DEFAULT_SCAN_PROTECTION_TIMEOUT (30 * 1000)
#define RESTRICTED_CHANNEL_SNIFF_TIMEOUT (5 * 1000)

static network_info_t *lookup_mac_beacon_info(const char *bssid)
{
	int i;
	for (i = 0; i < count_beacon_info; i++) {
		if (!memcmp(beacon_info[i].bssid, bssid,
			    MLAN_MAC_ADDR_LENGTH)) {
			return &beacon_info[i];
		}
	}
	return NULL;
}

static int ezconn_parse_beacon_frame(const wlan_frame_t *frame,
				     const uint16_t len)
{
	if (count_beacon_info == MAX_BEACON_COUNT)
		return -WM_FAIL;

	network_info_t *beacon = lookup_mac_beacon_info(
		frame->frame_data.data_info.bssid);
	if (beacon != NULL)
		return WM_SUCCESS;

	const char *frame_traversal = frame->frame_data.beacon_info.ssid +
		frame->frame_data.beacon_info.ssid_len;

	memcpy(beacon_info[count_beacon_info].bssid,
	       frame->frame_data.data_info.bssid, MLAN_MAC_ADDR_LENGTH);

	while ((frame_traversal - (char *)frame) < len) {
		switch (*frame_traversal) {
		case BEACON_CHANNEL_ELE_ID:
			beacon_info[count_beacon_info].channel =
				*(frame_traversal + 2);
			break;
		case BEACON_DTIM_ELE_ID:
			beacon_info[count_beacon_info].dtim_count =
				*(frame_traversal + 2);
			beacon_info[count_beacon_info].dtim_period =
				*(frame_traversal + 3);
			break;
		}
		frame_traversal += *(frame_traversal + 1) + 2;
	}
	if (beacon_info[count_beacon_info].channel != 0)
		count_beacon_info++;
	return WM_SUCCESS;
}

#define DECRYPT(key, keylen, iv, cipher, plain, size)		\
	{							\
		aes_t enc_aes;					\
		mdev_t *aes_dev;				\
		aes_dev = aes_drv_open("MDEV_AES_0");		\
		aes_drv_setkey(aes_dev, &enc_aes, key, keylen,	\
			       iv, AES_DECRYPTION,		\
			       HW_AES_MODE_CBC);		\
		aes_drv_encrypt(aes_dev, &enc_aes,		\
				(uint8_t *) cipher,		\
				(uint8_t *) plain, size);	\
		aes_drv_close(aes_dev);				\
	}


int prov_ezconn_set_device_key(uint8_t *key, int len)
{
	if (len < PROV_DEVICE_KEY_MIN_LENGTH ||
	    len > PROV_DEVICE_KEY_MAX_LENGTH) {
		prov_e("Invalid provisioning key length");
		return -WM_FAIL;
	}

	memcpy(ezconn_device_key, key, len);
	ezconn_device_key_len = len;
	return WM_SUCCESS;
}

void prov_ezconn_unset_device_key(void)
{
	memset(ezconn_device_key, 0, sizeof(ezconn_device_key));
	ezconn_device_key_len = 0;
	return;
}

extern const uint32_t crc_table[256];
/* soft_crc32 uses a different polynomial hence defining ezconn_crc32 api */
static uint32_t ezconn_crc32(uint32_t crc, const void *buf, size_t size)
{
	const uint8_t *p;

	p = buf;
	crc = crc ^ ~0U;

	while (size--)
		crc = crc_table[(crc ^ *p++) & 0xFF] ^ (crc >> 8);

	return crc ^ ~0U;
}


static inline int ezconn_match_crc(struct ezconn_state *esp)
{
	if (esp->crc_passphrase != ezconn_crc32(0, esp->plain_pass,
						esp->pass_len))
		return -WM_FAIL;
	if (esp->crc_ssid != ezconn_crc32(0, esp->ssid, esp->ssid_len))
		return -WM_FAIL;
	return WM_SUCCESS;
}

/* Masking for frame type bitwise x11111xx */
#define SM_S0_FRAME_TYPE_MASKING 0x7c
/* Preamble frame type bitwise x11110xx */
#define SM_S0_FRAME_TYPE_PREAMBLE 0x78
/* Masking for substate bitwise xxxxxx11 */
#define SM_S0_SUBSTATE_MASKING 0x03

static int ezconnect_sm_s0(const char *src, const char *dest,
			   const char *bssid, const uint16_t len,
			   struct ezconn_state *esp)
{
	if (((dest[3] & SM_S0_FRAME_TYPE_MASKING) == SM_S0_FRAME_TYPE_PREAMBLE)
	    && ((dest[3] & SM_S0_SUBSTATE_MASKING) == esp->substate)
	    && (dest[4] == preamble[esp->substate * 2 + 1])
	    && (dest[5] == preamble[esp->substate * 2])) {
		if (esp->substate == 0) {
			memcpy(&esp->BSSID, bssid, 6);
			memcpy(&esp->source, src, 6);
		} else if (esp->substate == 2) {
			esp->state = 1;
			esp->substate = 0;
			return WM_SUCCESS;
		}
		esp->substate++;
		return WM_SUCCESS;
	}
	return -WM_FAIL;
}

/* Substate for ssid length */
#define SM_SUBSTATE_SSID_LENGTH 0x00
/* Substate for crc */
#define SM_SUBSTATE_CRC(subs) ((subs == 1 || subs == 2) ? true : false)

/* Masking for frame type bitwise x11xxxxx */
#define SM_S1_FRAME_TYPE_MASKING 0x60
/* Preamble frame type bitwise x10xxxxx */
#define SM_S1_FRAME_TYPE_PREAMBLE 0x40
/* Masking for substate bitwise xxx11111 */
#define SM_S1_SUBSTATE_MASKING 0x1f

static int ezconnect_sm_s1(const char *src, const char *dest,
			   const char *bssid, const uint16_t len,
			   struct ezconn_state *esp)
{
	if (((dest[3] & SM_S1_FRAME_TYPE_MASKING) == SM_S1_FRAME_TYPE_PREAMBLE)
	    && ((dest[3] & SM_S1_SUBSTATE_MASKING) == esp->substate)) {
		if (esp->substate == SM_SUBSTATE_SSID_LENGTH &&
		    (dest[4] == dest[5])) {
			esp->ssid_len = dest[4];
		} else if (((dest[3] & SM_S1_SUBSTATE_MASKING) == esp->substate)
			   && (SM_SUBSTATE_CRC(esp->substate))) {
			esp->crc_ssid |= (int)
				(dest[5] << (((esp->substate - 1) * 2) * 8));
			esp->crc_ssid |= (int)
			     (dest[4] << ((((esp->substate - 1) * 2) + 1) * 8));
		} else {
			esp->ssid[(esp->substate - 3) * 2] = dest[5];
			esp->ssid[((esp->substate - 3) * 2) + 1] = dest[4];
		}

		if ((esp->ssid_len == 0) ||
		    ((esp->substate * 2) >= (esp->ssid_len + 4))) {
			esp->substate = 0;
			esp->state = 2;
		} else {
			esp->substate++;
		}
		return WM_SUCCESS;
	}
	return -WM_FAIL;
}

/* Masking for frame type bitwise x1xxxxxx */
#define SM_S2_FRAME_TYPE_MASKING 0x40
/* Preamble frame type bitwise x0xxxxxx */
#define SM_S2_FRAME_TYPE_PREAMBLE 0x00
/* Masking for substate bitwise xx111111 */
#define SM_S2_SUBSTATE_MASKING 0x3f

static int ezconnect_sm_s2(const char *src, const char *dest,
			   const char *bssid, const uint16_t len,
			   struct ezconn_state *esp)
{
	if (((dest[3] & SM_S2_FRAME_TYPE_MASKING) == SM_S2_FRAME_TYPE_PREAMBLE)
	    && ((dest[3] & 0x3f) == esp->substate)) {
		if (esp->substate == 0 && (dest[4] == dest[5])) {
			esp->pass_len = dest[4];
		} else if (((dest[3] & SM_S2_SUBSTATE_MASKING) == esp->substate)
			   && (SM_SUBSTATE_CRC(esp->substate))) {
			esp->crc_passphrase |= (int)
				(dest[5] << (((esp->substate - 1) * 2) * 8));
			esp->crc_passphrase |= (int)
			     (dest[4] << ((((esp->substate - 1) * 2) + 1) * 8));
		} else {
			esp->cipher_pass[(esp->substate - 3) * 2] = dest[5];
			esp->cipher_pass[((esp->substate - 3) * 2) + 1] =
				dest[4];
		}
		if ((esp->pass_len == 0) ||
		    ((esp->substate * 2) >= (esp->pass_len + 4))) {
			esp->substate = 0;
			esp->state = 3;
		} else {
			esp->substate++;
		}
		if (esp->state == 3 && esp->substate == 0) {
			memcpy(&esp->BSSID, bssid, 6);
		}
		return WM_SUCCESS;
	}

	return -WM_FAIL;
}

static void ezconnect_sm(const wlan_frame_t *frame, const uint16_t len)
{
	struct ezconn_state *esp;
	const char *src, *dest, *bssid;
	uint8_t client[MLAN_MAC_ADDR_LENGTH];
	network_info_t *data_frame;
	wlan_get_mac_address(client);

	/* check if the frame is beacon frame */
	if (frame->frame_type == BEACON) {
		ezconn_parse_beacon_frame(frame, len);
		return;
	}

	if (frame->frame_type == PROBE_REQ &&
	    frame->frame_data.probe_req_info.ssid_len) {

		/* To stop smc mode and start uAP on receiving directed probe
		 * request for uAP */
		if (strncmp(frame->frame_data.probe_req_info.ssid,
			    uap_network.ssid,
			    frame->frame_data.probe_req_info.ssid_len))
			return;
		received_packet_type = SMC_PROBE_REQ;
		os_semaphore_put(&smc_cs_sem);

	} else if (frame->frame_type == AUTH &&
		   (memcmp(frame->frame_data.auth_info.dest, client, 6) == 0)) {
		received_packet_type = SMC_AUTH;
		os_semaphore_put(&smc_cs_sem);
	}

	if (frame->frame_data.data_info.frame_ctrl_flags & 0x01) {
		esp = es1;
		dest = frame->frame_data.data_info.src;
		bssid = frame->frame_data.data_info.dest;
		src = frame->frame_data.data_info.bssid;
	} else {
		esp = es;
		src = frame->frame_data.data_info.src;
		bssid = frame->frame_data.data_info.bssid;
		dest = frame->frame_data.data_info.dest;
	}

	/* We are not interested in non-multicast packets */
	if (!(dest[0] == 0x01 && dest[1] == 0x00 && dest[2] == 0x5e)) {
		return;
	}

	/* Once we get the first packet of interest, we'll ignore other
	   packets from other sources */
	if (esp->state + esp->substate > 0) {
		if (memcmp(src, esp->source, MLAN_MAC_ADDR_LENGTH) != 0)
			return;
	}

	switch (esp->state) {
	case 0:
		ezconnect_sm_s0(src, dest, bssid, len, esp);
		break;
	case 1:
		if (!network_channel) {
			data_frame = lookup_mac_beacon_info(bssid);
			if (data_frame != NULL) {
				network_channel = data_frame->channel;
				received_packet_type = EZ_MULTICAST;
				os_semaphore_put(&smc_cs_sem);
			}
		}
		ezconnect_sm_s1(src, dest, bssid, len, esp);
		break;
	case 2:
		ezconnect_sm_s2(src, dest, bssid, len, esp);
		if (esp->state == 3) {
			if (ezconn_device_key_len) {
				if (esp->pass_len % 16 != 0) {
					prov_e("Invalid passphrase length");
					esp->state = 0;
					esp->substate = 0;
					return;
				}

				/*
				 * The 32 byte key is generated from the
				 * device key and salt (ssid) using password
				 * based key derivation function. The derived
				 * key will be used in decrypting cipher text
				 * using AES256 algorithm.
				 *
				 * [param] description
				 * out buffer
				 * in passphrase
				 * in passphrase length
				 * in salt
				 * in salt length
				 * in number of iterations
				 * in Expected length of the derived key
				 *    (For AES256, 32byte key is required)
				 * in Hash function SHA -> SHA1
				 */
				PBKDF2(ezconn_derived_secret, ezconn_device_key,
				       ezconn_device_key_len,
				       (uint8_t *)esp->ssid, esp->ssid_len,
				       4096, AES_256_KEY_SIZE, SHA);

				DECRYPT(ezconn_derived_secret,
					AES_256_KEY_SIZE, iv,
					esp->cipher_pass, esp->plain_pass,
					esp->pass_len);
				if (strlen(esp->plain_pass) < esp->pass_len) {
					esp->pass_len = strlen(esp->plain_pass);
				}
			} else {
				memcpy(esp->plain_pass, esp->cipher_pass,
				       esp->pass_len);
			}
			if (ezconn_match_crc(esp) == WM_SUCCESS) {
				received_packet_type = EZ_MULTICAST;
				os_semaphore_put(&smc_cs_sem);
			} else {
				prov_e("Checksum mismatch");
				esp->state = 0;
				esp->substate = 0;
				return;
			}
		}
		break;
	default:
		break;
	}
}

static int ezconnect_scan_cb(unsigned int count)
{
	int i;
	struct wlan_scan_result res;

	for (i = 0; i < count; i++) {
		wlan_get_scan_result(i, &res);
		if (memcmp(res.bssid, g_esp->BSSID, 6) == 0) {
			g_esp->security = WLAN_SECURITY_NONE;
			if (res.wpa && res.wpa2) {
				g_esp->security = WLAN_SECURITY_WPA_WPA2_MIXED;
			} else if (res.wpa2) {
				g_esp->security = WLAN_SECURITY_WPA2;
			} else if (res.wpa) {
				g_esp->security = WLAN_SECURITY_WPA_WPA2_MIXED;
			} else if (res.wep) {
				g_esp->security = WLAN_SECURITY_WEP_OPEN;
			}
			break;
		}
	}
	os_semaphore_put(&smc_cs_sem);
	return 0;
}

void register_ezconnect_provisioning_ssid(char *ssid)
{
	wlan_initialize_uap_network(&uap_network);
	memcpy(uap_network.ssid, ssid, sizeof(uap_network.ssid));
}

void restricted_chan_sniff_timer_cb()
{
	wlan_stop_smart_mode();
	memset(es, 0, sizeof(struct ezconn_state));
	memset(es1, 0, sizeof(struct ezconn_state));
	os_semaphore_put(&smc_cs_sem);
}

static void ezconn_configure_smc_mode(smart_mode_cfg_t *smc, int channel)
{
	uint8_t start_addr[] = {0x01, 0x00, 0x5e, 0x00, 0x00, 0x00};
	uint8_t end_addr[] = {0x01, 0x00, 0x5e, 0xff, 0xff, 0xff};

	smc->beacon_period = 20;
	smc->country_code = wlan_get_country();
	smc->channel = channel;
	if (channel) {
		smc->min_scan_time = 120;
		smc->max_scan_time = 140;
	} else {
		smc->min_scan_time = 60;
		smc->max_scan_time = 70;
	}
	smc->filter_type = 0x03;
	smc->custom_ie_len = 0;

	memcpy(smc->smc_start_addr, start_addr, sizeof(start_addr));
	memcpy(smc->smc_end_addr, end_addr, sizeof(end_addr));
}

#define PROV_EZCONNECT_LOCK "prov_ezconnect_lock"
void prov_ezconnect(os_thread_arg_t thandle)
{
	int ret;

	struct wlan_network *net;
	uint8_t smc_frame_filter[] = {0x02, 0x10, 0x01, 0x2c, 0x04, 0x02, 0x02,
				      0x20};

	ret = os_semaphore_create(&smc_cs_sem, "smc_cs_sem");
	os_semaphore_get(&smc_cs_sem, OS_WAIT_FOREVER);

	if (os_timer_create(&restricted_chan_sniff_timer,
			    "restricted_chan_sniff_timer",
			    os_msec_to_ticks(RESTRICTED_CHANNEL_SNIFF_TIMEOUT),
			    &restricted_chan_sniff_timer_cb,
			    NULL,
			    OS_TIMER_ONE_SHOT ,
			    OS_TIMER_NO_ACTIVATE) != WM_SUCCESS) {
		prov_e("Failed to create restricted channel sniffing timer");
	}

	beacon_info = os_mem_alloc(MAX_BEACON_COUNT * sizeof(network_info_t));
	if (!beacon_info) {
		prov_e("Failed to allocate memory.\r\n");
		return;
	}

	ret = aes_drv_init();
	if (ret != WM_SUCCESS) {
		prov_e("Unable to initialize AES engine.\r\n");
		return;
	}

	received_packet_type = NULL_TYPE;

	wakelock_get(PROV_EZCONNECT_LOCK);

	smart_mode_cfg_t smart_mode_cfg;
	bzero(&smart_mode_cfg, sizeof(smart_mode_cfg_t));

	/* uAP channel is set to 6 */
	uap_network.channel = 6;
	/* Configuring channel as auto */
	ezconn_configure_smc_mode(&smart_mode_cfg, 0);
	smart_mode_cfg.smc_frame_filter = smc_frame_filter;
	smart_mode_cfg.smc_frame_filter_len = sizeof(smc_frame_filter);

	ret = wlan_set_smart_mode_cfg(&uap_network, &smart_mode_cfg,
				      ezconnect_sm);
	if (ret != WM_SUCCESS)
		prov_e("Error: wlan_set_smart_mode_cfg failed.");

	es = os_mem_calloc(sizeof(struct ezconn_state));
	es1 = os_mem_calloc(sizeof(struct ezconn_state));

	wlan_start_smart_mode();

	os_semaphore_get(&smc_cs_sem, OS_WAIT_FOREVER);
	while (received_packet_type) {
		if (received_packet_type == EZ_MULTICAST) {
			if (es->state == 1 || es1->state == 1) {
				wlan_stop_smart_mode();
				ezconn_configure_smc_mode(&smart_mode_cfg,
							  network_channel);
				ret = wlan_set_smart_mode_cfg(&uap_network,
							      &smart_mode_cfg,
							      ezconnect_sm);
				wlan_start_smart_mode();
				if (restricted_chan_sniff_timer) {
					os_timer_activate(
						&restricted_chan_sniff_timer);
				}
				/* Control will come here on timeout or
				 * sniffed right packets */
				os_semaphore_get(&smc_cs_sem, OS_WAIT_FOREVER);
				if (es->state || es1->state) {
					os_timer_deactivate(
						&restricted_chan_sniff_timer);
				} else {
					/* reset everything and start again */
					received_packet_type = NULL_TYPE;
					network_channel = 0;
					ezconn_configure_smc_mode(
					      &smart_mode_cfg, network_channel);
					ret = wlan_set_smart_mode_cfg(
						&uap_network, &smart_mode_cfg,
						ezconnect_sm);
					wlan_start_smart_mode();
					os_semaphore_get(&smc_cs_sem,
							 OS_WAIT_FOREVER);
					continue;
				}
			}
			if (es->state == 3) {
				prov_d("EZConnect provisioning done "
				       "(from-DS).\r\n");
				g_esp = es;
			} else {
				prov_d("EZConnect provisioning done "
				       "(to_DS).\r\n");
				g_esp = es1;
			}
			wlan_stop_smart_mode();
			g_esp->security = -1;

			while (g_esp->security == -1) {
				prov_d("WLAN Scanning...\r\n");
				ret = wlan_scan(ezconnect_scan_cb);
				if (ret != WM_SUCCESS) {
					prov_e("Scan request failed. Retrying");
					os_thread_sleep(os_msec_to_ticks(100));
					continue;
				}

				os_semaphore_get(&smc_cs_sem, OS_WAIT_FOREVER);
			}

			net = &prov_g.current_net;
			bzero(net, sizeof(*net));

			snprintf(net->name, 8, "default");
			strncpy(net->ssid, g_esp->ssid, 33);
			net->security.type = g_esp->security;
			net->security.psk_len = g_esp->pass_len;
			if (g_esp->security == WLAN_SECURITY_WEP_OPEN) {
				ret = load_wep_key(
					(const uint8_t *) g_esp->plain_pass,
					(uint8_t *) net->security.psk,
					(uint8_t *) &net->security.psk_len,
					WLAN_PSK_MAX_LENGTH);
			} else {
				strncpy(net->security.psk, g_esp->plain_pass,
					WLAN_PSK_MAX_LENGTH);
			}
			net->ip.ipv4.addr_type = 1; /* DHCP */
			prov_g.state = PROVISIONING_STATE_DONE;

			prov_d("SSID: %s\r\n", g_esp->ssid);
			prov_d("Passphrase %s\r\n", g_esp->plain_pass);
			prov_d("Security %d\r\n", g_esp->security);

			(prov_g.pc)->provisioning_event_handler(
				PROV_NETWORK_SET_CB, &prov_g.current_net,
				sizeof(struct wlan_network));
			received_packet_type = NULL_TYPE;
		} else if (received_packet_type == SMC_PROBE_REQ ||
			   received_packet_type == SMC_AUTH) {
			wlan_stop_smart_mode_start_uap(&uap_network);
			(prov_g.pc)->provisioning_event_handler(
				PROV_MICRO_AP_UP_REQUESTED, &uap_network,
				sizeof(struct wlan_network));
			received_packet_type = NULL_TYPE;
		}
	}
	os_mem_free(beacon_info);

	wakelock_put(PROV_EZCONNECT_LOCK);

	os_thread_self_complete((os_thread_t *)thandle);

	return;
}


void prov_ezconnect_stop()
{
	wlan_stop_smart_mode();
	if (es) {
		os_mem_free(es);
		es = NULL;
	}
	if (es1) {
		os_mem_free(es1);
		es1 = NULL;
	}
	if (smc_cs_sem) {
		os_semaphore_put(&smc_cs_sem);
		os_semaphore_delete(&smc_cs_sem);
		smc_cs_sem = NULL;
	}
	return;
}
