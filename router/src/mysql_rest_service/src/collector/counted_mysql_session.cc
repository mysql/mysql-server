/*
  Copyright (c) 2023, 2026, Oracle and/or its affiliates.

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

#include "collector/counted_mysql_session.h"

#include <stdexcept>

#include "mrs/router_observation_entities.h"
#include "mysql/harness/destination.h"

namespace collector {

static bool did_server_disconnect(unsigned int code) {
  switch (code) {
    case CR_SERVER_GONE_ERROR:
    case CR_SERVER_LOST:
    case CR_SERVER_LOST_EXTENDED:
      return true;
    default:
      return false;
  }
}

class GuardBoolSetFalse {
 public:
  GuardBoolSetFalse(bool &value) : value_{value} {}
  ~GuardBoolSetFalse() { value_ = false; }

  bool &value_;
};

#define CHECK_DISCONNECTION_RETURN(X)                                       \
  {                                                                         \
    GuardBoolSetFalse guard{reconnect_at_next_query_};                      \
    try {                                                                   \
      return X;                                                             \
    } catch (const CountedMySQLSession::Error &e) {                         \
      if (reconnect_at_next_query_ && did_server_disconnect(e.code())) {    \
        try {                                                               \
          connect_and_set_opts(get_connection_parameters(), initial_sqls_); \
        } catch (...) {                                                     \
          throw e;                                                          \
        }                                                                   \
        return X;                                                           \
      }                                                                     \
      throw;                                                                \
    }                                                                       \
  }

#define CHECK_DISCONNECTION_VOID(X)                                         \
  {                                                                         \
    GuardBoolSetFalse guard{reconnect_at_next_query_};                      \
    try {                                                                   \
      X;                                                                    \
    } catch (const CountedMySQLSession::Error &e) {                         \
      if (reconnect_at_next_query_ && did_server_disconnect(e.code())) {    \
        try {                                                               \
          connect_and_set_opts(get_connection_parameters(), initial_sqls_); \
        } catch (...) {                                                     \
          throw e;                                                          \
        }                                                                   \
        X;                                                                  \
      }                                                                     \
      throw;                                                                \
    }                                                                       \
  }

using MySQLSession = mysqlrouter::MySQLSession;

CountedMySQLSession::CountedMySQLSession()
    : CountedMySQLSession(std::make_unique<MySQLSession>()) {}

CountedMySQLSession::CountedMySQLSession(
    std::unique_ptr<mysqlrouter::MySQLSession> impl)
    : impl_{std::move(impl)} {
  if (impl_ == nullptr) {
    throw std::logic_error(
        "CountedMySQLSession requires a MySQLSession implementation");
  }
  mrs::Counter<kEntityCounterMySQLConnectionsActive>::increment();
}

CountedMySQLSession::~CountedMySQLSession() {
  mrs::Counter<kEntityCounterMySQLConnectionsActive>::increment(-1);
}

void CountedMySQLSession::allow_failure_at_next_query() {
  reconnect_at_next_query_ = true;
}

CountedMySQLSession::ConnectionParameters
CountedMySQLSession::get_connection_parameters() const {
  return connection_params_;
}

void CountedMySQLSession::execute_initial_sqls() {
  for (const auto &sql : initial_sqls_) {
    execute(sql);
  }
}

CountedMySQLSession::Sqls CountedMySQLSession::get_initial_sqls() const {
  return initial_sqls_;
}

void CountedMySQLSession::connect_and_set_opts(
    const ConnectionParameters &connection_params, const Sqls &initial_sqls) {
  connection_params_ = connection_params;
  connection_id_ = 0;
  initial_sqls_ = initial_sqls;

  connect(connection_params);
  reconnect_at_next_query_ = false;
  execute_initial_sqls();
}

void CountedMySQLSession::connect(
    const ConnectionParameters &connection_params) {
  std::string host;
  uint16_t port{};
  std::string unix_socket;

  if (connection_params.conn_opts.destination.is_tcp()) {
    const auto dest = connection_params.conn_opts.destination.as_tcp();
    host = dest.hostname();
    port = dest.port();
  } else {
    const auto dest = connection_params.conn_opts.destination.as_local();
    unix_socket = dest.path();
  }

  const auto &opt = connection_params.ssl_opts;
  set_ssl_options(opt.ssl_mode, opt.tls_version, opt.ssl_cipher, opt.ca,
                  opt.capath, opt.crl, opt.crlpath);

  connect(host, port, connection_params.conn_opts.username,
          connection_params.conn_opts.password, unix_socket,
          connection_params.conn_opts.default_schema,
          connection_params.conn_opts.connect_timeout,
          connection_params.conn_opts.read_timeout,
          connection_params.conn_opts.extra_client_flags);
}

void CountedMySQLSession::set_ssl_options(mysql_ssl_mode ssl_mode,
                                          const std::string &tls_version,
                                          const std::string &ssl_cipher,
                                          const std::string &ca,
                                          const std::string &capath,
                                          const std::string &crl,
                                          const std::string &crlpath) {
  impl_->set_ssl_options(ssl_mode, tls_version, ssl_cipher, ca, capath, crl,
                         crlpath);
}

void CountedMySQLSession::connect(const std::string &host, unsigned int port,
                                  const std::string &username,
                                  const mysql_harness::SecureString &password,
                                  const std::string &unix_socket,
                                  const std::string &default_schema,
                                  int connect_timeout, int read_timeout,
                                  unsigned long extra_client_flags) {
  impl_->connect(host, port, username, password, unix_socket, default_schema,
                 connect_timeout, read_timeout, extra_client_flags);

  reconnect_at_next_query_ = false;
  connection_id_ = 0;
  update_connection_address(host, port, unix_socket);

  auto dest = !unix_socket.empty()
                  ? mysql_harness::Destination(
                        mysql_harness::LocalDestination(unix_socket))
                  : mysql_harness::Destination(
                        mysql_harness::TcpDestination(host, port));

  connection_params_.conn_opts.destination = dest;
  connection_params_.conn_opts.username = username;
  connection_params_.conn_opts.password = password;
  connection_params_.conn_opts.default_schema = default_schema;
  connection_params_.conn_opts.connect_timeout = connect_timeout;
  connection_params_.conn_opts.read_timeout = read_timeout;
  connection_params_.conn_opts.extra_client_flags = extra_client_flags;
}

void CountedMySQLSession::connect(const MySQLSession &other,
                                  const std::string &username,
                                  const mysql_harness::SecureString &password) {
  auto other_counted = dynamic_cast<const CountedMySQLSession *>(&other);
  if (!other_counted) {
    throw std::logic_error("Expected CountedMySQLSession instance");
  }
  auto params = other_counted->get_connection_parameters();
  auto sqls = other_counted->get_initial_sqls();
  params.conn_opts.username = username;
  params.conn_opts.password = password;

  connect_and_set_opts(params, sqls);
}

void CountedMySQLSession::change_user(
    const std::string &user, const mysql_harness::SecureString &password,
    const std::string &db) {
  mrs::Counter<kEntityCounterMySQLChangeUser>::increment();
  CHECK_DISCONNECTION_VOID(impl_->change_user(user, password, db));
  connection_params_.conn_opts.username = user;
  connection_params_.conn_opts.password = password;
  connection_params_.conn_opts.default_schema = db;
  connection_id_ = 0;
}

void CountedMySQLSession::disconnect() {
  impl_->disconnect();
  connection_address_.clear();

  reconnect_at_next_query_ = false;
  connection_id_ = 0;
}

void CountedMySQLSession::reset() { impl_->reset(); }

uint64_t CountedMySQLSession::prepare(const std::string &query) {
  mrs::Counter<kEntityCounterMySQLPrepare>::increment();
  CHECK_DISCONNECTION_RETURN(impl_->prepare(query));
}

void CountedMySQLSession::prepare_execute_with_bind_parameters(
    uint64_t ps_id, std::vector<MYSQL_BIND> bind_parameters,
    const ResultRowProcessor &processor, const FieldValidator &validator,
    const OnResultSetEnd &on_resultset_end) {
  mrs::Counter<kEntityCounterMySQLPrepareExecute>::increment();
  CHECK_DISCONNECTION_VOID(impl_->prepare_execute_with_bind_parameters(
      ps_id, bind_parameters, processor, validator, on_resultset_end));
}

void CountedMySQLSession::prepare_remove(uint64_t ps_id) {
  mrs::Counter<kEntityCounterMySQLPrepareRemove>::increment();
  CHECK_DISCONNECTION_VOID(impl_->prepare_remove(ps_id));
}

void CountedMySQLSession::execute(const std::string &query) {
  mrs::Counter<kEntityCounterMySQLQueries>::increment();
  CHECK_DISCONNECTION_VOID(impl_->execute(query));
}

void CountedMySQLSession::query(const std::string &query,
                                const ResultRowProcessor &processor,
                                const FieldValidator &validator) {
  mrs::Counter<kEntityCounterMySQLQueries>::increment();
  CHECK_DISCONNECTION_VOID(impl_->query(query, processor, validator));
}

std::unique_ptr<MySQLSession::ResultRow> CountedMySQLSession::query_one(
    const std::string &query, const FieldValidator &validator) {
  mrs::Counter<kEntityCounterMySQLQueries>::increment();
  CHECK_DISCONNECTION_RETURN(impl_->query_one(query, validator));
}

std::unique_ptr<MySQLSession::ResultRow> CountedMySQLSession::query_one(
    const std::string &query) {
  mrs::Counter<kEntityCounterMySQLQueries>::increment();
  CHECK_DISCONNECTION_RETURN(impl_->query_one(query));
}

int CountedMySQLSession::ping() { return impl_->ping(); }

bool CountedMySQLSession::execute_nb(const std::string &query) {
  return impl_->execute_nb(query);
}

uint64_t CountedMySQLSession::last_insert_id() noexcept {
  return impl_->last_insert_id();
}

uint64_t CountedMySQLSession::affected_rows() noexcept {
  return impl_->affected_rows();
}

unsigned CountedMySQLSession::warning_count() noexcept {
  return impl_->warning_count();
}

std::string CountedMySQLSession::quote(const std::string &s, char qchar) const {
  return impl_->quote(s, qchar);
}

bool CountedMySQLSession::is_connected() noexcept {
  return impl_->is_connected();
}

const char *CountedMySQLSession::last_sqlstate() {
  return impl_->last_sqlstate();
}

const char *CountedMySQLSession::last_error() { return impl_->last_error(); }

unsigned int CountedMySQLSession::last_errno() { return impl_->last_errno(); }

const char *CountedMySQLSession::ssl_cipher() { return impl_->ssl_cipher(); }

bool CountedMySQLSession::has_data_on_socket() {
  return impl_->has_data_on_socket();
}

std::vector<std::string> CountedMySQLSession::get_session_tracker_data(
    enum enum_session_state_type type) {
  return impl_->get_session_tracker_data(type);
}

bool CountedMySQLSession::is_ssl_session_reused() {
  return impl_->is_ssl_session_reused();
}

unsigned long CountedMySQLSession::server_version() {
  return impl_->server_version();
}

MYSQL *CountedMySQLSession::get_handle() { return impl_->get_handle(); }

void CountedMySQLSession::log_queries(bool log) { impl_->log_queries(log); }

bool CountedMySQLSession::log_queries() const { return impl_->log_queries(); }

uint64_t CountedMySQLSession::connection_id() {
  if (connection_id_ == 0) {
    auto *handle = impl_->get_handle();
    if (handle != nullptr) {
      connection_id_ = mysql_thread_id(handle);
    }

    if (connection_id_ == 0) {
      auto row = impl_->query_one("select connection_id()");
      if (!row) {
        throw std::logic_error("could not query connection_id()");
      }
      connection_id_ = std::stoull((*row)[0]);
    }
  }

  return connection_id_;
}

const std::string &CountedMySQLSession::get_address() noexcept {
  return connection_address_;
}

void CountedMySQLSession::update_connection_address(
    const std::string &host, unsigned int port,
    const std::string &unix_socket) {
  if (unix_socket.empty()) {
    connection_address_ =
        mysql_harness::Destination(mysql_harness::TcpDestination(host, port))
            .str();
  } else {
    connection_address_ =
        mysql_harness::Destination(mysql_harness::LocalDestination(unix_socket))
            .str();
  }
}

}  // namespace collector
