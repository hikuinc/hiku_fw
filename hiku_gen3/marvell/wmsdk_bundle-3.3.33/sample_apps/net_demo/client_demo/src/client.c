/*
 *  Copyright (C) 2008-2016, Marvell International Ltd.
 *  All Rights Reserved.
 */

#include <wm_os.h>
#include <app_framework.h>
#include <cli.h>
#include <wm_os.h>

#define PORT 80

#define MAXBUF          128

int client_demo(char *hostname)
{
	int sockfd;
	/* Get host address from the input name */
	struct hostent *hostinfo = gethostbyname(hostname);
	if (!hostinfo) {
		wmprintf("gethostbyname Failed\r\n");
		return -WM_FAIL;
	}

	struct sockaddr_in dest;

	char buffer[MAXBUF];
	/* Create a socket */
	/*---Open socket for streaming---*/
	if ((sockfd = net_socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		wmprintf("Error in socket\r\n");
		return -WM_FAIL;
	}

	/*---Initialize server address/port struct---*/
	bzero(&dest, sizeof(dest));
	dest.sin_family = AF_INET;
	dest.sin_port = htons(PORT);
	dest.sin_addr = *((struct in_addr *) hostinfo->h_addr);
	char ip[16];
	uint32_t address = dest.sin_addr.s_addr;
	net_inet_ntoa(address, ip);

	wmprintf("Server ip Address : %s\r\n", ip);
	/*---Connect to server---*/
	if (net_connect(sockfd,
			 (struct sockaddr *)&dest,
			 sizeof(dest)) != 0) {
		wmprintf("Error in connect\r\n");
		return -WM_FAIL;
	}
	/*---Get "Hello?"---*/
	bzero(buffer, MAXBUF);
	char wbuf[]
		= "GET / HTTP/1.1\r\nHost: www.marvell.com\r\nUser-Agent: wmsdk\r\nAccept: */*\r\n\r\n";
	net_write(sockfd, wbuf, sizeof(wbuf) - 1);
	net_read(sockfd, buffer, sizeof(buffer));
	/* buffer[MAXBUF - 1] = '\0'; */
	wmprintf("-------------------------\r\n");
	wmprintf("Note : 128 bytes read\r\n");
	wmprintf("-------------------------\r\n");
	wmprintf("%s", buffer);
	/*---Clean up---*/
	close(sockfd);
	return 0;
}


static void cmd_client_cli(int argc, char **argv)
{
	if (argc !=  2) {
		wmprintf("invalid arguments\r\n");
		return;
	}
	client_demo(argv[1]);
}

static struct cli_command client_cli_cmds[] = {
	{"client-connect",
	 "<host name>",
	 cmd_client_cli}
};

int client_cli_init()
{
	return cli_register_command(&client_cli_cmds[0]);
}


	

