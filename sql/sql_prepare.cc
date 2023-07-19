/* Copyright (c) 2002, 2023, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @file

This file contains the implementation of prepared statements.

When one prepares a statement:

  - Server gets the query from client with command 'COM_STMT_PREPARE';
    in the following format:
    [COM_STMT_PREPARE:1] [query]
  - Parse the query and recognize any parameter markers '?' and
    store its information list in lex->param_list
  - Allocate a new statement for this prepare; and keep this in
    'thd->stmt_map'.
  - Without executing the query, return back to client the total
    number of parameters along with result-set metadata information
    (if any) in the following format:
    @verbatim
    [STMT_ID:4]
    [Column_count:2]
    [Param_count:2]
    [Params meta info (stubs only for now)]  (if Param_count > 0)
    [Columns meta info] (if Column_count > 0)
    @endverbatim

  During prepare the tables used in a statement are opened, but no
  locks are acquired.  Table opening will block any DDL during the
  operation, and we do not need any locks as we neither read nor
  modify any data during prepare.  Tables are closed after prepare
  finishes.

When one executes a statement:

  - Server gets the command 'COM_STMT_EXECUTE' to execute the
    previously prepared query. If there are any parameter markers, then the
    client will send the data in the following format:
    @verbatim
    [COM_STMT_EXECUTE:1]
    [STMT_ID:4]
    [NULL_BITS:(param_count+7)/8)]
    [TYPES_SUPPLIED_BY_CLIENT(0/1):1]
    [[length]data]
    [[length]data] .. [[length]data].
    @endverbatim
    (Note: Except for string/binary types; all other types will not be
    supplied with length field)
  - If it is a first execute or types of parameters were altered by client,
    then setup the conversion routines.
  - Assign parameter items from the supplied data.
  - Execute the query without re-parsing and send back the results
    to client

  During execution of prepared statement tables are opened and locked
  the same way they would for normal (non-prepared) statement
  execution.  Tables are unlocked and closed after the execution.

When one supplies long data for a placeholder:

  - Server gets the long data in pieces with command type
    'COM_STMT_SEND_LONG_DATA'.
  - The packet received will have the format as:
    [COM_STMT_SEND_LONG_DATA:1][STMT_ID:4][parameter_number:2][data]
  - data from the packet is appended to the long data value buffer for this
    placeholder.
  - It's up to the client to stop supplying data chunks at any point. The
    server doesn't care; also, the server doesn't notify the client whether
    it got the data or not; if there is any error, then it will be returned
    at statement execute.
*/

#include "sql/sql_prepare.h"

#include "my_config.h"

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <limits>
#include <memory>
#include <unordered_map>
#include <utility>

#include "decimal.h"
#include "field_types.h"
#include "m_ctype.h"
#include "m_string.h"
#include "map_helpers.h"
#include "my_alloc.h"
#include "my_byteorder.h"
#include "my_command.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_sqlcommand.h"
#include "my_sys.h"
#include "my_time.h"
#include "mysql/com_data.h"
#include "mysql/components/services/log_shared.h"
#include "mysql/plugin_audit.h"
#include "mysql/psi/mysql_mutex.h"
#include "mysql/psi/mysql_ps.h"  // MYSQL_EXECUTE_PS
#include "mysql/udf_registration_types.h"
#include "mysql_com.h"
#include "mysql_time.h"
#include "mysqld_error.h"
#include "scope_guard.h"
#include "sql/auth/auth_acls.h"
#include "sql/auth/auth_common.h"  // check_table_access
#include "sql/auth/sql_security_ctx.h"
#include "sql/binlog.h"
#include "sql/debug_sync.h"
#include "sql/derror.h"  // ER_THD
#include "sql/field.h"
#include "sql/handler.h"
#include "sql/item.h"
#include "sql/item_func.h"  // user_var_entry
#include "sql/log.h"        // query_logger
#include "sql/mdl.h"
#include "sql/my_decimal.h"
#include "sql/mysqld.h"     // opt_general_log
#include "sql/opt_trace.h"  // Opt_trace_array
#include "sql/protocol.h"
#include "sql/protocol_classic.h"
#include "sql/psi_memory_key.h"
#include "sql/query_options.h"
#include "sql/query_result.h"
#include "sql/resourcegroups/resource_group_basic_types.h"
#include "sql/resourcegroups/resource_group_mgr.h"
#include "sql/session_tracker.h"
#include "sql/set_var.h"    // set_var_base
#include "sql/sp_cache.h"   // sp_cache_enforce_limit
#include "sql/sql_audit.h"  // mysql_global_audit_mask
#include "sql/sql_base.h"   // open_tables_for_query, open_temporary_table
#include "sql/sql_class.h"
#include "sql/sql_cmd.h"
#include "sql/sql_cmd_ddl_table.h"
#include "sql/sql_const.h"
#include "sql/sql_cursor.h"  // Server_side_cursor
#include "sql/sql_db.h"      // mysql_change_db
#include "sql/sql_digest_stream.h"
#include "sql/sql_handler.h"  // mysql_ha_rm_tables
#include "sql/sql_insert.h"   // Query_result_create
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_parse.h"  // sql_command_flags
#include "sql/sql_profile.h"
#include "sql/sql_query_rewrite.h"
#include "sql/sql_rewrite.h"  // mysql_rewrite_query
#include "sql/sql_view.h"     // create_view_precheck
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql/thd_raii.h"
#include "sql/thr_malloc.h"
#include "sql/transaction.h"  // trans_rollback_implicit
#include "sql/window.h"
#include "sql_string.h"
#include "violite.h"

namespace resourcegroups {
class Resource_group;
}  // namespace resourcegroups

using std::max;
using std::min;

/****************************************************************************/

namespace {

/**
  Execute one SQL statement in an isolated context.
*/

class Execute_sql_statement : public Server_runnable {
 public:
  Execute_sql_statement(LEX_STRING sql_text);
  bool execute_server_code(THD *thd) override;

 private:
  LEX_STRING m_sql_text;
};

/**
  A result class used to send cursor rows using the binary protocol.
*/

class Query_fetch_protocol_binary final : public Query_result_send {
  Protocol_binary protocol;

 public:
  explicit Query_fetch_protocol_binary(THD *thd)
      : Query_result_send(), protocol(thd) {}
  bool send_result_set_metadata(THD *thd, const mem_root_deque<Item *> &list,
                                uint flags) override;
  bool send_data(THD *thd, const mem_root_deque<Item *> &items) override;
  bool send_eof(THD *thd) override;
};

}  // namespace

/**
  Protocol_local: a helper class to intercept the result
  of the data written to the network.

  At the start of every result set, start_result_metadata allocates m_rset to
  prepare for the results. The metadata is stored on m_current_row which will
  be transferred to m_fields in end_result_metadata. The memory for the
  metadata is allocated on m_rset_root.

  Then, for every row of the result received, each of the fields is stored in
  m_current_row. Then the row is moved to m_rset and m_current_row is cleared
  to receive the next row. The memory for all the results are also stored in
  m_rset_root.

  Finally, at the end of the result set, a new instance of Ed_result_set is
  created on m_rset_root and the result set (m_rset and m_fields) is moved into
  this instance. The ownership of MEM_ROOT m_rset_root is also transferred to
  this instance. So, at the end we have a fresh MEM_ROOT, cleared m_rset and
  m_fields to accept the next result set.
*/

class Protocol_local final : public Protocol {
 public:
  Protocol_local(THD *thd, Ed_connection *ed_connection);
  ~Protocol_local() override { m_rset_root.Clear(); }

  int read_packet() override;

  int get_command(COM_DATA *com_data, enum_server_command *cmd) override;
  ulong get_client_capabilities() override;
  bool has_client_capability(unsigned long client_capability) override;
  void end_partial_result_set() override;
  int shutdown(bool server_shutdown = false) override;
  bool connection_alive() const override;
  void start_row() override;
  bool end_row() override;
  void abort_row() override {}
  uint get_rw_status() override;
  bool get_compression() override;

  char *get_compression_algorithm() override;
  uint get_compression_level() override;

  bool start_result_metadata(uint num_cols, uint flags,
                             const CHARSET_INFO *resultcs) override;
  bool end_result_metadata() override;
  bool send_field_metadata(Send_field *field,
                           const CHARSET_INFO *charset) override;
  bool flush() override { return true; }
  bool send_parameters(List<Item_param> *, bool) override { return false; }
  bool store_ps_status(ulong, uint, uint, ulong) override { return false; }

 protected:
  bool store_null() override;
  bool store_tiny(longlong from, uint32) override;
  bool store_short(longlong from, uint32) override;
  bool store_long(longlong from, uint32) override;
  bool store_longlong(longlong from, bool unsigned_flag, uint32) override;
  bool store_decimal(const my_decimal *, uint, uint) override;
  bool store_string(const char *from, size_t length,
                    const CHARSET_INFO *cs) override;
  bool store_datetime(const MYSQL_TIME &time, uint precision) override;
  bool store_date(const MYSQL_TIME &time) override;
  bool store_time(const MYSQL_TIME &time, uint precision) override;
  bool store_float(float value, uint32 decimals, uint32 zerofill) override;
  bool store_double(double value, uint32 decimals, uint32 zerofill) override;
  bool store_field(const Field *field) override;

  enum enum_protocol_type type() const override { return PROTOCOL_LOCAL; }
  enum enum_vio_type connection_type() const override { return VIO_TYPE_LOCAL; }

  bool send_ok(uint server_status, uint statement_warn_count,
               ulonglong affected_rows, ulonglong last_insert_id,
               const char *message) override;

  bool send_eof(uint server_status, uint statement_warn_count) override;
  bool send_error(uint sql_errno, const char *err_msg,
                  const char *sqlstate) override;

 private:
  bool store_string(const char *str, size_t length, const CHARSET_INFO *src_cs,
                    const CHARSET_INFO *dst_cs);

  bool store_column(const void *data, size_t length);
  void opt_add_row_to_rset();

  Ed_connection *m_connection;
  MEM_ROOT m_rset_root;
  List<Ed_row> *m_rset;
  size_t m_column_count;
  Ed_column *m_current_row;
  Ed_column *m_current_column;
  Ed_row *m_fields;
  bool m_send_metadata;
  THD *m_thd;
};

/******************************************************************************
  Implementation
******************************************************************************/

/**
  Rewrite the current query (to obfuscate passwords etc.) if needed
  (i.e. only if we'll be writing the query to any of our logs).

  Side-effect: thd->rewritten_query() may be populated with a rewritten
               query.  If the query is not of a rewritable type,
               thd->rewritten_query() will be empty.

  @param thd                thread handle
*/
static inline void rewrite_query_if_needed(THD *thd) {
  bool general =
      (opt_general_log && !(opt_general_log_raw || thd->slave_thread));

  if ((thd->sp_runtime_ctx == nullptr) &&
      (general || opt_slow_log || opt_bin_log)) {
    /*
      thd->m_rewritten_query may already contain "PREPARE stmt FROM ..."
      at this point, so we reset it here so mysql_rewrite_query()
      won't complain.
    */
    thd->reset_rewritten_query();
    /*
      Now replace the "PREPARE ..." with the obfuscated version of the
      actual query were prepare.
    */
    mysql_rewrite_query(thd);
  }
}

/**
  Unless we're doing dynamic SQL, write the current query to the
  general query log if it's open.  If we have a rewritten version
  of the query, use that instead of the "raw" one.

  Side-effect: query may be written to general log if it's open.

  @param thd                thread handle
*/
static inline void log_execute_line(THD *thd) {
  /*
    Do not print anything if this is an SQL prepared statement and
    we're inside a stored procedure (also called Dynamic SQL) --
    sub-statements inside stored procedures are not logged into
    the general log.
  */
  if (thd->sp_runtime_ctx != nullptr) return;

  if (thd->rewritten_query().length())
    query_logger.general_log_write(thd, COM_STMT_EXECUTE,
                                   thd->rewritten_query().ptr(),
                                   thd->rewritten_query().length());
  else
    query_logger.general_log_write(thd, COM_STMT_EXECUTE, thd->query().str,
                                   thd->query().length);
}

class Statement_backup {
  LEX *m_lex;
  LEX_CSTRING m_query_string;
  String m_rewritten_query;
  bool m_safe_to_display;

 public:
  LEX *lex() const { return m_lex; }

  /**
    Prepared the THD to execute the prepared statement.
    Save the current THD statement state.
  */
  void set_thd_to_ps(THD *thd, Prepared_statement *stmt) {
    DBUG_TRACE;

    mysql_mutex_lock(&thd->LOCK_thd_data);
    m_lex = thd->lex;
    thd->lex = stmt->m_lex;
    mysql_mutex_unlock(&thd->LOCK_thd_data);

    m_query_string = thd->query();
    thd->set_query(stmt->m_query_string);

    m_safe_to_display = thd->safe_to_display();

    /* Keep the current behaviour of displaying prepared statements always by
    default. This can be changed in future if required. */
    thd->set_safe_display(true);

    return;
  }

  /**
    Restore the THD statement state after the prepared
    statement has finished executing.
  */
  void restore_thd(THD *thd, Prepared_statement *stmt) {
    DBUG_TRACE;

    mysql_mutex_lock(&thd->LOCK_thd_data);
    stmt->m_lex = thd->lex;
    thd->lex = m_lex;
    mysql_mutex_unlock(&thd->LOCK_thd_data);

    thd->set_safe_display(m_safe_to_display);

    stmt->m_query_string = thd->query();
    thd->set_query(m_query_string);
  }

  /**
    Save the current rewritten query prior to
    rewriting the prepared statement.
  */
  void save_rlb(THD *thd) {
    DBUG_TRACE;

    if (thd->rewritten_query().length() > 0) {
      /* Duplicate the original rewritten query. */
      m_rewritten_query.copy(thd->rewritten_query());
      /* Swap the duplicate with the original. */
      thd->swap_rewritten_query(m_rewritten_query);
    }

    return;
  }

  /**
    Restore the rewritten query after the prepared
    statement has finished executing.
  */
  void restore_rlb(THD *thd) {
    DBUG_TRACE;

    if (m_rewritten_query.length() > 0) {
      /* Restore with swap() instead of '='. */
      thd->swap_rewritten_query(m_rewritten_query);
      /* Free the rewritten prepared statement. */
      m_rewritten_query.mem_free();
    }

    return;
  }
};

/**
  Set source parameter type based on information from protocol buffer

  @param param         Parameter to set source type for
  @param type          Field type
  @param unsigned_type true if type is unsigned (applicable for numeric types)
  @param cs_source     The source collation for non-binary string parameters

  If parameter metadata is invalid, set source data type MYSQL_TYPE_INVALID.
*/
static void set_parameter_type(Item_param *param, enum enum_field_types type,
                               bool unsigned_type,
                               const CHARSET_INFO *cs_source) {
  param->set_data_type_source(type, unsigned_type);

  switch (param->data_type_source()) {
    case MYSQL_TYPE_NULL:
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_INT24:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
    case MYSQL_TYPE_FLOAT:
    case MYSQL_TYPE_DOUBLE:
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_NEWDECIMAL:
    case MYSQL_TYPE_DATE:
    case MYSQL_TYPE_TIME:
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_TIMESTAMP:
      break;

    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_BLOB: {
      param->set_collation_source(&my_charset_bin);
      break;
    }
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_JSON:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_STRING: {
      param->set_collation_source(cs_source);
      break;
    }
    default:
      param->set_data_type_source(MYSQL_TYPE_INVALID, false);
      break;
  }
}

