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

#include "Backup.hpp"

#include <ndb_version.h>

#include <NdbTCP.h>
#include <Bitmask.hpp>

#include <signaldata/NodeFailRep.hpp>
#include <signaldata/ReadNodesConf.hpp>

#include <signaldata/ScanFrag.hpp>

#include <signaldata/GetTabInfo.hpp>
#include <signaldata/DictTabInfo.hpp>
#include <signaldata/ListTables.hpp>

#include <signaldata/FsOpenReq.hpp>
#include <signaldata/FsAppendReq.hpp>
#include <signaldata/FsCloseReq.hpp>
#include <signaldata/FsConf.hpp>
#include <signaldata/FsRef.hpp>
#include <signaldata/FsRemoveReq.hpp>

#include <signaldata/BackupImpl.hpp>
#include <signaldata/BackupSignalData.hpp>
#include <signaldata/BackupContinueB.hpp>
#include <signaldata/EventReport.hpp>

#include <signaldata/UtilSequence.hpp>

#include <signaldata/CreateTrig.hpp>
#include <signaldata/AlterTrig.hpp>
#include <signaldata/DropTrig.hpp>
#include <signaldata/FireTrigOrd.hpp>
#include <signaldata/TrigAttrInfo.hpp>
#include <AttributeHeader.hpp>

#include <signaldata/WaitGCP.hpp>

#include <NdbTick.h>

static NDB_TICKS startTime;

static const Uint32 BACKUP_SEQUENCE = 0x1F000000;

#ifdef VM_TRACE
#define DEBUG_OUT(x) ndbout << x << endl
#else
#define DEBUG_OUT(x) 
#endif

//#define DEBUG_ABORT

//---------------------------------------------------------
// Ignore this since a completed abort could have preceded
// this message.
//---------------------------------------------------------
#define slaveAbortCheck() \
if ((ptr.p->backupId != backupId) || \
    (ptr.p->slaveState.getState() == ABORTING)) { \
  jam(); \
  return; \
}

#define masterAbortCheck() \
if ((ptr.p->backupId != backupId) || \
    (ptr.p->masterData.state.getState() == ABORTING)) { \
  jam(); \
  return; \
}

#define defineSlaveAbortCheck() \
  if (ptr.p->slaveState.getState() == ABORTING) { \
    jam(); \
    closeFiles(signal, ptr); \
    return; \
  }

static Uint32 g_TypeOfStart = NodeState::ST_ILLEGAL_TYPE;

void
Backup::execSTTOR(Signal* signal) 
{
  jamEntry();                            

  const Uint32 startphase  = signal->theData[1];
  const Uint32 typeOfStart = signal->theData[7];

  if (startphase == 3) {
    jam();
    g_TypeOfStart = typeOfStart;
    signal->theData[0] = reference();
    sendSignal(NDBCNTR_REF, GSN_READ_NODESREQ, signal, 1, JBB);
    return;
  }//if

  if(startphase == 7 && g_TypeOfStart == NodeState::ST_INITIAL_START &&
     c_masterNodeId == getOwnNodeId()){
    jam();
    createSequence(signal);
    return;
  }//if
  
  sendSTTORRY(signal);  
  return;
}//Dbdict::execSTTOR()

void
Backup::execREAD_NODESCONF(Signal* signal)
{
  jamEntry();
  ReadNodesConf * conf = (ReadNodesConf *)signal->getDataPtr();
 
  c_aliveNodes.clear();

  Uint32 count = 0;
  for (Uint32 i = 0; i<MAX_NDB_NODES; i++) {
    jam();
    if(NodeBitmask::get(conf->allNodes, i)){
      jam();
      count++;

      NodePtr node;
      ndbrequire(c_nodes.seize(node));
      
      node.p->nodeId = i;
      if(NodeBitmask::get(conf->inactiveNodes, i)) {
        jam();
	node.p->alive = 0;
      } else {
        jam();
	node.p->alive = 1;
	c_aliveNodes.set(i);
      }//if
    }//if
  }//for
  c_masterNodeId = conf->masterNodeId;
  ndbrequire(count == conf->noOfNodes);
  sendSTTORRY(signal);
}

void
Backup::sendSTTORRY(Signal* signal)
{
  signal->theData[0] = 0;
  signal->theData[3] = 1;
  signal->theData[4] = 3;
  signal->theData[5] = 7;
  signal->theData[6] = 255; // No more start phases from missra
  sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 7, JBB);
}

void
Backup::createSequence(Signal* signal)
{
  UtilSequenceReq * req = (UtilSequenceReq*)signal->getDataPtrSend();
  
  req->senderData  = RNIL;
  req->sequenceId  = BACKUP_SEQUENCE;
  req->requestType = UtilSequenceReq::Create;
  
  sendSignal(DBUTIL_REF, GSN_UTIL_SEQUENCE_REQ, 
	     signal, UtilSequenceReq::SignalLength, JBB);
}

void
Backup::execCONTINUEB(Signal* signal)
{
  jamEntry();
  const Uint32 Tdata0 = signal->theData[0];
  const Uint32 Tdata1 = signal->theData[1];
  const Uint32 Tdata2 = signal->theData[2];
  
  switch(Tdata0) {
  case BackupContinueB::START_FILE_THREAD:
  case BackupContinueB::BUFFER_UNDERFLOW:
  {
    jam();
    BackupFilePtr filePtr;
    c_backupFilePool.getPtr(filePtr, Tdata1);
    checkFile(signal, filePtr);
    return;
  }
  break;
  case BackupContinueB::BUFFER_FULL_SCAN:
  {
    jam();
    BackupFilePtr filePtr;
    c_backupFilePool.getPtr(filePtr, Tdata1);
    checkScan(signal, filePtr);
    return;
  }
  break;
  case BackupContinueB::BUFFER_FULL_FRAG_COMPLETE:
  {
    jam();
    BackupFilePtr filePtr;
    c_backupFilePool.getPtr(filePtr, Tdata1);
    fragmentCompleted(signal, filePtr);
    return;
  }
  break;
  case BackupContinueB::BUFFER_FULL_META:
  {
    jam();
    BackupRecordPtr ptr;
    c_backupPool.getPtr(ptr, Tdata1);

    if (ptr.p->slaveState.getState() == ABORTING) {
      jam();
      closeFiles(signal, ptr);
      return;
    }//if
    BackupFilePtr filePtr;
    ptr.p->files.getPtr(filePtr, ptr.p->ctlFilePtr);
    FsBuffer & buf = filePtr.p->operation.dataBuffer;
    
    if(buf.getFreeSize() + buf.getMinRead() < buf.getUsableSize()) {
      jam();
      TablePtr tabPtr;
      c_tablePool.getPtr(tabPtr, Tdata2);
      
      DEBUG_OUT("Backup - Buffer full - " << buf.getFreeSize()
		<< " + " << buf.getMinRead()
		<< " < " << buf.getUsableSize()
		<< " - tableId = " << tabPtr.p->tableId);

      signal->theData[0] = BackupContinueB::BUFFER_FULL_META;
      signal->theData[1] = Tdata1;
      signal->theData[2] = Tdata2;
      sendSignalWithDelay(BACKUP_REF, GSN_CONTINUEB, signal, 100, 3);
      return;
    }//if
    
    TablePtr tabPtr;
    c_tablePool.getPtr(tabPtr, Tdata2);
    GetTabInfoReq * req = (GetTabInfoReq *)signal->getDataPtrSend();
    req->senderRef = reference();
    req->senderData = ptr.i;
    req->requestType = GetTabInfoReq::RequestById |
      GetTabInfoReq::LongSignalConf;
    req->tableId = tabPtr.p->tableId;
    sendSignal(DBDICT_REF, GSN_GET_TABINFOREQ, signal, 
	       GetTabInfoReq::SignalLength, JBB);
    return;
  }
  default:
    ndbrequire(0);
  }//switch
}

void
Backup::execDUMP_STATE_ORD(Signal* signal)
{
  jamEntry();
  
  if(signal->theData[0] == 20){
    if(signal->length() > 1){
      c_defaults.m_dataBufferSize = (signal->theData[1] * 1024 * 1024);
    }
    if(signal->length() > 2){
      c_defaults.m_logBufferSize = (signal->theData[2] * 1024 * 1024);
    }
    if(signal->length() > 3){
      c_defaults.m_minWriteSize = signal->theData[3] * 1024;
    }
    if(signal->length() > 4){
      c_defaults.m_maxWriteSize = signal->theData[4] * 1024;
    }
    
    infoEvent("Backup: data: %d log: %d min: %d max: %d",
	      c_defaults.m_dataBufferSize,
	      c_defaults.m_logBufferSize,
	      c_defaults.m_minWriteSize,
	      c_defaults.m_maxWriteSize);
    return;
  }
  if(signal->theData[0] == 21){
    BackupReq * req = (BackupReq*)signal->getDataPtrSend();
    req->senderData = 23;
    req->backupDataLen = 0;
    sendSignal(BACKUP_REF, GSN_BACKUP_REQ,signal,BackupReq::SignalLength, JBB);
    startTime = NdbTick_CurrentMillisecond();
    return;
  }

  if(signal->theData[0] == 22){
    const Uint32 seq = signal->theData[1];
    FsRemoveReq * req = (FsRemoveReq *)signal->getDataPtrSend();
    req->userReference = reference();
    req->userPointer = 23;
    req->directory = 1;
    req->ownDirectory = 1;
    FsOpenReq::setVersion(req->fileNumber, 2);
    FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_CTL);
    FsOpenReq::v2_setSequence(req->fileNumber, seq);
    FsOpenReq::v2_setNodeId(req->fileNumber, getOwnNodeId());
    sendSignal(NDBFS_REF, GSN_FSREMOVEREQ, signal, 
	       FsRemoveReq::SignalLength, JBA);
    return;
  }

  if(signal->theData[0] == 23){
    /**
     * Print records
     */
    BackupRecordPtr ptr;
    for(c_backups.first(ptr); ptr.i != RNIL; c_backups.next(ptr)){
      infoEvent("BackupRecord %d: BackupId: %d MasterRef: %x ClientRef: %x",
		ptr.i, ptr.p->backupId, ptr.p->masterRef, ptr.p->clientRef);
      if(ptr.p->masterRef == reference()){
	infoEvent(" MasterState: %d State: %d",
		  ptr.p->masterData.state.getState(),
		  ptr.p->slaveState.getState());
      } else {
	infoEvent(" State: %d", ptr.p->slaveState.getState());
      }
      BackupFilePtr filePtr;
      for(ptr.p->files.first(filePtr); filePtr.i != RNIL; 
	  ptr.p->files.next(filePtr)){
	jam();
	infoEvent(" file %d: type: %d open: %d running: %d done: %d scan: %d",
		  filePtr.i, filePtr.p->fileType, filePtr.p->fileOpened,
		  filePtr.p->fileRunning, 
		  filePtr.p->fileDone, filePtr.p->scanRunning);
      }
    }
  }
  if(signal->theData[0] == 24){
    /**
     * Print size of records etc.
     */
    infoEvent("Backup - dump pool sizes");
    infoEvent("BackupPool: %d BackupFilePool: %d TablePool: %d",
	      c_backupPool.getSize(), c_backupFilePool.getSize(), 
	      c_tablePool.getSize());
    infoEvent("AttrPool: %d TriggerPool: %d FragmentPool: %d",
	      c_backupPool.getSize(), c_backupFilePool.getSize(), 
	      c_tablePool.getSize());
    infoEvent("PagePool: %d",
	      c_pagePool.getSize());

  }
}

bool
Backup::findTable(const BackupRecordPtr & ptr, 
		  TablePtr & tabPtr, Uint32 tableId) const
{
  for(ptr.p->tables.first(tabPtr); 
      tabPtr.i != RNIL; 
      ptr.p->tables.next(tabPtr)) {
    jam();
    if(tabPtr.p->tableId == tableId){
      jam();
      return true;
    }//if
  }//for
  tabPtr.i = RNIL;
  tabPtr.p = 0;
  return false;
}

static Uint32 xps(Uint32 x, Uint64 ms)
{
  float fx = x;
  float fs = ms;
  
  if(ms == 0 || x == 0) {
    jam();
    return 0;
  }//if
  jam();
  return ((Uint32)(1000.0f * (fx + fs/2.1f))) / ((Uint32)fs);
}

struct Number {
  Number(Uint32 r) { val = r;}
  Number & operator=(Uint32 r) { val = r; return * this; }
  Uint32 val;
};

NdbOut &
operator<< (NdbOut & out, const Number & val){
  char p = 0;
  Uint32 loop = 1;
  while(val.val > loop){
    loop *= 1000;
    p += 3;
  }
  if(loop != 1){
    p -= 3;
    loop /= 1000;
  }

  switch(p){
  case 0:
    break;
  case 3:
    p = 'k';
    break;
  case 6:
    p = 'M';
    break;
  case 9:
    p = 'G';
    break;
  default:
    p = 0;
  }
  char str[2];
  str[0] = p;
  str[1] = 0;
  Uint32 tmp = (val.val + (loop >> 1)) / loop;
#if 1
  if(p > 0)
    out << tmp << str;
  else
    out << tmp;
#else
  out << val.val;
#endif

  return out;
}

void
Backup::execBACKUP_CONF(Signal* signal)
{
  jamEntry();
  BackupConf * conf = (BackupConf*)signal->getDataPtr();
  
  ndbout_c("Backup %d has started", conf->backupId);
}

void
Backup::execBACKUP_REF(Signal* signal)
{
  jamEntry();
  BackupRef * ref = (BackupRef*)signal->getDataPtr();

  ndbout_c("Backup (%d) has NOT started %d", ref->senderData, ref->errorCode);
}

void
Backup::execBACKUP_COMPLETE_REP(Signal* signal)
{
  jamEntry();
  BackupCompleteRep* rep = (BackupCompleteRep*)signal->getDataPtr();
 
  startTime = NdbTick_CurrentMillisecond() - startTime;
  
  ndbout_c("Backup %d has completed", rep->backupId);
  const Uint32 bytes = rep->noOfBytes;
  const Uint32 records = rep->noOfRecords;

  Number rps = xps(records, startTime);
  Number bps = xps(bytes, startTime);

  ndbout << " Data [ "
	 << Number(records) << " rows " 
	 << Number(bytes) << " bytes " << startTime << " ms ] " 
	 << " => "
	 << rps << " row/s & " << bps << "b/s" << endl;

  bps = xps(rep->noOfLogBytes, startTime);
  rps = xps(rep->noOfLogRecords, startTime);

  ndbout << " Log [ "
	 << Number(rep->noOfLogRecords) << " log records " 
	 << Number(rep->noOfLogBytes) << " bytes " << startTime << " ms ] " 
	 << " => "
	 << rps << " records/s & " << bps << "b/s" << endl;

}

void
Backup::execBACKUP_ABORT_REP(Signal* signal)
{
  jamEntry();
  BackupAbortRep* rep = (BackupAbortRep*)signal->getDataPtr();
  
  ndbout_c("Backup %d has been aborted %d", rep->backupId, rep->reason);
}

const TriggerEvent::Value triggerEventValues[] = {
  TriggerEvent::TE_INSERT,
  TriggerEvent::TE_UPDATE,
  TriggerEvent::TE_DELETE
};

const char* triggerNameFormat[] = {
  "NDB$BACKUP_%d_%d_INSERT",
  "NDB$BACKUP_%d_%d_UPDATE",
  "NDB$BACKUP_%d_%d_DELETE"
};

const Backup::State 
Backup::validMasterTransitions[] = {
  INITIAL,  DEFINING,
  DEFINING, DEFINED,
  DEFINED,  STARTED,
  STARTED,  SCANNING,
  SCANNING, STOPPING,
  STOPPING, INITIAL,

  DEFINING, ABORTING,
  DEFINED,  ABORTING,
  STARTED,  ABORTING,
  SCANNING, ABORTING,
  STOPPING, ABORTING,
  ABORTING, ABORTING,

  DEFINING, INITIAL,
  ABORTING, INITIAL,
  INITIAL,  INITIAL
};

const Backup::State 
Backup::validSlaveTransitions[] = {
  INITIAL,  DEFINING,
  DEFINING, DEFINED,
  DEFINED,  STARTED,
  STARTED,  STARTED, // Several START_BACKUP_REQ is sent
  STARTED,  SCANNING,
  SCANNING, STARTED,
  STARTED,  STOPPING,
  STOPPING, CLEANING,
  CLEANING, INITIAL,
  
  INITIAL,  ABORTING, // Node fail
  DEFINING, ABORTING,
  DEFINED,  ABORTING,
  STARTED,  ABORTING,
  SCANNING, ABORTING,
  STOPPING, ABORTING,
  CLEANING, ABORTING, // Node fail w/ master takeover
  ABORTING, ABORTING, // Slave who initiates ABORT should have this transition
  
  ABORTING, INITIAL,
  INITIAL,  INITIAL
};

const Uint32
Backup::validSlaveTransitionsCount = 
sizeof(Backup::validSlaveTransitions) / sizeof(Backup::State);

const Uint32
Backup::validMasterTransitionsCount = 
sizeof(Backup::validMasterTransitions) / sizeof(Backup::State);

void
Backup::CompoundState::setState(State newState){
  bool found = false;
  const State currState = state;
  for(unsigned i = 0; i<noOfValidTransitions; i+= 2) {
    jam();
    if(validTransitions[i]   == currState &&
       validTransitions[i+1] == newState){
      jam();
      found = true;
      break;
    }
  }
  ndbrequire(found);
  
  if (newState == INITIAL)
    abortState = INITIAL;
  if(newState == ABORTING && currState != ABORTING) {
    jam();
    abortState = currState;
  }
  state = newState;
#ifdef DEBUG_ABORT
  if (newState != currState) {
    ndbout_c("%u: Old state = %u, new state = %u, abort state = %u",
	     id, currState, newState, abortState);
  }
#endif
}

void
Backup::CompoundState::forceState(State newState)
{
  const State currState = state;
  if (newState == INITIAL)
    abortState = INITIAL;
  if(newState == ABORTING && currState != ABORTING) {
    jam();
    abortState = currState;
  }
  state = newState;
#ifdef DEBUG_ABORT
  if (newState != currState) {
    ndbout_c("%u: FORCE: Old state = %u, new state = %u, abort state = %u",
	     id, currState, newState, abortState);
  }
#endif
}

Backup::Table::Table(ArrayPool<Attribute> & ah, 
		     ArrayPool<Fragment> & fh) 
  : attributes(ah), fragments(fh)
{
  triggerIds[0] = ILLEGAL_TRIGGER_ID;
  triggerIds[1] = ILLEGAL_TRIGGER_ID;
  triggerIds[2] = ILLEGAL_TRIGGER_ID;
  triggerAllocated[0] = false;
  triggerAllocated[1] = false;
  triggerAllocated[2] = false;
}

/*****************************************************************************
 * 
 * Node state handling
 *
 *****************************************************************************/
