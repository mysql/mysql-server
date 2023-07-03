/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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

/*
  This file is used to build both the multithreaded and the singlethreaded
  ndbd. It is built twice, included from either SimulatedBlock_mt.cpp (with
  the macro NDBD_MULTITHREADED defined) or SimulatedBlock_nonmt.cpp (with the
  macro not defined).
*/

#include "util/require.h"
#include <ndb_global.h>
#include "portlib/ndb_compiler.h"

#include "SimulatedBlock.hpp"
#include <NdbOut.hpp>
#include <OutputStream.hpp>
#include <GlobalData.hpp>
#include <Emulator.hpp>
#include <WatchDog.hpp>
#include <ErrorHandlingMacros.hpp>
#include <TimeQueue.hpp>
#include <TransporterRegistry.hpp>
#include <SignalLoggerManager.hpp>
#include <FastScheduler.hpp>
#include "ndbd_malloc.hpp"
#include "signaldata/DumpStateOrd.hpp"
#include <signaldata/EventReport.hpp>
#include <signaldata/ContinueFragmented.hpp>
#include <signaldata/NodeStateSignalData.hpp>
#include <signaldata/FsRef.hpp>
#include <signaldata/SignalDroppedRep.hpp>
#include <signaldata/LocalRouteOrd.hpp>
#include <signaldata/TransIdAI.hpp>
#include <signaldata/Sync.hpp>
#include <DebuggerNames.hpp>
#include "LongSignal.hpp"

#include <Properties.hpp>
#include "Configuration.hpp"
#include <AttributeDescriptor.hpp>
#include <NdbSqlUtil.hpp>

#include "../blocks/dbdih/Dbdih.hpp"
#include <signaldata/CallbackSignal.hpp>
#include "LongSignalImpl.hpp"

#include "KeyDescriptor.hpp"

#include <EventLogger.hpp>

#define JAM_FILE_ID 252


//
// Constructor, Destructor
//
SimulatedBlock::SimulatedBlock(BlockNumber blockNumber,
			       struct Block_context & ctx,
                               Uint32 instanceNumber)
  : theNodeId(globalData.ownId),
    theNumber(blockNumber),
    theInstance(instanceNumber),
    theReference(numberToRef(blockNumber, instanceNumber, globalData.ownId)),
    theInstanceList(0),
    theMainInstance(0),
    m_pHighResTimer(0),
    m_ctx(ctx),
    m_global_page_pool(globalData.m_global_page_pool),
    m_shared_page_pool(globalData.m_shared_page_pool),
    c_fragmentInfoHash(c_fragmentInfoPool),
    c_linearFragmentSendList(c_fragmentSendPool),
    c_segmentedFragmentSendList(c_fragmentSendPool),
    c_mutexMgr(* this),
    c_counterMgr(* this)
#ifdef VM_TRACE_TIME
    ,m_currentGsn(0)
#endif
#ifdef VM_TRACE
    ,debugOutFile(globalSignalLoggers.getOutputStream())
    ,debugOut(debugOutFile)
#endif
{
  m_threadId = 0;
  m_watchDogCounter = NULL;
  m_jamBuffer = NDB_THREAD_TLS_JAM;
  NewVarRef = 0;
  
  SimulatedBlock* mainBlock = globalData.getBlock(blockNumber);

  if (theInstance == 0) {
    ndbrequire(mainBlock == 0);
    mainBlock = this;
    theMainInstance = mainBlock;
    globalData.setBlock(blockNumber, mainBlock);
    mainBlock->addInstance(this, theInstance);
  } else {
    ndbrequire(mainBlock != 0);
    mainBlock->addInstance(this, theInstance);
    theMainInstance = mainBlock;
  }

  c_fragmentIdCounter = 1;
  c_fragSenderRunning = false;
  
#ifdef VM_TRACE_TIME
  clearTimes();
#endif

  for(GlobalSignalNumber i = 0; i<=MAX_GSN; i++){
    theSignalHandlerArray[i].m_execFunction = nullptr;
    theSignalHandlerArray[i].m_signalScope = SignalScope::External;
  }

  installSimulatedBlockFunctions();

  m_callbackTableAddr = 0;

  CLEAR_ERROR_INSERT_VALUE;

#ifndef NDBD_MULTITHREADED
  /* Ndbd, init from GlobalScheduler */
  m_pHighResTimer = globalScheduler.getHighResTimerPtr();
#endif
}

void
SimulatedBlock::addInstance(SimulatedBlock* b, Uint32 theInstance)
{
  ndbrequire(theMainInstance == this);
  ndbrequire(number() == b->number());
  if (theInstanceList == 0)
  {
    theInstanceList = new SimulatedBlock* [MaxInstances];
    ndbrequire(theInstanceList != 0);
    for (Uint32 i = 0; i < MaxInstances; i++)
      theInstanceList[i] = 0;
  }
  ndbrequire(theInstance < MaxInstances);
  ndbrequire(theInstanceList[theInstance] == 0);
  theInstanceList[theInstance] = b;
}

void
SimulatedBlock::initCommon()
{
  static_assert(RG_COUNT == MM_RG_COUNT + 1);

  Uint32 count = 10;
  this->getParam("FragmentSendPool", &count);
  c_fragmentSendPool.setSize(count);

  count = 10;
  this->getParam("FragmentInfoPool", &count);
  c_fragmentInfoPool.setSize(count);

  count = 10;
  this->getParam("FragmentInfoHash", &count);
  c_fragmentInfoHash.setSize(count);

  Uint32 def = 5;
#ifdef NDBD_MULTITHREADED
  def += globalData.getBlockThreads();
#endif

  count = def;
  this->getParam("ActiveMutexes", &count);
  c_mutexMgr.setSize(count);

  count = def;
  this->getParam("ActiveCounters", &count);
  c_counterMgr.setSize(count);

  count = def;
  this->getParam("ActiveThreadSync", &count);
  c_syncThreadPool.setSize(count);
}

SimulatedBlock::~SimulatedBlock()
{
  freeBat();
#ifdef VM_TRACE_TIME
  printTimes(stdout);
#endif

  if (theInstanceList != 0) {
    Uint32 i;
    for (i = 0; i < MaxInstances; i++)
    {
      if (theInstanceList[i] != this)
      {
        delete theInstanceList[i];
      }
    }
    delete [] theInstanceList;
  }
  theInstanceList = 0;
}

void 
SimulatedBlock::installSimulatedBlockFunctions(){
  FunctionAndScope* a = theSignalHandlerArray;
  a[GSN_NODE_STATE_REP].m_execFunction = &SimulatedBlock::execNODE_STATE_REP;
  a[GSN_CHANGE_NODE_STATE_REQ].m_execFunction = &SimulatedBlock::execCHANGE_NODE_STATE_REQ;
  a[GSN_NDB_TAMPER].m_execFunction = &SimulatedBlock::execNDB_TAMPER;
  a[GSN_SIGNAL_DROPPED_REP].m_execFunction = &SimulatedBlock::execSIGNAL_DROPPED_REP;
  a[GSN_CONTINUE_FRAGMENTED].m_execFunction = &SimulatedBlock::execCONTINUE_FRAGMENTED;
  a[GSN_STOP_FOR_CRASH].m_execFunction = &SimulatedBlock::execSTOP_FOR_CRASH;
  a[GSN_UTIL_CREATE_LOCK_REF].m_execFunction  = &SimulatedBlock::execUTIL_CREATE_LOCK_REF;
  a[GSN_UTIL_CREATE_LOCK_CONF].m_execFunction = &SimulatedBlock::execUTIL_CREATE_LOCK_CONF;
  a[GSN_UTIL_DESTROY_LOCK_REF].m_execFunction = &SimulatedBlock::execUTIL_DESTORY_LOCK_REF;
  a[GSN_UTIL_DESTROY_LOCK_CONF].m_execFunction = &SimulatedBlock::execUTIL_DESTORY_LOCK_CONF;
  a[GSN_UTIL_LOCK_REF].m_execFunction = &SimulatedBlock::execUTIL_LOCK_REF;
  a[GSN_UTIL_LOCK_CONF].m_execFunction = &SimulatedBlock::execUTIL_LOCK_CONF;
  a[GSN_UTIL_UNLOCK_REF].m_execFunction = &SimulatedBlock::execUTIL_UNLOCK_REF;
  a[GSN_UTIL_UNLOCK_CONF].m_execFunction = &SimulatedBlock::execUTIL_UNLOCK_CONF;
  a[GSN_FSOPENREF].m_execFunction = &SimulatedBlock::execFSOPENREF;
  a[GSN_FSOPENREF].m_signalScope = SignalScope::Local;
  a[GSN_FSCLOSEREF].m_execFunction = &SimulatedBlock::execFSCLOSEREF;
  a[GSN_FSCLOSEREF].m_signalScope = SignalScope::Local;
  a[GSN_FSWRITEREF].m_execFunction = &SimulatedBlock::execFSWRITEREF;
  a[GSN_FSWRITEREF].m_signalScope = SignalScope::Local;
  a[GSN_FSREADREF].m_execFunction = &SimulatedBlock::execFSREADREF;
  a[GSN_FSREADREF].m_signalScope = SignalScope::Local;
  a[GSN_FSREMOVEREF].m_execFunction = &SimulatedBlock::execFSREMOVEREF;
  a[GSN_FSREMOVEREF].m_signalScope = SignalScope::Local;
  a[GSN_FSSYNCREF].m_execFunction = &SimulatedBlock::execFSSYNCREF;
  a[GSN_FSSYNCREF].m_signalScope = SignalScope::Local;
  a[GSN_FSAPPENDREF].m_execFunction = &SimulatedBlock::execFSAPPENDREF;
  a[GSN_FSAPPENDREF].m_signalScope = SignalScope::Local;
  a[GSN_NODE_START_REP].m_execFunction = &SimulatedBlock::execNODE_START_REP;
  a[GSN_API_START_REP].m_execFunction = &SimulatedBlock::execAPI_START_REP;
  a[GSN_SEND_PACKED].m_execFunction = &SimulatedBlock::execSEND_PACKED;
  a[GSN_CALLBACK_CONF].m_execFunction = &SimulatedBlock::execCALLBACK_CONF;
  a[GSN_SYNC_THREAD_REQ].m_execFunction = &SimulatedBlock::execSYNC_THREAD_REQ;
  a[GSN_SYNC_THREAD_CONF].m_execFunction = &SimulatedBlock::execSYNC_THREAD_CONF;
  a[GSN_LOCAL_ROUTE_ORD].m_execFunction = &SimulatedBlock::execLOCAL_ROUTE_ORD;
  a[GSN_SYNC_REQ].m_execFunction = &SimulatedBlock::execSYNC_REQ;
  a[GSN_SYNC_PATH_REQ].m_execFunction = &SimulatedBlock::execSYNC_PATH_REQ;
  a[GSN_SYNC_PATH_CONF].m_execFunction = &SimulatedBlock::execSYNC_PATH_CONF;
}

void
SimulatedBlock::addRecSignalImpl(GlobalSignalNumber gsn, 
				 ExecFunction f, bool force){
  if(gsn > MAX_GSN ||
     (!force &&  theSignalHandlerArray[gsn].m_execFunction != nullptr)){
    char errorMsg[255];
    BaseString::snprintf(errorMsg, 255, 
 	     "GSN %d(%d))", gsn, MAX_GSN); 
    ERROR_SET(fatal, NDBD_EXIT_ILLEGAL_SIGNAL, errorMsg, errorMsg);
  }
  theSignalHandlerArray[gsn].m_execFunction = f;
}

void
SimulatedBlock::addSignalScopeImpl(GlobalSignalNumber gsn,
                                   SignalScope scope){
  FunctionAndScope& fas = theSignalHandlerArray[gsn];

  if (!(scope == SignalScope::Local || scope == SignalScope::Remote || scope == SignalScope::Management || scope == SignalScope::External)){
    warningEvent("SimulatedBlock::addSignalScopeImpl, incorrect use, SignalScope out of range %u", scope);
    require(false);
  }
  // If scope is defined multiple times we assume the most restrictive
  fas.m_signalScope = MIN(fas.m_signalScope, scope);
}

void
SimulatedBlock::assignToThread(ThreadContext ctx)
{
  m_threadId = ctx.threadId;
  m_jamBuffer = ctx.jamBuffer;
  m_watchDogCounter = ctx.watchDogCounter;
  m_sectionPoolCache = ctx.sectionPoolCache;
  m_pHighResTimer = ctx.pHighResTimer;
}

Uint32 SimulatedBlock::getInstanceNo(Uint32 nodeId,
                                     Uint32 tableId,
                                     Uint32 fragId)
{
  Uint32 lqhWorkers = getNodeInfo(nodeId).m_lqh_workers;
  if (lqhWorkers == 0)
    return 0;
  Dbdih* dbdih = (Dbdih*)globalData.getBlock(DBDIH);
  Uint32 instanceKey = dbdih->dihGetInstanceKey(tableId, fragId);
  assert(instanceKey != 0);
  Uint32 instanceNo = 1 + ((instanceKey - 1) % lqhWorkers);
  return instanceNo;
}

Uint32 SimulatedBlock::getInstanceNo(Uint32 nodeId,
                                     Uint32 instanceKey)
{
  Uint32 lqhWorkers = getNodeInfo(nodeId).m_lqh_workers;
  if (lqhWorkers == 0)
    return 0;
  assert(instanceKey != 0);
  Uint32 instanceNo = 1 + ((instanceKey - 1) % lqhWorkers);
  return instanceNo;
}

Uint32 SimulatedBlock::getInstanceNoCanFail(Uint32 tableId, Uint32 fragId)
{
  Dbdih* dbdih = (Dbdih*)globalData.getBlock(DBDIH);
  Uint32 instanceKey = dbdih->dihGetInstanceKeyCanFail(tableId, fragId);
  if (likely(instanceKey != RNIL))
  {
    Uint32 lqhWorkers = globalData.ndbMtLqhWorkers;
    if (lqhWorkers == 0)
    {
      return 0;
    }
    return (1 + ((instanceKey - 1) % lqhWorkers));
  }
  else
  {
    return instanceKey;
  }
}

Uint32 SimulatedBlock::getInstanceKey(Uint32 tabId, Uint32 fragId)
{
  Dbdih* dbdih = (Dbdih*)globalData.getBlock(DBDIH);
  Uint32 instanceKey = dbdih->dihGetInstanceKey(tabId, fragId);
  return instanceKey;
}

Uint32
SimulatedBlock::getInstanceKeyCanFail(Uint32 tabId, Uint32 fragId)
{
  Dbdih* dbdih = (Dbdih*)globalData.getBlock(DBDIH);
  Uint32 instanceKey = dbdih->dihGetInstanceKeyCanFail(tabId, fragId);
  return instanceKey;
}

Uint32
SimulatedBlock::getInstance(Uint32 tableId, Uint32 fragId)
{
  Uint32 instanceKey = getInstanceKey(tableId, fragId);
  return getInstanceFromKey(instanceKey);
}

Uint32 SimulatedBlock::getInstanceFromKey(Uint32 nodeId, Uint32 instanceKey)
{
  Uint32 lqhWorkers = getNodeInfo(nodeId).m_lqh_workers;
  Uint32 instanceNo;
  if (lqhWorkers == 0)
  {
    instanceNo = 0;
  }
  else
  {
    assert(instanceKey != 0);
    instanceNo = 1 + (instanceKey - 1) % lqhWorkers;
  }
  return instanceNo;
}

