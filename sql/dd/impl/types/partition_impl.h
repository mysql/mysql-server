/* Copyright (c) 2014, 2022, Oracle and/or its affiliates.

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

#ifndef DD__PARTITION_IMPL_INCLUDED
#define DD__PARTITION_IMPL_INCLUDED

#include <sys/types.h>
#include <memory>
#include <new>

#include "sql/dd/collection.h"
#include "sql/dd/impl/properties_impl.h"
#include "sql/dd/impl/types/entity_object_impl.h"  // dd::Entity_object_impl
#include "sql/dd/impl/types/weak_object_impl.h"
#include "sql/dd/object_id.h"
#include "sql/dd/sdi_fwd.h"
#include "sql/dd/string_type.h"
#include "sql/dd/types/partition.h"        // dd::Partition
#include "sql/dd/types/partition_index.h"  // IWYU pragma: keep
#include "sql/dd/types/partition_value.h"  // IWYU pragma: keep
#include "sql/dd/types/table.h"

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Index;
class Object_table;
class Open_dictionary_tables_ctx;
class Properties;
class Raw_record;
class Sdi_rcontext;
class Sdi_wcontext;
class Table;
class Table_impl;
class Weak_object;

///////////////////////////////////////////////////////////////////////////

class Partition_impl : public Entity_object_impl, public Partition {
 public:
  Partition_impl();

  Partition_impl(Table_impl *table);

  Partition_impl(Table_impl *parent, Partition_impl *partition);

  Partition_impl(const Partition_impl &src, Table_impl *parent);

  Partition_impl(const Partition_impl &src, Partition_impl *partition);

  ~Partition_impl() override;

 public:
  const Object_table &object_table() const override;

  bool validate() const override;

  bool restore_children(Open_dictionary_tables_ctx *otx) override;

  bool store_children(Open_dictionary_tables_ctx *otx) override;

  bool drop_children(Open_dictionary_tables_ctx *otx) const override;

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
  // Table.
  /////////////////////////////////////////////////////////////////////////

  const Table &table() const override;

  Table &table() override;

  /* non-virtual */ const Table_impl &table_impl() const { return *m_table; }

  /* non-virtual */ Table_impl &table_impl() { return *m_table; }

  /////////////////////////////////////////////////////////////////////////
  // Parent partition.
  /////////////////////////////////////////////////////////////////////////

  virtual const Partition *parent_partition() const { return m_parent; }

  virtual Partition *parent_partition() {
    return const_cast<dd::Partition *>(m_parent);
  }

  /////////////////////////////////////////////////////////////////////////
  // parent_partition_id
  /////////////////////////////////////////////////////////////////////////

  Object_id parent_partition_id() const override {
    return m_parent_partition_id;
  }

  void set_parent_partition_id(Object_id parent_partition_id) override {
    m_parent_partition_id = parent_partition_id;
  }

  /////////////////////////////////////////////////////////////////////////
  // number.
  /////////////////////////////////////////////////////////////////////////

  uint number() const override { return m_number; }

  void set_number(uint number) override { m_number = number; }

  /////////////////////////////////////////////////////////////////////////
  // description_utf8.
  /////////////////////////////////////////////////////////////////////////

  const String_type &description_utf8() const override {
    return m_description_utf8;
  }

  void set_description_utf8(const String_type &description_utf8) override {
    m_description_utf8 = description_utf8;
  }

  /////////////////////////////////////////////////////////////////////////
  // engine.
  /////////////////////////////////////////////////////////////////////////

  const String_type &engine() const override { return m_engine; }

  void set_engine(const String_type &engine) override { m_engine = engine; }

  /////////////////////////////////////////////////////////////////////////
  // comment.
  /////////////////////////////////////////////////////////////////////////

  const String_type &comment() const override { return m_comment; }

  void set_comment(const String_type &comment) override { m_comment = comment; }

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
  // se_private_id.
  /////////////////////////////////////////////////////////////////////////

  Object_id se_private_id() const override { return m_se_private_id; }

  void set_se_private_id(Object_id se_private_id) override {
    m_se_private_id = se_private_id;
  }

  /////////////////////////////////////////////////////////////////////////
  // Tablespace.
  /////////////////////////////////////////////////////////////////////////

  Object_id tablespace_id() const override { return m_tablespace_id; }

  void set_tablespace_id(Object_id tablespace_id) override {
    m_tablespace_id = tablespace_id;
  }

  /////////////////////////////////////////////////////////////////////////
  // Partition-value collection
  /////////////////////////////////////////////////////////////////////////

  Partition_value *add_value() override;

  const Partition_values &values() const override { return m_values; }

  /////////////////////////////////////////////////////////////////////////
  // Partition-index collection
  /////////////////////////////////////////////////////////////////////////

  Partition_index *add_index(Index *idx) override;

  const Partition_indexes &indexes() const override { return m_indexes; }

  /* purecov: begin deadcode */
  Partition_indexes *indexes() override { return &m_indexes; }
  /* purecov: end */

  /////////////////////////////////////////////////////////////////////////
  // Sub Partition collection.
  /////////////////////////////////////////////////////////////////////////

  Partition *add_subpartition() override;

  const Table::Partition_collection &subpartitions() const override {
    return m_subpartitions;
  }

  Table::Partition_collection *subpartitions() override {
    return &m_subpartitions;
  }

  const Partition *parent() const override { return m_parent; }
  void set_parent(const Partition *parent) override { m_parent = parent; }

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

 public:
  static Partition_impl *restore_item(Table_impl *table) {
    return new (std::nothrow) Partition_impl(table);
  }

  static Partition_impl *restore_item(Partition_impl *part) {
    Partition_impl *p =
        new (std::nothrow) Partition_impl(&part->table_impl(), part);
    p->set_parent(part);

    return p;
  }

  static Partition_impl *clone(const Partition_impl &other, Table_impl *table) {
    return new (std::nothrow) Partition_impl(other, table);
  }

  static Partition_impl *clone(const Partition_impl &other,
                               Partition_impl *part) {
    return new (std::nothrow) Partition_impl(other, part);
  }

 private:
  // Fields.

  Object_id m_parent_partition_id;
  uint m_number;
  Object_id m_se_private_id;

  String_type m_description_utf8;
  String_type m_engine;
  String_type m_comment;
  Properties_impl m_options;
  Properties_impl m_se_private_data;

  // References to tightly-coupled objects.

  Table_impl *m_table;

  const Partition *m_parent;

  Partition_values m_values;
  Partition_indexes m_indexes;
  Table::Partition_collection m_subpartitions;

  // References to loosely-coupled objects.

  Object_id m_tablespace_id;
};

///////////////////////////////////////////////////////////////////////////

/** Used to compare two partition elements. */
struct Partition_order_comparator {
  // TODO : do we really need this ordering now ?
  bool operator()(const dd::Partition *p1, const dd::Partition *p2) const {
    if (p1->parent_partition_id() == p2->parent_partition_id())
      return p1->number() < p2->number();
    return p1->parent_partition_id() < p2->parent_partition_id();
  }
};

}  // namespace dd

#endif  // DD__PARTITION_IMPL_INCLUDED
