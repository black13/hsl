/* Copyright (c) 2010, 2011 Juergen Nickelsen <ni@jnickelsen.de>
 * See the file COPYRIGHT for details.
 */
/*
 * object support code, allocation, deallocation, traversal
 */

#include <stdlib.h>
#include <string.h>
#include "objects.h"
#include "xmemory.h"
#include "hashmap.h"
#include "names.h"
#include "io.h"
#include "signals.h"
#include "printer.h"





obp_t symbols;                          /* object containing map of all symbols */
hashmap_t symbols_map;                  /* map Lstring -> Lsymbol */

obp_t the_Nil;                          /* the nil symbol -- we better not look
                                           this up every time we need it */
obp_t the_T;                            /* the t symbol */

obp_t the_Lambda;                       /* the lambda symbol */
obp_t the_Mu;                           /* the mu symbol */

int traceflag;


/**
 * Return the list of all symbols.
 */
obp_t all_symbols(void)
{
        return hashmap_values(symbols_map);
}


typedef struct OBJ_OPS {
        void (*op_traverse)(obp_t, void (*do_func)(obp_t),
                            int (*stop_func)(obp_t));
        void (*op_free)(obp_t ob);
} obj_ops_t ;


/* object operations per type
 */

void free_map(obp_t ob);
void free_strbuf(obp_t ob);
void free_port(obp_t ob);

void traverse_nop(obp_t ob, void (*do_func)(obp_t), int (*stop_func)(obp_t));
void traverse_symbol(obp_t ob, void (*do_func)(obp_t), int (*stop_func)(obp_t));
void traverse_map(obp_t ob, void (*do_func)(obp_t), int (*stop_func)(obp_t));
void traverse_pair(obp_t ob, void (*do_func)(obp_t), int (*stop_func)(obp_t));
void traverse_vector(obp_t ob, void (*do_func)(obp_t), int (*stop_func)(obp_t));
void traverse_signal(obp_t ob, void (*do_func)(obp_t), int (*stop_func)(obp_t));
void traverse_func(obp_t ob, void (*do_func)(obp_t), int (*stop_func)(obp_t));
void traverse_gcprot(obp_t ob, void (*do_func)(obp_t), int (*stop_func)(obp_t));

obj_ops_t oops[] = {
        /* INVALiD */
        {
                0,
                0
        },
        /* SYMBOL */
        {
                traverse_symbol,
                ob_free
        },
        /* PAIR */
        {
                traverse_pair,
                ob_free
        },
        /* NUMBER */
        {
                traverse_nop,
                ob_free
        },
        /* STRING */
        {
                traverse_nop,
                ob_free
        },
        /* CHAR */
        {
                traverse_nop,
                ob_free
        },
        /* PORT */
        {
                traverse_nop,
                free_port
        },
        /* VECTOR */
        {
                traverse_vector,
                0
        },
        /* MAP */
        {
                traverse_map,
                free_map
        },
        /* NETADDR */
        {
                traverse_nop,
                0
        },
        /* SIGNAL */
        {
                traverse_signal,
                0
        },
        /* STRBUF */
        {
                traverse_nop,
                free_strbuf
        },
        /* FUNCTION */
        {
                traverse_func,
                0
        },
        /* ENVIRON */
        {
                0,
                0
        },
        /* GCPROT */
        {
                traverse_gcprot,
                0
        },
        /* SENTiNEL */
        {
                0,
                0
        },
};





/**
 * Get the name of an object type.
 */
char *type_name(objtype_t type)
{
        static char *tname[] = {
                "INVALiD",
                "SYMBOL",
                "PAIR",
                "NUMBER",
                "STRING",
                "CHAR",
                "PORT",
                "VECTOR",
                "MAP",
                "INETADDR",
                "SIGNAL",
                "STRBUF",
                "FUNCTION",
                "ENVIRON",
                "GCPROT",
                "SENTiNEL"
        };

        if (type <= INVALiD || type >= SENTiNEL) {
                return "!INVALiD!";
        } else {
                return tname[type];
        }
}


