# Copyright (C) 2008-2016, Marvell International Ltd.
# All Rights Reserved.

global-cflags-y += -I $(d)/aws_iot_src/protocol/mqtt -I $(d)/aws_iot_src/utils -I $(d)/aws_iot_src/shadow -I $(d)/aws_iot_src/protocol/mqtt/aws_iot_embedded_client_wrapper

-include $(d)/build.aws_iot.mk


