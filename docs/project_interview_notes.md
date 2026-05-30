# 项目学习与面试复习说明文档

> 生成日期：2026-05-30  
> 分析依据：当前工作区真实源码、头文件、Makefile、测试脚本、README/测试文档、静态资源和 CGI 示例。  
> 验证说明：本次已运行 `tests/run_integration_tests.sh`，全部通过；已运行 `tests/log_test.sh`，通过。`tests/config_test.sh` 因我并行运行多个会改写 `config/server.conf` 的测试脚本而互相干扰，结果不作为代码失败依据；`tests/mime_test.sh` 的沙箱外执行被系统用量限制拒绝，未绕过执行。

## 0. 项目总览

| 项目项 | 内容 |
|---|---|
| 项目名称 | Tinyhttpd Engineering Retrofit / Tinyhttpd 工程化改造 |
| 一句话定位 | 基于 tinyhttpd 教学项目改造的 C 语言轻量 HTTP Server，用来学习 socket、HTTP、线程池、CGI、配置解析、日志、MIME、路径安全和集成测试。 |
| 技术栈 | C、POSIX Socket、pthread、fork/pipe/execl、文件 I/O、Shell、curl、nc、perl、Makefile |
| 项目解决的问题 | 在 tinyhttpd 教学代码基础上，补充固定线程池、静态文件服务、CGI 执行、配置文件、访问/错误日志、MIME 类型、请求大小限制、路径安全、集成测试和简单压测。 |
| 核心功能 | 监听 TCP 端口；加载 `config/server.conf`；解析 GET/HEAD/POST/OPTIONS；返回 `root_dir` 下静态文件；执行 CGI；使用线程池处理连接；写 `logs/access.log` 和 `logs/error.log`；根据扩展名设置 Content-Type；限制 header/body；防止常见路径逃逸。 |
| 适合写进简历的亮点 | 将原教学 HTTP Server 模块化为 server/request/response/static file/CGI/utils/config/log/mime/threadpool；用线程池替代每请求创建线程；用 `realpath()` 约束文档根目录；用 `fread()` + `send_all()` 支持二进制响应；实现配置、日志、MIME 和多脚本测试。 |
| 面试风险点 | 不是生产级 Web Server；不完整支持 HTTP/1.1；没有 keep-alive、chunked、TLS、epoll/kqueue；CGI 没有 sandbox；URL decode 不完整；线程池初始化失败没有向 server 返回明确错误；benchmark 不严谨；部分旧文档仍有过时描述。 |
| 我需要重点复习的知识 | TCP socket 生命周期、HTTP 报文格式、阻塞 I/O、pthread 线程池、互斥锁和条件变量、fork/exec/pipe、CGI 环境变量、路径规范化、配置解析、日志并发写、MIME、C 字符串和缓冲区边界、Shell 集成测试。 |

## 1. 项目背景与定位

### 这个项目是什么？

这是一个 C 语言实现的轻量 HTTP Server。它的原始基础是 J. David Blackstone 的 tinyhttpd 教学项目，当前仓库已经明显超过原始 tinyhttpd 的单文件教学形态：代码被拆成 `main.c`、`server.c`、`request.c`、`response.c`、`static_file.c`、`cgi.c`、`utils.c`、`config.c`、`log.c`、`mime.c`、`threadpool.c` 等模块，并用 `Makefile` 统一编译。

它的目标不是成为 nginx/Apache 那种生产服务器，而是用尽量少的 C 代码把网络编程、HTTP 请求处理、文件响应、CGI 进程通信、线程池、配置、日志和测试串起来，适合作为系统编程和后端基础项目讲。

### 它解决什么问题？

从学习和面试角度，它解决这些问题：

- 如何创建 TCP server：`socket`、`setsockopt`、`bind`、`listen`、`accept`；
- 如何解析 HTTP 请求行和请求头；
- 如何把 URL 映射到本地文档根目录；
- 如何返回静态文件，并设置 `Content-Type`、`Content-Length`、`Connection: close`；
- 如何用 `pipe + fork + dup2 + execl` 执行 CGI；
- 如何把 POST body 写入 CGI stdin；
- 如何用固定线程池控制并发；
- 如何通过配置文件控制端口、线程数、根目录和日志开关；
- 如何把访问日志和错误日志写到文件；
- 如何写可复现的端到端测试脚本。

### 它属于什么类型的项目？

它属于网络程序 / 后端基础设施学习项目 / 系统编程练习项目。它不是业务系统，也不是 Web 框架，不包含数据库、ORM、认证授权、模板引擎或完整路由系统。

### 它适合投哪些岗位？

- C/C++ 后端开发；
- Linux 后端开发；
- 网络编程方向；
- 基础架构实习；
- 测试开发/运维开发中偏服务验证和工具开发的岗位；
- 国企、银行、运营商信息科技岗中需要展示网络、操作系统和 C 语言基础的场景。

### 面试时如何一句话介绍？

可以这样说：

> 这是我基于 tinyhttpd 教学项目做的 C 语言轻量 HTTP Server 工程化改造，当前代码拆成 server、request、response、static file、CGI、config、log、mime、threadpool 等模块，实现了基础 HTTP 方法、静态文件、CGI、配置、日志、路径安全、线程池和集成测试。

### 和真实工业级项目相比有什么差距？

- 协议：只做基础 HTTP/1.0 风格响应，不完整支持 HTTP/1.1；
- 连接：每个请求处理完关闭连接，没有 keep-alive；
- 传输：没有 chunked transfer、range request、gzip、TLS；
- 并发：固定线程池 + 阻塞 I/O，不是 epoll/kqueue 事件驱动；
- 安全：CGI 没有 sandbox，不限制 CPU/内存/系统调用；
- 配置：只支持简单 `key=value`，没有热加载；
- 日志：能线程安全追加日志，但没有日志轮转、结构化日志、指标；
- 测试：有多脚本测试，但没有 CI、单元测试、sanitizer、fuzz；
- 性能：有简单 benchmark 工具，但没有严谨性能报告。

### 30 秒介绍版本

这是一个基于 tinyhttpd 的 C 语言轻量 HTTP Server 改造项目。当前代码按模块拆分，`main.c` 负责加载配置和启动，`server.c` 负责 socket 和线程池主循环，`request.c` 解析 HTTP 请求并分发，`static_file.c` 返回静态文件，`cgi.c` 执行 CGI，`config.c/log.c/mime.c` 分别处理配置、日志和 MIME。它能跑 GET、HEAD、POST、OPTIONS，有路径安全、请求大小限制和测试脚本，但不是生产级 Web Server。

### 1 分钟介绍版本

项目启动时，`main.c` 调用 `load_config("config/server.conf")` 加载配置，缺失时使用默认端口 8080、线程数 4、根目录 `./htdocs`、访问/错误日志开启。随后 `server_run()` 创建监听 socket，注册 SIGINT/SIGTERM 退出处理，初始化线程池，主线程循环 `accept()`。连接交给 worker 后，`handle_client()` 设置 5 秒读取超时，进入 `accept_request()`。请求模块解析 method/url/header，限制单行 header 1024 字节、总 header 8KB、POST body 1MB；GET/HEAD 映射到配置的 `root_dir`，普通文件走静态响应，可执行文件或带 query 的请求走 CGI；POST 只面向 CGI。日志模块线程安全写访问日志和错误日志。测试脚本覆盖静态文件、CGI、HEAD/OPTIONS、路径逃逸、日志、配置和 MIME。

### 简历项目描述版本

基于 tinyhttpd 改造 C 语言轻量 HTTP Server，将原教学代码模块化为 server/request/response/static file/CGI/config/log/mime/threadpool；使用 POSIX Socket 和 pthread 固定线程池处理连接，支持 GET/HEAD/POST/OPTIONS、静态资源、CGI、配置文件、访问/错误日志和 MIME 响应头；通过 `realpath()` 约束文档根目录、限制 header/body 大小、统一 `send_all()` 发送逻辑，并编写 Shell 测试验证核心路径和异常场景。

## 2. 仓库结构分析

