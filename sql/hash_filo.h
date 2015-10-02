/* Copyright (c) 2000, 2015, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */


/*
** A class for static sized hash tables where old entries are deleted in
** first-in-last-out to usage.
*/

#ifndef  HASH_FILO_H
#define  HASH_FILO_H

#include "hash.h"        /* my_hash_get_key, my_hash_free_key, HASH */
#include "mysqld.h"      /* key_hash_filo_lock */

struct hash_filo_element
{
private:
  hash_filo_element *next_used,*prev_used;
public:
  hash_filo_element() {}
  hash_filo_element *next()
  { return next_used; }
  hash_filo_element *prev()
  { return prev_used; }
  
  friend class hash_filo;
};


class hash_filo
{
private:
  PSI_memory_key m_psi_key;
  const uint key_offset, key_length;
  const my_hash_get_key get_key;
  /** Size of this hash table. */
  uint m_size;
  my_hash_free_key free_element;
  CHARSET_INFO *hash_charset;

  hash_filo_element *first_link,*last_link;
public:
  mysql_mutex_t lock;
  HASH cache;

  hash_filo(PSI_memory_key psi_key,
            uint size, uint key_offset_arg , uint key_length_arg,
	    my_hash_get_key get_key_arg, my_hash_free_key free_element_arg,
	    CHARSET_INFO *hash_charset_arg)
    : m_psi_key(psi_key),
    key_offset(key_offset_arg), key_length(key_length_arg),
    get_key(get_key_arg), m_size(size),
    free_element(free_element_arg),
    hash_charset(hash_charset_arg),
    first_link(NULL),
    last_link(NULL)
  {
    memset(&cache, 0, sizeof(cache));
    mysql_mutex_init(key_hash_filo_lock, &lock, MY_MUTEX_INIT_FAST);
  }

  ~hash_filo()
  {
    if (cache.array.buffer)	/* Avoid problems with thread library */
      my_hash_free(&cache);
    mysql_mutex_destroy(&lock);
  }
  void clear(bool locked=0)
  {
    if (!locked)
      mysql_mutex_lock(&lock);
    first_link= NULL;
    last_link= NULL;
    my_hash_free(&cache);
    (void) my_hash_init(&cache, hash_charset, m_size, key_offset,
                        key_length, get_key, free_element, 0, m_psi_key);
    if (!locked)
      mysql_mutex_unlock(&lock);
  }

  hash_filo_element *first()
  {
    mysql_mutex_assert_owner(&lock);
    return first_link;
  }

  hash_filo_element *last()
  {
    mysql_mutex_assert_owner(&lock);
    return last_link;
  }

  hash_filo_element *search(uchar* key, size_t length)
  {
    mysql_mutex_assert_owner(&lock);

    hash_filo_element *entry=(hash_filo_element*)
      my_hash_search(&cache, key, length);
    if (entry)
    {						// Found; link it first
      DBUG_ASSERT(first_link != NULL);
      DBUG_ASSERT(last_link != NULL);
      if (entry != first_link)
      {						// Relink used-chain
	if (entry == last_link)
        {
	  last_link= last_link->prev_used;
          /*
            The list must have at least 2 elements,
            otherwise entry would be equal to first_link.
          */
          DBUG_ASSERT(last_link != NULL);
          last_link->next_used= NULL;
        }
	else
	{
          DBUG_ASSERT(entry->next_used != NULL);
          DBUG_ASSERT(entry->prev_used != NULL);
	  entry->next_used->prev_used = entry->prev_used;
	  entry->prev_used->next_used = entry->next_used;
	}
        entry->prev_used= NULL;
        entry->next_used= first_link;

        first_link->prev_used= entry;
        first_link=entry;
      }
    }
    return entry;
  }

  my_bool add(hash_filo_element *entry)
  {
    if (!m_size) return 1;
    if (cache.records == m_size)
    {
      hash_filo_element *tmp=last_link;
      last_link= last_link->prev_used;
      if (last_link != NULL)
      {
        last_link->next_used= NULL;
      }
      else
      {
        /* Pathological case, m_size == 1 */
        first_link= NULL;
      }
      my_hash_delete(&cache,(uchar*) tmp);
    }
    if (my_hash_insert(&cache,(uchar*) entry))
    {
      if (free_element)
	(*free_element)(entry);		// This should never happen
      return 1;
    }
    entry->prev_used= NULL;
    entry->next_used= first_link;
    if (first_link != NULL)
      first_link->prev_used= entry;
    else
      last_link= entry;
    first_link= entry;

    return 0;
  }

  uint size()
  { return m_size; }

  void resize(uint new_size)
  {
    mysql_mutex_lock(&lock);
    m_size= new_size;
    clear(true);
    mysql_mutex_unlock(&lock);
  }
};

#endif
