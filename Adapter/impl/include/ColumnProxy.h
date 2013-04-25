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
#include <NdbApi.hpp>

#include "ColumnHandler.h"

using namespace v8;

class ColumnProxy {
public:
  ColumnProxy();
  ~ColumnProxy();
  void setHandler(const ColumnHandler *);

  Handle<Value> get(char *);
  void          set(Handle<Value>);
  Handle<Value> write(char *);
  bool          isNull;  // value has been set to null

private:
  const ColumnHandler *handler;
  Persistent<Value> jsValue;
  char * recodeBuffer;
  bool isLoaded;         // value has been read from buffer
  bool isDirty;          // value should be rewritten in buffer
};


inline ColumnProxy::ColumnProxy() :
  isNull(false), recodeBuffer(0), isLoaded(false), isDirty(false)
{}

inline void ColumnProxy::setHandler(const ColumnHandler *h) {
  handler = h;
}


