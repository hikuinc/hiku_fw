#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <hkdf-sha.h>
#ifdef CONFIG_FWGEN_ED_CHACHA
#include <chacha20.h>
#include <ed25519.h>
#endif
#ifdef CONFIG_FWGEN_RSA_AES
#include <cyassl/ctaocrypt/aes.h>
#include <cyassl/ctaocrypt/rsa.h>
#include "fw_upg_keys.h"
#endif


#define SIGNING_KEY_ID          "fwupg_signing_key"
#define VERIFICATION_KEY_ID          "fwupg_verification_key"
#define ENCRYPT_ID	"fwupg_encrypt_decrypt_key"
#define VERSION_ID	"fwupg_version"

#define SK_FLAG			(1 << 0)
#define PK_FLAG			(1 << 1)
#define ENCRYPT_KEY_FLAG	(1 << 2)
#define VERSION_FLAG		(1 << 3)
/* VERSION_FLAG is purposely left out of ALL_FLAGS as it was added in a later
 * version of FW upgrade utility.
 */
#define ALL_FLAGS (SK_FLAG | PK_FLAG |  ENCRYPT_KEY_FLAG)
#define DELIM	":"
#define DEF_CONFIG_FILE	"config"

bool silent_output;

/* Current FW upgrade spec version */
static uint32_t fw_upgrade_spec_version = 2;
/* FW Upgrade spec version in the source config file.
 * It has been initialized with 1 as the version field was
 * added in the config from 2. SO, if the version field is
 * not found in source config file, we will assume it as 1
 */
static uint32_t src_fw_upgrade_spec_version = 1;

void put_u32_le(void *vp, uint32_t v)
{
	uint8_t *p = (uint8_t *)vp;

	p[0] = (uint8_t)v & 0xff;
	p[1] = (uint8_t)(v >> 8) & 0xff;
	p[2] = (uint8_t)(v >> 16) & 0xff;
	p[3] = (uint8_t)(v >> 24) & 0xff;
}

void hexdump_nospace(uint8_t *buf, int len)
{
	int i;
	for (i = 0; i < len; i++, buf++)
		printf("%02x", *buf);
	printf("\n");
}

unsigned int hex2bin(const uint8_t *ibuf, uint8_t *obuf,
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
			return j + 1;
		}
		ch = toupper(*ibuf++);	/* get next uppercase character */

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

void bin2hex(uint8_t *src, char *dest, unsigned int src_len,
		unsigned int dest_len)
{
	int i;
	for (i = 0; i < src_len; i++) {
		if (snprintf(dest, dest_len, "%02x", src[i]) >= dest_len) {
			printf(" Destination full. "
					"Truncating to avoid overflow.\r\n");
			return;
		}
		dest_len -= 2;
		dest += 2;
	}
}
int get_random_sequence(void *inBuffer, size_t inByteCount)
{
	int fd;
	fd = open("/dev/urandom", O_RDONLY);
	if (fd) {
		read(fd, inBuffer, inByteCount);
		close(fd);
		return 0;
	}
	return -1;
}

static int SHA512_vector(const unsigned char *d, size_t n, unsigned char *md)
{
	SHA512Context     sha;

	SHA512Reset(&sha);
	SHA512Input(&sha, d, n);
	SHA512Result(&sha, md);

	return 0;
}
static void print_keys(char *key_name, char *val)
{
	if (!silent_output)
		printf(" %s : %s", key_name, val);
}

#ifdef CONFIG_FWGEN_ED_CHACHA

uint8_t skey[32];
uint8_t pkey[32];
uint8_t encrypt_key[32];
uint8_t nonce[8];

void key_pair_handler()
{
	ed25519_secret_key sk;
	ed25519_public_key pk;

	printf("%s=:", SIGNING_KEY_ID);
	get_random_sequence(sk, sizeof(sk));
	hexdump_nospace(sk, sizeof(sk));

	printf("%s=:", VERIFICATION_KEY_ID);
	ed25519_publickey(sk, pk);
	hexdump_nospace(pk, sizeof(pk));

	get_random_sequence(pk, sizeof(pk));
	printf("%s=:", ENCRYPT_ID);
	hexdump_nospace(pk, sizeof(pk));

	printf("%s:%d\n", VERSION_ID, fw_upgrade_spec_version);

}

