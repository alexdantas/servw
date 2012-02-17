/**
 * @file http.c
 *
 * Implementacao dos procedimentos relacionados a parsear HTTP.
 *
 * Por ora, criei um parser extremamente simples, para parsear e construir
 * mensagens HTTP de uma forma bem restrita.
 * @todo Tornar o parser mais generalizado. (MUITO TRABALHO)
 */

#include <string.h>
#include <ctype.h>
#include "http.h"

#define PROTOCOL "HTTP/1.0"
#define PACKAGE_NAME PACKAGE"/"VERSION



/** Constroi e atribui o header HTTP ao 'buf' (respeitando 'bufsize').
 *
 *  A mensagem e construida de acordo com os parametros.
 *  @todo Remover valores arbitrarios dos buffers.
 *  @return O numero de caracteres efetivamente atribuidos a 'buf' ou
 *          -1 em erro.
 */
int http_build_header(struct c_handler* h)
{
  int n;

  //~ char last_modif[BUFFER_SIZE];
  //LIDAR COM O TEMPO!!!!!!
  //strftime (timebuf)

  n = snprintf(h->answer_header, h->answer_header_size,
                                          "%s %d %s\r\n"
                                          "Server: %s\r\n"
                                          "Content-Type: %s\r\n"
                                          "Content-Length: %d\r\n"
                                          //~ "Last-Modified: %s"
                                          "Connection: close\r\n"
                                          "\r\n",
                                          PROTOCOL, h->filestatus, h->filestatusmsg,
                                          PACKAGE_NAME,
                                          h->filetype,
                                          h->filesize);
  if (!find_crlf(h->answer_header))
    return -1;

  return n;
}



/** Atribui ao 'buf' a resposta em HTML para o erro 'status'
 *  (respeitando 'bufsize');
 */
int build_error_html(char* buf, size_t bufsize, int status, char* status_msg)
{
  int n = snprintf(buf, bufsize, "<html><head>\n"
                                "  <title>Error %d</title>\n"
                                "</head><body>\n"
                                "  <h1>Error %d - %s</h1>\n"
                                "  <hr><pre>%s</pre>\n"
                                "</body></html>\n",
                                status,
                                status, status_msg,
                                PACKAGE_NAME);
  // verificar se deu buffer overflow
  return n;
}


/** Retorna o valor do metodo presente na string #method.
 *
 *  @note Os valores retornados estao definidos em http.h (enum methods).
 */
int http_what_method(char *method, size_t size)
{
  if (size < 3)
    return UNKNOWN_M;

  if (strncmp(method, "GET", 3) == 0)
    return GET_M;
  if (strncmp(method, "PUT", 3) == 0)
    return PUT_M;
  if (strncmp(method, "HEAD", 4) == 0)
    return HEAD_M;
  if (strncmp(method, "POST", 4) == 0)
    return POST_M;
  if (strncmp(method, "TRACE", 5) == 0)
    return TRACE_M;
  if (strncmp(method, "DELETE", 6) == 0)
    return DELETE_M;
  if (strncmp(method, "OPTIONS", 7) == 0)
    return OPTIONS_M;
  if (strncmp(method, "CONNECT", 7) == 0)
    return CONNECT_M;

  return UNKNOWN_M;
}


int http_what_version(char *string, size_t size)
{
  switch(size)
  {
  case 8:
    if (strcmp(string, "HTTP/1.0") == 0)
      return HTTP_1_0;
    if (strcmp(string, "HTTP/1.1") == 0)
      return HTTP_1_1;
    break;

  default:
    break;
  }
  return UNKNOWN_V;
}


/** Armazena a mensagem de status HTTP equivalente ao numero 'status'
 *  em 'buff', respeitando 'buffsize'.
 *
 *  @return O tamanho da string de mensagem.
 */
int http_get_status_msg(int status, char* buff, size_t buffsize)
{
  char *msg;
  switch (status)
  {
  case OK_S:
    msg = "OK";
    break;
  case CREATED_S:
    msg = "Created";
    break;

  case BAD_REQUEST_S:
    msg = "Bad Request";
    break;
  case FORBIDDEN_S:
    msg = "Forbidden";
    break;
  case NOT_FOUND_S:
    msg = "Not Found";
    break;
  case REQUEST_URI_TOO_LARGE_S:
    msg = "Request-Uri Too Large";
    break;

  case SERVER_ERROR_S:
    msg = "Server Error";
    break;

  default:
    msg = "Unknown";
    break;
  }
  strncpy(buff, msg, buffsize);
  return strlen(buff);
}


// TODO TODO TODO
int http_get_file_type(char* file, size_t filesize, char *buff, size_t buffsize)
{
  //chegar ao ultimo '/'
  //procurar pelo ultimo '.'
  //comparar string

  //por enquanto...
  strncpy(buff, "text/html", buffsize);
  return 0;
}

/**
 *  @return 1 se for caso de erro, 0 se for um status normal ou intermediario.
 */
int http_status_is_error(int status)
{
  if (status >= BAD_REQUEST_S)
    return 1;
  else
    return 0;
}
