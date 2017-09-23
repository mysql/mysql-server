/*
  Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the
  Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
  Public License for more details.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA
*/
#ifndef NDBMEMCACHE_HASH_ITEM_UTIL_H
#define NDBMEMCACHE_HASH_ITEM_UTIL_H

// Work around a bug in the memcached C++ headers with GCC 4.x.
#ifndef bool
#define bool bool
#endif

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
