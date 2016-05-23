/*
 * Copyright (C) 2008-2015, Marvell International Ltd.
 * All Rights Reserved.
 */

/** tls_cyassl.c: the TLS API
 */

#include <openssl/ssl.h>
#include <cyassl/ssl.h>
#include <ctaocrypt/logging.h>

#include <wmassert.h>
#include <wm_os.h>
#include <wmstdio.h>
#include <mdev_aes.h>
#include <mdev_crc.h>

#include <lwip/sockets.h>
#include <wm_net.h>
#include <lwip/inet.h>
#include <lwip/netdb.h>

#include <memory.h>
#include "tls_common.h"
#include <wm-tls.h>
#ifdef CONFIG_WPA2_ENTP
#include <wmcrypto.h>
#define EAP_TLS_KEY_LEN	64
#define EAP_EMSK_LEN	64
#define TLS_KEY_LEN	32
#endif

typedef struct {
	SSL_CTX *ctx;
	SSL *ssl;
#ifdef CONFIG_WPA2_ENTP
	uint8_t key_data[TLS_KEY_LEN];
#endif
	bool session_setup_done;
} tls_session_t;

#ifdef CONFIG_WPA2_ENTP
struct tls_keys {
	unsigned char *master_key;
	unsigned int master_key_len;
	unsigned char *server_random;
	unsigned int server_random_len;
	unsigned char *client_random;
	unsigned int client_random_len;
};
#endif

static bool tls_lib_inited;

int (*wps_cyassl_tx_callback)(const uint8_t *buf, const size_t len);
int (*wps_cyassl_rx_callback)(char *buf, int *len);

#ifdef NON_BLOCKING
enum {
	SELECT_FAIL,
	TIMEOUT,
	RECV_READY,
	ERROR_READY
};


#ifdef CYASSL_CALLBACKS
Timeval timeout;

int handShakeCB(HandShakeInfo *info)
{
	(void)info;
	return 0;
}


int timeoutCB(TimeoutInfo *info)
{
	(void)info;
	return 0;
}

#endif

static int tcp_select(int socketfd, int to_sec)
{
	fd_set recvfds, errfds;
	int nfds = socketfd + 1;
	struct timeval timeout = { (to_sec > 0) ? to_sec : 0, 0};
	int result;

	FD_ZERO(&recvfds);
	FD_SET(socketfd, &recvfds);
	FD_ZERO(&errfds);
	FD_SET(socketfd, &errfds);

	result = select(nfds, &recvfds, NULL, &errfds, &timeout);

	if (result == 0)
		return TIMEOUT;
	else if (result > 0) {
		if (FD_ISSET(socketfd, &recvfds))
			return RECV_READY;
		else if (FD_ISSET(socketfd, &errfds))
			return ERROR_READY;
	}

	return SELECT_FAIL;
}

#ifdef NON_BLOCKING_CONNECT
static int NonBlockingSSL_Connect(CYASSL *ssl)
{
#ifndef CYASSL_CALLBACKS
	int ret = SSL_connect(ssl);
#else
	int ret = CyaSSL_connect_ex(ssl, handShakeCB, timeoutCB, timeout);
#endif
	int error = SSL_get_error(ssl, 0);
	int sockfd = (int) CyaSSL_get_fd(ssl);
	int select_ret;

	while (ret != SSL_SUCCESS && (error == SSL_ERROR_WANT_READ ||
				error == SSL_ERROR_WANT_WRITE)) {
		int currTimeout = 1;

		if (error == SSL_ERROR_WANT_READ)
			tls_d("... client would read block");
		else
			tls_d("... client would write block");

#ifdef CYASSL_DTLS
		currTimeout = CyaSSL_dtls_get_current_timeout(ssl);
#endif
		select_ret = tcp_select(sockfd, currTimeout);

		if ((select_ret == RECV_READY) ||
				(select_ret == ERROR_READY)) {
#ifndef CYASSL_CALLBACKS
			ret = SSL_connect(ssl);
#else
			ret = CyaSSL_connect_ex(ssl, handShakeCB, timeoutCB,
						timeout);
#endif
			error = SSL_get_error(ssl, 0);
		} else if (select_ret == TIMEOUT && !CyaSSL_dtls(ssl)) {
			error = SSL_ERROR_WANT_READ;
		}
#ifdef CYASSL_DTLS
		else if (select_ret == TIMEOUT && CyaSSL_dtls(ssl) &&
				CyaSSL_dtls_got_timeout(ssl) >= 0) {
			error = SSL_ERROR_WANT_READ;
		}
#endif
		else {
			error = SSL_FATAL_ERROR;
		}
	}
	if (ret != SSL_SUCCESS)
		tls_e("SSL_connect failed");

	return ret;
}
#endif /* NON_BLOCKING_CONNECT */

