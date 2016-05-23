/*
 *  Copyright (C) 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* Header files*/
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <wmlist.h>
#include <wmerrno.h>
#include <wm_os.h>
#include <cli.h>
#include <wm_utils.h>
#include <mdev_aes.h>
#include <stdint.h>

#define AES_TEST_DEBUG

#define aest_e(__fmt__, ...)					\
	wmprintf("[aest] Error: "__fmt__"\r\n", ##__VA_ARGS__)
#define aest_w(__fmt__, ...)					\
	wmprintf("[aest] Warn: "__fmt__"\r\n", ##__VA_ARGS__)

#ifdef AES_TEST_DEBUG
#define aest_d(__fmt__, ...)			\
	wmprintf(""__fmt__"", ##__VA_ARGS__)
#else
#define aest_d(...)
#endif

typedef struct sys_node {
	/* Mode of the test applied */
	int test_mode;
	/* char string (Plain-text) */
	uint8_t *plain;
	/* char string (Key) */
	uint8_t *key;
	/* char string (Initialization Vector i.e. IV) */
	uint8_t *iv;
	 /* Length of Plain-text */
	int len;
	 /* Length of Key */
	int key_sz;
	 /* Length of IV */
	int iv_sz;
	/* char string (Cipher-text) */
	uint8_t *cipher;
	 /* char string (Recovered-Plain-text) */
	uint8_t *rec_plain;
	/* Linux link list (wmlist.h) */
	list_head_t list;
} sys_node_t;

int MAX_PLAIN_TXT_LEN = 256;

static void prnt_hex(uint8_t *in_text, int len, char *helpStr)
{
	int i;
	aest_d("\t[Hex] [%s][%d]", helpStr, len);
	for (i = 0; i < len; i++) {
		if (i % 16 == 0)
			aest_d("\r\n\t");
		aest_d("%02x ", in_text[i]);
	}
	aest_d("\r\n");
}

static void verify_objects(list_head_t *anchor)
{
	aest_d("\t================================\r\n");
	aest_d("\t======[Verifying the list]======\r\n");
	aest_d("\t================================\r\n");
	sys_node_t *trav;

	list_for_each_entry(trav, anchor, list) {
		if (trav->key)
			prnt_hex(trav->key, trav->key_sz, "Key");

		if (trav->iv)
			prnt_hex(trav->iv, trav->iv_sz, "IV");

		if (trav->len)
			aest_d("\t[Len][%d]\r\n", trav->len);

		if (trav->cipher)
			prnt_hex(trav->cipher, trav->len, "Cipher");

		if (trav->plain)
			prnt_hex(trav->plain, trav->len, "Plain");

		if (trav->rec_plain)
			prnt_hex(trav->rec_plain, trav->len, "Rec Plain");

		aest_d("\r\n\n");
	}

	aest_d("\t================================\r\n");
	aest_d("\t====[Verifying objects done]====\r\n");
	aest_d("\t================================\r\n");
}
static void free_obj(sys_node_t *trav)
{
	if (trav->key) {
		os_mem_free(trav->key);
		aest_d("\t[key]\t[done]\r\n");
	}
	if (trav->iv) {
		os_mem_free(trav->iv);
		aest_d("\t[iv]\t[done]\r\n");
	}
	if (trav->cipher) {
		os_mem_free(trav->cipher);
		aest_d("\t[cipher]\t[done]\r\n");
	}
	if (trav->plain) {
		os_mem_free(trav->plain);
		aest_d("\t[plain]\t[done]\r\n");
	}
	if (trav->rec_plain) {
		os_mem_free(trav->rec_plain);
		aest_d("\t[rec_plain]\t[done]\r\n");
	}
	os_mem_free(trav);
}
static void free_all_links(list_head_t *anchor)
{
	aest_d("\t===========================\r\n");
	aest_d("\t=====[In freeing mode]=====\r\n");
	aest_d("\t===========================\r\n");
	sys_node_t *trav;
	list_head_t *link = anchor->next;

#ifdef AES_TEST_DEBUG
	int node_ctr = 0;
#endif

	while (!list_empty(anchor)) {
		aest_d("\t[Node no][%d]\r\n", node_ctr++);
		trav = list_entry(link, sys_node_t, list);
		list_del(link);
		link = anchor->next;

		free_obj(trav);
		aest_d("\t[Freed it]\t\r\n\n");
	}
	os_mem_free(anchor);
}

static int validation(uint8_t *in1, uint8_t *in2, int len)
{
	int i, rv = true;
	aest_d("\t[Validation]\t[");
	for (i = 0; i < len; i++) {
		if (in1[i] - in2[i]) {
			aest_d("*");
			rv = false;
		} else {
			aest_d("-");
		}
	}
	aest_d("]\r\n");
	return rv;
}

/* Common function for 4 modes*/
/* Common function: ==|Unit Test|==*/
static int aes_test_common(hw_aes_mode_t HW_AES_MODE)
{
	aes_t enc_aes;
	aes_t dec_aes;
	memset(&enc_aes, 0, sizeof(aes_t));
	memset(&dec_aes, 0, sizeof(aes_t));

	int rv = 0;
	uint8_t key[] = {
		0xc2, 0x86, 0x69, 0x6d,
		0x88, 0x7c, 0x9a, 0xa0,
		0x61, 0x1b, 0xbb, 0x3e,
		0x20, 0x25, 0xa4, 0x5a
	};

	uint8_t initial_vector[] = {
		0x56, 0x2e, 0x17, 0x99,
		0x6d, 0x09, 0x3d, 0x28,
		0xdd, 0xb3, 0xba, 0x69,
		0x5a, 0x2e, 0x6f, 0x58
	};

	uint8_t plain[] = {
		0xaa, 0xbb, 0xcc, 0xdd,
		0xee, 0xff, 0x11, 0x22,
		0x33, 0x44, 0x55, 0x66,
		0x77, 0x88, 0x99, 0x00
	};

	uint8_t cipher[16];
	uint8_t recovered_plain[16];

	aes_drv_init();
	mdev_t *aes_dev = aes_drv_open(MDEV_AES_0);

	/* Special for CCM mode*/
	if (HW_AES_MODE == HW_AES_MODE_CCM) {
		uint8_t authTag[sizeof(plain)];
		uint8_t nonce[] = {
			0x00, 0x11, 0x22, 0x33,
			0x44, 0x55, 0x66, 0x77,
			0x88, 0x99, 0xaa, 0xbb,
			0xcc
		};  /* Length of nonce is 13 (This is ideal length for nonce) */
		uint8_t authIn[] = {
			0x01, 0x12, 0x23, 0x34,
			0x45, 0x56, 0x67, 0x78,
			0x89, 0x90, 0x01, 0x12,
			0x23, 0x34, 0x45, 0x56
		};

		aes_drv_ccm_setkey(aes_dev, &enc_aes, key, sizeof(key));
		aes_drv_ccm_encrypt(
				    aes_dev,
				    &enc_aes,
				    (const char *) plain,
				    (char *) cipher,
				    sizeof(plain),
				    (const char *) nonce,
				    sizeof(nonce),
				    (char  *) authTag,
				    sizeof(authTag),
				    (const char *) authIn,
				    sizeof(plain)
				    );


		aes_drv_ccm_decrypt(
				    aes_dev,
				    &enc_aes,
				    (const char *) cipher,
				    (char *) recovered_plain,
				    sizeof(cipher),
				    (const char *) nonce,
				    sizeof(nonce),
				    (const char *) authTag,
				    sizeof(authTag),
				    (const char *) authIn,
				    sizeof(plain)
				    );

		prnt_hex(key, sizeof(key), "Key");
		prnt_hex(nonce, sizeof(nonce), "Nonce");
		prnt_hex(plain, sizeof(plain), "Plain");
		prnt_hex(authIn, sizeof(authIn), "Auth Integrity (AAD)");
		prnt_hex(cipher, sizeof(cipher), "Cipher");
		prnt_hex(authTag, sizeof(authTag), "Auth Tag");
		prnt_hex(recovered_plain, sizeof(recovered_plain),
			 "Recov Plain");

		goto the_end;
	}

	aes_drv_setkey(
		       aes_dev,
		       &enc_aes,
		       key,
		       sizeof(key),
		       initial_vector,
		       AES_ENCRYPTION,
		       HW_AES_MODE
		       );
	aes_drv_setkey(
		       aes_dev,
		       &dec_aes, key,
		       sizeof(key),
		       initial_vector,
		       AES_DECRYPTION,
		       HW_AES_MODE
		       );

	aes_drv_encrypt(
			aes_dev,
			&enc_aes,
			(const uint8_t *) plain,
			cipher,
			sizeof(plain)
			);
	aes_drv_decrypt(
			aes_dev,
			&dec_aes,
			(const uint8_t *) cipher,
			recovered_plain,
			sizeof(cipher)
			);

	prnt_hex(key, sizeof(key), "Key");
	prnt_hex(initial_vector, sizeof(initial_vector), "IV");
	prnt_hex(plain, sizeof(plain), "Plain");
	prnt_hex(cipher, sizeof(cipher), "Cipher");
	prnt_hex(recovered_plain, sizeof(recovered_plain), "Recov Plain");

 the_end:
	rv = validation(plain, recovered_plain, sizeof(plain));
	rv -= validation(plain, cipher, sizeof(plain));
	aes_drv_close(aes_dev);
	if (rv == 1)
		return WM_SUCCESS;
	else
		return -WM_FAIL;

}

static void print_test_status(int rv, const char *test_name)
{
	if (rv != WM_SUCCESS)
		wmprintf("\tTest Case : %s Failed \r\n", test_name);
	else
		wmprintf("\tTest Case : %s Successful OK \r\n", test_name);
}

static int verify_list(list_head_t *anchor)
{
	if (anchor->next == anchor) {
		return -WM_FAIL;
	}
	verify_objects(anchor);
	int rv = 0;
	sys_node_t *link;
	list_for_each_entry(link, anchor, list) {
		rv = validation(link->plain, link->rec_plain, link->len);
		rv -= validation(link->plain, link->cipher, link->len);

		if (rv != 1)
			return -WM_FAIL;
	}
	return WM_SUCCESS;
}

static void aes_on_list(list_head_t *anchor, unsigned int mode)
{
	if (anchor->next == anchor) {
		return;
	}
	sys_node_t *link;
	list_for_each_entry(link, anchor, list) {
		/* AES operation */
		aes_t enc_aes;
		aes_t dec_aes;
		memset(&enc_aes, 0, sizeof(aes_t));
		memset(&dec_aes, 0, sizeof(aes_t));

		aes_drv_init();
		mdev_t *aes_dev = aes_drv_open(MDEV_AES_0);

		aest_d("\tkey_sz[%d] iv_sz[%d] len[%d]\r\n",
		       link->key_sz, link->iv_sz, link->len);
		aes_drv_setkey(
			       aes_dev,
			       &enc_aes,
			       link->key,
			       link->key_sz,
			       link->iv,
			       AES_ENCRYPTION,
			       mode
			       );
		aes_drv_setkey(
			       aes_dev,
			       &dec_aes,
			       link->key,
			       link->key_sz,
			       link->iv,
			       AES_DECRYPTION,
			       mode
			       );

		aes_drv_encrypt(
				aes_dev,
				&enc_aes,
				(const uint8_t *) link->plain,
				link->cipher,
				link->len
				);
		aes_drv_decrypt(
				aes_dev,
				&dec_aes,
				(const uint8_t *) link->cipher,
				link->rec_plain,
				link->len
				);

		aes_drv_close(aes_dev);
	}
}
static void add_link(sys_node_t *link, list_head_t *anchor)
{
	INIT_LIST_HEAD(&link->list);
	list_add(&link->list, anchor);
}
/* This function will initialise the node with
 * key:		Random with size equal to key_sz
 * iv:		Random with size equal to iv_sz
 * plain:	Random with random size
 */
static int node_init(sys_node_t *link, int key_sz, int iv_sz, unsigned int mode)
{
	int pad_size = 0, rand_size = 0, i;

	if  ((key_sz == 16) || (key_sz == 24) || (key_sz == 32))
		aest_d("\t[key_sz][%d][ok]\r\n", key_sz);
	else {
		aest_d("\t[key_sz][%d][not_ok] => [16][default]\r\n");
		key_sz = 16;  /* Default assigned */
	}

	link->key_sz = key_sz;
	link->key = os_mem_alloc(link->key_sz + 1);
	if (!link->key) {
		return -WM_E_NOMEM;
	}
	memset(link->key, 'A', link->key_sz + 1);  /* Intilialise to 'A' */
	link->key[link->key_sz] = '\0';  /* The Last character as '\0' */

	for (i = 0; i < key_sz; i++) {
		link->key[i] = rand() % 128;
		 /* One byte assignement (2 hex char of 4 bit each) */
	}

	if ((iv_sz == 16) || (iv_sz == 24) || (iv_sz == 32))
		aest_d("\t[iv_sz][%d][ok]\r\n", iv_sz);
	else {
		aest_d("\t[iv_sz][%d][not_ok] => [16][default]\r\n");
		iv_sz = 16;  /* Default assigned */
	}

	link->iv_sz = iv_sz;
	link->iv = os_mem_alloc(iv_sz + 1);
	if (!link->iv) {
		return -WM_E_NOMEM;
	}
	memset(link->iv, 'B', link->iv_sz + 1);
	link->iv[link->iv_sz] = '\0';

	for (i = 0; i < iv_sz; i++) {
		link->iv[i] = rand() % 128;
	}

	rand_size = rand() % MAX_PLAIN_TXT_LEN;
	while (!rand_size) {
		aest_d("\t[Rand size is zero]\t\r\n");
		rand_size = rand() % MAX_PLAIN_TXT_LEN;
	}


	if (!(mode == HW_AES_MODE_CTR)) {
		pad_size = (16 - (rand_size % 16)) % 16;
	}

	link->len = rand_size + pad_size;
	link->plain = os_mem_alloc(link->len + 1);
	if (!link->plain) {
		return -WM_E_NOMEM;
	}
	memset(link->plain, 'C', link->len + 1);
	link->plain[link->len] = '\0';

	for (i = 0; i < rand_size; i++) {
		link->plain[i] = rand() % 128;
	}

	for (i = rand_size; i < link->len; i++) {
		link->plain[i] = 0;
	}

	link->cipher = os_mem_alloc(link->len + 1);
	if (!link->cipher) {
		return -WM_E_NOMEM;
	}
	memset(link->cipher, 'D', link->len + 1);
	link->cipher[link->len] = '\0';

	link->rec_plain = os_mem_alloc(link->len + 1);
	if (!link->rec_plain) {
		return -WM_E_NOMEM;
	}
	memset(link->rec_plain, 'E', link->len + 1);
	link->rec_plain[link->len] = '\0';

	return WM_SUCCESS;
}

static int sys_init(
		    unsigned int objCnt, int key_sz,
		    int iv_sz, unsigned int mode, list_head_t *anchor)
{
	aest_d("\t====================\r\n");
	aest_d("\t=====[Sys init]=====\r\n");
	aest_d("\t====================\r\n");

	INIT_LIST_HEAD(anchor);
	int obj_i = 0, rv;

	srand(123456);
	int tot = 0, obj_passed = 0;

	for (obj_i = 0; obj_i < objCnt; obj_i++) {
		aest_d("\n\n\t[Obj No][%d]\t\r\n", obj_i);
		sys_node_t *node_obj = NULL;
		node_obj = os_mem_alloc(sizeof(sys_node_t));
		if (!node_obj) {
			return -WM_E_NOMEM;
			aest_d("\t[Objects Required/ Created][%d/%d]\t\r\n",
			       objCnt, obj_passed);
		}
		rv = node_init(node_obj, key_sz, iv_sz, mode);
		if (rv != WM_SUCCESS) {
			return -WM_E_NOMEM;
			aest_d("\t[Objects Required/ Created][%d/%d]\t\r\n",
			       objCnt, obj_passed);
		}
		add_link(node_obj, anchor);
		obj_passed++;
		tot += node_obj->len;
	}
	aest_d("\t[Total plain-text] [%d]\t\r\n", tot);

	return WM_SUCCESS;
}
static int aes_test_init(unsigned int objCnt, int key_sz, int iv_sz, int mode)
{
	int rv = 0;
	list_head_t *anchor;
	anchor = os_mem_alloc(sizeof(list_head_t));
	if (!anchor) {
		return WM_E_NOMEM;
	}
	/* System init (Node init and adding the node to link)*/
	rv = sys_init(objCnt, key_sz, iv_sz, mode, anchor);

	/*
	  if (rv != WM_SUCCESS) {
	  aest_d("sys_init failed....\r\n");
	  free_all_links(anchor);
	  return -WM_FAIL;
	  }
	*/
	/* AES operation on list*/
	aes_on_list(anchor, mode);

	/* Verifying the list*/
	rv = verify_list(anchor);

	if (rv != WM_SUCCESS) {
		aest_d("verification failed...\r\n");
		free_all_links(anchor);
		return -WM_FAIL;
	}

	free_all_links(anchor);
	aest_d("\t[Freed all]\t\r\n");
	return rv;
}

/* Threading AES test */
#define AES_MAX_THREADS 50
#define AES_MODE_SUPPORT 3 /*  ECB (0), CBC (1), CTR (2), CCM (3) */
/*
 * Note:
 * CCM mode fails in multi-threading, hence not included here.
 */

/* Get a random mode */
static int get_rand_test_mode()
{
	return rand() % AES_MODE_SUPPORT;
}

typedef struct test_info {
	int thrd_id;
	unsigned int test_mode;
	char *test_name;
} test_info_t;

#ifdef AES_TEST_DEBUG
static void get_test_info(unsigned int rand_test)
{
	char *test_name;
	test_name = "[No test]";
	switch (rand_test) {
	case 0:
		test_name = "[AES-ECB]";
		break;
	case 1:
		test_name = "[AES-CBC]";
		break;
	case 2:
		test_name = "[AES-CTR]";
		break;
	case 3:
		test_name = "[AES-CCM]";
		break;
	}
	aest_d("[Mode_name][%s]: [Mode][%d]\r\n", test_name, rand_test);
}

#endif

os_semaphore_t sem_arr[AES_MAX_THREADS];
static os_thread_t test_thread_handle[AES_MAX_THREADS];

/* Semaphore declaration and initialization*/
static void sem_init(unsigned int thrdCnt)
{
	int sem_ctr = 0, rv;
	char name_buf[10];
	for (sem_ctr = 0; sem_ctr < thrdCnt; sem_ctr++) {
		snprintf(name_buf, sizeof(name_buf), "Sem-%d", sem_ctr);

		rv = os_semaphore_create_counting(&sem_arr[sem_ctr],
						  name_buf, 1, 0);
		if (rv != WM_SUCCESS)
			aest_d("Semaphore creation failed\r\n");
	}
}

static void os_sem_get(unsigned int thrdCnt)
{
	int sem_ctr = 0;
	for (sem_ctr = 0; sem_ctr < thrdCnt; sem_ctr++) {
		os_semaphore_get(&sem_arr[sem_ctr], OS_WAIT_FOREVER);
	}
}
static void os_sem_put(int thrd_id)
{
	os_semaphore_put(&sem_arr[thrd_id]);
}

/* Mutex declaration */

static int get_key_sz()
{
	int keys[] = {16, 24, 32};
	return keys[rand()%3];
}

static int thread_success;

static void aes_thread_func(void *arg)
{
	test_info_t *tmp;
	tmp = (test_info_t *)arg;
	int objCnt, key_sz, iv_sz, rv = 0;

	aest_d("\t[Thread_id][%d]\t\r\n", tmp->thrd_id);
#ifdef AES_TEST_DEBUG
	get_test_info(tmp->test_mode);
#endif


	objCnt = 1;  /* Only 1 object per thread */
	key_sz = get_key_sz();  /* 16, 24, 32 */
	iv_sz = 16;  /* Set to 16 */

	rv = aes_test_init(objCnt, key_sz, iv_sz, tmp->test_mode);
	if (rv != WM_SUCCESS) {
		aest_d("\t[Thread][%d] [Status][Failed]\r\n", tmp->thrd_id);
	} else {
		thread_success++;
		aest_d("\t[Thread][%d] [Status][Passed]\r\n", tmp->thrd_id);
		aest_d("\t[success][%d]\r\n", thread_success);
	}
	aest_d("\r\n---------------------------\r\n\n");

	os_sem_put(tmp->thrd_id);
	os_thread_self_complete(NULL);
}

static os_thread_stack_define(test_thread_stack, 1024);

static int threading(unsigned int thrdCnt)
{
	if (thrdCnt > AES_MAX_THREADS) {
		aest_d("Cannot create threads more than [%d]\r\n",
		       AES_MAX_THREADS);
		thrdCnt = AES_MAX_THREADS;
		aest_d("Creating MAX threads [%d]\r\n", thrdCnt);
	}
	sem_init(thrdCnt);

	int thrd_ctr = 0, max_thrd_prio = 5;
	/* Max thread priority set by max_thrd_prio */
	char test_mode_name[10];

	for (thrd_ctr = 0; thrd_ctr < thrdCnt; thrd_ctr++) {
		snprintf(test_mode_name,
			 sizeof(test_mode_name), "Thread-%d", thrd_ctr);

		test_info_t *tmp = os_mem_alloc(sizeof(test_info_t));
		memset(tmp, 0, sizeof(tmp));
		tmp->thrd_id = thrd_ctr;
		tmp->test_mode = get_rand_test_mode();

		os_thread_create(
				 &test_thread_handle[thrd_ctr],
				 test_mode_name,
				 aes_thread_func,
				 (void *) tmp,
				 &test_thread_stack,
				 thrd_ctr % max_thrd_prio);
	}
	os_sem_get(thrdCnt);
	aest_d("All threads finished\r\n");
	if (thread_success == thrdCnt) {
		return WM_SUCCESS;
	} else {
		return -WM_FAIL;
	}
}

/* |case 01-04|
   Key:		16 bytes
   IV:			16 bytes
   Plain:		16 bytes
   Cipher:		16 bytes
   Rec Plain:	16 bytes
*/
/* ----------------------------------------------------- */
/*
 * Test Case:  aes_test_case_01
 * Test Type: Unit
 * Test Description: A test case that tests AES in ECB mode
 *		(configuration stated above)
 * Expected Result: Decryption of cipher-text (rec_plain) should match with
 *		    input plain-text (plain). But cipher-text
 *                  (cipher) should
 *		    not match with input plain-text (plain) "completely".
 */
/* ----------------------------------------------------- */
/* case 01*/
static int aes_test_case_01()
{
	int rv = 0;
	aest_d("[AES-EBC]\r\n");
	rv = aes_test_common(HW_AES_MODE_ECB);
	return rv;
}

/* ----------------------------------------------------- */
/*
 * Test Case:  aes_test_case_02
 * Test Type: Unit
 * Test Description: A test case that tests AES in CBC mode
 *		(configuration stated above)
 * Expected Result: Decryption of cipher-text (rec_plain) should match with
 *		input plain-text (plain). But cipher-text (cipher) should
 *		not match with input plain-text (plain) "completely".
 *
 */
/* case 02*/
static int aes_test_case_02()
{
	int rv = 0;
	aest_d("[AES-CBC]\r\n");
	rv = aes_test_common(HW_AES_MODE_CBC);
	return rv;
}

/* ----------------------------------------------------- */
/*
 * Test Case:  aes_test_case_03
 * Test Type: Unit
 * Test Description: A test case that tests AES in CTR mode
 *		(configuration stated above)
 * Expected Result: Decryption of cipher-text (rec_plain) should match with
 *		input plain-text (plain). But cipher-text (cipher) should
 *		not match with input plain-text (plain) "completely".
 *
 */
/* case 03*/
static int aes_test_case_03()
{
	int rv = 0;
	aest_d("[AES-CTR]\r\n");
	rv = aes_test_common(HW_AES_MODE_CTR);
	return rv;
}

/* ----------------------------------------------------- */
/*
 * Test Case:  aes_test_case_04
 * Test Type: Unit
 * Test Description: A test case that tests AES in CCM mode
 *		(configuration stated above)
 * Expected Result: Decryption of cipher-text (rec_plain) should match with
 *		input plain-text (plain). But cipher-text (cipher) should
 *		not match with input plain-text (plain) "completely".
 *
 */
/* case 04*/
static int aes_test_case_04()
{
	int rv = 0;
	aest_d("[AES-CCM]\r\n");
	rv = aes_test_common(HW_AES_MODE_CCM);
	return rv;
}

/* |case 05-08|
   Key:		16 bytes
   IV:			16 bytes
   Plain:		Variable size

   Multiple objects possbile
   objCnt:		1 (Default)
*/

/* ----------------------------------------------------- */
/*
 * Test Case:  aes_test_case_05
 * Test Type: Functional
 * Test Description: A test case that tests AES in ECB mode
 *					(configuration stated above)
 * Expected Result: For each created object, decryption of
 *		cipher-text (rec_plain) should match with
 *		input plain-text (plain). But cipher-text (cipher) should
 *		not match with input plain-text (plain) "completely".
 *
 */
/* case 05*/
static int aes_test_case_05(unsigned int objCnt)
{
	int rv = 0;
	aest_d("[AES-ECB]\r\n");
	rv = aes_test_init(objCnt, 16, 16, HW_AES_MODE_ECB);
	return rv;
}

/* ----------------------------------------------------- */
/*
 * Test Case:  aes_test_case_06
 * Test Type: Functional
 * Test Description: A test case that tests AES in CBC mode
 *					(configuration stated above)
 * Expected Result: For each created object, decryption of
 *		cipher-text (rec_plain) should match with
 *		input plain-text (plain). But cipher-text (cipher) should
 *		not match with input plain-text (plain) "completely".
 *
 */
/* case 06*/
static int aes_test_case_06(unsigned int objCnt)
{
	int rv = 0;
	aest_d("[AES-CBC]\r\n");
	rv = aes_test_init(objCnt, 16, 16, HW_AES_MODE_CBC);
	return rv;
}

/* ----------------------------------------------------- */
/*
 * Test Case:  aes_test_case_07
 * Test Type: Functional
 * Test Description: A test case that tests AES in CTR mode
 *					(configuration stated above)
 * Expected Result: For each created object, decryption of
 *		cipher-text (rec_plain) should match with
 *		input plain-text (plain). But cipher-text (cipher) should
 *		not match with input plain-text (plain) "completely".
 *
 */
/* case 07*/
static int aes_test_case_07(unsigned int objCnt)
{
	int rv = 0;
	aest_d("[AES-CTR]\r\n");
	rv = aes_test_init(objCnt, 16, 16, HW_AES_MODE_CTR);
	return rv;
}

/* ----------------------------------------------------- */
/*
 * Test Case:  aes_test_case_08
 * Test Type: Functional
 * Test Description: A test case that tests AES in CCM mode
 *					(configuration stated above)
 * Expected Result: For each created object, decryption of
 *		cipher-text (rec_plain) should match with
 *		input plain-text (plain). But cipher-text (cipher) should
 *		not match with input plain-text (plain) "completely".
 *
 */
/* case 08*/
static int aes_test_case_08(unsigned int objCnt)
{
	int rv = 0;
	aest_d("[AES-CCM]\r\n");
	rv = aes_test_init(objCnt, 16, 16, HW_AES_MODE_CCM);
	return rv;
}

/* |case 09-12|
   Key:		24 bytes
   IV:			16 bytes
   Plain:		Variable size

   Multiple objects possbile
   objCnt:		1 (Default)
*/
/* ----------------------------------------------------- */
/*
 * Test Case:  aes_test_case_09
 * Test Type: Functional
 * Test Description: A test case that tests AES in ECB mode
 *					(configuration stated above)
 * Expected Result: For each created object, decryption of
 *		cipher-text (rec_plain) should match with
 *		input plain-text (plain). But cipher-text (cipher) should
 *		not match with input plain-text (plain) "completely".
 *
 */
/* case 09*/
static int aes_test_case_09(unsigned int objCnt)
{
	int rv = 0;
	aest_d("[AES-ECB]\r\n");
	rv = aes_test_init(objCnt, 24, 16, HW_AES_MODE_ECB);
	return rv;
}

/* ----------------------------------------------------- */
/*
 * Test Case:  aes_test_case_10
 * Test Type: Functional
 * Test Description: A test case that tests AES in CBC mode
 *					(configuration stated above)
 * Expected Result: For each created object, decryption of
 *		cipher-text (rec_plain) should match with
 *		input plain-text (plain). But cipher-text (cipher) should
 *		not match with input plain-text (plain) "completely".
 *
 */
/* case 10*/
static int aes_test_case_10(unsigned int objCnt)
{
	int rv = 0;
	aest_d("[AES-CBC]\r\n");
	rv = aes_test_init(objCnt, 24, 16, HW_AES_MODE_CBC);
	return rv;
}

/* ----------------------------------------------------- */
/*
 * Test Case:  aes_test_case_11
 * Test Type: Functional
 * Test Description: A test case that tests AES in CTR mode
 *					(configuration stated above)
 * Expected Result: For each created object, decryption of
 *		cipher-text (rec_plain) should match with
 *		input plain-text (plain). But cipher-text (cipher) should
 *		not match with input plain-text (plain) "completely".
 *
 */
/* case 11*/
static int aes_test_case_11(unsigned int objCnt)
{
	int rv = 0;
	aest_d("[AES-CTR]\r\n");
	rv = aes_test_init(objCnt, 24, 16, HW_AES_MODE_CTR);
	return rv;
}

/* ----------------------------------------------------- */
/*
 * Test Case:  aes_test_case_12
 * Test Type: Functional
 * Test Description: A test case that tests AES in CCM mode
 *					(configuration stated above)
 * Expected Result: For each created object, decryption of
 *		cipher-text (rec_plain) should match with
 *		input plain-text (plain). But cipher-text (cipher) should
 *		not match with input plain-text (plain) "completely".
 *
 */
/* case 12*/
static int aes_test_case_12(unsigned int objCnt)
{
	int rv = 0;
	aest_d("[AES-CCM]\r\n");
	rv = aes_test_init(objCnt, 24, 16, HW_AES_MODE_CCM);
	return rv;
}

/* |case 13-16|
   Key:		32 bytes
   IV:			16 bytes
   Plain:		Variable size

   Multiple objects possbile
   objCnt:		1 (Default)
*/
/* ----------------------------------------------------- */
/*
 * Test Case:  aes_test_case_13
 * Test Type: Functional
 * Test Description: A test case that tests AES in ECB mode
 *					(configuration stated above)
 * Expected Result: For each created object, decryption of
 *		cipher-text (rec_plain) should match with
 *		input plain-text (plain). But cipher-text (cipher) should
 *		not match with input plain-text (plain) "completely".
 *
 */
/* case 13*/
static int aes_test_case_13(unsigned int objCnt)
{
	int rv = 0;
	aest_d("[AES-ECB]\r\n");
	rv = aes_test_init(objCnt, 32, 16, HW_AES_MODE_ECB);
	return rv;
}

/* ----------------------------------------------------- */
/*
 * Test Case:  aes_test_case_14
 * Test Type: Functional
 * Test Description: A test case that tests AES in CBC mode
 *					(configuration stated above)
 * Expected Result: For each created object, decryption of
 *		cipher-text (rec_plain) should match with
 *		input plain-text (plain). But cipher-text (cipher) should
 *		not match with input plain-text (plain) "completely".
 *
 */
/* case 14*/
static int aes_test_case_14(unsigned int objCnt)
{
	int rv = 0;
	aest_d("[AES-CBC]\r\n");
	rv = aes_test_init(objCnt, 32, 16, HW_AES_MODE_CBC);
	return rv;
}

/* ----------------------------------------------------- */
/*
 * Test Case:  aes_test_case_15
 * Test Type: Functional
 * Test Description: A test case that tests AES in CTR mode
 *					(configuration stated above)
 * Expected Result: For each created object, decryption of
 *		cipher-text (rec_plain) should match with
 *		input plain-text (plain). But cipher-text (cipher) should
 *		not match with input plain-text (plain) "completely".
 *
 */
/* case 15*/
static int aes_test_case_15(unsigned int objCnt)
{
	int rv = 0;
	aest_d("[AES-CTR]\r\n");
	rv = aes_test_init(objCnt, 32, 16, HW_AES_MODE_CTR);
	return rv;
}

/* ----------------------------------------------------- */
/*
 * Test Case:  aes_test_case_16
 * Test Type: Functional
 * Test Description: A test case that tests AES in CCM mode
 *					(configuration stated above)
 * Expected Result: For each created object, decryption of
 *		cipher-text (rec_plain) should match with
 *		input plain-text (plain). But cipher-text (cipher) should
 *		not match with input plain-text (plain) "completely".
 *
 */
/* case 16*/
static int aes_test_case_16(unsigned int objCnt)
{
	int rv = 0;
	aest_d("[AES-CCM]\r\n");
	rv = aes_test_init(objCnt, 32, 16, HW_AES_MODE_CCM);
	return rv;
}

/* |case 17|
   Key:		Variable size (16, 24, 32)
   IV:			16 bytes
   Plain:		Variable size

   Multiple threads possbile
   thrdCnt:		1 (Default)
   objCnt:			1 (Default) per thread
*/
/* ----------------------------------------------------- */
/*
 * Test Case:  aes_test_case_17
 * Test Type: Stress, Threading
 * Test Description: A test case that tests AES for multiple threads. Each
 *                thread performs AES operation for a random configuration.
 *		(configuration stated above)
 * Expected Result: For each created thread, decryption of
 *		cipher-text (rec_plain) should match with
 *		input plain-text (plain). But cipher-text (cipher) should
 *		not match with input plain-text (plain) "completely".
 *
 */
/* case 17*/
static int aes_test_case_17(unsigned int thrdCnt)
{
	int rv = 0;
	thread_success = 0;
	aest_d("[AES-Multithreading]\r\n");
	rv = threading(thrdCnt);
	return rv;
}

/* test case functions*/
static void run_test(int option, unsigned int objCnt)
{
	int rv = 0;
	wmprintf("\r\n\tRunning test number: %d \r\n", option);
	aest_d("\r\n=====[Start]=====\r\n");
	switch (option) {
	case 1:
		rv = aes_test_case_01();
		print_test_status(rv, "aes_test_case_01");
		break;
	case 2:
		rv = aes_test_case_02();
		print_test_status(rv, "aes_test_case_02");
		break;
	case 3:
		rv = aes_test_case_03();
		print_test_status(rv, "aes_test_case_03");
		break;
	case 4:
		rv = aes_test_case_04();
		print_test_status(rv, "aes_test_case_04");
		break;
	case 5:
		rv = aes_test_case_05(objCnt);
		print_test_status(rv, "aes_test_case_05");
		break;
	case 6:
		rv = aes_test_case_06(objCnt);
		print_test_status(rv, "aes_test_case_06");
		break;
	case 7:
		rv = aes_test_case_07(objCnt);
		print_test_status(rv, "aes_test_case_07");
		break;
	case 8:
		rv = aes_test_case_08(objCnt);
		print_test_status(rv, "aes_test_case_08");
		break;
	case 9:
		rv = aes_test_case_09(objCnt);
		print_test_status(rv, "aes_test_case_09");
		break;
	case 10:
		rv = aes_test_case_10(objCnt);
		print_test_status(rv, "aes_test_case_10");
		break;
	case 11:
		rv = aes_test_case_11(objCnt);
		print_test_status(rv, "aes_test_case_11");
		break;
	case 12:
		rv = aes_test_case_12(objCnt);
		print_test_status(rv, "aes_test_case_12");
		break;
	case 13:
		rv = aes_test_case_13(objCnt);
		print_test_status(rv, "aes_test_case_13");
		break;
	case 14:
		rv = aes_test_case_14(objCnt);
		print_test_status(rv, "aes_test_case_14");
		break;
	case 15:
		rv = aes_test_case_15(objCnt);
		print_test_status(rv, "aes_test_case_15");
		break;
	case 16:
		rv = aes_test_case_16(objCnt);
		print_test_status(rv, "aes_test_case_16");
		break;
	case 17:
		rv = aes_test_case_17(objCnt);
		print_test_status(rv, "aes_test_case_17");
		break;
	default:
		break;
	}
	aest_d("=====[End]=====\r\n");
}

/* test_operations */
#define MAX_TEST_CNT 17
static void test_operations(int option, unsigned int objCnt)
{
	unsigned int count = 0;

	if (option == 0) {
		for (count = MAX_TEST_CNT; count > 0; count--) {
			run_test(count, objCnt);
		}
	} else if (option > MAX_TEST_CNT) {
		wmprintf("Input exceeds max test count [%d]\r\n", MAX_TEST_CNT);
	} else {
		run_test(option, objCnt);
	}
}

/* Print usage function */
static void print_usage()
{
	wmprintf("Usage : \r\n");
	wmprintf("aes-test [test_no] \r\n");
	wmprintf("         if test_no = 0 all test"
		 "cases will be executed\r\n");
}

/* test_function */
static void aes_test(int argc, char *argv[])
{
	uint32_t test_no = 0;
	if (argc < 2) {
		print_usage();
		return;
	}
	test_no = atoi(argv[1]);
	unsigned int objCnt = 10;  /* 10 Objects (default) */
	if (test_no > 4) {
		if (atoi(argv[2]))
			objCnt = atoi(argv[2]);
	}
	test_operations(test_no, objCnt);
	/* Note: Flow is different from the psm-test file */
}

static void aes_disp()
{
	wmprintf("This is AES test suite\r\n");
}

/* cli_commands declaration*/
static struct cli_command commands[] = {
	{"aes-test", "[test_no]", aes_test},
	{"aes", NULL, aes_disp}
};

/* cli_init function (registration of cli)*/
int aes_test_cli_init(void)
{
	int i;
	for (i = 0; i < sizeof(commands) /
		     sizeof(struct cli_command); i++)
		if (cli_register_command(&commands[i]))
			return 1;
	return 0;
}
