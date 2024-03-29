/* Copyright (c) 2014, 2023, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef DD__TABLE_STAT_IMPL_INCLUDED
#define DD__TABLE_STAT_IMPL_INCLUDED

#include <memory>
#include <new>
#include <string>

#include "my_inttypes.h"
#include "sql/dd/impl/raw/raw_record.h"
#include "sql/dd/impl/types/entity_object_impl.h"  // dd::Entity_object_impl
#include "sql/dd/object_id.h"
#include "sql/dd/string_type.h"
#include "sql/dd/types/entity_object_table.h"
#include "sql/dd/types/table_stat.h"  // dd::Table_stat

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Charset;
class Object_key;
class Object_table;
class Open_dictionary_tables_ctx;
class Raw_table;
class Transaction;
class Weak_object;
class Object_table;

///////////////////////////////////////////////////////////////////////////

class Table_stat_impl : public Entity_object_impl, public Table_stat {
 public:
  Table_stat_impl()
      : m_table_rows(0),
        m_avg_row_length(0),
        m_data_length(0),
        m_max_data_length(0),
        m_index_length(0),
        m_data_free(0),
        m_auto_increment(0),
        m_checksum(0),
        m_update_time(0),
        m_check_time(0),
        m_cached_time(0) {}

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
  // table_rows.
  /////////////////////////////////////////////////////////////////////////

  ulonglong table_rows() const override { return m_table_rows; }

  void set_table_rows(ulonglong table_rows) override {
    m_table_rows = table_rows;
  }

  /////////////////////////////////////////////////////////////////////////
  // avg_row_length.
  /////////////////////////////////////////////////////////////////////////

  ulonglong avg_row_length() const override { return m_avg_row_length; }

  void set_avg_row_length(ulonglong avg_row_length) override {
    m_avg_row_length = avg_row_length;
  }

  /////////////////////////////////////////////////////////////////////////
  // data_length.
  /////////////////////////////////////////////////////////////////////////

  ulonglong data_length() const override { return m_data_length; }

  void set_data_length(ulonglong data_length) override {
    m_data_length = data_length;
  }

  /////////////////////////////////////////////////////////////////////////
  // max_data_length.
  /////////////////////////////////////////////////////////////////////////

  ulonglong max_data_length() const override { return m_max_data_length; }

  void set_max_data_length(ulonglong max_data_length) override {
    m_max_data_length = max_data_length;
  }

  /////////////////////////////////////////////////////////////////////////
  // index_length.
  /////////////////////////////////////////////////////////////////////////

  ulonglong index_length() const override { return m_index_length; }

  void set_index_length(ulonglong index_length) override {
    m_index_length = index_length;
  }

  /////////////////////////////////////////////////////////////////////////
  // data_free.
  /////////////////////////////////////////////////////////////////////////

  ulonglong data_free() const override { return m_data_free; }

  void set_data_free(ulonglong data_free) override { m_data_free = data_free; }

  /////////////////////////////////////////////////////////////////////////
  // auto_increment.
  /////////////////////////////////////////////////////////////////////////

  ulonglong auto_increment() const override { return m_auto_increment; }

  void set_auto_increment(ulonglong auto_increment) override {
    m_auto_increment = auto_increment;
  }

  /////////////////////////////////////////////////////////////////////////
  // checksum.
  /////////////////////////////////////////////////////////////////////////

  ulonglong checksum() const override { return m_checksum; }

  void set_checksum(ulonglong checksum) override { m_checksum = checksum; }

  /////////////////////////////////////////////////////////////////////////
  // update_time.
  /////////////////////////////////////////////////////////////////////////

  ulonglong update_time() const override { return m_update_time; }

  void set_update_time(ulonglong update_time) override {
    m_update_time = update_time;
  }

  /////////////////////////////////////////////////////////////////////////
  // check_time.
  /////////////////////////////////////////////////////////////////////////

  ulonglong check_time() const override { return m_check_time; }

  void set_check_time(ulonglong check_time) override {
    m_check_time = check_time;
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

  ulonglong m_table_rows;
  ulonglong m_avg_row_length;
  ulonglong m_data_length;
  ulonglong m_max_data_length;
  ulonglong m_index_length;
  ulonglong m_data_free;
  ulonglong m_auto_increment;
  ulonglong m_checksum;
  ulonglong m_update_time;
  ulonglong m_check_time;
  ulonglong m_cached_time;
};

///////////////////////////////////////////////////////////////////////////

}  // namespace dd

#endif  // DD__TABLE_STAT_IMPL_INCLUDED
