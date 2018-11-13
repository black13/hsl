/* Copyright (c) 2010, 2011 Juergen Nickelsen <ni@jnickelsen.de>
 * See the file COPYRIGHT for details.
 */

#include "cbasics.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <stddef.h>

#include "hashmap.h"
#include "objects.h"
#include "xmemory.h"
#include "functions.h"
#include "printer.h"
#include "tunables.h"
#include "gc.h"

#ifdef MIN
#undef MIN
#endif  /* MIN */
#define MIN(a, b) ((a) < (b) ? (a) : (b))

/* structure of any entry in hashtable */
struct map_entry {
        obp_t key;
        obp_t value;
        struct map_entry *next;
};

typedef int (*eq_func_t)(obp_t o1, obp_t o2);

struct hashmap {                        /*  */
        mapentry_t *table;                /* Array of hash buckets. */
        unsigned int n_buckets;         /* Size of the above. */
        unsigned int n_entries;         /* count of entries we used */
        unsigned int listlen_max;       /* maximum length of entry list */
        unsigned int enum_index;        /* index of bucket for enumeration */
        mapentry_t enum_ptr;              /* Pointer to element for enumeration */
        eq_func_t eql;                  /* how do we consider keys equal? */
};


static int eq_eq(obp_t ob1, obp_t ob2)
{
        return ob1 == ob2;
}

static int eq_eqv(obp_t ob1, obp_t ob2)
{
        if (ob1 == ob2) {
/*              printf("hashmap:eq_eqv: ob1 == ob2\n"); */
                return 1;
        } else if (ob1->eq_is_eqv) {    /* must compare contents */
/*              printf("hashmap:eq_eqv: compare\n"); */
/*              xdump(stdout, ob1, ob1->size); */
/*              printf(".\n"); */
/*              xdump(stdout, ob2, ob2->size); */
                return ob1->size == ob2->size
                        && !memcmp(&ob1->size,
                                   &ob2->size,
                                   ob1->size - offsetof(Lobject_t, size));
        } else {
/*              printf("hashmap:eq_eqv: ob1 != ob2\n"); */
                return 0;
        }
}

static int eq_equal(obp_t ob1, obp_t ob2)
{
        assert(!"yet implemented");
        return 0;
}

eq_func_t eq_funcs[] = {
        eq_eq,
        eq_eqv,
        eq_equal
};


/**
 * Hash function using FNV-1a algorithm.
 */
static int hash_index(hashmap_t map, obp_t key)
{
        unsigned int hashval = 2166136261;
        unsigned int i;
        char *s;
        int keylen;

        if (key->eq_is_eqv) {           /* must compare contents */
                s = (char *) &(key->size);
                keylen = key->size - offsetof(Lobject_t, size);
        } else {                        /* need only compare pointer */
                s = (char *) &key;
                keylen = sizeof(key);
        }
        for (i = 0; i < keylen; i++) {
                hashval ^= s[i];
                hashval *= 16777619;
        }
/*      printf("hash value of "); */
/* #ifndef HASHMAP_MAIN */
/*      describe(key); */
/* #else */
/*      printf("%*s ", THE_STRINGL(key)); */
/* #endif        */
/*      printf("is %u\n", hashval); */
        return hashval % map->n_buckets;
}


static mapentry_t remove_from_list(hashmap_t map, mapentry_t list, obp_t key)
{
        if (list == NULL) {
                return NULL;
        } else if (map->eql(list->key, key)) {
                mapentry_t rest = list->next;
                map->n_entries--;
                return rest;
        } else {
                list->next = remove_from_list(map, list->next, key);
                return list;
        }
}


void hashmap_remove(hashmap_t map, obp_t key)
{
        mapentry_t *bucket = map->table + hash_index(map, key); 
        
        *bucket = remove_from_list(map, *bucket, key);
}


