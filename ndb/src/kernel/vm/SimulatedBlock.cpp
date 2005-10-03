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

#include "SimulatedBlock.hpp"
#include <NdbOut.hpp>
#include <GlobalData.hpp>
#include <Emulator.hpp>
#include <ErrorHandlingMacros.hpp>
#include <TimeQueue.hpp>
#include <TransporterRegistry.hpp>
#include <SignalLoggerManager.hpp>
#include <FastScheduler.hpp>
#include <NdbMem.h>
#include <signaldata/EventReport.hpp>
#include <signaldata/ContinueFragmented.hpp>
#include <signaldata/NodeStateSignalData.hpp>
#include <signaldata/FsRef.hpp>
#include <signaldata/SignalDroppedRep.hpp>
#include <DebuggerNames.hpp>
#include "LongSignal.hpp"

#include <Properties.hpp>
#include "Configuration.hpp"

#define ljamEntry() jamEntryLine(30000 + __LINE__)
#define ljam() jamLine(30000 + __LINE__)

//
// Constructor, Destructor
//
SimulatedBlock::SimulatedBlock(BlockNumber blockNumber,
			       const class Configuration & conf) 
  : theNodeId(globalData.ownId),
    theNumber(blockNumber),
    theReference(numberToRef(blockNumber, globalData.ownId)),
    theConfiguration(conf),
    c_fragmentInfoHash(c_fragmentInfoPool),
    c_linearFragmentSendList(c_fragmentSendPool),
    c_segmentedFragmentSendList(c_fragmentSendPool),
    c_mutexMgr(* this),
    c_counterMgr(* this),
    c_ptrMetaDataCommon(0)
{
  NewVarRef = 0;
  
  globalData.setBlock(blockNumber, this);
  c_fragmentIdCounter = 1;
  c_fragSenderRunning = false;
  
  Properties tmp;
  const Properties * p = &tmp;
  ndbrequire(p != 0);

  Uint32 count = 10;
  char buf[255];

  count = 10;
  BaseString::snprintf(buf, 255, "%s.FragmentSendPool", getBlockName(blockNumber));
  if(!p->get(buf, &count))
    p->get("FragmentSendPool", &count);
  c_fragmentSendPool.setSize(count);

  count = 10;
  BaseString::snprintf(buf, 255, "%s.FragmentInfoPool", getBlockName(blockNumber));
  if(!p->get(buf, &count))
    p->get("FragmentInfoPool", &count);
  c_fragmentInfoPool.setSize(count);

  count = 10;
  BaseString::snprintf(buf, 255, "%s.FragmentInfoHash", getBlockName(blockNumber));
  if(!p->get(buf, &count))
    p->get("FragmentInfoHash", &count);
  c_fragmentInfoHash.setSize(count);

  count = 5;
  BaseString::snprintf(buf, 255, "%s.ActiveMutexes", getBlockName(blockNumber));
  if(!p->get(buf, &count))
    p->get("ActiveMutexes", &count);
  c_mutexMgr.setSize(count);
  
  c_counterMgr.setSize(5);
  
#ifdef VM_TRACE_TIME
  clearTimes();
#endif

  for(GlobalSignalNumber i = 0; i<=MAX_GSN; i++)
    theExecArray[i] = 0;

  installSimulatedBlockFunctions();
  UpgradeStartup::installEXEC(this);

  CLEAR_ERROR_INSERT_VALUE;

#ifdef VM_TRACE
  m_global_variables = new Ptr<void> * [1];
  m_global_variables[0] = 0;
#endif
}

SimulatedBlock::~SimulatedBlock()
{
  freeBat();
#ifdef VM_TRACE_TIME
  printTimes(stdout);
#endif

#ifdef VM_TRACE
  delete [] m_global_variables;
#endif
}

void 
SimulatedBlock::installSimulatedBlockFunctions(){
  ExecFunction * a = theExecArray;
  a[GSN_NODE_STATE_REP] = &SimulatedBlock::execNODE_STATE_REP;
  a[GSN_CHANGE_NODE_STATE_REQ] = &SimulatedBlock::execCHANGE_NODE_STATE_REQ;
  a[GSN_NDB_TAMPER] = &SimulatedBlock::execNDB_TAMPER;
  a[GSN_SIGNAL_DROPPED_REP] = &SimulatedBlock::execSIGNAL_DROPPED_REP;
  a[GSN_CONTINUE_FRAGMENTED]= &SimulatedBlock::execCONTINUE_FRAGMENTED;
  a[GSN_UTIL_CREATE_LOCK_REF]   = &SimulatedBlock::execUTIL_CREATE_LOCK_REF;
  a[GSN_UTIL_CREATE_LOCK_CONF]  = &SimulatedBlock::execUTIL_CREATE_LOCK_CONF;
  a[GSN_UTIL_DESTROY_LOCK_REF]  = &SimulatedBlock::execUTIL_DESTORY_LOCK_REF;
  a[GSN_UTIL_DESTROY_LOCK_CONF] = &SimulatedBlock::execUTIL_DESTORY_LOCK_CONF;
  a[GSN_UTIL_LOCK_REF]    = &SimulatedBlock::execUTIL_LOCK_REF;
  a[GSN_UTIL_LOCK_CONF]   = &SimulatedBlock::execUTIL_LOCK_CONF;
  a[GSN_UTIL_UNLOCK_REF]  = &SimulatedBlock::execUTIL_UNLOCK_REF;
  a[GSN_UTIL_UNLOCK_CONF] = &SimulatedBlock::execUTIL_UNLOCK_CONF;
  a[GSN_READ_CONFIG_REQ] = &SimulatedBlock::execREAD_CONFIG_REQ;
  a[GSN_FSOPENREF]    = &SimulatedBlock::execFSOPENREF;
  a[GSN_FSCLOSEREF]   = &SimulatedBlock::execFSCLOSEREF;
  a[GSN_FSWRITEREF]   = &SimulatedBlock::execFSWRITEREF;
  a[GSN_FSREADREF]    = &SimulatedBlock::execFSREADREF;
  a[GSN_FSREMOVEREF]  = &SimulatedBlock::execFSREMOVEREF;
  a[GSN_FSSYNCREF]    = &SimulatedBlock::execFSSYNCREF;
  a[GSN_FSAPPENDREF]  = &SimulatedBlock::execFSAPPENDREF;
}

void
SimulatedBlock::addRecSignalImpl(GlobalSignalNumber gsn, 
				 ExecFunction f, bool force){
  if(gsn > MAX_GSN || (!force &&  theExecArray[gsn] != 0)){
    char errorMsg[255];
    BaseString::snprintf(errorMsg, 255, 
 	     "GSN %d(%d))", gsn, MAX_GSN); 
    ERROR_SET(fatal, NDBD_EXIT_ILLEGAL_SIGNAL, errorMsg, errorMsg);
  }
  theExecArray[gsn] = f;
}

void
SimulatedBlock::signal_error(Uint32 gsn, Uint32 len, Uint32 recBlockNo, 
			     const char* filename, int lineno) const 
{
  char objRef[255];
  BaseString::snprintf(objRef, 255, "%s:%d", filename, lineno);
  char probData[255];
  BaseString::snprintf(probData, 255, 
	   "Signal (GSN: %d, Length: %d, Rec Block No: %d)", 
	   gsn, len, recBlockNo);
  
  ErrorReporter::handleError(NDBD_EXIT_BLOCK_BNR_ZERO,
			     probData, 
			     objRef);
}


extern class SectionSegmentPool g_sectionSegmentPool;

