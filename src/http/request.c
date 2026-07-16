#include "request.h"

#include "cgi.h"
#include "http_parser.h"
#include "http_response.h"
#include "log.h"
#include "net_io.h"
#include "static_file.h"
#include "utils.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

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

static void write_error_response(int client, int status_code,
                                 const char *status_text,
                                 const char *message)
{
 http_response_buffer_t response;

 if (http_build_error_response(&response, status_code, status_text, message) == 0)
  net_write_all(client, response.data, response.length);
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

 if (getpeername(client, (struct sockaddr *)&client_addr, &client_len) == 0) {
  if (inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN) == NULL)
   snprintf(client_ip, sizeof(client_ip), "-");
 } else {
  snprintf(client_ip, sizeof(client_ip), "-");
 }

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
   write_error_response(
       client, 400, "BAD REQUEST",
       "Your browser sent a bad request, such as a POST without a Content-Length.");
  else if (status == 413)
   write_error_response(client, 413, "Payload Too Large",
                        "Request size exceeds the configured limit.");
  if (status > 0)
   log_access(client_ip, method, access_url, status);
  close(client);
  return;
 }

 //处理OPTIONS方法
 if (strcasecmp(method, "OPTIONS") == 0)
 {
  http_response_buffer_t response;

  if (http_build_options_response(&response) == 0)
   net_write_all(client, response.data, response.length);
  
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
 int status;

 if (method == NULL || url == NULL || method_size < 2 || url_size < 2)
  return -1;

 method[0] = '\0';
 url[0] = '\0';

 numchars = net_read_line(client, buf, sizeof(buf));

 if (numchars <= 0)
  return -1;

 status = http_parse_request_line(buf, (size_t)numchars,
                                  method, method_size, url, url_size);
 if (status == 400 && numchars == (int)sizeof(buf) - 1 &&
     buf[numchars - 1] != '\n')
 {
  fprintf(stderr, "[reject] client_fd=%d request_line_too_long\n", client);
 }
 if (status == 400)
  write_error_response(
      client, 400, "BAD REQUEST",
      "Your browser sent a bad request, such as a POST without a Content-Length.");
 else if (status == 501)
  write_error_response(client, 501, "Method Not Implemented",
                       "HTTP request method not supported.");
 else if (status == 414)
  write_error_response(client, 414, "URI Too Long",
                       "The requested URI exceeds the server limit.");

 return status;
}

int read_headers(int client, int *content_length)
{
 char headers[MAX_HEADER_SIZE + MAX_HEADER_LINE_SIZE + 1];
 char line[MAX_HEADER_LINE_SIZE];
 int numchars;
 size_t header_size = 0;
 int status;

 if (content_length != NULL)
  *content_length = -1;

 for (;;)
 {
  numchars = net_read_line(client, line, sizeof(line));
  if (numchars < 0)
   return -1;
  if (numchars == 0)
   return 0;

  if (numchars == (int)sizeof(line) - 1 && line[numchars - 1] != '\n')
  {
   fprintf(stderr, "[reject] client_fd=%d header_line_too_long\n", client);
   return 400;
  }

  if (header_size + (size_t)numchars > sizeof(headers) - 1)
   return 413;
  memcpy(headers + header_size, line, (size_t)numchars);
  header_size += (size_t)numchars;

  headers[header_size] = '\0';
  status = http_parse_headers(headers, header_size, content_length);
  if (status == 413)
   fprintf(stderr, "[reject] client_fd=%d header_size=%zu too large\n",
           client, header_size);
  if (status != 0)
   return status;

  if (strcmp("\n", line) == 0)
   return 0;
 }
}
