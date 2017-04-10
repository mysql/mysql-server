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

#ifndef DD__FOREIGN_KEY_INCLUDED
#define DD__FOREIGN_KEY_INCLUDED


#include "dd/collection.h"             // dd::Collection
#include "dd/sdi_fwd.h"                // dd::Sdi_wcontext
#include "dd/types/entity_object.h"    // dd::Entity_object

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Foreign_key_element;
class Foreign_key_impl;
class Index;
class Object_type;
class Table;

///////////////////////////////////////////////////////////////////////////

class Foreign_key : virtual public Entity_object
{
public:
  static const Object_type &TYPE();
  static const Object_table &OBJECT_TABLE();
  typedef Collection<Foreign_key_element*> Foreign_key_elements;
  typedef Foreign_key_impl Impl;

public:
  enum enum_rule
  {
    RULE_NO_ACTION= 1,
    RULE_RESTRICT,
    RULE_CASCADE,
    RULE_SET_NULL,
    RULE_SET_DEFAULT
  };

  enum enum_match_option
  {
    OPTION_NONE= 1,
    OPTION_PARTIAL,
    OPTION_FULL,
  };

public:
  virtual ~Foreign_key()
  { };

  /////////////////////////////////////////////////////////////////////////
  // parent table.
  /////////////////////////////////////////////////////////////////////////

  virtual const Table &table() const = 0;

  virtual Table &table() = 0;

  /////////////////////////////////////////////////////////////////////////
  // unique_constraint
  /////////////////////////////////////////////////////////////////////////

  virtual const Index &unique_constraint() const = 0;

  virtual void set_unique_constraint(const Index *unique_constraint) = 0;

  /////////////////////////////////////////////////////////////////////////
  // match_option.
  /////////////////////////////////////////////////////////////////////////

  virtual enum_match_option match_option() const = 0;
  virtual void set_match_option(enum_match_option match_option) = 0;

  /////////////////////////////////////////////////////////////////////////
  // update_rule.
  /////////////////////////////////////////////////////////////////////////

  virtual enum_rule update_rule() const = 0;
  virtual void set_update_rule(enum_rule update_rule) = 0;

  /////////////////////////////////////////////////////////////////////////
  // delete_rule.
  /////////////////////////////////////////////////////////////////////////

  virtual enum_rule delete_rule() const = 0;
  virtual void set_delete_rule(enum_rule delete_rule) = 0;

  /////////////////////////////////////////////////////////////////////////
  // the catalog name of the referenced table.
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &referenced_table_catalog_name() const = 0;
  virtual void referenced_table_catalog_name(const String_type &name) = 0;

  /////////////////////////////////////////////////////////////////////////
  // the schema name of the referenced table.
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &referenced_table_schema_name() const = 0;
  virtual void referenced_table_schema_name(const String_type &name) = 0;

  /////////////////////////////////////////////////////////////////////////
  // the name of the referenced table.
  /////////////////////////////////////////////////////////////////////////

  virtual const String_type &referenced_table_name() const = 0;
  virtual void referenced_table_name(const String_type &name) = 0;

  /////////////////////////////////////////////////////////////////////////
  // Foreign key element collection.
  /////////////////////////////////////////////////////////////////////////

  virtual Foreign_key_element *add_element() = 0;

  virtual const Foreign_key_elements &elements() const = 0;


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
    @return
      @retval false success
      @retval true  failure
  */

  virtual bool deserialize(Sdi_rcontext *rctx, const RJ_Value &val) = 0;
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__FOREIGN_KEY_INCLUDED
