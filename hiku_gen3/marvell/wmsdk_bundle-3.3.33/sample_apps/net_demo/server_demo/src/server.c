/*
 *  Copyright (C) 2008-2016, Marvell International Ltd.
 *  All Rights Reserved.
 */

#include <wm_os.h>
#include <app_framework.h>

#define PORT 80


/* Thread handle */
static os_thread_t server_thread;
/* Buffer to be used as stack */
static os_thread_stack_define(server_stack, 8 * 1024);

/* Thread function */
static void server(os_thread_arg_t data)
{
	int listenfd = 0, connfd = 0;
	struct sockaddr_in serv_addr;

	/*
	 * A Socket is an end point of communication between two systems
	 * on a network.
	 * To be a bit precise, a socket is a combination of IP address
	 * and port on one system. So on each system a socket exists for
	 * a process interacting with the socket on other system over
	 * the network.
	 */

	/* The call to the function ‘net_socket()’ creates an
	 * UN-named socket and returns an integer known as socket
	 * descriptor.
	*/

	/* This function takes domain/family as its first argument.
	 * For Internet family of IPv4 addresses we use AF_INET
	 */

	/*
	  The second argument ‘SOCK_STREAM’ specifies that the
	* transport layer protocol that we want should be reliable ie it
	* should have acknowledgement techniques like TCP.
	*/

	/* The third argument is generally left zero it decides
	 * the default protocol to use for this connection.
	 * For connection oriented reliable connections,
	 * the default protocol used is TCP.
	 */

	listenfd = net_socket(AF_INET, SOCK_STREAM, 0);

	memset(&serv_addr, 0, sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(PORT);

	/* The call to the function ‘net_bind()’ assigns the
	 * details specified in the structure ‘serv_addr’ to
	 * the socket created in the step above. The details include,
	 * the family/domain, the interface to listen on
	 *(in case the system has multiple interfaces to network)
	 * and the port on which the server will wait for the client
	 * requests to come.
	 */
	net_bind(listenfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	/* The call to the function ‘net_listen()’ with second argument
	 * as ’10’ specifies maximum number of client connections that
	 * server will queue for this listening socket.
	 * This call makes the socket a fully functional listening socket.
	 */
	net_listen(listenfd, 10);

	/* HTTP Header with response content */
	char _buf[] =
		"HTTP/1.1 200 OK\r\nContent-Length: 11\r\n\r\nHi Universe";

	while (1) {
		/* The call to net_accept() puts the server to sleep and
		 * when for an incoming client request, the three way
		 * TCP handshake is complete, the function net_accept()
		 * wakes up and returns the socket descriptor representing
		 * the client socket.
		 */

		connfd = net_accept(listenfd, (struct sockaddr *)NULL, NULL);
		/* As soon as server gets a request from client,
		 * it responds and writes on the client socket through
		 * the descriptor returned by net_accept().
		 * net_write() is called to send response.
		 */
		net_write(connfd, _buf, strlen(_buf));
		wmprintf("sendBuff ; %s\r\n", _buf);

		net_close(connfd);
		os_thread_sleep(1);
	}
	os_thread_self_complete(NULL);
	return;
}

int server_start()
{
	int ret = os_thread_create(&server_thread,  /* thread handle */
				   "server",/* thread name */
				   server,  /* entry function */
				   0,       /* argument */
				   &server_stack,  /* stack */
				   OS_PRIO_2);     /* priority - medium low */
	return ret;
}
