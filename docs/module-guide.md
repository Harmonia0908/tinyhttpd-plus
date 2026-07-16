# Tinyhttpd 模块维护指南

## 目录速查

```text
Tinyhttpd/
├── include/                 公共头文件和跨翻译单元契约
├── src/
│   ├── app/                 入口与进程级配置
│   ├── server/              listener、信号和连接调度
│   ├── http/                HTTP 解析、响应、路由、资源与静态文件
│   ├── cgi/                 CGI 环境、管道、子进程和转发
│   ├── concurrency/         固定线程池和 FIFO 队列
│   ├── net/                 socket 字节 I/O
│   └── common/              fd/fork 生命周期和日志
├── tools/                   独立客户端与 benchmark
├── tests/                   C 单元/组件测试和 shell 集成测试
├── htdocs/                  默认 document root 与 CGI 示例
├── config/                  运行时本地配置
├── scripts/                 统一构建/测试入口
├── docs/                    维护者文档
├── Makefile                 构建图、profile 和测试目标
└── .github/workflows/ci.yml 本地命令的 Ubuntu CI 映射
```

`build/`、根目录下的 `httpd`/`client`/`benchmark`、`logs/` 和调试符号目录都是运行或
构建产物，不是模块源代码。[`Makefile`](../Makefile) 把 profile 产物放在
`build/<profile>/`，同时复制根目录兼容入口。

当前行为以 [behavior-baseline.md](behavior-baseline.md) 为验收基线，架构和运行入口
分别以本文、[architecture.md](architecture.md) 和
[code-walkthrough.md](code-walkthrough.md) 为准。`LEARNING_PLAN.md` 与
`STUDY_GUIDE.md` 已明确冻结为目录重组前的历史学习资料，不应用其中旧路径定位当前
源码。被 `.gitignore` 排除的个人笔记（包括 `project_interview_notes.md`）不属于当前
维护者文档集合，也不能作为现行目录、接口或调用关系的依据。

## app

### 文件和职责

- [`src/app/main.c`](../src/app/main.c)：`main()`、私有 `parse_port()`；只负责配置、
  命令行端口覆盖、发布配置和启动 Server。
- [`src/app/config.c`](../src/app/config.c)：默认值、`key=value` 文件解析和进程级
  `active_config`。
- [`include/config.h`](../include/config.h)：`server_config_t`、`CONFIG_PATH` 和配置
  API。

### 公共接口

```c
void init_default_config(server_config_t *cfg);
int load_config(const char *path, server_config_t *cfg);
void set_server_config(const server_config_t *cfg);
const server_config_t *get_server_config(void);
```

### 维护规则

- 新增运行时配置项通常要同时修改 `include/config.h`、`src/app/config.c`、
  `tests/config_test.sh`、README/架构文档；真正使用该配置的模块也要加入最小测试。
- 命令行参数只属于 `main.c`。当前公开运行形式只有 `./httpd [port]`，不要在整理中
  改成新的 CLI 框架或改变无效端口回退到 8080 的行为。
- `main.c` 不应 include HTTP、CGI、线程同步或 socket 实现。
- `active_config` 在 Server 启动前写入、之后只读；若要支持动态重载，必须作为新功能
  重新设计同步和生命周期，不能直接在 worker 运行期间修改它。

## server

### 文件和职责

- [`src/server/server.c`](../src/server/server.c)：`startup()` 创建 listener；
  `server_run()` 管理信号、`poll()`/accept、线程池和 shutdown；`handle_client()` 设置
  socket timeout 并进入请求层。
- [`include/server.h`](../include/server.h)：当前三个可见入口。

### 公共接口

```c
void handle_client(int client);
int startup(uint16_t *port);
int server_run(uint16_t port);
```

### 维护规则

- listener/accept/提交策略变化应同时查看 `src/common/fd_lifecycle.c`、
  `src/concurrency/threadpool.c`、`tests/run_integration_tests.sh` 和
  [`THREADPOOL_INTEGRATION.md`](../THREADPOOL_INTEGRATION.md)。
- Server 只调度连接。HTTP 方法、header、路径、MIME 和 CGI 条件不得进入
  `server.c`。
- 成功提交后，client fd 的关闭责任属于 handler 路径；提交失败由 `server_run()`
  关闭。修改这条契约时必须对每个错误分支做 fd 泄漏检查。
