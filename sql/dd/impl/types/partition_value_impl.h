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

#ifndef DD__PARTITION_VALUE_IMPL_INCLUDED
#define DD__PARTITION_VALUE_IMPL_INCLUDED

#include "my_global.h"

#include "dd/impl/collection_item.h"         // dd::Collection_item
#include "dd/impl/os_specific.h"             // DD_HEADER_BEGIN
#include "dd/impl/types/weak_object_impl.h"  // dd::Weak_object_impl
#include "dd/types/object_type.h"            // dd::Object_type
#include "dd/types/partition_value.h"        // dd::Partition_value

DD_HEADER_BEGIN

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Partition_impl;
class Raw_record;
class Open_dictionary_tables_ctx;

///////////////////////////////////////////////////////////////////////////

class Partition_value_impl : virtual public Weak_object_impl,
                             virtual public Partition_value,
                             virtual public Collection_item
{
public:
  Partition_value_impl()
   :m_max_value(false),
    m_null_value(false),
    m_list_num(0),
    m_column_num(0),
    m_partition(NULL)
  { }

  virtual ~Partition_value_impl()
  { }

public:
  virtual const Object_table &object_table() const
  { return Partition_value::OBJECT_TABLE(); }

  virtual bool validate() const;

  virtual bool store_attributes(Raw_record *r);

  virtual bool restore_attributes(const Raw_record &r);

  void serialize(WriterVariant *wv) const;

  void deserialize(const RJ_Document *d);

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
  // index.
  /////////////////////////////////////////////////////////////////////////

  using Partition_value::partition;

  virtual Partition &partition();

  /////////////////////////////////////////////////////////////////////////
  // list_num.
  /////////////////////////////////////////////////////////////////////////

  virtual uint list_num() const
  { return m_list_num; }

  virtual void set_list_num(uint list_num)
  { m_list_num= list_num; }

  /////////////////////////////////////////////////////////////////////////
  // column_num.
  /////////////////////////////////////////////////////////////////////////

  virtual uint column_num() const
  { return m_column_num; }

  virtual void set_column_num(uint column_num)
  { m_column_num= column_num; }

  /////////////////////////////////////////////////////////////////////////
  // value.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &value_utf8() const
  { return m_value_utf8; }

  virtual void set_value_utf8(const std::string &value)
  { m_value_utf8= value; }

////////////////////////////////////////////////////////////////
  // max_value.
  /////////////////////////////////////////////////////////////////////////

  virtual bool max_value() const
  { return m_max_value; }

  virtual void set_max_value(bool max_value)
  { m_max_value= max_value; }

  ////////////////////////////////////////////////////////////////
  // null_value.
  /////////////////////////////////////////////////////////////////////////

  virtual bool is_value_null() const
  { return m_null_value; }

  virtual void set_value_null(bool is_null)
  { m_null_value= is_null; }

  /////////////////////////////////////////////////////////////////////////
public:
  class Factory : public Collection_item_factory
  {
  public:
    Factory(Partition_impl *partition)
     :m_partition(partition)
    { }

    virtual Collection_item *create_item() const;

  private:
    Partition_impl *m_partition;
  };

public:
  virtual void debug_print(std::string &outb) const;

public:
  virtual Object_key *create_primary_key() const;
  virtual bool has_new_primary_key() const;

private:
  // Fields
  bool m_max_value;
  bool m_null_value;

  uint m_list_num;
  uint m_column_num;

  std::string m_value_utf8;

  // References to other objects
  Partition_impl *m_partition;

#ifndef DBUG_OFF
  Partition_value_impl(const Partition_value_impl &src,
                       Partition_impl *parent);

public:
  Partition_value_impl *clone(Partition_impl *parent) const
  {
    return new Partition_value_impl(*this, parent);
  }
#endif /* !DBUG_OFF */
};

///////////////////////////////////////////////////////////////////////////

class Partition_value_type : public Object_type
{
public:
  virtual void register_tables(Open_dictionary_tables_ctx *otx) const;

  virtual Weak_object *create_object() const
  { return new (std::nothrow) Partition_value_impl(); }
};

///////////////////////////////////////////////////////////////////////////

}

DD_HEADER_END

#endif // DD__PARTITION_VALUE_IMPL_INCLUDED
