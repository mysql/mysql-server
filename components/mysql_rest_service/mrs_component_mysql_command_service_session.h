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

#ifndef MRS_COMPONENT_MYSQL_COMMAND_SERVICE_SESSION_H
#define MRS_COMPONENT_MYSQL_COMMAND_SERVICE_SESSION_H

#include <stdexcept>
#include <string>

#include "mysql.h"
#include "mysql/components/services/mysql_command_services.h"
#include "secure_string.h"

namespace mrs_component {

void initialize_worker_command_thread();
void deinitialize_worker_command_thread() noexcept;

/**
  Owns the component command-service handle and optional thread
  initialization.

  Callers retain responsibility for SQL policy and result handling.
*/
class Mysql_command_service_session {
 public:
  class Connection_error : public std::runtime_error {
   public:
    Connection_error(const std::string &message, unsigned int error_code)
        : std::runtime_error(message), m_error_code(error_code) {}

    [[nodiscard]] unsigned int error_code() const { return m_error_code; }

   private:
    unsigned int m_error_code;
  };

  Mysql_command_service_session() = default;
  ~Mysql_command_service_session();

  Mysql_command_service_session(const Mysql_command_service_session &) = delete;
  Mysql_command_service_session &operator=(
      const Mysql_command_service_session &) = delete;
  Mysql_command_service_session(Mysql_command_service_session &&) = delete;
  Mysql_command_service_session &operator=(Mysql_command_service_session &&) =
      delete;

  void connect(const std::string &user, const std::string &host,
               bool initialize_thread, unsigned long extra_client_flags = 0,
               bool bypass_max_connections = false);
  // Requires the caller's thread lifecycle to initialize
  // mysql_command_thread.
  void connect_authenticated(const std::string &user,
                             const mysql_harness::SecureString &password,
                             unsigned long extra_client_flags = 0);
  void close() noexcept;

  [[nodiscard]] bool reset() noexcept;
  [[nodiscard]] bool autocommit(bool mode) noexcept;

  [[nodiscard]] MYSQL_H handle() const noexcept { return mysql_h_; }
  [[nodiscard]] MYSQL *mysql() const noexcept;
  [[nodiscard]] bool is_connected() const noexcept {
    return mysql_h_ != nullptr;
  }

  [[nodiscard]] unsigned int last_errno() const noexcept;
  [[nodiscard]] std::string last_error() const;
  [[nodiscard]] std::string last_sqlstate() const;

 private:
  void connect_impl(const std::string &user, const std::string &host,
                    bool initialize_thread, unsigned long extra_client_flags,
                    bool bypass_max_connections,
                    const mysql_harness::SecureString *password);

  MYSQL_H mysql_h_{nullptr};
  bool thread_initialized_{false};
  std::string user_;
  std::string host_;
};

}  // namespace mrs_component

#endif  // MRS_COMPONENT_MYSQL_COMMAND_SERVICE_SESSION_H
