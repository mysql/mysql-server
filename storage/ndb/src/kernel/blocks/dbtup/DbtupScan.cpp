/*
   Copyright (c) 2005, 2018, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#define DBTUP_C
#define DBTUP_SCAN_CPP
#include "Dbtup.hpp"
#include "../backup/Backup.hpp"
#include <signaldata/AccScan.hpp>
#include <signaldata/NextScan.hpp>
#include <signaldata/AccLock.hpp>
#include <md5_hash.hpp>
#include <portlib/ndb_prefetch.h>
#include "../dblqh/Dblqh.hpp"

#define JAM_FILE_ID 408

#ifdef VM_TRACE
//#define DEBUG_LCP 1
//#define DEBUG_LCP_DEL 1
//#define DEBUG_LCP_DEL2 1
//#define DEBUG_LCP_DEL_EXTRA 1
//#define DEBUG_LCP_SKIP 1
//#define DEBUG_LCP_SKIP_EXTRA 1
//#define DEBUG_LCP_KEEP 1
//#define DEBUG_LCP_REL 1
//#define DEBUG_NR_SCAN 1
//#define DEBUG_NR_SCAN_EXTRA 1
//#define DEBUG_LCP_SCANNED_BIT 1
//#define DEBUG_LCP_FILTER 1
//#define DEBUG_LCP_SKIP 1
//#define DEBUG_LCP_DEL 1
//#define DEBUG_LCP_DELAY 1
#endif

#ifdef DEBUG_LCP_DELAY
#define DEB_LCP_DELAY(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_LCP_DELAY(arglist) do { } while (0)
#endif

#ifdef DEBUG_LCP_FILTER
#define DEB_LCP_FILTER(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_LCP_FILTER(arglist) do { } while (0)
#endif

#ifdef DEBUG_LCP
#define DEB_LCP(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_LCP(arglist) do { } while (0)
#endif

#ifdef DEBUG_LCP_DEL
#define DEB_LCP_DEL(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_LCP_DEL(arglist) do { } while (0)
#endif

#ifdef DEBUG_LCP_DEL2
#define DEB_LCP_DEL2(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_LCP_DEL2(arglist) do { } while (0)
#endif

#ifdef DEBUG_LCP_DEL_EXTRA
#define DEB_LCP_DEL_EXTRA(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_LCP_DEL_EXTRA(arglist) do { } while (0)
#endif

#ifdef DEBUG_LCP_SKIP
#define DEB_LCP_SKIP(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_LCP_SKIP(arglist) do { } while (0)
#endif

#ifdef DEBUG_LCP_SKIP_EXTRA
#define DEB_LCP_SKIP_EXTRA(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_LCP_SKIP_EXTRA(arglist) do { } while (0)
#endif

#ifdef DEBUG_LCP_KEEP
#define DEB_LCP_KEEP(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_LCP_KEEP(arglist) do { } while (0)
#endif

#ifdef DEBUG_LCP_REL
#define DEB_LCP_REL(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_LCP_REL(arglist) do { } while (0)
#endif

#ifdef DEBUG_NR_SCAN
#define DEB_NR_SCAN(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_NR_SCAN(arglist) do { } while (0)
#endif

#ifdef DEBUG_NR_SCAN_EXTRA
#define DEB_NR_SCAN_EXTRA(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_NR_SCAN_EXTRA(arglist) do { } while (0)
#endif

#ifdef VM_TRACE
#define dbg(x) globalSignalLoggers.log x
#else
#define dbg(x)
#endif

void
Dbtup::execACC_SCANREQ(Signal* signal)
{
  jamEntry();
  const AccScanReq reqCopy = *(const AccScanReq*)signal->getDataPtr();
  const AccScanReq* const req = &reqCopy;
  ScanOpPtr scanPtr;
  scanPtr.i = RNIL;
  do {
    // find table and fragment
    TablerecPtr tablePtr;
    tablePtr.i = req->tableId;
    ptrCheckGuard(tablePtr, cnoOfTablerec, tablerec);
    FragrecordPtr fragPtr;
    Uint32 fragId = req->fragmentNo;
    fragPtr.i = RNIL;
    getFragmentrec(fragPtr, fragId, tablePtr.p);
    ndbrequire(fragPtr.i != RNIL);
    Fragrecord& frag = *fragPtr.p;
    // flags
    Uint32 bits = 0;
    
    if (AccScanReq::getLcpScanFlag(req->requestInfo))
    {
      jam();
      bits |= ScanOp::SCAN_LCP;
      c_scanOpPool.getPtr(scanPtr, c_lcp_scan_op);
      ndbrequire(scanPtr.p->m_fragPtrI == fragPtr.i);
      ndbrequire(scanPtr.p->m_state == ScanOp::First);
    }
    else
    {
      // seize from pool and link to per-fragment list
      Local_ScanOp_list list(c_scanOpPool, frag.m_scanList);
      if (! list.seizeFirst(scanPtr)) {
	jam();
	break;
      }
      new (scanPtr.p) ScanOp;
      jam();
    }

    if (!AccScanReq::getNoDiskScanFlag(req->requestInfo)
        && tablePtr.p->m_no_of_disk_attributes)
    {
      jam();
      bits |= ScanOp::SCAN_DD;
    }
      
    bool mm = (bits & ScanOp::SCAN_DD);
    if ((tablePtr.p->m_attributes[mm].m_no_of_varsize +
         tablePtr.p->m_attributes[mm].m_no_of_dynamic) > 0) 
    {
      if (bits & ScanOp::SCAN_DD)
      {
        // only dd scan varsize pages
        // mm always has a fixed part
        jam();
        bits |= ScanOp::SCAN_VS;
      }
    }

    if (! AccScanReq::getReadCommittedFlag(req->requestInfo)) 
    {
      if (AccScanReq::getLockMode(req->requestInfo) == 0)
      {
        jam();
        bits |= ScanOp::SCAN_LOCK_SH;
      }
      else
      {
        jam();
        bits |= ScanOp::SCAN_LOCK_EX;
      }
    }

    if (AccScanReq::getNRScanFlag(req->requestInfo))
    {
      jam();
      bits |= ScanOp::SCAN_NR;
      scanPtr.p->m_endPage = req->maxPage;
      if (req->maxPage != RNIL && req->maxPage > frag.m_max_page_cnt)
      {
        DEB_NR_SCAN(("%u %u endPage: %u (noOfPages: %u maxPage: %u)", 
                     tablePtr.i,
                     fragId,
                     req->maxPage,
                     fragPtr.p->noOfPages,
                     fragPtr.p->m_max_page_cnt));
      }
    }
    else if (AccScanReq::getLcpScanFlag(req->requestInfo))
    {
      jam();
      ndbrequire((bits & ScanOp::SCAN_DD) == 0);
      ndbrequire((bits & ScanOp::SCAN_LOCK) == 0);
    }
    else
    {
      jam();
      scanPtr.p->m_endPage = RNIL;
    }

    if (bits & ScanOp::SCAN_VS)
    {
      jam();
      ndbrequire((bits & ScanOp::SCAN_NR) == 0);
      ndbrequire((bits & ScanOp::SCAN_LCP) == 0);
    }
    
    // set up scan op
    ScanOp& scan = *scanPtr.p;
    scan.m_state = ScanOp::First;
    scan.m_bits = bits;
    scan.m_userPtr = req->senderData;
    scan.m_userRef = req->senderRef;
    scan.m_tableId = tablePtr.i;
    scan.m_fragId = frag.fragmentId;
    scan.m_fragPtrI = fragPtr.i;
    scan.m_transId1 = req->transId1;
    scan.m_transId2 = req->transId2;
    scan.m_savePointId = req->savePointId;

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
  // LQH does not handle REF
  ndbabort();
  signal->theData[8] = 1; /* Failure */
  /* Return ACC_SCANREF */
}

void
Dbtup::execNEXT_SCANREQ(Signal* signal)
{
  jamEntryDebug();
  const NextScanReq reqCopy = *(const NextScanReq*)signal->getDataPtr();
  const NextScanReq* const req = &reqCopy;
  ScanOpPtr scanPtr;
  c_scanOpPool.getPtr(scanPtr, req->accPtr);
  ScanOp& scan = *scanPtr.p;
  switch (req->scanFlag) {
  case NextScanReq::ZSCAN_NEXT:
    jam();
    break;
  case NextScanReq::ZSCAN_COMMIT:
    jam();
    // Fall through
  case NextScanReq::ZSCAN_NEXT_COMMIT:
    jam();
    if ((scan.m_bits & ScanOp::SCAN_LOCK) != 0) {
      jam();
      AccLockReq* const lockReq = (AccLockReq*)signal->getDataPtrSend();
      lockReq->returnCode = RNIL;
      lockReq->requestInfo = AccLockReq::Unlock;
      lockReq->accOpPtr = req->accOperationPtr;
      EXECUTE_DIRECT(DBACC, GSN_ACC_LOCKREQ,
          signal, AccLockReq::UndoSignalLength);
      jamEntry();
      ndbrequire(lockReq->returnCode == AccLockReq::Success);
      removeAccLockOp(scan, req->accOperationPtr);
    }
    if (req->scanFlag == NextScanReq::ZSCAN_COMMIT) {
      signal->theData[0] = 0; /* Success */
      /**
       * signal->theData[0] = 0 means return signal
       * NEXT_SCANCONF for NextScanReq::ZSCAN_COMMIT
       */
      return;
    }
    break;
  case NextScanReq::ZSCAN_CLOSE:
    jam();
    if (scan.m_bits & ScanOp::SCAN_LOCK_WAIT) {
      jam();
      ndbrequire(scan.m_accLockOp != RNIL);
      // use ACC_ABORTCONF to flush out any reply in job buffer
      AccLockReq* const lockReq = (AccLockReq*)signal->getDataPtrSend();
      lockReq->returnCode = RNIL;
      lockReq->requestInfo = AccLockReq::AbortWithConf;
      lockReq->accOpPtr = scan.m_accLockOp;
      EXECUTE_DIRECT(DBACC, GSN_ACC_LOCKREQ,
		     signal, AccLockReq::UndoSignalLength);
      jamEntry();
      ndbrequire(lockReq->returnCode == AccLockReq::Success);
      scan.m_last_seen = __LINE__;
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
      EXECUTE_DIRECT(DBACC, GSN_ACC_LOCKREQ,
		     signal, AccLockReq::UndoSignalLength);
      jamEntry();
      ndbrequire(lockReq->returnCode == AccLockReq::Success);
      scan.m_accLockOp = RNIL;
    }
    scan.m_last_seen = __LINE__;
    scan.m_state = ScanOp::Aborting;
    scanClose(signal, scanPtr);
    return;
  case NextScanReq::ZSCAN_NEXT_ABORT:
    ndbabort();
  default:
    ndbabort();
  }
  // start looking for next scan result
  AccCheckScan* checkReq = (AccCheckScan*)signal->getDataPtrSend();
  checkReq->accPtr = scanPtr.i;
  checkReq->checkLcpStop = AccCheckScan::ZNOT_CHECK_LCP_STOP;
  EXECUTE_DIRECT(DBTUP, GSN_ACC_CHECK_SCAN, signal, AccCheckScan::SignalLength);
  jamEntryDebug();
}

void
Dbtup::execACC_CHECK_SCAN(Signal* signal)
{
  jamEntryDebug();
  const AccCheckScan reqCopy = *(const AccCheckScan*)signal->getDataPtr();
  const AccCheckScan* const req = &reqCopy;
  ScanOpPtr scanPtr;
  c_scanOpPool.getPtr(scanPtr, req->accPtr);
  ScanOp& scan = *scanPtr.p;
  // fragment
  FragrecordPtr fragPtr;
  fragPtr.i = scan.m_fragPtrI;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);
  Fragrecord& frag = *fragPtr.p;
  if (req->checkLcpStop == AccCheckScan::ZCHECK_LCP_STOP)
  {
    jam();
    CheckLcpStop* cls = (CheckLcpStop*) signal->theData;
    cls->scanPtrI = scan.m_userPtr;
    cls->scanState = CheckLcpStop::ZSCAN_RESOURCE_WAIT;
    EXECUTE_DIRECT(DBLQH, GSN_CHECK_LCP_STOP, signal, 2);
    jamEntry();
    ndbassert(signal->theData[0] == CheckLcpStop::ZTAKE_A_BREAK);
    return;
  }
  if (scan.m_bits & ScanOp::SCAN_LOCK_WAIT) {
    jam();
    // LQH asks if we are waiting for lock and we tell it to ask again
    NextScanConf* const conf = (NextScanConf*)signal->getDataPtrSend();
    conf->scanPtr = scan.m_userPtr;
    conf->accOperationPtr = RNIL;       // no tuple returned
    conf->fragId = frag.fragmentId;
    // if TC has ordered scan close, it will be detected here
    sendSignal(scan.m_userRef,
               GSN_NEXT_SCANCONF,
               signal,
               NextScanConf::SignalLengthNoTuple,
               JBB);
    return;     // stop
  }

  const bool lcp = (scan.m_bits & ScanOp::SCAN_LCP);

  if (scan.m_state == ScanOp::First)
  {
    if (lcp && ! fragPtr.p->m_lcp_keep_list_head.isNull())
    {
      jam();
      /**
       * Handle lcp keep list already here
       *   So that scan state is not altered
       *   if lcp_keep rows are found in ScanOp::First
       */
      scan.m_last_seen = __LINE__;
      handle_lcp_keep(signal, fragPtr, scanPtr.p);
      return;
    }
    jam();
    scanFirst(signal, scanPtr);
  }
  if (scan.m_state == ScanOp::Next)
  {
    jam();
    bool immediate = scanNext(signal, scanPtr);
    if (! immediate) {
      jam();
      // time-slicing via TUP or PGMAN
      return;
    }
    jam();
  }
  scanReply(signal, scanPtr);
}

