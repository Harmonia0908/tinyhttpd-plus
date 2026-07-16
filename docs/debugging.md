# Tinyhttpd 构建、测试与调试

## 环境要求

当前项目是 C17 + POSIX/pthread 的 Make 工程，不使用 CMake 或 CTest。基础构建需要：

- macOS 或 Linux；
- `make`；
- 支持 C17、pthread、ASan/UBSan（sanitizer profile）的 C 编译器；
- 完整集成测试还需要 Bash、`curl`、`nc`、`perl`、`grep` 和常见 POSIX 工具。

[`tests/run_integration_tests.sh`](../tests/run_integration_tests.sh) 只连接
`127.0.0.1`，不访问外网。Chrome 只用于可选手工验证。

## 构建

### 默认构建

```bash
make clean
make
```

默认 profile 使用 `-O2 -g`，并启用 C17、`-Wall -Wextra -Wpedantic -Wconversion
-Wshadow`。生成：

- 根目录兼容入口：`httpd`、`client`、`benchmark`；
- 对象和依赖文件：`build/default/obj/`；
- profile 二进制：`build/default/bin/`。

### Debug 与 Release

```bash
scripts/build.sh debug     # -O0 -g3
scripts/build.sh release   # -O2 -g0
```

等价 Make 目标是 `make debug`、`make release`。这两个目标会先执行 `make clean`，
所以不会保留上一个 profile 的根目录二进制。`scripts/build.sh` 无参数时默认 Debug；
非法参数返回退出码 2 并打印 usage。

如果需要指定工具链，可使用现有覆盖点，例如：

```bash
make clean
make CC=clang CFLAGS='-O0 -g3'
```

项目必需的 `-D_XOPEN_SOURCE=700 -Iinclude` 和 warning set 仍由 Makefile 添加。

## 运行

```bash
./httpd             # 使用 config/server.conf；缺失时使用内置默认配置
./httpd 18082       # 只覆盖端口
```

成功启动会输出 `httpd running on port <port>`。服务绑定 `INADDR_ANY`，但测试只访问
loopback。按 `Ctrl-C` 触发 SIGINT，Server 排空已接受任务后退出。

### 运行前先检查配置

配置路径由 [`CONFIG_PATH`](../include/config.h) 固定为 `config/server.conf`。支持：

```conf
port=8080
thread_num=4
root_dir=./htdocs
enable_access_log=1
enable_error_log=1
```

命令行参数只覆盖 `port`，不会覆盖 `root_dir` 或日志开关。若工作区中的本地配置指向
已经删除的临时目录，`./httpd 18082` 仍会启动，但静态请求会返回资源定位错误；这
不是端口问题。先检查 `root_dir` 是否存在、能被 `realpath()` 解析，并确认 CGI 文件
有执行位。

无效命令行端口（例如 `./httpd invalid`）当前静默回退到 8080；无效配置项则打印
warning 并对该项使用默认值。这是代码的现有行为。

## 测试

### 快速单元/组件测试

```bash
scripts/test.sh unit
# 或
make unit-test
```

依次运行：

- `request_test`：socketpair 上的请求行/header 边界；
- `cgi_test` + `cgi_fd_probe`：16 个并发 CGI exec 后的 fd 继承；
- `threadpool_test`：非法初始化、任务处理、计数、排空和重复 shutdown；
- `http_parser_test`：不创建 socket 的 buffer 解析；
- `http_response_test`：纯 builder 与旧 fd adapter 的精确响应字节。

### 完整测试

```bash
scripts/test.sh all
# 或
make test
```

在上述测试后继续运行四个 shell suite：

- [`run_integration_tests.sh`](../tests/run_integration_tests.sh)：静态 GET/HEAD、404、
  binary、CGI GET/POST/HEAD、OPTIONS、header 限制、symlink confinement、截断 POST、
  CGI timeout 和 busy-worker shutdown；