void traverse_nop(obp_t ob, void (*do_func)(obp_t), int (*stop_func)(obp_t))
{
        return;
}

void traverse_symbol(obp_t ob, void (*do_func)(obp_t), int (*stop_func)(obp_t))
{
        Lsymbol_t *sym = AS(ob, SYMBOL);
        traverse_ob(sym->name, do_func, stop_func);
        traverse_ob(sym->value, do_func, stop_func);
        traverse_ob(sym->function, do_func, stop_func);
        traverse_ob(sym->props, do_func, stop_func);
}

void traverse_map(obp_t ob, void (*do_func)(obp_t), int (*stop_func)(obp_t))
{
        
        hashmap_t map = AS(ob, MAP)->map;
        hashmap_enum_start(map);
        mapentry_t ent;
        while ((ent = hashmap_enum_next(map))) {
                traverse_ob(entry_get_key(ent), do_func, stop_func);
                traverse_ob(entry_get_value(ent), do_func, stop_func);
        }
}

void traverse_pair(obp_t ob, void (*do_func)(obp_t), int (*stop_func)(obp_t))
{
        traverse_ob(CAR(ob), do_func, stop_func);
        traverse_ob(CDR(ob), do_func, stop_func);
}

void traverse_vector(obp_t ob, void (*do_func)(obp_t), int (*stop_func)(obp_t))
{
        Lvector_t *vec = AS(ob, VECTOR);
        for (uint i = 0; i < vec->nelem; i++) {
                traverse_ob(vec->elem[i], do_func, stop_func);
        }
}

void traverse_signal(obp_t ob, void (*do_func)(obp_t), int (*stop_func)(obp_t))
{
        Lsignal_t *sig = AS(ob, SIGNAL);
        traverse_ob(sig->data, do_func, stop_func);
        traverse_ob(sig->message, do_func, stop_func);
}

void traverse_func(obp_t ob, void (*do_func)(obp_t), int (*stop_func)(obp_t))
{
        Lfunction_t *func = AS(ob, FUNCTION);
        switch (func->type) {
            case F_AUTOLOAD:
                traverse_ob(func->impl.filename, do_func, stop_func);
                break;
            case F_FORM:
                traverse_ob(func->impl.form, do_func, stop_func);
                break;
            default:                    /* nothing for the other type(s) */
                break;
        }
}

void traverse_gcprot(obp_t ob, void (*do_func)(obp_t), int (*stop_func)(obp_t))
{
        gcp_t gcp = AS(ob, GCPROT);
        if (gcp->is_obpp) {
                traverse_ob(*gcp->item.obpp, do_func, stop_func);
        } else {
                traverse_ob(gcp->item.value, do_func, stop_func);
        }
}

void traverse_ob(obp_t ob, void (*do_func)(obp_t), int (*stop_func)(obp_t))
{
        if (!ob || stop_func(ob)) {
                return;
        }
        if (traceflag) {
                printf("  %s: ", type_name(ob->type));
                print(ob);
        }
        do_func(ob);

        uchar type = ob->type;
        assert(type > INVALiD && type < SENTiNEL);
        oops[type].op_traverse(ob, do_func, stop_func);
}


void free_obj(obp_t ob)
{
        uchar type = ob->type;
        assert(type > INVALiD && type < SENTiNEL);
        oops[type].op_free(ob);
}


void free_port(obp_t ob)
{
        port_flush(ob);
        close_port(ob);
        ob_free(ob);
}


void free_map(obp_t ob)
{
        Lmap_t *ob_map = AS(ob, MAP);
        hashmap_destroy(ob_map->map);
        ob_free(ob);
}

void free_strbuf(obp_t ob)
{
        Lstrbuf_t *ob_strbuf = AS(ob, STRBUF);
        xfree(ob_strbuf->strbuf);
        ob_free(ob);
}


        
/**
 * Makes a new symbol. To be called _only_ by intern()!
 */
static obp_t new_symbol(obp_t name)
{
        Lsymbol_t *ob = NEW_OBJ(SYMBOL);
        ob->name = name;
        Lstring_t *ob_name = AS(name, STRING);
        if (traceflag) {
                printf("new symbol %*s\n", ob_name->length, ob_name->content);
        }
        hashmap_put(symbols_map, name, (obp_t) ob);
        return (obp_t) ob;
}


