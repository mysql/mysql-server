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

#include "pfs_example_country.h"

PFS_engine_table_share_proxy country_st_share;
native_mutex_t LOCK_country_records_array;

/* Total number of rows in table. */
unsigned int country_rows_in_table = 0;

/* Next available index for new record to be stored in global record array. */
unsigned int country_next_available_index = 0;

Country_record country_records_array[COUNTRY_MAX_ROWS] = {
    {"", 0, "", 0, "", 0, {0, true}, {0, true}, {0, true}, false}};

/**
  Check for duplicate value of Primary/Unique Key column(s).
  A sequential search is being used here, but it is up to the plugin writer to
  implement his/her own search to make sure duplicate values are not
  inserted/updated for Primary/Unique Key Column(s).

  @param record record to be checked for duplicate
  @param skip_index element at this index not to be considered for comparison
  @return true if duplicate found, false otherwise
*/
bool is_duplicate(Country_record *record, int skip_index) {
  for (int i = 0; i < COUNTRY_MAX_ROWS; i++) {
    Country_record *temp = &country_records_array[i];
    if (!temp->m_exist || i == skip_index) continue;

    if ((temp->name_length == record->name_length) &&
        (strncmp(temp->name, record->name, temp->name_length) == 0) &&
        (temp->continent_name_length == record->continent_name_length) &&
        (strncmp(temp->continent_name, record->continent_name,
                 temp->continent_name_length) == 0))
      return true;
  }

  return false;
}

/**
 * Instantiate Country_Table_Handle at plugin code when corresponding table
 * in performance schema is opened.
 */
PSI_table_handle *country_open_table(PSI_pos **pos) {
  Country_Table_Handle *temp = new Country_Table_Handle();
  temp->current_row.name_length = 0;
  temp->current_row.continent_name_length = 0;
  temp->current_row.country_code_length = 0;
  temp->current_row.year.is_null = true;
  temp->current_row.population.is_null = true;
  temp->current_row.growth_factor.is_null = true;

  *pos = (PSI_pos *)(&temp->m_pos);
  return (PSI_table_handle *)temp;
}

/**
 * Destroy the Country_Table_Handle at plugin code when corresponding table
 * in performance schema is closed.
 */
void country_close_table(PSI_table_handle *handle) {
  Country_Table_Handle *temp = (Country_Table_Handle *)handle;
  delete temp;
}

/* Copy record from source to destination */
static void copy_record(Country_record *dest, Country_record *source) {
  dest->name_length = source->name_length;
  strncpy(dest->name, source->name, dest->name_length);
  dest->continent_name_length = source->continent_name_length;
  strncpy(dest->continent_name, source->continent_name,
          dest->continent_name_length);
  dest->year = source->year;
  dest->population = source->population;
  dest->growth_factor = source->growth_factor;
  dest->m_exist = source->m_exist;
  dest->country_code_length = source->country_code_length;
  strncpy(dest->country_code, source->country_code, dest->country_code_length);
}

/* Define implementation of PFS_engine_table_proxy. */
int country_rnd_next(PSI_table_handle *handle) {
  Country_Table_Handle *h = (Country_Table_Handle *)handle;

  for (h->m_pos.set_at(&h->m_next_pos); h->m_pos.has_more(); h->m_pos.next()) {
    Country_record *record = &country_records_array[h->m_pos.get_index()];

    if (record->m_exist) {
      /* Make the current row from records_array buffer */
      copy_record(&h->current_row, record);
      h->m_next_pos.set_after(&h->m_pos);
      return 0;
    }
  }

  return PFS_HA_ERR_END_OF_FILE;
}

int country_rnd_init(PSI_table_handle *, bool) { return 0; }

/* Set position of a cursor on a specific index */
int country_rnd_pos(PSI_table_handle *handle) {
  Country_Table_Handle *h = (Country_Table_Handle *)handle;
  Country_record *record = &country_records_array[h->m_pos.get_index()];

  if (record->m_exist) {
    /* Make the current row from records_array buffer */
    copy_record(&h->current_row, record);
  }

  return 0;
}

/* Initialize the table index */
int country_index_init(PSI_table_handle *handle, unsigned int idx, bool,
                       PSI_index_handle **index) {
  Country_Table_Handle *h = (Country_Table_Handle *)handle;

  /* If there are multiple indexes, initialize based on the idx provided */
  switch (idx) {
    case 0: {
      h->index_num = idx;
      Country_index_by_name *i = &h->m_index;
      /* Initialize first key in index */
      i->m_country_name.m_name = "NAME";
      i->m_country_name.m_find_flags = 0;
      i->m_country_name.m_value_buffer = i->m_country_name_buffer;
      i->m_country_name.m_value_buffer_capacity =
          sizeof(i->m_country_name_buffer);

      i->m_continent_name.m_name = "CONTINENT";
      i->m_continent_name.m_find_flags = 0;
      i->m_continent_name.m_value_buffer = i->m_continent_name_buffer;
      i->m_continent_name.m_value_buffer_capacity =
          sizeof(i->m_continent_name_buffer);

      *index = (PSI_index_handle *)i;
    } break;
    default:
      assert(0);
      break;
  }

  return 0;
}

