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


#include <ndb_global.h>
#include <NdbOut.hpp>
#include <NdbRecAttr.hpp>
#include <NdbBlob.hpp>
#include "NdbDictionaryImpl.hpp"
#include <NdbTCP.h>

NdbRecAttr::NdbRecAttr(Ndb*)
{
  theStorageX = 0;
  init();
}

NdbRecAttr::~NdbRecAttr()
{
  release();
}

int
NdbRecAttr::setup(const class NdbDictionary::Column* col, char* aValue)
{
  return setup(&(col->m_impl), aValue);
}

int
NdbRecAttr::setup(const NdbColumnImpl* anAttrInfo, char* aValue)
{
  Uint32 tAttrSize = anAttrInfo->m_attrSize;
  Uint32 tArraySize = anAttrInfo->m_arraySize;
  Uint32 tAttrByteSize = tAttrSize * tArraySize;
  
  m_column = anAttrInfo;

  theAttrId = anAttrInfo->m_attrId;
  m_size_in_bytes = tAttrByteSize;

  return setup(tAttrByteSize, aValue);
}

int 
NdbRecAttr::setup(Uint32 byteSize, char* aValue)
{
  theValue = aValue;
  m_getVarValue = NULL; // set in getVarValue() only

  if (theStorageX)
    delete[] theStorageX;
  theStorageX = NULL; // "safety first"
  
  // check alignment to signal data
  // a future version could check alignment per data type as well
  
  if (aValue != NULL && (UintPtr(aValue)&3) == 0 && (byteSize&3) == 0) {
    theRef = aValue;
    return 0;
  }

  if (byteSize <= 32) {
    theStorage[0] = 0;
    theStorage[1] = 0;
    theStorage[2] = 0;
    theStorage[3] = 0;
    theRef = theStorage;
    return 0;
  }
  Uint32 tSize = (byteSize + 7) >> 3;
  Uint64* tRef = new Uint64[tSize];
  if (tRef != NULL) {
    for (Uint32 i = 0; i < tSize; i++) {
      tRef[i] = 0;
    }
    theStorageX = tRef;
    theRef = tRef;
    return 0;
  }
  errno= ENOMEM;
  return -1;
}


void
NdbRecAttr::copyout()
{
  char* tRef = (char*)theRef;
  char* tValue = theValue;
  if (tRef != tValue && tRef != NULL && tValue != NULL) {
    Uint32 n = m_size_in_bytes;
    while (n-- > 0) {
      *tValue++ = *tRef++;
    }
  }
}

NdbRecAttr *
NdbRecAttr::clone() const {
  NdbRecAttr * ret = new NdbRecAttr(0);
  if (ret == NULL)
  {
    errno = ENOMEM;
    return NULL;
  }
  ret->theAttrId = theAttrId;
  ret->m_size_in_bytes = m_size_in_bytes;
  ret->m_column = m_column;
  
  Uint32 n = m_size_in_bytes;
  if(n <= 32){
    ret->theRef = (char*)&ret->theStorage[0];
    ret->theStorageX = 0;
    ret->theValue = 0;
  } else {
    ret->theStorageX = new Uint64[((n + 7) >> 3)];
    if (ret->theStorageX == NULL)
    {
      delete ret;
      errno = ENOMEM;
      return NULL;
    }
    ret->theRef = (char*)ret->theStorageX;    
    ret->theValue = 0;
  }
  memcpy(ret->theRef, theRef, n);
  return ret;
}

bool
NdbRecAttr::receive_data(const Uint32 * data32, Uint32 sz)
{
  const unsigned char* data = (const unsigned char*)data32;
  if(sz)
  {
    if (unlikely(m_getVarValue != NULL)) {
      // ONLY for blob V2 implementation
      assert(m_column->getType() == NdbDictionary::Column::Longvarchar ||
             m_column->getType() == NdbDictionary::Column::Longvarbinary);
      assert(sz >= 2);
      Uint32 len = data[0] + (data[1] << 8);
      assert(len == sz - 2);
      assert(len < (1 << 16));
      *m_getVarValue = len;
      data += 2;
      sz -= 2;
    }
    if(!copyoutRequired())
      memcpy(theRef, data, sz);
    else
      memcpy(theValue, data, sz);
    m_size_in_bytes= sz;
    return true;
  } 
  else 
  {
    return setNULL();
  }
  return false;
}

static const NdbRecordPrintFormat default_print_format;

NdbOut&
ndbrecattr_print_formatted(NdbOut& out, const NdbRecAttr &r,
                           const NdbRecordPrintFormat &f)
{
  return NdbDictionary::printFormattedValue(out,
                                            f,
                                            r.getColumn(),
                                            r.isNULL()==0 ? r.aRef() : 0);
}

NdbOut& operator<<(NdbOut& out, const NdbRecAttr &r)
{
  return ndbrecattr_print_formatted(out, r, default_print_format);
}

Int64
NdbRecAttr::int64_value() const 
{
  Int64 val;
  memcpy(&val,theRef,8);
  return val;
}

Uint64
NdbRecAttr::u_64_value() const
{
  Uint64 val;
  memcpy(&val,theRef,8);
  return val;
}

float
NdbRecAttr::float_value() const
{
  float val;
  memcpy(&val,theRef,sizeof(val));
  return val;
}

double
NdbRecAttr::double_value() const
{
  double val;
  memcpy(&val,theRef,sizeof(val));
  return val;
}

Int32
NdbRecAttr::medium_value() const
{
  return sint3korr((unsigned char *)theRef);
}

Uint32
NdbRecAttr::u_medium_value() const
{
  return uint3korr((unsigned char*)theRef);
}