Uint32
SimulatedBlock::getInstanceFromKey(Uint32 instanceKey)
{
  Uint32 lqhWorkers = globalData.ndbMtLqhWorkers;
  Uint32 instanceNo;
  if (lqhWorkers == 0)
  {
    instanceNo = 0;
  }
  else
  {
    assert(instanceKey != 0);
    instanceNo = 1 + (instanceKey - 1) % lqhWorkers;
  }
  return instanceNo;
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
SimulatedBlock::handle_invalid_sections_in_send_signal(const Signal25* signal)
const
{
  char errMsg[160];
  BaseString::snprintf(errMsg, sizeof errMsg,
                       "Unhandled sections in sendSignal for GSN %u (%s).", 
                       signal->header.theVerId_signalNumber,
                       getSignalName(signal->header.theVerId_signalNumber));
  // Print message and terminate.
  ErrorReporter::handleError(NDBD_EXIT_BLOCK_BNR_ZERO,
                             errMsg,
                             "");
}

void
SimulatedBlock::handle_lingering_sections_after_execute(const Signal* signal)
const
{
  char errMsg[160];
  BaseString::snprintf(errMsg, sizeof errMsg,
                      "Unhandled sections after execute for GSN %u (%s).", 
                      signal->header.theVerId_signalNumber,
                      getSignalName(signal->header.theVerId_signalNumber));
  // Print message and terminate.
  ErrorReporter::handleError(NDBD_EXIT_BLOCK_BNR_ZERO,
                             errMsg,
                             "");
}

void
SimulatedBlock::handle_invalid_fragmentInfo(Signal25* signal) const
{
  ErrorReporter::handleError(NDBD_EXIT_BLOCK_BNR_ZERO,
                             "Incorrect header->m_fragmentInfo in sendSignal()",
                             "");
}

void
SimulatedBlock::handle_out_of_longsignal_memory(Signal25 * signal) const
{
  ErrorReporter::handleError(NDBD_EXIT_OUT_OF_LONG_SIGNAL_MEMORY,
			     "Out of LongMessageBuffer in sendSignal",
			     "");
}

template<typename SecPtr>
void
SimulatedBlock::handle_send_failed(SendStatus ss,
                                   Signal25 * signal,
                                   Uint32 recNode,
                                   SecPtr ptr[]) const
{
  switch(ss){
  case SEND_BUFFER_FULL:
    ErrorReporter::handleError(NDBD_EXIT_GENERIC,
                               "Out of SendBufferMemory in sendSignal", "");
    break;
  case SEND_MESSAGE_TOO_BIG:
    /* If message is too big when sending CmvmiDummySignal log a convenient
     * message about it to.
     * Note that CmvmiDummySignal is not intended for production usage but for
     * use by test cases.
     */
    if (signal->header.theVerId_signalNumber == GSN_DUMP_STATE_ORD &&
        signal->theData[0] == DumpStateOrd::CmvmiDummySignal)
    {
      jam();
      const Uint32 num_secs = signal->getNoOfSections();
      char msg[24*4];
      snprintf(msg,
               sizeof(msg),
               "Failed sending CmvmiDummySignal"
               " (size %u+%u+%u+%u+%u) from %u to %u.",
               signal->getLength(), num_secs,
               (num_secs > 0) ? ptr[0].sz : 0,
               (num_secs > 1) ? ptr[1].sz : 0,
               (num_secs > 2) ? ptr[2].sz : 0,
               signal->theData[2],
               recNode);
      g_eventLogger->info("%s", msg);
      infoEvent("%s", msg);
      return;
    }
    ErrorReporter::handleError(NDBD_EXIT_NDBREQUIRE,
                               "Message too big in sendSignal", "");
    break;
  case SEND_UNKNOWN_NODE:
    ErrorReporter::handleError(NDBD_EXIT_NDBREQUIRE,
                               "Unknown node in sendSignal", "");
    break;
  case SEND_OK:
  case SEND_BLOCKED:
  case SEND_DISCONNECTED:
    // Should never happen
    ndbabort();
  }
  ndbabort();
}

static void
linkSegments(Uint32 head, Uint32 tail){
  
  Ptr<SectionSegment> headPtr;
  require(g_sectionSegmentPool.getPtr(headPtr, head));
  
  Ptr<SectionSegment> tailPtr;
  require(g_sectionSegmentPool.getPtr(tailPtr, tail));
  
  Ptr<SectionSegment> oldTailPtr;
  require(g_sectionSegmentPool.getPtr(oldTailPtr, headPtr.p->m_lastSegment));
  
  /* Can only efficiently link segments if linking to the end of a 
   * multiple-of-segment-size sized chunk
   */
  if ((headPtr.p->m_sz % NDB_SECTION_SEGMENT_SZ) != 0)
  {
#if defined VM_TRACE || defined ERROR_INSERT
    ErrorReporter::handleError(NDBD_EXIT_BLOCK_BNR_ZERO,
                               "Bad head segment size",
                               "");
#else
    g_eventLogger->info("linkSegments : Bad head segment size");
#endif
  }

  headPtr.p->m_lastSegment = tailPtr.p->m_lastSegment;
  headPtr.p->m_sz += tailPtr.p->m_sz;
  
  oldTailPtr.p->m_nextSegment = tailPtr.i;
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
    [[fallthrough]];
  case 2:
    p = g_sectionSegmentPool.getPtr(tSec1);
    ptr[1].p = p;
    ptr[1].sz = p->m_sz;
    [[fallthrough]];
  case 1:
    p = g_sectionSegmentPool.getPtr(tSec0);
    ptr[0].p = p;
    ptr[0].sz = p->m_sz;
    [[fallthrough]];
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

Uint32 getSectionSz(Uint32 id)
{
  return g_sectionSegmentPool.getPtr(id)->m_sz;
}

Uint32* getLastWordPtr(Uint32 id)
{
  SectionSegment* first= g_sectionSegmentPool.getPtr(id);
  SectionSegment* last= g_sectionSegmentPool.getPtr(first->m_lastSegment);
  Uint32 offset= (first->m_sz -1) % SectionSegment::DataLength;
  return &last->theData[offset];
}

#ifdef NDBD_MULTITHREADED
#define SB_SP_ARG *m_sectionPoolCache,
#define SB_SP_REL_ARG f_section_lock, *m_sectionPoolCache,
#else
#define SB_SP_ARG
#define SB_SP_REL_ARG
#endif

static
void
releaseSections(SPC_ARG Uint32 secCount, SegmentedSectionPtr ptr[3]){
  Uint32 tSec0 = ptr[0].i;
  Uint32 tSz0 = ptr[0].sz;
  Uint32 tSec1 = ptr[1].i;
  Uint32 tSz1 = ptr[1].sz;
  Uint32 tSec2 = ptr[2].i;
  Uint32 tSz2 = ptr[2].sz;
  switch(secCount){
  case 3:
    g_sectionSegmentPool.releaseList(SPC_SEIZE_ARG
                                     relSz(tSz2), tSec2,
				     ptr[2].p->m_lastSegment);
    [[fallthrough]];
  case 2:
    g_sectionSegmentPool.releaseList(SPC_SEIZE_ARG
                                     relSz(tSz1), tSec1,
				     ptr[1].p->m_lastSegment);
    [[fallthrough]];
  case 1:
    g_sectionSegmentPool.releaseList(SPC_SEIZE_ARG
                                     relSz(tSz0), tSec0,
				     ptr[0].p->m_lastSegment);
    [[fallthrough]];
  case 0:
    return;
  }
  char msg[40];
  sprintf(msg, "secCount=%d", secCount);
  ErrorReporter::handleAssert(msg, __FILE__, __LINE__);
}

void
SimulatedBlock::getSendBufferLevel(NodeId node, SB_LevelType &level)
{
#ifdef NDBD_MULTITHREADED
  mt_getSendBufferLevel(m_threadId, node, level);
#else
  getNonMTTransporterSendHandle()->getSendBufferLevel(node, level);
#endif
}

Uint32
SimulatedBlock::getEstimatedJobBufferLevel()
{
  Uint32 num_signals;
#ifdef NDBD_MULTITHREADED
  num_signals = mt_getEstimatedJobBufferLevel(m_threadId);
#else
  num_signals = globalScheduler.getBOccupancy();
#endif
  return num_signals;
}

void
SimulatedBlock::startChangeNeighbourNode()
{
  /* We only treat neighbour nodes in a special manner in ndbmtd. */
#ifdef NDBD_MULTITHREADED
  mt_startChangeNeighbourNode();
#endif
}

void
SimulatedBlock::setNeighbourNode(NodeId node)
{
  /* We only treat neighbour nodes in a special manner in ndbmtd. */
#ifdef NDBD_MULTITHREADED
  mt_setNeighbourNode(node);
#endif
}

void
SimulatedBlock::setNoSend()
{
#ifdef NDBD_MULTITHREADED
  mt_setNoSend(m_threadId);
#endif
}

void
SimulatedBlock::endChangeNeighbourNode()
{
  /* We only treat neighbour nodes in a special manner in ndbmtd. */
#ifdef NDBD_MULTITHREADED
  mt_endChangeNeighbourNode();
#endif
}

void
SimulatedBlock::setWakeupThread(Uint32 wakeup_instance)
{
#ifdef NDBD_MULTITHREADED
  mt_setWakeupThread(m_threadId, wakeup_instance);
#endif
}

void
SimulatedBlock::setOverloadStatus(OverloadStatus new_status)
{
#ifdef NDBD_MULTITHREADED
  mt_setOverloadStatus(m_threadId, new_status);
#endif
}

void
SimulatedBlock::setNodeOverloadStatus(OverloadStatus new_status)
{
#ifdef NDBD_MULTITHREADED
  mt_setNodeOverloadStatus(m_threadId, new_status);
#endif
}

void
SimulatedBlock::setSendNodeOverloadStatus(OverloadStatus new_status)
{
#ifdef NDBD_MULTITHREADED
  mt_setSendNodeOverloadStatus(new_status);
#endif
}

Uint32
SimulatedBlock::getConfiguredSpintime()
{
#ifdef NDBD_MULTITHREADED
  return mt_getConfiguredSpintime(m_threadId);
#else
  return 0;
#endif
}

void
SimulatedBlock::setSpintime(Uint32 new_spintime)
{
#ifdef NDBD_MULTITHREADED
  mt_setSpintime(m_threadId, new_spintime);
#endif
}

Uint32
SimulatedBlock::getWakeupLatency()
{
#ifdef NDBD_MULTITHREADED
  return mt_getWakeupLatency();
#else
  return 25;
#endif
}

void
SimulatedBlock::setWakeupLatency(Uint32 latency)
{
#ifdef NDBD_MULTITHREADED
  mt_setWakeupLatency(latency);
#endif
}

void
SimulatedBlock::getPerformanceTimers(Uint64 & micros_sleep,
                                     Uint64 & spin_time,
                                     Uint64 & buffer_full_micros_sleep,
                                     Uint64 & micros_send)
{
#ifdef NDBD_MULTITHREADED
  mt_getPerformanceTimers(m_threadId,
                          micros_sleep,
                          spin_time,
                          buffer_full_micros_sleep,
                          micros_send);
#else
  micros_sleep = globalData.theMicrosSleep;
  spin_time = globalData.theMicrosSpin;
  buffer_full_micros_sleep = globalData.theBufferFullMicrosSleep;
  micros_send = globalData.theMicrosSend;
#endif
}

const char *
SimulatedBlock::getThreadDescription()
{
  const char *desc;
#ifdef NDBD_MULTITHREADED
  desc = mt_getThreadDescription(m_threadId);
#else
  desc = "ndbd single thread";
#endif
  return desc;
}

const char *
SimulatedBlock::getThreadName()
{
  const char *name;
#ifdef NDBD_MULTITHREADED
  name = mt_getThreadName(m_threadId);
#else
  name = "main";
#endif
  return name;
}

void
SimulatedBlock::getSendPerformanceTimers(Uint32 send_instance,
                                         Uint64 & exec_time,
                                         Uint64 & sleep_time,
                                         Uint64 & spin_time,
                                         Uint64 & user_time_os,
                                         Uint64 & kernel_time_os,
                                         Uint64 & elapsed_time_os)
{
  /* No send thread in ndbd */
#ifdef NDBD_MULTITHREADED
  mt_getSendPerformanceTimers(send_instance,
                              exec_time,
                              sleep_time,
                              spin_time,
                              user_time_os,
                              kernel_time_os,
                              elapsed_time_os);
#else
  exec_time = 0;
  sleep_time = 0;
  spin_time = 0;
  user_time_os = 0;
  kernel_time_os = 0;
  elapsed_time_os = 0;
#endif
}

Uint32
SimulatedBlock::getNumSendThreads()
{
#ifdef NDBD_MULTITHREADED
  return mt_getNumSendThreads();
#else
  return 0;
#endif
}

Uint32
SimulatedBlock::getNumThreads()
{
#ifdef NDBD_MULTITHREADED
  return mt_getNumThreads();
#else
  return 1;
#endif
}

void
SimulatedBlock::flush_send_buffers()
{
#ifdef NDBD_MULTITHREADED
  mt_flush_send_buffers(m_threadId);
#endif
}

void
SimulatedBlock::set_watchdog_counter()
{
#ifdef NDBD_MULTITHREADED
  mt_set_watchdog_counter(m_threadId);
#endif
}

void
SimulatedBlock::assign_recv_thread_new_trp(Uint32 trp_id)
{
#ifdef NDBD_MULTITHREADED
  mt_assign_recv_thread_new_trp(trp_id);
#endif
}

void
SimulatedBlock::assign_multi_trps_to_send_threads()
{
#ifdef NDBD_MULTITHREADED
  mt_assign_multi_trps_to_send_threads();
#endif
}

bool
SimulatedBlock::epoll_add_trp(NodeId node_id, TrpId trp_id)
{
#ifdef NDBD_MULTITHREADED
  return mt_epoll_add_trp(m_threadId, node_id, trp_id);
#else
  require(false);
  return false;
#endif
}

bool
SimulatedBlock::is_recv_thread_for_new_trp(NodeId node_id, TrpId trp_id)
{
#ifdef NDBD_MULTITHREADED
  return mt_is_recv_thread_for_new_trp(m_threadId, node_id, trp_id);
#else
  require(false);
  return false;
#endif
}

Uint32
SimulatedBlock::getMainThrmanInstance()
{
#ifdef NDBD_MULTITHREADED
  return mt_getMainThrmanInstance();
#else
  return 1;
#endif
}

void 
SimulatedBlock::sendSignal(BlockReference ref, 
			   GlobalSignalNumber gsn, 
                           Signal25* signal,
			   Uint32 length, 
			   JobBufferLevel jobBuffer) const {

  BlockReference sendBRef = reference();
  
  Uint32 recBlock = refToBlock(ref);
  Uint32 recNode   = refToNode(ref);
  Uint32 ourProcessor         = globalData.ownId;
  
  ndbrequire(signal->header.m_noOfSections == 0);
  check_sections(signal, signal->header.m_noOfSections, 0);

  signal->header.theLength = length;
  signal->header.theVerId_signalNumber = gsn;
  signal->header.theReceiversBlockNumber = recBlock;
  signal->header.m_noOfSections = 0;

  Uint32 tSignalId = signal->header.theSignalId;
  
  if (unlikely((length == 0) || length > 25 || (recBlock == 0)))
  {
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
				   proc);
  }
#endif
  
  if(recNode == ourProcessor || recNode == 0) {
    signal->header.theSendersSignalId = tSignalId;
    signal->header.theSendersBlockRef = sendBRef;
#ifdef NDBD_MULTITHREADED
    if (jobBuffer == JBB)
      sendlocal(m_threadId, &signal->header, signal->theData, NULL);
    else
      sendprioa(m_threadId, &signal->header, signal->theData, NULL);
#else
    globalScheduler.execute(signal, jobBuffer);
#endif
    return;
  } else { 
    // send distributed Signal
    SignalHeader sh;

    Uint32 tTrace = signal->getTrace();
    
    sh.theVerId_signalNumber   = gsn;
    sh.theReceiversBlockNumber = recBlock;
    sh.theSendersBlockRef      = refToBlock(sendBRef);
    sh.theLength               = length;
    sh.theTrace                = tTrace;
    sh.theSignalId             = tSignalId;
    sh.m_noOfSections          = 0;
    sh.m_fragmentInfo          = 0;
    
#ifdef TRACE_DISTRIBUTED
    g_eventLogger->info("send: %s(%d) to (%s, %d)", getSignalName(gsn), gsn,
                        getBlockName(recBlock), recNode);
#endif

    SendStatus ss;
#ifdef NDBD_MULTITHREADED
    ss = mt_send_remote(m_threadId, &sh, jobBuffer, &signal->theData[0],
                        recNode, 0);
#else
    TrpId trp_id = 0;
    ss = globalTransporterRegistry.
           prepareSend(getNonMTTransporterSendHandle(),
                       &sh, jobBuffer,
                       &signal->theData[0], recNode, trp_id,
                       (LinearSectionPtr*)0);
#endif
    
    if (unlikely(! (ss == SEND_OK ||
                    ss == SEND_BLOCKED ||
                    ss == SEND_DISCONNECTED)))
    {
      handle_send_failed(ss, signal, recNode, (LinearSectionPtr*)NULL);
    }
  }
  return;
}

void 
SimulatedBlock::sendSignal(NodeReceiverGroup rg, 
			   GlobalSignalNumber gsn, 
                           Signal25* signal,
			   Uint32 length, 
			   JobBufferLevel jobBuffer) const {

  Uint32 noOfSections = signal->header.m_noOfSections;
  Uint32 tSignalId = signal->header.theSignalId;
  Uint32 tTrace = signal->getTrace();
  
  Uint32 ourProcessor = globalData.ownId;
  Uint32 recBlock = rg.m_block;
  
  signal->header.theLength = length;
  signal->header.theVerId_signalNumber = gsn;
  signal->header.theReceiversBlockNumber = recBlock;
  signal->header.theSendersSignalId = tSignalId;
  signal->header.theSendersBlockRef = reference();
  signal->header.m_noOfSections = 0;

  check_sections(signal, noOfSections, 0);

  if ((length == 0) || (length > 25) || (recBlock == 0)) {
    signal_error(gsn, length, recBlock, __FILE__, __LINE__);
    return;
  }//if

  SignalHeader sh;
  
  sh.theVerId_signalNumber   = gsn;
  sh.theReceiversBlockNumber = recBlock;
  sh.theSendersBlockRef      = refToBlock(reference());
  sh.theLength               = length;
  sh.theTrace                = tTrace;
  sh.theSignalId             = tSignalId;
  sh.m_noOfSections          = 0;
  sh.m_fragmentInfo          = 0;

  /**
   * Check own node
   */
  if(rg.m_nodes.get(0) || rg.m_nodes.get(ourProcessor)){
#ifdef VM_TRACE
    if(globalData.testOn){
      globalSignalLoggers.sendSignal(signal->header, 
				     jobBuffer, 
				     &signal->theData[0],
				     ourProcessor);
    }
#endif

#ifdef NDBD_MULTITHREADED
    if (jobBuffer == JBB)
      sendlocal(m_threadId, &signal->header, signal->theData, NULL);
    else
      sendprioa(m_threadId, &signal->header, signal->theData, NULL);
#else
    globalScheduler.execute(signal, jobBuffer);
#endif
    
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
				     recNode);
    }
#endif

#ifdef TRACE_DISTRIBUTED
    g_eventLogger->info("send: %s(%d) to (%s, %d)", getSignalName(gsn), gsn,
                        getBlockName(recBlock), recNode);
#endif

    SendStatus ss;
#ifdef NDBD_MULTITHREADED
    ss = mt_send_remote(m_threadId, &sh, jobBuffer, &signal->theData[0],
                        recNode, 0);
#else
    TrpId trp_id = 0;
    ss = globalTransporterRegistry.
           prepareSend(getNonMTTransporterSendHandle(),
                       &sh, jobBuffer, 
                       &signal->theData[0], recNode, trp_id,
                       (LinearSectionPtr*)0);
#endif

    if (unlikely(! (ss == SEND_OK ||
                    ss == SEND_BLOCKED ||
                    ss == SEND_DISCONNECTED)))
    {
      handle_send_failed(ss, signal, recNode, (LinearSectionPtr*)NULL);
    }
  }

  return;
}

bool import(Ptr<SectionSegment> & first, const Uint32 * src, Uint32 len);

void 
SimulatedBlock::sendSignal(BlockReference ref, 
			   GlobalSignalNumber gsn, 
                           Signal25* signal,
			   Uint32 length, 
			   JobBufferLevel jobBuffer,
			   LinearSectionPtr ptr[3],
			   Uint32 noOfSections) const {
  
  BlockReference sendBRef = reference();
  
  Uint32 recBlock = refToBlock(ref);
  Uint32 recNode   = refToNode(ref);
  Uint32 ourProcessor         = globalData.ownId;
  
  check_sections(signal, signal->header.m_noOfSections, noOfSections);
  
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
    bool ok = true;
    Ptr<SectionSegment> segptr[3];
    for(Uint32 i = 0; i<noOfSections; i++){
      ok &= ::import(SB_SP_ARG segptr[i], ptr[i].p, ptr[i].sz);
      signal->theData[length+i] = segptr[i].i;
    }

    if (unlikely(! ok))
    {
      handle_out_of_longsignal_memory(signal);
    }
    
#ifdef NDBD_MULTITHREADED
    if (jobBuffer == JBB)
      sendlocal(m_threadId, &signal->header, signal->theData,
                signal->theData+length);
    else
      sendprioa(m_threadId, &signal->header, signal->theData,
                signal->theData+length);
#else
    globalScheduler.execute(signal, jobBuffer);
#endif
    signal->header.m_noOfSections = 0;
    return;
  } else { 
    // send distributed Signal
    SignalHeader sh;

    Uint32 tTrace = signal->getTrace();
    Uint32 noOfSections = signal->header.m_noOfSections;
    
    sh.theVerId_signalNumber   = gsn;
    sh.theReceiversBlockNumber = recBlock;
    sh.theSendersBlockRef      = refToBlock(sendBRef);
    sh.theLength               = length;
    sh.theTrace                = tTrace;
    sh.theSignalId             = tSignalId;
    sh.m_noOfSections          = noOfSections;
    sh.m_fragmentInfo          = tFragInfo;
    
#ifdef TRACE_DISTRIBUTED
    g_eventLogger->info("send: %s(%d) to (%s, %d)", getSignalName(gsn), gsn,
                        getBlockName(recBlock), recNode);
#endif

    SendStatus ss;
#ifdef NDBD_MULTITHREADED
    ss = mt_send_remote(m_threadId, &sh, jobBuffer, &signal->theData[0],
                        recNode, ptr);
#else
    TrpId trp_id = 0;
    ss = globalTransporterRegistry.
           prepareSend(getNonMTTransporterSendHandle(),
                       &sh, jobBuffer,
                       &signal->theData[0], recNode, trp_id,
                       ptr);
#endif

    if (unlikely(! (ss == SEND_OK ||
                    ss == SEND_BLOCKED ||
                    ss == SEND_DISCONNECTED)))
    {
      handle_send_failed(ss, signal, recNode, ptr);
    }
  }

  signal->header.m_noOfSections = 0;
  signal->header.m_fragmentInfo = 0;
  return;
}

void 
SimulatedBlock::sendSignal(NodeReceiverGroup rg, 
			   GlobalSignalNumber gsn, 
                           Signal25* signal,
			   Uint32 length, 
			   JobBufferLevel jobBuffer,
			   LinearSectionPtr ptr[3],
			   Uint32 noOfSections) const {
  
  Uint32 tSignalId = signal->header.theSignalId;
  Uint32 tTrace    = signal->getTrace();
  Uint32 tFragInfo = signal->header.m_fragmentInfo;
  
  Uint32 ourProcessor = globalData.ownId;
  Uint32 recBlock = rg.m_block;
  
  check_sections(signal, signal->header.m_noOfSections, noOfSections);
  
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
  sh.theSendersBlockRef      = refToBlock(reference());
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
    bool ok = true;
    Ptr<SectionSegment> segptr[3];
    for(Uint32 i = 0; i<noOfSections; i++){
      ok &= ::import(SB_SP_ARG segptr[i], ptr[i].p, ptr[i].sz);
      signal->theData[length+i] = segptr[i].i;
    }

    if (unlikely(! ok))
    {
      handle_out_of_longsignal_memory(signal);
    }

#ifdef NDBD_MULTITHREADED
    if (jobBuffer == JBB)
      sendlocal(m_threadId, &signal->header, signal->theData,
                signal->theData + length);
    else
      sendprioa(m_threadId, &signal->header, signal->theData,
                signal->theData + length);
#else
    globalScheduler.execute(signal, jobBuffer);
#endif
    
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
    g_eventLogger->info("send: %s(%d) to (%s, %d)", getSignalName(gsn), gsn,
                        getBlockName(recBlock), recNode);
#endif

    SendStatus ss;
#ifdef NDBD_MULTITHREADED
    ss = mt_send_remote(m_threadId, &sh, jobBuffer, &signal->theData[0],
                        recNode, ptr);
#else
    TrpId trp_id = 0;
    ss = globalTransporterRegistry.
           prepareSend(getNonMTTransporterSendHandle(),
                       &sh, jobBuffer,
                       &signal->theData[0], recNode, trp_id,
                       ptr);
#endif

    if (unlikely(! (ss == SEND_OK ||
                    ss == SEND_BLOCKED ||
                    ss == SEND_DISCONNECTED)))
    {
      handle_send_failed(ss, signal, recNode, ptr);
    }
  }
  
  signal->header.m_noOfSections = 0;
  signal->header.m_fragmentInfo = 0;
  
  return;
}

void
SimulatedBlock::sendSignal(BlockReference ref,
			   GlobalSignalNumber gsn,
                           Signal25* signal,
			   Uint32 length,
			   JobBufferLevel jobBuffer,
			   SectionHandle* sections) const {

  Uint32 noOfSections = sections->m_cnt;
  BlockReference sendBRef = reference();

  Uint32 recBlock = refToBlock(ref);
  Uint32 recNode   = refToNode(ref);
  Uint32 ourProcessor         = globalData.ownId;

  check_sections(signal, signal->header.m_noOfSections, noOfSections);

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
                                   sections->m_ptr, noOfSections);
  }
#endif

  if(recNode == ourProcessor || recNode == 0) {
    signal->header.theSendersSignalId = tSignalId;
    signal->header.theSendersBlockRef = sendBRef;

    /**
     * We have to copy the data
     */
    Uint32 * dst = signal->theData + length;
    * dst ++ = sections->m_ptr[0].i;
    * dst ++ = sections->m_ptr[1].i;
    * dst ++ = sections->m_ptr[2].i;

#ifdef NDBD_MULTITHREADED
    if (jobBuffer == JBB)
      sendlocal(m_threadId, &signal->header, signal->theData,
                signal->theData + length);
    else
      sendprioa(m_threadId, &signal->header, signal->theData,
                signal->theData + length);
#else
    globalScheduler.execute(signal, jobBuffer);
#endif
  } else {
    // send distributed Signal
    SignalHeader sh;

    Uint32 tTrace = signal->getTrace();

    sh.theVerId_signalNumber   = gsn;
    sh.theReceiversBlockNumber = recBlock;
    sh.theSendersBlockRef      = refToBlock(sendBRef);
    sh.theLength               = length;
    sh.theTrace                = tTrace;
    sh.theSignalId             = tSignalId;
    sh.m_noOfSections          = noOfSections;
    sh.m_fragmentInfo          = tFragInfo;

#ifdef TRACE_DISTRIBUTED
    g_eventLogger->info("send: %s(%d) to (%s, %d)", getSignalName(gsn), gsn,
                        getBlockName(recBlock), recNode);
#endif

    SendStatus ss;
#ifdef NDBD_MULTITHREADED
    ss = mt_send_remote(m_threadId, &sh, jobBuffer, &signal->theData[0],
                        recNode, &g_sectionSegmentPool, sections->m_ptr);
#else
    TrpId trp_id = 0;
    ss = globalTransporterRegistry.
           prepareSend(getNonMTTransporterSendHandle(),
                       &sh, jobBuffer,
                       &signal->theData[0], recNode, trp_id,
                       g_sectionSegmentPool, sections->m_ptr);
#endif

    if (unlikely(! (ss == SEND_OK ||
                    ss == SEND_BLOCKED ||
                    ss == SEND_DISCONNECTED)))
    {
      handle_send_failed(ss, signal, recNode, sections->m_ptr);
    }

    ::releaseSections(SB_SP_ARG noOfSections, sections->m_ptr);
  }

  signal->header.m_noOfSections = 0;
  signal->header.m_fragmentInfo = 0;
  sections->m_cnt = 0;
  return;
}

void
SimulatedBlock::sendSignal(NodeReceiverGroup rg,
			   GlobalSignalNumber gsn,
                           Signal25* signal,
			   Uint32 length,
			   JobBufferLevel jobBuffer,
			   SectionHandle * sections) const {

  Uint32 noOfSections = sections->m_cnt;
  Uint32 tSignalId = signal->header.theSignalId;
  Uint32 tTrace    = signal->getTrace();
  Uint32 tFragInfo = signal->header.m_fragmentInfo;

  Uint32 ourProcessor = globalData.ownId;
  Uint32 recBlock = rg.m_block;

  check_sections(signal, signal->header.m_noOfSections, noOfSections);

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
  sh.theSendersBlockRef      = refToBlock(reference());
  sh.theLength               = length;
  sh.theTrace                = tTrace;
  sh.theSignalId             = tSignalId;
  sh.m_noOfSections          = noOfSections;
  sh.m_fragmentInfo          = tFragInfo;

  /**
   * Check own node
   */
  bool release = true;
  if(rg.m_nodes.get(0) || rg.m_nodes.get(ourProcessor))
  {
    release = false;
#ifdef VM_TRACE
    if(globalData.testOn){
      globalSignalLoggers.sendSignal(signal->header,
				     jobBuffer,
				     &signal->theData[0],
				     ourProcessor,
                                     sections->m_ptr, noOfSections);
    }
#endif
    /**
     * We have to copy the data
     */
    Uint32 * dst = signal->theData + length;
    * dst ++ = sections->m_ptr[0].i;
    * dst ++ = sections->m_ptr[1].i;
    * dst ++ = sections->m_ptr[2].i;
#ifdef NDBD_MULTITHREADED
    if (jobBuffer == JBB)
      sendlocal(m_threadId, &signal->header, signal->theData,
                signal->theData + length);
    else
      sendprioa(m_threadId, &signal->header, signal->theData,
                signal->theData + length);
#else
    globalScheduler.execute(signal, jobBuffer);
#endif

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
                                     sections->m_ptr, noOfSections);
    }
