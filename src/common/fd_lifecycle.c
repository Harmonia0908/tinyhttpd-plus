#include "fd_lifecycle.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

static pthread_mutex_t fork_fd_mutex = PTHREAD_MUTEX_INITIALIZER;

static void fork_fd_lock(void)
{
 pthread_mutex_lock(&fork_fd_mutex);
}

static void fork_fd_unlock(void)
{
 pthread_mutex_unlock(&fork_fd_mutex);
}

int accept_cloexec_blocking(int listener, struct sockaddr *address,
                            socklen_t *address_length)
{
 int client;

 fork_fd_lock();
 client = accept(listener, address, address_length);
 if (client != -1 &&
     (set_cloexec(client) == -1 || set_blocking(client) == -1))
 {
  int saved_errno = errno;
  close(client);
  client = -1;
  errno = saved_errno;
 }
 fork_fd_unlock();
 return client;
}

pid_t fork_with_cloexec_pipes(int output_pipe[2], int input_pipe[2])
{
 pid_t pid;

 fork_fd_lock();
 if (pipe(output_pipe) == -1)
 {
  fork_fd_unlock();
  return -1;
 }
 if (pipe(input_pipe) == -1)
 {
  int saved_errno = errno;
  close(output_pipe[0]);
  close(output_pipe[1]);
  fork_fd_unlock();
  errno = saved_errno;
  return -1;
 }
 if (set_cloexec(output_pipe[0]) == -1 ||
     set_cloexec(output_pipe[1]) == -1 ||
     set_cloexec(input_pipe[0]) == -1 ||
     set_cloexec(input_pipe[1]) == -1)
 {
  int saved_errno = errno;
  close(output_pipe[0]);
  close(output_pipe[1]);
  close(input_pipe[0]);
  close(input_pipe[1]);
  fork_fd_unlock();
  errno = saved_errno;
  return -1;
 }

 pid = fork();
 if (pid != 0)
  fork_fd_unlock();
 if (pid == -1)
 {
  int saved_errno = errno;
  close(output_pipe[0]);
  close(output_pipe[1]);
  close(input_pipe[0]);
  close(input_pipe[1]);
  errno = saved_errno;
 }
 return pid;
}

int open_cloexec(const char *path, int flags, mode_t mode)
{
 int fd;

 fork_fd_lock();
 fd = open(path, flags, mode);
 if (fd != -1 && set_cloexec(fd) == -1)
 {
  int saved_errno = errno;
  close(fd);
  fd = -1;
  errno = saved_errno;
 }
 fork_fd_unlock();
 return fd;
}

int set_cloexec(int fd)
{
 int flags;

 do {
  flags = fcntl(fd, F_GETFD);
 } while (flags == -1 && errno == EINTR);
 if (flags == -1)
  return -1;

 while (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) == -1)
 {
  if (errno != EINTR)
   return -1;
 }
 return 0;
}

int set_nonblocking(int fd)
{
 int flags;

 do {
  flags = fcntl(fd, F_GETFL);
 } while (flags == -1 && errno == EINTR);
 if (flags == -1)
  return -1;

 while (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
 {
  if (errno != EINTR)
   return -1;
 }
 return 0;
}

int set_blocking(int fd)
{
 int flags;

 do {
  flags = fcntl(fd, F_GETFL);
 } while (flags == -1 && errno == EINTR);
 if (flags == -1)
  return -1;

 while (fcntl(fd, F_SETFL, flags & ~O_NONBLOCK) == -1)
 {
  if (errno != EINTR)
   return -1;
 }
 return 0;
}

void error_die(const char *sc)
{
 perror(sc);
 exit(1);
}
