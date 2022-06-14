/*
   Copyright (c) 2011, 2022, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "storage/ndb/plugin/ndb_event_data.h"

#include "sql/dd_table_share.h"
#include "sql/field.h"
#include "sql/sql_base.h"
#include "sql/sql_class.h"
#include "sql/strfunc.h"
#include "sql/table.h"
#include "storage/ndb/plugin/ndb_dd_table.h"
#include "storage/ndb/plugin/ndb_ndbapi_util.h"
#include "storage/ndb/plugin/ndb_table_map.h"

Ndb_event_data::Ndb_event_data(NDB_SHARE *the_share, size_t num_columns,
                               size_t ndbtab_num_attribs,
                               bool ndbtab_have_blobs)
    : shadow_table(nullptr),
      share(the_share),
      ndb_value{std::make_unique<NdbValue[]>(ndbtab_num_attribs),
                std::make_unique<NdbValue[]>(ndbtab_num_attribs)},
      have_blobs(ndbtab_have_blobs) {
  // Initialize bitmaps, using dynamically allocated bitbuf
  bitmap_init(&stored_columns, nullptr, num_columns);
  bitmap_init(&pk_bitmap, nullptr, num_columns);

  // Initialize mem_root where the shadow_table will be allocated
  init_sql_alloc(PSI_INSTRUMENT_ME, &mem_root, 1024);
}

Ndb_event_data::~Ndb_event_data() {
  if (shadow_table) {
    closefrm(shadow_table, true);
  }

  bitmap_free(&stored_columns);
  bitmap_free(&pk_bitmap);
}

/*
 * While writing an UPDATE_ROW event to the binlog, a bitmap is
 * used to indicate which columns should be written. An
 * UPDATE_ROW event contains 2 versions of the row: a Before Image
 * of the row before the update was done, and an After Image of
 * the row after the update. Column bitmaps are used to decide
 * which columns will be written to both images. The Before
 * Image and After Image can contain different columns.
 *
 * For the binlog formats UPDATED_ONLY_USE_UPDATE_MINIMAL and
 * FULL_USE_UPDATE_MINIMAL, it is necessary to write only primary
 * key columns to the Before Image, and to remove all primary key
 * columns from the After Image. A bitmap of primary key columns is
 * created for this purpose.
 */
void Ndb_event_data::init_pk_bitmap() {
  if (shadow_table->s->primary_key == MAX_KEY) {
    // Table without pk, no need for pk_bitmap since minimal is full
    return;
  }

  KEY *key = shadow_table->key_info + shadow_table->s->primary_key;
  KEY_PART_INFO *key_part_info = key->key_part;
  const uint key_parts = key->user_defined_key_parts;
  for (uint i = 0; i < key_parts; i++, key_part_info++) {
    bitmap_set_bit(&pk_bitmap, key_part_info->fieldnr - 1);
  }
  assert(!bitmap_is_clear_all(&pk_bitmap));
}

/*
 * Modify the column bitmaps generated for UPDATE_ROW as per
 * the MINIMAL binlog format type. Expected arguments:
 *
 * @before: empty bitmap to be populated with PK columns
 * @after: bitmap with updated cols, if --ndb-log-updated-only=ON
 *         bitmap with all cols, if --ndb-log-updated-only=OFF
 *
 * If no PK is defined, bitmaps revert to default behaviour:
 *  - before and after bitmaps are identical
 *  - bitmaps contain all/updated cols as per ndb_log_updated_only
 */
void Ndb_event_data::generate_minimal_bitmap(MY_BITMAP *before,
                                             MY_BITMAP *after) const {
  if (shadow_table->s->primary_key == MAX_KEY) {
    // no usable PK bitmap, set Before Image = After Image
    bitmap_copy(before, after);
    return;
  }

  assert(!bitmap_is_clear_all(&pk_bitmap));
  // set Before Image to contain only primary keys
  bitmap_copy(before, &pk_bitmap);
  // remove primary keys from After Image
  bitmap_subtract(after, &pk_bitmap);
}

void Ndb_event_data::init_stored_columns() {
  if (Ndb_table_map::has_virtual_gcol(shadow_table)) {
    for (uint i = 0; i < shadow_table->s->fields; i++) {
      Field *field = shadow_table->field[i];
      if (field->stored_in_db) {
        bitmap_set_bit(&stored_columns, i);
      }
    }
  } else {
    bitmap_set_all(&stored_columns);  // all columns are stored
  }
}

