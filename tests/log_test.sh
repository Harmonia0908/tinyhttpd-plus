#!/bin/bash

set -e

PORT="${PORT:-18082}"
SERVER_PID=""
FAIL_CGI="htdocs/log_fail.cgi"
CONFIG_FILE="config/server.conf"
CONFIG_BACKUP="config/server.conf.logtest.bak"
HAD_CONFIG_DIR=0
HAD_CONFIG_FILE=0

cleanup() {
    if [ -n "$SERVER_PID" ]; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    rm -f "$FAIL_CGI"
    if [ "$HAD_CONFIG_FILE" -eq 1 ]; then
        mv "$CONFIG_BACKUP" "$CONFIG_FILE"
    else
        rm -f "$CONFIG_FILE" "$CONFIG_BACKUP"
    fi
    if [ "$HAD_CONFIG_DIR" -eq 0 ]; then
        rmdir config 2>/dev/null || true
    fi
}

fail() {
    echo "FAIL: $1"
    exit 1
}

require_command() {
    command -v "$1" >/dev/null 2>&1 || fail "$1 is required"
}

wait_for_server() {
    local i
    for i in $(seq 1 30); do
        if curl --noproxy '*' -s -m 1 "http://127.0.0.1:$PORT/" >/dev/null 2>&1; then
            return 0
        fi
        sleep 0.2
    done
    return 1
}

assert_log_contains() {
    local file="$1"
    local pattern="$2"

    if ! grep -E "$pattern" "$file" >/dev/null 2>&1; then
        echo "Expected pattern not found: $pattern"
        echo "File content:"
        cat "$file" 2>/dev/null || true
        exit 1
    fi
}

trap cleanup EXIT

require_command curl
require_command grep

make clean && make

if [ -d config ]; then
    HAD_CONFIG_DIR=1
else
    mkdir config
fi
if [ -f "$CONFIG_FILE" ]; then
    HAD_CONFIG_FILE=1
    cp "$CONFIG_FILE" "$CONFIG_BACKUP"
fi
cat > "$CONFIG_FILE" <<EOF
port=$PORT
thread_num=4
root_dir=./htdocs
enable_access_log=1
enable_error_log=1
EOF

rm -rf logs
mkdir -p logs

printf '%s\n' \
    '#!/bin/sh' \
    'exit 1' \
    > "$FAIL_CGI"
chmod +x "$FAIL_CGI"

./httpd "$PORT" > server.log 2>&1 &
SERVER_PID=$!

wait_for_server || fail "server did not start"

curl --noproxy '*' -s -i -m 5 "http://127.0.0.1:$PORT/" >/tmp/tinyhttpd-log-200.out
curl --noproxy '*' -s -i -m 5 "http://127.0.0.1:$PORT/not-found-for-log-test.html" >/tmp/tinyhttpd-log-404.out
curl --noproxy '*' -s -i -m 5 "http://127.0.0.1:$PORT/date.cgi" >/tmp/tinyhttpd-log-cgi.out
curl --noproxy '*' -s -i -m 8 "http://127.0.0.1:$PORT/log_fail.cgi" >/tmp/tinyhttpd-log-cgi-fail.out

test -f logs/access.log || fail "logs/access.log was not created"
test -f logs/error.log || fail "logs/error.log was not created"

assert_log_contains logs/access.log '^\[[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}\] 127\.0\.0\.1 "GET /" 200$'
assert_log_contains logs/access.log '127\.0\.0\.1 "GET /not-found-for-log-test\.html" 404$'
assert_log_contains logs/access.log '127\.0\.0\.1 "GET /date\.cgi" 200$'
assert_log_contains logs/access.log '127\.0\.0\.1 "GET /log_fail\.cgi" 500$'
assert_log_contains logs/error.log 'ERROR: File not found: .*not-found-for-log-test\.html'
assert_log_contains logs/error.log 'ERROR: CGI execution failed: .*log_fail\.cgi'

echo "PASS: logging tests completed"