void
Backup::execNODE_FAILREP(Signal* signal)
{
  jamEntry();

  NodeFailRep * rep = (NodeFailRep*)signal->getDataPtr();
  
  bool doStuff = false;
  /*
  Start by saving important signal data which will be destroyed before the
  process is completed.
  */
  NodeId new_master_node_id = rep->masterNodeId;
  Uint32 theFailedNodes[NodeBitmask::Size];
  for (Uint32 i = 0; i < NodeBitmask::Size; i++)
    theFailedNodes[i] = rep->theNodes[i];

//  NodeId old_master_node_id = getMasterNodeId();
  c_masterNodeId = new_master_node_id;

  NodePtr nodePtr;
  for(c_nodes.first(nodePtr); nodePtr.i != RNIL; c_nodes.next(nodePtr)) {
    jam();
    if(NodeBitmask::get(theFailedNodes, nodePtr.p->nodeId)){
      if(nodePtr.p->alive){
	jam();
	ndbrequire(c_aliveNodes.get(nodePtr.p->nodeId));
	doStuff = true;
      } else {
        jam();
	ndbrequire(!c_aliveNodes.get(nodePtr.p->nodeId));
      }//if
      nodePtr.p->alive = 0;
      c_aliveNodes.clear(nodePtr.p->nodeId);
    }//if
  }//for

  if(!doStuff){
    jam();
    return;
  }//if
  
#ifdef DEBUG_ABORT
  ndbout_c("****************** Node fail rep ******************");
#endif

  NodeId newCoordinator = c_masterNodeId;
  BackupRecordPtr ptr;
  for(c_backups.first(ptr); ptr.i != RNIL; c_backups.next(ptr)) {
    jam();
    checkNodeFail(signal, ptr, newCoordinator, theFailedNodes);
  }
}

bool
Backup::verifyNodesAlive(const NdbNodeBitmask& aNodeBitMask)
{
  for (Uint32 i = 0; i < MAX_NDB_NODES; i++) {
    jam();
    if(aNodeBitMask.get(i)) {
      if(!c_aliveNodes.get(i)){
        jam();
        return false;
      }//if
    }//if
  }//for
  return true;
}

void
Backup::checkNodeFail(Signal* signal,
		      BackupRecordPtr ptr,
		      NodeId newCoord,
		      Uint32 theFailedNodes[NodeBitmask::Size])
{
  ndbrequire( ptr.p->nodes.get(newCoord)); /* just to make sure newCoord
					    * is part of the backup
					    */
  /* Update ptr.p->nodes to be up to date with current alive nodes
   */
  NodePtr nodePtr;
  bool found = false;
  for(c_nodes.first(nodePtr); nodePtr.i != RNIL; c_nodes.next(nodePtr)) {
    jam();
    if(NodeBitmask::get(theFailedNodes, nodePtr.p->nodeId)) {
      jam();
      if (ptr.p->nodes.get(nodePtr.p->nodeId)) {
	jam();
	ptr.p->nodes.clear(nodePtr.p->nodeId); 
	found = true;
      }
    }//if
  }//for

  if(!found) {
    jam();
    return; // failed node is not part of backup process, safe to continue
  }

  bool doMasterTakeover = false;
  if(NodeBitmask::get(theFailedNodes, refToNode(ptr.p->masterRef))){
    jam();
    doMasterTakeover = true;
  };

  if (newCoord == getOwnNodeId()){
    jam();
    if (doMasterTakeover) {
      /**
       * I'm new master
       */
      CRASH_INSERTION((10002));
#ifdef DEBUG_ABORT
      ndbout_c("**** Master Takeover: Node failed: Master id = %u", 
	       refToNode(ptr.p->masterRef));
#endif
      masterTakeOver(signal, ptr);
      return;
    }//if
    /**
     * I'm master for this backup
     */
    jam();
    CRASH_INSERTION((10001));
#ifdef DEBUG_ABORT
    ndbout_c("**** Master: Node failed: Master id = %u", 
	     refToNode(ptr.p->masterRef));
#endif
    masterAbort(signal, ptr, false);
    return;
  }//if

  /**
   * If there's a new master, (it's not me)
   * but remember who it is
   */
  ptr.p->masterRef = calcBackupBlockRef(newCoord);
#ifdef DEBUG_ABORT
  ndbout_c("**** Slave: Node failed: Master id = %u", 
	   refToNode(ptr.p->masterRef));
#endif
  /**
   * I abort myself as slave if not master
   */
  CRASH_INSERTION((10021));
  //    slaveAbort(signal, ptr);
} 

void
Backup::masterTakeOver(Signal* signal, BackupRecordPtr ptr)
{
  ptr.p->masterRef = reference();
  ptr.p->masterData.gsn = MAX_GSN + 1;

  switch(ptr.p->slaveState.getState()){
  case INITIAL:
    jam();
    ptr.p->masterData.state.forceState(INITIAL);
    break;
  case ABORTING:
    jam();
  case DEFINING:
    jam();
  case DEFINED:
    jam();
  case STARTED:
    jam();
  case SCANNING:
    jam();
    ptr.p->masterData.state.forceState(STARTED);
    break;
  case STOPPING:
    jam(); 
  case CLEANING:
    jam();
    ptr.p->masterData.state.forceState(STOPPING);
    break;
  default:
    ndbrequire(false);
  }
  masterAbort(signal, ptr, false);
}

void
Backup::execINCL_NODEREQ(Signal* signal)
{
  jamEntry();
  
  const Uint32 senderRef = signal->theData[0];
  const Uint32 inclNode  = signal->theData[1];

  NodePtr node;
  for(c_nodes.first(node); node.i != RNIL; c_nodes.next(node)) {
    jam();
    const Uint32 nodeId = node.p->nodeId;
    if(inclNode == nodeId){
      jam();
      
      ndbrequire(node.p->alive == 0);
      ndbrequire(!c_aliveNodes.get(nodeId));
      
      node.p->alive = 1;
      c_aliveNodes.set(nodeId);
      
      break;
    }//if
  }//for
  signal->theData[0] = reference();
  sendSignal(senderRef, GSN_INCL_NODECONF, signal, 1, JBB);
}

/*****************************************************************************
 * 
 * Master functionallity - Define backup
 *
 *****************************************************************************/

void
Backup::execBACKUP_REQ(Signal* signal)
{
  jamEntry();
  BackupReq * req = (BackupReq*)signal->getDataPtr();
  
  const Uint32 senderData = req->senderData;
  const BlockReference senderRef = signal->senderBlockRef();
  const Uint32 dataLen32 = req->backupDataLen; // In 32 bit words
  
  if(getOwnNodeId() != getMasterNodeId()) {
    jam();
    sendBackupRef(senderRef, signal, senderData, BackupRef::IAmNotMaster);
    return;
  }//if
  
  if(dataLen32 != 0) {
    jam();
    sendBackupRef(senderRef, signal, senderData, 
		  BackupRef::BackupDefinitionNotImplemented);
    return;
  }//if
  
#ifdef DEBUG_ABORT
  dumpUsedResources();
#endif
  /**
   * Seize a backup record
   */
  BackupRecordPtr ptr;
  c_backups.seize(ptr);
  if(ptr.i == RNIL) {
    jam();
    sendBackupRef(senderRef, signal, senderData, BackupRef::OutOfBackupRecord);
    return;
  }//if

  ndbrequire(ptr.p->pages.empty());
  ndbrequire(ptr.p->tables.isEmpty());
  
  ptr.p->masterData.state.forceState(INITIAL);
  ptr.p->masterData.state.setState(DEFINING);
  ptr.p->clientRef = senderRef;
  ptr.p->clientData = senderData;
  ptr.p->masterRef = reference();
  ptr.p->nodes = c_aliveNodes;
  ptr.p->backupId = 0;
  ptr.p->backupKey[0] = 0;
  ptr.p->backupKey[1] = 0;
  ptr.p->backupDataLen = 0;
  ptr.p->masterData.dropTrig.tableId = RNIL;
  ptr.p->masterData.alterTrig.tableId = RNIL;
  
  UtilSequenceReq * utilReq = (UtilSequenceReq*)signal->getDataPtrSend();
    
  ptr.p->masterData.gsn = GSN_UTIL_SEQUENCE_REQ;
  utilReq->senderData  = ptr.i;
  utilReq->sequenceId  = BACKUP_SEQUENCE;
  utilReq->requestType = UtilSequenceReq::NextVal;
  sendSignal(DBUTIL_REF, GSN_UTIL_SEQUENCE_REQ, 
	     signal, UtilSequenceReq::SignalLength, JBB);
}

void
Backup::execUTIL_SEQUENCE_REF(Signal* signal)
{
  BackupRecordPtr ptr;
  jamEntry();
  UtilSequenceRef * utilRef = (UtilSequenceRef*)signal->getDataPtr();
  ptr.i = utilRef->senderData;
  ndbrequire(ptr.i == RNIL);
  c_backupPool.getPtr(ptr);
  ndbrequire(ptr.p->masterData.gsn == GSN_UTIL_SEQUENCE_REQ);
  ptr.p->masterData.gsn = 0;
  sendBackupRef(signal, ptr, BackupRef::SequenceFailure);
}//execUTIL_SEQUENCE_REF()


void
Backup::sendBackupRef(Signal* signal, BackupRecordPtr ptr, Uint32 errorCode)
{
  jam();
  sendBackupRef(ptr.p->clientRef, signal, ptr.p->clientData, errorCode);
  //  ptr.p->masterData.state.setState(INITIAL);
  cleanupSlaveResources(ptr);
}

void
Backup::sendBackupRef(BlockReference senderRef, Signal *signal,
		      Uint32 senderData, Uint32 errorCode)
{
  jam();
  BackupRef* ref = (BackupRef*)signal->getDataPtrSend();
  ref->senderData = senderData;
  ref->errorCode = errorCode;
  ref->masterRef = numberToRef(BACKUP, getMasterNodeId());
  sendSignal(senderRef, GSN_BACKUP_REF, signal, BackupRef::SignalLength, JBB);

  if(errorCode != BackupRef::IAmNotMaster){
    signal->theData[0] = EventReport::BackupFailedToStart;
    signal->theData[1] = senderRef;
    signal->theData[2] = errorCode;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 3, JBB);
  }
}

void
Backup::execUTIL_SEQUENCE_CONF(Signal* signal)
{
  jamEntry();

  UtilSequenceConf * conf = (UtilSequenceConf*)signal->getDataPtr();
  
  if(conf->requestType == UtilSequenceReq::Create) {
    jam();
    sendSTTORRY(signal); // At startup in NDB
    return;
  }

  BackupRecordPtr ptr;
  ptr.i = conf->senderData;
  c_backupPool.getPtr(ptr);

  ndbrequire(ptr.p->masterData.gsn == GSN_UTIL_SEQUENCE_REQ);
  ptr.p->masterData.gsn = 0;
  if (ptr.p->masterData.state.getState() == ABORTING) {
    jam();
    sendBackupRef(signal, ptr, ptr.p->errorCode);
    return;
  }//if
  if (ERROR_INSERTED(10023)) {
    ptr.p->masterData.state.setState(ABORTING);
    sendBackupRef(signal, ptr, 323);
    return;
  }//if
  ndbrequire(ptr.p->masterData.state.getState() == DEFINING);

  ptr.p->backupId = conf->sequenceValue[0];
  ptr.p->backupKey[0] = (getOwnNodeId() << 16) | (ptr.p->backupId & 0xFFFF);
  ptr.p->backupKey[1] = NdbTick_CurrentMillisecond();

  ptr.p->masterData.gsn = GSN_UTIL_LOCK_REQ;
  Mutex mutex(signal, c_mutexMgr, ptr.p->masterData.m_defineBackupMutex);
  Callback c = { safe_cast(&Backup::defineBackupMutex_locked), ptr.i };
  ndbrequire(mutex.lock(c));

  return;
}

void
Backup::defineBackupMutex_locked(Signal* signal, Uint32 ptrI, Uint32 retVal){
  jamEntry();
  ndbrequire(retVal == 0);
  
  BackupRecordPtr ptr;
  ptr.i = ptrI;
  c_backupPool.getPtr(ptr);
  
  ndbrequire(ptr.p->masterData.gsn == GSN_UTIL_LOCK_REQ);
  ptr.p->masterData.gsn = 0;

  ptr.p->masterData.gsn = GSN_UTIL_LOCK_REQ;
  Mutex mutex(signal, c_mutexMgr, ptr.p->masterData.m_dictCommitTableMutex);
  Callback c = { safe_cast(&Backup::dictCommitTableMutex_locked), ptr.i };
  ndbrequire(mutex.lock(c));
}

void
Backup::dictCommitTableMutex_locked(Signal* signal, Uint32 ptrI,Uint32 retVal)
{
  jamEntry();
  ndbrequire(retVal == 0);
  
  /**
   * We now have both the mutexes
   */
  BackupRecordPtr ptr;
  ptr.i = ptrI;
  c_backupPool.getPtr(ptr);

  ndbrequire(ptr.p->masterData.gsn == GSN_UTIL_LOCK_REQ);
  ptr.p->masterData.gsn = 0;

  if (ERROR_INSERTED(10031)) {
    ptr.p->masterData.state.setState(ABORTING);
    ptr.p->setErrorCode(331);
  }//if

  if (ptr.p->masterData.state.getState() == ABORTING) {
    jam();
    
    /**
     * Unlock mutexes
     */
    jam();
    Mutex mutex1(signal, c_mutexMgr, ptr.p->masterData.m_dictCommitTableMutex);
    jam();
    mutex1.unlock(); // ignore response
    
    jam();
    Mutex mutex2(signal, c_mutexMgr, ptr.p->masterData.m_defineBackupMutex);
    jam();
    mutex2.unlock(); // ignore response

    sendBackupRef(signal, ptr, ptr.p->errorCode);
    return;
  }//if
  
  ndbrequire(ptr.p->masterData.state.getState() == DEFINING);

  sendDefineBackupReq(signal, ptr);
}

/*****************************************************************************
 * 
 * Master functionallity - Define backup cont'd (from now on all slaves are in)
 *
 *****************************************************************************/

void
Backup::sendSignalAllWait(BackupRecordPtr ptr, Uint32 gsn, Signal *signal, 
			  Uint32 signalLength, bool executeDirect)
{
  jam();
  ptr.p->masterData.gsn = gsn;
  ptr.p->masterData.sendCounter.clearWaitingFor();
  NodePtr node;
  for(c_nodes.first(node); node.i != RNIL; c_nodes.next(node)){
    jam();
    const Uint32 nodeId = node.p->nodeId;
    if(node.p->alive && ptr.p->nodes.get(nodeId)){
      jam();
      
      ptr.p->masterData.sendCounter.setWaitingFor(nodeId);
      
      const BlockReference ref = numberToRef(BACKUP, nodeId);
      if (!executeDirect || ref != reference()) {
        sendSignal(ref, gsn, signal, signalLength, JBB);
      }//if
    }//if
  }//for
  if (executeDirect) {
    EXECUTE_DIRECT(BACKUP, gsn, signal, signalLength);
  }
}

bool
Backup::haveAllSignals(BackupRecordPtr ptr, Uint32 gsn, Uint32 nodeId)
{ 
  ndbrequire(ptr.p->masterRef == reference());
  ndbrequire(ptr.p->masterData.gsn == gsn);
  ndbrequire(!ptr.p->masterData.sendCounter.done());
  ndbrequire(ptr.p->masterData.sendCounter.isWaitingFor(nodeId));
  
  ptr.p->masterData.sendCounter.clearWaitingFor(nodeId);
  
  if (ptr.p->masterData.sendCounter.done())
    ptr.p->masterData.gsn = 0;

  return ptr.p->masterData.sendCounter.done();
}

void
Backup::sendDefineBackupReq(Signal *signal, BackupRecordPtr ptr)
{
  /**
   * Sending define backup to all participants
   */
  DefineBackupReq * req = (DefineBackupReq*)signal->getDataPtrSend();
  req->backupId = ptr.p->backupId;
  req->clientRef = ptr.p->clientRef;
  req->clientData = ptr.p->clientData;
  req->senderRef = reference();
  req->backupPtr = ptr.i;
  req->backupKey[0] = ptr.p->backupKey[0];
  req->backupKey[1] = ptr.p->backupKey[1];
  req->nodes = ptr.p->nodes;
  req->backupDataLen = ptr.p->backupDataLen;
  
  ptr.p->masterData.errorCode = 0;
  ptr.p->okToCleanMaster = false; // master must wait with cleaning to last
  sendSignalAllWait(ptr, GSN_DEFINE_BACKUP_REQ, signal, 
		    DefineBackupReq::SignalLength,
		    true /* do execute direct on oneself */);
  /**
   * Now send backup data
   */
  const Uint32 len = ptr.p->backupDataLen;
  if(len == 0){
    /**
     * No data to send
     */
    jam();
    return;
  }//if
  
  /**
   * Not implemented
   */
  ndbrequire(0);
}

void
Backup::execDEFINE_BACKUP_REF(Signal* signal)
{
  jamEntry();

  DefineBackupRef* ref = (DefineBackupRef*)signal->getDataPtr();

  const Uint32 ptrI = ref->backupPtr;
  const Uint32 backupId = ref->backupId;
  const Uint32 nodeId = refToNode(signal->senderBlockRef());

  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);

  masterAbortCheck(); // macro will do return if ABORTING
  
  ptr.p->masterData.errorCode = ref->errorCode;
  defineBackupReply(signal, ptr, nodeId);
}

void
Backup::execDEFINE_BACKUP_CONF(Signal* signal)
{
  jamEntry();

  DefineBackupConf* conf = (DefineBackupConf*)signal->getDataPtr();
  const Uint32 ptrI = conf->backupPtr;
  const Uint32 backupId = conf->backupId;
  const Uint32 nodeId = refToNode(signal->senderBlockRef());

  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);

  masterAbortCheck(); // macro will do return if ABORTING

  if (ERROR_INSERTED(10024)) {
    ptr.p->masterData.errorCode = 324;
  }//if

  defineBackupReply(signal, ptr, nodeId);
}

void
Backup::defineBackupReply(Signal* signal, BackupRecordPtr ptr, Uint32 nodeId)
{
  if (!haveAllSignals(ptr, GSN_DEFINE_BACKUP_REQ, nodeId)) {
    jam();
    return;
  }
  /**
   * Unlock mutexes
   */
  jam();
  Mutex mutex1(signal, c_mutexMgr, ptr.p->masterData.m_dictCommitTableMutex);
  jam();
  mutex1.unlock(); // ignore response

  jam();
  Mutex mutex2(signal, c_mutexMgr, ptr.p->masterData.m_defineBackupMutex);
  jam();
  mutex2.unlock(); // ignore response

  if(ptr.p->errorCode) {
    jam();
    ptr.p->masterData.errorCode = ptr.p->errorCode;
  }

  if(ptr.p->masterData.errorCode){
    jam();
    ptr.p->setErrorCode(ptr.p->masterData.errorCode);
    sendAbortBackupOrd(signal, ptr, AbortBackupOrd::OkToClean);
    masterSendAbortBackup(signal, ptr);
    return;
  }
  
  /**
   * Reply to client
   */
  BackupConf * conf = (BackupConf*)signal->getDataPtrSend();
  conf->backupId = ptr.p->backupId;
  conf->senderData = ptr.p->clientData;
  conf->nodes = ptr.p->nodes;
  sendSignal(ptr.p->clientRef, GSN_BACKUP_CONF, signal, 
	     BackupConf::SignalLength, JBB);
  
  signal->theData[0] = EventReport::BackupStarted;
  signal->theData[1] = ptr.p->clientRef;
  signal->theData[2] = ptr.p->backupId;
  ptr.p->nodes.copyto(NdbNodeBitmask::Size, signal->theData+3);
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 3+NdbNodeBitmask::Size, JBB);
  
  ptr.p->masterData.state.setState(DEFINED);
  /**
   * Prepare Trig
   */
  TablePtr tabPtr;
  ndbrequire(ptr.p->tables.first(tabPtr));
  sendCreateTrig(signal, ptr, tabPtr);
}