#ifdef NON_BLOCKING_ACCEPT
static int NonBlockingSSL_Accept(SSL *ssl)
{
#ifndef CYASSL_CALLBACKS
	int ret = SSL_accept(ssl);
#else
	int ret = CyaSSL_accept_ex(ssl, srvHandShakeCB, srvTimeoutCB, srvTo);
#endif
	int error = SSL_get_error(ssl, 0);
	int sockfd = (int) CyaSSL_get_fd(ssl);
	int select_ret;

	while (ret != SSL_SUCCESS && (error == SSL_ERROR_WANT_READ ||
				error == SSL_ERROR_WANT_WRITE)) {
		int currTimeout = 1;

		if (error == SSL_ERROR_WANT_READ)
			tls_d("... server would read block");
		else
			tls_d("... server would write block");

#ifdef CYASSL_DTLS
		currTimeout = CyaSSL_dtls_get_current_timeout(ssl);
#endif
		select_ret = tcp_select(sockfd, currTimeout);

		if ((select_ret == RECV_READY) ||
				(select_ret == ERROR_READY)) {
#ifndef CYASSL_CALLBACKS
			ret = SSL_accept(ssl);
#else
			ret = CyaSSL_accept_ex(ssl,
					srvHandShakeCB, srvTimeoutCB, srvTo);
#endif
			error = SSL_get_error(ssl, 0);
		} else if (select_ret == TIMEOUT && !CyaSSL_dtls(ssl)) {
			error = SSL_ERROR_WANT_READ;
		}
#ifdef CYASSL_DTLS
		else if (select_ret == TIMEOUT && CyaSSL_dtls(ssl) &&
				CyaSSL_dtls_got_timeout(ssl) >= 0) {
			error = SSL_ERROR_WANT_READ;
		}
#endif
		else {
			error = SSL_FATAL_ERROR;
		}
	}
	if (ret != SSL_SUCCESS)
		tls_e("SSL_accept failed");

	return ret;
}
#endif /* NON_BLOCKING_ACCEPT */
#endif /* NON_BLOCKING */

/*
 * Some notes about the TLS server on WMSDK: When a page is fetched first
 * time over TLS in a browser ( Me using FireFox), in the Client hello
 * message the Session ID length is 0. All works well. When the server is
 * restarted now and the page is refreshed, the server sends an "Unexpected
 * message" Alert. The reason for this is that when the page is refreshed
 * the browser client sends a 32 byte session ID in the "Client hello"
 * request. The server because of it being restarted, is not aware of any
 * such session history.
 */

/*
 * This function is used standalone with tls_purge_server_context(). TLS
 * session creation and deletion is not part of these two functions.
 */
SSL_CTX *tls_create_server_context(const tls_init_config_t *cfg)
{
	tls_lib_init();

	SSL_METHOD *method = 0;
	int rv;

	ASSERT(cfg->tls.server.server_cert != NULL);
	ASSERT(cfg->tls.server.server_key != NULL);

#if defined(CYASSL_DTLS)
	method = DTLSv1_server_method();
#elif !defined(NO_TLS)
	method = SSLv23_server_method();
#else
	method = SSLv3_server_method();
#endif
	if (!method)
		return NULL;

	SSL_CTX *ctx = CyaSSL_CTX_new(method);
	if (!ctx) {
		/* 'method' is freed internally when call to SSL_CTX_new
		   fails */
		return NULL;
	}

#ifndef NO_PSK
	/* do PSK */
	SSL_CTX_set_psk_server_callback(ctx, my_psk_server_cb);
	SSL_CTX_use_psk_identity_hint(ctx, "cyassl server");
	SSL_CTX_set_cipher_list(ctx, "PSK-AES256-CBC-SHA");
#else
	if (cfg->flags & TLS_CHECK_CLIENT_CERT) {
		/* not using PSK, verify peer with certs */
		SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER |
				   SSL_VERIFY_FAIL_IF_NO_PEER_CERT, 0);
	} else {
		SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, 0);
	}
