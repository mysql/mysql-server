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

#ifndef DD__FOREIGN_KEY_ELEMENT_IMPL_INCLUDED
#define DD__FOREIGN_KEY_ELEMENT_IMPL_INCLUDED

#include "my_global.h"

#include "dd/impl/types/weak_object_impl.h"  // dd::Weak_object_impl
#include "dd/types/foreign_key_element.h"    // dd::Foreign_key_element
#include "dd/types/object_type.h"            // dd::Object_id
#include "dd/impl/collection_item.h"         // dd::Collection_item

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Foreign_key_impl;
class Raw_record;
class Open_dictionary_tables_ctx;

///////////////////////////////////////////////////////////////////////////

class Foreign_key_element_impl : public Weak_object_impl,
                                 public Foreign_key_element,
                                 public Collection_item
{
// Foreign keys not supported in the Global DD yet
/* purecov: begin deadcode */
public:
  Foreign_key_element_impl()
    : m_foreign_key(NULL),
      m_column(NULL),
      m_ordinal_position(0)
  { }

  virtual ~Foreign_key_element_impl()
  { }

public:
  virtual const Object_table &object_table() const
  { return Foreign_key_element::OBJECT_TABLE(); }

  virtual bool validate() const;

  virtual bool restore_attributes(const Raw_record &r);

  virtual bool store_attributes(Raw_record *r);

  void serialize(Sdi_wcontext *wctx, Sdi_writer *w) const;

  bool deserialize(Sdi_rcontext *rctx, const RJ_Value &val);

  void debug_print(std::string &outb) const;

public:
  // Required by Collection_item.
  virtual bool store(Open_dictionary_tables_ctx *otx)
  { return Weak_object_impl::store(otx); }

  // Required by Collection_item.
  virtual bool drop(Open_dictionary_tables_ctx *otx) const
  { return Weak_object_impl::drop(otx); }

  // Required by Collection_item, Foreign_key_element.
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

  // Required by Collection_item.
  virtual bool is_hidden() const
  { return false; }

public:
  /////////////////////////////////////////////////////////////////////////
  // Foreign key.
  /////////////////////////////////////////////////////////////////////////

  using Foreign_key_element::foreign_key;

  virtual Foreign_key &foreign_key();

  /////////////////////////////////////////////////////////////////////////
  // column.
  /////////////////////////////////////////////////////////////////////////

  using Foreign_key_element::column;

  virtual Column &column()
  { return *m_column; }

  /////////////////////////////////////////////////////////////////////////
  // ordinal_position. - Also required by Collection_item
  /////////////////////////////////////////////////////////////////////////

  virtual uint ordinal_position() const
  { return m_ordinal_position; }

  /////////////////////////////////////////////////////////////////////////
  // referenced column name.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &referenced_column_name() const
  { return m_referenced_column_name; }

  virtual void referenced_column_name(const std::string &name)
  { m_referenced_column_name= name; }

  // Fix "inherits ... via dominance" warnings
  virtual Weak_object_impl *impl()
  { return Weak_object_impl::impl(); }
  virtual const Weak_object_impl *impl() const
  { return Weak_object_impl::impl(); }

public:
  class Factory : public Collection_item_factory
  {
  public:
    Factory(Foreign_key_impl *fk)
     :m_fk(fk)
    { }

    virtual Collection_item *create_item() const;

  private:
    Foreign_key_impl *m_fk;
  };

public:
  virtual Object_key *create_primary_key() const;
  virtual bool has_new_primary_key() const;

private:
  Foreign_key_impl *m_foreign_key;
  Column *m_column;
  uint m_ordinal_position;
  std::string m_referenced_column_name;

  Foreign_key_element_impl(const Foreign_key_element_impl &src,
                           Foreign_key_impl *parent, Column *column);

public:
  Foreign_key_element_impl *clone(Foreign_key_impl *parent,
                                  Column *column) const
  {
    return new Foreign_key_element_impl(*this, parent, column);
  }
/* purecov: end */
};

///////////////////////////////////////////////////////////////////////////

class Foreign_key_element_type : public Object_type
{
public:
  virtual void register_tables(Open_dictionary_tables_ctx *otx) const;

  virtual Weak_object *create_object() const
  { return new (std::nothrow) Foreign_key_element_impl(); }
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__FOREIGN_KEY_ELEMENT_IMPL_INCLUDED
