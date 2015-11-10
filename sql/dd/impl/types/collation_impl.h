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

#ifndef DD__COLLATION_IMPL_INCLUDED
#define DD__COLLATION_IMPL_INCLUDED

#include "my_global.h"

#include "dd/impl/os_specific.h"              // DD_HEADER_BEGIN
#include "dd/impl/types/entity_object_impl.h" // dd::Entity_object_impl
#include "dd/types/collation.h"               // dd::Collation
#include "dd/types/dictionary_object_table.h" // dd::Dictionary_object_table
#include "dd/types/object_type.h"             // dd::Object_type

DD_HEADER_BEGIN

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Transaction;

///////////////////////////////////////////////////////////////////////////

class Collation_impl : virtual public Entity_object_impl,
                       virtual public Collation
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

public:
  virtual void debug_print(std::string &outb) const
  {
    char outbuf[1024];
    sprintf(outbuf, "COLLATION OBJECT: id= {OID: %lld}, name= %s,"
      "charset_id= {OID: %lld}, is_compiled= %d, sort_length= %u",
      id(), name().c_str(), m_charset_id,
      m_is_compiled, m_sort_length);
    outb= std::string(outbuf);
  }

private:
  // Fields
  bool m_is_compiled;
  uint m_sort_length;

  // References to other objects
  Object_id m_charset_id;

#ifndef DBUG_OFF
  Collation *clone() const
  {
    return new Collation_impl(*this);
  }
#endif /* !DBUG_OFF */
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

DD_HEADER_END

#endif // DD__COLLATION_IMPL_INCLUDED
