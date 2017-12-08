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

#ifndef DD__SCHEMA_INCLUDED
#define DD__SCHEMA_INCLUDED

#include "my_inttypes.h"
#include "sql/dd/impl/raw/object_keys.h"  // IWYU pragma: keep
#include "sql/dd/sdi_fwd.h"               // RJ_Document
#include "sql/dd/types/entity_object.h" // dd::Entity_object

class THD;

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Item_name_key;
class Primary_id_key;
class Schema_impl;
class Table;
class View;
class Event;
class Function;
class Procedure;
class Void_key;
class Time_zone;

namespace tables {
  class Schemata;
}

///////////////////////////////////////////////////////////////////////////

class Schema : virtual public Entity_object
{
public:
  typedef Schema_impl Impl;
  typedef Schema Cache_partition;
  typedef tables::Schemata DD_table;
  typedef Primary_id_key Id_key;
  typedef Item_name_key Name_key;
  typedef Void_key Aux_key;

  // We need a set of functions to update a preallocated key.
  virtual bool update_id_key(Id_key *key) const
  { return update_id_key(key, id()); }

  static bool update_id_key(Id_key *key, Object_id id);

  virtual bool update_name_key(Name_key *key) const
  { return update_name_key(key, name()); }

  static bool update_name_key(Name_key *key,
                              const String_type &name);

  virtual bool update_aux_key(Aux_key*) const
  { return true; }

public:
  virtual ~Schema()
  { };

  /////////////////////////////////////////////////////////////////////////
  // Default collation.
  /////////////////////////////////////////////////////////////////////////

  virtual Object_id default_collation_id() const = 0;
  virtual void set_default_collation_id(Object_id default_collation_id) = 0;

  /////////////////////////////////////////////////////////////////////////
  // created
  /////////////////////////////////////////////////////////////////////////

  virtual ulonglong created(bool convert_time) const = 0;
  virtual void set_created(ulonglong created) = 0;

  /////////////////////////////////////////////////////////////////////////
  // last_altered
  /////////////////////////////////////////////////////////////////////////

  virtual ulonglong last_altered(bool convert_time) const = 0;
  virtual void set_last_altered(ulonglong last_altered) = 0;

public:
  virtual Event *create_event(THD *thd) const = 0;

  virtual Function *create_function(THD *thd) const = 0;

  virtual Procedure *create_procedure(THD *thd) const = 0;

  virtual Table *create_table(THD *thd) const = 0;

  virtual View *create_view(THD *thd) const = 0;

  virtual View *create_system_view(THD *thd) const = 0;

  /**
    Allocate a new object and invoke the copy contructor.

    @return pointer to dynamically allocated copy
  */
  virtual Schema *clone() const = 0;
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__SCHEMA_INCLUDE
