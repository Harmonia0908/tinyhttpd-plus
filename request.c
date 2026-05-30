#include "request.h"

#include "cgi.h"
#include "log.h"
#include "response.h"
#include "static_file.h"
#include "utils.h"

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define ISspace(x) isspace((int)(x))

/*
 * Write error log entries for request failures with status codes that are
 * required by the logging contract.
 */
static void log_request_error(int status, const char *url, const char *path)
{
 if (status == 404)
  log_error_message("File not found: %s", path != NULL && path[0] != '\0' ? path : url);
 else if (status == 403)
  log_error_message("Permission denied: %s", path != NULL && path[0] != '\0' ? path : url);
 else if (status == 500)
  log_error_message("Internal error while handling: %s", url != NULL ? url : "-");
}

void accept_request(int client)
{
 char method[255] = "-";
 char url[255] = "-";
 char access_url[255] = "-";
 char path[512] = "";
 struct stat st;
 int cgi = 0;      /* becomes true if server decides this is a CGI
                    * program */
 char *query_string = NULL;
 struct sockaddr_in client_addr;
 socklen_t client_len = sizeof(client_addr);
 char client_ip[INET_ADDRSTRLEN];
 int content_length = -1;
 int status;

 snprintf(client_ip, sizeof(client_ip), "-");
 if (getpeername(client, (struct sockaddr *)&client_addr, &client_len) == 0)
  inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

 status = parse_request_line(client, method, sizeof(method), url, sizeof(url));
 if (status != 0)
 {
  if (status > 0)
   log_access(client_ip, method, url, status);
  close(client);
  return;
 }
 snprintf(access_url, sizeof(access_url), "%s", url);

 //如果是POST，cgi置为1
 if (strcasecmp(method, "POST") == 0)
  cgi = 1;

 status = read_headers(client, &content_length);
 if (status != 0)
 {
  if (status == 400)
   bad_request(client);
  else if (status == 413)
   send_413(client);
  if (status > 0)
   log_access(client_ip, method, access_url, status);
  close(client);
  return;
 }

 //处理OPTIONS方法
  if (strcasecmp(method, "OPTIONS") == 0)
 {
  char buf[1024];
  
  snprintf(buf, sizeof(buf), "HTTP/1.0 200 OK\r\n");
  send_all(client, buf, strlen(buf));
  
  snprintf(buf, sizeof(buf), "%s", SERVER_STRING);
  send_all(client, buf, strlen(buf));
  
  snprintf(buf, sizeof(buf), "Allow: GET, POST, HEAD, OPTIONS\r\n");
  send_all(client, buf, strlen(buf));
  
  snprintf(buf, sizeof(buf), "Access-Control-Allow-Origin: *\r\n");
  send_all(client, buf, strlen(buf));
  
  snprintf(buf, sizeof(buf), "Access-Control-Allow-Methods: GET, POST, HEAD, OPTIONS\r\n");
  send_all(client, buf, strlen(buf));
  
  snprintf(buf, sizeof(buf), "Access-Control-Allow-Headers: Content-Type\r\n");
  send_all(client, buf, strlen(buf));
  
  snprintf(buf, sizeof(buf), "\r\n");
  send_all(client, buf, strlen(buf));
  
  log_access(client_ip, method, access_url, 200);
  close(client);
  return;
 }

 //判断Get或HEAD请求
 if (strcasecmp(method, "GET") == 0 || strcasecmp(method, "HEAD") == 0)
 {
  query_string = url;
  while ((*query_string != '?') && (*query_string != '\0'))
   query_string++;
  if (*query_string == '?')
  {
   cgi = 1;
   *query_string = '\0';
   query_string++;
  }

  status = resolve_safe_path(client, url, path, sizeof(path), &st, 1);
  if (status != 0) {
   log_request_error(status, access_url, path);
   log_access(client_ip, method, access_url, status);
   close(client);
   return;
  }

  if ((st.st_mode & S_IXUSR) ||
      (st.st_mode & S_IXGRP) ||
      (st.st_mode & S_IXOTH))
   cgi = 1;

  if (!cgi)
   status = handle_static_file(client, path, strcasecmp(method, "HEAD") == 0, st.st_size);
  else
  {
   status = handle_cgi(client, path, method, query_string,
                       strcasecmp(method, "HEAD") == 0, -1);
  }
  log_request_error(status, access_url, path);
  log_access(client_ip, method, access_url, status);
 }
 else    /* POST */
 {
  query_string = url;

  status = resolve_safe_path(client, url, path, sizeof(path), &st, 1);
  if (status != 0) {
   log_request_error(status, access_url, path);
   log_access(client_ip, method, access_url, status);
   close(client);
   return;
  }

  //执行cgi文件
  status = handle_cgi(client, path, method, query_string, 0, content_length);
  log_request_error(status, access_url, path);
  log_access(client_ip, method, access_url, status);
 }}

