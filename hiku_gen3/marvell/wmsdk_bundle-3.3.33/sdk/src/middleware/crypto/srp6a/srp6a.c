/*
 * Secure Remote Password 6a implementation
 * Copyright (c) 2010 Tom Cocagne. All rights reserved.
 * http://csrp.googlecode.com/p/csrp/
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the Google Code nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL TOM COCAGNE BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <wmtime.h>
#include <wm_os.h>
#include <wmstdio.h>
#include <wmlog.h>
#include <wm-tls.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <wmcrypto.h>
#include <bn.h>

#include <hkdf-sha.h>

#include "srp6a.h"
#include "srp6a_util.h"

extern void hexdump(void *mem, unsigned int len);

static int g_initialized = 0;

typedef struct
{
	BIGNUM     * N;
	BIGNUM     * g;
} NGConstant;

struct NGHex
{
	const char * n_hex;
	const char * g_hex;
};

/* All constants here were pulled from Appendix A of RFC 5054 */
static struct NGHex global_Ng_constants[] = {
	{ /* 1024 */
		"E",
		"2"
	},
	{ /* 2048 */
		"A",
		"2"
	},
	{ /* 3072 */
		"FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD129024E088A67CC74020BBEA63B"
		"139B22514A08798E3404DDEF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245E485B576625E"
		"7EC6F44C42E9A637ED6B0BFF5CB6F406B7EDEE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B"
		"3DC2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F83655D23DCA3AD961C62F356208552BB"
		"9ED529077096966D670C354E4ABC9804F1746C08CA18217C32905E462E36CE3BE39E772C180E86039B"
		"2783A2EC07A28FB5C55DF06F4C52C9DE2BCBF6955817183995497CEA956AE515D2261898FA05101572"
		"8E5A8AAAC42DAD33170D04507A33A85521ABDF1CBA64ECFB850458DBEF0A8AEA71575D060C7DB3970F"
		"85A6E1E4C7ABF5AE8CDB0933D71E8C94E04A25619DCEE3D2261AD2EE6BF12FFA06D98A0864D8760273"
		"3EC86A64521F2B18177B200CBBE117577A615D6C770988C0BAD946E208E24FA074E5AB3143DB5BFCE0"
		"FD108E4B82D120A93AD2CAFFFFFFFFFFFFFFFF",
		"5"
	},
	{ /* 4096 */
		"F",
		"5"
	},
	{ /* 8192 */
		"F",
		"13"
	},
	{0,0} /* null sentinel */
};

static NGConstant * new_ng( SRP_NGType ng_type, const char * n_hex, const char * g_hex )
{
	ENTER();

	NGConstant * ng   = (NGConstant *) os_mem_alloc( sizeof(NGConstant) );

	if (ng == NULL)
		return NULL;

	ng->N             = BN_new();
	ng->g             = BN_new();

	if ( ng_type != SRP_NG_CUSTOM )
	{
		n_hex = global_Ng_constants[ ng_type ].n_hex;
		g_hex = global_Ng_constants[ ng_type ].g_hex;
	}

	BN_hex2bn( &ng->N, n_hex );
	BN_hex2bn( &ng->g, g_hex );

	LEAVE();
	return ng;
}

static void delete_ng( NGConstant * ng )
{
	ENTER();

	BN_free( ng->N );
	BN_free( ng->g );
	ng->N = 0;
	ng->g = 0;
	os_mem_free(ng);

	LEAVE();
}

typedef union
{
	SHA512Context	sha512;
} HashCTX;

struct SRPVerifier
{
	SRP_HashAlgorithm  hash_alg;
	NGConstant        *ng;

	const char          * username;
	const unsigned char * bytes_B;
	int		      len_B;
	const unsigned char * bytes_b;
	int		      len_b;
	int                   authenticated;

	unsigned char M[SHA512HashSize];
	unsigned char H_AMK[SHA512HashSize];
	unsigned char session_key[SHA512HashSize];
};

