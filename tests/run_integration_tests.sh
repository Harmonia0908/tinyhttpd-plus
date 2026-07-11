#!/bin/bash

# 集成测试脚本
# 自动启动服务器，执行测试，清理进程

set -e

# 颜色输出
GREEN="\033[0;32m"
RED="\033[0;31m"
YELLOW="\033[1;33m"
NC="\033[0m" # No Color

# 端口设置（支持环境变量 PORT）
PORT="${PORT:-18080}"
SERVER_PID=""
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/tinyhttpd-integration.XXXXXX")
SERVER_LOG="$TMP_DIR/server.log"
TEST_BINARY_FILE="htdocs/test_binary.bin"
TEST_STATIC_FILE="htdocs/test_static.txt"
TEST_CRLF_CGI="htdocs/head_crlf.cgi"
TEST_SLEEP_CGI="htdocs/sleep.cgi"
TEST_OUTSIDE_FILE="$TMP_DIR/outside_secret.txt"
TEST_SYMLINK="htdocs/outside_link.txt"
TEST_BINARY_OUT="$TMP_DIR/test_binary.out"

echo -e "${YELLOW}=== Tinyhttpd 集成测试 ===${NC}"
echo -e "测试端口: $PORT"

# 检查依赖
check_dependencies() {
    if ! command -v curl > /dev/null 2>&1; then
        echo -e "${RED}错误: curl 未安装${NC}"
        exit 1
    fi
    if ! command -v nc > /dev/null 2>&1; then
        echo -e "${RED}错误: nc 未安装${NC}"
        exit 1
    fi
    if ! command -v perl > /dev/null 2>&1; then
        echo -e "${RED}错误: perl 未安装${NC}"
        exit 1
    fi
}

# 使用 nc 检查端口是否可用（如果没有 lsof）
is_port_listening() {
    local port=$1
    # 尝试连接到端口
    if command -v nc > /dev/null 2>&1; then
        nc -z 127.0.0.1 $port 2>/dev/null
        return $?
    elif command -v timeout > /dev/null 2>&1; then
        timeout 1 bash -c "echo > /dev/tcp/127.0.0.1/$port" 2>/dev/null
        return $?
    else
        # 使用 curl 作为后备检查
        curl --noproxy '*' -s -m 1 http://127.0.0.1:$port/ > /dev/null 2>&1
        return $?
    fi
}

cleanup() {
    if [ -n "$SERVER_PID" ]; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    rm -f "$TEST_BINARY_FILE" "$TEST_STATIC_FILE" "$TEST_CRLF_CGI" \
          "$TEST_SLEEP_CGI" "$TEST_OUTSIDE_FILE" "$TEST_SYMLINK" \
          "$TEST_BINARY_OUT"
    rm -rf "$TMP_DIR"
}

trap cleanup EXIT

check_dependencies

create_test_fixtures() {
    perl -e 'print "ABC\0DEF\0GHI"' > "$TEST_BINARY_FILE"
    printf 'STATIC-HEAD-BODY\n' > "$TEST_STATIC_FILE"
    printf 'outside secret\n' > "$TEST_OUTSIDE_FILE"
    ln -sf "$TEST_OUTSIDE_FILE" "$TEST_SYMLINK"

    printf '%s\n' \
        '#!/usr/bin/perl' \
        'print "Content-Type: text/plain\r\n\r\nCGI-HEAD-BODY";' \
        > "$TEST_CRLF_CGI"
    chmod +x "$TEST_CRLF_CGI"

    printf '%s\n' \
        '#!/usr/bin/perl' \
        'sleep 10;' \
        'print "Content-Type: text/plain\n\nslow cgi done\n";' \
        > "$TEST_SLEEP_CGI"
    chmod +x "$TEST_SLEEP_CGI"
}

# 编译项目
echo -e "${YELLOW}1. 编译项目${NC}"
if [ "${SKIP_BUILD:-0}" != "1" ]; then
    make clean
    make
