/**
 * @file client.c
 *
 * Implementacao das funcoes relacionadas ao clienthandler.
 */

#include <stdio.h>
#include <stdlib.h>     /* atoi() realpath()                         */
#include <string.h>     /* memset()                                  */
#include <errno.h>      /* errno                                     */
#include <unistd.h>     /* fcntl()                                   */
#include <fcntl.h>      /* fcntl() O_RDWR                            */
#include <netdb.h>      /* gethostbyname() send() recv()             */
#include <sys/stat.h>   /* stat() S_ISDIR()                          */
#include <limits.h>     /* realpath()                                */

#include "client.h"
#include "http.h"
#include "verbose_macro.h"

#define H_ANSWER     0
#define H_ERROR      1
#define PROTOCOL     "HTTP/1.0"

#define PACKAGE_NAME PACKAGE"/"VERSION


/** Inicializa as variaveis internas de 'l', como o numero maximo
 *  de clientes suportados simultaneamente, 'max_clients'.
 *
 *  @return 0 em sucesso, -1 caso 'l' seja NULL.
 */
int c_handler_list_init(struct c_handler_list* l, int max_clients)
{
    if (l == NULL)
      return -1;

    l->current = 0;
    l->max     = max_clients;
    l->begin   = NULL;
    l->end     = NULL;

    return 0;
}


/** Inicializa as variaveis internas de 'h', como o cliente 'sck' e o
 *  diretorio root 'rootdir'.
 *
 *  Nessa funcao utiliza-se alocacao dinamica de memoria para criar um
 *  novo c_handler.
 *
 * @note Essa funcao supoe que o socket 'clientsckt' esta devidamente
 *        conectado ao cliente.
 *
 *  @todo Colocar o malloc() na main e deixar essa funcao mais simples.
 *
 *  @return 0 em sucesso, -1 caso 'h' seja NULL ou malloc() falhe.
 */
int c_handler_init(struct c_handler** h, int sck, char* rootdir)
{
  if ((h == NULL) || (*h != NULL))
    return -1;

  *h = malloc(sizeof(struct c_handler));
  if (*h == NULL)
    return -1;

  (*h)->next = NULL;
  (*h)->client = sck;
  (*h)->state = MSG_RECEIVING;

  memset(&((*h)->request),       '\0', BUFFER_SIZE);
  memset(&((*h)->answer),        '\0', BUFFER_SIZE);
  memset(&((*h)->fileerror),     '\0', BUFFER_SIZE);
  memset(&((*h)->filepath),      '\0', BUFFER_SIZE);
  memset(&((*h)->filestatusmsg), '\0', BUFFER_SIZE);

  (*h)->request_size = BUFFER_SIZE * 3;
  (*h)->output = NULL;
  (*h)->filep  = NULL;

  (*h)->answer_size = BUFFER_SIZE;

  strncpy((*h)->filepath, rootdir, BUFFER_SIZE);
  (*h)->filestatus = -1;
  (*h)->filesize   = -1;

  return 0;
}


/** Adiciona 'h' a lista 'l'.
 *
 *  @return 0 em sucesso, -1 em caso de erro - varios casos inclusos.
 */
int c_handler_add(struct c_handler* h, struct c_handler_list* l)
{
  if ((h == NULL) || (l == NULL))
    return -1;

  if (l->current == l->max)
    return -1;

  if (l->begin == NULL)
  {
    l->begin = h;
    l->end   = h;
  }
  else
  {
    struct c_handler* tmp = l->begin;

    while (tmp->next != NULL)
      tmp = tmp->next;

    tmp->next = h;
    l->end    = h;
  }

  l->current++;
  return 0;
}


/** Remove 'h' de 'l'.
 *
 *  @note Nao desaloca a memoria. Para isso, veja @see c_handler_exit()
 *
 *  @warning
 *  Caso l->begin seja NULL ou l->end seja NULL ou l->current seja 0,
 *  essa funcao nao continua. Entao, se algum desses for verdade mas
 *  os outros nao sejam, nunca poderemos remover - isso e um sinal para
 *  bug.
 *  tl;dr Se a lista estiver corrompida, essa funcao nao funciona.
 *
 *  @return 0 em sucesso, -1 em caso de erro - varios casos inclusos.
 */
