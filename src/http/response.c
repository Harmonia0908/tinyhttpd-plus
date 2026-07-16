#include "http_response.h"

#include "mime.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static int append_bytes(http_response_buffer_t *response,
                        const char *data, size_t length)
{
 if (response->length + length > sizeof(response->data))
  return -1;
 memcpy(response->data + response->length, data, length);
 response->length += length;
 return 0;
}

static int append_segment(http_response_buffer_t *response,
                          const char *format, ...)
{
 char segment[1024];
 va_list args;
 int written;

 va_start(args, format);
 written = vsnprintf(segment, sizeof(segment), format, args);
 va_end(args);
 if (written < 0)
  return -1;
 return append_bytes(response, segment, strlen(segment));
}

int http_build_static_headers(http_response_buffer_t *response,
                              const char *filename, off_t content_length)
{
 const char *mime_type;

 if (response == NULL || filename == NULL)
  return -1;
 response->length = 0;
 mime_type = get_mime_type(filename);

 if (append_segment(response, "HTTP/1.0 200 OK\r\n") != 0 ||
     append_segment(response, "%s", SERVER_STRING) != 0 ||
     append_segment(response, "Content-Type: %s\r\n", mime_type) != 0 ||
     append_segment(response, "Content-Length: %lld\r\n",
                    (long long)content_length) != 0 ||
     append_segment(response, "Connection: close\r\n") != 0 ||
     append_segment(response, "\r\n") != 0)
  return -1;

 return 0;
}

int http_build_error_response(http_response_buffer_t *response,
                              int status_code, const char *status_text,
                              const char *message)
{
 char body[4096];
 size_t body_length;
 int written;

 if (response == NULL || status_text == NULL || message == NULL)
  return -1;
 response->length = 0;

 written = snprintf(body, sizeof(body),
                    "<HTML><HEAD><TITLE>%d %s</TITLE></HEAD>\r\n"
                    "<BODY><CENTER><H1>%d %s</H1></CENTER>\r\n"
                    "<HR><CENTER>%s</CENTER>\r\n"
                    "<CENTER><P>%s</P></CENTER>\r\n"
                    "</BODY></HTML>\r\n",
                    status_code, status_text, status_code, status_text,
                    SERVER_STRING, message);
 if (written < 0)
  return -1;
 body_length = strlen(body);

 if (append_segment(response, "HTTP/1.0 %d %s\r\n",
                    status_code, status_text) != 0 ||
     append_segment(response, "%s", SERVER_STRING) != 0 ||
     append_segment(response, "Content-Type: text/html\r\n") != 0 ||
     append_segment(response, "Content-Length: %lld\r\n",
                    (long long)body_length) != 0 ||
     append_segment(response, "Connection: close\r\n") != 0 ||
     append_segment(response, "\r\n") != 0 ||
     append_bytes(response, body, body_length) != 0)
  return -1;

 return 0;
}

int http_build_options_response(http_response_buffer_t *response)
{
 if (response == NULL)
  return -1;
 response->length = 0;

 if (append_segment(response, "HTTP/1.0 200 OK\r\n") != 0 ||
     append_segment(response, "%s", SERVER_STRING) != 0 ||
     append_segment(response, "Allow: GET, POST, HEAD, OPTIONS\r\n") != 0 ||
     append_segment(response, "Access-Control-Allow-Origin: *\r\n") != 0 ||
     append_segment(response,
                    "Access-Control-Allow-Methods: GET, POST, HEAD, OPTIONS\r\n") != 0 ||
     append_segment(response,
                    "Access-Control-Allow-Headers: Content-Type\r\n") != 0 ||
     append_segment(response, "\r\n") != 0)
  return -1;

 return 0;
}
