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
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "rpl_info_table_access.h"
#include "rpl_utility.h"
#include "handler.h"
#include "sql_parse.h"

/**
  Constructor of the Rpl_info_table_access class.
 */
Rpl_info_table_access::Rpl_info_table_access()
{
  init_sql_alloc(&mem_root, 256, 512);
}

/**
  Destructor of the Rpl_info_table_access class.
 */
Rpl_info_table_access::~Rpl_info_table_access()
{
  free_root(&mem_root, MYF(0));
}

/**
  Opens and locks a table.

  It's assumed that the caller knows what they are doing:
  - whether it was necessary to reset-and-backup the open tables state
  - whether the requested lock does not lead to a deadlock
  - whether this open mode would work under LOCK TABLES, or inside a
  stored function or trigger.

  Note that if the table can't be locked successfully this operation will
  close it. Therefore it provides guarantee that it either opens and locks
  table or fails without leaving any tables open.

  @param[in]  thd           Thread requesting to open the table
  @param[in]  dbstr         Database where the table resides
  @param[in]  dbstr_size    Size of the string that represents the database
  @param[in]  tbstr         Table to be openned
  @param[in]  tbstr_size    Size of the string that represents the table
  @param[in]  max_num_field Maximum number of fields
  @param[in]  lock_type     How to lock the table
  @param[out] table         We will store the open table here
  @param[out] backup        Save the lock info. here

  @return
    @retval TRUE open and lock failed - an error message is pushed into the
                                        stack
    @retval FALSE success
*/
bool Rpl_info_table_access::open_table(THD* thd, const char *dbstr,
                                       size_t dbstr_size, const char *tbstr,
                                       size_t tbstr_size, uint max_num_field,
                                       enum thr_lock_type lock_type,
                                       TABLE** table,
                                       Open_tables_backup* backup)
{
  TABLE_LIST tables;
  DBUG_ENTER("Master_info_table::open_table");

  /*
    This is equivalent to a new "statement". For that reason, we call both
    lex_start() and mysql_reset_thd_for_next_command. Note that the calls
    may reset the value of current_stmt_binlog_format. So we need  to save
    the value outside the function and restore it after executing the new
    "statement".
  */
 
  if (thd->slave_thread || !current_thd)
  { 
    lex_start(thd);
    mysql_reset_thd_for_next_command(thd);
  }

  thd->reset_n_backup_open_tables_state(backup);

  tables.init_one_table(dbstr, dbstr_size, tbstr, tbstr_size, tbstr, lock_type);

  if (!open_n_lock_single_table(thd, &tables, lock_type, 0))
  {
    close_thread_tables(thd);
    my_error(ER_NO_SUCH_TABLE, MYF(0), dbstr, tbstr);
    DBUG_RETURN(TRUE);
  }

  if (tables.table->s->fields < max_num_field)
  {
    /*
      Safety: this can only happen if someone started the server and then
      altered the table.
    */
    my_error(ER_COL_COUNT_DOESNT_MATCH_CORRUPTED_V2, MYF(0),
             tables.table->alias, max_num_field,
             tables.table->s->fields);
    close_thread_tables(thd);
    DBUG_RETURN(TRUE);
  }

  *table= tables.table;
  tables.table->use_all_columns();
  DBUG_RETURN(FALSE);
}

/**
  Unlocks and closes a table.

  @param[in] thd    Thread requesting to close the table
  @param[in] table  Table to be closed
  @param[in] backup Restore the lock info from here

  This method needs to be called even if the open_table fails,
  in order to ensure the lock info is properly restored.

  @return
    @retval FALSE No error
    @retval TRUE  Failure
*/
bool Rpl_info_table_access::close_table(THD *thd, TABLE* table,
                                        Open_tables_backup *backup)
{
  DBUG_ENTER("Rpl_info_table_access::close_table");

  if (table)
  {
    close_thread_tables(thd);
    thd->restore_backup_open_tables_state(backup);
  }

  DBUG_RETURN(FALSE);
}

