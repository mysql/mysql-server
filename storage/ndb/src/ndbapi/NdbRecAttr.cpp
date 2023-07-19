/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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


#include <ndb_global.h>
#include <NdbOut.hpp>
#include <NdbRecAttr.hpp>
#include <NdbBlob.hpp>
#include "NdbDictionaryImpl.hpp"
#include <NdbTCP.h>

NdbRecAttr::NdbRecAttr(Ndb*)
{
  theStorageX = nullptr;
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
  m_size_in_bytes = -1; // UNDEFINED

  return setup(tAttrByteSize, aValue);
}

int 
NdbRecAttr::setup(Uint32 byteSize, char* aValue)
{
  theValue = aValue;
  m_getVarValue = nullptr; // set in getVarValue() only

  delete[] theStorageX;
  theStorageX = nullptr;
  
  // Check if application provided pointer should be used
  // NOTE! Neither pointers alignment or length of attribute matters since
  // memcpy() will be used to copy received data there.
  if (aValue != nullptr) {
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
  if (tRef != nullptr) {
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


NdbRecAttr *
NdbRecAttr::clone() const {
  NdbRecAttr * ret = new NdbRecAttr(nullptr);
  if (ret == nullptr)
  {
    errno = ENOMEM;
    return nullptr;
  }
  ret->theAttrId = theAttrId;
  ret->m_size_in_bytes = m_size_in_bytes;
  ret->m_column = m_column;
  
  Uint32 n = m_size_in_bytes;
  if(n <= 32){
    ret->theRef = (char*)&ret->theStorage[0];
    ret->theStorageX = nullptr;
    ret->theValue = nullptr;
  } else {
    ret->theStorageX = new Uint64[((n + 7) >> 3)];
    if (ret->theStorageX == nullptr)
    {
      delete ret;
      errno = ENOMEM;
      return nullptr;
    }
    ret->theRef = (char*)ret->theStorageX;    
    ret->theValue = nullptr;
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
    if (unlikely(m_getVarValue != nullptr)) {
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

    // Copy received data to destination pointer
    memcpy(theRef, data, sz);

    m_size_in_bytes= sz;
    return true;
  } 

  return setNULL();
}

static const NdbRecordPrintFormat default_print_format;

NdbOut&
ndbrecattr_print_formatted(NdbOut& out, const NdbRecAttr &r,
                           const NdbRecordPrintFormat &f)
{
  return NdbDictionary::printFormattedValue(out,
                                            f,
                                            r.getColumn(),
                                            r.isNULL()==0 ? r.aRef() : nullptr);
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
