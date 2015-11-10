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

#ifndef DD__CHARSET_IMPL_INCLUDED
#define DD__CHARSET_IMPL_INCLUDED

#include "my_global.h"

#include "dd/impl/os_specific.h"               // DD_HEADER_BEGIN
#include "dd/impl/types/entity_object_impl.h"  // dd::Entity_object_imp
#include "dd/types/charset.h"                  // dd::Charset
#include "dd/types/dictionary_object_table.h"  // dd::Dictionary_object_table
#include "dd/types/object_type.h"              // dd::Object_type

DD_HEADER_BEGIN

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Raw_record;
class Open_dictionary_tables_ctx;

///////////////////////////////////////////////////////////////////////////

class Charset_impl : virtual public Entity_object_impl,
                     virtual public Charset
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

  virtual const std::string &comment() const
  { return m_comment; }

  virtual void set_comment(std::string comment)
  { m_comment= comment; }

public:
  virtual void debug_print(std::string &outb) const
  {
    char outbuf[1024];
    sprintf(outbuf, "CHARSET OBJECT: {OID: %lld}, name= %s, "
                    "collation_id= {OID: %lld}, mb_max_length= %u, "
                    "comment= %s", id(), name().c_str(),
                    m_default_collation_id,
                    m_mb_max_length, m_comment.c_str());
    outb= std::string(outbuf);
  }

private:
  uint m_mb_max_length;
  std::string m_comment;

  Object_id m_default_collation_id;

#ifndef DBUG_OFF
  Charset *clone() const
  {
    return new Charset_impl(*this);
  }
#endif /* !DBUG_OFF */
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

DD_HEADER_END

#endif // DD__CHARSET_IMPL_INCLUDED
