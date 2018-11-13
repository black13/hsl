/* Copyright (c) 2010, 2011 Juergen Nickelsen <ni@jnickelsen.de>
 * See the file COPYRIGHT for details.
 */

#include <string.h>
#include <stdlib.h>
#include "printer.h"
#include "signals.h"
#include "names.h"
#include "xmemory.h"
#include "io.h"
#include "functions.h"
#include "math.h"


typedef void (*printer_t)(obp_t ob, obp_t port);
typedef strbuf_t (*stringer_t)(obp_t ob, strbuf_t sb, int flags);

void p_symbol(obp_t ob, obp_t port);
void p_pair(obp_t ob, obp_t port);
void p_number(obp_t ob, obp_t port);
void p_string(obp_t ob, obp_t port);
void p_char(obp_t ob, obp_t port);
void p_port(obp_t ob, obp_t port);
void p_vector(obp_t ob, obp_t port);
void p_map(obp_t ob, obp_t port);
void p_inetaddr(obp_t ob, obp_t port);
void p_strbuf(obp_t ob, obp_t port);
void p_signal(obp_t ob, obp_t port);
void p_function(obp_t ob, obp_t port);
void p_environ(obp_t ob, obp_t port);

strbuf_t s_symbol(obp_t ob, strbuf_t sb, int flags);
strbuf_t s_pair(obp_t ob, strbuf_t sb, int flags);
strbuf_t s_number(obp_t ob, strbuf_t sb, int flags);
strbuf_t s_string(obp_t ob, strbuf_t sb, int flags);
strbuf_t s_char(obp_t ob, strbuf_t sb, int flags);
strbuf_t s_port(obp_t ob, strbuf_t sb, int flags);
strbuf_t s_vector(obp_t ob, strbuf_t sb, int flags);
strbuf_t s_map(obp_t ob, strbuf_t sb, int flags);
strbuf_t s_inetaddr(obp_t ob, strbuf_t sb, int flags);
strbuf_t s_strbuf(obp_t ob, strbuf_t sb, int flags);
strbuf_t s_signal(obp_t ob, strbuf_t sb, int flags);
strbuf_t s_function(obp_t ob, strbuf_t sb, int flags);
strbuf_t s_environ(obp_t ob, strbuf_t sb, int flags);
strbuf_t s_gcprot(obp_t ob, strbuf_t sb, int flags);

char tmp_buf[128];                      /* temporary print buffer, will be
                                         * overwritten by something else
                                         * immediately, log enough for smallish
                                         * things
                                         */

stringer_t stringers[] = {
        0,                              /* INVALiD */
        s_symbol,                       /* SYMBOL */
        s_pair,                         /* PAIR */
        s_number,                       /* NUMBER */
        s_string,                       /* STRING */
        s_char,                         /* CHAR */
        s_port,                         /* PORT */
        s_vector,                       /* VECTOR */
        s_map,                          /* MAP */
        s_inetaddr,                     /* INETADDR */
        s_signal,                       /* SIGNAL */
        s_strbuf,                       /* STRBUF */
        s_function,                     /* FUNCTION */
        s_environ,                      /* ENVIRON */
        s_gcprot,                       /* GCPROT */
        0                               /* SENTiNEL */
};

char *q_chars[] = {
        "\\000",
        "\\001",
        "\\002",
        "\\003",
        "\\004",
        "\\005",
        "\\006",
        "\\a",
        "\\b",
        "\\t",
        "\\n",
        "\\v",
        "\\f",
        "\\r",
        "\\016",
        "\\017",
        "\\020",
        "\\021",
        "\\022",
        "\\023",
        "\\024",
        "\\025",
        "\\026",
        "\\027",
        "\\030",
        "\\031",
        "\\032",
        "\\033",
        "\\034",
        "\\035",
        "\\036",
        "\\037"
};


