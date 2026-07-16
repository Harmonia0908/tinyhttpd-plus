# Tinyhttpd 工程化重构历史

## 说明与证据边界

本文记录当前工作区相对于仓库 `HEAD` 中扁平布局的工程化变化。当前变化尚未被本文
假定为已提交版本；文件移动、拆分和测试事实以工作区的 [`src/`](../src)、
[`include/`](../include)、[`tests/`](../tests) 与 [`Makefile`](../Makefile) 为准，旧布局
可通过 `git show HEAD:<file>` 核对。

这不是一次项目重写。Server 的线程池模型、HTTP/1.0 wire bytes、CGI pipe/fork/exec、
配置格式、CLI、日志格式和连接关闭策略都被延续。

## 重构前的主要问题

### 1. 根目录扁平，模块边界依赖文件名约定

`HEAD` 中 `main.c`、`server.c`、`request.c`、`response.c`、`cgi.c`、`utils.c`、
`threadpool.c` 等实现和头文件都位于根目录。构建列表能表达“哪些文件参与链接”，却
不能直观看出 app/server/http/cgi/net/common 的依赖方向。

### 2. `request.c` 同时读取 socket 与实现 HTTP 解析

旧 [`request.c`](../src/http/request.c) 对应的 HEAD 版本同时包含 `get_line()` 输入、
method/URL token 扫描、Content-Length 解析、路由和错误发送。解析规则只能经 fd 或
`socketpair()` 测试，纯字符串边界难以独立覆盖。

### 3. `response.c` 同时构造和发送响应

旧响应函数直接接收 client fd 并调用 `send_all()`。状态文本、header 顺序、
Content-Length 与底层 socket 发送绑在一起，精确 wire bytes 测试必须建立 fd。

### 4. `utils.c` 职责过多

旧 `utils.c` 同时包含：

- accept/open/pipe/fork 与 fd flags；
- `send_all()`、`get_line()` 网络 I/O；
- `resolve_safe_path()` 和 traversal 判断；
- `error_die()`。

它既被 Server、HTTP、CGI 使用，又 include 配置和响应接口，形成宽泛的隐式公共层。

### 5. 构建图和 profile 不够清晰

旧 Makefile 直接把源文件一次性传给编译器。没有 profile 隔离、对象级头文件依赖、
纯 parser/response 测试目标或统一 build/test 脚本；Debug、Release 和 sanitizer 的
产物容易相互覆盖。

## 已完成的整理

### 目录与接口分离

实现按职责移动到 `src/app`、`src/server`、`src/http`、`src/cgi`、
`src/concurrency`、`src/net`、`src/common`，公共头文件移动到 `include/`，辅助程序移动
到 `tools/`。入口 [`main.c`](../src/app/main.c) 保持为配置、参数和启动装配。

这一步主要是物理边界和构建边界调整，没有引入框架或新的运行时架构。

### HTTP 与网络解耦

- 从请求适配中提取 [`src/http/parser.c`](../src/http/parser.c) 和
  [`include/http_parser.h`](../include/http_parser.h)。
- `http_parse_request_line(data, length, ...)` 和
  `http_parse_headers(data, length, ...)` 直接接受 buffer，不 include socket。
- [`src/net/io.c`](../src/net/io.c) 只保留可靠行读取和全量发送；旧 `get_line()`、
  `send_all()` 作为兼容入口保留。
- [`src/http/response.c`](../src/http/response.c) 改为填充固定
  `http_response_buffer_t`，不负责发送。
- [`src/http/response_writer.c`](../src/http/response_writer.c) 保留旧 fd API，避免
  一次性改写 CGI 和所有调用点。

选择兼容适配器而不是立即删除旧 API，是为了控制 diff 和保持错误响应、CGI 及连接
行为可回滚。

### `utils.c` 按真实职责拆分

- fd/fork 生命周期：[`src/common/fd_lifecycle.c`](../src/common/fd_lifecycle.c)；
- socket 字节 I/O：[`src/net/io.c`](../src/net/io.c)；
- HTTP 资源定位：[`src/http/resource.c`](../src/http/resource.c)。

