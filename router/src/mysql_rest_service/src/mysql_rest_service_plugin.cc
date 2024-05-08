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

#include <algorithm>  // max
#include <array>
#include <map>
#include <memory>  // unique_ptr
#include <set>
#include <stdexcept>  // runtime_error
#include <string>
#include <thread>
#include <vector>

#include "my_thread.h"     // NOLINT(build/include_subdir)
#include "mysqld_error.h"  // NOLINT(build/include_subdir)

#include "keyring/keyring_manager.h"
#include "mysql/harness/loader.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/plugin.h"
#include "mysqlrouter/mysql_rest_service_export.h"
#include "mysqlrouter/server_compatibility.h"

#include "collector/mysql_cache_manager.h"
#include "helper/plugin_monitor.h"
#include "mrs/database/schema_monitor.h"
#include "mrs/gtid_manager.h"
#include "mrs/object_manager.h"
#include "mrs/observability/entities_manager.h"
#include "mrs/router_observation_entities.h"
#include "mysql_rest_service_plugin_config.h"

IMPORT_LOG_FUNCTIONS()

static constexpr const char kSectionName[]{"mysql_rest_service"};

namespace {

void trace_error(const char *variable_user, const char *access,
                 const char *role, const mysqlrouter::MySQLSession::Error &e) {
  if (e.code() == ER_ROLE_NOT_GRANTED) {
    log_error(
        "MySQL Server account, set in '%s' (MRS/%s access), "
        "must be granted "
        "with '%s' role.",
        variable_user, access, role);
    log_info(
        "Please consult the MRS documentation on: how to configure MySQL "
        "Server accounts for MRS");
    return;
  }

  log_error(
      "User configured in '%s' variable, couldn't connect to MySQL Server. "
      "The process failed with %u error: %s",
      variable_user, e.code(), e.message().c_str());
}

}  // namespace

struct MrdsModule {
  MrdsModule(const ::mrs::Configuration &c) : configuration{c} {
    using namespace mysqlrouter;
    try {
      auto conn1 = mysql_connection_cache.get_instance(
          collector::kMySQLConnectionMetadataRO, true);

      check_version_compatibility(conn1.get());
    } catch (const MySQLSession::Error &e) {
      trace_error("mysql_user", "metadata", "mysql_rest_service_meta_provider",
                  e);
      throw std::runtime_error(
          "Can't start MySQL REST Service, because connection to MySQL server "
          "failed. For more informations look at previous error messages.");
    }

    try {
      auto conn2 = mysql_connection_cache.get_instance(
          collector::kMySQLConnectionUserdataRO, true);

      check_version_compatibility(conn2.get());
    } catch (const MySQLSession::Error &e) {
      trace_error("mysql_user_data_access", "user-data",
                  "mysql_rest_service_data_provider", e);
      throw std::runtime_error(
          "Can't start MySQL REST Service, because connection to MySQL server "
          "failed. For more informations look at previous error messages.");
    }

    mrs::initialize_entities(&entities_manager);
  }

  void start() { mrds_monitor.start(); }
  void stop() { mrds_monitor.stop(); }

  const ::mrs::Configuration &configuration;
  const std::string jwt_secret;
  collector::MysqlCacheManager mysql_connection_cache{configuration};
  mrs::GtidManager gtid_manager;
  mrs::authentication::AuthorizeManager authentication{
      &mysql_connection_cache, configuration.jwt_secret_};
  mrs::ObjectManager mrds_object_manager{&mysql_connection_cache,
                                         configuration.is_https_,
                                         &authentication, &gtid_manager};
  mrs::observability::EntitiesManager entities_manager;
  mrs::database::SchemaMonitor mrds_monitor{
      configuration,   &mysql_connection_cache, &mrds_object_manager,
      &authentication, &entities_manager,       &gtid_manager};
};

static std::string get_router_name(const mysql_harness::Config *config) {
  auto section = config->get_default_section();
  if (section.has("name")) {
    return section.get("name");
  }
  return "";
}

static std::unique_ptr<mrs::PluginConfig> g_mrs_configuration;
static std::unique_ptr<MrdsModule> g_mrds_module;

