/**
 * @file main.c
 *
 * A logica e funcao principal do programa.
 */

#include <stdio.h>
#include <stdlib.h>     /* atoi()                                    */
#include <string.h>     /* memset()                                  */
#include <errno.h>      /* errno                                     */
#include <unistd.h>     /* fcntl()                                   */
#include <fcntl.h>      /* fcntl() O_RDWR                            */
#include <netdb.h>      /* gethostbyname() send() recv()             */
#include <sys/stat.h>   /* stat() S_ISDIR()                          */
#include <limits.h>     /* realpath()                                */

#include "client.h"
#include "server.h"
#include "http.h"
#include "verbose_macro.h"

#define MAX_CLIENTS  10
#define BUFFER_SIZE  256




/** Lida com os argumentos passados pela linha de comando: port 'port number'
 *  e 'root directory'.
 */
int handle_args(int argc, char* argv[])
{
  int port;

  if (argc != 3)
  {
    printf("Usage: servw [port_number] [root_directory]\n");
    return -1;
  }

  port = atoi(argv[1]);
  if ((port < 0) || (port > 65535))
  {
    printf("Invalid port number %d! Choose between 0 and 65535!\n", port);
    return -1;
  }

  return 0;
}


/** Retorna o maior numbero, 'a' ou 'b'.
 */
int bigger(int a, int b)
{
  if (a > b)
    return a;
  else
    return b;
}


/** Adiciona 'orig' a string 'dest' a partir do final, respeitando o limite
 *  de 'size'.
 *
 *  @note 'size' e o tamanho TOTAL de 'dest' - nao pode ser o tamanho restante!
 */
int append(char* dest, char* orig, size_t size)
{
  int origsize = strlen(orig);
  int destsize = strlen(dest);
  int remaining = size - destsize;

  if (origsize < remaining)
  {
    strncat(dest, orig, remaining);
    return 0;
  }
  else
    return -1;
}

int find_crlf(char* where)
{
  if (where == NULL)
    return -1;
  if (strstr(where, "\r\n\r\n") == NULL)
    return 0;
  else
    return 1;
}


int get_http_status_msg(int status, char* buff, size_t buffsize)
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
    return -1;
  }

  return 0;
}


/** Cria um daemon atraves de fork(), 'matando' o processo pai e atribuindo
 *  stdout para 'logfile' e stderr para 'errfile'.
 *
 *  Caso haja um erro, essa funcao interrompe a execucao do programa e
 *  exibe a mensagem de erro em stderr.
 *  Isso porque ela depende do perror() especifico de cada funcao chamada.
 *
 */
void daemonize(FILE* logfile, char* logname, FILE* errfile, char* errlogname)
{
  pid_t pid;
  pid_t sid;
  int   retval;


  pid = fork();
  if (pid == -1)
  {
    perror("Error at fork()");
    exit(EXIT_FAILURE);
  }

  if (pid > 0)
    exit(EXIT_SUCCESS);

  logfile = freopen(logname, "w", stdout);
  if (logfile == NULL)
  {
    perror("Error at freopen()");
    exit(EXIT_FAILURE);
  }

  errfile = freopen(errlogname, "w", stderr);
  if (logfile == NULL)
  {
    perror("Error at freopen()");
    exit(EXIT_FAILURE);
  }

  /* mandar stdin pra /dev/null */
  retval = dup2(STDIN_FILENO, open("/dev/null", O_RDWR));
  if (retval == -1)
    perror("Error at dup2()");

  sid = setsid();
  if (sid == -1)
  {
    perror("Error at setsid()");
    exit(EXIT_FAILURE);
  }
}


