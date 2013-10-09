/*
   Copyright (c) 2011, 2012, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDB_DIST_PRIV_UTIL_H
#define NDB_DIST_PRIV_UTIL_H

class Ndb_dist_priv_util {
  size_t m_iter_curr_table;
public:
  Ndb_dist_priv_util()
  {
    iter_reset();
  }

  const char* database() const { return "mysql"; }

  // Iterator for distributed priv tables name
  const char* iter_next_table()
  {
    static const char* tables[] =
      { "user", "db", "tables_priv", "columns_priv", "procs_priv", "host",
        "proxies_priv" };

    if (m_iter_curr_table >= (sizeof(tables) / sizeof(tables[0])))
      return NULL;
    m_iter_curr_table++;
    return tables[m_iter_curr_table-1];
  }

  // Reset iterator to start at first table name
  void iter_reset() { m_iter_curr_table = 0; }

  // Determine if a given table name is in the list
  // of distributed priv tables
  static
  bool
  is_distributed_priv_table(const char *db, const char *table)
  {
    Ndb_dist_priv_util dist_priv;
    if (strcmp(db, dist_priv.database()))
    {
      return false; // Ignore tables not in dist_priv database
    }
    const char* priv_table_name;
    while((priv_table_name= dist_priv.iter_next_table()))
    {
      if (strcmp(table, priv_table_name) == 0)
      {
        return true;
      }
    }
    return false;
  }

};

#endif
