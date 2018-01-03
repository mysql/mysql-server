/* Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "sql/rpl_info_table.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "m_ctype.h"
#include "m_string.h"
#include "my_base.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_loglevel.h"
#include "my_sys.h"
#include "mysql/service_mysql_alloc.h"
#include "mysql/thread_type.h"
#include "mysql/udf_registration_types.h"
#include "mysqld_error.h"
#include "sql/dynamic_ids.h"        // Server_ids
#include "sql/field.h"
#include "sql/handler.h"
#include "sql/key.h"
#include "sql/log.h"
#include "sql/log_event.h"
#include "sql/psi_memory_key.h"
#include "sql/query_options.h"
#include "sql/rpl_info_table_access.h" // Rpl_info_table_access
#include "sql/rpl_info_values.h"    // Rpl_info_values
#include "sql/sql_class.h"          // THD
#include "sql/system_variables.h"
#include "sql/table.h"
#include "sql_string.h"
#include "thr_lock.h"


Rpl_info_table::Rpl_info_table(uint nparam,
                               const char* param_schema,
                               const char *param_table,
                               const uint param_n_pk_fields,
                               const uint *param_pk_field_indexes)
:Rpl_info_handler(nparam), is_transactional(FALSE)
{
  str_schema.str= str_table.str= NULL;
  str_schema.length= str_table.length= 0;

  size_t schema_length= strlen(param_schema);
  if ((str_schema.str= (char *) my_malloc(key_memory_Rpl_info_table,
                                          schema_length + 1, MYF(0))))
  {
    str_schema.length= schema_length;
    strmake(str_schema.str, param_schema, schema_length);
  }
  
  size_t table_length= strlen(param_table);
  if ((str_table.str= (char *) my_malloc(key_memory_Rpl_info_table,
                                         table_length + 1, MYF(0))))
  {
    str_table.length= table_length;
    strmake(str_table.str, param_table, table_length);
  }

  if ((description= (char *)
      my_malloc(key_memory_Rpl_info_table,
                str_schema.length + str_table.length + 2, MYF(0))))
  {
    char *pos= my_stpcpy(description, param_schema);
    pos= my_stpcpy(pos, ".");
    pos= my_stpcpy(pos, param_table);
  }

  m_n_pk_fields= param_n_pk_fields;
  m_pk_field_indexes= param_pk_field_indexes;

  access= new Rpl_info_table_access();
}

Rpl_info_table::~Rpl_info_table()
{
  delete access;
  
  my_free(description);

  my_free(str_table.str);

  my_free(str_schema.str);
}

int Rpl_info_table::do_init_info()
{
  return do_init_info(FIND_KEY, 0);
}

int Rpl_info_table::do_init_info(uint instance)
{
  return do_init_info(FIND_KEY, instance);
}

int Rpl_info_table::do_init_info(enum_find_method method, uint instance)
{
  int error= 1;
  enum enum_return_id res= FOUND_ID;
  TABLE *table= NULL;
  sql_mode_t saved_mode;
  Open_tables_backup backup;

  DBUG_ENTER("Rlp_info_table::do_init_info");

  THD *thd= access->create_thd();

  saved_mode= thd->variables.sql_mode;
  ulonglong saved_options= thd->variables.option_bits;
  thd->variables.option_bits &= ~OPTION_BIN_LOG;

  /*
    Opens and locks the rpl_info table before accessing it.
  */
  if (access->open_table(thd, str_schema, str_table,
                         get_number_info(), TL_WRITE,
                         &table, &backup))
    goto end;

  if (verify_table_primary_key_fields(table))
    goto end;

  /*
    Points the cursor at the row to be read according to the
    keys.
  */
  switch (method)
  {
    case FIND_KEY:
      res= access->find_info(field_values, table);
    break;

    case FIND_SCAN:
      res= access->scan_info(table, instance);
    break;

    default:
      DBUG_ASSERT(0);
    break;
  }

  if (res == FOUND_ID)
  {
    /*
      Reads the information stored in the rpl_info table into a
      set of variables. If there is a failure, an error is returned.
    */
    if (access->load_info_values(get_number_info(), table->field,
                                 field_values))
      goto end;
  }
  error= (res == ERROR_ID);
