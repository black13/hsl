/* Copyright (c) 2010, 2011 Juergen Nickelsen <ni@jnickelsen.de>
 * See the file COPYRIGHT for details.
 */

#include "cbasics.h"
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <stddef.h>
#include "builtins.h"
#include "names.h"
#include "signals.h"
#include "eval.h"
#include "io.h"
#include "reader.h"
#include "printer.h"
#include "gc.h"


/*
 * All builtin functions are called by call_builtin(). They have the number of
 * arguments checked beforehand, so they can rely on the actual number of
 * arguments being in the range specified with register_builtin(). The args list
 * is the one created by evalfun() and is already gc-protected. The sc->out
 * argument is the port to print errors to etc.; it will be replaced by a more
 * generalized interpreter context (or rather repl context) finally.
 */


obp_t new_autoload_function(char *name, uint namelen_t, obp_t filename,
                            int is_special);

/**
 * Return the length of the list, i. e. the number of pairs.
 */
uint list_length(obp_t list)
{
        uint length = 0;

        for ( ; IS(list, PAIR); list = CDR(list)) {
                length++;
        }
        return length;
}

/**
 * Call a builtin function and return the value (or error). The function object
 * fun must be of type F_BUILTIN. Check the number of arguments against the
 * range specified with register_builtin().
 */
obp_t call_builtin(obp_t fun, obp_t args, session_context_t *sc, int level)
{
        Lfunction_t *bi = AS(fun, FUNCTION);
        int nargs = list_length(args);

        if (nargs < bi->minargs ||
            (bi->maxargs >= 0 && nargs > bi->maxargs))
        {
                return throw_error(sc->out, ERR_NOARGS, fun,
                                   "invalid number of arguments, %d not in [%d..%d]",
                                   nargs, bi->minargs, bi->maxargs);
        }
        return bi->impl.builtin(nargs, args, sc, level);
}

/**
 * Return the length of the argument. Supported are lists, strings, vectors,
 * maps (number of entries), strbufs, symbols (length of name).
 * (length arg)
 */
obp_t bf_length(int nargs, obp_t args, session_context_t *sc, int level)
{
        obp_t ob = CAR(args);
        long value;
        switch (ob->type) {
            case PAIR:
                value = list_length(ob);
                break;
            case STRING:
                value = AS(ob, STRING)->length;
                break;
            case VECTOR:
                value = AS(ob, VECTOR)->nelem;
                break;
            case MAP:
                value = hashmap_size(AS(ob, MAP)->map);
                break;
            case STRBUF:
                value = strbuf_size(AS(ob, STRBUF)->strbuf);
                break;
            case SYMBOL:
                value = AS(AS(ob, SYMBOL)->name, STRING)->length;
                break;
            default:
                return throw_error(sc->out, ERR_INVARG, ob,
                                   "sorry, I cannot come up with any reasonable meaning of \"length\" for objects of type %s",
                                   type_name(ob->type));
        }
        return new_integer(value);
}

/**
 * (setq sym1 val1 ... ...)
 */
obp_t bf_setq(int nargs, obp_t args, session_context_t *sc, int level)
{
        PROTECT;
        PROTVAR(retval);
        PROTVAR(symbol);
        
        if (nargs % 2 != 0) {
                ERROR(sc->out, ERR_NOARGS, args,
                      "uneven number of args to setq");
        }
        while (args != the_Nil) {
                symbol = CAR(args);
                CHECKTYPE(sc->out, symbol, SYMBOL);
                if (symbol->immutable) {
                        ERROR(sc->out, ERR_IMMUTBL, symbol,
                              "symbol value may not be modified");
                }
                args = CDR(args);
                retval = eval(CAR(args), sc, level);
                CHECK_ERROR(retval);
                AS(symbol, SYMBOL)->value = retval;
                args = CDR(args);
        }
    EXIT:
        UNPROTECT;
        return retval;
}

/**
 * Return a function object. (lambda args [bodyform ...])
 */
obp_t bf_lambda(int nargs, obp_t args, session_context_t *sc, int level)
{
        return make_function(0, 0, new_pair(the_Lambda, args), sc);
}


obp_t bf_if(int nargs, obp_t args, session_context_t *sc, int level)
{
        PROTECT;
        PROTVAR(retval);
        PROTVAR(cond);

        if (IS(args, PAIR)) {
                cond = eval(CAR(args), sc, level);
                CHECK_ERROR(cond);
                args = CDR(args);
                if (cond != the_Nil) {
                        if (IS(args, PAIR)) {
                                retval = eval(CAR(args), sc, level);
                                CHECK_ERROR(retval);
                        }
                } else {
                        args = CDR(args);
                        while (args != the_Nil && IS(args, PAIR)) {
                                retval = eval(CAR(args), sc, level);
                                CHECK_ERROR(retval);
                                args = CDR(args);
                        }
                }
        }
    EXIT:
        UNPROTECT;
        return retval;
}

/**
 * Return the car of a pair or, of nil, nil.
 * (car arg)
 */
obp_t bf_car(int nargs, obp_t args, session_context_t *sc, int level)
{
        obp_t arg = CAR(args);
        if (IS(arg, PAIR)) {
                return CAR(arg);
        } else if (IS_NIL(arg)) {
                return the_Nil;
        } else {
                return throw_error(sc->out, ERR_NOLIST, arg, "car of non-list");
        }
}

