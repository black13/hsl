/* Copyright (c) 2010, 2011 Juergen Nickelsen <ni@jnickelsen.de>
 * See the file COPYRIGHT for details.
 */

#include "objects.h"

#define SIG_LERROR     1                /* an internal error */
#define SIG_UERROR     2                /* user error */
#define SIG_MESSAGE    3
#define SIG_THROW      4

#define ERR_RSYNTAX        1            /* reader syntax error */
#define ERR_READEOF        2            /* preliminary EOF in reader */
#define ERR_EVAL           3            /* eval error */
#define ERR_NOFUNC         4            /* not a function */
#define ERR_NOARGS         5            /* invalid argument count */
#define ERR_NOLIST         6            /* list operation on non-list */
#define ERR_NOSYMBOL       7            /* not a symbol */
#define ERR_INVARG         8            /* invalid argument */
#define ERR_SYSTEM         9            /* system call error */
#define ERR_CLPORT        10            /* port is not open */
#define ERR_INTERN        11            /* internal error */
#define ERR_MEMORY        12            /* unexpected memory shortage */
#define ERR_IO            13            /* I/O error */
#define ERR_LETARGS       14            /* invalid let argument list */
#define ERR_IMMUTBL       15            /* value may not be changed */
#define ERR_NOAUTOL       16            /* autoload failed to define function */

#define IS_EXIT(o) (o && IS(o, SIGNAL) &&               \
                    (AS(o, SIGNAL)->type == SIG_LERROR  \
                     || AS(o, SIGNAL)->type == SIG_THROW))
#define IS_ERROR(o) (o && IS(o, SIGNAL) &&              \
                     AS(o, SIGNAL)->type == SIG_LERROR)
#define CHECK_ERROR(o)                                                  \
        do {                                                            \
                if (!o || IS_EXIT(o)) {                                 \
                        retval = o; goto EXIT;                          \
                }                                                       \
        } while (0)
#define ERROR(...)  retval = throw_error(__VA_ARGS__); goto EXIT

obp_t new_signal(short type, short code, obp_t data, obp_t message);
obp_t throw_error(obp_t out_port, short code, obp_t ob, char *format, ...);
char *signal_type(short type);
char *signal_to_string(obp_t ob, int flags);
void print_error(obp_t error, obp_t port);
char *error_string(int errtype);

void print_signal(obp_t signal, obp_t port);
