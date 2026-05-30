#ifndef TINYHTTPD_CONFIG_H
#define TINYHTTPD_CONFIG_H

#include <sys/types.h>

#define CONFIG_PATH "config/server.conf"
#define MAX_ROOT_DIR_LEN 512

typedef struct {
 u_short port;
 int thread_num;
 char root_dir[MAX_ROOT_DIR_LEN];
 int enable_access_log;
 int enable_error_log;
} server_config_t;

/**
 * Initialize a server configuration with built-in defaults.
 */
void init_default_config(server_config_t *cfg);

/**
 * Load key=value settings from a configuration file.
 *
 * Missing files are not fatal. Unknown keys and invalid values print warnings
 * and leave the corresponding default value in place.
 */
int load_config(const char *path, server_config_t *cfg);

/**
 * Store the active server configuration for modules that need runtime options.
 */
void set_server_config(const server_config_t *cfg);

/**
 * Return the active server configuration.
 */
const server_config_t *get_server_config(void);

#endif
