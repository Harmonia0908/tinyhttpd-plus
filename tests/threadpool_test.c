#include "threadpool.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#define TEST_TASK_COUNT 8

static pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;
static int handled_count = 0;

static void fail(const char *message)
{
 fprintf(stderr, "FAIL: %s\n", message);
 exit(EXIT_FAILURE);
}

static void close_client(int client_fd)
{
 close(client_fd);
 pthread_mutex_lock(&count_mutex);
 handled_count++;
 pthread_mutex_unlock(&count_mutex);
}

static void test_invalid_initialization(void)
{
 if (threadpool_init(0, 1, close_client) == 0)
  fail("zero worker count was accepted");
 if (threadpool_init(THREADPOOL_MAX_THREADS + 1, 1, close_client) == 0)
  fail("excessive worker count was accepted");
 if (threadpool_init(1, 0, close_client) == 0)
  fail("zero queue size was accepted");
 if (threadpool_init(1, 1, NULL) == 0)
  fail("NULL handler was accepted");
 threadpool_shutdown();
}

static void test_processes_all_accepted_tasks(void)
{
 int peers[TEST_TASK_COUNT];
 int i;

 handled_count = 0;
 if (threadpool_init(2, TEST_TASK_COUNT, close_client) != 0)
  fail("valid thread pool initialization failed");

 for (i = 0; i < TEST_TASK_COUNT; i++)
 {
  int sockets[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockets) != 0)
   fail("socketpair failed");
  peers[i] = sockets[0];
  if (threadpool_submit(sockets[1]) != 0)
   fail("task submission failed");
 }

 threadpool_shutdown();
 threadpool_shutdown();

 for (i = 0; i < TEST_TASK_COUNT; i++)
  close(peers[i]);

 if (handled_count != TEST_TASK_COUNT)
  fail("shutdown did not drain accepted tasks");
 if (threadpool_get_processed_count() != TEST_TASK_COUNT)
  fail("processed task count is incorrect");
 if (threadpool_submit(-1) == 0)
  fail("task was accepted after shutdown");
}

int main(void)
{
 test_invalid_initialization();
 test_processes_all_accepted_tasks();
 puts("PASS: thread pool lifecycle tests completed");
 return EXIT_SUCCESS;
}
