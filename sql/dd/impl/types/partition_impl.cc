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

#include "dd/impl/types/partition_impl.h"

#include <stddef.h>
#include <sstream>

#include "dd/impl/properties_impl.h"               // Properties_impl
#include "dd/impl/raw/raw_record.h"                // Raw_record
#include "dd/impl/sdi_impl.h"                      // sdi read/write functions
#include "dd/impl/tables/index_partitions.h"       // Index_partitions
#include "dd/impl/tables/table_partition_values.h" // Table_partition_values
#include "dd/impl/tables/table_partitions.h"       // Table_partitions
#include "dd/impl/transaction_impl.h"              // Open_dictionary_tables_ctx
#include "dd/impl/types/partition_index_impl.h"    // Partition_index_impl
#include "dd/impl/types/partition_value_impl.h"    // Partition_value_impl
#include "dd/impl/types/table_impl.h"              // Table_impl
#include "dd/properties.h"
#include "dd/string_type.h"                        // dd::String_type
#include "dd/types/object_table.h"
#include "dd/types/partition_index.h"
#include "dd/types/partition_value.h"
#include "dd/types/weak_object.h"
#include "m_string.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysqld_error.h"                          // ER_*
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"

using dd::tables::Index_partitions;
using dd::tables::Table_partitions;
using dd::tables::Table_partition_values;

namespace dd {

class Index;
class Sdi_rcontext;
class Sdi_wcontext;
class Table;

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
  m_se_private_id(INVALID_OBJECT_ID),
  m_options(new Properties_impl()),
  m_se_private_data(new Properties_impl()),
  m_table(NULL),
  m_parent(NULL),
  m_values(),
  m_indexes(),
  m_tablespace_id(INVALID_OBJECT_ID)
{ }

Partition_impl::Partition_impl(Table_impl *table)
 :m_level(-1),
  m_number(-1),
  m_se_private_id((ulonglong)-1),
  m_options(new Properties_impl()),
  m_se_private_data(new Properties_impl()),
  m_table(table),
  m_parent(NULL),
  m_values(),
  m_indexes(),
  m_tablespace_id(INVALID_OBJECT_ID)
{ }


Partition_impl::~Partition_impl()
{ }

///////////////////////////////////////////////////////////////////////////

const Table &Partition_impl::table() const
{
  return *m_table;
}

Table &Partition_impl::table()
{
  return *m_table;
}

///////////////////////////////////////////////////////////////////////////

bool Partition_impl::set_options_raw(const String_type &options_raw)
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
                       const String_type &se_private_data_raw)
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

  if (m_engine.empty())
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             Partition_impl::OBJECT_TABLE().name().c_str(),
             "Engine name is not set.");
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
  return m_values.restore_items(
           this,
           otx,
           otx->get_table<Partition_value>(),
           Table_partition_values::create_key_by_partition_id(this->id()),
           Partition_value_order_comparator()) ||
         m_indexes.restore_items(
           // Index will be resolved in restore_attributes()
           // called from Collection::restore_items().
           this,
           otx,
           otx->get_table<Partition_index>(),
           Index_partitions::create_key_by_partition_id(this->id()),
           Partition_index_order_comparator());
}

///////////////////////////////////////////////////////////////////////////

bool Partition_impl::store_children(Open_dictionary_tables_ctx *otx)
{
  return m_values.store_items(otx) ||
         m_indexes.store_items(otx);
}

///////////////////////////////////////////////////////////////////////////