/**
 * Return the cdr of a pair or, of nil, nil.
 * (cdr arg)
 */
obp_t bf_cdr(int nargs, obp_t args, session_context_t *sc, int level)
{
        obp_t arg = CAR(args);
        if (IS(arg, PAIR)) {
                return CDR(arg);
        } else if (IS_NIL(arg)) {
                return the_Nil;
        } else {
                return throw_error(sc->out, ERR_NOLIST, arg, "cdr of non-list");
        }
}

/**
 * (cons carval cdrval)
 */
obp_t bf_cons(int nargs, obp_t args, session_context_t *sc, int level)
{
        obp_t arg1 = CAR(args);
        obp_t arg2 = CADR(args);
        return new_pair(arg1, arg2);
}

/**
 * Evaluate an object and return the value.
 * (eval obj)
 */
obp_t bf_eval(int nargs, obp_t args, session_context_t *sc, int level)
{
        return eval(CAR(args), sc, level);
}

/**
 * Return the literal argument.
 * (quote arg)
 */
obp_t bf_quote(int nargs, obp_t args, session_context_t *sc, int level)
{
        return CAR(args);
}

/**
 * Load a file and evaluate its contents. The filename may be a string or a
 * symbol.
 * (load filename)
 */
obp_t bf_load(int nargs, obp_t args, session_context_t *sc, int level)
{
        PROTECT;
        PROTVAR(retval);
        PROTVAR(name);
        
        while (args != the_Nil) {
                name = CAR(args);
                if (IS(name, SYMBOL)) {
                        name = AS(name, SYMBOL)->name;
                }
                CHECKTYPE(sc->out, name, STRING);
                retval = load_file(AS(name, STRING)->content, sc, level);
                CHECK_ERROR(retval);
                args = CDR(args);
        }
    EXIT:
        UNPROTECT;
        return retval;;
}

/**
 * Return the name of a symbol as a string.
 * (symbol-name sym)
 */
obp_t bf_symbol_name(int nargs, obp_t args, session_context_t *sc, int level)
{
        obp_t sym = CAR(args);
        CHECKTYPE_RET(sc->out, sym, SYMBOL);
        return AS(sym, SYMBOL)->name;
}

/**
 * Return the function associated with a symbol.
 * (symbol-function sym)
 */
obp_t bf_symbol_function(int nargs, obp_t args, session_context_t *sc, int level)
{
        obp_t sym = CAR(args);
        CHECKTYPE_RET(sc->out, sym, SYMBOL);
        return AS(sym, SYMBOL)->function;
}

/**
 * Return the list of all interned symbols.
 * (symbols)
 */
obp_t bf_symbols(int nargs, obp_t args, session_context_t *sc, int level)
{
        return all_symbols();
}

/**
 * Set the function associated with a symbol.
 (fset sym function)
 */
obp_t bf_fset(int nargs, obp_t args, session_context_t *sc, int level)
{
        PROTECT;
        PROTVAR(retval);
        PROTVAR(func);
        obp_t sym = CAR(args);
        obp_t form = CADR(args);

        if (sym->immutable) {
                ERROR(sc->out, ERR_IMMUTBL, sym,
                      "symbol's function cell may not be modified");
        }
        func = make_function(THE_STRINGS(AS(sym, SYMBOL)->name), form, sc);
        CHECK_ERROR(func);
        AS(sym, SYMBOL)->function = func;
        retval = func;
    EXIT:
        UNPROTECT;
        return retval;
}

/**
 * Common part of function and special form definitions.
 */
obp_t def_common(obp_t args, session_context_t *sc, obp_t marker)
{
        PROTECT;
        PROTVAR(form);
        PROTVAR(func);
        PROTVAR(retval);
        obp_t sym = CAR(args);
        
        form = new_pair(marker, CDR(args));
        func = make_function(THE_STRINGS(AS(sym, SYMBOL)->name), form, sc);
        CHECK_ERROR(func);
        AS(sym, SYMBOL)->function = func;
        retval = sym;
    EXIT:
        UNPROTECT;
        return retval;
}        

/**
 * Define a function.
 * (defun name args [bodyform ...])
 */
obp_t bf_defun(int nargs, obp_t args, session_context_t *sc, int level)
{
        return def_common(args, sc, the_Lambda);
}

/**
 * Define a special form. Arguments are passed unevaluated.
 * (defspecial name args [bodyform ...])
 */
obp_t bf_defspecial(int nargs, obp_t args, session_context_t *sc, int level)
{
        return def_common(args, sc, the_Mu);
}

/**
 * Return t if the two arguments are the same object, nil otherwise.
 * (eq arg1 arg2)
 */
obp_t bf_eq(int nargs, obp_t args, session_context_t *sc, int level)
{
        return CAR(args) == CADR(args) ? the_T : the_Nil;
}

/**
 * Return t iff the two arguments are the same object or have the same (not
 * equal!) value.
 * (eql arg1 arg2)
 */
