/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#define DBTUX_SCAN_CPP
#include "Dbtux.hpp"

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
    ndbrequire(frag.m_fragId < (1 << frag.m_fragOff));
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
    scanPtr.p->m_keyInfo = AccScanReq::getKeyinfoFlag(req->requestInfo);
#ifdef VM_TRACE
    if (debugFlags & DebugScan) {
      debugOut << "Seize scan " << scanPtr.i << " " << *scanPtr.p << endl;
    }
#endif
    /*
     * readCommitted lockMode keyInfo
     * 1 0 0 - read committed (no lock)
     * 0 0 0 - read latest (read lock)
     * 0 1 1 - read exclusive (write lock)
     */
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
 * end key).  Full bound type (< 4) is included but only the strict bit
 * is used since lower and upper have now been separated.
 */
void
Dbtux::execTUX_BOUND_INFO(Signal* signal)
{
  jamEntry();
  struct BoundInfo {
    int type;
    unsigned offset;
    unsigned size;
  };
  TuxBoundInfo* const sig = (TuxBoundInfo*)signal->getDataPtrSend();
  const TuxBoundInfo reqCopy = *(const TuxBoundInfo*)sig;
  const TuxBoundInfo* const req = &reqCopy;
  // get records
  ScanOp& scan = *c_scanOpPool.getPtr(req->tuxScanPtrI);
  Index& index = *c_indexPool.getPtr(scan.m_indexId);
  // collect lower and upper bounds
  BoundInfo boundInfo[2][MaxIndexAttributes];
  // largest attrId seen plus one
  Uint32 maxAttrId[2] = { 0, 0 };
  unsigned offset = 0;
  const Uint32* const data = (Uint32*)sig + TuxBoundInfo::SignalLength;
  // walk through entries
  while (offset + 2 <= req->boundAiLength) {
    jam();
    const unsigned type = data[offset];
    if (type > 4) {
      jam();
      scan.m_state = ScanOp::Invalid;
      sig->errorCode = TuxBoundInfo::InvalidAttrInfo;
      return;
    }
    const AttributeHeader* ah = (const AttributeHeader*)&data[offset + 1];
    const Uint32 attrId = ah->getAttributeId();
    const Uint32 dataSize = ah->getDataSize();
    if (attrId >= index.m_numAttrs) {
      jam();
      scan.m_state = ScanOp::Invalid;
      sig->errorCode = TuxBoundInfo::InvalidAttrInfo;
      return;
    }
    for (unsigned j = 0; j <= 1; j++) {
      // check if lower/upper bit matches
      const unsigned luBit = (j << 1);
      if ((type & 0x2) != luBit && type != 4)
        continue;
      // EQ -> LE, GE
      const unsigned type2 = (type & 0x1) | luBit;
      // fill in any gap
      while (maxAttrId[j] <= attrId) {
        BoundInfo& b = boundInfo[j][maxAttrId[j]++];
        b.type = -1;
      }
      BoundInfo& b = boundInfo[j][attrId];
      if (b.type != -1) {
        // compare with previous bound
        if (b.type != type2 ||
            b.size != 2 + dataSize ||
            memcmp(&data[b.offset + 2], &data[offset + 2], dataSize << 2) != 0) {
          jam();
          scan.m_state = ScanOp::Invalid;
          sig->errorCode = TuxBoundInfo::InvalidBounds;
          return;
        }
      } else {
        // enter new bound
        b.type = type2;
        b.offset = offset;
        b.size = 2 + dataSize;
      }
    }
    // jump to next
    offset += 2 + dataSize;
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
      if (b.type == -1 || (i + 1 < maxAttrId[j] && (b.type & 0x1))) {
        jam();
        scan.m_state = ScanOp::Invalid;
        sig->errorCode = TuxBoundInfo::InvalidBounds;
        return;
      }
      bool ok = scan.m_bound[j]->append(&data[b.offset], b.size);
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
      removeAccLockOp(scan, req->accOperationPtr);
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
      selectNode(signal, node, loc, AccHead);
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
      EXECUTE_DIRECT(DBACC, GSN_ACC_LOCKREQ, signal, AccLockReq::UndoSignalLength);
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
      lockReq->requestInfo = AccLockReq::Unlock;
      lockReq->accOpPtr = scan.m_accLockOp;
      EXECUTE_DIRECT(DBACC, GSN_ACC_LOCKREQ, signal, AccLockReq::UndoSignalLength);
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
    conf->fragId = frag.m_fragId | (ent.m_fragBit << frag.m_fragOff);
    unsigned signalLength = 3;
    // if TC has ordered scan close, it will be detected here
    sendSignal(scan.m_userRef, GSN_NEXT_SCANCONF,
        signal, signalLength, JBB);
    return;     // stop
  }
  if (scan.m_state == ScanOp::First) {
    jam();
    // search is done only once in single range scan
    scanFirst(signal, scanPtr);
#ifdef VM_TRACE
    if (debugFlags & DebugScan) {
      debugOut << "First scan " << scanPtr.i << " " << scan << endl;
    }
#endif
  }
  if (scan.m_state == ScanOp::Next) {
    jam();
    // look for next
    scanNext(signal, scanPtr);
  }
  // for reading tuple key in Current or Locked state
  Data pkData = c_dataBuffer;
  unsigned pkSize = 0; // indicates not yet done
  if (scan.m_state == ScanOp::Current) {
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
      lockReq->fragId = frag.m_fragId | (ent.m_fragBit << frag.m_fragOff);
      lockReq->fragPtrI = frag.m_accTableFragPtrI[ent.m_fragBit];
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
        if (debugFlags & DebugScan) {
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
        if (debugFlags & DebugScan) {
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
        // stay in Current state
        scan.m_state = ScanOp::Current;
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
    if (scan.m_keyInfo) {
      jam();
      if (pkSize == 0) {
        jam();
        readTablePk(frag, ent, pkData, pkSize);
      }
    }
    // conf signal
    NextScanConf* const conf = (NextScanConf*)signal->getDataPtrSend();
    conf->scanPtr = scan.m_userPtr;
    // the lock is passed to LQH
    Uint32 accLockOp = scan.m_accLockOp;
    if (accLockOp != RNIL) {
      scan.m_accLockOp = RNIL;
      // remember it until LQH unlocks it
      addAccLockOp(scan, accLockOp);
    } else {
      ndbrequire(scan.m_readCommitted);
      // operation RNIL in LQH would signal no tuple returned
      accLockOp = (Uint32)-1;
    }
    conf->accOperationPtr = accLockOp;
    conf->fragId = frag.m_fragId | (ent.m_fragBit << frag.m_fragOff);
    conf->localKey[0] = getTupAddr(frag, ent);
    conf->localKey[1] = 0;
    conf->localKeyLength = 1;
    unsigned signalLength = 6;
    // add key info
    if (scan.m_keyInfo) {
      jam();
      conf->keyLength = pkSize;
      // piggy-back first 4 words of key data
      for (unsigned i = 0; i < 4; i++) {
        conf->key[i] = i < pkSize ? pkData[i] : 0;
      }
      signalLength = 11;
    }
    if (! scan.m_readCommitted) {
      sendSignal(scan.m_userRef, GSN_NEXT_SCANCONF,
          signal, signalLength, JBB);
    } else {
      Uint32 blockNo = refToBlock(scan.m_userRef);
      EXECUTE_DIRECT(blockNo, GSN_NEXT_SCANCONF, signal, signalLength);
    }
    // send rest of key data
    if (scan.m_keyInfo && pkSize > 4) {
      unsigned total = 4;
      while (total < pkSize) {
        jam();
        unsigned length = pkSize - total;
        if (length > 20)
          length = 20;
        signal->theData[0] = scan.m_userPtr;
        signal->theData[1] = 0;
        signal->theData[2] = 0;
        signal->theData[3] = length;
        memcpy(&signal->theData[4], &pkData[total], length << 2);
        sendSignal(scan.m_userRef, GSN_ACC_SCAN_INFO24,
            signal, 4 + length, JBB);
        total += length;
      }
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
  if (debugFlags & DebugScan) {
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
    lockReq->requestInfo = AccLockReq::Unlock;
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
  if (debugFlags & DebugScan) {
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
  if (debugFlags & DebugScan) {
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
 * Find start position for single range scan.  If it exists, sets state
 * to Next and links the scan to the node.  The first entry is returned
 * by scanNext.
 */
void
Dbtux::scanFirst(Signal* signal, ScanOpPtr scanPtr)
{
  ScanOp& scan = *scanPtr.p;
  Frag& frag = *c_fragPool.getPtr(scan.m_fragPtrI);
  TreeHead& tree = frag.m_tree;
  // set up index keys for this operation
  setKeyAttrs(frag);
  // unpack lower bound into c_dataBuffer
  const ScanBound& bound = *scan.m_bound[0];
  ScanBoundIterator iter;
  bound.first(iter);
  for (unsigned j = 0; j < bound.getSize(); j++) {
    jam();
    c_dataBuffer[j] = *iter.data;
    bound.next(iter);
  }
  // search for scan start position
  TreePos treePos;
  searchToScan(signal, frag, c_dataBuffer, scan.m_boundCnt[0], treePos);
  if (treePos.m_loc == NullTupLoc) {
    // empty tree
    jam();
    scan.m_state = ScanOp::Last;
    return;
  }
  // set position and state
  scan.m_scanPos = treePos;
  scan.m_state = ScanOp::Next;
  // link the scan to node found
  NodeHandle node(frag);
  selectNode(signal, node, treePos.m_loc, AccFull);
  linkScan(node, scanPtr);
}

/*
 * Move to next entry.  The scan is already linked to some node.  When
 * we leave, if any entry was found, it will be linked to a possibly
 * different node.  The scan has a position, and a direction which tells
 * from where we came to this position.  This is one of:
 *
 * 0 - up from left child (scan this node next)
 * 1 - up from right child (proceed to parent)
 * 2 - up from root (the scan ends)
 * 3 - left to right within node (at end proceed to right child)
 * 4 - down from parent (proceed to left child)
 */
void
Dbtux::scanNext(Signal* signal, ScanOpPtr scanPtr)
{
  ScanOp& scan = *scanPtr.p;
  Frag& frag = *c_fragPool.getPtr(scan.m_fragPtrI);
#ifdef VM_TRACE
  if (debugFlags & DebugScan) {
    debugOut << "Next in scan " << scanPtr.i << " " << scan << endl;
  }
#endif
  if (scan.m_state == ScanOp::Locked) {
    jam();
    // version of a tuple locked by us cannot disappear (assert only)
#ifdef dbtux_wl_1942_is_done
    ndbassert(false);
#endif
    AccLockReq* const lockReq = (AccLockReq*)signal->getDataPtrSend();
    lockReq->returnCode = RNIL;
    lockReq->requestInfo = AccLockReq::Unlock;
    lockReq->accOpPtr = scan.m_accLockOp;
    EXECUTE_DIRECT(DBACC, GSN_ACC_LOCKREQ, signal, AccLockReq::UndoSignalLength);
    jamEntry();
    ndbrequire(lockReq->returnCode == AccLockReq::Success);
    scan.m_accLockOp = RNIL;
    scan.m_state = ScanOp::Current;
  }
  // set up index keys for this operation
  setKeyAttrs(frag);
  // unpack upper bound into c_dataBuffer
  const ScanBound& bound = *scan.m_bound[1];
  ScanBoundIterator iter;
  bound.first(iter);
  for (unsigned j = 0; j < bound.getSize(); j++) {
    jam();
    c_dataBuffer[j] = *iter.data;
    bound.next(iter);
  }
  // use copy of position
  TreePos pos = scan.m_scanPos;
  // get and remember original node
  NodeHandle origNode(frag);
  selectNode(signal, origNode, pos.m_loc, AccHead);
  ndbrequire(islinkScan(origNode, scanPtr));
  // current node in loop
  NodeHandle node = origNode;
  // copy of entry found
  TreeEnt ent;
  while (true) {
    jam();
    if (pos.m_dir == 2) {
      // coming up from root ends the scan
      jam();
      pos.m_loc = NullTupLoc;
      scan.m_state = ScanOp::Last;
      break;
    }
    if (node.m_loc != pos.m_loc) {
      jam();
      selectNode(signal, node, pos.m_loc, AccHead);
    }
    if (pos.m_dir == 4) {
      // coming down from parent proceed to left child
      jam();
      TupLoc loc = node.getLink(0);
      if (loc != NullTupLoc) {
        jam();
        pos.m_loc = loc;
        pos.m_dir = 4;  // unchanged
        continue;
      }
      // pretend we came from left child
      pos.m_dir = 0;
    }
    if (pos.m_dir == 0) {
      // coming up from left child scan current node
      jam();
      pos.m_pos = 0;
      pos.m_match = false;
      pos.m_dir = 3;
    }
    if (pos.m_dir == 3) {
      // within node
      jam();
      unsigned occup = node.getOccup();
      ndbrequire(occup >= 1);
      // access full node
      accessNode(signal, node, AccFull);
      // advance position
      if (! pos.m_match)
        pos.m_match = true;
      else
        pos.m_pos++;
      if (pos.m_pos < occup) {
        jam();
        ent = node.getEnt(pos.m_pos);
        pos.m_dir = 3;  // unchanged
        // read and compare all attributes
        readKeyAttrs(frag, ent, 0, c_entryKey);
        int ret = cmpScanBound(frag, 1, c_dataBuffer, scan.m_boundCnt[1], c_entryKey);
        ndbrequire(ret != NdbSqlUtil::CmpUnknown);
        if (ret < 0) {
          jam();
          // hit upper bound of single range scan
          pos.m_loc = NullTupLoc;
          scan.m_state = ScanOp::Last;
          break;
        }
        // can we see it
        if (! scanVisible(signal, scanPtr, ent)) {
          jam();
          continue;
        }
        // found entry
        scan.m_state = ScanOp::Current;
        break;
      }
      // after node proceed to right child
      TupLoc loc = node.getLink(1);
      if (loc != NullTupLoc) {
        jam();
        pos.m_loc = loc;
        pos.m_dir = 4;
        continue;
      }
      // pretend we came from right child
      pos.m_dir = 1;
    }
    if (pos.m_dir == 1) {
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
  if (scan.m_state == ScanOp::Current) {
    ndbrequire(pos.m_loc == node.m_loc);
    if (origNode.m_loc != node.m_loc) {
      jam();
      unlinkScan(origNode, scanPtr);
      linkScan(node, scanPtr);
    }
    // copy found entry
    scan.m_scanEnt = ent;
  } else if (scan.m_state == ScanOp::Last) {
    jam();
    ndbrequire(pos.m_loc == NullTupLoc);
    unlinkScan(origNode, scanPtr);
  } else {
    ndbrequire(false);
  }
#ifdef VM_TRACE
  if (debugFlags & DebugScan) {
    debugOut << "Next out scan " << scanPtr.i << " " << scan << endl;
  }
#endif
}

/*
 * Check if an entry is visible to the scan.
 *
 * There is a special check to never accept same tuple twice in a row.
 * This is faster than asking TUP.  It also fixes some special cases
 * which are not analyzed or handled yet.
 */
bool
Dbtux::scanVisible(Signal* signal, ScanOpPtr scanPtr, TreeEnt ent)
{
  const ScanOp& scan = *scanPtr.p;
  const Frag& frag = *c_fragPool.getPtr(scan.m_fragPtrI);
  Uint32 fragBit = ent.m_fragBit;
  Uint32 tableFragPtrI = frag.m_tupTableFragPtrI[fragBit];
  Uint32 fragId = frag.m_fragId | (fragBit << frag.m_fragOff);
  Uint32 tupAddr = getTupAddr(frag, ent);
  Uint32 tupVersion = ent.m_tupVersion;
  // check for same tuple twice in row
  if (scan.m_scanEnt.m_tupLoc == ent.m_tupLoc &&
      scan.m_scanEnt.m_fragBit == fragBit) {
    jam();
    return false;
  }
  Uint32 transId1 = scan.m_transId1;
  Uint32 transId2 = scan.m_transId2;
  Uint32 savePointId = scan.m_savePointId;
  bool ret = c_tup->tuxQueryTh(tableFragPtrI, tupAddr, tupVersion, transId1, transId2, savePointId);
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
  for (unsigned i = 0; i < MaxAccLockOps; i++) {
    if (scan.m_accLockOps[i] != RNIL) {
      jam();
      AccLockReq* const lockReq = (AccLockReq*)signal->getDataPtrSend();
      lockReq->returnCode = RNIL;
      lockReq->requestInfo = AccLockReq::Abort;
      lockReq->accOpPtr = scan.m_accLockOps[i];
      EXECUTE_DIRECT(DBACC, GSN_ACC_LOCKREQ, signal, AccLockReq::UndoSignalLength);
      jamEntry();
      ndbrequire(lockReq->returnCode == AccLockReq::Success);
      scan.m_accLockOps[i] = RNIL;
    }
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
Dbtux::addAccLockOp(ScanOp& scan, Uint32 accLockOp)
{
  ndbrequire(accLockOp != RNIL);
  Uint32* list = scan.m_accLockOps;
  bool ok = false;
  for (unsigned i = 0; i < MaxAccLockOps; i++) {
    ndbrequire(list[i] != accLockOp);
    if (! ok && list[i] == RNIL) {
      list[i] = accLockOp;
      ok = true;
      // continue check for duplicates
    }
  }
  ndbrequire(ok);
}

void
Dbtux::removeAccLockOp(ScanOp& scan, Uint32 accLockOp)
{
  ndbrequire(accLockOp != RNIL);
  Uint32* list = scan.m_accLockOps;
  bool ok = false;
  for (unsigned i = 0; i < MaxAccLockOps; i++) {
    if (list[i] == accLockOp) {
      list[i] = RNIL;
      ok = true;
      break;
    }
  }
  ndbrequire(ok);
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
