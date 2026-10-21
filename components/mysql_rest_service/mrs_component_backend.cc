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
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include <algorithm>
#include <cassert>
#include <exception>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "mysql/components/util/debug_sync.h"
#include "mysql/harness/application_type_manager.h"
#include "mysql/harness/logging/registry.h"
#include "mysql/harness/process_state_component.h"
#include "mysql/harness/tls_server_context.h"
#include "mysql/harness/utility/string.h"
#include "mysqld_error.h"
#include "mysqlrouter/io_component.h"
#include "mysqlrouter/supported_router_options.h"
#include "mysqlrouter/utils_sqlstring.h"
#include "router/router_config.h"
#include "scope_guard.h"

#include "component_application_type_name.h"
#include "mrs_component.h"
#include "mrs_component_backend.h"
#include "mrs_component_command_executor.h"
#include "mrs_component_log.h"
#include "mrs_component_runtime_services.h"

#include "random_generator.h"
#include "router/src/harness/src/builtin_plugins.h"
#include "router/src/harness/src/logging/componentlog_plugin.h"
#include "router/src/mysql_rest_service/include/mrs/embedded_runtime_services.h"

mysql_harness::Plugin harness_plugin_router_protobuf = {
    mysql_harness::PLUGIN_ABI_VERSION, mysql_harness::ARCHITECTURE_DESCRIPTOR,
    "", VERSION_NUMBER(0, 0, 1),
    // requires
    0, nullptr,
    // conflicts
    0, nullptr,
    nullptr,  // init
    [](mysql_harness::PluginFuncEnv *) {
      /* TODO: Define protobuf shutdown ownership. See Bug#39967188. */
    },
    nullptr,  // start
    nullptr,  // stop
    false,    // declare_readiness
    0, nullptr,
    nullptr,  // expose_configuration
};

// Forward declaration of plugin-descriptors (builtin plugins).
extern "C" mysql_harness::Plugin harness_plugin_mysql_rest_service;
extern "C" mysql_harness::Plugin harness_plugin_io;
extern "C" mysql_harness::Plugin harness_plugin_http_server;
extern "C" mysql_harness::Plugin harness_plugin_routing;
extern "C" mysql_harness::Plugin harness_plugin_host_cache;
// extern "C" mysql_harness::Plugin harness_plugin_router_protobuf;
extern "C" mysql_harness::Plugin harness_plugin_router_openssl;
extern "C" mysql_harness::Plugin harness_plugin_connection_pool;
extern "C" mysql_harness::Plugin harness_plugin_destination_status;

// DEBUG_SYNC declares service placeholders at its expansion scope. Keep this
// adapter in the global namespace so they bind the component placeholders.
static void mrs_debug_sync(const char *name [[maybe_unused]]) {
#if !defined(NDEBUG)
  MYSQL_THD thd = nullptr;
  if (mysql_service_mysql_current_thread_reader->get(&thd) != 0 ||
      thd == nullptr)
    return;

  DEBUG_SYNC(name);
#endif
}

namespace mrs_component {

constexpr unsigned int kCpusPerIoWorker = 4;

constexpr const char kMetadataSchemaCheckQuery[] = R"SQL(
  SELECT COUNT(*) AS item_count
  FROM information_schema.TABLES
  WHERE TABLE_SCHEMA = 'mysql_rest_service_metadata'
    AND TABLE_NAME = 'msm_schema_version'
    AND TABLE_TYPE = 'VIEW'
)SQL";
constexpr const char kMetadataSchemaVersionQuery[] =
    "SELECT major FROM mysql_rest_service_metadata.msm_schema_version";

void Backend::fetch_current_sec_ctx() {
  MYSQL_THD curr_thd = nullptr;
  Security_context_handle security_context{nullptr};

  MYSQL_LEX_CSTRING priv_user_{};
  MYSQL_LEX_CSTRING user_{};
  MYSQL_LEX_CSTRING priv_host_{};

  auto sec_ctx_cleanup = create_scope_guard([this] { m_sec_ctx = {}; });

  if (mysql_service_mysql_current_thread_reader->get(&curr_thd) != 0) {
    throw std::runtime_error("Failed to get current thread context");
  }

  // Get current thd security context and user/host
  if (thread_security_ctx->get(curr_thd, &security_context) != 0) {
    throw std::runtime_error("Failed to get current thread security context");
  }

  if (thread_security_ctx_opts->get(security_context, "priv_user",
                                    &priv_user_) != 0) {
    throw std::runtime_error("Failed to get priv_user from security context");
  }

  if (priv_user_.length != 0)
    m_sec_ctx.priv_user.assign(priv_user_.str, priv_user_.length);

  if (thread_security_ctx_opts->get(security_context, "priv_host",
                                    &priv_host_) != 0) {
    throw std::runtime_error("Failed to get priv_host from security context");
  }

  if (priv_host_.length != 0)
    m_sec_ctx.priv_hostname.assign(priv_host_.str, priv_host_.length);

  if (thread_security_ctx_opts->get(security_context, "user", &user_) != 0) {
    throw std::runtime_error("Failed to get user from security context");
  }

  if (user_.length != 0) m_sec_ctx.user.assign(user_.str, user_.length);

  sec_ctx_cleanup.release();
}