static void resize_table(hashmap_t map)
{
        unsigned int oldsize = map->n_buckets;
        //printf("resize to new n_buckets: %u\n", 2 * map->n_buckets);
        //hashmap_print(map);
        map->n_buckets *= 2;
        mapentry_t *newtable = xcalloc(map->n_buckets, sizeof(mapentry_t), 
                                       "resized hashtable");
        map->listlen_max = 0;
        for (int i = 0; i < oldsize; i++) {
                mapentry_t elp = map->table[i];
                while (elp) {
                        mapentry_t next_el = elp->next;
                        mapentry_t *newbucket = newtable + hash_index(map, elp->key);
                        elp->next = *newbucket;
                        *newbucket = elp;
                        //printf("moved ");
                        //print_expr(elp->key, 0);
                        //printf(" => ");
                        //print_expr(elp->value, 0);
                        //printf("\n");
                        elp = next_el;
                }
        }
        free(map->table);
        map->table = newtable;
        //hashmap_print(map);
}


/* return 1 if entry was already present, 0 else
 */
int hashmap_put(hashmap_t map, obp_t key, obp_t value)
{
        mapentry_t *bucket = map->table + hash_index(map, key);
        mapentry_t elp;
        unsigned int list_length = 0;   /* length of entry list */
        int was_present = 0;
        
        for (elp = *bucket; elp; elp = elp->next) {
                list_length++;
                if (map->eql(elp->key, key)) {
                        was_present = 1;
                        break;
                }
        }
        if (list_length > map->listlen_max) {
                map->listlen_max = list_length;
        }
        // printf("entry list length for %s: %u\n", key, list_length);

        /* after this loop, elp is either NULL or points to the correct
         * entry
         */
        if (elp == NULL) {
                elp = xmalloc(sizeof(struct map_entry), "new entry");
                elp->next = *bucket;
                *bucket = elp;
                elp->key = key;
                map->n_entries++;
        }
        elp->value = value;

        if (list_length > HMAP_MAXLISTLEN) {
                resize_table(map);
        }

        assert(map->n_entries == hashmap_size(map));
        return was_present;
}


obp_t hashmap_get(hashmap_t map, obp_t key)
{
        return entry_get_value(hashmap_get_entry(map, key));
}


mapentry_t hashmap_get_entry(hashmap_t map, obp_t key)
{
        mapentry_t elp = map->table[hash_index(map, key)]; 
        unsigned int list_length = 0;   /* length of entry list */

        while (elp) {
                list_length++;
                if (map->eql(elp->key, key)) {
                        return elp;
                }
                elp = elp->next;
        }
        if (list_length > map->listlen_max) {
                map->listlen_max = list_length;
        }
        return NULL;
}


void entry_put_value(mapentry_t entry, obp_t value)
{
        entry->value = value;
}


obp_t entry_get_value(mapentry_t entry)
{
        return entry ? entry->value : NULL;
}

obp_t entry_get_key(mapentry_t entry)
{
        return entry ? entry->key : NULL;
}


hashmap_t hashmap_create(eq_type_t eq_type)
{
        hashmap_t map = xmalloc(sizeof(struct hashmap), "new hashmap_t");

        map->n_buckets = 1;
        map->n_entries = 0;
        map->listlen_max = 0;
        map->table = xcalloc(map->n_buckets, sizeof(mapentry_t), "new map->table");
        map->eql = eq_funcs[eq_type];
        
        return map;
}


uint hashmap_size(hashmap_t map)
{
        int val1 = 0;
        int val2 = map->n_entries;
        
        for (int i = 0; i < map->n_buckets; i++) {
                for (mapentry_t el = map->table[i]; el; el = el->next) {
                        val1++;
                }
                
        }
        // printf("map size = %d ==? %d\n", val1, val2);
        return val2;
}


obp_t hashmap_keys(hashmap_t map)
{
        obp_t result = the_Nil;

        for (int bucket = 0; bucket < map->n_buckets; bucket++) {
                for (mapentry_t el = map->table[bucket]; el; el = el->next) {
                        result = new_pair(el->key, result);
                }
        }
        return result;
}

obp_t hashmap_values(hashmap_t map)
{
        PROTECT;
        PROTVAR(retval);

        for (int bucket = 0; bucket < map->n_buckets; bucket++) {
                for (mapentry_t el = map->table[bucket]; el; el = el->next) {
                        retval = new_pair(el->value, retval);
                }
        }
        UNPROTECT;
        return retval;
}

