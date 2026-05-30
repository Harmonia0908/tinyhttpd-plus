#!/bin/bash

set -e

PORT="${PORT:-18086}"
SERVER_PID=""
CONFIG_FILE="config/server.conf"
CONFIG_BACKUP="config/server.conf.mimetest.bak"
HAD_CONFIG_DIR=0
HAD_CONFIG_FILE=0
TEST_FILES="
htdocs/mime_test.html
htdocs/mime_test.css
htdocs/mime_test.js
htdocs/mime_test.png
htdocs/mime_test.unknown
"

cleanup() {
    if [ -n "$SERVER_PID" ]; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
    rm -f $TEST_FILES
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

headers_for() {
    local path="$1"
    curl --noproxy '*' -s -D - -o /dev/null -m 5 "http://127.0.0.1:$PORT$path"
}

assert_header() {
    local path="$1"
    local expected="$2"
    local output

    output=$(headers_for "$path")
    if ! echo "$output" | grep -qi "$expected"; then
        echo "Expected header '$expected' for $path"
        echo "Actual headers:"
        echo "$output"
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

printf '<!doctype html><title>mime</title>\n' > htdocs/mime_test.html
printf 'body { color: #111; }\n' > htdocs/mime_test.css
printf 'console.log("mime");\n' > htdocs/mime_test.js
printf '\211PNG\r\n\032\n' > htdocs/mime_test.png
printf 'unknown\n' > htdocs/mime_test.unknown

./httpd "$PORT" > server.log 2>&1 &
SERVER_PID=$!
wait_for_server || {
    cat server.log
    fail "server did not start"
}

assert_header "/mime_test.html" '^Content-Type: text/html'
assert_header "/mime_test.css" '^Content-Type: text/css'
assert_header "/mime_test.js" '^Content-Type: application/javascript'
assert_header "/mime_test.png" '^Content-Type: image/png'
assert_header "/mime_test.unknown" '^Content-Type: application/octet-stream'
assert_header "/not-found-for-mime-test" '^Content-Type: text/html'
assert_header "/not-found-for-mime-test" '^Content-Length: [0-9][0-9]*'

echo "PASS: MIME tests completed"
