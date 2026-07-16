# Tinyhttpd 14 天学习计划

> [!WARNING]
> 这是目录重组前的历史学习计划，其中根目录源码路径和行号冻结在旧布局，不代表当前
> 工作区。阅读当前代码请先使用 [architecture.md](architecture.md)、
> [code-walkthrough.md](code-walkthrough.md) 和 [module-guide.md](module-guide.md)；
> 本文只保留原学习顺序，不再作为文件定位依据。

## 使用说明

- 总周期 14 天，每天 1.5～2.5 小时；建议默认按 2 小时安排。
- 顺序严格对应当前仓库：先建立地图，再跟主链，随后研究资源/错误，最后进入网络安全、测试和面试表达。
- 所有“结论笔记”都要附 `文件:函数`。无法从代码或测试确认的判断，写成“推断：……”，并记录验证办法。
- 运行 TCP 测试前确认端口空闲；完整 shell 测试需要 Bash、`curl`、`nc` 和 Perl。
- 每天保留一页学习记录：调用链、资源所有权、当天自测答案、仍未解决的问题。

## 第 1 天：建立项目地图（约 2 小时）

### 当天目标

知道项目解决什么问题、明确不解决什么问题，能把入口、server、请求、响应、静态文件、CGI、线程池和测试放到一张图里。理解 `.codegraph/` 和 `graphify-out/` 是导航证据，不是源码真相。

### 时间安排

- 25 分钟：读 README 的概览、功能、项目结构和 Limitations。
- 35 分钟：浏览仓库文件和 Makefile 目标。
- 35 分钟：读图谱报告并查询结构化索引。
- 20 分钟：手画模块图。
- 5 分钟：口头复述。

### 需要阅读的具体文件

- `README.md`：第 1～16 行、137～170 行、260～371 行。
- `Makefile`：全部。
- `graphify-out/GRAPH_REPORT.md`：Summary、God Nodes、Communities、Suggested Questions。
- `.codegraph/daemon.log`、`.codegraph/.gitignore`。
- `.github/workflows/ci.yml`：先只看 job 名和命令。

### 需要理解的具体函数/入口

- 暂不钻实现，只定位：`main()`、`server_run()`、`accept_request()`、`serve_file()`、`execute_cgi()`、`threadpool_init()`。
- 能说出这些函数分别位于哪个文件、上下游是谁。

### 需要手动运行的命令

```bash
git ls-files
make -n all
rg -n '^(int|void|const char|pid_t|static).*\(' *.c
sed -n '1,160p' graphify-out/GRAPH_REPORT.md
sqlite3 'file:.codegraph/codegraph.db?mode=ro&immutable=1' \
  'SELECT language, COUNT(*), SUM(node_count) FROM files GROUP BY language;'
graphify query '从 main 到静态文件和 CGI 的主调用链是什么？' --budget 2000
```

### 需要回答的自测问题

1. `httpd`、`client`、`benchmark` 三个二进制分别由什么源码构建？
2. 哪个函数是请求处理枢纽，哪个函数是 server 生命周期枢纽？
3. 为什么图谱中的 `INFERRED` 边不能直接当成事实？
4. 项目明确不支持的生产能力至少有哪些四项？
5. C 源码模块、测试脚本、默认网页/CGI 分别放在哪里？

### 当天完成标准

- 不看仓库能画出 8 个核心模块和箭头。
- 能用 90 秒说清项目定位与主要限制。
- 每条图谱结论都能指出下一步应回查的源码文件。

## 第 2 天：构建、运行和观察协议（约 2 小时）

### 当天目标

掌握构建变量、产物和运行依赖；亲手观察 GET、HEAD、CGI POST、OPTIONS 的真实响应，以及 Ctrl-C 关停行为。

### 时间安排

- 30 分钟：精读 Makefile。
- 25 分钟：clean build，检查产物。
- 45 分钟：双终端运行和抓取响应。
- 15 分钟：运行小型 benchmark。
- 5 分钟：整理观察结果。

### 需要阅读的具体文件

- `Makefile`。
- `README.md:170-220`。
- `TESTING.md:5-27,75-101`。
- `BENCHMARK.md:7-25,133-140`。
- `htdocs/index.html`、`htdocs/date.cgi`。

