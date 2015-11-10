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

#ifndef DD__FOREIGN_KEY_INCLUDED
#define DD__FOREIGN_KEY_INCLUDED

#include "my_global.h"

#include "dd/types/entity_object.h"    // dd::Entity_object

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Foreign_key_element;
class Index;
class Object_type;
class Table;
template <typename I> class Iterator;
typedef Iterator<Foreign_key_element>       Foreign_key_element_iterator;
typedef Iterator<const Foreign_key_element> Foreign_key_element_const_iterator;

///////////////////////////////////////////////////////////////////////////

class Foreign_key : virtual public Entity_object
{
public:
  static const Object_type &TYPE();
  static const Object_table &OBJECT_TABLE();

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

  const Table &table() const
  { return const_cast<Foreign_key *> (this)->table(); }

  virtual Table &table() = 0;

  /////////////////////////////////////////////////////////////////////////
  // unique_constraint
  /////////////////////////////////////////////////////////////////////////

  const Index &unique_constraint() const
  { return const_cast<Foreign_key *> (this)->unique_constraint(); }

  virtual Index &unique_constraint() = 0;

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
  // the schema name of the referenced table.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &referenced_table_shema_name() const = 0;
  virtual void referenced_table_schema_name(const std::string &name) = 0;

  /////////////////////////////////////////////////////////////////////////
  // the name of the referenced table.
  /////////////////////////////////////////////////////////////////////////

  virtual const std::string &referenced_table_name() const = 0;
  virtual void referenced_table_name(const std::string &name) = 0;

  /////////////////////////////////////////////////////////////////////////
  // Foreign key element collection.
  /////////////////////////////////////////////////////////////////////////

  virtual Foreign_key_element *add_element() = 0;

  virtual Foreign_key_element_const_iterator *elements() const = 0;

  virtual Foreign_key_element_iterator *elements() = 0;

  /////////////////////////////////////////////////////////////////////////
  // Drop this FK from the collection.
  /////////////////////////////////////////////////////////////////////////

  virtual void drop() = 0;
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__FOREIGN_KEY_INCLUDED
