/* Copyright (c) 2005, 2023, Oracle and/or its affiliates.

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

#include "sql/sql_cursor.h"

#include <assert.h>
#include <sys/types.h>

#include <algorithm>
#include <utility>  // move

#include "memory_debugging.h"
#include "my_alloc.h"
#include "my_base.h"
#include "my_compiler.h"

#include "my_inttypes.h"
#include "mysql/components/services/bits/psi_statement_bits.h"
#include "mysql_com.h"
#include "sql/debug_sync.h"
#include "sql/field.h"
#include "sql/handler.h"
#include "sql/item.h"
#include "sql/parse_tree_node_base.h"
#include "sql/protocol.h"
#include "sql/query_options.h"
#include "sql/query_result.h"
#include "sql/sql_cmd_dml.h"  // Sql_cmd_dml
#include "sql/sql_digest_stream.h"
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_parse.h"      // mysql_execute_command
#include "sql/sql_tmp_table.h"  // tmp tables
#include "sql/sql_union.h"      // Query_result_union
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql/thd_raii.h"  // Prepared_stmt_arena_holder

/****************************************************************************
  Declarations.
****************************************************************************/

/**
  Materialized_cursor -- an insensitive materialized server-side
  cursor. The result set of this cursor is saved in a temporary
  table at open. The cursor itself is simply an interface for the
  handler of the temporary table.

  The materialized cursor is usually attached to a preparable statement
  through a query result object. The lifetime of the cursor is the same
  as the lifetime of the preparable statement. When the preparable statement
  is destroyed, the materialized cursor (including the temporary table) is
  also destroyed.
 */

class Materialized_cursor final : public Server_side_cursor {
  /// A fake unit to supply to Query_result_send when fetching
  Query_expression fake_query_expression;
  /// Cursor to the table that contains the materialized result
  TABLE *m_table{nullptr};
  /**
    List of items to send to client, copy of original items, but created in
    the cursor object's mem_root.
  */
  mem_root_deque<Item *> item_list;
  ulong fetch_limit{0};
  ulong fetch_count{0};
  bool is_rnd_inited{false};

 public:
  Materialized_cursor(Query_result *result);
  void set_table(TABLE *table_arg);
  void set_result(Query_result *result_arg) { m_result = result_arg; }
  int send_result_set_metadata(
      THD *thd, const mem_root_deque<Item *> &send_result_set_metadata);
  bool is_open() const override { return m_table->has_storage_handler(); }
  bool open(THD *) override;
  bool fetch(ulong num_rows) override;
  void close() override;
  ~Materialized_cursor() override;
};

/**
  Query_result_materialize -- a mediator between a cursor query and the
  protocol. In case we were not able to open a non-materialzed
  cursor, it creates an internal temporary memory table, and inserts
  all rows into it. If the table is in the Heap engine and if it reaches
  maximum Heap table size, it's converted to a disk-based temporary
  table. Later this table is used to create a Materialized_cursor.
*/

class Query_result_materialize final : public Query_result_union {
 public:
  Query_result_materialize(Query_result *result_arg)
      : Query_result_union(), m_result(result_arg) {}
  ~Query_result_materialize() override { destroy(m_cursor); }
  void set_result(Query_result *result_arg) {
    m_result = result_arg;
    if (m_cursor != nullptr) {
      m_cursor->set_result(result_arg);
    }
  }
  bool check_supports_cursor() const override { return false; }
  bool prepare(THD *thd, const mem_root_deque<Item *> &list,
               Query_expression *u) override;
  bool start_execution(THD *thd) override;
  bool send_result_set_metadata(THD *thd, const mem_root_deque<Item *> &list,
                                uint flags) override;
  void cleanup() override { m_result->cleanup(); }
  Server_side_cursor *cursor() const override { return m_cursor; }

 private:
  /// The materialized cursor associated with this result
  Materialized_cursor *m_cursor{nullptr};
  /// The query result supplied by the caller (PS or SP)
  Query_result *m_result;
};

