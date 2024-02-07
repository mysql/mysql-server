/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

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

#ifndef NDBT_CALC_HPP
#define NDBT_CALC_HPP

#include <NDBT_ResultRow.hpp>

/* *************************************************************
 * HugoCalculator
 *
 *  Common class for the Hugo test suite, provides the functions
 *  that is used for calculating values to load in to table and
 *  also knows how to verify a row that's been read from db
 *
 * ************************************************************/
class HugoCalculator {
 public:
  HugoCalculator(const NdbDictionary::Table &tab);
  Int32 calcValue(int record, int attrib, int updates) const;
  const char *calcValue(int record, int attrib, int updates, char *buf, int len,
                        Uint32 *real_len) const;

  int verifyRowValues(NDBT_ResultRow *const pRow) const;
  int verifyRecAttr(int record, int updates, const NdbRecAttr *recAttr);
  int verifyColValue(int record, int attrib, int updates, const char *valPtr,
                     Uint32 valLen);
  int getIdValue(NDBT_ResultRow *const pRow) const;
  int getUpdatesValue(NDBT_ResultRow *const pRow) const;
  int getIdColNo() const { return m_idCol; }
  bool isIdCol(int colId) { return m_idCol == colId; }
  bool isUpdateCol(int colId) { return m_updatesCol == colId; }

  const NdbDictionary::Table &getTable() const { return m_tab; }

  int equalForRow(Uint8 *, const NdbRecord *, int rowid);
  int setValues(Uint8 *, const NdbRecord *, int rowid, int updateval);

  class KeyParts {
   public:
    Ndb::Key_part_ptr ptrs[NDB_MAX_NO_OF_ATTRIBUTES_IN_KEY + 1];
    char buffer[NDB_MAX_TUPLE_SIZE];

    KeyParts() { ptrs[0].ptr = 0; }
  };

  /* Init buffer and ptrs in KeyParts object for row */
  int setKeyParts(KeyParts *keyParts, int rowid);

 private:
  const NdbDictionary::Table &m_tab;
  int m_idCol;
  int m_updatesCol;
};

#endif
