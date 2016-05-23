# Copyright (C) 2008-2015 Marvell International Ltd.
# All Rights Reserved.

libs-y += liblwip

liblwip-cflags-y := -Wno-address

liblwip-cflags-y+= -DMAX_SOCKETS_TCP=$(CONFIG_MAX_SOCKETS_TCP)
liblwip-cflags-y+= -DMAX_LISTENING_SOCKETS_TCP=$(CONFIG_MAX_LISTENING_SOCKETS_TCP)
liblwip-cflags-y+= -DMAX_SOCKETS_UDP=$(CONFIG_MAX_SOCKETS_UDP)
liblwip-cflags-y+= -DTCP_SND_BUF_COUNT=$(CONFIG_TCP_SND_BUF_COUNT)
liblwip-cflags-y+= -DTCPIP_STACK_TX_HEAP_SIZE=$(CONFIG_TCPIP_STACK_TX_HEAP_SIZE)


liblwip-objs-y :=  \
		src/api/api_lib.c \
		src/api/api_msg.c \
		src/api/err.c \
		src/api/netbuf.c \
		src/api/netdb.c \
		src/api/netifapi.c \
		src/api/sockets.c \
		src/api/tcpip.c \
		src/core/dhcp.c \
		src/core/dns.c \
		src/core/init.c \
		src/core/ipv4/autoip.c \
		src/core/ipv4/icmp.c \
		src/core/ipv4/igmp.c \
		src/core/ipv4/ip4_addr.c \
		src/core/ipv4/ip4.c \
		src/core/ipv4/ip_frag.c \
		src/core/mem.c \
		src/core/memp.c \
		src/core/netif.c \
		src/core/pbuf.c \
		src/core/raw.c \
		src/core/stats.c \
		src/core/stats_display.c \
		src/core/sys.c \
		src/core/tcp.c \
		src/core/tcp_in.c \
		src/core/tcp_out.c \
		src/core/timers.c \
		src/core/udp.c \
		src/netif/etharp.c \
		contrib/port/FreeRTOS/wmsdk/sys_arch.c \
		src/core/def.c \
		src/core/inet_chksum.c \
		src/netif/ethernetif.c \
		src/core/ipv6/ip6.c \
		src/core/ipv6/nd6.c \
		src/core/ipv6/ethip6.c \
		src/core/ipv6/ip6_frag.c \
		src/core/ipv6/mld6.c \
		src/core/ipv6/dhcp6.c \
		src/core/ipv6/ip6_addr.c \
		src/core/ipv6/inet6.c \
		src/core/ipv6/icmp6.c

