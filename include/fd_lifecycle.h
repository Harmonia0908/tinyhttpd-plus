#ifndef TINYHTTPD_FD_LIFECYCLE_H
#define TINYHTTPD_FD_LIFECYCLE_H

#include <sys/socket.h>
#include <sys/types.h>

void error_die(const char *sc);
int accept_cloexec_blocking(int listener, struct sockaddr *address,
                            socklen_t *address_length);
pid_t fork_with_cloexec_pipes(int output_pipe[2], int input_pipe[2]);
int open_cloexec(const char *path, int flags, mode_t mode);
int set_cloexec(int fd);
int set_blocking(int fd);
int set_nonblocking(int fd);

#endif