| 路径 | 作用 | 重要程度 | 面试是否可能问到 |
|---|---|---|---|
| `main.c` | 程序入口，加载 `config/server.conf`，解析命令行端口覆盖，设置全局配置并调用 `server_run()`。 | 必须看 | 很可能 |
| `server.c` / `server.h` | 服务生命周期：socket/bind/listen/accept、信号处理、线程池初始化、client fd 分发。 | 必须看 | 非常可能 |
| `request.c` / `request.h` | HTTP 请求核心：解析请求行、读取 header、分发 GET/HEAD/POST/OPTIONS、记录访问日志和错误日志。 | 必须看 | 非常可能 |
| `response.c` / `response.h` | 响应生成：错误页、静态文件 header、`Content-Length`、`Connection: close`、400/404/413/501/500。 | 必须看 | 很可能 |
| `static_file.c` / `static_file.h` | 静态文件打开、读取和发送；返回状态码。 | 必须看 | 很可能 |
| `cgi.c` / `cgi.h` | CGI 执行：POST 校验、pipe/fork/dup2/execl、环境变量、超时、父进程转发输出。 | 必须看 | 非常可能 |
| `utils.c` / `utils.h` | 通用函数：`send_all()`、`get_line()`、`resolve_safe_path()`、`is_path_traversal()`；包含请求大小限制宏。 | 必须看 | 非常可能 |
| `config.c` / `config.h` | 配置模块：默认配置、`key=value` 文件解析、全局 active config。 | 必须看 | 很可能 |
| `log.c` / `log.h` | 线程安全访问日志和错误日志，自动创建 `logs/`，用 mutex 保护写入。 | 必须看 | 很可能 |
| `mime.c` / `mime.h` | 根据文件扩展名返回 `Content-Type`，未知默认 `application/octet-stream`。 | 重要 | 可能 |
| `threadpool.c` / `threadpool.h` | 固定线程池、FIFO 队列、worker、mutex、cond、shutdown。 | 必须看 | 非常可能 |
| `Makefile` | 构建入口，`HTTPD_SRCS` 明确列出 server 所有模块。 | 必须看 | 可能 |
| `tests/run_integration_tests.sh` | 主集成测试，覆盖静态文件、CGI、HEAD、OPTIONS、header 限制、路径逃逸、POST 截断、CGI 超时。 | 必须看 | 非常可能 |
| `tests/config_test.sh` | 配置测试：缺失配置默认值、自定义端口、自定义根目录、非法配置不崩溃。 | 重要 | 可能 |
| `tests/log_test.sh` | 日志测试：验证 200/404/CGI 500 访问日志和错误日志格式。 | 重要 | 可能 |
| `tests/mime_test.sh` | MIME 测试：验证 html/css/js/png/unknown 和错误页 header。 | 重要 | 可能 |
| `benchmark.c` | 简单多线程 GET 压测工具，统计成功率、QPS、平均响应时间。 | 一般 | 可能 |
| `simpleclient.c` | 老式 TCP 字符客户端，连接 `127.0.0.1:9734`，与当前 HTTP 主流程无关。 | 低 | 偶尔 |
| `htdocs/index.html` | 默认首页，包含表单 POST 到 `color.cgi`。 | 重要 | 可能 |
| `htdocs/index2.html` | 另一个示例首页，表单 POST 到 `date.cgi`。 | 一般 | 不太可能 |
| `htdocs/date.cgi` | Bash CGI 示例，输出当前日期 HTML。 | 重要 | 可能 |
| `htdocs/color.cgi` | Bash CGI 示例，实际也只是输出日期，没有读取 color 参数。 | 一般 | 可能作为风险点 |
| `htdocs/check.cgi` | Perl CGI 示例，依赖 Perl CGI 模块，当前主测试未覆盖。 | 一般 | 可能 |
| `README.md` | 项目说明，新增了配置、日志、MIME 说明，但仍有部分流程/限制描述过时。 | 重要 | 可能 |
| `TESTING.md` | 测试复现说明，最新验证日期仍写 2026-05-09，已落后于当前代码。 | 参考 | 可能 |
| `THREADPOOL_INTEGRATION.md` | 线程池改造历史说明，仍围绕旧 `httpd.c`，与当前模块化结构不一致。 | 参考 | 可能 |
| `BENCHMARK.md` | benchmark 使用说明，示例多用 80 端口，与默认 8080/配置端口不完全一致。 | 参考 | 可能 |
| `logs/access.log`、`logs/error.log` | 运行日志，属于运行产物，`.gitignore` 已忽略 `logs/`。 | 运行产物 | 可提但不应当成源码 |
| `httpd`、`client`、`benchmark`、`threadpool.o`、`httpd.dSYM/` | 构建/调试产物，`.gitignore` 已覆盖。 | 生成物 | 不应写进核心能力 |
| `.DS_Store`、`.vscode/` | 本地系统/编辑器文件，非项目功能。 | 无关 | 不应重点讲 |

### README 与真实代码不一致或过时之处

- README 流程图写 `main: accept()`，但当前 `main.c` 只加载配置和调用 `server_run()`，真正 `accept()` 在 `server.c:server_run()`。
- README 线程池接口后写“当前 server 使用 4 个 worker”，当前默认确实是 4，但 `thread_num` 已可由 `config/server.conf` 配置，`server.c` 使用 `cfg->thread_num`。
- README limitation 仍说“accept 循环没有完整信号驱动退出流程”，但当前 `server.c` 已有 SIGINT/SIGTERM handler 和 `running` 标志；更准确的说法是“已有基础信号退出，但不是完整 graceful drain”。
- `THREADPOOL_INTEGRATION.md` 仍以 `httpd.c` 为中心，且任务内存管理描述和真实 `threadpool_task` 的 `int *client_fd_ptr` 不完全一致。
- `TESTING.md` 的“Latest Local Verification”停在 2026-05-09；当前已经新增 config/log/mime 测试脚本。
- `BENCHMARK.md` 示例使用 80 端口，当前默认端口是 8080，且端口可能由配置或命令行覆盖。

## 3. 编译、运行与测试方式

### 编译

```bash
make
make clean && make
```

`Makefile` 当前编译 `httpd` 的源码列表：

```makefile
HTTPD_SRCS = main.c server.c request.c response.c static_file.c cgi.c utils.c log.c config.c mime.c threadpool.c
```

输出：

- `httpd`：HTTP Server；
- `client`：旧 TCP demo client；
- `benchmark`：简单压测工具；
- `threadpool.o`：线程池对象文件。

### 运行

默认运行：

```bash
./httpd
```

指定端口覆盖配置：

```bash
./httpd 18080
```

配置文件路径固定为 `config/server.conf`。配置文件不存在时不报错，使用内置默认值：

```text
port=8080
thread_num=4
root_dir=./htdocs
enable_access_log=1
enable_error_log=1
```

示例配置：

```conf
port=18080
thread_num=8
root_dir=./htdocs
enable_access_log=1
enable_error_log=1
```

### demo 请求

```bash
curl --noproxy '*' -i http://127.0.0.1:8080/
curl --noproxy '*' -I http://127.0.0.1:8080/
curl --noproxy '*' -i http://127.0.0.1:8080/date.cgi
curl --noproxy '*' -i -X POST -d 'a=1' http://127.0.0.1:8080/date.cgi
curl --noproxy '*' -i -X OPTIONS http://127.0.0.1:8080/
```

### 测试脚本

```bash
tests/run_integration_tests.sh
tests/config_test.sh
tests/log_test.sh
tests/mime_test.sh
```

说明：

- `run_integration_tests.sh`：主集成测试，已在本次运行通过；
- `log_test.sh`：日志测试，已在本次运行通过；
- `config_test.sh`：脚本存在，覆盖配置行为；本次并行运行时被其他测试改写配置干扰，因此结果不可靠；
- `mime_test.sh`：脚本存在，覆盖 MIME；本次沙箱外运行被系统用量限制拒绝，未执行。

### 常见运行失败原因

- 端口被占用或沙箱禁止 bind；
- `root_dir` 配错或目录不存在，`realpath(root_dir)` 失败返回 500；
- CGI 脚本没有执行权限；
- `config/server.conf` 写错：非法值会 warning 并回退默认，不会中止；
- `curl` 走代理，本地测试建议 `--noproxy '*'`；
- `check.cgi` 依赖 Perl CGI 模块；
- worker 数配置过大：`config.c` 允许 `thread_num` 到 1024，但 `threadpool_init()` 只允许 1 到 100，超过 100 会打印 invalid 并返回，server 仍继续跑但线程池可能不可用。

### 面试时如何证明能跑、能测、能复现

可以按这个顺序演示：

1. `make clean && make`；
2. `./httpd 18080`；
3. `curl -i http://127.0.0.1:18080/`；
4. `curl -i http://127.0.0.1:18080/date.cgi`；
5. `tests/run_integration_tests.sh`；
6. `tests/log_test.sh`；
7. 如果环境允许，再跑 `tests/config_test.sh` 和 `tests/mime_test.sh`。

## 4. 核心功能清单

