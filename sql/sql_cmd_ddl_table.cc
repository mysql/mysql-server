/* Copyright (c) 2016, 2022, Oracle and/or its affiliates.

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

#include "sql/sql_cmd_ddl_table.h"

#include <string.h>
#include <sys/types.h>

#include "my_inttypes.h"
#include "my_sys.h"
#include "mysqld_error.h"
#include "scope_guard.h"
#include "sql/auth/auth_acls.h"
#include "sql/auth/auth_common.h"  // create_table_precheck()
#include "sql/binlog.h"            // mysql_bin_log
#include "sql/dd/cache/dictionary_client.h"
#include "sql/derror.h"         // ER_THD
#include "sql/error_handler.h"  // Ignore_error_handler
#include "sql/handler.h"
#include "sql/item.h"
#include "sql/mysqld.h"          // opt_log_slow_admin_statements
#include "sql/partition_info.h"  // check_partition_tablespace_names()
#include "sql/query_options.h"
#include "sql/query_result.h"
#include "sql/session_tracker.h"
#include "sql/sql_alter.h"
#include "sql/sql_base.h"  // open_tables_for_query()
#include "sql/sql_class.h"
#include "sql/sql_data_change.h"
#include "sql/sql_error.h"
#include "sql/sql_insert.h"  // Query_result_create
#include "sql/sql_lex.h"
#include "sql/sql_list.h"
#include "sql/sql_parse.h"       // prepare_index_and_data_dir_path()
#include "sql/sql_select.h"      // Query_result_create
#include "sql/sql_table.h"       // mysql_create_like_table()
#include "sql/sql_tablespace.h"  // validate_tablespace_name()
#include "sql/strfunc.h"
#include "sql/system_variables.h"  // system_variables
#include "sql/table.h"             // table
#include "sql/thd_raii.h"          // Prepared_stmt_arena_holder
#include "thr_lock.h"

#ifndef NDEBUG
#include "sql/current_thd.h"
#endif  // NDEBUG

Sql_cmd_ddl_table::Sql_cmd_ddl_table(Alter_info *alter_info)
    : m_alter_info(alter_info) {
#ifndef NDEBUG
  LEX *lex = current_thd->lex;
  assert(lex->alter_info == m_alter_info);
  assert(lex->sql_command == SQLCOM_ALTER_TABLE ||
         lex->sql_command == SQLCOM_ANALYZE ||
         lex->sql_command == SQLCOM_ASSIGN_TO_KEYCACHE ||
         lex->sql_command == SQLCOM_CHECK ||
         lex->sql_command == SQLCOM_CREATE_INDEX ||
         lex->sql_command == SQLCOM_CREATE_TABLE ||
         lex->sql_command == SQLCOM_DROP_INDEX ||
         lex->sql_command == SQLCOM_OPTIMIZE ||
         lex->sql_command == SQLCOM_PRELOAD_KEYS ||
         lex->sql_command == SQLCOM_REPAIR);
#endif  // NDEBUG
  assert(m_alter_info != nullptr);
}

/**
  Populate tables from result of evaluating a query expression.

  This function is required because a statement like CREATE TABLE ... SELECT
  cannot be implemented using DML statement execution functions
  since it performs an intermediate commit that requires special attention.

  @param thd thread handler
  @param lex represents a prepared query expression

  @returns false if success, true if error
*/
static bool populate_table(THD *thd, LEX *lex) {
  Query_expression *const unit = lex->unit;

  if (lex->set_var_list.elements && resolve_var_assignments(thd, lex))
    return true;

  // Use the hypergraph optimizer for the SELECT statement, if enabled.
  lex->using_hypergraph_optimizer =
      thd->optimizer_switch_flag(OPTIMIZER_SWITCH_HYPERGRAPH_OPTIMIZER);

  lex->set_exec_started();

  /*
    Table creation may perform an intermediate commit and must therefore
    be performed before locking the tables in the query expression.
  */
  if (unit->query_result()->create_table_for_query_block(thd)) return true;

  if (lock_tables(thd, lex->query_tables, lex->table_count, 0)) return true;

  if (unit->optimize(thd, nullptr, true, /*finalize_access_paths=*/true))
    return true;

  // Calculate the current statement cost.
  accumulate_statement_cost(lex);

  // Perform secondary engine optimizations, if needed.
  if (optimize_secondary_engine(thd)) return true;

  if (unit->execute(thd)) return true;

  return false;
}