void
Dbtup::scanReply(Signal* signal, ScanOpPtr scanPtr)
{
  ScanOp& scan = *scanPtr.p;
  FragrecordPtr fragPtr;
  fragPtr.i = scan.m_fragPtrI;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);
  Fragrecord& frag = *fragPtr.p;
  // for reading tuple key in Current state
  Uint32* pkData = (Uint32*)c_dataBuffer;
  unsigned pkSize = 0;
  if (scan.m_state == ScanOp::Current) {
    // found an entry to return
    jamDebug();
    ndbrequire(scan.m_accLockOp == RNIL);
    Uint32 scan_bits = scan.m_bits;
    if (scan_bits & ScanOp::SCAN_LOCK) {
      jam();
      ndbrequire((scan_bits & ScanOp::SCAN_LCP) == 0);
      scan.m_last_seen = __LINE__;
      // read tuple key - use TUX routine
      const ScanPos& pos = scan.m_scanPos;
      const Local_key& key_mm = pos.m_key_mm;
      int ret = tuxReadPk(fragPtr.i, pos.m_realpid_mm, key_mm.m_page_idx,
			  pkData, true);
      ndbrequire(ret > 0);
      pkSize = ret;
      dbg((DBTUP, "PK size=%d data=%08x", pkSize, pkData[0]));
      // get read lock or exclusive lock
      AccLockReq* const lockReq = (AccLockReq*)signal->getDataPtrSend();
      lockReq->returnCode = RNIL;
      lockReq->requestInfo = (scan.m_bits & ScanOp::SCAN_LOCK_SH) ?
        AccLockReq::LockShared : AccLockReq::LockExclusive;
      lockReq->accOpPtr = RNIL;
      lockReq->userPtr = scanPtr.i;
      lockReq->userRef = reference();
      lockReq->tableId = scan.m_tableId;
      lockReq->fragId = frag.fragmentId;
      lockReq->fragPtrI = RNIL; // no cached frag ptr yet
      lockReq->hashValue = md5_hash((Uint64*)pkData, pkSize);
      lockReq->page_id = key_mm.m_page_no;
      lockReq->page_idx = key_mm.m_page_idx;
      lockReq->transId1 = scan.m_transId1;
      lockReq->transId2 = scan.m_transId2;
      EXECUTE_DIRECT(DBACC, GSN_ACC_LOCKREQ,
          signal, AccLockReq::LockSignalLength);
      jamEntryDebug();
      switch (lockReq->returnCode) {
      case AccLockReq::Success:
      {
        jam();
        scan.m_state = ScanOp::Locked;
        scan.m_accLockOp = lockReq->accOpPtr;
        break;
      }
      case AccLockReq::IsBlocked:
      {
        jam();
        // normal lock wait
        scan.m_state = ScanOp::Blocked;
        scan.m_bits |= ScanOp::SCAN_LOCK_WAIT;
        scan.m_accLockOp = lockReq->accOpPtr;
        // LQH will wake us up
        CheckLcpStop* cls = (CheckLcpStop*) signal->theData;
        cls->scanPtrI = scan.m_userPtr;
        cls->scanState = CheckLcpStop::ZSCAN_RESOURCE_WAIT;
        EXECUTE_DIRECT(DBLQH, GSN_CHECK_LCP_STOP, signal, 2);
        jamEntry();
        ndbassert(signal->theData[0] == CheckLcpStop::ZTAKE_A_BREAK);
        return;
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
        return;
        break;
      }
      case AccLockReq::NoFreeOp:
      {
        jam();
        // max ops should depend on max scans (assert only)
        ndbassert(false);
        // stay in Current state
        scan.m_state = ScanOp::Current;
        CheckLcpStop* cls = (CheckLcpStop*) signal->theData;
        cls->scanPtrI = scan.m_userPtr;
        cls->scanState = CheckLcpStop::ZSCAN_RESOURCE_WAIT;
        EXECUTE_DIRECT(DBLQH, GSN_CHECK_LCP_STOP, signal, 2);
        jamEntry();
        ndbassert(signal->theData[0] == CheckLcpStop::ZTAKE_A_BREAK);
        return;
        break;
      }
      default:
        ndbabort();
      }
    } else {
      scan.m_state = ScanOp::Locked;
    }
  } 

  if (scan.m_state == ScanOp::Locked) {
    // we have lock or do not need one
    jamDebug();
    // conf signal
    NextScanConf* const conf = (NextScanConf*)signal->getDataPtrSend();
    conf->scanPtr = scan.m_userPtr;
    // the lock is passed to LQH
    Uint32 accLockOp = scan.m_accLockOp;
    if (accLockOp != RNIL) {
      scan.m_accLockOp = RNIL;
      // remember it until LQH unlocks it
      addAccLockOp(scan, accLockOp);
      scan.m_last_seen = __LINE__;
    } else {
      ndbrequire(! (scan.m_bits & ScanOp::SCAN_LOCK));
      // operation RNIL in LQH would signal no tuple returned
      accLockOp = (Uint32)-1;
      scan.m_last_seen = __LINE__;
    }
    const ScanPos& pos = scan.m_scanPos;
    conf->accOperationPtr = accLockOp;
    conf->fragId = frag.fragmentId;
    conf->localKey[0] = pos.m_key_mm.m_page_no;
    conf->localKey[1] = pos.m_key_mm.m_page_idx;
    // next time look for next entry
    scan.m_state = ScanOp::Next;
    prepareTUPKEYREQ(pos.m_key_mm.m_page_no,
                     pos.m_key_mm.m_page_idx,
                     fragPtr.i);
    /**
     * Running the lock code takes some extra execution time, one could
     * have this effect the number of tuples to read in one time slot.
     * We decided to ignore this here.
     */
    Uint32 blockNo = refToMain(scan.m_userRef);
    EXECUTE_DIRECT(blockNo,
                   GSN_NEXT_SCANCONF,
                   signal,
                   NextScanConf::SignalLengthNoGCI);
    jamEntryDebug();
    return;
  }
  if (scan.m_state == ScanOp::Last ||
      scan.m_state == ScanOp::Invalid) {
    jam();
    scan.m_last_seen = __LINE__;
    NextScanConf* const conf = (NextScanConf*)signal->getDataPtrSend();
    conf->scanPtr = scan.m_userPtr;
    conf->accOperationPtr = RNIL;
    conf->fragId = RNIL;
    Uint32 blockNo = refToMain(scan.m_userRef);
    EXECUTE_DIRECT(blockNo,
                   GSN_NEXT_SCANCONF,
                   signal,
                   NextScanConf::SignalLengthNoTuple);
    jamEntry();
    return;
  }
  ndbabort();
}

/*
 * Lock succeeded (after delay) in ACC.  If the lock is for current
 * entry, set state to Locked.  If the lock is for an entry we were
 * moved away from, simply unlock it.  Finally, if we are closing the
 * scan, do nothing since we have already sent an abort request.
 */