| 功能名称 | 代码位置 | 入口函数或核心函数 | 输入 | 输出 | 内部执行流程 | 边界情况 | 测试覆盖 | 简历能不能写 | 面试怎么讲 |
|---|---|---|---|---|---|---|---|---|---|
| 配置加载 | `config.c/h`、`main.c` | `load_config()`、`set_server_config()` | `config/server.conf` | `server_config_t` | 默认值 -> 打开文件 -> 解析 key=value -> 校验范围 -> 保存全局配置 | 缺失配置不报错；非法值 warning；未知 key warning | `config_test.sh` 脚本覆盖，本文未可靠复跑 | 能 | “用简单配置文件控制端口、线程数、根目录和日志开关。” |
| 服务启动 | `server.c` | `startup()`、`server_run()` | 端口 | 监听 socket | socket -> SO_REUSEADDR -> bind -> listen -> accept | bind/listen 失败直接退出 | 主集成测试覆盖 | 能 | “掌握 TCP 服务端启动流程。” |
| 信号退出 | `server.c` | `shutdown_handler()` | SIGINT/SIGTERM | `running=0` | accept 被 EINTR 打断后跳出循环并 shutdown 线程池 | 活跃请求仍需等待 worker 返回 | 未专门覆盖 | 可以 | “基础退出，不是完整 graceful drain。” |
| 线程池 | `threadpool.c/h` | `threadpool_init()`、`threadpool_submit()`、`worker()` | client fd | 调 handler | 有界 FIFO 队列、mutex、cond、worker | 队列满拒绝；初始化失败不反馈 server | 间接覆盖 | 能 | “固定 worker 控制资源。” |
| 请求行解析 | `request.c`、`utils.c` | `parse_request_line()`、`get_line()` | socket 字节流 | method/url | 逐字节读行，解析 method/url | 行过长 400；不支持方法 501 | 部分覆盖 | 能 | “手写最小 HTTP parser。” |
| header 解析 | `request.c` | `read_headers()` | header 行 | `content_length` | 逐行读，累计大小，解析 Content-Length | 单行 1024、总 8KB、body 1MB | 主集成测试覆盖 | 能 | “有基础输入边界限制。” |
| OPTIONS | `request.c` | `accept_request()` | OPTIONS 请求 | Allow/CORS 响应 | 直接写 200、Allow、CORS 头 | 不区分资源 | 主集成测试覆盖 | 可以 | “基础预检响应，不是完整 CORS 策略。” |
| 静态文件 | `utils.c`、`static_file.c`、`response.c` | `resolve_safe_path()`、`serve_file()` | URL | 文件响应 | root_dir + URL -> stat/realpath -> headers -> fread/send_all | 404、403；无 Range/缓存 | 主集成测试覆盖 | 能 | “支持二进制文件和 Content-Length。” |
| MIME | `mime.c`、`response.c` | `get_mime_type()` | 文件路径 | Content-Type | 按扩展名映射 | 未知默认 octet-stream | `mime_test.sh` 脚本覆盖，本文未复跑 | 能 | “基础 MIME 映射，不是完整数据库。” |
| CGI | `cgi.c` | `handle_cgi()`、`execute_cgi()` | method/path/query/body | CGI 输出 | pipe -> fork -> dup2 -> putenv -> execl -> 父进程转发 | 无 sandbox；按字节转发；exit 非 0 记录错误 | 主集成测试覆盖 | 能 | “核心是进程通信和 fd 重定向。” |
| 日志 | `log.c`、`request.c`、`cgi.c` | `log_access()`、`log_error_message()` | 请求信息/错误信息 | `logs/*.log` | mutex -> mkdir logs -> fopen append -> localtime_r -> fprintf | 无轮转；磁盘错误静默 | `log_test.sh` 通过 | 能 | “线程安全追加访问和错误日志。” |
| 错误页 | `response.c` | `send_error_page()` | status/message | HTML 响应 | 构造 body，设置 Content-Length、Connection close | message 当前为常量；body 4096 固定 | 多处间接覆盖 | 可以 | “统一错误响应，并补了 Content-Length。” |
| benchmark | `benchmark.c` | `send_request()` | host/port/path/线程数/请求数 | 成功率/QPS/平均耗时 | 多线程创建 socket 发 GET | 只 recv 一次；无超时/分位数 | 无自动测试 | 谨慎写 | “辅助压测，不是严谨基准。” |

## 5. 主流程深度解析

最核心链路：用户请求 `/`，server 接收连接，线程池 worker 解析 GET，映射到 `root_dir/index.html`，发送静态响应，记录访问日志并关闭连接。

### 完整流程图

```text
用户启动程序
-> main() [main.c]
-> load_config(CONFIG_PATH, &cfg) [config.c]
-> 命令行端口覆盖 cfg.port [main.c]
-> set_server_config(&cfg) [config.c]
-> server_run(cfg.port) [server.c]
-> signal(SIGPIPE, SIG_IGN) + sigaction(SIGINT/SIGTERM)
-> startup(&port) [server.c]
   -> socket
   -> setsockopt(SO_REUSEADDR)
   -> bind
   -> listen
-> threadpool_init(cfg->thread_num, 1000, handle_client) [threadpool.c]
-> accept(server_sock) [server.c]
-> threadpool_submit(client_sock) [threadpool.c]
-> worker() 出队 [threadpool.c]
-> handle_client(client) [server.c]
   -> setsockopt(SO_RCVTIMEO, 5s)
-> accept_request(client) [request.c]
   -> getpeername + inet_ntop
   -> parse_request_line
   -> read_headers
   -> GET/HEAD/POST/OPTIONS 分发
-> GET /
   -> resolve_safe_path(client, "/", path, ..., &st, 1) [utils.c]
   -> handle_static_file(client, path, is_head, st.st_size) [static_file.c]
   -> serve_file
   -> headers [response.c]
   -> cat fread + send_all [static_file.c/utils.c]
-> log_access(client_ip, method, access_url, 200) [log.c]
-> close(client)
```

### 参数如何解析

配置文件先加载，命令行端口后覆盖：

- `load_config(CONFIG_PATH, &cfg)` 设置默认值并读取 `config/server.conf`；
- 如果 `argc > 1`，`atoi(argv[1])` 转成端口；
- 非法端口回退 8080；
- `set_server_config(&cfg)` 存到全局 `active_config`。

风险：命令行端口仍使用 `atoi()`，不如配置里的 `strtol()` 严谨。

### 核心对象如何创建

- `server_config_t cfg`：启动配置；
- `server_sock`：监听 fd；
- `threadpool_t pool`：`threadpool.c` 内静态全局；
- `threadpool_task_t`：每个 client fd 的队列节点；
- 请求局部 buffer：`method[255]`、`url[255]`、`path[512]`；
- CGI pipe：`cgi_output[2]`、`cgi_input[2]`。

### 错误如何处理

- 启动系统调用失败：`error_die()` 退出；
- 请求行过长：400；
- 不支持方法：501；
- header 过长：400/413；
- body 太大：413；
- 文件不存在：404，同时错误日志记录；
- 路径逃逸：403，同时错误日志记录；
- 静态文件权限不足：403；
- CGI pipe/fork/输出异常/非 0 退出：500，同时错误日志记录；
- 线程池队列满：server 关闭 client fd。

### 资源如何释放

- 静态文件：`fclose()` + `close(client)`；
- CGI：关闭 pipe，`waitpid()` 回收子进程，关闭 client；
- 线程池任务：worker 释放 fd 指针和 task；
- server 停止：`threadpool_shutdown()` join worker，释放线程数组和同步对象；
- 测试脚本：trap cleanup 杀 server 和删除临时 fixture。

## 6. 核心模块逐个讲解

### 6.1 配置模块

#### 6.1.1 模块作用

配置模块负责提供默认配置、读取 `config/server.conf`、校验配置值，并把配置保存为全局 active config，供 server、utils、log 等模块读取。

#### 6.1.2 关键文件

- `config.c`
- `config.h`
- `main.c`
- `tests/config_test.sh`

#### 6.1.3 关键数据结构

```c
typedef struct {
 u_short port;
 int thread_num;
 char root_dir[MAX_ROOT_DIR_LEN];
 int enable_access_log;
 int enable_error_log;
} server_config_t;
```

全局状态：

- `static server_config_t active_config`；
- `static int active_config_initialized`。

#### 6.1.4 关键函数

- `init_default_config()`：写入默认端口、线程数、根目录、日志开关。
- `load_config()`：读取 `key=value`，忽略空行和注释，校验并覆盖默认值。
- `set_server_config()`：保存 active config。
- `get_server_config()`：返回 active config，未初始化时自动初始化默认值。
- `trim()`、`parse_long()`、`discard_line_remainder()`：内部工具函数。

错误情况：配置文件不存在时不是错误；打不开非 ENOENT 文件打印 warning；行过长忽略；未知 key warning；非法值回退默认。

#### 6.1.5 执行流程

```text
main
-> load_config
   -> init_default_config
   -> fopen config/server.conf
   -> fgets line
   -> trim
   -> parse key=value
   -> 按 key 校验并写 cfg
-> 命令行端口覆盖
-> set_server_config
```

#### 6.1.6 设计取舍

优点：配置缺失不影响启动，适合本地 demo；非法配置不会崩溃；模块接口清晰。  
局限：没有热加载；没有配置文件路径参数；配置项少；`thread_num` 在配置中允许 1024，但线程池只接受最大 100，存在模块约束不一致。