### 需要理解的具体函数

- `main()`：程序返回值从哪里来。
- `benchmark.c:main()`、`send_request()`：压测参数和“成功”定义。
- `simpleclient.c:main()`：为什么它不是本 HTTP server 的客户端验证工具。

### 需要手动运行的命令

终端 A：

```bash
make clean
make
./httpd 18082
```

终端 B：

```bash
file httpd client benchmark
curl --noproxy '*' -i http://127.0.0.1:18082/
curl --noproxy '*' -I http://127.0.0.1:18082/
curl --noproxy '*' -i http://127.0.0.1:18082/date.cgi
curl --noproxy '*' -i -X POST -d 'a=1' http://127.0.0.1:18082/date.cgi
curl --noproxy '*' -i -X OPTIONS http://127.0.0.1:18082/
./benchmark 127.0.0.1 18082 / 2 10
```

最后回到终端 A 按 Ctrl-C。

### 需要回答的自测问题

1. 默认编译标准和 warning 选项是什么？
2. 为什么静态 HEAD 仍有 `Content-Length`，但不应有 body？
3. OPTIONS 返回了哪些 header？
4. `benchmark` 为什么不能给出严谨的 p95/p99 或完整成功率？
5. Ctrl-C 后端口为什么应该释放？

### 当天完成标准

- clean build 无 warning/error。
- 五类手动请求符合 README 描述。
- 能解释三个构建产物的用途和 benchmark 的测量边界。

## 第 3 天：从 `main()` 到 accept 循环（约 2～2.5 小时）

### 当天目标

完整追踪启动配置、listener 创建、线程池初始化、poll/accept 和信号退出主链。

### 时间安排

- 35 分钟：`main.c` + `config.c/.h`。
- 50 分钟：`server.c/.h`。
- 25 分钟：画启动时序和失败分支。
- 20 分钟：运行配置回归。
- 10 分钟：自测。

### 需要阅读的具体文件

- `main.c`。
- `config.c`、`config.h`。
- `server.c`、`server.h`。
- `threadpool.h`。
- `tests/config_test.sh`。

### 需要理解的具体函数

- `parse_port()`、`main()`。
- `trim()`、`parse_long()`、`discard_line_remainder()`。
- `init_default_config()`、`load_config()`、`set_server_config()`、`get_server_config()`。
- `shutdown_handler()`、`startup()`、`server_run()`、`handle_client()`。

### 需要手动运行的命令

```bash
rg -n 'DEFAULT_|CONFIG_|THREADPOOL_MAX_THREADS|DEFAULT_QUEUE_SIZE' \
  main.c config.c config.h server.c threadpool.h
rg -n 'socket|setsockopt|bind|listen|poll|accept|sigaction|close' server.c
make
SKIP_BUILD=1 tests/config_test.sh
```

### 需要回答的自测问题

1. 配置文件不存在与不可读分别怎样处理？
2. 非法命令行端口为什么会回到 8080，而不是保留配置端口？
3. listener 为什么是非阻塞，而 accepted client 又被改成阻塞？
4. `poll(..., 250)` 与可靠观察停止标志有什么关系？
5. 线程池初始化失败时 listener 由谁关闭？
6. 哪些 startup 错误会直接 `exit(1)`？

### 当天完成标准

- 能从 `main()` 不停顿讲到第一次 `threadpool_submit()`。
- 能画出正常启动、线程池初始化失败、SIGTERM 三条路径。
- 配置测试全部通过并能解释每个 case 修改了什么。

## 第 4 天：请求行、Header 和方法分派（约 2～2.5 小时）

### 当天目标

掌握不受信任字节如何变成 method、URL、`Content-Length` 和状态码，理解 GET/HEAD/POST/OPTIONS 的分叉条件。

### 时间安排

- 55 分钟：精读 `request.c`。
- 25 分钟：精读 `get_line()`。
- 25 分钟：读响应错误函数。
- 20 分钟：跑解析测试。
- 15 分钟：画状态机和边界表。

### 需要阅读的具体文件

