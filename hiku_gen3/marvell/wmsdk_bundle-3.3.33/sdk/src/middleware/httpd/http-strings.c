/*
 *  Copyright (C) 2015, Marvell International Ltd.
 *  All Rights Reserved.
 *
 * You can add new entries to this file. When you add a new entry also add
 * the entry as extern to the http-strings.h header file.
 */
const char http_http[] = "http://";
const char http_200[] = "200 ";
const char http_301[] = "301 ";
const char http_302[] = "302 ";

/* HTTP methods */
const char http_get[] = "GET ";
const char http_post[] = "POST ";
const char http_put[] = "PUT ";
const char http_delete[] = "DELETE ";
const char http_head[] = "HEAD ";

/* HTTP versions */
const char http_10[] = "HTTP/1.0";
const char http_11[] = "HTTP/1.1";
const char http_last_chunk[] = "0\r\n\r\n";

/* HTTP Headers */
const char http_content_type[] = "Content-Type";
const char http_content_len[] = "Content-Length";
const char http_user_agent[] = "User-Agent";
const char http_if_none_match[] = "If-None-Match";
const char http_if_modified_since[] = "If-Modified-Since";
const char http_encoding[] = "Transfer-Encoding";
const char http_texthtml[] = "text/html";
const char http_location[] = "location";
const char http_host[] = "host";

const char http_crnl[] = "\r\n";
const char http_index_html[] = "/index.html";
const char http_404_html[] = "/404.html";
const char http_referer[] = "Referer:";
const char http_header_server[] = "Server: Marvell-WM\r\n";
const char http_header_conn_close[] = "Connection: close\r\n";
const char http_header_conn_keep_alive[] = "Connection: keep-alive\r\n";
const char http_header_type_chunked[] = "Transfer-Encoding: chunked\r\n";
const char http_header_cache_ctrl[] =
	"Cache-Control: no-store, no-cache, must-revalidate\r\n";
const char http_header_cache_ctrl_no_chk[] =
	"Cache-Control: post-check=0, pre-check=0\r\n";
const char http_header_pragma_no_cache[] = "Pragma: no-cache\r\n";
const char http_header_200[] = "HTTP/1.1 200 OK\r\n";

const char http_header_304_prologue[] = "HTTP/1.1 304 Not Modified\r\n";
const char http_header_404[] = "HTTP/1.1 404 Not Found\r\n";
const char http_header_400[] = "HTTP/1.1 400 Bad Request\r\n";
const char http_header_500[] = "HTTP/1.1 500 Internal Server Error\r\n";
const char http_header_505[] = "HTTP/1.1 505 HTTP Version Not Supported\r\n";
const char http_content_type_plain[] = "Content-Type: text/plain\r\n";
const char http_content_type_xml[] = "Content-Type: text/xml\r\n";
const char http_content_type_html[] =
	"Content-Type: text/html; charset=iso8859-1\r\n";
const char http_content_type_html_nocache[] =
	"Content-Type: text/html; charset=iso8859-1\r\n"
	"Cache-Control: no-store, no-cache, must-revalidate\r\n"
	"Cache-Control: post-check=0, pre-check=0\r\nPragma: no-cache\r\n";
const char http_content_type_css[] = "Content-Type: text/css\r\n";
const char http_content_type_png[] = "Content-Type: image/png\r\n";
const char http_content_type_gif[] = "Content-Type: image/gif\r\n";
const char http_content_type_jpg[] = "Content-Type: image/jpeg\r\n";
const char http_content_type_js[] = "Content-Type: text/javascript\r\n";
const char http_content_type_binary[] =
	"Content-Type: application/octet-stream\r\n";

const char http_content_type_json_nocache[] =
	"Content-Type: application/json\r\n"
	"Cache-Control: no-store, no-cache, must-revalidate\r\n"
	"Cache-Control: post-check=0, pre-check=0\r\n"
	"Pragma: no-cache\r\n\r\n";
const char http_content_type_xml_nocache[] =
	"Content-Type: text/xml\r\n"
	"Cache-Control: no-store, no-cache, must-revalidate\r\n"
	"Cache-Control: post-check=0, pre-check=0\r\nPragma: no-cache\r\n\r\n";
const char http_content_type_form[] =
	"Content-Type: application/x-www-form-urlencoded\r\n\r\n";

const char http_content_type_text_cache_manifest[] =
	"Content-Type: text/cache-manifest\r\n";
const char http_content_encoding_gz[] =
	"Vary: Accept-Encoding\r\nContent-Encoding: gzip\r\n";
const char http_html[] = ".html";
const char http_shtml[] = ".shtml";
const char http_htm[] = ".htm";
const char http_css[] = ".css";
const char http_png[] = ".png";
const char http_gif[] = ".gif";
const char http_jpg[] = ".jpg";
const char http_text[] = ".txt";
const char http_txt[] = ".txt";
const char http_xml[] = ".xml";
const char http_js[] = ".js";
const char http_gz[] = ".gz";
const char http_manifest[] = ".manifest";
const char http_cache_control[] = "Cache-Control: max-age=10\r\n";
