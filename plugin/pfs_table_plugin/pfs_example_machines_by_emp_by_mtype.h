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

#ifndef PLUGIN_PFS_TABLE_MACHINES_BY_EMP_BY_MTYPE_H_
#define PLUGIN_PFS_TABLE_MACHINES_BY_EMP_BY_MTYPE_H_

#include <mysql/components/service_implementation.h>
#include <mysql/components/services/pfs_plugin_table_service.h>
#include <mysql/plugin.h>

#include "plugin/pfs_table_plugin/pfs_example_employee_name.h"
#include "plugin/pfs_table_plugin/pfs_example_machine.h"

/* Service handle */
extern SERVICE_TYPE(pfs_plugin_table) * table_svc;

/* Global share pointer for table */
extern PFS_engine_table_share_proxy m_by_emp_by_mtype_st_share;

/* A structure to denote a single row of the table. */
class M_by_emp_by_mtype_record
{
public:
  char f_name[20];
  unsigned int f_name_length;
  char l_name[20];
  unsigned int l_name_length;
  PSI_enum machine_type;
  PSI_int count;

  /* If there is a value in this row */
  bool m_exist;
};

/* A class to define position of cursor in table. */
struct POS_m_by_emp_by_mtype
{
  /** Outer index for employee. */
  unsigned int m_index_1;
  /** Current index within index_1, for machine type. */
  unsigned int m_index_2;

  /**
    Constructor.
    @param index_1 the first index initial value.
    @param index_2 the second index initial value.
  */
  POS_m_by_emp_by_mtype(uint index_1, uint index_2)
    : m_index_1(index_1), m_index_2(index_2)
  {
  }

  POS_m_by_emp_by_mtype() : m_index_1(0), m_index_2(0)
  {
  }
  /**
    Set this index at a given position.
  */
  void
  set_at(uint index_1, uint index_2)
  {
    m_index_1 = index_1;
    m_index_2 = index_2;
  }

  /**
    Set this index at a given position.
    @param other a position
  */
  void
  set_at(const POS_m_by_emp_by_mtype* other)
  {
    m_index_1 = other->m_index_1;
    m_index_2 = other->m_index_2;
  }

  void
  reset()
  {
    m_index_1 = 0;
    m_index_2 = 0;
  }

  /**
    Set this index after a given position.
    @param other a position
  */
  void
  set_after(const POS_m_by_emp_by_mtype* other)
  {
    m_index_1 = other->m_index_1;
    m_index_2 = other->m_index_2 + 1;
  }

  bool
  has_more_employee()
  {
    if (m_index_1 < EMPLOYEEE_NAME_MAX_ROWS)
      return true;
    return false;
  }

  bool
  has_more_machine_type()
  {
    if (m_index_2 < (unsigned int)TYPE_END)
      return true;
    return false;
  }

  void
  next_machine_type()
  {
    m_index_2++;
  }

  void
  next_employee()
  {
    m_index_1++;
    m_index_2 = 0;
  }
};

/* A structure to define a handle for table in plugin/component code. */
typedef struct
{
  /* Current position instance */
  POS_m_by_emp_by_mtype m_pos;
  /* Next position instance */
  POS_m_by_emp_by_mtype m_next_pos;

  /* Current row for the table */
  M_by_emp_by_mtype_record current_row;
} M_by_emp_by_mtype_Table_Handle;

PSI_table_handle* m_by_emp_by_mtype_open_table(PSI_pos** pos);
void m_by_emp_by_mtype_close_table(PSI_table_handle* handle);
int m_by_emp_by_mtype_rnd_next(PSI_table_handle* handle);
int m_by_emp_by_mtype_rnd_init(PSI_table_handle* h, bool scan);
int m_by_emp_by_mtype_rnd_pos(PSI_table_handle* handle);
int m_by_emp_by_mtype_index_init(PSI_table_handle* handle,
                                 uint idx,
                                 bool sorted,
                                 PSI_index_handle** index);
int m_by_emp_by_mtype_index_read(PSI_index_handle* index,
                                 PSI_key_reader* reader,
                                 unsigned int idx,
                                 int find_flag);
int m_by_emp_by_mtype_index_next(PSI_table_handle* handle);
void m_by_emp_by_mtype_reset_position(PSI_table_handle* handle);
int m_by_emp_by_mtype_read_column_value(PSI_table_handle* handle,
                                        PSI_field* field,
                                        uint index);
int m_by_emp_by_mtype_write_row_values(PSI_table_handle* handle);
int m_by_emp_by_mtype_write_column_value(PSI_table_handle* handle,
                                         PSI_field* field,
                                         unsigned int index);
int m_by_emp_by_mtype_update_row_values(PSI_table_handle* handle);
int m_by_emp_by_mtype_update_column_value(PSI_table_handle* handle,
                                          PSI_field* field,
                                          unsigned int index);
int m_by_emp_by_mtype_delete_row_values(PSI_table_handle* handle);
int m_by_emp_by_mtype_delete_all_rows(void);
unsigned long long m_by_emp_by_mtype_get_row_count(void);
void init_m_by_emp_by_mtype_share(PFS_engine_table_share_proxy* share);

#endif /* PLUGIN_PFS_TABLE_MACHINES_BY_EMP_BY_MTYPE_H_ */
