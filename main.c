/* J. David's webserver */
/* This is a simple webserver.
 * Created November 1999 by J. David Blackstone.
 * CSE 4344 (Network concepts), Prof. Zeigler
 * University of Texas at Arlington
 */

#include <stdlib.h>
#include <sys/types.h>

#include "config.h"
#include "server.h"

int main(int argc, char *argv[])
{
 server_config_t cfg;

 load_config(CONFIG_PATH, &cfg);

 // 解析命令行参数；保留命令行端口覆盖能力，便于测试和临时启动。
 if (argc > 1) {
  int port_arg = atoi(argv[1]);
  if (port_arg <= 0 || port_arg > 65535) {
   cfg.port = 8080;
  } else {
   cfg.port = (u_short)port_arg;
  }
 }

 set_server_config(&cfg);
 return server_run(cfg.port);
}
