/* Copyright (c) 2016, 2022, Oracle and/or its affiliates.

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

#ifndef DD__PROCEDURE_IMPL_INCLUDED
#define DD__PROCEDURE_IMPL_INCLUDED

#include <new>

#include "my_inttypes.h"
#include "sql/dd/impl/types/entity_object_impl.h"
#include "sql/dd/impl/types/routine_impl.h"  // dd::Routine_impl
#include "sql/dd/impl/types/weak_object_impl.h"
#include "sql/dd/object_id.h"
#include "sql/dd/string_type.h"
#include "sql/dd/types/procedure.h"  // dd::Procedure
#include "sql/dd/types/routine.h"
#include "sql/dd/types/view.h"

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Open_dictionary_tables_ctx;
class Parameter;
class Weak_object;
class Object_table;

class Procedure_impl : public Routine_impl, public Procedure {
 public:
  Procedure_impl() = default;

  ~Procedure_impl() override = default;

 public:
  bool update_routine_name_key(Name_key *key, Object_id schema_id,
                               const String_type &name) const override;

  void debug_print(String_type &outb) const override;

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
    return Routine::update_name_key(key);
  }

 private:
  Procedure_impl(const Procedure_impl &src);
  Procedure_impl *clone() const override { return new Procedure_impl(*this); }

  // N.B.: returning dd::Procedure from this function might confuse MSVC
  // compiler thanks to diamond inheritance.
  Procedure_impl *clone_dropped_object_placeholder() const override {
    Procedure_impl *placeholder = new Procedure_impl();
    placeholder->set_id(id());
    placeholder->set_schema_id(schema_id());
    placeholder->set_name(name());
    return placeholder;
  }
};

///////////////////////////////////////////////////////////////////////////

}  // namespace dd

#endif  // DD__PROCEDURE_IMPL_INCLUDED