#endif

#ifdef OPENSSL_EXTRA
	SSL_CTX_set_default_passwd_cb(ctx, PasswordCallBack);
#endif

	tls_d("Loading certificates");
	if (cfg->tls.server.client_cert) {
		/* Chained certificates are automatically handled */
		rv = CyaSSL_CTX_load_verify_buffer(ctx,
						  cfg->tls.server.client_cert,
						  cfg->tls.
						  server.client_cert_size,
						  SSL_FILETYPE_PEM);
		if (rv != SSL_SUCCESS) {
			tls_e("Server loading client cert failed: %d", rv);
			goto tls_create_ctx_error;
		}

	} else {
		tls_d("Server mode: Client cert not given");
	}

	if (cfg->flags & TLS_CERT_BUFFER_CHAINED) {
		rv = CyaSSL_CTX_use_certificate_chain_buffer(ctx,
							     cfg->tls.server.
							     server_cert,
							     cfg->tls.server.
							     server_cert_size);
	} else {
		rv = CyaSSL_CTX_use_certificate_buffer(ctx,
						       cfg->tls.server.
						       server_cert,
						       cfg->tls.server.
						       server_cert_size,
						       SSL_FILETYPE_PEM);
	}
	if (rv != SSL_SUCCESS) {
		tls_e("Server loading server cert failed: %d", rv);
		goto tls_create_ctx_error;
	}

	rv = CyaSSL_CTX_use_PrivateKey_buffer(ctx, cfg->tls.server.server_key,
					     cfg->tls.server.server_key_size,
					     SSL_FILETYPE_PEM);
	if (rv != SSL_SUCCESS) {
		tls_e("Server loading server private key failed: %d", rv);
		goto tls_create_ctx_error;
	}

	return ctx;
 tls_create_ctx_error:
	CyaSSL_CTX_UnloadCAs(ctx);
	SSL_CTX_free(ctx);
	return NULL;
}

void tls_purge_server_context(SSL_CTX *ctx)
{
	CyaSSL_CTX_UnloadCAs(ctx);
	/* Certificate and private key is unloaded internally */
	SSL_CTX_free(ctx);
}

int tls_server_set_clientfd(SSL *ssl, int clientfd)
{
	ASSERT((ssl != 0) && (clientfd >= 0));

	SSL_set_fd(ssl, clientfd);
#ifdef NO_PSK
#if !defined(NO_FILESYSTEM) && defined(OPENSSL_EXTRA)
	CyaSSL_SetTmpDH_file(ssl, dhParam, SSL_FILETYPE_PEM);
#else
	/* will repick suites with DHE, higher priority than PSK */
	SetDH(ssl);
#endif
#endif

#if defined(NON_BLOCKING) && defined(NON_BLOCKING_ACCEPT)
	CyaSSL_set_using_nonblock(ssl, 1);
	tcp_set_nonblocking(&clientfd);
	if (NonBlockingSSL_Accept(ssl) != SSL_SUCCESS) {
#ifdef CONFIG_ENABLE_ERROR_LOGS
		int err = SSL_get_error(ssl, 0);
		char buffer[80];
		tls_e("error = %d, %s", err, ERR_error_string(err, buffer));
		tls_e("Server: NonBlocking SSL_Accept failed");
#endif /* CONFIG_ENABLE_ERROR_LOGS */
		return -WM_FAIL;
	}
#else
#ifndef CYASSL_CALLBACKS
	if (SSL_accept(ssl) != SSL_SUCCESS) {
#ifdef CONFIG_ENABLE_ERROR_LOGS
		int err = SSL_get_error(ssl, 0);
		char buffer[80];
		tls_e("error = %d, %s", err, ERR_error_string(err, buffer));
		tls_e("Server: SSL_accept failed");
#endif /* CONFIG_ENABLE_ERROR_LOGS */
		return -WM_FAIL;
	}
#else
	CyaSSL_set_using_nonblock(ssl, 1);
	tcp_set_nonblocking(&clientfd);
	if (NonBlockingSSL_Accept(ssl) != SSL_SUCCESS) {
#ifdef CONFIG_ENABLE_ERROR_LOGS
		int err = SSL_get_error(ssl, 0);
		char buffer[80];
		tls_e("error = %d, %s", err, ERR_error_string(err, buffer));
		tls_e("Server: NonBlocking SSL_Accept failed");
#endif /* CONFIG_ENABLE_ERROR_LOGS */
		return -WM_FAIL;
	}
#endif
#endif
	showPeer(ssl);
	return WM_SUCCESS;
}

