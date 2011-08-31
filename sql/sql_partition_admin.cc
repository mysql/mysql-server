/* Copyright (c) 2010, Oracle and/or its affiliates. All rights reserved.

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

#include "sql_parse.h"                      // check_one_table_access
#include "sql_table.h"                      // mysql_alter_table, etc.
#include "sql_lex.h"                        // Sql_statement
#include "sql_admin.h"                      // Analyze/Check/.._table_statement
#include "sql_partition_admin.h"            // Alter_table_*_partition
#ifdef WITH_PARTITION_STORAGE_ENGINE
#include "ha_partition.h"                   // ha_partition
#endif
#include "sql_base.h"                       // open_and_lock_tables

#ifndef WITH_PARTITION_STORAGE_ENGINE

bool Partition_statement_unsupported::execute(THD *)
{
  DBUG_ENTER("Partition_statement_unsupported::execute");
  /* error, partitioning support not compiled in... */
  my_error(ER_FEATURE_DISABLED, MYF(0), "partitioning",
           "--with-plugin-partition");
  DBUG_RETURN(TRUE);
}

#else

bool Alter_table_analyze_partition_statement::execute(THD *thd)
{
  bool res;
  DBUG_ENTER("Alter_table_analyze_partition_statement::execute");

  /*
    Flag that it is an ALTER command which administrates partitions, used
    by ha_partition
  */
  m_lex->alter_info.flags|= ALTER_ADMIN_PARTITION;

  res= Analyze_table_statement::execute(thd);

  DBUG_RETURN(res);
}


bool Alter_table_check_partition_statement::execute(THD *thd)
{
  bool res;
  DBUG_ENTER("Alter_table_check_partition_statement::execute");

  /*
    Flag that it is an ALTER command which administrates partitions, used
    by ha_partition
  */
  m_lex->alter_info.flags|= ALTER_ADMIN_PARTITION;

  res= Check_table_statement::execute(thd);

  DBUG_RETURN(res);
}


bool Alter_table_optimize_partition_statement::execute(THD *thd)
{
  bool res;
  DBUG_ENTER("Alter_table_optimize_partition_statement::execute");

  /*
    Flag that it is an ALTER command which administrates partitions, used
    by ha_partition
  */
  m_lex->alter_info.flags|= ALTER_ADMIN_PARTITION;

  res= Optimize_table_statement::execute(thd);

  DBUG_RETURN(res);
}


bool Alter_table_repair_partition_statement::execute(THD *thd)
{
  bool res;
  DBUG_ENTER("Alter_table_repair_partition_statement::execute");

  /*
    Flag that it is an ALTER command which administrates partitions, used
    by ha_partition
  */
  m_lex->alter_info.flags|= ALTER_ADMIN_PARTITION;

  res= Repair_table_statement::execute(thd);

  DBUG_RETURN(res);
}


bool Alter_table_truncate_partition_statement::execute(THD *thd)
{
  int error;
  ha_partition *partition;
  ulong timeout= thd->variables.lock_wait_timeout;
  TABLE_LIST *first_table= thd->lex->select_lex.table_list.first;
  bool binlog_stmt;
  DBUG_ENTER("Alter_table_truncate_partition_statement::execute");

  /*
    Flag that it is an ALTER command which administrates partitions, used
    by ha_partition.
  */
  m_lex->alter_info.flags|= ALTER_ADMIN_PARTITION |
                            ALTER_TRUNCATE_PARTITION;

  /* Fix the lock types (not the same as ordinary ALTER TABLE). */
  first_table->lock_type= TL_WRITE;
  first_table->mdl_request.set_type(MDL_EXCLUSIVE);

  /*
    Check table permissions and open it with a exclusive lock.
    Ensure it is a partitioned table and finally, upcast the
    handler and invoke the partition truncate method. Lastly,
    write the statement to the binary log if necessary.
  */

  if (check_one_table_access(thd, DROP_ACL, first_table))
    DBUG_RETURN(TRUE);

  if (open_and_lock_tables(thd, first_table, FALSE, 0))
    DBUG_RETURN(TRUE);

  /*
    TODO: Add support for TRUNCATE PARTITION for NDB and other
          engines supporting native partitioning.
  */
  if (first_table->table->s->db_type() != partition_hton)
  {
    my_error(ER_PARTITION_MGMT_ON_NONPARTITIONED, MYF(0));
    DBUG_RETURN(TRUE);
  }

  /*
    Under locked table modes this might still not be an exclusive
    lock. Hence, upgrade the lock since the handler truncate method
    mandates an exclusive metadata lock.
  */
  MDL_ticket *ticket= first_table->table->mdl_ticket;
  if (thd->mdl_context.upgrade_shared_lock_to_exclusive(ticket, timeout))
    DBUG_RETURN(TRUE);

  tdc_remove_table(thd, TDC_RT_REMOVE_NOT_OWN, first_table->db,
                   first_table->table_name, FALSE);

  partition= (ha_partition *) first_table->table->file;

  /* Invoke the handler method responsible for truncating the partition. */
  if ((error= partition->truncate_partition(&thd->lex->alter_info,
                                            &binlog_stmt)))
    first_table->table->file->print_error(error, MYF(0));

  /*
    All effects of a truncate operation are committed even if the
    operation fails. Thus, the query must be written to the binary
    log. The exception is a unimplemented truncate method or failure
    before any call to handler::truncate() is done.
    Also, it is logged in statement format, regardless of the binlog format.
  */
  if (error != HA_ERR_WRONG_COMMAND && binlog_stmt)
    error|= write_bin_log(thd, !error, thd->query(), thd->query_length());

  /*
    A locked table ticket was upgraded to a exclusive lock. After the
    the query has been written to the binary log, downgrade the lock
    to a shared one.
  */
  if (thd->locked_tables_mode)
    ticket->downgrade_exclusive_lock(MDL_SHARED_NO_READ_WRITE);

  if (! error)
    my_ok(thd);

  DBUG_RETURN(error);
}

#endif /* WITH_PARTITION_STORAGE_ENGINE */
