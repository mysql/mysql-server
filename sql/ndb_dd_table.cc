/*
   Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

// Implements the functions defined in ndb_dd_table.h
#include "ndb_dd_table.h"

#include "dd/properties.h"
#include "dd/types/table.h"


#ifndef FIXED_BUG26723442
// The key used to _temporarily_ store the NDB tables object version in the
// se_private_data field of DD
static const char* object_id_key = "object_id";
#endif

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

#ifndef FIXED_BUG26723442
  // Temporarily save the object id in se_private_data
  table_def->se_private_data().set_int32(object_id_key,
                                         object_id);
#else
  table_def->set_se_private_id(object_id);
#endif
  table_def->se_private_data().set_int32(object_version_key,
                                         object_version);
  DBUG_VOID_RETURN;
}


bool
ndb_dd_table_get_object_id_and_version(const dd::Table* table_def,
                                       int& object_id, int& object_version)
{
  DBUG_ENTER("ndb_dd_table_get_object_id_and_version");

#ifndef FIXED_BUG26723442
  // Temporarily read the object id from se_private_data
  if (!table_def->se_private_data().exists(object_id_key))
  {
    DBUG_PRINT("error", ("Table definition didn't contain "
                         "property '%s'", object_id_key));
    DBUG_RETURN(false);
  }

  if (table_def->se_private_data().get_int32(object_id_key,
                                             &object_id))
  {
    DBUG_PRINT("error", ("Table definition didn't have a valid number "
                         "for '%s'", object_id_key));
    DBUG_RETURN(false);
  }

#else
  if (table_def->se_private_id() == dd::INVALID_OBJECT_ID)
  {
    DBUG_PRINT("error", ("Table definition contained an invalid object id"));
    DBUG_RETURN(false);
  }
  object_id = table_def->se_private_id();
#endif

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
