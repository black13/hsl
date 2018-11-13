/* Copyright (c) 2010, 2011 Juergen Nickelsen <ni@jnickelsen.de>
 * See the file COPYRIGHT for details.
 */

#ifndef __GC_H_INC
#define __GC_H_INC

#include "cbasics.h"

gcp_t gc_start_protect(char *file, int line);
void gc_protect(char *what, int line, obp_t *obpp);
void gc_unprotect(gcp_t prot_state, char *file, int line);

#define PROTECT gcp_t gc_prot_state = gc_start_protect(__FILE__, __LINE__)
#define UNPROTECT gc_unprotect(gc_prot_state, __FILE__, __LINE__)
#define PROTVAR(var) obp_t var = the_Nil; \
        gc_protect(__FILE__":"#var, __LINE__, &var)
#define PROTVAL(var, val) obp_t var = val; \
        gc_protect(__FILE__":"#var, __LINE__, &var)

void gc();

#define protect(obvar) gc_protect(__FILE__":"#obvar, __LINE__, &obvar)


#endif  /* __GC_H_INC */
