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

#include "Cmvmi.hpp"

#include <Configuration.hpp>
#include <kernel_types.h>
#include <TransporterRegistry.hpp>
#include <NdbOut.hpp>
#include <NdbMem.h>

#include <SignalLoggerManager.hpp>
#include <FastScheduler.hpp>

#define DEBUG(x) { ndbout << "CMVMI::" << x << endl; }

#include <signaldata/TestOrd.hpp>
#include <signaldata/EventReport.hpp>
#include <signaldata/TamperOrd.hpp>
#include <signaldata/StartOrd.hpp>
#include <signaldata/CloseComReqConf.hpp>
#include <signaldata/SetLogLevelOrd.hpp>
#include <signaldata/EventSubscribeReq.hpp>
#include <signaldata/DumpStateOrd.hpp>
#include <signaldata/DisconnectRep.hpp>

#include <EventLogger.hpp>
#include <TimeQueue.hpp>
#include <new>

#include <NdbSleep.h>
#include <SafeCounter.hpp>

// Used here only to print event reports on stdout/console.
EventLogger g_eventLogger;
extern int simulate_error_during_shutdown;

Cmvmi::Cmvmi(const Configuration & conf) :
  SimulatedBlock(CMVMI, conf)
  ,theConfig((Configuration&)conf)
  ,subscribers(subscriberPool)
{
  BLOCK_CONSTRUCTOR(Cmvmi);

  Uint32 long_sig_buffer_size;
  const ndb_mgm_configuration_iterator * p = conf.getOwnConfigIterator();
  ndbrequire(p != 0);

  ndb_mgm_get_int_parameter(p, CFG_DB_LONG_SIGNAL_BUFFER,  
			    &long_sig_buffer_size);

  long_sig_buffer_size= long_sig_buffer_size / 256;
  g_sectionSegmentPool.setSize(long_sig_buffer_size);

  // Add received signals
  addRecSignal(GSN_CONNECT_REP, &Cmvmi::execCONNECT_REP);
  addRecSignal(GSN_DISCONNECT_REP, &Cmvmi::execDISCONNECT_REP);

  addRecSignal(GSN_NDB_TAMPER,  &Cmvmi::execNDB_TAMPER, true);
  addRecSignal(GSN_SET_LOGLEVELORD,  &Cmvmi::execSET_LOGLEVELORD);
  addRecSignal(GSN_EVENT_REP,  &Cmvmi::execEVENT_REP);
  addRecSignal(GSN_STTOR,  &Cmvmi::execSTTOR);
  addRecSignal(GSN_CLOSE_COMREQ,  &Cmvmi::execCLOSE_COMREQ);
  addRecSignal(GSN_ENABLE_COMORD,  &Cmvmi::execENABLE_COMORD);
  addRecSignal(GSN_OPEN_COMREQ,  &Cmvmi::execOPEN_COMREQ);
  addRecSignal(GSN_TEST_ORD,  &Cmvmi::execTEST_ORD);

  addRecSignal(GSN_STATISTICS_REQ,  &Cmvmi::execSTATISTICS_REQ);
  addRecSignal(GSN_TAMPER_ORD,  &Cmvmi::execTAMPER_ORD);
  addRecSignal(GSN_SET_VAR_REQ,  &Cmvmi::execSET_VAR_REQ);
  addRecSignal(GSN_SET_VAR_CONF,  &Cmvmi::execSET_VAR_CONF);
  addRecSignal(GSN_SET_VAR_REF,  &Cmvmi::execSET_VAR_REF);
  addRecSignal(GSN_STOP_ORD,  &Cmvmi::execSTOP_ORD);
  addRecSignal(GSN_START_ORD,  &Cmvmi::execSTART_ORD);
  addRecSignal(GSN_EVENT_SUBSCRIBE_REQ, 
               &Cmvmi::execEVENT_SUBSCRIBE_REQ);

  addRecSignal(GSN_DUMP_STATE_ORD, &Cmvmi::execDUMP_STATE_ORD);

  addRecSignal(GSN_TESTSIG, &Cmvmi::execTESTSIG);
  
  subscriberPool.setSize(5);

  const ndb_mgm_configuration_iterator * db = theConfig.getOwnConfigIterator();
  for(unsigned j = 0; j<LogLevel::LOGLEVEL_CATEGORIES; j++){
    Uint32 logLevel;
    if(!ndb_mgm_get_int_parameter(db, CFG_MIN_LOGLEVEL+j, &logLevel)){
      clogLevel.setLogLevel((LogLevel::EventCategory)j, 
			    logLevel);
    }
  }
  
  ndb_mgm_configuration_iterator * iter = theConfig.getClusterConfigIterator();
  for(ndb_mgm_first(iter); ndb_mgm_valid(iter); ndb_mgm_next(iter)){
    jam();
    Uint32 nodeId;
    Uint32 nodeType;

    ndbrequire(!ndb_mgm_get_int_parameter(iter,CFG_NODE_ID, &nodeId));
    ndbrequire(!ndb_mgm_get_int_parameter(iter,CFG_TYPE_OF_SECTION,&nodeType));

    switch(nodeType){
    case NodeInfo::DB:
      c_dbNodes.set(nodeId);
      break;
    case NodeInfo::API:
    case NodeInfo::MGM:
    case NodeInfo::REP:
      break;
    default:
      ndbrequire(false);
    }
    setNodeInfo(nodeId).m_type = nodeType;
  }

  setNodeInfo(getOwnNodeId()).m_connected = true;
}

Cmvmi::~Cmvmi()
{
}


