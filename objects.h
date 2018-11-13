/* Copyright (c) 2010, 2011 Juergen Nickelsen <ni@jnickelsen.de>
 * See the file COPYRIGHT for details.
 */
/*
 * The object definitions
 */

#ifndef __OBJECTS_H_INC
#define __OBJECTS_H_INC

#include "cbasics.h"
#include "tunables.h"
#include <netdb.h>
#include <assert.h>
#include <stdio.h>

#include "builtins.h"
#include "numbers.h"
#include "strbuf.h"
#include "session.h"

/*
 * definitions for freelist management, see ob_common.c
 */
#define OBSIZE_UNIT        8
#define FREELIST_ENTRIES 131            /* for strings up to 1K */
#define FREELIST_MAXSIZE ((FREELIST_ENTRIES - 1) * OBSIZE_UNIT)


/* all types of objects */
typedef enum {
        INVALiD = 0,                    /* to block uninitialized ones */
        SYMBOL,                         /* an interned symbol */
        PAIR,                           /* aka cons cell */
        NUMBER,                         /* numbers (integers at first) */
        STRING,                         /* usually uninterned */
        CHAR,                           /* not just ints or 1-length strings */
        PORT,                           /* generalized I/O port */
        VECTOR,                         /* a vector of objects */
        MAP,                            /* a map, object => object */
        NETADDR,                        /* network endpoint (e. g. IP address
                                         * with port number). */
        SIGNAL,                         /* a signal object (errors, ipc) */
        STRBUF,                         /* a string buffer like Java's */
        FUNCTION,                       /* a function or special form, builtin
                                           or lambda/mu */
        ENVIRON,                        /* environment */
        GCPROT,                         /* GC protection entry, not
                                           Lisp-visible */
        SENTiNEL                        /* also invalId */
} objtype_t;

typedef enum {
        F_BUILTIN,
        F_FORM,
        F_AUTOLOAD,
        F_BYTECODE
} functype_t;

/* The generic part of the object struct, more or less the virtual superclass;
 * will be contained in the beginning of each instantiable object struct.
 *
 */
typedef struct OBJECT {                 /* the object in general */
        struct OBJECT *next;            /* linkage in allocated and free
                                           lists */
        uint size;                      /* length of the actual object */
        uchar type;                     /* 256 should be enough for everyone. */
        uint mark:1;                    /* for GC and things */
        uint eq_is_eqv:1;               /* may be eq even if not same */
        uint immutable:1;               /* immutable if non-zero; we want this
                                           e. g. for the stdin/out/err file
                                           handles */
        uint num_is_int:1;              /* 1 if number is actually an integer */
} Lobject_t;

#define IS(obj, obtype) (obj->type == obtype)
#define AS(obj, obtype) (assert(IS(obj, obtype)), (struct obtype *) obj)

#define IS_NIL(ob) (ob == the_Nil)
#define IS_LIST(ob) (IS_NIL(ob) || IS(ob, PAIR))

#define IS_BUILTIN(ob) (IS(ob, FUNCTION) && AS(ob, FUNCTION)->type == F_BUILTIN)
#define IS_FORM(ob) (IS(ob, FUNCTION) && AS(ob, FUNCTION)->type == F_FORM)
#define IS_AUTOLOAD(ob) (IS(ob, FUNCTION) &&                    \
                         AS(ob, FUNCTION)->type == F_AUTOLOAD)
#define IS_SPECIAL(ob) (IS(ob, FUNCTION) && AS(ob, FUNCTION)->is_special)

#define CAR(o) (AS(o, PAIR)->car)
#define CDR(o) (AS(o, PAIR)->cdr)
#define CADR(o) CAR(CDR(o))
#define CADDR(o) CAR(CDR(CDR(o)))
#define CADDDR(o) CAR(CDR(CDR(CDR(o))))

#define NEW_OBJ(obtype) AS(new_object(sizeof(struct obtype), obtype), \
                               obtype)
#define NEW_OBJ2(obtype, length) AS(new_object(length, obtype), obtype)

#define THE_STRINGS(ob) AS(ob, STRING)->content, AS(ob, STRING)->length
#define THE_STRINGL(ob) AS(ob, STRING)->length, AS(ob, STRING)->content

#define CHECKTYPE(p, o, t)                                              \
        do {                                                            \
                if (!IS(o, t)) {                                        \
                        ERROR(p, ERR_INVARG, o,                         \
                              "not of type %s", type_name(t));          \
                }                                                       \
        } while (0)

#define CHECKTYPE_RET(p, o, t)                                          \
        do {                                                            \
                if (!IS(o, t)) {                                        \
                        return throw_error(p, ERR_INVARG, o,            \
                                           "not of type %s", type_name(t)); \
                }                                                       \
        } while (0)



typedef struct SYMBOL {
        Lobject_t obj;                  /* generic part */
        obp_t name;                     /* pointer to name string object */
        obp_t value;                    /* the value as a variable */
        obp_t function;                 /* the value as a function */
        obp_t props;                    /* map of properties */
} Lsymbol_t;


typedef struct PAIR {                   /* a cons cell */
        Lobject_t obj;
        obp_t car;
        obp_t cdr;
} Lpair_t;


typedef struct NUMBER {                 /* a number of any type */
        Lobject_t obj;
        long double value;              /* will be something else some day */
} Lnumber_t;


typedef struct STRING {                 /* a string, immutable */
        Lobject_t obj;
        uint length;                    /* may be huge, perhaps even ulong */
        char content[1];                /* zero-terminated *plus*
                                           length-controlled */
} Lstring_t;

#define IS_EOF(ob) (IS(ob, CHAR) && AS(ob, CHAR)->value == EOF)

