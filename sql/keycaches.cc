/* Copyright (c) 2002, 2010, Oracle and/or its affiliates. All rights reserved.

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

#include "keycaches.h"

/****************************************************************************
  Named list handling
****************************************************************************/

NAMED_ILIST key_caches;

/**
  ilink (intrusive list element) with a name
*/
class NAMED_ILINK :public ilink
{
public:
  const char *name;
  uint name_length;
  uchar* data;

  NAMED_ILINK(I_List<NAMED_ILINK> *links, const char *name_arg,
             uint name_length_arg, uchar* data_arg)
    :name_length(name_length_arg), data(data_arg)
  {
    name= my_strndup(name_arg, name_length, MYF(MY_WME));
    links->push_back(this);
  }
  inline bool cmp(const char *name_cmp, uint length)
  {
    return length == name_length && !memcmp(name, name_cmp, length);
  }
  ~NAMED_ILINK()
  {
    my_free((void *) name);
  }
};

uchar* find_named(I_List<NAMED_ILINK> *list, const char *name, uint length,
                NAMED_ILINK **found)
{
  I_List_iterator<NAMED_ILINK> it(*list);
  NAMED_ILINK *element;
  while ((element= it++))
  {
    if (element->cmp(name, length))
    {
      if (found)
        *found= element;
      return element->data;
    }
  }
  return 0;
}


void NAMED_ILIST::delete_elements(void (*free_element)(const char *name, uchar*))
{
  NAMED_ILINK *element;
  DBUG_ENTER("NAMED_ILIST::delete_elements");
  while ((element= get()))
  {
    (*free_element)(element->name, element->data);
    delete element;
  }
  DBUG_VOID_RETURN;
}


/* Key cache functions */

LEX_STRING default_key_cache_base= {C_STRING_WITH_LEN("default")};

KEY_CACHE zero_key_cache; ///< @@nonexistent_cache.param->value_ptr() points here

KEY_CACHE *get_key_cache(LEX_STRING *cache_name)
{
  if (!cache_name || ! cache_name->length)
    cache_name= &default_key_cache_base;
  return ((KEY_CACHE*) find_named(&key_caches,
                                  cache_name->str, cache_name->length, 0));
}

KEY_CACHE *create_key_cache(const char *name, uint length)
{
  KEY_CACHE *key_cache;
  DBUG_ENTER("create_key_cache");
  DBUG_PRINT("enter",("name: %.*s", length, name));
  
  if ((key_cache= (KEY_CACHE*) my_malloc(sizeof(KEY_CACHE),
                                             MYF(MY_ZEROFILL | MY_WME))))
  {
    if (!new NAMED_ILINK(&key_caches, name, length, (uchar*) key_cache))
    {
      my_free(key_cache);
      key_cache= 0;
    }
    else
    {
      /*
        Set default values for a key cache
        The values in dflt_key_cache_var is set by my_getopt() at startup

        We don't set 'buff_size' as this is used to enable the key cache
      */
      key_cache->param_block_size=     dflt_key_cache_var.param_block_size;
      key_cache->param_division_limit= dflt_key_cache_var.param_division_limit;
      key_cache->param_age_threshold=  dflt_key_cache_var.param_age_threshold;
    }
  }
  DBUG_RETURN(key_cache);
}


KEY_CACHE *get_or_create_key_cache(const char *name, uint length)
{
  LEX_STRING key_cache_name;
  KEY_CACHE *key_cache;

  key_cache_name.str= (char *) name;
  key_cache_name.length= length;
  if (!(key_cache= get_key_cache(&key_cache_name)))
    key_cache= create_key_cache(name, length);
  return key_cache;
}


void free_key_cache(const char *name, KEY_CACHE *key_cache)
{
  end_key_cache(key_cache, 1);		// Can never fail
  my_free(key_cache);
}


bool process_key_caches(process_key_cache_t func)
{
  I_List_iterator<NAMED_ILINK> it(key_caches);
  NAMED_ILINK *element;

  while ((element= it++))
  {
    KEY_CACHE *key_cache= (KEY_CACHE *) element->data;
    func(element->name, key_cache);
  }
  return 0;
}

#ifdef HAVE_EXPLICIT_TEMPLATE_INSTANTIATION
template class I_List_iterator<NAMED_ILINK>;
#endif

