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

#ifndef DD__SCHEMA_IMPL_INCLUDED
#define DD__SCHEMA_IMPL_INCLUDED

#include <stdio.h>
#include <new>
#include <string>

#include "my_inttypes.h"
#include "sql/dd/impl/properties_impl.h"           // Properties_impl
#include "sql/dd/impl/types/entity_object_impl.h"  // dd::Entity_object_impl
#include "sql/dd/impl/types/weak_object_impl.h"
#include "sql/dd/object_id.h"
#include "sql/dd/sdi_fwd.h"
#include "sql/dd/string_type.h"
#include "sql/dd/types/entity_object_table.h"  // dd::Entity_object_table
#include "sql/dd/types/schema.h"               // dd::Schema
#include "sql/sql_time.h"                      // gmt_time_to_local_time

class THD;

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Event;
class Function;
class Object_table;
class Open_dictionary_tables_ctx;
class Procedure;
class Raw_record;
class Sdi_rcontext;
class Sdi_wcontext;
class Table;
class View;
class Weak_object;
class Object_table;

///////////////////////////////////////////////////////////////////////////

class Schema_impl : public Entity_object_impl, public Schema {
 public:
  Schema_impl();

 public:
  const Object_table &object_table() const override;

  bool validate() const override;

  bool store_attributes(Raw_record *r) override;

  bool restore_attributes(const Raw_record &r) override;

 public:
  static void register_tables(Open_dictionary_tables_ctx *otx);

  /////////////////////////////////////////////////////////////////////////
  // Default collation.
  /////////////////////////////////////////////////////////////////////////

  Object_id default_collation_id() const override {
    return m_default_collation_id;
  }

  void set_default_collation_id(Object_id default_collation_id) override {
    m_default_collation_id = default_collation_id;
  }

  /////////////////////////////////////////////////////////////////////////
  // Default encryption.
  /////////////////////////////////////////////////////////////////////////

  bool default_encryption() const override {
    return m_default_encryption == enum_encryption_type::ET_YES;
  }

  void set_default_encryption(bool default_encryption) override {
    m_default_encryption = default_encryption ? enum_encryption_type::ET_YES
                                              : enum_encryption_type::ET_NO;
  }

  /////////////////////////////////////////////////////////////////////////
  // Read only.
  /////////////////////////////////////////////////////////////////////////
  bool read_only() const override;
  void set_read_only(bool state) override;

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
  // options.
  /////////////////////////////////////////////////////////////////////////

  const Properties &options() const override { return m_options; }

  Properties &options() override { return m_options; }

  bool set_options(const Properties &options) override {
    return m_options.insert_values(options);
  }

  bool set_options(const String_type &options_raw) override {
    return m_options.insert_values(options_raw);
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
  Event *create_event(THD *thd) const override;

  Function *create_function(THD *thd) const override;

  Procedure *create_procedure(THD *thd) const override;

  Table *create_table(THD *thd) const override;

  View *create_view(THD *thd) const override;

  View *create_system_view(THD *thd) const override;

 public:
  void debug_print(String_type &outb) const override {
    char outbuf[1024];
    sprintf(outbuf,
            "SCHEMA OBJECT: id= {OID: %lld}, name= %s, "
            "collation_id={OID: %lld},"
            "m_created= %llu, m_last_altered= %llu,"
            "m_default_encryption= %d, "
            "se_private_data= %s, options= %s",
            id(), name().c_str(), m_default_collation_id, m_created,
            m_last_altered, static_cast<int>(m_default_encryption),
            m_se_private_data.raw_string().c_str(),
            m_options.raw_string().c_str());
    outb = String_type(outbuf);
  }

 private:
  // Fields
  ulonglong m_created = 0;
  ulonglong m_last_altered = 0;
  enum_encryption_type m_default_encryption = enum_encryption_type::ET_NO;

  // The se_private_data column of a schema might be used by several storage
  // engines at the same time as the schema is not associated with any specific
  // engine. So to avoid any naming conflicts, we have the convention that the
  // keys should be prefixed with the engine name.
  Properties_impl m_se_private_data;

  Properties_impl m_options;

  // References to other objects
  Object_id m_default_collation_id;

  Schema *clone() const override { return new Schema_impl(*this); }

  Schema *clone_dropped_object_placeholder() const override {
    /*
      Even though we don't drop databases en masse we still create slimmed
      down version for consistency sake.
    */
    Schema_impl *placeholder = new Schema_impl();
    placeholder->set_id(id());
    placeholder->set_name(name());
    return placeholder;
  }
};

///////////////////////////////////////////////////////////////////////////

}  // namespace dd

#endif  // DD__SCHEMA_IMPL_INCLUDED
