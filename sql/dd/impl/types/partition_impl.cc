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

#include "dd/impl/types/partition_impl.h"

#include "mysqld_error.h"                          // ER_*

#include "dd/impl/collection_impl.h"               // Collection
#include "dd/impl/properties_impl.h"               // Properties_impl
#include "dd/impl/transaction_impl.h"              // Open_dictionary_tables_ctx
#include "dd/impl/raw/raw_record.h"                // Raw_record
#include "dd/impl/tables/index_partitions.h"       // Index_partitions
#include "dd/impl/tables/table_partitions.h"       // Table_partitions
#include "dd/impl/tables/table_partition_values.h" // Table_partition_values
#include "dd/impl/types/partition_index_impl.h"    // Partition_index_impl
#include "dd/impl/types/partition_value_impl.h"    // Partition_value_impl
#include "dd/impl/types/table_impl.h"              // Table_impl

#include <sstream>

using dd::tables::Index_partitions;
using dd::tables::Table_partitions;
using dd::tables::Table_partition_values;

namespace dd {

///////////////////////////////////////////////////////////////////////////
// Partition implementation.
///////////////////////////////////////////////////////////////////////////

const Object_table &Partition::OBJECT_TABLE()
{
  return Table_partitions::instance();
}

///////////////////////////////////////////////////////////////////////////

const Object_type &Partition::TYPE()
{
  static Partition_type s_instance;
  return s_instance;
}

///////////////////////////////////////////////////////////////////////////
// Partition_impl implementation.
///////////////////////////////////////////////////////////////////////////

Partition_impl::Partition_impl()
 :m_level(-1),
  m_number(-1),
  m_se_private_id((ulonglong)-1),
  m_options(new Properties_impl()),
  m_se_private_data(new Properties_impl()),
  m_table(NULL),
  m_values(new Value_collection()),
  m_indexes(new Index_collection()),
  m_tablespace_id(INVALID_OBJECT_ID)
{ }

///////////////////////////////////////////////////////////////////////////

Table &Partition_impl::table()
{
  return *m_table;
}

///////////////////////////////////////////////////////////////////////////

bool Partition_impl::set_options_raw(const std::string &options_raw)
{
  Properties *properties=
    Properties_impl::parse_properties(options_raw);

  if (!properties)
    return true; // Error status, current values has not changed.

  m_options.reset(properties);
  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Partition_impl::set_se_private_data_raw(
                       const std::string &se_private_data_raw)
{
  Properties *properties=
    Properties_impl::parse_properties(se_private_data_raw);

  if (!properties)
    return true; // Error status, current values has not changed.

  m_se_private_data.reset(properties);
  return false;
}

///////////////////////////////////////////////////////////////////////////

void Partition_impl::set_se_private_data(const Properties &se_private_data)
{ m_se_private_data->assign(se_private_data); }

///////////////////////////////////////////////////////////////////////////

bool Partition_impl::validate() const
{
  if (!m_table)
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             Partition_impl::OBJECT_TABLE().name().c_str(),
             "No table object associated with this partition.");
    return true;
  }

  // Partition values only relevant for LIST and RANGE partitioning,
  // not for KEY and HASH, so no validation on m_values.

  if (m_level == (uint) -1)
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             Partition_impl::OBJECT_TABLE().name().c_str(),
             "Partition level not set.");
    return true;
  }

  if (m_number == (uint) -1)
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             Partition_impl::OBJECT_TABLE().name().c_str(),
             "Partition number not set.");
    return true;
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Partition_impl::restore_children(Open_dictionary_tables_ctx *otx)
{

  return m_values->restore_items(
           Partition_value_impl::Factory(this),
           otx,
           otx->get_table<Partition_value>(),
           Table_partition_values::create_key_by_partition_id(this->id())) ||
         m_indexes->restore_items(
           // Index will be resolved in restore_attributes()
           // called from Collection::restore_items().
           Partition_index_impl::Factory(this, NULL),
           otx,
           otx->get_table<Partition_index>(),
           Index_partitions::create_key_by_partition_id(this->id()));
}

