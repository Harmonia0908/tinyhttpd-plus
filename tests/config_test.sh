#!/bin/bash

set -e

CONFIG_FILE="config/server.conf"
TMP_DIR=$(mktemp -d "${TMPDIR:-/tmp}/tinyhttpd-config.XXXXXX")
CONFIG_BACKUP="$TMP_DIR/server.conf.bak"
CONFIG_ROOT="$TMP_DIR/config_root"
SERVER_LOG="$TMP_DIR/server.log"
LOG_BACKUP="$TMP_DIR/logs.bak"
SERVER_PID=""
HAD_CONFIG_DIR=0
HAD_CONFIG_FILE=0
HAD_LOG_PATH=0

cleanup() {
    stop_server
    rm -rf "$CONFIG_ROOT"
    if [ "$HAD_CONFIG_FILE" -eq 1 ]; then
        mv "$CONFIG_BACKUP" "$CONFIG_FILE"
    else
        rm -f "$CONFIG_FILE" "$CONFIG_BACKUP"
    fi
    if [ "$HAD_CONFIG_DIR" -eq 0 ]; then
        rmdir config 2>/dev/null || true
    fi
    rm -rf logs
    if [ "$HAD_LOG_PATH" -eq 1 ]; then
        mv "$LOG_BACKUP" logs
    fi
    rm -rf "$TMP_DIR"
}

fail() {
    echo "FAIL: $1"
    exit 1
}

require_command() {
    command -v "$1" >/dev/null 2>&1 || fail "$1 is required"
}

backup_config() {
    if [ -d config ]; then
        HAD_CONFIG_DIR=1
    else
        mkdir config
    fi

    if [ -f "$CONFIG_FILE" ]; then
        HAD_CONFIG_FILE=1
        cp "$CONFIG_FILE" "$CONFIG_BACKUP"
    fi
}

backup_logs() {
    if [ -e logs ] || [ -L logs ]; then
        HAD_LOG_PATH=1
        mv logs "$LOG_BACKUP"
    fi
}

stop_server() {
    if [ -n "$SERVER_PID" ]; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
        SERVER_PID=""
    fi
}

wait_for_server() {
    local port="$1"
    local i

    for i in $(seq 1 30); do
        if curl --noproxy '*' -s -m 1 "http://127.0.0.1:$port/" >/dev/null 2>&1; then
            return 0
        fi
        sleep 0.2
    done
    return 1
}

start_server() {
    local port="$1"
    shift

    ./httpd "$@" > "$SERVER_LOG" 2>&1 &
    SERVER_PID=$!
    wait_for_server "$port" || {
        cat "$SERVER_LOG"
        fail "server did not start on port $port"
    }
}

assert_response_contains() {
    local port="$1"
    local expected="$2"
    local output

    output=$(curl --noproxy '*' -s -i -m 5 "http://127.0.0.1:$port/" 2>&1)
    if ! echo "$output" | grep -q "$expected"; then
        echo "Expected response to contain: $expected"
        echo "Actual response:"
        echo "$output"
        exit 1
    fi
}

write_config() {
    cat > "$CONFIG_FILE"
}

trap cleanup EXIT

backup_config
backup_logs

require_command curl
require_command grep

if [ "${SKIP_BUILD:-0}" != "1" ]; then
    make clean
    make
fi

rm -f "$CONFIG_FILE"
start_server 8080
assert_response_contains 8080 "HTTP/1.0 200 OK"
stop_server
echo "PASS: missing config uses defaults"

write_config <<EOF
port=18083
thread_num=2
root_dir=./htdocs
enable_access_log=1
enable_error_log=1
EOF
start_server 18083
assert_response_contains 18083 "HTTP/1.0 200 OK"
stop_server
echo "PASS: custom port works"

mkdir -p "$CONFIG_ROOT"
printf 'CONFIG-ROOT-OK\n' > "$CONFIG_ROOT/index.html"
write_config <<EOF
port=18084
thread_num=2
root_dir=$CONFIG_ROOT
enable_access_log=1
enable_error_log=1
EOF
start_server 18084
assert_response_contains 18084 "CONFIG-ROOT-OK"
stop_server
echo "PASS: custom root_dir works"

write_config <<EOF
port=invalid
thread_num=101
root_dir=
enable_access_log=maybe
enable_error_log=2
unknown_key=value
bad_line_without_equals
EOF
start_server 18085 18085
assert_response_contains 18085 "HTTP/1.0 200 OK"
stop_server
echo "PASS: invalid config does not crash"

echo "PASS: configuration tests completed"
