/*
  Copyright (c) 2026, Oracle and/or its affiliates.

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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "mrs_component_embedded_mysql_cache.h"

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "mrs_component_embedded_mysql_session.h"
#include "router/src/mysql_rest_service/include/collector/mysql_cache_manager.h"
#include "router/src/mysql_rest_service/include/collector/mysql_fixed_pool_manager.h"
#include "router/src/mysql_rest_service/src/mrs/router_observation_entities.h"

namespace mrs_component {

namespace {

using CountedMySQLSession = collector::CountedMySQLSession;
using Destination = mysql_harness::Destination;
using EmbeddedMysqlSessionCache =
    collector::CacheManager<CountedMySQLSession *>;

// Embedded sessions do not connect to this destination, but shared session
// state still records it for compatibility with logging and cache code
Destination make_destination(const std::string &address) {
  constexpr std::string_view k_local_prefix{"local:"};

  if (address.starts_with(k_local_prefix)) {
    auto destination =
        mysql_harness::LocalDestination(address.substr(k_local_prefix.size()));
    return destination;
  }

  auto destination = mysql_harness::make_tcp_destination(address);
  if (!destination) {
    throw std::runtime_error("Invalid local MySQL destination: " + address);
  }

  return {*destination};
}

CountedMySQLSession::ConnectionParameters build_connection_params(
    const Destination &destination, const std::string &auth_id) {
  CountedMySQLSession::ConnectionParameters params;
  params.conn_opts.destination = destination;
  // EmbeddedMysqlSession interprets username as the security-context auth id
  params.conn_opts.username = auth_id;
  params.conn_opts.extra_client_flags = CLIENT_FOUND_ROWS;
  return params;
}

std::unique_ptr<CountedMySQLSession> make_counted_embedded_mysql_session(
    bool bypass_max_connections = false) {
  return std::make_unique<CountedMySQLSession>(
      make_embedded_mysql_session(bypass_max_connections));
}

CountedMySQLSession *allocate_embedded_mysql_session(
    const Destination &destination, const std::string &internal_auth_id,
    collector::MySQLConnection type, bool bypass_max_connections = false) {
  auto params = build_connection_params(destination, internal_auth_id);
  auto sqls = collector::build_mysql_cache_initial_sqls(type);
  auto session = make_counted_embedded_mysql_session(bypass_max_connections);
  session->connect_and_set_opts(params, sqls);
  mrs::Counter<kEntityCounterMySQLConnectionsCreated>::increment();
  return session.release();
}

class EmbeddedMysqlCacheCallbacks
    : public EmbeddedMysqlSessionCache::Callbacks {
 public:
  using Object = EmbeddedMysqlSessionCache::Object;

  EmbeddedMysqlCacheCallbacks(const Destination &destination,
                              const std::string &internal_auth_id,
                              collector::MySQLConnection type)
      : destination_{destination},
        internal_auth_id_{internal_auth_id},
        type_{type} {}

  bool object_before_cache(Object, bool) override { return false; }
  bool object_retrived_from_cache(Object) override { return false; }
  void object_remove(Object obj) override {
    mrs::Counter<kEntityCounterMySQLConnectionsClosed>::increment();
    delete obj;
  }
  Object object_allocate(bool) override {
    return allocate_embedded_mysql_session(destination_, internal_auth_id_,
                                           type_);
  }

 private:
  const Destination &destination_;
  const std::string &internal_auth_id_;
  collector::MySQLConnection type_;
};

struct EmbeddedMysqlConnectionCache {
  EmbeddedMysqlConnectionCache(const Destination &destination,
                               const std::string &internal_auth_id,
                               collector::MySQLConnection type)
      : callbacks{destination, internal_auth_id, type}, cache{&callbacks, 0} {}

  EmbeddedMysqlCacheCallbacks callbacks;
  EmbeddedMysqlSessionCache cache;
};

class EmbeddedMysqlCacheManager : public collector::MysqlCacheManager {
 public:
  using Object = collector::MysqlCacheManager::Object;
  using CachedObject = collector::MysqlCacheManager::CachedObject;

  EmbeddedMysqlCacheManager(const mrs::Configuration &configuration,
                            Destination destination,
                            std::string metadata_auth_id,
                            std::string data_auth_id)
      : collector::MysqlCacheManager(configuration),
        destination_{std::move(destination)},
        metadata_auth_id_{std::move(metadata_auth_id)},
        data_auth_id_{std::move(data_auth_id)},
        metadata_ro_{destination_, metadata_auth_id_,
                     collector::kMySQLConnectionMetadataRO},
        userdata_ro_{destination_, data_auth_id_,
                     collector::kMySQLConnectionUserdataRO},
        metadata_rw_{destination_, metadata_auth_id_,
                     collector::kMySQLConnectionMetadataRW},
        userdata_rw_{destination_, data_auth_id_,
                     collector::kMySQLConnectionUserdataRW} {}

  CachedObject get_empty(collector::MySQLConnection type, bool wait) override {
    return CachedObject(&cache_for(type), wait);
  }

  CachedObject get_instance(collector::MySQLConnection type,
                            bool wait) override {
    return cache_for(type).get_instance(wait);
  }

  CachedObject get_control_plane_instance(
      collector::MySQLConnection type) override {
    auto &cache = cache_for(type);
    const bool metadata = type == collector::kMySQLConnectionMetadataRO ||
                          type == collector::kMySQLConnectionMetadataRW;

    // Embedded caches close returned sessions, preventing exempt sessions
    // from being reused by requests.
    return CachedObject(
        &cache, true,
        allocate_embedded_mysql_session(
            destination_, metadata ? metadata_auth_id_ : data_auth_id_, type,
            /*bypass_max_connections=*/true));
  }

  collector::MySQLConnection get_type(const CachedObject &obj) override {
    if (obj.parent_ == &metadata_ro_.cache)
      return collector::kMySQLConnectionMetadataRO;
    if (obj.parent_ == &userdata_ro_.cache)
      return collector::kMySQLConnectionUserdataRO;
    if (obj.parent_ == &metadata_rw_.cache)
      return collector::kMySQLConnectionMetadataRW;
    if (obj.parent_ == &userdata_rw_.cache)
      return collector::kMySQLConnectionUserdataRW;

    throw std::logic_error("Unknown embedded MRS MySQL connection cache");
  }

  std::unique_ptr<CountedMySQLSession> clone_instance(
      const CountedMySQLSession::ConnectionParameters &params) override {
    // Clones are used only for query cancellation, which must work at the
    // connection limit while retaining the original session's SQL identity.
    auto session = make_counted_embedded_mysql_session(
        /*bypass_max_connections=*/true);
    session->connect_and_set_opts(params, {});
    mrs::Counter<kEntityCounterMySQLConnectionsCreated>::increment();
    return session;
  }

  void change_instance(CachedObject &instance,
                       collector::MySQLConnection type) override {
    auto &target_cache = cache_for(type);
    if (instance.parent_ == &target_cache) return;

    instance = CachedObject(&target_cache, instance.wait_);
  }

  void return_instance(CachedObject &object) override {
    if (object.parent_ != nullptr) object.parent_->return_instance(object);
  }

  // Embedded sessions are intentionally not retained in the cache, so cache
  // size configuration has no effect for this implementation
  void change_cache_object_limit(uint32_t) override {}

 private:
  EmbeddedMysqlSessionCache &cache_for(collector::MySQLConnection type) {
    switch (type) {
      case collector::kMySQLConnectionMetadataRO:
        return metadata_ro_.cache;
      case collector::kMySQLConnectionUserdataRO:
        return userdata_ro_.cache;
      case collector::kMySQLConnectionMetadataRW:
        return metadata_rw_.cache;
      case collector::kMySQLConnectionUserdataRW:
        return userdata_rw_.cache;
    }

    throw std::logic_error("Unknown MRS MySQL connection type");
  }

  Destination destination_;
  std::string metadata_auth_id_;
  std::string data_auth_id_;
  EmbeddedMysqlConnectionCache metadata_ro_, userdata_ro_, metadata_rw_,
      userdata_rw_;
};

