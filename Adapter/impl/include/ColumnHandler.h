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

#include "Record.h"
#include "NdbTypeEncoders.h"

using namespace v8;

class ColumnHandler {
public:
  ColumnHandler();
  ~ColumnHandler();
  void init(const NdbDictionary::Column *, size_t, Handle<Value>);
  Handle<Value> read(char *) const;
  Handle<Value> write(Handle<Value>, char *) const;
    
private: 
  const NdbDictionary::Column *column;
  size_t offset;
  const NdbTypeEncoder *encoder;
  Persistent<Object> converterClass;
  Persistent<Object> converterReader;
  Persistent<Object> converterWriter;
  bool hasConverterReader, hasConverterWriter;
};

