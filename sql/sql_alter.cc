/* Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.

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

#include "sql_parse.h"                       // check_access
#include "sql_table.h"                       // mysql_alter_table,
                                             // mysql_exchange_partition
#include "sql_base.h"                        // open_temporary_tables
#include "sql_alter.h"

bool Sql_cmd_alter_table::execute(THD *thd)
{
  LEX *lex= thd->lex;
  /* first SELECT_LEX (have special meaning for many of non-SELECTcommands) */
  SELECT_LEX *select_lex= &lex->select_lex;
  /* first table of first SELECT_LEX */
  TABLE_LIST *first_table= (TABLE_LIST*) select_lex->table_list.first;
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
  /*
    We also require DROP priv for ALTER TABLE ... DROP PARTITION, as well
    as for RENAME TO, as being done by SQLCOM_RENAME_TABLE
  */
  if (alter_info.flags & (ALTER_DROP_PARTITION | ALTER_RENAME))
    priv_needed|= DROP_ACL;

  /* Must be set in the parser */
  DBUG_ASSERT(select_lex->db);
  DBUG_ASSERT(!(alter_info.flags & ALTER_EXCHANGE_PARTITION));
  DBUG_ASSERT(!(alter_info.flags & ALTER_ADMIN_PARTITION));
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
    memset(&tmp_table, 0, sizeof(tmp_table));
    tmp_table.table_name= lex->name.str;
    tmp_table.db= select_lex->db;
    tmp_table.grant.privilege= priv;
    if (check_grant(thd, INSERT_ACL | CREATE_ACL, &tmp_table, FALSE,
                    UINT_MAX, FALSE))
      DBUG_RETURN(TRUE);                  /* purecov: inspected */
  }

  /* Don't yet allow changing of symlinks with ALTER TABLE */
  if (create_info.data_file_name)
    push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
                        WARN_OPTION_IGNORED, ER(WARN_OPTION_IGNORED),
                        "DATA DIRECTORY");
  if (create_info.index_file_name)
    push_warning_printf(thd, Sql_condition::WARN_LEVEL_WARN,
                        WARN_OPTION_IGNORED, ER(WARN_OPTION_IGNORED),
                        "INDEX DIRECTORY");
  create_info.data_file_name= create_info.index_file_name= NULL;

  thd->enable_slow_log= opt_log_slow_admin_statements;

  result= mysql_alter_table(thd, select_lex->db, lex->name.str,
                            &create_info,
                            first_table,
                            &alter_info,
                            select_lex->order_list.elements,
                            select_lex->order_list.first,
                            lex->ignore);

  DBUG_RETURN(result);
}
