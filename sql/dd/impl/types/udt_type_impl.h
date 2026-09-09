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

#ifndef DD__UDT_TYPE_IMPL_INCLUDED
#define DD__UDT_TYPE_IMPL_INCLUDED

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
#include "sql/dd/types/udt_type.h"  // dd:UDT_Type
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

class UDT_Type_impl : public Entity_object_impl, public UDT_Type {
 public:
  UDT_Type_impl() : m_created(0), m_last_altered(0) {}

 private:
  UDT_Type_impl(const UDT_Type_impl &other)
      : Weak_object(other),
        Entity_object_impl(other),
        m_created(other.m_created),
        m_last_altered(other.m_last_altered),
        m_schema_id(other.m_schema_id) {}

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
  // schema.
  /////////////////////////////////////////////////////////////////////////

  Object_id schema_id() const override { return m_schema_id; }

  void set_schema_id(Object_id schema_id) override { m_schema_id = schema_id; }

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
            "UDT_Type OBJECT: id= {OID: %lld}, "
            "name= %s, m_created= %llu, m_last_altered= %llu",
            id(), name().c_str(), m_created, m_last_altered);
    outb = String_type(outbuf);
  }

 private:
  // Fields
  ulonglong m_created;
  ulonglong m_last_altered;

  Object_id m_schema_id;

  UDT_Type *clone() const override { return new UDT_Type_impl(*this); }

  UDT_Type *clone_dropped_object_placeholder() const override {
    /*
      Even though we don't drop SRSes en masse we still create slimmed
      down version for consistency sake.
    */
    UDT_Type_impl *placeholder = new UDT_Type_impl();
    placeholder->set_id(id());
    placeholder->set_schema_id(schema_id());
    placeholder->set_name(name());
    return placeholder;
  }
};

///////////////////////////////////////////////////////////////////////////

}  // namespace dd

#endif  // DD__UDT_TYPE_IMPL_INCLUDED
