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

#include "rpl_info_table.h"
#include "rpl_utility.h"

Rpl_info_table::Rpl_info_table(uint nparam,
                               const char* param_schema,
                               const char *param_table)
:Rpl_info_handler(nparam), is_transactional(FALSE)
{
  str_schema.str= str_table.str= NULL;
  str_schema.length= str_table.length= 0;

  uint schema_length= strlen(param_schema);
  if ((str_schema.str= (char *) my_malloc(schema_length + 1, MYF(0))))
  {
    str_schema.length= schema_length;
    strmake(str_schema.str, param_schema, schema_length);
  }
  
  uint table_length= strlen(param_table);
  if ((str_table.str= (char *) my_malloc(table_length + 1, MYF(0))))
  {
    str_table.length= table_length;
    strmake(str_table.str, param_table, table_length);
  }

  if ((description= (char *)
      my_malloc(str_schema.length + str_table.length + 2, MYF(0))))
  {
    char *pos= strmov(description, param_schema);
    pos= strmov(pos, ".");
    pos= strmov(pos, param_table);
  }

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
  return do_init_info(FIND_SCAN, instance);
}

int Rpl_info_table::do_init_info(enum_find_method method, uint instance)
{
  int error= 1;
  enum enum_return_id res= FOUND_ID;
  TABLE *table= NULL;
  ulong saved_mode;
  Open_tables_backup backup;

  DBUG_ENTER("Rlp_info_table::do_init_info");

  THD *thd= access->create_thd();

  saved_mode= thd->variables.sql_mode;
  tmp_disable_binlog(thd);

  /*
    Opens and locks the rpl_info table before accessing it.
  */
  if (access->open_table(thd, str_schema, str_table,
                         get_number_info(), TL_WRITE,
                         &table, &backup))
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
  reenable_binlog(thd);
  thd->variables.sql_mode= saved_mode;
  access->drop_thd(thd);
  DBUG_RETURN(error);
}

int Rpl_info_table::do_flush_info(const bool force)
{
  int error= 1;
  enum enum_return_id res= FOUND_ID;
  TABLE *table= NULL;
  ulong saved_mode;
  Open_tables_backup backup;

  DBUG_ENTER("Rpl_info_table::do_flush_info");

  if (!(force || (sync_period &&
      ++(sync_counter) >= sync_period)))
    DBUG_RETURN(0);

  THD *thd= access->create_thd();

  sync_counter= 0;
  saved_mode= thd->variables.sql_mode;
  tmp_disable_binlog(thd);

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
          "concurrency in mts: %u %lu\n", mts_debug_concurrent_access,
          (ulong) thd->thread_id));
        my_sleep(6000000);
      }
    };
  );

  /*
    Unlocks and closes the rpl_info table.
  */
  access->close_table(thd, table, &backup, error);
  reenable_binlog(thd);
  thd->variables.sql_mode= saved_mode;
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
  ulong saved_mode;
  Open_tables_backup backup;

  DBUG_ENTER("Rpl_info_table::do_remove_info");

  THD *thd= access->create_thd();

  saved_mode= thd->variables.sql_mode;
  tmp_disable_binlog(thd);

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
  reenable_binlog(thd);
  thd->variables.sql_mode= saved_mode;
  access->drop_thd(thd);
  DBUG_RETURN(error);
}

int Rpl_info_table::do_reset_info(uint nparam,
                                  const char* param_schema,
                                  const char *param_table)
{
  int error= 1;
  TABLE *table= NULL;
  ulong saved_mode;
  Open_tables_backup backup;
  Rpl_info_table *info= NULL;
  THD *thd= NULL;

  DBUG_ENTER("Rpl_info_table::do_reset_info");

  if (!(info= new Rpl_info_table(nparam, param_schema,
                                 param_table)))
    DBUG_RETURN(error);

  thd= info->access->create_thd();
  saved_mode= thd->variables.sql_mode;
  tmp_disable_binlog(thd);

  /*
    Opens and locks the rpl_info table before accessing it.
  */
  if (info->access->open_table(thd, info->str_schema, info->str_table,
                               info->get_number_info(), TL_WRITE,
                               &table, &backup))
    goto end;

  /*
    Deletes a row in the rpl_info table.
  */
  if ((error= table->file->truncate()))
  {
     table->file->print_error(error, MYF(0));
     goto end;
  }

  error= 0;

end:
  /*
    Unlocks and closes the rpl_info table.
  */
  info->access->close_table(thd, table, &backup, error);
  reenable_binlog(thd);
  thd->variables.sql_mode= saved_mode;
  info->access->drop_thd(thd);
  delete info;
  DBUG_RETURN(error);
}

enum_return_check Rpl_info_table::do_check_info()
{
  TABLE *table= NULL;
  ulong saved_mode;
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
    sql_print_warning("Info table is not ready to be used. Table "
                      "'%s.%s' cannot be opened.", str_schema.str,
                      str_table.str);

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

enum_return_check Rpl_info_table::do_check_info(uint instance,
                                                const bool ignore_error)
{
  TABLE *table= NULL;
  ulong saved_mode;
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
    sql_print_warning("Info table is not ready to be used. Table "
                      "'%s.%s' cannot be opened.", str_schema.str,
                      str_table.str);

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
  if (ERROR_CHECKING_REPOSITORY == return_check && ignore_error)
  {
    return_check= REPOSITORY_DOES_NOT_EXIST;
    thd->clear_error();
  }
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
  ulong saved_mode;
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
    sql_print_warning("Info table is not ready to be used. Table "
                      "'%s.%s' cannot be scanned.", info->str_schema.str,
                      info->str_table.str);
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

bool Rpl_info_table::do_set_info(const int pos, const Dynamic_ids *value)
{
  if (const_cast<Dynamic_ids *>(value)->pack_dynamic_ids(&field_values->value[pos]))
    return TRUE;

  return FALSE;
}

bool Rpl_info_table::do_get_info(const int pos, char *value, const size_t size,
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
                                 const uchar *default_value __attribute__((unused)))
{
  if (field_values->value[pos].length() == size)
    return (!memcpy((char *) value, (char *)
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

bool Rpl_info_table::do_get_info(const int pos, Dynamic_ids *value,
                                 const Dynamic_ids *default_value __attribute__((unused)))
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
  ulong saved_mode;
  TABLE *table= NULL;
  Open_tables_backup backup;

  DBUG_ENTER("Rpl_info_table::do_update_is_transactional");
  DBUG_EXECUTE_IF("simulate_update_is_transactional_error",
                  {
                    DBUG_RETURN(TRUE);
                  });

  THD *thd= access->create_thd();
  saved_mode= thd->variables.sql_mode;
  tmp_disable_binlog(thd);

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
  reenable_binlog(thd);
  thd->variables.sql_mode= saved_mode;
  access->drop_thd(thd);
  DBUG_RETURN(error);
}