obp_t bf_eql(int nargs, obp_t args, session_context_t *sc, int level)
{
        obp_t arg1 = CAR(args);
        obp_t arg2 = CADR(args);

        if (arg1 == arg2) {
                return the_T;
        } else if (arg1->size == arg2->size
                   && !memcmp(&arg1->size,
                              &arg2->size,
                              arg1->size - offsetof(Lobject_t, size))) {
                return the_T;
        } else {
                return the_Nil;
        }
}

/**
 * Return t iff the argument is nil.
 * (null arg)
 */
obp_t bf_null(int nargs, obp_t args, session_context_t *sc, int level)
{
        obp_t arg = CAR(args);
        return (arg == the_Nil ? the_T : the_Nil);
}

/**
 * With an argument, set the traceflag if the arguments value is not nil, or
 * clear it otherwise. Return t if the trace flag is set, nil otherwise.
 * (trace [arg])
 */
obp_t bf_trace(int nargs, obp_t args, session_context_t *sc, int level)
{
        if (args != the_Nil) {
                traceflag = CAR(args) == the_Nil ? 0 : 1;
        }
        return traceflag ? the_T : the_Nil;
}

/**
 * Return the name of the tty associated with port (or stdin). Return nil if
 * there is no tty; return t if there is one but could not be found.
 * (tty [port])
 */
obp_t bf_tty(int nargs, obp_t args, session_context_t *sc, int level)
{
        obp_t port = the_Stdin;
        if (args != the_Nil) {
                if (CDR(args) != the_Nil) {
                        return throw_error(sc->out, ERR_INVARG, 0,
                                           "too many arguments for " TTY_NAME);
                }
                port = CAR(args);
                CHECKTYPE_RET(sc->out, port, PORT);
        }
        return port_tty(port);
}

/**
 * Return T if the argument is not a pair, Nil otherwise.
 * (atom arg)
 */
obp_t bf_atom(int nargs, obp_t args, session_context_t *sc, int level)
{
        obp_t arg = CAR(args);
        return IS(arg, PAIR) ? the_Nil : the_T;
}

/**
 * Return the argument as a function. That is, a function object for a list (or
 * an error if the list does not represent a function), the value of its
 * function cell for a symbol, and the argument itself if it already is a
 * function.
 * (function arg)
 */
obp_t bf_function(int nargs, obp_t args, session_context_t *sc, int level)
{
        obp_t arg = CAR(args);
        switch (arg->type) {
            case SYMBOL:
                return AS(arg, SYMBOL)->function;
            case PAIR: 
                return make_function(0, 0, arg, sc);
            case FUNCTION:
                return arg;
            default:
                return throw_error(sc->out, ERR_NOFUNC, arg, "not a function");
        }
}

/**
 * The cond special form: for each argument, which is a list of an antecedent
 * and one or more succeedents, evaluate the antecedent; if it is true
 * (non-nil), the value of the succeedents (with an implicit progn) if the value
 * of the cond form, and no other argument is evaluated. If none if the
 * antecedents is true, the value of the cond form is nil.
 * (cond (ante succ) ...)
 */
obp_t bf_cond(int nargs, obp_t args, session_context_t *sc, int level)
{
        PROTECT;
        PROTVAR(antecedent);
        PROTVAR(retval);

        while (!IS_NIL(args)) {
                obp_t clause = CAR(args);
                CHECKTYPE(sc->out, clause, PAIR);
                antecedent = eval(CAR(clause), sc, level);
                CHECK_ERROR(antecedent);
                if (!IS_NIL(antecedent)) {
                        retval = eval(CDR(clause), sc, level);
                        goto EXIT;
                }
        }
        retval = the_Nil;
    EXIT:
        UNPROTECT;
        return retval;
}

/**
 * Special form: evaluate all arguments and return a pair of the time in
 * microseconds it took to evaluate them and the value of the last argument
 * evaluated.
 * (time [arg1 ...])
 */
obp_t bf_time(int nargs, obp_t args, session_context_t *sc, int level)
{
        PROTECT;
        PROTVAR(retval);
        PROTVAR(timeval);
        struct timeval from;
        struct timeval to;

        gettimeofday(&from, 0);
        while (!IS_NIL(args)) {
                retval = eval(CAR(args), sc, level);
                CHECK_ERROR(retval);
                args = CDR(args);
        }
        gettimeofday(&to, 0);
        timeval = new_integer((to.tv_sec * 1000000l + to.tv_usec)
                              - (from.tv_sec * 1000000l + from.tv_usec));
        retval = cons(timeval, retval);
    EXIT:
        UNPROTECT;
        return retval;
}

struct timeval start_time;

/**
 * Measure the evaluation of the arguments. Return a pair of the evaluation
 * value and a list of measurement variables, each a pair of the name and the
 * value of the variable as follows; usecs: microseconds elapsed during
 * evaluation, evals: number of calls to eval, applys: number of calls to apply,
 * bindings: number of symbol bindings done by apply; objects: number of objects
 * allocated during evaluation. Without argument, just return the list of
 * measurement variables, counted from the interpreter start.
 * (measure [arg1 ...])
 */
