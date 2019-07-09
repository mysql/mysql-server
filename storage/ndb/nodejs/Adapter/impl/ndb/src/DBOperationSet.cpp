/*
 Copyright (c) 2014, Oracle and/or its affiliates. All rights
 reserved.
 
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
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
 */


#include <NdbApi.hpp>

#include "adapter_global.h"
#include "js_wrapper_macros.h"
#include "Record.h"
#include "NdbWrappers.h"
#include "DBOperationSet.h"

DBOperationSet::~DBOperationSet() {
  DEBUG_PRINT("DBOperationSet destructor [size %d]", size);
  delete[] keyOperations;
  delete[] ops;
  delete[] errors;
}

void DBOperationSet::prepare(NdbTransaction *ndbtx) {
  for(int i = 0 ; i < size ; i++) {
    if(keyOperations[i].opcode > 0) {
      const NdbOperation *op = keyOperations[i].prepare(ndbtx);
      ops[i] = op;
      if(! op) errors[i] = & ndbtx->getNdbError();
      DEBUG_PRINT("prepare %s [%s]", keyOperations[i].getOperationName(),
                  op ? "ok" : errors[i]->message);
      if(keyOperations[i].isBlobReadOperation()) doesReadBlobs = true;
    }
    else {
      errors[i] = 0;
      ops[i] = 0;
    }
  }
}

bool DBOperationSet::tryImmediateStartTransaction() {
  if(doesReadBlobs) {
    return false;
  }
  return txContext->tryImmediateStartTransaction(& keyOperations[0]);
}


