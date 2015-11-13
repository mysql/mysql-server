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

#include "dd/impl/types/foreign_key_element_impl.h"

#include "mysqld_error.h"                            // ER_*

#include "dd/properties.h"                           // Needed for destructor
#include "dd/impl/collection_impl.h"                 // Collection
#include "dd/impl/transaction_impl.h"                // Open_dictionary_tables_ctx
#include "dd/impl/raw/raw_record.h"                  // Raw_record
#include "dd/impl/tables/foreign_key_column_usage.h" // Foreign_key_column_usage
#include "dd/impl/types/foreign_key_impl.h"          // Foreign_key_impl
#include "dd/impl/types/table_impl.h"                // Table_impl
#include "dd/types/column.h"                         // Column

using dd::tables::Foreign_key_column_usage;

namespace dd {

///////////////////////////////////////////////////////////////////////////
// Foreign_key_element implementation.
///////////////////////////////////////////////////////////////////////////

const Object_table &Foreign_key_element::OBJECT_TABLE()
{
  return Foreign_key_column_usage::instance();
}

///////////////////////////////////////////////////////////////////////////

const Object_type &Foreign_key_element::TYPE()
{
  static Foreign_key_element_type s_instance;
  return s_instance;
}

///////////////////////////////////////////////////////////////////////////
// Foreign_key_element_impl implementation.
///////////////////////////////////////////////////////////////////////////

// Foreign keys not supported in the Global DD yet
/* purecov: begin deadcode */
Foreign_key &Foreign_key_element_impl::foreign_key()
{
  return *m_foreign_key;
}

///////////////////////////////////////////////////////////////////////////

void Foreign_key_element_impl::drop()
{
  m_foreign_key->element_collection()->remove(this);
}

///////////////////////////////////////////////////////////////////////////

bool Foreign_key_element_impl::validate() const
{
  if (!m_foreign_key)
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             Foreign_key_element_impl::OBJECT_TABLE().name().c_str(),
             "No foreign key associated with this element.");
    return true;
  }

  if (!m_column)
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             Foreign_key_element_impl::OBJECT_TABLE().name().c_str(),
             "No Column is associated with this key element.");
    return true;
  }

  if (m_referenced_column_name.empty())
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             Foreign_key_element_impl::OBJECT_TABLE().name().c_str(),
             "Referenced column name is not set.");
    return true;
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Foreign_key_element_impl::restore_attributes(const Raw_record &r)
{
  if (check_parent_consistency(m_foreign_key,
        r.read_ref_id(Foreign_key_column_usage::FIELD_FOREIGN_KEY_ID)))
    return true;

  m_ordinal_position=
    r.read_uint(Foreign_key_column_usage::FIELD_ORDINAL_POSITION);

  m_referenced_column_name=
    r.read_str(Foreign_key_column_usage::FIELD_REFERENCED_COLUMN_NAME);

  m_column=
    m_foreign_key->table_impl().get_column(
      r.read_ref_id(Foreign_key_column_usage::FIELD_COLUMN_ID));

  return (m_column == NULL);
}

///////////////////////////////////////////////////////////////////////////

bool Foreign_key_element_impl::store_attributes(Raw_record *r)
{
  return r->store(Foreign_key_column_usage::FIELD_ORDINAL_POSITION, m_ordinal_position) ||
         r->store(Foreign_key_column_usage::FIELD_FOREIGN_KEY_ID, m_foreign_key->id()) ||
         r->store(Foreign_key_column_usage::FIELD_COLUMN_ID, m_column->id()) ||
         r->store(Foreign_key_column_usage::FIELD_REFERENCED_COLUMN_NAME, m_referenced_column_name);
}
/* purecov: end */

///////////////////////////////////////////////////////////////////////////

void
Foreign_key_element_impl::serialize(WriterVariant *wv) const
{

}

void
Foreign_key_element_impl::deserialize(const RJ_Document *d)
{

}

///////////////////////////////////////////////////////////////////////////
/* purecov: begin inspected */
void Foreign_key_element_impl::debug_print(std::string &outb) const
{
  std::stringstream ss;
  ss
    << "FOREIGN_KEY_ELEMENT OBJECT: { "
    << "m_foreign_key: {OID: " << m_foreign_key->id() << "}; "
    << "m_column: {OID: " << m_column->id() << "}; "
    << "m_referenced_column_name: " << m_referenced_column_name << "; ";

  ss << " }";

  outb= ss.str();
}
/* purecov: end */

///////////////////////////////////////////////////////////////////////////

/* purecov: begin deadcode */
Collection_item *Foreign_key_element_impl::Factory::create_item() const
{
  Foreign_key_element_impl *e= new (std::nothrow) Foreign_key_element_impl();
  e->m_foreign_key= m_fk;
  return e;
}

///////////////////////////////////////////////////////////////////////////

Object_key *Foreign_key_element_impl::create_primary_key() const
{
  return Foreign_key_column_usage::create_primary_key(
    m_foreign_key->id(), m_ordinal_position);
}

bool Foreign_key_element_impl::has_new_primary_key() const
{
  return m_foreign_key->has_new_primary_key();
}

///////////////////////////////////////////////////////////////////////////

#ifndef DBUG_OFF
Foreign_key_element_impl::
Foreign_key_element_impl(const Foreign_key_element_impl &src,
                         Foreign_key_impl *parent, Column *column)
  : Weak_object(src),
    m_foreign_key(parent),
    m_column(column),
    m_ordinal_position(src.m_ordinal_position),
    m_referenced_column_name(src.m_referenced_column_name)
{}
#endif /* !DBUG_OFF */
/* purecov: end */

///////////////////////////////////////////////////////////////////////////
// Foreign_key_element_type implementation.
///////////////////////////////////////////////////////////////////////////

void Foreign_key_element_type::register_tables(Open_dictionary_tables_ctx *otx) const
{
  otx->add_table<Foreign_key_column_usage>();
}

///////////////////////////////////////////////////////////////////////////

}
