/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <signaldata/LqhTransConf.hpp>

bool printLQH_TRANSCONF(FILE *output, const Uint32 *theData, Uint32 len,
                        Uint16 /*receiverBlockNo*/) {
  if (len < LqhTransConf::SignalLength) {
    assert(false);
    return false;
  }

  const LqhTransConf *const sig = (const LqhTransConf *)theData;
  fprintf(output, " tcRef: %x\n", sig->tcRef);
  fprintf(output, " lqhNodeId: %x\n", sig->lqhNodeId);
  fprintf(output, " operationStatus: %x\n", sig->operationStatus);
  fprintf(output, " transId1: %x\n", sig->transId1);
  fprintf(output, " transId2: %x\n", sig->transId2);
  fprintf(output, " apiRef: %x\n", sig->apiRef);
  fprintf(output, " apiOpRec: %x\n", sig->apiOpRec);
  fprintf(output, " lqhConnectPtr: %x\n", sig->lqhConnectPtr);
  fprintf(output, " oldTcOpRec: %x\n", sig->oldTcOpRec);
  fprintf(output, " requestInfo: %x\n", sig->requestInfo);
  fprintf(output, " gci_hi: %x\n", sig->gci_hi);
  fprintf(output, " gci_lo: %x\n", sig->gci_lo);
  fprintf(output, " nextNodeId1: %x\n", sig->nextNodeId1);
  fprintf(output, " nextNodeId2: %x\n", sig->nextNodeId2);
  fprintf(output, " nextNodeId3: %x\n", sig->nextNodeId3);
  fprintf(output, " tableId: %x\n", sig->tableId);
  return true;
}
