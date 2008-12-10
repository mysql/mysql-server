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

#include "mysys_priv.h"
#include <keycache.h>
#include <hash.h>
#include <m_string.h>
#include "my_safehash.h"

/*****************************************************************************
  Functions to handle the key cache objects
*****************************************************************************/

/* Variable to store all key cache objects */
static SAFE_HASH key_cache_hash;


my_bool multi_keycache_init(void)
{
  return safe_hash_init(&key_cache_hash, 16, (uchar*) dflt_key_cache);
}


void multi_keycache_free(void)
{
  safe_hash_free(&key_cache_hash);
}

/*
  Get a key cache to be used for a specific table.

  SYNOPSIS
    multi_key_cache_search()
    key				key to find (usually table path)
    uint length			Length of key.
    def				Default value if no key cache

  NOTES
    This function is coded in such a way that we will return the
    default key cache even if one never called multi_keycache_init.
    This will ensure that it works with old MyISAM clients.

  RETURN
    key cache to use
*/

KEY_CACHE *multi_key_cache_search(uchar *key, uint length,
                                  KEY_CACHE *def)
{
  if (!key_cache_hash.hash.records)
    return def;
  return (KEY_CACHE*) safe_hash_search(&key_cache_hash, key, length,
                                       (void*) def);
}


/*
  Assosiate a key cache with a key


  SYONOPSIS
    multi_key_cache_set()
    key				key (path to table etc..)
    length			Length of key
    key_cache			cache to assococite with the table

  NOTES
    This can be used both to insert a new entry and change an existing
    entry
*/


my_bool multi_key_cache_set(const uchar *key, uint length,
			    KEY_CACHE *key_cache)
{
  return safe_hash_set(&key_cache_hash, key, length, (uchar*) key_cache);
}


void multi_key_cache_change(KEY_CACHE *old_data,
			    KEY_CACHE *new_data)
{
  safe_hash_change(&key_cache_hash, (uchar*) old_data, (uchar*) new_data);
}


