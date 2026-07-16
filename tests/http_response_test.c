#include "http_response.h"
#include "response.h"

#include <errno.h>
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

static size_t finish_and_read_response(int sockets[2], char *buffer,
                                       size_t capacity)
{
 size_t used = 0;

 if (shutdown(sockets[1], SHUT_WR) != 0)
  fail("could not finish response socket");

 while (used < capacity)
 {
  ssize_t result = read(sockets[0], buffer + used, capacity - used);
  if (result < 0 && errno == EINTR)
   continue;
  if (result < 0)
   fail("could not read response bytes");
  if (result == 0)
   break;
  used += (size_t)result;
 }

 close(sockets[0]);
 close(sockets[1]);
 return used;
}

static void expect_bytes(const char *actual, size_t actual_length,
                         const char *expected, const char *message)
{
 size_t expected_length = strlen(expected);

 if (actual_length != expected_length ||
     memcmp(actual, expected, expected_length) != 0)
  fail(message);
}

static void test_static_headers_wire_bytes(void)
{
 static const char expected[] =
     "HTTP/1.0 200 OK\r\n"
     "Server: Tinyhttpd/1.0\r\n"
     "Content-Type: text/html\r\n"
     "Content-Length: 3\r\n"
     "Connection: close\r\n"
     "\r\n";
 int sockets[2];
 char actual[8192];
 size_t actual_length;

 if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0)
  fail("socketpair failed");
 headers(sockets[1], "index.html", 3);
 actual_length = finish_and_read_response(sockets, actual, sizeof(actual));
 expect_bytes(actual, actual_length, expected,
              "static response header bytes changed");
}

static void test_bad_request_wire_bytes(void)
{
 static const char expected[] =
     "HTTP/1.0 400 BAD REQUEST\r\n"
     "Server: Tinyhttpd/1.0\r\n"
     "Content-Type: text/html\r\n"
     "Content-Length: 261\r\n"
     "Connection: close\r\n"
     "\r\n"
     "<HTML><HEAD><TITLE>400 BAD REQUEST</TITLE></HEAD>\r\n"
     "<BODY><CENTER><H1>400 BAD REQUEST</H1></CENTER>\r\n"
     "<HR><CENTER>Server: Tinyhttpd/1.0\r\n</CENTER>\r\n"
     "<CENTER><P>Your browser sent a bad request, such as a POST without a Content-Length.</P></CENTER>\r\n"
     "</BODY></HTML>\r\n";
 int sockets[2];
 char actual[8192];
 size_t actual_length;

 if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0)
  fail("socketpair failed");
 bad_request(sockets[1]);
 actual_length = finish_and_read_response(sockets, actual, sizeof(actual));
 expect_bytes(actual, actual_length, expected,
              "bad request response bytes changed");
}

static void test_builds_static_headers_without_socket(void)
{
 static const char expected[] =
     "HTTP/1.0 200 OK\r\n"
     "Server: Tinyhttpd/1.0\r\n"
     "Content-Type: text/html\r\n"
     "Content-Length: 3\r\n"
     "Connection: close\r\n"
     "\r\n";
 http_response_buffer_t response;

 if (http_build_static_headers(&response, "index.html", 3) != 0)
  fail("could not build static response headers");
 expect_bytes(response.data, response.length, expected,
              "static response builder bytes changed");
}

static void test_builds_options_response_without_socket(void)
{
 static const char expected[] =
     "HTTP/1.0 200 OK\r\n"
     "Server: Tinyhttpd/1.0\r\n"
     "Allow: GET, POST, HEAD, OPTIONS\r\n"
     "Access-Control-Allow-Origin: *\r\n"
     "Access-Control-Allow-Methods: GET, POST, HEAD, OPTIONS\r\n"
     "Access-Control-Allow-Headers: Content-Type\r\n"
     "\r\n";
 http_response_buffer_t response;

 if (http_build_options_response(&response) != 0)
  fail("could not build OPTIONS response");
 expect_bytes(response.data, response.length, expected,
              "OPTIONS response builder bytes changed");
}

int main(void)
{
 test_static_headers_wire_bytes();
 test_bad_request_wire_bytes();
 test_builds_static_headers_without_socket();
 test_builds_options_response_without_socket();
 puts("PASS: HTTP response byte tests completed");
 return EXIT_SUCCESS;
}
