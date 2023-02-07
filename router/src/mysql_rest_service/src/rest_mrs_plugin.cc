/*
  Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

// TODO(lkotula): Whats wrond with this one (Shouldn't be in review)
//#define MYSQL_ROUTER_LOG_DOMAIN "io"

#include <algorithm>  // max
#include <array>
#include <map>
#include <memory>  // unique_ptr
#include <set>
#include <stdexcept>  // runtime_error
#include <string>
#include <thread>
#include <vector>

#include "keyring/keyring_manager.h"
#include "mysql/harness/loader.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/plugin.h"
#include "mysqlrouter/mysql_rest_service_export.h"

#include <helper/plugin_monitor.h>
#include "collector/mysql_cache_manager.h"
#include "mrs/database/schema_monitor.h"
#include "mrs/object_manager.h"
#include "mrs_plugin_config.h"
#include "mysqld_error.h"

IMPORT_LOG_FUNCTIONS()

static constexpr const char kSectionName[]{"mysql_rest_service"};

struct MrdsModule {
  MrdsModule(const ::mrs::Configuration &c) : configuration{c} {
    using namespace mysqlrouter;
    try {
      auto conn1 = mysql_connection_cache.get_instance(
          collector::kMySQLConnectionMetadata);
    } catch (const MySQLSession::Error &e) {
      if (e.code() == ER_ROLE_NOT_GRANTED) {
        log_error(
            "MySQL Server account, set in 'mysql_user' (MRS/metadata access), "
            "must be granted "
            "with 'mysql_rest_service_meta_provider' role.");
        log_info(
            "Please consult the MRS documentation on: how to configure MySQL "
            "Server accounts for MRS");
      }
      throw std::invalid_argument("mysql_user");
    }

    try {
      auto conn2 = mysql_connection_cache.get_instance(
          collector::kMySQLConnectionUserdata);
    } catch (const MySQLSession::Error &e) {
      if (e.code() == ER_ROLE_NOT_GRANTED) {
        log_error(
            "MySQL Server account, set in 'mysql_user_data_access' "
            "(MRS/user-data access), must be "
            "granted with 'mysql_rest_service_data_provider' role.");
        log_info(
            "Please consult the MRS documentation on: how to configure MySQL "
            "Server accounts for MRS");
      }
      throw std::invalid_argument("mysql_user_data_access");
    }

    mrds_monitor.start();
  }

  const ::mrs::Configuration &configuration;
  const std::string jwt_secret;
  collector::MysqlCacheManager mysql_connection_cache{configuration};
  mrs::authentication::AuthorizeManager authentication{
      &mysql_connection_cache, configuration.jwt_secret_};
  mrs::ObjectManager mrds_object_manager{
      &mysql_connection_cache, configuration.is_https_, &authentication};
  mrs::database::SchemaMonitor mrds_monitor{
      configuration, &mysql_connection_cache, &mrds_object_manager,
      &authentication};
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

    g_mrs_configuration.reset(
        new mrs::PluginConfig(sections.front(), routing_instances,
                              meta_instances, get_router_name(info->config)));

  } catch (const std::invalid_argument &exc) {
    set_error(env, mysql_harness::kConfigInvalidArgument, "%s", exc.what());
  } catch (const std::exception &exc) {
    set_error(env, mysql_harness::kRuntimeError, "%s", exc.what());
  } catch (...) {
    set_error(env, mysql_harness::kUndefinedError, "Unexpected exception");
  }
}

static void run(mysql_harness::PluginFuncEnv *env) {
  try {
    helper::PluginMonitor service_monitor;

    std::set<std::string> service_names;
    for (const auto &el : g_mrs_configuration->routing_names_)
      service_names.insert("routing:" + el);

    for (const auto &el : g_mrs_configuration->metada_names_)
      service_names.insert("metadata_cache:" + el);

    service_monitor.wait_for_services(service_names);
    g_mrs_configuration->init_runtime_configuration();
    g_mrds_module.reset(new MrdsModule(*g_mrs_configuration));

  } catch (const std::invalid_argument &exc) {
    set_error(env, mysql_harness::kConfigInvalidArgument, "%s", exc.what());
  } catch (const std::runtime_error &exc) {
    set_error(env, mysql_harness::kRuntimeError, "%s", exc.what());
  } catch (const std::exception &exc) {
    log_debug("New exception %s", exc.what());
    set_error(env, mysql_harness::kRuntimeError, "%s", exc.what());
  }
}

static void deinit(mysql_harness::PluginFuncEnv * /* env */) {
  g_mrds_module.reset();
}

static std::array<const char *, 3> required = {
    {"logger", "http_server", "rest_api"}};

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
        "MYSQL_REST_SERVICE",
        VERSION_NUMBER(0, 0, 1),
        // requires
        required.size(),
        required.data(),
        // conflicts
        0,
        nullptr,
        init,     // init
        deinit,   // deinit
        run,      // run
        nullptr,  // on_signal_stop
        false,    // signals ready
        supported_options.size(),
        supported_options.data(),
};
}
