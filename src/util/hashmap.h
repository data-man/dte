#ifndef UTIL_HASHMAP_H
#define UTIL_HASHMAP_H

#include <stdbool.h>
#include <stddef.h>
#include "macros.h"

typedef void (*FreeFunction)(void *ptr);

typedef struct {
    char *key;
    void *value;
    size_t hash;
} HashMapEntry;

typedef struct {
    HashMapEntry *entries;
    size_t mask; // Length of entries (which is always a power of 2) minus 1
    size_t count; // Number of active entries
} HashMap;

typedef struct {
    size_t idx;
    HashMapEntry *entry;
} HashMapIter;

#define HASHMAP_INIT { \
    .entries = NULL, \
    .mask = 0, \
    .count = 0 \
}

bool hashmap_init(HashMap *map, size_t capacity) NONNULL_ARGS WARN_UNUSED_RESULT;
void hashmap_free(HashMap *map, FreeFunction free_value) NONNULL_ARG(1);
bool hashmap_insert(HashMap *map, char *key, void *value) NONNULL_ARGS WARN_UNUSED_RESULT;
void *hashmap_remove(HashMap *map, const char *key) NONNULL_ARGS;
HashMapEntry *hashmap_find(const HashMap *map, const char *key) NONNULL_ARGS WARN_UNUSED_RESULT;
bool hashmap_next(const HashMap *map, HashMapIter *iter) NONNULL_ARGS WARN_UNUSED_RESULT;
void hashmap_xinit(HashMap *map, size_t capacity) NONNULL_ARGS;
void hashmap_xinsert(HashMap *map, char *key, void *value) NONNULL_ARGS;

static inline void *hashmap_get(const HashMap *map, const char *key)
{
    HashMapEntry *e = hashmap_find(map, key);
    return e ? e->value : NULL;
}

#endif