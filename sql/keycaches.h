#ifndef KEYCACHES_INCLUDED
#define KEYCACHES_INCLUDED

/* Copyright (c) 2002, 2022, Oracle and/or its affiliates.

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

#include <string_view>

#include "keycache.h"
#include "lex_string.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysql/service_mysql_alloc.h"
#include "sql/sql_list.h"
#include "sql/thr_malloc.h"

typedef int (*process_key_cache_t)(std::string_view, KEY_CACHE *);

/**
  ilink (intrusive list element) with a name
*/
class NAMED_ILINK : public ilink<NAMED_ILINK> {
 public:
  const std::string name;  ///< case-sensitive, system character set
  uchar *data;

  NAMED_ILINK(I_List<NAMED_ILINK> *links, const std::string_view &name_arg,
              uchar *data_arg)
      : name(name_arg), data(data_arg) {
    links->push_back(this);
  }

  bool cmp(const char *name_cmp, size_t length) {
    return length == name.size() && !memcmp(name.data(), name_cmp, length);
  }
};

class NAMED_ILIST : public I_List<NAMED_ILINK> {
 public:
  void delete_elements();
};

extern const LEX_CSTRING default_key_cache_base;
extern KEY_CACHE zero_key_cache;
extern NAMED_ILIST key_caches;

/**
  Create a MyISAM Multiple Key Cache

  @param name   Cache name (case insensitive, system character set).
*/
KEY_CACHE *create_key_cache(std::string_view name);
/**
  Resolve a MyISAM Multiple Key Cache by name.

  @param cache_name   Cache name (case insensitive, system character set).

  @returns      New key cache on success, otherwise nullptr.
*/
KEY_CACHE *get_key_cache(std::string_view cache_name);
/**
  Resolve an existent MyISAM Multiple Key Cache by name, otherwise create a
  new one.

  @param name   Cache name (case insensitive, system character set)

  @returns      Key cache on success, otherwise nullptr.
*/
KEY_CACHE *get_or_create_key_cache(std::string_view name);
bool process_key_caches(process_key_cache_t func);

#endif /* KEYCACHES_INCLUDED */