Query_result *new_cursor_result(MEM_ROOT *mem_root, Query_result *result) {
  return new (mem_root) Query_result_materialize(result);
}

/**************************************************************************/

/**
  Attempt to open a materialized cursor.

  @param      thd           thread handle
  @param[in]  result        result class of the caller used as a destination
                            for the rows fetched from the cursor
  @param[in,out] pcursor    a pointer to store a pointer to cursor in.
                            The cursor is usually created on first call.
                            Notice that a cursor may be returned even though
                            execution causes an error. Cursor is open
                            when execution is successful, closed otherwise.

  @return Error status

  @returns false on success, true on error

  @note
  Only used for cursors created by stored procedures. Cursors created
  for prepared statements are handled by simpler interfaces
  (new_cursor_result(), Materialized_cursor::open(), etc).
  On first invocation, mysql_open_cursor creates a query result object
  for management of the materialized result. When this cursor is prepared,
  it creates a materialized cursor object (Materialized_cursor) inside
  the cursor. In addition, an application specific result object supplied
  as argument is attached to the query result object.
  The query result object is also attached to the current prepared statement.
  A reference to the cursor object is returned in pcursor.
  The statement may or may not be prepared on first invocation,
  it is prepared if necessary.

  On subsequent invocations, the query result object is located inside
  the preparable statement and the cursor object is located inside this.
  A reference to the cursor object is returned in pcursor.

  On all invocations, the statement is executed and a temporary table managed
  by the cursor object is populated with the result set.
*/

bool mysql_open_cursor(THD *thd, Query_result *result,
                       Server_side_cursor **pcursor) {
  sql_digest_state *parent_digest;
  PSI_statement_locker *parent_locker;
  Query_result_materialize *result_materialize = nullptr;
  LEX *lex = thd->lex;

  Sql_cmd_dml *sql_cmd = lex->m_sql_cmd != nullptr && lex->m_sql_cmd->is_dml()
                             ? down_cast<Sql_cmd_dml *>(lex->m_sql_cmd)
                             : nullptr;

  // Only DML statements may have assigned a cursor.
  if (sql_cmd == nullptr) {
    my_error(ER_WRONG_ARGUMENTS, MYF(0), "with cursor");
    return true;
  }
  /*
    Cursors are not supported for regular (non-prepared, non-SP) statements,
    and the statement must return data (usually a SELECT statement).
  */
  assert(sql_cmd->may_use_cursor() && !sql_cmd->is_regular());

  /*
    Create the result object for materialization.
    Two situations are possible here:
    1. If this is a preparable un-prepared statement, create object in
       statement mem_root.
    2. If this is a prepared statement for which a result object for
       materialization exists, reuse this object.
  */
  if (!sql_cmd->is_prepared()) {
    Prepared_stmt_arena_holder ps_arena_holder(thd);

    result_materialize = new (thd->mem_root) Query_result_materialize(result);
    if (result_materialize == nullptr) return true;
  } else {
    result_materialize =
        down_cast<Query_result_materialize *>(sql_cmd->query_result());
    assert(sql_cmd->query_result() == result_materialize);
    result_materialize->set_result(result);
  }

  // Pass the Query_result_materialize object to the query
  lex->result = result_materialize;

  parent_digest = thd->m_digest;
  parent_locker = thd->m_statement_psi;
  thd->m_digest = nullptr;
  thd->m_statement_psi = nullptr;
  DBUG_EXECUTE_IF("bug33218625_kill_injection", thd->killed = THD::KILL_QUERY;);

  bool rc = mysql_execute_command(thd);

  thd->m_digest = parent_digest;
  DEBUG_SYNC(thd, "after_table_close");
  thd->m_statement_psi = parent_locker;

  // Get the cursor that was created for materialization
  Server_side_cursor *cursor =
      result_materialize != nullptr ? result_materialize->cursor() : nullptr;

  if (*pcursor == nullptr) *pcursor = cursor;

  if (rc) {
    /*
      Execution ended in error. Notice that a cursor may have been
      created, in this case metadata in client-server protocol is rolled
      back and the cursor is closed (if it is open).
    */
    if (cursor != nullptr) {
      result_materialize->abort_result_set(thd);
      cursor->close();
    }
    return true;
  }

  /*
    Execution was successful. For most queries, a cursor has been created
    and must be opened, however for some queries, no cursor is used.
    This is possible if some command writes directly to the
    network, bypassing Query_result mechanism. An example of
    such command is SHOW PRIVILEGES.
  */
  if (cursor != nullptr) {
    /*
      NOTE: close_thread_tables() has been called in
      mysql_execute_command(), so all tables except from the cursor
      temporary table have been closed.
    */
    if (cursor->open(thd)) {
      return true;
    }
  }

  return false;
}

