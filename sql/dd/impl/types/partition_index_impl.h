/* Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD__PARTITION_INDEX_IMPL_INCLUDED
#define DD__PARTITION_INDEX_IMPL_INCLUDED

#include "my_global.h"

#include "dd/impl/collection_item.h"            // dd::Collection_item
#include "dd/impl/os_specific.h"                // DD_HEADER_BEGIN
#include "dd/impl/types/weak_object_impl.h"     // dd::Weak_object_impl
#include "dd/types/object_type.h"               // dd::Object_type
#include "dd/types/partition_index.h"           // dd::Partition_index

#include <memory>

DD_HEADER_BEGIN

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Partition_impl;
class Raw_record;
class Open_dictionary_tables_ctx;

///////////////////////////////////////////////////////////////////////////

class Partition_index_impl : virtual public Weak_object_impl,
                             virtual public Partition_index,
                             virtual public Collection_item
{
public:
  Partition_index_impl();

  virtual ~Partition_index_impl()
  { }

public:
  virtual const Object_table &object_table() const
  { return Partition_index::OBJECT_TABLE(); }

  virtual bool validate() const;

  virtual bool restore_attributes(const Raw_record &r);

  virtual bool store_attributes(Raw_record *r);

  void serialize(WriterVariant *wv) const;

  void deserialize(const RJ_Document *d);

  void debug_print(std::string &outb) const;

public:
  // Required by Collection_item.
  virtual bool store(Open_dictionary_tables_ctx *otx)
  { return Weak_object_impl::store(otx); }

  // Required by Collection_item.
  virtual bool drop(Open_dictionary_tables_ctx *otx)
  { return Weak_object_impl::drop(otx); }

  virtual void drop();

  // Required by Collection_item.
  virtual bool restore_children(Open_dictionary_tables_ctx *otx)
  { return Weak_object_impl::restore_children(otx); }

  // Required by Collection_item.
  virtual bool drop_children(Open_dictionary_tables_ctx *otx)
  { return Weak_object_impl::drop_children(otx); }

  // Required by Collection_item.
  virtual void set_ordinal_position(uint ordinal_position)
  { }

  // Required by Collection_item.
  virtual uint ordinal_position() const
  { return -1; }

  // Required by Collection_item.
  virtual bool is_hidden() const
  { return false; }

public:
  /////////////////////////////////////////////////////////////////////////
  // Partition.
  /////////////////////////////////////////////////////////////////////////

  using Partition_index::partition;

  virtual Partition &partition();

  Partition_impl &partition_impl()
  { return *m_partition; }

  /////////////////////////////////////////////////////////////////////////
  // Index.
  /////////////////////////////////////////////////////////////////////////

  using Partition_index::index;

  virtual Index &index();

  /////////////////////////////////////////////////////////////////////////
  // Options.
  /////////////////////////////////////////////////////////////////////////

  using Partition_index::options;

  Properties &options()
  { return *m_options; }

  virtual bool set_options_raw(const std::string &options_raw);

  /////////////////////////////////////////////////////////////////////////
  // se_private_data.
  /////////////////////////////////////////////////////////////////////////

  using Partition_index::se_private_data;

  virtual Properties &se_private_data()
  { return *m_se_private_data; }

  virtual bool set_se_private_data_raw(const std::string &se_private_data_raw);

  virtual void set_se_private_data(const Properties &se_private_data);

  /////////////////////////////////////////////////////////////////////////
  // Tablespace.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id tablespace_id() const
  { return m_tablespace_id; }

  virtual void set_tablespace_id(Object_id tablespace_id)
  { m_tablespace_id= tablespace_id; }

public:
  class Factory : public Collection_item_factory
  {
  public:
    Factory(Partition_impl *partition, Index *idx)
     :m_partition(partition),
      m_index(idx)
    { }

    virtual Collection_item *create_item() const;

  private:
    Partition_impl *m_partition;
    Index *m_index;
  };

public:
  virtual Object_key *create_primary_key() const;
  virtual bool has_new_primary_key() const;

private:
  // Fields.

  std::unique_ptr<Properties> m_options;
  std::unique_ptr<Properties> m_se_private_data;

  // References to tightly-coupled objects.

  Partition_impl *m_partition;
  Index *m_index;

  // References to loosely-coupled objects.

  Object_id m_tablespace_id;

#ifndef DBUG_OFF
  Partition_index_impl(const Partition_index_impl &src,
                       Partition_impl *parent, Index *index);

public:
  Partition_index_impl *clone(Partition_impl *parent, Index *index) const
  {
    return new Partition_index_impl(*this, parent, index);
  }
#endif /* !DBUG_OFF */
};

///////////////////////////////////////////////////////////////////////////

class Partition_index_type : public Object_type
{
public:
  virtual void register_tables(Open_dictionary_tables_ctx *otx) const;

  virtual Weak_object *create_object() const
  { return new (std::nothrow) Partition_index_impl(); }
};

///////////////////////////////////////////////////////////////////////////

}

DD_HEADER_END

#endif // DD__PARTITION_INDEX_IMPL_INCLUDED