obp_t bf_measure(int nargs, obp_t args, session_context_t *sc, int level)
{
        PROTECT;
        PROTVAR(usecs);
        PROTVAR(evals);
        PROTVAR(applys);
        PROTVAR(bindings);
        PROTVAR(objects);
        PROTVAR(retval);
        PROTVAR(value);
        PROTVAR(temp);

        struct timeval from;
        struct timeval to;
        long start_evals = eval_count;
        long start_bindings = bind_count;
        long start_applys = apply_count;
        long start_objects = object_count;

        gettimeofday(&from, 0);
        if (IS_NIL(args)) {
                usecs = new_integer((from.tv_sec * 1000000l + from.tv_usec)
                                    - (start_time.tv_sec * 1000000l
                                       + start_time.tv_usec));
                evals = new_integer(eval_count);
                applys = new_integer(apply_count);
                bindings = new_integer(bind_count);
                objects = new_integer(object_count);
        } else {
                while (!IS_NIL(args)) {
                        value = eval(CAR(args), sc, level);
                        CHECK_ERROR(value);
                        args = CDR(args);
                }
                gettimeofday(&to, 0);
                usecs = new_integer((to.tv_sec * 1000000l + to.tv_usec)
                                    - (from.tv_sec * 1000000l + from.tv_usec));
                evals = new_integer(eval_count - start_evals);
                applys = new_integer(apply_count - start_applys);
                bindings = new_integer(bind_count - start_bindings);
                objects = new_integer(object_count - start_objects);
        }
        temp = cons(intern_z(OBJECTS_NAME), objects);
        retval = cons(temp, the_Nil);
        temp = cons(intern_z(BINDINGS_NAME), bindings);
        retval = cons(temp, retval);
        temp = cons(intern_z(APPLYS_NAME), applys);
        retval = cons(temp, retval);
        temp = cons(intern_z(EVALS_NAME), evals);
        retval = cons(temp, retval);
        temp = cons(intern_z(USECS_NAME), usecs);
        retval = cons(temp, retval);
        retval = cons(value, retval);
    EXIT:
        UNPROTECT;
        return retval;
}

/**
 * Return the list of symbols containing the argument (a symbol or a string) as
 * a substring of their name.
 * (apropos arg)
 */
obp_t bf_apropos(int nargs, obp_t args, session_context_t *sc, int level)
{
        obp_t pattern = princ_string(CAR(args));
        Lstring_t *search = AS(pattern, STRING);
        hashmap_t syms = AS(symbols, MAP)->map;
        mapentry_t entry;
        obp_t result = the_Nil;

        hashmap_enum_start(syms);
        while ((entry = hashmap_enum_next(syms))) {
                obp_t name = entry_get_key(entry);
                if (strstr(AS(name, STRING)->content, search->content)) {
                        result = cons(entry_get_value(entry), result);
                }
        }
        return result;
}

/**
 * Call the function func with the arguments arg1, arg2, ... Funcall is similar
 * to apply except that the separate arguments are given to funcall, rather than
 * a list of arguments.
 * (funcall func [arg1 ...])
 */
obp_t bf_funcall(int nargs, obp_t args, session_context_t *sc, int level)
{
        obp_t fun = CAR(args);
        CHECKTYPE_RET(sc->out, fun, FUNCTION);
        obp_t arglist = CDR(args);
        return apply(fun, arglist, sc, level);
}

/**
 * Call the function func with the specified argument list.
 * (apply func arglist)
 */
obp_t bf_apply(int nargs, obp_t args, session_context_t *sc, int level)
{
        obp_t fun = CAR(args);
        CHECKTYPE_RET(sc->out, fun, FUNCTION);
        obp_t arglist = CADR(args);
        if (!IS_NIL(arglist) && !IS(arglist, PAIR)) {
                return throw_error(sc->out, ERR_NOLIST, arglist,
                                   "apply to non-list");
        }
        return apply(fun, arglist, sc, level);
}

/**
 * Return a list of the arguments.
 * (list [arg1 ...])
 */
obp_t bf_list(int nargs, obp_t args, session_context_t *sc, int level)
{
        return args;
}

/**
 * Evaluate forms in a new binding context. The first argument is a list of
 * bindings. Each element of this list is either a symbol, which is then bound
 * to nil in this context, or a list of a symbol and an optional value form, in
 * which case the symbol is bound to the value of the value form, or to nil if
 * no value form is specified. The bindings are done in parallel, meaning that
 * the value forms of the bindings are evaluated without any of the other
 * bindings already in place. The remaining arguments of the let, the body, are
 * forms evaluated in this binding context. The value of the last form is the
 * value of the let form.
 * (let ((sym1 val1) ...) bodyform1 ...)
 */
