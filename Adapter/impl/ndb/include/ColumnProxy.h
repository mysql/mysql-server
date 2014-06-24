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

#include "ColumnHandler.h"

using namespace v8;

class ColumnProxy {
public:
  ColumnProxy();
  ~ColumnProxy();
  void setHandler(const ColumnHandler *);
  void setBlobBuffer(Handle<Object>);
  bool valueIsNull();
  BlobWriteHandler * createBlobWriteHandle(int);

  Handle<Value> get(char *);
  void          set(Handle<Value>);
  Handle<Value> write(char *);

protected:
  void Dispose();

private:
  const ColumnHandler *handler;
  Persistent<Value> jsValue;
  Persistent<Object> blobBuffer;
  bool isNull;           // value has been set to null
  bool isLoaded;         // value has been read from buffer
  bool isDirty;          // value should be rewritten in buffer
};


inline ColumnProxy::ColumnProxy() :
  jsValue(), blobBuffer(), isNull(false), isLoaded(false), isDirty(false)
{}

inline void ColumnProxy::setHandler(const ColumnHandler *h) {
  handler = h;
}

inline bool ColumnProxy::valueIsNull() {
  return isNull;
}

