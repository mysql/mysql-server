/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef NDBT_TABLE_HPP
#define NDBT_TABLE_HPP

#include <ndb_global.h>
#include <assert.h>

#include <NdbApi.hpp>
#include <NdbOut.hpp>

class NDBT_Attribute : public NdbDictionary::Column {
  friend class NdbOut& operator <<(class NdbOut&, const NDBT_Attribute &);
public:
  NDBT_Attribute(const char* anAttrName,
		 AttrType type,
		 int sz = 4,
		 KeyType key = NoKey,
		 bool nullable = false,
		 StorageAttributeType indexOnly = NormalStorageAttribute,
		 StorageMode _sm = MMBased) :
    NdbDictionary::Column(anAttrName)
  {
    assert(anAttrName != 0);
    
    setNullable(nullable);
    setIndexOnlyStorage(indexOnly == IndexStorageAttribute);
    setPrimaryKey(key != NoKey);
    setTupleKey(key == TupleId);
    setLength(1);
    switch(type){
    case ::Signed:
      if(sz == 8)
	setType(NdbDictionary::Column::Bigint);
      else if (sz == 4)
	setType(NdbDictionary::Column::Int);
      else {
	setType(NdbDictionary::Column::Int);
	setLength(sz);
      }
      break;
      
    case ::UnSigned:
      if(sz == 8)
	setType(NdbDictionary::Column::Bigunsigned);
      else if (sz == 4)
	setType(NdbDictionary::Column::Unsigned);
      else {
	setType(NdbDictionary::Column::Unsigned);
	setLength(sz);
      }
      break;
      
    case ::Float:
      if(sz == 8)
	setType(NdbDictionary::Column::Double);
      else if (sz == 4)
	setType(NdbDictionary::Column::Float);
      else{
	setType(NdbDictionary::Column::Float);
	setLength(sz);
      }
      break;
      
    case ::String:
      setType(NdbDictionary::Column::Char);
      setLength(sz);
      break;
      
    case ::NoAttrTypeDef:
      break;
    }
  }

  NDBT_Attribute(const char* _name,
		 Column::Type _type,
		 int _length = 1,
		 bool _pk = false, 
		 bool _nullable = false):
    NdbDictionary::Column(_name)
  {
    assert(_name != 0);
    
    setNullable(_nullable);
    setPrimaryKey(_pk);
    setLength(_length);
    setType(_type);
  }
};

class NDBT_Table : public NdbDictionary::Table {
  /**
   * Print meta information about table 
   * (information on how it is strored, what the attributes look like etc.)
   */
  friend class NdbOut& operator <<(class NdbOut&, const NDBT_Table &);
public: 
  
  NDBT_Table(const char* name, 
	     int noOfAttributes,
	     const NdbDictionary::Column attributes[],
	     bool stored = true)
    : NdbDictionary::Table(name)
  {
    assert(name != 0);
    
    setStoredTable(stored);
    for(int i = 0; i<noOfAttributes; i++)
      addColumn(attributes[i]);
  }

  static const NdbDictionary::Table * discoverTableFromDb(Ndb* ndb,
							  const char * name);
};

inline
const NdbDictionary::Table * 
NDBT_Table::discoverTableFromDb(Ndb* ndb, const char * name){
  return ndb->getDictionary()->getTable(name);
}

#endif
