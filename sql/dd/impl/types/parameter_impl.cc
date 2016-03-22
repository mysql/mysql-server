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

#include "dd/impl/types/parameter_impl.h"

#include "dd/impl/collection_impl.h"                  // Collection
#include "dd/impl/properties_impl.h"                  // Properties_impl
#include "dd/impl/transaction_impl.h"                 // Open_dictionary_tables_ctx
#include "dd/impl/raw/raw_record.h"                   // Raw_record
#include "dd/impl/tables/parameters.h"                // Parameters
#include "dd/impl/tables/parameter_type_elements.h"   // Parameter_type_elements
#include "dd/impl/types/parameter_type_element_impl.h"// Parameter_type_element_impl
#include "dd/impl/types/routine_impl.h"               // Routine_impl

#include <sstream>

using dd::tables::Parameters;
using dd::tables::Parameter_type_elements;

namespace dd {

///////////////////////////////////////////////////////////////////////////
// Parameter implementation.
///////////////////////////////////////////////////////////////////////////

const Object_table &Parameter::OBJECT_TABLE()
{
  return Parameters::instance();
}

///////////////////////////////////////////////////////////////////////////

const Object_type &Parameter::TYPE()
{
  static Parameter_type s_instance;
  return s_instance;
}

///////////////////////////////////////////////////////////////////////////
// Parameter_impl implementation.
///////////////////////////////////////////////////////////////////////////

Parameter_impl::Parameter_impl()
 :m_is_name_null(false),
  m_parameter_mode(PM_IN),
  m_parameter_mode_null(false),
  m_data_type(enum_column_types::LONG),
  m_is_zerofill(false),
  m_is_unsigned(false),
  m_ordinal_position(0),
  m_char_length(0),
  m_numeric_precision(0),
  m_numeric_scale(0),
  m_numeric_scale_null(true),
  m_datetime_precision(0),
  m_enum_elements(new Parameter_type_element_collection()),
  m_set_elements(new Parameter_type_element_collection()),
  m_options(new Properties_impl()),
  m_routine(NULL),
  m_collation_id(INVALID_OBJECT_ID)
{ }

///////////////////////////////////////////////////////////////////////////

/* purecov: begin deadcode */
Routine &Parameter_impl::routine()
{
  return *m_routine;
}
/* purecov: end */

///////////////////////////////////////////////////////////////////////////

bool Parameter_impl::set_options_raw(const std::string &options_raw)
{
  Properties *properties=
    Properties_impl::parse_properties(options_raw);

  if (!properties)
    return true; // Error status, current values has not changed.

  m_options.reset(properties);
  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Parameter_impl::validate() const
{
  if (!m_routine)
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             Parameter_impl::OBJECT_TABLE().name().c_str(),
             "Parameter does not belong to any routine.");
    return true;
  }

