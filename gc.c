/* Copyright (c) 2010, 2011 Juergen Nickelsen <ni@jnickelsen.de>
 * See the file COPYRIGHT for details.
 */

#include "cbasics.h"
#include <stdlib.h>
#include "objects.h"
#include "xmemory.h"
#include "gc.h"
#include "printer.h"

gcp_t gc_prot_root;

gcp_t gc_start_protect(char *file, int line)
{
        if (traceflag) {
                printf("PROTECT %s:%d:%p\n", file, line, gc_prot_root);
        }
        return gc_prot_root;
}

void gc_protect(char *what, int line, obp_t *obpp)
{
        if (traceflag) {
                printf("protect %s:%d:%p\n", what, line, obpp);
        }
        gcp_t newp = (gcp_t) new_object(sizeof(struct GCPROT), GCPROT);
        newp->item.obpp = obpp;
        newp->next = gc_prot_root;
        newp->is_obpp = 1;
        gc_prot_root = newp;
}


void gc_unprotect(gcp_t prot_state, char *file, int line)
{
        gc_prot_root = prot_state;
        if (traceflag) {
                printf("UNPROTECT %s:%d:%p\n", file, line, gc_prot_root);
        }
}

uint marked;
uint freed;
uint alloced;
uint visited;

void gc_mark(obp_t ob)
{
        ob->mark = 1;
        marked++;
}

int gc_stop_traverse(obp_t ob)
{
        return ob == 0 || ob->mark;
}


obp_t sweep_runner(obp_t obs, obp_t result)
{
        if (obs == 0) {
                return result;
        }

        visited++;
        obp_t first = obs;
        obp_t rest = obs->next;
        
        if (first->mark) {
                first->mark = 0;
                first->next = result;
                return sweep_runner(rest, first);
        } else {
                ob_free(first);
                freed++;
                return sweep_runner(rest, result);
        }
}


void gc_sweep(void)
{
        alloced_obs = sweep_runner(alloced_obs, 0);
        obp_t foo = alloced_obs;
        while (foo) {
                alloced++;
                foo = foo->next;
        }
}

void gc(void)
{
        marked = 0;
        freed = 0;
        alloced = 0;
        visited = 0;
        fprintf(stderr, "[GC");
        traverse_ob((obp_t) gc_prot_root, gc_mark, gc_stop_traverse);
        fprintf(stderr, ".");
        traverse_ob((obp_t) pushdown_list, gc_mark, gc_stop_traverse);
        fprintf(stderr, ".");
        traverse_ob(symbols, gc_mark, gc_stop_traverse);
        fprintf(stderr, ".");
        gc_sweep();
        fprintf(stderr, " %u marked, %u freed, %u alloced, %u visited]\n",
                marked, freed, alloced, visited);
}


/* EOF */