static void hash_init( SRP_HashAlgorithm alg, HashCTX *c )
{
	ENTER();

	SHA512Reset(&c->sha512);

	LEAVE();
}
static void hash_update( SRP_HashAlgorithm alg, HashCTX *c, const void *data, size_t len )
{
	ENTER();

	SHA512Input(&c->sha512, data, len);

	LEAVE();
}
static int hash_final(SRP_HashAlgorithm alg, HashCTX *c, unsigned char *md)
{
	ENTER();

	return SHA512Result(&c->sha512, md);

	LEAVE();
}

static int SHA512_vector(const unsigned char *d, size_t n, unsigned char *md)
{
	SHA512Context     sha;

	SHA512Reset(&sha);
	SHA512Input(&sha, d, n);
	SHA512Result(&sha, md);

	return 0;
}

static int hash(SRP_HashAlgorithm alg, const unsigned char *d, size_t n,
			unsigned char *md)
{
	ENTER();

	return SHA512_vector(d, n, md);

	LEAVE();
}
static int hash_length( SRP_HashAlgorithm alg )
{
	ENTER();

	return SHA512HashSize;

	LEAVE();
}

void dump_bn(const char *header, const BIGNUM *b)
{
#ifdef CRYPTO_HEX_DEBUG_ENABLE
	wmlog("crypto", "-- x --  -- x -- -- x --\r\n");
	int len = BN_num_bytes(b);
	wmlog("crypto", "Dumping: %s %d\r\n", header, len);

	char *str = os_mem_alloc(len);
	if (!str) {
		wmlog("crypto", "Debug Alloc failed\r\n");
		return;
	}
	BN_bn2bin(b, (unsigned char *)str);
	hexdump(str, len);
	wmlog("crypto", "-- x --  -- x -- -- x --\r\n");
	os_mem_free(str);
#endif
}


static BIGNUM * H_nn( SRP_HashAlgorithm alg, const BIGNUM * n1, const BIGNUM * n2 )
{
	ENTER();

	unsigned char   buff[SHA512HashSize];
	int             len_n1 = BN_num_bytes(n1);
	int             len_n2 = BN_num_bytes(n2);
	int             nbytes = 2 * len_n1;
	unsigned char * bin    = (unsigned char *) os_mem_alloc( nbytes );
	unsigned char * pad_buf = (unsigned char *) os_mem_alloc(len_n1);

        if (bin == NULL)
		return NULL;

	BN_bn2bin(n1, bin);

	if (len_n2 == 1) {
		memset(pad_buf, 0x00, len_n1);
		memcpy(bin + len_n1, pad_buf, len_n1 - 1);
		BN_bn2bin(n2, bin + 2 * len_n1 - 1);
	} else {
		BN_bn2bin(n2, bin + len_n1);
	}

	hash( alg, bin, nbytes, buff );
	os_mem_free(pad_buf);
	os_mem_free(bin);
	LEAVE();
	return BN_bin2bn(buff, hash_length(alg), NULL);
}

static BIGNUM * H_ns( SRP_HashAlgorithm alg, const BIGNUM * n, const unsigned char * bytes, int len_bytes )
{
	ENTER();

	unsigned char   buff[SHA512HashSize];
	int             len_n  = BN_num_bytes(n);
	int             nbytes = len_n + len_bytes;
	unsigned char * bin    = (unsigned char *) os_mem_alloc( nbytes );

	if (bin == NULL)
		return NULL;

	BN_bn2bin(n, bin);
	memcpy( bin + len_n, bytes, len_bytes );
	hash( alg, bin, nbytes, buff );
	os_mem_free(bin);

	LEAVE();
	return BN_bin2bn(buff, hash_length(alg), NULL);
}

