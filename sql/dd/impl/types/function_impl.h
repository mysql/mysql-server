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

class Function_impl : virtual public Routine_impl,
                      virtual public Function
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
