#ifndef TINYHTTPD_SERVER_H
#define TINYHTTPD_SERVER_H

#include <sys/types.h>

void handle_client(int client);
int startup(u_short *port);
int server_run(u_short port);

#endif