void
Dbtup::execACCKEYCONF(Signal* signal)
{
  jamEntry();
  ScanOpPtr scanPtr;
  scanPtr.i = signal->theData[0];

  Uint32 localKey1 = signal->theData[3];
  Uint32 localKey2 = signal->theData[4];
  Local_key tmp;
  tmp.m_page_no = localKey1;
  tmp.m_page_idx = localKey2;

  c_scanOpPool.getPtr(scanPtr);
  ScanOp& scan = *scanPtr.p;
  ndbrequire(scan.m_bits & ScanOp::SCAN_LOCK_WAIT && scan.m_accLockOp != RNIL);
  scan.m_bits &= ~ ScanOp::SCAN_LOCK_WAIT;
  if (scan.m_state == ScanOp::Blocked) {
    // the lock wait was for current entry
    jam();

    if (likely(scan.m_scanPos.m_key_mm.m_page_no == tmp.m_page_no &&
               scan.m_scanPos.m_key_mm.m_page_idx == tmp.m_page_idx))
    {
      jam();
      scan.m_state = ScanOp::Locked;
      // LQH has the ball
      return;
    }
    else
    {
      jam();
      /**
       * This means that there was DEL/INS on rowid that we tried to lock
       *   and the primary key that was previously located on this rowid
       *   (scanPos.m_key_mm) has moved.
       *   (DBACC keeps of track of primary keys)
       *
       * We don't care about the primary keys, but is interested in ROWID
       *   so rescan this position.
       *   Which is implemented by using execACCKEYREF...
       */
      ndbout << "execACCKEYCONF "
             << scan.m_scanPos.m_key_mm
             << " != " << tmp << " ";
      scan.m_bits |= ScanOp::SCAN_LOCK_WAIT;
      execACCKEYREF(signal);
      return;
    }
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
Dbtup::execACCKEYREF(Signal* signal)
{
  jamEntry();
  ScanOpPtr scanPtr;
  scanPtr.i = signal->theData[0];
  c_scanOpPool.getPtr(scanPtr);
  ScanOp& scan = *scanPtr.p;
  ndbrequire(scan.m_bits & ScanOp::SCAN_LOCK_WAIT && scan.m_accLockOp != RNIL);
  scan.m_bits &= ~ ScanOp::SCAN_LOCK_WAIT;
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
      //ndbassert(false);
      if (scan.m_bits & ScanOp::SCAN_NR)
      {
	jam();
	scan.m_state = ScanOp::Next;
	scan.m_scanPos.m_get = ScanPos::Get_tuple;
	DEB_NR_SCAN(("Ignoring scan.m_state == ScanOp::Blocked, refetch"));
      }
      else
      {
	jam();
	scan.m_state = ScanOp::Next;
	DEB_NR_SCAN(("Ignoring scan.m_state == ScanOp::Blocked"));
      }
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
Dbtup::execACC_ABORTCONF(Signal* signal)
{
  jamEntry();
  ScanOpPtr scanPtr;
  scanPtr.i = signal->theData[0];
  c_scanOpPool.getPtr(scanPtr);
  ScanOp& scan = *scanPtr.p;
  ndbrequire(scan.m_state == ScanOp::Aborting);
  // most likely we are still in lock wait
  if (scan.m_bits & ScanOp::SCAN_LOCK_WAIT) {
    jam();
    scan.m_bits &= ~ ScanOp::SCAN_LOCK_WAIT;
    scan.m_accLockOp = RNIL;
  }
  scanClose(signal, scanPtr);
}

void
Dbtup::scanFirst(Signal*, ScanOpPtr scanPtr)
{
  ScanOp& scan = *scanPtr.p;
  ScanPos& pos = scan.m_scanPos;
  Local_key& key = pos.m_key;
  const Uint32 bits = scan.m_bits;
  // fragment
  FragrecordPtr fragPtr;
  fragPtr.i = scan.m_fragPtrI;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);
  Fragrecord& frag = *fragPtr.p;

  if (bits & ScanOp::SCAN_NR)
  { 
    if (scan.m_endPage == 0 && frag.m_max_page_cnt == 0)
    {
      jam();
      scan.m_state = ScanOp::Last;
      return;
    }
  }
  else if (frag.noOfPages == 0)
  {
    jam();
    if (!(bits & ScanOp::SCAN_LCP))
    {
      jam();
      scan.m_state = ScanOp::Last;
      return;
    }
    /**
     * LCP scans will have to go through all pages even if no pages are still
     * remaining to ensure that we reset the LCP scanned bits that possibly
     * have been set before arriving here.
     */
  }

  if (bits & ScanOp::SCAN_LCP)
  {
    jam();
    if (scan.m_endPage == 0)
    {
      jam();
      /**
       * Partition was empty at start of LCP, no records to report.
       * In this case we cannot have set any LCP scanned bit since
       * no page was around in table when the scan was started.
       */
      scan.m_last_seen = __LINE__;
      scan.m_state = ScanOp::Last;
      return;
    }
    c_backup->init_lcp_scan(scan.m_scanGCI,
                            pos.m_lcp_scan_changed_rows_page);
    scan.m_last_seen = __LINE__;
  }

  if (! (bits & ScanOp::SCAN_DD)) {
    key.m_file_no = ZNIL;
    key.m_page_no = 0;
    pos.m_get = ScanPos::Get_page_mm;

    // for MM scan real page id is cached for efficiency
    pos.m_realpid_mm = RNIL;
  } else {
    Disk_alloc_info& alloc = frag.m_disk_alloc_info;
    // for now must check disk part explicitly
    if (alloc.m_extent_list.isEmpty()) {
      jam();
      scan.m_state = ScanOp::Last;
      return;
    }
    pos.m_extent_info_ptr_i = alloc.m_extent_list.getFirst();
    Extent_info* ext = c_extent_pool.getPtr(pos.m_extent_info_ptr_i);
    key.m_file_no = ext->m_key.m_file_no;
    key.m_page_no = ext->m_first_page_no;
    pos.m_get = ScanPos::Get_page_dd;
  }
  key.m_page_idx = ((bits & ScanOp::SCAN_VS) == 0) ? 0 : 1;
  // let scanNext() do the work
  scan.m_state = ScanOp::Next;
}

#define ZSCAN_FOUND_TUPLE 1
#define ZSCAN_FOUND_DELETED_ROWID 2
#define ZSCAN_FOUND_PAGE_END 3
#define ZSCAN_FOUND_DROPPED_CHANGE_PAGE 4
#define ZSCAN_FOUND_NEXT_ROW 5
/**
 * Start a scan of a page in LCP scan
 * ----------------------------------
 * We have seven options here for LCP scans:
 * 1) The page entry is empty and was empty at start of
 * LCP. In this case there is no flag set in the page
 * map indicating that page was dropped since last it
 * was dropped.
 * 1a) The page was belonging to the CHANGED ROWS pages and the
 * last LCP state was A. In this case we need to record a
 * DELETE by PAGEID in the LCP.
 *
 * 1b) The page belonged to the CHANGED ROWS pages and the last
 * LCP state was D. In this case we can ignore the page.
 *
 * 1c) The page was belonging to the ALL ROWS category.
 * We can ignore it since we only record rows existing at start of
 * the LCP.
 * Then we continue with the next page.
 *
 * 2) The page entry is empty and it was recorded as being
 * dropped since the LCP started. In this case the LCP scan
 * have already taken care of this page, the needed information
 * was sent to the LCP scan through the LCP keep list.
 * 3) The page entry was not empty but the page map indicates
 * that the page was dropped after the LCP scan started. In this
 * tricky case the LCP scan started, the page was dropped, the
 * page was resurrected again and finally now we come here to
 * handle the page. Again in this case we can move on since the
 * page was handled at the time the page was dropped.
 *
 * 2) and 3) are found through either the LCP_SCANNED_BIT being
 * set in the page map, or by the page_to_skip_lcp bit being set
 * on the page object.
 *
 * 4) The page entry is non-empty. This is the normal page
 * handling where we scan one row at a time.
 *
 * Finally the case 4) can have four distinct options as well.
 * 4a) The page existed before the LCP started and had rows
 * in it that need to checked one by one. This is the normal
 * case and by far the most commonly executed.
 *
 * 4b) The page did not exist before the LCP scan was started, but
 * it was allocated after the LCP scan started and before we scanned
 * it (thus got the LCP skip bit set on the page). It belonged to
 * the ALL ROWS pages and thus the page will be skipped.
 *
 * Discovered either by LCP_SCANNED_BIT or by page_to_skip_lcp bit
 * being set on the page.
 *
 * 4c) Same as 4b) except that it belongs to the CHANGED ROWS pages.
 * Also the last LCP state was D. Page is ignored.
 *
 * 4d) Same as 4c) except that last LCP state was A. In this we
 * record the page as a DELETE by PAGEID in the LCP.
 */
Uint32
Dbtup::prepare_lcp_scan_page(ScanOp& scan,
                             Local_key& key,
                             Uint32 *next_ptr,
                             Uint32 *prev_ptr)
{
  ScanPos& pos = scan.m_scanPos;
  bool lcp_page_already_scanned = get_lcp_scanned_bit(next_ptr);
  if (lcp_page_already_scanned)
  {
    jam();
    /* Coverage tested */
#ifdef DEBUG_LCP_SCANNED_BIT
    if (next_ptr)
    {
      g_eventLogger->info("(%u)tab(%u,%u).%u"
                          " reset_lcp_scanned_bit(2)",
                          instance(),
                          m_curr_fragptr.p->fragTableId,
                          m_curr_fragptr.p->fragmentId,
                          key.m_page_no);
    }
#endif
    reset_lcp_scanned_bit(next_ptr);
    c_backup->skip_page_lcp_scanned_bit();
    /* Either 2) or 3) as described above */
    /**
     * No state in page map to update, the page hasn't been
     * defined yet, so the position in page map is empty.
     */
    pos.m_get = ScanPos::Get_next_page_mm;
    scan.m_last_seen = __LINE__;
    return ZSCAN_FOUND_PAGE_END; // incr loop count
  }
  else if (unlikely(pos.m_realpid_mm == RNIL))
  {
    bool is_last_lcp_state_A = !get_last_lcp_state(prev_ptr);
    bool need_record_dropped_change =
      pos.m_lcp_scan_changed_rows_page && is_last_lcp_state_A;
    /**
     * Case 1) from above
     * If we come here without having LCP_SCANNED_BIT set then
     * we haven't released the page during LCP scan. Thus the
     * new last LCP state is D. Ensure that LAST_LCP_FREE_BIT
    * is set to indicate that LCP state is D for this LCP.
     */
    DEB_LCP_DEL2(("(%u)tab(%u,%u) page(%u),"
                  " is_last_lcp_state_A: %u, CHANGED: %u",
                  instance(),
                  m_curr_fragptr.p->fragTableId,
                  m_curr_fragptr.p->fragmentId,
                  key.m_page_no,
                  is_last_lcp_state_A,
                  pos.m_lcp_scan_changed_rows_page));

    set_last_lcp_state(prev_ptr, true);
    if (!need_record_dropped_change)
    {
      jam();
      /* Coverage tested */
      /* LCP case 1b) and 1c) above goes this way */
      scan.m_last_seen = __LINE__;
      pos.m_get = ScanPos::Get_next_page_mm;
      c_backup->skip_empty_page_lcp();
      return ZSCAN_FOUND_PAGE_END; // incr loop count
    }
    else
    {
      jam();
      /* Coverage tested */
      /* 1a) as described above */
      scan.m_last_seen = __LINE__;
      pos.m_get = ScanPos::Get_next_page_mm;
      c_backup->record_dropped_empty_page_lcp();
      return ZSCAN_FOUND_DROPPED_CHANGE_PAGE;
    }
  }
  else
  {
    jam();
    /**
     * Case 4) above, we need to set the last LCP state flag
     * on the pos object to ensure that we know when a row
     * needs to be DELETE by ROWID or if it needs to be ignored.
     */
    pos.m_is_last_lcp_state_D = get_last_lcp_state(prev_ptr);
    scan.m_last_seen = __LINE__;
  }
  return ZSCAN_FOUND_TUPLE;
}

Uint32
Dbtup::handle_lcp_skip_page(ScanOp& scan,
                            Local_key key,
                            Page* page)
{
  ScanPos& pos = scan.m_scanPos;
  /**
   * The page was allocated after the LCP started, so it can only
   * contain rows that was allocated after start of LCP and should
   * thus not be part of LCP. It is case 4b), 4c) or 4d). We need to
   * clear the skip bit on the page. We need to get the old lcp state
   * to be able to decide if it is 4c) or 4d). We also need to set
   * the last LCP* state to D.
   */
  DEB_LCP_SKIP(("(%u)Clear LCP_SKIP on tab(%u,%u), page(%u), change: %u, D: %u",
                instance(),
                m_curr_fragptr.p->fragTableId,
                m_curr_fragptr.p->fragmentId,
                key.m_page_no,
                pos.m_lcp_scan_changed_rows_page,
                pos.m_is_last_lcp_state_D));

  page->clear_page_to_skip_lcp();
  set_last_lcp_state(m_curr_fragptr.p,
                     key.m_page_no,
                     true /* Set state to D */);

  if (pos.m_lcp_scan_changed_rows_page && !pos.m_is_last_lcp_state_D)
  {
    jam();
    /* Coverage tested */
    /**
     * Case 4d) from above
     * At start of LCP the page was dropped, we have information that
     * the page was dropped after the previous LCP. Thus we need to
     * record the entire page as DELETE by PAGEID.
     */
    scan.m_last_seen = __LINE__;
    pos.m_get = ScanPos::Get_next_page_mm;
    c_backup->record_late_alloc_page_lcp();
    return ZSCAN_FOUND_DROPPED_CHANGE_PAGE;
  }
  jam();
  /* Coverage tested */
  /**
   * Case 4b) and 4c) from above
   * For ALL ROWS pages the rows should be skipped for LCP, we clear
   * the LCP skip flag on page in this case to speed up skipping.
   *
   * We need to keep track of the state Get_next_page_mm when checking
   * if a rowid is part of the remaining lcp set. If we do a real-time
   * break right after setting Get_next_page_mm we need to move the
   * page number forward one step since we have actually completed the
   * current page number.
   */
  scan.m_last_seen = __LINE__;
  pos.m_get = ScanPos::Get_next_page_mm;
  c_backup->page_to_skip_lcp(!pos.m_is_last_lcp_state_D);
  return ZSCAN_FOUND_PAGE_END; //incr loop count
}

Uint32
Dbtup::handle_scan_change_page_rows(ScanOp& scan,
                                    Fix_page *fix_page,
                                    Tuple_header *tuple_header_ptr,
                                    Uint32 & foundGCI)
{
  ScanPos& pos = scan.m_scanPos;
  Local_key& key = pos.m_key;
  /**
   * Coming here means that the following condition is true.
   * bits & ScanOp::SCAN_LCP && pos.m_lcp_changed_page
   *
   * We have 3 cases here,
   * foundGCI == 0:
   *   This means that the row has not been committed yet
   *   and it has not had any previous rows in this row
   *   id either. However the previous LCP might still have
   *   had a row in this position since we could have
   *   deallocated a page and allocated it again between
   *   2 LCPs. In this case we have to ensure that the
   *   row id is deleted as part of the restore.
   *
   * foundGCI > scanGCI
   * Record has changed since last LCP
   *   if header says tuple is free then the row is a deleted
   *   row and we record it
   *   otherwise it is a normal row to be recorded in normal
   *   manner for LCPs.
   *
   * We record deleted rowid's only if scanGCI which indicates
   * that we are recording only changes from this row. We need
   * not record deleted rowids for those parts where we record
   * all rows.
   */
  Uint32 thbits = tuple_header_ptr->m_header_bits;
  if ((foundGCI = *tuple_header_ptr->get_mm_gci(m_curr_tabptr.p)) >
       scan.m_scanGCI)
  {
    if (unlikely(thbits & Tuple_header::LCP_DELETE))
    {
      jam();
      /* Ensure that LCP_DELETE bit is clear before we move on */
      /* Coverage tested */
      tuple_header_ptr->m_header_bits =
        thbits & (~Tuple_header::LCP_DELETE);
      updateChecksum(tuple_header_ptr,
                     m_curr_tabptr.p,
                     thbits,
                     tuple_header_ptr->m_header_bits);
      fix_page->set_change_map(key.m_page_idx);
      jamDebug();
      jamLineDebug((Uint16)key.m_page_idx);
      ndbassert(!(thbits & Tuple_header::LCP_SKIP));
      DEB_LCP_DEL(("(%u)Reset LCP_DELETE on tab(%u,%u),"
                   " row(%u,%u), header: %x",
                   instance(),
                   m_curr_fragptr.p->fragTableId,
                   m_curr_fragptr.p->fragmentId,
                   key.m_page_no,
                   key.m_page_idx,
                   thbits));
      scan.m_last_seen = __LINE__;
      return ZSCAN_FOUND_DELETED_ROWID;
    }
    else if (! (thbits & Tuple_header::FREE ||
                thbits & Tuple_header::ALLOC))
    {
      jam();
      /**
       * Tuple has changed since last LCP, we need to record
       * the row as a changed row unless the LCP_SKIP bit is
       * set on the rowid which means that the row was inserted
       * after starting the LCP.
       */
      scan.m_last_seen = __LINE__;
      return ZSCAN_FOUND_TUPLE;
    }
    else if (scan.m_scanGCI > 0 &&
             !(thbits & Tuple_header::LCP_SKIP))
    {
      jam();
      /**
       * We have found a row which is free, we are however scanning
       * CHANGED ROWS pages and thus we need to insert a DELETE by
       * ROWID in LCP since the page was deleted since the last
       * LCP was executed. We check that LCP_SKIP bit isn't set, if
       * LCP_SKIP bit is set it means that the tuple was deleted
       * since the LCP started and we have already recorded the
       * row present at start of LCP when the tuple was deleted.
       *
       * If we delete it after LCP start we will certainly set
       * the GCI on the record > scanGCI, so it is an important
       * check for LCP_SKIP bit set.
       */
      scan.m_last_seen = __LINE__;
      return ZSCAN_FOUND_DELETED_ROWID;
    }
    else if (unlikely(thbits & Tuple_header::LCP_SKIP))
    {
      /* Ensure that LCP_SKIP bit is clear before we move on */
      jam();
      /* Coverage tested */
      tuple_header_ptr->m_header_bits =
        thbits & (~Tuple_header::LCP_SKIP);
      DEB_LCP_SKIP(("(%u) 2 Reset LCP_SKIP on tab(%u,%u), row(%u,%u)"
                    ", header: %x",
                    instance(),
                    m_curr_fragptr.p->fragTableId,
                    m_curr_fragptr.p->fragmentId,
                    key.m_page_no,
                    key.m_page_idx,
                    thbits));
      updateChecksum(tuple_header_ptr,
                     m_curr_tabptr.p,
                     thbits,
                     tuple_header_ptr->m_header_bits);
      fix_page->set_change_map(key.m_page_idx);
      jamDebug();
      jamLineDebug((Uint16)key.m_page_idx);
    }
    else
    {
      jamDebug();
      ndbassert(!(thbits & Tuple_header::LCP_SKIP));
      DEB_LCP_SKIP_EXTRA(("(%u)Skipped tab(%u,%u), row(%u,%u),"
                    " foundGCI: %u, scanGCI: %u, header: %x",
                    instance(),
                    m_curr_fragptr.p->fragTableId,
                    m_curr_fragptr.p->fragmentId,
                    key.m_page_no,
                    key.m_page_idx,
                    foundGCI,
                    scan.m_scanGCI,
                    thbits));
      /* Coverage tested */
    }
    jam();
    scan.m_last_seen = __LINE__;
    /* Continue with next row */
    return ZSCAN_FOUND_NEXT_ROW;
  }
  else
  {
    /**
     * When setting LCP_DELETE flag we must also have deleted the
     * row and set rowGCI > scanGCI. So can't be set if we arrive
     * here. Same goes for LCP_SKIP flag.
     */
    ndbassert(!(thbits & Tuple_header::LCP_DELETE));
    if (foundGCI == 0 && scan.m_scanGCI > 0)
    {
      jam();
      /* Coverage tested */
      /* Cannot have LCP_SKIP bit set on rowid's not yet used */
      ndbrequire(!(thbits & Tuple_header::LCP_SKIP));
      scan.m_last_seen = __LINE__;
      return ZSCAN_FOUND_DELETED_ROWID;
    }
    else
    {
      jam();
      /* Coverage tested */
      ndbassert(!(thbits & Tuple_header::LCP_SKIP));
      DEB_LCP_SKIP_EXTRA(("(%u)Skipped tab(%u,%u), row(%u,%u),"
                    " foundGCI: %u, scanGCI: %u, header: %x",
                    instance(),
                    m_curr_fragptr.p->fragTableId,
                    m_curr_fragptr.p->fragmentId,
                    key.m_page_no,
                    key.m_page_idx,
                    foundGCI,
                    scan.m_scanGCI,
                    thbits));
    }
  }
  scan.m_last_seen = __LINE__;
  return ZSCAN_FOUND_NEXT_ROW;
  /* Continue LCP scan, no need to handle this row in this LCP */
}

 /**
 * LCP scanning of CHANGE ROW pages:
 * ---------------------------------
 * The below description is implemented by the setup_change_page_for_scan and
 * handle_scan_change_page_rows methods.
 *
 * When scanning changed pages we only need to record those rows that actually
 * changed. There are two things that we need to ensure here. The first is
 * that we need to ensure that we restore the correct data. The second is that
 * we ensure that each checkpoint maintains structural consistency.
 *
 * To prove that we will restore the correct data we notice that the last
 * change to restore is in a previous checkpoint.
 *
 * In the previous checkpoint we wrote all rows that changed in the first GCI
 * that wasn't completed before we started the GCI or in any later GCI.
 * From this follows that we will definitely have written all changes since
 * the last checkpoint and even more than that.
 *
 * Given that we restore using multiple LCPs there could be a risk that we cut
 * away the LCP part where the changed row was recorded. This is not possible
 * for the following reason:
 * Restore of a page always start at a LCP where the page was fully written.
 * If this happened after the change we know that the record is there.
 * If the change happened after the LCP where ALL changes were recorded we
 * know that the LCP part is part of the restore AND we know that our change is
 * in this LCP part.
 *
 * From this it follows that we will restore the correct data since no changes
 * will be missing from the restored data.
 *
 * Next we need to verify that maintain structural consistency.This means that
 * we must restore exactly the set of rows that was present at the start of
 * the LCP that we are restoring.
 *
 * To maintain this we need to ensure that any INSERTs that happened after
 * start of the previous LCP but before we scanned this row is not missed due
 * to that no changes occurred in this page since we last scanned it. To ensure
 * that we don't miss those rows we will notice that those rows will always
 * be marked with an LCP_DELETE flag for CHANGE pages. This means that when we
 * encounter a row with this flag we need to set the bit in the change map to
 * ensure that this row is recorded in the next LCP.
 *
 * Next we need to handle DELETEs that occur after the LCP started but before
 * we scanned the page. All these rows have the LCP_SKIP bit set. This means
 * that when we encounter the LCP_SKIP for CHANGE pages we should ensure that
 * the row is checked also in the next LCP by setting the change map to
 * indicate this.
 *
 * Finally if there are so many deletes that the state on the page is deleted
 * since the page is dropped, this we need not worry about since this is
 * handled in the same manner as the original partial LCP solution. So the
 * proof of this applies.
 *
 * Finally UPDATEs that occur after the LCP start but before we scan the row
 * will be recorded in the previous LCP and will not require setting any bits
 * in the change map. This is in line with normal behaviour of the LCPs, the
 * LCP is structurally consistent with the start of the LCP (the exact same
 * set of rows exists that existed at start of LCP). The data is however not
 * necessarily consistent since we rely o* the REDO log to bring data
 * consistency.
 *
 * The major benefit of these change map pages comes when an entire page can
 * be skipped. In this case we can change scanning hundreds of rows to a
 * simple check of a small bitmap on the page. To handle very large databases
 * well we implement the bitmaps using a sort of BLOOM filter.
 *
 * We have 8 bits that indicate changes in 4 kB of the page. If this bit isn't
 * set we can skip an entire 4 kB part of page that could easily contain up to
 * a bit more than * one hundred rows.
 *
 * Finally we have a bitmap consisting of 128 bits that each means we can skip
 * 256 bytes at a time when a bit isn't set.
 *
 * One problem with scanning using those bitmaps is that there is a cost
 * attached to skipping rows since it is harder to prefetch data. Thus we will
 * ignore the small area change bitmap when we have enough bits set and simply
 * scan all rows, we will still check the large area change bitmap though
 * also in this case.
 */
Uint32
Dbtup::setup_change_page_for_scan(ScanOp& scan,
                                  Fix_page *fix_page,
                                  Local_key& key,
                                  Uint32 size)
{
  ScanPos& pos = scan.m_scanPos;
  /**
   * This is the first row of the page, we need to decide how
   * to scan this page or possibly even that we don't need to
   * scan it at all since no changes exist on the page. No need
   * to check this once we started scanning the page.
   */
  if (!fix_page->get_any_changes())
  {
    /**
     * We only check this condition for the first row in the page.
     * If we passed this point we will start clearing the bits on
     * the page piece by piece, thus this check is only ok at the
     * first row of the page.
     *
     * No one has touched the page since the start of the
     * previous LCP. It is possible that some updates occurred
     * after the start of the LCP but before the previous LCP
     * scanned this page. These updates will have been recorded
     * in the previous LCP and thus as proved above will be part
     * of the previous LCP that will be part of the recovery
     * processing.
     */
#ifdef VM_TRACE
    Uint32 debug_idx = key.m_page_idx;
    do
    {
      Tuple_header* tuple_header_ptr;
      tuple_header_ptr = (Tuple_header*)&fix_page->m_data[debug_idx];
      Uint32 thbits = tuple_header_ptr->m_header_bits;
      if (thbits & Tuple_header::LCP_DELETE ||
          thbits & Tuple_header::LCP_SKIP)
      {
        g_eventLogger->info("(%u)LCP_DELETE on page with no"
                            " changes tab(%u,%u), page(%u,%u)"
                            ", thbits: %x",
                            instance(),
                            m_curr_fragptr.p->fragTableId,
                            m_curr_fragptr.p->fragmentId,
                            key.m_page_no,
                            key.m_page_idx,
                            thbits);

      }
      ndbassert(!(thbits & Tuple_header::LCP_DELETE));
      ndbassert(!(thbits & Tuple_header::LCP_SKIP));
      debug_idx += size;
    } while ((debug_idx + size) <= Fix_page::DATA_WORDS);
#endif
    DEB_LCP_FILTER(("(%u) tab(%u,%u) page(%u) filtered out",
                    instance(),
                    m_curr_fragptr.p->fragTableId,
                    m_curr_fragptr.p->fragmentId,
                    fix_page->frag_page_id));
    scan.m_last_seen = __LINE__;
    pos.m_get = ScanPos::Get_next_page_mm;
    c_backup->skip_no_change_page();
    return ZSCAN_FOUND_PAGE_END;
  }
  Uint32 num_changes = fix_page->get_num_changes();
  if (num_changes <= 15)
  {
    jam();
    /**
     * We will check every individual small area and also
     * check the large areas. There are only a few areas
     * that actually contain changes.
     * In this case we will not use any prefetches since
     * it is hard to predict which cache lines we will
     * actually read.
     *
     * When NDB is used with very large data sizes this
     * will be the most common code path since this only
     * looks at one individual page. If there is
     * 1 TB of data memory this means that we have
     * 32M of 32kB pages and thus the update frequency
     * must be at least 500M updates per LCP for the
     * number of changes to exceed 15 on most pages.
     * This is clearly not going to be the common case.
     *
     * For smaller databases with say 1 GB of data memory
     * there will be only 32k pages and thus around
     * 500k updates per LCP will be sufficient to exceed
     * 15 updates per page in the common case. Thus much
     * more likely.
     *
     * We keep the bits here until we have passed them with
     * the scan. Exactly the same proof that this works on
     * a page level now applies on the row level.
     *
     * Thus when we check the large area bit and find that no
     * changes have occurred we also know that no small area
     * bits are set, so no need to reset those. We know that
     * no one has touched those pages since the start of the
     * last LCP apart possibly from updates that doesn't change
     * structural consistency of the LCP.
     *
     * We initialise both the small area check index and the
     * large area check index to 0 to ensure that we check
     * already at the first row both of those areas.
     */
    pos.m_all_rows = false;
    pos.m_next_small_area_check_idx = 0;
    pos.m_next_large_area_check_idx = 0;
  }
  else
  {
    jam();
    /**
     * There are more than 15 parts that have changed.
     * In this case we expect to gain more from checking
     * all rows since this means that we can prefetch
     * memory to the CPU caches when we scan in linear
     * order.
     *
     * In this case we can clear the small area change map and
     * the large area change map already here since we won't
     * clear any bits during the page scan.
     *
     * With 15 changes or more the likelihhod is very high that all
     * 8 large areas are also set. So we will ignore checking these
     * to avoid extra costs attached to checking this on
     * each row.
     *
     * We set area check indexes to an impossible value to ensure
     * that we don't use those by mistake.
     */
    pos.m_all_rows = true;
    fix_page->clear_small_change_map();
    fix_page->clear_large_change_map();
    pos.m_next_small_area_check_idx = RNIL;
    pos.m_next_large_area_check_idx = RNIL;
  }
  return ZSCAN_FOUND_TUPLE;
}

Uint32
Dbtup::move_to_next_change_page_row(ScanOp & scan,
                                    Fix_page *fix_page,
                                    Tuple_header **tuple_header_ptr,
                                    Uint32 & loop_count,
                                    Uint32 size)
{
  ScanPos& pos = scan.m_scanPos;
  Local_key& key = pos.m_key;
  jam();
  ndbrequire(pos.m_next_large_area_check_idx != RNIL &&
             pos.m_next_small_area_check_idx != RNIL);
  do
  {
    loop_count++;
    if (pos.m_next_large_area_check_idx == key.m_page_idx)
    {
      jamDebug();
      jamLineDebug(Uint16(key.m_page_idx));
      pos.m_next_large_area_check_idx =
        fix_page->get_next_large_idx(key.m_page_idx, size);
      if (!fix_page->get_and_clear_large_change_map(key.m_page_idx))
      {
        jamDebug();
        DEB_LCP_FILTER(("(%u) tab(%u,%u) page(%u) large area filtered"
                        ", start_idx: %u",
                        instance(),
                        m_curr_fragptr.p->fragTableId,
                        m_curr_fragptr.p->fragmentId,
                        fix_page->frag_page_id,
                        key.m_page_idx));

        if (unlikely((pos.m_next_large_area_check_idx + size) >
                      Fix_page::DATA_WORDS))
        {
          jamDebug();
          ndbassert(fix_page->verify_change_maps());
          return ZSCAN_FOUND_PAGE_END;
        }
        jamDebug();
        /**
         * We have moved forward to a new large area. We assume that all
         * small areas we move past don't have their bits set.
         * It is important to start checking immediately the small area
         * since we have no idea if the first small area is to be checked
         * or not.
         */
        Uint32 next_to_check = pos.m_next_large_area_check_idx;
        key.m_page_idx = next_to_check;
        pos.m_next_small_area_check_idx = next_to_check;
        continue;
      }
    }
    if (pos.m_next_small_area_check_idx == key.m_page_idx)
    {
      jamDebug();
      jamLineDebug(Uint16(key.m_page_idx));
      pos.m_next_small_area_check_idx =
        fix_page->get_next_small_idx(key.m_page_idx, size);
      if (!fix_page->get_and_clear_small_change_map(key.m_page_idx))
      {
        jamDebug();
        DEB_LCP_FILTER(("(%u) tab(%u,%u) page(%u) small area filtered"
                        ", start_idx: %u",
                        instance(),
                        m_curr_fragptr.p->fragTableId,
                        m_curr_fragptr.p->fragmentId,
                        fix_page->frag_page_id,
                        key.m_page_idx));
        if (unlikely((pos.m_next_small_area_check_idx + size) >
                      Fix_page::DATA_WORDS))
        {
          jamDebug();
          ndbassert(fix_page->verify_change_maps());
          return ZSCAN_FOUND_PAGE_END;
        }
        jamDebug();
        /**
         * Since 1024 is a multiple of 64 there is no risk that we move
         * ourselves past the next large area check.
         */
        key.m_page_idx = pos.m_next_small_area_check_idx;
        ndbassert(key.m_page_idx <= pos.m_next_large_area_check_idx);
        continue;
      }
    }
    break;
  } while (1);
  (*tuple_header_ptr) = (Tuple_header*)&fix_page->m_data[key.m_page_idx];
  jamDebug();
  jamLineDebug(Uint16(key.m_page_idx));
  Uint32 map_val = (fix_page->m_change_map[3] >> 16);
  jamLineDebug(Uint16(map_val));
  return ZSCAN_FOUND_TUPLE;
}

/**
 * Handling heavy insert and delete activity during LCP scans
 * ----------------------------------------------------------
 * As part of the LCP we need to record all rows that existed at the beginning
 * of the LCP. This means that any rows that are inserted after the LCP
 * started can be skipped. This is a common activity during database load
 * activity, so we ensure that the LCP can run quick in this case to provide
 * much CPU resources for the insert activity. Also important to make good
 * progress on LCPs to ensure that we can free REDO log space to avoid running
 * out of this resource.
 *
 * We use three ways to signal that a row or a set of rows is not needed to
 * record during an LCP.
 *
 * 1) We record the maximum page number at the start of the LCP, we never
 *    need to scan beyond this point, there can only be pages here that
 *    won't need recording in an LCP. We also avoid setting LCP_SKIP bits
 *    on these pages and rows.
 *    This will cover the common case of a small set of pages at the
 *    start of the LCP that grows quickly during the LCP scan.
 *
 * 2) If a page was allocated after the LCP started, then it can only contain
 *    rows that won't need recording in the LCP. If the page number was
 *    within the maximum page number at start of LCP, and beyond the page
 *    currently checked in LCP, then we will record the LCP skip information
 *    in the page header. So when the LCP scan reaches this page it will
 *    quickly move on to the next page since the page didn't have any records
 *    eligible for LCP recording. After skipping the page we clear the LCP
 *    skip flag since the rows should be recorded in the next LCP.
 *
 * 3) In case a row is allocated in a page that existed at start of LCP, then
 *    we record the LCP skip information in the tuple header unless the row
 *    has already been checked by the current LCP. We skip all rows with this
 *    bit set and reset it to ensure that we record it in the next LCP.
 */

bool
Dbtup::scanNext(Signal* signal, ScanOpPtr scanPtr)
{
  ScanOp& scan = *scanPtr.p;
  ScanPos& pos = scan.m_scanPos;
  Local_key& key = pos.m_key;
  const Uint32 bits = scan.m_bits;
  // table
  TablerecPtr tablePtr;
  tablePtr.i = scan.m_tableId;
  ptrCheckGuard(tablePtr, cnoOfTablerec, tablerec);
  Tablerec& table = *tablePtr.p;
  m_curr_tabptr = tablePtr;
  // fragment
  FragrecordPtr fragPtr;
  fragPtr.i = scan.m_fragPtrI;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);
  Fragrecord& frag = *fragPtr.p;
  m_curr_fragptr = fragPtr;
  // tuple found
  Tuple_header* tuple_header_ptr = 0;
  Uint32 thbits = 0;
  Uint32 loop_count = 0;
  Uint32 foundGCI;

  const bool mm_index = (bits & ScanOp::SCAN_DD);
  const bool lcp = (bits & ScanOp::SCAN_LCP);

  const Uint32 size = ((bits & ScanOp::SCAN_VS) == 0) ?
    table.m_offsets[mm_index].m_fix_header_size : 1;
  const Uint32 first = ((bits & ScanOp::SCAN_VS) == 0) ? 0 : 1;

  if (lcp && ! fragPtr.p->m_lcp_keep_list_head.isNull())
  {
    jam();
    /**
     * Handle lcp keep list here too, due to scanCont
     */
    /* Coverage tested */
    handle_lcp_keep(signal, fragPtr, scanPtr.p);
    scan.m_last_seen = __LINE__;
    return false;
  }

  switch(pos.m_get){
  case ScanPos::Get_next_tuple:
    jam();
    key.m_page_idx += size;
    pos.m_get = ScanPos::Get_page;
    pos.m_realpid_mm = RNIL;
    break;
  case ScanPos::Get_tuple:
    jam();
    /**
     * We need to refetch page after timeslice
     */
    pos.m_get = ScanPos::Get_page;
    pos.m_realpid_mm = RNIL;
    break;
  default:
    break;
  }
  
  while (true) {
    switch (pos.m_get) {
    case ScanPos::Get_next_page:
      // move to next page
      jam();
      {
        if (! (bits & ScanOp::SCAN_DD))
          pos.m_get = ScanPos::Get_next_page_mm;
        else
          pos.m_get = ScanPos::Get_next_page_dd;
      }
      continue;
    case ScanPos::Get_page:
      // get real page
      jam();
      {
        if (! (bits & ScanOp::SCAN_DD))
          pos.m_get = ScanPos::Get_page_mm;
        else
          pos.m_get = ScanPos::Get_page_dd;
      }
      continue;
    case ScanPos::Get_next_page_mm:
      // move to next logical TUP page
      jam();
      {
        /**
         * Code for future activation, see  below for more details.
         * bool break_flag;
         * break_flag = false;
         */
        key.m_page_no++;
        if (likely(bits & ScanOp::SCAN_LCP))
        {
          jam();
          /* Coverage tested path */
          /**
           * We could be scanning for a long time and only finding LCP_SKIP
           * records, we need to keep the LCP watchdog aware that we are
           * progressing, so we report each change to a new page by reporting
           * the id of the next page to scan.
           */
          c_backup->update_lcp_pages_scanned(signal,
                      c_lqh->get_scan_api_op_ptr(scan.m_userPtr),
                      key.m_page_no,
                      scan.m_scanGCI,
                      pos.m_lcp_scan_changed_rows_page);
          scan.m_last_seen = __LINE__;
        }
        if (unlikely(key.m_page_no >= frag.m_max_page_cnt))
        {
          if ((bits & ScanOp::SCAN_NR) && (scan.m_endPage != RNIL))
          {
            if (key.m_page_no < scan.m_endPage)
            {
              jam();
              DEB_NR_SCAN(("scanning page %u", key.m_page_no));
              goto cont;
            }
            jam();
            // no more pages, scan ends
            pos.m_get = ScanPos::Get_undef;
            scan.m_state = ScanOp::Last;
            return true;
          }
          else if (bits & ScanOp::SCAN_LCP &&
                   key.m_page_no < scan.m_endPage)
          {
            /**
             * We come here with ScanOp::SCAN_LCP set AND
             * frag.m_max_page_cnt < scan.m_endPage. In this case
             * it is still ok to finish the LCP scan. The missing
             * pages are handled when they are dropped, so before
             * we drop a page we record all entries that needs
             * recording for the LCP. These have been sent to the
             * LCP keep list. Since when we come here the LCP keep
             * list is empty we are done with the scan.
             *
             * We will however continue the scan for LCP scans. The
             * reason is that we might have set the LCP_SCANNED_BIT
             * on pages already dropped. So we need to continue scanning
             * to ensure that all the lcp scanned bits are reset.
             *
             * For the moment this code is unreachable since m_max_page_cnt
             * cannot decrease. Thus m_max_page_cnt cannot be smaller
             * than scan.m_endPage since scan.m_endPage is initialised to
             * m_max_page_cnt at start of scan.
             *
             * This is currently not implemented. So we
             * will make this code path using an ndbrequire instead.
             * 
             * We keep the code as comments to be activated when we implement
             * the possibility to release pages in the directory.
             */
            ndbabort();
            /* We will not scan this page, so reset flag immediately */
            // reset_lcp_scanned_bit(fragPtr.p, key.m_page_no);
            // scan.m_last_seen = __LINE__;
            // break_flag = true;
          }
          else
          {
            // no more pages, scan ends
            pos.m_get = ScanPos::Get_undef;
            scan.m_last_seen = __LINE__;
            scan.m_state = ScanOp::Last;
            return true;
          }
        }
        if (unlikely((bits & ScanOp::SCAN_LCP) &&
                     (key.m_page_no >= scan.m_endPage)))
        {
          jam();
          /**
           * We have arrived at a page number that didn't exist at start of
           * LCP, we can quit the LCP scan since we cannot find any more
           * pages that are containing rows to be saved in LCP.
           */
          // no more pages, scan ends
          pos.m_get = ScanPos::Get_undef;
          scan.m_last_seen = __LINE__;
          scan.m_state = ScanOp::Last;
          return true;
        }
        /**
         * Activate this code if we implement support for decreasing
         * frag.m_max_page_cnt
         *
         * if (break_flag)
         * {
         * jam();
         * pos.m_get = ScanPos::Get_next_page_mm;
         * scan.m_last_seen = __LINE__;
         * break; // incr loop count
         * }
         */
    cont:
        key.m_page_idx = first;
        pos.m_get = ScanPos::Get_page_mm;
        // clear cached value
        pos.m_realpid_mm = RNIL;
      }
      /*FALLTHRU*/
    case ScanPos::Get_page_mm:
      // get TUP real page
      {
        PagePtr pagePtr;
        loop_count+= 4;
        if (pos.m_realpid_mm == RNIL)
        {
          Uint32 *next_ptr, *prev_ptr;
          pos.m_realpid_mm = getRealpidScan(fragPtr.p,
                                            key.m_page_no,
                                            &next_ptr,
                                            &prev_ptr);
          if (bits & ScanOp::SCAN_LCP)
          {
            jam();
            Uint32 ret_val = prepare_lcp_scan_page(scan,
                                                   key,
                                                   next_ptr,
                                                   prev_ptr);
            if (ret_val == ZSCAN_FOUND_PAGE_END)
              break;
            else if (ret_val == ZSCAN_FOUND_DROPPED_CHANGE_PAGE)
             goto record_dropped_change_page;
            /* else continue */
          }
          else if (unlikely(pos.m_realpid_mm == RNIL))
          {
            jam();
            if (bits & ScanOp::SCAN_NR)
            {
              jam();
              goto nopage;
            }
            pos.m_get = ScanPos::Get_next_page_mm;
            break; // incr loop count
          }
          else
          {
            jam();
          }
        }
        else
        {
          jam();
        }
	c_page_pool.getPtr(pagePtr, pos.m_realpid_mm);
        /**
         * We are in the process of performing a Full table scan, this can be
         * either due to a user requesting a full table scan, it can also be
         * as part of Node Recovery where we are assisting the starting node
         * to be synchronized (SCAN_NR set) and it is also used for LCP scans
         * (SCAN_LCP set).
         * 
         * We know that we will touch all cache lines where there is a tuple
         * header and all scans using main memory pages are done on the fixed
         * pages. To speed up scan processing we will prefetch such that we
         * always are a few tuples ahead. We scan ahead 4 tuples here and then
         * we scan yet one more ahead at each new tuple we get to. We only need
         * initialise by scanning 3 rows ahead since we will immediately fetch
         * the fourth one before looking at the first row.
         *
         * PREFETCH_SCAN_TUPLE:
         */
        if (likely((key.m_page_idx + (size * 3)) <= Fix_page::DATA_WORDS))
        {
          struct Tup_fixsize_page *page_ptr =
            (struct Tup_fixsize_page*)pagePtr.p;
          NDB_PREFETCH_READ(page_ptr->get_ptr(key.m_page_idx,
                                              size));
          NDB_PREFETCH_READ(page_ptr->get_ptr(key.m_page_idx + size,
                                              size));
          NDB_PREFETCH_READ(page_ptr->get_ptr(key.m_page_idx + (size * 2),
                                              size));
        }
        if (bits & ScanOp::SCAN_LCP)
        {
          if (pagePtr.p->is_page_to_skip_lcp())
          {
            Uint32 ret_val = handle_lcp_skip_page(scan,
                                                  key,
                                                  pagePtr.p);
            if (ret_val == ZSCAN_FOUND_PAGE_END)
            {
              jamDebug();
              break;
            }
            else
            {
              jamDebug();
              assert(ret_val == ZSCAN_FOUND_DROPPED_CHANGE_PAGE);
              goto record_dropped_change_page;
            }
          }
          else if (pos.m_lcp_scan_changed_rows_page)
          {
            /* CHANGE page is accessed */
            if (key.m_page_idx == 0)
            {
              jamDebug();
              /* First access of a CHANGE page */
              Uint32 ret_val = setup_change_page_for_scan(scan,
                                                          (Fix_page*)pagePtr.p,
                                                          key,
                                                          size);
              if (ret_val == ZSCAN_FOUND_PAGE_END)
              {
                jamDebug();
                /* No changes found on page level bitmaps */
                break;
              }
              else
              {
                ndbassert(ret_val == ZSCAN_FOUND_TUPLE);
              }
            }
          }
          else
          {
            /* LCP ALL page is accessed */
            jamDebug();
            /**
             * Make sure those values have defined values if we were to enter
             * the wrong path for some reason. These values will lead to a
             * crash if we try to run the CHANGE page code for an ALL page.
             */
            pos.m_all_rows = false;
            pos.m_next_small_area_check_idx = RNIL;
            pos.m_next_large_area_check_idx = RNIL;
          }
        }
        /* LCP normal case 4a) above goes here */

    nopage:
        pos.m_page = pagePtr.p;
        pos.m_get = ScanPos::Get_tuple;
      }
      continue;
    case ScanPos::Get_next_page_dd:
      // move to next disk page
      jam();
      {
        Disk_alloc_info& alloc = frag.m_disk_alloc_info;
        Local_fragment_extent_list list(c_extent_pool, alloc.m_extent_list);
        Ptr<Extent_info> ext_ptr;
        c_extent_pool.getPtr(ext_ptr, pos.m_extent_info_ptr_i);
        Extent_info* ext = ext_ptr.p;
        key.m_page_no++;
        if (key.m_page_no >= ext->m_first_page_no + alloc.m_extent_size) {
          // no more pages in this extent
          jam();
          if (! list.next(ext_ptr)) {
            // no more extents, scan ends
            jam();
            pos.m_get = ScanPos::Get_undef;
            scan.m_state = ScanOp::Last;
            return true;
          } else {
            // move to next extent
            jam();
            pos.m_extent_info_ptr_i = ext_ptr.i;
            ext = c_extent_pool.getPtr(pos.m_extent_info_ptr_i);
            key.m_file_no = ext->m_key.m_file_no;
            key.m_page_no = ext->m_first_page_no;
          }
        }
        key.m_page_idx = first;
        pos.m_get = ScanPos::Get_page_dd;
        /*
          read ahead for scan in disk order
          do read ahead every 8:th page
        */
        if ((bits & ScanOp::SCAN_DD) &&
            (((key.m_page_no - ext->m_first_page_no) & 7) == 0))
        {
          jam();
          // initialize PGMAN request
          Page_cache_client::Request preq;
          preq.m_page = pos.m_key;
          preq.m_callback = TheNULLCallback;

          // set maximum read ahead
          Uint32 read_ahead = m_max_page_read_ahead;

          while (true)
          {
            // prepare page read ahead in current extent
            Uint32 page_no = preq.m_page.m_page_no;
            Uint32 page_no_limit = page_no + read_ahead;
            Uint32 limit = ext->m_first_page_no + alloc.m_extent_size;
            if (page_no_limit > limit)
            {
              jam();
              // read ahead crosses extent, set limit for this extent
              read_ahead = page_no_limit - limit;
              page_no_limit = limit;
              // and make sure we only read one extra extent next time around
              if (read_ahead > alloc.m_extent_size)
                read_ahead = alloc.m_extent_size;
            }
            else
            {
              jam();
              read_ahead = 0; // no more to read ahead after this
            }
            // do read ahead pages for this extent
            while (page_no < page_no_limit)
            {
              // page request to PGMAN
              jam();
              preq.m_page.m_page_no = page_no;
              preq.m_table_id = frag.fragTableId;
              preq.m_fragment_id = frag.fragmentId;
              int flags = Page_cache_client::DISK_SCAN;
              // ignore result
              Page_cache_client pgman(this, c_pgman);
              pgman.get_page(signal, preq, flags);
              jamEntry();
              page_no++;
            }
            if (!read_ahead || !list.next(ext_ptr))
            {
              // no more extents after this or read ahead done
              jam();
              break;
            }
            // move to next extent and initialize PGMAN request accordingly
            Extent_info* ext = c_extent_pool.getPtr(ext_ptr.i);
            preq.m_page.m_file_no = ext->m_key.m_file_no;
            preq.m_page.m_page_no = ext->m_first_page_no;
          }
        } // if ScanOp::SCAN_DD read ahead
      }
      /*FALLTHRU*/
    case ScanPos::Get_page_dd:
      // get global page in PGMAN cache
      jam();
      {
        // check if page is un-allocated or empty
	if (likely(! (bits & ScanOp::SCAN_NR)))
	{
          D("Tablespace_client - scanNext");
	  Tablespace_client tsman(signal, this, c_tsman,
                         frag.fragTableId, 
                         frag.fragmentId,
                         c_lqh->getCreateSchemaVersion(frag.fragTableId),
                         frag.m_tablespace_id);
	  unsigned uncommitted, committed;
	  uncommitted = committed = ~(unsigned)0;
	  int ret = tsman.get_page_free_bits(&key, &uncommitted, &committed);
	  ndbrequire(ret == 0);
	  if (committed == 0 && uncommitted == 0) {
	    // skip empty page
	    jam();
	    pos.m_get = ScanPos::Get_next_page_dd;
	    break; // incr loop count
	  }
	}
        // page request to PGMAN
        Page_cache_client::Request preq;
        preq.m_page = pos.m_key;
        preq.m_table_id = frag.fragTableId;
        preq.m_fragment_id = frag.fragmentId;
        preq.m_callback.m_callbackData = scanPtr.i;
        preq.m_callback.m_callbackFunction =
          safe_cast(&Dbtup::disk_page_tup_scan_callback);
        int flags = Page_cache_client::DISK_SCAN;
        Page_cache_client pgman(this, c_pgman);
        Ptr<GlobalPage> pagePtr;
        int res = pgman.get_page(signal, preq, flags);
        pagePtr = pgman.m_ptr;
        jamEntry();
        if (res == 0) {
          jam();
          // request queued
          pos.m_get = ScanPos::Get_tuple;
          return false;
        }
        ndbrequire(res > 0);
        pos.m_page = (Page*)pagePtr.p;
      }
      pos.m_get = ScanPos::Get_tuple;
      continue;
      // get tuple
      // move to next tuple
    case ScanPos::Get_next_tuple:
      // move to next fixed size tuple
      jam();
      {
        key.m_page_idx += size;
        pos.m_get = ScanPos::Get_tuple;
      }
      /*FALLTHRU*/
    case ScanPos::Get_tuple:
      // get fixed size tuple
      jam();
      if ((bits & ScanOp::SCAN_VS) == 0)
      {
        Fix_page* page = (Fix_page*)pos.m_page;
        if (key.m_page_idx + size <= Fix_page::DATA_WORDS) 
	{
	  pos.m_get = ScanPos::Get_next_tuple;
	  if (unlikely((bits & ScanOp::SCAN_NR) &&
              pos.m_realpid_mm == RNIL))
          {
            /**
             * pos.m_page isn't initialized this path, so handle early
             * We're doing a node restart and we are scanning beyond our
             * existing rowid's since starting node had those rowid's
             * defined.
             */
            jam();
            foundGCI = 0;
            goto found_deleted_rowid;
          }
#ifdef VM_TRACE
          if (! (bits & ScanOp::SCAN_DD))
          {
            Uint32 realpid = getRealpidCheck(fragPtr.p, key.m_page_no);
            ndbassert(pos.m_realpid_mm == realpid);
          }
#endif
          tuple_header_ptr = (Tuple_header*)&page->m_data[key.m_page_idx];

          if ((key.m_page_idx + (size * 4)) <= Fix_page::DATA_WORDS)
          {
            /**
             * Continue staying ahead of scan on this page by prefetching
             * a row 4 tuples ahead of this tuple, prefetched the first 3
             * at PREFETCH_SCAN_TUPLE.
             */
            struct Tup_fixsize_page *page_ptr =
              (struct Tup_fixsize_page*)page;
            NDB_PREFETCH_READ(page_ptr->get_ptr(key.m_page_idx + (size * 3),
                                                size));
          }
	  if (likely((! ((bits & ScanOp::SCAN_NR) ||
                         (bits & ScanOp::SCAN_LCP))) ||
                     ((bits & ScanOp::SCAN_LCP) &&
                      !pos.m_lcp_scan_changed_rows_page)))
          {
            jam();
            /**
             * We come here for normal full table scans and also for LCP
             * scans where we scan ALL ROWS pages.
             *
             * We simply check if the row is free, if it isn't then we will
             * handle it. For LCP scans we will also check at found_tuple that
             * the LCP_SKIP bit isn't set. If it is then the rowid was empty
             * at start of LCP. If the rowid is free AND we are scanning an
             * ALL ROWS page then the LCP_SKIP cannot be set, this is set only
             * for CHANGED ROWS pages when deleting tuples.
             *
             * Free rowid's might have existed at start of LCP. This was
             * handled by using the LCP keep list when tuple was deleted.
             * So when we come here we don't have to worry about LCP scanning
             * those rows.
             *
             * LCP_DELETE flag can never be set on ALL ROWS pages.
             *
             * The state Tuple_header::ALLOC means that the row is being
             * inserted, it thus have no current committed state and is
             * thus here equivalent to the FREE state for LCP scans.
             */
            thbits = tuple_header_ptr->m_header_bits;
            if ((bits & ScanOp::SCAN_LCP) &&
                (thbits & Tuple_header::LCP_DELETE))
            {
              g_eventLogger->info("(%u)LCP_DELETE on tab(%u,%u), row(%u,%u)"
                                  " ALL ROWS page, header: %x",
                                  instance(),
                                  fragPtr.p->fragTableId,
                                  fragPtr.p->fragmentId,
                                  key.m_page_no,
                                  key.m_page_idx,
                                  thbits);
              ndbabort();
            }
	    if (! ((thbits & Tuple_header::FREE) ||
                   ((bits & ScanOp::SCAN_LCP) &&
                    (thbits & Tuple_header::ALLOC))))
	    {
              jam();
              scan.m_last_seen = __LINE__;
              goto found_tuple;
	    }
            /**
             * Ensure that LCP_SKIP bit is clear before we move on
             * It could be set if the row was inserted after LCP
             * start and then followed by a delete of the row before
             * we arrive here.
             */
            if ((bits & ScanOp::SCAN_LCP) &&
                (thbits & Tuple_header::LCP_SKIP))
            {
              jam();
              tuple_header_ptr->m_header_bits =
                thbits & (~Tuple_header::LCP_SKIP);
              DEB_LCP_SKIP(("(%u)Reset LCP_SKIP on tab(%u,%u), row(%u,%u)"
                            ", header: %x"
                            ", new header: %x"
                            ", tuple_header_ptr: %p",
                            instance(),
                            fragPtr.p->fragTableId,
                            fragPtr.p->fragmentId,
                            key.m_page_no,
                            key.m_page_idx,
                            thbits,
                            tuple_header_ptr->m_header_bits,
                            tuple_header_ptr));
              updateChecksum(tuple_header_ptr,
                             tablePtr.p,
                             thbits,
                             tuple_header_ptr->m_header_bits);
            }
            scan.m_last_seen = __LINE__;
	  }
	  else if (bits & ScanOp::SCAN_NR)
	  {
            thbits = tuple_header_ptr->m_header_bits;
	    if ((foundGCI = *tuple_header_ptr->get_mm_gci(tablePtr.p)) >
                 scan.m_scanGCI ||
                foundGCI == 0)
	    {
              /**
               * foundGCI == 0 means that the row is initialised but has not
               * yet been committed as part of insert transaction. All other
               * rows have the GCI entry set to last GCI it was changed, this
               * is true for even deleted rows as long as the page is still
               * maintained by the fragment.
               */
	      if (! (thbits & Tuple_header::FREE))
	      {
		jam();
		goto found_tuple;
	      }
	      else
	      {
		goto found_deleted_rowid;
	      }
	    }
	    else if ((thbits & Fix_page::FREE_RECORD) != Fix_page::FREE_RECORD && 
		      tuple_header_ptr->m_operation_ptr_i != RNIL)
	    {
	      jam();
	      goto found_tuple; // Locked tuple...
	      // skip free tuple
	    }
            DEB_NR_SCAN_EXTRA(("(%u)NR_SCAN_SKIP:tab(%u,%u) row(%u,%u),"
                               " recGCI: %u, scanGCI: %u, header: %x",
                               instance(),
                               fragPtr.p->fragTableId,
                               fragPtr.p->fragmentId,
                               key.m_page_no,
                               key.m_page_idx,
                               foundGCI,
                               scan.m_scanGCI,
                               thbits));
	  }
          else
          {
            ndbassert(c_backup->is_partial_lcp_enabled());
            ndbassert((bits & ScanOp::SCAN_LCP) &&
                       pos.m_lcp_scan_changed_rows_page);
            Uint32 ret_val;
            if (!pos.m_all_rows)
            {
              ret_val = move_to_next_change_page_row(scan,
                                                     page,
                                                     &tuple_header_ptr,
                                                     loop_count,
                                                     size);
              if (ret_val == ZSCAN_FOUND_PAGE_END)
              {
                /**
                 * We've finished scanning a page that was using filtering using
                 * the bitmaps on the page. We are ready to set the last LCP
                 * state to A.
                 */
                /* Coverage tested */
                set_last_lcp_state(fragPtr.p,
                                   key.m_page_no,
                                   false /* Set state to A */);
                scan.m_last_seen = __LINE__;
                pos.m_get = ScanPos::Get_next_page;
                break;
              }
            }
            ret_val = handle_scan_change_page_rows(scan,
                                                   page,
                                                   tuple_header_ptr,
                                                   foundGCI);
            if (likely(ret_val == ZSCAN_FOUND_TUPLE))
            {
              thbits = tuple_header_ptr->m_header_bits;
              goto found_tuple;
            }
            else if (ret_val == ZSCAN_FOUND_DELETED_ROWID)
              goto found_deleted_rowid;
            ndbassert(ret_val == ZSCAN_FOUND_NEXT_ROW);
          }
        }
        else
        {
          jam();
          /**
           * We've finished scanning a page, for LCPs we are ready to
           * set the last LCP state to A.
           */
          if (bits & ScanOp::SCAN_LCP)
          {
            jam();
            /* Coverage tested */
            set_last_lcp_state(fragPtr.p,
                               key.m_page_no,
                               false /* Set state to A */);
            if (!pos.m_all_rows)
            {
              ndbassert(page->verify_change_maps());
            }
            scan.m_last_seen = __LINE__;
          }
          // no more tuples on this page
          pos.m_get = ScanPos::Get_next_page;
        }
      }
      else
      {
        jam();
        Var_page * page = (Var_page*)pos.m_page;
        if (key.m_page_idx < page->high_index)
        {
          jam();
          pos.m_get = ScanPos::Get_next_tuple;
          if (!page->is_free(key.m_page_idx))
          {
            tuple_header_ptr = (Tuple_header*)page->get_ptr(key.m_page_idx);
            thbits = tuple_header_ptr->m_header_bits;
            goto found_tuple;
          }
        }
        else
        {
          jam();
          // no more tuples on this page
          pos.m_get = ScanPos::Get_next_page;
          break;
        }
      }
      break; // incr loop count
  found_tuple:
      // found possible tuple to return
      jam();
      {
        // caller has already set pos.m_get to next tuple
        if (likely(! (bits & ScanOp::SCAN_LCP &&
                      thbits & Tuple_header::LCP_SKIP)))
        {
          Local_key& key_mm = pos.m_key_mm;
          if (likely(! (bits & ScanOp::SCAN_DD)))
          {
            key_mm = pos.m_key;
            // real page id is already set
            if (bits & ScanOp::SCAN_LCP)
            {
              c_backup->update_pause_lcp_counter(loop_count);
            }
          }
          else
          {
            tuple_header_ptr->get_base_record_ref(key_mm);
            // recompute for each disk tuple
            pos.m_realpid_mm = getRealpid(fragPtr.p, key_mm.m_page_no);
          }
          // TUPKEYREQ handles savepoint stuff
          scan.m_state = ScanOp::Current;
          return true;
        }
        else
        {
          jam();
          /* Clear LCP_SKIP bit so that it will not show up in next LCP */
          tuple_header_ptr->m_header_bits =
            thbits & ~(Uint32)Tuple_header::LCP_SKIP;

          DEB_LCP_SKIP(("(%u) 3 Reset LCP_SKIP on tab(%u,%u), row(%u,%u)"
                        ", header: %x",
                        instance(),
                        fragPtr.p->fragTableId,
                        fragPtr.p->fragmentId,
                        key.m_page_no,
                        key.m_page_idx,
                        thbits));

          updateChecksum(tuple_header_ptr,
                         tablePtr.p,
                         thbits,
                         tuple_header_ptr->m_header_bits);
          scan.m_last_seen = __LINE__;
        }
      }
      break;

  record_dropped_change_page:
      {
        ndbassert(c_backup->is_partial_lcp_enabled());
        c_backup->update_pause_lcp_counter(loop_count);
        record_delete_by_pageid(signal,
                                frag.fragTableId,
                                frag.fragmentId,
                                scan,
                                key.m_page_no,
                                size,
                                true);
        return false;
      }

  found_deleted_rowid:

      ndbrequire((bits & ScanOp::SCAN_NR) ||
                 (bits & ScanOp::SCAN_LCP));
      if (!(bits & ScanOp::SCAN_LCP && pos.m_is_last_lcp_state_D))
      {
        ndbassert(bits & ScanOp::SCAN_NR ||
                  pos.m_lcp_scan_changed_rows_page);

        Local_key& key_mm = pos.m_key_mm;
        if (! (bits & ScanOp::SCAN_DD))
        {
          jam();
          key_mm = pos.m_key;
          // caller has already set pos.m_get to next tuple
          // real page id is already set
        }
        else
        {
          jam();
          /**
           * Currently dead code since NR scans never use Disk data scans.
           */
          ndbassert(bits & ScanOp::SCAN_NR);
          tuple_header_ptr->get_base_record_ref(key_mm);
          // recompute for each disk tuple
          pos.m_realpid_mm = getRealpid(fragPtr.p, key_mm.m_page_no);
  
          Fix_page *mmpage = (Fix_page*)c_page_pool.getPtr(pos.m_realpid_mm);
          tuple_header_ptr =
            (Tuple_header*)(mmpage->m_data + key_mm.m_page_idx);
          if ((foundGCI = *tuple_header_ptr->get_mm_gci(tablePtr.p)) >
               scan.m_scanGCI ||
              foundGCI == 0)
          {
            thbits = tuple_header_ptr->m_header_bits;
            if (! (thbits & Tuple_header::FREE))
            {
              jam();
              break;
            }
            jam();
          }
        }
        /**
         * This code handles Node recovery, the row might still exist at the
         * starting node although it no longer exists at this live node. We
         * send a DELETE by ROWID to the starting node.
         *
         * This code is also used by LCPs to record deleted row ids.
         */
        c_backup->update_pause_lcp_counter(loop_count);
        record_delete_by_rowid(signal,
                               frag.fragTableId,
                               frag.fragmentId,
                               scan,
                               pos.m_key_mm,
                               foundGCI,
                               true);
        // TUPKEYREQ handles savepoint stuff
        return false;
      }
      scan.m_last_seen = __LINE__;
      break; // incr loop count
    default:
      ndbabort();
    }
    loop_count+= 4;
    if (loop_count >= 512)
    {
      jam();
      if (bits & ScanOp::SCAN_LCP)
      {
        jam();
        c_backup->update_pause_lcp_counter(loop_count);
        if (!c_backup->check_pause_lcp())
        {
          loop_count = 0;
          continue;
        }
        c_backup->pausing_lcp(5,loop_count);
      }
      break;
    }
  }
  // TODO: at drop table we have to flush and terminate these
  jam();
  scan.m_last_seen = __LINE__;
  signal->theData[0] = ZTUP_SCAN;
  signal->theData[1] = scanPtr.i;
  if (!c_lqh->rt_break_is_scan_prioritised(scan.m_userPtr))
  {
    jam();
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
  }
  else
  {
    /**
     * Sending with bounded delay means that we allow all signals in job buffer
     * to be executed until the maximum is arrived at which is currently 100.
     * So sending with bounded delay means that we get more predictable delay.
     * It might be longer than with priority B, but it will never be longer
     * than 100 signals.
     */
    jam();
//#ifdef VM_TRACE
    c_debug_count++;
    if (c_debug_count % 10000 == 0)
    {
      DEB_LCP_DELAY(("(%u)TupScan delayed 10000 times", instance()));
    }
//#endif
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, BOUNDED_DELAY, 2);
  }
  return false;
}

