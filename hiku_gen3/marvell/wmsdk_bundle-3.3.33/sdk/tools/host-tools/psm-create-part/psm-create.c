#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <psm-v2.h>
#include <errno.h>
#include <wmerrno.h>
#include <flash.h>
#include <strings.h>
#include <autoconf.h>

#ifdef CONFIG_SECURE_PSM
#include <openssl/aes.h>
#include "psm-v2-secure.h"
#endif

bool silent_opt;
#define TEST_DEBUG

#define psmt_e(__fmt__, ...)					\
	printf("[psmt] Error: "__fmt__"\r\n", ##__VA_ARGS__)
#define psmt_w(__fmt__, ...)					\
	printf("[psmt] Warn: "__fmt__"\r\n", ##__VA_ARGS__)

#ifdef TEST_DEBUG
#define psmt_d(__fmt__, ...)					\
	printf("[psmt]: "__fmt__"\r\n", ##__VA_ARGS__)
#else
#define psmt_d(...)
#endif

#define MAX_FILE_NAME_LEN	128
#define FLASH_FILE_SIZE 32768

FILE *fp;
char flash_file[MAX_FILE_NAME_LEN];
char g_buffer[FLASH_FILE_SIZE];
const char *encrypt_key, *encrypt_nonce;

flash_desc_t fdesc = {
	.fl_dev = 0,
	/** The start address on flash */
	.fl_start = 0,
	/** The size on flash  */
	/* .fl_size = <will be set later> */
};

static psm_hnd_t cached_phandle;

/* mingw does not have this function. */
size_t strnlen(const char *s, size_t maxlen)
{
	size_t len = 0;

	while (len < maxlen && s[len])
		len++;
	return len;
}

char *index(const char *s, int c)
{
	if (!s)
		return NULL;

	int i = 0;
	while (1) {
		if (!s[i])
			break;

		if (s[i] == c)
			return (char *)&s[i];

		i++;
	}

	return NULL;
}

#ifdef SEGGER
uint8_t bin_buf[256];
char hex_buf[512];
void bin2hex(uint8_t *src, char *dest, unsigned int src_len,
	     unsigned int dest_len)
{
	int i;
	for (i = 0; i < src_len; i++) {
		if (snprintf(dest, dest_len, "%02x", src[i]) >= dest_len) {
			printf("Destination full. "
				"Truncating to avoid overflow.\r\n");
			return;
		}
		dest_len -= 2;
		dest += 2;
	}
}

int generate_hex_file(char *filename)
{
	int rv;
	int file_len;
	uint16_t offset = 0x0;
	char prefix[10];
	FILE *dat_fp;

	fp = fopen(flash_file, "rb+");
	rv = fseek(fp, 0, SEEK_END);
	if (rv != 0) {
		psmt_e("fseek for read failed: %s", strerror(errno));
		return -1;
	}
	/* Length of bin file */
	file_len = ftell(fp);

	rv = fseek(fp, 0, SEEK_SET);
	if (rv != 0) {
		psmt_e("fseek for read failed: %s", strerror(errno));
		return -1;
	}

	rv = fread(bin_buf, file_len, 1, fp);

	/* Convert bin to hex */
	bin2hex(bin_buf, hex_buf, file_len, sizeof(hex_buf));

	dat_fp = fopen(filename, "wb+");

	/* First byte is offset */
	snprintf(prefix, sizeof(prefix), "%x,%x:", offset,
			(unsigned int)file_len);
	/* Write hex characters into new file */
	rv = fwrite(prefix, strlen(prefix), 1, dat_fp);
	rv = fwrite(hex_buf, 2*file_len, 1, dat_fp);

	fclose(dat_fp);
	fclose(fp);
	return WM_SUCCESS;
}
#endif

static int create_flash_file(uint32_t flash_size)
{
	if (!silent_opt)
		printf("Writing to file: %s\n", flash_file);
	FILE *fp = fopen(flash_file, "wb+");
	if (fp == NULL) {
		psmt_e("Unable to open flash file");
		return -1;
	}

	void *pattern = malloc(flash_size);
	if (!pattern) {
		psmt_e("Memory allocation failed");
		fclose(fp);
		unlink(flash_file);
		return -1;
	}

	memset(pattern, 0xFF, flash_size);
	int rv = fwrite(pattern, flash_size, 1, fp);
	if (rv != 1) {
		psmt_e("Failed to write pattern");
		fclose(fp);
		unlink(flash_file);
		free(pattern);
		return -1;
	}
	fclose(fp);
	free(pattern);
	return 0;
}

void flash_drv_init(void)
{
	/* Stub */
}

mdev_t *flash_drv_open(int fl_dev)
{
	/* Stub */
	return (void *)0xAA55; /* Dummy */
}

int flash_drv_close(mdev_t *dev)
{
	return WM_SUCCESS;
}

int flash_drv_read(mdev_t *fldev, uint8_t *buffer,
		   uint32_t size, uint32_t address)
{
	int rv = fseek(fp, address, SEEK_SET);
	if (rv != 0) {
		psmt_e("fseek(0x%x) for read failed: %s",
		       (unsigned)address, strerror(errno));
		return -1;
	}

	/* printf("%s: %d @ %x\n", __func__, size, address); */
	rv = fread(buffer, size, 1, fp);
	if (rv != 1) {
		psmt_e("%d: fread(%u) failed: %s", rv, size, strerror(errno));
		return -1;
	}

	/* int i; */
	/* for (i = 0; i < size; i++) */
	/*	printf("%x ", *((uint8_t *)buffer + i)); */
	/* printf("\n\r"); */

	return 0;
}

int flash_drv_write(mdev_t *fldev, uint8_t *buffer,
		     uint32_t size, uint32_t address)
{
	int rv = fseek(fp, address, SEEK_SET);
	if (rv != 0) {
		psmt_e("fseek(%u) write failed: %s", address,
		       strerror(errno));
		return -1;
	}

	/* printf("%s: %d @ %x\n", __func__, size, address); */
	/* int i; */
	/* for (i = 0; i < size; i++) */
	/*	printf("%x ", *((uint8_t *)buffer + i)); */
	/* printf("\n\r"); */
	rv = fwrite(buffer, size, 1, fp);
	if (rv != 1) {
		psmt_e("fwrite(%u) failed: %s", size, strerror(errno));
		return -1;
	}

	return 0;
}

int flash_drv_erase(mdev_t *fldev, unsigned long address, unsigned long size)
{
	psmt_d("%lu %lu", address, size);

	int rv = fseek(fp, address, SEEK_SET);
	if (rv != 0) {
		psmt_e("fseek(%lu) erase failed: %s", address,
		       strerror(errno));
		return -1;
	}

	char *ptr = malloc(size);
	if (!ptr) {
		psmt_e("Unable to allocate erase pattern");
		return -1;
	}

	memset(ptr, 0xFF, size);
	rv = fwrite(ptr, size, 1, fp);
	if (rv != 1) {
		psmt_e("fwrite(%lu) while erase failed: %s", size,
		       strerror(errno));
		free(ptr);
		return -1;
	}

	free(ptr);
	return 0;
}


#define DUMP_WRAPAROUND 16
void dump_hex(const void *data, unsigned len)
{
	printf("**** Dump @ %p Len: %d ****\n\r", data, len);

	int i;
	const char *data8 = (const char *)data;
	for (i = 0; i < len;) {
		printf("%02x ", data8[i++]);
		if (!(i % DUMP_WRAPAROUND))
			wmprintf("\n\r");
	}

	printf("\n\r******** End Dump *******\n\r");
}

unsigned int hex2bin(const char *ibuf, char *obuf,
		     unsigned max_olen)
{
	unsigned int i;		/* loop iteration variable */
	unsigned int j = 0;	/* current character */
	unsigned int by = 0;	/* byte value for conversion */
	unsigned char ch;	/* current character */
	unsigned int len = strlen((char *)ibuf);
	/* process the list of characters */
	for (i = 0; i < len; i++) {
		if (i == (2 * max_olen)) {
			wmprintf("Destination full. "
				"Truncating to avoid overflow.\r\n");
			return j + 1;
		}
		/* get next uppercase character */
		ch = toupper((unsigned char) *ibuf++);

		/* do the conversion */
		if (ch >= '0' && ch <= '9')
			by = (by << 4) + ch - '0';
		else if (ch >= 'A' && ch <= 'F')
			by = (by << 4) + ch - 'A' + 10;
		else {		/* error if not hexadecimal */
			return 0;
		}

		/* store a byte for each pair of hexadecimal digits */
		if (i & 1) {
			j = ((i + 1) / 2) - 1;
			obuf[j] = by & 0xff;
		}
	}
	return j + 1;
}

static void update_psm_file(const char *cfg_file)
{
	if (!cfg_file) {
		printf("No config file given\n");
		return;
	}

	FILE *cfg_fp = fopen(cfg_file, "rb");
	if (!cfg_fp) {
		printf("Could not open %s: %s\n",
		       cfg_file, strerror(errno));
		return;
	}

	int rv;
	char *variable, *value, *new_value = NULL;
	bool eof_detected = false;
	/* printf("Manufacturing Contents:\n"); */
	while (!eof_detected) {
		/* read the whole line into buffer. We are not using
		   getline() here as mingw does not support it */
		int byte_index = 0;
		while (1) {
			g_buffer[byte_index] = fgetc(cfg_fp);
			if (g_buffer[byte_index] == '\n') {
				/* Null terminate */
				g_buffer[byte_index] = 0;
				break;
			}
			if (g_buffer[byte_index] == EOF) {
				g_buffer[byte_index] = 0;
				eof_detected = true;
				break;
			}

			if (byte_index == 0 && g_buffer[byte_index] == '\n') {
				/* Line with only '\n' */
				break;
			}

			byte_index++;
			/* Check impending overrun */
			if (byte_index == sizeof(g_buffer)) {
				fprintf(stderr, "File line size greater than"
					" %d not supported. Aborting\n",
					(int)sizeof(g_buffer));
				_exit(1);
			}
		}

		if (byte_index == 0)
			continue;

		variable = NULL;
		value = NULL;
		/* Get the variable name */
		char *equal_ptr = index(g_buffer, '=');
		if (!equal_ptr) {
			fprintf(stderr, "Invalid line: %s\n", g_buffer);
			_exit(1);
		}

		/* Replace '=' with '\0' thus NULL terminating name */
		*equal_ptr = 0;
		variable = g_buffer;

		if (*(equal_ptr + 1) == ':')
			value = equal_ptr + 2;
		else
			value = equal_ptr + 1;
		uint32_t value_len = strlen(value);

		/*
		  printf("%s(%d) = %s(%d)\n", variable, strlen(variable), value,
		  value_len);
		*/

		if (*(equal_ptr + 1) == ':') {
			if (!value_len % 2) {
				printf("Value len is not even for hex2bin\n");
				_exit(1);
			}

			/* after hex2bin bin length is always half */
			uint32_t bin_len = value_len / 2;

			new_value = malloc(bin_len);
			if (!new_value) {
				fprintf(stderr, "value alloc failed\n");
				_exit(1);
			}
			hex2bin(value, new_value, bin_len);
			rv = psm_set_variable(cached_phandle, variable,
					      new_value, bin_len);
			if (rv != WM_SUCCESS) {
				printf("psm_set_variable failed\r\n");
				_exit(1);
			}
			free(new_value);
		} else
			rv = psm_set_variable(cached_phandle, variable,
					value, value_len);
		if (rv != WM_SUCCESS) {
			printf("Error: Failed to update PSM");
			return;
		}
	}

	fclose(cfg_fp);
}

#ifdef TRUNCATE
static int truncate_file(FILE *fp)
{
	long int extra_chars = 0;
	uint8_t charac = 0xFF;
	int rv;
	/* Remove trailing 0xFF's */
	/* Set file position to end of file, and remove all FF's while
	 * traversing backwords */

	for (extra_chars = 0; charac == 0xFF; extra_chars++) {
		rv = fseek(fp, -extra_chars, SEEK_END);
		if (rv != 0) {
			psmt_e("fseek for read failed: %s", strerror(errno));
			return -1;
		}
		rv = fread(&charac, 1, 1, fp);
	}

	rv = ftruncate(fileno(fp), ftell(fp));
	if (rv != 0) {
		psmt_e("ftruncate failed: %s", strerror(errno));
		return -1;
	}
	return WM_SUCCESS;

}
#endif

#ifdef CONFIG_SECURE_PSM
typedef struct {
	unsigned char iv[16];
	unsigned int num;
	unsigned char ecount[16];
}  ctr_state_t;

uint8_t ctrKey[16];
AES_KEY key;
ctr_state_t ctr_state;

int psm_security_init(psm_sec_hnd_t *sec_handle)
{
	return 0;
}

int psm_key_reset(psm_sec_hnd_t sec_handle, psm_resetkey_mode_t mode)
{
	int rv = hex2bin((char *)encrypt_key, (char *)ctrKey, sizeof(ctrKey));
	/* printf("Processed key: %d bytes\n", rv); */
	memset(&ctr_state, 0x00, sizeof(ctr_state));
	rv = hex2bin((char *)encrypt_nonce, (char *)ctr_state.iv,
		     sizeof(ctr_state.iv));
	/* printf("Processed nonce: %d bytes\n", rv); */
	/* Initializing the encryption KEY */
	rv = AES_set_encrypt_key(ctrKey, 128, &key);
	if (rv < 0) {
		fprintf(stderr, "Could not set encryption key !");
		return -1;
	}

	return WM_SUCCESS;
}

int psm_encrypt(psm_sec_hnd_t sec_handle, const void *plain, void *cipher,
		uint32_t len)
{
	AES_ctr128_encrypt(plain, cipher, len, &key,
			   ctr_state.iv, ctr_state.ecount, &ctr_state.num);
	return 0;
}

int psm_decrypt(psm_sec_hnd_t sec_handle, const void *cipher, void *plain,
		uint32_t len)
{
	AES_ctr128_encrypt(cipher, plain, len, &key,
			   ctr_state.iv, ctr_state.ecount, &ctr_state.num);
	return 0;
}

void psm_security_deinit(psm_sec_hnd_t *sec_handle)
{
	return;
}
#endif /* CONFIG_SECURE_PSM */

static void print_usage(const char *pre_msg)
{
	if (pre_msg)
		wmprintf("%s\n", pre_msg);
	printf("======================\n"
	       "Usage: psm-create <config-file-name> <output-file>\n"
	       "If secure mfg PSM partition is to be generated:\n"
	       "psm-create <config-file-name> <output-file> "
	       "[key-in-hex] [nonce-in-hex]\n"
	       "Config file should be in one of the two following "
		       "format:\n"
	       "1) variable-name=variable-value\n"
	       "AND/OR\n"
	       "2) variable-name=:hexadecimal-variable-value\n"
	       "Each variable should be on a new line\n"
	       "======================\n");
}


int main(int argc, char **argv)
{
	if (argc == 1) {
		print_usage(NULL);
		return 1;
	}
	if (argc < 3 || argc > 6) {
		print_usage("Invalid params");
		return 1;
	}

	snprintf(flash_file, MAX_FILE_NAME_LEN, "%s", argv[2]);

	if (argc == 6 && (strcmp(argv[5], "-s") == 0)) {
		silent_opt = true;
	} else if (argc == 4 && (strcmp(argv[3], "-s") == 0)) {
		silent_opt = true;
	}
#ifdef CONFIG_SECURE_PSM
	if (argc >= 5) {
		encrypt_key = argv[3];
		encrypt_nonce = argv[4];
	}
	if (!silent_opt) {
		printf("Using Encryption Key: %s\n"
		       "      Nonce        : %s\n",
		       encrypt_key, encrypt_nonce);
	}
#endif

	long flash_size = FLASH_FILE_SIZE;
	fdesc.fl_size = flash_size;

	int rv;
	if (!access(flash_file, W_OK | R_OK)) {
		if (!silent_opt)
			printf("Overwriting existing file %s\n", flash_file);
	}

	/* No existing file */
	rv = create_flash_file(flash_size);
	if (rv != 0) {
		return rv;
	}

	fp = fopen(flash_file, "rb+");
	if (fp == NULL) {
		psmt_e("Unable to open flash file");
		return -1;
	}

	psm_cfg_t psm_cfg;
	memset(&psm_cfg, 0x00, sizeof(psm_cfg_t));
#ifdef CONFIG_SECURE_PSM
	if (encrypt_key)
		psm_cfg.secure = true;
#endif /* CONFIG_SECURE_PSM */

	rv = psm_module_init(&fdesc, &cached_phandle, &psm_cfg);
	if (rv != WM_SUCCESS) {
		printf("%s: init failed\r\n", __func__);
		return rv;
	}

	update_psm_file(argv[1]);

#ifdef TRUNCATE
	truncate_file(fp);
#endif
	psm_module_deinit(&cached_phandle);
	cached_phandle = 0;
	fclose(fp);

#ifdef SEGGER
	generate_hex_file("flash.dat");
#endif

	return 0;
}