/**
  Set parameter value from protocol buffer

  @param param   Parameter to set a value for
  @param pos     Pointer to parameter value in protocol buffer
  @param len     Length of parameter value

  @returns false on success, true on error

  The function reads a binary valuefrom pos, converts it to the requested type
  and assigns it to the paramameter.
*/
static bool set_parameter_value(Item_param *param, const uchar **pos,
                                ulong len) {
  switch (param->data_type_source()) {
    case MYSQL_TYPE_TINY: {
      assert(len >= 1);
      int8 value = (int8) * *pos;
      if (param->is_unsigned_actual())
        param->set_int((ulonglong)((uint8)value));
      else
        param->set_int((longlong)value);
      break;
    }
    case MYSQL_TYPE_SHORT: {
      assert(len >= 2);
      int16 value = sint2korr(*pos);
      if (param->is_unsigned_actual())
        param->set_int((ulonglong)((uint16)value));
      else
        param->set_int((longlong)value);
      break;
    }
    case MYSQL_TYPE_LONG: {
      assert(len >= 4);
      int32 value = sint4korr(*pos);
      if (param->is_unsigned_actual())
        param->set_int((ulonglong)((uint32)value));
      else
        param->set_int((longlong)value);
      break;
    }
    case MYSQL_TYPE_LONGLONG: {
      assert(len >= 8);
      longlong value = sint8korr(*pos);
      if (param->is_unsigned_actual())
        param->set_int((ulonglong)value);
      else
        param->set_int(value);
      break;
    }
    case MYSQL_TYPE_FLOAT: {
      assert(len >= 4);
      float data = float4get(*pos);
      param->set_double((double)data);
      break;
    }
    case MYSQL_TYPE_DOUBLE: {
      assert(len >= 8);
      double data = float8get(*pos);
      param->set_double(data);
      break;
    }
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_NEWDECIMAL: {
      param->set_decimal((const char *)*pos, len);
      break;
    }
    case MYSQL_TYPE_TIME: {
      MYSQL_TIME tm;
      if (len >= 8) {
        const uchar *to = *pos;
        tm.neg = (bool)to[0];
        uint day = (uint)sint4korr(to + 1);
        tm.hour = (uint)to[5] + day * 24;
        tm.minute = (uint)to[6];
        tm.second = (uint)to[7];
        tm.second_part = (len > 8) ? (ulong)sint4korr(to + 8) : 0;
        if (tm.hour > 838) {
          /* TODO: add warning 'Data truncated' here */
          tm.hour = 838;
          tm.minute = 59;
          tm.second = 59;
        }
        tm.day = tm.year = tm.month = 0;
      } else {
        set_zero_time(&tm, MYSQL_TIMESTAMP_TIME);
      }
      param->set_time(&tm, MYSQL_TIMESTAMP_TIME);
      break;
    }
    case MYSQL_TYPE_DATE: {
      MYSQL_TIME tm;
      if (len >= 4) {
        const uchar *to = *pos;
        tm.year = (uint)sint2korr(to);
        tm.month = (uint)to[2];
        tm.day = (uint)to[3];

        tm.hour = tm.minute = tm.second = 0;
        tm.second_part = 0;
        tm.neg = false;
      } else {
        set_zero_time(&tm, MYSQL_TIMESTAMP_DATE);
      }
      param->set_time(&tm, MYSQL_TIMESTAMP_DATE);
      break;
    }
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_TIMESTAMP: {
      MYSQL_TIME tm;
      enum_mysql_timestamp_type type = MYSQL_TIMESTAMP_DATETIME;
      const uchar *to = *pos;

      assert(len == 0 || len == 4 || len == 7 || len == 11 || len == 13);
      if (len < 4) {
        set_zero_time(&tm, MYSQL_TIMESTAMP_DATETIME);
      } else {
        tm.neg = false;
        tm.year = (uint)sint2korr(to);
        tm.month = (uint)to[2];
        tm.day = (uint)to[3];
      }
      if (len >= 7) {
        tm.hour = (uint)to[4];
        tm.minute = (uint)to[5];
        tm.second = (uint)to[6];
      } else {  // len == 4
        tm.hour = tm.minute = tm.second = 0;
      }
      tm.second_part =
          (len >= 11) ? static_cast<std::uint64_t>(sint4korr(to + 7)) : 0;

      if (len >= 13) {
        tm.time_zone_displacement = sint2korr(to + 11) * SECS_PER_MIN;
        type = MYSQL_TIMESTAMP_DATETIME_TZ;
      }
      param->set_time(&tm, type);
      break;
    }
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_BLOB: {
      param->set_str((const char *)*pos, len);

      break;
    }
    case MYSQL_TYPE_VARCHAR:
    case MYSQL_TYPE_JSON:
    case MYSQL_TYPE_VAR_STRING:
    case MYSQL_TYPE_STRING: {
      param->set_str((const char *)*pos, len);
      break;
    }
    default:
      /*
        The client library ensures that only the type codes listed above
        are encountered. The 'default' label makes it possible to handle
        malformed packets as well.
      */
      break;
  }
  return false;
}

/**
  Check whether this parameter data type is compatible with long data.
  Used to detect whether a long data stream has been supplied to a
  incompatible data type.
  Notice that VARCHAR type is MYSQL_TYPE_STRING, not MYSQL_TYPE_VARCHAR,
  when sent on protocol between client and server.
*/
inline bool is_param_long_data_type(Item_param *param) {
  return param->data_type_source() >= MYSQL_TYPE_TINY_BLOB &&
         param->data_type_source() <= MYSQL_TYPE_STRING;
}

/**
  Assign parameter values from data supplied by the client.
  If required, generate a valid non-parameterized query for logging.

  @param thd        current thread.
  @param query      The query with parameter markers replaced with values
                    supplied by user that were used to execute the query.
  @param has_new_types if true, new types of actual parameters are provided,
                       otherwise use the parameters from previous execution.
  @param parameters Array of actual parameter values.
                    Contains parameter types if has_new_types is true.

  @returns false if success, true if error

  @note
    m_with_log is set when one of slow or general logs are open.
    Logging of prepared statements in all cases is performed
    by means of regular queries: if parameter data was supplied from C API,
    each placeholder in the query is replaced with its actual value;
    if we're logging a [Dynamic] SQL prepared statement, parameter markers
    are replaced with variable names.
    Example:
    @verbatim
     mysqld_stmt_prepare("UPDATE t1 SET a=a*1.25 WHERE a=?")
       --> general logs gets [Prepare] UPDATE t1 SET a*1.25 WHERE a=?"
     mysqld_stmt_execute(stmt);
       --> general and binary logs get
                             [Execute] UPDATE t1 SET a*1.25 WHERE a=1"
    @endverbatim

    If a statement has been prepared using SQL syntax:
    @verbatim
     PREPARE stmt FROM "UPDATE t1 SET a=a*1.25 WHERE a=?"
       --> general log gets [Query] PREPARE stmt FROM "UPDATE ..."
     EXECUTE stmt USING @a
       --> general log gets [Query] EXECUTE stmt USING @a;
    @endverbatim
*/

bool Prepared_statement::insert_parameters(THD *thd, String *query,
                                           bool has_new_types,
                                           PS_PARAM *parameters) {
  DBUG_TRACE;

  Item_param **end = m_param_array + m_param_count;
  size_t length = 0;

  // Reserve an extra space of 32 bytes for each placeholder parameter.
  if (m_with_log && query->reserve(m_query_string.length + 32 * m_param_count))
    return true;

  uint i = 0;
  for (Item_param **it = m_param_array; it < end; ++it, i++) {
    Item_param *const param = *it;

    if (has_new_types) {
      set_parameter_type(param, parameters[i].type, parameters[i].unsigned_type,
                         thd->variables.character_set_client);
      // Client may have provided invalid metadata:
      if (param->data_type_source() == MYSQL_TYPE_INVALID) {
        my_error(ER_WRONG_ARGUMENTS, MYF(0), "mysqld_stmt_execute");
        return true;
      }
    }

    if (param->param_state() == Item_param::LONG_DATA_VALUE) {
      /*
        A long data stream was supplied for this parameter marker.
        This was done after prepare, prior to providing a placeholder
        type (the types are supplied at execute). Check that the
        supplied type of placeholder can accept a data stream.
      */
      if (!is_param_long_data_type(param)) {
        my_error(ER_WRONG_ARGUMENTS, MYF(0), "mysqld_stmt_execute");
        return true;
      }
      param->set_data_type_actual(MYSQL_TYPE_VARCHAR);
      // see Item_param::set_str() for explanation
      param->set_collation_actual(
          param->collation_source() == &my_charset_bin
              ? &my_charset_bin
              : param->collation.collation != &my_charset_bin
                    ? param->collation.collation
                    : current_thd->variables.collation_connection);

    } else if (parameters[i].null_bit) {
      param->set_null();
    } else {
      if (set_parameter_value(param, &parameters[i].value,
                              parameters[i].length)) {
        return true;
      }
      // NO_VALUE probably means broken client, no metadata provided.
      if (param->param_state() == Item_param::NO_VALUE) {
        my_error(ER_WRONG_ARGUMENTS, MYF(0), "mysqld_stmt_execute");
        return true;
      }

      // Pinning of data types only implemented for integers
      assert(!param->is_type_pinned() || param->result_type() == INT_RESULT);

      if (param->is_type_pinned()) {
        // Accept string values from client
        // @todo Validate string values, do not accept garbage in string
        if (param->data_type_actual() == MYSQL_TYPE_VARCHAR) {
          longlong val = param->val_int();
          if (param->unsigned_flag)
            param->set_int((ulonglong)val);
          else
            param->set_int(val);
        } else if (param->data_type_actual() != MYSQL_TYPE_LONGLONG) {
          my_error(ER_WRONG_ARGUMENTS, MYF(0), "mysqld_stmt_execute");
          return true;
        }
        if ((param->unsigned_flag && !param->is_unsigned_actual() &&
             param->value.integer < 0) ||
            (!param->unsigned_flag && param->is_unsigned_actual() &&
             param->value.integer < 0)) {
          my_error(ER_DATA_OUT_OF_RANGE, MYF(0), "signed integer",
                   "mysqld_stmt_execute");
          return true;
        }
      }
    }
    if (m_with_log) {
      String str;
      const String *val = param->query_val_str(thd, &str);
      if (val == nullptr) return true;
      if (param->convert_value()) return true; /* out of memory */

      size_t num_bytes = param->pos_in_query - length;
      if (query->length() + num_bytes + val->length() >
          std::numeric_limits<uint32>::max()) {
        my_error(ER_WRONG_ARGUMENTS, MYF(0), "mysqld_stmt_execute");
        return true;
      }
      if (query->append(m_query_string.str + length, num_bytes) ||
          query->append(*val))
        return true;

      length = param->pos_in_query + 1;
    } else {
      if (param->convert_value()) return true; /* out of memory */
    }
    param->sync_clones();
  }

  // Copy part of query string after last parameter marker
  if (m_with_log && query->append(m_query_string.str + length,
                                  m_query_string.length - length))
    return true;

  return false;
}

/**
  Copy parameter metada data from parameter array into current prepared stmt.

  @param from_param_array Parameter array to copy from.
*/
void Prepared_statement::copy_parameter_types(Item_param **from_param_array) {
  for (uint i = 0; i < m_param_count; ++i) {
    Item_param *from = from_param_array[i];
    Item_param *to = this->m_param_array[i];
    to->copy_param_actual_type(from);
  }
}

/**
  Setup data conversion routines using an array of parameter
  markers from the original prepared statement.
  Swap the parameter data of the original prepared
  statement to the new one.

  Used only when we re-prepare a prepared statement.
  There are two reasons for this function to exist:

  1) In the binary client/server protocol, parameter metadata
  is sent only at first execute. Consequently, if we need to
  reprepare a prepared statement at a subsequent execution,
  we may not have metadata information in the packet.
  In that case we use the parameter array of the original
  prepared statement to setup parameter types of the new
  prepared statement.

  2) In the binary client/server protocol, we may supply
  long data in pieces. When the last piece is supplied,
  we assemble the pieces and convert them from client
  character set to the connection character set. After
  that the parameter value is only available inside
  the parameter, the original pieces are lost, and thus
  we can only assign the corresponding parameter of the
  reprepared statement from the original value.

  @param[out]  param_array_dst  parameter markers of the new statement
  @param[in]   param_array_src  parameter markers of the original
                                statement
  @param[in]   param_count      total number of parameters. Is the
                                same in src and dst arrays, since
                                the statement query is the same
*/

static void swap_parameter_array(Item_param **param_array_dst,
                                 Item_param **param_array_src,
                                 uint param_count) {
  Item_param **dst = param_array_dst;
  Item_param **src = param_array_src;
  Item_param **end = param_array_dst + param_count;

  for (; dst < end; ++src, ++dst) {
    (*dst)->set_param_type_and_swap_value(*src);
    (*dst)->sync_clones();
    (*src)->sync_clones();
  }
}

/**
  Assign prepared statement parameters from user variables.
  If m_with_log is set, also construct query string for binary log.

  @param thd       Current thread.
  @param varnames  List of variables. Caller must ensure that number
                   of variables in the list is equal to number of statement
                   parameters
  @param query     The query with parameter markers replaced with corresponding
                   user variables that were used to execute the query.

  @returns false if success, true if error
*/

bool Prepared_statement::insert_parameters_from_vars(THD *thd,
                                                     List<LEX_STRING> &varnames,
                                                     String *query) {
  Item_param **end = m_param_array + m_param_count;
  List_iterator<LEX_STRING> var_it(varnames);
  StringBuffer<STRING_BUFFER_USUAL_SIZE> buf;
  size_t length = 0;

  DBUG_TRACE;

  // Reserve an extra space of 32 bytes for each placeholder parameter.
  if (m_with_log) query->reserve(m_query_string.length + 32 * m_param_count);

  // Protects thd->user_vars
  mysql_mutex_lock(&thd->LOCK_thd_data);

  for (Item_param **it = m_param_array; it < end; ++it) {
    Item_param *const param = *it;
    LEX_STRING *const varname = var_it++;

    user_var_entry *entry =
        find_or_nullptr(thd->user_vars, to_string(*varname));
    if (m_with_log) {
      if (entry != nullptr) {
        set_parameter_type(param, Item::result_to_type(entry->type()),
                           entry->unsigned_flag, entry->collation.collation);
      } else {
        set_parameter_type(param, param->data_type(), param->unsigned_flag,
                           param->collation.collation);
      }

      if (param->set_from_user_var(thd, entry)) goto error;
      const String *val = param->query_val_str(thd, &buf);
      if (val == nullptr) goto error;

      if (param->convert_value()) goto error;

      size_t num_bytes = param->pos_in_query - length;
      if (query->length() + num_bytes + val->length() >
          std::numeric_limits<uint32>::max())
        goto error;

      if (query->append(m_query_string.str + length, num_bytes) ||
          query->append(*val))
        goto error;

      length = param->pos_in_query + 1;
    } else {
      assert(false);
      if (param->set_from_user_var(thd, entry)) goto error;

      if (entry) length += entry->length();

      if (length > std::numeric_limits<uint32>::max() || param->convert_value())
        goto error;
    }
    param->sync_clones();
  }

  // Copy part of query string after last parameter marker
  if (m_with_log && query->append(m_query_string.str + length,
                                  m_query_string.length - length))
    goto error;

  mysql_mutex_unlock(&thd->LOCK_thd_data);
  return false;

error:
  mysql_mutex_unlock(&thd->LOCK_thd_data);
  return true;
}

static bool send_statement(THD *thd, const Prepared_statement *stmt,
                           uint no_columns, Query_result *result,
                           const mem_root_deque<Item *> *types) {
  // Send the statement status(id, no_columns, no_params, error_count);
  bool rc = thd->get_protocol()->store_ps_status(
      stmt->id(), no_columns, stmt->m_param_count,
      thd->get_stmt_da()->current_statement_cond_count());
  if (!rc && stmt->m_param_count != 0) {
    // Send the list of parameters
    mem_root_deque<Item *> param_list(thd->mem_root);
    for (Item_param &item : stmt->m_lex->param_list) {
      param_list.push_back(&item);
    }
    rc |= thd->send_result_metadata(param_list, Protocol::SEND_EOF);
  }
  if (rc) return true; /* purecov: inspected */

  // Send
  if (types && result &&
      result->send_result_set_metadata(thd, *types, Protocol::SEND_EOF))
    return true; /* purecov: inspected */

  // Flag that a response has already been sent
  thd->get_stmt_da()->disable_status();

  // Flush the result only if previous statements succeeded.
  if (!rc) {
    thd->get_stmt_da()->set_overwrite_status(true);
    rc |= thd->get_protocol()->flush();
    thd->get_stmt_da()->set_overwrite_status(false);
  }

  return rc;
}

/**
  Validate and prepare for execution SET statement expressions.

  @param thd                current thread
  @param stmt               prepared statement
  @param tables             list of tables used in this query
  @param var_list           list of expressions

  @returns false if success, true if error
*/

static bool mysql_test_set_fields(THD *thd,
                                  Prepared_statement *stmt [[maybe_unused]],
                                  Table_ref *tables,
                                  List<set_var_base> *var_list) {
  List_iterator_fast<set_var_base> it(*var_list);
  set_var_base *var;
  DBUG_TRACE;
  assert(stmt->m_arena.is_stmt_prepare());

  thd->lex->using_hypergraph_optimizer =
      thd->optimizer_switch_flag(OPTIMIZER_SWITCH_HYPERGRAPH_OPTIMIZER);

  if (tables &&
      check_table_access(thd, SELECT_ACL, tables, false, UINT_MAX, false))
    return true; /* purecov: inspected */

  if (open_tables_for_query(thd, tables, MYSQL_OPEN_FORCE_SHARED_MDL))
    return true; /* purecov: inspected */

  Prepared_stmt_arena_holder ps_arena_holder(thd);

  while ((var = it++)) {
    if (var->light_check(thd)) return true; /* purecov: inspected */
    var->cleanup();
  }

  thd->lex->unit->set_prepared();
  thd->lex->save_cmd_properties(thd);
  thd->lex->cleanup(false);

  return false;
}

/**
  @param thd       thread handler
  @param result    query result sink
  @param added_options options added to query block before preparation

  @returns false if success, true if error
*/

static bool select_like_stmt_test(THD *thd, Query_result *result,
                                  ulonglong added_options) {
  DBUG_TRACE;
  LEX *const lex = thd->lex;

  lex->query_block->context.resolve_in_select_list = true;

  if (lex->unit->prepare(thd, result, nullptr, added_options, 0)) {
    return true;
  }
  lex->save_cmd_properties(thd);

  return false;
}

/**
  Validate and prepare for execution CREATE TABLE statement.

  @param thd          Thread handle.

  @returns false if success, true if error
*/

