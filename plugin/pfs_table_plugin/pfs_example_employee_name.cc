/* Copyright (c) 2017, 2023, Oracle and/or its affiliates.

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

#include "plugin/pfs_table_plugin/pfs_example_employee_name.h"

#include "mysql/psi/mysql_mutex.h"

PFS_engine_table_share_proxy ename_st_share;
mysql_mutex_t LOCK_ename_records_array;

/* Total number of rows in table. */
unsigned int ename_rows_in_table = 0;

/* Next available index for new record to be stored in global record array. */
unsigned int ename_next_available_index = 0;

Ename_Record ename_records_array[EMPLOYEEE_NAME_MAX_ROWS] = {
    {{0, true}, "", 0, "", 0, false}};

/**
  Check for duplicate value of Primary/Unique Key column(s).
  A sequential search is being used here, but it is up to the plugin writer to
  implement a search of their own to make sure duplicate values are not inserted
  for Primary/Unique Key Column(s).

  @param record record to be checked for duplicate
  @param skip_index element at this index not to be considered for comparison
  @return true if duplicate found, false otherwise
*/
static bool is_duplicate(Ename_Record *record, int skip_index) {
  for (int i = 0; i < EMPLOYEEE_NAME_MAX_ROWS; i++) {
    Ename_Record *temp = &ename_records_array[i];
    if (!temp->m_exist || i == skip_index) continue;
    if (temp->e_number.val == record->e_number.val) return true;
  }

  return false;
}

/**
 * Instantiate Ename_Table_Handle at plugin code when corresponding table
 * in performance schema is opened.
 */
PSI_table_handle *ename_open_table(PSI_pos **pos) {
  Ename_Table_Handle *temp = new Ename_Table_Handle();
  temp->current_row.e_number.is_null = true;
  temp->current_row.f_name_length = 0;
  temp->current_row.l_name_length = 0;

  *pos = (PSI_pos *)(&temp->m_pos);
  return (PSI_table_handle *)temp;
}

/**
 * Destroy the Ename_Table_Handle at plugin code when corresponding table
 * in performance schema is closed.
 */
void ename_close_table(PSI_table_handle *handle) {
  Ename_Table_Handle *temp = (Ename_Table_Handle *)handle;
  delete temp;
}

/* Copy record from source to destination */
static void copy_record(Ename_Record *dest, Ename_Record *source) {
  dest->e_number = source->e_number;
  dest->f_name_length = source->f_name_length;
  strncpy(dest->f_name, source->f_name, dest->f_name_length);
  dest->l_name_length = source->l_name_length;
  strncpy(dest->l_name, source->l_name, dest->l_name_length);
  dest->m_exist = source->m_exist;
}

/* Define implementation of PFS_engine_table_proxy. */
int ename_rnd_next(PSI_table_handle *handle) {
  Ename_Table_Handle *h = (Ename_Table_Handle *)handle;

  for (h->m_pos.set_at(&h->m_next_pos); h->m_pos.has_more(); h->m_pos.next()) {
    Ename_Record *record = &ename_records_array[h->m_pos.get_index()];

    if (record->m_exist) {
      /* Make the current row from records_array buffer */
      copy_record(&h->current_row, record);
      h->m_next_pos.set_after(&h->m_pos);
      return 0;
    }
  }

  return PFS_HA_ERR_END_OF_FILE;
}

int ename_rnd_init(PSI_table_handle *h [[maybe_unused]],
                   bool scan [[maybe_unused]]) {
  return 0;
}

/* Set position of a cursor on a specific index */
int ename_rnd_pos(PSI_table_handle *handle) {
  Ename_Table_Handle *h = (Ename_Table_Handle *)handle;
  Ename_Record *record = &ename_records_array[h->m_pos.get_index()];

  if (record->m_exist) {
    /* Make the current row from records_array buffer */
    copy_record(&h->current_row, record);
  }

  return 0;
}

/* Initialize the table index */
int ename_index_init(PSI_table_handle *handle, uint idx,
                     bool sorted [[maybe_unused]], PSI_index_handle **index) {
  Ename_Table_Handle *h = (Ename_Table_Handle *)handle;

  /* If there are multiple indexes, initialize based on the idx provided */
  switch (idx) {
    case 0: {
      h->index_num = idx;
      Ename_index_by_emp_num *i = &h->m_emp_num_index;
      /* Initialize first key in index */
      i->m_emp_num.m_name = "EMPLOYEE_NUMBER";
      i->m_emp_num.m_find_flags = 0;
      *index = (PSI_index_handle *)i;
    } break;
    case 1: {
      h->index_num = idx;
      Ename_index_by_emp_fname *i = &h->m_emp_fname_index;
      i->m_emp_fname.m_name = "FIRST_NAME";
      i->m_emp_fname.m_find_flags = 0;
      i->m_emp_fname.m_value_buffer = i->m_emp_fname_buffer;
      i->m_emp_fname.m_value_buffer_capacity = sizeof(i->m_emp_fname_buffer);

      *index = (PSI_index_handle *)i;
    } break;
    default:
      assert(0);
      break;
  }
  return 0;
}

