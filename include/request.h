#ifndef TINYHTTPD_REQUEST_H
#define TINYHTTPD_REQUEST_H

#include <stddef.h>

void accept_request(int client);
int parse_request_line(int client, char *method, size_t method_size, char *url, size_t url_size);
int read_headers(int client, int *content_length);

#endif
