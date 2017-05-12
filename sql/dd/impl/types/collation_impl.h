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

#ifndef DD__COLLATION_IMPL_INCLUDED
#define DD__COLLATION_IMPL_INCLUDED

#include <stdio.h>
#include <sys/types.h>
#include <new>
#include <string>

#include "dd/impl/raw/raw_record.h"
#include "dd/impl/types/entity_object_impl.h" // dd::Entity_object_impl
#include "dd/impl/types/weak_object_impl.h"
#include "dd/object_id.h"
#include "dd/types/collation.h"               // dd::Collation
#include "dd/types/dictionary_object.h"
#include "dd/types/dictionary_object_table.h" // dd::Dictionary_object_table
#include "dd/types/object_type.h"             // dd::Object_type

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Transaction;
class Open_dictionary_tables_ctx;

///////////////////////////////////////////////////////////////////////////

class Collation_impl : public Entity_object_impl,
                       public Collation
{
public:
  Collation_impl()
   :m_is_compiled(false),
    m_sort_length(0),
    m_charset_id(INVALID_OBJECT_ID)
  { }

  virtual ~Collation_impl()
  { }

public:
  virtual const Dictionary_object_table &object_table() const
  { return Collation::OBJECT_TABLE(); }

  virtual bool validate() const;

  virtual bool restore_attributes(const Raw_record &r);

  virtual bool store_attributes(Raw_record *r);

public:
  /////////////////////////////////////////////////////////////////////////
  // Character set.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id charset_id() const
  { return m_charset_id; }

  virtual void set_charset_id(Object_id charset_id)
  { m_charset_id= charset_id; }

  /////////////////////////////////////////////////////////////////////////
  // compiled
  /////////////////////////////////////////////////////////////////////////

  virtual bool is_compiled() const
  { return m_is_compiled; }

  virtual void set_is_compiled(bool is_compiled)
  { m_is_compiled= is_compiled; }

  /////////////////////////////////////////////////////////////////////////
  // sort_length
  /////////////////////////////////////////////////////////////////////////

  virtual uint sort_length() const
  { return m_sort_length; }

  virtual void set_sort_length(uint sort_length)
  { m_sort_length= sort_length; }

  /////////////////////////////////////////////////////////////////////////
  // pad_attribute
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &pad_attribute() const
  { return m_pad_attribute; }

  virtual void set_pad_attribute(const String_type &pad_attribute)
  { m_pad_attribute= pad_attribute; }

  // Fix "inherits ... via dominance" warnings
  virtual Weak_object_impl *impl()
  { return Weak_object_impl::impl(); }
  virtual const Weak_object_impl *impl() const
  { return Weak_object_impl::impl(); }
  virtual Object_id id() const
  { return Entity_object_impl::id(); }
  virtual bool is_persistent() const
  { return Entity_object_impl::is_persistent(); }
  virtual const String_type &name() const
  { return Entity_object_impl::name(); }
  virtual void set_name(const String_type &name)
  { Entity_object_impl::set_name(name); }

public:
  virtual void debug_print(String_type &outb) const
  {
    char outbuf[1024];
    sprintf(outbuf, "COLLATION OBJECT: id= {OID: %lld}, name= %s,"
      "charset_id= {OID: %lld}, is_compiled= %d, sort_length= %u",
      id(), name().c_str(), m_charset_id,
      m_is_compiled, m_sort_length);
    outb= String_type(outbuf);
  }

private:
  // Fields
  bool m_is_compiled;
  uint m_sort_length;
  String_type m_pad_attribute;

  // References to other objects
  Object_id m_charset_id;

  Collation *clone() const
  {
    return new Collation_impl(*this);
  }
};

///////////////////////////////////////////////////////////////////////////

class Collation_type : public Object_type
{
public:
  virtual Dictionary_object *create_object() const
  { return new (std::nothrow) Collation_impl(); }

  virtual void register_tables(Open_dictionary_tables_ctx *otx) const;
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__COLLATION_IMPL_INCLUDED
