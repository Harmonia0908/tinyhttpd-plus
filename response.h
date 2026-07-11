#ifndef TINYHTTPD_RESPONSE_H
#define TINYHTTPD_RESPONSE_H

#include <sys/types.h>

void bad_request(int client);
void cannot_execute(int client);
void headers(int client, const char *filename, off_t content_length);
void not_found(int client);
void send_error_page(int client, int status_code, const char *status_text, const char *message);
void unimplemented(int client);
void send_413(int client);
void uri_too_long(int client);

#endif
