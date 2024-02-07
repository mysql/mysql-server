/* Copyright (c) 2014, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef DD__INDEX_STAT_IMPL_INCLUDED
#define DD__INDEX_STAT_IMPL_INCLUDED

#include <memory>
#include <new>
#include <string>

#include "my_inttypes.h"
#include "sql/dd/impl/raw/raw_record.h"
#include "sql/dd/impl/types/entity_object_impl.h"  // dd::Entity_object_impl
#include "sql/dd/object_id.h"
#include "sql/dd/string_type.h"
#include "sql/dd/types/entity_object_table.h"
#include "sql/dd/types/index_stat.h"  // dd::Index_stat

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Charset;
class Object_key;
class Open_dictionary_tables_ctx;
class Raw_table;
class Weak_object;
class Object_table;

///////////////////////////////////////////////////////////////////////////

class Index_stat_impl : public Entity_object_impl, public Index_stat {
 public:
  Index_stat_impl() : m_cardinality(0), m_cached_time(0) {}

 public:
  void debug_print(String_type &outb) const override;

  const Object_table &object_table() const override;

  bool validate() const override;

  bool restore_attributes(const Raw_record &r) override;
  bool store_attributes(Raw_record *r) override;

 public:
  static void register_tables(Open_dictionary_tables_ctx *otx);

  /////////////////////////////////////////////////////////////////////////
  // schema name.
  /////////////////////////////////////////////////////////////////////////

  const String_type &schema_name() const override { return m_schema_name; }

  void set_schema_name(const String_type &schema_name) override {
    m_schema_name = schema_name;
  }

  /////////////////////////////////////////////////////////////////////////
  // table name.
  /////////////////////////////////////////////////////////////////////////

  const String_type &table_name() const override { return m_table_name; }

  void set_table_name(const String_type &table_name) override {
    m_table_name = table_name;
  }

  /////////////////////////////////////////////////////////////////////////
  // index name.
  /////////////////////////////////////////////////////////////////////////

  const String_type &index_name() const override { return m_index_name; }

  void set_index_name(const String_type &index_name) override {
    m_index_name = index_name;
  }

  /////////////////////////////////////////////////////////////////////////
  // column name.
  /////////////////////////////////////////////////////////////////////////

  const String_type &column_name() const override { return m_column_name; }

  void set_column_name(const String_type &column_name) override {
    m_column_name = column_name;
  }

  /////////////////////////////////////////////////////////////////////////
  // cardinality.
  /////////////////////////////////////////////////////////////////////////

  ulonglong cardinality() const override { return m_cardinality; }

  void set_cardinality(ulonglong cardinality) override {
    m_cardinality = cardinality;
  }

  /////////////////////////////////////////////////////////////////////////
  // cached_time.
  /////////////////////////////////////////////////////////////////////////

  ulonglong cached_time() const override { return m_cached_time; }

  void set_cached_time(ulonglong cached_time) override {
    m_cached_time = cached_time;
  }

 public:
  Object_key *create_primary_key() const override;
  bool has_new_primary_key() const override;

  // Fix "inherits ... via dominance" warnings
  Entity_object_impl *impl() override { return Entity_object_impl::impl(); }
  const Entity_object_impl *impl() const override {
    return Entity_object_impl::impl();
  }
  Object_id id() const override { return Entity_object_impl::id(); }
  bool is_persistent() const override {
    return Entity_object_impl::is_persistent();
  }
  const String_type &name() const override {
    return Entity_object_impl::name();
  }
  void set_name(const String_type &name) override {
    Entity_object_impl::set_name(name);
  }

 private:
  // Fields
  String_type m_schema_name;
  String_type m_table_name;
  String_type m_index_name;
  String_type m_column_name;

  ulonglong m_cardinality;
  ulonglong m_cached_time;
};

///////////////////////////////////////////////////////////////////////////

}  // namespace dd

#endif  // DD__INDEX_STAT_IMPL_INCLUDED
