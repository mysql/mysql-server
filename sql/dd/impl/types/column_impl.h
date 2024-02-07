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

#ifndef DD__COLUMN_IMPL_INCLUDED
#define DD__COLUMN_IMPL_INCLUDED

#include <assert.h>
#include <stddef.h>
#include <sys/types.h>
#include <memory>  // std::unique_ptr
#include <new>
#include <optional>

#include "sql/dd/impl/properties_impl.h"           // Properties_impl
#include "sql/dd/impl/types/entity_object_impl.h"  // dd::Entity_object_impl
#include "sql/dd/impl/types/weak_object_impl.h"
#include "sql/dd/object_id.h"
#include "sql/dd/properties.h"
#include "sql/dd/sdi_fwd.h"
#include "sql/dd/string_type.h"
#include "sql/dd/types/column.h"               // dd::Column
#include "sql/dd/types/column_type_element.h"  // IWYU pragma: keep
#include "sql/gis/srid.h"                      // gis::srid_t
#include "sql/strfunc.h"

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Abstract_table;
class Abstract_table_impl;
class Object_table;
class Open_dictionary_tables_ctx;
class Properties;
class Raw_record;
class Sdi_rcontext;
class Sdi_wcontext;
class Weak_object;

///////////////////////////////////////////////////////////////////////////

class Column_impl : public Entity_object_impl, public Column {
 public:
  Column_impl();

  Column_impl(Abstract_table_impl *table);

  Column_impl(const Column_impl &src, Abstract_table_impl *parent);

  ~Column_impl() override;

 public:
  const Object_table &object_table() const override;

  static void register_tables(Open_dictionary_tables_ctx *otx);

  bool validate() const override;

  bool restore_children(Open_dictionary_tables_ctx *otx) override;

  bool store_children(Open_dictionary_tables_ctx *otx) override;

  bool drop_children(Open_dictionary_tables_ctx *otx) const override;

  bool restore_attributes(const Raw_record &r) override;

  bool store_attributes(Raw_record *r) override;

  void serialize(Sdi_wcontext *wctx, Sdi_writer *w) const override;

  bool deserialize(Sdi_rcontext *rctx, const RJ_Value &val) override;

  void debug_print(String_type &outb) const override;

  void set_ordinal_position(uint ordinal_position) {
    m_ordinal_position = ordinal_position;
  }

 public:
  /////////////////////////////////////////////////////////////////////////
  // table.
  /////////////////////////////////////////////////////////////////////////

  const Abstract_table &table() const override;

  Abstract_table &table() override;

  /////////////////////////////////////////////////////////////////////////
  // type.
  /////////////////////////////////////////////////////////////////////////

  enum_column_types type() const override { return m_type; }

  void set_type(enum_column_types type) override { m_type = type; }

  /////////////////////////////////////////////////////////////////////////
  // collation.
  /////////////////////////////////////////////////////////////////////////

  Object_id collation_id() const override { return m_collation_id; }

  void set_collation_id(Object_id collation_id) override {
    m_collation_id = collation_id;
  }

  void set_is_explicit_collation(bool is_explicit_collation) override {
    m_is_explicit_collation = is_explicit_collation;
  }

  bool is_explicit_collation() const override {
    return m_is_explicit_collation;
  }

  /////////////////////////////////////////////////////////////////////////
  // nullable.
  /////////////////////////////////////////////////////////////////////////

  bool is_nullable() const override { return m_is_nullable; }

  void set_nullable(bool nullable) override { m_is_nullable = nullable; }

  /////////////////////////////////////////////////////////////////////////
  // is_zerofill.
  /////////////////////////////////////////////////////////////////////////

  bool is_zerofill() const override { return m_is_zerofill; }

  void set_zerofill(bool zerofill) override { m_is_zerofill = zerofill; }

  /////////////////////////////////////////////////////////////////////////
  // is_unsigned.
  /////////////////////////////////////////////////////////////////////////

  bool is_unsigned() const override { return m_is_unsigned; }

  void set_unsigned(bool unsigned_flag) override {
    m_is_unsigned = unsigned_flag;
  }

  /////////////////////////////////////////////////////////////////////////
  // auto increment.
  /////////////////////////////////////////////////////////////////////////

  bool is_auto_increment() const override { return m_is_auto_increment; }

  void set_auto_increment(bool auto_increment) override {
    m_is_auto_increment = auto_increment;
  }

  /////////////////////////////////////////////////////////////////////////
  // ordinal_position.
  /////////////////////////////////////////////////////////////////////////

  uint ordinal_position() const override { return m_ordinal_position; }

