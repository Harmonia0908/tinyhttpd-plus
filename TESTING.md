# Tinyhttpd Reproducible Test Guide

This document records how to reproduce the current functional test pass for this project. The project is a lightweight HTTP server engineering retrofit based on tinyhttpd, not a production web server.

## Prerequisites

- macOS or Linux with `make` and a C compiler.
- `curl`, `nc`, and `perl` available on `PATH`.
- Chrome is optional and only needed for the browser smoke test.

## Clean Build

```sh
make clean
make
```

Expected result: `httpd`, `client`, and `benchmark` are built with C17 and the configured warning set, without compiler errors or warnings.

## Unit Tests

```sh
make unit-test
```

These tests use `socketpair()` and local processes rather than TCP ports. They cover request/header boundaries, CGI descriptor inheritance, and thread-pool lifecycle behavior.

## Full Automated Integration Test

```sh
make test
```

This builds once, runs the unit tests, then runs all four shell integration suites. They start servers only on `127.0.0.1`, create isolated temporary directories, preserve an existing `config/server.conf`, and clean up processes, ports, fixtures, and output files on exit.

To use a different port:

```sh
PORT=18081 tests/run_integration_tests.sh
```

Current coverage includes:

- `GET /` static HTML success path.
- `404 Not Found` for missing static files.
- Static binary files containing NUL bytes are not truncated.
- Static `GET` includes correct `Content-Length`.
- Static `HEAD` includes `Content-Length` and returns no body.
- Symlinks that resolve outside `htdocs` are rejected.
- CGI `GET`, `POST`, and `HEAD`.
- CGI `HEAD` returns headers only, including CRLF-terminated CGI headers.
- Truncated or abnormal `POST` request bodies return `400`.
- `OPTIONS` returns `Allow` and CORS-related headers.
- Single overlong request/header lines return `400`.
- Total headers over 8 KiB return `413` for `GET`, `HEAD`, `POST`, and `OPTIONS`.
- CGI timeout handling releases workers instead of blocking the pool permanently.
- SIGTERM during a busy CGI worker drains the pool and releases the listening port.
- Overlong URI, missing URI, duplicate `Content-Length`, and maximum body-length boundaries.
- CGI children do not inherit client sockets across `execve()`.
- Thread-pool initialization failures, task draining, processed count, and repeated shutdown.
- Configuration, access/error logging, and MIME mappings.

## Sanitizers

```sh
make sanitizer-test
```

The target rebuilds every executable and unit test with AddressSanitizer and UndefinedBehaviorSanitizer, then runs the same unit and integration suites. Apple Clang does not provide LeakSanitizer on macOS; the Ubuntu CI run uses ASan's Linux defaults, including supported leak detection.

## Continuous Integration

`.github/workflows/ci.yml` runs `make test` and `make sanitizer-test` as separate Ubuntu jobs. The tests do not contact external services; the workflow only installs the local `nc` test utility before running them.

## Manual Server Run

Start the server:

```sh
./httpd 18082
```

In another terminal, run smoke checks:

```sh
curl --noproxy '*' -i http://127.0.0.1:18082/
curl --noproxy '*' -I http://127.0.0.1:18082/
curl --noproxy '*' -i http://127.0.0.1:18082/date.cgi
curl --noproxy '*' -i -X POST -d 'color=blue' http://127.0.0.1:18082/date.cgi
curl --noproxy '*' -i -X OPTIONS http://127.0.0.1:18082/
```

Expected results:

- `/` returns `HTTP/1.0 200 OK`, `Content-Type: text/html`, and a `Content-Length` header.
- `HEAD /` returns headers only.
- `/date.cgi` returns a CGI page containing `Today is:`.
- `POST /date.cgi` returns a CGI response instead of hanging or crashing.
- `OPTIONS /` returns `Allow: GET, HEAD, POST, OPTIONS` plus CORS-related headers.

Stop the server with `Ctrl-C` in the terminal running `./httpd`.

## Chrome Smoke Test

1. Start the server with `./httpd 18082`.
2. Open Chrome.
3. Visit `http://127.0.0.1:18082/`.
4. Confirm the page renders `Welcome to J. David's webserver.` and the CGI demo form.
5. Visit `http://127.0.0.1:18082/date.cgi`.
6. Confirm the page renders `Today is:` and a current date.

## Latest Local Verification

Verified locally on 2026-07-11 with:

```sh
make test
make sanitizer-test
```

Result: strict build, unit tests, all integration suites, ASan, and UBSan passed. On this macOS host, forcing `ASAN_OPTIONS=detect_leaks=1` is unsupported, so the portable target does not force that option.

## Troubleshooting

- If the server cannot start, check whether the port is already in use and rerun with another port.
- If `curl` uses a proxy in your shell, keep `--noproxy '*'` in the command.
- If CGI requests fail, confirm CGI files under `htdocs` are executable.
- If tests fail midway, rerun `tests/run_integration_tests.sh`; it is designed to clean up its temporary files on exit.
