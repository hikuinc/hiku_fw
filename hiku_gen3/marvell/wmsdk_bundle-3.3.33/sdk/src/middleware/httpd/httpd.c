/*
 *  Copyright 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

/* httpd.c: This file contains the routines for the main HTTPD server thread and
 * its initialization.
 */

#ifdef __linux__
#include <linux/tcp.h>
#endif /* __linux__ */

#include <string.h>

#include <wmstdio.h>
#include <wm_os.h>
#include <wm_net.h>
#include <wmassert.h>
#include <httpd.h>
#include <http-strings.h>
#ifdef CONFIG_ENABLE_TLS
#include <wm-tls.h>
#include <openssl/ssl.h>
#endif /* CONFIG_ENABLE_TLS */

#include "httpd_priv.h"

typedef enum {
	HTTPD_INACTIVE = 0,
	HTTPD_INIT_DONE,
	HTTPD_THREAD_RUNNING,
	HTTPD_THREAD_SUSPENDED,
} httpd_state_t;

httpd_state_t httpd_state;

enum {
	HTTP_CONN = false,
	HTTPS_CONN = true
};

static os_thread_t httpd_main_thread;
#ifdef CONFIG_ENABLE_HTTPS_SERVER
static os_thread_stack_define(httpd_stack, 4096 * 2 + 4096);
#else /* ! CONFIG_ENABLE_HTTPS_SERVER */
static os_thread_stack_define(httpd_stack, 4096 * 2 + 2048);
#endif /* CONFIG_ENABLE_HTTPS_SERVER */

/* Why HTTPD_MAX_MESSAGE + 2?
 * Handlers are allowed to use HTTPD_MAX_MESSAGE bytes of this buffer.
 * Internally, the POST var processing needs a null termination byte and an
 * '&' termination byte.
 */
static bool httpd_stop_req;

#define HTTPD_TIMEOUT_EVENT 0

/** Maximum number of backlogged http connections
 *
 *  httpd has a single listening socket from which it accepts connections.
 *  HTTPD_MAX_BACKLOG_CONN is the maximum number of connections that can be
 *  pending.  For example, suppose a webpage contains 10 images.  If a client
 *  attempts to load all 10 of those images at once, only the first
 *  HTTPD_MAX_BACKLOG_CONN attempts can succeed.  Some clients will retry when
 *  the attempts fail; others will limit the maximum number of open connections
 *  that it has.  But some may attempt to load all 10 simultaneously.  If your
 *  web pages have many images, or css files, or java script files, you may
 *  need to increase this number.
 *
 *  \note Your underlying TCP/IP stack may have other limitations
 *  besides the backlog.  For example, the treck stack limits the
 *  number of system-wide TCP sockets to TM_OPTION_TCP_SOCKETS_MAX.
 *  You will have to adjust this value if you need more than
 *  TM_OPTION_TCP_SOCKETS_MAX simultaneous TCP sockets.
 *
 */
#define HTTPD_MAX_BACKLOG_CONN 5

#ifdef CONFIG_HTTP_CLIENT_SOCKETS
#define MAX_HTTP_CLIENT_SOCKETS	CONFIG_HTTP_CLIENT_SOCKETS
#else
#define MAX_HTTP_CLIENT_SOCKETS 1
#endif

#ifdef CONFIG_ENABLE_HTTP_SERVER
static int http_sockfd;
#ifdef CONFIG_IPV6
static int http_ipv6_sockfd;
#endif /* CONFIG_IPV6 */
#endif /* CONFIG_ENABLE_HTTP_SERVER */

#ifdef CONFIG_ENABLE_HTTPS_SERVER
static int https_sockfd;
#ifdef CONFIG_IPV6
static int https_ipv6_sockfd;
#endif /* CONFIG_IPV6 */
/* TLS context. This is reused for each HTTPS client socket */
static SSL_CTX *ssl_ctx;
#endif /* CONFIG_ENABLE_HTTPS_SERVER */

typedef struct {
	void *sock_data_ptr;
	int sockfd;
	httpd_free_priv_data_fn_t free_priv_data_fn;
	/* Mutex name will be of the form m<x> where x is a number
	 * from 0-99
	 */
	char mutex_name[4];
	os_mutex_t sock_mutex;
#ifdef CONFIG_ENABLE_HTTPD_PURGE_LRU
	uint64_t timestamp;
#endif /* !CONFIG_ENABLE_HTTPD_PURGE_LRU */
#ifdef CONFIG_ENABLE_HTTPS_SERVER
	SSL *ssl;
#endif /* CONFIG_ENABLE_HTTPS_SERVER */
} client_sock_data_t;

client_sock_data_t client_sock_data[MAX_HTTP_CLIENT_SOCKETS];

WEAK int httpd_get_http_port()
{
	return HTTP_PORT;
}

WEAK int httpd_get_https_port()
{
	return HTTPS_PORT;
}

