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

#include <signaldata/LqhKey.hpp>

bool printLQHKEYREQ(FILE *output, const Uint32 *theData, Uint32 len,
                    Uint16 /*receiverBlockNo*/) {
  if (len < LqhKeyReq::FixedSignalLength) {
    assert(false);
    return false;
  }

  const LqhKeyReq *const sig = (const LqhKeyReq *)theData;

  fprintf(output,
          " ClientPtr = H\'%.8x hashValue = H\'%.8x tcBlockRef = H\'%.8x\n"
          " transId1 = H\'%.8x transId2 = H\'%.8x savePointId = H\'%.8x\n",
          sig->clientConnectPtr,  // DATA 0
          sig->hashValue,         // DATA 2
          sig->tcBlockref,        // DATA 4
          sig->transId1,          // DATA 7
          sig->transId2,          // DATA 8
          sig->savePointId        // DATA 9
  );

  const Uint32 reqInfo = sig->requestInfo;
  const Uint32 attrLen = sig->attrLen;

  fprintf(
      output, " Operation: %s\n",
      LqhKeyReq::getOperation(reqInfo) == ZREAD
          ? "Read"
          : LqhKeyReq::getOperation(reqInfo) == ZREAD_EX
                ? "Read-Ex"
                : LqhKeyReq::getOperation(reqInfo) == ZUPDATE
                      ? "Update"
                      : LqhKeyReq::getOperation(reqInfo) == ZINSERT
                            ? "Insert"
                            : LqhKeyReq::getOperation(reqInfo) == ZDELETE
                                  ? "Delete"
                                  : LqhKeyReq::getOperation(reqInfo) == ZWRITE
                                        ? "Write"
                                        : LqhKeyReq::getOperation(reqInfo) ==
                                                  ZUNLOCK
                                              ? "Unlock"
                                              : LqhKeyReq::getOperation(
                                                    reqInfo) == ZREFRESH
                                                    ? "Refresh"
                                                    : "Unknown");

  fprintf(output, " Op: %d Lock: %d Flags: ", LqhKeyReq::getOperation(reqInfo),
          LqhKeyReq::getLockType(reqInfo));
  if (LqhKeyReq::getSimpleFlag(reqInfo)) fprintf(output, "Simple ");
  if (LqhKeyReq::getDirtyFlag(reqInfo)) {
    if (LqhKeyReq::getNormalProtocolFlag(reqInfo))
      fprintf(output, "Dirty(N) ");
    else
      fprintf(output, "Dirty ");
  }
  if (LqhKeyReq::getInterpretedFlag(reqInfo)) fprintf(output, "Interpreted ");
  if (LqhKeyReq::getScanTakeOverFlag(attrLen)) fprintf(output, "ScanTakeOver ");
  if (LqhKeyReq::getReorgFlag(attrLen))
    fprintf(output, "reorg: %u ", LqhKeyReq::getReorgFlag(attrLen));
  if (LqhKeyReq::getMarkerFlag(reqInfo)) fprintf(output, "CommitAckMarker ");
  if (LqhKeyReq::getNoDiskFlag(reqInfo)) fprintf(output, "NoDisk ");
  if (LqhKeyReq::getRowidFlag(reqInfo)) fprintf(output, "Rowid ");
  if (LqhKeyReq::getNrCopyFlag(reqInfo)) fprintf(output, "NrCopy ");
  if (LqhKeyReq::getGCIFlag(reqInfo)) fprintf(output, "GCI ");
  if (LqhKeyReq::getQueueOnRedoProblemFlag(reqInfo)) fprintf(output, "Queue ");
  if (LqhKeyReq::getDeferredConstraints(reqInfo))
    fprintf(output, "Deferred-constraints ");
  if (LqhKeyReq::getNoTriggersFlag(reqInfo)) fprintf(output, "NoTriggers ");
  if (LqhKeyReq::getUtilFlag(reqInfo)) fprintf(output, "UtilFlag ");
  if (LqhKeyReq::getNoWaitFlag(reqInfo)) fprintf(output, "NoWait ");

  fprintf(output, "ScanInfo/noFiredTriggers: H\'%x\n", sig->scanInfo);

  if (LqhKeyReq::getDisableFkConstraints(reqInfo))
    fprintf(output, "Disable FK constraints");

  fprintf(output,
          " AttrLen: %d (%d in this) KeyLen: %d TableId: %d SchemaVer: %d\n",
          LqhKeyReq::getAttrLen(attrLen), LqhKeyReq::getAIInLqhKeyReq(reqInfo),
          LqhKeyReq::getKeyLen(reqInfo),
          LqhKeyReq::getTableId(sig->tableSchemaVersion),
          LqhKeyReq::getSchemaVersion(sig->tableSchemaVersion));

  fprintf(output, " FragId: %d ReplicaNo: %d LastReplica: %d NextNodeId: %d\n",
          LqhKeyReq::getFragmentId(sig->fragmentData),
          LqhKeyReq::getSeqNoReplica(reqInfo),
          LqhKeyReq::getLastReplicaNo(reqInfo),
          LqhKeyReq::getNextReplicaNodeId(sig->fragmentData));

  bool printed = false;
  Uint32 nextPos = LqhKeyReq::getApplicationAddressFlag(reqInfo) << 1;
  if (nextPos != 0) {
    fprintf(output, " ApiRef: H\'%.8x ApiOpRef: H\'%.8x", sig->variableData[0],
            sig->variableData[1]);
    printed = true;
  }

  if (LqhKeyReq::getSameClientAndTcFlag(reqInfo)) {
    fprintf(output, " TcOpRec: H\'%.8x", sig->variableData[nextPos]);
    nextPos++;
    printed = true;
  }

  Uint32 tmp = LqhKeyReq::getLastReplicaNo(reqInfo) -
               LqhKeyReq::getSeqNoReplica(reqInfo);
  if (tmp > 1) {
    NodeId node2 = sig->variableData[nextPos] & 0xffff;
    NodeId node3 = sig->variableData[nextPos] >> 16;
    fprintf(output, " NextNodeId2: %d NextNodeId3: %d", node2, node3);
    nextPos++;
    printed = true;
  }
  if (printed) fprintf(output, "\n");

  printed = false;
  if (LqhKeyReq::getStoredProcFlag(attrLen)) {
    fprintf(output, " StoredProcId: %d", sig->variableData[nextPos]);
    nextPos++;
    printed = true;
  }

  if (LqhKeyReq::getReturnedReadLenAIFlag(reqInfo)) {
    fprintf(output, " ReturnedReadLenAI: %d", sig->variableData[nextPos]);
    nextPos++;
    printed = true;
  }

  /**
   * Key info is only sent here if short signal, we assume it
   * is a long signal.
   *
  const UintR keyLen = LqhKeyReq::getKeyLen(reqInfo);
  if(keyLen > 0){
    fprintf(output, " KeyInfo: ");
    for(UintR i = 0; i<keyLen && i<4; i++, nextPos++)
      fprintf(output, "H\'%.8x ", sig->variableData[nextPos]);
    fprintf(output, "\n");
  }
  */

  if (LqhKeyReq::getRowidFlag(reqInfo)) {
    fprintf(output, " Rowid: [ page: %d idx: %d ]\n",
            sig->variableData[nextPos + 0], sig->variableData[nextPos + 1]);
    nextPos += 2;
  }

  if (LqhKeyReq::getGCIFlag(reqInfo)) {
    fprintf(output, " GCI: %u", sig->variableData[nextPos + 0]);
    nextPos++;
  }

  if (LqhKeyReq::getCorrFactorFlag(reqInfo)) {
    fprintf(output, " corrFactorLo: 0x%x", sig->variableData[nextPos + 0]);
    nextPos++;
    fprintf(output, " corrFactorHi: 0x%x", sig->variableData[nextPos + 0]);
    nextPos++;
  }

  if (!LqhKeyReq::getInterpretedFlag(reqInfo)) {
    fprintf(output, " AttrInfo: ");
    for (int i = 0; i < LqhKeyReq::getAIInLqhKeyReq(reqInfo); i++, nextPos++)
      fprintf(output, "H\'%.8x ", sig->variableData[nextPos]);
    fprintf(output, "\n");
  } else {
    /* Only have section sizes if it's a short LQHKEYREQ */
    if (LqhKeyReq::getAIInLqhKeyReq(reqInfo) == LqhKeyReq::MaxAttrInfo) {
      fprintf(output,
              " InitialReadSize: %d InterpretedSize: %d "
              "FinalUpdateSize: %d FinalReadSize: %d SubroutineSize: %d\n",
              sig->variableData[nextPos + 0], sig->variableData[nextPos + 1],
              sig->variableData[nextPos + 2], sig->variableData[nextPos + 3],
              sig->variableData[nextPos + 4]);
      nextPos += 5;
    }
  }
  return true;
}

bool printLQHKEYCONF(FILE *output, const Uint32 *theData, Uint32 len,
                     Uint16 /*receiverBlockNo*/) {
  //  const LqhKeyConf * const sig = (const LqhKeyConf *) theData;

  fprintf(output, "Signal data: ");
  Uint32 i = 0;
  while (i < len) fprintf(output, "H\'%.8x ", theData[i++]);
  fprintf(output, "\n");

  return true;
}

bool printLQHKEYREF(FILE *output, const Uint32 *theData, Uint32 len,
                    Uint16 /*receiverBlockNo*/) {
  //  const LqhKeyRef * const sig = (const LqhKeyRef *) theData;

  fprintf(output, "Signal data: ");
  Uint32 i = 0;
  while (i < len) fprintf(output, "H\'%.8x ", theData[i++]);
  fprintf(output, "\n");

  return true;
}
