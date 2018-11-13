/* Copyright (c) 2010, 2011 Juergen Nickelsen <ni@jnickelsen.de>
 * See the file COPYRIGHT for details.
 */
/*
 * Operation common to all object types, mainly memory management.
 */

#include <string.h>
#include "objects.h"
#include "xmemory.h"
#include "gc.h"

/* The freelist array is indexed by object size in units of 8 bytes; obviously
 * the first two entries aren't used. The array should cover all common object
 * sizes; to find the best compromise will be a matter of optimization. For the
 * beginning, cover many and count them. Objects with a larger size will be
 * handled by malloc/free each time.
 */

obp_t freelist[FREELIST_ENTRIES];
long ob_sizecount[FREELIST_ENTRIES];    /* count objects per size */


long object_count = 0;                  /* count of all objects ever created */

obp_t alloced_obs;                      /* list of all objects not in the
                                           freelist */

gcp_t pushdown_list;


void pushdown(obp_t value)
{
        gcp_t entry = (gcp_t) new_object(sizeof(struct GCPROT), GCPROT);
        entry->item.value = value;
        entry->next = pushdown_list;
        entry->is_obpp = 0;
        pushdown_list = entry;
}

obp_t popup(void)
{
        gcp_t entry = pushdown_list;
        pushdown_list = pushdown_list->next;
        obp_t value = entry->item.value;
        ob_free((obp_t) entry);
        return value;
}


void ob_free(obp_t ob)
{
        int index = ob->size >> 3;

        memset(ob, 0, ob->size);
        ob->next = freelist[index];
        freelist[index] = ob;
        ob_sizecount[index]++;
}


/**
 * Create a new object and register it so it gets included in the garbage
 * collection sweep.
 */
obp_t new_object(uint size, int type)
{
        obp_t ob = 0;                   /* the object to be */

        if (object_count
            && object_count % GC_OBJ_COUNT == 0
            && type != GCPROT)
        {
                gc();
        }

        if (size % OBSIZE_UNIT) {
                size += OBSIZE_UNIT - (size % OBSIZE_UNIT);
        }
        
        if (size <= FREELIST_MAXSIZE) {
                int index = size >> 3;
                if (freelist[index]) {
                        ob = freelist[index];
                        freelist[index] = freelist[index]->next;
                        ob_sizecount[index]--;
                }
        }
        if (ob == 0) {
                ob = xcalloc(1, size, "new object");
        }

        object_count++;
        memset(ob, 0, size);
        assert(type);
        ob->type = type;
        ob->size = size;
        ob->next = alloced_obs;
        alloced_obs = ob;

        return ob;
}

/* EOF */
