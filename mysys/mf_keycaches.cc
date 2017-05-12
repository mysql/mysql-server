/* Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.

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

/**
  @file mysys/mf_keycaches.cc
  Handling of multiple key caches.

  The idea is to have a thread safe hash on the table name,
  with a default key cache value that is returned if the table name is not in
  the cache.
*/

#include <hash.h>
#include <keycache.h>
#include <string.h>
#include <sys/types.h>

#include "m_ctype.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql/psi/mysql_rwlock.h"
#include "mysql/service_mysql_alloc.h"
#include "mysys_priv.h"
#include "template_utils.h"

/*****************************************************************************
  General functions to handle SAFE_HASH objects.

  A SAFE_HASH object is used to store the hash, the lock and default value
  needed by the rest of the key cache code.
  This is a separate struct to make it easy to later reuse the code for other
  purposes

  All entries are linked in a list to allow us to traverse all elements
  and delete selected ones. (HASH doesn't allow any easy ways to do this).
*****************************************************************************/

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
  mysql_rwlock_t lock;
  HASH hash;
  uchar *default_value;
  SAFE_HASH_ENTRY *root;
} SAFE_HASH;


/*
  Free a SAFE_HASH_ENTRY

  This function is called by the hash object on delete
*/

static void safe_hash_entry_free(void *entry)
{
  DBUG_ENTER("free_assign_entry");
  my_free(entry);
  DBUG_VOID_RETURN;
}


/* Get key and length for a SAFE_HASH_ENTRY */

static const uchar *safe_hash_entry_get(const uchar *arg, size_t *length)
{
  const SAFE_HASH_ENTRY *entry= pointer_cast<const SAFE_HASH_ENTRY*>(arg);
  *length=entry->length;
  return entry->key;
}


/*
  Init a SAFE_HASH object

  SYNOPSIS
    safe_hash_init()
    hash		safe_hash handler
    elements		Expected max number of elements
    default_value	default value

  NOTES
    In case of error we set hash->default_value to 0 to allow one to call
    safe_hash_free on an object that couldn't be initialized.

  RETURN
    0  ok
    1  error
*/

static bool safe_hash_init(SAFE_HASH *hash, uint elements,
                           uchar *default_value)
{
  DBUG_ENTER("safe_hash");
  if (my_hash_init(&hash->hash, &my_charset_bin, elements, 0,
                   safe_hash_entry_get,
                   safe_hash_entry_free, 0,
                   key_memory_SAFE_HASH_ENTRY))
  {
    hash->default_value= 0;
    DBUG_RETURN(1);
  }
  mysql_rwlock_init(key_SAFE_HASH_lock, &hash->lock);
  hash->default_value= default_value;
  hash->root= 0;
  DBUG_RETURN(0);
}


/*
  Free a SAFE_HASH object

  NOTES
    This is safe to call on any object that has been sent to safe_hash_init()
*/

static void safe_hash_free(SAFE_HASH *hash)
{
  /*
    Test if safe_hash_init succeeded. This will also guard us against multiple
    free calls.
  */
  if (hash->default_value)
  {
    my_hash_free(&hash->hash);
    mysql_rwlock_destroy(&hash->lock);
    hash->default_value=0;
  }
}

/*
  Return the value stored for a key or default value if no key
*/

static uchar *safe_hash_search(SAFE_HASH *hash, const uchar *key, uint length)
{
  uchar *result;
  DBUG_ENTER("safe_hash_search");
  mysql_rwlock_rdlock(&hash->lock);
  result= my_hash_search(&hash->hash, key, length);
  mysql_rwlock_unlock(&hash->lock);
  if (!result)
    result= hash->default_value;
  else
    result= ((SAFE_HASH_ENTRY*) result)->data;
  DBUG_PRINT("exit",("data: %p", result));
  DBUG_RETURN(result);
}


/*
  Associate a key with some data

  SYONOPSIS
    safe_hash_set()
    hash			Hash handle
    key				key (path to table etc..)
    length			Length of key
    data			data to to associate with the data

  NOTES
    This can be used both to insert a new entry and change an existing
    entry.
    If one associates a key with the default key cache, the key is deleted

  RETURN
    0  ok
    1  error (Can only be EOM). In this case my_message() is called.
*/

