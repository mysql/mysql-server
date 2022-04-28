/*
   Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

#ifndef NDB_DD_SCHEMA_H
#define NDB_DD_SCHEMA_H

namespace dd {
class Schema;
}

/**
  @brief Read the counter and node id from the se_private_data field
         of the Schema object

  @param schema           Pointer to the schema object from which the
                          values are to be read
  @param[out] counter     Output parameter to read the counter value into.
  @param[out] node_id     Output parameter to read the node_id value into.

  @return Returns true if the values are read successfully.
                  false if the read failed.
 */
bool ndb_dd_schema_get_counter_and_nodeid(const dd::Schema *schema,
                                          unsigned int &counter,
                                          unsigned int &node_id);

/**
  @brief Set the counter and node id values to the se_private_data
         field of the Schema object

  @param schema           Pointer to the schema object to which the
                          values are to be set
  @param counter          The counter value to be set to the Schema.
  @param node_id          The node_id value to be set to the Schema.
 */
void ndb_dd_schema_set_counter_and_nodeid(dd::Schema *schema,
                                          unsigned int counter,
                                          unsigned int node_id);

#endif
