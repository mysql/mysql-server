/* Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "plugin/pfs_table_plugin/pfs_example_employee_salary.h"

PFS_engine_table_share_proxy esalary_st_share;
mysql_mutex_t LOCK_esalary_records_array;

/* Total number of rows in table. */
unsigned int esalary_rows_in_table = 0;

std::vector<Esalary_Record> esalary_records_vector;

/**
 * Instantiate Esalary_Table_Handle at plugin code when corresponding table
 * in performance schema is opened.
 */
PSI_table_handle*
esalary_open_table(PSI_pos** pos)
{
  Esalary_Table_Handle* temp = new Esalary_Table_Handle();
  temp->current_row.e_number.is_null = true;
  temp->current_row.e_salary.is_null = true;
  temp->current_row.e_dob_length = 0;
  temp->current_row.e_tob_length = 0;

  *pos = (PSI_pos*)(&temp->m_pos);
  return (PSI_table_handle*)temp;
}

/**
 * Destroy the Esalary_Table_Handle at plugin code when corresponding table
 * in performance schema is closed.
 */
void
esalary_close_table(PSI_table_handle* handle)
{
  Esalary_Table_Handle* temp = (Esalary_Table_Handle*)handle;
  delete temp;
}

static void
copy_record(Esalary_Record* dest, Esalary_Record* source)
{
  dest->e_number = source->e_number;
  dest->e_salary = source->e_salary;
  dest->e_dob_length = source->e_dob_length;
  strncpy(dest->e_dob, source->e_dob, dest->e_dob_length);
  dest->e_tob_length = source->e_tob_length;
  strncpy(dest->e_tob, source->e_tob, dest->e_tob_length);
  dest->m_exist = source->m_exist;
}

/* Define implementation of PFS_engine_table_proxy. */
int
esalary_rnd_next(PSI_table_handle* handle)
{
  Esalary_Table_Handle* h = (Esalary_Table_Handle*)handle;

  for (h->m_pos.set_at(&h->m_next_pos) ; h->m_pos.has_more() ; h->m_pos.next())
  {
    Esalary_Record *record = &esalary_records_vector.at(h->m_pos.get_index());
    if (record->m_exist)
    {
      /* Make the current row from records_array buffer */
      copy_record(&h->current_row, record);
      h->m_next_pos.set_after(&h->m_pos);
      return 0;
    }
  }

  return PFS_HA_ERR_END_OF_FILE;
}

int
esalary_rnd_init(PSI_table_handle* h MY_ATTRIBUTE((unused)),
                 bool scan MY_ATTRIBUTE((unused)))
{
  return 0;
}

int
esalary_rnd_pos(PSI_table_handle* handle)
{
  Esalary_Table_Handle* h = (Esalary_Table_Handle*)handle;
  Esalary_Record* record = &esalary_records_vector[h->m_pos.get_index()];

  if (record->m_exist)
  {
    /* Make the current row from records_array buffer */
    copy_record(&h->current_row, record);
  }

  return 0;
}

/* Initialize the table index */
int
esalary_index_init(PSI_table_handle* handle MY_ATTRIBUTE((unused)),
                   uint idx MY_ATTRIBUTE((unused)),
                   bool sorted MY_ATTRIBUTE((unused)),
                   PSI_index_handle** index MY_ATTRIBUTE((unused)))
{
  /* Do nothing as there are no index */
  return 0;
}

/* For each key in index, read value specified in query */
int
esalary_index_read(PSI_index_handle* index MY_ATTRIBUTE((unused)),
                   PSI_key_reader* reader MY_ATTRIBUTE((unused)),
                   unsigned int idx MY_ATTRIBUTE((unused)),
                   int find_flag MY_ATTRIBUTE((unused)))
{
  /* Do nothing as there are no index */
  return 0;
}

/* Read the next indexed value */
int
esalary_index_next(PSI_table_handle* handle MY_ATTRIBUTE((unused)))
{
  /* Do nothing as there are no index */
  return PFS_HA_ERR_END_OF_FILE;
}

/* Reset cursor position */
void
esalary_reset_position(PSI_table_handle* handle)
{
  Esalary_Table_Handle* h = (Esalary_Table_Handle*)handle;
  h->m_pos.reset();
  h->m_next_pos.reset();
  return;
}

/* Read current row from the current_row and display them in the table */
int
esalary_read_column_value(PSI_table_handle* handle,
                          PSI_field* field,
                          uint index)
{
  Esalary_Table_Handle* h = (Esalary_Table_Handle*)handle;

  switch (index)
  {
  case 0: /* EMPLOYEE_NUMBER */
    table_svc->set_field_integer(field, h->current_row.e_number);
    break;
  case 1: /* EMPLOYEE_SALARY */
    table_svc->set_field_bigint(field, h->current_row.e_salary);
    break;
  case 2: /* DATE_OF_BIRTH */
    table_svc->set_field_date(
      field, h->current_row.e_dob, h->current_row.e_dob_length);
    break;
  case 3: /* TIME_OF_BIRTH */
    table_svc->set_field_time(
      field, h->current_row.e_tob, h->current_row.e_tob_length);
    break;
  default: /* We should never reach here */
    DBUG_ASSERT(0);
    break;
  }

  return 0;
}

