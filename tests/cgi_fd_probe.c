#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
 const char *query = getenv("QUERY_STRING");
 char *end = NULL;
 long fd;
 int open_after_exec;

 if (query == NULL)
  return EXIT_FAILURE;

 errno = 0;
 fd = strtol(query, &end, 10);
 if (errno != 0 || end == query || *end != '\0' || fd < 0)
  return EXIT_FAILURE;

 errno = 0;
 open_after_exec = fcntl((int)fd, F_GETFD);
 printf("Content-Type: text/plain\r\n\r\n%s\n",
        open_after_exec == -1 && errno == EBADF ? "CLOSED" : "LEAKED");
 return EXIT_SUCCESS;
}