void Cmvmi::execNDB_TAMPER(Signal* signal) 
{
  jamEntry();
  SET_ERROR_INSERT_VALUE(signal->theData[0]);
  if(ERROR_INSERTED(9999)){
    CRASH_INSERTION(9999);
  }

  if(ERROR_INSERTED(9998)){
    while(true) NdbSleep_SecSleep(1);
  }

  if(ERROR_INSERTED(9997)){
    ndbrequire(false);
  }

  if(ERROR_INSERTED(9996)){
    simulate_error_during_shutdown= SIGSEGV;
    ndbrequire(false);
  }

  if(ERROR_INSERTED(9995)){
    simulate_error_during_shutdown= SIGSEGV;
    kill(getpid(), SIGABRT);
  }
}//execNDB_TAMPER()

void Cmvmi::execSET_LOGLEVELORD(Signal* signal) 
{
  SetLogLevelOrd * const llOrd = (SetLogLevelOrd *)&signal->theData[0];
  LogLevel::EventCategory category;
  Uint32 level;
  jamEntry();

  for(unsigned int i = 0; i<llOrd->noOfEntries; i++){
    category = (LogLevel::EventCategory)llOrd->theCategories[i];
    level = llOrd->theLevels[i];

    clogLevel.setLogLevel(category, level);
  }
}//execSET_LOGLEVELORD()

void Cmvmi::execEVENT_REP(Signal* signal) 
{
  //-----------------------------------------------------------------------
  // This message is sent to report any types of events in NDB.
  // Based on the log level they will be either ignored or
  // reported. Currently they are printed, but they will be
  // transferred to the management server for further distribution
  // to the graphical management interface.
  //-----------------------------------------------------------------------
  EventReport * const eventReport = (EventReport *)&signal->theData[0]; 
  EventReport::EventType eventType = eventReport->getEventType();

  jamEntry();
  
  /**
   * If entry is not found
   */
  Uint32 threshold = 16;
  LogLevel::EventCategory eventCategory = (LogLevel::EventCategory)0;
  
  for(unsigned int i = 0; i< EventLogger::matrixSize; i++){
    if(EventLogger::matrix[i].eventType == eventType){
      eventCategory = EventLogger::matrix[i].eventCategory;
      threshold     = EventLogger::matrix[i].threshold;
      break;
    }
  }
  
  if(threshold > 15){
    // No entry found in matrix (or event that should never be printed)
    return;
  }
  
  SubscriberPtr ptr;
  for(subscribers.first(ptr); ptr.i != RNIL; subscribers.next(ptr)){
    if(ptr.p->logLevel.getLogLevel(eventCategory) < threshold){
      continue;
    }
    
    sendSignal(ptr.p->blockRef, GSN_EVENT_REP, signal, signal->length(), JBB);
  }
  
  if(clogLevel.getLogLevel(eventCategory) < threshold){
    return;
  }

  // Print the event info
  g_eventLogger.log(eventReport->getEventType(), signal->theData);

}//execEVENT_REP()

void
Cmvmi::execEVENT_SUBSCRIBE_REQ(Signal * signal){
  EventSubscribeReq * subReq = (EventSubscribeReq *)&signal->theData[0];
  SubscriberPtr ptr;

  jamEntry();

  /**
   * Search for subcription
   */
  for(subscribers.first(ptr); ptr.i != RNIL; subscribers.next(ptr)){
    if(ptr.p->blockRef == subReq->blockRef)
      break;
  }
  
  if(ptr.i == RNIL){
    /**
     * Create a new one
     */
    if(subscribers.seize(ptr) == false){
      sendSignal(subReq->blockRef, GSN_EVENT_SUBSCRIBE_REF, signal, 1, JBB);
      return;
    }
    ptr.p->logLevel.clear();
    ptr.p->blockRef = subReq->blockRef;    
  }
  
  if(subReq->noOfEntries == 0){
    /**
     * Cancel subscription
     */
    subscribers.release(ptr.i);
  } else {
    /**
     * Update subscription
     */
    LogLevel::EventCategory category;
    Uint32 level = 0;
    for(Uint32 i = 0; i<subReq->noOfEntries; i++){
      category = (LogLevel::EventCategory)subReq->theCategories[i];
      level = subReq->theLevels[i];
      ptr.p->logLevel.setLogLevel(category,
                                  level);
    }
  }
  
  signal->theData[0] = ptr.i;
  sendSignal(ptr.p->blockRef, GSN_EVENT_SUBSCRIBE_CONF, signal, 1, JBB);
}

void
Cmvmi::cancelSubscription(NodeId nodeId){
  
  SubscriberPtr ptr;
  subscribers.first(ptr);
  
  while(ptr.i != RNIL){
    Uint32 i = ptr.i;
    BlockReference blockRef = ptr.p->blockRef;
    
    subscribers.next(ptr);
    
    if(refToNode(blockRef) == nodeId){
      subscribers.release(i);
    }
  }
}

void Cmvmi::sendSTTORRY(Signal* signal)
{
  jam();
  signal->theData[3] = 1;
  signal->theData[4] = 3;
  signal->theData[5] = 8;
  signal->theData[6] = 255;
  sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 7, JBB);
}//Cmvmi::sendSTTORRY


void Cmvmi::execSTTOR(Signal* signal)
{
  Uint32 theStartPhase  = signal->theData[1];

  jamEntry();
  if (theStartPhase == 1){
    jam();
    sendSTTORRY(signal);
    return;
  } else if (theStartPhase == 3) {
    jam();
    globalData.activateSendPacked = 1;
    sendSTTORRY(signal);
  } else if (theStartPhase == 8){
    /*---------------------------------------------------*/
    /* Open com to API + REP nodes                       */
    /*---------------------------------------------------*/
    signal->theData[0] = 0; // no answer
    signal->theData[1] = 0; // no id
    signal->theData[2] = NodeInfo::API;
    execOPEN_COMREQ(signal);
    signal->theData[0] = 0; // no answer
    signal->theData[1] = 0; // no id
    signal->theData[2] = NodeInfo::REP;
    execOPEN_COMREQ(signal);    
    globalData.theStartLevel = NodeState::SL_STARTED;
    sendSTTORRY(signal);
  } else {
    jam();

    if(theConfig.lockPagesInMainMemory()){
      int res = NdbMem_MemLockAll();
      if(res != 0){
	g_eventLogger.warning("Failed to memlock pages");
	warningEvent("Failed to memlock pages");
      }
    }
    
    sendSTTORRY(signal);
  }
}

