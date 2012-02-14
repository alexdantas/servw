/**
 * @file http.h
 *
 * Definicao dos procedimentos relacionados a parsear HTTP.
 */

#ifndef HTTP_H_DEFINED
#define HTTP_H_DEFINED


/* Informacoes e coisas sobre o protocolo HTTP */

/** Os status codes HTTP.
 *
 * 1xx - Informational Message
 * 2xx - Success
 * 3xx - Redirection
 * 4xx - Client Error
 * 5xx - Server Error
 */
enum status_codes
{
  NONE              = -1,

  OK                = 200,
  CREATED           = 201,

  BAD_REQUEST       = 400,
  FORBIDDEN         = 403,
  NOT_FOUND         = 404,

  SERVER_ERROR      = 500
};

enum http_methods
{
  UNKNOWN_M = -1,
  GET_M,
  HEAD_M,
  POST_M,
  PUT_M,
  OPTIONS_M,
  DELETE_M,
  TRACE_M,
  CONNECT_M
};

enum http_versions
{
  UNKNOWN_V = -1,
  HTTP_1_0,
  HTTP_1_1
};


int http_get_status_msg(int status, char* buff, size_t buffsize);
int http_what_method(char *method, size_t size);
int http_what_version(char *string, size_t);


#endif /* HTTP_H_DEFINED */
