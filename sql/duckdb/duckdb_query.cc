/*****************************************************************************

Copyright (c) 2025, Alibaba and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file sql/duckdb/duckdb_query.cc
 * Invoke duckdb to execute query.
 */

#include "duckdb_query.h"
#include <optional>
#include "duckdb/catalog/catalog_search_path.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/main/client_data.hpp"
#include "duckdb_context.h"
#include "duckdb_manager.h"
#include "my_time.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/strings/m_ctype.h"
#include "mysqld_error.h"
#include "sql/debug_sync.h"
#include "sql/duckdb/duckdb_context.h"
#include "sql/duckdb/duckdb_log.h"
#include "sql/item.h"
#include "sql/opt_explain.h"
#include "sql/protocol.h"
#include "sql/sql_class.h"  // THD
#include "sql/sql_lex.h"    // lex
#include "sql/sql_lex.h"
#include "sql/sql_time.h"
#include "sql/tztime.h"

//===--------------------------------------------------------------------===//
// Functions to execute query.
//===--------------------------------------------------------------------===//

namespace myduck {

static constexpr int INTERRUPT_CHECK_ROW = 256;

class DuckdbSchemaGuard {
 public:
  explicit DuckdbSchemaGuard(duckdb::ClientContext &context, bool enable = true)
      : m_context(context), m_enabled(enable) {
    if (m_enabled) {
      m_saved_entry =
          duckdb::ClientData::Get(context).catalog_search_path->GetDefault();
    }
  }

  ~DuckdbSchemaGuard() {
    /* Only restore if enabled and we have a saved entry */
    if (m_enabled && m_saved_entry.has_value()) {
      try {
        duckdb::ClientData::Get(m_context).catalog_search_path->Set(
            m_saved_entry.value(), duckdb::CatalogSetPathType::SET_SCHEMA);
      } catch (...) {
        /* Best-effort restore; ignore any exception during cleanup. */
      }
    }
  }

  /* Non-copyable, non-movable */
  DuckdbSchemaGuard(const DuckdbSchemaGuard &) = delete;
  DuckdbSchemaGuard &operator=(const DuckdbSchemaGuard &) = delete;

