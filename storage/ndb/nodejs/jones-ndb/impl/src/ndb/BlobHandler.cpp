/*
 Copyright (c) 2016, 2023, Oracle and/or its affiliates.
 
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

#include <stdlib.h>
#include <assert.h>

#include <NdbApi.hpp>

#include "adapter_global.h"
#include "unified_debug.h"
#include "BlobHandler.h"
#include "JsWrapper.h"
#include "JsValueAccess.h"

// BlobHandler constructor
BlobHandler::BlobHandler(int colId, int fieldNo) :
  ndbBlob(0),
  next(0),
  content(0),
  length(0),
  columnId(colId), 
  fieldNumber(fieldNo)
{ 
}


// Helper functions for BlobReadHandler
int blobHandlerActiveHook(NdbBlob * ndbBlob, void * handler) {
  BlobReadHandler * blobHandler = static_cast<BlobReadHandler *>(handler);
  return blobHandler->runActiveHook(ndbBlob);
}

void freeBufferContentsFromJs(char *data, void *) {
  DEBUG_PRINT("Free %p", data);
  free(data);                                                // here is free
}


// BlobReadHandler methods 
void BlobReadHandler::prepare(const NdbOperation * ndbop) {
  ndbBlob = ndbop->getBlobHandle(columnId);
  assert(ndbBlob);
  ndbBlob->setActiveHook(blobHandlerActiveHook, this);

  if(next) next->prepare(ndbop);
}

int BlobReadHandler::runActiveHook(NdbBlob *b) {
  assert(b == ndbBlob);
  int isNull;
  ndbBlob->getNull(isNull);
  if(! isNull) {
    ndbBlob->getLength(length);
    uint32_t nBytes = static_cast<uint32_t>(length);
    content = (char *) malloc(length);                        // here is malloc
    if(content) {
      int rv = ndbBlob->readData(content, nBytes);
      DEBUG_PRINT("BLOB read: column %d, length %llu, read %d/%d", 
                  columnId, length, rv, nBytes);
    } else {
      return -1;
    }
  }
  return 0;
}

Local<Object> BlobReadHandler::getResultBuffer(v8::Isolate * iso) {
  Local<Object> buffer;
  if(content) {
    buffer = NewJsBuffer(iso, content, length, freeBufferContentsFromJs);
    /* Content belongs to someone else now; clear it for the next user */
    content = 0;
    length = 0;
  }
  return buffer;
}


// BlobWriteHandler methods

BlobWriteHandler::BlobWriteHandler(int colId, int fieldNo,
                                   Local<Object> blobValue) :
  BlobHandler(colId, fieldNo)
{
  length = GetBufferLength(blobValue);
  content = GetBufferData(blobValue);
}

void BlobWriteHandler::prepare(const NdbOperation * ndbop) {
  ndbBlob = ndbop->getBlobHandle(columnId);
  if(! ndbBlob) { 
    DEBUG_PRINT("getBlobHandle %d: [%d] %s", columnId, 
                ndbop->getNdbError().code, ndbop->getNdbError().message);
    assert(false);
  }

  DEBUG_PRINT("Prepare write for BLOB column %d, length %llu", columnId, length);
  ndbBlob->setValue(content, static_cast<uint32_t>(length));
  if(next) next->prepare(ndbop);
}

