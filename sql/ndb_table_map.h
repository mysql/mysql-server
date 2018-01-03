/*
   Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_TABLE_MAP_H
#define NDB_TABLE_MAP_H

#include "my_bitmap.h"
#include "my_dbug.h"
#include "storage/ndb/include/ndbapi/NdbApi.hpp"

/** Ndb_table_map
*
*   An Ndb_table_map for a table provides a map between MySQL fields and
*   NDB columns.  Some MySQL fields, such as virtual generated columns, do
*   not exist in NDB.  Some NDB columns, such as hidden primary keys and
*   partition ID columns, are not visible as MySQL fields.
*
*   Ndb_table_map provides a getColumn() method that wraps
*   NdbDictionary::Table::getColumn(), translating from a field number to the
*   appropriate column number.  It also provides a method get_column_mask()
*   for wholesale translation, when needed, of an entire bitmap of field
*   numbers to column numbers.
*
*   The introduction of virtual generated columns from WL#411 requires the
*   handler to understand that some fields are not stored, and to map between 
*   MySQL Field Ids and NDB Column Ids (which are no longer equivalent).
*
*/
class Ndb_table_map {
public:
  Ndb_table_map(struct TABLE*, const NdbDictionary::Table * ndb_table = 0);
  ~Ndb_table_map();

  /* Get the NDB column number for a MySQL field.
     The user must check field->stored_in_db, and only look up stored fields.
  */
  uint get_column_for_field(uint mysql_field_number) const;

  /* Get an NDB column by MySQL field number.
     The user must check field->stored_in_db, and only look up stored fields.
  */
  const NdbDictionary::Column * getColumn(uint mysql_field_number) const;

  /* Get column by field number; non-const version for CREATE TABLE.
     The user must check field->stored_in_db, and only look up stored fields.
  */
  NdbDictionary::Column * getColumn(NdbDictionary::Table &,
                                    uint mysql_field_number) const;

  /* Get Blob handle by MySQL field number.
     The user must check field->stored_in_db, and only look up stored fields.
  */
  NdbBlob * getBlobHandle(const NdbOperation *, uint mysql_field_number) const;

  /* Get NDB column numbers for special columns that are hidden from MySQL */
  uint get_hidden_key_column() const;
  uint get_partition_id_column() const;

  /* Get the MySQL field number for an NBD column */
  uint get_field_for_column(uint ndb_col_number) const;

  /* get_column_mask():
     Takes a pointer to a MySQL bitmask.  
     Returns a pointer which can be used as a record mask when building an
     NdbRecord operation.

     If mysql_field_map is NULL, rewrite_bitmap() returns NULL.

     If mysql_field_map is non-NULL but translation is not necessary,
     rewrite_bitmap() returns a pointer to the bitmap within mysql_field_map.

     If necessary, rewrite_bitmap() will update an internal bitmask to provide
     a translation from the field numbers in mysql_field_map to NDB column
     numbers, and return a pointer to the bitmap in this mask.  The memory
     for this internal bitmask is owned by Ndb_table_map and will be reused
     by subsequent calls to rewrite_bitmap().
 */
  unsigned char * get_column_mask(const MY_BITMAP * mysql_field_map);


  /*
   Adapter function for checking wheter a TABLE*
   has virtual generated columns.
   Function existed in 5.7 as table->has_virtual_gcol()
  */
  static bool has_virtual_gcol(const struct TABLE* table);

  /*
    Adapter function for returning the number of
    stored fields in the TABLE*(i.e those who are
    not virtual).
  */
  static uint num_stored_fields(const struct TABLE* table);

  /*
    Check if the table has physical blob columns(i.e actually stored in
    the engine)
   */
  static bool have_physical_blobs(const struct TABLE* table);

#ifndef DBUG_OFF
  static void print_record(const struct TABLE *table, const uchar *record);
  static void print_table(const char *info, const struct TABLE *table);
#endif

private:
  const NdbDictionary::Table * m_ndb_table;
  MY_BITMAP m_moved_fields;
  MY_BITMAP m_rewrite_set;
  int * m_map_by_field;
  int * m_map_by_col;
  const uint m_array_size;
  const uint m_stored_fields;
  const unsigned short m_hidden_pk;
  const bool m_trivial;
};


// inline implementations

inline const NdbDictionary::Column * Ndb_table_map::getColumn(uint field) const
{
  return m_ndb_table->getColumn(get_column_for_field(field));
}

inline NdbDictionary::Column *
  Ndb_table_map::getColumn(NdbDictionary::Table & create_table, uint field) const
{
  return create_table.getColumn(get_column_for_field(field));
}

inline NdbBlob * Ndb_table_map::getBlobHandle(const NdbOperation *ndb_op,
                                              uint mysql_field_number) const
{
  return ndb_op->getBlobHandle(get_column_for_field(mysql_field_number));
}

inline uint Ndb_table_map::get_hidden_key_column() const
{
  DBUG_ASSERT(m_hidden_pk);
  // The hidden primary key is just after the final stored, visible column
  return m_stored_fields;
}

inline uint Ndb_table_map::get_partition_id_column() const
{
  // The hidden partition id, if present, is the final column
  return m_stored_fields + m_hidden_pk;
}



#endif