- [`config_test.sh`](../tests/config_test.sh)：缺失/合法/非法配置、端口和 root；
- [`log_test.sh`](../tests/log_test.sh)：access/error 日志和 CGI 失败；
- [`mime_test.sh`](../tests/mime_test.sh)：MIME 和错误响应 header。

这些脚本会备份并恢复已有 `config/server.conf` 和 `logs/`，创建隔离临时目录，并在
trap 中清理后台 Server。测试失败后如果怀疑遗留进程，可先检查端口，而不要盲目
终止所有 `httpd` 进程。

### Sanitizer

```bash
scripts/test.sh sanitizer
# 或
make sanitizer-test
```

该 profile 使用 AddressSanitizer + UndefinedBehaviorSanitizer，并运行相同单元和集成
测试。Apple Clang 不支持 LeakSanitizer，Make 目标没有强制 `detect_leaks=1`；Linux
CI 使用平台默认支持。

### 单独运行一个 shell suite

```bash
PORT=18081 tests/run_integration_tests.sh
SKIP_BUILD=1 PORT=18086 tests/mime_test.sh
```

`SKIP_BUILD=1` 只应在当前 `httpd` 已经构建且与你要验证的源码一致时使用。

## 手工功能验证

确认 `root_dir` 指向仓库的 `htdocs` 后启动：

```bash
./httpd 18082
```

在另一个终端执行：

```bash
curl --noproxy '*' -i http://127.0.0.1:18082/
curl --noproxy '*' -I http://127.0.0.1:18082/
curl --noproxy '*' -i http://127.0.0.1:18082/not-found.html
curl --noproxy '*' -i http://127.0.0.1:18082/date.cgi
curl --noproxy '*' -i -X POST -d 'a=1' http://127.0.0.1:18082/date.cgi
curl --noproxy '*' -i -X OPTIONS http://127.0.0.1:18082/
```

关键预期：

| 请求 | 当前预期 |
|---|---|
| `GET /` | `HTTP/1.0 200 OK`、HTML MIME、准确 Content-Length 和 body |
| `HEAD /` | 与 GET 对应的 header，不含 body |
| 缺失文件 | `HTTP/1.0 404 NOT FOUND` 错误页 |
| `GET /date.cgi` | 固定 200 前缀 + CGI 输出，页面含 `Today is:` |
| POST CGI | body 经 CGI stdin 传入，不挂起 |
| OPTIONS | `Allow: GET, POST, HEAD, OPTIONS` 和 CORS headers |

[`TESTING.md`](../TESTING.md)、[`response.c`](../src/http/response.c) 和两类测试当前
一致使用 `GET, POST, HEAD, OPTIONS`；调试 wire bytes 时也应验证这个精确顺序。

## LLDB 调试

建议先构建 Debug：

```bash
scripts/build.sh debug
lldb -- ./httpd 18082
```

常用命令：

```text
(lldb) breakpoint set --name main
(lldb) breakpoint set --name server_run
(lldb) breakpoint set --name accept_request
(lldb) run
(lldb) thread list
(lldb) thread backtrace all
(lldb) frame variable
(lldb) continue
```

`accept_request()` 在线程池 worker 中执行，不一定是主线程；请求到达后用
`thread list` 和 `thread backtrace all` 找到停住的 worker。需要观察 CGI child 时，
可尝试在运行前设置：

```text
(lldb) settings set target.process.follow-fork-mode child
```

不同 LLDB/平台的 fork 跟随能力有差异。更稳定的父进程排查方式是在
`fork_with_cloexec_pipes()`、`read_retry()`、`wait_for_child()` 处断点，并从父进程
观察 pid、pipe fd 和 wait status。

## GDB 调试

Linux/GDB 可使用：

```bash
gdb --args ./httpd 18082
```

```text
(gdb) break main
(gdb) break accept_request
(gdb) run
(gdb) info threads
(gdb) thread apply all bt
(gdb) set follow-fork-mode child
(gdb) set detach-on-fork off
```

