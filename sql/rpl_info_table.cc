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
:Rpl_info_handler(nparam), field_idx(param_field_idx), use_default(FALSE)
{
  strmov(str_schema, param_schema);
  strmov(str_table, param_table);
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
  TABLE *table= NULL;
  ulong saved_mode;
  Open_tables_state backup;

  THD *thd= access->create_fake_thd();

  DBUG_ENTER("Rlp_info_table::do_init_info");

  saved_mode= thd->variables.sql_mode;
  thd->variables.sql_mode= 0;
  tmp_disable_binlog(thd);

  /*
    Opens and locks the rpl_info table before accessing it.
  */
  if (access->open_table(thd, str_schema, str_table, get_number_info(), TL_WRITE,
                          &table, &backup))
    goto end;

  /*
    Points the cursor at the row to be read where the master_id
    equals to the server_id. If the row is not found, the slave
    is being initialized and a new row is inserted.
  */
  longlong2str(server_id, field_values->field[field_idx].use.str, 10);
  field_values->field[field_idx].use.length= strlen(field_values->field[field_idx].use.str);
  if (!access->find_info_id(field_idx, field_values->field[field_idx].use, table))
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

  error= 0;

end:
  /*
    Unlocks and closes the rpl_info table.
  */
  access->close_table(thd, table, &backup);
  reenable_binlog(thd);
  thd->variables.sql_mode= saved_mode;
  access->drop_fake_thd(thd, error);
  DBUG_RETURN(test(error));
}

int Rpl_info_table::do_flush_info(const bool force)
{
  int error= 1;
  TABLE *table= NULL;
  ulong saved_mode;
  Open_tables_state backup;

  THD *thd= access->create_fake_thd();

  DBUG_ENTER("Rpl_info_table::do_flush_info");

  if (!(force || (sync_period &&
      ++(sync_counter) >= sync_period)))
  {
    access->drop_fake_thd(thd, 0);
    DBUG_RETURN(0);
  }

  sync_counter= 0;
  saved_mode= thd->variables.sql_mode;
  tmp_disable_binlog(thd);

  /*
    Opens and locks the rpl_info table before accessing it.
  */
  if (access->open_table(thd, str_schema, str_table, get_number_info(), TL_WRITE,
                         &table, &backup))
    goto end;

  /*
    Points the cursor at the row to be read where the master_id
    equals to the server_id. If the row is not found an error is
    reported.
  */
  longlong2str(server_id, field_values->field[field_idx].use.str, 10);
  field_values->field[field_idx].use.length= strlen(field_values->field[field_idx].use.str);
  if (access->find_info_id(field_idx, field_values->field[field_idx].use, table))
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
  }
  else
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
  }
  error= 0;

end:
  /*
    Unlocks and closes the rpl_info table.
  */
  access->close_table(thd, table, &backup);
  reenable_binlog(thd);
  thd->variables.sql_mode= saved_mode;
  access->drop_fake_thd(thd, error);
  DBUG_RETURN(test(error));
}

int Rpl_info_table::do_reset_info()
{
  int error= 1;
  TABLE *table= NULL;
  ulong saved_mode;
  Open_tables_state backup;

  THD *thd= access->create_fake_thd();

  DBUG_ENTER("Rpl_info_table::do_reset_info");

  saved_mode= thd->variables.sql_mode;
  tmp_disable_binlog(thd);

  /*
    Opens and locks the rpl_info table before accessing it.
  */
  if (access->open_table(thd, str_schema, str_table, get_number_info(), TL_WRITE,
                         &table, &backup))
    goto end;

  /*
    Points the cursor at the row to be deleted where the the master_id
    equals to the server_id. If the row is not found, the execution
    proceeds normally.
  */
  longlong2str(server_id, field_values->field[field_idx].use.str, 10);
  field_values->field[field_idx].use.length= strlen(field_values->field[field_idx].use.str);
  if (!access->find_info_id(field_idx, field_values->field[field_idx].use, table))
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
  error= 0;

end:
  /*
    Unlocks and closes the rpl_info table.
  */
  access->close_table(thd, table, &backup);
  reenable_binlog(thd);
  thd->variables.sql_mode= saved_mode;
  access->drop_fake_thd(thd, error);
  DBUG_RETURN(test(error));
}

int Rpl_info_table::do_check_info()
{
  int error= 1;
  TABLE *table= NULL;
  Open_tables_state backup;

  THD *thd= access->create_fake_thd();

  DBUG_ENTER("Rpl_info_table::do_check_info");

  /*
    Opens and locks the rpl_info table before accessing it.
  */
  if (access->open_table(thd, str_schema, str_table, get_number_info(), TL_READ,
                         &table, &backup))
    goto end;

  /*
    Points the cursor at the row to be deleted where the the master_id
    equals to the server_id. If the row is not found, an error is
    reported.
  */
  longlong2str(server_id, field_values->field[field_idx].use.str, 10);
  field_values->field[field_idx].use.length= strlen(field_values->field[field_idx].use.str);
  if (access->find_info_id(field_idx, field_values->field[field_idx].use, table))
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
  access->close_table(thd, table, &backup);
  access->drop_fake_thd(thd, error);
  DBUG_RETURN(test(error));
}

void Rpl_info_table::do_end_info()
{
}

int Rpl_info_table::do_prepare_info_for_read()
{
  if (!field_values)
    return TRUE;

  use_default= do_check_info();
  cursor= 1;

  return FALSE;
}

