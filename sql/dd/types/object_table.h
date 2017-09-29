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

#ifndef DD__OBJECT_TABLE_INCLUDED
#define DD__OBJECT_TABLE_INCLUDED

#include "my_inttypes.h"
#include "sql/dd/string_type.h"                // dd::String_type

class THD;

namespace dd {

///////////////////////////////////////////////////////////////////////////

class Object_table_definition;

///////////////////////////////////////////////////////////////////////////

/**
  This class represents all data dictionary table like
  mysql.tables, mysql.columns and more. This is base class of all
  the classes defined in sql/dd/impl/tables/ headers.

  @note This class may be inherited along different paths
        for some subclasses due to the diamond shaped
        inheritance hierarchy; thus, direct subclasses
        must inherit this class virtually.
*/

class Object_table
{
public:
  virtual const String_type &name() const = 0;

  /**
    Get the definition for the dictionary table for the submitted
    dictionary version.

    @param version   Dictionary version for which to get the definition.

    @return Pointer to the definition of the table.
   */
  virtual const Object_table_definition *table_definition(
                                          uint version) const= 0;

  /**
    Get the definition for the dictionary table for the default
    dictionary version.

    @param thd  Thread context.

    @return Pointer to the definition of the table.
   */
  virtual const Object_table_definition *table_definition(
                                          THD *thd) const= 0;

  /**
    Get the dictionary version to use, given the circumstances.

    @param thd  Thread context.

    @return Dictionary version to use.
   */
  virtual uint default_dd_version(THD *thd) const= 0;

  /**
    Execute low level code for populating the table.

    @return Boolean operation outcome, false if success.
   */
  virtual bool populate(THD *thd) const= 0;

  /*
    Most of Object tables (alias DD tables) are hidden from users,
    but some of them are expected to be visible (not hidden) to user and be
    able to update them, e.g., innodb_index_stats/innodb_table_stats.
  */
  virtual bool hidden() const= 0;

public:
  virtual ~Object_table()
  { }
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__OBJECT_TABLE_INCLUDED
