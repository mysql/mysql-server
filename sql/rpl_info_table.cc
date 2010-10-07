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

Rpl_info_table::Rpl_info_table(uint nparam, uint param_field_idx,
                               const char* param_schema,
                               const char *param_table)
:Rpl_info_handler(nparam), field_idx(param_field_idx)
{

  strmov(str_schema, param_schema);
  strmov(str_table, param_table);
  str_schema_size= strlen(str_schema);
  str_table_size= strlen(str_table);
  char *pos= strmov(description, param_schema);
  pos= strmov(pos, ".");
  pos= strmov(pos, param_table);
  access= new Rpl_info_table_access();
}

Rpl_info_table::~Rpl_info_table()
{ 
  if (access)
    delete access;
}

int Rpl_info_table::do_init_info()
{
  int error= 1;
  enum enum_return_id res= FOUND_ID;
  TABLE *table= NULL;
  ulong saved_mode;
  Open_tables_backup backup;

  THD *thd= access->create_bootstrap_thd();

  DBUG_ENTER("Rlp_info_table::do_init_info");

  saved_mode= thd->variables.sql_mode;
  tmp_disable_binlog(thd);

  /*
    Opens and locks the rpl_info table before accessing it.
  */
  if (access->open_table(thd, str_schema, str_schema_size, str_table,
                         str_table_size, get_number_info(), TL_WRITE,
                         &table, &backup))
    goto end;

  /*
    Points the cursor at the row to be read where the master_id equals to
    the server_id.
  */
  if ((res= access->find_info_id(server_id, field_idx,
                                 field_values, table)) == FOUND_ID)
  {
    /*
      Reads the information stored in the rpl_info table into a
      set of variables. If there is a failure, an error is returned.
      Then executes some initialization routines.
    */
    if (access->load_info_fields(get_number_info(), table->field,
                                 field_values))
      goto end;
  }
  error= ((res == FOUND_ID || res == NOT_FOUND_ID) ? 0 : 1);
end:
  /*
    Unlocks and closes the rpl_info table.
  */
  access->close_table(thd, table, &backup, error);
  reenable_binlog(thd);
  thd->variables.sql_mode= saved_mode;
  access->drop_bootstrap_thd(thd);
  DBUG_RETURN(test(error));
}