- `request.c`、`request.h`。
- `utils.c:196-249`、`utils.h:9-17`。
- `response.c`、`response.h`。
- `tests/request_test.c`。

### 需要理解的具体函数

- `accept_request()`。
- `parse_request_line()`、`read_headers()`。
- `get_line()`。
- `bad_request()`、`send_413()`、`uri_too_long()`、`unimplemented()`、`send_error_page()`。

### 需要手动运行的命令

```bash
make unit-test
build/tests/request_test
rg -n 'MAX_REQUEST_SIZE|MAX_HEADER_SIZE|MAX_HEADER_LINE_SIZE|414|413|501' \
  request.c response.c utils.h tests/request_test.c
```

可选的真实 socket 观察，先在另一终端运行 `./httpd 18082`：

```bash
printf 'PUT / HTTP/1.0\r\n\r\n' | nc -w 3 127.0.0.1 18082
printf 'GET\r\n' | nc -w 3 127.0.0.1 18082
printf 'POST /date.cgi HTTP/1.0\r\nContent-Length: 3\r\nContent-Length: 4\r\n\r\nabc' \
  | nc -w 3 127.0.0.1 18082
```

### 需要回答的自测问题

1. 请求行、URL、单 header、累计 headers、body 的上限分别是多少？
2. 为什么重复 `Content-Length` 必须拒绝？
3. `get_line()` 如何处理 CR、LF、CRLF、EINTR 和读超时？
4. URL 带 `?` 时为什么会强制 CGI？
5. POST 为什么一定进入 CGI？POST 查询串当前有什么局限？
6. 哪些 `-1` 错误不会生成 access log？

### 当天完成标准

- 能根据 8 个畸形请求预测 400/413/414/501 或直接关闭。
- 能从 `accept_request()` 画出四种方法的分支图。
- `request_test` 通过，并能说出它尚未覆盖的解析边界。

## 第 5 天：静态响应、CGI 总览、MIME 与日志（约 2～2.5 小时）

### 当天目标

完成主请求路径：理解静态文件如何发送、CGI 如何被选中、响应状态如何回到日志链。

### 时间安排

- 35 分钟：静态文件 + response。
- 30 分钟：CGI 主流程概览，不深挖 fd 细节。
- 25 分钟：MIME + 日志。
- 30 分钟：MIME/日志测试。
- 10 分钟：对比静态与 CGI。

### 需要阅读的具体文件

- `static_file.c/.h`。
- `response.c/.h`。
- `mime.c/.h`。
- `log.c/.h`。
- `cgi.c:147-169,219-255,296-412`。
- `htdocs/date.cgi`、`htdocs/check.cgi`、`htdocs/color.cgi`。
- `tests/mime_test.sh`、`tests/log_test.sh`。

### 需要理解的具体函数

- `handle_static_file()`、`serve_file()`、`cat()`。
- `headers()`、`send_error_page()`。
- `get_mime_type()`。
- `open_log_file()`、`ensure_log_dir()`、`log_access()`、`log_error_message()`。
- `handle_cgi()`、`execute_cgi()` 的父子分支轮廓。

### 需要手动运行的命令

```bash
make
SKIP_BUILD=1 PORT=18086 tests/mime_test.sh
SKIP_BUILD=1 PORT=18087 tests/log_test.sh
rg -n 'fread|send_all|Content-Length|Content-Type|fclose|close\(' \
  static_file.c response.c log.c cgi.c
```

### 需要回答的自测问题

1. 为什么 `cat()` 能正确发送包含 NUL 的文件？
2. 静态文件打开失败时 403 与 404 如何区分？
3. CGI 的选择依据为什么是执行位而不是扩展名？
4. MIME 未知扩展名返回什么？
5. handler 已关闭 client 后，`accept_request()` 为什么还能写日志？
6. 日志 mutex 解决什么问题，又引入什么瓶颈？

### 当天完成标准

- 能分别讲清静态 GET、静态 HEAD、CGI GET 三条链。
- MIME 与日志 suite 全通过。
- 能指出“HTTP 响应已经发送”和“access log 最终状态”可能不一致的 CGI 场景。

## 第 6 天：线程池数据结构与生命周期（约 2 小时）