end:
  /*
    Unlocks and closes the rpl_info table.
  */
  access->close_table(thd, table, &backup, error);
  thd->variables.sql_mode= saved_mode;
  thd->variables.option_bits= saved_options;
  access->drop_thd(thd);
  DBUG_RETURN(error);
}

int Rpl_info_table::do_flush_info(const bool force)
{
  int error= 1;
  enum enum_return_id res= FOUND_ID;
  TABLE *table= NULL;
  sql_mode_t saved_mode;
  Open_tables_backup backup;

  DBUG_ENTER("Rpl_info_table::do_flush_info");

  if (!(force || (sync_period &&
      ++(sync_counter) >= sync_period)))
    DBUG_RETURN(0);

  THD *thd= access->create_thd();

  sync_counter= 0;
  saved_mode= thd->variables.sql_mode;
  ulonglong saved_options= thd->variables.option_bits;
  thd->variables.option_bits &= ~OPTION_BIN_LOG;
  thd->is_operating_substatement_implicitly= true;

  /*
    Opens and locks the rpl_info table before accessing it.
  */
  if (access->open_table(thd, str_schema, str_table,
                         get_number_info(), TL_WRITE,
                         &table, &backup))
    goto end;

  /*
    Points the cursor at the row to be read according to the
    keys. If the row is not found an error is reported.
  */
  if ((res= access->find_info(field_values, table)) == NOT_FOUND_ID)
  {
    /*
      Prepares the information to be stored before calling ha_write_row.
    */
    empty_record(table);
    if (access->store_info_values(get_number_info(), table->field,
                                  field_values))
      goto end;

    /*
      Inserts a new row into rpl_info table.
    */
    if ((error= table->file->ha_write_row(table->record[0])))
    {
      table->file->print_error(error, MYF(0));
      /*
        This makes sure that the error is 1 and not the status returned
        by the handler.
      */
      error= 1;
      goto end;
    }
    error= 0;
  }
  else if (res == FOUND_ID)
  {
    /*
      Prepares the information to be stored before calling ha_update_row.
    */
    store_record(table, record[1]);
    if (access->store_info_values(get_number_info(), table->field,
                                  field_values))
      goto end;
 
    /*
      Updates a row in the rpl_info table.
    */
    if ((error= table->file->ha_update_row(table->record[1], table->record[0])) &&
        error != HA_ERR_RECORD_IS_THE_SAME)
    {
      table->file->print_error(error, MYF(0));
      /*
        This makes sure that the error is 1 and not the status returned
        by the handler.
      */
      error= 1;
      goto end;
    }
    error= 0;
  }

end:
  DBUG_EXECUTE_IF("mts_debug_concurrent_access",
    {
      while (thd->system_thread == SYSTEM_THREAD_SLAVE_WORKER &&
             mts_debug_concurrent_access < 2 && mts_debug_concurrent_access >  0)
      {
        DBUG_PRINT("mts", ("Waiting while locks are acquired to show "
                           "concurrency in mts: %u %u\n",
                           mts_debug_concurrent_access,
                           thd->thread_id()));
        my_sleep(6000000);
      }
    };
  );

  /*
    Unlocks and closes the rpl_info table.
  */
  access->close_table(thd, table, &backup, error);
  thd->is_operating_substatement_implicitly= false;
  thd->variables.sql_mode= saved_mode;
  thd->variables.option_bits= saved_options;
  access->drop_thd(thd);
  DBUG_RETURN(error);
}

int Rpl_info_table::do_remove_info()
{
  return do_clean_info();
}

