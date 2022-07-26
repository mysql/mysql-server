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

#ifndef DD__ENTITY_OBJECT_TABLE_IMPL_INCLUDED
#define DD__ENTITY_OBJECT_TABLE_IMPL_INCLUDED

#include <sys/types.h>

#include "my_compiler.h"
#include "sql/dd/impl/types/object_table_impl.h"  // Object_table_impl
#include "sql/dd/types/entity_object_table.h"     // dd::Entity_object_table

class THD;

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Entity_object;
class Object_table_definition;
class Open_dictionary_tables_ctx;
class Properties;
class Raw_record;

class Entity_object_table_impl : public Object_table_impl,
                                 public Entity_object_table {
 public:
  ~Entity_object_table_impl() override = default;

  bool restore_object_from_record(Open_dictionary_tables_ctx *otx,
                                  const Raw_record &record,
                                  Entity_object **o) const override;

  // Fix "inherits ... via dominance" warnings

  const String_type &name() const override { return Object_table_impl::name(); }

  Object_table_definition_impl *target_table_definition() override {
    return Object_table_impl::target_table_definition();
  }

  const Object_table_definition_impl *target_table_definition() const override {
    return Object_table_impl::target_table_definition();
  }

  void set_abandoned(uint last_dd_version) const override {
    return Object_table_impl::set_abandoned(last_dd_version);
  }

  bool is_abandoned() const override {
    return Object_table_impl::is_abandoned();
  }

  const Object_table_definition_impl *actual_table_definition() const override {
    return Object_table_impl::actual_table_definition();
  }

  bool set_actual_table_definition(
      const Properties &table_def_properties) const override {
    return Object_table_impl::set_actual_table_definition(table_def_properties);
  }

  int field_number(int target_field_number,
                   const String_type &field_label) const override {
    return Object_table_impl::field_number(target_field_number, field_label);
  }

  int field_number(const String_type &field_label) const override {
    return Object_table_impl::field_number(field_label);
  }

  bool populate(THD *thd) const override {
    return Object_table_impl::populate(thd);
  }

  bool is_hidden() const override { return Object_table_impl::is_hidden(); }

  void set_hidden(bool hidden) override {
    return Object_table_impl::set_hidden(hidden);
  }
};

///////////////////////////////////////////////////////////////////////////

}  // namespace dd

#endif  // DD__ENTITY_OBJECT_TABLE_IMPL_INCLUDED
