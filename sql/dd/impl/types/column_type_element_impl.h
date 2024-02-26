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

#ifndef DD__COLUMN_TYPE_ELEMENT_IMPL_INCLUDED
#define DD__COLUMN_TYPE_ELEMENT_IMPL_INCLUDED

#include <sys/types.h>
#include <new>

#include "sql/dd/impl/types/weak_object_impl.h"  // dd::Weak_object_impl
#include "sql/dd/sdi_fwd.h"
#include "sql/dd/string_type.h"
#include "sql/dd/types/column_type_element.h"  // dd::Column_type_element

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Column;
class Column_impl;
class Object_key;
class Object_table;
class Open_dictionary_tables_ctx;
class Raw_record;
class Sdi_rcontext;
class Sdi_wcontext;
class Weak_object;

///////////////////////////////////////////////////////////////////////////

class Column_type_element_impl : public Weak_object_impl,
                                 public Column_type_element {
 public:
  Column_type_element_impl() : m_index(0) {}

  Column_type_element_impl(Column_impl *column)
      : m_index(0), m_column(column) {}

  Column_type_element_impl(const Column_type_element_impl &src,
                           Column_impl *parent);

  ~Column_type_element_impl() override = default;

 public:
  const Object_table &object_table() const override;

  static void register_tables(Open_dictionary_tables_ctx *otx);

  bool validate() const override;

  bool store_attributes(Raw_record *r) override;

  bool restore_attributes(const Raw_record &r) override;

  void set_ordinal_position(uint ordinal_position) {
    m_index = ordinal_position;
  }

  virtual uint ordinal_position() const { return index(); }

  void serialize(Sdi_wcontext *wctx, Sdi_writer *w) const override;

  bool deserialize(Sdi_rcontext *rctx, const RJ_Value &val) override;

 public:
  static Column_type_element_impl *restore_item(Column_impl *column) {
    return new (std::nothrow) Column_type_element_impl(column);
  }

  static Column_type_element_impl *clone(const Column_type_element_impl &other,
                                         Column_impl *column) {
    return new (std::nothrow) Column_type_element_impl(other, column);
  }

 public:
  /////////////////////////////////////////////////////////////////////////
  // Name.
  /////////////////////////////////////////////////////////////////////////

  const String_type &name() const override { return m_name; }

  void set_name(const String_type &name) override { m_name = name; }

  /////////////////////////////////////////////////////////////////////////
  // Column
  /////////////////////////////////////////////////////////////////////////

  const Column &column() const override;

  /////////////////////////////////////////////////////////////////////////
  // index.
  /////////////////////////////////////////////////////////////////////////

  uint index() const override { return m_index; }

 public:
  void debug_print(String_type &outb) const override;

 protected:
  Object_key *create_primary_key() const override;
  bool has_new_primary_key() const override;

 protected:
  // Fields
  String_type m_name;
  uint m_index;

  // References to other objects
  Column_impl *m_column;
};

///////////////////////////////////////////////////////////////////////////

}  // namespace dd

#endif  // DD__COLUMN_TYPE_ELEMENT_IMPL_INCLUDED
