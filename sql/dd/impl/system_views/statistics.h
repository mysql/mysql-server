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

#ifndef DD_SYSTEM_VIEWS__STATISTICS_INCLUDED
#define DD_SYSTEM_VIEWS__STATISTICS_INCLUDED

#include "sql/dd/impl/system_views/system_view_definition_impl.h"
#include "sql/dd/impl/system_views/system_view_impl.h"
#include "sql/dd/string_type.h"

namespace dd {
namespace system_views {

/*
  The class representing INFORMATION_SCHEMA.STATISTICS system view
  definition common to both modes of setting
  'information_schema_stats=latest|cached'. This class is also
  used by SHOW STATISTICS command.

  There are two definitions of information_schema.statistics.
  1. INFORMATION_SCHEMA.STATISTICS view which picks dynamic column
     statistics from mysql.index_stats which gets populated when
     we execute 'anaylze table' command.

  2. INFORMATION_SCHEMA.STATISTICS_DYNAMIC view which retrieves dynamic
     column statistics using a internal UDF which opens the user
     table and reads dynamic table statistics.

  MySQL server uses definition 1) by default. The session variable
  information_schema_stats=latest would enable use of definition 2).
*/
class Statistics_base :
        public System_view_impl<System_view_select_definition_impl>
{
public:
  enum enum_fields
  {
    FIELD_TABLE_CATALOG,
    FIELD_TABLE_SCHEMA,
    FIELD_TABLE_NAME,
    FIELD_NON_UNIQUE,
    FIELD_INDEX_SCHEMA,
    FIELD_INDEX_NAME,
    FIELD_SEQ_IN_INDEX,
    FIELD_COLUMN_NAME,
    FIELD_COLLATION,
    FIELD_CARDINALITY,
    FIELD_SUB_PART,
    FIELD_PACKED,
    FIELD_NULLABLE,
    FIELD_INDEX_TYPE,
    FIELD_COMMENT,
    FIELD_INDEX_COMMENT,
    FIELD_IS_VISIBLE,
    FIELD_INDEX_ORDINAL_POSITION,
    FIELD_COLUMN_ORDINAL_POSITION
  };

  Statistics_base();

  virtual const String_type &name() const = 0;
};


/*
 The class representing INFORMATION_SCHEMA.STATISTICS system view definition
 used when setting is 'information_schema_stats=cached'.
*/
class Statistics: public Statistics_base
{
public:
  Statistics();

  static const Statistics_base &instance();

  static const String_type &view_name()
  {
    static String_type s_view_name("STATISTICS");
    return s_view_name;
  }

  virtual const String_type &name() const
  { return Statistics::view_name(); }
};


/*
 The class representing INFORMATION_SCHEMA.STATISTICS system view definition
 used when setting is 'information_schema_stats=latest'.
*/
class Statistics_dynamic: public Statistics_base
{
public:
  Statistics_dynamic();

  static const Statistics_base &instance();

  static const String_type &view_name()
  {
    static String_type s_view_name("STATISTICS_DYNAMIC");
    return s_view_name;
  }

  virtual const String_type &name() const
  { return Statistics_dynamic::view_name(); }

  // This view definition is hidden from user.
  virtual bool hidden() const
  { return true; }
};


/*
 The class represents system view definition used by SHOW STATISTICS when
 setting is 'information_schema_stats=cached'.
*/
class Show_statistics: public Statistics
{
public:
  Show_statistics();

  static const Statistics_base &instance();

  static const String_type &view_name()
  {
    static String_type s_view_name("SHOW_STATISTICS");
    return s_view_name;
  }

  virtual const String_type &name() const
  { return Show_statistics::view_name(); }

  // This view definition is hidden from user.
  virtual bool hidden() const
  { return true; }
};


/*
 The class represents system view definition used by SHOW STATISTICS when
 setting is 'information_schema_stats=latest'.
*/
class Show_statistics_dynamic: public Statistics_dynamic
{
public:
  Show_statistics_dynamic();

  static const Statistics_base &instance();

  static const String_type &view_name()
  {
    static String_type s_view_name("SHOW_STATISTICS_DYNAMIC");
    return s_view_name;
  }

  virtual const String_type &name() const
  { return Show_statistics_dynamic::view_name(); }

  // This view definition is hidden from user.
  virtual bool hidden() const
  { return true; }
};

}
}

#endif // DD_SYSTEM_VIEWS__STATISTICS_INCLUDED