namespace {
using MRSAccount = RouterConfig::MRSAccount;

constexpr char internal_account_host[] = "localhost";
constexpr char provider_role_host[] = "%";

std::string format_auth_id(const std::string &username,
                           std::string_view host = internal_account_host) {
  mysqlrouter::sqlstring auth_id{"?@?"};
  auth_id << username << std::string{host};
  return auth_id;
}

bool account_is_locked(const MRSAccount &account) {
  MYSQL_THD current_thd = nullptr;
  if (mysql_service_mysql_current_thread_reader->get(&current_thd) != 0) {
    throw std::runtime_error("Failed to get current thread context");
  }

  const bool initialize_thread = current_thd == nullptr;
  if (initialize_thread && cmd_thread->init() != 0) {
    throw std::runtime_error("Failed to initialize MySQL command thread");
  }
  const auto command_thread_cleanup = create_scope_guard([initialize_thread] {
    if (initialize_thread) cmd_thread->end();
  });

  Security_context_handle security_context{nullptr};
  if (security_ctx_factory->create(&security_context) != 0 ||
      security_context == nullptr) {
    throw std::runtime_error(
        "Failed to create a security context for internal MRS account " +
        format_auth_id(account.username));
  }

  const auto security_context_cleanup = create_scope_guard([security_context] {
    (void)security_ctx_factory->destroy(security_context);
  });

  if (security_ctx_lookup->lookup(security_context, account.username.c_str(),
                                  internal_account_host, nullptr,
                                  nullptr) != 0) {
    throw std::runtime_error("Internal MRS account " +
                             format_auth_id(account.username) +
                             " cannot be resolved to a security context");
  }

  bool is_locked = false;
  if (thread_security_ctx_opts->get(security_context, "account_locked",
                                    &is_locked) != 0) {
    throw std::runtime_error(
        "Failed to get account lock state for internal MRS account " +
        format_auth_id(account.username));
  }

  return is_locked;
}

void validate_account_in_session(Command_executor &query_executor,
                                 const MRSAccount &account, bool is_locked) {
  // The fixed internal accounts are an execution boundary. Accept an existing
  // account only if it is locked, has exactly the provider role as default
  // without admin option, and has no direct privileges or extra direct roles.
  // Do not repair invalid accounts here; that could hide a privileged account
  // left behind by an operator or a previous component version.
  if (!is_locked) {
    throw std::runtime_error("Internal MRS account " +
                             format_auth_id(account.username) +
                             " must be locked");
  }

  // Inherited and mandatory roles are not account-specific grants. Validate
  // the provider role here and direct memberships through SHOW GRANTS below.
  query_executor.set_query(
      "SELECT GRANTEE, GRANTEE_HOST, ROLE_NAME, ROLE_HOST, IS_GRANTABLE, "
      "IS_DEFAULT "
      "FROM information_schema.applicable_roles");
  Resultset roles;
  query_executor.execute(&roles);

  bool provider_role_is_valid = false;
  bool role_result_is_valid = true;

  for (const auto &row : roles) {
    if (row.size() != 6) {
      role_result_is_valid = false;
      break;
    }

    if (row[0] == account.username && row[1] == internal_account_host &&
        row[2] == account.provider_role && row[3] == provider_role_host &&
        row[4] == "NO" && row[5] == "YES") {
      provider_role_is_valid = true;
    }
  }

  if (!role_result_is_valid || !provider_role_is_valid) {
    throw std::runtime_error(
        "Internal MRS account " + format_auth_id(account.username) +
        " must have " +
        format_auth_id(account.provider_role, provider_role_host) +
        " as its default role without admin option");
  }

  // Exclude privileges inherited from the default role so SHOW GRANTS exposes
  // only the account's direct privileges and role memberships.
  query_executor.set_query("SET ROLE NONE");
  query_executor.execute();

  query_executor.set_query("SHOW GRANTS FOR CURRENT_USER()");
  Resultset grants;
  query_executor.execute(&grants);

  const std::string expected_usage = "GRANT USAGE ON *.* TO `" +
                                     account.username + "`@`" +
                                     internal_account_host + "`";
  const std::string expected_role =
      "GRANT `" + account.provider_role + "`@`" + provider_role_host +
      "` TO `" + account.username + "`@`" + internal_account_host + "`";
  bool has_usage = false;
  bool has_role = false;
  bool grants_are_valid = true;

  for (const auto &row : grants) {
    if (row.size() != 1) {
      grants_are_valid = false;
      continue;
    }

    const auto &grant = row[0];
    if (grant == expected_usage) {
      if (has_usage) grants_are_valid = false;
      has_usage = true;
      continue;
    }

    if (grant == expected_role) {
      if (has_role) grants_are_valid = false;
      has_role = true;
      continue;
    }

    grants_are_valid = false;
    break;
  }

  query_executor.set_query("SET ROLE DEFAULT");
  query_executor.execute();

  if (!grants_are_valid || !has_usage || !has_role) {
    throw std::runtime_error(
        "Internal MRS account " + format_auth_id(account.username) +
        " must have only " +
        format_auth_id(account.provider_role, provider_role_host) +
        " as a direct role and no direct privileges");
  }
}

void validate_account(const MRSAccount &account) {
  // account_is_locked() owns a temporary command thread, so it must finish
  // before opening the account's command-service session.
  const bool is_locked = account_is_locked(account);
  auto query_executor = Command_executor::make_with_explicit_ctx(
      account.username, internal_account_host, Command_executor::Mode::Query);
  validate_account_in_session(query_executor, account, is_locked);
}

bool create_account_if_missing(Command_executor &query_executor,
                               const MRSAccount &account,
                               bool defer_missing_provider_role) {
  bool created = false;

  try {
    mysqlrouter::sqlstring create_user_q{
        "CREATE USER ?@'localhost' DEFAULT ROLE ? ACCOUNT LOCK"};
    create_user_q << account.username << account.provider_role;
    query_executor.set_query(create_user_q);
    query_executor.execute();
    created = true;
  } catch (const std::runtime_error &) {
    if (query_executor.get_last_error_code() == ER_USER_DOES_NOT_EXIST &&
        defer_missing_provider_role) {
      MRS_LOG_INFO(
          "Provider role %s is not available yet; deferring creation of "
          "internal MRS accounts until the metadata schema is ready",
          format_auth_id(account.provider_role, provider_role_host).c_str());
      return false;
    }
    if (query_executor.get_last_error_code() != ER_CANNOT_USER) throw;
  }

  if (created) {
    MRS_LOG_INFO("Created internal MRS account %s",
                 format_auth_id(account.username).c_str());
  } else {
    MRS_LOG_INFO("Validating existing internal MRS account %s",
                 format_auth_id(account.username).c_str());
  }

  return true;
}

template <typename Operation>
int run_backend_operation(const char *name, Operation &&operation) noexcept {
  // Lifecycle operations are reached from component and UDF C callbacks. Do
  // not let a C++ exception cross those ABI boundaries.
  try {
    return std::forward<Operation>(operation)();
  } catch (const std::exception &e) {
    MRS_LOG_ERROR("Backend %s failed: %s", name, e.what());
  } catch (...) {
    MRS_LOG_ERROR("Backend %s failed with an unknown exception", name);
  }

  return 1;
}

}  // namespace

bool Backend::provision_accounts(
    bool ignore_missing_provider_role_error) const {
  // This session must close before validation opens its command-service
  // sessions because command services cannot nest.
  auto query_executor = Command_executor::make_with_explicit_ctx(
      get_current_sec_ctx().priv_user, get_current_sec_ctx().priv_hostname,
      Command_executor::Mode::Query);

  if (!create_account_if_missing(query_executor,
                                 m_router_config.metadata_account,
                                 ignore_missing_provider_role_error)) {
    return false;
  }

  return create_account_if_missing(query_executor, m_router_config.data_account,
                                   ignore_missing_provider_role_error);
}

void Backend::configure_accounts(bool retry_account_provisioning,
                                 bool validate_metadata_account) {
  const auto &metadata_account = m_router_config.metadata_account;
  const auto &data_account = m_router_config.data_account;

  if (retry_account_provisioning) {
    provision_accounts(false);  // Don't ignore ER_USER_DOES_NOT_EXIST now
    m_deferred_account_provisioning_context.reset();
  }

  if (validate_metadata_account) validate_account(metadata_account);
  validate_account(data_account);
}

