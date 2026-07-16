#include "utils.h"

#include "config.h"
#include "http_response.h"
#include "net_io.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void write_error_response(int client, int status_code,
                                 const char *status_text,
                                 const char *message)
{
 http_response_buffer_t response;

 if (http_build_error_response(&response, status_code, status_text, message) == 0)
  net_write_all(client, response.data, response.length);
}

int resolve_safe_path(int client, const char *url, char *path,
                      size_t path_size, struct stat *st, int check_file)
{
 int written;
 size_t path_len;
 const server_config_t *cfg = get_server_config();
 char docroot[PATH_MAX];
 char resolved_path[PATH_MAX];

 if (is_path_traversal(url))
 {
  write_error_response(client, 403, "Forbidden",
                       "Path traversal is not allowed.");
  return 403;
 }

 if (strlen(cfg->root_dir) >= PATH_MAX)
 {
  write_error_response(client, 500, "Internal Server Error",
                       "Document root path is too long.");
  return 500;
 }

 if (realpath(cfg->root_dir, docroot) == NULL)
 {
  write_error_response(client, 500, "Internal Server Error",
                       "Document root is not available.");
  return 500;
 }

 written = snprintf(path, path_size, "%s%s", cfg->root_dir, url);
 if (written < 0 || (size_t)written >= path_size)
 {
  write_error_response(
      client, 400, "BAD REQUEST",
      "Your browser sent a bad request, such as a POST without a Content-Length.");
  return 400;
 }

 if (!check_file)
  return 0;

 if (path[strlen(path) - 1] == '/')
 {
  path_len = strlen(path);
  written = snprintf(path + path_len, path_size - path_len, "index.html");
  if (written < 0 || (size_t)written >= path_size - path_len)
  {
   write_error_response(
       client, 400, "BAD REQUEST",
       "Your browser sent a bad request, such as a POST without a Content-Length.");
   return 400;
  }
 }

 if (stat(path, st) == -1) {
  write_error_response(
      client, 404, "NOT FOUND",
      "The server could not fulfill your request because the resource specified is unavailable or nonexistent.");
  return 404;
 }

 if ((st->st_mode & S_IFMT) == S_IFDIR)
 {
  path_len = strlen(path);
  written = snprintf(path + path_len, path_size - path_len, "/index.html");
  if (written < 0 || (size_t)written >= path_size - path_len)
  {
   write_error_response(
       client, 400, "BAD REQUEST",
       "Your browser sent a bad request, such as a POST without a Content-Length.");
   return 400;
  }
  if (stat(path, st) == -1) {
   write_error_response(
       client, 404, "NOT FOUND",
       "The server could not fulfill your request because the resource specified is unavailable or nonexistent.");
   return 404;
  }
 }

 if (realpath(path, resolved_path) == NULL)
 {
  write_error_response(
      client, 404, "NOT FOUND",
      "The server could not fulfill your request because the resource specified is unavailable or nonexistent.");
  return 404;
 }

 path_len = strlen(docroot);
 if (strncmp(resolved_path, docroot, path_len) != 0 ||
     !(resolved_path[path_len] == '\0' || resolved_path[path_len] == '/'))
 {
  write_error_response(client, 403, "Forbidden",
                       "Path escapes document root.");
  return 403;
 }

 written = snprintf(path, path_size, "%s", resolved_path);
 if (written < 0 || (size_t)written >= path_size)
 {
  write_error_response(
      client, 400, "BAD REQUEST",
      "Your browser sent a bad request, such as a POST without a Content-Length.");
  return 400;
 }

 return 0;
}

int is_path_traversal(const char *path)
{
 const char *p;
 const char *start;

 if (path == NULL)
  return 1;

 p = path;
 while (*p)
 {
  while (*p == '/')
   p++;
  if (*p == '\0')
   break;

  start = p;
  while (*p != '\0' && *p != '/')
   p++;

  if (p == start + 2 && start[0] == '.' && start[1] == '.')
   return 1;
 }

 return 0;
}
