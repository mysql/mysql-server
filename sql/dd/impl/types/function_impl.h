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

#ifndef DD__FUNCTION_IMPL_INCLUDED
#define DD__FUNCTION_IMPL_INCLUDED

#include "my_global.h"

#include "dd/impl/types/routine_impl.h"        // dd::Routine_impl
#include "dd/types/function.h"                 // dd::Function
#include "dd/types/object_type.h"              // dd::Object_type

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Function_impl : public Routine_impl,
                      public Function
{
public:
  Function_impl();

  virtual ~Function_impl()
  { }

  virtual bool update_routine_name_key(name_key_type *key,
                                       Object_id schema_id,
                                       const std::string &name) const;

public:
  virtual bool validate() const;

  virtual bool restore_attributes(const Raw_record &r);

  virtual bool store_attributes(Raw_record *r);

  virtual void debug_print(std::string &outb) const;

public:
  /////////////////////////////////////////////////////////////////////////
  // result data type.
  /////////////////////////////////////////////////////////////////////////

  virtual enum_column_types result_data_type() const
  { return m_result_data_type; }

  virtual void set_result_data_type(enum_column_types result_data_type)
  { m_result_data_type= result_data_type; }

  virtual bool is_result_data_type_null() const
  { return m_result_data_type_null; }

  virtual void set_result_data_type_null(bool is_null)
  { m_result_data_type_null= is_null; }

  /////////////////////////////////////////////////////////////////////////
  // result_is_zerofill.
  /////////////////////////////////////////////////////////////////////////

  virtual bool result_is_zerofill() const
  { return m_result_is_zerofill; }

  virtual void set_result_zerofill(bool zerofill)
  { m_result_is_zerofill= zerofill; }

  /////////////////////////////////////////////////////////////////////////
  // result_is_unsigned.
  /////////////////////////////////////////////////////////////////////////

  virtual bool result_is_unsigned() const
  { return m_result_is_unsigned; }

  virtual void set_result_unsigned(bool unsigned_flag)
  { m_result_is_unsigned= unsigned_flag; }

  /////////////////////////////////////////////////////////////////////////
  // result_char_length.
  /////////////////////////////////////////////////////////////////////////

  virtual size_t result_char_length() const
  { return m_result_char_length; }

  virtual void set_result_char_length(size_t char_length)
  { m_result_char_length= char_length; }

  /////////////////////////////////////////////////////////////////////////
  // result_numeric_precision.
  /////////////////////////////////////////////////////////////////////////

  virtual uint result_numeric_precision() const
  { return m_result_numeric_precision; }

  virtual void set_result_numeric_precision(uint result_numeric_precision)
  { m_result_numeric_precision= result_numeric_precision; }

  /////////////////////////////////////////////////////////////////////////
  // result_numeric_scale.
  /////////////////////////////////////////////////////////////////////////

  virtual uint result_numeric_scale() const
  { return m_result_numeric_scale; }

  virtual void set_result_numeric_scale(uint result_numeric_scale)
  {
    m_result_numeric_scale_null= false;
    m_result_numeric_scale= result_numeric_scale;
  }

  virtual void set_result_numeric_scale_null(bool is_null)
  { m_result_numeric_scale_null= is_null; }

  virtual bool is_result_numeric_scale_null() const
  { return m_result_numeric_scale_null; }

  /////////////////////////////////////////////////////////////////////////
  // result_datetime_precision.
  /////////////////////////////////////////////////////////////////////////

  virtual uint result_datetime_precision() const
  { return m_result_datetime_precision; }

  virtual void set_result_datetime_precision(uint result_datetime_precision)
  { m_result_datetime_precision= result_datetime_precision; }

  /////////////////////////////////////////////////////////////////////////
  // result_collation.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id result_collation_id() const
  { return m_result_collation_id; }

  virtual void set_result_collation_id(Object_id result_collation_id)
  { m_result_collation_id= result_collation_id; }

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
  { return Function::update_name_key(key); }

private:
  enum_column_types m_result_data_type;

  bool m_result_data_type_null;
  bool m_result_is_zerofill;
  bool m_result_is_unsigned;
  bool m_result_numeric_scale_null;

  uint m_result_numeric_precision;
  uint m_result_numeric_scale;
  uint m_result_datetime_precision;

  size_t m_result_char_length;

  Object_id m_result_collation_id;

  Function_impl(const Function_impl &src);
  Function_impl *clone() const
  {
    return new Function_impl(*this);
  }
};

///////////////////////////////////////////////////////////////////////////

class Function_type : public Object_type
{
public:
  virtual void register_tables(Open_dictionary_tables_ctx *otx) const;

  virtual Weak_object *create_object() const
  { return new (std::nothrow) Function_impl(); }
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__FUNCTION_IMPL_INCLUDED
