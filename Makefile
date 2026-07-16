.DEFAULT_GOAL := all

CC ?= cc
BUILD_TYPE ?= default

PROJECT_CPPFLAGS = -D_XOPEN_SOURCE=700 -Iinclude
WARNINGS = -std=c17 -Wall -Wextra -Wpedantic -Wconversion -Wshadow
DEPFLAGS = -MMD -MP
LDLIBS += -pthread

DEFAULT_CFLAGS = -O2 -g
DEBUG_CFLAGS = -O0 -g3
RELEASE_CFLAGS = -O2 -g0
SANITIZER_FLAGS = -fsanitize=address,undefined -fno-omit-frame-pointer

ifeq ($(BUILD_TYPE),debug)
PROFILE_CFLAGS = $(DEBUG_CFLAGS)
else ifeq ($(BUILD_TYPE),release)
PROFILE_CFLAGS = $(RELEASE_CFLAGS)
else ifeq ($(BUILD_TYPE),sanitizer)
PROFILE_CFLAGS = -O1 -g $(SANITIZER_FLAGS)
else ifeq ($(BUILD_TYPE),default)
PROFILE_CFLAGS = $(DEFAULT_CFLAGS)
else
$(error unsupported BUILD_TYPE '$(BUILD_TYPE)'; expected default, debug, release, or sanitizer)
endif

CFLAGS ?= $(PROFILE_CFLAGS)
COMPILE_FLAGS = $(CFLAGS) $(WARNINGS)

BUILD_DIR = build/$(BUILD_TYPE)
OBJ_DIR = $(BUILD_DIR)/obj
BIN_DIR = $(BUILD_DIR)/bin
PROFILE_TEST_DIR = $(BUILD_DIR)/tests
TEST_DIR = build/tests

APP_SRCS = src/app/main.c src/app/config.c
SERVER_SRCS = src/server/server.c
HTTP_SRCS = src/http/request.c src/http/parser.c src/http/response.c \
	src/http/response_writer.c src/http/static_file.c src/http/mime.c \
	src/http/resource.c
CGI_SRCS = src/cgi/cgi.c
CONCURRENCY_SRCS = src/concurrency/threadpool.c
NET_SRCS = src/net/io.c
COMMON_SRCS = src/common/fd_lifecycle.c src/common/log.c

HTTPD_SRCS = $(APP_SRCS) $(SERVER_SRCS) $(HTTP_SRCS) $(CGI_SRCS) \
	$(CONCURRENCY_SRCS) $(NET_SRCS) $(COMMON_SRCS)
CLIENT_SRCS = tools/simpleclient.c
BENCHMARK_SRCS = tools/benchmark.c

REQUEST_TEST_SRCS = tests/request_test.c src/http/request.c src/http/parser.c \
	src/http/response.c src/http/response_writer.c src/http/static_file.c \
	src/http/mime.c src/http/resource.c src/cgi/cgi.c src/net/io.c \
	src/common/fd_lifecycle.c src/common/log.c src/app/config.c
CGI_TEST_SRCS = tests/cgi_test.c src/cgi/cgi.c src/http/response.c \
	src/http/response_writer.c src/http/mime.c src/net/io.c \
	src/common/fd_lifecycle.c src/common/log.c src/app/config.c
THREADPOOL_TEST_SRCS = tests/threadpool_test.c src/concurrency/threadpool.c
HTTP_PARSER_TEST_SRCS = tests/http_parser_test.c src/http/parser.c
HTTP_RESPONSE_TEST_SRCS = tests/http_response_test.c src/http/response.c \
	src/http/response_writer.c src/http/mime.c src/net/io.c
CGI_FD_PROBE_SRCS = tests/cgi_fd_probe.c

objects = $(addprefix $(OBJ_DIR)/,$(1:.c=.o))

HTTPD_OBJS = $(call objects,$(HTTPD_SRCS))
CLIENT_OBJS = $(call objects,$(CLIENT_SRCS))
BENCHMARK_OBJS = $(call objects,$(BENCHMARK_SRCS))
REQUEST_TEST_OBJS = $(call objects,$(REQUEST_TEST_SRCS))
CGI_TEST_OBJS = $(call objects,$(CGI_TEST_SRCS))
THREADPOOL_TEST_OBJS = $(call objects,$(THREADPOOL_TEST_SRCS))
HTTP_PARSER_TEST_OBJS = $(call objects,$(HTTP_PARSER_TEST_SRCS))
HTTP_RESPONSE_TEST_OBJS = $(call objects,$(HTTP_RESPONSE_TEST_SRCS))
CGI_FD_PROBE_OBJS = $(call objects,$(CGI_FD_PROBE_SRCS))

ALL_OBJS = $(sort $(HTTPD_OBJS) $(CLIENT_OBJS) $(BENCHMARK_OBJS) \
	$(REQUEST_TEST_OBJS) $(CGI_TEST_OBJS) $(THREADPOOL_TEST_OBJS) \
	$(HTTP_PARSER_TEST_OBJS) $(HTTP_RESPONSE_TEST_OBJS) $(CGI_FD_PROBE_OBJS))
ALL_DEPS = $(ALL_OBJS:.o=.d)

UNIT_TESTS = $(TEST_DIR)/request_test $(TEST_DIR)/cgi_test \
	$(TEST_DIR)/cgi_fd_probe $(TEST_DIR)/threadpool_test \
	$(TEST_DIR)/http_parser_test $(TEST_DIR)/http_response_test

SANITIZER_ENV = ASAN_OPTIONS=halt_on_error=1 \
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1