/* For each key in index, read value specified in query */
int ename_index_read(PSI_index_handle *index, PSI_key_reader *reader,
                     unsigned int idx, int find_flag) {
  switch (idx) {
    case 0: {
      Ename_index_by_emp_num *i = (Ename_index_by_emp_num *)index;
      /* Read all keys on index one by one */
      col_int_svc->read_key(reader, &i->m_emp_num, find_flag);
    } break;
    case 1: {
      Ename_index_by_emp_fname *i = (Ename_index_by_emp_fname *)index;
      /* Read all keys on index one by one */
      col_string_svc->read_key_string(reader, &i->m_emp_fname, find_flag);
    } break;
    default:
      assert(0);
      break;
  }

  return 0;
}

/* Read the next indexed value */
int ename_index_next(PSI_table_handle *handle) {
  Ename_Table_Handle *h = (Ename_Table_Handle *)handle;
  Ename_index *i = nullptr;

  switch (h->index_num) {
    case 0:
      i = (Ename_index_by_emp_num *)&h->m_emp_num_index;
      break;
    case 1:
      i = (Ename_index_by_emp_fname *)&h->m_emp_fname_index;
      break;
    default:
      assert(0);
      break;
  }

  for (h->m_pos.set_at(&h->m_next_pos); h->m_pos.has_more(); h->m_pos.next()) {
    Ename_Record *record = &ename_records_array[h->m_pos.get_index()];

    if (record->m_exist) {
      if (i->match(record)) {
        copy_record(&h->current_row, record);
        h->m_next_pos.set_after(&h->m_pos);
        return 0;
      }
    }
  }

  return PFS_HA_ERR_END_OF_FILE;
}

/* Reset cursor position */
void ename_reset_position(PSI_table_handle *handle) {
  Ename_Table_Handle *h = (Ename_Table_Handle *)handle;
  h->m_pos.reset();
  h->m_next_pos.reset();
  return;
}

/* Read current row from the current_row and display them in the table */
int ename_read_column_value(PSI_table_handle *handle, PSI_field *field,
                            uint index) {
  Ename_Table_Handle *h = (Ename_Table_Handle *)handle;

  switch (index) {
    case 0: /* EMPLOYEE_NUMBER */
      col_int_svc->set(field, h->current_row.e_number);
      break;
    case 1: /* FIRST_NAME */
      col_string_svc->set_char_utf8mb4(field, h->current_row.f_name,
                                       h->current_row.f_name_length);
      break;
    case 2: /* LAST_NAME */
      col_string_svc->set_varchar_utf8mb4_len(field, h->current_row.l_name,
                                              h->current_row.l_name_length);
      break;
    default: /* We should never reach here */
      assert(0);
      break;
  }

  return 0;
}

/* Store row data into records array */
int ename_write_row_values(PSI_table_handle *handle) {
  Ename_Table_Handle *h = (Ename_Table_Handle *)handle;

  mysql_mutex_lock(&LOCK_ename_records_array);

  /* If there is no more space for inserting a record, return */
  if (ename_rows_in_table >= EMPLOYEEE_NAME_MAX_ROWS) {
    mysql_mutex_unlock(&LOCK_ename_records_array);
    return PFS_HA_ERR_RECORD_FILE_FULL;
  }

  h->current_row.m_exist = true;

  if (is_duplicate(&h->current_row, -1)) {
    mysql_mutex_unlock(&LOCK_ename_records_array);
    return PFS_HA_ERR_FOUND_DUPP_KEY;
  }

  copy_record(&ename_records_array[ename_next_available_index],
              &h->current_row);
  ename_rows_in_table++;

  /* set next available index */
  if (ename_rows_in_table < EMPLOYEEE_NAME_MAX_ROWS) {
    int i = (ename_next_available_index + 1) % EMPLOYEEE_NAME_MAX_ROWS;
    int itr_count = 0;
    while (itr_count < EMPLOYEEE_NAME_MAX_ROWS) {
      if (ename_records_array[i].m_exist == false) {
        ename_next_available_index = i;
        break;
      }
      i = (i + 1) % EMPLOYEEE_NAME_MAX_ROWS;
      itr_count++;
    }
  }

  mysql_mutex_unlock(&LOCK_ename_records_array);

  return 0;
}

