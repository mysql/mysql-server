/* Copyright (c) 2017, 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License, version 2.0, for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <assert.h>
#include <cstring>

#include "pfs_example_continent.h"

/* Global share pointer for a table */
PFS_engine_table_share_proxy continent_st_share;

native_mutex_t LOCK_continent_records_array;

/* Total number of rows in table. */
unsigned int continent_rows_in_table = 0;

/* Next available index for new record to be stored in global record array. */
unsigned int continent_next_available_index = 0;

Continent_record continent_records_array[CONTINENT_MAX_ROWS] = {{"", 0, false}};

int continent_delete_all_rows(void) {
  native_mutex_lock(&LOCK_continent_records_array);
  for (int i = 0; i < CONTINENT_MAX_ROWS; i++)
    continent_records_array[i].m_exist = false;
  continent_rows_in_table = 0;
  continent_next_available_index = 0;
  native_mutex_unlock(&LOCK_continent_records_array);
  return 0;
}

/**
 * Instantiate Continent_Table_Handle at plugin code when corresponding table
 * in performance schema is opened.
 */
PSI_table_handle *continent_open_table(PSI_pos **pos) {
  Continent_Table_Handle *temp = new Continent_Table_Handle();
  temp->current_row.name_length = 0;

  *pos = (PSI_pos *)(&temp->m_pos);
  return (PSI_table_handle *)temp;
}

/**
 * Destroy the Continent_Table_Handle at plugin code when corresponding table
 * in performance schema is closed.
 */
void continent_close_table(PSI_table_handle *handle) {
  Continent_Table_Handle *temp = (Continent_Table_Handle *)handle;
  delete temp;
}

/* Copy record from source to destination */
static void copy_record(Continent_record *dest, Continent_record *source) {
  dest->name_length = source->name_length;
  strncpy(dest->name, source->name, dest->name_length);
  dest->m_exist = source->m_exist;
}

/* Define implementation of PFS_engine_table_proxy. */
int continent_rnd_next(PSI_table_handle *handle) {
  Continent_Table_Handle *h = (Continent_Table_Handle *)handle;

  for (h->m_pos.set_at(&h->m_next_pos); h->m_pos.has_more(); h->m_pos.next()) {
    Continent_record *record = &continent_records_array[h->m_pos.get_index()];

    if (record->m_exist) {
      /* Make the current row from records_array buffer */
      copy_record(&h->current_row, record);
      h->m_next_pos.set_after(&h->m_pos);
      return 0;
    }
  }

  return PFS_HA_ERR_END_OF_FILE;
}

int continent_rnd_init(PSI_table_handle *, bool) { return 0; }

/* Set position of a cursor on a specific index */
int continent_rnd_pos(PSI_table_handle *handle) {
  Continent_Table_Handle *h = (Continent_Table_Handle *)handle;
  Continent_record *record = &continent_records_array[h->m_pos.get_index()];

  if (record->m_exist) {
    /* Make the current row from records_array buffer */
    copy_record(&h->current_row, record);
  }

  return 0;
}

/* Initialize the table index */
int continent_index_init(PSI_table_handle *handle, unsigned int idx, bool,
                         PSI_index_handle **index) {
  Continent_Table_Handle *h = (Continent_Table_Handle *)handle;

  /* If there are multiple indexes, initialize based on the idx provided */
  switch (idx) {
    case 0: {
      h->index_num = idx;
      Continent_index_by_name *i = &h->m_index;
      /* Initialize first key in index */
      i->m_name.m_name = "NAME";
      i->m_name.m_find_flags = 0;
      i->m_name.m_value_buffer = i->m_name_buffer;
      i->m_name.m_value_buffer_capacity = sizeof(i->m_name_buffer);
      *index = (PSI_index_handle *)i;
    } break;
    default:
      assert(0);
      break;
  }

  return 0;
}

