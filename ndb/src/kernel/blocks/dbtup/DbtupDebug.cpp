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
#include <RefConvert.hpp>
#include <ndb_limits.h>
#include <pc.hpp>
#include <signaldata/DropTab.hpp>
#include <signaldata/DumpStateOrd.hpp>
#include <signaldata/EventReport.hpp>
#include <Vector.hpp>

#define ljam() { jamLine(30000 + __LINE__); }
#define ljamEntry() { jamEntryLine(30000 + __LINE__); }

/* **************************************************************** */
/* ---------------------------------------------------------------- */
/* ------------------------ DEBUG MODULE -------------------------- */
/* ---------------------------------------------------------------- */
/* **************************************************************** */
void Dbtup::execDEBUG_SIG(Signal* signal) 
{
  PagePtr regPagePtr;
  ljamEntry();
  regPagePtr.i = signal->theData[0];
  ptrCheckGuard(regPagePtr, cnoOfPage, page);
}//Dbtup::execDEBUG_SIG()

#ifdef TEST_MR
#include <time.h>

void startTimer(struct timespec *tp)
{
  clock_gettime(CLOCK_REALTIME, tp);
}//startTimer()

int stopTimer(struct timespec *tp)
{
  double timer_count;
  struct timespec theStopTime;
  clock_gettime(CLOCK_REALTIME, &theStopTime);
  timer_count = (double)(1000000*((double)theStopTime.tv_sec - (double)tp->tv_sec)) + 
                (double)((double)((double)theStopTime.tv_nsec - (double)tp->tv_nsec)/(double)1000);
  return (int)timer_count;
}//stopTimer()

#endif // end TEST_MR

struct Chunk {
  Uint32 pageId;
  Uint32 pageCount;
};

void
Dbtup::reportMemoryUsage(Signal* signal, int incDec){
  signal->theData[0] = EventReport::MemoryUsage;
  signal->theData[1] = incDec;
  signal->theData[2] = sizeof(Page);
  signal->theData[3] = cnoOfAllocatedPages;
  signal->theData[4] = cnoOfPage;
  signal->theData[5] = DBTUP;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 6, JBB);
}

