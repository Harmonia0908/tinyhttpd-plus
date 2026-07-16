#ifndef TINYHTTPD_NET_IO_H
#define TINYHTTPD_NET_IO_H

#include <stddef.h>

int net_read_line(int fd, char *buffer, size_t capacity);
int net_write_all(int fd, const void *data, size_t length);

/* Compatibility entry points retained for existing callers. */
int get_line(int sock, char *buf, int size);
int send_all(int client, const void *data, size_t len);

#endif
