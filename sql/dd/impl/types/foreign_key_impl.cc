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

#include "dd/impl/types/foreign_key_impl.h"

#include "mysqld_error.h"                            // ER_*

#include "dd/properties.h"                           // Needed for destructor
#include "dd/impl/collection_impl.h"                 // Collection
#include "dd/impl/transaction_impl.h"                // Open_dictionary_tables_ctx
#include "dd/impl/raw/raw_record.h"                  // Raw_record
#include "dd/impl/tables/foreign_keys.h"             // Foreign_keys
#include "dd/impl/tables/foreign_key_column_usage.h" // Foreign_key_column_usage
#include "dd/impl/types/foreign_key_element_impl.h"  // Foreign_key_element_impl
#include "dd/impl/types/table_impl.h"                // Table_impl
#include "dd/types/index.h"                          // Index

#ifndef DBUG_OFF
#include "dd/types/column.h"                         // Column::id()
#endif /* !DBUG_OFF */

#include <sstream>

using dd::tables::Foreign_keys;
using dd::tables::Foreign_key_column_usage;

namespace dd {

///////////////////////////////////////////////////////////////////////////
// Foreign_key implementation.
///////////////////////////////////////////////////////////////////////////

const Object_table &Foreign_key::OBJECT_TABLE()
{
  return  Foreign_keys::instance();
}

///////////////////////////////////////////////////////////////////////////

const Object_type &Foreign_key::TYPE()
{
  static Foreign_key_type s_instance;
  return s_instance;
}

///////////////////////////////////////////////////////////////////////////
// Foreign_key_impl implementation.
///////////////////////////////////////////////////////////////////////////

// Foreign keys not supported in the Global DD yet
/* purecov: begin deadcode */
Foreign_key_impl::Foreign_key_impl()
 :m_match_option(OPTION_NONE),
  m_update_rule(RULE_NO_ACTION),
  m_delete_rule(RULE_NO_ACTION),
  m_elements(new Element_collection())
{ }

///////////////////////////////////////////////////////////////////////////

Table &Foreign_key_impl::table()
{
  return *m_table;
}

///////////////////////////////////////////////////////////////////////////

bool Foreign_key_impl::validate() const
{
  if (!m_unique_constraint)
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             Foreign_key_impl::OBJECT_TABLE().name().c_str(),
             "Cannot use non-unique constraint.");
    return true;
  }

  if (!m_table)
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             Foreign_key_impl::OBJECT_TABLE().name().c_str(),
             "No table object associated with this foreign key.");
    return true;
  }

  if (m_referenced_table_catalog_name.empty())
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             Foreign_key_impl::OBJECT_TABLE().name().c_str(),
             "Referenced table catalog name is not set.");
    return true;
  }

  if (m_referenced_table_schema_name.empty())
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             Foreign_key_impl::OBJECT_TABLE().name().c_str(),
             "Referenced table schema name is not set.");
    return true;
  }

  if (m_referenced_table_name.empty())
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             Foreign_key_impl::OBJECT_TABLE().name().c_str(),
             "Referenced table name is not set.");
    return true;
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Foreign_key_impl::restore_children(Open_dictionary_tables_ctx *otx)
{

  return m_elements->restore_items(
    Foreign_key_element_impl::Factory(this),
    otx,
    otx->get_table<Foreign_key_element>(),
    Foreign_key_column_usage::create_key_by_foreign_key_id(this->id()));
}

///////////////////////////////////////////////////////////////////////////

bool Foreign_key_impl::store_children(Open_dictionary_tables_ctx *otx)
{
  return m_elements->store_items(otx);
}

///////////////////////////////////////////////////////////////////////////

bool Foreign_key_impl::drop_children(Open_dictionary_tables_ctx *otx)
{
  return m_elements->drop_items(
    otx,
    otx->get_table<Foreign_key_element>(),
    Foreign_key_column_usage::create_key_by_foreign_key_id(this->id()));
}

///////////////////////////////////////////////////////////////////////////

bool Foreign_key_impl::restore_attributes(const Raw_record &r)
{
  if (check_parent_consistency(m_table, r.read_ref_id(Foreign_keys::FIELD_TABLE_ID)))
    return true;

  restore_id(r, Foreign_keys::FIELD_ID);
  restore_name(r, Foreign_keys::FIELD_NAME);

  m_match_option= (enum_match_option) r.read_int(Foreign_keys::FIELD_MATCH_OPTION);
  m_update_rule= (enum_rule)          r.read_int(Foreign_keys::FIELD_UPDATE_RULE);
  m_delete_rule= (enum_rule)          r.read_int(Foreign_keys::FIELD_DELETE_RULE);

  m_unique_constraint=
    m_table->get_index(
      r.read_ref_id(Foreign_keys::FIELD_UNIQUE_CONSTRAINT_ID));

  return (m_unique_constraint == NULL);
}