strbuf_t quote_c(strbuf_t sb, char c)
{
        if (c <= 037) {
                sb = strbuf_append(sb, q_chars[(int) c]);
        } else if (c == 0177) {
                sb = strbuf_append(sb, "\\0177");
        } else {
                sb = strbuf_addc(sb, c);
        }
        return sb;
}


strbuf_t quote_nstring(strbuf_t sb, char *s, int len)
{
        for (int i = 0; i < len; i++) {
                sb = quote_c(sb, s[i]);
        }
        return sb;
}

strbuf_t s_symbol(obp_t ob, strbuf_t sb, int flags)
{
        obp_t name = AS(ob, SYMBOL)->name;
        return strbuf_nappend(sb, THE_STRINGS(name));
}

strbuf_t s_pair(obp_t ob, strbuf_t sb, int flags)
{
        sb = strbuf_addc(sb, '(');
        sb = s_expr(CAR(ob), sb, flags);
        obp_t body = CDR(ob);
        while (IS(body, PAIR)) {
                sb = strbuf_addc(sb, ' ');
                sb = s_expr(CAR(body), sb, flags);
                body = CDR(body);
        }
        if (!IS_NIL(body)) {
                sb = strbuf_append(sb, " . ");
                sb = s_expr(body, sb, flags);
        }
        return strbuf_addc(sb, ')');
}

strbuf_t s_number(obp_t ob, strbuf_t sb, int flags)
{
        if (IS_INT(ob)) {
                sprintf(tmp_buf, "%ld", lrint(AS(ob, NUMBER)->value));
        } else {
                sprintf(tmp_buf, "%Lg", AS(ob, NUMBER)->value);
        }
        return strbuf_append(sb, tmp_buf);
}

strbuf_t s_string(obp_t ob, strbuf_t sb, int flags)
{
        Lstring_t *str = AS(ob, STRING);
        if (flags & TOSTRING_READ) {
                sb = strbuf_addc(sb, '\"');
                sb = quote_nstring(sb, str->content, str->length);
                sb = strbuf_addc(sb, '\"');
        } else {
                sb = strbuf_nappend(sb, str->content, str->length);
        }
        return sb;
}

strbuf_t s_char(obp_t ob, strbuf_t sb, int flags)
{
        char c = (char) AS(ob, CHAR)->value;
        if (flags & TOSTRING_READ) {
                return quote_c(sb, c);
        } else {
                return strbuf_addc(sb, c);
        }
}

strbuf_t s_port(obp_t ob, strbuf_t sb, int flags)
{
        Lport_t *p = AS(ob, PORT);
        sb = strbuf_append(sb, "#<port:");
        sb = strbuf_append(sb, p->name);
        sprintf(tmp_buf, ":%s,%s%s>", port_type_name(p->type),
                (p->in ? "i" : ""), (p->out ? "o" : ""));
        return strbuf_append(sb, tmp_buf);
}

strbuf_t s_vector(obp_t ob, strbuf_t sb, int flags)
{
        Lvector_t *ob_vector = AS(ob, VECTOR);
        sb = strbuf_addc(sb, '[');
        if (flags & TOSTRING_BRIEF) {
                sprintf(tmp_buf, "%d", ob_vector->nelem);
                sb = strbuf_append(sb, tmp_buf);
        } else {
                for (int i = 0; i < ob_vector->nelem; i++) {
                        sb = s_expr(ob_vector->elem[i], sb, flags);
                        if (i < ob_vector->nelem - 1) {
                                sb = strbuf_addc(sb, ' ');
                        }
                }
        }
        return strbuf_addc(sb, ']');
}

strbuf_t s_map(obp_t ob, strbuf_t sb, int flags)
{
        Lmap_t *ob_map = AS(ob, MAP);

        sb = strbuf_addc(sb, '{');
        if (flags & TOSTRING_BRIEF) {
                sprintf(tmp_buf, "%d", hashmap_size(ob_map->map));
                sb = strbuf_append(sb, tmp_buf);
        } else {
                for (obp_t values = hashmap_pairs(ob_map->map);
                     values != the_Nil;
                     values = CDR(values))
                {
                        sb = s_pair(CAR(values), sb, flags);
                        if (CDR(values) != the_Nil) {
                                sb = strbuf_addc(sb, ' ');
                        }
                }
        }
        return strbuf_addc(sb, '}');
}