## 推荐断点

| 断点 | 观察内容 |
|---|---|
| `main` / `load_config` | `cfg.port`、`thread_num`、`root_dir`、日志开关和 CLI 覆盖 |
| `startup` | `port`、bind errno、listener flags |
| `server_run` | `running`、poll revents、client fd、submit 结果 |
| `threadpool_submit` / `worker` | `task_count`、shutdown、fd 所有权转移 |
| `handle_client` | socket timeout 设置是否成功（当前返回值未检查） |
| `accept_request` | `method`、`url`、`access_url`、`content_length`、`cgi`、`status` |
| `http_parse_request_line` | `data/length` 与 400/414/501 边界 |
| `http_parse_headers` | 重复/非法 Content-Length、header_size |
| `resolve_safe_path` | raw URL、真实 root、候选路径、realpath 结果、`st_mode` |
| `serve_file` | open errno、file_size、HEAD 标志、发送失败 |
| `execute_cgi` | `is_post`、body remaining、pipe fd、pid、wait status |
| `net_read_line` / `net_write_all` | errno、超时、短写和客户端提前断开 |
| `log_access` / `log_error_message` | 配置开关、日志路径和 open/fdopen 失败 |

## 常见问题定位

### `bind: Address already in use`

指定另一个端口，并确认 `config/server.conf` 与命令行实际选择的端口。测试脚本在端口
已被占用时会失败，但不会杀死占用该端口的未知进程。

### Server 启动但 `/` 返回 404/500

检查当前工作目录和 `root_dir`。`resolve_safe_path()` 先对配置 root 做 `realpath()`；
路径不存在时不会自动退回 `./htdocs`。命令行端口覆盖与 root 无关。

### CGI 返回 500

依次检查：目标是否位于真实 root 内、是否设置执行位、shebang 解释器是否存在、脚本
是否能在 5 秒内输出至少一个字节、是否非零退出。启用错误日志后查看
`logs/error.log`；CGI 子进程本身的 stderr 没有重定向到 HTTP 响应。

### POST CGI 返回 400

缺失/非法/重复 `Content-Length`、声明超过 1 MiB、body 比声明短、1 秒 body 读取超时
都会导致 400 或 413。用 `nc` 复现时必须发送精确 CRLF 和声明长度。

### 请求无响应或连接被关闭

- 请求行读取失败/EOF 会直接关闭，不保证错误页；
- 线程池队列满时主线程直接关闭新连接；
- client socket 初始读取超时为 5 秒，POST body 在 CGI 父进程中为 1 秒；
- 客户端断开不会杀死 Server，因为进程忽略 SIGPIPE。

### 日志没有生成

确认 `enable_access_log`/`enable_error_log` 为 1。日志是 best-effort，目录创建、open 或
fdopen 失败不会反馈给客户端，也不会让请求失败。路径固定为 `logs/access.log` 和
`logs/error.log`，相对于 Server 当前工作目录。

### 测试卡在 CGI

检查 `nc`、`perl` 和 CGI 脚本执行权限。`sleep.cgi` 场景依赖 5 秒 alarm 释放 worker；
完整测试本身还带 watchdog。调试时不要在 CGI child 的 alarm 设置后长时间单步，
否则预期超时会主动终止子进程。

## 格式与 CI

`.clang-format` 适配当前 C 风格，只格式化正在修改的文件：

```bash
clang-format --style=file --dump-config >/dev/null
clang-format -i src/http/parser.c include/http_parser.h
```

项目没有启用 clang-tidy，也没有 compilation database；不要把一次性历史告警清理
混入功能修改。[`ci.yml`](../.github/workflows/ci.yml) 在 Ubuntu 上调用与本地相同的
Debug/Release 构建、完整测试和 sanitizer 脚本。CI 失败时应先在本地运行对应
`scripts/*.sh`，不要另造 CI 专用修复路径。
