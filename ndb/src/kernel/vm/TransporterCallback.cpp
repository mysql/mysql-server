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

#include <ndb_global.h>

#include <TransporterCallback.hpp>
#include <TransporterRegistry.hpp>
#include <FastScheduler.hpp>
#include <Emulator.hpp>
#include <ErrorHandlingMacros.hpp>

#include "LongSignal.hpp"

#include <signaldata/EventReport.hpp>
#include <signaldata/TestOrd.hpp>
#include <signaldata/SignalDroppedRep.hpp>
#include <signaldata/DisconnectRep.hpp>

#include "VMSignal.hpp"
#include <NdbOut.hpp>
#include "DataBuffer.hpp"

/**
 * The instance
 */
SectionSegmentPool g_sectionSegmentPool;

bool
import(Ptr<SectionSegment> & first, const Uint32 * src, Uint32 len){
  /**
   * Dummy data used when setting prev.m_nextSegment for first segment of a
   *   section
   */
  Uint32 dummyPrev[4]; 

  first.p = 0;
  if(g_sectionSegmentPool.seize(first)){
    ;
  } else {
    return false;
  }

  first.p->m_sz = len;
  first.p->m_ownerRef = 0;
  
  Ptr<SectionSegment> prevPtr = { (SectionSegment *)&dummyPrev[0], 0 };
  Ptr<SectionSegment> currPtr = first;
  
  while(len > SectionSegment::DataLength){
    prevPtr.p->m_nextSegment = currPtr.i;
    memcpy(&currPtr.p->theData[0], src, 4 * SectionSegment::DataLength);
    src += SectionSegment::DataLength;
    len -= SectionSegment::DataLength;
    prevPtr = currPtr;
    if(g_sectionSegmentPool.seize(currPtr)){
      ;
    } else {
      first.p->m_lastSegment = prevPtr.i;
      return false;
    }
  }

  first.p->m_lastSegment = currPtr.i;
  currPtr.p->m_nextSegment = RNIL;
  memcpy(&currPtr.p->theData[0], src, 4 * len);
  return true;
}

void
linkSegments(Uint32 head, Uint32 tail){
  
  Ptr<SectionSegment> headPtr;
  g_sectionSegmentPool.getPtr(headPtr, head);
  
  Ptr<SectionSegment> tailPtr;
  g_sectionSegmentPool.getPtr(tailPtr, tail);
  
  Ptr<SectionSegment> oldTailPtr;
  g_sectionSegmentPool.getPtr(oldTailPtr, headPtr.p->m_lastSegment);
  
  headPtr.p->m_lastSegment = tailPtr.p->m_lastSegment;
  headPtr.p->m_sz += tailPtr.p->m_sz;
  
  oldTailPtr.p->m_nextSegment = tailPtr.i;
}

void 
copy(Uint32 * & insertPtr, 
     class SectionSegmentPool & thePool, const SegmentedSectionPtr & _ptr){

  Uint32 len = _ptr.sz;
  SectionSegment * ptrP = _ptr.p;
  
  while(len > 60){
    memcpy(insertPtr, &ptrP->theData[0], 4 * 60);
    len -= 60;
    insertPtr += 60;
    ptrP = thePool.getPtr(ptrP->m_nextSegment);
  }
  memcpy(insertPtr, &ptrP->theData[0], 4 * len);
  insertPtr += len;
}

void
copy(Uint32 * dst, SegmentedSectionPtr src){
  copy(dst, g_sectionSegmentPool, src);
}

void
getSections(Uint32 secCount, SegmentedSectionPtr ptr[3]){
  Uint32 tSec0 = ptr[0].i;
  Uint32 tSec1 = ptr[1].i;
  Uint32 tSec2 = ptr[2].i;
  SectionSegment * p;
  switch(secCount){
  case 3:
    p = g_sectionSegmentPool.getPtr(tSec2);
    ptr[2].p = p;
    ptr[2].sz = p->m_sz;
  case 2:
    p = g_sectionSegmentPool.getPtr(tSec1);
    ptr[1].p = p;
    ptr[1].sz = p->m_sz;
  case 1:
    p = g_sectionSegmentPool.getPtr(tSec0);
    ptr[0].p = p;
    ptr[0].sz = p->m_sz;
  case 0:
    return;
  }
  char msg[40];
  sprintf(msg, "secCount=%d", secCount);
  ErrorReporter::handleAssert(msg, __FILE__, __LINE__);
}

void
getSection(SegmentedSectionPtr & ptr, Uint32 i){
  ptr.i = i;
  SectionSegment * p = g_sectionSegmentPool.getPtr(i);
  ptr.p = p;
  ptr.sz = p->m_sz;
}

#define relSz(x) ((x + SectionSegment::DataLength - 1) / SectionSegment::DataLength)

void
release(SegmentedSectionPtr & ptr){
  g_sectionSegmentPool.releaseList(relSz(ptr.sz),
				   ptr.i, 
				   ptr.p->m_lastSegment);
}

