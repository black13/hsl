/* Copyright (c) 2010, 2011 Juergen Nickelsen <ni@jnickelsen.de>
 * See the file COPYRIGHT for details.
 */

/**
 * For the moment the garbage collector is simply called after the allocation of
 * so many objects.
 */
#define GC_OBJ_COUNT 10000

/**
 * Maximum length of the bucket lists in the hashmap. The map will be expanded
 * when this amount is exceeded.
 */
#define HMAP_MAXLISTLEN 4