/*****************************************************************************
 * 
 * Master functionallity - Prepare triggers
 *
 *****************************************************************************/
void
Backup::createAttributeMask(TablePtr tabPtr, 
			    Bitmask<MAXNROFATTRIBUTESINWORDS> & mask)
{
  mask.clear();
  Table & table = * tabPtr.p;
  for(Uint32 i = 0; i<table.noOfAttributes; i++) {
    jam();
    AttributePtr attr;
    table.attributes.getPtr(attr, i);
    if(attr.p->data.key != 0){
      jam();
      continue;
    }
    mask.set(i);
  }
}

void
Backup::sendCreateTrig(Signal* signal, 
			   BackupRecordPtr ptr, TablePtr tabPtr)
{
  CreateTrigReq * req =(CreateTrigReq *)signal->getDataPtrSend();
  
  ptr.p->errorCode = 0;
  ptr.p->masterData.gsn = GSN_CREATE_TRIG_REQ;
  ptr.p->masterData.sendCounter = 3;
  ptr.p->masterData.createTrig.tableId = tabPtr.p->tableId;

  req->setUserRef(reference());
  req->setConnectionPtr(ptr.i);
  req->setRequestType(CreateTrigReq::RT_USER);
  
  Bitmask<MAXNROFATTRIBUTESINWORDS> attrMask;
  createAttributeMask(tabPtr, attrMask);
  req->setAttributeMask(attrMask);
  req->setTableId(tabPtr.p->tableId);
  req->setIndexId(RNIL);        // not used
  req->setTriggerId(RNIL);      // to be created
  req->setTriggerType(TriggerType::SUBSCRIPTION);
  req->setTriggerActionTime(TriggerActionTime::TA_DETACHED);
  req->setMonitorReplicas(true);
  req->setMonitorAllAttributes(false);
  req->setOnline(false);        // leave trigger offline

  char triggerName[MAX_TAB_NAME_SIZE];
  Uint32 nameBuffer[2 + ((MAX_TAB_NAME_SIZE + 3) >> 2)];  // SP string
  LinearWriter w(nameBuffer, sizeof(nameBuffer) >> 2);
  LinearSectionPtr lsPtr[3];
  
  for (int i=0; i < 3; i++) {
    req->setTriggerEvent(triggerEventValues[i]);
    snprintf(triggerName, sizeof(triggerName), triggerNameFormat[i],
	     ptr.p->backupId, tabPtr.p->tableId);
    w.reset();
    w.add(CreateTrigReq::TriggerNameKey, triggerName);
    lsPtr[0].p = nameBuffer;
    lsPtr[0].sz = w.getWordsUsed();
    sendSignal(DBDICT_REF, GSN_CREATE_TRIG_REQ, 
	       signal, CreateTrigReq::SignalLength, JBB, lsPtr, 1);
  }
}

void
Backup::execCREATE_TRIG_CONF(Signal* signal)
{
  jamEntry();
  CreateTrigConf * conf = (CreateTrigConf*)signal->getDataPtr();
  
  const Uint32 ptrI = conf->getConnectionPtr();
  const Uint32 tableId = conf->getTableId();
  const TriggerEvent::Value type = conf->getTriggerEvent();
  const Uint32 triggerId = conf->getTriggerId();

  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);

  /**
   * Verify that I'm waiting for this conf
   */
  ndbrequire(ptr.p->masterRef == reference());
  ndbrequire(ptr.p->masterData.gsn == GSN_CREATE_TRIG_REQ);
  ndbrequire(ptr.p->masterData.sendCounter.done() == false);
  ndbrequire(ptr.p->masterData.createTrig.tableId == tableId);
  
  TablePtr tabPtr;
  ndbrequire(findTable(ptr, tabPtr, tableId));
  ndbrequire(type < 3); // if some decides to change the enums

  ndbrequire(tabPtr.p->triggerIds[type] == ILLEGAL_TRIGGER_ID);
  tabPtr.p->triggerIds[type] = triggerId;
  
  createTrigReply(signal, ptr);
}

void
Backup::execCREATE_TRIG_REF(Signal* signal)
{
  CreateTrigRef* ref = (CreateTrigRef*)signal->getDataPtr();

  const Uint32 ptrI = ref->getConnectionPtr();
  const Uint32 tableId = ref->getTableId();

  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);

  /**
   * Verify that I'm waiting for this ref
   */
  ndbrequire(ptr.p->masterRef == reference());
  ndbrequire(ptr.p->masterData.gsn == GSN_CREATE_TRIG_REQ);
  ndbrequire(ptr.p->masterData.sendCounter.done() == false);
  ndbrequire(ptr.p->masterData.createTrig.tableId == tableId);

  ptr.p->setErrorCode(ref->getErrorCode());
  
  createTrigReply(signal, ptr);
}

void
Backup::createTrigReply(Signal* signal, BackupRecordPtr ptr)
{
  CRASH_INSERTION(10003);

  /**
   * Check finished with table
   */
  ptr.p->masterData.sendCounter--;
  if(ptr.p->masterData.sendCounter.done() == false){
    jam();
    return;
  }//if

  ptr.p->masterData.gsn = 0;

  if(ptr.p->checkError()) {
    jam();
    masterAbort(signal, ptr, true);
    return;
  }//if

  if (ERROR_INSERTED(10025)) {
    ptr.p->errorCode = 325;
    masterAbort(signal, ptr, true);
    return;
  }//if

  TablePtr tabPtr;
  ndbrequire(findTable(ptr, tabPtr, ptr.p->masterData.createTrig.tableId));
  
  /**
   * Next table
   */
  ptr.p->tables.next(tabPtr);
  if(tabPtr.i != RNIL){
    jam();
    sendCreateTrig(signal, ptr, tabPtr);
    return;
  }//if

  /**
   * Finished with all tables, send StartBackupReq
   */
  ptr.p->masterData.state.setState(STARTED);

  ptr.p->tables.first(tabPtr);
  ptr.p->errorCode = 0;
  ptr.p->masterData.startBackup.signalNo = 0;
  ptr.p->masterData.startBackup.noOfSignals = 
    (ptr.p->tables.noOfElements() + StartBackupReq::MaxTableTriggers - 1) / 
    StartBackupReq::MaxTableTriggers;
  sendStartBackup(signal, ptr, tabPtr);
}

/*****************************************************************************
 * 
 * Master functionallity - Start backup
 *
 *****************************************************************************/
void
Backup::sendStartBackup(Signal* signal, BackupRecordPtr ptr, TablePtr tabPtr)
{

  ptr.p->masterData.startBackup.tablePtr = tabPtr.i;
  
  StartBackupReq* req = (StartBackupReq*)signal->getDataPtrSend();
  req->backupId = ptr.p->backupId;
  req->backupPtr = ptr.i;
  req->signalNo = ptr.p->masterData.startBackup.signalNo;
  req->noOfSignals = ptr.p->masterData.startBackup.noOfSignals;
  Uint32 i;
  for(i = 0; i<StartBackupReq::MaxTableTriggers; i++) {
    jam();
    req->tableTriggers[i].tableId = tabPtr.p->tableId;
    req->tableTriggers[i].triggerIds[0] = tabPtr.p->triggerIds[0];
    req->tableTriggers[i].triggerIds[1] = tabPtr.p->triggerIds[1];
    req->tableTriggers[i].triggerIds[2] = tabPtr.p->triggerIds[2];
    if(!ptr.p->tables.next(tabPtr)){
      jam();
      i++;
      break;
    }//if
  }//for
  req->noOfTableTriggers = i;

  sendSignalAllWait(ptr, GSN_START_BACKUP_REQ, signal,
		    StartBackupReq::HeaderLength + 
		    (i * StartBackupReq::TableTriggerLength));
}

void
Backup::execSTART_BACKUP_REF(Signal* signal)
{
  jamEntry();

  StartBackupRef* ref = (StartBackupRef*)signal->getDataPtr();
  const Uint32 ptrI = ref->backupPtr;
  const Uint32 backupId = ref->backupId;
  const Uint32 signalNo = ref->signalNo;
  const Uint32 nodeId = refToNode(signal->senderBlockRef());

  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);

  masterAbortCheck(); // macro will do return if ABORTING

  ptr.p->setErrorCode(ref->errorCode);
  startBackupReply(signal, ptr, nodeId, signalNo);
}

void
Backup::execSTART_BACKUP_CONF(Signal* signal)
{
  jamEntry();
  
  StartBackupConf* conf = (StartBackupConf*)signal->getDataPtr();
  const Uint32 ptrI = conf->backupPtr;
  const Uint32 backupId = conf->backupId;
  const Uint32 signalNo = conf->signalNo;
  const Uint32 nodeId = refToNode(signal->senderBlockRef());
  
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);

  masterAbortCheck(); // macro will do return if ABORTING

  startBackupReply(signal, ptr, nodeId, signalNo);
}

void
Backup::startBackupReply(Signal* signal, BackupRecordPtr ptr, 
			 Uint32 nodeId, Uint32 signalNo)
{

  CRASH_INSERTION((10004));

  ndbrequire(ptr.p->masterData.startBackup.signalNo == signalNo);
  if (!haveAllSignals(ptr, GSN_START_BACKUP_REQ, nodeId)) {
    jam();
    return;
  }

  if(ptr.p->checkError()){
    jam();
    masterAbort(signal, ptr, true);
    return;
  }
  
  if (ERROR_INSERTED(10026)) {
    ptr.p->errorCode = 326;
    masterAbort(signal, ptr, true);
    return;
  }//if

  TablePtr tabPtr;
  c_tablePool.getPtr(tabPtr, ptr.p->masterData.startBackup.tablePtr);
  for(Uint32 i = 0; i<StartBackupReq::MaxTableTriggers; i++) {
    jam();
    if(!ptr.p->tables.next(tabPtr)) {
      jam();
      break;
    }//if
  }//for
  
  if(tabPtr.i != RNIL) {
    jam();
    ptr.p->masterData.startBackup.signalNo++;
    sendStartBackup(signal, ptr, tabPtr);
    return;
  }

  sendAlterTrig(signal, ptr);
}

/*****************************************************************************
 * 
 * Master functionallity - Activate triggers
 *
 *****************************************************************************/
void
Backup::sendAlterTrig(Signal* signal, BackupRecordPtr ptr)
{
  AlterTrigReq * req =(AlterTrigReq *)signal->getDataPtrSend();
  
  ptr.p->errorCode = 0;
  ptr.p->masterData.gsn = GSN_ALTER_TRIG_REQ;
  ptr.p->masterData.sendCounter = 0;
  
  req->setUserRef(reference());
  req->setConnectionPtr(ptr.i);
  req->setRequestType(AlterTrigReq::RT_USER);
  req->setTriggerInfo(0);       // not used on ALTER via DICT
  req->setOnline(true);
  req->setReceiverRef(reference());

  TablePtr tabPtr;

  if (ptr.p->masterData.alterTrig.tableId == RNIL) {
    jam();
    ptr.p->tables.first(tabPtr);
  } else {
    jam();
    ndbrequire(findTable(ptr, tabPtr, ptr.p->masterData.alterTrig.tableId));
    ptr.p->tables.next(tabPtr);
  }//if
  if (tabPtr.i != RNIL) {
    jam();
    ptr.p->masterData.alterTrig.tableId = tabPtr.p->tableId;
    req->setTableId(tabPtr.p->tableId);

    req->setTriggerId(tabPtr.p->triggerIds[0]);
    sendSignal(DBDICT_REF, GSN_ALTER_TRIG_REQ, 
	       signal, AlterTrigReq::SignalLength, JBB);
    
    req->setTriggerId(tabPtr.p->triggerIds[1]);
    sendSignal(DBDICT_REF, GSN_ALTER_TRIG_REQ, 
	       signal, AlterTrigReq::SignalLength, JBB);

    req->setTriggerId(tabPtr.p->triggerIds[2]);
    sendSignal(DBDICT_REF, GSN_ALTER_TRIG_REQ, 
	       signal, AlterTrigReq::SignalLength, JBB);

    ptr.p->masterData.sendCounter += 3;
    return;
  }//if
  ptr.p->masterData.alterTrig.tableId = RNIL;
  /**
   * Finished with all tables
   */
  ptr.p->masterData.gsn = GSN_WAIT_GCP_REQ;
  ptr.p->masterData.waitGCP.startBackup = true;
  
  WaitGCPReq * waitGCPReq = (WaitGCPReq*)signal->getDataPtrSend();
  waitGCPReq->senderRef = reference();
  waitGCPReq->senderData = ptr.i;
  waitGCPReq->requestType = WaitGCPReq::CompleteForceStart;
  sendSignal(DBDIH_REF, GSN_WAIT_GCP_REQ, signal, 
	     WaitGCPReq::SignalLength,JBB);
}

void
Backup::execALTER_TRIG_CONF(Signal* signal)
{
  jamEntry();

  AlterTrigConf* conf = (AlterTrigConf*)signal->getDataPtr();
  const Uint32 ptrI = conf->getConnectionPtr();
  
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);
  
  alterTrigReply(signal, ptr);
}

void
Backup::execALTER_TRIG_REF(Signal* signal)
{
  jamEntry();

  AlterTrigRef* ref = (AlterTrigRef*)signal->getDataPtr();
  const Uint32 ptrI = ref->getConnectionPtr();
  
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);

  ptr.p->setErrorCode(ref->getErrorCode());
  
  alterTrigReply(signal, ptr);
}

void
Backup::alterTrigReply(Signal* signal, BackupRecordPtr ptr)
{

  CRASH_INSERTION((10005));

  ndbrequire(ptr.p->masterRef == reference());
  ndbrequire(ptr.p->masterData.gsn == GSN_ALTER_TRIG_REQ);
  ndbrequire(ptr.p->masterData.sendCounter.done() == false);

  ptr.p->masterData.sendCounter--;

  if(ptr.p->masterData.sendCounter.done() == false){
    jam();
    return;
  }//if

  ptr.p->masterData.gsn = 0;

  if(ptr.p->checkError()){
    jam();
    masterAbort(signal, ptr, true);
    return;
  }//if

  sendAlterTrig(signal, ptr);
}

void
Backup::execWAIT_GCP_REF(Signal* signal)
{
  jamEntry();
  
  CRASH_INSERTION((10006));

  WaitGCPRef * ref = (WaitGCPRef*)signal->getDataPtr();
  const Uint32 ptrI = ref->senderData;
  
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);

  ndbrequire(ptr.p->masterRef == reference());
  ndbrequire(ptr.p->masterData.gsn == GSN_WAIT_GCP_REQ);

  WaitGCPReq * req = (WaitGCPReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->senderData = ptr.i;
  req->requestType = WaitGCPReq::CompleteForceStart;
  sendSignal(DBDIH_REF, GSN_WAIT_GCP_REQ, signal, 
	     WaitGCPReq::SignalLength,JBB);
}

void
Backup::execWAIT_GCP_CONF(Signal* signal){
  jamEntry();

  CRASH_INSERTION((10007));

  WaitGCPConf * conf = (WaitGCPConf*)signal->getDataPtr();
  const Uint32 ptrI = conf->senderData;
  const Uint32 gcp = conf->gcp;
  
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);
  
  ndbrequire(ptr.p->masterRef == reference());
  ndbrequire(ptr.p->masterData.gsn == GSN_WAIT_GCP_REQ);
  ptr.p->masterData.gsn = 0;
  
  if(ptr.p->checkError()) {
    jam();
    masterAbort(signal, ptr, true);
    return;
  }//if
  
  if(ptr.p->masterData.waitGCP.startBackup) {
    jam();
    CRASH_INSERTION((10008));
    ptr.p->startGCP = gcp;
    ptr.p->masterData.state.setState(SCANNING);
    nextFragment(signal, ptr);
  } else {
    jam();
    CRASH_INSERTION((10009));
    ptr.p->stopGCP = gcp;
    ptr.p->masterData.state.setState(STOPPING);
    sendDropTrig(signal, ptr); // regular dropping of triggers
  }//if
}
/*****************************************************************************
 * 
 * Master functionallity - Backup fragment
 *
 *****************************************************************************/
void
Backup::nextFragment(Signal* signal, BackupRecordPtr ptr)
{
  jam();

  BackupFragmentReq* req = (BackupFragmentReq*)signal->getDataPtrSend();
  req->backupPtr = ptr.i;
  req->backupId = ptr.p->backupId;

  NodeBitmask nodes = ptr.p->nodes;
  Uint32 idleNodes = nodes.count();
  Uint32 saveIdleNodes = idleNodes;
  ndbrequire(idleNodes > 0);

  TablePtr tabPtr;
  ptr.p->tables.first(tabPtr);
  for(; tabPtr.i != RNIL && idleNodes > 0; ptr.p->tables.next(tabPtr)) {
    jam();
    FragmentPtr fragPtr;
    Array<Fragment> & frags = tabPtr.p->fragments;
    const Uint32 fragCount = frags.getSize();
    
    for(Uint32 i = 0; i<fragCount && idleNodes > 0; i++) {
      jam();
      tabPtr.p->fragments.getPtr(fragPtr, i);
      const Uint32 nodeId = fragPtr.p->node;
      if(fragPtr.p->scanning != 0) {
        jam();
	ndbrequire(nodes.get(nodeId));
	nodes.clear(nodeId);
	idleNodes--;
      } else if(fragPtr.p->scanned == 0 && nodes.get(nodeId)){
	jam();
	fragPtr.p->scanning = 1;
	nodes.clear(nodeId);
	idleNodes--;
	
	req->tableId = tabPtr.p->tableId;
	req->fragmentNo = i;
	req->count = 0;

	const BlockReference ref = numberToRef(BACKUP, nodeId);
	sendSignal(ref, GSN_BACKUP_FRAGMENT_REQ, signal,
		   BackupFragmentReq::SignalLength, JBB);
      }//if
    }//for
  }//for
  
  if(idleNodes != saveIdleNodes){
    jam();
    return;
  }//if

  /**
   * Finished with all tables
   */
  {
    ptr.p->masterData.gsn = GSN_WAIT_GCP_REQ;
    ptr.p->masterData.waitGCP.startBackup = false;
    
    WaitGCPReq * req = (WaitGCPReq*)signal->getDataPtrSend();
    req->senderRef = reference();
    req->senderData = ptr.i;
    req->requestType = WaitGCPReq::CompleteForceStart;
    sendSignal(DBDIH_REF, GSN_WAIT_GCP_REQ, signal, 
	       WaitGCPReq::SignalLength, JBB);
  }
}