bool Partition_impl::drop_children(Open_dictionary_tables_ctx *otx) const
{
  return m_values.drop_items(
           otx,
           otx->get_table<Partition_value>(),
           Table_partition_values::create_key_by_partition_id(this->id())) ||
         m_indexes.drop_items(
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

  m_engine=          r.read_str(Table_partitions::FIELD_ENGINE);
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
         r->store(Table_partitions::FIELD_ENGINE, m_engine) ||
         r->store(Table_partitions::FIELD_COMMENT, m_comment) ||
         r->store(Table_partitions::FIELD_OPTIONS, *m_options) ||
         r->store(Table_partitions::FIELD_SE_PRIVATE_DATA, *m_se_private_data) ||
         r->store(Table_partitions::FIELD_SE_PRIVATE_ID,
                  m_se_private_id, m_se_private_id == INVALID_OBJECT_ID) ||
         r->store_ref_id(Table_partitions::FIELD_TABLESPACE_ID, m_tablespace_id);
}

///////////////////////////////////////////////////////////////////////////
static_assert(Table_partitions::FIELD_TABLESPACE_ID==10,
              "Table_partitions definition has changed. Review (de)ser memfuns!");
void
Partition_impl::serialize(Sdi_wcontext *wctx, Sdi_writer *w) const
{
  w->StartObject();
  Entity_object_impl::serialize(wctx, w);
  write(w, m_level, STRING_WITH_LEN("level"));
  write(w, m_number, STRING_WITH_LEN("number"));
  write(w, m_se_private_id, STRING_WITH_LEN("se_private_id"));
  write(w, m_engine, STRING_WITH_LEN("engine"));
  write(w, m_comment, STRING_WITH_LEN("comment"));
  write_properties(w, m_options, STRING_WITH_LEN("options"));
  write_properties(w, m_se_private_data, STRING_WITH_LEN("se_private_data"));
  serialize_each(wctx, w, m_values, STRING_WITH_LEN("values"));
  serialize_each(wctx, w, m_indexes, STRING_WITH_LEN("indexes"));
  serialize_tablespace_ref(wctx, w, m_tablespace_id,
                           STRING_WITH_LEN("tablespace_ref"));
  w->EndObject();
}

///////////////////////////////////////////////////////////////////////////

bool
Partition_impl::deserialize(Sdi_rcontext *rctx, const RJ_Value &val)
{
  Entity_object_impl::deserialize(rctx, val);
  read(&m_level, val, "level");
  read(&m_number, val, "number");
  read(&m_se_private_id, val, "se_private_id");
  read(&m_engine, val, "engine");
  read(&m_comment, val, "comment");
  read_properties(&m_options, val, "options");
  read_properties(&m_se_private_data, val, "se_private_data");
  deserialize_each(rctx, [this] () { return add_value(); },
                   val, "values");
  deserialize_each(rctx, [this] () { return add_index(nullptr); },
                   val, "indexes");

  return deserialize_tablespace_ref(rctx, &m_tablespace_id, val,
                                    "tablespace_ref");
}

///////////////////////////////////////////////////////////////////////////

void Partition_impl::debug_print(String_type &outb) const
{
  dd::Stringstream_type ss;
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
    << "m_values: " << m_values.size()
    << " [ ";

  {
    for (const Partition_value *c : values())
    {
      String_type ob;
      c->debug_print(ob);
      ss << ob;
    }
  }
  ss << "] ";

  ss << "m_indexes: " << m_indexes.size()
    << " [ ";

  {
    for (const Partition_index *i : indexes())
    {
      String_type ob;
      i->debug_print(ob);
      ss << ob;
    }
  }
  ss << "] ";

  ss << " }";

  outb= ss.str();
}

/////////////////////////////////////////////////////////////////////////

Partition_value *Partition_impl::add_value()
{
  Partition_value_impl *e= new (std::nothrow) Partition_value_impl(this);
  m_values.push_back(e);
  return e;
}

///////////////////////////////////////////////////////////////////////////

Partition_index *Partition_impl::add_index(Index *idx)
{
  Partition_index_impl *e= new (std::nothrow) Partition_index_impl(this, idx);
  m_indexes.push_back(e);
  return e;
}

///////////////////////////////////////////////////////////////////////////

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
    m_parent(src.parent() ? parent->get_partition(src.parent()->name()) : NULL),
    m_values(),
    m_indexes(),
    m_tablespace_id(src.m_tablespace_id)
{
  m_values.deep_copy(src.m_values, this);
  m_indexes.deep_copy(src.m_indexes, this);
}

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
