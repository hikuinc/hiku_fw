/*
 * wifi_task.c
 *
 * Created: 3/24/2016 5:12:09 PM
 *  Author: Rajan Bala
 */ 

#include "hiku_utils.h"
#include "hiku-tasks.h"
#include "asf.h"
#include "wifi_config.h"
#include <string.h>
#include "bsp/include/nm_bsp.h"
#include "driver/include/m2m_wifi.h"
#include "socket/include/socket.h"

#define STRING_EOL    "\r\n"
#define STRING_HEADER "-- WINC1500 send email example --"STRING_EOL \
"-- "BOARD_NAME " --"STRING_EOL	\
"-- Compiled: "__DATE__ " "__TIME__ " --"STRING_EOL

/** IP address of host. */
uint32_t gu32HostIp = 0;

uint8_t gu8SocketStatus = SocketInit;

/** SMTP information. */
uint8_t gu8SmtpStatus = SMTP_INIT;

/** SMTP email error information. */
int8_t gs8EmailError = MAIN_EMAIL_ERROR_NONE;

/** Send and receive buffer definition. */
char gcSendRecvBuffer[MAIN_SMTP_BUF_LEN];

/** Handler buffer definition. */
char gcHandlerBuffer[MAIN_SMTP_BUF_LEN];

/** Username basekey definition. */
char gcUserBasekey[128];

/** Password basekey definition. */
char gcPasswordBasekey[128];

/** Retry count. */
uint8_t gu8RetryCount = 0;

/** TCP client socket handler. */
static SOCKET tcp_client_socket = -1;

/** Wi-Fi status variable. */
static bool gbConnectedWifi = false;

/** Get host IP status variable. */
static bool gbHostIpByName = false;

//------------------------ function declarations -----------------------

void wifi_task(void *prvParameters);
static void hw_wifi_init(void);
static void generateBase64Key(char *input, char *basekey);
extern void ConvertToBase64(char *pcOutStr, const char *pccInStr, int iLen);
static void resolve_cb(uint8_t *hostName, uint32_t hostIp);
static void socket_cb(SOCKET sock, uint8_t u8Msg, void *pvMsg);
static void wifi_cb(uint8_t u8MsgType, void *pvMsg);
static void close_socket(void);





//--------------- function implementations ----------------------

void create_wifi_task(Twi *twi_base, uint16_t stack_depth_words,
unsigned portBASE_TYPE task_priority,
portBASE_TYPE set_asynchronous_api)
{
	/* Register the default CLI commands. */
	//vRegisterCLICommands();
	
	hw_wifi_init();

	/* Create the USART CLI task. */
	xTaskCreate(	wifi_task,			/* The task that implements the command console. */
	(const int8_t *const) "WIFI_TSK",	/* Text name assigned to the task.  This is just to assist debugging.  The kernel does not use this name itself. */
	stack_depth_words,					/* The size of the stack allocated to the task. */
	NULL,			/* The parameter is used to pass the already configured USART port into the task. */
	task_priority,						/* The priority allocated to the task. */
	NULL);	
}
portBASE_TYPE did_wifi_test_pass(void)
{
	return 0;
}

void wifi_task(void *prvParameters)
{
	

	/* Connect to router. */
	m2m_wifi_connect((char *)MAIN_WLAN_SSID, sizeof(MAIN_WLAN_SSID),
	MAIN_WLAN_AUTH, (char *)MAIN_WLAN_PSK, M2M_WIFI_CH_ALL);

	while (1) {
		m2m_wifi_handle_events(NULL);

		if (gbConnectedWifi && gbHostIpByName) {
			if (gu8SocketStatus == SocketInit) {
				if (tcp_client_socket < 0) {
					gu8SocketStatus = SocketWaiting;
					//if (smtpConnect() != SOCK_ERR_NO_ERROR) {
						gu8SocketStatus = SocketInit;
					//}
				}
				} else if (gu8SocketStatus == SocketConnect) {
				gu8SocketStatus = SocketWaiting;
				/*
				if (smtpStateHandler() != MAIN_EMAIL_ERROR_NONE) {
					if (gs8EmailError == MAIN_EMAIL_ERROR_INIT) {
						gu8SocketStatus = SocketError;
						} else {
						close_socket();
						break;
					}
				}
				*/
				} else if (gu8SocketStatus == SocketComplete) {
				printf("main: Email was successfully sent.\r\n");
				//ioport_set_pin_level(LED_0_PIN, false);
				close_socket();
				break;
				} else if (gu8SocketStatus == SocketError) {
				if (gu8RetryCount < MAIN_RETRY_COUNT) {
					gu8RetryCount++;
					printf("main: Waiting to connect server.(30 seconds)\r\n\r\n");
					//retry_smtp_server();
					} else {
					printf("main: Failed retry to server. Press reset your board.\r\n");
					gu8RetryCount = 0;
					close_socket();
					break;
				}
			}
		}
	}

	return 0;
}