### 当天目标

掌握 FIFO、worker 条件等待、任务节点所有权、init 失败回滚和 drain shutdown。

### 时间安排

- 55 分钟：逐行读 `threadpool.c`。
- 25 分钟：读接口契约文档。
- 20 分钟：读线程池测试。
- 15 分钟：画状态/所有权图。
- 5 分钟：自测。

### 需要阅读的具体文件

- `threadpool.c`、`threadpool.h`。
- `THREADPOOL_INTEGRATION.md`。
- `tests/threadpool_test.c`。
- `server.c:75-155`。

### 需要理解的具体函数和结构

- `threadpool_task_t`、`threadpool_t`。
- `task_create()`、`worker()`、`reset_pool_state()`。
- `threadpool_init()`、`threadpool_submit()`、`threadpool_shutdown()`。
- `threadpool_get_processed_count()`。

### 需要手动运行的命令

```bash
make unit-test
build/tests/threadpool_test
rg -n 'lifecycle_mutex|pool\.mutex|task_available|task_count|shutdown|processed_count' \
  threadpool.c
```

### 需要回答的自测问题

1. 为什么需要 `lifecycle_mutex` 和 `pool.mutex` 两把锁？
2. worker 为什么要用 `while` 而不是 `if` 等待条件变量？
3. shutdown 时队列非空，worker 会怎样？
4. submit 失败时 client fd 归谁？
5. worker 创建到一半失败时哪些资源必须回滚？
6. relaxed 原子计数为什么不影响调度正确性？

### 当天完成标准

- 能写出队列的三个不变量：head/tail、count、shutdown 访问规则。
- 能从任一 init 失败点倒推完整清理顺序。
- 能解释重复 shutdown 为什么是安全 no-op。

## 第 7 天：fd、管道、子进程与 CGI 资源生命周期（约 2.5 小时）

### 当天目标

把 CGI 的两对 pipe、父子 fd、环境数组、超时、waitpid 和异常清理全部画清楚；理解多线程 fork 的 close-on-exec 竞态。

### 时间安排

- 65 分钟：精读 `cgi.c`。
- 40 分钟：精读 `utils.c` 的 fd helpers。
- 25 分钟：读并运行并发 fd 测试。
- 15 分钟：画父子 fd 表。
- 5 分钟：自测。

### 需要阅读的具体文件

- `cgi.c`、`cgi.h`。
- `utils.c:17-188`、`utils.h`。
- `server.c:26-47,131-149`。
- `tests/cgi_test.c`、`tests/cgi_fd_probe.c`。

### 需要理解的具体函数

- `is_cgi_environment_entry()`、`build_cgi_environment()`。
- `read_retry()`、`recv_retry()`、`write_all_fd()`、`wait_for_child()`、`terminate_cgi_child()`。
- `handle_cgi()`、`execute_cgi()`。
- `accept_cloexec_blocking()`、`fork_with_cloexec_pipes()`、`open_cloexec()`。
- `set_cloexec()`、`set_blocking()`、`set_nonblocking()`。

### 需要手动运行的命令

```bash
make unit-test
build/tests/cgi_test "$(pwd)/build/tests/cgi_fd_probe"
rg -n 'pipe|fork|dup2|execve|close|alarm|waitpid|kill|FD_CLOEXEC' cgi.c utils.c
```

### 需要回答的自测问题

1. `cgi_output`、`cgi_input` 四个端点在父子进程中分别由谁关闭？
2. 为什么 CGI 环境在 fork 前构造？
3. child 为什么使用 `_exit()` 而不是 `exit()`？
4. 为什么仅设置 `FD_CLOEXEC` 还不够，仍需要 `fork_fd_mutex`？
5. 截断 POST body 怎样保证没有僵尸子进程？
6. 16 路并发测试究竟证明了什么，没有证明什么？

### 当天完成标准

- 不看代码能画出两条 pipe 的方向和所有 close 点。
- 能逐步解释 exec 前 child 的操作顺序。
- CGI fd 回归通过，并能指出测试只观察 fd 3–255。

## 第 8 天：错误路径和清理审计（约 2～2.5 小时）

### 当天目标

