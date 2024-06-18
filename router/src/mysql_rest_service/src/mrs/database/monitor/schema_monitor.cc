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
#include "mrs/interface/schema_monitor_factory.h"
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

class AccessDatabase {
 public:
  using SchemaMonitorFactory = mrs::interface::SchemaMonitorFactory;

 public:
  AccessDatabase(SchemaMonitorFactory *factory)
      : state{factory->create_turn_state_fetcher()},
        object{factory->create_route_fetcher()},
        authentication{factory->create_authentication_fetcher()},
        content_file{factory->create_content_file_fetcher()},
        factory_{factory} {}

  std::unique_ptr<database::QueryState> state;
  std::unique_ptr<database::QueryEntryDbObject> object;
  std::unique_ptr<database::QueryEntriesAuthApp> authentication;
  std::unique_ptr<database::QueryEntriesContentFile> content_file;

  void query(mysqlrouter::MySQLSession *session) {
    mysqlrouter::MySQLSession::Transaction transaction(session);
    state->query_state(session);
    object->query_entries(session);
    authentication->query_entries(session);
    content_file->query_entries(session);
  }

  void update_access_factory_if_needed() {
    if (!fetcher_updated_) {
      state = factory_->create_turn_state_monitor(state.get());
      object = factory_->create_route_monitor(content_file->get_last_update());
      authentication = factory_->create_authentication_monitor(
          content_file->get_last_update());
      content_file = factory_->create_content_file_monitor(
          content_file->get_last_update());
      fetcher_updated_ = true;
    }
  }

 private:
  bool fetcher_updated_{false};
  SchemaMonitorFactory *factory_;
};

}  // namespace

SchemaMonitor::SchemaMonitor(
    const mrs::Configuration &configuration,
    collector::MysqlCacheManager *cache, mrs::ObjectManager *dbobject_manager,
    authentication::AuthorizeManager *auth_manager,
    mrs::observability::EntitiesManager *entities_manager,
    mrs::GtidManager *gtid_manager, SchemaMonitorFactoryMethod method)
    : configuration_{configuration},
      cache_{cache},
      dbobject_manager_{dbobject_manager},
      auth_manager_{auth_manager},
      entities_manager_{entities_manager},
      gtid_manager_{gtid_manager},
      schema_monitor_factory_method_{method} {}

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

void SchemaMonitor::run() {
  log_system("Starting MySQL REST Metadata monitor");

  do {
    try {
      auto session_check_version =
          cache_->get_instance(collector::kMySQLConnectionMetadataRW, true);
      auto supported_schema_version =
          query_supported_mrs_version(session_check_version.get());

      auto factory{schema_monitor_factory_method_(supported_schema_version)};
      mrs::database::FileFromOptions options_files;

      AccessDatabase fetcher(factory.get());

      log_system("Monitoring MySQL REST metadata (version: %s)",
                 to_string(supported_schema_version).c_str());

      do {
        using namespace observability;
        auto session = session_check_version.empty()
                           ? cache_->get_instance(
                                 collector::kMySQLConnectionMetadataRW, true)
                           : std::move(session_check_version);

        fetcher.query(session.get());

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
            dbobject_manager_->update(options_files.content_files_);
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
          dbobject_manager_->update(fetcher.object->entries);
          EntityCounter<kEntityCounterUpdatesObjects>::increment(
              fetcher.object->entries.size());

          if (options_files.content_files_.size()) {
            dbobject_manager_->update(options_files.content_files_);
            EntityCounter<kEntityCounterUpdatesFiles>::increment(
                options_files.content_files_.size());
          }
        }

        if (!fetcher.content_file->entries.empty()) {
          dbobject_manager_->update(fetcher.content_file->entries);
          EntityCounter<kEntityCounterUpdatesFiles>::increment(
              fetcher.content_file->entries.size());

          options_files.analyze(fetcher.content_file->entries);
          if (options_files.content_files_.size()) {
            dbobject_manager_->update(options_files.content_files_);
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
                "Storing statistics failed, because of following error:%s.",
                exc.what());
          }
        }
      } while (wait_until_next_refresh());
    } catch (const std::exception &exc) {
      // TODO(lkotula): For now we ignore those errors (Shouldn't be in
      // review)

      log_error("Can't refresh MRDS layout, because of following error:%s.",
                exc.what());
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
