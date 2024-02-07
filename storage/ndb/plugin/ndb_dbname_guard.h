/*
   Copyright (c) 2020, 2024, Oracle and/or its affiliates.

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

#ifndef NDB_DBNAME_GUARD_H
#define NDB_DBNAME_GUARD_H

#include <string>

#include "my_dbug.h"
#include "storage/ndb/include/ndbapi/Ndb.hpp"

/**
   @brief Class used for setting the database of the Ndb object. The database
   name will be restored when class go out of scope.

   The database name will only be changed when different, this means that
   this class may be used in several different scopes where the database is
   still the same (which is the most common) without causing unnecessary
   switching.

 */
class Ndb_dbname_guard {
  Ndb *const m_ndb;
  const std::string m_save_dbname;
  bool m_changed_dbname{false};
  bool m_change_database_failed{false};

  Ndb_dbname_guard() = delete;
  Ndb_dbname_guard(const Ndb_dbname_guard &) = delete;

 public:
  Ndb_dbname_guard(Ndb *ndb, const std::string &dbname)
      : m_ndb(ndb), m_save_dbname(ndb->getDatabaseName()) {
    // Only change database name if it's different
    if (dbname != m_ndb->getDatabaseName()) {
      DBUG_PRINT("info", ("Changing database name: '%s' -> '%s'",
                          m_save_dbname.c_str(), dbname.c_str()));
      if (m_ndb->setDatabaseName(dbname.c_str()) != 0) {
        m_change_database_failed = true;
        return;
      }
      m_changed_dbname = true;
    }
  }

  /**
     @brief Determine if change of database failed.

     @note The class changes database in the constructor. Failure to change the
     database of a Ndb object is very rare, but to be safe functions that depend
     on correct database and have ability to return failure to the user should
     use this function.

     @return true if change failed
   */
  bool change_database_failed() const { return m_change_database_failed; }

  ~Ndb_dbname_guard() {
    DBUG_TRACE;

    if (!m_changed_dbname) {
      // The database name was not changed, don't change back
      return;
    }

    DBUG_PRINT("info", ("Restoring database name: '%s' -> '%s'",
                        m_ndb->getDatabaseName(), m_save_dbname.c_str()));

    // Change back to saved database name
    if (m_ndb->setDatabaseName(m_save_dbname.c_str())) {
      // Failed to restore old database name
      assert(false);
    }
  }

#ifndef NDEBUG
  /*
     @brief Check that database name has been set on the Ndb object.

     @note This function is used in functions whose implementation
     require/assume that database name of the Ndb object has been set. The
     function is only used in debug compile.

     @param ndb         The Ndb object to check
     @param dbname      Name of database that should be set

     @return true when database of Ndb object matches "dbname"

  */
  static bool check_dbname(Ndb *ndb, const std::string &dbname) {
    // Check that:
    // - dbname parameter is not empty string
    // - database name of the Ndb object is identical to the dbname parameter
    if (dbname.empty() || dbname != ndb->getDatabaseName()) {
      assert(false);  // Internal error, crash here in debug
      return false;
    }
    return true;
  }
#endif
};

#endif