void Backend::init_builtins() {
  const char *k_mrs_plugin_name = "mysql_rest_service";
  auto &builtins = mysql_harness::BuiltinPlugins::instance();

  if (builtins.has(k_mrs_plugin_name)) {
    // Builtins already registered.
    return;
  }

  builtins.add(k_mrs_plugin_name, {&harness_plugin_mysql_rest_service, false});
  builtins.add("io", {&harness_plugin_io, false});
  builtins.add("http_server", {&harness_plugin_http_server, false});
  builtins.add("host_cache", {&harness_plugin_host_cache, false});
  builtins.add("routing", {&harness_plugin_routing, false});
  builtins.add("router_protobuf", {&harness_plugin_router_protobuf, false});
  builtins.add("router_openssl", {&harness_plugin_router_openssl, false});
  builtins.add("connection_pool", {&harness_plugin_connection_pool, false});
  builtins.add("destination_status",
               {&harness_plugin_destination_status, false});
}

uint64_t Backend::populate_mrs_metadata() const {
  /**
   * Check if the entry already exists in the Metadata first, because for
   * example the component was restarted, and if so, obtain it's ID. If the
   * router does not exist yet we need to "bootstrap" it and insert a new entry
   * in the table.
   *
   * Info:
   *
   *  router_name = "@@hostname":"http_port"
   *  address = "@@hostname"
   *  product_name = "MySQL Rest Service Component"
   *  version = current
   *  attributes = {"server_uuid": "@@server_uuid", "mrs_metadata_accounts":
   *  metadata_account, "mrs_data_accounts": data_account}
   *  options = {}
   */

  uint64_t id = 0;

  const auto &server_configs = mrs_component().server_config();
  const auto server_uuid = server_configs.m_server_uuid;
  const std::string metadata_account =
      m_router_config.metadata_account.username + "@localhost";
  const std::string data_account =
      m_router_config.data_account.username + "@localhost";

  const std::string router_name = m_router_config.name;
  const std::string address = server_configs.m_hostname;
  const std::string product_name = "MySQL Rest Service Component";

  // Check if the this entity already exists
  std::optional<uint64_t> existing_id = std::nullopt;
  {
    Command_executor exec = Command_executor::make_with_explicit_ctx(
        get_current_sec_ctx().priv_user, get_current_sec_ctx().priv_hostname,
        Command_executor::Mode::Query);

    mysqlrouter::sqlstring select_router_q{
        "SELECT id FROM mysql_rest_service_metadata.router WHERE "
        "JSON_EXTRACT(attributes, '$.server_uuid') = ?"};
    select_router_q << server_uuid;

    exec.set_query(select_router_q);

    Resultset rs;
    exec.execute(&rs);

    if (!rs.empty() && !rs[0].empty()) existing_id = std::stoull(rs[0][0]);
  }

  Command_executor exec_sp = Command_executor::make_with_explicit_ctx(
      get_current_sec_ctx().priv_user, get_current_sec_ctx().priv_hostname,
      Command_executor::Mode::PreparedStatement);

  // Found, update the router name, address, and MRS accounts.
  if (existing_id.has_value()) {
    const std::string update_query =
        "UPDATE mysql_rest_service_metadata.router SET router_name = ?, "
        "address = ?, "
        "attributes = JSON_SET(attributes, "
        "'$.mrs_metadata_accounts', JSON_ARRAY(?), "
        "'$.mrs_data_accounts', JSON_ARRAY(?)), "
        "last_check_in = NOW() WHERE id = ?";

    exec_sp.set_query(update_query);

    int bind_index = 0;
    exec_sp.bind_param(bind_index++, router_name, MYSQL_SP_ARG_TYPE_VARCHAR);
    exec_sp.bind_param(bind_index++, address, MYSQL_SP_ARG_TYPE_VARCHAR);
    exec_sp.bind_param(bind_index++, metadata_account,
                       MYSQL_SP_ARG_TYPE_VARCHAR);
    exec_sp.bind_param(bind_index++, data_account, MYSQL_SP_ARG_TYPE_VARCHAR);
    exec_sp.bind_param(bind_index++, *existing_id, MYSQL_SP_ARG_TYPE_LONGLONG,
                       true);

    exec_sp.execute();

    return *existing_id;
  }

  // Router not found, insert it
  const std::string insert_query =
      "INSERT INTO mysql_rest_service_metadata.router (router_name, address, "
      "product_name, version, last_check_in, attributes, options) VALUES (?, "
      "?, ?, ?, NOW(), JSON_OBJECT('server_uuid', ?, "
      "'mrs_metadata_accounts', JSON_ARRAY(?), 'mrs_data_accounts', "
      "JSON_ARRAY(?)), '{}')";

  exec_sp.set_query(insert_query);

  int bind_index = 0;
  exec_sp.bind_param(bind_index++, router_name, MYSQL_SP_ARG_TYPE_VARCHAR);
  exec_sp.bind_param(bind_index++, address, MYSQL_SP_ARG_TYPE_VARCHAR);
  exec_sp.bind_param(bind_index++, product_name, MYSQL_SP_ARG_TYPE_VARCHAR);
  exec_sp.bind_param(bind_index++, MYSQL_ROUTER_VERSION,
                     MYSQL_SP_ARG_TYPE_VARCHAR);
  exec_sp.bind_param(bind_index++, server_uuid, MYSQL_SP_ARG_TYPE_VARCHAR);
  exec_sp.bind_param(bind_index++, metadata_account, MYSQL_SP_ARG_TYPE_VARCHAR);
  exec_sp.bind_param(bind_index++, data_account, MYSQL_SP_ARG_TYPE_VARCHAR);

  exec_sp.execute();

  id = exec_sp.last_insert_id();
  return id;
}

void Backend::enable_lifecycle_operations() {
  std::lock_guard lock(m_lifecycle_state_mtx);
  m_lifecycle_admission_open = true;
}

void Backend::log_disabled_lifecycle_operation_rejection(
    const char *operation) const {
  MRS_LOG_ERROR("Backend %s rejected because lifecycle operations are disabled",
                operation);
}

const char *Backend::lifecycle_state_name() const {
  std::lock_guard lock(m_lifecycle_state_mtx);

  switch (m_lifecycle_state) {
    case LifecycleState::kStopped:
      return "STOPPED";
    case LifecycleState::kStarting:
      return "STARTING";
    case LifecycleState::kWaitingForMetadata:
      return "WAITING_FOR_METADATA";
    case LifecycleState::kReady:
      return "READY";
    case LifecycleState::kStopping:
      return "STOPPING";
    case LifecycleState::kError:
      return "ERROR";
    default:
      assert(false);
      return "ERROR";
  }
}