#### 6.1.7 面试可能问什么

问：配置文件不存在怎么办？  
答：`load_config()` 先初始化默认配置，`fopen()` 如果是 ENOENT 就直接返回 0，server 用默认值启动。

### 6.2 服务生命周期模块

#### 6.2.1 模块作用

`server.c` 负责网络服务启动、信号处理、监听 socket 和连接分发。

#### 6.2.2 关键文件

- `server.c`
- `server.h`
- `threadpool.h`
- `request.h`
- `config.h`

#### 6.2.3 关键数据结构

- `static volatile sig_atomic_t running`；
- `struct sockaddr_in name`；
- `server_sock`、`client_sock`；
- `const server_config_t *cfg`。

#### 6.2.4 关键函数

- `startup(u_short *port)`：创建 socket 并监听。
- `server_run(u_short port)`：注册信号，启动线程池，accept 主循环。
- `handle_client(int client)`：设置 5 秒接收超时，调用 `accept_request()`。
- `shutdown_handler()`：设置 `running=0`。

#### 6.2.5 执行流程

```text
server_run
-> signal/sigaction
-> startup
-> threadpool_init(cfg->thread_num, 1000, handle_client)
-> accept loop
-> threadpool_submit
-> exit loop
-> threadpool_shutdown
```

#### 6.2.6 设计取舍

优点：主线程只负责 accept，业务处理交给线程池；SIGPIPE 被忽略，避免客户端断开杀进程。  
局限：listen backlog 固定 5；队列大小固定 1000；线程池初始化失败没有明确返回；退出不是完整 graceful drain。

#### 6.2.7 面试可能问什么

问：为什么忽略 SIGPIPE？  
答：客户端提前断开时服务端写 socket 可能触发 SIGPIPE，默认会终止进程；忽略后让 `send()` 返回错误。

### 6.3 请求解析模块

#### 6.3.1 模块作用

`request.c` 是 HTTP 请求处理核心，负责解析请求、读取 header、按方法分发、记录日志。

#### 6.3.2 关键文件

- `request.c`
- `request.h`
- `utils.c`
- `response.c`
- `static_file.c`
- `cgi.c`
- `log.c`

#### 6.3.3 关键数据结构

- `method[255]`；
- `url[255]`；
- `access_url[255]`；
- `path[512]`；
- `struct stat st`；
- `content_length`；
- `query_string`；
- `client_ip[INET_ADDRSTRLEN]`。

#### 6.3.4 关键函数

- `accept_request()`：总分发函数。
- `parse_request_line()`：解析 method/url。
- `read_headers()`：读取 header 并解析 Content-Length。
- `log_request_error()`：根据 403/404/500 写错误日志。

错误情况：请求行太长 400；不支持方法 501；header 单行太长 400；header 总大小超 8KB 413；Content-Length 非法 400。

#### 6.3.5 执行流程

```text
client fd
-> getpeername/inet_ntop
-> parse_request_line
-> read_headers
-> OPTIONS: 直接响应 + access log
-> GET/HEAD:
   -> 分离 query
   -> resolve_safe_path
   -> 根据可执行位/query 判断 CGI
   -> static_file 或 cgi
   -> access/error log
-> POST:
   -> resolve_safe_path
   -> handle_cgi
   -> access/error log
```

#### 6.3.6 设计取舍

优点：能明确返回状态码，并把状态码带入日志；把 access_url 保存下来，避免 query 拆分修改原 URL 后日志丢失。  
局限：不解析 HTTP version；不支持 chunked；URL decode 不完整；请求行格式校验较弱。

#### 6.3.7 面试可能问什么

问：为什么要保存 `access_url`？  
答：GET/HEAD 如果有 `?`，代码会把 `url` 里的 `?` 改成 `\0` 用于路径解析；`access_url` 保留原始 URL 便于日志记录。

### 6.4 响应与 MIME 模块

#### 6.4.1 模块作用

`response.c` 负责发送响应头和错误页；`mime.c` 负责根据扩展名决定 Content-Type。

#### 6.4.2 关键文件

- `response.c`
- `response.h`
- `mime.c`
- `mime.h`

#### 6.4.3 关键数据结构

无复杂结构体，主要是 `char buf[1024]`、`char body[4096]` 和 MIME 字符串。

#### 6.4.4 关键函数

- `headers()`：发送静态文件 200 header。
- `send_error_page()`：生成 HTML 错误 body 并带 Content-Length。
- `bad_request()`、`not_found()`、`unimplemented()`、`send_413()`、`cannot_execute()`。
- `get_mime_type()`：扩展名映射。

#### 6.4.5 执行流程

```text
静态文件
-> headers
-> get_mime_type
-> send_all 状态行/Server/Content-Type/Content-Length/Connection

错误
-> send_error_page
-> snprintf body
-> 计算 body_len
-> send header + body
```

#### 6.4.6 设计取舍

优点：错误页也有 `Content-Length`；未知 MIME 默认 octet-stream 比 text/plain 更保守。  
局限：MIME 表是硬编码；错误 body 固定 4096，虽然当前 message 是常量，未来若接用户输入要注意截断和 HTML 注入。

#### 6.4.7 面试可能问什么

问：为什么未知类型用 `application/octet-stream`？  
答：未知文件按二进制下载更保守，避免把未知内容当文本解释。

### 6.5 静态文件模块

#### 6.5.1 模块作用

打开真实路径下的文件，发送响应头和文件内容，处理 HEAD 和权限错误。

#### 6.5.2 关键文件

- `static_file.c`
- `static_file.h`
- `utils.c`
- `response.c`

#### 6.5.3 关键数据结构

- `FILE *resource`；
- `char buf[4096]`；
- `off_t file_size`。

#### 6.5.4 关键函数

- `handle_static_file()`：调用 `serve_file()` 并返回状态码。
- `serve_file()`：打开文件，发送 header/body，返回 200/403/404。
- `cat()`：`fread()` 分块读并 `send_all()`。

#### 6.5.5 执行流程

```text
resolve_safe_path 得到 path
-> serve_file
-> fopen
   -> EACCES: 403
   -> 其他失败: 404
-> headers
-> !HEAD 时 cat
-> fclose + close(client)
```

#### 6.5.6 设计取舍

优点：用 `fread()` 支持 NUL 字节；权限错误能返回 403。  
局限：没有 `sendfile()`、Range、缓存、压缩；`fopen("r")` 在 POSIX 没问题，但不是显式二进制模式。

#### 6.5.7 面试可能问什么

问：如何避免二进制文件被截断？  
答：不使用 `strlen()`，而是用 `fread()` 返回的实际字节数传给 `send_all()`。

### 6.6 CGI 模块

#### 6.6.1 模块作用

执行 CGI 脚本，设置 CGI 环境变量，转发 POST body 和 CGI 输出。

#### 6.6.2 关键文件

- `cgi.c`
- `cgi.h`
- `htdocs/date.cgi`
- `htdocs/color.cgi`
- `htdocs/check.cgi`

#### 6.6.3 关键数据结构

- `cgi_output[2]`；
- `cgi_input[2]`；
- `pid_t pid`；
- `REQUEST_METHOD`、`QUERY_STRING`、`CONTENT_LENGTH`。

#### 6.6.4 关键函数

- `handle_cgi()`：POST 前置校验。
- `execute_cgi()`：pipe、fork、dup2、putenv、alarm、execl、父进程转发。

错误情况：POST 缺 Content-Length 返回 400；body 过大 413；pipe/fork/write/read 失败 500；CGI 非 0 退出记录错误并返回 500。

#### 6.6.5 执行流程

```text
handle_cgi
-> execute_cgi
-> pipe output/input
-> fork
   -> child:
      -> dup2 stdout/stdin
      -> putenv REQUEST_METHOD/QUERY_STRING/CONTENT_LENGTH
      -> alarm(5)
      -> execl(path, path, NULL)
      -> 失败 exit(127)
   -> parent:
      -> POST 时 recv body 写 input pipe
      -> 先 read CGI 第一个字节，失败则 500
      -> 发送 HTTP/1.0 200 OK + Server
      -> HEAD 只转发 header
      -> 非 HEAD 转发全部 CGI 输出
      -> waitpid，非 0 记录错误并返回 500
```

#### 6.6.6 设计取舍

优点：能讲清楚进程、管道和 fd 重定向；比旧版本多了 CGI 非 0 退出检测。  
局限：父进程在发送 200 后才 `waitpid()` 检测退出码，如果 CGI 已输出部分内容后失败，客户端可能已经收到 200；没有 sandbox；按字节转发效率低。

#### 6.6.7 面试可能问什么

问：为什么两个 pipe？  
答：一个传 CGI stdout 到父进程，一个传 POST body 到 CGI stdin；pipe 是单向的。

### 6.7 日志模块