static BIGNUM * calculate_x( SRP_HashAlgorithm alg, const BIGNUM * salt, const char * username, const unsigned char * password, int password_len )
{
	ENTER();

	unsigned char ucp_hash[SHA512HashSize];
	HashCTX       ctx;

	hash_init( alg, &ctx );

	hash_update( alg, &ctx, username, strlen(username) );
	hash_update( alg, &ctx, ":", 1 );
	hash_update( alg, &ctx, password, password_len );

	hash_final( alg, &ctx, ucp_hash );

	LEAVE();
	return H_ns( alg, salt, ucp_hash, hash_length(alg) );
}

static int update_hash_n( SRP_HashAlgorithm alg, HashCTX *ctx, const BIGNUM * n )
{
	ENTER();

	unsigned long len = BN_num_bytes(n);
	unsigned char * n_bytes = (unsigned char *) os_mem_alloc( len );

	if (n_bytes == NULL)
		return -1;

	BN_bn2bin(n, n_bytes);
	hash_update(alg, ctx, n_bytes, len);
	os_mem_free(n_bytes);

	return 0;

	LEAVE();
}

static int hash_num( SRP_HashAlgorithm alg, const BIGNUM * n, unsigned char * dest )
{
	ENTER();

	int             nbytes = BN_num_bytes(n);
	unsigned char * bin    = (unsigned char *) os_mem_alloc( nbytes );

	if (bin == NULL)
		return -1;

	BN_bn2bin(n, bin);
	hash( alg, bin, nbytes, dest );
	os_mem_free(bin);

	return 0;

	LEAVE();
}

static int calculate_M( SRP_HashAlgorithm alg, NGConstant *ng, unsigned char * dest, const char * I, const BIGNUM * s,
		const BIGNUM * A, const BIGNUM * B, const unsigned char * K )
{
	ENTER();

	unsigned char H_N[SHA512HashSize];
	unsigned char H_g[SHA512HashSize];
	unsigned char H_I[SHA512HashSize];
	unsigned char H_xor[SHA512HashSize];
	HashCTX       ctx;
	int           i = 0;
	int           hash_len = hash_length(alg);

	if (hash_num( alg, ng->N, H_N ) < 0)
		return -1;
	if (hash_num( alg, ng->g, H_g ) < 0)
		return -1;

	hash(alg, (const unsigned char *)I, strlen(I), H_I);


	for (i=0; i < hash_len; i++ )
		H_xor[i] = H_N[i] ^ H_g[i];

	hash_init( alg, &ctx );

	hash_update( alg, &ctx, H_xor, hash_len );
	hash_update( alg, &ctx, H_I,   hash_len );
	if (update_hash_n( alg, &ctx, s ) < 0)
		return -1;

	if (update_hash_n( alg, &ctx, A ) < 0)
		return -1;

	if (update_hash_n( alg, &ctx, B ) < 0)
		return -1;
	hash_update(alg, &ctx, K, hash_len);

	hash_final( alg, &ctx, dest );

	return 0;

	LEAVE();
}

static int calculate_H_AMK( SRP_HashAlgorithm alg, unsigned char *dest, const BIGNUM * A, const unsigned char * M, const unsigned char * K )
{
	ENTER();

	HashCTX ctx;

	hash_init( alg, &ctx );

	if (update_hash_n( alg, &ctx, A ) < 0)
		return -1;

	hash_update( alg, &ctx, M, hash_length(alg) );
	hash_update(alg, &ctx, K, hash_length(alg));

	hash_final( alg, &ctx, dest );

	return 0;

	LEAVE();
}


static void init_random()
{
	ENTER();

	if (g_initialized)
		return;

	g_initialized = 1;

	unsigned char buff[32];

	get_random_sequence( buff, sizeof(buff) );

	//RAND_seed( buff, sizeof(buff) );

	LEAVE();
}


/***********************************************************************************************************
 *
 *  Exported Functions
 *
 ***********************************************************************************************************/

void srp_random_seed( unsigned char * random_data, int data_length )
{
	ENTER();

	g_initialized = 1;

	if (random_data)
		get_random_sequence( random_data, data_length );
	//RAND_seed( random_data, data_length );

	LEAVE();
}


