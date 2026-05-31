# Tinyhttpd Engineering Retrofit

这是一个基于 J. David Blackstone 的 tinyhttpd 教学项目改造的轻量级 C HTTP Server。项目目标是保留 tinyhttpd 适合学习的代码规模，同时补充更接近工程实践的模块拆分、线程池、配置文件、日志、MIME、基础安全边界、CGI 处理和可复现测试。

它不是生产级 Web Server，也不试图完整实现 HTTP/1.1、TLS、CGI sandbox 或高性能事件驱动架构。

## 已实现功能

### HTTP 方法

- `GET`：读取静态文件或执行 CGI。
- `HEAD`：返回响应头，不返回响应 body。
- `POST`：面向 CGI，读取请求体并通过 stdin 传给 CGI。
- `OPTIONS`：返回 `Allow` 和基础 CORS 相关 header。

### 静态文件服务

- 从配置项 `root_dir` 指定的目录读取静态文件，默认是 `./htdocs`。
- 目录请求会尝试补 `index.html`。
- 根据文件扩展名返回 `Content-Type`。
- 返回 `Content-Length` 和 `Connection: close`。
- 使用 `fread()` 分块读取文件，并通过 `send_all()` 发送，避免因 `NUL` 字节截断二进制内容。
- `HEAD` 静态文件响应只返回 header，不发送 body。

### CGI

- 支持 CGI `GET` / `HEAD` / `POST`。
- `GET` / `HEAD` 使用 `QUERY_STRING` 环境变量传递查询参数。
- `POST` 使用 `CONTENT_LENGTH` 环境变量，并将请求体写入 CGI stdin。
- 通过 `pipe + fork + dup2 + execl` 执行 CGI。
- CGI 子进程设置 5 秒 wall-clock 超时，避免单个 CGI 永久占用 worker。
- CGI `HEAD` 只转发 CGI 输出 header，识别 `\n\n` 和 `\r\n\r\n` 作为 header 结束。
- CGI 非 0 退出会记录错误日志并返回失败状态给调用链；注意如果 CGI 已经输出内容，客户端可能已经收到 `200 OK`，这仍不是完整 CGI 错误语义。

### 线程池与任务队列

- 主线程负责 `accept()`。
- 连接 fd 提交到线程池 FIFO 任务队列。
- worker 线程从队列取出 fd 并调用请求处理函数。
- 队列满时拒绝新连接，由主线程关闭 socket。
- worker 复用固定线程，替代原始 tinyhttpd 的 pthread-per-request 模型。
- worker 数量由配置项 `thread_num` 控制，默认 4；当前线程池实现自身限制为 1 到 100。

### 配置文件

启动时会尝试读取 `config/server.conf`。配置文件不存在时使用内置默认值。

支持的配置项：

| 配置项 | 默认值 | 说明 |
|---|---:|---|
| `port` | `8080` | 监听端口，范围 `1-65535`。 |
| `thread_num` | `4` | 线程池 worker 数。配置解析允许 `1-1024`，但当前线程池实现限制为 `1-100`。 |
| `root_dir` | `./htdocs` | 静态文件和 CGI 根目录，最大长度 511 字节。 |
| `enable_access_log` | `1` | 是否写入 `logs/access.log`。 |
| `enable_error_log` | `1` | 是否写入 `logs/error.log`。 |

配置格式为 `key=value`，空行和 `#` 注释会被忽略。未知 key 和非法值会打印 warning，但不会阻止 server 启动。

示例：

```conf
# config/server.conf
port=8080
thread_num=8
root_dir=./htdocs
enable_access_log=1
enable_error_log=1
```

命令行端口会覆盖配置文件中的端口：

```bash
./httpd 18080
```

### 日志

服务器会自动创建 `logs/` 目录，并根据配置追加写入：

- `logs/access.log`：访问日志。
- `logs/error.log`：错误日志。

访问日志格式：

```text
[YYYY-MM-DD HH:MM:SS] CLIENT_IP "METHOD URL" STATUS
```

错误日志格式：

```text
[YYYY-MM-DD HH:MM:SS] ERROR: MESSAGE
```

日志写入由 `pthread_mutex_t` 保护，避免多个 worker 并发写入时日志行交错。当前没有实现日志轮转。

### MIME 类型

静态文件响应会根据扩展名设置 `Content-Type`：

