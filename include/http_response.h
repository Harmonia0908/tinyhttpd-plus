#ifndef TINYHTTPD_HTTP_RESPONSE_H
#define TINYHTTPD_HTTP_RESPONSE_H

#include "http_protocol.h"

#include <stddef.h>
#include <sys/types.h>

#define HTTP_RESPONSE_CAPACITY 8192

typedef struct {
 char data[HTTP_RESPONSE_CAPACITY];
 size_t length;
} http_response_buffer_t;

int http_build_static_headers(http_response_buffer_t *response,
                              const char *filename, off_t content_length);
int http_build_error_response(http_response_buffer_t *response,
                              int status_code, const char *status_text,
                              const char *message);
int http_build_options_response(http_response_buffer_t *response);

#endif
