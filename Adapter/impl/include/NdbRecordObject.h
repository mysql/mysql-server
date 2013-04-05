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

#include <node.h>

#include "ColumnProxy.h"


class NdbRecordObject {
public:
  NdbRecordObject(Record *, ColumnHandlerSet *, Handle<Value>);
  ~NdbRecordObject();
  
  Handle<Value> getField(int);
  void setField(int nField, Handle<Value> value);
  Handle<Value> prepare();
  void resetMask();

  Record * getRecord() const;
  char * getBuffer() const;
  const uint8_t * getMask() const;

private:
  Record * record;
  char * buffer;
  ColumnHandlerSet * handlers;
  Persistent<Value> persistentBufferHandle;
  const unsigned int ncol;
  ColumnProxy * const proxy;
  uint8_t row_mask[4];
  void maskIn(unsigned int nField);
  bool isMaskedIn(unsigned int nField);
};


inline void NdbRecordObject::maskIn(unsigned int nField) {
  assert(nField < ncol);
  row_mask[nField >> 3] |= (1 << (nField & 7));
}

  
inline bool NdbRecordObject::isMaskedIn(unsigned int nField) {
  assert(nField < ncol);
  return (row_mask[nField >> 3] & (1<<(nField & 7)));
}


inline void NdbRecordObject::setField(int nField, Handle<Value> value) {
  maskIn(nField); 
  proxy[nField].set(value);
}


inline Record * NdbRecordObject::getRecord() const {
  return record;
}


inline char * NdbRecordObject::getBuffer() const {
  return buffer;
}


inline const uint8_t * NdbRecordObject::getMask() const {
  return & row_mask[0];
}


inline void NdbRecordObject::resetMask() {
  row_mask[3] = row_mask[2] = row_mask[1] = row_mask[0] = 0;
}

