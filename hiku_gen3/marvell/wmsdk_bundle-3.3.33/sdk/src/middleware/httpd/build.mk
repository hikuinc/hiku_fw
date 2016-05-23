# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

libs-y += libhttpd

libhttpd-objs-y := httpd.c http_parse.c httpd_handle.c httpd_sys.c
libhttpd-objs-y += http-strings.c httpd_ssi.c httpd_wsgi.c httpd_test.c
libhttpd-objs-y += httpd_handle_file.c
