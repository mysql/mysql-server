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
  c_page_pool.getPtr(regPagePtr);
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
  signal->theData[0] = NDB_LE_MemoryUsage;
  signal->theData[1] = incDec;
  signal->theData[2] = sizeof(Page);
  signal->theData[3] = cnoOfAllocatedPages;
  signal->theData[4] = c_page_pool.getSize();
  signal->theData[5] = DBTUP;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 6, JBB);
}

void
Dbtup::execDUMP_STATE_ORD(Signal* signal)
{
  Uint32 type = signal->theData[0];
  if(type == DumpStateOrd::DumpPageMemory && signal->getLength() == 1){
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
#if defined VM_TRACE && 0
  if (type == 1211){
    ndbout_c("Startar modul test av Page Manager");

    Vector<Chunk> chunks;
    const Uint32 LOOPS = 1000;
    for(Uint32 i = 0; i<LOOPS; i++){

      // Case
      Uint32 c = (rand() % 3);
      const Uint32 free = c_page_pool.getSize() - cnoOfAllocatedPages;
      
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
	  c_page_pool.getPtr(pagePtr);
	  pagePtr.p->page_state = ~ZFREE_COMMON;
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
  TablerecPtr regTabPtr;
  regTabPtr.i = 2;
  ptrCheckGuard(regTabPtr, cnoOfTablerec, tablerec);
  if(tablerec && regTabPtr.p->tableStatus == DEFINED)
    validate_page(regTabPtr.p, 0);

#if 0
  const Dbtup::Tablerec& tab = *tup->tabptr.p;

  PagePtr regPagePtr;
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
      ptrCheckGuard(regPagePtr, cnoOfPage, cpage);
      regPagePtr.i = regPagePtr.p->next_page;
      data[0]++;
    }//while
  }//for
  sendSignal(blockref, GSN_MEMCHECKCONF, signal, 25, JBB);
#endif
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

  c_page_pool.getPtr(tmpPageP, pageid);

  tmpFragP.i = fragid;
  ptrCheckGuard(tmpFragP, cnoOfFragrec, fragrecord);

  tmpTableP.i = tmpFragP.p->fragTableId;
  ptrCheckGuard(tmpTableP, cnoOfTablerec, tablerec);

  ndbout << "Fragid: " << fragid << " Pageid: " << pageid << endl
	 << "----------------------------------------" << endl;

  ndbout << "PageHead : ";
  ndbout << endl;
}//Dbtup::printoutTuplePage

#ifdef VM_TRACE
NdbOut&
operator<<(NdbOut& out, const Dbtup::Operationrec& op)
{
  out << "[Operationrec " << hex << &op;
  // table
  out << " [fragmentPtr " << hex << op.fragmentPtr << "]";
  // type
  out << " [op_type " << dec << op.op_struct.op_type << "]";
  out << " [delete_insert_flag " << dec;
  out << op.op_struct.delete_insert_flag << "]";
  // state
  out << " [tuple_state " << dec << op.op_struct.tuple_state << "]";
  out << " [trans_state " << dec << op.op_struct.trans_state << "]";
  out << " [in_active_list " << dec << op.op_struct.in_active_list << "]";
  // links
  out << " [prevActiveOp " << hex << op.prevActiveOp << "]";
  out << " [nextActiveOp " << hex << op.nextActiveOp << "]";
  // tuples
  out << " [tupVersion " << hex << op.tupVersion << "]";
  out << " [m_tuple_location " << op.m_tuple_location << "]";
  out << " [m_copy_tuple_location " << op.m_copy_tuple_location << "]";
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
  if (tab.m_bits & Dbtup::Tablerec::TR_Checksum)
    out << " [checksum " << hex << th.data[i++] << "]";
  out << " [nullbits";
  for (unsigned j = 0; j < tab.m_offsets[Dbtup::MM].m_null_words; j++)
    out << " " << hex << th.data[i++];
  out << "]";
  out << " [data";
  while (i < tab.m_offsets[Dbtup::MM].m_fix_header_size)
    out << " " << hex << th.data[i++];
  out << "]";
  out << "]";
  return out;
}
#endif

#ifdef VM_TRACE
template class Vector<Chunk>;
#endif
// uses global tabptr

NdbOut&
operator<<(NdbOut& out, const Local_key & key)
{
  out << "[ m_page_no: " << dec << key.m_page_no 
      << " m_file_no: " << dec << key.m_file_no 
      << " m_page_idx: " << dec << key.m_page_idx << "]";
  return out;
}

static
NdbOut&
operator<<(NdbOut& out, const Dbtup::Tablerec::Tuple_offsets& off)
{
  out << "[ null_words: " << (Uint32)off.m_null_words
      << " null off: " << (Uint32)off.m_null_offset
      << " disk_off: " << off.m_disk_ref_offset
      << " var_off: " << off.m_varpart_offset
      << " max_var_off: " << off.m_max_var_offset
      << " ]";

  return out;
}

NdbOut&
operator<<(NdbOut& out, const Dbtup::Tablerec& tab)
{
  out << "[ total_rec_size: " << tab.total_rec_size
      << " checksum: " << !!(tab.m_bits & Dbtup::Tablerec::TR_Checksum)
      << " attr: " << tab.m_no_of_attributes
      << " disk: " << tab.m_no_of_disk_attributes 
      << " mm: " << tab.m_offsets[Dbtup::MM]
      << " [ fix: " << tab.m_attributes[Dbtup::MM].m_no_of_fixsize
      << " var: " << tab.m_attributes[Dbtup::MM].m_no_of_varsize << "]"
    
      << " dd: " << tab.m_offsets[Dbtup::DD]
      << " [ fix: " << tab.m_attributes[Dbtup::DD].m_no_of_fixsize
      << " var: " << tab.m_attributes[Dbtup::DD].m_no_of_varsize << "]"
      << " ]"  << endl;
  return out;
}

NdbOut&
operator<<(NdbOut& out, const AttributeDescriptor& off)
{
  Uint32 word;
  memcpy(&word, &off, 4);
  return out;
}

#include "AttributeOffset.hpp"

NdbOut&
operator<<(NdbOut& out, const AttributeOffset& off)
{
  Uint32 word;
  memcpy(&word, &off, 4);
  out << "[ offset: " << AttributeOffset::getOffset(word)
      << " nullpos: " << AttributeOffset::getNullFlagPos(word);
  if(AttributeOffset::getCharsetFlag(word))
    out << " charset: %d" << AttributeOffset::getCharsetPos(word);
  out << " ]";
  return out;
}

