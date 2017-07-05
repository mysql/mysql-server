/*
   Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.

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

#include <ndbapi/NdbApi.hpp>
#include "ndb_table_map.h"

#include "table.h"
#include "field.h"

Ndb_table_map::Ndb_table_map(struct TABLE * mysqlTable,
                             const NdbDictionary::Table * ndbTable) :
  m_ndb_table(ndbTable),
  m_array_size(mysqlTable->s->fields),
  m_stored_fields(mysqlTable->s->stored_fields),
  m_hidden_pk((mysqlTable->s->primary_key == MAX_KEY) ? 1 : 0),
  m_trivial(m_array_size == m_stored_fields)
{
  if(! m_trivial)
  {
    /* Allocate arrays */
    m_map_by_field = new int[m_array_size];
    m_map_by_col = new int[m_array_size];

    /* Initialize the two bitmaps */
    bitmap_init(& m_moved_fields, 0, m_array_size, 0);
    bitmap_init(& m_rewrite_set, 0, m_array_size, 0);

    /* Initialize both arrays full of -1 */
    for(uint i = 0 ; i < m_array_size ; i++)
    {
      m_map_by_field[i] = m_map_by_col[i] = -1;
    }

    /* Build mappings, and set bits in m_moved_fields */
    for(uint fieldId= 0, colId= 0; fieldId < m_array_size ; fieldId ++)
    {
      if(colId != fieldId)
      {
        bitmap_set_bit(& m_moved_fields, fieldId);
      }

      if(mysqlTable->field[fieldId]->stored_in_db)
      {
        m_map_by_field[fieldId] = colId;
        m_map_by_col[colId] = fieldId;
        colId++;
      }
    } // for(uint fieldId ...
  } // if(! m_trivial ...
}


uint Ndb_table_map::get_column_for_field(uint fieldId) const
{
  assert(fieldId < m_array_size);
  if(m_trivial) return fieldId;

  const int colId = m_map_by_field[fieldId];
  assert(colId >= 0);  // The user must not ask for virtual fields
  return (uint) colId;
}


uint Ndb_table_map::get_field_for_column(uint colId) const
{
  assert(colId < m_stored_fields);  // The user must not ask for hidden columns
  if(m_trivial) return colId;

  const int fieldId = m_map_by_col[colId];
  DBUG_ASSERT(fieldId >= 0);  // We do not expect any non-final hidden columns
  return (uint) fieldId;
}


unsigned char * Ndb_table_map::get_column_mask(const st_bitmap *field_mask)
{
  unsigned char * map = 0;
  if(field_mask)
  {
    map = (unsigned char *)(field_mask->bitmap);
    if((! m_trivial) && bitmap_is_overlapping(& m_moved_fields, field_mask))
    {
      map = (unsigned char *)(m_rewrite_set.bitmap);
      bitmap_clear_all(& m_rewrite_set);
      for(uint i = 0 ; i < m_array_size ; i++)
      {
        int & colId = m_map_by_field[i];
        if(bitmap_is_set(field_mask, i) && colId >= 0)
        {
          bitmap_set_bit(& m_rewrite_set, colId);
        }
      }
    }
  }
  return map;
}


Ndb_table_map::~Ndb_table_map()
{
  if(! m_trivial)
  {
    delete[] m_map_by_field;
    delete[] m_map_by_col;
    bitmap_free(& m_moved_fields);
    bitmap_free(& m_rewrite_set);
  }
}

