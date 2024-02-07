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

#include <DebuggerNames.hpp>
#include <RefConvert.hpp>
#include <signaldata/LCP.hpp>

bool printSTART_LCP_REQ(FILE *output, const Uint32 *theData, Uint32 len,
                        Uint16 /*receiverBlockNo*/) {
  if (len < StartLcpReq::SignalLength) {
    assert(false);
    return false;
  }

  const StartLcpReq *const sig = (const StartLcpReq *)theData;

  char buf1[NdbNodeBitmask48::TextLength + 1];
  char buf2[NdbNodeBitmask48::TextLength + 1];

  if (sig->participatingDIH_v1.isclear() &&
      sig->participatingLQH_v1.isclear()) {
    fprintf(output, " ParticipatingDIH and ParticipatingLQH in signal section");
  } else {
    fprintf(output,
            " Sender: %d LcpId: %d PauseStart: %d\n"
            " ParticipatingDIH = %s\n"
            " ParticipatingLQH = %s\n",
            refToNode(sig->senderRef), sig->lcpId, sig->pauseStart,
            sig->participatingDIH_v1.getText(buf1),
            sig->participatingLQH_v1.getText(buf2));
  }
  return true;
}

bool printSTART_LCP_CONF(FILE *output, const Uint32 *theData, Uint32 len,
                         Uint16 /*receiverBlockNo*/) {
  if (len < StartLcpConf::SignalLength) {
    assert(false);
    return false;
  }

  const StartLcpConf *const sig = (const StartLcpConf *)theData;

  fprintf(output, " Sender: %d LcpId: %d\n", refToNode(sig->senderRef),
          sig->lcpId);

  return true;
}

bool printLCP_FRAG_ORD(FILE *output, const Uint32 *theData, Uint32 len,
                       Uint16 /*receiverBlockNo*/) {
  if (len < LcpFragOrd::SignalLength) {
    assert(false);
    return false;
  }

  const LcpFragOrd *const sig = (const LcpFragOrd *)theData;

  fprintf(output, " LcpId: %d LcpNo: %d Table: %d Fragment: %d\n", sig->lcpId,
          sig->lcpNo, sig->tableId, sig->fragmentId);

  fprintf(output, " KeepGCI: %d LastFragmentFlag: %d\n", sig->keepGci,
          sig->lastFragmentFlag);
  return true;
}

bool printLCP_FRAG_REP(FILE *output, const Uint32 *theData, Uint32 len,
                       Uint16 /*receiverBlockNo*/) {
  if (len < LcpFragRep::SignalLength) {
    assert(false);
    return false;
  }

  const LcpFragRep *const sig = (const LcpFragRep *)theData;

  fprintf(output, " LcpId: %d LcpNo: %d NodeId: %d Table: %d Fragment: %d\n",
          sig->lcpId, sig->lcpNo, sig->nodeId, sig->tableId, sig->fragId);
  fprintf(output, " Max GCI Started: %d Max GCI Completed: %d\n",
          sig->maxGciStarted, sig->maxGciCompleted);
  return true;
}

bool printLCP_COMPLETE_REP(FILE *output, const Uint32 *theData, Uint32 len,
                           Uint16 /*receiverBlockNo*/) {
  if (len < LcpCompleteRep::SignalLength) {
    assert(false);
    return false;
  }

  const LcpCompleteRep *const sig = (const LcpCompleteRep *)theData;

  fprintf(output, " LcpId: %d NodeId: %d Block: %s\n", sig->lcpId, sig->nodeId,
          getBlockName(sig->blockNo));
  return true;
}

bool printLCP_STATUS_REQ(FILE *output, const Uint32 *theData, Uint32 len,
                         Uint16 /*receiverBlockNo*/) {
  if (len < LcpStatusReq::SignalLength) {
    assert(false);
    return false;
  }

  const LcpStatusReq *const sig = (const LcpStatusReq *)theData;

  fprintf(output, " SenderRef : %x SenderData : %u\n", sig->senderRef,
          sig->senderData);
  return true;
}

bool printLCP_STATUS_CONF(FILE *output, const Uint32 *theData, Uint32 len,
                          Uint16 /*receiverBlockNo*/) {
  if (len < LcpStatusConf::SignalLength) {
    assert(false);
    return false;
  }

  const LcpStatusConf *const sig = (const LcpStatusConf *)theData;

  fprintf(output,
          " SenderRef : %x SenderData : %u LcpState : %u tableId : %u fragId : "
          "%u\n",
          sig->senderRef, sig->senderData, sig->lcpState, sig->tableId,
          sig->fragId);
  fprintf(output,
          " replica(Progress : %llu), lcpDone (Rows : %llu, Bytes : %llu)\n",
          (((Uint64)sig->completionStateHi) << 32) + sig->completionStateLo,
          (((Uint64)sig->lcpDoneRowsHi) << 32) + sig->lcpDoneRowsLo,
          (((Uint64)sig->lcpDoneBytesHi) << 32) + sig->lcpDoneBytesLo);
  fprintf(output, "lcpScannedPages : %u\n", sig->lcpScannedPages);
  return true;
}

