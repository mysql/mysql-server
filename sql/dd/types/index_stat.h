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

#ifndef DD__INDEX_STAT_INCLUDED
#define DD__INDEX_STAT_INCLUDED

#include "sql/dd/types/entity_object.h"       // Entity_object
#include "sql/dd/types/entity_object_table.h" // Entity_object_table

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Composite_4char_key;
class Index_stat_impl;

namespace tables {
  class Index_stats;
};

///////////////////////////////////////////////////////////////////////////

class Index_stat : virtual public Entity_object
{
public:
  typedef Index_stat_impl Impl;
  typedef tables::Index_stats DD_table;
  typedef Composite_4char_key Name_key;

public:
  /////////////////////////////////////////////////////////////////////////
  // schema name.
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &schema_name() const = 0;
  virtual void set_schema_name(const String_type &schema_name) = 0;

  /////////////////////////////////////////////////////////////////////////
  // table name.
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &table_name() const = 0;
  virtual void set_table_name(const String_type &table_name) = 0;

  /////////////////////////////////////////////////////////////////////////
  // index name.
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &index_name() const = 0;
  virtual void set_index_name(const String_type &index_name) = 0;

  /////////////////////////////////////////////////////////////////////////
  // column name.
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &column_name() const = 0;
  virtual void set_column_name(const String_type &column_name) = 0;

  /////////////////////////////////////////////////////////////////////////
  // cardinality.
  /////////////////////////////////////////////////////////////////////////

  virtual ulonglong cardinality() const = 0;
  virtual void set_cardinality(ulonglong cardinality) = 0;

  /////////////////////////////////////////////////////////////////////////
  // cached_time.
  /////////////////////////////////////////////////////////////////////////

  virtual ulonglong cached_time() const = 0;
  virtual void set_cached_time(ulonglong cached_time) = 0;

};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__INDEX_STAT_INCLUDED