void
Dbtup::execDUMP_STATE_ORD(Signal* signal)
{
  Uint32 type = signal->theData[0];
  if(type == DumpStateOrd::DumpPageMemory){
    reportMemoryUsage(signal, 0);
    return;
  }
  DumpStateOrd * const dumpState = (DumpStateOrd *)&signal->theData[0];

#if 0
  if (type == 100) {
    RelTabMemReq * const req = (RelTabMemReq *)signal->getDataPtrSend();
    req->primaryTableId = 2;
    req->secondaryTableId = RNIL;
    req->userPtr = 2;
    req->userRef = DBDICT_REF;
    sendSignal(cownref, GSN_REL_TABMEMREQ, signal,
               RelTabMemReq::SignalLength, JBB);
    return;
  }//if
  if (type == 101) {
    RelTabMemReq * const req = (RelTabMemReq *)signal->getDataPtrSend();
    req->primaryTableId = 4;
    req->secondaryTableId = 5;
    req->userPtr = 4;
    req->userRef = DBDICT_REF;
    sendSignal(cownref, GSN_REL_TABMEMREQ, signal,
               RelTabMemReq::SignalLength, JBB);
    return;
  }//if
  if (type == 102) {
    RelTabMemReq * const req = (RelTabMemReq *)signal->getDataPtrSend();
    req->primaryTableId = 6;
    req->secondaryTableId = 8;
    req->userPtr = 6;
    req->userRef = DBDICT_REF;
    sendSignal(cownref, GSN_REL_TABMEMREQ, signal,
               RelTabMemReq::SignalLength, JBB);
    return;
  }//if
  if (type == 103) {
    DropTabFileReq * const req = (DropTabFileReq *)signal->getDataPtrSend();
    req->primaryTableId = 2;
    req->secondaryTableId = RNIL;
    req->userPtr = 2;
    req->userRef = DBDICT_REF;
    sendSignal(cownref, GSN_DROP_TABFILEREQ, signal,
               DropTabFileReq::SignalLength, JBB);
    return;
  }//if
  if (type == 104) {
    DropTabFileReq * const req = (DropTabFileReq *)signal->getDataPtrSend();
    req->primaryTableId = 4;
    req->secondaryTableId = 5;
    req->userPtr = 4;
    req->userRef = DBDICT_REF;
    sendSignal(cownref, GSN_DROP_TABFILEREQ, signal,
               DropTabFileReq::SignalLength, JBB);
    return;
  }//if
  if (type == 105) {
    DropTabFileReq * const req = (DropTabFileReq *)signal->getDataPtrSend();
    req->primaryTableId = 6;
    req->secondaryTableId = 8;
    req->userPtr = 6;
    req->userRef = DBDICT_REF;
    sendSignal(cownref, GSN_DROP_TABFILEREQ, signal,
               DropTabFileReq::SignalLength, JBB);
    return;
  }//if
#endif
#ifdef ERROR_INSERT
  if (type == DumpStateOrd::EnableUndoDelayDataWrite) {
    ndbout << "Dbtup:: delay write of datapages for table = " 
	   << dumpState->args[1]<< endl;
    c_errorInsert4000TableId = dumpState->args[1];
    SET_ERROR_INSERT_VALUE(4000);
    return;
  }//if
#endif
#ifdef VM_TRACE
  if (type == 1211){
    ndbout_c("Startar modul test av Page Manager");

    Vector<Chunk> chunks;
    const Uint32 LOOPS = 1000;
    for(Uint32 i = 0; i<LOOPS; i++){

      // Case
      Uint32 c = (rand() % 3);
      const Uint32 free = cnoOfPage - cnoOfAllocatedPages;
      
      Uint32 alloc = 0;
      if(free <= 1){
	c = 0;
	alloc = 1;
      } else 
	alloc = 1 + (rand() % (free - 1));
      
      if(chunks.size() == 0 && c == 0){
	c = 1 + rand() % 2;
      }

      ndbout_c("loop=%d case=%d free=%d alloc=%d", i, c, free, alloc);
      switch(c){ 
      case 0:{ // Release
	const int ch = rand() % chunks.size();
	Chunk chunk = chunks[ch];
	chunks.erase(ch);
	returnCommonArea(chunk.pageId, chunk.pageCount);
      }
	break;
      case 2: { // Seize(n) - fail
	alloc += free;
	// Fall through
      }
      case 1: { // Seize(n) (success)

	Chunk chunk;
	allocConsPages(alloc, chunk.pageCount, chunk.pageId);
	ndbrequire(chunk.pageCount <= alloc);
	if(chunk.pageCount != 0){
	  chunks.push_back(chunk);
	  if(chunk.pageCount != alloc) {
	    ndbout_c("  Tried to allocate %d - only allocated %d - free: %d",
		     alloc, chunk.pageCount, free);
	  }
	} else {
	  ndbout_c("  Failed to alloc %d pages with %d pages free",
		   alloc, free);
	}
	
	for(Uint32 i = 0; i<chunk.pageCount; i++){
	  PagePtr pagePtr;
	  pagePtr.i = chunk.pageId + i;
	  ptrCheckGuard(pagePtr, cnoOfPage, page);
	  pagePtr.p->pageWord[ZPAGE_STATE_POS] = ~ZFREE_COMMON;
	}

	if(alloc == 1 && free > 0)
	  ndbrequire(chunk.pageCount == alloc);
      }
	break;
      }
    }
    while(chunks.size() > 0){
      Chunk chunk = chunks.back();
      returnCommonArea(chunk.pageId, chunk.pageCount);      
      chunks.erase(chunks.size() - 1);
    }
  }
#endif
}//Dbtup::execDUMP_STATE_ORD()