fi

create_test_fixtures

# 启动服务器
echo -e "${YELLOW}2. 启动服务器${NC}"

if is_port_listening "$PORT"; then
    echo -e "${RED}端口 $PORT 已被其他进程占用${NC}"
    exit 1
fi

# 后台启动服务器
./httpd "$PORT" > "$SERVER_LOG" 2>&1 &
SERVER_PID=$!

for _ in $(seq 1 30); do
    if is_port_listening "$PORT"; then
        break
    fi
    sleep 0.1
done

# 检查服务器是否启动
echo -e "${YELLOW}3. 检查服务器状态${NC}"
if ! is_port_listening $PORT; then
    echo -e "${RED}服务器启动失败${NC}"
    cat "$SERVER_LOG"
    exit 1
fi
echo -e "${GREEN}服务器启动成功，PID: $SERVER_PID${NC}"

# 测试函数
test_case() {
    local name="$1"
    local command="$2"
    local expected="$3"

    echo -e "\n${YELLOW}测试: $name${NC}"
    echo -e "命令: $command"

    output=$(eval "$command")
    exit_code=$?

    if [ $exit_code -ne 0 ]; then
        echo -e "${RED}❌ 测试失败 (exit code: $exit_code)${NC}"
        echo "输出: $output"
        return 1
    fi

    if echo "$output" | grep -q "$expected"; then
        echo -e "${GREEN}✅ 测试通过${NC}"
        return 0
    else
        echo -e "${RED}❌ 测试失败${NC}"
        echo "期望包含: $expected"
        echo "实际输出: $output"
        return 1
    fi
}

