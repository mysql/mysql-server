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

#ifndef MRS_COMPONENT_BACKEND_H
#define MRS_COMPONENT_BACKEND_H

#include <sys/types.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <thread>

#include "components/mysql_rest_service/mrs_component_runtime_services.h"
#include "dim.h"
#include "mrs/mrs_start_observer.h"
#include "mysql/harness/loader.h"
#include "mysql/harness/logging/logger.h"

namespace mrs_component {

enum class LogLevel : uint8_t { ERROR, WARNING, DEBUG };

inline std::string to_string(LogLevel level) {
  switch (level) {
    case LogLevel::ERROR:
      return "error";
    case LogLevel::WARNING:
      return "warning";
    case LogLevel::DEBUG:
      return "debug";
  }
  throw std::logic_error("Unknown log level");
}

inline LogLevel log_level_from_string(const std::string &level_str) {
  if (level_str == "1") return LogLevel::ERROR;
  if (level_str == "2") return LogLevel::WARNING;
  if (level_str == "3") return LogLevel::DEBUG;

  throw std::logic_error("Unexpected value for @@log_error_verbosity: " +
                         level_str);
}

struct RouterConfig {
  struct MRSAccount {
    std::string username;
    std::string provider_role;
  };

  std::string http_port{"0"};
  std::string logging_folder;  // Empty defaults to stdout
  std::string local_address;
  // metadata_account, used to access MRS metadata in
  // `mysql_rest_service_metadata`
  MRSAccount metadata_account{
      .username = "mysql_mrs_metadata",
      .provider_role = "mysql_rest_service_meta_provider"};

  // data_account: used to execute limited SQL operations needed to serve REST
  // endpoints
  MRSAccount data_account{.username = "mysql_mrs_data",
                          .provider_role = "mysql_rest_service_data_provider"};
  std::string ssl_cert;
  std::string ssl_key;
  std::string ssl_cipher;
  unsigned long io_worker_threads{0};
  uint64_t router_id = 0;
  std::string name;  // @@hostname:http_port
};

struct CurrentSecurityCtx {
  std::string priv_user, user;
  std::string priv_hostname;
};

enum class AccountProvisioning : std::uint8_t {
  // Normal lifecycle starts require the internal accounts to exist already.
  kValidateOnly,
  // The automatic install-time start may provision accounts using the
  // component installer's security context.
  kAllowInstallerContext,
};

/**
  Owns and controls the embedded MRS backend lifecycle.
*/
class Backend : private mrs::MrsStartObserver {
 public:
  Backend() = default;
  ~Backend() override = default;
  Backend(const Backend &src) = delete;
  Backend(Backend &&src) = delete;
  Backend &operator=(const Backend &src) = delete;
  Backend &operator=(Backend &&src) = delete;

  // Enables lifecycle requests after component initialization has registered
  // every SQL interface. shutdown() disables them before teardown.
  void enable_lifecycle_operations();

  // Accepts an asynchronous start. Repeated starts are idempotent.
  int start(AccountProvisioning account_provisioning);

  // Stops the current lifecycle synchronously, then accepts a new
  // asynchronous start.
  int restart();

  // Stops the current lifecycle synchronously. Repeated stops are idempotent.
  int stop();

  // Permanently disables lifecycle requests for this component instance and
  // synchronously releases the backend during component teardown.
  int shutdown();

  // Returns the coarse lifecycle state exposed by mysql_rest_service.state.
  [[nodiscard]] const char *lifecycle_state_name() const;

  CurrentSecurityCtx get_current_sec_ctx() const { return m_sec_ctx; }

 private:
  // Coarse externally visible lifecycle state. Internal startup stages and
  // individual metadata or request failures are intentionally not represented.
  enum class LifecycleState : std::uint8_t {
    kStopped,
    kStarting,
    kWaitingForMetadata,
    kReady,
    kStopping,
    kError,
  };

  // Raw runtime operations. Their callers serialize access with
  // m_lifecycle_operation_mtx.
  int start_backend(AccountProvisioning account_provisioning);
  int stop_backend();
  void log_disabled_lifecycle_operation_rejection(const char *operation) const;
  void report_waiting_for_metadata();
  void report_metadata_ready();
  void report_backend_error();

  mrs::MrsStartResult on_mrs_start() override;
  void fetch_current_sec_ctx();
  bool provision_accounts(bool ignore_missing_provider_role_error) const;
  void configure_accounts(bool retry_account_provisioning,
                          bool validate_metadata_account);
  uint64_t populate_mrs_metadata() const;
  void reset_process_state_component() const;
  void setup_logger(mysql_harness::DIM &dim) const;
  void configure_loader(mysql_harness::DIM &dim) const;
  void install_embedded_runtime_integration();
  void reset_embedded_runtime_integration();
  void breakable_sleep(std::chrono::seconds duration) const;
  bool wait_for_metadata_schema(const std::string &user,
                                const std::string &host,
                                bool validate_metadata_account);
  void run(std::promise<int> p, bool has_bootstrap_context);

  mysql_harness::DIM &init_DIM();
  void init_builtins();

  RouterConfig m_router_config;
  LogLevel m_log_level{LogLevel::ERROR};
  mysql_harness::logging::DomainLogger m_logger;

  // Serializes complete start, restart, stop, and shutdown operations.
  std::mutex m_lifecycle_operation_mtx;

  // Guards the externally visible state and lifecycle-admission flag. It is
  // released before backend cancellation and thread joins.
  mutable std::mutex m_lifecycle_state_mtx;
  LifecycleState m_lifecycle_state{LifecycleState::kStopped};
  bool m_lifecycle_admission_open{false};

  // Synchronizes Loader creation between run() and stop_backend().
  std::mutex m_loader_create_mtx;

  std::unique_ptr<mysql_harness::Loader> m_loader;
  std::unique_ptr<ComponentEmbeddedRuntimeServices> m_embedded_runtime_services;
  // Thread handling
  std::thread m_service_thread, m_backend_starter_thread;
  std::atomic<bool> m_backend_thd_stop_requested{false};
  // Retain the installer identity while install-time account provisioning is
  // pending. Lifecycle retries may continue that operation, but cannot provide
  // replacement provisioning authority.
  std::optional<CurrentSecurityCtx> m_deferred_account_provisioning_context;
#if !defined(NDEBUG)
  bool m_debug_pause_before_account_provisioning{false};
  // MTR synchronization hook. When enabled at backend start, pause the
  // MRS-ready callback until stop() requests shutdown or an idempotent start
  // explicitly releases it.
  bool m_debug_pause_before_runtime_status_publication{false};
  std::atomic<bool> m_debug_resume_runtime_status_publication{false};
#endif

  CurrentSecurityCtx m_sec_ctx;
};

}  // namespace mrs_component

#endif /* MRS_COMPONENT_BACKEND_H */
