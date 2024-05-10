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

#include <signaldata/TcKeyReq.hpp>

bool printTCKEYREQ(FILE *output, const Uint32 *theData, Uint32 len,
                   Uint16 /*receiverBlockNo*/) {
  const TcKeyReq *const sig = (const TcKeyReq *)theData;

  UintR requestInfo = sig->requestInfo;

  fprintf(output, " apiConnectPtr: H\'%.8x, apiOperationPtr: H\'%.8x\n",
          sig->apiConnectPtr, sig->apiOperationPtr);
  fprintf(output, " Operation: %s, Flags: ",
          sig->getOperationType(requestInfo) == ZREAD      ? "Read"
          : sig->getOperationType(requestInfo) == ZREAD_EX ? "Read-Ex"
          : sig->getOperationType(requestInfo) == ZUPDATE  ? "Update"
          : sig->getOperationType(requestInfo) == ZINSERT  ? "Insert"
          : sig->getOperationType(requestInfo) == ZDELETE  ? "Delete"
          : sig->getOperationType(requestInfo) == ZWRITE   ? "Write"
          : sig->getOperationType(requestInfo) == ZUNLOCK  ? "Unlock"
          : sig->getOperationType(requestInfo) == ZREFRESH ? "Refresh"
                                                           : "Unknown");
  {
    if (sig->getDirtyFlag(requestInfo)) {
      fprintf(output, "Dirty ");
    }
    if (sig->getStartFlag(requestInfo)) {
      fprintf(output, "Start ");
    }
    if (sig->getExecuteFlag(requestInfo)) {
      fprintf(output, "Execute ");
    }
    if (sig->getCommitFlag(requestInfo)) {
      fprintf(output, "Commit ");
    }
    if (sig->getNoDiskFlag(requestInfo)) {
      fprintf(output, "NoDisk ");
    }

    UintR TcommitType = sig->getAbortOption(requestInfo);
    if (TcommitType == TcKeyReq::AbortOnError) {
      fprintf(output, "AbortOnError ");
    } else if (TcommitType == TcKeyReq::IgnoreError) {
      fprintf(output, "IgnoreError ");
    }  // if

    if (sig->getSimpleFlag(requestInfo)) {
      fprintf(output, "Simple ");
    }
    if (sig->getScanIndFlag(requestInfo)) {
      fprintf(output, "ScanInd ");
    }
    if (sig->getInterpretedFlag(requestInfo)) {
      fprintf(output, "Interpreted ");
    }
    if (sig->getDistributionKeyFlag(sig->requestInfo)) {
      fprintf(output, "d-key ");
    }
    if (sig->getViaSPJFlag(sig->requestInfo)) {
      fprintf(output, " spj");
    }
    if (sig->getQueueOnRedoProblemFlag(sig->requestInfo))
      fprintf(output, "Queue ");

    if (sig->getDeferredConstraints(sig->requestInfo))
      fprintf(output, "Deferred-constraints ");

    if (sig->getDisableFkConstraints(sig->requestInfo))
      fprintf(output, "Disable-FK-constraints ");

    if (sig->getReorgFlag(sig->requestInfo)) fprintf(output, "reorg ");

    if (sig->getReadCommittedBaseFlag(sig->requestInfo))
      fprintf(output, "rc_base ");

    if (sig->getNoWaitFlag(sig->requestInfo)) fprintf(output, "nowait");

    fprintf(output, "\n");
  }

  const int keyLen = sig->getKeyLength(requestInfo);
  const int attrInThis = sig->getAIInTcKeyReq(requestInfo);
  const int attrLen = sig->getAttrinfoLen(sig->attrLen);
  fprintf(output,
          " keyLen: %d, attrLen: %d, AI in this: %d, tableId: %d, "
          "tableSchemaVer: %d\n",
          keyLen, attrLen, attrInThis, sig->tableId, sig->tableSchemaVersion);

  fprintf(output, " transId(1, 2): (H\'%.8x, H\'%.8x)\n -- Variable Data --\n",
          sig->transId1, sig->transId2);

  if (len >= TcKeyReq::StaticLength) {
    Uint32 restLen = (len - TcKeyReq::StaticLength);
    const Uint32 *rest = &sig->scanInfo;
    while (restLen >= 7) {
      fprintf(output,
              " H\'%.8x H\'%.8x H\'%.8x H\'%.8x H\'%.8x H\'%.8x H\'%.8x\n",
              rest[0], rest[1], rest[2], rest[3], rest[4], rest[5], rest[6]);
      restLen -= 7;
      rest += 7;
    }
    if (restLen > 0) {
      for (Uint32 i = 0; i < restLen; i++) fprintf(output, " H\'%.8x", rest[i]);
      fprintf(output, "\n");
    }
  } else {
    fprintf(output, "*** invalid len %u ***\n", len);
  }
  return true;
}