int httpd_net_close(int sockfd)
{
	int i;
	for (i = 0; i < MAX_HTTP_CLIENT_SOCKETS; i++) {
		if (sockfd == client_sock_data[i].sockfd) {
			/* Before closing the socket, ensure than no other
			 * entity is using it, by taking the mutex.
			 */
			httpd_get_sock_mutex(sockfd,
					OS_WAIT_FOREVER);
#ifdef CONFIG_ENABLE_HTTPS_SERVER
			if (client_sock_data[i].ssl) {
				SSL_shutdown(client_sock_data[i].ssl);
				SSL_free(client_sock_data[i].ssl);
				client_sock_data[i].ssl = NULL;
			}
#endif /* CONFIG_ENABLE_HTTPS_SERVER */
			if (net_close(client_sock_data[i].sockfd) ==
					WM_SUCCESS) {
				httpd_clear_priv_data(sockfd);
				/* Release the mutex before resetting the
				 * sockfd calue to -1.
				 */
				httpd_put_sock_mutex(sockfd);
				client_sock_data[i].sockfd = -1;
#ifdef CONFIG_ENABLE_HTTPD_PURGE_LRU
				client_sock_data[i].timestamp = 0;
#endif /* !CONFIG_ENABLE_HTTPD_PURGE_LRU */
				return WM_SUCCESS;
			} else {
				/* Release the mutex, even if the net_close()
				 * above has not succeeded.
				 */
				httpd_put_sock_mutex(sockfd);
			}
		}
	}
	return -WM_FAIL;
}

#ifdef CONFIG_ENABLE_HTTPS_SERVER
SSL *httpd_get_tls_handle(int sockfd)
{
	int i;
	for (i = 0; i < MAX_HTTP_CLIENT_SOCKETS; i++) {
		if (sockfd == client_sock_data[i].sockfd)
			return client_sock_data[i].ssl;
	}

	return NULL;
}
#endif /* CONFIG_ENABLE_HTTPS_SERVER */

void *httpd_get_priv_data(int sockfd)
{
	int i;
	for (i = 0; i < MAX_HTTP_CLIENT_SOCKETS; i++) {
		if (sockfd == client_sock_data[i].sockfd)
			return client_sock_data[i].sock_data_ptr;
	}
	return NULL;
}

httpd_free_priv_data_fn_t httpd_get_free_priv_data_fn(int sockfd)
{
	int i;
	for (i = 0; i < MAX_HTTP_CLIENT_SOCKETS; i++) {
		if (sockfd == client_sock_data[i].sockfd)
			return client_sock_data[i].free_priv_data_fn;
	}
	return NULL;

}
void httpd_clear_priv_data(int sockfd)
{
	int i;
	for (i = 0; i < MAX_HTTP_CLIENT_SOCKETS; i++) {
		if (sockfd == client_sock_data[i].sockfd) {
			if (client_sock_data[i].sock_data_ptr != NULL) {
				if (client_sock_data[i].free_priv_data_fn)
					client_sock_data[i].free_priv_data_fn(
							client_sock_data[i].
							sock_data_ptr);
				else {
					os_mem_free(client_sock_data[i].
							sock_data_ptr);
				}
				client_sock_data[i].free_priv_data_fn = NULL;
				client_sock_data[i].sock_data_ptr = NULL;
			}
		}
	}
}

int httpd_assign_priv_data(int sockfd, void *ptr,
		httpd_free_priv_data_fn_t free_priv_data_fn)
{
	int i;
	for (i = 0; i < MAX_HTTP_CLIENT_SOCKETS; i++) {
		if (sockfd == client_sock_data[i].sockfd) {
			client_sock_data[i].sock_data_ptr = ptr;
			client_sock_data[i].free_priv_data_fn =
				free_priv_data_fn;
			return WM_SUCCESS;
		}
	}

	httpd_d("Could not assign priv data. Socket entry not found.");
	return -WM_FAIL;
}
#ifdef CONFIG_ENABLE_HTTPD_PURGE_LRU
void http_close_lru_socket()
{
	uint64_t smallest_timestamp = ~((uint64_t)0);
	int socket = 0;
	int i;
	for (i = 0; i < MAX_HTTP_CLIENT_SOCKETS; i++) {
		if (client_sock_data[i].sockfd != -1) {
			if (client_sock_data[i].timestamp <
					smallest_timestamp) {
				smallest_timestamp =
					client_sock_data[i].timestamp;
				socket = client_sock_data[i].sockfd;
			}
		}
	}
	httpd_net_close(socket);
}

void httpd_update_sock_timestamp(int sockfd)
{
	int i;
	for (i = 0; i < MAX_HTTP_CLIENT_SOCKETS; i++) {
		if (sockfd == client_sock_data[i].sockfd) {
			client_sock_data[i].timestamp =
				os_total_ticks_get();
			return;
		}
	}
}
#else
void httpd_update_sock_timestamp(int sockfd)
{
}
#endif /* !CONFIG_ENABLE_HTTPD_PURGE_LRU */
int httpd_get_sock_mutex(int sockfd, unsigned long wait)
{
	int i;
	for (i = 0; i < MAX_HTTP_CLIENT_SOCKETS; i++) {
		if (sockfd == client_sock_data[i].sockfd) {
			return os_mutex_get(&client_sock_data[i].sock_mutex,
					wait);
		}
	}
	return -WM_FAIL;
}

int httpd_put_sock_mutex(int sockfd)
{
	int i;
	for (i = 0; i < MAX_HTTP_CLIENT_SOCKETS; i++) {
		if (sockfd == client_sock_data[i].sockfd)
			return os_mutex_put(&client_sock_data[i].sock_mutex);
	}
	return -WM_FAIL;
}
static int add_socket_to_db(int sockfd)
{
	int i;
	for (i = 0; i < MAX_HTTP_CLIENT_SOCKETS; i++) {
		if (client_sock_data[i].sockfd == -1) {
			client_sock_data[i].sockfd = sockfd;
			return i;
		}
	}
	return -WM_FAIL;
}