bool Sql_cmd_create_table::prepare(THD *thd) {
  LEX *const lex = thd->lex;
  Query_block *query_block = lex->query_block;
  Table_ref *create_table = lex->query_tables;
  DBUG_TRACE;

  if (create_table_precheck(thd, query_expression_tables, create_table))
    return true;

  auto cleanup_se_guard = create_scope_guard(
      [lex] { lex->set_secondary_engine_execution_context(nullptr); });

  if (!query_block->fields.empty()) {
    /* Base table and temporary table are not in the same name space. */
    if (!(lex->create_info->options & HA_LEX_CREATE_TMP_TABLE))
      create_table->open_type = OT_BASE_ONLY;

    if (open_tables_for_query(thd, lex->query_tables,
                              MYSQL_OPEN_FORCE_SHARED_MDL))
      return true;

    query_block->context.resolve_in_select_list = true;

    // Use the hypergraph optimizer for the SELECT statement, if enabled.
    lex->using_hypergraph_optimizer =
        thd->optimizer_switch_flag(OPTIMIZER_SWITCH_HYPERGRAPH_OPTIMIZER);

    Prepared_stmt_arena_holder ps_arena_holder(thd);

    Query_result *result = new (thd->mem_root)
        Query_result_create(create_table, &query_block->fields, lex->duplicates,
                            query_expression_tables);
    if (result == nullptr) return true;

    bool link_to_local;
    lex->unlink_first_table(&link_to_local);
    bool res = select_like_stmt_test(thd, result, SELECT_NO_UNLOCK);
    lex->link_first_table_back(create_table, link_to_local);
    if (res) return true;
  } else {
    /*
      Check that the source table exist, and also record
      its metadata version. Even though not strictly necessary,
      we validate metadata of all CREATE TABLE statements,
      which keeps metadata validation code simple.
    */
    if (open_tables_for_query(thd, lex->query_tables,
                              MYSQL_OPEN_FORCE_SHARED_MDL))
      return true;
  }

  set_prepared();
  return false;
}

/**
  @brief Validate and prepare for execution CREATE VIEW statement

  @param thd  current thread
  @param stmt prepared statement

  @note This function handles create view commands.

  @returns false if success, true if error
  @retval false Operation was a success.
  @retval true An error occurred.
*/

static bool mysql_test_create_view(THD *thd, Prepared_statement *stmt) {
  DBUG_TRACE;
  assert(stmt->m_arena.is_stmt_prepare());

  LEX *lex = stmt->m_lex;
  Query_block *const select = lex->query_block;
  bool res = true;
  /* Skip first table, which is the view we are creating */
  bool link_to_local;
  Table_ref *view = lex->unlink_first_table(&link_to_local);
  Table_ref *tables = lex->query_tables;

  if (create_view_precheck(thd, tables, view, lex->create_view_mode)) goto err;

  /*
    Since we can't pre-open temporary tables for SQLCOM_CREATE_VIEW,
    (see mysql_create_view) we have to do it here instead.
  */
  if (open_temporary_tables(thd, tables)) goto err;

  if (open_tables_for_query(thd, tables, MYSQL_OPEN_FORCE_SHARED_MDL)) goto err;

  lex->context_analysis_only |= CONTEXT_ANALYSIS_ONLY_VIEW;

  {
    Prepared_stmt_arena_holder ps_arena_holder(thd);

    res = select_like_stmt_test(thd, nullptr, 0);
    if (res) goto err;

    /* Check if the auto generated column names are conforming. */
    make_valid_column_names(lex);

    /*
      Only column names of the first query block should be checked for
      duplication; any further UNION-ed part isn't used for determining
      names of the view's columns.
    */
    if (check_duplicate_names(view->derived_column_names(), select->fields,
                              true)) {
      res = true;
      goto err;
    }

    /*
      Make sure the view doesn't have so many columns that we hit the
      64k header limit if the view is materialized as a MyISAM table.
    */
    if (select->fields.size() > MAX_FIELDS) {
      my_error(ER_TOO_MANY_FIELDS, MYF(0));
      res = true;
      goto err;
    }
  }

err:
  /* put view back for PS rexecuting */
  lex->link_first_table_back(view, link_to_local);
  return res;
}

/**
  Perform semantic analysis of query and send a response packet to client.

    This function
    - opens all tables and checks privileges to them.
    - validates semantics of statement columns and SQL functions
      by calling fix_fields.
    - performs global transformations such as semi-join conversion.

  @param thd    current thread

  @retval false success, statement metadata is sent to client
  @retval true  error, error message is set in THD (but not sent)
*/

bool Prepared_statement::prepare_query(THD *thd) {
  DBUG_TRACE;
  assert(m_lex == thd->lex);  // set_n_backup_active_arena() guarantees that
  Query_block *query_block = m_lex->query_block;
  enum enum_sql_command sql_command = m_lex->sql_command;
  int res = 0;
  DBUG_PRINT("enter",
             ("command: %d  param_count: %u", sql_command, m_param_count));

  m_lex->first_lists_tables_same();
  Table_ref *const tables = m_lex->query_tables;

  /* set context for commands which do not use setup_tables */
  query_block->context.resolve_in_table_list_only(
      query_block->get_table_list());

  /*
    For the optimizer trace, this is the symmetric, for statement preparation,
    of what is done at statement execution (in mysql_execute_command()).
  */
  Opt_trace_start ots(thd, tables, sql_command, &m_lex->var_list,
                      thd->query().str, thd->query().length, nullptr,
                      thd->variables.character_set_client);

  Opt_trace_object trace_command(&thd->opt_trace);
  Opt_trace_array trace_command_steps(&thd->opt_trace, "steps");

  if ((m_lex->keep_diagnostics == DA_KEEP_COUNTS) ||
      (m_lex->keep_diagnostics == DA_KEEP_DIAGNOSTICS)) {
    my_error(ER_UNSUPPORTED_PS, MYF(0));
    return true;
  }

  if (sql_command_flags[sql_command] & CF_HA_CLOSE)
    mysql_ha_rm_tables(thd, tables);

  /*
    Open temporary tables that are known now. Temporary tables added by
    prelocking will be opened afterwards (during open_tables()).
  */
  if (sql_command_flags[sql_command] & CF_PREOPEN_TMP_TABLES) {
    if (open_temporary_tables(thd, tables)) return true;
  }

  switch (sql_command) {
    case SQLCOM_CREATE_VIEW:
      if (m_lex->create_view_mode == enum_view_create_mode::VIEW_ALTER) {
        my_error(ER_UNSUPPORTED_PS, MYF(0));
        return true;
      }
      res = mysql_test_create_view(thd, this);
      break;

    case SQLCOM_SET_PASSWORD:
    case SQLCOM_SET_OPTION:
      res = mysql_test_set_fields(thd, this, tables, &m_lex->var_list);
      break;

      /*
        Note that we don't need to have cases in this list if they are
        marked with CF_STATUS_COMMAND in sql_command_flags
      */
    case SQLCOM_DROP_TABLE:
    case SQLCOM_RENAME_TABLE:
    case SQLCOM_ALTER_TABLE:
    case SQLCOM_COMMIT:
    case SQLCOM_CREATE_INDEX:
    case SQLCOM_DROP_INDEX:
    case SQLCOM_ROLLBACK:
    case SQLCOM_TRUNCATE:
    case SQLCOM_DROP_VIEW:
    case SQLCOM_REPAIR:
    case SQLCOM_ANALYZE:
    case SQLCOM_OPTIMIZE:
    case SQLCOM_CHANGE_MASTER:
    case SQLCOM_CHANGE_REPLICATION_FILTER:
    case SQLCOM_RESET:
    case SQLCOM_FLUSH:
    case SQLCOM_SLAVE_START:
    case SQLCOM_SLAVE_STOP:
    case SQLCOM_INSTALL_PLUGIN:
    case SQLCOM_UNINSTALL_PLUGIN:
    case SQLCOM_CREATE_DB:
    case SQLCOM_DROP_DB:
    case SQLCOM_CHECKSUM:
    case SQLCOM_CREATE_USER:
    case SQLCOM_RENAME_USER:
    case SQLCOM_DROP_USER:
    case SQLCOM_ALTER_USER:
    case SQLCOM_ASSIGN_TO_KEYCACHE:
    case SQLCOM_PRELOAD_KEYS:
    case SQLCOM_GRANT:
    case SQLCOM_GRANT_ROLE:
    case SQLCOM_REVOKE:
    case SQLCOM_REVOKE_ALL:
    case SQLCOM_REVOKE_ROLE:
    case SQLCOM_KILL:
    case SQLCOM_ALTER_INSTANCE:
    case SQLCOM_SET_ROLE:
    case SQLCOM_ALTER_USER_DEFAULT_ROLE:
      break;

    case SQLCOM_CREATE_TABLE:
      /*
        CREATE TABLE ... START TRANSACTION is not supported with
        prepared statements
      */
      if (m_lex->create_info->m_transactional_ddl) {
        my_error(ER_UNSUPPORTED_PS, MYF(0));
        return true;
      }
#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(fallthrough)
      [[fallthrough]];
#endif
#endif
    case SQLCOM_SELECT:
    case SQLCOM_DO:
    case SQLCOM_DELETE:
    case SQLCOM_DELETE_MULTI:
    case SQLCOM_UPDATE:
    case SQLCOM_UPDATE_MULTI:
    case SQLCOM_INSERT:
    case SQLCOM_INSERT_SELECT:
    case SQLCOM_REPLACE:
    case SQLCOM_REPLACE_SELECT:
    case SQLCOM_CALL:
    case SQLCOM_SHOW_BINLOG_EVENTS:
    case SQLCOM_SHOW_BINLOGS:
    case SQLCOM_SHOW_CHARSETS:
    case SQLCOM_SHOW_COLLATIONS:
    case SQLCOM_SHOW_CREATE_DB:
    case SQLCOM_SHOW_CREATE_EVENT:
    case SQLCOM_SHOW_CREATE_FUNC:
    case SQLCOM_SHOW_CREATE_PROC:
    case SQLCOM_SHOW_CREATE:
    case SQLCOM_SHOW_CREATE_TRIGGER:
    case SQLCOM_SHOW_CREATE_USER:
    case SQLCOM_SHOW_DATABASES:
    case SQLCOM_SHOW_ENGINE_LOGS:
    case SQLCOM_SHOW_ENGINE_MUTEX:
    case SQLCOM_SHOW_ENGINE_STATUS:
    case SQLCOM_SHOW_ERRORS:
    case SQLCOM_SHOW_EVENTS:
    case SQLCOM_SHOW_FIELDS:
    case SQLCOM_SHOW_FUNC_CODE:
    case SQLCOM_SHOW_GRANTS:
    case SQLCOM_SHOW_KEYS:
    case SQLCOM_SHOW_MASTER_STAT:
    case SQLCOM_SHOW_OPEN_TABLES:
    case SQLCOM_SHOW_PLUGINS:
    case SQLCOM_SHOW_PRIVILEGES:
    case SQLCOM_SHOW_PROC_CODE:
    case SQLCOM_SHOW_PROCESSLIST:
    case SQLCOM_SHOW_PROFILE:
    case SQLCOM_SHOW_PROFILES:
    case SQLCOM_SHOW_RELAYLOG_EVENTS:
    case SQLCOM_SHOW_SLAVE_HOSTS:
    case SQLCOM_SHOW_SLAVE_STAT:
    case SQLCOM_SHOW_STATUS:
    case SQLCOM_SHOW_STATUS_PROC:
    case SQLCOM_SHOW_STATUS_FUNC:
    case SQLCOM_SHOW_STORAGE_ENGINES:
    case SQLCOM_SHOW_TABLE_STATUS:
    case SQLCOM_SHOW_TABLES:
    case SQLCOM_SHOW_TRIGGERS:
    case SQLCOM_SHOW_VARIABLES:
    case SQLCOM_SET_RESOURCE_GROUP:
    case SQLCOM_SHOW_WARNS:
      res = m_lex->m_sql_cmd->prepare(thd);
      break;

    case SQLCOM_PREPARE:
    case SQLCOM_EXECUTE:
    case SQLCOM_DEALLOCATE_PREPARE:
    default:
      /*
        Trivial check of all status commands and diagnostic commands.
        This is easier than having things in the above case list,
        as it's less chance for mistakes.
      */
      if (!(sql_command_flags[sql_command] & CF_STATUS_COMMAND) ||
          (sql_command_flags[sql_command] & CF_DIAGNOSTIC_STMT)) {
        /* All other statements are not supported yet. */
        my_error(ER_UNSUPPORTED_PS, MYF(0));
        return true;
      }
      break;
  }
  if (res) return true;

  trace_parameter_types(thd);

  if (is_sql_prepare()) return false;

  mem_root_deque<Item *> *types = nullptr;
  Query_result *result = nullptr;
  uint no_columns = 0;

  if ((sql_command_flags[sql_command] & CF_HAS_RESULT_SET) &&
      !m_lex->is_explain()) {
    Query_expression *unit = m_lex->unit;
    result = unit->query_result();
    if (result == nullptr) result = unit->first_query_block()->query_result();
    if (result == nullptr) result = m_lex->result;
    types = unit->get_unit_column_types();
    no_columns = result->field_count(*types);
  }

  return send_statement(thd, this, no_columns, result, types);
}

const char *fieldtype2str(enum enum_field_types type);

void Prepared_statement::trace_parameter_types(THD *thd) {
  if (m_param_count == 0) return;
  Opt_trace_object anon(&thd->opt_trace);
  Opt_trace_array typ(&thd->opt_trace, "statement_parameters");
  Item_param **end = m_param_array + m_param_count;
  char buf[50];

  for (Item_param **it = m_param_array; it < end; ++it) {
    enum_field_types t = (*it)->data_type();
    const char *n = fieldtype2str(t);
    switch (t) {
      case MYSQL_TYPE_NEWDECIMAL:
      case MYSQL_TYPE_TIME:
      case MYSQL_TYPE_DATETIME:
        snprintf(buf, sizeof(buf), "%s decimals=%d", n, (*it)->decimals);
        n = buf;
      default:;
    }
    typ.add_alnum(n);
  }
}

/**
  Initialize array of parameters in statement from LEX.
  (We need to have quick access to items by number in mysql_stmt_get_longdata).
  This is to avoid using malloc/realloc in the parser.
*/

static bool init_param_array(THD *thd, Prepared_statement *stmt) {
  LEX *lex = stmt->m_lex;
  if ((stmt->m_param_count = lex->param_list.elements)) {
    if (stmt->m_param_count > static_cast<uint>(UINT_MAX16)) {
      /* Error code to be defined in 5.0 */
      my_error(ER_PS_MANY_PARAM, MYF(0));
      return true;
    }

    Item_param **to;
    List_iterator<Item_param> param_iterator(lex->param_list);
    /* Use thd->mem_root as it points at statement mem_root */
    stmt->m_param_array =
        thd->mem_root->ArrayAlloc<Item_param *>(stmt->m_param_count);
    if (stmt->m_param_array == nullptr) return true;
    for (to = stmt->m_param_array;
         to < stmt->m_param_array + stmt->m_param_count; ++to) {
      *to = param_iterator++;
    }
  }
  return false;
}

/**
  COM_STMT_PREPARE handler.

    Given a query string with parameter markers, create a prepared
    statement from it and send PS info back to the client.

    If parameter markers are found in the query, then store the information
    using Item_param along with maintaining a list in lex->param_array, so
    that a fast and direct retrieval can be made without going through all
    field items.

    In case of success a new statement id and metadata is sent
    to the client, otherwise an error message is set in THD.

  @param thd                thread handle
  @param query              query to be prepared
  @param length             query string length, including ignored
                            trailing NULL or quote char.
  @param stmt               Prepared_statement to be used for preparation.

  @note
    This function parses the query and sends the total number of parameters
    and resultset metadata information back to client (if any), without
    executing the query i.e. without any log/disk writes. This allows the
    queries to be re-executed without re-parsing during execute.
*/

void mysqld_stmt_prepare(THD *thd, const char *query, uint length,
                         Prepared_statement *stmt) {
  DBUG_TRACE;
  DBUG_PRINT("prep_query", ("%s", query));
  assert(stmt != nullptr);

  bool switch_protocol = thd->is_classic_protocol();
  if (switch_protocol) {
    // set the current client capabilities before switching the protocol
    thd->protocol_binary->set_client_capabilities(
        thd->get_protocol()->get_client_capabilities());
    thd->push_protocol(thd->protocol_binary);
  }

  // Initially, optimize the statement for the primary storage engine.
  // If an eligible secondary storage engine is found, the statement
  // may be reprepared for the secondary storage engine later.
  const auto saved_secondary_engine = thd->secondary_engine_optimization();
  thd->set_secondary_engine_optimization(
      Secondary_engine_optimization::PRIMARY_TENTATIVELY);
  /* Create PS table entry, set query text after rewrite. */
  stmt->m_prepared_stmt =
      MYSQL_CREATE_PS(stmt, stmt->m_id, thd->m_statement_psi, stmt->name().str,
                      stmt->name().length, nullptr, 0);

  if (stmt->prepare(thd, query, length, nullptr)) {
    /* Delete this stmt stats from PS table. */
    MYSQL_DESTROY_PS(stmt->m_prepared_stmt);
    /* Statement map deletes statement on erase */
    thd->stmt_map.erase(stmt);
  }

  thd->set_secondary_engine_optimization(saved_secondary_engine);
  if (switch_protocol) thd->pop_protocol();

  sp_cache_enforce_limit(thd->sp_proc_cache, stored_program_cache_size);
  sp_cache_enforce_limit(thd->sp_func_cache, stored_program_cache_size);

  // Prepared_statement::prepare_query() sends metadata packet if success
}

