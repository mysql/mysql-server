/* Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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

#ifndef DD__ROUTINE_IMPL_INCLUDED
#define DD__ROUTINE_IMPL_INCLUDED

#include <stddef.h>
#include <memory>  // std::unique_ptr
#include <string>

#include "my_dbug.h"
#include "my_inttypes.h"
#include "sql/dd/impl/raw/raw_record.h"
#include "sql/dd/impl/types/entity_object_impl.h"  // dd::Entity_object_impl
#include "sql/dd/impl/types/weak_object_impl.h"
#include "sql/dd/object_id.h"
#include "sql/dd/string_type.h"
#include "sql/dd/types/parameter.h"  // dd::Parameter
#include "sql/dd/types/routine.h"    // dd::Routine
#include "sql/dd/types/view.h"
#include "sql/sql_time.h"  // gmt_time_to_local_time

namespace dd {
class Open_dictionary_tables_ctx;
class Parameter;
class Parameter_collection;
class Weak_object;
class Object_table;

///////////////////////////////////////////////////////////////////////////

class Routine_impl : public Entity_object_impl, virtual public Routine {
 public:
  Routine_impl();

  ~Routine_impl() override;

 public:
  const Object_table &object_table() const override;

  bool validate() const override;

  bool restore_children(Open_dictionary_tables_ctx *otx) override;

  bool store_children(Open_dictionary_tables_ctx *otx) override;

  bool drop_children(Open_dictionary_tables_ctx *otx) const override;

  bool restore_attributes(const Raw_record &r) override;

  bool store_attributes(Raw_record *r) override;

  void debug_print(String_type &outb) const override;

 public:
  static void register_tables(Open_dictionary_tables_ctx *otx);

  /////////////////////////////////////////////////////////////////////////
  // schema.
  /////////////////////////////////////////////////////////////////////////

  Object_id schema_id() const override { return m_schema_id; }

  void set_schema_id(Object_id schema_id) override { m_schema_id = schema_id; }

  /////////////////////////////////////////////////////////////////////////
  // routine Partition type
  /////////////////////////////////////////////////////////////////////////

  enum_routine_type type() const override { return m_routine_type; }

  virtual void set_type(enum_routine_type routine_type) {
    m_routine_type = routine_type;
  }

  /////////////////////////////////////////////////////////////////////////
  // definition/utf8.
  /////////////////////////////////////////////////////////////////////////

  const String_type &definition() const override { return m_definition; }

  void set_definition(const String_type &definition) override {
    m_definition = definition;
  }

  const String_type &definition_utf8() const override {
    return m_definition_utf8;
  }

  void set_definition_utf8(const String_type &definition_utf8) override {
    m_definition_utf8 = definition_utf8;
  }

  /////////////////////////////////////////////////////////////////////////
  // parameter_str
  /////////////////////////////////////////////////////////////////////////

  const String_type &parameter_str() const override { return m_parameter_str; }

  void set_parameter_str(const String_type &parameter_str) override {
    m_parameter_str = parameter_str;
  }

  /////////////////////////////////////////////////////////////////////////
  // is_deterministic.
  /////////////////////////////////////////////////////////////////////////

  bool is_deterministic() const override { return m_is_deterministic; }

  void set_deterministic(bool deterministic) override {
    m_is_deterministic = deterministic;
  }

  /////////////////////////////////////////////////////////////////////////
  // sql data access.
  /////////////////////////////////////////////////////////////////////////

  enum_sql_data_access sql_data_access() const override {
    return m_sql_data_access;
  }

  void set_sql_data_access(enum_sql_data_access sda) override {
    m_sql_data_access = sda;
  }

  /////////////////////////////////////////////////////////////////////////
  // security_type.
  /////////////////////////////////////////////////////////////////////////

  View::enum_security_type security_type() const override {
    return m_security_type;
  }

  void set_security_type(View::enum_security_type security_type) override {
    m_security_type = security_type;
  }

  /////////////////////////////////////////////////////////////////////////
  // sql_mode
  /////////////////////////////////////////////////////////////////////////

  ulonglong sql_mode() const override { return m_sql_mode; }

  void set_sql_mode(ulonglong sm) override { m_sql_mode = sm; }

  /////////////////////////////////////////////////////////////////////////
  // definer.
  /////////////////////////////////////////////////////////////////////////

  const String_type &definer_user() const override { return m_definer_user; }

  const String_type &definer_host() const override { return m_definer_host; }

  void set_definer(const String_type &username,
                   const String_type &hostname) override {
    m_definer_user = username;
    m_definer_host = hostname;
  }

  /////////////////////////////////////////////////////////////////////////
  // collation.
  /////////////////////////////////////////////////////////////////////////

  Object_id client_collation_id() const override {
    return m_client_collation_id;
  }

  void set_client_collation_id(Object_id client_collation_id) override {
    m_client_collation_id = client_collation_id;
  }

  Object_id connection_collation_id() const override {
    return m_connection_collation_id;
  }

  void set_connection_collation_id(Object_id connection_collation_id) override {
    m_connection_collation_id = connection_collation_id;
  }

  Object_id schema_collation_id() const override {
    return m_schema_collation_id;
  }

  void set_schema_collation_id(Object_id schema_collation_id) override {
    m_schema_collation_id = schema_collation_id;
  }

  /////////////////////////////////////////////////////////////////////////
  // created.
  /////////////////////////////////////////////////////////////////////////

  ulonglong created(bool convert_time) const override {
    return convert_time ? gmt_time_to_local_time(m_created) : m_created;
  }

  void set_created(ulonglong created) override { m_created = created; }

  /////////////////////////////////////////////////////////////////////////
  // last altered.
  /////////////////////////////////////////////////////////////////////////

  ulonglong last_altered(bool convert_time) const override {
    return convert_time ? gmt_time_to_local_time(m_last_altered)
                        : m_last_altered;
  }

  void set_last_altered(ulonglong last_altered) override {
    m_last_altered = last_altered;
  }

  /////////////////////////////////////////////////////////////////////////
  // comment.
  /////////////////////////////////////////////////////////////////////////

  const String_type &comment() const override { return m_comment; }

  void set_comment(const String_type &comment) override { m_comment = comment; }

  /////////////////////////////////////////////////////////////////////////
  // Parameter collection.
  /////////////////////////////////////////////////////////////////////////

  Parameter *add_parameter() override;

  const Parameter_collection &parameters() const override {
    return m_parameters;
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

 private:
  enum_routine_type m_routine_type;
  enum_sql_data_access m_sql_data_access;
  View::enum_security_type m_security_type;

  bool m_is_deterministic;

  ulonglong m_sql_mode;
  ulonglong m_created;
  ulonglong m_last_altered;

  String_type m_definition;
  String_type m_definition_utf8;
  String_type m_parameter_str;
  String_type m_definer_user;
  String_type m_definer_host;
  String_type m_comment;

  // Collections.

  Parameter_collection m_parameters;

  // References.

  Object_id m_schema_id;
  Object_id m_client_collation_id;
  Object_id m_connection_collation_id;
  Object_id m_schema_collation_id;

 protected:
  Routine_impl(const Routine_impl &src);
};

///////////////////////////////////////////////////////////////////////////

}  // namespace dd

#endif  // DD__ROUTINE_IMPL_INCLUDED
