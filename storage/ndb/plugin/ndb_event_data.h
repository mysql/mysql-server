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

#ifndef NDB_EVENT_DATA_H
#define NDB_EVENT_DATA_H

#include <array>
#include <memory>

#include "my_alloc.h"  // MEM_ROOT
#include "storage/ndb/plugin/ndb_bitmap.h"

namespace dd {
class Table;
}

struct NDB_SHARE;
struct TABLE;
class THD;
union NdbValue;

/*
  Keeps the state related to receiving events from NDB and injecting them
  into the injector. The Ndb_event_data is created when a NDB table is setup for
  binlogging or schema distribution.

  Each event subscription consist of one NdbEventOperation which has a
  Ndb_event_data attached in the "custom data" member.

  The Ndb_event_data also has a pointer back to the NDB_SHARE which it's created
  for, that pointer is used while processing events (to extract some small
  setup details), when reconfiguring the event subscription during DDL and also
  when tearing down event subscription to unregister from the share.

  NDB_SHARE {
    ...
    m_op -> NdbEventOperation {
      ..
      <custom data> -> Ndb_event_data {
        ..
        share -> points "back" to the same share that owns the m_op
         ^^^ this pointer is what holds "event_data" reference
      }
    }
  }
*/
class Ndb_event_data {
 public:
  Ndb_event_data(const Ndb_event_data &) = delete;
  Ndb_event_data(NDB_SHARE *the_share, size_t num_columns,
                 size_t ndbtab_num_attribs, bool ndbtab_have_blobs);
  ~Ndb_event_data();

 private:
  void init_stored_columns();
  void init_pk_bitmap();
  TABLE *open_shadow_table(THD *thd, const char *db, const char *table_name,
                           const char *key, const dd::Table *table_def);

 public:
  MEM_ROOT mem_root;
  TABLE *shadow_table;

  // Pointer "back" to the NDB_SHARE this event data is created for
  // NOTE! This pointer is what holds the "event_data" reference
  NDB_SHARE *const share;

  // Arrays keeping track of both before and after values for each attribute in
  // the NDB table for whom event will be received.
  std::array<std::unique_ptr<NdbValue[]>, 2> const ndb_value;

  // The NDB table have blobs, used for determining if handle_data_get_blobs()
  // need to be called while handling event
  const bool have_blobs;

  // Bitmap with all stored columns, used as the initial value when determining
  // which attributes are received in an event
  MY_BITMAP stored_columns;

 private:
  // Bitmap with all primary key columns, used for "minimal bitmap"
  MY_BITMAP pk_bitmap;

 public:
  void generate_minimal_bitmap(MY_BITMAP *before, MY_BITMAP *after) const;

  // Factory function to create Ndb_event_data, open the shadow_table, setup
  // ndb_value arrays and initialize bitmaps.
  static const Ndb_event_data *create_event_data(
      THD *thd, const char *db, const char *table_name, const char *key,
      NDB_SHARE *share, const dd::Table *table_def, size_t ndbtab_num_attribs,
      bool ndbtab_have_blobs);
  static void destroy(const Ndb_event_data *);

  // Read uint32 value directly from NdbRecAttr in received event
  uint32 unpack_uint32(unsigned attr_id) const;
  // Read string value directly from NdbRecAttr in received event
  const char *unpack_string(unsigned attr_id) const;

  // Paranoid check of opaque Ndb_event_data pointer
  static bool check_custom_data(void *check_event_data_ptr,
                                const NDB_SHARE *check_share);

  // Convert the opaque pointer stored as 'custom data' in the event operation
  // to Ndb_event_data*, perform the paranoid checks in debug
  static inline Ndb_event_data *get_event_data(void *custom_data_ptr,
                                               const NDB_SHARE *check_share
                                               [[maybe_unused]] = nullptr) {
    assert(check_custom_data(custom_data_ptr, check_share));
    return static_cast<Ndb_event_data *>(custom_data_ptr);
  }
};

#endif
