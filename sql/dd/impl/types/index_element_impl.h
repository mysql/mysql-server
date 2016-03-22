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

#ifndef DD__INDEX_ELEMENT_IMPL_INCLUDED
#define DD__INDEX_ELEMENT_IMPL_INCLUDED

#include "my_global.h"

#include "dd/impl/collection_item.h"        // dd::Collection_item
#include "dd/impl/types/index_impl.h"       // dd::Index_impl
#include "dd/impl/types/weak_object_impl.h" // dd::Weak_object_impl
#include "dd/types/index_element.h"         // dd::Index_element
#include "dd/types/object_type.h"           // dd::Object_type

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Index;
class Raw_record;
class Open_dictionary_tables_ctx;

///////////////////////////////////////////////////////////////////////////

class Index_element_impl : public Weak_object_impl,
                           public Index_element,
                           public Collection_item
{
public:
  Index_element_impl()
   :m_ordinal_position(0),
    m_length(-1),
    m_order(Index_element::ORDER_ASC),
    m_hidden(false),
    m_index(NULL),
    m_column(NULL)
  { }

public:
  virtual const Object_table &object_table() const
  { return Index_element::OBJECT_TABLE(); }

  virtual bool validate() const;

  virtual bool store_attributes(Raw_record *r);

  virtual bool restore_attributes(const Raw_record &r);

  void serialize(Sdi_wcontext *wctx, Sdi_writer *w) const;

  bool deserialize(Sdi_rcontext *rctx, const RJ_Value &val);

public:
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

  // Required by Collection_item.
  virtual void set_ordinal_position(uint ordinal_position)
  { m_ordinal_position= ordinal_position; }

public:
  /////////////////////////////////////////////////////////////////////////
  // index.
  /////////////////////////////////////////////////////////////////////////

  using Index_element::index;

  virtual Index &index();

  /////////////////////////////////////////////////////////////////////////
  // column.
  /////////////////////////////////////////////////////////////////////////

  using Index_element::column;

  virtual Column &column()
  { return *m_column; }

  /////////////////////////////////////////////////////////////////////////
  // ordinal_position - Also used by Collection_item
  /////////////////////////////////////////////////////////////////////////

  virtual uint ordinal_position() const
  { return m_ordinal_position; }

  /////////////////////////////////////////////////////////////////////////
  // length.
  /////////////////////////////////////////////////////////////////////////

  virtual uint length() const
  { return m_length; }

  virtual void set_length(uint length)
  { m_length= length; }

  virtual void set_length_null(bool is_null)
  { m_length= (uint) -1; }

  virtual bool is_length_null() const
  { return m_length == (uint) -1; }

  /////////////////////////////////////////////////////////////////////////
  // is_hidden. Also required by Collection_item
  /////////////////////////////////////////////////////////////////////////

  virtual bool is_hidden() const
  { return m_hidden; }

  virtual void set_hidden(bool hidden)
  {
    m_index->invalidate_user_elements_count_cache();
    m_hidden= hidden;
  }

  /////////////////////////////////////////////////////////////////////////
  // order.
  /////////////////////////////////////////////////////////////////////////

  virtual enum_index_element_order order() const
  { return m_order; }

  virtual void set_order(enum_index_element_order order)
  { m_order= order; }


  /////////////////////////////////////////////////////////////////////////
  // Make a clone of this object.  (Renamed to factory_clone to avoid
  // clashing with virtual clone() needed for unit testing)
  /////////////////////////////////////////////////////////////////////////

  virtual Index_element_impl *factory_clone() const
  { return new (std::nothrow) Index_element_impl(*this); }

  // Fix "inherits ... via dominance" warnings
  virtual Weak_object_impl *impl()
  { return Weak_object_impl::impl(); }
  virtual const Weak_object_impl *impl() const
  { return Weak_object_impl::impl(); }

public:
  class Factory : public Collection_item_factory
  {
  public:
    Factory(Index_impl *index, Column *column)
     :m_index(index),
      m_column(column)
    { }

    virtual Collection_item *create_item() const;

  private:
    Index_impl *m_index;
    Column *m_column;
  };

  class Factory_clone : public Collection_item_factory
  {
  public:
    Factory_clone(Index_impl *index, const Index_element &element)
     :m_index(index),
      m_element(element)
    { }

    virtual Collection_item *create_item() const;

  private:
    Index_impl *m_index;
    const Index_element &m_element;
  };

public:
  virtual void debug_print(std::string &outb) const;

public:
  virtual Object_key *create_primary_key() const;
  virtual bool has_new_primary_key() const;

private:
  Index_element_impl(const Index_element_impl &e)
    :Weak_object(),
     m_ordinal_position(e.m_ordinal_position),
     m_length(e.m_length),
     m_order(e.m_order),
     m_hidden(e.m_hidden),
     m_index(e.m_index),
     m_column(e.m_column)
  { }

  virtual ~Index_element_impl()
  { }

private:
  // Fields
  uint m_ordinal_position;
  uint m_length;

  enum_index_element_order m_order;

  bool m_hidden;

  // References to other objects
  Index_impl *m_index;
  Column *m_column;

  Index_element_impl(const Index_element_impl &src, Index_impl *parent,
                     Column *column);

public:
  Index_element_impl *clone(Index_impl *parent, Column *column) const
  {
    return new Index_element_impl(*this, parent, column);
  }
};

///////////////////////////////////////////////////////////////////////////

class Index_element_type : public Object_type
{
public:
  virtual void register_tables(Open_dictionary_tables_ctx *otx) const;

  virtual Weak_object *create_object() const
  { return new (std::nothrow) Index_element_impl(); }
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__INDEX_ELEMENT_IMPL_INCLUDED