用“失败点—响应—状态码—日志—close/free/wait—进程是否继续”的统一框架审计所有主要错误路径。

### 时间安排

- 35 分钟：启动/线程池错误。
- 35 分钟：解析/路径/静态错误。
- 40 分钟：CGI 错误。
- 20 分钟：运行端到端故障 case。
- 10 分钟：完成清理矩阵。

### 需要阅读的具体文件

- `server.c`、`threadpool.c`。
- `request.c`、`response.c`、`static_file.c`。
- `cgi.c`、`utils.c`。
- `tests/run_integration_tests.sh:322-402`。
- `tests/log_test.sh:107-131`。

### 需要理解的具体函数

- `error_die()`、`send_error_page()`、`log_request_error()`。
- `resolve_safe_path()`、`serve_file()`。
- `terminate_cgi_child()`、`wait_for_child()`、`execute_cgi()` 的每个 early return。
- `threadpool_init()` 的 `fail` 和 worker 创建失败分支。

### 需要手动运行的命令

```bash
make
SKIP_BUILD=1 PORT=18081 tests/run_integration_tests.sh
SKIP_BUILD=1 PORT=18082 tests/log_test.sh
rg -n 'return (400|403|404|413|414|500|501|-1)|goto fail|error_die|close\(|free\(|wait' \
  server.c threadpool.c request.c static_file.c cgi.c utils.c
```

### 需要回答的自测问题

1. 哪些错误导致整个进程退出，哪些只结束单个请求？
2. `read_headers()` 返回 -1 时客户端和日志发生什么？
3. CGI 已经发送部分响应后非 0 退出，客户端状态与 access log 各是什么？
4. 哪些上层函数忽略 `send_all()` 失败？
5. `poll()` 非 EINTR 失败为什么最终返回 0？这是怎样的语义选择？
6. shutdown 残余任务释放循环为什么正常情况下应为空？

### 当天完成标准

- 完成至少 15 行错误/清理矩阵。
- 对任何 early return 都能回答 client、pipe、child、heap 是否已清理。
- 集成和日志测试通过。

## 第 9 天：网络系统调用、超时与关停（约 2 小时）

### 当天目标

从操作系统视角理解 listener/client 的阻塞属性、`poll` readiness、backlog、socket 读超时、SIGPIPE 和信号驱动关停。

### 时间安排

- 45 分钟：server/socket 系统调用。
- 30 分钟：`get_line()`、`send_all()` 与超时。
- 25 分钟：真实端口和连接观察。
- 15 分钟：画 socket 状态变化。
- 5 分钟：自测。

### 需要阅读的具体文件

- `server.c`。
- `utils.c:29-46,114-188,196-249`。
- `request.c:172-323`。
- `tests/run_integration_tests.sh:153-179,369-401`。
- `benchmark.c:66-165`。

### 需要理解的具体函数

- `startup()`、`server_run()`、`shutdown_handler()`、`handle_client()`。
- `accept_cloexec_blocking()`、`set_nonblocking()`、`set_blocking()`。
- `get_line()`、`send_all()`。

### 需要手动运行的命令

终端 A：

```bash
make
./httpd 18082
```

终端 B：

```bash
lsof -nP -iTCP:18082 -sTCP:LISTEN
curl --noproxy '*' -v http://127.0.0.1:18082/ -o /dev/null
printf 'GET / HTTP/1.0\r\n' | nc -w 7 127.0.0.1 18082
```

结束时向终端 A 发送 Ctrl-C，再运行：

```bash
lsof -nP -iTCP:18082 -sTCP:LISTEN
```

### 需要回答的自测问题

1. backlog 5 与应用队列 1000 有什么区别？
2. 为什么 `poll()` 可读后 `accept()` 仍需处理 EAGAIN/EWOULDBLOCK？
3. 读超时在哪里设置，POST body 为什么变成 1 秒？
4. 当前为什么仍可能在发送时长时间占用 worker？
5. server 和 CGI child 对 SIGPIPE 的策略为什么不同？
6. listener 绑定 `INADDR_ANY` 对暴露面意味着什么？

### 当天完成标准