#ifdef CONFIG_WPA2_ENTP
void wps_register_cyassl_tx_callback(int (*WPSEAPoLCyasslTxDataHandler)
				(const uint8_t *buf, const size_t len))
{
	wps_cyassl_tx_callback = WPSEAPoLCyasslTxDataHandler;
}

void wps_deregister_cyassl_tx_callback()
{
	wps_cyassl_tx_callback = NULL;
}

void wps_register_cyassl_rx_callback(int (*WPSEAPoLCyasslRxDataHandler)
				(char *buf, int *len))
{
	wps_cyassl_rx_callback = WPSEAPoLCyasslRxDataHandler;
}

void wps_deregister_cyassl_rx_callback()
{
	wps_cyassl_rx_callback = NULL;
}

static int SSLSend(SSL *ssl, char *buf, int sz, void *ctx)
{
	if (wps_cyassl_tx_callback)
		return wps_cyassl_tx_callback((const uint8_t *)buf, sz);
	return -WM_FAIL;
}

static int SSLReceive(SSL *ssl, char *buf, int sz, void *ctx)
{
	if (wps_cyassl_rx_callback)
		return wps_cyassl_rx_callback((char *)buf, &sz);
	return -WM_FAIL;
}

int tls_derive_key(tls_session_t *s, size_t len)
{
	struct tls_keys keys;
	unsigned char *rnd, *out;

	if (SSL_get_keys(s->ssl, &keys.master_key, &keys.master_key_len,
			&keys.server_random, &keys.server_random_len,
			&keys.client_random, &keys.client_random_len) ==
				SSL_SUCCESS) {

		out = os_mem_alloc(len);
		if (out == NULL) {
			tls_e("No memory for tls_derive_key");
			goto fail;
		}

		if (keys.master_key == NULL || keys.server_random == NULL ||
			keys.client_random == NULL)
			goto fail;

		rnd = os_mem_alloc(keys.client_random_len +
				   keys.server_random_len);
		if (rnd == NULL) {
			tls_e("No memory for rnd:: %d",
			      keys.client_random_len + keys.server_random_len);
			goto fail;
		}

		memcpy(rnd, keys.client_random, keys.client_random_len);
		memcpy(rnd + keys.client_random_len, keys.server_random,
			keys.server_random_len);

		mrvl_tls_prf(keys.master_key, keys.master_key_len,
		"client EAP encryption", rnd,
			     keys.client_random_len + keys.server_random_len,
			     out, len);

		memcpy(s->key_data, out, TLS_KEY_LEN);

		os_mem_free(rnd);
		os_mem_free(out);
		return WM_SUCCESS;
	}
fail:
	tls_e("EAP-TLS: Failed to derive key");
	return -WM_FAIL;

}

#define EAP_TLS_KEY_LEN 64
#define EAP_EMSK_LEN 64

int tls_session_success(tls_session_t *s)
{
	int ret;

	tls_d("EAP-TLS: Done");

	ret = tls_derive_key(s, EAP_TLS_KEY_LEN + EAP_EMSK_LEN);

	return ret;
}