obp_t hashmap_pairs(hashmap_t map)
{
        PROTECT;
        PROTVAR(retval);
        PROTVAR(kvpair);

        for (int bucket = 0; bucket < map->n_buckets; bucket++) {
                for (mapentry_t el = map->table[bucket]; el; el = el->next) {
                        kvpair = new_pair(el->key, el->value);
                        retval = new_pair(kvpair, retval);
                }
        }
        UNPROTECT;
        return retval;
}

void hashmap_destroy(hashmap_t map)
{
        
}


void hashmap_print(hashmap_t map)
{
        printf("{\n");
        for (int bucket = 0; bucket < map->n_buckets; bucket++) {
                for (mapentry_t el = map->table[bucket]; el; el = el->next) {
                        printf("  ");
                        print_expr(el->key, 0);
                        printf(" => ");
                        print_expr(el->value, 0);
                        printf("\n");
                }
        }
        printf("}\n");
}


void hashmap_enum_start(hashmap_t map)
{
        map->enum_index = 0;
        map->enum_ptr = map->table[map->enum_index];
}

mapentry_t hashmap_enum_next(hashmap_t map)
{
        while (map->enum_ptr == NULL) {
                map->enum_index++;
                if (map->enum_index >= map->n_buckets) {
                        return 0;
                }
                map->enum_ptr = map->table[map->enum_index];
        }
        mapentry_t ent = map->enum_ptr;

        map->enum_ptr = ent->next;
        return ent;
}


#ifdef HASHMAP_MAIN                             \

#define NENTRIES 10

static obp_t testGetEntry(hashmap_t map, obp_t key)
{
        obp_t tmp = hashmap_get(map, key);
        if (tmp == NULL) {
                return new_zstring("(NULL)");
        } else {
                return tmp;
        }
}


int main (void) {
        hashmap_t hashtable =
                hashmap_create(EQ_EQV);

        obp_t result = testGetEntry(hashtable, new_zstring("key4r"));
        printf("get: %*s\n", THE_STRINGL(result));
        hashmap_put(hashtable, new_zstring("key1"), new_zstring("value1"));
        hashmap_put(hashtable, new_zstring("key2"), new_zstring("value2"));
        hashmap_put(hashtable, new_zstring("key3"), new_zstring("value3"));
        hashmap_put(hashtable, new_zstring("key4"), new_zstring("value4"));
    
        result = testGetEntry(hashtable, new_zstring("key4r"));
        printf("get: %*s\n", THE_STRINGL(result));
        result = testGetEntry(hashtable, new_zstring("key1"));
        printf("get: %*s\n", THE_STRINGL(result));
        result = testGetEntry(hashtable, new_zstring("key3"));
        printf("get: %*s\n", THE_STRINGL(result));
        result = testGetEntry(hashtable, new_zstring("key2"));
        printf("get: %*s\n", THE_STRINGL(result));
        result = testGetEntry(hashtable, new_zstring("key4"));
        printf("get: %*s\n", THE_STRINGL(result));

        hashmap_remove(hashtable, new_zstring("key3")); 
        result = testGetEntry(hashtable, new_zstring("key3"));
        printf("get: %*s\n", THE_STRINGL(result));

        char key[100];
        char value[100];

        printf("put entries:\n");
        for (int i = 0; i < NENTRIES; i++) {
                sprintf(key, "key%d", i);
                sprintf(value, "valwasichwill%d", i);
                hashmap_put(hashtable, new_zstring(key), new_zstring(value));
        }

        printf("get entries:\n");
        for (int i = 0; i < NENTRIES; i++) {
                sprintf(key, "key%d", i);
                sprintf(value, "valwasichwill%d", i);
                obp_t result = testGetEntry(hashtable, new_zstring(key));
                if (strlen(value) != AS(result, STRING)->length
                    || strncmp(value, THE_STRINGS(result)))
                {
                        printf("ERROR: %s != %s: %*s\n", key, value,
                               THE_STRINGL(result));
                }
        }
        printf("remove entries:\n");
        for (int i = 0; i < NENTRIES; i++) {
                sprintf(key, "key%d", i);
                hashmap_remove(hashtable, new_zstring(key));
                if (hashmap_get(hashtable, new_zstring(key))) {
                        printf("ERROR: %s not removed\n", key);
                }
        }
        printf("listlen_max = %u\n", hashtable->listlen_max);

        return 0;
}
#endif  /* HASHMAP_MAIN */
