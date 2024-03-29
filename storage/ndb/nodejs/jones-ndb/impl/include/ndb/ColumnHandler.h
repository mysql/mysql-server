/*
 Copyright (c) 2013, 2023, Oracle and/or its affiliates.
 
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

#include "NdbTypeEncoders.h"
#include "unified_debug.h"

using v8::Local;
using v8::Value;
using v8::Object;

class BlobWriteHandler;

class ColumnHandler {
public:
  ColumnHandler();
  ~ColumnHandler();
  void init(v8::Isolate *, const NdbDictionary::Column *, uint32_t);
  Local<Value> read(char *, Local<Object>) const;
  Local<Value> write(Local<Value>, char *) const;
  BlobWriteHandler * createBlobWriteHandle(Local<Value>, int fieldNo) const;
  bool isBlob() const;

public:
  const NdbDictionary::Column *column;
private: 
  const NdbTypeEncoder *encoder;
  v8::Isolate *isolate;
  uint32_t offset;
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


