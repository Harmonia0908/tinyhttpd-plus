#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>

#define MAX_REQUESTS 1000
#define MAX_THREADS 50

struct thread_data {
 int thread_id;
 int num_requests;
 char *host;
 int port;
 char *path;
 int *success_count;
 int *fail_count;
 double *total_time;
 pthread_mutex_t *mutex;
};

void *send_request(void *arg)
{
 struct thread_data *data = (struct thread_data *)arg;
 int sock;
 struct sockaddr_in server;
 char request[1024];
 char response[1024];
 int bytes_received;
 struct timeval start, end;
 double elapsed;

 for (int i = 0; i < data->num_requests; i++) {
  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
   pthread_mutex_lock(data->mutex);
   (*data->fail_count)++;
   pthread_mutex_unlock(data->mutex);
   continue;
  }

  server.sin_family = AF_INET;
  server.sin_port = htons(data->port);
  inet_pton(AF_INET, data->host, &server.sin_addr);

  gettimeofday(&start, NULL);
  if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
   gettimeofday(&end, NULL);
   elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
   
   pthread_mutex_lock(data->mutex);
   (*data->fail_count)++;
   (*data->total_time) += elapsed;
   pthread_mutex_unlock(data->mutex);
   
   close(sock);
   continue;
  }

  sprintf(request, "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", 
          data->path, data->host);
  
  if (send(sock, request, strlen(request), 0) < 0) {
   gettimeofday(&end, NULL);
   elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;
   
   pthread_mutex_lock(data->mutex);
   (*data->fail_count)++;
   (*data->total_time) += elapsed;
   pthread_mutex_unlock(data->mutex);
   
   close(sock);
   continue;
  }

  bytes_received = recv(sock, response, sizeof(response) - 1, 0);
  gettimeofday(&end, NULL);
  elapsed = (end.tv_sec - start.tv_sec) + (end.tv_usec - start.tv_usec) / 1000000.0;

  if (bytes_received > 0) {
   response[bytes_received] = '\0';
   if (strstr(response, "200 OK")) {
    pthread_mutex_lock(data->mutex);
    (*data->success_count)++;
    (*data->total_time) += elapsed;
    pthread_mutex_unlock(data->mutex);
   } else {
    pthread_mutex_lock(data->mutex);
    (*data->fail_count)++;
    (*data->total_time) += elapsed;
    pthread_mutex_unlock(data->mutex);
   }
  } else {
   pthread_mutex_lock(data->mutex);
   (*data->fail_count)++;
   (*data->total_time) += elapsed;
   pthread_mutex_unlock(data->mutex);
  }

  close(sock);
 }

 return NULL;
}

int main(int argc, char *argv[])
{
 if (argc < 6) {
  printf("Usage: %s <host> <port> <path> <num_threads> <requests_per_thread>\n", argv[0]);
  printf("Example: %s 127.0.0.1 80 / 10 100\n", argv[0]);
  return 1;
 }

 char *host = argv[1];
 int port = atoi(argv[2]);
 char *path = argv[3];
 int num_threads = atoi(argv[4]);
 int requests_per_thread = atoi(argv[5]);

 if (num_threads > MAX_THREADS) {
  printf("Error: Maximum threads is %d\n", MAX_THREADS);
  return 1;
 }

 int total_requests = num_threads * requests_per_thread;
 int success_count = 0;
 int fail_count = 0;
 double total_time = 0.0;
 pthread_mutex_t mutex;
 pthread_t threads[MAX_THREADS];
 struct thread_data thread_data_array[MAX_THREADS];

 pthread_mutex_init(&mutex, NULL);

 printf("Starting load test...\n");
 printf("Host: %s\n", host);
 printf("Port: %d\n", port);
 printf("Path: %s\n", path);
 printf("Threads: %d\n", num_threads);
 printf("Requests per thread: %d\n", requests_per_thread);
 printf("Total requests: %d\n\n", total_requests);

 struct timeval test_start, test_end;
 gettimeofday(&test_start, NULL);

 for (int i = 0; i < num_threads; i++) {
  thread_data_array[i].thread_id = i;
  thread_data_array[i].num_requests = requests_per_thread;
  thread_data_array[i].host = host;
  thread_data_array[i].port = port;
  thread_data_array[i].path = path;
  thread_data_array[i].success_count = &success_count;
  thread_data_array[i].fail_count = &fail_count;
  thread_data_array[i].total_time = &total_time;
  thread_data_array[i].mutex = &mutex;

  if (pthread_create(&threads[i], NULL, send_request, &thread_data_array[i]) != 0) {
   printf("Error creating thread %d\n", i);
   return 1;
  }
 }

 for (int i = 0; i < num_threads; i++) {
  pthread_join(threads[i], NULL);
 }

 gettimeofday(&test_end, NULL);
 double test_elapsed = (test_end.tv_sec - test_start.tv_sec) + 
                      (test_end.tv_usec - test_start.tv_usec) / 1000000.0;

 pthread_mutex_destroy(&mutex);

 printf("\nLoad test completed!\n");
 printf("========================================\n");
 printf("Total requests: %d\n", total_requests);
 printf("Successful: %d\n", success_count);
 printf("Failed: %d\n", fail_count);
 printf("Success rate: %.2f%%\n", (success_count * 100.0) / total_requests);
 printf("Total time: %.2f seconds\n", test_elapsed);
 printf("Requests per second: %.2f\n", total_requests / test_elapsed);
 printf("Average response time: %.4f seconds\n", total_time / total_requests);
 printf("========================================\n");

 return 0;
}
