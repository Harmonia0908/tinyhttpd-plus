#include "http_parser.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define ISspace(x) isspace((unsigned char)(x))
#define MAX_REQUEST_LINE_SIZE 1024

int http_parse_request_line(const char *data, size_t length,
                            char *method, size_t method_size,
                            char *url, size_t url_size)
{
 size_t line_length = 0;
 size_t normalized_length;
 size_t i;
 size_t j;
 int has_line_ending = 0;

 if (data == NULL || method == NULL || url == NULL ||
     method_size < 2 || url_size < 2)
  return -1;

 method[0] = '\0';
 url[0] = '\0';

 while (line_length < length && data[line_length] != '\r' &&
        data[line_length] != '\n')
  line_length++;
 if (line_length < length)
  has_line_ending = 1;

 normalized_length = line_length + (has_line_ending ? 1U : 0U);
 if (normalized_length == 0)
  return -1;
 if (normalized_length > MAX_REQUEST_LINE_SIZE - 1 ||
     (!has_line_ending && normalized_length == MAX_REQUEST_LINE_SIZE - 1))
  return 400;

 i = 0;
 j = 0;
 while (j < line_length && !ISspace(data[j]))
 {
  if (i + 1 >= method_size)
   return 501;
  method[i++] = data[j++];
 }
 method[i] = '\0';

 if (method[0] == '\0' ||
     (strcasecmp(method, "GET") != 0 &&
      strcasecmp(method, "POST") != 0 &&
      strcasecmp(method, "HEAD") != 0 &&
      strcasecmp(method, "OPTIONS") != 0))
  return 501;

 while (j < line_length && ISspace(data[j]))
  j++;

 i = 0;
 while (j < line_length && !ISspace(data[j]))
 {
  if (i + 1 >= url_size)
   return 414;
  url[i++] = data[j++];
 }
 url[i] = '\0';

 if (url[0] == '\0' || url[0] != '/')
  return 400;

 return 0;
}

int http_parse_headers(const char *data, size_t length, int *content_length)
{
 size_t offset = 0;
 size_t header_size = 0;
 int content_length_seen = 0;

 if (data == NULL && length != 0)
  return -1;
 if (content_length != NULL)
  *content_length = -1;

 while (offset < length)
 {
  char line[MAX_HEADER_LINE_SIZE];
  size_t line_start = offset;
  size_t line_length;
  size_t normalized_length;
  int has_line_ending = 0;

  while (offset < length && data[offset] != '\r' && data[offset] != '\n')
   offset++;
  line_length = offset - line_start;

  if (offset < length)
  {
   has_line_ending = 1;
   if (data[offset] == '\r' && offset + 1 < length &&
       data[offset + 1] == '\n')
    offset += 2;
   else
    offset++;
  }

  normalized_length = line_length + (has_line_ending ? 1U : 0U);
  if (normalized_length > MAX_HEADER_LINE_SIZE - 1 ||
      (!has_line_ending && normalized_length == MAX_HEADER_LINE_SIZE - 1))
   return 400;

  header_size += normalized_length;
  if (header_size > MAX_HEADER_SIZE)
   return 413;

  if (line_length == 0)
   break;

  memcpy(line, data + line_start, line_length);
  line[line_length] = '\0';

  if (content_length != NULL &&
      strncasecmp(line, "Content-Length:", 15) == 0)
  {
   char *value = line + 15;
   char *end;
   long parsed_length;

   if (content_length_seen)
    return 400;
   content_length_seen = 1;

   while (ISspace(*value))
    value++;

   errno = 0;
   parsed_length = strtol(value, &end, 10);
   while (*end == ' ' || *end == '\t')
    end++;

   if (value == end || errno == ERANGE || parsed_length < 0 || *end != '\0')
    return 400;
   if (parsed_length > MAX_REQUEST_SIZE)
    return 413;

   *content_length = (int)parsed_length;
  }
 }

 return 0;
}
