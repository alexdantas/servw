/**
 * @file client.h
 *
 * Definicao de procedimentos relacionados a servir clientes.
 */

#include <stdio.h>
#include <time.h>
#include "timer.h"

#ifndef CLIENT_H_DEFINED
#define CLIENT_H_DEFINED


#ifndef BUFFER_SIZE
  #define BUFFER_SIZE  256
#endif

struct c_handler_list
{
  int current;  /**< Quantos handlers estao servindo clientes agora */
  int max;      /**< Maximo de handlers possiveis ao mesmo tempo */
  struct timeval *smaller_timeout; /**< Menor timeout para sair do select() */
  struct timeval onesec_timeout;  /**< Guarda 'um segundo' para comparacao */
  struct c_handler *begin;  /**< Primeiro handler na lista */
  struct c_handler *end;    /**< Ultimo handler na lista */
};

struct c_handler
{
  struct c_handler *next; /**< Proximo handler na lista */

  int  client;                   /**< Socket do cliente servido. */
  int  state;                    /**< Estado em que se encontra o handler */
  int  bandwidth;                /**< Limite de banda - quantos bytes/segundo posso mandar por usuario */

  char request[BUFFER_SIZE * 3]; /**< Toda a request HTTP solicitada pelo cliente. */
  int  request_size;             /**< Tamanho de caracteres que 'request' suporta. */

  char filepath[BUFFER_SIZE];    /**< Localizacao do arquivo que o cliente solicitou. */
  int  filepathsize;

  int  filestatus;               /**< Indica se o arquivo existe ou qual erro esta associado a ele.
                                   *  Seus valores sao os mesmos da especificacao HTTP (status codes). */
  char filestatusmsg[BUFFER_SIZE]; /**< Mensagem equivalente ao status do arquivo. */
  int  filestatusmsg_size;         /**< O tamanho da mensagem de status do arquivo. */

  FILE*  filep;                  /**< Arquivo que o cliente pede */
  int    filesize;               /**< Tamanho do arquivo solicitado*/
  char   filetype[BUFFER_SIZE];  /**< O MIME-type do arquivo */
  int    filetype_size;          /**< Tamanho do MIME-type do arquivo */
  time_t filelastm;              /**< Data de ultima modificacao do arquivo */

  char answer_header[BUFFER_SIZE];     /**< Header a ser enviado como resposta ao cliente, antes do arquivo */
  int  answer_header_size;             /**< O tamanho total do header */

  FILE* output;
  int   output_size;
  int   output_sizeleft;
  int   output_sizesent;
  char  outputbuff[BUFFER_SIZE];
  int   outputbuff_size;
  int   outputbuff_sizeleft;
  int   outputbuff_sizesent;

  char error_html[BUFFER_SIZE];
  int  error_html_size;

  int need_file_chunk;           /**< Flag que indica se precisa pegar um pedaco do arquivo. */

  int timer_sizesent;        /**< Indica quanto foi mandado dentro de um intervalo */
  struct timert timer;       /**< */
  struct timert cronometro; /**< Vai subtrair o #timer a cada rodada do loop principal */
  int waiting;         /**< Indica se o cliente esta 'esperando' para receber dados entre segundos */

  int next_state; /**< Guarda o estado que tem que ir apos enviar o arquivo */
};




/*    Usando ASCIIDoc
 *

ASCIIDoc é um modo simples de se escrever documentos. Você escreve um documento plain-text com uma formatação específica e o programa +asciidoc+ converte em outros formatos.
Por exemplo, veja o bloco de texto abaixo:


Depois de aplicar a conversão para HTML, por exemplo, ele fica assim:


Welcome to my project site!

Here I'll talk about my programs, ideas and stuff.
I mostly program in C and I try hard to make simple, easy-to-read code. Also, I love ASCII Art and text-based software.

Current projects:

* nSnake (The classic snake game with ncurses)
**  A implementation of the classic snake game with textual interface. It is playable at command-line and uses the nCurses C library for graphics.
* Custom Doxygen CSS (A new interface for the Doxygen documentation)
**  My CSS mod of the default Doxygen documentation appearance. I try to comment a lot to ease new modifications.


 *
 */


/** Todos os estados possiveis de c_handler.
 */
enum states
{
  FINISHED = -1, HEADER_RECEIVING, BODY_RECEIVING, REQUEST_RECEIVED,
  REQUEST_ANALYZE, GET_CHECK_FILE, PUT_CHECK_FILE, HEADER_PREPARE, FILE_PREPARE,
  ERROR_HANDLE, FILE_SENDING, FILE_SENT

  //~ MSG_RECEIVING, MSG_RECEIVED, FILE_PROCESSING, HEADER_SENDING,
  //~ ERROR_HEADER_SENDING, FILE_SENDING, ERROR_SENDING,
  //~ HEADER_SENT, ERROR_HEADER_SENT, FILE_SENT, ERROR_SENT
};


int  c_handler_list_init(struct c_handler_list* l, int max_clients);
int  c_handler_init(struct c_handler** h, int sck, char* rootdir, size_t rootdirsize, int bandwidth);
int  c_handler_add(struct c_handler* h, struct c_handler_list* l);
int  c_handler_remove(struct c_handler* h, struct c_handler_list* l);
void c_handler_exit(struct c_handler* h);

int receive_request(struct c_handler* h);
int parse_request(struct c_handler* h);

void get_new_smaller_timeout (struct c_handler_list* l, struct c_handler *h);
void get_new_maxfds(int* maxfds, struct c_handler_list* l, struct c_handler* h);

int open_file(struct c_handler *h, FILE *file, size_t size);
int close_file(struct c_handler* h);
int get_chunk(struct c_handler* h);
int send_chunk(struct c_handler* h);
int resolve_symlinks(char *path, size_t size);
int check_path(char *path, char *rootdir, size_t rootdirsize);
int check_file(char *path);
int check_file_is_dir(char *path);
int append_index_html(char *path, size_t pathsize);
int get_file_size(char *path);

#endif /* CLIENT_H_DEFINED */
