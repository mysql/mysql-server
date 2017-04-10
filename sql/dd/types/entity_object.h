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

#ifndef DD__ENTITY_OBJECT_INCLUDED
#define DD__ENTITY_OBJECT_INCLUDED


#include "dd/object_id.h"             // dd::Object_id
#include "dd/string_type.h"           // dd::String_type
#include "dd/types/weak_object.h"     // dd::Weak_object



namespace dd {

// Some of enums shared by more than one entity object.

// SQL Modes
enum enum_sql_mode
{
  SM_REAL_AS_FLOAT = 1,
  SM_PIPES_AS_CONCAT,
  SM_ANSI_QUOTES,
  SM_IGNORE_SPACE,
  SM_NOT_USED,
  SM_ONLY_FULL_GROUP_BY,
  SM_NO_UNSIGNED_SUBTRACTION,
  SM_NO_DIR_IN_CREATE,
  SM_POSTGRESQL,
  SM_ORACLE,
  SM_MSSQL,
  SM_DB2,
  SM_MAXDB,
  SM_NO_KEY_OPTIONS,
  SM_NO_TABLE_OPTIONS,
  SM_NO_FIELD_OPTIONS,
  SM_MYSQL323,
  SM_MYSQL40,
  SM_ANSI,
  SM_NO_AUTO_VALUE_ON_ZERO,
  SM_NO_BACKSLASH_ESCAPES,
  SM_STRICT_TRANS_TABLES,
  SM_STRICT_ALL_TABLES,
  SM_NO_ZERO_IN_DATE,
  SM_NO_ZERO_DATE,
  SM_INVALID_DATES,
  SM_ERROR_FOR_DIVISION_BY_ZERO,
  SM_TRADITIONAL,
  SM_NO_AUTO_CREATE_USER,
  SM_HIGH_NOT_PRECEDENCE,
  SM_NO_ENGINE_SUBSTITUTION,
  SM_PAD_CHAR_TO_FULL_LENGTH
};

///////////////////////////////////////////////////////////////////////////

/**
  Base class for dictionary objects which has single column
  integer primary key.

  @note This class may be inherited along different paths
        for some subclasses due to the diamond shaped
        inheritance hierarchy; thus, direct subclasses
        must inherit this class virtually.
*/

class Entity_object : virtual public Weak_object
{
public:
  virtual ~Entity_object()
  { };

  /// The unique dictionary object id.
  virtual Object_id id() const = 0;

  /// Is dictionary object persistent in dictionary tables ?
  virtual bool is_persistent() const = 0;

  virtual const String_type &name() const = 0;
  virtual void set_name(const String_type &name) = 0;
};

///////////////////////////////////////////////////////////////////////////

}

#endif // DD__ENTITY_OBJECT_INCLUDED