bool Sql_cmd_create_table::execute(THD *thd) {
  LEX *const lex = thd->lex;
  Query_block *const query_block = lex->query_block;
  Query_expression *const query_expression = lex->unit;
  Table_ref *const create_table = lex->query_tables;
  partition_info *part_info = lex->part_info;

  /*
    Code below (especially in mysql_create_table() and Query_result_create
    methods) may modify HA_CREATE_INFO structure in LEX, so we have to
    use a copy of this structure to make execution prepared statement-
    safe. A shallow copy is enough as this code won't modify any memory
    referenced from this structure.
  */
  HA_CREATE_INFO create_info(*lex->create_info);
  /*
    We need to copy alter_info for the same reasons of re-execution
    safety, only in case of Alter_info we have to do (almost) a deep
    copy.
  */
  Alter_info alter_info(*m_alter_info, thd->mem_root);

  if (thd->is_error()) {
    /* If out of memory when creating a copy of alter_info. */
    return true;
  }

  if (((lex->create_info->used_fields & HA_CREATE_USED_DATADIR) != 0 ||
       (lex->create_info->used_fields & HA_CREATE_USED_INDEXDIR) != 0) &&
      check_access(thd, FILE_ACL, any_db, nullptr, nullptr, false, false)) {
    my_error(ER_SPECIFIC_ACCESS_DENIED_ERROR, MYF(0), "FILE");
    return true;
  }

  if (!thd->is_plugin_fake_ddl()) {
    if (create_table_precheck(thd, query_expression_tables, create_table))
      return true;
  }

  /* Might have been updated in create_table_precheck */
  create_info.alias = create_table->alias;

  /*
    If no engine type was given, work out the default now
    rather than at parse-time.
  */
  if (!(create_info.used_fields & HA_CREATE_USED_ENGINE))
    create_info.db_type = create_info.options & HA_LEX_CREATE_TMP_TABLE
                              ? ha_default_temp_handlerton(thd)
                              : ha_default_handlerton(thd);

  assert(create_info.db_type != nullptr);
  if ((m_alter_info->flags & Alter_info::ANY_ENGINE_ATTRIBUTE) != 0 &&
      ((create_info.db_type->flags & HTON_SUPPORTS_ENGINE_ATTRIBUTE) == 0 &&
       DBUG_EVALUATE_IF("simulate_engine_attribute_support", false, true))) {
    my_error(ER_ENGINE_ATTRIBUTE_NOT_SUPPORTED, MYF(0),
             ha_resolve_storage_engine_name(create_info.db_type));
    return true;
  }

  /*
    Assign target tablespace name to enable locking in lock_table_names().
    Reject invalid names.
  */
  if (create_info.tablespace) {
    if (validate_tablespace_name_length(create_info.tablespace) ||
        validate_tablespace_name(TS_CMD_NOT_DEFINED, create_info.tablespace,
                                 create_info.db_type))
      return true;

    if (lex_string_strmake(thd->mem_root, &create_table->target_tablespace_name,
                           create_info.tablespace,
                           strlen(create_info.tablespace)))
      return true;
  }

  // Reject invalid tablespace names specified for partitions.
  if (validate_partition_tablespace_name_lengths(part_info) ||
      validate_partition_tablespace_names(part_info, create_info.db_type))
    return true;

  /* Fix names if symlinked or relocated tables */
  if (prepare_index_and_data_dir_path(thd, &create_info.data_file_name,
                                      &create_info.index_file_name,
                                      create_table->table_name))
    return true;

  {
    partition_info *part = thd->lex->part_info;
    if (part != nullptr && has_external_data_or_index_dir(*part) &&
        check_access(thd, FILE_ACL, any_db, nullptr, nullptr, false, false)) {
      return true;
    }
    if (part && !(part = thd->lex->part_info->get_clone(thd, true)))
      return true;
    thd->work_part_info = part;
  }

  if (part_info != nullptr && part_info->part_expr &&
      part_info->part_expr->fixed) {  // @todo Code may be redundant
    part_info->fixed = true;
  }
  bool res = false;

  if (!query_block->field_list_is_empty())  // With select
  {
    /*
      CREATE TABLE...IGNORE/REPLACE SELECT... can be unsafe, unless
      ORDER BY PRIMARY KEY clause is used in SELECT statement. We therefore
      use row based logging if mixed or row based logging is available.
      TODO: Check if the order of the output of the select statement is
      deterministic. Waiting for BUG#42415
    */
    if (lex->is_ignore())
      lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_CREATE_IGNORE_SELECT);

    if (lex->duplicates == DUP_REPLACE)
      lex->set_stmt_unsafe(LEX::BINLOG_STMT_UNSAFE_CREATE_REPLACE_SELECT);

    /**
      Disallow creation of foreign keys if,

      - SE supports atomic DDL's.
      - The binlogging is enabled.
      - The binlog format is ROW.

      This is done to avoid complications involved in locking,
      updating and invalidation (in case of rollback) of DD cache
      for parent table.
    */
    if ((alter_info.flags & Alter_info::ADD_FOREIGN_KEY) &&
        (create_info.db_type->flags & HTON_SUPPORTS_ATOMIC_DDL) &&
        mysql_bin_log.is_open() &&
        (thd->variables.option_bits & OPTION_BIN_LOG) &&
        thd->variables.binlog_format == BINLOG_FORMAT_ROW) {
      my_error(ER_FOREIGN_KEY_WITH_ATOMIC_CREATE_SELECT, MYF(0));
      return true;
    }

    // Reject request to CREATE TABLE AS SELECT with START TRANSACTION.
    if (create_info.m_transactional_ddl) {
      my_error(ER_NOT_ALLOWED_WITH_START_TRANSACTION, MYF(0),
               "with CREATE TABLE ... AS SELECT statement.");
      return true;
    }

    /*
      If:
      a) we inside an SP and there was NAME_CONST substitution,
      b) binlogging is on (STMT mode),
      c) we log the SP as separate statements
      raise a warning, as it may cause problems
      (see 'NAME_CONST issues' in 'Binary Logging of Stored Programs')
     */
    if (thd->query_name_consts && mysql_bin_log.is_open() &&
        thd->variables.binlog_format == BINLOG_FORMAT_STMT &&
        !mysql_bin_log.is_query_in_union(thd, thd->query_id)) {
      uint splocal_refs = 0;
      /* Count SP local vars in the top-level SELECT list */
      for (Item *item : query_block->visible_fields()) {
        if (item->is_splocal()) splocal_refs++;
      }
      /*
        If it differs from number of NAME_CONST substitution applied,
        we may have a SOME_FUNC(NAME_CONST()) in the SELECT list,
        that may cause a problem with binary log (see BUG#35383),
        raise a warning.
      */
      if (splocal_refs != thd->query_name_consts)
        push_warning(
            thd, Sql_condition::SL_WARNING, ER_UNKNOWN_ERROR,
            "Invoked routine ran a statement that may cause problems with "
            "binary log, see 'NAME_CONST issues' in 'Binary Logging of Stored "
            "Programs' "
            "section of the manual.");
    }

    /*
      Disable non-empty MERGE tables with CREATE...SELECT. Too
      complicated. See Bug #26379. Empty MERGE tables are read-only
      and don't allow CREATE...SELECT anyway.
    */
    if (create_info.used_fields & HA_CREATE_USED_UNION) {
      my_error(ER_WRONG_OBJECT, MYF(0), create_table->db,
               create_table->table_name, "BASE TABLE");
      return true;
    }

    if (query_expression->is_prepared()) {
      cleanup(thd);
    }
    auto cleanup_se_guard = create_scope_guard(
        [lex] { lex->set_secondary_engine_execution_context(nullptr); });
    if (open_tables_for_query(thd, lex->query_tables, false)) return true;

    /* The table already exists */
    if (create_table->table || create_table->is_view()) {
      if (create_info.options & HA_LEX_CREATE_IF_NOT_EXISTS) {
        push_warning_printf(thd, Sql_condition::SL_NOTE, ER_TABLE_EXISTS_ERROR,
                            ER_THD(thd, ER_TABLE_EXISTS_ERROR),
                            create_info.alias);
        my_ok(thd);
        return false;
      } else {
        my_error(ER_TABLE_EXISTS_ERROR, MYF(0), create_info.alias);
        return false;
      }
    }

    /*
      Remove target table from main select and name resolution
      context. This can't be done earlier as it will break view merging in
      statements like "CREATE TABLE IF NOT EXISTS existing_view SELECT".
    */
    bool link_to_local;
    lex->unlink_first_table(&link_to_local);

    /* Updating any other table is prohibited in CTS statement */
    for (Table_ref *table = lex->query_tables; table;
         table = table->next_global) {
      if (table->lock_descriptor().type >= TL_WRITE_ALLOW_WRITE) {
        lex->link_first_table_back(create_table, link_to_local);

        my_error(ER_CANT_UPDATE_TABLE_IN_CREATE_TABLE_SELECT, MYF(0),
                 table->table_name, create_info.alias);
        return true;
      }
    }

    Query_result_create *result;
    if (!query_expression->is_prepared()) {
      Prepared_stmt_arena_holder ps_arena_holder(thd);
      result = new (thd->mem_root)
          Query_result_create(create_table, &query_block->fields,
                              lex->duplicates, query_expression_tables);
      if (result == nullptr) {
        lex->link_first_table_back(create_table, link_to_local);
        return true;
      }
      if (query_expression->prepare(thd, result, nullptr, SELECT_NO_UNLOCK,
                                    0)) {
        lex->link_first_table_back(create_table, link_to_local);
        return true;
      }
      if (!thd->stmt_arena->is_regular() && lex->save_cmd_properties(thd)) {
        lex->link_first_table_back(create_table, link_to_local);
        return true;
      }
    } else {
      result = down_cast<Query_result_create *>(
          query_expression->query_result() != nullptr
              ? query_expression->query_result()
              : query_block->query_result());
      // Restore prepared statement properties, bind table and field information
      lex->restore_cmd_properties();
      bind_fields(thd->stmt_arena->item_list());
    }
    if (validate_use_secondary_engine(lex)) return true;

    result->set_two_fields(&create_info, &alter_info);

    // For objects acquired during table creation.
    dd::cache::Dictionary_client::Auto_releaser releaser(thd->dd_client());

    Ignore_error_handler ignore_handler;
    Strict_error_handler strict_handler;
    if (lex->is_ignore())
      thd->push_internal_handler(&ignore_handler);
    else if (thd->is_strict_mode())
      thd->push_internal_handler(&strict_handler);

    res = populate_table(thd, lex);

    // Count the number of statements offloaded to a secondary storage engine.
    if (using_secondary_storage_engine() && lex->unit->is_executed())
      ++thd->status_var.secondary_engine_execution_count;

    if (lex->is_ignore() || thd->is_strict_mode()) thd->pop_internal_handler();
    lex->cleanup(false);
    thd->clear_current_query_costs();
    lex->clear_values_map();

    // Abort the result set if execution ended in error
    if (res) result->abort_result_set(thd);

    result->cleanup();

    lex->link_first_table_back(create_table, link_to_local);
    THD_STAGE_INFO(thd, stage_end);
  } else {
    Strict_error_handler strict_handler;
    /* Push Strict_error_handler */
    if (!lex->is_ignore() && thd->is_strict_mode())
      thd->push_internal_handler(&strict_handler);
    /* regular create */
    if (create_info.options & HA_LEX_CREATE_TABLE_LIKE) {
      /* CREATE TABLE ... LIKE ... */
      res = mysql_create_like_table(thd, create_table, query_expression_tables,
                                    &create_info);
    } else {
      /* Regular CREATE TABLE */
      res = mysql_create_table(thd, create_table, &create_info, &alter_info);
    }
    /* Pop Strict_error_handler */
    if (!lex->is_ignore() && thd->is_strict_mode()) thd->pop_internal_handler();
    if (!res) {
      /* in case of create temp tables if @@session_track_state_change is
         ON then send session state notification in OK packet */
      if (create_info.options & HA_LEX_CREATE_TMP_TABLE &&
          thd->session_tracker.get_tracker(SESSION_STATE_CHANGE_TRACKER)
              ->is_enabled())
        thd->session_tracker.get_tracker(SESSION_STATE_CHANGE_TRACKER)
            ->mark_as_changed(thd, {});
      my_ok(thd);
    }
  }
  // The following code is required to make CREATE TABLE re-execution safe.
  // @todo Consider refactoring this code.
  if (part_info != nullptr) {
    if (part_info->part_expr != nullptr &&
        part_info->part_expr->type() == Item::FIELD_ITEM)
      down_cast<Item_field *>(part_info->part_expr)->reset_field();

    if (part_info->subpart_expr != nullptr &&
        part_info->subpart_expr->type() == Item::FIELD_ITEM)
      down_cast<Item_field *>(part_info->subpart_expr)->reset_field();
  }
  return res;
}

