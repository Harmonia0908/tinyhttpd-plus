#include "cgi.h"

#include "log.h"
#include "response.h"
#include "utils.h"

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

#define CGI_ENV_BUFFER_SIZE 1024

static int is_cgi_environment_entry(const char *entry)
{
 static const char *names[] = {
  "REQUEST_METHOD=", "QUERY_STRING=", "CONTENT_LENGTH="
 };
 size_t i;

 for (i = 0; i < sizeof(names) / sizeof(names[0]); i++)
 {
  size_t name_length = strlen(names[i]);
  if (strncmp(entry, names[i], name_length) == 0)
   return 1;
 }
 return 0;
}

static char **build_cgi_environment(const char *method,
                                    const char *query_string,
                                    int content_length,
                                    char method_env[CGI_ENV_BUFFER_SIZE],
                                    char query_env[CGI_ENV_BUFFER_SIZE],
                                    char length_env[CGI_ENV_BUFFER_SIZE])
{
 char **cgi_env;
 size_t environment_count = 0;
 size_t output_count = 0;
 size_t i;
 int result;
 int is_post = strcasecmp(method, "POST") == 0;

 result = snprintf(method_env, CGI_ENV_BUFFER_SIZE,
                   "REQUEST_METHOD=%s", method);
 if (result < 0 || result >= CGI_ENV_BUFFER_SIZE)
  return NULL;

 if (is_post)
  result = snprintf(length_env, CGI_ENV_BUFFER_SIZE,
                    "CONTENT_LENGTH=%d", content_length);
 else
  result = snprintf(query_env, CGI_ENV_BUFFER_SIZE,
                    "QUERY_STRING=%s", query_string != NULL ? query_string : "");
 if (result < 0 || result >= CGI_ENV_BUFFER_SIZE)
  return NULL;

 while (environ[environment_count] != NULL)
  environment_count++;
 if (environment_count > (SIZE_MAX / sizeof(*cgi_env)) - 3)
  return NULL;

 cgi_env = malloc((environment_count + 3) * sizeof(*cgi_env));
 if (cgi_env == NULL)
  return NULL;

 for (i = 0; i < environment_count; i++)
 {
  if (!is_cgi_environment_entry(environ[i]))
   cgi_env[output_count++] = environ[i];
 }
 cgi_env[output_count++] = method_env;
 cgi_env[output_count++] = is_post ? length_env : query_env;
 cgi_env[output_count] = NULL;
 return cgi_env;
}

static ssize_t read_retry(int fd, void *buffer, size_t length)
{
 ssize_t result;

 do {
  result = read(fd, buffer, length);
 } while (result < 0 && errno == EINTR);
 return result;
}

static ssize_t recv_retry(int fd, void *buffer, size_t length)
{
 ssize_t result;

 do {
  result = recv(fd, buffer, length, 0);
 } while (result < 0 && errno == EINTR);
 return result;
}

static int write_all_fd(int fd, const void *data, size_t length)
{
 const char *buffer = data;
 size_t written = 0;

 while (written < length)
 {
  ssize_t result = write(fd, buffer + written, length - written);
  if (result < 0 && errno == EINTR)
   continue;
  if (result <= 0)
   return -1;
  written += (size_t)result;
 }
 return 0;
}

static int wait_for_child(pid_t pid, int *status)
{
 pid_t result;

 do {
  result = waitpid(pid, status, 0);
 } while (result == -1 && errno == EINTR);
 return result == pid ? 0 : -1;
}

