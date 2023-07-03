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

#ifndef DD__PARTITION_VALUE_IMPL_INCLUDED
#define DD__PARTITION_VALUE_IMPL_INCLUDED

#include <stddef.h>
#include <sys/types.h>
#include <new>

#include "sql/dd/impl/types/weak_object_impl.h"  // dd::Weak_object_impl
#include "sql/dd/sdi_fwd.h"
#include "sql/dd/string_type.h"
#include "sql/dd/types/partition_value.h"  // dd::Partition_value

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Object_key;
class Object_table;
class Open_dictionary_tables_ctx;
class Partition;
class Partition_impl;
class Raw_record;
class Sdi_rcontext;
class Sdi_wcontext;
class Weak_object;

///////////////////////////////////////////////////////////////////////////

class Partition_value_impl : public Weak_object_impl, public Partition_value {
 public:
  Partition_value_impl()
      : m_max_value(false),
        m_null_value(false),
        m_list_num(0),
        m_column_num(0),
        m_partition(nullptr) {}

  Partition_value_impl(Partition_impl *partition)
      : m_max_value(false),
        m_null_value(false),
        m_list_num(0),
        m_column_num(0),
        m_partition(partition) {}

  Partition_value_impl(const Partition_value_impl &src, Partition_impl *parent);

  ~Partition_value_impl() override = default;

 public:
  const Object_table &object_table() const override;

  bool validate() const override;

  bool store_attributes(Raw_record *r) override;

  bool restore_attributes(const Raw_record &r) override;

  void serialize(Sdi_wcontext *wctx, Sdi_writer *w) const override;

  bool deserialize(Sdi_rcontext *rctx, const RJ_Value &val) override;

  void set_ordinal_position(uint) {}

  virtual uint ordinal_position() const { return -1; }

 public:
  static void register_tables(Open_dictionary_tables_ctx *otx);

  /////////////////////////////////////////////////////////////////////////
  // index.
  /////////////////////////////////////////////////////////////////////////

  const Partition &partition() const override;

  Partition &partition() override;

  /////////////////////////////////////////////////////////////////////////
  // list_num.
  /////////////////////////////////////////////////////////////////////////

  uint list_num() const override { return m_list_num; }

  void set_list_num(uint list_num) override { m_list_num = list_num; }

  /////////////////////////////////////////////////////////////////////////
  // column_num.
  /////////////////////////////////////////////////////////////////////////

  uint column_num() const override { return m_column_num; }

  void set_column_num(uint column_num) override { m_column_num = column_num; }

  /////////////////////////////////////////////////////////////////////////
  // value.
  /////////////////////////////////////////////////////////////////////////

  const String_type &value_utf8() const override { return m_value_utf8; }

  void set_value_utf8(const String_type &value) override {
    m_value_utf8 = value;
  }

  ////////////////////////////////////////////////////////////////
  // max_value.
  /////////////////////////////////////////////////////////////////////////

  bool max_value() const override { return m_max_value; }

  void set_max_value(bool max_value) override { m_max_value = max_value; }

  ////////////////////////////////////////////////////////////////
  // null_value.
  /////////////////////////////////////////////////////////////////////////

  bool is_value_null() const override { return m_null_value; }

  void set_value_null(bool is_null) override { m_null_value = is_null; }

  /////////////////////////////////////////////////////////////////////////

 public:
  static Partition_value_impl *restore_item(Partition_impl *partition) {
    return new (std::nothrow) Partition_value_impl(partition);
  }

  static Partition_value_impl *clone(const Partition_value_impl &other,
                                     Partition_impl *partition) {
    return new (std::nothrow) Partition_value_impl(other, partition);
  }

 public:
  void debug_print(String_type &outb) const override;

 public:
  Object_key *create_primary_key() const override;
  bool has_new_primary_key() const override;

 private:
  // Fields
  bool m_max_value;
  bool m_null_value;

  uint m_list_num;
  uint m_column_num;

  String_type m_value_utf8;

  // References to other objects
  Partition_impl *m_partition;
};

///////////////////////////////////////////////////////////////////////////

/**
  Used to sort Partition_value objects for the same partition first
  according to list number and then according to the column number.
*/

struct Partition_value_order_comparator {
  bool operator()(const dd::Partition_value *pv1,
                  const dd::Partition_value *pv2) const {
    return ((pv1->list_num() < pv2->list_num()) ||
            (pv1->list_num() == pv2->list_num() &&
             pv1->column_num() < pv2->column_num()));
  }
};

///////////////////////////////////////////////////////////////////////////

}  // namespace dd

#endif  // DD__PARTITION_VALUE_IMPL_INCLUDED
