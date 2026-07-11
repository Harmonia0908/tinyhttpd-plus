#include "utils.h"

#include "config.h"
#include "response.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

int send_all(int client, const void *data, size_t len)
{
 const char *buf = (const char *)data;
 size_t sent = 0;

 while (sent < len)
 {
  ssize_t n = send(client, buf + sent, len - sent, 0);
  if (n < 0)
  {
   if (errno == EINTR)
    continue;
   return -1;
  }
  if (n == 0)
   return -1;
  sent += (size_t)n;
 }

 return 0;
}

int set_cloexec(int fd)
{
 int flags;

 do {
  flags = fcntl(fd, F_GETFD);
 } while (flags == -1 && errno == EINTR);
 if (flags == -1)
  return -1;

 while (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1)
 {
  if (errno != EINTR)
   return -1;
 }
 return 0;
}

void error_die(const char *sc)
{
 perror(sc);
 exit(1);
}

int get_line(int sock, char *buf, int size)
{
 int i = 0;
 char c = '\0';
 ssize_t n;

 while ((i < size - 1) && (c != '\n'))
 {
  do {
   n = recv(sock, &c, 1, 0);
  } while (n < 0 && errno == EINTR);
  if (n > 0)
  {
   if (c == '\r')
   {
     do {
      n = recv(sock, &c, 1, MSG_PEEK);
     } while (n < 0 && errno == EINTR);
     if ((n > 0) && (c == '\n'))
     {
      do {
       n = recv(sock, &c, 1, 0);
      } while (n < 0 && errno == EINTR);
      if (n <= 0)
      {
       buf[i] = '\0';
       return -1;
      }
     }
     else
      c = '\n';
   }
   buf[i] = c;
   i++;
  }
  else if (n == 0)
  {
   buf[i] = '\0';
   return i;
  }
  else
  {
   if (errno == EAGAIN || errno == EWOULDBLOCK)
   {
    fprintf(stderr, "[timeout] client_fd=%d\n", sock);
   }
   buf[i] = '\0';
   return -1;
  }
 }
 buf[i] = '\0';

 return i;
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
  send_error_page(client, 403, "Forbidden", "Path traversal is not allowed.");
  return 403;
 }

 if (strlen(cfg->root_dir) >= PATH_MAX)
 {
  send_error_page(client, 500, "Internal Server Error",
                  "Document root path is too long.");
  return 500;
 }

 if (realpath(cfg->root_dir, docroot) == NULL)
 {
  send_error_page(client, 500, "Internal Server Error",
                  "Document root is not available.");
  return 500;
 }

 written = snprintf(path, path_size, "%s%s", cfg->root_dir, url);
 if (written < 0 || (size_t)written >= path_size)
 {
  bad_request(client);
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
   bad_request(client);
   return 400;
  }
 }

 if (stat(path, st) == -1) {
  not_found(client);
  return 404;
 }

 if ((st->st_mode & S_IFMT) == S_IFDIR)
 {
  path_len = strlen(path);
  written = snprintf(path + path_len, path_size - path_len, "/index.html");
  if (written < 0 || (size_t)written >= path_size - path_len)
  {
   bad_request(client);
   return 400;
  }
  if (stat(path, st) == -1) {
   not_found(client);
   return 404;
  }
 }

 if (realpath(path, resolved_path) == NULL)
 {
  not_found(client);
  return 404;
 }

 path_len = strlen(docroot);
 if (strncmp(resolved_path, docroot, path_len) != 0 ||
     !(resolved_path[path_len] == '\0' || resolved_path[path_len] == '/'))
 {
  send_error_page(client, 403, "Forbidden", "Path escapes document root.");
  return 403;
 }

 written = snprintf(path, path_size, "%s", resolved_path);
 if (written < 0 || (size_t)written >= path_size)
 {
  bad_request(client);
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
		/* 跳过连续的 '/' */
		while (*p == '/')
			p++;
		if (*p == '\0')
			break;

		/* 计算当前路径组件 */
		start = p;
		while (*p != '\0' && *p != '/')
			p++;

		/* 长度恰好为 2 且两个字符都是 '.' */
		if (p == start + 2 && start[0] == '.' && start[1] == '.')
			return 1;
	}

	return 0;
}
