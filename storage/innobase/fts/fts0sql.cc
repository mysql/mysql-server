/*****************************************************************************

Copyright (c) 2007, 2023, Oracle and/or its affiliates.

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

/** @file fts/fts0sql.cc
 Full Text Search functionality.

 Created 2007-03-27 Sunny Bains
 *******************************************************/

#include <sys/types.h>

#include "dict0dd.h"
#include "dict0dict.h"
#include "fts0priv.h"
#include "fts0types.h"
#include "pars0pars.h"
#include "que0que.h"
#include "trx0roll.h"

#include <algorithm>
#include <string>
#include "current_thd.h"

/** SQL statements for creating the ancillary FTS tables. */

/** Preamble to all SQL statements. */
static const char *fts_sql_begin = "PROCEDURE P() IS\n";

/** Postamble to non-committing SQL statements. */
static const char *fts_sql_end =
    "\n"
    "END;\n";

/** Get the table id.
 @return number of bytes written */
int fts_get_table_id(
    const fts_table_t *fts_table, /*!< in: FTS Auxiliary table */
    char *table_id)               /*!< out: table id, must be at least
                                  FTS_AUX_MIN_TABLE_ID_LENGTH bytes
                                  long */
{
  int len;

  ut_a(fts_table->table != nullptr);

  switch (fts_table->type) {
    case FTS_COMMON_TABLE:
      len = fts_write_object_id(fts_table->table_id, table_id);
      break;

    case FTS_INDEX_TABLE:

      len = fts_write_object_id(fts_table->table_id, table_id);

      table_id[len] = '_';
      ++len;
      table_id += len;

      len += fts_write_object_id(fts_table->index_id, table_id);
      break;

    default:
      ut_error;
  }

  ut_a(len >= 16);
  ut_a(len < FTS_AUX_MIN_TABLE_ID_LENGTH);

  return (len);
}

/** Construct the prefix name of an FTS table.
@param[in]      fts_table       Auxiliary FTS table
@param[in]      is_5_7          true if we need 5.7 compatible name
@return own: table name, must be freed with ut::free() */
static char *fts_get_table_name_prefix_low(const fts_table_t *fts_table,
                                           bool is_5_7) {
  int len;
  const char *slash;
  char *prefix_name;
  int dbname_len = 0;
  int prefix_name_len;
  char table_id[FTS_AUX_MIN_TABLE_ID_LENGTH];

  slash = static_cast<const char *>(
      memchr(fts_table->parent, '/', strlen(fts_table->parent)));

  if (slash) {
    /* Print up to and including the separator. */
    dbname_len = static_cast<int>(slash - fts_table->parent) + 1;
  }

  len = fts_get_table_id(fts_table, table_id);

  prefix_name_len = dbname_len + 4 + len + 1;

  prefix_name = static_cast<char *>(
      ut::malloc_withkey(UT_NEW_THIS_FILE_PSI_KEY, prefix_name_len));

  len = sprintf(prefix_name, "%.*s%s%s", dbname_len, fts_table->parent,
                is_5_7 ? FTS_PREFIX_5_7 : FTS_PREFIX, table_id);

  ut_a(len > 0);
  ut_a(len == prefix_name_len - 1);

  return (prefix_name);
}

/** Construct the prefix name of an FTS table.
 @return own: table name, must be freed with ut::free() */
char *fts_get_table_name_prefix(
    const fts_table_t *fts_table) /*!< in: Auxiliary table type */
{
  return (fts_get_table_name_prefix_low(fts_table, false));
}

/** Construct the prefix name of an FTS table in 5.7 compatible name
@param[in]      fts_table       Auxiliary FTS table
@return own: table name, must be freed with ut::free() */
char *fts_get_table_name_prefix_5_7(const fts_table_t *fts_table) {
  return (fts_get_table_name_prefix_low(fts_table, true));
}

/** Construct the name of an ancillary FTS table for the given table.
Caller must allocate enough memory(usually size of MAX_FULL_NAME_LEN)
for param 'table_name'
@param[in]      fts_table       FTS Aux table
@param[in,out]  table_name      aux table name
@param[in]      is_5_7          true if we need 5.7 compatible name */
static void fts_get_table_name_low(const fts_table_t *fts_table,
                                   char *table_name, bool is_5_7) {
  int len;
  char *prefix_name;

  prefix_name = is_5_7 ? fts_get_table_name_prefix_5_7(fts_table)
                       : fts_get_table_name_prefix(fts_table);

  len = sprintf(table_name, "%s_%s", prefix_name, fts_table->suffix);

  ut_a(len > 0);
  ut_a(strlen(prefix_name) + 1 + strlen(fts_table->suffix) ==
       static_cast<uint>(len));

  ut::free(prefix_name);
}

/** Construct the name of an ancillary FTS table for the given table.
 Caller must allocate enough memory(usually size of MAX_FULL_NAME_LEN)
 for param 'table_name'. */