/***************************************************************************
 Materialized_cursor
****************************************************************************/

Materialized_cursor::Materialized_cursor(Query_result *result_arg)
    : Server_side_cursor(result_arg),
      fake_query_expression(CTX_NONE),
      item_list(*THR_MALLOC) {}

/// Bind a temporary table with a materialized cursor.
void Materialized_cursor::set_table(TABLE *table_arg) { m_table = table_arg; }

/**
  Preserve the original metadata to be sent to the client.
  Initiate sending of the original metadata to the client
  (call Protocol::send_result_set_metadata()).

  @param thd Thread identifier.
  @param send_result_set_metadata List of fields that would be sent.
*/

int Materialized_cursor::send_result_set_metadata(
    THD *thd, const mem_root_deque<Item *> &send_result_set_metadata) {
  /*
    Create objects in the mem_root of the cursor. The item list will be
    referenced after the execution of the current statement, so it cannot
    created on the execution mem_root.
  */
  Query_arena backup_arena;
  thd->swap_query_arena(m_arena, &backup_arena);
  if (item_list.empty()) {
    if (m_table->fill_item_list(&item_list)) {
      thd->swap_query_arena(backup_arena, &m_arena);
      return true;
    }

    assert(CountVisibleFields(send_result_set_metadata) == item_list.size());

    /*
      Unless we preserve the original metadata, it will be lost,
      since new fields describe columns of the temporary table.
      Allocate a copy of the name for safety only. Currently
      items with original names are always kept in memory,
      but in case this changes a memory leak may be hard to notice.
    */
    auto it_org = VisibleFields(send_result_set_metadata).begin();
    auto it_dst = item_list.begin();
    while (it_dst != item_list.end() &&
           it_org != VisibleFields(send_result_set_metadata).end()) {
      Item *item_org = *it_org++;
      Item *item_dst = *it_dst++;
      Send_field send_field;
      Item_ident *ident = static_cast<Item_ident *>(item_dst);
      item_org->make_field(&send_field);

      ident->db_name = thd->mem_strdup(send_field.db_name);
      ident->table_name = thd->mem_strdup(send_field.table_name);
    }
  }

  /*
    Original metadata result set should be sent here. After
    mysql_execute_command() is finished, item_list can not be used for
    sending metadata, because it references closed table.
  */
  if (m_result->send_result_set_metadata(thd, item_list,
                                         Protocol::SEND_NUM_ROWS)) {
    thd->swap_query_arena(backup_arena, &m_arena);
    return true;
  }

  thd->swap_query_arena(backup_arena, &m_arena);

  assert(!thd->is_error());

  return false;
}

bool Materialized_cursor::open(THD *thd) {
  bool rc;
  Query_arena backup_arena;

  thd->swap_query_arena(m_arena, &backup_arena);

  /* Create a list of fields and start sequential scan. */

  rc = m_result->prepare(thd, item_list, &fake_query_expression);
  rc = !rc && m_table->file->ha_rnd_init(true);
  is_rnd_inited = !rc;

  thd->swap_query_arena(backup_arena, &m_arena);

  /* Commit or rollback metadata in the client-server protocol. */

  if (!rc) {
    thd->server_status |= SERVER_STATUS_CURSOR_EXISTS;
    m_result->send_eof(thd);
  } else {
    m_result->abort_result_set(thd);
  }

  fetch_limit = 0;
  fetch_count = 0;

  return rc;
}

