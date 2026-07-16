#ifndef THREADPOOL_H
#define THREADPOOL_H

#define DEFAULT_QUEUE_SIZE 1000
#define THREADPOOL_MAX_THREADS 100

typedef void (*task_handler)(int client_fd);

int threadpool_init(int thread_count, int max_queue_size, task_handler handler);
int threadpool_submit(int client_fd);
void threadpool_shutdown(void);
int threadpool_get_processed_count(void);

#endif