int tls_session_key(tls_handle_t h, uint8_t *key, int len)
{
	tls_session_t *s = (tls_session_t *) h;
	ASSERT((h != 0) && (key != NULL));
	ASSERT(s->session_setup_done == true);

	if (s->key_data) {
		memcpy(key, s->key_data, len);
		return len;
	}
	return 0;
}
#endif

SSL_CTX *tls_create_client_context(const tls_client_t *cfg)
{
	if (!cfg)
		return NULL;

	tls_lib_init();

	int ret;
	SSL_METHOD *method;

#if defined(CYASSL_DTLS)
	method = DTLSv1_client_method();
#elif !defined(NO_TLS)
	method = SSLv23_client_method();
#else
	method = SSLv3_client_method();
#endif

	if (!method)
		return NULL;

	SSL_CTX *ctx = SSL_CTX_new(method);
	if (!ctx) {
		/* 'method' is freed internally when call to SSL_CTX_new
		   fails */
		return NULL;
	}

#ifndef NO_PSK
	SSL_CTX_set_psk_client_callback(ctx, my_psk_client_cb);
#endif

#if defined(CYASSL_SNIFFER) && !defined(HAVE_NTRU) && !defined(HAVE_ECC)
	/* don't use EDH, can't sniff tmp keys */
	SSL_CTX_set_cipher_list(ctx, "AES256-SHA");
#endif

	if (cfg->ca_cert) {
		ASSERT(cfg->ca_cert_size != 0);
		/* Load server certificates  from buffer */
		tls_d("Loading CA certificate file. Size: %d",
		      cfg->ca_cert_size);
		/* Chained certificates are automatically handled */
		ret = CyaSSL_CTX_load_verify_buffer(ctx, cfg->ca_cert,
						    cfg->ca_cert_size,
						    SSL_FILETYPE_PEM);
		if (ret != SSL_SUCCESS) {
			tls_e("client: Unable to load CA certificate");
			SSL_CTX_free(ctx);
			return NULL;
		}
	} else {
		tls_d("Disabling certificate check");
		SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, 0);
	}

	if (!cfg->client_cert) {
		/* The server doesn't wants to verify our certificates */
		return ctx;
	}

	ASSERT(cfg->client_cert_size != 0);
	ASSERT(cfg->client_key_size != 0);

	/* Load client certificates and client private keys from buffer */
	tls_d("Loading client certificate buffer. Size: %d",
	      cfg->client_cert_size);
	if (cfg->client_cert_is_chained)
		ret = CyaSSL_CTX_use_certificate_chain_buffer(ctx,
							      cfg->client_cert,
							      cfg->
							      client_cert_size);
	else
		ret = CyaSSL_CTX_use_certificate_buffer(ctx, cfg->client_cert,
							cfg->client_cert_size,
							SSL_FILETYPE_PEM);


	if (ret != SSL_SUCCESS) {
		tls_e("Unable to load client certificate");
		goto tls_client_create_ctx_error;
	}

	tls_d("Loading client private key buffer. Size: %d",
	      cfg->client_key_size);
	ret = CyaSSL_CTX_use_PrivateKey_buffer(ctx, cfg->client_key,
					       cfg->client_key_size,
					       SSL_FILETYPE_PEM);
	if (ret != SSL_SUCCESS) {
		tls_e("Unable to load client key");
		goto tls_client_create_ctx_error;
	}

	return ctx;

 tls_client_create_ctx_error:
	CyaSSL_CTX_UnloadCAs(ctx);
	SSL_CTX_free(ctx);
	return NULL;
}

void tls_purge_client_context(SSL_CTX *ctx)
{
	CyaSSL_CTX_UnloadCAs(ctx);
	/* Certificate and private key is unloaded internally */
	SSL_CTX_free(ctx);
}

