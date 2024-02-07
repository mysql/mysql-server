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

#ifndef DD__INDEX_ELEMENT_IMPL_INCLUDED
#define DD__INDEX_ELEMENT_IMPL_INCLUDED

#include <stddef.h>
#include <sys/types.h>
#include <new>

#include "m_string.h"
#include "sql/dd/impl/types/index_impl.h"        // dd::Index_impl
#include "sql/dd/impl/types/weak_object_impl.h"  // dd::Weak_object_impl
#include "sql/dd/sdi_fwd.h"
#include "sql/dd/string_type.h"
#include "sql/dd/types/index_element.h"  // dd::Index_element

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Column;
class Index;
class Object_key;
class Object_table;
class Open_dictionary_tables_ctx;
class Raw_record;
class Sdi_rcontext;
class Sdi_wcontext;
class Weak_object;

///////////////////////////////////////////////////////////////////////////

class Index_element_impl : public Weak_object_impl, public Index_element {
 public:
  Index_element_impl()
      : m_ordinal_position(0),
        m_length(-1),
        m_order(Index_element::ORDER_ASC),
        m_hidden(false),
        m_index(nullptr),
        m_column(nullptr) {}

  Index_element_impl(Index_impl *index, Column *column)
      : m_ordinal_position(0),
        m_length(-1),
        m_order(Index_element::ORDER_ASC),
        m_hidden(false),
        m_index(index),
        m_column(column) {}

  Index_element_impl(const Index_element_impl &src, Index_impl *parent,
                     Column *column);

  ~Index_element_impl() override = default;

 public:
  const Object_table &object_table() const override;

  bool validate() const override;

  bool store_attributes(Raw_record *r) override;

  bool restore_attributes(const Raw_record &r) override;

  void serialize(Sdi_wcontext *wctx, Sdi_writer *w) const override;

  bool deserialize(Sdi_rcontext *rctx, const RJ_Value &val) override;

  void set_ordinal_position(uint ordinal_position) {
    m_ordinal_position = ordinal_position;
  }

 public:
  static void register_tables(Open_dictionary_tables_ctx *otx);

  /////////////////////////////////////////////////////////////////////////
  // index.
  /////////////////////////////////////////////////////////////////////////

  const Index &index() const override { return *m_index; }

  Index &index() override { return *m_index; }

  /////////////////////////////////////////////////////////////////////////
  // column.
  /////////////////////////////////////////////////////////////////////////

  const Column &column() const override { return *m_column; }

  Column &column() override { return *m_column; }

  /////////////////////////////////////////////////////////////////////////
  // ordinal_position.
  /////////////////////////////////////////////////////////////////////////

  uint ordinal_position() const override { return m_ordinal_position; }

  /////////////////////////////////////////////////////////////////////////
  // length.
  /////////////////////////////////////////////////////////////////////////

  uint length() const override { return m_length; }

  void set_length(uint length) override { m_length = length; }

  void set_length_null(bool) override { m_length = (uint)-1; }

  bool is_length_null() const override { return m_length == (uint)-1; }

  /////////////////////////////////////////////////////////////////////////
  // is_hidden.
  /////////////////////////////////////////////////////////////////////////

  bool is_hidden() const override { return m_hidden; }

  void set_hidden(bool hidden) override { m_hidden = hidden; }

  /////////////////////////////////////////////////////////////////////////
  // order.
  /////////////////////////////////////////////////////////////////////////

  enum_index_element_order order() const override { return m_order; }

  void set_order(enum_index_element_order order) override { m_order = order; }

  bool is_prefix() const override;

 public:
  static Index_element_impl *restore_item(Index_impl *index) {
    return new (std::nothrow) Index_element_impl(index, nullptr);
  }

  static Index_element_impl *clone(const Index_element_impl &other,
                                   Index_impl *index);

 public:
  void debug_print(String_type &outb) const override;

 public:
  Object_key *create_primary_key() const override;
  bool has_new_primary_key() const override;

 private:
  // Fields
  uint m_ordinal_position;
  uint m_length;

  enum_index_element_order m_order;

  bool m_hidden;

  // References to other objects
  Index_impl *m_index;
  Column *m_column;
};

///////////////////////////////////////////////////////////////////////////

}  // namespace dd

#endif  // DD__INDEX_ELEMENT_IMPL_INCLUDED
