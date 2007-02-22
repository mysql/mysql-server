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

#define DBTUP_C
#include "Dbtup.hpp"
#include <signaldata/AccScan.hpp>
#include <signaldata/NextScan.hpp>

#undef jam
#undef jamEntry
#define jam() { jamLine(32000 + __LINE__); }
#define jamEntry() { jamEntryLine(32000 + __LINE__); }

void
Dbtup::execACC_SCANREQ(Signal* signal)
{
  jamEntry();
  const AccScanReq reqCopy = *(const AccScanReq*)signal->getDataPtr();
  const AccScanReq* const req = &reqCopy;
  ScanOpPtr scanPtr;
  scanPtr.i = RNIL;
  do {
    // find table and fragments
    TablerecPtr tablePtr;
    tablePtr.i = req->tableId;
    ptrCheckGuard(tablePtr, cnoOfTablerec, tablerec);
    FragrecordPtr fragPtr[2];
    Uint32 fragId = req->fragmentNo << 1;
    fragPtr[0].i = fragPtr[1].i = RNIL;
    getFragmentrec(fragPtr[0], fragId | 0, tablePtr.p);
    getFragmentrec(fragPtr[1], fragId | 1, tablePtr.p);
    ndbrequire(fragPtr[0].i != RNIL && fragPtr[1].i != RNIL);
    Fragrecord& frag = *fragPtr[0].p;
    // seize from pool and link to per-fragment list
    if (! frag.m_scanList.seize(scanPtr)) {
      jam();
      break;
    }
    new (scanPtr.p) ScanOp();
    ScanOp& scan = *scanPtr.p;
    scan.m_state = ScanOp::First;
    scan.m_userPtr = req->senderData;
    scan.m_userRef = req->senderRef;
    scan.m_tableId = tablePtr.i;
    scan.m_fragId = frag.fragmentId;
    scan.m_fragPtrI[0] = fragPtr[0].i;
    scan.m_fragPtrI[1] = fragPtr[1].i;
    scan.m_transId1 = req->transId1;
    scan.m_transId2 = req->transId2;
    // conf
    AccScanConf* const conf = (AccScanConf*)signal->getDataPtrSend();
    conf->scanPtr = req->senderData;
    conf->accPtr = scanPtr.i;
    conf->flag = AccScanConf::ZNOT_EMPTY_FRAGMENT;
    sendSignal(req->senderRef, GSN_ACC_SCANCONF, signal,
        AccScanConf::SignalLength, JBB);
    return;
  } while (0);
  if (scanPtr.i != RNIL) {
    jam();
    releaseScanOp(scanPtr);
  }
  // LQH does not handle REF
  signal->theData[0] = 0x313;
  sendSignal(req->senderRef, GSN_ACC_SCANREF, signal, 1, JBB);
}

