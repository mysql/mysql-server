/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#define DBTUX_SCAN_CPP
#include "Dbtux.hpp"
#include <my_sys.h>

void
Dbtux::execACC_SCANREQ(Signal* signal)
{
  jamEntry();
  const AccScanReq reqCopy = *(const AccScanReq*)signal->getDataPtr();
  const AccScanReq* const req = &reqCopy;
  ScanOpPtr scanPtr;
  scanPtr.i = RNIL;
  do {
    // get the index
    IndexPtr indexPtr;
    c_indexPool.getPtr(indexPtr, req->tableId);
    // get the fragment
    FragPtr fragPtr;
    fragPtr.i = RNIL;
    for (unsigned i = 0; i < indexPtr.p->m_numFrags; i++) {
      jam();
      if (indexPtr.p->m_fragId[i] == req->fragmentNo) {
        jam();
        c_fragPool.getPtr(fragPtr, indexPtr.p->m_fragPtrI[i]);
        break;
      }
    }
    ndbrequire(fragPtr.i != RNIL);
    Frag& frag = *fragPtr.p;
    // must be normal DIH/TC fragment
    TreeHead& tree = frag.m_tree;
    // check for empty fragment
    if (tree.m_root == NullTupLoc) {
      jam();
      AccScanConf* const conf = (AccScanConf*)signal->getDataPtrSend();
      conf->scanPtr = req->senderData;
      conf->accPtr = RNIL;
      conf->flag = AccScanConf::ZEMPTY_FRAGMENT;
      sendSignal(req->senderRef, GSN_ACC_SCANCONF,
          signal, AccScanConf::SignalLength, JBB);
      return;
    }
    // seize from pool and link to per-fragment list
    if (! frag.m_scanList.seize(scanPtr)) {
      jam();
      break;
    }
    new (scanPtr.p) ScanOp(c_scanBoundPool);
    scanPtr.p->m_state = ScanOp::First;
    scanPtr.p->m_userPtr = req->senderData;
    scanPtr.p->m_userRef = req->senderRef;
    scanPtr.p->m_tableId = indexPtr.p->m_tableId;
    scanPtr.p->m_indexId = indexPtr.i;
    scanPtr.p->m_fragId = fragPtr.p->m_fragId;
    scanPtr.p->m_fragPtrI = fragPtr.i;
    scanPtr.p->m_transId1 = req->transId1;
    scanPtr.p->m_transId2 = req->transId2;
    scanPtr.p->m_savePointId = req->savePointId;
    scanPtr.p->m_readCommitted = AccScanReq::getReadCommittedFlag(req->requestInfo);
    scanPtr.p->m_lockMode = AccScanReq::getLockMode(req->requestInfo);
    scanPtr.p->m_descending = AccScanReq::getDescendingFlag(req->requestInfo);
    /*
     * readCommitted lockMode keyInfo
     * 1 0 0 - read committed (no lock)
     * 0 0 0 - read latest (read lock)
     * 0 1 1 - read exclusive (write lock)
     */
#ifdef VM_TRACE
    if (debugFlags & DebugScan) {
      debugOut << "Seize scan " << scanPtr.i << " " << *scanPtr.p << endl;
    }
#endif
    // conf
    AccScanConf* const conf = (AccScanConf*)signal->getDataPtrSend();
    conf->scanPtr = req->senderData;
    conf->accPtr = scanPtr.i;
    conf->flag = AccScanConf::ZNOT_EMPTY_FRAGMENT;
    sendSignal(req->senderRef, GSN_ACC_SCANCONF,
        signal, AccScanConf::SignalLength, JBB);
    return;
  } while (0);
  if (scanPtr.i != RNIL) {
    jam();
    releaseScanOp(scanPtr);
  }
  // LQH does not handle REF
  signal->theData[0] = 0x313;
  sendSignal(req->senderRef, GSN_ACC_SCANREF,
      signal, 1, JBB);
}

/*
 * Receive bounds for scan in single direct call.  The bounds can arrive
 * in any order.  Attribute ids are those of index table.
 *
 * Replace EQ by equivalent LE + GE.  Check for conflicting bounds.
 * Check that sets of lower and upper bounds are on initial sequences of
 * keys and that all but possibly last bound is non-strict.
 *
 * Finally save the sets of lower and upper bounds (i.e. start key and
 * end key).  Full bound type is included but only the strict bit is
 * used since lower and upper have now been separated.
 */
