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

#include <my_global.h> // my_alloc.h
#include <my_alloc.h> // MEM_ROOT
#include <my_bitmap.h>

#include <ndbapi/ndbapi_limits.h>

class Ndb_event_data
{
public:
  Ndb_event_data(); // Not implemented
  Ndb_event_data(const Ndb_event_data&); // Not implemented
  Ndb_event_data(struct NDB_SHARE *the_share);

  ~Ndb_event_data();

  MEM_ROOT mem_root;
  struct TABLE *shadow_table;
  struct NDB_SHARE *share;
  union NdbValue *ndb_value[2];
  /* Bitmap with bit set for all primary key columns. */
  MY_BITMAP *pk_bitmap;
  my_bitmap_map pk_bitbuf[(NDB_MAX_ATTRIBUTES_IN_TABLE +
                            8*sizeof(my_bitmap_map) - 1) /
                           (8*sizeof(my_bitmap_map))];

  void print(const char* where, FILE* file) const;
  void init_pk_bitmap();
  void generate_minimal_bitmap(MY_BITMAP *before, MY_BITMAP *after);
};

#endif