void Cmvmi::execCLOSE_COMREQ(Signal* signal)
{
  // Close communication with the node and halt input/output from 
  // other blocks than QMGR
  
  CloseComReqConf * const closeCom = (CloseComReqConf *)&signal->theData[0];

  const BlockReference userRef = closeCom->xxxBlockRef;
  Uint32 failNo = closeCom->failNo;
//  Uint32 noOfNodes = closeCom->noOfNodes;
  
  jamEntry();
  for (unsigned i = 0; i < MAX_NODES; i++){
    if(NodeBitmask::get(closeCom->theNodes, i)){
    
      jam();

      //-----------------------------------------------------
      // Report that the connection to the node is closed
      //-----------------------------------------------------
      signal->theData[0] = EventReport::CommunicationClosed;
      signal->theData[1] = i;
      sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);
      
      globalTransporterRegistry.setIOState(i, HaltIO);
      globalTransporterRegistry.do_disconnect(i);
    }
  }
  if (failNo != 0) {
    jam();
    signal->theData[0] = userRef;
    signal->theData[1] = failNo;
    sendSignal(QMGR_REF, GSN_CLOSE_COMCONF, signal, 19, JBA);
  }
}

void Cmvmi::execOPEN_COMREQ(Signal* signal)
{
  // Connect to the specifed NDB node, only QMGR allowed communication 
  // so far with the node

  const BlockReference userRef = signal->theData[0];
  Uint32 tStartingNode = signal->theData[1];
  Uint32 tData2 = signal->theData[2];
  jamEntry();

  const Uint32 len = signal->getLength();
  if(len == 2){
    globalTransporterRegistry.do_connect(tStartingNode);
    globalTransporterRegistry.setIOState(tStartingNode, HaltIO);

    //-----------------------------------------------------
    // Report that the connection to the node is opened
    //-----------------------------------------------------
    signal->theData[0] = EventReport::CommunicationOpened;
    signal->theData[1] = tStartingNode;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);
    //-----------------------------------------------------
  } else {
    for(unsigned int i = 1; i < MAX_NODES; i++ ) {
      jam();
      if (i != getOwnNodeId() && getNodeInfo(i).m_type == tData2){
	jam();
	globalTransporterRegistry.do_connect(i);
	globalTransporterRegistry.setIOState(i, HaltIO);
	
	signal->theData[0] = EventReport::CommunicationOpened;
	signal->theData[1] = i;
	sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);
      }
    }
  }
  
  if (userRef != 0) {
    jam(); 
    signal->theData[0] = tStartingNode;
    signal->theData[1] = tData2;
    sendSignal(userRef, GSN_OPEN_COMCONF, signal, len - 1,JBA);
  }
}

void Cmvmi::execENABLE_COMORD(Signal* signal)
{
  // Enable communication with all our NDB blocks to this node
  
  Uint32 tStartingNode = signal->theData[0];
  globalTransporterRegistry.setIOState(tStartingNode, NoHalt);
  setNodeInfo(tStartingNode).m_connected = true;
    //-----------------------------------------------------
  // Report that the version of the node
  //-----------------------------------------------------
  signal->theData[0] = EventReport::ConnectedApiVersion;
  signal->theData[1] = tStartingNode;
  signal->theData[2] = getNodeInfo(tStartingNode).m_version;

  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 3, JBB);
  //-----------------------------------------------------
  
  jamEntry();
}

void Cmvmi::execDISCONNECT_REP(Signal *signal)
{
  const DisconnectRep * const rep = (DisconnectRep *)&signal->theData[0];
  const Uint32 hostId = rep->nodeId;
  const Uint32 errNo  = rep->err;
  
  jamEntry();

  setNodeInfo(hostId).m_connected = false;
  setNodeInfo(hostId).m_connectCount++;
  const NodeInfo::NodeType type = getNodeInfo(hostId).getType();
  ndbrequire(type != NodeInfo::INVALID);
  
  if(type == NodeInfo::DB || globalData.theStartLevel == NodeState::SL_STARTED){
    jam();
    DisconnectRep * const rep = (DisconnectRep *)&signal->theData[0];
    rep->nodeId = hostId;
    rep->err = errNo;
    sendSignal(QMGR_REF, GSN_DISCONNECT_REP, signal, 
	       DisconnectRep::SignalLength, JBA);
  } else if((globalData.theStartLevel == NodeState::SL_CMVMI ||
	     globalData.theStartLevel == NodeState::SL_STARTING)
	    && type == NodeInfo::MGM) {
    /**
     * Someone disconnected during cmvmi period
     */
    jam();
    globalTransporterRegistry.do_connect(hostId);
  }

  cancelSubscription(hostId);

  signal->theData[0] = EventReport::Disconnected;
  signal->theData[1] = hostId;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);
}
 
