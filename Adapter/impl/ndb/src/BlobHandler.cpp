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

#include "adapter_global.h"
#include "unified_debug.h"
#include "BlobHandler.h"


BlobHandler::BlobHandler(int field, v8::Handle<v8::Object> blobValue) :
  ndbBlob(0),
  columnNumber(field),
  next(0)
{
  jsBlobValue = v8::Persistent<v8::Object>::New(blobValue);
  DEBUG_PRINT("NEW %p", this);
}

BlobHandler::~BlobHandler() 
{
  jsBlobValue.Dispose();
}

void BlobHandler::prepareRead(const NdbOperation * ndbop) {
  ndbBlob = ndbop->getBlobHandle(columnNumber);
  assert(ndbBlob);

  if(next) next->prepareRead(ndbop);
}

void BlobHandler::prepareWrite(const NdbOperation * ndbop) {
  ndbBlob = ndbop->getBlobHandle(columnNumber);
  if(! ndbBlob) { 
    DEBUG_PRINT("getBlobHandle: %s", ndbop->getNdbError().message);
    assert(false);
  }

  char * content = node::Buffer::Data(jsBlobValue);
  size_t length  = node::Buffer::Length(jsBlobValue);
  DEBUG_PRINT("Prepare write for BLOB column %d, length %d", columnNumber, length);

  ndbBlob->setValue(content, length);
  
  if(next) next->prepareWrite(ndbop);
}

