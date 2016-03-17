/* Copyright (c) 2016 Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD_TABLES__ROUTINES_INCLUDED
#define DD_TABLES__ROUTINES_INCLUDED

#include "my_global.h"

#include "dd/impl/types/dictionary_object_table_impl.h" // dd::Dictionary_obj...
#include "dd/types/routine.h"                           // dd::Routine

namespace dd {
  class Object_key;
namespace tables {

///////////////////////////////////////////////////////////////////////////

class Routines : public Dictionary_object_table_impl
{
public:
  static const Routines &instance()
  {
    static Routines *s_instance= new Routines();
    return *s_instance;
  }

  static const std::string &table_name()
  {
    static std::string s_table_name("routines");
    return s_table_name;
  }

public:
  enum enum_fields
  {
    FIELD_ID,
    FIELD_SCHEMA_ID,
    FIELD_NAME,
    FIELD_TYPE,
    FIELD_RESULT_DATA_TYPE,
    FIELD_RESULT_IS_ZEROFILL,
    FIELD_RESULT_IS_UNSIGNED,
    FIELD_RESULT_CHAR_LENGTH,
    FIELD_RESULT_NUMERIC_PRECISION,
    FIELD_RESULT_NUMERIC_SCALE,
    FIELD_RESULT_DATETIME_PRECISION,
    FIELD_RESULT_COLLATION_ID,
    FIELD_DEFINITION,
    FIELD_DEFINITION_UTF8,
    FIELD_PARAMETER_STR,
    FIELD_IS_DETERMINISTIC,
    FIELD_SQL_DATA_ACCESS,
    FIELD_SECURITY_TYPE,
    FIELD_DEFINER,
    FIELD_SQL_MODE,
    FIELD_CLIENT_COLLATION_ID,
    FIELD_CONNECTION_COLLATION_ID,
    FIELD_SCHEMA_COLLATION_ID,
    FIELD_CREATED,
    FIELD_LAST_ALTERED,
    FIELD_COMMENT
  };

public:
  Routines();

  virtual const std::string &name() const
  { return Routines::table_name(); }

  virtual Dictionary_object *create_dictionary_object(
                               const Raw_record &) const;

public:
  static bool update_object_key(Routine_name_key *key,
                                Object_id schema_id,
                                Routine::enum_routine_type type,
                                const std::string &routine_name);

  static Object_key *create_key_by_schema_id(Object_id schema_id);
};

///////////////////////////////////////////////////////////////////////////

}
}

#endif // DD_TABLES__ROUTINES_INCLUDED