void
Backup::execBACKUP_FRAGMENT_CONF(Signal* signal)
{
  jamEntry();

  CRASH_INSERTION((10010));
  
  BackupFragmentConf * conf = (BackupFragmentConf*)signal->getDataPtr();
  const Uint32 ptrI = conf->backupPtr;
  const Uint32 backupId = conf->backupId;
  const Uint32 tableId = conf->tableId;
  const Uint32 fragmentNo = conf->fragmentNo;
  const Uint32 nodeId = refToNode(signal->senderBlockRef());
  const Uint32 noOfBytes = conf->noOfBytes;
  const Uint32 noOfRecords = conf->noOfRecords;
  
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);

  masterAbortCheck(); // macro will do return if ABORTING

  ptr.p->noOfBytes += noOfBytes;
  ptr.p->noOfRecords += noOfRecords;

  TablePtr tabPtr;
  ndbrequire(findTable(ptr, tabPtr, tableId));

  FragmentPtr fragPtr;
  tabPtr.p->fragments.getPtr(fragPtr, fragmentNo);

  ndbrequire(fragPtr.p->scanned == 0);
  ndbrequire(fragPtr.p->scanning == 1);
  ndbrequire(fragPtr.p->node == nodeId);

  fragPtr.p->scanned = 1;
  fragPtr.p->scanning = 0;

  if(ptr.p->checkError()) {
    jam();
    masterAbort(signal, ptr, true);
    return;
  }//if
  if (ERROR_INSERTED(10028)) {
    ptr.p->errorCode = 328;
    masterAbort(signal, ptr, true);
    return;
  }//if
  nextFragment(signal, ptr);
}

void
Backup::execBACKUP_FRAGMENT_REF(Signal* signal)
{
  jamEntry();

  CRASH_INSERTION((10011));

  BackupFragmentRef * ref = (BackupFragmentRef*)signal->getDataPtr();
  const Uint32 ptrI = ref->backupPtr;
  const Uint32 backupId = ref->backupId;
  
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);

  masterAbortCheck(); // macro will do return if ABORTING

  ptr.p->setErrorCode(ref->errorCode);
  masterAbort(signal, ptr, true);
}

/*****************************************************************************
 * 
 * Master functionallity - Drop triggers
 *
 *****************************************************************************/

void
Backup::sendDropTrig(Signal* signal, BackupRecordPtr ptr)
{
  TablePtr tabPtr;
  if (ptr.p->masterData.dropTrig.tableId == RNIL) {
    jam();
    ptr.p->tables.first(tabPtr);
  } else {
    jam();
    ndbrequire(findTable(ptr, tabPtr, ptr.p->masterData.dropTrig.tableId));
    ptr.p->tables.next(tabPtr);
  }//if
  if (tabPtr.i != RNIL) {
    jam();
    sendDropTrig(signal, ptr, tabPtr);
  } else {
    jam();
    ptr.p->masterData.dropTrig.tableId = RNIL;

    sendAbortBackupOrd(signal, ptr, AbortBackupOrd::OkToClean);

    if(ptr.p->masterData.state.getState() == STOPPING) {
      jam();
      sendStopBackup(signal, ptr);
      return;
    }//if
    ndbrequire(ptr.p->masterData.state.getState() == ABORTING);
    masterSendAbortBackup(signal, ptr);
  }//if
}

void
Backup::sendDropTrig(Signal* signal, BackupRecordPtr ptr, TablePtr tabPtr)
{
  jam();
  DropTrigReq * req = (DropTrigReq *)signal->getDataPtrSend();

  ptr.p->masterData.gsn = GSN_DROP_TRIG_REQ;
  ptr.p->masterData.sendCounter = 0;
    
  req->setConnectionPtr(ptr.i);
  req->setUserRef(reference()); // Sending to myself
  req->setRequestType(DropTrigReq::RT_USER);
  req->setIndexId(RNIL);
  req->setTriggerInfo(0);       // not used on DROP via DICT

  char triggerName[MAX_TAB_NAME_SIZE];
  Uint32 nameBuffer[2 + ((MAX_TAB_NAME_SIZE + 3) >> 2)];  // SP string
  LinearWriter w(nameBuffer, sizeof(nameBuffer) >> 2);
  LinearSectionPtr lsPtr[3];
  
  ptr.p->masterData.dropTrig.tableId = tabPtr.p->tableId;
  req->setTableId(tabPtr.p->tableId);

  for (int i = 0; i < 3; i++) {
    Uint32 id = tabPtr.p->triggerIds[i];
    req->setTriggerId(id);
    if (id != ILLEGAL_TRIGGER_ID) {
      sendSignal(DBDICT_REF, GSN_DROP_TRIG_REQ, 
		 signal, DropTrigReq::SignalLength, JBB);
    } else {
      snprintf(triggerName, sizeof(triggerName), triggerNameFormat[i],
	       ptr.p->backupId, tabPtr.p->tableId);
      w.reset();
      w.add(CreateTrigReq::TriggerNameKey, triggerName);
      lsPtr[0].p = nameBuffer;
      lsPtr[0].sz = w.getWordsUsed();
      sendSignal(DBDICT_REF, GSN_DROP_TRIG_REQ, 
		 signal, DropTrigReq::SignalLength, JBB, lsPtr, 1);
    }
    ptr.p->masterData.sendCounter ++;
  }
}

void
Backup::execDROP_TRIG_REF(Signal* signal)
{
  jamEntry();

  DropTrigRef* ref = (DropTrigRef*)signal->getDataPtr();
  const Uint32 ptrI = ref->getConnectionPtr();
  
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);
 
  //ndbrequire(ref->getErrorCode() == DropTrigRef::NoSuchTrigger);
  dropTrigReply(signal, ptr);
}

void
Backup::execDROP_TRIG_CONF(Signal* signal)
{
  jamEntry();
  
  DropTrigConf* conf = (DropTrigConf*)signal->getDataPtr();
  const Uint32 ptrI = conf->getConnectionPtr();
  
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);
  
  dropTrigReply(signal, ptr);
}

void
Backup::dropTrigReply(Signal* signal, BackupRecordPtr ptr)
{

  CRASH_INSERTION((10012));

  ndbrequire(ptr.p->masterRef == reference());
  ndbrequire(ptr.p->masterData.gsn == GSN_DROP_TRIG_REQ);
  ndbrequire(ptr.p->masterData.sendCounter.done() == false);
  
  ptr.p->masterData.sendCounter--;
  if(ptr.p->masterData.sendCounter.done() == false){
    jam();
    return;
  }//if
  
  ptr.p->masterData.gsn = 0;
  sendDropTrig(signal, ptr); // recursive next
}

/*****************************************************************************
 * 
 * Master functionallity - Stop backup
 *
 *****************************************************************************/
void
Backup::execSTOP_BACKUP_REF(Signal* signal)
{
  jamEntry();
  ndbrequire(0);
}

void
Backup::sendStopBackup(Signal* signal, BackupRecordPtr ptr)
{
  jam();
  ptr.p->masterData.gsn = GSN_STOP_BACKUP_REQ;

  StopBackupReq* stop = (StopBackupReq*)signal->getDataPtrSend();
  stop->backupPtr = ptr.i;
  stop->backupId = ptr.p->backupId;
  stop->startGCP = ptr.p->startGCP;
  stop->stopGCP = ptr.p->stopGCP;

  sendSignalAllWait(ptr, GSN_STOP_BACKUP_REQ, signal, 
		    StopBackupReq::SignalLength);
}

void
Backup::execSTOP_BACKUP_CONF(Signal* signal)
{
  jamEntry();
  
  StopBackupConf* conf = (StopBackupConf*)signal->getDataPtr();
  const Uint32 ptrI = conf->backupPtr;
  const Uint32 backupId = conf->backupId;
  const Uint32 nodeId = refToNode(signal->senderBlockRef());
  
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);

  masterAbortCheck(); // macro will do return if ABORTING

  ptr.p->noOfLogBytes += conf->noOfLogBytes;
  ptr.p->noOfLogRecords += conf->noOfLogRecords;
  
  stopBackupReply(signal, ptr, nodeId);
}

void
Backup::stopBackupReply(Signal* signal, BackupRecordPtr ptr, Uint32 nodeId)
{
  CRASH_INSERTION((10013));

  if (!haveAllSignals(ptr, GSN_STOP_BACKUP_REQ, nodeId)) {
    jam();
    return;
  }

  //  ptr.p->masterData.state.setState(INITIAL);
 
  // send backup complete first to slaves so that they know 
  sendAbortBackupOrd(signal, ptr, AbortBackupOrd::BackupComplete);

  BackupCompleteRep * rep = (BackupCompleteRep*)signal->getDataPtrSend();
  rep->backupId = ptr.p->backupId;
  rep->senderData = ptr.p->clientData;
  rep->startGCP = ptr.p->startGCP;
  rep->stopGCP = ptr.p->stopGCP;
  rep->noOfBytes = ptr.p->noOfBytes;
  rep->noOfRecords = ptr.p->noOfRecords;
  rep->noOfLogBytes = ptr.p->noOfLogBytes;
  rep->noOfLogRecords = ptr.p->noOfLogRecords;
  rep->nodes = ptr.p->nodes;
  sendSignal(ptr.p->clientRef, GSN_BACKUP_COMPLETE_REP, signal,
	     BackupCompleteRep::SignalLength, JBB);

  signal->theData[0] = EventReport::BackupCompleted;
  signal->theData[1] = ptr.p->clientRef;
  signal->theData[2] = ptr.p->backupId;
  signal->theData[3] = ptr.p->startGCP;
  signal->theData[4] = ptr.p->stopGCP;
  signal->theData[5] = ptr.p->noOfBytes;
  signal->theData[6] = ptr.p->noOfRecords;
  signal->theData[7] = ptr.p->noOfLogBytes;
  signal->theData[8] = ptr.p->noOfLogRecords;
  ptr.p->nodes.copyto(NdbNodeBitmask::Size, signal->theData+9);
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 9+NdbNodeBitmask::Size, JBB);
}

/*****************************************************************************
 * 
 * Master functionallity - Abort backup
 *
 *****************************************************************************/
void
Backup::masterAbort(Signal* signal, BackupRecordPtr ptr, bool controlledAbort)
{
  if(ptr.p->masterData.state.getState() == ABORTING) {
#ifdef DEBUG_ABORT
    ndbout_c("---- Master already aborting");
#endif
    jam();
    return;
  }
  jam();
#ifdef DEBUG_ABORT
  ndbout_c("************ masterAbort");
#endif

  sendAbortBackupOrd(signal, ptr, AbortBackupOrd::BackupFailure);
  if (!ptr.p->checkError())
    ptr.p->errorCode = AbortBackupOrd::BackupFailureDueToNodeFail;

  const State s = ptr.p->masterData.state.getState();

  ptr.p->masterData.state.setState(ABORTING);

  ndbrequire(s == INITIAL ||
	     s == STARTED ||
	     s == DEFINING ||
	     s == DEFINED ||
	     s == SCANNING ||
	     s == STOPPING ||
	     s == ABORTING);
  if(ptr.p->masterData.gsn == GSN_UTIL_SEQUENCE_REQ) {
    jam();
    DEBUG_OUT("masterAbort: gsn = GSN_UTIL_SEQUENCE_REQ");
    //-------------------------------------------------------
    // We are waiting for UTIL_SEQUENCE response. We rely on
    // this to arrive and check for ABORTING in response. 
    // No slaves are involved at this point and ABORT simply
    // results in BACKUP_REF to client
    //-------------------------------------------------------
    /**
     * Waiting for Sequence Id
     * @see execUTIL_SEQUENCE_CONF
     */ 
    return;
  }//if
  
  if(ptr.p->masterData.gsn == GSN_UTIL_LOCK_REQ) {
    jam();
    DEBUG_OUT("masterAbort: gsn = GSN_UTIL_LOCK_REQ");
    //-------------------------------------------------------
    // We are waiting for UTIL_LOCK response (mutex). We rely on
    // this to arrive and check for ABORTING in response.
    // No slaves are involved at this point and ABORT simply
    // results in BACKUP_REF to client
    //-------------------------------------------------------
    /**
     * Waiting for lock
     * @see execUTIL_LOCK_CONF
     */ 
    return;
  }//if
  
  /**
   * Unlock mutexes only at master
   */
  jam();
  Mutex mutex1(signal, c_mutexMgr, ptr.p->masterData.m_dictCommitTableMutex);
  jam();
  mutex1.unlock(); // ignore response
  
  jam();
  Mutex mutex2(signal, c_mutexMgr, ptr.p->masterData.m_defineBackupMutex);
  jam();
  mutex2.unlock(); // ignore response

  if (!controlledAbort) {
    jam();
    if (s == DEFINING) {
      jam();
//-------------------------------------------------------
// If we are in the defining phase all work is done by
// slaves. No triggers have been allocated thus slaves
// may free all "Master" resources, let them know...
//-------------------------------------------------------
      sendAbortBackupOrd(signal, ptr, AbortBackupOrd::OkToClean);
      return;
    }//if
    if (s == DEFINED) {
      jam();
//-------------------------------------------------------
// DEFINED is the state when triggers are created. We rely
// on that DICT will report create trigger failure in case
// of node failure. Thus no special action is needed here.
// We will check for errorCode != 0 when receiving
// replies on create trigger.
//-------------------------------------------------------
      return;
    }//if
    if(ptr.p->masterData.gsn == GSN_WAIT_GCP_REQ) {
      jam();
      DEBUG_OUT("masterAbort: gsn = GSN_WAIT_GCP_REQ");
//-------------------------------------------------------
// We are waiting for WAIT_GCP response. We rely on
// this to arrive and check for ABORTING in response.
//-------------------------------------------------------

      /**
       * Waiting for GCP
       * @see execWAIT_GCP_CONF
       */ 
      return;
    }//if
  
    if(ptr.p->masterData.gsn == GSN_ALTER_TRIG_REQ) {
      jam();
      DEBUG_OUT("masterAbort: gsn = GSN_ALTER_TRIG_REQ");
//-------------------------------------------------------
// We are waiting for ALTER_TRIG response. We rely on
// this to arrive and check for ABORTING in response.
//-------------------------------------------------------

      /**
       * All triggers haven't been created yet
       */
      return;
    }//if
  
    if(ptr.p->masterData.gsn == GSN_DROP_TRIG_REQ) {
      jam();
      DEBUG_OUT("masterAbort: gsn = GSN_DROP_TRIG_REQ");
//-------------------------------------------------------
// We are waiting for DROP_TRIG response. We rely on
// this to arrive and will continue dropping triggers
// until completed.
//-------------------------------------------------------

      /**
       * I'm currently dropping the trigger
       */
      return;
    }//if
  }//if

//-------------------------------------------------------
// If we are waiting for START_BACKUP responses we can
// safely start dropping triggers (state == STARTED).
// We will ignore any START_BACKUP responses after this.
//-------------------------------------------------------
  DEBUG_OUT("masterAbort: sendDropTrig");
  sendDropTrig(signal, ptr); // dropping due to error
}

void
Backup::masterSendAbortBackup(Signal* signal, BackupRecordPtr ptr)
{
  if (ptr.p->masterData.state.getState() != ABORTING) {
    sendAbortBackupOrd(signal, ptr, AbortBackupOrd::BackupFailure);
    ptr.p->masterData.state.setState(ABORTING);
  }
  const State s = ptr.p->masterData.state.getAbortState();
  
  /**
   * First inform to client
   */
  if(s == DEFINING) {
    jam();
#ifdef DEBUG_ABORT
    ndbout_c("** Abort: sending BACKUP_REF to mgmtsrvr");
#endif
    sendBackupRef(ptr.p->clientRef, signal, ptr.p->clientData, 
		  ptr.p->errorCode);

  } else {
    jam();
#ifdef DEBUG_ABORT
    ndbout_c("** Abort: sending BACKUP_ABORT_REP to mgmtsrvr");
#endif
    BackupAbortRep* rep = (BackupAbortRep*)signal->getDataPtrSend();
    rep->backupId = ptr.p->backupId;
    rep->senderData = ptr.p->clientData;
    rep->reason = ptr.p->errorCode;
    sendSignal(ptr.p->clientRef, GSN_BACKUP_ABORT_REP, signal, 
	       BackupAbortRep::SignalLength, JBB);

    signal->theData[0] = EventReport::BackupAborted;
    signal->theData[1] = ptr.p->clientRef;
    signal->theData[2] = ptr.p->backupId;
    signal->theData[3] = ptr.p->errorCode;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 4, JBB);
  }//if
  
  //  ptr.p->masterData.state.setState(INITIAL);
  
  sendAbortBackupOrd(signal, ptr, AbortBackupOrd::BackupFailure);
}

/*****************************************************************************
 * 
 * Slave functionallity: Define Backup 
 *
 *****************************************************************************/
void
Backup::defineBackupRef(Signal* signal, BackupRecordPtr ptr, Uint32 errCode)
{
  if (ptr.p->slaveState.getState() == ABORTING) {
    jam();
    return;
  }
  ptr.p->slaveState.setState(ABORTING);

  if (errCode != 0) {
    jam();
    ptr.p->setErrorCode(errCode);
  }//if
  ndbrequire(ptr.p->errorCode != 0);

  DefineBackupRef* ref = (DefineBackupRef*)signal->getDataPtrSend();
  ref->backupId = ptr.p->backupId;
  ref->backupPtr = ptr.i;
  ref->errorCode = ptr.p->errorCode;
  sendSignal(ptr.p->masterRef, GSN_DEFINE_BACKUP_REF, signal, 
	     DefineBackupRef::SignalLength, JBB);

  closeFiles(signal, ptr);
}

