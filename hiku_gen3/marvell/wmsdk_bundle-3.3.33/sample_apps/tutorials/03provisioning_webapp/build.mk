exec-y                            += 03provisioning_webapp
03provisioning_webapp-objs-y      := src/main.o

03provisioning_webapp-ftfs-y      := 03provisioning_webapp.ftfs
03provisioning_webapp-ftfs-dir-y  := $(d)/www
03provisioning_webapp-ftfs-api-y  := 100