strbuf_t s_inetaddr(obp_t ob, strbuf_t sb, int flags)
{
        Lnetaddr_t *ob_netaddr = AS(ob, NETADDR);
        sprintf(tmp_buf, "#<netaddr:%p>", ob_netaddr);

        return strbuf_append(sb, tmp_buf);
}

strbuf_t s_signal(obp_t ob, strbuf_t sb, int flags)
{
        Lsignal_t *sig = AS(ob, SIGNAL);

        if (IS_EXIT(ob)) {
                sprintf(tmp_buf, "#<sig-%s:%s,",
                        signal_type(sig->type), error_string(sig->code));
                sb = strbuf_append(sb, tmp_buf);
                sb = strbuf_nappend(sb, THE_STRINGS(sig->message));
                if (!(flags & TOSTRING_BRIEF) && sig->data) {
                        sb = strbuf_addc(sb, ':');
                        sb = s_expr(sig->data, sb, flags);
                }
        } else {
                sprintf(tmp_buf, "#<sig-%s,%d:",
                        signal_type(sig->type), sig->code);
                sb = strbuf_append(sb, tmp_buf);
                sb = strbuf_nappend(sb, THE_STRINGS(sig->message));
        }
        return strbuf_addc(sb, '>');
}

strbuf_t s_strbuf(obp_t ob, strbuf_t sb, int flags)
{
        strbuf_t obsb = AS(ob, STRBUF)->strbuf;
        if (flags & TOSTRING_READ) {
                sb = strbuf_addc(sb, '\"');
                sb = quote_nstring(sb, strbuf_string(obsb), strbuf_size(obsb));
                sb = strbuf_addc(sb, '\"');
        } else {
                sb = strbuf_nappend(sb, strbuf_string(obsb), strbuf_size(obsb));
        }
        return sb;
}

char *functype_name[] = {
        "builtin",
        "form",
        "autoload",
        "bytecode",
        0
};

strbuf_t s_function(obp_t ob, strbuf_t sb, int flags)
{
        Lfunction_t *func = AS(ob, FUNCTION);
        sb = strbuf_append(sb, "#<");
        sb = strbuf_append(sb, functype_name[func->type]);
        if (func->is_special) {
                sb = strbuf_append(sb, "_s");
        }
        if (func->name) {
                sb = strbuf_addc(sb, ':');
                sb = strbuf_nappend(sb, func->name, func->namelen);
        }
        sprintf(tmp_buf, ":%d..", func->minargs);
        sb = strbuf_append(sb, tmp_buf);
        if (func->maxargs >= 0) {
                sprintf(tmp_buf, "%d", func->maxargs);
                sb = strbuf_append(sb, tmp_buf);
        }
        return strbuf_addc(sb, '>');
}


strbuf_t s_environ(obp_t ob, strbuf_t sb, int flags)
{
        Lenviron_t *ob_environ = AS(ob, ENVIRON);
        sprintf(tmp_buf, "#<environ:%p>", ob_environ);
        return strbuf_append(sb, tmp_buf);
}

strbuf_t s_gcprot(obp_t ob, strbuf_t sb, int flags)
{
        gcp_t ob_gcprot = AS(ob, GCPROT);
        sprintf(tmp_buf, "#<gcprot:%p>", ob_gcprot);
        return strbuf_append(sb, tmp_buf);
}


strbuf_t s_expr(obp_t ob, strbuf_t sb, int flags)
{
        if (ob) {
                return stringers[ob->type](ob, sb, flags);
        } else {
                return strbuf_append(sb, "#<undef>");
        }
}


