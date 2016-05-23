/*
 * Copyright (C) 2015, Marvell International Ltd.
 * All Rights Reserved.
 */

#ifndef __UTILS_H__
#define __UTILS_H__

#include <stdint.h>
#include <string.h>

#define sboot_e(__fmt__, ...)					\
	printf("[sboot] Error: "__fmt__"\n", ##__VA_ARGS__)

#define sboot_w(__fmt__, ...)					\
	printf("[sboot] Warn: "__fmt__"\n", ##__VA_ARGS__)

#define sboot_v(__fmt__, ...)					\
	if (verbose)						\
		printf("[sboot] Info: "__fmt__"\n", ##__VA_ARGS__)

#define sboot_i(__fmt__, ...)					\
	printf("[sboot] Info: "__fmt__"\n", ##__VA_ARGS__)

#define sboot_assert(cond, cleanup_func, errcode, __fmt__, ...)\
	if (!(cond)) {						\
		if (strlen(__fmt__))				\
			sboot_e(__fmt__, ##__VA_ARGS__);	\
		cleanup_func;					\
		return errcode;					\
	}

/* utils.c declarations */
/* Used by main to communicate with parse_opt. */
struct arguments {
	char *boot2_file;
	char *fw_file;
	char *config_file;
	char *otp_file;
	char *ks_file;
	char *makefile;
	int rawboot2;
};

extern int verbose;
extern char *outdir;

int parse_opt(int argc, char **argv, struct arguments *arguments);

#define MAX_SUFFIX 12
int file_len(char *file);
int file_read(char *file, uint8_t **buf);
char *get_outfile_name(char *infile, uint8_t add_header, uint8_t keystore,
			uint8_t encrypt, uint8_t sign);
void get_outfile_suffix(char *suffix, uint8_t add_header, uint8_t keystore,
			uint8_t encrypt, uint8_t sign);
char *get_outdir_filename(char *infile, char *suffix);

int dec2bin(const char *ibuf, void *obuf, unsigned max_olen);
int binstr2bin(const char *ibuf, uint8_t *obuf, unsigned max_olen);
uint32_t hex2bin(const char *ibuf, uint8_t *obuf, unsigned max_olen);
char *bin2hex(const uint8_t *src, char *dest, unsigned int src_len,
		unsigned int dest_len);

int PlatformRandomBytes(void *inBuffer, size_t inByteCount);

/* parse_config.c declarations */
#define MAX_CONFIG_LINE_SIZE 2048
#define MAX_CONFIG_STORE_SIZE (8 * 1024)

extern uint8_t config_store[];
int parse_config_file(char *config_file);

/* otp.c declarations */
#define OTP_JTAG_ENABLE 0x0
#define OTP_JTAG_DISABLE 0x1
#define OTP_USB_UART_BOOT_ENABLE 0x0
#define OTP_USB_UART_BOOT_DISABLE 0x2
#define OTP_ENCRYPTED_BOOT 0x4
#define OTP_SIGNED_BOOT 0x8
#define MAX_OTP_USER_DATA 252

int otp_write_header(FILE *otp);
int otp_write_boot_config(FILE *otp, uint8_t boot_config);
int otp_write_decrypt_key(FILE *otp, const uint8_t *key, uint32_t len);
int otp_write_public_key_hash(FILE *otp, const uint8_t *hash, uint32_t len);
int otp_write_user_data(FILE *otp, const uint8_t *udata, uint32_t len);
int otp_close_file(FILE *otp);
int generate_otpfile(char *infile);

/* boot2_generator.c declarations */
extern uint8_t keystore[SB_KEYSTORE_SIZE];

int generate_keystore();
int write_keystore_to_file(char *keystore_file);
int get_boot2_encrypt_type(uint8_t *pencrypt_type);
int get_boot2_hash_type(uint8_t *phash_type);
int get_boot2_sign_type(uint8_t *psign_type);
int generate_boot2(char *infile, int add_header);

/* fw_generator.c declarations */
#define true 1
#define false 0

int get_fw_encrypt_type(uint8_t *pencrypt_type);
int get_fw_hash_type(uint8_t *phash_type);
int get_fw_sign_type(uint8_t *psign_type);
int generate_fw(char *infile);

/* Functions to generate negative test images for secure boot validation */
#ifdef TEST_BUILD
enum test_num_t {
	TEST_NO_ERROR		= 0,
	TEST_BAD_KS_MAGIC	= 1 << 0,
	TEST_BAD_KS_LEN		= 1 << 1,
	TEST_BAD_KS_CRC		= 1 << 2,
	TEST_BAD_FW_SHMAGIC	= 1 << 3,
	TEST_BAD_FW_SHLEN	= 1 << 4,
	TEST_BAD_FW_SHCRC	= 1 << 5,
	TEST_BAD_FW_IMGLEN	= 1 << 6,
	TEST_BAD_FW_NONCE	= 1 << 7,
	TEST_BAD_FW_MIC		= 1 << 8,
	TEST_BAD_FW_SIGN	= 1 << 9
};

void corrupt_keystore(uint8_t *keystore);
void corrupt_sec_hdr(uint8_t *sec_hdr);
void corrupt_sec_hdr_keys(uint8_t *sec_hdr);
#endif

#define ERR_KEYSTORE_GEN -1
#define ERR_UNSUPPORTED_ENCRYPT_ALGO -2
#define ERR_INSUFFICIENT_MEMORY -3
#define ERR_NO_HASH -4
#define ERR_UNSUPPORTED_HASH_ALGO -5
#define ERR_UNSUPPORTED_SIGN_ALGO -6
#define ERR_NO_PRIVATE_KEY -7
#define ERR_NO_PUBLIC_KEY -8
#define ERR_NO_ENCRYPT_KEY -9
#define ERR_NO_DECRYPT_KEY -10
#define ERR_NO_NONCE -11
#define ERR_SIGNATURE_GENERATION -12
#define ERR_RNG_INIT -13
#define ERR_PRIVATE_KEY_DECODE -14
#define ERR_HASH_INIT -15
#define ERR_HASH_UPDATE -16
#define ERR_HASH_FINISH -17
#define ERR_SIGNATURE_INIT -18
#define ERR_ENCRYPT_INIT -19
#define ERR_ENCRYPT_DATA -20
#define ERR_INVALID_CONFIG_FILE -21
#define ERR_FILE_OPEN -22
#define ERR_FILE_READ -23
#define ERR_FILE_WRITE -24
#define ERR_BAD_OUTFILE_NAME -25
#define ERR_INVALID_BOOT2_FILE -26
#define ERR_INVALID_FW_FILE -27
#define ERR_INVALID_ARGS -28
#define ERR_NO_BOOT2_LOAD_ADDR -29
#define ERR_NO_BOOT2_ENTRY_ADDR -30
#define ERR_ENCRYPT_QUERY -31

#endif /* _UTILS_H_ */
