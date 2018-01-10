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

// Implements the functions defined in ndb_dd_table.h
#include "sql/ndb_dd_table.h"

#include "sql/dd/properties.h"
#include "sql/dd/types/column.h"
#include "sql/dd/types/table.h"

// The key used to store the NDB tables object version in the
// se_private_data field of DD
static const char* object_version_key = "object_version";

void
ndb_dd_table_set_object_id_and_version(dd::Table* table_def,
                                       int object_id, int object_version)
{
  DBUG_ENTER("ndb_dd_table_set_object_id_and_version");
  DBUG_PRINT("enter", ("object_id: %d, object_version: %d",
                       object_id, object_version));

  table_def->set_se_private_id(object_id);
  table_def->se_private_data().set_int32(object_version_key,
                                         object_version);
  DBUG_VOID_RETURN;
}


bool
ndb_dd_table_get_object_id_and_version(const dd::Table* table_def,
                                       int& object_id, int& object_version)
{
  DBUG_ENTER("ndb_dd_table_get_object_id_and_version");

  if (table_def->se_private_id() == dd::INVALID_OBJECT_ID)
  {
    DBUG_PRINT("error", ("Table definition contained an invalid object id"));
    DBUG_RETURN(false);
  }
  object_id = table_def->se_private_id();

  if (!table_def->se_private_data().exists(object_version_key))
  {
    DBUG_PRINT("error", ("Table definition didn't contain property '%s'",
                         object_version_key));
    DBUG_RETURN(false);
  }

  if (table_def->se_private_data().get_int32(object_version_key,
                                             &object_version))
  {
    DBUG_PRINT("error", ("Table definition didn't have a valid number for '%s'",
                         object_version_key));
    DBUG_RETURN(false);
  }

  DBUG_PRINT("exit", ("object_id: %d, object_version: %d",
                      object_id, object_version));

  DBUG_RETURN(true);
}


void
ndb_dd_table_mark_as_hidden(dd::Table* table_def)
{
  DBUG_ENTER("ndb_dd_table_mark_as_hidden");
  DBUG_PRINT("enter", ("table_name: %s", table_def->name().c_str()));

  // Only allow mysql.ndb_schema table to be hidden for now, there are a
  // few hacks elsewehere in these ndb_dd_* files and those need to be
  // hacked to keep the table hidden
  DBUG_ASSERT(table_def->name() == "ndb_schema");

  // Mark it as hidden by SE. I.e "Table which is implicitly
  // created and dropped by SE"
  table_def->set_hidden(dd::Abstract_table::HT_HIDDEN_SE);

  DBUG_VOID_RETURN;
}


dd::String_type ndb_dd_table_get_engine(const dd::Table* table_def)
{
  return table_def->engine();
}

size_t ndb_dd_table_get_num_columns(const dd::Table* table_def)
{
  const dd::Abstract_table::Column_collection& cols = table_def->columns();
  return cols.size();
}

bool ndb_dd_table_is_using_fixed_row_format(const dd::Table* table_def)
{
  return table_def->row_format() == dd::Table::RF_FIXED;
}
