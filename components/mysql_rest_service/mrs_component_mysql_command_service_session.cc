/* Copyright (c) 2026, Oracle and/or its affiliates.

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
GNU General Public License, version 2.0, for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "mrs_component_mysql_command_service_session.h"

#include <stdexcept>

#include "components/mysql_rest_service/mrs_component.h"
#include "sql/server_component/mysql_command_services_imp.h"

namespace mrs_component {

namespace {

thread_local unsigned int command_thread_ref_count = 0;

void acquire_command_thread() {
  if (command_thread_ref_count == 0) {
    if (cmd_thread->init() != 0) {
      throw std::runtime_error("Failed to initialize MySQL command thread");
    }
  }

  ++command_thread_ref_count;
}

void release_command_thread() noexcept {
  if (command_thread_ref_count == 0) return;

  --command_thread_ref_count;
  if (command_thread_ref_count == 0) {
    cmd_thread->end();
  }
}

void set_session_thread_account(MYSQL_H mysql_h, const std::string &user,
                                const std::string &host) {
  auto *mysql_handle = reinterpret_cast<Mysql_handle *>(mysql_h);
  if (mysql_handle == nullptr || mysql_handle->mysql == nullptr) return;

  auto *thd = static_cast<MYSQL_THD>(mysql_handle->mysql->thd);
  uint32_t processlist_id = 0;
  if (thd == nullptr || thd_attributes->get(thd, "thread_id", &processlist_id))
    return;

  auto *session_psi = PSI_THREAD_CALL(get_thread_by_id)(processlist_id);
  if (session_psi == nullptr) return;

  auto *worker_psi = PSI_THREAD_CALL(get_thread)();

  PSI_THREAD_CALL(set_thread)(session_psi);
  PSI_THREAD_CALL(set_thread_account)
  (user.data(), user.size(), host.data(), host.size());
  PSI_THREAD_CALL(set_thread)(worker_psi);
}

}  // namespace

void initialize_worker_command_thread() { acquire_command_thread(); }

void deinitialize_worker_command_thread() noexcept { release_command_thread(); }

Mysql_command_service_session::~Mysql_command_service_session() { close(); }

void Mysql_command_service_session::connect(const std::string &user,
                                            const std::string &host,
                                            bool initialize_thread,
                                            unsigned long extra_client_flags,
                                            bool bypass_max_connections) {
  connect_impl(user, host, initialize_thread, extra_client_flags,
               bypass_max_connections, nullptr);
}

void Mysql_command_service_session::connect_authenticated(
    const std::string &user, const mysql_harness::SecureString &password,
    unsigned long extra_client_flags) {
  constexpr bool initialize_thread = false;
  const char *host = "";

  // Select the transport before authentication so one HTTP login does not
  // consume two hourly connection admissions.
  const auto &config = mrs_component().server_config();
  if (config.m_port != "0") {
    // MRS rejects comma-separated bind_address values during startup,
    // so no address-list parsing is needed here.
    const auto &bind_address = config.m_bind_address;
    if (bind_address == "::" || bind_address == "::1") {
      host = "::1";
    } else if (bind_address == "*" || bind_address == "0.0.0.0" ||
               bind_address == "127.0.0.1") {
      host = "127.0.0.1";
    }
  }
#ifdef _WIN32
  if (host[0] == '\0') {
    close();
    throw std::runtime_error(
        "MySQL authentication requires a supported loopback TCP endpoint "
        "on Windows");
  }
#else
  // Unix sockets remain secure when MySQL TLS is unavailable.
  if (host[0] != '\0' && config.requires_local_authentication()) host = "";
#endif  // _WIN32

  connect_impl(user, host, initialize_thread, extra_client_flags,
               /*bypass_max_connections=*/false, &password);
}

