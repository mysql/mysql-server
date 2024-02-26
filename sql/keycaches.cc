/* Copyright (c) 2002, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql/keycaches.h"

#include "m_string.h"
#include "my_dbug.h"
#include "mysys/mysys_priv.h"
#include "template_utils.h"

/****************************************************************************
  Named list handling
****************************************************************************/

NAMED_ILIST key_caches;

static uchar *find_named(I_List<NAMED_ILINK> *list, std::string_view name,
                         NAMED_ILINK **found) {
  I_List_iterator<NAMED_ILINK> it(*list);
  NAMED_ILINK *element;
  while ((element = it++)) {
    if (element->cmp(name.data(), name.size())) {
      if (found) *found = element;
      return element->data;
    }
  }
  return nullptr;
}

void NAMED_ILIST::delete_elements() {
  NAMED_ILINK *element;
  DBUG_TRACE;
  while ((element = get())) {
    end_key_cache(pointer_cast<KEY_CACHE *>(element->data),
                  true);  // Can never fail
    my_free(element->data);
    delete element;
  }
}

/* Key cache functions */

const LEX_CSTRING default_key_cache_base = {STRING_WITH_LEN("default")};

KEY_CACHE
zero_key_cache;  ///< @@nonexistent_cache.param->value_ptr() points here

KEY_CACHE *get_key_cache(std::string_view cache_name) {
  std::string_view name = cache_name.empty()
                              ? std::string_view{default_key_cache_base.str,
                                                 default_key_cache_base.length}
                              : cache_name;
  return pointer_cast<KEY_CACHE *>(find_named(&key_caches, name, nullptr));
}

KEY_CACHE *create_key_cache(std::string_view name) {
  KEY_CACHE *key_cache;
  DBUG_TRACE;
  DBUG_PRINT("enter",
             ("name: %.*s", static_cast<int>(name.size()), name.data()));

  if ((key_cache =
           (KEY_CACHE *)my_malloc(key_memory_KEY_CACHE, sizeof(KEY_CACHE),
                                  MYF(MY_ZEROFILL | MY_WME)))) {
    if (!new NAMED_ILINK(&key_caches, name, (uchar *)key_cache)) {
      my_free(key_cache);
      key_cache = nullptr;
    } else {
      /*
        Set default values for a key cache
        The values in dflt_key_cache_var is set by my_getopt() at startup

        We don't set 'buff_size' as this is used to enable the key cache
      */
      key_cache->param_block_size = dflt_key_cache_var.param_block_size;
      key_cache->param_division_limit = dflt_key_cache_var.param_division_limit;
      key_cache->param_age_threshold = dflt_key_cache_var.param_age_threshold;
    }
  }
  return key_cache;
}

KEY_CACHE *get_or_create_key_cache(std::string_view name) {
  KEY_CACHE *key_cache;

  if (!(key_cache = get_key_cache(name))) key_cache = create_key_cache(name);
  return key_cache;
}

bool process_key_caches(process_key_cache_t func) {
  I_List_iterator<NAMED_ILINK> it(key_caches);
  NAMED_ILINK *element;

  while ((element = it++)) {
    KEY_CACHE *key_cache = (KEY_CACHE *)element->data;
    func(element->name, key_cache);
  }
  return false;
}
