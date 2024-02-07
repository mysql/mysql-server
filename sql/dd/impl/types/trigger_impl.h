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

#ifndef DD__TRIGGER_IMPL_INCLUDED
#define DD__TRIGGER_IMPL_INCLUDED

#include "my_config.h"

#include "my_inttypes.h"
#include "sql/dd/string_type.h"

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <sys/types.h>
#include <new>

#include "sql/dd/impl/raw/raw_record.h"
#include "sql/dd/impl/types/entity_object_impl.h"  // dd::Entity_object_impl
#include "sql/dd/impl/types/table_impl.h"          // dd::Table_impl
#include "sql/dd/impl/types/weak_object_impl.h"
#include "sql/dd/object_id.h"
#include "sql/dd/types/trigger.h"  // dd::Trigger

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Object_table;
class Open_dictionary_tables_ctx;
class Table;
class Weak_object;

class Trigger_impl : virtual public Entity_object_impl, virtual public Trigger {
 public:
  Trigger_impl();

  Trigger_impl(Table_impl *table);

  Trigger_impl(const Trigger_impl &src, Table_impl *parent);

 public:
  const Object_table &object_table() const override;

  bool validate() const override;

  bool restore_attributes(const Raw_record &r) override;

  bool store_attributes(Raw_record *r) override;

  void debug_print(String_type &outb) const override;

  void set_ordinal_position(uint ordinal_position) {
    m_ordinal_position = ordinal_position;
  }

  uint ordinal_position() const { return m_ordinal_position; }

 public:
  static void register_tables(Open_dictionary_tables_ctx *otx);

  /////////////////////////////////////////////////////////////////////////
  // Table.
  /////////////////////////////////////////////////////////////////////////

  virtual const Table &table() const;

  virtual Table &table();

  /* non-virtual */ void set_table(Table_impl *parent) { m_table = parent; }

  /* non-virtual */ const Table_impl &table_impl() const { return *m_table; }

  /* non-virtual */ Table_impl &table_impl() { return *m_table; }

  /////////////////////////////////////////////////////////////////////////
  // schema.
  /////////////////////////////////////////////////////////////////////////

  Object_id schema_id() const override {
    return (m_table != nullptr ? m_table->schema_id() : INVALID_OBJECT_ID);
  }

  /////////////////////////////////////////////////////////////////////////
  // event type
  /////////////////////////////////////////////////////////////////////////

  enum_event_type event_type() const override { return m_event_type; }

  void set_event_type(enum_event_type event_type) override {
    m_event_type = event_type;
  }

  /////////////////////////////////////////////////////////////////////////
  // table.
  /////////////////////////////////////////////////////////////////////////

  Object_id table_id() const override { return m_table->id(); }

  /////////////////////////////////////////////////////////////////////////
  // action timing
  /////////////////////////////////////////////////////////////////////////

  enum_action_timing action_timing() const override { return m_action_timing; }

  void set_action_timing(enum_action_timing action_timing) override {
    m_action_timing = action_timing;
  }

  /////////////////////////////////////////////////////////////////////////
  // action_order.
  /////////////////////////////////////////////////////////////////////////

  uint action_order() const override { return m_action_order; }

  void set_action_order(uint action_order) override {
    m_action_order = action_order;
  }

  /////////////////////////////////////////////////////////////////////////
  // action_statement/utf8.
  /////////////////////////////////////////////////////////////////////////

  const String_type &action_statement() const override {
    return m_action_statement;
  }

  void set_action_statement(const String_type &action_statement) override {
    m_action_statement = action_statement;
  }

  const String_type &action_statement_utf8() const override {
    return m_action_statement_utf8;
  }

  void set_action_statement_utf8(
      const String_type &action_statement_utf8) override {
    m_action_statement_utf8 = action_statement_utf8;
  }

  /////////////////////////////////////////////////////////////////////////
  // created.
  /////////////////////////////////////////////////////////////////////////

  my_timeval created() const override { return m_created; }

  void set_created(my_timeval created) override { m_created = created; }

  /////////////////////////////////////////////////////////////////////////
  // last altered.
  /////////////////////////////////////////////////////////////////////////

  my_timeval last_altered() const override { return m_last_altered; }

  void set_last_altered(my_timeval last_altered) override {
    m_last_altered = last_altered;
  }

  /////////////////////////////////////////////////////////////////////////
  // sql_mode
  /////////////////////////////////////////////////////////////////////////

  ulonglong sql_mode() const override { return m_sql_mode; }

  void set_sql_mode(ulonglong sql_mode) override { m_sql_mode = sql_mode; }

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
  static Trigger_impl *restore_item(Table_impl *table) {
    return new (std::nothrow) Trigger_impl(table);
  }

  static Trigger_impl *clone(const Trigger_impl &other, Table_impl *table) {
    return new (std::nothrow) Trigger_impl(other, table);
  }

 private:
  enum_event_type m_event_type;
  enum_action_timing m_action_timing;

  /*
    We use m_ordinal_position to help implement
    add_trigger_following and add_trigger_preceding.
    This is required mainly because we maintain a single
    collection to maintain all triggers.
  */
  uint m_ordinal_position;
  uint m_action_order;

  ulonglong m_sql_mode;
  my_timeval m_created;
  my_timeval m_last_altered;

  String_type m_action_statement_utf8;
  String_type m_action_statement;
  String_type m_definer_user;
  String_type m_definer_host;

  // References to tightly-coupled objects.

  Table_impl *m_table;

  // References to loosely-coupled objects.

  Object_id m_client_collation_id;
  Object_id m_connection_collation_id;
  Object_id m_schema_collation_id;
};

///////////////////////////////////////////////////////////////////////////

/**
  Used to sort Triggers of the same table by action timing, event type and
  action order.
*/

struct Trigger_order_comparator {
  bool operator()(const dd::Trigger *t1, const dd::Trigger *t2) const {
    return ((t1->action_timing() < t2->action_timing()) ||
            (t1->action_timing() == t2->action_timing() &&
             t1->event_type() < t2->event_type()) ||
            (t1->action_timing() == t2->action_timing() &&
             t1->event_type() == t2->event_type() &&
             t1->action_order() < t2->action_order()));
  }
};

///////////////////////////////////////////////////////////////////////////
}  // namespace dd

#endif  // DD__TRIGGER_IMPL_INCLUDED
