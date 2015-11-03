/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

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

#include "dd/impl/object_table_registry.h"

#include "dd/iterator.h"                             // dd::Iterator
#include "dd/properties.h"                           // Needed for destructor
#include "dd/impl/tables/character_sets.h"           // Character_sets
#include "dd/impl/tables/columns.h"                  // Columns
#include "dd/impl/tables/column_type_elements.h"     // Column_type_elements
#include "dd/impl/tables/collations.h"               // Collations
#include "dd/impl/tables/foreign_keys.h"             // Foreign_keys
#include "dd/impl/tables/foreign_key_column_usage.h" // Foreign_key_column_usage
#include "dd/impl/tables/indexes.h"                  // Indexes
#include "dd/impl/tables/index_column_usage.h"       // Index_column_usage
#include "dd/impl/tables/index_partitions.h"         // Index_partitions
#include "dd/impl/tables/non_represented_tables.h"   // Innodb_table_stats
#include "dd/impl/tables/schemata.h"                 // Schemata
#include "dd/impl/tables/tables.h"                   // Tables
#include "dd/impl/tables/table_partitions.h"         // Table_partitions
#include "dd/impl/tables/table_partition_values.h"   // Table_partition_values
#include "dd/impl/tables/tablespaces.h"              // Tablespaces
#include "dd/impl/tables/tablespace_files.h"         // Tablespace_files
#include "dd/impl/tables/view_table_usage.h"         // View_table_usage

using namespace dd::tables;

///////////////////////////////////////////////////////////////////////////

namespace
{
  template <typename X>
  void register_object_table()
  {
    dd::Object_table_registry::instance()->add_type(X::instance());
  }
}

///////////////////////////////////////////////////////////////////////////

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Object_table_iterator : public Iterator<const Object_table>
{
public:
  Object_table_iterator(Object_table_array *types)
   :m_types(types),
    m_iterator(m_types->begin())
  { } /* purecov: tested */

public:
  virtual const Object_table *next()
  {
    if (m_iterator == m_types->end())
      return 0;

    const Object_table *t= *m_iterator;

    ++m_iterator;

    return t;
  }

private:
  Object_table_array *m_types;
  Object_table_array::iterator m_iterator;
};

///////////////////////////////////////////////////////////////////////////

Iterator<const Object_table> *Object_table_registry::types()
{
  return new (std::nothrow) Object_table_iterator(&m_types);
}

///////////////////////////////////////////////////////////////////////////

bool Object_table_registry::init()
{
  // Order is dictated by the foreign key constraints
  register_object_table<Innodb_table_stats>();
  register_object_table<Innodb_index_stats>();
  register_object_table<Character_sets>();
  register_object_table<Collations>();
  register_object_table<Tablespaces>();
  register_object_table<Tablespace_files>();
  register_object_table<Catalogs>();
  register_object_table<Schemata>();
  register_object_table<Tables>();
  register_object_table<View_table_usage>();
  register_object_table<Columns>();
  register_object_table<Indexes>();
  register_object_table<Index_column_usage>();
  register_object_table<Column_type_elements>();
  register_object_table<Foreign_keys>();
  register_object_table<Foreign_key_column_usage>();
  register_object_table<Table_partitions>();
  register_object_table<Table_partition_values>();
  register_object_table<Index_partitions>();

  return false;
}

///////////////////////////////////////////////////////////////////////////

}
