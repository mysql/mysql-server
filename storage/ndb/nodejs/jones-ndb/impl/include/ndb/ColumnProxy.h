/*
 Copyright (c) 2013, 2024, Oracle and/or its affiliates.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is designed to work with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have either included with
 the program or referenced in the documentation.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#include "ColumnHandler.h"

using v8::Persistent;

class ColumnProxy {
  friend class NdbRecordObject;

 protected:
  ColumnProxy();
  ~ColumnProxy();
  void setHandler(const ColumnHandler *);
  void setBlobBuffer(v8::Isolate *, Local<Object>);
  bool valueIsNull();
  BlobWriteHandler *createBlobWriteHandle(v8::Isolate *, int);

  Local<Value> get(v8::Isolate *, char *);
  void set(v8::Isolate *, Local<Value>);
  Local<Value> write(v8::Isolate *, char *);

 private:
  const ColumnHandler *handler;
  Persistent<Value> jsValue;
  Persistent<Object> blobBuffer;
  bool isNull;    // value has been set to null
  bool isLoaded;  // value has been read from buffer
  bool isDirty;   // value should be rewritten in buffer
};

inline ColumnProxy::ColumnProxy()
    : jsValue(), blobBuffer(), isNull(false), isLoaded(false), isDirty(false) {}

inline ColumnProxy::~ColumnProxy() {
  jsValue.Reset();
  blobBuffer.Reset();
}

inline void ColumnProxy::setHandler(const ColumnHandler *h) { handler = h; }

inline bool ColumnProxy::valueIsNull() { return isNull; }

inline void ColumnProxy::setBlobBuffer(v8::Isolate *isolate,
                                       Local<Object> buffer) {
  blobBuffer.Reset(isolate, buffer);
}
