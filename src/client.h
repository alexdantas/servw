/**
 * @file client.h
 *
 * Definicao de procedimentos relacionados a servir clientes.
 */


#ifndef CLIENT_H_DEFINED
#define CLIENT_H_DEFINED


#ifndef BUFFER_SIZE
  #define BUFFER_SIZE  256
#endif

struct c_handler_list
{
  int current;             /**< Quantos handlers estao servindo clientes agora */
  int max;                 /**< Maximo de handlers possiveis ao mesmo tempo */

  struct c_handler* begin; /**< Primeiro handler na lista */
  struct c_handler* end;   /**< Ultimo handler na lista */
};

struct c_handler
{
  struct c_handler* next;        /**< Proximo handler na lista */

  int  client;                   /**< Socket do cliente servido. */
  int  state;                    /**< Estado em que se encontra o handler */
  char request[BUFFER_SIZE * 3]; /**< Toda a request HTTP solicitada pelo cliente. */
  int  request_size;             /**< Tamanho de caracteres que 'request' suporta. */
  char filepath[BUFFER_SIZE];    /**< Localizacao do arquivo que o cliente solicitou. */
  int  filestatus;               /**< Indica se o arquivo existe ou qual erro esta associado a ele.
                                   *  Seus valores sao os mesmos da especificacao HTTP (status codes). */

  char filestatusmsg[BUFFER_SIZE]; /**< Mensagem equivalente ao status do arquivo. */
  int  filestatusmsg_size;         /**< O tamanho da mensagem de status do arquivo. */

  FILE* filep;                   /**< Arquivo que o cliente pede */
  int   filesize;                /**< Tamanho do arquivo solicitado*/
  int   filesentsize;            /**< Tamanho que ja foi enviado do arquivo como um todo */
  time_t filelastm;              /**< Data de ultima modificacao do arquivo */

  char filebuff[BUFFER_SIZE];    /**< Buffer onde serao guardadas partes temporarias do arquivo */
  int  filebuffsize;

  char answer[BUFFER_SIZE];     /**< Header a ser enviado como resposta ao cliente, antes do arquivo */
  int  answer_size;             /**< O tamanho total do header */

  int size_left; /**< Quanto do arquivo (caso exista) ainda precisa ser enviado ao cliente. */
  int size_sent; /**< Quanto do arquivo (caso exista) ja foi enviado ao cliente. */

  char fileerror[BUFFER_SIZE]; /** Se houver algum erro relacionado ao arquivo, sua mensagem estara
                                 *  aqui (no formato de pagina HTML) para ser enviada ao cliente. */
  int fileerrorsize;           /**< Tamanho da pagina HTML de erro */

  char *output;      /**< Ponteiro que vai indicar o que vai ser enviado - o erro ou o header */

  int need_file_chunk;           /** Flag que indica se precisa pegar um pedaco do arquivo. */
};




/** Todos os estados possiveis do c_handler.
 */
enum states
{
  FINISHED, MSG_RECEIVING, FILE_PROCESSING, HEADER_SENDING,
  ERROR_HEADER_SENDING, FILE_SENDING, ERROR_SENDING, MSG_RECEIVED,
  HEADER_SENT, ERROR_HEADER_SENT, FILE_SENT, ERROR_SENT
};


int  c_handler_list_init(struct c_handler_list* l, int max_clients);
int  c_handler_init(struct c_handler** h, int sck, char* rootdir);
int  c_handler_add(struct c_handler* h, struct c_handler_list* l);
int  c_handler_remove(struct c_handler* h, struct c_handler_list* l);
void c_handler_exit(struct c_handler* h);

int  receive_message(struct c_handler* h);
int  file_check(struct c_handler* h, char* rootdir, int rootdirsize);
int  prepare_msg_to_send(struct c_handler* h);
int  keep_sending_msg(struct c_handler* h);
int  start_sending_file(struct c_handler* h);
int  stop_sending_file(struct c_handler* h);
int  get_file_chunk(struct c_handler* h);
void build_error_html(struct c_handler* h);
int  parse_request(struct c_handler* h);
int  build_header(struct c_handler* h);

#endif /* CLIENT_H_DEFINED */