void 
SimulatedBlock::sendSignal(BlockReference ref, 
			   GlobalSignalNumber gsn, 
			   Signal* signal, 
			   Uint32 length, 
			   JobBufferLevel jobBuffer) const {

  BlockNumber sendBnr = number();
  BlockReference sendBRef = reference();
  
  Uint32 noOfSections = signal->header.m_noOfSections;
  Uint32 recBlock = refToBlock(ref);
  Uint32 recNode   = refToNode(ref);
  Uint32 ourProcessor         = globalData.ownId;
  
  signal->header.theLength = length;
  signal->header.theVerId_signalNumber = gsn;
  signal->header.theReceiversBlockNumber = recBlock;
  
  Uint32 tSignalId = signal->header.theSignalId;
  
  if ((length == 0) || (length + noOfSections > 25) || (recBlock == 0)) {
    signal_error(gsn, length, recBlock, __FILE__, __LINE__);
    return;
  }//if
#ifdef VM_TRACE
  if(globalData.testOn){
    Uint16 proc = 
      (recNode == 0 ? globalData.ownId : recNode);
    signal->header.theSendersBlockRef = sendBRef;
    globalSignalLoggers.sendSignal(signal->header, 
				   jobBuffer, 
				   &signal->theData[0],
				   proc,
                                   signal->m_sectionPtr,
                                   signal->header.m_noOfSections);
  }
#endif
  
  if(recNode == ourProcessor || recNode == 0) {
    signal->header.theSendersSignalId = tSignalId;
    signal->header.theSendersBlockRef = sendBRef;
    signal->header.theLength = length;
    globalScheduler.execute(signal, jobBuffer, recBlock,
			    gsn);
    signal->header.m_noOfSections = 0;
    signal->header.m_fragmentInfo = 0;
    return;
  } else { 
    // send distributed Signal
    SignalHeader sh;

    Uint32 tTrace = signal->getTrace();
    
    sh.theVerId_signalNumber   = gsn;
    sh.theReceiversBlockNumber = recBlock;
    sh.theSendersBlockRef      = sendBnr;
    sh.theLength               = length;
    sh.theTrace                = tTrace;
    sh.theSignalId             = tSignalId;
    sh.m_noOfSections          = noOfSections;
    sh.m_fragmentInfo          = 0;
    
#ifdef TRACE_DISTRIBUTED
    ndbout_c("send: %s(%d) to (%s, %d)",
	     getSignalName(gsn), gsn, getBlockName(recBlock),
	     recNode);
#endif
    SendStatus ss = globalTransporterRegistry.prepareSend(&sh, jobBuffer, 
							  &signal->theData[0],
							  recNode, 
							  g_sectionSegmentPool,
							  signal->m_sectionPtr);
    
    ndbrequire(ss == SEND_OK || ss == SEND_BLOCKED || ss == SEND_DISCONNECTED);
    ::releaseSections(noOfSections, signal->m_sectionPtr);
    signal->header.m_noOfSections = 0;   
  }
  return;
}

void 
SimulatedBlock::sendSignal(NodeReceiverGroup rg, 
			   GlobalSignalNumber gsn, 
			   Signal* signal, 
			   Uint32 length, 
			   JobBufferLevel jobBuffer) const {

  Uint32 noOfSections = signal->header.m_noOfSections;
  Uint32 tSignalId = signal->header.theSignalId;
  Uint32 tTrace = signal->getTrace();
  Uint32 tFragInf = signal->header.m_fragmentInfo;
  
  Uint32 ourProcessor = globalData.ownId;
  Uint32 recBlock = rg.m_block;
  
  signal->header.theLength = length;
  signal->header.theVerId_signalNumber = gsn;
  signal->header.theReceiversBlockNumber = recBlock;
  signal->header.theSendersSignalId = tSignalId;
  signal->header.theSendersBlockRef = reference();
  
  if ((length == 0) || (length + noOfSections > 25) || (recBlock == 0)) {
    signal_error(gsn, length, recBlock, __FILE__, __LINE__);
    return;
  }//if

  SignalHeader sh;
  
  sh.theVerId_signalNumber   = gsn;
  sh.theReceiversBlockNumber = recBlock;
  sh.theSendersBlockRef      = number();
  sh.theLength               = length;
  sh.theTrace                = tTrace;
  sh.theSignalId             = tSignalId;
  sh.m_noOfSections          = noOfSections;
  sh.m_fragmentInfo          = tFragInf;

  /**
   * Check own node
   */
  bool release = true;
  if(rg.m_nodes.get(0) || rg.m_nodes.get(ourProcessor)){
#ifdef VM_TRACE
    if(globalData.testOn){
      globalSignalLoggers.sendSignal(signal->header, 
				     jobBuffer, 
				     &signal->theData[0],
				     ourProcessor,
                                     signal->m_sectionPtr,
                                     signal->header.m_noOfSections);
    }
#endif
    globalScheduler.execute(signal, jobBuffer, recBlock, gsn);
    
    rg.m_nodes.clear((Uint32)0);
    rg.m_nodes.clear(ourProcessor);
    release = false;
  }

  /**
   * Do the big loop
   */
  Uint32 recNode = 0;
  while(!rg.m_nodes.isclear()){
    recNode = rg.m_nodes.find(recNode + 1);
    rg.m_nodes.clear(recNode);
#ifdef VM_TRACE
    if(globalData.testOn){
      globalSignalLoggers.sendSignal(signal->header, 
				     jobBuffer, 
				     &signal->theData[0],
				     recNode,
                                     signal->m_sectionPtr,
                                     signal->header.m_noOfSections);
    }
#endif

#ifdef TRACE_DISTRIBUTED
    ndbout_c("send: %s(%d) to (%s, %d)",
	     getSignalName(gsn), gsn, getBlockName(recBlock),
	     recNode);
#endif

    SendStatus ss = globalTransporterRegistry.prepareSend(&sh, jobBuffer, 
							  &signal->theData[0],
							  recNode,
							  g_sectionSegmentPool,
							  signal->m_sectionPtr);
    ndbrequire(ss == SEND_OK || ss == SEND_BLOCKED || ss == SEND_DISCONNECTED);
  }
  
  if(release){
    ::releaseSections(noOfSections, signal->m_sectionPtr);
  }
  
  signal->header.m_noOfSections = 0;
  signal->header.m_fragmentInfo = 0;
  
  return;
}

bool import(Ptr<SectionSegment> & first, const Uint32 * src, Uint32 len);

void 
SimulatedBlock::sendSignal(BlockReference ref, 
			   GlobalSignalNumber gsn, 
			   Signal* signal, 
			   Uint32 length, 
			   JobBufferLevel jobBuffer,
			   LinearSectionPtr ptr[3],
			   Uint32 noOfSections) const {
  
  BlockNumber sendBnr = number();
  BlockReference sendBRef = reference();
  
  Uint32 recBlock = refToBlock(ref);
  Uint32 recNode   = refToNode(ref);
  Uint32 ourProcessor         = globalData.ownId;
  
  ::releaseSections(signal->header.m_noOfSections, signal->m_sectionPtr);
  
  signal->header.theLength = length;
  signal->header.theVerId_signalNumber = gsn;
  signal->header.theReceiversBlockNumber = recBlock;
  signal->header.m_noOfSections = noOfSections;

  Uint32 tSignalId = signal->header.theSignalId;
  Uint32 tFragInfo = signal->header.m_fragmentInfo;
  
  if ((length == 0) || (length + noOfSections > 25) || (recBlock == 0)) {
    signal_error(gsn, length, recBlock, __FILE__, __LINE__);
    return;
  }//if
#ifdef VM_TRACE
  if(globalData.testOn){
    Uint16 proc = 
      (recNode == 0 ? globalData.ownId : recNode);
    signal->header.theSendersBlockRef = sendBRef;
    globalSignalLoggers.sendSignal(signal->header, 
				   jobBuffer, 
				   &signal->theData[0],
				   proc,
                                   ptr, noOfSections);
  }
#endif
  
  if(recNode == ourProcessor || recNode == 0) {
    signal->header.theSendersSignalId = tSignalId;
    signal->header.theSendersBlockRef = sendBRef;

    /**
     * We have to copy the data
     */
    Ptr<SectionSegment> segptr[3];
    for(Uint32 i = 0; i<noOfSections; i++){
      ndbrequire(import(segptr[i], ptr[i].p, ptr[i].sz));
      signal->m_sectionPtr[i].i = segptr[i].i;
    }
    
    globalScheduler.execute(signal, jobBuffer, recBlock,
			    gsn);
    signal->header.m_noOfSections = 0;
    return;
  } else { 
    // send distributed Signal
    SignalHeader sh;

    Uint32 tTrace = signal->getTrace();
    Uint32 noOfSections = signal->header.m_noOfSections;
    
    sh.theVerId_signalNumber   = gsn;
    sh.theReceiversBlockNumber = recBlock;
    sh.theSendersBlockRef      = sendBnr;
    sh.theLength               = length;
    sh.theTrace                = tTrace;
    sh.theSignalId             = tSignalId;
    sh.m_noOfSections          = noOfSections;
    sh.m_fragmentInfo          = tFragInfo;
    
#ifdef TRACE_DISTRIBUTED
    ndbout_c("send: %s(%d) to (%s, %d)",
	     getSignalName(gsn), gsn, getBlockName(recBlock),
	     recNode);
#endif

    SendStatus ss = globalTransporterRegistry.prepareSend(&sh, jobBuffer, 
							  &signal->theData[0],
							  recNode, 
							  ptr);
    ndbrequire(ss == SEND_OK || ss == SEND_BLOCKED || ss == SEND_DISCONNECTED);
  }

  signal->header.m_noOfSections = 0;
  signal->header.m_fragmentInfo = 0;
  return;
}