TABLE *Ndb_event_data::open_shadow_table(THD *thd, const char *db,
                                         const char *table_name,
                                         const char *key,
                                         const dd::Table *table_def) {
  DBUG_TRACE;
  assert(table_def);

  // Allocate memory for shadow table from MEM_ROOT
  TABLE_SHARE *shadow_table_share =
      (TABLE_SHARE *)mem_root.Alloc(sizeof(TABLE_SHARE));
  TABLE *shadow_table = (TABLE *)mem_root.Alloc(sizeof(TABLE));

  init_tmp_table_share(thd, shadow_table_share, db, 0, table_name, key,
                       nullptr);

  int error = 0;
  if ((error = open_table_def(thd, shadow_table_share, *table_def)) ||
      (error = open_table_from_share(
           thd, shadow_table_share, "", 0,
           (uint)(SKIP_NEW_HANDLER | DELAYED_OPEN | READ_ALL), 0, shadow_table,
           false, table_def))) {
    DBUG_PRINT("error", ("failed to open shadow table, error: %d", error));
    free_table_share(shadow_table_share);
    return nullptr;
  }

  mysql_mutex_lock(&LOCK_open);
  assign_new_table_id(shadow_table_share);
  mysql_mutex_unlock(&LOCK_open);

  // Allocate strings for db and table_name for shadow_table
  // in event_data's MEM_ROOT(where the shadow_table itself is allocated)
  lex_string_strmake(&mem_root, &shadow_table->s->db, db, strlen(db));
  lex_string_strmake(&mem_root, &shadow_table->s->table_name, table_name,
                     strlen(table_name));

  // The shadow table is not really "in_use" by the thd who opened it, rather
  // only used later on to tell injector which table data changes are for.
  // NOTE! There is small chance that opening of the shadow table have
  // side-effects on the THD or vice versa that shadow table is affected by some
  // setting in THD, in such case this need to be changed so that shadow table
  // are opened by it's own THD object.
  assert(shadow_table->in_use == thd);
  shadow_table->in_use = nullptr;

  // Can't use 'use_all_columns()' as the file object is not setup
  // yet (and never will)
  shadow_table->column_bitmaps_set_no_signal(&shadow_table->s->all_set,
                                             &shadow_table->s->all_set);

  return shadow_table;
}

/**
   @brief Create event data used for receiving event for NDB table.
   This includes opening a shadow table which is used when injecting the
   received event into injector.

   @param thd             Thread handle (for creating the shadow table)
   @param db              Database of table to create event data for
   @param table_name      Name of table to create event data for
   @param key             Key of table to create event data for
   @param share           Pointer to the NDB_SHARE (opaque type in this module)
   @param table_def       Pointer to MySQL table definition for shadow table
   @param ndbtab_num_attribs Number of attributes in the NDB table (for
                          sizing the value arrays storing attributes received
                          in events)
   @param ndbtab_have_blobs Does the NDB table have blobs.

   @return Pointer to the newly created Ndb_event_data or nullptr if create
   fails.
 */
const Ndb_event_data *Ndb_event_data::create_event_data(
    THD *thd, const char *db, const char *table_name, const char *key,
    NDB_SHARE *share, const dd::Table *table_def, size_t ndbtab_num_attribs,
    bool ndbtab_have_blobs) {
  DBUG_TRACE;
  assert(table_def);

  const size_t num_columns = ndb_dd_table_get_num_columns(table_def);

  auto event_data = std::make_unique<Ndb_event_data>(
      share, num_columns, ndbtab_num_attribs, ndbtab_have_blobs);

  // Setup THR_MALLOC to allocate memory for shadow table from the MEM_ROOT in
  // the newly created Ndb_event_data
  MEM_ROOT **root_ptr = THR_MALLOC;
  MEM_ROOT *old_root = *root_ptr;
  *root_ptr = &event_data->mem_root;

  // Create the shadow table
  TABLE *shadow_table =
      event_data->open_shadow_table(thd, db, table_name, key, table_def);
  if (!shadow_table) {
    DBUG_PRINT("error", ("failed to open shadow table"));
    *root_ptr = old_root;
    return nullptr;
  }

  // Restore original MEM_ROOT
  *root_ptr = old_root;

  // Check that number of columns from table_def match the
  // number in shadow_table
  assert(num_columns == shadow_table->s->fields);

  event_data->shadow_table = shadow_table;

  // Calculate bitmaps after assigning the shadow_table
  event_data->init_pk_bitmap();
  event_data->init_stored_columns();

  return event_data.release();
}

void Ndb_event_data::destroy(const Ndb_event_data *event_data) {
  DBUG_TRACE;

  delete event_data;
}

uint32 Ndb_event_data::unpack_uint32(unsigned attr_id) const {
  return ndb_value[0][attr_id].rec->u_32_value();
}

const char *Ndb_event_data::unpack_string(unsigned attr_id) const {
  return ndb_value[0][attr_id].rec->aRef();
}

bool Ndb_event_data::check_custom_data(void *check_event_data_ptr,
                                       const NDB_SHARE *check_share) {
  Ndb_event_data *event_data =
      static_cast<Ndb_event_data *>(check_event_data_ptr);

  // No event_data pointer is not allowed
  if (!event_data) {
    return false;
  }

  if (event_data->shadow_table == nullptr ||
      event_data->ndb_value[0] == nullptr ||
      event_data->ndb_value[1] == nullptr) {
    return false;
  }

  // The share pointer should match, unless checking against nullptr
  if (check_share && event_data->share != check_share) {
    return false;
  }

  return true;
}