bool printLCP_STATUS_REF(FILE *output, const Uint32 *theData, Uint32 len,
                         Uint16 /*receiverBlockNo*/) {
  if (len < LcpStatusRef::SignalLength) {
    assert(false);
    return false;
  }

  const LcpStatusRef *const sig = (const LcpStatusRef *)theData;

  fprintf(output, " SenderRef : %x, SenderData : %u Error : %u\n",
          sig->senderRef, sig->senderData, sig->error);
  return true;
}

bool printLCP_PREPARE_REQ(FILE *output, const Uint32 *theData, Uint32 len,
                          Uint16 /*receiverBlockNo*/) {
  if (len < LcpPrepareReq::SignalLength) {
    assert(false);
    return false;
  }

  const LcpPrepareReq *const sig = (const LcpPrepareReq *)theData;

  fprintf(output,
          "senderData: %x, senderRef: %x, lcpNo: %u, tableId: %u, "
          "fragmentId: %u\n"
          "lcpId: %u, localLcpId: %u, backupPtr: %u, backupId: %u,"
          " createGci: %u\n",
          sig->senderData, sig->senderRef, sig->lcpNo, sig->tableId,
          sig->fragmentId, sig->lcpId, sig->localLcpId, sig->backupPtr,
          sig->backupId, sig->createGci);
  return true;
}

bool printLCP_PREPARE_CONF(FILE *output, const Uint32 *theData, Uint32 len,
                           Uint16 /*receiverBlockNo*/) {
  if (len < LcpPrepareConf::SignalLength) {
    assert(false);
    return false;
  }

  const LcpPrepareConf *const sig = (const LcpPrepareConf *)theData;

  fprintf(output,
          "senderData: %x, senderRef: %x, tableId: %u, fragmentId: %u\n",
          sig->senderData, sig->senderRef, sig->tableId, sig->fragmentId);
  return true;
}

bool printLCP_PREPARE_REF(FILE *output, const Uint32 *theData, Uint32 len,
                          Uint16 /*receiverBlockNo*/) {
  if (len < LcpPrepareRef::SignalLength) {
    assert(false);
    return false;
  }

  const LcpPrepareRef *const sig = (const LcpPrepareRef *)theData;

  fprintf(output,
          "senderData: %x, senderRef: %x, tableId: %u, fragmentId: %u"
          ", errorCode: %u\n",
          sig->senderData, sig->senderRef, sig->tableId, sig->fragmentId,
          sig->errorCode);
  return true;
}

bool printSYNC_PAGE_CACHE_REQ(FILE *output, const Uint32 *theData, Uint32 len,
                              Uint16 /*receiverBlockNo*/) {
  if (len < SyncPageCacheReq::SignalLength) {
    assert(false);
    return false;
  }

  const SyncPageCacheReq *const sig = (const SyncPageCacheReq *)theData;
  fprintf(output,
          "senderData: %x, senderRef: %x, tableId: %u, fragmentId: %u\n",
          sig->senderData, sig->senderRef, sig->tableId, sig->fragmentId);
  return true;
}

bool printSYNC_PAGE_CACHE_CONF(FILE *output, const Uint32 *theData, Uint32 len,
                               Uint16 /*receiverBlockNo*/) {
  if (len < SyncPageCacheConf::SignalLength) {
    assert(false);
    return false;
  }

  const SyncPageCacheConf *const sig = (const SyncPageCacheConf *)theData;
  fprintf(output,
          "senderData: %x, senderRef: %x, tableId: %u, fragmentId: %u\n"
          "diskDataExistFlag: %u\n",
          sig->senderData, sig->senderRef, sig->tableId, sig->fragmentId,
          sig->diskDataExistFlag);
  return true;
}

bool printEND_LCPREQ(FILE *output, const Uint32 *theData, Uint32 len,
                     Uint16 /*receiverBlockNo*/) {
  if (len < EndLcpReq::SignalLength) {
    assert(false);
    return false;
  }

  const EndLcpReq *const sig = (const EndLcpReq *)theData;
  fprintf(output,
          "senderData: %x, senderRef: %x, backupPtr: %u, backupId: %u\n"
          "proxyBlockNo: %u\n",
          sig->senderData, sig->senderRef, sig->backupPtr, sig->backupId,
          sig->proxyBlockNo);
  return true;
}

bool printEND_LCPCONF(FILE *output, const Uint32 *theData, Uint32 len,
                      Uint16 /*receiverBlockNo*/) {
  if (len < EndLcpConf::SignalLength) {
    assert(false);
    return false;
  }

  const EndLcpConf *const sig = (const EndLcpConf *)theData;
  fprintf(output, "senderData: %x, senderRef: %x\n", sig->senderData,
          sig->senderRef);
  return true;
}
