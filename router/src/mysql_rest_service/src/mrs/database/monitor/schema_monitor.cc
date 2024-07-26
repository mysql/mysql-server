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

#include "mrs/database/schema_monitor.h"

#include "helper/string/contains.h"
#include "helper/string/generic.h"
#include "mrs/database/helper/content_file_from_options.h"
#include "mrs/database/query_statistics.h"
#include "mrs/database/query_version.h"
#include "mrs/interface/query_monitor_factory.h"
#include "mrs/observability/entity.h"
#include "mrs/router_observation_entities.h"

#include "router_config.h"
#include "socket_operations.h"

#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace database {

namespace {

const std::string &to_string(
    mrs::interface::SupportedMrsMetadataVersion version) {
  const static std::string k_version2{"2"};
  const static std::string k_version3{"3"};
  return (version == mrs::interface::kSupportedMrsMetadataVersion_2
              ? k_version2
              : k_version3);
}

mrs::interface::SupportedMrsMetadataVersion query_supported_mrs_version(
    mysqlrouter::MySQLSession *session) {
  QueryVersion q;
  auto mrs_version = q.query_version(session);

  if (mrs_version.major == 2 && mrs_version.minor <= 2)
    return mrs::interface::kSupportedMrsMetadataVersion_2;

  if (mrs_version.major != 3 || mrs_version.minor != 0) {
    std::stringstream error_message;
    error_message << "Unsupported MRS version detected: " << mrs_version.major
                  << "." << mrs_version.minor << "." << mrs_version.patch;
    throw std::runtime_error(error_message.str());
  }

  return mrs::interface::kSupportedMrsMetadataVersion_3;
}

#if 0
std::string query_developer(mysqlrouter::MySQLSession *session,
                            std::optional<uint64_t> router_id) {
  if (!router_id.has_value()) {
    return "";
  }

  mysqlrouter::sqlstring q{
      "select json_unquote(json_extract(options, '$.developer')) from "
      "mysql_rest_service_metadata.router where id = ?"};

  q << router_id.value();
  auto res = session->query_one(q.str());
  if (nullptr == res.get()) return {};

  return (*res)[0] ? (*res)[0] : "";
}
#endif

std::set<UniversalId> query_allowed_services(
    mysqlrouter::MySQLSession *session,
    const mrs::interface::SupportedMrsMetadataVersion &md_version,
    std::optional<uint64_t> router_id) {
  if (!router_id.has_value()) {
    return {};
  }

  mysqlrouter::sqlstring q;
  std::set<UniversalId> result;
  if (md_version < mrs::interface::SupportedMrsMetadataVersion::
                       kSupportedMrsMetadataVersion_3) {
    q = "select s.id from mysql_rest_service_metadata.service s"
        " where (enabled = 1)";
  } else {
    q = "select s.id "
        " from mysql_rest_service_metadata.service s where (enabled = 1) AND"
        " ("
        "  ((published = 1) AND (NOT EXISTS (select s2.id from"
        "     mysql_rest_service_metadata.service s2"
        "      where s.url_host_id=s2.url_host_id"
        "          AND s.url_context_root=s2.url_context_root AND"
        "          JSON_OVERLAPS((select options->'$.developer' from"
        "              mysql_rest_service_metadata.router"
        "              where id = ?), s2.in_development->>'$.developers'))))"
        " OR"
        "  ((published = 0) AND (s.id IN (select s2.id from"
        "     mysql_rest_service_metadata.service s2"
        "      where s.url_host_id=s2.url_host_id"
        "          AND s.url_context_root=s2.url_context_root AND"
        "          JSON_OVERLAPS((select options->'$.developer' from"
        "          mysql_rest_service_metadata.router"
        "              where id = ?), s2.in_development->>'$.developers'))))"
        " )";
    q << router_id.value() << router_id.value();
  }

  auto result_processor =
      [&result](const mysqlrouter::MySQLSession::Row &row) -> bool {
    assert(row.size() == 1);
    assert(row[0]);

    entry::UniversalId session_id;
    entry::UniversalId::from_raw(&session_id, row[0]);

    result.insert(session_id);

    return true;
  };

  session->query(q, result_processor);

  return result;
}

class AccessDatabase {
 public:
  using QueryMonitorFactory = mrs::interface::QueryMonitorFactory;

 public:
  AccessDatabase(mrs::interface::QueryFactory *query_factory,
                 QueryMonitorFactory *query_monitor_factory)
      : state{query_monitor_factory->create_turn_state_fetcher()},
        url_host{query_monitor_factory->create_url_host_fetcher()},
        db_service{query_monitor_factory->create_db_service_fetcher()},
        db_schema{query_monitor_factory->create_db_schema_fetcher()},
        db_object{
            query_monitor_factory->create_db_object_fetcher(query_factory)},
        object{query_monitor_factory->create_route_fetcher(
            query_factory)},  // TODO: remove
        authentication{query_monitor_factory->create_authentication_fetcher()},
        content_file{query_monitor_factory->create_content_file_fetcher()},
        query_monitor_factory_{query_monitor_factory},
        query_factory_{query_factory} {}

