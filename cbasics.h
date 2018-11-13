/* Copyright (c) 2010, 2011 Juergen Nickelsen <ni@jnickelsen.de>
 * See the file COPYRIGHT for details.
 */
/*
 * basic C definitions needed here and there; must be self-contained so we can
 * include it at the top of every module
 */

#ifndef __CBASICS_H_INC
#define __CBASICS_H_INC


#define __EXTENSIONS__
#define __USE_POSIX
#define _GNU_SOURCE

//#define NDEBUG

typedef unsigned int uint;
typedef unsigned char uchar;
typedef unsigned long ulong;

typedef struct hashmap *hashmap_t;
typedef struct OBJECT *obp_t;

//

#ifdef MIN
#undef MIN
#endif  /* MIN */
#define MIN(a, b) ((a) <= (b) ? (a) : (b))
#ifdef MAX
#undef MAX
#endif  /* MAX */
#define MAX(a, b) ((a) >= (b) ? (a) : (b))


#endif /* __CBASICS_H_INC */
/* EOF */
