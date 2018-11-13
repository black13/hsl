/* Copyright (c) 2010, 2011 Juergen Nickelsen <ni@jnickelsen.de>
 * See the file COPYRIGHT for details.
 */

#include <stdlib.h>
#include "session.h"
#include "io.h"
#include "reader.h"
#include "printer.h"
#include "eval.h"
#include "signals.h"
#include "gc.h"
#include "xmemory.h"

#define PROMPT "> "


session_context_t *new_session(obp_t in, obp_t out, int interactive)
{
        session_context_t *sc = xcalloc(sizeof(*sc), 1, "new session");
        sc->name = xstrdup(AS(in, PORT)->name, "session name");
        sc->in = in;
        sc->out = out;
        sc->is_interactive = interactive;
        new_reader(sc);
        return sc;
}


void free_session(session_context_t *sc)
{
        if (sc->tokbuf) {
                free(sc->tokbuf);
        }
        free(sc->name);
        free(sc);
}


obp_t repl(session_context_t *sc, int level)
{
        PROTECT;
        PROTVAR(value);
        PROTVAR(expr);
        protect(sc->in);
        protect(sc->out);

        while (1) {
                if (sc->is_interactive) {
                        port_printf(sc->out,
                                    "%ld obs %ld evals %ld applys %ld bindings\n",
                                    object_count, eval_count, apply_count,
                                    bind_count);
                        port_print(sc->out, PROMPT);
                        port_flush(sc->out);
                }
                expr = read_expr(sc);
                if (traceflag) {
                        port_print(sc->out, "read expression: ");
                        print_expr(expr, sc->out);
                        terpri(0);
                }
                if (!expr) {
                        break;
                }
                value = eval(expr, sc, level);
                if (sc->is_interactive) {
                        if (!IS_EXIT(value)) {
                                print_expr(value, sc->out);
                                terpri(sc->out);
                        }
                } else {
                        if (IS_EXIT(value)) { 
                                break;
                        }
                }
        }
        if (sc->is_interactive) {
                terpri(sc->out);
        }
        UNPROTECT;
        return value;
}

/* EOF */
