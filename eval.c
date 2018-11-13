/* Copyright (c) 2010, 2011 Juergen Nickelsen <ni@jnickelsen.de>
 * See the file COPYRIGHT for details.
 */

#include "cbasics.h"
#include <stdlib.h>
#include "objects.h"
#include "eval.h"
#include "signals.h"
#include "printer.h"
#include "hashmap.h"
#include "io.h"
#include "builtins.h"
#include "gc.h"

long eval_count = 0;
long apply_count = 0;
long bind_count = 0;

/**
 * return a function object if argument is a proper function
 */
obp_t make_function(char *name, uint namelen, obp_t func, session_context_t *sc)
{
        PROTECT;
        short minargs = 0;
        short maxargs = 0;
        PROTVAR(retval);
        
        if (IS(func, FUNCTION)) {
                return func;
        }
        if (!IS(func, PAIR)) {
                ERROR(sc->out, ERR_NOFUNC, func, "not function object or list");
        }
        obp_t marker = CAR(func);
        if (marker != the_Lambda && marker != the_Mu) {
                ERROR(sc->out, ERR_NOFUNC, func,
                      "is not builtin or lambda or special form");
        }
        obp_t body = CDR(func);
        if (!IS(body, PAIR)) {
                ERROR(sc->out, ERR_NOFUNC, func,
                      "lambda or special form body is not a list");
        }
        obp_t arglist = CAR(body);
        /* arglist of the form (sym sym sym) or (sym sym . sym) */
        while (IS(arglist, PAIR)) {
                obp_t sym = CAR(arglist);
                if (!IS(sym, SYMBOL)) {
                        ERROR(sc->out, ERR_NOFUNC, func,
                              "argument list member is not a symbol");
                }
                minargs++;
                maxargs++;
                arglist = CDR(arglist);
        }
        if (!IS_NIL(arglist)) {
                if (IS(arglist, SYMBOL)) {
                        maxargs = -1;
                } else {
                        ERROR(sc->out, ERR_NOFUNC, func,
                              "argument list end not nil or symbol");
                }
        }
        body = CDR(body);
        while (IS(body, PAIR)) {
                body = CDR(body);
        }
        if (!IS_NIL(body)) {
                ERROR(sc->out, ERR_NOFUNC, func, "body is not a proper list");
        }
        retval = new_form_function(name, namelen, func,
                                   marker == the_Mu, minargs, maxargs);
    EXIT:
        UNPROTECT;
        return retval;
}


void restore_bindings(obp_t params, int nargs, session_context_t *sc, int level)
{
        if (traceflag) {
                port_printf(sc->out, "%s* unbind ", blanks(level));
        }
        while (nargs--) {
                obp_t sym = CAR(params);
                AS(sym, SYMBOL)->value = popup();
                params = CDR(params);
                if (traceflag) {
                        print_expr((obp_t) sym, 0);
                        port_print(the_Stdout, " to ");
                        print_expr(AS(sym, SYMBOL)->value, 0);
                        printf(", ");
                }
        }
        if (traceflag) {
                terpri(0);
        }
}