static bool is_socket_db_full()
{
	int i;
	for (i = 0; i < MAX_HTTP_CLIENT_SOCKETS; i++) {
		if (client_sock_data[i].sockfd == -1) {
			return false;
		}
	}
	return true;
}

static int httpd_close_all_client_sockets()
{
	int i, ret;
	for (i = 0; i < MAX_HTTP_CLIENT_SOCKETS; i++) {
		if (client_sock_data[i].sockfd != -1) {
			httpd_w("Force closing client socket: %d",
					client_sock_data[i].sockfd);
			ret = httpd_net_close(client_sock_data[i].sockfd);
			if (ret != 0) {
				httpd_w("Failed to close client socket: %d",
					net_get_sock_error(
						client_sock_data[i].sockfd));
				return -WM_FAIL;
			}
		}
	}
	return WM_SUCCESS;
}

static void httpd_delete_all_client_mutex()
{
	int i;
	for (i = 0; i < MAX_HTTP_CLIENT_SOCKETS; i++) {
		/* If mutex is not NULL, delete it */
		if (client_sock_data[i].sock_mutex)
			os_mutex_delete(&client_sock_data[i].sock_mutex);
	}
}
static int httpd_close_sockets()
{
	int ret, status = WM_SUCCESS;

#ifdef CONFIG_ENABLE_HTTP_SERVER
	if (http_sockfd != -1) {
		ret = net_close(http_sockfd);
		if (ret != 0) {
			httpd_d("failed to close http socket: %d",
				net_get_sock_error(http_sockfd));
			status = -WM_FAIL;
		}
		http_sockfd = -1;
	}
#ifdef CONFIG_IPV6
	if (http_ipv6_sockfd != -1) {
		ret = net_close(http_ipv6_sockfd);
		if (ret != 0) {
			httpd_d("failed to close http socket: %d",
				net_get_sock_error(http_ipv6_sockfd));
			status = -WM_FAIL;
		}
		http_ipv6_sockfd = -1;
	}
#endif /* CONFIG_IPV6 */
#endif /* CONFIG_ENABLE_HTTP_SERVER */

#ifdef CONFIG_ENABLE_HTTPS_SERVER
	if (https_sockfd != -1) {
		ret = net_close(https_sockfd);
		if (ret != 0) {
			httpd_d("failed to close https socket: %d",
				net_get_sock_error(https_sockfd));
			status = -WM_FAIL;
		}
		https_sockfd = -1;
	}
#ifdef CONFIG_IPV6
	if (https_ipv6_sockfd != -1) {
		ret = net_close(https_ipv6_sockfd);
		if (ret != 0) {
			httpd_d("failed to close http socket: %d",
				net_get_sock_error(https_ipv6_sockfd));
			status = -WM_FAIL;
		}
		https_ipv6_sockfd = -1;
	}
#endif /* CONFIG_IPV6 */
#endif /* CONFIG_ENABLE_HTTPS_SERVER */
	status = httpd_close_all_client_sockets();
	httpd_delete_all_client_mutex();
	return status;
}

static void httpd_suspend_thread(bool warn)
{
	if (warn) {
		httpd_w("Suspending thread");
	} else {
		httpd_d("Suspending thread");
	}

	httpd_close_sockets();
	httpd_state = HTTPD_THREAD_SUSPENDED;
	os_thread_self_complete(NULL);
}

/* Valid values for isipv6 are:
 * 1: Create socket for IPv6 i.e. use PF_INET6 family and fill up IPv6 address
 * for listen()
 * 0: Create socket for IPv4 i.e. use PF_INET family and fill up IPv4 address
 * for listen()
 * */
static int httpd_setup_new_socket(int port, int isipv6)
{
	int one = 1;
	int status, addr_len, sockfd;
	struct sockaddr_in addr_listen;
#ifdef CONFIG_IPV6
	struct sockaddr_in6 addr6_listen;
	static const struct in6_addr local_in6addr_any = {
		.un = {IN6ADDR_ANY_INIT}
	};
#else
	/* CONFIG_IPV6 is not set. Hence IPv6 socket can not be created */
	if (isipv6)
		return -WM_FAIL;
#endif /* CONFIG_IPV6 */

	/* create listening TCP socket */
	if (!isipv6) {
		sockfd = net_socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	}
#ifdef CONFIG_IPV6
	if (isipv6) {
		sockfd = net_socket(PF_INET6, SOCK_STREAM, IPPROTO_TCP);
	}
#endif /* CONFIG_IPV6 */

	if (sockfd < 0) {
		status = net_get_sock_error(sockfd);
		httpd_d("Socket creation failed: Port: %d Status: %d",
			port, status);
		return status;
	}

	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,
		   (char *)&one, sizeof(one));
	if (!isipv6) {
		addr_listen.sin_family = AF_INET;
		addr_listen.sin_port = htons(port);
		addr_listen.sin_addr.s_addr = htonl(INADDR_ANY);
		addr_len = sizeof(struct sockaddr_in);
	}
#ifdef CONFIG_IPV6
	if (isipv6) {
		addr6_listen.sin6_family = AF_INET6;
		addr6_listen.sin6_port = htons(port);
		addr6_listen.sin6_addr = local_in6addr_any;
		addr_len = sizeof(struct sockaddr_in6);
	}
#endif /* CONFIG_IPV6 */