  std::unique_ptr<database::QueryState> state;
  std::unique_ptr<database::QueryEntriesUrlHost> url_host;
  std::unique_ptr<database::QueryEntriesDbService> db_service;
  std::unique_ptr<database::QueryEntriesDbSchema> db_schema;
  std::unique_ptr<database::QueryEntriesDbObjectLite> db_object;
  std::unique_ptr<database::QueryEntriesDbObject> object;  // TODO: remove
  std::unique_ptr<database::QueryEntriesAuthApp> authentication;
  std::unique_ptr<database::QueryEntriesContentFile> content_file;

  void query(mysqlrouter::MySQLSession *session) {
    mysqlrouter::MySQLSession::Transaction transaction(session);
    state->query_state(session);
    object->query_entries(session);  // TODO: remove
#if 0
    url_host->query_entries(session);
    db_service->query_entries(session);
    db_schema->query_entries(session);
    db_object->query_entries(session);
#endif
    authentication->query_entries(session);
    content_file->query_entries(session);
  }

  void update_access_factory_if_needed() {
    if (!fetcher_updated_) {
      state = query_monitor_factory_->create_turn_state_monitor(state.get());
      object = query_monitor_factory_->create_route_monitor(
          query_factory_, content_file->get_last_update());  // TODO: remove
      url_host = query_monitor_factory_->create_url_host_monitor(
          content_file->get_last_update());
      db_service = query_monitor_factory_->create_db_service_monitor(
          content_file->get_last_update());
      db_schema = query_monitor_factory_->create_db_schema_monitor(
          content_file->get_last_update());
      db_object = query_monitor_factory_->create_db_object_monitor(
          query_factory_, content_file->get_last_update());
      authentication = query_monitor_factory_->create_authentication_monitor(
          content_file->get_last_update());
      content_file = query_monitor_factory_->create_content_file_monitor(
          content_file->get_last_update());
      fetcher_updated_ = true;
    }
  }

 private:
  bool fetcher_updated_{false};
  QueryMonitorFactory *query_monitor_factory_;
  mrs::interface::QueryFactory *query_factory_;
};

}  // namespace

SchemaMonitor::SchemaMonitor(
    const mrs::Configuration &configuration,
    collector::MysqlCacheManager *cache, mrs::ObjectManager *dbobject_manager,
    authentication::AuthorizeManager *auth_manager,
    mrs::observability::EntitiesManager *entities_manager,
    mrs::GtidManager *gtid_manager,
    mrs::database::QueryFactoryProxy *query_factory)
    : configuration_{configuration},
      cache_{cache},
      dbobject_manager_{dbobject_manager},
      auth_manager_{auth_manager},
      entities_manager_{entities_manager},
      gtid_manager_{gtid_manager},
      proxy_query_factory_{query_factory} {}

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
  // The thread might be already stopped or even it has never started
  if (monitor_thread_.joinable()) monitor_thread_.join();
}

class ServiceDisabled : public std::runtime_error {
 public:
  explicit ServiceDisabled() : std::runtime_error("service disabled") {}
};

