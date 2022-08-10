/*
  Copyright (c) 2016, 2022, Oracle and/or its affiliates.

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

#include "mysql_session_replayer.h"
#include <iostream>
#include "mysqlrouter/utils_sqlstring.h"

using mysqlrouter::MySQLSession;

MySQLSessionReplayer::MySQLSessionReplayer(bool trace) : trace_(trace) {}

MySQLSessionReplayer::~MySQLSessionReplayer() = default;

void MySQLSessionReplayer::connect(const std::string &host, unsigned int port,
                                   const std::string &user,
                                   const std::string &password,
                                   const std::string &unix_socket,
                                   const std::string &, int, int) {
  if (trace_) {
    std::cout << "connect: " << user << ":" << password << "@"
              << (unix_socket.length() > 0 ? unix_socket
                                           : host + ":" + std::to_string(port))
              << std::endl;
  }

  // check if connect() is not expected to fail. Note that since we mostly just
  // connect() without errors and go on about our business, connect() is allowed
  // to be called without a prior call to expect_connect(). This is in contrast
  // to execute(), query() and friends, which must be preceded by their
  // respective expect_*() call.
  if (call_info_.size() && call_info_.front().type == CallInfo::Connect) {
    const CallInfo info(call_info_.front());
    call_info_.pop_front();

    connected_ = false;
    if (info.host != host) {
      throw MySQLSession::Error(
          (std::string("expected host not found: expected ") + info.host +
           ", got " + host)
              .c_str(),
          info.error_code);
    }

    if (info.port != port) {
      throw MySQLSession::Error(
          (std::string("expected port not found: expected ") +
           std::to_string(info.port) + ", got " + std::to_string(port))
              .c_str(),
          info.error_code);
    }

    if (info.unix_socket != unix_socket) {
      throw MySQLSession::Error(
          (std::string("expected unix_socket not found: expected ") +
           info.unix_socket + ", got " + unix_socket)
              .c_str(),
          info.error_code);
    }

    if (info.user != user) {
      throw MySQLSession::Error(
          (std::string("expected user not found: expected ") + info.user +
           ", got " + user)
              .c_str(),
          info.error_code);
    }

    if (info.password != password) {
      throw MySQLSession::Error(
          (std::string("expected password not found: expected ") +
           info.password + ", got " + password)
              .c_str(),
          info.error_code);
    }

    // all params match, but called wanted to inject a error-code and error-msg
    if (info.error_code != 0) {
      last_error_msg = info.error;
      last_error_code = info.error_code;

      throw MySQLSession::Error(info.error.c_str(), info.error_code);
    }
  }

  connected_ = true;
}

void MySQLSessionReplayer::disconnect() { connected_ = false; }

void MySQLSessionReplayer::execute(const std::string &sql) {
  if (call_info_.empty()) {
    if (trace_) std::cout << "unexpected execute: " << sql << "\n";
    throw std::logic_error("Unexpected call to execute(" + sql + ")");
  }
  const CallInfo info(call_info_.front());
  if (sql.compare(0, info.sql.length(), info.sql) != 0 ||
      info.type != CallInfo::Execute) {
    if (trace_) std::cout << "wrong execute: " << sql << "\n";
    throw std::logic_error("Unexpected/out-of-order call to execute(" + sql +
                           ")\nExpected: " + info.sql);
  }
  last_insert_id_ = info.last_insert_id;
  warning_count_ = info.warning_count;
  if (trace_) std::cout << "execute: " << sql << "\n";
  if (info.error_code != 0) {
    call_info_.pop_front();

    // NOTE: in real implementation, 1st and 3rd args differ, but for testing
    //       this approximation is good enough atm; make it 1:1 if needed
    throw MySQLSession::Error(info.error.c_str(), info.error_code,
                              info.error.c_str());
  }
  call_info_.pop_front();
}

void MySQLSessionReplayer::query(
    const std::string &sql, const RowProcessor &processor,
    const FieldValidator & /*=null_field_validator*/) {
  // TODO FieldValidator is ignored for now, implement when needed

  if (call_info_.empty()) {
    if (trace_) std::cout << "unexpected query: " << sql << "\n";
    throw std::logic_error("Unexpected call to query(" + sql + ")");
  }
  const CallInfo info(call_info_.front());
  if (sql.compare(0, info.sql.length(), info.sql) != 0 ||
      info.type != CallInfo::Query) {
    if (trace_)
      std::cout << "wrong query: " << sql << "\nExpected: " + info.sql + "\n";
    throw std::logic_error("Unexpected/out-of-order call to query(" + sql +
                           ")\nExpected: " + info.sql);
  }
  if (trace_) std::cout << "query: " << sql << "\n";

  if (info.error_code != 0) {
    last_error_msg = info.error;
    last_error_code = info.error_code;

    call_info_.pop_front();

    // NOTE: in real implementation, 1st and 3rd args differ, but for testing
    //       this approximation is good enough atm; make it 1:1 if needed
    throw MySQLSession::Error(info.error.c_str(), info.error_code,
                              info.error.c_str());
  }
  for (auto &row : info.rows) {
    Row r;
    for (auto &field : row) {
      if (field) {
        r.push_back(field.c_str());
      } else {
        r.push_back(nullptr);
      }
    }
    try {
      if (!processor(r)) break;
    } catch (...) {
      last_insert_id_ = 0;
      warning_count_ = info.warning_count;
      call_info_.pop_front();
      throw;
    }
  }

  last_insert_id_ = 0;
  warning_count_ = info.warning_count;
  call_info_.pop_front();
}