/**
 * Get the symbol with the specified name. Is created if it does not exist.
 */
obp_t intern(char *name, int namelen)
{
        obp_t s_name = new_string(name, namelen);
        obp_t symbol = hashmap_get(symbols_map, s_name);
        if (!symbol) {
                symbol = new_symbol(s_name);
        }
        return symbol;
}


/**
 * Get a symbol with the specified name from a zero-terminated string.
 */
obp_t intern_z(char *name)
{
        return intern(name, strlen(name));
}


obp_t new_pair(obp_t car, obp_t cdr)
{
        Lpair_t *ob = NEW_OBJ(PAIR);
        ob->car = car;
        ob->cdr = cdr;
        return (obp_t) ob;
}


obp_t new_integer(long value)
{
        Lnumber_t *ob = NEW_OBJ(NUMBER);
        ob->value = value;
        ob->obj.eq_is_eqv = 1;
        IS_INT(ob) = 1;
        return (obp_t) ob;
}

obp_t new_ldouble(long double value)
{
        Lnumber_t *ob = NEW_OBJ(NUMBER);
        ob->value = value;
        ob->obj.eq_is_eqv = 1;
        return (obp_t) ob;
}

obp_t new_string(char *content, uint length)
{
        Lstring_t *ob = NEW_OBJ2(STRING, sizeof(Lstring_t) + length);
        ob->length = length;
        ob->obj.eq_is_eqv = 1;
        memcpy(ob->content, content, length);
        return (obp_t) ob;
}

obp_t new_strbuf(char *content, uint length)
{
        Lstrbuf_t *ob = NEW_OBJ(STRBUF);
        strbuf_t sb = ob->strbuf = strbuf_new();
        if (content) {
                strbuf_nappend(sb, content, length);
        }
        return (obp_t) ob;
}

obp_t new_zstring(char *zero_terminated_string)
{
        return new_string(zero_terminated_string, strlen(zero_terminated_string));
}

obp_t new_char(int content)
{
        Lchar_t *ob = NEW_OBJ(CHAR);
        ob->value = content;
        return (obp_t) ob;
}

obp_t new_port(char *name, FILE *stream, int fd, strbuf_t sb,
               port_type_t type, uint in, uint out)
{
        Lport_t *ob = NEW_OBJ(PORT);
        ob->name = name;
        ob->type = type;
        switch (type) {
            case STREAM_PORT:
                ob->port.stream = stream;
                break;
            case FD_PORT:
                ob->port.fd = fd;
                break;
            case STRING_PORT:
                ob->port.strbuf = sb;
                break;
            default:
                fprintf(stderr, "invalid type %d of new port", ob->type);
                exit(1);
        }
        ob->ungotten = -1;
        ob->in = !!in;
        ob->out = !!out;
        ob->closed = 0;
        ob->obj.eq_is_eqv = 1;
        return (obp_t) ob;
}

obp_t new_map(eq_type_t eq_type, int weak_keyref)
{
        Lmap_t *ob = NEW_OBJ(MAP);
        ob->map = hashmap_create(eq_type);
        ob->eq_type = (char) eq_type;
        ob->weak_keyref = weak_keyref;
        return (obp_t) ob;
}



obp_t new_netaddr(struct addrinfo *ai)
{
        
        return 0;
}

/**
 * Initialize the objects module, including globals.
 */
void init_objects(void)
{
        symbols = new_map(EQ_EQV, 0);
        symbols_map = AS(symbols, MAP)->map;

        the_Nil = intern_z(NIL_NAME);
        AS(the_Nil, SYMBOL)->value = the_Nil;
        the_Nil->immutable = 1;
        the_T = intern_z(T_NAME);
        AS(the_T, SYMBOL)->value = the_T;
        the_T->immutable = 1;
        
        the_Lambda = intern_z(LAMBDA_NAME);
        the_Mu = intern_z(SPECIAL_NAME);
}


/* EOF */
