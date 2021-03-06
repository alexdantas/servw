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
#include <signal.h>     /* sigaction()                               */

#include "client.h"
#include "server.h"
#include "http.h"
#include "macros.h"
#include "timer.h"

#define MAX_CLIENTS  10
#define BUFFER_SIZE  256


/** Lida com os argumentos passados pela linha de comando: port 'port number'
 *  e 'root directory'.
 */
int handle_args(int argc, char* argv[])
{
  int port;
  int bandwidth;

  if (argc != 4)
  {
    printf("Usage: servw [port_number] [root_directory] [bandwidth (Bytes/s)]\n");
    return -1;
  }

  port = atoi(argv[1]);
  if ((port < 0) || (port > 65535))
  {
    printf("Invalid port number %d! Choose between 0 and 65535!\n", port);
    return -1;
  }

  bandwidth = atoi(argv[3]);
  if (bandwidth <= 0)
  {
    printf("Invalid bandwidth '%d bytes/s'! Choose a number greater than 0.\n", atoi(argv[3]));
    return -1;
  }
  return 0;
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


/** Cria um daemon atraves de fork(), 'matando' o processo pai e atribuindo
 *  stdout para 'logfile' e stderr para 'errfile'.
 *
 *  Caso haja um erro, essa funcao interrompe a execucao do programa e
 *  exibe a mensagem de erro em stderr.
 *  Isso porque ela depende do perror() especifico de cada funcao chamada.
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


/** Ignora o sinal SIGPIPE que e mandado toda vez que um cliente desconecta
 *  enquanto estou mandando dados.
 *
 *  @note Nao se preocupe, ignorar SIGPIPE nao traz nenhum problema...
 */
void ignore_sigpipe(int signal)
{ }


/** Determina o que o programa vai fazer quando receber sinais especificos.
 *
 *  @note Por enquanto so lidamos com SIGPIPE
 */
void set_signals()
{
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = ignore_sigpipe;

    sigaction (SIGPIPE, &sa, NULL);
}


int main(int argc, char *argv[])
{
  FILE *logfile = NULL;
  FILE *errfile = NULL;

  int  port_number;
  char rootdir[BUFFER_SIZE];
  int  rootdirsize;

  int listener = -1;
  int maxfds;
  int select_retval;

  struct c_handler_list handler_list;
  struct c_handler* handler = NULL;

  fd_set readfds;
  fd_set writefds;
  fd_set clientfds;
  fd_set total_readfds;
  fd_set total_writefds;
  int total_clients = 0;

  struct timeval tmp_timer;

  char buffer[BUFFER_SIZE];
  int retval;


  retval = handle_args(argc, argv);
  if (retval == -1)
    exit (EXIT_FAILURE);

  /* Inicializar daemon - pode ser commented-out */
  //~ daemonize(logfile, "servw.log", errfile, "servwERR.log");

  /* Inicializar servidor */
  port_number = atoi(argv[1]);

  set_signals();

  memset(rootdir, '\0', BUFFER_SIZE);
  rootdirsize = 0;
  if (argv[2][0] == '/')
  {
    // Caminho absoluto
    strncpy(rootdir, argv[2], (BUFFER_SIZE - 1));
    rootdirsize = strlen(argv[2]);
  }
  else
  {
    // Caminho relativo
    strncpy(rootdir, getenv("PWD"), (BUFFER_SIZE - 1));
    rootdirsize = strlen (rootdir);
    rootdir[rootdirsize] = '/';
    rootdirsize++;
    strncat(rootdir, argv[2], (BUFFER_SIZE - 1) - rootdirsize);
    rootdirsize += strlen (argv[2]);
  }

  // Expandir os symbolic links do diretorio root
  if (realpath(rootdir, buffer) == NULL)
  {
    if (errno == ENOENT)
      printf("Error! Directory doesn't exist: %s\n", rootdir);
    else
      perror("Erro em realpath()");
    exit(EXIT_FAILURE);
  }
  strncpy(rootdir, buffer, (BUFFER_SIZE - 1));
  rootdirsize = strlen(rootdir);
  printf("Diretorio raiz: %s\n", rootdir);

  // server_start -  muito importante!
  listener = server_start(port_number);
  if (listener == -1)
    exit(EXIT_FAILURE);


  /* Inicializar select() */
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_ZERO(&clientfds);
  FD_ZERO(&total_readfds);
  FD_ZERO(&total_writefds);
  FD_SET(listener, &total_readfds);

  maxfds = listener;


  /* Inicializar clienthandlers */
  c_handler_list_init(&handler_list, MAX_CLIENTS);
  if (handler_list.max > FD_SETSIZE)
  {
    LOG_ERROR("O maximo de clientes permitido por select() e FD_SETSIZE");
    exit(EXIT_FAILURE);
  }

  LOG_WRITE("Inicializacao completa!");


  /* Main Loop */
  while (1)
  {
    readfds  = total_readfds;
    writefds = total_writefds;

    // 'backup' do smaller_timeout - veja 'man 2 select', Linux Notes
    if (handler_list.smaller_timeout != NULL)
    {
      tmp_timer.tv_sec  = handler_list.smaller_timeout->tv_sec;
      tmp_timer.tv_usec = handler_list.smaller_timeout->tv_usec;
    }


    select_retval = select(maxfds + 1, &readfds, &writefds, NULL, handler_list.smaller_timeout);


    if (handler_list.smaller_timeout != NULL)
    {
      handler_list.smaller_timeout->tv_sec = tmp_timer.tv_sec;
      handler_list.smaller_timeout->tv_usec = tmp_timer.tv_usec;
    }

    if (select_retval == -1)
      perror("Erro em select()");

    /* nova conexao */
    if (FD_ISSET (listener, &readfds))
    {
      struct c_handler* handler = NULL;
      int new_client = -1;

      VERBOSE(printf("Novo cliente tentando se conectar\n"));

      if (handler_list.current > handler_list.max)
      {
        LOG_ERROR("Limite de clientes excedido!");
        continue;
      }

      new_client = accept(listener, NULL, NULL);
      if (new_client == -1)
      {
        perror("Erro em accept()");
        continue;
      }

      retval = c_handler_init(&handler, new_client, rootdir, rootdirsize, atoi(argv[3]));
      if (retval == -1)
      {
        perror("Erro em c_handler_init()");
        close(new_client);
        continue;
      }

      retval = c_handler_add(handler, &handler_list);
      if (retval == -1)
      {
        LOG_ERROR("Erro em c_handler_add()");
        close(new_client);
        continue;
      }

      if (new_client > maxfds)
        maxfds = new_client;
      FD_SET(new_client, &total_readfds);
      FD_SET(new_client, &total_writefds);
      LOG_WRITE("*** Nova conexao de cliente aceita! ***");
      total_clients++;
    }


    if (handler_list.current == 0)
      continue;


    handler = handler_list.begin;
    while (handler != NULL)
    {
      /* Lidar com quem esta esperando */
      if (handler->waiting == 1)
      {
        timer_stop(&(handler->cronometro));
        timer_delta(&(handler->cronometro));
        // tirar do tempo de espera o tempo que acabou de passar
        timersub(&(handler->timer.delta), &(handler->cronometro.delta), &(handler->timer.delta));

        float tmptmp =  handler->timer.delta.tv_sec + handler->timer.delta.tv_usec / 1e6;
        if (tmptmp <= 0)
        {
          VERBOSE(printf("Continuar a enviar arquivo para cliente %d\n", handler->client));
          FD_SET(handler->client, &total_writefds);
          if (handler->client > maxfds)
            maxfds = handler->client;

          if (handler_list.smaller_timeout == &(handler->timer.delta))
          {
            if (handler_list.current == 1)
              handler_list.smaller_timeout = NULL;
            else
              get_new_smaller_timeout(&handler_list, handler);
          }
          handler->waiting = 0;
        }
        else
        {
          timer_start(&(handler->cronometro));
        }
      }

      /* Maquina de estados dos c_handlers */
      switch (handler->state)
      {
      case HEADER_RECEIVING:
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
          retval = receive_request(handler);
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

          // tomar diferentes acoes baseado no metodo
          // (continuar recebendo dados ou nao)
          if (find_crlf(handler->request))
          {
            switch (http_what_method(handler->request, handler->request_size))
            {
            case GET_M:
              handler->state = REQUEST_RECEIVED;
              break;
            case PUT_M:
              handler->state = BODY_RECEIVING;
              break;
            case UNKNOWN_M:
              // mandar mensagem de erro (wtf)
              break;
            default:
              // mandar mensagem de erro (metodo nao suportado)
              break;
            }

          }
        }
        break;

      case BODY_RECEIVING:
        //put - vou implementar depois

        break;

      case REQUEST_RECEIVED:
        LOG_WRITE("Mensagem recebida!");
        handler->state = REQUEST_ANALYZE;
        break;

      case REQUEST_ANALYZE:
        LOG_WRITE("Analisando pedido...");
        parse_request(handler);
        switch (http_what_method(handler->request, handler->request_size))
        {
        case GET_M:
          handler->state = GET_CHECK_FILE;
          break;
        case PUT_M:
          handler->state = PUT_CHECK_FILE;
          break;
        case UNKNOWN_M:
          // mandar mensagem de erro (wtf)
          break;
        default:
          // mandar mensagem de erro (metodo nao suportado)
          break;
        }
        break;

      case GET_CHECK_FILE:
        //checar arquivo handler->filepath
        retval = resolve_symlinks(handler->filepath, handler->filepathsize);
        if (http_status_is_error(retval))
        {
          handler->filestatus = retval;
          handler->state = ERROR_HANDLE;
          break;
        }

        retval = check_path(handler->filepath, rootdir, rootdirsize);
        if (http_status_is_error(retval))
        {
          handler->filestatus = retval;
          handler->state = ERROR_HANDLE;
          break;
        }

        if (check_file_is_dir(handler->filepath))
        {
          retval = append_index_html(handler->filepath, handler->filepathsize);
          if (retval == -1)
          {
            // buffer overflow, nao da pra anexar...
          }
        }

        retval = check_file(handler->filepath);
        if (http_status_is_error(retval))
        {
          handler->filestatus = retval;
          handler->state = ERROR_HANDLE;
          break;
        }

        // se chegou ate aqui, significa que nao tem erros! \o/
        handler->filestatus = OK_S;
        handler->filetype_size = http_get_file_type(handler->filepath, handler->filepathsize, handler->filetype, BUFFER_SIZE);
        handler->state = HEADER_PREPARE;
        break;

      case ERROR_HANDLE:

        strncpy(handler->filetype, "text/html", BUFFER_SIZE);
        handler->state = HEADER_PREPARE;
        break;

      case HEADER_PREPARE:
        handler->filestatusmsg_size = http_get_status_msg(handler->filestatus, handler->filestatusmsg, BUFFER_SIZE);

        if (http_status_is_error(handler->filestatus))
        {
          handler->error_html_size = build_error_html(handler->error_html, BUFFER_SIZE, handler->filestatus, handler->filestatusmsg);
          handler->filesize = handler->error_html_size;
        }
        else
        {
          handler->filesize = get_file_size(handler->filepath);
        }

        handler->answer_header_size = http_build_header(handler);

        FILE *fp = fmemopen(handler->answer_header, handler->answer_header_size, "r");
        if (fp == NULL)
        {
          //errno
        }

        open_file(handler, fp, handler->answer_header_size);

        handler->state = FILE_SENDING;
        handler->next_state = FILE_PREPARE;
        handler->timer_sizesent = 0;
        timer_start(&(handler->timer));
        LOG_WRITE("Enviando Header...");
        break;

      case FILE_PREPARE:

        if (http_status_is_error(handler->filestatus))
        {
          handler->filep = fmemopen(handler->error_html, handler->error_html_size, "r");
          if (handler->filep == NULL)
          {
            LOG_PERROR("Erro em main()->FILE_PREPARE->fmemopen()");
            handler->state = FINISHED;
            break;
          }
        }
        else
        {
          handler->filep = fopen(handler->filepath, "r");
          if (handler->filep == NULL)
          {
            LOG_PERROR("Erro em main()->FILE_PREPARE->fopen()");
            handler->state = FINISHED;
            break;
          }
        }

        open_file(handler, handler->filep, handler->filesize);

        handler->state = FILE_SENDING;
        handler->next_state = FINISHED;
        handler->timer_sizesent = 0;
        timer_start(&(handler->timer));
        LOG_WRITE("Enviando Arquivo...");
        break;

      case PUT_CHECK_FILE:
        //put - implementar depois
        break;


      case FILE_SENDING:
        // Checar se o cliente desconectou
        if (FD_ISSET(handler->client, &readfds))
        {
          retval = receive_request(handler);
          if (retval != 0)
            handler->state = FINISHED;
        }

        // Continuar mandando arquivo
        if (FD_ISSET(handler->client, &writefds))
        {
          float delta;

          if (handler->need_file_chunk == 1)
          {
            retval = get_chunk(handler);
            if (retval == -1)
            {
              LOG_WRITE("Erro na leitura do arquivo!");
              handler->state = FINISHED;
              break;
            }
            if (retval == 1)
            {
              //terminou de pegar do arquivo!
            }
            handler->need_file_chunk = 0;
          }

          timer_stop(&(handler->timer));
          delta = timer_delta(&(handler->timer));
          if (delta < 1)
          {
            if ((handler->timer_sizesent) < (handler->bandwidth))
            {
              retval = send_chunk(handler);
              if (retval == -1)
              {
                LOG_WRITE("Erro de conexao!");
                handler->state = FINISHED;
                break;
              }

              if (retval == 0)
                handler->need_file_chunk = 1;

              handler->timer_sizesent += retval;
              handler->output_sizesent += retval;
              handler->output_sizeleft -= retval;
            }
            // Ja mandei tudo o que podia mas ainda nao deu 1 segundo
            else
            {
              // Pra poupar processamento, tirar cliente do select()
              FD_CLR(handler->client, &total_writefds);

              VERBOSE(printf("Pausar o envio de arquivo para cliente %d\n", handler->client));

              if (maxfds == handler->client)
              {
                if (handler_list.current == 1)
                  maxfds = listener;
                else
                  get_new_maxfds(&maxfds, &handler_list, handler);
              }

              // delta = (1 - delta)
              timersub(&(handler_list.onesec_timeout), &(handler->timer.delta), &(handler->timer.delta));

              if (handler_list.smaller_timeout == NULL)
                handler_list.smaller_timeout = &(handler->timer.delta);
              else
              {
                if (timercmp(&(handler->timer.delta), handler_list.smaller_timeout, <))
                  handler_list.smaller_timeout = &(handler->timer.delta);
              }

              timer_start(&(handler->cronometro));
              handler->waiting = 1;
            }
          }
          // Ja passou de 1 segundo
          else
          {
            VERBOSE(printf ("Velocidade: %.2f Bytes/s para o cliente %d\n", (handler->timer_sizesent / delta), handler->client));
            timer_start(&(handler->timer));
            handler->timer_sizesent = 0;

            // Novo smaller timeout
            if (handler_list.smaller_timeout == &(handler->timer.delta))
            {
              if (handler_list.current == 1)
                handler_list.smaller_timeout = NULL;
              else
                get_new_smaller_timeout(&handler_list, handler);
            }
          }

          if (handler->output_sizesent >= handler->output_size)
            handler->state = FILE_SENT;
        }
        break;

      case FILE_SENT:
        LOG_WRITE("Enviado!");
        close_file(handler);
        handler->state = handler->next_state;
        break;

      case FINISHED:
        close_file(handler);
        FD_CLR(handler->client, &total_writefds);
        FD_CLR(handler->client, &total_readfds);

        if (handler_list.smaller_timeout == &(handler->timer.delta))
        {
          if (handler_list.current == 1)
            handler_list.smaller_timeout = NULL;
          else
            get_new_smaller_timeout(&handler_list, handler);
        }

        if (maxfds == handler->client)
        {
          if (handler_list.current == 1)
            maxfds = listener;
          else
            get_new_maxfds(&maxfds, &handler_list, handler);
        }

        close(handler->client);
        c_handler_remove(handler, &handler_list);
        c_handler_exit(handler);
        handler = NULL;

        LOG_WRITE("Cliente desconectou\n");
        printf("Requests Servidas: %d\n", total_clients);
        break;

      default:
        break;
      }

    if (handler != NULL)
      handler = handler->next;

    } /*  while (handler != NULL) */

  } /* while(1) */

  return 0;
}