  /////////////////////////////////////////////////////////////////////////
  // char_length.
  /////////////////////////////////////////////////////////////////////////

  size_t char_length() const override { return m_char_length; }

  void set_char_length(size_t char_length) override {
    m_char_length = char_length;
  }

  /////////////////////////////////////////////////////////////////////////
  // numeric_precision.
  /////////////////////////////////////////////////////////////////////////

  uint numeric_precision() const override { return m_numeric_precision; }

  void set_numeric_precision(uint numeric_precision) override {
    m_numeric_precision = numeric_precision;
  }

  /////////////////////////////////////////////////////////////////////////
  // numeric_scale.
  /////////////////////////////////////////////////////////////////////////

  uint numeric_scale() const override { return m_numeric_scale; }

  void set_numeric_scale(uint numeric_scale) override {
    m_numeric_scale_null = false;
    m_numeric_scale = numeric_scale;
  }

  void set_numeric_scale_null(bool is_null) override {
    m_numeric_scale_null = is_null;
  }

  bool is_numeric_scale_null() const override { return m_numeric_scale_null; }

  /////////////////////////////////////////////////////////////////////////
  // datetime_precision.
  /////////////////////////////////////////////////////////////////////////

  uint datetime_precision() const override { return m_datetime_precision; }

  void set_datetime_precision(uint datetime_precision) override {
    m_datetime_precision_null = false;
    m_datetime_precision = datetime_precision;
  }

  void set_datetime_precision_null(bool is_null) override {
    m_datetime_precision_null = is_null;
  }

  bool is_datetime_precision_null() const override {
    return m_datetime_precision_null;
  }

  /////////////////////////////////////////////////////////////////////////
  // has_no_default.
  /////////////////////////////////////////////////////////////////////////

  bool has_no_default() const override { return m_has_no_default; }

  void set_has_no_default(bool has_no_default) override {
    m_has_no_default = has_no_default;
  }

  /////////////////////////////////////////////////////////////////////////
  // default_value (binary).
  /////////////////////////////////////////////////////////////////////////

  const String_type &default_value() const override { return m_default_value; }

  void set_default_value(const String_type &default_value) override {
    m_default_value_null = false;
    m_default_value = default_value;
  }

  void set_default_value_null(bool is_null) override {
    m_default_value_null = is_null;
  }

  bool is_default_value_null() const override { return m_default_value_null; }

  /////////////////////////////////////////////////////////////////////////
  // default_value_utf8
  /////////////////////////////////////////////////////////////////////////

  const String_type &default_value_utf8() const override {
    return m_default_value_utf8;
  }

  void set_default_value_utf8(const String_type &default_value_utf8) override {
    m_default_value_utf8_null = false;
    m_default_value_utf8 = default_value_utf8;
  }

  void set_default_value_utf8_null(bool is_null) override {
    m_default_value_utf8_null = is_null;
  }

  /* purecov: begin deadcode */
  bool is_default_value_utf8_null() const override {
    return m_default_value_utf8_null;
  }
  /* purecov: end */

  /////////////////////////////////////////////////////////////////////////
  // is virtual ?
  /////////////////////////////////////////////////////////////////////////

  bool is_virtual() const override { return m_is_virtual; }

  void set_virtual(bool is_virtual) override { m_is_virtual = is_virtual; }

  /////////////////////////////////////////////////////////////////////////
  // generation_expression (binary).
  /////////////////////////////////////////////////////////////////////////

  const String_type &generation_expression() const override {
    return m_generation_expression;
  }

  void set_generation_expression(
      const String_type &generation_expression) override {
    m_generation_expression = generation_expression;
  }

  bool is_generation_expression_null() const override {
    return m_generation_expression.empty();
  }

  /////////////////////////////////////////////////////////////////////////
  // generation_expression_utf8
  /////////////////////////////////////////////////////////////////////////

  const String_type &generation_expression_utf8() const override {
    return m_generation_expression_utf8;
  }

  void set_generation_expression_utf8(
      const String_type &generation_expression_utf8) override {
    m_generation_expression_utf8 = generation_expression_utf8;
  }

  bool is_generation_expression_utf8_null() const override {
    return m_generation_expression_utf8.empty();
  }

  /////////////////////////////////////////////////////////////////////////
  // default_option.
  /////////////////////////////////////////////////////////////////////////

  const String_type &default_option() const override {
    return m_default_option;
  }

  void set_default_option(const String_type &default_option) override {
    m_default_option = default_option;
  }

  /////////////////////////////////////////////////////////////////////////
  // update_option.
  /////////////////////////////////////////////////////////////////////////

