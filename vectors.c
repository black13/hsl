/* Copyright (c) 2010, 2011 Juergen Nickelsen <ni@jnickelsen.de>
 * See the file COPYRIGHT for details.
 */

#include <string.h>
#include "objects.h"
#include "xmemory.h"

static void v_realloc(Lvector_t *v, uint newsize)
{
        v->elem = xrealloc(v->elem, newsize * sizeof(obp_t), "vector resize");
        v->allocated = newsize;
}

obp_t new_vector(uint nelem)
{
        Lvector_t *vec = NEW_OBJ(VECTOR);
        if (nelem) {
                v_realloc(vec, nelem);
        }
        return (obp_t) vec;
}

obp_t vector_append(obp_t ob, obp_t new_elem)
{
        Lvector_t *vec = AS(ob, VECTOR);

        if (vec->nelem >= vec->allocated) {
                v_realloc(vec, MIN(vec->allocated * 2, 1));
        }
        vec->elem[++vec->nelem] = new_elem;
        return ob;
}

obp_t vector_put(obp_t ob, obp_t new_elem, uint slot)
{
        Lvector_t *vec = AS(ob, VECTOR);

        if (slot >= vec->allocated) {
                v_realloc(vec, MIN(slot + 1, 1));
                /* must zero out newly allocated area */
                char *start = (char *) (vec->elem + vec->nelem);
                uint n_chars = (char *) (vec->elem + slot) - start;
                memset(start, 0, n_chars);
        }
        vec->elem[slot] = new_elem;
        return ob;
}

obp_t vector_get(obp_t ob, uint slot)
{
        Lvector_t *vec = AS(ob, VECTOR);
        if (slot >= vec->nelem) {
                return the_Nil;
        }
        if (vec->elem[slot]) {
                return vec->elem[slot];
        } else {
                return the_Nil;
        }
}

/* EOF */