int parse_request_line(int client, char *method, size_t method_size,
                       char *url, size_t url_size)
{
 char buf[1024];
 int numchars;
 size_t i, j;

 numchars = get_line(client, buf, sizeof(buf));

 if (numchars <= 0)
  return -1;

 if (numchars == (int)sizeof(buf) - 1 && buf[numchars - 1] != '\n') {
  fprintf(stderr, "[reject] client_fd=%d request_line_too_long\n", client);
  bad_request(client);
  return 400;
 }

 i = 0; j = 0;
 while (!ISspace(buf[j]) && (i < method_size - 1))
 {
  method[i] = buf[j];
  i++; j++;
 }
 method[i] = '\0';

 if (strcasecmp(method, "GET") && strcasecmp(method, "POST") &&
     strcasecmp(method, "HEAD") && strcasecmp(method, "OPTIONS"))
 {
  unimplemented(client);
  return 501;
 }

 i = 0;
 while (ISspace(buf[j]) && (j < sizeof(buf)))
  j++;

 while (!ISspace(buf[j]) && (i < url_size - 1) && (j < sizeof(buf)))
 {
  url[i] = buf[j];
  i++; j++;
 }
 url[i] = '\0';

 return 0;
}

int read_headers(int client, int *content_length)
{
 char buf[MAX_HEADER_LINE_SIZE];
 int numchars;
 size_t header_size = 0;

 if (content_length != NULL)
  *content_length = -1;

 numchars = get_line(client, buf, sizeof(buf));
 while ((numchars > 0) && strcmp("\n", buf))
 {
  if (numchars == (int)sizeof(buf) - 1 && buf[numchars - 1] != '\n')
  {
   fprintf(stderr, "[reject] client_fd=%d header_line_too_long\n", client);
   return 400;
  }

  header_size += (size_t)numchars;
  if (header_size > MAX_HEADER_SIZE)
  {
   fprintf(stderr, "[reject] client_fd=%d header_size=%zu too large\n",
           client, header_size);
   return 413;
  }

  if (content_length != NULL &&
      strncasecmp(buf, "Content-Length:", 15) == 0)
  {
   char *value = buf + 15;
   char *end;
   long len;

   while (ISspace(*value))
    value++;

   errno = 0;
   len = strtol(value, &end, 10);
   while (*end == ' ' || *end == '\t')
    end++;

   if (value == end || errno == ERANGE || len < 0 ||
       !(*end == '\0' || *end == '\r' || *end == '\n'))
    return 400;

   if (len > MAX_REQUEST_SIZE)
    return 413;

   *content_length = (int)len;
  }

  numchars = get_line(client, buf, sizeof(buf));
 }

 if (numchars < 0)
  return -1;

 if (numchars > 0)
 {
  header_size += (size_t)numchars;
  if (header_size > MAX_HEADER_SIZE)
  {
   fprintf(stderr, "[reject] client_fd=%d header_size=%zu too large\n",
           client, header_size);
   return 413;
  }
 }

 return 0;
}
