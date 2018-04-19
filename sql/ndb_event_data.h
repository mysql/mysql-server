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

#ifndef NDB_EVENT_DATA_H
#define NDB_EVENT_DATA_H

#include "my_alloc.h" // MEM_ROOT
#include "sql/ndb_bitmap.h"

namespace dd {
  class Table;
}

struct NDB_SHARE;
struct TABLE;

/*
  Ndb_event_data holds information related to
  receiving events from NDB. It's created when
  the table is setup for binlogging or schema
  distribution.
*/

class Ndb_event_data
{
  Ndb_event_data() = delete;
  Ndb_event_data(const Ndb_event_data&) = delete;

  Ndb_event_data(NDB_SHARE* the_share, size_t num_columns);
  ~Ndb_event_data();

  void init_stored_columns();
  void init_pk_bitmap();
  TABLE *open_shadow_table(class THD* thd,
                           const char *db, const char *table_name,
                           const char *key, const dd::Table *table_def,
                           class THD *owner_thd);
public:
  MEM_ROOT mem_root;
  TABLE *shadow_table;
  NDB_SHARE *share;
  union NdbValue *ndb_value[2];

  // Bitmap with all stored columns
  MY_BITMAP stored_columns;
  /* Bitmap with all primary key columns. */
  MY_BITMAP pk_bitmap;
  // The NDB table have blobs
  bool have_blobs;

  void generate_minimal_bitmap(MY_BITMAP *before, MY_BITMAP *after) const;

  // Factory function to create Ndb_event_data, open the shadow_table and
  // initialize bitmaps.
  static Ndb_event_data* create_event_data(class THD* thd,
                                           NDB_SHARE *share,
                                           const char *db,
                                           const char *table_name,
                                           const char *key,
                                           class THD* owner_thd,
                                           const dd::Table *table_def);
  static void destroy(const Ndb_event_data*);
};

#endif
