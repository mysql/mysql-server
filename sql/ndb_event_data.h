/*
   Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef NDB_EVENT_DATA_H
#define NDB_EVENT_DATA_H

#include "my_alloc.h" // MEM_ROOT
#include "sql/ndb_bitmap.h"
#include "storage/ndb/include/ndbapi/ndbapi_limits.h"

namespace dd {
  class Table;
}

struct NDB_SHARE;

/*
  Ndb_event_data holds information related to
  receiving events from NDB
*/

class Ndb_event_data
{
  Ndb_event_data(); // Not implemented
  Ndb_event_data(const Ndb_event_data&); // Not implemented
  Ndb_event_data(NDB_SHARE* the_share);
public:
  ~Ndb_event_data();

  MEM_ROOT mem_root;
  struct TABLE *shadow_table;
  NDB_SHARE *share;
  union NdbValue *ndb_value[2];

  MY_BITMAP stored_columns;
  void init_stored_columns();

  // Does the table have blobs
  bool have_blobs;

private:
  /* Bitmap with bit set for all primary key columns. */
  MY_BITMAP *pk_bitmap;
  my_bitmap_map pk_bitbuf[(NDB_MAX_ATTRIBUTES_IN_TABLE +
                            8*sizeof(my_bitmap_map) - 1) /
                           (8*sizeof(my_bitmap_map))];
public:
  void init_pk_bitmap();
  void generate_minimal_bitmap(MY_BITMAP *before, MY_BITMAP *after);

private:
  TABLE *open_shadow_table(class THD* thd,
                           const char *db, const char *table_name,
                           const char *key, const dd::Table *table_def,
                           class THD *owner_thd);
public:
  // Factory function
  static Ndb_event_data* create_event_data(class THD* thd,
                                           NDB_SHARE *share,
                                           const char *db,
                                           const char *table_name,
                                           const char *key,
                                           class THD* owner_thd,
                                           const dd::Table *table_def);
};

#endif
