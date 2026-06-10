/*
  Copyright (c) 2021, 2026, Oracle and/or its affiliates.

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

#include "mrs/database/schema_monitor.h"

#include <chrono>

#include "helper/string/contains.h"
#include "helper/string/generic.h"

#include "mrs/database/helper/query_audit_log_maxid.h"
#include "mrs/database/monitor/db_access.h"
#include "mrs/database/query_statistics.h"
#include "mrs/database/query_version.h"

#include "mrs/observability/entity.h"
#include "mrs/router_observation_entities.h"

#include "mysqld_error.h"
#include "router_config.h"
#include "socket_operations.h"

#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace database {

namespace {

std::vector<mrs::rest::entry::AppUrlHost> make_app_url_host(
    const std::vector<mrs::database::entry::UrlHost> &entries,
    const std::optional<std::string> &data) {
  std::vector<mrs::rest::entry::AppUrlHost> result;
  result.reserve(entries.size());

  for (const auto &entry : entries) {
    result.emplace_back(entry, data);
  }

  return result;
}

const std::string &to_string(
    mrs::interface::SupportedMrsMetadataVersion version) {
  const static std::string k_version2{"2"};
  const static std::string k_version3{"3"};
  return (version == mrs::interface::kSupportedMrsMetadataVersion_2
              ? k_version2
              : k_version3);
}

mrs::database::MrsSchemaVersion query_schema_version(
    mysqlrouter::MySQLSession *session) {
  QueryVersion q;
  return q.query_version(session);
}

mrs::interface::SupportedMrsMetadataVersion check_supported_mrs_version(
    const mrs::database::MrsSchemaVersion &mrs_version) {
  if (mrs_version.is_compatible({mrs::database::kCurrentMrsMetadataVersion}))
    return mrs::interface::kSupportedMrsMetadataVersion_4;

  if (mrs_version.is_compatible({{3, 0, 2}}))
    return mrs::interface::kSupportedMrsMetadataVersion_3;

  if (mrs_version.is_compatible({{2, 2, 0}}))
    return mrs::interface::kSupportedMrsMetadataVersion_2;

  std::stringstream error_message;
  if (mrs_version == mrs::database::kSchemaUpgradeMrsMetadataVersion) {
    error_message << "MRS metadata schema upgrade detected: stopping the "
                     "service until it's finished";
  } else {
    error_message << "Unsupported MRS version detected: " << mrs_version.major
                  << "." << mrs_version.minor << "." << mrs_version.patch;
  }

  throw std::runtime_error(error_message.str());
}

bool query_is_node_read_only(mysqlrouter::MySQLSession *session) {
  mysqlrouter::sqlstring q = "select @@super_read_only, @@read_only";

  auto result{session->query_one(q)};

  if (nullptr == result.get()) return false;
  if (!(*result)[0] || (!(*result)[1])) return false;

  return std::stoul(std::string((*result)[0])) == 1 ||
         std::stoul(std::string((*result)[1])) == 1;
}

void update_router_attributes_on_start(mysqlrouter::MySQLSession *session,
                                       std::optional<uint64_t> router_id_,
                                       const std::string &developer) {
  if (!router_id_) return;

  std::string sql = "UPDATE mysql_rest_service_metadata.router SET attributes=";
  if (developer.empty()) {
    sql += "JSON_REMOVE(attributes, '$.developer')";
  } else {
    sql +=
        "JSON_SET(attributes, '$.developer'," + session->quote(developer) + ")";
  }

  sql += " WHERE id = " + std::to_string(*router_id_);

  session->execute(sql);
}

}  // namespace

SchemaMonitor::SchemaMonitor(
    const mrs::Configuration &configuration,
    collector::MysqlCacheManager *cache, mrs::EndpointManager *dbobject_manager,
    authentication::AuthorizeManager *auth_manager,
    mrs::observability::EntitiesManager *entities_manager,
    mrs::GtidManager *gtid_manager,
    mrs::database::QueryFactoryProxy *query_factory,
    mrs::ResponseCache *response_cache, mrs::ResponseCache *file_cache,
    SlowQueryMonitor *slow_query_monitor, MetadataLogger *metadata_logger)
    : configuration_{configuration},
      router_name_{configuration_.router_name_},
      cache_{cache},
      dbobject_manager_{dbobject_manager},
      auth_manager_{auth_manager},
      entities_manager_{entities_manager},
      gtid_manager_{gtid_manager},
      proxy_query_factory_{query_factory},
      response_cache_{response_cache},
      file_cache_{file_cache},
      slow_query_monitor_{slow_query_monitor},
      metadata_logger_{metadata_logger},
      md_source_destination_{cache, configuration_.provider_rw_->is_dynamic()} {
}

SchemaMonitor::~SchemaMonitor() { stop(); }

void SchemaMonitor::start() {
  if (state_.exchange(k_initializing, k_running)) {
    log_debug("SchemaMonitor::start");
    run();
  }
}

void SchemaMonitor::stop() {
  waitable_.serialize_with_cv([this](void *, std::condition_variable &cv) {
    if (state_.exchange({k_initializing, k_running}, k_stopped)) {
      log_debug("SchemaMonitor::stop");
      cv.notify_all();
    }
  });
}

void SchemaMonitor::reset() {
  assert(state_.is(k_stopped));
  state_.set(k_initializing);
}

class ServiceDisabled : public std::runtime_error {
 public:
  explicit ServiceDisabled() : std::runtime_error("service disabled") {}
};

class AuditLogInconsistency : public std::runtime_error {
 public:
  explicit AuditLogInconsistency()
      : std::runtime_error("audit log inconsistency") {}
};

class MetadataSchemaVersionChange : public std::runtime_error {
 public:
  explicit MetadataSchemaVersionChange()
      : std::runtime_error("metadata schema version change") {}
};

void SchemaMonitor::run() {
  log_system("Starting MySQL REST Metadata monitor");

  bool force_clear{true};
  bool state{false};
  uint64_t max_audit_log_id{0};
  bool attributes_updated_on_start{false};
  mrs::database::MrsSchemaVersion current_schema_version;

  do {
    try {
      auto session_check_version = md_source_destination_.get_rw_session();
      if (!session_check_version) continue;

      if (!attributes_updated_on_start) {
        update_router_attributes_on_start((*session_check_version).get(),
                                          configuration_.router_id_,
                                          configuration_.developer_);
        attributes_updated_on_start = true;
      }

      current_schema_version =
          query_schema_version((*session_check_version).get());
      auto supported_schema_version =
          check_supported_mrs_version(current_schema_version);

      auto factory{create_schema_monitor_factory(supported_schema_version)};

      proxy_query_factory_->change_subject(
          create_query_factory(supported_schema_version));

      DbAccess fetcher(proxy_query_factory_, factory.get(),
                       configuration_.router_id_);

      log_system("Monitoring MySQL REST metadata (version: %s)",
                 to_string(supported_schema_version).c_str());

      auto last_stored_time = std::chrono::system_clock::now();
      bool initial_run = true;

      do {
        using namespace observability;
        auto session = (*session_check_version).empty()
                           ? cache_->get_instance(
                                 collector::kMySQLConnectionMetadataRW, true)
                           : std::move(*session_check_version);

        const auto schema_version = query_schema_version(session.get());
        if (schema_version != current_schema_version) {
          metadata_logger_->on_metadata_version_change(schema_version);

          throw MetadataSchemaVersionChange();
        }

        metadata_logger_->on_metadata_available(schema_version, session.get());

        // Delete expired sessions etc
        auth_manager_->collect_garbage();

        // Detect the inconsistency between audit_log table content and our
        // state. This only does a basic check to detect a scenario where the
        // max(id) is lower than what we have already seen. This should be good
        // enough for making sure we reinitialize as expected between the MTR
        // test runs if needed. TODO: This should be removed once we have a
        // proper way of forcing the MRS full refresh from outside while it is
        // running.
        auto audit_log_id =
            QueryAuditLogMaxId().query_max_id_or_null(session.get());
        if (!audit_log_id) audit_log_id = 0;
        if (max_audit_log_id > *audit_log_id) {
          max_audit_log_id = *audit_log_id;
          throw AuditLogInconsistency();
        }
        max_audit_log_id = *audit_log_id;

        fetcher.query(session.get());

        auto service_enabled = fetcher.get_state().service_enabled;
        if (service_enabled != state) {
          state = service_enabled;
          if (!service_enabled) {
            throw ServiceDisabled();
          }
        }

        if (fetcher.get_state_was_changed()) {
          auto global_json_config = fetcher.get_state().data.value_or("{}");
          dbobject_manager_->configure(global_json_config);
          auth_manager_->configure(global_json_config);
          gtid_manager_->configure(global_json_config);
          cache_->configure(global_json_config);
          response_cache_->configure(global_json_config);
          file_cache_->configure(global_json_config);
          slow_query_monitor_->configure(global_json_config);

          log_debug("route turn=%s, changed=%s",
                    (fetcher.get_state().service_enabled ? "on" : "off"),
                    fetcher.get_state_was_changed() ? "yes" : "no");
        }

        if (!fetcher.get_auth_app_entries().empty()) {
          auth_manager_->update(fetcher.get_auth_app_entries());
          EntityCounter<kEntityCounterUpdatesAuthentications>::increment(
              fetcher.get_auth_app_entries().size());
        }

        if (!fetcher.get_auth_user_changed_ids().empty()) {
          log_debug("clearing the auth_user cache");
          auth_manager_->update_users_cache(
              fetcher.get_auth_user_changed_ids());
        }

        if (!fetcher.get_host_entries().empty()) {
          dbobject_manager_->update(make_app_url_host(
              fetcher.get_host_entries(), fetcher.get_state().data));
        }

        if (!fetcher.get_service_entries().empty()) {
          dbobject_manager_->update(fetcher.get_service_entries());
        }

        if (!fetcher.get_schema_entries().empty()) {
          dbobject_manager_->update(fetcher.get_schema_entries());
        }

        if (!fetcher.get_content_set_entries().empty()) {
          dbobject_manager_->update(fetcher.get_content_set_entries());
        }

        auto db_object_entries = fetcher.get_db_object_entries();
        if (!db_object_entries.empty()) {
          dbobject_manager_->update(db_object_entries);
        }

        if (!fetcher.get_content_file_entries().empty()) {
          dbobject_manager_->update(fetcher.get_content_file_entries());
        }

        fetcher.update_access_factory_if_needed();

        if (fetcher.get_state().service_enabled) {
          mysqlrouter::sqlstring update{
              "INSERT INTO mysql_rest_service_metadata.router"
              " (id, router_name, address, product_name, version, attributes, "
              "options)"
              " VALUES (?,?,?,?,?,?,'{}') ON DUPLICATE KEY UPDATE "
              "version=?, router_name=?, last_check_in=NOW()"};

          const auto [name, address] = get_router_name_and_address();
          update << configuration_.router_id_ << name << address
                 << MYSQL_ROUTER_PACKAGE_NAME << MYSQL_ROUTER_VERSION
                 << (configuration_.developer_.empty()
                         ? "{}"
                         : "{\"developer\": \"" + configuration_.developer_ +
                               "\"}")
                 << MYSQL_ROUTER_VERSION << name;
          session->execute(update.str());

          try {
            const auto actual_stored_time = std::chrono::system_clock::now();
            auto status_time =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    actual_stored_time - last_stored_time)
                    .count();

            if (initial_run) {
              status_time += configuration_.metadata_refresh_interval_.count();
              initial_run = false;
            }

            QueryStatistics store_stats;
            store_stats.update_statistics(
                session.get(), configuration_.router_id_, status_time,
                entities_manager_->fetch_counters());
            last_stored_time = actual_stored_time;
          } catch (const std::exception &exc) {
            log_error(
                "Storing statistics failed, because of the following error:%s.",
                exc.what());
          }
        }
      } while (wait_until_next_refresh());
    } catch (const std::exception &) {
      force_clear = md_source_destination_.handle_error();
    }

    if (force_clear) {
      dbobject_manager_->clear();
      auth_manager_->clear();
      metadata_logger_->stop();
      force_clear = false;
    }
  } while (wait_until_next_refresh());

  // The SchemaMonitor could be restarted, in that case the object will be not
  // destroyed, the code must ensure the objects are always discarded.
  dbobject_manager_->clear();
  auth_manager_->clear();
  log_system("Stopping MySQL REST Service monitor");
}

bool SchemaMonitor::wait_until_next_refresh() {
  waitable_.wait_for(
      std::chrono::milliseconds(configuration_.metadata_refresh_interval_),
      [this](void *) { return !state_.is(k_running); });
  return state_.is(k_running);
}

std::pair<std::string, std::string>
SchemaMonitor::get_router_name_and_address() {
  auto socket_ops = mysql_harness::SocketOperations::instance();

  bool get_hostname_successful{true};
  std::string address;
  try {
    address = socket_ops->get_local_hostname();
  } catch (
      const mysql_harness::SocketOperations::LocalHostnameResolutionError &) {
    address = mrs::kHostOnResolveFailed;
    get_hostname_successful = false;
  }

  std::string name;
  if (!router_name_) {
    name = address + ":" + std::to_string(configuration_.http_port_);
    if (get_hostname_successful) {
      router_name_ = name;
    }
  } else {
    name = *router_name_;
  }

  return {name, address};
}

std::optional<collector::MysqlCacheManager::CachedObject>
SchemaMonitor::MetadataSourceDestination::get_rw_session() {
  if (is_dynamic_) {
    return cache_->get_instance(collector::kMySQLConnectionMetadataRW, true);
  }

  collector::MysqlCacheManager::CachedObject result_session;
  try {
    result_session =
        cache_->get_instance(collector::kMySQLConnectionMetadataRW, true);
    if (current_destination_state_ == DestinationState::k_offline) {
      current_destination_state_ = DestinationState::k_ok;
    }
  } catch (const mysqlrouter::MySQLSession::Error &exc) {
    if (exc.code() == ER_SERVER_OFFLINE_MODE ||
        exc.code() == ER_SERVER_OFFLINE_MODE_REASON) {
      current_destination_state_ = DestinationState::k_offline;
    } else
      throw;
  }

  if (current_destination_state_ == DestinationState::k_read_only) {
    if (!query_is_node_read_only(result_session.get())) {
      current_destination_state_ = DestinationState::k_ok;
    }
  }

  if (previous_destination_state_ != current_destination_state_) {
    if (current_destination_state_ != DestinationState::k_ok) {
      const std::string node_id =
          current_destination_state_ != DestinationState::k_offline
              ? result_session.get()->get_address()
              : "";
      log_warning("Node %s is %s, stopping the REST service", node_id.c_str(),
                  (current_destination_state_ == DestinationState::k_read_only)
                      ? "read-only"
                      : "offline");
    } else {
      log_info("Node %s is not read-only nor offline",
               result_session.get()->get_address().c_str());
    }
    previous_destination_state_ = current_destination_state_;
  }
  if (current_destination_state_ != DestinationState::k_ok) {
    return std::nullopt;
  }

  return result_session;
}

bool SchemaMonitor::MetadataSourceDestination::handle_error() {
  bool force_clear{false};
  try {
    throw;
  } catch (const mrs::database::QueryState::NoRows &exc) {
    log_error("Can't refresh MRDS layout, because of the following error:%s.",
              exc.what());
    force_clear = true;
  } catch (const mysqlrouter::MySQLSession::Error &exc) {
    log_error("Can't refresh MRDS layout, because of the following error:%s.",
              exc.what());
    if (exc.code() == ER_BAD_DB_ERROR || exc.code() == ER_NO_SUCH_TABLE) {
      force_clear = true;
    }

    if (!is_dynamic_) {
      if (exc.code() == ER_OPTION_PREVENTS_STATEMENT) {
        current_destination_state_ = DestinationState::k_read_only;
        force_clear = true;
      }

      if (exc.code() == ER_SERVER_OFFLINE_MODE ||
          exc.code() == ER_SERVER_OFFLINE_MODE_REASON) {
        current_destination_state_ = DestinationState::k_offline;
        force_clear = true;
      }
    }
  } catch (const ServiceDisabled &) {
    force_clear = true;
  } catch (const AuditLogInconsistency &) {
    log_warning(
        "audit_log table inscosistency discovered, forcing full refresh from "
        "metadata");
    force_clear = true;
  } catch (const MetadataSchemaVersionChange &) {
    log_warning(
        "metadata schema version change discovered, forcing full refresh from "
        "metadata");
    force_clear = true;
  } catch (const std::exception &exc) {
    log_error("Can't refresh MRDS layout, because of the following error:%s.",
              exc.what());
  }

  return force_clear;
}

}  // namespace database
}  // namespace mrs