#### 6.7.1 模块作用

线程安全写访问日志和错误日志，日志开关由配置控制。

#### 6.7.2 关键文件

- `log.c`
- `log.h`
- `config.h`
- `tests/log_test.sh`

#### 6.7.3 关键数据结构

- `static pthread_mutex_t log_mutex`；
- `logs/access.log`；
- `logs/error.log`。

#### 6.7.4 关键函数

- `log_access()`：写 `[time] ip "METHOD URL" status`。
- `log_error_message()`：写 `[time] ERROR: message`。
- `ensure_log_dir()`：创建 `logs/`。
- `format_timestamp()`：用 `localtime_r()` 格式化时间。

#### 6.7.5 执行流程

```text
log_access/log_error_message
-> 检查配置开关
-> pthread_mutex_lock
-> ensure_log_dir
-> fopen append
-> format_timestamp
-> fprintf
-> fclose
-> unlock
```

#### 6.7.6 设计取舍

优点：mutex 避免多线程日志行交错；`localtime_r()` 比 `localtime()` 更适合多线程；日志开关可配置。  
局限：每次请求都 open/close 文件；没有日志轮转；磁盘写失败没有返回给请求处理。

#### 6.7.7 面试可能问什么

问：日志为什么需要锁？  
答：多个 worker 会并发处理请求并写日志，锁能避免同一文件的日志行交错和数据竞争。

### 6.8 路径安全与工具模块

#### 6.8.1 模块作用

`utils.c/h` 提供完整发送、读行、路径解析和基础路径遍历检查。

#### 6.8.2 关键文件

- `utils.c`
- `utils.h`
- `config.h`
- `response.h`

#### 6.8.3 关键数据结构

- `MAX_REQUEST_SIZE`、`MAX_HEADER_SIZE`、`MAX_HEADER_LINE_SIZE`、`CGI_TIMEOUT_SECONDS`；
- `docroot[PATH_MAX]`；
- `resolved_path[PATH_MAX]`。

#### 6.8.4 关键函数

- `send_all()`：循环发送完整 buffer。
- `get_line()`：读 socket 一行，兼容 CRLF。
- `resolve_safe_path()`：用配置的 `root_dir` 和 `realpath()` 做路径约束。
- `is_path_traversal()`：拒绝包含 `..` 的 path。
- `error_die()`：系统错误退出。

#### 6.8.5 执行流程

```text
url
-> is_path_traversal
-> get_server_config()->root_dir
-> realpath(root_dir)
-> snprintf(root_dir + url)
-> 目录补 index.html
-> stat
-> realpath(path)
-> 检查 resolved_path 是否在 docroot 下
```

#### 6.8.6 设计取舍

优点：`realpath()` 能防 symlink 逃逸，root_dir 可配置。  
局限：不做 URL decode；`is_path_traversal()` 只要包含 `..` 就拒绝，简单但粗糙；`path[strlen(path)-1]` 假设 path 非空。

#### 6.8.7 面试可能问什么

问：为什么只检查字符串不够？  
答：symlink 可以让路径字符串看起来在 root_dir 内，但真实文件在外部；必须用 `realpath()` 校验真实路径。

### 6.9 线程池模块

#### 6.9.1 模块作用

固定 worker 线程池，用有界 FIFO 队列承接 client fd。

#### 6.9.2 关键文件

- `threadpool.c`
- `threadpool.h`

#### 6.9.3 关键数据结构

```c
typedef struct threadpool_task {
    int *client_fd_ptr;
    struct threadpool_task *next;
} threadpool_task_t;
```

```c
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
```

#### 6.9.4 关键函数

- `threadpool_init()`；
- `threadpool_submit()`；
- `worker()`；
- `threadpool_shutdown()`；
- `threadpool_get_processed_count()`。

#### 6.9.5 执行流程

```text
submit
-> lock
-> 检查 shutdown/queue full
-> task_create
-> 入队
-> signal cond
-> unlock

worker
-> lock
-> 队列空 cond_wait
-> 出队
-> unlock
-> handler(client_fd)
```

#### 6.9.6 设计取舍

优点：固定线程数，避免线程爆炸；队列上限明确；关闭时 join worker。  
局限：全局单例；`full_cond` 基本没用；fd 单独 malloc 不必要；`threadpool_init()` 无返回值，server 无法知道失败；配置允许 thread_num 到 1024，但线程池限制到 100。

#### 6.9.7 面试可能问什么

问：线程池队列如何保证安全？  
答：head/tail/task_count/shutdown 都在 mutex 下访问，worker 用 cond 等待任务。

## 7. 数据结构与数据流

### 数据从哪里来

- 配置数据来自 `config/server.conf`；
- 请求数据来自 TCP client socket；
- 静态文件来自 `cfg->root_dir`；
- CGI 输出来自子进程 stdout 管道；
- 日志数据来自请求处理结果和错误路径。

### 如何存储

- 配置：`server_config_t active_config`；
- 请求：栈数组 `method/url/path`；
- header：逐行 buffer，不构建完整 header map；
- POST body：不整体缓存，边读边写 CGI pipe；
- 线程池任务：链表节点；
- 日志：追加到 `logs/access.log`、`logs/error.log`。

### 如何传递和修改

```text
config/server.conf -> load_config -> active_config
client socket -> accept -> threadpool task -> worker -> accept_request
URL -> resolve_safe_path -> path 变成 realpath 后的绝对路径
GET query: url 中 ? 被改成 \0，query_string 指向后半段
CGI POST body: client recv -> cgi_input pipe -> CGI stdin
CGI output: CGI stdout -> cgi_output pipe -> client socket
```

### 如何输出

- HTTP 响应用 `send_all()`；
- 静态文件用 `fread()` + `send_all()`；
- CGI 输出由父进程读 pipe 后转发；
- 访问日志和错误日志写文件；
- benchmark 输出统计到 stdout。

### 持久化、缓存、同步、一致性

- 持久化：只有日志文件；
- 缓存：没有静态文件缓存、响应缓存、CGI 进程池；
- 状态同步：线程池队列用 mutex/cond，日志用 mutex；
- 并发访问：多个 worker 并发处理请求；
- 一致性风险：静态文件发送过程中被外部修改会导致 `Content-Length` 与实际内容不一致；配置启动后不会热更新；日志没有轮转，长期运行文件会增长。

### 协议格式

请求行：

```text
GET /path?x=1 HTTP/1.1\r\n
```

当前只解析 method 和 URL，不使用 HTTP version。

配置格式：

```conf
key=value
# comment
```

日志格式：

```text
[YYYY-MM-DD HH:MM:SS] 127.0.0.1 "GET /" 200
[YYYY-MM-DD HH:MM:SS] ERROR: File not found: /path
```

## 8. 错误处理与边界情况

| 场景 | 当前处理 | 代码位置 | 改进建议 |
|---|---|---|---|
| 参数非法 | 命令行端口非法回退 8080 | `main.c` | 用 `strtol()` 并打印提示 |
| 配置非法 | warning 并回退对应默认值 | `config.c` | 校验与线程池限制统一 |
| 文件不存在 | 404，记录错误日志 | `utils.c`、`request.c` | 更精细区分 stat errno |
| 权限不足 | `fopen` EACCES 返回 403 | `static_file.c` | `stat/realpath` 阶段也可区分权限 |
| 网络错误 | `get_line()` 返回 -1；`send_all()` 返回 -1 | `utils.c` | 统一记录 send/recv errno |
| 内存分配失败 | 线程池 task malloc 失败拒绝连接 | `threadpool.c` | 初始化失败返回错误给 server |
| 输入格式错误 | 请求行/header/Content-Length 返回 400/413 | `request.c` | 更严格解析 HTTP version |
| 资源释放失败 | 多数 close/fclose 不检查 | 多处 | 对日志/文件写失败加监控 |
| 并发冲突 | 线程池和日志有锁 | `threadpool.c`、`log.c` | 补压力测试和 TSAN |
| 数据损坏 | 无业务数据；日志可能写失败但不反馈 | `log.c` | 返回日志错误或统计指标 |
| 外部命令失败 | CGI 非 0 退出记录错误，返回 500 | `cgi.c` | 避免已发 200 后才发现失败 |

## 9. 安全性与鲁棒性分析

### 做得好的地方

- header 和 body 有大小限制；
- socket 读取有超时；
- `send_all()` 处理短写；
- `realpath()` 限制 root_dir，防 symlink 逃逸；
- 静态文件 EACCES 返回 403；
- 日志写入有 mutex 和 `localtime_r()`；
- CGI 子进程有 `alarm(5)`；
- CGI 非 0 退出会记录错误；
- 错误响应包含 Content-Length 和 Connection close；
- 配置缺失/非法不会直接崩溃。

### 薄弱点