/**
  Searches for the statement with the specified id and validates it.

  @param thd [in]           thread handle
  @param com_data [in]      command data
  @param cmd [in]           command type to be executed
  @param stmt [out]         pointer to Prepared_statement to store it if found
*/

bool mysql_stmt_precheck(THD *thd, const COM_DATA *com_data,
                         enum enum_server_command cmd,
                         Prepared_statement **stmt) {
  *stmt = nullptr;
  ulong stmt_id = 0;

  switch (cmd) {
    case COM_STMT_FETCH:
      stmt_id = com_data->com_stmt_fetch.stmt_id;
      if (!(*stmt = thd->stmt_map.find(stmt_id))) goto not_found;
      break;
    case COM_STMT_CLOSE:
      stmt_id = com_data->com_stmt_close.stmt_id;
      if (!(*stmt = thd->stmt_map.find(stmt_id))) goto silent_error;
      break;
    case COM_STMT_RESET: {
      stmt_id = com_data->com_stmt_reset.stmt_id;
      if (!(*stmt = thd->stmt_map.find(stmt_id))) goto not_found;
      break;
    }
    case COM_STMT_PREPARE: {
      if (!(*stmt = new Prepared_statement(thd)))
        // out of memory: error is set in MEM_ROOT
        goto silent_error; /* purecov: inspected */

      if (thd->stmt_map.insert(*stmt))
        /*
          The error is set in the insert. The statement itself
          will be also deleted there (this is how the hash works).
        */
        goto silent_error;
      break;
    }
    case COM_STMT_SEND_LONG_DATA: {
      stmt_id = com_data->com_stmt_send_long_data.stmt_id;
      if (!(*stmt = thd->stmt_map.find(stmt_id))) goto silent_error;
      if (com_data->com_stmt_send_long_data.param_number >=
          (*stmt)->m_param_count) {
        /* Error will be sent in execute call */
        (*stmt)->m_arena.set_state(Query_arena::STMT_ERROR);
        (*stmt)->m_last_errno = ER_WRONG_ARGUMENTS;
        sprintf((*stmt)->m_last_error, ER_THD(thd, ER_WRONG_ARGUMENTS),
                "mysql_stmt_precheck");
        goto silent_error;
      }
      break;
    }
    case COM_STMT_EXECUTE: {
      stmt_id = com_data->com_stmt_execute.stmt_id;
      if (!(*stmt = thd->stmt_map.find(stmt_id))) goto not_found;
      if (thd->get_protocol()->has_client_capability(CLIENT_QUERY_ATTRIBUTES)) {
        /*
          For client supporting query attributes it's perfectly fine to send
          more parameter values than the prepared statement has. The prepared
          statement will take the first param_count values and will leave the
          rest for the component service to consume. Thus altering the validity
          check accordingly to just error out if there's less than the expected
          number of parameters and pass if there's more.
        */
        if ((*stmt)->m_param_count > com_data->com_stmt_execute.parameter_count)
          goto wrong_arg;
      } else {
        if ((*stmt)->m_param_count !=
            com_data->com_stmt_execute.parameter_count)
          goto wrong_arg;
      }
      break;
    }
    default:
      assert(0);
      return true;
  }
  return false;

not_found:
  char llbuf[22];
  my_error(ER_UNKNOWN_STMT_HANDLER, MYF(0), static_cast<int>(sizeof(llbuf)),
           llstr(stmt_id, llbuf), "mysql_stmt_precheck");
  return true;

wrong_arg:
  my_error(ER_WRONG_ARGUMENTS, MYF(0), "COM_STMT_EXECUTE");
  return true;

silent_error:
  return true;
}

/**
  Get an SQL statement text from a user variable or from plain text.

  If the statement is plain text, just assign the
  pointers, otherwise allocate memory in thd->mem_root and copy
  the contents of the variable, possibly with character
  set conversion.

  @param[in]  lex               main lex
  @param[out] query_len         length of the SQL statement (is set only
    in case of success)

  @retval
    non-zero  success
  @retval
    0         in case of error (out of memory)
*/

static const char *get_dynamic_sql_string(LEX *lex, size_t *query_len) {
  THD *thd = lex->thd;
  char *query_str = nullptr;

  if (lex->prepared_stmt_code_is_varref) {
    /* This is PREPARE stmt FROM or EXECUTE IMMEDIATE @var. */
    String str;
    const CHARSET_INFO *to_cs = thd->variables.collation_connection;
    bool needs_conversion;
    String *var_value = &str;
    size_t unused;
    size_t len;

    /* Protects thd->user_vars */
    mysql_mutex_lock(&thd->LOCK_thd_data);

    const auto it = thd->user_vars.find(to_string(lex->prepared_stmt_code));

    /*
      Convert @var contents to string in connection character set. Although
      it is known that int/real/NULL value cannot be a valid query we still
      convert it for error messages to be uniform.
    */
    if (it != thd->user_vars.end() && it->second->ptr()) {
      user_var_entry *entry = it->second.get();
      bool is_var_null;
      var_value = entry->val_str(&is_var_null, &str, DECIMAL_NOT_SPECIFIED);

      mysql_mutex_unlock(&thd->LOCK_thd_data);

      /*
        NULL value of variable checked early as entry->value so here
        we can't get NULL in normal conditions
      */
      assert(!is_var_null);
      if (!var_value) goto end;
    } else {
      mysql_mutex_unlock(&thd->LOCK_thd_data);

      /*
        variable absent or equal to NULL, so we need to set variable to
        something reasonable to get a readable error message during parsing
      */
      str.set(STRING_WITH_LEN("NULL"), &my_charset_latin1);
    }

    needs_conversion = String::needs_conversion(
        var_value->length(), var_value->charset(), to_cs, &unused);

    len = (needs_conversion ? var_value->length() * to_cs->mbmaxlen
                            : var_value->length());
    if (!(query_str = (char *)thd->mem_root->Alloc(len + 1))) goto end;

    if (needs_conversion) {
      uint dummy_errors;
      len = copy_and_convert(query_str, len, to_cs, var_value->ptr(),
                             var_value->length(), var_value->charset(),
                             &dummy_errors);
    } else
      memcpy(query_str, var_value->ptr(), var_value->length());
    query_str[len] = '\0';  // Safety (mostly for debug)
    *query_len = len;
  } else {
    query_str = lex->prepared_stmt_code.str;
    *query_len = lex->prepared_stmt_code.length;
  }
end:
  return query_str;
}

/**
  SQLCOM_PREPARE implementation.

    Prepare an SQL prepared statement. This is called from
    mysql_execute_command and should therefore behave like an
    ordinary query (e.g. should not reset any global THD data).

    In case of success, OK packet is sent to the client,
    otherwise an error message is set in THD.

  @param thd     thread handle
*/

void mysql_sql_stmt_prepare(THD *thd) {
  LEX *lex = thd->lex;
  const LEX_CSTRING &name = lex->prepared_stmt_name;
  DBUG_TRACE;

  Prepared_statement *stmt = thd->stmt_map.find_by_name(name);
  if (stmt != nullptr) {
    /*
      If there is a statement with the same name, remove it. It is ok to
      remove old and fail to insert a new one at the same time.
    */
    if (stmt->is_in_use()) {
      my_error(ER_PS_NO_RECURSION, MYF(0));
      return;
    }

    MYSQL_DESTROY_PS(stmt->m_prepared_stmt);
    stmt->deallocate(thd);
  }

  size_t query_len = 0;
  const char *query = get_dynamic_sql_string(lex, &query_len);
  if (query == nullptr) return; /* out of memory */

  stmt = new Prepared_statement(thd);
  if (stmt == nullptr) return; /* out of memory */

  stmt->set_sql_prepare();

  /* Set the name first, insert should know that this statement has a name */
  if (stmt->set_name(name)) {
    delete stmt;
    return;
  }

  if (thd->stmt_map.insert(stmt)) {
    /* The statement is deleted and an error is set if insert fails */
    return;
  }

  /* Create PS table entry, set query text after rewrite. */
  stmt->m_prepared_stmt =
      MYSQL_CREATE_PS(stmt, stmt->id(), thd->m_statement_psi, stmt->name().str,
                      stmt->name().length, nullptr, 0);

  if (stmt->prepare(thd, query, query_len, nullptr)) {
    /* Delete this stmt stats from PS table. */
    MYSQL_DESTROY_PS(stmt->m_prepared_stmt);
    /* Statement map deletes the statement on erase */
    thd->stmt_map.erase(stmt);
  } else {
    /* send the boolean tracker in the OK packet when
       @@session_track_state_change is set to ON */
    if (thd->session_tracker.get_tracker(SESSION_STATE_CHANGE_TRACKER)
            ->is_enabled())
      thd->session_tracker.get_tracker(SESSION_STATE_CHANGE_TRACKER)
          ->mark_as_changed(thd, {});
    my_ok(thd, 0L, 0L, "Statement prepared");
  }
}

/**
  Clears parameters from data left from previous execution or long data.

  @param stmt               prepared statement for which parameters should
                            be reset
*/
static void reset_stmt_parameters(Prepared_statement *stmt) {
  Item_param **item = stmt->m_param_array;
  Item_param **end = item + stmt->m_param_count;
  for (; item < end; ++item) {
    (**item).reset();
    (**item).sync_clones();
  }
}

/**
  COM_STMT_EXECUTE handler: execute a previously prepared statement.

    If there are any parameters, then replace parameter markers with the
    data supplied from the client, and then execute the statement.
    This function uses binary protocol to send a possible result set
    to the client.

    In case of success OK packet or a result set is sent to the
    client, otherwise an error message is set in THD.

  @param thd            current thread
  @param stmt           prepared statement
  @param has_new_types  true if parsed parameters have data types defined,
                        otherwise types from last execution will be used
  @param execute_flags  flag used to decide if a cursor should be used
  @param parameters     prepared statement's parsed parameters
*/

void mysqld_stmt_execute(THD *thd, Prepared_statement *stmt, bool has_new_types,
                         ulong execute_flags, PS_PARAM *parameters) {
  DBUG_TRACE;
  statement_id_to_session(thd);
#if defined(ENABLED_PROFILING)
  thd->profiling->set_query_source(stmt->m_query_string.str,
                                   stmt->m_query_string.length);
#endif
  DBUG_PRINT("info", ("stmt: %p", stmt));

  bool switch_protocol = thd->is_classic_protocol();
  if (switch_protocol) {
    // set the current client capabilities before switching the protocol
    thd->protocol_binary->set_client_capabilities(
        thd->get_protocol()->get_client_capabilities());
    thd->push_protocol(thd->protocol_binary);
  }

  MYSQL_EXECUTE_PS(thd->m_statement_psi, stmt->m_prepared_stmt);

  // Initially, optimize the statement for the primary storage engine.
  // If an eligible secondary storage engine is found, the statement
  // may be reprepared for the secondary storage engine later.
  const auto saved_secondary_engine = thd->secondary_engine_optimization();
  thd->set_secondary_engine_optimization(
      Secondary_engine_optimization::PRIMARY_TENTATIVELY);

  MYSQL_SET_PS_SECONDARY_ENGINE(stmt->m_prepared_stmt, false);

  // Query text for binary, general or slow log, if any of them is open
  String expanded_query;
  expanded_query.set_charset(default_charset_info);
  // If no error happened while setting the parameters, execute statement.
  if (!stmt->set_parameters(thd, &expanded_query, has_new_types, parameters)) {
    bool open_cursor = execute_flags & (ulong)CURSOR_TYPE_READ_ONLY;
    stmt->execute_loop(thd, &expanded_query, open_cursor);
  }

  thd->set_secondary_engine_optimization(saved_secondary_engine);

  if (switch_protocol) thd->pop_protocol();

  sp_cache_enforce_limit(thd->sp_proc_cache, stored_program_cache_size);
  sp_cache_enforce_limit(thd->sp_func_cache, stored_program_cache_size);

  /* Close connection socket; for use with client testing (Bug#43560). */
  DBUG_EXECUTE_IF("close_conn_after_stmt_execute",
                  thd->get_protocol()->shutdown(););
}

/**
  SQLCOM_EXECUTE implementation.

    Execute prepared statement using parameter values from
    lex->prepared_stmt_params and send result to the client using
    text protocol. This is called from mysql_execute_command and
    therefore should behave like an ordinary query (e.g. not change
    global THD data, such as warning count, server status, etc).
    This function uses text protocol to send a possible result set.

    In case of success, OK (or result set) packet is sent to the
    client, otherwise an error is set in THD.

  @param thd                thread handle
*/

void mysql_sql_stmt_execute(THD *thd) {
  LEX *lex = thd->lex;
  const LEX_CSTRING &name = lex->prepared_stmt_name;
  DBUG_TRACE;
  DBUG_PRINT("info", ("EXECUTE: %.*s\n", (int)name.length, name.str));

  Prepared_statement *stmt = thd->stmt_map.find_by_name(name);
  if (stmt == nullptr) {
    my_error(ER_UNKNOWN_STMT_HANDLER, MYF(0), static_cast<int>(name.length),
             name.str, "EXECUTE");
    return;
  }

  if (stmt->m_param_count != lex->prepared_stmt_params.elements) {
    my_error(ER_WRONG_ARGUMENTS, MYF(0), "EXECUTE");
    return;
  }

  DBUG_PRINT("info", ("stmt: %p", stmt));
  MYSQL_EXECUTE_PS(thd->m_statement_psi, stmt->m_prepared_stmt);

  // Query text for binary, general or slow log, if any of them is open
  String expanded_query;
  if (stmt->set_parameters(thd, &expanded_query)) return;

  stmt->execute_loop(thd, &expanded_query, false);
}

/**
  COM_STMT_FETCH handler: fetches requested amount of rows from cursor.

  @param thd                Thread handle.
  @param stmt               Pointer to the prepared statement.
  @param num_rows           Number of rows to fetch.
*/

void mysqld_stmt_fetch(THD *thd, Prepared_statement *stmt, ulong num_rows) {
  DBUG_TRACE;
  thd->status_var.com_stmt_fetch++;

  Server_side_cursor *cursor = stmt->m_cursor;
  if (cursor == nullptr || !cursor->is_open()) {
    my_error(ER_STMT_HAS_NO_OPEN_CURSOR, MYF(0), stmt->m_id);
    return;
  }

  thd->stmt_arena = &stmt->m_arena;
  Statement_backup stmt_backup;
  stmt_backup.set_thd_to_ps(thd, stmt);

  cursor->fetch(num_rows);

  if (!cursor->is_open()) reset_stmt_parameters(stmt);

  stmt_backup.restore_thd(thd, stmt);
  thd->stmt_arena = thd;
}

/**
  Reset a prepared statement in case there was a recoverable error.

    This function resets statement to the state it was right after prepare.
    It can be used to:
    - clear an error happened during mysqld_stmt_send_long_data
    - cancel long data stream for all placeholders without
      having to call mysqld_stmt_execute.
    - close an open cursor
    Sends 'OK' packet in case of success (statement was reset)
    or 'ERROR' packet (unrecoverable error/statement not found/etc).

  @param thd                Thread handle
  @param stmt               Pointer to the Prepared_statement
*/

void mysqld_stmt_reset(THD *thd, Prepared_statement *stmt) {
  DBUG_TRACE;

  thd->status_var.com_stmt_reset++;
  stmt->close_cursor();

  /*
    Clear parameters from data which could be set by
    mysqld_stmt_send_long_data() call.
  */
  reset_stmt_parameters(stmt);

  stmt->m_arena.set_state(Query_arena::STMT_PREPARED);

  query_logger.general_log_print(thd, thd->get_command(), NullS);

  my_ok(thd);
}

/**
  Delete a prepared statement from memory.

  @note
    we don't send any reply to this command.
*/

void mysqld_stmt_close(THD *thd, Prepared_statement *stmt) {
  DBUG_TRACE;
  /*
    The only way currently a statement can be deallocated when it's
    in use is from within Dynamic SQL.
  */
  assert(!stmt->is_in_use());
  MYSQL_DESTROY_PS(stmt->m_prepared_stmt);
  stmt->deallocate(thd);
  query_logger.general_log_print(thd, thd->get_command(), NullS);
}

/**
  SQLCOM_DEALLOCATE implementation.

    Close an SQL prepared statement. As this can be called from Dynamic
    SQL, we should be careful to not close a statement that is currently
    being executed.

    OK packet is sent in case of success, otherwise an error
    message is set in THD.
*/