///////////////////////////////////////////////////////////////////////////

bool Partition_impl::store_children(Open_dictionary_tables_ctx *otx)
{
  return m_values->store_items(otx) ||
         m_indexes->store_items(otx);
}

///////////////////////////////////////////////////////////////////////////

bool Partition_impl::drop_children(Open_dictionary_tables_ctx *otx)
{
  return m_values->drop_items(
           otx,
           otx->get_table<Partition_value>(),
           Table_partition_values::create_key_by_partition_id(this->id())) ||
         m_indexes->drop_items(
           otx,
           otx->get_table<Partition_index>(),
           Index_partitions::create_key_by_partition_id(this->id()));
}

///////////////////////////////////////////////////////////////////////////

bool Partition_impl::restore_attributes(const Raw_record &r)
{
  if (check_parent_consistency(m_table,
        r.read_ref_id(Table_partitions::FIELD_TABLE_ID)))
    return true;

  restore_id(r, Table_partitions::FIELD_ID);
  restore_name(r, Table_partitions::FIELD_NAME);

  m_level=           r.read_uint(Table_partitions::FIELD_LEVEL);
  m_number=          r.read_uint(Table_partitions::FIELD_NUMBER);

  m_engine=          r.read_str(Table_partitions::FIELD_ENGINE, "");
  m_comment=         r.read_str(Table_partitions::FIELD_COMMENT);

  m_tablespace_id= r.read_ref_id(Table_partitions::FIELD_TABLESPACE_ID);

  m_se_private_id= r.read_uint(Table_partitions::FIELD_SE_PRIVATE_ID, -1);

  set_options_raw(r.read_str(Table_partitions::FIELD_OPTIONS, ""));
  set_se_private_data_raw(r.read_str(Table_partitions::FIELD_SE_PRIVATE_DATA, ""));

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Partition_impl::store_attributes(Raw_record *r)
{
  return store_id(r, Table_partitions::FIELD_ID) ||
         store_name(r, Table_partitions::FIELD_NAME) ||
         r->store(Table_partitions::FIELD_TABLE_ID, m_table->id()) ||
         r->store(Table_partitions::FIELD_LEVEL, m_level) ||
         r->store(Table_partitions::FIELD_NUMBER, m_number) ||
         r->store(Table_partitions::FIELD_ENGINE, m_engine, m_engine.empty()) ||
         r->store(Table_partitions::FIELD_COMMENT, m_comment) ||
         r->store(Table_partitions::FIELD_OPTIONS, *m_options) ||
         r->store(Table_partitions::FIELD_SE_PRIVATE_DATA, *m_se_private_data) ||
         r->store(Table_partitions::FIELD_SE_PRIVATE_ID,
                  m_se_private_id, m_se_private_id == (ulonglong) -1) ||
         r->store_ref_id(Table_partitions::FIELD_TABLESPACE_ID, m_tablespace_id);
}

///////////////////////////////////////////////////////////////////////////

void
Partition_impl::serialize(WriterVariant *wv) const
{

}

void
Partition_impl::deserialize(const RJ_Document *d)
{

}

///////////////////////////////////////////////////////////////////////////

void Partition_impl::debug_print(std::string &outb) const
{
  std::stringstream ss;
  ss
    << "Partition OBJECT: { "
    << "m_id: {OID: " << id() << "}; "
    << "m_table: {OID: " << m_table->id() << "}; "
    << "m_name: " << name() << "; "
    << "m_level: " << m_level << "; "
    << "m_number: " << m_number << "; "
    << "m_engine: " << m_engine << "; "
    << "m_comment: " << m_comment << "; "
    << "m_options " << m_options->raw_string() << "; "
    << "m_se_private_data " << m_se_private_data->raw_string() << "; "
    << "m_se_private_id: {OID: " << m_se_private_id << "}; "
    << "m_tablespace: {OID: " << m_tablespace_id << "}; "
    << "m_values: " << m_values->size()
    << " [ ";

  {
    std::unique_ptr<Partition_value_const_iterator> it(values());

    while (true)
    {
      const Partition_value *c= it->next();

      if (!c)
        break;

      std::string ob;
      c->debug_print(ob);
      ss << ob;
    }
  }
  ss << "] ";

  ss << "m_indexes: " << m_indexes->size()
    << " [ ";

  {
    std::unique_ptr<Partition_index_const_iterator> it(indexes());

    while (true)
    {
      const Partition_index *i= it->next();

      if (!i)
        break;

      std::string ob;
      i->debug_print(ob);
      ss << ob;
    }
  }
  ss << "] ";

  ss << " }";

  outb= ss.str();
}

/////////////////////////////////////////////////////////////////////////

void Partition_impl::drop()
{
  m_table->partition_collection()->remove(this);
}

/////////////////////////////////////////////////////////////////////////

Partition_value *Partition_impl::add_value()
{
  return
    m_values->add(
      Partition_value_impl::Factory(this));
}

///////////////////////////////////////////////////////////////////////////

Partition_value_const_iterator *Partition_impl::values() const
{
  return m_values->const_iterator();
}

///////////////////////////////////////////////////////////////////////////

Partition_value_iterator *Partition_impl::values()
{
  return m_values->iterator();
}

/////////////////////////////////////////////////////////////////////////

Partition_index *Partition_impl::add_index(Index *idx)
{
  return
    m_indexes->add(
      Partition_index_impl::Factory(this, idx));
}

///////////////////////////////////////////////////////////////////////////

Partition_index_const_iterator *Partition_impl::indexes() const
{
  return m_indexes->const_iterator();
}

///////////////////////////////////////////////////////////////////////////

Partition_index_iterator *Partition_impl::indexes()
{
  return m_indexes->iterator();
}

///////////////////////////////////////////////////////////////////////////
// Partition_impl::Factory implementation.
///////////////////////////////////////////////////////////////////////////

Collection_item *Partition_impl::Factory::create_item() const
{
  Partition_impl *i= new (std::nothrow) Partition_impl();
  i->m_table= m_table;
  return i;
}

///////////////////////////////////////////////////////////////////////////

#ifndef DBUG_OFF
Partition_impl::Partition_impl(const Partition_impl &src,
                               Table_impl *parent)
  : Weak_object(src), Entity_object_impl(src),
    m_level(src.m_level), m_number(src.m_number),
    m_se_private_id(src.m_se_private_id), m_engine(src.m_engine),
    m_comment(src.m_comment),
    m_options(Properties_impl::parse_properties(src.m_options->raw_string())),
    m_se_private_data(Properties_impl::
                      parse_properties(src.m_se_private_data->raw_string())),
    m_table(parent),
    m_values(new Value_collection()),
    m_indexes(new Index_collection()),
    m_tablespace_id(src.m_tablespace_id)
{
  typedef Base_collection::Array::const_iterator i_type;
  i_type end= src.m_values->aref().end();
  m_values->aref().reserve(src.m_values->size());
  for (i_type i= src.m_values->aref().begin(); i != end; ++i)
  {
    m_values->aref().push_back(dynamic_cast<Partition_value_impl*>(*i)->
                                 clone(this));
  }

  end= src.m_indexes->aref().end();
  m_indexes->aref().reserve(src.m_indexes->size());
  for (i_type i= src.m_indexes->aref().begin(); i != end; ++i)
  {
    Index *dstix= NULL;
    const Index &srcix= dynamic_cast<Partition_index_impl*>(*i)->index();
    dstix= parent->get_index(srcix.id());
    m_indexes->aref().push_back(dynamic_cast<Partition_index_impl*>(*i)->
                                clone(this, dstix));
  }
}
#endif /* !DBUG_OFF */

///////////////////////////////////////////////////////////////////////////
// Partition_type implementation.
///////////////////////////////////////////////////////////////////////////

void Partition_type::register_tables(Open_dictionary_tables_ctx *otx) const
{
  otx->add_table<Table_partitions>();

  otx->register_tables<Partition_value>();
  otx->register_tables<Partition_index>();
}

///////////////////////////////////////////////////////////////////////////

}
