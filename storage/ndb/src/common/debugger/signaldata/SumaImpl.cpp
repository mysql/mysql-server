/* Copyright (c) 2003, 2024, Oracle and/or its affiliates.

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

#include <signaldata/SumaImpl.hpp>

bool printSUB_CREATE_REQ(FILE *output, const Uint32 *theData, Uint32 len,
                         Uint16 /*receiverBlockNo*/) {
  if (len < SubCreateReq::SignalLength) {
    assert(false);
    return false;
  }

  const SubCreateReq *const sig = (const SubCreateReq *)theData;
  fprintf(output, " senderRef: %x\n", sig->senderRef);
  fprintf(output, " senderData: %x\n", sig->senderData);
  fprintf(output, " subscriptionId: %x\n", sig->subscriptionId);
  fprintf(output, " subscriptionKey: %x\n", sig->subscriptionKey);
  fprintf(output, " subscriptionType: %x\n", sig->subscriptionType);
  fprintf(output, " tableId: %x\n", sig->tableId);
  fprintf(output, " schemaTransId: %x\n", sig->schemaTransId);
  return false;
}

bool printSUB_CREATE_CONF(FILE *output, const Uint32 *theData, Uint32 len,
                          Uint16 /*receiverBlockNo*/) {
  if (len < SubCreateConf::SignalLength) {
    assert(false);
    return false;
  }

  const SubCreateConf *const sig = (const SubCreateConf *)theData;
  fprintf(output, " senderData: %x\n", sig->senderData);
  return false;
}

bool printSUB_CREATE_REF(FILE *output, const Uint32 *theData, Uint32 len,
                         Uint16 /*receiverBlockNo*/) {
  if (len < SubCreateRef::SignalLength) {
    assert(false);
    return false;
  }

  const SubCreateRef *const sig = (const SubCreateRef *)theData;
  fprintf(output, " senderData: %x\n", sig->senderData);
  return false;
}

bool printSUB_REMOVE_REQ(FILE *output, const Uint32 *theData, Uint32 len,
                         Uint16 /*receiverBlockNo*/) {
  if (len < SubRemoveReq::SignalLength) {
    assert(false);
    return false;
  }

  const SubRemoveReq *const sig = (const SubRemoveReq *)theData;
  fprintf(output, " subscriptionId: %x\n", sig->subscriptionId);
  fprintf(output, " subscriptionKey: %x\n", sig->subscriptionKey);
  return false;
}

bool printSUB_REMOVE_CONF(FILE *output, const Uint32 *theData, Uint32 len,
                          Uint16 /*receiverBlockNo*/) {
  if (len < SubRemoveConf::SignalLength) {
    assert(false);
    return false;
  }

  const SubRemoveConf *const sig = (const SubRemoveConf *)theData;
  fprintf(output, " subscriptionId: %x\n", sig->subscriptionId);
  fprintf(output, " subscriptionKey: %x\n", sig->subscriptionKey);
  fprintf(output, " senderData: %x\n", sig->senderData);
  return false;
}

bool printSUB_REMOVE_REF(FILE *output, const Uint32 *theData, Uint32 len,
                         Uint16 /*receiverBlockNo*/) {
  if (len < SubRemoveRef::SignalLength) {
    assert(false);
    return false;
  }

  const SubRemoveRef *const sig = (const SubRemoveRef *)theData;
  fprintf(output, " subscriptionId: %x\n", sig->subscriptionId);
  fprintf(output, " subscriptionKey: %x\n", sig->subscriptionKey);
  fprintf(output, " senderData: %x\n", sig->senderData);
  fprintf(output, " errorCode: %x\n", sig->errorCode);
  return false;
}

bool printSUB_START_REQ(FILE *output, const Uint32 *theData, Uint32 len,
                        Uint16 /*receiverBlockNo*/) {
  if (len < SubStartReq::SignalLength) {
    assert(false);
    return false;
  }

  const SubStartReq *const sig = (const SubStartReq *)theData;
  fprintf(output, " subscriptionId: %x\n", sig->subscriptionId);
  fprintf(output, " subscriptionKey: %x\n", sig->subscriptionKey);
  fprintf(output, " senderData: %x\n", sig->senderData);
  if (len > SubStartReq::SignalLengthWithoutRequestInfo)
    fprintf(output, " requestInfo: %x\n", sig->requestInfo);

  return false;
}