void
Backup::execDEFINE_BACKUP_REQ(Signal* signal)
{
  jamEntry();

  DefineBackupReq* req = (DefineBackupReq*)signal->getDataPtr();
  
  BackupRecordPtr ptr;
  const Uint32 ptrI = req->backupPtr;
  const Uint32 backupId = req->backupId;
  const BlockReference senderRef = req->senderRef;

  if(senderRef == reference()){
    /**
     * Signal sent from myself -> record already seized
     */
    jam();
    c_backupPool.getPtr(ptr, ptrI);
  } else { // from other node
    jam();
#ifdef DEBUG_ABORT
    dumpUsedResources();
#endif
    if(!c_backups.seizeId(ptr, ptrI)) {
      jam();
      ndbrequire(false); // If master has succeeded slave should succed
    }//if
  }//if

  CRASH_INSERTION((10014));
  
  ptr.p->slaveState.forceState(INITIAL);
  ptr.p->slaveState.setState(DEFINING);
  ptr.p->errorCode = 0;
  ptr.p->clientRef = req->clientRef;
  ptr.p->clientData = req->clientData;
  ptr.p->masterRef = senderRef;
  ptr.p->nodes = req->nodes;
  ptr.p->backupId = backupId;
  ptr.p->backupKey[0] = req->backupKey[0];
  ptr.p->backupKey[1] = req->backupKey[1];
  ptr.p->backupDataLen = req->backupDataLen;
  ptr.p->masterData.dropTrig.tableId = RNIL;
  ptr.p->masterData.alterTrig.tableId = RNIL;
  ptr.p->noOfBytes = 0;
  ptr.p->noOfRecords = 0;
  ptr.p->noOfLogBytes = 0;
  ptr.p->noOfLogRecords = 0;
  ptr.p->currGCP = 0;
  
  /**
   * Allocate files
   */
  BackupFilePtr files[3];
  Uint32 noOfPages[] = {
    NO_OF_PAGES_META_FILE,
    2,   // 32k
    0    // 3M
  };
  const Uint32 maxInsert[] = {
    2048,  // Temporarily to solve TR515
    //25,      // 100 bytes
    2048,    // 4k
    16*3000, // Max 16 tuples
  };
  Uint32 minWrite[] = {
    8192,
    8192,
    32768
  };
  Uint32 maxWrite[] = {
    8192,
    8192,
    32768
  };
  
  minWrite[1] = c_defaults.m_minWriteSize;
  maxWrite[1] = c_defaults.m_maxWriteSize;
  noOfPages[1] = (c_defaults.m_logBufferSize + sizeof(Page32) - 1) / 
    sizeof(Page32);
  minWrite[2] = c_defaults.m_minWriteSize;
  maxWrite[2] = c_defaults.m_maxWriteSize;
  noOfPages[2] = (c_defaults.m_dataBufferSize + sizeof(Page32) - 1) / 
    sizeof(Page32);
  
  for(Uint32 i = 0; i<3; i++) {
    jam();
    if(!ptr.p->files.seize(files[i])) {
      jam();
      defineBackupRef(signal, ptr, 
		      DefineBackupRef::FailedToAllocateFileRecord);
      return;
    }//if

    files[i].p->tableId = RNIL;
    files[i].p->backupPtr = ptr.i;
    files[i].p->filePointer = RNIL;
    files[i].p->fileDone = 0;
    files[i].p->fileOpened = 0;
    files[i].p->fileRunning = 0;    
    files[i].p->scanRunning = 0;
    files[i].p->errorCode = 0;
    
    if(files[i].p->pages.seize(noOfPages[i]) == false) {
      jam();
      DEBUG_OUT("Failed to seize " << noOfPages[i] << " pages");
      defineBackupRef(signal, ptr, DefineBackupRef::FailedToAllocateBuffers);
      return;
    }//if
    Page32Ptr pagePtr;
    files[i].p->pages.getPtr(pagePtr, 0);
    
    const char * msg = files[i].p->
      operation.dataBuffer.setup((Uint32*)pagePtr.p, 
				 noOfPages[i] * (sizeof(Page32) >> 2),
				 128,
				 minWrite[i] >> 2,
				 maxWrite[i] >> 2,
				 maxInsert[i]);
    if(msg != 0) {
      jam();
      defineBackupRef(signal, ptr, DefineBackupRef::FailedToSetupFsBuffers);
      return;
    }//if
  }//for
  files[0].p->fileType = BackupFormat::CTL_FILE;
  files[1].p->fileType = BackupFormat::LOG_FILE;
  files[2].p->fileType = BackupFormat::DATA_FILE;
  
  ptr.p->ctlFilePtr = files[0].i;
  ptr.p->logFilePtr = files[1].i;
  ptr.p->dataFilePtr = files[2].i;
  
  if (!verifyNodesAlive(ptr.p->nodes)) {
    jam();
    defineBackupRef(signal, ptr, DefineBackupRef::Undefined);
    //  sendBackupRef(signal, ptr, 
    //                ptr.p->errorCode?ptr.p->errorCode:BackupRef::Undefined);
    return;
  }//if
  if (ERROR_INSERTED(10027)) {
    jam();
    defineBackupRef(signal, ptr, 327);
    //    sendBackupRef(signal, ptr, 327);
    return;
  }//if

  if(ptr.p->backupDataLen == 0) {
    jam();
    backupAllData(signal, ptr);
    return;
  }//if
  
  /**
   * Not implemented
   */
  ndbrequire(0);
}

void
Backup::backupAllData(Signal* signal, BackupRecordPtr ptr)
{
  /**
   * Get all tables from dict
   */
  ListTablesReq * req = (ListTablesReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->senderData = ptr.i;
  req->requestData = 0;
  sendSignal(DBDICT_REF, GSN_LIST_TABLES_REQ, signal, 
	     ListTablesReq::SignalLength, JBB);
}

void
Backup::execLIST_TABLES_CONF(Signal* signal)
{
  jamEntry();
  
  ListTablesConf* conf = (ListTablesConf*)signal->getDataPtr();

  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, conf->senderData);
  
  const Uint32 len = signal->length() - ListTablesConf::HeaderLength;
  for(unsigned int i = 0; i<len; i++) {
    jam();
    Uint32 tableId = ListTablesConf::getTableId(conf->tableData[i]);
    Uint32 tableType = ListTablesConf::getTableType(conf->tableData[i]);
    if (!DictTabInfo::isTable(tableType) && !DictTabInfo::isIndex(tableType)){
      jam();
      continue;
    }//if
    TablePtr tabPtr;
    ptr.p->tables.seize(tabPtr);
    if(tabPtr.i == RNIL) {
      jam();
      defineBackupRef(signal, ptr, DefineBackupRef::FailedToAllocateTables);
      return;
    }//if
    tabPtr.p->tableId = tableId;
    tabPtr.p->tableType = tableType;
  }//for
  
  if(len == ListTablesConf::DataLength) {
    jam();
    /**
     * Not finished...
     */
    return;
  }//if

  defineSlaveAbortCheck();

  /**
   * All tables fetched
   */
  openFiles(signal, ptr);
}

void
Backup::openFiles(Signal* signal, BackupRecordPtr ptr)
{
  jam();

  BackupFilePtr filePtr;

  FsOpenReq * req = (FsOpenReq *)signal->getDataPtrSend();
  req->userReference = reference();
  req->fileFlags = 
    FsOpenReq::OM_WRITEONLY | 
    FsOpenReq::OM_TRUNCATE |
    FsOpenReq::OM_CREATE | 
    FsOpenReq::OM_APPEND |
    FsOpenReq::OM_SYNC;
  FsOpenReq::v2_setCount(req->fileNumber, 0xFFFFFFFF);
  
  /**
   * Ctl file
   */
  c_backupFilePool.getPtr(filePtr, ptr.p->ctlFilePtr);
  ndbrequire(filePtr.p->fileRunning == 0);
  filePtr.p->fileRunning = 1;

  req->userPointer = filePtr.i;
  FsOpenReq::setVersion(req->fileNumber, 2);
  FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_CTL);
  FsOpenReq::v2_setSequence(req->fileNumber, ptr.p->backupId);
  FsOpenReq::v2_setNodeId(req->fileNumber, getOwnNodeId());
  sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, FsOpenReq::SignalLength, JBA);

  /**
   * Log file
   */
  c_backupFilePool.getPtr(filePtr, ptr.p->logFilePtr);
  ndbrequire(filePtr.p->fileRunning == 0);
  filePtr.p->fileRunning = 1;
  
  req->userPointer = filePtr.i;
  FsOpenReq::setVersion(req->fileNumber, 2);
  FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_LOG);
  FsOpenReq::v2_setSequence(req->fileNumber, ptr.p->backupId);
  FsOpenReq::v2_setNodeId(req->fileNumber, getOwnNodeId());
  sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, FsOpenReq::SignalLength, JBA);

  /**
   * Data file
   */
  c_backupFilePool.getPtr(filePtr, ptr.p->dataFilePtr);
  ndbrequire(filePtr.p->fileRunning == 0);
  filePtr.p->fileRunning = 1;

  req->userPointer = filePtr.i;
  FsOpenReq::setVersion(req->fileNumber, 2);
  FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_DATA);
  FsOpenReq::v2_setSequence(req->fileNumber, ptr.p->backupId);
  FsOpenReq::v2_setNodeId(req->fileNumber, getOwnNodeId());
  FsOpenReq::v2_setCount(req->fileNumber, 0);
  sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, FsOpenReq::SignalLength, JBA);
}

void
Backup::execFSOPENREF(Signal* signal)
{
  jamEntry();

  FsRef * ref = (FsRef *)signal->getDataPtr();
  
  const Uint32 userPtr = ref->userPointer;
  
  BackupFilePtr filePtr;
  c_backupFilePool.getPtr(filePtr, userPtr);
  
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, filePtr.p->backupPtr);
  ptr.p->setErrorCode(ref->errorCode);
  openFilesReply(signal, ptr, filePtr);
}

void
Backup::execFSOPENCONF(Signal* signal)
{
  jamEntry();
  
  FsConf * conf = (FsConf *)signal->getDataPtr();
  
  const Uint32 userPtr = conf->userPointer;
  const Uint32 filePointer = conf->filePointer;
  
  BackupFilePtr filePtr;
  c_backupFilePool.getPtr(filePtr, userPtr);
  filePtr.p->filePointer = filePointer; 
  
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, filePtr.p->backupPtr);

  ndbrequire(filePtr.p->fileOpened == 0);
  filePtr.p->fileOpened = 1;
  openFilesReply(signal, ptr, filePtr);
}

void
Backup::openFilesReply(Signal* signal, 
		       BackupRecordPtr ptr, BackupFilePtr filePtr)
{
  jam();

  /**
   * Mark files as "opened"
   */
  ndbrequire(filePtr.p->fileRunning == 1);
  filePtr.p->fileRunning = 0;
  
  /**
   * Check if all files have recived open_reply
   */
  for(ptr.p->files.first(filePtr); filePtr.i!=RNIL;ptr.p->files.next(filePtr)) 
  {
    jam();
    if(filePtr.p->fileRunning == 1) {
      jam();
      return;
    }//if
  }//for

  defineSlaveAbortCheck();

  /**
   * Did open succeed for all files
   */
  if(ptr.p->checkError()) {
    jam();
    defineBackupRef(signal, ptr);
    return;
  }//if

  /**
   * Insert file headers
   */
  ptr.p->files.getPtr(filePtr, ptr.p->ctlFilePtr);
  if(!insertFileHeader(BackupFormat::CTL_FILE, ptr.p, filePtr.p)) {
    jam();
    defineBackupRef(signal, ptr, DefineBackupRef::FailedInsertFileHeader);
    return;
  }//if

  ptr.p->files.getPtr(filePtr, ptr.p->logFilePtr);
  if(!insertFileHeader(BackupFormat::LOG_FILE, ptr.p, filePtr.p)) {
    jam();
    defineBackupRef(signal, ptr, DefineBackupRef::FailedInsertFileHeader);
    return;
  }//if

  ptr.p->files.getPtr(filePtr, ptr.p->dataFilePtr);
  if(!insertFileHeader(BackupFormat::DATA_FILE, ptr.p, filePtr.p)) {
    jam();
    defineBackupRef(signal, ptr, DefineBackupRef::FailedInsertFileHeader);
    return;
  }//if

  /**
   * Start CTL file thread
   */
  ptr.p->files.getPtr(filePtr, ptr.p->ctlFilePtr);
  filePtr.p->fileRunning = 1;
  
  signal->theData[0] = BackupContinueB::START_FILE_THREAD;
  signal->theData[1] = ptr.p->ctlFilePtr;
  sendSignalWithDelay(BACKUP_REF, GSN_CONTINUEB, signal, 100, 2);

  /**
   * Insert table list in ctl file
   */
  FsBuffer & buf = filePtr.p->operation.dataBuffer;

  const Uint32 sz = 
    (sizeof(BackupFormat::CtlFile::TableList) >> 2) +
    ptr.p->tables.noOfElements() - 1;
  
  Uint32 * dst;
  ndbrequire(sz < buf.getMaxWrite());
  if(!buf.getWritePtr(&dst, sz)) {
    jam();
    defineBackupRef(signal, ptr, DefineBackupRef::FailedInsertTableList);
    return;
  }//if
  
  BackupFormat::CtlFile::TableList* tl = 
    (BackupFormat::CtlFile::TableList*)dst;
  tl->SectionType   = htonl(BackupFormat::TABLE_LIST);
  tl->SectionLength = htonl(sz);

  TablePtr tabPtr;
  Uint32 count = 0;
  for(ptr.p->tables.first(tabPtr); 
      tabPtr.i != RNIL;
      ptr.p->tables.next(tabPtr)){
    jam();
    tl->TableIds[count] = htonl(tabPtr.p->tableId);
    count++;
  }//for
  
  buf.updateWritePtr(sz);
  
  /**
   * Start getting table definition data
   */
  ndbrequire(ptr.p->tables.first(tabPtr));

  signal->theData[0] = BackupContinueB::BUFFER_FULL_META;
  signal->theData[1] = ptr.i;
  signal->theData[2] = tabPtr.i;
  sendSignalWithDelay(BACKUP_REF, GSN_CONTINUEB, signal, 100, 3);
  return;
}

bool
Backup::insertFileHeader(BackupFormat::FileType ft, 
			 BackupRecord * ptrP,
			 BackupFile * filePtrP){
  FsBuffer & buf = filePtrP->operation.dataBuffer;

  const Uint32 sz = sizeof(BackupFormat::FileHeader) >> 2;

  Uint32 * dst;
  ndbrequire(sz < buf.getMaxWrite());
  if(!buf.getWritePtr(&dst, sz)) {
    jam();
    return false;
  }//if
  
  BackupFormat::FileHeader* header = (BackupFormat::FileHeader*)dst;
  ndbrequire(sizeof(header->Magic) == sizeof(BACKUP_MAGIC));
  memcpy(header->Magic, BACKUP_MAGIC, sizeof(BACKUP_MAGIC));
  header->NdbVersion    = htonl(NDB_VERSION);
  header->SectionType   = htonl(BackupFormat::FILE_HEADER);
  header->SectionLength = htonl(sz - 3);
  header->FileType      = htonl(ft);
  header->BackupId      = htonl(ptrP->backupId);
  header->BackupKey_0   = htonl(ptrP->backupKey[0]);
  header->BackupKey_1   = htonl(ptrP->backupKey[1]);
  header->ByteOrder     = 0x12345678;
  
  buf.updateWritePtr(sz);
  return true;
}

void
Backup::execGET_TABINFOREF(Signal* signal)
{
  GetTabInfoRef * ref = (GetTabInfoRef*)signal->getDataPtr();
  
  const Uint32 senderData = ref->senderData;
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, senderData);

  defineSlaveAbortCheck();

  defineBackupRef(signal, ptr, ref->errorCode);
}

void
Backup::execGET_TABINFO_CONF(Signal* signal)
{
  jamEntry();

  if(!assembleFragments(signal)) {
    jam();
    return;
  }//if

  GetTabInfoConf * const conf = (GetTabInfoConf*)signal->getDataPtr();
  //const Uint32 senderRef = info->senderRef;
  const Uint32 len = conf->totalLen;
  const Uint32 senderData = conf->senderData;

  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, senderData);
  
  defineSlaveAbortCheck();

  SegmentedSectionPtr dictTabInfoPtr;
  signal->getSection(dictTabInfoPtr, GetTabInfoConf::DICT_TAB_INFO);
  ndbrequire(dictTabInfoPtr.sz == len);

  /**
   * No of pages needed
   */
  const Uint32 noPages = (len + sizeof(Page32) - 1) / sizeof(Page32);
  if(ptr.p->pages.getSize() < noPages) {
    jam();
    ptr.p->pages.release();
    if(ptr.p->pages.seize(noPages) == false) {
      jam();
      ptr.p->setErrorCode(DefineBackupRef::FailedAllocateTableMem);
      ndbrequire(false);
      releaseSections(signal);
      defineBackupRef(signal, ptr);
      return;
    }//if
  }//if
  
  BackupFilePtr filePtr;
  ptr.p->files.getPtr(filePtr, ptr.p->ctlFilePtr);
  FsBuffer & buf = filePtr.p->operation.dataBuffer;
  { // Write into ctl file
    Uint32* dst, dstLen = len + 2;
    if(!buf.getWritePtr(&dst, dstLen)) {
      jam();
      ndbrequire(false);
      ptr.p->setErrorCode(DefineBackupRef::FailedAllocateTableMem);
      releaseSections(signal);
      defineBackupRef(signal, ptr);
      return;
    }//if
    if(dst != 0) {
      jam();

      BackupFormat::CtlFile::TableDescription * desc = 
        (BackupFormat::CtlFile::TableDescription*)dst;
      desc->SectionType = htonl(BackupFormat::TABLE_DESCRIPTION);
      desc->SectionLength = htonl(len + 2);
      dst += 2;

      copy(dst, dictTabInfoPtr);
      buf.updateWritePtr(dstLen);
    }//if
  }
  
  ndbrequire(ptr.p->pages.getSize() >= noPages);
  Page32Ptr pagePtr;
  ptr.p->pages.getPtr(pagePtr, 0);
  copy(&pagePtr.p->data[0], dictTabInfoPtr);
  releaseSections(signal);
  
  if(ptr.p->checkError()) {
    jam();
    defineBackupRef(signal, ptr);
    return;
  }//if

  TablePtr tabPtr = parseTableDescription(signal, ptr, len);
  if(tabPtr.i == RNIL) {
    jam();
    defineBackupRef(signal, ptr);
    return;
  }//if

  TablePtr tmp = tabPtr;
  ptr.p->tables.next(tabPtr);
  if(DictTabInfo::isIndex(tmp.p->tableType)){
    ptr.p->tables.release(tmp);
  }
  
  if(tabPtr.i == RNIL) {
    jam();
    
    ptr.p->pages.release();
    
    ndbrequire(ptr.p->tables.first(tabPtr));
    signal->theData[0] = RNIL;
    signal->theData[1] = tabPtr.p->tableId;
    signal->theData[2] = ptr.i;
    sendSignal(DBDIH_REF, GSN_DI_FCOUNTREQ, signal, 3, JBB);
    return;
  }//if

  signal->theData[0] = BackupContinueB::BUFFER_FULL_META;
  signal->theData[1] = ptr.i;
  signal->theData[2] = tabPtr.i;
  sendSignalWithDelay(BACKUP_REF, GSN_CONTINUEB, signal, 100, 3);
  return;
}

