/* Copyright (c) 2010, 2011 Juergen Nickelsen <ni@jnickelsen.de>
 * See the file COPYRIGHT for details.
 */

#include "cbasics.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include "signals.h"
#include "names.h"
#include "io.h"
#include "xmemory.h"
#include "reader.h"
#include "eval.h"
#include "session.h"
#include "gc.h"

obp_t the_Stdin;
obp_t the_Stdout;
obp_t the_Stderr;

char portname_buf[100];
int counter;

char *port_type_name(port_type_t type)
{
        switch (type) {
            case STRING_PORT: return "string";
            case STREAM_PORT: return "stream";
            case FD_PORT: return "fd";
            default: return "invalid";
        }
}

/**
 * returns a port
 */
obp_t make_stream_port(char *fname, char *fmode)
{
        FILE *stream = fopen(fname, fmode);
        PROTVAR(retval);
        if (stream == 0) {
                ERROR(the_Stderr, ERR_SYSTEM, 0, "error opening %s: %s",
                      fname, strerror(errno));
        }
        int in = fmode[0] == 'r' || fmode[1] == '+';
        int out = strchr("wa", fmode[0]) || fmode[1] == '+';
        retval = new_port(fname, stream, -1, 0, STREAM_PORT, in, out);
    EXIT:
        return retval;
}

obp_t make_string_port(char *name)
{
        strbuf_t sb = strbuf_new();
        if (!name) {
                sprintf(portname_buf, "string-port%d", counter++);
                name = portname_buf;
        }
        return new_port(name, 0, -1, sb, STRING_PORT, 1, 1);
}


obp_t port_tty(obp_t port)
{
        Lport_t *p = AS(port, PORT);
        int fd = -1;
        if (p->type == STREAM_PORT) {
                fd = fileno(p->port.stream);
        } else if (p->type == FD_PORT) {
                fd = p->port.fd;
        }
        if (fd >= 0) {
                char *tty = ttyname(fd);
                return tty ? new_zstring(tty) : the_T;
        }
        return the_Nil;
}

obp_t port_flush(obp_t port)
{
        Lport_t *p = AS(port, PORT);

        if (p->type == STREAM_PORT) {
                fflush(p->port.stream);
                return the_T;
        }
        return the_Nil;
}

obp_t port_print(obp_t port, char *s)
{
        return port_write(port, s, strlen(s));
}


obp_t port_putc(obp_t port, char c)
{
        return port_write(port, &c, 1);
}


obp_t port_write(obp_t port, char *s, uint len)
{
        PROTECT;
        PROTVAR(retval);
        Lport_t *p = AS(port, PORT);

        if (p->closed) {
                ERROR(the_Stderr, ERR_CLPORT, port, "port is closed");
        }
        if (!p->out) {
                ERROR(the_Stderr, ERR_CLPORT, port, "port is not output");
        }
        switch (p->type) {
            case STREAM_PORT:
                if (fwrite(s, 1, len, p->port.stream) < len) {
                        ERROR(the_Stderr, ERR_IO, port,
                              "fwrite to port failed: %s", strerror(errno));
                }
                break;
            case FD_PORT:
                if (write(p->port.fd, s, len) < 0) {
                        ERROR(the_Stderr, ERR_IO, port, "write to port failed: %s",
                              strerror(errno));
                }
                break;
            case STRING_PORT:
                p->port.strbuf = strbuf_nappend(p->port.strbuf, s, len);
                break;
            default:
                ERROR(the_Stderr, ERR_INVARG, port,
                      "invalid type %d of port", p->type);
        }
        retval = port;
    EXIT:
        UNPROTECT;
        return retval;
}

obp_t port_vprintf(obp_t port, char *format, va_list arglist)
{
        char *string ;
        PROTVAR(retval);

        int vasprintf_ret = vasprintf(&string, format, arglist);
        if (vasprintf_ret < 0) {
                ERROR(the_Stderr, ERR_MEMORY, 0, "vasprintf() failed");
        }

        retval = port_print(port, string);
        free(string);
    EXIT:
        return retval;
}

obp_t port_printf(obp_t port, char *format, ...)
{
        va_list arglist;
        obp_t result;

        va_start(arglist, format);
        result = port_vprintf(port, format, arglist);
        va_end(arglist);
        return result;
}


obp_t get_port_string(obp_t port)
{
        Lport_t *p = AS(port, PORT);
        if (p->type == STRING_PORT) {
                return new_string(strbuf_string(p->port.strbuf),
                                  strbuf_size(p->port.strbuf));
        } else {
                return throw_error(the_Stderr, ERR_INVARG, port,
                                   "port is not a string buffer");
        }
}