int c_handler_remove(struct c_handler* h, struct c_handler_list* l)
{
  struct c_handler* tmp = NULL;
  int exists = 0;

  if ((h == NULL) || (l == NULL))
    return -1;

  if ((l->begin == NULL) || (l->end == NULL) || (l->current == 0))
    return -1;

  //checar se h esta realmente em l
  tmp = l->begin;
  do {
    if (tmp == h)
      exists = 1;

    tmp = tmp->next;
  } while ((exists == 0) && (tmp != NULL));

  if (exists == 0)
    return -1;

  tmp = l->begin;
  //efetivamente remover
  if (l->begin == h)
  {
    if (h->next == NULL)
      l->end = NULL;

    l->begin = h->next;
    h->next = NULL;
  }
  else if (l->end == h)
  {
    tmp = l->begin;

    while (tmp->next != h)
      tmp = tmp->next;

    l->end = tmp;
    tmp->next = NULL;
  }
  else
  {
    while (tmp->next != h)
      tmp = tmp->next;

    tmp->next = h->next;
    h->next = NULL;
  }

  l->current--;
  return 0;
}


/** Libera a memoria alocada para 'h'.
 *
 *  @note O valor de 'h' se torna NULL.
 */
void c_handler_exit(struct c_handler* h)
{
  free(h);
  h = NULL;
}


/** Recebe a mensagem atraves de recv() de uma maneira nao-bloqueante
 *
 *  @return 0 caso a mensagem esteja sendo recebida, -1 em caso de erro
 *          e 1 se a mensagem terminou de ser recebida.
 */
int receive_message(struct c_handler* h)
{
  char buffer[BUFFER_SIZE];
  int  buffer_size = BUFFER_SIZE;
  int  retval;

  usleep(200000);
  retval = recv(h->client, buffer, buffer_size, 0);
  if (retval == -1)
    if ((errno != EWOULDBLOCK) && (errno != EAGAIN))
      return -1;

  if (retval == 0)
    return 1;

  buffer[retval] = '\0';
  append(h->request, buffer, h->request_size);

  return 0;
}


/** Checa se o arquivo descrito dentro de 'h' existe ou nao e atribui
 *  a 'h' a mensagem de erro correspondente.
 *
 *  Primeiro eu uso realpath() pra expandir todos os symbolic links do
 *  path recebido pelo cliente.
 *  Se o arquivo nao existir, independentemente se ele esteja fora do
 *  diretorio permitido como root, ele vai avisar que nao existe.
 *  Se o arquivo existir, mas estiver fora do diretorio permitido como
 *  root, ele vai avisar que esta fora do range.
 *  @n
 *  tl;dr E possivel um hacker saber quais arquivos existem fora do
 *        diretorio root, atraves de tentativa e erro.
 *
 *  @return 0 caso nao haja erro e -1 em algum erro
 */
