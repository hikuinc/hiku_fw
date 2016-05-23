exec-y                       += 07led_pushbutton
07led_pushbutton-objs-y      := src/main.o src/plug_handlers.o src/fw_upgrade.c

07led_pushbutton-ftfs-y      := 07led_pushbutton.ftfs
07led_pushbutton-ftfs-dir-y  := $(d)/www
07led_pushbutton-ftfs-api-y  := 100