void 
SimulatedBlock::sendSignal(NodeReceiverGroup rg, 
			   GlobalSignalNumber gsn, 
			   Signal* signal, 
			   Uint32 length, 
			   JobBufferLevel jobBuffer,
			   LinearSectionPtr ptr[3],
			   Uint32 noOfSections) const {
  
  Uint32 tSignalId = signal->header.theSignalId;
  Uint32 tTrace    = signal->getTrace();
  Uint32 tFragInfo = signal->header.m_fragmentInfo;
  
  Uint32 ourProcessor = globalData.ownId;
  Uint32 recBlock = rg.m_block;
  
  ::releaseSections(signal->header.m_noOfSections, signal->m_sectionPtr);
  
  signal->header.theLength = length;
  signal->header.theVerId_signalNumber = gsn;
  signal->header.theReceiversBlockNumber = recBlock;
  signal->header.theSendersSignalId = tSignalId;
  signal->header.theSendersBlockRef = reference();
  signal->header.m_noOfSections = noOfSections;
  
  if ((length == 0) || (length + noOfSections > 25) || (recBlock == 0)) {
    signal_error(gsn, length, recBlock, __FILE__, __LINE__);
    return;
  }//if

  SignalHeader sh;
  sh.theVerId_signalNumber   = gsn;
  sh.theReceiversBlockNumber = recBlock;
  sh.theSendersBlockRef      = number();
  sh.theLength               = length;
  sh.theTrace                = tTrace;
  sh.theSignalId             = tSignalId;
  sh.m_noOfSections          = noOfSections;
  sh.m_fragmentInfo          = tFragInfo;

  /**
   * Check own node
   */
  if(rg.m_nodes.get(0) || rg.m_nodes.get(ourProcessor)){
#ifdef VM_TRACE
    if(globalData.testOn){
      globalSignalLoggers.sendSignal(signal->header, 
				     jobBuffer, 
				     &signal->theData[0],
				     ourProcessor,
                                     ptr, noOfSections);
    }
#endif
    /**
     * We have to copy the data
     */
    Ptr<SectionSegment> segptr[3];
    for(Uint32 i = 0; i<noOfSections; i++){
      ndbrequire(import(segptr[i], ptr[i].p, ptr[i].sz));
      signal->m_sectionPtr[i].i = segptr[i].i;
    }
    globalScheduler.execute(signal, jobBuffer, recBlock, gsn);
    
    rg.m_nodes.clear((Uint32)0);
    rg.m_nodes.clear(ourProcessor);
  }
  
  /**
   * Do the big loop
   */
  Uint32 recNode = 0;
  while(!rg.m_nodes.isclear()){
    recNode = rg.m_nodes.find(recNode + 1);
    rg.m_nodes.clear(recNode);
    
#ifdef VM_TRACE
    if(globalData.testOn){
      globalSignalLoggers.sendSignal(signal->header, 
				     jobBuffer, 
				     &signal->theData[0],
				     recNode,
                                     ptr, noOfSections);
    }
#endif
    
#ifdef TRACE_DISTRIBUTED
    ndbout_c("send: %s(%d) to (%s, %d)",
	     getSignalName(gsn), gsn, getBlockName(recBlock),
	     recNode);
#endif

    SendStatus ss = globalTransporterRegistry.prepareSend(&sh, jobBuffer, 
							  &signal->theData[0],
							  recNode,
							  ptr);
    ndbrequire(ss == SEND_OK || ss == SEND_BLOCKED || ss == SEND_DISCONNECTED);
  }
  
  signal->header.m_noOfSections = 0;
  signal->header.m_fragmentInfo = 0;
  
  return;
}

void
SimulatedBlock::sendSignalWithDelay(BlockReference ref, 
				    GlobalSignalNumber gsn,
				    Signal* signal,
				    Uint32 delayInMilliSeconds, 
				    Uint32 length) const {
  
  BlockNumber bnr = refToBlock(ref);
  
  //BlockNumber sendBnr = number();
  BlockReference sendBRef = reference();
  
  if (bnr == 0) {
    bnr_error();
  }//if
  
  signal->header.theLength = length;
  signal->header.theSendersSignalId = signal->header.theSignalId;
  signal->header.theSendersBlockRef = sendBRef;
  signal->header.theVerId_signalNumber = gsn;
  signal->header.theReceiversBlockNumber = bnr;

#ifdef VM_TRACE
  {
    if(globalData.testOn){
      globalSignalLoggers.sendSignalWithDelay(delayInMilliSeconds,
					      signal->header,
					      0,
					      &signal->theData[0], 
					      globalData.ownId,
                                              signal->m_sectionPtr,
                                              signal->header.m_noOfSections);
    }
  }
#endif
  globalTimeQueue.insert(signal, bnr, gsn, delayInMilliSeconds);

  signal->header.m_noOfSections = 0;
  signal->header.m_fragmentInfo = 0;
  
  // befor 2nd parameter to globalTimeQueue.insert
  // (Priority)theSendSig[sigIndex].jobBuffer
}

void
SimulatedBlock::releaseSections(Signal* signal){
  ::releaseSections(signal->header.m_noOfSections, signal->m_sectionPtr);
  signal->header.m_noOfSections = 0;
}

class SectionSegmentPool& 
SimulatedBlock::getSectionSegmentPool(){
  return g_sectionSegmentPool;
}

NewVARIABLE *
SimulatedBlock::allocateBat(int batSize){
  NewVARIABLE* bat = NewVarRef;
  bat = (NewVARIABLE*)realloc(bat, batSize * sizeof(NewVARIABLE));
  NewVarRef = bat;
  theBATSize = batSize;
  return bat;
}

void
SimulatedBlock::freeBat(){
  if(NewVarRef != 0){
    free(NewVarRef);
    NewVarRef = 0;
  }
}

const NewVARIABLE *
SimulatedBlock::getBat(Uint16 blockNo){
  SimulatedBlock * sb = globalData.getBlock(blockNo);
  if(sb == 0)
    return 0;
  return sb->NewVarRef;
}

Uint16
SimulatedBlock::getBatSize(Uint16 blockNo){
  SimulatedBlock * sb = globalData.getBlock(blockNo);
  if(sb == 0)
    return 0;
  return sb->theBATSize;
}

void* 
SimulatedBlock::allocRecord(const char * type, size_t s, size_t n, bool clear) 
{

  void * p = NULL;
  size_t size = n*s;
  refresh_watch_dog(); 
  if (size > 0){
#ifdef VM_TRACE_MEM
    ndbout_c("%s::allocRecord(%s, %u, %u) = %u bytes", 
	     getBlockName(number()), 
	     type,
	     s,
	     n,
	     size);
#endif
    p = NdbMem_Allocate(size);
    if (p == NULL){
      char buf1[255];
      char buf2[255];
      BaseString::snprintf(buf1, sizeof(buf1), "%s could not allocate memory for %s", 
	       getBlockName(number()), type);
      BaseString::snprintf(buf2, sizeof(buf2), "Requested: %ux%u = %u bytes", 
	       (Uint32)s, (Uint32)n, (Uint32)size);
      ERROR_SET(fatal, NDBD_EXIT_MEMALLOC, buf1, buf2);
    }

    if(clear){
      char * ptr = (char*)p;
      const Uint32 chunk = 128 * 1024;
      while(size > chunk){
	refresh_watch_dog(); 
	memset(ptr, 0, chunk);
	ptr += chunk;
	size -= chunk;
      }
      refresh_watch_dog(); 
      memset(ptr, 0, size);
    }
  }
  return p;
}

