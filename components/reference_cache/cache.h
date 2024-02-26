/* Copyright (c) 2020, 2023, Oracle and/or its affiliates.

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

#include <mysql/components/my_service.h>
#include <mysql/components/services/registry.h>
#include <set>
#include <string>
#include <unordered_map>
#include "cache_allocator.h"
#include "reference_cache_common.h"

namespace reference_caching {

class channel_imp;

class cache_imp : public Cache_malloced {
 public: /* top level APIs */
  static cache_imp *create(channel_imp *channel,
                           SERVICE_TYPE(registry) * registry);
  static bool destroy(cache_imp *cache);
  bool get(unsigned service_name_index, const my_h_service **ref);
  bool flush();

 public: /* utility */
  cache_imp(channel_imp *channel, SERVICE_TYPE(registry) * registry);
  ~cache_imp();

 private:
  // disable copy constructors
  cache_imp(const cache_imp &);
  cache_imp &operator=(const cache_imp &);

  channel_imp *m_channel;
  /*
    This is a opaque pointer handle used to store the acquired service
    implementation handles.
  */
  my_h_service **m_cache;
  SERVICE_TYPE(registry) * m_registry;
  service_names_set<> m_service_names;
  service_names_set<> m_ignore_list;
};

}  // namespace reference_caching
