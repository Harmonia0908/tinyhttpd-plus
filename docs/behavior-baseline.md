# Tinyhttpd 行为基线

## 目的与范围

本文锁定当前 Tinyhttpd 的外部可观察行为，供后续渐进式重构比较。基线事实来自
[`src/`](../src)、[`include/`](../include)、[`Makefile`](../Makefile) 和
[`tests/`](../tests)，并于 2026-07-16 在 macOS arm64、Apple Clang 21.0.0、GNU
Make 3.81 环境完成本地验证。

本文记录“当前真实行为”，不把已知限制描述成待实现功能。若代码、测试与旧文档
冲突，以代码和自动化测试为准。

## 构建、运行和测试环境

### 必需环境

- C17 编译器；
- POSIX socket、pthread、fork/pipe/execve、signal、poll；
- GNU/BSD Make；
- 完整 shell 测试需要 Bash、`curl`、`nc`、`perl` 和常见 POSIX 工具；
- sanitizer 目标需要编译器支持 AddressSanitizer 和 UndefinedBehaviorSanitizer。

项目没有 CMake/CTest、数据库、容器运行时或第三方 C 业务库。默认二进制只链接系统
运行库和 pthread。

### 已确认命令

| 命令 | 输入 | 成功输出/产物 | 退出码 | 文件变化 |
|---|---|---|---:|---|
| `make clean` | 无 | 打印删除命令 | 0 | 删除根二进制、对象和 `build/` |
| `make` | 当前源码 | 生成 `httpd`、`client`、`benchmark` | 0 | 写入 `build/default/` 和根目录兼容二进制 |
| `scripts/build.sh debug` | 可选参数 `debug` | `-O0 -g3` 完整构建 | 0 | 先 clean，再写 `build/debug/` 和根二进制 |
| `scripts/build.sh release` | 参数 `release` | `-O2 -g0` 完整构建 | 0 | 先 clean，再写 `build/release/` 和根二进制 |
| `scripts/build.sh invalid` | 非法 profile | 向 stderr 打印 usage | 2 | 无预期源码变化 |
| `make unit-test` | 无 | 五个测试程序打印 `PASS` | 0 | 写入 profile tests 和 `build/tests/` |
| `scripts/test.sh all` | 可选环境变量 `PORT` | 单元测试与四组 shell suite 全部通过 | 0 | 临时创建配置、日志、fixture 和进程，退出时恢复/清理 |
| `scripts/test.sh sanitizer` | 无 | 同一套测试在 ASan/UBSan 下通过 | 0 | clean 后写 sanitizer 产物，测试临时文件会清理 |

普通编译统一使用 C17 和 `-Wall -Wextra -Wpedantic -Wconversion -Wshadow`。当前
Default、Debug、Release 和 sanitizer 构建均没有编译警告或链接错误。

## 可执行程序与命令行

### `httpd`

公开运行形式：

```text
./httpd [port]
```

- 不传参数：读取 `config/server.conf`；文件缺失时端口为 8080。
- `argv[1]` 是十进制端口，合法范围 1–65535，并覆盖配置端口。
- 非法端口不会打印 usage 或退出，而是把端口设为 8080。
- `argv[2]` 及之后参数当前被忽略。
- 成功监听后向 stdout 打印：`httpd running on port <port>`。
- 正常 SIGINT/SIGTERM：停止 accept，排空已提交任务，关闭 listener，退出码 0。
- socket/fcntl/bind/listen/非暂态 accept 错误：`perror()` 后退出码 1。
- 线程池初始化失败：stderr 打印 `failed to initialize thread pool`，关闭 listener，
  退出码 1。

实现依据是 [`main()`](../src/app/main.c)、[`server_run()`](../src/server/server.c) 和
[`error_die()`](../src/common/fd_lifecycle.c)。

仓库本机可能存在被 `.gitignore` 排除的 `config/server.conf`；它仍会影响本地运行，
但不是可移植的交付配置。排查端口或 document root 时应先查看该文件。

### `client`

[`tools/simpleclient.c`](../tools/simpleclient.c) 忽略命令行参数，固定连接
`127.0.0.1:9734`，发送一个字符 `A` 并读取一个字符。socket/connect/I/O 失败返回 1，
成功打印 `char from server = <char>` 并返回 0。它不是 HTTP 客户端。

