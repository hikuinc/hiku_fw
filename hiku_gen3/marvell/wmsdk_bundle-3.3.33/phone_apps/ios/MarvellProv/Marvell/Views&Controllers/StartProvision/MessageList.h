//
//  MessageList.h
//  Marvell
//
//  Copyright (c) 2014 Marvell. All rights reserved.
//

#ifndef Marvell_MessageList_h
#define Marvell_MessageList_h

#define  MARVELL_NETWORK_NAME @"wmdemo"

/* response error message */
#define  MARVELL_DHCP_FAILED @"dhcp_failed"
#define  MARVELL_AUTH_FAILED @"auth_failed"
#define  MARVELL_NW_NOT_FOUND @"network_not_found"

/* error messages */
#define  MARVELL_AUTH_FAILED_MSG @"Authentication failure"
#define  MARVELL_DHCP_FAILED_MSG @"DHCP Failure"
#define  MARVELL_NETWORK_NOT_FOUND_MSG @"Network not found"


/* alert messages handling validations */
#define  MARVELL_NO_NETWORK_IPHONE @"Please connect your iPhone to the WiFi network wmdemo-xxxx"
#define  MARVELL_NO_NETWORK_IPOD @"Please connect your iPod to the WiFi network wmdemo-xxxx"
#define  MARVELL_NO_NETWORK_IPAD @"Please connect your iPad to the WiFi network wmdemo-xxxx"
#define  MARVELL_BSSIDNOT_MATCH @"The device has been successfully configured with provided settings. Please check the indicators on the device for the connectivity status"
#define MARVELL_TIMEOUT @"The device has been successfully configured with provided settings. Please check the indicators on the device for the connectivity status"
/* @"Could not confirm if the device is successfully connected to the network" */
#define MARVELL_PROVISIONED @"Device is already provisioned Please connect to another device"
#define MARVELL_RESET @"Reset to provisioning successful"
#define MARVELL_CHOOSE_NETWORK @"Please enter the network name !!!"
#define MARVELL_CHOOSE_PASSOWORD @"Please enter the password !!!"
#define MARVELL_INVALID_SECURITY @"Invalid security! Choose another network"
#define MARVELL_WPA_MINIMUM @"Passphrase should be of minimum 8 characters"
#define MARVELL_RST_To_PROV @"The device is configured with provided settings. However device can’t connect to configured network. Reason:"
#define MARVELL_INCORRECT_DATA @"Incorrect configuration data, please retry the provisioning again"
#define MARVELL_NET_NOTFOUND @"Could not configure network, please check your connection"
#define MARVELL_WPS @"Not a valid device, please check your connection"
#define MARVELL_SUCESS_PROV @"Device is now successfully connected to the network "
#define MARVELL_NETWORK_TIMEOUT @"Could not get list of networks, please check your connection"


#endif
