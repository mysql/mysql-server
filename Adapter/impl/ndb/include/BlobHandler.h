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
  BlobHandler(int columnNumber, v8::Handle<v8::Object> jsBlobs);
  ~BlobHandler();
  void prepareRead(const NdbOperation *);
  void prepareWrite(const NdbOperation *);  
  void setNext(BlobHandler *);
  BlobHandler * getNext();

private:
  NdbBlob * ndbBlob;
  v8::Persistent<v8::Object> jsBlobValue;
  int columnNumber;
  BlobHandler * next;
};


inline void BlobHandler::setNext(BlobHandler *that) {
  next = that;
};

inline BlobHandler * BlobHandler::getNext() {
  return next;
};

#endif


