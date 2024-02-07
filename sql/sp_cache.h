/* -*- C++ -*- */
/* Copyright (c) 2002, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef _SP_CACHE_H_
#define _SP_CACHE_H_

#include <sys/types.h>

#include "my_inttypes.h"

/*
  Stored procedures/functions cache. This is used as follows:
   * Each thread has its own cache.
   * Each sp_head object is put into its thread cache before it is used, and
     then remains in the cache until deleted.
*/

class sp_cache;
class sp_head;
class sp_name;

/*
  Cache usage scenarios:
  1. SP execution in thread:
  1.1 While holding sp_head* pointers:

    // look up a routine in the cache (no checks if it is up to date or not)
    sp_cache_lookup();

    sp_cache_insert();
    sp_cache_invalidate();

  1.2 When not holding any sp_head* pointers:
    sp_cache_flush_obsolete();

  2. Before thread exit:
    sp_cache_clear();
*/

void sp_cache_clear(sp_cache **cp);
void sp_cache_insert(sp_cache **cp, sp_head *sp);
sp_head *sp_cache_lookup(sp_cache **cp, const sp_name *name);
void sp_cache_invalidate();
void sp_cache_flush_obsolete(sp_cache **cp, sp_head **sp);
int64 sp_cache_version();
void sp_cache_enforce_limit(sp_cache *cp, ulong upper_limit_for_elements);

/**
 * @brief Check if the sp_cache contains the specified stored program.
 *
 * @note If the sp is part of a recursion, check if the first instance is part
 * of the sp_cache
 *
 * @param[in] cp - the sp_cache that is to be checked.
 * @param[in] sp - the stored program that needs to be part of that cache.
 * @return true if the element is in the cache.
 * @return false if not.
 */
bool sp_cache_has(sp_cache *cp, sp_head *sp);

#endif /* _SP_CACHE_H_ */