const MYSQL_LEX_CSTRING *
Sql_cmd_create_table::eligible_secondary_storage_engine() const {
  // Now check if the opened tables are available in a secondary
  // storage engine. Only use the secondary tables if all the tables
  // have a secondary tables, and they are all in the same secondary
  // storage engine.
  const LEX_CSTRING *secondary_engine = nullptr;

  for (const Table_ref *tl = query_expression_tables; tl != nullptr;
       tl = tl->next_global) {
    // Schema tables are not available in secondary engines.
    if (tl->schema_table != nullptr) return nullptr;

    // We're only interested in base tables.
    if (tl->is_placeholder()) continue;

    assert(!tl->table->s->is_secondary_engine());
    // If not in a secondary engine
    if (!tl->table->s->has_secondary_engine()) return nullptr;
    // Compare two engine names using the system collation.
    auto equal = [](const LEX_CSTRING &s1, const LEX_CSTRING &s2) {
      return system_charset_info->coll->strnncollsp(
                 system_charset_info,
                 pointer_cast<const unsigned char *>(s1.str), s1.length,
                 pointer_cast<const unsigned char *>(s2.str), s2.length) == 0;
    };

    if (secondary_engine == nullptr) {
      // First base table. Save its secondary engine name for later.
      secondary_engine = &tl->table->s->secondary_engine;
    } else if (!equal(*secondary_engine, tl->table->s->secondary_engine)) {
      // In a different secondary engine than the previous base tables.
      return nullptr;
    }
  }

  return secondary_engine;
}

