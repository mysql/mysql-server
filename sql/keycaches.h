/* Copyright (C) 2002-2006 MySQL AB, 2009 Sun Microsystems, Inc.

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

#include "sql_list.h"
#include <keycache.h>

extern "C"
{
  typedef int (*process_key_cache_t) (const char *, KEY_CACHE *);
}

class NAMED_ILINK;

class NAMED_ILIST: public I_List<NAMED_ILINK>
{
  public:
  void delete_elements(void (*free_element)(const char*, uchar*));
};

extern LEX_STRING default_key_cache_base;
extern KEY_CACHE zero_key_cache;
extern NAMED_ILIST key_caches;

KEY_CACHE *create_key_cache(const char *name, uint length);
KEY_CACHE *get_key_cache(LEX_STRING *cache_name);
KEY_CACHE *get_or_create_key_cache(const char *name, uint length);
void free_key_cache(const char *name, KEY_CACHE *key_cache);
bool process_key_caches(process_key_cache_t func);