#ifndef CONFIG_LWIP_STACK
	/* Limit receive queue of listening socket which in turn limits backlog
	 * connection queue size */
	int opt, optlen;
	opt = 2048;
	optlen = sizeof(opt);
	if (setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF,
		       (char *)&opt, optlen) == -1) {
		httpd_d("Unsupported option SO_RCVBUF on Port: %d: Status: %d",
			net_get_sock_error(sockfd), port);
	}
#endif /* CONFIG_LWIP_STACK */

	/* bind insocket */
	if (!isipv6) {
		status = net_bind(sockfd, (struct sockaddr *)&addr_listen,
				addr_len);
	}
#ifdef CONFIG_IPV6
	if (isipv6) {
		status = net_bind(sockfd, (struct sockaddr *)&addr6_listen,
				addr_len);
	}
#endif
	if (status < 0) {
		status = net_get_sock_error(sockfd);
		httpd_d("Failed to bind socket on port: %d Status: %d",
			status, port);
		return status;
	}

	status = net_listen(sockfd, HTTPD_MAX_BACKLOG_CONN);
	if (status < 0) {
		status = net_get_sock_error(sockfd);
		httpd_d("Failed to listen on port %d: %d.", port, status);
		return status;
	}

	httpd_d("Listening on port %d.", port);
	return sockfd;
}

static int httpd_setup_main_sockets()
{
#ifdef CONFIG_ENABLE_HTTP_SERVER
	http_sockfd = httpd_setup_new_socket(httpd_get_http_port(), 0);
	if (http_sockfd < 0) {
		/* Socket creation failed */
		return http_sockfd;
	}
#ifdef CONFIG_IPV6
	http_ipv6_sockfd = httpd_setup_new_socket(httpd_get_http_port(), 1);
	if (http_ipv6_sockfd < 0) {
		/* Socket creation failed */
		return http_ipv6_sockfd;
	}
#endif /* CONFIG_IPV6 */
#endif /* CONFIG_ENABLE_HTTP_SERVER */

#ifdef CONFIG_ENABLE_HTTPS_SERVER
	/*
	 * From this point on we do not need data structure passed in call
	 * httpd_use_tls_certificates()
	 */
	https_sockfd = httpd_setup_new_socket(httpd_get_https_port(), 0);
	if (https_sockfd < 0) {
		/* Socket creation failed */
		return https_sockfd;
	}
#ifdef CONFIG_IPV6
	https_ipv6_sockfd = httpd_setup_new_socket(httpd_get_https_port(), 1);
	if (https_ipv6_sockfd < 0) {
		/* Socket creation failed */
		return https_ipv6_sockfd;
	}
#endif /* CONFIG_IPV6 */
#endif /* CONFIG_ENABLE_HTTPS_SERVER */

	return WM_SUCCESS;
}

static int httpd_select(int max_sock, const fd_set *readfds,
			 fd_set *active_readfds, int timeout_secs)
{
	int activefds_cnt;
	struct timeval timeout;

	fd_set local_readfds;

	if (timeout_secs >= 0)
		timeout.tv_sec = timeout_secs;
	timeout.tv_usec = 0;

	memcpy(&local_readfds, readfds, sizeof(fd_set));
	httpd_d("WAITING for activity");

	activefds_cnt = net_select(max_sock + 1, &local_readfds,
				   NULL, NULL,
				   timeout_secs >= 0 ? &timeout : NULL);
	if (activefds_cnt < 0) {
		httpd_e("Select failed: %d, errno: %d", timeout_secs, errno);
		/* If the error is not due to a bad file descriptor, it may
		 * be a catastrophic failure, and so we suspend the thread.
		 * TODO: Add a callback to notify the error to the application
		 * OR generate a CRITICAL ERROR.
		 *
		 * If the error is due to a bad file descriptor, we will just
		 * return from here, since it is not a major failure.
		 */
		if (errno != EBADF)
			httpd_suspend_thread(true);
		return activefds_cnt;
	}

	if (httpd_stop_req) {
		httpd_d("HTTPD stop request received");
		httpd_stop_req = FALSE;
		httpd_suspend_thread(false);
	}

	if (activefds_cnt) {
		/* Update users copy of fd_set only if he wants */
		if (active_readfds)
			memcpy(active_readfds, &local_readfds,
			       sizeof(fd_set));
		return activefds_cnt;
	}

	httpd_d("TIMEOUT");

	return HTTPD_TIMEOUT_EVENT;
}

