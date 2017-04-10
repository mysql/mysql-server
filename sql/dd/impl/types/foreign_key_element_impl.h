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

#ifndef DD__FOREIGN_KEY_ELEMENT_IMPL_INCLUDED
#define DD__FOREIGN_KEY_ELEMENT_IMPL_INCLUDED

#include <stddef.h>
#include <sys/types.h>
#include <new>
#include <string>

#include "dd/impl/types/weak_object_impl.h"  // dd::Weak_object_impl
#include "dd/sdi_fwd.h"
#include "dd/types/foreign_key_element.h"    // dd::Foreign_key_element
#include "dd/types/object_type.h"            // dd::Object_id

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Foreign_key_impl;
class Open_dictionary_tables_ctx;
class Raw_record;
class Column;
class Foreign_key;
class Object_key;
class Object_table;
class Sdi_rcontext;
class Sdi_wcontext;
class Weak_object;

///////////////////////////////////////////////////////////////////////////

class Foreign_key_element_impl : public Weak_object_impl,
                                 public Foreign_key_element
{
public:
  Foreign_key_element_impl()
    : m_foreign_key(NULL),
      m_column(NULL),
      m_ordinal_position(0)
  { }

  Foreign_key_element_impl(Foreign_key_impl *foreign_key)
    : m_foreign_key(foreign_key),
      m_column(NULL),
      m_ordinal_position(0)
  { }

  Foreign_key_element_impl(const Foreign_key_element_impl &src,
                           Foreign_key_impl *parent, Column *column);

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

  void debug_print(String_type &outb) const;

  void set_ordinal_position(uint ordinal_position)
  { m_ordinal_position= ordinal_position; }

public:
  /////////////////////////////////////////////////////////////////////////
  // Foreign key.
  /////////////////////////////////////////////////////////////////////////

  virtual const Foreign_key &foreign_key() const;

  virtual Foreign_key &foreign_key();

  /////////////////////////////////////////////////////////////////////////
  // column.
  /////////////////////////////////////////////////////////////////////////

  virtual const Column &column() const
  { return *m_column; }

  virtual void set_column(const Column *column)
  { m_column= column; }

  /////////////////////////////////////////////////////////////////////////
  // ordinal_position.
  /////////////////////////////////////////////////////////////////////////

  virtual uint ordinal_position() const
  { return m_ordinal_position; }

  virtual void set_ordinal_position(int ordinal_position)
  { m_ordinal_position= ordinal_position; }

  /////////////////////////////////////////////////////////////////////////
  // referenced column name.
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &referenced_column_name() const
  { return m_referenced_column_name; }

  virtual void referenced_column_name(const String_type &name)
  { m_referenced_column_name= name; }

  // Fix "inherits ... via dominance" warnings
  virtual Weak_object_impl *impl()
  { return Weak_object_impl::impl(); }
  virtual const Weak_object_impl *impl() const
  { return Weak_object_impl::impl(); }

public:
  static Foreign_key_element_impl *restore_item(Foreign_key_impl *fk)
  {
    return new (std::nothrow) Foreign_key_element_impl(fk);
  }

  static Foreign_key_element_impl *clone(const Foreign_key_element_impl &other,
                                         Foreign_key_impl *fk);

public:
  virtual Object_key *create_primary_key() const;
  virtual bool has_new_primary_key() const;

private:
  Foreign_key_impl *m_foreign_key;
  const Column *m_column;
  uint m_ordinal_position;
  String_type m_referenced_column_name;
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