| 扩展名 | Content-Type |
|---|---|
| `.html`, `.htm` | `text/html` |
| `.css` | `text/css` |
| `.js` | `application/javascript` |
| `.json` | `application/json` |
| `.txt` | `text/plain` |
| `.png` | `image/png` |
| `.jpg`, `.jpeg` | `image/jpeg` |
| `.gif` | `image/gif` |
| `.svg` | `image/svg+xml` |
| `.ico` | `image/x-icon` |
| `.pdf` | `application/pdf` |
| `.bin` | `application/octet-stream` |

未知扩展名和无扩展名文件默认使用 `application/octet-stream`。错误响应使用 `text/html`。

### 基础安全与鲁棒性

- 请求行过长返回 `400`。
- 单个 header 行超过缓冲区返回 `400`。
- 请求 header 累计超过 8KB 返回 `413`。
- POST body 通过 `Content-Length` 限制为 1MB，超过返回 `413`。
- POST body 截断或读取异常返回 `400`。
- socket 读取设置 5 秒 `SO_RCVTIMEO`。
- 静态文件和 CGI 路径通过 `realpath()` 校验，最终路径必须位于配置的真实 `root_dir` 下，防止 symlink 逃逸。
- 静态文件权限不足时返回 `403`。
- 错误响应包含 `Content-Length` 和 `Connection: close`。
- `SIGPIPE` 被忽略，避免客户端断开导致 server 进程退出。
- `SIGINT` / `SIGTERM` 会触发基础退出流程，停止 accept 并关闭线程池。

## 请求处理流程

```mermaid
flowchart TD
    A["main.c: main"] --> B["load_config(config/server.conf)"]
    B --> C["set_server_config"]
    C --> D["server.c: server_run"]
    D --> E["startup: socket/bind/listen"]
    E --> F["threadpool_init(cfg->thread_num, 1000)"]
    F --> G["accept(client_fd)"]
    G --> H["threadpool_submit(client_fd)"]
    H -->|queue full| Z["close(client_fd)"]
    H -->|queue has room| I["worker thread"]
    I --> J["handle_client: set SO_RCVTIMEO"]
    J --> K["request.c: accept_request"]
    K --> L["parse_request_line"]
    L --> M["read_headers"]
    M --> N{"method"}
    N -->|OPTIONS| O["send Allow + CORS headers"]
    N -->|GET / HEAD| P["resolve_safe_path(root_dir + url)"]
    N -->|POST| Q["resolve_safe_path + handle_cgi"]
    P --> R{"static or CGI"}
    R -->|static| S["serve_file: headers + fread + send_all"]
    R -->|CGI| T["handle_cgi"]
    Q --> T
    T --> U["fork + pipe + dup2 + execl"]
    U --> V["parent forwards CGI output"]
    O --> W["log_access + close"]
    S --> W
    V --> W
```

## 编译、运行、测试

### 编译

```bash
make clean && make
```

`httpd` 由以下模块编译链接：

```text
main.c server.c request.c response.c static_file.c cgi.c utils.c
log.c config.c mime.c threadpool.c
```

### 运行

使用默认配置启动：

```bash
./httpd
```

临时覆盖端口：

```bash
./httpd 18080
```

### 手动请求

```bash
curl --noproxy '*' -i http://127.0.0.1:8080/
curl --noproxy '*' -I http://127.0.0.1:8080/
curl --noproxy '*' -i http://127.0.0.1:8080/date.cgi
curl --noproxy '*' -i -X POST -d 'a=1' http://127.0.0.1:8080/date.cgi
curl --noproxy '*' -i -X OPTIONS http://127.0.0.1:8080/
```

### 自动化测试

```bash
tests/run_integration_tests.sh
PORT=18081 tests/run_integration_tests.sh
tests/log_test.sh
tests/config_test.sh
tests/mime_test.sh
```

测试脚本会编译项目、启动本地 server、创建临时 fixture、执行请求并清理资源。

测试覆盖概要：

- `tests/run_integration_tests.sh`
  - 基础静态资源 `GET /`。
  - 404。
  - 二进制静态文件不会因 `NUL` 字节截断。
  - 静态文件 GET/HEAD 的 `Content-Length`。
  - HEAD 不返回 body。
  - CGI GET/HEAD/POST。
  - OPTIONS。
  - header 单行和累计大小限制。
  - symlink 指向根目录外部时返回 403。
  - POST body 截断返回 400。
  - CGI 超时后 worker 可继续处理普通请求。
- `tests/log_test.sh`
  - 访问日志格式。
  - 404 错误日志。
  - CGI 失败错误日志。
