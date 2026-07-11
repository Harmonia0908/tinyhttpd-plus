#include "server.h"

#include "config.h"
#include "request.h"
#include "threadpool.h"
#include "utils.h"

#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

static volatile sig_atomic_t running = 1;

static void shutdown_handler(int sig)
{
 (void)sig;
 running = 0;
}

void handle_client(int client) {
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    accept_request(client);
}

int startup(u_short *port)
{
 int httpd = 0;
 struct sockaddr_in name;
 int opt = 1;

 httpd = socket(PF_INET, SOCK_STREAM, 0);
 if (httpd == -1)
  error_die("socket");
 if (set_cloexec(httpd) == -1)
  error_die("fcntl(FD_CLOEXEC)");
 
 // 设置 SO_REUSEADDR，避免端口占用问题
 if (setsockopt(httpd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
  error_die("setsockopt");
 }

 memset(&name, 0, sizeof(name));
 name.sin_family = AF_INET;
 name.sin_port = htons(*port);
 name.sin_addr.s_addr = htonl(INADDR_ANY);
 //绑定socket
 if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
  error_die("bind");
 //如果端口没有设置，提供个随机端口
 if (*port == 0)  /* if dynamically allocating a port */
 {
  socklen_t  namelen = sizeof(name);
  if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1)
   error_die("getsockname");
  *port = ntohs(name.sin_port);
 }
 //监听
 if (listen(httpd, 5) < 0)
  error_die("listen");
 return(httpd);
}

int server_run(u_short port)
{
 int server_sock = -1;
 int client_sock = -1;
 const server_config_t *cfg = get_server_config();
 struct sockaddr_in client_name;
 socklen_t client_name_len = sizeof(client_name);

 signal(SIGPIPE, SIG_IGN);
 {
  struct sigaction sa;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sa.sa_handler = shutdown_handler;
  sigaction(SIGINT, &sa, NULL);
  sigaction(SIGTERM, &sa, NULL);
 }

 server_sock = startup(&port);
 printf("httpd running on port %d\n", port);

 threadpool_init(cfg->thread_num, 1000, handle_client);

 while (running)
 {
  client_sock = accept(server_sock,
                       (struct sockaddr *)&client_name,
                       &client_name_len);
  if (client_sock == -1) {
   if (errno == EINTR) {
    if (!running)
     break;
    continue;
   }
   if (errno == EAGAIN || errno == EWOULDBLOCK) {
    continue;
   }
   error_die("accept");
  }

  if (set_cloexec(client_sock) == -1) {
   perror("fcntl(FD_CLOEXEC)");
   close(client_sock);
   continue;
  }

  if (threadpool_submit(client_sock) != 0) {
   close(client_sock);
  }
 }

 threadpool_shutdown();
 close(server_sock);

 return 0;
}
