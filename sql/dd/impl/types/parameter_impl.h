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

#ifndef DD__PARAMETER_IMPL_INCLUDED
#define DD__PARAMETER_IMPL_INCLUDED

#include "my_global.h"

#include "dd/properties.h"                    // dd::Properties
#include "dd/impl/collection_item.h"          // dd::Collection_item
#include "dd/impl/types/entity_object_impl.h" // dd::Entity_object_impl
#include "dd/types/object_type.h"             // dd::Object_type
#include "dd/types/parameter.h"               // dd::Parameter

#include <memory>   // std::unique_ptr

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Routine;
class Routine_impl;
class Parameter_type_element;
template <typename T> class Collection;

///////////////////////////////////////////////////////////////////////////

class Parameter_impl : public Entity_object_impl,
                       public Parameter,
                       public Collection_item
{
public:
  typedef Collection<Parameter_type_element> Parameter_type_element_collection;

  Parameter_impl();

  virtual ~Parameter_impl()
  { }

public:
  virtual const Object_table &object_table() const
  { return Parameter::OBJECT_TABLE(); }

  virtual bool validate() const;

  virtual bool restore_children(Open_dictionary_tables_ctx *otx);

  virtual bool store_children(Open_dictionary_tables_ctx *otx);

  virtual bool drop_children(Open_dictionary_tables_ctx *otx) const;

  virtual bool store_attributes(Raw_record *r);

  virtual bool restore_attributes(const Raw_record &r);

  virtual void debug_print(std::string &outb) const;

public:
  // Required by Collection_item.
  virtual bool store(Open_dictionary_tables_ctx *otx)
  { return Weak_object_impl::store(otx); }

  // Required by Collection_item.
  virtual bool drop(Open_dictionary_tables_ctx *otx) const
  { return Weak_object_impl::drop(otx); }

  virtual void drop();

  // Required by Collection_item.
  virtual void set_ordinal_position(uint ordinal_position)
  { m_ordinal_position= ordinal_position; }

  // Required by Collection_item.
  virtual bool is_hidden() const
  { return false; }

public:
  /////////////////////////////////////////////////////////////////////////
  // Name is nullable in case of function return type.
  /////////////////////////////////////////////////////////////////////////

  virtual void set_name_null(bool is_null)
  { m_is_name_null= is_null; }

  virtual bool is_name_null() const
  { return m_is_name_null; }

  /////////////////////////////////////////////////////////////////////////
  // ordinal_position - Also used by Collection_item
  /////////////////////////////////////////////////////////////////////////

  virtual uint ordinal_position() const
  { return m_ordinal_position; }

  /////////////////////////////////////////////////////////////////////////
  // parameter_mode.
  /////////////////////////////////////////////////////////////////////////

  virtual enum_parameter_mode mode() const
  { return m_parameter_mode; }

  virtual void set_mode(enum_parameter_mode mode)
  { m_parameter_mode= mode; }

  virtual void set_parameter_mode_null(bool is_null)
  { m_parameter_mode_null= is_null; }

  virtual bool is_parameter_mode_null() const
  { return m_parameter_mode_null; }

  /////////////////////////////////////////////////////////////////////////
  // parameter_mode.
  /////////////////////////////////////////////////////////////////////////

  virtual enum_column_types data_type() const
  { return m_data_type; }

  virtual void set_data_type(enum_column_types type)
  { m_data_type= type; }

  /////////////////////////////////////////////////////////////////////////
  // is_zerofill.
  /////////////////////////////////////////////////////////////////////////

  virtual bool is_zerofill() const
  { return m_is_zerofill; }

  virtual void set_zerofill(bool zerofill)
  { m_is_zerofill= zerofill; }

  /////////////////////////////////////////////////////////////////////////
  // is_unsigned.
  /////////////////////////////////////////////////////////////////////////

  virtual bool is_unsigned() const
  { return m_is_unsigned; }

  virtual void set_unsigned(bool unsigned_flag)
  { m_is_unsigned= unsigned_flag; }

  /////////////////////////////////////////////////////////////////////////
  // char_length.
  /////////////////////////////////////////////////////////////////////////

  virtual size_t char_length() const
  { return m_char_length; }

  virtual void set_char_length(size_t char_length)
  { m_char_length= char_length; }

  /////////////////////////////////////////////////////////////////////////
  // numeric_precision.
  /////////////////////////////////////////////////////////////////////////

  virtual uint numeric_precision() const
  { return m_numeric_precision; }

  virtual void set_numeric_precision(uint numeric_precision)
  { m_numeric_precision= numeric_precision; }

  /////////////////////////////////////////////////////////////////////////
  // numeric_scale.
  /////////////////////////////////////////////////////////////////////////

  virtual uint numeric_scale() const
  { return m_numeric_scale; }

  virtual void set_numeric_scale(uint numeric_scale)
  {
    m_numeric_scale_null= false;
    m_numeric_scale= numeric_scale;
  }

  virtual void set_numeric_scale_null(bool is_null)
  { m_numeric_scale_null= is_null; }

  virtual bool is_numeric_scale_null() const
  { return m_numeric_scale_null; }

  /////////////////////////////////////////////////////////////////////////
  // datetime_precision.
  /////////////////////////////////////////////////////////////////////////

  virtual uint datetime_precision() const
  { return m_datetime_precision; }

  virtual void set_datetime_precision(uint datetime_precision)
  { m_datetime_precision= datetime_precision; }

  /////////////////////////////////////////////////////////////////////////
  // collation.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id collation_id() const
  { return m_collation_id; }

  virtual void set_collation_id(Object_id collation_id)
  { m_collation_id= collation_id; }

  /////////////////////////////////////////////////////////////////////////
  // Options.
  /////////////////////////////////////////////////////////////////////////

  using Parameter::options;

  virtual Properties &options()
  { return *m_options; }

  virtual bool set_options_raw(const std::string &options_raw);

  /////////////////////////////////////////////////////////////////////////
  // routine.
  /////////////////////////////////////////////////////////////////////////

  using Parameter::routine;

  virtual Routine &routine();

  /////////////////////////////////////////////////////////////////////////
  // Enum-elements.
  /////////////////////////////////////////////////////////////////////////

  virtual Parameter_type_element *add_enum_element();

  virtual Parameter_type_element_const_iterator *enum_elements() const;

  virtual Parameter_type_element_iterator *enum_elements();

  virtual size_t enum_elements_count() const;

  /////////////////////////////////////////////////////////////////////////
  // Set-elements.
  /////////////////////////////////////////////////////////////////////////

  virtual Parameter_type_element *add_set_element();

  virtual Parameter_type_element_const_iterator *set_elements() const;

  virtual Parameter_type_element_iterator *set_elements();

  virtual size_t set_elements_count() const;

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

public:
  class Factory : public Collection_item_factory
  {
  public:
    Factory(Routine_impl *routine)
     :m_rt(routine)
    { }

    virtual Collection_item *create_item() const;

  private:
    Routine_impl *m_rt;
  };

private:
  // Fields
  bool m_is_name_null;

  enum_parameter_mode m_parameter_mode;
  bool m_parameter_mode_null;
  enum_column_types m_data_type;

  bool m_is_zerofill;
  bool m_is_unsigned;

  uint m_ordinal_position;
  size_t m_char_length;
  uint m_numeric_precision;
  uint m_numeric_scale;
  bool m_numeric_scale_null;
  uint m_datetime_precision;

  std::unique_ptr<Parameter_type_element_collection> m_enum_elements;
  std::unique_ptr<Parameter_type_element_collection> m_set_elements;

  std::unique_ptr<Properties> m_options;

  // References to other tightly coupled objects
  Routine_impl *m_routine;

  // References to loosely-coupled objects.

  Object_id m_collation_id;

  Parameter_impl(const Parameter_impl &src, Routine_impl *parent);

public:
  Parameter_impl *clone(Routine_impl *parent) const
  {
    return new Parameter_impl(*this, parent);
  }
};

///////////////////////////////////////////////////////////////////////////

class Parameter_type : public Object_type
{
public:
  virtual void register_tables(Open_dictionary_tables_ctx *otx) const;

  virtual Weak_object *create_object() const
  { return new (std::nothrow) Parameter_impl(); }
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__PARAMETER_IMPL_INCLUDED