[`include/utils.h`](../include/utils.h) 暂时作为兼容 umbrella，转引旧调用者需要的
专用头文件。它没有被继续包装成新的“通用工具框架”。

### 测试基线增强

- [`tests/http_parser_test.c`](../tests/http_parser_test.c)：直接 buffer 输入，锁定请求
  行、header、Content-Length 和大小边界；
- [`tests/http_response_test.c`](../tests/http_response_test.c)：锁定 builder 及旧 fd
  adapter 的完整字节；
- [`tests/request_test.c`](../tests/request_test.c)：继续锁定 socket 适配层的即时错误
  和边界；
- 原有 CGI fd、线程池、配置、日志、MIME、静态/CGI 集成场景继续保留。

### 构建基础设施

[`Makefile`](../Makefile) 现在按模块列源文件，生成对象和 `.d` 头文件依赖，并隔离
default/debug/release/sanitizer profile。根目录二进制和 `build/tests/` 作为兼容路径
保留。[`scripts/build.sh`](../scripts/build.sh)、[`scripts/test.sh`](../scripts/test.sh)
与 [`ci.yml`](../.github/workflows/ci.yml) 使用同一命令。

## 为什么按这些边界拆分

### parser 与 net 的边界

HTTP 语法的输入是“已读取的字节序列”，而不是“某个具体 socket”。把
`net_read_line()` 留在适配层，使 parser 能独立测试，又不需要 transport interface、
mock socket 或虚函数表。Server 仍使用原来的阻塞 client socket 和读取超时。

### response builder 与 writer 的边界

响应构造决定 wire bytes，writer 只负责可靠传输。纯 builder 使 header 顺序、CRLF、
Content-Length 和错误页能精确回归；兼容 writer 保持 CGI 调用方式不变。

### fd lifecycle 与 resource 的边界

close-on-exec 和 fork 互斥是进程/并发资源问题；URL 到文件系统路径是 HTTP 资源问题。
二者原来只是因为都在 `utils.c` 而相邻，并不共享业务语义。

### 目录边界而不是新架构

当前依然是：单进程 listener + 固定线程池 + 每连接一个请求 + 同步静态/CGI handler。
没有改成 reactor、异步 runtime、对象系统、插件系统或完整协议栈。

## 刻意保持不变的行为

- 运行形式仍为 `./httpd [port]`；无效 CLI 端口回退到 8080。
- 配置仍是 `config/server.conf` 的 `key=value`，缺失不致命，非法项 warning 后回退。
- 默认端口 8080、4 个 worker、`./htdocs` 和日志开关不变。
- 支持方法仍是 GET、POST、HEAD、OPTIONS，方法比较大小写不敏感。
- 静态和错误响应仍使用 HTTP/1.0、固定 Server header 与 `Connection: close`。
- OPTIONS wire 顺序仍是 `GET, POST, HEAD, OPTIONS`。
- 请求行/header/URI/Content-Length 的现有边界和状态码保持。
- GET query 或可执行文件进入 CGI；POST 进入 CGI。
- CGI 环境变量、pipe/fork/dup2/execve、5 秒 alarm 和 HEAD 截断语义保持。
- listener 非阻塞、accepted client 阻塞、worker socket 读取超时和固定线程池保持。
- 队列满时直接关闭新连接；shutdown 排空已接受任务。
- access/error 日志格式、路径和 best-effort 失败语义保持。
- client fd 仍由最终请求 handler 路径关闭，而不是线程池统一关闭。

## 仍然存在的技术债

### 高风险、应先补行为测试再处理

1. [`execute_cgi()`](../src/cgi/cgi.c) 仍超过 200 行，同时管理环境、pipe、fork、
   POST body、输出、HEAD 状态机、wait 和所有清理分支。拆分必须先证明每个 fd/pid
   所有权和已发送响应状态。
2. CGI 收到首字节后立即发送 200；脚本随后失败只能记录 500，客户端已经看到 200。
   改变它会影响流式输出、延迟和内存策略，不是单纯清理。
