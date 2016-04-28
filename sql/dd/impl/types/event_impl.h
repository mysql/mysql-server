/* Copyright (c) 2016 Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software Foundation,
   51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef DD__EVENT_IMPL_INCLUDED
#define DD__EVENT_IMPL_INCLUDED

#include "my_global.h"

#include "dd/impl/types/entity_object_impl.h"  // dd::Entity_object_impl
#include "dd/types/dictionary_object_table.h"  // dd::Dictionary_object_table
#include "dd/types/event.h"                    // dd::Event
#include "dd/types/object_type.h"              // dd::Object_type

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Event_impl : public Entity_object_impl,
                   public Event
{
public:
  Event_impl();
  Event_impl(const Event_impl&);

  virtual ~Event_impl()
  { }

public:
  virtual const Dictionary_object_table &object_table() const
  { return Event::OBJECT_TABLE(); }

  virtual bool validate() const;

  virtual bool restore_attributes(const Raw_record &r);

  virtual bool store_attributes(Raw_record *r);

  virtual void debug_print(std::string &outb) const;

public:
  /////////////////////////////////////////////////////////////////////////
  // schema.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id schema_id() const
  { return m_schema_id; }

  virtual void set_schema_id(Object_id schema_id)
  { m_schema_id= schema_id; }

  /////////////////////////////////////////////////////////////////////////
  // definer.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &definer_user() const
  { return m_definer_user; }

  virtual const std::string &definer_host() const
  { return m_definer_host; }

  virtual void set_definer(const std::string &username,
                           const std::string &hostname)
  {
    m_definer_user= username;
    m_definer_host= hostname;
  }

  /////////////////////////////////////////////////////////////////////////
  // time_zone
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &time_zone() const
  { return m_time_zone; }

  virtual void set_time_zone(const std::string &time_zone)
  { m_time_zone= time_zone; }

  /////////////////////////////////////////////////////////////////////////
  // definition/utf8.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &definition() const
  { return m_definition; }

  virtual void set_definition(const std::string &definition)
  { m_definition= definition; }

  virtual const std::string &definition_utf8() const
  { return m_definition_utf8; }

  virtual void set_definition_utf8(const std::string &definition_utf8)
  { m_definition_utf8= definition_utf8; }

  /////////////////////////////////////////////////////////////////////////
  // execute_at.
  /////////////////////////////////////////////////////////////////////////

  virtual my_time_t execute_at() const
  { return m_execute_at; }

  virtual void set_execute_at(my_time_t execute_at)
  { m_execute_at= execute_at; }

  virtual void set_execute_at_null(bool is_null)
  { m_is_execute_at_null= is_null; }

  virtual bool is_execute_at_null() const
  { return m_is_execute_at_null; }

  /////////////////////////////////////////////////////////////////////////
  // interval_value.
  /////////////////////////////////////////////////////////////////////////

  virtual uint interval_value() const
  { return m_interval_value; }

  virtual void set_interval_value(uint interval_value)
  { m_interval_value= interval_value; }

  virtual void set_interval_value_null(bool is_null)
  { m_is_interval_value_null= is_null; }

  virtual bool is_interval_value_null() const
  { return m_is_interval_value_null; }

  /////////////////////////////////////////////////////////////////////////
  // interval_field
  /////////////////////////////////////////////////////////////////////////

  virtual enum_interval_field interval_field() const
  { return m_interval_field; }

  virtual void set_interval_field(enum_interval_field interval_field)
  { m_interval_field= interval_field; }

  virtual void set_interval_field_null(bool is_null)
  { m_is_interval_field_null= is_null; }

  virtual bool is_interval_field_null() const
  { return m_is_interval_field_null; }

  /////////////////////////////////////////////////////////////////////////
  // sql_mode
  /////////////////////////////////////////////////////////////////////////

  virtual ulonglong sql_mode() const
  { return m_sql_mode; }

  virtual void set_sql_mode(ulonglong sm)
  { m_sql_mode= sm; }

  /////////////////////////////////////////////////////////////////////////
  // starts.
  /////////////////////////////////////////////////////////////////////////

  virtual my_time_t starts() const
  { return m_starts; }

  virtual void set_starts(my_time_t starts)
  { m_starts= starts; }

  virtual void set_starts_null(bool is_null)
  { m_is_starts_null= is_null; }

  virtual bool is_starts_null() const
  { return m_is_starts_null; }

  /////////////////////////////////////////////////////////////////////////
  // ends.
  /////////////////////////////////////////////////////////////////////////

  virtual my_time_t ends() const
  { return m_ends; }

  virtual void set_ends(my_time_t ends)
  { m_ends= ends; }

  virtual void set_ends_null(bool is_null)
  { m_is_ends_null= is_null; }

  virtual bool is_ends_null() const
  { return m_is_ends_null; }

  /////////////////////////////////////////////////////////////////////////
  // event_status
  /////////////////////////////////////////////////////////////////////////

  virtual enum_event_status event_status() const
  { return m_event_status; }

  virtual void set_event_status(enum_event_status event_status)
  { m_event_status= event_status; }

  virtual void set_event_status_null(bool is_null)
  { m_is_event_status_null= is_null; }

  virtual bool is_event_status_null() const
  { return m_is_event_status_null; }

  /////////////////////////////////////////////////////////////////////////
  // on_completion
  /////////////////////////////////////////////////////////////////////////

  virtual enum_on_completion on_completion() const
  { return m_on_completion; }

  virtual void set_on_completion(enum_on_completion on_completion)
  { m_on_completion= on_completion; }

  /////////////////////////////////////////////////////////////////////////
  // created.
  /////////////////////////////////////////////////////////////////////////

  virtual ulonglong created() const
  { return m_created; }

  virtual void set_created(ulonglong created)
  { m_created= created; }

  /////////////////////////////////////////////////////////////////////////
  // last altered.
  /////////////////////////////////////////////////////////////////////////

  virtual ulonglong last_altered() const
  { return m_last_altered; }

  virtual void set_last_altered(ulonglong last_altered)
  { m_last_altered= last_altered; }

  /////////////////////////////////////////////////////////////////////////
  // last_executed.
  /////////////////////////////////////////////////////////////////////////

  virtual my_time_t last_executed() const
  { return m_last_executed; }

  virtual void set_last_executed(my_time_t last_executed)
  { m_last_executed= last_executed; }

  virtual void set_last_executed_null(bool is_null)
  { m_is_last_executed_null= is_null; }

  virtual bool is_last_executed_null() const
  { return m_is_last_executed_null; }

  /////////////////////////////////////////////////////////////////////////
  // comment.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &comment() const
  { return m_comment; }

  virtual void set_comment(const std::string &comment)
  { m_comment= comment; }

  /////////////////////////////////////////////////////////////////////////
  // originator
  /////////////////////////////////////////////////////////////////////////

  virtual ulonglong originator() const
  { return m_originator; }

  virtual void set_originator(ulonglong originator)
  { m_originator= originator; }

  /////////////////////////////////////////////////////////////////////////
  // collation.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id client_collation_id() const
  { return m_client_collation_id; }

  virtual void set_client_collation_id(Object_id client_collation_id)
  { m_client_collation_id= client_collation_id; }

  virtual Object_id connection_collation_id() const
  { return m_connection_collation_id; }

  virtual void set_connection_collation_id(Object_id connection_collation_id)
  { m_connection_collation_id= connection_collation_id; }

  virtual Object_id schema_collation_id() const
  { return m_schema_collation_id; }

  virtual void set_schema_collation_id(Object_id schema_collation_id)
  { m_schema_collation_id= schema_collation_id; }

  // Fix "inherits ... via dominance" warnings
  virtual Weak_object_impl *impl()
  { return Weak_object_impl::impl(); }
  virtual const Weak_object_impl *impl() const
  { return Weak_object_impl::impl(); }
  virtual Object_id id() const
  { return Entity_object_impl::id(); }
  virtual bool is_persistent() const
  { return Entity_object_impl::is_persistent(); }
  virtual const std::string &name() const
  { return Entity_object_impl::name(); }
  virtual void set_name(const std::string &name)
  { Entity_object_impl::set_name(name); }

private:
  enum_interval_field m_interval_field;
  enum_event_status   m_event_status;
  enum_on_completion  m_on_completion;

  ulonglong m_sql_mode;
  ulonglong m_created;
  ulonglong m_last_altered;
  ulonglong m_originator;
  uint      m_interval_value;

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

  std::string m_time_zone;
  std::string m_definition;
  std::string m_definition_utf8;
  std::string m_definer_user;
  std::string m_definer_host;
  std::string m_comment;

  // References.

  Object_id m_schema_id;
  Object_id m_client_collation_id;
  Object_id m_connection_collation_id;
  Object_id m_schema_collation_id;

  Event *clone() const
  {
    return new Event_impl(*this);
  }
};

///////////////////////////////////////////////////////////////////////////

class Event_type : public Object_type
{
public:
  virtual void register_tables(Open_dictionary_tables_ctx *otx) const;

  virtual Weak_object *create_object() const
  { return new (std::nothrow) Event_impl(); }
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__EVENT_IMPL_INCLUDED