static void init(mysql_harness::PluginFuncEnv *env) {
  log_debug("init");
  const mysql_harness::AppInfo *info = get_app_info(env);
  std::vector<std::string> routing_instances;
  std::vector<std::string> meta_instances;

  if (info == nullptr || nullptr == info->config) {
    return;
  }

  // assume there is only one section for us
  try {
    for (const mysql_harness::ConfigSection *section :
         info->config->sections()) {
      if (section->name == "routing") {
        routing_instances.push_back(section->key);
      } else if (section->name == "metadata_cache") {
        meta_instances.push_back(section->key);
      }
    }

    auto sections = info->config->get(kSectionName);

    if (sections.empty())
      throw std::invalid_argument(
          "Missing configuration section for MRDS plugin.");

    if (1 < sections.size())
      throw std::invalid_argument(
          std::string("Found another config-section '") + kSectionName +
          "', only one allowed");

    g_mrs_configuration.reset(new mrs::PluginConfig(
        sections.front(), routing_instances, get_router_name(info->config)));

  } catch (const std::invalid_argument &exc) {
    set_error(env, mysql_harness::kConfigInvalidArgument, "%s", exc.what());
  } catch (const std::exception &exc) {
    set_error(env, mysql_harness::kRuntimeError, "%s", exc.what());
  } catch (...) {
    set_error(env, mysql_harness::kUndefinedError, "Unexpected exception");
  }
}

static void run(mysql_harness::PluginFuncEnv *env) {
  my_thread_self_setname("MRS main");
  log_debug("run");
  using namespace std::chrono_literals;
  try {
    std::set<std::string> service_names;
    auto routing_plugins =
        g_mrs_configuration->get_waiting_for_routing_plugins();

    for (const auto &el : routing_plugins) {
      service_names.insert(el.empty() ? "routing" : "routing:" + el);
    }

    if (g_mrs_configuration->service_monitor_->wait_for_services(
            service_names) &&
        g_mrs_configuration->init_runtime_configuration()) {
      g_mrds_module.reset(new MrdsModule(*g_mrs_configuration));
      g_mrds_module->start();
    }
  } catch (const std::invalid_argument &exc) {
    set_error(env, mysql_harness::kConfigInvalidArgument, "%s", exc.what());
  } catch (const std::runtime_error &exc) {
    set_error(env, mysql_harness::kRuntimeError, "%s", exc.what());
  } catch (const std::exception &exc) {
    log_debug("New exception %s", exc.what());
    set_error(env, mysql_harness::kRuntimeError, "%s", exc.what());
  }
}

static void stop(mysql_harness::PluginFuncEnv * /* env */) {
  log_debug("stop");
  if (g_mrs_configuration) g_mrs_configuration->service_monitor_->abort();
  if (g_mrds_module) g_mrds_module->stop();
}

static void deinit(mysql_harness::PluginFuncEnv * /* env */) {
  log_debug("deinit");
  if (g_mrs_configuration) g_mrs_configuration->service_monitor_->abort();
  g_mrds_module.reset();
}

static std::array<const char *, 4> required = {
    {"logger", "http_server", "rest_api", "io"}};

static const std::array<const char *, 7> supported_options{
    "mysql_user",
    "mysql_user_data_access",
    "mysql_read_write_route",
    "mysql_read_only_route",
    "router_id",
    "metadata_refresh_interval"};

// TODO(lkotula): Consider renaming the plugin from rest_mrds to mrds or
// something other if it already changed in DB schema, consult with router
// guys if such change would break their feelings (Shouldn't be in review)
// TODO(lkotula): also rename the namespace mrds + file names (Shouldn't be in
// review)
extern "C" {
mysql_harness::Plugin MYSQL_REST_SERVICE_EXPORT
    harness_plugin_mysql_rest_service = {
        mysql_harness::PLUGIN_ABI_VERSION,       // abi-version
        mysql_harness::ARCHITECTURE_DESCRIPTOR,  // arch-descriptor
        "MYSQL_REST_SERVICE", VERSION_NUMBER(0, 0, 1),
        // requires
        required.size(), required.data(),
        // conflicts
        0, nullptr,
        init,    // init
        deinit,  // deinit
        run,     // run
        stop,    // on_signal_stop
        false,   // signals ready
        supported_options.size(), supported_options.data(),
        nullptr  // TODO(lkotula): add (Shouldn't be in review)
};
}