/**
  Positions the internal pointer of `table` to the place where (id)
  is stored.

  In case search succeeded, the table cursor points to the found row.

  @param[in]      server_id    Server id
  @param[in]      idx          Index field
  @param[in,out]  field_values The sequence of values
  @param[in,out]  table        Table

  @return
    @retval FOUND     The row was found.
    @retval NOT_FOUND The row was not found.
    @retval ERROR     There was a failure.
*/
enum enum_return_id Rpl_info_table_access::find_info_id(ulong server_id,
                                                        uint idx,
                                                        Rpl_info_fields *field_values,
                                                        TABLE *table)
{
  uchar key[MAX_KEY_LENGTH];
  DBUG_ENTER("Rpl_info_table_access::find_info_id");

  longlong2str(server_id, field_values->field[idx].use.str, 10);
  field_values->field[idx].use.length= strlen(field_values->field[idx].use.str);

  if (field_values->field[idx].use.length > table->field[idx]->field_length)
    DBUG_RETURN(ERROR_ID);

  table->field[idx]->store(field_values->field[idx].use.str,
                           field_values->field[idx].use.length,
                           &my_charset_bin);

  if (!(table->field[idx]->flags & PRI_KEY_FLAG))
    DBUG_RETURN(ERROR_ID);

  key_copy(key, table->record[0], table->key_info, table->key_info->key_length);

  if (table->file->index_read_idx_map(table->record[0], 0, key, HA_WHOLE_KEY,
                                      HA_READ_KEY_EXACT))
  {
    DBUG_RETURN(NOT_FOUND_ID);
  }

  DBUG_RETURN(FOUND_ID);
}

/**
  Reads information from a sequence of fields into a set of LEX_STRING
  structures. The additional parameters must be specified as a pair:
  (i) field number and (ii) LEX_STRING structure. The last additional
  parameter  must be equal to the first regular parameter in order to
  identify when one must stop reading fields.
   
  @param[in] max_num_field Maximum number of fields
  @param[in] fields        The sequence of fields

  @return
    @retval FALSE No error
    @retval TRUE  Failure
 */
bool Rpl_info_table_access::load_info_fields(uint max_num_field, Field **fields, ...)
{
  va_list args;
  uint field_idx;
  LEX_STRING *field_value;

  DBUG_ENTER("Rpl_info_table_access::load_info_fields");

  va_start(args, fields);
  field_idx= (uint) va_arg(args, int);
  while (field_idx < max_num_field)
  {
    field_value= va_arg(args, LEX_STRING *);
    field_value->str= get_field(&mem_root, fields[field_idx]);
    field_value->length= field_value->str ? strlen(field_value->str) : 0;
    field_idx= (uint) va_arg(args, int);
  }
  va_end(args);

  DBUG_RETURN(FALSE);
}

/**
  Stores information from a set of LEX_STRING structures into a sequence
  of fields. The additional parameters must be specified as a pair:
  (i) field number and (i) LEX_STRING structure. The last additional
  parameter must be equal to the first regular parameter in order to
  identify when one must stop reading LEX_STRING structures.
   
  @param[in] max_num_field Maximum number of fields
  @param[in] fields        The sequence of fields

  @return
    @retval FALSE No error
    @retval TRUE  Failure
 */
bool Rpl_info_table_access::store_info_fields(uint max_num_field, Field **fields, ...)
{
  va_list args;
  uint field_idx;
  LEX_STRING *field_value;

  DBUG_ENTER("Rpl_info_table_access::load_info_fields");

  va_start(args, fields);
  field_idx= (uint) va_arg(args, int);
  while (field_idx < max_num_field)
  {
    field_value= va_arg(args, LEX_STRING *);
    fields[field_idx]->set_notnull();
    if (fields[field_idx]->store(field_value->str,
                                 field_value->length,
                                 &my_charset_bin))
    {
      my_error(ER_INFO_DATA_TOO_LONG, MYF(0),
               fields[field_idx]->field_name);
      DBUG_RETURN(TRUE);
    }
    field_idx= (uint) va_arg(args, int);
  }
  va_end(args);

  DBUG_RETURN(FALSE);
}

