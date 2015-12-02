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

#ifndef DD__PARTITION_IMPL_INCLUDED
#define DD__PARTITION_IMPL_INCLUDED

#include "my_global.h"

#include "dd/impl/os_specific.h"               // DD_HEADER_BEGIN
#include "dd/impl/collection_item.h"           // dd::Collection_item
#include "dd/impl/types/entity_object_impl.h"  // dd::Entity_object_impl
#include "dd/types/object_type.h"              // dd::Object_type
#include "dd/types/partition.h"                // dd::Partition

#include <memory>

DD_HEADER_BEGIN

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Raw_record;
class Table_impl;
class Open_dictionary_tables_ctx;
template <typename T> class Collection;

///////////////////////////////////////////////////////////////////////////

class Partition_impl : virtual public Entity_object_impl,
                       virtual public Partition,
                       virtual public Collection_item
{
public:
  typedef Collection<Partition_value> Value_collection;
  typedef Collection<Partition_index> Index_collection;

public:
  Partition_impl();

  virtual ~Partition_impl()
  { }

public:
  virtual const Object_table &object_table() const
  { return Partition::OBJECT_TABLE(); }

  virtual bool validate() const;

  virtual bool restore_children(Open_dictionary_tables_ctx *otx);

  virtual bool store_children(Open_dictionary_tables_ctx *otx);

  virtual bool drop_children(Open_dictionary_tables_ctx *otx);

  virtual bool restore_attributes(const Raw_record &r);

  virtual bool store_attributes(Raw_record *r);

  void serialize(WriterVariant *wv) const;

  void deserialize(const RJ_Document *d);

  void debug_print(std::string &outb) const;

public:
  // Required by Collection_item.
  virtual bool store(Open_dictionary_tables_ctx *otx)
  { return Entity_object_impl::store(otx); }

  // Required by Collection_item.
  virtual bool drop(Open_dictionary_tables_ctx *otx)
  { return Entity_object_impl::drop(otx); }

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
  virtual void drop();

public:
  /////////////////////////////////////////////////////////////////////////
  // Table.
  /////////////////////////////////////////////////////////////////////////

  using Partition::table;

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

  using Partition::options;

  Properties &options()
  { return *m_options; }

  virtual bool set_options_raw(const std::string &options_raw);

  /////////////////////////////////////////////////////////////////////////
  // se_private_data.
  /////////////////////////////////////////////////////////////////////////

  using Partition::se_private_data;

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

  virtual Partition_value_const_iterator *values() const;

  virtual Partition_value_iterator *values();

  Value_collection *value_collection()
  { return m_values.get(); }

  /////////////////////////////////////////////////////////////////////////
  // Partition-index collection
  /////////////////////////////////////////////////////////////////////////

  virtual Partition_index *add_index(Index *idx);

  virtual Partition_index_const_iterator *indexes() const;

  virtual Partition_index_iterator *indexes();

  Index_collection *index_collection()
  { return m_indexes.get(); }

public:
  class Factory : public Collection_item_factory
  {
  public:
    Factory(Table_impl *table)
     :m_table(table)
    { }

    virtual Collection_item *create_item() const;

  private:
    Table_impl *m_table;
  };

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

  std::unique_ptr<Value_collection> m_values;
  std::unique_ptr<Index_collection> m_indexes;

  // References to loosely-coupled objects.

  Object_id m_tablespace_id;

#ifndef DBUG_OFF
  Partition_impl(const Partition_impl &src, Table_impl *parent);

public:
  Partition_impl *clone(Table_impl *parent) const
  {
    return new Partition_impl(*this, parent);
  }
#endif /* !DBUG_OFF */
};

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////

class Partition_type : public Object_type
{
public:
  virtual void register_tables(Open_dictionary_tables_ctx *otx) const;

public:
  // Used to compare two partition elements by Collection
  // sort interface.
  struct Partition_order_comparator
  {
    inline bool operator() (const Partition* p1,
                            const Partition* p2)
    {
      if (p1->level() == p2->level())
        return p1->number() < p2->number();

      return p1->level() < p2->level();
    }
  };

  virtual Weak_object *create_object() const
  { return new (std::nothrow) Partition_impl(); }
};

///////////////////////////////////////////////////////////////////////////

}

DD_HEADER_END

#endif // DD__PARTITION_IMPL_INCLUDED