.PHONY: all clean debug release help test unit-test sanitizers \
	sanitizer-test threadpool FORCE

all: httpd client benchmark

httpd: $(BIN_DIR)/httpd FORCE
	cp $< $@

$(BIN_DIR)/httpd: $(HTTPD_OBJS) | $(BIN_DIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

client: $(BIN_DIR)/client FORCE
	cp $< $@

$(BIN_DIR)/client: $(CLIENT_OBJS) | $(BIN_DIR)
	$(CC) $(LDFLAGS) -o $@ $^

benchmark: $(BIN_DIR)/benchmark FORCE
	cp $< $@

$(BIN_DIR)/benchmark: $(BENCHMARK_OBJS) | $(BIN_DIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

threadpool: threadpool.o

threadpool.o: $(OBJ_DIR)/src/concurrency/threadpool.o FORCE
	cp $< $@

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(PROJECT_CPPFLAGS) $(CPPFLAGS) $(COMPILE_FLAGS) $(DEPFLAGS) -c $< -o $@

$(BIN_DIR) $(PROFILE_TEST_DIR) $(TEST_DIR):
	mkdir -p $@

$(PROFILE_TEST_DIR)/request_test: $(REQUEST_TEST_OBJS) | $(PROFILE_TEST_DIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(PROFILE_TEST_DIR)/cgi_test: $(CGI_TEST_OBJS) | $(PROFILE_TEST_DIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(PROFILE_TEST_DIR)/cgi_fd_probe: $(CGI_FD_PROBE_OBJS) | $(PROFILE_TEST_DIR)
	$(CC) $(LDFLAGS) -o $@ $^

$(PROFILE_TEST_DIR)/threadpool_test: $(THREADPOOL_TEST_OBJS) | $(PROFILE_TEST_DIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(PROFILE_TEST_DIR)/http_parser_test: $(HTTP_PARSER_TEST_OBJS) | $(PROFILE_TEST_DIR)
	$(CC) $(LDFLAGS) -o $@ $^

$(PROFILE_TEST_DIR)/http_response_test: $(HTTP_RESPONSE_TEST_OBJS) | $(PROFILE_TEST_DIR)
	$(CC) $(LDFLAGS) -o $@ $^

$(TEST_DIR)/request_test: $(PROFILE_TEST_DIR)/request_test FORCE | $(TEST_DIR)
	cp $< $@

$(TEST_DIR)/cgi_test: $(PROFILE_TEST_DIR)/cgi_test FORCE | $(TEST_DIR)
	cp $< $@

$(TEST_DIR)/cgi_fd_probe: $(PROFILE_TEST_DIR)/cgi_fd_probe FORCE | $(TEST_DIR)
	cp $< $@

$(TEST_DIR)/threadpool_test: $(PROFILE_TEST_DIR)/threadpool_test FORCE | $(TEST_DIR)
	cp $< $@

$(TEST_DIR)/http_parser_test: $(PROFILE_TEST_DIR)/http_parser_test FORCE | $(TEST_DIR)
	cp $< $@

$(TEST_DIR)/http_response_test: $(PROFILE_TEST_DIR)/http_response_test FORCE | $(TEST_DIR)
	cp $< $@

unit-test: $(UNIT_TESTS)
	$(TEST_DIR)/request_test
	$(TEST_DIR)/cgi_test $(abspath $(TEST_DIR)/cgi_fd_probe)
	$(TEST_DIR)/threadpool_test
	$(TEST_DIR)/http_parser_test
	$(TEST_DIR)/http_response_test

test: all unit-test
	SKIP_BUILD=1 tests/run_integration_tests.sh
	SKIP_BUILD=1 tests/config_test.sh
	SKIP_BUILD=1 tests/log_test.sh
	SKIP_BUILD=1 tests/mime_test.sh

debug:
	$(MAKE) clean
	$(MAKE) BUILD_TYPE=debug all

release:
	$(MAKE) clean
	$(MAKE) BUILD_TYPE=release all

sanitizers:
	$(MAKE) clean
	$(MAKE) BUILD_TYPE=sanitizer \
		LDFLAGS="$(SANITIZER_FLAGS)" all $(UNIT_TESTS)

sanitizer-test: sanitizers
	$(SANITIZER_ENV) $(TEST_DIR)/request_test
	$(SANITIZER_ENV) $(TEST_DIR)/cgi_test $(abspath $(TEST_DIR)/cgi_fd_probe)
	$(SANITIZER_ENV) $(TEST_DIR)/threadpool_test
	$(SANITIZER_ENV) $(TEST_DIR)/http_parser_test
	$(SANITIZER_ENV) $(TEST_DIR)/http_response_test
	$(SANITIZER_ENV) SKIP_BUILD=1 tests/run_integration_tests.sh
	$(SANITIZER_ENV) SKIP_BUILD=1 tests/config_test.sh
	$(SANITIZER_ENV) SKIP_BUILD=1 tests/log_test.sh
	$(SANITIZER_ENV) SKIP_BUILD=1 tests/mime_test.sh

help:
	@printf '%s\n' \
		'make                 Build httpd, client, and benchmark (compatible default)' \
		'make debug           Clean and build with -O0 -g3' \
		'make release         Clean and build with -O2 -g0' \
		'make unit-test       Build and run unit tests' \
		'make test            Run unit and integration tests' \
		'make sanitizer-test  Run tests with ASan and UBSan' \
		'make clean           Remove generated files'

clean:
	rm -rf httpd client benchmark threadpool *.o build

FORCE:

-include $(ALL_DEPS)