static void hw_wifi_init(void)
{
	tstrWifiInitParam param;
	int8_t ret;

	/* Initialize the board. */
	sysclk_init();
	board_init();

	/* Initialize the UART console. */
	printf(STRING_HEADER);

	/* Initialize the BSP. */
	nm_bsp_init();

	/* Initialize Wi-Fi parameters structure. */
	memset((uint8_t *)&param, 0, sizeof(tstrWifiInitParam));

	/* Initialize Wi-Fi driver with data and status callbacks. */
	param.pfAppWifiCb = wifi_cb;
	ret = m2m_wifi_init(&param);
	if (M2M_SUCCESS != ret) {
		printf("main: m2m_wifi_init call error!(%d)\r\n", ret);
		while (1) {
		}
	}

	/* Initialize Socket module */
	socketInit();
	registerSocketCallback(socket_cb, resolve_cb);
}



// utilities

/**
 * \brief Generates Base64 Key needed for authentication.
 *
 * \param[in] input is the string to be converted to base64.
 * \param[in] basekey1 is the base64 converted output.
 *
 * \return None.
 */
static void generateBase64Key(char *input, char *basekey)
{
	/* In case the input string needs to be modified before conversion, define */
	/*  new string to pass-through Use InputStr and *pIn */
	int16_t InputLen = strlen(input);
	char InputStr[128];
	char *pIn = (char *)InputStr;

	/* Generate Base64 string, right now is only the function input parameter */
	memcpy(pIn, input, InputLen);
	pIn += InputLen;

	/* to64frombits function */
	ConvertToBase64(basekey, (void *)InputStr, InputLen);
}

/**
 * \brief Callback function of IP address.
 *
 * \param[in] hostName Domain name.
 * \param[in] hostIp Server IP.
 *
 * \return None.
 */
static void resolve_cb(uint8_t *hostName, uint32_t hostIp)
{
	gu32HostIp = hostIp;
	gbHostIpByName = true;
	printf("Host IP is %d.%d.%d.%d\r\n", (int)IPV4_BYTE(hostIp, 0), (int)IPV4_BYTE(hostIp, 1),
			(int)IPV4_BYTE(hostIp, 2), (int)IPV4_BYTE(hostIp, 3));
	printf("Host Name is %s\r\n", hostName);
}

/**
 * \brief Callback function of TCP client socket.
 *
 * \param[in] sock socket handler.
 * \param[in] u8Msg Type of Socket notification
 * \param[in] pvMsg A structure contains notification informations.
 *
 * \return None.
 */
