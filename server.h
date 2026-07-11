#ifndef TINYHTTPD_SERVER_H
#define TINYHTTPD_SERVER_H

#include <stdint.h>

void handle_client(int client);
int startup(uint16_t *port);
int server_run(uint16_t port);

#endif
