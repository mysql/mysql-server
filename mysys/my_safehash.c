/* Copyright (C) 2003-2007 MySQL AB

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

#include "mysys_priv.h"
#include <m_string.h>
#include "my_safehash.h"

/*****************************************************************************
  General functions to handle SAFE_HASH objects.

  A SAFE_HASH object is used to store the hash, the mutex and default value
  needed by the rest of the key cache code.
  This is a separate struct to make it easy to later reuse the code for other
  purposes

  All entries are linked in a list to allow us to traverse all elements
  and delete selected ones. (HASH doesn't allow any easy ways to do this).
*****************************************************************************/


/*
  Free a SAFE_HASH_ENTRY

  SYNOPSIS
    safe_hash_entry_free()
    entry                The entry which should be freed

  NOTE
    This function is called by the hash object on delete
*/

static void safe_hash_entry_free(SAFE_HASH_ENTRY *entry)
{
  DBUG_ENTER("safe_hash_entry_free");
  my_free(entry);
  DBUG_VOID_RETURN;
}


/*
  Get key and length for a SAFE_HASH_ENTRY

  SYNOPSIS
    safe_hash_entry_get()
    entry                The entry for which the key should be returned
    length               Length of the key

  RETURN
    #  reference on the key
*/

static uchar *safe_hash_entry_get(SAFE_HASH_ENTRY *entry, size_t *length,
                                  my_bool not_used __attribute__((unused)))
{
  *length= entry->length;
  return (uchar*) entry->key;
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
    0  OK
    1  error
*/

my_bool safe_hash_init(SAFE_HASH *hash, uint elements,
                       uchar *default_value)
{
  DBUG_ENTER("safe_hash_init");
  if (my_hash_init(&hash->hash, &my_charset_bin, elements,
                   0, 0, (hash_get_key) safe_hash_entry_get,
                   (void (*)(void*)) safe_hash_entry_free, 0))
  {
    hash->default_value= 0;
    DBUG_RETURN(1);
  }
  my_rwlock_init(&hash->mutex, 0);
  hash->default_value= default_value;
  hash->root= 0;
  DBUG_RETURN(0);
}


/*
  Free a SAFE_HASH object

  SYNOPSIS
    safe_hash_free()
    hash                 Hash handle

  NOTES
    This is safe to call on any object that has been sent to safe_hash_init()
*/

void safe_hash_free(SAFE_HASH *hash)
{
  /*
    Test if safe_hash_init succeeded. This will also guard us against multiple
    free calls.
  */
  if (hash->default_value)
  {
    my_hash_free(&hash->hash);
    rwlock_destroy(&hash->mutex);
    hash->default_value=0;
  }
}


/*
  Return the value stored for a key or default value if no key

  SYNOPSIS
    safe_hash_search()
    hash                 Hash handle
    key                  key (path to table etc..)
    length               Length of key
    def                  Default value of data

  RETURN
    #  data associated with the key of default value if data was not found
*/

uchar *safe_hash_search(SAFE_HASH *hash, const uchar *key, uint length,
                        uchar *def)
{
  uchar *result;
  DBUG_ENTER("safe_hash_search");
  rw_rdlock(&hash->mutex);
  result= my_hash_search(&hash->hash, key, length);
  rw_unlock(&hash->mutex);
  if (!result)
    result= def;
  else
    result= ((SAFE_HASH_ENTRY*) result)->data;
  DBUG_PRINT("exit",("data: 0x%lx", (long) result));
  DBUG_RETURN(result);
}


/*
  Associate a key with some data

  SYNOPSIS
    safe_hash_set()
    hash                 Hash handle
    key                  key (path to table etc..)
    length               Length of key
    data                 data to to associate with the data

  NOTES
    This can be used both to insert a new entry and change an existing
    entry.
    If one associates a key with the default key cache, the key is deleted

  RETURN
    0  OK
    1  error (Can only be EOM). In this case my_message() is called.
*/

my_bool safe_hash_set(SAFE_HASH *hash, const uchar *key, uint length,
                      uchar *data)
{
  SAFE_HASH_ENTRY *entry;
  my_bool error= 0;
  DBUG_ENTER("safe_hash_set");
  DBUG_PRINT("enter",("key: %.*s  data: 0x%lx", length, key, (long) data));

  rw_wrlock(&hash->mutex);
  entry= (SAFE_HASH_ENTRY*) my_hash_search(&hash->hash, key, length);

  if (data == hash->default_value)
  {
    /*
      The key is to be associated with the default entry. In this case
      we can just delete the entry (if it existed) from the hash as a
      search will return the default entry
    */
    if (!entry)          /* nothing to do */
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
    if (!(entry= (SAFE_HASH_ENTRY *) my_malloc(sizeof(*entry) + length,
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
  rw_unlock(&hash->mutex);
  DBUG_RETURN(error);
}


/*
  Change all entries with one data value to another data value

  SYNOPSIS
    safe_hash_change()
    hash                 Hash handle
    old_data             Old data
    new_data             Change all 'old_data' to this

  NOTES
    We use the linked list to traverse all elements in the hash as
    this allows us to delete elements in the case where 'new_data' is the
    default value.
*/

void safe_hash_change(SAFE_HASH *hash, uchar *old_data, uchar *new_data)
{
  SAFE_HASH_ENTRY *entry, *next;
  DBUG_ENTER("safe_hash_change");

  rw_wrlock(&hash->mutex);

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

  rw_unlock(&hash->mutex);
  DBUG_VOID_RETURN;
}