void
Dbtux::execTUX_BOUND_INFO(Signal* signal)
{
  jamEntry();
  // get records
  TuxBoundInfo* const sig = (TuxBoundInfo*)signal->getDataPtrSend();
  const TuxBoundInfo* const req = (const TuxBoundInfo*)sig;
  ScanOp& scan = *c_scanOpPool.getPtr(req->tuxScanPtrI);
  const Index& index = *c_indexPool.getPtr(scan.m_indexId);
  const DescEnt& descEnt = getDescEnt(index.m_descPage, index.m_descOff);
  // collect normalized lower and upper bounds
  struct BoundInfo {
    int type2;     // with EQ -> LE/GE
    Uint32 offset; // offset in xfrmData
    Uint32 size;
  };
  BoundInfo boundInfo[2][MaxIndexAttributes];
  const unsigned dstSize = 1024 * MAX_XFRM_MULTIPLY;
  Uint32 xfrmData[dstSize];
  Uint32 dstPos = 0;
  // largest attrId seen plus one
  Uint32 maxAttrId[2] = { 0, 0 };
  // walk through entries
  const Uint32* const data = (Uint32*)sig + TuxBoundInfo::SignalLength;
  Uint32 offset = 0;
  while (offset + 2 <= req->boundAiLength) {
    jam();
    const unsigned type = data[offset];
    const AttributeHeader* ah = (const AttributeHeader*)&data[offset + 1];
    const Uint32 attrId = ah->getAttributeId();
    const Uint32 dataSize = ah->getDataSize();
    if (type > 4 || attrId >= index.m_numAttrs || dstPos + 2 + dataSize > dstSize) {
      jam();
      scan.m_state = ScanOp::Invalid;
      sig->errorCode = TuxBoundInfo::InvalidAttrInfo;
      return;
    }
    // copy header
    xfrmData[dstPos + 0] = data[offset + 0];
    xfrmData[dstPos + 1] = data[offset + 1];
    // copy bound value
    Uint32 dstWords = 0;
    if (! ah->isNULL()) {
      jam();
      const uchar* srcPtr = (const uchar*)&data[offset + 2];
      const DescAttr& descAttr = descEnt.m_descAttr[attrId];
      Uint32 typeId = descAttr.m_typeId;
      Uint32 maxBytes = AttributeDescriptor::getSizeInBytes(descAttr.m_attrDesc);
      Uint32 lb, len;
      bool ok = NdbSqlUtil::get_var_length(typeId, srcPtr, maxBytes, lb, len);
      if (! ok) {
        jam();
        scan.m_state = ScanOp::Invalid;
        sig->errorCode = TuxBoundInfo::InvalidCharFormat;
        return;
      }
      Uint32 srcBytes = lb + len;
      Uint32 srcWords = (srcBytes + 3) / 4;
      if (srcWords != dataSize) {
        jam();
        scan.m_state = ScanOp::Invalid;
        sig->errorCode = TuxBoundInfo::InvalidAttrInfo;
        return;
      }
      uchar* dstPtr = (uchar*)&xfrmData[dstPos + 2];
      if (descAttr.m_charset == 0) {
        memcpy(dstPtr, srcPtr, srcWords << 2);
        dstWords = srcWords;
      } else {
        jam();
        CHARSET_INFO* cs = all_charsets[descAttr.m_charset];
        Uint32 xmul = cs->strxfrm_multiply;
        if (xmul == 0)
          xmul = 1;
        // see comment in DbtcMain.cpp
        Uint32 dstLen = xmul * (maxBytes - lb);
        if (dstLen > ((dstSize - dstPos) << 2)) {
          jam();
          scan.m_state = ScanOp::Invalid;
          sig->errorCode = TuxBoundInfo::TooMuchAttrInfo;
          return;
        }
        int n = NdbSqlUtil::strnxfrm_bug7284(cs, dstPtr, dstLen, srcPtr + lb, len);
        ndbrequire(n != -1);
        while ((n & 3) != 0) {
          dstPtr[n++] = 0;
        }
        dstWords = n / 4;
      }
    }
    for (unsigned j = 0; j <= 1; j++) {
      jam();
      // check if lower/upper bit matches
      const unsigned luBit = (j << 1);
      if ((type & 0x2) != luBit && type != 4)
        continue;
      // EQ -> LE, GE
      const unsigned type2 = (type & 0x1) | luBit;
      // fill in any gap
      while (maxAttrId[j] <= attrId) {
        jam();
        BoundInfo& b = boundInfo[j][maxAttrId[j]];
        maxAttrId[j]++;
        b.type2 = -1;
      }
      BoundInfo& b = boundInfo[j][attrId];
      if (b.type2 != -1) {
        // compare with previously defined bound
        if (b.type2 != (int)type2 ||
            b.size != 2 + dstWords ||
            memcmp(&xfrmData[b.offset + 2], &xfrmData[dstPos + 2], dstWords << 2) != 0) {
          jam();
          scan.m_state = ScanOp::Invalid;
          sig->errorCode = TuxBoundInfo::InvalidBounds;
          return;
        }
      } else {
        // fix length
        AttributeHeader* ah = (AttributeHeader*)&xfrmData[dstPos + 1];
        ah->setDataSize(dstWords);
        // enter new bound
        jam();
        b.type2 = type2;
        b.offset = dstPos;
        b.size = 2 + dstWords;
      }
    }
    // jump to next
    offset += 2 + dataSize;
    dstPos += 2 + dstWords;
  }
  if (offset != req->boundAiLength) {
    jam();
    scan.m_state = ScanOp::Invalid;
    sig->errorCode = TuxBoundInfo::InvalidAttrInfo;
    return;
  }
  for (unsigned j = 0; j <= 1; j++) {
    // save lower/upper bound in index attribute id order
    for (unsigned i = 0; i < maxAttrId[j]; i++) {
      jam();
      const BoundInfo& b = boundInfo[j][i];
      // check for gap or strict bound before last
      if (b.type2 == -1 || (i + 1 < maxAttrId[j] && (b.type2 & 0x1))) {
        jam();
        scan.m_state = ScanOp::Invalid;
        sig->errorCode = TuxBoundInfo::InvalidBounds;
        return;
      }
      bool ok = scan.m_bound[j]->append(&xfrmData[b.offset], b.size);
      if (! ok) {
        jam();
        scan.m_state = ScanOp::Invalid;
        sig->errorCode = TuxBoundInfo::OutOfBuffers;
        return;
      }
    }
    scan.m_boundCnt[j] = maxAttrId[j];
  }
  // no error
  sig->errorCode = 0;
}

