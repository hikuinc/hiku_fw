# Copyright (C) 2008-2016, Marvell International Ltd.
# All Rights Reserved.

subdir-y                         += hello_world
ifneq ($(CONFIG_ENABLE_CPP_SUPPORT),)
ifneq ($(CONFIG_ENABLE_MCU_PM3),y)
subdir-$(CONFIG_CPU_MW300)       += hello_world_cpp
endif
endif
subdir-y                         += baremetal
subdir-y                         += cert_demo
subdir-y                         += perf_demo
subdir-y                         += assembly_demo

subdir-y                         += cloud_demo/arrayent_demo
subdir-y                         += cloud_demo/aws_demo
subdir-y                         += cloud_demo/evrythng_demo
subdir-y                         += cloud_demo/xively_demo

subdir-y                         += io_demo/adc
subdir-y                         += io_demo/gpio
subdir-y                         += io_demo/dac_demo
subdir-y                         += io_demo/i2c/advanced/master_demo
subdir-y                         += io_demo/i2c/advanced/slave_demo
subdir-y                         += io_demo/i2c/simple/master_demo
subdir-y                         += io_demo/i2c/simple/slave_demo
subdir-y                         += io_demo/ssp/simple/master_demo
subdir-y                         += io_demo/ssp/simple/slave_demo
subdir-y                         += io_demo/ssp/ssp_full_duplex_demo
subdir-y                         += io_demo/uart/uart_dma_rx_demo
subdir-y                         += io_demo/uart/uart_dma_tx_demo
subdir-y                         += io_demo/uart/uart_echo_demo
subdir-$(CONFIG_CPU_MW300)	 += power_profiles/deepsleep_pm4
subdir-$(CONFIG_CPU_MW300)	 += power_profiles/pdn_pm4
subdir-$(CONFIG_CPU_MW300)       += test_apps/io_demo/uart/uart_test
subdir-$(CONFIG_CPU_MW300)       += test_apps/io_demo/i2c/i2c_test
subdir-$(CONFIG_CPU_MW300)       += test_apps/io_demo/ssp/ssp_test
subdir-$(CONFIG_USB_DRIVER)      += io_demo/usb/usb_client_cdc
ifdef USB_HOST_PATH
 subdir-y                        += io_demo/usb/usb_host_cdc
 subdir-y                        += io_demo/usb/usb_host_uac
 subdir-y                        += io_demo/usb/usb_host_uvc
 subdir-y                        += io_demo/usb/usb_host_msc
endif

subdir-y                         += os_api_demo/mutex_demo
subdir-y                         += os_api_demo/semaphore_demo
subdir-y                         += os_api_demo/timer_demo
subdir-y                         += module_demo/cli_demo
subdir-y                         += module_demo/psm_demo
subdir-y                         += module_demo/webhandler_demo
subdir-y                         += module_demo/websocket_demo
subdir-y                         += module_demo/fw_upgrade/secure_fw_upgrade_demo
subdir-y                         += module_demo/fw_upgrade/low_level_fw_upgrade_demo
subdir-y                         += module_demo/fw_upgrade/fw_upgrade_demo
subdir-$(CONFIG_ENABLE_HTTPC_SECURE) += module_demo/httpc_demo
subdir-$(CONFIG_ENABLE_HTTPS_SERVER) += module_demo/httpd_secure_demo

subdir-y                         += mfg/uart_wifi_bridge
subdir-$(CONFIG_BT_SUPPORT)      += mfg/hci_bridge
subdir-y                         += mfg/fcc_app
subdir-y                         += net_demo/ntpc_demo
subdir-y                         += net_demo/server_demo
subdir-y                         += net_demo/client_demo

subdir-y                         += power_measure_demo/pm_mcu_wifi_demo
subdir-$(CONFIG_CPU_MC200)       += power_measure_demo/pm_mc200_demo

ifeq (n, $(CONFIG_ENABLE_MCU_PM3))
	subdir-$(CONFIG_CPU_MW300)     += power_measure_demo/pm_mw300_demo
endif

ifeq (y, $(CONFIG_ENABLE_MCU_PM3))
	subdir-$(CONFIG_CPU_MW300)     += power_profiles/deepsleep_pm3
endif

subdir-$(CONFIG_ENABLE_HTTPC_SECURE) += test_apps/tls_demo

subdir-$(CONFIG_CMSIS_DSPLIB)    += dsp_demo/dsp_dotproduct_demo
subdir-$(CONFIG_CMSIS_DSPLIB)    += dsp_demo/dsp_fft_bin_demo
subdir-$(CONFIG_CMSIS_DSPLIB)    += dsp_demo/dsp_matrix_demo

subdir-y                         += test_apps/all_in_one
subdir-y                         += test_apps/qa1
subdir-y                         += test_apps/qa2
subdir-y                         += test_apps/qa3
subdir-y                         += test_apps/qa4
subdir-y                         += test_apps/os/rw_lock_demo

subdir-y                         += test_apps/os/recursive_mutex_demo
subdir-y                         += test_apps/os/heap_alloc_demo


subdir-y                         += test_apps/pm_i2c_demo/pm_i2c_master
subdir-y                         += test_apps/pm_i2c_demo/pm_i2c_slave

subdir-y                         += wifi_driver_demo
subdir-y                         += wlan/wlan_uap
subdir-y                         += wlan/wm_demo
subdir-y                         += wlan/wlan_11d
subdir-y                         += wlan/wlan_mac
subdir-y                         += wlan/wlan_eu_cert
subdir-y                         += wlan/wlan_cal
subdir-$(CONFIG_WiFi_8801)       += wlan/wlan_low_power
subdir-$(CONFIG_CPU_MW300)       += wlan/wlan_power_save
subdir-$(CONFIG_CPU_MW300)       += wlan/wlan_pdn
subdir-y                         += wlan/uap_prov
subdir-y                         += wlan/wlan_sniffer
subdir-y                         += wlan/wlan_trpc_demo
subdir-y                         += wlan/wlan_frame_inject_demo
subdir-$(CONFIG_WPA2_ENTP)       += wlan/wpa2_enterprise
subdir-$(CONFIG_P2P)             += wlan/p2p_demo
subdir-$(CONFIG_P2P)             += wlan/raw_p2p_demo

subdir-y                         += tutorials
