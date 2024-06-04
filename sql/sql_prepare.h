#ifndef SQL_PREPARE_H
#define SQL_PREPARE_H
/* Copyright (c) 2009, 2024, Oracle and/or its affiliates.

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
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <assert.h>
#include <stddef.h>
#include <sys/types.h>

#include "lex_string.h"
#include "my_alloc.h"
#include "my_command.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "my_psi_config.h"
#include "mysql/components/services/bits/psi_statement_bits.h"
#include "mysql_com.h"
#include "sql/sql_class.h"  // Query_arena

class Item_param;
class Prepared_statement;
class Protocol;
class Query_result;
class Server_runnable;
class String;
struct LEX;
struct PS_PARAM;
template <class T>
class List;
union COM_DATA;

/**
  An interface that is used to take an action when
  the locking module notices that a table version has changed
  since the last execution. "Table" here may refer to any kind of
  table -- a base table, a temporary table, a view or an
  information schema table.

  When we open and lock tables for execution of a prepared
  statement, we must verify that they did not change
  since statement prepare. If some table did change, the statement
  parse tree *may* be no longer valid, e.g. in case it contains
  optimizations that depend on table metadata.

  This class provides an interface (a method) that is
  invoked when such a situation takes place.
  The implementation of the method simply reports an error, but
  the exact details depend on the nature of the SQL statement.

  At most 1 instance of this class is active at a time, in which
  case THD::m_reprepare_observer is not NULL.

  @sa check_and_update_table_version() for details of the
  version tracking algorithm

  @sa Open_tables_state::m_reprepare_observer for the life cycle
  of metadata observers.
*/

class Reprepare_observer final {
 public:
  /**
    Check if a change of metadata is OK. In future
    the signature of this method may be extended to accept the old
    and the new versions, but since currently the check is very
    simple, we only need the THD to report an error.
  */
  bool report_error(THD *thd);
  /**
    @returns true if some table metadata is changed and statement should be
                  re-prepared.
  */
  bool is_invalidated() const { return m_invalidated; }
  void reset_reprepare_observer() { m_invalidated = false; }
  /// @returns true if prepared statement can (and will) be retried
  bool can_retry() const {
    // Only call for a statement that is invalidated
    assert(is_invalidated());
    return m_attempt <= MAX_REPREPARE_ATTEMPTS &&
           DBUG_EVALUATE_IF("simulate_max_reprepare_attempts_hit_case", false,
                            true);
  }

 private:
  bool m_invalidated{false};
  int m_attempt{0};

  /*
    We take only 3 attempts to reprepare the query, otherwise we might end up
    in endless loop.
  */
  static constexpr int MAX_REPREPARE_ATTEMPTS = 3;
};

void rewrite_query(THD *thd);
void log_execute_line(THD *thd);

bool ask_to_reprepare(THD *thd);
bool mysql_stmt_precheck(THD *thd, const COM_DATA *com_data,
                         enum enum_server_command cmd,
                         Prepared_statement **stmt);
void mysqld_stmt_prepare(THD *thd, const char *query, uint length,
                         Prepared_statement *stmt);
void mysqld_stmt_execute(THD *thd, Prepared_statement *stmt, bool has_new_types,
                         ulong execute_flags, PS_PARAM *parameters);
void mysqld_stmt_close(THD *thd, Prepared_statement *stmt);
void mysql_sql_stmt_prepare(THD *thd);
void mysql_sql_stmt_execute(THD *thd);
void mysql_sql_stmt_close(THD *thd);
void mysqld_stmt_fetch(THD *thd, Prepared_statement *stmt, ulong num_rows);
void mysqld_stmt_reset(THD *thd, Prepared_statement *stmt);
void mysql_stmt_get_longdata(THD *thd, Prepared_statement *stmt,
                             uint param_number, uchar *longdata, ulong length);
bool select_like_stmt_cmd_test(THD *thd, class Sql_cmd_dml *cmd,
                               ulong setup_tables_done_option);
void reset_stmt_parameters(Prepared_statement *stmt);

/**
  Execute direct interface.

  @todo Implement support for prelocked mode.
*/

class Server_side_cursor;

/**
  Prepared_statement: a statement that can contain placeholders.
*/

class Prepared_statement final {
 public:
  /// Memory allocation arena, for permanent allocations to statement.
  Query_arena m_arena;

  /// Array of parameters used for statement, may be NULL if there are none.
  Item_param **m_param_array{nullptr};

  /// Pointer to cursor, may be NULL if statement never used with a cursor.
  Server_side_cursor *m_cursor{nullptr};

  /// Used to check that the protocol is stable during execution
  const Protocol *m_active_protocol{nullptr};

  /// Number of parameters expected for statement
  uint m_param_count{0};

  uint m_last_errno{0};
  char m_last_error[MYSQL_ERRMSG_SIZE];

