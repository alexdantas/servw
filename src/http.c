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

#ifdef TENHO_QUE_CONSERTAR_ISSO_DEPOIS

/** Constroi e atribui o header HTTP ao 'buf' (respeitando 'bufsize').
 *
 *  A mensagem e construida de acordo com os parametros.
 *  @todo Remover valores arbitrarios dos buffers.
 *  @return O numero de caracteres efetivamente atribuidos a 'buf'.
 */
int build_header(char* buf, size_t bufsize, int status, char* status_msg
                 char* cont_type, off_t cont_length, char* last_modif)
{
  int n;

  char status_msg[256];

  //LIDAR COM O TEMPO!!!!!!
  //strftime (timebuf)

//  n = get_status_msg(status, status_msg, 256);
  //if (n == -1)
    //strncpy(status_msg, "Unknown Status Code", 256);
      // FAZER ISSO EM UMA OUTRA FUNCAO QUE VAI ENGLOBAR ESSA E VAI
        // CONSTRUIR TAMBEM TODAS AS OUTRAS COISAS QUE ESSA FUNCAO PRECISA
          // COMO O TIME_T

  n = snprintf(buf, bufsize, "%s %d %s\r\n"
                             "Server: %s\r\n"
                             "Content-Type: %s\r\n"
                             "Content-Length: %d\r\n"
                             "Last-Modified: %s"
                             "Connection: close\r\n"
                             "\r\n",
                             PROTOCOL, status, status_msg,
                             PACKAGE_NAME,
                             cont_type,
                             cont_length,
                             last_modif);
  return n;
}



/** Atribui ao 'buf' a resposta em HTML para o erro 'status'
 *  (respeitando 'bufsize');
 */
int build_error(char* buf, size_t bufsize, int status)
{
  char status_msg[256];
  int n;

  n = get_status_msg(status, status_msg, 256);
  if (n == -1)
    strncpy(status_msg, "Unknown Status Code", 256);

  n = snprintf(buf, bufsize, "<html> <head>"
                             " <title>Error %d</title>"
                             "</head> <body>"
                             " <h2>Error %d - %s</h2>"
                             "</body> </html>",
                             status,
                             status, status_msg);
  return n;
}

#endif


/** Retorna o valor do metodo presente em #method.
 *
 *  @note Os valores retornados estao definidos em http.h (enum methods).
 */
int http_what_method(char *method, size_t size)
{
  switch(size)
  {
  case 3:
    if (strncmp(method, "GET", 3) == 0)
      return GET_M;
    if (strncmp(method, "PUT", 3) == 0)
      return PUT_M;
    break;

  case 4:
    if (strncmp(method, "HEAD", 4) == 0)
      return HEAD_M;
    if (strncmp(method, "POST", 4) == 0)
      return POST_M;
    break;

  case 5:
    if (strncmp(method, "TRACE", 5) == 0)
      return TRACE_M;
    break;

  case 6:
    if (strncmp(method, "DELETE", 6) == 0)
      return DELETE_M;
    break;

  case 7:
    if (strncmp(method, "OPTIONS", 7) == 0)
      return OPTIONS_M;
    if (strncmp(method, "CONNECT", 7) == 0)
      return CONNECT_M;
    break;

  default:
    break;
  }
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
  switch (status)
  {
  case OK:
    strncpy(buff, "OK", buffsize);
    break;

  case BAD_REQUEST:
    strncpy(buff, "Bad Request", buffsize);
    break;
  case FORBIDDEN:
    strncpy(buff, "Forbidden", buffsize);
    break;
  case NOT_FOUND:
    strncpy(buff, "Not Found", buffsize);
    break;

  case SERVER_ERROR:
    strncpy(buff, "Server Error", buffsize);
    break;

  default:
    strncpy(buff, "Unknown Status Code", buffsize);
    break;
  }

  return strlen(buff);
}


/** Diz se o numero de status 'status' e um aviso de erro.
 *
 *  @return 1 se for caso de erro, 0 se for um status normal.
 */
int is_error_status(int status)
{
  switch (status)
  {
  case NOT_FOUND:
  case SERVER_ERROR:
    return 1;
    break;
  default:
    return 0;
    break;
  }
}


/** Atribui a mensagem de status correspondente ao 'status' em 'buf'
 *  (respeitando 'bufsize').
 *
 *  @return O numero de caracteres escritos em 'buf' e -1 em caso de
 *          erros.
 *  @todo adicionar suporte a mais mensagens de erro!
 */
int get_status_msg(int status, char* buf, size_t bufsize)
{
  char* msg;

  if (buf == NULL)
    return -1;

  switch(status)
  {
  case OK:
    msg = "OK";
    break;
  case NOT_FOUND:
    msg = "Not Found";
    break;
  case SERVER_ERROR:
    msg = "Server Error";
    break;

  default:
    msg = "Unknown Status Code";
    break;
  }

  strncpy(buf, msg, bufsize);

  return strlen(buf);
}
