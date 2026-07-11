#include "log.h"

#include "config.h"
#include "utils.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define LOG_DIR "logs"
#define ACCESS_LOG_PATH "logs/access.log"
#define ERROR_LOG_PATH "logs/error.log"

static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

static FILE *open_log_file(const char *path)
{
 int fd = open_cloexec(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
 FILE *file;

 if (fd == -1)
  return NULL;
 file = fdopen(fd, "a");
 if (file == NULL)
  close(fd);
 return file;
}

/*
 * Ensure the log directory exists before opening log files.
 */
static int ensure_log_dir(void)
{
 if (mkdir(LOG_DIR, 0755) == -1 && errno != EEXIST)
  return -1;

 return 0;
}

/*
 * Format the current local time for log entries.
 */
static void format_timestamp(char *buf, size_t buf_size)
{
 time_t now;
 struct tm tm_now;

 time(&now);
 if (localtime_r(&now, &tm_now) == NULL)
 {
  snprintf(buf, buf_size, "0000-00-00 00:00:00");
  return;
 }

 strftime(buf, buf_size, "%Y-%m-%d %H:%M:%S", &tm_now);
}

void log_access(const char *client_ip, const char *method,
                const char *url, int status_code)
{
 FILE *fp;
 char time_str[64];
 const server_config_t *cfg = get_server_config();

 if (!cfg->enable_access_log)
  return;

 pthread_mutex_lock(&log_mutex);

 if (ensure_log_dir() == 0)
 {
  fp = open_log_file(ACCESS_LOG_PATH);
  if (fp != NULL)
  {
   format_timestamp(time_str, sizeof(time_str));
   fprintf(fp, "[%s] %s \"%s %s\" %d\n",
           time_str,
           client_ip != NULL ? client_ip : "-",
           method != NULL ? method : "-",
           url != NULL ? url : "-",
           status_code);
   fclose(fp);
  }
 }

 pthread_mutex_unlock(&log_mutex);
}

void log_error_message(const char *fmt, ...)
{
 FILE *fp;
 char time_str[64];
 va_list args;
 const server_config_t *cfg = get_server_config();

 if (!cfg->enable_error_log)
  return;

 pthread_mutex_lock(&log_mutex);

 if (ensure_log_dir() == 0)
 {
  fp = open_log_file(ERROR_LOG_PATH);
  if (fp != NULL)
  {
   format_timestamp(time_str, sizeof(time_str));
   fprintf(fp, "[%s] ERROR: ", time_str);
   va_start(args, fmt);
   vfprintf(fp, fmt, args);
   va_end(args);
   fprintf(fp, "\n");
   fclose(fp);
  }
 }

 pthread_mutex_unlock(&log_mutex);
}