int Backend::start(AccountProvisioning account_provisioning) {
  mrs_debug_sync("mrs_component_before_lifecycle_operation_lock");
  // Keep the operation lock for the complete request so concurrent lifecycle
  // callers wait and then reevaluate the resulting state.
  std::lock_guard operation_lock(m_lifecycle_operation_mtx);
#if !defined(NDEBUG)
  if (debug_keyword_service->lookup_debug_keyword(
          "mrs_component_resume_runtime_status_publication") != 0) {
    m_debug_resume_runtime_status_publication = true;
  }
#endif
  bool cleanup_failed_backend = false;

  {
    std::lock_guard state_lock(m_lifecycle_state_mtx);
    if (!m_lifecycle_admission_open) {
      log_disabled_lifecycle_operation_rejection("start");
      return 1;
    }

    switch (m_lifecycle_state) {
      case LifecycleState::kStarting:
        [[fallthrough]];
      case LifecycleState::kWaitingForMetadata:
        [[fallthrough]];
      case LifecycleState::kReady:
        // Starting or already running satisfies an idempotent start request.
        return 0;
      case LifecycleState::kStopped:
        m_lifecycle_state = LifecycleState::kStarting;
        break;
      case LifecycleState::kError:
        // A failed asynchronous start may have left joinable threads or
        // partially initialized runtime resources.
        cleanup_failed_backend = true;
        m_lifecycle_state = LifecycleState::kStopping;
        break;
      case LifecycleState::kStopping:
        MRS_LOG_ERROR("Backend start rejected while shutdown is in progress");
        return 1;
    }
  }

  if (cleanup_failed_backend) {
    const int stop_error =
        run_backend_operation("cleanup", [this] { return stop_backend(); });

    if (stop_error != 0) {
      std::lock_guard state_lock(m_lifecycle_state_mtx);
      if (m_lifecycle_admission_open) {
        m_lifecycle_state = LifecycleState::kError;
      }
      return 1;
    }

    {
      std::lock_guard state_lock(m_lifecycle_state_mtx);
      m_lifecycle_state = LifecycleState::kStopped;
      // shutdown() can disable lifecycle operations while waiting for the
      // operation lock. Never start a new backend after that point.
      if (!m_lifecycle_admission_open) return 1;
      m_lifecycle_state = LifecycleState::kStarting;
    }
  }

  const int start_error =
      run_backend_operation("start", [this, account_provisioning] {
        return start_backend(account_provisioning);
      });

  if (start_error != 0) report_backend_error();

  return start_error;
}

int Backend::restart() {
  mrs_debug_sync("mrs_component_before_lifecycle_operation_lock");
  // Restart owns both its synchronous stop and asynchronous start so another
  // lifecycle request cannot run between them.
  std::lock_guard operation_lock(m_lifecycle_operation_mtx);
  bool stop_first = false;

  {
    std::lock_guard state_lock(m_lifecycle_state_mtx);
    if (!m_lifecycle_admission_open) {
      log_disabled_lifecycle_operation_rejection("restart");
      return 1;
    }

    stop_first = m_lifecycle_state != LifecycleState::kStopped;
    m_lifecycle_state =
        stop_first ? LifecycleState::kStopping : LifecycleState::kStarting;
  }

  if (stop_first) {
    const int stop_error = run_backend_operation(
        "stop for restart", [this] { return stop_backend(); });

    if (stop_error != 0) {
      std::lock_guard state_lock(m_lifecycle_state_mtx);
      if (m_lifecycle_admission_open) {
        m_lifecycle_state = LifecycleState::kError;
      }
      return 1;
    }

    {
      std::lock_guard state_lock(m_lifecycle_state_mtx);
      m_lifecycle_state = LifecycleState::kStopped;
      // Component teardown may have disabled lifecycle operations while it
      // waited for the synchronous stop to complete.
      if (!m_lifecycle_admission_open) return 1;
      m_lifecycle_state = LifecycleState::kStarting;
    }
  }

  // Only the automatic component start may use the installer context.
  const int start_error = run_backend_operation("restart", [this] {
    return start_backend(AccountProvisioning::kValidateOnly);
  });

  if (start_error != 0) report_backend_error();

  return start_error;
}

int Backend::stop() {
  mrs_debug_sync("mrs_component_before_lifecycle_operation_lock");
  std::lock_guard operation_lock(m_lifecycle_operation_mtx);

  {
    std::lock_guard state_lock(m_lifecycle_state_mtx);
    if (!m_lifecycle_admission_open) {
      log_disabled_lifecycle_operation_rejection("stop");
      return 1;
    }

    if (m_lifecycle_state == LifecycleState::kStopped) {
      m_deferred_account_provisioning_context.reset();
      return 0;
    }
    m_lifecycle_state = LifecycleState::kStopping;
  }

  // Do not hold the state lock while cancellation and thread joins run.
  const int stop_error =
      run_backend_operation("stop", [this] { return stop_backend(); });

  {
    std::lock_guard state_lock(m_lifecycle_state_mtx);
    if (stop_error == 0) {
      m_lifecycle_state = LifecycleState::kStopped;
      m_deferred_account_provisioning_context.reset();
    } else if (m_lifecycle_admission_open) {
      // Once shutdown disables lifecycle operations, it owns the final state.
      m_lifecycle_state = LifecycleState::kError;
    }
  }

  return stop_error;
}

int Backend::shutdown() {
  {
    std::lock_guard state_lock(m_lifecycle_state_mtx);
    // Close admission before waiting for an in-flight UDF operation. Pending
    // and new lifecycle requests will then fail instead of restarting MRS.
    m_lifecycle_admission_open = false;
    if (m_lifecycle_state != LifecycleState::kStopped) {
      m_lifecycle_state = LifecycleState::kStopping;
    }
  }

  mrs_debug_sync("mrs_component_after_lifecycle_admission_closed");
  // The state lock must not be held while waiting for the operation owner or
  // while cancelling and joining backend threads.
  std::lock_guard operation_lock(m_lifecycle_operation_mtx);

  {
    std::lock_guard state_lock(m_lifecycle_state_mtx);
    // The in-flight lifecycle operation may already have stopped the backend.
    if (m_lifecycle_state == LifecycleState::kStopped) {
      m_deferred_account_provisioning_context.reset();
      return 0;
    }
    m_lifecycle_state = LifecycleState::kStopping;
  }

  const int stop_error =
      run_backend_operation("shutdown", [this] { return stop_backend(); });

  {
    std::lock_guard state_lock(m_lifecycle_state_mtx);
    // Failed cleanup may leave joinable threads or live runtime resources.
    // Keep STOPPING so a later component deinitialization retries cleanup.
    if (stop_error == 0) m_lifecycle_state = LifecycleState::kStopped;
  }

  if (stop_error == 0) m_deferred_account_provisioning_context.reset();

  return stop_error;
}

void Backend::report_waiting_for_metadata() {
  std::lock_guard lock(m_lifecycle_state_mtx);
  // Ignore a late report after another lifecycle operation superseded startup.
  if (m_lifecycle_state == LifecycleState::kStarting) {
    m_lifecycle_state = LifecycleState::kWaitingForMetadata;
  }
}