void
releaseSections(Uint32 secCount, SegmentedSectionPtr ptr[3]){
  Uint32 tSec0 = ptr[0].i;
  Uint32 tSz0 = ptr[0].sz;
  Uint32 tSec1 = ptr[1].i;
  Uint32 tSz1 = ptr[1].sz;
  Uint32 tSec2 = ptr[2].i;
  Uint32 tSz2 = ptr[2].sz;
  switch(secCount){
  case 3:
    g_sectionSegmentPool.releaseList(relSz(tSz2), tSec2, 
				     ptr[2].p->m_lastSegment);
  case 2:
    g_sectionSegmentPool.releaseList(relSz(tSz1), tSec1, 
				     ptr[1].p->m_lastSegment);
  case 1:
    g_sectionSegmentPool.releaseList(relSz(tSz0), tSec0, 
				     ptr[0].p->m_lastSegment);
  case 0:
    return;
  }
  char msg[40];
  sprintf(msg, "secCount=%d", secCount);
  ErrorReporter::handleAssert(msg, __FILE__, __LINE__);
}

#include <DebuggerNames.hpp>

void
execute(void * callbackObj, 
	SignalHeader * const header, 
	Uint8 prio, 
	Uint32 * const theData,
	LinearSectionPtr ptr[3]){

  const Uint32 secCount = header->m_noOfSections;
  const Uint32 length = header->theLength;

#ifdef TRACE_DISTRIBUTED
  ndbout_c("recv: %s(%d) from (%s, %d)",
	   getSignalName(header->theVerId_signalNumber), 
	   header->theVerId_signalNumber,
	   getBlockName(refToBlock(header->theSendersBlockRef)),
	   refToNode(header->theSendersBlockRef));
#endif
  
  bool ok = true;
  Ptr<SectionSegment> secPtr[3];
  switch(secCount){
  case 3:
    ok &= import(secPtr[2], ptr[2].p, ptr[2].sz);
  case 2:
    ok &= import(secPtr[1], ptr[1].p, ptr[1].sz);
  case 1:
    ok &= import(secPtr[0], ptr[0].p, ptr[0].sz);
  }

  /**
   * Check that we haven't received a too long signal
   */
  ok &= (length + secCount <= 25);
  
  Uint32 secPtrI[3];
  if(ok){
    /**
     * Normal path 
     */
    secPtrI[0] = secPtr[0].i;
    secPtrI[1] = secPtr[1].i;
    secPtrI[2] = secPtr[2].i;

    globalScheduler.execute(header, prio, theData, secPtrI);  
    return;
  }
  
  /**
   * Out of memory
   */
  for(Uint32 i = 0; i<secCount; i++){
    if(secPtr[i].p != 0){
      g_sectionSegmentPool.releaseList(relSz(ptr[i].sz), secPtr[i].i, 
				       secPtr[i].p->m_lastSegment);
    }
  }
  Uint32 gsn = header->theVerId_signalNumber;
  Uint32 len = header->theLength;
  Uint32 newLen= (len > 22 ? 22 : len);
  SignalDroppedRep * rep = (SignalDroppedRep*)theData;
  memmove(rep->originalData, theData, (4 * newLen));
  rep->originalGsn = gsn;
  rep->originalLength = len;
  rep->originalSectionCount = secCount;
  header->theVerId_signalNumber = GSN_SIGNAL_DROPPED_REP;
  header->theLength = newLen + 3;
  header->m_noOfSections = 0;
  globalScheduler.execute(header, prio, theData, secPtrI);    
}

NdbOut & 
operator<<(NdbOut& out, const SectionSegment & ss){
  out << "[ last= " << ss.m_lastSegment << " next= " << ss.nextPool << " ]";
  return out;
}

void
print(SectionSegment * s, Uint32 len, FILE* out){
  for(Uint32 i = 0; i<len; i++){
    fprintf(out, "H\'0x%.8x ", s->theData[i]);
    if(((i + 1) % 6) == 0)
      fprintf(out, "\n");
  }
}

void
print(SegmentedSectionPtr ptr, FILE* out){

  ptr.p = g_sectionSegmentPool.getPtr(ptr.i);
  Uint32 len = ptr.p->m_sz;
  
  fprintf(out, "ptr.i = %d(%p) ptr.sz = %d(%d)\n", ptr.i, ptr.p, len, ptr.sz);
  while(len > SectionSegment::DataLength){
    print(ptr.p, SectionSegment::DataLength, out);
    
    len -= SectionSegment::DataLength;
    fprintf(out, "ptr.i = %d\n", ptr.p->m_nextSegment);
    ptr.p = g_sectionSegmentPool.getPtr(ptr.p->m_nextSegment);
  }
  
  print(ptr.p, len, out);
  fprintf(out, "\n");
}

int
checkJobBuffer() {
  /** 
   * Check to see if jobbbuffers are starting to get full
   * and if so call doJob
   */
  return globalScheduler.checkDoJob();
}