int file_check(struct c_handler* h, char* rootdir, int rootdirsize)
{
  struct stat st;
  char   errormsg[BUFFER_SIZE];
  char   buffer[BUFFER_SIZE];
  int    errornum;
  int    retval;


  if (realpath(h->filepath, buffer) == NULL)
  {
    perror("Error at realpath()");
    switch (errno)
    {
    case ENOENT:
      sprintf(h->filestatusmsg, "File Not Found");
      h->filestatus = NOT_FOUND;
      break;
    case EACCES:
      sprintf(h->filestatusmsg, "Search permission denied on this directory");
      h->filestatus = FORBIDDEN;
      break;
    default:
      break;
    }
    return -1;
  }
  strncpy(h->filepath, buffer, BUFFER_SIZE);

  if (strncmp(h->filepath, rootdir, rootdirsize) != 0)
  {
    sprintf(h->filestatusmsg, "Forbidden");
    h->filestatus = FORBIDDEN;
    return -1;
  }

  retval = stat(h->filepath, &st);
  if (retval == -1)
  {
    switch (errno)
    {
    case EACCES:
      sprintf(errormsg, "Search permission denied on this directory");
      errornum = FORBIDDEN;
      break;
    case ENOTDIR:
      sprintf(errormsg, "A component of the path is not a directory");
      errornum = BAD_REQUEST;
      break;
    case ENOMEM:
      sprintf(errormsg, "Out of Memory");
      errornum = SERVER_ERROR;
      break;
    default:
      sprintf(errormsg, "File Not Found");
      errornum = NOT_FOUND;
      break;
    }
    sprintf(h->filestatusmsg, "%s", errormsg);
    h->filestatus = errornum;

    return -1;
  }

  if (S_ISDIR(st.st_mode))
  {
    int length = strlen(h->filepath);

    if (h->filepath[length] != '/')
    {
      append(h->filepath, "/", BUFFER_SIZE);
    }
    append(h->filepath, "index.html", BUFFER_SIZE);

    //AGORA PRECISAMOS CHECAR NOVAMENTE!
    //TODO TODO TODO BUG TODO

    retval = stat(h->filepath, &st);
    if (retval == -1)
    {
      switch (errno)
      {
      case EACCES:
        sprintf(errormsg, "Search permission denied on this directory");
        errornum = FORBIDDEN;
        break;
      case ENOTDIR:
        sprintf(errormsg, "A component of the path is not a directory");
        errornum = BAD_REQUEST;
        break;
      case ENOMEM:
        sprintf(errormsg, "Out of Memory");
        errornum = SERVER_ERROR;
        break;
      default:
        sprintf(errormsg, "File Not Found");
        errornum = NOT_FOUND;
        break;
      }
      sprintf(h->filestatusmsg, "%s", errormsg);
      h->filestatus = errornum;

      return -1;
    }
  }

  h->filestatus = OK;
  sprintf(h->filestatusmsg, "OK");
  h->filesize   = st.st_size;
  h->filelastm  = st.st_mtime;

  return 0;
}


/** Reseta os valores 'h->size_left' e 'h->size_sent' para indicar
 *  que vamos comecar a mandar um arquivo.
 *
 *  @warning A cada 'coisa' que formos enviar, temos que adicionar
 *           aqui. Por exemplo, vamos mandar um arquivo, um header
 *           ou uma mensagem de erro. Cada caso deve ser considerado
 *           aqui.
 */
int prepare_msg_to_send(struct c_handler* h)
{
  if (h->output == NULL)
    return -1;


  if (h->output == h->fileerror)
    h->size_left = h->fileerrorsize;

  else if (h->output == h->answer)
    h->size_left = h->answer_size;

  else if (h->output == h->filebuff)
    h->size_left = h->filebuffsize;

  else
    h->size_left = strlen(h->output);

  h->size_sent = 0;

  return 0;
}


/** Continua enviando para h->client a mensagem apontada por h->output
 *  atraves de sockets nao-bloqueantes.
 *
 *  @return O numero de caracteres enviados em caso de sucesso.
 *          Se houver algum erro fatal, retorna -1. Se o socket for
 *          bloquear, retorna -2.
 */
int keep_sending_msg(struct c_handler* h)
{
  int retval = send(h->client, h->output + h->size_sent, h->size_left, 0);

  if (retval == -1)
  {
    if ((errno != EWOULDBLOCK) && (errno != EAGAIN))
    {
      perror("Error at send()");
      return -1;
    }
    return -2;
  }
  else
  {
    h->size_sent += retval;
    h->size_left -= retval;
  }

  return retval;
}


/** Abre o arquivo em 'h' para ser enviado
 *
 *  @return 0 em sucesso, -1 em caso de erro.
 */
int start_sending_file(struct c_handler* h)
{
  h->filep = fopen(h->filepath, "r");
  if (h->filep == NULL)
    return -1;

  return 0;
}


/** Fecha o arquivo aberto para o 'h'.
 *
 *  @return 0 em sucesso e -1 em caso de erro.
 */
int stop_sending_file(struct c_handler* h)
{
  int retval = fclose(h->filep);
  if (retval == EOF)
    return -1;
  h->filep = NULL;
  return 0;
}


