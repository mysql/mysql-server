/*
   Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "my_dbug.h"
#include "storage/ndb/include/ndbapi/NdbDictionary.hpp"

class Ndb_table_guard
{
public:
  Ndb_table_guard(NdbDictionary::Dictionary *dict)
    : m_dict(dict), m_ndbtab(NULL), m_invalidate(0)
  {}
  Ndb_table_guard(NdbDictionary::Dictionary *dict, const char *tabname)
    : m_dict(dict), m_ndbtab(NULL), m_invalidate(0)
  {
    DBUG_ENTER("Ndb_table_guard");
    init(tabname);
    DBUG_VOID_RETURN;
  }
  ~Ndb_table_guard()
  {
    DBUG_ENTER("~Ndb_table_guard");
    reinit();
    DBUG_VOID_RETURN;
  }
  void init(const char *tabname)
  {
    DBUG_ENTER("Ndb_table_guard::init");
    /* must call reinit() if already initialized */
    DBUG_ASSERT(m_ndbtab == NULL);
    m_ndbtab= m_dict->getTableGlobal(tabname);
    m_invalidate= 0;
    DBUG_PRINT("info", ("m_ndbtab: %p", m_ndbtab));
    DBUG_VOID_RETURN;
  }
  void reinit(const char *tabname= 0)
  {
    DBUG_ENTER("Ndb_table_guard::reinit");
    if (m_ndbtab)
    {
      DBUG_PRINT("info", ("m_ndbtab: %p  m_invalidate: %d",
                          m_ndbtab, m_invalidate));
      m_dict->removeTableGlobal(*m_ndbtab, m_invalidate);
      m_ndbtab= NULL;
      m_invalidate= 0;
    }
    if (tabname)
      init(tabname);
    DBUG_PRINT("info", ("m_ndbtab: %p", m_ndbtab));
    DBUG_VOID_RETURN;
  }
  const NdbDictionary::Table *get_table() { return m_ndbtab; }
  void invalidate() { m_invalidate= 1; }
  const NdbDictionary::Table *release()
  {
    DBUG_ENTER("Ndb_table_guard::release");
    const NdbDictionary::Table *tmp= m_ndbtab;
    DBUG_PRINT("info", ("m_ndbtab: %p", m_ndbtab));
    m_ndbtab = 0;
    DBUG_RETURN(tmp);
  }
private:
  NdbDictionary::Dictionary *m_dict;
  const NdbDictionary::Table *m_ndbtab;
  int m_invalidate;
};

/* NDB_TABLE_GUARD_H */
#endif