/* ---------------------------------------------------------------- */
/* ---------      MEMORY       CHECK        ----------------------- */
/* ---------------------------------------------------------------- */
void Dbtup::execMEMCHECKREQ(Signal* signal) 
{
  PagePtr regPagePtr;
  DiskBufferSegmentInfoPtr dbsiPtr;
  CheckpointInfoPtr ciPtr;
  UndoPagePtr regUndoPagePtr;
  Uint32* data = &signal->theData[0];

  ljamEntry();
  BlockReference blockref = signal->theData[0];
  Uint32 i;
  for (i = 0; i < 25; i++) {
    ljam();
    data[i] = 0;
  }//for
  for (i = 0; i < 16; i++) {
    regPagePtr.i = cfreepageList[i];
    ljam();
    while (regPagePtr.i != RNIL) {
      ljam();
      ptrCheckGuard(regPagePtr, cnoOfPage, page);
      regPagePtr.i = regPagePtr.p->pageWord[ZPAGE_NEXT_POS];
      data[0]++;
    }//while
  }//for
  regUndoPagePtr.i = cfirstfreeUndoSeg;
  while (regUndoPagePtr.i != RNIL) {
    ljam();
    ptrCheckGuard(regUndoPagePtr, cnoOfUndoPage, undoPage);
    regUndoPagePtr.i = regUndoPagePtr.p->undoPageWord[ZPAGE_NEXT_POS];
    data[1] += ZUB_SEGMENT_SIZE;
  }//while
  ciPtr.i = cfirstfreeLcp;
  while (ciPtr.i != RNIL) {
    ljam();
    ptrCheckGuard(ciPtr, cnoOfLcpRec, checkpointInfo);
    ciPtr.i = ciPtr.p->lcpNextRec;
    data[2]++;
  }//while
  dbsiPtr.i = cfirstfreePdx;
  while (dbsiPtr.i != ZNIL) {
    ljam();
    ptrCheckGuard(dbsiPtr, cnoOfConcurrentWriteOp, diskBufferSegmentInfo);
    dbsiPtr.i = dbsiPtr.p->pdxNextRec;
    data[3]++;
  }//while
  sendSignal(blockref, GSN_MEMCHECKCONF, signal, 25, JBB);
}//Dbtup::memCheck()

// ------------------------------------------------------------------------
// Help function to be used when debugging. Prints out a tuple page.
// printLimit is the number of bytes that is printed out from the page. A 
// page is of size 32768 bytes as of March 2003.
// ------------------------------------------------------------------------
void Dbtup::printoutTuplePage(Uint32 fragid, Uint32 pageid, Uint32 printLimit) 
{
  PagePtr tmpPageP;
  FragrecordPtr tmpFragP;
  TablerecPtr tmpTableP;
  Uint32 tmpTupleSize;

  tmpPageP.i = pageid;
  ptrCheckGuard(tmpPageP, cnoOfPage, page);

  tmpFragP.i = fragid;
  ptrCheckGuard(tmpFragP, cnoOfFragrec, fragrecord);

  tmpTableP.i = tmpFragP.p->fragTableId;
  ptrCheckGuard(tmpTableP, cnoOfTablerec, tablerec);

  tmpTupleSize = tmpTableP.p->tupheadsize;

  ndbout << "Fragid: " << fragid << " Pageid: " << pageid << endl
	 << "----------------------------------------" << endl;

  ndbout << "PageHead : ";
  for (Uint32 i1 = 0; i1 < ZPAGE_HEADER_SIZE; i1++) {
    if (i1 == 3)
      ndbout << (tmpPageP.p->pageWord[i1] >> 16) << "," << (tmpPageP.p->pageWord[i1] & 0xffff) << " ";
    else if (tmpPageP.p->pageWord[i1] == 4059165169u)
      ndbout <<  "F1F1F1F1 ";
    else if (tmpPageP.p->pageWord[i1] == 268435455u)
      ndbout << "RNIL ";
    else
      ndbout << tmpPageP.p->pageWord[i1] << " ";
  }//for
  ndbout << endl;
  for (Uint32 i = ZPAGE_HEADER_SIZE; i < printLimit; i += tmpTupleSize) {
    ndbout << "pagepos " << i << " : ";
	 
    for (Uint32 j = i; j < i + tmpTupleSize; j++) {
      if (tmpPageP.p->pageWord[j] == 4059165169u)
	ndbout <<  "F1F1F1F1 ";
      else if (tmpPageP.p->pageWord[j] == 268435455u)
	ndbout << "RNIL ";
      else
	ndbout << tmpPageP.p->pageWord[j] << " ";
    }//for
    ndbout << endl;
  }//for
}//Dbtup::printoutTuplePage

