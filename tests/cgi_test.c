#include "cgi.h"
#include "utils.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define CONCURRENT_CGI_COUNT 16

typedef struct {
 const char *probe_path;
 int client_fd;
 int peer_fd;
 int passed;
} probe_task_t;

static pthread_mutex_t start_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t ready_condition = PTHREAD_COND_INITIALIZER;
static pthread_cond_t start_condition = PTHREAD_COND_INITIALIZER;
static int ready_count;
static int start_tasks;

static void fail(const char *message)
{
 fprintf(stderr, "FAIL: %s\n", message);
 exit(EXIT_FAILURE);
}

static void *run_probe(void *argument)
{
 probe_task_t *task = argument;
 char response[2048];
 size_t used = 0;
 int status;

 pthread_mutex_lock(&start_mutex);
 ready_count++;
 pthread_cond_signal(&ready_condition);
 while (!start_tasks)
  pthread_cond_wait(&start_condition, &start_mutex);
 pthread_mutex_unlock(&start_mutex);

 status = handle_cgi(task->client_fd, task->probe_path, "GET", "", 0, -1);
 if (status != 200)
  return NULL;

 while (used + 1 < sizeof(response))
 {
  ssize_t count = read(task->peer_fd, response + used,
                       sizeof(response) - used - 1);
  if (count < 0)
   return NULL;
  if (count == 0)
   break;
  used += (size_t)count;
 }
 response[used] = '\0';
 close(task->peer_fd);
 task->passed = strstr(response, "\r\n\r\nNO_EXTRA_FDS\n") != NULL;
 return NULL;
}

int main(int argc, char *argv[])
{
 probe_task_t tasks[CONCURRENT_CGI_COUNT];
 pthread_t threads[CONCURRENT_CGI_COUNT];
 int i;

 if (argc != 2)
  fail("expected CGI probe path");

 for (i = 0; i < CONCURRENT_CGI_COUNT; i++)
 {
  int sockets[2];

  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0)
   fail("socketpair failed");
  if (set_cloexec(sockets[0]) != 0 || set_cloexec(sockets[1]) != 0)
   fail("could not mark test socket close-on-exec");
  tasks[i].probe_path = argv[1];
  tasks[i].peer_fd = sockets[0];
  tasks[i].client_fd = sockets[1];
  tasks[i].passed = 0;
  if (pthread_create(&threads[i], NULL, run_probe, &tasks[i]) != 0)
   fail("could not create CGI test thread");
 }

 pthread_mutex_lock(&start_mutex);
 while (ready_count != CONCURRENT_CGI_COUNT)
  pthread_cond_wait(&ready_condition, &start_mutex);
 start_tasks = 1;
 pthread_cond_broadcast(&start_condition);
 pthread_mutex_unlock(&start_mutex);

 for (i = 0; i < CONCURRENT_CGI_COUNT; i++)
 {
  pthread_join(threads[i], NULL);
  if (!tasks[i].passed)
   fail("concurrent CGI inherited an unrelated descriptor");
 }

 puts("PASS: concurrent CGI descriptor inheritance test completed");
 return EXIT_SUCCESS;
}