  const String_type &update_option() const override { return m_update_option; }

  void set_update_option(const String_type &update_option) override {
    m_update_option = update_option;
  }

  /////////////////////////////////////////////////////////////////////////
  // Comment.
  /////////////////////////////////////////////////////////////////////////

  const String_type &comment() const override { return m_comment; }

  void set_comment(const String_type &comment) override { m_comment = comment; }

  /////////////////////////////////////////////////////////////////////////
  // Hidden.
  /////////////////////////////////////////////////////////////////////////

  enum_hidden_type hidden() const override { return m_hidden; }

  void set_hidden(enum_hidden_type hidden) override { m_hidden = hidden; }

  /////////////////////////////////////////////////////////////////////////
  // Options.
  /////////////////////////////////////////////////////////////////////////

  const Properties &options() const override { return m_options; }

  Properties &options() override { return m_options; }

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

  bool set_se_private_data(const Properties &se_private_data) override {
    return m_se_private_data.insert_values(se_private_data);
  }

  bool set_se_private_data(const String_type &se_private_data_raw) override {
    return m_se_private_data.insert_values(se_private_data_raw);
  }

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
  // Column key type.
  /////////////////////////////////////////////////////////////////////////

  void set_column_key(enum_column_key column_key) override {
    m_column_key = column_key;
  }

  enum_column_key column_key() const override { return m_column_key; }

  /////////////////////////////////////////////////////////////////////////
  // Spatial reference system ID
  /////////////////////////////////////////////////////////////////////////
  void set_srs_id(std::optional<gis::srid_t> srs_id) override {
    m_srs_id = srs_id;
  }

  std::optional<gis::srid_t> srs_id() const override { return m_srs_id; }

  /////////////////////////////////////////////////////////////////////////
  // Elements.
  /////////////////////////////////////////////////////////////////////////

  Column_type_element *add_element() override;

  const Column_type_element_collection &elements() const override {
    assert(type() == enum_column_types::ENUM ||
           type() == enum_column_types::SET);
    return m_elements;
  }

  /////////////////////////////////////////////////////////////////////////
  // Column display type
  /////////////////////////////////////////////////////////////////////////

  const String_type &column_type_utf8() const override {
    return m_column_type_utf8;
  }

  void set_column_type_utf8(const String_type &column_type_utf8) override {
    m_column_type_utf8 = column_type_utf8;
  }

  size_t elements_count() const override { return m_elements.size(); }

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

  bool is_array() const override {
    // Is this a typed array field?
    if (options().exists("is_array")) {
      bool is_array;
      if (!options().get("is_array", &is_array)) return is_array;
    }

    return false;
  }

 public:
  static Column_impl *restore_item(Abstract_table_impl *table) {
    return new (std::nothrow) Column_impl(table);
  }

  static Column_impl *clone(const Column_impl &other,
                            Abstract_table_impl *table) {
    return new (std::nothrow) Column_impl(other, table);
  }

  Column_impl *clone(Abstract_table_impl *parent) const {
    return new Column_impl(*this, parent);
  }

 private:
  // Fields.

  enum_column_types m_type;

  bool m_is_nullable;
  bool m_is_zerofill;
  bool m_is_unsigned;
  bool m_is_auto_increment;
  bool m_is_virtual;
  enum_hidden_type m_hidden;

  uint m_ordinal_position;
  size_t m_char_length;
  uint m_numeric_precision;
  uint m_numeric_scale;
  bool m_numeric_scale_null;
  uint m_datetime_precision;
  uint m_datetime_precision_null;

  bool m_has_no_default;

  bool m_default_value_null;
  String_type m_default_value;
  bool m_default_value_utf8_null;
  String_type m_default_value_utf8;

  String_type m_default_option;
  String_type m_update_option;
  String_type m_comment;

  String_type m_generation_expression;
  String_type m_generation_expression_utf8;

  Properties_impl m_options;
  Properties_impl m_se_private_data;

  // Se-specific json attributes
  String_type m_engine_attribute;
  String_type m_secondary_engine_attribute;

  // References to tightly-coupled objects.

  Abstract_table_impl *m_table;

  Column_type_element_collection m_elements;

  String_type m_column_type_utf8;

  // References to loosely-coupled objects.

  Object_id m_collation_id;
  bool m_is_explicit_collation;

  enum_column_key m_column_key;

  std::optional<gis::srid_t> m_srs_id;
};

///////////////////////////////////////////////////////////////////////////

}  // namespace dd

#endif  // DD__COLUMN_IMPL_INCLUDED
