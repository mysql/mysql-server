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

#ifndef DD_TABLES__INDEX_PARTITIONS_INCLUDED
#define DD_TABLES__INDEX_PARTITIONS_INCLUDED

#include "sql/dd/impl/types/object_table_impl.h" // dd::Object_table_impl
#include "sql/dd/object_id.h"                // dd::Object_id
#include "sql/dd/string_type.h"

namespace dd {
  class Object_key;

namespace tables {

///////////////////////////////////////////////////////////////////////////

class Index_partitions : public Object_table_impl
{
public:
  static const Index_partitions &instance();

  enum enum_fields
  {
    FIELD_PARTITION_ID,
    FIELD_INDEX_ID,
    FIELD_OPTIONS,
    FIELD_SE_PRIVATE_DATA,
    FIELD_TABLESPACE_ID
  };

  enum enum_indexes
  {
    INDEX_PK_PARTITION_ID_INDEX_ID,
    INDEX_K_INDEX_ID,
    INDEX_K_TABLESPACE_ID
 };

  enum enum_foreign_keys
  {
    FK_TABLE_PARTITION_ID,
    FK_INDEX_ID,
    FK_TABLESPACE_ID
  };

  Index_partitions();

  static Object_key *create_key_by_partition_id(Object_id partition_id);

  static Object_key *create_primary_key(
    Object_id partition_id, Object_id index_id);
};

///////////////////////////////////////////////////////////////////////////

}
}

#endif // DD_TABLES__INDEX_PARTITIONS_INCLUDED
