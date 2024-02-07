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

#ifndef DD__FUNCTION_IMPL_INCLUDED
#define DD__FUNCTION_IMPL_INCLUDED

#include <stddef.h>
#include <sys/types.h>
#include <new>

#include "my_inttypes.h"
#include "sql/dd/impl/raw/raw_record.h"
#include "sql/dd/impl/types/entity_object_impl.h"
#include "sql/dd/impl/types/routine_impl.h"  // dd::Routine_impl
#include "sql/dd/impl/types/weak_object_impl.h"
#include "sql/dd/object_id.h"
#include "sql/dd/string_type.h"
#include "sql/dd/types/column.h"
#include "sql/dd/types/function.h"  // dd::Function
#include "sql/dd/types/routine.h"
#include "sql/dd/types/view.h"

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Open_dictionary_tables_ctx;
class Parameter;
class Weak_object;
class Object_table;

class Function_impl : public Routine_impl, public Function {
 public:
  Function_impl();

  ~Function_impl() override = default;

  bool update_routine_name_key(Name_key *key, Object_id schema_id,
                               const String_type &name) const override;

 public:
  bool validate() const override;

  bool restore_attributes(const Raw_record &r) override;

  bool store_attributes(Raw_record *r) override;

  void debug_print(String_type &outb) const override;

 public:
  /////////////////////////////////////////////////////////////////////////
  // result data type.
  /////////////////////////////////////////////////////////////////////////

  enum_column_types result_data_type() const override {
    return m_result_data_type;
  }

  void set_result_data_type(enum_column_types result_data_type) override {
    m_result_data_type = result_data_type;
  }

  bool is_result_data_type_null() const override {
    return m_result_data_type_null;
  }

  void set_result_data_type_null(bool is_null) override {
    m_result_data_type_null = is_null;
  }

  /////////////////////////////////////////////////////////////////////////
  // Result display type
  /////////////////////////////////////////////////////////////////////////

  const String_type &result_data_type_utf8() const override {
    return m_result_data_type_utf8;
  }

  void set_result_data_type_utf8(
      const String_type &result_data_type_utf8) override {
    m_result_data_type_utf8 = result_data_type_utf8;
  }

  /////////////////////////////////////////////////////////////////////////
  // result_is_zerofill.
  /////////////////////////////////////////////////////////////////////////

  bool result_is_zerofill() const override { return m_result_is_zerofill; }

  void set_result_zerofill(bool zerofill) override {
    m_result_is_zerofill = zerofill;
  }

  /////////////////////////////////////////////////////////////////////////
  // result_is_unsigned.
  /////////////////////////////////////////////////////////////////////////

  bool result_is_unsigned() const override { return m_result_is_unsigned; }

  void set_result_unsigned(bool unsigned_flag) override {
    m_result_is_unsigned = unsigned_flag;
  }

  /////////////////////////////////////////////////////////////////////////
  // result_char_length.
  /////////////////////////////////////////////////////////////////////////

  size_t result_char_length() const override { return m_result_char_length; }

  void set_result_char_length(size_t char_length) override {
    m_result_char_length = char_length;
  }

  /////////////////////////////////////////////////////////////////////////
  // result_numeric_precision.
  /////////////////////////////////////////////////////////////////////////

  uint result_numeric_precision() const override {
    return m_result_numeric_precision;
  }

  void set_result_numeric_precision(uint result_numeric_precision) override {
    m_result_numeric_precision_null = false;
    m_result_numeric_precision = result_numeric_precision;
  }

  virtual void set_result_numeric_precision_null(bool is_null) {
    m_result_numeric_precision_null = is_null;
  }

  virtual bool is_result_numeric_precision_null() const {
    return m_result_numeric_precision_null;
  }

  /////////////////////////////////////////////////////////////////////////
  // result_numeric_scale.
  /////////////////////////////////////////////////////////////////////////

  uint result_numeric_scale() const override { return m_result_numeric_scale; }

  void set_result_numeric_scale(uint result_numeric_scale) override {
    m_result_numeric_scale_null = false;
    m_result_numeric_scale = result_numeric_scale;
  }

  void set_result_numeric_scale_null(bool is_null) override {
    m_result_numeric_scale_null = is_null;
  }

  bool is_result_numeric_scale_null() const override {
    return m_result_numeric_scale_null;
  }

  /////////////////////////////////////////////////////////////////////////
  // result_datetime_precision.
  /////////////////////////////////////////////////////////////////////////

  uint result_datetime_precision() const override {
    return m_result_datetime_precision;
  }

  void set_result_datetime_precision(uint result_datetime_precision) override {
    m_result_datetime_precision_null = false;
    m_result_datetime_precision = result_datetime_precision;
  }

  virtual void set_result_datetime_precision_null(bool is_null) {
    m_result_datetime_precision_null = is_null;
  }

  virtual bool is_result_datetime_precision_null() const {
    return m_result_datetime_precision_null;
  }

  /////////////////////////////////////////////////////////////////////////
  // result_collation.
  /////////////////////////////////////////////////////////////////////////