static int _tls_session_init_client(tls_session_t *s, int sockfd,
				    const tls_init_config_t *cfg)
{
	int ret;
	SSL_METHOD *method;

#if defined(CYASSL_DTLS)
	method = DTLSv1_client_method();
#elif !defined(NO_TLS)
	method = SSLv23_client_method();
#else
	method = SSLv3_client_method();
#endif

	if (!method)
		return -WM_E_NOMEM;

	s->ctx = SSL_CTX_new(method);
	if (!s->ctx) {
		/* 'method' is freed internally when call to SSL_CTX_new
		   fails */
		return -WM_E_NOMEM;
	}

#ifndef NO_PSK
	SSL_CTX_set_psk_client_callback(s->ctx, my_psk_client_cb);
#endif

#if defined(CYASSL_SNIFFER) && !defined(HAVE_NTRU) && !defined(HAVE_ECC)
	/* don't use EDH, can't sniff tmp keys */
	SSL_CTX_set_cipher_list(s->ctx, "AES256-SHA");
#endif

	if (cfg->flags & TLS_CHECK_SERVER_CERT) {
		ASSERT(cfg->tls.client.ca_cert_size != 0);
		/* Load server certificates  from buffer */
		tls_d("Loading CA certificate file. Size: %d",
		      cfg->tls.client.ca_cert_size);
		ret = CyaSSL_CTX_load_verify_buffer(s->ctx,
						    cfg->tls.client.ca_cert,
						    cfg->tls.client.
						    ca_cert_size,
						    SSL_FILETYPE_PEM);
		if (ret != SSL_SUCCESS) {
			tls_e("Unable to load CA certificate");
			SSL_CTX_free(s->ctx);
			return -WM_FAIL;
		}
	} else {
		tls_d("Disabling certificate check");
		SSL_CTX_set_verify(s->ctx, SSL_VERIFY_NONE, 0);
	}

	if (cfg->flags & TLS_USE_CLIENT_CERT) {

		/* The server wants to verify our certificates */
		ASSERT(cfg->tls.client.client_cert_size != 0);
		ASSERT(cfg->tls.client.client_key_size != 0);

		/* Load client certificates and client private keys from
		   buffer */
		tls_d("Loading client certificate buffer. Size: %d",
		      cfg->tls.client.client_cert_size);
		ret = CyaSSL_CTX_use_certificate_buffer(s->ctx,
							cfg->tls.client.
							client_cert,
							cfg->tls.client.
							client_cert_size,
							SSL_FILETYPE_PEM);
		if (ret != SSL_SUCCESS) {
			tls_e("Unable to load client certificate");
			SSL_CTX_free(s->ctx);
			return -WM_FAIL;
		}

		tls_d("Loading client private key buffer. Size: %d",
		      cfg->tls.client.client_key_size);
		ret = CyaSSL_CTX_use_PrivateKey_buffer(s->ctx,
						       cfg->tls.client.
						       client_key,
						       cfg->tls.client.
						       client_key_size,
						       SSL_FILETYPE_PEM);
		if (ret != SSL_SUCCESS) {
			tls_e("Unable to load client key");
			SSL_CTX_free(s->ctx);
			return -WM_FAIL;
		}
	}

	s->ssl = SSL_new(s->ctx);
	if (!s->ssl) {
		if (cfg->tls.client.ca_cert)
			CyaSSL_CTX_UnloadCAs(s->ctx);

		/* Certificate and private key is unloaded internally */
		SSL_CTX_free(s->ctx);
		return -WM_E_NOMEM;
	}

	SSL_set_fd(s->ssl, sockfd);

#ifdef CONFIG_WPA2_ENTP
	if (cfg->flags & TLS_WPA2_ENTP) {
		CyaSSL_SetIOSend(s->ctx, SSLSend);
		CyaSSL_SetIORecv(s->ctx, SSLReceive);
	}
#endif

#if defined(NON_BLOCKING) && defined(NON_BLOCKING_CONNECT)
	CyaSSL_set_using_nonblock(s->ssl, 1);
	tcp_set_nonblocking(&sockfd);
	tls_d("Starting Non Blocking SSL connect");
	/* see note at top of README */
	if (NonBlockingSSL_Connect(s->ssl) != SSL_SUCCESS) {
#ifdef CONFIG_ENABLE_ERROR_LOGS
		int err = SSL_get_error(s->ssl, 0);
		char buffer[80];
		tls_e("err = %d, %s", err, ERR_error_string(err, buffer));
		/* if you're getting an error here  */
		tls_e("SSL_connect failed");
#endif /* CONFIG_ENABLE_ERROR_LOGS */
		SSL_free(s->ssl);
		SSL_CTX_free(s->ctx);
		return -WM_FAIL;
	}
#else
#ifndef CYASSL_CALLBACKS
	tls_d("Starting SSL connect");
	/* see note at top of README */
	if (SSL_connect(s->ssl) != SSL_SUCCESS) {
#ifdef CONFIG_ENABLE_ERROR_LOGS
		int err = SSL_get_error(s->ssl, 0);
		char buffer[80];
		tls_e("err = %d, %s", err, ERR_error_string(err, buffer));
		/* if you're getting an error here  */
		tls_e("SSL_connect failed");
#endif /* CONFIG_ENABLE_ERROR_LOGS */
		SSL_free(s->ssl);
		SSL_CTX_free(s->ctx);
		return -WM_FAIL;
	}
#else
	timeout.tv_sec = 2;
	timeout.tv_usec = 0;
	CyaSSL_set_using_nonblock(s->ssl, 1);
	tcp_set_nonblocking(&sockfd);
	tls_d("Starting Non Blocking SSL connect");
	/* see note at top of README */
	if (NonBlockingSSL_Connect(s->ssl) != SSL_SUCCESS) {
#ifdef CONFIG_ENABLE_ERROR_LOGS
		int err = SSL_get_error(s->ssl, 0);
		char buffer[80];
		tls_e("err = %d, %s", err, ERR_error_string(err, buffer));
		/* if you're getting an error here  */
		tls_e("SSL_connect failed");
#endif /* CONFIG_ENABLE_ERROR_LOGS */
		SSL_free(s->ssl);
		SSL_CTX_free(s->ctx);
		return -WM_FAIL;
	}
#endif
#endif
	showPeer(s->ssl);

	s->session_setup_done = true;

	tls_d("SSL Connect success");

#ifdef CONFIG_WPA2_ENTP
	if (cfg->flags & TLS_WPA2_ENTP)
		return tls_session_success(s);
#endif

	return WM_SUCCESS;
}

