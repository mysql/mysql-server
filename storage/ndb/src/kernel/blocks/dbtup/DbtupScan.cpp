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
    FragrecordPtr fragPtr;
    Uint32 fragId = req->fragmentNo;
    fragPtr.i = RNIL;
    getFragmentrec(fragPtr, fragId, tablePtr.p);
    ndbrequire(fragPtr.i != RNIL);
    Fragrecord& frag = *fragPtr.p;
    // seize from pool and link to per-fragment list

    Uint32 bits= 0;
    if(frag.m_lcp_scan_op != RNIL)
    {
      bits |= ScanOp::SCAN_LCP;
      ndbrequire(frag.m_lcp_scan_op == c_lcp_scan_op);
      c_scanOpPool.getPtr(scanPtr, frag.m_lcp_scan_op);
    }
    else 
    {
      LocalDLList<ScanOp> list(c_scanOpPool, frag.m_scanList);
      if (! list.seize(scanPtr)) 
      {
	jam();
	break;
      }
    }
    new (scanPtr.p) ScanOp();
    ScanOp& scan = *scanPtr.p;
    scan.m_state = ScanOp::First;
    // TODO scan disk only if any scanned attribute on disk

    if(! (bits & ScanOp::SCAN_LCP))
    {
      /**
       * Remove this until disk scan has been implemented
       */
      if(tablePtr.p->m_attributes[DD].m_no_of_fixsize > 0 ||
	 tablePtr.p->m_attributes[DD].m_no_of_varsize > 0)
      {
	bits |= ScanOp::SCAN_DD;
	
	if (tablePtr.p->m_attributes[DD].m_no_of_varsize > 0)
	  bits |= ScanOp::SCAN_DD_VS;
      }
    }
    
    if(tablePtr.p->m_attributes[MM].m_no_of_varsize)
    {
      bits |= ScanOp::SCAN_VS;
    }

    scan.m_bits = bits;
    scan.m_userPtr = req->senderData;
    scan.m_userRef = req->senderRef;
    scan.m_tableId = tablePtr.i;
    scan.m_fragId = frag.fragmentId;
    scan.m_fragPtrI = fragPtr.i;
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
  if (scanPtr.i != RNIL)
  {
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
  fragPtr.i = scan.m_fragPtrI;
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
    scanFirst(signal, fragPtr.p, scanPtr);
  }
  if (scan.m_state == ScanOp::Next) {
    jam();
    scanNext(signal, fragPtr.p, scanPtr);
  }
  if (scan.m_state == ScanOp::Locked) {
    jam();
    const PagePos& pos = scan.m_scanPos;
    NextScanConf* const conf = (NextScanConf*)signal->getDataPtrSend();
    conf->scanPtr = scan.m_userPtr;
    conf->accOperationPtr = (Uint32)-1; // no lock returned
    conf->fragId = frag.fragmentId;
    conf->localKey[0] = pos.m_key.ref();
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
Dbtup::scanFirst(Signal*, Fragrecord* fragPtrP, ScanOpPtr scanPtr)
{
  ScanOp& scan = *scanPtr.p;
  // set to first fragment, first page, first tuple
  const Uint32 first_page_idx = scan.m_bits & ScanOp::SCAN_VS ? 1 : 0;
  PagePos& pos = scan.m_scanPos;
  pos.m_key.m_page_no = 0;
  pos.m_key.m_page_idx = first_page_idx;
  // just before
  pos.m_match = false;
  // let scanNext() do the work
  scan.m_state = ScanOp::Next;

  if (scan.m_bits & ScanOp::SCAN_DD)
  {
    pos.m_extent_info_ptr_i = 
      fragPtrP->m_disk_alloc_info.m_extent_list.firstItem;
  }
}

void
Dbtup::scanNext(Signal* signal, Fragrecord* fragPtrP, ScanOpPtr scanPtr)
{
  ScanOp& scan = *scanPtr.p;
  PagePos& pos = scan.m_scanPos;
  Uint32 bits = scan.m_bits;
  Local_key& key = pos.m_key;
  TablerecPtr tablePtr;
  tablePtr.i = scan.m_tableId;
  ptrCheckGuard(tablePtr, cnoOfTablerec, tablerec);
  Fragrecord& frag = *fragPtrP;
  const Uint32 first_page_idx = bits & ScanOp::SCAN_VS ? 1 : 0;
  while (true) {
    // TODO time-slice here after X loops
    jam();
    // get page
    PagePtr pagePtr;
    if (key.m_page_no >= frag.noOfPages) {
      jam();
      scan.m_state = ScanOp::Last;
      break;
    }
    Uint32 realPageId = getRealpid(fragPtrP, key.m_page_no);
    pagePtr.i = realPageId;
    ptrCheckGuard(pagePtr, cnoOfPage, cpage);
    Uint32 pageState = pagePtr.p->page_state;
    // skip empty page
    if (pageState == ZEMPTY_MM) {
      jam();
      key.m_page_no++;
      key.m_page_idx = first_page_idx;
      pos.m_match = false;
      continue;
    }
    // get next tuple
    const Tuple_header* th = 0;
    if (! (bits & ScanOp::SCAN_VS)) {
      Uint32 tupheadsize = tablePtr.p->m_offsets[MM].m_fix_header_size;
      if (pos.m_match)
        key.m_page_idx += tupheadsize;
      pos.m_match = true;
      if (key.m_page_idx + tupheadsize > Fix_page::DATA_WORDS) {
        jam();
        key.m_page_no++;
        key.m_page_idx = first_page_idx;
        pos.m_match = false;
        continue;
      }
      th = (Tuple_header*)&pagePtr.p->m_data[key.m_page_idx];
      // skip over free tuple
      if (th->m_header_bits & Tuple_header::FREE) {
          jam();
          continue;
      }
    } else {
      Var_page* page_ptr = (Var_page*)pagePtr.p;
      if (pos.m_match)
        key.m_page_idx += 1;
      pos.m_match = true;
      if (key.m_page_idx >= page_ptr->high_index) {
        jam();
        key.m_page_no++;
        key.m_page_idx = first_page_idx;
        pos.m_match = false;
        continue;
      }

      Uint32 len= page_ptr->get_entry_len(key.m_page_idx);
      if (len == 0)
      {
        // skip empty slot or 
        jam();
        continue;
      }
      if(len & Var_page::CHAIN)
      {
	// skip varpart chain
	jam();
	continue;
      }
      th = (Tuple_header*)page_ptr->get_ptr(key.m_page_idx);
    }

    if(bits & ScanOp::SCAN_LCP && 
       th->m_header_bits & Tuple_header::LCP_SKIP)
    {
      /**
       * Clear it so that it will show up in next LCP
       */
      ((Tuple_header*)th)->m_header_bits &= ~(Uint32)Tuple_header::LCP_SKIP;
      continue;
    }
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
  fragPtr.i = scanPtr.p->m_fragPtrI;
  ptrCheckGuard(fragPtr, cnoOfFragrec, fragrecord);

  if(! (scanPtr.p->m_bits & ScanOp::SCAN_LCP))
  {
    LocalDLList<ScanOp> list(c_scanOpPool, fragPtr.p->m_scanList);    
    list.release(scanPtr);
  }
  else
  {
    ndbrequire(fragPtr.p->m_lcp_scan_op == scanPtr.i);
    fragPtr.p->m_lcp_scan_op = RNIL;
  }
}

void
Dbtup::execLCP_FRAG_ORD(Signal* signal)
{
  LcpFragOrd* req= (LcpFragOrd*)signal->getDataPtr();
  
  TablerecPtr tablePtr;
  tablePtr.i = req->tableId;
  ptrCheckGuard(tablePtr, cnoOfTablerec, tablerec);

  if(tablePtr.p->m_no_of_disk_attributes)
  {
    jam();
    FragrecordPtr fragPtr;
    Uint32 fragId = req->fragmentId;
    fragPtr.i = RNIL;
    getFragmentrec(fragPtr, fragId, tablePtr.p);
    ndbrequire(fragPtr.i != RNIL);
    Fragrecord& frag = *fragPtr.p;
    
    ndbrequire(frag.m_lcp_scan_op == RNIL && c_lcp_scan_op != RNIL);
    frag.m_lcp_scan_op = c_lcp_scan_op;
    ScanOpPtr scanPtr;
    c_scanOpPool.getPtr(scanPtr, frag.m_lcp_scan_op);
    
    scanFirst(signal, fragPtr.p, scanPtr);
    scanPtr.p->m_state = ScanOp::First;
  }
}
