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

#include "dd/impl/types/partition_index_impl.h"

#include <sstream>

#include "dd/impl/properties_impl.h"          // Properties_impl
#include "dd/impl/raw/raw_record.h"           // Raw_record
#include "dd/impl/sdi_impl.h"                 // sdi read/write functions
#include "dd/impl/tables/index_partitions.h"  // Index_partitions
#include "dd/impl/transaction_impl.h"         // Open_dictionary_tables_ctx
#include "dd/impl/types/entity_object_impl.h"
#include "dd/impl/types/partition_impl.h"     // Partition_impl
#include "dd/impl/types/table_impl.h"         // Table_impl
#include "dd/string_type.h"                   // dd::String_type
#include "dd/types/index.h"
#include "dd/types/object_table.h"
#include "dd/types/weak_object.h"
#include "m_string.h"
#include "my_inttypes.h"
#include "my_sys.h"
#include "mysqld_error.h"                     // ER_*
#include "rapidjson/document.h"
#include "rapidjson/prettywriter.h"

using dd::tables::Index_partitions;

namespace dd {

class Object_key;
class Partition;
class Sdi_rcontext;
class Sdi_wcontext;

///////////////////////////////////////////////////////////////////////////
// Partition_index implementation.
///////////////////////////////////////////////////////////////////////////

const Object_table &Partition_index::OBJECT_TABLE()
{
  return Index_partitions::instance();
}

///////////////////////////////////////////////////////////////////////////

const Object_type &Partition_index::TYPE()
{
  static Partition_index_type s_instance;
  return s_instance;
}

///////////////////////////////////////////////////////////////////////////
// Partition_index_impl implementation.
///////////////////////////////////////////////////////////////////////////

Partition_index_impl::Partition_index_impl()
 :m_options(new Properties_impl()),
  m_se_private_data(new Properties_impl()),
  m_partition(NULL),
  m_index(NULL),
  m_tablespace_id(INVALID_OBJECT_ID)
{ }

Partition_index_impl::Partition_index_impl(Partition_impl *partition,
                                           Index *index)
 :m_options(new Properties_impl()),
  m_se_private_data(new Properties_impl()),
  m_partition(partition),
  m_index(index),
  m_tablespace_id(INVALID_OBJECT_ID)
{ }

///////////////////////////////////////////////////////////////////////////

const Partition &Partition_index_impl::partition() const
{
  return *m_partition;
}

Partition &Partition_index_impl::partition()
{
  return *m_partition;
}

///////////////////////////////////////////////////////////////////////////

const Index &Partition_index_impl::index() const
{
  return *m_index;
}

Index &Partition_index_impl::index()
{
  return *m_index;
}

///////////////////////////////////////////////////////////////////////////

bool Partition_index_impl::set_options_raw(const String_type &options_raw)
{
  Properties *properties=
    Properties_impl::parse_properties(options_raw);

  if (!properties)
    return true; // Error status, current values has not changed.

  m_options.reset(properties);
  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Partition_index_impl::set_se_private_data_raw(
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

void Partition_index_impl::set_se_private_data(
                             const Properties &se_private_data)
{ m_se_private_data->assign(se_private_data); }

///////////////////////////////////////////////////////////////////////////

bool Partition_index_impl::validate() const
{
  if (!m_partition)
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             Partition_index_impl::OBJECT_TABLE().name().c_str(),
             "No partition object associated with this element.");
    return true;
  }

  if (!m_index)
  {
    my_error(ER_INVALID_DD_OBJECT,
             MYF(0),
             Partition_index_impl::OBJECT_TABLE().name().c_str(),
             "No index object associated with this element.");
    return true;
  }

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Partition_index_impl::restore_attributes(const Raw_record &r)
{
  // Must resolve ambiguity by static cast.
  if (check_parent_consistency(
        static_cast<Entity_object_impl*>(m_partition),
        r.read_ref_id(Index_partitions::FIELD_PARTITION_ID)))
    return true;

  m_index=
    m_partition->table_impl().get_index(
      r.read_ref_id(Index_partitions::FIELD_INDEX_ID));

  m_tablespace_id= r.read_ref_id(Index_partitions::FIELD_TABLESPACE_ID);

  set_options_raw(
    r.read_str(
      Index_partitions::FIELD_OPTIONS, ""));

  set_se_private_data_raw(
    r.read_str(
      Index_partitions::FIELD_SE_PRIVATE_DATA, ""));

  return false;
}

///////////////////////////////////////////////////////////////////////////

bool Partition_index_impl::store_attributes(Raw_record *r)
{
  return r->store(Index_partitions::FIELD_PARTITION_ID, m_partition->id()) ||
         r->store(Index_partitions::FIELD_INDEX_ID, m_index->id()) ||
         r->store(Index_partitions::FIELD_OPTIONS, *m_options) ||
         r->store(Index_partitions::FIELD_SE_PRIVATE_DATA, *m_se_private_data) ||
         r->store_ref_id(Index_partitions::FIELD_TABLESPACE_ID, m_tablespace_id);
}

///////////////////////////////////////////////////////////////////////////
static_assert(Index_partitions::FIELD_TABLESPACE_ID==4,
              "Index_partitions definition has changed, review (de)ser memfuns!");
void
Partition_index_impl::serialize(Sdi_wcontext *wctx, Sdi_writer *w) const
{
  w->StartObject();
  write_properties(w, m_options, STRING_WITH_LEN("options"));
  write_properties(w, m_se_private_data, STRING_WITH_LEN("se_private_data"));
  write_opx_reference(w, m_index, STRING_WITH_LEN("index_opx"));

  serialize_tablespace_ref(wctx, w, m_tablespace_id,
                           STRING_WITH_LEN("tablespace_ref"));
  w->EndObject();
}

///////////////////////////////////////////////////////////////////////////

bool
Partition_index_impl::deserialize(Sdi_rcontext *rctx, const RJ_Value &val)
{
  read_properties(&m_options, val, "options");
  read_properties(&m_se_private_data, val, "se_private_data");
  read_opx_reference(rctx, &m_index, val, "index_opx");

  return deserialize_tablespace_ref(rctx, &m_tablespace_id, val,
                                    "tablespace_ref");
}

///////////////////////////////////////////////////////////////////////////

void Partition_index_impl::debug_print(String_type &outb) const
{
  dd::Stringstream_type ss;
  ss
    << "PARTITION INDEX OBJECT: { "
    << "m_partition: {OID: " << m_partition->id() << "}; "
    << "m_index: {OID: " << m_index->id() << "}; "
    << "m_options " << m_options->raw_string() << "; "
    << "m_se_private_data " << m_se_private_data->raw_string() << "; "
    << "m_tablespace {OID: " << m_tablespace_id << "}";

  ss << " }";

  outb= ss.str();
}

///////////////////////////////////////////////////////////////////////////

Object_key *Partition_index_impl::create_primary_key() const
{
  return Index_partitions::create_primary_key(
    m_partition->id(), m_index->id());
}

bool Partition_index_impl::has_new_primary_key() const
{
  /*
    Ideally, we should also check if index has newly generated ID.
    Unfortunately, we don't have Index_impl available here and it is
    hard to make it available.
    Since at the moment we can't have old partition object but new
    index objects the below check works OK.
    Also note that it is OK to be pessimistic and treat new key as an
    existing key. In theory, we simply get a bit higher probability of
    deadlock between two concurrent DDL as result. However in practice
    such deadlocks are impossible, since they also require two concurrent
    DDL updating metadata for the same existing partition which is not
    supported anyway.
  */
  return m_partition->has_new_primary_key();
}

///////////////////////////////////////////////////////////////////////////

Partition_index_impl *Partition_index_impl::clone(const Partition_index_impl &other,
                                                  Partition_impl *partition)
{
  Index *dstix= partition->table_impl().get_index(other.m_index->id());
  return new Partition_index_impl(other, partition, dstix);
}

Partition_index_impl::
Partition_index_impl(const Partition_index_impl &src,
                     Partition_impl *parent, Index *index)
  : Weak_object(src),
    m_options(Properties_impl::parse_properties(src.m_options->raw_string())),
    m_se_private_data(Properties_impl::
                      parse_properties(src.m_se_private_data->raw_string())),
    m_partition(parent),
    m_index(index),
    m_tablespace_id(src.m_tablespace_id)
{}

///////////////////////////////////////////////////////////////////////////
//Partition_index_type implementation.
///////////////////////////////////////////////////////////////////////////

void Partition_index_type::register_tables(Open_dictionary_tables_ctx *otx) const
{
  otx->add_table<Index_partitions>();
}

///////////////////////////////////////////////////////////////////////////
}
