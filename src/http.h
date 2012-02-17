/**
 * @file http.h
 *
 * Definicao dos procedimentos relacionados a HTTP.
 * Informacoes, status codes, metodos e funcoes que parseiam HTTP.
 */

#ifndef HTTP_H_DEFINED
#define HTTP_H_DEFINED

#include "client.h"

/* Informacoes e coisas sobre o protocolo HTTP */

/** Valores para os status codes HTTP. Possuem posfixo '_S'.
 *
 * 1xx - Informational Message
 * 2xx - Success
 * 3xx - Redirection
 * 4xx - Client Error
 * 5xx - Server Error
 */
enum status_codes
{
  UNKNOWN_S         = -1,

  OK_S      = 200,
  CREATED_S = 201,

  BAD_REQUEST_S           = 400,
  FORBIDDEN_S             = 403,
  NOT_FOUND_S             = 404,
  REQUEST_URI_TOO_LARGE_S = 414,

  SERVER_ERROR_S = 500
};

/** Valores para os metodos HTTP. Possuem posfixo '_M'.
 */
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

/** Valores para as versoes HTTP. Possuem posfixo '_V'.
 */
enum http_versions
{
  UNKNOWN_V = -1,
  HTTP_1_0,
  HTTP_1_1
};


int http_build_header(struct c_handler* h);
int build_error_html(char* buf, size_t bufsize, int status, char* status_msg);
int http_get_file_type(char* file, size_t filesize, char *buff, size_t buffsize);
/** Diz se o numero de status 'status' e um aviso de erro. */
int http_status_is_error(int status);
int http_get_status_msg(int status, char* buff, size_t buffsize);
int http_what_method(char *method, size_t size);
int http_what_version(char *string, size_t);


#endif /* HTTP_H_DEFINED */