Backup::TablePtr
Backup::parseTableDescription(Signal* signal, BackupRecordPtr ptr, Uint32 len)
{

  Page32Ptr pagePtr;
  ptr.p->pages.getPtr(pagePtr, 0);
  
  SimplePropertiesLinearReader it(&pagePtr.p->data[0], len);
  
  it.first();
  
  DictTabInfo::Table tmpTab; tmpTab.init();
  SimpleProperties::UnpackStatus stat;
  stat = SimpleProperties::unpack(it, &tmpTab, 
				  DictTabInfo::TableMapping, 
				  DictTabInfo::TableMappingSize, 
				  true, true);
  ndbrequire(stat == SimpleProperties::Break);
  
  TablePtr tabPtr;
  ndbrequire(findTable(ptr, tabPtr, tmpTab.TableId));
  if(DictTabInfo::isIndex(tabPtr.p->tableType)){
    jam();
    return tabPtr;
  }
  
  /**
   * Initialize table object
   */
  tabPtr.p->frag_mask = RNIL;

  tabPtr.p->schemaVersion = tmpTab.TableVersion;
  tabPtr.p->noOfAttributes = tmpTab.NoOfAttributes;
  tabPtr.p->noOfKeys = tmpTab.NoOfKeyAttr;
  tabPtr.p->noOfNull = 0;
  tabPtr.p->noOfVariable = 0; // Computed while iterating over attribs
  tabPtr.p->sz_FixedKeys = 0; // Computed while iterating over attribs
  tabPtr.p->sz_FixedAttributes = 0; // Computed while iterating over attribs
  tabPtr.p->variableKeyId = RNIL;   // Computed while iterating over attribs
  tabPtr.p->triggerIds[0] = ILLEGAL_TRIGGER_ID;
  tabPtr.p->triggerIds[1] = ILLEGAL_TRIGGER_ID;
  tabPtr.p->triggerIds[2] = ILLEGAL_TRIGGER_ID;
  tabPtr.p->triggerAllocated[0] = false;
  tabPtr.p->triggerAllocated[1] = false;
  tabPtr.p->triggerAllocated[2] = false;

  if(tabPtr.p->attributes.seize(tabPtr.p->noOfAttributes) == false) {
    jam();
    ptr.p->setErrorCode(DefineBackupRef::FailedToAllocateAttributeRecord);
    tabPtr.i = RNIL;
    return tabPtr;
  }//if
  
  const Uint32 count = tabPtr.p->noOfAttributes;
  for(Uint32 i = 0; i<count; i++) {
    jam();
    DictTabInfo::Attribute tmp; tmp.init();
    stat = SimpleProperties::unpack(it, &tmp, 
				    DictTabInfo::AttributeMapping, 
				    DictTabInfo::AttributeMappingSize,
				    true, true);
    
    ndbrequire(stat == SimpleProperties::Break);

    const Uint32 arr = tmp.AttributeArraySize;
    const Uint32 sz = 1 << tmp.AttributeSize;
    const Uint32 sz32 = (sz * arr + 31) >> 5;

    AttributePtr attrPtr;
    tabPtr.p->attributes.getPtr(attrPtr, tmp.AttributeId);
    
    attrPtr.p->data.nullable = tmp.AttributeNullableFlag;
    attrPtr.p->data.fixed = (tmp.AttributeArraySize != 0);
    attrPtr.p->data.key = tmp.AttributeKeyFlag;
    attrPtr.p->data.sz32 = sz32;

    /**
     * Either
     * 1) Fixed
     * 2) Nullable
     * 3) Variable
     * 4) Fixed key
     * 5) Variable key
     */
    if(attrPtr.p->data.key == false) {
      jam();
      
      if(attrPtr.p->data.fixed == true && attrPtr.p->data.nullable == false) {
	jam();
	attrPtr.p->data.offset = tabPtr.p->sz_FixedAttributes;
	tabPtr.p->sz_FixedAttributes += sz32;
      }//if

      if(attrPtr.p->data.fixed == true && attrPtr.p->data.nullable == true) {
	jam();
	attrPtr.p->data.offset = 0;

	attrPtr.p->data.offsetNull = tabPtr.p->noOfNull;
	tabPtr.p->noOfNull++;
	tabPtr.p->noOfVariable++;
      }//if
      
      if(attrPtr.p->data.fixed == false) {
	jam();
	tabPtr.p->noOfVariable++;
	ndbrequire(0);
      }//if
      
    } else if(attrPtr.p->data.key == true) {
      jam();
      ndbrequire(attrPtr.p->data.nullable == false);
      
      if(attrPtr.p->data.fixed == true) { // Fixed key
	jam();
	tabPtr.p->sz_FixedKeys += sz32;
      }//if
      
      if(attrPtr.p->data.fixed == false) { // Variable key
	jam();
	attrPtr.p->data.offset = 0;
	tabPtr.p->noOfVariable++;
	ndbrequire(tabPtr.p->variableKeyId == RNIL); // Only one variable key
	tabPtr.p->variableKeyId = attrPtr.i;
	ndbrequire(0);
      }//if
    }//if
    
    it.next(); // Move Past EndOfAttribute
  }//for
  return tabPtr;
}

void
Backup::execDI_FCOUNTCONF(Signal* signal)
{
  jamEntry();
  
  const Uint32 userPtr = signal->theData[0];
  const Uint32 fragCount = signal->theData[1];
  const Uint32 tableId = signal->theData[2];
  const Uint32 senderData = signal->theData[3];

  ndbrequire(userPtr == RNIL && signal->length() == 5);
  
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, senderData);

  defineSlaveAbortCheck();

  TablePtr tabPtr;
  ndbrequire(findTable(ptr, tabPtr, tableId));
  
  ndbrequire(tabPtr.p->fragments.seize(fragCount) != false);
  tabPtr.p->frag_mask = calculate_frag_mask(fragCount);
  for(Uint32 i = 0; i<fragCount; i++) {
    jam();
    FragmentPtr fragPtr;
    tabPtr.p->fragments.getPtr(fragPtr, i);
    fragPtr.p->scanned = 0;
    fragPtr.p->scanning = 0;
    fragPtr.p->tableId = tableId;
    fragPtr.p->node = RNIL;
  }//for
  
  /**
   * Next table
   */
  if(ptr.p->tables.next(tabPtr)) {
    jam();
    signal->theData[0] = RNIL;
    signal->theData[1] = tabPtr.p->tableId;
    signal->theData[2] = ptr.i;
    sendSignal(DBDIH_REF, GSN_DI_FCOUNTREQ, signal, 3, JBB);    
    return;
  }//if
  
  ptr.p->tables.first(tabPtr);
  getFragmentInfo(signal, ptr, tabPtr, 0);
}

void
Backup::getFragmentInfo(Signal* signal, 
			BackupRecordPtr ptr, TablePtr tabPtr, Uint32 fragNo)
{
  jam();
  
  for(; tabPtr.i != RNIL; ptr.p->tables.next(tabPtr)) {
    jam();
    const Uint32 fragCount = tabPtr.p->fragments.getSize();
    for(; fragNo < fragCount; fragNo ++) {
      jam();
      FragmentPtr fragPtr;
      tabPtr.p->fragments.getPtr(fragPtr, fragNo);
      
      if(fragPtr.p->scanned == 0 && fragPtr.p->scanning == 0) {
	jam();
	signal->theData[0] = RNIL;
	signal->theData[1] = ptr.i;
	signal->theData[2] = tabPtr.p->tableId;
	signal->theData[3] = fragNo;
	sendSignal(DBDIH_REF, GSN_DIGETPRIMREQ, signal, 4, JBB);
	return;
      }//if
    }//for
    fragNo = 0;
  }//for
  
  getFragmentInfoDone(signal, ptr);
}

void
Backup::execDIGETPRIMCONF(Signal* signal)
{
  jamEntry();
  
  const Uint32 userPtr = signal->theData[0];
  const Uint32 senderData = signal->theData[1];
  const Uint32 nodeCount = signal->theData[6];
  const Uint32 tableId = signal->theData[7];
  const Uint32 fragNo = signal->theData[8];

  ndbrequire(userPtr == RNIL && signal->length() == 9);
  ndbrequire(nodeCount > 0 && nodeCount <= MAX_REPLICAS);
  
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, senderData);

  defineSlaveAbortCheck();

  TablePtr tabPtr;
  ndbrequire(findTable(ptr, tabPtr, tableId));

  FragmentPtr fragPtr;
  tabPtr.p->fragments.getPtr(fragPtr, fragNo);

  fragPtr.p->node = signal->theData[2];

  getFragmentInfo(signal, ptr, tabPtr, fragNo + 1);
}

void
Backup::getFragmentInfoDone(Signal* signal, BackupRecordPtr ptr)
{
  // Slave must now hold on to master data until 
  // AbortBackupOrd::OkToClean signal
  ptr.p->okToCleanMaster = false;
  ptr.p->slaveState.setState(DEFINED);
  DefineBackupConf * conf = (DefineBackupConf*)signal->getDataPtr();
  conf->backupPtr = ptr.i;
  conf->backupId = ptr.p->backupId;
  sendSignal(ptr.p->masterRef, GSN_DEFINE_BACKUP_CONF, signal,
	     DefineBackupConf::SignalLength, JBB);
}


/*****************************************************************************
 * 
 * Slave functionallity: Start backup
 *
 *****************************************************************************/
void
Backup::execSTART_BACKUP_REQ(Signal* signal)
{
  jamEntry();

  CRASH_INSERTION((10015));
  
  StartBackupReq* req = (StartBackupReq*)signal->getDataPtr();
  const Uint32 ptrI = req->backupPtr;
  const Uint32 backupId = req->backupId;
  const Uint32 signalNo = req->signalNo;

  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);

  slaveAbortCheck(); // macro will do return if ABORTING
  
  ptr.p->slaveState.setState(STARTED);
  
  for(Uint32 i = 0; i<req->noOfTableTriggers; i++) {
    jam();
    TablePtr tabPtr;
    ndbrequire(findTable(ptr, tabPtr, req->tableTriggers[i].tableId));
    for(Uint32 j = 0; j<3; j++) {
      jam();
      const Uint32 triggerId = req->tableTriggers[i].triggerIds[j];
      tabPtr.p->triggerIds[j] = triggerId;
      
      TriggerPtr trigPtr;
      if(!ptr.p->triggers.seizeId(trigPtr, triggerId)) {
        jam();
	StartBackupRef* ref = (StartBackupRef*)signal->getDataPtrSend();
	ref->backupPtr = ptr.i;
	ref->backupId = ptr.p->backupId;
	ref->signalNo = signalNo;
	ref->errorCode = StartBackupRef::FailedToAllocateTriggerRecord;
	sendSignal(ptr.p->masterRef, GSN_START_BACKUP_REF, signal,
		   StartBackupRef::SignalLength, JBB);
	return;
      }//if

      tabPtr.p->triggerAllocated[i] = true;
      trigPtr.p->backupPtr = ptr.i;
      trigPtr.p->tableId = tabPtr.p->tableId;
      trigPtr.p->tab_ptr_i = tabPtr.i;
      trigPtr.p->logEntry = 0;
      trigPtr.p->event = j;
      trigPtr.p->maxRecordSize = 2048;
      trigPtr.p->operation = 
	&ptr.p->files.getPtr(ptr.p->logFilePtr)->operation;
      trigPtr.p->operation->noOfBytes = 0;
      trigPtr.p->operation->noOfRecords = 0;
      trigPtr.p->errorCode = 0;
    }//for
  }//for
  
  /**
   * Start file threads...
   */
  BackupFilePtr filePtr;
  for(ptr.p->files.first(filePtr); 
      filePtr.i!=RNIL; 
      ptr.p->files.next(filePtr)){
    jam();
    if(filePtr.p->fileRunning == 0) {
      jam();
      filePtr.p->fileRunning = 1;
      signal->theData[0] = BackupContinueB::START_FILE_THREAD;
      signal->theData[1] = filePtr.i;
      sendSignalWithDelay(BACKUP_REF, GSN_CONTINUEB, signal, 100, 2);
    }//if
  }//for
  
  StartBackupConf* conf = (StartBackupConf*)signal->getDataPtrSend();
  conf->backupPtr = ptr.i;
  conf->backupId = ptr.p->backupId;
  conf->signalNo = signalNo;
  sendSignal(ptr.p->masterRef, GSN_START_BACKUP_CONF, signal,
	     StartBackupConf::SignalLength, JBB);
}

/*****************************************************************************
 * 
 * Slave functionallity: Backup fragment
 *
 *****************************************************************************/
void
Backup::execBACKUP_FRAGMENT_REQ(Signal* signal)
{
  jamEntry();
  BackupFragmentReq* req = (BackupFragmentReq*)signal->getDataPtr();

  CRASH_INSERTION((10016));

  const Uint32 ptrI = req->backupPtr;
  const Uint32 backupId = req->backupId;
  const Uint32 tableId = req->tableId;
  const Uint32 fragNo = req->fragmentNo;
  const Uint32 count = req->count;
  
  /**
   * Get backup record
   */
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);

  slaveAbortCheck(); // macro will do return if ABORTING
  
  ptr.p->slaveState.setState(SCANNING);
  
  /**
   * Get file
   */
  BackupFilePtr filePtr;
  c_backupFilePool.getPtr(filePtr, ptr.p->dataFilePtr);
  
  ndbrequire(filePtr.p->backupPtr == ptrI);
  ndbrequire(filePtr.p->fileOpened == 1);
  ndbrequire(filePtr.p->fileRunning == 1);
  ndbrequire(filePtr.p->scanRunning == 0);
  ndbrequire(filePtr.p->fileDone == 0);
  
  /**
   * Get table
   */
  TablePtr tabPtr;
  ndbrequire(findTable(ptr, tabPtr, tableId));

  /**
   * Get fragment
   */
  FragmentPtr fragPtr;
  tabPtr.p->fragments.getPtr(fragPtr, fragNo);

  ndbrequire(fragPtr.p->scanned == 0);
  ndbrequire(fragPtr.p->scanning == 0 || 
	     refToNode(ptr.p->masterRef) == getOwnNodeId());
  
  /**
   * Init operation
   */
  if(filePtr.p->tableId != tableId) {
    jam();
    filePtr.p->operation.init(tabPtr);
    filePtr.p->tableId = tableId;
  }//if
  
  /**
   * Check for space in buffer
   */
  if(!filePtr.p->operation.newFragment(tableId, fragNo)) {
    jam();
    req->count = count + 1;
    sendSignalWithDelay(BACKUP_REF, GSN_BACKUP_FRAGMENT_REQ, signal, 50,
			signal->length());
    ptr.p->slaveState.setState(STARTED);
    return;
  }//if
  
  /**
   * Mark things as "in use"
   */
  fragPtr.p->scanning = 1;
  filePtr.p->fragmentNo = fragNo;

  /**
   * Start scan
   */
  {
    filePtr.p->scanRunning = 1;
    
    Table & table = * tabPtr.p;
    ScanFragReq * req = (ScanFragReq *)signal->getDataPtrSend();
    const Uint32 parallelism = 16;
    const Uint32 attrLen = 5 + table.noOfAttributes - table.noOfKeys;

    req->senderData = filePtr.i;
    req->resultRef = reference();
    req->schemaVersion = table.schemaVersion;
    req->fragmentNo = fragNo;
    req->requestInfo = 0;
    req->savePointId = 0;
    req->tableId = table.tableId;
    ScanFragReq::setLockMode(req->requestInfo, 0);
    ScanFragReq::setHoldLockFlag(req->requestInfo, 0);
    ScanFragReq::setKeyinfoFlag(req->requestInfo, 1);
    ScanFragReq::setAttrLen(req->requestInfo,attrLen); 
    req->transId1 = 0;
    req->transId2 = (BACKUP << 20) + (getOwnNodeId() << 8);
    req->clientOpPtr= filePtr.i;
    req->batch_size_rows= 16;
    req->batch_size_bytes= 0;
    sendSignal(DBLQH_REF, GSN_SCAN_FRAGREQ, signal,
               ScanFragReq::SignalLength, JBB);
    
    signal->theData[0] = filePtr.i;
    signal->theData[1] = 0;
    signal->theData[2] = (BACKUP << 20) + (getOwnNodeId() << 8);
    
    // Return all
    signal->theData[3] = table.noOfAttributes - table.noOfKeys;
    signal->theData[4] = 0;
    signal->theData[5] = 0;
    signal->theData[6] = 0;
    signal->theData[7] = 0;
    
    Uint32 dataPos = 8;
    Uint32 i;
    for(i = 0; i<table.noOfAttributes; i++) {
      jam();
      AttributePtr attr;
      table.attributes.getPtr(attr, i);
      if(attr.p->data.key != 0) {
	jam();
	continue;
      }//if
      
      AttributeHeader::init(&signal->theData[dataPos], i, 0);
      dataPos++;
      if(dataPos == 25) {
        jam();
	sendSignal(DBLQH_REF, GSN_ATTRINFO, signal, 25, JBB);
	dataPos = 3;
      }//if
    }//for
    if(dataPos != 3) {
      jam();
      sendSignal(DBLQH_REF, GSN_ATTRINFO, signal, dataPos, JBB);
    }//if
  }
}

void
Backup::execSCAN_HBREP(Signal* signal)
{
  jamEntry();
}

void
Backup::execTRANSID_AI(Signal* signal)
{
  jamEntry();

  const Uint32 filePtrI = signal->theData[0];
  //const Uint32 transId1 = signal->theData[1];
  //const Uint32 transId2 = signal->theData[2];
  const Uint32 dataLen  = signal->length() - 3;
  
  BackupFilePtr filePtr;
  c_backupFilePool.getPtr(filePtr, filePtrI);

  OperationRecord & op = filePtr.p->operation;
  
  TablePtr tabPtr;
  c_tablePool.getPtr(tabPtr, op.tablePtr);
  
  Table & table = * tabPtr.p;
  
  /**
   * Unpack data
   */
  op.attrSzTotal += dataLen;

  Uint32 srcSz = dataLen;
  const Uint32 * src = &signal->theData[3];

  Uint32 * dst = op.dst;
  Uint32 dstSz = op.attrSzLeft;
  
  while(srcSz > 0) {
    jam();

    if(dstSz == 0) {
      jam();

      /**
       * Finished with one attribute now find next
       */
      const AttributeHeader attrHead(* src);
      const Uint32 attrId = attrHead.getAttributeId();
      const bool null = attrHead.isNULL();
      const Attribute::Data attr = table.attributes.getPtr(attrId)->data;
      
      srcSz -= attrHead.getHeaderSize();
      src   += attrHead.getHeaderSize();
      
      if(null) {
	jam();
	ndbrequire(attr.nullable);
	op.nullAttribute(attr.offsetNull);
	dstSz = 0;
	continue;
      }//if
      
      dstSz = attrHead.getDataSize();
      ndbrequire(dstSz == attr.sz32);
      if(attr.fixed && ! attr.nullable) {
	jam();
	dst = op.newAttrib(attr.offset, dstSz);
      } else if (attr.fixed && attr.nullable) {
	jam();
	dst = op.newNullable(attrId, dstSz);
      } else {
	ndbrequire(false);
	//dst = op.newVariable(attrId, attrSize);
      }//if
    }//if
    
    const Uint32 szCopy = (dstSz > srcSz) ? srcSz : dstSz;
    memcpy(dst, src, (szCopy << 2));

    srcSz -= szCopy;
    dstSz -= szCopy;
    src   += szCopy;
    dst   += szCopy;
  }//while
  op.dst        = dst;
  op.attrSzLeft = dstSz;
  
  if(op.finished()){
    jam();
    op.newRecord(op.dst);
  }
}

