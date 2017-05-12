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

#ifndef DD__PARTITION_INDEX_IMPL_INCLUDED
#define DD__PARTITION_INDEX_IMPL_INCLUDED

#include <stddef.h>
#include <sys/types.h>
#include <memory>
#include <new>
#include <string>

#include "dd/impl/types/weak_object_impl.h"     // dd::Weak_object_impl
#include "dd/object_id.h"
#include "dd/properties.h"
#include "dd/sdi_fwd.h"
#include "dd/types/object_type.h"               // dd::Object_type
#include "dd/types/partition_index.h"           // dd::Partition_index

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Open_dictionary_tables_ctx;
class Partition_impl;
class Raw_record;
class Index;
class Object_key;
class Object_table;
class Partition;
class Sdi_rcontext;
class Sdi_wcontext;
class Weak_object;

///////////////////////////////////////////////////////////////////////////

class Partition_index_impl : public Weak_object_impl,
                             public Partition_index
{
public:
  Partition_index_impl();

  Partition_index_impl(Partition_impl *partition, Index *index);

  Partition_index_impl(const Partition_index_impl &src,
                       Partition_impl *parent, Index *index);

  virtual ~Partition_index_impl()
  { }

public:
  virtual const Object_table &object_table() const
  { return Partition_index::OBJECT_TABLE(); }

  virtual bool validate() const;

  virtual bool restore_attributes(const Raw_record &r);

  virtual bool store_attributes(Raw_record *r);

  void serialize(Sdi_wcontext *wctx, Sdi_writer *w) const;

  bool deserialize(Sdi_rcontext *rctx, const RJ_Value &val);

  void debug_print(String_type &outb) const;

  void set_ordinal_position(uint)
  { }

  virtual uint ordinal_position() const
  { return -1; }

public:
  /////////////////////////////////////////////////////////////////////////
  // Partition.
  /////////////////////////////////////////////////////////////////////////

  virtual const Partition &partition() const;

  virtual Partition &partition();

  Partition_impl &partition_impl()
  { return *m_partition; }

  /////////////////////////////////////////////////////////////////////////
  // Index.
  /////////////////////////////////////////////////////////////////////////

  virtual const Index &index() const;

  virtual Index &index();

  /////////////////////////////////////////////////////////////////////////
  // Options.
  /////////////////////////////////////////////////////////////////////////

  virtual const Properties &options() const
  { return *m_options; }

  virtual Properties &options()
  { return *m_options; }

  virtual bool set_options_raw(const String_type &options_raw);

  /////////////////////////////////////////////////////////////////////////
  // se_private_data.
  /////////////////////////////////////////////////////////////////////////

  virtual const Properties &se_private_data() const
  { return *m_se_private_data; }

  virtual Properties &se_private_data()
  { return *m_se_private_data; }

  virtual bool set_se_private_data_raw(const String_type &se_private_data_raw);

  virtual void set_se_private_data(const Properties &se_private_data);

  /////////////////////////////////////////////////////////////////////////
  // Tablespace.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id tablespace_id() const
  { return m_tablespace_id; }

  virtual void set_tablespace_id(Object_id tablespace_id)
  { m_tablespace_id= tablespace_id; }

  // Fix "inherits ... via dominance" warnings
  virtual Weak_object_impl *impl()
  { return Weak_object_impl::impl(); }
  virtual const Weak_object_impl *impl() const
  { return Weak_object_impl::impl(); }

public:
  static Partition_index_impl *restore_item(Partition_impl *partition)
  {
    return new (std::nothrow) Partition_index_impl(partition, NULL);
  }

  static Partition_index_impl *clone(const Partition_index_impl &other,
                                     Partition_impl *partition);

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

#endif // DD__PARTITION_INDEX_IMPL_INCLUDED