void 
SimulatedBlock::deallocRecord(void ** ptr, 
			      const char * type, size_t s, size_t n){
  (void)type;
  (void)s;
  (void)n;

  if(* ptr != 0){
    NdbMem_Free(* ptr);
    * ptr = 0;
  }
}

void
SimulatedBlock::refresh_watch_dog()
{
  globalData.incrementWatchDogCounter(1);
}

void
SimulatedBlock::progError(int line, int err_code, const char* extra) const {
  jamLine(line);

  const char *aBlockName = getBlockName(number(), "VM Kernel");

  // Pack status of interesting config variables 
  // so that we can print them in error.log
  int magicStatus = 
    (theConfiguration.stopOnError()<<1) + 
    (theConfiguration.getInitialStart()<<2) + 
    (theConfiguration.getDaemonMode()<<3);
  

  /* Add line number to block name */
  char buf[100];
  BaseString::snprintf(&buf[0], 100, "%s (Line: %d) 0x%.8x", 
	   aBlockName, line, magicStatus);

  ErrorReporter::handleError(err_code, extra, buf);

}

void 
SimulatedBlock::infoEvent(const char * msg, ...) const {
  if(msg == 0)
    return;
  
  Uint32 theData[25];
  theData[0] = NDB_LE_InfoEvent;
  char * buf = (char *)&(theData[1]);
  
  va_list ap;
  va_start(ap, msg);
  BaseString::vsnprintf(buf, 96, msg, ap); // 96 = 100 - 4
  va_end(ap);
  
  int len = strlen(buf) + 1;
  if(len > 96){
    len = 96;
    buf[95] = 0;
  }

  /**
   * Init and put it into the job buffer
   */
  SignalHeader sh;
  memset(&sh, 0, sizeof(SignalHeader));
  
  const Signal * signal = globalScheduler.getVMSignals();
  Uint32 tTrace = signal->header.theTrace;
  Uint32 tSignalId = signal->header.theSignalId;
  
  sh.theVerId_signalNumber   = GSN_EVENT_REP;
  sh.theReceiversBlockNumber = CMVMI;
  sh.theSendersBlockRef      = reference();
  sh.theTrace                = tTrace;
  sh.theSignalId             = tSignalId;
  sh.theLength               = ((len+3)/4)+1;
  
  Uint32 secPtrI[3]; // Dummy
  globalScheduler.execute(&sh, JBB, theData, secPtrI);
}

void 
SimulatedBlock::warningEvent(const char * msg, ...) const {
  if(msg == 0)
    return;
  
  Uint32 theData[25];
  theData[0] = NDB_LE_WarningEvent;
  char * buf = (char *)&(theData[1]);
  
  va_list ap;
  va_start(ap, msg);
  BaseString::vsnprintf(buf, 96, msg, ap); // 96 = 100 - 4
  va_end(ap);
  
  int len = strlen(buf) + 1;
  if(len > 96){
    len = 96;
    buf[95] = 0;
  }

  /**
   * Init and put it into the job buffer
   */
  SignalHeader sh;
  memset(&sh, 0, sizeof(SignalHeader));
  
  const Signal * signal = globalScheduler.getVMSignals();
  Uint32 tTrace = signal->header.theTrace;
  Uint32 tSignalId = signal->header.theSignalId;
  
  sh.theVerId_signalNumber   = GSN_EVENT_REP;
  sh.theReceiversBlockNumber = CMVMI;
  sh.theSendersBlockRef      = reference();
  sh.theTrace                = tTrace;
  sh.theSignalId             = tSignalId;
  sh.theLength               = ((len+3)/4)+1;

  Uint32 secPtrI[3]; // Dummy
  globalScheduler.execute(&sh, JBB, theData, secPtrI);
}

void
SimulatedBlock::execNODE_STATE_REP(Signal* signal){
  const NodeStateRep * const  rep = (NodeStateRep *)&signal->theData[0];
  
  this->theNodeState = rep->nodeState;
}

void
SimulatedBlock::execCHANGE_NODE_STATE_REQ(Signal* signal){
  const ChangeNodeStateReq * const  req = 
    (ChangeNodeStateReq *)&signal->theData[0];
  
  this->theNodeState = req->nodeState;
  const Uint32 senderData = req->senderData;
  const BlockReference senderRef = req->senderRef;

  /**
   * Pack return signal
   */
  ChangeNodeStateConf * const  conf = 
    (ChangeNodeStateConf *)&signal->theData[0];
  
  conf->senderData = senderData;

  sendSignal(senderRef, GSN_CHANGE_NODE_STATE_CONF, signal, 
	     ChangeNodeStateConf::SignalLength, JBB);
}

void
SimulatedBlock::execNDB_TAMPER(Signal * signal){
  SET_ERROR_INSERT_VALUE(signal->theData[0]);
}

void
SimulatedBlock::execSIGNAL_DROPPED_REP(Signal * signal){
  char msg[64];
  const SignalDroppedRep * const rep = (SignalDroppedRep *)&signal->theData[0];
  snprintf(msg, sizeof(msg), "%s GSN: %u (%u,%u)", getBlockName(number()),
	   rep->originalGsn, rep->originalLength,rep->originalSectionCount);
  ErrorReporter::handleError(NDBD_EXIT_OUT_OF_LONG_SIGNAL_MEMORY,
			     msg,
			     __FILE__,
			     NST_ErrorHandler);
}

void
SimulatedBlock::execCONTINUE_FRAGMENTED(Signal * signal){
  ljamEntry();
  
  Ptr<FragmentSendInfo> fragPtr;
  
  c_segmentedFragmentSendList.first(fragPtr);  
  for(; !fragPtr.isNull();){
    ljam();
    Ptr<FragmentSendInfo> copyPtr = fragPtr;
    c_segmentedFragmentSendList.next(fragPtr);
    
    sendNextSegmentedFragment(signal, * copyPtr.p);
    if(copyPtr.p->m_status == FragmentSendInfo::SendComplete){
      ljam();
      if(copyPtr.p->m_callback.m_callbackFunction != 0) {
        ljam();
	execute(signal, copyPtr.p->m_callback, 0);
      }//if
      c_segmentedFragmentSendList.release(copyPtr);
    }
  }
  
  c_linearFragmentSendList.first(fragPtr);  
  for(; !fragPtr.isNull();){
    ljam(); 
    Ptr<FragmentSendInfo> copyPtr = fragPtr;
    c_linearFragmentSendList.next(fragPtr);
    
    sendNextLinearFragment(signal, * copyPtr.p);
    if(copyPtr.p->m_status == FragmentSendInfo::SendComplete){
      ljam();
      if(copyPtr.p->m_callback.m_callbackFunction != 0) {
        ljam();
	execute(signal, copyPtr.p->m_callback, 0);
      }//if
      c_linearFragmentSendList.release(copyPtr);
    }
  }
  
  if(c_segmentedFragmentSendList.isEmpty() && 
     c_linearFragmentSendList.isEmpty()){
    ljam();
    c_fragSenderRunning = false;
    return;
  }
  
  ContinueFragmented * sig = (ContinueFragmented*)signal->getDataPtrSend();
  sig->line = __LINE__;
  sendSignal(reference(), GSN_CONTINUE_FRAGMENTED, signal, 1, JBB);
}

#ifdef VM_TRACE_TIME
void
SimulatedBlock::clearTimes() {
  for(Uint32 i = 0; i <= MAX_GSN; i++){
    m_timeTrace[i].cnt = 0;
    m_timeTrace[i].sum = 0;
    m_timeTrace[i].sub = 0;
  }
}

void
SimulatedBlock::printTimes(FILE * output){
  fprintf(output, "-- %s --\n", getBlockName(number()));
  Uint64 sum = 0;
  for(Uint32 i = 0; i <= MAX_GSN; i++){
    Uint32 n = m_timeTrace[i].cnt;
    if(n != 0){
      double dn = n;
      
      double avg = m_timeTrace[i].sum;
      double avg2 = avg - m_timeTrace[i].sub;
      
      avg /= dn;
      avg2 /= dn;
      
      fprintf(output, 
	      //name ; cnt ; loc ; acc
	      "%s ; #%d ; %dus ; %dus ; %dms\n",
	      getSignalName(i), n, (Uint32)avg, (Uint32)avg2, 
	      (Uint32)((m_timeTrace[i].sum - m_timeTrace[i].sub + 500)/ 1000));
      
      sum += (m_timeTrace[i].sum - m_timeTrace[i].sub);
    }
  }
  sum = (sum + 500)/ 1000;
  fprintf(output, "-- %s : %d --\n", getBlockName(number()), sum);
  fprintf(output, "\n");
  fflush(output);
}