- 能逐个解释 socket/bind/listen/poll/accept/setsockopt 的参数和失败语义。
- 实际观察 server 监听和 Ctrl-C 后端口释放。
- 能区分读取超时、CGI alarm、测试 curl 超时三种机制。

## 第 10 天：并发、安全和边界处理（约 2.5 小时）

### 当天目标

把线程池、日志锁、fork/fd 锁、路径约束、输入限制和 CGI 信任边界组合成一份安全评审。

### 时间安排

- 40 分钟：并发不变量复盘。
- 45 分钟：路径与 CGI 信任边界。
- 25 分钟：压力/并发观察。
- 25 分钟：写风险优先级。
- 15 分钟：自测。

### 需要阅读的具体文件

- `threadpool.c`。
- `utils.c:17-112,251-373`。
- `cgi.c`。
- `log.c`。
- `request.c`。
- `README.md:320-369`。
- `tests/run_integration_tests.sh:322-367`。

### 需要理解的具体函数

- `worker()`、`threadpool_submit()`、`threadpool_shutdown()`。
- `resolve_safe_path()`、`is_path_traversal()`。
- `fork_with_cloexec_pipes()`、`build_cgi_environment()`、`execute_cgi()`。
- `log_access()`、`log_error_message()`。

### 需要手动运行的命令

终端 A：

```bash
make
./httpd 18082
```

终端 B：

```bash
./benchmark 127.0.0.1 18082 / 10 50
./benchmark 127.0.0.1 18082 /date.cgi 4 5
curl --path-as-is --noproxy '*' -i 'http://127.0.0.1:18082/../../etc/passwd'
```

再单独运行完整安全/超时回归：

```bash
PORT=18081 tests/run_integration_tests.sh
```

### 需要回答的自测问题

1. 路径防护为什么既做 `..` 组件检查又做 final `realpath()` 前缀检查？
2. 为什么仍存在 TOCTOU？`openat()` 能怎样改进？
3. CGI 继承哪些父环境，为什么可能暴露秘密？
4. POST CGI 为什么可能发生双管道互等？
5. 队列满时为什么不算“优雅降载”？
6. 哪些推断性风险当前测试没有证实，需要怎样复现？

### 当天完成标准

- 产出按高/中/低分级的风险清单，每项附代码位置和测试现状。
- 能解释四把 mutex 各自保护什么。
- 能提出 CGI、路径和发送路径各一个具体改造方案。

## 第 11 天：C 单元测试与可测试性（约 1.5～2 小时）

### 当天目标

理解为什么纯解析、线程池和 CGI fd 继承能不监听 TCP 端口就测试，以及测试如何验证资源生命周期。

### 时间安排

- 30 分钟：请求测试。
- 25 分钟：线程池测试。
- 30 分钟：CGI 并发 fd 测试。
- 20 分钟：读 Makefile 测试链接关系并运行。
- 5 分钟：列测试缺口。

### 需要阅读的具体文件

- `tests/request_test.c`。
- `tests/threadpool_test.c`。
- `tests/cgi_test.c`、`tests/cgi_fd_probe.c`。
- `Makefile:11-61`。
- 对照 `request.c`、`threadpool.c`、`cgi.c` 相应函数。

### 需要理解的具体函数

- 测试 helpers：`open_request_socket()`、`write_request()`、`close_request_socket()`。
- 四个 request test case。
- `test_invalid_initialization()`、`test_processes_all_accepted_tasks()`。
- `run_probe()` 和 fd probe 的 `main()`。

### 需要手动运行的命令

```bash
make clean
make unit-test
build/tests/request_test
build/tests/threadpool_test
build/tests/cgi_test "$(pwd)/build/tests/cgi_fd_probe"
```

### 需要回答的自测问题

1. `socketpair()` 相比真实 TCP 测试的优势和缺失是什么？
2. 为什么 request test 要对写端 `shutdown(SHUT_WR)`？
3. threadpool test 如何证明 shutdown 排空而不是丢任务？
4. CGI test 为什么用条件变量让 16 个线程同时开始？
5. 哪些分配失败或系统调用失败仍缺单元测试？

### 当天完成标准