void Cmvmi::execCONNECT_REP(Signal *signal){
  const Uint32 hostId = signal->theData[0];
  jamEntry();
  
  const NodeInfo::NodeType type = (NodeInfo::NodeType)getNodeInfo(hostId).m_type;
  ndbrequire(type != NodeInfo::INVALID);
  globalData.m_nodeInfo[hostId].m_version = 0;
  globalData.m_nodeInfo[hostId].m_signalVersion = 0;
  
  if(type == NodeInfo::DB || globalData.theStartLevel >= NodeState::SL_STARTED){
    jam();
    
    /**
     * Inform QMGR that client has connected
     */

    signal->theData[0] = hostId;
    sendSignal(QMGR_REF, GSN_CONNECT_REP, signal, 1, JBA);
  } else if(globalData.theStartLevel == NodeState::SL_CMVMI ||
            globalData.theStartLevel == NodeState::SL_STARTING) {
    jam();
    /**
     * Someone connected before start was finished
     */
    if(type == NodeInfo::MGM){
      jam();
    } else {
      /**
       * Dont allow api nodes to connect
       */
      abort();
      globalTransporterRegistry.do_disconnect(hostId);
    }
  }
  
  /* Automatically subscribe events for MGM nodes.
   */
  if(type == NodeInfo::MGM){
    jam();
    globalTransporterRegistry.setIOState(hostId, NoHalt);
  }

  //------------------------------------------
  // Also report this event to the Event handler
  //------------------------------------------
  signal->theData[0] = EventReport::Connected;
  signal->theData[1] = hostId;
  signal->header.theLength = 2;
  
  execEVENT_REP(signal);
}

#ifdef VM_TRACE
void
modifySignalLogger(bool allBlocks, BlockNumber bno, 
                   TestOrd::Command cmd, 
                   TestOrd::SignalLoggerSpecification spec){
  SignalLoggerManager::LogMode logMode;

  /**
   * Mapping between SignalLoggerManager::LogMode and 
   *                 TestOrd::SignalLoggerSpecification
   */
  switch(spec){
  case TestOrd::InputSignals:
    logMode = SignalLoggerManager::LogIn;
    break;
  case TestOrd::OutputSignals:
    logMode = SignalLoggerManager::LogOut;
    break;
  case TestOrd::InputOutputSignals:
    logMode = SignalLoggerManager::LogInOut;
    break;
  default:
    return;
    break;
  }
  
  switch(cmd){
  case TestOrd::On:
    globalSignalLoggers.logOn(allBlocks, bno, logMode);
    break;
  case TestOrd::Off:
    globalSignalLoggers.logOff(allBlocks, bno, logMode);
    break;
  case TestOrd::Toggle:
    globalSignalLoggers.logToggle(allBlocks, bno, logMode);
    break;
  case TestOrd::KeepUnchanged:
    // Do nothing
    break;
  }
  globalSignalLoggers.flushSignalLog();
}
#endif

void
Cmvmi::execTEST_ORD(Signal * signal){
  jamEntry();
  
#ifdef VM_TRACE
  TestOrd * const testOrd = (TestOrd *)&signal->theData[0];

  TestOrd::Command cmd;

  {
    /**
     * Process Trace command
     */
    TestOrd::TraceSpecification traceSpec;

    testOrd->getTraceCommand(cmd, traceSpec);
    unsigned long traceVal = traceSpec;
    unsigned long currentTraceVal = globalSignalLoggers.getTrace();
    switch(cmd){
    case TestOrd::On:
      currentTraceVal |= traceVal;
      break;
    case TestOrd::Off:
      currentTraceVal &= (~traceVal);
      break;
    case TestOrd::Toggle:
      currentTraceVal ^= traceVal;
      break;
    case TestOrd::KeepUnchanged:
      // Do nothing
      break;
    }
    globalSignalLoggers.setTrace(currentTraceVal);
  }
  
  {
    /**
     * Process Log command
     */
    TestOrd::SignalLoggerSpecification logSpec;
    BlockNumber bno;
    unsigned int loggers = testOrd->getNoOfSignalLoggerCommands();
    
    if(loggers == (unsigned)~0){ // Apply command to all blocks
      testOrd->getSignalLoggerCommand(0, bno, cmd, logSpec);
      modifySignalLogger(true, bno, cmd, logSpec);
    } else {
      for(unsigned int i = 0; i<loggers; i++){
        testOrd->getSignalLoggerCommand(i, bno, cmd, logSpec);
        modifySignalLogger(false, bno, cmd, logSpec);
      }
    }
  }

  {
    /**
     * Process test command
     */
    testOrd->getTestCommand(cmd);
    switch(cmd){
    case TestOrd::On:{
      SET_GLOBAL_TEST_ON;
    }
    break;
    case TestOrd::Off:{
      SET_GLOBAL_TEST_OFF;
    }
    break;
    case TestOrd::Toggle:{
      TOGGLE_GLOBAL_TEST_FLAG;
    }
    break;
    case TestOrd::KeepUnchanged:
      // Do nothing
      break;
    }
  }

#endif
}

void Cmvmi::execSTATISTICS_REQ(Signal* signal) 
{
  // TODO Note ! This is only a test implementation...

  static int stat1 = 0;
  jamEntry();

  //ndbout << "data 1: " << signal->theData[1];

  int x = signal->theData[0];
  stat1++;
  signal->theData[0] = stat1;
  sendSignal(x, GSN_STATISTICS_CONF, signal, 7, JBB);

}//execSTATISTICS_REQ()



void Cmvmi::execSTOP_ORD(Signal* signal) 
{
  jamEntry();
  globalData.theRestartFlag = perform_stop;
}//execSTOP_ORD()