bool printSUB_START_REF(FILE *output, const Uint32 *theData, Uint32 len,
                        Uint16 /*receiverBlockNo*/) {
  if (len < SubStartRef::SignalLength) {
    assert(false);
    return false;
  }

  const SubStartRef *const sig = (const SubStartRef *)theData;
  fprintf(output, " subscriptionId: %x\n", sig->subscriptionId);
  fprintf(output, " subscriptionKey: %x\n", sig->subscriptionKey);
  fprintf(output, " startPart: %x\n", sig->part);
  fprintf(output, " senderData: %x\n", sig->senderData);
  fprintf(output, " errorCode: %x\n", sig->errorCode);
  return false;
}

bool printSUB_START_CONF(FILE *output, const Uint32 *theData, Uint32 len,
                         Uint16 /*receiverBlockNo*/) {
  if (len < SubStartConf::SignalLength) {
    assert(false);
    return false;
  }

  const SubStartConf *const sig = (const SubStartConf *)theData;
  fprintf(output, " subscriptionId: %x\n", sig->subscriptionId);
  fprintf(output, " subscriptionKey: %x\n", sig->subscriptionKey);
  fprintf(output, " startPart: %x\n", sig->part);
  fprintf(output, " senderData: %x\n", sig->senderData);
  return false;
}

bool printSUB_STOP_REQ(FILE *output, const Uint32 *theData, Uint32 len,
                       Uint16 /*receiverBlockNo*/) {
  if (len < SubStopReq::SignalLength) {
    assert(false);
    return false;
  }

  const SubStopReq *const sig = (const SubStopReq *)theData;
  fprintf(output, " subscriptionId: %x\n", sig->subscriptionId);
  fprintf(output, " subscriptionKey: %x\n", sig->subscriptionKey);
  fprintf(output, " senderData: %x\n", sig->senderData);
  return false;
}

bool printSUB_STOP_REF(FILE *output, const Uint32 *theData, Uint32 len,
                       Uint16 /*receiverBlockNo*/) {
  if (len < SubStopRef::SignalLength) {
    assert(false);
    return false;
  }

  const SubStopRef *const sig = (const SubStopRef *)theData;
  fprintf(output, " subscriptionId: %x\n", sig->subscriptionId);
  fprintf(output, " subscriptionKey: %x\n", sig->subscriptionKey);
  fprintf(output, " senderData: %x\n", sig->senderData);
  fprintf(output, " errorCode: %x\n", sig->errorCode);
  return false;
}

bool printSUB_STOP_CONF(FILE *output, const Uint32 *theData, Uint32 len,
                        Uint16 /*receiverBlockNo*/) {
  if (len < SubStopConf::SignalLength) {
    assert(false);
    return false;
  }

  const SubStopConf *const sig = (const SubStopConf *)theData;
  fprintf(output, " subscriptionId: %x\n", sig->subscriptionId);
  fprintf(output, " subscriptionKey: %x\n", sig->subscriptionKey);
  fprintf(output, " senderData: %x\n", sig->senderData);
  return false;
}

bool printSUB_SYNC_REQ(FILE *output, const Uint32 *theData, Uint32 len,
                       Uint16 /*receiverBlockNo*/) {
  if (len < SubSyncReq::SignalLength) {
    assert(false);
    return false;
  }

  const SubSyncReq *const sig = (const SubSyncReq *)theData;
  fprintf(output, " subscriptionId: %x\n", sig->subscriptionId);
  fprintf(output, " subscriptionKey: %x\n", sig->subscriptionKey);
  fprintf(output, " syncPart: %x\n", sig->part);
  fprintf(output, " requestInfo : %x\n", sig->requestInfo);
  fprintf(output, " fragCount : %u\n", sig->fragCount);
  fprintf(output, " fragId : %u\n", sig->fragId);
  fprintf(output, " batchSize : %u\n", sig->batchSize);
  return false;
}

