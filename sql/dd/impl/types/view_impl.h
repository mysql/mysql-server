/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD__VIEW_IMPL_INCLUDED
#define DD__VIEW_IMPL_INCLUDED

#include <sys/types.h>
#include <new>
#include <string>

#include "dd/impl/raw/raw_record.h"
#include "dd/impl/types/abstract_table_impl.h" // dd::Abstract_table_impl
#include "dd/impl/types/entity_object_impl.h"
#include "dd/impl/types/weak_object_impl.h"
#include "dd/object_id.h"
#include "dd/types/abstract_table.h"
#include "dd/types/dictionary_object_table.h"  // dd::Dictionary_object_table
#include "dd/types/object_type.h"
#include "dd/types/view.h"                     // dd::View
#include "dd/types/view_routine.h"             // dd::View_routine
#include "dd/types/view_table.h"               // dd::View_table
#include "my_inttypes.h"

namespace dd {
class Column;
class Open_dictionary_tables_ctx;
class Properties;
class View_routine;
class View_table;
class Weak_object;
}  // namespace dd

typedef struct charset_info_st CHARSET_INFO;

namespace dd {

///////////////////////////////////////////////////////////////////////////

class View_impl : public Abstract_table_impl,
                  public View
{
public:
  View_impl();

  virtual ~View_impl()
  { }

public:
  virtual const Dictionary_object_table &object_table() const
  { return View::OBJECT_TABLE(); }

  virtual bool validate() const;

  virtual bool restore_children(Open_dictionary_tables_ctx *otx);

  virtual bool store_children(Open_dictionary_tables_ctx *otx);

  virtual bool drop_children(Open_dictionary_tables_ctx *otx) const;

  virtual void remove_children();

  virtual bool restore_attributes(const Raw_record &r);

  virtual bool store_attributes(Raw_record *r);

  virtual void debug_print(String_type &outb) const;

public:
  /////////////////////////////////////////////////////////////////////////
  // enum_table_type.
  /////////////////////////////////////////////////////////////////////////

  virtual enum_table_type type() const
  { return m_type; }

  /////////////////////////////////////////////////////////////////////////
  // regular/system view flag.
  /////////////////////////////////////////////////////////////////////////

  virtual void set_system_view(bool system_view)
  {
    m_type= system_view ?
      enum_table_type::SYSTEM_VIEW :
      enum_table_type::USER_VIEW;
  }

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

  /////////////////////////////////////////////////////////////////////////
  // definition/utf8.
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &definition() const
  { return m_definition; }

  virtual void set_definition(const String_type &definition)
  { m_definition= definition; }

  virtual const String_type &definition_utf8() const
  { return m_definition_utf8; }

  virtual void set_definition_utf8(const String_type &definition_utf8)
  { m_definition_utf8= definition_utf8; }

  /////////////////////////////////////////////////////////////////////////
  // check_option.
  /////////////////////////////////////////////////////////////////////////

  virtual enum_check_option check_option() const
  { return m_check_option; }

  virtual void set_check_option(enum_check_option check_option)
  { m_check_option= check_option; }

  /////////////////////////////////////////////////////////////////////////
  // is_updatable.
  /////////////////////////////////////////////////////////////////////////

  virtual bool is_updatable() const
  { return m_is_updatable; }

  virtual void set_updatable(bool updatable)
  { m_is_updatable= updatable; }

  /////////////////////////////////////////////////////////////////////////
  // algorithm.
  /////////////////////////////////////////////////////////////////////////

  virtual enum_algorithm algorithm() const
  { return m_algorithm; }

  virtual void set_algorithm(enum_algorithm algorithm)
  { m_algorithm= algorithm; }

  /////////////////////////////////////////////////////////////////////////
  // security_type.
  /////////////////////////////////////////////////////////////////////////

  virtual enum_security_type security_type() const
  { return m_security_type; }

  virtual void set_security_type(enum_security_type security_type)
  { m_security_type= security_type; }

  /////////////////////////////////////////////////////////////////////////
  // definer.
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &definer_user() const
  { return m_definer_user; }

  virtual const String_type &definer_host() const
  { return m_definer_host; }

  virtual void set_definer(const String_type &username,
                           const String_type &hostname)
  {
    m_definer_user= username;
    m_definer_host= hostname;
  }

  /////////////////////////////////////////////////////////////////////////
  // Explicit list of column names.
  /////////////////////////////////////////////////////////////////////////

  virtual const Properties &column_names() const
  { return *m_column_names; }

  virtual Properties &column_names()
  { return *m_column_names; }

  /////////////////////////////////////////////////////////////////////////
  // View_table collection.
  /////////////////////////////////////////////////////////////////////////

  virtual View_table *add_table();

  virtual const View_tables &tables() const
  { return m_tables; }

  /////////////////////////////////////////////////////////////////////////
  // View_routine collection.
  /////////////////////////////////////////////////////////////////////////

  virtual View_routine *add_routine();

  virtual const View_routines &routines() const
  { return m_routines; }

  // Fix "inherits ... via dominance" warnings
  virtual Weak_object_impl *impl()
  { return Weak_object_impl::impl(); }
  virtual const Weak_object_impl *impl() const
  { return Weak_object_impl::impl(); }
  virtual Object_id id() const
  { return Entity_object_impl::id(); }
  virtual bool is_persistent() const
  { return Entity_object_impl::is_persistent(); }
  virtual const String_type &name() const
  { return Entity_object_impl::name(); }
  virtual void set_name(const String_type &name)
  { Entity_object_impl::set_name(name); }
  virtual Object_id schema_id() const
  { return Abstract_table_impl::schema_id(); }
  virtual void set_schema_id(Object_id schema_id)
  { Abstract_table_impl::set_schema_id(schema_id); }
  virtual uint mysql_version_id() const
  { return Abstract_table_impl::mysql_version_id(); }
  virtual const Properties &options() const
  { return Abstract_table_impl::options(); }
  virtual Properties &options()
  { return Abstract_table_impl::options(); }
  virtual bool set_options_raw(const String_type &options_raw)
  { return Abstract_table_impl::set_options_raw(options_raw); }
  virtual bool set_column_names_raw(const String_type &column_names_raw);
  virtual ulonglong created() const
  { return Abstract_table_impl::created(); }
  virtual void set_created(ulonglong created)
  { Abstract_table_impl::set_created(created); }
  virtual ulonglong last_altered() const
  { return Abstract_table_impl::last_altered(); }
  virtual void set_last_altered(ulonglong last_altered)
  { Abstract_table_impl::set_last_altered(last_altered); }
  virtual Column *add_column()
  { return Abstract_table_impl::add_column(); }
  virtual const Column_collection &columns() const
  { return Abstract_table_impl::columns(); }
  virtual Column_collection *columns()
  { return Abstract_table_impl::columns(); }
  const Column *get_column(const String_type name) const
  { return Abstract_table_impl::get_column(name); }
  Column *get_column(const String_type name)
  { return Abstract_table_impl::get_column(name); }
  virtual bool hidden() const
  { return Abstract_table_impl::hidden(); }
  virtual void set_hidden(bool hidden)
  { Abstract_table_impl::set_hidden(hidden); }

private:
  enum_table_type    m_type;
  bool               m_is_updatable;
  enum_check_option  m_check_option;
  enum_algorithm     m_algorithm;
  enum_security_type m_security_type;

  String_type m_definition;
  String_type m_definition_utf8;
  String_type m_definer_user;
  String_type m_definer_host;

  std::unique_ptr<Properties> m_column_names;

  // Collections.

  View_tables   m_tables;
  View_routines m_routines;

  // References.

  Object_id m_client_collation_id;
  Object_id m_connection_collation_id;

  View_impl(const View_impl &src);
  View_impl *clone() const
  {
    return new View_impl(*this);
  }
};

///////////////////////////////////////////////////////////////////////////

class View_type : public Object_type
{
public:
  virtual void register_tables(Open_dictionary_tables_ctx *otx) const;

  virtual Weak_object *create_object() const
  { return new (std::nothrow) View_impl(); }
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__VIEW_IMPL_INCLUDED