/* For each key in index, read value specified in query */
int country_index_read(PSI_index_handle *index, PSI_key_reader *reader,
                       unsigned int idx, int find_flag) {
  switch (idx) {
    case 0: {
      Country_index_by_name *i = (Country_index_by_name *)index;
      /* Read all keys on index one by one */
      pc_string_srv->read_key_string(reader, &i->m_country_name, find_flag);
      pc_string_srv->read_key_string(reader, &i->m_continent_name, find_flag);

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
int country_index_next(PSI_table_handle *handle) {
  Country_Table_Handle *h = (Country_Table_Handle *)handle;
  Country_index *i = nullptr;

  switch (h->index_num) {
    case 0:
      i = (Country_index_by_name *)&h->m_index;
      break;
    default:
      assert(0);
      break;
  }

  for (h->m_pos.set_at(&h->m_next_pos); h->m_pos.has_more(); h->m_pos.next()) {
    Country_record *record = &country_records_array[h->m_pos.get_index()];

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
void country_reset_position(PSI_table_handle *handle) {
  Country_Table_Handle *h = (Country_Table_Handle *)handle;
  h->m_pos.reset();
  h->m_next_pos.reset();
  return;
}

/* Read current row from the current_row and display them in the table */
int country_read_column_value(PSI_table_handle *handle, PSI_field *field,
                              unsigned int index) {
  Country_Table_Handle *h = (Country_Table_Handle *)handle;

  switch (index) {
    case 0: /* COUNTRY_NAME */
      pc_string_srv->set_char_utf8mb4(field, h->current_row.name,
                                      h->current_row.name_length);
      break;
    case 1: /* CONTINENT_NAME */
      pc_string_srv->set_char_utf8mb4(field, h->current_row.continent_name,
                                      h->current_row.continent_name_length);
      break;
    case 2: /* YEAR */
      pc_year_srv->set(field, h->current_row.year);
      break;
    case 3: /* POPULATION */
      pc_bigint_srv->set(field, h->current_row.population);
      break;
    case 4: /* GROWTH_FACTOR */
      pc_double_srv->set(field, h->current_row.growth_factor);
      break;
    case 5: /* COUNTRY_CODE */
      pc_text_srv->set(field, h->current_row.country_code,
                       h->current_row.country_code_length);
      break;
    default: /* We should never reach here */
      assert(0);
      break;
  }

  return 0;
}

/* Store row data into records array */
int country_write_row_values(PSI_table_handle *handle) {
  Country_Table_Handle *h = (Country_Table_Handle *)handle;

  native_mutex_lock(&LOCK_country_records_array);

  /* If there is no more space for inserting a record, return */
  if (country_rows_in_table >= COUNTRY_MAX_ROWS) {
    native_mutex_unlock(&LOCK_country_records_array);
    return PFS_HA_ERR_RECORD_FILE_FULL;
  }

  h->current_row.m_exist = true;

  if (is_duplicate(&h->current_row, -1)) {
    native_mutex_unlock(&LOCK_country_records_array);
    return PFS_HA_ERR_FOUND_DUPP_KEY;
  }

  copy_record(&country_records_array[country_next_available_index],
              &h->current_row);
  country_rows_in_table++;

  /* set next available index */
  if (country_rows_in_table < COUNTRY_MAX_ROWS) {
    int i = (country_next_available_index + 1) % COUNTRY_MAX_ROWS;
    int itr_count = 0;
    while (itr_count < COUNTRY_MAX_ROWS) {
      if (country_records_array[i].m_exist == false) {
        country_next_available_index = i;
        break;
      }
      i = (i + 1) % COUNTRY_MAX_ROWS;
      itr_count++;
    }
  }

  native_mutex_unlock(&LOCK_country_records_array);

  return 0;
}

/* Read field data from Field and store that into buffer */
int country_write_column_value(PSI_table_handle *handle, PSI_field *field,
                               unsigned int index) {
  Country_Table_Handle *h = (Country_Table_Handle *)handle;

  char *name = (char *)h->current_row.name;
  unsigned int *name_length = &h->current_row.name_length;
  char *continent_name = (char *)h->current_row.continent_name;
  unsigned int *continent_name_length = &h->current_row.continent_name_length;
  char *country_code = (char *)h->current_row.country_code;
  unsigned int *country_code_length = &h->current_row.country_code_length;

  switch (index) {
    case 0: /* COUNTRY_NAME */
      pc_string_srv->get_char_utf8mb4(field, name, name_length);
      break;
    case 1: /* CONTINENT_NAME */
      pc_string_srv->get_char_utf8mb4(field, continent_name,
                                      continent_name_length);
      break;
    case 2: /* YEAR */
      pc_year_srv->get(field, &h->current_row.year);
      break;
    case 3: /* POPULATION */
      pc_bigint_srv->get(field, &h->current_row.population);
      break;
    case 4: /* GROWTH_FACTOR */
      pc_double_srv->get(field, &h->current_row.growth_factor);
      break;
    case 5: /* COUNTRY_CODE */
      pc_text_srv->get(field, country_code, country_code_length);
      break;
    default: /* We should never reach here */
      assert(0);
      break;
  }

  return 0;
}

/* Update row data in stats array */
int country_update_row_values(PSI_table_handle *handle) {
  int result = 0;
  Country_Table_Handle *h = (Country_Table_Handle *)handle;

  Country_record *cur = &country_records_array[h->m_pos.get_index()];

  assert(cur->m_exist == true);

  native_mutex_lock(&LOCK_country_records_array);
  if (is_duplicate(&h->current_row, h->m_pos.get_index()))
    result = PFS_HA_ERR_FOUND_DUPP_KEY;
  else
    copy_record(cur, &h->current_row);
  native_mutex_unlock(&LOCK_country_records_array);

  return result;
}

int country_update_column_value(PSI_table_handle *handle, PSI_field *field,
                                unsigned int index) {
  Country_Table_Handle *h = (Country_Table_Handle *)handle;

  char *name = (char *)h->current_row.name;
  unsigned int *name_length = &h->current_row.name_length;
  char *continent_name = (char *)h->current_row.continent_name;
  unsigned int *continent_name_length = &h->current_row.continent_name_length;
  char *country_code = (char *)h->current_row.country_code;
  unsigned int *country_code_length = &h->current_row.country_code_length;

  switch (index) {
    case 0: /* COUNTRY_NAME */
      pc_string_srv->get_char_utf8mb4(field, name, name_length);
      break;
    case 1: /* CONTINENT_NAME */
      pc_string_srv->get_char_utf8mb4(field, continent_name,
                                      continent_name_length);
      break;
    case 2: /* YEAR */
      pc_year_srv->get(field, &h->current_row.year);
      break;
    case 3: /* POPULATION */
      pc_bigint_srv->get(field, &h->current_row.population);
      break;
    case 4: /* GROWTH_FACTOR */
      pc_double_srv->get(field, &h->current_row.growth_factor);
      break;
    case 5: /* COUNTRY_CODE */
      pc_text_srv->get(field, country_code, country_code_length);
      break;
    default: /* We should never reach here */
      assert(0);
      break;
  }
  return 0;
}

/* Delete row data from records array */
int country_delete_row_values(PSI_table_handle *handle) {
  Country_Table_Handle *h = (Country_Table_Handle *)handle;

  Country_record *cur = &country_records_array[h->m_pos.get_index()];

  assert(cur->m_exist == true);

  native_mutex_lock(&LOCK_country_records_array);
  cur->m_exist = false;
  country_rows_in_table--;
  native_mutex_unlock(&LOCK_country_records_array);

  return 0;
}

int country_delete_all_rows(void) {
  native_mutex_lock(&LOCK_country_records_array);
  for (int i = 0; i < COUNTRY_MAX_ROWS; i++)
    country_records_array[i].m_exist = false;
  country_rows_in_table = 0;
  country_next_available_index = 0;
  native_mutex_unlock(&LOCK_country_records_array);
  return 0;
}

unsigned long long country_get_row_count(void) { return country_rows_in_table; }

void init_country_share(PFS_engine_table_share_proxy *share) {
  /* Instantiate and initialize PFS_engine_table_share_proxy */
  share->m_table_name = "pfs_example_country";
  share->m_table_name_length = 21;
  share->m_table_definition =
      "NAME char(20) not null, CONTINENT char(20),"
      " YEAR year, POPULATION bigint, GROWTH_FACTOR double(10,2),"
      " COUNTRY_CODE text,"
      " UNIQUE KEY(NAME, CONTINENT)";
  share->m_ref_length = sizeof(Country_POS);
  share->m_acl = EDITABLE;
  share->get_row_count = country_get_row_count;
  share->delete_all_rows = country_delete_all_rows;

  /* Initialize PFS_engine_table_proxy */
  share->m_proxy_engine_table = {country_rnd_next,
                                 country_rnd_init,
                                 country_rnd_pos,
                                 country_index_init,
                                 country_index_read,
                                 country_index_next,
                                 country_read_column_value,
                                 country_reset_position,
                                 country_write_column_value,
                                 country_write_row_values,
                                 country_update_column_value,
                                 country_update_row_values,
                                 country_delete_row_values,
                                 country_open_table,
                                 country_close_table};
}
