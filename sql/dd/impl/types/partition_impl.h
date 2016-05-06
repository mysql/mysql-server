/* Copyright (c) 2014, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD__PARTITION_IMPL_INCLUDED
#define DD__PARTITION_IMPL_INCLUDED

#include "my_global.h"

#include "dd/impl/types/entity_object_impl.h"  // dd::Entity_object_impl
#include "dd/types/object_type.h"              // dd::Object_type
#include "dd/types/partition.h"                // dd::Partition
#include "dd/types/partition_index.h"          // dd::Partition_index
#include "dd/types/partition_value.h"          // dd::Partition_value

#include <memory>

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Raw_record;
class Table_impl;
class Open_dictionary_tables_ctx;

///////////////////////////////////////////////////////////////////////////

class Partition_impl : public Entity_object_impl,
                       public Partition
{
public:
  Partition_impl();

  Partition_impl(Table_impl *table);

  Partition_impl(const Partition_impl &src, Table_impl *parent);

  virtual ~Partition_impl();

public:
  virtual const Object_table &object_table() const
  { return Partition::OBJECT_TABLE(); }

  virtual bool validate() const;

  virtual bool restore_children(Open_dictionary_tables_ctx *otx);

  virtual bool store_children(Open_dictionary_tables_ctx *otx);

  virtual bool drop_children(Open_dictionary_tables_ctx *otx) const;

  virtual bool restore_attributes(const Raw_record &r);

  virtual bool store_attributes(Raw_record *r);

  void serialize(Sdi_wcontext *wctx, Sdi_writer *w) const;

  bool deserialize(Sdi_rcontext *rctx, const RJ_Value &val);

  void debug_print(std::string &outb) const;

  void set_ordinal_position(uint ordinal_position)
  { }

  virtual uint ordinal_position() const
  { return -1; }

public:
  /////////////////////////////////////////////////////////////////////////
  // Table.
  /////////////////////////////////////////////////////////////////////////

  virtual const Table &table() const;

  virtual Table &table();

  /* non-virtual */ const Table_impl &table_impl() const
  { return *m_table; }

  /* non-virtual */ Table_impl &table_impl()
  { return *m_table; }

  /////////////////////////////////////////////////////////////////////////
  // level
  /////////////////////////////////////////////////////////////////////////

  virtual uint level() const
  { return m_level; }

  virtual void set_level(uint level)
  { m_level= level; }

  /////////////////////////////////////////////////////////////////////////
  // number.
  /////////////////////////////////////////////////////////////////////////

  virtual uint number() const
  { return m_number; }

  virtual void set_number(uint number)
  { m_number= number; }

  /////////////////////////////////////////////////////////////////////////
  // engine.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &engine() const
  { return m_engine; }

  virtual void set_engine(const std::string &engine)
  { m_engine= engine; }

  /////////////////////////////////////////////////////////////////////////
  // comment.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &comment() const
  { return m_comment; }

  virtual void set_comment(const std::string &comment)
  { m_comment= comment; }

  /////////////////////////////////////////////////////////////////////////
  // Options.
  /////////////////////////////////////////////////////////////////////////

  virtual const Properties &options() const
  { return *m_options; }

  virtual Properties &options()
  { return *m_options; }

  virtual bool set_options_raw(const std::string &options_raw);

  /////////////////////////////////////////////////////////////////////////
  // se_private_data.
  /////////////////////////////////////////////////////////////////////////

  virtual const Properties &se_private_data() const
  { return *m_se_private_data; }

  virtual Properties &se_private_data()
  { return *m_se_private_data; }

  virtual bool set_se_private_data_raw(const std::string &se_private_data_raw);

  virtual void set_se_private_data(const Properties &se_private_data);

  /////////////////////////////////////////////////////////////////////////
  // se_private_id.
  /////////////////////////////////////////////////////////////////////////

  virtual ulonglong se_private_id() const
  { return m_se_private_id; }

  virtual void set_se_private_id(ulonglong se_private_id)
  { m_se_private_id= se_private_id; }

  /////////////////////////////////////////////////////////////////////////
  // Tablespace.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id tablespace_id() const
  { return m_tablespace_id; }

  virtual void set_tablespace_id(Object_id tablespace_id)
  { m_tablespace_id= tablespace_id; }

  /////////////////////////////////////////////////////////////////////////
  // Partition-value collection
  /////////////////////////////////////////////////////////////////////////

  virtual Partition_value *add_value();

  virtual const Partition_values &values() const
  { return m_values; }

  /////////////////////////////////////////////////////////////////////////
  // Partition-index collection
  /////////////////////////////////////////////////////////////////////////

  virtual Partition_index *add_index(Index *idx);

  virtual const Partition_indexes &indexes() const
  { return m_indexes; }

  // Fix "inherits ... via dominance" warnings
  virtual Weak_object_impl *impl()
  { return Weak_object_impl::impl(); }
  virtual const Weak_object_impl *impl() const
  { return Weak_object_impl::impl(); }
  virtual Object_id id() const
  { return Entity_object_impl::id(); }
  virtual bool is_persistent() const
  { return Entity_object_impl::is_persistent(); }
  virtual const std::string &name() const
  { return Entity_object_impl::name(); }
  virtual void set_name(const std::string &name)
  { Entity_object_impl::set_name(name); }

public:
  static Partition_impl *restore_item(Table_impl *table)
  {
    return new (std::nothrow) Partition_impl(table);
  }

  static Partition_impl *clone(const Partition_impl &other,
                               Table_impl *table)
  {
    return new (std::nothrow) Partition_impl(other, table);
  }

private:
  // Fields.

  uint m_level;
  uint m_number;
  ulonglong m_se_private_id;

  std::string m_engine;
  std::string m_comment;
  std::unique_ptr<Properties> m_options;
  std::unique_ptr<Properties> m_se_private_data;

  // References to tightly-coupled objects.

  Table_impl *m_table;

  Partition_values m_values;
  Partition_indexes m_indexes;

  // References to loosely-coupled objects.

  Object_id m_tablespace_id;
};

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

class Partition_type : public Object_type
{
public:
  virtual void register_tables(Open_dictionary_tables_ctx *otx) const;

  virtual Weak_object *create_object() const
  { return new (std::nothrow) Partition_impl(); }
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__PARTITION_IMPL_INCLUDED
