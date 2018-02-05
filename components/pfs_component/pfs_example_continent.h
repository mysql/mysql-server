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

#ifndef PLUGIN_PFS_TABLE_PLUGIN_pfs_example_continent_H_
#define PLUGIN_PFS_TABLE_PLUGIN_pfs_example_continent_H_

#include <mysql/components/service_implementation.h>
#include <mysql/components/services/pfs_plugin_table_service.h>

#include "pfs_example_component_population.h"
#include "thr_mutex.h"

/* Global share pointer for pfs_example_continent table */
extern PFS_engine_table_share_proxy continent_st_share;

/* Maximum number of rows in the table */
#define CONTINENT_MAX_ROWS 10

/* A mutex instance to protect:
 * - continent_rows_in_table
 * - continent_next_available_index
 * - continent_records_array
 */
extern native_mutex_t LOCK_continent_records_array;

/* A structure to denote a single row of the table. */
struct {
  char name[20];
  unsigned int name_length;

  /* If there is a value in this row */
  bool m_exist;
} typedef Continent_record;

/**
 * An array to keep rows of the tables.
 * When a row is inserted in plugin table, it will be stored here.
 * When a row is queried from plugin table, it will be fetched from here.
 */
extern Continent_record continent_records_array[CONTINENT_MAX_ROWS];

/* A class to define position of cursor in table. */
class Continent_POS {
 private:
  unsigned int m_index;

 public:
  ~Continent_POS(){};
  Continent_POS() { m_index = 0; }

  bool has_more() {
    if (m_index < CONTINENT_MAX_ROWS) return true;
    return false;
  }
  void next() { m_index++; }

  void reset() { m_index = 0; }

  unsigned int get_index() { return m_index; }

  void set_at(unsigned int index) { m_index = index; }

  void set_at(Continent_POS *pos) { m_index = pos->m_index; }

  void set_after(Continent_POS *pos) { m_index = pos->m_index + 1; }
};

class Continent_index {
 public:
  virtual ~Continent_index(){};

  virtual bool match(Continent_record *record) = 0;
};

/* An index on Continent Name */
class Continent_index_by_name : public Continent_index {
 public:
  PSI_plugin_key_string m_name;
  char m_name_buffer[20];

  bool match(Continent_record *record) {
    return mysql_service_pfs_plugin_table->match_key_string(
        false, record->name, record->name_length, &m_name);
  }
};

/* A structure to define a handle for table in plugin/component code. */
typedef struct {
  /* Current position instance */
  Continent_POS m_pos;
  /* Next position instance */
  Continent_POS m_next_pos;

  /* Current row for the table */
  Continent_record current_row;

  /* Index on table */
  Continent_index_by_name m_index;

  /* Index indicator */
  unsigned int index_num;
} Continent_Table_Handle;

PSI_table_handle *continent_open_table(PSI_pos **pos);
void continent_close_table(PSI_table_handle *handle);
int continent_rnd_next(PSI_table_handle *handle);
int continent_rnd_init(PSI_table_handle *h, bool scan);
int continent_rnd_pos(PSI_table_handle *handle);
int continent_index_init(PSI_table_handle *handle, unsigned int idx,
                         bool sorted, PSI_index_handle **index);
int continent_index_read(PSI_index_handle *index, PSI_key_reader *reader,
                         unsigned int idx, int find_flag);
int continent_index_next(PSI_table_handle *handle);
void continent_reset_position(PSI_table_handle *handle);
int continent_read_column_value(PSI_table_handle *handle, PSI_field *field,
                                unsigned int index);
int continent_write_row_values(PSI_table_handle *handle);
int continent_write_column_value(PSI_table_handle *handle, PSI_field *field,
                                 unsigned int index);
int continent_update_row_values(PSI_table_handle *handle);
int continent_update_column_value(PSI_table_handle *handle, PSI_field *field,
                                  unsigned int index);
int continent_delete_row_values(PSI_table_handle *handle);
int continent_delete_all_rows(void);
unsigned long long continent_get_row_count(void);
void init_continent_share(PFS_engine_table_share_proxy *share);

extern int write_rows_from_component(Continent_Table_Handle *h);

#endif /* PLUGIN_PFS_TABLE_PLUGIN_pfs_example_continent_H_ */
