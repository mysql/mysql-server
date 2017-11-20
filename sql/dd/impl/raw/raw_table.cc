/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#include "sql/dd/impl/raw/raw_table.h"

#include <stddef.h>
#include <algorithm>
#include <new>

#include "m_string.h"
#include "my_base.h"
#include "my_bitmap.h"
#include "my_dbug.h"
#include "my_inttypes.h"
#include "mysql/udf_registration_types.h"
#include "sql/dd/impl/object_key.h"          // dd::Object_key
#include "sql/dd/impl/raw/raw_key.h"         // dd::Raw_key
#include "sql/dd/impl/raw/raw_record.h"      // dd::Raw_record
#include "sql/dd/impl/raw/raw_record_set.h"  // dd::Raw_record_set
#include "sql/handler.h"

namespace dd {

///////////////////////////////////////////////////////////////////////////

Raw_table::Raw_table(thr_lock_type lock_type,
                     const String_type &name)
{
  m_table_list.init_one_table(STRING_WITH_LEN("mysql"),
                              name.c_str(),
                              name.length(),
                              name.c_str(),
                              lock_type);
  m_table_list.is_dd_ctx_table= true;
}

///////////////////////////////////////////////////////////////////////////

/**
  @brief
  Find record and populate raw_record.

  @param key - Pointer to Raw_record after moving to next row.
  @param r - row on which data is updated.

  @return false - On success. 1) We found a row.
                              2) OR Either we don't have any matching rows
  @return true - On failure and error is reported.
*/
bool Raw_table::find_record(const Object_key &key,
                            std::unique_ptr<Raw_record> &r)
{
  DBUG_ENTER("Raw_table::find_record");

  TABLE *table= get_table();
  std::unique_ptr<Raw_key> k(key.create_access_key(this));

  int rc;
  if (!table->file->inited &&
      (rc= table->file->ha_index_init(k->index_no, true)))
  {
    table->file->print_error(rc, MYF(0));
    DBUG_RETURN(true);
  }

  rc= table->file->ha_index_read_idx_map(
    table->record[0],
    k->index_no,
    k->key,
    k->keypart_map,
    (k->keypart_map == HA_WHOLE_KEY) ?  HA_READ_KEY_EXACT : HA_READ_PREFIX);

  if (table->file->inited)
    table->file->ha_index_end();  // Close the scan over the index

  // Row not found.
  if (rc == HA_ERR_KEY_NOT_FOUND || rc == HA_ERR_END_OF_FILE)
  {
    r.reset(NULL);
    DBUG_RETURN(false);
  }

  // Got unexpected error.
  if (rc)
  {
    table->file->print_error(rc, MYF(0));
    r.reset(NULL);
    DBUG_RETURN(true);
  }

  r.reset(new Raw_record(table));
  DBUG_RETURN(false);
}

///////////////////////////////////////////////////////////////////////////

/**
  @brief
  Write modified data into row buffer.

  @param key - Pointer to Raw_record after moving to next row.
  @param r - row on which data is updated.

  @return false - On success.
  @return true - On failure and error is reported.
*/
bool Raw_table::prepare_record_for_update(const Object_key &key,
                                          std::unique_ptr<Raw_record> &r)
{
  DBUG_ENTER("Raw_table::prepare_record_for_update");
 
  TABLE *table= get_table();

  // Setup row buffer for update
  table->use_all_columns();
  bitmap_set_all(table->write_set);
  bitmap_set_all(table->read_set);

  if (find_record(key, r))
    DBUG_RETURN(true);

  store_record(table, record[1]);

  DBUG_RETURN(false);
}

///////////////////////////////////////////////////////////////////////////

Raw_new_record *Raw_table::prepare_record_for_insert()
{
  return new (std::nothrow) Raw_new_record(get_table());
}

///////////////////////////////////////////////////////////////////////////

/**
  @brief
   Initiate table scan operation for the given key.

  @return false - on success.
  @return true - on failure and error is reported.
*/
bool Raw_table::open_record_set(const Object_key *key,
                                std::unique_ptr<Raw_record_set> &rs)
{
  DBUG_ENTER("Raw_table::open_record_set");

  Raw_key *access_key= NULL;

  // Create specific access key if submitted.
  if (key)
  {
    restore_record(get_table(), s->default_values);
    access_key= key->create_access_key(this);
  }

  std::unique_ptr<Raw_record_set> rs1(
    new (std::nothrow) Raw_record_set(get_table(), access_key));

  if (rs1->open())
    DBUG_RETURN(true);

  rs= std::move(rs1);

  DBUG_RETURN(false);
}

///////////////////////////////////////////////////////////////////////////

/**
  @brief
  Find last record in table and populate raw_record.

  @param key - Pointer to Raw_record after moving to next row.
  @param r - row on which data is updated.

  @return false - On success. 1) We found a row.
                              2) OR Either we don't have any matching rows
  @return true - On failure and error is reported.
*/
/* purecov: begin deadcode */
bool Raw_table::find_last_record(const Object_key &key,
                                 std::unique_ptr<Raw_record> &r)
{
  DBUG_ENTER("Raw_table::find_last_record");

  TABLE *table= get_table();
  std::unique_ptr<Raw_key> k(key.create_access_key(this));

  int rc;
  if (!table->file->inited &&
      (rc=table->file->ha_index_init(k->index_no, true)))
  {
    table->file->print_error(rc, MYF(0));
    DBUG_RETURN(true);
  }

  rc= table->file->ha_index_read_idx_map(table->record[0],
                                             k->index_no,
                                             k->key,
                                             k->keypart_map,
                                             HA_READ_PREFIX_LAST_OR_PREV);

  if (table->file->inited)
    table->file->ha_index_end();  // Close the scan over the index

  // Row not found.
  if (rc == HA_ERR_KEY_NOT_FOUND || rc == HA_ERR_END_OF_FILE)
  {
    r.reset(NULL);
    DBUG_RETURN(false);
  }

  // Got unexpected error.
  if (rc)
  {
    table->file->print_error(rc, MYF(0));
    r.reset(NULL);
    DBUG_RETURN(true);
  }

  r.reset(new Raw_record(table));

  DBUG_RETURN(false);
}
/* purecov: end */

///////////////////////////////////////////////////////////////////////////

}