void
Dbtup::record_delete_by_rowid(Signal *signal,
                              Uint32 tableId,
                              Uint32 fragmentId,
                              ScanOp &scan,
                              Local_key &key,
                              Uint32 foundGCI,
                              bool set_scan_state)
{
  const Uint32 bits = scan.m_bits;
  DEB_LCP_DEL_EXTRA(("(%u)Delete by rowid tab(%u,%u), row(%u,%u)",
                     instance(),
                     tableId,
                     fragmentId,
                     key.m_page_no,
                     key.m_page_idx));
  NextScanConf* const conf = (NextScanConf*)signal->getDataPtrSend();
  conf->scanPtr = scan.m_userPtr;
  conf->accOperationPtr = (bits & ScanOp::SCAN_LCP) ? Uint32(-1) : RNIL;
  conf->fragId = fragmentId;
  conf->localKey[0] = key.m_page_no;
  conf->localKey[1] = key.m_page_idx;
  conf->gci = foundGCI;
  Uint32 blockNo = refToMain(scan.m_userRef);
  if (set_scan_state)
    scan.m_state = ScanOp::Next;
  EXECUTE_DIRECT(blockNo,
                 GSN_NEXT_SCANCONF,
                 signal,
                 NextScanConf::SignalLengthNoKeyInfo);
  jamEntry();
}