- 所有 C 测试独立通过。
- 能为一个尚未覆盖的边界设计输入、断言和清理方式。
- 能解释各测试二进制在 Makefile 中为什么链接那些生产模块。

## 第 12 天：集成测试、Sanitizer 与 CI（约 2～2.5 小时）

### 当天目标

掌握测试环境备份/恢复、fixture、端口检查、信号/超时回归、ASan/UBSan 构建和 CI 编排。

### 时间安排

- 45 分钟：主集成脚本。
- 35 分钟：配置/日志/MIME 脚本。
- 20 分钟：Makefile Sanitizer 目标和 CI。
- 30 分钟：完整运行。
- 10 分钟：整理覆盖矩阵。

### 需要阅读的具体文件

- `tests/run_integration_tests.sh`。
- `tests/config_test.sh`、`tests/log_test.sh`、`tests/mime_test.sh`。
- `Makefile:47-79`。
- `.github/workflows/ci.yml`。
- `TESTING.md`。

### 需要理解的具体函数/脚本入口

- shell helpers：`cleanup()`、`prepare_environment()`、`create_test_fixtures()`、`is_port_listening()`、`test_case()`、`raw_request_test_case()`。
- 专项 case：`test_symlink_escape_forbidden()`、`test_truncated_post_body_returns_400()`、`test_cgi_timeout_releases_worker()`、`test_signal_shutdown_with_busy_worker()`。
- Make targets：`test`、`sanitizers`、`sanitizer-test`。

### 需要手动运行的命令

```bash
make test
make sanitizer-test
sed -n '1,220p' .github/workflows/ci.yml
git status --short
```

### 需要回答的自测问题

1. 每个脚本如何避免破坏用户已有的 config 和 logs？
2. `SKIP_BUILD=1` 为什么只应在已有匹配构建时使用？
3. ASan 和 UBSan 分别主要发现什么？为什么这里没有 TSan？
4. macOS 与 Ubuntu 的 leak detection 有什么差异？
5. 两个 CI job 为什么分开而不是只跑 Sanitizer？
6. integration script 如何证明忙 worker 下 SIGTERM 能释放端口？

### 当天完成标准

- `make test` 与 `make sanitizer-test` 全通过。
- 测试后 `git status` 没有源码/测试/配置意外改动。
- 能把每个风险点映射到现有测试，明确尚未覆盖项。

## 第 13 天：提炼亮点、缺陷和改进方案（约 2 小时）

### 当天目标

把“代码做了什么”提升为“为什么这样设计、代价是什么、怎样演进”，形成可用于评审或面试的项目判断。

### 时间安排

- 30 分钟：从调用图找 5 个枢纽。
- 35 分钟：列 6 个可证明亮点。
- 35 分钟：列 6 个限制并排序。
- 15 分钟：为前三项写改进方案和验证标准。
- 5 分钟：一分钟总结。

### 需要阅读的具体文件

- `README.md:320-371`。
- `graphify-out/GRAPH_REPORT.md:28-58,128-145`。
- `BENCHMARK.md:126-166`。
- 回查：`server.c`、`threadpool.c`、`request.c`、`utils.c`、`cgi.c`、`log.c`。
- `docs/STUDY_GUIDE.md:14-16` 章节。

### 需要理解的具体函数

- `server_run()`、`threadpool_shutdown()`：关停亮点。
- `resolve_safe_path()`：防护与 TOCTOU。
- `fork_with_cloexec_pipes()`：fd 安全与复杂度。
- `execute_cgi()`：超时、错误语义和 pipe 风险。
- `send_all()`、`cat()`：短写处理与错误传播缺口。
- `send_request()`：benchmark 结论边界。

### 需要手动运行的命令

```bash
graphify query 'server_run、accept_request、execute_cgi 为什么是跨模块枢纽？' --budget 2500
rg -n 'TODO|timeout|alarm|realpath|open_cloexec|send_all|shutdown|Content-Length' \
  README.md *.c tests/*
git log --oneline -10
```

### 需要回答的自测问题

