/* Copyright (c) 2014, 2016 Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD__INDEX_STAT_IMPL_INCLUDED
#define DD__INDEX_STAT_IMPL_INCLUDED

#include "dd/impl/types/entity_object_impl.h" // dd::Entity_object_impl
#include "dd/types/dictionary_object_table.h" // dd::Dictionary_object_table
#include "dd/types/index_stat.h"              // dd::Index_stats
#include "dd/types/object_type.h"             // dd::Object_type

#include <memory>

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Raw_table;
class Transaction;

class Charset;

///////////////////////////////////////////////////////////////////////////

class Index_stat_impl : public Entity_object_impl,
                        public Index_stat
{
public:
  Index_stat_impl()
   :m_cardinality(0)
  { }

public:
  virtual void debug_print(std::string &outb) const;

  virtual const Dictionary_object_table &object_table() const
  { return Index_stat::OBJECT_TABLE(); }

  virtual bool validate() const;

  virtual bool restore_attributes(const Raw_record &r);
  virtual bool store_attributes(Raw_record *r);

public:

  /////////////////////////////////////////////////////////////////////////
  // schema name.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &schema_name() const
  { return m_schema_name; }

  virtual void set_schema_name(const std::string &schema_name)
  { m_schema_name= schema_name; }

  /////////////////////////////////////////////////////////////////////////
  // table name.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &table_name() const
  { return m_table_name; }

  virtual void set_table_name(const std::string &table_name)
  { m_table_name= table_name; }

  /////////////////////////////////////////////////////////////////////////
  // index name.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &index_name() const
  { return m_index_name; }

  virtual void set_index_name(const std::string &index_name)
  { m_index_name= index_name; }

  /////////////////////////////////////////////////////////////////////////
  // column name.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &column_name() const
  { return m_column_name; }

  virtual void set_column_name(const std::string &column_name)
  { m_column_name= column_name; }

  /////////////////////////////////////////////////////////////////////////
  // cardinality.
  /////////////////////////////////////////////////////////////////////////

  virtual ulonglong cardinality() const
  { return m_cardinality; }

  virtual void set_cardinality(ulonglong cardinality)
  { m_cardinality= cardinality; }

public:
  virtual Object_key *create_primary_key() const;
  virtual bool has_new_primary_key() const;

private:
  // Fields
  std::string m_schema_name;
  std::string m_table_name;
  std::string m_index_name;
  std::string m_column_name;

  ulonglong m_cardinality;

};

///////////////////////////////////////////////////////////////////////////

class Index_stat_type : public Object_type
{
public:
  virtual Weak_object *create_object() const
  { return new (std::nothrow) Index_stat_impl(); }

  virtual void register_tables(Open_dictionary_tables_ctx *otx) const;
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__INDEX_STAT_IMPL_INCLUDED
