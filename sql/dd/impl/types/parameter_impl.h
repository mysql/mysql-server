/* Copyright (c) 2016, 2024, Oracle and/or its affiliates.

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

#ifndef DD__PARAMETER_IMPL_INCLUDED
#define DD__PARAMETER_IMPL_INCLUDED

#include <assert.h>
#include <stddef.h>
#include <sys/types.h>
#include <memory>  // std::unique_ptr
#include <new>

#include "sql/dd/impl/properties_impl.h"  // dd::Properties_imp
#include "sql/dd/impl/raw/raw_record.h"
#include "sql/dd/impl/types/entity_object_impl.h"  // dd::Entity_object_impl
#include "sql/dd/impl/types/weak_object_impl.h"
#include "sql/dd/object_id.h"
#include "sql/dd/string_type.h"
#include "sql/dd/types/column.h"
#include "sql/dd/types/parameter.h"               // dd::Parameter
#include "sql/dd/types/parameter_type_element.h"  // IWYU pragma: keep

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Object_table;
class Open_dictionary_tables_ctx;
class Routine;
class Routine_impl;
class Weak_object;

///////////////////////////////////////////////////////////////////////////

class Parameter_impl : public Entity_object_impl, public Parameter {
 public:
  Parameter_impl();

  Parameter_impl(Routine_impl *routine);

  Parameter_impl(const Parameter_impl &src, Routine_impl *parent);

  ~Parameter_impl() override = default;

 public:
  const Object_table &object_table() const override;

  bool validate() const override;

  bool restore_children(Open_dictionary_tables_ctx *otx) override;

  bool store_children(Open_dictionary_tables_ctx *otx) override;

  bool drop_children(Open_dictionary_tables_ctx *otx) const override;

  bool store_attributes(Raw_record *r) override;

  bool restore_attributes(const Raw_record &r) override;

  void debug_print(String_type &outb) const override;

  void set_ordinal_position(uint ordinal_position) {
    m_ordinal_position = ordinal_position;
  }

 public:
  static void register_tables(Open_dictionary_tables_ctx *otx);

  /////////////////////////////////////////////////////////////////////////
  // Name is nullable in case of function return type.
  /////////////////////////////////////////////////////////////////////////

  void set_name_null(bool is_null) override { m_is_name_null = is_null; }

  bool is_name_null() const override { return m_is_name_null; }

  /////////////////////////////////////////////////////////////////////////
  // ordinal_position.
  /////////////////////////////////////////////////////////////////////////

  uint ordinal_position() const override { return m_ordinal_position; }

  /////////////////////////////////////////////////////////////////////////
  // parameter_mode.
  /////////////////////////////////////////////////////////////////////////

  enum_parameter_mode mode() const override { return m_parameter_mode; }

  void set_mode(enum_parameter_mode mode) override { m_parameter_mode = mode; }

  void set_parameter_mode_null(bool is_null) override {
    m_parameter_mode_null = is_null;
  }

  bool is_parameter_mode_null() const override { return m_parameter_mode_null; }

  /////////////////////////////////////////////////////////////////////////
  // parameter_mode.
  /////////////////////////////////////////////////////////////////////////

  enum_column_types data_type() const override { return m_data_type; }

  void set_data_type(enum_column_types type) override { m_data_type = type; }

  /////////////////////////////////////////////////////////////////////////
  // display type
  /////////////////////////////////////////////////////////////////////////

  const String_type &data_type_utf8() const override {
    return m_data_type_utf8;
  }

  void set_data_type_utf8(const String_type &data_type_utf8) override {
    m_data_type_utf8 = data_type_utf8;
  }

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
    m_numeric_precision_null = false;
    m_numeric_precision = numeric_precision;
  }

  virtual void set_numeric_precision_null(bool is_null) {
    m_numeric_precision_null = is_null;
  }

  virtual bool is_numeric_precision_null() const {
    return m_numeric_precision_null;
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

  virtual void set_datetime_precision_null(bool is_null) {
    m_datetime_precision_null = is_null;
  }

  virtual bool is_datetime_precision_null() const {
    return m_datetime_precision_null;
  }

  /////////////////////////////////////////////////////////////////////////
  // collation.
  /////////////////////////////////////////////////////////////////////////

  Object_id collation_id() const override { return m_collation_id; }

  void set_collation_id(Object_id collation_id) override {
    m_collation_id = collation_id;
  }

  /////////////////////////////////////////////////////////////////////////
  // Options.
  /////////////////////////////////////////////////////////////////////////

  const Properties &options() const override { return m_options; }

  Properties &options() override { return m_options; }

  bool set_options(const String_type &options_raw) override {
    return m_options.insert_values(options_raw);
  }

  /////////////////////////////////////////////////////////////////////////
  // routine.
  /////////////////////////////////////////////////////////////////////////

  const Routine &routine() const override;

  Routine &routine() override;

  /////////////////////////////////////////////////////////////////////////
  // Elements.
  /////////////////////////////////////////////////////////////////////////

  Parameter_type_element *add_element() override;

  const Parameter_type_element_collection &elements() const override {
    assert(data_type() == enum_column_types::ENUM ||
           data_type() == enum_column_types::SET);
    return m_elements;
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

 public:
  static Parameter_impl *restore_item(Routine_impl *routine) {
    return new (std::nothrow) Parameter_impl(routine);
  }

  static Parameter_impl *clone(const Parameter_impl &other,
                               Routine_impl *routine) {
    return new (std::nothrow) Parameter_impl(other, routine);
  }

 private:
  // Fields
  bool m_is_name_null;

  enum_parameter_mode m_parameter_mode;
  bool m_parameter_mode_null;
  enum_column_types m_data_type;
  String_type m_data_type_utf8;

  bool m_is_zerofill;
  bool m_is_unsigned;

  uint m_ordinal_position;
  size_t m_char_length;
  uint m_numeric_precision;
  bool m_numeric_precision_null;
  uint m_numeric_scale;
  bool m_numeric_scale_null;
  uint m_datetime_precision;
  bool m_datetime_precision_null;

  Parameter_type_element_collection m_elements;

  Properties_impl m_options;

  // References to other tightly coupled objects
  Routine_impl *m_routine;

  // References to loosely-coupled objects.

  Object_id m_collation_id;
};

///////////////////////////////////////////////////////////////////////////

}  // namespace dd

#endif  // DD__PARAMETER_IMPL_INCLUDED