int srp_create_salted_verification_key( SRP_HashAlgorithm alg,
		SRP_NGType ng_type, const char * username,
		const unsigned char * password, int len_password,
		const unsigned char ** bytes_s, int * len_s,
		const unsigned char ** bytes_v, int * len_v,
		const char *n_hex, const char *g_hex, int salt_len)
{
	ENTER();

	int ret = -1;

	BIGNUM     * s   = BN_new();
	BIGNUM     * v   = BN_new();
	BIGNUM     * x   = 0;
	BN_CTX     * ctx = BN_CTX_new();
	NGConstant * ng  = new_ng( ng_type, n_hex, g_hex );

	if (s == NULL || v == NULL || ctx == NULL || ng == NULL)
		goto cleanup_and_exit;

	init_random(); /* Only happens once */

	BN_rand(s, 8 * salt_len, -1, 0);
#if 0
	/* Kept around for testing */
	unsigned char ks_salt[] = { 0xcd, 0x2a, 0xf6, 0xd6, 0x68, 0x74, 0x8d,
			   0xcc, 0x4a, 0xcd, 0x23, 0xb1, 0x13, 0x8d, 0xfd,
			   0xb3 };
	BN_bin2bn(ks_salt, sizeof(ks_salt), s);
#endif
	dump_bn("s", s);
	x = calculate_x( alg, s, username, password, len_password );
	dump_bn("x", x);
	if (x == NULL)
		goto cleanup_and_exit;

	BN_mod_exp(v, ng->g, x, ng->N, ctx);
	dump_bn("v", v);
	*len_s   = BN_num_bytes(s);
	*len_v   = BN_num_bytes(v);

	*bytes_s = (const unsigned char *) os_mem_alloc( *len_s );
	*bytes_v = (const unsigned char *) os_mem_alloc( *len_v );

	if (*bytes_s == NULL || *bytes_v == NULL)
		goto cleanup_and_exit;

	BN_bn2bin(s, (unsigned char *) *bytes_s);
	BN_bn2bin(v, (unsigned char *) *bytes_v);

	ret = 0;

cleanup_and_exit:
	delete_ng( ng );
	BN_free(s);
	BN_free(v);
	BN_free(x);
	BN_CTX_free(ctx);

	LEAVE();

	return ret;
}



/* Out: bytes_B, len_B.
 *
 * On failure, bytes_B will be set to NULL and len_B will be set to 0
 */