void
Cmvmi::execSTART_ORD(Signal* signal) {

  StartOrd * const startOrd = (StartOrd *)&signal->theData[0];
  jamEntry();
  
  Uint32 tmp = startOrd->restartInfo;
  if(StopReq::getPerformRestart(tmp)){
    jam();
    /**
     *
     */
    NdbRestartType type = NRT_Default;
    if(StopReq::getNoStart(tmp) && StopReq::getInitialStart(tmp))
      type = NRT_NoStart_InitialStart;
    if(StopReq::getNoStart(tmp) && !StopReq::getInitialStart(tmp))
      type = NRT_NoStart_Restart;
    if(!StopReq::getNoStart(tmp) && StopReq::getInitialStart(tmp))
      type = NRT_DoStart_InitialStart;
    if(!StopReq::getNoStart(tmp)&&!StopReq::getInitialStart(tmp))
      type = NRT_DoStart_Restart;
    NdbShutdown(NST_Restart, type);
  }

  if(globalData.theRestartFlag == system_started){
    jam()
    /**
     * START_ORD received when already started(ignored)
     */
    //ndbout << "START_ORD received when already started(ignored)" << endl;
    return;
  }
  
  if(globalData.theRestartFlag == perform_stop){
    jam()
    /**
     * START_ORD received when stopping(ignored)
     */
    //ndbout << "START_ORD received when stopping(ignored)" << endl;
    return;
  }
  
  if(globalData.theStartLevel == NodeState::SL_NOTHING){
    jam();
    globalData.theStartLevel = NodeState::SL_CMVMI;
    /**
     * Open connections to management servers
     */
    for(unsigned int i = 1; i < MAX_NODES; i++ ){
      if (getNodeInfo(i).m_type == NodeInfo::MGM){ 
        if(!globalTransporterRegistry.is_connected(i)){
          globalTransporterRegistry.do_connect(i);
          globalTransporterRegistry.setIOState(i, NoHalt);
        }
      }
    }
    return ;
  }
  
  if(globalData.theStartLevel == NodeState::SL_CMVMI){
    jam();
    globalData.theStartLevel  = NodeState::SL_STARTING;
    globalData.theRestartFlag = system_started;
    /**
     * StartLevel 1
     *
     * Do Restart
     */

    globalScheduler.clear();
    globalTimeQueue.clear();
    
    // Disconnect all nodes as part of the system restart. 
    // We need to ensure that we are starting up
    // without any connected nodes.   
    for(unsigned int i = 1; i < MAX_NODES; i++ ){
      if (i != getOwnNodeId() && getNodeInfo(i).m_type != NodeInfo::MGM){
        globalTransporterRegistry.do_disconnect(i);
        globalTransporterRegistry.setIOState(i, HaltIO);
      }
    }
    
    /**
     * Start running startphases
     */
    sendSignal(NDBCNTR_REF, GSN_START_ORD, signal, 1, JBA);  
    return;
  }
}//execSTART_ORD()

void Cmvmi::execTAMPER_ORD(Signal* signal) 
{
  jamEntry();
  // TODO We should maybe introduce a CONF and REF signal
  // to be able to indicate if we really introduced an error.
#ifdef ERROR_INSERT
  TamperOrd* const tamperOrd = (TamperOrd*)&signal->theData[0];
  
  signal->theData[1] = tamperOrd->errorNo;
  signal->theData[0] = 5;
  sendSignal(DBDIH_REF, GSN_DIHNDBTAMPER, signal, 3,JBB);
#endif

}//execTAMPER_ORD()



void Cmvmi::execSET_VAR_REQ(Signal* signal) 
{
#if 0

  SetVarReq* const setVarReq = (SetVarReq*)&signal->theData[0];
  ConfigParamId var = setVarReq->variable();
  jamEntry();
  switch (var) {
    
    // NDBCNTR_REF
    
    // DBTC
  case TransactionDeadlockDetectionTimeout:
  case TransactionInactiveTime:
  case NoOfConcurrentProcessesHandleTakeover:
    sendSignal(DBTC_REF, GSN_SET_VAR_REQ, signal, 3, JBB);
    break;
    
    // DBDIH
  case TimeBetweenLocalCheckpoints:
  case TimeBetweenGlobalCheckpoints:
    sendSignal(DBDIH_REF, GSN_SET_VAR_REQ, signal, 3, JBB);
    break;

    // DBLQH
  case NoOfConcurrentCheckpointsDuringRestart:
  case NoOfConcurrentCheckpointsAfterRestart:
    sendSignal(DBLQH_REF, GSN_SET_VAR_REQ, signal, 3, JBB);
    break;

    // DBACC
  case NoOfDiskPagesToDiskDuringRestartACC:
  case NoOfDiskPagesToDiskAfterRestartACC:
    sendSignal(DBACC_REF, GSN_SET_VAR_REQ, signal, 3, JBB);
    break;

    // DBTUP
  case NoOfDiskPagesToDiskDuringRestartTUP:
  case NoOfDiskPagesToDiskAfterRestartTUP:
    sendSignal(DBTUP_REF, GSN_SET_VAR_REQ, signal, 3, JBB);
    break;

    // DBDICT

    // NDBCNTR
  case TimeToWaitAlive:

    // QMGR
  case HeartbeatIntervalDbDb: // TODO ev till Ndbcnt också
  case HeartbeatIntervalDbApi:
  case ArbitTimeout:
    sendSignal(QMGR_REF, GSN_SET_VAR_REQ, signal, 3, JBB);
    break;

    // NDBFS

    // CMVMI
  case MaxNoOfSavedMessages:
  case LockPagesInMainMemory:
  case TimeBetweenWatchDogCheck:
  case StopOnError:
    handleSET_VAR_REQ(signal);
    break;


    // Not possible to update (this could of course be handled by each block
    // instead but I havn't investigated where they belong)
  case Id:
  case ExecuteOnComputer:
  case ShmKey:
  case MaxNoOfConcurrentOperations:
  case MaxNoOfConcurrentTransactions:
  case MemorySpaceIndexes:
  case MemorySpaceTuples:
  case MemoryDiskPages:
  case NoOfFreeDiskClusters:
  case NoOfDiskClusters:
  case NoOfFragmentLogFiles:
  case NoOfDiskClustersPerDiskFile:
  case NoOfDiskFiles:
  case MaxNoOfSavedEvents:
  default:

    int mgmtSrvr = setVarReq->mgmtSrvrBlockRef();
    sendSignal(mgmtSrvr, GSN_SET_VAR_REF, signal, 0, JBB);
  } // switch

#endif
}//execSET_VAR_REQ()