void Backend::report_metadata_ready() {
  std::lock_guard lock(m_lifecycle_state_mtx);
  if (m_lifecycle_state == LifecycleState::kWaitingForMetadata) {
    // Metadata is ready, but listener startup is still in progress. READY is
    // published only by on_mrs_start().
    m_lifecycle_state = LifecycleState::kStarting;
  }
}

void Backend::report_backend_error() {
  std::lock_guard lock(m_lifecycle_state_mtx);
  // Expected termination during stop must not overwrite STOPPING or STOPPED.
  if (m_lifecycle_state != LifecycleState::kStopping &&
      m_lifecycle_state != LifecycleState::kStopped) {
    m_lifecycle_state = LifecycleState::kError;
  }
}

int Backend::start_backend(AccountProvisioning account_provisioning) {
  init_builtins();

  if (!mrs_component().initialized()) {
    MRS_LOG_ERROR(
        "Failed to init Backend: MRS_Component main handler is not "
        "initialized");
    return 1;
  }

  if (m_service_thread.joinable()) {
    MRS_LOG_ERROR("Backend service thread already running");
    return 1;
  }

  mrs_component().status_vars().clear_current_runtime_values();

  // Reset the thread signal only when a new backend can be started
  m_backend_thd_stop_requested = false;
#if !defined(NDEBUG)
  // Debug keywords are session-local, so capture this before starting the
  // backend threads.
  m_debug_resume_runtime_status_publication = false;
  m_debug_pause_before_runtime_status_publication =
      debug_keyword_service->lookup_debug_keyword(
          "mrs_component_pause_before_runtime_status_publication") != 0;
  m_debug_pause_before_account_provisioning =
      debug_keyword_service->lookup_debug_keyword(
          "mrs_component_pause_before_account_provisioning") != 0;
#endif

  // Set the Application type
  mysql_harness::ApplicationTypeManager::instance().set(
      mysql_harness::kComponentApplicationTypeName);

  // Init the componentlog sink
  ComponentlogHandler::init_log_service(log_bi, log_bs);

  const auto &server_configs = mrs_component().server_config();
  const auto configured_values =
      mrs_component().system_vars().get_configured_values();

  bool has_bootstrap_context = false;

  // Get current thd security context and user/host
  try {
    m_sec_ctx = {};
    fetch_current_sec_ctx();
    has_bootstrap_context =
        account_provisioning == AccountProvisioning::kAllowInstallerContext &&
        !m_sec_ctx.user.empty();

    if (!has_bootstrap_context &&
        m_deferred_account_provisioning_context.has_value()) {
      // A lifecycle retry interrupted install-time account provisioning
      // while it was waiting for the metadata provider roles. Resume that
      // operation with the original installer's authority.
      m_sec_ctx = *m_deferred_account_provisioning_context;
      has_bootstrap_context = true;
    } else if (!has_bootstrap_context) {
      // Ordinary server startup and lifecycle UDFs use the fixed metadata
      // identity and only validate accounts provisioned during installation.
      m_sec_ctx.priv_user = m_router_config.metadata_account.username;
      m_sec_ctx.priv_hostname = internal_account_host;
    }

    MRS_LOG_DEBUG("Current context user is '%s'",
                  get_current_sec_ctx().priv_user.c_str());
    MRS_LOG_DEBUG("Current context host is '%s'",
                  get_current_sec_ctx().priv_hostname.c_str());
  } catch (const std::exception &) {
    MRS_LOG_ERROR("Failed to get current THD security context");
  }

  // Set member variables
  m_log_level = log_level_from_string(server_configs.m_log_level);

  /**
   * Set the local_address:
   *
   * 1) Prefer socket if available: fetch the socket full path
   * 2) Fallback to 127.0.0.1 otherwise.
   *
   * On Windows, support for named_pipes is not available we need to fallback to
   * tcp/ip.
   */
  m_router_config.local_address = [&]() -> std::string {
    auto datadir = server_configs.m_datadir;
    auto socket = server_configs.m_socket;

#ifdef _WIN32
    // On Windows: no UNIX sockets and no support for named-pipes. Fallback to
    // tcp/ip.
    return "127.0.0.1:" + server_configs.m_port;
#else
    // On Unix-like systems, prefer socket if available

    // If socket is empty, fallback to tcp/ip
    if (socket.empty()) {
      return "127.0.0.1:" + server_configs.m_port;
    }

    // If socket is not an absolute path assume it's relative to datadir
    std::filesystem::path sock_path = socket;

    if (!sock_path.is_absolute()) {
      sock_path = std::filesystem::path(datadir) / sock_path;
    }

    return "local:" + sock_path.string();
#endif
  }();

  // TLS is mandatory for the REST listener.
  m_router_config.ssl_cert = configured_values.ssl_cert;
  m_router_config.ssl_key = configured_values.ssl_key;
  m_router_config.ssl_cipher = configured_values.ssl_cipher;
  m_router_config.io_worker_threads = configured_values.io_worker_threads;
  if (m_router_config.io_worker_threads == 0) {
    // Component mode shares CPU with mysqld, so automatic sizing uses one
    // worker per four logical CPUs instead of Router's one-per-CPU default.
    const auto available_cpus =
        std::max(std::thread::hardware_concurrency(), 1U);
    m_router_config.io_worker_threads =
        1U + (available_cpus - 1U) / kCpusPerIoWorker;
  }

  const auto http_port = configured_values.http_port;
  m_router_config.http_port = std::to_string(http_port);

  // Set the router name - @@hostname:http_port
  m_router_config.name =
      server_configs.m_hostname + ":" + std::to_string(http_port);

  // Promise to signal when backend init completes (before loader_->start())
  std::promise<int> init_result;
  auto init_future = init_result.get_future();

  // Start the service thread using the stored configuration
  m_service_thread = std::thread(
      [this, has_bootstrap_context, p = std::move(init_result)]() mutable {
        try {
          // run() does the initialization of the backend, spawns another thread
          // enters a long-running loop and returns. Pass along the promise.
          run(std::move(p), has_bootstrap_context);
        } catch (const std::exception &e) {
          // If run() fails during initialization log the error
          MRS_LOG_ERROR("Backend startup failed: %s", e.what());
          report_backend_error();
        }
      });

  // Wait for run() to complete minimal init (right before spawning the md
  // schema checker)
  return init_future.get();
}

mysql_harness::DIM &Backend::init_DIM() {
  static mysql_harness::logging::Registry static_registry;

  mysql_harness::DIM &dim = mysql_harness::DIM::instance();

  // logging facility
  dim.set_static_LoggingRegistry(&static_registry);

  // Random generator
  static mysql_harness::RandomGenerator rg;
  dim.set_RandomGenerator(&rg,
                          [](mysql_harness::RandomGeneratorInterface *) {});

  MRS_LOG_INFO("DIM successfully initialized");

  return dim;
}

