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

#include "mysql/harness/logging/logging.h"

#include "helper/string/contains.h"
#include "helper/string/generic.h"
#include "mrs/database/helper/content_file_from_options.h"
#include "mrs/database/query_changes_auth_app.h"
#include "mrs/database/query_changes_content_file.h"
#include "mrs/database/query_changes_db_object.h"
#include "mrs/database/query_changes_state.h"
#include "mrs/database/query_entries_auth_app.h"
#include "mrs/database/query_entries_content_file.h"
#include "mrs/database/query_entries_db_object.h"
#include "mrs/database/query_state.h"
#include "mrs/database/query_statistics.h"
#include "mrs/observability/entity.h"
#include "mrs/router_observation_entities.h"

#include "router_config.h"
#include "socket_operations.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace database {

SchemaMonitor::SchemaMonitor(
    const mrs::Configuration &configuration,
    collector::MysqlCacheManager *cache, mrs::ObjectManager *dbobject_manager,
    authentication::AuthorizeManager *auth_manager,
    mrs::observability::EntitiesManager *entities_manager,
    mrs::GtidManager *gtid_manager)
    : configuration_{configuration},
      cache_{cache},
      dbobject_manager_{dbobject_manager},
      auth_manager_{auth_manager},
      entities_manager_{entities_manager},
      gtid_manager_{gtid_manager} {}

SchemaMonitor::~SchemaMonitor() { stop(); }

void SchemaMonitor::start() {
  state_.exchange(k_initializing, k_running);
  log_debug("State at start:%i", static_cast<int>(state_.get()));
  run();
}

void SchemaMonitor::stop() {
  waitable_.serialize_with_cv([this](void *, std::condition_variable &cv) {
    state_.exchange({k_initializing, k_running}, k_stopped);
    log_debug("State at stop:%i", static_cast<int>(state_.get()));
    cv.notify_all();
  });
  // The thread might be already stopped or even it has never started
  if (monitor_thread_.joinable()) monitor_thread_.join();
}

void SchemaMonitor::run() {
  // TODO(lkotula): Remove below log-entry (Shouldn't be in review)
  //  using RowProcessor = mysqlrouter::MySQLSession::RowProcessor;
  log_system("Starting monitor");
  bool full_fetch_compleated = false;
  mrs::database::FileFromOptions options_files;
  std::unique_ptr<database::QueryState> turn_state{new database::QueryState()};
  std::unique_ptr<database::QueryEntryDbObject> route_fetcher{
      new database::QueryEntryDbObject()};
  std::unique_ptr<database::QueryEntriesAuthApp> authentication_fetcher{
      new database::QueryEntriesAuthApp()};
  std::unique_ptr<database::QueryEntriesContentFile> content_file_fetcher{
      new database::QueryEntriesContentFile()};
  //  int i = 0;

  do {
    try {
      using namespace observability;
      auto session =
          cache_->get_instance(collector::kMySQLConnectionMetadataRW, true);

      turn_state->query_state(session.get());
      authentication_fetcher->query_entries(session.get());
      route_fetcher->query_entries(session.get());
      content_file_fetcher->query_entries(session.get());

      if (turn_state->was_changed()) {
        auto global_json_config = turn_state->get_json_data();
        auto state = turn_state->get_state();
        dbobject_manager_->turn(state, global_json_config);
        auth_manager_->configure(global_json_config);
        gtid_manager_->configure(global_json_config);
        cache_->configure(global_json_config);

        log_debug("route turn=%s, changed=%s",
                  (turn_state->get_state() == stateOn ? "on" : "off"),
                  turn_state->was_changed() ? "yes" : "no");

        options_files.analyze_global(state == mrs::stateOn, global_json_config);
        if (options_files.content_files_.size()) {
          dbobject_manager_->update(options_files.content_files_);
          EntityCounter<kEntityCounterUpdatesFiles>::increment(
              options_files.content_files_.size());
        }
      }

      if (!authentication_fetcher->entries.empty()) {
        auth_manager_->update(authentication_fetcher->entries);
        EntityCounter<kEntityCounterUpdatesAuthentications>::increment(
            authentication_fetcher->entries.size());
      }

      if (!route_fetcher->entries.empty()) {
        options_files.analyze(route_fetcher->entries);
        dbobject_manager_->update(route_fetcher->entries);
        EntityCounter<kEntityCounterUpdatesObjects>::increment(
            route_fetcher->entries.size());

        if (options_files.content_files_.size()) {
          dbobject_manager_->update(options_files.content_files_);
          EntityCounter<kEntityCounterUpdatesFiles>::increment(
              options_files.content_files_.size());
        }
      }

      if (!content_file_fetcher->entries.empty()) {
        dbobject_manager_->update(content_file_fetcher->entries);
        EntityCounter<kEntityCounterUpdatesFiles>::increment(
            content_file_fetcher->entries.size());

        options_files.analyze(content_file_fetcher->entries);
        if (options_files.content_files_.size()) {
          dbobject_manager_->update(options_files.content_files_);
          EntityCounter<kEntityCounterUpdatesFiles>::increment(
              options_files.content_files_.size());
        }
      }

      if (!full_fetch_compleated) {
        full_fetch_compleated = true;
        turn_state.reset(new database::QueryChangesState(turn_state.get()));
        route_fetcher.reset(new database::QueryChangesDbObject(
            route_fetcher->get_last_update()));
        authentication_fetcher.reset(new database::QueryChangesAuthApp(
            authentication_fetcher->get_last_update()));

        content_file_fetcher.reset(new database::QueryChangesContentFile(
            content_file_fetcher->get_last_update()));
      }

      if (turn_state->get_state() == mrs::stateOn &&
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
               << socket_ops->get_local_hostname() << MYSQL_ROUTER_PACKAGE_NAME
               << MYSQL_ROUTER_VERSION << MYSQL_ROUTER_VERSION;
        session->execute(update.str());
      }

      if (configuration_.router_id_.has_value()) {
        try {
          QueryStatistics store_stats;
          store_stats.update_statistics(
              session.get(), configuration_.router_id_.value(),
              configuration_.metadata_refresh_interval_.count(),
              entities_manager_->fetch_counters());
        } catch (const std::exception &exc) {
          log_error("Storing statistics failed, because of following error:%s.",
                    exc.what());
        }
      }

      // TODO(lkotula): Set dirty/clean should be used before START_TRANSACTION
      // and COMMIT (Shouldn't be in review)
      session.set_clean();
    } catch (const std::exception &exc) {
      // TODO(lkotula): For now we ignore those errors (Shouldn't be in
      // review)

      log_error("Can't refresh MRDS layout, because of following error:%s.",
                exc.what());
    }
  } while (wait_until_next_refresh());
  log_system("Stopping monitor");
}

bool SchemaMonitor::wait_until_next_refresh() {
  waitable_.wait_for(
      std::chrono::seconds(configuration_.metadata_refresh_interval_),
      [this](void *) { return !state_.is(k_running); });
  return state_.is(k_running);
}

}  // namespace database
}  // namespace mrs
