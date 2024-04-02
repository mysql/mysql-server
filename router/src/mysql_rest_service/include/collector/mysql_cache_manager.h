/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#include <cassert>

#include "collector/cache_manager.h"
#include "mrs/configuration.h"

#include "collector/counted_mysql_session.h"
#include "collector/destination_provider.h"

namespace collector {

enum MySQLConnection {
  kMySQLConnectionMetadataRO,
  kMySQLConnectionUserdataRO,
  kMySQLConnectionMetadataRW,
  kMySQLConnectionUserdataRW
};

class ConnectionConfiguration {
 public:
  ConnectionConfiguration() = default;

  ConnectionConfiguration(MySQLConnection type,
                          const mrs::Configuration &configuration)
      : type_{type},
        provider_{is_rw() ? configuration.provider_rw_.get()
                          : configuration.provider_ro_.get()} {
    switch (type) {
      case kMySQLConnectionMetadataRW:
      case kMySQLConnectionMetadataRO:
        mysql_user_ = configuration.mysql_user_;
        mysql_password_ = configuration.mysql_user_password_;
        break;
      case kMySQLConnectionUserdataRW:
      case kMySQLConnectionUserdataRO:
        mysql_user_ = configuration.mysql_user_data_access_;
        mysql_password_ = configuration.mysql_user_data_access_password_;
        break;
    }
  }

  bool is_rw() const {
    switch (type_) {
      case kMySQLConnectionMetadataRW:
      case kMySQLConnectionUserdataRW:
        return true;
      default:
        return false;
    }
  }

  MySQLConnection type_{kMySQLConnectionMetadataRO};
  DestinationProvider *provider_{nullptr};
  std::string mysql_user_;
  std::string mysql_password_;
};

class MysqlCacheManager {
 public:
  using MySQLSession = CountedMySQLSession;
  using ConnectionParameters = MySQLSession::ConnectionParameters;
  using MySqlCacheManager = CacheManager<CountedMySQLSession *>;
  using CachedObject = MySqlCacheManager::CachedObject;
  using Callbacks = MySqlCacheManager::Callbacks;
  using Object = MySqlCacheManager::Object;

  class MysqlCacheCallbacks : public Callbacks {
   public:
    MysqlCacheCallbacks(const ConnectionConfiguration &configuration =
                            ConnectionConfiguration{},
                        const std::string &role = {})
        : connection_configuration_{configuration}, role_{role} {}

    bool object_before_cache(Object, bool dirty) override;
    bool object_retrived_from_cache(Object) override;
    void object_remove(Object) override;
    Object object_allocate(bool wait) override;
    bool is_default_user(Object &) const;

    const ConnectionConfiguration &get_connection_configuration() const;

   private:
    void object_restore_defaults(Object &, bool dirty);
    bool is_default_server(Object &) const;

    ConnectionParameters new_connection_params(bool wait);

   private:
    ConnectionConfiguration connection_configuration_;
    std::string role_;
    int node_rount_robin_{0};
  };

 public:
  MysqlCacheManager(const mrs::Configuration &configuration)
      : default_mysql_cache_instances_{configuration
                                           .default_mysql_cache_instances_},
        callbacks_metadata_ro_{
            {collector::kMySQLConnectionMetadataRO, configuration},
            "mysql_rest_service_meta_provider"},
        callbacks_userdata_ro_{{
                                   collector::kMySQLConnectionUserdataRO,
                                   configuration,
                               },
                               "mysql_rest_service_data_provider"},
        callbacks_metadata_rw_{
            {collector::kMySQLConnectionMetadataRW, configuration},
            "mysql_rest_service_meta_provider"},
        callbacks_userdata_rw_{{
                                   collector::kMySQLConnectionUserdataRW,
                                   configuration,
                               },
                               "mysql_rest_service_data_provider"} {}
  MysqlCacheManager(Callbacks *callbacks_meta, Callbacks *callbacks_user)
      : default_mysql_cache_instances_{10},
        cache_manager_metadata_ro_{callbacks_meta},
        cache_manager_userdata_ro_{callbacks_user} {}

  virtual ~MysqlCacheManager() = default;