struct SRPVerifier *  srp_verifier_new( SRP_HashAlgorithm alg, SRP_NGType ng_type, const char * username,
		const unsigned char * bytes_s, int len_s,
		const unsigned char * bytes_v, int len_v,
		const unsigned char * bytes_A, int len_A,
		const unsigned char ** bytes_B, int * len_B,
		const char * n_hex, const char * g_hex )
{
	ENTER();

	BIGNUM     *v    = BN_bin2bn(bytes_v, len_v, NULL);
	BIGNUM     *B    = BN_new();
	BIGNUM     *b    = BN_new();
	BIGNUM     *k    = 0;
	BIGNUM     *tmp1 = BN_new();
	BIGNUM     *tmp2 = BN_new();
	BIGNUM     *tmp3 = BN_new();
	BN_CTX     *ctx  = BN_CTX_new();
	int         ulen = strlen(username) + 1;
	NGConstant *ng   = new_ng( ng_type, n_hex, g_hex );
	const unsigned char *bytes_b;
	int len_b;

	struct SRPVerifier * ver = (struct SRPVerifier *) os_mem_alloc( sizeof(struct SRPVerifier) );

	if (B == NULL || b == NULL || tmp1 == NULL || ctx == NULL || ng == NULL || ver == NULL)
		goto failed;

	memset(ver, 0x00, sizeof(struct SRPVerifier));

	init_random(); /* Only happens once */

	ver->username = (char *) os_mem_alloc( ulen );

	if (ver->username == NULL) {
		os_mem_free(ver);
		ver = 0;
		*len_B = 0;
		goto failed;
	}

	ver->hash_alg = alg;
	ver->ng       = ng;

	memcpy( (char*)ver->username, username, ulen );

	ver->authenticated = 0;

	BN_rand(b, 256, -1, 0);
#if 0
	/* Kept around for testing */
	unsigned char ks_b[] = { 0xcd, 0xaa, 0x76, 0x56, 0xe8, 0xf4, 0x0d,
			0x4c, 0xca,
			0x4d, 0xa3, 0x31, 0x93, 0x0d, 0x7d, 0x33,
			0x03, 0xca, 0x14, 0x3a,
			0x49, 0x1b, 0xad, 0x7b, 0x2a, 0x77, 0x96,
			0x2a, 0xca, 0x3b, 0xfe,
			0x4d};
	BN_bin2bn(ks_b, sizeof(ks_b), b);
#endif
	dump_bn("b", b);

	k = H_nn(alg, ng->N, ng->g);

	dump_bn("k", k);
	dump_bn("N", ng->N);
	dump_bn("g", ng->g);

	if (k == NULL)
		return NULL;

	/* B = kv + g^b */
	BN_mod_mul(tmp1, k, v, ng->N, ctx);
	BN_mod_exp(tmp2, ng->g, b, ng->N, ctx);
	BN_add(tmp3, tmp1, tmp2);

	BN_mod(B, tmp3, ng->N, ctx);
	dump_bn("B", B);

	*len_B   = BN_num_bytes(B);
	*bytes_B = os_mem_alloc(*len_B);

	if (*bytes_B == NULL) {
		os_mem_free((void *) ver->username);
		os_mem_free(ver);
		ver = 0;
		*len_B = 0;
		goto failed;
	}

	BN_bn2bin(B, (unsigned char *) *bytes_B);

	ver->bytes_B = *bytes_B;
	ver->len_B = *len_B;

	len_b   = BN_num_bytes(b);
	bytes_b = os_mem_alloc(len_b);

	if (bytes_b == NULL) {
		os_mem_free((void *) *bytes_B);
		os_mem_free((void *) ver->username);
		os_mem_free(ver);
		ver = 0;
		*len_B = 0;
		goto failed;
	}

	BN_bn2bin(b, (unsigned char *) bytes_b);

	ver->bytes_b = bytes_b;
	ver->len_b = len_b;
failed:
	BN_free(v);
	if (k) BN_free(k);
	BN_free(B);
	BN_free(b);
	BN_free(tmp1);
	BN_free(tmp2);
	BN_free(tmp3);
	BN_CTX_free(ctx);

	LEAVE();
	return ver;
}

/* Out: 0.
 *
 * On failure, -1
 */
