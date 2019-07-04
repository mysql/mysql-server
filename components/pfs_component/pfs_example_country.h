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

#ifndef PLUGIN_PFS_TABLE_PLUGIN_POC_PLUGIN_COUNTRY_H_
#define PLUGIN_PFS_TABLE_PLUGIN_POC_PLUGIN_COUNTRY_H_

#include <mysql/components/service_implementation.h>
#include <mysql/components/services/pfs_plugin_table_service.h>

#include "pfs_example_component_population.h"
#include "thr_mutex.h"

/* Global share pointer for pfs_example_country table */
extern PFS_engine_table_share_proxy country_st_share;

/* Maximum number of rows in the table */
#define COUNTRY_MAX_ROWS 10

/* A mutex instance to protect:
 * - country_rows_in_table
 * - country_next_available_index
 * - country_records_array
 */
extern native_mutex_t LOCK_country_records_array;

/* A structure to denote a single row of the table. */
class Country_record {
 public:
  char name[20];
  unsigned int name_length;
  char continent_name[20];
  unsigned int continent_name_length;
  PSI_year year;
  PSI_bigint population;
  PSI_double growth_factor;
  /* If there is a value in this row */
  bool m_exist;
};

/**
 * An array to keep rows of the tables.
 * When a row is inserted in plugin table, it will be stored here.
 * When a row is queried from plugin table, it will be fetched from here.
 */
extern Country_record country_records_array[COUNTRY_MAX_ROWS];

/* A class to define position of cursor in table. */
class Country_POS {
 private:
  unsigned int m_index;

 public:
  ~Country_POS() {}
  Country_POS() { m_index = 0; }

  bool has_more() {
    if (m_index < COUNTRY_MAX_ROWS) return true;
    return false;
  }
  void next() { m_index++; }

  void reset() { m_index = 0; }

  unsigned int get_index() { return m_index; }

  void set_at(unsigned int index) { m_index = index; }

  void set_at(Country_POS *pos) { m_index = pos->m_index; }

  void set_after(Country_POS *pos) { m_index = pos->m_index + 1; }
};

class Country_index {
 public:
  virtual ~Country_index() {}

  virtual bool match(Country_record *record) = 0;
};

/* An index on Country Name */
class Country_index_by_name : public Country_index {
 public:
  PSI_plugin_key_string m_continent_name;
  char m_continent_name_buffer[20];

  PSI_plugin_key_string m_country_name;
  char m_country_name_buffer[20];

  bool match(Country_record *record) {
    return mysql_service_pfs_plugin_table->match_key_string(
               false, record->name, record->name_length, &m_country_name) &&
           mysql_service_pfs_plugin_table->match_key_string(
               false, record->continent_name, record->continent_name_length,
               &m_continent_name);
  }
};

/* A structure to define a handle for table in plugin/component code. */
typedef struct {
  /* Current position instance */
  Country_POS m_pos;
  /* Next position instance */
  Country_POS m_next_pos;

  /* Current row for the table */
  Country_record current_row;

  /* Current index for the table */
  Country_index_by_name m_index;

  /* Index indicator */
  unsigned int index_num;
} Country_Table_Handle;

PSI_table_handle *country_open_table(PSI_pos **pos);
void country_close_table(PSI_table_handle *handle);
int country_rnd_next(PSI_table_handle *handle);
int country_rnd_init(PSI_table_handle *h, bool scan);
int country_rnd_pos(PSI_table_handle *handle);
int country_index_init(PSI_table_handle *handle, unsigned int idx, bool sorted,
                       PSI_index_handle **index);
int country_index_read(PSI_index_handle *index, PSI_key_reader *reader,
                       unsigned int idx, int find_flag);
int country_index_next(PSI_table_handle *handle);
void country_reset_position(PSI_table_handle *handle);
int country_read_column_value(PSI_table_handle *handle, PSI_field *field,
                              unsigned int index);
int country_write_row_values(PSI_table_handle *handle);
int country_write_column_value(PSI_table_handle *handle, PSI_field *field,
                               unsigned int index);
int country_update_row_values(PSI_table_handle *handle);
int country_update_column_value(PSI_table_handle *handle, PSI_field *field,
                                unsigned int index);
int country_delete_row_values(PSI_table_handle *handle);
int country_delete_all_rows(void);
unsigned long long country_get_row_count(void);
void init_country_share(PFS_engine_table_share_proxy *share);

#endif /* PLUGIN_PFS_TABLE_PLUGIN_POC_PLUGIN_COUNTRY_H_ */
