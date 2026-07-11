CC ?= cc
CPPFLAGS += -D_XOPEN_SOURCE=700
CFLAGS ?= -O2 -g
WARNINGS = -std=c17 -Wall -Wextra -Wpedantic -Wconversion -Wshadow
COMPILE_FLAGS = $(CFLAGS) $(WARNINGS)
LDLIBS = -pthread

HTTPD_SRCS = main.c server.c request.c response.c static_file.c cgi.c utils.c \
	log.c config.c mime.c threadpool.c
REQUEST_TEST_SRCS = tests/request_test.c request.c response.c static_file.c \
	cgi.c utils.c log.c config.c mime.c
CGI_TEST_SRCS = tests/cgi_test.c cgi.c response.c utils.c log.c config.c mime.c
TEST_DIR = build/tests
UNIT_TESTS = $(TEST_DIR)/request_test $(TEST_DIR)/cgi_test \
	$(TEST_DIR)/cgi_fd_probe $(TEST_DIR)/threadpool_test
SANITIZER_FLAGS = -fsanitize=address,undefined -fno-omit-frame-pointer
SANITIZER_ENV = ASAN_OPTIONS=halt_on_error=1 \
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

.PHONY: all clean test unit-test sanitizers sanitizer-test threadpool

all: httpd client benchmark

httpd: $(HTTPD_SRCS)
	$(CC) $(CPPFLAGS) $(COMPILE_FLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

client: simpleclient.c
	$(CC) $(CPPFLAGS) $(COMPILE_FLAGS) $(LDFLAGS) -o $@ $^

benchmark: benchmark.c
	$(CC) $(CPPFLAGS) $(COMPILE_FLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

threadpool: threadpool.o

threadpool.o: threadpool.c threadpool.h
	$(CC) $(CPPFLAGS) $(COMPILE_FLAGS) -c $< -o $@

$(TEST_DIR):
	mkdir -p $@

$(TEST_DIR)/request_test: $(REQUEST_TEST_SRCS) | $(TEST_DIR)
	$(CC) $(CPPFLAGS) -I. $(COMPILE_FLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(TEST_DIR)/cgi_test: $(CGI_TEST_SRCS) | $(TEST_DIR)
	$(CC) $(CPPFLAGS) -I. $(COMPILE_FLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(TEST_DIR)/cgi_fd_probe: tests/cgi_fd_probe.c | $(TEST_DIR)
	$(CC) $(CPPFLAGS) $(COMPILE_FLAGS) $(LDFLAGS) -o $@ $^

$(TEST_DIR)/threadpool_test: tests/threadpool_test.c threadpool.c threadpool.h | $(TEST_DIR)
	$(CC) $(CPPFLAGS) -I. $(COMPILE_FLAGS) $(LDFLAGS) -o $@ \
		tests/threadpool_test.c threadpool.c $(LDLIBS)

unit-test: $(UNIT_TESTS)
	$(TEST_DIR)/request_test
	$(TEST_DIR)/cgi_test $(abspath $(TEST_DIR)/cgi_fd_probe)
	$(TEST_DIR)/threadpool_test

test: all unit-test
	SKIP_BUILD=1 tests/run_integration_tests.sh
	SKIP_BUILD=1 tests/config_test.sh
	SKIP_BUILD=1 tests/log_test.sh
	SKIP_BUILD=1 tests/mime_test.sh

sanitizers: clean
	$(MAKE) CFLAGS="-O1 -g $(SANITIZER_FLAGS)" \
		LDFLAGS="$(SANITIZER_FLAGS)" all $(UNIT_TESTS)

sanitizer-test: sanitizers
	$(SANITIZER_ENV) $(TEST_DIR)/request_test
	$(SANITIZER_ENV) $(TEST_DIR)/cgi_test $(abspath $(TEST_DIR)/cgi_fd_probe)
	$(SANITIZER_ENV) $(TEST_DIR)/threadpool_test
	$(SANITIZER_ENV) SKIP_BUILD=1 tests/run_integration_tests.sh
	$(SANITIZER_ENV) SKIP_BUILD=1 tests/config_test.sh
	$(SANITIZER_ENV) SKIP_BUILD=1 tests/log_test.sh
	$(SANITIZER_ENV) SKIP_BUILD=1 tests/mime_test.sh

clean:
	rm -rf httpd client benchmark threadpool *.o build