class MyResultRow : public MySQLSession::ResultRow {
 public:
  static MySQLSession::Row from_replayer_row(
      const std::vector<MySQLSessionReplayer::optional_string> &real_row) {
    MySQLSession::Row row;
    for (auto &field : real_row) {
      if (field) {
        row.push_back(field.c_str());
      } else {
        row.push_back(nullptr);
      }
    }

    return row;
  }

  MyResultRow(const std::vector<MySQLSessionReplayer::optional_string> &row)
      : ResultRow(from_replayer_row(row)), real_row_(row) {
    // ResultRow doesn't own the fields, it only keeps pointers
    //
    // as we just moved those pointers from 'row' to 'real_row_' we must update
    // non-null pointers
    size_t ndx{};
    for (auto &field : real_row_) {
      if (field) {
        this->operator[](ndx) = field.c_str();
      }

      ndx++;
    }
  }

 private:
  std::vector<MySQLSessionReplayer::optional_string> real_row_;
};

std::unique_ptr<MySQLSession::ResultRow> MySQLSessionReplayer::query_one(
    const std::string &sql, const FieldValidator & /*=null_field_validator*/) {
  // TODO FieldValidator is ignored for now, implement when needed

  if (call_info_.empty()) {
    if (trace_) std::cout << "unexpected query_one: " << sql << "\n";
    throw std::logic_error("Unexpected call to query_one(" + sql + ")");
  }
  const CallInfo info(call_info_.front());
  if (sql.compare(0, info.sql.length(), info.sql) != 0 ||
      info.type != CallInfo::QueryOne) {
    if (trace_) std::cout << "unexpected query_one: " << sql << "\n";
    throw std::logic_error("Unexpected/out-of-order call to query_one(" + sql +
                           ")\nExpected: " + info.sql);
  }
  if (trace_) std::cout << "query_one: " << sql << "\n";

  if (info.error_code != 0) {
    last_error_msg = info.error;
    last_error_code = info.error_code;

    call_info_.pop_front();

    // NOTE: in real implementation, 1st and 3rd args differ, but for testing
    //       this approximation is good enough atm; make it 1:1 if needed
    throw MySQLSession::Error(info.error.c_str(), info.error_code,
                              info.error.c_str());
  }
  std::unique_ptr<ResultRow> result;
  if (!info.rows.empty()) {
    result = std::make_unique<MyResultRow>(info.rows.front());
  }
  last_insert_id_ = 0;
  warning_count_ = info.warning_count;
  call_info_.pop_front();

  return result;
}

uint64_t MySQLSessionReplayer::last_insert_id() noexcept {
  return last_insert_id_;
}

unsigned MySQLSessionReplayer::warning_count() noexcept {
  return warning_count_;
}

const char *MySQLSessionReplayer::last_error() {
  return last_error_msg.c_str();
}

unsigned int MySQLSessionReplayer::last_errno() { return last_error_code; }

std::string MySQLSessionReplayer::quote(const std::string &s,
                                        char qchar) const {
  std::string quoted;
  quoted.push_back(qchar);
  quoted.append(mysqlrouter::escape_sql_string(s));
  quoted.push_back(qchar);
  return quoted;
}

MySQLSessionReplayer &MySQLSessionReplayer::expect_connect(
    const std::string &host, unsigned port, const std::string &user,
    const std::string &password, const std::string &unix_socket) {
  CallInfo call;
  call.type = CallInfo::Connect;
  call.host = host;
  call.port = port;
  call.user = user;
  call.password = password;
  call.unix_socket = unix_socket;
  call_info_.push_back(call);
  return *this;
}

MySQLSessionReplayer &MySQLSessionReplayer::expect_execute(
    const std::string &q) {
  CallInfo call;
  call.type = CallInfo::Execute;
  call.sql = q;
  call_info_.push_back(call);
  return *this;
}

MySQLSessionReplayer &MySQLSessionReplayer::expect_query(const std::string &q) {
  CallInfo call;
  call.type = CallInfo::Query;
  call.sql = q;
  call_info_.push_back(call);
  return *this;
}

MySQLSessionReplayer &MySQLSessionReplayer::expect_query_one(
    const std::string &q) {
  CallInfo call;
  call.type = CallInfo::QueryOne;
  call.sql = q;
  call_info_.push_back(call);
  return *this;
}

void MySQLSessionReplayer::then_ok(uint64_t the_last_insert_id /*=0*/,
                                   unsigned warning_count /*=0*/) {
  call_info_.back().last_insert_id = the_last_insert_id;
  call_info_.back().warning_count = warning_count;
}

void MySQLSessionReplayer::then_error(const std::string &error,
                                      unsigned int code) {
  call_info_.back().error = error;
  call_info_.back().error_code = code;
}

void MySQLSessionReplayer::then_return(
    unsigned int num_fields, std::vector<std::vector<optional_string>> rows) {
  call_info_.back().num_fields = num_fields;
  call_info_.back().rows = rows;
}

bool MySQLSessionReplayer::print_expected() {
  std::cout << "Expected MySQLSession calls:\n";
  for (auto &info : call_info_) {
    switch (info.type) {
      case CallInfo::Execute:
        std::cout << "\texecute: ";
        std::cout << info.sql << "\n";
        break;
      case CallInfo::Query:
        std::cout << "\tquery: ";
        std::cout << info.sql << "\n";
        break;
      case CallInfo::QueryOne:
        std::cout << "\tquery_one: ";
        std::cout << info.sql << "\n";
        break;
      case CallInfo::Connect:
        std::cout << "\tconnect: ";
        std::cout << info.user << ":" << info.password << "@" << info.host
                  << ":" << info.port << "\n";
        break;
    }
  }
  return !call_info_.empty();
}

MySQLSessionReplayer::CallInfo::CallInfo(const CallInfo &) = default;
