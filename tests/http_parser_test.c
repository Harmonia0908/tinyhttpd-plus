#include "http_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fail(const char *message)
{
 fprintf(stderr, "FAIL: %s\n", message);
 exit(EXIT_FAILURE);
}

static void test_parses_valid_request_line_from_buffer(void)
{
 static const char request_line[] = "POST /date.cgi HTTP/1.0\r\n";
 char method[16];
 char url[255];
 int status;

 status = http_parse_request_line(request_line, sizeof(request_line) - 1,
                                  method, sizeof(method), url, sizeof(url));

 if (status != 0 || strcmp(method, "POST") != 0 ||
     strcmp(url, "/date.cgi") != 0)
  fail("valid request line buffer was not parsed");
}

static void test_parses_headers_from_buffer(void)
{
 static const char headers[] =
     "Host: localhost\r\n"
     "Content-Length: 42\r\n"
     "X-Test: value\r\n"
     "\r\n";
 int content_length = -1;
 int status;

 status = http_parse_headers(headers, sizeof(headers) - 1, &content_length);

 if (status != 0 || content_length != 42)
  fail("valid header buffer was not parsed");
}

static void expect_request_status(const char *line, size_t length,
                                  int expected_status,
                                  const char *message)
{
 char method[16];
 char url[255];
 int status = http_parse_request_line(line, length, method, sizeof(method),
                                      url, sizeof(url));

 if (status != expected_status)
  fail(message);
}

static void expect_header_status(const char *headers, size_t length,
                                 int expected_status,
                                 const char *message)
{
 int content_length = -1;
 int status = http_parse_headers(headers, length, &content_length);

 if (status != expected_status)
  fail(message);
}

static void test_preserves_request_line_error_statuses(void)
{
 char overlong_uri[400];
 char overlong_line[1023];
 size_t offset;

 expect_request_status("GET\r\n", 5, 400,
                       "missing URI did not return 400");
 expect_request_status("PATCH / HTTP/1.0\r\n", 18, 501,
                       "unsupported method did not return 501");

 offset = (size_t)snprintf(overlong_uri, sizeof(overlong_uri), "GET /");
 memset(overlong_uri + offset, 'a', 300);
 offset += 300;
 memcpy(overlong_uri + offset, " HTTP/1.0\r\n", 11);
 offset += 11;
 expect_request_status(overlong_uri, offset, 414,
                       "overlong URI did not return 414");

 memset(overlong_line, 'a', sizeof(overlong_line));
 expect_request_status(overlong_line, sizeof(overlong_line), 400,
                       "overlong request line did not return 400");
}

static void test_preserves_header_error_statuses(void)
{
 static const char duplicate_content_length[] =
     "Content-Length: 3\r\nContent-Length: 4\r\n\r\n";
 char long_line[1023];
 char large_headers[10000];
 size_t offset = 0;
 int i;

 expect_header_status(duplicate_content_length,
                      sizeof(duplicate_content_length) - 1, 400,
                      "duplicate Content-Length did not return 400");
 expect_header_status("Content-Length: -1\r\n\r\n", 22, 400,
                      "negative Content-Length did not return 400");
 expect_header_status("Content-Length: 3x\r\n\r\n", 22, 400,
                      "invalid Content-Length did not return 400");
 expect_header_status("Content-Length: 1048577\r\n\r\n", 27, 413,
                      "oversized Content-Length did not return 413");

 memset(long_line, 'a', sizeof(long_line));
 expect_header_status(long_line, sizeof(long_line), 400,
                      "overlong header line did not return 400");

 for (i = 0; i < 9; i++)
 {
  int written = snprintf(large_headers + offset,
                         sizeof(large_headers) - offset, "X-%d: ", i);
  if (written < 0)
   fail("could not create large header fixture");
  offset += (size_t)written;
  memset(large_headers + offset, 'a', 1000);
  offset += 1000;
  memcpy(large_headers + offset, "\r\n", 2);
  offset += 2;
 }
 memcpy(large_headers + offset, "\r\n", 2);
 offset += 2;
 expect_header_status(large_headers, offset, 413,
                      "aggregate header limit did not return 413");
}

static void test_accepts_current_content_length_boundaries(void)
{
 static const char headers[] = "Content-Length: 1048576\n\n";
 int content_length = -1;
 int status;

 status = http_parse_headers(headers, sizeof(headers) - 1, &content_length);
 if (status != 0 || content_length != 1048576)
  fail("maximum Content-Length boundary was rejected");
}

int main(void)
{
 test_parses_valid_request_line_from_buffer();
 test_parses_headers_from_buffer();
 test_preserves_request_line_error_statuses();
 test_preserves_header_error_statuses();
 test_accepts_current_content_length_boundaries();
 puts("PASS: HTTP parser buffer tests completed");
 return EXIT_SUCCESS;
}
