/*
  Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTER_SRC_REST_MRS_SRC_FIXED_CONNECTION_POOL_MANAGER_H_
#define ROUTER_SRC_REST_MRS_SRC_FIXED_CONNECTION_POOL_MANAGER_H_

#include <cassert>

#include "collector/cache_manager.h"
#include "collector/counted_mysql_session.h"
#include "collector/destination_provider.h"
#include "mrs/configuration.h"

#include "secure_string.h"  // NOLINT(build/include_subdir)

namespace collector {

class MysqlFixedPoolManager {
 public:
  using MySQLSession = CountedMySQLSession;
  using ConnectionParameters = MySQLSession::ConnectionParameters;
  using MySqlFixedCacheManagerImpl = CacheManager<CountedMySQLSession *>;
  using CachedObject = MySqlFixedCacheManagerImpl::CachedObject;
  using Callbacks = MySqlFixedCacheManagerImpl::Callbacks;
  using Object = MySqlFixedCacheManagerImpl::Object;

  class MysqlCacheCallbacks : public Callbacks {
   public:
    bool object_before_cache(Object, bool dirty) override;
    bool object_retrived_from_cache(Object) override;
    void object_remove(Object) override;
    Object object_allocate(bool wait) override;
  };

 public:
  explicit MysqlFixedPoolManager(uint32_t passthrough_pool_size)
      : num_instances_{passthrough_pool_size},
        callbacks_{},
        cache_manager_{&callbacks_, num_instances_} {}

  virtual ~MysqlFixedPoolManager() = default;

  virtual CachedObject get_instance() {
    return cache_manager_.get_instance(false);
  }

  void init(DestinationProvider *destination, const std::string &user,
            const mysql_harness::SecureString &password);

  virtual void return_instance(CachedObject &object) {
    if (object.parent_) object.parent_->return_instance(object);
  }

 private:
  uint32_t num_instances_;
  MysqlCacheCallbacks callbacks_;
  MySqlFixedCacheManagerImpl cache_manager_;
};

class db_pool_exhausted : public std::exception {};

}  // namespace collector

#endif  // ROUTER_SRC_REST_MRS_SRC_FIXED_CONNECTION_POOL_MANAGER_H_
