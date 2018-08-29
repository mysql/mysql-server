/* Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "plugin/pfs_table_plugin/pfs_example_machine.h"

#include "mysql/psi/mysql_mutex.h"

PFS_engine_table_share_proxy machine_st_share;
mysql_mutex_t LOCK_machine_records_array;

/* Total number of rows in table. */
unsigned int machine_rows_in_table = 0;

std::vector<Machine_Record> machine_records_vector;

/**
 * Instantiate Machine_Table_Handle at plugin code when corresponding table
 * in performance schema is opened.
 */
PSI_table_handle *machine_open_table(PSI_pos **pos) {
  Machine_Table_Handle *temp = new Machine_Table_Handle();
  temp->current_row.machine_number.is_null = true;
  temp->current_row.machine_type.is_null = true;
  temp->current_row.employee_number.is_null = true;
  temp->current_row.machine_made_length = 0;

  *pos = (PSI_pos *)(&temp->m_pos);
  return (PSI_table_handle *)temp;
}

/**
 * Destroy the Machine_Table_Handle at plugin code when corresponding table
 * in performance schema is closed.
 */
void machine_close_table(PSI_table_handle *handle) {
  Machine_Table_Handle *temp = (Machine_Table_Handle *)handle;
  delete temp;
}

static void copy_record(Machine_Record *dest, Machine_Record *source) {
  dest->machine_number = source->machine_number;
  dest->machine_type = source->machine_type;
  dest->machine_made_length = source->machine_made_length;
  strncpy(dest->machine_made, source->machine_made, dest->machine_made_length);
  dest->employee_number = source->employee_number;

  dest->m_exist = source->m_exist;
}

/* Define implementation of PFS_engine_table_proxy. */
int machine_rnd_next(PSI_table_handle *handle) {
  Machine_Table_Handle *h = (Machine_Table_Handle *)handle;

  for (h->m_pos.set_at(&h->m_next_pos); h->m_pos.has_more(); h->m_pos.next()) {
    Machine_Record *record = &machine_records_vector.at(h->m_pos.get_index());
    if (record->m_exist) {
      /* Make the current row from records_array buffer */
      copy_record(&h->current_row, record);
      h->m_next_pos.set_after(&h->m_pos);
      return 0;
    }
  }

  return PFS_HA_ERR_END_OF_FILE;
}

int machine_rnd_init(PSI_table_handle *h MY_ATTRIBUTE((unused)),
                     bool scan MY_ATTRIBUTE((unused))) {
  return 0;
}

int machine_rnd_pos(PSI_table_handle *handle) {
  Machine_Table_Handle *h = (Machine_Table_Handle *)handle;
  Machine_Record *record = &machine_records_vector[h->m_pos.get_index()];

  if (record->m_exist) {
    /* Make the current row from records_array buffer */
    copy_record(&h->current_row, record);
  }

  return 0;
}

/* Initialize the table index */
int machine_index_init(PSI_table_handle *handle MY_ATTRIBUTE((unused)),
                       uint idx MY_ATTRIBUTE((unused)),
                       bool sorted MY_ATTRIBUTE((unused)),
                       PSI_index_handle **index MY_ATTRIBUTE((unused))) {
  /* Do nothing as there are no index */
  return 0;
}

/* For each key in index, read value specified in query */
int machine_index_read(PSI_index_handle *index MY_ATTRIBUTE((unused)),
                       PSI_key_reader *reader MY_ATTRIBUTE((unused)),
                       unsigned int idx MY_ATTRIBUTE((unused)),
                       int find_flag MY_ATTRIBUTE((unused))) {
  /* Do nothing as there are no index */
  return 0;
}

/* Read the next indexed value */
int machine_index_next(PSI_table_handle *handle MY_ATTRIBUTE((unused))) {
  /* Do nothing as there are no index */
  return PFS_HA_ERR_END_OF_FILE;
}

/* Reset cursor position */
void machine_reset_position(PSI_table_handle *handle) {
  Machine_Table_Handle *h = (Machine_Table_Handle *)handle;
  h->m_pos.reset();
  h->m_next_pos.reset();
  return;
}

/* Read current row from the current_row and display them in the table */
int machine_read_column_value(PSI_table_handle *handle, PSI_field *field,
                              uint index) {
  Machine_Table_Handle *h = (Machine_Table_Handle *)handle;

  switch (index) {
    case 0: /* MACHINE_SL_NUMBER */
      table_svc->set_field_integer(field, h->current_row.machine_number);
      break;
    case 1: /* MACHINE_TYPE */
      table_svc->set_field_enum(field, h->current_row.machine_type);
      break;
    case 2: /* MACHINE_MADE */
      table_svc->set_field_char_utf8(field, h->current_row.machine_made,
                                     h->current_row.machine_made_length);
      break;
    case 3: /* EMPLOYEE_NUMBER */
      table_svc->set_field_integer(field, h->current_row.employee_number);
      break;
    default: /* We should never reach here */
      DBUG_ASSERT(0);
      break;
  }

  return 0;
}