void mysql_sql_stmt_close(THD *thd) {
  const LEX_CSTRING &name = thd->lex->prepared_stmt_name;
  DBUG_PRINT("info",
             ("DEALLOCATE PREPARE: %.*s\n", (int)name.length, name.str));

  Prepared_statement *stmt = thd->stmt_map.find_by_name(name);
  if (stmt == nullptr) {
    my_error(ER_UNKNOWN_STMT_HANDLER, MYF(0), static_cast<int>(name.length),
             name.str, "DEALLOCATE PREPARE");
    return;
  }
  if (stmt->is_in_use()) {
    my_error(ER_PS_NO_RECURSION, MYF(0));
    return;
  }

  MYSQL_DESTROY_PS(stmt->m_prepared_stmt);
  stmt->deallocate(thd);
  if (thd->session_tracker.get_tracker(SESSION_STATE_CHANGE_TRACKER)
          ->is_enabled())
    thd->session_tracker.get_tracker(SESSION_STATE_CHANGE_TRACKER)
        ->mark_as_changed(thd, {});
  my_ok(thd);
}

/**
  Handle long data in pieces from client.

    Get a part of a long data. To make the protocol efficient, we are
    not sending any return packets here. If something goes wrong, then
    we will send the error on 'execute' We assume that the client takes
    care of checking that all parts are sent to the server. (No checking
    that we get a 'end of column' in the server is performed).

  @param thd                Thread handle
  @param stmt               Pointer to Prepared_statement
  @param param_number       Number of parameters
  @param str                String to append
  @param length             Length of string (including end \\0)
*/

void mysql_stmt_get_longdata(THD *thd, Prepared_statement *stmt,
                             uint param_number, uchar *str, ulong length) {
  DBUG_TRACE;

  thd->status_var.com_stmt_send_long_data++;
  Diagnostics_area new_stmt_da(false);
  thd->push_diagnostics_area(&new_stmt_da);

  Item_param *param = stmt->m_param_array[param_number];
  param->set_longdata((char *)str, length);
  if (thd->get_stmt_da()->is_error()) {
    stmt->m_arena.set_state(Query_arena::STMT_ERROR);
    stmt->m_last_errno = thd->get_stmt_da()->mysql_errno();
    snprintf(stmt->m_last_error, sizeof(stmt->m_last_error), "%.*s",
             MYSQL_ERRMSG_SIZE - 1, thd->get_stmt_da()->message_text());
  }
  thd->pop_diagnostics_area();

  query_logger.general_log_print(thd, thd->get_command(), NullS);
}

/***************************************************************************
 Query_fetch_protocol_binary
****************************************************************************/

namespace {

bool Query_fetch_protocol_binary::send_result_set_metadata(
    THD *thd, const mem_root_deque<Item *> &list, uint flags) {
  bool rc;

  protocol.set_client_capabilities(
      thd->get_protocol()->get_client_capabilities());
  /*
    Protocol::send_result_set_metadata caches the information about column
    types: this information is later used to send data. Therefore, the same
    dedicated Protocol object must be used for all operations with
    a cursor.
  */
  thd->push_protocol(&protocol);
  rc = Query_result_send::send_result_set_metadata(thd, list, flags);
  thd->pop_protocol();

  return rc;
}

bool Query_fetch_protocol_binary::send_eof(THD *thd) {
  /*
    Don't send EOF if we're in error condition (which implies we've already
    sent or are sending an error)
  */
  if (thd->is_error()) return true;

  ::my_eof(thd);
  return false;
}

bool Query_fetch_protocol_binary::send_data(
    THD *thd, const mem_root_deque<Item *> &fields) {
  bool rc;

  // set the current client capabilities before switching the protocol
  protocol.set_client_capabilities(
      thd->get_protocol()->get_client_capabilities());
  thd->push_protocol(&protocol);
  rc = Query_result_send::send_data(thd, fields);
  thd->pop_protocol();

  return rc;
}

}  // namespace

/*******************************************************************
 * Reprepare_observer
 *******************************************************************/
/** Push an error to the error stack and return true for now. */

bool Reprepare_observer::report_error(THD *thd) {
  /*
    This 'error' is purely internal to the server:
    - No exception handler is invoked,
    - No condition is added in the condition area (warn_list).
    The Diagnostics Area is set to an error status to enforce
    that this thread execution stops and returns to the caller,
    backtracking all the way to Prepared_statement::execute_loop().

    As the DA has not yet been reset at this point, we'll need to
    reset the previous statement's result status first.
    Test with rpl_sp_effects and friends.
  */
  thd->get_stmt_da()->reset_diagnostics_area();
  thd->get_stmt_da()->set_error_status(thd, ER_NEED_REPREPARE);
  m_invalidated = true;
  m_attempt++;

  return true;
}

/**
  Requests for repreparation of statement.
  @returns true if request has been placed.
*/
bool ask_to_reprepare(THD *thd) {
  Reprepare_observer *reprepare_observer = thd->get_reprepare_observer();
  if (reprepare_observer != nullptr && reprepare_observer->report_error(thd)) {
    assert(thd->is_error());
    return true;
  }
  return false;
}

/*******************************************************************
 * Server_runnable
 *******************************************************************/

Server_runnable::~Server_runnable() = default;

///////////////////////////////////////////////////////////////////////////

namespace {

Execute_sql_statement::Execute_sql_statement(LEX_STRING sql_text)
    : m_sql_text(sql_text) {}

/**
  Parse and execute a statement. Does not prepare the query.

  Allows to execute a statement from within another statement.
  The main property of the implementation is that it does not
  affect the environment -- i.e. you  can run many
  executions without having to cleanup/reset THD in between.
*/

bool Execute_sql_statement::execute_server_code(THD *thd) {
  sql_digest_state *parent_digest;
  PSI_statement_locker *parent_locker;
  bool error;

  if (alloc_query(thd, m_sql_text.str, m_sql_text.length)) return true;

  Parser_state parser_state;
  if (parser_state.init(thd, thd->query().str, thd->query().length))
    return true;

  parser_state.m_lip.multi_statements = false;
  lex_start(thd);

  parent_digest = thd->m_digest;
  parent_locker = thd->m_statement_psi;
  thd->m_digest = nullptr;
  thd->m_statement_psi = nullptr;
  error = parse_sql(thd, &parser_state, nullptr) || thd->is_error();
  thd->m_digest = parent_digest;
  thd->m_statement_psi = parent_locker;

  if (error) goto end;

  thd->lex->set_trg_event_type_for_tables();

  parent_locker = thd->m_statement_psi;
  thd->m_statement_psi = nullptr;

  /*
    Rewrite first (if needed); execution might replace passwords
    with hashes in situ without flagging it, and then we'd make
    a hash of that hash.
  */
  rewrite_query_if_needed(thd);
  log_execute_line(thd);

  error = mysql_execute_command(thd);
  thd->m_statement_psi = parent_locker;

end:
  lex_end(thd->lex);

  return error;
}

}  // namespace

/***************************************************************************
 Prepared_statement
****************************************************************************/

Prepared_statement::Prepared_statement(THD *thd_arg)
    : m_arena(&m_mem_root, Query_arena::STMT_INITIALIZED),
      m_id(++thd_arg->statement_id_counter),
      m_mem_root(key_memory_prepared_statement_main_mem_root,
                 thd_arg->variables.query_alloc_block_size) {
  *m_last_error = '\0';
}

void Prepared_statement::close_cursor() {
  if (m_cursor == nullptr) return;
  m_cursor->close();
  m_used_as_cursor = false;
}

void Prepared_statement::setup_stmt_logging(THD *thd) {
  DBUG_EXECUTE_IF("bug16617026_simulate_audit_log_ps",
                  { m_lex->safe_to_cache_query = 0; });
  /*
    Decide if we have to expand the statement (because we must write it to logs)
    or not.
    We don't have to substitute the params when bin-logging DML in RBL.
  */
  if ((mysql_bin_log.is_open() && is_update_query(m_lex->sql_command) &&
       (!thd->is_current_stmt_binlog_format_row() ||
        ((sql_command_flags[m_lex->sql_command] & CF_AUTO_COMMIT_TRANS) ==
         CF_AUTO_COMMIT_TRANS))) ||
      opt_general_log || opt_slow_log ||
      (m_lex->sql_command == SQLCOM_SELECT && m_lex->safe_to_cache_query &&
       !m_lex->is_explain()) ||
      is_global_audit_mask_set()) {
    m_with_log = true;
  }
  /*
    @todo WL#6570
    In the if() above there is is_global_audit_mask_set()) .
    Because caching_sha2 is loaded by default, it this function return true
    which makes with_log always true.
    If not desirable, we can limit the test to only these plugin classes:
    MYSQL_AUDIT_GENERAL_CLASS, MYSQL_AUDIT_PARSE_CLASS,
    MYSQL_AUDIT_TABLE_ACCESS_CLASS, MYSQL_AUDIT_QUERY_CLASS,
    MYSQL_AUDIT_STORED_PROGRAM_CLASS, MYSQL_AUDIT_AUTHENTICATION_CLASS.
    E.g. the plugin which mandated this change (bug#16617026) was about
    Enterprise audit log plugin which is of class MYSQL_AUDIT_GENERAL_CLASS.
    Marek said.
    Another related issue: in SQL PREPARE/EXECUTE
    Item_param::set_type_actual() is only called in the "if (m_with_log)" branch
    of insert_parameters_from_vars(); so, if m_with_log is false, it is
    skipped. Testcase:
    - comment-out the condition is_global_audit_mask_set() above, recompile
    - start with --disable-log-in, no general log, no slow log
    - with that and RAND() below (=>safe_to_cache_query=false),
    m_with_log is false.
    - prepare s from "select ?, rand()";
      set @a:="ab";
      execute s using @a; execution fails.
  */
}

/**
  Destroy this prepared statement, cleaning up all used memory
  and resources.

  This is called from @c deallocate() to handle COM_STMT_CLOSE and
  DEALLOCATE PREPARE or when THD ends and all prepared statements are freed.
*/

Prepared_statement::~Prepared_statement() {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("stmt: %p  cursor: %p", this, m_cursor));
  if (m_used_as_cursor) {
    close_cursor();
  }
  if (m_regular_result != nullptr) destroy(m_regular_result);
  if (m_cursor_result != nullptr) destroy(m_cursor_result);
  if (m_aux_result != nullptr) destroy(m_aux_result);
  m_cursor = nullptr;
  m_cursor_result = nullptr;
  m_regular_result = nullptr;
  m_aux_result = nullptr;
  /*
    We have to call free on the items even if cleanup is called as some items,
    like Item_param, don't free everything until free_items()
  */
  m_arena.free_items();
  if (m_lex != nullptr) {
    assert(m_lex->sphead == nullptr);
    lex_end(m_lex);
    m_lex->destroy();
    delete pointer_cast<st_lex_local *>(m_lex);  // TRASH memory
  }
}

void Prepared_statement::cleanup_stmt(THD *thd) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("stmt: %p", this));

  cleanup_items(m_arena.item_list());
  thd->cleanup_after_query();
}

bool Prepared_statement::set_name(const LEX_CSTRING &name_arg) {
  m_name.length = name_arg.length;
  m_name.str = static_cast<char *>(
      memdup_root(m_arena.mem_root, name_arg.str, name_arg.length));
  return m_name.str == nullptr;
}

/**
  Remember the current database.

  We must reset/restore the current database during execution of
  a prepared statement since it affects execution environment:
  privileges, @@character_set_database, and other.

  @return Returns an error if out of memory.
*/

bool Prepared_statement::set_db(const LEX_CSTRING &db_arg) {
  /* Remember the current database. */
  if (db_arg.str && db_arg.length) {
    m_db.str = m_arena.strmake(db_arg.str, db_arg.length);
    m_db.length = db_arg.length;
  } else {
    m_db = NULL_CSTR;
  }
  return db_arg.str != nullptr && m_db.str == nullptr;
}

/**************************************************************************
  Common parts of mysql_[sql]_stmt_prepare, mysql_[sql]_stmt_execute.
  Essentially, these functions do all the magic of preparing/executing
  a statement, leaving network communication, input data handling and
  global THD state management to the caller.
***************************************************************************/

/**
  Parse statement text, validate the statement, and prepare it for execution.

    You should not change global THD state in this function, if at all
    possible: it may be called from any context, e.g. when executing
    a COM_* command, and SQLCOM_* command, or a stored procedure.

  @param thd              thread handle
  @param query_str        Statement text
  @param query_length     Length of statement string
  @param orig_param_array Array containing pointers to parameter items
                          that contain data type information for query.
                          This is used during reprepare.
                          = NULL: Derive parameter metadata from query only.

  @note
    Precondition:
    The caller must ensure that thd->change_list and thd->item_list
    is empty: this function will not back them up but will free
    in the end of its execution.

  @note
    Postcondition:
    thd->mem_root contains unused memory allocated during validation.
*/

bool Prepared_statement::prepare(THD *thd, const char *query_str,
                                 size_t query_length,
                                 Item_param **orig_param_array) {
  bool error;
  Query_arena arena_backup;
  Query_arena *old_stmt_arena;
  sql_digest_state *parent_digest = thd->m_digest;
  PSI_statement_locker *parent_locker = thd->m_statement_psi;
  unsigned char *token_array = nullptr;

  DBUG_TRACE;
  /*
    If this is an SQLCOM_PREPARE, we also increase Com_prepare_sql.
    However, it seems handy if com_stmt_prepare is increased always,
    no matter what kind of prepare is processed.
  */
  thd->status_var.com_stmt_prepare++;

  assert(m_lex == nullptr);

  m_lex = new (m_arena.mem_root) st_lex_local;
  if (m_lex == nullptr) return true;

  if (set_db(thd->db())) return true;

  /*
    alloc_query() uses thd->memroot && thd->query, so we should call
    both of backup_statement() and backup_query_arena() here.
  */
  Statement_backup stmt_backup;
  stmt_backup.set_thd_to_ps(thd, this);
  stmt_backup.save_rlb(thd);
  thd->swap_query_arena(m_arena, &arena_backup);

  if (alloc_query(thd, query_str, query_length)) {
    stmt_backup.restore_thd(thd, this);
    stmt_backup.restore_rlb(thd);
    thd->swap_query_arena(arena_backup, &m_arena);
    return true;
  }

  if (max_digest_length > 0) {
    token_array = (unsigned char *)thd->alloc(max_digest_length);
  }

  old_stmt_arena = thd->stmt_arena;
  thd->stmt_arena = &m_arena;

  Parser_state parser_state;
  if (parser_state.init(thd, thd->query().str, thd->query().length)) {
    stmt_backup.restore_thd(thd, this);
    stmt_backup.restore_rlb(thd);
    thd->swap_query_arena(arena_backup, &m_arena);
    thd->stmt_arena = old_stmt_arena;
    return true;
  }

  parser_state.m_lip.stmt_prepare_mode = true;
  parser_state.m_lip.multi_statements = false;

  lex_start(thd);
  m_lex->context_analysis_only |= CONTEXT_ANALYSIS_ONLY_PREPARE;

  thd->m_digest = nullptr;
  thd->m_statement_psi = nullptr;

  sql_digest_state digest;
  digest.reset(token_array, max_digest_length);
  thd->m_digest = &digest;

  parser_state.m_input.m_has_digest = true;

  // we produce digest if it's not explicitly turned off
  // by setting maximum digest length to zero
  if (get_max_digest_length() != 0)
    parser_state.m_input.m_compute_digest = true;

  thd->m_parser_state = &parser_state;
  invoke_pre_parse_rewrite_plugins(thd);
  thd->m_parser_state = nullptr;

  error = thd->is_error();

  if (!error) {
    error = parse_sql(thd, &parser_state, nullptr);
  }
  error |= thd->is_error();
  if (!error) {  // We've just created the statement maybe there is a rewrite
    invoke_post_parse_rewrite_plugins(thd, true);
    error = init_param_array(thd, this);
  }
  error |= thd->is_error();

  // Bind Sql command object with this prepared statement
  if (m_lex->m_sql_cmd != nullptr) m_lex->m_sql_cmd->set_owner(this);

  m_lex->set_trg_event_type_for_tables();

  /*
    Pre-clear the diagnostics area unless a warning was thrown
    during parsing.
  */
  if (thd->lex->keep_diagnostics != DA_KEEP_PARSE_ERROR)
    thd->get_stmt_da()->reset_condition_info(thd);

  /*
    While doing context analysis of the query (in prepare_query())
    we allocate a lot of additional memory: for open tables, JOINs, derived
    tables, etc.  Let's save a snapshot of current parse tree to the
    statement and restore original THD. In cases when some tree
    transformation can be reused on execute, we set again thd->mem_root from
    stmt->mem_root (see setup_wild for one place where we do that).
  */
  thd->swap_query_arena(arena_backup, &m_arena);

  /*
    If called from a stored procedure, ensure that we won't rollback
    external changes when cleaning up after validation.
  */
  assert(thd->change_list.is_empty());

  /*
    Marker used to release metadata locks acquired while the prepared
    statement is being checked.
  */
  MDL_savepoint mdl_savepoint = thd->mdl_context.mdl_savepoint();

  // A subsystem, such as the Audit plugin, may have set error unnoticed:
  error |= thd->is_error();

  if (orig_param_array != nullptr) copy_parameter_types(orig_param_array);
  /*
   The only case where we should have items in the thd->item_list is
   after stmt->set_params_from_vars(), which may in some cases create
   Item_null objects.
  */

  if (error == 0) {
    // Update system variables specified in SET_VAR hints while preparing
    if (m_lex->opt_hints_global && m_lex->opt_hints_global->sys_var_hint)
      m_lex->opt_hints_global->sys_var_hint->update_vars(thd);

    error = prepare_query(thd);

    if (m_lex->opt_hints_global && m_lex->opt_hints_global->sys_var_hint)
      m_lex->opt_hints_global->sys_var_hint->restore_vars(thd);
  }
  assert(error || !thd->is_error());

  /*
    Currently CREATE PROCEDURE/TRIGGER/EVENT are prohibited in prepared
    statements: ensure we have no memory leak here if by someone tries
    to PREPARE stmt FROM "CREATE PROCEDURE ..."
  */
  assert(m_lex->sphead == nullptr || error != 0);
  /* The order is important */
  m_lex->cleanup(true);

  m_lex->clear_values_map();

  close_thread_tables(thd);
  thd->mdl_context.rollback_to_savepoint(mdl_savepoint);

  /*
    Transaction rollback was requested since MDL deadlock was discovered
    while trying to open tables. Rollback transaction in all storage
    engines including binary log and release all locks.

    Once dynamic SQL is allowed as substatements the below if-statement
    has to be adjusted to not do rollback in substatement.
  */
  assert(!thd->in_sub_stmt);
  if (thd->transaction_rollback_request) {
    trans_rollback_implicit(thd);
    thd->mdl_context.release_transactional_locks();
  }

  lex_end(m_lex);

  rewrite_query_if_needed(thd);

  if (thd->rewritten_query().length()) {
    thd->set_query_for_display(thd->rewritten_query().ptr(),
                               thd->rewritten_query().length());
    MYSQL_SET_PS_TEXT(m_prepared_stmt, thd->rewritten_query().ptr(),
                      thd->rewritten_query().length());
  } else {
    thd->set_query_for_display(thd->query().str, thd->query().length);
    MYSQL_SET_PS_TEXT(m_prepared_stmt, thd->query().str, thd->query().length);
  }

  cleanup_stmt(thd);
  stmt_backup.restore_thd(thd, this);
  thd->stmt_arena = old_stmt_arena;

  if (error == 0) {
    setup_stmt_logging(thd);
    m_lex->context_analysis_only &= ~CONTEXT_ANALYSIS_ONLY_PREPARE;
    m_arena.set_state(Query_arena::STMT_PREPARED);
    m_in_use = false;

    /*
      Log COM_STMT_PREPARE to the general log. Note, that in case of SQL
      prepared statements this causes two records to be output:

      Query       PREPARE stmt from @user_variable
      Prepare     <statement SQL text>

      This is considered user-friendly, since in the  second log Entry
      we output the actual statement text rather than the variable name.

      Rewriting/password obfuscation:

      - If we're preparing from a string literal rather than from a
        variable, the literal is elided in the "Query" log line, as
        it may contain a password.  (As we've parsed the PREPARE statement,
        but not the statement to prepare yet, we don't know at that point.)
        Eliding the literal is fine, as we'll print it in the next log line
        ("Prepare"), anyway.

      - Any passwords in the "Prepare" line should be substituted with their
        hashes, or a notice.

      Do not print anything if this is an SQL prepared statement and
      we're inside a stored procedure (also called Dynamic SQL) --
      sub-statements inside stored procedures are not logged into
      the general log.
    */
    if (thd->sp_runtime_ctx == nullptr) {
      if (thd->rewritten_query().length())
        query_logger.general_log_write(thd, COM_STMT_PREPARE,
                                       thd->rewritten_query().ptr(),
                                       thd->rewritten_query().length());
      else
        query_logger.general_log_write(
            thd, COM_STMT_PREPARE, m_query_string.str, m_query_string.length);

      /* audit plugins can return an error */
      error |= thd->is_error();
    }
  }

  /* Restore the original rewritten query. */
  stmt_backup.restore_rlb(thd);

  thd->m_digest = parent_digest;
  thd->m_statement_psi = parent_locker;

  return error;
}