void Cmvmi::execSET_VAR_CONF(Signal* signal) 
{
  int mgmtSrvr = signal->theData[0];
  sendSignal(mgmtSrvr, GSN_SET_VAR_CONF, signal, 0, JBB);

}//execSET_VAR_CONF()


void Cmvmi::execSET_VAR_REF(Signal* signal) 
{
  int mgmtSrvr = signal->theData[0];
  sendSignal(mgmtSrvr, GSN_SET_VAR_REF, signal, 0, JBB);

}//execSET_VAR_REF()


void Cmvmi::handleSET_VAR_REQ(Signal* signal) {
#if 0
  SetVarReq* const setVarReq = (SetVarReq*)&signal->theData[0];
  ConfigParamId var = setVarReq->variable();
  int val = setVarReq->value();

  switch (var) {
  case MaxNoOfSavedMessages:
    theConfig.maxNoOfErrorLogs(val);
    sendSignal(CMVMI_REF, GSN_SET_VAR_CONF, signal, 1, JBB);
    break;
    
  case LockPagesInMainMemory:
    int result;
    if (val == 0) {
      result = NdbMem_MemUnlockAll();
    }
    else {
      result = NdbMem_MemLockAll();
    }
    if (result == 0) {
      sendSignal(CMVMI_REF, GSN_SET_VAR_CONF, signal, 1, JBB);
    }
    else {
      sendSignal(CMVMI_REF, GSN_SET_VAR_REF, signal, 1, JBB);
    }
    break;

  case TimeBetweenWatchDogCheck:
    theConfig.timeBetweenWatchDogCheck(val);
    sendSignal(CMVMI_REF, GSN_SET_VAR_CONF, signal, 1, JBB);
    break;

  case StopOnError:
    theConfig.stopOnError(val);
    sendSignal(CMVMI_REF, GSN_SET_VAR_CONF, signal, 1, JBB);
    break;
    
  default:
    sendSignal(CMVMI_REF, GSN_SET_VAR_REF, signal, 1, JBB);
    return;
  } // switch
#endif
}

#ifdef VM_TRACE
class RefSignalTest {
public:
  enum ErrorCode {
    OK = 0,
    NF_FakeErrorREF = 7
  };
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 errorCode;
};
#endif


static int iii;

static
int
recurse(char * buf, int loops, int arg){
  char * tmp = (char*)alloca(arg);
  printf("tmp = %p\n", tmp);
  for(iii = 0; iii<arg; iii += 1024){
    tmp[iii] = (iii % 23 + (arg & iii));
  }
  
  if(loops == 0)
    return tmp[345];
  else
    return tmp[arg/loops] + recurse(tmp, loops - 1, arg);
}

