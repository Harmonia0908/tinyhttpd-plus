#include "config.h"
#include "threadpool.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CONFIG_LINE_SIZE 1024
#define DEFAULT_PORT 8080
#define DEFAULT_THREAD_NUM 4
#define DEFAULT_ROOT_DIR "./htdocs"
#define DEFAULT_ENABLE_ACCESS_LOG 1
#define DEFAULT_ENABLE_ERROR_LOG 1

static server_config_t active_config;
static int active_config_initialized = 0;

static char *trim(char *s)
{
 char *end;

 while (isspace((unsigned char)*s))
  s++;

 if (*s == '\0')
  return s;

 end = s + strlen(s) - 1;
 while (end > s && isspace((unsigned char)*end))
  end--;
 end[1] = '\0';

 return s;
}

static int parse_long(const char *value, long min_value, long max_value,
                      long *result)
{
 char *end;
 long parsed;

 errno = 0;
 parsed = strtol(value, &end, 10);
 if (value == end || errno == ERANGE)
  return -1;

 while (isspace((unsigned char)*end))
  end++;
 if (*end != '\0')
  return -1;

 if (parsed < min_value || parsed > max_value)
  return -1;

 *result = parsed;
 return 0;
}

static void discard_line_remainder(FILE *fp)
{
 int ch;

 do {
  ch = fgetc(fp);
 } while (ch != '\n' && ch != EOF);
}

void init_default_config(server_config_t *cfg)
{
 if (cfg == NULL)
  return;

 cfg->port = DEFAULT_PORT;
 cfg->thread_num = DEFAULT_THREAD_NUM;
 snprintf(cfg->root_dir, sizeof(cfg->root_dir), "%s", DEFAULT_ROOT_DIR);
 cfg->enable_access_log = DEFAULT_ENABLE_ACCESS_LOG;
 cfg->enable_error_log = DEFAULT_ENABLE_ERROR_LOG;
}

int load_config(const char *path, server_config_t *cfg)
{
 FILE *fp;
 char line[CONFIG_LINE_SIZE];
 int line_no = 0;

 if (cfg == NULL)
  return -1;

 init_default_config(cfg);

 fp = fopen(path, "r");
 if (fp == NULL)
 {
  if (errno != ENOENT)
   fprintf(stderr, "warning: cannot open config file %s: %s\n",
           path, strerror(errno));
  return 0;
 }

 while (fgets(line, sizeof(line), fp) != NULL)
 {
  char *key;
  char *value;
  char *eq;
  size_t len;
  long parsed;

  line_no++;
  len = strlen(line);
  if (len > 0 && line[len - 1] != '\n' && !feof(fp))
  {
   fprintf(stderr, "warning: config line %d is too long and was ignored\n",
           line_no);
   discard_line_remainder(fp);
   continue;
  }

  key = trim(line);
  if (*key == '\0' || *key == '#')
   continue;

  eq = strchr(key, '=');
  if (eq == NULL)
  {
   fprintf(stderr, "warning: invalid config line %d: missing '='\n",
           line_no);
   continue;
  }

  *eq = '\0';
  value = trim(eq + 1);
  key = trim(key);

  if (strcmp(key, "port") == 0)
  {
   if (parse_long(value, 1, 65535, &parsed) == 0)
    cfg->port = (u_short)parsed;
   else
   {
    cfg->port = DEFAULT_PORT;
    fprintf(stderr, "warning: invalid port on line %d, using default\n",
            line_no);
   }
  }
  else if (strcmp(key, "thread_num") == 0)
  {
   if (parse_long(value, 1, THREADPOOL_MAX_THREADS, &parsed) == 0)
    cfg->thread_num = (int)parsed;
   else
   {
    cfg->thread_num = DEFAULT_THREAD_NUM;
    fprintf(stderr, "warning: invalid thread_num on line %d, using default\n",
            line_no);
   }
  }
  else if (strcmp(key, "root_dir") == 0)
  {
   if (*value != '\0' && strlen(value) < sizeof(cfg->root_dir))
    snprintf(cfg->root_dir, sizeof(cfg->root_dir), "%s", value);
   else
   {
    snprintf(cfg->root_dir, sizeof(cfg->root_dir), "%s", DEFAULT_ROOT_DIR);
    fprintf(stderr, "warning: invalid root_dir on line %d, using default\n",
            line_no);
   }
  }
  else if (strcmp(key, "enable_access_log") == 0)
  {
   if (parse_long(value, 0, 1, &parsed) == 0)
    cfg->enable_access_log = (int)parsed;
   else
   {
    cfg->enable_access_log = DEFAULT_ENABLE_ACCESS_LOG;
    fprintf(stderr,
            "warning: invalid enable_access_log on line %d, using default\n",
            line_no);
   }
  }
  else if (strcmp(key, "enable_error_log") == 0)
  {
   if (parse_long(value, 0, 1, &parsed) == 0)
    cfg->enable_error_log = (int)parsed;
   else
   {
    cfg->enable_error_log = DEFAULT_ENABLE_ERROR_LOG;
    fprintf(stderr,
            "warning: invalid enable_error_log on line %d, using default\n",
            line_no);
   }
  }
  else
  {
   fprintf(stderr, "warning: unknown config key '%s' on line %d\n",
           key, line_no);
  }
 }

 fclose(fp);
 return 0;
}

void set_server_config(const server_config_t *cfg)
{
 if (cfg == NULL)
  init_default_config(&active_config);
 else
  active_config = *cfg;
 active_config_initialized = 1;
}

const server_config_t *get_server_config(void)
{
 if (!active_config_initialized)
 {
  init_default_config(&active_config);
  active_config_initialized = 1;
 }

 return &active_config;
}