int Rpl_info_table::do_prepare_info_for_write()
{
  if (!field_values)
    return TRUE;

  cursor= 1;
  if (field_values)
    field_values->restore();

  return FALSE;
}

bool Rpl_info_table::do_set_info(const int pos, const char *value)
{
  if (pos >= ninfo || prv_error)
    return TRUE;

  /*
    Note that we did not need to copy any information at this point.
  */
  field_values->field[pos].use.str= const_cast<char *>(value);
  field_values->field[pos].use.length= strlen(field_values->field[pos].use.str);

  return FALSE;
}

bool Rpl_info_table::do_set_info(const int pos, const ulong value)
{
  if (pos >= ninfo || pos != cursor || prv_error)
    return TRUE;

  /*
    There is no need to check if the information fits in the reserved
    space:
    ULONG_MAX 	32 bit compiler   +4,294,967,295
                64 bit compiler   +18,446,744,073,709,551,615
  */
  if (sprintf(field_values->field[pos].use.str, "%li", value) < 0)
    return TRUE;
  field_values->field[pos].use.length= strlen(field_values->field[pos].use.str);

  return (FALSE);
}

bool Rpl_info_table::do_set_info(const int pos, const int value)
{
  if (pos >= ninfo || prv_error)
    return TRUE;

  /*
    There is no need to check if the information fits in the reserved
    space:
    INT_MIN    –2,147,483,648
    INT_MAX    +2,147,483,647
  */
  if ((sprintf(field_values->field[pos].use.str, "%d", value)) < 0)
    return TRUE;
  field_values->field[pos].use.length= strlen(field_values->field[pos].use.str);

  return FALSE;
}

bool Rpl_info_table::do_set_info(const int pos, const float value)
{
  if (pos >= ninfo || prv_error)
    return TRUE;

  /*
    There is no need to check if the information fits in the reserved
    space:
    FLT_MAX  The value of this macro is the maximum number representable
             in type float. It is supposed to be at least 1E+37.
    FLT_MIN  Similar to the FLT_MAX, we have 1E-37.
  */
  if (sprintf(field_values->field[pos].use.str, "%.3f", value) < 0)
    return TRUE;
  field_values->field[pos].use.length=
    strlen(field_values->field[pos].use.str);

  return FALSE;
}

bool Rpl_info_table::do_set_info(const int pos, const my_off_t value)
{
  if (pos >= ninfo || prv_error)
    return TRUE;

  /*
    There is no need to check if the information fits in the reserved
    space as it is equivalent to my_off_t.
  */
  if (sprintf(field_values->field[pos].use.str, "%lu", (ulong) value) < 0)
    return TRUE;
  field_values->field[pos].use.length= strlen(field_values->field[pos].use.str);

  return FALSE;
}

bool Rpl_info_table::do_set_info(const int pos, const Server_ids *value)
{
  if (pos >= ninfo || prv_error)
    return TRUE;

  int needed_size= (sizeof(::server_id) * 3 + 1) *
                    (1 + value->server_ids.elements);
  /*
    If the information does not fit in the field_values->field[pos].use.str,
    memory is reallocated and the size updated.
  */
  if (field_values->resize(needed_size, pos))
    return TRUE;

  /*
    This produces a line listing the total number and all the server_ids.
  */
  if (const_cast<Server_ids *>(value)->pack_server_ids(field_values->field[pos].use.str))
    return TRUE;
  field_values->field[pos].use.length= strlen(field_values->field[pos].use.str);
    
  return FALSE;
}

bool Rpl_info_table::do_get_info(const int pos, char *value, const size_t size,
                                 const char *default_value)
{
  if (pos >= ninfo || prv_error)
    return TRUE;

  if (use_default)
    strmov(value, default_value ? default_value : "");
  else
    strmov(value, field_values->field[pos].use.str ? 
           field_values->field[pos].use.str : "");
  
  return FALSE;
}

bool Rpl_info_table::do_get_info(const int pos, ulong *value,
                                 const ulong default_value)
{
  if (pos >= ninfo || prv_error)
    return TRUE;

  *value= (use_default ? default_value :
           (int) strtoul(field_values->field[pos].use.str, 0, 10));

  return FALSE;
}

bool Rpl_info_table::do_get_info(const int pos, int *value,
                                 const int default_value)
{
  if (pos >= ninfo || prv_error)
    return TRUE;

  *value= (use_default ? default_value :
           (int) strtol(field_values->field[pos].use.str, 0, 10));

  return FALSE;
}

bool Rpl_info_table::do_get_info(const int pos, float *value,
                                 const float default_value)
{
  if (pos >= ninfo || prv_error)
    return TRUE;

  *value= (use_default ? default_value :
           strtof(field_values->field[pos].use.str, 0));

  return FALSE;
}

bool Rpl_info_table::do_get_info(const int pos, my_off_t *value,
                                 const my_off_t default_value)
{
  if (pos >= ninfo || prv_error)
    return TRUE;

  *value= (use_default ? default_value :
           (my_off_t) ((int) strtol(field_values->field[pos].use.str, 0, 10)));

  return FALSE;
}

bool Rpl_info_table::do_get_info(const int pos, Server_ids *value,
                                 const Server_ids *default_value __attribute__((unused)))
{
  if (pos >= ninfo || prv_error)
    return TRUE;

  if (value->unpack_server_ids(field_values->field[pos].use.str))
    return TRUE;

  return FALSE;
}
