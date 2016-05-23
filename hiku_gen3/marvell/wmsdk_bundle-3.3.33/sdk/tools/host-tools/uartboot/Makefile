# Copyright 2008-2015 Marvell International Ltd.
# All Rights Reserved.

TARGET ?= uartboot
CC ?= gcc

OS := $(shell uname)
ifneq ($(OS), Darwin)
STRIP = -s
endif

all: $(TARGET)

$(TARGET): uart_boot.c
	@echo " [$(CC)] $^"
	@$(CC) $(CFLAGS) $(STRIP) uart_boot.c -o $(TARGET)

clean:
	@rm -f $(TARGET) *.o
