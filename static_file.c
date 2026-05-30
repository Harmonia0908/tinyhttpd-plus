#include "static_file.h"

#include "response.h"
#include "utils.h"

#include <errno.h>
#include <stdio.h>
#include <unistd.h>

int handle_static_file(int client, const char *path, int is_head, off_t file_size)
{
 return serve_file(client, path, is_head, file_size);
}

void cat(int client, FILE *resource)
{
 char buf[4096];
 size_t nread;

 while ((nread = fread(buf, 1, sizeof(buf), resource)) > 0)
 {
  if (send_all(client, buf, nread) < 0)
   break;
 }
}

int serve_file(int client, const char *filename, int is_head, off_t file_size)
{
 FILE *resource = NULL;

 resource = fopen(filename, "r");
 if (resource == NULL) {
  int saved_errno = errno;
  if (saved_errno == EACCES)
   send_error_page(client, 403, "Forbidden", "Permission denied.");
  else
   not_found(client);
  close(client);
  return saved_errno == EACCES ? 403 : 404;
 } else {
  headers(client, filename, file_size);
  if (!is_head)
   cat(client, resource);
  fclose(resource);
  close(client);
  return 200;
 }
}