void
Dbtux::execNEXT_SCANREQ(Signal* signal)
{
  jamEntry();
  const NextScanReq reqCopy = *(const NextScanReq*)signal->getDataPtr();
  const NextScanReq* const req = &reqCopy;
  ScanOpPtr scanPtr;
  scanPtr.i = req->accPtr;
  c_scanOpPool.getPtr(scanPtr);
  ScanOp& scan = *scanPtr.p;
  Frag& frag = *c_fragPool.getPtr(scan.m_fragPtrI);
#ifdef VM_TRACE
  if (debugFlags & DebugScan) {
    debugOut << "NEXT_SCANREQ scan " << scanPtr.i << " " << scan << endl;
  }
#endif
  // handle unlock previous and close scan
  switch (req->scanFlag) {
  case NextScanReq::ZSCAN_NEXT:
    jam();
    break;
  case NextScanReq::ZSCAN_NEXT_COMMIT:
    jam();
  case NextScanReq::ZSCAN_COMMIT:
    jam();
    if (! scan.m_readCommitted) {
      jam();
      AccLockReq* const lockReq = (AccLockReq*)signal->getDataPtrSend();
      lockReq->returnCode = RNIL;
      lockReq->requestInfo = AccLockReq::Unlock;
      lockReq->accOpPtr = req->accOperationPtr;
      EXECUTE_DIRECT(DBACC, GSN_ACC_LOCKREQ, signal, AccLockReq::UndoSignalLength);
      jamEntry();
      ndbrequire(lockReq->returnCode == AccLockReq::Success);
      removeAccLockOp(scanPtr, req->accOperationPtr);
    }
    if (req->scanFlag == NextScanReq::ZSCAN_COMMIT) {
      jam();
      NextScanConf* const conf = (NextScanConf*)signal->getDataPtrSend();
      conf->scanPtr = scan.m_userPtr;
      unsigned signalLength = 1;
      sendSignal(scanPtr.p->m_userRef, GSN_NEXT_SCANCONF,
		 signal, signalLength, JBB);
      return;
    }
    break;
  case NextScanReq::ZSCAN_CLOSE:
    jam();
    // unlink from tree node first to avoid state changes
    if (scan.m_scanPos.m_loc != NullTupLoc) {
      jam();
      const TupLoc loc = scan.m_scanPos.m_loc;
      NodeHandle node(frag);
      selectNode(node, loc);
      unlinkScan(node, scanPtr);
      scan.m_scanPos.m_loc = NullTupLoc;
    }
    if (scan.m_lockwait) {
      jam();
      ndbrequire(scan.m_accLockOp != RNIL);
      // use ACC_ABORTCONF to flush out any reply in job buffer
      AccLockReq* const lockReq = (AccLockReq*)signal->getDataPtrSend();
      lockReq->returnCode = RNIL;
      lockReq->requestInfo = AccLockReq::AbortWithConf;
      lockReq->accOpPtr = scan.m_accLockOp;
      EXECUTE_DIRECT(DBACC, GSN_ACC_LOCKREQ, signal, 
		     AccLockReq::UndoSignalLength);
      jamEntry();
      ndbrequire(lockReq->returnCode == AccLockReq::Success);
      scan.m_state = ScanOp::Aborting;
      return;
    }
    if (scan.m_state == ScanOp::Locked) {
      jam();
      ndbrequire(scan.m_accLockOp != RNIL);
      AccLockReq* const lockReq = (AccLockReq*)signal->getDataPtrSend();
      lockReq->returnCode = RNIL;
      lockReq->requestInfo = AccLockReq::Abort;
      lockReq->accOpPtr = scan.m_accLockOp;
      EXECUTE_DIRECT(DBACC, GSN_ACC_LOCKREQ, signal, 
		     AccLockReq::UndoSignalLength);
      jamEntry();
      ndbrequire(lockReq->returnCode == AccLockReq::Success);
      scan.m_accLockOp = RNIL;
    }
    scan.m_state = ScanOp::Aborting;
    scanClose(signal, scanPtr);
    return;
  case NextScanReq::ZSCAN_NEXT_ABORT:
    jam();
  default:
    jam();
    ndbrequire(false);
    break;
  }
  // start looking for next scan result
  AccCheckScan* checkReq = (AccCheckScan*)signal->getDataPtrSend();
  checkReq->accPtr = scanPtr.i;
  checkReq->checkLcpStop = AccCheckScan::ZNOT_CHECK_LCP_STOP;
  EXECUTE_DIRECT(DBTUX, GSN_ACC_CHECK_SCAN, signal, AccCheckScan::SignalLength);
  jamEntry();
}