obp_t bf_let(int nargs, obp_t args, session_context_t *sc, int level)
{
        PROTECT;
        obp_t bindings = CAR(args);
        obp_t body = CDR(args);
        PROTVAR(retval);
        PROTVAR(symbols);
        PROTVAR(values);
        PROTVAR(nbindings);
        PROTVAR(sym);
        PROTVAR(value);
        
        while (IS(bindings, PAIR)) {
                obp_t binding = CAR(bindings);  /* (sym value) */
                if (IS(binding, SYMBOL)) {
                        sym = binding;
                        value = the_Nil;
                } else if (IS(binding, PAIR)) {
                        obp_t rest = CDR(binding);
                        sym = CAR(binding);
                        if (!IS(sym, SYMBOL)) {
                                ERROR(sc->out, ERR_LETARGS, sym,
                                      "not a symbol");
                        }
                        if (!IS(rest, PAIR) || !IS_NIL(CDR(rest))) {
                                ERROR(sc->out, ERR_LETARGS, binding,
                                      "malformed binding");
                        }
                        value = eval(CAR(rest), sc, level);
                        CHECK_ERROR(value);
                } else {
                        ERROR(sc->out, ERR_LETARGS, binding,
                              "not symbol or list");
                }
                symbols = cons(sym, symbols);
                values = cons(value, values);
                bindings = CDR(bindings);
        }
        if (!IS_NIL(bindings)) {
                ERROR(sc->out, ERR_LETARGS, CAR(args),
                      "bindings not a proper list");
        }
        nbindings = make_bindings(symbols, values, sc, level);
        CHECK_ERROR(nbindings);

        while (IS(body, PAIR)) {
                retval = eval(CAR(body), sc, level);
                CHECK_ERROR(retval);
                body = CDR(body);
        }
    EXIT:
        restore_bindings(symbols, AS(nbindings, NUMBER)->value, sc, level);
        UNPROTECT;
        return retval;
}

/**
 * Evaluate forms in a new binding context. The first argument is a list of
 * bindings. Each element of this list is either a symbol, which is then bound
 * to nil in this context, or a list of a symbol and an optional value form, in
 * which case the symbol is bound to the value of the value form, or to nil if
 * no value form is specified. Other than with let, the bindings are done in
 * sequence, meaning that the value forms of the bindings are evaluated with the
 * preceding bindings already in place. The remaining arguments of the let*, the
 * body, are forms evaluated in this binding context. The value of the last form
 * is the value of the let form.
 * (let* ((sym1 val1) ...) bodyform1 ...)
 */
obp_t bf_letrec(int nargs, obp_t args, session_context_t *sc, int level)
{
        PROTECT;
        obp_t bindings = CAR(args);
        int nbindings = 0;
        obp_t body = CDR(args);
        PROTVAR(retval);
        PROTVAR(symbols);
        PROTVAR(sym);
        PROTVAR(newvalue);

        while (IS(bindings, PAIR)) {
                obp_t binding = CAR(bindings);
                if (IS(binding, SYMBOL)) {
                        sym = binding;
                        newvalue = the_Nil;
                } else if (IS(binding, PAIR)) {
                        obp_t rest = CDR(binding);
                        sym = CAR(binding);

                        if (!IS(sym, SYMBOL)) {
                                ERROR(sc->out, ERR_LETARGS,
                                      sym, "not a symbol");
                        }
                        if (!IS(rest, PAIR) || !IS_NIL(CDR(rest))) {
                                ERROR(sc->out, ERR_LETARGS, binding,
                                      "malformed binding");
                        }
                        newvalue = eval(CAR(rest), sc, level);
                        CHECK_ERROR(newvalue);
                } else {
                        ERROR(sc->out, ERR_LETARGS, binding,
                              "not symbol or list");
                }
                Lsymbol_t *symbol = AS(sym, SYMBOL);
                pushdown(symbol->value);
                symbols = cons(sym, symbols);
                nbindings++;
                symbol->value = newvalue;
                bindings = CDR(bindings);
        }

        while (IS(body, PAIR)) {
                retval = eval(CAR(body), sc, level);
                CHECK_ERROR(retval);
                body = CDR(body);
        }

    EXIT:
        restore_bindings(symbols, nbindings, sc, level);
        UNPROTECT;
        return retval;
}

/**
 * Return T if the argument is a pair, and Nil otherwise.
 * (listp arg)
 */
obp_t bf_listp(int nargs, obp_t args, session_context_t *sc, int level)
{
        obp_t arg = CAR(args);
        return IS_LIST(arg) ? the_T : the_Nil;
}

/**
 * Evaluate all arguments in sequence, and if one of them evaluates to nil,
 * return nil immediately without evaluating any of the remaining arguments. If
 * all arguments evaluate to a non-nil value, return the value of the last
 * argument.
 * (and [arg1 ...])
 */
obp_t bf_and(int nargs, obp_t args, session_context_t *sc, int level)
{
        PROTECT;
        PROTVAL(retval, the_T);
        while (!IS_NIL(args)) {
                obp_t cond = CAR(args);
                retval = eval(cond, sc, level);
                CHECK_ERROR(retval);
                if (IS_NIL(retval)) {
                        break;
                }
        }
    EXIT:
        UNPROTECT;
        return retval;
}

/**
 * Evaluate all arguments in sequence, and if one of them evaluates to a non-nil
 * value, return this value immediately without evaluating any of the remaining
 * arguments. If all arguments evaluate to nil, return nil.
 * (or [arg1 ...])
 */
obp_t bf_or(int nargs, obp_t args, session_context_t *sc, int level)
{
        PROTECT;
        PROTVAR(retval);        
        while (!IS_NIL(args)) {
                obp_t cond = CAR(args);
                retval = eval(cond, sc, level);
                CHECK_ERROR(retval);
                if (!IS_NIL(retval)) {
                        break;
                }
        }
    EXIT:
        UNPROTECT;
        return retval;
}

