exec-y                      += 05local_control
05local_control-objs-y      := src/main.o src/plug_handlers.o

05local_control-ftfs-y      := 05local_control.ftfs
05local_control-ftfs-dir-y  := $(d)/www
05local_control-ftfs-api-y  := 100