#endif

#ifdef TRACE_DISTRIBUTED
    g_eventLogger->info("send: %s(%d) to (%s, %d)", getSignalName(gsn), gsn,
                        getBlockName(recBlock), recNode);
#endif

    SendStatus ss;
#ifdef NDBD_MULTITHREADED
    ss = mt_send_remote(m_threadId, &sh, jobBuffer, &signal->theData[0],
                        recNode, &g_sectionSegmentPool, sections->m_ptr);
#else
    TrpId trp_id = 0;
    ss = globalTransporterRegistry.
           prepareSend(getNonMTTransporterSendHandle(),
                       &sh, jobBuffer,
                       &signal->theData[0], recNode, trp_id,
                       g_sectionSegmentPool, sections->m_ptr);
#endif

    if (unlikely(! (ss == SEND_OK ||
                    ss == SEND_BLOCKED ||
                    ss == SEND_DISCONNECTED)))
    {
      handle_send_failed(ss, signal, recNode, sections->m_ptr);
    }
  }

  if (release)
  {
    ::releaseSections(SB_SP_ARG noOfSections, sections->m_ptr);
  }

  sections->m_cnt = 0;
  signal->header.m_noOfSections = 0;
  signal->header.m_fragmentInfo = 0;

  return;
}

void
SimulatedBlock::sendSignalNoRelease(BlockReference ref,
                                    GlobalSignalNumber gsn,
                                    Signal25* signal,
                                    Uint32 length,
                                    JobBufferLevel jobBuffer,
                                    SectionHandle* sections) const {

  /**
   * Implementation the same as sendSignal(), except that
   * the sections are duplicated when sending locally, and
   * not released
   */

  Uint32 noOfSections = sections->m_cnt;
  BlockReference sendBRef = reference();

  Uint32 recBlock = refToBlock(ref);
  Uint32 recNode   = refToNode(ref);
  Uint32 ourProcessor         = globalData.ownId;

  check_sections(signal, signal->header.m_noOfSections, noOfSections);

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
                                   sections->m_ptr, noOfSections);
  }
#endif

  if(recNode == ourProcessor || recNode == 0) {
    signal->header.theSendersSignalId = tSignalId;
    signal->header.theSendersBlockRef = sendBRef;

    Uint32 * dst = signal->theData + length;

    /* We need to copy the segmented section data into separate
     * sections when sending locally and keeping a copy ourselves
     */
    for (Uint32 sec=0; sec < noOfSections; sec++)
    {
      Uint32 secCopy;
      if (unlikely(! ::dupSection(SB_SP_ARG secCopy, sections->m_ptr[sec].i)))
      {
        handle_out_of_longsignal_memory(signal);
        return;
      }
      * dst ++ = secCopy;
    }

#ifdef NDBD_MULTITHREADED
    if (jobBuffer == JBB)
      sendlocal(m_threadId, &signal->header, signal->theData,
                signal->theData + length);
    else
      sendprioa(m_threadId, &signal->header, signal->theData,
                signal->theData + length);
#else
    globalScheduler.execute(signal, jobBuffer);
#endif
  } else {
    // send distributed Signal
    SignalHeader sh;

    Uint32 tTrace = signal->getTrace();

    sh.theVerId_signalNumber   = gsn;
    sh.theReceiversBlockNumber = recBlock;
    sh.theSendersBlockRef      = refToBlock(sendBRef);
    sh.theLength               = length;
    sh.theTrace                = tTrace;
    sh.theSignalId             = tSignalId;
    sh.m_noOfSections          = noOfSections;
    sh.m_fragmentInfo          = tFragInfo;

#ifdef TRACE_DISTRIBUTED
    g_eventLogger->info("send: %s(%d) to (%s, %d)", getSignalName(gsn), gsn,
                        getBlockName(recBlock), recNode);
#endif

    SendStatus ss;
#ifdef NDBD_MULTITHREADED
    ss = mt_send_remote(m_threadId, &sh, jobBuffer, &signal->theData[0],
                        recNode, &g_sectionSegmentPool, sections->m_ptr);
#else
    TrpId trp_id = 0;
    ss = globalTransporterRegistry.
           prepareSend(getNonMTTransporterSendHandle(),
                       &sh, jobBuffer,
                       &signal->theData[0], recNode, trp_id,
                       g_sectionSegmentPool, sections->m_ptr);
#endif

    if (unlikely(! (ss == SEND_OK ||
                    ss == SEND_BLOCKED ||
                    ss == SEND_DISCONNECTED)))
    {
      handle_send_failed(ss, signal, recNode, sections->m_ptr);
    }
  }

  signal->header.m_noOfSections = 0;
  signal->header.m_fragmentInfo = 0;
  return;
}

void
SimulatedBlock::sendSignalNoRelease(NodeReceiverGroup rg,
                                    GlobalSignalNumber gsn,
                                    Signal25* signal,
                                    Uint32 length,
                                    JobBufferLevel jobBuffer,
                                    SectionHandle * sections) const {
  /**
   * Implementation the same as sendSignal(), except that
   * the sections are duplicated when sending locally, and
   * not released
   */

  Uint32 noOfSections = sections->m_cnt;
  Uint32 tSignalId = signal->header.theSignalId;
  Uint32 tTrace    = signal->getTrace();
  Uint32 tFragInfo = signal->header.m_fragmentInfo;

  Uint32 ourProcessor = globalData.ownId;
  Uint32 recBlock = rg.m_block;

  check_sections(signal, signal->header.m_noOfSections, noOfSections);

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
  sh.theSendersBlockRef      = refToBlock(reference());
  sh.theLength               = length;
  sh.theTrace                = tTrace;
  sh.theSignalId             = tSignalId;
  sh.m_noOfSections          = noOfSections;
  sh.m_fragmentInfo          = tFragInfo;

  /**
   * Check own node
   */
  if(rg.m_nodes.get(0) || rg.m_nodes.get(ourProcessor))
  {
#ifdef VM_TRACE
    if(globalData.testOn){
      globalSignalLoggers.sendSignal(signal->header,
				     jobBuffer,
				     &signal->theData[0],
				     ourProcessor,
                                     sections->m_ptr, noOfSections);
    }
#endif

    Uint32 * dst = signal->theData + length;

    /* We need to copy the segmented section data into separate
     * sections when sending locally and keeping a copy ourselves
     */
    for (Uint32 sec=0; sec < noOfSections; sec++)
    {
      Uint32 secCopy;
      if (unlikely(! ::dupSection(SB_SP_ARG secCopy, sections->m_ptr[sec].i)))
      {
        handle_out_of_longsignal_memory(signal);
        return;
      }
      * dst ++ = secCopy;
    }

#ifdef NDBD_MULTITHREADED
    if (jobBuffer == JBB)
      sendlocal(m_threadId, &signal->header, signal->theData,
                signal->theData + length);
    else
      sendprioa(m_threadId, &signal->header, signal->theData,
                signal->theData + length);
#else
    globalScheduler.execute(signal, jobBuffer);
#endif

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
                                     sections->m_ptr, noOfSections);
    }
#endif

#ifdef TRACE_DISTRIBUTED
    g_eventLogger->info("send: %s(%d) to (%s, %d)", getSignalName(gsn), gsn,
                        getBlockName(recBlock), recNode);
#endif

    SendStatus ss;
#ifdef NDBD_MULTITHREADED
    ss = mt_send_remote(m_threadId, &sh, jobBuffer, &signal->theData[0],
                        recNode, &g_sectionSegmentPool, sections->m_ptr);
#else
    TrpId trp_id = 0;
    ss = globalTransporterRegistry.
           prepareSend(getNonMTTransporterSendHandle(),
                       &sh, jobBuffer,
                       &signal->theData[0], recNode, trp_id,
                       g_sectionSegmentPool, sections->m_ptr);
#endif

    if (unlikely(! (ss == SEND_OK ||
                    ss == SEND_BLOCKED ||
                    ss == SEND_DISCONNECTED)))
    {
      handle_send_failed(ss, signal, recNode, sections->m_ptr);
    }
  }

  signal->header.m_noOfSections = 0;
  signal->header.m_fragmentInfo = 0;

  return;
}


void
SimulatedBlock::sendSignalWithDelay(BlockReference ref, 
				    GlobalSignalNumber gsn,
                                    Signal25* signal,
				    Uint32 delayInMilliSeconds, 
				    Uint32 length) const {
  
  BlockNumber bnr = refToBlock(ref);

  check_sections(signal, signal->header.m_noOfSections, 0);
  
  signal->header.theLength = length;
  signal->header.theSendersSignalId = signal->header.theSignalId;
  signal->header.theVerId_signalNumber = gsn;
  signal->header.theReceiversBlockNumber = bnr;
  signal->header.theSendersBlockRef = reference();

  assert(length <= 25);
#ifdef VM_TRACE
  {
    if(globalData.testOn){
      globalSignalLoggers.sendSignalWithDelay(delayInMilliSeconds,
					      signal->header,
					      0,
					      &signal->theData[0], 
					      globalData.ownId);
    }
  }
#endif

#ifdef NDBD_MULTITHREADED
  senddelay(m_threadId, &signal->header, delayInMilliSeconds);
#else
  globalTimeQueue.insert(signal, delayInMilliSeconds);
#endif

  // before 2nd parameter to globalTimeQueue.insert
  // (Priority)theSendSig[sigIndex].jobBuffer
}

void
SimulatedBlock::sendSignalWithDelay(BlockReference ref,
				    GlobalSignalNumber gsn,
                                    Signal25* signal,
				    Uint32 delayInMilliSeconds,
				    Uint32 length,
				    SectionHandle * sections) const {

  Uint32 noOfSections = sections->m_cnt;
  BlockNumber bnr = refToBlock(ref);

  BlockReference sendBRef = reference();

  if (bnr == 0) {
    bnr_error();
  }//if

  check_sections(signal, signal->header.m_noOfSections, noOfSections);

  signal->header.theLength = length;
  signal->header.theSendersSignalId = signal->header.theSignalId;
  signal->header.theSendersBlockRef = sendBRef;
  signal->header.theVerId_signalNumber = gsn;
  signal->header.theReceiversBlockNumber = bnr;
  signal->header.m_noOfSections = noOfSections;

  assert(length + noOfSections <= 25);
  Uint32 * dst = signal->theData + length;
  * dst ++ = sections->m_ptr[0].i;
  * dst ++ = sections->m_ptr[1].i;
  * dst ++ = sections->m_ptr[2].i;

#ifdef VM_TRACE
  {
    if(globalData.testOn){
      globalSignalLoggers.sendSignalWithDelay(delayInMilliSeconds,
					      signal->header,
					      0,
					      &signal->theData[0],
					      globalData.ownId);
    }
  }
#endif

#ifdef NDBD_MULTITHREADED
  senddelay(m_threadId, &signal->header, delayInMilliSeconds);
#else
  globalTimeQueue.insert(signal, delayInMilliSeconds);
#endif

  signal->header.m_noOfSections = 0;
  signal->header.m_fragmentInfo = 0;
  sections->m_cnt = 0;
}

/*
 * Copy implementation for sendSignalOverAllLinks from sendSignal, excluding part for
 * local send.
 */
void
SimulatedBlock::sendSignalOverAllLinks(BlockReference ref,
                                 GlobalSignalNumber gsn,
                                 Signal25* signal,
                                 Uint32 length,
                                 JobBufferLevel jobBuffer) const
{

  BlockReference sendBRef = reference();

  Uint32 recBlock = refToBlock(ref);
  Uint32 recNode   = refToNode(ref);
  Uint32 ourProcessor         = globalData.ownId;

  ndbrequire(signal->header.m_noOfSections == 0);
  check_sections(signal, signal->header.m_noOfSections, 0);

  signal->header.theLength = length;
  signal->header.theVerId_signalNumber = gsn;
  signal->header.theReceiversBlockNumber = recBlock;
  signal->header.m_noOfSections = 0;

  Uint32 tSignalId = signal->header.theSignalId;

  if (unlikely((length == 0) || length > 25 || (recBlock == 0)))
  {
    signal_error(gsn, length, recBlock, __FILE__, __LINE__);
    return;
  }//if
#ifdef VM_TRACE
  if (globalData.testOn){
    Uint16 proc =
      (recNode == 0 ? globalData.ownId : recNode);
    signal->header.theSendersBlockRef = sendBRef;
    globalSignalLoggers.sendSignal(signal->header,
                                   jobBuffer,
                                   &signal->theData[0],
                                   proc);
  }
#endif

  // Local send part not copied from sendSignal.
  ndbrequire(recNode != ourProcessor);
  ndbrequire(recNode != 0);

  // send distributed Signal
  SignalHeader sh;

  Uint32 tTrace = signal->getTrace();

  sh.theVerId_signalNumber   = gsn;
  sh.theReceiversBlockNumber = recBlock;
  sh.theSendersBlockRef      = refToBlock(sendBRef);
  sh.theLength               = length;
  sh.theTrace                = tTrace;
  sh.theSignalId             = tSignalId;
  sh.m_noOfSections          = 0;
  sh.m_fragmentInfo          = 0;

#ifdef TRACE_DISTRIBUTED
  g_eventLogger->info("send: %s(%d) to (%s, %d)", getSignalName(gsn), gsn,
                      getBlockName(recBlock), recNode);
#endif

  SendStatus ss;
#ifdef NDBD_MULTITHREADED
  ss = mt_send_remote_over_all_links(m_threadId, &sh, jobBuffer,
                                     &signal->theData[0], recNode);
#else
  TrpBitmask trp_ids;
  ss = globalTransporterRegistry.
         prepareSendOverAllLinks(getNonMTTransporterSendHandle(),
                                 &sh, jobBuffer,
                                 &signal->theData[0], recNode, trp_ids);
#endif

  if (unlikely(! (ss == SEND_OK ||
                  ss == SEND_BLOCKED ||
                  ss == SEND_DISCONNECTED)))
  {
    handle_send_failed(ss, signal, recNode, (LinearSectionPtr*)NULL);
  }
  return;
}

void
SimulatedBlock::release(SegmentedSectionPtr & ptr)
{
  ::release(SB_SP_ARG ptr);
}

void
SimulatedBlock::releaseSection(Uint32 firstSegmentIVal)
{
  ::releaseSection(SB_SP_ARG firstSegmentIVal);
}

void
SimulatedBlock::releaseSections(SectionHandle& handle)
{
  ::releaseSections(SB_SP_ARG handle.m_cnt, handle.m_ptr);
  handle.m_cnt = 0;
}

bool
SimulatedBlock::appendToSection(Uint32& firstSegmentIVal, const Uint32* src, Uint32 len)
{
  return ::appendToSection(SB_SP_ARG firstSegmentIVal, src, len);
}

bool
SimulatedBlock::import(Ptr<SectionSegment> & first, const Uint32 * src, Uint32 len)
{
  return ::import(SB_SP_ARG first, src, len);
}

bool
SimulatedBlock::import(SegmentedSectionPtr& ptr, const Uint32* src, Uint32 len) const
{
  Ptr<SectionSegment> tmp;
  if (::import(SB_SP_ARG tmp, src, len))
  {
    ptr.i = tmp.i;
    ptr.p = tmp.p;
    ptr.sz = len;
    return true;
  }
  return false;
}

bool
SimulatedBlock::import(SectionHandle * dst,
                       LinearSectionPtr src[3],
                       Uint32 cnt)
{
  ndbassert(dst->m_cnt == 0);
  if (dst->m_cnt)
  {
    releaseSections(* dst);
  }

  for (Uint32 i = 0; i < cnt; i++)
  {
    if (unlikely(!import(dst->m_ptr[i], src[i].p, src[i].sz)))
    {
      if (i)
      {
        dst->m_cnt = i - 1;
        releaseSections(* dst);
        return false;
      }
    }
  }
  dst->m_cnt = cnt;
  return true;
}


bool
SimulatedBlock::dupSection(Uint32& copyFirstIVal, Uint32 srcFirstIVal)
{
  return ::dupSection(SB_SP_ARG copyFirstIVal, srcFirstIVal);
}