void
Cmvmi::execDUMP_STATE_ORD(Signal* signal)
{

  sendSignal(QMGR_REF, GSN_DUMP_STATE_ORD,    signal, signal->length(), JBB);
  sendSignal(NDBCNTR_REF, GSN_DUMP_STATE_ORD, signal, signal->length(), JBB);
  sendSignal(DBTC_REF, GSN_DUMP_STATE_ORD,    signal, signal->length(), JBB);
  sendSignal(DBDIH_REF, GSN_DUMP_STATE_ORD,   signal, signal->length(), JBB);
  sendSignal(DBDICT_REF, GSN_DUMP_STATE_ORD,  signal, signal->length(), JBB);
  sendSignal(DBLQH_REF, GSN_DUMP_STATE_ORD,   signal, signal->length(), JBB);
  sendSignal(DBTUP_REF, GSN_DUMP_STATE_ORD,   signal, signal->length(), JBB);
  sendSignal(DBACC_REF, GSN_DUMP_STATE_ORD,   signal, signal->length(), JBB);
  sendSignal(NDBFS_REF, GSN_DUMP_STATE_ORD,   signal, signal->length(), JBB);
  sendSignal(BACKUP_REF, GSN_DUMP_STATE_ORD,  signal, signal->length(), JBB);
  sendSignal(DBUTIL_REF, GSN_DUMP_STATE_ORD,  signal, signal->length(), JBB);
  sendSignal(SUMA_REF, GSN_DUMP_STATE_ORD,    signal, signal->length(), JBB);
  sendSignal(GREP_REF, GSN_DUMP_STATE_ORD,    signal, signal->length(), JBB);
  sendSignal(TRIX_REF, GSN_DUMP_STATE_ORD,    signal, signal->length(), JBB);
  sendSignal(DBTUX_REF, GSN_DUMP_STATE_ORD,   signal, signal->length(), JBB);
  
  /**
   *
   * Here I can dump CMVMI state if needed
   */
  if(signal->theData[0] == 13){
#if 0
    int loop = 100;
    int len = (10*1024*1024);
    if(signal->getLength() > 1)
      loop = signal->theData[1];
    if(signal->getLength() > 2)
      len = signal->theData[2];
    
    ndbout_c("recurse(%d loop, %dkb per recurse)", loop, len/1024);
    int a = recurse(0, loop, len);
    ndbout_c("after...%d", a);
#endif
  }

  DumpStateOrd * const & dumpState = (DumpStateOrd *)&signal->theData[0];
  if (dumpState->args[0] == DumpStateOrd::CmvmiDumpConnections){
    for(unsigned int i = 1; i < MAX_NODES; i++ ){
      const char* nodeTypeStr = "";
      switch(getNodeInfo(i).m_type){
      case NodeInfo::DB:
	nodeTypeStr = "DB";
	break;
      case NodeInfo::API:
	nodeTypeStr = "API";
	break;
      case NodeInfo::MGM:
	nodeTypeStr = "MGM";
	break;
      case NodeInfo::REP:
	nodeTypeStr = "REP";
	break;
      case NodeInfo::INVALID:
	nodeTypeStr = 0;
	break;
      default:
	nodeTypeStr = "<UNKNOWN>";
      }

      if(nodeTypeStr == 0)
	continue;

      infoEvent("Connection to %d (%s) %s", 
                i, 
                nodeTypeStr,
                globalTransporterRegistry.getPerformStateString(i));
    }
  }
  
  if (dumpState->args[0] == DumpStateOrd::CmvmiDumpLongSignalMemory){
    infoEvent("Cmvmi: g_sectionSegmentPool size: %d free: %d",
	      g_sectionSegmentPool.getSize(),
	      g_sectionSegmentPool.getNoOfFree());
  }
  
  if (dumpState->args[0] == DumpStateOrd::CmvmiSetRestartOnErrorInsert){
    if(signal->getLength() == 1)
      theConfig.setRestartOnErrorInsert((int)NRT_NoStart_Restart);
    else
      theConfig.setRestartOnErrorInsert(signal->theData[1]);
  }

  if (dumpState->args[0] == DumpStateOrd::CmvmiTestLongSigWithDelay) {
    unsigned i;
    Uint32 loopCount = dumpState->args[1];
    const unsigned len0 = 11;
    const unsigned len1 = 123;
    Uint32 sec0[len0];
    Uint32 sec1[len1];
    for (i = 0; i < len0; i++)
      sec0[i] = i;
    for (i = 0; i < len1; i++)
      sec1[i] = 16 * i;
    Uint32* sig = signal->getDataPtrSend();
    sig[0] = reference();
    sig[1] = 20; // test type
    sig[2] = 0;
    sig[3] = 0;
    sig[4] = loopCount;
    sig[5] = len0;
    sig[6] = len1;
    sig[7] = 0;
    LinearSectionPtr ptr[3];
    ptr[0].p = sec0;
    ptr[0].sz = len0;
    ptr[1].p = sec1;
    ptr[1].sz = len1;
    sendSignal(reference(), GSN_TESTSIG, signal, 8, JBB, ptr, 2);
  }

#ifdef VM_TRACE
#if 0
  {
    SafeCounterManager mgr(* this); mgr.setSize(1);
    SafeCounterHandle handle;

    {
      SafeCounter tmp(mgr, handle);
      tmp.init<RefSignalTest>(CMVMI, GSN_TESTSIG, /* senderData */ 13);
      tmp.setWaitingFor(3);
      ndbrequire(!tmp.done());
      ndbout_c("Allocted");
    }
    ndbrequire(!handle.done());
    {
      SafeCounter tmp(mgr, handle);
      tmp.clearWaitingFor(3);
      ndbrequire(tmp.done());
      ndbout_c("Deallocted");
    }
    ndbrequire(handle.done());
  }
#endif
#endif
}//Cmvmi::execDUMP_STATE_ORD()


BLOCK_FUNCTIONS(Cmvmi);

static Uint32 g_print;
static LinearSectionPtr g_test[3];

