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

#ifndef PLUGIN_PFS_TABLE_PLUGIN_pfs_example_employee_name_H_
#define PLUGIN_PFS_TABLE_PLUGIN_pfs_example_employee_name_H_

#include <mysql/components/service_implementation.h>
#include <mysql/components/services/pfs_plugin_table_service.h>
#include <mysql/plugin.h>

/* Service handle */
extern SERVICE_TYPE(pfs_plugin_table) * table_svc;

/* Global share pointer for pfs_example_employee_name table */
extern PFS_engine_table_share_proxy ename_st_share;

/* Maximum number of rows in the table */
#define EMPLOYEEE_NAME_MAX_ROWS 100

/* A mutex instance to protect:
 * - ename_rows_in_table
 * - ename_next_available_index
 * - ename_records_array
 */
extern mysql_mutex_t LOCK_ename_records_array;

/* A structure to denote a single row of the table. */
struct {
 public:
  PSI_int e_number;
  char f_name[20];
  unsigned int f_name_length;
  char l_name[20];
  unsigned int l_name_length;

  /* If there is a value in this row */
  bool m_exist;
} typedef Ename_Record;

/**
 * An array to keep rows of the tables.
 * When a row is inserted in plugin table, it will be stored here.
 * When a row is queried from plugin table, it will be fetched from here.
 */
extern Ename_Record ename_records_array[EMPLOYEEE_NAME_MAX_ROWS];

/* A class to define position of cursor in table. */
class Ename_POS {
 private:
  unsigned int m_index;

 public:
  ~Ename_POS() {}
  Ename_POS() { m_index = 0; }

  bool has_more() {
    if (m_index < EMPLOYEEE_NAME_MAX_ROWS) return true;
    return false;
  }
  void next() { m_index++; }

  void reset() { m_index = 0; }

  unsigned int get_index() { return m_index; }

  void set_at(unsigned int index) { m_index = index; }

  void set_at(Ename_POS *pos) { m_index = pos->m_index; }

  void set_after(Ename_POS *pos) { m_index = pos->m_index + 1; }
};

class Ename_index {
 public:
  virtual ~Ename_index() {}
  virtual bool match(Ename_Record *record) = 0;
};

/* And index on Employee Number */
class Ename_index_by_emp_num : public Ename_index {
 public:
  PSI_plugin_key_integer m_emp_num;

  bool match(Ename_Record *record) {
    return table_svc->match_key_integer(false, record->e_number.val,
                                        &m_emp_num);
  }
};

/* An index on Employee First Name */
class Ename_index_by_emp_fname : public Ename_index {
 public:
  PSI_plugin_key_string m_emp_fname;
  char m_emp_fname_buffer[20];

  bool match(Ename_Record *record) {
    return table_svc->match_key_string(false, record->f_name,
                                       record->f_name_length, &m_emp_fname);
  }
};

/* A structure to define a handle for table in plugin/component code. */
typedef struct {
  /* Current position instance */
  Ename_POS m_pos;
  /* Next position instance */
  Ename_POS m_next_pos;

  /* Current row for the table */
  Ename_Record current_row;

  /* Index on table */
  Ename_index_by_emp_num m_emp_num_index;
  Ename_index_by_emp_fname m_emp_fname_index;

  /* Index indicator */
  unsigned int index_num;
} Ename_Table_Handle;

PSI_table_handle *ename_open_table(PSI_pos **pos);
void ename_close_table(PSI_table_handle *handle);
int ename_rnd_next(PSI_table_handle *handle);
int ename_rnd_init(PSI_table_handle *h, bool scan);
int ename_rnd_pos(PSI_table_handle *handle);
int ename_index_init(PSI_table_handle *handle, uint idx, bool sorted,
                     PSI_index_handle **index);
int ename_index_read(PSI_index_handle *index, PSI_key_reader *reader,
                     unsigned int idx, int find_flag);
int ename_index_next(PSI_table_handle *handle);
void ename_reset_position(PSI_table_handle *handle);
int ename_read_column_value(PSI_table_handle *handle, PSI_field *field,
                            uint index);
int ename_write_row_values(PSI_table_handle *handle);
int ename_write_column_value(PSI_table_handle *handle, PSI_field *field,
                             unsigned int index);
int ename_update_row_values(PSI_table_handle *handle);
int ename_update_column_value(PSI_table_handle *handle, PSI_field *field,
                              unsigned int index);
int ename_delete_row_values(PSI_table_handle *handle);
int ename_delete_all_rows(void);
unsigned long long ename_get_row_count(void);
void init_ename_share(PFS_engine_table_share_proxy *share);

#endif /* PLUGIN_PFS_TABLE_PLUGIN_pfs_example_employee_name_H_ */