void Mysql_command_service_session::connect_impl(
    const std::string &user, const std::string &host, bool initialize_thread,
    unsigned long extra_client_flags, bool bypass_max_connections,
    const mysql_harness::SecureString *password) {
  close();
  user_ = user;
  host_ = host;

  try {
    if (initialize_thread) {
      // TODO: Move command-thread ownership to the Router I/O component and
      // instrument its lifecycle there.
      acquire_command_thread();
      thread_initialized_ = true;
    }

    if (cmd_factory->init(&mysql_h_) != 0 || mysql_h_ == nullptr) {
      throw std::runtime_error("Failed to initialize MySQL command handle");
    }

    const bool no_lock_registry = true;
    const uint32_t client_flags = static_cast<uint32_t>(extra_client_flags);
    // Preserve the bounded timeouts used by the former MySQLSession path.
    constexpr unsigned int authenticated_connect_timeout = 5;
    constexpr unsigned int authenticated_read_timeout = 30;
    constexpr unsigned int authenticated_ssl_mode = SSL_MODE_REQUIRED;
    // Allow ML-DSA server certificates while retaining classical signatures.
    constexpr bool use_pqc_sign = true;
    if ((password == nullptr &&
         (cmd_options->set(mysql_h_, MYSQL_NO_LOCK_REGISTRY,
                           &no_lock_registry) != 0 ||
          cmd_options->set(mysql_h_, MYSQL_COMMAND_BYPASS_MAX_CONNECTIONS,
                           &bypass_max_connections) != 0)) ||
        cmd_options->set(mysql_h_, MYSQL_COMMAND_USER_NAME, user_.c_str()) !=
            0 ||
        ((password == nullptr || !host_.empty()) &&
         cmd_options->set(mysql_h_, MYSQL_COMMAND_HOST_NAME, host_.c_str()) !=
             0) ||
        cmd_options->set(mysql_h_, MYSQL_COMMAND_CLIENT_FLAGS, &client_flags) !=
            0 ||
        (password != nullptr &&
         ((!host_.empty() && cmd_options->set(mysql_h_, MYSQL_OPT_SSL_MODE,
                                              &authenticated_ssl_mode) != 0) ||
          (host_.empty() &&
           cmd_options->set(mysql_h_, MYSQL_COMMAND_PROTOCOL,
                            mrs_component().server_config().m_socket.c_str()) !=
               0) ||
          cmd_options->set(mysql_h_, MYSQL_OPT_CONNECT_TIMEOUT,
                           &authenticated_connect_timeout) != 0 ||
          cmd_options->set(mysql_h_, MYSQL_OPT_READ_TIMEOUT,
                           &authenticated_read_timeout) != 0 ||
          cmd_options->set(mysql_h_, MYSQL_OPT_USE_PQC_SIGN, &use_pqc_sign) !=
              0 ||
          cmd_options->set(mysql_h_, MYSQL_COMMAND_PASSWORD,
                           password->c_str()) != 0))) {
      throw std::runtime_error(
          "Failed to initialize MySQL command handler options");
    }

    if (cmd_factory->connect(mysql_h_) != 0) {
      const auto error_code = last_errno();
      auto error = last_error();
      if (error.empty()) error = "Failed to connect MySQL command handle";
      throw Connection_error(error, error_code);
    }

  } catch (...) {
    close();
    throw;
  }
}

void Mysql_command_service_session::close() noexcept {
  if (mysql_h_ != nullptr) {
    cmd_factory->close(mysql_h_);
    mysql_h_ = nullptr;
  }

  if (thread_initialized_) {
    release_command_thread();
    thread_initialized_ = false;
  }
  user_.clear();
  host_.clear();
}

bool Mysql_command_service_session::reset() noexcept {
  if (mysql_h_ == nullptr || cmd_factory->reset(mysql_h_) != 0) return false;

  set_session_thread_account(mysql_h_, user_, host_);
  return true;
}

bool Mysql_command_service_session::autocommit(bool mode) noexcept {
  return mysql_h_ != nullptr && cmd_factory->autocommit(mysql_h_, mode) == 0;
}

MYSQL *Mysql_command_service_session::mysql() const noexcept {
  auto *mysql_handle = reinterpret_cast<Mysql_handle *>(mysql_h_);
  return mysql_handle ? mysql_handle->mysql : nullptr;
}

unsigned int Mysql_command_service_session::last_errno() const noexcept {
  unsigned int error_code = 0;
  if (mysql_h_ != nullptr) {
    (void)cmd_error_info->sql_errno(mysql_h_, &error_code);
  }
  return error_code;
}

std::string Mysql_command_service_session::last_error() const {
  if (mysql_h_ == nullptr) return {};

  char errmsg[MYSQL_ERRMSG_SIZE]{'\0'};
  char *error_message = errmsg;
  (void)cmd_error_info->sql_error(mysql_h_, &error_message);
  return error_message ? error_message : "";
}

std::string Mysql_command_service_session::last_sqlstate() const {
  if (mysql_h_ == nullptr) return {};

  char *sqlstate = nullptr;
  (void)cmd_error_info->sql_state(mysql_h_, &sqlstate);
  return sqlstate ? sqlstate : "";
}

}  // namespace mrs_component
