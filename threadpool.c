#include "threadpool.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* #define DEBUG */

#ifdef DEBUG
#define DEBUG_PRINT(fmt, ...) \
    fprintf(stderr, "[threadpool:pthread %p] " fmt "\n", (void *)pthread_self(), ##__VA_ARGS__)
#else
#define DEBUG_PRINT(fmt, ...) do {} while (0)
#endif

typedef struct threadpool_task {
    int *client_fd_ptr;
    struct threadpool_task *next;
} threadpool_task_t;

typedef struct {
    threadpool_task_t *head;
    threadpool_task_t *tail;
    int task_count;
    int max_queue_size;
    int shutdown;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    pthread_cond_t full_cond;
    pthread_t *threads;
    int thread_count;
    task_handler handler;
} threadpool_t;

static threadpool_t pool;
static int processed_count = 0;
static const int REPORT_INTERVAL = 100;

static threadpool_task_t *task_create(int client_fd) {
    threadpool_task_t *t = malloc(sizeof(threadpool_task_t));
    if (t == NULL) {
        return NULL;
    }
    int *fd_ptr = malloc(sizeof(int));
    if (fd_ptr == NULL) {
        free(t);
        return NULL;
    }
    *fd_ptr = client_fd;
    t->client_fd_ptr = fd_ptr;
    t->next = NULL;
    return t;
}

static void task_destroy(threadpool_task_t *t) {
    if (t != NULL) {
        free(t->client_fd_ptr);
        free(t);
    }
}

static void *worker(void *arg) {
    (void)arg;
    while (1) {
        pthread_mutex_lock(&pool.mutex);

        while (pool.task_count == 0) {
            if (pool.shutdown) {
                DEBUG_PRINT("exiting (shutdown)");
                pthread_mutex_unlock(&pool.mutex);
                pthread_exit(NULL);
            }
            pthread_cond_wait(&pool.cond, &pool.mutex);
        }

        threadpool_task_t *t = pool.head;
        pool.head = t->next;
        pool.task_count--;

        if (pool.head == NULL) {
            pool.tail = NULL;
        }

        if (pool.task_count == pool.max_queue_size - 1) {
            pthread_cond_signal(&pool.full_cond);
        }

        int client_fd = *t->client_fd_ptr;
        free(t->client_fd_ptr);
        free(t);

        pthread_mutex_unlock(&pool.mutex);

        DEBUG_PRINT("handling client_fd=%d", client_fd);
        pool.handler(client_fd);

        int cnt = __sync_fetch_and_add(&processed_count, 1) + 1;
        if (cnt % REPORT_INTERVAL == 0) {
            fprintf(stderr, "[threadpool] processed %d requests\n", cnt);
        }
    }
    return NULL;
}

void threadpool_init(int thread_count, int max_queue_size, task_handler handler) {
    if (thread_count <= 0 || thread_count > 100) {
        fprintf(stderr, "Invalid thread count: %d\n", thread_count);
        return;
    }
    if (max_queue_size <= 0) {
        fprintf(stderr, "Invalid queue size: %d\n", max_queue_size);
        return;
    }

    pool.handler = handler;
    pool.thread_count = thread_count;
    pool.head = NULL;
    pool.tail = NULL;
    pool.task_count = 0;
    pool.max_queue_size = max_queue_size;
    pool.shutdown = 0;

    pthread_mutex_init(&pool.mutex, NULL);
    pthread_cond_init(&pool.cond, NULL);
    pthread_cond_init(&pool.full_cond, NULL);

    pool.threads = malloc(sizeof(pthread_t) * thread_count);
    if (pool.threads == NULL) {
        pool.shutdown = 1;
        pool.thread_count = 0;
        return;
    }

    for (int i = 0; i < thread_count; i++) {
        if (pthread_create(&pool.threads[i], NULL, worker, NULL) != 0) {
            pool.shutdown = 1;
            pthread_cond_broadcast(&pool.cond);
            for (int j = 0; j < i; j++) {
                pthread_join(pool.threads[j], NULL);
            }
            free(pool.threads);
            pool.thread_count = 0;
            pool.threads = NULL;
            return;
        }
    }
}

int threadpool_submit(int client_fd) {
    if (pool.shutdown) {
        return -1;
    }

    pthread_mutex_lock(&pool.mutex);

    if (pool.task_count >= pool.max_queue_size) {
        pthread_mutex_unlock(&pool.mutex);
        fprintf(stderr, "[threadpool] queue full, dropped connection (fd=%d, queue_size=%d)\n",
                client_fd, pool.max_queue_size);
        return -1;
    }

    threadpool_task_t *t = task_create(client_fd);
    if (t == NULL) {
        pthread_mutex_unlock(&pool.mutex);
        fprintf(stderr, "[threadpool] memory allocation failed, dropped connection (fd=%d)\n",
                client_fd);
        return -1;
    }

    if (pool.tail == NULL) {
        pool.head = t;
        pool.tail = t;
    } else {
        pool.tail->next = t;
        pool.tail = t;
    }
    pool.task_count++;

    pthread_cond_signal(&pool.cond);
    pthread_mutex_unlock(&pool.mutex);
    return 0;
}

int threadpool_get_processed_count(void) {
    return __sync_fetch_and_add(&processed_count, 0);
}

void threadpool_shutdown(void) {
    pthread_mutex_lock(&pool.mutex);
    pool.shutdown = 1;
    pthread_cond_broadcast(&pool.cond);
    pthread_mutex_unlock(&pool.mutex);

    for (int i = 0; i < pool.thread_count; i++) {
        pthread_join(pool.threads[i], NULL);
    }

    while (pool.head != NULL) {
        threadpool_task_t *t = pool.head;
        pool.head = t->next;
        task_destroy(t);
    }

    free(pool.threads);
    pthread_cond_destroy(&pool.cond);
    pthread_cond_destroy(&pool.full_cond);
    pthread_mutex_destroy(&pool.mutex);
}