#endif

void release(SegmentedSectionPtr & ptr);

SimulatedBlock::FragmentInfo::FragmentInfo(Uint32 fragId, Uint32 sender){
  m_fragmentId = fragId;
  m_senderRef = sender;
  m_sectionPtrI[0] = RNIL; 
  m_sectionPtrI[1] = RNIL;
  m_sectionPtrI[2] = RNIL;
}

SimulatedBlock::FragmentSendInfo::FragmentSendInfo()
{
}

bool
SimulatedBlock::assembleFragments(Signal * signal){
  Uint32 sigLen = signal->length() - 1;
  Uint32 fragId = signal->theData[sigLen];
  Uint32 fragInfo = signal->header.m_fragmentInfo;
  Uint32 senderRef = signal->getSendersBlockRef();
  
  if(fragInfo == 0){
    return true;
  }
  
  const Uint32 secs = signal->header.m_noOfSections;
  const Uint32 * const secNos = &signal->theData[sigLen - secs];
  
  if(fragInfo == 1){
    /**
     * First in train
     */
    Ptr<FragmentInfo> fragPtr;
    if(!c_fragmentInfoHash.seize(fragPtr)){
      ndbrequire(false);
      return false;
    }
    
    new (fragPtr.p)FragmentInfo(fragId, senderRef);
    c_fragmentInfoHash.add(fragPtr);
    
    for(Uint32 i = 0; i<secs; i++){
      Uint32 sectionNo = secNos[i];
      ndbassert(sectionNo < 3);
      fragPtr.p->m_sectionPtrI[sectionNo] = signal->m_sectionPtr[i].i;
    }
    
    /**
     * Don't release allocated segments
     */
    signal->header.m_noOfSections = 0;
    return false;
  }
  
  FragmentInfo key(fragId, senderRef);
  Ptr<FragmentInfo> fragPtr;
  if(c_fragmentInfoHash.find(fragPtr, key)){
    
    /**
     * FragInfo == 2 or 3
     */
    Uint32 i;
    for(i = 0; i<secs; i++){
      Uint32 sectionNo = secNos[i];
      ndbassert(sectionNo < 3);
      Uint32 sectionPtrI = signal->m_sectionPtr[i].i;
      if(fragPtr.p->m_sectionPtrI[sectionNo] != RNIL){
	linkSegments(fragPtr.p->m_sectionPtrI[sectionNo], sectionPtrI);
      } else {
	fragPtr.p->m_sectionPtrI[sectionNo] = sectionPtrI;
      }
    }
    
    /**
     * fragInfo = 2
     */
    if(fragInfo == 2){
      signal->header.m_noOfSections = 0;
      return false;
    }
    
    /**
     * fragInfo = 3
     */
    for(i = 0; i<3; i++){
      Uint32 ptrI = fragPtr.p->m_sectionPtrI[i];
      if(ptrI != RNIL){
	signal->m_sectionPtr[i].i = ptrI;
      } else {
	break;
      }
    }
    signal->setLength(sigLen - i);
    signal->header.m_noOfSections = i;
    signal->header.m_fragmentInfo = 0;
    getSections(i, signal->m_sectionPtr);
    
    c_fragmentInfoHash.release(fragPtr);
    return true;
  }
  
  /**
   * Unable to find fragment
   */
  ndbrequire(false);
  return false;
}

bool
SimulatedBlock::sendFirstFragment(FragmentSendInfo & info,
				  NodeReceiverGroup rg, 
				  GlobalSignalNumber gsn, 
				  Signal* signal, 
				  Uint32 length, 
				  JobBufferLevel jbuf,
				  Uint32 messageSize){
  
  info.m_sectionPtr[0].m_segmented.i = RNIL;
  info.m_sectionPtr[1].m_segmented.i = RNIL;
  info.m_sectionPtr[2].m_segmented.i = RNIL;
  
  Uint32 totalSize = 0;
  SectionSegment * p;
  switch(signal->header.m_noOfSections){
  case 3:
    p = signal->m_sectionPtr[2].p;  
    info.m_sectionPtr[2].m_segmented.p = p;
    info.m_sectionPtr[2].m_segmented.i = signal->m_sectionPtr[2].i;
    totalSize += p->m_sz;
  case 2:
    p = signal->m_sectionPtr[1].p;  
    info.m_sectionPtr[1].m_segmented.p = p;
    info.m_sectionPtr[1].m_segmented.i = signal->m_sectionPtr[1].i;
    totalSize += p->m_sz;
  case 1:
    p = signal->m_sectionPtr[0].p;  
    info.m_sectionPtr[0].m_segmented.p = p;
    info.m_sectionPtr[0].m_segmented.i = signal->m_sectionPtr[0].i;
    totalSize += p->m_sz;
  }

  if(totalSize <= messageSize + SectionSegment::DataLength){
    /**
     * Send signal directly
     */
    sendSignal(rg, gsn, signal, length, jbuf);
    info.m_status = FragmentSendInfo::SendComplete;
    return true;
  }

  /**
   * Consume sections
   */
  signal->header.m_noOfSections = 0;
    
  /**
   * Setup info object
   */
  info.m_status = FragmentSendInfo::SendNotComplete;
  info.m_prio = (Uint8)jbuf;
  info.m_gsn = gsn;
  info.m_fragInfo = 1;
  info.m_messageSize = messageSize;
  info.m_fragmentId = c_fragmentIdCounter++;
  info.m_nodeReceiverGroup = rg;
  info.m_callback.m_callbackFunction = 0;
  
  Ptr<SectionSegment> tmp;
  if(!import(tmp, &signal->theData[0], length)){
    ndbrequire(false);
    return false;
  }
  info.m_theDataSection.p = &tmp.p->theData[0];
  info.m_theDataSection.sz = length;
  tmp.p->theData[length] = tmp.i;
  
  sendNextSegmentedFragment(signal, info);
  
  if(c_fragmentIdCounter == 0){
    /**
     * Fragment id 0 is invalid
     */
    c_fragmentIdCounter = 1;
  }

  return true;
}

#if 0
#define lsout(x) x
#else
#define lsout(x)
#endif

