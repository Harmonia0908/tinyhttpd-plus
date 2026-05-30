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

Expected result: `httpd`, `client`, and `benchmark` are built without compiler errors.

## Full Automated Integration Test

```sh
tests/run_integration_tests.sh
```

The script starts the server on `127.0.0.1:18080` by default, creates temporary fixtures under `htdocs`, runs HTTP requests, and cleans up its test files when it exits.

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

Verified on 2026-05-09 with:

```sh
tests/run_integration_tests.sh
```

Result: all integration tests passed.

Also verified manually in Chrome against `http://127.0.0.1:18082/` and `http://127.0.0.1:18082/date.cgi`.

## Troubleshooting

- If the server cannot start, check whether the port is already in use and rerun with another port.
- If `curl` uses a proxy in your shell, keep `--noproxy '*'` in the command.
- If CGI requests fail, confirm CGI files under `htdocs` are executable.
- If tests fail midway, rerun `tests/run_integration_tests.sh`; it is designed to clean up its temporary files on exit.