void
Dbtup::execNEXT_SCANREQ(Signal* signal)
{
  jamEntry();
  const NextScanReq reqCopy = *(const NextScanReq*)signal->getDataPtr();
  const NextScanReq* const req = &reqCopy;
  ScanOpPtr scanPtr;
  c_scanOpPool.getPtr(scanPtr, req->accPtr);
  ScanOp& scan = *scanPtr.p;
  FragrecordPtr fragPtr;
  fragPtr.i = scan.m_fragPtrI[0];
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);
  switch (req->scanFlag) {
  case NextScanReq::ZSCAN_NEXT:
    jam();
    break;
  case NextScanReq::ZSCAN_NEXT_COMMIT:
    jam();
    break;
  case NextScanReq::ZSCAN_COMMIT:
    jam();
    {
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
  EXECUTE_DIRECT(DBTUP, GSN_ACC_CHECK_SCAN, signal, AccCheckScan::SignalLength);
  jamEntry();
}

void
Dbtup::execACC_CHECK_SCAN(Signal* signal)
{
  jamEntry();
  const AccCheckScan reqCopy = *(const AccCheckScan*)signal->getDataPtr();
  const AccCheckScan* const req = &reqCopy;
  ScanOpPtr scanPtr;
  c_scanOpPool.getPtr(scanPtr, req->accPtr);
  ScanOp& scan = *scanPtr.p;
  FragrecordPtr fragPtr;
  fragPtr.i = scan.m_fragPtrI[0];
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);
  Fragrecord& frag = *fragPtr.p;
  if (req->checkLcpStop == AccCheckScan::ZCHECK_LCP_STOP) {
    jam();
    signal->theData[0] = scan.m_userPtr;
    signal->theData[1] = true;
    EXECUTE_DIRECT(DBLQH, GSN_CHECK_LCP_STOP, signal, 2);
    jamEntry();
    return;
  }
  if (scan.m_state == ScanOp::First) {
    jam();
    scanFirst(signal, scanPtr);
  }
  if (scan.m_state == ScanOp::Next) {
    jam();
    scanNext(signal, scanPtr);
  }
  if (scan.m_state == ScanOp::Locked) {
    jam();
    const PagePos& pos = scan.m_scanPos;
    NextScanConf* const conf = (NextScanConf*)signal->getDataPtrSend();
    conf->scanPtr = scan.m_userPtr;
    conf->accOperationPtr = (Uint32)-1; // no lock returned
    conf->fragId = frag.fragmentId | pos.m_fragBit;
    conf->localKey[0] = (pos.m_pageId << MAX_TUPLES_BITS) |
                        (pos.m_tupleNo << 1);
    conf->localKey[1] = 0;
    conf->localKeyLength = 1;
    unsigned signalLength = 6;
    Uint32 blockNo = refToBlock(scan.m_userRef);
    EXECUTE_DIRECT(blockNo, GSN_NEXT_SCANCONF, signal, signalLength);
    jamEntry();
    // next time look for next entry
    scan.m_state = ScanOp::Next;
    return;
  }
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

void
Dbtup::scanFirst(Signal* signal, ScanOpPtr scanPtr)
{
  ScanOp& scan = *scanPtr.p;
  // set to first fragment, first page, first tuple
  PagePos& pos = scan.m_scanPos;
  pos.m_fragId = scan.m_fragId;
  pos.m_fragBit = 0;
  pos.m_pageId = 0;
  pos.m_tupleNo = 0;
  // just before
  pos.m_match = false;
  // let scanNext() do the work
  scan.m_state = ScanOp::Next;
}

// TODO optimize this + index build
void
Dbtup::scanNext(Signal* signal, ScanOpPtr scanPtr)
{
  ScanOp& scan = *scanPtr.p;
  PagePos& pos = scan.m_scanPos;
  TablerecPtr tablePtr;
  tablePtr.i = scan.m_tableId;
  ptrCheckGuard(tablePtr, cnoOfTablerec, tablerec);
  while (true) {
    // TODO time-slice here after X loops
    jam();
    // get fragment
    if (pos.m_fragBit == 2) {
      jam();
      scan.m_state = ScanOp::Last;
      break;
    }
    ndbrequire(pos.m_fragBit <= 1);
    FragrecordPtr fragPtr;
    fragPtr.i = scan.m_fragPtrI[pos.m_fragBit];
    ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);
    Fragrecord& frag = *fragPtr.p;
    // get page
    PagePtr pagePtr;
    if (pos.m_pageId >= frag.noOfPages) {
      jam();
      pos.m_fragBit++;
      pos.m_pageId = 0;
      pos.m_tupleNo = 0;
      pos.m_match = false;
      continue;
    }
    Uint32 realPageId = getRealpid(fragPtr.p, pos.m_pageId);
    pagePtr.i = realPageId;
    ptrCheckGuard(pagePtr, cnoOfPage, page);
    const Uint32 pageState = pagePtr.p->pageWord[ZPAGE_STATE_POS];
    if (pageState != ZTH_MM_FREE &&
        pageState != ZTH_MM_FULL) {
      jam();
      pos.m_pageId++;
      pos.m_tupleNo = 0;
      pos.m_match = false;
      continue;
    }
    // get next tuple
    if (pos.m_match)
      pos.m_tupleNo++;
    pos.m_match = true;
    const Uint32 tupheadsize = tablePtr.p->tupheadsize;
    Uint32 pageOffset = ZPAGE_HEADER_SIZE + pos.m_tupleNo * tupheadsize;
    if (pageOffset + tupheadsize > ZWORDS_ON_PAGE) {
      jam();
      pos.m_pageId++;
      pos.m_tupleNo = 0;
      pos.m_match = false;
      continue;
    }
    // skip over free tuple
    bool isFree = false;
    if (pageState == ZTH_MM_FREE) {
      jam();
      if ((pagePtr.p->pageWord[pageOffset] >> 16) == tupheadsize) {
        Uint32 nextTuple = pagePtr.p->pageWord[ZFREELIST_HEADER_POS] >> 16;
        while (nextTuple != 0) {
          jam();
          if (nextTuple == pageOffset) {
            jam();
            isFree = true;
            break;
          }
          nextTuple = pagePtr.p->pageWord[nextTuple] & 0xffff;
        }
      }
    }
    if (isFree) {
      jam();
      continue;
    }
    // TODO check for operation and return latest in own tx
    scan.m_state = ScanOp::Locked;
    break;
  }
}

void
Dbtup::scanClose(Signal* signal, ScanOpPtr scanPtr)
{
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
Dbtup::releaseScanOp(ScanOpPtr& scanPtr)
{
  FragrecordPtr fragPtr;
  fragPtr.i = scanPtr.p->m_fragPtrI[0];
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);
  fragPtr.p->m_scanList.release(scanPtr);
}
