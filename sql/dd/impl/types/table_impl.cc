/* Copyright (c) 2014, 2016 Oracle and/or its affiliates. All rights reserved.

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

#include "dd/impl/types/table_impl.h"

#include "mysqld_error.h"                            // ER_*

#include "dd/impl/collection_impl.h"                 // Collection
#include "dd/impl/object_key.h"                      // Needed for destructor
#include "dd/impl/properties_impl.h"                 // Properties_impl
#include "dd/impl/sdi_impl.h"                        // sdi read/write functions
#include "dd/impl/transaction_impl.h"                // Open_dictionary_tables_ctx
#include "dd/impl/raw/raw_record.h"                  // Raw_record
#include "dd/impl/tables/foreign_keys.h"             // Foreign_keys
#include "dd/impl/tables/indexes.h"                  // Indexes
#include "dd/impl/tables/tables.h"                   // Tables
#include "dd/impl/tables/table_partitions.h"         // Table_partitions
#include "dd/impl/types/foreign_key_impl.h"          // Foreign_key_impl
#include "dd/impl/types/index_impl.h"                // Index_impl
#include "dd/impl/types/partition_impl.h"            // Partition_impl
#include "dd/types/column.h"                         // Column

#include <sstream>

using dd::tables::Foreign_keys;
using dd::tables::Indexes;
using dd::tables::Tables;
using dd::tables::Table_partitions;

namespace dd {

///////////////////////////////////////////////////////////////////////////
// Table implementation.
///////////////////////////////////////////////////////////////////////////

const Object_type &Table::TYPE()
{
  static Table_type s_instance;
  return s_instance;
}

///////////////////////////////////////////////////////////////////////////
// Table_impl implementation.
///////////////////////////////////////////////////////////////////////////

Table_impl::Table_impl()
 :m_hidden(false),
  m_se_private_id((ulonglong)-1),
  m_se_private_data(new Properties_impl()),
  m_partition_type(PT_NONE),
  m_default_partitioning(DP_NONE),
  m_subpartition_type(ST_NONE),
  m_default_subpartitioning(DP_NONE),
  m_indexes(new Index_collection()),
  m_foreign_keys(new Foreign_key_collection()),
  m_partitions(new Partition_collection()),
  m_collation_id(INVALID_OBJECT_ID),
  m_tablespace_id(INVALID_OBJECT_ID)
{
}


///////////////////////////////////////////////////////////////////////////

bool Table_impl::set_se_private_data_raw(const std::string &se_private_data_raw)
{
  Properties *properties=
    Properties_impl::parse_properties(se_private_data_raw);

  if (!properties)
    return true; // Error status, current values has not changed.

  m_se_private_data.reset(properties);
  return false;
}

///////////////////////////////////////////////////////////////////////////

void Table_impl::set_se_private_data(const Properties &se_private_data)
{ m_se_private_data->assign(se_private_data); }

///////////////////////////////////////////////////////////////////////////

bool Table_impl::validate() const
{
  if (Abstract_table_impl::validate())
    return true;

  if (m_collation_id == INVALID_OBJECT_ID)
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             Table_impl::OBJECT_TABLE().name().c_str(),
             "Collation ID not set.");
    return true;
  }

  if (m_engine.empty())
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             Table_impl::OBJECT_TABLE().name().c_str(),
             "Engine name is not set.");
    return true;
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Table_impl::restore_children(Open_dictionary_tables_ctx *otx)
{
  // NOTE: the order of restoring collections is important because:
  //   - Index-objects reference Column-objects
  //     (thus, Column-objects must be loaded before Index-objects).
  //   - Foreign_key-objects reference both Index-objects and Column-objects.
  //     (thus, both Indexes and Columns must be loaded before FKs).
  //   - Partitions should be loaded at the end, as it refers to
  //     indexes.

  return
    Abstract_table_impl::restore_children(otx)
    ||
    m_indexes->restore_items(
      Index_impl::Factory(this),
      otx,
      otx->get_table<Index>(),
      Indexes::create_key_by_table_id(this->id()))
    ||
    m_foreign_keys->restore_items(
      Foreign_key_impl::Factory(this),
      otx,
      otx->get_table<Foreign_key>(),
      Foreign_keys::create_key_by_table_id(this->id()))
    ||
    m_partitions->restore_items(
      Partition_impl::Factory(this),
      otx,
      otx->get_table<Partition>(),
      Table_partitions::create_key_by_table_id(this->id()));
}

///////////////////////////////////////////////////////////////////////////

bool Table_impl::store_children(Open_dictionary_tables_ctx *otx)
{
  if (Abstract_table_impl::store_children(otx))
    return true;

  // Note that indexes has to be stored first, as
  // partitions refer indexes.
  bool ret= m_indexes->store_items(otx);
  if (!ret)
  {
    ret= m_foreign_keys->store_items(otx);
  }
  if (!ret)
  {
    ret= m_partitions->store_items(otx);
  }
  return ret;
}

///////////////////////////////////////////////////////////////////////////

bool Table_impl::drop_children(Open_dictionary_tables_ctx *otx) const
{
  // Note that partition collection has to be dropped first
  // as it has foreign key to indexes.

  return
    m_partitions->drop_items(otx,
      otx->get_table<Partition>(),
      Table_partitions::create_key_by_table_id(this->id()))
    ||
    m_foreign_keys->drop_items(otx,
      otx->get_table<Foreign_key>(),
      Foreign_keys::create_key_by_table_id(this->id()))
    ||
    m_indexes->drop_items(otx,
      otx->get_table<Index>(),
      Indexes::create_key_by_table_id(this->id()))
    ||
    Abstract_table_impl::drop_children(otx);
}

/////////////////////////////////////////////////////////////////////////

bool Table_impl::restore_attributes(const Raw_record &r)
{
  {
    enum_table_type table_type=
      static_cast<enum_table_type>(r.read_int(Tables::FIELD_TYPE));

    if (table_type != enum_table_type::BASE_TABLE)
      return true;
  }

  if (Abstract_table_impl::restore_attributes(r))
    return true;

  m_hidden=          r.read_bool(Tables::FIELD_HIDDEN);
  m_comment=         r.read_str(Tables::FIELD_COMMENT);

  // Partitioning related fields (NULL -> enum value 0!)

  m_partition_type=
    (enum_partition_type) r.read_int(Tables::FIELD_PARTITION_TYPE, 0);

  m_default_partitioning=
    (enum_default_partitioning) r.read_int(Tables::FIELD_DEFAULT_PARTITIONING,
                                           0);

  m_subpartition_type=
    (enum_subpartition_type) r.read_int(Tables::FIELD_SUBPARTITION_TYPE, 0);

  m_default_subpartitioning=
    (enum_default_partitioning)
      r.read_int(Tables::FIELD_DEFAULT_SUBPARTITIONING, 0);

  // Special cases dealing with NULL values for nullable fields

  m_se_private_id= dd::tables::Tables::read_se_private_id(r);

  m_collation_id= r.read_ref_id(Tables::FIELD_COLLATION_ID);
  m_tablespace_id= r.read_ref_id(Tables::FIELD_TABLESPACE_ID);

  set_se_private_data_raw(r.read_str(Tables::FIELD_SE_PRIVATE_DATA, ""));

  m_engine= r.read_str(Tables::FIELD_ENGINE);

  m_partition_expression= r.read_str(Tables::FIELD_PARTITION_EXPRESSION, "");
  m_subpartition_expression= r.read_str(Tables::FIELD_SUBPARTITION_EXPRESSION, "");

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Table_impl::store_attributes(Raw_record *r)
{
  //
  // Special cases dealing with NULL values for nullable fields
  //   - Store NULL if version is not set
  //     Eg: USER_VIEW or SYSTEM_VIEW may not have version set
  //   - Store NULL if se_private_id is not set
  //     Eg: A non-innodb table may not have se_private_id
  //   - Store NULL if collation id is not set
  //     Eg: USER_VIEW will not have collation id set.
  //   - Store NULL if tablespace id is not set
  //     Eg: A non-innodb table may not have tablespace
  //   - Store NULL in options if there are no key=value pairs
  //   - Store NULL in se_private_data if there are no key=value pairs
  //   - Store NULL in partition type if not set.
  //   - Store NULL in partition expression if not set.
  //   - Store NULL in default partitioning if not set.
  //   - Store NULL in subpartition type if not set.
  //   - Store NULL in subpartition expression if not set.
  //   - Store NULL in default subpartitioning if not set.
  //

  // Store field values
  return
    Abstract_table_impl::store_attributes(r) ||
    r->store(Tables::FIELD_ENGINE, m_engine) ||
    r->store_ref_id(Tables::FIELD_COLLATION_ID, m_collation_id) ||
    r->store(Tables::FIELD_COMMENT, m_comment) ||
    r->store(Tables::FIELD_HIDDEN, m_hidden) ||
    r->store(Tables::FIELD_SE_PRIVATE_DATA, *m_se_private_data) ||
    r->store(Tables::FIELD_SE_PRIVATE_ID,
             m_se_private_id,
             m_se_private_id == (ulonglong) -1) ||
    r->store_ref_id(Tables::FIELD_TABLESPACE_ID, m_tablespace_id) ||
    r->store(Tables::FIELD_PARTITION_TYPE,
             m_partition_type,
             m_partition_type == PT_NONE) ||
    r->store(Tables::FIELD_PARTITION_EXPRESSION,
             m_partition_expression,
             m_partition_expression.empty()) ||
    r->store(Tables::FIELD_DEFAULT_PARTITIONING,
             m_default_partitioning,
             m_default_partitioning == DP_NONE) ||
    r->store(Tables::FIELD_SUBPARTITION_TYPE,
             m_subpartition_type,
             m_subpartition_type == ST_NONE) ||
    r->store(Tables::FIELD_SUBPARTITION_EXPRESSION,
             m_subpartition_expression,
             m_subpartition_expression.empty()) ||
    r->store(Tables::FIELD_DEFAULT_SUBPARTITIONING,
             m_default_subpartitioning,
             m_default_subpartitioning == DP_NONE);
}

///////////////////////////////////////////////////////////////////////////

void
Table_impl::serialize(Sdi_wcontext *wctx, Sdi_writer *w) const
{
  w->StartObject();
  Abstract_table_impl::serialize(wctx, w);
  write(w, m_hidden, STRING_WITH_LEN("hidden"));
  write(w, m_se_private_id, STRING_WITH_LEN("se_private_id"));
  write(w, m_engine, STRING_WITH_LEN("engine"));
  write(w, m_comment, STRING_WITH_LEN("comment"));
  write_properties(w, m_se_private_data, STRING_WITH_LEN("se_private_data"));
  write_enum(w, m_partition_type, STRING_WITH_LEN("partition_type"));
  write(w, m_partition_expression, STRING_WITH_LEN("partition_expression"));
  write_enum(w, m_default_partitioning, STRING_WITH_LEN("default_partitioning"));
  write_enum(w, m_subpartition_type, STRING_WITH_LEN("subpartition_type"));
  write(w, m_subpartition_expression, STRING_WITH_LEN("subpartition_expression"));
  write_enum(w, m_default_subpartitioning, STRING_WITH_LEN("default_subpartitioning"));
  serialize_each(wctx, w, m_indexes.get(), STRING_WITH_LEN("indexes"));
  serialize_each(wctx, w, m_foreign_keys.get(), STRING_WITH_LEN("foreign_keys"));
  serialize_each(wctx, w, m_partitions.get(), STRING_WITH_LEN("partitions"));
  write(w, m_collation_id, STRING_WITH_LEN("collation_id"));
  serialize_tablespace_ref(wctx, w, m_tablespace_id,
                           STRING_WITH_LEN("tablespace_ref"));
  w->EndObject();
}

///////////////////////////////////////////////////////////////////////////

bool
Table_impl::deserialize(Sdi_rcontext *rctx, const RJ_Value &val)
{
  Abstract_table_impl::deserialize(rctx, val);
  read(&m_hidden, val, "hidden");
  read(&m_se_private_id, val, "se_private_id");
  read(&m_engine, val, "engine");
  read(&m_comment, val, "comment");
  read_properties(&m_se_private_data, val, "se_private_data");
  read_enum(&m_partition_type, val, "partition_type");
  read(&m_partition_expression, val, "partition_expression");
  read_enum(&m_default_partitioning, val, "default_partitioning");
  read_enum(&m_subpartition_type, val, "subpartition_type");
  read(&m_subpartition_expression, val, "subpartition_expression");
  read_enum(&m_default_subpartitioning, val, "default_subpartitioning");

  // Note! Deserialization of ordinal position cross-referenced
  // objects (i.e. Index and Column) must happen before deserializing
  // objects which refrence these objects:
  // Foreign_key_element -> Column,
  // Foreign_key         -> Index,
  // Index_element       -> Column,
  // Partition_index     -> Index
  // Otherwise the cross-references will not be deserialized correctly
  // (as we don't know the address of the referenced Column or Index
  // object).

  deserialize_each(rctx, [this] () { return add_index(); }, val,
                   "indexes");

  deserialize_each(rctx, [this] () { return add_foreign_key(); },
                   val, "foreign_keys");
  deserialize_each(rctx, [this] () { return add_partition(); }, val,
                   "partitions");
  read(&m_collation_id, val, "collation_id");
  return deserialize_tablespace_ref(rctx, &m_tablespace_id, val, "tablespace_id");
}

///////////////////////////////////////////////////////////////////////////

void Table_impl::debug_print(std::string &outb) const
{
  std::string s;
  Abstract_table_impl::debug_print(s);

  std::stringstream ss;
  ss
    << "TABLE OBJECT: { "
    << s
    << "m_engine: " << m_engine << "; "
    << "m_collation: {OID: " << m_collation_id << "}; "
    << "m_comment: " << m_comment << "; "
    << "m_hidden: " << m_hidden << "; "
    << "m_se_private_data " << m_se_private_data->raw_string() << "; "
    << "m_se_private_id: {OID: " << m_se_private_id << "}; "
    << "m_tablespace: {OID: " << m_tablespace_id << "}; "
    << "m_partition_type " << m_partition_type << "; "
    << "m_default_partitioning " << m_default_partitioning << "; "
    << "m_partition_expression " << m_partition_expression << "; "
    << "m_subpartition_type " << m_subpartition_type << "; "
    << "m_default_subpartitioning " << m_default_subpartitioning << "; "
    << "m_subpartition_expression " << m_subpartition_expression << "; "
    << "m_partitions: " << m_partitions->size() << " [ ";

  {
    std::unique_ptr<Partition_const_iterator> it(partitions());

    while (true)
    {
      const Partition *i= it->next();

      if (!i)
        break;

      std::string s;
      i->debug_print(s);
      ss << s << " | ";
    }
  }

  ss << "] m_indexes: " << m_indexes->size() << " [ ";

  {
    std::unique_ptr<Index_const_iterator> it(indexes());

    while (true)
    {
      const Index *i= it->next();

      if (!i)
        break;

      std::string s;
      i->debug_print(s);
      ss << s << " | ";
    }
  }

  ss << "] m_foreign_keys: " << m_foreign_keys->size() << " [ ";

  {
    std::unique_ptr<Foreign_key_const_iterator> it(foreign_keys());

    while (true)
    {
      const Foreign_key *fk= it->next();

      if (!fk)
        break;

      std::string s;
      fk->debug_print(s);
      ss << s << " | ";
    }
  }
  ss << "] ";

  ss << " }";

  outb= ss.str();
}

///////////////////////////////////////////////////////////////////////////
// Index collection.
///////////////////////////////////////////////////////////////////////////

Index *Table_impl::add_index()
{
  return
    m_indexes->add(
      Index_impl::Factory(this));
}

///////////////////////////////////////////////////////////////////////////

Index *Table_impl::add_first_index()
{
  return m_indexes->add_first(
      Index_impl::Factory(this));
}

///////////////////////////////////////////////////////////////////////////

Index_const_iterator *Table_impl::indexes() const
{
  return m_indexes->const_iterator();
}

///////////////////////////////////////////////////////////////////////////

Index_iterator *Table_impl::indexes()
{
  return m_indexes->iterator();
}

///////////////////////////////////////////////////////////////////////////

Index_const_iterator *Table_impl::user_indexes() const
{
  return m_indexes->const_iterator(Collection<Index>::SKIP_HIDDEN_ITEMS);
}

///////////////////////////////////////////////////////////////////////////

Index_iterator *Table_impl::user_indexes()
{
  return m_indexes->iterator(Collection<Index>::SKIP_HIDDEN_ITEMS);
}

///////////////////////////////////////////////////////////////////////////

Index *Table_impl::get_index(Object_id index_id)
{
  std::unique_ptr<Index_iterator> it(indexes());

  while (true)
  {
    Index *i= it->next();

    if (!i)
      break;

    if (i->id() == index_id)
      return i;
  }

  return NULL;
}

///////////////////////////////////////////////////////////////////////////
// Foreign key collection.
///////////////////////////////////////////////////////////////////////////

Foreign_key *Table_impl::add_foreign_key()
{
  return
    m_foreign_keys->add(
      Foreign_key_impl::Factory(this));
}

///////////////////////////////////////////////////////////////////////////

Foreign_key_const_iterator *Table_impl::foreign_keys() const
{
  return m_foreign_keys->const_iterator();
}

///////////////////////////////////////////////////////////////////////////

Foreign_key_iterator *Table_impl::foreign_keys()
{
  return m_foreign_keys->iterator();
}

///////////////////////////////////////////////////////////////////////////
// Partition collection.
///////////////////////////////////////////////////////////////////////////

Partition *Table_impl::add_partition()
{
  return
    m_partitions->add(
      Partition_impl::Factory(this));
}

///////////////////////////////////////////////////////////////////////////

Partition_const_iterator *Table_impl::partitions() const
{
  Partition_type::Partition_order_comparator
    p= Partition_type::Partition_order_comparator();

  m_partitions->sort_items(p);

  return m_partitions->const_iterator();
}

///////////////////////////////////////////////////////////////////////////

Partition_iterator *Table_impl::partitions()
{
  Partition_type::Partition_order_comparator
    p= Partition_type::Partition_order_comparator();

  m_partitions->sort_items(p);

  return m_partitions->iterator();
}

///////////////////////////////////////////////////////////////////////////

Partition *Table_impl::get_partition(Object_id partition_id)
{
  std::unique_ptr<Partition_iterator> it(partitions());

  while (true)
  {
    Partition *i= it->next();

    if (!i)
      break;

    if (i->id() == partition_id)
      return i;
  }

  return NULL;
}

///////////////////////////////////////////////////////////////////////////

const Partition *Table_impl::get_last_partition() const
{
  return m_partitions->back();
}

///////////////////////////////////////////////////////////////////////////

bool Table::update_aux_key(aux_key_type *key,
                           const std::string &engine,
                           Object_id se_private_id)
{
  if (se_private_id != INVALID_OBJECT_ID)
    return Tables::update_aux_key(key, engine, se_private_id);

  return true;
}

////////////////////////////////////////////////////////////////////////////
// Table_type implementation.
///////////////////////////////////////////////////////////////////////////

void Table_type::register_tables(Open_dictionary_tables_ctx *otx) const
{
  otx->add_table<Tables>();

  otx->register_tables<Column>();
  otx->register_tables<Index>();
  otx->register_tables<Foreign_key>();
  otx->register_tables<Partition>();
}

///////////////////////////////////////////////////////////////////////////

Table_impl::Table_impl(const Table_impl &src)
  : Weak_object(src), Abstract_table_impl(src),
    m_hidden(src.m_hidden), m_se_private_id(src.m_se_private_id),
    m_engine(src.m_engine),
    m_comment(src.m_comment),
    m_se_private_data(Properties_impl::
                      parse_properties(src.m_se_private_data->raw_string())),
    m_partition_type(src.m_partition_type),
    m_partition_expression(src.m_partition_expression),
    m_default_partitioning(src.m_default_partitioning),
    m_subpartition_type(src.m_subpartition_type),
    m_subpartition_expression(src.m_subpartition_expression),
    m_default_subpartitioning(src.m_default_subpartitioning),
    m_indexes(new Index_collection()),
    m_foreign_keys(new Foreign_key_collection()),
    m_partitions(new Partition_collection()),
    m_collation_id(src.m_collation_id), m_tablespace_id(src.m_tablespace_id)
{
  typedef Base_collection::Array::const_iterator i_type;
  i_type end= src.m_indexes->aref().end();
  m_indexes->aref().reserve(src.m_indexes->size());
  for (i_type i= src.m_indexes->aref().begin(); i != end; ++i)
  {
    m_indexes->aref().push_back(dynamic_cast<Index_impl*>(*i)->
                                clone(this));
  }

  end= src.m_foreign_keys->aref().end();
  m_foreign_keys->aref().reserve(src.m_foreign_keys->size());
  for (i_type i= src.m_foreign_keys->aref().begin(); i != end; ++i)
  {
    Foreign_key_impl &src_fk= *dynamic_cast<Foreign_key_impl*>(*i);
    m_foreign_keys->aref().
      push_back(src_fk.clone(this, get_index(src_fk.unique_constraint().id())));
  }

  end= src.m_partitions->aref().end();
  m_partitions->aref().reserve(src.m_partitions->size());
  for (i_type i= src.m_partitions->aref().begin(); i != end; ++i)
  {
    m_partitions->aref().push_back(dynamic_cast<Partition_impl*>(*i)->
                                   clone(this));
  }
}
}