static void socket_cb(SOCKET sock, uint8_t u8Msg, void *pvMsg)
{
	/* Check for socket event on TCP socket. */
	if (sock == tcp_client_socket) {
		switch (u8Msg) {
		case SOCKET_MSG_CONNECT:
		{
			tstrSocketConnectMsg *pstrConnect = (tstrSocketConnectMsg *)pvMsg;
			if (pstrConnect && pstrConnect->s8Error >= SOCK_ERR_NO_ERROR) {
				memset(gcHandlerBuffer, 0, MAIN_SMTP_BUF_LEN);
				recv(tcp_client_socket, gcHandlerBuffer, sizeof(gcHandlerBuffer), 0);
			} else {
				printf("SOCKET_MSG_CONNECT : connect error!\r\n");
				gu8SocketStatus = SocketError;
			}
		}
		break;

		case SOCKET_MSG_SEND:
		{
			switch (gu8SmtpStatus) {
			case SMTP_MESSAGE_SUBJECT:
				gu8SocketStatus = SocketConnect;
				gu8SmtpStatus = SMTP_MESSAGE_TO;
				break;

			case SMTP_MESSAGE_TO:
				gu8SocketStatus = SocketConnect;
				gu8SmtpStatus = SMTP_MESSAGE_FROM;
				break;

			case SMTP_MESSAGE_FROM:
				gu8SocketStatus = SocketConnect;
				gu8SmtpStatus = SMTP_MESSAGE_CRLF;
				break;

			case SMTP_MESSAGE_CRLF:
				gu8SocketStatus = SocketConnect;
				gu8SmtpStatus = SMTP_MESSAGE_BODY;
				break;

			case SMTP_MESSAGE_BODY:
				gu8SocketStatus = SocketConnect;
				gu8SmtpStatus = SMTP_MESSAGE_DATAEND;
				break;

			case SMTP_QUIT:
				gu8SocketStatus = SocketComplete;
				gu8SmtpStatus = SMTP_INIT;
				break;

			default:
				break;
			}
		}
		break;

		case SOCKET_MSG_RECV:
		{
			tstrSocketRecvMsg *pstrRecv = (tstrSocketRecvMsg *)pvMsg;

			if (gu8SocketStatus == SocketWaiting) {
				gu8SocketStatus = SocketConnect;
				switch (gu8SmtpStatus) {
				case SMTP_INIT:
					if (pstrRecv && pstrRecv->s16BufferSize > 0) {
						/* If buffer has 220 'OK' from server, set state to HELO */
						if (pstrRecv->pu8Buffer[0] == cSmtpCodeReady[0] &&
								pstrRecv->pu8Buffer[1] == cSmtpCodeReady[1] &&
								pstrRecv->pu8Buffer[2] == cSmtpCodeReady[2]) {
							gu8SmtpStatus = SMTP_HELO;
						} else {
							printf("No response from server.\r\n");
							gu8SmtpStatus = SMTP_ERROR;
							gs8EmailError = MAIN_EMAIL_ERROR_INIT;
						}
					} else {
						printf("SMTP_INIT : recv error!\r\n");
						gu8SmtpStatus = SMTP_ERROR;
						gs8EmailError = MAIN_EMAIL_ERROR_INIT;
					}

					break;

				case SMTP_HELO:
					if (pstrRecv && pstrRecv->s16BufferSize > 0) {
						/* If buffer has 220, set state to HELO */
						if (pstrRecv->pu8Buffer[0] == cSmtpCodeOkReply[0] &&
								pstrRecv->pu8Buffer[1] == cSmtpCodeOkReply[1] &&
								pstrRecv->pu8Buffer[2] == cSmtpCodeOkReply[2]) {
							gu8SmtpStatus = SMTP_AUTH;
						} else {
							printf("No response for HELO.\r\n");
							gu8SmtpStatus = SMTP_ERROR;
							gs8EmailError = MAIN_EMAIL_ERROR_HELO;
						}
					} else {
						printf("SMTP_HELO : recv error!\r\n");
						gu8SmtpStatus = SMTP_ERROR;
						gs8EmailError = MAIN_EMAIL_ERROR_HELO;
					}

					break;

				case SMTP_AUTH:
					if (pstrRecv && pstrRecv->s16BufferSize > 0) {
						/* Function handles authentication for all services */
						generateBase64Key((char *)MAIN_FROM_ADDRESS, gcUserBasekey);

						/* If buffer is 334, give username in base64 */
						if (pstrRecv->pu8Buffer[0] == cSmtpCodeAuthReply[0] &&
								pstrRecv->pu8Buffer[1] == cSmtpCodeAuthReply[1] &&
								pstrRecv->pu8Buffer[2] == cSmtpCodeAuthReply[2]) {
							gu8SmtpStatus = SMTP_AUTH_USERNAME;
						} else {
							printf("No response for authentication.\r\n");
							gu8SmtpStatus = SMTP_ERROR;
							gs8EmailError = MAIN_EMAIL_ERROR_AUTH;
						}
					} else {
						printf("SMTP_AUTH : recv error!\r\n");
						gu8SmtpStatus = SMTP_ERROR;
						gs8EmailError = MAIN_EMAIL_ERROR_AUTH;
					}

					break;

				case SMTP_AUTH_USERNAME:
					if (pstrRecv && pstrRecv->s16BufferSize > 0) {
						generateBase64Key((char *)MAIN_FROM_PASSWORD, gcPasswordBasekey);

						/* If buffer is 334, give password in base64 */
						if (pstrRecv->pu8Buffer[0] == cSmtpCodeAuthReply[0] &&
								pstrRecv->pu8Buffer[1] == cSmtpCodeAuthReply[1] &&
								pstrRecv->pu8Buffer[2] == cSmtpCodeAuthReply[2]) {
							gu8SmtpStatus = SMTP_AUTH_PASSWORD;
						} else {
							printf("No response for username authentication.\r\n");
							gu8SmtpStatus = SMTP_ERROR;
							gs8EmailError = MAIN_EMAIL_ERROR_AUTH_USERNAME;
						}
					} else {
						printf("SMTP_AUTH_USERNAME : recv error!\r\n");
						gu8SmtpStatus = SMTP_ERROR;
						gs8EmailError = MAIN_EMAIL_ERROR_AUTH_USERNAME;
					}

					break;

				case SMTP_AUTH_PASSWORD:
					if (pstrRecv && pstrRecv->s16BufferSize > 0) {
						if (pstrRecv->pu8Buffer[0] == cSmtpCodeAuthSuccess[0] &&
								pstrRecv->pu8Buffer[1] == cSmtpCodeAuthSuccess[1] &&
								pstrRecv->pu8Buffer[2] == cSmtpCodeAuthSuccess[2]) {
							/* Authentication was successful, set state to FROM */
							gu8SmtpStatus = SMTP_FROM;
						} else {
							printf("No response for password authentication.\r\n");
							gu8SmtpStatus = SMTP_ERROR;
							gs8EmailError = MAIN_EMAIL_ERROR_AUTH_PASSWORD;
						}
					} else {
						printf("SMTP_AUTH_PASSWORD : recv error!\r\n");
						gu8SmtpStatus = SMTP_ERROR;
						gs8EmailError = MAIN_EMAIL_ERROR_AUTH_PASSWORD;
					}

					break;

				case SMTP_FROM:
					if (pstrRecv && pstrRecv->s16BufferSize > 0) {
						/* If buffer has 250, set state to RCPT */
						if (pstrRecv->pu8Buffer[0] == cSmtpCodeOkReply[0] &&
								pstrRecv->pu8Buffer[1] == cSmtpCodeOkReply[1] &&
								pstrRecv->pu8Buffer[2] == cSmtpCodeOkReply[2]) {
							gu8SmtpStatus = SMTP_RCPT;
						} else {
							printf("No response for sender transmission.\r\n");
							gu8SmtpStatus = SMTP_ERROR;
							gs8EmailError = MAIN_EMAIL_ERROR_FROM;
						}
					} else {
						printf("SMTP_FROM : recv error!\r\n");
						gu8SmtpStatus = SMTP_ERROR;
						gs8EmailError = MAIN_EMAIL_ERROR_FROM;
					}

					break;

				case SMTP_RCPT:
					if (pstrRecv && pstrRecv->s16BufferSize > 0) {
						/* If buffer has 250, set state to DATA */
						if (pstrRecv->pu8Buffer[0] == cSmtpCodeOkReply[0] &&
								pstrRecv->pu8Buffer[1] == cSmtpCodeOkReply[1] &&
								pstrRecv->pu8Buffer[2] == cSmtpCodeOkReply[2]) {
							gu8SmtpStatus = SMTP_DATA;
						} else {
							printf("No response for recipient transmission.\r\n");
							gu8SmtpStatus = SMTP_ERROR;
							gs8EmailError = MAIN_EMAIL_ERROR_RCPT;
						}
					} else {
						printf("SMTP_RCPT : recv error!\r\n");
						gu8SmtpStatus = SMTP_ERROR;
						gs8EmailError = MAIN_EMAIL_ERROR_RCPT;
					}

					break;

				case SMTP_DATA:
					if (pstrRecv && pstrRecv->s16BufferSize > 0) {
						/* If buffer has 250, set state to DATA */
						if (pstrRecv->pu8Buffer[0] == cSmtpCodeIntermedReply[0] &&
								pstrRecv->pu8Buffer[1] == cSmtpCodeIntermedReply[1] &&
								pstrRecv->pu8Buffer[2] == cSmtpCodeIntermedReply[2]) {
							gu8SmtpStatus = SMTP_MESSAGE_SUBJECT;
						} else {
							printf("No response for data transmission.\r\n");
							gu8SmtpStatus = SMTP_ERROR;
							gs8EmailError = MAIN_EMAIL_ERROR_DATA;
						}
					} else {
						printf("SMTP_DATA : recv error!\r\n");
						gu8SmtpStatus = SMTP_ERROR;
						gs8EmailError = MAIN_EMAIL_ERROR_DATA;
					}

					break;

				case SMTP_MESSAGE_DATAEND:
					if (pstrRecv && pstrRecv->s16BufferSize > 0) {
						/* If buffer has 250, set state to DATA */
						if (pstrRecv->pu8Buffer[0] == cSmtpCodeOkReply[0] &&
								pstrRecv->pu8Buffer[1] == cSmtpCodeOkReply[1] &&
								pstrRecv->pu8Buffer[2] == cSmtpCodeOkReply[2]) {
							gu8SmtpStatus = SMTP_QUIT;
						} else {
							printf("No response for dataend transmission.\r\n");
							gu8SmtpStatus = SMTP_ERROR;
							gs8EmailError = MAIN_EMAIL_ERROR_MESSAGE;
						}
					} else {
						printf("SMTP_MESSAGE_DATAEND : recv error!\r\n");
						gu8SmtpStatus = SMTP_ERROR;
						gs8EmailError = MAIN_EMAIL_ERROR_MESSAGE;
					}

					break;

				default:
					break;
				}
			}
		}
		break;

		default:
			break;
		}
	}
}