///////////////////////////////////////////////////////////////////////////

bool Foreign_key_impl::store_attributes(Raw_record *r)
{
  return store_id(r, Foreign_keys::FIELD_ID) ||
         store_name(r, Foreign_keys::FIELD_NAME) ||
         r->store(Foreign_keys::FIELD_TABLE_ID, m_table->id()) ||
         r->store(Foreign_keys::FIELD_UNIQUE_CONSTRAINT_ID, m_unique_constraint->id()) ||
         r->store(Foreign_keys::FIELD_MATCH_OPTION, m_match_option) ||
         r->store(Foreign_keys::FIELD_UPDATE_RULE, m_update_rule) ||
         r->store(Foreign_keys::FIELD_DELETE_RULE, m_delete_rule);
}
/* purecov: end */

///////////////////////////////////////////////////////////////////////////

void
Foreign_key_impl::serialize(WriterVariant *wv) const
{

}

void
Foreign_key_impl::deserialize(const RJ_Document *d)
{

}

///////////////////////////////////////////////////////////////////////////

/* purecov: begin inspected */
void Foreign_key_impl::debug_print(std::string &outb) const
{
  std::stringstream ss;
  ss
    << "FOREIGN_KEY OBJECT: { "
    << "m_id: {OID: " << id() << "}; "
    << "m_name: " << name() << "; "
    << "m_unique_constraint_id: {OID: " << m_unique_constraint->id() << "}; "
    << "m_match_option: " << m_match_option << "; "
    << "m_update_rule: " << m_update_rule << "; "
    << "m_delete_rule: " << m_delete_rule << "; ";

  {
    std::unique_ptr<Foreign_key_element_const_iterator> it(elements());

    while (true)
    {
      const Foreign_key_element *e= it->next();

      if (!e)
        break;

      std::string ob;
      e->debug_print(ob);
      ss << ob;
    }
  }

  ss << " }";

  outb= ss.str();
}
/* purecov: end */

/////////////////////////////////////////////////////////////////////////

/* purecov: begin deadcode */
Foreign_key_element *Foreign_key_impl::add_element()
{
  return m_elements->add(
    Foreign_key_element_impl::Factory(this));
}

/////////////////////////////////////////////////////////////////////////

Foreign_key_element_const_iterator *Foreign_key_impl::elements() const
{
  return m_elements->const_iterator();
}

/////////////////////////////////////////////////////////////////////////

Foreign_key_element_iterator *Foreign_key_impl::elements()
{
  return m_elements->iterator();
}

/////////////////////////////////////////////////////////////////////////

void Foreign_key_impl::drop()
{
  m_table->foreign_key_collection()->remove(this);
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

Collection_item *Foreign_key_impl::Factory::create_item() const
{
  Foreign_key_impl *fk= new (std::nothrow) Foreign_key_impl();
  fk->m_table= m_table;
  return fk;
}

///////////////////////////////////////////////////////////////////////////

#ifndef DBUG_OFF
Foreign_key_impl::Foreign_key_impl(const Foreign_key_impl &src,
                                   Table_impl *parent,
                                   Index *unique_constraint)
  : Weak_object(src), Entity_object_impl(src),
    m_match_option(src.m_match_option), m_update_rule(src.m_update_rule),
    m_delete_rule(src.m_delete_rule),
    m_unique_constraint(unique_constraint),
    m_referenced_table_catalog_name(src.m_referenced_table_catalog_name),
    m_referenced_table_schema_name(src.m_referenced_table_schema_name),
    m_referenced_table_name(src.m_referenced_table_name),
    m_table(parent),
    m_elements(new Element_collection())
{
  typedef Base_collection::Array::const_iterator i_type;
  i_type end= src.m_elements->aref().end();
  m_elements->aref().reserve(src.m_elements->size());
  for (i_type i= src.m_elements->aref().begin(); i != end; ++i)
  {
    Foreign_key_element_impl &src_fke= *dynamic_cast<Foreign_key_element_impl*>(*i);
    m_elements->aref().push_back(src_fke.clone(this, parent->get_column(src_fke.column().id())));
  }
}
#endif /* !DBUG_OFF */
/* purecov: end */

///////////////////////////////////////////////////////////////////////////
// Foreign_key_type implementation.
///////////////////////////////////////////////////////////////////////////

void Foreign_key_type::register_tables(Open_dictionary_tables_ctx *otx) const
{
  otx->add_table<Foreign_keys>();

  otx->register_tables<Foreign_key_element>();
}

///////////////////////////////////////////////////////////////////////////

}