class EmbeddedMysqlFixedPoolCallbacks
    : public EmbeddedMysqlSessionCache::Callbacks {
 public:
  using Object = EmbeddedMysqlSessionCache::Object;
  EmbeddedMysqlFixedPoolCallbacks(
      const CountedMySQLSession::ConnectionParameters &connection_parameters,
      const uint32_t pool_size)
      : connection_parameters_{connection_parameters}, pool_size_{pool_size} {}

  bool object_before_cache(Object, bool) override { return false; }

  bool object_retrived_from_cache(Object) override { return false; }

  void object_remove(Object obj) override {
    mrs::Counter<kEntityCounterMySQLConnectionsClosed>::increment();
    delete obj;
    object_count_.fetch_sub(1);
  }

  Object object_allocate(bool) override {
    if (pool_size_ <= object_count_.fetch_add(1)) {
      object_count_.fetch_sub(1);
      throw collector::db_pool_exhausted();
    }
    try {
      auto session = make_counted_embedded_mysql_session();
      session->connect_and_set_opts(connection_parameters_, {"SET ROLE ALL"});
      mrs::Counter<kEntityCounterMySQLConnectionsCreated>::increment();

      return session.release();
    } catch (...) {
      object_count_.fetch_sub(1);
      throw;
    }

    return nullptr;
  }

 private:
  CountedMySQLSession::ConnectionParameters connection_parameters_;
  const uint32_t pool_size_;
  std::atomic<uint32_t> object_count_{0};
};

class EmbeddedMysqlFixedPoolManager : public collector::MysqlFixedPoolManager {
 public:
  EmbeddedMysqlFixedPoolManager(uint32_t passthrough_pool_size,
                                Destination destination, std::string auth_id)
      : collector::MysqlFixedPoolManager(passthrough_pool_size),
        callbacks_{build_connection_params(destination, auth_id),
                   passthrough_pool_size},
        cache_{&callbacks_, passthrough_pool_size} {}

  CachedObject get_instance() override { return cache_.get_instance(false); }

  void return_instance(CachedObject &object) override {
    if (object.parent_) object.parent_->return_instance(object);
  }

 private:
  EmbeddedMysqlFixedPoolCallbacks callbacks_;
  EmbeddedMysqlSessionCache cache_;
};

}  // namespace

std::unique_ptr<collector::MysqlCacheManager>
create_embedded_mysql_cache_manager(const mrs::Configuration &configuration,
                                    const std::string &local_address,
                                    const std::string &metadata_auth_id,
                                    const std::string &data_auth_id) {
  return std::make_unique<EmbeddedMysqlCacheManager>(
      configuration, make_destination(local_address), metadata_auth_id,
      data_auth_id);
}

std::shared_ptr<collector::MysqlFixedPoolManager>
create_embedded_mysql_user_session_pool(uint32_t passthrough_pool_size,
                                        const std::string &local_address,
                                        const std::string &auth_id) {
  return std::make_shared<EmbeddedMysqlFixedPoolManager>(
      passthrough_pool_size, make_destination(local_address), auth_id);
}

}  // namespace mrs_component