/**
  Reads information from a sequence of fields into a set of LEX_STRING
  structures, where the sequence of values is specified through the object
  Rpl_info_fields.

  @param[in] max_num_field Maximum number of fields
  @param[in] fields        The sequence of fields
  @param[in] field_values  The sequence of values

  @return
    @retval FALSE No error
    @retval TRUE  Failure
 */
bool Rpl_info_table_access::load_info_fields(uint max_num_field, Field **fields,
                                             Rpl_info_fields *field_values)
{
  DBUG_ENTER("Rpl_info_table_access::load_info_fields");
  uint field_idx= 0;

  while (field_idx < max_num_field)
  {
    field_values->field[field_idx].use.str= 
      get_field(&mem_root, fields[field_idx]);
    field_values->field[field_idx].use.length= 
      (field_values->field[field_idx].use.str ?
       strlen(field_values->field[field_idx].use.str) : 0);
    field_idx++;
  }

  DBUG_RETURN(FALSE);
}

/**
  Stores information from a sequence of fields into a set of LEX_STRING
  structures, where the sequence of values is specified through the object
  Rpl_info_fields.

  @param[in] max_num_field Maximum number of fields
  @param[in] fields        The sequence of fields
  @param[in] field_values  The sequence of values

  @return
    @retval FALSE No error
    @retval TRUE  Failure
 */
bool Rpl_info_table_access::store_info_fields(uint max_num_field, Field **fields,
                                              Rpl_info_fields *field_values)
{
  DBUG_ENTER("Rpl_info_table_access::store_info_fields");
  uint field_idx= 0;

  while (field_idx < max_num_field)
  {
    DBUG_PRINT("info", ("store %s %d\n", field_values->field[field_idx].use.str, 
            field_idx));
    fields[field_idx]->set_notnull();
    if (fields[field_idx]->store(field_values->field[field_idx].use.str,
                                 field_values->field[field_idx].use.length,
                                 &my_charset_bin))
    {
      my_error(ER_INFO_DATA_TOO_LONG, MYF(0),
               fields[field_idx]->field_name);
      DBUG_RETURN(TRUE);
    }
    field_idx++;
  }

  DBUG_RETURN(FALSE);
}

/**
  Creates a new thread if necessary, saves and set the system_thread
  information. In the bootstrap process or in the mysqld startup, a
  thread is created in order to be able to access a table. Otherwise,
  the current thread is used.

  @return
    @retval THD* Pointer to thread structure
*/
THD *Rpl_info_table_access::create_fake_thd()
{
  THD *thd= NULL;
  saved_current_thd= current_thd;

  if (!current_thd)
  {
    thd= new THD;
    thd->thread_stack= (char*) &thd;
    thd->store_globals();
  }
  else
  {
    thd= current_thd;
  }

  saved_thd_type= thd->system_thread;
  thd->system_thread= SYSTEM_THREAD_INFO;

  return(thd);
}

/**
  Destroys the created thread if necessary and does the following actions:

  - Restores the system_thread information. 
  - If there is an error, rolls back the current statement. Otherwise,
  commits it. 
  - If a new thread was created and there is an error, the transaction
  must be rolled back. Otherwise, it must be committed. In this case,
  the changes were not done on behalf of any user transaction and if
  not finished, there would be pending changes. 

  @return
    @retval THD* Pointer to thread structure
*/
bool Rpl_info_table_access::drop_fake_thd(THD *thd, bool error)
{
  DBUG_ENTER("Rpl_info::drop_fake_thd");

  thd->system_thread= saved_thd_type;

  if (error)
    ha_rollback_trans(thd, FALSE);
  else
    ha_commit_trans(thd, FALSE);

  if (saved_current_thd != current_thd)
  {
    if (error)
      ha_rollback_trans(thd, TRUE);
    else
      ha_commit_trans(thd, TRUE);
    
    delete thd;
    my_pthread_setspecific_ptr(THR_THD,  NULL);
  }

  DBUG_RETURN(FALSE);
}
