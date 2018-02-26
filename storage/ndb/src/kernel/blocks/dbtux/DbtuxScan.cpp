/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#define DBTUX_SCAN_CPP
#include "Dbtux.hpp"
#include <my_sys.h>

#define JAM_FILE_ID 371

void
Dbtux::execACC_CHECK_SCAN(Signal* signal)
{
  jamEntryDebug();
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
  if (req->checkLcpStop == AccCheckScan::ZCHECK_LCP_STOP &&
      scan.m_lockwait)
  {
    /**
     * Go to sleep for one millisecond if we encounter a locked row.
     */
    jam();
    CheckLcpStop* cls = (CheckLcpStop*) signal->theData;
    cls->scanPtrI = scan.m_userPtr;
    cls->scanState = CheckLcpStop::ZSCAN_RESOURCE_WAIT;
    EXECUTE_DIRECT(DBLQH, GSN_CHECK_LCP_STOP, signal, 2);
    jamEntry();
    ndbassert(signal->theData[0] == CheckLcpStop::ZTAKE_A_BREAK);
    return;   // stop
  }
  continue_scan(signal, scanPtr, frag);
}

/*
 * Error handling:  Any seized scan op is released.  ACC_SCANREF is sent
 * to LQH.  LQH sets error code, and treats this like ZEMPTY_FRAGMENT.
 * Therefore scan is now closed on both sides.
 */
void
Dbtux::execACC_SCANREQ(Signal* signal)
{
  jamEntry();
  const AccScanReq reqCopy = *(const AccScanReq*)signal->getDataPtr();
  const AccScanReq* const req = &reqCopy;
  Uint32 errorCode = 0;
  ScanOpPtr scanPtr;
  scanPtr.i = RNIL;
  do {
    // get the index
    IndexPtr indexPtr;
    c_indexPool.getPtr(indexPtr, req->tableId);
    // get the fragment
    FragPtr fragPtr;
    findFrag(jamBuffer(), *indexPtr.p, req->fragmentNo, fragPtr);
    ndbrequire(fragPtr.i != RNIL);
    Frag& frag = *fragPtr.p;
    // check for index not Online (i.e. Dropping)
    if (unlikely(indexPtr.p->m_state != Index::Online)) {
      jam();
#ifdef VM_TRACE
      if (debugFlags & (DebugMeta | DebugScan)) {
        debugOut << "Index dropping at ACC_SCANREQ " << indexPtr.i << " " << *indexPtr.p << endl;
      }
#endif
      errorCode = AccScanRef::TuxIndexNotOnline;
      break;
    }
    // must be normal DIH/TC fragment
    TreeHead& tree = frag.m_tree;
    // check for empty fragment
    if (tree.m_root == NullTupLoc) {
      jam();
      AccScanConf* const conf = (AccScanConf*)signal->getDataPtrSend();
      conf->scanPtr = req->senderData;
      conf->accPtr = RNIL;
      conf->flag = AccScanConf::ZEMPTY_FRAGMENT;
      signal->theData[8] = 0;
      /* Return ACC_SCANCONF */
      return;
    }
    // seize from pool and link to per-fragment list
    if (ERROR_INSERTED(12008) ||
        ! frag.m_scanList.seizeFirst(scanPtr)) {
      CLEAR_ERROR_INSERT_VALUE;
      jam();
      // should never happen but can be used to test error handling
      errorCode = AccScanRef::TuxNoFreeScanOp;
      break;
    }
    new (scanPtr.p) ScanOp;
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
    const bool isStatScan = AccScanReq::getStatScanFlag(req->requestInfo);
    if (unlikely(isStatScan)) {
      jam();
      if (!scanPtr.p->m_readCommitted) {
        jam();
        errorCode = AccScanRef::TuxInvalidLockMode;
        break;
      }
      StatOpPtr statPtr;
      if (!c_statOpPool.seize(statPtr)) {
        jam();
        errorCode = AccScanRef::TuxNoFreeStatOp;
        break;
      }
      scanPtr.p->m_statOpPtrI = statPtr.i;
      new (statPtr.p) StatOp(*indexPtr.p);
      statPtr.p->m_scanOpPtrI = scanPtr.i;
      // rest of StatOp is initialized in execTUX_BOUND_INFO
#ifdef VM_TRACE
      if (debugFlags & DebugStat) {
        debugOut << "Seize stat op" << endl;
      }
#endif
    }
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
    signal->theData[8] = 0;
    /* Return ACC_SCANCONF */
    return;
  } while (0);
  if (scanPtr.i != RNIL) {
    jam();
    releaseScanOp(scanPtr);
  }
  // ref
  ndbrequire(errorCode != 0);
  AccScanRef* ref = (AccScanRef*)signal->getDataPtrSend();
  ref->scanPtr = req->senderData;
  ref->accPtr = RNIL;
  ref->errorCode = errorCode;
  signal->theData[8] = 1;
  /* Return ACC_SCANREF */
}