void
Dbtup::record_delete_by_pageid(Signal *signal,
                               Uint32 tableId,
                               Uint32 fragmentId,
                               ScanOp &scan,
                               Uint32 page_no,
                               Uint32 record_size,
                               bool set_scan_state)
{
  DEB_LCP_DEL_EXTRA(("(%u)Delete by pageid tab(%u,%u), page(%u)",
                     instance(),
                     tableId,
                     fragmentId,
                     page_no));
  jam();
  /**
   * Set page_idx to flag to LQH that it is a
   * DELETE by PAGEID, this also ensures that we go to the next
   * page when we return to continue the LCP scan.
   */
  Uint32 page_idx = ZNIL;

  NextScanConf* const conf = (NextScanConf*)signal->getDataPtrSend();
  conf->scanPtr = scan.m_userPtr;
  conf->accOperationPtr = Uint32(-1);
  conf->fragId = fragmentId;
  conf->localKey[0] = page_no;
  conf->localKey[1] = page_idx;
  conf->gci = record_size; /* Used to transport record size */
  Uint32 blockNo = refToMain(scan.m_userRef);
  if (set_scan_state)
    scan.m_state = ScanOp::Next;
  EXECUTE_DIRECT(blockNo,
                 GSN_NEXT_SCANCONF,
                 signal,
                 NextScanConf::SignalLengthNoKeyInfo);
  jamEntry();
}