int Rpl_info_table::do_clean_info()
{
  int error= 1;
  enum enum_return_id res= FOUND_ID;
  TABLE *table= NULL;
  sql_mode_t saved_mode;
  Open_tables_backup backup;

  DBUG_ENTER("Rpl_info_table::do_remove_info");

  THD *thd= access->create_thd();

  saved_mode= thd->variables.sql_mode;
  ulonglong saved_options= thd->variables.option_bits;
  thd->variables.option_bits &= ~OPTION_BIN_LOG;

  /*
    Opens and locks the rpl_info table before accessing it.
  */
  if (access->open_table(thd, str_schema, str_table,
                         get_number_info(), TL_WRITE,
                         &table, &backup))
    goto end;

  /*
    Points the cursor at the row to be deleted according to the
    keys. If the row is not found, the execution proceeds normally.
  */
  if ((res= access->find_info(field_values, table)) == FOUND_ID)
  {
    /*
      Deletes a row in the rpl_info table.
    */
    if ((error= table->file->ha_delete_row(table->record[0])))
    {
      table->file->print_error(error, MYF(0));
      goto end;
    }
  }
  error= (res == ERROR_ID);
end:
  /*
    Unlocks and closes the rpl_info table.
  */
  access->close_table(thd, table, &backup, error);
  thd->variables.sql_mode= saved_mode;
  thd->variables.option_bits= saved_options;
  access->drop_thd(thd);
  DBUG_RETURN(error);
}

/**
   Removes records belonging to the channel_name parameter's channel.

   @param nparam             number of fields in the table
   @param param_schema       schema name
   @param param_table        table name
   @param channel_name       channel name

   @return 0   on success
           1   when a failure happens
*/
int Rpl_info_table::do_reset_info(uint nparam,
                                  const char* param_schema,
                                  const char *param_table,
                                  const char *channel_name)
{
  int error= 0;
  TABLE *table= NULL;
  sql_mode_t saved_mode;
  Open_tables_backup backup;
  Rpl_info_table *info= NULL;
  THD *thd= NULL;
  int handler_error= 0;

  DBUG_ENTER("Rpl_info_table::do_reset_info");

  if (!(info= new Rpl_info_table(nparam, param_schema,
                                 param_table)))
    DBUG_RETURN(1);

  thd= info->access->create_thd();
  saved_mode= thd->variables.sql_mode;
  ulonglong saved_options= thd->variables.option_bits;
  thd->variables.option_bits &= ~OPTION_BIN_LOG;

  /*
    Opens and locks the rpl_info table before accessing it.
  */
  if (info->access->open_table(thd, info->str_schema, info->str_table,
                               info->get_number_info(), TL_WRITE,
                               &table, &backup))
  {
    error= 1;
    goto end;
  }

  if (!(handler_error= table->file->ha_index_init(0, 1)))
  {
    KEY *key_info= table->key_info;

    /*
      Currently this method is used only for Worker info table
      resetting.
      todo: for another table in future, consider to make use of the
      passed parameter to locate the lookup key.
    */
    DBUG_ASSERT(strcmp(info->str_table.str, "slave_worker_info") == 0);

    if (info->verify_table_primary_key_fields(table))
    {
      error= 1;
      table->file->ha_index_end();
      goto end;
    }

    uint fieldnr= key_info->key_part[0].fieldnr - 1;
    table->field[fieldnr]->store(channel_name,
                                 strlen(channel_name),
                                 &my_charset_bin);
    uint key_len= key_info->key_part[0].store_length;
    uchar *key_buf= table->field[fieldnr]->ptr;

    if (!(handler_error= table->file->ha_index_read_map(table->record[0],
                                                        key_buf,
                                                        (key_part_map) 1,
                                                        HA_READ_KEY_EXACT)))
    {
      do
      {
        if ((handler_error= table->file->ha_delete_row(table->record[0])))
          break;
      }
      while (!(handler_error= table->file->ha_index_next_same(table->record[0],
                                                           key_buf,
                                                           key_len)));
      if (handler_error != HA_ERR_END_OF_FILE)
        error= 1;
    }
    else
    {
      /*
        Being reset table can be even empty, and that's benign.
      */
      if (handler_error != HA_ERR_KEY_NOT_FOUND)
        error= 1;
    }

    if (error)
      table->file->print_error(handler_error, MYF(0));
    table->file->ha_index_end();
  }
end:
  /*
    Unlocks and closes the rpl_info table.
  */
  info->access->close_table(thd, table, &backup, error);
  thd->variables.sql_mode= saved_mode;
  thd->variables.option_bits= saved_options;
  info->access->drop_thd(thd);
  delete info;
  DBUG_RETURN(error);
}