- shutdown 当前会排空已提交请求；不要把它悄悄改为丢弃队列或强停 worker。

## http

### 文件和职责

| 文件 | 核心符号 | 职责 |
|---|---|---|
| [`parser.c`](../src/http/parser.c) | `http_parse_request_line()`、`http_parse_headers()` | 对显式 buffer/length 做纯解析，不读取 socket。 |
| [`request.c`](../src/http/request.c) | `accept_request()`、`parse_request_line()`、`read_headers()` | fd 适配、请求阶段编排、方法分派和访问日志。 |
| [`response.c`](../src/http/response.c) | `http_build_static_headers()`、`http_build_error_response()`、`http_build_options_response()` | 纯响应字节构造。 |
| [`response_writer.c`](../src/http/response_writer.c) | `bad_request()` 等 | 旧 fd 响应 API 到 builder + net 的兼容适配。 |
| [`resource.c`](../src/http/resource.c) | `resolve_safe_path()`、`is_path_traversal()` | document root 解析、目录 index、realpath confinement；当前也发送错误响应。 |
| [`static_file.c`](../src/http/static_file.c) | `handle_static_file()`、`serve_file()`、`cat()` | 静态 header、文件分块发送和资源关闭。 |
| [`mime.c`](../src/http/mime.c) | `get_mime_type()` | 文件扩展名到 Content-Type 映射。 |

### 公共接口

- [`include/http_parser.h`](../include/http_parser.h)：纯解析接口和 1 MiB/8 KiB/1 KiB
  边界常量。
- [`include/http_response.h`](../include/http_response.h)：
  `http_response_buffer_t` 与三个 builder。
- [`include/request.h`](../include/request.h)：连接请求入口和两个 fd 解析适配器。
- [`include/response.h`](../include/response.h)：旧 fd 风格响应兼容接口。
- [`include/static_file.h`](../include/static_file.h)：静态文件处理入口。
- [`include/mime.h`](../include/mime.h)：MIME 查询。
- [`include/http_protocol.h`](../include/http_protocol.h)：固定 `SERVER_STRING`。
- [`include/utils.h`](../include/utils.h)：当前仍暴露资源定位及多个转引头；属于兼容
  umbrella，不宜继续扩展。

### 维护规则

- 修改请求语法或限制时，应一起修改 `parser.c`、`http_parser.h`、
  `tests/http_parser_test.c`，并检查 fd 行为测试 `tests/request_test.c` 和集成测试。
- 修改响应文本、header 顺序、大小写或 CRLF 时，应一起修改 `response.c`、
  `tests/http_response_test.c` 和对应集成断言。响应字节是已锁定行为，不是普通格式化。
- 修改静态文件行为时，联动 `resource.c`、`static_file.c`、`mime.c` 及 symlink、
  binary、HEAD、Content-Length、MIME 集成测试。
- 纯 `parser.c`/`response.c` 禁止 include `net_io.h` 或调用 `recv()`/`send()`；socket
  适配应停留在 `request.c`/`response_writer.c`/具体 handler。
- `accept_request()` 当前承担路由编排。新增 HTTP 方法先明确它是静态、CGI 还是独立
  handler，再加入纯解析测试和端到端测试；不要把方法判断散落到 net/server。

## cgi

### 文件和职责

- [`src/cgi/cgi.c`](../src/cgi/cgi.c)：CGI 输入验证、环境构造、pipe/fork/exec、POST
  body、HEAD header 截断、输出转发、异常终止和 `waitpid()`。
- [`include/cgi.h`](../include/cgi.h)：`handle_cgi()` 与较低层的 `execute_cgi()`。

### 公共接口

```c
int handle_cgi(int client, const char *path, const char *method,
               const char *query_string, int is_head, int content_length);
int execute_cgi(int client, const char *path, const char *method,
                const char *query_string, int is_head,
                int req_content_length);
```

### 维护规则

- 修改 fork/pipe/fd 时必须同时检查 `src/common/fd_lifecycle.c`、
  `tests/cgi_test.c`、`tests/cgi_fd_probe.c` 和 timeout/shutdown 集成场景。
- 修改 CGI header/body 或退出语义时必须覆盖 GET、POST、HEAD、空输出、非零退出和
  截断 POST。当前“首字节后先发 200”的行为不可顺手改变。