/**
 * Evaluate all arguments in sequence and return the value of the last.
 */
obp_t bf_progn(int nargs, obp_t args, session_context_t *sc, int level)
{
        PROTECT;
        PROTVAR(retval);
        while (!IS_NIL(args)) {
                obp_t arg = CAR(args);
                retval = eval(arg, sc, level);
                CHECK_ERROR(retval);
                args = CDR(args);
        }
    EXIT:
        UNPROTECT;
        return retval;
}


/**
 * Evaluate all arguments in sequence and return the value of the first.
 */
obp_t bf_prog1(int nargs, obp_t args, session_context_t *sc, int level)
{
        PROTECT;
        obp_t first = CAR(args);
        PROTVAL(retval, eval(first, sc, level));
        CHECK_ERROR(retval);

        args = CDR(args);
        while (!IS_NIL(args)) {
                retval = eval(CAR(args), sc, level);
                CHECK_ERROR(retval);
                args = CDR(args);
        }
    EXIT:
        UNPROTECT;
        return retval;
}


/**
 * Evaluate all arguments in sequence and return the value of the second.
 */
obp_t bf_prog2(int nargs, obp_t args, session_context_t *sc, int level)
{
        PROTECT;
        PROTVAR(retval);
        obp_t first = CAR(args);
        PROTVAL(val, eval(first, sc, level));
        CHECK_ERROR(retval);
        args = CDR(args);

        obp_t second = CAR(args);
        retval = eval(second, sc, level);
        CHECK_ERROR(retval);
        args = CDR(args);
        
        while (!IS_NIL(args)) {
                val = eval(CAR(args), sc, level);
                CHECK_ERROR(val);
                args = CDR(args);
        }
    EXIT:
        UNPROTECT;
        return retval;
}

/**
 * If the condition argument evaluates to non-nil, evaluate the body forms;
 * repeat. The value of the while form is nil.
 * (while condition bodyform1 ...)
 */
obp_t bf_while(int nargs, obp_t args, session_context_t *sc, int level)
{
        PROTECT;
        PROTVAR(cond);
        PROTVAR(retval);
        obp_t test = CAR(args);
        obp_t body = CDR(args);

        do {
                cond = eval(test, sc, level);
                CHECK_ERROR(cond);
                if (IS_NIL(cond)) {
                        break;
                }
                obp_t forms = body;
                while (!IS_NIL(forms)) {
                        CHECK_ERROR(eval(CAR(forms), sc, level));
                        forms = CDR(forms);
                }
        } while (1);
    EXIT:
        UNPROTECT;
        return retval;
}

/**
 * Evaluate the first argument, and even in case of an error or a throw,
 * evaluate the remaining arguments. Return the value of the first form.
 * (unwind-protect valueform cleanupform1 ...)
 */
obp_t bf_unwind_protect(int nargs, obp_t args, session_context_t *sc, int level)
{
        obp_t form = CAR(args);
        obp_t cleanupforms = CDR(args);

        PROTECT;
        PROTVAL(retval, eval(form, sc, level));
        while (!IS_NIL(cleanupforms)) {
                eval(CAR(cleanupforms), sc, level);
                cleanupforms = CDR(cleanupforms);
        }
        UNPROTECT;
        return retval;                   /* which may be a error */
}

/**
 * Return a string containing the printed representation of the argument. This
 * representation is meant for display, not for being read in again as a Lisp
 * object.
 * (princs arg)
 */
obp_t bf_princs(int nargs, obp_t args, session_context_t *sc, int level)
{
        obp_t expr = CAR(args);
        return princ_string(expr);
}

/**
 * Return a string containing the printed representation of the argument. This
 * representation is meant to be readable by the Lisp reader to re-create the
 * argument object, if possible.
 * (prin1s arg)
 */
obp_t bf_prin1s(int nargs, obp_t args, session_context_t *sc, int level)
{
        obp_t expr = CAR(args);
        return prin1_string(expr);
}

/**
 * Print the argument to the specified port (or stdout, if no port or nil is
 * specified). The output is meant to be readable by the Lisp reader to
 * re-create the argument object, if possible.
 * (prin1 arg [port])
 */
obp_t bf_prin1(int nargs, obp_t args, session_context_t *sc, int level)
{
        obp_t expr = CAR(args);
        args = CDR(args);
        if (!IS_NIL(args)) {
                sc->out = CAR(args);
                if (IS_NIL(sc->out)) {
                        sc->out = the_Stdout;
                }
        }
        obp_t s = prin1_string(expr);
        port_write(sc->out, THE_STRINGS(s));
        return s;
}

/**
 * Print the argument to the specified port (or stdout, if no port or nil is
 * specified). The output is meant for display, not for being read in again as a
 * Lisp object.
 * (princ arg [port])
 */
obp_t bf_princ(int nargs, obp_t args, session_context_t *sc, int level)
{
        obp_t expr = CAR(args);
        args = CDR(args);
        if (!IS_NIL(args)) {
                sc->out = CAR(args);
                if (IS_NIL(sc->out)) {
                        sc->out = the_Stdout;
                }
        }
        obp_t s = princ_string(expr);
        port_write(sc->out, THE_STRINGS(s));
        return s;
}

