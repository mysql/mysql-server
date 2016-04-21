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

#ifndef DD__PROCEDURE_IMPL_INCLUDED
#define DD__PROCEDURE_IMPL_INCLUDED

#include "my_global.h"

#include "dd/impl/types/routine_impl.h"        // dd::Routine_impl
#include "dd/types/object_type.h"              // dd::Object_type
#include "dd/types/procedure.h"                // dd::Procedure

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Procedure_impl : public Routine_impl,
                       public Procedure
{
public:
  Procedure_impl()
  { }

  virtual ~Procedure_impl()
  { }

public:

  virtual bool update_routine_name_key(name_key_type *key,
                                       Object_id schema_id,
                                       const std::string &name) const;

  virtual void debug_print(std::string &outb) const;

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
  virtual const Dictionary_object_table &object_table() const
  { return Routine_impl::object_table(); }
  virtual Object_id schema_id() const
  { return Routine_impl::schema_id(); }
  virtual void set_schema_id(Object_id schema_id)
  { Routine_impl::set_schema_id(schema_id); }
  virtual enum_routine_type type() const
  { return Routine_impl::type(); }
  virtual const std::string &definition() const
  { return Routine_impl::definition(); }
  virtual void set_definition(const std::string &definition)
  { Routine_impl::set_definition(definition); }
  virtual const std::string &definition_utf8() const
  { return Routine_impl::definition_utf8(); }
  virtual void set_definition_utf8(const std::string &definition_utf8)
  { Routine_impl::set_definition_utf8(definition_utf8); }
  virtual const std::string &parameter_str() const
  { return Routine_impl::parameter_str(); }
  virtual void set_parameter_str(const std::string &parameter_str)
  { Routine_impl::set_parameter_str(parameter_str); }
  virtual bool is_deterministic() const
  { return Routine_impl::is_deterministic(); }
  virtual void set_deterministic(bool deterministic)
  { Routine_impl::set_deterministic(deterministic); }
  virtual enum_sql_data_access sql_data_access() const
  { return Routine_impl::sql_data_access(); }
  virtual void set_sql_data_access(enum_sql_data_access sda)
  { Routine_impl::set_sql_data_access(sda); }
  virtual View::enum_security_type security_type() const
  { return Routine_impl::security_type(); }
  virtual void set_security_type(View::enum_security_type security_type)
  { Routine_impl::set_security_type(security_type); }
  virtual ulonglong sql_mode() const
  { return Routine_impl::sql_mode(); }
  virtual void set_sql_mode(ulonglong sm)
  { Routine_impl::set_sql_mode(sm); }
  virtual const std::string &definer_user() const
  { return Routine_impl::definer_user(); }
  virtual const std::string &definer_host() const
  { return Routine_impl::definer_host(); }
  virtual void set_definer(const std::string &username,
                           const std::string &hostname)
  { Routine_impl::set_definer(username, hostname); }
  virtual Object_id client_collation_id() const
  { return Routine_impl::client_collation_id(); }
  virtual void set_client_collation_id(Object_id client_collation_id)
  { Routine_impl::set_client_collation_id(client_collation_id); }
  virtual Object_id connection_collation_id() const
  { return Routine_impl::connection_collation_id(); }
  virtual void set_connection_collation_id(Object_id connection_collation_id)
  { Routine_impl::set_connection_collation_id(connection_collation_id); }
  virtual Object_id schema_collation_id() const
  { return Routine_impl::schema_collation_id(); }
  virtual void set_schema_collation_id(Object_id schema_collation_id)
  { Routine_impl::set_schema_collation_id(schema_collation_id); }
  virtual ulonglong created() const
  { return Routine_impl::created(); }
  virtual void set_created(ulonglong created)
  { Routine_impl::set_created(created); }
  virtual ulonglong last_altered() const
  { return Routine_impl::last_altered(); }
  virtual void set_last_altered(ulonglong last_altered)
  { Routine_impl::set_last_altered(last_altered); }
  virtual const std::string &comment() const
  { return Routine_impl::comment(); }
  virtual void set_comment(const std::string &comment)
  { Routine_impl::set_comment(comment); }
  virtual Parameter *add_parameter()
  { return Routine_impl::add_parameter(); }
  virtual Parameter_const_iterator *parameters() const
  { return Routine_impl::parameters(); }
  virtual Parameter_iterator *parameters()
  { return Routine_impl::parameters(); }
  virtual bool update_name_key(name_key_type *key) const
  { return Routine::update_name_key(key); }

private:
  Procedure_impl(const Procedure_impl &src);
  Procedure_impl *clone() const
  {
    return new Procedure_impl(*this);
  }
};

///////////////////////////////////////////////////////////////////////////

class Procedure_type : public Object_type
{
public:
  virtual void register_tables(Open_dictionary_tables_ctx *otx) const;

  virtual Weak_object *create_object() const
  { return new (std::nothrow) Procedure_impl(); }
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__PROCEDURE_IMPL_INCLUDED
