/*
 * Copyright (c) 2017, 2026, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

// MySQL DB access module, for use by plugins and others
// For the module that implements interactive DB functionality see mod_db

#ifndef MYSQLSHDK_LIBS_DB_MYSQL_SESSION_H_
#define MYSQLSHDK_LIBS_DB_MYSQL_SESSION_H_

#include <mysql.h>
#include <mysqld_error.h>

#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "database/query_attributes.h"
#include "database/result.h"
#include "mysqlrouter/jit_executor_db_interface.h"
#include "mysqlrouter/jit_executor_plugin_export.h"
#include "router/src/router/include/mysqlrouter/mysql_session.h"

namespace shcore {
namespace polyglot {
namespace database {

using jit_executor::db::IResult;
using jit_executor::db::ISession;

class JIT_EXECUTOR_PLUGIN_EXPORT Session
    : public ISession,
      public std::enable_shared_from_this<Session> {
  friend class DbResult;  // The Result class uses some functions of this class
 public:
  Session(MYSQL *mysql);
  ~Session() override;

  // Ensures the connection is ready for the next query execution
  void reset() override;

 private:
  std::shared_ptr<IResult> query(
      const char *sql, size_t len, bool buffered,
      const std::vector<Query_attribute> &query_attributes = {});
  std::shared_ptr<IResult> query_udf(std::string_view sql, bool buffered);
  void execute(const char *sql, size_t len);

  inline void execute(const char *sql) { execute(sql, ::strlen(sql)); }

  void start_transaction();
  void commit();
  void rollback();

  bool next_resultset();
  void prepare_fetch(DbResult *target);

  std::string uri() { return _uri; }

  // Utility functions to retrieve session status
  uint64_t get_thread_id() const { return m_thread_id; }

  uint64_t get_protocol_info() {
    if (_mysql) return mysql_get_proto_info(_mysql);
    return 0;
  }
  bool is_compression_enabled() const {
    return _mysql ? _mysql->net.compress : false;
  }
  const char *get_connection_info() {
    if (_mysql) return mysql_get_host_info(_mysql);
    return nullptr;
  }
  const char *get_server_info() {
    if (_mysql) return mysql_get_server_info(_mysql);
    return nullptr;
  }
  const char *get_stats() {
    _prev_result.reset();
    if (_mysql) return mysql_stat(_mysql);
    return nullptr;
  }
  const char *get_ssl_cipher() {
    if (_mysql) return mysql_get_ssl_cipher(_mysql);
    return nullptr;
  }

  const char *get_mysql_info() const { return mysql_info(_mysql); }

  bool is_open() const { return _mysql ? true : false; }

  const char *get_last_error(int *out_code, const char **out_sqlstate) {
    if (out_code) *out_code = mysql_errno(_mysql);
    if (out_sqlstate) *out_sqlstate = mysql_sqlstate(_mysql);

    return mysql_error(_mysql);
  }

  std::vector<std::string> get_last_gtids() const;
  std::optional<std::string> get_last_statement_id() const;

  uint32_t get_server_status() const {
    return _mysql ? _mysql->server_status : 0;
  }

  uint64_t warning_count() const {
    return _mysql ? mysql_warning_count(_mysql) : 0;
  }

  // TODO(rennox): these functions go on the high level object
  void set_query_attributes(const shcore::Dictionary_t &args);
  std::vector<Query_attribute> query_attributes() const;

  std::shared_ptr<IResult> run_sql(const std::string &sql) override;

  std::shared_ptr<IResult> run_sql(
      const char *sql, size_t len, bool lazy_fetch, bool is_udf,
      const std::vector<Query_attribute> &query_attributes = {});

  MYSQL *get_handle() { return _mysql; }

  std::string _uri;
  MYSQL *_mysql = nullptr;
  std::shared_ptr<MYSQL_RES> _prev_result;
  uint64_t m_thread_id = 0;

  struct Local_infile_callbacks {
    int (*init)(void **, const char *, void *) = nullptr;
    int (*read)(void *, char *, unsigned int) = nullptr;
    void (*end)(void *) = nullptr;
    int (*error)(void *, char *, unsigned int) = nullptr;
    void *userdata = nullptr;
  };
  Local_infile_callbacks m_local_infile;
  Query_attribute_store m_query_attributes;
};

}  // namespace database
}  // namespace polyglot
}  // namespace shcore
#endif  // MYSQLSHDK_LIBS_DB_MYSQL_SESSION_H_
