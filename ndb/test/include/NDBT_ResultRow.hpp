/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

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