/**
 * Evaluate the argument forms. If no error occurs, return a list with the value
 * of the last form as the only element. If an error occurs, return a string
 * with the error message.
 * (errset form1 ...)
 */
obp_t bf_errset(int nargs, obp_t args, session_context_t *sc, int level)
{
        PROTECT;
        PROTVAR(retval);
        PROTVAR(errport);
        
        while (!IS_NIL(args)) {
                retval = eval(CAR(args), sc, level);
                if (IS_EXIT(retval)) {
                        errport = make_string_port("errstr");
                        print_error(retval, errport);
                        retval = get_port_string(errport);
                        close_port(errport);
                        goto EXIT;
                }
                args = CDR(args);
        }
        retval = cons(retval, the_Nil);
    EXIT:
        UNPROTECT;
        return retval;
}

/**
 * Define function for symbol that is autoloaded from a file when it is applied. 
 * If the third argument is present and non-nil, the function is defined as a
 * special form.
 * (autoload symbol filename [special])
 */
obp_t bf_autoload(int nargs, obp_t args, session_context_t *sc, int level)
{
        PROTECT;
        PROTVAR(retval);
        obp_t sym = CAR(args);
        args = CDR(args);
        obp_t filename = CAR(args);
        args = CDR(args);
        obp_t is_special = args == the_Nil ? the_Nil : CAR(args);
        CHECKTYPE(sc->out, sym, SYMBOL);
        CHECKTYPE(sc->out, filename, STRING);
        
        PROTVAL(func, new_autoload_function(THE_STRINGS(AS(sym, SYMBOL)->name),
                                          filename, is_special != the_Nil));
        AS(sym, SYMBOL)->function = func;
        retval = func;
    EXIT:
        UNPROTECT;
        return retval;
}

/**
 * Describe an object. The description is printed to stdout or, if the second
 * argument is present and non-nil, returned as a string.
 * (describe arg [tostring])
 */
obp_t bf_describe(int nargs, obp_t args, session_context_t *sc, int level)
{
        PROTECT;
        obp_t ob = CAR(args);
        strbuf_t sb = describe(ob, 0);
        PROTVAL(retval, the_T);
        
        if (CDR(args) != the_Nil && CADR(args) != the_Nil) {
                retval = new_string(strbuf_string(sb), strbuf_size(sb));
        } else {
                port_write(the_Stdout, strbuf_string(sb), strbuf_size(sb));
        }
        UNPROTECT;
        free(sb);
        return retval;
}

/**
 * Invoke the garbage collector.
 * (gc)
 */
obp_t bf_gc(int nargs, obp_t args, session_context_t *sc, int level)
{
        gc();
        return the_Nil;
}

/**
 * Get or set the trace bit on a function. With one argument, return t if the
 * trace flag is set, nil otherwise. If the second argument is present, set the
 * trace flag accordingly. When the trace bit is set on a function, the function
 * and its arguments are printed on every call.
 * (trace-function func [value])
 */
obp_t bf_trace_function(int nargs, obp_t args, session_context_t *sc, int level)
{
        obp_t arg1 = CAR(args);
        Lfunction_t *func = AS(arg1, FUNCTION);

        if (nargs == 2) {
                func->trace = CADR(args) == the_Nil ? 0 : 1;
        }
        return func->trace ? the_T : the_Nil;
}

/**
 * Return a list of pairs representing all freelist buckets. The car of the pair
 * is the size of the objects in that bucket, the cdr the number of available
 * objects in it.
 * (show-freelist)
 */
obp_t bf_show_freelist(int nargs, obp_t args, session_context_t *sc, int level)
{
        PROTECT;
        PROTVAR(retval);
        PROTVAR(pair);
        PROTVAR(size);
        PROTVAR(count);

        for (int i = FREELIST_ENTRIES - 1; i >= 0; i--) {
                size = new_integer(i << 3);
                count = new_integer(ob_sizecount[i]);
                pair = cons(size, count);
                retval = cons(pair, retval);
        }
        UNPROTECT;
        return retval;
}

/**
 * Common part of all function definitions.
 */
Lfunction_t *new_function_common(char *name, uint namelen, int is_special,
                                 short minargs, short maxargs)
{
        Lfunction_t *ob = NEW_OBJ(FUNCTION);
        ob->name = name;
        ob->namelen = namelen;
        ob->is_special = !!is_special;
        ob->minargs = minargs;
        ob->maxargs = maxargs;

        return ob;
}


/**
 * Define an autoload function object.
 */
obp_t new_autoload_function(char *name, uint namelen, obp_t filename,
                            int is_special)
{
        Lfunction_t *ob = new_function_common(name, namelen, is_special, 0, -1);
        ob->type = F_AUTOLOAD;
        ob->impl.filename = filename;
        return (obp_t) ob;
}

/**
 * Create a function from a form.
 */
obp_t new_form_function(char *name, uint namelen, obp_t form, int is_special,
                        short minargs, short maxargs)
{
        Lfunction_t *ob = new_function_common(name, namelen, is_special,
                                              minargs, maxargs);
        ob->impl.form = form;
        ob->type = F_FORM;
        return (obp_t) ob;
}

/**
 * Define a new builtin function.
 */