void Backend::reset_process_state_component() const {
  auto &proc_state = mysql_harness::ProcessStateComponent::get_instance();

  // Ensure current callbacks queue is clean
  proc_state.clear();

  // Ensure no callbacks are left in memory, Reason::NONE is a no-op
  proc_state.request_application_shutdown(
      mysql_harness::ShutdownPending::Reason::NONE,
      "Clearing the callbacks queue");
}

void Backend::setup_logger(mysql_harness::DIM &dim) const {
  mysql_harness::logging::Registry &registry = dim.get_LoggingRegistry();

  const auto loglevel = mysql_harness::logging::LogLevel::kNotSet;

  mysql_harness::logging::clear_registry(registry);
  mysql_harness::logging::create_module_loggers(
      registry, loglevel, {mysql_harness::logging::kMainLogger, "http_server"},
      mysql_harness::logging::kMainLogger);
  mysql_harness::logging::create_main_log_handler(
      registry, "mrs_component", m_router_config.logging_folder, true);

  registry.set_ready();
}

void Backend::configure_loader(mysql_harness::DIM &dim) const {
  // Configure up the loader
  auto loader_config = std::make_unique<mysql_harness::LoaderConfig>(
      mysql_harness::Config::allow_keys);

  // [DEFAULT] section
  loader_config->set_default("name", m_router_config.name);
  loader_config->set_default("logging_folder", m_router_config.logging_folder);

  // Those are unused but must be set
  loader_config->set_default("config_folder", "");
  loader_config->set_default("runtime_folder", "");
  loader_config->set_default("data_folder", "");
  loader_config->set_default("plugin_folder", "");

  const auto &server_configs = mrs_component().server_config();

  // [logger] section
  loader_config->add("logger");
  auto &logger_conf = loader_config->get("logger", "");

  // Use only the componentlog sink
  logger_conf.add("sinks", "componentlog");

  loader_config->add("componentlog");
  auto &componentlog_conf = loader_config->get("componentlog", "");

  // Get @@log_error_verbosity and set [componentlog] level to its value
  // accordingly
  componentlog_conf.add("level", to_string(m_log_level));

  // Setup the plugins

  // [io] section
  auto &io_config = loader_config->add("io");
  io_config.add("library", "io");
  io_config.add("threads", std::to_string(m_router_config.io_worker_threads));

  // [host_cache] section
  auto &host_cache_config = loader_config->add("host_cache");
  host_cache_config.add("library", "host_cache");

  // [http] section
  auto &http_server_config = loader_config->add("http_server", "");
  http_server_config.set("library", "http_server");

  /**
   * Due to BUG#38080803 we need to validate the value of bind_address:
   *
   *   - If @@bind_address=[set_of_addresses] - Error out, unsupported
   *   - If @@bind_address=* OR @@bind_address=:: : Log that needs dual-stack
   *   - Any other value remains unchanged.
   */
  std::string resolved_address = [&]() -> std::string {
    auto addr = server_configs.m_bind_address;

    if (addr.find(',') != std::string::npos) {
      throw std::runtime_error(
          "Multiple bind addresses are not supported: @@bind_address = '" +
          addr + "'");
    }

    if (addr == "*" || addr == "::") {
      MRS_LOG_INFO(
          "@@bind_address='%s' requires dual-stack support (IPv4 and IPv6)",
          addr.c_str());

      //  Due to a bug in Router, in Windows, Solaries and MacOS, the HTTP
      //  plugin
      //  binds to the loopback address when set to the '*' wildcard
      //  TODO(miguel): Remove this as soon as the bug in router is fixed.
#if defined(_WIN32) || defined(__sun) || defined(__APPLE__)
      addr = "0.0.0.0";
#endif
    }

    return addr;
  }();

  MRS_LOG_INFO("Setting HTTP bind_address: '%s'", resolved_address.c_str());
  http_server_config.set("bind_address", resolved_address);

  MRS_LOG_INFO("Setting HTTP port: '%s'", m_router_config.http_port.c_str());
  http_server_config.set("port", m_router_config.http_port);
  http_server_config.set("static_folder", "");

  http_server_config.set("ssl", "1");

  MRS_LOG_DEBUG("Setting ssl_cert: '%s'", m_router_config.ssl_cert.c_str());
  http_server_config.set("ssl_cert", m_router_config.ssl_cert);

  MRS_LOG_DEBUG("Setting ssl_key: '%s'", m_router_config.ssl_key.c_str());
  http_server_config.set("ssl_key", m_router_config.ssl_key);

  if (!m_router_config.ssl_cipher.empty()) {
    MRS_LOG_DEBUG("Setting ssl_cipher: '%s'",
                  m_router_config.ssl_cipher.c_str());
    http_server_config.set("ssl_cipher", m_router_config.ssl_cipher);
  }

  // [routing:] sections
  auto add_routing_section = [&](const std::string &name) -> decltype(auto) {
    auto &section = loader_config->add("routing", name);
    section.set("library", "routing");
    MRS_LOG_INFO("Setting destination '%s' for: '%s'",
                 m_router_config.local_address.c_str(), name.c_str());
    section.set("destinations", m_router_config.local_address);
    section.set("routing_strategy", "round-robin");
    section.set("protocol", "classic");
    section.set("router_require_enforce", "0");
    section.set("accept_external_connections", "0");
    return section;
  };

  add_routing_section("mrs_rw");
  add_routing_section("mrs_ro");

  // [mysql_rest_service] section
  auto &mrs = loader_config->add("mysql_rest_service", "");
  mrs.set("library", "mysql_rest_service");
  mrs.set("mysql_read_only_route", "mrs_ro");
  mrs.set("mysql_read_write_route", "mrs_rw");
  MRS_LOG_INFO("Setting mysql_user: '%s'",
               m_router_config.metadata_account.username.c_str());
  mrs.set("mysql_user", m_router_config.metadata_account.username);
  MRS_LOG_INFO("Setting mysql_user_data_access: '%s'",
               m_router_config.data_account.username.c_str());
  mrs.set("mysql_user_data_access", m_router_config.data_account.username);
  mrs.set("router_id", std::to_string(m_router_config.router_id));

  loader_config->add("router_protobuf", "");
  loader_config->add("router_openssl", "");
  loader_config->add("connection_pool", "");
  loader_config->add("destination_status", "");
  dim.set_Config(loader_config.release(),
                 std::default_delete<mysql_harness::LoaderConfig>());
}

void Backend::install_embedded_runtime_integration() {
  auto &rg = mysql_harness::DIM::instance().get_RandomGenerator();
  // Component-mode JWTs are intentionally restart-scoped. A fresh runtime
  // secret invalidates existing sessions when the embedded backend restarts.
  m_embedded_runtime_services =
      std::make_unique<ComponentEmbeddedRuntimeServices>(
          m_router_config.local_address,
          m_router_config.metadata_account.username,
          m_router_config.data_account.username,
          rg.generate_strong_password(64));
  mrs::set_embedded_runtime_services(m_embedded_runtime_services.get());
  mrs::set_mrs_start_observer(this);
}