void fts_get_table_name(const fts_table_t *fts_table,
                        /*!< in: Auxiliary table type */
                        char *table_name)
/*!< in/out: aux table name */
{
  fts_get_table_name_low(fts_table, table_name, false);
}

/** Construct the name of an ancillary FTS table for the given table in
5.7 compatible format. Caller must allocate enough memory(usually size
of MAX_FULL_NAME_LEN) for param 'table_name'
@param[in]      fts_table       Auxiliary table object
@param[in,out]  table_name      aux table name */
void fts_get_table_name_5_7(const fts_table_t *fts_table, char *table_name) {
  fts_get_table_name_low(fts_table, table_name, true);
}

/** Parse an SQL string.
 @return query graph */
que_t *fts_parse_sql(
    fts_table_t *fts_table, /*!< in: FTS auxiliarry table info */
    pars_info_t *info,      /*!< in: info struct, or NULL */
    const char *sql)        /*!< in: SQL string to evaluate */
{
  char *str;
  que_t *graph;
  dict_table_t *aux_table = nullptr;
  MDL_ticket *mdl = nullptr;
  THD *thd = current_thd;

  str = ut_str3cat(fts_sql_begin, sql, fts_sql_end);

  /* To open this table in advance, in case it has to be opened
  in pars_sql where pars_mutex is held. This is because holding
  a mutex to open a table which may access InnoDB is not safe */
  if (fts_table != nullptr) {
    char table_name[MAX_FULL_NAME_LEN];

    fts_get_table_name(fts_table, table_name);

    aux_table = dd_table_open_on_name_in_mem(table_name, false);
    DBUG_EXECUTE_IF(
        "force_evict_fts_aux_table_and_reload", if (aux_table != nullptr) {
          dict_sys_mutex_enter();
          dd_table_close(aux_table, nullptr, nullptr, true);
          dict_table_remove_from_cache(aux_table);
          dict_sys_mutex_exit();
          aux_table = nullptr;
        });

    if (aux_table == nullptr) {
      aux_table = dd_table_open_on_name(thd, &mdl, table_name, false,
                                        DICT_ERR_IGNORE_NONE);
    }
  }

  /* The InnoDB SQL parser is not re-entrant. */
  mutex_enter(&pars_mutex);

  graph = pars_sql(info, str);
  ut_a(graph);

  mutex_exit(&pars_mutex);

  if (aux_table != nullptr) {
    dd_table_close(aux_table, thd, &mdl, false);
  }

  ut::free(str);

  return (graph);
}

/** Evaluate an SQL query graph.
 @return DB_SUCCESS or error code */
dberr_t fts_eval_sql(trx_t *trx,   /*!< in: transaction */
                     que_t *graph) /*!< in: Query graph to evaluate */
{
  graph->trx = trx;
  graph->fork_type = QUE_FORK_MYSQL_INTERFACE;

  auto thr = que_fork_start_command(graph);
  ut_a(thr);

  que_run_threads(thr);

  return (trx->error_state);
}

/** Construct the column specification part of the SQL string for selecting the
 indexed FTS columns for the given table. Adds the necessary bound
 ids to the given 'info' and returns the SQL string. Examples:

 One indexed column named "text":

  "$sel0",
  info/ids: sel0 -> "text"

 Two indexed columns named "subject" and "content":

  "$sel0, $sel1",
  info/ids: sel0 -> "subject", sel1 -> "content",
 @return heap-allocated WHERE string */
const char *fts_get_select_columns_str(
    dict_index_t *index, /*!< in: index */
    pars_info_t *info,   /*!< in/out: parser info */
    mem_heap_t *heap)    /*!< in: memory heap */
{
  ulint i;
  const char *str = "";

  for (i = 0; i < index->n_user_defined_cols; i++) {
    char *sel_str;

    dict_field_t *field = index->get_field(i);

    sel_str = mem_heap_printf(heap, "sel%lu", (ulong)i);

    /* Set copy_name to true since it's dynamic. */
    pars_info_bind_id(info, true, sel_str, field->name);

    str = mem_heap_printf(heap, "%s%s$%s", str, (*str) ? ", " : "", sel_str);
  }

  return (str);
}

/** Commit a transaction.
 @return DB_SUCCESS or error code */
dberr_t fts_sql_commit(trx_t *trx) /*!< in: transaction */
{
  dberr_t error;

  error = trx_commit_for_mysql(trx);

  /* Commit should always succeed */
  ut_a(error == DB_SUCCESS);

  return (DB_SUCCESS);
}

/** Rollback a transaction.
 @return DB_SUCCESS or error code */
dberr_t fts_sql_rollback(trx_t *trx) /*!< in: transaction */
{
  return (trx_rollback_to_savepoint(trx, nullptr));
}