/*
 * Receive bounds for scan in single direct call.  The bounds can arrive
 * in any order.  Attribute ids are those of index table.
 *
 * Replace EQ by equivalent LE + GE.  Check for conflicting bounds.
 * Check that sets of lower and upper bounds are on initial sequences of
 * keys and that all but possibly last bound is non-strict.
 *
 * Finally convert the sets of lower and upper bounds (i.e. start key
 * and end key) to NdbPack format.  The data is saved in segmented
 * memory.  The bound is reconstructed at use time via unpackBound().
 *
 * Error handling:  Error code is set in the scan and also returned in
 * EXECUTE_DIRECT (the old way).
 */
void
Dbtux::execTUX_BOUND_INFO(Signal* signal)
{
  jamEntry();
  // get records
  TuxBoundInfo* const req = (TuxBoundInfo*)signal->getDataPtrSend();
  ScanOpPtr scanPtr;
  scanPtr.i = req->tuxScanPtrI;
  c_scanOpPool.getPtr(scanPtr);
  ScanOp& scan = *scanPtr.p;
  const Index& index = *c_indexPool.getPtr(scan.m_indexId);
  // compiler warning unused: const DescHead& descHead = getDescHead(index);
  // compiler warning unused: const KeyType* keyTypes = getKeyTypes(descHead);
  // data passed in Signal
  const Uint32* const boundData = &req->data[0];
  Uint32 boundLen = req->boundAiLength;
  Uint32 boundOffset = 0;
  // initialize stats scan
  if (unlikely(scan.m_statOpPtrI != RNIL)) {
    // stats options before bounds
    StatOpPtr statPtr;
    statPtr.i = scan.m_statOpPtrI;
    c_statOpPool.getPtr(statPtr);
    Uint32 usedLen = 0;
    if (statScanInit(statPtr, boundData, boundLen, &usedLen) == -1) {
      jam();
      ndbrequire(scan.m_errorCode != 0);
      req->errorCode = scan.m_errorCode;
      return;
    }
    ndbrequire(usedLen <= boundLen);
    boundLen -= usedLen;
    boundOffset += usedLen;
  }
  // extract lower and upper bound in separate passes
  for (unsigned idir = 0; idir <= 1; idir++) {
    jam();
    struct BoundInfo {
      int type2;      // with EQ -> LE/GE
      Uint32 offset;  // word offset in signal data
      Uint32 bytes;
    };
    BoundInfo boundInfo[MaxIndexAttributes];
    // largest attrId seen plus one
    Uint32 maxAttrId = 0;
    const Uint32* const data = &boundData[boundOffset];
    Uint32 offset = 0;
    while (offset + 2 <= boundLen) {
      jam();
      const Uint32 type = data[offset];
      const AttributeHeader* ah = (const AttributeHeader*)&data[offset + 1];
      const Uint32 attrId = ah->getAttributeId();
      const Uint32 byteSize = ah->getByteSize();
      const Uint32 dataSize = ah->getDataSize();
      // check type
      if (unlikely(type > 4)) {
        jam();
        scan.m_errorCode = TuxBoundInfo::InvalidAttrInfo;
        req->errorCode = scan.m_errorCode;
        return;
      }
      Uint32 type2 = type;
      if (type2 == 4) {
        jam();
        type2 = (idir << 1); // LE=0 GE=2
      }
      // check if attribute belongs to this bound
      if ((type2 & 0x2) == (idir << 1)) {
        if (unlikely(attrId >= index.m_numAttrs)) {
          jam();
          scan.m_errorCode = TuxBoundInfo::InvalidAttrInfo;
          req->errorCode = scan.m_errorCode;
          return;
        }
        // mark entries in any gap as undefined
        while (maxAttrId <= attrId) {
          jam();
          BoundInfo& b = boundInfo[maxAttrId];
          b.type2 = -1;
          maxAttrId++;
        }
        BoundInfo& b = boundInfo[attrId];
        // duplicate no longer allowed (wl#4163)
        if (unlikely(b.type2 != -1)) {
          jam();
          scan.m_errorCode = TuxBoundInfo::InvalidBounds;
          req->errorCode = scan.m_errorCode;
          return;
        }
        b.type2 = (int)type2;
        b.offset = offset + 1; // poai
        b.bytes = byteSize;
      }
      // jump to next
      offset += 2 + dataSize;
    }
    if (unlikely(offset != boundLen)) {
      jam();
      scan.m_errorCode = TuxBoundInfo::InvalidAttrInfo;
      req->errorCode = scan.m_errorCode;
      return;
    }
    // check and pack the bound data
    KeyData searchBoundData(index.m_keySpec, true, 0);
    KeyBound searchBound(searchBoundData);
    searchBoundData.set_buf(c_ctx.c_searchKey, MaxAttrDataSize << 2);
    int strict = 0; // 0 or 1
    Uint32 i;
    for (i = 0; i < maxAttrId; i++) {
      jam();
      const BoundInfo& b = boundInfo[i];
       // check for gap or strict bound before last
       strict = (b.type2 & 0x1);
       if (unlikely(b.type2 == -1 || (i + 1 < maxAttrId && strict))) {
         jam();
         scan.m_errorCode = TuxBoundInfo::InvalidBounds;
         req->errorCode = scan.m_errorCode;
         return;
       }
       Uint32 len;
       if (unlikely(searchBoundData.add_poai(&data[b.offset], &len) == -1 ||
           b.bytes != len)) {
         jam();
         scan.m_errorCode = TuxBoundInfo::InvalidCharFormat;
         req->errorCode = scan.m_errorCode;
         return;
       }
    }
    int side = 0;
    if (maxAttrId != 0) {
      // arithmetic is faster
      // side = (idir == 0 ? (strict ? +1 : -1) : (strict ? -1 : +1));
      side = (-1) * (1 - 2 * strict) * (1 - 2 * int(idir));
    }
    if (unlikely(searchBound.finalize(side) == -1)) {
      jam();
      scan.m_errorCode = TuxBoundInfo::InvalidCharFormat;
      req->errorCode = scan.m_errorCode;
      return;
    }
    ScanBound& scanBound = scan.m_scanBound[idir];
    scanBound.m_cnt = maxAttrId;
    scanBound.m_side = side;
    // save data words in segmented memory
    {
      ScanBoundBuffer::Head& head = scanBound.m_head;
      LocalScanBoundBuffer b(c_scanBoundPool, head);
      const Uint32* data = (const Uint32*)searchBoundData.get_data_buf();
      Uint32 size = (searchBoundData.get_data_len() + 3) / 4;
      bool ok = b.append(data, size);
      if (unlikely(!ok)) {
        jam();
        scan.m_errorCode = TuxBoundInfo::OutOfBuffers;
        req->errorCode = scan.m_errorCode;
        return;
      }
    }
  }
  if (ERROR_INSERTED(12009)) {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    scan.m_errorCode = TuxBoundInfo::InvalidBounds;
    req->errorCode = scan.m_errorCode;
    return;
  }
  // no error
  req->errorCode = 0;
}

