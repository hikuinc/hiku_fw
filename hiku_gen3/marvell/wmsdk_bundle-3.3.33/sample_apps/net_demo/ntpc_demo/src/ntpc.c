/*
 *  Copyright (C) 2008-2016, Marvell International Ltd.
 *  All Rights Reserved.
 */

#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <wmtime.h>
#include <wm_net.h>
#include <string.h>
#include <wm_os.h>

#define EPOCH_TIME_IN_SECS 2208988800U

#define NTP_RESP_TIMEOUT 2

#define RECV_BUF_SIZE 100

/*NTP is port 123*/
#define NTP_PORT 123

#define DEFAULT_MAX_XCHANGE 5



static int ntp_time_get(const char *hostname,
			uint32_t client_transmit_timestamp,
			uint32_t *server_receive_timestamp,
			uint32_t *server_transmit_timestamp)
{

	/* Get the  host address from input name */
	struct hostent *hostinfo = gethostbyname(hostname);
	if (!hostinfo) {
		wmprintf("gethostbyname Failed\r\n");
		return -WM_FAIL;
	}

	/* socket for ntp */
	int ntp_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (ntp_sockfd < 0)
		return -WM_FAIL;

	int one = 1;
	int rv = setsockopt(ntp_sockfd, SOL_SOCKET, SO_REUSEADDR, (char *)&one,
			    sizeof(int));

	struct sockaddr_in server_addr;
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(NTP_PORT);
	server_addr.sin_addr = *((struct in_addr *) hostinfo->h_addr);

	/*
	 * build a message.The message contains all zeros except
	 * for a one in the protocol version field
	 * it should be a total of 48 bytes long
	 */

	unsigned char msg[48] = {010, 0, 0, 0, 0, 0, 0, 0, 0};
	rv = sendto(ntp_sockfd, msg, sizeof(msg), 0,
		    (struct sockaddr *) &server_addr,
		    sizeof(server_addr));
	if (rv == -1) {
		wmprintf("Send Data failed\r\n");
		net_close(ntp_sockfd);
		return -WM_FAIL;
	}

	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(ntp_sockfd, &readfds);

	struct timeval timeout;
	memset(&timeout, 0x00, sizeof(struct timeval));
	timeout.tv_sec = NTP_RESP_TIMEOUT;

	/* Wait for response from NTP server */
	rv = select(ntp_sockfd + 1, &readfds,
		    NULL, NULL,
		    &timeout);
	if (rv <= 0) {
		wmprintf("ntp: no resp received\r\n");
		net_close(ntp_sockfd);
		return -WM_FAIL;
	}

	/* get the data back */
	if (!FD_ISSET(ntp_sockfd, &readfds)) {
		wmprintf("ntp: no read resp received\r\n");
		net_close(ntp_sockfd);
		return -WM_FAIL;
	}

	uint32_t *buf = os_mem_calloc(RECV_BUF_SIZE);
	if (!buf) {
		net_close(ntp_sockfd);
		return -WM_E_NOMEM;
	}

	rv = recv(ntp_sockfd, buf, RECV_BUF_SIZE, MSG_DONTWAIT);
	if (rv <= 0) {
		wmprintf("ntp: recv failed\r\n");
		os_mem_free(buf);
		net_close(ntp_sockfd);
		return -WM_FAIL;
	}

	/* 10th word has the time in seconds */
	*server_transmit_timestamp = ntohl(buf[10]);
	*server_receive_timestamp = ntohl(buf[8]);

	os_mem_free(buf);
	close(ntp_sockfd);

	return WM_SUCCESS;
}

int ntpc_sync(const char *ntp_server, uint32_t max_num_pkt_xchange)
{
	if (!ntp_server)
		return -WM_E_INVAL;

	uint32_t client_transmit_timestamp;
	uint32_t  server_transmit_timestamp;
	uint32_t server_receive_timestamp;
	uint32_t Hardware_timestamp;
	uint32_t cnt;

	if (max_num_pkt_xchange <=  0 || max_num_pkt_xchange > 10)
		max_num_pkt_xchange = DEFAULT_MAX_XCHANGE;

	Hardware_timestamp = wmtime_time_get_posix();
	client_transmit_timestamp = Hardware_timestamp + EPOCH_TIME_IN_SECS;
	/*  first packet*/
	int rv = ntp_time_get(ntp_server,
			      htonl(client_transmit_timestamp),
			      &server_receive_timestamp,
			      &server_transmit_timestamp);
	/* Calculate the Round trip time
	 * for the packet send and  response received
	 */
	uint32_t rtt = (wmtime_time_get_posix() - Hardware_timestamp) +
		(server_transmit_timestamp - server_receive_timestamp);

	for (cnt = 0; cnt < max_num_pkt_xchange; cnt++) {
		client_transmit_timestamp = server_transmit_timestamp + rtt;

		Hardware_timestamp = wmtime_time_get_posix();

		rv = ntp_time_get(ntp_server,
				  client_transmit_timestamp,
				  &server_receive_timestamp,
				  &server_transmit_timestamp);

		if (rv != WM_SUCCESS) {
			return rv;
		}

		rtt = (wmtime_time_get_posix() - Hardware_timestamp) +
			(server_transmit_timestamp - server_receive_timestamp);
	}
	client_transmit_timestamp = server_transmit_timestamp;
	wmtime_time_set_posix(client_transmit_timestamp - EPOCH_TIME_IN_SECS);

	return rv;
}

