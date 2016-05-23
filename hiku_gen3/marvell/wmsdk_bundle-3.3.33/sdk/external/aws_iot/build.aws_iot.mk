# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

libs-y += libaws_iot

libaws_iot-objs-y := aws_mqtt_embedded_client_lib/MQTTPacket/src/MQTTSerializePublish.c \
	aws_mqtt_embedded_client_lib/MQTTPacket/src/MQTTUnsubscribeClient.c \
	aws_mqtt_embedded_client_lib/MQTTPacket/src/MQTTConnectClient.c \
	aws_mqtt_embedded_client_lib/MQTTPacket/src/MQTTPacket.c \
	aws_mqtt_embedded_client_lib/MQTTPacket/src/MQTTSubscribeClient.c \
	aws_mqtt_embedded_client_lib/MQTTPacket/src/MQTTFormat.c \
	aws_mqtt_embedded_client_lib/MQTTPacket/src/MQTTSubscribeServer.c \
	aws_mqtt_embedded_client_lib/MQTTPacket/src/MQTTConnectServer.c \
	aws_mqtt_embedded_client_lib/MQTTPacket/src/MQTTUnsubscribeServer.c \
	aws_mqtt_embedded_client_lib/MQTTPacket/src/MQTTDeserializePublish.c \
	aws_mqtt_embedded_client_lib/MQTTClient-C/src/MQTTClient.c \
	aws_iot_src/protocol/mqtt/aws_iot_embedded_client_wrapper/aws_iot_mqtt_embedded_client_wrapper.c \
	aws_iot_src/utils/aws_iot_json_utils.c \
	aws_iot_src/protocol/mqtt/aws_iot_embedded_client_wrapper/platform_wmsdk/network_interface.c \
	aws_iot_src/shadow/aws_iot_shadow_json.c \
	aws_iot_src/shadow/aws_iot_shadow_actions.c \
	aws_iot_src/shadow/aws_iot_shadow.c \
	aws_iot_src/shadow/aws_iot_shadow_records.c \
	aws_iot_src/protocol/mqtt/aws_iot_embedded_client_wrapper/platform_wmsdk/timer.c \

libaws_iot-cflags-y := -I $(d)/aws_iot_src/protocol/mqtt/aws_iot_embedded_client_wrapper -I $(d)/aws_iot_src/shadow -I $(d)aws_iot_src/protocol/mqtt -I $(d)/aws_iot_src/utils -I $(d)/aws_mqtt_embedded_client_lib/MQTTPacket/src -I $(d)/aws_mqtt_embedded_client_lib/MQTTClient-C/src
