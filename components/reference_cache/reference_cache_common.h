/* Copyright (c) 2020, 2022, Oracle and/or its affiliates.

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

#ifndef REFERENCE_CACHE_COMMON_H
#define REFERENCE_CACHE_COMMON_H

#include <set>
#include <string>

namespace reference_caching {

extern PSI_memory_key KEY_mem_reference_cache;
#define PSI_category "refcache"

template <class Key = std::string, class Less = std::less<>>
class service_names_set
    : public std::set<Key, Less, Component_malloc_allocator<Key>> {
 public:
  service_names_set()
      : std::set<Key, Less, Component_malloc_allocator<Key>>(
            Component_malloc_allocator<>(KEY_mem_reference_cache)) {}
};

}  // namespace reference_caching

#endif /* REFERENCE_CACHE_COMMON_H */
