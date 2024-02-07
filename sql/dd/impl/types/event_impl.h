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

#ifndef DD__EVENT_IMPL_INCLUDED
#define DD__EVENT_IMPL_INCLUDED

#include <sys/types.h>
#include <new>
#include <string>

#include "my_inttypes.h"
#include "sql/dd/impl/raw/raw_record.h"
#include "sql/dd/impl/types/entity_object_impl.h"  // dd::Entity_object_impl
#include "sql/dd/impl/types/weak_object_impl.h"
#include "sql/dd/object_id.h"
#include "sql/dd/string_type.h"
#include "sql/dd/types/event.h"  // dd::Event
#include "sql/sql_time.h"        // gmt_time_to_local_time

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Open_dictionary_tables_ctx;
class Weak_object;
class Object_table;

class Event_impl : public Entity_object_impl, public Event {
 public:
  Event_impl();
  Event_impl(const Event_impl &);

  ~Event_impl() override = default;

 public:
  const Object_table &object_table() const override;

  static void register_tables(Open_dictionary_tables_ctx *otx);

  bool validate() const override;

  bool restore_attributes(const Raw_record &r) override;

  bool store_attributes(Raw_record *r) override;

  void debug_print(String_type &outb) const override;

 public:
  /////////////////////////////////////////////////////////////////////////
  // schema.
  /////////////////////////////////////////////////////////////////////////

  Object_id schema_id() const override { return m_schema_id; }

  void set_schema_id(Object_id schema_id) override { m_schema_id = schema_id; }

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
  // time_zone
  /////////////////////////////////////////////////////////////////////////

  const String_type &time_zone() const override { return m_time_zone; }

  void set_time_zone(const String_type &time_zone) override {
    m_time_zone = time_zone;
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
  // execute_at.
  /////////////////////////////////////////////////////////////////////////

  my_time_t execute_at() const override { return m_execute_at; }

  void set_execute_at(my_time_t execute_at) override {
    m_execute_at = execute_at;
  }

  void set_execute_at_null(bool is_null) override {
    m_is_execute_at_null = is_null;
  }

  bool is_execute_at_null() const override { return m_is_execute_at_null; }

  /////////////////////////////////////////////////////////////////////////
  // interval_value.
  /////////////////////////////////////////////////////////////////////////

  uint interval_value() const override { return m_interval_value; }

  void set_interval_value(uint interval_value) override {
    m_interval_value = interval_value;
  }

  void set_interval_value_null(bool is_null) override {
    m_is_interval_value_null = is_null;
  }

  bool is_interval_value_null() const override {
    return m_is_interval_value_null;
  }

  /////////////////////////////////////////////////////////////////////////
  // interval_field
  /////////////////////////////////////////////////////////////////////////

  enum_interval_field interval_field() const override {
    return m_interval_field;
  }

  void set_interval_field(enum_interval_field interval_field) override {
    m_interval_field = interval_field;
  }

  void set_interval_field_null(bool is_null) override {
    m_is_interval_field_null = is_null;
  }

  bool is_interval_field_null() const override {
    return m_is_interval_field_null;
  }

  /////////////////////////////////////////////////////////////////////////
  // sql_mode
  /////////////////////////////////////////////////////////////////////////

  ulonglong sql_mode() const override { return m_sql_mode; }

  void set_sql_mode(ulonglong sm) override { m_sql_mode = sm; }

  /////////////////////////////////////////////////////////////////////////
  // starts.
  /////////////////////////////////////////////////////////////////////////

  my_time_t starts() const override { return m_starts; }

  void set_starts(my_time_t starts) override { m_starts = starts; }

  void set_starts_null(bool is_null) override { m_is_starts_null = is_null; }

  bool is_starts_null() const override { return m_is_starts_null; }

  /////////////////////////////////////////////////////////////////////////
  // ends.
  /////////////////////////////////////////////////////////////////////////

  my_time_t ends() const override { return m_ends; }

  void set_ends(my_time_t ends) override { m_ends = ends; }

  void set_ends_null(bool is_null) override { m_is_ends_null = is_null; }

  bool is_ends_null() const override { return m_is_ends_null; }

  /////////////////////////////////////////////////////////////////////////
  // event_status
  /////////////////////////////////////////////////////////////////////////

  enum_event_status event_status() const override { return m_event_status; }

  void set_event_status(enum_event_status event_status) override {
    m_event_status = event_status;
  }

  void set_event_status_null(bool is_null) override {
    m_is_event_status_null = is_null;
  }

  bool is_event_status_null() const override { return m_is_event_status_null; }

  /////////////////////////////////////////////////////////////////////////
  // on_completion
  /////////////////////////////////////////////////////////////////////////

  enum_on_completion on_completion() const override { return m_on_completion; }

  void set_on_completion(enum_on_completion on_completion) override {
    m_on_completion = on_completion;
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
  // last_executed.
  /////////////////////////////////////////////////////////////////////////

  my_time_t last_executed() const override { return m_last_executed; }

  void set_last_executed(my_time_t last_executed) override {
    m_is_last_executed_null = false;
    m_last_executed = last_executed;
  }

  void set_last_executed_null(bool is_null) override {
    m_is_last_executed_null = is_null;
  }

  bool is_last_executed_null() const override {
    return m_is_last_executed_null;
  }

  /////////////////////////////////////////////////////////////////////////
  // comment.
  /////////////////////////////////////////////////////////////////////////

  const String_type &comment() const override { return m_comment; }

  void set_comment(const String_type &comment) override { m_comment = comment; }

  /////////////////////////////////////////////////////////////////////////
  // originator
  /////////////////////////////////////////////////////////////////////////

  ulonglong originator() const override { return m_originator; }

  void set_originator(ulonglong originator) override {
    m_originator = originator;
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
  enum_interval_field m_interval_field;
  enum_event_status m_event_status;
  enum_on_completion m_on_completion;

  ulonglong m_sql_mode;
  ulonglong m_created;
  ulonglong m_last_altered;
  ulonglong m_originator;
  uint m_interval_value;

  my_time_t m_execute_at;
  my_time_t m_starts;
  my_time_t m_ends;
  my_time_t m_last_executed;

  bool m_is_execute_at_null;
  bool m_is_interval_value_null;
  bool m_is_interval_field_null;
  bool m_is_starts_null;
  bool m_is_ends_null;
  bool m_is_event_status_null;
  bool m_is_last_executed_null;

  String_type m_time_zone;
  String_type m_definition;
  String_type m_definition_utf8;
  String_type m_definer_user;
  String_type m_definer_host;
  String_type m_comment;

  // References.

  Object_id m_schema_id;
  Object_id m_client_collation_id;
  Object_id m_connection_collation_id;
  Object_id m_schema_collation_id;

  Event *clone() const override { return new Event_impl(*this); }

  Event *clone_dropped_object_placeholder() const override {
    Event_impl *placeholder = new Event_impl();
    placeholder->set_id(id());
    placeholder->set_schema_id(schema_id());
    placeholder->set_name(name());
    return placeholder;
  }
};

///////////////////////////////////////////////////////////////////////////

}  // namespace dd

#endif  // DD__EVENT_IMPL_INCLUDED
