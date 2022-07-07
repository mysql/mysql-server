/*
   Copyright (c) 2011, 2022, Oracle and/or its affiliates.

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

#ifndef NDB_DIST_PRIV_UTIL_H
#define NDB_DIST_PRIV_UTIL_H

#include <cstring>  // strcmp

class Ndb_dist_priv_util {
  size_t m_iter_curr_table;

 public:
  Ndb_dist_priv_util() { iter_reset(); }

  const char *database() const { return "mysql"; }

  // Iterator for distributed priv tables name
  const char *iter_next_table() {
    static const char *tables[] = {
        "user",
        "db",
        "tables_priv",
        "columns_priv",
        "procs_priv",
        "proxies_priv"
#ifndef NDEBUG
        ,
        "mtr__acl_test_table"  // For test ndb_ddl.dist_priv_migration
#endif
    };

    if (m_iter_curr_table >= (sizeof(tables) / sizeof(tables[0])))
      return nullptr;
    m_iter_curr_table++;
    return tables[m_iter_curr_table - 1];
  }

  // Reset iterator to start at first table name
  void iter_reset() { m_iter_curr_table = 0; }

  // Determine if a given table is a MySQL 5.7 privilege table
  static bool is_privilege_table(const char *db, const char *table) {
    Ndb_dist_priv_util dist_priv;
    if (strcmp(db, dist_priv.database())) {
      return false;  // Ignore tables not in dist_priv database
    }
    const char *priv_table_name;
    while ((priv_table_name = dist_priv.iter_next_table())) {
      if (strcmp(table, priv_table_name) == 0) {
        return true;
      }
    }
    return false;
  }
};

#endif
