/*
  Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTER_SRC_REST_MRS_SRC_CONNECTION_CACHE_MANAGER_H_
#define ROUTER_SRC_REST_MRS_SRC_CONNECTION_CACHE_MANAGER_H_

#include "mysqlrouter/mysql_session.h"

#include "collector/cache_manager.h"
#include "mrs/configuration.h"

namespace collector {

enum MySQLConnection { kMySQLConnectionMetadata, kMySQLConnectionUserdata };

class ConnectionConfiguration {
 public:
  ConnectionConfiguration() = default;

  ConnectionConfiguration(MySQLConnection type,
                          const mrs::Configuration &configuration)
      : ssl_{configuration.ssl_}, nodes_{configuration.nodes_} {
    switch (type) {
      case kMySQLConnectionMetadata:
        mysql_user_ = configuration.mysql_user_;
        mysql_password_ = configuration.mysql_user_password_;
        break;
      case kMySQLConnectionUserdata:
        mysql_user_ = configuration.mysql_user_data_access_;
        mysql_password_ = configuration.mysql_user_data_access_password_;
        break;
    }
  }

  std::string mysql_user_;
  std::string mysql_password_;
  mrs::SslConfiguration ssl_;
  std::vector<mrs::Node> nodes_;
};

class MysqlCacheManager {
 public:
  using ConnectionParameters = mysqlrouter::MySQLSession::ConnectionParameters;
  using MySQLSession = mysqlrouter::MySQLSession;
  using MySqlCacheManager = CacheManager<::mysqlrouter::MySQLSession *>;
  using CachedObject = MySqlCacheManager::CachedObject;
  using Callbacks = MySqlCacheManager::Callbacks;
  using Object = MySqlCacheManager::Object;

  class MysqlCacheCallbacks : public Callbacks {
   public:
    MysqlCacheCallbacks(const ConnectionConfiguration &configuration =
                            ConnectionConfiguration{})
        : configuration_{configuration} {}

    bool object_before_cache(Object) override;
    void object_retrived_from_cache(Object) override;
    void object_remove(Object) override;
    Object object_allocate() override;
    bool is_default_user(Object &) const;

    const ConnectionConfiguration &get_connection_configuration() const;

   private:
    void object_restore_defaults(Object &);
    bool is_default_server(Object &) const;

    ConnectionParameters new_connection_params();

   private:
    ConnectionConfiguration configuration_;
    int node_rount_robin_{0};
  };

 public:
  MysqlCacheManager(const mrs::Configuration &configuration)
      : callbacks_metadata_{{collector::kMySQLConnectionMetadata,
                             configuration}},
        callbacks_userdata_{
            {collector::kMySQLConnectionUserdata, configuration}} {}
  MysqlCacheManager(Callbacks *callbacks_meta, Callbacks *callbacks_user)
      : cache_manager_metadata_{callbacks_meta},
        cache_manager_userdata_{callbacks_user} {}

  virtual ~MysqlCacheManager() = default;

  virtual CachedObject get_empty(collector::MySQLConnection type) {
    switch (type) {
      case collector::kMySQLConnectionMetadata:
        return {&cache_manager_metadata_};
      case collector::kMySQLConnectionUserdata:
        return {&cache_manager_userdata_};
    }
  }
  virtual CachedObject get_instance(collector::MySQLConnection type) {
    switch (type) {
      case collector::kMySQLConnectionMetadata:
        return cache_manager_metadata_.get_instance();
      case collector::kMySQLConnectionUserdata:
        return cache_manager_userdata_.get_instance();
    }
  }

  virtual void change_instance(CachedObject &instance,
                               collector::MySQLConnection type) {
    switch (type) {
      case collector::kMySQLConnectionMetadata:
        change_to(instance, &cache_manager_metadata_);
        break;
      case collector::kMySQLConnectionUserdata:
        change_to(instance, &cache_manager_userdata_);
        break;
    }
  }

  virtual void return_instance(CachedObject &object) {
    if (object.parent_) object.parent_->return_instance(object);
  }

  virtual void change_cache_object_limit(uint32_t limit) {
    cache_manager_metadata_.change_cache_object_limit(limit);
    cache_manager_userdata_.change_cache_object_limit(limit);
  }

 private:
  static void change_to(CachedObject &instance, MySqlCacheManager *m) {
    if (instance.parent_ != m) {
      instance.parent_ = m;
      if (instance.get()) {
        auto cb = dynamic_cast<MysqlCacheCallbacks *>(m->get_callbacks());

        instance.get()->change_user(
            cb->get_connection_configuration().mysql_user_,
            cb->get_connection_configuration().mysql_password_, "");
      }
    }
  }

  MysqlCacheCallbacks callbacks_metadata_;
  MysqlCacheCallbacks callbacks_userdata_;
  MySqlCacheManager cache_manager_metadata_{&callbacks_metadata_};
  MySqlCacheManager cache_manager_userdata_{&callbacks_userdata_};
};

}  // namespace collector

#endif  // ROUTER_SRC_REST_MRS_SRC_CONNECTION_CACHE_MANAGER_H_
