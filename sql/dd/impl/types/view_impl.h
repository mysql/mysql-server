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

#ifndef DD__VIEW_IMPL_INCLUDED
#define DD__VIEW_IMPL_INCLUDED

#include <sys/types.h>
#include <memory>
#include <new>

#include "my_inttypes.h"
#include "sql/dd/impl/properties_impl.h"
#include "sql/dd/impl/raw/raw_record.h"
#include "sql/dd/impl/types/abstract_table_impl.h"  // dd::Abstract_table_impl
#include "sql/dd/impl/types/entity_object_impl.h"
#include "sql/dd/impl/types/weak_object_impl.h"
#include "sql/dd/object_id.h"
#include "sql/dd/string_type.h"
#include "sql/dd/types/abstract_table.h"
#include "sql/dd/types/view.h"          // dd::View
#include "sql/dd/types/view_routine.h"  // IWYU pragma: keep
#include "sql/dd/types/view_table.h"    // IWYU pragma: keep

namespace dd {
class Column;
class Open_dictionary_tables_ctx;
class Weak_object;
class Object_table;
}  // namespace dd

namespace dd {

///////////////////////////////////////////////////////////////////////////

class View_impl : public Abstract_table_impl, public View {
 public:
  View_impl();

  ~View_impl() override = default;

 public:
  static void register_tables(Open_dictionary_tables_ctx *otx);

  bool validate() const override;

  bool restore_children(Open_dictionary_tables_ctx *otx) override;

  bool store_children(Open_dictionary_tables_ctx *otx) override;

  bool drop_children(Open_dictionary_tables_ctx *otx) const override;

  void remove_children() override;

  bool restore_attributes(const Raw_record &r) override;

  bool store_attributes(Raw_record *r) override;

  void debug_print(String_type &outb) const override;

 public:
  /////////////////////////////////////////////////////////////////////////
  // enum_table_type.
  /////////////////////////////////////////////////////////////////////////

  enum_table_type type() const override { return m_type; }

  /////////////////////////////////////////////////////////////////////////
  // regular/system view flag.
  /////////////////////////////////////////////////////////////////////////

  void set_system_view(bool system_view) override {
    m_type =
        system_view ? enum_table_type::SYSTEM_VIEW : enum_table_type::USER_VIEW;
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
  // check_option.
  /////////////////////////////////////////////////////////////////////////

  enum_check_option check_option() const override { return m_check_option; }

  void set_check_option(enum_check_option check_option) override {
    m_check_option = check_option;
  }

  /////////////////////////////////////////////////////////////////////////
  // is_updatable.
  /////////////////////////////////////////////////////////////////////////

  bool is_updatable() const override { return m_is_updatable; }

  void set_updatable(bool updatable) override { m_is_updatable = updatable; }

  /////////////////////////////////////////////////////////////////////////
  // algorithm.
  /////////////////////////////////////////////////////////////////////////

  enum_algorithm algorithm() const override { return m_algorithm; }

  void set_algorithm(enum_algorithm algorithm) override {
    m_algorithm = algorithm;
  }

  /////////////////////////////////////////////////////////////////////////
  // security_type.
  /////////////////////////////////////////////////////////////////////////

  enum_security_type security_type() const override { return m_security_type; }

  void set_security_type(enum_security_type security_type) override {
    m_security_type = security_type;
  }

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
  // Explicit list of column names.
  /////////////////////////////////////////////////////////////////////////

  const Properties &column_names() const override { return m_column_names; }

  Properties &column_names() override { return m_column_names; }

  /////////////////////////////////////////////////////////////////////////
  // View_table collection.
  /////////////////////////////////////////////////////////////////////////

  View_table *add_table() override;

  const View_tables &tables() const override { return m_tables; }

  /////////////////////////////////////////////////////////////////////////
  // View_routine collection.
  /////////////////////////////////////////////////////////////////////////

  View_routine *add_routine() override;

  const View_routines &routines() const override { return m_routines; }

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
  Object_id schema_id() const override {
    return Abstract_table_impl::schema_id();
  }
  void set_schema_id(Object_id schema_id) override {
    Abstract_table_impl::set_schema_id(schema_id);
  }
  uint mysql_version_id() const override {
    return Abstract_table_impl::mysql_version_id();
  }
  const Properties &options() const override {
    return Abstract_table_impl::options();
  }
  Properties &options() override { return Abstract_table_impl::options(); }
  bool set_options(const Properties &options) override {
    return Abstract_table_impl::set_options(options);
  }
  bool set_options(const String_type &options_raw) override {
    return Abstract_table_impl::set_options(options_raw);
  }
  ulonglong created(bool convert_time) const override {
    return Abstract_table_impl::created(convert_time);
  }
  void set_created(ulonglong created) override {
    Abstract_table_impl::set_created(created);
  }
  ulonglong last_altered(bool convert_time) const override {
    return Abstract_table_impl::last_altered(convert_time);
  }
  void set_last_altered(ulonglong last_altered) override {
    Abstract_table_impl::set_last_altered(last_altered);
  }
  Column *add_column() override { return Abstract_table_impl::add_column(); }
  bool drop_column(const String_type &name) override {
    return Abstract_table_impl::drop_column(name);
  }
  const Column_collection &columns() const override {
    return Abstract_table_impl::columns();
  }
  Column_collection *columns() override {
    return Abstract_table_impl::columns();
  }
  const Column *get_column(const String_type &name) const override {
    return Abstract_table_impl::get_column(name);
  }
  Column *get_column(const String_type &name) {
    return Abstract_table_impl::get_column(name);
  }
  enum_hidden_type hidden() const override {
    return Abstract_table_impl::hidden();
  }
  void set_hidden(enum_hidden_type hidden) override {
    Abstract_table_impl::set_hidden(hidden);
  }

 private:
  enum_table_type m_type;
  bool m_is_updatable;
  enum_check_option m_check_option;
  enum_algorithm m_algorithm;
  enum_security_type m_security_type;

  String_type m_definition;
  String_type m_definition_utf8;
  String_type m_definer_user;
  String_type m_definer_host;

  Properties_impl m_column_names;

  // Collections.

  View_tables m_tables;
  View_routines m_routines;

  // References.

  Object_id m_client_collation_id;
  Object_id m_connection_collation_id;

  View_impl(const View_impl &src);
  View_impl *clone() const override { return new View_impl(*this); }

  // N.B.: returning dd::View from this function confuses MSVC compiler
  // thanks to diamond inheritance.
  View_impl *clone_dropped_object_placeholder() const override {
    View_impl *placeholder = new View_impl();
    placeholder->set_id(id());
    placeholder->set_schema_id(schema_id());
    placeholder->set_name(name());
    return placeholder;
  }
};

///////////////////////////////////////////////////////////////////////////

}  // namespace dd

#endif  // DD__VIEW_IMPL_INCLUDED
