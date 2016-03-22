/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

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

#include "dd/impl/types/column_impl.h"

#include "mysqld_error.h"                            // ER_*

#include "dd/impl/collection_impl.h"                 // Collection
#include "dd/impl/properties_impl.h"                 // Properties_impl
#include "dd/impl/sdi_impl.h"                        // sdi read/write functions
#include "dd/impl/transaction_impl.h"                // Open_dictionary_tables_ctx
#include "dd/impl/raw/raw_record.h"                  // Raw_record
#include "dd/impl/tables/columns.h"                  // Colummns
#include "dd/impl/tables/column_type_elements.h"     // Column_type_elements
#include "dd/impl/types/abstract_table_impl.h"       // Abstract_table_impl
#include "dd/impl/types/column_type_element_impl.h"  // Column_type_element_impl
#include "dd/types/column_type_element.h"            // Column_type_element

#include <memory>
#include <sstream>


using dd::tables::Columns;
using dd::tables::Column_type_elements;

namespace dd {

///////////////////////////////////////////////////////////////////////////
// Column implementation.
///////////////////////////////////////////////////////////////////////////

const Object_table &Column::OBJECT_TABLE()
{
  return Columns::instance();
}

///////////////////////////////////////////////////////////////////////////

const Object_type &Column::TYPE()
{
  static Column_type s_instance;
  return s_instance;
}

///////////////////////////////////////////////////////////////////////////
// Column_impl implementation.
///////////////////////////////////////////////////////////////////////////

Column_impl::Column_impl()
 :m_type(enum_column_types::LONG),
  m_is_nullable(true),
  m_is_zerofill(false),
  m_is_unsigned(false),
  m_is_auto_increment(false),
  m_is_virtual(false),
  m_hidden(false),
  m_ordinal_position(0),
  m_char_length(0),
  m_numeric_precision(0),
  m_numeric_scale(0),
  m_numeric_scale_null(true),
  m_datetime_precision(0),
  m_has_no_default(false),
  m_default_value_null(true),
  m_options(new Properties_impl()),
  m_se_private_data(new Properties_impl()),
  m_table(NULL),
  m_enum_elements(new Column_type_element_collection()),
  m_set_elements(new Column_type_element_collection()),
  m_collation_id(INVALID_OBJECT_ID)
{ }

///////////////////////////////////////////////////////////////////////////

Abstract_table &Column_impl::table()
{
  return *m_table;
}

///////////////////////////////////////////////////////////////////////////

bool Column_impl::set_options_raw(const std::string &options_raw)
{
  Properties *properties=
    Properties_impl::parse_properties(options_raw);

  if (!properties)
    return true; // Error status, current values has not changed.

  m_options.reset(properties);
  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Column_impl::set_se_private_data_raw( const std::string &se_private_data_raw)
{
  Properties *properties=
    Properties_impl::parse_properties(se_private_data_raw);

  if (!properties)
    return true; // Error status, current values has not changed.

  m_se_private_data.reset(properties);
  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Column_impl::validate() const
{
  if (!m_table)
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             Column_impl::OBJECT_TABLE().name().c_str(),
             "Column does not belong to any table.");
    return true;
  }

  if (m_collation_id == INVALID_OBJECT_ID)
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             Column_impl::OBJECT_TABLE().name().c_str(),
             "Collation ID is not set");
    return true;
  }

  if (type() == enum_column_types::ENUM && m_enum_elements->is_empty())
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             Column_impl::OBJECT_TABLE().name().c_str(),
             "There are no ENUM elements supplied.");
    return true;
  }
  else if (type() == enum_column_types::SET && m_set_elements->is_empty())
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             Column_impl::OBJECT_TABLE().name().c_str(),
             "There are no SET elements supplied.");
    return true;
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Column_impl::restore_children(Open_dictionary_tables_ctx *otx)
{
  switch (type())
  {
    case enum_column_types::ENUM:
      return
        m_enum_elements->restore_items(
          Column_type_element_impl::Factory(this, m_enum_elements.get()),
          otx,
          otx->get_table<Column_type_element>(),
          Column_type_elements::create_key_by_column_id(this->id()));

    case enum_column_types::SET:
      return
        m_set_elements->restore_items(
          Column_type_element_impl::Factory(this, m_set_elements.get()),
          otx,
          otx->get_table<Column_type_element>(),
          Column_type_elements::create_key_by_column_id(this->id()));

    default:
      return false;
  }
}

///////////////////////////////////////////////////////////////////////////

