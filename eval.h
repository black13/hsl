/* Copyright (c) 2010, 2011 Juergen Nickelsen <ni@jnickelsen.de>
 * See the file COPYRIGHT for details.
 */

#include "objects.h"
#include "io.h"

extern long eval_count;
extern long apply_count;
extern long bind_count;

// obp_t evalfun(obp_t fun, obp_t args);
obp_t eval(obp_t ob, session_context_t *sc, int level);

obp_t apply(obp_t fun, obp_t args, session_context_t *sc, int level);


obp_t make_bindings(obp_t params, obp_t args, session_context_t *sc, int level);
void restore_bindings(obp_t params, int nargs, session_context_t *sc, int level);

/**
 * return NULL if argument is a proper function
 */
obp_t make_function(char *name, uint namelen, obp_t func, session_context_t *sc);