bool printSUB_SYNC_REF(FILE *output, const Uint32 *theData, Uint32 len,
                       Uint16 /*receiverBlockNo*/) {
  if (len < SubSyncRef::SignalLength) {
    assert(false);
    return false;
  }

  const SubSyncRef *const sig = (const SubSyncRef *)theData;
  fprintf(output, " senderData: %x\n", sig->senderData);
  fprintf(output, " errorCode: %x\n", sig->errorCode);
  return false;
}

bool printSUB_SYNC_CONF(FILE *output, const Uint32 *theData, Uint32 len,
                        Uint16 /*receiverBlockNo*/) {
  if (len < SubSyncConf::SignalLength) {
    assert(false);
    return false;
  }

  const SubSyncConf *const sig = (const SubSyncConf *)theData;
  fprintf(output, " senderData: %x\n", sig->senderData);
  return false;
}

bool printSUB_TABLE_DATA(FILE *output, const Uint32 *theData, Uint32 len,
                         Uint16 /*receiverBlockNo*/) {
  if (len < SubTableData::SignalLength) {
    assert(false);
    return false;
  }

  const SubTableData *const sig = (const SubTableData *)theData;
  fprintf(output, " senderData: %x\n", sig->senderData);
  fprintf(output, " gci_hi: %x\n", sig->gci_hi);
  fprintf(output, " gci_lo: %x\n", sig->gci_lo);
  fprintf(output, " tableId: %x\n", sig->tableId);
  fprintf(output, " operation: %x\n",
          SubTableData::getOperation(sig->requestInfo));
  if (len == SubTableData::SignalLengthWithTransId) {
    fprintf(output, " TransId : %x %x\n", sig->transId1, sig->transId2);
  }
  return false;
}

bool printSUB_SYNC_CONTINUE_REQ(FILE *output, const Uint32 *theData, Uint32 len,
                                Uint16 /*receiverBlockNo*/) {
  if (len < SubSyncContinueReq::SignalLength) {
    assert(false);
    return false;
  }

  const SubSyncContinueReq *const sig = (const SubSyncContinueReq *)theData;
  fprintf(output, " subscriberData: %x\n", sig->subscriberData);
  fprintf(output, " noOfRowsSent: %x\n", sig->noOfRowsSent);
  return false;
}

bool printSUB_SYNC_CONTINUE_REF(FILE *output, const Uint32 *theData, Uint32 len,
                                Uint16 /*receiverBlockNo*/) {
  if (len < SubSyncContinueRef::SignalLength) {
    assert(false);
    return false;
  }

  const SubSyncContinueRef *const sig = (const SubSyncContinueRef *)theData;
  fprintf(output, " subscriptionId: %x\n", sig->subscriptionId);
  fprintf(output, " subscriptionKey: %x\n", sig->subscriptionKey);
  return false;
}

bool printSUB_SYNC_CONTINUE_CONF(FILE *output, const Uint32 *theData,
                                 Uint32 len, Uint16 /*receiverBlockNo*/) {
  if (len < SubSyncContinueConf::SignalLength) {
    assert(false);
    return false;
  }

  const SubSyncContinueConf *const sig = (const SubSyncContinueConf *)theData;
  fprintf(output, " subscriptionId: %x\n", sig->subscriptionId);
  fprintf(output, " subscriptionKey: %x\n", sig->subscriptionKey);
  return false;
}

bool printSUB_GCP_COMPLETE_REP(FILE *output, const Uint32 *theData, Uint32 len,
                               Uint16 /*receiverBlockNo*/) {
  if (len < SubGcpCompleteRep::SignalLength) {
    assert(false);
    return false;
  }

  const SubGcpCompleteRep *const sig = (const SubGcpCompleteRep *)theData;
  fprintf(output, " gci_hi: %x gci_lo: %x\n", sig->gci_hi, sig->gci_lo);
  return false;
}
