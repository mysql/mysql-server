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

#include "KeyOperation.h"

KeyOperation::~KeyOperation() {
  while(blobHandler) {
    BlobHandler *b = blobHandler;
    blobHandler = b->getNext();
    delete b;
  }
}

/* BlobHandler::prepareRead() and prepareWrite() will iterate the 
   chain of BlobHandlers themselves, so we call the head only. 
*/
inline void KeyOperation::prepareBlobReads(const NdbOperation * op) {
  if(blobHandler) {
    blobHandler->prepareRead(op);
  }
}

inline void KeyOperation::prepareBlobWrites(const NdbOperation *op) {
  if(blobHandler) {
    blobHandler->prepareWrite(op);
  }
}

const NdbOperation * 
  KeyOperation::readTuple(NdbTransaction *tx) {
    const NdbOperation *op;
    op = tx->readTuple(key_record->getNdbRecord(), key_buffer,
                       row_record->getNdbRecord(), row_buffer, lmode, read_mask_ptr);
    prepareBlobReads(op);
    return op;                       
}

const NdbOperation * KeyOperation::deleteTuple(NdbTransaction *tx) {
    return tx->deleteTuple(key_record->getNdbRecord(), key_buffer,
                           row_record->getNdbRecord(), 0, 0, options);
}                         

const NdbOperation * KeyOperation::writeTuple(NdbTransaction *tx) { 
  const NdbOperation *op;
  op = tx->writeTuple(key_record->getNdbRecord(), key_buffer,
                      row_record->getNdbRecord(), row_buffer, u.row_mask);
  prepareBlobWrites(op);
  return op;
}

const NdbOperation * KeyOperation::insertTuple(NdbTransaction *tx) { 
  const NdbOperation *op;
  op = tx->insertTuple(row_record->getNdbRecord(), row_buffer,
                       u.row_mask, options);
  prepareBlobWrites(op);
  return op;
}

const NdbOperation * KeyOperation::updateTuple(NdbTransaction *tx) { 
  const NdbOperation *op;
  op = tx->updateTuple(key_record->getNdbRecord(), key_buffer,
                       row_record->getNdbRecord(), row_buffer,
                       u.row_mask, options);
  prepareBlobWrites(op);
  return op;
}


const NdbOperation * KeyOperation::prepare(NdbTransaction *tx) {
  switch(opcode) {
    case 1:  // OP_READ:
      opcode_str = "read  ";
      return readTuple(tx);
    case 2:  // OP_INSERT:
      opcode_str = "insert";
      return insertTuple(tx);
    case 4:  // OP_UPDATE:
      opcode_str = "update";
      return updateTuple(tx);
    case 8:  // OP_WRITE:
      opcode_str = "write ";
      return writeTuple(tx);
    case 16: // OP_DELETE:
      opcode_str = "delete";
      return deleteTuple(tx);
    default:
      opcode_str = "-XXX-";
      return NULL;
  }
}


