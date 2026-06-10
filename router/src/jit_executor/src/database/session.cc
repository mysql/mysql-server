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

#include "database/session.h"

#include <mysql_version.h>

#include <mutex>
#include <regex>
#include <utility>
#include <vector>

// #include "mysqlshdk/libs/utils/debug.h"
// #include "mysqlshdk/libs/utils/fault_injection.h"
// #include "mysqlshdk/libs/utils/log_sql.h"
#include "mysql/harness/scoped_callback.h"
#include "objects/polyglot_date.h"
#include "router/src/router/include/mysqlrouter/mysql_session.h"
#include "utils/profiling.h"

namespace shcore {
namespace polyglot {
namespace database {

using jit_executor::db::IResult;

namespace {
constexpr size_t K_MAX_QUERY_ATTRIBUTES = 32;
}  // namespace

//-------------------------- Session Implementation ----------------------------

Session::Session(MYSQL *mysql) : _mysql{mysql} {}

std::shared_ptr<IResult> Session::query(
    const char *sql, size_t len, bool buffered,
    const std::vector<Query_attribute> &query_attributes) {
  return run_sql(sql, len, buffered, false, query_attributes);
}

std::shared_ptr<IResult> Session::query_udf(std::string_view sql,
                                            bool buffered) {
  return run_sql(sql.data(), sql.size(), buffered, true);
}

void Session::execute(const char *sql, size_t len) {
  auto result = run_sql(sql, len, true, false);
}

void Session::set_query_attributes(const shcore::Dictionary_t &args) {
  if (!m_query_attributes.set(args)) {
    m_query_attributes.handle_errors(true);
  }
}

std::vector<Query_attribute> Session::query_attributes() const {
  using shcore::polyglot::database::Classic_query_attribute;
  return m_query_attributes.get_query_attributes(
      [](const shcore::Value &att) -> std::unique_ptr<Classic_query_attribute> {
        switch (att.get_type()) {
          case shcore::Value_type::String:
            return std::make_unique<Classic_query_attribute>(att.get_string());
          case shcore::Value_type::Bool:
            return std::make_unique<Classic_query_attribute>(att.as_int());
          case shcore::Value_type::Integer:
            return std::make_unique<Classic_query_attribute>(att.as_int());
          case shcore::Value_type::Float:
            return std::make_unique<Classic_query_attribute>(att.as_double());
          case shcore::Value_type::UInteger:
            return std::make_unique<Classic_query_attribute>(att.as_uint());
          case shcore::Value_type::Null:
            return std::make_unique<Classic_query_attribute>();
          case shcore::Value_type::ObjectBridge: {
            if (auto date = att.as_object_bridge<Date>(); date) {
              MYSQL_TIME time;
              std::memset(&time, 0, sizeof(time));

              time.year = date->get_year();
              time.month = date->get_month();
              time.day = date->get_day();
              time.hour = date->get_hour();
              time.minute = date->get_min();
              time.second = date->get_sec();
              time.second_part = date->get_usec();

              enum_field_types type = MYSQL_TYPE_TIMESTAMP;
              time.time_type = MYSQL_TIMESTAMP_DATETIME;
              if (date->has_date()) {
                if (!date->has_time()) {
                  type = MYSQL_TYPE_DATE;
                  time.time_type = MYSQL_TIMESTAMP_DATE;
                }
              } else {
                type = MYSQL_TYPE_TIME;
                time.time_type = MYSQL_TIMESTAMP_TIME;
              }
              return std::make_unique<Classic_query_attribute>(time, type);
            }
            break;
          }
          default:
            // This should never happen
            assert(false);
            break;
        }

        return {};
      });
}

void Session::reset() {
  if (_mysql == nullptr) return;

  if (_prev_result) {
    _prev_result.reset();
  } else {
    MYSQL_RES *unread_result = mysql_use_result(_mysql);
    mysql_free_result(unread_result);
  }

  // Discards any pending result
  while (mysql_next_result(_mysql) == 0) {
    MYSQL_RES *trailing_result = mysql_use_result(_mysql);
    mysql_free_result(trailing_result);
  }
}

std::shared_ptr<IResult> Session::run_sql(const std::string &sql) {
  auto attributes = query_attributes();

  mysql_harness::ScopedCallback clean_query_attributes(
      [this]() { m_query_attributes.clear(); });

  return run_sql(sql.c_str(), sql.size(), false, false, attributes);
}

std::shared_ptr<IResult> Session::run_sql(
    const char *sql, size_t len, bool buffered, bool is_udf,
    const std::vector<Query_attribute> &query_attributes) {
  if (_mysql == nullptr) throw std::runtime_error("Not connected");
  shcore::utils::Profile_timer timer;
  timer.stage_begin("run_sql");
  reset();

  // TODO(rennox): enable logging

  // auto log_sql_handler = shcore::current_log_sql();
  // log_sql_handler->log(get_thread_id(), std::string_view{sql, len});

  auto process_error = [this](std::string_view /*sql_script*/) {
    auto err = mysqlrouter::MySQLSession::Error(
        mysql_error(_mysql), mysql_errno(_mysql), mysql_sqlstate(_mysql));

    // shcore::current_log_sql()->log(get_thread_id(), sql_script, err);

    return err;
  };

  // Attribute references need to be alive while the query is executed
  const char *attribute_names[K_MAX_QUERY_ATTRIBUTES];
  MYSQL_BIND attribute_values[K_MAX_QUERY_ATTRIBUTES];

  if (!query_attributes.empty()) {
    memset(attribute_values, 0, sizeof(attribute_values));
    size_t attribute_count = 0;
    for (const auto &att : query_attributes) {
      attribute_names[attribute_count] = att.name.data();
      const auto &value =
          dynamic_cast<Classic_query_attribute *>(att.value.get());

      attribute_values[attribute_count].buffer_type = value->type;
      attribute_values[attribute_count].buffer = value->data_ptr;
      attribute_values[attribute_count].length = &value->size;
      attribute_values[attribute_count].is_null = &value->is_null;
      attribute_values[attribute_count].is_unsigned =
          (value->flags & UNSIGNED_FLAG) ? true : false;
      attribute_count++;
    }

    mysql_bind_param(_mysql, attribute_count, attribute_values,
                     attribute_names);
  }

  if (mysql_real_query(_mysql, sql, len) != 0) {
    throw process_error({sql, len});
  }

  // warning count can only be obtained after consuming the result data
  std::shared_ptr<DbResult> result(
      new DbResult(shared_from_this(), mysql_affected_rows(_mysql),
                   mysql_insert_id(_mysql), mysql_info(_mysql), buffered));

  /*
  Because of the way UDFs are implemented in the server, they don't return
  errors the same way "regular" functions do. In short, they have two parts: an
  initialization part and a fecth data part, which means that we may only know
  if the UDF returned an error when we try and read the results returned.

  This also means that calling mysql_store_result (which executes both parts in
  sequence), is completly different than calling mysql_use_result (which only
  does the first part because the data must be read explicitly). Hence the code
  below:
   - if buffered (mysql_store_result) then the data was already read and so we
  only need to check for errors (like in a non UDF) if no result was returned
   - if not buffered (mysql_use_result), then we need to fetch the first row to
  check for errors (this row is read but kept in result to avoid being lost)
  */
  if (is_udf) {
    if (buffered && !result->has_resultset()) {
      throw process_error({sql, len});
    } else if (!buffered) {
      result->pre_fetch_row();  // this will throw in case of error
    }
  }

  timer.stage_end();
  result->set_execution_time(timer.total_seconds_elapsed());
  return std::static_pointer_cast<IResult>(result);
}

template <class T>
static void free_result(T *result) {
  mysql_free_result(result);
  result = NULL;
}

bool Session::next_resultset() {
  if (_prev_result) _prev_result.reset();

  int rc = mysql_next_result(_mysql);

  if (rc > 0) {
    throw mysqlrouter::MySQLSession::Error(
        mysql_error(_mysql), mysql_errno(_mysql), mysql_sqlstate(_mysql));
  }

  return rc == 0;
}

void Session::prepare_fetch(DbResult *target) {
  MYSQL_RES *result;

  if (target->is_buffered())
    result = mysql_store_result(_mysql);
  else
    result = mysql_use_result(_mysql);

  if (result)
    _prev_result = std::shared_ptr<MYSQL_RES>(result, &free_result<MYSQL_RES>);

  if (_prev_result) {
    // We need to update the received result object with the information
    // for the next result set
    target->reset(_prev_result);
  } else {
    // Update the result object for stmts that don't return a result
    target->reset(nullptr);
  }
}

Session::~Session() { _prev_result.reset(); }

std::vector<std::string> Session::get_last_gtids() const {
  const char *data;
  size_t length;
  std::vector<std::string> gtids;

  if (mysql_session_track_get_first(_mysql, SESSION_TRACK_GTIDS, &data,
                                    &length) == 0) {
    gtids.emplace_back(data, length);

    while (mysql_session_track_get_next(_mysql, SESSION_TRACK_GTIDS, &data,
                                        &length) == 0) {
      gtids.emplace_back(std::string(data, length));
    }
  }

  return gtids;
}

std::optional<std::string> Session::get_last_statement_id() const {
  const char *data;
  size_t length;
  std::optional<std::string> statement_id;

  if (mysql_session_track_get_first(_mysql, SESSION_TRACK_SYSTEM_VARIABLES,
                                    &data, &length) == 0) {
    bool found_statement_id = strncmp(data, "statement_id", length) == 0;

    while (!statement_id.has_value() &&
           mysql_session_track_get_next(_mysql, SESSION_TRACK_SYSTEM_VARIABLES,
                                        &data, &length) == 0) {
      if (found_statement_id) {
        statement_id = std::string(data, length);
      } else {
        found_statement_id = strncmp(data, "statement_id", length) == 0;
      }
    }
  }

  return statement_id;
}

}  // namespace database
}  // namespace polyglot
}  // namespace shcore