#ifdef VM_TRACE
NdbOut&
operator<<(NdbOut& out, const Dbtup::Operationrec& op)
{
  out << "[Operationrec " << hex << &op;
  // table
  out << " [tableRef " << dec << op.tableRef << "]";
  out << " [fragId " << dec << op.fragId << "]";
  out << " [fragmentPtr " << hex << op.fragmentPtr << "]";
  // type
  out << " [optype " << dec << op.optype << "]";
  out << " [deleteInsertFlag " << dec << op.deleteInsertFlag << "]";
  out << " [dirtyOp " << dec << op.dirtyOp << "]";
  out << " [interpretedExec " << dec << op.interpretedExec << "]";
  out << " [opSimple " << dec << op.opSimple << "]";
  // state
  out << " [tupleState " << dec << op.tupleState << "]";
  out << " [transstate " << dec << op.transstate << "]";
  out << " [inFragList " << dec << op.inFragList << "]";
  out << " [inActiveOpList " << dec << op.inActiveOpList << "]";
  out << " [undoLogged " << dec << op.undoLogged << "]";
  // links
  out << " [prevActiveOp " << hex << op.prevActiveOp << "]";
  out << " [nextActiveOp " << hex << op.nextActiveOp << "]";
  // tuples
  out << " [tupVersion " << hex << op.tupVersion << "]";
  out << " [fragPageId " << dec << op.fragPageId << "]";
  out << " [pageIndex " << dec << op.pageIndex << "]";
  out << " [realPageId " << hex << op.realPageId << "]";
  out << " [pageOffset " << dec << op.pageOffset << "]";
  out << " [fragPageIdC " << dec << op.fragPageIdC << "]";
  out << " [pageIndexC " << dec << op.pageIndexC << "]";
  out << " [realPageIdC " << hex << op.realPageIdC << "]";
  out << " [pageOffsetC " << dec << op.pageOffsetC << "]";
  // trans
  out << " [transid1 " << hex << op.transid1 << "]";
  out << " [transid2 " << hex << op.transid2 << "]";
  out << "]";
  return out;
}

// uses global tabptr
NdbOut&
operator<<(NdbOut& out, const Dbtup::Th& th)
{
  // ugly
  Dbtup* tup = (Dbtup*)globalData.getBlock(DBTUP);
  const Dbtup::Tablerec& tab = *tup->tabptr.p;
  unsigned i = 0;
  out << "[Th " << hex << &th;
  out << " [op " << hex << th.data[i++] << "]";
  out << " [version " << hex << (Uint16)th.data[i++] << "]";
  if (tab.checksumIndicator)
    out << " [checksum " << hex << th.data[i++] << "]";
  out << " [nullbits";
  for (unsigned j = 0; j < tab.tupNullWords; j++)
    out << " " << hex << th.data[i++];
  out << "]";
  if (tab.GCPIndicator)
    out << " [gcp " << dec << th.data[i++] << "]";
  out << " [data";
  while (i < tab.tupheadsize)
    out << " " << hex << th.data[i++];
  out << "]";
  out << "]";
  return out;
}
#endif