void
SimulatedBlock::sendNextSegmentedFragment(Signal* signal,
					  FragmentSendInfo & info){
  
  /**
   * Store "theData"
   */
  const Uint32 sigLen = info.m_theDataSection.sz;
  memcpy(&signal->theData[0], info.m_theDataSection.p, 4 * sigLen);
  
  Uint32 sz = 0; 
  Uint32 maxSz = info.m_messageSize;
  
  Int32 secNo = 2;
  Uint32 secCount = 0;
  Uint32 * secNos = &signal->theData[sigLen];
  
  enum { Unknown = 0, Full = 1 } loop = Unknown;
  for(; secNo >= 0 && secCount < 3; secNo--){
    Uint32 ptrI = info.m_sectionPtr[secNo].m_segmented.i;
    if(ptrI == RNIL)
      continue;
    
    info.m_sectionPtr[secNo].m_segmented.i = RNIL;
    
    SectionSegment * ptrP = info.m_sectionPtr[secNo].m_segmented.p;
    const Uint32 size = ptrP->m_sz;
    
    signal->m_sectionPtr[secCount].i = ptrI;
    signal->m_sectionPtr[secCount].p = ptrP;
    signal->m_sectionPtr[secCount].sz = size;
    secNos[secCount] = secNo;
    secCount++;
    
    const Uint32 sizeLeft = maxSz - sz;
    if(size <= sizeLeft){
      /**
       * The section fits
       */
      sz += size;
      lsout(ndbout_c("section %d saved as %d", secNo, secCount-1));
      continue;
    }
    
    const Uint32 overflow = size - sizeLeft; // > 0
    if(overflow <= SectionSegment::DataLength){
      /**
       * Only one segment left to send
       *   send even if sizeLeft <= size
       */
      lsout(ndbout_c("section %d saved as %d but full over: %d", 
		     secNo, secCount-1, overflow));
      secNo--;
      break;
    }

    // size >= 61
    if(sizeLeft < SectionSegment::DataLength){
      /**
       * Less than one segment left (space)
       *   dont bother sending
       */
      secCount--;
      info.m_sectionPtr[secNo].m_segmented.i = ptrI;
      loop = Full;
      lsout(ndbout_c("section %d not saved", secNo));
      break;
    }
    
    /**
     * Split list
     * 1) Find place to split
     * 2) Rewrite header (the part that will be sent)
     * 3) Write new header (for remaining part)
     * 4) Store new header on FragmentSendInfo - record
     */
    // size >= 61 && sizeLeft >= 60
    Uint32 sum = SectionSegment::DataLength;
    Uint32 prevPtrI = ptrI;
    ptrI = ptrP->m_nextSegment;
    const Uint32 fill = sizeLeft - SectionSegment::DataLength;
    while(sum < fill){
      prevPtrI = ptrI;
      ptrP = g_sectionSegmentPool.getPtr(ptrI);
      ptrI = ptrP->m_nextSegment;
      sum += SectionSegment::DataLength;
    }
    
    /**
     * Rewrite header w.r.t size and last
     */
    Uint32 prev = secCount - 1;
    const Uint32 last = signal->m_sectionPtr[prev].p->m_lastSegment;
    signal->m_sectionPtr[prev].p->m_lastSegment = prevPtrI;
    signal->m_sectionPtr[prev].p->m_sz = sum;
    signal->m_sectionPtr[prev].sz = sum;
      
    /**
     * Write "new" list header
     */
    ptrP = g_sectionSegmentPool.getPtr(ptrI);
    ptrP->m_lastSegment = last;
    ptrP->m_sz = size - sum;
    
    /**
     * And store it on info-record
     */
    info.m_sectionPtr[secNo].m_segmented.i = ptrI;
    info.m_sectionPtr[secNo].m_segmented.p = ptrP;
    
    loop = Full;
    lsout(ndbout_c("section %d split into %d", secNo, prev));
    break;
  }
  
  lsout(ndbout_c("loop: %d secNo: %d secCount: %d sz: %d", 
		 loop, secNo, secCount, sz));
  
  /**
   * Store fragment id
   */
  secNos[secCount] = info.m_fragmentId;
  
  Uint32 fragInfo = info.m_fragInfo;
  info.m_fragInfo = 2;
  switch(loop){
  case Unknown:
    if(secNo >= 0){
      lsout(ndbout_c("Unknown - Full"));
      /**
       * Not finished
       */
      break;
    }
    // Fall through
    lsout(ndbout_c("Unknown - Done"));
    info.m_status = FragmentSendInfo::SendComplete;
    ndbassert(fragInfo == 2);
    fragInfo = 3;
  case Full:
    break;
  }
  
  signal->header.m_fragmentInfo = fragInfo;
  signal->header.m_noOfSections = secCount;
  
  sendSignal(info.m_nodeReceiverGroup,
	     info.m_gsn,
	     signal, 
	     sigLen + secCount + 1,
	     (JobBufferLevel)info.m_prio);
  
  if(fragInfo == 3){
    /**
     * This is the last signal
     */
    g_sectionSegmentPool.release(info.m_theDataSection.p[sigLen]);
  }
}

bool
SimulatedBlock::sendFirstFragment(FragmentSendInfo & info,
				  NodeReceiverGroup rg, 
				  GlobalSignalNumber gsn, 
				  Signal* signal, 
				  Uint32 length, 
				  JobBufferLevel jbuf,
				  LinearSectionPtr ptr[3],
				  Uint32 noOfSections,
				  Uint32 messageSize){
  
  ::releaseSections(signal->header.m_noOfSections, signal->m_sectionPtr);
  signal->header.m_noOfSections = 0;
  
  info.m_sectionPtr[0].m_linear.p = NULL;
  info.m_sectionPtr[1].m_linear.p = NULL;
  info.m_sectionPtr[2].m_linear.p = NULL;
  
  Uint32 totalSize = 0;
  switch(noOfSections){
  case 3:
    info.m_sectionPtr[2].m_linear = ptr[2];
    totalSize += ptr[2].sz;
  case 2:
    info.m_sectionPtr[1].m_linear = ptr[1];
    totalSize += ptr[1].sz;
  case 1:
    info.m_sectionPtr[0].m_linear = ptr[0];
    totalSize += ptr[0].sz;
  }

  if(totalSize <= messageSize + SectionSegment::DataLength){
    /**
     * Send signal directly
     */
    sendSignal(rg, gsn, signal, length, jbuf, ptr, noOfSections);
    info.m_status = FragmentSendInfo::SendComplete;
    
    /**
     * Indicate to sendLinearSignalFragment
     *   that we'r already done
     */
    return true;
  }

  /**
   * Setup info object
   */
  info.m_status = FragmentSendInfo::SendNotComplete;
  info.m_prio = (Uint8)jbuf;
  info.m_gsn = gsn;
  info.m_messageSize = messageSize;
  info.m_fragInfo = 1;
  info.m_fragmentId = c_fragmentIdCounter++;
  info.m_nodeReceiverGroup = rg;
  info.m_callback.m_callbackFunction = 0;

  Ptr<SectionSegment> tmp;
  if(!import(tmp, &signal->theData[0], length)){
    ndbrequire(false);
    return false;
  }

  info.m_theDataSection.p = &tmp.p->theData[0];
  info.m_theDataSection.sz = length;
  tmp.p->theData[length] = tmp.i;
  
  sendNextLinearFragment(signal, info);

  if(c_fragmentIdCounter == 0){
    /**
     * Fragment id 0 is invalid
     */
    c_fragmentIdCounter = 1;
  }
  
  return true;
}

void
SimulatedBlock::sendNextLinearFragment(Signal* signal,
				       FragmentSendInfo & info){
  
  /**
   * Store "theData"
   */
  const Uint32 sigLen = info.m_theDataSection.sz;
  memcpy(&signal->theData[0], info.m_theDataSection.p, 4 * sigLen);
  
  Uint32 sz = 0; 
  Uint32 maxSz = info.m_messageSize;
  
  Int32 secNo = 2;
  Uint32 secCount = 0;
  Uint32 * secNos = &signal->theData[sigLen];
  LinearSectionPtr signalPtr[3];
  
  enum { Unknown = 0, Full = 2 } loop = Unknown;
  for(; secNo >= 0 && secCount < 3; secNo--){
    Uint32 * ptrP = info.m_sectionPtr[secNo].m_linear.p;
    if(ptrP == NULL)
      continue;
    
    info.m_sectionPtr[secNo].m_linear.p = NULL;
    const Uint32 size = info.m_sectionPtr[secNo].m_linear.sz;
    
    signalPtr[secCount].p = ptrP;
    signalPtr[secCount].sz = size;
    secNos[secCount] = secNo;
    secCount++;
    
    const Uint32 sizeLeft = maxSz - sz;
    if(size <= sizeLeft){
      /**
       * The section fits
       */
      sz += size;
      lsout(ndbout_c("section %d saved as %d", secNo, secCount-1));
      continue;
    }
    
    const Uint32 overflow = size - sizeLeft; // > 0
    if(overflow <= SectionSegment::DataLength){
      /**
       * Only one segment left to send
       *   send even if sizeLeft <= size
       */
      lsout(ndbout_c("section %d saved as %d but full over: %d", 
		     secNo, secCount-1, overflow));
      secNo--;
      break;
    }

    // size >= 61
    if(sizeLeft < SectionSegment::DataLength){
      /**
       * Less than one segment left (space)
       *   dont bother sending
       */
      secCount--;
      info.m_sectionPtr[secNo].m_linear.p = ptrP;
      loop = Full;
      lsout(ndbout_c("section %d not saved", secNo));
      break;
    }
    
    /**
     * Split list
     * 1) Find place to split
     * 2) Rewrite header (the part that will be sent)
     * 3) Write new header (for remaining part)
     * 4) Store new header on FragmentSendInfo - record
     */
    Uint32 sum = sizeLeft;
    sum /= SectionSegment::DataLength;
    sum *= SectionSegment::DataLength;
    
    /**
     * Rewrite header w.r.t size
     */
    Uint32 prev = secCount - 1;
    signalPtr[prev].sz = sum;
    
    /**
     * Write/store "new" header
     */
    info.m_sectionPtr[secNo].m_linear.p = ptrP + sum;
    info.m_sectionPtr[secNo].m_linear.sz = size - sum;
    
    loop = Full;
    lsout(ndbout_c("section %d split into %d", secNo, prev));
    break;
  }
  
  lsout(ndbout_c("loop: %d secNo: %d secCount: %d sz: %d", 
		 loop, secNo, secCount, sz));
  
  /**
   * Store fragment id
   */
  secNos[secCount] = info.m_fragmentId;
  
  Uint32 fragInfo = info.m_fragInfo;
  info.m_fragInfo = 2;
  switch(loop){
  case Unknown:
    if(secNo >= 0){
      lsout(ndbout_c("Unknown - Full"));
      /**
       * Not finished
       */
      break;
    }
    // Fall through
    lsout(ndbout_c("Unknown - Done"));
    info.m_status = FragmentSendInfo::SendComplete;
    ndbassert(fragInfo == 2);
    fragInfo = 3;
  case Full:
    break;
  }
  
  signal->header.m_noOfSections = 0;
  signal->header.m_fragmentInfo = fragInfo;
  
  sendSignal(info.m_nodeReceiverGroup,
	     info.m_gsn,
	     signal, 
	     sigLen + secCount + 1,
	     (JobBufferLevel)info.m_prio,
	     signalPtr,
	     secCount);
  
  if(fragInfo == 3){
    /**
     * This is the last signal
     */
    g_sectionSegmentPool.release(info.m_theDataSection.p[sigLen]);
  }
}

