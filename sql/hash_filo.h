/* Copyright (C) 2000-2003, 2005 MySQL AB

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
** A class for static sized hash tables where old entries are deleted in
** first-in-last-out to usage.
*/

#ifndef  HASH_FILO_H
#define  HASH_FILO_H

#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class interface */
#endif

class hash_filo_element
{
  hash_filo_element *next_used,*prev_used;
 public:
  hash_filo_element() {}
  friend class hash_filo;
};


class hash_filo
{
  const uint size, key_offset, key_length;
  const hash_get_key get_key;
  hash_free_key free_element;
  bool init;
  CHARSET_INFO *hash_charset;

  hash_filo_element *first_link,*last_link;
public:
  pthread_mutex_t lock;
  HASH cache;

  hash_filo(uint size_arg, uint key_offset_arg , uint key_length_arg,
	    hash_get_key get_key_arg, hash_free_key free_element_arg,
	    CHARSET_INFO *hash_charset_arg)
    :size(size_arg), key_offset(key_offset_arg), key_length(key_length_arg),
    get_key(get_key_arg), free_element(free_element_arg),init(0),
    hash_charset(hash_charset_arg)
  {
    bzero((char*) &cache,sizeof(cache));
  }

  ~hash_filo()
  {
    if (init)
    {
      if (cache.array.buffer)	/* Avoid problems with thread library */
	(void) hash_free(&cache);
      pthread_mutex_destroy(&lock);
    }
  }
  void clear(bool locked=0)
  {
    if (!init)
    {
      init=1;
      (void) pthread_mutex_init(&lock,MY_MUTEX_INIT_FAST);
    }
    if (!locked)
      (void) pthread_mutex_lock(&lock);
    (void) hash_free(&cache);
    (void) hash_init(&cache,hash_charset,size,key_offset, 
    		     key_length, get_key, free_element,0);
    if (!locked)
      (void) pthread_mutex_unlock(&lock);
    first_link=last_link=0;
  }

  hash_filo_element *search(uchar* key, size_t length)
  {
    hash_filo_element *entry=(hash_filo_element*)
      hash_search(&cache,(uchar*) key,length);
    if (entry)
    {						// Found; link it first
      if (entry != first_link)
      {						// Relink used-chain
	if (entry == last_link)
	  last_link=entry->prev_used;
	else
	{
	  entry->next_used->prev_used = entry->prev_used;
	  entry->prev_used->next_used = entry->next_used;
	}
	if ((entry->next_used= first_link))
	  first_link->prev_used=entry;
	first_link=entry;
      }
    }
    return entry;
  }

  my_bool add(hash_filo_element *entry)
  {
    if (cache.records == size)
    {
      hash_filo_element *tmp=last_link;
      last_link=last_link->prev_used;
      hash_delete(&cache,(uchar*) tmp);
    }
    if (my_hash_insert(&cache,(uchar*) entry))
    {
      if (free_element)
	(*free_element)(entry);		// This should never happen
      return 1;
    }
    if ((entry->next_used=first_link))
      first_link->prev_used=entry;
    else
      last_link=entry;
    first_link=entry;
    return 0;
  }
};

#endif
