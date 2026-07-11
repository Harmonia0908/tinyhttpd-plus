# Tinyhttpd 压力测试说明

## 概述

本目录包含一个用于测试 Tinyhttpd 服务器性能的压力测试工具 `benchmark`。该工具使用多线程并发发送 HTTP 请求，可以测试服务器的并发处理能力和响应性能。

## 编译

```bash
make benchmark
```

## 使用方法

```bash
./benchmark <host> <port> <path> <num_threads> <requests_per_thread>
```

### 参数说明

- `host`: 服务器地址（如 127.0.0.1）
- `port`: 服务器端口，范围 1-65535（如 8080）
- `path`: 请求路径（如 / 或 /index.html）
- `num_threads`: 并发线程数，范围 1-50
- `requests_per_thread`: 每个线程发送的请求数，范围 1-1000

## 示例

### 基础测试
```bash
./benchmark 127.0.0.1 8080 / 10 100
```
- 10 个线程
- 每个线程发送 100 个请求
- 总共 1000 个请求

### 高并发测试
```bash
./benchmark 127.0.0.1 8080 / 50 100
```
- 50 个线程
- 每个线程发送 100 个请求
- 总共 5000 个请求

### 轻量测试
```bash
./benchmark 127.0.0.1 8080 /index.html 5 50
```
- 5 个线程
- 每个线程发送 50 个请求
- 总共 250 个请求

## 测试流程

1. **启动服务器**
   ```bash
   ./httpd
   ```

2. **在另一个终端运行压测**
   ```bash
   ./benchmark 127.0.0.1 8080 / 10 100
   ```

3. **查看结果**
   - Total requests: 总请求数
   - Successful: 成功请求数
   - Failed: 失败请求数
   - Success rate: 成功率
   - Total time: 总耗时
   - Requests per second: 每秒请求数（QPS）
   - Average response time: 平均响应时间

## 输出示例

```
Starting load test...
Host: 127.0.0.1
Port: 8080
Path: /
Threads: 10
Requests per thread: 100
Total requests: 1000

Load test completed!
========================================
Total requests: 1000
Successful: 998
Failed: 2
Success rate: 99.80%
Total time: 2.35 seconds
Requests per second: 425.53
Average response time: 0.0234 seconds
========================================
```

## 测试场景建议

### 1. 静态文件测试
```bash
./benchmark 127.0.0.1 8080 /index.html 10 100
```

### 2. CGI 脚本测试
```bash
./benchmark 127.0.0.1 8080 /color.cgi 5 50
```

### 3. 路径遍历防护测试
```bash
./benchmark 127.0.0.1 8080 /../../../etc/passwd 5 20
```
预期：所有请求都应该失败（返回 403）

### 4. HEAD 方法测试
需要修改 benchmark.c 中的 GET 为 HEAD，然后重新编译

### 5. 不同并发级别测试
```bash
./benchmark 127.0.0.1 8080 / 1 100
./benchmark 127.0.0.1 8080 / 10 100
./benchmark 127.0.0.1 8080 / 20 100
./benchmark 127.0.0.1 8080 / 50 100
```

## 性能指标说明

- **QPS (Queries Per Second)**: 每秒处理的请求数，越高越好
- **成功率**: 成功请求占总请求的百分比，应该接近 100%
- **平均响应时间**: 单个请求的平均处理时间，越低越好
- **总耗时**: 完成所有请求的总时间

## 注意事项

1. **线程数限制**: 最大线程数为 50，超过此值会报错
2. **服务器资源**: 高并发测试可能会占用大量 CPU 和内存
3. **网络延迟**: 测试结果受网络环境影响
4. **服务器状态**: 确保服务器在测试前已启动
5. **端口占用**: 确保测试端口未被其他程序占用
6. **结果边界**: 工具只读取响应的第一个 1KB 块，并以其中出现 `200 OK` 作为成功；没有连接/读取超时、响应完整性校验或延迟分位数，因此不应用于严谨性能结论

## 故障排查

### 连接失败
- 检查服务器是否启动
- 检查端口是否正确
- 检查防火墙设置

### 成功率低
- 检查服务器日志
- 检查系统资源使用情况
- 减少并发线程数

### 响应时间过长
- 检查服务器性能
- 检查网络状况
- 检查请求的文件大小

## 扩展功能

如需修改测试行为，可以编辑 `benchmark.c` 源代码：

- 修改请求方法（GET/POST/HEAD）
- 添加自定义 HTTP 头
- 修改超时设置
- 添加更多统计指标

## 清理

```bash
make clean
```
