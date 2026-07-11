#ifndef TINYHTTPD_UTILS_H
#define TINYHTTPD_UTILS_H

#include <stddef.h>
#include <sys/stat.h>

#define MAX_REQUEST_SIZE (1024 * 1024)
#define MAX_HEADER_SIZE (8 * 1024)
#define MAX_HEADER_LINE_SIZE 1024
#define CGI_TIMEOUT_SECONDS 5

#define SERVER_STRING "Server: Tinyhttpd/1.0\r\n"

void error_die(const char *sc);
int get_line(int sock, char *buf, int size);
int is_path_traversal(const char *path);
int resolve_safe_path(int client, const char *url, char *path, size_t path_size, struct stat *st, int check_file);
int send_all(int client, const void *data, size_t len);
int set_cloexec(int fd);

#endif
