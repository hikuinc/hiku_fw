exec-y += hiku
hiku-objs-y := src/main.o src/connection_manager.o src/http_manager.o src/ota_update.o src/button_manager.o src/hiku_board.c

hiku-ftfs-y	:= hiku.ftfs
hiku-ftfs-dir-y	:= $(d)/www
hiku-ftfs-api-y	:= 100