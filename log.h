#ifndef TINYHTTPD_LOG_H
#define TINYHTTPD_LOG_H

/**
 * Write one access log entry to logs/access.log.
 *
 * The function is thread-safe and appends one complete log line containing
 * timestamp, client IP, HTTP method, URL, and response status code.
 */
void log_access(const char *client_ip, const char *method,
                const char *url, int status_code);

/**
 * Write one formatted error log entry to logs/error.log.
 *
 * The function is thread-safe and prefixes each message with a timestamp and
 * "ERROR:". The format string follows printf-style formatting rules.
 */
void log_error_message(const char *fmt, ...);

#endif