void
Dbtux::execNEXT_SCANREQ(Signal* signal)
{
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
    jamDebug();
    break;
  case NextScanReq::ZSCAN_COMMIT:
    jamDebug();
  case NextScanReq::ZSCAN_NEXT_COMMIT:
    jamDebug();
    if (! scan.m_readCommitted)
    {
      jam();
      AccLockReq* const lockReq = (AccLockReq*)signal->getDataPtrSend();
      lockReq->returnCode = RNIL;
      lockReq->requestInfo = AccLockReq::Unlock;
      lockReq->accOpPtr = req->accOperationPtr;
      EXECUTE_DIRECT(DBACC, GSN_ACC_LOCKREQ, signal, AccLockReq::UndoSignalLength);
      jamEntryDebug();
      ndbrequire(lockReq->returnCode == AccLockReq::Success);
      removeAccLockOp(scanPtr, req->accOperationPtr);
    }
    if (req->scanFlag == NextScanReq::ZSCAN_COMMIT)
    {
      jamDebug();
      signal->theData[0] = 0; /* Success */
      /**
       * Return with signal->theData[0] = 0 means a return
       * signal NEXT_SCANCONF for NextScanReq::ZSCAN_COMMIT
       */
      return;
    }
    break;
  case NextScanReq::ZSCAN_CLOSE:
    jamDebug();
    // unlink from tree node first to avoid state changes
    if (scan.m_scanPos.m_loc != NullTupLoc)
    {
      jam();
      const TupLoc loc = scan.m_scanPos.m_loc;
      NodeHandle node(frag);
      selectNode(node, loc);
      unlinkScan(node, scanPtr);
      scan.m_scanPos.m_loc = NullTupLoc;
    }
    if (unlikely(scan.m_lockwait))
    {
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
    if (scan.m_state == ScanOp::Locked)
    {
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
  continue_scan(signal, scanPtr, frag);
}

void
Dbtux::continue_scan(Signal *signal,
                     ScanOpPtr scanPtr,
                     Frag& frag)
{
  ScanOp& scan = *scanPtr.p;
#ifdef VM_TRACE
  if (debugFlags & DebugScan) {
    debugOut << "ACC_CHECK_SCAN scan " << scanPtr.i << " " << scan << endl;
  }
#endif
  if (unlikely(scan.m_lockwait))
  {
    jam();
    // LQH asks if we are waiting for lock and we tell it to ask again
    NextScanConf* const conf = (NextScanConf*)signal->getDataPtrSend();
    conf->scanPtr = scan.m_userPtr;
    conf->accOperationPtr = RNIL;       // no tuple returned
    conf->fragId = frag.m_fragId;
    // if TC has ordered scan close, it will be detected here
    sendSignal(scan.m_userRef,
               GSN_NEXT_SCANCONF,
               signal,
               NextScanConf::SignalLengthNoTuple,
               JBB);
    return;     // stop
  }
  // check index online
  const Index& index = *c_indexPool.getPtr(frag.m_indexId);
  if (unlikely(index.m_state != Index::Online) &&
      scan.m_errorCode == 0)
  {
    jam();
#ifdef VM_TRACE
    if (debugFlags & (DebugMeta | DebugScan)) {
      debugOut << "Index dropping at execACC_CHECK_SCAN " << scanPtr.i << " " << *scanPtr.p << endl;
    }
#endif
    scan.m_errorCode = AccScanRef::TuxIndexNotOnline;
  }
  if (scan.m_state == ScanOp::First)
  {
    jamDebug();
    // search is done only once in single range scan
    scanFirst(scanPtr, frag, index);
  }
  if (scan.m_state == ScanOp::Current ||
      scan.m_state == ScanOp::Next)
  {
    jamDebug();
    // look for next
    scanFind(scanPtr, frag);
  }
  // for reading tuple key in Found or Locked state
  Uint32* pkData = c_ctx.c_dataBuffer;
  unsigned pkSize = 0; // indicates not yet done
  if (likely(scan.m_state == ScanOp::Found))
  {
    // found an entry to return
    jamDebug();
    ndbrequire(scan.m_accLockOp == RNIL);
    if (unlikely(! scan.m_readCommitted))
    {
      jamDebug();
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
      Uint32 lkey1, lkey2;
      getTupAddr(frag, ent, lkey1, lkey2);
      lockReq->page_id = lkey1;
      lockReq->page_idx = lkey2;
      lockReq->transId1 = scan.m_transId1;
      lockReq->transId2 = scan.m_transId2;
      // execute
      EXECUTE_DIRECT(DBACC, GSN_ACC_LOCKREQ, signal, AccLockReq::LockSignalLength);
      jamEntryDebug();
      switch (lockReq->returnCode)
      {
      case AccLockReq::Success:
      {
        jam();
        scan.m_state = ScanOp::Locked;
        scan.m_accLockOp = lockReq->accOpPtr;
#ifdef VM_TRACE
        if (debugFlags & (DebugScan | DebugLock))
        {
          debugOut << "Lock immediate scan " << scanPtr.i << " " << scan << endl;
        }
#endif
        break;
      }
      case AccLockReq::IsBlocked:
      {
        jam();
        // normal lock wait
        scan.m_state = ScanOp::Blocked;
        scan.m_lockwait = true;
        scan.m_accLockOp = lockReq->accOpPtr;
#ifdef VM_TRACE
        if (debugFlags & (DebugScan | DebugLock))
        {
          debugOut << "Lock wait scan " << scanPtr.i << " " << scan << endl;
        }
#endif
        // LQH will wake us up
        CheckLcpStop* cls = (CheckLcpStop*) signal->theData;
        cls->scanPtrI = scan.m_userPtr;
        cls->scanState = CheckLcpStop::ZSCAN_RESOURCE_WAIT;
        EXECUTE_DIRECT(DBLQH, GSN_CHECK_LCP_STOP, signal, 2);
        jamEntry();
        ndbassert(signal->theData[0] == CheckLcpStop::ZTAKE_A_BREAK);
        return;  // stop
        break;
      }
      case AccLockReq::Refused:
      {
        jam();
        // we cannot see deleted tuple (assert only)
        ndbassert(false);
        // skip it
        scan.m_state = ScanOp::Next;
        CheckLcpStop* cls = (CheckLcpStop*) signal->theData;
        cls->scanPtrI = scan.m_userPtr;
        cls->scanState = CheckLcpStop::ZSCAN_RESOURCE_WAIT;
        EXECUTE_DIRECT(DBLQH, GSN_CHECK_LCP_STOP, signal, 2);
        jamEntry();
        ndbassert(signal->theData[0] == CheckLcpStop::ZTAKE_A_BREAK);
        return;  // stop
        break;
      }
      case AccLockReq::NoFreeOp:
      {
        jam();
        // max ops should depend on max scans (assert only)
        ndbassert(false);
        // stay in Found state
        scan.m_state = ScanOp::Found;
        CheckLcpStop* cls = (CheckLcpStop*) signal->theData;
        cls->scanPtrI = scan.m_userPtr;
        cls->scanState = CheckLcpStop::ZSCAN_RESOURCE_WAIT;
        EXECUTE_DIRECT(DBLQH, GSN_CHECK_LCP_STOP, signal, 2);
        jamEntry();
        ndbassert(signal->theData[0] == CheckLcpStop::ZTAKE_A_BREAK);
        return;  // stop
        break;
      }
      default:
        ndbrequire(false);
        break;
      }
    }
    else
    {
      scan.m_state = ScanOp::Locked;
    }
  }
  else if (scan.m_state == ScanOp::Next)
  {
    jam();
    // Taking a break from searching the tree
    CheckLcpStop* cls = (CheckLcpStop*) signal->theData;
    cls->scanPtrI = scan.m_userPtr;
    cls->scanState = CheckLcpStop::ZSCAN_RUNNABLE_YIELD;
    EXECUTE_DIRECT(DBLQH, GSN_CHECK_LCP_STOP, signal, 2);
    jam();
    ndbassert(signal->theData[0] == CheckLcpStop::ZTAKE_A_BREAK);
    return;
  }
  if (likely(scan.m_state == ScanOp::Locked))
  {
    // we have lock or do not need one
    jamDebug();
    // read keys if not already done (uses signal)
    const TreeEnt ent = scan.m_scanEnt;
    // conf signal
    NextScanConf* const conf = (NextScanConf*)signal->getDataPtrSend();
    conf->scanPtr = scan.m_userPtr;
    // the lock is passed to LQH
    Uint32 accLockOp = scan.m_accLockOp;
    if (unlikely(accLockOp != RNIL))
    {
      scan.m_accLockOp = RNIL;
      // remember it until LQH unlocks it
      addAccLockOp(scanPtr, accLockOp);
    }
    else
    {
      ndbrequire(scan.m_readCommitted);
      // operation RNIL in LQH would signal no tuple returned
      accLockOp = (Uint32)-1;
    }
    conf->accOperationPtr = accLockOp;
    conf->fragId = frag.m_fragId;
    const TupLoc tupLoc = ent.m_tupLoc;
    Uint32 lkey1 = tupLoc.getPageId();
    Uint32 lkey2 = tupLoc.getPageOffset();
    conf->localKey[0] = lkey1;
    conf->localKey[1] = lkey2;
#ifdef VM_TRACE
    /**
     * We need another call to make sure that ndbassert's are
     * correct even if we called prepare_scan_TUPKEYREQ and
     * set an original page id as row id when performing a
     * lock request through ACC between here and before
     * scanCheck.
     * No need to do this in production code since it is ok
     * to call prepare methods several times on the same row.
     */
    c_tup->prepare_scan_tux_TUPKEYREQ(lkey1, lkey2);
#endif
    // add key info
    // next time look for next entry
    scan.m_state = ScanOp::Next;
    signal->setLength(NextScanConf::SignalLengthNoGCI);
    c_lqh->exec_next_scan_conf(signal);
    return;
  }
  // In ACC this is checked before req->checkLcpStop
  if (scan.m_state == ScanOp::Last)
  {
    jamDebug();
    NextScanConf* const conf = (NextScanConf*)signal->getDataPtrSend();
    conf->scanPtr = scan.m_userPtr;
    conf->accOperationPtr = RNIL;
    conf->fragId = RNIL;
    signal->setLength(NextScanConf::SignalLengthNoTuple);
    c_lqh->exec_next_scan_conf(signal);
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
      // can happen when Dropping
#ifdef VM_TRACE
      const Frag& frag = *c_fragPool.getPtr(scan.m_fragPtrI);
      const Index& index = *c_indexPool.getPtr(frag.m_indexId);
      ndbassert(index.m_state != Index::Online);
#endif
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
 * ACCKEYCONF or ACCKEYREF which may have been in job buffer.
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
  c_lqh->setup_scan_pointers(scan.m_userPtr);
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
Dbtux::scanFirst(ScanOpPtr scanPtr, Frag& frag, const Index& index)
{
  ScanOp& scan = *scanPtr.p;
#ifdef VM_TRACE
  if (debugFlags & DebugScan)
  {
    debugOut << "Enter first scan " << scanPtr.i << " " << scan << endl;
  }
#endif
  // scan direction 0, 1
  const unsigned idir = scan.m_descending;
  // set up bound from segmented memory
  const ScanBound& scanBound = scan.m_scanBound[idir];
  KeyDataC searchBoundData(index.m_keySpec, true);
  KeyBoundC searchBound(searchBoundData);
  unpackBound(c_ctx, scanBound, searchBound);
  TreePos treePos;
  searchToScan(frag, idir, searchBound, treePos);
  if (treePos.m_loc != NullTupLoc)
  {
    scan.m_scanPos = treePos;
    // link the scan to node found
    NodeHandle node(frag);
    selectNode(node, treePos.m_loc);
    linkScan(node, scanPtr);
    if (treePos.m_dir == 3)
    {
      jamDebug();
      // check upper bound
      TreeEnt ent = node.getEnt(treePos.m_pos);
      const TupLoc tupLoc = ent.m_tupLoc;
      jamDebug();
      c_tup->prepare_scan_tux_TUPKEYREQ(tupLoc.getPageId(),
                                        tupLoc.getPageOffset());
      jamDebug();
      if (scanCheck(scanPtr, ent, frag))
      {
        jamDebug();
        scan.m_state = ScanOp::Current;
      }
      else
      {
        jamDebug();
        scan.m_state = ScanOp::Last;
      }
    } else {
      jamDebug();
      scan.m_state = ScanOp::Next;
    }
  } else {
    jamDebug();
    scan.m_state = ScanOp::Last;
  }
#ifdef VM_TRACE
  if (debugFlags & DebugScan)
  {
    debugOut << "Leave first scan " << scanPtr.i << " " << scan << endl;
  }
#endif
}

/*
 * Look for entry to return as scan result.
 */
void
Dbtux::scanFind(ScanOpPtr scanPtr, Frag& frag)
{
  ScanOp& scan = *scanPtr.p;
#ifdef VM_TRACE
  if (debugFlags & DebugScan)
  {
    debugOut << "Enter find scan " << scanPtr.i << " " << scan << endl;
  }
#endif
  ndbrequire(scan.m_state == ScanOp::Current || scan.m_state == ScanOp::Next);
  while (1)
  {
    jamDebug();
    if (scan.m_state == ScanOp::Next)
    {
      scanNext(scanPtr, false, frag);
    }
    if (scan.m_state == ScanOp::Current)
    {
      jamDebug();
      const TreePos pos = scan.m_scanPos;
      NodeHandle node(frag);
      selectNode(node, pos.m_loc);
      const TreeEnt ent = node.getEnt(pos.m_pos);
      if (unlikely(scan.m_statOpPtrI != RNIL))
      {
        StatOpPtr statPtr;
        statPtr.i = scan.m_statOpPtrI;
        c_statOpPool.getPtr(statPtr);
        // report row to stats, returns true if a sample is available
        int ret = statScanAddRow(statPtr, ent);
        if (ret == 1)
        {
          jam();
          scan.m_state = ScanOp::Found;
          // may not access non-pseudo cols but must return valid ent
          scan.m_scanEnt = ent;
          break;
        }
        else if (ret == 2)
        {
          // take a break
          jam();
          scan.m_state = ScanOp::Next;
          scan.m_scanEnt = ent;
          break;
        }
      }
      else if (scanVisible(scanPtr, ent))
      {
        jamDebug();
        scan.m_state = ScanOp::Found;
        scan.m_scanEnt = ent;
        break;
      }
    }
    else
    {
      jamDebug();
      break;
    }
    scan.m_state = ScanOp::Next;
  }
#ifdef VM_TRACE
  if (debugFlags & DebugScan)
  {
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
 * 3 - left to right within node (at end set state 5)
 * 4 - down from parent (proceed to left child)
 * 5 - at node end proceed to right child (state becomes 4)
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
Dbtux::scanNext(ScanOpPtr scanPtr, bool fromMaintReq, Frag& frag)
{
  ScanOp& scan = *scanPtr.p;
#ifdef VM_TRACE
  if (debugFlags & (DebugMaint | DebugScan))
  {
    debugOut << "Enter next scan " << scanPtr.i << " " << scan << endl;
  }
#endif
  // cannot be moved away from tuple we have locked
#if defined VM_TRACE || defined ERROR_INSERT
  ndbrequire(scan.m_state != ScanOp::Locked);
#else
  ndbrequire(fromMaintReq || scan.m_state != ScanOp::Locked);
#endif
  // scan direction
  const unsigned idir = scan.m_descending; // 0, 1
  const int jdir = 1 - 2 * (int)idir;      // 1, -1
  // use copy of position
  TreePos pos = scan.m_scanPos;
  // get and remember original node
  NodeHandle origNode(frag);
  selectNode(origNode, pos.m_loc);
  ndbrequire(islinkScan(origNode, scanPtr));
  if (unlikely(scan.m_state == ScanOp::Locked))
  {
    // bug#32040 - no fix, just unlock and continue
    jam();
    if (scan.m_accLockOp != RNIL)
    {
      jam();
      Signal* signal = c_signal_bug32040;
      AccLockReq* const lockReq = (AccLockReq*)signal->getDataPtrSend();
      lockReq->returnCode = RNIL;
      lockReq->requestInfo = AccLockReq::Abort;
      lockReq->accOpPtr = scan.m_accLockOp;
      EXECUTE_DIRECT(DBACC, GSN_ACC_LOCKREQ, signal, AccLockReq::UndoSignalLength);
      jamEntry();
      ndbrequire(lockReq->returnCode == AccLockReq::Success);
      scan.m_accLockOp = RNIL;
      scan.m_lockwait = false;
    }
    scan.m_state = ScanOp::Next;
  }
  // current node in loop
  NodeHandle node = origNode;
  // copy of entry found
  TreeEnt ent;
  while (true)
  {
    jamDebug();
#ifdef VM_TRACE
    if (debugFlags & (DebugMaint | DebugScan))
    {
      debugOut << "Current scan " << scanPtr.i << " pos " << pos;
      debugOut << " node " << node << endl;
    }
#endif
    if (pos.m_dir == 2)
    {
      // coming up from root ends the scan
      jamDebug();
      pos.m_loc = NullTupLoc;
      break;
    }
    if (node.m_loc != pos.m_loc)
    {
      jamDebug();
      selectNode(node, pos.m_loc);
    }
    if (pos.m_dir == 4)
    {
      // coming down from parent proceed to left child
      jamDebug();
      TupLoc loc = node.getLink(idir);
      if (loc != NullTupLoc)
      {
        jamDebug();
        pos.m_loc = loc;
        pos.m_dir = 4;  // unchanged
        continue;
      }
      // pretend we came from left child
      pos.m_dir = idir;
    }
    if (pos.m_dir == 5)
    {
      // at node end proceed to right child
      jamDebug();
      TupLoc loc = node.getLink(1 - idir);
      if (loc != NullTupLoc)
      {
        jamDebug();
        pos.m_loc = loc;
        pos.m_dir = 4;  // down from parent as usual
        continue;
      }
      // pretend we came from right child
      pos.m_dir = 1 - idir;
    }
    const unsigned occup = node.getOccup();
    if (occup == 0)
    {
      jamDebug();
      ndbrequire(fromMaintReq);
      // move back to parent - see comment in treeRemoveInner
      pos.m_loc = node.getLink(2);
      pos.m_dir = node.getSide();
      continue;
    }
    if (pos.m_dir == idir)
    {
      // coming up from left child scan current node
      jamDebug();
      pos.m_pos = idir == 0 ? (Uint16)-1 : occup;
      pos.m_dir = 3;
    }
    if (pos.m_dir == 3)
    {
      // before or within node
      jamDebug();
      // advance position - becomes ZNIL (> occup) if 0 and descending
      pos.m_pos += jdir;
      if (pos.m_pos < occup)
      {
        jamDebug();
        pos.m_dir = 3;  // unchanged
        ent = node.getEnt(pos.m_pos);
        const TupLoc tupLoc = ent.m_tupLoc;
        jamDebug();
        c_tup->prepare_scan_tux_TUPKEYREQ(tupLoc.getPageId(),
                                          tupLoc.getPageOffset());
        jamDebug();
        if (! scanCheck(scanPtr, ent, frag))
        {
          jamDebug();
          pos.m_loc = NullTupLoc;
        }
        break;
      }
      // after node proceed to right child
      pos.m_dir = 5;
      continue;
    }
    if (pos.m_dir == 1 - idir)
    {
      // coming up from right child proceed to parent
      jamDebug();
      pos.m_loc = node.getLink(2);
      pos.m_dir = node.getSide();
      continue;
    }
    ndbrequire(false);
  }
  // copy back position
  scan.m_scanPos = pos;
  // relink
  if (pos.m_loc != NullTupLoc)
  {
    ndbrequire(pos.m_dir == 3);
    ndbrequire(pos.m_loc == node.m_loc);
    if (origNode.m_loc != node.m_loc)
    {
      jamDebug();
      unlinkScan(origNode, scanPtr);
      linkScan(node, scanPtr);
    }
    if (scan.m_state != ScanOp::Blocked)
    {
      scan.m_state = ScanOp::Current;
    }
    else
    {
      jamDebug();
      ndbrequire(fromMaintReq);
      TreeEnt& scanEnt = scan.m_scanEnt;
      ndbrequire(scanEnt.m_tupLoc != NullTupLoc);
      if (scanEnt.eqtuple(ent))
      {
        // remains blocked on another version
        scanEnt = ent;
      } else {
        jamDebug();
        scanEnt.m_tupLoc = NullTupLoc;
        scan.m_state = ScanOp::Current;
      }
    }
  }
  else
  {
    jamDebug();
    unlinkScan(origNode, scanPtr);
    scan.m_state = ScanOp::Last;
  }
#ifdef VM_TRACE
  if (debugFlags & (DebugMaint | DebugScan))
  {
    debugOut << "Leave next scan " << scanPtr.i << " " << scan << endl;
  }
#endif
}

/*
 * Check end key.  Return true if scan is still within range.
 *
 * Error handling:  If scan error code has been set, return false at
 * once.  This terminates the scan and also avoids kernel crash on
 * invalid data.
 */
bool
Dbtux::scanCheck(ScanOpPtr scanPtr, TreeEnt ent, Frag& frag)
{
  jamDebug();
  ScanOp& scan = *scanPtr.p;
  if (unlikely(scan.m_errorCode != 0))
  {
    jam();
    return false;
  }
  const Index& index = *c_indexPool.getPtr(frag.m_indexId);
  const unsigned idir = scan.m_descending;
  const int jdir = 1 - 2 * (int)idir;
  const ScanBound& scanBound = scan.m_scanBound[1 - idir];
  int ret = 0;
  if (scanBound.m_cnt != 0)
  {
    jamDebug();
    // set up bound from segmented memory
    KeyDataC searchBoundData(index.m_keySpec, true);
    KeyBoundC searchBound(searchBoundData);
    unpackBound(c_ctx, scanBound, searchBound);
    // key data for the entry
    KeyData entryKey(index.m_keySpec, true, 0);
    entryKey.set_buf(c_ctx.c_entryKey, MaxAttrDataSize << 2);
    readKeyAttrsCurr(c_ctx, frag, ent, entryKey, index.m_numAttrs);
    // compare bound to key
    const Uint32 boundCount = searchBound.get_data().get_cnt();
    ret = cmpSearchBound(c_ctx, searchBound, entryKey, boundCount);
    ndbrequire(ret != 0);
    ret = (-1) * ret; // reverse for key vs bound
    ret = jdir * ret; // reverse for descending scan
  }
#ifdef VM_TRACE
  if (debugFlags & DebugScan)
  {
    debugOut << "Check scan " << scanPtr.i << " " << scan
             << " ret:" << dec << ret << endl;
  }
#endif
  return (ret <= 0);
}

/*
 * Check if an entry is visible to the scan.
 *
 * There is a special check to never accept same tuple twice in a row.
 * This is faster than asking TUP.  It also fixes some special cases
 * which are not analyzed or handled yet.
 *
 * Error handling:  If scan error code has been set, return false since
 * no new result can be returned to LQH.  The scan will then look for
 * next result and terminate via scanCheck():
 */
bool
Dbtux::scanVisible(ScanOpPtr scanPtr, TreeEnt ent)
{
  const ScanOp& scan = *scanPtr.p;
  if (unlikely(scan.m_errorCode != 0)) {
    jam();
    return false;
  }
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
  jamEntryDebug();
  return ret;
}

/*
 * Finish closing of scan and send conf.  Any lock wait has been done
 * already.
 *
 * Error handling:  Every scan ends here.  If error code has been set,
 * send a REF.
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
  Uint32 blockNo = refToMain(scanPtr.p->m_userRef);
  if (scanPtr.p->m_errorCode == 0) {
    jamDebug();
    // send conf
    NextScanConf* const conf = (NextScanConf*)signal->getDataPtrSend();
    conf->scanPtr = scanPtr.p->m_userPtr;
    conf->accOperationPtr = RNIL;
    conf->fragId = RNIL;
    releaseScanOp(scanPtr);
    signal->setLength(NextScanConf::SignalLengthNoTuple);
    c_lqh->exec_next_scan_conf(signal);
    return;
  } else {
    // send ref
    NextScanRef* ref = (NextScanRef*)signal->getDataPtr();
    ref->scanPtr = scanPtr.p->m_userPtr;
    ref->accOperationPtr = RNIL;
    ref->fragId = RNIL;
    ref->errorCode = scanPtr.p->m_errorCode;
    releaseScanOp(scanPtr);
    EXECUTE_DIRECT(blockNo,
                   GSN_NEXT_SCANREF,
                   signal,
                   NextScanRef::SignalLength);
    return;
  }
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
  Local_ScanLock_fifo list(c_scanLockPool, scan.m_accLockOps);
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
  Local_ScanLock_fifo list(c_scanLockPool, scan.m_accLockOps);
  ScanLockPtr lockPtr;
#ifdef VM_TRACE
  list.first(lockPtr);
  while (lockPtr.i != RNIL) {
    ndbrequire(lockPtr.p->m_accLockOp != accLockOp);
    list.next(lockPtr);
  }
#endif
  bool ok = list.seizeLast(lockPtr);
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
  Local_ScanLock_fifo list(c_scanLockPool, scan.m_accLockOps);
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
  for (unsigned i = 0; i <= 1; i++) {
    ScanBound& scanBound = scanPtr.p->m_scanBound[i];
    ScanBoundBuffer::Head& head = scanBound.m_head;
    LocalScanBoundBuffer b(c_scanBoundPool, head);
    b.release();
  }
  if (unlikely(scanPtr.p->m_statOpPtrI != RNIL)) {
    jam();
    StatOpPtr statPtr;
    statPtr.i = scanPtr.p->m_statOpPtrI;
    c_statOpPool.getPtr(statPtr);
    c_statOpPool.release(statPtr);
  }
  // unlink from per-fragment list and release from pool
  frag.m_scanList.release(scanPtr);
}
