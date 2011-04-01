/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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

#ifndef NDBT_CALC_HPP
#define NDBT_CALC_HPP

#include <NDBT_ResultRow.hpp>

/* *************************************************************
 * HugoCalculator
 *
 *  Comon class for the Hugo test suite, provides the functions 
 *  that is used for calculating values to load in to table and 
 *  also knows how to verify a row that's been read from db 
 *
 * ************************************************************/
class HugoCalculator {
public:
  HugoCalculator(const NdbDictionary::Table& tab);
  Int32 calcValue(int record, int attrib, int updates) const;
  const char* calcValue(int record, int attrib, int updates, char* buf, 
			int len, Uint32* real_len) const;

  int verifyRowValues(NDBT_ResultRow* const  pRow) const;
  int verifyRecAttr(int record, int updates, const NdbRecAttr* recAttr);
  int verifyColValue(int record, int attrib, int updates, 
                     const char* valPtr, Uint32 valLen);
  int getIdValue(NDBT_ResultRow* const pRow) const;
  int getUpdatesValue(NDBT_ResultRow* const pRow) const;
  int isIdCol(int colId) { return m_idCol == colId; };
  int isUpdateCol(int colId){ return m_updatesCol == colId; };

  const NdbDictionary::Table& getTable() const { return m_tab;}

  int equalForRow(Uint8 *, const NdbRecord*, int rowid);
  int setValues(Uint8*, const NdbRecord*, int rowid, int updateval);
private:
  const NdbDictionary::Table& m_tab;
  int m_idCol;
  int m_updatesCol;
};


#endif