  virtual CachedObject get_empty(collector::MySQLConnection type, bool wait) {
    switch (type) {
      case collector::kMySQLConnectionMetadataRO:
        return CachedObject(&cache_manager_metadata_ro_, wait);
      case collector::kMySQLConnectionUserdataRO:
        return CachedObject(&cache_manager_userdata_ro_, wait);
      case collector::kMySQLConnectionMetadataRW:
        return CachedObject(&cache_manager_metadata_rw_, wait);
      case collector::kMySQLConnectionUserdataRW:
        return CachedObject(&cache_manager_userdata_rw_, wait);
      default:
        assert(nullptr && "Shouldn't happen");
        return {};
    }
  }

  virtual collector::MySQLConnection get_type(const CachedObject &obj) {
    if (obj.parent_ == &cache_manager_metadata_ro_)
      return collector::kMySQLConnectionMetadataRO;
    else if (obj.parent_ == &cache_manager_metadata_ro_)
      return collector::kMySQLConnectionMetadataRW;
    else if (obj.parent_ == &cache_manager_metadata_rw_)
      return collector::kMySQLConnectionUserdataRO;
    else if (obj.parent_ == &cache_manager_userdata_rw_)
      return collector::kMySQLConnectionUserdataRW;

    return collector::kMySQLConnectionUserdataRO;
  }

  virtual CachedObject get_instance(collector::MySQLConnection type,
                                    bool wait) {
    switch (type) {
      case collector::kMySQLConnectionMetadataRO:
        return cache_manager_metadata_ro_.get_instance(wait);
      case collector::kMySQLConnectionUserdataRO:
        return cache_manager_userdata_ro_.get_instance(wait);
      case collector::kMySQLConnectionMetadataRW:
        return cache_manager_metadata_rw_.get_instance(wait);
      case collector::kMySQLConnectionUserdataRW:
        return cache_manager_userdata_rw_.get_instance(wait);
      default:
        assert(nullptr && "Shouldn't happen");
        return {};
    }
  }

  virtual void change_instance(CachedObject &instance,
                               collector::MySQLConnection type) {
    switch (type) {
      case collector::kMySQLConnectionMetadataRO:
        change_to(instance, &cache_manager_metadata_ro_);
        break;
      case collector::kMySQLConnectionUserdataRO:
        change_to(instance, &cache_manager_userdata_ro_);
        break;
      case collector::kMySQLConnectionMetadataRW:
        change_to(instance, &cache_manager_metadata_rw_);
        break;
      case collector::kMySQLConnectionUserdataRW:
        change_to(instance, &cache_manager_userdata_rw_);
        break;
    }
  }

  virtual void return_instance(CachedObject &object) {
    if (object.parent_) object.parent_->return_instance(object);
  }

  virtual void change_cache_object_limit(uint32_t limit) {
    cache_manager_metadata_ro_.change_cache_object_limit(limit);
    cache_manager_userdata_ro_.change_cache_object_limit(limit);
    cache_manager_metadata_rw_.change_cache_object_limit(limit);
    cache_manager_userdata_rw_.change_cache_object_limit(limit);
  }

  void configure(const std::string &json_object);

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

  uint32_t default_mysql_cache_instances_;
  MysqlCacheCallbacks callbacks_metadata_ro_;
  MysqlCacheCallbacks callbacks_userdata_ro_;
  MysqlCacheCallbacks callbacks_metadata_rw_;
  MysqlCacheCallbacks callbacks_userdata_rw_;
  MySqlCacheManager cache_manager_metadata_ro_{&callbacks_metadata_ro_,
                                               default_mysql_cache_instances_};
  MySqlCacheManager cache_manager_userdata_ro_{&callbacks_userdata_ro_,
                                               default_mysql_cache_instances_};
  MySqlCacheManager cache_manager_metadata_rw_{&callbacks_metadata_rw_,
                                               default_mysql_cache_instances_};
  MySqlCacheManager cache_manager_userdata_rw_{&callbacks_userdata_rw_,
                                               default_mysql_cache_instances_};
};

}  // namespace collector

#endif  // ROUTER_SRC_REST_MRS_SRC_CONNECTION_CACHE_MANAGER_H_