void
SimulatedBlock::sendFragmentedSignal(BlockReference ref, 
				     GlobalSignalNumber gsn, 
				     Signal* signal, 
				     Uint32 length, 
				     JobBufferLevel jbuf,
				     Callback & c,
				     Uint32 messageSize){
  bool res = true;
  Ptr<FragmentSendInfo> ptr;
  res = c_segmentedFragmentSendList.seize(ptr);
  ndbrequire(res);
  
  res = sendFirstFragment(* ptr.p, 
			  NodeReceiverGroup(ref),
			  gsn,
			  signal,
			  length,
			  jbuf,
			  messageSize);
  ndbrequire(res);
  
  if(ptr.p->m_status == FragmentSendInfo::SendComplete){
    c_segmentedFragmentSendList.release(ptr);
    if(c.m_callbackFunction != 0)
      execute(signal, c, 0);
    return;
  }
  ptr.p->m_callback = c;

  if(!c_fragSenderRunning){
    c_fragSenderRunning = true;
    ContinueFragmented * sig = (ContinueFragmented*)signal->getDataPtrSend();
    sig->line = __LINE__;
    sendSignal(reference(), GSN_CONTINUE_FRAGMENTED, signal, 1, JBB);
  }
}

void
SimulatedBlock::sendFragmentedSignal(NodeReceiverGroup rg, 
				     GlobalSignalNumber gsn, 
				     Signal* signal, 
				     Uint32 length, 
				     JobBufferLevel jbuf,
				     Callback & c,
				     Uint32 messageSize){
  bool res = true;
  Ptr<FragmentSendInfo> ptr;
  res = c_segmentedFragmentSendList.seize(ptr);
  ndbrequire(res);
  
  res = sendFirstFragment(* ptr.p, 
			  rg,
			  gsn,
			  signal,
			  length,
			  jbuf,
			  messageSize);
  ndbrequire(res);
  
  if(ptr.p->m_status == FragmentSendInfo::SendComplete){
    c_segmentedFragmentSendList.release(ptr);
    if(c.m_callbackFunction != 0)
      execute(signal, c, 0);
    return;
  }
  ptr.p->m_callback = c;

  if(!c_fragSenderRunning){
    c_fragSenderRunning = true;
    ContinueFragmented * sig = (ContinueFragmented*)signal->getDataPtrSend();
    sig->line = __LINE__;
    sendSignal(reference(), GSN_CONTINUE_FRAGMENTED, signal, 1, JBB);
  }
}

SimulatedBlock::Callback SimulatedBlock::TheEmptyCallback = {0, 0};

void
SimulatedBlock::sendFragmentedSignal(BlockReference ref, 
				     GlobalSignalNumber gsn, 
				     Signal* signal, 
				     Uint32 length, 
				     JobBufferLevel jbuf,
				     LinearSectionPtr ptr[3],
				     Uint32 noOfSections,
				     Callback & c,
				     Uint32 messageSize){
  bool res = true;
  Ptr<FragmentSendInfo> tmp;
  res = c_linearFragmentSendList.seize(tmp);
  ndbrequire(res);

  res = sendFirstFragment(* tmp.p, 
			  NodeReceiverGroup(ref),
			  gsn,
			  signal,
			  length,
			  jbuf,
			  ptr,
			  noOfSections,
			  messageSize);
  ndbrequire(res);
  
  if(tmp.p->m_status == FragmentSendInfo::SendComplete){
    c_linearFragmentSendList.release(tmp);
    if(c.m_callbackFunction != 0)
      execute(signal, c, 0);
    return;
  }
  tmp.p->m_callback = c;
  
  if(!c_fragSenderRunning){
    c_fragSenderRunning = true;
    ContinueFragmented * sig = (ContinueFragmented*)signal->getDataPtrSend();
    sig->line = __LINE__;
    sendSignal(reference(), GSN_CONTINUE_FRAGMENTED, signal, 1, JBB);
  }
}

void
SimulatedBlock::sendFragmentedSignal(NodeReceiverGroup rg, 
				     GlobalSignalNumber gsn, 
				     Signal* signal, 
				     Uint32 length, 
				     JobBufferLevel jbuf,
				     LinearSectionPtr ptr[3],
				     Uint32 noOfSections,
				     Callback & c,
				     Uint32 messageSize){
  bool res = true;
  Ptr<FragmentSendInfo> tmp;
  res = c_linearFragmentSendList.seize(tmp);
  ndbrequire(res);

  res = sendFirstFragment(* tmp.p, 
			  rg,
			  gsn,
			  signal,
			  length,
			  jbuf,
			  ptr,
			  noOfSections,
			  messageSize);
  ndbrequire(res);
  
  if(tmp.p->m_status == FragmentSendInfo::SendComplete){
    c_linearFragmentSendList.release(tmp);
    if(c.m_callbackFunction != 0)
      execute(signal, c, 0);
    return;
  }
  tmp.p->m_callback = c;
  
  if(!c_fragSenderRunning){
    c_fragSenderRunning = true;
    ContinueFragmented * sig = (ContinueFragmented*)signal->getDataPtrSend();
    sig->line = __LINE__;
    sendSignal(reference(), GSN_CONTINUE_FRAGMENTED, signal, 1, JBB);
  }
}

NodeInfo &
SimulatedBlock::setNodeInfo(NodeId nodeId) {
  ndbrequire(nodeId > 0 && nodeId < MAX_NODES);
  return globalData.m_nodeInfo[nodeId];
}

void 
SimulatedBlock::execUTIL_CREATE_LOCK_REF(Signal* signal){
  ljamEntry();
  c_mutexMgr.execUTIL_CREATE_LOCK_REF(signal);
}

void SimulatedBlock::execUTIL_CREATE_LOCK_CONF(Signal* signal){
  ljamEntry();
  c_mutexMgr.execUTIL_CREATE_LOCK_CONF(signal);
}

void SimulatedBlock::execUTIL_DESTORY_LOCK_REF(Signal* signal){
  ljamEntry();
  c_mutexMgr.execUTIL_DESTORY_LOCK_REF(signal);
}

void SimulatedBlock::execUTIL_DESTORY_LOCK_CONF(Signal* signal){
  ljamEntry();
  c_mutexMgr.execUTIL_DESTORY_LOCK_CONF(signal);
}

void SimulatedBlock::execUTIL_LOCK_REF(Signal* signal){
  ljamEntry();
  c_mutexMgr.execUTIL_LOCK_REF(signal);
}

void SimulatedBlock::execUTIL_LOCK_CONF(Signal* signal){
  ljamEntry();
  c_mutexMgr.execUTIL_LOCK_CONF(signal);
}

