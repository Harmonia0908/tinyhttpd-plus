#include "threadpool.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#define REPORT_INTERVAL 100

typedef struct threadpool_task {
 int client_fd;
 struct threadpool_task *next;
} threadpool_task_t;

typedef struct {
 threadpool_task_t *head;
 threadpool_task_t *tail;
 size_t task_count;
 size_t max_queue_size;
 int shutdown;
 int initialized;
 pthread_mutex_t mutex;
 pthread_cond_t task_available;
 pthread_t *threads;
 size_t thread_count;
 task_handler handler;
} threadpool_t;

static threadpool_t pool;
static pthread_mutex_t lifecycle_mutex = PTHREAD_MUTEX_INITIALIZER;
static int processed_count;

static threadpool_task_t *task_create(int client_fd)
{
 threadpool_task_t *task = malloc(sizeof(*task));

 if (task == NULL)
  return NULL;
 task->client_fd = client_fd;
 task->next = NULL;
 return task;
}

static void *worker(void *arg)
{
 (void)arg;

 for (;;)
 {
  threadpool_task_t *task;
  int count;

  pthread_mutex_lock(&pool.mutex);
  while (pool.task_count == 0 && !pool.shutdown)
   pthread_cond_wait(&pool.task_available, &pool.mutex);

  if (pool.task_count == 0 && pool.shutdown)
  {
   pthread_mutex_unlock(&pool.mutex);
   return NULL;
  }

  task = pool.head;
  pool.head = task->next;
  pool.task_count--;
  if (pool.head == NULL)
   pool.tail = NULL;
  pthread_mutex_unlock(&pool.mutex);

  pool.handler(task->client_fd);
  free(task);

  count = __atomic_add_fetch(&processed_count, 1, __ATOMIC_RELAXED);
  if (count % REPORT_INTERVAL == 0)
   fprintf(stderr, "[threadpool] processed %d requests\n", count);
 }
}

static void reset_pool_state(void)
{
 pool.head = NULL;
 pool.tail = NULL;
 pool.task_count = 0;
 pool.max_queue_size = 0;
 pool.shutdown = 0;
 pool.initialized = 0;
 pool.threads = NULL;
 pool.thread_count = 0;
 pool.handler = NULL;
}

int threadpool_init(int thread_count, int max_queue_size, task_handler handler)
{
 size_t created = 0;

 if (thread_count <= 0 || thread_count > THREADPOOL_MAX_THREADS ||
     max_queue_size <= 0 || handler == NULL)
  return -1;

 pthread_mutex_lock(&lifecycle_mutex);
 if (pool.initialized)
 {
  pthread_mutex_unlock(&lifecycle_mutex);
  return -1;
 }

 reset_pool_state();
 pool.max_queue_size = (size_t)max_queue_size;
 pool.thread_count = (size_t)thread_count;
 pool.handler = handler;
 __atomic_store_n(&processed_count, 0, __ATOMIC_RELAXED);

 if (pthread_mutex_init(&pool.mutex, NULL) != 0)
  goto fail;
 if (pthread_cond_init(&pool.task_available, NULL) != 0)
 {
  pthread_mutex_destroy(&pool.mutex);
  goto fail;
 }

 pool.threads = calloc(pool.thread_count, sizeof(*pool.threads));
 if (pool.threads == NULL)
 {
  pthread_cond_destroy(&pool.task_available);
  pthread_mutex_destroy(&pool.mutex);
  goto fail;
 }

 pool.initialized = 1;
 for (created = 0; created < pool.thread_count; created++)
 {
  if (pthread_create(&pool.threads[created], NULL, worker, NULL) != 0)
  {
   size_t i;

   pthread_mutex_lock(&pool.mutex);
   pool.shutdown = 1;
   pthread_cond_broadcast(&pool.task_available);
   pthread_mutex_unlock(&pool.mutex);
   for (i = 0; i < created; i++)
    pthread_join(pool.threads[i], NULL);
   free(pool.threads);
   pthread_cond_destroy(&pool.task_available);
   pthread_mutex_destroy(&pool.mutex);
   reset_pool_state();
   pthread_mutex_unlock(&lifecycle_mutex);
   return -1;
  }
 }

 pthread_mutex_unlock(&lifecycle_mutex);
 return 0;

fail:
 reset_pool_state();
 pthread_mutex_unlock(&lifecycle_mutex);
 return -1;
}

int threadpool_submit(int client_fd)
{
 threadpool_task_t *task;

 pthread_mutex_lock(&lifecycle_mutex);
 if (!pool.initialized)
 {
  pthread_mutex_unlock(&lifecycle_mutex);
  return -1;
 }

 pthread_mutex_lock(&pool.mutex);
 if (pool.shutdown || pool.task_count >= pool.max_queue_size)
 {
  size_t queue_size = pool.task_count;
  size_t max_queue_size = pool.max_queue_size;

  pthread_mutex_unlock(&pool.mutex);
  pthread_mutex_unlock(&lifecycle_mutex);
  if (queue_size >= max_queue_size)
   fprintf(stderr, "[threadpool] queue full, dropped connection "
                   "(fd=%d, queue_size=%zu)\n",
           client_fd, max_queue_size);
  return -1;
 }

 task = task_create(client_fd);
 if (task == NULL)
 {
  pthread_mutex_unlock(&pool.mutex);
  pthread_mutex_unlock(&lifecycle_mutex);
  fprintf(stderr, "[threadpool] memory allocation failed, "
                  "dropped connection (fd=%d)\n", client_fd);
  return -1;
 }

 if (pool.tail == NULL)
  pool.head = task;
 else
  pool.tail->next = task;
 pool.tail = task;
 pool.task_count++;
 pthread_cond_signal(&pool.task_available);
 pthread_mutex_unlock(&pool.mutex);
 pthread_mutex_unlock(&lifecycle_mutex);
 return 0;
}

int threadpool_get_processed_count(void)
{
 return __atomic_load_n(&processed_count, __ATOMIC_RELAXED);
}

void threadpool_shutdown(void)
{
 size_t i;

 pthread_mutex_lock(&lifecycle_mutex);
 if (!pool.initialized)
 {
  pthread_mutex_unlock(&lifecycle_mutex);
  return;
 }

 pthread_mutex_lock(&pool.mutex);
 pool.shutdown = 1;
 pthread_cond_broadcast(&pool.task_available);
 pthread_mutex_unlock(&pool.mutex);

 for (i = 0; i < pool.thread_count; i++)
  pthread_join(pool.threads[i], NULL);

 while (pool.head != NULL)
 {
  threadpool_task_t *task = pool.head;
  pool.head = task->next;
  free(task);
 }

 free(pool.threads);
 pthread_cond_destroy(&pool.task_available);
 pthread_mutex_destroy(&pool.mutex);
 reset_pool_state();
 pthread_mutex_unlock(&lifecycle_mutex);
}
