================================================================================
线程池集成说明 - 如何在 Tinyhttpd 中替换 pthread_create
================================================================================

一、修改 Makefile
--------------------------------------------------------------------------------

修改 Makefile，在编译时添加 threadpool.c：

    httpd: httpd.c threadpool.c
        gcc -g3 -W -Wall -pthread -o $@ $^

二、修改 httpd.c
--------------------------------------------------------------------------------

1. 添加头文件包含：

    #include "threadpool.h"

2. 在 httpd.c 开头添加 handle_client 包装函数（放在 accept_request 之前）：

    void handle_client(int client_fd) {
        struct timeval tv;
        tv.tv_sec = 5;
        tv.tv_usec = 0;
        setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        accept_request(client_fd);
    }

3. 修改 main 函数，使用线程池代替 pthread_create：

    int main(int argc, char *argv[]) {
        int server_sock = -1;
        u_short port = 8080; // 默认端口 8080
        int client_sock = -1;
        struct sockaddr_in client_name;
        socklen_t client_name_len = sizeof(client_name);

        // 解析命令行参数
        if (argc > 1) {
            int port_arg = atoi(argv[1]);
            if (port_arg <= 0 || port_arg > 65535) {
                port = 8080;
            } else {
                port = (u_short)port_arg;
            }
        }

        server_sock = startup(&port);
        printf("httpd running on port %d\n", port);

        // 创建线程池，4个工作线程，队列大小1000
        threadpool_init(4, 1000, handle_client);

        while (1) {
            client_sock = accept(server_sock,
                                 (struct sockaddr *)&client_name,
                                 &client_name_len);
            if (client_sock == -1) {
                // 处理瞬时错误，避免服务退出
                if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                    continue;
                }
                error_die("accept");
            }

            // 提交任务到线程池，而不是创建新线程
            if (threadpool_submit(client_sock) != 0) {
                close(client_sock);
            }
        }

        threadpool_shutdown();
        close(server_sock);
        return 0;
    }

4. 修改 accept_request 函数签名：

    // 旧签名（需要修改）：
    // void accept_request(int *client_ptr);
    
    // 新签名（直接接收 client_fd）：
    void accept_request(int client);

5. 删除 pthread 相关代码（不再需要）：

    // 删除这两行：
    // pthread_t newthread;
    // if (pthread_create(&newthread, NULL, (void *)accept_request, (void *)(intptr_t)client_sock) != 0)

三、关键改动对比
--------------------------------------------------------------------------------

原来的代码（每个请求创建一个线程）：

    pthread_t newthread;
    while (1) {
        client_sock = accept(...);
        if (pthread_create(&newthread, NULL,
            (void *)accept_request,
            (void *)(intptr_t)client_sock) != 0)
            perror("pthread_create");
    }

使用线程池后：

    threadpool_init(4, 1000, handle_client);
    while (1) {
        client_sock = accept(...);
        if (threadpool_submit(client_sock) != 0) {
            close(client_sock);
        }
    }

四、内存管理说明
--------------------------------------------------------------------------------

线程池使用堆分配传递 client_fd：

    task_create():
        - 分配 task 结构体 (malloc)
        - 直接存储 client_fd (int)

    worker():
        - 调用 handler(client_fd) 处理请求
        - 调用 task_destroy() 释放 task 结构体

    handle_client(int client_fd):
        - 直接使用 client_fd 值

    accept_request(int client):
        - 直接使用 client 值
        - 函数结束时自动释放栈变量

五、编译测试
--------------------------------------------------------------------------------

    make clean
    make
    ./httpd [端口号]  # 默认 8080

例如：

    ./httpd 8080  # 在 8080 端口启动
    ./httpd 8888  # 在 8888 端口启动

================================================================================