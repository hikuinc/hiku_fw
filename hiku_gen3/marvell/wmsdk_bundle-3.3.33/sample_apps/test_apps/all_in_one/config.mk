# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

######################### all_in_one configuration #######################

ALL_IN_ONE_CONFIG_MDNS_ENABLE=y
ALL_IN_ONE_CONFIG_WPS_ENABLE=y
ALL_IN_ONE_CONFIG_PM_ENABLE=n
ALL_IN_ONE_CONFIG_OVERLAY_ENABLE=n
ALL_IN_ONE_CONFIG_HTTPS_CLOUD=n

# Select the type of cloud to be enabled with all_in_one
# Set WEBSOCKET_CLOUD to y for websocket based cloud
# Set LONG_POLL_CLOUD to y for long polling based cloud
# Set XIVELY_CLOUD to y for xively cloud
# Set ARRAYENT_CLOUD to y for Arrayent cloud
# Make sure that only of these options is enabled at a time
ALL_IN_ONE_WEBSOCKET_CLOUD = y
#ALL_IN_ONE_LONG_POLL_CLOUD = y
#ALL_IN_ONE_XIVELY_CLOUD = y
#ALL_IN_ONE_ARRAYENT_CLOUD = y

# Ezconnect provisioning mode
ALL_IN_ONE_CONFIG_PROV_EZCONNECT=n

# HTTPS Server support
ifeq ($(CONFIG_ENABLE_HTTPS_SERVER), y)
ALL_IN_ONE_CONFIG_HTTPS_SERVER=y
endif
#####################################################################
