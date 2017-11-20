/* Copyright (c) 2016, 2017, Oracle and/or its affiliates. All rights reserved.

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

#ifndef DD_TABLES__TRIGGERS_INCLUDED
#define DD_TABLES__TRIGGERS_INCLUDED

#include <string>

#include "sql/dd/impl/raw/raw_record.h"
#include "sql/dd/impl/types/object_table_definition_impl.h"
#include "sql/dd/impl/types/object_table_impl.h"        // dd::Object_table_i...
#include "sql/dd/impl/types/trigger_impl.h"             // dd::Trigger_impl
#include "sql/dd/object_id.h"
#include "sql/dd/string_type.h"

class THD;

namespace dd {
class Object_key;
}  // namespace dd


namespace dd {
namespace tables {

///////////////////////////////////////////////////////////////////////////

class Triggers : virtual public Object_table_impl
{
public:
  static const Triggers &instance()
  {
    static Triggers *s_instance= new Triggers();
    return *s_instance;
  }

  static const String_type &table_name()
  {
    static String_type s_table_name("triggers");
    return s_table_name;
  }

public:
  enum enum_fields
  {
    FIELD_ID,
    FIELD_SCHEMA_ID,
    FIELD_NAME,
    FIELD_EVENT_TYPE,
    FIELD_TABLE_ID,
    FIELD_ACTION_TIMING,
    FIELD_ACTION_ORDER,
    FIELD_ACTION_STATEMENT,
    FIELD_ACTION_STATEMENT_UTF8,
    FIELD_CREATED,
    FIELD_LAST_ALTERED,
    FIELD_SQL_MODE,
    FIELD_DEFINER,
    FIELD_CLIENT_COLLATION_ID,
    FIELD_CONNECTION_COLLATION_ID,
    FIELD_SCHEMA_COLLATION_ID
  };

public:
  Triggers();

  virtual const String_type &name() const
  { return Triggers::table_name(); }

public:

  /**
    Create a key to find all triggers for a given schema.

    @param schema_id   Object_id of the schema.

    @returns Pointer to Object_key.
  */

  static Object_key *create_key_by_schema_id(Object_id schema_id);


  /**
    Create a key to find all triggers for a given table.

    @param table_id   Object_id of the table.

    @returns Pointer to Object_key.
  */

  static Object_key *create_key_by_table_id(Object_id table_id);


  /**
    Find table's Object_id for a given trigger name.

    @param thd          Thread
    @param schema_id    Object_id of schema in which the trigger exists.
    @param trigger_name Name of trigger we are search for.
    @param oid [out]     The Object_id of trigger.

    @returns
      false on success.
      true upon failure.
  */

  static bool get_trigger_table_id(THD *thd,
                                   Object_id schema_id,
                                   const String_type &trigger_name,
                                   Object_id *oid);


private:

  /**
    Create a key to find a trigger by schema_id and trigger name.

    @param schema_id    Object_id of the table.
    @param trigger_name Name of trigger.

    @returns Pointer to Object_key.
  */

  static Object_key *create_key_by_trigger_name(Object_id schema_id,
                                                const char *trigger_name);


  /**
    Get the table id from the record.

    @param r    const Reference to the Raw_record.

    @returns Object_id of the table.
  */

  static Object_id read_table_id(const Raw_record &r);

};

///////////////////////////////////////////////////////////////////////////

}
}

#endif // DD_TABLES__TRIGGERS_INCLUDED