void chacha_encrypt(char *ip_file, char *op_file, char *config_file)
{
	ed25519_signature ed_sign;
	uint32_t fw_len;
	uint8_t sha_hash[64];
	char buf[512 * 1024];
	char line[100];
	uint8_t	flags = 0;
	char *val;
	FILE *fp1, *fp2;
	FILE *cfg = fopen(config_file, "r");
	if (!cfg) {
		printf(" Could not open %s\r\n", config_file);
		return;
	}
	while (fgets(line, sizeof(line), cfg)) {
		if (!strncmp(line, SIGNING_KEY_ID, strlen(SIGNING_KEY_ID))) {
			val = strstr(line, DELIM) + 1;
			if (val) {
				hex2bin((uint8_t *)val, skey, 32);
				print_keys(SIGNING_KEY_ID, val);
				flags |= SK_FLAG;
			}
		} else if (!strncmp(line, VERIFICATION_KEY_ID,
				 strlen(VERIFICATION_KEY_ID))) {
			val = strstr(line, DELIM) + 1;
			if (val) {
				hex2bin((uint8_t *)val, pkey, 32);
				print_keys(VERIFICATION_KEY_ID, val);
				flags |= PK_FLAG;
			}
		} else if (!strncmp(line, ENCRYPT_ID, strlen(ENCRYPT_ID))) {
			val = strstr(line, DELIM) + 1;
			if (val) {
				hex2bin((uint8_t *)val, encrypt_key, 32);
				print_keys(ENCRYPT_ID, val);
				flags |= ENCRYPT_KEY_FLAG;
			}
		} else if (!strncmp(line, VERSION_ID, strlen(VERSION_ID))) {
			val = strstr(line, DELIM) + 1;
			if (val) {
				src_fw_upgrade_spec_version = atoi(val);
				flags |= VERSION_FLAG;
			}
		}
	}
	if (!silent_output)
		printf(" %s : %d\n", VERSION_ID, src_fw_upgrade_spec_version);
	if ((flags & ALL_FLAGS) != ALL_FLAGS) {
		printf
			(" Something missing in the config file."
				" Expected format:\r\n");
		printf(" fwupg_signing_key=:<Secret Key>\r\n");
		printf(" fwupg_verification_key=:<Public Key>\r\n");
		printf(" fwupg_encrypt_decrypt_key=:<Encrypt Key>\r\n");
		printf(" fwupg_version:<Firmware Upgarde Version>\r\n");
		return;
	}
	if (!silent_output)
		printf(" Using %s as the input file\r\n", ip_file);
	fp1 = fopen(ip_file, "r");
	if (!fp1) {
		printf(" Could not open %s\r\n", ip_file);
		return;
	}
	fp2 = fopen(op_file, "w");
	if (!fp2) {
		printf(" Could not open %s\r\n", op_file);
		fclose(fp1);
		return;
	}
	fw_len = fread(buf, 1, sizeof(buf), fp1);

	SHA512_vector((unsigned char *)buf, fw_len, sha_hash);
	ed25519_sign(sha_hash, sizeof(sha_hash), skey, pkey, ed_sign);


	/* Write the firmware upgrade specification version */
	uint8_t tmp_buf[4] = {0, 0, 0, 1};
	if (src_fw_upgrade_spec_version > 1) {
		put_u32_le(tmp_buf, fw_upgrade_spec_version);
	}
	fwrite(tmp_buf, 1, sizeof(tmp_buf), fp2);

	if ((src_fw_upgrade_spec_version > 1)) {
		uint32_t header_length = sizeof(nonce) + sizeof(fw_len);
		put_u32_le(tmp_buf, header_length);
		fwrite(tmp_buf, 1, sizeof(tmp_buf), fp2);
		put_u32_le(tmp_buf, fw_len);
		fwrite(tmp_buf, 1, sizeof(tmp_buf), fp2);
	}

	/* Generate a random Nonce */
	get_random_sequence(nonce, sizeof(nonce));
	fwrite(nonce, 1, sizeof(nonce), fp2);

	chacha20_ctx_t chctx;
	chacha20_init(&chctx, encrypt_key, sizeof(encrypt_key),
			nonce, sizeof(nonce));
	chacha20_encrypt(&chctx, ed_sign, ed_sign, sizeof(ed_sign));
	fwrite(ed_sign, 1, sizeof(ed_sign), fp2);

	chacha20_encrypt(&chctx, (uint8_t *)buf, (uint8_t *)buf, fw_len);
	fwrite(buf, 1, fw_len, fp2);

	fclose(fp1);
	fclose(fp2);
	if (!silent_output) {
		printf(" Encrypted firmware image file \"%s\" created\r\n",
				op_file);
	}
}
#endif /* CONFIG_FWGEN_ED_CHACHA */

