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

#ifndef PLUGIN_PFS_TABLE_PLUGIN_pfs_example_employee_salary_H_
#define PLUGIN_PFS_TABLE_PLUGIN_pfs_example_employee_salary_H_

#include <algorithm>
#include <vector>
#include <mysql/plugin.h>
#include <mysql/components/service_implementation.h>
#include <mysql/components/services/pfs_plugin_table_service.h>

/* Service handle */
extern SERVICE_TYPE(pfs_plugin_table) * table_svc;

/* Global share pointer for pfs_example_employee_salary table */
extern PFS_engine_table_share_proxy esalary_st_share;

/* A mutex instance to protect:
 * - esalary_rows_in_table
 * - esalary_records_vector
 */
extern mysql_mutex_t LOCK_esalary_records_array;

/* A structure to denote a single row of the table. */
struct
{
public:
  PSI_int e_number;
  PSI_bigint e_salary;
  char e_dob[20];
  unsigned int e_dob_length;
  char e_tob[20];
  unsigned int e_tob_length;

  /* If there is a value in this row */
  bool m_exist;
} typedef Esalary_Record;

/**
 * An array to keep rows of the tables.
 * When a row is inserted in plugin table, it will be stored here.
 * When a row is queried from plugin table, it will be fetched from here.
 */
extern std::vector<Esalary_Record> esalary_records_vector;

/* A class to define position of cursor in table. */
class Esalary_POS
{
private:
  unsigned int m_index;

public:
  ~Esalary_POS(){};
  Esalary_POS()
  {
    m_index = 0;
  }

  bool
  has_more()
  {
    if (m_index < esalary_records_vector.size())
      return true;
    return false;
  }
  void
  next()
  {
    m_index++;
  }

  void
  reset()
  {
    m_index = 0;
  }

  unsigned int
  get_index()
  {
    return m_index;
  }

  void
  set_at(unsigned int index)
  {
    m_index = index;
  }

  void
  set_at(Esalary_POS* pos)
  {
    m_index = pos->m_index;
  }

  void
  set_after(Esalary_POS* pos)
  {
    m_index = pos->m_index + 1;
  }
};

/* A structure to define a handle for table in plugin/component code. */
typedef struct
{
  /* Current position instance */
  Esalary_POS m_pos;
  /* Next position instance */
  Esalary_POS m_next_pos;

  /* Current row for the table */
  Esalary_Record current_row;
} Esalary_Table_Handle;

PSI_table_handle* esalary_open_table(PSI_pos** pos);
void esalary_close_table(PSI_table_handle* handle);
int esalary_rnd_next(PSI_table_handle* handle);
int esalary_rnd_init(PSI_table_handle* h, bool scan);
int esalary_rnd_pos(PSI_table_handle* handle);
int esalary_index_init(PSI_table_handle* handle,
                       uint idx,
                       bool sorted,
                       PSI_index_handle** index);
int esalary_index_read(PSI_index_handle* index,
                       PSI_key_reader* reader,
                       unsigned int idx,
                       int find_flag);
int esalary_index_next(PSI_table_handle* handle);
void esalary_reset_position(PSI_table_handle* handle);
int esalary_read_column_value(PSI_table_handle* handle,
                              PSI_field* field,
                              uint index);
int esalary_write_row_values(PSI_table_handle* handle);
int esalary_write_column_value(PSI_table_handle* handle,
                               PSI_field* field,
                               unsigned int index);
int esalary_update_row_values(PSI_table_handle* handle);
int esalary_update_column_value(PSI_table_handle* handle,
                                PSI_field* field,
                                unsigned int index);
int esalary_delete_row_values(PSI_table_handle* handle);
int esalary_delete_all_rows(void);
unsigned long long esalary_get_row_count(void);
void init_esalary_share(PFS_engine_table_share_proxy* share);

#endif /* PLUGIN_PFS_TABLE_PLUGIN_pfs_example_employee_salary_H_ */
