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
#include <atomic>
#include <map>
#include <set>
#include <string>
#include "cache_allocator.h"
#include "reference_cache_common.h"

namespace reference_caching {

class channel_imp : public Cache_malloced {
 public:
  static channel_imp *create(service_names_set<> &service_names);
  static bool destroy(channel_imp *channel);
  static bool factory_init();
  static bool factory_deinit();
  static channel_imp *channel_by_name(std::string service_name);

  bool is_valid() { return m_valid; }
  void set_valid(bool new_value) {
    m_valid.store(new_value, std::memory_order_relaxed);
  }

  service_names_set<> &get_service_names() { return m_service_names; }

  void ignore_list_copy(service_names_set<> &dest_set);
  bool ignore_list_add(std::string service_implementation);
  bool ignore_list_remove(std::string service_implementation);
  bool ignore_list_clear();

  bool is_alone() { return m_reference_count == 1; }
  channel_imp *ref() {
    m_reference_count.fetch_add(1, std::memory_order_relaxed);
    return this;
  }
  int unref() {
    return m_reference_count.fetch_sub(1, std::memory_order_relaxed);
  }
  channel_imp(service_names_set<> &service_names)
      : m_has_ignore_list(false), m_valid{true}, m_reference_count{0} {
    m_service_names = service_names;
  }
  ~channel_imp() = default;

  bool operator==(channel_imp &other) const {
    return m_service_names == other.m_service_names;
  }

 private:
  // disable copy constructors
  channel_imp(const channel_imp &);
  channel_imp &operator=(const channel_imp &);

  service_names_set<> m_service_names;
  service_names_set<> m_ignore_list;
  std::atomic<bool> m_has_ignore_list;
  std::atomic<bool> m_valid;
  std::atomic<int> m_reference_count;
};

}  // namespace reference_caching