/**
 * The LCP requires that some rows which are deleted during the main-memory
 * scan of fragments with disk-data parts are included in the main-memory LCP.
 * This is done so that during recovery, the main-memory part can be used to
 * find the disk-data part again, so that it can be deleted during Redo
 * application.
 *
 * This is implemented by copying the row content into
 * 'undo memory' / copy tuple space, and adding it to a per-fragment
 * 'lcp keep list', before deleting it at transaction commit time.
 * The row content is then only reachable via the lcp keep list, and does not
 * cause any ROWID reuse issues (899).
 *
 * The LCP scan treats the fragment's 'lcp keep list' as a top-priority source
 * of rows to be included in the fragment LCP, so rows should only be kept
 * momentarily.
 *
 * As these rows exist solely in DBTUP undo memory, it is not necessary to
 * perform the normal ACC locking protocols etc, but it is necessary to prepare
 * TUP for the coming TUPKEYREQ...
 *
 * The principle behind the LCP keep list is described in more detail in
 * the research paper:
 * Recovery Principles of MySQL Cluster 5.1 presented at VLDB in 2005.
 * The main thought is that we restore the disk data part to the point in time
 * when we start the LCP on the fragment. Thus we need to ensure that any rows
 * that exist at start of LCP also exist in the LCP and vice versa any row
 * that didn't exist at start of LCP doesn't exist in LCP. Updates of rows
 * don't matter since the REDO log application will ensure that the row
 * gets synchronized.
 *
 * An important part of this is to record the number of pages at start of LCP.
 * We don't need to worry about scanning pages deleted during LCP since the
 * LCP keep list ensures that those rows were checkpointed before being
 * deleted.
 */
