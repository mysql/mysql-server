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

#ifndef DD_TABLES__TABLE_STATS_INCLUDED
#define DD_TABLES__TABLE_STATS_INCLUDED


#include <new>
#include <string>

#include "sql/dd/impl/raw/object_keys.h"      // Composite_char_key
#include "sql/dd/impl/types/entity_object_table_impl.h"
                                              // Entity_object_table_impl
#include "sql/dd/impl/types/table_stat_impl.h" // Table_stat
#include "sql/dd/string_type.h"
#include "sql/dd/types/table_stat.h"

namespace dd {

class Raw_record;

namespace tables {

///////////////////////////////////////////////////////////////////////////

class Table_stats: virtual public Entity_object_table_impl
{
public:
  Table_stats();

  static const Table_stats &instance();

  static const String_type &table_name()
  {
    static String_type s_table_name("table_stats");
    return s_table_name;
  }

  enum enum_fields
  {
    FIELD_SCHEMA_NAME,
    FIELD_TABLE_NAME,
    FIELD_TABLE_ROWS,
    FIELD_AVG_ROW_LENGTH,
    FIELD_DATA_LENGTH,
    FIELD_MAX_DATA_LENGTH,
    FIELD_INDEX_LENGTH,
    FIELD_DATA_FREE,
    FIELD_AUTO_INCREMENT,
    FIELD_CHECKSUM,
    FIELD_UPDATE_TIME,
    FIELD_CHECK_TIME,
    FIELD_CACHED_TIME
  };

public:

  virtual const String_type &name() const
  { return Table_stats::table_name(); }

  virtual Table_stat *create_entity_object(const Raw_record &) const
  { return new (std::nothrow) Table_stat_impl(); }

public:
  static Table_stat::name_key_type *create_object_key(const String_type &schema_name,
                                                      const String_type &table_name);
};

}
}

#endif // DD_TABLES__TABLE_STATS_INCLUDED
