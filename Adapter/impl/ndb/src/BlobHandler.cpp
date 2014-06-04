/*
 Copyright (c) 2014, Oracle and/or its affiliates. All rights
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

#include <assert.h>
#include <NdbApi.hpp>
#include <node_buffer.h>

#include "BlobHandler.h"


BlobHandler::BlobHandler(int field, v8::Handle<v8::Value> jsBlobs) :
  ndbBlob(0),
  fieldNumber(field),
  next(0)
{
  jsBlobsArray = jsBlobs->ToObject();
};

void BlobHandler::prepareRead(const NdbOperation * ndbop) {
  ndbBlob = ndbop->getBlobHandle(fieldNumber);
  assert(ndbBlob);

  if(next) next->prepareRead(ndbop);
};

void BlobHandler::prepareWrite(const NdbOperation * ndbop) {
  ndbBlob = ndbop->getBlobHandle(fieldNumber);
  assert(ndbBlob);
  
  v8::Handle<v8::Object> buffer = jsBlobsArray->Get(fieldNumber)->ToObject();
  
  char * content = node::Buffer::Data(buffer);
  size_t length  = node::Buffer::Length(buffer);

  ndbBlob->setValue(content, length);
  
  if(next) next->prepareWrite(ndbop);
};

