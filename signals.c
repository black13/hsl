/* Copyright (c) 2010, 2011 Juergen Nickelsen <ni@jnickelsen.de>
 * See the file COPYRIGHT for details.
 */

#include <stdarg.h>
#include <string.h>
#include "signals.h"
#include "io.h"
#include "printer.h"
#include "names.h"


obp_t new_signal(short type, short code, obp_t data, obp_t message)
{
        Lsignal_t *ob = NEW_OBJ(SIGNAL);
        ob->data = data;
        ob->message = message;
        ob->type = type;
        ob->code = code;
        return (obp_t) ob;
}


obp_t throw_error(obp_t out_port, short code, obp_t ob, char *format, ...)
{
        va_list arglist;

        va_start(arglist, format);
        obp_t port = port_vprintf(make_string_port("error"), format, arglist);
        obp_t errstr = get_port_string(port);
        obp_t error = new_signal(SIG_LERROR, code, ob, errstr);
        va_end(arglist);
        AS(intern_z(LAST_ERROR_NAME), SYMBOL)->value =
                new_signal(SIG_UERROR, code, ob, errstr);
        print_error(error, out_port);
        return error;
}


char *signal_type(short type)
{
#define SIGNAME_MAX 10
        static char buffer[SIGNAME_MAX];
        switch (type) {
            case SIG_LERROR:   return "ERROR";
            case SIG_MESSAGE:  return "MESSAGE";
            default:
                snprintf(buffer, SIGNAME_MAX, "SIG%d", type);
                return buffer;
        }
}

char *error_string(int errtype)
{
        switch (errtype) {
            case ERR_RSYNTAX:  return "reader syntax error";
            case ERR_READEOF:  return "preliminary EOF in reader";
            case ERR_EVAL:     return "eval error";
            case ERR_NOFUNC:   return "not a function";
            case ERR_NOARGS:   return "invalid argument count";
            case ERR_NOLIST:   return "list operation on non-list";
            case ERR_NOSYMBOL: return "not a symbol";
            case ERR_INVARG:   return "invalid argument";
            case ERR_SYSTEM:   return "system call error";
            case ERR_CLPORT:   return "port is not open";
            case ERR_INTERN:   return "internal error";
            case ERR_MEMORY:   return "unexpected memory shortage";
            case ERR_IO:       return "I/O error";
            case ERR_LETARGS:  return "invalid let arguments list";
            case ERR_IMMUTBL:  return "object is immutable";
            case ERR_NOAUTOL:  return "autoload failed to define function";
            default: return "unknown error??";
        }
}

void print_error(obp_t error, obp_t out_port)
{
        Lsignal_t *err = AS(error, SIGNAL);

        port_print(out_port, "Error: ");
        port_print(out_port, error_string(err->code));
        port_print(out_port, "; ");
        port_print(out_port, AS(err->message, STRING)->content);
        if (err->data) {
                port_print(out_port, ": ");
                print_expr(err->data, out_port);
        }
        terpri(out_port);
}


/* EOF */
