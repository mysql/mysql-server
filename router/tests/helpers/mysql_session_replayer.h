/*
  Copyright (c) 2016, 2018, Oracle and/or its affiliates. All rights reserved.

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

#ifndef ROUTER_MYSQL_SESSION_REPLAYER_INCLUDED
#define ROUTER_MYSQL_SESSION_REPLAYER_INCLUDED

#include <deque>
#include "mysqlrouter/mysql_session.h"

class MySQLSessionReplayer : public mysqlrouter::MySQLSession {
 public:
  MySQLSessionReplayer(bool trace = false);
  ~MySQLSessionReplayer() override;

  virtual void connect(const std::string &host, unsigned int port,
                       const std::string &user, const std::string &password,
                       const std::string &unix_socket,
                       const std::string &default_schema,
                       int connect_timeout = kDefaultConnectTimeout,
                       int read_timeout = kDefaultReadTimeout) override;
  virtual void disconnect() override;
  virtual bool is_connected() noexcept override { return connected_; }

  virtual void execute(const std::string &sql) override;
  virtual void query(const std::string &sql,
                     const RowProcessor &processor) override;
  virtual ResultRow *query_one(const std::string &sql) override;

  virtual uint64_t last_insert_id() noexcept override;

  virtual std::string quote(const std::string &s,
                            char qchar = '\'') noexcept override;

  virtual const char *last_error() override;
  virtual unsigned int last_errno() override;

 public:
  class string {
   public:
    string(const char *s) : s_(s ? s : ""), is_null_(s == nullptr) {}
    string() : is_null_(true) {}

    operator const std::string &() const { return s_; }
    operator bool() const { return !is_null_; }
    const char *c_str() const { return !is_null_ ? s_.c_str() : nullptr; }

   private:
    std::string s_;
    bool is_null_;
  };
  string string_or_null(const char *s) { return string(s); }
  string string_or_null() { return string(); }

  MySQLSessionReplayer &expect_connect(const std::string &host, unsigned port,
                                       const std::string &user,
                                       const std::string &password,
                                       const std::string &unix_socket);
  MySQLSessionReplayer &expect_execute(const std::string &q);
  MySQLSessionReplayer &expect_query(const std::string &q);
  MySQLSessionReplayer &expect_query_one(const std::string &q);
  void then_ok(uint64_t the_last_insert_id = 0);
  void then_error(const std::string &error, unsigned int code);
  void then_return(unsigned int num_fields,
                   std::vector<std::vector<string>> rows);
  bool print_expected();

  bool empty() { return call_info_.empty(); }

  void clear_expects() { call_info_.clear(); }

 private:
  struct CallInfo {
    CallInfo() {}
    CallInfo(const CallInfo &ci);

    enum Type { Connect, Execute, Query, QueryOne };

    // common fields
    Type type;
    std::string error;
    unsigned int error_code = 0;

    // SQL fields
    std::string sql;
    uint64_t last_insert_id = 0;
    unsigned int num_fields = 0;
    std::vector<std::vector<string>> rows;

    // connect fields
    std::string host;
    unsigned int port;
    std::string user;
    std::string password;
    std::string unix_socket;
  };
  std::deque<CallInfo> call_info_;
  uint64_t last_insert_id_;
  std::string last_error_msg;
  unsigned int last_error_code;
  bool trace_ = false;
  bool connected_ = false;
};

#endif  // ROUTER_MYSQL_SESSION_REPLAYER_INCLUDED