- URL decode 不完整；
- CGI 没有 sandbox；
- `alarm()` 不能限制复杂子进程树；
- CGI 可能先返回 200 再发现脚本退出失败；
- 线程池初始化失败没有传回 server；
- 没有 TLS、鉴权、限流；
- 没有日志轮转；
- benchmark 不严谨；
- 没有 fuzz/sanitizer/CI。

### 面试保守表述

> 我做了基础安全边界：请求大小限制、socket 超时、`realpath()` 路径约束、日志锁和 CGI 超时。但它不是生产安全方案，CGI 没有 sandbox，HTTP parser 也不是完整协议实现。

## 10. 性能分析

### 复杂度

- 请求行解析 O(L)，L 为请求行长度；
- header 解析 O(H)，H 上限 8KB；
- 路径解析依赖文件系统；
- 静态文件发送 O(F)，F 为文件大小；
- 线程池入队/出队 O(1)；
- CGI 请求包含 fork/exec，开销明显更高。

### 空间复杂度

- 每连接使用固定大小栈 buffer；
- 静态文件分块读取，不整体加载；
- 任务队列最多 1000；
- worker 数由配置决定，但线程池代码最大 100。

### 瓶颈

- 阻塞 I/O；
- slow client 占 worker；
- CGI fork/exec；
- CGI 输出按字节读取；
- 静态文件没有 sendfile/cache；
- 日志每次 open/close；
- listen backlog 固定 5；
- `benchmark.c` 自身只 recv 一次，不适合严谨测试。

### 是否有并发优化

有：固定线程池替代每请求创建线程，并允许配置线程数。它主要提高资源可控性，不等同于高性能事件驱动服务器。

### benchmark

有 `benchmark.c`，但没有真实严谨 benchmark 数据。本文不虚构 QPS。补测应固定机器、端口、请求文件大小、线程数，跑多轮，记录 p50/p95/p99、错误率、CPU、内存和日志开销。

## 11. 测试体系分析

| 脚本 | 覆盖内容 | 输入 | 预期 | 判断方式 | 本次运行 |
|---|---|---|---|---|---|
| `tests/run_integration_tests.sh` | 静态文件、404、二进制、HEAD、CGI、OPTIONS、header 限制、路径逃逸、POST 截断、CGI 超时 | curl/nc/perl 构造请求 | 状态码/header/body 正确 | grep/cmp/curl exit | 通过 |
| `tests/log_test.sh` | 访问日志、错误日志、CGI 失败日志 | 200、404、date.cgi、失败 CGI | logs 文件存在且内容匹配 | grep -E | 通过 |
| `tests/config_test.sh` | 默认配置、自定义端口、自定义 root_dir、非法配置 | 临时 `config/server.conf` | 服务可启动且响应正确 | curl/grep | 本次并行干扰，结果不可靠 |
| `tests/mime_test.sh` | Content-Type 和错误页 Content-Length | html/css/js/png/unknown | header 匹配 | curl -D + grep | 未执行，沙箱外执行被系统限制 |

### 测试不足

- 没有 C 单元测试；
- 没有 CI；
- 没有 sanitizer；
- 没有 fuzz；
- 没有队列满测试；
- 没有 SIGTERM graceful shutdown 测试；
- 没有日志关闭配置测试；
- 没有 thread_num >100 配置不一致测试；
- 没有 CGI 已输出后失败的语义测试。

### 面试怎么讲测试

> 我有端到端测试脚本，能从 clean build 开始启动真实 server，用 curl/nc/perl 验证正常和异常 HTTP 请求。主测试和日志测试已经跑通；配置和 MIME 也有脚本，但本次执行环境限制导致没有可靠复跑。测试仍缺单元测试、CI 和 sanitizer。

## 12. 简历表达建议

### 偏互联网开发岗版本

- 基于 tinyhttpd 重构 C 语言轻量 HTTP Server，将请求解析、响应生成、静态文件、CGI、配置、日志、MIME 和线程池拆分为独立模块，提升代码可读性和可维护性。
- 使用 POSIX Socket + pthread 固定线程池实现主线程 accept、worker 处理请求的并发模型，支持 GET/HEAD/POST/OPTIONS、静态文件和 CGI，并通过 `realpath()` 限制文档根目录。
- 实现 `config/server.conf` 配置解析、线程安全访问/错误日志、扩展名 MIME 映射和 Shell 集成测试，覆盖静态资源、CGI、异常请求、日志等核心场景。

### 偏国企/银行/运营商信息科技岗版本

- 完成 C 语言 HTTP 服务端工程化改造，掌握 socket 监听、连接接收、请求解析、文件读取、响应发送和日志记录完整流程。
- 引入线程池、有界任务队列和配置文件，支持端口、线程数、根目录、日志开关等运行参数，并处理请求超时、错误响应和基础退出。
- 编写自动化测试脚本，验证静态页面、CGI、HEAD/OPTIONS、路径安全、请求大小限制、日志和配置行为。

### 偏测试开发/运维开发版本

- 为轻量 HTTP Server 建立 Shell 端到端测试体系，自动构建、启动服务、构造 curl/nc/perl 请求并断言状态码、响应头、body 和日志文件。
- 设计二进制文件、symlink 逃逸、POST 截断、CGI 超时、日志格式、配置回退、MIME 类型等测试场景，提升回归验证可复现性。
- 实现简易多线程 benchmark 工具，支持按 host、port、path、线程数和请求数发起 GET 压测，用于辅助观察成功率和吞吐趋势。

## 13. 面试讲法

### 13.1 30 秒项目介绍

这是一个基于 tinyhttpd 的 C 语言轻量 HTTP Server 改造项目。我把它拆成 server、request、response、static file、CGI、config、log、mime 和 threadpool 模块，支持 GET、HEAD、POST、OPTIONS、静态文件、CGI、配置文件、访问/错误日志和 MIME 响应头。它能通过集成测试验证主要功能，但定位是系统编程学习项目，不是生产级 Web Server。

### 13.2 1 分钟项目介绍

项目入口在 `main.c`，先加载 `config/server.conf`，没有配置就用默认端口 8080、4 个 worker 和 `./htdocs`。`server.c` 创建监听 socket，初始化线程池，主线程 accept 后把 client fd 放入队列。worker 设置 socket 读取超时，进入 `request.c` 解析请求。GET/HEAD 会通过 `resolve_safe_path()` 把 URL 映射到 root_dir，普通文件由 `static_file.c` 返回，可执行文件或 query 走 CGI；POST 只面向 CGI，会把 body 写到 CGI stdin。`response.c` 生成 header 和错误页，`mime.c` 决定 Content-Type，`log.c` 线程安全写访问和错误日志。项目有主集成测试、日志测试、配置测试和 MIME 测试脚本。

### 13.3 3 分钟项目介绍

我会从四层讲。第一层是启动配置：`main.c` 调 `load_config()` 解析 `key=value` 配置，支持端口、线程数、根目录、日志开关，命令行端口可以覆盖配置。第二层是网络和并发：`server.c` 用 socket/bind/listen/accept 建立 TCP server，初始化固定线程池，worker 用条件变量等待任务。第三层是请求处理：`request.c` 解析 method/url/header，限制 header 总大小和 POST body，按方法分发；OPTIONS 直接返回 Allow/CORS；GET/HEAD 走静态或 CGI；POST 走 CGI。第四层是响应和工程保障：静态文件用 `fread()` 分块发送并设置 MIME、Content-Length；CGI 用两个 pipe、fork、dup2、execl；日志模块用 mutex 写 access/error log；测试脚本覆盖核心正常和异常路径。最后我会主动说明限制：没有完整 HTTP/1.1、没有 TLS、CGI 没有 sandbox、benchmark 不严谨。

### 13.4 被问“你最熟悉哪部分”时怎么回答

> 我最熟悉请求处理主链路和 CGI。主链路从 `server_run()` 的 `accept()` 开始，进入线程池，worker 调 `handle_client()`，再由 `accept_request()` 解析请求、读取 header、做大小限制和方法分发。CGI 这块涉及 pipe、fork、dup2、环境变量、execl 和 waitpid，能比较完整地体现操作系统知识。

### 13.5 被问“这个项目是不是跟教程做的”时怎么回答

> 原始基础是 J. David Blackstone 的 tinyhttpd 教学项目，不是我从零原创的生产服务器。我做的是工程化理解和改造：模块化拆分、线程池、配置、日志、MIME、路径安全、二进制发送、CGI POST body、HEAD/OPTIONS、超时和测试脚本。我不会把它包装成完全原创或生产级项目。

## 14. 面试高频问题与推荐回答

### 1. 项目核心流程是什么？

推荐回答：`main()` 加载配置，`server_run()` 建立监听和线程池，主线程 `accept()` 后提交 client fd，worker 进入 `accept_request()` 解析请求并分发到静态文件或 CGI。  
代码依据：`main.c`、`server.c`、`request.c`。  
容易答崩：说 `main` 直接 accept。  
保守回答：当前 accept 在 `server.c`，不是 README 流程图里的 main。

