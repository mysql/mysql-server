/*
   Copyright (c) 2019, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

// Implements the functions declared in ndb_dd_schema.h
#include "storage/ndb/plugin/ndb_dd_schema.h"

#include "my_dbug.h"
#include "sql/dd/properties.h"
#include "sql/dd/types/schema.h"

// Schema se_private_data keys. The keys are prefixed with 'ndb' as
// other SEs can also write into se_private_data of Schema object.
static const char *ndb_counter_key = "ndb_counter";
static const char *ndb_node_id_key = "ndb_node_id";

bool ndb_dd_schema_get_counter_and_nodeid(const dd::Schema *schema,
                                          unsigned int &counter,
                                          unsigned int &node_id) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("Reading se_private_data of schema '%s'",
                       schema->name().c_str()));

  // Fetch counter and node_id values if they exist
  if (schema->se_private_data().exists(ndb_counter_key) &&
      schema->se_private_data().get(ndb_counter_key, &counter)) {
    DBUG_PRINT("error", ("Schema definition has an invalid value for '%s'",
                         ndb_counter_key));
    assert(false);
    return false;
  }

  if (schema->se_private_data().exists(ndb_node_id_key) &&
      schema->se_private_data().get(ndb_node_id_key, &node_id)) {
    DBUG_PRINT("error", ("Schema definition has an invalid value for '%s'",
                         ndb_node_id_key));
    assert(false);
    return false;
  }

  DBUG_PRINT("exit", ("counter: %u, node id: %u", counter, node_id));
  return true;
}

void ndb_dd_schema_set_counter_and_nodeid(dd::Schema *schema,
                                          unsigned int counter,
                                          unsigned int node_id) {
  DBUG_TRACE;
  DBUG_PRINT("enter", ("Schema : %s, counter : %u, node_id : %u",
                       schema->name().c_str(), counter, node_id));

  schema->se_private_data().set(ndb_counter_key, counter);
  schema->se_private_data().set(ndb_node_id_key, node_id);
}
