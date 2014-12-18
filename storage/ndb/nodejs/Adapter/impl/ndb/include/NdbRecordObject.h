/*
 Copyright (c) 2013, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
*/

#include "Record.h"
#include "ColumnProxy.h"
#include "KeyOperation.h"

class NdbRecordObject {
public:
  NdbRecordObject(const Record *, ColumnHandlerSet *, Handle<Value>, Handle<Value>);
  ~NdbRecordObject();
  
  Handle<Value> getField(int);
  void setField(int nField, Handle<Value> value);
  Handle<Value> prepare();
  void resetMask();

  const Record * getRecord() const;
  char * getBuffer() const;
  uint32_t getMaskValue() const;
  unsigned short getWriteCount() const;
  int createBlobWriteHandles(KeyOperation &);

private:
  const Record * record;
  char * buffer;
  ColumnHandlerSet * handlers;
  Persistent<Value> persistentBufferHandle;
  const unsigned int ncol;
  ColumnProxy * const proxy;
  union {
    uint8_t row_mask[4];
    uint32_t maskvalue;
  } u;
  unsigned short nWrites;
  
  void maskIn(unsigned int nField);
  bool isMaskedIn(unsigned int nField);
};


inline void NdbRecordObject::maskIn(unsigned int nField) {
  assert(nField < ncol);
  u.row_mask[nField >> 3] |= (1 << (nField & 7));
}

  
inline bool NdbRecordObject::isMaskedIn(unsigned int nField) {
  assert(nField < ncol);
  return (u.row_mask[nField >> 3] & (1<<(nField & 7)));
}


inline void NdbRecordObject::setField(int nField, Handle<Value> value) {
  nWrites++;
  maskIn(nField);
  proxy[nField].set(value);
}


inline const Record * NdbRecordObject::getRecord() const {
  return record;
}


inline char * NdbRecordObject::getBuffer() const {
  return buffer;
}


inline uint32_t NdbRecordObject::getMaskValue() const {
  return u.maskvalue;
}


inline void NdbRecordObject::resetMask() {
  u.maskvalue = 0;
}

inline unsigned short NdbRecordObject::getWriteCount() const {
  return nWrites;
}