  if (m_collation_id == INVALID_OBJECT_ID)
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             Parameter_impl::OBJECT_TABLE().name().c_str(),
             "Collation ID is not set");
    return true;
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Parameter_impl::restore_children(Open_dictionary_tables_ctx *otx)
{
  switch (data_type())
  {
  case enum_column_types::ENUM:
      return
        m_enum_elements->restore_items(
          Parameter_type_element_impl::Factory(this, m_enum_elements.get()),
          otx,
          otx->get_table<Parameter_type_element>(),
          Parameter_type_elements::create_key_by_parameter_id(this->id()));

  case enum_column_types::SET:
      return
        m_set_elements->restore_items(
          Parameter_type_element_impl::Factory(this, m_set_elements.get()),
          otx,
          otx->get_table<Parameter_type_element>(),
          Parameter_type_elements::create_key_by_parameter_id(this->id()));

  default:
    return false;
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Parameter_impl::store_children(Open_dictionary_tables_ctx *otx)
{
  return m_enum_elements->store_items(otx) ||
         m_set_elements->store_items(otx);
}

///////////////////////////////////////////////////////////////////////////

bool Parameter_impl::drop_children(Open_dictionary_tables_ctx *otx) const
{
  if (data_type() == enum_column_types::ENUM ||
      data_type() == enum_column_types::SET)
    return m_enum_elements->drop_items(
             otx,
             otx->get_table<Parameter_type_element>(),
             Parameter_type_elements::create_key_by_parameter_id(this->id()))
           ||
           m_set_elements->drop_items(
             otx,
             otx->get_table<Parameter_type_element>(),
             Parameter_type_elements::create_key_by_parameter_id(this->id()));

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Parameter_impl::restore_attributes(const Raw_record &r)
{
  check_parent_consistency(m_routine, r.read_ref_id(Parameters::FIELD_ROUTINE_ID));

  restore_id(r, Parameters::FIELD_ID);
  restore_name(r, Parameters::FIELD_NAME);
  m_is_name_null= r.is_null(Parameters::FIELD_NAME);

  m_is_zerofill=       r.read_bool(Parameters::FIELD_IS_ZEROFILL);
  m_is_unsigned=       r.read_bool(Parameters::FIELD_IS_UNSIGNED);

  m_parameter_mode= (enum_parameter_mode) r.read_int(Parameters::FIELD_MODE);
  m_parameter_mode_null= r.is_null(Parameters::FIELD_MODE);

  m_data_type= (enum_column_types) r.read_int(Parameters::FIELD_DATA_TYPE);

  m_ordinal_position=        r.read_uint(Parameters::FIELD_ORDINAL_POSITION);
  m_char_length=             r.read_uint(Parameters::FIELD_CHAR_LENGTH);
  m_numeric_precision=       r.read_uint(Parameters::FIELD_NUMERIC_PRECISION);
  m_numeric_scale=           r.read_uint(Parameters::FIELD_NUMERIC_SCALE);
  m_numeric_scale_null=      r.is_null(Parameters::FIELD_NUMERIC_SCALE);
  m_datetime_precision=      r.read_uint(Parameters::FIELD_DATETIME_PRECISION);

  m_collation_id= r.read_ref_id(Parameters::FIELD_COLLATION_ID);

  set_options_raw(r.read_str(Parameters::FIELD_OPTIONS, ""));

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Parameter_impl::store_attributes(Raw_record *r)
{
  return
    store_id(r, Parameters::FIELD_ID) ||
    store_name(r, Parameters::FIELD_NAME, m_is_name_null) ||
    r->store(Parameters::FIELD_ROUTINE_ID, m_routine->id()) ||
    r->store(Parameters::FIELD_ORDINAL_POSITION, m_ordinal_position) ||
    r->store(Parameters::FIELD_MODE,
             m_parameter_mode,
             m_parameter_mode_null) ||
    r->store(Parameters::FIELD_DATA_TYPE, static_cast<int>(m_data_type)) ||
    r->store(Parameters::FIELD_IS_ZEROFILL, m_is_zerofill) ||
    r->store(Parameters::FIELD_IS_UNSIGNED, m_is_unsigned) ||
    r->store(Parameters::FIELD_CHAR_LENGTH, static_cast<uint>(m_char_length)) ||
    r->store(Parameters::FIELD_NUMERIC_PRECISION, m_numeric_precision) ||
    r->store(Parameters::FIELD_NUMERIC_SCALE, m_numeric_scale, m_numeric_scale_null) ||
    r->store(Parameters::FIELD_DATETIME_PRECISION, m_datetime_precision) ||
    r->store_ref_id(Parameters::FIELD_COLLATION_ID, m_collation_id) ||
    r->store(Parameters::FIELD_OPTIONS, *m_options);
}

///////////////////////////////////////////////////////////////////////////

void Parameter_impl::debug_print(std::string &outb) const
{
  std::stringstream ss;
  ss
    << "PARAMETER OBJECT: { "
    << "m_id: {OID: " << id() << "}; "
    << "m_routine_id: {OID: " << m_routine->id() << "}; "
    << "m_name: " << name() << "; "
    << "m_is_name_null: " << m_is_name_null << "; "
    << "m_ordinal_position: " << m_ordinal_position << "; "
    << "m_parameter_mode: " << m_parameter_mode << "; "
    << "m_parameter_mode_null: " << m_parameter_mode_null << "; "
    << "m_data_type: " << static_cast<int>(m_data_type) << "; "
    << "m_is_zerofill: " << m_is_zerofill << "; "
    << "m_is_unsigned: " << m_is_unsigned << "; "
    << "m_char_length: " << m_char_length << "; "
    << "m_numeric_precision: " << m_numeric_precision << "; "
    << "m_numeric_scale: " << m_numeric_scale << "; "
    << "m_numeric_scale_null: " << m_numeric_scale_null << "; "
    << "m_datetime_precision: " << m_datetime_precision << "; "
    << "m_collation_id: {OID: " << m_collation_id << "}; "
    << "m_options: " << m_options->raw_string() << "; ";

  if (data_type() == enum_column_types::ENUM)
  {
    /* purecov: begin inspected */
    ss << "m_enum_elements: [ ";

    std::unique_ptr<Parameter_type_element_const_iterator> it(enum_elements());

    while (true)
    {
      const Parameter_type_element *e= it->next();

      if (!e)
        break;

      std::string ob;
      e->debug_print(ob);
      ss << ob;
    }

    ss << " ]";
    /* purecov: end */
  }
  else if(data_type() == enum_column_types::SET)
  {
    /* purecov: begin inspected */
    ss << "m_set_elements: [ ";

    std::unique_ptr<Parameter_type_element_const_iterator> it(set_elements());

    while (true)
    {
      const Parameter_type_element *e= it->next();

      if (!e)
        break;

      std::string ob;
      e->debug_print(ob);
      ss << ob;
    }

    ss << " ]";
    /* purecov: end */
  }

  outb= ss.str();
}

///////////////////////////////////////////////////////////////////////////

/* purecov: begin deadcode */
void Parameter_impl::drop()
{
  m_routine->parameter_collection()->remove(this);
}
/* purecov: end */

///////////////////////////////////////////////////////////////////////////
// Enum-elements.
///////////////////////////////////////////////////////////////////////////

Parameter_type_element *Parameter_impl::add_enum_element()
{
  if (data_type() != enum_column_types::ENUM)
    return NULL;

  return
    m_enum_elements->add(
      Parameter_type_element_impl::Factory(this, m_enum_elements.get()));
}

///////////////////////////////////////////////////////////////////////////

Parameter_type_element_const_iterator *Parameter_impl::enum_elements() const
{
  return data_type() == enum_column_types::ENUM ?
                          m_enum_elements->const_iterator() : NULL;
}

///////////////////////////////////////////////////////////////////////////

/* purecov: begin deadcode */
Parameter_type_element_iterator *Parameter_impl::enum_elements()
{
  return data_type() == enum_column_types::ENUM ?
                          m_enum_elements->iterator() : NULL;
}
/* purecov: end */

///////////////////////////////////////////////////////////////////////////

size_t Parameter_impl::enum_elements_count() const
{
  return m_enum_elements->size();
}

///////////////////////////////////////////////////////////////////////////
// Set-elements.
///////////////////////////////////////////////////////////////////////////

Parameter_type_element *Parameter_impl::add_set_element()
{
  if (data_type() != enum_column_types::SET)
    return NULL;

  return
    m_set_elements->add(
      Parameter_type_element_impl::Factory(this, m_set_elements.get()));
}

///////////////////////////////////////////////////////////////////////////

Parameter_type_element_const_iterator *Parameter_impl::set_elements() const
{
  return data_type() == enum_column_types::SET ?
                          m_set_elements->const_iterator() : NULL;
}

///////////////////////////////////////////////////////////////////////////

/* purecov: begin deadcode */
Parameter_type_element_iterator *Parameter_impl::set_elements()
{
  return data_type() == enum_column_types::SET ?
                          m_set_elements->iterator() : NULL;
}
/* purecov: end */

///////////////////////////////////////////////////////////////////////////

size_t Parameter_impl::set_elements_count() const
{
  return m_set_elements->size();
}

///////////////////////////////////////////////////////////////////////////

Collection_item *Parameter_impl::Factory::create_item() const
{
  Parameter_impl *p= new (std::nothrow) Parameter_impl();
  p->m_routine= m_rt;
  return p;
}

///////////////////////////////////////////////////////////////////////////

Parameter_impl::Parameter_impl(const Parameter_impl &src,
                               Routine_impl *parent)
  : Weak_object(src), Entity_object_impl(src),
    m_is_name_null(src.m_is_name_null),
    m_parameter_mode(src.m_parameter_mode),
    m_parameter_mode_null(src.m_parameter_mode_null),
    m_data_type(src.m_data_type),
    m_is_zerofill(src.m_is_zerofill),
    m_is_unsigned(src.m_is_unsigned),
    m_ordinal_position(src.m_ordinal_position),
    m_char_length(src.m_char_length),
    m_numeric_precision(src.m_numeric_precision),
    m_numeric_scale(src.m_numeric_scale),
    m_numeric_scale_null(src.m_numeric_scale_null),
    m_datetime_precision(src.m_datetime_precision),
    m_enum_elements(new Parameter_type_element_collection()),
    m_set_elements(new Parameter_type_element_collection()),
    m_options(Properties_impl::parse_properties(src.m_options->raw_string())),
    m_routine(parent),
    m_collation_id(src.m_collation_id)
{
  typedef Base_collection::Array::const_iterator i_type;
  i_type end= src.m_enum_elements->aref().end();
  m_enum_elements->aref().reserve(src.m_enum_elements->size());
  for (i_type i= src.m_enum_elements->aref().begin(); i != end; ++i)
  {
    m_enum_elements->aref().push_back(dynamic_cast<Parameter_type_element_impl*>(*i)->
                                      clone(this, m_enum_elements.get()));
  }

  end= src.m_set_elements->aref().end();
  m_set_elements->aref().reserve(src.m_set_elements->size());
  for (i_type i= src.m_set_elements->aref().begin(); i != end; ++i)
  {
    m_set_elements->aref().push_back(dynamic_cast<Parameter_type_element_impl*>(*i)->
                                     clone(this, m_set_elements.get()));
  }
}

///////////////////////////////////////////////////////////////////////////
// Parameter_type implementation.
///////////////////////////////////////////////////////////////////////////

void Parameter_type::register_tables(Open_dictionary_tables_ctx *otx) const
{
  otx->add_table<Parameters>();

  otx->register_tables<Parameter_type_element>();
}

///////////////////////////////////////////////////////////////////////////

}