int Rpl_info_table::do_flush_info(const bool force)
{
  int error= 1;
  enum enum_return_id res= FOUND_ID;
  TABLE *table= NULL;
  ulong saved_mode;
  Open_tables_backup backup;

  THD *thd= access->create_bootstrap_thd();

  DBUG_ENTER("Rpl_info_table::do_flush_info");

  if (!(force || (sync_period &&
      ++(sync_counter) >= sync_period)))
  {
    access->drop_bootstrap_thd(thd);
    DBUG_RETURN(0);
  }

  sync_counter= 0;
  saved_mode= thd->variables.sql_mode;
  tmp_disable_binlog(thd);

  /*
    Opens and locks the rpl_info table before accessing it.
  */
  if (access->open_table(thd, str_schema, str_schema_size, str_table,
                         str_table_size, get_number_info(), TL_WRITE,
                         &table, &backup))
    goto end;

  /*
    Points the cursor at the row to be read where the master_id
    equals to the server_id. If the row is not found an error is
    reported.
  */
  if ((res= access->find_info_id(server_id, field_idx,
                                 field_values, table)) == NOT_FOUND_ID)
  {
    /*
      Prepares the information to be stored before calling ha_write_row.
    */
    empty_record(table);
    if (access->store_info_fields(get_number_info(), table->field,
                                  field_values))
      goto end;

    /*
      Inserts a new row into rpl_info table.
    */
    if (table->file->ha_write_row(table->record[0]))
    {
      table->file->print_error(error, MYF(0));
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
    if (access->store_info_fields(get_number_info(), table->field,
                                  field_values))
      goto end;
 
    /*
      Updates a row in the rpl_info table.
    */
    if ((error= table->file->ha_update_row(table->record[1], table->record[0])) &&
        error != HA_ERR_RECORD_IS_THE_SAME)
    {
      table->file->print_error(error, MYF(0));
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
  access->drop_bootstrap_thd(thd);
  DBUG_RETURN(test(error));
}

int Rpl_info_table::do_reset_info()
{
  int error= 1;
  enum enum_return_id res= FOUND_ID;
  TABLE *table= NULL;
  ulong saved_mode;
  Open_tables_backup backup;

  THD *thd= access->create_bootstrap_thd();

  DBUG_ENTER("Rpl_info_table::do_reset_info");

  saved_mode= thd->variables.sql_mode;
  tmp_disable_binlog(thd);

  /*
    Opens and locks the rpl_info table before accessing it.
  */
  if (access->open_table(thd, str_schema, str_schema_size, str_table,
                         str_table_size, get_number_info(), TL_WRITE,
                         &table, &backup))
    goto end;

  /*
    Points the cursor at the row to be deleted where the the master_id
    equals to the server_id. If the row is not found, the execution
    proceeds normally.
  */
  if ((res= access->find_info_id(server_id, field_idx,
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
  error= ((res == FOUND_ID || res == NOT_FOUND_ID) ? 0 : 1);
end:
  /*
    Unlocks and closes the rpl_info table.
  */
  access->close_table(thd, table, &backup, error);
  reenable_binlog(thd);
  thd->variables.sql_mode= saved_mode;
  access->drop_bootstrap_thd(thd);
  DBUG_RETURN(test(error));
}

int Rpl_info_table::do_check_info()
{
  int error= 1;
  TABLE *table= NULL;
  ulong saved_mode;
  Open_tables_backup backup;

  THD *thd= access->create_bootstrap_thd();

  DBUG_ENTER("Rpl_info_table::do_check_info");

  saved_mode= thd->variables.sql_mode;

  /*
    Opens and locks the rpl_info table before accessing it.
  */
  if (access->open_table(thd, str_schema, str_schema_size, str_table,
                         str_table_size, get_number_info(), TL_READ,
                         &table, &backup))
    goto end;

  /*
    Points the cursor at the row to be deleted where the the master_id
    equals to the server_id. If the row is not found, an error is
    reported.
  */
  if (access->find_info_id(server_id, field_idx,
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
  access->drop_bootstrap_thd(thd);
  DBUG_RETURN(test(error));
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
  if (!field_values)
    return TRUE;

  cursor= 1;

  return FALSE;
}

bool Rpl_info_table::do_set_info(const int pos, const char *value)
{
  strmov(field_values->field[pos].value.str, const_cast<char *>(value));
  field_values->field[pos].value.length= strlen(field_values->field[pos].value.str);

  return FALSE;
}

bool Rpl_info_table::do_set_info(const int pos, const ulong value)
{
  /*
    There is no need to check if the information fits in the reserved
    space:
    ULONG_MAX 	32 bit compiler   +4,294,967,295
                64 bit compiler   +18,446,744,073,709,551,615
  */
  if (sprintf(field_values->field[pos].value.str, "%lu", value) < 0)
    return TRUE;
  field_values->field[pos].value.length= strlen(field_values->field[pos].value.str);

  return (FALSE);
}

bool Rpl_info_table::do_set_info(const int pos, const int value)
{
  /*
    There is no need to check if the information fits in the reserved
    space:
    INT_MIN    –2,147,483,648
    INT_MAX    +2,147,483,647
  */
  if ((sprintf(field_values->field[pos].value.str, "%d", value)) < 0)
    return TRUE;
  field_values->field[pos].value.length= strlen(field_values->field[pos].value.str);

  return FALSE;
}

bool Rpl_info_table::do_set_info(const int pos, const float value)
{
  /*
    There is no need to check if the information fits in the reserved
    space. We are assuming that the precision is 3 bytes (See the
    appropriate set function):

    FLT_MAX  The value of this macro is the maximum number representable
             in type float. It is supposed to be at least 1E+37.
    FLT_MIN  Similar to the FLT_MAX, we have 1E-37.

    If a file is manually and not properly changed, this function may
    crash the server.
  */
  if (sprintf(field_values->field[pos].value.str, "%.3f", value) < 0)
    return TRUE;
  field_values->field[pos].value.length=
    strlen(field_values->field[pos].value.str);

  return FALSE;
}

bool Rpl_info_table::do_set_info(const int pos, const Server_ids *value)
{
  int needed_size= (sizeof(::server_id) * 3 + 1) *
                    (1 + value->server_ids.elements);
  /*
    If the information does not fit in the field_values->field[pos].value.str,
    memory is reallocated and the size updated.
  */
  if (field_values->resize(needed_size, pos))
    return TRUE;

  /*
    This produces a line listing the total number and all the server_ids.
  */
  if (const_cast<Server_ids *>(value)->pack_server_ids(field_values->field[pos].value.str))
    return TRUE;
  field_values->field[pos].value.length= strlen(field_values->field[pos].value.str);
    
  return FALSE;
}

bool Rpl_info_table::do_get_info(const int pos, char *value, const size_t size,
                                 const char *default_value)
{
  strmov(value, field_values->field[pos].value.str ?
         field_values->field[pos].value.str : "");
  
  return FALSE;
}

bool Rpl_info_table::do_get_info(const int pos, ulong *value,
                                 const ulong default_value)
{
  *value= strtoul(field_values->field[pos].value.str, 0, 10);

  return FALSE;
}

bool Rpl_info_table::do_get_info(const int pos, int *value,
                                 const int default_value)
{
  *value=  atoi(field_values->field[pos].value.str);

  return FALSE;
}

bool Rpl_info_table::do_get_info(const int pos, float *value,
                                 const float default_value)
{
  if (sscanf(field_values->field[pos].value.str, "%f", value) != 1)
    return TRUE;

  return FALSE;
}

bool Rpl_info_table::do_get_info(const int pos, Server_ids *value,
                                 const Server_ids *default_value __attribute__((unused)))
{
  if (value->unpack_server_ids(field_values->field[pos].value.str))
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

  THD *thd= access->create_bootstrap_thd();

  DBUG_ENTER("Rpl_info_table::do_is_transactional");

  saved_mode= thd->variables.sql_mode;

  /*
    Opens and locks the rpl_info table before accessing it.
  */
  if (!access->open_table(thd, str_schema, str_schema_size, str_table,
                          str_table_size, get_number_info(), TL_READ,
                          &table, &backup))
    is_trans= table->file->has_transactions();

  access->close_table(thd, table, &backup, 0);
  thd->variables.sql_mode= saved_mode;
  access->drop_bootstrap_thd(thd);

  DBUG_RETURN(is_trans);
}