void
Dbtux::execACC_CHECK_SCAN(Signal* signal)
{
  jamEntry();
  const AccCheckScan reqCopy = *(const AccCheckScan*)signal->getDataPtr();
  const AccCheckScan* const req = &reqCopy;
  ScanOpPtr scanPtr;
  scanPtr.i = req->accPtr;
  c_scanOpPool.getPtr(scanPtr);
  ScanOp& scan = *scanPtr.p;
  Frag& frag = *c_fragPool.getPtr(scan.m_fragPtrI);
#ifdef VM_TRACE
  if (debugFlags & DebugScan) {
    debugOut << "ACC_CHECK_SCAN scan " << scanPtr.i << " " << scan << endl;
  }
#endif
  if (req->checkLcpStop == AccCheckScan::ZCHECK_LCP_STOP) {
    jam();
    signal->theData[0] = scan.m_userPtr;
    signal->theData[1] = true;
    EXECUTE_DIRECT(DBLQH, GSN_CHECK_LCP_STOP, signal, 2);
    jamEntry();
    return;   // stop
  }
  if (scan.m_lockwait) {
    jam();
    // LQH asks if we are waiting for lock and we tell it to ask again
    const TreeEnt ent = scan.m_scanEnt;
    NextScanConf* const conf = (NextScanConf*)signal->getDataPtrSend();
    conf->scanPtr = scan.m_userPtr;
    conf->accOperationPtr = RNIL;       // no tuple returned
    conf->fragId = frag.m_fragId;
    unsigned signalLength = 3;
    // if TC has ordered scan close, it will be detected here
    sendSignal(scan.m_userRef, GSN_NEXT_SCANCONF,
        signal, signalLength, JBB);
    return;     // stop
  }
  if (scan.m_state == ScanOp::First) {
    jam();
    // search is done only once in single range scan
    scanFirst(scanPtr);
  }
  if (scan.m_state == ScanOp::Current ||
      scan.m_state == ScanOp::Next) {
    jam();
    // look for next
    scanFind(scanPtr);
  }
  // for reading tuple key in Found or Locked state
  Data pkData = c_dataBuffer;
  unsigned pkSize = 0; // indicates not yet done
  if (scan.m_state == ScanOp::Found) {
    // found an entry to return
    jam();
    ndbrequire(scan.m_accLockOp == RNIL);
    if (! scan.m_readCommitted) {
      jam();
      const TreeEnt ent = scan.m_scanEnt;
      // read tuple key
      readTablePk(frag, ent, pkData, pkSize);
      // get read lock or exclusive lock
      AccLockReq* const lockReq = (AccLockReq*)signal->getDataPtrSend();
      lockReq->returnCode = RNIL;
      lockReq->requestInfo =
        scan.m_lockMode == 0 ? AccLockReq::LockShared : AccLockReq::LockExclusive;
      lockReq->accOpPtr = RNIL;
      lockReq->userPtr = scanPtr.i;
      lockReq->userRef = reference();
      lockReq->tableId = scan.m_tableId;
      lockReq->fragId = frag.m_fragId;
      lockReq->fragPtrI = frag.m_accTableFragPtrI;
      const Uint32* const buf32 = static_cast<Uint32*>(pkData);
      const Uint64* const buf64 = reinterpret_cast<const Uint64*>(buf32);
      lockReq->hashValue = md5_hash(buf64, pkSize);
      lockReq->tupAddr = getTupAddr(frag, ent);
      lockReq->transId1 = scan.m_transId1;
      lockReq->transId2 = scan.m_transId2;
      // execute
      EXECUTE_DIRECT(DBACC, GSN_ACC_LOCKREQ, signal, AccLockReq::LockSignalLength);
      jamEntry();
      switch (lockReq->returnCode) {
      case AccLockReq::Success:
        jam();
        scan.m_state = ScanOp::Locked;
        scan.m_accLockOp = lockReq->accOpPtr;
#ifdef VM_TRACE
        if (debugFlags & (DebugScan | DebugLock)) {
          debugOut << "Lock immediate scan " << scanPtr.i << " " << scan << endl;
        }
#endif
        break;
      case AccLockReq::IsBlocked:
        jam();
        // normal lock wait
        scan.m_state = ScanOp::Blocked;
        scan.m_lockwait = true;
        scan.m_accLockOp = lockReq->accOpPtr;
#ifdef VM_TRACE
        if (debugFlags & (DebugScan | DebugLock)) {
          debugOut << "Lock wait scan " << scanPtr.i << " " << scan << endl;
        }
#endif
        // LQH will wake us up
        signal->theData[0] = scan.m_userPtr;
        signal->theData[1] = true;
        EXECUTE_DIRECT(DBLQH, GSN_CHECK_LCP_STOP, signal, 2);
        jamEntry();
        return;  // stop
        break;
      case AccLockReq::Refused:
        jam();
        // we cannot see deleted tuple (assert only)
        ndbassert(false);
        // skip it
        scan.m_state = ScanOp::Next;
        signal->theData[0] = scan.m_userPtr;
        signal->theData[1] = true;
        EXECUTE_DIRECT(DBLQH, GSN_CHECK_LCP_STOP, signal, 2);
        jamEntry();
        return;  // stop
        break;
      case AccLockReq::NoFreeOp:
        jam();
        // max ops should depend on max scans (assert only)
        ndbassert(false);
        // stay in Found state
        scan.m_state = ScanOp::Found;
        signal->theData[0] = scan.m_userPtr;
        signal->theData[1] = true;
        EXECUTE_DIRECT(DBLQH, GSN_CHECK_LCP_STOP, signal, 2);
        jamEntry();
        return;  // stop
        break;
      default:
        ndbrequire(false);
        break;
      }
    } else {
      scan.m_state = ScanOp::Locked;
    }
  }
  if (scan.m_state == ScanOp::Locked) {
    // we have lock or do not need one
    jam();
    // read keys if not already done (uses signal)
    const TreeEnt ent = scan.m_scanEnt;
    // conf signal
    NextScanConf* const conf = (NextScanConf*)signal->getDataPtrSend();
    conf->scanPtr = scan.m_userPtr;
    // the lock is passed to LQH
    Uint32 accLockOp = scan.m_accLockOp;
    if (accLockOp != RNIL) {
      scan.m_accLockOp = RNIL;
      // remember it until LQH unlocks it
      addAccLockOp(scanPtr, accLockOp);
    } else {
      ndbrequire(scan.m_readCommitted);
      // operation RNIL in LQH would signal no tuple returned
      accLockOp = (Uint32)-1;
    }
    conf->accOperationPtr = accLockOp;
    conf->fragId = frag.m_fragId;
    conf->localKey[0] = getTupAddr(frag, ent);
    conf->localKey[1] = 0;
    conf->localKeyLength = 1;
    unsigned signalLength = 6;
    // add key info
    if (! scan.m_readCommitted) {
      sendSignal(scan.m_userRef, GSN_NEXT_SCANCONF,
          signal, signalLength, JBB);
    } else {
      Uint32 blockNo = refToBlock(scan.m_userRef);
      EXECUTE_DIRECT(blockNo, GSN_NEXT_SCANCONF, signal, signalLength);
    }
    // next time look for next entry
    scan.m_state = ScanOp::Next;
    return;
  }
  // XXX in ACC this is checked before req->checkLcpStop
  if (scan.m_state == ScanOp::Last ||
      scan.m_state == ScanOp::Invalid) {
    jam();
    NextScanConf* const conf = (NextScanConf*)signal->getDataPtrSend();
    conf->scanPtr = scan.m_userPtr;
    conf->accOperationPtr = RNIL;
    conf->fragId = RNIL;
    unsigned signalLength = 3;
    sendSignal(scanPtr.p->m_userRef, GSN_NEXT_SCANCONF,
        signal, signalLength, JBB);
    return;
  }
  ndbrequire(false);
}

