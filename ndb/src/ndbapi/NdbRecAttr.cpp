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


/************************************************************************************************
Name:          NdbRecAttr.C
Include:
Link:
Author:        UABRONM Mikael Ronström UAB/B/SD                         
Date:          971206
Version:       0.1
Description:   Interface between TIS and NDB
Documentation:
Adjust:  971206  UABRONM First version
************************************************************************************************/
#include <ndb_global.h>
#include <NdbOut.hpp>
#include <NdbRecAttr.hpp>
#include "NdbDictionaryImpl.hpp"

NdbRecAttr::NdbRecAttr() :
  theStorageX(NULL),
  theValue(NULL),
  theRef(NULL),
  theNext(NULL),
  theAttrId(0xFFFF),
  theNULLind(-1)
{ 
}

NdbRecAttr::~NdbRecAttr()
{
  release();
}

int
NdbRecAttr::setup(const NdbColumnImpl* anAttrInfo, char* aValue)
{
  Uint32 tAttrSize = anAttrInfo->m_attrSize;
  Uint32 tArraySize = anAttrInfo->m_arraySize;
  Uint32 tAttrByteSize = tAttrSize * tArraySize;
  
  m_column = anAttrInfo;
  theAttrId = anAttrInfo->m_attrId;
  theAttrSize = tAttrSize;
  theArraySize = tArraySize;
  theValue = aValue;

  // check alignment to signal data
  // a future version could check alignment per data type as well
  
  if (aValue != NULL && (UintPtr(aValue)&3) == 0 && (tAttrByteSize&3) == 0) {
    theStorageX = NULL;
    theRef = aValue;
    return 0;
  }
  if (tAttrByteSize <= 32) {
    theStorageX = NULL;
    theStorage[0] = 0;
    theStorage[1] = 0;
    theStorage[2] = 0;
    theStorage[3] = 0;
    theRef = theStorage;
    return 0;
  }
  Uint32 tSize = (tAttrByteSize + 7) >> 3;
  Uint64* tRef = new Uint64[tSize];
  if (tRef != NULL) {
    for (Uint32 i = 0; i < tSize; i++) {
      tRef[i] = 0;
    }
    theStorageX = tRef;
    theRef = tRef;
    return 0;
  }
  return -1;
}

void
NdbRecAttr::copyout()
{
  char* tRef = (char*)theRef;
  char* tValue = theValue;
  if (tRef != tValue && tRef != NULL && tValue != NULL) {
    Uint32 n = theAttrSize * theArraySize;
    while (n-- > 0) {
      *tValue++ = *tRef++;
    }
  }
}

NdbRecAttr *
NdbRecAttr::clone() const {
  NdbRecAttr * ret = new NdbRecAttr();

  ret->theAttrId = theAttrId;
  ret->theNULLind = theNULLind;
  ret->theAttrSize = theAttrSize;
  ret->theArraySize = theArraySize;
  ret->m_column = m_column;
  
  Uint32 n = theAttrSize * theArraySize;  
  if(n <= 32){
    ret->theRef = (char*)&ret->theStorage[0];
    ret->theStorageX = 0;
    ret->theValue = 0;
  } else {
    ret->theStorageX = new Uint64[((n + 7) >> 3)];
    ret->theRef = (char*)ret->theStorageX;    
    ret->theValue = 0;
  }
  memcpy(ret->theRef, theRef, n);
  return ret;
}

NdbOut& operator <<(NdbOut& ndbout, const NdbRecAttr &r)
{
  if (r.isNULL())
  {
    ndbout << "[NULL]";
    return ndbout;
  }

  switch(r.getType()){

  case NdbDictionary::Column::Bigunsigned:
    ndbout << r.u_64_value();
    break;
  case NdbDictionary::Column::Unsigned:
    ndbout << r.u_32_value();
    break;
  case NdbDictionary::Column::Smallunsigned:
    ndbout << r.u_short_value();
    break;
  case NdbDictionary::Column::Tinyunsigned:
    ndbout << (unsigned) r.u_char_value();
    break;
  case NdbDictionary::Column::Bigint:
    ndbout << r.int64_value();
    break;
  case NdbDictionary::Column::Int:
    ndbout << r.int32_value();
    break;
  case NdbDictionary::Column::Smallint:
    ndbout << r.short_value();
    break;
  case NdbDictionary::Column::Tinyint:
    ndbout << (int) r.char_value();
    break;
    
  case NdbDictionary::Column::Char:
  case NdbDictionary::Column::Varchar:
    {
      int aSize = r.arraySize();
      char* buf = new char[aSize+1];
      memcpy(buf, r.aRef(), aSize);
      buf[aSize] = 0;
      ndbout << buf;
      delete [] buf;
    }
    break;
    
  case NdbDictionary::Column::Float:
    ndbout << r.float_value();
    break;
  case NdbDictionary::Column::Double:
    ndbout << r.double_value();
    break;
  case NdbDictionary::Column::Mediumint:
    ndbout << "[Mediumint]";
    break;
  case NdbDictionary::Column::Mediumunsigned:
    ndbout << "[Mediumunsigend]";
    break;
  case NdbDictionary::Column::Binary:
    ndbout << "[Binary]";
    break;
  case NdbDictionary::Column::Varbinary:
    ndbout << "[Varbinary]";
    break;
  case NdbDictionary::Column::Decimal:
    ndbout << "[Decimal]";
    break;
  case NdbDictionary::Column::Timespec:
    ndbout << "[Timespec]";
    break;
  case NdbDictionary::Column::Blob:
    ndbout << "[Blob]";
    break;
  case NdbDictionary::Column::Undefined:
    ndbout << "[Undefined]";
    break;
  default:
    ndbout << "[unknown]";
    break;
  }

  return ndbout;
}
