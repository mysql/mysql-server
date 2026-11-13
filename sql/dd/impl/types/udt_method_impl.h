/* Copyright (c) 2016, 2026, Oracle and/or its affiliates.

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

#ifndef DD__UDT_METHOD_IMPL_INCLUDED
#define DD__UDT_METHOD_IMPL_INCLUDED

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
#include "sql/dd/types/udt_method.h"  // dd:UDT_Method
#include "sql/dd/types/udt_type.h"    // dd:UDT_Type
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

class UDT_Method_impl : public Entity_object_impl, public UDT_Method {
 public:
  UDT_Method_impl();

  ~UDT_Method_impl() override = default;

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

  Object_id type_id() const override { return m_type_id; }

  void set_type_id(Object_id type_id) override { m_type_id = type_id; }

  /////////////////////////////////////////////////////////////////////////
  // Column specific name.
  /////////////////////////////////////////////////////////////////////////

  const String_type &specific_name() const override { return m_specific_name; }

  void set_specific_name(const String_type &specific_name) override {
    m_specific_name = specific_name;
  }

  /////////////////////////////////////////////////////////////////////////
  // is_static.
  /////////////////////////////////////////////////////////////////////////

  bool is_static() const override { return m_is_static; }
  void set_is_static(bool is_static) override { m_is_static = is_static; }

  /////////////////////////////////////////////////////////////////////////
  // is_overriding.
  /////////////////////////////////////////////////////////////////////////

  bool is_overriding() const override { return m_is_overriding; }
  void set_is_overriding(bool is_overriding) override {
    m_is_overriding = is_overriding;
  }

  /////////////////////////////////////////////////////////////////////////
  // is_constructor.
  /////////////////////////////////////////////////////////////////////////

  bool is_constructor() const override { return m_is_constructor; }
  void set_is_constructor(bool is_constructor) override {
    m_is_constructor = is_constructor;
  }

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

  size_t char_max_length() const override { return m_char_max_length; }
  void set_char_max_length(size_t char_max_length) override {
    m_char_max_length = char_max_length;
  }

  size_t char_octet_length() const override { return m_char_octet_length; }
  void set_char_octet_length(size_t char_octet_length) override {
    m_char_octet_length = char_octet_length;
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
  // return_type.
  /////////////////////////////////////////////////////////////////////////

  Object_id return_type_id() const override { return m_return_type_id; }
  void set_return_type_id(Object_id return_type_id) override {
    m_return_type_id = return_type_id;
  }

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
            "UDT_Method OBJECT: id= {OID: %lld}, "
            "name= %s, m_created= %llu, m_last_altered= %llu",
            id(), name().c_str(), m_created, m_last_altered);
    outb = String_type(outbuf);
  }

 private:
  // Fields

  Object_id m_type_id;
  String_type m_specific_name;
  bool m_is_static;
  bool m_is_overriding;
  bool m_is_constructor;

  bool m_is_unsigned;
  size_t m_char_max_length;
  size_t m_char_octet_length;
  uint m_numeric_precision;
  uint m_numeric_scale;
  bool m_numeric_scale_null;
  uint m_datetime_precision;
  uint m_datetime_precision_null;

  Object_id m_return_type_id;

  Object_id m_collation_id;
  bool m_is_explicit_collation;

  ulonglong m_created;
  ulonglong m_last_altered;

  UDT_Method *clone() const override { return new UDT_Method_impl(*this); }

  UDT_Method *clone_dropped_object_placeholder() const override {
    /*
      Even though we don't drop SRSes en masse we still create slimmed
      down version for consistency sake.
    */
    UDT_Method_impl *placeholder = new UDT_Method_impl();
    placeholder->set_id(id());
    placeholder->set_type_id(type_id());
    placeholder->set_name(name());
    return placeholder;
  }
};

///////////////////////////////////////////////////////////////////////////

}  // namespace dd

#endif  // DD__UDT_TYPE_METHOD_INCLUDED
