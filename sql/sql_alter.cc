/* Copyright (c) 2010, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "sql_alter.h"

#include "auth_common.h"                     // check_access
#include "sql_table.h"                       // mysql_alter_table,
                                             // mysql_exchange_partition
#include "sql_base.h"                        // open_temporary_tables
#include "log.h"

bool has_external_data_or_index_dir(partition_info &pi);

Alter_info::Alter_info(const Alter_info &rhs, MEM_ROOT *mem_root)
  :drop_list(rhs.drop_list, mem_root),
  alter_list(rhs.alter_list, mem_root),
  key_list(rhs.key_list, mem_root),
  alter_rename_key_list(rhs.alter_rename_key_list, mem_root),
  create_list(rhs.create_list, mem_root),
  flags(rhs.flags),
  keys_onoff(rhs.keys_onoff),
  partition_names(rhs.partition_names, mem_root),
  num_parts(rhs.num_parts),
  requested_algorithm(rhs.requested_algorithm),
  requested_lock(rhs.requested_lock),
  with_validation(rhs.with_validation)
{
  /*
    Make deep copies of used objects.
    This is not a fully deep copy - clone() implementations
    of Alter_drop, Alter_column, Key, foreign_key, Key_part_spec
    do not copy string constants. At the same length the only
    reason we make a copy currently is that ALTER/CREATE TABLE
    code changes input Alter_info definitions, but string
    constants never change.
  */
  list_copy_and_replace_each_value(drop_list, mem_root);
  list_copy_and_replace_each_value(alter_list, mem_root);
  list_copy_and_replace_each_value(key_list, mem_root);
  list_copy_and_replace_each_value(alter_rename_key_list, mem_root);
  list_copy_and_replace_each_value(create_list, mem_root);
  /* partition_names are not deeply copied currently */
}


bool Alter_info::set_requested_algorithm(const LEX_STRING *str)
{
  // To avoid adding new keywords to the grammar, we match strings here.
  if (!my_strcasecmp(system_charset_info, str->str, "INPLACE"))
    requested_algorithm= ALTER_TABLE_ALGORITHM_INPLACE;
  else if (!my_strcasecmp(system_charset_info, str->str, "COPY"))
    requested_algorithm= ALTER_TABLE_ALGORITHM_COPY;
  else if (!my_strcasecmp(system_charset_info, str->str, "DEFAULT"))
    requested_algorithm= ALTER_TABLE_ALGORITHM_DEFAULT;
  else
    return true;
  return false;
}


bool Alter_info::set_requested_lock(const LEX_STRING *str)
{
  // To avoid adding new keywords to the grammar, we match strings here.
  if (!my_strcasecmp(system_charset_info, str->str, "NONE"))
    requested_lock= ALTER_TABLE_LOCK_NONE;
  else if (!my_strcasecmp(system_charset_info, str->str, "SHARED"))
    requested_lock= ALTER_TABLE_LOCK_SHARED;
  else if (!my_strcasecmp(system_charset_info, str->str, "EXCLUSIVE"))
    requested_lock= ALTER_TABLE_LOCK_EXCLUSIVE;
  else if (!my_strcasecmp(system_charset_info, str->str, "DEFAULT"))
    requested_lock= ALTER_TABLE_LOCK_DEFAULT;
  else
    return true;
  return false;
}


Alter_table_ctx::Alter_table_ctx()
  : datetime_field(NULL), error_if_not_empty(false),
    tables_opened(0),
    db(NULL), table_name(NULL), alias(NULL),
    new_db(NULL), new_name(NULL), new_alias(NULL)
#ifndef DBUG_OFF
    , tmp_table(false)
#endif
{
}


Alter_table_ctx::Alter_table_ctx(THD *thd, TABLE_LIST *table_list,
                                 uint tables_opened_arg,
                                 const char *new_db_arg,
                                 const char *new_name_arg)
  : datetime_field(NULL), error_if_not_empty(false),
    tables_opened(tables_opened_arg),
    new_db(new_db_arg), new_name(new_name_arg)
#ifndef DBUG_OFF
    , tmp_table(false)