/* Store row data into records array */
int machine_write_row_values(PSI_table_handle *handle) {
  Machine_Table_Handle *h = (Machine_Table_Handle *)handle;
  bool found = false;

  mysql_mutex_lock(&LOCK_machine_records_array);

  h->current_row.m_exist = true;
  int size = machine_records_vector.size();
  for (int i = 0; i < size; i++) {
    Machine_Record *record = &machine_records_vector.at(i);
    if (record->m_exist == false) {
      copy_record(record, &h->current_row);
      found = true;
      break;
    }
  }

  if (!found) machine_records_vector.push_back(h->current_row);

  machine_rows_in_table++;

  mysql_mutex_unlock(&LOCK_machine_records_array);

  return 0;
}

/* Read field data from Field and store that into buffer */
int machine_write_column_value(PSI_table_handle *handle, PSI_field *field,
                               unsigned int index) {
  Machine_Table_Handle *h = (Machine_Table_Handle *)handle;

  char *machine_made = (char *)h->current_row.machine_made;
  unsigned int *machine_made_length = &h->current_row.machine_made_length;

  switch (index) {
    case 0: /* MACHINE_SL_NUMBER */
      table_svc->get_field_integer(field, &h->current_row.machine_number);
      break;
    case 1: /* MACHINE_TYPE */
      table_svc->get_field_enum(field, &h->current_row.machine_type);
      break;
    case 2: /* MACHINE_MADE */
      table_svc->get_field_char_utf8(field, machine_made, machine_made_length);
      break;
    case 3: /* EMPLOYEE_NUMBER */
      table_svc->get_field_integer(field, &h->current_row.employee_number);
      break;
    default: /* We should never reach here */
      DBUG_ASSERT(0);
      break;
  }

  return 0;
}

/* Update row data in records array */
int machine_update_row_values(PSI_table_handle *handle) {
  Machine_Table_Handle *h = (Machine_Table_Handle *)handle;

  Machine_Record *cur = &machine_records_vector[h->m_pos.get_index()];

  DBUG_ASSERT(cur->m_exist == true);

  mysql_mutex_lock(&LOCK_machine_records_array);
  copy_record(cur, &h->current_row);
  mysql_mutex_unlock(&LOCK_machine_records_array);

  return 0;
}

int machine_update_column_value(PSI_table_handle *handle, PSI_field *field,
                                unsigned int index) {
  Machine_Table_Handle *h = (Machine_Table_Handle *)handle;

  char *machine_made = (char *)h->current_row.machine_made;
  unsigned int *machine_made_length = &h->current_row.machine_made_length;

  switch (index) {
    case 0: /* MACHINE_SL_NUMBER */
      table_svc->get_field_integer(field, &h->current_row.machine_number);
      break;
    case 1: /* MACHINE_TYPE */
      table_svc->get_field_enum(field, &h->current_row.machine_type);
      break;
    case 2: /* MACHINE_MADE */
      table_svc->get_field_char_utf8(field, machine_made, machine_made_length);
      break;
    case 3: /* EMPLOYEE_NUMBER */
      table_svc->get_field_integer(field, &h->current_row.employee_number);
      break;
    default: /* We should never reach here */
      DBUG_ASSERT(0);
      break;
  }

  return 0;
}

/* Delete row data from records array */
int machine_delete_row_values(PSI_table_handle *handle) {
  Machine_Table_Handle *h = (Machine_Table_Handle *)handle;

  Machine_Record *cur = &machine_records_vector.at(h->m_pos.get_index());

  DBUG_ASSERT(cur->m_exist == true);

  mysql_mutex_lock(&LOCK_machine_records_array);
  cur->m_exist = false;
  machine_rows_in_table--;
  mysql_mutex_unlock(&LOCK_machine_records_array);

  return 0;
}

int machine_delete_all_rows(void) {
  mysql_mutex_lock(&LOCK_machine_records_array);
  machine_records_vector.clear();
  machine_rows_in_table = 0;
  mysql_mutex_unlock(&LOCK_machine_records_array);
  return 0;
}

unsigned long long machine_get_row_count(void) { return machine_rows_in_table; }

void init_machine_share(PFS_engine_table_share_proxy *share) {
  share->m_table_name = "pfs_example_machine";
  share->m_table_name_length = 20;
  share->m_table_definition =
      "MACHINE_SL_NUMBER INTEGER, MACHINE_TYPE "
      "enum('LAPTOP','DESKTOP','MOBILE'), MACHINE_MADE char(20), "
      "EMPLOYEE_NUMBER "
      "INTEGER";
  share->m_ref_length = sizeof(Machine_POS);
  share->m_acl = EDITABLE;
  share->get_row_count = machine_get_row_count;
  share->delete_all_rows = machine_delete_all_rows;

  /* Initialize PFS_engine_table_proxy */
  share->m_proxy_engine_table = {machine_rnd_next,
                                 machine_rnd_init,
                                 machine_rnd_pos,
                                 machine_index_init,
                                 machine_index_read,
                                 machine_index_next,
                                 machine_read_column_value,
                                 machine_reset_position,
                                 machine_write_column_value,
                                 machine_write_row_values,
                                 machine_update_column_value,
                                 machine_update_row_values,
                                 machine_delete_row_values,
                                 machine_open_table,
                                 machine_close_table};
}
