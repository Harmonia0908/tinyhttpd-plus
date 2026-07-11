#include "response.h"

#include "mime.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>

void bad_request(int client)
{
 send_error_page(client, 400, "BAD REQUEST", 
  "Your browser sent a bad request, such as a POST without a Content-Length.");
}

void cannot_execute(int client)
{
 send_error_page(client, 500, "Internal Server Error", 
  "Error prohibited CGI execution.");
}

void headers(int client, const char *filename, off_t content_length)
{
 char buf[1024];
 const char *mime_type;

 mime_type = get_mime_type(filename);

 snprintf(buf, sizeof(buf), "HTTP/1.0 200 OK\r\n");
 send_all(client, buf, strlen(buf));
 snprintf(buf, sizeof(buf), "%s", SERVER_STRING);
 send_all(client, buf, strlen(buf));
 snprintf(buf, sizeof(buf), "Content-Type: %s\r\n", mime_type);
 send_all(client, buf, strlen(buf));
 snprintf(buf, sizeof(buf), "Content-Length: %lld\r\n", (long long)content_length);
 send_all(client, buf, strlen(buf));
 snprintf(buf, sizeof(buf), "Connection: close\r\n");
 send_all(client, buf, strlen(buf));
 snprintf(buf, sizeof(buf), "\r\n");
 send_all(client, buf, strlen(buf));
}

void send_error_page(int client, int status_code, const char *status_text, const char *message)
{
 char buf[1024];
 char body[4096];
 size_t body_len;

 snprintf(body, sizeof(body),
          "<HTML><HEAD><TITLE>%d %s</TITLE></HEAD>\r\n"
          "<BODY><CENTER><H1>%d %s</H1></CENTER>\r\n"
          "<HR><CENTER>%s</CENTER>\r\n"
          "<CENTER><P>%s</P></CENTER>\r\n"
          "</BODY></HTML>\r\n",
          status_code, status_text, status_code, status_text,
          SERVER_STRING, message);
 body_len = strlen(body);

 snprintf(buf, sizeof(buf), "HTTP/1.0 %d %s\r\n", status_code, status_text);
 send_all(client, buf, strlen(buf));
 snprintf(buf, sizeof(buf), "%s", SERVER_STRING);
 send_all(client, buf, strlen(buf));
 snprintf(buf, sizeof(buf), "Content-Type: text/html\r\n");
 send_all(client, buf, strlen(buf));
 snprintf(buf, sizeof(buf), "Content-Length: %lld\r\n", (long long)body_len);
 send_all(client, buf, strlen(buf));
 snprintf(buf, sizeof(buf), "Connection: close\r\n");
 send_all(client, buf, strlen(buf));
 snprintf(buf, sizeof(buf), "\r\n");
 send_all(client, buf, strlen(buf));
 send_all(client, body, body_len);
}

void not_found(int client)
{
 send_error_page(client, 404, "NOT FOUND", 
  "The server could not fulfill your request because the resource specified is unavailable or nonexistent.");
}

void unimplemented(int client)
{
 send_error_page(client, 501, "Method Not Implemented", 
  "HTTP request method not supported.");
}

void send_413(int client)
{
 send_error_page(client, 413, "Payload Too Large", 
  "Request size exceeds the configured limit.");
}

void uri_too_long(int client)
{
 send_error_page(client, 414, "URI Too Long",
  "The requested URI exceeds the server limit.");
}
