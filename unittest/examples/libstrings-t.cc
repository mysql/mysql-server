/* Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include <iostream>

// Visual studio gave Command-Line Warning D9025 for
// -UMYSQL_PROJECT on the command line.
#if defined(UNDEFINE_MYSQL_PROJECT)
#undef MYSQL_PROJECT
#endif

#include "mysql/strings/collations.h"
#include "mysql/strings/m_ctype.h"

/**
  @file Simple executable linked with libstrings.a or libstrings_shared.so
  to show that it works.
  */

int main() {
  mysql::collation::initialize();
  const CHARSET_INFO *cs = mysql::collation::find_primary("utf8mb3");
  std::cout << "Ok: " << cs->csname << std::endl;
  const CHARSET_INFO *alias_cs = mysql::collation::find_primary("utf8");
  std::cout << "Ok: " << alias_cs->csname << std::endl;
  const CHARSET_INFO *zh_cs =
      mysql::collation::find_by_name("utf8mb4_zh_0900_as_cs");
  std::cout << "Ok: " << zh_cs->csname << " " << zh_cs->m_coll_name
            << std::endl;
  const CHARSET_INFO *cs_id = mysql::collation::find_by_id(cs->number);
  std::cout << "OK: " << cs_id->csname << " " << cs_id->number << std::endl;
  mysql::collation::shutdown();
  return 0;
}
