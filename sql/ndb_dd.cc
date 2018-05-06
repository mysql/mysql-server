/*
   Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

// Implements the functions defined in ndb_dd.h
#include "sql/ndb_dd.h"

// Using
#include "sql/ndb_dd_client.h"
#include "sql/ndb_dd_table.h"
#include "sql/ndb_dd_sdi.h"
#include "sql/ndb_name_util.h"

#include "sql/sql_class.h"
#include "sql/table.h"
#include "sql/thd_raii.h"
#include "sql/transaction.h"

#include "sql/dd/dd.h"
#include "sql/dd/properties.h"
#include "sql/dd/types/table.h"

bool ndb_sdi_serialize(THD *thd,
                       const dd::Table *table_def,
                       const char* schema_name,
                       dd::sdi_t& sdi)
{
  // Require the table to be visible, hidden by SE(like mysql.ndb_schema)
  // or else have temporary name
  DBUG_ASSERT(table_def->hidden() == dd::Abstract_table::HT_VISIBLE ||
              table_def->hidden() == dd::Abstract_table::HT_HIDDEN_SE ||
              ndb_name_is_temp(table_def->name().c_str()));

  // Make a copy of the table definition to allow it to
  // be modified before serialization
  std::unique_ptr<dd::Table> table_def_clone(table_def->clone());

  // Don't include the se_private_id in the serialized table def.
  table_def_clone->set_se_private_id(dd::INVALID_OBJECT_ID);

  // Don't include any se_private_data properties in the
  // serialized table def.
  table_def_clone->se_private_data().clear();

  sdi = ndb_dd_sdi_serialize(thd, *table_def_clone,
                             dd::String_type(schema_name));
  if (sdi.empty())
    return false; // Failed to serialize

  return true; // OK
}


/*
  Workaround for BUG#25657041

  During inplace alter table, the table has a temporary
  tablename and is also marked as hidden. Since the temporary
  name and hidden status is part of the serialized table
  definition, there's a mismatch down the line when this is
  stored as extra metadata in the NDB dictionary.

  The workaround for now involves setting the table as a user
  visible table and restoring the original table name
*/

void ndb_dd_fix_inplace_alter_table_def(dd::Table* table_def,
                                        const char* proper_table_name)
{
  DBUG_ENTER("ndb_dd_fix_inplace_alter_table_def");
  DBUG_PRINT("enter", ("table_name: %s", table_def->name().c_str()));
  DBUG_PRINT("enter", ("proper_table_name: %s", proper_table_name));

  // Check that the proper_table_name is not a temporary name
  DBUG_ASSERT(!ndb_name_is_temp(proper_table_name));

  table_def->set_name(proper_table_name);
  table_def->set_hidden(dd::Abstract_table::HT_VISIBLE);

  DBUG_VOID_RETURN;
}

bool ndb_dd_remove_table(THD *thd, const char *schema_name,
                         const char *table_name)
{
  DBUG_ENTER("ndb_dd_remove_table");

  Ndb_dd_client dd_client(thd);

  if (!dd_client.remove_table(schema_name, table_name))
  {
    DBUG_RETURN(false);
  }

  dd_client.commit();

  DBUG_RETURN(true); // OK
}


bool
ndb_dd_rename_table(THD *thd,
                    const char *old_schema_name, const char *old_table_name,
                    const char *new_schema_name, const char *new_table_name,
                    int new_table_id, int new_table_version)
{
  DBUG_ENTER("ndb_dd_rename_table");
  DBUG_PRINT("enter", ("old: '%s'.'%s'  new: '%s'.'%s'",
                       old_schema_name, old_table_name,
                       new_schema_name, new_table_name));

  Ndb_dd_client dd_client(thd);

  if (!dd_client.rename_table(old_schema_name, old_table_name,
                              new_schema_name, new_table_name,
                              new_table_id, new_table_version))
  {
    DBUG_RETURN(false);
  }

  dd_client.commit();

  DBUG_RETURN(true); // OK
}


bool
ndb_dd_get_engine_for_table(THD *thd,
                            const char *schema_name,
                            const char *table_name,
                            dd::String_type* engine)
{
  DBUG_ENTER("ndb_dd_get_engine_for_table");

  Ndb_dd_client dd_client(thd);

  if (!dd_client.get_engine(schema_name, table_name, engine))
  {
    DBUG_RETURN(false);
  }

  DBUG_PRINT("info", (("table '%s.%s' exists in DD, engine: '%s'"),
                      schema_name, table_name, engine->c_str()));

  dd_client.commit();

  DBUG_RETURN(true); // OK
}