int tls_session_init(tls_handle_t *h, int sockfd,
		     const tls_init_config_t *cfg)
{
	int status;
	tls_session_t *s;

	if (!tls_lib_inited) {
		tls_e("TLS library not initialised");
		return -WM_FAIL;
	}

	tls_d("Session init");

	ASSERT(h != NULL);
	ASSERT(sockfd >= 0);
	ASSERT(cfg != NULL);

	s = os_mem_alloc(sizeof(tls_session_t));
	if (!s) {
		tls_e("No memory for tls session");
		return -WM_E_NOMEM;
	}

	memset(s, 0x00, sizeof(tls_session_t));

	if (!(cfg->flags & TLS_SERVER_MODE)) {
		status = _tls_session_init_client(s, sockfd, cfg);
		if (status != WM_SUCCESS) {
			os_mem_free(s);
			return status;
		}
	}

	*h = (tls_handle_t) s;
	return WM_SUCCESS;
}

int tls_send(tls_handle_t h, const void *buf, int len)
{
	tls_session_t *s = (tls_session_t *) h;
	/* tls_d("Send: %x: %d", buf, len); */
	ASSERT((h != 0) && (buf != NULL));
	ASSERT(s->session_setup_done == true);

	return SSL_write(s->ssl, buf, len);
}

int tls_recv(tls_handle_t h, void *buf, int max_len)
{
	tls_session_t *s = (tls_session_t *) h;
	/* tls_d("Recv: %x: %d", buf, max_len); */
	ASSERT((h != 0) && (buf != NULL));
	ASSERT(s->session_setup_done == true);

	return SSL_read(s->ssl, buf, max_len);
}