/*
 * Lock succeeded (after delay) in ACC.  If the lock is for current
 * entry, set state to Locked.  If the lock is for an entry we were
 * moved away from, simply unlock it.  Finally, if we are closing the
 * scan, do nothing since we have already sent an abort request.
 */
void
Dbtux::execACCKEYCONF(Signal* signal)
{
  jamEntry();
  ScanOpPtr scanPtr;
  scanPtr.i = signal->theData[0];
  c_scanOpPool.getPtr(scanPtr);
  ScanOp& scan = *scanPtr.p;
#ifdef VM_TRACE
  if (debugFlags & (DebugScan | DebugLock)) {
    debugOut << "Lock obtained scan " << scanPtr.i << " " << scan << endl;
  }
#endif
  ndbrequire(scan.m_lockwait && scan.m_accLockOp != RNIL);
  scan.m_lockwait = false;
  if (scan.m_state == ScanOp::Blocked) {
    // the lock wait was for current entry
    jam();
    scan.m_state = ScanOp::Locked;
    // LQH has the ball
    return;
  }
  if (scan.m_state != ScanOp::Aborting) {
    // we were moved, release lock
    jam();
    AccLockReq* const lockReq = (AccLockReq*)signal->getDataPtrSend();
    lockReq->returnCode = RNIL;
    lockReq->requestInfo = AccLockReq::Abort;
    lockReq->accOpPtr = scan.m_accLockOp;
    EXECUTE_DIRECT(DBACC, GSN_ACC_LOCKREQ, signal, AccLockReq::UndoSignalLength);
    jamEntry();
    ndbrequire(lockReq->returnCode == AccLockReq::Success);
    scan.m_accLockOp = RNIL;
    // LQH has the ball
    return;
  }
  // lose the lock
  scan.m_accLockOp = RNIL;
  // continue at ACC_ABORTCONF
}

/*
 * Lock failed (after delay) in ACC.  Probably means somebody ahead of
 * us in lock queue deleted the tuple.
 */
void
Dbtux::execACCKEYREF(Signal* signal)
{
  jamEntry();
  ScanOpPtr scanPtr;
  scanPtr.i = signal->theData[0];
  c_scanOpPool.getPtr(scanPtr);
  ScanOp& scan = *scanPtr.p;
#ifdef VM_TRACE
  if (debugFlags & (DebugScan | DebugLock)) {
    debugOut << "Lock refused scan " << scanPtr.i << " " << scan << endl;
  }
#endif
  ndbrequire(scan.m_lockwait && scan.m_accLockOp != RNIL);
  scan.m_lockwait = false;
  if (scan.m_state != ScanOp::Aborting) {
    jam();
    // release the operation
    AccLockReq* const lockReq = (AccLockReq*)signal->getDataPtrSend();
    lockReq->returnCode = RNIL;
    lockReq->requestInfo = AccLockReq::Abort;
    lockReq->accOpPtr = scan.m_accLockOp;
    EXECUTE_DIRECT(DBACC, GSN_ACC_LOCKREQ, signal, AccLockReq::UndoSignalLength);
    jamEntry();
    ndbrequire(lockReq->returnCode == AccLockReq::Success);
    scan.m_accLockOp = RNIL;
    // scan position should already have been moved (assert only)
    if (scan.m_state == ScanOp::Blocked) {
      jam();
      ndbassert(false);
      scan.m_state = ScanOp::Next;
    }
    // LQH has the ball
    return;
  }
  // lose the lock
  scan.m_accLockOp = RNIL;
  // continue at ACC_ABORTCONF
}