raw_request_test_case() {
    local name="$1"
    local method="$2"
    local request_type="$3"
    local expected="$4"

    echo -e "\n${YELLOW}测试: $name${NC}"

    set +e
    output=$(perl -e '
        my ($method, $request_type) = @ARGV;
        my $path = $method eq "POST" ? "/date.cgi" : "/";

        print "$method $path HTTP/1.1\r\n";
        if ($request_type eq "long-line") {
            print "X-Long: " . ("a" x 1024) . "\r\n\r\n";
        } else {
            print "Host: localhost\r\n";
            print "Content-Length: 0\r\n" if $method eq "POST";
            for my $i (1..9) {
                print "X-$i: " . ("a" x 1000) . "\r\n";
            }
            print "\r\n";
        }
    ' "$method" "$request_type" | nc -w 5 127.0.0.1 "$PORT" 2>&1)
    exit_code=$?
    set -e

    if [ $exit_code -ne 0 ] && [ -z "$output" ]; then
        echo -e "${RED}❌ 测试失败 (exit code: $exit_code)${NC}"
        echo "输出: $output"
        return 1
    fi

    if echo "$output" | grep -q "$expected"; then
        echo -e "${GREEN}✅ 测试通过${NC}"
        return 0
    else
        echo -e "${RED}❌ 测试失败${NC}"
        echo "期望包含: $expected"
        echo "实际输出: $output"
        return 1
    fi
}

pass() {
    echo -e "${GREEN}✅ 测试通过${NC}"
}

fail() {
    echo -e "${RED}❌ 测试失败${NC}"
    echo "$1"
    exit 1
}

test_binary_static_file() {
    echo -e "\n${YELLOW}测试: 静态二进制文件不会因 NUL 字节截断${NC}"
    curl --noproxy '*' -s -m 5 "http://127.0.0.1:$PORT/test_binary.bin" -o "$TEST_BINARY_OUT"
    if cmp -s "$TEST_BINARY_FILE" "$TEST_BINARY_OUT"; then
        pass
    else
        fail "二进制响应与源文件不一致"
    fi
}

test_static_get_content_length() {
    local expected_size
    local output

    echo -e "\n${YELLOW}测试: GET 静态文件包含正确 Content-Length${NC}"
    expected_size=$(wc -c < "$TEST_STATIC_FILE" | tr -d ' ')
    output=$(curl --noproxy '*' -i -m 5 "http://127.0.0.1:$PORT/test_static.txt" 2>&1)

    if echo "$output" | grep -qi "Content-Length: $expected_size"; then
        pass
    else
        fail "期望 Content-Length: $expected_size，实际输出: $output"
    fi
}

test_static_head_content_length_no_body() {
    local expected_size
    local output

    echo -e "\n${YELLOW}测试: HEAD 静态文件包含 Content-Length 且不返回 body${NC}"
    expected_size=$(wc -c < "$TEST_STATIC_FILE" | tr -d ' ')
    output=$(printf 'HEAD /test_static.txt HTTP/1.1\r\nHost: localhost\r\n\r\n' | nc -w 5 127.0.0.1 "$PORT" 2>&1)

    if ! echo "$output" | grep -qi "Content-Length: $expected_size"; then
        fail "期望 Content-Length: $expected_size，实际输出: $output"
    fi
    if echo "$output" | grep -q "STATIC-HEAD-BODY"; then
        fail "HEAD 响应不应包含静态文件 body，实际输出: $output"
    fi
    pass
}

test_cgi_head_crlf_no_body() {
    local output

    echo -e "\n${YELLOW}测试: CGI HEAD 只返回 header 不返回 body (CRLF)${NC}"
    output=$(printf 'HEAD /head_crlf.cgi HTTP/1.1\r\nHost: localhost\r\n\r\n' | nc -w 5 127.0.0.1 "$PORT" 2>&1)

    if ! echo "$output" | grep -q "HTTP/1.0 200 OK"; then
        fail "期望 CGI HEAD 返回 200，实际输出: $output"
    fi
    if echo "$output" | grep -q "CGI-HEAD-BODY"; then
        fail "CGI HEAD 不应包含 body，实际输出: $output"
    fi
    pass
}

test_symlink_escape_forbidden() {
    echo -e "\n${YELLOW}测试: symlink 指向 htdocs 外部文件时返回 403${NC}"
    test_case "symlink escape forbidden" "curl --noproxy '*' -i -m 5 http://127.0.0.1:$PORT/outside_link.txt 2>&1" "HTTP/1.0 403 Forbidden"
}

test_truncated_post_body_returns_400() {
    local output

    echo -e "\n${YELLOW}测试: POST body 截断时返回 400${NC}"
    output=$(perl -e 'print "POST /date.cgi HTTP/1.1\r\nHost: localhost\r\nContent-Length: 10\r\n\r\nabc"' | nc -w 5 127.0.0.1 "$PORT" 2>&1)

    if echo "$output" | grep -q "HTTP/1.0 400 BAD REQUEST"; then
        pass
    else
        fail "期望 400 BAD REQUEST，实际输出: $output"
    fi
}

test_cgi_timeout_releases_worker() {
    local output
    local pids=""

    echo -e "\n${YELLOW}测试: CGI 超时不会让 worker 永久阻塞${NC}"
    for _ in 1 2 3 4; do
        curl --noproxy '*' -s -m 8 "http://127.0.0.1:$PORT/sleep.cgi" >/dev/null 2>&1 &
        pids="$pids $!"
    done

    sleep 1
    set +e
    output=$(curl --noproxy '*' -s -i -m 8 "http://127.0.0.1:$PORT/" 2>&1)
    exit_code=$?
    for pid in $pids; do
        wait "$pid" 2>/dev/null || true
    done
    set -e

    if [ $exit_code -ne 0 ]; then
        fail "普通请求在 CGI 超时释放 worker 前失败，curl exit code: $exit_code，输出: $output"
    fi
    if echo "$output" | grep -q "HTTP/1.0 200 OK"; then
        pass
    else
        fail "期望 worker 释放后普通请求返回 200，实际输出: $output"
    fi
}

# 运行测试
echo -e "${YELLOW}4. 执行测试用例${NC}"

# 1. 基础静态资源
test_case "基础静态资源 GET /" "curl --noproxy '*' -i -m 5 http://127.0.0.1:$PORT/ 2>&1" "HTTP/1.0 200 OK"

# 2. 404
test_case "404 错误" "curl --noproxy '*' -i -m 5 http://127.0.0.1:$PORT/nonexistent.html 2>&1" "HTTP/1.0 404 NOT FOUND"

# 3. Static file regressions
test_binary_static_file
test_static_get_content_length
test_static_head_content_length_no_body
test_symlink_escape_forbidden

# 4. CGI GET
test_case "CGI GET /date.cgi" "curl --noproxy '*' -i -m 5 http://127.0.0.1:$PORT/date.cgi 2>&1" "HTTP/1.0 200 OK"
test_case "CGI GET 有 body" "curl --noproxy '*' -i -m 5 http://127.0.0.1:$PORT/date.cgi 2>&1" "<HTML><BODY>"

# 5. CGI HEAD
test_case "CGI HEAD /date.cgi" "curl --noproxy '*' -I -m 5 http://127.0.0.1:$PORT/date.cgi 2>&1" "HTTP/1.0 200 OK"
# 测试 HEAD 无 body（grep 无匹配时返回 1，需要特殊处理）
echo -e "\n${YELLOW}测试: CGI HEAD 无 body${NC}"
output=$(curl --noproxy '*' -I -m 5 http://127.0.0.1:$PORT/date.cgi 2>&1)
exit_code=$?
if [ $exit_code -ne 0 ]; then
    echo -e "${RED}❌ 测试失败 (exit code: $exit_code)${NC}"
    echo "输出: $output"
    exit 1
fi
# 检查是否包含 body 标签
if echo "$output" | grep -E '(<HTML>|<BODY>)' > /dev/null; then
    echo -e "${RED}❌ 测试失败${NC}"
    echo "期望: 无 body 内容"
    echo "实际: $(echo "$output" | grep -E '(<HTML>|<BODY>)')"
    exit 1
else
    echo -e "${GREEN}✅ 测试通过${NC}"
fi
test_cgi_head_crlf_no_body

# 6. CGI POST
test_case "CGI POST /date.cgi" "curl --noproxy '*' -X POST -d 'a=1' -i -m 5 http://127.0.0.1:$PORT/date.cgi 2>&1" "HTTP/1.0 200 OK"
test_truncated_post_body_returns_400

# 7. OPTIONS
test_case "OPTIONS 方法" "curl --noproxy '*' -i -X OPTIONS -m 5 http://127.0.0.1:$PORT/ 2>&1" "HTTP/1.0 200 OK"
test_case "OPTIONS 有 Allow 头" "curl --noproxy '*' -i -X OPTIONS -m 5 http://127.0.0.1:$PORT/ 2>&1" "Allow: GET, POST, HEAD, OPTIONS"

# 8. Header Size Limit
raw_request_test_case "Header 单行超过缓冲区返回 400" "GET" "long-line" "HTTP/1.0 400 BAD REQUEST"

for method in GET HEAD POST OPTIONS; do
    raw_request_test_case "Header 总大小超过 8KB 返回 413 ($method)" "$method" "large-headers" "HTTP/1.0 413 Payload Too Large"
done

# 9. CGI timeout
test_cgi_timeout_releases_worker

# 清理
echo -e "\n${YELLOW}5. 清理资源${NC}"
cleanup
SERVER_PID=""

# 检查是否成功清理
sleep 1
if is_port_listening $PORT; then
    echo -e "${RED}❌ 测试 server 退出后端口 $PORT 仍被占用${NC}"
    exit 1
fi

echo -e "${GREEN}✅ 所有测试通过！${NC}"
echo -e "${YELLOW}=== 测试完成 ===${NC}"