int main(int argc, char *argv[])
{
  FILE *logfile = NULL;
  FILE *errfile = NULL;

  int  port_number;
  char rootdir[BUFFER_SIZE];
  int  rootdirsize;

  int  listener    = -1;
  int  maxfds;

  struct c_handler_list handler_list;
  fd_set readfds;
  fd_set writefds;
  fd_set clientfds;

  char buffer[BUFFER_SIZE];
  int retval;


  retval = handle_args(argc, argv);
  if (retval == -1)
    exit (EXIT_FAILURE);


  /* Inicializar daemon */
  //daemonize(logfile, "servw.log", errfile, "servwERR.log");

  /* Inicializar servidor */
  port_number = atoi(argv[1]);

  memset(rootdir, '\0', BUFFER_SIZE);
  rootdirsize = BUFFER_SIZE;
  if (argv[2][0] == '/')
  {
    strncpy(rootdir, argv[2], rootdirsize - 1);
    rootdirsize -= strlen(argv[2]);
  }
  else
  {
    strncpy(rootdir, getenv("PWD"), rootdirsize - 1);
    rootdirsize -= strlen (getenv("PWD"));
    rootdir[strlen(rootdir)] = '/';
    rootdirsize--;
    strncat(rootdir, argv[2], rootdirsize - 1);
    rootdirsize -= strlen (argv[2]);
  }


  if (realpath(rootdir, buffer) == NULL)
  {
    perror("Error at realpath()");
    exit(EXIT_FAILURE);
  }
  strncpy(rootdir, buffer, rootdirsize - 1);

  printf("Root directory set to: %s\n", rootdir);

  listener = server_start(port_number);
  if (listener == -1)
    exit(EXIT_FAILURE);


  /* Inicializar select() */
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_ZERO(&clientfds);

  maxfds = listener;


  /* Inicializar clienthandlers */
  c_handler_list_init(&handler_list, MAX_CLIENTS);
  if (handler_list.max > FD_SETSIZE)
  {
    LOG_WRITE_ERROR("O maximo de clientes permitido por select() e FD_SETSIZE");
    exit(EXIT_FAILURE);
  }

  LOG_WRITE("Inicializacao completa!");


  /* Main Loop */
  while (1)
  {
    readfds  = clientfds;
    writefds = clientfds;

    FD_SET  (listener, &readfds);

    retval = select(maxfds + 1, &readfds, &writefds, NULL, NULL);

    /* nova conexao */
    if (FD_ISSET (listener, &readfds))
    {
      LOG_WRITE("\n *** \nTem um cliente tentando se conectar!");

      if (handler_list.current < handler_list.max)
      {
        int new_client = accept(listener, NULL, NULL);

        if (new_client == -1)
          perror("Erro em accept()");
        else
        {
          struct c_handler* handler = NULL;

          retval = c_handler_init(&handler, new_client, rootdir);
          if (retval != -1)
          {
            retval = c_handler_add(handler, &handler_list);
            if (retval != -1)
            {
              maxfds = bigger(new_client, maxfds);
              FD_SET(new_client, &clientfds);
              LOG_WRITE("Nova conexao de cliente aceita!");
            }
          }
        } /** @todo limpar isso, esta muito feio */
      }
    }


    /* Maquina de estados dos clienthandlers */
    if (handler_list.current > 0)
    {
      struct c_handler* handler = NULL;

      handler = handler_list.begin;
      while (handler != NULL)
      {
        switch (handler->state)
        {
        case MSG_RECEIVING:
          /** @todo @bug @warning
           *  Caso o cliente conecte mas tenha um 'lag', o FD_ISSET do readfds
           *  nao vai dar.
           *  Porem, se o cliente terminar de mandar a mensagem, vai acontecer
           *  a mesma coisa.
           *  Como diferenciar o fato de que o cliente pode estar numa conexao
           *  lenta com o fato de que o cliente pode ter terminado de enviar?
           */
          if (FD_ISSET(handler->client, &readfds))
          {
            retval = receive_message(handler);
            if (retval == -1)
            {
              LOG_WRITE("Erro de conexao com cliente!");
              handler->state = FINISHED;
            }
            if (retval == 1)
            {
              LOG_WRITE("Cliente desconectou");
              handler->state = FINISHED;
            }
          }
          else
          {
            if (find_crlf(handler->request))
              handler->state = MSG_RECEIVED;
          }
          break;

        case MSG_RECEIVED:
          LOG_WRITE("Mensagem recebida!");
          handler->request_size = strlen(handler->request);
          handler->state = FILE_PROCESSING;
          break;

        case FILE_PROCESSING:
          LOG_WRITE("Processando request...");
          retval = parse_request(handler);
          if (retval == -1)
          {
            // TODO TODO TODO LIDAR COM COISAS ESTRANHAS
          }
          retval = file_check(handler, rootdir, strlen(rootdir));
          if (retval == 0)
          {
            build_header(handler);
            handler->output = handler->answer;
            handler->state = HEADER_SENDING;
          }
          else
          {
            build_error_html(handler);
            build_header(handler);
            handler->output = handler->answer;
            handler->state = ERROR_HEADER_SENDING;
          }

          prepare_msg_to_send(handler);
          LOG_WRITE("Arquivo processado!");
          break;

        case HEADER_SENDING:
          LOG_WRITE("Enviando Headers...");
          if (FD_ISSET(handler->client, &writefds))
          {
            retval = keep_sending_msg(handler);
            if (retval == 0)
              handler->state = HEADER_SENT;
            if (retval == -1)
              handler->state = FINISHED;
          }
          break;

        case HEADER_SENT:
          LOG_WRITE("Header enviado");
          handler->output = handler->filebuff;
          start_sending_file(handler);
          handler->state = FILE_SENDING;
          handler->need_file_chunk = 1;
          handler->filesentsize = 0;
          break;

        case FILE_SENDING:
          if (FD_ISSET(handler->client, &writefds))
          {
            LOG_WRITE("Enviando arquivo");

            if (handler->need_file_chunk == 1)
            {
              retval = get_file_chunk(handler);
              if (retval == -1)
              {
                LOG_WRITE("Erro na leitura do arquivo!");
                handler->state = FINISHED;
                break;
              }

              prepare_msg_to_send(handler);
              handler->need_file_chunk = 0;
            }

            retval = keep_sending_msg(handler);
            if (retval == -1)
            {
              LOG_WRITE("Erro de conexao!");
              handler->state = FINISHED;
              break;
            }
            if (retval == 0)
            {
              handler->need_file_chunk = 1;
              //~ LOG_WRITE("... ");
            }
            else
              handler->filesentsize += retval;

            if (handler->filesentsize >= handler->filesize)
              handler->state = FILE_SENT;
          }
          break;

        case FILE_SENT:
          LOG_WRITE("Arquivo enviado");
          stop_sending_file(handler);
          handler->state = FINISHED;
          break;

        case ERROR_HEADER_SENDING:
          if (FD_ISSET(handler->client, &writefds))
          {
            LOG_WRITE("Enviando Header de erro");
            retval = keep_sending_msg(handler);
            if (retval == 0)
              handler->state = ERROR_HEADER_SENT;
            else if (retval == -1)
              handler->state = FINISHED;
            else
            {
              //sending...
            }
          }
          break;

        case ERROR_HEADER_SENT:
          LOG_WRITE("Header de erro enviado");
          handler->output = handler->fileerror;
          prepare_msg_to_send(handler);
          handler->state = ERROR_SENDING;
          break;

        case ERROR_SENDING:
          if (FD_ISSET(handler->client, &writefds))
          {
            LOG_WRITE("Enviando arquivo de erro");
            retval = keep_sending_msg(handler);
            if (retval == 0)
              handler->state = ERROR_SENT;
            if (retval == -1)
              handler->state = FINISHED;

            handler->fileerrorsize -= retval;

            if (handler->fileerrorsize <= 0)
              handler->state = ERROR_SENT;
          }
          break;

        case ERROR_SENT:
          LOG_WRITE("Arquivo de erro enviado");
          handler->state = FINISHED;
          break;

        case FINISHED:
          FD_CLR(handler->client, &clientfds);
          if (maxfds == handler->client)
          {
            struct c_handler* tmp = handler_list.begin;

            if (handler_list.current == 1)
              maxfds = listener;
            else
            {
              //untested
              while (tmp != NULL)
              {
                if (tmp != handler)
                  maxfds = bigger(tmp->client, maxfds);
                tmp = tmp->next;
              }
            }

          }
          close(handler->client);

          c_handler_remove(handler, &handler_list);
          c_handler_exit(handler);
          handler = handler_list.begin;

          LOG_WRITE("Cliente desconectou\n");
          break;

        default:
          break;
        }

      if (handler != NULL)
        handler = handler->next;

      } /*  while (handler != NULL) */

    } /* if (handler_list.current > 0) */

  } /* while(1) */

  return 0;
}

/*

List of nCurses Software

<resumim de ncurses>
<motivacao pessoal>

Window Managers

File Manager

Bittorrent client

Games









*/
