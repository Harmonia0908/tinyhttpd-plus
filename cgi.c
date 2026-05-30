#include "cgi.h"

#include "log.h"
#include "response.h"
#include "utils.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

int handle_cgi(int client, const char *path, const char *method,
               const char *query_string, int is_head, int content_length)
{
 if (strcasecmp(method, "POST") == 0)
 {
  if (content_length == -1) {
   bad_request(client);
   close(client);
   return 400;
  }
  if (content_length > MAX_REQUEST_SIZE) {
   send_413(client);
   close(client);
   return 413;
  }
 }

 return execute_cgi(client, path, method, query_string, is_head, content_length);
}

int execute_cgi(int client, const char *path,
                const char *method, const char *query_string, int is_head,
                int req_content_length)
{
//缓冲区
 char buf[1024];

 //2个管道，用于父子进程的双向通信
 //这里父进程才是和cgi脚本直接通信的。
 //子进程主要负责把pipe和标准输入输出连接起来，以及配置环境变量，但其实这些事情在父进程里配置也完全可以
 //综合来看，父进程从http客户端接收post请求，传递给cgi脚本，再把cgi脚本的输出结果，转发给http客户端。
 //而子进程，真正不可替代的作用，是调用execl函数，启动cgi脚本。
 int cgi_output[2];
 int cgi_input[2];

 //进程pid和状态
 pid_t pid;
 int status;

 char c;
 int cgi_input_open = 0;
 
 //http的content_length
 int content_length = -1;

 //默认字符
 buf[0] = 'A'; buf[1] = '\0';

 //GET/HEAD: accept_request已经读取并丢弃了header，这里不需要再读
 //POST: 使用上层传递的content_length
 if (strcasecmp(method, "POST") == 0)
 {
  if (req_content_length < 0) {
   bad_request(client);
   close(client);
   return 400;
  }
  if (req_content_length > MAX_REQUEST_SIZE) {
   send_413(client);
   close(client);
   return 413;
  }
  content_length = req_content_length;
 }
 //GET/HEAD: 不需要读取body
 //建立output管道
 if (pipe(cgi_output) < 0) {
  log_error_message("CGI execution failed: %s", path);
  cannot_execute(client);
  close(client);
  return 500;
 }

 //建立input管道
 if (pipe(cgi_input) < 0) {
  close(cgi_output[0]);
  close(cgi_output[1]);
  log_error_message("CGI execution failed: %s", path);
  cannot_execute(client);
  close(client);
  return 500;
 }
 cgi_input_open = 1;
 //       fork后管道都复制了一份，都是一样的
 //       子进程关闭2个无用的端口，避免浪费
 //       ×<------------------------->1    output
 //       0<-------------------------->×   input

 //       父进程关闭2个无用的端口，避免浪费
 //       0<-------------------------->×   output
 //       ×<------------------------->1    input
 //       此时父子进程已经可以通信


 //fork进程，子进程用于执行CGI
 //父进程用于收数据以及发送子进程处理的回复数据
 if ( (pid = fork()) < 0 ) {
  close(cgi_output[0]);
  close(cgi_output[1]);
  close(cgi_input[0]);
  close(cgi_input[1]);
  log_error_message("CGI execution failed: %s", path);
  cannot_execute(client);
  close(client);
  return 500;
 }
 if (pid == 0)  /* child: CGI script */
 {
  char meth_env[255];
  char query_env[255];
  char length_env[255];
  int env_len;

  // cgi_output这个pipe的写端，重定向到标准输出流，
  // 即cgi脚本的控制台输出，会传递到cgi_output这个pipe中。
  // 后续父进程可以从cgi_output里读cgi脚本处理结果
  dup2(cgi_output[1], 1);
  // cgi_input这个pipe的读端，重定向到标准输入流。
  // 这意味着以后从标准输入读取数据时，实际上是从cgi_input这个pipe中读取数据。
  // 后续父进程向cgi_input里写入数据，等于向标准输入流写入数据
  dup2(cgi_input[0], 0);

  //在子进程中，关闭另外2个pipe的端口
  close(cgi_output[0]);
  close(cgi_output[1]);
  close(cgi_input[0]);
  close(cgi_input[1]);

  //写入新的环境变量，用于后续cgi脚本使用
  env_len = snprintf(meth_env, sizeof(meth_env), "REQUEST_METHOD=%s", method);
  if (env_len < 0 || (size_t)env_len >= sizeof(meth_env))
   exit(1);
  putenv(meth_env);
  if (strcasecmp(method, "GET") == 0 || strcasecmp(method, "HEAD") == 0) {
   env_len = snprintf(query_env, sizeof(query_env), "QUERY_STRING=%s", query_string);
   if (env_len < 0 || (size_t)env_len >= sizeof(query_env))
    exit(1);
   putenv(query_env);
  }
  else {   /* POST */
   env_len = snprintf(length_env, sizeof(length_env), "CONTENT_LENGTH=%d", content_length);
   if (env_len < 0 || (size_t)env_len >= sizeof(length_env))
    exit(1);
   putenv(length_env);
  }
  signal(SIGPIPE, SIG_DFL);
  signal(SIGALRM, SIG_DFL);
  alarm(CGI_TIMEOUT_SECONDS);
  //替换后续代码的进程镜像，执行cgi脚本。
  execl(path, path, NULL);
  //int m = execl(path, path, NULL);
  //如果path有问题，例如将html网页改成可执行的，但是执行后m为-1
  //退出子进程，管道被破坏，但是父进程还在往里面写东西，触发Program received signal SIGPIPE, Broken pipe.
  exit(127);
 } else {    /* parent */

	  //关闭无用管道口
	  close(cgi_output[1]);
	  close(cgi_input[0]);
	  if (strcasecmp(method, "POST") != 0) {
	   close(cgi_input[1]);
	   cgi_input_open = 0;
	  }
	  if (strcasecmp(method, "POST") == 0) {
	   int remaining = content_length;
	   while (remaining > 0) {
	    size_t chunk = (remaining < (int)sizeof(buf)) ? (size_t)remaining : sizeof(buf);
	    ssize_t n = recv(client, buf, chunk, 0);
	    if (n <= 0) {
	     close(cgi_input[1]);
	     cgi_input_open = 0;
	     close(cgi_output[0]);
	     kill(pid, SIGKILL);
	     waitpid(pid, &status, 0);
	     bad_request(client);
	     close(client);
	     return 400;
	    }
	    size_t written = 0;
	    while (written < (size_t)n) {
	     ssize_t w = write(cgi_input[1], buf + written, (size_t)n - written);
	     if (w <= 0) {
	      close(cgi_input[1]);
	      cgi_input_open = 0;
	      close(cgi_output[0]);
	      kill(pid, SIGKILL);
	      waitpid(pid, &status, 0);
	      log_error_message("CGI execution failed: %s", path);
	      cannot_execute(client);
	      close(client);
	      return 500;
	     }
	     written += (size_t)w;
	    }
	    remaining -= (int)n;
	   }
	  }
	  if (cgi_input_open) {
	   close(cgi_input[1]);
	   cgi_input_open = 0;
	  }
  //从output管道读到子进程处理后的信息，然后send出去
  //其实是cgi脚本运行后，向控制台打印的标准输出流，被重定向到cgi_output这个pipe里。
  //因此父进程是截获了cgi脚本的运行结果，转发给http客户端了。

  if (read(cgi_output[0], &c, 1) <= 0)
  {
   close(cgi_output[0]);
   waitpid(pid, &status, 0);
   log_error_message("CGI execution failed: %s", path);
   cannot_execute(client);
   close(client);
   return 500;
  }

  snprintf(buf, sizeof(buf), "HTTP/1.0 200 OK\r\n");
  send_all(client, buf, strlen(buf));
  
  snprintf(buf, sizeof(buf), "%s", SERVER_STRING);
  send_all(client, buf, strlen(buf));

	  if (is_head)
	  {
	   int b0 = -1;
	   int b1 = -1;
	   int b2 = -1;
	   int b3 = -1;
	   
	   do
	   {
	    // 发送当前字符
	    if (send_all(client, &c, 1) < 0)
	     break;

	    b0 = b1;
	    b1 = b2;
	    b2 = b3;
	    b3 = (unsigned char)c;
	    
	    // 检查头部结束条件：\r\n\r\n 或 \n\n
	    if ((b2 == '\n' && b3 == '\n') ||
	        (b0 == '\r' && b1 == '\n' && b2 == '\r' && b3 == '\n')) {
	     break;
	    }
	   } while (read(cgi_output[0], &c, 1) > 0);
   // 读取并丢弃剩余的body
   while (read(cgi_output[0], &c, 1) > 0)
    ;
  }
  else
	  {
	   // 转发整个CGI输出（头部+body）
	   do
	    if (send_all(client, &c, 1) < 0)
	     break;
	   while (read(cgi_output[0], &c, 1) > 0);
	  }

	  //完成操作后关闭管道
	  close(cgi_output[0]);
	  if (cgi_input_open)
	   close(cgi_input[1]);

  //等待子进程返回
  waitpid(pid, &status, 0);
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
  {
   log_error_message("CGI execution failed: %s", path);
   close(client);
   return 500;
  }

  close(client);
  return 200;
 }
}