/** Pega um pedaco do arquivo e guarda no buffer dentro de 'h'.
 *
 *  @return 0 se pegar todo o pedaco do arquivo de uma vez, 1 se o arquivo
 *          terminou de ser lido e -1 em caso de erro.
 */
int get_file_chunk(struct c_handler* h)
{
  int retval;

  if (h->filep == NULL)
    return -1;

  memset(&(h->filebuff), '\0', BUFFER_SIZE);
  retval = fread(h->filebuff, sizeof(char), BUFFER_SIZE - 1, h->filep);

  h->filebuffsize = retval;
  h->size_left = retval;
  h->size_sent = 0;

  if (retval < (BUFFER_SIZE - 1))
  {
    if (feof(h->filep))
      return 1;

    if (ferror(h->filep))
    {
      perror("Error at ferror()");
      return -1;
    }
  }
  return 0;
}


/** Constroi e armazena em 'h->fileerror' uma pagina HTML contendo
 *  o erro ocorrido.
 */
void build_error_html(struct c_handler* h)
{
  int size = get_http_status_msg(h->filestatus, h->filestatusmsg, BUFFER_SIZE);
  h->filestatusmsg_size = size;

  snprintf(h->fileerror, BUFFER_SIZE,
           "<html>\n<head>\n<title>Error %d</title>\n</head>\n<body>\n"
           "<h3>Error %d - %s</h3>\n<hr><pre>%s</pre>"
           "</body>\n</html>",
           h->filestatus, h->filestatus, h->filestatusmsg, PACKAGE_NAME);

  h->fileerrorsize = strlen(h->fileerror);
}


/** Separa as partes uteis da request HTTP enviada pelo usuario.
 *
 *  @todo Por enquanto so mexemos com filename. Implementar metodo e versao
 *  @return 0 se tudo der certo, -1 caso a request contenha algum metodo
 *          nao-implementado.
 */
int parse_request(struct c_handler* h)
{
  char buff[BUFFER_SIZE];
  char *method;
  char *filename;
  char *http_version;

  strncpy(buff, h->request, BUFFER_SIZE);

  method = strtok(buff, " ");
  filename = strtok(NULL, " ");
  http_version = strtok(NULL, "\r\n");

  if (strcmp(method, "GET") != 0)
    return -1;
  if ((strcmp(http_version, "HTTP/1.0") != 0) &&
      (strcmp(http_version, "HTTP/1.1") != 0))
    return -1;

  strncat(h->filepath, filename, BUFFER_SIZE - strlen(h->filepath));

  return 0;
}


/** Constroi e atribui o header HTTP ao 'buf' (respeitando 'bufsize').
 *
 *  A mensagem e construida de acordo com os parametros.
 *  @todo Remover valores arbitrarios dos buffers.
 *  @return O numero de caracteres efetivamente atribuidos a 'buf'.
 */
int build_header(struct c_handler* h)
{
  int n;

  //~ char last_modif[BUFFER_SIZE];
  //LIDAR COM O TEMPO!!!!!!
  //strftime (timebuf)


  if (h->filestatus == OK)
  {
    n = snprintf(h->answer, h->answer_size, "%s %d %s\r\n"
                                            "Server: %s\r\n"
                                            "Content-Type: %s\r\n"
                                            "Content-Length: %d\r\n"
                                            //~ "Last-Modified: %s"
                                            "Connection: close\r\n"
                                            "\r\n",
                                            PROTOCOL, h->filestatus, h->filestatusmsg,
                                            PACKAGE_NAME,
                                            "text/html",
                                            h->filesize);
  }
  else
  {
    n = snprintf(h->answer, h->answer_size, "%s %d %s\r\n"
                                            "Server: %s\r\n"
                                            "Content-Type: %s\r\n"
                                            "Content-Length: %d\r\n"
                                            "Connection: close\r\n"
                                            "\r\n",
                                            PROTOCOL, h->filestatus, h->filestatusmsg,
                                            PACKAGE_NAME,
                                            "text/html",
                                            h->fileerrorsize);
  }

  return n;
}
