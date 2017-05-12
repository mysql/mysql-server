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

#ifndef DD__CHARSET_IMPL_INCLUDED
#define DD__CHARSET_IMPL_INCLUDED

#include <stdio.h>
#include <sys/types.h>
#include <new>
#include <string>

#include "dd/impl/types/entity_object_impl.h"  // dd::Entity_object_imp
#include "dd/impl/types/weak_object_impl.h"
#include "dd/object_id.h"
#include "dd/types/charset.h"                  // dd::Charset
#include "dd/types/dictionary_object_table.h"  // dd::Dictionary_object_table
#include "dd/types/object_type.h"              // dd::Object_type

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Open_dictionary_tables_ctx;
class Raw_record;
class Weak_object;

///////////////////////////////////////////////////////////////////////////

class Charset_impl : public Entity_object_impl,
                     public Charset
{
public:
  Charset_impl()
   :m_mb_max_length(0),
    m_default_collation_id(INVALID_OBJECT_ID)
  { }

  virtual ~Charset_impl()
  { }

public:
  virtual const Dictionary_object_table &object_table() const
  { return Charset::OBJECT_TABLE(); }

  virtual bool validate() const;

  virtual bool restore_attributes(const Raw_record &r);

  virtual bool store_attributes(Raw_record *r);

public:
  /////////////////////////////////////////////////////////////////////////
  // Default collation.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id default_collation_id() const
  { return m_default_collation_id; }

  virtual void set_default_collation_id(Object_id collation_id)
  { m_default_collation_id= collation_id; }

  /////////////////////////////////////////////////////////////////////////
  // mb_max_length
  /////////////////////////////////////////////////////////////////////////

  virtual uint mb_max_length() const
  { return m_mb_max_length; }

  virtual void set_mb_max_length(uint mb_max_length)
  { m_mb_max_length= mb_max_length; }

  /////////////////////////////////////////////////////////////////////////
  // comment
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &comment() const
  { return m_comment; }

  virtual void set_comment(String_type comment)
  { m_comment= comment; }

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
    sprintf(outbuf, "CHARSET OBJECT: {OID: %lld}, name= %s, "
                    "collation_id= {OID: %lld}, mb_max_length= %u, "
                    "comment= %s", id(), name().c_str(),
                    m_default_collation_id,
                    m_mb_max_length, m_comment.c_str());
    outb= String_type(outbuf);
  }

private:
  uint m_mb_max_length;
  String_type m_comment;

  Object_id m_default_collation_id;

  Charset *clone() const
  {
    return new Charset_impl(*this);
  }
};

///////////////////////////////////////////////////////////////////////////

class Charset_type : public Object_type
{
public:
  virtual void register_tables(Open_dictionary_tables_ctx *otx) const;

  virtual Weak_object *create_object() const
  { return new (std::nothrow) Charset_impl(); }
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__CHARSET_IMPL_INCLUDED
