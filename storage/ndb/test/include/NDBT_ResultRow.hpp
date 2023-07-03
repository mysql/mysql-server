/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

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

#ifndef NDBT_RESULTROW_HPP
#define NDBT_RESULTROW_HPP

#include <NdbApi.hpp>

class NDBT_ResultRow {
public:
  NDBT_ResultRow(const NdbDictionary::Table &tab, char attrib_delimiter='\t');
  ~NDBT_ResultRow();
  NdbRecAttr * & attributeStore(int i);
  const NdbRecAttr * attributeStore(int i) const ;
  const NdbRecAttr * attributeStore(const char* name) const ;
  
  BaseString c_str() const ;

  NdbOut & header (NdbOut &) const;
  friend NdbOut & operator << (NdbOut&, const NDBT_ResultRow &);
  
  /**
   * Make copy of NDBT_ResultRow 
   */
  NDBT_ResultRow * clone() const;

  bool operator==(const NDBT_ResultRow&) const ;
  bool operator!=(const NDBT_ResultRow& other) const { 
    return ! (*this == other);
  }

  const NdbDictionary::Table& getTable() const { return m_table;}
  
private:
  int cols;
  char **names;
  NdbRecAttr **data;
  char ad[2];

  bool m_ownData;
  const NdbDictionary::Table & m_table;
  
  NDBT_ResultRow(const NDBT_ResultRow &);
  NDBT_ResultRow& operator=(const NDBT_ResultRow &);
};




#endif