obp_t new_builtin_function(char *name, uint namelen, builtin_func_t builtin,
                           int is_special, short minargs, short maxargs)
{
        Lfunction_t *ob = new_function_common(name, namelen, is_special,
                                              minargs, maxargs);
        ob->impl.builtin = builtin;
        ob->type = F_BUILTIN;
        return (obp_t) ob;
}

/**
 * Define a new builtin function and register it under a name.
 */
obp_t register_builtin(char *name, builtin_func_t builtin,
                       int is_special, short minargs, short maxargs)
{
        PROTECT;
        PROTVAL(bi, new_builtin_function(name, strlen(name), builtin,
                                         is_special, minargs, maxargs));
        Lsymbol_t *sym = AS(intern_z(name), SYMBOL);
        //describe((obp_t) sym);
        sym->function = bi;
        //describe((obp_t) sym);
        UNPROTECT;
        return bi;
}

/**
 * Initialize all builtin functions at program startup.
 */
void init_builtins(void)
{
        register_builtin(SETQ_NAME, bf_setq, 1, 0, -1);
        register_builtin(LAMBDA_NAME, bf_lambda, 1, 1, -1);
        register_builtin(FSET_NAME, bf_fset, 0, 2, 2);
        register_builtin(IF_NAME, bf_if, 1, 0, -1);
        register_builtin(CAR_NAME, bf_car, 0, 1, 1);
        register_builtin(CDR_NAME, bf_cdr, 0, 1, 1);
        register_builtin(CONS_NAME, bf_cons, 0, 2, 2);
        register_builtin(EVAL_NAME, bf_eval, 0, 1, 1);
        register_builtin(QUOTE_NAME, bf_quote, 1, 1, 1);
        register_builtin(LOAD_NAME, bf_load, 0, 0, -1);
        register_builtin(SYMBOL_FUNCTION_NAME, bf_symbol_function, 0, 1, 1);
        register_builtin(SYMBOL_NAME_NAME, bf_symbol_name, 0, 1, 1);
        register_builtin(SYMBOLS_NAME, bf_symbols, 0, 0, 0);
        register_builtin(FSET_NAME, bf_fset, 0, 2, 2);
        register_builtin(DEFUN_NAME, bf_defun, 1, 2, -1);
        register_builtin(EQ_NAME, bf_eq, 0, 2, 2);
        register_builtin(NULL_NAME, bf_null, 0, 1, 1);
        register_builtin(TRACE_NAME, bf_trace, 0, 0, 1);
        register_builtin(TTY_NAME, bf_tty, 0, 0, 1);
        register_builtin(ATOM_NAME, bf_atom, 0, 1, 1);
        register_builtin(FUNCTION_NAME, bf_function, 1, 1, 1);
        register_builtin(COND_NAME, bf_cond, 1, 0, -1);
        register_builtin(TIME_NAME, bf_time, 1, 0, -1);
        register_builtin(FUNCALL_NAME, bf_funcall, 0, 1, -1);
        register_builtin(APPLY_NAME, bf_apply, 0, 2, 2);
        register_builtin(LIST_NAME, bf_list, 0, 0, -1);
        register_builtin(MEASURE_NAME, bf_measure, 1, 0, -1);
        register_builtin(LET_NAME, bf_let, 1, 1, -1);
        register_builtin(LETREC_NAME, bf_letrec, 1, 1, -1);
        register_builtin(AND_NAME, bf_and, 1, 0, -1);
        register_builtin(OR_NAME, bf_or, 1, 0, -1);
        register_builtin(NOT_NAME, bf_null, 0, 1, 1);
        register_builtin(PROGN_NAME, bf_progn, 1, 0, -1);
        register_builtin(WHILE_NAME, bf_while, 1, 1, -1);
        register_builtin(UNWIND_PROTECT_NAME, bf_unwind_protect, 1, 1, -1);
        register_builtin(ERRSET_NAME, bf_errset, 1, 0, -1);
        register_builtin(EQL_NAME, bf_eql, 0, 2, 2);
        register_builtin(PROG1_NAME, bf_prog1, 1, 1, -1);
        register_builtin(PROG2_NAME, bf_prog2, 1, 2, -1);
        register_builtin(PRIN1S_NAME, bf_prin1s, 0, 1, 1);
        register_builtin(PRIN1_NAME, bf_prin1, 0, 1, 2);
        register_builtin(PRINC_NAME, bf_princ, 0, 1, 2);
        register_builtin(AUTOLOAD_NAME, bf_autoload, 0, 2, 3);
        register_builtin(PRINCS_NAME, bf_princs, 0, 1, 2);
        register_builtin(DESCRIBE_NAME, bf_describe, 0, 1, 2);
        register_builtin(LENGTH_NAME, bf_length, 0, 1, 1);
        register_builtin(APROPOS_NAME, bf_apropos, 0, 1, 1);
        register_builtin(GC_NAME, bf_gc, 0, 0, 0);
        register_builtin(TRACE_FUNCTION_NAME, bf_trace_function, 0, 1, 2);
        register_builtin(SHOW_FREELIST_NAME, bf_show_freelist, 0, 0, 0);
        
        gettimeofday(&start_time, 0);
}

/* EOF */