1. 哪三个设计最能体现工程化，而不只是功能增加？
2. 哪些亮点有自动化测试直接支撑？
3. CGI、路径和阻塞发送风险应如何排序，依据是什么？
4. 若用非阻塞 `poll()` 改 CGI，状态机需要哪些输入/输出缓冲？
5. 若用 `openat()` 改路径，接口边界和测试要怎样变化？
6. 哪些判断属于“推断”，需要什么实验才能升级为事实？

### 当天完成标准

- 形成“6 个亮点 + 6 个缺陷 + 3 个改进方案”的一页纸。
- 每项都附源码位置、现有测试和未覆盖风险。
- 能回答“为什么不是生产级”而不贬低项目的教学价值。

## 第 14 天：模拟面试和项目讲解（约 2～2.5 小时）

### 当天目标

在不看稿的情况下完成 1 分钟、3 分钟、5 分钟介绍；能白板追踪主链、资源所有权和一个故障路径；完成一轮高频问答。

### 时间安排

- 20 分钟：复盘模块图和所有权图。
- 30 分钟：录制 1/3/5 分钟介绍各一遍。
- 35 分钟：回答 12 个高频问题。
- 25 分钟：白板讲静态 GET、CGI POST、SIGTERM 三条链。
- 20 分钟：现场运行 smoke test 并解释输出。
- 10 分钟：复盘表达缺口。

### 需要阅读的具体文件

- `docs/STUDY_GUIDE.md`：第 3～7、12、14～16 章。
- `main.c`、`server.c`、`request.c`：只看主链。
- `threadpool.c`、`cgi.c`、`utils.c`：只看所有权和风险函数。
- `Makefile`、`.github/workflows/ci.yml`：只看验证入口。

### 需要理解的具体函数

- 必须能白板写出：`main()` → `server_run()` → `threadpool_submit()` → `worker()` → `accept_request()`。
- 静态链：`resolve_safe_path()` → `serve_file()` → `headers()`/`cat()`。
- CGI 链：`handle_cgi()` → `execute_cgi()` → `fork_with_cloexec_pipes()` → `execve()`/`waitpid()`。
- 关闭链：`shutdown_handler()` → accept loop 退出 → `threadpool_shutdown()`。

### 需要手动运行的命令

```bash
make clean
make test
./httpd 18082
```

另一个终端做现场演示：

```bash
curl --noproxy '*' -i http://127.0.0.1:18082/
curl --noproxy '*' -i -X POST -d 'a=1' http://127.0.0.1:18082/date.cgi
./benchmark 127.0.0.1 18082 / 4 25
```

### 需要回答的自测问题

1. 30 秒内讲清线程模型和队列满行为。
2. 60 秒内讲清 client fd 所有权。
3. 90 秒内讲清 CGI POST 的两条 pipe 和错误清理。
4. 为什么 `realpath()` 防护仍不是绝对安全？
5. 为什么 ASan/UBSan 通过不能证明没有数据竞争？
6. 说出一个代码亮点、一个设计代价、一个最优先改进，并给证据。
7. 面试官质疑“CGI 已超时就没有死锁风险”，你如何反驳？
8. 面试官要求扩展到 keep-alive，你会先改哪些所有权和状态边界？

### 当天完成标准

- 三个时长版本都在目标时间 ±10% 内完成，并覆盖定位、架构、难点、验证、限制。
- 12 个高频问题至少 10 个能先给结论、再给代码证据、最后给权衡。
- 能现场跑通测试和两个请求，并解释关键输出。
- 最终只保留基于真实代码的表述；所有未验证判断都明确标“推断”。

## 14 天结束后的验收清单

- [ ] 能从 `main()` 追到静态文件和 CGI 的每一个关键函数。
- [ ] 能画出 config、threadpool task、client fd、静态 `FILE *`、CGI pipe/child 的生命周期。
- [ ] 能解释四把 mutex 和 relaxed 原子计数的职责。
- [ ] 能说出所有输入上限、读超时和 CGI 超时。
- [ ] 能解释路径 confinement、fd close-on-exec 和其剩余风险。
- [ ] 能独立运行 `make test` 与 `make sanitizer-test` 并定位失败层次。
- [ ] 能区分“测试证明的事实”“源码直接事实”和“推断”。
- [ ] 能完成 1/3/5 分钟介绍和一轮系统化项目问答。
