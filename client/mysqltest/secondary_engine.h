#ifndef SECONDARY_ENGINE_INCLUDED
#define SECONDARY_ENGINE_INCLUDED
// Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 2.0,
// as published by the Free Software Foundation.
//
// This program is also distributed with certain software (including
// but not limited to OpenSSL) that is licensed under separate terms,
// as designated in a particular file or component or in included license
// documentation.  The authors of MySQL hereby grant you an additional
// permission to link the program and your derivative works with the
// separately licensed software that they have included with MySQL.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License, version 2.0, for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.

#include <vector>

#include "mysql.h"

/// Check if the statement is a CREATE TABLE statement or a DDL
/// statement. If yes, run the ALTER TABLE statements needed to change
/// the secondary engine and to load the data from primary engine to
/// secondary engine.
///
/// @param secondary_engine       Secondary engine name
/// @param statement              Original statement
/// @param mysql                  mysql handle
/// @param expected_errors        List of expected errors
/// @param opt_change_propagation Boolean flag indicating whether change
///                               propagation is enabled or not.
///
/// @retval True if load operation fails, false otherwise.
bool run_secondary_engine_load_statements(
    const char *secondary_engine, char *statement, MYSQL *mysql,
    std::vector<unsigned int> expected_errors, bool opt_change_propagation);

/// Check if the statement is a DDL statement. If yes, run the ALTER
/// TABLE statements needed to change the secondary engine to NULL and
/// to unload the data from secondary engine.
///
/// @param statement       Original statement
/// @param mysql           mysql handle
/// @param expected_errors List of expected errors
///
/// @retval True if unload operation fails, false otherwise.
bool run_secondary_engine_unload_statements(
    char *statement, MYSQL *mysql, std::vector<unsigned int> expected_errors);

#endif  // SECONDARY_ENGINE_INCLUDED
