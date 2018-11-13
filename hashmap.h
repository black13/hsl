/* Copyright (c) 2010, 2011 Juergen Nickelsen <ni@jnickelsen.de>
 * See the file COPYRIGHT for details.
 */
/*
 * Map with objects as keys and values. A map may be weak (not keeping key
 * objects from being gced).
 */

#ifndef __HASHMAP_H_INC
#define __HASHMAP_H_INC


#include "cbasics.h"

typedef enum { EQ_EQ, EQ_EQV, EQ_EQUAL } eq_type_t;

void hashmap_remove(hashmap_t map, obp_t key);

int hashmap_put(hashmap_t map, obp_t key, obp_t value);

obp_t hashmap_get(hashmap_t map, obp_t key);

hashmap_t hashmap_create(eq_type_t eq_type);

unsigned int hashmap_size(hashmap_t map);

obp_t hashmap_keys(hashmap_t map);

obp_t hashmap_values(hashmap_t map);

obp_t hashmap_pairs(hashmap_t map);

void hashmap_destroy(hashmap_t map);

void hashmap_print(hashmap_t map);

typedef struct map_entry *mapentry_t;

mapentry_t hashmap_get_entry(hashmap_t map, obp_t key);

void entry_put_value(mapentry_t entry, obp_t value);

obp_t entry_get_value(mapentry_t entry);

obp_t entry_get_key(mapentry_t entry);

void hashmap_enum_start(hashmap_t map);

mapentry_t hashmap_enum_next(hashmap_t map);

#endif  /* __HASHMAP_H_INC */