### `benchmark`

[`tools/benchmark.c`](../tools/benchmark.c) 的形式为：

```text
./benchmark <host> <port> <path> <num_threads> <requests_per_thread>
```

它只发送 GET，以首次接收的至多 1023 字节中包含 `200 OK` 判断成功。线程最大 50、
每线程请求最大 1000；总请求数是二者乘积。它没有完整响应读取、网络超时或延迟
分位数，不能作为严谨性能基准。

## 配置文件格式

路径固定为 `config/server.conf`，格式是逐行 `key=value`：

| key | 默认值 | 有效范围/语义 |
|---|---:|---|
| `port` | 8080 | 1–65535 |
| `thread_num` | 4 | 1–100 |
| `root_dir` | `./htdocs` | 非空，最多 511 字节 |
| `enable_access_log` | 1 | 0 或 1 |
| `enable_error_log` | 1 | 0 或 1 |

空行和首个非空字符为 `#` 的行被忽略。未知 key、缺少 `=`、过长行或非法值向
stderr 打印 warning，但不阻止启动；非法字段恢复该字段的内置默认值。配置文件
缺失不打印错误并返回默认配置；其他打开错误打印 warning 后继续使用默认值。

配置结构与 API 位于 [`include/config.h`](../include/config.h)，解析实现位于
[`src/app/config.c`](../src/app/config.c)。配置文件不会由 Server 自动创建。

## HTTP 输入协议

### 连接模型

- IPv4 TCP，listener 绑定 `INADDR_ANY`；
- listener 非阻塞，accepted client fd 被改回阻塞；
- 每个连接只处理一个请求，不支持跨请求复用；提交线程池失败时由 Server 关闭
  client fd，提交成功后由请求、静态文件或 CGI 处理路径负责关闭；
- 不支持 keep-alive；
- worker 首次请求读取超时为 5 秒；POST CGI body 读取超时为 1 秒；
- 进程忽略 SIGPIPE。

### 请求行

支持的方法比较大小写不敏感：

- GET
- HEAD
- POST
- OPTIONS

不支持的方法返回 501。请求行缺失 URI、格式错误或超过 1023 字节返回 400；URL
无法放入 255 字节调用方缓冲区时返回 414。URL 必须以 `/` 开头。协议版本 token
不会被建模为独立字段。

纯解析接口 [`http_parse_request_line()`](../src/http/parser.c) 接受
`data + length`；socket 适配器 [`parse_request_line()`](../src/http/request.c) 负责读取
和发送错误响应。

### Headers

当前只解释 `Content-Length`：

- 缺失值保持 -1；
- 重复、负数或含无效尾随字符返回 400；
- 最大接受值为 1,048,576；更大返回 413；
- 单行达到 1023 字节且未结束返回 400；
- header 累计超过 8192 字节返回 413；
- 其他 header 被读取但不保存。

CRLF、裸 CR 和 LF 由 [`net_read_line()`](../src/net/io.c) 规范成以 `\n` 结束的行。
纯解析器虽接受显式字节长度，字段语义仍基于 C 字符串，不承诺任意 embedded NUL
行为。

### Body

只有 POST CGI 读取请求 body。声明长度缺失为 400，超过 1 MiB 为 413；实际 body
短于声明、提前 EOF、超时或读取失败为 400。GET、HEAD、OPTIONS 不读取请求 body。

## HTTP 输出格式

### 通用响应

静态文件和 Server 生成的错误响应使用：

```text
HTTP/1.0 <status>\r\n
Server: Tinyhttpd/1.0\r\n
...
Connection: close\r\n
\r\n
```

静态成功响应还包含按扩展名决定的 `Content-Type` 和十进制 `Content-Length`。错误
响应包含 `Content-Type: text/html`、准确 Content-Length 和固定 HTML body。

当前状态与状态文本包括：

| 状态 | 文本 | 主要触发条件 |
|---:|---|---|
| 200 | `OK` | 静态、OPTIONS 或开始转发 CGI |
| 400 | `BAD REQUEST` | 请求行/header/路径缓冲/body 错误 |
| 403 | `Forbidden` | `..` 路径段、root escape、静态权限不足 |
| 404 | `NOT FOUND` | 资源不存在或无法解析 |
| 413 | `Payload Too Large` | header 或 body 声明超过限制 |
| 414 | `URI Too Long` | URL 超过内部边界 |
| 500 | `Internal Server Error` | document root、CGI 或内部资源失败 |
| 501 | `Method Not Implemented` | 不支持的方法 |