bool Column_impl::store_children(Open_dictionary_tables_ctx *otx)
{
  return m_enum_elements->store_items(otx) ||
         m_set_elements->store_items(otx);
}

///////////////////////////////////////////////////////////////////////////

bool Column_impl::drop_children(Open_dictionary_tables_ctx *otx) const
{
  if (type() == enum_column_types::ENUM ||
      type() == enum_column_types::SET)
    return m_enum_elements->drop_items(
             otx,
             otx->get_table<Column_type_element>(),
             Column_type_elements::create_key_by_column_id(this->id()))
           ||
           m_set_elements->drop_items(
             otx,
             otx->get_table<Column_type_element>(),
             Column_type_elements::create_key_by_column_id(this->id()));

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Column_impl::restore_attributes(const Raw_record &r)
{
  if (check_parent_consistency(m_table,
                               r.read_ref_id(Columns::FIELD_TABLE_ID)))
    return true;

  restore_id(r, Columns::FIELD_ID);
  restore_name(r, Columns::FIELD_NAME);

  m_is_nullable=       r.read_bool(Columns::FIELD_IS_NULLABLE);
  m_is_zerofill=       r.read_bool(Columns::FIELD_IS_ZEROFILL);
  m_is_unsigned=       r.read_bool(Columns::FIELD_IS_UNSIGNED);
  m_is_auto_increment= r.read_bool(Columns::FIELD_IS_AUTO_INCREMENT);
  m_hidden=            r.read_bool(Columns::FIELD_HIDDEN);

  m_type= (enum_column_types) r.read_int(Columns::FIELD_TYPE);
  m_numeric_precision=       r.read_uint(Columns::FIELD_NUMERIC_PRECISION);
  m_numeric_scale_null=      r.is_null(Columns::FIELD_NUMERIC_SCALE);
  m_numeric_scale=           r.read_uint(Columns::FIELD_NUMERIC_SCALE);
  m_datetime_precision=      r.read_uint(Columns::FIELD_DATETIME_PRECISION);
  m_ordinal_position=        r.read_uint(Columns::FIELD_ORDINAL_POSITION);
  m_char_length=             r.read_uint(Columns::FIELD_CHAR_LENGTH);

  m_has_no_default=     r.read_bool(Columns::FIELD_HAS_NO_DEFAULT);
  m_default_value_null= r.is_null(Columns::FIELD_DEFAULT_VALUE);
  m_default_value=      r.read_str(Columns::FIELD_DEFAULT_VALUE, "");
  m_comment=            r.read_str(Columns::FIELD_COMMENT);

  m_is_virtual= r.read_bool(Columns::FIELD_IS_VIRTUAL);
  m_generation_expression=
    r.read_str(Columns::FIELD_GENERATION_EXPRESSION, "");
  m_generation_expression_utf8=
    r.read_str(Columns::FIELD_GENERATION_EXPRESSION_UTF8, "");

  m_collation_id= r.read_ref_id(Columns::FIELD_COLLATION_ID);

  // Special cases dealing with NULL values for nullable fields

  set_options_raw(r.read_str(Columns::FIELD_OPTIONS, ""));
  set_se_private_data_raw(r.read_str(Columns::FIELD_SE_PRIVATE_DATA, ""));

  set_default_option(r.read_str(Columns::FIELD_DEFAULT_OPTION, ""));
  set_update_option(r.read_str(Columns::FIELD_UPDATE_OPTION, ""));

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Column_impl::store_attributes(Raw_record *r)
{
  //
  // Special cases dealing with NULL values for nullable fields:
  //   - Store NULL in default_option if it is not set.
  //   - Store NULL in update_option if it is not set.
  //   - Store NULL in options if there are no key=value pairs
  //   - Store NULL in se_private_data if there are no key=value pairs
  //

  // TODO-NOW - May be zerofill, unsigned, auto_increment, char_length,
  // numeric_precision, datetime_precision.
  // What value should we store in those columns in case when specific
  // attribute doesn't make sense for the type ? E.g. "unsigned" for
  // VARCHAR column

  return
    store_id(r, Columns::FIELD_ID) ||
    store_name(r, Columns::FIELD_NAME) ||
    r->store(Columns::FIELD_TABLE_ID, m_table->id()) ||
    r->store(Columns::FIELD_ORDINAL_POSITION, m_ordinal_position) ||
    r->store(Columns::FIELD_TYPE, static_cast<int>(m_type)) ||
    r->store(Columns::FIELD_IS_NULLABLE, m_is_nullable) ||
    r->store(Columns::FIELD_IS_ZEROFILL, m_is_zerofill) ||
    r->store(Columns::FIELD_IS_UNSIGNED, m_is_unsigned) ||
    r->store(Columns::FIELD_CHAR_LENGTH, static_cast<uint>(m_char_length)) ||
    r->store(Columns::FIELD_NUMERIC_PRECISION, m_numeric_precision) ||
    r->store(Columns::FIELD_NUMERIC_SCALE, m_numeric_scale, m_numeric_scale_null) ||
    r->store(Columns::FIELD_DATETIME_PRECISION, m_datetime_precision) ||
    r->store_ref_id(Columns::FIELD_COLLATION_ID, m_collation_id) ||
    r->store(Columns::FIELD_HAS_NO_DEFAULT, m_has_no_default) ||
    r->store(Columns::FIELD_DEFAULT_VALUE,
             m_default_value,
             m_default_value_null) ||
    r->store(Columns::FIELD_DEFAULT_OPTION,
             m_default_option, m_default_option.empty()) ||
    r->store(Columns::FIELD_UPDATE_OPTION,
             m_update_option,
             m_update_option.empty()) ||
    r->store(Columns::FIELD_IS_AUTO_INCREMENT, m_is_auto_increment) ||
    r->store(Columns::FIELD_IS_VIRTUAL, m_is_virtual) ||
    r->store(Columns::FIELD_GENERATION_EXPRESSION,
             m_generation_expression, m_generation_expression.empty()) ||
    r->store(Columns::FIELD_GENERATION_EXPRESSION_UTF8,
             m_generation_expression_utf8,
             m_generation_expression_utf8.empty()) ||
    r->store(Columns::FIELD_COMMENT, m_comment) ||
    r->store(Columns::FIELD_HIDDEN, m_hidden) ||
    r->store(Columns::FIELD_OPTIONS, *m_options) ||
    r->store(Columns::FIELD_SE_PRIVATE_DATA, *m_se_private_data);
}

///////////////////////////////////////////////////////////////////////////
static_assert(Columns::FIELD_SE_PRIVATE_DATA==24,
              "Columns definition has changed, review (de)ser memfuns!");
void
Column_impl::serialize(Sdi_wcontext *wctx, Sdi_writer *w) const
{
  w->StartObject();
  Entity_object_impl::serialize(wctx, w);
  write_enum(w, m_type, STRING_WITH_LEN("type"));
  write(w, m_is_nullable, STRING_WITH_LEN("is_nullable"));
  write(w, m_is_zerofill, STRING_WITH_LEN("is_zerofill"));
  write(w, m_is_unsigned, STRING_WITH_LEN("is_unsigned"));
  write(w, m_is_auto_increment, STRING_WITH_LEN("is_auto_increment"));
  write(w, m_is_virtual, STRING_WITH_LEN("is_virtual"));
  write(w, m_hidden, STRING_WITH_LEN("hidden"));
  write(w, m_ordinal_position, STRING_WITH_LEN("ordinal_position"));
  write(w, m_char_length, STRING_WITH_LEN("char_length"));
  write(w, m_numeric_precision, STRING_WITH_LEN("numeric_precision"));
  write(w, m_numeric_scale, STRING_WITH_LEN("numeric_scale"));
  write(w, m_datetime_precision, STRING_WITH_LEN("datetime_precision"));
  write(w, m_has_no_default, STRING_WITH_LEN("has_no_default"));
  write(w, m_default_value_null, STRING_WITH_LEN("default_value_null"));

  // Binary
  write_binary(wctx, w, m_default_value, STRING_WITH_LEN("default_value"));
  write(w, m_default_option, STRING_WITH_LEN("default_option"));
  write(w, m_update_option, STRING_WITH_LEN("update_option"));
  write(w, m_comment, STRING_WITH_LEN("comment"));
  write(w, m_generation_expression, STRING_WITH_LEN("generation_expression"));
  write(w, m_generation_expression_utf8, STRING_WITH_LEN("generation_expression_utf8"));
  write_properties(w, m_options, STRING_WITH_LEN("options"));
  write_properties(w, m_se_private_data, STRING_WITH_LEN("se_private_data"));
  serialize_each(wctx, w, m_enum_elements.get(), STRING_WITH_LEN("enum_elements"));
  serialize_each(wctx, w, m_set_elements.get(), STRING_WITH_LEN("set_elements"));
  write(w, m_collation_id, STRING_WITH_LEN("collation_id"));
  w->EndObject();
}

///////////////////////////////////////////////////////////////////////////

bool
Column_impl::deserialize(Sdi_rcontext *rctx, const RJ_Value &val)
{
  Entity_object_impl::deserialize(rctx, val);

  read_enum(&m_type, val, "type");
  read(&m_is_nullable, val, "is_nullable");
  read(&m_is_zerofill, val, "is_zerofill");
  read(&m_is_unsigned, val, "is_unsigned");
  read(&m_is_auto_increment, val, "is_auto_increment");
  read(&m_is_virtual, val, "is_virtual");
  read(&m_hidden, val, "hidden");
  read(&m_ordinal_position, val, "ordinal_position");
  read(&m_char_length, val, "char_length");
  read(&m_numeric_precision, val, "numeric_precision");
  read(&m_numeric_scale, val, "numeric_scale");
  read(&m_datetime_precision, val, "datetime_precision");
  read(&m_has_no_default, val, "has_no_default");
  read(&m_default_value_null, val, "default_value_null");
  read_binary(rctx, &m_default_value, val, "default_value");
  read(&m_default_option, val, "default_option");
  read(&m_update_option, val, "update_option");
  read(&m_comment, val, "comment");
  read(&m_generation_expression, val, "generation_expression");
  read(&m_generation_expression_utf8, val, "generation_expression_utf8");
  read_properties(&m_options, val, "options");
  read_properties(&m_se_private_data, val, "se_private_data");

  deserialize_each(rctx, [this] () { return add_enum_element(); },
                   val, "enum_elements");
  deserialize_each(rctx, [this] () { return add_set_element(); },
                   val, "set_elements");

  read(&m_collation_id, val, "collation_id");

  track_object(rctx, this);

  return false;
}

///////////////////////////////////////////////////////////////////////////

void Column_impl::debug_print(std::string &outb) const
{
  std::stringstream ss;
  ss
    << "COLUMN OBJECT: { "
    << "m_id: {OID: " << id() << "}; "
    << "m_table_id: {OID: " << m_table->id() << "}; "
    << "m_name: " << name() << "; "
    << "m_ordinal_position: " << m_ordinal_position << "; "
    << "m_type: " << static_cast<int>(m_type) << "; "
    << "m_is_nullable: " << m_is_nullable << "; "
    << "m_is_zerofill: " << m_is_zerofill << "; "
    << "m_is_unsigned: " << m_is_unsigned << "; "
    << "m_char_length: " << m_char_length << "; "
    << "m_numeric_precision: " << m_numeric_precision << "; "
    << "m_numeric_scale: " << m_numeric_scale << "; "
    << "m_datetime_precision: " << m_datetime_precision << "; "
    << "m_collation_id: {OID: " << m_collation_id << "}; "
    << "m_has_no_default: " << m_has_no_default << "; "
    << "m_default_value: <excluded from output>" << "; "
    << "m_default_option: " << m_default_option << "; "
    << "m_update_option: " << m_update_option << "; "
    << "m_is_auto_increment: " <<  m_is_auto_increment << "; "
    << "m_comment: " << m_comment << "; "
    << "m_is_virtual " << m_is_virtual << "; "
    << "m_generation_expression: " << m_generation_expression << "; "
    << "m_generation_expression_utf8: " << m_generation_expression_utf8 << "; "
    << "m_hidden: " << m_hidden << "; "
    << "m_options: " << m_options->raw_string() << "; ";

  if (m_type == enum_column_types::ENUM)
  {
    ss << "m_enum_elements: [ ";

    std::unique_ptr<Column_type_element_const_iterator> it(enum_elements());

    while (true)
    {
      const Column_type_element *e= it->next();

      if (!e)
        break;

      std::string ob;
      e->debug_print(ob);
      ss << ob;
    }

    ss << " ]";
  }
  else if(m_type == enum_column_types::SET)
  {
    ss << "m_set_elements: [ ";

    std::unique_ptr<Column_type_element_const_iterator> it(set_elements());

    while (true)
    {
      const Column_type_element *e= it->next();

      if (!e)
        break;

      std::string ob;
      e->debug_print(ob);
      ss << ob;
    }

    ss << " ]";
  }

  ss << " }";

  outb= ss.str();
}

///////////////////////////////////////////////////////////////////////////

/* purecov: begin deadcode */
void Column_impl::drop()
{
  m_table->column_collection()->remove(this);
}
/* purecov: end */

///////////////////////////////////////////////////////////////////////////
// Enum-elements.
///////////////////////////////////////////////////////////////////////////

Column_type_element *Column_impl::add_enum_element()
{
  if (type() != enum_column_types::ENUM)
    return NULL;

  return
    m_enum_elements->add(
      Column_type_element_impl::Factory(this, m_enum_elements.get()));
}

///////////////////////////////////////////////////////////////////////////

Column_type_element_const_iterator *Column_impl::enum_elements() const
{
  return type() == enum_column_types::ENUM ?
    m_enum_elements->const_iterator() : NULL;
}

///////////////////////////////////////////////////////////////////////////

Column_type_element_iterator *Column_impl::enum_elements()
{
  return type() == enum_column_types::ENUM ?
    m_enum_elements->iterator() : NULL;
}

///////////////////////////////////////////////////////////////////////////

size_t Column_impl::enum_elements_count() const
{
  return m_enum_elements->size();
}

///////////////////////////////////////////////////////////////////////////
// Set-elements.
///////////////////////////////////////////////////////////////////////////

Column_type_element *Column_impl::add_set_element()
{
  if (type() != enum_column_types::SET)
    return NULL;

  return
    m_set_elements->add(
      Column_type_element_impl::Factory(this, m_set_elements.get()));
}

///////////////////////////////////////////////////////////////////////////

Column_type_element_const_iterator *Column_impl::set_elements() const
{
  return type() == enum_column_types::SET ?
    m_set_elements->const_iterator() : NULL;
}

///////////////////////////////////////////////////////////////////////////

Column_type_element_iterator *Column_impl::set_elements()
{
  return type() == enum_column_types::SET ?
    m_set_elements->iterator() : NULL;
}

///////////////////////////////////////////////////////////////////////////

size_t Column_impl::set_elements_count() const
{
  return m_set_elements->size();
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

Collection_item *Column_impl::Factory::create_item() const
{
  Column_impl *c= new (std::nothrow) Column_impl();
  c->m_table= m_table;
  return c;
}

///////////////////////////////////////////////////////////////////////////

Column_impl::Column_impl(const Column_impl &src, Abstract_table_impl *parent)
  : Weak_object(src), Entity_object_impl(src), m_type(src.m_type),
    m_is_nullable(src.m_is_nullable),
    m_is_zerofill(src.m_is_zerofill), m_is_unsigned(src.m_is_unsigned),
    m_is_auto_increment(src.m_is_auto_increment),
    m_is_virtual(src.m_is_virtual), m_hidden(src.m_hidden),
    m_ordinal_position(src.m_ordinal_position),
    m_char_length(src.m_char_length),
    m_numeric_precision(src.m_numeric_precision),
    m_numeric_scale(src.m_numeric_scale),
    m_numeric_scale_null(src.m_numeric_scale_null),
    m_datetime_precision(src.m_datetime_precision),
    m_has_no_default(src.m_has_no_default),
    m_default_value_null(src.m_default_value_null),
    m_default_value(src.m_default_value),
    m_default_option(src.m_default_option),
    m_update_option(src.m_update_option), m_comment(src.m_comment),
    m_generation_expression(src.m_generation_expression),
    m_generation_expression_utf8(src.m_generation_expression_utf8),
    m_options(Properties_impl::parse_properties(src.m_options->raw_string())),
    m_se_private_data(Properties_impl::
                      parse_properties(src.m_se_private_data->raw_string())),
    m_table(parent), m_enum_elements(new Column_type_element_collection()),
    m_set_elements(new Column_type_element_collection()),
    m_collation_id(src.m_collation_id)
{
  typedef Base_collection::Array::const_iterator i_type;
  i_type end= src.m_enum_elements->aref().end();
  m_enum_elements->aref().reserve(src.m_enum_elements->size());
  for (i_type i= src.m_enum_elements->aref().begin(); i != end; ++i)
  {
    m_enum_elements->aref().push_back(dynamic_cast<Column_type_element_impl*>(*i)->
                                      clone(this, m_enum_elements.get()));
  }

  end= src.m_set_elements->aref().end();
  m_set_elements->aref().reserve(src.m_set_elements->size());
  for (i_type i= src.m_set_elements->aref().begin(); i != end; ++i)
  {
    m_set_elements->aref().push_back(dynamic_cast<Column_type_element_impl*>(*i)->
                                     clone(this, m_set_elements.get()));
  }
}

///////////////////////////////////////////////////////////////////////////
// Column_type implementation.
///////////////////////////////////////////////////////////////////////////

void Column_type::register_tables(Open_dictionary_tables_ctx *otx) const
{
  otx->add_table<Columns>();

  otx->register_tables<Column_type_element>();
}

///////////////////////////////////////////////////////////////////////////

}
