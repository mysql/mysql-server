/* Copyright (c) 2014, 2015 Oracle and/or its affiliates. All rights reserved.

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

#include "mysqld_error.h"                         // ER_*

#include "dd/impl/collection_impl.h"              // Collection
#include "dd/impl/transaction_impl.h"             // Open_dictionary_tables_ctx
#include "dd/impl/raw/raw_record.h"               // Raw_record
#include "dd/impl/tables/column_type_elements.h"  // Column_type_elements
#include "dd/impl/types/column_impl.h"            // Column_impl

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
  if (check_parent_consistency(m_column,
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

void Column_type_element_impl::drop()
{
  m_collection->remove(this);
}

///////////////////////////////////////////////////////////////////////////

void
Column_type_element_impl::serialize(WriterVariant *wv) const
{

}

void
Column_type_element_impl::deserialize(const RJ_Document *d)
{

}

///////////////////////////////////////////////////////////////////////////

void Column_type_element_impl::debug_print(std::string &outb) const
{
  char outbuf[1024];
  sprintf(outbuf, "%s: "
    "name=%s, column_id={OID: %lld}, ordinal_position= %u",
    object_table().name().c_str(),
    m_name.c_str(), m_column->id(), m_index);
  outb= std::string(outbuf);
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
///////////////////////////////////////////////////////////////////////////

Collection_item *Column_type_element_impl::Factory::create_item() const
{
  Column_type_element_impl *e= new (std::nothrow) Column_type_element_impl();
  e->m_column= m_column;
  e->m_collection= m_collection;
  return e;
}

///////////////////////////////////////////////////////////////////////////

#ifndef DBUG_OFF
Column_type_element_impl::
Column_type_element_impl(const Column_type_element_impl &src,
                         Column_impl *parent, Collection<Column_type_element> *owner)
  : Weak_object(src),
    m_name(src.m_name), m_index(src.m_index), m_column(parent),
    m_collection(owner)
{}
#endif /* !DBUG_OFF */

///////////////////////////////////////////////////////////////////////////
// Column_type_element_type implementation.
///////////////////////////////////////////////////////////////////////////

void Column_type_element_type::register_tables(Open_dictionary_tables_ctx *otx) const
{
  otx->add_table<Column_type_elements>();
}

///////////////////////////////////////////////////////////////////////////
}