#endif
{
  /*
    Assign members db, table_name, new_db and new_name
    to simplify further comparisions: we want to see if it's a RENAME
    later just by comparing the pointers, avoiding the need for strcmp.
  */
  db= table_list->db;
  table_name= table_list->table_name;
  alias= (lower_case_table_names == 2) ? table_list->alias : table_name;

  if (!new_db || !my_strcasecmp(table_alias_charset, new_db, db))
    new_db= db;

  if (new_name)
  {
    DBUG_PRINT("info", ("new_db.new_name: '%s'.'%s'", new_db, new_name));

    if (lower_case_table_names == 1) // Convert new_name/new_alias to lower case
    {
      my_casedn_str(files_charset_info, const_cast<char*>(new_name));
      new_alias= new_name;
    }
    else if (lower_case_table_names == 2) // Convert new_name to lower case
    {
      my_stpcpy(new_alias_buff, new_name);
      new_alias= (const char*)new_alias_buff;
      my_casedn_str(files_charset_info, const_cast<char*>(new_name));
    }
    else
      new_alias= new_name; // LCTN=0 => case sensitive + case preserving

    if (!is_database_changed() &&
        !my_strcasecmp(table_alias_charset, new_name, table_name))
    {
      /*
        Source and destination table names are equal:
        make is_table_renamed() more efficient.
      */
      new_alias= table_name;
      new_name= table_name;
    }
  }
  else
  {
    new_alias= alias;
    new_name= table_name;
  }

  my_snprintf(tmp_name, sizeof(tmp_name), "%s-%lx_%x", tmp_file_prefix,
              current_pid, thd->thread_id());
  /* Safety fix for InnoDB */
  if (lower_case_table_names)
    my_casedn_str(files_charset_info, tmp_name);

  if (table_list->table->s->tmp_table == NO_TMP_TABLE)
  {
    build_table_filename(path, sizeof(path) - 1, db, table_name, "", 0);

    build_table_filename(new_path, sizeof(new_path) - 1, new_db, new_name, "", 0);

    build_table_filename(new_filename, sizeof(new_filename) - 1,
                         new_db, new_name, reg_ext, 0);

    build_table_filename(tmp_path, sizeof(tmp_path) - 1, new_db, tmp_name, "",
                         FN_IS_TMP);
  }
  else
  {
    /*
      We are not filling path, new_path and new_filename members if
      we are altering temporary table as these members are not used in
      this case. This fact is enforced with assert.
    */
    build_tmptable_filename(thd, tmp_path, sizeof(tmp_path));
#ifndef DBUG_OFF
    tmp_table= true;
#endif
  }
}