- 子进程 fork 后到 exec 前只能做审慎的系统调用；不要加入日志、malloc、stdio 或
  pthread 锁操作。
- CGI 不是 sandbox。不要在本模块假定脚本不可信隔离已经存在。

## concurrency

### 文件和职责

- [`src/concurrency/threadpool.c`](../src/concurrency/threadpool.c)：私有
  `threadpool_task_t`、`threadpool_t`、worker loop 和进程级 `pool`。
- [`include/threadpool.h`](../include/threadpool.h)：任务 handler、队列默认大小、线程
  上限和生命周期 API。

### 公共接口

```c
typedef void (*task_handler)(int client_fd);
int threadpool_init(int thread_count, int max_queue_size, task_handler handler);
int threadpool_submit(int client_fd);
void threadpool_shutdown(void);
int threadpool_get_processed_count(void);
```

### 维护规则

- 本模块不知道 HTTP 或 CGI，只把一个 `int client_fd` 交给 handler。
- 修改队列、shutdown、计数或重复初始化行为时更新
  `tests/threadpool_test.c`，并运行 busy-worker shutdown 集成测试。
- `lifecycle_mutex` 保护 init/submit/shutdown 的生命周期，`pool.mutex` 保护队列；
  调整锁顺序前先画出所有路径，避免与 handler 长时间持锁。
- 当前 worker 在 handler 返回后释放任务并增加计数；它不会关闭 fd。

## net

### 文件和职责

- [`src/net/io.c`](../src/net/io.c)：`net_read_line()`、`net_write_all()`；另保留
  `get_line()`、`send_all()` 兼容别名。
- [`include/net_io.h`](../include/net_io.h)：全部网络字节 I/O 接口。

### 公共接口

```c
int net_read_line(int fd, char *buffer, size_t capacity);
int net_write_all(int fd, const void *data, size_t length);
int get_line(int sock, char *buf, int size);       /* compatibility */
int send_all(int client, const void *data, size_t len); /* compatibility */
```

### 维护规则

- net 只返回字节数/成功失败，不构造 HTTP 状态、记录 URL 或决定连接路由。
- 改变 CR/LF 规范化会直接影响解析边界，需联动 `request_test.c` 和 parser/集成测试。
- `net_write_all()` 的语义是完整写入或失败；不要退化为单次 `send()`。

## common

### 文件和职责

- [`src/common/fd_lifecycle.c`](../src/common/fd_lifecycle.c)：fd flags、受锁保护的
  accept/open/pipe+fork 和致命错误入口。
- [`src/common/log.c`](../src/common/log.c)：日志目录、时间戳、线程安全追加。
- [`include/fd_lifecycle.h`](../include/fd_lifecycle.h)、
  [`include/log.h`](../include/log.h)：对应接口。

### 维护规则

- “common” 不是放置任意代码的理由。只有多个模块共享、语义稳定且不属于更具体
  模块的能力才应进入这里。
- fd 创建接口必须维护 close-on-exec 和 `fork_fd_mutex` 约束；不要绕过
  `open_cloexec()` 打开会与 CGI fork 并发的 Server 内部文件。
- `log.c -> config.c` 是当前已知反向依赖。新 common 工具不得继续读取 app 全局状态。
- 修改日志格式要同步 `tests/log_test.sh`；日志开关和路径也涉及配置测试。

## tools、htdocs、tests 与构建文件

- [`tools/simpleclient.c`](../tools/simpleclient.c) 是独立 TCP demo；
  [`tools/benchmark.c`](../tools/benchmark.c) 只发 GET，并以响应含 `200 OK` 判断成功。
  两者不是 Server 公共 API。
- [`htdocs/`](../htdocs) 是默认 document root，`.cgi` 文件需要保持可执行位。修改
  示例页面可能影响浏览器手工检查或集成 fixture 复制结果。
- [`tests/http_parser_test.c`](../tests/http_parser_test.c) 与
  [`tests/http_response_test.c`](../tests/http_response_test.c) 是无 socket 的纯逻辑
  测试；`request_test.c` 使用 `socketpair()`；`cgi_test.c` 使用并发本地进程；四个
  shell suite 启动本地 TCP Server。
