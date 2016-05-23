exec-y                          += 06firmware_upgrades
06firmware_upgrades-objs-y      := src/main.o src/plug_handlers.o src/fw_upgrade.c

06firmware_upgrades-ftfs-y      := 06firmware_upgrades.ftfs
06firmware_upgrades-ftfs-dir-y  := $(d)/www
06firmware_upgrades-ftfs-api-y  := 100