obp_t to_string(obp_t ob, int flags)
{
        strbuf_t sb = strbuf_new();
        if (flags & TOSTRING_PRINT) {
                sb = strbuf_addc(sb, '\n');
        }
        sb = s_expr(ob, sb, flags);
        if (flags & TOSTRING_PRINT) {
                sb = strbuf_addc(sb, ' ');
        }
        obp_t s = new_string(strbuf_string(sb), strbuf_size(sb));
        free(sb);
        return s;
}

obp_t princ_string(obp_t ob)
{
        if (IS(ob, STRING)) {
                return ob;
        }
        if (IS(ob, SYMBOL)) {
                return AS(ob, SYMBOL)->name;
        }
        return to_string(ob, 0);
}

obp_t prin1_string(obp_t ob)
{
        return to_string(ob, TOSTRING_READ);
}

obp_t print_string(obp_t ob)
{
        return to_string(ob, TOSTRING_READ | TOSTRING_PRINT);
}


void print(obp_t ob)
{
        strbuf_t sb = strbuf_new();
        sb = s_expr(ob, sb, TOSTRING_BRIEF);
        printf("%.*s\n", strbuf_size(sb), strbuf_string(sb));
}


char *blanks(int n)
{
        static char *b;
        static uint len;

        if (n > len || len == 0) {
                b = xrealloc(b, n + 1, "resize blanks buffer");
                memset(b + len, ' ', n - len);
                b[n] = 0;
                len = n;
        }
        return b + len - n;
}


strbuf_t describe_ob(obp_t ob, strbuf_t sb)
{
        if (ob == 0) {
                return strbuf_append(sb, UNBOUND_VALUE_NAME "\n");
        }
        sprintf(tmp_buf, "ob %p type %d %s mark %d eq %d r/o %d size %d int %d: ",
               ob, ob->type, type_name(ob->type), ob->mark,
                ob->eq_is_eqv, ob->immutable, ob->size, ob->num_is_int);
        sb = strbuf_append(sb, tmp_buf);
        sb = s_expr(ob, sb, TOSTRING_READ);
        sb = strbuf_addc(sb, '\n');
        if (IS(ob, SYMBOL)) {
                sb = strbuf_append(sb, "  name: ");
                sb = s_expr(AS(ob, SYMBOL)->name, sb,
                            TOSTRING_READ | TOSTRING_PRINT);
                sb = strbuf_append(sb, "\n  value: ");
                sb = s_expr(AS(ob, SYMBOL)->value, sb,
                            TOSTRING_READ | TOSTRING_PRINT);
                sb = strbuf_append(sb, "\n  function: ");
                sb = describe_ob(AS(ob, SYMBOL)->function, sb);
                sb = strbuf_append(sb, "  props: ");
                sb = s_expr(AS(ob, SYMBOL)->props, sb,
                            TOSTRING_READ | TOSTRING_PRINT);
                sb = strbuf_addc(sb, '\n');
        }
        xdump(stdout, ob, ob->size);
        return sb;
}

strbuf_t describe(obp_t ob, int do_print)
{
        strbuf_t sb = strbuf_new();
        sb = describe_ob(ob, sb);
        if (do_print) {
                fwrite(strbuf_string(sb), strbuf_size(sb), 1, stdout);
        }
        return sb;
}


void desc(obp_t ob)
{
        describe(ob, 1);
}


void print_expr(obp_t ob, obp_t port)
{
        strbuf_t sb = strbuf_new();
        sb = s_expr(ob, sb, TOSTRING_READ);

        if (port == 0 || port == the_Nil) {
                port = the_Stdout;
        }
        port_write(port, strbuf_string(sb), strbuf_size(sb));
        free(sb);
}

void terpri(obp_t port)
{
        if (port == 0 || port == the_Nil) {
                port = the_Stdout;
        }
        port_putc(port, '\n');
}

/* EOF */