bool Sql_cmd_create_or_drop_index_base::execute(THD *thd) {
  /*
    CREATE INDEX and DROP INDEX are implemented by calling ALTER
    TABLE with proper arguments.

    In the future ALTER TABLE will notice that the request is to
    only add indexes and create these one by one for the existing
    table without having to do a full rebuild.
  */

  LEX *const lex = thd->lex;
  Query_block *const query_block = lex->query_block;
  Table_ref *const first_table = query_block->get_table_list();
  Table_ref *const all_tables = first_table;

  /* Prepare stack copies to be re-execution safe */
  HA_CREATE_INFO create_info;
  Alter_info alter_info(*m_alter_info, thd->mem_root);

  if (thd->is_fatal_error()) /* out of memory creating a copy of alter_info */
    return true;             // OOM

  if (check_one_table_access(thd, INDEX_ACL, all_tables)) return true;
  /*
    Currently CREATE INDEX or DROP INDEX cause a full table rebuild
    and thus classify as slow administrative statements just like
    ALTER TABLE.
  */
  thd->enable_slow_log = opt_log_slow_admin_statements;

  create_info.db_type = nullptr;
  create_info.row_type = ROW_TYPE_NOT_USED;
  create_info.default_table_charset = thd->variables.collation_database;

  /* Push Strict_error_handler */
  Strict_error_handler strict_handler;
  if (thd->is_strict_mode()) thd->push_internal_handler(&strict_handler);
  assert(!query_block->order_list.elements);
  const bool res =
      mysql_alter_table(thd, first_table->db, first_table->table_name,
                        &create_info, first_table, &alter_info);
  /* Pop Strict_error_handler */
  if (thd->is_strict_mode()) thd->pop_internal_handler();
  return res;
}

bool Sql_cmd_cache_index::execute(THD *thd) {
  Table_ref *const first_table = thd->lex->query_block->get_table_list();
  if (check_table_access(thd, INDEX_ACL, first_table, true, UINT_MAX, false))
    return true;

  return assign_to_keycache(thd, first_table);
}

bool Sql_cmd_load_index::execute(THD *thd) {
  Table_ref *const first_table = thd->lex->query_block->get_table_list();
  if (check_table_access(thd, INDEX_ACL, first_table, true, UINT_MAX, false))
    return true;
  return preload_keys(thd, first_table);
}