/**
  Assign parameter values from the execute packet.

  @param thd             current thread
  @param expanded_query  a container with the original SQL statement.
                         '?' placeholders will be replaced with
                         their values in case of success.
                         The result is used for logging and replication
  @param has_new_types   flag used to signal that new types are provided.
  @param parameters      prepared statement's parsed parameters.

  @returns false if success, true if error
           (likely a conversion error, out of memory, or malformed packet)
*/

bool Prepared_statement::set_parameters(THD *thd, String *expanded_query,
                                        bool has_new_types,
                                        PS_PARAM *parameters) {
  if (m_param_count == 0) return false;

  if (insert_parameters(thd, expanded_query, has_new_types, parameters)) {
    reset_stmt_parameters(this);
    return true;
  }
  return false;
}

/**
  Assign parameter values from specified variables..

  @param thd             current thread
  @param expanded_query  a container with the original SQL statement.
                         '?' placeholders will be replaced with
                         their values in case of success.
                         The result is used for logging and replication

  @returns false if success, true if error
           (likely a conversion error or out of memory)
*/

bool Prepared_statement::set_parameters(THD *thd, String *expanded_query) {
  if (insert_parameters_from_vars(thd, thd->lex->prepared_stmt_params,
                                  expanded_query)) {
    reset_stmt_parameters(this);
    return true;
  }
  return false;
}

/**
  Disables the general log for the current session by setting the OPTION_LOG_OFF
  bit in thd->variables.option_bits.

  @param thd the session
  @return whether the setting was changed
  @retval false if the general log was already disabled for this session
  @retval true if the general log was enabled for the session and is now
  disabled
*/
static bool disable_general_log(THD *thd) {
  if ((thd->variables.option_bits & OPTION_LOG_OFF) != 0) return false;
  thd->variables.option_bits |= OPTION_LOG_OFF;
  return true;
}

/**
  Check resolved parameter types versus actual parameter types.
  Assumes that parameter values have been assigned to the parameters.

  @returns true if parameter types are compatible, false otherwise.
*/

bool Prepared_statement::check_parameter_types() {
  Item_param **end = m_param_array + m_param_count;

  for (Item_param **it = m_param_array; it < end; ++it) {
    Item_param *const item = *it;

    assert(item->param_state() != Item_param::NO_VALUE);

    /*
      - An inherited type is always accepted, it is like a dynamic cast and
        a runtime check is needed.
      - A pinned type is always accepted and not force a reprepare. However,
        the actual type must be checked for consistency when parameters values
        are analyzed.
      - The NULL value is accepted for all parameter types.
    */
    if (item->is_type_inherited() || item->is_type_pinned() ||
        item->param_state() == Item_param::NULL_VALUE)
      continue;

    /*
      It is expected that all string values may be cast to the desired data
      type (but errors may be reported during this conversion).
      So we do not reprepare in that case, with one exception. Consider:
      col = param, where 'col' is of an signed integer type; and so 'param' is
      BIGINT SIGNED. Assume that a string is passed for 'param', like
      '18446744073709551615'. In a conventional, non-prepared execution,
      comparison would be done as DOUBLE and would yield FALSE. In prepared
      execution, conversion of this string to signed BIGINT means that 'param'
      would be -1, and the comparison could yield TRUE. This is dealt with
      below.
    */
    if (item->data_type_actual() == MYSQL_TYPE_VARCHAR) {
      if (item->result_type() == INT_RESULT) {
        // First evaluate as DECIMAL
        String *s = item->val_str(nullptr);
        my_decimal decimal_value;
        int result = str2my_decimal(0, s->ptr(), s->length(), s->charset(),
                                    &decimal_value);
        if (result == E_DEC_TRUNCATED || result == E_DEC_BAD_NUM) {
          // Garbage in string, execution proceeds; at evaluation time val_int()
          // will send a warning/error, which is not the present function's job.
          continue;
        }
        // If result==E_DEC_OVERFLOW, decimal_value was rounded to a value
        // (+-9999...) which is larger than any integer, so the comparison with
        // 'col' will be correct. So we needn't handle this case
        // specially.
        if (result == E_DEC_OK) {
          longlong i;  // Convert to integer, just to see if it is in range
          result = my_decimal2int(0, &decimal_value, item->unsigned_flag, &i);
        }
        if (result != E_DEC_OK) {
          // Value is outside integer range; re-prepare as DECIMAL
          item->set_decimal(&decimal_value);
          return false;
        }
      }
      continue;
    }

    switch (item->data_type()) {
      case MYSQL_TYPE_BOOL:
      case MYSQL_TYPE_TINY:
      case MYSQL_TYPE_SHORT:
      case MYSQL_TYPE_INT24:
      case MYSQL_TYPE_LONG:
      case MYSQL_TYPE_LONGLONG:
      case MYSQL_TYPE_BIT:
      case MYSQL_TYPE_YEAR:
        /*
          When resolved type is integer, accept only integer values.
          Accept only values of same signedness, otherwise actual values may
          be out of range.
          Exception: YEAR values must be range-checked anyway, so they are
          allowed, regardless of signedness.
        */
        if (item->data_type_actual() != MYSQL_TYPE_LONGLONG ||
            (item->data_type() != MYSQL_TYPE_YEAR &&
             item->unsigned_flag != item->is_unsigned_actual()))
          return false;
        break;
      case MYSQL_TYPE_NEWDECIMAL:
        /*
          Parameters of type DECIMAL have large precisions, so they can
          also accommodate any integer values, both signed and unsigned.
        */
        assert(item->decimal_precision() - item->decimals >= 20);
        if (item->data_type_actual() != MYSQL_TYPE_LONGLONG &&
            item->data_type_actual() != MYSQL_TYPE_NEWDECIMAL)
          return false;
        break;
      case MYSQL_TYPE_FLOAT:
      case MYSQL_TYPE_DOUBLE:
        /*
          Parameters of floating-point type accept also integer and
          decimal values. Rounding errors may occur during conversion to
          DOUBLE, but this is acceptable since it has already been inferred
          that the function using the parameter treats its arguments as
          floating-point.
        */
        if (item->data_type_actual() != MYSQL_TYPE_LONGLONG &&
            item->data_type_actual() != MYSQL_TYPE_NEWDECIMAL &&
            item->data_type_actual() != MYSQL_TYPE_DOUBLE)
          return false;
        break;
      case MYSQL_TYPE_DATE:
        /*
          Allow temporal values to be provided as numbers.
          (a) date_expr = numeric *literal* has a special behaviour, the
          numeric literal is converted to a date (e.g. 20010101 -> 2001-01-01)
          and arguments are compared as DATE.
          (b) date_expr = other numeric expression, doesn't have this (so
          arguments get compared as DOUBLE).
          (a) and (b) differ in this subtle case: if the number is 00010101 it
          is 10101 and thus as date it is a two-year date so converted to
          20010101. While as DOUBLE it is 10101. So in one case it matches
          the date 2001-01-01 and in the other it matches 0001-01-01.
          We want {date_expr = ? , pass a number for ?}
          to behave like (a). Without the special case below, it would cause a
          re-preparation and then be treated like (b).
        */
        if (item->data_type_actual() == MYSQL_TYPE_LONGLONG ||
            item->data_type_actual() == MYSQL_TYPE_NEWDECIMAL ||
            item->data_type_actual() == MYSQL_TYPE_DOUBLE ||
            item->data_type_actual() == MYSQL_TYPE_DATE)
          continue;
        // If actual type is TIME or DATETIME, force a reprepare as DATETIME
        if (item->data_type_actual() == MYSQL_TYPE_DATETIME ||
            item->data_type_actual() == MYSQL_TYPE_TIME)
          return false;
        break;
      case MYSQL_TYPE_TIME:
        if (item->data_type_actual() == MYSQL_TYPE_LONGLONG ||
            item->data_type_actual() == MYSQL_TYPE_NEWDECIMAL ||
            item->data_type_actual() == MYSQL_TYPE_DOUBLE ||
            item->data_type_actual() == MYSQL_TYPE_TIME ||
            item->data_type_actual() == MYSQL_TYPE_DATETIME)
          continue;
        /*
          Actual type DATETIME: use the time component (sic)
          Actual type DATE: reprepare as DATETIME
        */
        if (item->data_type_actual() == MYSQL_TYPE_DATE) {
          return false;
        }
        break;
      case MYSQL_TYPE_DATETIME:
        if (item->data_type_actual() == MYSQL_TYPE_LONGLONG ||
            item->data_type_actual() == MYSQL_TYPE_NEWDECIMAL ||
            item->data_type_actual() == MYSQL_TYPE_DOUBLE ||
            item->data_type_actual() == MYSQL_TYPE_DATETIME ||
            item->data_type_actual() == MYSQL_TYPE_DATE ||
            item->data_type_actual() == MYSQL_TYPE_TIME)
          continue;
        break;
      case MYSQL_TYPE_VARCHAR:
      case MYSQL_TYPE_JSON:
      case MYSQL_TYPE_TINY_BLOB:
      case MYSQL_TYPE_MEDIUM_BLOB:
      case MYSQL_TYPE_BLOB:
      case MYSQL_TYPE_LONG_BLOB:
        if (item->data_type_actual() == MYSQL_TYPE_LONGLONG ||
            item->data_type_actual() == MYSQL_TYPE_NEWDECIMAL ||
            item->data_type_actual() == MYSQL_TYPE_DOUBLE ||
            item->data_type_actual() == MYSQL_TYPE_DATETIME ||
            item->data_type_actual() == MYSQL_TYPE_TIMESTAMP ||
            item->data_type_actual() == MYSQL_TYPE_DATE ||
            item->data_type_actual() == MYSQL_TYPE_TIME)
          return false;
        break;
      default:
        assert(false);
    }
  }

  return true;
}

/**
  Execute a prepared statement. Re-prepare it a limited number
  of times if necessary.

  Try to execute a prepared statement. If there is a metadata
  validation error, prepare a new copy of the prepared statement,
  swap the old and the new statements, and try again.
  If there is a validation error again, repeat the above, but
  perform not more than a maximum number of times. Reprepare_observer
  ensures that a prepared statement execution is retried not more than a
  maximum number of times.

  @note We have to try several times in a loop since we
  release metadata locks on tables after prepared statement
  prepare. Therefore, a DDL statement may sneak in between prepare
  and execute of a new statement. If a prepared statement execution
  is retried for a maximum number of times then we give up.

  @param thd              current thread.
  @param expanded_query   Query string.
  @param open_cursor      Flag to specify if a cursor should be used.

  @return  a bool value representing the function execution status.
  @retval  true    error: either statement execution is retried for
                   a maximum number of times or some general error.
  @retval  false   successfully executed the statement, perhaps
                   after having reprepared it a few times.
*/