static int httpd_accept_client_socket(const int main_sockfd,
				      bool conn_type)
{
	struct sockaddr_in addr_from;
	socklen_t addr_from_len;
	int client_sockfd;
#ifdef CONFIG_ENABLE_HTTPS_SERVER
	int status;
#endif /* CONFIG_ENABLE_HTTPS_SERVER */
#ifdef CONFIG_ENABLE_HTTPD_PURGE_LRU
	if (is_socket_db_full())
		http_close_lru_socket();
#endif /* !CONFIG_ENABLE_HTTPD_PURGE_LRU */

	addr_from_len = sizeof(addr_from);

	client_sockfd = net_accept(main_sockfd, (struct sockaddr *)&addr_from,
				   &addr_from_len);
	if (client_sockfd < 0) {
		httpd_d("net_accept client socket failed %d (errno: %d).",
			client_sockfd, errno);
		return -WM_FAIL;
	}
	int index = add_socket_to_db(client_sockfd);
	if (index < 0) {
		httpd_d("No more sockets can be accepted");
		net_close(client_sockfd);
		return -WM_FAIL;
	}
#ifdef CONFIG_ENABLE_HTTPD_PURGE_LRU
	client_sock_data[index].timestamp = os_total_ticks_get();
#endif /* !CONFIG_ENABLE_HTTPD_PURGE_LRU */
#ifdef CONFIG_ENABLE_HTTPD_KEEPALIVE
	/*
	 * Enable TCP Keep-alive for accepted client connection
	 *  -- By enabling this feature TCP sends probe packet if there is
	 *  inactivity over connection for specified interval
	 *  -- If there is no response to probe packet for specified retries
	 *  then connection is closed with RST packet to peer end
	 *  -- Ref: http://tldp.org/HOWTO/html_single/TCP-Keepalive-HOWTO/
	 *
	 * We are doing this as we have single threaded web server with
	 * synchronous (blocking) API usage like send, recv and they might get
	 * blocked due to un-availability of peer end, causing web server to
	 * be in-responsive forever.
	 */
	int optval = true;
	if (setsockopt(client_sockfd, SOL_SOCKET, SO_KEEPALIVE,
					&optval, sizeof(optval)) == -1) {
		httpd_d("Unsupported option SO_KEEPALIVE: %d",
				net_get_sock_error(client_sockfd));
	}

	/* TCP Keep-alive idle/inactivity timeout is 60 seconds */
	optval = 60;
	if (setsockopt(client_sockfd, IPPROTO_TCP, TCP_KEEPIDLE,
					&optval, sizeof(optval)) == -1) {
		httpd_d("Unsupported option TCP_KEEPIDLE: %d",
				net_get_sock_error(client_sockfd));
	}

	/* TCP Keep-alive retry count is 20 */
	optval = 20;
	if (setsockopt(client_sockfd, IPPROTO_TCP, TCP_KEEPCNT,
					&optval, sizeof(optval)) == -1) {
		httpd_w("Unsupported option TCP_KEEPCNT: %d",
				net_get_sock_error(client_sockfd));
	}

	/* TCP Keep-alive retry interval (in case no response for probe
	 * packet) is 2 seconds.
	 */
	optval = 2;
	if (setsockopt(client_sockfd, IPPROTO_TCP, TCP_KEEPINTVL,
					&optval, sizeof(optval)) == -1) {
		httpd_d("Unsupported option TCP_KEEPINTVL: %d",
				net_get_sock_error(client_sockfd));
	}
#endif /* !CONFIG_ENABLE_HTTPD_KEEPALIVE */

#ifndef CONFIG_LWIP_STACK
	int opt, optlen;
	opt = 2048;
	optlen = sizeof(opt);
	if (setsockopt(client_sockfd, SOL_SOCKET, SO_SNDBUF,
		       (char *)&opt, optlen) == -1) {
		httpd_d("Unsupported option SO_SNDBUF: %d",
			net_get_sock_error(client_sockfd));
	}
#endif /* CONFIG_LWIP_STACK */

	httpd_d("connecting %d to %x:%d.", client_sockfd,
	      addr_from.sin_addr.s_addr, addr_from.sin_port);

#ifdef CONFIG_ENABLE_HTTPS_SERVER
	if (conn_type == HTTPS_CONN) {
		client_sock_data[index].ssl = SSL_new(ssl_ctx);
		if (!client_sock_data[index].ssl) {
			httpd_e("SSL_new failed");
			httpd_net_close(client_sockfd);
			httpd_suspend_thread(true);
		}
		
		status = tls_server_set_clientfd(client_sock_data[index].ssl,
						 client_sockfd);
		if (status != WM_SUCCESS) {
			httpd_net_close(client_sockfd);
			httpd_e("TLS Client socket set failed");
			return status;
		}
	}
#endif /* CONFIG_ENABLE_HTTPS_SERVER */
	return client_sockfd;
}

/* Pass the http socket as the parameter.
 * If we do not wish to wait on the main http socket
 * and wait only on the client sockets, pass -1 as there
 * value.
 */
static int get_max_sockfd(int sockfd)
{
	int i;
	for (i = 0; i < MAX_HTTP_CLIENT_SOCKETS; i++) {
		if (client_sock_data[i].sockfd > sockfd)
			sockfd = client_sock_data[i].sockfd;
	}
	return sockfd;
}