### 2. 配置文件怎么处理？

推荐回答：`load_config()` 先写默认值，再读 `config/server.conf` 的 key=value，非法值 warning 并回退默认，最后 `set_server_config()` 保存。  
代码依据：`config.c`。  
容易答崩：说配置缺失会启动失败。  
保守回答：缺失配置不失败，但配置能力很简单。

### 3. 命令行端口和配置端口谁优先？

推荐回答：配置先加载，命令行端口后解析并覆盖 `cfg.port`。  
代码依据：`main.c`。  
容易答崩：忽略覆盖顺序。  
保守回答：只有端口支持命令行覆盖。

### 4. 为什么用线程池？

推荐回答：固定 worker 控制并发资源，避免每请求创建线程；队列满时拒绝连接。  
代码依据：`threadpool.c`。  
容易答崩：声称一定提升性能。  
保守回答：主要是资源可控，性能需测试。

### 5. 线程池如何同步？

推荐回答：队列 head/tail/task_count/shutdown 由 mutex 保护，worker 用 cond_wait 等待任务。  
代码依据：`worker()`、`threadpool_submit()`。  
容易答崩：忘记 cond_wait 会释放锁。  
保守回答：队列安全，但线程池初始化反馈还弱。

### 6. 队列满怎么办？

推荐回答：`threadpool_submit()` 返回 -1，`server_run()` 关闭 client fd。  
代码依据：`server.c`、`threadpool.c`。  
容易答崩：说会阻塞等待。  
保守回答：当前是直接拒绝策略。

### 7. GET 如何映射到文件？

推荐回答：分离 query 后调用 `resolve_safe_path()`，用配置 `root_dir` 拼 URL，目录补 index.html，stat/realpath 后确认真实路径仍在 docroot 内。  
代码依据：`request.c`、`utils.c`。  
容易答崩：只说字符串拼接。  
保守回答：有基础路径安全，但 URL decode 不完整。

### 8. 如何防 symlink 逃逸？

推荐回答：docroot 和目标路径都 `realpath()`，再比较目标真实路径前缀是否属于 docroot。  
代码依据：`utils.c:resolve_safe_path()`。  
容易答崩：只提 `..`。  
保守回答：能防普通 symlink 外指，编码绕过还需补。

### 9. 静态文件如何支持二进制？

推荐回答：`cat()` 用 `fread()` 得到实际字节数，再用 `send_all()`，不使用 `strlen()`。  
代码依据：`static_file.c`。  
容易答崩：把文件当 C 字符串。  
保守回答：测试覆盖 NUL 字节文件。

### 10. MIME 如何实现？

推荐回答：`mime.c:get_mime_type()` 按扩展名硬编码映射，未知默认 `application/octet-stream`。  
代码依据：`mime.c`。  
容易答崩：说完整 MIME 数据库。  
保守回答：基础常见类型映射。

### 11. 错误页是否有 Content-Length？

推荐回答：有，`send_error_page()` 先拼 body，计算 `strlen(body)`，再发 Content-Length。  
代码依据：`response.c`。  
容易答崩：沿用旧版本说没有。  
保守回答：当前错误页有长度，但 body buffer 固定。

### 12. HEAD 如何实现？

推荐回答：静态文件发 header 后不调用 `cat()`；CGI HEAD 只转发 CGI header，读到 `\n\n` 或 `\r\n\r\n` 后丢弃 body。  
代码依据：`static_file.c`、`cgi.c`。  
容易答崩：说和 GET 一样。  
保守回答：基础实现，不覆盖所有 CGI 异常格式。

### 13. POST 如何处理？

推荐回答：POST 被视为 CGI，请求必须有合法 Content-Length，body 通过 pipe 写入 CGI stdin。  
代码依据：`request.c`、`cgi.c`。  
容易答崩：说支持任意 POST 路由。  
保守回答：当前 POST 主要服务 CGI。

### 14. CGI 如何执行？

推荐回答：两个 pipe，fork 子进程，子进程 dup2 stdin/stdout，设置环境变量，alarm 后 execl；父进程转发输入输出。  
代码依据：`cgi.c`。  
容易答崩：说同进程调用脚本。  
保守回答：基础 CGI，无 sandbox。

### 15. CGI 失败如何处理？

推荐回答：pipe/fork/read/write 失败返回 500；子进程非 0 退出会记录错误并返回 500，但如果已经发出 200，响应语义可能不准。  
代码依据：`cgi.c`。  
容易答崩：说所有 CGI 失败都正确响应。  
保守回答：失败检测有进步，但语义仍不完美。

### 16. 日志如何保证线程安全？

推荐回答：`log.c` 用静态 `pthread_mutex_t log_mutex` 包住 mkdir/fopen/fprintf/fclose。  
代码依据：`log.c`。  
容易答崩：说文件 append 自然安全。  
保守回答：日志写入有锁，但没有轮转。

### 17. 日志能关闭吗？

推荐回答：能，配置 `enable_access_log` 和 `enable_error_log` 控制。  
代码依据：`log.c`、`config.c`。  
容易答崩：忽略配置开关。  
保守回答：只在启动时生效，无热更新。

### 18. 支持完整 HTTP/1.1 吗？

推荐回答：不支持。响应是 HTTP/1.0，连接处理完关闭，没有 keep-alive/chunked。  
代码依据：`response.c`、`cgi.c`。  
容易答崩：curl 发 HTTP/1.1 就说支持。  
保守回答：能处理常见请求格式，不是完整协议实现。

### 19. `SO_RCVTIMEO` 作用是什么？

推荐回答：给 client socket 的 recv 设置 5 秒超时，避免慢客户端永久占 worker。  
代码依据：`server.c:handle_client()`。  
容易答崩：说它控制整个请求生命周期。  
保守回答：只是接收超时。

### 20. 为什么忽略 SIGPIPE？

推荐回答：客户端断开后服务端 send 可能触发 SIGPIPE，默认杀进程；忽略后由 send 返回错误。  
代码依据：`server.c`。  
容易答崩：不知道默认行为。  
保守回答：基础鲁棒性处理。

### 21. 服务如何退出？

推荐回答：SIGINT/SIGTERM 设置 `running=0`，accept 被 EINTR 打断后退出循环，调用 `threadpool_shutdown()`。  
代码依据：`server.c`。  
容易答崩：引用 README 旧 limitation。  
保守回答：有基础退出，不是完整 graceful drain。

### 22. 访问日志格式是什么？

推荐回答：`[time] client_ip "METHOD URL" status`。  
代码依据：`log.c`、`tests/log_test.sh`。  
容易答崩：说不出字段。  
保守回答：普通文本日志，不是结构化日志。

### 23. 错误日志记录哪些？

推荐回答：404 文件未找到、403 权限/路径问题、500 内部错误和 CGI 失败。  
代码依据：`request.c:log_request_error()`、`cgi.c`。  
容易答崩：以为所有错误都有详细 errno。  
保守回答：记录关键错误，细节还可补充。

### 24. benchmark 有什么局限？

推荐回答：只发 GET，只 recv 一次，没有超时、分位数、多轮统计，不代表严谨性能。  
代码依据：`benchmark.c`。  
容易答崩：编造 QPS。  
保守回答：只是辅助工具。

### 25. 测试体系怎么讲？

推荐回答：有主集成测试、日志测试、配置测试、MIME 测试脚本，能启动真实 server 验证功能和异常。  
代码依据：`tests/*.sh`。  
容易答崩：只说手测。  
保守回答：脚本测试较多，但无 CI/单元测试/sanitizer。

### 26. 配置和线程池有什么不一致？

推荐回答：`config.c` 允许 `thread_num` 到 1024，但 `threadpool_init()` 限制最大 100。  
代码依据：`config.c:parse_long(..., 1, 1024)`、`threadpool.c`。  
容易答崩：说配置都完全生效。  
保守回答：这是需要修的边界不一致。

### 27. 如何支持 epoll？

推荐回答：需要非阻塞 socket 和连接状态机，重写 `get_line()`、静态发送、CGI pipe 读取，不是局部替换。  
代码依据：当前阻塞 I/O。  
容易答崩：说加 epoll 即可。  
保守回答：架构级改造。

### 28. 如何支持 chunked？

推荐回答：需要解析 `Transfer-Encoding: chunked`，按 chunk size 读取 body，当前只支持 Content-Length。  
代码依据：`read_headers()`。  
容易答崩：认为 POST 都支持。  
保守回答：当前不支持 chunked。

### 29. 你最想重构哪里？

推荐回答：先统一配置限制和线程池限制，再把 HTTP parser 抽成可单测模块，补 CI/sanitizer。  
代码依据：当前模块边界和测试缺口。  
容易答崩：先谈复杂架构。  
保守回答：先收敛工程质量。

