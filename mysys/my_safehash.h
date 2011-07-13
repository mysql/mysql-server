/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

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

#include <hash.h>

/*
  Struct to store a key and pointer to object
*/

typedef struct st_safe_hash_entry
{
  uchar *key;
  uint length;
  uchar *data;
  struct st_safe_hash_entry *next, **prev;
} SAFE_HASH_ENTRY;


typedef struct st_safe_hash_with_default
{
  mysql_rwlock_t mutex;
  HASH hash;
  uchar *default_value;
  SAFE_HASH_ENTRY *root;
} SAFE_HASH;


my_bool safe_hash_init(SAFE_HASH *hash, uint elements,
                       uchar *default_value);
void safe_hash_free(SAFE_HASH *hash);
uchar *safe_hash_search(SAFE_HASH *hash, const uchar *key, uint length,
                       uchar *def);
my_bool safe_hash_set(SAFE_HASH *hash, const uchar *key, uint length,
                      uchar *data);
void safe_hash_change(SAFE_HASH *hash, uchar *old_data, uchar *new_data);