void SimulatedBlock::execUTIL_UNLOCK_REF(Signal* signal){
  ljamEntry();
  c_mutexMgr.execUTIL_UNLOCK_REF(signal);
}

void SimulatedBlock::execUTIL_UNLOCK_CONF(Signal* signal){
  ljamEntry();
  c_mutexMgr.execUTIL_UNLOCK_CONF(signal);
}

void 
SimulatedBlock::execREAD_CONFIG_REQ(Signal* signal){
  const ReadConfigReq * req = (ReadConfigReq*)signal->getDataPtr();

  Uint32 ref = req->senderRef;
  Uint32 senderData = req->senderData;

  ReadConfigConf * conf = (ReadConfigConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(ref, GSN_READ_CONFIG_CONF, signal, 
	     ReadConfigConf::SignalLength, JBB);
}

void
SimulatedBlock::ignoreMutexUnlockCallback(Signal* signal, 
					  Uint32 ptrI, Uint32 retVal){
  c_mutexMgr.release(ptrI);
}

void 
UpgradeStartup::installEXEC(SimulatedBlock* block){
  SimulatedBlock::ExecFunction * a = block->theExecArray;
  switch(block->number()){
  case QMGR:
    a[UpgradeStartup::GSN_CM_APPCHG] = &SimulatedBlock::execUPGRADE;
    break;
  case CNTR:
    a[UpgradeStartup::GSN_CNTR_MASTERREF] = &SimulatedBlock::execUPGRADE;
    a[UpgradeStartup::GSN_CNTR_MASTERCONF] = &SimulatedBlock::execUPGRADE;
    break;
  }
}

void
SimulatedBlock::execUPGRADE(Signal* signal){
  Uint32 gsn = signal->header.theVerId_signalNumber;
  switch(gsn){
  case UpgradeStartup::GSN_CM_APPCHG:
    UpgradeStartup::execCM_APPCHG(* this, signal);
    break;
  case UpgradeStartup::GSN_CNTR_MASTERREF:
    UpgradeStartup::execCNTR_MASTER_REPLY(* this, signal);
    break;
  case UpgradeStartup::GSN_CNTR_MASTERCONF:
    UpgradeStartup::execCNTR_MASTER_REPLY(* this, signal);
    break;
  }
}

void
SimulatedBlock::fsRefError(Signal* signal, Uint32 line, const char *msg) 
{
  const FsRef *fsRef = (FsRef*)signal->getDataPtr();
  Uint32 errorCode = fsRef->errorCode;
  Uint32 osErrorCode = fsRef->osErrorCode;
  char msg2[100];

  sprintf(msg2, "%s: %s. OS errno: %u", getBlockName(number()), msg, osErrorCode);

  progError(line, errorCode, msg2);
}

void
SimulatedBlock::execFSWRITEREF(Signal* signal) 
{
  fsRefError(signal, __LINE__, "File system write failed");
}

void
SimulatedBlock::execFSREADREF(Signal* signal) 
{
  fsRefError(signal, __LINE__, "File system read failed");
}

void
SimulatedBlock::execFSCLOSEREF(Signal* signal) 
{
  fsRefError(signal, __LINE__, "File system close failed");
}

void
SimulatedBlock::execFSOPENREF(Signal* signal) 
{
  fsRefError(signal, __LINE__, "File system open failed");
}

void
SimulatedBlock::execFSREMOVEREF(Signal* signal) 
{
  fsRefError(signal, __LINE__, "File system remove failed");
}

void
SimulatedBlock::execFSSYNCREF(Signal* signal) 
{
  fsRefError(signal, __LINE__, "File system sync failed");
}

void
SimulatedBlock::execFSAPPENDREF(Signal* signal) 
{
  fsRefError(signal, __LINE__, "File system append failed");
}

#ifdef VM_TRACE
void
SimulatedBlock::clear_global_variables(){
  Ptr<void> ** tmp = m_global_variables;
  while(* tmp != 0){
    (* tmp)->i = RNIL;
    (* tmp)->p = 0;
    tmp++;
  }
}

void
SimulatedBlock::init_globals_list(void ** tmp, size_t cnt){
  m_global_variables = new Ptr<void> * [cnt+1];
  for(size_t i = 0; i<cnt; i++){
    m_global_variables[i] = (Ptr<void>*)tmp[i];
  }
  m_global_variables[cnt] = 0;
}

#endif

#include "KeyDescriptor.hpp"

Uint32
SimulatedBlock::xfrm_key(Uint32 tab, const Uint32* src, 
			 Uint32 *dst, Uint32 dstSize,
			 Uint32 keyPartLen[MAX_ATTRIBUTES_IN_INDEX]) const
{
  const KeyDescriptor * desc = g_key_descriptor_pool.getPtr(tab);
  const Uint32 noOfKeyAttr = desc->noOfKeyAttr;

  Uint32 i = 0;
  Uint32 srcPos = 0;
  Uint32 dstPos = 0;
  while (i < noOfKeyAttr) 
  {
    const KeyDescriptor::KeyAttr& keyAttr = desc->keyAttr[i];
    
    Uint32 srcBytes = 
      AttributeDescriptor::getSizeInBytes(keyAttr.attributeDescriptor);
    Uint32 srcWords = (srcBytes + 3) / 4;
    Uint32 dstWords = ~0;
    uchar* dstPtr = (uchar*)&dst[dstPos];
    const uchar* srcPtr = (const uchar*)&src[srcPos];
    CHARSET_INFO* cs = keyAttr.charsetInfo;
    
    if (cs == NULL) 
    {
      jam();
      memcpy(dstPtr, srcPtr, srcWords << 2);
      dstWords = srcWords;
    } 
    else 
    {
      jam();
      Uint32 typeId =
	AttributeDescriptor::getType(keyAttr.attributeDescriptor);
      Uint32 lb, len;
      bool ok = NdbSqlUtil::get_var_length(typeId, srcPtr, srcBytes, lb, len);
      ndbrequire(ok);
      Uint32 xmul = cs->strxfrm_multiply;
      if (xmul == 0)
	xmul = 1;
      /*
       * Varchar is really Char.  End spaces do not matter.  To get
       * same hash we blank-pad to maximum length via strnxfrm.
       * TODO use MySQL charset-aware hash function instead
       */
      Uint32 dstLen = xmul * (srcBytes - lb);
      ndbrequire(dstLen <= ((dstSize - dstPos) << 2));
      int n = NdbSqlUtil::strnxfrm_bug7284(cs, dstPtr, dstLen, srcPtr + lb, len);
      ndbrequire(n != -1);
      while ((n & 3) != 0) 
      {
	dstPtr[n++] = 0;
      }
      dstWords = (n >> 2);
    }
    dstPos += dstWords;
    srcPos += srcWords;
    keyPartLen[i++] = dstWords;
  }

  return dstPos;
}

Uint32
SimulatedBlock::create_distr_key(Uint32 tableId,
				 Uint32 *data, 
				 const Uint32 
				 keyPartLen[MAX_ATTRIBUTES_IN_INDEX]) const 
{
  const KeyDescriptor* desc = g_key_descriptor_pool.getPtr(tableId);
  const Uint32 noOfKeyAttr = desc->noOfKeyAttr;
  Uint32 noOfDistrKeys = desc->noOfDistrKeys;
  
  Uint32 *src = data;
  Uint32 *dst = data;
  Uint32 i = 0;
  Uint32 dstPos = 0;
  
  if(keyPartLen)
  {
    while (i < noOfKeyAttr && noOfDistrKeys) 
    {
      Uint32 attr = desc->keyAttr[i].attributeDescriptor;
      Uint32 len = keyPartLen[i];
      if(AttributeDescriptor::getDKey(attr))
      {
	noOfDistrKeys--;
	memmove(dst+dstPos, src, len << 2);
	dstPos += len;
      }
      src += len;
      i++;
    }
  }
  else
  {
    while (i < noOfKeyAttr && noOfDistrKeys) 
    {
      Uint32 attr = desc->keyAttr[i].attributeDescriptor;
      Uint32 len = AttributeDescriptor::getSizeInWords(attr);
      if(AttributeDescriptor::getDKey(attr))
      {
	noOfDistrKeys--;
	memmove(dst+dstPos, src, len << 2);
	dstPos += len;
      }
      src += len;
      i++;
    }
  }
  return dstPos;
}
