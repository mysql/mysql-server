/*
   Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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

#include <my_global.h> // my_alloc.h
#include <my_alloc.h> // MEM_ROOT

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

  void print(const char* where, FILE* file) const;
};

#endif