void
Dbtup::handle_lcp_keep(Signal* signal,
                       FragrecordPtr fragPtr,
                       ScanOp* scanPtrP)
{
  TablerecPtr tablePtr;
  tablePtr.i = scanPtrP->m_tableId;
  ptrCheckGuard(tablePtr, cnoOfTablerec, tablerec);

  ndbassert(!fragPtr.p->m_lcp_keep_list_head.isNull());
  Local_key tmp = fragPtr.p->m_lcp_keep_list_head;
  Uint32 * copytuple = get_copy_tuple_raw(&tmp);
  if (copytuple[0] == FREE_PAGE_RNIL)
  {
    jam();
    ndbassert(c_backup->is_partial_lcp_enabled());
    /* Handle DELETE by ROWID or DELETE by PAGEID */
    Uint32 num_entries = copytuple[4];
    Uint32 page_id = copytuple[5];
    Uint16 *page_index_array = (Uint16*)&copytuple[6];
    c_backup->change_current_page_temp(page_id);
    if (page_index_array[0] == ZNIL)
    {
      jam();
      /* DELETE by PAGEID */
      const Uint32 size = tablePtr.p->m_offsets[MM].m_fix_header_size;
      Local_key key;
      key.m_page_no = page_id;
      key.m_page_idx = ZNIL;
      ndbrequire(num_entries == 1);
      DEB_LCP_KEEP(("(%u)tab(%u,%u) page(%u): Handle LCP keep DELETE by PAGEID",
                    instance(),
                    fragPtr.p->fragTableId,
                    fragPtr.p->fragmentId,
                    page_id));
      remove_top_from_lcp_keep_list(fragPtr.p, copytuple, tmp);
      c_backup->lcp_keep_delete_by_page_id();
      record_delete_by_pageid(signal,
                              fragPtr.p->fragTableId,
                              fragPtr.p->fragmentId,
                              *scanPtrP,
                              page_id,
                              size,
                              false);
      c_undo_buffer.free_copy_tuple(&tmp);
    }
    else
    {
      jam();
      /* DELETE by ROWID */
      Local_key key;
      key.m_page_no = page_id;
      ndbrequire(num_entries > 0);
      num_entries--;
      key.m_page_no = page_id;
      key.m_page_idx = page_index_array[num_entries];
      copytuple[4] = num_entries;
      c_backup->lcp_keep_delete_row();
      DEB_LCP_KEEP(("(%u)tab(%u,%u) page(%u,%u): "
                    "Handle LCP keep DELETE by ROWID",
                    instance(),
                    fragPtr.p->fragTableId,
                    fragPtr.p->fragmentId,
                    key.m_page_no,
                    key.m_page_idx));
      if (num_entries == 0)
      {
        jam();
        remove_top_from_lcp_keep_list(fragPtr.p, copytuple, tmp);
      }
      record_delete_by_rowid(signal,
                             fragPtr.p->fragTableId,
                             fragPtr.p->fragmentId,
                             *scanPtrP,
                             key,
                             0,
                             false);
      if (num_entries == 0)
      {
        jam();
        c_undo_buffer.free_copy_tuple(&tmp);
      }
    }
  }
  else
  {
    jam();
    /**
     * tmp points to copy tuple. We need real page id to change to correct
     * current page temporarily. This can be found in copytuple[0]
     * where handle_lcp_keep_commit puts it.
     */
    c_backup->change_current_page_temp(copytuple[0]);
    c_backup->lcp_keep_row();
    remove_top_from_lcp_keep_list(fragPtr.p, copytuple, tmp);
    DEB_LCP_KEEP(("(%u)tab(%u,%u) row(%u,%u) page(%u,%u): Handle LCP keep"
                  " insert entry",
                  instance(),
                  fragPtr.p->fragTableId,
                  fragPtr.p->fragmentId,
                  copytuple[0],
                  copytuple[1],
                  tmp.m_page_no,
                  tmp.m_page_idx));
    Local_key save = tmp;
    setCopyTuple(tmp.m_page_no, tmp.m_page_idx);
    prepareTUPKEYREQ(tmp.m_page_no, tmp.m_page_idx, fragPtr.i);
    NextScanConf* const conf = (NextScanConf*)signal->getDataPtrSend();
    conf->scanPtr = scanPtrP->m_userPtr;
    conf->accOperationPtr = (Uint32)-1;
    conf->fragId = fragPtr.p->fragmentId;
    conf->localKey[0] = tmp.m_page_no;
    conf->localKey[1] = tmp.m_page_idx;
    Uint32 blockNo = refToMain(scanPtrP->m_userRef);
    EXECUTE_DIRECT(blockNo,
                   GSN_NEXT_SCANCONF,
                   signal,
                   NextScanConf::SignalLengthNoGCI);
    c_undo_buffer.free_copy_tuple(&save);
  }
}

void
Dbtup::remove_top_from_lcp_keep_list(Fragrecord *fragPtrP,
                                     Uint32 *copytuple,
                                     Local_key tmp)
{
  memcpy(&fragPtrP->m_lcp_keep_list_head,
         copytuple+2,
         sizeof(Local_key));

  if (fragPtrP->m_lcp_keep_list_head.isNull())
  {
    jam();
    DEB_LCP_KEEP(("(%u) tab(%u,%u) tmp(%u,%u) keep_list(%u,%u):"
                  " LCP keep list empty again",
                  instance(),
                  fragPtrP->fragTableId,
                  fragPtrP->fragmentId,
                  tmp.m_page_no,
                  tmp.m_page_idx,
                  fragPtrP->m_lcp_keep_list_tail.m_page_no,
                  fragPtrP->m_lcp_keep_list_tail.m_page_idx));
    ndbassert(tmp.m_page_no == fragPtrP->m_lcp_keep_list_tail.m_page_no);
    ndbassert(tmp.m_page_idx == fragPtrP->m_lcp_keep_list_tail.m_page_idx);
    fragPtrP->m_lcp_keep_list_tail.setNull();
  }
  else
  {
    jam();
    DEB_LCP_KEEP(("(%u)tab(%u,%u) move LCP keep head(%u,%u),tail(%u,%u)",
                  instance(),
                  fragPtrP->fragTableId,
                  fragPtrP->fragmentId,
                  fragPtrP->m_lcp_keep_list_head.m_page_no,
                  fragPtrP->m_lcp_keep_list_head.m_page_idx,
                  fragPtrP->m_lcp_keep_list_tail.m_page_no,
                  fragPtrP->m_lcp_keep_list_tail.m_page_idx));
  }
}