/**
  Fetch up to the given number of rows from a materialized cursor.

    Precondition: the cursor is open.

    If the cursor points after the last row, the fetch will automatically
    close the cursor and not send any data (except the 'EOF' packet
    with SERVER_STATUS_LAST_ROW_SENT). This is an extra round trip
    and probably should be improved to return
    SERVER_STATUS_LAST_ROW_SENT along with the last row.
*/

bool Materialized_cursor::fetch(ulong num_rows) {
  THD *thd = current_thd;

  int res = 0;
  for (fetch_limit += num_rows; fetch_count < fetch_limit; fetch_count++) {
    if ((res = m_table->file->ha_rnd_next(m_table->record[0]))) break;
    /* Send data only if the read was successful. */
    /*
      If network write failed (i.e. due to a closed socked),
      the error has already been set. Return true if the error
      is set.
    */
    if (m_result->send_data(thd, item_list)) return true;
  }

  switch (res) {
    case 0:
      thd->server_status |= SERVER_STATUS_CURSOR_EXISTS;
      m_result->send_eof(thd);
      break;
    case HA_ERR_END_OF_FILE:
      thd->server_status |= SERVER_STATUS_LAST_ROW_SENT;
      m_result->send_eof(thd);
      close();
      break;
    default:
      m_table->file->print_error(res, MYF(0));
      close();
      return true;
  }

  return false;
}

void Materialized_cursor::close() {
  if (is_rnd_inited) {
    (void)m_table->file->ha_rnd_end();
    is_rnd_inited = false;
  }
  close_tmp_table(m_table);
  m_arena.free_items();
  item_list.clear();
  mem_root.ClearForReuse();
}

Materialized_cursor::~Materialized_cursor() {
  assert(!is_open());
  if (m_table != nullptr) free_tmp_table(m_table);
}

/***************************************************************************
 Query_result_materialize
****************************************************************************/

bool Query_result_materialize::prepare(THD *thd,
                                       const mem_root_deque<Item *> &fields,
                                       Query_expression *u) {
  unit = u;

  if (m_result->prepare(thd, fields, u)) return true;

  assert(table == nullptr && m_cursor == nullptr);

  m_cursor = new (thd->mem_root) Materialized_cursor(m_result);
  if (m_cursor == nullptr) return true;
  /*
    Objects associated with the temporary table should be created as follows:
    - Metadata about the temporary table are created on the Statement mem_root.
      This mem_root should be bound to THD when this function is called.
    - HANDLER objects are created on the mem_root of the materialized cursor,
      since the handler must be kept open for subsequent FETCH operations.
      This must be ensured when the temporary table is instantiated.
  */
  if (create_result_table(thd, *unit->get_unit_column_types(), false,
                          thd->variables.option_bits | TMP_TABLE_ALL_COLUMNS,
                          "", false, false)) {
    destroy(m_cursor);
    return true;
  }
  m_cursor->set_table(table);

  return false;
}

bool Query_result_materialize::start_execution(THD *thd) {
  // If UNION, we may call this function multiple times.
  if (table->is_created()) return false;

  MEM_ROOT *saved_mem_root = thd->mem_root;
  thd->mem_root = &m_cursor->mem_root;
  if (instantiate_tmp_table(thd, table)) {
    thd->mem_root = saved_mem_root;
    return true;
  }

  table->file->ha_extra(HA_EXTRA_IGNORE_DUP_KEY);
  if (table->hash_field) table->file->ha_index_init(0, false);
  thd->mem_root = saved_mem_root;

  return false;
}

bool Query_result_materialize::send_result_set_metadata(
    THD *thd, const mem_root_deque<Item *> &list, uint) {
  if (m_cursor->send_result_set_metadata(thd, list)) {
    return true;
  }

  return false;
}
