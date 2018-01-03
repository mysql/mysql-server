/*
   Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "sql/ndb_event_data.h"

#include "sql/dd_table_share.h"
#include "sql/ndb_dd_table.h"
#include "sql/ndb_table_map.h"
#include "sql/sql_base.h"
#include "sql/sql_class.h"
#include "sql/table.h"


Ndb_event_data::Ndb_event_data(NDB_SHARE *the_share, size_t num_columns) :
  shadow_table(nullptr),
  share(the_share)
{
  ndb_value[0] = nullptr;
  ndb_value[1] = nullptr;

  // Initialize bitmaps, using dynamically allocated bitbuf
  bitmap_init(&stored_columns, nullptr, num_columns, false);
  bitmap_init(&pk_bitmap, nullptr, num_columns, false);

  // Initialize mem_root where the shadow_table will be allocated
  init_sql_alloc(PSI_INSTRUMENT_ME, &mem_root, 1024, 0);
}


Ndb_event_data::~Ndb_event_data()
{
  if (shadow_table)
    closefrm(shadow_table, 1);
  shadow_table = nullptr;

  bitmap_free(&stored_columns);
  bitmap_free(&pk_bitmap);

  free_root(&mem_root, MYF(0));
  share = nullptr;
  /*
    ndbvalue[] allocated with my_multi_malloc -> only
    first pointer need to be freed
  */
  my_free(ndb_value[0]);
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
void Ndb_event_data::init_pk_bitmap()
{
  if (shadow_table->s->primary_key == MAX_KEY)
  {
    // Table without pk, no need for pk_bitmap since minimal is full
    return;
  }

  KEY* key = shadow_table->key_info + shadow_table->s->primary_key;
  KEY_PART_INFO* key_part_info = key->key_part;
  const uint key_parts = key->user_defined_key_parts;
  for (uint i = 0; i < key_parts; i++, key_part_info++)
  {
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
                                             MY_BITMAP *after) const
{
  if (shadow_table->s->primary_key == MAX_KEY)
  {
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



void Ndb_event_data::init_stored_columns()
{
  if (Ndb_table_map::has_virtual_gcol(shadow_table))
  {
    for(uint i = 0 ; i < shadow_table->s->fields; i++)
    {
      Field * field = shadow_table->field[i];
      if (field->stored_in_db)
        bitmap_set_bit(&stored_columns, i);
    }
  }
  else
  {
    bitmap_set_all(&stored_columns);  // all columns are stored
  }
}

TABLE* Ndb_event_data::open_shadow_table(THD* thd, const char* db,
                                         const char* table_name,
                                         const char* key,
                                         const dd::Table* table_def,
                                         THD* owner_thd) {
  DBUG_ENTER("Ndb_event_data::open_shadow_table");
  DBUG_ASSERT(table_def);

  TABLE_SHARE* shadow_table_share =
      (TABLE_SHARE*)alloc_root(&mem_root, sizeof(TABLE_SHARE));
  TABLE* shadow_table = (TABLE*)alloc_root(&mem_root, sizeof(TABLE));

  init_tmp_table_share(thd, shadow_table_share, db, 0, table_name, key,
                       nullptr);

  int error = 0;
  if ((error = open_table_def(thd, shadow_table_share, *table_def)) ||
      (error = open_table_from_share(
           thd, shadow_table_share, "", 0,
           (uint)(OPEN_FRM_FILE_ONLY | DELAYED_OPEN | READ_ALL), 0,
           shadow_table, false, table_def))) {
    DBUG_PRINT("error", ("failed to open shadow table, error: %d", error));
    free_table_share(shadow_table_share);
    DBUG_RETURN(nullptr);
  }

  mysql_mutex_lock(&LOCK_open);
  assign_new_table_id(shadow_table_share);
  mysql_mutex_unlock(&LOCK_open);

  // Allocate strings for db and table_name for shadow_table
  // in event_data's MEM_ROOT(where the shadow_table itself is allocated)
  lex_string_copy(&mem_root, &shadow_table->s->db, db);
  lex_string_copy(&mem_root, &shadow_table->s->table_name, table_name);

  shadow_table->in_use = owner_thd;

  // Can't use 'use_all_columns()' as the file object is not setup
  // yet (and never will)
  shadow_table->column_bitmaps_set_no_signal(&shadow_table->s->all_set,
                                             &shadow_table->s->all_set);

  DBUG_RETURN(shadow_table);
}

/*
  Create event data for the table given in share. This includes
  opening a shadow table. The shadow table is used when
  receiving and event from the data nodes which need to be written
  to the binlog injector.
*/

Ndb_event_data* Ndb_event_data::create_event_data(
    THD* thd, NDB_SHARE* share, const char* db, const char* table_name,
    const char* key, THD* owner_thd, const dd::Table* table_def) {
  DBUG_ENTER("Ndb_event_data::create_event_data");
  DBUG_ASSERT(table_def);

  const size_t num_columns = ndb_dd_table_get_num_columns(table_def);

  Ndb_event_data* event_data = new Ndb_event_data(share, num_columns);

  // Setup THR_MALLOC to allocate memory from the MEM_ROOT in the
  // newly created Ndb_event_data
  MEM_ROOT** root_ptr = THR_MALLOC;
  MEM_ROOT* old_root = *root_ptr;
  *root_ptr = &event_data->mem_root;

  // Create the shadow table
  TABLE* shadow_table = event_data->open_shadow_table(thd, db, table_name, key,
                                                      table_def, owner_thd);
  if (!shadow_table) {
    DBUG_PRINT("error", ("failed to open shadow table"));
    delete event_data;
    *root_ptr = old_root;
    DBUG_RETURN(nullptr);
  }

  // Check that number of columns from table_def match the
  // number in shadow_table
  DBUG_ASSERT(num_columns == shadow_table->s->fields);

  event_data->shadow_table = shadow_table;

  // Calculate bitmaps after assigning the shadow_table
  event_data->init_pk_bitmap();
  event_data->init_stored_columns();

  // Calculate if the assigned shadow_table have blobs and save that
  // information for later when events are received
  event_data->have_blobs = Ndb_table_map::have_physical_blobs(shadow_table);

  // Restore old root
  *root_ptr = old_root;

  DBUG_RETURN(event_data);
}

void Ndb_event_data::destroy(const Ndb_event_data* event_data)
{
  DBUG_ENTER("delete_event_data");

  delete event_data;

  DBUG_VOID_RETURN;
}