/* For each key in index, read value specified in query */
int continent_index_read(PSI_index_handle *index, PSI_key_reader *reader,
                         unsigned int idx, int find_flag) {
  switch (idx) {
    case 0: {
      Continent_index_by_name *i = (Continent_index_by_name *)index;
      /* Read all keys on index one by one */
      pc_string_srv->read_key_string(reader, &i->m_name, find_flag);
      /* Remember the number of key parts found. */
      i->m_fields = pt_srv->get_parts_found(reader);
      break;
    }
    default:
      assert(0);
      break;
  }

  return 0;
}

/* Read the next indexed value */
int continent_index_next(PSI_table_handle *handle) {
  Continent_Table_Handle *h = (Continent_Table_Handle *)handle;
  Continent_index *i = nullptr;

  switch (h->index_num) {
    case 0:
      i = (Continent_index_by_name *)&h->m_index;
      break;
    default:
      assert(0);
      break;
  }

  for (h->m_pos.set_at(&h->m_next_pos); h->m_pos.has_more(); h->m_pos.next()) {
    Continent_record *record = &continent_records_array[h->m_pos.get_index()];

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
void continent_reset_position(PSI_table_handle *handle) {
  Continent_Table_Handle *h = (Continent_Table_Handle *)handle;
  h->m_pos.reset();
  h->m_next_pos.reset();
  return;
}

/* Read current row from the current_row and display them in the table */
int continent_read_column_value(PSI_table_handle *handle, PSI_field *field,
                                unsigned int index) {
  Continent_Table_Handle *h = (Continent_Table_Handle *)handle;

  switch (index) {
    case 0: /* NAME */
      pc_string_srv->set_char_utf8mb4(field, h->current_row.name,
                                      h->current_row.name_length);
      break;
    default: /* We should never reach here */
      assert(0);
      break;
  }

  return 0;
}

/* As this is a read-only table, we can't use continent_write_row_values
   function, so use this function to populate rows from component code.
*/
int write_rows_from_component(Continent_Table_Handle *handle) {
  if (!handle) return 1;

  native_mutex_lock(&LOCK_continent_records_array);

  /* If there is no more space for inserting a record, return */
  if (continent_rows_in_table >= CONTINENT_MAX_ROWS) {
    native_mutex_unlock(&LOCK_continent_records_array);
    return 1;
  }

  copy_record(&continent_records_array[continent_next_available_index],
              &handle->current_row);
  continent_rows_in_table++;

  /* set next available index */
  if (continent_rows_in_table < CONTINENT_MAX_ROWS) {
    int i = (continent_next_available_index + 1) % CONTINENT_MAX_ROWS;
    int itr_count = 0;
    while (itr_count < CONTINENT_MAX_ROWS) {
      if (continent_records_array[i].m_exist == false) {
        continent_next_available_index = i;
        break;
      }
      i = (i + 1) % CONTINENT_MAX_ROWS;
      itr_count++;
    }
  }

  native_mutex_unlock(&LOCK_continent_records_array);
  return 0;
}

unsigned long long continent_get_row_count(void) {
  return continent_rows_in_table;
}

void init_continent_share(PFS_engine_table_share_proxy *share) {
  /* Instantiate and initialize PFS_engine_table_share_proxy */
  share->m_table_name = "pfs_example_continent";
  share->m_table_name_length = 21;
  share->m_table_definition = "NAME char(20) not null, PRIMARY KEY(NAME)";
  share->m_ref_length = sizeof(Continent_POS);
  share->m_acl = READONLY;
  share->get_row_count = continent_get_row_count;
  share->delete_all_rows = nullptr; /* READONLY TABLE */

  /* Initialize PFS_engine_table_proxy */
  share->m_proxy_engine_table = {
      continent_rnd_next, continent_rnd_init, continent_rnd_pos,
      continent_index_init, continent_index_read, continent_index_next,
      continent_read_column_value, continent_reset_position,
      /* READONLY TABLE */
      nullptr, /* write_column_value */
      nullptr, /* write_row_values */
      nullptr, /* update_column_value */
      nullptr, /* update_row_values */
      nullptr, /* delete_row_values */
      continent_open_table, continent_close_table};
}