/*
 * Received when scan is closing.  This signal arrives after any
 * ACCKEYCON or ACCKEYREF which may have been in job buffer.
 */
void
Dbtux::execACC_ABORTCONF(Signal* signal)
{
  jamEntry();
  ScanOpPtr scanPtr;
  scanPtr.i = signal->theData[0];
  c_scanOpPool.getPtr(scanPtr);
  ScanOp& scan = *scanPtr.p;
#ifdef VM_TRACE
  if (debugFlags & (DebugScan | DebugLock)) {
    debugOut << "ACC_ABORTCONF scan " << scanPtr.i << " " << scan << endl;
  }
#endif
  ndbrequire(scan.m_state == ScanOp::Aborting);
  // most likely we are still in lock wait
  if (scan.m_lockwait) {
    jam();
    scan.m_lockwait = false;
    scan.m_accLockOp = RNIL;
  }
  scanClose(signal, scanPtr);
}

/*
 * Find start position for single range scan.
 */
void
Dbtux::scanFirst(ScanOpPtr scanPtr)
{
  ScanOp& scan = *scanPtr.p;
  Frag& frag = *c_fragPool.getPtr(scan.m_fragPtrI);
#ifdef VM_TRACE
  if (debugFlags & DebugScan) {
    debugOut << "Enter first scan " << scanPtr.i << " " << scan << endl;
  }
#endif
  // set up index keys for this operation
  setKeyAttrs(frag);
  // scan direction 0, 1
  const unsigned idir = scan.m_descending;
  unpackBound(*scan.m_bound[idir], c_dataBuffer);
  TreePos treePos;
  searchToScan(frag, c_dataBuffer, scan.m_boundCnt[idir], scan.m_descending, treePos);
  if (treePos.m_loc != NullTupLoc) {
    scan.m_scanPos = treePos;
    // link the scan to node found
    NodeHandle node(frag);
    selectNode(node, treePos.m_loc);
    linkScan(node, scanPtr);
    if (treePos.m_dir == 3) {
      jam();
      // check upper bound
      TreeEnt ent = node.getEnt(treePos.m_pos);
      if (scanCheck(scanPtr, ent))
        scan.m_state = ScanOp::Current;
      else
        scan.m_state = ScanOp::Last;
    } else {
      scan.m_state = ScanOp::Next;
    }
  } else {
    jam();
    scan.m_state = ScanOp::Last;
  }
#ifdef VM_TRACE
  if (debugFlags & DebugScan) {
    debugOut << "Leave first scan " << scanPtr.i << " " << scan << endl;
  }
#endif
}

/*
 * Look for entry to return as scan result.
 */
void
Dbtux::scanFind(ScanOpPtr scanPtr)
{
  ScanOp& scan = *scanPtr.p;
  Frag& frag = *c_fragPool.getPtr(scan.m_fragPtrI);
#ifdef VM_TRACE
  if (debugFlags & DebugScan) {
    debugOut << "Enter find scan " << scanPtr.i << " " << scan << endl;
  }
#endif
  ndbrequire(scan.m_state == ScanOp::Current || scan.m_state == ScanOp::Next);
  while (1) {
    jam();
    if (scan.m_state == ScanOp::Next)
      scanNext(scanPtr, false);
    if (scan.m_state == ScanOp::Current) {
      jam();
      const TreePos pos = scan.m_scanPos;
      NodeHandle node(frag);
      selectNode(node, pos.m_loc);
      const TreeEnt ent = node.getEnt(pos.m_pos);
      if (scanVisible(scanPtr, ent)) {
        jam();
        scan.m_state = ScanOp::Found;
        scan.m_scanEnt = ent;
        break;
      }
    } else {
      jam();
      break;
    }
    scan.m_state = ScanOp::Next;
  }
#ifdef VM_TRACE
  if (debugFlags & DebugScan) {
    debugOut << "Leave find scan " << scanPtr.i << " " << scan << endl;
  }
#endif
}

/*
 * Move to next entry.  The scan is already linked to some node.  When
 * we leave, if an entry was found, it will be linked to a possibly
 * different node.  The scan has a position, and a direction which tells
 * from where we came to this position.  This is one of (all comments
 * are in terms of ascending scan):
 *
 * 0 - up from left child (scan this node next)
 * 1 - up from right child (proceed to parent)
 * 2 - up from root (the scan ends)
 * 3 - left to right within node (at end proceed to right child)
 * 4 - down from parent (proceed to left child)
 *
 * If an entry was found, scan direction is 3.  Therefore tree
 * re-organizations need not worry about scan direction.
 *
 * This method is also used to move a scan when its entry is removed
 * (see moveScanList).  If the scan is Blocked, we check if it remains
 * Blocked on a different version of the tuple.  Otherwise the tuple is
 * lost and state becomes Current.
 */
