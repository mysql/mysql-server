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

#include "rpl_info_table.h"
#include "rpl_utility.h"
#include "sql_parse.h"

Rpl_info_table::Rpl_info_table(uint nparam, uint param_field_idx,
                               const char* param_schema,
                               const char *param_table)
:Rpl_info_handler(nparam), field_idx(param_field_idx)
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
  if (access)
    delete access;
  
  if (description)
    my_free(description);

  if (str_table.str)
    my_free(str_table.str);

  if (str_schema.str)
    my_free(str_schema.str);
}

int Rpl_info_table::do_init_info()
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
    Points the cursor at the row to be read where the master_id equals to
    the server_id.
  */
  if ((res= access->find_info_for_server_id(server_id, field_idx,
                                            field_values, table)) == FOUND_ID)
  {
    /*
      Reads the information stored in the rpl_info table into a
      set of variables. If there is a failure, an error is returned.
      Then executes some initialization routines.
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
    Points the cursor at the row to be read where the master_id
    equals to the server_id. If the row is not found an error is
    reported.
  */
  if ((res= access->find_info_for_server_id(server_id, field_idx,
                                            field_values, table)) == NOT_FOUND_ID)
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
    table->timestamp_field_type= TIMESTAMP_NO_AUTO_SET;
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
    Points the cursor at the row to be deleted where the the master_id
    equals to the server_id. If the row is not found, the execution
    proceeds normally.
  */
  if ((res= access->find_info_for_server_id(server_id, field_idx,
                                            field_values, table)) == FOUND_ID)
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

int Rpl_info_table::do_check_info()
{
  int error= 1;
  TABLE *table= NULL;
  ulong saved_mode;
  Open_tables_backup backup;

  DBUG_ENTER("Rpl_info_table::do_check_info");

  THD *thd= access->create_thd();

  saved_mode= thd->variables.sql_mode;

  /*
    Opens and locks the rpl_info table before accessing it.
  */
  if (access->open_table(thd, str_schema, str_table,
                         get_number_info(), TL_READ,
                         &table, &backup))
    goto end;

  /*
    Points the cursor at the row to be deleted where the the master_id
    equals to the server_id. If the row is not found, an error is
    reported.
  */
  if (access->find_info_for_server_id(server_id, field_idx,
                                      field_values, table) != FOUND_ID)
  {
    /* 
       We cannot simply call my_error here because it does not
       really means that there was a failure but only that the
       record was not found.
    */
    goto end;
  }
  error= 0;

end:
  /*
    Unlocks and closes the rpl_info table.
  */
  access->close_table(thd, table, &backup, error);
  thd->variables.sql_mode= saved_mode;
  access->drop_thd(thd);
  DBUG_RETURN(error);
}

void Rpl_info_table::do_end_info()
{
}

int Rpl_info_table::do_prepare_info_for_read()
{
  if (!field_values)
    return TRUE;

  cursor= 1;

  return FALSE;
}

int Rpl_info_table::do_prepare_info_for_write()
{
  return(do_prepare_info_for_read());
}

bool Rpl_info_table::do_set_info(const int pos, const char *value)
{
  return (field_values->value[pos].copy(value, strlen(value),
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
  if (const_cast<Server_ids *>(value)->pack_server_ids(&field_values->value[pos]))
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
                                 const Server_ids *default_value __attribute__((unused)))
{
  if (value->unpack_server_ids(field_values->value[pos].c_ptr_safe()))
    return TRUE;

  return FALSE;
}

char* Rpl_info_table::do_get_description_info()
{
  return description;
}

bool Rpl_info_table::do_is_transactional()
{
  ulong saved_mode;
  TABLE *table= NULL;
  Open_tables_backup backup;
  bool is_trans= FALSE;

  DBUG_ENTER("Rpl_info_table::do_is_transactional");

  THD *thd= access->create_thd();

  saved_mode= thd->variables.sql_mode;

  /*
    Opens and locks the rpl_info table before accessing it.
  */
  if (!access->open_table(thd, str_schema, str_table,
                          get_number_info(), TL_READ,
                          &table, &backup))
    is_trans= table->file->has_transactions();

  access->close_table(thd, table, &backup, 0);
  thd->variables.sql_mode= saved_mode;
  access->drop_thd(thd);
  DBUG_RETURN(is_trans);
}

bool Rpl_info_table::change_engine(const char *engine)
{
  bool error= TRUE;
  ulong saved_mode;

  DBUG_ENTER("Rpl_info_table::do_check_info");

  THD *thd= access->create_thd();

  saved_mode= thd->variables.sql_mode;
  tmp_disable_binlog(thd);

  /* TODO: Change the engine using internal functions */

  error= FALSE;

  reenable_binlog(thd);
  thd->variables.sql_mode= saved_mode;
  access->drop_thd(thd);
  DBUG_RETURN(error);
}
