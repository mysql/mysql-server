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

#include "v8.h"

#ifndef NODEJS_ADAPTER_NDB_BLOBHANDLER_H
#define NODEJS_ADAPTER_NDB_BLOBHANDLER_H

class BlobHandler { 
public:
  BlobHandler(int columnId, int fieldNumber);
  BlobHandler * getNext();
  void setNext(BlobHandler *);
  int getFieldNumber();

  virtual ~BlobHandler() {};
  virtual void prepare(const NdbOperation *) = 0;
  
protected:
  NdbBlob * ndbBlob;
  BlobHandler * next;
  char * content;
  unsigned long long length;
  int columnId;
  int fieldNumber;
};


// BlobReadHandler
class BlobReadHandler : public BlobHandler {
public:
  BlobReadHandler(int columnId, int fieldNumber);
  void prepare(const NdbOperation *);
  int runActiveHook(NdbBlob *);
  v8::Handle<v8::Value> getResultBuffer();
};  


// BlobWriteHandler
class BlobWriteHandler : public BlobHandler {
public:
  BlobWriteHandler(int colId, int fieldNo, v8::Handle<v8::Object> jsBlob);
  void prepare(const NdbOperation *);
};


// BlobHandler inline methods
inline void BlobHandler::setNext(BlobHandler *that) {
  next = that;
}

inline BlobHandler * BlobHandler::getNext() {
  return next;
}

inline int BlobHandler::getFieldNumber() {
  return fieldNumber;
}


// BlobReadHandler inline methods
inline BlobReadHandler::BlobReadHandler(int colId, int fieldNo) : 
  BlobHandler(colId, fieldNo)
{ }

#endif
