#ifndef TINYHTTPD_UTILS_H
#define TINYHTTPD_UTILS_H

#include "fd_lifecycle.h"
#include "http_parser.h"
#include "http_protocol.h"
#include "net_io.h"

#include <stddef.h>
#include <sys/stat.h>

#define CGI_TIMEOUT_SECONDS 5

int is_path_traversal(const char *path);
int resolve_safe_path(int client, const char *url, char *path, size_t path_size, struct stat *st, int check_file);

#endif
