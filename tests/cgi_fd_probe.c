#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
 int fd;

 printf("Content-Type: text/plain\r\n\r\n");
 for (fd = 3; fd < 256; fd++)
 {
  errno = 0;
  if (fcntl(fd, F_GETFD) != -1 || errno != EBADF)
  {
   printf("LEAKED_FD=%d\n", fd);
   return EXIT_FAILURE;
  }
 }

 puts("NO_EXTRA_FDS");
 return EXIT_SUCCESS;
}