#ifdef CONFIG_FWGEN_RSA_AES

static char buf[512 * 1024];
static char outbuf[512 * 1024];
static char line[3000];

uint8_t skey[2048];
uint8_t pkey[2048];
uint8_t encrypt_key[16];
uint8_t nonce[16];

int aes_encrypt(char *ip_file, char *op_file, char *config_file)
{
	RsaKey key;
	RNG    rng;
	word32 idx = 0;
	Aes enc;
	uint8_t sig[256];
	uint8_t signout[256];
	int ret;
	uint32_t fw_len;
	uint8_t sha_hash[64];
	uint8_t	flags = 0;
	char *val;
	FILE *fp1, *fp2;
	FILE *cfg = fopen(config_file, "r");
	if (!cfg) {
		printf(" Could not open %s\r\n", config_file);
		return -1;
	}
	while (fgets(line, sizeof(line), cfg)) {
		if (!strncmp(line, SIGNING_KEY_ID, strlen(SIGNING_KEY_ID))) {
			val = strstr(line, DELIM) + 1;
			if (val) {
				hex2bin((uint8_t *)val, skey, 2048);
				print_keys(SIGNING_KEY_ID, val);
				flags |= SK_FLAG;
			}
		} else if (!strncmp(line, VERIFICATION_KEY_ID,
				 strlen(VERIFICATION_KEY_ID))) {
			val = strstr(line, DELIM) + 1;
			if (val) {
				hex2bin((uint8_t *)val, pkey, 2048);
				print_keys(VERIFICATION_KEY_ID, val);
				flags |= PK_FLAG;
			}
		} else if (!strncmp(line, ENCRYPT_ID, strlen(ENCRYPT_ID))) {
			val = strstr(line, DELIM) + 1;
			if (val) {
				hex2bin((uint8_t *)val, encrypt_key, 16);
				print_keys(ENCRYPT_ID, val);
				flags |= ENCRYPT_KEY_FLAG;
			}
		} else if (!strncmp(line, VERSION_ID, strlen(VERSION_ID))) {
			val = strstr(line, DELIM) + 1;
			if (val) {
				src_fw_upgrade_spec_version = atoi(val);
				flags |= VERSION_FLAG;
			}
		}
	}
	if (!silent_output)
		printf(" %s : %d\n", VERSION_ID, src_fw_upgrade_spec_version);
	if ((flags & ALL_FLAGS) != ALL_FLAGS) {
		printf(" Something missing in the config file."
		       " Expected format:\r\n");
		printf(" fwupg_signing_key=:<Secret Key>\r\n");
		printf(" fwupg_verification_key=:<Public Key>\r\n");
		printf(" fwupg_encrypt_decrypt_key=:<Encrypt Key>\r\n");
		printf(" fwupg_version:<Firmware Upgarde Version>\r\n");
		return -1;
	}
	if (!silent_output)
		printf(" Using %s as the input file\r\n", ip_file);
	fp1 = fopen(ip_file, "r");
	if (!fp1) {
		printf(" Could not open %s\r\n", ip_file);
		return -1;
	}
	fp2 = fopen(op_file, "w");
	if (!fp2) {
		printf(" Could not open %s\r\n", op_file);
		fclose(fp1);
		return -1;
	}
	fw_len = fread(buf, 1, sizeof(buf), fp1);

	SHA512_vector((unsigned char *)buf, fw_len, sha_hash);

	/* Write the firmware upgrade specification version */
	uint8_t tmp_buf[4] = {0, 0, 0, 1};
	if (src_fw_upgrade_spec_version > 1) {
		put_u32_le(tmp_buf, fw_upgrade_spec_version);
	}
	fwrite(tmp_buf, 1, sizeof(tmp_buf), fp2);

	if ((src_fw_upgrade_spec_version > 1)) {
		uint32_t header_length = sizeof(nonce) + sizeof(fw_len);
		put_u32_le(tmp_buf, header_length);
		fwrite(tmp_buf, 1, sizeof(tmp_buf), fp2);
		put_u32_le(tmp_buf, fw_len);
		fwrite(tmp_buf, 1, sizeof(tmp_buf), fp2);
	}

	/* Generate a random Nonce */
	get_random_sequence(nonce, sizeof(nonce));
	fwrite(nonce, 1, sizeof(nonce), fp2);

	/* Below api calls generates the RSA Signature from the RSA
	   Private Key specified through rsa_key_der_2048 */
	InitRsaKey(&key, 0);

	ret = RsaPrivateKeyDecode(skey, &idx, &key, sizeof(skey));
	if (ret != 0) {
		printf(" RSA Private Key Decode error: %d\r\n", ret);
		return -41;
	}

	ret = InitRng(&rng);
	if (ret != 0) {
		printf(" RSA Random number generator error: %d\r\n",
		       ret);
		return -42;
	}

	ret = RsaSSL_Sign(sha_hash, sizeof(sha_hash), sig, sizeof(sig),
			  &key, &rng);
	if (!silent_output)
		printf(" Generated RSA Signature size : %d\r\n", ret);
	if (ret < 0) {
		printf(" RSA Sign error : %d\r\n", ret);
		return -46;
	}

	/* Encrypt the RSA Signature using AES CTR 128-bit */
	AesSetKeyDirect(&enc, encrypt_key, AES_BLOCK_SIZE, nonce,
			AES_ENCRYPTION);
	AesCtrEncrypt(&enc, signout, sig, sizeof(sig));

	/* Write the Encrypted RSA Signature to the output file */
	fwrite(signout, 1, sizeof(sig), fp2);

	/* Encrypt the firmware image data */
	AesCtrEncrypt(&enc, outbuf, buf, fw_len);

	if (!silent_output)
		printf(" Performed AES CTR 128-bit Encryption : %d\r\n",
				fw_len);
	/* Write the Encrypted firmware data to the output file */
	fwrite(outbuf, 1, fw_len, fp2);
	if (!silent_output)
		printf(" Encrypted firmware image file \"%s\" created\r\n",
				op_file);
	fclose(fp1);
	fclose(fp2);
}

