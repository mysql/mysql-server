/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "mysqlrouter/server_compatibility.h"

#include <iostream>

#include "mysqlrouter/mysql_session.h"

void check_version_compatibility(mysqlrouter::MySQLSession *session) {
  auto row = session->query_one(
      "SELECT substring_index(@@version, '.', 1), concat(@@version_comment, "
      "@@version)");
  bool ok = true;
  if (std::atoi((*row)[0]) < 8 || (strncmp((*row)[1], "MySQL", 5) != 0 &&
                                   strstr((*row)[1], "-labs-mrs") == nullptr)) {
    std::cout << "Unsupported MySQL server version: " << (*row)[1] << "\n";
    ok = false;
  }

  row = session->query_one("SELECT @@basedir");
  if (strstr((*row)[0], "rds")) {
    ok = false;
  }
  try {
    session->query_one("SELECT aurora_version()");
    ok = false;
  } catch (...) {
  }
  if (!ok) throw std::runtime_error("Target DB System is not fully supported");
}
