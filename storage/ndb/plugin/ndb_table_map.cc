/*
   Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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

#include "storage/ndb/plugin/ndb_table_map.h"

#include "my_dbug.h"
#include "sql/field.h"
#include "sql/table.h"
#include "storage/ndb/include/ndbapi/NdbApi.hpp"

Ndb_table_map::Ndb_table_map(const TABLE *mysqlTable,
                             const NdbDictionary::Table *ndbTable)
    : m_ndb_table(ndbTable),
      m_array_size(mysqlTable->s->fields),
      m_stored_fields(num_stored_fields(mysqlTable)),
      m_hidden_pk((mysqlTable->s->primary_key == MAX_KEY) ? 1 : 0),
      m_trivial(m_array_size == m_stored_fields) {
  if (!m_trivial) {
    /* Allocate arrays */
    m_map_by_field = new int[m_array_size];
    m_map_by_col = new int[m_array_size];

    /* Initialize the two bitmaps */
    bitmap_init(&m_moved_fields, nullptr, m_array_size);
    bitmap_init(&m_rewrite_set, nullptr, m_array_size);

    /* Initialize both arrays full of -1 */
    for (uint i = 0; i < m_array_size; i++) {
      m_map_by_field[i] = m_map_by_col[i] = -1;
    }

    /* Build mappings, and set bits in m_moved_fields */
    for (uint fieldId = 0, colId = 0; fieldId < m_array_size; fieldId++) {
      if (colId != fieldId) {
        bitmap_set_bit(&m_moved_fields, fieldId);
      }

      if (mysqlTable->field[fieldId]->stored_in_db) {
        m_map_by_field[fieldId] = colId;
        m_map_by_col[colId] = fieldId;
        colId++;
      }
    }  // for(uint fieldId ...
  }    // if(! m_trivial ...
}

uint Ndb_table_map::get_column_for_field(uint fieldId) const {
  assert(fieldId < m_array_size);
  if (m_trivial) return fieldId;

  const int colId = m_map_by_field[fieldId];
  assert(colId >= 0);  // The user must not ask for virtual fields
  return (uint)colId;
}

uint Ndb_table_map::get_field_for_column(uint colId) const {
  assert(colId < m_stored_fields);  // The user must not ask for hidden columns
  if (m_trivial) return colId;

  const int fieldId = m_map_by_col[colId];
  assert(fieldId >= 0);  // We do not expect any non-final hidden columns
  return (uint)fieldId;
}

unsigned char *Ndb_table_map::get_column_mask(const MY_BITMAP *field_mask) {
  unsigned char *map = nullptr;
  if (field_mask) {
    map = (unsigned char *)(field_mask->bitmap);
    if ((!m_trivial) && bitmap_is_overlapping(&m_moved_fields, field_mask)) {
      map = (unsigned char *)(m_rewrite_set.bitmap);
      bitmap_clear_all(&m_rewrite_set);
      for (uint i = 0; i < m_array_size; i++) {
        int &colId = m_map_by_field[i];
        if (bitmap_is_set(field_mask, i) && colId >= 0) {
          bitmap_set_bit(&m_rewrite_set, colId);
        }
      }
    }
  }
  return map;
}

Ndb_table_map::~Ndb_table_map() {
  if (!m_trivial) {
    delete[] m_map_by_field;
    delete[] m_map_by_col;
    bitmap_free(&m_moved_fields);
    bitmap_free(&m_rewrite_set);
  }
}

bool Ndb_table_map::has_virtual_gcol(const TABLE *table) {
  if (table->vfield == nullptr) return false;
  for (Field **gc = table->vfield; *gc; gc++) {
    if (!(*gc)->stored_in_db) return true;
  }
  return false;
}

uint Ndb_table_map::num_stored_fields(const TABLE *table) {
  if (table->vfield == nullptr) {
    // Table has no virtual fields, just return number of fields
    return table->s->fields;
  }

  // Table has virtual fields, loop through and subtract those
  // which are not stored
  uint num_stored_fields = table->s->fields;
  for (Field **vfield_ptr = table->vfield; *vfield_ptr; vfield_ptr++) {
    if (!(*vfield_ptr)->stored_in_db) num_stored_fields--;
  }
  return num_stored_fields;
}

#ifndef NDEBUG
void Ndb_table_map::print_record(const TABLE *table, const uchar *record) {
  for (uint j = 0; j < table->s->fields; j++) {
    char buf[40];
    int pos = 0;
    Field *field = table->field[j];
    const uchar *field_ptr = field->field_ptr() - table->record[0] + record;
    int pack_len = field->pack_length();
    int n = pack_len < 10 ? pack_len : 10;

    for (int i = 0; i < n && pos < 20; i++) {
      pos += sprintf(&buf[pos], " %x", (int)(uchar)field_ptr[i]);
    }
    buf[pos] = 0;
    DBUG_PRINT("info", ("[%u]field_ptr[0->%d]: %s", j, n, buf));
  }
}

void Ndb_table_map::print_table(const char *info, const TABLE *table) {
  if (table == nullptr) {
    DBUG_PRINT("info", ("%s: (null)", info));
    return;
  }
  DBUG_PRINT("info",
             ("%s: %s.%s s->fields: %d  "
              "reclength: %lu  rec_buff_length: %u  record[0]: %p  "
              "record[1]: %p",
              info, table->s->db.str, table->s->table_name.str,
              table->s->fields, table->s->reclength, table->s->rec_buff_length,
              table->record[0], table->record[1]));

  for (unsigned int i = 0; i < table->s->fields; i++) {
    Field *f = table->field[i];
    DBUG_PRINT(
        "info",
        ("[%d] \"%s\"(0x%lx:%s%s%s%s%s%s) type: %d  pack_length: %d  "
         "ptr: %p[+%d]  null_bit: %u  null_ptr: 0x%lx[+%d]",
         i, f->field_name, (long)f->all_flags(),
         f->is_flag_set(PRI_KEY_FLAG) ? "pri" : "attr",
         f->is_flag_set(NOT_NULL_FLAG) ? "" : ",nullable",
         f->is_flag_set(UNSIGNED_FLAG) ? ",unsigned" : ",signed",
         f->is_flag_set(ZEROFILL_FLAG) ? ",zerofill" : "",
         f->is_flag_set(BLOB_FLAG) ? ",blob" : "",
         f->is_flag_set(BINARY_FLAG) ? ",binary" : "", f->real_type(),
         f->pack_length(), f->field_ptr(), (int)(f->offset(table->record[0])),
         f->null_bit, (long)f->null_offset(nullptr), (int)f->null_offset()));
    if (f->type() == MYSQL_TYPE_BIT) {
      Field_bit *g = (Field_bit *)f;
      DBUG_PRINT("MYSQL_TYPE_BIT",
                 ("field_length: %d  bit_ptr: %p[+%d] "
                  "bit_ofs: %d  bit_len: %u",
                  g->field_length, g->bit_ptr,
                  (int)((uchar *)g->bit_ptr - table->record[0]), g->bit_ofs,
                  g->bit_len));
    }
  }
}
#endif
