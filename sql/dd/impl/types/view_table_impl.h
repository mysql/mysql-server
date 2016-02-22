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

#ifndef DD__VIEW_TABLE_IMPL_INCLUDED
#define DD__VIEW_TABLE_IMPL_INCLUDED

#include "dd/impl/collection_item.h"         // dd::Collection_item
#include "dd/impl/types/weak_object_impl.h"  // dd::Weak_object_impl
#include "dd/types/object_type.h"            // dd::Object_type
#include "dd/types/view_table.h"             // dd::View_table

namespace dd {

///////////////////////////////////////////////////////////////////////////

class View;
class View_impl;

///////////////////////////////////////////////////////////////////////////

class View_table_impl : public Weak_object_impl,
                        public View_table,
                        public Collection_item
{
public:
  View_table_impl();

  virtual ~View_table_impl()
  { }

public:
  virtual const Object_table &object_table() const
  { return View_table::OBJECT_TABLE(); }

  virtual bool validate() const;

  virtual bool store_attributes(Raw_record *r);

  virtual bool restore_attributes(const Raw_record &r);

  virtual void debug_print(std::string &outb) const;

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
  { }

  // Required by Collection_item.
  virtual uint ordinal_position() const
  { return -1; }

  // Required by Collection_item.
  virtual bool is_hidden() const
  { return false; }

public:
  /////////////////////////////////////////////////////////////////////////
  //table_catalog.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &table_catalog() const
  { return m_table_catalog; }

  virtual void set_table_catalog(const std::string &table_catalog)
  { m_table_catalog= table_catalog; }

  /////////////////////////////////////////////////////////////////////////
  //table_schema.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &table_schema() const
  { return m_table_schema; }

  virtual void set_table_schema(const std::string &table_schema)
  { m_table_schema= table_schema; }

  /////////////////////////////////////////////////////////////////////////
  //table_name.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &table_name() const
  { return m_table_name; }

  virtual void set_table_name(const std::string &table_name)
  { m_table_name= table_name; }

  /////////////////////////////////////////////////////////////////////////
  //view.
  /////////////////////////////////////////////////////////////////////////

  using View_table::view;

  virtual View &view();

  // Fix "inherits ... via dominance" warnings
  virtual Weak_object_impl *impl()
  { return Weak_object_impl::impl(); }
  virtual const Weak_object_impl *impl() const
  { return Weak_object_impl::impl(); }

public:
  class Factory : public Collection_item_factory
  {
  public:
    Factory(View_impl *ts)
     :m_ts(ts)
    { }

    virtual Collection_item *create_item() const;

  private:
    View_impl *m_ts;
  };

public:
  virtual Object_key *create_primary_key() const;
  virtual bool has_new_primary_key() const;

private:
  std::string m_table_catalog;
  std::string m_table_schema;
  std::string m_table_name;

  // References to other objects
  View_impl *m_view;

  View_table_impl(const View_table_impl &src,
                  View_impl *parent);

public:
  View_table_impl *clone(View_impl *parent) const
  {
    return new View_table_impl(*this, parent);
  }
};

///////////////////////////////////////////////////////////////////////////

class View_table_type : public Object_type
{
public:
  virtual void register_tables(Open_dictionary_tables_ctx *otx) const;

  virtual Weak_object *create_object() const
  { return new (std::nothrow) View_table_impl(); }
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__VIEW_TABLE_IMPL_INCLUDED
