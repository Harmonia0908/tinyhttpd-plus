#ifndef TINYHTTPD_STATIC_FILE_H
#define TINYHTTPD_STATIC_FILE_H

#include <stdio.h>
#include <sys/types.h>

int handle_static_file(int client, const char *path, int is_head, off_t file_size);
void cat(int client, FILE *resource);
int serve_file(int client, const char *filename, int is_head, off_t file_size);

#endif
