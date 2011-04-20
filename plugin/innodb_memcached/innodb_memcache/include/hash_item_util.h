#ifndef NDBMEMCACHE_HASH_ITEM_UTIL_H
#define NDBMEMCACHE_HASH_ITEM_UTIL_H

#include <sys/types.h>
#include <stdint.h>

#include <memcached/engine.h> 

#include "dbmemcache_global.h"

#define ITEM_WITH_CAS 1

struct default_engine;   // forward reference; needed in items.h

DECLARE_FUNCTIONS_WITH_C_LINKAGE

#include "items.h"

uint16_t hash_item_get_key_len(const hash_item *item);
uint32_t hash_item_get_data_len(const hash_item *item);
char * hash_item_get_key(const hash_item *item);
char * hash_item_get_data(const hash_item *item);
uint64_t hash_item_get_cas(const hash_item* item);
uint64_t hash_item_get_exp(const hash_item* item);
uint32_t hash_item_get_flag(const hash_item* item);
uint64_t * hash_item_get_cas_ptr(const hash_item* item);
void	hash_item_set_flag(hash_item* item, uint32_t value);
void	hash_item_set_cas(hash_item* item, uint64_t cas);

END_FUNCTIONS_WITH_C_LINKAGE

#endif
