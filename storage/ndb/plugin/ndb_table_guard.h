/*
   Copyright (c) 2011, 2019, Oracle and/or its affiliates. All rights reserved.

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

class Ndb_table_guard {
  NdbDictionary::Dictionary *const m_dict;
  const NdbDictionary::Table *m_ndbtab{nullptr};
  int m_invalidate{0};

  void deinit() {
    DBUG_PRINT("info",
               ("m_ndbtab: %p  m_invalidate: %d", m_ndbtab, m_invalidate));
    m_dict->removeTableGlobal(*m_ndbtab, m_invalidate);
    m_ndbtab = nullptr;
    m_invalidate = 0;
  }

 public:
  Ndb_table_guard(NdbDictionary::Dictionary *dict) : m_dict(dict) {}
  Ndb_table_guard(NdbDictionary::Dictionary *dict, const char *tabname)
      : m_dict(dict) {
    DBUG_TRACE;
    init(tabname);
    return;
  }
  Ndb_table_guard(Ndb *ndb, const char *dbname, const char *tabname)
      : m_dict(ndb->getDictionary()) {
    const std::string save_dbname(ndb->getDatabaseName());
    if (ndb->setDatabaseName(dbname) != 0) {
      // Failed to set databasname, indicate error by returning
      // without initializing the table pointer
      return;
    }
    init(tabname);
    (void)ndb->setDatabaseName(save_dbname.c_str());
  }
  ~Ndb_table_guard() {
    DBUG_TRACE;
    if (m_ndbtab) {
      deinit();
    }
    return;
  }
  void init(const char *tabname) {
    DBUG_TRACE;
    /* Don't allow init() if already initialized */
    DBUG_ASSERT(m_ndbtab == nullptr);
    m_ndbtab = m_dict->getTableGlobal(tabname);
    m_invalidate = 0;
    DBUG_PRINT("info", ("m_ndbtab: %p", m_ndbtab));
    return;
  }
  void reinit() {
    DBUG_TRACE;
    /* Don't allow reinit() if not initialized already */
    DBUG_ASSERT(m_ndbtab != nullptr);
    std::string table_name(m_ndbtab->getName());
    deinit();
    init(table_name.c_str());
  }
  const NdbDictionary::Table *get_table() const { return m_ndbtab; }
  void invalidate() { m_invalidate = 1; }
  const NdbDictionary::Table *release() {
    DBUG_TRACE;
    const NdbDictionary::Table *tmp = m_ndbtab;
    DBUG_PRINT("info", ("m_ndbtab: %p", m_ndbtab));
    m_ndbtab = 0;
    return tmp;
  }
};

#endif