/**
 * \brief Callback to get the Wi-Fi status update.
 *
 * \param[in] u8MsgType Type of Wi-Fi notification.
 * \param[in] pvMsg A pointer to a buffer containing the notification parameters.
 *
 * \return None.
 */
static void wifi_cb(uint8_t u8MsgType, void *pvMsg)
{
	switch (u8MsgType) {
	case M2M_WIFI_RESP_CON_STATE_CHANGED:
	{
		tstrM2mWifiStateChanged *pstrWifiState = (tstrM2mWifiStateChanged *)pvMsg;
		if (pstrWifiState->u8CurrState == M2M_WIFI_CONNECTED) {
			printf("wifi_cb: M2M_WIFI_RESP_CON_STATE_CHANGED: CONNECTED\r\n");
			m2m_wifi_request_dhcp_client();
		} else if (pstrWifiState->u8CurrState == M2M_WIFI_DISCONNECTED) {
			printf("wifi_cb: M2M_WIFI_RESP_CON_STATE_CHANGED: DISCONNECTED\r\n");
			gbConnectedWifi = false;
			gbHostIpByName = false;
			m2m_wifi_connect((char *)MAIN_WLAN_SSID, sizeof(MAIN_WLAN_SSID),
					MAIN_WLAN_AUTH, (char *)MAIN_WLAN_PSK, M2M_WIFI_CH_ALL);
		}

		break;
	}

	case M2M_WIFI_REQ_DHCP_CONF:
	{
		uint8_t *pu8IPAddress = (uint8_t *)pvMsg;
		/* Turn LED0 on to declare that IP address received. */
		printf("wifi_cb: M2M_WIFI_REQ_DHCP_CONF: IP is %u.%u.%u.%u\r\n",
				pu8IPAddress[0], pu8IPAddress[1], pu8IPAddress[2], pu8IPAddress[3]);
		gbConnectedWifi = true;

		/* Obtain the IP Address by network name */
		gethostbyname((uint8_t *)MAIN_GMAIL_HOST_NAME);
		break;
	}

	default:
	{
		break;
	}
	}
}

/**
 * \brief Close socket function.
 * \return None.
 */
static void close_socket(void)
{
	close(tcp_client_socket);
	tcp_client_socket = -1;
}