bool
SimulatedBlock::writeToSection(Uint32 firstSegmentIVal, Uint32 offset,
                               const Uint32* src, Uint32 len)
{
  return ::writeToSection(firstSegmentIVal, offset, src, len);
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

/**
 * Return pointer to a const NewVARIABLE object indexed by
 * blockNo, instanceNo and varNo.
 * Will return NULL if no such variable exists
 */
const NewVARIABLE *
SimulatedBlock::getBatVar(Uint16 blockNo, Uint32 instanceNo, Uint32 varNo){
  assert(blockNo == blockToMain(blockNo));
  /* Check blockNo in range */
  if (unlikely(blockNo > MAX_BLOCK_NO ||
               blockNo < MIN_BLOCK_NO))
  {
    assert(false);
    return NULL;
  }
  SimulatedBlock * sb = globalData.getBlock(blockNo);
  if (sb != NULL && instanceNo != 0)
  {
    /* Lookup instance within block type */
    sb = sb->getInstance(instanceNo);
  }

  /* Check block exists, Bat exists, Var exists */
  if(unlikely(sb == NULL))
  {
    return NULL;
  }
  if (unlikely(sb->NewVarRef == NULL))
  {
    return NULL;
  }
  if (unlikely(varNo >= sb->theBATSize))
  {
    return NULL;
  }
  return &sb->NewVarRef[varNo];
}

void* SimulatedBlock::allocRecord(const char * type, size_t s, size_t n, bool clear, Uint32 paramId)
{
  return allocRecordAligned(type, s, n, 0, 0, clear, paramId);
}

void* 
SimulatedBlock::allocRecordAligned(const char * type, size_t s, size_t n, void **unaligned_buffer, Uint32 align, bool clear, Uint32 paramId)
{

  void * p = NULL;
  Uint32 over_alloc = unaligned_buffer ? (align - 1) : 0;
  size_t size = n*s + over_alloc;
  Uint64 real_size = (Uint64)((Uint64)n)*((Uint64)s) + over_alloc;
  refresh_watch_dog(9);
  if (real_size > 0){
#if defined(VM_TRACE_MEM)
    g_eventLogger->info("%s::allocRecord(%s, %zu, %zu) = %llu bytes",
	                getBlockName(number()),
	                type,
	                s,
	                n,
	                real_size);
#endif
    if( real_size == (Uint64)size )
      p = ndbd_malloc_watched(size, get_watch_dog());
    if (p == NULL){
      char buf1[255];
      char buf2[255];
      struct ndb_mgm_param_info param_info;
      size_t size = sizeof(ndb_mgm_param_info);

      if(0 != paramId && 0 == ndb_mgm_get_db_parameter_info(paramId, &param_info, &size)) {
        BaseString::snprintf(buf1, sizeof(buf1), "%s could not allocate memory for parameter %s", 
	         getBlockName(number()), param_info.m_name);
      } else {
        BaseString::snprintf(buf1, sizeof(buf1), "%s could not allocate memory for %s", 
	         getBlockName(number()), type);
      }
      BaseString::snprintf(buf2, sizeof(buf2), "Requested: %ux%u = %llu bytes", 
	       (Uint32)s, (Uint32)n, (Uint64)real_size);
      ERROR_SET(fatal, NDBD_EXIT_MEMALLOC, buf1, buf2);
    }

    if(clear){
      char * ptr = (char*)p;
      const Uint32 chunk = 128 * 1024;
      while(size > chunk){
	refresh_watch_dog(9);
	memset(ptr, 0, chunk);
	ptr += chunk;
	size -= chunk;
      }
      refresh_watch_dog(9);
      memset(ptr, 0, size);
    }
    if (unaligned_buffer)
    {
      *unaligned_buffer = p;
      p = (void *)(((UintPtr)p + over_alloc) & ~(UintPtr)(over_alloc));
#ifdef VM_TRACE
      g_eventLogger->info("'%s' (%u) %llu %llu, alignment correction %u bytes",
                          type, align, (Uint64)p, (Uint64)p+n*s,
                          (Uint32)((UintPtr)p - (UintPtr)*unaligned_buffer));
#endif
    }
  }
  return p;
}

void 
SimulatedBlock::deallocRecord(void ** ptr, 
			      const char * type, size_t s, size_t n){
  (void)type;

  if(* ptr != 0){
      ndbd_free(* ptr, n*s);
    * ptr = 0;
  }
}

int
SimulatedBlock::sortchunks(const void * _e0, const void * _e1)
{
  const AllocChunk *p0 = (const AllocChunk*)_e0;
  const AllocChunk *p1 = (const AllocChunk*)_e1;

  if (p0->ptrI > p1->ptrI)
    return 1;
  if (p0->ptrI < p1->ptrI)
    return -1;
  return 0;
}

Uint32
SimulatedBlock::allocChunks(AllocChunk dst[],
                            Uint32 arraysize,
                            Uint32 rg,
                            Uint32 pages,
                            Uint32 paramId)
{
  const Uint32 save = pages; // For fail
  Uint32 i = 0;
  for (; i<arraysize && pages > 0; i++)
  {
    Uint32 cnt = pages;
    m_ctx.m_mm.alloc_pages(rg, &dst[i].ptrI, &cnt, 1);
    if (unlikely(cnt == 0))
      goto fail;
    // DBLQH error range since this function is only used there.
    if (ERROR_INSERTED(5107))
    {
      /*
       * Try chop up the allocation in chunks, with unused pages between the
       * chunks.
       *
       * First allocation will typically succeed allocate all memory, we keep
       * the last pages in the range.  Then the next allocation will also
       * succeed to allocate all remaining pages (if there are enough page
       * memory), we keep the last pages again, and there will be a gap between
       * the chunks.  If there are too little page memory, allocation may
       * succeed reusing the gaps and there might not be gaps between the
       * chunks, if that happen try configure more page memory for test.
       */
      const Uint32 min_pages_per_chunk = pages / (arraysize - i) + 1;
      if (cnt > min_pages_per_chunk)
      {
        const Uint32 gap_pages = cnt - min_pages_per_chunk;
        m_ctx.m_mm.release_pages(rg, dst[i].ptrI, gap_pages);
        dst[i].ptrI += gap_pages;
        cnt -= gap_pages;
      }
    }
    pages -= cnt;
    dst[i].cnt = cnt;
  }
  if (unlikely(pages != 0))
    goto fail;

  qsort(dst, i, sizeof(dst[0]), sortchunks);
  return i;

fail:
  char buf1[255];
  char buf2[255];
  struct ndb_mgm_param_info param_info;
  size_t size = sizeof(ndb_mgm_param_info);

  if (ndb_mgm_get_db_parameter_info(paramId, &param_info, &size) != 0)
  {
    ndbassert(false);
    param_info.m_name = "<unknown>";
  }

  BaseString::snprintf(buf1, sizeof(buf1),
                       "%s could not allocate memory for parameter %s",
                       getBlockName(number()), param_info.m_name);
  BaseString::snprintf(buf2, sizeof(buf2), "Requested: %llu bytes",
                       Uint64(save) * sizeof(GlobalPage));
  ERROR_SET(fatal, NDBD_EXIT_MEMALLOC, buf1, buf2);
  return 0;
}

void
SimulatedBlock::refresh_watch_dog(Uint32 place)
{
#ifdef NDBD_MULTITHREADED
  (*m_watchDogCounter) = place;
#else
  globalData.incrementWatchDogCounter(place);
#endif
}

volatile Uint32*
SimulatedBlock::get_watch_dog()
{
#ifdef NDBD_MULTITHREADED
  return m_watchDogCounter;
#else
  return globalData.getWatchDogPtr();
#endif
}

void
SimulatedBlock::update_watch_dog_timer(Uint32 interval)
{
  extern EmulatorData globalEmulatorData;
  globalEmulatorData.theWatchDog->setCheckInterval(interval);
}

void
SimulatedBlock::progError(int line, int err_code, const char* extra,
                          const char* check) const {
  jamNoBlock();

  const char *aBlockName = getBlockName(number(), "VM Kernel");

  // Pack status of interesting config variables 
  // so that we can print them in error.log
  int magicStatus = 
    (m_ctx.m_config.stopOnError()<<1) + 
    (m_ctx.m_config.getInitialStart()<<2);

  /* Add line number and failed expression to block name */
  char buf[500];
  /*Add the check to the log message only if default value of ""
    is over-written. */
  if(native_strcasecmp(check,"") == 0)
    BaseString::snprintf(&buf[0], 100, "%s (Line: %d) 0x%.8x",
        aBlockName, line, magicStatus);
  else
    BaseString::snprintf(&buf[0], sizeof(buf),
        "%s (Line: %d) 0x%.8x Check %.400s failed", aBlockName,
        line, magicStatus, check);

  ErrorReporter::handleError(err_code, extra, buf);

}

#define MAX_EVENT_REP_SIZE_BYTES (MAX_EVENT_REP_SIZE_WORDS * 4)
void 
SimulatedBlock::infoEvent(const char * msg, ...) const
{
  if(msg == 0)
    return;
  
  Signal25 signalT;
  signalT.theData[0] = NDB_LE_InfoEvent;
  Uint32 buf_str[MAX_EVENT_REP_SIZE_WORDS];
  char * buf = (char *)&buf_str[1];
  
  buf_str[0] = signalT.theData[0];
  va_list ap;
  va_start(ap, msg);
  BaseString::vsnprintf(buf, MAX_EVENT_REP_SIZE_BYTES - 5, msg, ap);
  va_end(ap);
  
  size_t len = strlen(buf) + 1;
  if(len >= (MAX_EVENT_REP_SIZE_BYTES - 5))
  {
    len = MAX_EVENT_REP_SIZE_BYTES - 4;
    buf[MAX_EVENT_REP_SIZE_BYTES - 5] = 0;
  }

  SegmentedSectionPtr segptr;
  Uint32 len_words = 1 + ((len + 3) / 4);
  bool ok = import(segptr,
                   &buf_str[0],
                   len_words);
  signalT.theData[1] = segptr.i;
  if (!ok)
  {
    return;
  }
  /**
   * Init and put it into the job buffer
   */
  memset(&signalT.header, 0, sizeof(SignalHeader));
  
  const Signal * signal = globalScheduler.getVMSignals();
  Uint32 tTrace = signal->header.theTrace;
  Uint32 tSignalId = signal->header.theSignalId;
  
  signalT.header.theVerId_signalNumber   = GSN_EVENT_REP;
  signalT.header.theReceiversBlockNumber = CMVMI;
  signalT.header.theSendersBlockRef      = reference();
  signalT.header.theTrace                = tTrace;
  signalT.header.theSignalId             = tSignalId;
  signalT.header.theLength               = 1;
  signalT.header.m_noOfSections          = 1;
#ifdef NDBD_MULTITHREADED
  sendlocal(m_threadId,
            &signalT.header, signalT.theData, signalT.theData + 1);
#else
  globalScheduler.execute(&signalT.header, JBB, signalT.theData,
                          signalT.theData + 1);
#endif
}

void 
SimulatedBlock::warningEvent(const char * msg, ...)
{
  if(msg == 0)
    return;

  Signal25 signalT;
  signalT.theData[0] = NDB_LE_WarningEvent;
  Uint32 buf_str[MAX_EVENT_REP_SIZE_WORDS];
  char * buf = (char *)&buf_str[1];
  memset(&buf_str[0], 0, 4);
  
  va_list ap;
  va_start(ap, msg);
  BaseString::vsnprintf(buf, MAX_EVENT_REP_SIZE_BYTES - 5, msg, ap);
  va_end(ap);
  
  size_t len = strlen(buf) + 1;
  if(len >= (MAX_EVENT_REP_SIZE_BYTES - 5))
  {
    len = MAX_EVENT_REP_SIZE_BYTES - 4;
    buf[MAX_EVENT_REP_SIZE_BYTES - 5] = 0;
  }
  SegmentedSectionPtr segptr;
  Uint32 len_words = 1 + ((len + 3) / 4);
  bool ok = import(segptr,
                   &buf_str[0],
                   len_words);
  signalT.theData[1] = segptr.i;
  if (!ok)
  {
    return;
  }

  /**
   * Init and put it into the job buffer
   */
  memset(&signalT.header, 0, sizeof(SignalHeader));
  
  const Signal * signal = globalScheduler.getVMSignals();
  Uint32 tTrace = signal->header.theTrace;
  Uint32 tSignalId = signal->header.theSignalId;
  
  signalT.header.theVerId_signalNumber   = GSN_EVENT_REP;
  signalT.header.theReceiversBlockNumber = CMVMI;
  signalT.header.theSendersBlockRef      = reference();
  signalT.header.theTrace                = tTrace;
  signalT.header.theSignalId             = tSignalId;
  signalT.header.theLength               = 1;
  signalT.header.m_noOfSections          = 1;

#ifdef NDBD_MULTITHREADED
  sendlocal(m_threadId,
            &signalT.header, signalT.theData, signalT.theData + 1);
#else
  globalScheduler.execute(&signalT.header, JBB, signalT.theData,
                          signalT.theData + 1);
#endif
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
  if (signal->getLength() == 1)
  {
    SET_ERROR_INSERT_VALUE(signal->theData[0]);
  }
  else
  {
    SET_ERROR_INSERT_VALUE2(signal->theData[0], signal->theData[1]);
  }
}

void
SimulatedBlock::execSIGNAL_DROPPED_REP(Signal * signal){
  /* Note no need for fragmented signal handling as we are
   * going to crash this node
   */
  char msg[64];
  const SignalDroppedRep * const rep = (SignalDroppedRep *)&signal->theData[0];
  BaseString::snprintf(msg, sizeof(msg), "%s GSN: %u (%u,%u)", getBlockName(number()),
	   rep->originalGsn, rep->originalLength,rep->originalSectionCount);
  ErrorReporter::handleError(NDBD_EXIT_OUT_OF_LONG_SIGNAL_MEMORY,
			     msg,
			     __FILE__,
			     NST_ErrorHandler);
}

void
SimulatedBlock::execCONTINUE_FRAGMENTED(Signal * signal){
  jamEntry();

  ContinueFragmented * sig = (ContinueFragmented*)signal->getDataPtrSend();
  ndbrequire(signal->getSendersBlockRef() == reference()); /* Paranoia */

  switch (sig->type)
  {
  case ContinueFragmented::CONTINUE_SENDING :
  {
    jam();
    Ptr<FragmentSendInfo> fragPtr;
    
    c_segmentedFragmentSendList.first(fragPtr);  
    for(; !fragPtr.isNull();){
      jam();
      Ptr<FragmentSendInfo> copyPtr = fragPtr;
      c_segmentedFragmentSendList.next(fragPtr);
      
      sendNextSegmentedFragment(signal, * copyPtr.p);
      if(copyPtr.p->m_status == FragmentSendInfo::SendComplete){
        jam();
        if(copyPtr.p->m_callback.m_callbackFunction != 0) {
          jam();
          execute(signal, copyPtr.p->m_callback, 0);
        }//if
        c_segmentedFragmentSendList.release(copyPtr);
      }
    }
    
    c_linearFragmentSendList.first(fragPtr);  
    for(; !fragPtr.isNull();){
      jam(); 
      Ptr<FragmentSendInfo> copyPtr = fragPtr;
      c_linearFragmentSendList.next(fragPtr);
      
      sendNextLinearFragment(signal, * copyPtr.p);
      if(copyPtr.p->m_status == FragmentSendInfo::SendComplete){
        jam();
        if(copyPtr.p->m_callback.m_callbackFunction != 0) {
          jam();
          execute(signal, copyPtr.p->m_callback, 0);
        }//if
        c_linearFragmentSendList.release(copyPtr);
      }
    }
    
    if(c_segmentedFragmentSendList.isEmpty() && 
       c_linearFragmentSendList.isEmpty()){
      jam();
      c_fragSenderRunning = false;
      return;
    }
    
    sig->type = ContinueFragmented::CONTINUE_SENDING;
    sig->line = __LINE__;
    sendSignal(reference(), GSN_CONTINUE_FRAGMENTED, signal, 2, JBB);
    break;
  }
  case ContinueFragmented::CONTINUE_CLEANUP:
  {
    jam();
    
    const Uint32 callbackWords = (sizeof(Callback) + 3) >> 2;
    /* Check length of signal */
    ndbassert(signal->getLength() ==
              ContinueFragmented::CONTINUE_CLEANUP_FIXED_WORDS + 
              callbackWords);
    
    Callback cb;
    memcpy(&cb, &sig->cleanup.callbackStart, callbackWords << 2);

    doNodeFailureCleanup(signal,
                         sig->cleanup.failedNodeId,
                         sig->cleanup.resource,
                         sig->cleanup.cursor,
                         sig->cleanup.elementsCleaned,
                         cb);
    break;
  }
  default:
    ndbabort();
  }
}

void
SimulatedBlock::execSTOP_FOR_CRASH(Signal* signal)
{
#ifdef NDBD_MULTITHREADED
  mt_execSTOP_FOR_CRASH();
#endif
}

void
SimulatedBlock::execNODE_START_REP(Signal* signal)
{
}

void
SimulatedBlock::execAPI_START_REP(Signal* signal)
{
}

void
SimulatedBlock::execSEND_PACKED(Signal* signal)
{
}

ATTRIBUTE_NOINLINE
void
SimulatedBlock::handle_execute_error(GlobalSignalNumber gsn)
{
  /**
   * This method only called if an error has occurred
   */
  char errorMsg[255];
  if (!(gsn <= MAX_GSN)) {
    BaseString::snprintf(errorMsg, 255, "Illegal signal received (GSN %d too high)", gsn);
    ERROR_SET(fatal, NDBD_EXIT_PRGERR, errorMsg, errorMsg);
  }
  if (!(theSignalHandlerArray[gsn].m_execFunction != nullptr)) {
    BaseString::snprintf(errorMsg, 255, "Illegal signal received (GSN %d not added)", gsn);
    ERROR_SET(fatal, NDBD_EXIT_PRGERR, errorMsg, errorMsg);
  }
  ndbabort();
}

ATTRIBUTE_NOINLINE
void
SimulatedBlock::handle_sender_error(GlobalSignalNumber gsn, Signal *signal, SignalScope scope)
{
  const BlockReference ref = (signal->senderBlockRef());
  Uint32 nodeId = refToNode(ref);
  char errorMsg[255];
    switch (scope)
    {
    case SignalScope::Local:
    {
      CRASH_INSERTION(10054);
      BaseString::snprintf(errorMsg, 255, "Illegal signal %s received for SignalScope::Local (GSN %d from node %d, block 0x%X to 0x%X)", getSignalName(gsn), gsn, nodeId, refToMain(ref), refToMain(reference()));
      ERROR_SET(fatal, NDBD_EXIT_PRGERR, errorMsg, getBlockName(number()));
      break;
    }
    case SignalScope::Remote:
    {
      BaseString::snprintf(errorMsg, 255, "Illegal signal %s received for SignalScope::Remote (GSN %d from node %d, block 0x%X to 0x%X)", getSignalName(gsn), gsn, nodeId, refToMain(ref), refToMain(reference()));
      ERROR_SET(fatal, NDBD_EXIT_PRGERR, errorMsg, getBlockName(number()));
      break;
    }
    case SignalScope::Management:
    {
      BaseString::snprintf(errorMsg, 255, "Illegal signal %s received for SignalScope::Management (GSN %d from API node %d, block 0x%X to 0x%X)", getSignalName(gsn), gsn, nodeId, refToMain(ref), refToMain(reference()));
      ERROR_SET(fatal, NDBD_EXIT_PRGERR, errorMsg, getBlockName(number()));
      break;
    }
    case SignalScope::External:
      // Should not be reachable
      ndbassert(false);
      BaseString::snprintf(errorMsg, 255, "Illegal signal %s eceived for SignalScope::External (GSN %d from API node %d, block 0x%X to 0x%X)", getSignalName(gsn), gsn, nodeId, refToMain(ref), refToMain(reference()));
      ERROR_SET(fatal, NDBD_EXIT_PRGERR, errorMsg, getBlockName(number()));
      break;
    }
   ndbabort();
}

// MT LQH callback CONF via signal

const SimulatedBlock::CallbackEntry&
SimulatedBlock::getCallbackEntry(Uint32 ci)
{
  ndbrequire(m_callbackTableAddr != 0);
  const CallbackTable& ct = *m_callbackTableAddr;
  ndbrequire(ci < ct.m_count);
  return ct.m_entry[ci];
}

void
SimulatedBlock::sendCallbackConf(Signal* signal, Uint32 fullBlockNo,
                                 CallbackPtr& cptr,
                                 Uint32 senderData, Uint32 callbackInfo,
                                 Uint32 returnCode)
{
  Uint32 blockNo = blockToMain(fullBlockNo);
  Uint32 instanceNo = blockToInstance(fullBlockNo);
  SimulatedBlock* b = globalData.getBlock(blockNo, instanceNo);
  ndbrequire(b != 0);

  const CallbackEntry& ce = b->getCallbackEntry(cptr.m_callbackIndex);

  if (!isNdbMtLqh()) {
    Callback c;
    c.m_callbackFunction = ce.m_function;
    c.m_callbackData = cptr.m_callbackData;
    b->execute(signal, c, returnCode);

    if (ce.m_flags & CALLBACK_ACK) {
      jam();
      CallbackAck* ack = (CallbackAck*)signal->getDataPtrSend();
      ack->senderData = senderData;
      ack->callbackInfo = callbackInfo;
      EXECUTE_DIRECT(number(), GSN_CALLBACK_ACK,
                     signal, CallbackAck::SignalLength);
    }
  } else {
    CallbackConf* conf = (CallbackConf*)signal->getDataPtrSend();
    conf->senderData = senderData;
    conf->senderRef = reference();
    conf->callbackIndex = cptr.m_callbackIndex;
    conf->callbackData = cptr.m_callbackData;
    conf->callbackInfo = callbackInfo;
    conf->returnCode = returnCode;

    if (ce.m_flags & CALLBACK_DIRECT) {
      jam();
      EXECUTE_DIRECT_MT(blockNo, GSN_CALLBACK_CONF,
                        signal, CallbackConf::SignalLength, instanceNo);
    } else {
      jam();
      BlockReference ref = numberToRef(fullBlockNo, getOwnNodeId());
      sendSignal(ref, GSN_CALLBACK_CONF,
                 signal, CallbackConf::SignalLength, JBB);
    }
  }
  cptr.m_callbackIndex = ZNIL;
}

void
SimulatedBlock::execCALLBACK_CONF(Signal* signal)
{
  const CallbackConf* conf = (const CallbackConf*)signal->getDataPtr();

  Uint32 senderData = conf->senderData;
  Uint32 senderRef = conf->senderRef;
  Uint32 callbackIndex = conf->callbackIndex;
  Uint32 callbackData = conf->callbackData;
  Uint32 callbackInfo = conf->callbackInfo;
  Uint32 returnCode = conf->returnCode;
  ndbrequire(returnCode == 0);

  ndbrequire(m_callbackTableAddr != 0);
  const CallbackEntry& ce = getCallbackEntry(callbackIndex);
  CallbackFunction function = ce.m_function;

  Callback callback;
  callback.m_callbackFunction = function;
  callback.m_callbackData = callbackData;

  /**
   * For both PROCESS_LOG_SYNC_WAITERS and PROCESS_LOG_BUFFER_WAITERS,
   * sendCallbackConf() places logfile_group_id in senderData.
   * drop_table_log_buffer_callback() needs logfile_group_id.
  */
  execute(signal, callback, senderData);

  if (ce.m_flags & CALLBACK_ACK) {
    jam();
    CallbackAck* ack = (CallbackAck*)signal->getDataPtrSend();
    ack->senderData = senderData;
    ack->callbackInfo = callbackInfo;
    sendSignal(senderRef, GSN_CALLBACK_ACK,
               signal, CallbackAck::SignalLength, JBB);
  }
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
  fprintf(output, "-- %s : %u --\n", getBlockName(number()), (Uint32)sum);
  fprintf(output, "\n");
  fflush(output);
}

#endif

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

  Uint32 *sectionPtr = signal->m_sectionPtrI;
  
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
      ndbabort();
      return false;
    }
    
    new (fragPtr.p)FragmentInfo(fragId, senderRef);
    c_fragmentInfoHash.add(fragPtr);
    
    for(Uint32 i = 0; i<secs; i++){
      Uint32 sectionNo = secNos[i];
      ndbassert(sectionNo < 3);
      fragPtr.p->m_sectionPtrI[sectionNo] = sectionPtr[i];
    }
    
    ndbassert(! fragPtr.p->isDropped() );
    
    /**
     * Don't release allocated segments
     */
    signal->header.m_fragmentInfo = 0;
    signal->header.m_noOfSections = 0;
    return false;
  }
  
  FragmentInfo key(fragId, senderRef);
  Ptr<FragmentInfo> fragPtr;
  if(c_fragmentInfoHash.find(fragPtr, key)){
    
    /**
     * FragInfo == 2 or 3
     */
    if ( likely(! fragPtr.p->isDropped()) )
    {
      Uint32 i;
      for(i = 0; i<secs; i++){
        Uint32 sectionNo = secNos[i];
        ndbassert(sectionNo < 3);
        Uint32 sectionPtrI = sectionPtr[i];
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
        signal->header.m_fragmentInfo = 0;
        signal->header.m_noOfSections = 0;
        return false;
      }
      
      /**
       * fragInfo = 3
       */
      for(i = 0; i<3; i++){
        Uint32 ptrI = fragPtr.p->m_sectionPtrI[i];
        if(ptrI != RNIL){
          signal->m_sectionPtrI[i] = ptrI;
        } else {
          break;
        }
      }

      signal->setLength(sigLen - secs);
      signal->header.m_noOfSections = i;
      signal->header.m_fragmentInfo = 0;
      
      c_fragmentInfoHash.release(fragPtr);
      return true;
    }
    else
    {
      /* This fragmented signal has already had at least 1 fragment
       * dropped.  We must release the received segments.
       */
      for (Uint32 i=0; i < secs; i++)
        releaseSection( sectionPtr[i] );
      
      signal->header.m_fragmentInfo = 0;
      signal->header.m_noOfSections = 0;
      
      /* FragInfo == 2 
       * More fragments to come, keep waiting
       */
      if (fragInfo == 2)
        return false;
      
      /* FragInfo == 3
       * That was the last fragment.
       * We're now ready for handling the dropped signal.
       */      
      SignalDroppedRep * rep = (SignalDroppedRep*)signal->theData;
      Uint32 gsn = signal->header.theVerId_signalNumber;
      Uint32 len = signal->header.theLength;
      Uint32 newLen= (len > 22 ? 22 : len);
      memmove(rep->originalData, signal->theData, (4 * newLen));
      rep->originalGsn = gsn;
      rep->originalLength = len;
      rep->originalSectionCount = 0;
      signal->header.theVerId_signalNumber = GSN_SIGNAL_DROPPED_REP;
      signal->header.theLength = newLen + 3;
      signal->header.m_noOfSections = 0;
      signal->header.m_fragmentInfo = 3;


      /**
       * NOTE: Don't use EXECUTE_DIRECT as it 
       *       sets sendersBlockRef to reference()
       */
      /* Perform dropped signal handling, in this thread, now */
      jamBuffer()->markEndOfSigExec();
      executeFunction(GSN_SIGNAL_DROPPED_REP, signal);
      
      /* return false to caller - they should not process the signal */
      return false;
    } // else (isDropped())
  }
  
  /**
   * Unable to find fragment
   */
  ndbabort();
  return false;
}

bool
SimulatedBlock::assembleDroppedFragments(Signal* signal)
{
  /* This method is called at the start of a  SIGNAL_DROPPED_REP 
   * handler when there is a chance that the dropped signal could
   * be part of a fragmented signal.
   * If the dropped signal was a fragmented signal, this
   * needs to be handled specially to ensure that fragments
   * of the signal are correctly dropped to avoid segment
   * leaks etc.
   * There are a number of cases : 
   *   1) First fragment dropped  (FragInfo=1)
   *      All remaining fragments must be dropped when they 
   *      arrive.  The Signal dropped report handler must be 
   *      executed when the last fragment has arrived.
   *   2) Middle fragment dropped  (FragInfo=2)
   *      Any existing stored segments must be released.  
   *      All remaining fragments must be dropped when they
   *      arrive.  
   *   3) Last fragment dropped  (FragInfo=3)
   *      Any existing stored segments must be released.  
   *      Signal Dropped handling can occur, so return true.
   *
   * To indicate that a fragment has been dropped for a signal,
   * all the section I Values in the fragment's hash entry are 
   * set to RNIL.
   * Signal Dropped Report handling is performed when the last
   * fragment arrives.  If the last fragment is not dropped
   * by the transporter layer then normal fragment assembly 
   * arranges for dropped signal handling to occur.
   */
  Uint32 sigLen = signal->length() - 1;
  Uint32 fragId = signal->theData[sigLen];
  Uint32 fragInfo = signal->header.m_fragmentInfo;
  Uint32 senderRef = signal->getSendersBlockRef();

  if(fragInfo == 0){
    return true;
  }
  
  /* This method is for handling SIGNAL_DROPPED_REP only */
  ndbrequire(signal->header.theVerId_signalNumber == GSN_SIGNAL_DROPPED_REP);
  ndbrequire(signal->header.m_noOfSections == 0);

  if(fragInfo == 1){
    /**
     * First in train
     */
    Ptr<FragmentInfo> fragPtr;
    if(!c_fragmentInfoHash.seize(fragPtr)){
      ndbabort();
      return false;
    }
    
    new (fragPtr.p)FragmentInfo(fragId, senderRef);
    c_fragmentInfoHash.add(fragPtr);
    
    /* Mark entry in hash as belonging to dropped signal so subsequent
     * fragments can also be dropped
     */
    fragPtr.p->m_sectionPtrI[0]= RNIL;
    fragPtr.p->m_sectionPtrI[1]= RNIL;
    fragPtr.p->m_sectionPtrI[2]= RNIL;

    /* Wait for last fragment before SignalDroppedRep handling */
    signal->header.m_fragmentInfo = 0;
    return false;
  }
  
  FragmentInfo key(fragId, senderRef);
  Ptr<FragmentInfo> fragPtr;
  if(c_fragmentInfoHash.find(fragPtr, key)){
    
    /**
     * FragInfo == 2 or 3
     */
    if (! fragPtr.p->isDropped() )
    {
      /* Fragmented Signal not already marked as dropped
       * Need to free stored segments
       */
      releaseSection(fragPtr.p->m_sectionPtrI[0]);
      releaseSection(fragPtr.p->m_sectionPtrI[1]);
      releaseSection(fragPtr.p->m_sectionPtrI[2]);
      
      /* Mark as dropped now */
      fragPtr.p->m_sectionPtrI[0]= RNIL;
      fragPtr.p->m_sectionPtrI[1]= RNIL;
      fragPtr.p->m_sectionPtrI[2]= RNIL;
      
      ndbassert( fragPtr.p->isDropped() );
    }

    /**
     * fragInfo = 2
     *   Still waiting for final fragments.
     *   Return false to caller.
     */
    if(fragInfo == 2){
      signal->header.m_fragmentInfo = 0;
      return false;
    }
    
    /**
     * fragInfo = 3
     *   All fragments received, remove entry
     *   from hash and return to caller for
     *   dropped signal handling.
     */
    signal->header.m_fragmentInfo = 0;

    c_fragmentInfoHash.release(fragPtr);
    return true;
  }
  
  /**
   * Unable to find fragment
   */
  ndbabort();
  return false;
}

