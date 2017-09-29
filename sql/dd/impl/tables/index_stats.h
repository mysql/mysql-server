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

#ifndef DD_TABLES__INDEX_STATS_INCLUDED
#define DD_TABLES__INDEX_STATS_INCLUDED

#include <new>

#include "sql/dd/impl/types/entity_object_table_impl.h"
#include "sql/dd/impl/types/index_stat_impl.h"
#include "sql/dd/string_type.h"                          // dd::String_type
#include "sql/dd/types/index_stat.h"

namespace dd {

class Object_key;
class Raw_record;

namespace tables {

///////////////////////////////////////////////////////////////////////////

class Index_stats: virtual public Entity_object_table_impl
{
public:
  Index_stats();

  static const Index_stats &instance();

  static const String_type &table_name()
  {
    static String_type s_table_name("index_stats");
    return s_table_name;
  }

  enum enum_fields
  {
    FIELD_SCHEMA_NAME,
    FIELD_TABLE_NAME,
    FIELD_INDEX_NAME,
    FIELD_COLUMN_NAME,
    FIELD_CARDINALITY,
    FIELD_CACHED_TIME
  };

public:
  virtual const String_type &name() const
  { return Index_stats::table_name(); }

  virtual Index_stat *create_entity_object(const Raw_record &) const
  { return new (std::nothrow) Index_stat_impl(); }

public:
  static Index_stat::name_key_type *create_object_key(
                                      const String_type &schema_name,
                                      const String_type &table_name,
                                      const String_type &index_name,
                                      const String_type &column_name);

  static Object_key *create_range_key_by_table_name(
                                       const String_type &schema_name,
                                       const String_type &table_name);
};

}
}

#endif // DD_TABLES__INDEX_STATS_INCLUDED
