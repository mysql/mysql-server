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

#include "plugin/pfs_table_plugin/pfs_example_machines_by_emp_by_mtype.h"

PFS_engine_table_share_proxy m_by_emp_by_mtype_st_share;

/**
 * Instantiate Table_Handle at plugin code when corresponding table
 * in performance schema is opened.
 */
PSI_table_handle *m_by_emp_by_mtype_open_table(PSI_pos **pos) {
  M_by_emp_by_mtype_Table_Handle *temp = new M_by_emp_by_mtype_Table_Handle();
  *pos = (PSI_pos *)(&temp->m_pos);
  return (PSI_table_handle *)temp;
}

/**
 * Destroy the Table_Handle at plugin code when corresponding table
 * in performance schema is closed.
 */
void m_by_emp_by_mtype_close_table(PSI_table_handle *handle) {
  M_by_emp_by_mtype_Table_Handle *temp =
      (M_by_emp_by_mtype_Table_Handle *)handle;
  delete temp;
}

/* Reset current record */
static void reset_record(M_by_emp_by_mtype_record *record) {
  record->m_exist = false;
  record->f_name[0] = '\0';
  record->f_name_length = 0;
  record->l_name[0] = '\0';
  record->l_name_length = 0;
  record->machine_type.val = TYPE_END;
  record->machine_type.is_null = false;
  record->count.val = 0;
  record->count.is_null = false;
}

/* Make a record from employee and machine record */
static void make_record(M_by_emp_by_mtype_record *record,
                        Ename_Record *e_record, Machine_Record *m_record) {
  record->count.val = 1;
  record->count.is_null = false;
  record->f_name_length = e_record->f_name_length;
  strncpy(record->f_name, e_record->f_name, record->f_name_length);
  record->l_name_length = e_record->l_name_length;
  strncpy(record->l_name, e_record->l_name, record->l_name_length);
  record->machine_type = m_record->machine_type;
  record->m_exist = true;
}

/* Define implementation of PFS_engine_table_proxy. */
int m_by_emp_by_mtype_rnd_next(PSI_table_handle *handle) {
  M_by_emp_by_mtype_Table_Handle *h = (M_by_emp_by_mtype_Table_Handle *)handle;
  enum machine_type_enum machine_type;

  /* Loop for employees */
  for (h->m_pos.set_at(&h->m_next_pos); h->m_pos.has_more_employee();
       h->m_pos.next_employee()) {
    Ename_Record *e_record = &ename_records_array[h->m_pos.m_index_1];

    if (e_record->m_exist) {
      /* Loop for machine types */
      for (; h->m_pos.has_more_machine_type(); h->m_pos.next_machine_type()) {
        machine_type = (enum machine_type_enum)h->m_pos.m_index_2;

        /* Reset the record */
        reset_record(&h->current_row);

        std::vector<Machine_Record>::iterator it =
            machine_records_vector.begin();
        while (it != machine_records_vector.end()) {
          Machine_Record *m_record = &(*it);
          if (m_record->employee_number.val == e_record->e_number.val &&
              m_record->machine_type.val == machine_type) {
            if (h->current_row.m_exist) /* increment count */
              h->current_row.count.val++;
            else
              /* Make current row */
              make_record(&h->current_row, e_record, m_record);
          }
          ++it;
        }
        /* If a record was formed */
        if (h->current_row.m_exist) {
          h->m_next_pos.set_after(&h->m_pos);
          return 0;
        }
      }
    }
  }

  return PFS_HA_ERR_END_OF_FILE;
}

int m_by_emp_by_mtype_rnd_init(PSI_table_handle *h [[maybe_unused]],
                               bool scan [[maybe_unused]]) {
  return 0;
}

/* Set position of a cursor on a specific index */
int m_by_emp_by_mtype_rnd_pos(PSI_table_handle *handle) {
  M_by_emp_by_mtype_Table_Handle *h = (M_by_emp_by_mtype_Table_Handle *)handle;
  Ename_Record *e_record = &ename_records_array[h->m_pos.m_index_1];
  Machine_Record *m_record = &machine_records_vector[h->m_pos.m_index_2];

  if (e_record && e_record->m_exist && m_record && m_record->m_exist) {
    /* Make the current row from records_array buffer */
    make_record(&h->current_row, e_record, m_record);
  }
  return 0;
}