/**
 * doCleanupFragInfo
 * Iterate over block's Fragment assembly hash, looking
 * for in-assembly fragments from the failed node
 * Release these
 * Returns after each scanned bucket to avoid consuming
 * too much time.
 *
 * Parameters
 *   failedNodeId    : Node id of failed node
 *   cursor          : Hash bucket to start iteration from
 *   rtUnitsUsed     : Total rt units used
 *   elementsCleaned : Number of elements cleaned
 *
 * Updates
 *   cursor          : Hash bucket to continue iteration from
 *   rtUnitsUsed     : += units used
 *   elementsCleaned : += elements cleaned
 * 
 * Returns
 *   true  if all FragInfo structs cleaned up
 *   false if more to do 
 */
bool
SimulatedBlock::doCleanupFragInfo(Uint32 failedNodeId,
                                  Uint32& cursor,
                                  Uint32& rtUnitsUsed,
                                  Uint32& elementsCleaned)
{
  jam();
  FragmentInfo_hash::Iterator iter;
  
  c_fragmentInfoHash.next(cursor, iter);

  const Uint32 startBucket = iter.bucket;

  while (!iter.isNull() &&
         (iter.bucket == startBucket))
  {
    jam();

    Ptr<FragmentInfo> curr = iter.curr;
    c_fragmentInfoHash.next(iter);

    FragmentInfo* fragInfo = curr.p;
    
    if (refToNode(fragInfo->m_senderRef) == failedNodeId)
    {
      jam();
      /* We were assembling a fragmented signal from the
       * failed node, discard the partially assembled
       * sections and free the FragmentInfo hash entry
       */
      for(Uint32 s = 0; s<3; s++)
      {
        if (fragInfo->m_sectionPtrI[s] != RNIL)
        {
          jam();
          SegmentedSectionPtr ssptr;
          getSection(ssptr, fragInfo->m_sectionPtrI[s]);
          release(ssptr);
        }
      }
      
      /* Release FragmentInfo hash element */
      c_fragmentInfoHash.release(curr);

      elementsCleaned++;
      rtUnitsUsed+=3;
    }
      
    rtUnitsUsed++;
  } // while
   
  cursor = iter.bucket;
  return iter.isNull();
}

bool
SimulatedBlock::doCleanupFragSend(Uint32 failedNodeId,
                                  Uint32& cursor,
                                  Uint32& rtUnitsUsed,
                                  Uint32& elementsCleaned)
{
  jam();
  
  Ptr<FragmentSendInfo> fragPtr;
  const Uint32 NumSendLists = 2;
  ndbrequire(cursor < NumSendLists);

  FragmentSendInfo_list* fragSendLists[ NumSendLists ] =
    { &c_segmentedFragmentSendList,
      &c_linearFragmentSendList };
  
  FragmentSendInfo_list* list = fragSendLists[ cursor ];
  
  list->first(fragPtr);  
  for(; !fragPtr.isNull();){
    jam();
    Ptr<FragmentSendInfo> copyPtr = fragPtr;
    list->next(fragPtr);
    rtUnitsUsed++;

    NodeReceiverGroup& rg = copyPtr.p->m_nodeReceiverGroup;
    
    if (rg.m_nodes.get(failedNodeId))
    {
      jam();
      /* Fragmented signal is being sent to node */
      rg.m_nodes.clear(failedNodeId);
      
      if (rg.m_nodes.isclear())
      {
        jam();
        /* No other nodes in receiver group - send
         * is cancelled
         * Will be cleaned up in the usual CONTINUE_FRAGMENTED
         * handling code.
         */
        copyPtr.p->m_status = FragmentSendInfo::SendCancelled;
      }
      elementsCleaned++;
    }
  }

  /* Next time we'll do the next list */
  cursor++;
  
  return (cursor == NumSendLists);
}


Uint32
SimulatedBlock::doNodeFailureCleanup(Signal* signal,
                                     Uint32 failedNodeId,
                                     Uint32 resource,
                                     Uint32 cursor,
                                     Uint32 elementsCleaned,
                                     Callback& cb)
{
  jam();
  const bool userCallback = (cb.m_callbackFunction != 0);
  const Uint32 maxRtUnits = userCallback ?
#ifdef VM_TRACE
    2 :
#else
    16 :
#endif 
    ~0; /* Must complete all processing in this call */
  
  Uint32 rtUnitsUsed = 0;

  /* Loop over resources, cleaning them up */
  do
  {
    bool resourceDone= false;
    switch(resource) {
    case ContinueFragmented::RES_FRAGSEND:
    {
      jam();
      resourceDone = doCleanupFragSend(failedNodeId, cursor,
                                       rtUnitsUsed, elementsCleaned);
      break;
    }
    case ContinueFragmented::RES_FRAGINFO:
    {
      jam();
      resourceDone = doCleanupFragInfo(failedNodeId, cursor, 
                                       rtUnitsUsed, elementsCleaned);
      break;
    }
    case ContinueFragmented::RES_LAST:
    {
      jam();
      /* Node failure processing complete, execute user callback if provided */
      if (userCallback)
        execute(signal, cb, elementsCleaned);
      
      return elementsCleaned;
    }
    default:
      ndbabort();
    }

    /* Did we complete cleaning up this resource? */
    if (resourceDone)
    {
      resource++;
      cursor= 0;
    }

  } while (rtUnitsUsed <= maxRtUnits);
  
  jam();

  /* Not yet completed failure handling.
   * Must have exhausted RT units.  
   * Update cursor and re-invoke
   */
  ndbassert(userCallback);
  
  /* Send signal to continue processing */
  
  ContinueFragmented * sig = (ContinueFragmented*)signal->getDataPtrSend();
  sig->type = ContinueFragmented::CONTINUE_CLEANUP;
  sig->cleanup.failedNodeId = failedNodeId;
  sig->cleanup.resource = resource;
  sig->cleanup.cursor = cursor;
  sig->cleanup.elementsCleaned= elementsCleaned;
  constexpr Uint32 callbackWords = (sizeof(Callback) + 3) >> 2;
  constexpr Uint32 sigLen = ContinueFragmented::CONTINUE_CLEANUP_FIXED_WORDS +
    callbackWords;
  static_assert(sigLen <= 25);
  memcpy(&sig->cleanup.callbackStart, &cb, callbackWords << 2);
  
  sendSignal(reference(), GSN_CONTINUE_FRAGMENTED, signal, sigLen, JBB);

  return elementsCleaned;
}
  
Uint32
SimulatedBlock::simBlockNodeFailure(Signal* signal,
                                    Uint32 failedNodeId, 
                                    Callback& cb)
{
  jam();
  return doNodeFailureCleanup(signal, failedNodeId, 0, 0, 0, cb);
}

Uint32
SimulatedBlock::debugPrintFragmentCounts()
{
  const char* blockName = getBlockName(theNumber);
  FragmentInfo_hash::Iterator iter;
  Uint32 fragmentInfoCount = 0;
  c_fragmentInfoHash.first(iter);
  
  while(!iter.isNull())
  {
    fragmentInfoCount++;
    c_fragmentInfoHash.next(iter);
  }
  
  Ptr<FragmentSendInfo> ptr;
  Uint32 linSendInfoCount = 0;

  c_linearFragmentSendList.first(ptr);
  
  while (!ptr.isNull())
  {
    linSendInfoCount++;
    c_linearFragmentSendList.next(ptr);
  }
  
  Uint32 segSendInfoCount = 0;
  c_segmentedFragmentSendList.first(ptr);
  
  while (!ptr.isNull())
  {
    segSendInfoCount++;
    c_segmentedFragmentSendList.next(ptr);
  }

  g_eventLogger->info("%s : Fragment assembly hash entry count : %d", blockName,
                      fragmentInfoCount);

  g_eventLogger->info("%s : Linear fragment send list size : %d", blockName,
                      linSendInfoCount);

  g_eventLogger->info("%s : Segmented fragment send list size : %d", blockName,
                      segSendInfoCount);

  return fragmentInfoCount + 
    linSendInfoCount +
    segSendInfoCount;
}