bool Sql_cmd_alter_table::execute(THD *thd)
{
  LEX *lex= thd->lex;
  /* first SELECT_LEX (have special meaning for many of non-SELECTcommands) */
  SELECT_LEX *select_lex= lex->select_lex;
  /* first table of first SELECT_LEX */
  TABLE_LIST *first_table= select_lex->get_table_list();
  /*
    Code in mysql_alter_table() may modify its HA_CREATE_INFO argument,
    so we have to use a copy of this structure to make execution
    prepared statement- safe. A shallow copy is enough as no memory
    referenced from this structure will be modified.
    @todo move these into constructor...
  */
  HA_CREATE_INFO create_info(lex->create_info);
  Alter_info alter_info(lex->alter_info, thd->mem_root);
  ulong priv=0;
  ulong priv_needed= ALTER_ACL;
  bool result;

  DBUG_ENTER("Sql_cmd_alter_table::execute");

  if (thd->is_fatal_error) /* out of memory creating a copy of alter_info */
    DBUG_RETURN(TRUE);

  {
    partition_info *part_info= thd->lex->part_info;
    if (part_info != NULL && has_external_data_or_index_dir(*part_info) &&
        check_access(thd, FILE_ACL, any_db, NULL, NULL, FALSE, FALSE))

      DBUG_RETURN(TRUE);
  }
  /*
    We also require DROP priv for ALTER TABLE ... DROP PARTITION, as well
    as for RENAME TO, as being done by SQLCOM_RENAME_TABLE
  */
  if (alter_info.flags & (Alter_info::ALTER_DROP_PARTITION |
                          Alter_info::ALTER_RENAME))
    priv_needed|= DROP_ACL;

  /* Must be set in the parser */
  DBUG_ASSERT(select_lex->db);
  DBUG_ASSERT(!(alter_info.flags & Alter_info::ALTER_EXCHANGE_PARTITION));
  DBUG_ASSERT(!(alter_info.flags & Alter_info::ALTER_ADMIN_PARTITION));
  if (check_access(thd, priv_needed, first_table->db,
                   &first_table->grant.privilege,
                   &first_table->grant.m_internal,
                   0, 0) ||
      check_access(thd, INSERT_ACL | CREATE_ACL, select_lex->db,
                   &priv,
                   NULL, /* Don't use first_tab->grant with sel_lex->db */
                   0, 0))
    DBUG_RETURN(TRUE);                  /* purecov: inspected */

  /* If it is a merge table, check privileges for merge children. */
  if (create_info.merge_list.first)
  {
    /*
      The user must have (SELECT_ACL | UPDATE_ACL | DELETE_ACL) on the
      underlying base tables, even if there are temporary tables with the same
      names.

      From user's point of view, it might look as if the user must have these
      privileges on temporary tables to create a merge table over them. This is
      one of two cases when a set of privileges is required for operations on
      temporary tables (see also CREATE TABLE).

      The reason for this behavior stems from the following facts:

        - For merge tables, the underlying table privileges are checked only
          at CREATE TABLE / ALTER TABLE time.

          In other words, once a merge table is created, the privileges of
          the underlying tables can be revoked, but the user will still have
          access to the merge table (provided that the user has privileges on
          the merge table itself). 

        - Temporary tables shadow base tables.

          I.e. there might be temporary and base tables with the same name, and
          the temporary table takes the precedence in all operations.

        - For temporary MERGE tables we do not track if their child tables are
          base or temporary. As result we can't guarantee that privilege check
          which was done in presence of temporary child will stay relevant later
          as this temporary table might be removed.

      If SELECT_ACL | UPDATE_ACL | DELETE_ACL privileges were not checked for
      the underlying *base* tables, it would create a security breach as in
      Bug#12771903.
    */

    if (check_table_access(thd, SELECT_ACL | UPDATE_ACL | DELETE_ACL,
                           create_info.merge_list.first, FALSE, UINT_MAX, FALSE))
      DBUG_RETURN(TRUE);
  }

  if (check_grant(thd, priv_needed, first_table, FALSE, UINT_MAX, FALSE))
    DBUG_RETURN(TRUE);                  /* purecov: inspected */

  if (lex->name.str && !test_all_bits(priv, INSERT_ACL | CREATE_ACL))
  {
    // Rename of table
    TABLE_LIST tmp_table;

    tmp_table.table_name= lex->name.str;
    tmp_table.db= select_lex->db;
    tmp_table.grant.privilege= priv;
    if (check_grant(thd, INSERT_ACL | CREATE_ACL, &tmp_table, FALSE,
                    UINT_MAX, FALSE))
      DBUG_RETURN(TRUE);                  /* purecov: inspected */
  }

  /* Don't yet allow changing of symlinks with ALTER TABLE */
  if (create_info.data_file_name)
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        WARN_OPTION_IGNORED, ER(WARN_OPTION_IGNORED),
                        "DATA DIRECTORY");
  if (create_info.index_file_name)
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        WARN_OPTION_IGNORED, ER(WARN_OPTION_IGNORED),
                        "INDEX DIRECTORY");
  create_info.data_file_name= create_info.index_file_name= NULL;

  thd->enable_slow_log= opt_log_slow_admin_statements;

  /* Push Strict_error_handler for alter table*/
  Strict_error_handler strict_handler;
  if (!thd->lex->is_ignore() && thd->is_strict_mode())
    thd->push_internal_handler(&strict_handler);

  Partition_in_shared_ts_error_handler partition_in_shared_ts_handler;
  thd->push_internal_handler(&partition_in_shared_ts_handler);
  result= mysql_alter_table(thd, select_lex->db, lex->name.str,
                            &create_info, first_table, &alter_info);
  thd->pop_internal_handler();

  if (!thd->lex->is_ignore() && thd->is_strict_mode())
    thd->pop_internal_handler();
  DBUG_RETURN(result);
}


bool Sql_cmd_discard_import_tablespace::execute(THD *thd)
{
  /* first SELECT_LEX (have special meaning for many of non-SELECTcommands) */
  SELECT_LEX *select_lex= thd->lex->select_lex;
  /* first table of first SELECT_LEX */
  TABLE_LIST *table_list= select_lex->get_table_list();

  if (check_access(thd, ALTER_ACL, table_list->db,
                   &table_list->grant.privilege,
                   &table_list->grant.m_internal,
                   0, 0))
    return true;

  if (check_grant(thd, ALTER_ACL, table_list, false, UINT_MAX, false))
    return true;

  thd->enable_slow_log= opt_log_slow_admin_statements;

  /*
    Check if we attempt to alter mysql.slow_log or
    mysql.general_log table and return an error if
    it is the case.
    TODO: this design is obsolete and will be removed.
  */
  enum_log_table_type table_kind=
    query_logger.check_if_log_table(table_list, false);

  if (table_kind != QUERY_LOG_NONE)
  {
    /* Disable alter of enabled query log tables */
    if (query_logger.is_log_table_enabled(table_kind))
    {
      my_error(ER_BAD_LOG_STATEMENT, MYF(0), "ALTER");
      return true;
    }
  }

  /*
    Add current database to the list of accessed databases
    for this statement. Needed for MTS.
  */
  thd->add_to_binlog_accessed_dbs(table_list->db);

  return
    mysql_discard_or_import_tablespace(thd, table_list,
                                       m_tablespace_op == DISCARD_TABLESPACE);
}
