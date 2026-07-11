# 线程池设计与集成说明

本文记录当前实现，不是迁移步骤。Tinyhttpd 的主线程只负责 `accept()`，固定数量的 worker 从有界 FIFO 队列取出连接并执行 `handle_client()`。

## 公共接口

```c
typedef void (*task_handler)(int client_fd);

int threadpool_init(int thread_count, int max_queue_size,
                    task_handler handler);
int threadpool_submit(int client_fd);
void threadpool_shutdown(void);
int threadpool_get_processed_count(void);
```

- `threadpool_init()`：成功返回 0，参数非法、重复初始化、分配失败或 worker 创建失败时返回 -1。worker 数范围为 1–100。
- `threadpool_submit()`：只接收已经初始化且尚未关闭的 pool。队列满或分配失败时返回 -1，调用者仍拥有并负责关闭 `client_fd`。
- `threadpool_shutdown()`：停止接收新任务，唤醒 worker，排空已经接受的任务，join 全部 worker，并释放 mutex、condition variable、线程数组和队列节点。未初始化或重复调用是安全的 no-op。
- `threadpool_get_processed_count()`：返回本次 pool 生命周期中 handler 已完成的任务数。

## 所有权与资源规则

```text
accept() 成功
  ├─ submit() 失败 → server 主线程 close(client_fd)
  └─ submit() 成功 → 队列拥有 fd 值
                        └─ worker 调用 handle_client(fd)
                              └─ 请求处理路径关闭 fd
```

任务节点直接保存 `int client_fd`，不再为 fd 单独分配堆内存。handler 必须在所有返回路径关闭连接；线程池只管理任务节点和线程生命周期。

## 并发不变量

- 队列的 `head`、`tail`、`task_count` 和 `shutdown` 只在 `pool.mutex` 下访问。
- 初始化、提交和关闭之间的生命周期切换由单独的 `lifecycle_mutex` 串行化，避免 submit 与 mutex 销毁并发。
- worker 在“队列为空且 shutdown 已设置”时退出；若 shutdown 时队列非空，会继续处理直到排空。
- 完成计数使用原子操作，不参与任务调度正确性。

## Server 集成

`server_run()` 使用配置的 `thread_num` 和固定队列长度 1000 初始化线程池。初始化失败时关闭监听 socket 并返回非 0；队列满时关闭刚接受的 client socket。`SIGINT` / `SIGTERM` 令 accept 循环退出，然后调用 `threadpool_shutdown()` 完成有界的优雅关闭。

## 测试

`tests/threadpool_test.c` 覆盖：

- 非法 worker 数、队列长度和 NULL handler。
- 合法初始化和任务提交。
- shutdown 排空已接受任务。
- processed count。
- 重复 shutdown 和关闭后拒绝 submit。

运行：

```bash
make unit-test
```
