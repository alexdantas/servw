
/* Macro para tornar mais facil escrever dados nos logs.
 *
 * Log normal:   stdout
 * Log de errro: stderr
 *
 */


#define WILL_WRITE_TO_LOG              1

#define LOG_WRITE(a)                   if (WILL_WRITE_TO_LOG == 1)     \
                                       {                               \
                                         fprintf(stdout, a);           \
                                         fprintf(stdout, "\n");        \
                                         fflush(stdout);               \
                                       }

#define LOG_FLUSH                      fflush(stdout);

#define LOG_WRITE_ERROR(a)             if (WILL_WRITE_TO_LOG == 1)     \
                                       {                               \
                                         fprintf(stderr, a);           \
                                         fprintf(stdout, "Erro gravado em stderr\n"); \
                                       }


#define VERBOSE_MODE                   0

#define VERBOSE(printf_command)        if (VERBOSE_MODE == 1)          \
                                       {                               \
                                         printf_command;               \
                                         fflush(stdout);               \
                                       }