3. POST 先完整写 CGI stdin，再读 stdout。脚本若边写大量 stdout 边等待 stdin，
   两条 pipe 可能互等，最终依赖 5 秒 alarm。
4. 路径 `realpath()` 检查与后续 `open()`/`execve()` 分离，对能并发修改 document root
   的本地进程仍有 TOCTOU 窗口。
5. client fd 的关闭分散在 `request.c`、`resource.c` 的调用者、`static_file.c`、
   `cgi.c`。任何新 early return 都可能泄漏或 double-close。

### 中风险、适合独立小阶段

1. `http/request -> cgi -> http/response compatibility` 是真实模块环。可考虑把路由
   编排提升到独立 handler 层，但要先固定 CGI/静态日志和关闭顺序。
2. `common/log -> app/config` 使 common 反向读取 app 全局。可改为启动时注入日志
   配置，但要避免引入通用 DI 框架。
3. [`resolve_safe_path()`](../src/http/resource.c) 同时定位资源并生成/发送 HTTP 错误，
   因而资源层仍依赖 response/net 和 client fd。
4. [`include/utils.h`](../include/utils.h) 是宽 umbrella；调用者应渐进改为 include 专用
   头，再删除不需要的转引。
5. `active_config`、`pool`、`running`、`fork_fd_mutex` 和日志 mutex 都是进程级共享
   状态，测试隔离与多实例 Server 因此受限。
6. 网络写错误常被 handler 忽略，access log 状态表示逻辑选择结果，不表示客户端
   完整接收。

### 低风险或可选优化

1. parser API 有显式 length，但字段处理仍基于 C 字符串；embedded NUL 的语义没有
   被定义或完整测试。
2. CGI 输出按单字节读取/发送，逻辑直观但效率低。
3. `http_response_buffer_t` 固定 8192 字节，当前固定响应足够，但 builder 失败在部分
   调用点被忽略。
4. 当前只解释 Content-Length，其他请求 header 不建模；URL 也不做完整 percent
   decoding。这是协议范围限制，不只是代码组织问题。
5. 源码仍有历史中英文注释和局部样式差异；不值得为统一外观制造大范围 diff。

## 下一步不建议自动执行的重构

- 不要把同步线程池替换为 event loop、async framework 或 thread-per-request。
- 不要重写完整 HTTP parser，或顺带增加 keep-alive、chunked、HTTP/1.1 状态机。
- 不要更改 CGI 环境、超时、200 发送时点、header 转发或进程权限，除非有单独需求和
  端到端兼容测试。
- 不要为 net、parser、response 引入抽象工厂、transport class、插件注册表或大量
  mock interface。
- 不要一次性消除所有全局状态；配置、线程池、信号和 fork 锁的生命周期不同，应
  分开设计和验证。
- 不要在没有路径安全设计审查时加入 URL decode；解码时点会改变 traversal 判断与
  资源定位语义。
- 不要删除 `response.h`、`get_line()`、`send_all()` 或 `utils.h` 兼容入口，直到所有
  调用者已在独立、可编译阶段迁移并通过 wire-level 测试。
- 不要批量 clang-format 历史文件，或为清空全部 warning 修改业务分支。
- 不要把 `mime.c`、`net/io.c`、`main.c` 等小而内聚的文件继续机械拆分。

## 推荐的后续节奏

若继续工程化，建议每轮只选一项并保持可独立回滚：

1. 收窄 `utils.h` 使用者的 include，不改运行逻辑；
2. 为 `resolve_safe_path()` 增加纯路径结果测试，再考虑分离“定位”与“发错误响应”；
3. 为 CGI 建立资源状态表和更多失败注入测试，再拆 `execute_cgi()` 的父/子/转发阶段；
4. 设计并测试日志配置注入，消除 `common/log -> app/config`；
5. 只有在上述边界稳定后，再评估把路由编排从 `http/request.c` 提升到独立 handler。

每一阶段都应执行 default/debug/release 构建、完整测试、必要的手工 wire 验证，并
确认 HTTP 文本、状态码、连接关闭和文件/进程副作用没有变化。
