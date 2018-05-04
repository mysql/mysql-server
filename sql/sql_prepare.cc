/* Copyright (c) 2002, 2018, Oracle and/or its affiliates. All rights reserved.

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
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include <atomic>
#include <memory>
#include <unordered_map>
#include <utility>

#include "binary_log_types.h"
#include "decimal.h"
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
#include "sql/auth/auth_acls.h"
#include "sql/auth/auth_common.h"  // check_table_access
#include "sql/auth/sql_security_ctx.h"
#include "sql/binlog.h"
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
#include "sql/psi_memory_key.h"
#include "sql/resourcegroups/resource_group_basic_types.h"
#include "sql/resourcegroups/resource_group_mgr.h"
#include "sql/session_tracker.h"
#include "sql/set_var.h"    // set_var_base
#include "sql/sp_cache.h"   // sp_cache_enforce_limit
#include "sql/sql_audit.h"  // mysql_global_audit_mask
#include "sql/sql_base.h"   // open_tables_for_query, open_temporary_table
#include "sql/sql_cmd.h"
#include "sql/sql_cmd_ddl_table.h"
#include "sql/sql_const.h"
#include "sql/sql_cursor.h"  // Server_side_cursor
#include "sql/sql_db.h"      // mysql_change_db
#include "sql/sql_digest_stream.h"
#include "sql/sql_handler.h"  // mysql_ha_rm_tables
#include "sql/sql_lex.h"
#include "sql/sql_parse.h"  // sql_command_flags
#include "sql/sql_profile.h"
#include "sql/sql_query_rewrite.h"
#include "sql/sql_rewrite.h"  // mysql_rewrite_query
#include "sql/sql_view.h"     // create_view_precheck
#include "sql/system_variables.h"
#include "sql/table.h"
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

/**
  Execute one SQL statement in an isolated context.
*/

class Execute_sql_statement : public Server_runnable {
 public:
  Execute_sql_statement(LEX_STRING sql_text);
  virtual bool execute_server_code(THD *thd);

 private:
  LEX_STRING m_sql_text;
};

/**
  Protocol_local: a helper class to intercept the result
  of the data written to the network.
*/

class Protocol_local : public Protocol {
 public:
  Protocol_local(THD *thd, Ed_connection *ed_connection);
  ~Protocol_local() { free_root(&m_rset_root, MYF(0)); }

  virtual int read_packet();

  virtual int get_command(COM_DATA *com_data, enum_server_command *cmd);
  virtual ulong get_client_capabilities();
  virtual bool has_client_capability(unsigned long client_capability);
  virtual void end_partial_result_set();
  virtual int shutdown(bool server_shutdown = false);
  virtual bool connection_alive();
  virtual void start_row();
  virtual bool end_row();
  virtual void abort_row(){};
  virtual uint get_rw_status();
  virtual bool get_compression();

  virtual bool start_result_metadata(uint num_cols, uint flags,
                                     const CHARSET_INFO *resultcs);
  virtual bool end_result_metadata();
  virtual bool send_field_metadata(Send_field *field,
                                   const CHARSET_INFO *charset);
  virtual bool flush() { return true; }
  virtual bool send_parameters(List<Item_param> *, bool) { return false; }
  bool store_ps_status(ulong, uint, uint, ulong) { return false; }

 protected:
  String *convert;
  virtual bool store_null();
  virtual bool store_tiny(longlong from);
  virtual bool store_short(longlong from);
  virtual bool store_long(longlong from);
  virtual bool store_longlong(longlong from, bool unsigned_flag);
  virtual bool store_decimal(const my_decimal *, uint, uint);
  virtual bool store(const char *from, size_t length, const CHARSET_INFO *cs);
  virtual bool store(const char *from, size_t length,
                     const CHARSET_INFO *fromcs, const CHARSET_INFO *tocs);
  virtual bool store(MYSQL_TIME *time, uint precision);
  virtual bool store_date(MYSQL_TIME *time);
  virtual bool store_time(MYSQL_TIME *time, uint precision);
  virtual bool store(float value, uint32 decimals, String *buffer);
  virtual bool store(double value, uint32 decimals, String *buffer);
  virtual bool store(Proto_field *field);

  virtual enum enum_protocol_type type() { return PROTOCOL_LOCAL; }
  virtual enum enum_vio_type connection_type() { return VIO_TYPE_LOCAL; }

  virtual bool send_ok(uint server_status, uint statement_warn_count,
                       ulonglong affected_rows, ulonglong last_insert_id,
                       const char *message);

  virtual bool send_eof(uint server_status, uint statement_warn_count);
  virtual bool send_error(uint sql_errno, const char *err_msg,
                          const char *sqlstate);

 private:
  bool store_string(const char *str, size_t length, const CHARSET_INFO *src_cs,
                    const CHARSET_INFO *dst_cs);

  bool store_column(const void *data, size_t length);
  void opt_add_row_to_rset();

 private:
  Ed_connection *m_connection;
  MEM_ROOT m_rset_root;
  List<Ed_row> *m_rset;
  size_t m_column_count;
  Ed_column *m_current_row;
  Ed_column *m_current_column;
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

  if ((thd->sp_runtime_ctx == NULL) && (general || opt_slow_log || opt_bin_log))
    mysql_rewrite_query(thd);
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
  if (thd->sp_runtime_ctx != NULL) return;

  if (thd->rewritten_query.length())
    query_logger.general_log_write(thd, COM_STMT_EXECUTE,
                                   thd->rewritten_query.c_ptr_safe(),
                                   thd->rewritten_query.length());
  else
    query_logger.general_log_write(thd, COM_STMT_EXECUTE, thd->query().str,
                                   thd->query().length);
}

class Statement_backup {
  LEX *m_lex;
  LEX_CSTRING m_query_string;
  String m_rewritten_query;

 public:
  LEX *lex() const { return m_lex; }

  /**
    Prepared the THD to execute the prepared statement.
    Save the current THD statement state.
  */
  void set_thd_to_ps(THD *thd, Prepared_statement *stmt) {
    DBUG_ENTER("Statement_backup::set_thd_to_ps");

    mysql_mutex_lock(&thd->LOCK_thd_data);
    m_lex = thd->lex;
    thd->lex = stmt->lex;
    mysql_mutex_unlock(&thd->LOCK_thd_data);

    m_query_string = thd->query();
    thd->set_query(stmt->m_query_string);

    DBUG_VOID_RETURN;
  }

  /**
    Restore the THD statement state after the prepared
    statement has finished executing.
  */
  void restore_thd(THD *thd, Prepared_statement *stmt) {
    DBUG_ENTER("Statement_backup::restore_thd");

    mysql_mutex_lock(&thd->LOCK_thd_data);
    stmt->lex = thd->lex;
    thd->lex = m_lex;
    mysql_mutex_unlock(&thd->LOCK_thd_data);

    stmt->m_query_string = thd->query();
    thd->set_query(m_query_string);

    DBUG_VOID_RETURN;
  }

  /**
    Save the current rewritten query prior to
    rewriting the prepared statement.
  */
  void save_rlb(THD *thd) {
    DBUG_ENTER("Statement_backup::save_rlb");

    if (thd->rewritten_query.length() > 0) {
      /* Duplicate the original rewritten query. */
      m_rewritten_query.copy(thd->rewritten_query);
      /* Swap the duplicate with the original. */
      thd->rewritten_query.swap(m_rewritten_query);
    }

    DBUG_VOID_RETURN;
  }

  /**
    Restore the rewritten query after the prepared
    statement has finished executing.
  */
  void restore_rlb(THD *thd) {
    DBUG_ENTER("Statement_backup::restore_rlb");

    if (m_rewritten_query.length() > 0) {
      /* Restore with swap() instead of '='. */
      thd->rewritten_query.swap(m_rewritten_query);
      /* Free the rewritten prepared statement. */
      m_rewritten_query.mem_free();
    }

    DBUG_VOID_RETURN;
  }
};

static bool send_statement(THD *thd, const Prepared_statement *stmt,
                           uint no_columns, Query_result *result,
                           List<Item> *types);

/**
  Data conversion routines.

    All these functions read the data from pos, convert it to requested
    type and assign to param; pos is advanced to predefined length.

    Make a note that the NULL handling is examined at first execution
    (i.e. when input types altered) and for all subsequent executions
    we don't read any values for this.

  @param  param             parameter item
  @param  pos               input data buffer
  @param  len               length of data in the buffer
*/

static void set_param_tiny(Item_param *param, uchar **pos, ulong len) {
  if (len < 1) return;
  int8 value = (int8) * *pos;
  param->set_int(
      param->unsigned_flag ? (longlong)((uint8)value) : (longlong)value, 4);
}

static void set_param_short(Item_param *param, uchar **pos, ulong len) {
  int16 value;
  if (len < 2) return;
  value = sint2korr(*pos);
  param->set_int(
      param->unsigned_flag ? (longlong)((uint16)value) : (longlong)value, 6);
}

static void set_param_int32(Item_param *param, uchar **pos, ulong len) {
  int32 value;
  if (len < 4) return;
  value = sint4korr(*pos);
  param->set_int(
      param->unsigned_flag ? (longlong)((uint32)value) : (longlong)value, 11);
}

static void set_param_int64(Item_param *param, uchar **pos, ulong len) {
  longlong value;
  if (len < 8) return;
  value = sint8korr(*pos);
  param->set_int(value, 21);
}

static void set_param_float(Item_param *param, uchar **pos, ulong len) {
  float data;
  if (len < 4) return;
  float4get(&data, *pos);
  param->set_double((double)data);
}

static void set_param_double(Item_param *param, uchar **pos, ulong len) {
  double data;
  if (len < 8) return;
  float8get(&data, *pos);
  param->set_double(data);
}

static void set_param_decimal(Item_param *param, uchar **pos, ulong len) {
  param->set_decimal((char *)*pos, len);
}

/*
  Read date/time/datetime parameter values from network (binary
  protocol). See writing counterparts of these functions in
  libmysql.c (store_param_{time,date,datetime}).
*/

/**
  @todo
    Add warning 'Data truncated' here
*/
static void set_param_time(Item_param *param, uchar **pos, ulong len) {
  MYSQL_TIME tm;

  if (len >= 8) {
    uchar *to = *pos;
    uint day;

    tm.neg = (bool)to[0];
    day = (uint)sint4korr(to + 1);
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
  } else
    set_zero_time(&tm, MYSQL_TIMESTAMP_TIME);
  param->set_time(&tm, MYSQL_TIMESTAMP_TIME,
                  MAX_TIME_FULL_WIDTH * MY_CHARSET_BIN_MB_MAXLEN);
}

static void set_param_datetime(Item_param *param, uchar **pos, ulong len) {
  MYSQL_TIME tm;

  if (len >= 4) {
    uchar *to = *pos;

    tm.neg = 0;
    tm.year = (uint)sint2korr(to);
    tm.month = (uint)to[2];
    tm.day = (uint)to[3];
    if (len > 4) {
      tm.hour = (uint)to[4];
      tm.minute = (uint)to[5];
      tm.second = (uint)to[6];
    } else
      tm.hour = tm.minute = tm.second = 0;

    tm.second_part = (len > 7) ? (ulong)sint4korr(to + 7) : 0;
  } else
    set_zero_time(&tm, MYSQL_TIMESTAMP_DATETIME);
  param->set_time(&tm, MYSQL_TIMESTAMP_DATETIME,
                  MAX_DATETIME_FULL_WIDTH * MY_CHARSET_BIN_MB_MAXLEN);
}

