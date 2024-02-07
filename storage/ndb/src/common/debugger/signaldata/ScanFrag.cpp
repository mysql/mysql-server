/*
   Copyright (c) 2004, 2024, Oracle and/or its affiliates.

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

#include <BlockNumbers.h>
#include <signaldata/ScanFrag.hpp>
#include <signaldata/ScanTab.hpp>

bool printSCAN_FRAGREQ(FILE *output, const Uint32 *theData, Uint32 len,
                       Uint16 /*receiverBlockNo*/) {
  if (len < ScanFragReq::SignalLength) {
    assert(false);
    return false;
  }

  const ScanFragReq *const sig = (const ScanFragReq *)theData;
  fprintf(output, " senderData: 0x%x\n", sig->senderData);
  fprintf(output, " resultRef: 0x%x\n", sig->resultRef);
  fprintf(output, " savePointId: %u\n", sig->savePointId);

  fprintf(output, " flags: ");
  if (ScanFragReq::getLockMode(sig->requestInfo)) fprintf(output, "X");
  if (ScanFragReq::getPrioAFlag(sig->requestInfo)) fprintf(output, "a");
  if (ScanFragReq::getHoldLockFlag(sig->requestInfo)) fprintf(output, "h");
  if (ScanFragReq::getKeyinfoFlag(sig->requestInfo)) fprintf(output, "k");
  if (ScanFragReq::getReadCommittedFlag(sig->requestInfo)) fprintf(output, "d");
  if (ScanFragReq::getRangeScanFlag(sig->requestInfo)) fprintf(output, "r");
  if (ScanFragReq::getDescendingFlag(sig->requestInfo))
    fprintf(output, "(desc)");
  if (ScanFragReq::getTupScanFlag(sig->requestInfo)) fprintf(output, "t");
  if (ScanFragReq::getFirstMatchFlag(sig->requestInfo)) fprintf(output, "f");
  if (ScanFragReq::getQueryThreadFlag(sig->requestInfo)) fprintf(output, "q");
  if (ScanFragReq::getNoDiskFlag(sig->requestInfo)) fprintf(output, "(nodisk)");
  fprintf(output, " attrLen: %u", ScanFragReq::getAttrLen(sig->requestInfo));
  fprintf(output, " reorg: %u", ScanFragReq::getReorgFlag(sig->requestInfo));
  fprintf(output, " corr: %u",
          ScanFragReq::getCorrFactorFlag(sig->requestInfo));
  fprintf(output, " mfrag: %u",
          ScanFragReq::getMultiFragFlag(sig->requestInfo));
  fprintf(output, " stat: %u", ScanFragReq::getStatScanFlag(sig->requestInfo));
  fprintf(output, " ni: %u",
          ScanFragReq::getNotInterpretedFlag(sig->requestInfo));
  fprintf(output, "\n");

  fprintf(output, " tableId: %u\n", sig->tableId);
  fprintf(output, " fragmentNo: %u\n", sig->fragmentNoKeyLen & 0xFFFF);
  fprintf(output, " keyLen: %u\n", sig->fragmentNoKeyLen >> 16);
  fprintf(output, " schemaVersion: 0x%x\n", sig->schemaVersion);
  fprintf(output, " transId1: 0x%x\n", sig->transId1);
  fprintf(output, " transId2: 0x%x\n", sig->transId2);
  fprintf(output, " clientOpPtr: 0x%x\n", sig->clientOpPtr);
  fprintf(output, " batch_size_rows: %u\n", sig->batch_size_rows);
  fprintf(output, " batch_size_bytes: %u\n", sig->batch_size_bytes);

  if (ScanFragReq::getCorrFactorFlag(sig->requestInfo)) {
    fprintf(output, " corrFactorLo: 0x%x\n", sig->variableData[0]);
    fprintf(output, " corrFactorHi: 0x%x\n", sig->variableData[1]);
  }

  return true;
}

bool printSCAN_FRAGCONF(FILE *output, const Uint32 *theData, Uint32 len,
                        Uint16 /*receiverBlockNo*/) {
  const ScanFragConf *const sig =
      reinterpret_cast<const ScanFragConf *>(theData);
  fprintf(output, " senderData: 0x%x\n", sig->senderData);
  fprintf(output, " completedOps: %u\n", sig->completedOps);
  fprintf(output, " fragmentCompleted: 0x%x\n", sig->fragmentCompleted);
  fprintf(output, " transId1: 0x%x\n", sig->transId1);
  fprintf(output, " transId2: 0x%x\n", sig->transId2);
  fprintf(output, " total_len: %u\n", sig->total_len);
  if (len >= ScanFragConf::SignalLength_ext)
    fprintf(output, " activeMask: 0x%x\n", sig->activeMask);
  else
    fprintf(output, " activeMask: 0(not an ext-signal)\n");
  if (len >= ScanFragConf::SignalLength_query)
    fprintf(output, " senderRef = %x\n", sig->senderRef);
  return true;
}
