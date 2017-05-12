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

#include "dd/impl/types/column_type_element_impl.h"

#include <stdio.h>

#include "dd/impl/raw/raw_record.h"               // Raw_record
#include "dd/impl/sdi_impl.h"                     // sdi read/write functions
#include "dd/impl/tables/column_type_elements.h"  // Column_type_elements
#include "dd/impl/transaction_impl.h"             // Open_dictionary_tables_ctx
#include "dd/impl/types/column_impl.h"            // Column_impl
#include "dd/impl/types/entity_object_impl.h"
#include "dd/types/object_table.h"
#include "dd/types/weak_object.h"
#include "m_string.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysqld_error.h"                         // ER_*
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"

namespace dd {
class Column;
class Object_key;
class Sdi_rcontext;
class Sdi_wcontext;
}  // namespace dd


using dd::tables::Column_type_elements;

namespace dd {

///////////////////////////////////////////////////////////////////////////
// Column_type_element implementation.
///////////////////////////////////////////////////////////////////////////

const Object_table &Column_type_element::OBJECT_TABLE()
{
  return Column_type_elements::instance();
}

///////////////////////////////////////////////////////////////////////////

const Object_type &Column_type_element::TYPE()
{
  static Column_type_element_type s_instance;
  return s_instance;
}

///////////////////////////////////////////////////////////////////////////
// Column_type_element_impl implementation.
///////////////////////////////////////////////////////////////////////////

const Column &Column_type_element_impl::column() const
{
  return *m_column;
}

bool Column_type_element_impl::validate() const
{
  if (!m_column)
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             Column_type_element_impl::OBJECT_TABLE().name().c_str(),
             "No column associated with this object.");
    return true;
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Column_type_element_impl::restore_attributes(const Raw_record &r)
{
  // Must resolve ambiguity by static cast.
  if (check_parent_consistency(static_cast<Entity_object_impl*>(m_column),
        r.read_ref_id(Column_type_elements::FIELD_COLUMN_ID)))
    return true;

  m_index= r.read_uint(Column_type_elements::FIELD_INDEX);
  m_name= r.read_str(Column_type_elements::FIELD_NAME);

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Column_type_element_impl::store_attributes(Raw_record *r)
{
  return r->store(Column_type_elements::FIELD_COLUMN_ID, m_column->id()) ||
         r->store(Column_type_elements::FIELD_INDEX, m_index) ||
         r->store(Column_type_elements::FIELD_NAME, m_name);
}

///////////////////////////////////////////////////////////////////////////

static_assert(Column_type_elements::FIELD_NAME==2,
              "Column_type_elements definition has changed, review (de)ser memfuns!");
void
Column_type_element_impl::serialize(Sdi_wcontext *wctx, Sdi_writer *w) const
{
  w->StartObject();
  // Binary value (VARBINARY)
  write_binary(wctx, w, m_name, STRING_WITH_LEN("name"));
  write(w, m_index, STRING_WITH_LEN("index"));
  w->EndObject();
}

///////////////////////////////////////////////////////////////////////////

bool
Column_type_element_impl::deserialize(Sdi_rcontext *rctx, const RJ_Value &val)
{
  read_binary(rctx, &m_name, val, "name");
  read(&m_index, val, "index");
  return false;
}

///////////////////////////////////////////////////////////////////////////

void Column_type_element_impl::debug_print(String_type &outb) const
{
  char outbuf[1024];
  sprintf(outbuf, "%s: "
    "name=%s, column_id={OID: %lld}, ordinal_position= %u",
    object_table().name().c_str(),
    m_name.c_str(), m_column->id(), m_index);
  outb= String_type(outbuf);
}

///////////////////////////////////////////////////////////////////////////

Object_key *Column_type_element_impl::create_primary_key() const
{
  return Column_type_elements::create_primary_key(m_column->id(), m_index);
}

bool Column_type_element_impl::has_new_primary_key() const
{
  return m_column->has_new_primary_key();
}

///////////////////////////////////////////////////////////////////////////

Column_type_element_impl::
Column_type_element_impl(const Column_type_element_impl &src,
                         Column_impl *parent)
  : Weak_object(src),
    m_name(src.m_name), m_index(src.m_index), m_column(parent)
{}

///////////////////////////////////////////////////////////////////////////
// Column_type_element_type implementation.
///////////////////////////////////////////////////////////////////////////

void Column_type_element_type::register_tables(Open_dictionary_tables_ctx *otx) const
{
  otx->add_table<Column_type_elements>();
}

///////////////////////////////////////////////////////////////////////////
}