static void terminate_cgi_child(pid_t pid, int output_fd, int input_fd,
                                int *input_open, int *status)
{
 if (*input_open)
 {
  close(input_fd);
  *input_open = 0;
 }
 close(output_fd);
 kill(pid, SIGKILL);
 wait_for_child(pid, status);
}

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
 int is_post = strcasecmp(method, "POST") == 0;
 char method_env[CGI_ENV_BUFFER_SIZE];
 char query_env[CGI_ENV_BUFFER_SIZE];
 char length_env[CGI_ENV_BUFFER_SIZE];
 char **cgi_env;
 char *cgi_argv[2];
 struct sigaction default_action;

 char c;
 int cgi_input_open = 0;
 
 //http的content_length
 int content_length = -1;

 //默认字符
 buf[0] = 'A'; buf[1] = '\0';

 //GET/HEAD: accept_request已经读取并丢弃了header，这里不需要再读
 //POST: 使用上层传递的content_length
 if (is_post)
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

 cgi_env = build_cgi_environment(method, query_string, content_length,
                                 method_env, query_env, length_env);
 if (cgi_env == NULL)
 {
  log_error_message("CGI environment setup failed: %s", path);
  cannot_execute(client);
  close(client);
  return 500;
 }
 cgi_argv[0] = (char *)path;
 cgi_argv[1] = NULL;
 memset(&default_action, 0, sizeof(default_action));
 sigemptyset(&default_action.sa_mask);
 default_action.sa_handler = SIG_DFL;

 //GET/HEAD: 不需要读取body
 fork_fd_lock();
 //建立output管道
 if (pipe(cgi_output) < 0) {
  fork_fd_unlock();
  free(cgi_env);
  log_error_message("CGI execution failed: %s", path);
  cannot_execute(client);
  close(client);
  return 500;
 }

 //建立input管道
 if (pipe(cgi_input) < 0) {
  close(cgi_output[0]);
  close(cgi_output[1]);
  fork_fd_unlock();
  free(cgi_env);
  log_error_message("CGI execution failed: %s", path);
  cannot_execute(client);
  close(client);
  return 500;
 }
 if (set_cloexec(cgi_output[0]) == -1 ||
     set_cloexec(cgi_output[1]) == -1 ||
     set_cloexec(cgi_input[0]) == -1 ||
     set_cloexec(cgi_input[1]) == -1)
 {
  close(cgi_output[0]);
  close(cgi_output[1]);
  close(cgi_input[0]);
  close(cgi_input[1]);
  fork_fd_unlock();
  free(cgi_env);
  log_error_message("CGI descriptor setup failed: %s", path);
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
  fork_fd_unlock();
  free(cgi_env);
  log_error_message("CGI execution failed: %s", path);
  cannot_execute(client);
  close(client);
  return 500;
 }
 if (pid != 0)
  fork_fd_unlock();
 if (pid == 0)  /* child: CGI script */
 {
  // cgi_output这个pipe的写端，重定向到标准输出流，
  // 即cgi脚本的控制台输出，会传递到cgi_output这个pipe中。
  // 后续父进程可以从cgi_output里读cgi脚本处理结果
  if (dup2(cgi_output[1], STDOUT_FILENO) == -1)
   _exit(126);
  // cgi_input这个pipe的读端，重定向到标准输入流。
  // 这意味着以后从标准输入读取数据时，实际上是从cgi_input这个pipe中读取数据。
  // 后续父进程向cgi_input里写入数据，等于向标准输入流写入数据
  if (dup2(cgi_input[0], STDIN_FILENO) == -1)
   _exit(126);

  //在子进程中，关闭另外2个pipe的端口
  close(cgi_output[0]);
  close(cgi_output[1]);
  close(cgi_input[0]);
  close(cgi_input[1]);
  close(client);

  if (sigaction(SIGPIPE, &default_action, NULL) == -1 ||
      sigaction(SIGALRM, &default_action, NULL) == -1)
   _exit(126);
  alarm(CGI_TIMEOUT_SECONDS);
  //替换后续代码的进程镜像，执行cgi脚本。
  execve(path, cgi_argv, cgi_env);
  //int m = execl(path, path, NULL);
  //如果path有问题，例如将html网页改成可执行的，但是执行后m为-1
  //退出子进程，管道被破坏，但是父进程还在往里面写东西，触发Program received signal SIGPIPE, Broken pipe.
  _exit(127);
 } else {    /* parent */

      free(cgi_env);

	  //关闭无用管道口
	  close(cgi_output[1]);
	  close(cgi_input[0]);
	  if (!is_post) {
	   close(cgi_input[1]);
	   cgi_input_open = 0;
	  }
	  if (is_post) {
	   size_t remaining = (size_t)content_length;
	   while (remaining > 0) {
	    size_t chunk = remaining < sizeof(buf) ? remaining : sizeof(buf);
	    ssize_t n = recv_retry(client, buf, chunk);
	    if (n <= 0) {
	     terminate_cgi_child(pid, cgi_output[0], cgi_input[1],
	                         &cgi_input_open, &status);
	     bad_request(client);
	     close(client);
	     return 400;
	    }
	    if (write_all_fd(cgi_input[1], buf, (size_t)n) != 0) {
	      terminate_cgi_child(pid, cgi_output[0], cgi_input[1],
	                          &cgi_input_open, &status);
	      log_error_message("CGI execution failed: %s", path);
	      cannot_execute(client);
	      close(client);
	      return 500;
	    }
	    remaining -= (size_t)n;
	   }
	  }
	  if (cgi_input_open) {
	   close(cgi_input[1]);
	   cgi_input_open = 0;
	  }
  //从output管道读到子进程处理后的信息，然后send出去
  //其实是cgi脚本运行后，向控制台打印的标准输出流，被重定向到cgi_output这个pipe里。
  //因此父进程是截获了cgi脚本的运行结果，转发给http客户端了。

  if (read_retry(cgi_output[0], &c, 1) <= 0)
  {
   close(cgi_output[0]);
   wait_for_child(pid, &status);
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
	   } while (read_retry(cgi_output[0], &c, 1) > 0);
   // 读取并丢弃剩余的body
   while (read_retry(cgi_output[0], &c, 1) > 0)
    ;
  }
  else
	  {
	   // 转发整个CGI输出（头部+body）
	   do
	    if (send_all(client, &c, 1) < 0)
	     break;
	   while (read_retry(cgi_output[0], &c, 1) > 0);
	  }

	  //完成操作后关闭管道
	  close(cgi_output[0]);
	  if (cgi_input_open)
	   close(cgi_input[1]);

  //等待子进程返回
  if (wait_for_child(pid, &status) != 0 ||
      !WIFEXITED(status) || WEXITSTATUS(status) != 0)
  {
   log_error_message("CGI execution failed: %s", path);
   close(client);
   return 500;
  }

  close(client);
  return 200;
 }
}
