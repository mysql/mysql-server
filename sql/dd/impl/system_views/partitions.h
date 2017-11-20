/* Copyright (c) 2017 Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD_SYSTEM_VIEWS__PARTITIONS_INCLUDED
#define DD_SYSTEM_VIEWS__PARTITIONS_INCLUDED

#include "dd/impl/system_views/system_view_definition_impl.h"
#include "dd/impl/system_views/system_view_impl.h"

namespace dd {
namespace system_views {

/*
  The class representing INFORMATION_SCHEMA.PARTITIONS system view definition.
*/
class Partitions : public System_view_impl<System_view_select_definition_impl>
{
public:
  enum enum_fields
  {
    FIELD_TABLE_CATALOG,
    FIELD_TABLE_SCHEMA,
    FIELD_TABLE_NAME,
    FIELD_PARTITION_NAME,
    FIELD_SUBPARTITION_NAME,
    FIELD_PARTITION_ORDINAL_POSITION,
    FIELD_SUBPARTITION_ORDINAL_POSITION,
    FIELD_PARTITION_METHOD,
    FIELD_SUBPARTITION_METHOD,
    FIELD_PARTITION_EXPRESSION,
    FIELD_SUBPARTITION_EXPRESSION,
    FIELD_PARTITION_DESCRIPTION,
    FIELD_TABLE_ROWS,
    FIELD_AVG_ROW_LENGTH,
    FIELD_DATA_LENGTH,
    FIELD_MAX_DATA_LENGTH,
    FIELD_INDEX_LENGTH,
    FIELD_DATA_FREE,
    FIELD_CREATE_TIME,
    FIELD_UPDATE_TIME,
    FIELD_CHECK_TIME,
    FIELD_CHECKSUM,
    FIELD_PARTITION_COMMENT,
    FIELD_NODEGROUP,
    FIELD_TABLESPACE_NAME
  };

  Partitions();

  static const Partitions &instance();

  static const String_type &view_name()
  {
    static String_type s_view_name("PARTITIONS");
    return s_view_name;
  }
  virtual const String_type &name() const
  { return Partitions::view_name(); }
};

}
}

#endif // DD_SYSTEM_VIEWS__PARTITIONS_INCLUDED