obp_t make_bindings(obp_t params, obp_t args, session_context_t *sc, int level)
{
        PROTECT;
        PROTVAR(retval);
        
        if (traceflag) {
                printf("%s* bind ", blanks(level));
        }
        int nargs = 0;                  /* number of bindings actually saved */
        while (IS(args, PAIR) && IS(params, PAIR)) {
                obp_t car = CAR(params);
                Lsymbol_t *sym = AS(car, SYMBOL);
                pushdown(sym->value);
                sym->value = CAR(args);
                bind_count++;
                nargs++;
                if (traceflag) {
                        print_expr((obp_t) sym, 0);
                        port_print(the_Stdout, " to ");
                        print_expr(sym->value, 0);
                        port_print(the_Stdout, ", ");
                }
                args = CDR(args);
                params = CDR(params);
        }
        if (!IS_NIL(params)) {
                if (IS(params, SYMBOL)) {
                        Lsymbol_t *sym = AS(params, SYMBOL);
                        pushdown(sym->value);
                        sym->value = args;
                        bind_count++;
                        if (traceflag) {
                                print_expr((obp_t) sym, 0);
                                port_print(the_Stdout, " to ");
                                print_expr(sym->value, 0);
                                port_print(the_Stdout, ", ");
                        }
                } else {
                        restore_bindings(params, nargs, sc, level);
                        ERROR(sc->out, ERR_NOARGS, 0,
                              "too few arguments for function");
                }
        } else {
                if (args != the_Nil) {
                        restore_bindings(params, nargs, sc, level);
                        ERROR(sc->out, ERR_NOARGS, 0,
                              "too many arguments for function");
                }
        }
        if (traceflag) {
                terpri(0);
        }
        retval = new_integer(nargs);
    EXIT:
        UNPROTECT;
        return retval;
}


obp_t apply(obp_t fun, obp_t args, session_context_t *sc, int level)
{
        PROTECT;
        PROTVAR(retval);
        PROTVAR(nargs);
        apply_count++;

        /* We need not check for fun being a function, because we already have
         * a function object
         */
        assert(IS(fun, FUNCTION));
        if (IS_BUILTIN(fun)) {
                retval = call_builtin(fun, args, sc, level);
        } else if (IS_FORM(fun)) {
                obp_t form = AS(fun, FUNCTION)->impl.form;
                obp_t body = CDR(form); /* step over form */
                obp_t params = CAR(body);  /* formal parameters */
                nargs = make_bindings(params, args, sc, level);
                CHECK_ERROR(nargs);
                
                while (IS((body = CDR(body)), PAIR)) {
                        retval = eval(CAR(body), sc, level);
                        CHECK_ERROR(retval);
                }
                restore_bindings(params, (int) AS(nargs, NUMBER)->value,
                                 sc, level);
        } else {
                ERROR(sc->out, ERR_NOFUNC, fun, "not a valid function");
        }
    EXIT:
        UNPROTECT;
        return retval;
}


#define IS_LAMBDA(ob) (IS(ob, PAIR) && CAR(ob) == the_Lambda)
#define IS_MU(ob) (IS(ob, PAIR) && CAR(ob) == the_Mu)


obp_t autoload(obp_t fun, session_context_t *sc, int level)
{
        PROTECT;
        PROTVAR(retval);
        CHECKTYPE(sc->out, fun, FUNCTION);
        
        if (!IS_AUTOLOAD(fun)) {
                ERROR(sc->out, ERR_INVARG, fun, "not an autoload function");
        }
        Lfunction_t *func = AS(fun, FUNCTION);
        retval = load_file(AS(func->impl.filename, STRING)->content, sc, level);
        if (IS_ERROR(retval)) {
                ERROR(sc->out, ERR_NOAUTOL, fun, "load triggered error");
        }
        Lfunction_t *newfunc =
                AS(AS(intern(func->name, func->namelen), SYMBOL)->function,
                   FUNCTION);
        if (!newfunc) {
                ERROR(sc->out, ERR_NOAUTOL, fun, "function undefined");
        }
        if (newfunc->type == F_AUTOLOAD) {
                ERROR(sc->out, ERR_NOAUTOL, fun, "still not defined");
        }
        retval = (obp_t) newfunc;
    EXIT:
        UNPROTECT;
        return retval;
}


