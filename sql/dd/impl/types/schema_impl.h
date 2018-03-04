/* Copyright (c) 2014, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef DD__SCHEMA_IMPL_INCLUDED
#define DD__SCHEMA_IMPL_INCLUDED

#include <stdio.h>
#include <new>
#include <string>

#include "my_inttypes.h"
#include "sql/dd/impl/types/entity_object_impl.h" // dd::Entity_object_impl
#include "sql/dd/impl/types/weak_object_impl.h"
#include "sql/dd/object_id.h"
#include "sql/dd/sdi_fwd.h"
#include "sql/dd/string_type.h"
#include "sql/dd/types/entity_object_table.h" // dd::Entity_object_table
#include "sql/dd/types/schema.h"              // dd::Schema
#include "sql/sql_time.h"                     // gmt_time_to_local_time

class THD;

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Event;
class Function;
class Object_table;
class Open_dictionary_tables_ctx;
class Procedure;
class Raw_record;
class Sdi_rcontext;
class Sdi_wcontext;
class Table;
class View;
class Weak_object;
class Object_table;

///////////////////////////////////////////////////////////////////////////

class Schema_impl : public Entity_object_impl,
                    public Schema
{
public:
  Schema_impl()
   :m_created(0),
    m_last_altered(0),
    m_default_collation_id(INVALID_OBJECT_ID)
  { }

  virtual ~Schema_impl()
  { }

public:
  virtual const Object_table &object_table() const;

  virtual bool validate() const;

  virtual bool store_attributes(Raw_record *r);

  virtual bool restore_attributes(const Raw_record &r);

public:
  static void register_tables(Open_dictionary_tables_ctx *otx);

  /////////////////////////////////////////////////////////////////////////
  // Default collation.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id default_collation_id() const
  { return m_default_collation_id; }

  virtual void set_default_collation_id(Object_id default_collation_id)
  { m_default_collation_id= default_collation_id; }

  /////////////////////////////////////////////////////////////////////////
  // created
  /////////////////////////////////////////////////////////////////////////

  virtual ulonglong created(bool convert_time) const
  { return convert_time ? gmt_time_to_local_time(m_created) : m_created; }

  virtual void set_created(ulonglong created)
  { m_created= created; }

  /////////////////////////////////////////////////////////////////////////
  // last_altered
  /////////////////////////////////////////////////////////////////////////

  virtual ulonglong last_altered(bool convert_time) const
  {
    return convert_time ? gmt_time_to_local_time(m_last_altered) :
                          m_last_altered;
  }

  virtual void set_last_altered(ulonglong last_altered)
  { m_last_altered= last_altered; }

  // Fix "inherits ... via dominance" warnings
  virtual Entity_object_impl *impl()
  { return Entity_object_impl::impl(); }
  virtual const Entity_object_impl *impl() const
  { return Entity_object_impl::impl(); }
  virtual Object_id id() const
  { return Entity_object_impl::id(); }
  virtual bool is_persistent() const
  { return Entity_object_impl::is_persistent(); }
  virtual const String_type &name() const
  { return Entity_object_impl::name(); }
  virtual void set_name(const String_type &name)
  { Entity_object_impl::set_name(name); }

public:

  virtual Event *create_event(THD *thd) const;

  virtual Function *create_function(THD *thd) const;

  virtual Procedure *create_procedure(THD *thd) const;

  virtual Table *create_table(THD *thd) const;

  virtual View *create_view(THD *thd) const;

  virtual View *create_system_view(THD *thd) const;

public:
  virtual void debug_print(String_type &outb) const
  {
    char outbuf[1024];
    sprintf(outbuf, "SCHEMA OBJECT: id= {OID: %lld}, name= %s, "
      "collation_id={OID: %lld},"
      "m_created= %llu, m_last_altered= %llu",
      id(), name().c_str(),
      m_default_collation_id,
      m_created, m_last_altered);
    outb= String_type(outbuf);
  }

private:
  // Fields
  ulonglong m_created;
  ulonglong m_last_altered;

  // References to other objects
  Object_id m_default_collation_id;

  Schema *clone() const
  {
    return new Schema_impl(*this);
  }
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__SCHEMA_IMPL_INCLUDED
