#ifndef TINYHTTPD_CGI_H
#define TINYHTTPD_CGI_H

int handle_cgi(int client, const char *path, const char *method, const char *query_string, int is_head, int content_length);
int execute_cgi(int client, const char *path, const char *method, const char *query_string, int is_head, int req_content_length);

#endif
