/* Copyright (c) 2011, 2014, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef FIELD_ITERATOR_INCLUDED
#define	FIELD_ITERATOR_INCLUDED

#include "value.h"
#include "rows_event.h"
#include "row_of_fields.h"
#include <vector>

namespace binary_log {

/**
  @class Row_event_iterator

  The class gives a forward input iterator to the container
  binary_log::Row_event_set.
*/
template <class Iterator_value_type >
class Row_event_iterator
{
public:
  Row_event_iterator() : m_row_event(0), m_table_map(0),
                         m_new_field_offset_calculated(0), m_field_offset(0)
  { }


  /**
    The Row_event_iterator needs a table map as well as a row event for its
    construction.

    @param row_event  The Rows_event object to be decoded
    @param table_map  Table map event associated with the row event. This event
                      is used to extract information about the table structure,
                      into which the INSERT/UPDATE/DELETE is performed.
  */
  Row_event_iterator(const Rows_event *row_event,
                     const Table_map_event *table_map)
    : m_row_event(row_event), m_table_map(table_map),
      m_new_field_offset_calculated(0)
  {
      m_field_offset= 0;
  }

  Iterator_value_type operator*();

  Row_event_iterator& operator++();

  Row_event_iterator operator++(int);

  bool operator==(const Row_event_iterator& x) const;

  bool operator!=(const Row_event_iterator& x) const;

  /**
    This method checks if the bit at a paricular index in the bitmap
    is set or unset. The method is called while decoding a row
    for the row_event.

    @param bitmap Bitmap of columns in the row received in a row event
    @param index  Index representing the column number queried, starting
                  from 0
    @return 1     If the column can be null
            0     If the column cannot be null
  */
  bool is_null(unsigned char *bitmap, int index) const;

  /**
    This method returns the amount of memory required to store the metadata,
    in bytes. Metadata of a field depends on the field type(and not the
    field value).

    @param field_type The input column type
    @return           number of bytes required to store metadata information

  */
  int lookup_metadata_field_size(enum_field_types field_type) const;

  /**
    Returns the metadata information of a column.

    @param map    Table_map_event. It contains the information of the col type.
    @param col_no The index of the column for which the metadata info is needed.
                  The count starts from 0.
    @return       The metadata value.
  */
  uint32_t extract_metadata(const Table_map_event *map, int col_no);

  //Row_iterator end() const;
private:
    uint32_t fields(Iterator_value_type *fields_vector );
    const Rows_event *m_row_event;
    const Table_map_event *m_table_map;
    unsigned long m_new_field_offset_calculated;
    unsigned long m_field_offset;
};
}
#endif	/* FIELD_ITERATOR_INCLUDED */
