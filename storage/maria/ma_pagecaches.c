/* Copyright (C) 2003-2007 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
  Handling of multiple key caches

  The idea is to have a thread safe hash on the table name,
  with a default key cache value that is returned if the table name is not in
  the cache.
*/

#include "maria_def.h"
#include "ma_pagecache.h"
#include <hash.h>
#include <m_string.h>
#include "../../mysys/my_safehash.h"

/*****************************************************************************
  Functions to handle the pagecache objects
*****************************************************************************/

/* Variable to store all key cache objects */
static SAFE_HASH pagecache_hash;


my_bool multi_pagecache_init(void)
{
  return safe_hash_init(&pagecache_hash, 16, (uchar*) maria_pagecache);
}


void multi_pagecache_free(void)
{
  safe_hash_free(&pagecache_hash);
}

/*
  Get a key cache to be used for a specific table.

  SYNOPSIS
    multi_pagecache_search()
    key				key to find (usually table path)
    uint length			Length of key.
    def				Default value if no key cache

  NOTES
    This function is coded in such a way that we will return the
    default key cache even if one never called multi_pagecache_init.
    This will ensure that it works with old MyISAM clients.

  RETURN
    key cache to use
*/

PAGECACHE *multi_pagecache_search(uchar *key, uint length,
                                  PAGECACHE *def)
{
  if (!pagecache_hash.hash.records)
    return def;
  return (PAGECACHE*) safe_hash_search(&pagecache_hash, key, length,
                                       (void*) def);
}


/*
  Assosiate a key cache with a key


  SYONOPSIS
    multi_pagecache_set()
    key				key (path to table etc..)
    length			Length of key
    pagecache			cache to assococite with the table

  NOTES
    This can be used both to insert a new entry and change an existing
    entry
*/


my_bool multi_pagecache_set(const uchar *key, uint length,
			    PAGECACHE *pagecache)
{
  return safe_hash_set(&pagecache_hash, key, length, (uchar*) pagecache);
}


void multi_pagecache_change(PAGECACHE *old_data,
			    PAGECACHE *new_data)
{
  safe_hash_change(&pagecache_hash, (uchar*) old_data, (uchar*) new_data);
}