void
Backup::execKEYINFO20(Signal* signal)
{
  jamEntry();
  
  const Uint32 filePtrI = signal->theData[0];
  const Uint32 keyLen   = signal->theData[1];
  //const Uint32 scanInfo = signal->theData[2];
  //const Uint32 transId1 = signal->theData[3];
  //const Uint32 transId2 = signal->theData[4];
  const Uint32 dataLen  = signal->length() - 5;

  BackupFilePtr filePtr;
  c_backupFilePool.getPtr(filePtr, filePtrI);
  
  OperationRecord & op = filePtr.p->operation;
  
  /**
   * Unpack data
   */
  ndbrequire(keyLen == dataLen);
  const Uint32 * src = &signal->theData[5];
  const Uint32 klFixed = op.getFixedKeySize();
  ndbrequire(keyLen >= klFixed);
  
  Uint32 * dst = op.newKey();
  memcpy(dst, src, klFixed << 2);
  
  const Uint32 szLeft = (keyLen - klFixed);
  if(szLeft > 0) {
    jam();
    src += klFixed;
    dst = op.newVariableKey(szLeft);
    memcpy(dst, src, (szLeft << 2));
    ndbrequire(0);
  }//if
  
  if(op.finished()){
    jam();
    op.newRecord(op.dst);
  }
}

void 
Backup::OperationRecord::init(const TablePtr & ptr)
{
  
  tablePtr = ptr.i;
  noOfAttributes = (ptr.p->noOfAttributes - ptr.p->noOfKeys) + 1;
  variableKeyId = ptr.p->variableKeyId;
  
  sz_Bitmask = (ptr.p->noOfNull + 31) >> 5;
  sz_FixedKeys = ptr.p->sz_FixedKeys;
  sz_FixedAttribs = ptr.p->sz_FixedAttributes;

  if(ptr.p->noOfVariable == 0) {
    jam();
    maxRecordSize = 1 + sz_Bitmask + sz_FixedKeys + sz_FixedAttribs;
  } else {
    jam();
    maxRecordSize = 
      1 + sz_Bitmask + 2048 /* Max tuple size */ + 2 * ptr.p->noOfVariable;
  }//if
}

bool
Backup::OperationRecord::newFragment(Uint32 tableId, Uint32 fragNo)
{
  Uint32 * tmp;
  const Uint32 headSz = (sizeof(BackupFormat::DataFile::FragmentHeader) >> 2);
  const Uint32 sz = headSz + 16 * maxRecordSize;
  
  ndbrequire(sz < dataBuffer.getMaxWrite());
  if(dataBuffer.getWritePtr(&tmp, sz)) {
    jam();
    BackupFormat::DataFile::FragmentHeader * head = 
      (BackupFormat::DataFile::FragmentHeader*)tmp;

    head->SectionType   = htonl(BackupFormat::FRAGMENT_HEADER);
    head->SectionLength = htonl(headSz);
    head->TableId       = htonl(tableId);
    head->FragmentNo    = htonl(fragNo);
    head->ChecksumType  = htonl(0);

    opNoDone = opNoConf = opLen = 0;
    newRecord(tmp + headSz);
    scanStart = tmp;
    scanStop  = (tmp + headSz);
    
    noOfRecords = 0;
    noOfBytes = 0;
    return true;
  }//if
  return false;
}

bool
Backup::OperationRecord::fragComplete(Uint32 tableId, Uint32 fragNo)
{
  Uint32 * tmp;
  const Uint32 footSz = sizeof(BackupFormat::DataFile::FragmentFooter) >> 2;

  if(dataBuffer.getWritePtr(&tmp, footSz + 1)) {
    jam();
    * tmp = 0; // Finish record stream
    tmp++;
    BackupFormat::DataFile::FragmentFooter * foot = 
      (BackupFormat::DataFile::FragmentFooter*)tmp;
    foot->SectionType   = htonl(BackupFormat::FRAGMENT_FOOTER);
    foot->SectionLength = htonl(footSz);
    foot->TableId       = htonl(tableId);
    foot->FragmentNo    = htonl(fragNo);
    foot->NoOfRecords   = htonl(noOfRecords);
    foot->Checksum      = htonl(0);
    dataBuffer.updateWritePtr(footSz + 1);
    return true;
  }//if
  return false;
}

bool
Backup::OperationRecord::newScan()
{
  Uint32 * tmp;
  ndbrequire(16 * maxRecordSize < dataBuffer.getMaxWrite());
  if(dataBuffer.getWritePtr(&tmp, 16 * maxRecordSize)) {
    jam();
    opNoDone = opNoConf = opLen = 0;
    newRecord(tmp);
    scanStart = tmp;
    scanStop = tmp;
    return true;
  }//if
  return false;
}

bool 
Backup::OperationRecord::scanConf(Uint32 noOfOps, Uint32 total_len)
{
  const Uint32 done = opNoDone-opNoConf;
  
  ndbrequire(noOfOps == done);
  ndbrequire(opLen == total_len);
  opNoConf = opNoDone;
  
  const Uint32 len = (scanStop - scanStart);
  ndbrequire(len < dataBuffer.getMaxWrite());
  dataBuffer.updateWritePtr(len);
  noOfBytes += (len << 2);
  return true;
}

void
Backup::execSCAN_FRAGREF(Signal* signal)
{
  jamEntry();

  ScanFragRef * ref = (ScanFragRef*)signal->getDataPtr();
  
  const Uint32 filePtrI = ref->senderData;
  BackupFilePtr filePtr;
  c_backupFilePool.getPtr(filePtr, filePtrI);
  
  filePtr.p->errorCode = ref->errorCode;
  
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, filePtr.p->backupPtr);
  
  abortFile(signal, ptr, filePtr);
}

void
Backup::execSCAN_FRAGCONF(Signal* signal)
{
  jamEntry();

  CRASH_INSERTION((10017));

  ScanFragConf * conf = (ScanFragConf*)signal->getDataPtr();
  
  const Uint32 filePtrI = conf->senderData;
  BackupFilePtr filePtr;
  c_backupFilePool.getPtr(filePtr, filePtrI);

  OperationRecord & op = filePtr.p->operation;
  
  op.scanConf(conf->completedOps, conf->total_len);
  const Uint32 completed = conf->fragmentCompleted;
  if(completed != 2) {
    jam();
    
    checkScan(signal, filePtr);
    return;
  }//if

  fragmentCompleted(signal, filePtr);
}

void
Backup::fragmentCompleted(Signal* signal, BackupFilePtr filePtr)
{
  jam();

  if(filePtr.p->errorCode != 0){
    jam();    
    abortFileHook(signal, filePtr, true); // Scan completed
    return;
  }//if
    
  OperationRecord & op = filePtr.p->operation;
  if(!op.fragComplete(filePtr.p->tableId, filePtr.p->fragmentNo)) {
    jam();
    signal->theData[0] = BackupContinueB::BUFFER_FULL_FRAG_COMPLETE;
    signal->theData[1] = filePtr.i;
    sendSignalWithDelay(BACKUP_REF, GSN_CONTINUEB, signal, 50, 2);
    return;
  }//if
  
  filePtr.p->scanRunning = 0;
  
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, filePtr.p->backupPtr);

  BackupFragmentConf * conf = (BackupFragmentConf*)signal->getDataPtrSend();
  conf->backupId = ptr.p->backupId;
  conf->backupPtr = ptr.i;
  conf->tableId = filePtr.p->tableId;
  conf->fragmentNo = filePtr.p->fragmentNo;
  conf->noOfRecords = op.noOfRecords;
  conf->noOfBytes = op.noOfBytes;
  sendSignal(ptr.p->masterRef, GSN_BACKUP_FRAGMENT_CONF, signal,
	     BackupFragmentConf::SignalLength, JBB);
  
  ptr.p->slaveState.setState(STARTED);
  return;
}
 
void
Backup::checkScan(Signal* signal, BackupFilePtr filePtr)
{  
  if(filePtr.p->errorCode != 0){
    jam();
    abortFileHook(signal, filePtr, false); // Scan not completed
    return;
  }//if

  OperationRecord & op = filePtr.p->operation;
  if(op.newScan()) {
    jam();
    
    ScanFragNextReq * req = (ScanFragNextReq *)signal->getDataPtrSend();
    req->senderData = filePtr.i;
    req->closeFlag = 0;
    req->transId1 = 0;
    req->transId2 = (BACKUP << 20) + (getOwnNodeId() << 8);
    req->batch_size_rows= 16;
    req->batch_size_bytes= 0;
    sendSignal(DBLQH_REF, GSN_SCAN_NEXTREQ, signal, 
	       ScanFragNextReq::SignalLength, JBB);
    return;
  }//if
  
  signal->theData[0] = BackupContinueB::BUFFER_FULL_SCAN;
  signal->theData[1] = filePtr.i;
  sendSignalWithDelay(BACKUP_REF, GSN_CONTINUEB, signal, 50, 2);
}

void
Backup::execFSAPPENDREF(Signal* signal)
{
  jamEntry();
  
  FsRef * ref = (FsRef *)signal->getDataPtr();

  const Uint32 filePtrI = ref->userPointer;
  const Uint32 errCode = ref->errorCode;
  
  BackupFilePtr filePtr;
  c_backupFilePool.getPtr(filePtr, filePtrI);

  filePtr.p->fileRunning = 0;  
  filePtr.p->errorCode = errCode;
  
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, filePtr.p->backupPtr);
  
  abortFile(signal, ptr, filePtr);
}

void
Backup::execFSAPPENDCONF(Signal* signal)
{
  jamEntry();
  
  CRASH_INSERTION((10018));

  //FsConf * conf = (FsConf*)signal->getDataPtr();
  const Uint32 filePtrI = signal->theData[0]; //conf->userPointer;
  const Uint32 bytes = signal->theData[1]; //conf->bytes;
  
  BackupFilePtr filePtr;
  c_backupFilePool.getPtr(filePtr, filePtrI);

  if (ERROR_INSERTED(10029)) {
    BackupRecordPtr ptr;
    c_backupPool.getPtr(ptr, filePtr.p->backupPtr);
    abortFile(signal, ptr, filePtr);
  }//if
  
  OperationRecord & op = filePtr.p->operation;
  
  op.dataBuffer.updateReadPtr(bytes >> 2);

  checkFile(signal, filePtr);
}

void
Backup::checkFile(Signal* signal, BackupFilePtr filePtr)
{

#ifdef DEBUG_ABORT
  //  ndbout_c("---- check file filePtr.i = %u", filePtr.i);
#endif

  OperationRecord & op = filePtr.p->operation;

  Uint32 * tmp, sz; bool eof;
  if(op.dataBuffer.getReadPtr(&tmp, &sz, &eof)) {
    jam();
    
    if(filePtr.p->errorCode == 0) {
      jam();
      FsAppendReq * req = (FsAppendReq *)signal->getDataPtrSend();
      req->filePointer   = filePtr.p->filePointer;
      req->userPointer   = filePtr.i;
      req->userReference = reference();
      req->varIndex      = 0;
      req->offset        = tmp - c_startOfPages;
      req->size          = sz;

      sendSignal(NDBFS_REF, GSN_FSAPPENDREQ, signal, 
		 FsAppendReq::SignalLength, JBA);
      return;
    } else {
      jam();
      if (filePtr.p->scanRunning == 1)
	eof = false;
    }//if
  }//if
  
  if(!eof) {
    jam();
    signal->theData[0] = BackupContinueB::BUFFER_UNDERFLOW;
    signal->theData[1] = filePtr.i;
    sendSignalWithDelay(BACKUP_REF, GSN_CONTINUEB, signal, 50, 2);
    return;
  }//if
  
  ndbrequire(filePtr.p->fileDone == 1);
  
  if(sz > 0 && filePtr.p->errorCode == 0) {
    jam();
    FsAppendReq * req = (FsAppendReq *)signal->getDataPtrSend();
    req->filePointer   = filePtr.p->filePointer;
    req->userPointer   = filePtr.i;
    req->userReference = reference();
    req->varIndex      = 0;
    req->offset        = tmp - c_startOfPages;
    req->size          = sz; // Avrunda uppot
    
    sendSignal(NDBFS_REF, GSN_FSAPPENDREQ, signal, 
	       FsAppendReq::SignalLength, JBA);
    return;
  }//if
  
  filePtr.p->fileRunning = 0;
  
  FsCloseReq * req = (FsCloseReq *)signal->getDataPtrSend();
  req->filePointer = filePtr.p->filePointer;
  req->userPointer = filePtr.i;
  req->userReference = reference();
  req->fileFlag = 0;
#ifdef DEBUG_ABORT
  ndbout_c("***** FSCLOSEREQ filePtr.i = %u", filePtr.i);
#endif
  sendSignal(NDBFS_REF, GSN_FSCLOSEREQ, signal, FsCloseReq::SignalLength, JBA);
}

void
Backup::abortFile(Signal* signal, BackupRecordPtr ptr, BackupFilePtr filePtr)
{
  jam();
  
  if(ptr.p->slaveState.getState() != ABORTING) {
    /**
     * Inform master of failure
     */
    jam();
    ptr.p->slaveState.setState(ABORTING);
    ptr.p->setErrorCode(AbortBackupOrd::FileOrScanError);
    sendAbortBackupOrdSlave(signal, ptr, AbortBackupOrd::FileOrScanError);
    return;
  }//if

  
  for(ptr.p->files.first(filePtr); 
      filePtr.i!=RNIL; 
      ptr.p->files.next(filePtr)){
    jam();
    filePtr.p->errorCode = 1;
  }//for
  
  closeFiles(signal, ptr);
}

void
Backup::abortFileHook(Signal* signal, BackupFilePtr filePtr, bool scanComplete)
{
  jam();
  
  if(!scanComplete) {
    jam();

    ScanFragNextReq * req = (ScanFragNextReq *)signal->getDataPtrSend();
    req->senderData = filePtr.i;
    req->closeFlag = 1;
    req->transId1 = 0;
    req->transId2 = (BACKUP << 20) + (getOwnNodeId() << 8);
    sendSignal(DBLQH_REF, GSN_SCAN_NEXTREQ, signal, 
	       ScanFragNextReq::SignalLength, JBB);
    return;
  }//if
  
  filePtr.p->scanRunning = 0;

  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, filePtr.p->backupPtr);

  filePtr.i = RNIL;
  abortFile(signal, ptr, filePtr);
}

/****************************************************************************
 * 
 * Slave functionallity: Perform logging
 *
 ****************************************************************************/
Uint32
Backup::calculate_frag_mask(Uint32 count)
{
  Uint32 mask = 1;
  while (mask < count) mask <<= 1;
  mask -= 1;
  return mask;
}

void
Backup::execBACKUP_TRIG_REQ(Signal* signal)
{
  /*
  TUP asks if this trigger is to be fired on this node.
  */
  TriggerPtr trigPtr;
  TablePtr tabPtr;
  FragmentPtr fragPtr;
  Uint32 trigger_id = signal->theData[0];
  Uint32 frag_id = signal->theData[1];
  Uint32 result;

  jamEntry();
  c_triggerPool.getPtr(trigPtr, trigger_id);
  c_tablePool.getPtr(tabPtr, trigPtr.p->tab_ptr_i);
  frag_id = frag_id & tabPtr.p->frag_mask;
  /*
  At the moment the fragment identity known by TUP is the
  actual fragment id but with possibly an extra bit set.
  This is due to that ACC splits the fragment. Thus fragment id 5 can
  here be either 5 or 13. Thus masking with 2 ** n - 1 where number of
  fragments <= 2 ** n will always provide a correct fragment id.
  */
  tabPtr.p->fragments.getPtr(fragPtr, frag_id);
  if (fragPtr.p->node != getOwnNodeId()) {
    jam();
    result = ZFALSE;
  } else {
    jam();
    result = ZTRUE;
  }//if
  signal->theData[0] = result;
}

void
Backup::execTRIG_ATTRINFO(Signal* signal) {
  jamEntry();

  CRASH_INSERTION((10019));

  TrigAttrInfo * trg = (TrigAttrInfo*)signal->getDataPtr();

  TriggerPtr trigPtr;
  c_triggerPool.getPtr(trigPtr, trg->getTriggerId());
  ndbrequire(trigPtr.p->event != ILLEGAL_TRIGGER_ID); // Online...
  
  if(trigPtr.p->errorCode != 0) {
    jam();
    return;
  }//if
  
  if(trg->getAttrInfoType() == TrigAttrInfo::BEFORE_VALUES) {
    jam();
    /**
     * Backup is doing REDO logging and don't need before values
     */
    return;
  }//if

  BackupFormat::LogFile::LogEntry * logEntry = trigPtr.p->logEntry;
  if(logEntry == 0) {
    jam();
    Uint32 * dst;
    FsBuffer & buf = trigPtr.p->operation->dataBuffer;
    ndbrequire(trigPtr.p->maxRecordSize <= buf.getMaxWrite());

    BackupRecordPtr ptr;
    c_backupPool.getPtr(ptr, trigPtr.p->backupPtr);
    if(!buf.getWritePtr(&dst, trigPtr.p->maxRecordSize)) {
      jam();
      trigPtr.p->errorCode = AbortBackupOrd::LogBufferFull;
      sendAbortBackupOrdSlave(signal, ptr, AbortBackupOrd::LogBufferFull);
      return;
    }//if
    if(trigPtr.p->operation->noOfBytes > 123 && ERROR_INSERTED(10030)) {
      jam();
      trigPtr.p->errorCode = AbortBackupOrd::LogBufferFull;
      sendAbortBackupOrdSlave(signal, ptr, AbortBackupOrd::LogBufferFull);
      return;
    }//if
    
    logEntry = (BackupFormat::LogFile::LogEntry *)dst;
    trigPtr.p->logEntry = logEntry;
    logEntry->Length       = 0;
    logEntry->TableId      = htonl(trigPtr.p->tableId);
    logEntry->TriggerEvent = htonl(trigPtr.p->event);
  } else {
    ndbrequire(logEntry->TableId == htonl(trigPtr.p->tableId));
    ndbrequire(logEntry->TriggerEvent == htonl(trigPtr.p->event));
  }//if
  
  const Uint32 pos = logEntry->Length; 
  const Uint32 dataLen = signal->length() - TrigAttrInfo::StaticLength;
  memcpy(&logEntry->Data[pos], trg->getData(), dataLen << 2);

  logEntry->Length = pos + dataLen;
}

void
Backup::execFIRE_TRIG_ORD(Signal* signal)
{
  jamEntry();
  FireTrigOrd* trg = (FireTrigOrd*)signal->getDataPtr();

  const Uint32 gci = trg->getGCI();
  const Uint32 trI = trg->getTriggerId();

  TriggerPtr trigPtr;
  c_triggerPool.getPtr(trigPtr, trI);
  
  ndbrequire(trigPtr.p->event != ILLEGAL_TRIGGER_ID);

  if(trigPtr.p->errorCode != 0) {
    jam();
    return;
  }//if

  ndbrequire(trigPtr.p->logEntry != 0);
  Uint32 len = trigPtr.p->logEntry->Length;

  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, trigPtr.p->backupPtr);
  if(gci != ptr.p->currGCP) {
    jam();

    trigPtr.p->logEntry->TriggerEvent = htonl(trigPtr.p->event | 0x10000);
    trigPtr.p->logEntry->Data[len] = htonl(gci);
    len ++;
    ptr.p->currGCP = gci;
  }//if
  
  len += (sizeof(BackupFormat::LogFile::LogEntry) >> 2) - 2;
  trigPtr.p->logEntry->Length = htonl(len);

  ndbrequire(len + 1 <= trigPtr.p->operation->dataBuffer.getMaxWrite());
  trigPtr.p->operation->dataBuffer.updateWritePtr(len + 1);
  trigPtr.p->logEntry = 0;
  
  trigPtr.p->operation->noOfBytes += (len + 1) << 2;
  trigPtr.p->operation->noOfRecords += 1;
}

