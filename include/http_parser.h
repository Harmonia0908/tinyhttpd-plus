#ifndef TINYHTTPD_HTTP_PARSER_H
#define TINYHTTPD_HTTP_PARSER_H

#include <stddef.h>

#define MAX_REQUEST_SIZE (1024 * 1024)
#define MAX_HEADER_SIZE (8 * 1024)
#define MAX_HEADER_LINE_SIZE 1024

int http_parse_request_line(const char *data, size_t length,
                            char *method, size_t method_size,
                            char *url, size_t url_size);
int http_parse_headers(const char *data, size_t length, int *content_length);

#endif