响应 builder 位于 [`src/http/response.c`](../src/http/response.c)，只生成字节；发送由
调用方使用 [`net_write_all()`](../src/net/io.c) 完成。

### OPTIONS

OPTIONS 的完整方法顺序是：

```text
Allow: GET, POST, HEAD, OPTIONS
Access-Control-Allow-Origin: *
Access-Control-Allow-Methods: GET, POST, HEAD, OPTIONS
Access-Control-Allow-Headers: Content-Type
```

OPTIONS 响应没有 Server 生成的 body，也没有 `Content-Length` 或
`Connection: close` header。这是当前精确 wire behavior。

## 静态文件行为

- document root 来自 `root_dir`；目录请求补 `index.html`；
- raw URL 中名为 `..` 的路径段返回 403；
- root 和最终目标经过 `realpath()`，最终路径必须仍位于真实 root 内；
- 指向 root 外的已存在 symlink 返回 403；
- 普通文件通过 `open_cloexec()` 打开，`fread()` 每次最多 4096 字节；
- `net_write_all()` 使用显式长度，因此文件内 NUL 不截断；
- HEAD 返回与 GET 一致的静态 header，不发送 body；
- 目录或文件不存在返回 404；打开权限不足返回 403。

MIME 映射由 [`get_mime_type()`](../src/http/mime.c) 提供：html/htm、css、js、json、
txt、png、jpg/jpeg、gif、svg、ico、pdf、bin；未知或无扩展名使用
`application/octet-stream`，错误页使用 `text/html`。

## CGI 行为与协议

进入 CGI 的条件：

- POST 总是按 CGI 处理；
- GET/HEAD 带 query string；
- GET/HEAD 目标文件有任一执行位。

环境变量：

- 总是设置 `REQUEST_METHOD`；
- GET/HEAD 设置 `QUERY_STRING`；
- POST 设置 `CONTENT_LENGTH`，不设置 QUERY_STRING。

[`execute_cgi()`](../src/cgi/cgi.c) 使用两组 pipe、fork、dup2 和 execve。子进程
stdout 进入 Server，stdin 接收 POST body；子进程恢复 SIGPIPE/SIGALRM 默认动作并
设置 5 秒 alarm。父进程负责关闭 pipe 和 waitpid，不把 CGI stderr 写进 HTTP 响应。

CGI 输出行为：

- 父进程读到 CGI 第一个输出字节后，先发送 `HTTP/1.0 200 OK` 和 Server header；
- 非 HEAD 原样转发全部 CGI stdout；
- HEAD 只转发到 `\n\n` 或 `\r\n\r\n`，再读取并丢弃 body；
- CGI 无输出或启动失败时可返回完整 500；
- CGI 已输出后再非零退出，访问日志记录/调用链返回 500，但客户端可能已收到 200。

CGI 输出被视为受信任脚本输出，Server 不完整解析、补全或重写其 header。CGI 没有
chroot、权限降级、CPU/内存配额、seccomp 或系统调用 sandbox。

## 文件格式与副作用

### 日志

日志相对于 Server 工作目录：

```text
logs/access.log
logs/error.log
```

访问日志格式：

```text
[YYYY-MM-DD HH:MM:SS] CLIENT_IP "METHOD URL" STATUS
```

错误日志格式：

```text
[YYYY-MM-DD HH:MM:SS] ERROR: MESSAGE
```

日志函数线程安全追加并按需创建 `logs/`。打开、目录创建或写入失败是 best-effort，
不会改变 HTTP 结果，也不会向调用者返回错误。没有日志轮转。

### 测试文件变化

shell tests 会临时：

- 备份/替换 `config/server.conf`；
- 备份/创建 `logs/`；
- 创建隔离 document root、静态文件、CGI 和输出文件；
- 启动并终止本地 `httpd`；
- 使用 trap 恢复原配置和日志路径并删除临时目录。

测试只访问 `127.0.0.1`。若测试进程被不可捕获信号终止，仍可能遗留临时目录或进程，
应人工检查。

