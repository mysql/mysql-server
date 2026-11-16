/* Copyright (c) 2026, Oracle and/or its affiliates.

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

#ifndef DD__UDT_PARAMETER_IMPL_INCLUDED
#define DD__UDT_PARAMETER_IMPL_INCLUDED

#include <assert.h>
#include <stdio.h>

#include <cstddef>  // std::nullptr_t
#include <memory>   // std::unique_ptr
#include <new>
#include <optional>

#include "my_inttypes.h"
#include "sql/dd/impl/types/entity_object_impl.h"  // dd::Entity_object_impl
#include "sql/dd/impl/types/weak_object_impl.h"
#include "sql/dd/object_id.h"
#include "sql/dd/sdi_fwd.h"
#include "sql/dd/string_type.h"
#include "sql/dd/types/udt_method.h"     // dd:UDT_Method
#include "sql/dd/types/udt_parameter.h"  // dd:UDT_Parameter
#include "sql/dd/types/weak_object.h"
#include "sql/sql_time.h"  // gmt_time_to_local_time

class THD;

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Open_dictionary_tables_ctx;
class Raw_record;
class Sdi_rcontext;
class Sdi_wcontext;
class Object_table;

///////////////////////////////////////////////////////////////////////////

class UDT_Parameter_impl : public Entity_object_impl, public UDT_Parameter {
 public:
  UDT_Parameter_impl();

  ~UDT_Parameter_impl() override = default;

 public:
  const Object_table &object_table() const override;

  bool validate() const override;

  bool store_attributes(Raw_record *r) override;

  bool restore_attributes(const Raw_record &r) override;

  void serialize(Sdi_wcontext *wctx, Sdi_writer *w) const;

  bool deserialize(Sdi_rcontext *rctx, const RJ_Value &val);

 public:
  static void register_tables(Open_dictionary_tables_ctx *otx);

  /////////////////////////////////////////////////////////////////////////
  // type.
  /////////////////////////////////////////////////////////////////////////

  Object_id method_id() const override { return m_method_id; }

  void set_method_id(Object_id method_id) override { m_method_id = method_id; }

  /////////////////////////////////////////////////////////////////////////
  // ordinal_position.
  /////////////////////////////////////////////////////////////////////////

  uint ordinal_position() const override { return m_ordinal_position; }
  void set_ordinal_position(uint ordinal_position) override {
    m_ordinal_position = ordinal_position;
  }

  /////////////////////////////////////////////////////////////////////////
  // mode
  /////////////////////////////////////////////////////////////////////////

  enum_parameter_mode mode() const override { return m_parameter_mode; }
  void set_mode(enum_parameter_mode mode) override { m_parameter_mode = mode; }

  /////////////////////////////////////////////////////////////////////////
  // data type.
  /////////////////////////////////////////////////////////////////////////

  enum_column_types data_type() const override { return m_data_type; }
  void set_data_type(enum_column_types type) override { m_data_type = type; }

  /////////////////////////////////////////////////////////////////////////
  // is_unsigned.
  /////////////////////////////////////////////////////////////////////////

  bool is_unsigned() const override { return m_is_unsigned; }
  void set_is_unsigned(bool is_unsigned) override {
    m_is_unsigned = is_unsigned;
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
  // is_result.
  /////////////////////////////////////////////////////////////////////////

  bool is_result() const override { return m_is_result; }
  void set_is_result(bool is_result) override { m_is_result = is_result; }

  /////////////////////////////////////////////////////////////////////////
  // created
  /////////////////////////////////////////////////////////////////////////

  ulonglong created(bool convert_time) const override {
    return convert_time ? gmt_time_to_local_time(m_created) : m_created;
  }

  void set_created(ulonglong created) override { m_created = created; }

  /////////////////////////////////////////////////////////////////////////
  // last_altered
  /////////////////////////////////////////////////////////////////////////

  ulonglong last_altered(bool convert_time) const override {
    return convert_time ? gmt_time_to_local_time(m_last_altered)
                        : m_last_altered;
  }

  void set_last_altered(ulonglong last_altered) override {
    m_last_altered = last_altered;
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

 public:
  void debug_print(String_type &outb) const override {
    char outbuf[1024];
    sprintf(outbuf,
            "UDT_Parameter OBJECT: id= {OID: %lld}, "
            "name= %s, m_created= %llu, m_last_altered= %llu",
            id(), name().c_str(), m_created, m_last_altered);
    outb = String_type(outbuf);
  }

 private:
  // Fields

  Object_id m_method_id;
  enum_column_types m_data_type;
  enum_parameter_mode m_parameter_mode;
  uint m_ordinal_position;
  bool m_is_result;

  bool m_is_unsigned;
  size_t m_char_length;
  uint m_numeric_precision;
  uint m_numeric_scale;
  bool m_numeric_scale_null;
  uint m_datetime_precision;
  uint m_datetime_precision_null;

  Object_id m_collation_id;
  bool m_is_explicit_collation;

  ulonglong m_created;
  ulonglong m_last_altered;

  UDT_Parameter *clone() const override {
    return new UDT_Parameter_impl(*this);
  }

  UDT_Parameter *clone_dropped_object_placeholder() const override {
    /*
      Even though we don't drop SRSes en masse we still create slimmed
      down version for consistency sake.
    */
    UDT_Parameter_impl *placeholder = new UDT_Parameter_impl();
    placeholder->set_id(id());
    placeholder->set_method_id(method_id());
    placeholder->set_name(name());
    return placeholder;
  }
};

///////////////////////////////////////////////////////////////////////////

}  // namespace dd

#endif  // DD__UDT_TYPE_PARAMETER_INCLUDED