bool Prepared_statement::execute_loop(THD *thd, String *expanded_query,
                                      bool open_cursor) {
  Reprepare_observer reprepare_observer;
  bool error;
  bool reprepared_for_types [[maybe_unused]] = false;

  /* Check if we got an error when sending long data */
  if (m_arena.get_state() == Query_arena::STMT_ERROR) {
    my_message(m_last_errno, m_last_error, MYF(0));
    return true;
  }

  assert(!thd->get_stmt_da()->is_set());

  if (unlikely(!thd->security_context()->account_is_locked() &&
               thd->security_context()->password_expired() &&
               m_lex->sql_command != SQLCOM_SET_PASSWORD &&
               m_lex->sql_command != SQLCOM_ALTER_USER)) {
    my_error(ER_MUST_CHANGE_PASSWORD, MYF(0));
    return true;
  }

  // Remember if the general log was temporarily disabled when repreparing the
  // statement for a secondary engine.
  bool general_log_temporarily_disabled = false;

  // Reprepare statement unconditionally if it contains UDF references
  if (m_lex->has_udf() && reprepare(thd)) return true;

  // Reprepare statement if protocol has changed.
  // Note: this is not possible in current code base, hence the assert.
  if (m_active_protocol != nullptr &&
      m_active_protocol != thd->get_protocol()) {
    assert(false);
    if (reprepare(thd)) return true;
  }

reexecute:
  /*
    If the item_list is not empty, we'll wrongly free some externally
    allocated items when cleaning up after validation of the prepared
    statement.
  */
  assert(thd->item_list() == nullptr);

  if (!check_parameter_types()) {
    // Only one reprepare is required in case of parameter mismatch
    assert(!reprepared_for_types);
    reprepared_for_types = true;
    if (reprepare(thd)) return true;
    goto reexecute;
  }

  reprepared_for_types = false;
  /*
    Install the metadata observer. If some metadata version is
    different from prepare time and an observer is installed,
    the observer method will be invoked to push an error into
    the error stack.
  */
  Reprepare_observer *stmt_reprepare_observer = nullptr;

  if (sql_command_flags[m_lex->sql_command] & CF_REEXECUTION_FRAGILE) {
    reprepare_observer.reset_reprepare_observer();
    stmt_reprepare_observer = &reprepare_observer;
  }

  thd->push_reprepare_observer(stmt_reprepare_observer);

  error = execute(thd, expanded_query, open_cursor) || thd->is_error();

  thd->pop_reprepare_observer();

  // Check if we have a non-fatal error and the statement allows reexecution.
  if ((sql_command_flags[m_lex->sql_command] & CF_REEXECUTION_FRAGILE) &&
      error && !thd->is_fatal_error() && !thd->is_killed()) {
    // If we have an error due to a metadata change, reprepare the
    // statement and execute it again.
    if (reprepare_observer.is_invalidated()) {
      assert(thd->get_stmt_da()->mysql_errno() == ER_NEED_REPREPARE);

      if (reprepare_observer.can_retry()) {
        thd->clear_error();
        error = reprepare(thd);
        DEBUG_SYNC(thd, "after_statement_reprepare");
      } else {
        /*
          Reprepare_observer sets error status in DA but Sql_condition is not
          added. Please check Reprepare_observer::report_error(). Pushing
          Sql_condition for ER_NEED_REPREPARE here.
        */
        Diagnostics_area *da = thd->get_stmt_da();
        da->push_warning(thd, da->mysql_errno(), da->returned_sqlstate(),
                         Sql_condition::SL_ERROR, da->message_text());
      }
    } else {
      // Otherwise, if repreparation was requested, try again in the primary
      // or secondary engine, depending on cause.
      const uint err_seen = thd->get_stmt_da()->mysql_errno();
      if (err_seen == ER_PREPARE_FOR_PRIMARY_ENGINE ||
          err_seen == ER_PREPARE_FOR_SECONDARY_ENGINE) {
        assert(thd->secondary_engine_optimization() ==
               Secondary_engine_optimization::PRIMARY_TENTATIVELY);
        assert(!m_lex->unit->is_executed());
        thd->clear_error();
        if (err_seen == ER_PREPARE_FOR_SECONDARY_ENGINE) {
          thd->set_secondary_engine_optimization(
              Secondary_engine_optimization::SECONDARY);
          MYSQL_SET_PS_SECONDARY_ENGINE(m_prepared_stmt, true);
        } else {
          thd->set_secondary_engine_optimization(
              Secondary_engine_optimization::PRIMARY_ONLY);
          MYSQL_SET_PS_SECONDARY_ENGINE(m_prepared_stmt, false);
        }
        // Disable the general log. The query was written to the general log in
        // the first attempt to execute it. No need to write it twice.
        general_log_temporarily_disabled |= disable_general_log(thd);
        error = reprepare(thd);
      }

      // If (re-?)preparation or optimization failed and it was for
      // a secondary storage engine, disable the secondary storage
      // engine and try again without it.
      if (error && m_lex->m_sql_cmd != nullptr &&
          thd->secondary_engine_optimization() ==
              Secondary_engine_optimization::SECONDARY &&
          !m_lex->unit->is_executed()) {
        if (has_external_table(m_lex->query_tables)) {
          set_external_engine_fail_reason(m_lex,
                                          thd->get_stmt_da()->message_text());
        }
        thd->clear_error();
        thd->set_secondary_engine_optimization(
            Secondary_engine_optimization::PRIMARY_ONLY);
        MYSQL_SET_PS_SECONDARY_ENGINE(m_prepared_stmt, false);
        error = reprepare(thd);
        if (!error) {
          // The reprepared statement should not use a secondary engine.
          assert(!m_lex->m_sql_cmd->using_secondary_storage_engine());
          m_lex->m_sql_cmd->disable_secondary_storage_engine();
        }
      }
    }

    if (!error) /* Success */
      goto reexecute;
  }
  reset_stmt_parameters(this);

  // Re-enable the general log if it was temporarily disabled while repreparing
  // and executing a statement for a secondary engine.
  if (general_log_temporarily_disabled)
    thd->variables.option_bits &= ~OPTION_LOG_OFF;

  return error;
}

bool Prepared_statement::execute_server_runnable(
    THD *thd, Server_runnable *server_runnable) {
  Query_arena arena_backup;
  bool error;
  Query_arena *save_stmt_arena = thd->stmt_arena;
  Item_change_list save_change_list;
  thd->change_list.move_elements_to(&save_change_list);

  m_arena.set_state(Query_arena::STMT_REGULAR_EXECUTION);

  assert(m_lex == nullptr);

  m_lex = new (m_arena.mem_root) st_lex_local;
  if (m_lex == nullptr) return true;

  Statement_backup stmt_backup;
  stmt_backup.set_thd_to_ps(thd, this);
  stmt_backup.save_rlb(thd);
  thd->swap_query_arena(m_arena, &arena_backup);
  thd->stmt_arena = &m_arena;

  error = server_runnable->execute_server_code(thd);

  thd->cleanup_after_query();

  thd->swap_query_arena(arena_backup, &m_arena);
  stmt_backup.restore_thd(thd, this);
  stmt_backup.restore_rlb(thd);
  thd->stmt_arena = save_stmt_arena;

  save_change_list.move_elements_to(&thd->change_list);

  // Items and memory will be freed in destructor

  return error;
}

/**
  Reprepare this prepared statement.

  Performs a light reset of the Prepared_statement object (resets its MEM_ROOT
  and clears its MEM_ROOT allocated members), and calls prepare() on it again.

  The resetting of the MEM_ROOT and clearing of the MEM_ROOT allocated members
  is performed by a swap operation (swap_prepared_statement()) on this
  Prepared_statement and a newly created, intermediate Prepared_statement. This
  both clears the data from this object and stores a backup of the original data
  in the intermediate object. If the repreparation fails, the original data is
  swapped back into this Prepared_statement.

  @param   thd    current thread.

  @retval  true   an error occurred. Possible errors include
                  incompatibility of new and old result set
                  metadata
  @retval  false  success, the statement has been reprepared
*/

bool Prepared_statement::reprepare(THD *thd) {
  char saved_cur_db_name_buf[NAME_LEN + 1];
  LEX_STRING saved_cur_db_name = {saved_cur_db_name_buf,
                                  sizeof(saved_cur_db_name_buf)};

  /*
    Create an intermediate Prepared_statement object and move the MEM_ROOT
    allocated data of the original Prepared_statement into it. Set up a guard
    that moves the data back into the original statement if the repreparation
    fails.
  */
  Prepared_statement copy(thd);

  swap_prepared_statement(&copy);
  auto copy_guard =
      create_scope_guard([&]() { swap_prepared_statement(&copy); });

  thd->status_var.com_stmt_reprepare++;

  /*
    m_name has been moved to the copy. Allocate it again in the original
    statement.
  */
  if (copy.m_name.str != nullptr && set_name(copy.m_name)) return true;

  bool cur_db_changed;
  if (mysql_opt_change_db(thd, copy.m_db, &saved_cur_db_name, true,
                          &cur_db_changed))
    return true;

  /*
    Suppress sending metadata to the client while repreparing. It was sent
    during the initial preparation.
  */
  const unsigned saved_sql_prepare = is_sql_prepare();
  set_sql_prepare();
  m_arena.is_repreparing = true;  // To check whether this is a reprepare.
  const bool prepare_error =
      prepare(thd, copy.m_query_string.str, copy.m_query_string.length,
              copy.m_param_array);
  set_sql_prepare(saved_sql_prepare);
  m_arena.is_repreparing = false;

  if (cur_db_changed)
    mysql_change_db(thd, to_lex_cstring(saved_cur_db_name), true);

  if (prepare_error) return true;

  if (validate_metadata(thd, &copy)) return true;

  /* Update reprepare count for this prepared statement in P_S table. */
  MYSQL_REPREPARE_PS(m_prepared_stmt);

  /*
    A new parameter array was created by prepare(). Make sure it contains the
    same values as the original array.
  */
  assert(m_param_count == copy.m_param_count);
  swap_parameter_array(m_param_array, copy.m_param_array, m_param_count);

  /*
    Clear possible warnings during reprepare, it has to be completely
    transparent to the user. We use reset_condition_info() since
    there were no separate query id issued for re-prepare.
    Sic: we can't simply silence warnings during reprepare, because if
    it's failed, we need to return all the warnings to the user.
  */
  thd->get_stmt_da()->reset_condition_info(thd);

  copy_guard.commit();

  return false;
}

/**
  Validate statement result set metadata (if the statement returns
  a result set).

  Currently we only check that the number of columns of the result
  set did not change.
  This is a helper method used during re-prepare.

  @param thd   current thread.
  @param copy  the re-prepared prepared statement to verify the metadata of

  @retval true  error, ER_PS_REBIND is reported
  @retval false statement return no or compatible metadata
*/

bool Prepared_statement::validate_metadata(THD *thd, Prepared_statement *copy) {
  /**
    If this is an SQL prepared statement or EXPLAIN,
    return false -- the metadata of the original SELECT,
    if any, has not been sent to the client.
  */
  if (is_sql_prepare() || m_lex->is_explain()) return false;

  if (m_lex->query_block->num_visible_fields() !=
      copy->m_lex->query_block->num_visible_fields()) {
    /** Column counts mismatch, update the client */
    thd->server_status |= SERVER_STATUS_METADATA_CHANGED;
  }

  return false;
}

/**
  Swap the MEM_ROOT allocated data of two prepared statements.

  This is a private helper that is used as part of statement reprepare. It is
  used in the beginning of reprepare() to clear the MEM_ROOT of the statement
  before the new preparation, while keeping the data available as some of it is
  needed later in the repreparation. It is also used for restoring the original
  data from the copy, should the repreparation fail.

  The operation is symmetric. It can be used both for saving an original
  statement into a backup, and for restoring the original state of the statement
  from the backup.
*/

void Prepared_statement::swap_prepared_statement(Prepared_statement *copy) {
  Query_arena tmp_arena;

  /* Swap memory roots. */
  std::swap(m_mem_root, copy->m_mem_root);

  /* Swap the arenas */
  m_arena.swap_query_arena(copy->m_arena, &tmp_arena);
  copy->m_arena.set_query_arena(tmp_arena);

  /* Swap the statement attributes */
  std::swap(m_lex, copy->m_lex);

  std::swap(m_query_string, copy->m_query_string);

  /* Swap mem_roots back, they must continue pointing at the m_mem_roots */
  std::swap(m_arena.mem_root, copy->m_arena.mem_root);
  /*
    Swap the old and the new parameters array. The old array
    is allocated in the old arena.
  */
  std::swap(m_param_array, copy->m_param_array);
  std::swap(m_param_count, copy->m_param_count);

  /* Don't swap flags: the copy has IS_SQL_PREPARE always set. */
  /* std::swap(flags, copy->flags); */
  /* Swap names, the old name is allocated in the wrong memory root */
  std::swap(m_name, copy->m_name);
  /* Ditto */
  std::swap(m_db, copy->m_db);
  // Need a new cursor-specific query result after repreparation
  std::swap(m_cursor_result, copy->m_cursor_result);
  std::swap(m_regular_result, copy->m_regular_result);
  std::swap(m_aux_result, copy->m_aux_result);

  // Need a new cursor, if requested
  std::swap(m_cursor, copy->m_cursor);
}

/**
  Execute a prepared statement that has already been prepared.

  @param thd                current thread.
  @param expanded_query     A query for binlogging which has all parameter
                            markers ('?') replaced with their actual values.
  @param open_cursor        True if an attempt to open a cursor should be made.
                            Currently used only in the binary protocol.

  @note
    Preconditions:
    - Caller must ensure that thd->change_list and thd->item_list are empty;
      This function will free them after execution.

    Postconditions.
    - There are no items in thd->change_list.
    - thd->mem_root may contain memory allocated during execution.

  @returns false if success, true if error
*/

bool Prepared_statement::execute(THD *thd, String *expanded_query,
                                 bool open_cursor) {
  // This flag is required for proper cleanup in the scope guard.
  bool status = true;

  Query_arena *old_stmt_arena = nullptr;
  char saved_cur_db_name_buf[NAME_LEN + 1];
  LEX_STRING saved_cur_db_name = {saved_cur_db_name_buf,
                                  sizeof(saved_cur_db_name_buf)};
  bool cur_db_changed = false;

  Sql_cmd_dml *sql_cmd =
      m_lex->m_sql_cmd != nullptr && m_lex->m_sql_cmd->is_dml()
          ? down_cast<Sql_cmd_dml *>(m_lex->m_sql_cmd)
          : nullptr;

  resourcegroups::Resource_group *src_res_grp = nullptr;
  resourcegroups::Resource_group *dest_res_grp = nullptr;
  MDL_ticket *ticket = nullptr;
  MDL_ticket *cur_ticket = nullptr;
  auto mgr_ptr = resourcegroups::Resource_group_mgr::instance();

  bool resource_group_switched = false;

  // Validate preconditions:
  assert(thd->change_list.is_empty());
  assert(thd->item_list() == nullptr);

  thd->status_var.com_stmt_execute++;

  /*
    Reset the diagnostics area.

    For regular statements, this would have happened in the parsing
    stage.

    SQL prepared statements (SQLCOM_EXECUTE) also have a parsing
    stage first (where we find out it's EXECUTE ... [USING ...]).

    However, ps-protocol prepared statements have no parsing stage for
    COM_STMT_EXECUTE before coming here, so we reset the condition info
    here.  Since diagnostics statements can't be prepared, we don't need
    to make an exception for them.

    Note that this will also reset any warnings reported during initial
    processing of parameter values, e.g when converting string values to
    temporal values.
  */
  thd->get_stmt_da()->reset_condition_info(thd);

  if (open_cursor) {
    // Only DML statements may have assigned a cursor.
    if (sql_cmd == nullptr) {
      my_error(ER_WRONG_ARGUMENTS, MYF(0), "with cursor");
      return true;
    }
    /*
      Some statements that return result sets don't support a cursor,
      e.g. SELECT ... INTO.
    */
    if (sql_cmd->query_result() != nullptr &&
        sql_cmd->query_result()->check_supports_cursor()) {
      DBUG_PRINT("info", ("Cursor asked for not SELECT stmt"));
      return true;
    }
    /*
      For some statements, such as the CALL statement, query_result() is NULL.
      For these, no error is returned and no cursor is opened -- the client
      library will recognize this case and materialize the result set.
    */
  }

  // Prevent recursive calls from e.g. stored routines
  if (m_in_use) {
    my_error(ER_PS_NO_RECURSION, MYF(0));
    return true;
  }
  m_in_use = true;

  // If statement was used as cursor in last execution, close it.
  if (m_used_as_cursor) close_cursor();

  /*
    Set up THD for execution of prepared statement:
    - THD::lex becomes statement's LEX
    - THD::m_query_string becomes statement's query string
    - THD::m_rewritten_query becomes statement's rewritten query
    - THD::stmt_arena becomes statement's arena (m_item_list, state)
    - THD::mem_root remains the current execution mem_root
  */
  Statement_backup stmt_backup;
  stmt_backup.set_thd_to_ps(thd, this);
  stmt_backup.save_rlb(thd);

  auto execute_guard = create_scope_guard([&]() {
    // In an error situation, cursor may have been left open, close it:
    if (status && open_cursor) {
      if (m_cursor == nullptr && m_cursor_result != nullptr) {
        m_cursor = m_cursor_result->cursor();
      }
      if (m_cursor != nullptr && m_cursor->is_open()) {
        close_cursor();
      }
    }
    /*
      Restore the current database (if changed).

      Force switching back to the saved current database (if changed),
      because it may be NULL. In this case, mysql_change_db() would generate
      an error.
    */
    if (cur_db_changed)
      mysql_change_db(thd, to_lex_cstring(saved_cur_db_name), true);

    cleanup_stmt(thd);

    m_lex->release_plugins();

    if (resource_group_switched)
      mgr_ptr->restore_original_resource_group(thd, src_res_grp, dest_res_grp);
    thd->resource_group_ctx()->m_switch_resource_group_str[0] = '\0';
    if (ticket != nullptr)
      mgr_ptr->release_shared_mdl_for_resource_group(thd, ticket);
    if (cur_ticket != nullptr)
      mgr_ptr->release_shared_mdl_for_resource_group(thd, cur_ticket);

    /*
      Note that we cannot call restore_thd() here as that would overwrite
      the expanded query in THD::m_query_string, which is needed for slow
      logging. Use alloc_query() to make sure the query is allocated on
      the correct MEM_ROOT, since otherwise THD::m_query_string could end up
      as a dangling pointer (i.e. pointer to freed memory) once
      the PS MEM_ROOT is freed.
    */
    mysql_mutex_lock(&thd->LOCK_thd_data);
    thd->lex = stmt_backup.lex();
    mysql_mutex_unlock(&thd->LOCK_thd_data);
    alloc_query(thd, thd->query().str, thd->query().length);

    thd->stmt_arena = old_stmt_arena;

    // Restore the original rewritten query.
    stmt_backup.restore_rlb(thd);

    if (m_arena.get_state() == Query_arena::STMT_PREPARED)
      m_arena.set_state(Query_arena::STMT_EXECUTED);

    if (!status && m_lex->sql_command == SQLCOM_CALL)
      thd->get_protocol()->send_parameters(&m_lex->param_list,
                                           is_sql_prepare());
    m_in_use = false;

    // Validate postconditions:
    assert(thd->change_list.is_empty());
  });

  /*
    Change the current database (if needed).

    Force switching, because the database of the prepared statement may be
    NULL (prepared statements can be created while no current database
    selected).
  */
  if (mysql_opt_change_db(thd, m_db, &saved_cur_db_name, true,
                          &cur_db_changed)) {
    return true;
  }

  // Allocate the expanded query on the THD memroot
  if (expanded_query->length() != 0 &&
      alloc_query(thd, expanded_query->ptr(), expanded_query->length())) {
    my_error(ER_OUTOFMEMORY, MYF(ME_FATALERROR), expanded_query->length());
    return true;
  }

  /*
    Make the statement's statement arena available in the THD.
    This is required so that the statement's item list is available.
    But notice that thd->mem_root is still the caller's mem_root.
  */
  old_stmt_arena = thd->stmt_arena;
  thd->stmt_arena = &m_arena;

  if (m_lex->check_preparation_invalid(thd)) return true;

  /*
    Set a hint so mysql_execute_command() won't clear the DA *again*,
    thereby discarding any conditions we might raise in here
    (e.g. "database we prepared with no longer exists", ER_BAD_DB_ERROR).
  */
  m_lex->keep_diagnostics = DA_KEEP_PARSE_ERROR;

  // Prepare for a new execution
  m_lex->clear_execution();

  if (open_cursor) {
    Query_result_send *new_result = nullptr;
    m_lex->safe_to_cache_query = false;
    /*
      Initialize a Query_result object before opening the cursor, unless
      it has already been created for this statement.
      Remark that query result object is created depending upon current
      protocol, hence we need to ensure that protocol does not change
      during the lifetime of a prepared statement.
      cf. Prepared_statement::execute_loop().
    */
    if (m_cursor_result != nullptr) {
      m_active_protocol = thd->get_protocol();
    } else if (sql_cmd->may_use_cursor()) {
      if (thd->is_classic_protocol())
        new_result = new (m_arena.mem_root) Query_fetch_protocol_binary(thd);
      else
        new_result = new (m_arena.mem_root) Query_result_send();
      if (new_result == nullptr) return true;  // OOM
      m_cursor_result = new_cursor_result(m_arena.mem_root, new_result);
      if (m_cursor_result == nullptr) {
        destroy(new_result);
        return true;
      }
      // Saved result for proper destruction
      m_aux_result = new_result;
      // Signal that query result must be prepared on execution
      sql_cmd->set_lazy_result();
    }
    sql_cmd->set_query_result(m_cursor_result);
    m_lex->result = m_cursor_result;

    if (m_cursor_result == nullptr) {
      m_cursor_result = m_lex->unit->query_result();
    }
  } else {
    if (sql_cmd != nullptr) {
      if (m_regular_result == nullptr)
        m_regular_result = sql_cmd->query_result();
      else
        sql_cmd->set_query_result(m_regular_result);
      m_lex->result = m_regular_result;
    }
  }
  /*
    Log COM_STMT_EXECUTE to the general log. Note, that in case of SQL
    prepared statements this causes two records to be output:

    Query       EXECUTE <statement name>
    Execute     <statement SQL text>

    This is considered user-friendly, since in the
    second log entry we output values of parameter markers.

    Rewriting/password obfuscation:

    - Any passwords in the "Execute" line should be substituted with
      their hashes, or a notice.

    Rewrite first (if needed); execution might replace passwords
    with hashes in situ without flagging it, and then we'd make
    a hash of that hash.
  */
  rewrite_query_if_needed(thd);
  log_execute_line(thd);

  thd->binlog_need_explicit_defaults_ts =
      m_lex->binlog_need_explicit_defaults_ts;
  resource_group_switched = mgr_ptr->switch_resource_group_if_needed(
      thd, &src_res_grp, &dest_res_grp, &ticket, &cur_ticket);

  status = mysql_execute_command(thd, true);
  if (status) return true;

  if (open_cursor) {
    if (m_cursor == nullptr && m_cursor_result != nullptr) {
      m_cursor = m_cursor_result->cursor();
    }
    /*
      Execution was successful. For most queries, a cursor has been created
      and must be opened, however for some queries, no cursor is used.
      This is possible if some command writes directly to the network,
      bypassing Query_result mechanism. An example of such command is
      SHOW PRIVILEGES (whose command class inherits from Sql_cmd_show_noplan).
    */
    if (m_cursor != nullptr) {
      m_used_as_cursor = true;
      /*
        NOTE: close_thread_tables() has been called in mysql_execute_command(),
        so all tables except for the cursor temporary table have been closed.
      */
      if (m_cursor->open(thd)) {
        return true;
      }
    }
  }
  return false;
}