  /**
    Uniquely identifies each statement object in thread scope; change during
    statement lifetime.
  */
  const ulong m_id;

  LEX *m_lex{nullptr};  // parse tree descriptor

  /// The query string associated with this statement.
  LEX_CSTRING m_query_string{NULL_CSTR};

  /// Performance Schema interface for a prepared statement.
  PSI_prepared_stmt *m_prepared_stmt{nullptr};

 private:
  /// True if statement is used with cursor, false if used in regular execution
  bool m_used_as_cursor{false};

  /// Query result used when statement is used in regular execution.
  Query_result *m_regular_result{nullptr};

  /// Query result used when statement is used with a cursor.
  Query_result *m_cursor_result{nullptr};

  /// Auxiliary query result object, saved for proper destruction
  Query_result *m_aux_result{nullptr};

  /// Flag that specifies preparation state
  bool m_is_sql_prepare{false};

  /// Flag that prevents recursive invocation of prepared statements
  bool m_in_use{false};

  bool m_with_log{false};

  bool m_first_execution{true};

  /// Name of the prepared statement.
  LEX_CSTRING m_name{NULL_CSTR};
  /**
    Name of the current (default) database.

    If there is the current (default) database, "db" contains its name. If
    there is no current (default) database, "db" is NULL and "db_length" is
    0. In other words, "db", "db_length" must either be NULL, or contain a
    valid database name.

    @note this attribute is set and allocated by the slave SQL thread (for
    the THD of that thread); that thread is (and must remain, for now) the
    only responsible for freeing this member.
  */
  LEX_CSTRING m_db{NULL_CSTR};

  /**
    The memory root to allocate parsed tree elements (instances of Item,
    Query_block and other classes).
  */
  MEM_ROOT m_mem_root;

  bool prepare_query(THD *thd);

 public:
  Prepared_statement(THD *thd_arg);
  virtual ~Prepared_statement();

  bool set_name(const LEX_CSTRING &name);
  const LEX_CSTRING &name() const { return m_name; }
  ulong id() const { return m_id; }
  bool is_in_use() const { return m_in_use; }
  bool is_sql_prepare() const { return m_is_sql_prepare; }
  void set_sql_prepare(bool prepare = true) { m_is_sql_prepare = prepare; }
  void deallocate(THD *thd);
  bool prepare(THD *thd, const char *packet, size_t packet_length,
               Item_param **orig_param_array);
  bool execute_loop(THD *thd, String *expanded_query, bool open_cursor);
  bool execute_server_runnable(THD *thd, Server_runnable *server_runnable);
#ifdef HAVE_PSI_PS_INTERFACE
  PSI_prepared_stmt *get_PS_prepared_stmt() { return m_prepared_stmt; }
#endif

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
  bool set_parameters(THD *thd, String *expanded_query, bool has_new_types,
                      PS_PARAM *parameters);

  enum enum_param_pack_type {
    /*
     Parameter values are coming from client over network (vio).
     */
    PACKED,

    /*
     Parameter values are sent either using using Server_component service
     API's or Plugins or directly from SQL layer modules.

     UNPACKED means that the parameter value buffer points to MYSQL_TIME*
     */
    UNPACKED
  };

  /**
    Assign parameter values from the execute packet.

    @param thd             current thread
    @param expanded_query  a container with the original SQL statement.
                           '?' placeholders will be replaced with
                           their values in case of success.
                           The result is used for logging and replication
    @param has_new_types   flag used to signal that new types are provided.
    @param parameters      prepared statement's parsed parameters.
    @param param_pack_type parameters pack type.

    @returns false if success, true if error
             (likely a conversion error, out of memory, or malformed packet)
  */
  bool set_parameters(THD *thd, String *expanded_query, bool has_new_types,
                      PS_PARAM *parameters,
                      enum enum_param_pack_type param_pack_type);

  bool set_parameters(THD *thd, String *expanded_query);
  void trace_parameter_types(THD *thd);
  void close_cursor();

 private:
  void cleanup_stmt(THD *thd);
  void setup_stmt_logging(THD *thd);
  bool check_parameter_types();
  void copy_parameter_types(Item_param **from_param_array);
  bool set_db(const LEX_CSTRING &db_length);

  bool execute(THD *thd, String *expanded_query, bool open_cursor);
  bool reprepare(THD *thd);
  bool validate_metadata(THD *thd, Prepared_statement *copy);
  void swap_prepared_statement(Prepared_statement *copy);
  bool insert_parameters_from_vars(THD *thd, List<LEX_STRING> &varnames,
                                   String *query);
  bool insert_parameters(THD *thd, String *query, bool has_new_types,
                         PS_PARAM *parameters,
                         enum enum_param_pack_type param_pack_type);
};

#endif  // SQL_PREPARE_H
