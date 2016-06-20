/*
 * wifi_config.h
 *
 * Created: 3/24/2016 5:11:04 PM
 *  Author: Rajan Bala
 */ 


#ifndef WIFI_CONFIG_H_
#define WIFI_CONFIG_H_


#ifdef __cplusplus
extern "C" {
#endif

/** Wi-Fi Settings */
#define MAIN_WLAN_SSID                  "SVG" /**< Destination SSID */
#define MAIN_WLAN_AUTH                  M2M_WIFI_SEC_WPA_PSK /**< Security manner */
#define MAIN_WLAN_PSK                   "accesssvg" /**< Password for Destination SSID */

/** Using IP address. */
#define IPV4_BYTE(val, index)           ((val >> (index * 8)) & 0xFF)

/** All SMTP defines */
#define MAIN_SMTP_BUF_LEN               1024
#define MAIN_GMAIL_HOST_NAME            "smtp.gmail.com"
#define MAIN_GMAIL_HOST_PORT            465
#define MAIN_SENDER_RFC                 "rajan.bala@gmail.com" /* Set Sender Email Address */
#define MAIN_RECIPIENT_RFC              "rajan@hiku.us"  /* Set Recipient Email Address */
#define MAIN_EMAIL_SUBJECT              "Hello from WINC1500!"
#define MAIN_TO_ADDRESS                 "rajan@hiku.us" /* Set To Email Address */
#define MAIN_FROM_ADDRESS               "rajan.bala@gmail.com" /* Set From Email Address */
#define MAIN_FROM_PASSWORD              "BLAHBAH"              /* Set Sender Email Password */
#define MAIN_EMAIL_MSG                  "This mail is sent from Send Email Example."
#define MAIN_WAITING_TIME               30000
#define MAIN_RETRY_COUNT                3

typedef enum {
	SocketInit = 0,
	SocketConnect,
	SocketWaiting,
	SocketComplete,
	SocketError
} eSocketStatus;

typedef enum {
	SMTP_INACTIVE = 0,
	SMTP_INIT,
	SMTP_HELO,
	SMTP_AUTH,
	SMTP_AUTH_USERNAME,
	SMTP_AUTH_PASSWORD,
	SMTP_FROM,
	SMTP_RCPT,
	SMTP_DATA,
	SMTP_MESSAGE_SUBJECT,
	SMTP_MESSAGE_TO,
	SMTP_MESSAGE_FROM,
	SMTP_MESSAGE_CRLF,
	SMTP_MESSAGE_BODY,
	SMTP_MESSAGE_DATAEND,
	SMTP_QUIT,
	SMTP_ERROR
} eSMTPStatus;

/* Initialize error handling flags for smtp state machine */
typedef enum {
	MAIN_EMAIL_ERROR_FAILED = -1,
	MAIN_EMAIL_ERROR_NONE = 0,
	MAIN_EMAIL_ERROR_INIT,
	MAIN_EMAIL_ERROR_HELO,
	MAIN_EMAIL_ERROR_AUTH,
	MAIN_EMAIL_ERROR_AUTH_USERNAME,
	MAIN_EMAIL_ERROR_AUTH_PASSWORD,
	MAIN_EMAIL_ERROR_FROM,
	MAIN_EMAIL_ERROR_RCPT,
	MAIN_EMAIL_ERROR_DATA,
	MAIN_EMAIL_ERROR_MESSAGE,
	MAIN_EMAIL_ERROR_QUIT
} eMainEmailError;

/** Return Codes */
const char cSmtpCodeReady[] = {'2', '2', '0', '\0'};
const char cSmtpCodeOkReply[] = {'2', '5', '0', '\0'};
const char cSmtpCodeIntermedReply[] = {'3', '5', '4', '\0'};
const char cSmtpCodeAuthReply[] = {'3', '3', '4', '\0'};
const char cSmtpCodeAuthSuccess[] = {'2', '3', '5', '\0'};

/** Send Codes */
const char cSmtpHelo[] = {'H', 'E', 'L', 'O', '\0'};
const char cSmtpMailFrom[] = {'M', 'A', 'I', 'L', ' ', 'F', 'R', 'O', 'M', ':', ' ', '\0'};
const char cSmtpRcpt[] = {'R', 'C', 'P', 'T', ' ', 'T', 'O', ':', ' ', '\0'};
const char cSmtpData[] = "DATA";
const char cSmtpCrlf[] = "\r\n";
const char cSmtpSubject[] = "Subject: ";
const char cSmtpTo[] = "To: ";
const char cSmtpFrom[] = "From: ";
const char cSmtpDataEnd[] = {'\r', '\n', '.', '\r', '\n', '\0'};
const char cSmtpQuit[] = {'Q', 'U', 'I', 'T', '\r', '\n', '\0'};

#ifdef __cplusplus
}
#endif

#endif /* WIFI_CONFIG_H_ */