static void httpd_handle_connection()
{
	int activefds_cnt, status, i;
	int max_sockfd = -1, main_sockfd = -1;
	fd_set readfds, active_readfds;
	if (httpd_stop_req) {
		httpd_d("HTTPD stop request received");
		httpd_stop_req = FALSE;
		httpd_suspend_thread(false);
	}
	FD_ZERO(&readfds);
	for (i = 0; i < MAX_HTTP_CLIENT_SOCKETS; i++) {
		if (client_sock_data[i].sockfd != -1)
			FD_SET(client_sock_data[i].sockfd, &readfds);
	}
	/* If there is no space available to hold the data for new sockets,
	 * there is no point in waiting on the http socket to accept new
	 * client connections. Else, there will be an unnecesary accept
	 * followed immediately by a close.
	 * So, we first check if the database is full and only then add
	 * the http sockets to the readfs.
	 * However, if CONFIG_ENABLE_HTTPD_HTTPD_LRU is enabled, old sockets
	 * will get purged if the database is full. So the check is
	 * done only if this config option is not set
	 */
#ifndef CONFIG_ENABLE_HTTPD_PURGE_LRU
	if (is_socket_db_full()) {
		max_sockfd = get_max_sockfd(-1);
	} else
#endif /* !CONFIG_ENABLE_HTTPD_PURGE_LRU */
	{
#ifdef CONFIG_ENABLE_HTTP_SERVER
		FD_SET(http_sockfd, &readfds);
#ifdef CONFIG_IPV6
		FD_SET(http_ipv6_sockfd, &readfds);
		max_sockfd = get_max_sockfd(http_ipv6_sockfd);
#else
		max_sockfd = get_max_sockfd(http_sockfd);
#endif /* CONFIG_IPV6 */
#endif /* CONFIG_ENABLE_HTTP_SERVER */

#ifdef CONFIG_ENABLE_HTTPS_SERVER
		FD_SET(https_sockfd, &readfds);
#ifdef CONFIG_IPV6
		FD_SET(https_ipv6_sockfd, &readfds);
		max_sockfd = get_max_sockfd(https_ipv6_sockfd);
#else
		max_sockfd = get_max_sockfd(https_sockfd);
#endif /* CONFIG_IPV6 */
#endif /* CONFIG_ENABLE_HTTPS_SERVER */
	}
	/* A negative value for timeout means that the select will never
	 * timeout. However, it will return on one of these
	 * 1) A new request received on the http server socket
	 * 2) New data received on an accepted client socket
	 * 3) Accepted client socket timed out after the keep-alive mechanism
	 * failed
	 * 4) HTTPD stop request received.
	 */
	activefds_cnt = httpd_select(max_sockfd, &readfds, &active_readfds,
						-1);
	httpd_d("Active FDs count = %d", activefds_cnt);
	if (httpd_stop_req) {
		httpd_d("HTTPD stop request received");
		httpd_stop_req = FALSE;
		httpd_suspend_thread(false);
	}
	/* If activefds_cnt < 0 , it means that httpd_select() returned an
	 * error. There is no need for any further checks and we can directly
	 * return from here.
	 */
	if (activefds_cnt < 0)
		return;
	/* If a TIMEOUT is received it means that all sockets have timed out.
	 * Hence, we close all the client sockets.
	 */
	if (activefds_cnt == HTTPD_TIMEOUT_EVENT) {
		/* Timeout has occurred */
		if (httpd_close_all_client_sockets() != WM_SUCCESS)
			httpd_suspend_thread(true);
		return;
	}
	/* First check if there is any activity on the main HTTP sockets
	 * If yes, we need to first accept the new client connection.
	 */
#ifdef CONFIG_ENABLE_HTTP_SERVER
	if (FD_ISSET(http_sockfd, &active_readfds)) {
		main_sockfd = http_sockfd;
		int sockfd = httpd_accept_client_socket(main_sockfd,
							HTTP_CONN);
		if (sockfd > 0) {
			httpd_d("Client socket accepted: %d", sockfd);
		}
	}
#ifdef CONFIG_IPV6
	if (FD_ISSET(http_ipv6_sockfd, &active_readfds)) {
		main_sockfd = http_ipv6_sockfd;
		int sockfd = httpd_accept_client_socket(main_sockfd,
							HTTP_CONN);
		if (sockfd > 0) {
			httpd_d("Client socket accepted: %d", sockfd);
		}
	}
#endif /* CONFIG_IPV6 */
#endif /* CONFIG_ENABLE_HTTP_SERVER */

#ifdef CONFIG_ENABLE_HTTPS_SERVER
	if (FD_ISSET(https_sockfd, &active_readfds) && (main_sockfd == -1)) {
		main_sockfd = https_sockfd;
		int sockfd = httpd_accept_client_socket(main_sockfd,
							HTTPS_CONN);
		if (sockfd > 0) {
			httpd_d("Client socket accepted: %d", sockfd);
		}
	}
#ifdef CONFIG_IPV6
	if (FD_ISSET(https_ipv6_sockfd, &active_readfds) &&
		(main_sockfd == -1)) {
		main_sockfd = https_ipv6_sockfd;
		int sockfd = httpd_accept_client_socket(main_sockfd,
							HTTPS_CONN);
		if (sockfd > 0) {
			httpd_d("Client socket accepted: %d", sockfd);
		}
	}
#endif /* CONFIG_IPV6 */
#endif /* CONFIG_ENABLE_HTTPS_SERVER */


	/* Check for activity on all client sockets */
	for (i = 0; i < MAX_HTTP_CLIENT_SOCKETS; i++) {
		if (client_sock_data[i].sockfd == -1)
			continue;
		if (FD_ISSET(client_sock_data[i].sockfd, &active_readfds)) {
			int client_sock = client_sock_data[i].sockfd;
			/* Take the mutex before handling the data on the
			 * socket, so that no other entity will try to use it
			 */
			httpd_get_sock_mutex(client_sock, OS_WAIT_FOREVER);
			httpd_d("Handling %d", client_sock);
			/* Note:
			 * Connection will be handled with call to
			 * httpd_handle_message twice, first for
			 * handling request (WM_SUCCESS) and second
			 * time as there is no more data to receive
			 * (client closed connection) and hence
			 * will return with status HTTPD_DONE
			 * closing socket.
			 */
			status = httpd_handle_message(client_sock);
			/* All data has been handled. Release the mutex */
			httpd_put_sock_mutex(client_sock);
			if (status == WM_SUCCESS) {
#ifdef CONFIG_ENABLE_HTTPD_PURGE_LRU
				/* We could have used
				 * httpd_update_sock_timestamp() here.
				 * But since we have access to
				 * client_sock_data[i].timestamp directly,
				 * there is no need to call the API and
				 * introduce a delay.
				 *
				 * TODO: Do something similar for
				 * httpd_put_sock_mutex and
				 * httpd_get_sock_mutex to speed up the socket
				 * handling
				 */
				client_sock_data[i].timestamp =
					os_total_ticks_get();
#endif /* !CONFIG_ENABLE_HTTPD_PURGE_LRU */
				/* The handlers are expecting more data on the
				   socket */
				continue;
			}

			/* Either there was some error or everything went well*/
			httpd_d("Close socket %d.  %s: %d", client_sock,
					status == HTTPD_DONE ?
					"Handler done" : "Handler failed",
					status);

			status = httpd_net_close(client_sock);
			if (status != WM_SUCCESS) {
				status = net_get_sock_error(client_sock);
				httpd_e("Failed to close socket %d", status);
				httpd_suspend_thread(true);
			}
			continue;
		}
	}
}