typedef struct CHAR {
        Lobject_t obj;
        int value;                      /* yep, this may be very wide */
} Lchar_t;


typedef enum { STREAM_PORT, FD_PORT, STRING_PORT } port_type_t;

typedef struct PORT {                   /* generalized I/O ports, this may
                                         * include sockets, buffers, etc. */
        Lobject_t obj;
        char *name;                     /* the name, for diagnostic output */
        union {
                FILE *stream;           /* this one if non-null */
                int fd;                 /* instead of FILE *, for poll() */
                strbuf_t strbuf;        /* to write to */
                
        } port;
        int ungotten;                   /* from ungetc if >= 0 */
        port_type_t type;
        unsigned in:1;                  /* may read from that port */
        unsigned out:1;                 /* may write to that port */
        unsigned closed:1;
} Lport_t;


typedef struct VECTOR {                 /* an array of things */
        Lobject_t obj;
        uint nelem;                     /* number of elements (last + 1) */
        uint allocated;                 /* number of allocated slots */
        obp_t *elem;                    /* allocated as necessary */
} Lvector_t;


#include "hashmap.h"

typedef struct MAP {                    /* object to object map */
        Lobject_t obj;
        char eq_type;
        uint eq_is_eqv:1;               /* compare not identity, but contents */
        uint weak_keyref:1;             /* let keys be collected */
        hashmap_t map;                  /* the map */
} Lmap_t;


typedef struct NETADDR {               /* as of sockets, ipaddr:port */
        Lobject_t obj;
        struct addrinfo *ai;            /* as returned from getaddrinfo() */
        uint flags;                     /* maybe replace by bitfields later */
} Lnetaddr_t;

typedef struct SIGNAL {                 /* an error object */
        Lobject_t obj;
        obp_t data;                     /* optional related data */
        obp_t message;                  /* a message string, human-readable (in
                                           theory) */
        short type;                     /* error or else */
        short code;                     /* id of the signal or error code */
} Lsignal_t;

typedef struct STRBUF {                 /* an expandable string buffer */
        Lobject_t obj;
        strbuf_t strbuf;
} Lstrbuf_t;

typedef struct FUNCTION {
        Lobject_t obj;
        union {
                builtin_func_t *builtin; /* the C function */
                obp_t form;             /* lambda or mu form; pre-checked for
                                           being a proper function, so we need
                                           not check it any more on
                                           application */
                obp_t filename;         /* for autoload */
        } impl;
        char *name;                     /* maybe undefined for lambdas */
        uint namelen;   
        short minargs;                  /* minimum number of arguments */
        short maxargs;                  /* maximum number of arguments, < 0 if
                                           unlimited */
        uchar type;                     /* F_BUILTIN, F_FORM, ... */
        uint is_special:1;              /* special form if non-zero */
        uint trace:1;                   /* trace this function */
} Lfunction_t;

typedef struct ENVIRON {
        Lobject_t obj;
        hashmap_t map;                  /* the current symbol map */
        struct ENVIRON *parent;         /* the parent environment */
} Lenviron_t;

typedef struct GCPROT {
        Lobject_t obj;
        union {
                obp_t *obpp;            /* for GC protect list */
                obp_t value;            /* for bindings pushdown list */
        } item;
        struct GCPROT *next;            /* next entry in GC protect list */
        uint is_obpp:1;                 /* obpp is used, not value */
} *gcp_t;

obp_t new_object(uint size, int type);
#define cons new_pair
obp_t new_pair(obp_t car, obp_t cdr);
#define new_pair0() new_pair(the_Nil, the_Nil)
obp_t new_integer(long value);
obp_t new_ldouble(long double value);
obp_t new_string(char *content, uint length);
obp_t new_zstring(char *zero_terminated_string);
obp_t new_char(int content);
obp_t new_port(char *name, FILE *stream, int fd, strbuf_t sb,
               port_type_t type, uint in, uint out);
obp_t new_vector(uint nelem);
obp_t new_map(eq_type_t eq_type, int weak_keyref);
obp_t new_netaddr(struct addrinfo *ai);
obp_t new_strbuf(char *content, uint length);
obp_t new_builtin(char *name, uint namelen, builtin_func_t builtin,
                  int is_special, short minargs, short maxargs);
obp_t new_form_function(char *name, uint namelen, obp_t form, int is_special,
                        short minargs, short maxargs);
obp_t all_symbols(void);

/**
 * Get the name of an object type.
 */
char *type_name(objtype_t type);

/**
 * Register a newly created object so it gets included in the garbage collection
 * sweep.
 */
void register_object(obp_t ob);

/**
 * Initialize the objects module.
 */
void init_objects(void);

/**
 * Get a symbol with the specified name. Is created if it does not exist.
 */
obp_t intern(char *name, int namelen);

/**
 * Get a symbol with the specified name from a zero-terminated string.
 */
obp_t intern_z(char *name);

/**
 * Return the list of all symbols.
 */
obp_t Loblist(void);

void pushdown(obp_t value);
obp_t popup(void);
void ob_free(obp_t ob);

void traverse_ob(obp_t ob, void (*do_func)(obp_t), int (*stop_func)(obp_t));


obp_t vector_append(obp_t ob, obp_t new_elem);
obp_t vector_put(obp_t ob, obp_t new_elem, uint slot);
obp_t vector_get(obp_t ob, uint slot);


extern obp_t symbols;                   /* map of all symbols */
extern obp_t the_Nil;
extern obp_t the_T;
extern obp_t the_Lambda;
extern obp_t the_Mu;
extern gcp_t pushdown_list;
extern obp_t alloced_obs;               /* all objects not in a free list */

extern int traceflag;
extern long object_count;
extern long ob_sizecount[FREELIST_ENTRIES];


#endif /* __OBJECTS_H_INC */
/* EOF */