项目没有数据库文件格式、持久化 schema 或容器卷格式。

## 公共 C 接口基线

当前公共头文件位于 [`include/`](../include)：

- app/config：`server_config_t`、`init_default_config()`、`load_config()`、
  `set_server_config()`、`get_server_config()`；
- server：`handle_client()`、`startup()`、`server_run()`；
- HTTP 连接适配：`accept_request()`、`parse_request_line()`、`read_headers()`；
- 纯 HTTP：`http_parse_request_line()`、`http_parse_headers()`、
  `http_response_buffer_t` 和三个 response builder；
- 静态/MIME：`handle_static_file()`、`serve_file()`、`cat()`、`get_mime_type()`；
- CGI：`handle_cgi()`、`execute_cgi()`；
- concurrency：四个 threadpool 生命周期/计数接口；
- net：`net_read_line()`、`net_write_all()`，以及兼容 `get_line()`、`send_all()`；
- common：fd lifecycle 和日志接口；
- legacy response：`bad_request()`、`cannot_execute()`、`headers()`、`not_found()`、
  `send_error_page()`、`unimplemented()`、`send_413()`、`uri_too_long()`。

兼容行为：对有效 buffer 调用 `get_line(sock, buf, size <= 0)` 会把 `buf[0]` 写为 NUL
并返回 0；`net_read_line()` 对 `capacity == 0` 返回 -1。两者分别代表旧兼容入口与新
严格接口。

头文件已经从仓库根目录移动到 `include/`；使用项目 Makefile 的调用者无需调整，
自建编译命令需要添加 `-Iinclude`。

## 已确认测试覆盖

- 请求行、方法、URI、Content-Length、单行和累计 header 边界；
- 纯 buffer parser，不创建 socket；
- 静态、400、OPTIONS 的精确 response bytes 和 legacy fd adapter；
- `get_line()` 非正 size 兼容行为；
- 静态 GET/HEAD、Content-Length、binary NUL、MIME、404、symlink escape；
- CGI GET/POST/HEAD、CRLF header、截断 POST、timeout、fd inheritance；
- 线程池初始化、任务处理、计数、排空和重复 shutdown；
- busy CGI worker 下 SIGTERM 排空并释放端口；
- 缺失/合法/非法配置；
- access/error 日志格式和 CGI 失败日志；
- ASan/UBSan 下的同一套测试。

另以 `HEAD` 版本和当前版本并行运行，确认静态 GET、静态 HEAD、404 和 OPTIONS 的
完整 wire bytes 相同。

## 已知限制与现有缺陷

1. 不支持完整 HTTP/1.1、keep-alive、chunked、range、virtual host 或 TLS。
2. URL 不做完整 percent decoding；encoded traversal 没有完整定义。
3. parser 对 explicit length 的支持不等于 embedded NUL 安全语义。
4. 路径 `realpath()` 检查与后续 open/exec 分离，存在本地 TOCTOU 窗口。
5. CGI 不是 sandbox，且 POST stdin/stdout 顺序可能导致双管道互等，依赖 alarm 释放。
6. CGI 已开始输出后失败，客户端可能看到 200，而日志记录 500。
7. 静态或 CGI 网络写失败通常不改变 access log 中已选择的状态码。
8. `accept_request()`、`execute_cgi()` 和 `resolve_safe_path()` 仍混合多项职责。
9. `http/request -> cgi -> http/response` 存在模块环；日志反向读取 app 配置。
10. 日志是同步 mutex + 文件追加，没有轮转；日志失败静默忽略。
11. response buffer 固定 8192 字节，部分调用者忽略 builder/发送失败。
12. benchmark 只读取一次响应，没有超时或可靠统计。

## 尚未自动覆盖的高风险行为

- Server 层真实队列满时客户端观察到的行为；
- worker 创建到一半失败的资源清理；
- 日志磁盘满、权限变化和短写；
- client 在各响应阶段断开时的精确日志状态；
- embedded NUL、encoded traversal 和并发 document root 替换；
- 恶意 CGI 大量同时读写 stdin/stdout、复杂子进程树和资源耗尽；
- 所有 legacy C API 的无效指针参数；
- 远程 GitHub Actions Runner 的实际执行结果。
