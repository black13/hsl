/* Copyright (c) 2010, 2011 Juergen Nickelsen <ni@jnickelsen.de>
 * See the file COPYRIGHT for details.
 */

#ifndef __SESSION_H_INC
#define __SESSION_H_INC

#include "cbasics.h"
#include "strbuf.h"

typedef struct SESSION {
        obp_t in;
        obp_t out;
        char *name;                     /* name of input stream */
        strbuf_t tokbuf;                /* content of multi-character tokens */
        obp_t tok_atom;                 /* the value in case of T_ISATOM */
        int pushback_token;
        int lineno;
        int column;
        uint is_interactive:1;
} session_context_t;

session_context_t *new_session(obp_t in, obp_t out, int interactive);
void free_session(session_context_t *sc);

obp_t repl(session_context_t *sc, int level);

#endif  /* __SESSION_H_INC */