mrs::MrsStartResult Backend::on_mrs_start() {
#if !defined(NDEBUG)
  if (m_debug_pause_before_runtime_status_publication) {
    MRS_LOG_WARNING("Paused before publishing current runtime values");
    while (!m_backend_thd_stop_requested &&
           !m_debug_resume_runtime_status_publication) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    if (m_backend_thd_stop_requested) {
      return mrs::MrsStartResult::kCancelled;
    }
  }
#endif
  // Keep cancellation validation, runtime-value publication, and the READY
  // transition atomic with stop and shutdown state changes.
  std::lock_guard lifecycle_lock(m_lifecycle_state_mtx);
  // A readiness callback from a cancelled or superseded startup must not
  // publish active values or move the lifecycle back to READY.
  if (m_backend_thd_stop_requested || !m_lifecycle_admission_open ||
      (m_lifecycle_state != LifecycleState::kStarting &&
       m_lifecycle_state != LifecycleState::kWaitingForMetadata)) {
    return mrs::MrsStartResult::kCancelled;
  }

  mrs_component().system_vars().apply_http_limits();

  const auto current_tls_cipher =
      m_router_config.ssl_cipher.empty()
          ? mysql_harness::join(TlsServerContext::default_ciphers(), ":")
          : m_router_config.ssl_cipher;
  const auto current_io_worker_threads =
      IoComponent::get_instance().io_threads().size();

  mrs_component().status_vars().publish_current_runtime_values(
      std::stoul(m_router_config.http_port), m_router_config.ssl_cert,
      m_router_config.ssl_key, current_tls_cipher,
      static_cast<unsigned long>(current_io_worker_threads));

  // Publish the complete active-value snapshot before exposing READY.
  m_lifecycle_state = LifecycleState::kReady;
  MRS_LOG_SYSTEM(
      "MySQL REST Service is ready and accepting connections on port %s.",
      m_router_config.http_port.c_str());

  MRS_LOG_INFO(
      "HTTP listener TLS protocol policy is TLSv1.2 and TLSv1.3 when "
      "supported; effective TLSv1.2 cipher policy is '%s'",
      current_tls_cipher.c_str());

  return mrs::MrsStartResult::kStarted;
}

void Backend::reset_embedded_runtime_integration() {
  mrs::set_mrs_start_observer(nullptr);
  mrs::set_embedded_runtime_services(nullptr);
  m_embedded_runtime_services.reset();
}

void Backend::breakable_sleep(std::chrono::seconds duration) const {
  while (duration > std::chrono::seconds::zero() &&
         !m_backend_thd_stop_requested) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
    --duration;
  }
}

bool Backend::wait_for_metadata_schema(const std::string &user,
                                       const std::string &host,
                                       bool validate_metadata_account) {
  bool waiting_message_already_shown = false;
  int connect_failures = 0;

  while (!m_backend_thd_stop_requested) {
    bool account_validation_in_progress = false;
    bool metadata_account_is_locked = false;

    // Distinguish a missing account from transient command-service readiness
    // failures, which remain retryable during server startup.
    if (validate_metadata_account) {
      metadata_account_is_locked =
          account_is_locked(m_router_config.metadata_account);
    }

    try {
      auto cmd_executor = Command_executor::make_with_explicit_ctx(
          user, host, Command_executor::Mode::Query);

      if (validate_metadata_account) {
        account_validation_in_progress = true;
        validate_account_in_session(cmd_executor,
                                    m_router_config.metadata_account,
                                    metadata_account_is_locked);
        account_validation_in_progress = false;
        validate_metadata_account = false;
      }

      cmd_executor.set_query(kMetadataSchemaCheckQuery);

      Resultset rs;
      cmd_executor.execute(&rs);

      if (!rs.empty() && !rs[0].empty() && rs[0][0] == "1") {
        MRS_LOG_DEBUG("Metadata schema detected, checking readiness...");

        cmd_executor.set_query(kMetadataSchemaVersionQuery);

        Resultset rs_inner;
        cmd_executor.execute(&rs_inner);

        // Check if the schema version is not 0.0.0 which means the deployment
        // of it is not complete yet
        if (!rs_inner.empty() && !rs_inner[0].empty() &&
            rs_inner[0][0] != "0") {
          MRS_LOG_INFO("Metadata schema ready.");
          report_metadata_ready();
          return true;
        }
      }
      connect_failures = 0;
      report_waiting_for_metadata();
      if (!waiting_message_already_shown) {
        waiting_message_already_shown = true;
        MRS_LOG_SYSTEM("Waiting for MySQL REST Service metadata schema.");
      }
      MRS_LOG_DEBUG("Metadata schema not detected, retrying in 30s...");
      breakable_sleep(std::chrono::seconds(30));
    } catch (const std::exception &e) {
      if (m_backend_thd_stop_requested) return false;

      if (account_validation_in_progress) throw;

      if (const auto *connection_error =
              dynamic_cast<const Command_executor::Connection_error *>(&e);
          connection_error != nullptr) {
        MRS_LOG_DEBUG("Metadata connection failed: %s (%u)", e.what(),
                      connection_error->error_code());

        if (++connect_failures == 3) {
          throw std::runtime_error(
              "Unable to connect to MySQL after 3 attempts. Aborting backend "
              "startup. Please ensure the internal MRS accounts are valid or "
              "restart the component.");
        }
      } else {
        MRS_LOG_ERROR("Metadata check failed: %s", e.what());
        connect_failures = 0;
      }

      breakable_sleep(std::chrono::seconds(10));
    }
  }

  return false;
}

