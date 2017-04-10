/*
 Copyright (c) 2013, 2016, Oracle and/or its affiliates. All rights
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

#include "NdbTypeEncoders.h"
#include "unified_debug.h"

using v8::Persistent;
using v8::Handle;
using v8::Value;
using v8::Object;

class BlobWriteHandler;

class ColumnHandler {
public:
  ColumnHandler();
  ~ColumnHandler();
  void init(v8::Isolate *, const NdbDictionary::Column *, uint32_t);
  Handle<Value> read(char *, Handle<Object>) const;
  Handle<Value> write(Handle<Value>, char *) const;
  BlobWriteHandler * createBlobWriteHandle(Local<Value>, int fieldNo) const;
  bool isBlob() const;

public:
  const NdbDictionary::Column *column;
private: 
  uint32_t offset;
  const NdbTypeEncoder *encoder;
  bool isLob, isText;
};

inline bool ColumnHandler::isBlob() const {
  return isLob;
}

class ColumnHandlerSet {
public:
  ColumnHandlerSet(int);
  ~ColumnHandlerSet();
  ColumnHandler * getHandler(int);
private:
  int size;
  ColumnHandler * const handlers;
};


inline ColumnHandlerSet::ColumnHandlerSet(int _size) :
  size(_size),
  handlers(new ColumnHandler[size])
{ }

inline ColumnHandlerSet::~ColumnHandlerSet() {
  delete[] handlers;
}

inline ColumnHandler * ColumnHandlerSet::getHandler(int i) {
  assert(i < size);
  return & handlers[i];
}


