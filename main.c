/* J. David's webserver */
/* This is a simple webserver.
 * Created November 1999 by J. David Blackstone.
 * CSE 4344 (Network concepts), Prof. Zeigler
 * University of Texas at Arlington
 */

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

#include "config.h"
#include "server.h"

static int parse_port(const char *value, uint16_t *port)
{
 char *end;
 long parsed;

 errno = 0;
 parsed = strtol(value, &end, 10);
 if (value == end || *end != '\0' || errno == ERANGE ||
     parsed <= 0 || parsed > 65535)
  return -1;
 *port = (uint16_t)parsed;
 return 0;
}

int main(int argc, char *argv[])
{
 server_config_t cfg;

 load_config(CONFIG_PATH, &cfg);

 // 解析命令行参数；保留命令行端口覆盖能力，便于测试和临时启动。
 if (argc > 1) {
  uint16_t port_arg;
  if (parse_port(argv[1], &port_arg) != 0) {
   cfg.port = 8080;
  } else {
   cfg.port = port_arg;
  }
 }

 set_server_config(&cfg);
 return server_run(cfg.port);
}