enum_return_check Rpl_info_table::do_check_info()
{
  TABLE *table= NULL;
  sql_mode_t saved_mode;
  Open_tables_backup backup;
  enum_return_check return_check= ERROR_CHECKING_REPOSITORY;

  DBUG_ENTER("Rpl_info_table::do_check_info");

  THD *thd= access->create_thd();
  saved_mode= thd->variables.sql_mode;

  /*
    Opens and locks the rpl_info table before accessing it.
  */
  if (access->open_table(thd, str_schema, str_table,
                         get_number_info(), TL_READ,
                         &table, &backup))
  {
    LogErr(WARNING_LEVEL, ER_RPL_CANT_OPEN_INFO_TABLE,
           str_schema.str, str_table.str);

    return_check= ERROR_CHECKING_REPOSITORY;
    goto end;
  }

  /*
    Points the cursor at the row to be read according to the
    keys.
  */
  if (access->find_info(field_values, table) != FOUND_ID)
  {
    /* 
       We cannot simply call my_error here because it does not
       really means that there was a failure but only that the
       record was not found.
    */
    return_check= REPOSITORY_DOES_NOT_EXIST;
    goto end;
  }
  return_check= REPOSITORY_EXISTS;


end:
  /*
    Unlocks and closes the rpl_info table.
  */
  access->close_table(thd, table, &backup,
                      return_check == ERROR_CHECKING_REPOSITORY);
  thd->variables.sql_mode= saved_mode;
  access->drop_thd(thd);
  DBUG_RETURN(return_check);
}

enum_return_check Rpl_info_table::do_check_info(uint instance)
{
  TABLE *table= NULL;
  sql_mode_t saved_mode;
  Open_tables_backup backup;
  enum_return_check return_check= ERROR_CHECKING_REPOSITORY;

  DBUG_ENTER("Rpl_info_table::do_check_info");

  THD *thd= access->create_thd();
  saved_mode= thd->variables.sql_mode;

  /*
    Opens and locks the rpl_info table before accessing it.
  */
  if (access->open_table(thd, str_schema, str_table,
                         get_number_info(), TL_READ,
                         &table, &backup))
  {
    LogErr(WARNING_LEVEL, ER_RPL_CANT_OPEN_INFO_TABLE,
           str_schema.str, str_table.str);

    return_check= ERROR_CHECKING_REPOSITORY;
    goto end;
  }

  if (verify_table_primary_key_fields(table))
  {
    return_check= ERROR_CHECKING_REPOSITORY;
    goto end;
  }

  /*
    Points the cursor at the row to be read according to the
    keys.
  */
  if (access->scan_info(table, instance) != FOUND_ID)
  {
    /* 
       We cannot simply call my_error here because it does not
       really means that there was a failure but only that the
       record was not found.
    */
    return_check= REPOSITORY_DOES_NOT_EXIST;
    goto end;
  }
  return_check= REPOSITORY_EXISTS;


end:
  /*
    Unlocks and closes the rpl_info table.
  */
  access->close_table(thd, table, &backup,
                      return_check == ERROR_CHECKING_REPOSITORY);
  thd->variables.sql_mode= saved_mode;
  access->drop_thd(thd);
  DBUG_RETURN(return_check);
}

