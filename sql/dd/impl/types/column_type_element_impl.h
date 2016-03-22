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

#ifndef DD__COLUMN_TYPE_ELEMENT_IMPL_INCLUDED
#define DD__COLUMN_TYPE_ELEMENT_IMPL_INCLUDED

#include "my_global.h"

#include "dd/impl/collection_item.h"          // dd::Collection_item
#include "dd/impl/types/weak_object_impl.h"   // dd::Weak_object_impl
#include "dd/types/column_type_element.h"     // dd::Column_type_element
#include "dd/types/object_type.h"             // dd::Object_type

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Raw_record;
class Open_dictionary_tables_ctx;
class Column_impl;
template <typename T> class Collection;

///////////////////////////////////////////////////////////////////////////

class Column_type_element_impl : public Weak_object_impl,
                                 public Column_type_element,
                                 public Collection_item
{
public:
  Column_type_element_impl()
   :m_index(0)
  { }

  virtual ~Column_type_element_impl()
  { }

public:
  virtual const Object_table &object_table() const
  { return Column_type_element::OBJECT_TABLE(); }

  virtual bool validate() const;

  virtual bool store_attributes(Raw_record *r);

  virtual bool restore_attributes(const Raw_record &r);

  // Required by Collection_item.
  virtual void set_ordinal_position(uint ordinal_position)
  { m_index= ordinal_position; }

  // Required by Collection_item.
  virtual uint ordinal_position() const
  { return index(); }

  // Required by Collection_item.
  virtual bool is_hidden() const
  { return false; }

  // Required by Collection_item.
  virtual bool store(Open_dictionary_tables_ctx *otx)
  { return Weak_object_impl::store(otx); }

  // Required by Collection_item.
  virtual bool drop(Open_dictionary_tables_ctx *otx) const
  { return Weak_object_impl::drop(otx); }

  virtual void drop();

  // Required by Collection_item.
  virtual bool restore_children(Open_dictionary_tables_ctx *otx)
  { return Weak_object_impl::restore_children(otx); }

  // Required by Collection_item.
  virtual bool drop_children(Open_dictionary_tables_ctx *otx) const
  { return Weak_object_impl::drop_children(otx); }

  void serialize(Sdi_wcontext *wctx, Sdi_writer *w) const;

  bool deserialize(Sdi_rcontext *rctx, const RJ_Value &val);

public:
  class Factory : public Collection_item_factory
  {
  public:
    Factory(Column_impl *c, Collection<Column_type_element> *collection)
     :m_column(c),
      m_collection(collection)
    { }

    virtual Collection_item *create_item() const;

  private:
    Column_impl *m_column;
    Collection<Column_type_element> *m_collection;
  };

public:
  /////////////////////////////////////////////////////////////////////////
  // Name.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &name() const
  { return m_name; }

  virtual void set_name(const std::string &name)
  { m_name= name; }

  /////////////////////////////////////////////////////////////////////////
  // Column
  /////////////////////////////////////////////////////////////////////////

  virtual const Column &column() const;

  /////////////////////////////////////////////////////////////////////////
  // index.
  /////////////////////////////////////////////////////////////////////////

  virtual uint index() const
  { return m_index; }

  // Fix "inherits ... via dominance" warnings
  virtual Weak_object_impl *impl()
  { return Weak_object_impl::impl(); }
  virtual const Weak_object_impl *impl() const
  { return Weak_object_impl::impl(); }

public:
  virtual void debug_print(std::string &outb) const;

protected:
  virtual Object_key *create_primary_key() const;
  virtual bool has_new_primary_key() const;

protected:
  // Fields
  std::string m_name;
  uint m_index;

  // References to other objects
  Column_impl *m_column;

  // A pointer to the collection owning this item.
  Collection<Column_type_element> *m_collection;

  Column_type_element_impl(const Column_type_element_impl &src,
                           Column_impl *parent,
                           Collection<Column_type_element> *owner);

public:
  Column_type_element_impl *clone(Column_impl *parent,
                                  Collection<Column_type_element> *owner) const
  {
    return new Column_type_element_impl(*this, parent, owner);
  }
};

///////////////////////////////////////////////////////////////////////////

class Column_type_element_type : public Object_type
{
public:
  virtual void register_tables(Open_dictionary_tables_ctx *otx) const;

  virtual Weak_object *create_object() const
  { return new (std::nothrow) Column_type_element_impl(); }
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__COLUMN_TYPE_ELEMENT_IMPL_INCLUDED