void
Dbtup::handle_lcp_drop_change_page(Fragrecord *fragPtrP,
                                   Uint32 logicalPageId,
                                   PagePtr pagePtr,
                                   bool delete_by_pageid)
{
  /**
   * We are performing an LCP scan currently. This page is part of the
   * CHANGED ROWS pages. This means that we need to record all rows
   * that was deleted at start of LCP. If the row was deleted since the
   * last LCP scan then we need to record it as a DELETE by ROWID in
   * the LCP. The rows that was deleted after LCP start have already
   * been handled. Those that have been handled have got the LCP_SKIP
   * bit set in the tuple header. Those not handled we need to check
   * the Row GCI to see if it is either 0 or >= scanGCI. If so then
   * we need to record them as part of LCP.
   *
   * We store all the rowid's we find to record as DELETE by ROWID in
   * in a local data array on the stack before we start writing them
   * into the LCP keep list.
   *
   * The page itself that we are scanning will be returned to the same
   * memory pool as we are allocating copy tuples from. So after
   * scanning the page we will do the following:
   * 1) Acquire a global lock on the NDB memory manager to ensure that
   *    no other thread is allowed to snatch the page from us until
   *    we are sure that we got what we needed.
   * 2) Release the page with the lock held
   * 3) Acquire the needed set of copy tuples (called with a lock flag
   *    set).
   * 4) Release the lock on the NDB memory manager
   *
   * This procedure will guarantee that we have space to record the
   * DELETE by ROWIDs in the LCP keep list.
   *
   * An especially complex case happens when the LCP scan is in the
   * middle of scanning this page. This could happen due to an
   * inopportune real-time break in combination with multiple
   * deletes happening within this real-time break.
   *
   * If page_to_skip_lcp bit was set we will perform delete_by_pageid
   * here. So we need not worry about this flag in call to
   * is_rowid_in_remaining_lcp_set for each row in loop, this call will
   * ensure that we will skip any rows already handled by the LCP scan.
   */
  ScanOpPtr scanPtr;
  TablerecPtr tablePtr;
  c_scanOpPool.getPtr(scanPtr, fragPtrP->m_lcp_scan_op);
  tablePtr.i = fragPtrP->fragTableId;
  ptrCheckGuard(tablePtr, cnoOfTablerec, tablerec);
  Uint32 scanGCI = scanPtr.p->m_scanGCI;
  Uint32 idx = 0; /* First record index */
  Uint32 size = tablePtr.p->m_offsets[MM].m_fix_header_size; /* Row size */
  Fix_page *page = (Fix_page*)pagePtr.p;
  Uint32 found_idx_count = 0;
  ndbrequire(size >= 4);
  Uint16 found_idx[2048]; /* Fixed size header never smaller than 16 bytes */
  DEB_LCP_REL(("(%u)tab(%u,%u)page(%u) handle_lcp_drop_page,"
               " delete_by_page: %u",
               instance(),
               fragPtrP->fragTableId,
               fragPtrP->fragmentId,
               logicalPageId,
               delete_by_pageid));
  if (!delete_by_pageid)
  {
    jam();
    Local_key key;
    /* Coverage tested */
    key.m_page_no = logicalPageId;
    while ((idx + size) <= Fix_page::DATA_WORDS)
    {
      Tuple_header *th = (Tuple_header*)&page->m_data[idx];
      Uint32 thbits = th->m_header_bits;
      Uint32 rowGCI = *th->get_mm_gci(tablePtr.p);
      bool lcp_skip_not_set =
        (thbits & Tuple_header::LCP_SKIP) ? false : true;
      ndbassert(thbits & Tuple_header::FREE);
      ndbassert(!(thbits & Tuple_header::LCP_DELETE) || lcp_skip_not_set);
      /**
       * We ignore LCP_DELETE on row here since if it is set then we also
       * know that LCP_SKIP isn't set, also we know rowGCI > scanGCI since the
       * row was inserted after start of LCP. So we will definitely record it
       * here for DELETE by ROWID.
       */
      key.m_page_idx = idx;
      bool is_in_remaining_lcp_set =
        is_rowid_in_remaining_lcp_set(pagePtr.p,
                                      fragPtrP,
                                      key,
                                      *scanPtr.p,
                                      0);
      if ((rowGCI > scanGCI || rowGCI == 0) &&
          lcp_skip_not_set &&
          is_in_remaining_lcp_set)
      {
        /* Coverage tested */
        jam();
        jamLine((Uint16)idx);
        found_idx[found_idx_count] = idx;
        found_idx_count++;
        DEB_LCP_REL(("(%u)tab(%u,%u)page(%u,%u) Keep_list DELETE_BY_ROWID",
                     instance(),
                     fragPtrP->fragTableId,
                     fragPtrP->fragmentId,
                     logicalPageId,
                     idx));
      }
      else
      {
        /* Coverage tested */
        DEB_LCP_REL(("(%u)tab(%u,%u)page(%u,%u) skipped "
                     "lcp_skip_not_set: %u, rowGCI: %u"
                     " scanGCI: %u, in LCP set: %u",
                     instance(),
                     fragPtrP->fragTableId,
                     fragPtrP->fragmentId,
                     logicalPageId,
                     idx,
                     lcp_skip_not_set,
                     rowGCI,
                     scanGCI,
                     is_in_remaining_lcp_set));
      }
      idx += size;
    }
  }
  else
  {
    jam();
    //ndbassert(false); //COVERAGE TEST
    found_idx_count = 1;
    found_idx[0] = ZNIL; /* Indicates DELETE by PAGEID */
    DEB_LCP_REL(("(%u)tab(%u,%u)page(%u) Keep_list DELETE_BY_PAGEID",
                 instance(),
                 fragPtrP->fragTableId,
                 fragPtrP->fragmentId,
                 logicalPageId));
  }
  Local_key location;
  /**
   * We store the following content into the copy tuple with a set of
   * DELETE by ROWID.
   * 1) Header (4 words)
   * 2) Number of rowids stored (1 word)
   * 3) Page Id (1 word)
   * 4) Array of Page indexes (1/2 word per entry)
   */
  if (found_idx_count == 0)
  {
    /* Nothing to store, all rows were already handled. */
    jam();
    returnCommonArea(pagePtr.i, 1);
    return;
  }
  Uint32 words = 6 + ((found_idx_count + 1) / 2);
  m_ctx.m_mm.lock();
  returnCommonArea(pagePtr.i, 1, true);
  ndbrequire(c_undo_buffer.alloc_copy_tuple(&location, words, true) != 0);
  m_ctx.m_mm.unlock();
  Uint32 * copytuple = get_copy_tuple_raw(&location);
  Local_key flag_key;
  flag_key.m_page_no = FREE_PAGE_RNIL;
  flag_key.m_page_idx = 0;
  flag_key.m_file_no = 0;

  copytuple[4] = found_idx_count;
  copytuple[5] = logicalPageId;
  memcpy(&copytuple[6], &found_idx[0], 2 * found_idx_count);
  insert_lcp_keep_list(fragPtrP,
                       location,
                       copytuple,
                       &flag_key);
}

void
Dbtup::insert_lcp_keep_list(Fragrecord *fragPtrP,
                            Local_key location,
                            Uint32 *copytuple,
                            const Local_key *rowid)
{
  /**
   * Store original row-id in copytuple[0,1]
   * Store next-ptr in copytuple[2,3] (set to RNIL/RNIL)
   */
  assert(sizeof(Local_key) == 8);
  memcpy(copytuple+0, rowid, sizeof(Local_key));
  Local_key nil;
  nil.setNull();
  memcpy(copytuple+2, &nil, sizeof(Local_key));
  DEB_LCP_KEEP(("(%u)tab(%u,%u) Insert LCP keep for row(%u,%u)"
                " from location page(%u,%u)",
                instance(),
                fragPtrP->fragTableId,
                fragPtrP->fragmentId,
                rowid->m_page_no,
                rowid->m_page_idx,
                location.m_page_no,
                location.m_page_idx));

  /**
   * Link in the copy tuple into the LCP keep list.
   */
  if (fragPtrP->m_lcp_keep_list_tail.isNull())
  {
    jam();
    fragPtrP->m_lcp_keep_list_head = location;
  }
  else
  {
    jam();
    Uint32 *tail = get_copy_tuple_raw(&fragPtrP->m_lcp_keep_list_tail);
    Local_key nextptr;
    memcpy(&nextptr, tail+2, sizeof(Local_key));
    ndbrequire(nextptr.isNull());
    memcpy(tail+2, &location, sizeof(Local_key));
  }
  fragPtrP->m_lcp_keep_list_tail = location;
}

void
Dbtup::scanCont(Signal* signal, ScanOpPtr scanPtr)
{
  bool immediate = scanNext(signal, scanPtr);
  if (! immediate) {
    jam();
    // time-slicing again
    return;
  }
  scanReply(signal, scanPtr);
}

void
Dbtup::disk_page_tup_scan_callback(Signal* signal, Uint32 scanPtrI, Uint32 page_i)
{
  ScanOpPtr scanPtr;
  c_scanOpPool.getPtr(scanPtr, scanPtrI);
  ScanOp& scan = *scanPtr.p;
  ScanPos& pos = scan.m_scanPos;
  // get cache page
  Ptr<GlobalPage> gptr;
  m_global_page_pool.getPtr(gptr, page_i);
  pos.m_page = (Page*)gptr.p;
  // continue
  scanCont(signal, scanPtr);
}

void
Dbtup::scanClose(Signal* signal, ScanOpPtr scanPtr)
{
  ScanOp& scan = *scanPtr.p;
  ndbrequire(! (scan.m_bits & ScanOp::SCAN_LOCK_WAIT) && scan.m_accLockOp == RNIL);
  {
    /**
     * unlock all not unlocked by LQH
     * Ensure that LocalDLFifoList is destroyed before calling
     * EXECUTE_DIRECT on NEXT_SCANCONF which might end up
     * creating the same object further down the stack.
     */
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
  // send conf
  scan.m_last_seen = __LINE__;
  Uint32 blockNo = refToMain(scanPtr.p->m_userRef);
  NextScanConf* const conf = (NextScanConf*)signal->getDataPtrSend();
  conf->scanPtr = scanPtr.p->m_userPtr;
  conf->accOperationPtr = RNIL;
  conf->fragId = RNIL;
  releaseScanOp(scanPtr);
  EXECUTE_DIRECT(blockNo,
                 GSN_NEXT_SCANCONF,
                 signal,
                 NextScanConf::SignalLengthNoTuple);
}

void
Dbtup::addAccLockOp(ScanOp& scan, Uint32 accLockOp)
{
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
  lockPtr.p->m_accLockOp = accLockOp;
}

void
Dbtup::removeAccLockOp(ScanOp& scan, Uint32 accLockOp)
{
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

void
Dbtup::stop_lcp_scan(Uint32 tableId, Uint32 fragId)
{
  jamEntry();
  TablerecPtr tablePtr;
  tablePtr.i = tableId;
  ptrCheckGuard(tablePtr, cnoOfTablerec, tablerec);

  FragrecordPtr fragPtr;
  fragPtr.i = RNIL;
  getFragmentrec(fragPtr, fragId, tablePtr.p);
  ndbrequire(fragPtr.i != RNIL);
  Fragrecord& frag = *fragPtr.p;

  ndbrequire(frag.m_lcp_scan_op != RNIL && c_lcp_scan_op != RNIL);
  ScanOpPtr scanPtr;
  c_scanOpPool.getPtr(scanPtr, frag.m_lcp_scan_op);
  ndbrequire(scanPtr.p->m_fragPtrI != RNIL);

  fragPtr.p->m_lcp_scan_op = RNIL;
  scanPtr.p->m_fragPtrI = RNIL;
  scanPtr.p->m_tableId = RNIL;
}

void
Dbtup::releaseScanOp(ScanOpPtr& scanPtr)
{
  FragrecordPtr fragPtr;
  fragPtr.i = scanPtr.p->m_fragPtrI;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);

  if(scanPtr.p->m_bits & ScanOp::SCAN_LCP)
  {
    jam();
    /**
     * Ignore, handled in release_lcp_scan, an LCP scan
     * can happen in several scans, one per LCP file.
     */
  }
  else
  {
    jam();
    Local_ScanOp_list list(c_scanOpPool, fragPtr.p->m_scanList);
    list.release(scanPtr);
  }
}

void
Dbtup::start_lcp_scan(Uint32 tableId,
                      Uint32 fragId,
                      Uint32 & max_page_cnt)
{
  jamEntry();
  TablerecPtr tablePtr;
  tablePtr.i = tableId;
  ptrCheckGuard(tablePtr, cnoOfTablerec, tablerec);

  FragrecordPtr fragPtr;
  fragPtr.i = RNIL;
  getFragmentrec(fragPtr, fragId, tablePtr.p);
  ndbrequire(fragPtr.i != RNIL);
  Fragrecord& frag = *fragPtr.p;
  
  ndbrequire(frag.m_lcp_scan_op == RNIL && c_lcp_scan_op != RNIL);
  frag.m_lcp_scan_op = c_lcp_scan_op;
  ScanOpPtr scanPtr;
  c_scanOpPool.getPtr(scanPtr, frag.m_lcp_scan_op);
  ndbrequire(scanPtr.p->m_fragPtrI == RNIL);
  new (scanPtr.p) ScanOp;
  scanPtr.p->m_fragPtrI = fragPtr.i;
  scanPtr.p->m_tableId = tableId;
  scanPtr.p->m_state = ScanOp::First;
  scanPtr.p->m_last_seen = __LINE__;
  scanPtr.p->m_endPage = frag.m_max_page_cnt;
  max_page_cnt = frag.m_max_page_cnt;

  ndbassert(frag.m_lcp_keep_list_head.isNull());
  ndbassert(frag.m_lcp_keep_list_tail.isNull());
}

void
Dbtup::lcp_frag_watchdog_print(Uint32 tableId, Uint32 fragId)
{
  TablerecPtr tablePtr;
  tablePtr.i = tableId;
  if (tableId > cnoOfTablerec)
  {
    jam();
    return;
  }
  ptrCheckGuard(tablePtr, cnoOfTablerec, tablerec);

  FragrecordPtr fragPtr;
  fragPtr.i = RNIL;
  getFragmentrec(fragPtr, fragId, tablePtr.p);
  ndbrequire(fragPtr.i != RNIL);
  Fragrecord& frag = *fragPtr.p;

  if (c_lcp_scan_op == RNIL)
  {
    jam();
    g_eventLogger->info("No LCP scan ongoing in TUP tab(%u,%u)",
                        tableId, fragId);
    ndbabort();
  }
  else if (frag.m_lcp_scan_op == RNIL)
  {
    jam();
    DEB_LCP(("LCP scan stopped, signal to stop watchdog still in flight tab(%u,%u)",
             tableId, fragId));
  }
  else if (frag.m_lcp_scan_op != c_lcp_scan_op)
  {
    jam();
    g_eventLogger->info("Corrupt internal, LCP scan not on correct tab(%u,%u)",
                        tableId, fragId);
    ndbabort();
  }
  else
  {
    jam();
    ScanOpPtr scanPtr;
    c_scanOpPool.getPtr(scanPtr, frag.m_lcp_scan_op);
    g_eventLogger->info("LCP Frag watchdog: tab(%u,%u), state: %u,"
                        " last seen line %u",
                        tableId, fragId,
                        scanPtr.p->m_state,
                        scanPtr.p->m_last_seen);
  }
}