void
reportError(void * callbackObj, NodeId nodeId, TransporterError errorCode){
#ifdef DEBUG_TRANSPORTER
  char buf[255];
  sprintf(buf, "reportError (%d, 0x%x)", nodeId, errorCode);
  ndbout << buf << endl;
#endif

  if(errorCode == TE_SIGNAL_LOST_SEND_BUFFER_FULL){
    ErrorReporter::handleError(ecError,
			       ERR_PROGRAMERROR,
			       "Signal lost, send buffer full",
			       __FILE__,
			       NST_ErrorHandler);
  }

  if(errorCode == TE_SIGNAL_LOST){
    ErrorReporter::handleError(ecError,
			       ERR_PROGRAMERROR,
			       "Signal lost (unknown reason)",
			       __FILE__,
			       NST_ErrorHandler);
  }
  
  if(errorCode & 0x8000){
    reportDisconnect(callbackObj, nodeId, errorCode);
  }
  
  Signal signal;
  memset(&signal.header, 0, sizeof(signal.header));


  if(errorCode & 0x8000)
    signal.theData[0] = EventReport::TransporterError;
  else
    signal.theData[0] = EventReport::TransporterWarning;
  
  signal.theData[1] = nodeId;
  signal.theData[2] = errorCode;
  
  signal.header.theLength = 3;  
  signal.header.theSendersSignalId = 0;
  signal.header.theSendersBlockRef = numberToRef(0, globalData.ownId);
  globalScheduler.execute(&signal, JBA, CMVMI, GSN_EVENT_REP);
}

/**
 * Report average send length in bytes (4096 last sends)
 */
void
reportSendLen(void * callbackObj, 
	      NodeId nodeId, Uint32 count, Uint64 bytes){

  Signal signal;
  memset(&signal.header, 0, sizeof(signal.header));

  signal.header.theLength = 3;
  signal.header.theSendersSignalId = 0;
  signal.header.theSendersBlockRef = numberToRef(0, globalData.ownId);
  signal.theData[0] = EventReport::SendBytesStatistic;
  signal.theData[1] = nodeId;
  signal.theData[2] = (bytes/count);
  globalScheduler.execute(&signal, JBA, CMVMI, GSN_EVENT_REP);
}

/**
 * Report average receive length in bytes (4096 last receives)
 */
void
reportReceiveLen(void * callbackObj, 
		 NodeId nodeId, Uint32 count, Uint64 bytes){

  Signal signal;
  memset(&signal.header, 0, sizeof(signal.header));

  signal.header.theLength = 3;  
  signal.header.theSendersSignalId = 0;
  signal.header.theSendersBlockRef = numberToRef(0, globalData.ownId);
  signal.theData[0] = EventReport::ReceiveBytesStatistic;
  signal.theData[1] = nodeId;
  signal.theData[2] = (bytes/count);
  globalScheduler.execute(&signal, JBA, CMVMI, GSN_EVENT_REP);
}

/**
 * Report connection established
 */

void
reportConnect(void * callbackObj, NodeId nodeId){

  Signal signal;
  memset(&signal.header, 0, sizeof(signal.header));

  signal.header.theLength = 1; 
  signal.header.theSendersSignalId = 0;
  signal.header.theSendersBlockRef = numberToRef(0, globalData.ownId);
  signal.theData[0] = nodeId;
  
  globalScheduler.execute(&signal, JBA, CMVMI, GSN_CONNECT_REP);
}

/**
 * Report connection broken
 */
void
reportDisconnect(void * callbackObj, NodeId nodeId, Uint32 errNo){

  Signal signal;
  memset(&signal.header, 0, sizeof(signal.header));

  signal.header.theLength = DisconnectRep::SignalLength; 
  signal.header.theSendersSignalId = 0;
  signal.header.theSendersBlockRef = numberToRef(0, globalData.ownId);
  signal.header.theTrace = TestOrd::TraceDisconnect;

  DisconnectRep * const  rep = (DisconnectRep *)&signal.theData[0];
  rep->nodeId = nodeId;
  rep->err = errNo;

  globalScheduler.execute(&signal, JBA, CMVMI, GSN_DISCONNECT_REP);
}

void
SignalLoggerManager::printSegmentedSection(FILE * output,
                                           const SignalHeader & sh,
                                           const SegmentedSectionPtr ptr[3],
                                           unsigned i)
{
  fprintf(output, "SECTION %u type=segmented", i);
  if (i >= 3) {
    fprintf(output, " *** invalid ***\n");
    return;
  }
  const Uint32 len = ptr[i].sz;
  SectionSegment * ssp = ptr[i].p;
  Uint32 pos = 0;
  fprintf(output, " size=%u\n", (unsigned)len);
  while (pos < len) {
    if (pos > 0 && pos % SectionSegment::DataLength == 0) {
      ssp = g_sectionSegmentPool.getPtr(ssp->m_nextSegment);
    }
    printDataWord(output, pos, ssp->theData[pos % SectionSegment::DataLength]);
  }
  if (len > 0)
    putc('\n', output);
}

