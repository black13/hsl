/* Copyright (c) 2010, 2011 Juergen Nickelsen <ni@jnickelsen.de>
 * See the file COPYRIGHT for details.
 */

void *xmalloc(int size, char *purpose);
void *xcalloc(int nelems, int elsize, char *purpose);
void *xrealloc(void *oldmem, int newsize, char *purpose);
void *xstrdup(char *s, char *purpose);
void xfree(void *mem);