obp_t evalfun(obp_t func, obp_t args, session_context_t *sc, int level)
{
        PROTECT;
        PROTVAR(retval);
        PROTVAR(ev_args);                 /* evaluated arguments */
        PROTVAR(last);                    /* last element of list */
        PROTVAR(value);
        
        if (IS(func, PAIR)) {
                if (!IS_LAMBDA(func) && !IS_MU(func)) {
                        func = eval(func, sc, level);
                        CHECK_ERROR(func);
                }
        } else if (IS(func, SYMBOL)) {
                obp_t sym = func;
                func = AS(sym, SYMBOL)->function;
                if (func == NULL) {
                        func = AS(sym, SYMBOL)->value;
                }
                if (func == NULL) {
                        ERROR(sc->out, ERR_NOFUNC, sym,
                              "symbol has no function definition");
                }
        }
        if (IS_AUTOLOAD(func)) {
                func = autoload(func, sc, level);
                CHECK_ERROR(func);
        }
        /* now we *should* have a function object of any kind  */
        if (!IS(func, FUNCTION) && !IS_LAMBDA(func) && !IS_MU(func)) {
                ERROR(sc->out, ERR_NOFUNC, func, "not a function object");
        }
        if (IS_LAMBDA(func) || IS_MU(func)) { /* may still be a constructed form! */
                func = make_function(0, 0, func, sc);
                CHECK_ERROR(func);
        } else if (!IS(func, FUNCTION)) {
                ERROR(sc->out, ERR_NOFUNC, func, "not a function object");
        }
        /* now we *have* a function object of any kind  */
        
        if (!IS_SPECIAL(func)) {
                /* must evaluate arguments */
                for (obp_t elem = args; IS(elem, PAIR); elem = CDR(elem)) {
                        value = eval(CAR(elem), sc, level);
                        CHECK_ERROR(value);
                        if (ev_args == the_Nil) {
                                ev_args = last = new_pair(value, the_Nil);
                        } else {
                                obp_t p = new_pair(value, the_Nil);
                                CDR(last) = p;
                                last = p;
                        }
                }
                args = ev_args;
        }
        if (AS(func, FUNCTION)->trace) {
                strbuf_t sb = strbuf_new();
                sb = s_expr(func, sb, 0);
                port_printf(sc->out, "%s(%s", blanks(level), strbuf_string(sb));
                for (obp_t elem = ev_args; IS(elem, PAIR); elem = CDR(elem)) {
                        strbuf_reinit(sb);
                        sb = s_expr(elem, sb, 0);
                        port_printf(sc->out, " %s", strbuf_string(sb));
                }
                port_printf(sc->out, ")\n");
                free(sb);
        }
        retval = apply(func, args, sc, level);
    EXIT:
        UNPROTECT;
        return retval;
}


obp_t eval(obp_t ob, session_context_t *sc, int level)
{
        PROTECT;
        PROTVAR(retval);
        Lpair_t *pair;

        eval_count++;
        if (traceflag) {
                port_printf(sc->out, "%seval[%d]", blanks(level), level);
        }
        switch (ob->type) {
            case SYMBOL:
                retval = AS(ob, SYMBOL)->value;
                if (!retval) {
                        ERROR(sc->out, ERR_EVAL, ob, "symbol undefined");
                }
                if (traceflag) {
                        print_expr(new_zstring(" sym: "), sc->out);
                        print_expr(ob, sc->out);
                        print_expr(new_zstring(" = "), sc->out);
                        print_expr(retval, sc->out);
                        terpri(sc->out);
                }
                break;
            case PAIR:
                pair = AS(ob, PAIR);
                if (traceflag) {
                        print_expr(new_zstring(" pair: "), sc->out);
                        print_expr(ob, sc->out);
                        terpri(sc->out);
                }
                retval = evalfun(pair->car, pair->cdr, sc, level + 1);
                break;
            default:
                if (traceflag) {
                        print_expr(new_zstring(" other: "), sc->out);
                        print_expr(ob, sc->out);
                        terpri(sc->out);
                }
                retval = ob;
                break;
        }
        if (traceflag) {
                port_printf(sc->out,
                            "%s=val[%d] ", blanks(level), level);
                print_expr(retval, sc->out);
                terpri(sc->out);
        } else if (IS_EXIT(retval)) {
                port_printf(sc->out, "#%d: ", level);
                print_expr(ob, sc->out);
                terpri(sc->out);
        }
    EXIT:
        UNPROTECT;
        return retval;
}

 
/* EOF */