static void httpd_main(os_thread_arg_t data)
{
	int status;

	status = httpd_setup_main_sockets();
	if (status != WM_SUCCESS)
		httpd_suspend_thread(true);

	while (1) {
		httpd_d("Waiting on sockets");
		httpd_handle_connection();
	}

	/*
	 * Thread will never come here. The functions called from the above
	 * infinite loop will cleanly shutdown this thread when situation
	 * demands so.
	 */
}

static inline int tcp_local_connect(int *sockfd)
{
	uint16_t port;
	int retry_cnt = 3;

	httpd_d("Doing local connect for shutting down server\n\r");

	*sockfd = -1;
	while (retry_cnt--) {
		*sockfd = net_socket(AF_INET, SOCK_STREAM, 0);
		if (*sockfd >= 0)
			break;
		/* Wait some time to allow some sockets to get released */
		os_thread_sleep(os_msec_to_ticks(1000));
	}

	if (*sockfd < 0) {
		httpd_d("Unable to create socket to stop server");
		return -WM_FAIL;
	}


#ifdef CONFIG_ENABLE_HTTPS_SERVER
	port = httpd_get_https_port();
#else
	port = httpd_get_http_port();
#endif /* CONFIG_ENABLE_HTTPS_SERVER */

	char *host = "127.0.0.1";
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(struct sockaddr_in));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = inet_addr(host);
	memset(&(addr.sin_zero), '\0', 8);

	httpd_d("local connecting ...");
	if (connect(*sockfd, (const struct sockaddr *)&addr,
		    sizeof(addr)) != 0) {
		httpd_d("Server close error. tcp connect failed %s:%d",
			host, port);
		net_close(*sockfd);
		*sockfd = 0;
		return -WM_FAIL;
	}

	/*
	 * We do not wish to do anything with this connection. Its sole
	 * purpose was to wake the main httpd thread out of sleep.
	 */

	return WM_SUCCESS;
}


static int httpd_signal_and_wait_for_halt()
{
	const int total_wait_time_ms = 1000 * 20;	/* 20 seconds */
	const int check_interval_ms = 100;	/* 100 ms */

	int num_iterations = total_wait_time_ms / check_interval_ms;

	httpd_d("Sent stop request");
	httpd_stop_req = TRUE;

	/* If the database is full, close the first socket in the client list */
	if (is_socket_db_full())
		httpd_net_close(client_sock_data[0].sockfd);

	/* Do a dummy local connect to wakeup the httpd thread */
	int sockfd;
	int rv = tcp_local_connect(&sockfd);
	if (rv != WM_SUCCESS)
		return rv;

	while (httpd_state != HTTPD_THREAD_SUSPENDED && num_iterations--) {
		os_thread_sleep(os_msec_to_ticks(check_interval_ms));
	}

	net_close(sockfd);
	if (httpd_state == HTTPD_THREAD_SUSPENDED)
		return WM_SUCCESS;

	httpd_d("Timed out waiting for httpd to stop. "
		"Force closed temporary socket");

	httpd_stop_req = FALSE;
	return -WM_FAIL;
}

static int httpd_thread_cleanup(void)
{
	int status = WM_SUCCESS;

	switch (httpd_state) {
	case HTTPD_INIT_DONE:
		/*
		 * We have no threads, no sockets to close.
		 */
		break;
	case HTTPD_THREAD_RUNNING:
		status = httpd_signal_and_wait_for_halt();
		if (status != WM_SUCCESS)
			httpd_e("Unable to stop thread. Force killing it.");
		/* No break here on purpose */
	case HTTPD_THREAD_SUSPENDED:
		status = os_thread_delete(&httpd_main_thread);
		if (status != WM_SUCCESS)
			httpd_d("Failed to delete thread.");
		status = httpd_close_sockets();
		httpd_state = HTTPD_INIT_DONE;
		break;
	default:
		ASSERT(0);
		return -WM_FAIL;
	}

#ifdef CONFIG_ENABLE_HTTPS_SERVER
	if (ssl_ctx) {
		tls_purge_server_context(ssl_ctx);
		ssl_ctx = NULL;
	}
#endif /* CONFIG_ENABLE_HTTPS_SERVER */

	return status;
}

int httpd_is_running(void)
{
	return (httpd_state == HTTPD_THREAD_RUNNING);
}

