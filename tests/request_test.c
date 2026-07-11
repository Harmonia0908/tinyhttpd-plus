#include "request.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void fail(const char *message)
{
 fprintf(stderr, "FAIL: %s\n", message);
 exit(EXIT_FAILURE);
}

static void write_request(int fd, const char *request)
{
 size_t length = strlen(request);
 size_t written = 0;

 while (written < length)
 {
  ssize_t result = write(fd, request + written, length - written);
  if (result <= 0)
   fail("could not write test request");
  written += (size_t)result;
 }
}

static void open_request_socket(const char *request, int sockets[2])
{
 if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0)
  fail("socketpair failed");
 write_request(sockets[0], request);
 if (shutdown(sockets[0], SHUT_WR) != 0)
  fail("shutdown failed");
}

static void close_request_socket(int sockets[2])
{
 close(sockets[0]);
 close(sockets[1]);
}

static void test_rejects_overlong_uri(void)
{
 char request[700];
 char method[16];
 char url[255];
 int sockets[2];
 int status;
 size_t offset;

 offset = (size_t)snprintf(request, sizeof(request), "GET /");
 memset(request + offset, 'a', 300);
 offset += 300;
 snprintf(request + offset, sizeof(request) - offset, " HTTP/1.0\r\n");

 open_request_socket(request, sockets);
 status = parse_request_line(sockets[1], method, sizeof(method), url, sizeof(url));
 close_request_socket(sockets);

 if (status != 414)
  fail("overlong URI was not rejected with 414");
}

static void test_rejects_missing_uri(void)
{
 char method[16];
 char url[255];
 int sockets[2];
 int status;

 open_request_socket("GET\r\n", sockets);
 status = parse_request_line(sockets[1], method, sizeof(method), url, sizeof(url));
 close_request_socket(sockets);

 if (status != 400)
  fail("request line without a URI was not rejected with 400");
}

static void test_rejects_duplicate_content_length(void)
{
 int sockets[2];
 int content_length = -1;
 int status;

 open_request_socket("Content-Length: 3\r\nContent-Length: 4\r\n\r\n", sockets);
 status = read_headers(sockets[1], &content_length);
 close_request_socket(sockets);

 if (status != 400)
  fail("duplicate Content-Length was not rejected with 400");
}

static void test_accepts_valid_request_boundaries(void)
{
 char method[16];
 char url[255];
 int sockets[2];
 int status;
 int content_length = -1;

 open_request_socket("POST /date.cgi HTTP/1.0\r\n", sockets);
 status = parse_request_line(sockets[1], method, sizeof(method), url, sizeof(url));
 close_request_socket(sockets);
 if (status != 0 || strcmp(method, "POST") != 0 || strcmp(url, "/date.cgi") != 0)
  fail("valid request line was rejected");

 open_request_socket("Content-Length: 1048576\r\n\r\n", sockets);
 status = read_headers(sockets[1], &content_length);
 close_request_socket(sockets);
 if (status != 0 || content_length != 1048576)
  fail("maximum permitted Content-Length was rejected");
}

int main(void)
{
 test_rejects_overlong_uri();
 test_rejects_missing_uri();
 test_rejects_duplicate_content_length();
 test_accepts_valid_request_boundaries();
 puts("PASS: request parser boundary tests completed");
 return EXIT_SUCCESS;
}