/* Store row data into records array */
int
esalary_write_row_values(PSI_table_handle* handle)
{
  Esalary_Table_Handle* h = (Esalary_Table_Handle*)handle;
  bool found = false;

  mysql_mutex_lock(&LOCK_esalary_records_array);

  h->current_row.m_exist = true;
  int size = esalary_records_vector.size();
  for (int i=0 ; i < size ; i++)
  {
    Esalary_Record *record = &esalary_records_vector.at(i);
    if (record->m_exist == false)
    {
      copy_record(record, &h->current_row);
      found = true;
      break;
    }
  }

  if (!found)
    esalary_records_vector.push_back(h->current_row);

  esalary_rows_in_table++;

  mysql_mutex_unlock(&LOCK_esalary_records_array);

  return 0;
}

/* Read field data from Field and store that into buffer */
int
esalary_write_column_value(PSI_table_handle* handle,
                           PSI_field* field,
                           unsigned int index)
{
  Esalary_Table_Handle* h = (Esalary_Table_Handle*)handle;
  char* dob_val = &h->current_row.e_dob[0];
  unsigned int* dob_len = &h->current_row.e_dob_length;
  char* tob_val = &h->current_row.e_tob[0];
  unsigned int* tob_len = &h->current_row.e_tob_length;

  switch (index)
  {
  case 0: /* EMPLOYEE_NUMBER */
    table_svc->get_field_integer(field, &h->current_row.e_number);
    break;
  case 1: /* EMPLOYEE_SALARY */
    table_svc->get_field_bigint(field, &h->current_row.e_salary);
    break;
  case 2: /* DATE_OF_BIRTH */
    table_svc->get_field_date(field, dob_val, dob_len);
    break;
  case 3: /* TIME_OF_BIRTH */
    table_svc->get_field_time(field, tob_val, tob_len);
    break;
  default: /* We should never reach here */
    DBUG_ASSERT(0);
    break;
  }

  return 0;
}

/* Update row data in records array */
int
esalary_update_row_values(PSI_table_handle* handle)
{
  Esalary_Table_Handle* h = (Esalary_Table_Handle*)handle;

  Esalary_Record* cur = &esalary_records_vector[h->m_pos.get_index()];

  DBUG_ASSERT(cur->m_exist == true);

  mysql_mutex_lock(&LOCK_esalary_records_array);
  copy_record(cur, &h->current_row);
  mysql_mutex_unlock(&LOCK_esalary_records_array);

  return 0;
}

int
esalary_update_column_value(PSI_table_handle* handle,
                            PSI_field* field,
                            unsigned int index)
{
  Esalary_Table_Handle* h = (Esalary_Table_Handle*)handle;
  char* dob_val = &h->current_row.e_dob[0];
  unsigned int* dob_len = &h->current_row.e_dob_length;
  char* tob_val = &h->current_row.e_tob[0];
  unsigned int* tob_len = &h->current_row.e_tob_length;

  switch (index)
  {
  case 0: /* EMPLOYEE_NUMBER */
    table_svc->get_field_integer(field, &h->current_row.e_number);
    break;
  case 1: /* EMPLOYEE_SALARY */
    table_svc->get_field_bigint(field, &h->current_row.e_salary);
    break;
  case 2: /* DATE_OF_BIRTH */
    table_svc->get_field_date(field, dob_val, dob_len);
    break;
  case 3: /* TIME_OF_BIRTH */
    table_svc->get_field_time(field, tob_val, tob_len);
    break;
  default: /* We should never reach here */
    DBUG_ASSERT(0);
    break;
  }

  return 0;
}

/* Delete row data from records array */
int
esalary_delete_row_values(PSI_table_handle* handle)
{
  Esalary_Table_Handle* h = (Esalary_Table_Handle*)handle;

  Esalary_Record* cur = &esalary_records_vector.at(h->m_pos.get_index());

  DBUG_ASSERT(cur->m_exist == true);

  mysql_mutex_lock(&LOCK_esalary_records_array);
  cur->m_exist = false;
  esalary_rows_in_table--;
  mysql_mutex_unlock(&LOCK_esalary_records_array);

  return 0;
}

int
esalary_delete_all_rows(void)
{
  mysql_mutex_lock(&LOCK_esalary_records_array);
  esalary_records_vector.clear();
  esalary_rows_in_table = 0;
  mysql_mutex_unlock(&LOCK_esalary_records_array);
  return 0;
}

unsigned long long
esalary_get_row_count(void)
{
  return esalary_rows_in_table;
}

void
init_esalary_share(PFS_engine_table_share_proxy* share)
{
  share->m_table_name = "pfs_example_employee_salary";
  share->m_table_name_length = 27;
  share->m_table_definition =
    "EMPLOYEE_NUMBER INTEGER, EMPLOYEE_SALARY BIGINT, DATE_OF_BIRTH DATE, "
    "TIME_OF_BIRTH TIME";
  share->m_ref_length = sizeof(Esalary_POS);
  share->m_acl = EDITABLE;
  share->get_row_count = esalary_get_row_count;
  share->delete_all_rows = esalary_delete_all_rows;

  /* Initialize PFS_engine_table_proxy */
  share->m_proxy_engine_table = {esalary_rnd_next,
                                 esalary_rnd_init,
                                 esalary_rnd_pos,
                                 esalary_index_init,
                                 esalary_index_read,
                                 esalary_index_next,
                                 esalary_read_column_value,
                                 esalary_reset_position,
                                 esalary_write_column_value,
                                 esalary_write_row_values,
                                 esalary_update_column_value,
                                 esalary_update_row_values,
                                 esalary_delete_row_values,
                                 esalary_open_table,
                                 esalary_close_table};
}