/* This pairs with httpd_stop() */
int httpd_start(void)
{
	int status;

	if (httpd_state != HTTPD_INIT_DONE) {
		httpd_d("Already started");
		return WM_SUCCESS;
	}

#ifdef CONFIG_ENABLE_HTTPS_SERVER
	if (!ssl_ctx) {
		httpd_d("Please set the certificates by calling "
			"httpd_use_tls_certificates()");
		return -WM_FAIL;
	}
	tls_lib_init();
#endif /* CONFIG_ENABLE_HTTPS_SERVER */

	status = os_thread_create(&httpd_main_thread, "httpd", httpd_main, 0,
			       &httpd_stack, OS_PRIO_3);
	if (status != WM_SUCCESS) {
		httpd_d("Failed to create httpd thread: %d", status);
		return -WM_FAIL;
	}

	httpd_state = HTTPD_THREAD_RUNNING;
	return WM_SUCCESS;
}

/* This pairs with httpd_start() */
int httpd_stop(void)
{
	ASSERT(httpd_state >= HTTPD_THREAD_RUNNING);
	return httpd_thread_cleanup();
}

/* This pairs with httpd_init() */
int httpd_shutdown(void)
{
	int ret;
	ASSERT(httpd_state >= HTTPD_INIT_DONE);

	httpd_d("Shutting down.");

	ret = httpd_thread_cleanup();
	if (ret != WM_SUCCESS)
		httpd_e("Thread cleanup failed");

	httpd_state = HTTPD_INACTIVE;

	return ret;
}

/* This pairs with httpd_shutdown() */
int httpd_init()
{
	int status;

	if (httpd_state != HTTPD_INACTIVE)
		return WM_SUCCESS;

	httpd_d("Initializing");
	int i;
	for (i = 0; i < MAX_HTTP_CLIENT_SOCKETS ; i++) {
		client_sock_data[i].sockfd = -1;
#ifdef CONFIG_ENABLE_HTTPD_PURGE_LRU
		client_sock_data[i].timestamp = 0;
#endif /* !CONFIG_ENABLE_HTTPD_PURGE_LRU */
		client_sock_data[i].sock_data_ptr = NULL;
		client_sock_data[i].free_priv_data_fn = NULL;
		snprintf(client_sock_data[i].mutex_name,
				sizeof(client_sock_data[i].mutex_name),
				"m%d", i);
		if (os_mutex_create(&client_sock_data[i].sock_mutex,
					client_sock_data[i].mutex_name,
					 1) != WM_SUCCESS) {
			/* If a mutex creation fails, delete all the mutexes
			 * created earlier and then return an error.
			 */
			int j;
			for (j = 0; j < i; j++) {
				os_mutex_delete
					(&client_sock_data[j].sock_mutex);
			}
			return -WM_E_NOMEM;
		}
	}
#ifdef CONFIG_ENABLE_HTTP_SERVER
	http_sockfd  = -1;
#ifdef CONFIG_IPV6
	http_ipv6_sockfd  = -1;
#endif /* CONFIG_IPV6 */
#endif /* CONFIG_ENABLE_HTTP_SERVER */
#ifdef CONFIG_ENABLE_HTTPS_SERVER
	https_sockfd  = -1;
#ifdef CONFIG_IPV6
	https_ipv6_sockfd  = -1;
#endif /* CONFIG_IPV6 */
#endif /* CONFIG_ENABLE_HTTPS_SERVER */

	status = httpd_wsgi_init();
	if (status != WM_SUCCESS) {
		httpd_d("Failed to initialize WSGI!");
		return status;
	}

	status = httpd_ssi_init();
	if (status != WM_SUCCESS) {
		httpd_d("Failed to initialize SSI!");
		return status;
	}

	httpd_state = HTTPD_INIT_DONE;

	return WM_SUCCESS;
}

int httpd_use_tls_certificates(const httpd_tls_certs_t *httpd_tls_certs)
{
#ifdef CONFIG_ENABLE_HTTPS_SERVER
	ASSERT(httpd_tls_certs->server_cert != NULL);
	ASSERT(httpd_tls_certs->server_key != NULL);
	/* Client can be NULL */
	httpd_d("Setting TLS certificates");

	tls_lib_init();

	tls_init_config_t tls_server_cfg = {
		.flags = TLS_SERVER_MODE,
		/* From the included header file */
		.tls.server.server_cert = httpd_tls_certs->server_cert,
		.tls.server.server_cert_size = httpd_tls_certs->
		server_cert_size,
		.tls.server.server_key = httpd_tls_certs->server_key,
		.tls.server.server_key_size = httpd_tls_certs->server_key_size,
		.tls.server.client_cert = httpd_tls_certs->client_cert,
		.tls.server.client_cert_size = httpd_tls_certs->
		client_cert_size,
	};

	if (httpd_tls_certs->server_cert_chained)
		tls_server_cfg.flags |= TLS_CERT_BUFFER_CHAINED;

	ssl_ctx = tls_create_server_context(&tls_server_cfg);
	if (!ssl_ctx) {
		httpd_d("Context creation failed");
		return -WM_FAIL;
	}

	return WM_SUCCESS;
#else /* ! CONFIG_ENABLE_HTTPS_SERVER */
	httpd_d("HTTPS is not enabled in server. "
		"Superfluous call to %s", __func__);
	return -WM_FAIL;
#endif /* CONFIG_ENABLE_HTTPS_SERVER */
}
