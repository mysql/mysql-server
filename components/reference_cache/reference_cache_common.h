/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2.0,
as published by the Free Software Foundation.

This program is designed to work with certain software (including
but not limited to OpenSSL) that is licensed under separate terms,
as designated in a particular file or component or in included license
documentation.  The authors of MySQL hereby grant you an additional
permission to link the program and your derivative works with the
separately licensed software that they have either included with
the program or referenced in the documentation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef REFERENCE_CACHE_COMMON_H
#define REFERENCE_CACHE_COMMON_H

#include <atomic>
#include <set>
#include <string>

namespace reference_caching {

extern PSI_memory_key KEY_mem_reference_cache;
#define PSI_category "refcache"

struct Service_name_entry {
  explicit Service_name_entry(const char *name, unsigned int count)
      : name_{name}, count_{count} {}
  Service_name_entry(const Service_name_entry &src)
      : Service_name_entry(src.name_.c_str(), src.count_.load()) {}
  Service_name_entry &operator=(const Service_name_entry &rhs) {
    name_ = rhs.name_;
    count_ = rhs.count_.load();
    return *this;
  }
  std::string name_;
  mutable std::atomic<unsigned int> count_{0};
};

struct Compare_service_name_entry {
  bool operator()(const Service_name_entry &lhs,
                  const Service_name_entry &rhs) const {
    return lhs.name_ < rhs.name_;
  }
};

template <class Key = Service_name_entry,
          class Less = Compare_service_name_entry>
class service_names_set
    : public std::set<Key, Less, Component_malloc_allocator<Key>> {
 public:
  service_names_set()
      : std::set<Key, Less, Component_malloc_allocator<Key>>(
            Component_malloc_allocator<>(KEY_mem_reference_cache)) {}
};

}  // namespace reference_caching

#endif /* REFERENCE_CACHE_COMMON_H */
