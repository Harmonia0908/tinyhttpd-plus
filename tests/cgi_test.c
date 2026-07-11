#include "cgi.h"

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

int main(int argc, char *argv[])
{
 char query[32];
 char response[2048];
 size_t used = 0;
 int sockets[2];
 int status;

 if (argc != 2)
  fail("expected CGI probe path");
 if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0)
  fail("socketpair failed");

 snprintf(query, sizeof(query), "%d", sockets[1]);
 status = handle_cgi(sockets[1], argv[1], "GET", query, 0, -1);
 if (status != 200)
  fail("CGI probe did not complete successfully");

 while (used + 1 < sizeof(response))
 {
  ssize_t count = read(sockets[0], response + used, sizeof(response) - used - 1);
  if (count < 0)
   fail("could not read CGI response");
  if (count == 0)
   break;
  used += (size_t)count;
 }
 response[used] = '\0';
 close(sockets[0]);

 if (strstr(response, "\r\n\r\nCLOSED\n") == NULL)
 {
  fprintf(stderr, "CGI response:\n%s\n", response);
  fail("CGI child inherited the client socket across exec");
 }

 puts("PASS: CGI descriptor inheritance test completed");
 return EXIT_SUCCESS;
}