int srp_verifier_validate( struct SRPVerifier * ver,
		SRP_HashAlgorithm alg, SRP_NGType ng_type, const char * username,
		const unsigned char * bytes_s, int len_s,
		const unsigned char * bytes_v, int len_v,
		const unsigned char * bytes_A, int len_A,
		const char * n_hex, const char * g_hex )
{
	ENTER();

	int ret = -1;

	const unsigned char *bytes_B = ver->bytes_B;
	int len_B = ver->len_B;
	const unsigned char *bytes_b = ver->bytes_b;
	int len_b = ver->len_b;
	const unsigned char *bytes_S = NULL;
	int len_S;
	BIGNUM     *s    = BN_bin2bn(bytes_s, len_s, NULL);
	BIGNUM     *v    = BN_bin2bn(bytes_v, len_v, NULL);
	BIGNUM     *A    = BN_bin2bn(bytes_A, len_A, NULL);
	BIGNUM     *u    = 0;
	BIGNUM     *B    = BN_bin2bn(bytes_B, len_B, NULL);
	BIGNUM     *S    = BN_new();
	BIGNUM     *b    = BN_bin2bn(bytes_b, len_b, NULL);
	BIGNUM     *k    = 0;
	BIGNUM     *tmp1 = BN_new();
	BIGNUM     *tmp2 = BN_new();
	BN_CTX     *ctx  = BN_CTX_new();
	NGConstant *ng   = ver->ng;

	if (ver == NULL || s == NULL || v == NULL || A == NULL || B == NULL || S == NULL ||
		b == NULL || tmp1 == NULL || tmp2 == NULL || ctx == NULL || ng == NULL)
		goto cleanup_and_exit;

	/* SRP-6a safety check */
	BN_mod(tmp1, A, ng->N, ctx);
	if ( !BN_is_zero(tmp1) )
	{
		u = H_nn(alg, A, B);

		if (u == NULL)
			goto cleanup_and_exit;

		dump_bn("u", u);

		/* S = (A *(v^u)) ^ b */
		BN_mod_exp(tmp1, v, u, ng->N, ctx);
		BN_mod_mul(tmp2, A, tmp1, ng->N, ctx);
		BN_mod_exp(S, tmp2, b, ng->N, ctx);

		dump_bn("S", S);

		len_S   = BN_num_bytes(S);
		bytes_S = os_mem_alloc(len_S);

		if (bytes_S == NULL)
			goto cleanup_and_exit;

		BN_bn2bin(S, (unsigned char *) bytes_S);

		hash_num(alg, S, ver->session_key);

		hexdump(ver->session_key, sizeof(ver->session_key));

		if (calculate_M( alg, ng, ver->M, username, s, A, B, ver->session_key ) < 0)
			goto cleanup_and_exit;

		if (calculate_H_AMK( alg, ver->H_AMK, A, ver->M, ver->session_key ) < 0)
			goto cleanup_and_exit;

		ret = 0;
	}

cleanup_and_exit:
	BN_free(s);
	BN_free(v);
	BN_free(A);
	if (u) BN_free(u);
	if (k) BN_free(k);
	BN_free(B);
	BN_free(S);
	BN_free(b);
	BN_free(tmp1);
	BN_free(tmp2);
	BN_CTX_free(ctx);
	if (bytes_S)
		os_mem_free((void *)bytes_S);

	LEAVE();
	return ret;
}

void srp_verifier_delete( struct SRPVerifier * ver )
{
	ENTER();

	if (ver) {
		if (ver->ng)
			delete_ng(ver->ng);
		if (ver->username)
			os_mem_free((char *) ver->username);
		if (ver->bytes_B)
			os_mem_free((unsigned char *) ver->bytes_B);
		if (ver->bytes_b)
			os_mem_free((unsigned char *) ver->bytes_b);

		os_mem_free(ver);
	}

	LEAVE();
}



int srp_verifier_is_authenticated( struct SRPVerifier * ver )
{
	ENTER();
	LEAVE();

	return ver->authenticated;
}


const char * srp_verifier_get_username( struct SRPVerifier * ver )
{
	ENTER();
	LEAVE();

	return ver->username;
}


const unsigned char * srp_verifier_get_session_key( struct SRPVerifier * ver, int * key_length )
{
	ENTER();

	if (key_length)
		*key_length = hash_length(ver->hash_alg);

	LEAVE();
	return ver->session_key;
}


int srp_verifier_get_srp_proof_length( struct SRPVerifier * ver )
{
	ENTER();
	LEAVE();

	return hash_length( ver->hash_alg );
}


/* user_M must be exactly SHA512HashSize bytes in size */
void srp_verifier_verify_session( struct SRPVerifier * ver, const unsigned char * user_M, const unsigned char ** bytes_HAMK )
{
	ENTER();

	if ( memcmp( ver->M, user_M, hash_length(ver->hash_alg) ) == 0 )
	{
		ver->authenticated = 1;
		*bytes_HAMK = ver->H_AMK;
	}
	else
		*bytes_HAMK = NULL;

	LEAVE();
}

/*******************************************************************************/
