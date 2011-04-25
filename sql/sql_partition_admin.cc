/* Copyright (c) 2010 Oracle and/or its affiliates. All rights reserved.

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
#include "sql_truncate.h"                   // mysql_truncate_table,
                                            // Truncate_statement
#include "sql_admin.h"                      // Analyze/Check/.._table_statement
#include "sql_partition_admin.h"            // Alter_table_*_partition

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
  TABLE_LIST *first_table= thd->lex->select_lex.table_list.first;
  bool res;
  enum_sql_command original_sql_command;
  DBUG_ENTER("Alter_table_truncate_partition_statement::execute");

  /*
    Execute TRUNCATE PARTITION just like TRUNCATE TABLE.
    Some storage engines (InnoDB, partition) checks thd_sql_command,
    so we set it to SQLCOM_TRUNCATE during the execution.
  */
  original_sql_command= m_lex->sql_command;
  m_lex->sql_command= SQLCOM_TRUNCATE;
  
  /*
    Flag that it is an ALTER command which administrates partitions, used
    by ha_partition.
  */
  m_lex->alter_info.flags|= ALTER_ADMIN_PARTITION;
   
  /*
    Fix the lock types (not the same as ordinary ALTER TABLE).
  */
  first_table->lock_type= TL_WRITE;
  first_table->mdl_request.set_type(MDL_SHARED_NO_READ_WRITE);

  /* execute as a TRUNCATE TABLE */
  res= Truncate_statement::execute(thd);

  m_lex->sql_command= original_sql_command;
  DBUG_RETURN(res);
}

#endif /* WITH_PARTITION_STORAGE_ENGINE */