void Backend::run(std::promise<int> p, bool has_bootstrap_context) {
  std::lock_guard<std::mutex> guard(m_loader_create_mtx);

  std::promise<void> loader_created_promise;
  auto loader_created_future = loader_created_promise.get_future();

  try {
    m_loader.reset();
    // Ensure a clean process state
    reset_process_state_component();

    // Init the DIM
    auto &dim = init_DIM();

    // Setup the logger
    setup_logger(dim);

    if (has_bootstrap_context) {
      // Preserve the installer identity before minimal startup is reported.
      // A lifecycle restart may cancel the worker immediately afterwards.
      m_deferred_account_provisioning_context = m_sec_ctx;
    }

    // Signal successful minimal startup so init() can return early
    p.set_value(0);

    std::string current_ctx_user = get_current_sec_ctx().priv_user;
    std::string current_ctx_host = get_current_sec_ctx().priv_hostname;

    m_backend_starter_thread = std::thread(
        [this, &dim, current_ctx_user, current_ctx_host, has_bootstrap_context,
         loader_created = std::move(loader_created_promise)]() mutable {
          auto loader_created_guard = create_scope_guard(
              [&loader_created]() { loader_created.set_value(); });

          const auto abort_startup_if_requested = [this]() {
            if (!m_backend_thd_stop_requested) return false;

            MRS_LOG_DEBUG("Backend thread aborted");
            reset_embedded_runtime_integration();
            m_loader.reset();
            return true;
          };

          const auto validate_tls_configuration = [this]() {
            const auto validate_file = [](std::string_view description,
                                          const std::string &path) {
              std::error_code ec;
              if (std::filesystem::is_regular_file(path, ec)) return;

              throw std::runtime_error(
                  "TLS initialization failed because the configured SSL " +
                  std::string{description} + " file '" + path +
                  "' does not exist, is not a regular file, or cannot be "
                  "accessed. Configure mysql_rest_service.ssl_cert and "
                  "mysql_rest_service.ssl_key, then restart "
                  "the service");
            };

            if (m_router_config.ssl_cert.empty() ||
                m_router_config.ssl_key.empty()) {
              throw std::runtime_error(
                  "TLS initialization failed because no usable SSL "
                  "certificate and private key are configured. Configure "
                  "mysql_rest_service.ssl_cert and mysql_rest_service.ssl_key, "
                  "then restart the "
                  "service");
            }

            validate_file("certificate", m_router_config.ssl_cert);
            validate_file("private key", m_router_config.ssl_key);
          };

          try {
#if !defined(NDEBUG)
            while (m_debug_pause_before_account_provisioning &&
                   !m_backend_thd_stop_requested) {
              std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
#endif
            if (abort_startup_if_requested()) return;

            if (!has_bootstrap_context) validate_tls_configuration();

            // Only installation, including continuation of its pending
            // provisioning, may create missing fixed accounts.
            bool accounts_provisioned = false;
            if (has_bootstrap_context) {
              // Schema bootstrap can race with component installation. Treat
              // ER_USER_DOES_NOT_EXIST as temporary, wait for schema readiness,
              // then retry account provisioning.
              accounts_provisioned = provision_accounts(true);
              if (accounts_provisioned) {
                m_deferred_account_provisioning_context.reset();
              }
            }

            if (abort_startup_if_requested()) return;

            if (!wait_for_metadata_schema(current_ctx_user, current_ctx_host,
                                          !has_bootstrap_context)) {
              (void)abort_startup_if_requested();
              return;
            }

            if (abort_startup_if_requested()) return;

            const bool retry_account_provisioning =
                has_bootstrap_context && !accounts_provisioned;
            const bool validate_metadata_account = has_bootstrap_context;

            configure_accounts(retry_account_provisioning,
                               validate_metadata_account);

            // INSTALL COMPONENT, including continuation of its deferred
            // provisioning, is the only path allowed to create the fixed
            // accounts. Complete provisioning before leaving MRS inactive so
            // configuring TLS and restarting can recover.
            if (has_bootstrap_context) validate_tls_configuration();

            if (abort_startup_if_requested()) return;

            // Populate the MRS metadata schema (router table)
            m_router_config.router_id = populate_mrs_metadata();

            if (abort_startup_if_requested()) return;

            install_embedded_runtime_integration();

            if (abort_startup_if_requested()) return;

            // Configure the loader
            configure_loader(dim);

            if (abort_startup_if_requested()) return;

            register_io_thread_instrumentation();

            auto &io_component = IoComponent::get_instance();
            io_component.set_thread_lifecycle(
                {initialize_io_thread_instrumentation,
                 deinitialize_io_thread_instrumentation});
            const auto io_thread_lifecycle_guard = create_scope_guard(
                [&io_component] { io_component.reset_thread_lifecycle(); });

            // Configure and start the loader
            m_loader = std::make_unique<mysql_harness::Loader>(
                "mrs_component", dim.get_Config(), true);

            m_loader->register_supported_app_options(router_supported_options);

            // When there are only builtins we need to call this manually.
            m_loader->setup_info();

            // Clear the active values after Loader stops.
            auto *status_vars = &mrs_component().status_vars();
            auto clear_current_values = create_scope_guard(
                [status_vars] { status_vars->clear_current_runtime_values(); });

            // TODO(miguel): Make Loader activation cancellation-aware. A
            // shutdown request after this check is observed only after plugins
            // start
            if (abort_startup_if_requested()) return;

            loader_created.set_value();
            loader_created_guard.release();

            // Start the loader: runs in background and does not return
            m_loader->start();

            // A normal return is expected only after stop_backend() requests
            // shutdown. Any other return is a fatal backend failure.
            if (!m_backend_thd_stop_requested) {
              MRS_LOG_ERROR(
                  "Backend terminated unexpectedly; MySQL REST Service "
                  "remains inactive.");
              reset_embedded_runtime_integration();
              report_backend_error();
            }
          } catch (const std::exception &e) {
            if (!abort_startup_if_requested()) {
              MRS_LOG_ERROR(
                  "Backend startup failed in background thread: %s; MySQL "
                  "REST Service remains inactive.",
                  e.what());
              reset_embedded_runtime_integration();
              report_backend_error();
            }
          }
        });
  } catch (const std::exception &e) {
    MRS_LOG_ERROR("Backend startup failed during initial setup: %s", e.what());
    report_backend_error();
    try {
      p.set_value(1);  // Ensure the promise is fulfilled on failure
    } catch (...) {
      // Do nothing: promise may already be satisfied or broken
    }

    return;
  }

  // Wait until Loader is ready to start, startup is cancelled, or it fails
  loader_created_future.wait();
}

int Backend::stop_backend() {
  // Request the stop of the backend_thread before the lock
  m_backend_thd_stop_requested = true;

  mrs_debug_sync("mrs_component_after_stop_requested");
  std::lock_guard<std::mutex> guard(m_loader_create_mtx);
  if (m_loader) {
#if !defined(NDEBUG)
    if (debug_keyword_service->lookup_debug_keyword(
            "mrs_component_shutdown_request_failure") != 0) {
      throw std::runtime_error("Injected backend shutdown request failure");
    }
#endif
    MRS_LOG_SYSTEM(
        "Stopping the MySQL REST Service from accepting new connections.");
    mysql_harness::ProcessStateComponent::get_instance()
        .request_application_shutdown(
            mysql_harness::ShutdownPending::Reason::REQUESTED,
            "Stopping MySQL REST service");
  } else {
    MRS_LOG_INFO("Loader already stopped");
  }

  if (m_backend_starter_thread.joinable()) {
    m_backend_starter_thread.join();
  }

  if (m_service_thread.joinable()) {
    m_service_thread.join();  // Wait for run() to exit
  } else {
    MRS_LOG_INFO("Backend thread not joinable: already stopped?");
  }

  reset_embedded_runtime_integration();

  m_loader.reset();

  // Reset the Application type
  mysql_harness::ApplicationTypeManager::instance().reset();

  return 0;
}

}  // namespace mrs_component