void
Cmvmi::execTESTSIG(Signal* signal){
  Uint32 i;
  /**
   * Test of SafeCounter
   */
  jamEntry();

  if(!assembleFragments(signal)){
    jam();
    return;
  }

  Uint32 ref = signal->theData[0];
  Uint32 testType = signal->theData[1];
  Uint32 fragmentLength = signal->theData[2];
  g_print = signal->theData[3];
//  Uint32 returnCount = signal->theData[4];
  Uint32 * secSizes = &signal->theData[5];
  
  if(g_print){
    SignalLoggerManager::printSignalHeader(stdout, 
					   signal->header,
					   0,
					   getOwnNodeId(),
					   true);
    ndbout_c("-- Fixed section --");    
    for(i = 0; i<signal->length(); i++){
      fprintf(stdout, "H'0x%.8x ", signal->theData[i]);
      if(((i + 1) % 6) == 0)
	fprintf(stdout, "\n");
    }
    fprintf(stdout, "\n");
    
    for(i = 0; i<signal->header.m_noOfSections; i++){
      SegmentedSectionPtr ptr;
      ndbout_c("-- Section %d --", i);
      signal->getSection(ptr, i);
      ndbrequire(ptr.p != 0);
      print(ptr, stdout);
      ndbrequire(ptr.sz == secSizes[i]);
    }
  }

  /**
   * Validate length:s
   */
  for(i = 0; i<signal->header.m_noOfSections; i++){
    SegmentedSectionPtr ptr;
    signal->getSection(ptr, i);
    ndbrequire(ptr.p != 0);
    ndbrequire(ptr.sz == secSizes[i]);
  }

  /**
   * Testing send with delay.
   */
  if (testType == 20) {
    if (signal->theData[4] == 0) {
      releaseSections(signal);
      return;
    }
    signal->theData[4]--;
    sendSignalWithDelay(reference(), GSN_TESTSIG, signal, 100, 8);
    return;
  }
  
  NodeReceiverGroup rg(CMVMI, c_dbNodes);

  if(signal->getSendersBlockRef() == ref){
    /**
     * Signal from API (not via NodeReceiverGroup)
     */
    if((testType % 2) == 1){
      signal->theData[4] = 1;
    } else {
      signal->theData[1] --;
      signal->theData[4] = rg.m_nodes.count();
    }
  } 
  
  switch(testType){
  case 1:
    sendSignal(ref, GSN_TESTSIG,  signal, signal->length(), JBB);      
    break;
  case 2:
    sendSignal(rg, GSN_TESTSIG,  signal, signal->length(), JBB);
    break;
  case 3:
  case 4:{
    LinearSectionPtr ptr[3];
    const Uint32 secs = signal->getNoOfSections();
    for(i = 0; i<secs; i++){
      SegmentedSectionPtr sptr;
      signal->getSection(sptr, i);
      ptr[i].sz = sptr.sz;
      ptr[i].p = new Uint32[sptr.sz];
      copy(ptr[i].p, sptr);
    }
    
    if(testType == 3){
      sendSignal(ref, GSN_TESTSIG,  signal, signal->length(), JBB, ptr, secs); 
    } else {
      sendSignal(rg, GSN_TESTSIG,  signal, signal->length(), JBB, ptr, secs); 
    }
    for(Uint32 i = 0; i<secs; i++){
      delete[] ptr[i].p;
    }
    break;
  }
  case 5:
  case 6:{
    
    NodeReceiverGroup tmp;
    if(testType == 5){
      tmp  = ref;
    } else {
      tmp = rg;
    }
    
    FragmentSendInfo fragSend;
    sendFirstFragment(fragSend,
		      tmp,
		      GSN_TESTSIG,
		      signal,
		      signal->length(),
		      JBB,
		      fragmentLength);
    int count = 1;
    while(fragSend.m_status != FragmentSendInfo::SendComplete){
      count++;
      if(g_print)
	ndbout_c("Sending fragment %d", count);
      sendNextSegmentedFragment(signal, fragSend);
    }
    break;
  }
  case 7:
  case 8:{
    LinearSectionPtr ptr[3];
    const Uint32 secs = signal->getNoOfSections();
    for(i = 0; i<secs; i++){
      SegmentedSectionPtr sptr;
      signal->getSection(sptr, i);
      ptr[i].sz = sptr.sz;
      ptr[i].p = new Uint32[sptr.sz];
      copy(ptr[i].p, sptr);
    }

    NodeReceiverGroup tmp;
    if(testType == 7){
      tmp  = ref;
    } else {
      tmp = rg;
    }

    FragmentSendInfo fragSend;
    sendFirstFragment(fragSend,
		      tmp,
		      GSN_TESTSIG,
		      signal,
		      signal->length(),
		      JBB,
		      ptr,
		      secs,
		      fragmentLength);
    
    int count = 1;
    while(fragSend.m_status != FragmentSendInfo::SendComplete){
      count++;
      if(g_print)
	ndbout_c("Sending fragment %d", count);
      sendNextLinearFragment(signal, fragSend);
    }
    
    for(i = 0; i<secs; i++){
      delete[] ptr[i].p;
    }
    break;
  }
  case 9:
  case 10:{

    Callback m_callBack;
    m_callBack.m_callbackFunction = 
      safe_cast(&Cmvmi::sendFragmentedComplete);
    
    if(testType == 9){
      m_callBack.m_callbackData = 9;
      sendFragmentedSignal(ref,
			   GSN_TESTSIG, signal, signal->length(), JBB, 
			   m_callBack,
			   fragmentLength);
    } else {
      m_callBack.m_callbackData = 10;
      sendFragmentedSignal(rg,
			   GSN_TESTSIG, signal, signal->length(), JBB, 
			   m_callBack,
			   fragmentLength);
    }
    break;
  }
  case 11:
  case 12:{

    const Uint32 secs = signal->getNoOfSections();
    memset(g_test, 0, sizeof(g_test));
    for(i = 0; i<secs; i++){
      SegmentedSectionPtr sptr;
      signal->getSection(sptr, i);
      g_test[i].sz = sptr.sz;
      g_test[i].p = new Uint32[sptr.sz];
      copy(g_test[i].p, sptr);
    }
    
    
    Callback m_callBack;
    m_callBack.m_callbackFunction = 
      safe_cast(&Cmvmi::sendFragmentedComplete);
    
    if(testType == 11){
      m_callBack.m_callbackData = 11;
      sendFragmentedSignal(ref,
			   GSN_TESTSIG, signal, signal->length(), JBB, 
			   g_test, secs,
			   m_callBack,
			   fragmentLength);
    } else {
      m_callBack.m_callbackData = 12;
      sendFragmentedSignal(rg,
			   GSN_TESTSIG, signal, signal->length(), JBB, 
			   g_test, secs,
			   m_callBack,
			   fragmentLength);
    }
    break;
  }
  case 13:{
    ndbrequire(signal->getNoOfSections() == 0);
    Uint32 loop = signal->theData[9];
    if(loop > 0){
      signal->theData[9] --;
      sendSignal(CMVMI_REF, GSN_TESTSIG, signal, signal->length(), JBB);
      return;
    }
    sendSignal(ref, GSN_TESTSIG, signal, signal->length(), JBB);
    return;
  }
  case 14:{
    Uint32 count = signal->theData[8];
    signal->theData[10] = count * rg.m_nodes.count();
    for(i = 0; i<count; i++){
      sendSignal(rg, GSN_TESTSIG, signal, signal->length(), JBB); 
    }
    return;
  }

  default:
    ndbrequire(false);
  }
  return;
}

void
Cmvmi::sendFragmentedComplete(Signal* signal, Uint32 data, Uint32 returnCode){
  if(g_print)
    ndbout_c("sendFragmentedComplete: %d", data);
  if(data == 11 || data == 12){
    for(Uint32 i = 0; i<3; i++){
      if(g_test[i].p != 0)
	delete[] g_test[i].p;
    }
  }
}
