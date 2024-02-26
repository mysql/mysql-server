/*
  Copyright (c) 2017, 2023, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/
#ifndef NDBMEMCACHE_HASH_ITEM_UTIL_H
#define NDBMEMCACHE_HASH_ITEM_UTIL_H

// Work around a bug in the memcached C++ headers with GCC 4.x.
#ifndef bool
#define bool bool
#endif

#include <stdint.h>
#include <sys/types.h>

#include <memcached/engine.h>

#include "dbmemcache_global.h"

#define ITEM_WITH_CAS 1

struct default_engine;  // forward reference; needed in items.h

DECLARE_FUNCTIONS_WITH_C_LINKAGE

#include "items.h"

uint16_t hash_item_get_key_len(const hash_item *item);
uint32_t hash_item_get_data_len(const hash_item *item);
char *hash_item_get_key(const hash_item *item);
char *hash_item_get_data(const hash_item *item);
uint64_t hash_item_get_cas(const hash_item *item);
uint64_t hash_item_get_exp(const hash_item *item);
uint32_t hash_item_get_flag(const hash_item *item);
uint64_t *hash_item_get_cas_ptr(const hash_item *item);
void hash_item_set_flag(hash_item *item, uint32_t value);
void hash_item_set_cas(hash_item *item, uint64_t cas);

END_FUNCTIONS_WITH_C_LINKAGE

#endif