static void set_param_date(Item_param *param, uchar **pos, ulong len) {
  MYSQL_TIME tm;

  if (len >= 4) {
    uchar *to = *pos;

    tm.year = (uint)sint2korr(to);
    tm.month = (uint)to[2];
    tm.day = (uint)to[3];

    tm.hour = tm.minute = tm.second = 0;
    tm.second_part = 0;
    tm.neg = 0;
  } else
    set_zero_time(&tm, MYSQL_TIMESTAMP_DATE);
  param->set_time(&tm, MYSQL_TIMESTAMP_DATE,
                  MAX_DATE_WIDTH * MY_CHARSET_BIN_MB_MAXLEN);
}

static void set_param_str(Item_param *param, uchar **pos, ulong len) {
  param->set_str((const char *)*pos, len);
}

static void setup_one_conversion_function(THD *thd, Item_param *param,
                                          enum enum_field_types param_type) {
  switch (param_type) {
    case MYSQL_TYPE_TINY:
      param->set_param_func = set_param_tiny;
      param->item_type = Item::INT_ITEM;
      param->item_result_type = INT_RESULT;
      break;
    case MYSQL_TYPE_SHORT:
      param->set_param_func = set_param_short;
      param->item_type = Item::INT_ITEM;
      param->item_result_type = INT_RESULT;
      break;
    case MYSQL_TYPE_LONG:
      param->set_param_func = set_param_int32;
      param->item_type = Item::INT_ITEM;
      param->item_result_type = INT_RESULT;
      break;
    case MYSQL_TYPE_LONGLONG:
      param->set_param_func = set_param_int64;
      param->item_type = Item::INT_ITEM;
      param->item_result_type = INT_RESULT;
      break;
    case MYSQL_TYPE_FLOAT:
      param->set_param_func = set_param_float;
      param->item_type = Item::REAL_ITEM;
      param->item_result_type = REAL_RESULT;
      break;
    case MYSQL_TYPE_DOUBLE:
      param->set_param_func = set_param_double;
      param->item_type = Item::REAL_ITEM;
      param->item_result_type = REAL_RESULT;
      break;
    case MYSQL_TYPE_DECIMAL:
    case MYSQL_TYPE_NEWDECIMAL:
      param->set_param_func = set_param_decimal;
      param->item_type = Item::DECIMAL_ITEM;
      param->item_result_type = DECIMAL_RESULT;
      break;
    case MYSQL_TYPE_TIME:
      param->set_param_func = set_param_time;
      param->item_type = Item::STRING_ITEM;
      param->item_result_type = STRING_RESULT;
      break;
    case MYSQL_TYPE_DATE:
      param->set_param_func = set_param_date;
      param->item_type = Item::STRING_ITEM;
      param->item_result_type = STRING_RESULT;
      break;
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_TIMESTAMP:
      param->set_param_func = set_param_datetime;
      param->item_type = Item::STRING_ITEM;
      param->item_result_type = STRING_RESULT;
      break;
    case MYSQL_TYPE_TINY_BLOB:
    case MYSQL_TYPE_MEDIUM_BLOB:
    case MYSQL_TYPE_LONG_BLOB:
    case MYSQL_TYPE_BLOB:
      param->set_param_func = set_param_str;
      param->value.cs_info.character_set_of_placeholder = &my_charset_bin;
      param->value.cs_info.character_set_client =
          thd->variables.character_set_client;
      DBUG_ASSERT(thd->variables.character_set_client);
      param->value.cs_info.final_character_set_of_str_value = &my_charset_bin;
      param->item_type = Item::STRING_ITEM;
      param->item_result_type = STRING_RESULT;
      break;
    default:
      /*
        The client library ensures that we won't get any other typecodes
        except typecodes above and typecodes for string types. Marking
        label as 'default' lets us to handle malformed packets as well.
      */
      {
        const CHARSET_INFO *fromcs = thd->variables.character_set_client;
        const CHARSET_INFO *tocs = thd->variables.collation_connection;
        size_t dummy_offset;

        param->value.cs_info.character_set_of_placeholder = fromcs;
        param->value.cs_info.character_set_client = fromcs;

        /*
          Setup source and destination character sets so that they
          are different only if conversion is necessary: this will
          make later checks easier.
        */
        param->value.cs_info.final_character_set_of_str_value =
            String::needs_conversion(0, fromcs, tocs, &dummy_offset) ? tocs
                                                                     : fromcs;
        param->set_param_func = set_param_str;
        /*
          Exact value of max_length is not known unless data is converted to
          charset of connection, so we have to set it later.
        */
        param->item_type = Item::STRING_ITEM;
        param->item_result_type = STRING_RESULT;
      }
  }
  param->set_data_type(param_type);
}

/**
  Check whether this parameter data type is compatible with long data.
  Used to detect whether a long data stream has been supplied to a
  incompatible data type.
*/
inline bool is_param_long_data_type(Item_param *param) {
  return ((param->data_type() >= MYSQL_TYPE_TINY_BLOB) &&
          (param->data_type() <= MYSQL_TYPE_STRING));
}

/**
  Routines to assign parameters from data supplied by the client.

    Update the parameter markers by reading data from the packet and
    and generate a valid query for logging.

  @note
    with_log is set when one of slow or general logs are open.
    Logging of prepared statements in all cases is performed
    by means of conventional queries: if parameter
    data was supplied from C API, each placeholder in the query is
    replaced with its actual value; if we're logging a [Dynamic] SQL
    prepared statement, parameter markers are replaced with variable names.
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
       --> general log gets
                                 [Query]   PREPARE stmt FROM "UPDATE ..."
     EXECUTE stmt USING @a
       --> general log gets
                             [Query]   EXECUTE stmt USING @a;
    @endverbatim

  @retval
    0  if success
  @retval
    1  otherwise
*/

bool Prepared_statement::insert_params(String *query, PS_PARAM *parameters) {
  Item_param **begin = param_array;
  Item_param **end = begin + param_count;
  size_t length = 0;
  String str;
  const String *res;
  DBUG_ENTER("insert_params");

  if (with_log && query->copy(m_query_string.str, m_query_string.length,
                              default_charset_info))
    DBUG_RETURN(true);

  uint i = 0;
  for (Item_param **it = begin; it < end; ++it, ++i) {
    Item_param *param = *it;
    if (param->state != Item_param::LONG_DATA_VALUE) {
      if (parameters[i].null_bit)
        param->set_null();
      else {
        // TODO: Add error handling for set_param_func functions.
        param->set_param_func(param, const_cast<uchar **>(&parameters[i].value),
                              parameters[i].length);
        if (param->state == Item_param::NO_VALUE) DBUG_RETURN(true);

        if (with_log && param->limit_clause_param &&
            param->state != Item_param::INT_VALUE) {
          param->set_int(param->val_int(), MY_INT64_NUM_DECIMAL_DIGITS);
          param->item_type = Item::INT_ITEM;
          if (!param->unsigned_flag && param->value.integer < 0)
            DBUG_RETURN(true);
        }
      }
    }
    /*
      A long data stream was supplied for this parameter marker.
      This was done after prepare, prior to providing a placeholder
      type (the types are supplied at execute). Check that the
      supplied type of placeholder can accept a data stream.
    */
    else if (!is_param_long_data_type(param))
      DBUG_RETURN(true);
    if (with_log) {
      res = param->query_val_str(thd, &str);
      if (param->convert_str_value(thd)) DBUG_RETURN(true); /* out of memory */

      if (query->replace(param->pos_in_query + length, 1, *res))
        DBUG_RETURN(true);

      length += res->length() - 1;
    } else {
      if (param->convert_str_value(thd)) DBUG_RETURN(true); /* out of memory */
    }
    param->sync_clones();
  }
  DBUG_RETURN(false);
}