  Object_id result_collation_id() const override {
    return m_result_collation_id;
  }

  void set_result_collation_id(Object_id result_collation_id) override {
    m_result_collation_id = result_collation_id;
  }

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
  const Object_table &object_table() const override {
    return Routine_impl::object_table();
  }
  Object_id schema_id() const override { return Routine_impl::schema_id(); }
  void set_schema_id(Object_id schema_id) override {
    Routine_impl::set_schema_id(schema_id);
  }
  enum_routine_type type() const override { return Routine_impl::type(); }
  const String_type &definition() const override {
    return Routine_impl::definition();
  }
  void set_definition(const String_type &definition) override {
    Routine_impl::set_definition(definition);
  }
  const String_type &definition_utf8() const override {
    return Routine_impl::definition_utf8();
  }
  void set_definition_utf8(const String_type &definition_utf8) override {
    Routine_impl::set_definition_utf8(definition_utf8);
  }
  const String_type &parameter_str() const override {
    return Routine_impl::parameter_str();
  }
  void set_parameter_str(const String_type &parameter_str) override {
    Routine_impl::set_parameter_str(parameter_str);
  }
  bool is_deterministic() const override {
    return Routine_impl::is_deterministic();
  }
  void set_deterministic(bool deterministic) override {
    Routine_impl::set_deterministic(deterministic);
  }
  enum_sql_data_access sql_data_access() const override {
    return Routine_impl::sql_data_access();
  }
  void set_sql_data_access(enum_sql_data_access sda) override {
    Routine_impl::set_sql_data_access(sda);
  }
  const String_type &external_language() const override {
    return Routine_impl::external_language();
  }
  void set_external_language(const String_type &el) override {
    Routine_impl::set_external_language(el);
  }
  View::enum_security_type security_type() const override {
    return Routine_impl::security_type();
  }
  void set_security_type(View::enum_security_type security_type) override {
    Routine_impl::set_security_type(security_type);
  }
  ulonglong sql_mode() const override { return Routine_impl::sql_mode(); }
  void set_sql_mode(ulonglong sm) override { Routine_impl::set_sql_mode(sm); }
  const String_type &definer_user() const override {
    return Routine_impl::definer_user();
  }
  const String_type &definer_host() const override {
    return Routine_impl::definer_host();
  }
  void set_definer(const String_type &username,
                   const String_type &hostname) override {
    Routine_impl::set_definer(username, hostname);
  }
  Object_id client_collation_id() const override {
    return Routine_impl::client_collation_id();
  }
  void set_client_collation_id(Object_id client_collation_id) override {
    Routine_impl::set_client_collation_id(client_collation_id);
  }
  Object_id connection_collation_id() const override {
    return Routine_impl::connection_collation_id();
  }
  void set_connection_collation_id(Object_id connection_collation_id) override {
    Routine_impl::set_connection_collation_id(connection_collation_id);
  }
  Object_id schema_collation_id() const override {
    return Routine_impl::schema_collation_id();
  }
  void set_schema_collation_id(Object_id schema_collation_id) override {
    Routine_impl::set_schema_collation_id(schema_collation_id);
  }
  ulonglong created(bool convert_time) const override {
    return Routine_impl::created(convert_time);
  }
  void set_created(ulonglong created) override {
    Routine_impl::set_created(created);
  }
  ulonglong last_altered(bool convert_time) const override {
    return Routine_impl::last_altered(convert_time);
  }
  void set_last_altered(ulonglong last_altered) override {
    Routine_impl::set_last_altered(last_altered);
  }
  const String_type &comment() const override {
    return Routine_impl::comment();
  }
  void set_comment(const String_type &comment) override {
    Routine_impl::set_comment(comment);
  }
  Parameter *add_parameter() override { return Routine_impl::add_parameter(); }
  const Parameter_collection &parameters() const override {
    return Routine_impl::parameters();
  }
  bool update_name_key(Name_key *key) const override {
    return Function::update_name_key(key);
  }

 private:
  enum_column_types m_result_data_type;
  String_type m_result_data_type_utf8;

  bool m_result_data_type_null;
  bool m_result_is_zerofill;
  bool m_result_is_unsigned;

  bool m_result_numeric_precision_null;
  bool m_result_numeric_scale_null;
  bool m_result_datetime_precision_null;

  uint m_result_numeric_precision;
  uint m_result_numeric_scale;
  uint m_result_datetime_precision;

  size_t m_result_char_length;

  Object_id m_result_collation_id;

  Function_impl(const Function_impl &src);
  Function_impl *clone() const override { return new Function_impl(*this); }

  // N.B.: returning dd::Function from this function confuses MSVC compiler
  // thanks to diamond inheritance.
  Function_impl *clone_dropped_object_placeholder() const override {
    Function_impl *placeholder = new Function_impl();
    placeholder->set_id(id());
    placeholder->set_schema_id(schema_id());
    placeholder->set_name(name());
    return placeholder;
  }
};

///////////////////////////////////////////////////////////////////////////

}  // namespace dd

#endif  // DD__FUNCTION_IMPL_INCLUDED