bool Rpl_info_table::do_count_info(uint nparam,
                                   const char* param_schema,
                                   const char *param_table,
                                   uint* counter)
{
  int error= 1;
  TABLE *table= NULL;
  sql_mode_t saved_mode;
  Open_tables_backup backup;
  Rpl_info_table *info= NULL;
  THD *thd= NULL;
   
  DBUG_ENTER("Rpl_info_table::do_count_info");

  if (!(info= new Rpl_info_table(nparam, param_schema, param_table)))
    DBUG_RETURN(true);

  thd= info->access->create_thd();
  saved_mode= thd->variables.sql_mode;

  /*
    Opens and locks the rpl_info table before accessing it.
  */
  if (info->access->open_table(thd, info->str_schema, info->str_table,
                               info->get_number_info(), TL_READ,
                               &table, &backup))
  {
    /*
      We cannot simply print out a warning message at this
      point because this may represent a bootstrap.
    */
    error= 0;
    goto end;
  }

  /*
    Counts entries in the rpl_info table.
  */
  if (info->access->count_info(table, counter))
  {
    LogErr(WARNING_LEVEL, ER_RPL_CANT_SCAN_INFO_TABLE,
           info->str_schema.str, info->str_table.str);
    goto end;
  }
  error= 0;

end:
  /*
    Unlocks and closes the rpl_info table.
  */
  info->access->close_table(thd, table, &backup, error);
  thd->variables.sql_mode= saved_mode;
  info->access->drop_thd(thd);
  delete info;
  DBUG_RETURN(error);
}

void Rpl_info_table::do_end_info()
{
}

int Rpl_info_table::do_prepare_info_for_read()
{
  if (!field_values)
    return TRUE;

  cursor= 0;
  prv_error= FALSE; 

  return FALSE;
}

int Rpl_info_table::do_prepare_info_for_write()
{
  return(do_prepare_info_for_read());
}

uint Rpl_info_table::do_get_rpl_info_type()
{
  return INFO_REPOSITORY_TABLE;
}

bool Rpl_info_table::do_set_info(const int pos, const char *value)
{
  return (field_values->value[pos].copy(value, strlen(value),
                                        &my_charset_bin));
}

bool Rpl_info_table::do_set_info(const int pos, const uchar *value,
                                 const size_t size)
{
  return (field_values->value[pos].copy((char *) value, size,
                                        &my_charset_bin));
}

bool Rpl_info_table::do_set_info(const int pos, const ulong value)
{
  return (field_values->value[pos].set_int(value, TRUE,
                                           &my_charset_bin));
}

bool Rpl_info_table::do_set_info(const int pos, const int value)
{
  return (field_values->value[pos].set_int(value, FALSE,
                                           &my_charset_bin));
}

bool Rpl_info_table::do_set_info(const int pos, const float value)
{
  return (field_values->value[pos].set_real(value, NOT_FIXED_DEC,
                                            &my_charset_bin));
}

bool Rpl_info_table::do_set_info(const int pos, const Server_ids *value)
{
  if (const_cast<Server_ids*>(value)->pack_dynamic_ids(&field_values->value[pos]))
    return TRUE;

  return FALSE;
}

bool Rpl_info_table::do_get_info(const int pos, char *value, const size_t,
                                 const char *default_value)
{
  if (field_values->value[pos].length())
    strmake(value, field_values->value[pos].c_ptr_safe(),
            field_values->value[pos].length());
  else if (default_value)
    strmake(value, default_value, strlen(default_value));
  else
    *value= '\0';

  return FALSE;
}

bool Rpl_info_table::do_get_info(const int pos, uchar *value, const size_t size,
                                 const uchar *default_value MY_ATTRIBUTE((unused)))
{
  if (field_values->value[pos].length() == size)
    return (!memcpy((char *) value,
            field_values->value[pos].c_ptr_safe(), size));
  return TRUE;
}

bool Rpl_info_table::do_get_info(const int pos, ulong *value,
                                 const ulong default_value)
{
  if (field_values->value[pos].length())
  {
    *value= strtoul(field_values->value[pos].c_ptr_safe(), 0, 10);
    return FALSE;
  }
  else if (default_value)
  {
    *value= default_value;
    return FALSE;
  }

  return TRUE;
}

