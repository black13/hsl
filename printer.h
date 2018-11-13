/* Copyright (c) 2010, 2011 Juergen Nickelsen <ni@jnickelsen.de>
 * See the file COPYRIGHT for details.
 */


#include "cbasics.h"
#include "strbuf.h"

/* flags to s_expr */
#define TOSTRING_DEBUG      1           /* include debug information about an
                                           object */
#define TOSTRING_STATIC     2           /* string will be used and forgotten
                                           before the next call to a stringer,
                                           so one that is overwritten on the
                                           next call will suffice */
#define TOSTRING_READ       4           /* should have read syntax, if
                                           possible */
#define TOSTRING_PRINT      8           /* decorate with "\n%s " for (print) */
#define TOSTRING_BRIEF     16           /* no allocation, no traversal */

strbuf_t describe(obp_t ob, int do_print);
void terpri(obp_t port);
void print_expr(obp_t ob, obp_t port);
obp_t princ_string(obp_t ob);
obp_t prin1_string(obp_t ob);
obp_t print_string(obp_t ob);
void print(obp_t ob);
char *blanks(int n);
strbuf_t s_expr(obp_t ob, strbuf_t sb, int flags);