void SchemaMonitor::run() {
  log_system("Starting MySQL REST Metadata monitor");

  bool force_clear{true};
  mrs::State state{stateOff};
  do {
    try {
      auto session_check_version =
          cache_->get_instance(collector::kMySQLConnectionMetadataRW, true);
      auto supported_schema_version =
          query_supported_mrs_version(session_check_version.get());

#if 0
      const auto developer = query_developer(session_check_version.get(),
                                             configuration_.router_id_);
#endif

      auto factory{create_schema_monitor_factory(supported_schema_version)};
      mrs::database::FileFromOptions options_files;

      proxy_query_factory_->change_subject(
          create_query_factory(supported_schema_version));

      AccessDatabase fetcher(proxy_query_factory_, factory.get());

      log_system("Monitoring MySQL REST metadata (version: %s)",
                 to_string(supported_schema_version).c_str());
#if 0
      if (!developer.empty()) {
        log_system("Configured to expose the services for developer '%s'",
                   developer.c_str());
      }
#endif

      do {
        using namespace observability;
        auto session = session_check_version.empty()
                           ? cache_->get_instance(
                                 collector::kMySQLConnectionMetadataRW, true)
                           : std::move(session_check_version);

        const auto allowed_services = query_allowed_services(
            session.get(), supported_schema_version, configuration_.router_id_);

        fetcher.query(session.get());

        auto current_state = fetcher.state->get_state();
        if (current_state != state) {
          state = current_state;
          if (current_state == stateOff) {
            throw ServiceDisabled();
          }
        }

        if (fetcher.state->was_changed()) {
          auto global_json_config = fetcher.state->get_json_data();
          auto state = fetcher.state->get_state();
          dbobject_manager_->turn(state, global_json_config);
          auth_manager_->configure(global_json_config);
          gtid_manager_->configure(global_json_config);
          cache_->configure(global_json_config);

          log_debug("route turn=%s, changed=%s",
                    (fetcher.state->get_state() == stateOn ? "on" : "off"),
                    fetcher.state->was_changed() ? "yes" : "no");

          options_files.analyze_global(state == mrs::stateOn,
                                       global_json_config);
          if (options_files.content_files_.size()) {
            dbobject_manager_->update(options_files.content_files_,
                                      allowed_services);
            EntityCounter<kEntityCounterUpdatesFiles>::increment(
                options_files.content_files_.size());
          }
        }

        if (!fetcher.authentication->get_entries().empty()) {
          auth_manager_->update(fetcher.authentication->get_entries());
          EntityCounter<kEntityCounterUpdatesAuthentications>::increment(
              fetcher.authentication->get_entries().size());
        }

        if (!fetcher.object->entries.empty()) {
          options_files.analyze(fetcher.object->entries);
          dbobject_manager_->update(fetcher.object->entries, allowed_services);
          EntityCounter<kEntityCounterUpdatesObjects>::increment(
              fetcher.object->entries.size());

          if (options_files.content_files_.size()) {
            dbobject_manager_->update(options_files.content_files_,
                                      allowed_services);
            EntityCounter<kEntityCounterUpdatesFiles>::increment(
                options_files.content_files_.size());
          }
        }

        if (!fetcher.url_host->entries.empty()) {
          // dbobject_manager_->update(fetcher.url_host->entries);
          EntityCounter<kEntityCounterUpdatesHosts>::increment(
              fetcher.url_host->entries.size());
        }

        if (!fetcher.db_service->entries.empty()) {
          // dbobject_manager_->update(fetcher.db_service->entries);
          EntityCounter<kEntityCounterUpdatesServices>::increment(
              fetcher.db_service->entries.size());
        }

        if (!fetcher.db_schema->entries.empty()) {
          // dbobject_manager_->update(fetcher.db_schema->entries);
          EntityCounter<kEntityCounterUpdatesSchemas>::increment(
              fetcher.db_schema->entries.size());
        }

        if (!fetcher.db_object->entries.empty()) {
          // dbobject_manager_->update(fetcher.db_object->entries);
          EntityCounter<kEntityCounterUpdatesObjects>::increment(
              fetcher.db_object->entries.size());
        }

        if (!fetcher.content_file->entries.empty()) {
          dbobject_manager_->update(fetcher.content_file->entries,
                                    allowed_services);
          EntityCounter<kEntityCounterUpdatesFiles>::increment(
              fetcher.content_file->entries.size());

          options_files.analyze(fetcher.content_file->entries);
          if (options_files.content_files_.size()) {
            dbobject_manager_->update(options_files.content_files_,
                                      allowed_services);
            EntityCounter<kEntityCounterUpdatesFiles>::increment(
                options_files.content_files_.size());
          }
        }

        fetcher.update_access_factory_if_needed();

        if (fetcher.state->get_state() == mrs::stateOn &&
            configuration_.router_id_.has_value()) {
          auto socket_ops = mysql_harness::SocketOperations::instance();

          mysqlrouter::sqlstring update{
              "INSERT INTO mysql_rest_service_metadata.router"
              " (id, router_name, address, product_name, version, attributes, "
              "options)"
              " VALUES (?,?,?,?,?,'{}','{}') ON DUPLICATE KEY UPDATE "
              "version=?, last_check_in=NOW()"};

          update << configuration_.router_id_.value()
                 << configuration_.router_name_
                 << socket_ops->get_local_hostname()
                 << MYSQL_ROUTER_PACKAGE_NAME << MYSQL_ROUTER_VERSION
                 << MYSQL_ROUTER_VERSION;
          session->execute(update.str());

          try {
            QueryStatistics store_stats;
            store_stats.update_statistics(
                session.get(), configuration_.router_id_.value(),
                configuration_.metadata_refresh_interval_.count(),
                entities_manager_->fetch_counters());
          } catch (const std::exception &exc) {
            log_error(
                "Storing statistics failed, because of the following error:%s.",
                exc.what());
          }
        }
      } while (wait_until_next_refresh());
    } catch (const mrs::database::QueryState::NoRows &exc) {
      log_error("Can't refresh MRDS layout, because of the following error:%s.",
                exc.what());
      force_clear = true;
    } catch (const mysqlrouter::MySQLSession::Error &exc) {
      log_error("Can't refresh MRDS layout, because of the following error:%s.",
                exc.what());
      if (exc.code() == 1049 /*unknown database*/ ||
          exc.code() == 1146 /*table does not exist*/) {
        force_clear = true;
      }
    } catch (const ServiceDisabled &) {
      force_clear = true;
    } catch (const std::exception &exc) {
      // TODO(lkotula): For now we ignore those errors (Shouldn't be in
      // review)

      log_error("Can't refresh MRDS layout, because of the following error:%s.",
                exc.what());
    }

    if (force_clear) {
      dbobject_manager_->clear();
      auth_manager_->clear();
      force_clear = false;
    }
  } while (wait_until_next_refresh());

  log_system("Stopping MySQL REST Service monitor");
}

bool SchemaMonitor::wait_until_next_refresh() {
  waitable_.wait_for(
      std::chrono::seconds(configuration_.metadata_refresh_interval_),
      [this](void *) { return !state_.is(k_running); });
  return state_.is(k_running);
}

}  // namespace database
}  // namespace mrs