static bool setup_conversion_functions(Prepared_statement *stmt,
                                       PS_PARAM *parameters) {
  DBUG_ENTER("setup_conversion_functions");
  /*
    First execute or types altered by the client, setup the
    conversion routines for all parameters (one time)
  */
  Item_param **it = stmt->param_array;
  Item_param **end = it + stmt->param_count;
  for (uint i = 0; it < end; ++it, ++i) {
    (**it).unsigned_flag = parameters[i].unsigned_type;
    setup_one_conversion_function(stmt->thd, *it, parameters[i].type);
    (**it).sync_clones();
  }
  DBUG_RETURN(0);
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
  If with_log is set, also construct query test for binary log.

  @param varnames  List of variables. Caller must ensure that number
                   of variables in the list is equal to number of statement
                   parameters
  @param query     The query with parameter markers replaced with corresponding
                   user variables that were used to execute the query.
*/

bool Prepared_statement::insert_params_from_vars(List<LEX_STRING> &varnames,
                                                 String *query) {
  Item_param **begin = param_array;
  Item_param **end = begin + param_count;
  List_iterator<LEX_STRING> var_it(varnames);
  StringBuffer<STRING_BUFFER_USUAL_SIZE> buf;
  const String *val;
  size_t length = 0;

  DBUG_ENTER("insert_params_from_vars");

  // Reserve an extra space of 32 bytes for each placeholder parameter.
  if (with_log) query->reserve(m_query_string.length + 32 * param_count);

  /* Protects thd->user_vars */
  mysql_mutex_lock(&thd->LOCK_thd_data);

  for (Item_param **it = begin; it < end; ++it) {
    Item_param *param = *it;
    LEX_STRING *varname = var_it++;

    user_var_entry *entry =
        find_or_nullptr(thd->user_vars, to_string(*varname));
    if (with_log) {
      /*
        We have to call the setup_one_conversion_function() here to set
        the parameter's members that might be needed further
        (e.g. value.cs_info.character_set_client is used in the
        query_val_str()).
      */
      setup_one_conversion_function(thd, param, param->data_type());
      if (param->set_from_user_var(thd, entry)) goto error;
      val = param->query_val_str(thd, &buf);

      if (param->convert_str_value(thd)) goto error;

      size_t num_bytes = param->pos_in_query - length;
      if (query->length() + num_bytes + val->length() >
          std::numeric_limits<uint32>::max())
        goto error;

      if (query->append(m_query_string.str + length, num_bytes) ||
          query->append(*val))
        goto error;

      length = param->pos_in_query + 1;
    } else {
      if (param->set_from_user_var(thd, entry) || param->convert_str_value(thd))
        goto error;
    }
    param->sync_clones();
  }

  /*
    If logging, take care of tail.
  */
  if (with_log)
    query->append(m_query_string.str + length, m_query_string.length - length);

  mysql_mutex_unlock(&thd->LOCK_thd_data);
  DBUG_RETURN(0);

error:
  mysql_mutex_unlock(&thd->LOCK_thd_data);
  DBUG_RETURN(1);
}

/**
  Validate SHOW statement.

    In case of success, if this query is not EXPLAIN, send column list info
    back to the client.

  @param stmt               prepared statement
  @param tables             list of tables used in the query

  @return false on succes and true on error
*/

bool mysql_test_show(Prepared_statement *stmt, TABLE_LIST *tables) {
  THD *thd = stmt->thd;
  LEX *lex = stmt->lex;
  SELECT_LEX_UNIT *unit = lex->unit;
  DBUG_ENTER("mysql_test_show");

  DBUG_ASSERT(!thd->is_error());

  lex->select_lex->context.resolve_in_select_list = true;

  if (show_precheck(thd, lex, false)) goto error;

  DBUG_ASSERT(lex->result == NULL);
  DBUG_ASSERT(lex->sql_command != SQLCOM_DO);
  DBUG_ASSERT(!lex->is_explain());

  if (!lex->result) {
    if (!(lex->result = new (stmt->mem_root) Query_result_send(thd)))
      goto error; /* purecov: inspected */
  }

  if (open_tables_for_query(thd, tables, MYSQL_OPEN_FORCE_SHARED_MDL))
    goto error;

  thd->lex->used_tables = 0;  // Updated by setup_fields

  /*
    SELECT_LEX::prepare calls
    It is not SELECT COMMAND for sure, so setup_tables will be called as
    usual, and we pass 0 as setup_tables_done_option
  */
  if (unit->prepare(thd, 0, 0, 0)) goto error;

  DBUG_RETURN(false);
error:
  DBUG_RETURN(true);
}

bool send_statement(THD *thd, const Prepared_statement *stmt, uint no_columns,
                    Query_result *result, List<Item> *types) {
  // Send the statement status(id, no_columns, no_params, error_count);
  bool rc = thd->get_protocol()->store_ps_status(
      stmt->id, no_columns, stmt->param_count,
      thd->get_stmt_da()->current_statement_cond_count());
  if (!rc && stmt->param_count)
    // Send the list of parameters
    rc |= thd->send_result_metadata((List<Item> *)&stmt->lex->param_list,
                                    Protocol::SEND_EOF);
  if (rc) return true; /* purecov: inspected */

  // Send
  if (types && result &&
      result->send_result_set_metadata(*types, Protocol::SEND_EOF))
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

  @param stmt               prepared statement
  @param tables             list of tables used in this query
  @param var_list           list of expressions

  @retval
    false             success
  @retval
    true              error, error message is set in THD
*/

static bool mysql_test_set_fields(Prepared_statement *stmt, TABLE_LIST *tables,
                                  List<set_var_base> *var_list) {
  List_iterator_fast<set_var_base> it(*var_list);
  THD *thd = stmt->thd;
  set_var_base *var;
  DBUG_ENTER("mysql_test_set_fields");
  DBUG_ASSERT(stmt->is_stmt_prepare());

  if (tables &&
      check_table_access(thd, SELECT_ACL, tables, false, UINT_MAX, false))
    DBUG_RETURN(true); /* purecov: inspected */

  if (open_tables_for_query(thd, tables, MYSQL_OPEN_FORCE_SHARED_MDL))
    DBUG_RETURN(true); /* purecov: inspected */

  while ((var = it++)) {
    if (var->light_check(thd)) DBUG_RETURN(true); /* purecov: inspected */
  }

  DBUG_RETURN(false);
}

/**
  Check internal SELECT of the prepared command.

  @note Old version. Will be replaced with select_like_stmt_cmd_test() after
        the parser refactoring.

  @param thd            Thread handle.

  @note
    This function won't directly open tables used in select. They should
    be opened either by calling function (and in this case you probably
    should use select_like_stmt_test_with_open()) or by
    "specific_prepare" call (like this happens in case of multi-update).

  @retval
    false                success
  @retval
    true                 error, error message is set in THD
*/

static bool select_like_stmt_test(THD *thd) {
  DBUG_ENTER("select_like_stmt_test");
  LEX *const lex = thd->lex;

  lex->select_lex->context.resolve_in_select_list = true;

  thd->lex->used_tables = 0;  // Updated by setup_fields

  /* Calls SELECT_LEX::prepare */
  const bool ret = lex->unit->prepare(thd, 0, 0, 0);
  DBUG_RETURN(ret);
}

/**
  Validate and prepare for execution CREATE TABLE statement.

  @param thd          Thread handle.

  @retval
    false             success
  @retval
    true              error, error message is set in THD
*/

bool Sql_cmd_create_table::prepare(THD *thd) {
  LEX *const lex = thd->lex;
  SELECT_LEX *select_lex = lex->select_lex;
  TABLE_LIST *create_table = lex->query_tables;
  DBUG_ENTER("mysql_test_create_table");

  if (create_table_precheck(thd, query_expression_tables, create_table))
    DBUG_RETURN(true);

  if (select_lex->item_list.elements) {
    /* Base table and temporary table are not in the same name space. */
    if (!(lex->create_info->options & HA_LEX_CREATE_TMP_TABLE))
      create_table->open_type = OT_BASE_ONLY;

    if (open_tables_for_query(thd, lex->query_tables,
                              MYSQL_OPEN_FORCE_SHARED_MDL))
      DBUG_RETURN(true);

    select_lex->context.resolve_in_select_list = true;

    bool link_to_local;
    lex->unlink_first_table(&link_to_local);
    const bool res = select_like_stmt_test(thd);
    lex->link_first_table_back(create_table, link_to_local);
    if (res) DBUG_RETURN(true);
  } else {
    /*
      Check that the source table exist, and also record
      its metadata version. Even though not strictly necessary,
      we validate metadata of all CREATE TABLE statements,
      which keeps metadata validation code simple.
    */
    if (open_tables_for_query(thd, lex->query_tables,
                              MYSQL_OPEN_FORCE_SHARED_MDL))
      DBUG_RETURN(true);
  }

  set_prepared();
  DBUG_RETURN(false);
}

/**
  @brief Validate and prepare for execution CREATE VIEW statement

  @param stmt prepared statement

  @note This function handles create view commands.

  @retval false Operation was a success.
  @retval true An error occured.
*/

static bool mysql_test_create_view(Prepared_statement *stmt) {
  THD *thd = stmt->thd;
  LEX *lex = stmt->lex;
  bool res = true;
  /* Skip first table, which is the view we are creating */
  bool link_to_local;
  TABLE_LIST *view = lex->unlink_first_table(&link_to_local);
  TABLE_LIST *tables = lex->query_tables;
  DBUG_ENTER("mysql_test_create_view");
  DBUG_ASSERT(stmt->is_stmt_prepare());

  if (create_view_precheck(thd, tables, view, lex->create_view_mode)) goto err;

  /*
    Since we can't pre-open temporary tables for SQLCOM_CREATE_VIEW,
    (see mysql_create_view) we have to do it here instead.
  */
  if (open_temporary_tables(thd, tables)) goto err;

  if (open_tables_for_query(thd, tables, MYSQL_OPEN_FORCE_SHARED_MDL)) goto err;

  lex->context_analysis_only |= CONTEXT_ANALYSIS_ONLY_VIEW;
  res = select_like_stmt_test(thd);

err:
  /* put view back for PS rexecuting */
  lex->link_first_table_back(view, link_to_local);
  DBUG_RETURN(res);
}

/**
  Perform semantic analysis of the parsed tree and send a response packet
  to the client.

    This function
    - opens all tables and checks access rights
    - validates semantics of statement columns and SQL functions
      by calling fix_fields.

  @param stmt               prepared statement

  @retval
    false             success, statement metadata is sent to client
  @retval
    true              error, error message is set in THD (but not sent)
*/

static bool check_prepared_statement(Prepared_statement *stmt) {
  THD *thd = stmt->thd;
  LEX *lex = stmt->lex;
  DBUG_ASSERT(lex == thd->lex);  // set_n_backup_active_arena() guarantees that
  SELECT_LEX *select_lex = lex->select_lex;
  enum enum_sql_command sql_command = lex->sql_command;
  int res = 0;
  DBUG_ENTER("check_prepared_statement");
  DBUG_PRINT("enter",
             ("command: %d  param_count: %u", sql_command, stmt->param_count));

  lex->first_lists_tables_same();
  TABLE_LIST *const tables = lex->query_tables;

  /* set context for commands which do not use setup_tables */
  lex->select_lex->context.resolve_in_table_list_only(
      select_lex->get_table_list());

  /*
    For the optimizer trace, this is the symmetric, for statement preparation,
    of what is done at statement execution (in mysql_execute_command()).
  */
  Opt_trace_start ots(thd, tables, sql_command, &lex->var_list,
                      thd->query().str, thd->query().length, NULL,
                      thd->variables.character_set_client);

  Opt_trace_object trace_command(&thd->opt_trace);
  Opt_trace_array trace_command_steps(&thd->opt_trace, "steps");

  if ((thd->lex->keep_diagnostics == DA_KEEP_COUNTS) ||
      (thd->lex->keep_diagnostics == DA_KEEP_DIAGNOSTICS)) {
    my_error(ER_UNSUPPORTED_PS, MYF(0));
    DBUG_RETURN(true);
  }

  if (sql_command_flags[sql_command] & CF_HA_CLOSE)
    mysql_ha_rm_tables(thd, tables);

  /*
    Open temporary tables that are known now. Temporary tables added by
    prelocking will be opened afterwards (during open_tables()).
  */
  if (sql_command_flags[sql_command] & CF_PREOPEN_TMP_TABLES) {
    if (open_temporary_tables(thd, tables)) DBUG_RETURN(true);
  }

  switch (sql_command) {
    /* The following allow WHERE clause, so they must be tested like SELECT */
    case SQLCOM_SHOW_DATABASES:
    case SQLCOM_SHOW_TABLES:
    case SQLCOM_SHOW_TRIGGERS:
    case SQLCOM_SHOW_EVENTS:
    case SQLCOM_SHOW_OPEN_TABLES:
    case SQLCOM_SHOW_COLLATIONS:
    case SQLCOM_SHOW_CHARSETS:
    case SQLCOM_SHOW_VARIABLES:
    case SQLCOM_SHOW_STATUS:
    case SQLCOM_SHOW_TABLE_STATUS:
    case SQLCOM_SHOW_STATUS_PROC:
    case SQLCOM_SHOW_STATUS_FUNC:
      if (mysql_test_show(stmt, tables)) DBUG_RETURN(true);
      break;
    case SQLCOM_CREATE_VIEW:
      if (lex->create_view_mode == enum_view_create_mode::VIEW_ALTER) {
        my_error(ER_UNSUPPORTED_PS, MYF(0));
        DBUG_RETURN(true);
      }
      res = mysql_test_create_view(stmt);
      break;

    case SQLCOM_SET_OPTION:
      res = mysql_test_set_fields(stmt, tables, &lex->var_list);
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
    case SQLCOM_SHOW_FIELDS:
    case SQLCOM_SHOW_KEYS:
    case SQLCOM_CREATE_TABLE:
      res = lex->m_sql_cmd->prepare(thd);
      // @todo Temporary solution: Unprepare after preparation to preserve
      //       old behaviour
      if (!res) lex->m_sql_cmd->unprepare(thd);
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
        DBUG_RETURN(true);
      }
      break;
  }
  if (res) DBUG_RETURN(true);

  if (stmt->is_sql_prepare()) DBUG_RETURN(false);

  List<Item> *types = nullptr;
  Query_result *result = nullptr;
  uint no_columns = 0;

  if ((sql_command_flags[lex->sql_command] & CF_HAS_RESULT_SET) &&
      !lex->is_explain()) {
    SELECT_LEX_UNIT *unit = lex->unit;
    result = unit->query_result();
    if (result == nullptr) result = unit->first_select()->query_result();
    if (result == nullptr) result = lex->result;
    types = unit->get_unit_column_types();
    no_columns = result->field_count(*types);
  }

  DBUG_RETURN(send_statement(thd, stmt, no_columns, result, types));
}

/**
  Initialize array of parameters in statement from LEX.
  (We need to have quick access to items by number in mysql_stmt_get_longdata).
  This is to avoid using malloc/realloc in the parser.
*/

static bool init_param_array(Prepared_statement *stmt) {
  LEX *lex = stmt->lex;
  if ((stmt->param_count = lex->param_list.elements)) {
    if (stmt->param_count > (uint)UINT_MAX16) {
      /* Error code to be defined in 5.0 */
      my_error(ER_PS_MANY_PARAM, MYF(0));
      return true;
    }

    Item_param **to;
    List_iterator<Item_param> param_iterator(lex->param_list);
    /* Use thd->mem_root as it points at statement mem_root */
    stmt->param_array = (Item_param **)alloc_root(
        stmt->thd->mem_root, sizeof(Item_param *) * stmt->param_count);
    if (!stmt->param_array) return true;
    for (to = stmt->param_array; to < stmt->param_array + stmt->param_count;
         ++to) {
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
  DBUG_ENTER("mysqld_stmt_prepare");
  DBUG_PRINT("prep_query", ("%s", query));
  DBUG_ASSERT(stmt != nullptr);

  bool switch_protocol = thd->is_classic_protocol();
  if (switch_protocol) {
    // set the current client capabilities before switching the protocol
    thd->protocol_binary.set_client_capabilities(
        thd->get_protocol()->get_client_capabilities());
    thd->push_protocol(&thd->protocol_binary);
  }

  /* Create PS table entry, set query text after rewrite. */
  stmt->m_prepared_stmt =
      MYSQL_CREATE_PS(stmt, stmt->id, thd->m_statement_psi, stmt->name().str,
                      stmt->name().length, NULL, 0);

  if (stmt->prepare(query, length)) {
    /* Delete this stmt stats from PS table. */
    MYSQL_DESTROY_PS(stmt->m_prepared_stmt);
    /* Statement map deletes statement on erase */
    thd->stmt_map.erase(stmt);
  }

  if (switch_protocol) thd->pop_protocol();

  sp_cache_enforce_limit(thd->sp_proc_cache, stored_program_cache_size);
  sp_cache_enforce_limit(thd->sp_func_cache, stored_program_cache_size);

  /* check_prepared_statement sends the metadata packet in case of success */
  DBUG_VOID_RETURN;
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
          (*stmt)->param_count) {
        /* Error will be sent in execute call */
        (*stmt)->state = Query_arena::STMT_ERROR;
        (*stmt)->last_errno = ER_WRONG_ARGUMENTS;
        sprintf((*stmt)->last_error, ER_THD(thd, ER_WRONG_ARGUMENTS),
                "mysql_stmt_precheck");
        goto silent_error;
      }
      break;
    }
    case COM_STMT_EXECUTE: {
      stmt_id = com_data->com_stmt_execute.stmt_id;
      if (!(*stmt = thd->stmt_map.find(stmt_id))) goto not_found;
      if ((*stmt)->param_count != com_data->com_stmt_execute.parameter_count)
        goto wrong_arg;
      break;
    }
    default:
      DBUG_ASSERT(0);
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
  char *query_str = 0;

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
      var_value = entry->val_str(&is_var_null, &str, NOT_FIXED_DEC);

      mysql_mutex_unlock(&thd->LOCK_thd_data);

      /*
        NULL value of variable checked early as entry->value so here
        we can't get NULL in normal conditions
      */
      DBUG_ASSERT(!is_var_null);
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
    if (!(query_str = (char *)alloc_root(thd->mem_root, len + 1))) goto end;

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
  Prepared_statement *stmt;
  const char *query;
  size_t query_len = 0;
  DBUG_ENTER("mysql_sql_stmt_prepare");

  if ((stmt = thd->stmt_map.find_by_name(name))) {
    /*
      If there is a statement with the same name, remove it. It is ok to
      remove old and fail to insert a new one at the same time.
    */
    if (stmt->is_in_use()) {
      my_error(ER_PS_NO_RECURSION, MYF(0));
      DBUG_VOID_RETURN;
    }

    MYSQL_DESTROY_PS(stmt->m_prepared_stmt);
    stmt->deallocate();
  }

  if (!(query = get_dynamic_sql_string(lex, &query_len)) ||
      !(stmt = new Prepared_statement(thd))) {
    DBUG_VOID_RETURN; /* out of memory */
  }

  stmt->set_sql_prepare();

  /* Set the name first, insert should know that this statement has a name */
  if (stmt->set_name(name)) {
    delete stmt;
    DBUG_VOID_RETURN;
  }

  if (thd->stmt_map.insert(stmt)) {
    /* The statement is deleted and an error is set if insert fails */
    DBUG_VOID_RETURN;
  }

  /* Create PS table entry, set query text after rewrite. */
  stmt->m_prepared_stmt =
      MYSQL_CREATE_PS(stmt, stmt->id, thd->m_statement_psi, stmt->name().str,
                      stmt->name().length, NULL, 0);

  if (stmt->prepare(query, query_len)) {
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
          ->mark_as_changed(thd, NULL);
    my_ok(thd, 0L, 0L, "Statement prepared");
  }

  DBUG_VOID_RETURN;
}

/**
  Reinit prepared statement/stored procedure before execution. Resets the LEX
  object.


  @todo
    When the new table structure is ready, then have a status bit
    to indicate the table is altered, and re-do the setup_*
    and open the tables back.

  @retval false OK.
  @retval true Error.
*/

bool reinit_stmt_before_use(THD *thd, LEX *lex) {
  SELECT_LEX *sl = lex->all_selects_list;
  DBUG_ENTER("reinit_stmt_before_use");

  // Default to READ access for every field that is resolved
  thd->mark_used_columns = MARK_COLUMNS_READ;
  /*
    We have to update "thd" pointer in LEX, all its units and in LEX::result,
    since statements which belong to trigger body are associated with TABLE
    object and because of this can be used in different threads.
  */
  lex->thd = thd;

  if (lex->m_sql_cmd != NULL) lex->m_sql_cmd->cleanup(thd);

  for (; sl; sl = sl->next_select_in_list()) {
    if (!sl->first_execution) {
      /* see unique_table() */
      sl->exclude_from_table_unique_test = false;

      /*
        These must be reset before every new preparation.
        @note done here and not in SELECT_LEX::prepare() since for
              multi-table UPDATE and DELETE, derived tables are merged into
              the outer query block before ::prepare() is called.
      */
      sl->cond_count = 0;
      sl->between_count = 0;
      sl->max_equal_elems = 0;

      if (sl->where_cond()) {
        DBUG_ASSERT(sl->where_cond()->real_item());  // no dangling 'ref'
        sl->where_cond()->cleanup();
      }
      if (sl->having_cond()) {
        DBUG_ASSERT(sl->having_cond()->real_item());
        sl->having_cond()->cleanup();
      }
      DBUG_ASSERT(sl->join == 0);
      ORDER *order;
      /* Fix GROUP list */
      if (sl->group_list_ptrs && sl->group_list_ptrs->size() > 0) {
        for (uint ix = 0; ix < sl->group_list_ptrs->size() - 1; ++ix) {
          order = sl->group_list_ptrs->at(ix);
          order->next = sl->group_list_ptrs->at(ix + 1);
        }
      }
      for (order = sl->group_list.first; order; order = order->next)
        order->item = &order->item_ptr;
      /* Fix ORDER list */
      if (sl->order_list_ptrs && sl->order_list_ptrs->size() > 0) {
        for (uint ix = 0; ix < sl->order_list_ptrs->size() - 1; ++ix) {
          order = sl->order_list_ptrs->at(ix);
          order->next = sl->order_list_ptrs->at(ix + 1);
        }
      }
      for (order = sl->order_list.first; order; order = order->next)
        order->item = &order->item_ptr;
      if (sl->m_windows.elements > 0) {
        List_iterator<Window> li(sl->m_windows);
        Window *w;
        while ((w = li++)) w->reinit_before_use();
      }
    }
    {
      SELECT_LEX_UNIT *unit = sl->master_unit();
      unit->unclean();
      unit->types.empty();
      /* for derived tables & PS (which can't be reset by Item_subquery) */
      unit->reinit_exec_mechanism();
      unit->set_thd(thd);
    }
  }

  /*
    m_view_ctx_list contains all the view tables view_ctx objects and must
    be emptied now since it's going to be re-populated below as we reiterate
    over all query_tables and call TABLE_LIST::prepare_security().
  */
  thd->m_view_ctx_list.empty();

  /*
    TODO: When the new table structure is ready, then have a status bit
    to indicate the table is altered, and re-do the setup_*
    and open the tables back.
  */
  /*
    NOTE: We should reset whole table list here including all tables added
    by prelocking algorithm (it is not a problem for substatements since
    they have their own table list).
    Another note: this loop uses query_tables so does not see TABLE_LISTs
    which represent join nests.
  */
  for (TABLE_LIST *tables = lex->query_tables; tables;
       tables = tables->next_global) {
    tables->reinit_before_use(thd);
  }

  lex->set_current_select(lex->select_lex);

  if (lex->result) lex->result->set_thd(thd);

  lex->allow_sum_func = 0;
  lex->m_deny_window_func = 0;
  lex->in_sum_func = NULL;

  lex->reset_exec_started();

  if (unlikely(lex->is_broken())) {
    // Force a Reprepare, to get a fresh LEX
    Reprepare_observer *reprepare_observer = thd->get_reprepare_observer();
    if (reprepare_observer && reprepare_observer->report_error(thd)) {
      DBUG_ASSERT(thd->is_error());
      DBUG_RETURN(true);
    }
  }

  DBUG_RETURN(false);
}

/**
  Clears parameters from data left from previous execution or long data.

  @param stmt               prepared statement for which parameters should
                            be reset
*/

static void reset_stmt_params(Prepared_statement *stmt) {
  Item_param **item = stmt->param_array;
  Item_param **end = item + stmt->param_count;
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

  @param thd                  current thread
  @param stmt                 prepared statement
  @param has_new_types        true if parsed parameters have data types defined
  @param execute_flags        flag used to decide if a cursor should be used
  @param parameters           prepared statement's parsed parameters
*/

void mysqld_stmt_execute(THD *thd, Prepared_statement *stmt, bool has_new_types,
                         ulong execute_flags, PS_PARAM *parameters) {
  DBUG_ENTER("mysqld_stmt_execute");

#if defined(ENABLED_PROFILING)
  thd->profiling->set_query_source(stmt->m_query_string.str,
                                   stmt->m_query_string.length);
#endif
  DBUG_PRINT("info", ("stmt: %p", stmt));

  bool switch_protocol = thd->is_classic_protocol();
  if (switch_protocol) {
    // set the current client capabilities before switching the protocol
    thd->protocol_binary.set_client_capabilities(
        thd->get_protocol()->get_client_capabilities());
    thd->push_protocol(&thd->protocol_binary);
  }

  MYSQL_EXECUTE_PS(thd->m_statement_psi, stmt->m_prepared_stmt);

  // Query text for binary, general or slow log, if any of them is open
  String expanded_query;
  // If no error happened while setting the parameters, execute statement.
  if (!stmt->set_parameters(&expanded_query, has_new_types, parameters)) {
    bool open_cursor =
        static_cast<bool>(execute_flags & (ulong)CURSOR_TYPE_READ_ONLY);
    stmt->execute_loop(&expanded_query, open_cursor);
  }

  if (switch_protocol) thd->pop_protocol();

  sp_cache_enforce_limit(thd->sp_proc_cache, stored_program_cache_size);
  sp_cache_enforce_limit(thd->sp_func_cache, stored_program_cache_size);

  /* Close connection socket; for use with client testing (Bug#43560). */
  DBUG_EXECUTE_IF("close_conn_after_stmt_execute",
                  thd->get_protocol()->shutdown(););

  DBUG_VOID_RETURN;
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
  DBUG_ENTER("mysql_sql_stmt_execute");
  DBUG_PRINT("info", ("EXECUTE: %.*s\n", (int)name.length, name.str));

  Prepared_statement *stmt;
  if (!(stmt = thd->stmt_map.find_by_name(name))) {
    my_error(ER_UNKNOWN_STMT_HANDLER, MYF(0), static_cast<int>(name.length),
             name.str, "EXECUTE");
    DBUG_VOID_RETURN;
  }

  if (stmt->param_count != lex->prepared_stmt_params.elements) {
    my_error(ER_WRONG_ARGUMENTS, MYF(0), "EXECUTE");
    DBUG_VOID_RETURN;
  }

  DBUG_PRINT("info", ("stmt: %p", stmt));
  MYSQL_EXECUTE_PS(thd->m_statement_psi, stmt->m_prepared_stmt);

  // Query text for binary, general or slow log, if any of them is open
  String expanded_query;
  if (stmt->set_parameters(&expanded_query)) DBUG_VOID_RETURN;

  stmt->execute_loop(&expanded_query, false);

  DBUG_VOID_RETURN;
}

/**
  COM_STMT_FETCH handler: fetches requested amount of rows from cursor.

  @param thd                Thread handle.
  @param stmt               Pointer to the prepared statement.
  @param num_rows           Number of rows to fetch.
*/

void mysqld_stmt_fetch(THD *thd, Prepared_statement *stmt, ulong num_rows) {
  DBUG_ENTER("mysqld_stmt_fetch");
  thd->status_var.com_stmt_fetch++;

  Server_side_cursor *cursor = stmt->cursor;
  if (!cursor) {
    my_error(ER_STMT_HAS_NO_OPEN_CURSOR, MYF(0), stmt->id);
    DBUG_VOID_RETURN;
  }

  thd->stmt_arena = stmt;
  Statement_backup stmt_backup;
  stmt_backup.set_thd_to_ps(thd, stmt);

  cursor->fetch(num_rows);

  if (!cursor->is_open()) {
    stmt->close_cursor();
    reset_stmt_params(stmt);
  }

  stmt_backup.restore_thd(thd, stmt);
  thd->stmt_arena = thd;

  DBUG_VOID_RETURN;
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
  DBUG_ENTER("mysqld_stmt_reset");

  thd->status_var.com_stmt_reset++;
  stmt->close_cursor();

  /*
    Clear parameters from data which could be set by
    mysqld_stmt_send_long_data() call.
  */
  reset_stmt_params(stmt);

  stmt->state = Query_arena::STMT_PREPARED;

  query_logger.general_log_print(thd, thd->get_command(), NullS);

  my_ok(thd);

  DBUG_VOID_RETURN;
}

/**
  Delete a prepared statement from memory.

  @note
    we don't send any reply to this command.
*/

void mysqld_stmt_close(THD *thd, Prepared_statement *stmt) {
  DBUG_ENTER("mysqld_stmt_close");
  /*
    The only way currently a statement can be deallocated when it's
    in use is from within Dynamic SQL.
  */
  DBUG_ASSERT(!stmt->is_in_use());
  MYSQL_DESTROY_PS(stmt->m_prepared_stmt);
  stmt->deallocate();
  query_logger.general_log_print(thd, thd->get_command(), NullS);

  DBUG_VOID_RETURN;
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
  Prepared_statement *stmt;
  const LEX_CSTRING &name = thd->lex->prepared_stmt_name;
  DBUG_PRINT("info",
             ("DEALLOCATE PREPARE: %.*s\n", (int)name.length, name.str));

  if (!(stmt = thd->stmt_map.find_by_name(name)))
    my_error(ER_UNKNOWN_STMT_HANDLER, MYF(0), static_cast<int>(name.length),
             name.str, "DEALLOCATE PREPARE");
  else if (stmt->is_in_use())
    my_error(ER_PS_NO_RECURSION, MYF(0));
  else {
    MYSQL_DESTROY_PS(stmt->m_prepared_stmt);
    stmt->deallocate();
    if (thd->session_tracker.get_tracker(SESSION_STATE_CHANGE_TRACKER)
            ->is_enabled())
      thd->session_tracker.get_tracker(SESSION_STATE_CHANGE_TRACKER)
          ->mark_as_changed(thd, NULL);
    my_ok(thd);
  }
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
  DBUG_ENTER("mysql_stmt_get_longdata");

  thd->status_var.com_stmt_send_long_data++;
  Diagnostics_area new_stmt_da(false);
  thd->push_diagnostics_area(&new_stmt_da);

  Item_param *param = stmt->param_array[param_number];
  param->set_longdata((char *)str, length);
  if (thd->get_stmt_da()->is_error()) {
    stmt->state = Query_arena::STMT_ERROR;
    stmt->last_errno = thd->get_stmt_da()->mysql_errno();
    snprintf(stmt->last_error, sizeof(stmt->last_error), "%.*s",
             MYSQL_ERRMSG_SIZE - 1, thd->get_stmt_da()->message_text());
  }
  thd->pop_diagnostics_area();

  query_logger.general_log_print(thd, thd->get_command(), NullS);

  DBUG_VOID_RETURN;
}

/***************************************************************************
 Select_fetch_protocol_binary
****************************************************************************/

bool Query_fetch_protocol_binary::send_result_set_metadata(List<Item> &list,
                                                           uint flags) {
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
  rc = Query_result_send::send_result_set_metadata(list, flags);
  thd->pop_protocol();

  return rc;
}

bool Query_fetch_protocol_binary::send_eof() {
  /*
    Don't send EOF if we're in error condition (which implies we've already
    sent or are sending an error)
  */
  if (thd->is_error()) return true;

  ::my_eof(thd);
  return false;
}

bool Query_fetch_protocol_binary::send_data(List<Item> &fields) {
  bool rc;

  // set the current client capabilities before switching the protocol
  protocol.set_client_capabilities(
      thd->get_protocol()->get_client_capabilities());
  thd->push_protocol(&protocol);
  rc = Query_result_send::send_data(fields);
  thd->pop_protocol();
  return rc;
}

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

  return true;
}

/*******************************************************************
 * Server_runnable
 *******************************************************************/

Server_runnable::~Server_runnable() {}

///////////////////////////////////////////////////////////////////////////

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
  thd->m_digest = NULL;
  thd->m_statement_psi = NULL;
  error = parse_sql(thd, &parser_state, NULL) || thd->is_error();
  thd->m_digest = parent_digest;
  thd->m_statement_psi = parent_locker;

  if (error) goto end;

  thd->lex->set_trg_event_type_for_tables();

  parent_locker = thd->m_statement_psi;
  thd->m_statement_psi = NULL;

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

/***************************************************************************
 Prepared_statement
****************************************************************************/

Prepared_statement::Prepared_statement(THD *thd_arg)
    : Query_arena(&main_mem_root, STMT_INITIALIZED),
      thd(thd_arg),
      param_array(nullptr),
      cursor(nullptr),
      param_count(0),
      last_errno(0),
      id(++thd_arg->statement_id_counter),
      lex(nullptr),
      m_query_string(NULL_CSTR),
      m_prepared_stmt(nullptr),
      result(nullptr),
      flags((uint)IS_IN_USE),
      with_log(false),
      m_name(NULL_CSTR),
      m_db(NULL_CSTR) {
  init_sql_alloc(key_memory_prepared_statement_main_mem_root, &main_mem_root,
                 thd_arg->variables.query_alloc_block_size,
                 thd_arg->variables.query_prealloc_size);
  *last_error = '\0';
}

void Prepared_statement::close_cursor() {
  destroy(result);
  result = nullptr;
  delete cursor;
  cursor = nullptr;
}

void Prepared_statement::setup_set_params() {
  DBUG_EXECUTE_IF("bug16617026_simulate_audit_log_ps",
                  { lex->safe_to_cache_query = 0; });
  /*
    Decide if we have to expand the query (because we must write it to logs)
    or not.
    We don't have to substitute the params when bin-logging DML in RBL.
  */
  if ((mysql_bin_log.is_open() && is_update_query(lex->sql_command) &&
       (!thd->is_current_stmt_binlog_format_row() ||
        ((sql_command_flags[lex->sql_command] & CF_AUTO_COMMIT_TRANS) ==
         CF_AUTO_COMMIT_TRANS))) ||
      opt_general_log || opt_slow_log ||
      (lex->sql_command == SQLCOM_SELECT && lex->safe_to_cache_query &&
       !lex->is_explain()) ||
      is_global_audit_mask_set()) {
    with_log = true;
  }
}

/**
  Destroy this prepared statement, cleaning up all used memory
  and resources.

  This is called from @c deallocate() to handle COM_STMT_CLOSE and
  DEALLOCATE PREPARE or when THD ends and all prepared statements are freed.
*/

Prepared_statement::~Prepared_statement() {
  DBUG_ENTER("Prepared_statement::~Prepared_statement");
  DBUG_PRINT("enter", ("stmt: %p  cursor: %p", this, cursor));
  destroy(result);
  delete cursor;
  /*
    We have to call free on the items even if cleanup is called as some items,
    like Item_param, don't free everything until free_items()
  */
  free_items();
  if (lex) {
    DBUG_ASSERT(lex->sphead == NULL);
    lex_end(lex);
    destroy(lex->result);
    delete (st_lex_local *)lex;  // TRASH memory
  }
  free_root(&main_mem_root, MYF(0));
  DBUG_VOID_RETURN;
}

void Prepared_statement::cleanup_stmt() {
  DBUG_ENTER("Prepared_statement::cleanup_stmt");
  DBUG_PRINT("enter", ("stmt: %p", this));

  cleanup_items(free_list);
  thd->cleanup_after_query();
  thd->rollback_item_tree_changes();

  DBUG_VOID_RETURN;
}

bool Prepared_statement::set_name(const LEX_CSTRING &name_arg) {
  m_name.length = name_arg.length;
  m_name.str =
      static_cast<char *>(memdup_root(mem_root, name_arg.str, name_arg.length));
  return m_name.str == NULL;
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
    m_db.str = this->strmake(db_arg.str, db_arg.length);
    m_db.length = db_arg.length;
  } else {
    m_db = NULL_CSTR;
  }
  return db_arg.str != NULL && m_db.str == NULL;
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

  @param query_str             statement text
  @param query_length

  @note
    Precondition:
    The caller must ensure that thd->change_list and thd->free_list
    is empty: this function will not back them up but will free
    in the end of its execution.

  @note
    Postcondition:
    thd->mem_root contains unused memory allocated during validation.
*/

bool Prepared_statement::prepare(const char *query_str, size_t query_length) {
  bool error;
  Query_arena arena_backup;
  Query_arena *old_stmt_arena;
  sql_digest_state *parent_digest = thd->m_digest;
  PSI_statement_locker *parent_locker = thd->m_statement_psi;
  unsigned char *token_array = NULL;

  DBUG_ENTER("Prepared_statement::prepare");
  /*
    If this is an SQLCOM_PREPARE, we also increase Com_prepare_sql.
    However, it seems handy if com_stmt_prepare is increased always,
    no matter what kind of prepare is processed.
  */
  thd->status_var.com_stmt_prepare++;

  if (!(lex = new (mem_root) st_lex_local)) DBUG_RETURN(true);

  if (set_db(thd->db())) DBUG_RETURN(true);

  /*
    alloc_query() uses thd->memroot && thd->query, so we should call
    both of backup_statement() and backup_query_arena() here.
  */
  Statement_backup stmt_backup;
  stmt_backup.set_thd_to_ps(thd, this);
  stmt_backup.save_rlb(thd);
  thd->set_n_backup_active_arena(this, &arena_backup);

  if (alloc_query(thd, query_str, query_length)) {
    stmt_backup.restore_thd(thd, this);
    stmt_backup.restore_rlb(thd);
    thd->restore_active_arena(this, &arena_backup);
    DBUG_RETURN(true);
  }

  if (max_digest_length > 0) {
    token_array = (unsigned char *)thd->alloc(max_digest_length);
  }

  old_stmt_arena = thd->stmt_arena;
  thd->stmt_arena = this;

  Parser_state parser_state;
  if (parser_state.init(thd, thd->query().str, thd->query().length)) {
    stmt_backup.restore_thd(thd, this);
    stmt_backup.restore_rlb(thd);
    thd->restore_active_arena(this, &arena_backup);
    thd->stmt_arena = old_stmt_arena;
    DBUG_RETURN(true);
  }

  parser_state.m_lip.stmt_prepare_mode = true;
  parser_state.m_lip.multi_statements = false;

  lex_start(thd);
  lex->context_analysis_only |= CONTEXT_ANALYSIS_ONLY_PREPARE;

  thd->m_digest = NULL;
  thd->m_statement_psi = NULL;

  sql_digest_state digest;
  digest.reset(token_array, max_digest_length);
  thd->m_digest = &digest;

  enable_digest_if_any_plugin_needs_it(thd, &parser_state);
  if (is_audit_plugin_class_active(thd, MYSQL_AUDIT_GENERAL_CLASS))
    parser_state.m_input.m_compute_digest = true;

  thd->m_parser_state = &parser_state;
  invoke_pre_parse_rewrite_plugins(thd);
  thd->m_parser_state = NULL;

  error = thd->is_error();

  if (!error) {
    error = parse_sql(thd, &parser_state, NULL) || thd->is_error() ||
            init_param_array(this);
  }
  if (!error) {  // We've just created the statement maybe there is a rewrite
    invoke_post_parse_rewrite_plugins(thd, true);
    error = init_param_array(this);
  }

  // Bind Sql command object with this prepared statement
  if (lex->m_sql_cmd) lex->m_sql_cmd->set_owner(this);

  lex->set_trg_event_type_for_tables();

  /*
    Pre-clear the diagnostics area unless a warning was thrown
    during parsing.
  */
  if (thd->lex->keep_diagnostics != DA_KEEP_PARSE_ERROR)
    thd->get_stmt_da()->reset_condition_info(thd);

  /*
    While doing context analysis of the query (in check_prepared_statement)
    we allocate a lot of additional memory: for open tables, JOINs, derived
    tables, etc.  Let's save a snapshot of current parse tree to the
    statement and restore original THD. In cases when some tree
    transformation can be reused on execute, we set again thd->mem_root from
    stmt->mem_root (see setup_wild for one place where we do that).
  */
  thd->restore_active_arena(this, &arena_backup);

  /*
    If called from a stored procedure, ensure that we won't rollback
    external changes when cleaning up after validation.
  */
  DBUG_ASSERT(thd->change_list.is_empty());

  /*
    Marker used to release metadata locks acquired while the prepared
    statement is being checked.
  */
  MDL_savepoint mdl_savepoint = thd->mdl_context.mdl_savepoint();

  // A subsystem, such as the Audit plugin, may have set error unnoticed:
  error |= thd->is_error();

  /*
   The only case where we should have items in the thd->free_list is
   after stmt->set_params_from_vars(), which may in some cases create
   Item_null objects.
  */

  if (error == 0) error = check_prepared_statement(this);
  DBUG_ASSERT(error || !thd->is_error());

  /*
    Currently CREATE PROCEDURE/TRIGGER/EVENT are prohibited in prepared
    statements: ensure we have no memory leak here if by someone tries
    to PREPARE stmt FROM "CREATE PROCEDURE ..."
  */
  DBUG_ASSERT(lex->sphead == NULL || error != 0);
  /* The order is important */
  lex->unit->cleanup(true);

  lex->clear_values_map();

  close_thread_tables(thd);
  thd->mdl_context.rollback_to_savepoint(mdl_savepoint);

  /*
    Transaction rollback was requested since MDL deadlock was discovered
    while trying to open tables. Rollback transaction in all storage
    engines including binary log and release all locks.

    Once dynamic SQL is allowed as substatements the below if-statement
    has to be adjusted to not do rollback in substatement.
  */
  DBUG_ASSERT(!thd->in_sub_stmt);
  if (thd->transaction_rollback_request) {
    trans_rollback_implicit(thd);
    thd->mdl_context.release_transactional_locks();
  }

  lex_end(lex);

  rewrite_query_if_needed(thd);

  if (thd->rewritten_query.length()) {
    MYSQL_SET_PS_TEXT(m_prepared_stmt, thd->rewritten_query.c_ptr_safe(),
                      thd->rewritten_query.length());
  } else {
    MYSQL_SET_PS_TEXT(m_prepared_stmt, thd->query().str, thd->query().length);
  }

  cleanup_stmt();
  stmt_backup.restore_thd(thd, this);
  thd->stmt_arena = old_stmt_arena;

  if (error == 0) {
    setup_set_params();
    lex->context_analysis_only &= ~CONTEXT_ANALYSIS_ONLY_PREPARE;
    state = Query_arena::STMT_PREPARED;
    flags &= ~(uint)IS_IN_USE;

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
    if (thd->sp_runtime_ctx == NULL) {
      if (thd->rewritten_query.length())
        query_logger.general_log_write(thd, COM_STMT_PREPARE,
                                       thd->rewritten_query.c_ptr_safe(),
                                       thd->rewritten_query.length());
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

  DBUG_RETURN(error);
}

/**
  Assign parameter values either from variables, in case of SQL PS
  or from the execute packet.

  @param expanded_query  a container with the original SQL statement.
                         '?' placeholders will be replaced with
                         their values in case of success.
                         The result is used for logging and replication
  @param has_new_types   flag used to signal that new types are provided.
  @param parameters      prepared statement's parsed parameters.

  @todo Use a paremeter source class family instead of 'if's, and
  support stored procedure variables.

  @return bool representing the function execution status.
  @retval true an error occurred when assigning a parameter (likely
          a conversion error or out of memory, or malformed packet)
  @retval false success
*/

bool Prepared_statement::set_parameters(String *expanded_query,
                                        bool has_new_types,
                                        PS_PARAM *parameters) {
  if (!param_count) return false;
  /*
    Setup conversion functions if new types are provided
    and insert parameters (types supplied / first execute)
  */
  if ((has_new_types && setup_conversion_functions(this, parameters)) ||
      insert_params(expanded_query, parameters)) {
    my_error(ER_WRONG_ARGUMENTS, MYF(0), "mysqld_stmt_execute");
    reset_stmt_params(this);
    return true;
  }
  return false;
}

bool Prepared_statement::set_parameters(String *expanded_query) {
  /* SQL prepared statement */
  if (insert_params_from_vars(thd->lex->prepared_stmt_params, expanded_query)) {
    my_error(ER_WRONG_ARGUMENTS, MYF(0), "EXECUTE");
    reset_stmt_params(this);
    return true;
  }
  return false;
}

/**
  Execute a prepared statement. Re-prepare it a limited number
  of times if necessary.

  Try to execute a prepared statement. If there is a metadata
  validation error, prepare a new copy of the prepared statement,
  swap the old and the new statements, and try again.
  If there is a validation error again, repeat the above, but
  perform no more than MAX_REPREPARE_ATTEMPTS.

  @note We have to try several times in a loop since we
  release metadata locks on tables after prepared statement
  prepare. Therefore, a DDL statement may sneak in between prepare
  and execute of a new statement. If this happens repeatedly
  more than MAX_REPREPARE_ATTEMPTS times, we give up.

  @param expanded_query   Query string.
  @param open_cursor      Flag to specift if a cursor should be used.

  @return  a bool value representing the function execution status.
  @retval  true    error: either MAX_REPREPARE_ATTEMPTS has been reached,
                   or some general error
  @retval  false   successfully executed the statement, perhaps
                   after having reprepared it a few times.
*/

bool Prepared_statement::execute_loop(String *expanded_query,
                                      bool open_cursor) {
  const int MAX_REPREPARE_ATTEMPTS = 3;
  Reprepare_observer reprepare_observer;
  bool error;
  int reprepare_attempt = 0;

  /* Check if we got an error when sending long data */
  if (state == Query_arena::STMT_ERROR) {
    my_message(last_errno, last_error, MYF(0));
    return true;
  }

  DBUG_ASSERT(!thd->get_stmt_da()->is_set());

  if (unlikely(!thd->security_context()->account_is_locked() &&
               thd->security_context()->password_expired() &&
               lex->sql_command != SQLCOM_SET_PASSWORD &&
               lex->sql_command != SQLCOM_ALTER_USER)) {
    my_error(ER_MUST_CHANGE_PASSWORD, MYF(0));
    return true;
  }

reexecute:
  /*
    If the free_list is not empty, we'll wrongly free some externally
    allocated items when cleaning up after validation of the prepared
    statement.
  */
  DBUG_ASSERT(thd->free_list == NULL);

  /*
    Install the metadata observer. If some metadata version is
    different from prepare time and an observer is installed,
    the observer method will be invoked to push an error into
    the error stack.
  */
  Reprepare_observer *stmt_reprepare_observer = NULL;

  if (sql_command_flags[lex->sql_command] & CF_REEXECUTION_FRAGILE) {
    reprepare_observer.reset_reprepare_observer();
    stmt_reprepare_observer = &reprepare_observer;
  }

  thd->push_reprepare_observer(stmt_reprepare_observer);

  error = execute(expanded_query, open_cursor) || thd->is_error();

  thd->pop_reprepare_observer();

  if ((sql_command_flags[lex->sql_command] & CF_REEXECUTION_FRAGILE) && error &&
      !thd->is_fatal_error && !thd->killed &&
      reprepare_observer.is_invalidated() &&
      reprepare_attempt++ < MAX_REPREPARE_ATTEMPTS) {
    DBUG_ASSERT(thd->get_stmt_da()->mysql_errno() == ER_NEED_REPREPARE);
    thd->clear_error();

    error = reprepare();

    if (!error) /* Success */
      goto reexecute;
  }
  reset_stmt_params(this);

  return error;
}

bool Prepared_statement::execute_server_runnable(
    Server_runnable *server_runnable) {
  Query_arena arena_backup;
  bool error;
  Query_arena *save_stmt_arena = thd->stmt_arena;
  Item_change_list save_change_list;
  thd->change_list.move_elements_to(&save_change_list);

  state = STMT_CONVENTIONAL_EXECUTION;

  if (!(lex = new (mem_root) st_lex_local)) return true;

  Statement_backup stmt_backup;
  stmt_backup.set_thd_to_ps(thd, this);
  stmt_backup.save_rlb(thd);
  thd->set_n_backup_active_arena(this, &arena_backup);
  thd->stmt_arena = this;

  error = server_runnable->execute_server_code(thd);

  thd->cleanup_after_query();

  thd->restore_active_arena(this, &arena_backup);
  stmt_backup.restore_thd(thd, this);
  stmt_backup.restore_rlb(thd);
  thd->stmt_arena = save_stmt_arena;

  save_change_list.move_elements_to(&thd->change_list);

  /* Items and memory will freed in destructor */

  return error;
}

/**
  Reprepare this prepared statement.

  Currently this is implemented by creating a new prepared
  statement, preparing it with the original query and then
  swapping the new statement and the original one.

  @retval  true   an error occurred. Possible errors include
                  incompatibility of new and old result set
                  metadata
  @retval  false  success, the statement has been reprepared
*/

bool Prepared_statement::reprepare() {
  char saved_cur_db_name_buf[NAME_LEN + 1];
  LEX_STRING saved_cur_db_name = {saved_cur_db_name_buf,
                                  sizeof(saved_cur_db_name_buf)};
  bool cur_db_changed;
  bool error;

  Prepared_statement copy(thd);

  copy.set_sql_prepare(); /* To suppress sending metadata to the client. */

  thd->status_var.com_stmt_reprepare++;

  if (mysql_opt_change_db(thd, m_db, &saved_cur_db_name, true, &cur_db_changed))
    return true;

  error = ((m_name.str && copy.set_name(m_name)) ||
           copy.prepare(m_query_string.str, m_query_string.length) ||
           validate_metadata(&copy));

  if (cur_db_changed)
    mysql_change_db(thd, to_lex_cstring(saved_cur_db_name), true);

  if (!error) {
    copy.m_prepared_stmt = m_prepared_stmt;
    /* Update reprepare count for this prepared statement in P_S table. */
    MYSQL_REPREPARE_PS(copy.m_prepared_stmt);

    swap_prepared_statement(&copy);
    swap_parameter_array(param_array, copy.param_array, param_count);
#ifndef DBUG_OFF
    is_reprepared = true;
#endif
    /*
      Clear possible warnings during reprepare, it has to be completely
      transparent to the user. We use clear_warning_info() since
      there were no separate query id issued for re-prepare.
      Sic: we can't simply silence warnings during reprepare, because if
      it's failed, we need to return all the warnings to the user.
    */
    thd->get_stmt_da()->reset_condition_info(thd);
  }
  return error;
}

/**
  Validate statement result set metadata (if the statement returns
  a result set).

  Currently we only check that the number of columns of the result
  set did not change.
  This is a helper method used during re-prepare.

  @param[in]  copy  the re-prepared prepared statement to verify
                    the metadata of

  @retval true  error, ER_PS_REBIND is reported
  @retval false statement return no or compatible metadata
*/

bool Prepared_statement::validate_metadata(Prepared_statement *copy) {
  /**
    If this is an SQL prepared statement or EXPLAIN,
    return false -- the metadata of the original SELECT,
    if any, has not been sent to the client.
  */
  if (is_sql_prepare() || lex->is_explain()) return false;

  if (lex->select_lex->item_list.elements !=
      copy->lex->select_lex->item_list.elements) {
    /** Column counts mismatch, update the client */
    thd->server_status |= SERVER_STATUS_METADATA_CHANGED;
  }

  return false;
}

/**
  Replace the original prepared statement with a prepared copy.

  This is a private helper that is used as part of statement
  reprepare.
*/

void Prepared_statement::swap_prepared_statement(Prepared_statement *copy) {
  Query_arena tmp_arena;

  /* Swap memory roots. */
  std::swap(main_mem_root, copy->main_mem_root);

  /* Swap the arenas */
  tmp_arena.set_query_arena(this);
  set_query_arena(copy);
  copy->set_query_arena(&tmp_arena);

  /* Swap the statement attributes */
  std::swap(lex, copy->lex);
  std::swap(m_query_string, copy->m_query_string);

  /* Swap mem_roots back, they must continue pointing at the main_mem_roots */
  std::swap(mem_root, copy->mem_root);
  /*
    Swap the old and the new parameters array. The old array
    is allocated in the old arena.
  */
  std::swap(param_array, copy->param_array);
  /* Don't swap flags: the copy has IS_SQL_PREPARE always set. */
  /* std::swap(flags, copy->flags); */
  /* Swap names, the old name is allocated in the wrong memory root */
  std::swap(m_name, copy->m_name);
  /* Ditto */
  std::swap(m_db, copy->m_db);

  // The call to copy.prepare() will have set the copy as the owner of
  // the Sql_cmd object, if there is one. Set it back to this.
  if (lex->m_sql_cmd != nullptr) {
    DBUG_ASSERT(lex->m_sql_cmd->get_owner() == copy);
    lex->m_sql_cmd->set_owner(this);
  }

  DBUG_ASSERT(param_count == copy->param_count);
  DBUG_ASSERT(thd == copy->thd);
  last_error[0] = '\0';
  last_errno = 0;
}

/**
  Execute a prepared statement.

    You should not change global THD state in this function, if at all
    possible: it may be called from any context, e.g. when executing
    a COM_* command, and SQLCOM_* command, or a stored procedure.

  @param expanded_query     A query for binlogging which has all parameter
                            markers ('?') replaced with their actual values.
  @param open_cursor        True if an attempt to open a cursor should be made.
                            Currenlty used only in the binary protocol.

  @note
    Preconditions, postconditions.
    - See the comment for Prepared_statement::prepare().

  @retval
    false	    ok
  @retval
    true		Error
*/

bool Prepared_statement::execute(String *expanded_query, bool open_cursor) {
  Query_arena *old_stmt_arena;
  char saved_cur_db_name_buf[NAME_LEN + 1];
  LEX_STRING saved_cur_db_name = {saved_cur_db_name_buf,
                                  sizeof(saved_cur_db_name_buf)};
  bool cur_db_changed;

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
  */
  thd->get_stmt_da()->reset_condition_info(thd);

  if (flags & (uint)IS_IN_USE) {
    my_error(ER_PS_NO_RECURSION, MYF(0));
    return true;
  }

  /*
    For SHOW VARIABLES lex->result is NULL, as it's a non-SELECT
    command. For such queries we don't return an error and don't
    open a cursor -- the client library will recognize this case and
    materialize the result set.
    For SELECT statements lex->result is created in
    check_prepared_statement. lex->result->simple_select() is false
    in INSERT ... SELECT and similar commands.
  */

  if (open_cursor && lex->result && lex->result->check_simple_select()) {
    DBUG_PRINT("info", ("Cursor asked for not SELECT stmt"));
    return true;
  }

  /* In case the command has a call to SP which re-uses this statement name */
  flags |= IS_IN_USE;

  close_cursor();

  /*
    If the free_list is not empty, we'll wrongly free some externally
    allocated items when cleaning up after execution of this statement.
  */
  DBUG_ASSERT(thd->change_list.is_empty());

  /*
   The only case where we should have items in the thd->free_list is
   after stmt->set_params_from_vars(), which may in some cases create
   Item_null objects.
  */

  Statement_backup stmt_backup;
  stmt_backup.set_thd_to_ps(thd, this);
  stmt_backup.save_rlb(thd);

  /*
    Change the current database (if needed).

    Force switching, because the database of the prepared statement may be
    NULL (prepared statements can be created while no current database
    selected).
  */
  if (mysql_opt_change_db(thd, m_db, &saved_cur_db_name, true,
                          &cur_db_changed)) {
    flags &= ~(uint)IS_IN_USE;
    stmt_backup.restore_thd(thd, this);
    stmt_backup.restore_rlb(thd);
    return true;
  }

  /* Allocate query. */

  if (expanded_query->length() &&
      alloc_query(thd, (char *)expanded_query->ptr(),
                  expanded_query->length())) {
    my_error(ER_OUTOFMEMORY, MYF(ME_FATALERROR), expanded_query->length());
    flags &= ~(uint)IS_IN_USE;
    stmt_backup.restore_thd(thd, this);
    stmt_backup.restore_rlb(thd);
    return true;
  }

  /*
    At first execution of prepared statement we may perform logical
    transformations of the query tree. Such changes should be performed
    on the parse tree of current prepared statement and new items should
    be allocated in its memory root. Set the appropriate pointer in THD
    to the arena of the statement.
  */
  old_stmt_arena = thd->stmt_arena;
  thd->stmt_arena = this;
  bool error = reinit_stmt_before_use(thd, lex);

  /*
    Set a hint so mysql_execute_command() won't clear the DA *again*,
    thereby discarding any conditions we might raise in here
    (e.g. "database we prepared with no longer exists", ER_BAD_DB_ERROR).
  */
  thd->lex->keep_diagnostics = DA_KEEP_PARSE_ERROR;

  if (!error) {
    // Execute
    if (open_cursor) {
      lex->safe_to_cache_query = 0;
      // Initialize Query_result_send before opening the cursor
      if (thd->is_classic_protocol())
        result = new (mem_root) Query_fetch_protocol_binary(thd);
      else
        result = new (mem_root) Query_result_send(thd);
      if (!result) {
        error = true;  // OOM
      } else if ((error = mysql_open_cursor(thd, result, &cursor))) {
        // cursor is freed inside mysql_open_cursor
        destroy(result);
        result = nullptr;
      }
    } else {
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
          lex->binlog_need_explicit_defaults_ts;
      resourcegroups::Resource_group *src_res_grp = nullptr;
      resourcegroups::Resource_group *dest_res_grp = nullptr;
      MDL_ticket *ticket = nullptr;
      MDL_ticket *cur_ticket = nullptr;
      auto mgr_ptr = resourcegroups::Resource_group_mgr::instance();
      bool switched = mgr_ptr->switch_resource_group_if_needed(
          thd, &src_res_grp, &dest_res_grp, &ticket, &cur_ticket);

      error = mysql_execute_command(thd, true);

      if (switched)
        mgr_ptr->restore_original_resource_group(thd, src_res_grp,
                                                 dest_res_grp);
      thd->resource_group_ctx()->m_switch_resource_group_str[0] = '\0';
      if (ticket != nullptr)
        mgr_ptr->release_shared_mdl_for_resource_group(thd, ticket);
      if (cur_ticket != nullptr)
        mgr_ptr->release_shared_mdl_for_resource_group(thd, cur_ticket);
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

  // Assert that if an error, the cursor and the result are deallocated.
  DBUG_ASSERT(!error || (cursor == nullptr && result == nullptr));

  if (!cursor) cleanup_stmt();

  /*
   Note that we cannot call restore_thd() here as that would overwrite
   the expanded query in THD::m_query_string, which is needed for is
   needed for slow logging. Use alloc_query() to make sure the query
   is allocated on the correct MEM_ROOT, since otherwise
   THD::m_query_string could end up as a dangling pointer
   (i.e. pointer to freed memory) once the PS MEM_ROOT is freed.
  */
  mysql_mutex_lock(&thd->LOCK_thd_data);
  thd->lex = stmt_backup.lex();
  mysql_mutex_unlock(&thd->LOCK_thd_data);
  alloc_query(thd, thd->query().str, thd->query().length);

  thd->stmt_arena = old_stmt_arena;

  /* Restore the original rewritten query. */
  stmt_backup.restore_rlb(thd);

  if (state == Query_arena::STMT_PREPARED) state = Query_arena::STMT_EXECUTED;

  if (error == 0 && this->lex->sql_command == SQLCOM_CALL)
    thd->get_protocol()->send_parameters(&this->lex->param_list,
                                         is_sql_prepare());
  flags &= ~(uint)IS_IN_USE;
  return error;
}

/** Common part of DEALLOCATE PREPARE and mysqld_stmt_close. */

void Prepared_statement::deallocate() {
  /* We account deallocate in the same manner as mysqld_stmt_close */
  thd->status_var.com_stmt_close++;
  /* Statement map calls delete stmt on erase */
  thd->stmt_map.erase(this);
}

/***************************************************************************
 * Ed_result_set
 ***************************************************************************/
/**
  Use operator delete to free memory of Ed_result_set.
  Accessing members of a class after the class has been destroyed
  is a violation of the C++ standard but is commonly used in the
  server code.
*/

void Ed_result_set::operator delete(void *ptr, size_t) throw() {
  if (ptr) {
    /*
      Move into the stack, otherwise free_root() will attempt to
      write to freed memory.
    */
    MEM_ROOT own_root = std::move(((Ed_result_set *)ptr)->m_mem_root);
    free_root(&own_root, MYF(0));
  }
}

/**
  Initialize an instance of Ed_result_set.

  Instances of the class, as well as all result set rows, are
  always allocated in the memory root passed over as the second
  argument. In the constructor, we take over ownership of the
  memory root. It will be freed when the class is destroyed.

  sic: Ed_result_est is not designed to be allocated on stack.
*/

Ed_result_set::Ed_result_set(List<Ed_row> *rows_arg, size_t column_count_arg,
                             MEM_ROOT *mem_root_arg)
    : m_mem_root(std::move(*mem_root_arg)),
      m_column_count(column_count_arg),
      m_rows(rows_arg),
      m_next_rset(NULL) {}

/***************************************************************************
 * Ed_result_set
 ***************************************************************************/

/**
  Create a new "execute direct" connection.
*/

Ed_connection::Ed_connection(THD *thd)
    : m_diagnostics_area(false), m_thd(thd), m_rsets(0), m_current_rset(0) {}

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
  DBUG_ENTER("Ed_connection::execute_direct");

  free_old_result(); /* Delete all data from previous execution, if any */

  Protocol_local protocol_local(m_thd, this);
  m_thd->push_protocol(&protocol_local);
  m_thd->push_diagnostics_area(&m_diagnostics_area);

  Prepared_statement stmt(m_thd);
  bool rc = stmt.execute_server_runnable(server_runnable);
  m_thd->send_statement_status();

  m_thd->pop_protocol();
  m_thd->pop_diagnostics_area();
  /*
    Protocol_local makes use of m_current_rset to keep
    track of the last result set, while adding result sets to the end.
    Reset it to point to the first result set instead.
  */
  m_current_rset = m_rsets;

  DBUG_RETURN(rc);
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
      m_rset(NULL),
      m_column_count(0),
      m_current_row(NULL),
      m_current_column(NULL),
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
  if (m_current_column == NULL)
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
  if (m_current_column == NULL)
    return true; /* start_row() failed to allocate memory. */
  /*
    alloc_root() automatically aligns memory, so we don't need to
    do any extra alignment if we're pointing to, say, an integer.
  */
  m_current_column->str =
      (char *)memdup_root(&m_rset_root, data, length + 1 /* Safety */);
  if (!m_current_column->str) return true;
  m_current_column->str[length] = '\0'; /* Safety */
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
  uint error_unused;

  if (dst_cs && !my_charset_same(src_cs, dst_cs) && src_cs != &my_charset_bin &&
      dst_cs != &my_charset_bin) {
    if (convert->copy(str, length, src_cs, dst_cs, &error_unused)) return true;
    str = convert->ptr();
    length = convert->length();
  }
  return store_column(str, length);
}

/** Store a tiny int as is (1 byte) in a result set column. */

bool Protocol_local::store_tiny(longlong value) {
  char v = (char)value;
  return store_column(&v, 1);
}

/** Store a short as is (2 bytes, host order) in a result set column. */

bool Protocol_local::store_short(longlong value) {
  int16 v = (int16)value;
  return store_column(&v, 2);
}

/** Store a "long" as is (4 bytes, host order) in a result set column.  */

bool Protocol_local::store_long(longlong value) {
  int32 v = (int32)value;
  return store_column(&v, 4);
}

/** Store a "longlong" as is (8 bytes, host order) in a result set column. */

bool Protocol_local::store_longlong(longlong value, bool) {
  int64 v = (int64)value;
  return store_column(&v, 8);
}

/** Store a decimal in string format in a result set column */

bool Protocol_local::store_decimal(const my_decimal *value, uint prec,
                                   uint dec) {
  char buf[DECIMAL_MAX_STR_LENGTH];
  String str(buf, sizeof(buf), &my_charset_bin);
  int rc;

  rc = my_decimal2string(E_DEC_FATAL_ERROR, value, prec, dec, '0', &str);

  if (rc) return true;

  return store_column(str.ptr(), str.length());
}

/** Convert to cs_results and store a string. */

bool Protocol_local::store(const char *str, size_t length,
                           const CHARSET_INFO *src_cs) {
  const CHARSET_INFO *dst_cs;

  dst_cs = m_connection->m_thd->variables.character_set_results;
  return store_string(str, length, src_cs, dst_cs);
}

/** Store a string. */

bool Protocol_local::store(const char *str, size_t length,
                           const CHARSET_INFO *src_cs,
                           const CHARSET_INFO *dst_cs) {
  return store_string(str, length, src_cs, dst_cs);
}

/* Store MYSQL_TIME (in binary format) */

bool Protocol_local::store(MYSQL_TIME *time,
                           uint precision MY_ATTRIBUTE((unused))) {
  return store_column(time, sizeof(MYSQL_TIME));
}

/** Store MYSQL_TIME (in binary format) */

bool Protocol_local::store_date(MYSQL_TIME *time) {
  return store_column(time, sizeof(MYSQL_TIME));
}

/** Store MYSQL_TIME (in binary format) */

bool Protocol_local::store_time(MYSQL_TIME *time,
                                uint precision MY_ATTRIBUTE((unused))) {
  return store_column(time, sizeof(MYSQL_TIME));
}

/* Store a floating point number, as is. */

bool Protocol_local::store(float value, uint32, String *) {
  return store_column(&value, sizeof(float));
}

/* Store a double precision number, as is. */

bool Protocol_local::store(double value, uint32, String *) {
  return store_column(&value, sizeof(double));
}

/* Store a Field. */

bool Protocol_local::store(Proto_field *field) {
  return field->send_binary(this);
}

/** Called for statements that don't have a result set, at statement end. */

bool Protocol_local::send_ok(uint, uint, ulonglong, ulonglong, const char *) {
  /*
    Just make sure nothing is sent to the client, we have grabbed
    the status information in the connection Diagnostics Area.
  */
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

  DBUG_ASSERT(m_rset);

  opt_add_row_to_rset();
  m_current_row = 0;

  ed_result_set =
      new (&m_rset_root) Ed_result_set(m_rset, m_column_count, &m_rset_root);

  m_rset = NULL;

  if (!ed_result_set) return true;

  /* In case of successful allocation memory ownership was transferred. */
  DBUG_ASSERT(!alloc_root_inited(&m_rset_root));

  /*
    Link the created Ed_result_set instance into the list of connection
    result sets. Never fails.
  */
  m_connection->add_result_set(ed_result_set);
  return false;
}

/** Called to send an error to the client at the end of a statement. */

bool Protocol_local::send_error(uint, const char *, const char *) {
  /*
    Just make sure that nothing is sent to the client (default
    implementation).
  */
  return false;
}

int Protocol_local::read_packet() { return 0; }

ulong Protocol_local::get_client_capabilities() { return 0; }

bool Protocol_local::has_client_capability(unsigned long) { return false; }

bool Protocol_local::connection_alive() { return false; }

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
  DBUG_ENTER("Protocol_local::start_row");
  DBUG_ASSERT(alloc_root_inited(&m_rset_root));

  opt_add_row_to_rset();
  /* Start a new row. */
  m_current_row =
      (Ed_column *)alloc_root(&m_rset_root, sizeof(Ed_column) * m_column_count);
  m_current_column = m_current_row;
  DBUG_VOID_RETURN;
}

/**
In "real" protocols this is called to finish a result set row.
Unused in the local implementation.
*/
bool Protocol_local::end_row() {
  DBUG_ENTER("Protocol_local::end_row");
  DBUG_RETURN(false);
}

uint Protocol_local::get_rw_status() { return 0; }

bool Protocol_local::start_result_metadata(uint, uint, const CHARSET_INFO *) {
  return 0;
}

bool Protocol_local::end_result_metadata() { return false; }

bool Protocol_local::send_field_metadata(Send_field *, const CHARSET_INFO *) {
  return false;
}

bool Protocol_local::get_compression() { return false; }

int Protocol_local::get_command(COM_DATA *, enum_server_command *) {
  return -1;
}