static bool safe_hash_set(SAFE_HASH *hash, const uchar *key, uint length,
                          uchar *data)
{
  SAFE_HASH_ENTRY *entry;
  bool error= 0;
  DBUG_ENTER("safe_hash_set");
  DBUG_PRINT("enter",("key: %.*s  data: %p", length, key, data));

  mysql_rwlock_wrlock(&hash->lock);
  entry= (SAFE_HASH_ENTRY*) my_hash_search(&hash->hash, key, length);

  if (data == hash->default_value)
  {
    /*
      The key is to be associated with the default entry. In this case
      we can just delete the entry (if it existed) from the hash as a
      search will return the default entry
    */
    if (!entry)					/* nothing to do */
      goto end;
    /* unlink entry from list */
    if ((*entry->prev= entry->next))
      entry->next->prev= entry->prev;
    my_hash_delete(&hash->hash, (uchar*) entry);
    goto end;
  }
  if (entry)
  {
    /* Entry existed;  Just change the pointer to point at the new data */
    entry->data= data;
  }
  else
  {
    if (!(entry= (SAFE_HASH_ENTRY *) my_malloc(key_memory_SAFE_HASH_ENTRY,
                                               sizeof(*entry) + length,
					       MYF(MY_WME))))
    {
      error= 1;
      goto end;
    }
    entry->key= (uchar*) (entry +1);
    memcpy((char*) entry->key, (char*) key, length);
    entry->length= length;
    entry->data= data;
    /* Link entry to list */
    if ((entry->next= hash->root))
      entry->next->prev= &entry->next;
    entry->prev= &hash->root;
    hash->root= entry;
    if (my_hash_insert(&hash->hash, (uchar*) entry))
    {
      /* This can only happen if hash got out of memory */
      my_free(entry);
      error= 1;
      goto end;
    }
  }

end:
  mysql_rwlock_unlock(&hash->lock);
  DBUG_RETURN(error);
}


/*
  Change all entres with one data value to another data value

  SYONOPSIS
    safe_hash_change()
    hash			Hash handle
    old_data			Old data
    new_data			Change all 'old_data' to this

  NOTES
    We use the linked list to traverse all elements in the hash as
    this allows us to delete elements in the case where 'new_data' is the
    default value.
*/

static void safe_hash_change(SAFE_HASH *hash, uchar *old_data, uchar *new_data)
{
  SAFE_HASH_ENTRY *entry, *next;
  DBUG_ENTER("safe_hash_set");

  mysql_rwlock_wrlock(&hash->lock);

  for (entry= hash->root ; entry ; entry= next)
  {
    next= entry->next;
    if (entry->data == old_data)
    {
      if (new_data == hash->default_value)
      {
        if ((*entry->prev= entry->next))
          entry->next->prev= entry->prev;
	my_hash_delete(&hash->hash, (uchar*) entry);
      }
      else
	entry->data= new_data;
    }
  }

  mysql_rwlock_unlock(&hash->lock);
  DBUG_VOID_RETURN;
}


/*****************************************************************************
  Functions to handle the key cache objects
*****************************************************************************/

/* Variable to store all key cache objects */
static SAFE_HASH key_cache_hash;


bool multi_keycache_init(void)
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

  NOTES
    This function is coded in such a way that we will return the
    default key cache even if one never called multi_keycache_init.
    This will ensure that it works with old MyISAM clients.

  RETURN
    key cache to use
*/

KEY_CACHE *multi_key_cache_search(uchar *key, uint length)
{
  if (!key_cache_hash.hash.records)
    return dflt_key_cache;
  return (KEY_CACHE*) safe_hash_search(&key_cache_hash, key, length);
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


bool multi_key_cache_set(const uchar *key, uint length,
                         KEY_CACHE *key_cache)
{
  return safe_hash_set(&key_cache_hash, key, length, (uchar*) key_cache);
}


void multi_key_cache_change(KEY_CACHE *old_data,
			    KEY_CACHE *new_data)
{
  safe_hash_change(&key_cache_hash, (uchar*) old_data, (uchar*) new_data);
}
