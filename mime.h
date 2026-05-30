#ifndef TINYHTTPD_MIME_H
#define TINYHTTPD_MIME_H

/**
 * Return the Content-Type for a filesystem path based on its extension.
 *
 * Unknown or extensionless paths are treated as application/octet-stream.
 */
const char *get_mime_type(const char *path);

#endif
