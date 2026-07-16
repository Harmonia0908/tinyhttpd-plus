#include "net_io.h"

#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>

int net_write_all(int fd, const void *data, size_t length)
{
 const char *buffer = data;
 size_t sent = 0;

 while (sent < length)
 {
  ssize_t result = send(fd, buffer + sent, length - sent, 0);
  if (result < 0)
  {
   if (errno == EINTR)
    continue;
   return -1;
  }
  if (result == 0)
   return -1;
  sent += (size_t)result;
 }

 return 0;
}

int net_read_line(int fd, char *buffer, size_t capacity)
{
 size_t used = 0;
 char c = '\0';
 ssize_t result;

 if (buffer == NULL || capacity == 0)
  return -1;

 while (used + 1 < capacity && c != '\n')
 {
  do {
   result = recv(fd, &c, 1, 0);
  } while (result < 0 && errno == EINTR);

  if (result > 0)
  {
   if (c == '\r')
   {
    do {
     result = recv(fd, &c, 1, MSG_PEEK);
    } while (result < 0 && errno == EINTR);
    if (result > 0 && c == '\n')
    {
     do {
      result = recv(fd, &c, 1, 0);
     } while (result < 0 && errno == EINTR);
     if (result <= 0)
     {
      buffer[used] = '\0';
      return -1;
     }
    }
    else
     c = '\n';
   }
   buffer[used++] = c;
  }
  else if (result == 0)
  {
   buffer[used] = '\0';
   return (int)used;
  }
  else
  {
   if (errno == EAGAIN || errno == EWOULDBLOCK)
    fprintf(stderr, "[timeout] client_fd=%d\n", fd);
   buffer[used] = '\0';
   return -1;
  }
 }

 buffer[used] = '\0';
 return (int)used;
}

int get_line(int sock, char *buf, int size)
{
 if (size <= 0)
 {
  buf[0] = '\0';
  return 0;
 }
 return net_read_line(sock, buf, (size_t)size);
}

int send_all(int client, const void *data, size_t len)
{
 return net_write_all(client, data, len);
}