### 30. 这个项目最大风险是什么？

推荐回答：容易被误认为生产 Web Server；真实风险是协议不完整、CGI 无 sandbox、URL decode 不完整、性能未严谨测试。  
代码依据：README limitations 和源码。  
容易答崩：夸大能力。  
保守回答：这是教学项目工程化改造。

## 15. 最容易被追问的风险点

| 风险点 | 为什么容易被问 | 代码在哪里 | 我应该怎么补 | 面试不要乱说什么 | 推荐保守回答 |
|---|---|---|---|---|---|
| HTTP/1.1 不完整 | Web Server 高频 | `request.c`、`response.c` | keep-alive/chunked/range | 不说完整支持 | “HTTP/1.0 风格基础实现。” |
| CGI 无 sandbox | 安全高频 | `cgi.c` | chroot/seccomp/资源限制 | 不说安全隔离 | “只执行受信任脚本。” |
| CGI 失败后可能已发 200 | 细节追问 | `cgi.c` | exec/退出状态提前回传 | 不说错误语义完美 | “有失败记录，但响应语义还可改。” |
| URL decode 不完整 | 路径安全常问 | `utils.c` | percent-decode + 测试 | 不说彻底防攻击 | “能防普通 `..` 和 symlink。” |
| thread_num 限制不一致 | 配置边界 | `config.c`、`threadpool.c` | 统一上限或返回错误 | 不说配置完全可靠 | “这是明确改进点。” |
| 线程池初始化无返回 | 鲁棒性 | `threadpool.c`、`server.c` | 改为返回 int | 不说所有失败可感知 | “submit 失败能处理，init 反馈弱。” |
| 日志无轮转 | 运维问题 | `log.c` | rotation/大小限制 | 不说生产日志系统 | “线程安全追加，无轮转。” |
| benchmark 不严谨 | 性能追问 | `benchmark.c` | 超时/完整读/分位数 | 不编 QPS | “辅助压测。” |
| 测试无 CI | 工程化 | `tests/` | GitHub Actions | 不说自动化完整 | “有脚本，缺 CI。” |
| `simpleclient.c` 无关 | 文件职责 | `simpleclient.c` | 删除或标注 legacy | 不说是 HTTP client | “历史 TCP demo。” |

## 16. 后续改进方向

| 方向 | 为什么要改 | 如何改 | 涉及文件 | 难度 | 简历价值 | 面试扩展 |
|---|---|---|---|---|---|---|
| 统一配置和线程池限制 | `thread_num` 上限不一致 | 配置上限改 100 或线程池支持更高并返回错误 | `config.c`、`threadpool.c` | 低 | 中 | 边界严谨 |
| URL decode | 路径安全不完整 | 先 decode 再 `realpath()`，补编码测试 | `utils.c`、`tests/` | 中 | 高 | 安全 |
| CGI 错误语义 | 可能已发 200 后失败 | 子进程 exec 失败通过 pipe 回传；先确认状态再发响应 | `cgi.c` | 中 | 高 | OS 细节 |
| HTTP parser 单元测试 | 当前只有端到端 | 抽 parser 函数，写 test runner | `request.c` | 中 | 高 | 测试分层 |
| CI + sanitizer | 防回归 | GitHub Actions 跑 make/tests/ASan/UBSan | `.github/`、Makefile | 中 | 高 | 工程化 |
| 日志轮转 | 日志无限增长 | 按大小 rename 或按日期切分 | `log.c` | 中 | 中 | 运维 |
| 配置热加载 | 当前只启动时读取 | SIGHUP 触发 reload，谨慎同步 | `config.c`、`server.c` | 中高 | 中 | 服务治理 |
| sendfile 优化 | 降低静态文件拷贝 | `sendfile()` + fallback | `static_file.c` | 中 | 中 | 性能 |
| benchmark 增强 | 当前不严谨 | 完整读取、超时、p95/p99、多轮 CSV | `benchmark.c` | 中 | 中 | 测开 |
| graceful shutdown | 活跃请求 drain 不完整 | 停止接入、等待活跃计数、超时强杀 | `server.c`、`threadpool.c` | 中高 | 中 | 生命周期 |

## 17. 面试前 30 分钟速背提纲

### 项目一句话

基于 tinyhttpd 的 C 语言轻量 HTTP Server 工程化改造，模块化实现 socket、线程池、HTTP 解析、静态文件、CGI、配置、日志、MIME 和测试。

### 核心模块

- `main.c`：加载配置、命令行覆盖；
- `server.c`：socket、信号、线程池、accept；
- `request.c`：请求解析和分发；
- `response.c/mime.c`：响应头、错误页、Content-Type；
- `static_file.c`：文件读取发送；
- `cgi.c`：pipe/fork/exec；
- `config.c`：配置解析；
- `log.c`：线程安全日志；
- `threadpool.c`：固定线程池。

### 核心流程

```text
main
-> load_config
-> set_server_config
-> server_run
-> startup
-> threadpool_init
-> accept
-> threadpool_submit
-> worker
-> handle_client
-> accept_request
-> parse_request_line/read_headers
-> resolve_safe_path
-> serve_file 或 execute_cgi
-> log_access/log_error
-> close
```

### 3 个最大亮点

- 模块化 + 配置/日志/MIME；
- 固定线程池 + 有界队列；
- `realpath()` 路径安全 + 多脚本端到端测试。

### 3 个最大风险点

- 非完整 HTTP/1.1；
- CGI 无 sandbox 且失败语义仍可改；
- 性能和测试体系不是生产级。

### 5 个必背问题

1. 配置如何加载并覆盖端口？
2. GET `/` 的完整函数链是什么？
3. CGI 两个 pipe 分别做什么？
4. `realpath()` 如何防路径逃逸？
5. 日志如何做到线程安全？

### 5 个保守回答

- “这是教学项目工程化改造，不是生产级 Web Server。”
- “支持基础 HTTP 方法，不完整支持 HTTP/1.1。”
- “CGI 没有 sandbox，只适合受信任脚本。”
- “路径安全做了基础 confinement，但 URL decode 还需补。”
- “benchmark 只是辅助工具，没有严谨 QPS 结论。”

### 不能夸大的地方

- 不说生产可用；
- 不说高性能；
- 不说完整 HTTP/1.1；
- 不说 CGI 安全隔离；
- 不说有完整 CI；
- 不编造 benchmark 数据；
- 不说配置/日志已经工业级。

## 18. 学习路线

### 第一天：跑起来，理解启动链路

命令：

```bash
make clean && make
./httpd 18080
curl --noproxy '*' -i http://127.0.0.1:18080/
curl --noproxy '*' -i http://127.0.0.1:18080/date.cgi
```

读文件：

- `Makefile`
- `main.c`
- `config.c/h`
- `server.c/h`
- `htdocs/index.html`
- `htdocs/date.cgi`

要能回答：

- 默认配置是什么？
- 命令行端口如何覆盖配置？
- `accept()` 在哪个函数？
- 线程池在哪里初始化？
- 静态根目录从哪里来？

### 第二天：理解请求解析、响应和静态文件

命令：

```bash
tests/run_integration_tests.sh
```

读文件：

- `request.c/h`
- `utils.c/h`
- `response.c/h`
- `mime.c/h`
- `static_file.c/h`

要能回答：

- 请求行怎么解析？
- header 大小限制是多少？
- `resolve_safe_path()` 如何工作？
- MIME 如何判断？
- HEAD 和 GET 有什么区别？

### 第三天：理解 CGI、日志、线程池和测试

命令：

```bash
tests/log_test.sh
tests/config_test.sh
tests/mime_test.sh
./benchmark 127.0.0.1 8080 / 5 20
```

读文件：

- `cgi.c/h`
- `log.c/h`
- `threadpool.c/h`
- `benchmark.c`
- `tests/*.sh`

要能回答：

- CGI 为什么需要两个 pipe？
- POST body 如何传给 CGI？
- 日志为什么要 mutex？
- worker 如何等待任务？
- benchmark 有哪些局限？

### 最后如何自测

不看文档回答：

1. 从 `./httpd 18080` 到监听成功发生了什么？
2. 从 `curl /` 到返回 HTML 的函数链是什么？
3. 配置文件缺失和非法值如何处理？
4. `realpath()` 如何防 symlink 逃逸？
5. POST 截断时如何返回 400？
6. CGI 失败为什么可能仍有响应语义问题？
7. 访问日志和错误日志分别记录什么？
8. 哪些测试已经覆盖，哪些还缺？
9. 这个项目不能夸大的地方有哪些？
10. 下一步最值得改什么？

掌握原则：先讲真实函数链，再讲实现边界，最后讲改进方向。这个项目的价值在于能把 C 网络编程、操作系统进程通信、线程同步和工程测试串起来，而不是把它包装成生产级 Web Server。
