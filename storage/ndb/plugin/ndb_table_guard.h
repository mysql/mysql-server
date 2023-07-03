/*
   Copyright (c) 2011, 2023, Oracle and/or its affiliates.

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

#ifndef NDB_TABLE_GUARD_H
#define NDB_TABLE_GUARD_H

#include <string>

#include "my_dbug.h"
#include "storage/ndb/include/ndbapi/Ndb.hpp"
#include "storage/ndb/include/ndbapi/NdbDictionary.hpp"
#include "storage/ndb/plugin/ndb_dbname_guard.h"

/*
   @brief This class maintains the reference to a NDB table definition retrieved
   from NdbApi's global dictionary cache. To avoid stale table definitions in
   the global cache the reference is released when the class goes out of scope
   unless the user takes ownership of the reference by calling release().

   The class also encapculates the function calls which need to be done to
   properly indicate which database the table to load should be fetched from.
   The database name is not passed as a parameter to getTableGlobal() but rather
   passed using a global database name parameter of the Ndb object, which is
   then used when the NdbApi formats the full name of the table.

   @note When the table definition is retrieved it may be loaded from NDB, this
   causes one roundtrip and network related errors may occur. The use cases
   where this need to be distinguished from the case where table does not
   exist need to examine the NDB error saved by the class.

*/
class Ndb_table_guard {
  Ndb *const m_ndb;
  const NdbDictionary::Table *m_ndbtab{nullptr};
  int m_invalidate{0};
  NdbError m_ndberror;

  void deinit() {
    DBUG_TRACE;
    DBUG_PRINT("info", ("m_ndbtab: %p", m_ndbtab));
    DBUG_PRINT("info", ("m_invalidate: %d", m_invalidate));
    // Ignore return code since function can't fail
    (void)m_ndb->getDictionary()->removeTableGlobal(*m_ndbtab, m_invalidate);
    m_ndbtab = nullptr;
    m_invalidate = 0;
  }

 public:
  Ndb_table_guard(Ndb *ndb) : m_ndb(ndb) {}
  Ndb_table_guard(Ndb *ndb, const char *dbname, const char *tabname)
      : m_ndb(ndb) {
    init(dbname, tabname);
  }

  ~Ndb_table_guard() {
    DBUG_TRACE;
    if (m_ndbtab) {
      deinit();
    }
    return;
  }

  void init(const char *dbname, const char *tabname) {
    DBUG_TRACE;
    /* Don't allow init() if already initialized */
    assert(m_ndbtab == nullptr);

    // Change to the database where the table should be found
    Ndb_dbname_guard dbname_guard(m_ndb, dbname);
    if (dbname_guard.change_database_failed()) {
      // Failed to change database, indicate error by returning
      // without initializing the table pointer. Save NDB error to
      // allow caller to handle different errors.
      m_ndberror = m_ndb->getNdbError();
      DBUG_PRINT("error", ("change database, code: %d, message: %s",
                           m_ndberror.code, m_ndberror.message));
      return;
    }

    NdbDictionary::Dictionary *dict = m_ndb->getDictionary();
    m_ndbtab = dict->getTableGlobal(tabname);
    if (!m_ndbtab) {
      // Failed to retrieve table definition, error will be indicated by
      // uninitialized table pointer. Save NDB error to allow caller to handle
      // different errors.
      m_ndberror = dict->getNdbError();
      DBUG_PRINT("error", ("getTableGlobal, code: %d, message: %s",
                           m_ndberror.code, m_ndberror.message));
      return;
    }

    DBUG_PRINT("info", ("ndbtab: %p", m_ndbtab));
    assert(m_invalidate == 0);
    return;
  }

  void reinit(const char *dbname, const char *table_name) {
    DBUG_TRACE;
    /* Don't allow reinit() if not initialized already */
    assert(m_ndbtab != nullptr);
    // Table name argument of reinit must match already loaded table
    assert(strcmp(m_ndbtab->getName(), table_name) == 0);
    deinit();
    init(dbname, table_name);
  }

  /**
     @brief Return pointer to table definition. Nullptr will be returned
     both when table does not exist and when an error has occurred. If there is
     a need to distinguish between the two cases the user need to examine the
     NDB error.

     @return pointer to the NdbApi table definition or nullptr when table
     doesn't exist or error occurs
   */
  const NdbDictionary::Table *get_table() const { return m_ndbtab; }

  // Invalidate table definition in NdbApi cache when class goes out of scope
  void invalidate() { m_invalidate = 1; }

  // Release ownership of the loaded NdbApi table definition reference
  const NdbDictionary::Table *release() {
    DBUG_TRACE;
    const NdbDictionary::Table *tmp = m_ndbtab;
    DBUG_PRINT("info", ("m_ndbtab: %p", m_ndbtab));
    m_ndbtab = nullptr;
    return tmp;
  }

  const NdbError &getNdbError() const { return m_ndberror; }
};

#endif
