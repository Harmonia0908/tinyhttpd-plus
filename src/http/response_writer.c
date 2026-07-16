#include "response.h"

#include "http_response.h"
#include "net_io.h"

static void write_response(int client, const http_response_buffer_t *response)
{
 net_write_all(client, response->data, response->length);
}

void send_error_page(int client, int status_code, const char *status_text,
                     const char *message)
{
 http_response_buffer_t response;

 if (http_build_error_response(&response, status_code, status_text, message) == 0)
  write_response(client, &response);
}

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
 http_response_buffer_t response;

 if (http_build_static_headers(&response, filename, content_length) == 0)
  write_response(client, &response);
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
