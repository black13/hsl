/* Copyright (c) 2010, 2011 Juergen Nickelsen <ni@jnickelsen.de>
 * See the file COPYRIGHT for details.
 */

#include "cbasics.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


void *xmalloc(size_t size, char *purpose)
{
        void *tmp = malloc(size);
        if (!tmp) {
                fprintf(stderr, "cannot malloc %zu bytes (%s)\n",
                        size, purpose);
                exit(2);
        }
        return tmp;
}

void *xstrdup(char *s, char *purpose)
{
        char *tmp = xmalloc(strlen(s) + 1, purpose);
        strcpy(tmp, s);
        return tmp;
}

void *xcalloc(size_t nelems, size_t elsize, char *purpose)
{
        void *tmp = calloc(nelems, elsize);
        if (!tmp) {
                fprintf(stderr, "cannot calloc %zu * %zu bytes (%s)\n",
                        nelems, elsize, purpose);
                exit(2);
        }
        return tmp;
}


void *xrealloc(void *oldmem, size_t newsize, char *purpose)
{
        void *tmp = realloc(oldmem, newsize);
        if (!tmp) {
                fprintf(stderr, "cannot realloc to %zu bytes (%s)\n",
                        newsize, purpose);
                exit(2);
        }
        return tmp;
}


void xfree(void *mem)
{
        free(mem);
}

/* EOF */
