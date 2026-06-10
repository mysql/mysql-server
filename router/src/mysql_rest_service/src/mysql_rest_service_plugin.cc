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

#include <algorithm>  // max
#include <array>
#include <map>
#include <memory>  // unique_ptr
#include <set>
#include <stdexcept>  // runtime_error
#include <string>
#include <thread>
#include <vector>

#include "my_sys.h"
#include "my_thread.h"     // NOLINT(build/include_subdir)
#include "mysqld_error.h"  // NOLINT(build/include_subdir)
#include "socket_operations.h"

#include "keyring/keyring_manager.h"
#include "mysql/harness/loader.h"
#include "mysql/harness/logging/logging.h"
#include "mysql/harness/logging/registry.h"
#include "mysql/harness/plugin.h"
#include "mysqlrouter/mysql_rest_service_export.h"
#include "mysqlrouter/server_compatibility.h"

#include "collector/mysql_cache_manager.h"
#include "helper/plugin_monitor.h"
#include "helper/task_control.h"
#include "mrs/authentication/auth_handler_factory.h"
#include "mrs/database/metadata_logger.h"
#include "mrs/database/query_factory_proxy.h"
#include "mrs/database/query_router_info.h"
#include "mrs/database/schema_monitor.h"
#include "mrs/database/slow_query_monitor.h"
#include "mrs/endpoint/handler/handler_debug.cc"
#include "mrs/endpoint_configuration.h"
#include "mrs/endpoint_manager.h"
#include "mrs/gtid_manager.h"
#include "mrs/observability/entities_manager.h"
#include "mrs/router_observation_entities.h"
#include "mysql/harness/logging/logger_plugin.h"
#include "mysql_rest_service_plugin_config.h"
#include "mysqlrouter/http_constants.h"
#include "mysqlrouter/router_config_utils.h"

#include "supported_mysql_rest_service_options.h"

IMPORT_LOG_FUNCTIONS()

static constexpr const char kSectionName[]{"mysql_rest_service"};