void
Backup::sendAbortBackupOrdSlave(Signal* signal, BackupRecordPtr ptr, 
				Uint32 requestType)
{
  jam();
  AbortBackupOrd *ord = (AbortBackupOrd*)signal->getDataPtrSend();
  ord->backupId = ptr.p->backupId;
  ord->backupPtr = ptr.i;
  ord->requestType = requestType;
  ord->senderData= ptr.i;
  sendSignal(ptr.p->masterRef, GSN_ABORT_BACKUP_ORD, signal, 
	     AbortBackupOrd::SignalLength, JBB);
}

void
Backup::sendAbortBackupOrd(Signal* signal, BackupRecordPtr ptr, 
			   Uint32 requestType)
{
  jam();
  AbortBackupOrd *ord = (AbortBackupOrd*)signal->getDataPtrSend();
  ord->backupId = ptr.p->backupId;
  ord->backupPtr = ptr.i;
  ord->requestType = requestType;
  ord->senderData= ptr.i;
  NodePtr node;
  for(c_nodes.first(node); node.i != RNIL; c_nodes.next(node)) {
    jam();
    const Uint32 nodeId = node.p->nodeId;
    if(node.p->alive && ptr.p->nodes.get(nodeId)) {
      jam();
      sendSignal(numberToRef(BACKUP, nodeId), GSN_ABORT_BACKUP_ORD, signal, 
		 AbortBackupOrd::SignalLength, JBB);
    }//if
  }//for
}

/*****************************************************************************
 * 
 * Slave functionallity: Stop backup
 *
 *****************************************************************************/
void
Backup::execSTOP_BACKUP_REQ(Signal* signal)
{
  jamEntry();
  StopBackupReq * req = (StopBackupReq*)signal->getDataPtr();
  
  CRASH_INSERTION((10020));

  const Uint32 ptrI = req->backupPtr;
  const Uint32 backupId = req->backupId;
  const Uint32 startGCP = req->startGCP;
  const Uint32 stopGCP = req->stopGCP;

  /**
   * At least one GCP must have passed
   */
  ndbrequire(stopGCP > startGCP);
  
  /**
   * Get backup record
   */
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);

  ptr.p->slaveState.setState(STOPPING);
  slaveAbortCheck(); // macro will do return if ABORTING

  /**
   * Insert footers
   */
  {
    BackupFilePtr filePtr;
    ptr.p->files.getPtr(filePtr, ptr.p->logFilePtr);
    Uint32 * dst;
    ndbrequire(filePtr.p->operation.dataBuffer.getWritePtr(&dst, 1));
    * dst = 0;
    filePtr.p->operation.dataBuffer.updateWritePtr(1);
  }

  {
    BackupFilePtr filePtr;
    ptr.p->files.getPtr(filePtr, ptr.p->ctlFilePtr);
    
    const Uint32 gcpSz = sizeof(BackupFormat::CtlFile::GCPEntry) >> 2;
    
    Uint32 * dst;
    ndbrequire(filePtr.p->operation.dataBuffer.getWritePtr(&dst, gcpSz));
    
    BackupFormat::CtlFile::GCPEntry * gcp = 
      (BackupFormat::CtlFile::GCPEntry*)dst;
    
    gcp->SectionType   = htonl(BackupFormat::GCP_ENTRY);
    gcp->SectionLength = htonl(gcpSz);
    gcp->StartGCP      = htonl(startGCP);
    gcp->StopGCP       = htonl(stopGCP - 1);
    filePtr.p->operation.dataBuffer.updateWritePtr(gcpSz);
  }
  
  closeFiles(signal, ptr);
}

void
Backup::closeFiles(Signal* sig, BackupRecordPtr ptr)
{
  if (ptr.p->closingFiles) {
    jam();
    return;
  }
  ptr.p->closingFiles = true;

  /**
   * Close all files
   */
  BackupFilePtr filePtr;
  int openCount = 0;
  for(ptr.p->files.first(filePtr); filePtr.i!=RNIL; ptr.p->files.next(filePtr))
  {
    if(filePtr.p->fileOpened == 0) {
      jam();
      continue;
    }
    
    jam();
    openCount++;
    
    if(filePtr.p->fileDone == 1){
      jam();
      continue;
    }//if
    
    filePtr.p->fileDone = 1;
    
    if(filePtr.p->fileRunning == 1){
      jam();
#ifdef DEBUG_ABORT
      ndbout_c("Close files fileRunning == 1, filePtr.i=%u", filePtr.i);
#endif
      filePtr.p->operation.dataBuffer.eof();
    } else {
      jam();
      
      FsCloseReq * req = (FsCloseReq *)sig->getDataPtrSend();
      req->filePointer = filePtr.p->filePointer;
      req->userPointer = filePtr.i;
      req->userReference = reference();
      req->fileFlag = 0;
#ifdef DEBUG_ABORT
      ndbout_c("***** FSCLOSEREQ filePtr.i = %u", filePtr.i);
#endif
      sendSignal(NDBFS_REF, GSN_FSCLOSEREQ, sig, 
		 FsCloseReq::SignalLength, JBA);
    }//if
  }//for
  
  if(openCount == 0){
    jam();
    closeFilesDone(sig, ptr);
  }//if
}

void
Backup::execFSCLOSEREF(Signal* signal)
{
  jamEntry();
  
  FsRef * ref = (FsRef*)signal->getDataPtr();
  const Uint32 filePtrI = ref->userPointer;
  
  BackupFilePtr filePtr;
  c_backupFilePool.getPtr(filePtr, filePtrI);

  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, filePtr.p->backupPtr);
  
  /**
   * This should only happen during abort of backup
   */
  ndbrequire(ptr.p->slaveState.getState() == ABORTING);
  
  filePtr.p->fileOpened = 1;
  FsConf * conf = (FsConf*)signal->getDataPtr();
  conf->userPointer = filePtrI;
  
  execFSCLOSECONF(signal);
}

void
Backup::execFSCLOSECONF(Signal* signal)
{
  jamEntry();

  FsConf * conf = (FsConf*)signal->getDataPtr();
  const Uint32 filePtrI = conf->userPointer;
  
  BackupFilePtr filePtr;
  c_backupFilePool.getPtr(filePtr, filePtrI);

#ifdef DEBUG_ABORT
  ndbout_c("***** FSCLOSECONF filePtrI = %u", filePtrI);
#endif

  ndbrequire(filePtr.p->fileDone == 1);
  ndbrequire(filePtr.p->fileOpened == 1);
  ndbrequire(filePtr.p->fileRunning == 0);
  ndbrequire(filePtr.p->scanRunning == 0);	     
  
  filePtr.p->fileOpened = 0;
  
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, filePtr.p->backupPtr);
  for(ptr.p->files.first(filePtr); filePtr.i!=RNIL;ptr.p->files.next(filePtr)) 
  {
    jam();
    if(filePtr.p->fileOpened == 1) {
      jam();
#ifdef DEBUG_ABORT
      ndbout_c("waiting for more FSCLOSECONF's filePtr.i = %u", filePtr.i);
#endif
      return; // we will be getting more FSCLOSECONF's
    }//if
  }//for
  closeFilesDone(signal, ptr);
}

void
Backup::closeFilesDone(Signal* signal, BackupRecordPtr ptr)
{
  jam();
  
  if(ptr.p->slaveState.getState() == STOPPING) {
    jam();
    BackupFilePtr filePtr;
    ptr.p->files.getPtr(filePtr, ptr.p->logFilePtr);
    
    StopBackupConf* conf = (StopBackupConf*)signal->getDataPtrSend();
    conf->backupId = ptr.p->backupId;
    conf->backupPtr = ptr.i;
    conf->noOfLogBytes = filePtr.p->operation.noOfBytes;
    conf->noOfLogRecords = filePtr.p->operation.noOfRecords;
    sendSignal(ptr.p->masterRef, GSN_STOP_BACKUP_CONF, signal,
	       StopBackupConf::SignalLength, JBB);
    
    ptr.p->slaveState.setState(CLEANING);
    return;
  }//if
  
  ndbrequire(ptr.p->slaveState.getState() == ABORTING);
  removeBackup(signal, ptr);
}

/*****************************************************************************
 * 
 * Slave functionallity: Abort backup
 *
 *****************************************************************************/
void
Backup::removeBackup(Signal* signal, BackupRecordPtr ptr)
{
  jam();
  
  FsRemoveReq * req = (FsRemoveReq *)signal->getDataPtrSend();
  req->userReference = reference();
  req->userPointer = ptr.i;
  req->directory = 1;
  req->ownDirectory = 1;
  FsOpenReq::setVersion(req->fileNumber, 2);
  FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_CTL);
  FsOpenReq::v2_setSequence(req->fileNumber, ptr.p->backupId);
  FsOpenReq::v2_setNodeId(req->fileNumber, getOwnNodeId());
  sendSignal(NDBFS_REF, GSN_FSREMOVEREQ, signal, 
	     FsRemoveReq::SignalLength, JBA);
}

void
Backup::execFSREMOVEREF(Signal* signal)
{
  jamEntry();
  ndbrequire(0);
}

void
Backup::execFSREMOVECONF(Signal* signal){
  jamEntry();

  FsConf * conf = (FsConf*)signal->getDataPtr();
  const Uint32 ptrI = conf->userPointer;
  
  /**
   * Get backup record
   */
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);
  
  ndbrequire(ptr.p->slaveState.getState() == ABORTING);
  if (ptr.p->masterRef == reference()) {
    if (ptr.p->masterData.state.getAbortState() == DEFINING) {
      jam();
      sendBackupRef(signal, ptr, ptr.p->errorCode);
      return;
    } else {
      jam();
    }//if
  }//if
  cleanupSlaveResources(ptr);
}

/*****************************************************************************
 * 
 * Slave functionallity: Abort backup
 *
 *****************************************************************************/
void
Backup::execABORT_BACKUP_ORD(Signal* signal)
{
  jamEntry();
  AbortBackupOrd* ord = (AbortBackupOrd*)signal->getDataPtr();

  const Uint32 backupId = ord->backupId;
  const AbortBackupOrd::RequestType requestType = 
    (AbortBackupOrd::RequestType)ord->requestType;
  const Uint32 senderData = ord->senderData;
  
#ifdef DEBUG_ABORT
  ndbout_c("******** ABORT_BACKUP_ORD ********* nodeId = %u", 
	   refToNode(signal->getSendersBlockRef()));
  ndbout_c("backupId = %u, requestType = %u, senderData = %u, ",
	   backupId, requestType, senderData);
  dumpUsedResources();
#endif

  BackupRecordPtr ptr;
  if(requestType == AbortBackupOrd::ClientAbort) {
    if (getOwnNodeId() != getMasterNodeId()) {
      jam();
      // forward to master
#ifdef DEBUG_ABORT
      ndbout_c("---- Forward to master nodeId = %u", getMasterNodeId());
#endif
      sendSignal(calcBackupBlockRef(getMasterNodeId()), GSN_ABORT_BACKUP_ORD, 
		 signal, AbortBackupOrd::SignalLength, JBB);
      return;
    }
    jam();
    for(c_backups.first(ptr); ptr.i != RNIL; c_backups.next(ptr)) {
      jam();
      if(ptr.p->backupId == backupId && ptr.p->clientData == senderData) {
        jam();
	break;
      }//if
    }//for
    if(ptr.i == RNIL) {
      jam();
      return;
    }//if
  } else {
    if (c_backupPool.findId(senderData)) {
      jam();
      c_backupPool.getPtr(ptr, senderData);
    } else { // TODO might be abort sent to not master, 
             // or master aborting too early
      jam();
#ifdef DEBUG_ABORT
      ndbout_c("Backup: abort request type=%u on id=%u,%u not found",
	       requestType, backupId, senderData);
#endif
      return;
    }
  }//if
  
  const bool isCoordinator = (ptr.p->masterRef == reference());

  bool ok = false;
  switch(requestType){

    /**
     * Requests sent to master
     */

  case AbortBackupOrd::ClientAbort:
    jam();
    // fall through
  case AbortBackupOrd::LogBufferFull:
    jam();
    // fall through
  case AbortBackupOrd::FileOrScanError:
    jam();
    if(ptr.p->masterData.state.getState() == ABORTING) {
#ifdef DEBUG_ABORT
      ndbout_c("---- Already aborting");
#endif
      jam();
      return;
    }
    ptr.p->setErrorCode(requestType);
    ndbrequire(isCoordinator); // Sent from slave to coordinator
    masterAbort(signal, ptr, false);
    return;

    /**
     * Info sent to slave
     */

  case AbortBackupOrd::OkToClean:
    jam();
    cleanupMasterResources(ptr);
    return;

    /**
     * Requests sent to slave
     */

  case AbortBackupOrd::BackupComplete:
    jam();
    if (ptr.p->slaveState.getState() == CLEANING) { // TODO what if state is 
                                                    // not CLEANING?
      jam();
      cleanupSlaveResources(ptr);
    }//if
    return;
    break;
  case AbortBackupOrd::BackupFailureDueToNodeFail:
    jam();
    ok = true;
    if (ptr.p->errorCode != 0)
      ptr.p->setErrorCode(requestType);
    break;
  case AbortBackupOrd::BackupFailure:
    jam();
    ok = true;
    break;
  }
  ndbrequire(ok);
  
  /**
   * Slave abort
   */
  slaveAbort(signal, ptr);
}

void
Backup::slaveAbort(Signal* signal, BackupRecordPtr ptr)
{
  if(ptr.p->slaveState.getState() == ABORTING) {
#ifdef DEBUG_ABORT
    ndbout_c("---- Slave already aborting");
#endif
    jam();
    return;
  }
#ifdef DEBUG_ABORT
  ndbout_c("************* slaveAbort");
#endif

  State slaveState = ptr.p->slaveState.getState();
  ptr.p->slaveState.setState(ABORTING);
  switch(slaveState) {
  case DEFINING:
    jam();
    return;
//------------------------------------------
// Will watch for the abort at various places
// in the defining phase.
//------------------------------------------
  case ABORTING:
    jam();
    //Fall through
  case DEFINED:
    jam();
    //Fall through
  case STOPPING:
    jam();
    closeFiles(signal, ptr);
    return;
  case STARTED:
    jam();
    //Fall through
  case SCANNING:
    jam();
    BackupFilePtr filePtr;
    filePtr.i = RNIL;
    abortFile(signal, ptr, filePtr);
    return;
  case CLEANING:
    jam();
    cleanupSlaveResources(ptr);
    return;
  case INITIAL:
    jam();
    ndbrequire(false);
    return;
  }
}

void
Backup::dumpUsedResources()
{
  jam();
  BackupRecordPtr ptr;

  for(c_backups.first(ptr); ptr.i != RNIL; c_backups.next(ptr)) {
    ndbout_c("Backup id=%u, slaveState.getState = %u, errorCode=%u",
	     ptr.p->backupId,
	     ptr.p->slaveState.getState(),
	     ptr.p->errorCode);

    TablePtr tabPtr;
    for(ptr.p->tables.first(tabPtr);
	tabPtr.i != RNIL;
	ptr.p->tables.next(tabPtr)) {
      jam();
      for(Uint32 j = 0; j<3; j++) {
	jam();
	TriggerPtr trigPtr;
	if(tabPtr.p->triggerAllocated[j]) {
	  jam();
	  c_triggerPool.getPtr(trigPtr, tabPtr.p->triggerIds[j]);
	  ndbout_c("Allocated[%u] Triggerid = %u, event = %u",
		 j,
		 tabPtr.p->triggerIds[j],
		 trigPtr.p->event);
	}//if
      }//for
    }//for
    
    BackupFilePtr filePtr;
    for(ptr.p->files.first(filePtr);
	filePtr.i != RNIL;
	ptr.p->files.next(filePtr)) {
      jam();
      ndbout_c("filePtr.i = %u, filePtr.p->fileOpened=%u fileRunning=%u "
	       "scanRunning=%u",
	       filePtr.i,
	       filePtr.p->fileOpened,
	       filePtr.p->fileRunning,
	       filePtr.p->scanRunning);
    }//for
  }
}

void
Backup::cleanupMasterResources(BackupRecordPtr ptr)
{
#ifdef DEBUG_ABORT
  ndbout_c("******** Cleanup Master Resources *********");
  ndbout_c("backupId = %u, errorCode = %u", ptr.p->backupId, ptr.p->errorCode);
#endif

  TablePtr tabPtr;
  for(ptr.p->tables.first(tabPtr); tabPtr.i != RNIL;ptr.p->tables.next(tabPtr))
  {
    jam();
    tabPtr.p->attributes.release();
    tabPtr.p->fragments.release();
    for(Uint32 j = 0; j<3; j++) {
      jam();
      TriggerPtr trigPtr;
      if(tabPtr.p->triggerAllocated[j]) {
        jam();
	c_triggerPool.getPtr(trigPtr, tabPtr.p->triggerIds[j]);
	trigPtr.p->event = ILLEGAL_TRIGGER_ID;
        tabPtr.p->triggerAllocated[j] = false;
      }//if
      tabPtr.p->triggerIds[j] = ILLEGAL_TRIGGER_ID;
    }//for
  }//for
  ptr.p->tables.release();
  ptr.p->triggers.release();
  ptr.p->okToCleanMaster = true;

  cleanupFinalResources(ptr);
}

void
Backup::cleanupSlaveResources(BackupRecordPtr ptr)
{
#ifdef DEBUG_ABORT
  ndbout_c("******** Clean Up Slave Resources*********");
  ndbout_c("backupId = %u, errorCode = %u", ptr.p->backupId, ptr.p->errorCode);
#endif

  BackupFilePtr filePtr;
  for(ptr.p->files.first(filePtr);
      filePtr.i != RNIL; 
      ptr.p->files.next(filePtr)) {
    jam();
    ndbrequire(filePtr.p->fileOpened == 0);
    ndbrequire(filePtr.p->fileRunning == 0);
    ndbrequire(filePtr.p->scanRunning == 0);
    filePtr.p->pages.release();
  }//for
  ptr.p->files.release();

  cleanupFinalResources(ptr);
}

void
Backup::cleanupFinalResources(BackupRecordPtr ptr)
{
#ifdef DEBUG_ABORT
  ndbout_c("******** Clean Up Final Resources*********");
  ndbout_c("backupId = %u, errorCode = %u", ptr.p->backupId, ptr.p->errorCode);
#endif

  //  if (!ptr.p->tables.empty() || !ptr.p->files.empty()) {
  if (!ptr.p->okToCleanMaster || !ptr.p->files.empty()) {
    jam();
#ifdef DEBUG_ABORT
    ndbout_c("******** Waiting to do final cleanup");
#endif
    return;
  }
  ptr.p->pages.release();
  ptr.p->masterData.state.setState(INITIAL);
  ptr.p->slaveState.setState(INITIAL);
  ptr.p->backupId = 0;

  ptr.p->closingFiles    = false;
  ptr.p->okToCleanMaster = true;

  c_backups.release(ptr);
  //  ndbrequire(false);
}