bool Rpl_info_table::do_get_info(const int pos, int *value,
                                 const int default_value)
{
  if (field_values->value[pos].length())
  {
    *value=  atoi(field_values->value[pos].c_ptr_safe());
    return FALSE;
  }
  else if (default_value)
  {
    *value= default_value;
    return FALSE;
  }

  return TRUE;
}

bool Rpl_info_table::do_get_info(const int pos, float *value,
                                 const float default_value)
{
  if (field_values->value[pos].length())
  {
    if (sscanf(field_values->value[pos].c_ptr_safe(), "%f", value) != 1)
      return TRUE;
    return FALSE;
  }
  else if (default_value != 0.0)
  {
    *value= default_value;
    return FALSE;
  }

  return TRUE;
}

bool Rpl_info_table::do_get_info(const int pos, Server_ids *value,
                                 const Server_ids *default_value MY_ATTRIBUTE((unused)))
{
  if (value->unpack_dynamic_ids(field_values->value[pos].c_ptr_safe()))
    return TRUE;

  return FALSE;
}

char* Rpl_info_table::do_get_description_info()
{
  return description;
}

bool Rpl_info_table::do_is_transactional()
{
  return is_transactional;
}

bool Rpl_info_table::do_update_is_transactional()
{
  bool error= TRUE;
  sql_mode_t saved_mode;
  TABLE *table= NULL;
  Open_tables_backup backup;

  DBUG_ENTER("Rpl_info_table::do_update_is_transactional");
  DBUG_EXECUTE_IF("simulate_update_is_transactional_error",
                  {
                    DBUG_RETURN(TRUE);
                  });

  THD *thd= access->create_thd();
  saved_mode= thd->variables.sql_mode;
  ulonglong saved_options= thd->variables.option_bits;
  thd->variables.option_bits &= ~OPTION_BIN_LOG;

  /*
    Opens and locks the rpl_info table before accessing it.
  */
  if (access->open_table(thd, str_schema, str_table,
                         get_number_info(), TL_READ,
                         &table, &backup))
    goto end;

  is_transactional= table->file->has_transactions();
  error= FALSE;

end:
  access->close_table(thd, table, &backup, 0);
  thd->variables.sql_mode= saved_mode;
  thd->variables.option_bits= saved_options;
  access->drop_thd(thd);
  DBUG_RETURN(error);
}

bool Rpl_info_table::verify_table_primary_key_fields(TABLE *table)
{
  DBUG_ENTER("Rpl_info_table::verify_table_primary_key_fields");
  KEY *key_info= table->key_info;
  bool error;

  /*
    If the table has no keys or has less key fields than expected,
    it must be corrupted.
  */
  if ((error= !key_info ||
              key_info->user_defined_key_parts == 0 ||
              (m_n_pk_fields > 0 &&
               key_info->user_defined_key_parts != m_n_pk_fields)))
  {
    LogErr(ERROR_LEVEL, ER_RPL_CORRUPTED_INFO_TABLE,
           str_schema.str, str_table.str);
  }

  if (!error && m_n_pk_fields && m_pk_field_indexes)
  {
    /*
      If any of its primary key fields are not at the expected position,
      the table must be corrupted.
    */
    for (uint idx= 0; idx < m_n_pk_fields; idx++)
    {
      if (key_info->key_part[idx].field != table->field[m_pk_field_indexes[idx]])
      {
        const char *key_field_name= key_info->key_part[idx].field->field_name;
        const char *table_field_name= table->field[m_pk_field_indexes[idx]]->field_name;
        LogErr(ERROR_LEVEL, ER_RPL_CORRUPTED_KEYS_IN_INFO_TABLE,
               str_schema.str, str_table.str,
               m_pk_field_indexes[idx],
               key_field_name,
               table_field_name);
        error= true;
        break;
      }
    }
  }

  DBUG_RETURN(error);
}
