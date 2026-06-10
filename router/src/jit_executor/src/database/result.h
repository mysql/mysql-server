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

#ifndef MYSQLSHDK_LIBS_DB_MYSQL_RESULT_H_
#define MYSQLSHDK_LIBS_DB_MYSQL_RESULT_H_

#include "mysqlrouter/jit_executor_db_interface.h"

#include <atomic>
#include <deque>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <mysql.h>

#include "database/column.h"
#include "database/row_by_name.h"
#include "database/row_copy.h"

#include "utils/atomic_flag.h"

namespace shcore {
namespace polyglot {
namespace database {

using jit_executor::db::IColumn;
using jit_executor::db::IResult;
using jit_executor::db::Warning;

class Session;
class Row;

class DbResult : public IResult, public std::enable_shared_from_this<DbResult> {
  friend class Session;

 public:
  ~DbResult() override;

  // Data Retrieving
  const jit_executor::db::IRow *fetch_one() override;
  bool next_resultset() override;
  std::unique_ptr<jit_executor::db::Warning> fetch_one_warning() override;

  // Metadata retrieval
  int64_t get_auto_increment_value() const override { return _last_insert_id; }
  bool has_resultset() override { return _has_resultset; }
  uint64_t get_affected_row_count() const override {
    return (_affected_rows == ~(my_ulonglong)0) ? 0 : _affected_rows;
  }
  uint64_t get_fetched_row_count() const override { return _fetched_row_count; }
  uint64_t get_warning_count() const override;
  std::string get_info() const override { return _info; }
  const std::vector<std::string> &get_gtids() const override { return _gtids; }
  const std::vector<std::shared_ptr<IColumn>> &get_metadata() const override {
    return _metadata;
  }
  std::string get_statement_id() const override;

  void buffer() override;
  void rewind() override;

  bool is_buffered() { return m_buffered; }

 protected:
  DbResult(std::shared_ptr<Session> owner, uint64_t affected_rows,
           uint64_t last_insert_id, const char *info, bool buffered);
  void reset(std::shared_ptr<MYSQL_RES> res);

  std::deque<Row_copy> _pre_fetched_rows;
  // size_t _fetched_warning_count = 0;
  shcore::atomic_flag _stop_pre_fetch;
  bool _pre_fetched = false;
  bool _persistent_pre_fetch = false;
  bool _pre_fetched_clear_at_end = false;

  bool pre_fetch_row();
  bool pre_fetch_rows(bool persistent);
  void stop_pre_fetch();

  void fetch_metadata();
  void fetch_statement_id();

  // TODO(rennox) enable collation handling
  Type map_data_type(int raw_type, int flags /*, int collation_id*/);

  virtual std::shared_ptr<Field_names> field_names() const;

  std::weak_ptr<shcore::polyglot::database::Session> _session;
  std::vector<std::shared_ptr<IColumn>> _metadata;
  std::unique_ptr<Row> _row;
  std::weak_ptr<MYSQL_RES> _result;
  std::vector<std::string> _gtids;
  mutable std::shared_ptr<Field_names> _field_names;
  uint64_t _affected_rows = 0;
  uint64_t _last_insert_id = 0;
  uint64_t _fetched_row_count = 0;
  std::string _info;
  std::list<std::unique_ptr<Warning>> _warnings;
  bool _has_resultset = false;
  bool _fetched_warnings = false;
  bool m_buffered = false;
  std::optional<std::string> m_statement_id;
};
}  // namespace database
}  // namespace polyglot
}  // namespace shcore
#endif  // MYSQLSHDK_LIBS_DB_MYSQL_RESULT_H_