#define FOURK_BUF 4096
void key_pair_handler()
{
	FILE *file;
	byte *tmp;
	size_t bytes;

	tmp = (byte *)malloc(FOURK_BUF);
	if (tmp == NULL)
		return;

	system("openssl genrsa -out prvKey.pem 2048 >/dev/null 2>&1");
	system("openssl rsa -in prvKey.pem -pubout -out pubKey.der"
	       " -outform DER >/dev/null 2>&1");
	system("openssl rsa -in prvKey.pem -outform DER -out prvKey.der"
	       " >/dev/null 2>&1");
	file = fopen("prvKey.der", "rb");
	if (!file) {
		free(tmp);
		return;
	}
	bytes = fread(tmp, 1, FOURK_BUF, file);
	fclose(file);
	printf("%s=:", SIGNING_KEY_ID);
	hexdump_nospace(tmp, bytes);

	file = fopen("pubKey.der", "rb");
	if (!file) {
		free(tmp);
		return;
	}
	bytes = fread(tmp, 1, FOURK_BUF, file);
	fclose(file);
	printf("%s=:", VERIFICATION_KEY_ID);
	hexdump_nospace(tmp, bytes);

	get_random_sequence(tmp, 16);
	printf("%s=:", ENCRYPT_ID);
	hexdump_nospace(tmp, 16);

	printf("%s:%d\n", VERSION_ID, fw_upgrade_spec_version);
	return;
}
#endif /* CONFIG_FWGEN_RSA_AES */


int main(int argc, char **argv)
{
	if (argc == 2 && !strcmp(argv[1], "config")) {
		key_pair_handler();
	} else if (argc == 4 || argc == 5) {
		if ((argc == 5) && (strcmp(argv[4], "-s") == 0))
			silent_output = true;
#ifdef CONFIG_FWGEN_ED_CHACHA
		chacha_encrypt(argv[1], argv[2], argv[3]);
#endif
#ifdef CONFIG_FWGEN_RSA_AES
		aes_encrypt(argv[1], argv[2], argv[3]);
#endif
	} else {
		printf(" Usage:\r\n");
		printf(" Generate a config: ./<crypto_method>_fw_generator"
		       " config > <config_file>\r\n");
		printf(" Create an encrypted image:"
			"./<crypto_method>_fw_generator"
			"<ipfile> <opfile> <config_file> [-s]\r\n");
		printf(" Where crypto_method can be "
			"either ed_chacha or rsa_aesctr\r\n");
		printf(" -s can be used to suppress the output "
				"print messages\r\n");
	}
	return 0;
}