/** Common part of DEALLOCATE PREPARE and mysqld_stmt_close. */

void Prepared_statement::deallocate(THD *thd) {
  /* We account deallocate in the same manner as mysqld_stmt_close */
  thd->status_var.com_stmt_close++;
  /* Statement map calls delete stmt on erase */
  thd->stmt_map.erase(this);
}

/***************************************************************************
 * Ed_result_set
 ***************************************************************************/
/**
  Initialize an instance of Ed_result_set.

  Instances of the class, as well as all result set rows, are
  always allocated in the memory root passed over as the third
  argument. In the constructor, we take over ownership of the
  memory root. It will be freed when the class is destroyed.

  sic: Ed_result_est is not designed to be allocated on stack.
*/

Ed_result_set::Ed_result_set(List<Ed_row> *rows_arg, Ed_row *fields,
                             size_t column_count_arg, MEM_ROOT *mem_root_arg)
    : m_mem_root(std::move(*mem_root_arg)),
      m_column_count(column_count_arg),
      m_rows(rows_arg),
      m_fields(fields),
      m_next_rset(nullptr) {}

/***************************************************************************
 * Ed_result_set
 ***************************************************************************/

/**
  Create a new "execute direct" connection.
*/

Ed_connection::Ed_connection(THD *thd)
    : m_diagnostics_area(false),
      m_thd(thd),
      m_rsets(nullptr),
      m_current_rset(nullptr) {}

/**
  Free all result sets of the previous statement, if any,
  and reset warnings and errors.

  Called before execution of the next query.
*/

void Ed_connection::free_old_result() {
  while (m_rsets) {
    Ed_result_set *rset = m_rsets->m_next_rset;
    delete m_rsets;
    m_rsets = rset;
  }
  m_current_rset = m_rsets;
  m_diagnostics_area.reset_diagnostics_area();
  m_diagnostics_area.reset_condition_info(m_thd);
}

/**
  A simple wrapper that uses a helper class to execute SQL statements.
*/

bool Ed_connection::execute_direct(LEX_STRING sql_text) {
  Execute_sql_statement execute_sql_statement(sql_text);
  DBUG_PRINT("ed_query", ("%s", sql_text.str));

  return execute_direct(&execute_sql_statement);
}

/**
  Execute a fragment of server functionality without an effect on
  thd, and store results in memory.

  Conventions:
  - the code fragment must finish with OK, EOF or ERROR.
  - the code fragment doesn't have to close thread tables,
  free memory, commit statement transaction or do any other
  cleanup that is normally done in the end of dispatch_command().

  @param server_runnable A code fragment to execute.
*/

bool Ed_connection::execute_direct(Server_runnable *server_runnable) {
  DBUG_TRACE;

  free_old_result(); /* Delete all data from previous execution, if any */

  Protocol_local protocol_local(m_thd, this);
  m_thd->push_protocol(&protocol_local);
  m_thd->push_diagnostics_area(&m_diagnostics_area);

  Prepared_statement stmt(m_thd);
  bool rc = stmt.execute_server_runnable(m_thd, server_runnable);
  m_thd->send_statement_status();

  m_thd->pop_protocol();
  m_thd->pop_diagnostics_area();
  /*
    Protocol_local makes use of m_current_rset to keep
    track of the last result set, while adding result sets to the end.
    Reset it to point to the first result set instead.
  */
  m_current_rset = m_rsets;

  /*
    Reset rewritten (for password obfuscation etc.) query after
    internal call from NDB etc.  Without this, a rewritten query
    would get "stuck" in SHOW PROCESSLIST.
  */
  m_thd->reset_rewritten_query();
  m_thd->reset_query_for_display();

  return rc;
}

/**
  A helper method that is called only during execution.

  Although Ed_connection doesn't support multi-statements,
  a statement may generate many result sets. All subsequent
  result sets are appended to the end.

  @pre This is called only by Protocol_local.
*/

void Ed_connection::add_result_set(Ed_result_set *ed_result_set) {
  if (m_rsets) {
    m_current_rset->m_next_rset = ed_result_set;
    /* While appending, use m_current_rset as a pointer to the tail. */
    m_current_rset = ed_result_set;
  } else
    m_current_rset = m_rsets = ed_result_set;
}

/*************************************************************************
 * Protocol_local
 **************************************************************************/

Protocol_local::Protocol_local(THD *thd, Ed_connection *ed_connection)
    : m_connection(ed_connection),
      m_rset(nullptr),
      m_column_count(0),
      m_current_row(nullptr),
      m_current_column(nullptr),
      m_send_metadata(false),
      m_thd(thd) {}

/**
  A helper function to add the current row to the current result
  set. Called in @sa start_row(), when a new row is started,
  and in send_eof(), when the result set is finished.
*/

void Protocol_local::opt_add_row_to_rset() {
  if (m_current_row) {
    /* Add the old row to the result set */
    Ed_row *ed_row = new (&m_rset_root) Ed_row(m_current_row, m_column_count);
    if (ed_row) m_rset->push_back(ed_row, &m_rset_root);
  }
}

/**
  Add a NULL column to the current row.
*/

bool Protocol_local::store_null() {
  if (m_current_column == nullptr)
    return true; /* start_row() failed to allocate memory. */

  memset(m_current_column, 0, sizeof(*m_current_column));
  ++m_current_column;
  return false;
}

/**
  A helper method to add any column to the current row
  in its binary form.

  Allocates memory for the data in the result set memory root.
*/

bool Protocol_local::store_column(const void *data, size_t length) {
  if (m_current_column == nullptr)
    return true; /* start_row() failed to allocate memory. */

  m_current_column->str = new (&m_rset_root) char[length + 1];
  if (!m_current_column->str) return true;
  memcpy(m_current_column->str, data, length);
  m_current_column->str[length + 1] = '\0'; /* Safety */
  m_current_column->length = length;
  ++m_current_column;
  return false;
}

/**
  Store a string value in a result set column, optionally
  having converted it to character_set_results.
*/

bool Protocol_local::store_string(const char *str, size_t length,
                                  const CHARSET_INFO *src_cs,
                                  const CHARSET_INFO *dst_cs) {
  /* Store with conversion */
  String convert;
  uint error_unused;

  if (dst_cs && !my_charset_same(src_cs, dst_cs) && src_cs != &my_charset_bin &&
      dst_cs != &my_charset_bin) {
    if (convert.copy(str, length, src_cs, dst_cs, &error_unused)) return true;
    str = convert.ptr();
    length = convert.length();
  }

  if (m_current_column == nullptr)
    return true; /* start_row() failed to allocate memory. */

  m_current_column->str = strmake_root(&m_rset_root, str, length);
  if (!m_current_column->str) return true;
  m_current_column->length = length;
  ++m_current_column;
  return false;
}

/** Store a tiny int as is (1 byte) in a result set column. */

bool Protocol_local::store_tiny(longlong value, uint32) {
  char v = (char)value;
  return store_column(&v, 1);
}

/** Store a short as is (2 bytes, host order) in a result set column. */

bool Protocol_local::store_short(longlong value, uint32) {
  int16 v = (int16)value;
  return store_column(&v, 2);
}

/** Store a "long" as is (4 bytes, host order) in a result set column.  */

bool Protocol_local::store_long(longlong value, uint32) {
  int32 v = (int32)value;
  return store_column(&v, 4);
}

/** Store a "longlong" as is (8 bytes, host order) in a result set column. */

bool Protocol_local::store_longlong(longlong value, bool, uint32) {
  int64 v = (int64)value;
  return store_column(&v, 8);
}

/** Store a decimal in string format in a result set column */

bool Protocol_local::store_decimal(const my_decimal *value, uint prec,
                                   uint dec) {
  StringBuffer<DECIMAL_MAX_STR_LENGTH> str;
  const int rc = my_decimal2string(E_DEC_FATAL_ERROR, value, prec, dec, &str);

  if (rc) return true;

  return store_column(str.ptr(), str.length());
}

/** Convert to cs_results and store a string. */

bool Protocol_local::store_string(const char *str, size_t length,
                                  const CHARSET_INFO *src_cs) {
  const CHARSET_INFO *dst_cs;

  dst_cs = m_connection->m_thd->variables.character_set_results;
  return store_string(str, length, src_cs, dst_cs);
}

/* Store MYSQL_TIME (in binary format) */

bool Protocol_local::store_datetime(const MYSQL_TIME &time, uint) {
  return store_column(&time, sizeof(MYSQL_TIME));
}

/** Store MYSQL_TIME (in binary format) */

bool Protocol_local::store_date(const MYSQL_TIME &time) {
  return store_column(&time, sizeof(MYSQL_TIME));
}

/** Store MYSQL_TIME (in binary format) */

bool Protocol_local::store_time(const MYSQL_TIME &time, uint) {
  return store_column(&time, sizeof(MYSQL_TIME));
}

/* Store a floating point number, as is. */

bool Protocol_local::store_float(float value, uint32, uint32) {
  return store_column(&value, sizeof(float));
}

/* Store a double precision number, as is. */

bool Protocol_local::store_double(double value, uint32, uint32) {
  return store_column(&value, sizeof(double));
}

/* Store a Field. */

bool Protocol_local::store_field(const Field *field) {
  return field->send_to_protocol(this);
}

/** Called for statements that don't have a result set, at statement end. */

bool Protocol_local::send_ok(uint, uint, ulonglong, ulonglong, const char *) {
  /*
    Just make sure nothing is sent to the client, we have grabbed
    the status information in the connection Diagnostics Area.
  */
  m_column_count = 0;
  return false;
}

/**
  Called at the end of a result set. Append a complete
  result set to the list in Ed_connection.

  Don't send anything to the client, but instead finish
  building of the result set at hand.
*/

bool Protocol_local::send_eof(uint, uint) {
  Ed_result_set *ed_result_set;

  assert(m_rset);
  m_current_row = nullptr;

  ed_result_set = new (&m_rset_root)
      Ed_result_set(m_rset, m_fields, m_column_count, &m_rset_root);

  m_rset = nullptr;
  m_fields = nullptr;

  if (!ed_result_set) return true;

  /*
    Link the created Ed_result_set instance into the list of connection
    result sets. Never fails.
  */
  m_connection->add_result_set(ed_result_set);
  m_column_count = 0;
  return false;
}

/** Called to send an error to the client at the end of a statement. */

bool Protocol_local::send_error(uint, const char *, const char *) {
  /*
    Just make sure that nothing is sent to the client (default
    implementation).
  */
  m_column_count = 0;
  return false;
}

int Protocol_local::read_packet() { return 0; }

ulong Protocol_local::get_client_capabilities() { return 0; }

bool Protocol_local::has_client_capability(unsigned long) { return false; }

bool Protocol_local::connection_alive() const { return false; }

void Protocol_local::end_partial_result_set() {}

int Protocol_local::shutdown(bool) { return 0; }

/**
  Called between two result set rows.

  Prepare structures to fill result set rows.
  Unfortunately, we can't return an error here. If memory allocation
  fails, we'll have to return an error later. And so is done
  in methods such as @sa store_column().
*/
void Protocol_local::start_row() {
  DBUG_TRACE;

  if (m_send_metadata) return;
  assert(alloc_root_inited(&m_rset_root));

  /* Start a new row. */
  m_current_row =
      (Ed_column *)m_rset_root.Alloc(sizeof(Ed_column) * m_column_count);
  m_current_column = m_current_row;
}

/**
  Add the current row to the result set
*/
bool Protocol_local::end_row() {
  DBUG_TRACE;
  if (m_send_metadata) return false;

  assert(m_rset);
  opt_add_row_to_rset();
  m_current_row = nullptr;

  return false;
}

uint Protocol_local::get_rw_status() { return 0; }

bool Protocol_local::start_result_metadata(uint elements, uint,
                                           const CHARSET_INFO *) {
  m_column_count = elements;
  start_row();
  m_send_metadata = true;
  m_rset = new (&m_rset_root) List<Ed_row>;
  return false;
}

bool Protocol_local::end_result_metadata() {
  m_send_metadata = false;
  m_fields = new (&m_rset_root) Ed_row(m_current_row, m_column_count);
  m_current_row = nullptr;
  return false;
}

bool Protocol_local::send_field_metadata(Send_field *field,
                                         const CHARSET_INFO *cs) {
  store_string(field->col_name, strlen(field->col_name), cs);
  return false;
}

bool Protocol_local::get_compression() { return false; }

char *Protocol_local::get_compression_algorithm() { return nullptr; }

uint Protocol_local::get_compression_level() { return 0; }

int Protocol_local::get_command(COM_DATA *, enum_server_command *) {
  return -1;
}