void tls_close(tls_handle_t *h)
{
	tls_d("Close session");
	ASSERT(h != NULL);
	ASSERT(*h != 0);
	tls_session_t *s = (tls_session_t *) *h;
	ASSERT(s->session_setup_done == true);

	/* Check whether s is valid or not
	 * because WPS module calls tls_close
	 * from multiple locations during WPA2
	 * Enterprise negotiation.
	 */

	if (!s)
		return;

	if (s->ssl) {
		SSL_shutdown(s->ssl);
		SSL_free(s->ssl);
	}
	if (s->ctx)
		SSL_CTX_free(s->ctx);

	s->session_setup_done = false;

	os_mem_free(s);

	*h = 0;
}

static void *cyassl_mem_malloc(size_t size)
{
	void *buffer = os_mem_alloc(size);
	if (!buffer) {
		tls_e("failed to allocate mem for CyaSSL");
		return NULL;
	}

	return buffer;
}

static void cyassl_mem_free(void *buffer)
{
	os_mem_free(buffer);
}

static void *cyassl_mem_realloc(void *buffer, size_t size)
{
	void *new_buffer = os_mem_realloc(buffer, size);
	if (!new_buffer) {
		tls_e("CYASSL:failed to allocate memory");
		return NULL;
	}

	return new_buffer;
}

int ctaocrypt_lib_init()
{
	static bool cinited;
	if (!cinited) {
		aes_drv_init();
		crc_drv_init();
		CyaSSL_SetAllocators(cyassl_mem_malloc, cyassl_mem_free,
				     cyassl_mem_realloc);
		cinited = 1;
	}
	return WM_SUCCESS;
}

#ifdef CONFIG_ENABLE_CYASSL_DEBUG
static void cyassl_logging_cb(const int logLevel, const char *const logMessage)
{

	switch (logLevel) {
	case ERROR_LOG:
#ifdef CONFIG_CYASSL_ENABLE_ERROR_LOG
		wmprintf("%s\n\r", logMessage);
#endif
		break;
	case INFO_LOG:
#ifdef CONFIG_CYASSL_ENABLE_INFO_LOG
		wmprintf("%s\n\r", logMessage);
#endif
		break;
	case ENTER_LOG:
#ifdef CONFIG_CYASSL_ENABLE_FUNC_ENTRY_LOG
		wmprintf("%s\n\r", logMessage);
#endif
		break;
	case LEAVE_LOG:
#ifdef CONFIG_CYASSL_ENABLE_FUNC_LEAVE_LOG
		wmprintf("%s\n\r", logMessage);
#endif
		break;
	case OTHER_LOG:
#ifdef CONFIG_CYASSL_ENABLE_OTHER_LOG
		wmprintf("%s\n\r", logMessage);
#endif
		break;
	default:
		break;
	}
}
#endif /* CONFIG_ENABLE_CYASSL_DEBUG */

int tls_lib_init()
{
	if (!tls_lib_inited) {
		ctaocrypt_lib_init();
		CyaSSL_Init();
		/* InitCyaSSL(); */
#ifdef CONFIG_ENABLE_CYASSL_DEBUG
		CyaSSL_Debugging_ON();
		CyaSSL_SetLoggingCb(cyassl_logging_cb);
#endif
		tls_lib_inited = 1;
	}
	return WM_SUCCESS;
}

void test_1(tls_handle_t h)
{
	char reply[11];
	tls_session_t *s = (tls_session_t *) h;
	int total_bytes;

	int input = SSL_read(s->ssl, reply, sizeof(reply) - 1);
	if (input > 0) {
		total_bytes += input;
		reply[input] = 0;
		tls_d("Server response: %s", reply);

		while (1) {
			input = SSL_read(s->ssl, reply, sizeof(reply) - 1);
			if (input > 0) {
				reply[input] = 0;
				wmprintf("%s\n\r", reply);
			} else
				break;
		}
	}
}

int tls_pending(tls_handle_t h)
{
	tls_session_t *s = (tls_session_t *) h;
	ASSERT(h != 0);
	return SSL_pending(s->ssl);
}

int tls_get_error(tls_handle_t h, int rc)
{
	tls_session_t *s = (tls_session_t *) h;
	ASSERT(h != 0);
	return SSL_get_error(s->ssl, rc);
}