- [`Makefile`](../Makefile) 是唯一构建系统，没有 CMake/CTest。新增 `.c` 文件必须
  放入相应 `*_SRCS`；有独立测试时还要加入链接源、`UNIT_TESTS` 和执行 recipe。
- [`scripts/build.sh`](../scripts/build.sh) 与 [`scripts/test.sh`](../scripts/test.sh)
  是 CI 和本地一致入口；[`ci.yml`](../.github/workflows/ci.yml) 不应复制另一套命令。

## 常见联动修改矩阵

| 需求 | 通常一起修改 | 必须验证 |
|---|---|---|
| 新增/修改配置项 | `config.h`、`config.c`、使用模块、`config_test.sh`、文档 | 缺失配置、合法值、非法值、CLI 优先级 |
| 修改 HTTP 请求规则 | `http_parser.h`、`parser.c`、`request.c`（仅适配需要时）、parser/request tests | 精确状态码、边界长度、TCP 集成 |
| 修改响应 wire bytes | `http_response.h`、`response.c`、可能的 writer/handler、response test | 精确 CRLF、header 顺序、Content-Length |
| 新增 MIME | `mime.c`、`mime_test.sh`、README/模块文档 | 已知/未知扩展和错误响应 |
| 静态路径安全 | `resource.c`、可能的 fd lifecycle、integration | `..`、symlink escape、目录 index、403/404 |
| CGI 生命周期 | `cgi.c`、`fd_lifecycle.c`、CGI tests、integration | GET/POST/HEAD、timeout、fd 继承、shutdown |
| 线程池策略 | `threadpool.h/.c`、server、threadpool test、integration | 队列满、排空、重复 shutdown、busy worker |
| 日志格式/开关 | `log.h/.c`、config、`log_test.sh` | 并发行完整性、200/404/CGI 失败、禁用开关 |
| 新增模块源文件 | 对应 `include/`/`src/`、Makefile | default/debug/release、unit/full test |

## 新功能应该放在哪里

- 新 HTTP 语法或 header 值解析：`src/http/parser.c`，保持纯 buffer API。
- 新响应格式：`src/http/response.c`；具体发送仍由 handler/net 适配层负责。
- 新静态资源策略：`resource.c` 或 `static_file.c`，按“定位”与“读取发送”区分。
- 新 CGI 环境变量或进程交互：`src/cgi/cgi.c`，同时扩充 CGI 集成测试。
- 新连接接受/拒绝策略：`src/server/server.c`；通用任务队列能力才放 concurrency。
- 新的可靠 socket 原语：`src/net/io.c`，不得携带 HTTP 含义。
- 新的进程级启动选项：app/config；不要让下层自行解析配置文件。
- 真正跨模块的 fd/日志基础能力：common；否则优先放进语义最具体的模块。

## 禁止新增的依赖方向

- `net -> http/cgi/server/config/log`
- `concurrency -> http/cgi/server/config`
- 纯 `http/parser` 或 `http/response -> net/cgi/server/config/log`
- `server -> http/parser` 的具体语法、`http/response` 的 wire 细节、CGI pipe 实现
- `app/main -> request/static/cgi/net`
- 通过 `utils.h` 继续制造新的隐式转引依赖
- 生产模块依赖 `tests/` 或 tools

当前已经存在的 `http/request -> cgi -> http/response compatibility` 与
`common/log -> app/config` 应视为待处理例外，而不是新增依赖的先例。

## 不建议拆分或抽象的代码

- [`src/http/mime.c`](../src/http/mime.c) 只有一个稳定映射函数，继续拆成 registry、
  provider 或插件没有实际收益。
- [`src/net/io.c`](../src/net/io.c) 的两个核心原语很小；为测试引入虚函数表、transport
  interface 或工厂会增加理解成本。
- [`src/app/main.c`](../src/app/main.c) 已经只做装配，不需要 application class、DI
  container 或命令框架。
- [`threadpool_task_t`](../src/concurrency/threadpool.c) 与私有 FIFO 队列只服务一个
  handler 类型，不需要通用任务对象层级。
- 固定容量 [`http_response_buffer_t`](../include/http_response.h) 与三个 builder 在
  当前响应规模下足够直接；除非真实响应超过容量，不要先引入动态 response object。
- `handle_static_file()` 虽然只是 `serve_file()` 的薄入口，但它表达路由层到静态模块
  的命名边界；单独删除价值很低，且会扩大无关 diff。