/* Read field data to be written from Field and store that into buffer */
int ename_write_column_value(PSI_table_handle *handle, PSI_field *field,
                             unsigned int index) {
  Ename_Table_Handle *h = (Ename_Table_Handle *)handle;

  char *f_name = (char *)h->current_row.f_name;
  unsigned int *f_name_length = &h->current_row.f_name_length;
  char *l_name = (char *)h->current_row.l_name;
  unsigned int *l_name_length = &h->current_row.l_name_length;

  switch (index) {
    case 0: /* EMPLOYEE_NUMBER */
      col_int_svc->get(field, &h->current_row.e_number);
      break;
    case 1: /* FIRST_NAME */
      col_string_svc->get_char_utf8mb4(field, (char *)f_name, f_name_length);
      break;
    case 2: /* LAST_NAME */
      col_string_svc->get_varchar_utf8mb4(field, (char *)l_name, l_name_length);
      break;
    default: /* We should never reach here */
      assert(0);
      break;
  }

  return 0;
}

/* Update row data from records array */
int ename_update_row_values(PSI_table_handle *handle) {
  int result = 0;
  Ename_Table_Handle *h = (Ename_Table_Handle *)handle;

  Ename_Record *cur = &ename_records_array[h->m_pos.get_index()];

  assert(cur->m_exist == true);

  mysql_mutex_lock(&LOCK_ename_records_array);
  if (is_duplicate(&h->current_row, h->m_pos.get_index()))
    result = PFS_HA_ERR_FOUND_DUPP_KEY;
  else
    copy_record(cur, &h->current_row);
  mysql_mutex_unlock(&LOCK_ename_records_array);

  return result;
}

/* Read field data to be updated from Field and store that into buffer */
int ename_update_column_value(PSI_table_handle *handle, PSI_field *field,
                              unsigned int index) {
  Ename_Table_Handle *h = (Ename_Table_Handle *)handle;

  char *f_name = (char *)h->current_row.f_name;
  unsigned int *f_name_length = &h->current_row.f_name_length;
  char *l_name = (char *)h->current_row.l_name;
  unsigned int *l_name_length = &h->current_row.l_name_length;

  switch (index) {
    case 0: /* EMPLOYEE_NUMBER */
      col_int_svc->get(field, &h->current_row.e_number);
      break;
    case 1: /* FIRST_NAME */
      col_string_svc->get_char_utf8mb4(field, (char *)f_name, f_name_length);
      break;
    case 2: /* LAST_NAME */
      col_string_svc->get_varchar_utf8mb4(field, (char *)l_name, l_name_length);
      break;
    default: /* We should never reach here */
      assert(0);
      break;
  }

  return 0;
}

/* Delete row data form records array */
int ename_delete_row_values(PSI_table_handle *handle) {
  Ename_Table_Handle *h = (Ename_Table_Handle *)handle;

  Ename_Record *cur = &ename_records_array[h->m_pos.get_index()];

  assert(cur->m_exist == true);

  mysql_mutex_lock(&LOCK_ename_records_array);
  cur->m_exist = false;
  ename_rows_in_table--;
  mysql_mutex_unlock(&LOCK_ename_records_array);

  return 0;
}

int ename_delete_all_rows(void) {
  mysql_mutex_lock(&LOCK_ename_records_array);
  for (int i = 0; i < EMPLOYEEE_NAME_MAX_ROWS; i++)
    ename_records_array[i].m_exist = false;
  ename_rows_in_table = 0;
  ename_next_available_index = 0;
  mysql_mutex_unlock(&LOCK_ename_records_array);
  return 0;
}

unsigned long long ename_get_row_count(void) { return ename_rows_in_table; }

void init_ename_share(PFS_engine_table_share_proxy *share) {
  share->m_table_name = "pfs_example_employee_name";
  share->m_table_name_length = 25;
  share->m_table_definition =
      "EMPLOYEE_NUMBER INTEGER, FIRST_NAME char(20), LAST_NAME varchar(20), "
      "PRIMARY "
      "KEY(`EMPLOYEE_NUMBER`), KEY(`FIRST_NAME`)";
  share->m_ref_length = sizeof(Ename_POS);
  share->m_acl = EDITABLE;
  share->get_row_count = ename_get_row_count;
  share->delete_all_rows = ename_delete_all_rows;

  /* Initialize PFS_engine_table_proxy */
  share->m_proxy_engine_table = {ename_rnd_next,
                                 ename_rnd_init,
                                 ename_rnd_pos,
                                 ename_index_init,
                                 ename_index_read,
                                 ename_index_next,
                                 ename_read_column_value,
                                 ename_reset_position,
                                 ename_write_column_value,
                                 ename_write_row_values,
                                 ename_update_column_value,
                                 ename_update_row_values,
                                 ename_delete_row_values,
                                 ename_open_table,
                                 ename_close_table};
}
