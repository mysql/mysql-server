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

#ifndef DD__SCHEMA_INCLUDED
#define DD__SCHEMA_INCLUDED

#include "my_global.h"

#include "dd/types/dictionary_object.h"   // dd::Dictionary_object
#include "dd/sdi_fwd.h"                   // RJ_Document

class THD;

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Item_name_key;
class Object_type;
class Primary_id_key;
class Table;
class View;
class Event;
class Function;
class Procedure;
class Void_key;

namespace tables {
  class Schemata;
}

///////////////////////////////////////////////////////////////////////////

class Schema : public Dictionary_object
{
public:
  static const Object_type &TYPE();
  static const Dictionary_object_table &OBJECT_TABLE();

  typedef Schema cache_partition_type;
  typedef tables::Schemata cache_partition_table_type;
  typedef Primary_id_key id_key_type;
  typedef Item_name_key name_key_type;
  typedef Void_key aux_key_type;

  // We need a set of functions to update a preallocated key.
  virtual bool update_id_key(id_key_type *key) const
  { return update_id_key(key, id()); }

  static bool update_id_key(id_key_type *key, Object_id id);

  virtual bool update_name_key(name_key_type *key) const
  { return update_name_key(key, name()); }

  static bool update_name_key(name_key_type *key,
                              const std::string &name);

  virtual bool update_aux_key(aux_key_type *key) const
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

  virtual ulonglong created() const = 0;
  virtual void set_created(ulonglong created) = 0;

  /////////////////////////////////////////////////////////////////////////
  // last_altered
  /////////////////////////////////////////////////////////////////////////

  virtual ulonglong last_altered() const = 0;
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


  /**
    Converts *this into json.

    Converts all member variables that are to be included in the sdi
    into json by transforming them appropriately and passing them to
    the rapidjson writer provided.

    @param wctx opaque context for data needed by serialization
    @param w rapidjson writer which will perform conversion to json

  */

  virtual void serialize(Sdi_wcontext *wctx, Sdi_writer *w) const = 0;


  /**
    Re-establishes the state of *this by reading sdi information from
    the rapidjson DOM subobject provided.

    Cross-references encountered within this object are tracked in
    sdictx, so that they can be updated when the entire object graph
    has been established.

    @param rctx stores book-keeping information for the
    deserialization process
    @param val subobject of rapidjson DOM containing json
    representation of this object
  */

  virtual bool deserialize(Sdi_rcontext *rctx, const RJ_Value &val) = 0;
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__SCHEMA_INCLUDE
