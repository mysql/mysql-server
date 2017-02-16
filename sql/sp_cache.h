/* -*- C++ -*- */
/* Copyright (c) 2002, 2012, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef _SP_CACHE_H_
#define _SP_CACHE_H_

#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class implementation */
#endif

#include "my_global.h"                          /* ulong */

/*
  Stored procedures/functions cache. This is used as follows:
   * Each thread has its own cache.
   * Each sp_head object is put into its thread cache before it is used, and
     then remains in the cache until deleted.
*/

class sp_head;
class sp_cache;
class sp_name;

/*
  Cache usage scenarios:
  1. Application-wide init:
    sp_cache_init();

  2. SP execution in thread:
  2.1 While holding sp_head* pointers:
  
    // look up a routine in the cache (no checks if it is up to date or not)
    sp_cache_lookup(); 
    
    sp_cache_insert();
    sp_cache_invalidate();
  
  2.2 When not holding any sp_head* pointers:
    sp_cache_flush_obsolete();
  
  3. Before thread exit:
    sp_cache_clear();
*/

void sp_cache_init();
void sp_cache_end();
void sp_cache_clear(sp_cache **cp);
void sp_cache_insert(sp_cache **cp, sp_head *sp);
sp_head *sp_cache_lookup(sp_cache **cp, sp_name *name);
void sp_cache_invalidate();
void sp_cache_flush_obsolete(sp_cache **cp, sp_head **sp);
ulong sp_cache_version();
void sp_cache_enforce_limit(sp_cache *cp, ulong upper_limit_for_elements);

#endif /* _SP_CACHE_H_ */