/* Initialize the table index */
int m_by_emp_by_mtype_index_init(PSI_table_handle *handle [[maybe_unused]],
                                 uint idx [[maybe_unused]],
                                 bool sorted [[maybe_unused]],
                                 PSI_index_handle **index [[maybe_unused]]) {
  /* No Index */
  return 0;
}

/* For each key in index, read value specified in query */
int m_by_emp_by_mtype_index_read(PSI_index_handle *index [[maybe_unused]],
                                 PSI_key_reader *reader [[maybe_unused]],
                                 unsigned int idx [[maybe_unused]],
                                 int find_flag [[maybe_unused]]) {
  /* No Index */
  return 0;
}

/* Read the next indexed value */
int m_by_emp_by_mtype_index_next(PSI_table_handle *handle [[maybe_unused]]) {
  /* No Index */
  return 0;
}

/* Reset cursor position */
void m_by_emp_by_mtype_reset_position(PSI_table_handle *handle) {
  M_by_emp_by_mtype_Table_Handle *h = (M_by_emp_by_mtype_Table_Handle *)handle;
  h->m_pos.reset();
  h->m_next_pos.reset();
  return;
}

/* Read current row from the current_row and display them in the table */
int m_by_emp_by_mtype_read_column_value(PSI_table_handle *handle,
                                        PSI_field *field, uint index) {
  M_by_emp_by_mtype_Table_Handle *h = (M_by_emp_by_mtype_Table_Handle *)handle;

  switch (index) {
    case 0: /* FIRST_NAME */
      col_string_svc->set_char_utf8mb4(field, h->current_row.f_name,
                                       h->current_row.f_name_length);
      break;
    case 1: /* LAST_NAME */
      col_string_svc->set_char_utf8mb4(field, h->current_row.l_name,
                                       h->current_row.l_name_length);
      break;
    case 2: /* MACHINE_TYPE */
      col_enum_svc->set(field, h->current_row.machine_type);
      break;
    case 3: /* COUNT */
      col_int_svc->set(field, h->current_row.count);
      break;
    default: /* We should never reach here */
      assert(0);
      break;
  }

  return 0;
}

unsigned long long m_by_emp_by_mtype_get_row_count(void) { return 0; }

void init_m_by_emp_by_mtype_share(PFS_engine_table_share_proxy *share) {
  share->m_table_name = "pfs_example_machine_by_employee_by_type";
  share->m_table_name_length = 40;
  share->m_table_definition =
      "FIRST_NAME char(20), LAST_NAME char(20), MACHINE_TYPE "
      "enum('LAPTOP','DESKTOP','MOBILE'), COUNT INTEGER";
  share->m_ref_length = sizeof(POS_m_by_emp_by_mtype);
  share->m_acl = READONLY;
  share->get_row_count = m_by_emp_by_mtype_get_row_count;
  share->delete_all_rows = nullptr; /* READONLY TABLE */

  /* Initialize PFS_engine_table_proxy */
  share->m_proxy_engine_table = {m_by_emp_by_mtype_rnd_next,
                                 m_by_emp_by_mtype_rnd_init,
                                 m_by_emp_by_mtype_rnd_pos,
                                 m_by_emp_by_mtype_index_init,
                                 m_by_emp_by_mtype_index_read,
                                 m_by_emp_by_mtype_index_next,
                                 m_by_emp_by_mtype_read_column_value,
                                 m_by_emp_by_mtype_reset_position,
                                 nullptr, /* READONLY TABLE */
                                 nullptr, /* READONLY TABLE */
                                 nullptr, /* READONLY TABLE */
                                 nullptr, /* READONLY TABLE */
                                 nullptr, /* READONLY TABLE */
                                 m_by_emp_by_mtype_open_table,
                                 m_by_emp_by_mtype_close_table};
}
