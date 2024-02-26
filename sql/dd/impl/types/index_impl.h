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

#ifndef DD__INDEX_IMPL_INCLUDED
#define DD__INDEX_IMPL_INCLUDED

#include <sys/types.h>
#include <memory>
#include <new>

#include "sql/dd/impl/properties_impl.h"           // Properties_impl
#include "sql/dd/impl/types/entity_object_impl.h"  // dd::Entity_object_impl
#include "sql/dd/impl/types/weak_object_impl.h"
#include "sql/dd/object_id.h"
#include "sql/dd/properties.h"
#include "sql/dd/sdi_fwd.h"
#include "sql/dd/string_type.h"
#include "sql/dd/types/index.h"          // dd::Index
#include "sql/dd/types/index_element.h"  // IWYU pragma: keep
#include "sql/strfunc.h"

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Column;
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

class Index_impl : public Entity_object_impl, public Index {
 public:
  Index_impl();

  Index_impl(Table_impl *table);

  Index_impl(const Index_impl &src, Table_impl *parent);

  ~Index_impl() override;

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

  void set_ordinal_position(uint ordinal_position) override {
    m_ordinal_position = ordinal_position;
  }

  uint ordinal_position() const override { return m_ordinal_position; }

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
  // is_generated
  /////////////////////////////////////////////////////////////////////////

  bool is_generated() const override { return m_is_generated; }

  void set_generated(bool generated) override { m_is_generated = generated; }

  /////////////////////////////////////////////////////////////////////////
  // is_hidden.
  /////////////////////////////////////////////////////////////////////////

  bool is_hidden() const override { return m_hidden; }

  void set_hidden(bool hidden) override { m_hidden = hidden; }

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
  // Tablespace.
  /////////////////////////////////////////////////////////////////////////

  Object_id tablespace_id() const override { return m_tablespace_id; }

  void set_tablespace_id(Object_id tablespace_id) override {
    m_tablespace_id = tablespace_id;
  }

  /////////////////////////////////////////////////////////////////////////
  // Engine.
  /////////////////////////////////////////////////////////////////////////

  const String_type &engine() const override { return m_engine; }

  void set_engine(const String_type &engine) override { m_engine = engine; }

  /////////////////////////////////////////////////////////////////////////
  // Index type.
  /////////////////////////////////////////////////////////////////////////

  Index::enum_index_type type() const override { return m_type; }

  void set_type(Index::enum_index_type type) override { m_type = type; }

  /////////////////////////////////////////////////////////////////////////
  // Index algorithm.
  /////////////////////////////////////////////////////////////////////////

  Index::enum_index_algorithm algorithm() const override { return m_algorithm; }

  void set_algorithm(Index::enum_index_algorithm algorithm) override {
    m_algorithm = algorithm;
  }

  bool is_algorithm_explicit() const override {
    return m_is_algorithm_explicit;
  }

  void set_algorithm_explicit(bool alg_expl) override {
    m_is_algorithm_explicit = alg_expl;
  }

  bool is_visible() const override { return m_is_visible; }

  void set_visible(bool is_visible) override { m_is_visible = is_visible; }

  LEX_CSTRING engine_attribute() const override {
    return lex_cstring_handle(m_engine_attribute);
  }
  void set_engine_attribute(LEX_CSTRING a) override {
    m_engine_attribute.assign(a.str, a.length);
  }
  LEX_CSTRING secondary_engine_attribute() const override {
    return lex_cstring_handle(m_secondary_engine_attribute);
  }
  void set_secondary_engine_attribute(LEX_CSTRING a) override {
    m_secondary_engine_attribute.assign(a.str, a.length);
  }

  /////////////////////////////////////////////////////////////////////////
  // Index-element collection
  /////////////////////////////////////////////////////////////////////////

  Index_element *add_element(Column *c) override;

  const Index_elements &elements() const override { return m_elements; }

  bool is_candidate_key() const override;

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
  static Index_impl *restore_item(Table_impl *table) {
    return new (std::nothrow) Index_impl(table);
  }

  static Index_impl *clone(const Index_impl &other, Table_impl *table) {
    return new (std::nothrow) Index_impl(other, table);
  }

 private:
  // Fields.

  bool m_hidden;
  bool m_is_generated;

  uint m_ordinal_position;

  String_type m_comment;
  Properties_impl m_options;
  Properties_impl m_se_private_data;

  Index::enum_index_type m_type;
  Index::enum_index_algorithm m_algorithm;
  bool m_is_algorithm_explicit;
  bool m_is_visible;

  String_type m_engine;

  String_type m_engine_attribute;
  String_type m_secondary_engine_attribute;

  // References to tightly-coupled objects.

  Table_impl *m_table;

  Index_elements m_elements;

  // References to loosely-coupled objects.

  Object_id m_tablespace_id;
};

///////////////////////////////////////////////////////////////////////////

}  // namespace dd

#endif  // DD__INDEX_IMPL_INCLUDED
