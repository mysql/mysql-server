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

#ifndef DD__ENTITY_OBJECT_TABLE_IMPL_INCLUDED
#define DD__ENTITY_OBJECT_TABLE_IMPL_INCLUDED

#include <sys/types.h>

#include "my_compiler.h"
#include "sql/dd/impl/types/object_table_impl.h" // Object_table_impl
#include "sql/dd/types/entity_object_table.h" // dd::Entity_object_table

class THD;

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Entity_object;
class Object_table_definition;
class Open_dictionary_tables_ctx;
class Properties;
class Raw_record;

class Entity_object_table_impl : public Object_table_impl,
                                 public Entity_object_table
{
public:
  virtual ~Entity_object_table_impl()
  { };

  virtual bool restore_object_from_record(
    Open_dictionary_tables_ctx *otx,
    const Raw_record &record,
    Entity_object **o) const;

  // Fix "inherits ... via dominance" warnings
  virtual Object_table_definition_impl *target_table_definition()
  { return Object_table_impl::target_table_definition(); }

  virtual const Object_table_definition_impl *target_table_definition() const
  { return Object_table_impl::target_table_definition(); }

  virtual void set_abandoned(uint last_dd_version) const
  { return Object_table_impl::set_abandoned(last_dd_version); }

  virtual bool is_abandoned() const
  { return Object_table_impl::is_abandoned(); }

  virtual const Object_table_definition_impl *actual_table_definition() const
  { return Object_table_impl::actual_table_definition(); }

  virtual bool set_actual_table_definition(
    const Properties &table_def_properties) const
  {
    return Object_table_impl::set_actual_table_definition(
            table_def_properties);
  }

  virtual bool populate(THD *thd) const
  { return Object_table_impl::populate(thd); }

  virtual bool is_hidden() const
  { return Object_table_impl::is_hidden(); }

  virtual void set_hidden(bool hidden)
  { return Object_table_impl::set_hidden(hidden); }
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__ENTITY_OBJECT_TABLE_IMPL_INCLUDED