void
Dbtux::scanNext(ScanOpPtr scanPtr, bool fromMaintReq)
{
  ScanOp& scan = *scanPtr.p;
  Frag& frag = *c_fragPool.getPtr(scan.m_fragPtrI);
#ifdef VM_TRACE
  if (debugFlags & (DebugMaint | DebugScan)) {
    debugOut << "Enter next scan " << scanPtr.i << " " << scan << endl;
  }
#endif
  // cannot be moved away from tuple we have locked
  ndbrequire(scan.m_state != ScanOp::Locked);
  // set up index keys for this operation
  setKeyAttrs(frag);
  // scan direction
  const unsigned idir = scan.m_descending; // 0, 1
  const int jdir = 1 - 2 * (int)idir;      // 1, -1
  // use copy of position
  TreePos pos = scan.m_scanPos;
  // get and remember original node
  NodeHandle origNode(frag);
  selectNode(origNode, pos.m_loc);
  ndbrequire(islinkScan(origNode, scanPtr));
  // current node in loop
  NodeHandle node = origNode;
  // copy of entry found
  TreeEnt ent;
  while (true) {
    jam();
#ifdef VM_TRACE
    if (debugFlags & (DebugMaint | DebugScan)) {
      debugOut << "Current scan " << scanPtr.i << " pos " << pos << " node " << node << endl;
    }
#endif
    if (pos.m_dir == 2) {
      // coming up from root ends the scan
      jam();
      pos.m_loc = NullTupLoc;
      break;
    }
    if (node.m_loc != pos.m_loc) {
      jam();
      selectNode(node, pos.m_loc);
    }
    if (pos.m_dir == 4) {
      // coming down from parent proceed to left child
      jam();
      TupLoc loc = node.getLink(idir);
      if (loc != NullTupLoc) {
        jam();
        pos.m_loc = loc;
        pos.m_dir = 4;  // unchanged
        continue;
      }
      // pretend we came from left child
      pos.m_dir = idir;
    }
    const unsigned occup = node.getOccup();
    if (occup == 0) {
      jam();
      ndbrequire(fromMaintReq);
      // move back to parent - see comment in treeRemoveInner
      pos.m_loc = node.getLink(2);
      pos.m_dir = node.getSide();
      continue;
    }
    if (pos.m_dir == idir) {
      // coming up from left child scan current node
      jam();
      pos.m_pos = idir == 0 ? (Uint16)-1 : occup;
      pos.m_dir = 3;
    }
    if (pos.m_dir == 3) {
      // before or within node
      jam();
      // advance position - becomes ZNIL (> occup) if 0 and descending
      pos.m_pos += jdir;
      if (pos.m_pos < occup) {
        jam();
        pos.m_dir = 3;  // unchanged
        ent = node.getEnt(pos.m_pos);
        if (! scanCheck(scanPtr, ent)) {
          jam();
          pos.m_loc = NullTupLoc;
        }
        break;
      }
      // after node proceed to right child
      TupLoc loc = node.getLink(1 - idir);
      if (loc != NullTupLoc) {
        jam();
        pos.m_loc = loc;
        pos.m_dir = 4;
        continue;
      }
      // pretend we came from right child
      pos.m_dir = 1 - idir;
    }
    if (pos.m_dir == 1 - idir) {
      // coming up from right child proceed to parent
      jam();
      pos.m_loc = node.getLink(2);
      pos.m_dir = node.getSide();
      continue;
    }
    ndbrequire(false);
  }
  // copy back position
  scan.m_scanPos = pos;
  // relink
  if (pos.m_loc != NullTupLoc) {
    ndbrequire(pos.m_dir == 3);
    ndbrequire(pos.m_loc == node.m_loc);
    if (origNode.m_loc != node.m_loc) {
      jam();
      unlinkScan(origNode, scanPtr);
      linkScan(node, scanPtr);
    }
    if (scan.m_state != ScanOp::Blocked) {
      scan.m_state = ScanOp::Current;
    } else {
      jam();
      ndbrequire(fromMaintReq);
      TreeEnt& scanEnt = scan.m_scanEnt;
      ndbrequire(scanEnt.m_tupLoc != NullTupLoc);
      if (scanEnt.eqtuple(ent)) {
        // remains blocked on another version
        scanEnt = ent;
      } else {
        jam();
        scanEnt.m_tupLoc = NullTupLoc;
        scan.m_state = ScanOp::Current;
      }
    }
  } else {
    jam();
    unlinkScan(origNode, scanPtr);
    scan.m_state = ScanOp::Last;
  }
#ifdef VM_TRACE
  if (debugFlags & (DebugMaint | DebugScan)) {
    debugOut << "Leave next scan " << scanPtr.i << " " << scan << endl;
  }
#endif
}

/*
 * Check end key.  Return true if scan is still within range.
 */
bool
Dbtux::scanCheck(ScanOpPtr scanPtr, TreeEnt ent)
{
  ScanOp& scan = *scanPtr.p;
  Frag& frag = *c_fragPool.getPtr(scan.m_fragPtrI);
  const unsigned idir = scan.m_descending;
  const int jdir = 1 - 2 * (int)idir;
  unpackBound(*scan.m_bound[1 - idir], c_dataBuffer);
  unsigned boundCnt = scan.m_boundCnt[1 - idir];
  readKeyAttrs(frag, ent, 0, c_entryKey);
  int ret = cmpScanBound(frag, 1 - idir, c_dataBuffer, boundCnt, c_entryKey);
  ndbrequire(ret != NdbSqlUtil::CmpUnknown);
  if (jdir * ret > 0)
    return true;
  // hit upper bound of single range scan
  return false;
}

/*
 * Check if an entry is visible to the scan.
 *
 * There is a special check to never accept same tuple twice in a row.
 * This is faster than asking TUP.  It also fixes some special cases
 * which are not analyzed or handled yet.
 */