bool
SimulatedBlock::sendFirstFragment(FragmentSendInfo & info,
				  NodeReceiverGroup rg, 
				  GlobalSignalNumber gsn, 
				  Signal* signal, 
				  Uint32 length, 
				  JobBufferLevel jbuf,
				  SectionHandle* sections,
                                  bool noRelease,
                                  Uint32 messageSize) {
  
  Uint32 noSections = sections->m_cnt;
  SegmentedSectionPtr * ptr = sections->m_ptr;

  info.m_sectionPtr[0].m_segmented.i = RNIL;
  info.m_sectionPtr[1].m_segmented.i = RNIL;
  info.m_sectionPtr[2].m_segmented.i = RNIL;
  
  Uint32 totalSize = 0;
  switch(noSections){
  case 3:
    info.m_sectionPtr[2].m_segmented.i = ptr[2].i;
    info.m_sectionPtr[2].m_segmented.p = ptr[2].p;
    totalSize += ptr[2].sz;
    [[fallthrough]];
  case 2:
    info.m_sectionPtr[1].m_segmented.i = ptr[1].i;
    info.m_sectionPtr[1].m_segmented.p = ptr[1].p;
    totalSize += ptr[1].sz;
    [[fallthrough]];
  case 1:
    info.m_sectionPtr[0].m_segmented.i = ptr[0].i;
    info.m_sectionPtr[0].m_segmented.p = ptr[0].p;
    totalSize += ptr[0].sz;
  }

  if (totalSize + length <= MAX_SIZE_SINGLE_SIGNAL)
  {
    /**
     * Send signal directly
     */
    if (noRelease)
      sendSignalNoRelease(rg, gsn, signal, length, jbuf, sections);
    else
      sendSignal(rg, gsn, signal, length, jbuf, sections);
      
    info.m_status = FragmentSendInfo::SendComplete;
    return true;
  }
  ndbrequire(blockToMain(rg.m_block) != V_QUERY);

  /**
   * Setup info object
   */
  info.m_status = FragmentSendInfo::SendNotComplete;
  info.m_prio = (Uint8)jbuf;
  info.m_gsn = gsn;
  info.m_fragInfo = 1;
  info.m_flags = 0;
  info.m_messageSize = messageSize;
  info.m_fragmentId = c_fragmentIdCounter++;
  info.m_nodeReceiverGroup = rg;
  info.m_callback.m_callbackFunction = 0;

  if (noRelease)
  {
    /* Record info that we are not releasing segments */
    info.m_flags|= FragmentSendInfo::SendNoReleaseSeg;
  }
  else
  {
    /**
     * Clear sections in caller's handle.  Actual send
     * will consume them
     */
    sections->m_cnt = 0;
  } 
  
  /* Store main signal data in a segment for sending later */
  Ptr<SectionSegment> tmp;
  if(!import(tmp, &signal->theData[0], length))
  {
    handle_out_of_longsignal_memory(0);
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
  
  if (unlikely(info.m_status == FragmentSendInfo::SendCancelled))
  {
    /* Send was cancelled - all dest. nodes have failed
     * since send was started
     */
    if (0 == (info.m_flags & FragmentSendInfo::SendNoReleaseSeg))
    {
      /*
       * Free any sections still to be sent
       */
      SectionHandle handle(this);
      for (Uint32 s = 0; s < 3; s++)
      {
        Uint32 sectionI = info.m_sectionPtr[s].m_segmented.i;
        if (sectionI != RNIL)
        {
          getSection(handle.m_ptr[handle.m_cnt], sectionI);
          info.m_sectionPtr[s].m_segmented.i = RNIL;
          info.m_sectionPtr[s].m_segmented.p = NULL;
          handle.m_cnt++;
        }
      }
      
      releaseSections(handle);
    }
    
    /* Free inline signal data storage section */
    Uint32 inlineDataI = info.m_theDataSection.p[info.m_theDataSection.sz];
    g_sectionSegmentPool.release(SB_SP_REL_ARG inlineDataI);
    
    info.m_status = FragmentSendInfo::SendComplete;
    return;
  }

  /**
   * Setup main signal data from stored copy
   */
  const Uint32 sigLen = info.m_theDataSection.sz;
  memcpy(&signal->theData[0], info.m_theDataSection.p, 4 * sigLen);
  
  Uint32 sz = 0; 
  Uint32 maxSz = info.m_messageSize;
  
  Int32 secNo = 2;
  Uint32 secCount = 0;
  Uint32 * secNos = &signal->theData[sigLen];
  
  SectionHandle sections(this);
  SegmentedSectionPtr *ptr = sections.m_ptr;

  bool split= false;
  Uint32 splitSectionStartI= RNIL;
  SectionSegment* splitSectionStartP= NULL;
  Uint32 splitSectionLastSegment= RNIL;
  Uint32 splitSectionSz= 0;

  enum { Unknown = 0, Full = 1 } loop = Unknown;
  for(; secNo >= 0 && secCount < 3; secNo--){
    Uint32 ptrI = info.m_sectionPtr[secNo].m_segmented.i;
    if(ptrI == RNIL)
      continue;
    
    info.m_sectionPtr[secNo].m_segmented.i = RNIL;
    
    SectionSegment * ptrP = info.m_sectionPtr[secNo].m_segmented.p;
    const Uint32 size = ptrP->m_sz;
    
    ptr[secCount].i = ptrI;
    ptr[secCount].p = ptrP;
    ptr[secCount].sz = size;
    secNos[secCount] = secNo;
    secCount++;
    
    const Uint32 sizeLeft = maxSz - sz;
    if(size <= sizeLeft){
      /**
       * The section fits
       */
      sz += size;
      lsout(g_eventLogger->info("section %d saved as %d", secNo, secCount - 1));
      continue;
    }
    
    const Uint32 overflow = size - sizeLeft; // > 0
    if(overflow <= SectionSegment::DataLength){
      /**
       * Only one segment left to send
       *   send even if sizeLeft <= size
       */
      lsout(g_eventLogger->info("section %d saved as %d but full over: %d",
                                secNo, secCount - 1, overflow));
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
      lsout(g_eventLogger->info("section %d not saved", secNo));
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
    while (sum <= fill)
    {
      prevPtrI = ptrI;
      ptrP = g_sectionSegmentPool.getPtr(ptrI);
      ptrI = ptrP->m_nextSegment;
      sum += SectionSegment::DataLength;
    }
    
    Uint32 prev = secCount - 1;
    /**
     * Record details of the section pre-split
     * This allows the split to be 'healed' afterwards in the
     * no release case.
     */
    split= true;
    splitSectionStartI= ptr[prev].i;
    splitSectionStartP= ptr[prev].p;
    splitSectionLastSegment= splitSectionStartP->m_lastSegment;
    splitSectionSz= splitSectionStartP->m_sz;

    /**
     * Rewrite header w.r.t size and last
     * This is what will be sent in this fragment.
     */
    splitSectionStartP->m_lastSegment = prevPtrI;
    splitSectionStartP->m_sz = sum;
    ptr[prev].sz = sum;
      
    /**
     * Write "new" list header
     * This is what remains to be sent in this section
     */
    ptrP = g_sectionSegmentPool.getPtr(ptrI);
    ptrP->m_lastSegment = splitSectionLastSegment;
    ptrP->m_sz = size - sum;
    
    /**
     * And store it on info-record
     */
    info.m_sectionPtr[secNo].m_segmented.i = ptrI;
    info.m_sectionPtr[secNo].m_segmented.p = ptrP;
    
    loop = Full;
    lsout(g_eventLogger->info("section %d split into %d", secNo, prev));
    break;
  }

  lsout(g_eventLogger->info("loop: %d secNo: %d secCount: %d sz: %d", loop,
                            secNo, secCount, sz));

  /**
   * Store fragment id
   */
  secNos[secCount] = info.m_fragmentId;
  
  Uint32 fragInfo = info.m_fragInfo;
  info.m_fragInfo = 2;
  switch(loop){
  case Unknown:
    if(secNo >= 0){
      lsout(g_eventLogger->info("Unknown - Full"));
      /**
       * Not finished
       */
      break;
    }
    lsout(g_eventLogger->info("Unknown - Done"));
    info.m_status = FragmentSendInfo::SendComplete;
    ndbassert(fragInfo == 2);
    fragInfo = 3;
    [[fallthrough]];
  case Full:
    break;
  }
  
  signal->header.m_fragmentInfo = fragInfo;
  signal->header.m_noOfSections = 0;
  sections.m_cnt = secCount;

  if (info.m_flags & FragmentSendInfo::SendNoReleaseSeg)
  {
    sendSignalNoRelease(info.m_nodeReceiverGroup,
                        info.m_gsn,
                        signal, 
                        sigLen + secCount + 1,
                        (JobBufferLevel)info.m_prio,
                        &sections);
    /* NoRelease leaves SectionHandle populated, we'll
     * clear it here.  The actual sections themselves 
     * remain allocated.
     */
    sections.m_cnt = 0;

    if (split)
    {
      /* There was a split section, which required us to modify the
       * segment list.
       * Now restore the split section's segment list back to
       * its previous state
       * (Only really required for first segment, but we do
       *  it for all of them, to be a good citizen)
       */
      ndbrequire( splitSectionStartI != RNIL );
      ndbrequire( splitSectionStartP != NULL );
      ndbrequire( splitSectionLastSegment != RNIL );

      splitSectionStartP->m_lastSegment= splitSectionLastSegment;
      splitSectionStartP->m_sz= splitSectionSz;

      /* Check our handiwork */
      assert(verifySection(splitSectionStartI));
    }
  }
  else
  {
    /* Normal, release sections case */
    sendSignal(info.m_nodeReceiverGroup,
               info.m_gsn,
               signal, 
               sigLen + secCount + 1,
               (JobBufferLevel)info.m_prio,
               &sections);
  }
  
  if(fragInfo == 3){
    /**
     * This is the last signal
     * Release saved 'main signal' words segment
     */
    g_sectionSegmentPool.release(SB_SP_REL_ARG info.m_theDataSection.p[sigLen]);
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
  
  check_sections(reinterpret_cast<Signal25*>(signal),
                 signal->header.m_noOfSections,
                 noOfSections);
  
  info.m_sectionPtr[0].m_linear.p = NULL;
  info.m_sectionPtr[1].m_linear.p = NULL;
  info.m_sectionPtr[2].m_linear.p = NULL;
  
  Uint32 totalSize = 0;
  switch(noOfSections){
  case 3:
    info.m_sectionPtr[2].m_linear = ptr[2];
    totalSize += ptr[2].sz;
    [[fallthrough]];
  case 2:
    info.m_sectionPtr[1].m_linear = ptr[1];
    totalSize += ptr[1].sz;
    [[fallthrough]];
  case 1:
    info.m_sectionPtr[0].m_linear = ptr[0];
    totalSize += ptr[0].sz;
  }

  if (totalSize + length <= MAX_SIZE_SINGLE_SIGNAL)
  {
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
  ndbrequire(blockToMain(rg.m_block) != V_QUERY);

  /**
   * Setup info object
   */
  info.m_status = FragmentSendInfo::SendNotComplete;
  info.m_prio = (Uint8)jbuf;
  info.m_gsn = gsn;
  info.m_messageSize = messageSize;
  info.m_fragInfo = 1;
  info.m_flags = 0;
  info.m_fragmentId = c_fragmentIdCounter++;
  info.m_nodeReceiverGroup = rg;
  info.m_callback.m_callbackFunction = 0;

  Ptr<SectionSegment> tmp;
  if(unlikely(!import(tmp, &signal->theData[0], length)))
  {
    handle_out_of_longsignal_memory(0);
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
  
  if (unlikely(info.m_status == FragmentSendInfo::SendCancelled))
  {
    /* Send was cancelled - all dest. nodes have failed
     * since send was started
     */
    /* Free inline signal data storage section */
    Uint32 inlineDataI = info.m_theDataSection.p[info.m_theDataSection.sz];
    g_sectionSegmentPool.release(SB_SP_REL_ARG inlineDataI);
    
    info.m_status = FragmentSendInfo::SendComplete;
    return;
  }

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
    const Uint32* ptrP = info.m_sectionPtr[secNo].m_linear.p;
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
      lsout(g_eventLogger->info("section %d saved as %d", secNo, secCount - 1));
      continue;
    }
    
    const Uint32 overflow = size - sizeLeft; // > 0
    if(overflow <= SectionSegment::DataLength){
      /**
       * Only one segment left to send
       *   send even if sizeLeft <= size
       */
      lsout(g_eventLogger->info("section %d saved as %d but full over: %d",
                                secNo, secCount - 1, overflow));
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
      lsout(g_eventLogger->info("section %d not saved", secNo));
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
    lsout(g_eventLogger->info("section %d split into %d", secNo, prev));
    break;
  }

  lsout(g_eventLogger->info("loop: %d secNo: %d secCount: %d sz: %d", loop,
                            secNo, secCount, sz));

  /**
   * Store fragment id
   */
  secNos[secCount] = info.m_fragmentId;
  
  Uint32 fragInfo = info.m_fragInfo;
  info.m_fragInfo = 2;
  switch(loop){
  case Unknown:
    if(secNo >= 0){
      lsout(g_eventLogger->info("Unknown - Full"));
      /**
       * Not finished
       */
      break;
    }
    lsout(g_eventLogger->info("Unknown - Done"));
    info.m_status = FragmentSendInfo::SendComplete;
    ndbassert(fragInfo == 2);
    fragInfo = 3;
    [[fallthrough]];
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
    g_sectionSegmentPool.release(SB_SP_REL_ARG info.m_theDataSection.p[sigLen]);
  }
}

void
SimulatedBlock::sendFragmentedSignal(BlockReference ref, 
				     GlobalSignalNumber gsn, 
				     Signal* signal, 
				     Uint32 length, 
				     JobBufferLevel jbuf,
				     SectionHandle* sections,
				     Callback & c,
				     Uint32 messageSize){
  bool res = true;
  Ptr<FragmentSendInfo> tmp;
  res = c_segmentedFragmentSendList.seizeFirst(tmp);
  ndbrequire(res);
  
  res = sendFirstFragment(* tmp.p,
			  NodeReceiverGroup(ref),
			  gsn,
			  signal,
			  length,
			  jbuf,
			  sections,
                          false, // Release sections on send
			  messageSize);
  ndbrequire(res);
  
  if(tmp.p->m_status == FragmentSendInfo::SendComplete){
    c_segmentedFragmentSendList.release(tmp);
    if(c.m_callbackFunction != 0)
      execute(signal, c, 0);
    return;
  }
  tmp.p->m_callback = c;

  if(!c_fragSenderRunning)
  {
    SaveSignal<2> save(signal);
    c_fragSenderRunning = true;
    ContinueFragmented * sig = (ContinueFragmented*)signal->getDataPtrSend();
    sig->type = ContinueFragmented::CONTINUE_SENDING;
    sig->line = __LINE__;
    sendSignal(reference(), GSN_CONTINUE_FRAGMENTED, signal, 2, JBB);
  }
}

void
SimulatedBlock::sendFragmentedSignal(NodeReceiverGroup rg, 
				     GlobalSignalNumber gsn, 
				     Signal* signal, 
				     Uint32 length, 
				     JobBufferLevel jbuf,
				     SectionHandle * sections,
				     Callback & c,
				     Uint32 messageSize){
  bool res = true;
  Ptr<FragmentSendInfo> tmp;
  res = c_segmentedFragmentSendList.seizeFirst(tmp);
  ndbrequire(res);
  
  res = sendFirstFragment(* tmp.p,
			  rg,
			  gsn,
			  signal,
			  length,
			  jbuf,
			  sections,
			  false, // Release sections on send
                          messageSize);
  ndbrequire(res);
  
  if(tmp.p->m_status == FragmentSendInfo::SendComplete){
    c_segmentedFragmentSendList.release(tmp);
    if(c.m_callbackFunction != 0)
      execute(signal, c, 0);
    return;
  }
  tmp.p->m_callback = c;

  if(!c_fragSenderRunning)
  {
    SaveSignal<2> save(signal);
    c_fragSenderRunning = true;
    ContinueFragmented * sig = (ContinueFragmented*)signal->getDataPtrSend();
    sig->type = ContinueFragmented::CONTINUE_SENDING;
    sig->line = __LINE__;
    sendSignal(reference(), GSN_CONTINUE_FRAGMENTED, signal, 2, JBB);
  }
}

SimulatedBlock::Callback SimulatedBlock::TheEmptyCallback = {0, 0};
void
SimulatedBlock::TheNULLCallbackFunction(class Signal*, Uint32, Uint32)
{ abort(); /* should never be called */ }
SimulatedBlock::Callback SimulatedBlock::TheNULLCallback =
{ &SimulatedBlock::TheNULLCallbackFunction, 0 };

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
  res = c_linearFragmentSendList.seizeFirst(tmp);
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
  
  if(!c_fragSenderRunning)
  {
    SaveSignal<2> save(signal);
    c_fragSenderRunning = true;
    ContinueFragmented * sig = (ContinueFragmented*)signal->getDataPtrSend();
    sig->type = ContinueFragmented::CONTINUE_SENDING;
    sig->line = __LINE__;
    sendSignal(reference(), GSN_CONTINUE_FRAGMENTED, signal, 2, JBB);
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
  res = c_linearFragmentSendList.seizeFirst(tmp);
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
  
  if(!c_fragSenderRunning)
  {
    SaveSignal<2> save(signal);
    c_fragSenderRunning = true;
    ContinueFragmented * sig = (ContinueFragmented*)signal->getDataPtrSend();
    sig->type = ContinueFragmented::CONTINUE_SENDING;
    sig->line = __LINE__;
    sendSignal(reference(), GSN_CONTINUE_FRAGMENTED, signal, 2, JBB);
  }
}

void
SimulatedBlock::sendBatchedFragmentedSignal(BlockReference ref,
                                            GlobalSignalNumber gsn,
                                            Signal* signal,
                                            Uint32 length,
                                            JobBufferLevel jbuf,
                                            SectionHandle* sections,
                                            bool noRelease,
                                            Callback & c,
                                            Uint32 messageSize)
{
  jam();
  bool res = true;
  FragmentSendInfo fragSendInfo;

  const Uint32 noOfSections = sections->m_cnt;
  SegmentedSectionPtr * const ptr = sections->m_ptr;
  const Uint32 totalSize =
    (noOfSections >= 1 ? ptr[0].sz : 0) +
    (noOfSections >= 2 ? ptr[1].sz : 0) +
    (noOfSections >= 3 ? ptr[2].sz : 0);

  res = sendFirstFragment(fragSendInfo,
                          NodeReceiverGroup(ref),
                          gsn,
                          signal,
                          length,
                          jbuf,
                          sections,
                          noRelease,
                          messageSize);
  ndbrequire(res);

  Uint32 guard = totalSize / messageSize + 1 + noOfSections;

  while (guard > 0 && fragSendInfo.m_status != FragmentSendInfo::SendComplete)
  {
    jam();
    guard--;
    // Send remaining fragments
    sendNextSegmentedFragment(signal, fragSendInfo);
  }

  ndbrequire(fragSendInfo.m_status == FragmentSendInfo::SendComplete);

  if (c.m_callbackFunction != nullptr)
  {
    jam();
    execute(signal, c, 0);
  }
  return;
}

void
SimulatedBlock::sendBatchedFragmentedSignal(NodeReceiverGroup rg,
                                            GlobalSignalNumber gsn,
                                            Signal* signal,
                                            Uint32 length,
                                            JobBufferLevel jbuf,
                                            SectionHandle * sections,
                                            bool noRelease,
                                            Callback & c,
                                            Uint32 messageSize)
{
  jam();
  bool res = true;
  FragmentSendInfo fragSendInfo;

  const Uint32 noOfSections = sections->m_cnt;
  SegmentedSectionPtr * const ptr = sections->m_ptr;
  const Uint32 totalSize =
    (noOfSections >= 1 ? ptr[0].sz : 0) +
    (noOfSections >= 2 ? ptr[1].sz : 0) +
    (noOfSections >= 3 ? ptr[2].sz : 0);

  res = sendFirstFragment(fragSendInfo,
                          rg,
                          gsn,
                          signal,
                          length,
                          jbuf,
                          sections,
                          noRelease,
                          messageSize);
  ndbrequire(res);

  Uint32 guard = totalSize / messageSize + 1 + noOfSections;

  while (guard > 0 && fragSendInfo.m_status != FragmentSendInfo::SendComplete)
  {
    jam();
    guard--;
    // Send remaining fragments
    sendNextSegmentedFragment(signal, fragSendInfo);
  }

  ndbrequire(fragSendInfo.m_status == FragmentSendInfo::SendComplete);

  if (c.m_callbackFunction != nullptr)
  {
    jam();
    execute(signal, c, 0);
  }
  return;
}

void
SimulatedBlock::sendBatchedFragmentedSignal(BlockReference ref,
                                            GlobalSignalNumber gsn,
                                            Signal* signal,
                                            Uint32 length,
                                            JobBufferLevel jbuf,
                                            LinearSectionPtr ptr[3],
                                            Uint32 noOfSections,
                                            Callback & c,
                                            Uint32 messageSize)
{
  jam();
  bool res = true;
  FragmentSendInfo fragSendInfo;

  res = sendFirstFragment(fragSendInfo,
                          NodeReceiverGroup(ref),
                          gsn,
                          signal,
                          length,
                          jbuf,
                          ptr,
                          noOfSections,
                          messageSize);
  ndbrequire(res);

  const Uint32 totalSize =
    (noOfSections >= 1 ? ptr[0].sz : 0) +
    (noOfSections >= 2 ? ptr[1].sz : 0) +
    (noOfSections >= 3 ? ptr[2].sz : 0);

  Uint32 guard = totalSize / messageSize + 1 + noOfSections;

  while (guard > 0 && fragSendInfo.m_status != FragmentSendInfo::SendComplete)
  {
    jam();
    guard--;
    // Send remaining fragments
    sendNextLinearFragment(signal, fragSendInfo);
  }

  ndbrequire(fragSendInfo.m_status == FragmentSendInfo::SendComplete);

  if (c.m_callbackFunction != nullptr)
  {
    execute(signal, c, 0);
  }
  return;
}

void
SimulatedBlock::sendBatchedFragmentedSignal(NodeReceiverGroup rg,
                                            GlobalSignalNumber gsn,
                                            Signal* signal,
                                            Uint32 length,
                                            JobBufferLevel jbuf,
                                            LinearSectionPtr ptr[3],
                                            Uint32 noOfSections,
                                            Callback & c,
                                            Uint32 messageSize)
{
  jam();
  bool res = true;
  FragmentSendInfo fragSendInfo;

  res = sendFirstFragment(fragSendInfo,
                          rg,
                          gsn,
                          signal,
                          length,
                          jbuf,
                          ptr,
                          noOfSections,
                          messageSize);
  ndbrequire(res);

  const Uint32 totalSize =
    (noOfSections >= 1 ? ptr[0].sz : 0) +
    (noOfSections >= 2 ? ptr[1].sz : 0) +
    (noOfSections >= 3 ? ptr[2].sz : 0);

  Uint32 guard = totalSize / messageSize + 1 + noOfSections;

  while (guard > 0 && fragSendInfo.m_status != FragmentSendInfo::SendComplete)
  {
    jam();
    guard--;
    // Send remaining fragments
    sendNextLinearFragment(signal, fragSendInfo);
  }

  ndbrequire(fragSendInfo.m_status == FragmentSendInfo::SendComplete);

  if (c.m_callbackFunction != nullptr)
  {
    execute(signal, c, 0);
  }
  return;
}


NodeInfo &
SimulatedBlock::setNodeInfo(NodeId nodeId) {
  ndbrequire(nodeId > 0 && nodeId < MAX_NODES);
  return globalData.m_nodeInfo[nodeId];
}

bool
SimulatedBlock::isMultiThreaded()
{
#ifdef NDBD_MULTITHREADED
  return true;
#else
  return false;
#endif
}


void 
SimulatedBlock::execUTIL_CREATE_LOCK_REF(Signal* signal){
  jamEntry();
  c_mutexMgr.execUTIL_CREATE_LOCK_REF(signal);
}

void SimulatedBlock::execUTIL_CREATE_LOCK_CONF(Signal* signal){
  jamEntry();
  c_mutexMgr.execUTIL_CREATE_LOCK_CONF(signal);
}

void SimulatedBlock::execUTIL_DESTORY_LOCK_REF(Signal* signal){
  jamEntry();
  c_mutexMgr.execUTIL_DESTORY_LOCK_REF(signal);
}

void SimulatedBlock::execUTIL_DESTORY_LOCK_CONF(Signal* signal){
  jamEntry();
  c_mutexMgr.execUTIL_DESTORY_LOCK_CONF(signal);
}

void SimulatedBlock::execUTIL_LOCK_REF(Signal* signal){
  jamEntry();
  c_mutexMgr.execUTIL_LOCK_REF(signal);
}

void SimulatedBlock::execUTIL_LOCK_CONF(Signal* signal){
  jamEntry();
  c_mutexMgr.execUTIL_LOCK_CONF(signal);
}

void SimulatedBlock::execUTIL_UNLOCK_REF(Signal* signal){
  jamEntry();
  c_mutexMgr.execUTIL_UNLOCK_REF(signal);
}

void SimulatedBlock::execUTIL_UNLOCK_CONF(Signal* signal){
  jamEntry();
  c_mutexMgr.execUTIL_UNLOCK_CONF(signal);
}

void
SimulatedBlock::ignoreMutexUnlockCallback(Signal* signal, 
					  Uint32 ptrI, Uint32 retVal){
  c_mutexMgr.release(ptrI);
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

#if defined(USE_INIT_GLOBAL_VARIABLES)
void
SimulatedBlock::disable_global_variables()
{
#ifdef NDBD_MULTITHREADED
  mt_disable_global_variables(m_threadId);
#endif
}

void
SimulatedBlock::enable_global_variables()
{
#ifdef NDBD_MULTITHREADED
  mt_enable_global_variables(m_threadId);
#endif
}

void
SimulatedBlock::init_global_ptrs(void ** tmp, size_t cnt)
{
#ifdef NDBD_MULTITHREADED
  mt_init_global_variables_ptr_instances(m_threadId, tmp, cnt);
#endif
}

void
SimulatedBlock::init_global_uint32_ptrs(void ** tmp, size_t cnt)
{
#ifdef NDBD_MULTITHREADED
  mt_init_global_variables_uint32_ptr_instances(m_threadId, tmp, cnt);
#endif
}

void
SimulatedBlock::init_global_uint32(void ** tmp, size_t cnt)
{
#ifdef NDBD_MULTITHREADED
  mt_init_global_variables_uint32_instances(m_threadId, tmp, cnt);
#endif
}
#endif

int
SimulatedBlock::cmp_key(Uint32 tab, const Uint32 *s1, const Uint32 *s2) const
{
  const KeyDescriptor * desc = g_key_descriptor_pool.getPtr(tab);
  const Uint32 noOfKeyAttr = desc->noOfKeyAttr;

  for (Uint32 i = 0; i < noOfKeyAttr; i++)
  {
    const KeyDescriptor::KeyAttr& keyAttr = desc->keyAttr[i];
    const Uint32 attrDesc = keyAttr.attributeDescriptor;
    const Uint32 srcBytes = AttributeDescriptor::getSizeInBytes(attrDesc);

    const int res = cmp_attr(attrDesc, keyAttr.charsetInfo,
			     s1, srcBytes, s2, srcBytes);    
    if (res != 0)
      return res;

    if (i+1 < noOfKeyAttr) //Optimization; skip if last keyAttr
    {
      const Uint32 typeId = AttributeDescriptor::getType(attrDesc);
      Uint32 lb, len;
      ndbrequire(NdbSqlUtil::get_var_length(typeId, s1, srcBytes, lb, len));
      s1 += ((len+lb+3) >> 2);

      ndbrequire(NdbSqlUtil::get_var_length(typeId, s2, srcBytes, lb, len));
      s2 += ((len+lb+3) >> 2);
    }
  }
  // Fall through: Compared equal
  return 0;
}

int
SimulatedBlock::cmp_attr(Uint32 attrDesc, const CHARSET_INFO* cs,
			 const Uint32 *s1, Uint32 s1Len,
			 const Uint32 *s2, Uint32 s2Len) const
{
  const Uint32 typeId = AttributeDescriptor::getType(attrDesc);
  NdbSqlUtil::Cmp *cmp = NdbSqlUtil::getType(typeId).m_cmp;
  return (*cmp)(cs, s1, s1Len, s2, s2Len);
}


Uint32
SimulatedBlock::xfrm_key_hash(
                         Uint32 tab, const Uint32* src,
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
    Uint32 dstWords =
      xfrm_attr_hash(keyAttr.attributeDescriptor, keyAttr.charsetInfo,
                src, srcPos, dst, dstPos, dstSize);
    keyPartLen[i++] = dstWords;
    if (unlikely(dstWords == 0))
      return 0;
  }

  if (0)
  {
    for(Uint32 i = 0; i<dstPos; i++)
    {
      printf("%.8x ", dst[i]);
    }
    printf("\n");
  }
  return dstPos;
}

Uint32
SimulatedBlock::xfrm_attr_hash(
                          Uint32 attrDesc, const CHARSET_INFO* cs,
                          const Uint32* src, Uint32 & srcPos,
                          Uint32* dst, Uint32 & dstPos, Uint32 dstSize) const
{
  Uint32 array = 
    AttributeDescriptor::getArrayType(attrDesc);
  Uint32 srcBytes = 
    AttributeDescriptor::getSizeInBytes(attrDesc);

  Uint32 srcWords = ~0;
  Uint32 dstWords = ~0;
  uchar* dstPtr = (uchar*)&dst[dstPos];
  const uchar* srcPtr = (const uchar*)&src[srcPos];
  
  if (cs == NULL)
  {
    jam();
    Uint32 len = 0;
    switch(array){
    case NDB_ARRAYTYPE_SHORT_VAR:
      len = 1 + srcPtr[0];
      break;
    case NDB_ARRAYTYPE_MEDIUM_VAR:
      len = 2 + srcPtr[0] + (srcPtr[1] << 8);
      break;
#ifndef VM_TRACE
    default:
      abort();
#endif
    case NDB_ARRAYTYPE_FIXED:
      len = srcBytes;
    }
    srcWords = (len + 3) >> 2;
    dstWords = srcWords;
    memcpy(dstPtr, srcPtr, dstWords << 2);
    
    if (0)
    {
      g_eventLogger->info(
          "srcPos: %d dstPos: %d len: %d srcWords: %d dstWords: %d", srcPos,
          dstPos, len, srcWords, dstWords);

      for(Uint32 i = 0; i<srcWords; i++)
        printf("%.8x ", src[srcPos + i]);
      printf("\n");
    }
  } 
  else
  {
    jam();
    Uint32 typeId =
      AttributeDescriptor::getType(attrDesc);
    Uint32 lb, len;
    bool ok = NdbSqlUtil::get_var_length(typeId, srcPtr, srcBytes, lb, len);
    if (unlikely(!ok))
      return 0;

    // remLen: Remaining dst-buffer length
    // len:    Actual length of 'src'
    // defLen: Max defined length of src data 
    const unsigned remLen = ((dstSize - dstPos) << 2);
    const unsigned defLen = srcBytes - lb;
    int n = NdbSqlUtil::strnxfrm_hash(cs, typeId,
                                 dstPtr, remLen, 
                                 srcPtr + lb, len, defLen);
    
    if (unlikely(n == -1))
      return 0;
    while ((n & 3) != 0) 
    {
      dstPtr[n++] = 0;
    }
    dstWords = (n >> 2);
    srcWords = (lb + len + 3) >> 2; 
  }

  dstPos += dstWords;
  srcPos += srcWords;
  return dstWords;
}

Uint32
SimulatedBlock::create_distr_key(Uint32 tableId,
				 const Uint32 *src,
                                 Uint32* dst,
				 const Uint32 
				 keyPartLen[MAX_ATTRIBUTES_IN_INDEX]) const 
{
  const KeyDescriptor* desc = g_key_descriptor_pool.getPtr(tableId);
  const Uint32 noOfKeyAttr = desc->noOfKeyAttr;
  Uint32 noOfDistrKeys = desc->noOfDistrKeys;
  
  Uint32 i = 0;
  Uint32 dstPos = 0;
  
  /* --Note that src and dst may be the same location-- */

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
      ndbrequire(AttributeDescriptor::getArrayType(attr) == NDB_ARRAYTYPE_FIXED);
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

CArray<KeyDescriptor> g_key_descriptor_pool;

void
SimulatedBlock::sendRoutedSignal(RoutePath path[], Uint32 pathcnt,
                                 Uint32 dst[],
                                 Uint32 dstcnt,
                                 Uint32 gsn,
                                 Signal * signal,
                                 Uint32 sigLen,
                                 JobBufferLevel prio,
                                 SectionHandle * userhandle)
{
  ndbrequire(pathcnt > 0); // don't support (now) directly multi-cast
  pathcnt--; // first hop is made from here


  Uint32 len = LocalRouteOrd::StaticLen + (2 * pathcnt) + dstcnt;
  ndbrequire(len <= 25);

  SectionHandle handle(this, signal);
  if (userhandle)
  {
    jam();
    handle.m_cnt = userhandle->m_cnt;
    for (Uint32 i = 0; i<handle.m_cnt; i++)
      handle.m_ptr[i] = userhandle->m_ptr[i];
    userhandle->m_cnt = 0;
  }

  if (len + sigLen > 25)
  {
    jam();

    /**
     * we need to store theData in a section
     */
    ndbrequire(handle.m_cnt < 3);
    handle.m_ptr[2] = handle.m_ptr[1];
    handle.m_ptr[1] = handle.m_ptr[0];
    Ptr<SectionSegment> tmp;
    if (unlikely(! import(tmp, signal->theData, sigLen)))
    {
      handle_out_of_longsignal_memory(0);
    }
    handle.m_ptr[0].p = tmp.p;
    handle.m_ptr[0].i = tmp.i;
    handle.m_ptr[0].sz = sigLen;
    handle.m_cnt ++;
  }
  else
  {
    jam();
    memmove(signal->theData + len, signal->theData, 4 * sigLen);
    len += sigLen;
  }

  LocalRouteOrd * ord = (LocalRouteOrd*)signal->getDataPtrSend();
  ord->cnt = (pathcnt << 16) | (dstcnt);
  ord->gsn = gsn;
  ord->prio = Uint32(prio);

  Uint32 * dstptr = ord->path;
  for (Uint32 i = 1; i <= pathcnt; i++)
  {
    ndbrequire(refToNode(path[i].ref) == 0 ||
               refToNode(path[i].ref) == getOwnNodeId());

    * dstptr++ = path[i].ref;
    * dstptr++ = Uint32(path[i].prio);
  }

  for (Uint32 i = 0; i<dstcnt; i++)
  {
    ndbrequire(refToNode(dst[i]) == 0 ||
               refToNode(dst[i]) == getOwnNodeId());

    * dstptr++ = dst[i];
  }

  sendSignal(path[0].ref, GSN_LOCAL_ROUTE_ORD, signal, len,
             path[0].prio, &handle);
}

void
SimulatedBlock::execLOCAL_ROUTE_ORD(Signal* signal)
{
  jamEntry();

  if (!assembleFragments(signal))
  {
    jam();
    return;
  }

  if (ERROR_INSERTED(1001))
  {
    /**
     * This NDBCNTR error code 1001
     */
    jam();
    SectionHandle handle(this, signal);
    sendSignalWithDelay(reference(), GSN_LOCAL_ROUTE_ORD, signal, 200, 
                        signal->getLength(), &handle);
    return;
  }

  LocalRouteOrd* ord = (LocalRouteOrd*)signal->getDataPtr();
  const Uint32 pathcnt = ord->cnt >> 16;
  const Uint32 dstcnt = ord->cnt & 0xFFFF;
  Uint32 sigLen = signal->getLength();

  ndbrequire(sigLen >= (LocalRouteOrd::StaticLen +
                        (pathcnt * 2) +
                        (dstcnt)));

  if (pathcnt == 0)
  {
    /**
     * Send to final destination(s);
     */
    jam();
    Uint32 gsn = ord->gsn;
    Uint32 prio = ord->prio;
    ndbrequire(dstcnt <= LocalRouteOrd::MaxDstCount);
    static_assert(25 + LocalRouteOrd::MaxDstCount <= NDB_ARRAY_SIZE(signal->theData));
    memcpy(signal->theData+25, ord->path, 4*dstcnt);
    SectionHandle handle(this, signal);
    if (sigLen > LocalRouteOrd::StaticLen + dstcnt)
    {
      jam();
      /**
       * Data is at end of this...
       */
      memmove(signal->theData,
              signal->theData + LocalRouteOrd::StaticLen + dstcnt,
              4 * (sigLen - (LocalRouteOrd::StaticLen + dstcnt)));
      sigLen = sigLen - (LocalRouteOrd::StaticLen + dstcnt);
    }
    else
    {
      jam();
      /**
       * Put section 0 in signal->theData
       */
      sigLen = handle.m_ptr[0].sz;
      ndbrequire(sigLen <= 25);
      copy(signal->theData, handle.m_ptr[0]);
      release(handle.m_ptr[0]);

      for (Uint32 i = 0; i < handle.m_cnt - 1; i++)
        handle.m_ptr[i] = handle.m_ptr[i+1];
      handle.m_cnt--;
    }

    /*
     * The extra if-statement is as sendSignalNoRelease will copy sections
     *   which is not necessary is only sending to one destination
     */
    if (dstcnt > 1)
    {
      jam();
      for (Uint32 i = 0; i<dstcnt; i++)
      {
        jam();
        sendSignalNoRelease(signal->theData[25+i], gsn, signal, sigLen,
                            JobBufferLevel(prio), &handle);
      }
      releaseSections(handle);
    }
    else
    {
      jam();
      sendSignal(signal->theData[25+0], gsn, signal, sigLen,
                 JobBufferLevel(prio), &handle);
    }
  }
  else
  {
    /**
     * Reroute
     */
    jam();
    SectionHandle handle(this, signal);
    Uint32 ref = ord->path[0];
    Uint32 prio = ord->path[1];
    Uint32 len = sigLen - 2;
    ord->cnt = ((pathcnt - 1) << 16) | dstcnt;
    memmove(ord->path, ord->path+2, 4 * (len - LocalRouteOrd::StaticLen));
    sendSignal(ref, GSN_LOCAL_ROUTE_ORD, signal, len,
               JobBufferLevel(prio), &handle);
  }
}


#ifdef VM_TRACE
bool
SimulatedBlock::debugOutOn()
{
  SignalLoggerManager::LogMode mask = SignalLoggerManager::LogInOut;
  return
    globalData.testOn &&
    globalSignalLoggers.logMatch(number(), mask);
}

const char*
SimulatedBlock::debugOutTag(char *buf, int line)
{
  char blockbuf[40];
  char instancebuf[40];
  char linebuf[40];
  char timebuf[40];
  sprintf(blockbuf, "%s", getBlockName(number(), "UNKNOWN"));
  instancebuf[0] = 0;
  if (instance() != 0)
    sprintf(instancebuf, "/%u", instance());
  sprintf(linebuf, " %d", line);
  timebuf[0] = 0;
#ifdef VM_TRACE_TIME
  {
    Uint64 t = NdbTick_CurrentMillisecond();
    uint s = (t / 1000) % 3600;
    uint ms = t % 1000;
    sprintf(timebuf, " - %u.%03u -", s, ms);
  }
#endif
  sprintf(buf, "%s%s%s%s ", blockbuf, instancebuf, linebuf, timebuf);
  return buf;
}
#endif

#ifdef NDBD_MULTITHREADED
// Leave synchronize_threads() undefined for ndbd where it should not be used.
void
SimulatedBlock::synchronize_threads(Signal * signal,
                                    const BlockThreadBitmask& threads,
                                    const Callback & cb,
                                    JobBufferLevel req_prio,
                                    JobBufferLevel conf_prio)
{
  jam();

  Ptr<SyncThreadRecord> ptr;
  ndbrequire(c_syncThreadPool.seize(ptr));
  ptr.p->m_threads = threads;
  ptr.p->m_cnt = 0;
  ptr.p->m_next = 0;
  ptr.p->m_callback = cb;

  const Uint32 cnt = threads.count();
  if (cnt == 0)
  {
    jam();
    Callback copy = cb;
    c_syncThreadPool.release(ptr);
    execute(signal, copy, 0);
    return;
  }

  signal->theData[0] = reference();
  signal->theData[1] = ptr.i;
  signal->theData[2] = Uint32(req_prio);
  signal->theData[3] = Uint32(conf_prio);
  ptr.p->m_next = ptr.p->m_threads.find_first();
  sendSYNC_THREAD_REQ(signal, ptr);
}
#endif

void
SimulatedBlock::synchronize_threads_for_blocks(Signal * signal,
                                               const Uint32 blocks[],
                                               const Callback & cb,
                                               JobBufferLevel req_prio,
                                               JobBufferLevel conf_prio)
{
#ifndef NDBD_MULTITHREADED
  Callback copy = cb;
  execute(signal, copy, 0);
#else
  jam();

  BlockThreadBitmask threads;

  mt_get_threads_for_blocks_no_proxy(blocks, threads);

  if (conf_prio == ILLEGAL_JB_LEVEL)
  {
    conf_prio = req_prio;
  }
  synchronize_threads(signal, threads, cb, req_prio, conf_prio);
#endif
}

void
SimulatedBlock::sendSYNC_THREAD_REQ(Signal* signal, Ptr<SyncThreadRecord> ptr)
{
  JobBufferLevel req_prio = JobBufferLevel(signal->theData[2]);
  Uint32 instance = ptr.p->m_next;
  constexpr Uint32 MAX_FAN_OUT = 4;
  constexpr Uint32 MAX_INFLIGHT = 50;
  for (Uint32 fan_out = 0;
       ptr.p->m_cnt < MAX_INFLIGHT &&
       fan_out < MAX_FAN_OUT &&
       instance != BlockThreadBitmask::NotFound;
       fan_out++, instance = ptr.p->m_threads.find_next(instance + 1))
  {
    Uint32 ref = numberToRef(THRMAN, instance, 0);
    sendSignal(ref, GSN_SYNC_THREAD_REQ, signal, 4, req_prio);
    ptr.p->m_cnt++;
  }
  ptr.p->m_next = instance;
}

void
SimulatedBlock::execSYNC_THREAD_REQ(Signal* signal)
{
  jamEntry();
  Uint32 ref = signal->theData[0];
  Uint32 conf_prio = signal->theData[3];
  sendSignal(ref, GSN_SYNC_THREAD_CONF, signal, signal->getLength(),
             JobBufferLevel(conf_prio));
}

void
SimulatedBlock::execSYNC_THREAD_CONF(Signal* signal)
{
  jamEntry();
  Ptr<SyncThreadRecord> ptr;
  ndbrequire(c_syncThreadPool.getPtr(ptr, signal->theData[1]));

  ndbrequire(ptr.p->m_cnt > 0);
  ptr.p->m_cnt--;

  sendSYNC_THREAD_REQ(signal, ptr);

  if (ptr.p->m_cnt > 0)
  {
    jam();
    return;
  }

  Callback copy = ptr.p->m_callback;
  c_syncThreadPool.release(ptr);
  execute(signal, copy, 0);
  return;
}

void
SimulatedBlock::execSYNC_REQ(Signal* signal)
{
  jamEntry();
  Uint32 ref = signal->theData[0];
  Uint32 prio = signal->theData[2];
  sendSignal(ref, GSN_SYNC_CONF, signal, signal->getLength(),
             JobBufferLevel(prio));
}

void
SimulatedBlock::synchronize_external_signals(Signal* signal, const Callback& cb)
{
#ifndef NDBD_MULTITHREADED
  Callback copy = cb;
  execute(signal, copy, 0);
#else
  jam();

  BlockThreadBitmask threads;

  const Uint32 my_thr_no = getThreadId();
  Uint32 cnt = mt_get_addressable_threads(my_thr_no, threads);

  // Assume current thread does not need synchronization
  if (threads.get(my_thr_no))
  {
    jam();
    threads.clear(my_thr_no);
    cnt--;
  }

  synchronize_threads(signal, threads, cb, JBB, JBA);
#endif
}

void
SimulatedBlock::synchronize_path(Signal * signal,
                                 const Uint32 blocks[],
                                 const Callback & cb,
                                 JobBufferLevel prio)
{
  jam();

  // reuse SyncThreadRecord
  Ptr<SyncThreadRecord> ptr;
  ndbrequire(c_syncThreadPool.seize(ptr));
  ptr.p->m_cnt = 0; // with count of 0
  ptr.p->m_callback = cb;

  SyncPathReq* req = CAST_PTR(SyncPathReq, signal->getDataPtrSend());
  req->senderData = ptr.i;
  req->prio = Uint32(prio);
  req->count = 1;
  if (blocks[0] == 0)
  {
    jam();
    ndbabort(); // TODO
  }
  else
  {
    jam();
    Uint32 len = 0;
    for (; blocks[len+1] != 0; len++)
    {
      req->path[len] = blocks[len+1];
    }
    req->pathlen = 1 + len;
    req->path[len] = reference();
    sendSignal(numberToRef(blocks[0], getOwnNodeId()),
               GSN_SYNC_PATH_REQ, signal,
               SyncPathReq::SignalLength + (1 + len), prio);
  }
}

void
SimulatedBlock::execSYNC_PATH_REQ(Signal* signal)
{
  LOCAL_SIGNAL(signal);
  jamEntry();
  SyncPathReq * req = CAST_PTR(SyncPathReq, signal->getDataPtrSend());
  ndbrequire(req->pathlen >= 1 && req->pathlen <= SyncPathReq::MaxPathLen);
  ndbrequire(signal->getLength() >= SyncPathReq::SignalLength + req->pathlen);
  if (req->pathlen == 1)
  {
    jam();
    SyncPathReq copy = *req;
    SyncPathConf* conf = CAST_PTR(SyncPathConf, signal->getDataPtrSend());
    conf->senderData = copy.senderData;
    conf->count = copy.count;
    sendSignal(copy.path[0], GSN_SYNC_PATH_CONF, signal,
               SyncPathConf::SignalLength, JobBufferLevel(copy.prio));
  }
  else
  {
    jam();
    Uint32 ref = numberToRef(req->path[0], getOwnNodeId());
    req->pathlen--;
    memmove(req->path, req->path + 1, 4 * req->pathlen);
    sendSignal(ref, GSN_SYNC_PATH_REQ, signal,
               SyncPathReq::SignalLength + (1 + req->pathlen),
               JobBufferLevel(req->prio));
  }
}

void
SimulatedBlock::execSYNC_PATH_CONF(Signal* signal)
{
  jamEntry();
  SyncPathConf conf = * CAST_CONSTPTR(SyncPathConf, signal->getDataPtr());
  Ptr<SyncThreadRecord> ptr;

  ndbrequire(c_syncThreadPool.getPtr(ptr, conf.senderData));

  if (ptr.p->m_cnt == 0)
  {
    jam();
    ptr.p->m_cnt = conf.count;
  }

  if (ptr.p->m_cnt == 1)
  {
    jam();
    Callback copy = ptr.p->m_callback;
    c_syncThreadPool.release(ptr);
    execute(signal, copy, 0);
    return;
  }

  ptr.p->m_cnt --;
}


bool
SimulatedBlock::checkNodeFailSequence(Signal* signal)
{
  Uint32 ref = signal->getSendersBlockRef();

  /**
   * Make sure that a signal being part of node-failure handling
   *   from a remote node, does not get to us before we got the NODE_FAILREP
   *   (this to avoid tricky state handling to some extent when receiving
   *    signals from old nodes)
   *
   * To ensure this, we send the signal via the transporter for the remote
   * sender node via QMGR and NDBCNTR to DBDIH.  Although approximating
   * synchronization between all threads and transporters with a single hop to
   * DBLQH_REF to at least send via another thread for multi threaded data
   * node.
   *
   * The extra time should be negilable
   *
   * Note, make an exception for signals sent by our self
   *       as they are only sent as a consequence of NODE_FAILREP
   *
   * Also note that this function no longer guarantee that signal arrives to
   * its destination after corresponding NODE_FAILREP, as a complement caller
   * need some further logic delaying the processing of the signal until
   * NODE_FAILREP have been seen.
   */
  if (ref == reference() ||
      (refToNode(ref) == getOwnNodeId() &&
       refToMain(ref) == DBDIH))
  {
    jam();
    return true;
  }

  Uint32 trpman_ref;
  if (globalData.ndbMtReceiveThreads == 0)
  {
    jam();
    ndbrequire(!isNdbMt());
    trpman_ref = TRPMAN_REF;
  }
  else
  {
    jam();
    ndbrequire(isNdbMt());
    Uint32 sender_node = refToNode(ref);
    Uint32 inst = (get_recv_thread_idx(sender_node) + /* proxy */ 1);
    if (inst > NDBMT_MAX_BLOCK_INSTANCES)
    {
      jam();
      trpman_ref = TRPMAN_REF;
    }
    else
    {
      jam();
      trpman_ref = numberToRef(TRPMAN, inst, getOwnNodeId());
    }
  }

  RoutePath path[5];
  Uint32 path_idx = 0;

  /* Start at TRPMAN for sending node */
  path[path_idx].ref = trpman_ref;
  path[path_idx].prio = JBA;
  path_idx++;

  /* Follow COMMIT_FAILREQ to QMGR */
  path[path_idx].ref = QMGR_REF;
  path[path_idx].prio = JBB;
  path_idx++;

  /*
   * Should be sync_threads, but sends only to DBLQH_REF to at least send
   * to another thread than main thread (if using a multi threaded data node)
   */
  path[path_idx].ref = DBLQH_REF;
  path[path_idx].prio = JBB;
  path_idx++;

  /* Follow NODE_FAILREP to NDBCNT */
  path[path_idx].ref = NDBCNTR_REF;
  path[path_idx].prio = JBB;
  path_idx++;

  /* Follow NODE_FAILREP to DBDIH */
  path[path_idx].ref = DBDIH_REF;
  path[path_idx].prio = JBB;
  path_idx++;

  ndbrequire(path_idx <= NDB_ARRAY_SIZE(path));

  Uint32 dst[1];
  dst[0] = reference();

  SectionHandle handle(this, signal);
  Uint32 gsn = signal->header.theVerId_signalNumber;
  Uint32 len = signal->getLength();

  sendRoutedSignal(path, path_idx, dst, 1, gsn, signal, len, JBB, &handle);
  return false;
}

#ifdef ERROR_INSERT
void
SimulatedBlock::setDelayedPrepare()
{
#ifdef NDBD_MULTITHREADED
  mt_set_delayed_prepare(m_threadId);
#else
  // ndbd todo
#endif
}
#endif

void
SimulatedBlock::setup_wakeup()
{
#ifdef NDBD_MULTITHREADED
#else
  globalTransporterRegistry.setup_wakeup_socket();
#endif
}

void
SimulatedBlock::wakeup()
{
#ifdef NDBD_MULTITHREADED
  mt_wakeup(this);
#else
  globalTransporterRegistry.wakeup();
#endif
}


void
SimulatedBlock::ndbinfo_send_row(Signal* signal,
                                 const DbinfoScanReq& req,
                                 const Ndbinfo::Row& row,
                                 Ndbinfo::Ratelimit& rl) const
{
  // Check correct number of columns against table
  assert(row.columns() == Ndbinfo::getTable(req.tableId).columns());

  TransIdAI *tidai= (TransIdAI*)signal->getDataPtrSend();
  tidai->connectPtr= req.resultData;
  tidai->transId[0]= req.transId[0];
  tidai->transId[1]= req.transId[1];

  LinearSectionPtr ptr[3];
  ptr[0].p = row.getDataPtr();
  ptr[0].sz = row.getLength();

  rl.rows++;
  rl.bytes += row.getLength();

  sendSignal(req.resultRef, GSN_DBINFO_TRANSID_AI,
             signal, TransIdAI::HeaderLength, JBB, ptr, 1);
}


void
SimulatedBlock::ndbinfo_send_scan_break(Signal* signal,
                                       DbinfoScanReq& req,
                                       const Ndbinfo::Ratelimit& rl,
                                       Uint32 data1, Uint32 data2,
                                       Uint32 data3, Uint32 data4) const
{
  DbinfoScanConf* conf= (DbinfoScanConf*)signal->getDataPtrSend();
  const Uint32 signal_length = DbinfoScanReq::SignalLength + req.cursor_sz;
  MEMCOPY_NO_WORDS(conf, &req, signal_length);

  conf->returnedRows = rl.rows;

  // Update the cursor with current item number
  Ndbinfo::ScanCursor* cursor =
    (Ndbinfo::ScanCursor*)DbinfoScan::getCursorPtrSend(conf);

  cursor->data[0] = data1;
  cursor->data[1] = data2;
  cursor->data[2] = data3;
  cursor->data[3] = data4;

  // Increase number of rows and bytes sent to far
  cursor->totalRows += rl.rows;
  cursor->totalBytes += rl.bytes;

  Ndbinfo::ScanCursor::setHasMoreData(cursor->flags, true);

  if (rl.rows > 0)
  {
    jam();
    /* Send what we have so far back to requestor */
    sendSignal(cursor->senderRef, GSN_DBINFO_SCANCONF, signal,
               signal_length, JBB);
  }
  else
  {
    jam();
    /* We have nothing yet, take a RTB, but keep scanning */
    sendSignal(reference(), GSN_DBINFO_SCANREQ, signal,
               signal_length, JBB);
  }
}

void
SimulatedBlock::ndbinfo_send_scan_conf(Signal* signal,
                                       DbinfoScanReq& req,
                                       const Ndbinfo::Ratelimit& rl) const
{
  DbinfoScanConf* conf= (DbinfoScanConf*)signal->getDataPtrSend();
  const Uint32 signal_length = DbinfoScanReq::SignalLength + req.cursor_sz;
  Uint32 sender_ref = req.resultRef;
  MEMCOPY_NO_WORDS(conf, &req, signal_length);

  conf->returnedRows = rl.rows;

  if (req.cursor_sz)
  {
    jam();
    // Update the cursor with current item number
    Ndbinfo::ScanCursor* cursor =
      (Ndbinfo::ScanCursor*)DbinfoScan::getCursorPtrSend(conf);

    // Reset all data holders
    memset(cursor->data, 0, sizeof(cursor->data));

    // Increase number of rows and bytes sent to far
    cursor->totalRows += rl.rows;
    cursor->totalBytes += rl.bytes;

    Ndbinfo::ScanCursor::setHasMoreData(cursor->flags, false);

    sender_ref = cursor->senderRef;

  }
  sendSignal(sender_ref, GSN_DBINFO_SCANCONF, signal,
             signal_length, JBB);
}

void SimulatedBlock::init_elapsed_time(Signal *signal,
                                       NDB_TICKS &latestTIME_SIGNAL)
{
  const NDB_TICKS currentTime = NdbTick_getCurrentTicks();
  signal->theData[0] = Uint32(currentTime.getUint64() >> 32);
  signal->theData[1] = Uint32(currentTime.getUint64() & 0xFFFFFFFF);
  latestTIME_SIGNAL = currentTime;
  sendSignal(reference(), GSN_TIME_SIGNAL, signal, 2, JBB);
}

void SimulatedBlock::sendTIME_SIGNAL(Signal *signal,
                                     const NDB_TICKS currentTime,
                                     Uint32 delay)
{
  signal->theData[0] = Uint32(currentTime.getUint64() >> 32);
  signal->theData[1] = Uint32(currentTime.getUint64() & 0xFFFFFFFF);
  sendSignalWithDelay(reference(), GSN_TIME_SIGNAL, signal, delay, 2);
}

/*
  This function is used to handle TIME_SIGNAL. This signal is intended to
  be used sort of like a drum beat. We should execute some timer calls
  every so often. However the OS can easily make the delayed signals to
  be delayed if the OS is occupied with other things. We will never report
  sleeps for longer than twice the expected delay. We rely on the delayed
  signal scheduler to ensure that we run time a bit faster for a while
  after long sleeps.

  This function will return the elapsed time since last time we called it.
*/
Uint64
SimulatedBlock::elapsed_time(Signal *signal,
                             const NDB_TICKS currentTime,
                             NDB_TICKS &latestTIME_SIGNAL,
                             Uint32 expected_delay)
{
  const Uint64 elapsed_time =
    NdbTick_Elapsed(latestTIME_SIGNAL, currentTime).milliSec();
  latestTIME_SIGNAL = currentTime;

  if (elapsed_time > Uint64(2 * expected_delay))
  {
    return Uint64(2 * expected_delay);
  }
  return elapsed_time;
}

#ifdef VM_TRACE
void
SimulatedBlock::assertOwnThread()
{
#ifdef NDBD_MULTITHREADED
  mt_assert_own_thread(this);
#endif
}

#endif

Uint32
SimulatedBlock::get_recv_thread_idx(TrpId trp_id)
{
#ifdef NDBD_MULTITHREADED
  return mt_get_recv_thread_idx(trp_id);
#else
  return 0;
#endif
}

#ifndef NDBD_MULTITHREADED
/**
 * Add a stub for this function since we have some code in ErrorReporter.cpp
 * that needs this function, it's only really needed for ndbmtd, so need an
 * empty function in ndbd.
 */
void
ErrorReporter::prepare_to_crash(bool first_phase, bool error_insert_crash)
{
  (void)first_phase;
  (void)error_insert_crash;
}
#endif

/**
 * Implementation of SegmentUtils
 * Here we forward the calls, but
 * in ndbmtd, add our thread cache
 * + lock function arguments.
 */

SectionSegment*
SimulatedBlock::getSegmentPtr(Uint32 iVal)
{
  return g_sectionSegmentPool.getPtr(iVal);
}

bool
SimulatedBlock::seizeSegment(Ptr<SectionSegment>& p)
{
  return g_sectionSegmentPool.seize(SB_SP_REL_ARG p);
}

void
SimulatedBlock::releaseSegment(Uint32 iVal)
{
  g_sectionSegmentPool.release(SB_SP_REL_ARG iVal);
}

void
SimulatedBlock::releaseSegmentList(Uint32 firstSegmentIVal)
{
  ::releaseSection(SB_SP_ARG firstSegmentIVal);
}

#ifdef NDB_DEBUG_RES_OWNERSHIP
void
SimulatedBlock::lock_global_ssp()
{
#ifdef NDBD_MULTITHREADED
  f_section_lock.lock();
#endif
}

void
SimulatedBlock::unlock_global_ssp()
{
#ifdef NDBD_MULTITHREADED
  f_section_lock.unlock();
#endif
}
#endif

void
SimulatedBlock::print_static_distr_info(DistributionHandler * const handle)
{
  Uint32 num_ldm_instances = getNumLDMInstances();
  if (m_num_query_thread_per_ldm == 0)
  {
    /* No distribution information used at all */
    return;
  }
  /* Print the LDM groups */
  for (Uint32 ldm = 0; ldm < num_ldm_instances; ldm++)
  {
    g_eventLogger->info("LDM Group %u contains LDM thread %u",
                        ldm, ldm + 1);
    Uint32 first_query_instance = (ldm + 1) * m_num_query_thread_per_ldm;
    for (Uint32 qt = 0; qt < m_num_query_thread_per_ldm; qt++)
    {
      g_eventLogger->info("LDM Group %u contains Query thread %u",
                          ldm, first_query_instance + qt);
    }
  }
  /* Print the Round Robin groups */
  Uint32 found_ldm = 0;
  Uint32 num_rr_groups = m_num_rr_groups;
  for (Uint32 rr_group = 0; rr_group < num_rr_groups; rr_group++)
  {
    for (Uint32 ldm = 0; ldm < num_ldm_instances; ldm++)
    {
      if (m_rr_group[ldm] == rr_group)
      {
        found_ldm++;
        /* LDM contained in this Round Robin group */
        g_eventLogger->info("LDM Group %u contained in Round Robin group %u"
                            ", RR groups only use query threads",
                            ldm, rr_group);
      }
    }
  }
  ndbrequire(found_ldm == num_ldm_instances);
}

#ifdef NDBD_MULTITHREADED
#ifdef DEBUG_SCHED_STATS
  static int deb_count = 0;
#endif
#endif
void
SimulatedBlock::print_debug_sched_stats(DistributionHandler * const handle)
{
#ifdef NDBD_MULTITHREADED
#ifdef DEBUG_SCHED_STATS
  if (deb_count++ < 20)
  {
    return;
  }
  deb_count = 0;
  g_eventLogger->info("LQHKEYREQ LDM: %llu, LQHKEYREQ LQ: %llu"
                      ", LQHKEYREQ RR: %llu",
    handle->m_lqhkeyreq_ldm,
    handle->m_lqhkeyreq_lq,
    handle->m_lqhkeyreq_rr);
  g_eventLogger->info("SCAN_FRAGREQ LDM %llu, SCAN_FRAGREQ LQ: %llu"
                      ", SCAN_FRAGREQ RR: %llu",
    handle->m_scan_fragreq_ldm,
    handle->m_scan_fragreq_lq,
    handle->m_scan_fragreq_rr);
  g_eventLogger->info("LQHKEYREQ LDM instances %u %u %u %u %u %u %u %u",
    handle->m_lqhkeyreq_ldm_count[1], handle->m_lqhkeyreq_ldm_count[2],
    handle->m_lqhkeyreq_ldm_count[3], handle->m_lqhkeyreq_ldm_count[4],
    handle->m_lqhkeyreq_ldm_count[5], handle->m_lqhkeyreq_ldm_count[6],
    handle->m_lqhkeyreq_ldm_count[7], handle->m_lqhkeyreq_ldm_count[8]);
  g_eventLogger->info("LQHKEYREQ QT instances %u %u %u %u %u %u %u %u",
    handle->m_lqhkeyreq_qt_count[1], handle->m_lqhkeyreq_qt_count[2],
    handle->m_lqhkeyreq_qt_count[3], handle->m_lqhkeyreq_qt_count[4],
    handle->m_lqhkeyreq_qt_count[5], handle->m_lqhkeyreq_qt_count[6],
    handle->m_lqhkeyreq_qt_count[7], handle->m_lqhkeyreq_qt_count[8]);
  g_eventLogger->info("SCAN_FRAGREQ LDM instances %u %u %u %u %u %u %u %u",
    handle->m_scan_fragreq_ldm_count[1], handle->m_scan_fragreq_ldm_count[2],
    handle->m_scan_fragreq_ldm_count[3], handle->m_scan_fragreq_ldm_count[4],
    handle->m_scan_fragreq_ldm_count[5], handle->m_scan_fragreq_ldm_count[6],
    handle->m_scan_fragreq_ldm_count[7], handle->m_scan_fragreq_ldm_count[8]);
  g_eventLogger->info("SCAN_FRAGREQ QT instances %u %u %u %u %u %u %u %u",
    handle->m_scan_fragreq_qt_count[1], handle->m_scan_fragreq_qt_count[2],
    handle->m_scan_fragreq_qt_count[3], handle->m_scan_fragreq_qt_count[4],
    handle->m_scan_fragreq_qt_count[5], handle->m_scan_fragreq_qt_count[6],
    handle->m_scan_fragreq_qt_count[7], handle->m_scan_fragreq_qt_count[8]);
  handle->m_lqhkeyreq_ldm = 0;
  handle->m_lqhkeyreq_lq = 0;
  handle->m_lqhkeyreq_rr = 0;
  handle->m_scan_fragreq_ldm = 0;
  handle->m_scan_fragreq_lq = 0;
  handle->m_scan_fragreq_rr = 0;
  for (Uint32 i = 0; i < MAX_NDBMT_LQH_THREADS; i++)
  {
    handle->m_lqhkeyreq_ldm_count[i] = 0;
    handle->m_scan_fragreq_ldm_count[i] = 0;
  }
  for (Uint32 i = 0; i < MAX_NDBMT_QUERY_THREADS; i++)
  {
    handle->m_lqhkeyreq_qt_count[i] = 0;
    handle->m_scan_fragreq_qt_count[i] = 0;
  }
  Uint64 *total_words;
  Uint64 *est_stats;
  for (Uint32 j = 1; j <= 8; j++)
  {
    get_jbb_estimated_stats(DBLQH, j, &total_words, &est_stats);
    g_eventLogger->info("LDM(%u) total: %llu, %llu %llu %llu %llu %llu"
                        " %llu %llu %llu %llu %llu",
                        j, *total_words,
                        est_stats[0], est_stats[1], est_stats[2], est_stats[3],
                        est_stats[4], est_stats[5], est_stats[6], est_stats[7],
                        est_stats[8], est_stats[9]);
    *total_words = 0;
    for (Uint32 i = 0; i < 10; i++)
    {
      est_stats[i] = 0;
    }
  }
  for (Uint32 j = 1; j <= 8; j++)
  {
    get_jbb_estimated_stats(DBQLQH, j, &total_words, &est_stats);
    g_eventLogger->info("QT(%u) total: %llu, %llu %llu %llu %llu %llu"
                        " %llu %llu %llu %llu %llu",
                        j, *total_words,
                        est_stats[0], est_stats[1], est_stats[2], est_stats[3],
                        est_stats[4], est_stats[5], est_stats[6], est_stats[7],
                        est_stats[8], est_stats[9]);
    *total_words = 0;
    for (Uint32 i = 0; i < 10; i++)
    {
      est_stats[i] = 0;
    }
  }
#endif
#endif
}

void
SimulatedBlock::get_load_indicators(DistributionHandler * const handle,
                                    Uint32 rr_group)
{
#ifdef NDBD_MULTITHREADED
  prefetch_load_indicators(&m_rr_group[0], rr_group);
  Uint32 num_ldm_threads = globalData.ndbMtLqhThreads;
  Uint32 first_ldm_instance = globalData.ndbMtMainThreads;
  Uint32 num_query_threads = globalData.ndbMtQueryThreads;
  Uint32 num_distr_threads = num_ldm_threads + num_query_threads;
  Uint32 min_load = 0xFF;
  for (Uint32 i = 0; i < num_ldm_threads; i++)
  {
    if (m_rr_group[i] == rr_group)
    {
      Uint32 thr_no = i + first_ldm_instance;
      struct LdmThreadState *ldm_state = &handle->m_ldm_state[i];
      ldm_state->m_load_indicator = get_load_indicator(thr_no);
      min_load = MIN(min_load, ldm_state->m_load_indicator);
    }
  }
  for (Uint32 i = num_ldm_threads; i < num_distr_threads; i++)
  {
    if (m_rr_group[i] == rr_group)
    {
      Uint32 thr_no = i + first_ldm_instance;
      struct QueryThreadState *q_state = &handle->m_query_state[i];
      q_state->m_load_indicator = get_load_indicator(thr_no);
      min_load = MIN(min_load, q_state->m_load_indicator);
    }
  }
  if (min_load > 1)
  {
    /**
     * All threads in round robin group are overloaded, thus set minimum
     * load to represent the unloaded case.
     */
    for (Uint32 i = 0; i < num_ldm_threads; i++)
    {
      if (m_rr_group[i] == rr_group)
      {
        struct LdmThreadState *ldm_state = &handle->m_ldm_state[i];
        ldm_state->m_load_indicator -= (min_load - 1);
      }
    }
    for (Uint32 i = num_ldm_threads; i < num_distr_threads; i++)
    {
      if (m_rr_group[i] == rr_group)
      {
        struct QueryThreadState *q_state = &handle->m_query_state[i];
        q_state->m_load_indicator -= (min_load - 1);
      }
    }
  }
#endif
}

/**
 * We are sending an LQHKEYREQ signal using COMMITTED READ to either a
 * query thread or an LDM thread. This code makes the selection of
 * where to send it and returns a block reference where the signal
 * should be sent.
 *
 * The selection is based on load levels. We use two levels of loads
 * to impact this decision. We use a long-term scheduling based on
 * the CPU load the last 100 milliseconds. Thus we change the
 * scheduling 10 times per second to accommodate the load of the various
 * threads. This means that e.g. if a certain LDM thread is very busy
 * with write actions and other things it will get a lower risk of being
 * selected here in the next scheduling decision.
 *
 * The 100 ms scheduling takes into account long-term changes of the load.
 * However we need a mechanism to also react a bit quicker. If a complex
 * query starts up and starts consuming large amounts of CPU it could very
 * will spawn up and complete within one 100 ms interval. Thus it would
 * not impact the scheduling at all. It could still in a very short time
 * consume a major portion of the CPU power.
 *
 * To handle this we introduce a load indicator. Each scheduler (TC threads
 * and receive threads) need to know about load indicators in the executor
 * threads (LDM threads and Query threads).
 *
 * The generic idea is that the normal load is 1, if the load increases
 * we can set the load indicator to 2 and we will only receive half of
 * the scheduled signals compared to the 100 ms decision. We can set the
 * load to at most 5 where it will receive at most 20% of the load the
 * long term scheduling.
 *
 * The load indicators is set by the executor threads at intervals of
 * a few hundred microseconds and are based on queue levels for the
 * past few hundred microseconds.
 *
 * The scheduler thread that executes here will retrieve this information
 * at certain intervals. Each load indicator will be in use for a constant
 * number of schedulings to a certain Round Robin group. This is currently
 * set to 48 signals. The idea is to ensure that we react to changes in the
 * execution environment after about 1 millisecond.
 *
 * 1. Scheduling
 * -------------
 *
 * 1.1. LDM Scheduling
 * -------------------
 * The scheduling to an LDM thread follows the following pattern.
 * We calculate a weight based on the load information once every 100 ms.
 * If no load is found we start at a weight of 33. The idea with this weight
 * is that 33% of the scheduled LQHKEYREQ/SCAN_FRAGREQ will be sent to the
 * LDM thread owning the data. A higher weight means that the LDM thread is
 * not so busy and can handle a larger portion of the load and a lower weight
 * means that it is busy and needs to take a smaller portion of the load.
 *
 * We increment
 * m_current_weight_lqhkeyreq_count/m_current_weight_scan_fragreq_count
 * by the weight (m_current_weight) each time we make a scheduling decision.
 * The 1 ms load gets into the picture by dividing the weight by the
 * load indicator (normally equal to 1). Now if the current weight count
 * is smaller than the current weight, then we will use the LDM thread.
 * Otherwise we will move to decide which Query thread that should handle
 * the signal instead.
 *
 * The same code for deciding to handle by LDM is also in
 * get_scan_fragreq_ref, but it has its own counters. Also the decision
 * is kept for a number of signals to get a bit of batching happening.
 * We do currently more batching of LQHKEYREQ and not so much for scans.
 * It is intended that this should be modelled and tested a bit more.
 *
 * 1.2. Query thread scheduling
 * ----------------------------
 * We have decided to use a query thread and not the LDM thread based on
 * the above discussion. The next step is to pick which query thread to
 * use. We will always use a query thread within the same Round Robin
 * group.
 *
 * The preference is the use a query thread in the same LDM group as the
 * LDM thread. Normally the decision would be a round robin decision based
 * on the weights of the query thread. However we can "steal" from the
 * round robin up to a certain point. This means that if we decide to
 * schedule it to the same LDM group we have to ensure that we skip this
 * query thread the next time it is selected by the round robin selection.
 *
 * We can "steal" up to a maximum level which is based on the weight. With
 * high weight we get scheduled often and thus we can handle many "steals".
 * The load indicator is used here by adding to the stolen signals each
 * time the thread is used. Thus we will skip a number of signals after
 * using the thread at high load.
 *
 * The weight count is slightly modified for scans to give LDM threads a
 * bit higher priority to run scans. The reason is that it is less
 * concurrency issues with running key lookups in query threads and thus
 * we want to give LDM threads a bit more space to run scan queries.
 */
Uint32 SimulatedBlock::get_lqhkeyreq_ref(DistributionHandler * const handle,
                                         Uint32 ldm_instance_no)
{
#ifdef NDBD_MULTITHREADED
  Uint32 rr_group = m_rr_group[ldm_instance_no - 1];
  struct RoundRobinInfo *rr_info = 
    (struct RoundRobinInfo*)&handle->m_rr_info[rr_group];
  {
    /**
     * load_counter tracks when we need to gather load information
     * from the threads in the Round Robin group.
     *
     */
    Uint32 load_counter = rr_info->m_load_indicator_counter;
    struct LdmThreadState *ldm_state =
      &handle->m_ldm_state[ldm_instance_no - 1];
    Uint32 counter = ldm_state->m_lqhkeyreq_counter;
    Uint32 current_weight_count = ldm_state->m_current_weight_lqhkeyreq_count;
    Uint32 load_indicator = ldm_state->m_load_indicator;
    Uint32 curr_weight = ldm_state->m_current_weight / load_indicator;
    counter++;
    load_counter++;
    if (counter >= m_num_lqhkeyreq_counts)
    {
      current_weight_count += curr_weight;
      if (current_weight_count >= MAX_LDM_DISTRIBUTION_WEIGHT)
      {
        current_weight_count -= MAX_LDM_DISTRIBUTION_WEIGHT;
      }
      counter = 0;
      ldm_state->m_current_weight_lqhkeyreq_count = current_weight_count;
    }
    ldm_state->m_lqhkeyreq_counter = counter + 1;
    if (likely(load_counter < m_rr_load_refresh_count))
    {
      rr_info->m_load_indicator_counter = load_counter;
    }
    else
    {
      get_load_indicators(handle, rr_group);
    }
    if (current_weight_count < curr_weight)
    {
      /**
       * We load balance between LDM thread and the query threads.
       * In this case we have selected to use the LDM thread.
       */
      Uint32 ref = numberToRef(DBLQH,
                               ldm_instance_no,
                               getOwnNodeId());
#ifdef DEBUG_SCHED_STATS
      handle->m_lqhkeyreq_ldm++;
      handle->m_lqhkeyreq_ldm_count[ldm_instance_no]++;
#endif
      return ref;
    }
  }
  /**
   * We selected to choose a query thread for execution of this query.
   * We will first attempt to use the query thread local to the LDM
   * thread.
   */
  {
    struct QueryThreadState *q_state =
      &handle->m_query_state[ldm_instance_no - 1];
    Uint32 max_count = q_state->m_max_lqhkeyreq_count;
    Uint32 load_indicator = q_state->m_load_indicator;
    Uint32 current_stolen_count = q_state->m_current_stolen_lqhkeyreq;
    if (current_stolen_count <= max_count)
    {
      /**
       * There was sufficient room to steal a slot for sending to the
       * query thread of the same instance as the LDM thread. So send
       * to this query thread and increment the current stolen count.
       */
      assert(load_indicator > 0);
      Uint32 ref = numberToRef(DBQLQH,
                               ldm_instance_no,
                               getOwnNodeId());
      q_state->m_current_stolen_lqhkeyreq = current_stolen_count +
                                            load_indicator;
#ifdef DEBUG_SCHED_STATS
      handle->m_lqhkeyreq_lq++;
      handle->m_lqhkeyreq_qt_count[ldm_instance_no]++;
#endif
      return ref;
    }
  }
  /* Have to select from the Round Robin group of query threads. */
  /* Pick next according to Round Robin distribution */
  do
  {
    Uint32 count = rr_info->m_lqhkeyreq_to_same_thread;
    Uint32 inx = rr_info->m_lqhkeyreq_distr_signal_index;
    if (count >= m_num_lqhkeyreq_counts)
    {
      count = 0;
      inx++;
      if (inx < rr_info->m_distribution_signal_size)
      {
        ;
      }
      else
      {
        inx = 0;
      }
      rr_info->m_lqhkeyreq_distr_signal_index = inx;
    }
    Uint32 ref = rr_info->m_distribution_signal[inx];
    rr_info->m_lqhkeyreq_to_same_thread = count + 1;
    Uint32 instance_no = refToInstance(ref);
    struct QueryThreadState *q_state = &handle->m_query_state[instance_no - 1];
    Uint32 load_indicator = q_state->m_load_indicator;
    Uint32 current_stolen = q_state->m_current_stolen_lqhkeyreq;
    if (current_stolen == 0)
    {
      /**
       * This query thread haven't stolen anything yet, so simply use it to
       * send this signal. We add stolen signals to handle load indicator
       * here. With normal load (== 1) we don't add anything here. With
       * load indicator set to 2 we add 1 to stolen, this means that we will
       * skip sending to this half of the time. Thus cutting the load to
       * the thread by half.
       */
      assert(load_indicator > 0);
      q_state->m_current_stolen_lqhkeyreq = load_indicator - 1;
#ifdef DEBUG_SCHED_STATS
      handle->m_lqhkeyreq_rr++;
      handle->m_lqhkeyreq_qt_count[instance_no]++;
#endif
      return ref;
    }
    /**
     * The query thread had already stolen a signal before, we need to
     * decrement the stolen count and continue searching for the query
     * thread to receive this signal.
     */
    q_state->m_current_stolen_lqhkeyreq = current_stolen - 1;
  } while (true);
#endif
  return 0;
}

Uint32 SimulatedBlock::get_scan_fragreq_ref(DistributionHandler * const handle,
                                            Uint32 ldm_instance_no)
{
#ifdef NDBD_MULTITHREADED
  Uint32 rr_group = m_rr_group[ldm_instance_no - 1];
  struct RoundRobinInfo *rr_info = 
    (struct RoundRobinInfo*)&handle->m_rr_info[rr_group];
  {
    Uint32 load_counter = rr_info->m_load_indicator_counter;
    struct LdmThreadState *ldm_state =
      &handle->m_ldm_state[ldm_instance_no - 1];
    Uint32 counter = ldm_state->m_scan_fragreq_counter;
    Uint32 current_weight_count = 
      ldm_state->m_current_weight_scan_fragreq_count;
    Uint32 load_indicator = ldm_state->m_load_indicator;
    Uint32 curr_weight = ldm_state->m_current_weight / load_indicator;
    counter++;
    load_counter += LOAD_SCAN_FRAGREQ;
    if (counter >= m_num_scan_fragreq_counts)
    {
      current_weight_count += curr_weight;
      if (current_weight_count >= MAX_LDM_DISTRIBUTION_WEIGHT)
      {
        current_weight_count -= MAX_LDM_DISTRIBUTION_WEIGHT;
      }
      counter = 0;
      ldm_state->m_current_weight_scan_fragreq_count = current_weight_count;
    }
    ldm_state->m_scan_fragreq_counter = counter + 1;
    if (likely(load_counter < m_rr_load_refresh_count))
    {
      rr_info->m_load_indicator_counter = load_counter;
    }
    else
    {
      get_load_indicators(handle, rr_group);
    }
    if (current_weight_count < curr_weight ||
        (load_indicator == 1 && current_weight_count < (curr_weight + 3)))
    {
      /**
       * We load balance between LDM thread and the query threads.
       * In this case we have selected to use the LDM thread.
       * We give scans a tad higher priority in the LDM threads since
       * the scans have more lock conflicts with LDM thread when executed
       * from the query threads.
       */
      Uint32 ref = numberToRef(DBLQH,
                               ldm_instance_no,
                               getOwnNodeId());
#ifdef DEBUG_SCHED_STATS
      handle->m_scan_fragreq_ldm++;
      handle->m_scan_fragreq_ldm_count[ldm_instance_no]++;
#endif
      return ref;
    }
  }
  {
    struct QueryThreadState *q_state =
      &handle->m_query_state[ldm_instance_no - 1];
    Uint32 max_count = q_state->m_max_scan_fragreq_count;
    Uint32 load_indicator = q_state->m_load_indicator;
    Uint32 current_stolen_count = q_state->m_current_stolen_scan_fragreq;
    if (current_stolen_count <= max_count)
    {
      assert(load_indicator > 0);
      Uint32 ref = numberToRef(DBQLQH,
                               ldm_instance_no,
                               getOwnNodeId());
      q_state->m_current_stolen_scan_fragreq = current_stolen_count +
                                               load_indicator;
#ifdef DEBUG_SCHED_STATS
      handle->m_scan_fragreq_lq++;
      handle->m_scan_fragreq_qt_count[ldm_instance_no]++;
#endif
      return ref;
    }
  }
  do
  {
    Uint32 count = rr_info->m_scan_fragreq_to_same_thread;
    Uint32 inx = rr_info->m_scan_distr_signal_index;
    if (count >= m_num_scan_fragreq_counts)
    {
      count = 0;
      inx++;
      if (inx < rr_info->m_distribution_signal_size)
      {
        ;
      }
      else
      {
        inx = 0;
      }
      rr_info->m_scan_distr_signal_index = inx;
    }
    Uint32 ref = rr_info->m_distribution_signal[inx];
    rr_info->m_scan_fragreq_to_same_thread = count + 1;
    Uint32 instance_no = refToInstance(ref);
    struct QueryThreadState *q_state = &handle->m_query_state[instance_no - 1];
    Uint32 current_stolen = q_state->m_current_stolen_scan_fragreq;
    Uint32 load_indicator = q_state->m_load_indicator;
    if (current_stolen == 0)
    {
      assert(load_indicator > 0);
      q_state->m_current_stolen_scan_fragreq = load_indicator - 1;
#ifdef DEBUG_SCHED_STATS
      handle->m_scan_fragreq_rr++;
      handle->m_scan_fragreq_qt_count[instance_no]++;
#endif
      return ref;
    }
    q_state->m_current_stolen_scan_fragreq = current_stolen - 1;
  } while (true);
#endif
  return 0;
}

/**
 * Need to create the static variables in the SimulatedBlock class.
 */
Uint32 SimulatedBlock::m_rr_group[MAX_DISTR_THREADS];
Uint32 SimulatedBlock::m_num_lqhkeyreq_counts = NUM_LQHKEYREQ_COUNTS;
Uint32 SimulatedBlock::m_num_scan_fragreq_counts = NUM_SCAN_FRAGREQ_COUNTS;
Uint32 SimulatedBlock::m_rr_load_refresh_count = RR_LOAD_REFRESH_COUNT;
Uint32 SimulatedBlock::m_num_rr_groups = 0;
Uint32 SimulatedBlock::m_num_query_thread_per_ldm = 0;
Uint32 SimulatedBlock::m_num_distribution_threads = 0;
bool SimulatedBlock::m_inited_rr_groups = false;

/** 
 * #undef is needed since this file is included by SimulatedBlock_nonmt.cpp
 * and SimulatedBlock_mt.cpp
 */
#undef JAM_FILE_ID
