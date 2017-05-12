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

#ifndef DD__SCHEMA_IMPL_INCLUDED
#define DD__SCHEMA_IMPL_INCLUDED

#include <stdio.h>
#include <new>
#include <string>

#include "dd/impl/types/entity_object_impl.h" // dd::Entity_object_impl
#include "dd/impl/types/weak_object_impl.h"
#include "dd/object_id.h"
#include "dd/sdi_fwd.h"
#include "dd/types/dictionary_object_table.h" // dd::Dictionary_object_table
#include "dd/types/object_type.h"             // dd::Object_type
#include "dd/types/schema.h"                  // dd:Schema
#include "my_inttypes.h"

class THD;

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Event;
class Function;
class Open_dictionary_tables_ctx;
class Procedure;
class Raw_record;
class Sdi_rcontext;
class Sdi_wcontext;
class Table;
class View;
class Weak_object;

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
  virtual const Dictionary_object_table &object_table() const
  { return Schema::OBJECT_TABLE(); }

  virtual bool validate() const;

  virtual bool store_attributes(Raw_record *r);

  virtual bool restore_attributes(const Raw_record &r);

  void serialize(Sdi_wcontext *wctx, Sdi_writer *w) const;

  bool deserialize(Sdi_rcontext *rctx, const RJ_Value &val);

public:
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

  virtual ulonglong created() const
  { return m_created; }

  virtual void set_created(ulonglong created)
  { m_created= created; }

  /////////////////////////////////////////////////////////////////////////
  // last_altered
  /////////////////////////////////////////////////////////////////////////

  virtual ulonglong last_altered() const
  { return m_last_altered; }

  virtual void set_last_altered(ulonglong last_altered)
  { m_last_altered= last_altered; }

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

class Schema_type : public Object_type
{
public:
  virtual void register_tables(Open_dictionary_tables_ctx *otx) const;

  virtual Weak_object *create_object() const
  { return new (std::nothrow) Schema_impl(); }
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__SCHEMA_IMPL_INCLUDED
