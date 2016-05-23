# Copyright (C) 2015 Marvell International Ltd.

libs-y += libevrythng

global-cflags-y += -I$(d)/src -DTLSSOCKET

libevrythng-cflags-y := -DTLSSOCKET -DNO_FILESYSTEM  -DNO_PERSISTENCE -DNO_HEAP_TRACKING  -DNOSTACKTRACE

libevrythng-cflags-y += -I$(d)/mqtt/src
libevrythng-objs-y := \
		src/evrythng.c \
		mqtt/src/marvell_api.c \
		mqtt/src/Clients.c \
		mqtt/src/Heap.c \
		mqtt/src/LinkedList.c \
		mqtt/src/Messages.c \
		mqtt/src/MQTTClient.c \
		mqtt/src/MQTTPacket.c \
		mqtt/src/MQTTPacketOut.c \
		mqtt/src/MQTTProtocolClient.c \
		mqtt/src/MQTTProtocolOut.c \
		mqtt/src/SSLSocket.c \
		mqtt/src/TLSSocket.c \
		mqtt/src/Socket.c \
		mqtt/src/SocketBuffer.c \
		mqtt/src/Thread.c \
		mqtt/src/Tree.c \
		mqtt/src/utf-8.c

