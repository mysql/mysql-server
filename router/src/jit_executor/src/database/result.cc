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

#include "database/result.h"

#include <cstdlib>
#include <sstream>
#include <string>
#include <utility>

// #include "mysqlshdk/libs/db/charset.h"
#include "database/row.h"
#include "database/session.h"
// #include "shellcore/interrupt_handler.h"
// #include "utils/utils_general.h"
#include "utils/utils_string.h"

#include "router/src/router/include/mysqlrouter/mysql_session.h"

namespace shcore {
namespace polyglot {
namespace database {

DbResult::DbResult(std::shared_ptr<Session> owner, uint64_t affected_rows_,
                   uint64_t last_insert_id, const char *info_, bool buffered)
    : _session(owner),
      _affected_rows(affected_rows_),
      _last_insert_id(last_insert_id),
      _fetched_row_count(0),
      _has_resultset(false),
      m_buffered(buffered) {
  if (info_) _info.assign(info_);
  if (owner) {
    owner->prepare_fetch(this);
  }

  _row.reset(new Row(this));
}

// MYSQL-SERVER-CODE mysql.cc:3341
static const char *fieldtype2str(enum enum_field_types type) {
  switch (type) {
    case MYSQL_TYPE_BIT:
      return "BIT";
    case MYSQL_TYPE_BLOB:
      return "BLOB";
    case MYSQL_TYPE_DATE:
      return "DATE";
    case MYSQL_TYPE_DATETIME:
      return "DATETIME";
    case MYSQL_TYPE_NEWDECIMAL:
      return "NEWDECIMAL";
    case MYSQL_TYPE_DECIMAL:
      return "DECIMAL";
    case MYSQL_TYPE_DOUBLE:
      return "DOUBLE";
    case MYSQL_TYPE_ENUM:
      return "ENUM";
    case MYSQL_TYPE_FLOAT:
      return "FLOAT";
    case MYSQL_TYPE_GEOMETRY:
      return "GEOMETRY";
    case MYSQL_TYPE_INT24:
      return "INT24";
    case MYSQL_TYPE_JSON:
      return "JSON";
    case MYSQL_TYPE_LONG:
      return "LONG";
    case MYSQL_TYPE_LONGLONG:
      return "LONGLONG";
    case MYSQL_TYPE_LONG_BLOB:
      return "LONG_BLOB";
    case MYSQL_TYPE_MEDIUM_BLOB:
      return "MEDIUM_BLOB";
    case MYSQL_TYPE_NEWDATE:
      return "NEWDATE";
    case MYSQL_TYPE_NULL:
      return "NULL";
    case MYSQL_TYPE_SET:
      return "SET";
    case MYSQL_TYPE_SHORT:
      return "SHORT";
    case MYSQL_TYPE_STRING:
      return "STRING";
    case MYSQL_TYPE_TIME:
      return "TIME";
    case MYSQL_TYPE_TIMESTAMP:
      return "TIMESTAMP";
    case MYSQL_TYPE_TINY:
      return "TINY";
    case MYSQL_TYPE_TINY_BLOB:
      return "TINY_BLOB";
    case MYSQL_TYPE_VAR_STRING:
      return "VAR_STRING";
    case MYSQL_TYPE_YEAR:
      return "YEAR";
    case MYSQL_TYPE_VECTOR:
      return "VECTOR";
    default:
      return "?-unknown-?";
  }
}

// MYSQL-SERVER-CODE (slightly modified) mysql.c:3402
static std::string fieldflags2str(unsigned int f) {
  std::ostringstream s;

#define ff2s_check_flag(X) \
  if (f & X##_FLAG) {      \
    s << #X " ";           \
    f &= ~X##_FLAG;        \
  }
  ff2s_check_flag(NOT_NULL);
  ff2s_check_flag(PRI_KEY);
  ff2s_check_flag(UNIQUE_KEY);
  ff2s_check_flag(MULTIPLE_KEY);
  ff2s_check_flag(BLOB);
  ff2s_check_flag(UNSIGNED);
  ff2s_check_flag(ZEROFILL);
  ff2s_check_flag(BINARY);
  ff2s_check_flag(ENUM);
  ff2s_check_flag(AUTO_INCREMENT);
  ff2s_check_flag(TIMESTAMP);
  ff2s_check_flag(SET);
  ff2s_check_flag(NO_DEFAULT_VALUE);
  ff2s_check_flag(NUM);
  ff2s_check_flag(PART_KEY);
  ff2s_check_flag(UNIQUE);
  ff2s_check_flag(BINCMP);
  ff2s_check_flag(ON_UPDATE_NOW);
#undef ff2s_check_flag
  if (f) s << shcore::str_format(" unknowns=0x%04x", f);
  return s.str();
}

void DbResult::fetch_metadata() {
  int num_fields = 0;

  _metadata.clear();

  // res could be NULL on queries not returning data
  std::shared_ptr<MYSQL_RES> res = _result.lock();

  if (res) {
    num_fields = mysql_num_fields(res.get());
    MYSQL_FIELD *fields = mysql_fetch_fields(res.get());

    for (int index = 0; index < num_fields; index++) {
      _metadata.push_back(
          std::make_shared<Column>(
              fields[index].catalog, fields[index].db, fields[index].org_table,
              fields[index].table, fields[index].org_name, fields[index].name,
              fields[index].length, fields[index].decimals,
              // TODO(rennox): enable collation handling
              map_data_type(fields[index].type,
                            fields[index]
                                .flags /*,
fields[index].charsetnr*/),
              fields[index].charsetnr,
              static_cast<bool>(fields[index].flags & UNSIGNED_FLAG),
              static_cast<bool>(fields[index].flags & ZEROFILL_FLAG),
              static_cast<bool>(fields[index].flags & BINARY_FLAG),
              fieldflags2str(fields[index].flags),
              fieldtype2str(fields[index].type)));
    }
  }
}

DbResult::~DbResult() = default;

const IRow *DbResult::fetch_one() {
  if (_pre_fetched) {
    if (!_persistent_pre_fetch) {
      if (!_pre_fetched_rows.empty()) {
        if (_fetched_row_count > 0)  // free the previously fetched row
          _pre_fetched_rows.pop_front();
        if (!_pre_fetched_rows.empty()) {
          // return the next row, but don't pop it yet otherwise it'll be freed
          const IRow *row = &_pre_fetched_rows.front();
          _fetched_row_count++;
          return row;
        }
      }
    } else {
      if (_fetched_row_count < _pre_fetched_rows.size()) {
        return &_pre_fetched_rows[_fetched_row_count++];
      }
    }
    if (_pre_fetched_clear_at_end &&
        (_fetched_row_count == _pre_fetched_rows.size()))
      _pre_fetched = false;

  } else {
    // clear state if we exited a pre_fetch scenario
    if (_pre_fetched_clear_at_end) {
      assert(_pre_fetched_rows.size() == 1);
      _pre_fetched_rows.clear();
      _pre_fetched_clear_at_end = false;
    }

    if (has_resultset()) {
      // Loads the first row
      std::shared_ptr<MYSQL_RES> res = _result.lock();

      if (res) {
        MYSQL_ROW mysql_row = mysql_fetch_row(res.get());
        if (mysql_row) {
          unsigned long *lengths;
          lengths = mysql_fetch_lengths(res.get());

          _row->reset(mysql_row, lengths);

          // Each read row increases the count
          _fetched_row_count++;
        } else {
          _row.reset();
          if (auto session = _session.lock()) {
            int code = 0;
            const char *state;
            const char *err = session->get_last_error(&code, &state);
            if (code != 0)
              throw mysqlrouter::MySQLSession::Error(err, code, state);
          }

          // It means we are done, time to fetch the statement id
          fetch_statement_id();
        }
      } else {
        _row.reset();
      }
    } else {
      fetch_statement_id();
      _row.reset();
    }
    return _row.get();
  }

  return nullptr;
}

void DbResult::fetch_statement_id() {
  if (!m_statement_id.has_value()) {
    if (auto s = _session.lock()) {
      m_statement_id = s->get_last_statement_id();
    }
  }
}

std::string DbResult::get_statement_id() const {
  return m_statement_id.value_or("");
}

bool DbResult::next_resultset() {
  bool ret_val = false;

  if (auto s = _session.lock()) {
    ret_val = s->next_resultset();

    if (ret_val) s->prepare_fetch(this);

    _row.reset(new Row(this));
  }

  return ret_val;
}

void DbResult::rewind() {
  _fetched_row_count = 0;
  _row.reset(new Row(this));

  if (std::shared_ptr<MYSQL_RES> res = _result.lock())
    mysql_data_seek(res.get(), 0);
}

uint64_t DbResult::get_warning_count() const {
  if (auto s = _session.lock()) return s->warning_count();
  return 0;
}

std::unique_ptr<Warning> DbResult::fetch_one_warning() {
  if (!_fetched_warnings && get_warning_count()) {
    _fetched_warnings = true;
    if (auto s = _session.lock()) {
      auto result =
          s->query("show warnings", sizeof("show warnings") - 1, true);
      while (auto row = result->fetch_one()) {
        auto w = std::make_unique<Warning>();
        std::string level = row->get_string(0);
        if (level == "Error") {
          w->level = Warning::Level::Error;
        } else if (level == "Warning") {
          w->level = Warning::Level::Warn;
        } else {
          assert(level == "Note");
          w->level = Warning::Level::Note;
        }
        w->code = row->get_int(1);
        w->msg = row->get_string(2);
        _warnings.push_back(std::move(w));
      }
    }
  }

  if (!_warnings.empty()) {
    auto tmp = std::move(_warnings.front());
    _warnings.pop_front();
    return tmp;
  }
  return {};
}

void DbResult::reset(std::shared_ptr<MYSQL_RES> res) {
  _field_names.reset();
  _has_resultset = false;
  _pre_fetched = false;
  _pre_fetched_clear_at_end = false;
  _fetched_row_count = 0;
  _pre_fetched_rows.clear();
  _result = std::move(res);

  if (res) {
    _has_resultset = true;
    fetch_metadata();
  }

  if (auto s = _session.lock()) _gtids = s->get_last_gtids();

  // Non query results may have the statement_id information already available
  fetch_statement_id();
}

void DbResult::buffer() {
  // shcore::Interrupt_handler intr([this]() {
  //   stop_pre_fetch();
  //   return false;
  // });
  pre_fetch_rows(true);
}

bool DbResult::pre_fetch_row() {
  if (auto result = _result.lock(); result) {
    _persistent_pre_fetch = false;

    if (!has_resultset()) return false;

    _pre_fetched_rows.emplace_back(*fetch_one());
    _fetched_row_count = 0;
    _pre_fetched = true;
    _pre_fetched_clear_at_end = true;
  }
  return true;
}

bool DbResult::pre_fetch_rows(bool persistent) {
  auto result = _result.lock();
  if (result) {
    _persistent_pre_fetch = persistent;
    _stop_pre_fetch.clear();
    if (!has_resultset()) return false;
    while (auto row = fetch_one()) {
      if (_stop_pre_fetch.test()) return true;
      _pre_fetched_rows.emplace_back(*row);
    }
    _fetched_row_count = 0;

    _pre_fetched = true;
  }
  return true;
}

void DbResult::stop_pre_fetch() { _stop_pre_fetch.test_and_set(); }

std::shared_ptr<Field_names> DbResult::field_names() const {
  if (!_field_names) {
    _field_names = std::make_shared<Field_names>();
    for (const auto &column : _metadata)
      _field_names->add(column->get_column_label());
  }
  return _field_names;
}

// TODO(rennox): enable collation handling...
Type DbResult::map_data_type(int raw_type, int flags /*, int collation_id*/) {
  switch (raw_type) {
    case MYSQL_TYPE_NULL:
      return Type::Null;
    case MYSQL_TYPE_NEWDECIMAL:
    case MYSQL_TYPE_DECIMAL:
      return Type::Decimal;
    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_NEWDATE:
      return Type::Date;
    case MYSQL_TYPE_TIME2:
    case MYSQL_TYPE_TIME:
      return Type::Time;
    case MYSQL_TYPE_STRING:
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_VAR_STRING:
      if (flags & ENUM_FLAG)
        // ENUM STRING TYPE
        return Type::Enum;
      else if (flags & SET_FLAG)
        // SET STRING TYPE
        return Type::Set;
      // TODO(rennox): collation re-enable this
      // else if (collation_id == charset::k_binary_collation_id) {
      //   // BINARY/VARBINARY STRING TYPES
      //   return Type::Bytes;
      // } else {
      // CHAR/VARCHAR STRING TYPES
      else
        return Type::String;
      // }
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_BLOB:
      // TODO(rennox): collation re-enable this
      // if (collation_id == charset::k_binary_collation_id) {
      //   // BLOB STRING TYPE
      //   return Type::Bytes;
      // } else {
      // TEXT STRING TYPE
      return Type::String;
      // }
    case MYSQL_TYPE_GEOMETRY:
      return Type::Geometry;
    case MYSQL_TYPE_JSON:
      return Type::Json;
    case MYSQL_TYPE_YEAR:
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
      return (flags & UNSIGNED_FLAG) ? Type::UInteger : Type::Integer;
    case MYSQL_TYPE_FLOAT:
      return Type::Float;
    case MYSQL_TYPE_DOUBLE:
      return Type::Double;
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_TIMESTAMP:
    case MYSQL_TYPE_DATETIME2:
    case MYSQL_TYPE_TIMESTAMP2:
      // The difference between TIMESTAMP and DATETIME is entirely in terms
      // of internal representation at the server side. At the client side,
      // there is no difference.
      // TIMESTAMP is the number of seconds since epoch, so it cannot store
      // dates before 1970. DATETIME is an arbitrary date and time value,
      // so it does not have that limitation.
      return Type::DateTime;
    case MYSQL_TYPE_BIT:
      return Type::Bit;
    case MYSQL_TYPE_ENUM:
      return Type::Enum;
    case MYSQL_TYPE_SET:
      return Type::Set;
    case MYSQL_TYPE_VECTOR:
      return Type::Vector;
  }
  throw std::logic_error("Invalid type");
}
}  // namespace database
}  // namespace polyglot
}  // namespace shcore