obp_t port_ungetc(obp_t port, int c)
{
        AS(port, PORT)->ungotten = c;
        return the_Nil;
}

obp_t port_getc(obp_t port)
{
        PROTECT;
        PROTVAR(retval);
        
        Lport_t *p = AS(port, PORT);
        if (p->ungotten >= 0) {
                retval = new_char(p->ungotten);
                p->ungotten = -1;
                goto EXIT;
        }

        obp_t string = port_read(port, 1);
        if (!string || IS_EXIT(string)) {
                retval = string;
                goto EXIT;
        }
        Lstring_t *s = AS(string, STRING);
        retval = new_char(s->length ? s->content[0] : EOF);
    EXIT:
        UNPROTECT;
        return retval;
}

obp_t port_read(obp_t port, uint len)
{
        Lport_t *p = AS(port, PORT);
        char *read_buf = 0;
        int read_ret;
        PROTVAR(retval);

        if (p->closed) {
                ERROR(the_Stderr, ERR_CLPORT, port, "port is closed");
        }
        if (!p->in) {
                ERROR(the_Stderr, ERR_CLPORT, port, "port is not input");
        }
        switch (p->type) {
            case STREAM_PORT:
                read_buf = xmalloc(len, "read buffer for stream port");
                read_ret = fread(read_buf, len, 1, p->port.stream);
                if (read_ret < 0) {
                        ERROR(the_Stderr, ERR_IO, port,
                              "fread from port failed: %s", strerror(errno));
                }
                break;
            case FD_PORT:
                read_buf = xmalloc(len, "read buffer for fd port");
                read_ret = read(p->port.fd, read_buf, len);
                if (read_ret < 0) {
                        ERROR(the_Stderr, ERR_IO, port,
                              "read from port failed: %s", strerror(errno));
                }
                break;
            case STRING_PORT:
                read_ret = strbuf_readn(p->port.strbuf, len, &read_buf);
                break;
            default:
                ERROR(the_Stderr, ERR_INVARG, port,
                      "invalid type %d of port", p->type);
        }
        retval = new_string(read_buf, read_ret);
    EXIT:
        free(read_buf);
        return retval;
}

obp_t close_port(obp_t port)
{
        PROTVAR(retval);
        if (!IS(port, PORT)) {
                ERROR(the_Stderr, ERR_INVARG, port, "port argument needed");
        }
        Lport_t *p = AS(port, PORT);
        if (p->closed) {
                ERROR(the_Stderr, ERR_CLPORT, port, "port is already closed");
        }
        p->closed = 1;
        switch (p->type) {
            case STREAM_PORT:
                if (fclose(p->port.stream)) {
                        ERROR(the_Stderr, ERR_SYSTEM, port,
                              "error closing port: %s", strerror(errno));
                }
                break;
            case FD_PORT:
                if (close(p->port.fd)) {
                        ERROR(the_Stderr, ERR_SYSTEM, port,
                              "error closing port: %s", strerror(errno));
                }
                break;
            case STRING_PORT:
                free(p->port.strbuf);
                p->port.strbuf = 0;
                break;
            default:
                ERROR(the_Stderr, ERR_INVARG, port,
                      "invalid type %d of close port", p->type);
        }
        retval = port;
    EXIT:
        return retval;
}

obp_t load_file(char *fname, session_context_t *sc, int level)
{
        PROTECT;
        PROTVAR(retval);
        PROTVAL(new_in, make_stream_port(fname, "r"));
        CHECK_ERROR(new_in);
        PROTVAL(saved_port, sc->in);
        int saved_int = sc->is_interactive;

        sc->in = new_in;
        sc->is_interactive = 0;
        retval = repl(sc, level);
        close_port(new_in);
        sc->is_interactive = saved_int;
        sc->in = saved_port;
    EXIT:
        UNPROTECT;
        return retval;
}

void init_io(void)
{
        PROTECT;
        PROTVAR(name);
        protect(the_Stdin);
        protect(the_Stdout);
        protect(the_Stderr);

        the_Stdin = new_port("*stdin*", stdin, -1, 0, STREAM_PORT, 1, 0);
        AS(intern_z(STDIN_PORT_NAME), SYMBOL)->value = the_Stdin;

        the_Stdout = new_port("*stdout*", stdout, -1, 0, STREAM_PORT, 0, 1);
        AS(intern_z(STDOUT_PORT_NAME), SYMBOL)->value = the_Stdout;

        the_Stderr = new_port("*stderr*", stderr, -1, 0, STREAM_PORT, 0, 1);
        AS(intern_z(STDERR_PORT_NAME), SYMBOL)->value = the_Stderr;
        
        UNPROTECT;
}

/* EOF */