bool
Dbtux::scanVisible(ScanOpPtr scanPtr, TreeEnt ent)
{
  const ScanOp& scan = *scanPtr.p;
  const Frag& frag = *c_fragPool.getPtr(scan.m_fragPtrI);
  Uint32 tableFragPtrI = frag.m_tupTableFragPtrI;
  Uint32 pageId = ent.m_tupLoc.getPageId();
  Uint32 pageOffset = ent.m_tupLoc.getPageOffset();
  Uint32 tupVersion = ent.m_tupVersion;
  // check for same tuple twice in row
  if (scan.m_scanEnt.m_tupLoc == ent.m_tupLoc)
  {
    jam();
    return false;
  }
  Uint32 transId1 = scan.m_transId1;
  Uint32 transId2 = scan.m_transId2;
  bool dirty = scan.m_readCommitted;
  Uint32 savePointId = scan.m_savePointId;
  bool ret = c_tup->tuxQueryTh(tableFragPtrI, pageId, pageOffset, tupVersion, transId1, transId2, dirty, savePointId);
  jamEntry();
  return ret;
}

/*
 * Finish closing of scan and send conf.  Any lock wait has been done
 * already.
 */
void
Dbtux::scanClose(Signal* signal, ScanOpPtr scanPtr)
{
  ScanOp& scan = *scanPtr.p;
  ndbrequire(! scan.m_lockwait && scan.m_accLockOp == RNIL);
  // unlock all not unlocked by LQH
  if (! scan.m_accLockOps.isEmpty()) {
    jam();
    abortAccLockOps(signal, scanPtr);
  }
  // send conf
  NextScanConf* const conf = (NextScanConf*)signal->getDataPtrSend();
  conf->scanPtr = scanPtr.p->m_userPtr;
  conf->accOperationPtr = RNIL;
  conf->fragId = RNIL;
  unsigned signalLength = 3;
  sendSignal(scanPtr.p->m_userRef, GSN_NEXT_SCANCONF,
      signal, signalLength, JBB);
  releaseScanOp(scanPtr);
}

void
Dbtux::abortAccLockOps(Signal* signal, ScanOpPtr scanPtr)
{
  ScanOp& scan = *scanPtr.p;
#ifdef VM_TRACE
  if (debugFlags & (DebugScan | DebugLock)) {
    debugOut << "Abort locks in scan " << scanPtr.i << " " << scan << endl;
  }
#endif
  LocalDLFifoList<ScanLock> list(c_scanLockPool, scan.m_accLockOps);
  ScanLockPtr lockPtr;
  while (list.first(lockPtr)) {
    jam();
    AccLockReq* const lockReq = (AccLockReq*)signal->getDataPtrSend();
    lockReq->returnCode = RNIL;
    lockReq->requestInfo = AccLockReq::Abort;
    lockReq->accOpPtr = lockPtr.p->m_accLockOp;
    EXECUTE_DIRECT(DBACC, GSN_ACC_LOCKREQ, signal, AccLockReq::UndoSignalLength);
    jamEntry();
    ndbrequire(lockReq->returnCode == AccLockReq::Success);
    list.release(lockPtr);
  }
}

void
Dbtux::addAccLockOp(ScanOpPtr scanPtr, Uint32 accLockOp)
{
  ScanOp& scan = *scanPtr.p;
#ifdef VM_TRACE
  if (debugFlags & (DebugScan | DebugLock)) {
    debugOut << "Add lock " << hex << accLockOp << dec
             << " to scan " << scanPtr.i << " " << scan << endl;
  }
#endif
  LocalDLFifoList<ScanLock> list(c_scanLockPool, scan.m_accLockOps);
  ScanLockPtr lockPtr;
#ifdef VM_TRACE
  list.first(lockPtr);
  while (lockPtr.i != RNIL) {
    ndbrequire(lockPtr.p->m_accLockOp != accLockOp);
    list.next(lockPtr);
  }
#endif
  bool ok = list.seize(lockPtr);
  ndbrequire(ok);
  ndbrequire(accLockOp != RNIL);
  lockPtr.p->m_accLockOp = accLockOp;
}

void
Dbtux::removeAccLockOp(ScanOpPtr scanPtr, Uint32 accLockOp)
{
  ScanOp& scan = *scanPtr.p;
#ifdef VM_TRACE
  if (debugFlags & (DebugScan | DebugLock)) {
    debugOut << "Remove lock " << hex << accLockOp << dec
             << " from scan " << scanPtr.i << " " << scan << endl;
  }
#endif
  LocalDLFifoList<ScanLock> list(c_scanLockPool, scan.m_accLockOps);
  ScanLockPtr lockPtr;
  list.first(lockPtr);
  while (lockPtr.i != RNIL) {
    if (lockPtr.p->m_accLockOp == accLockOp) {
      jam();
      break;
    }
    list.next(lockPtr);
  }
  ndbrequire(lockPtr.i != RNIL);
  list.release(lockPtr);
}

/*
 * Release allocated records.
 */
void
Dbtux::releaseScanOp(ScanOpPtr& scanPtr)
{
#ifdef VM_TRACE
  if (debugFlags & DebugScan) {
    debugOut << "Release scan " << scanPtr.i << " " << *scanPtr.p << endl;
  }
#endif
  Frag& frag = *c_fragPool.getPtr(scanPtr.p->m_fragPtrI);
  scanPtr.p->m_boundMin.release();
  scanPtr.p->m_boundMax.release();
  // unlink from per-fragment list and release from pool
  frag.m_scanList.release(scanPtr);
}
