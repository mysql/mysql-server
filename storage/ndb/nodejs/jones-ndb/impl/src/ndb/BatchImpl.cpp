/*
 Copyright (c) 2013, 2022, Oracle and/or its affiliates.
 
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


#include <NdbApi.hpp>

#include "adapter_global.h"
#include "js_wrapper_macros.h"
#include "Record.h"
#include "NdbWrappers.h"
#include "BatchImpl.h"


BatchImpl::BatchImpl(TransactionImpl * ctx, int _sz) :
  keyOperations(new KeyOperation[_sz]),
  ops(new const NdbOperation *[_sz]),
  errors(new NdbError[_sz]),
  size(_sz),
  doesReadBlobs(false),
  transactionImpl(ctx),
  transactionNdbError(0)
{}


BatchImpl::~BatchImpl() {
  DEBUG_PRINT("BatchImpl destructor [size %d]", size);
  delete[] keyOperations;
  delete[] ops;
  delete[] errors;
  delete transactionNdbError;
}

void BatchImpl::setOperationNdbError(int i, const NdbError & err) {
  if(err.code > 0) {
    errors[i].status = err.status;
    errors[i].classification = err.classification;
    errors[i].code = err.code;
    errors[i].mysql_code = err.mysql_code;
    errors[i].message = err.message;
  }
}

void BatchImpl::prepare(NdbTransaction *ndbtx) {
  for(int i = 0 ; i < size ; i++) {
    ops[i] = 0;
    if(keyOperations[i].opcode > 0) {
      const NdbOperation *op = keyOperations[i].prepare(ndbtx);
      if(op) {
        ops[i] = op;
      } else {
        setOperationNdbError(i, ndbtx->getNdbError());
      }
      DEBUG_PRINT("prepare %s [%s]", keyOperations[i].getOperationName(),
                  op ? "ok" : errors[i].message);
      if(keyOperations[i].isBlobReadOperation()) doesReadBlobs = true;
    }
  }
}

bool BatchImpl::tryImmediateStartTransaction() {
  if(doesReadBlobs) {
    return false;
  }
  return transactionImpl->tryImmediateStartTransaction(& keyOperations[0]);
}

void BatchImpl::saveNdbErrors() {
  transactionNdbError = new NdbError(transactionImpl->getNdbError());
  for(int i = 0 ; i < size ; i++)
    if(ops[i])
      setOperationNdbError(i, ops[i]->getNdbError());
}

const NdbError * BatchImpl::getError(int n) {
  if(n < size) {  // If operation is not open, use saved error
    return ops[n] ? & ops[n]->getNdbError() : & errors[n];
  }
  return 0;  // This becomes JavaScript "true"
}

const NdbError & BatchImpl::getNdbError() {
  return transactionNdbError ?
    * transactionNdbError : transactionImpl->getNdbError();
}

void BatchImpl::transactionIsClosed() {
  for(int i = 0 ; i < size ; i++)
    ops[i] = 0;
}