 private:
  duckdb::ClientContext &m_context;
  bool m_enabled;
  std::optional<duckdb::CatalogSearchEntry> m_saved_entry;
};

std::unique_ptr<duckdb::QueryResult> duckdb_query(THD *thd,
                                                  const std::string &query,
                                                  bool need_config,
                                                  bool keep_schema) {
  DuckdbThdContext *context = thd->get_duckdb_context();
  /* Config duckdb parameters before executing query. */
  if (need_config) {
    auto res = context->config_duckdb_env(thd, context->get_connection());
    if (res && res->HasError()) {
      return res;
    }
  }

  return duckdb_query(context->get_connection(), query, keep_schema);
}

std::unique_ptr<duckdb::QueryResult> duckdb_query(
    duckdb::Connection &connection, const std::string &query,
    bool keep_schema) {
  return duckdb_query(*connection.context, query, keep_schema);
}

std::unique_ptr<duckdb::QueryResult> duckdb_query(const std::string &query,
                                                  bool keep_schema) {
  auto connection = DuckdbManager::CreateConnection();
  return duckdb_query(*connection, query, keep_schema);
}

std::unique_ptr<duckdb::QueryResult> duckdb_query(
    duckdb::ClientContext &context, const std::string &query,
    bool keep_schema) {
  if (myduck::duckdb_log_options & LOG_DUCKDB_QUERY) {
    LogErr(INFORMATION_LEVEL, ER_DUCKDB, query.c_str());
  }

  /* Guard captures and restores schema only if keep_schema is true */
  DuckdbSchemaGuard schema_guard(context, keep_schema);

  auto res = context.Query(query, true);

  if (myduck::duckdb_log_options & LOG_DUCKDB_QUERY_RESULT) {
    if (res->HasError()) {
      LogErr(INFORMATION_LEVEL, ER_DUCKDB, res->GetError().c_str());
    }

    LogErr(INFORMATION_LEVEL, ER_DUCKDB, res->ToString().c_str());
  }

  return res;
}

bool duckdb_query_and_send(THD *thd, const std::string &query, bool send_result,
                           bool push_error) {
  DEBUG_SYNC(thd, "before_duckdb_query");

  if (thd->killed) {
    if (push_error) {
      my_error(ER_DUCKDB_CLIENT, MYF(0),
               "current query or connection was killed");
    }
    return true;
  }

  auto res = duckdb_query(thd, query, true);

  if (res->HasError()) {
    if (push_error) {
      my_error(ER_DUCKDB_CLIENT, MYF(0), res->GetError().c_str());
    }
    return true;
  } else if (send_result) {
    try {
      duckdb_send_result(thd, *res);
    } catch (std::exception &ex) {
      duckdb::ErrorData error(ex);
      if (push_error) {
        my_error(ER_DUCKDB_SEND_RESULT_ERROR, MYF(0),
                 error.RawMessage().c_str());
      }
      return true;
    }
  }
  return false;
}

static void duckdb_send_explain_result(THD *thd, duckdb::QueryResult &result) {
  assert(result.statement_type == duckdb::StatementType::EXPLAIN_STATEMENT);

  Query_result_send my_result;
  {
    mem_root_deque<Item *> field_list(thd->mem_root);
    Item *item =
        new Item_empty_string("EXPLAIN FROM DUCKDB", 78, system_charset_info);
    field_list.push_back(item);
    if (my_result.send_result_set_metadata(
            thd, field_list, Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF)) {
      my_result.send_eof(thd);
      return;
    }
  }

  std::string plan_str;
  std::unique_ptr<duckdb::DataChunk> data_chunk = result.Fetch();
  while (true) {
    if (!data_chunk || !data_chunk->size()) {
      break;
    }
    assert(data_chunk->ColumnCount() == 2);
    for (idx_t row_idx = 0; row_idx < data_chunk->size(); ++row_idx) {
      plan_str += data_chunk->GetValue(0, row_idx).ToString();
      plan_str += "\n";
      plan_str += data_chunk->GetValue(1, row_idx).ToString();
    }
    data_chunk = result.Fetch();
  }
  if (plan_str.empty()) {
    my_error(ER_INTERNAL_ERROR, MYF(0), "Failed to get query plan from duckdb");
    my_result.send_eof(thd);
    return;
  }

  {
    mem_root_deque<Item *> field_list(thd->mem_root);
    Item *item =
        new Item_string(plan_str.data(), plan_str.size(), system_charset_info);
    field_list.push_back(item);

    if (thd->killed) {
      thd->raise_warning(ER_QUERY_INTERRUPTED);
    }

    if (my_result.send_data(thd, field_list)) {
      my_result.send_eof(thd);
      return;
    }
  }

  my_result.send_eof(thd);
}

// TODO: extract Result_convertor for converting from QueryResult to item_list
void duckdb_send_result(THD *thd, duckdb::QueryResult &result) {
  if (result.statement_type == duckdb::StatementType::EXPLAIN_STATEMENT) {
    thd->status_var.com_duckdb_explain++;
    duckdb_send_explain_result(thd, result);
    return;
  }

  DEBUG_SYNC(thd, "wait_duckdb_send_result");
  mem_root_deque<Item *> visible_field_list(thd->mem_root);
  mem_root_deque<Item *> *field_list = nullptr;

  std::vector<result_template> mysql_data_types;

  Query_expression *unit = thd->lex->unit;
  /**
    Depending on the sql type, we need to get field_list from three different
    places.
    1. Simple query without any set operatoin
    2. There is a set operation in SQL and a temporary table needs to be
    materialized, such as union.
    3. There is a set operation in SQL and it can be executed in streaming mode.
  */
  if (unit->is_simple())
    field_list = &(down_cast<Query_block *>(unit->query_term()))->fields;
  else if (unit->set_operation()->is_materialized())
    field_list = &(unit->query_term()->query_block()->fields);
  else
    field_list = unit->query_term()->fields();
  for (Item *item : VisibleFields(*field_list)) {
    visible_field_list.push_back(item);
    mysql_data_types.push_back(
        result_template{item->data_type(), item->unsigned_flag, item->decimals,
                        item->collation.collation});
  }

  thd->send_result_metadata(visible_field_list,
                            Protocol::SEND_NUM_ROWS | Protocol::SEND_EOF);

  std::unique_ptr<duckdb::DataChunk> data_chunk = result.Fetch();
  if (result.statement_type == duckdb::StatementType::INSERT_STATEMENT) {
    my_eof(thd);
    return;
  }

  /* DuckDB returns results without going through `Query_result`, therefore it
  does not support server-side cursors. In `mysqld_stmt_execute`, server-side
  cursors are disabled for DuckDB, meaning `SERVER_STATUS_CURSOR_EXIST` returns
  false. JDBC has a Bug#112090, when handles this situation, jdbc client will be
  blocked forever. This bug is fixed by jdbc version 9.5.0 in commit#1e484a2.
  Therefore, for empty query results without rows, we send an additional EOF
  packet. */
  if ((!data_chunk || data_chunk->size() == 0) && thd->stmt_arena &&
      thd->stmt_arena->is_duckdb_cursor_disabled &&
      thd->variables.duckdb_psmt_cursor_send_extra_eof) {
    thd->get_protocol()->send_eof(thd->server_status, 0);
    my_eof(thd);
    return;
  }

  while (true) {
    if (!data_chunk || data_chunk->size() == 0) {
      my_eof(thd);
      return;
    }
    assert(data_chunk->ColumnCount() == result.types.size());

    Protocol *protocol = thd->get_protocol();
    for (uint64_t row_index = 0; row_index < data_chunk->size(); row_index++) {
      DBUG_EXECUTE_IF("simulate_interrupt_duckdb_row", {
        sleep(1);
        row_index = 256;
      });
      if (thd->duckdb_context->get_connection().context->interrupted &&
          row_index % INTERRUPT_CHECK_ROW == 0) {
#ifndef NDEBUG
        fprintf(stderr, "Interrupt when fetching rows\n");
#endif
        throw duckdb::InterruptException();
      }

      protocol->start_row();

      idx_t mysql_col = 0;
      for (idx_t col = 0; col < data_chunk->ColumnCount();) {
        std::string col_name = result.ColumnName(col);
        result_template &mysql_type = mysql_data_types[mysql_col];
        auto value = data_chunk->GetValue(col, row_index);
        if (value.IsNull()) {
          protocol->store_null();
        } else {
          switch (mysql_type.type) {
            case MYSQL_TYPE_TINY_BLOB:
            case MYSQL_TYPE_MEDIUM_BLOB:
            case MYSQL_TYPE_LONG_BLOB:
            case MYSQL_TYPE_BLOB:
            case MYSQL_TYPE_GEOMETRY:
            case MYSQL_TYPE_BIT:
            case MYSQL_TYPE_VARCHAR:
            case MYSQL_TYPE_VAR_STRING:
            case MYSQL_TYPE_STRING: {
              if (mysql_type.cs != &my_charset_bin) {
                auto str = value.GetValue<duckdb::string>();
                auto varchar = str.c_str();
                auto varchar_len = str.size();

                /* DuckDB store latin1 column's data as utf8mb4. */
                if (strcmp(mysql_type.cs->csname, "latin1") == 0) {
                  const CHARSET_INFO *utf8mb4_cs =
                      myduck::get_utf8mb4_charset_for_latin1(mysql_type.cs);
                  /* Here we donot do the conversion, but use utf8mb4 as input.
                  protocol will convert to character_set_result if need. */
                  protocol->store_string(varchar, varchar_len, utf8mb4_cs);
                } else {
                  protocol->store_string(varchar, varchar_len, mysql_type.cs);
                }
              } else {
                auto str = value.GetValueUnsafe<duckdb::string>();
                auto varchar = str.c_str();
                auto varchar_len = str.size();
                if (value.type().id() == duckdb::LogicalTypeId::BIT) {
                  auto padding = *(reinterpret_cast<const uint8_t *>(varchar));
                  str[1] = str[1] & ((1 << (8 - padding)) - 1);
                  varchar = str.c_str() + 1;
                  varchar_len -= 1;
                }
                protocol->store_string(varchar, varchar_len, &my_charset_bin);
              }
              break;
            }
            case MYSQL_TYPE_JSON:
            case MYSQL_TYPE_NULL:
            case MYSQL_TYPE_BOOL:
            case MYSQL_TYPE_INVALID:
            case MYSQL_TYPE_DECIMAL:
            case MYSQL_TYPE_ENUM:
            case MYSQL_TYPE_SET:
            case MYSQL_TYPE_NEWDECIMAL: {
              auto str = value.GetValue<duckdb::string>();
              auto varchar = str.c_str();
              auto varchar_len = str.size();
              protocol->store_string(varchar, varchar_len, system_charset_info);
              break;
            }
            case MYSQL_TYPE_TINY: {
              int64_t v = value.GetValue<int64_t>();
              protocol->store_tiny(v);
              break;
            }
            case MYSQL_TYPE_YEAR:
            case MYSQL_TYPE_SHORT: {
              int64_t v = value.GetValue<int64_t>();
              protocol->store_short(v);
              break;
            }
            case MYSQL_TYPE_INT24:
            case MYSQL_TYPE_LONG: {
              int64_t v = value.GetValue<int64_t>();
              protocol->store_long(v);
              break;
            }
            case MYSQL_TYPE_LONGLONG: {
              int64_t v;
              if (mysql_type.is_unsigned) {
                v = value.GetValue<uint64_t>();
              } else {
                v = value.GetValue<int64_t>();
              }
              protocol->store_longlong(v, mysql_type.is_unsigned);
              break;
            }
            case MYSQL_TYPE_DATE: {
              MYSQL_TIME tm;
              duckdb::date_t v =
                  value.GetValue<duckdb::date_t>() + days_at_timestart;
              get_date_from_daynr(v.days, &tm.year, &tm.month, &tm.day);
              protocol->store_date(Date_val(tm.year, tm.month, tm.day));
              break;
            }
            case MYSQL_TYPE_FLOAT: {
              float v = value.GetValue<float>();
              protocol->store_float(v, mysql_type.decimals, 0);
              break;
            }
            case MYSQL_TYPE_DOUBLE: {
              double v = value.GetValue<double>();
              protocol->store_double(v, mysql_type.decimals, 0);
              break;
            }
            case MYSQL_TYPE_DATETIME: {
              Time_zone *time_zone =
                  (value.type().id() == duckdb::LogicalTypeId::TIMESTAMP_TZ)
                      ? thd->time_zone()
                      : my_tz_UTC;

              int64_t v = value.GetValue<int64_t>();
              MYSQL_TIME ltime;
              my_timeval tm;
              if (v < 0) {
                my_micro_time_to_timeval(0, &tm);
                time_zone->gmt_sec_to_TIME(&ltime, tm);

                Interval interval{};
                interval.neg = true;
                interval.second_part = -v;
                date_add_interval_with_warn(thd, &ltime, INTERVAL_MICROSECOND,
                                            interval);
              } else {
                my_micro_time_to_timeval(v, &tm);
                time_zone->gmt_sec_to_TIME(&ltime, tm);
              }
              protocol->store_datetime(ltime, mysql_type.decimals);
              break;
            }
            case MYSQL_TYPE_TIMESTAMP: {
              int64_t v = value.GetValue<int64_t>();
              MYSQL_TIME ltime;
              my_timeval tm;
              my_micro_time_to_timeval(v, &tm);
              thd->time_zone()->gmt_sec_to_TIME(&ltime, tm);
              protocol->store_datetime(ltime, mysql_type.decimals);
              break;
            }
            case MYSQL_TYPE_TIME: {
              const auto time_value =
                  value.type().id() == duckdb::LogicalTypeId::TIME
                      ? value
                      : value.CastAs(*thd->get_duckdb_context()
                                          ->get_connection()
                                          .context,
                                     duckdb::LogicalType::TIME);
              int64_t v = time_value.GetValue<int64_t>();
              bool negative = v < 0;
              if (negative) v = -v;
              MYSQL_TIME ltime;
              ltime.second_part = v % 1000000;
              v /= 1000000;
              ltime.second = v % 60;
              v /= 60;
              ltime.minute = v % 60;
              v /= 60;
              ltime.hour = v;
              ltime.year = ltime.month = ltime.day = 0;
              ltime.neg = negative;
              ltime.time_zone_displacement = 0;
              ltime.time_type = MYSQL_TIMESTAMP_TIME;
              protocol->store_time(Time_val(ltime), mysql_type.decimals);
              break;
            }
            default:
              // TODO: no support
              assert(0);
              break;
          }
        }
        col++;
        mysql_col++;
      }
      protocol->end_row();
    }

    DBUG_EXECUTE_IF("simulate_interrupt_duckdb_chunk", { sleep(1); });
    if (thd->duckdb_context->get_connection().context->interrupted) {
#ifndef NDEBUG
      fprintf(stderr, "Interrupt when fetching chunks\n");
#endif
      throw duckdb::InterruptException();
    }
    data_chunk = result.Fetch();
  }
}

std::string BytesToHumanReadableString(uint64_t bytes, uint64_t multiplier) {
  std::string str =
      duckdb::StringUtil::BytesToHumanReadableString(bytes, multiplier);
  str.erase(std::remove(str.begin(), str.end(), ' '), str.end());
  return str;
}
}  // namespace myduck