- `tests/config_test.sh`
  - 配置文件缺失时使用默认值。
  - 自定义端口。
  - 自定义 `root_dir`。
  - 非法配置不导致 server 崩溃。
- `tests/mime_test.sh`
  - 常见扩展名的 `Content-Type`。
  - 未知扩展名默认 `application/octet-stream`。
  - 错误响应包含 `Content-Type` 和 `Content-Length`。

## 项目结构

```text
Tinyhttpd/
├── main.c                        # 入口: 加载配置, 命令行端口覆盖, 启动 server
├── server.c / server.h           # socket/bind/listen/accept, 信号处理, 线程池接入
├── request.c / request.h         # 请求行解析, header 读取, 方法分发, 状态日志
├── response.c / response.h       # 静态响应头, 错误响应, Content-Length
├── static_file.c / static_file.h # 静态文件读取与发送
├── cgi.c / cgi.h                 # CGI: pipe/fork/dup2/execl + 超时
├── utils.c / utils.h             # send_all, get_line, 路径安全校验
├── config.c / config.h           # key=value 配置解析和全局配置
├── log.c / log.h                 # access/error 日志, mutex 保护
├── mime.c / mime.h               # 扩展名到 Content-Type 映射
├── threadpool.c / threadpool.h   # 固定线程池 + FIFO 任务队列
├── simpleclient.c                # 历史 TCP 客户端 demo
├── benchmark.c                   # 简单多线程压测工具
├── Makefile
├── htdocs/                       # 默认静态文件和 CGI 脚本
│   ├── index.html
│   ├── index2.html
│   ├── date.cgi
│   ├── check.cgi
│   └── color.cgi
├── tests/
│   ├── run_integration_tests.sh
│   ├── config_test.sh
│   ├── log_test.sh
│   └── mime_test.sh
├── docs/
├── BENCHMARK.md
├── THREADPOOL_INTEGRATION.md     # 历史线程池改造说明
├── TESTING.md
└── README.md
```

## 线程池接口

```c
void threadpool_init(int thread_count, int max_queue_size, task_handler handler);
int threadpool_submit(int client_fd);
void threadpool_shutdown(void);
int threadpool_get_processed_count(void);
```

当前 server 使用配置项 `thread_num` 作为 worker 数，任务队列大小固定为 1000。队列满时 `threadpool_submit()` 返回 `-1`，主线程关闭新连接。

## Benchmark

编译：

```bash
make benchmark
```

使用：

```bash
./benchmark <host> <port> <path> <num_threads> <requests_per_thread>
```

示例：

```bash
./benchmark 127.0.0.1 8080 / 10 100
```

注意：`benchmark` 只是简单辅助工具，只发送 GET，请求成功标准是响应中包含 `200 OK`，不提供延迟分位数、完整响应读取、超时控制或严谨性能结论。

## Limitations

- 不建议生产使用。本项目定位是 tinyhttpd 的工程化改造和 C 网络编程练习。
- 不支持完整 HTTP/1.1：没有 keep-alive、chunked transfer、range request、host-based virtual hosting、完整 header 解析等。
- CGI 超时只是简单的子进程 wall-clock `alarm()`，不是完整 sandbox；不限制 CPU、内存、文件系统、系统调用，也不清理复杂子进程树。
- CGI 如果已经输出部分响应后再以非 0 状态退出，server 会记录错误并返回失败状态给调用链，但客户端可能已经收到 `200 OK`。
- URL decoding 行为有限：server 当前不做完整 URL decode；路径约束依赖 raw URL 检查和最终 `realpath()` confinement。
- `thread_num` 的配置解析允许到 1024，但当前 `threadpool_init()` 限制最大 100；超过时线程池初始化会失败并打印错误，这一点后续应统一。
- 日志写入是线程安全追加，但没有日志轮转。
- benchmark 工具只是简单压测辅助，不代表严谨性能评测。

## 后续可改进方向

- 统一 `thread_num` 配置上限和线程池实现上限，并让 `threadpool_init()` 返回初始化状态。
- 增加 URL decode 和对应路径安全测试。
- 改进 CGI 错误语义，避免脚本失败时客户端已收到 `200 OK`。
- 增加 GitHub Actions CI，运行 `make` 和全部测试脚本。
- 增加 AddressSanitizer / UndefinedBehaviorSanitizer 构建目标。
- 增加日志轮转。
- 使用 `sendfile()` 优化静态文件发送。
- 增强 benchmark：完整读取响应、超时、p95/p99、多轮统计。
