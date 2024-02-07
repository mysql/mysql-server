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

#ifndef DD__PARTITION_INDEX_IMPL_INCLUDED
#define DD__PARTITION_INDEX_IMPL_INCLUDED

#include <stddef.h>
#include <sys/types.h>
#include <memory>
#include <new>

#include "sql/dd/impl/properties_impl.h"
#include "sql/dd/impl/types/weak_object_impl.h"  // dd::Weak_object_impl
#include "sql/dd/object_id.h"
#include "sql/dd/sdi_fwd.h"
#include "sql/dd/string_type.h"
#include "sql/dd/types/index.h"
#include "sql/dd/types/partition_index.h"  // dd::Partition_index

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Index;
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

class Partition_index_impl : public Weak_object_impl, public Partition_index {
 public:
  Partition_index_impl();

  Partition_index_impl(Partition_impl *partition, Index *index);

  Partition_index_impl(const Partition_index_impl &src, Partition_impl *parent,
                       Index *index);

  ~Partition_index_impl() override = default;

 public:
  const Object_table &object_table() const override;

  bool validate() const override;

  bool restore_attributes(const Raw_record &r) override;

  bool store_attributes(Raw_record *r) override;

  void serialize(Sdi_wcontext *wctx, Sdi_writer *w) const override;

  bool deserialize(Sdi_rcontext *rctx, const RJ_Value &val) override;

  void debug_print(String_type &outb) const override;

  void set_ordinal_position(uint) {}

  virtual uint ordinal_position() const { return -1; }

 public:
  static void register_tables(Open_dictionary_tables_ctx *otx);

  /////////////////////////////////////////////////////////////////////////
  // Partition.
  /////////////////////////////////////////////////////////////////////////

  const Partition &partition() const override;

  Partition &partition() override;

  Partition_impl &partition_impl() { return *m_partition; }

  /////////////////////////////////////////////////////////////////////////
  // Index.
  /////////////////////////////////////////////////////////////////////////

  const Index &index() const override;

  Index &index() override;

  /////////////////////////////////////////////////////////////////////////
  // Options.
  /////////////////////////////////////////////////////////////////////////

  const Properties &options() const override { return m_options; }

  Properties &options() override { return m_options; }

  bool set_options(const Properties &options) override {
    return m_options.insert_values(options);
  }

  bool set_options(const String_type &options_raw) override {
    return m_options.insert_values(options_raw);
  }

  /////////////////////////////////////////////////////////////////////////
  // se_private_data.
  /////////////////////////////////////////////////////////////////////////

  const Properties &se_private_data() const override {
    return m_se_private_data;
  }

  Properties &se_private_data() override { return m_se_private_data; }

  bool set_se_private_data(const String_type &se_private_data_raw) override {
    return m_se_private_data.insert_values(se_private_data_raw);
  }

  bool set_se_private_data(const Properties &se_private_data) override {
    return m_se_private_data.insert_values(se_private_data);
  }

  /////////////////////////////////////////////////////////////////////////
  // Tablespace.
  /////////////////////////////////////////////////////////////////////////

  Object_id tablespace_id() const override { return m_tablespace_id; }

  void set_tablespace_id(Object_id tablespace_id) override {
    m_tablespace_id = tablespace_id;
  }

 public:
  static Partition_index_impl *restore_item(Partition_impl *partition) {
    return new (std::nothrow) Partition_index_impl(partition, nullptr);
  }

  static Partition_index_impl *clone(const Partition_index_impl &other,
                                     Partition_impl *partition);

 public:
  Object_key *create_primary_key() const override;
  bool has_new_primary_key() const override;

 private:
  // Fields.

  Properties_impl m_options;
  Properties_impl m_se_private_data;

  // References to tightly-coupled objects.

  Partition_impl *m_partition;
  Index *m_index;

  // References to loosely-coupled objects.

  Object_id m_tablespace_id;
};

///////////////////////////////////////////////////////////////////////////

/**
  Used to sort Partition_index objects for the same partition in
  the same order as Index objects for the table.
*/

struct Partition_index_order_comparator {
  bool operator()(const dd::Partition_index *pi1,
                  const dd::Partition_index *pi2) const {
    return pi1->index().ordinal_position() < pi2->index().ordinal_position();
  }
};

///////////////////////////////////////////////////////////////////////////

}  // namespace dd

#endif  // DD__PARTITION_INDEX_IMPL_INCLUDED
