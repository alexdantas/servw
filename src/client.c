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
#include <time.h>       /* clock_gettime()                           */

#include "client.h"
#include "http.h"
#include "macros.h"


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

    l->smaller_timeout = NULL;
    l->onesec_timeout.tv_sec  = 1;
    l->onesec_timeout.tv_usec = 0;

    return 0;
}


/** Inicializa as variaveis internas de 'h', como o cliente 'sck' e o
 *  diretorio root 'rootdir'.
 *
 *  Nessa funcao utiliza-se alocacao dinamica de memoria para criar um
 *  novo c_handler.
 *
 *  @bug  Essa funcao nao necessariamente seta 'errno'. Chamar perror()
 *        logo apos ela pode gerar mensagens de erros indefinidas.
 *  @note Essa funcao supoe que o socket 'clientsckt' esta devidamente
 *        conectado ao cliente.
 *
 *  @todo Colocar o malloc() na main e deixar essa funcao mais simples.
 *
 *  @return 0 em sucesso, -1 caso 'h' seja NULL ou malloc() falhe.
 */
int c_handler_init(struct c_handler** h, int sck, char* rootdir, size_t rootdirsize, int bandwidth)
{
  if ((h == NULL) || (*h != NULL))
    return -1;

  *h = malloc(sizeof(struct c_handler));
  if (*h == NULL)
    return -1;

  (*h)->next = NULL;
  (*h)->client = sck;
  (*h)->state = HEADER_RECEIVING;

  memset(&((*h)->request),       '\0', BUFFER_SIZE * 3);
  memset(&((*h)->outputbuff),    '\0', BUFFER_SIZE);
  memset(&((*h)->answer_header), '\0', BUFFER_SIZE);
  memset(&((*h)->filepath),      '\0', BUFFER_SIZE);
  memset(&((*h)->filestatusmsg), '\0', BUFFER_SIZE);
  memset(&((*h)->filetype),      '\0', BUFFER_SIZE);

  (*h)->outputbuff_size      = 0;
  (*h)->outputbuff_sizeleft = 0;
  (*h)->outputbuff_sizesent = 0;

  (*h)->request_size = 0;
  (*h)->output = NULL;
  (*h)->filep  = NULL;

  (*h)->answer_header_size = BUFFER_SIZE;

  strncpy((*h)->filepath, rootdir, BUFFER_SIZE);
  (*h)->filepathsize = rootdirsize;
  (*h)->filestatus = -1;
  (*h)->filesize   = -1;
  (*h)->bandwidth  = bandwidth;

  (*h)->waiting = 0;

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
 *  Pega um pedaco e ja anexa ao #request
 *
 *  @return 0 caso a mensagem esteja sendo recebida, -1 em caso de erro
 *          e 1 se a mensagem terminou de ser recebida.
 */
int receive_request(struct c_handler* h)
{
  char buffer[BUFFER_SIZE];
  int  buffer_size = BUFFER_SIZE;
  int  retval;

  // Para simular leitura lenta
  //~ usleep(200000);
  retval = recv(h->client, buffer, buffer_size, 0);
  if (retval == -1)
    if ((errno != EWOULDBLOCK) && (errno != EAGAIN))
      return -1;

  if (retval == 0)
    return 1;

  buffer[retval] = '\0';

  if ((h->request_size + retval) > BUFFER_SIZE * 3)
    return -1;

  strncat(h->request, buffer, retval);

  h->request_size += retval;

  return 0;
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
  char *version;

  strncpy(buff, h->request, BUFFER_SIZE);

  method = strtok(buff, " ");
  filename = strtok(NULL, " ");
  version = strtok(NULL, "\r\n");

  switch(http_what_method(method, strlen(method)))
  {
  case GET_M:
    // pode continuar
    break;
  default:
    return -1;
    break;
  }

  switch(http_what_version(version, strlen(version)))
  {
  case HTTP_1_0:
  case HTTP_1_1:
    // pode continuar
    break;
  default:
    return -1;
    break;
  }

  strncat(h->filepath, filename, BUFFER_SIZE - h->filepathsize);
  h->filepathsize += strlen(filename);

  return 0;
}





/* Pegar um novo smaller_timeout*/
void get_new_smaller_timeout (struct c_handler_list* l, struct c_handler *h)
{
  struct c_handler* tmp = l->begin;

  l->smaller_timeout = &(l->onesec_timeout);
  while (tmp != NULL)
  {
    if ((tmp->timer.delta.tv_sec > 0) &&
        (timercmp(&(tmp->timer.delta), l->smaller_timeout, <)) &&
        (tmp != h))
      l->smaller_timeout = &(tmp->timer.delta);

    tmp = tmp->next;
  }
  if (l->smaller_timeout == &(l->onesec_timeout))
    l->smaller_timeout = NULL;
}

void get_new_maxfds(int* maxfds, struct c_handler_list* l, struct c_handler* h)
{
  struct c_handler* tmp = l->begin;

  while (tmp != NULL)
  {
    if (tmp != h)
    {
      if (tmp->client > *maxfds)
        *maxfds = tmp->client;
    }
    tmp = tmp->next;
  }
}


/*
 *
 * open_file
 * close_file
 *
 *
 * h->output
 * h->output_sizeleft
 * h->output_sizesent
 *
 *
 *
 * get chunk
 * prepare chunk
 * send chunk
 *
 * h->outputbuff
 * h->outputbuff_sizeleft
 * h->outputbuff_sizesent
 *
 *
 */

/** Prepara o c_handler para enviar o arquivo #file.
 *
 *  Associa o #h->output para a stream #file.
 *  @return Retorna 0 em sucesso, -1 caso algum argumento seja NULL.
 */
int open_file(struct c_handler *h, FILE *file, size_t size)
{
  if ((h == NULL) || (file == NULL))
    return -1;

  h->output = file;
  h->output_size = size;
  h->output_sizeleft = size;
  h->output_sizesent = 0;
  return 0;
}

/**
 *  @return Retorna 0 em sucesso, -1 em caso de erro.
 */
int close_file(struct c_handler* h)
{
  if ((h->output == NULL) || (h == NULL))
    return -1;

  int retval = fclose(h->output);
  if (retval == EOF)
  {
    LOG_PERROR("Erro em close_file() - fclose()");
    return -1;
  }
  h->output = NULL;
  return 0;
}

/** Le um pedaco do arquivo apontado por #h->output e armazena em
 *  #h->outputbuff. O tamanho do buffer e #h->outputbuff_size.
 *
 *  @note Le o pedaco caractere por caractere (sizeof(char) x 1).
 */
int get_chunk(struct c_handler* h)
{
  int retval;

  if ((h->output == NULL) || (h->outputbuff == NULL))
    return -1;

  memset(&(h->outputbuff), '\0', BUFFER_SIZE);
  retval = fread(h->outputbuff, sizeof(char), BUFFER_SIZE - 1, h->output);

  h->outputbuff_size     = retval;
  h->outputbuff_sizeleft = retval;
  h->outputbuff_sizesent = 0;

  if (retval < (BUFFER_SIZE - 1))
  {
    // Acabou o arquivo!
    if (feof(h->output))
      return 1;

    // Aghw
    if (ferror(h->output))
    {
      LOG_PERROR("Erro em fread()");
      return -1;
    }
  }
  return 0;
}

/** Envia o pedaco de arquivo apontado por #h->outputbuff para o cliente
 *  em #h->client.
 *
 *  @return O numero de caracteres enviados, -1 em caso de erro e 0 caso
 *          ja tenha enviado tudo.
 */
int send_chunk(struct c_handler* h)
{
  int size;
  int retval;


  if ((h->output == NULL) || (h->outputbuff == NULL))
    return -1;

  if (h->output_sizeleft == 0)
    return 0;

  // limitar baseado no tamanho restante do buffer
  if ((h->outputbuff_sizeleft) > (h->bandwidth))
    size = h->outputbuff_sizeleft - (h->outputbuff_sizeleft - h->bandwidth);
  else
    size = h->bandwidth - (h->bandwidth - h->outputbuff_sizeleft);

  // limitar baseado no tamanho ja enviado ate agora
  if ((h->timer_sizesent + size) > h->bandwidth)
    size = (h->bandwidth - h->timer_sizesent);

  retval = send(h->client, h->outputbuff + h->outputbuff_sizesent, size, 0);

  if (retval == -1)
  {
    if ((errno != EWOULDBLOCK) && (errno != EAGAIN))
    {
      perror("Error at send()");
      return -1;
    }
    // bloqueou
    return -2;
  }


  h->outputbuff_sizesent += retval;
  h->outputbuff_sizeleft -= retval;

  return retval;
}

/** Modifica #path para uma string com o caminho absoluto e canonico.
 *
 *  @note Exemplos sao '../', './', '../../././' e '///'.
 *
 */
int resolve_symlinks(char *path, size_t size)
{
  char buffer[BUFFER_SIZE];

  if (path == NULL)
    return -1;

  if (realpath(path, buffer) == NULL)
  {
    LOG_PERROR("Erro em resolve_symlinks() - realpath()");
    switch (errno)
    {
    case ENOENT:
      return NOT_FOUND_S;
    case EACCES:
      return FORBIDDEN_S;
    case ENAMETOOLONG:
      return REQUEST_URI_TOO_LARGE_S;

    default:
      return SERVER_ERROR_S;
    }
    return -1;
  }
  strncpy(path, buffer, size);
  return OK_S;
}


/** Verifica se #path esta dentro de #rootdir.
 *
 *  @return #status_codes HTTP com o erro encontrado.
 */
int check_path(char *path, char *rootdir, size_t rootdirsize)
{
  if (strncmp(path, rootdir, rootdirsize) != 0)
    return FORBIDDEN_S;

  return OK_S;
}

/** Verifica se o arquivo existe e se e permitido localiza-lo.
 *
 *  @return #status_codes HTTP com o erro encontrado.
 */
int check_file(char *path)
{
  struct stat st;

  if (stat(path, &st) == -1)
  {
    LOG_PERROR("Erro em check_file() - stat()");
    switch (errno)
    {
    case ENOENT:
      return NOT_FOUND_S;
    case EACCES:
      return FORBIDDEN_S;
    case ENOTDIR:
      return BAD_REQUEST_S;
    case ENAMETOOLONG:
      return REQUEST_URI_TOO_LARGE_S;
    default:
      return SERVER_ERROR_S;
    }
  }
  return OK_S;
}

/** Verifica se #path e um diretorio.
 *
 *  @return Caso #path seja um diretorio, retorna 1. Se nao for, retorna 0.
 *          Retorna -1 em caso de erro.
 */
int check_file_is_dir(char *path)
{
  struct stat st;

  if (stat(path, &st) == -1)
  {
    LOG_PERROR("Erro em check_file_is_dir() - stat()");
    return -1;
  }
  return S_ISDIR(st.st_mode);
}

int get_file_size(char *path)
{
  struct stat st;

  if (stat(path, &st) == -1)
  {
    LOG_PERROR("Erro em check_file() - stat()");
    return -1;
  }

  return st.st_size;
}


/** Anexa a string "index.html" ao #h->filepath.
 *
 *  @return 0 em sucesso, -1 caso nao caiba.
 */
int append_index_html(char *path, size_t pathsize)
{
  char *index_html = "index.html";

  size_t indexsize = strlen(index_html);

  if (path[pathsize] != '/')
  {
    if ((pathsize + 1) >= BUFFER_SIZE)
      return -1;

    path[pathsize + 1] = '/';
  }

  if ((pathsize + indexsize) >= BUFFER_SIZE)
    return -1;

  strncat(path, index_html, pathsize);
  return 0;
}






