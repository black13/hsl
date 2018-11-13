/* Copyright (c) 2010, 2011 Juergen Nickelsen <ni@jnickelsen.de>
 * See the file COPYRIGHT for details.
 */
/**
 * Builtin functions
 */

#ifndef __BUILTINS_H_INC
#define __BUILTINS_H_INC

#include "cbasics.h"
#include "session.h"

typedef obp_t builtin_func_t(int nargs, obp_t args,
                             session_context_t *sc, int level);

obp_t register_builtin(char *name, builtin_func_t builtin,
                       int is_special, short minargs, short maxargs);
obp_t call_builtin(obp_t fun, obp_t args, session_context_t *sc, int level);
void init_builtins(void);

#endif  /* __BUILTINS_H_INC */

/* EOF */