namespace {

void trace_error(const char *variable_user, const char *access,
                 const char *role, const mysqlrouter::MySQLSession::Error &e) {
  if (e.code() == ER_ROLE_NOT_GRANTED) {
    log_error(
        "MySQL Server account, set in '%s' (MRS/%s access), "
        "must be granted with '%s' role.",
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

std::optional<uint64_t> find_existing_routers(
    mysqlrouter::MySQLSession *session, const std::string &router_name,
    const std::string &address) {
  mrs::database::QueryRouterInfo q;
  return q.find_existing_router_instances(session, router_name, address);
}

}  // namespace

class MrsModule {
 public:
  MrsModule(const ::mrs::Configuration &c) : configuration{c} {
    mrs::initialize_entities(&entities_manager);
  }

  virtual ~MrsModule() = default;

  void init() {
    using namespace std::chrono_literals;
    const auto end_time = std::chrono::system_clock::now() +
                          configuration.wait_for_metadata_schema_access_;
    const auto kStep = 500ms;

    while (!init(std::chrono::system_clock::now() >= end_time)) {
      const auto timeout_left = end_time - std::chrono::system_clock::now();
      std::this_thread::sleep_for(timeout_left > kStep ? kStep : timeout_left);
    }
  }

  bool init(bool fail_on_no_role_granted) {
    using namespace mysqlrouter;
    // TODO(areliga): remove that when rebased to the Router version that fixes
    // ODR issues
    my_init();
    collector::MysqlCacheManager::CachedObject conn1;

    try {
      conn1 = mysql_connection_cache.get_instance(
          collector::kMySQLConnectionMetadataRO, true);

      check_version_compatibility(conn1.get());
    } catch (const MySQLSession::Error &e) {
      if (!fail_on_no_role_granted &&
          (e.code() == ER_ROLE_NOT_GRANTED ||
           e.code() == ER_ACCESS_DENIED_ERROR ||
           e.code() == ER_ACCESS_DENIED_NO_PASSWORD_ERROR)) {
        return false;
      }
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
      if (!fail_on_no_role_granted &&
          (e.code() == ER_ROLE_NOT_GRANTED ||
           e.code() == ER_ACCESS_DENIED_ERROR ||
           e.code() == ER_ACCESS_DENIED_NO_PASSWORD_ERROR)) {
        return false;
      }
      trace_error("mysql_user_data_access", "user-data",
                  "mysql_rest_service_data_provider", e);
      throw std::runtime_error(
          "Can't start MySQL REST Service, because connection to MySQL server "
          "failed. For more informations look at previous error messages.");
    }

    auto socket_ops = mysql_harness::SocketOperations::instance();
    std::string address;
    try {
      address = socket_ops->get_local_hostname();
    } catch (
        const mysql_harness::SocketOperations::LocalHostnameResolutionError &) {
      address = mrs::kHostOnResolveFailed;
    }

    std::string name;
    if (configuration.router_name_) {
      name = *configuration.router_name_;
    } else {
      name = address + ":" + std::to_string(configuration.http_port_);
    }

    const auto existing_id_maybe =
        find_existing_routers(conn1.get(), name, address);
    if (existing_id_maybe && *existing_id_maybe != configuration.router_id_) {
      throw std::runtime_error(
          "Metadata already contains Router registered as '" + name + "' at '" +
          address + "' with id: " + std::to_string(*existing_id_maybe) +
          ", new id: " + std::to_string(configuration.router_id_));
    }

    return true;
  }

  virtual void start() {
    init();
    task_monitor.start();
    slow_monitor.start();
    metadata_logger.start(&configuration, &mysql_connection_cache);
    // must be called last
    mrs_monitor.start();
  }

  virtual void stop() {
    slow_monitor.stop();
    task_monitor.stop();
    mrs_monitor.stop();
    // metadata_logger has to be stopped after mrs_monitor
    metadata_logger.stop();
  }

  void reset() {
    slow_monitor.reset();
    task_monitor.reset();
    mrs_monitor.reset();
  }

  using AuthHandlerFactory = mrs::authentication::AuthHandlerFactory;
  using AuthorizeManager = mrs::authentication::AuthorizeManager;

  const ::mrs::Configuration &configuration;
  std::shared_ptr<mrs::interface::EndpointConfiguration> endpoint_configuration{
      std::make_shared<mrs::EndpointConfiguration>(configuration)};
  const std::string jwt_secret;
  mrs::database::QueryFactoryProxy query_factory{
      std::make_shared<mrs::database::v2::QueryFactory>()};
  collector::MysqlCacheManager mysql_connection_cache{configuration};
  mrs::GtidManager gtid_manager;
  std::shared_ptr<AuthHandlerFactory> auth_handler_factory{
      std::make_shared<AuthHandlerFactory>(&query_factory)};
  AuthorizeManager authentication{
      endpoint_configuration, &mysql_connection_cache,
      configuration.jwt_secret_, &query_factory, auth_handler_factory};
  mrs::ResponseCache response_cache{"responseCache"};
  mrs::ResponseCache file_cache{"fileCache"};
  mrs::database::SlowQueryMonitor slow_monitor{configuration,
                                               &mysql_connection_cache};
  mrs::database::MysqlTaskMonitor task_monitor;
  mrs::database::MetadataLogger &metadata_logger{
      mrs::database::MetadataLogger::instance()};

  mrs::EndpointManager mrds_object_manager{endpoint_configuration,
                                           &mysql_connection_cache,
                                           &authentication,
                                           &gtid_manager,
                                           nullptr,
                                           &response_cache,
                                           &file_cache,
                                           &slow_monitor,
                                           &task_monitor};
  mrs::observability::EntitiesManager entities_manager;

  /**
   * Class responsible for monitoring changes in MRS schema
   *
   * This class fetches the changes in MRS and distributes them to
   * different "manager" classes.
   */
  mrs::database::SchemaMonitor mrs_monitor{
      configuration,   &mysql_connection_cache, &mrds_object_manager,
      &authentication, &entities_manager,       &gtid_manager,
      &query_factory,  &response_cache,         &file_cache,
      &slow_monitor,   &metadata_logger};
};

using HandlerDebug = mrs::endpoint::handler::HandlerDebug;
using HandlerCallback = mrs::endpoint::handler::HandlerCallback;
using HandlerConfiguration = mrs::interface::RestHandler::Configuration;

template <typename T>
class HttpControl : public T, HandlerCallback {
 public:
  template <typename... Args>
  HttpControl(Args &&...args) : T(std::forward<Args>(args)...) {
    handler_debug = std::make_shared<mrs::endpoint::handler::HandlerDebug>(
        static_cast<HandlerCallback *>(this));
    handler_debug->initialize(HandlerConfiguration());
  }

  void handler_start() override { T::task_resume(); }

  void handler_stop() override { T::task_suspend(); }

  std::shared_ptr<mrs::endpoint::handler::HandlerDebug> handler_debug;
};

static std::unique_ptr<mrs::PluginConfig> g_mrs_configuration;
static std::unique_ptr<MrsModule> g_mrds_module;

static void init_metadata_logger(
    const mysql_harness::Config *config,
    const mysql_harness::ConfigSection *mrs_section) {
  if (!mysql_harness::logging::handler_registered(kSectionName)) return;

  // 'mysql_rest_service' is configured as a sink, figure out the log level and
  // create a sink
  using mysql_harness::logging::get_default_log_level;
  using mysql_harness::logging::log_level_from_string;
  using mysql_harness::logging::LogLevel;

  LogLevel log_level;
  static constexpr const char *kLogLevel = "level";
  if (mrs_section->has(kLogLevel)) {
    log_level = log_level_from_string(mrs_section->get(kLogLevel));
  } else {
    log_level = get_default_log_level(*config);
  }

  mrs::database::MetadataLogger::instance().init(log_level);
}

static void init(mysql_harness::PluginFuncEnv *env) {
  log_debug("init");
  const mysql_harness::AppInfo *info = get_app_info(env);
  std::vector<std::string> routing_instances;

  if (info == nullptr || nullptr == info->config) {
    return;
  }

  // assume there is only one section for us
  try {
    for (const mysql_harness::ConfigSection *section :
         info->config->sections()) {
      if (section->name == "routing") {
        if (section->has("protocol") && section->get("protocol") == "x")
          continue;
        routing_instances.push_back(section->key);
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

    init_metadata_logger(info->config, sections.front());

    g_mrs_configuration.reset(new mrs::PluginConfig(
        sections.front(), routing_instances,
        get_configured_router_name(*info->config, kDefaultHttpPort),
        get_configured_http_port(*info->config, kDefaultHttpPort)));

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
      if (getenv("_MRS_CONTROL_START")) {
        g_mrds_module.reset(new HttpControl<helper::TaskControl<MrsModule>>(
            *g_mrs_configuration));
      } else {
        g_mrds_module.reset(new MrsModule(*g_mrs_configuration));
      }

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
  g_mrs_configuration.reset();

  mrs::database::MetadataLogger::instance().deinit();
}

#ifndef HAVE_JIT_EXECUTOR_PLUGIN
static std::array<const char *, 3> required = {{"logger", "http_server", "io"}};
#else
static std::array<const char *, 4> required = {
    {"logger", "http_server", "io", "jit_executor"}};
#endif

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
        mysql_rest_service_supported_options.size(),
        mysql_rest_service_supported_options.data(),
        nullptr  // TODO(lkotula): add (Shouldn't be in review)
};
}
