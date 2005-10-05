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

#define DBDIH_C
#include <ndb_limits.h>
#include <ndb_version.h>
#include <NdbOut.hpp>

#include "Dbdih.hpp"
#include "Configuration.hpp"

#include <signaldata/BlockCommitOrd.hpp>
#include <signaldata/CheckNodeGroups.hpp>
#include <signaldata/CreateFrag.hpp>
#include <signaldata/CopyActive.hpp>
#include <signaldata/CopyFrag.hpp>
#include <signaldata/CopyGCIReq.hpp>
#include <signaldata/DiAddTab.hpp>
#include <signaldata/DictStart.hpp>
#include <signaldata/DiGetNodes.hpp>
#include <signaldata/DihContinueB.hpp>
#include <signaldata/DihSwitchReplica.hpp>
#include <signaldata/DumpStateOrd.hpp>
#include <signaldata/EmptyLcp.hpp>
#include <signaldata/EndTo.hpp>
#include <signaldata/EventReport.hpp>
#include <signaldata/GCPSave.hpp>
#include <signaldata/HotSpareRep.hpp>
#include <signaldata/MasterGCP.hpp>
#include <signaldata/MasterLCP.hpp>
#include <signaldata/NFCompleteRep.hpp>
#include <signaldata/NodeFailRep.hpp>
#include <signaldata/ReadNodesConf.hpp>
#include <signaldata/StartFragReq.hpp>
#include <signaldata/StartInfo.hpp>
#include <signaldata/StartMe.hpp>
#include <signaldata/StartPerm.hpp>
#include <signaldata/StartRec.hpp>
#include <signaldata/StartTo.hpp>
#include <signaldata/StopPerm.hpp>
#include <signaldata/StopMe.hpp>
#include <signaldata/TestOrd.hpp>
#include <signaldata/UpdateTo.hpp>
#include <signaldata/WaitGCP.hpp>
#include <signaldata/DihStartTab.hpp>
#include <signaldata/LCP.hpp>
#include <signaldata/SystemError.hpp>

#include <signaldata/DropTab.hpp>
#include <signaldata/AlterTab.hpp>
#include <signaldata/PrepDropTab.hpp>
#include <signaldata/SumaImpl.hpp>
#include <signaldata/DictTabInfo.hpp>
#include <signaldata/CreateFragmentation.hpp>
#include <signaldata/LqhFrag.hpp>
#include <signaldata/FsOpenReq.hpp>
#include <DebuggerNames.hpp>

#include <EventLogger.hpp>
extern EventLogger g_eventLogger;

#define SYSFILE ((Sysfile *)&sysfileData[0])

#define RETURN_IF_NODE_NOT_ALIVE(node) \
  if (!checkNodeAlive((node))) { \
    jam(); \
    return; \
  } \

#define RETURN_IF_TAKE_OVER_INTERRUPTED(takeOverIndex, regTOPtr) \
  regTOPtr.i = takeOverIndex; \
  ptrCheckGuard(regTOPtr, MAX_NDB_NODES, takeOverRecord); \
  if (checkToInterrupted(regTOPtr)) { \
    jam(); \
    return; \
  } \

#define receiveLoopMacro(sigName, receiveNodeId)\
{                                                \
  c_##sigName##_Counter.clearWaitingFor(receiveNodeId); \
  if(c_##sigName##_Counter.done() == false){     \
     jam();                                      \
     return;                                     \
  }                                              \
}

#define sendLoopMacro(sigName, signalRoutine)  \
{                                                                       \
  c_##sigName##_Counter.clearWaitingFor();                              \
  NodeRecordPtr specNodePtr;                                            \
  specNodePtr.i = cfirstAliveNode;                                      \
  do {                                                                  \
    jam();                                                              \
    ptrCheckGuard(specNodePtr, MAX_NDB_NODES, nodeRecord);              \
    c_##sigName##_Counter.setWaitingFor(specNodePtr.i);                 \
    signalRoutine(signal, specNodePtr.i);                               \
    specNodePtr.i = specNodePtr.p->nextNode;                            \
  } while (specNodePtr.i != RNIL);                                      \
}

static
Uint32
prevLcpNo(Uint32 lcpNo){
  if(lcpNo == 0)
    return MAX_LCP_STORED - 1;
  return lcpNo - 1;
}

static
Uint32
nextLcpNo(Uint32 lcpNo){
  lcpNo++;
  if(lcpNo == MAX_LCP_STORED)
    return 0;
  return lcpNo;
}

#define gth(x, y) ndbrequire(((int)x)>((int)y))

void Dbdih::nullRoutine(Signal* signal, Uint32 nodeId)
{
}//Dbdih::nullRoutine()

void Dbdih::sendCOPY_GCIREQ(Signal* signal, Uint32 nodeId) 
{
  ndbrequire(c_copyGCIMaster.m_copyReason != CopyGCIReq::IDLE);
  
  const BlockReference ref = calcDihBlockRef(nodeId);
  const Uint32 wordPerSignal = CopyGCIReq::DATA_SIZE;
  const Uint32 noOfSignals = ((Sysfile::SYSFILE_SIZE32 + (wordPerSignal - 1)) /
			      wordPerSignal);
  
  CopyGCIReq * const copyGCI = (CopyGCIReq *)&signal->theData[0];  
  copyGCI->anyData = nodeId;
  copyGCI->copyReason = c_copyGCIMaster.m_copyReason;
  copyGCI->startWord = 0;
  
  for(Uint32 i = 0; i < noOfSignals; i++) {
    jam();
    { // Do copy
      const int startWord = copyGCI->startWord;
      for(Uint32 j = 0; j < wordPerSignal; j++) {
        copyGCI->data[j] = sysfileData[j+startWord];
      }//for
    }
    sendSignal(ref, GSN_COPY_GCIREQ, signal, 25, JBB);
    copyGCI->startWord += wordPerSignal;
  }//for
}//Dbdih::sendCOPY_GCIREQ()


void Dbdih::sendDIH_SWITCH_REPLICA_REQ(Signal* signal, Uint32 nodeId)
{
  const BlockReference ref    = calcDihBlockRef(nodeId);
  sendSignal(ref, GSN_DIH_SWITCH_REPLICA_REQ, signal, 
             DihSwitchReplicaReq::SignalLength, JBB);
}//Dbdih::sendDIH_SWITCH_REPLICA_REQ()

void Dbdih::sendEMPTY_LCP_REQ(Signal* signal, Uint32 nodeId)
{
  BlockReference ref = calcLqhBlockRef(nodeId);
  sendSignal(ref, GSN_EMPTY_LCP_REQ, signal, EmptyLcpReq::SignalLength, JBB);
}//Dbdih::sendEMPTY_LCPREQ()

void Dbdih::sendEND_TOREQ(Signal* signal, Uint32 nodeId)
{
  BlockReference ref = calcDihBlockRef(nodeId);
  sendSignal(ref, GSN_END_TOREQ, signal, EndToReq::SignalLength, JBB);
}//Dbdih::sendEND_TOREQ()

void Dbdih::sendGCP_COMMIT(Signal* signal, Uint32 nodeId)
{
  BlockReference ref = calcDihBlockRef(nodeId);
  signal->theData[0] = cownNodeId;
  signal->theData[1] = cnewgcp;
  sendSignal(ref, GSN_GCP_COMMIT, signal, 2, JBA);
}//Dbdih::sendGCP_COMMIT()

void Dbdih::sendGCP_PREPARE(Signal* signal, Uint32 nodeId)
{
  BlockReference ref = calcDihBlockRef(nodeId);
  signal->theData[0] = cownNodeId;
  signal->theData[1] = cnewgcp;
  sendSignal(ref, GSN_GCP_PREPARE, signal, 2, JBA);
}//Dbdih::sendGCP_PREPARE()

void Dbdih::sendGCP_SAVEREQ(Signal* signal, Uint32 nodeId)
{
  GCPSaveReq * const saveReq = (GCPSaveReq*)&signal->theData[0];
  BlockReference ref = calcLqhBlockRef(nodeId);
  saveReq->dihBlockRef = reference();
  saveReq->dihPtr = nodeId;
  saveReq->gci = coldgcp;
  sendSignal(ref, GSN_GCP_SAVEREQ, signal, GCPSaveReq::SignalLength, JBB);
}//Dbdih::sendGCP_SAVEREQ()

void Dbdih::sendINCL_NODEREQ(Signal* signal, Uint32 nodeId)
{
  BlockReference nodeDihRef = calcDihBlockRef(nodeId);
  signal->theData[0] = reference();
  signal->theData[1] = c_nodeStartMaster.startNode;
  signal->theData[2] = c_nodeStartMaster.failNr;
  signal->theData[3] = 0;
  signal->theData[4] = currentgcp;  
  sendSignal(nodeDihRef, GSN_INCL_NODEREQ, signal, 5, JBB);
}//Dbdih::sendINCL_NODEREQ()

void Dbdih::sendMASTER_GCPREQ(Signal* signal, Uint32 nodeId)
{
  BlockReference ref = calcDihBlockRef(nodeId);
  sendSignal(ref, GSN_MASTER_GCPREQ, signal, MasterGCPReq::SignalLength, JBB);
}//Dbdih::sendMASTER_GCPREQ()

void Dbdih::sendMASTER_LCPREQ(Signal* signal, Uint32 nodeId)
{
  BlockReference ref = calcDihBlockRef(nodeId);
  sendSignal(ref, GSN_MASTER_LCPREQ, signal, MasterLCPReq::SignalLength, JBB);
}//Dbdih::sendMASTER_LCPREQ()

void Dbdih::sendSTART_INFOREQ(Signal* signal, Uint32 nodeId)
{
  const BlockReference ref = calcDihBlockRef(nodeId);
  sendSignal(ref, GSN_START_INFOREQ, signal, StartInfoReq::SignalLength, JBB);
}//sendSTART_INFOREQ()

void Dbdih::sendSTART_RECREQ(Signal* signal, Uint32 nodeId)
{
  StartRecReq * const req = (StartRecReq*)&signal->theData[0];
  BlockReference ref = calcLqhBlockRef(nodeId);
  req->receivingNodeId = nodeId;
  req->senderRef = reference();
  req->keepGci = SYSFILE->keepGCI;
  req->lastCompletedGci = SYSFILE->lastCompletedGCI[nodeId];
  req->newestGci = SYSFILE->newestRestorableGCI;
  sendSignal(ref, GSN_START_RECREQ, signal, StartRecReq::SignalLength, JBB);

  signal->theData[0] = NDB_LE_StartREDOLog;
  signal->theData[1] = nodeId;
  signal->theData[2] = SYSFILE->keepGCI;
  signal->theData[3] = SYSFILE->lastCompletedGCI[nodeId];
  signal->theData[4] = SYSFILE->newestRestorableGCI;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 5, JBB);
}//Dbdih::sendSTART_RECREQ()

void Dbdih::sendSTART_TOREQ(Signal* signal, Uint32 nodeId)
{
  BlockReference ref = calcDihBlockRef(nodeId);
  sendSignal(ref, GSN_START_TOREQ, signal, StartToReq::SignalLength, JBB);
}//Dbdih::sendSTART_TOREQ()

void Dbdih::sendSTOP_ME_REQ(Signal* signal, Uint32 nodeId)
{
  if (nodeId != getOwnNodeId()) {
    jam();
    const BlockReference ref = calcDihBlockRef(nodeId);
    sendSignal(ref, GSN_STOP_ME_REQ, signal, StopMeReq::SignalLength, JBB);
  }//if
}//Dbdih::sendSTOP_ME_REQ()

void Dbdih::sendTC_CLOPSIZEREQ(Signal* signal, Uint32 nodeId)
{
  BlockReference ref = calcTcBlockRef(nodeId);
  signal->theData[0] = nodeId;
  signal->theData[1] = reference();
  sendSignal(ref, GSN_TC_CLOPSIZEREQ, signal, 2, JBB);
}//Dbdih::sendTC_CLOPSIZEREQ()

void Dbdih::sendTCGETOPSIZEREQ(Signal* signal, Uint32 nodeId)
{
  BlockReference ref = calcTcBlockRef(nodeId);
  signal->theData[0] = nodeId;
  signal->theData[1] = reference();
  sendSignal(ref, GSN_TCGETOPSIZEREQ, signal, 2, JBB);
}//Dbdih::sendTCGETOPSIZEREQ()

void Dbdih::sendUPDATE_TOREQ(Signal* signal, Uint32 nodeId)
{
  const BlockReference ref = calcDihBlockRef(nodeId);
  sendSignal(ref, GSN_UPDATE_TOREQ, signal, UpdateToReq::SignalLength, JBB);
}//sendUPDATE_TOREQ()

void Dbdih::execCONTINUEB(Signal* signal)
{
  jamEntry();
  switch ((DihContinueB::Type)signal->theData[0]) {
  case DihContinueB::ZPACK_TABLE_INTO_PAGES:
    {
      jam();
      Uint32 tableId = signal->theData[1];
      packTableIntoPagesLab(signal, tableId);
      return;
      break;
    }
  case DihContinueB::ZPACK_FRAG_INTO_PAGES:
    {
      RWFragment wf;
      jam();
      wf.rwfTabPtr.i = signal->theData[1];
      ptrCheckGuard(wf.rwfTabPtr, ctabFileSize, tabRecord);
      wf.fragId = signal->theData[2];
      wf.pageIndex = signal->theData[3];
      wf.wordIndex = signal->theData[4];
      packFragIntoPagesLab(signal, &wf);
      return;
      break;
    }
  case DihContinueB::ZREAD_PAGES_INTO_TABLE:
    {
      jam();
      Uint32 tableId = signal->theData[1];
      readPagesIntoTableLab(signal, tableId);
      return;
      break;
    }
  case DihContinueB::ZREAD_PAGES_INTO_FRAG:
    {
      RWFragment rf;
      jam();
      rf.rwfTabPtr.i = signal->theData[1];
      ptrCheckGuard(rf.rwfTabPtr, ctabFileSize, tabRecord);
      rf.fragId = signal->theData[2];
      rf.pageIndex = signal->theData[3];
      rf.wordIndex = signal->theData[4];
      readPagesIntoFragLab(signal, &rf);
      return;
      break;
    }
  case DihContinueB::ZCOPY_TABLE:
    {
      jam();
      Uint32 tableId = signal->theData[1];
      copyTableLab(signal, tableId);
      return;
    }
  case DihContinueB::ZCOPY_TABLE_NODE:
    {
      NodeRecordPtr nodePtr;
      CopyTableNode ctn;
      jam();
      ctn.ctnTabPtr.i = signal->theData[1];
      ptrCheckGuard(ctn.ctnTabPtr, ctabFileSize, tabRecord);
      nodePtr.i = signal->theData[2];
      ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
      ctn.pageIndex = signal->theData[3];
      ctn.wordIndex = signal->theData[4];
      ctn.noOfWords = signal->theData[5];
      copyTableNode(signal, &ctn, nodePtr);
      return;
    }
  case DihContinueB::ZSTART_FRAGMENT:
    {
      jam();
      Uint32 tableId = signal->theData[1];
      Uint32 fragId = signal->theData[2];
      startFragment(signal, tableId, fragId);
      return;
    }
  case DihContinueB::ZCOMPLETE_RESTART:
    jam();
    completeRestartLab(signal);
    return;
  case DihContinueB::ZREAD_TABLE_FROM_PAGES:
    {
      TabRecordPtr tabPtr;
      jam();
      tabPtr.i = signal->theData[1];
      ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
      readTableFromPagesLab(signal, tabPtr);
      return;
    }
  case DihContinueB::ZSR_PHASE2_READ_TABLE:
    {
      TabRecordPtr tabPtr;
      jam();
      tabPtr.i = signal->theData[1];
      ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
      srPhase2ReadTableLab(signal, tabPtr);
      return;
    }
  case DihContinueB::ZCHECK_TC_COUNTER:
    jam();
#ifndef NO_LCP
    checkTcCounterLab(signal);
#endif
    return;
  case DihContinueB::ZCALCULATE_KEEP_GCI:
    {
      jam();
      Uint32 tableId = signal->theData[1];
      Uint32 fragId = signal->theData[2];
      calculateKeepGciLab(signal, tableId, fragId);
      return;
    }
  case DihContinueB::ZSTORE_NEW_LCP_ID:
    jam();
    storeNewLcpIdLab(signal);
    return;
  case DihContinueB::ZTABLE_UPDATE:
    {
      TabRecordPtr tabPtr;
      jam();
      tabPtr.i = signal->theData[1];
      ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
      tableUpdateLab(signal, tabPtr);
      return;
    }
  case DihContinueB::ZCHECK_LCP_COMPLETED:
    {
      jam();
      checkLcpCompletedLab(signal);
      return;
    }
  case DihContinueB::ZINIT_LCP:
    {
      jam();
      Uint32 senderRef = signal->theData[1];
      Uint32 tableId = signal->theData[2];
      initLcpLab(signal, senderRef, tableId);
      return;
    }
  case DihContinueB::ZADD_TABLE_MASTER_PAGES:
    {
      TabRecordPtr tabPtr;
      jam();
      tabPtr.i = signal->theData[1];
      ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
      tabPtr.p->tabUpdateState = TabRecord::US_ADD_TABLE_MASTER;
      tableUpdateLab(signal, tabPtr);
      return;
      break;
    }
  case DihContinueB::ZDIH_ADD_TABLE_MASTER:
    {
      jam();
      addTable_closeConf(signal, signal->theData[1]);
      return;
    }
  case DihContinueB::ZADD_TABLE_SLAVE_PAGES:
    {
      TabRecordPtr tabPtr;
      jam();
      tabPtr.i = signal->theData[1];
      ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
      tabPtr.p->tabUpdateState = TabRecord::US_ADD_TABLE_SLAVE;
      tableUpdateLab(signal, tabPtr);
      return;
    }
  case DihContinueB::ZDIH_ADD_TABLE_SLAVE:
    {
      ndbrequire(false);
      return;
    }
  case DihContinueB::ZSTART_GCP:
    jam();
#ifndef NO_GCP
    startGcpLab(signal, signal->theData[1]);
#endif
    return;
    break;
  case DihContinueB::ZCOPY_GCI:{
    jam();
    CopyGCIReq::CopyReason reason = (CopyGCIReq::CopyReason)signal->theData[1];
    ndbrequire(c_copyGCIMaster.m_copyReason == reason);
    sendLoopMacro(COPY_GCIREQ, sendCOPY_GCIREQ);
    return;
  }
    break;
  case DihContinueB::ZEMPTY_VERIFY_QUEUE:
    jam();
    emptyverificbuffer(signal, true);
    return;
    break;
  case DihContinueB::ZCHECK_GCP_STOP:
    jam();
#ifndef NO_GCP
    checkGcpStopLab(signal);
#endif
    return;
    break;
  case DihContinueB::ZREMOVE_NODE_FROM_TABLE:
    {
      jam();
      Uint32 nodeId = signal->theData[1];
      Uint32 tableId = signal->theData[2];
      removeNodeFromTables(signal, nodeId, tableId);
      return;
    }
  case DihContinueB::ZCOPY_NODE:
    {
      jam();
      Uint32 tableId = signal->theData[1];
      copyNodeLab(signal, tableId);
      return;
    }
  case DihContinueB::ZSTART_TAKE_OVER:
    {
      jam();
      Uint32 takeOverPtrI = signal->theData[1];
      Uint32 startNode = signal->theData[2];
      Uint32 toNode = signal->theData[3];
      startTakeOver(signal, takeOverPtrI, startNode, toNode);
      return;
      break;
    }
  case DihContinueB::ZCHECK_START_TAKE_OVER:
    jam();
    checkStartTakeOver(signal);
    break;
  case DihContinueB::ZTO_START_COPY_FRAG:
    {
      jam();
      Uint32 takeOverPtrI = signal->theData[1];
      startNextCopyFragment(signal, takeOverPtrI);
      return;
    }
  case DihContinueB::ZINVALIDATE_NODE_LCP:
    {
      jam();
      const Uint32 nodeId = signal->theData[1];
      const Uint32 tableId = signal->theData[2];
      invalidateNodeLCP(signal, nodeId, tableId);
      return;
    }
  case DihContinueB::ZINITIALISE_RECORDS:
    jam();
    initialiseRecordsLab(signal, 
			 signal->theData[1], 
			 signal->theData[2], 
			 signal->theData[3]);
    return;
    break;
  case DihContinueB::ZSTART_PERMREQ_AGAIN:
    jam();
    nodeRestartPh2Lab(signal);
    return;
    break;
  case DihContinueB::SwitchReplica:
    {
      jam();
      const Uint32 nodeId = signal->theData[1];
      const Uint32 tableId = signal->theData[2];
      const Uint32 fragNo = signal->theData[3];
      switchReplica(signal, nodeId, tableId, fragNo);
      return;
    }
  case DihContinueB::ZSEND_START_TO:
    {
      jam();
      Uint32 takeOverPtrI = signal->theData[1];
      sendStartTo(signal, takeOverPtrI);
      return;
    }
  case DihContinueB::ZSEND_ADD_FRAG:
    {
      jam();
      Uint32 takeOverPtrI = signal->theData[1];
      toCopyFragLab(signal, takeOverPtrI);
      return;
    }
  case DihContinueB::ZSEND_UPDATE_TO:
    {
      jam();
      Uint32 takeOverPtrI = signal->theData[1];
      Uint32 updateState = signal->theData[4];
      sendUpdateTo(signal, takeOverPtrI, updateState);
      return;
    }
  case DihContinueB::ZSEND_END_TO:
    {
      jam();
      Uint32 takeOverPtrI = signal->theData[1];
      sendEndTo(signal, takeOverPtrI);
      return;
    }
  case DihContinueB::ZSEND_CREATE_FRAG:
    {
      jam();
      Uint32 takeOverPtrI = signal->theData[1];
      Uint32 storedType = signal->theData[2];
      Uint32 startGci = signal->theData[3];
      sendCreateFragReq(signal, startGci, storedType, takeOverPtrI);
      return;
    }
  case DihContinueB::WAIT_DROP_TAB_WRITING_TO_FILE:{
    jam();
    TabRecordPtr tabPtr;
    tabPtr.i = signal->theData[1];
    ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
    waitDropTabWritingToFile(signal, tabPtr);
    return;
  }
  case DihContinueB::CHECK_WAIT_DROP_TAB_FAILED_LQH:{
    jam();
    Uint32 nodeId = signal->theData[1];
    Uint32 tableId = signal->theData[2];
    checkWaitDropTabFailedLqh(signal, nodeId, tableId);
    return;
  }
  }//switch
  
  ndbrequire(false);
  return;
}//Dbdih::execCONTINUEB()

void Dbdih::execCOPY_GCIREQ(Signal* signal) 
{
  CopyGCIReq * const copyGCI = (CopyGCIReq *)&signal->theData[0];
  jamEntry();
  CopyGCIReq::CopyReason reason = (CopyGCIReq::CopyReason)copyGCI->copyReason;
  const Uint32 tstart = copyGCI->startWord;
  
  ndbrequire(cmasterdihref == signal->senderBlockRef()) ;
  ndbrequire(c_copyGCISlave.m_copyReason  == CopyGCIReq::IDLE);
  ndbrequire(c_copyGCISlave.m_expectedNextWord == tstart);
  ndbrequire(reason != CopyGCIReq::IDLE);
  
  arrGuard(tstart + CopyGCIReq::DATA_SIZE, sizeof(sysfileData)/4);
  for(Uint32 i = 0; i<CopyGCIReq::DATA_SIZE; i++)
    cdata[tstart+i] = copyGCI->data[i];
  
  if ((tstart + CopyGCIReq::DATA_SIZE) >= Sysfile::SYSFILE_SIZE32) {
    jam();
    c_copyGCISlave.m_expectedNextWord = 0;
  } else {
    jam();
    c_copyGCISlave.m_expectedNextWord += CopyGCIReq::DATA_SIZE;
    return;
  }//if
  
  memcpy(sysfileData, cdata, sizeof(sysfileData));
  
  c_copyGCISlave.m_copyReason = reason;
  c_copyGCISlave.m_senderRef  = signal->senderBlockRef();
  c_copyGCISlave.m_senderData = copyGCI->anyData;

  CRASH_INSERTION2(7020, reason==CopyGCIReq::LOCAL_CHECKPOINT);
  CRASH_INSERTION2(7008, reason==CopyGCIReq::GLOBAL_CHECKPOINT);

  /* -------------------------------------------------------------------------*/
  /*     WE SET THE REQUESTER OF THE COPY GCI TO THE CURRENT MASTER. IF THE   */
  /*     CURRENT MASTER WE DO NOT WANT THE NEW MASTER TO RECEIVE CONFIRM OF   */
  /*     SOMETHING HE HAS NOT SENT. THE TAKE OVER MUST BE CAREFUL.            */
  /* -------------------------------------------------------------------------*/
  bool ok = false;
  switch(reason){
  case CopyGCIReq::IDLE:
    ok = true;
    jam();
    ndbrequire(false);
    break;
  case CopyGCIReq::LOCAL_CHECKPOINT: {
    ok = true;
    jam();
    c_lcpState.setLcpStatus(LCP_COPY_GCI, __LINE__);
    c_lcpState.m_masterLcpDihRef = cmasterdihref;
    setNodeInfo(signal);
    break;
  }
  case CopyGCIReq::RESTART: {
    ok = true;
    jam();
    coldgcp = SYSFILE->newestRestorableGCI;
    crestartGci = SYSFILE->newestRestorableGCI;
    Sysfile::setRestartOngoing(SYSFILE->systemRestartBits);
    currentgcp = coldgcp + 1;
    cnewgcp = coldgcp + 1;
    setNodeInfo(signal);
    if ((Sysfile::getLCPOngoing(SYSFILE->systemRestartBits))) {
      jam();
      /* -------------------------------------------------------------------- */
      //  IF THERE WAS A LOCAL CHECKPOINT ONGOING AT THE CRASH MOMENT WE WILL
      //    INVALIDATE THAT LOCAL CHECKPOINT.
      /* -------------------------------------------------------------------- */
      invalidateLcpInfoAfterSr();
    }//if
    break;
  }
  case CopyGCIReq::GLOBAL_CHECKPOINT: {
    ok = true;
    jam();
    cgcpParticipantState = GCP_PARTICIPANT_COPY_GCI_RECEIVED;
    setNodeInfo(signal);
    break;
  }//if
  case CopyGCIReq::INITIAL_START_COMPLETED:
    ok = true;
    jam();
    break;
  }
  ndbrequire(ok);
  
  /* ----------------------------------------------------------------------- */
  /*     WE START BY TRYING TO OPEN THE FIRST RESTORABLE GCI FILE.           */
  /* ----------------------------------------------------------------------- */
  FileRecordPtr filePtr;
  filePtr.i = crestartInfoFile[0];
  ptrCheckGuard(filePtr, cfileFileSize, fileRecord);
  if (filePtr.p->fileStatus == FileRecord::OPEN) {
    jam();
    openingCopyGciSkipInitLab(signal, filePtr);
    return;
  }//if
  openFileRw(signal, filePtr);
  filePtr.p->reqStatus = FileRecord::OPENING_COPY_GCI;
  return;
}//Dbdih::execCOPY_GCIREQ()

void Dbdih::execDICTSTARTCONF(Signal* signal) 
{
  jamEntry();
  Uint32 nodeId = refToNode(signal->getSendersBlockRef());
  if (nodeId != getOwnNodeId()) {
    jam();
    nodeDictStartConfLab(signal);
  } else {
    jam();
    dictStartConfLab(signal);
  }//if
}//Dbdih::execDICTSTARTCONF()

void Dbdih::execFSCLOSECONF(Signal* signal) 
{
  FileRecordPtr filePtr;
  jamEntry();
  filePtr.i = signal->theData[0];
  ptrCheckGuard(filePtr, cfileFileSize, fileRecord);
  filePtr.p->fileStatus = FileRecord::CLOSED;
  FileRecord::ReqStatus status = filePtr.p->reqStatus;
  filePtr.p->reqStatus = FileRecord::IDLE;
  switch (status) {
  case FileRecord::CLOSING_GCP:
    jam();
    closingGcpLab(signal, filePtr);
    break;
  case FileRecord::CLOSING_GCP_CRASH:
    jam();
    closingGcpCrashLab(signal, filePtr);
    break;
  case FileRecord::CLOSING_TABLE_CRASH:
    jam();
    closingTableCrashLab(signal, filePtr);
    break;
  case FileRecord::CLOSING_TABLE_SR:
    jam();
    closingTableSrLab(signal, filePtr);
    break;
  case FileRecord::TABLE_CLOSE:
    jam();
    tableCloseLab(signal, filePtr);
    break;
  case FileRecord::TABLE_CLOSE_DELETE:
    jam();
    tableDeleteLab(signal, filePtr);
    break;
  default:
    ndbrequire(false);
    break;
  }//switch
  return;
}//Dbdih::execFSCLOSECONF()

void Dbdih::execFSCLOSEREF(Signal* signal) 
{
  FileRecordPtr filePtr;
  jamEntry();
  filePtr.i = signal->theData[0];
  ptrCheckGuard(filePtr, cfileFileSize, fileRecord);
  FileRecord::ReqStatus status = filePtr.p->reqStatus;
  filePtr.p->reqStatus = FileRecord::IDLE;
  switch (status) {
  case FileRecord::CLOSING_GCP:
    jam();
    break;
  case FileRecord::CLOSING_GCP_CRASH:
    jam();
    closingGcpCrashLab(signal, filePtr);
    return;
  case FileRecord::CLOSING_TABLE_CRASH:
    jam();
    closingTableCrashLab(signal, filePtr);
    return;
  case FileRecord::CLOSING_TABLE_SR:
    jam();
    break;
  case FileRecord::TABLE_CLOSE:
    jam();
    break;
  case FileRecord::TABLE_CLOSE_DELETE:
    jam();
    break;
  default:
    jam();
    break;

  }//switch
  {
    char msg[100];
    sprintf(msg, "File system close failed during FileRecord status %d", (Uint32)status);
    fsRefError(signal,__LINE__,msg);
  }

  return;
}//Dbdih::execFSCLOSEREF()

void Dbdih::execFSOPENCONF(Signal* signal) 
{
  FileRecordPtr filePtr;
  jamEntry();
  filePtr.i = signal->theData[0];
  ptrCheckGuard(filePtr, cfileFileSize, fileRecord);
  filePtr.p->fileRef = signal->theData[1];
  filePtr.p->fileStatus = FileRecord::OPEN;
  FileRecord::ReqStatus status = filePtr.p->reqStatus;
  filePtr.p->reqStatus = FileRecord::IDLE;
  switch (status) {
  case FileRecord::CREATING_GCP:
    jam();
    creatingGcpLab(signal, filePtr);
    break;
  case FileRecord::OPENING_COPY_GCI:
    jam();
    openingCopyGciSkipInitLab(signal, filePtr);
    break;
  case FileRecord::CREATING_COPY_GCI:
    jam();
    openingCopyGciSkipInitLab(signal, filePtr);
    break;
  case FileRecord::OPENING_GCP:
    jam();
    openingGcpLab(signal, filePtr);
    break;
  case FileRecord::OPENING_TABLE:
    jam();
    openingTableLab(signal, filePtr);
    break;
  case FileRecord::TABLE_CREATE:
    jam();
    tableCreateLab(signal, filePtr);
    break;
  case FileRecord::TABLE_OPEN_FOR_DELETE:
    jam();
    tableOpenLab(signal, filePtr);
    break;
  default:
    ndbrequire(false);
    break;
  }//switch
  return;
}//Dbdih::execFSOPENCONF()

void Dbdih::execFSOPENREF(Signal* signal) 
{
  FileRecordPtr filePtr;
  jamEntry();
  filePtr.i = signal->theData[0];
  ptrCheckGuard(filePtr, cfileFileSize, fileRecord);
  FileRecord::ReqStatus status = filePtr.p->reqStatus;
  filePtr.p->reqStatus = FileRecord::IDLE;
  switch (status) {
  case FileRecord::CREATING_GCP:
    /* --------------------------------------------------------------------- */
    /*   WE DID NOT MANAGE TO CREATE A GLOBAL CHECKPOINT FILE. SERIOUS ERROR */
    /*   WHICH CAUSES A SYSTEM RESTART.                                      */
    /* --------------------------------------------------------------------- */
    jam();
    break;
  case FileRecord::OPENING_COPY_GCI:
    jam();
    openingCopyGciErrorLab(signal, filePtr);
    return;
  case FileRecord::CREATING_COPY_GCI:
    jam();
    break;
  case FileRecord::OPENING_GCP:
    jam();
    openingGcpErrorLab(signal, filePtr);
    return;
  case FileRecord::OPENING_TABLE:
    jam();
    openingTableErrorLab(signal, filePtr);
    return;
  case FileRecord::TABLE_CREATE:
    jam();
    break;
  case FileRecord::TABLE_OPEN_FOR_DELETE:
    jam();
    tableDeleteLab(signal, filePtr);
    return;
  default:
    jam();
    break;
  }//switch
  {
    char msg[100];
    sprintf(msg, "File system open failed during FileRecord status %d", (Uint32)status);
    fsRefError(signal,__LINE__,msg);
  }
  return;
}//Dbdih::execFSOPENREF()

void Dbdih::execFSREADCONF(Signal* signal) 
{
  FileRecordPtr filePtr;
  jamEntry();
  filePtr.i = signal->theData[0];
  ptrCheckGuard(filePtr, cfileFileSize, fileRecord);
  FileRecord::ReqStatus status = filePtr.p->reqStatus;
  filePtr.p->reqStatus = FileRecord::IDLE;
  switch (status) {
  case FileRecord::READING_GCP:
    jam();
    readingGcpLab(signal, filePtr);
    break;
  case FileRecord::READING_TABLE:
    jam();
    readingTableLab(signal, filePtr);
    break;
  default:
    ndbrequire(false);
    break;
  }//switch
  return;
}//Dbdih::execFSREADCONF()

void Dbdih::execFSREADREF(Signal* signal) 
{
  FileRecordPtr filePtr;
  jamEntry();
  filePtr.i = signal->theData[0];
  ptrCheckGuard(filePtr, cfileFileSize, fileRecord);
  FileRecord::ReqStatus status = filePtr.p->reqStatus;
  filePtr.p->reqStatus = FileRecord::IDLE;
  switch (status) {
  case FileRecord::READING_GCP:
    jam();
    readingGcpErrorLab(signal, filePtr);
    return;
  case FileRecord::READING_TABLE:
    jam();
    readingTableErrorLab(signal, filePtr);
    return;
  default:
    break;
  }//switch
  {
    char msg[100];
    sprintf(msg, "File system read failed during FileRecord status %d", (Uint32)status);
    fsRefError(signal,__LINE__,msg);
  }
}//Dbdih::execFSREADREF()

void Dbdih::execFSWRITECONF(Signal* signal) 
{
  FileRecordPtr filePtr;
  jamEntry();
  filePtr.i = signal->theData[0];
  ptrCheckGuard(filePtr, cfileFileSize, fileRecord);
  FileRecord::ReqStatus status = filePtr.p->reqStatus;
  filePtr.p->reqStatus = FileRecord::IDLE;
  switch (status) {
  case FileRecord::WRITING_COPY_GCI:
    jam();
    writingCopyGciLab(signal, filePtr);
    break;
  case FileRecord::WRITE_INIT_GCP:
    jam();
    writeInitGcpLab(signal, filePtr);
    break;
  case FileRecord::TABLE_WRITE:
    jam();
    tableWriteLab(signal, filePtr);
    break;
  default:
    ndbrequire(false);
    break;
  }//switch
  return;
}//Dbdih::execFSWRITECONF()

void Dbdih::execFSWRITEREF(Signal* signal) 
{
  FileRecordPtr filePtr;
  jamEntry();
  filePtr.i = signal->theData[0];
  ptrCheckGuard(filePtr, cfileFileSize, fileRecord);
  FileRecord::ReqStatus status = filePtr.p->reqStatus;
  filePtr.p->reqStatus = FileRecord::IDLE;
  switch (status) {
  case FileRecord::WRITING_COPY_GCI:
    /* --------------------------------------------------------------------- */
    /*  EVEN CREATING THE FILE DID NOT WORK. WE WILL THEN CRASH.             */
    /*  ERROR IN WRITING FILE. WE WILL NOT CONTINUE FROM HERE.               */
    /* --------------------------------------------------------------------- */
    jam();
    break;
  case FileRecord::WRITE_INIT_GCP:
    /* --------------------------------------------------------------------- */
    /*   AN ERROR OCCURRED IN WRITING A GCI FILE WHICH IS A SERIOUS ERROR    */
    /*   THAT CAUSE A SYSTEM RESTART.                                        */
    /* --------------------------------------------------------------------- */
    jam();
    break;
  case FileRecord::TABLE_WRITE:
    jam();
    break;
  default:
    jam();
    break;
  }//switch
  {
    char msg[100];
    sprintf(msg, "File system write failed during FileRecord status %d", (Uint32)status);
    fsRefError(signal,__LINE__,msg);
  }
  return;
}//Dbdih::execFSWRITEREF()

void Dbdih::execGETGCIREQ(Signal* signal) 
{

  jamEntry();
  Uint32 userPtr = signal->theData[0];
  BlockReference userRef = signal->theData[1];

  signal->theData[0] = userPtr;
  signal->theData[1] = SYSFILE->newestRestorableGCI;
  sendSignal(userRef, GSN_GETGCICONF, signal, 2, JBB);
}//Dbdih::execGETGCIREQ()

void Dbdih::execREAD_CONFIG_REQ(Signal* signal) 
{
  const ReadConfigReq * req = (ReadConfigReq*)signal->getDataPtr();
  Uint32 ref = req->senderRef;
  Uint32 senderData = req->senderData;
  ndbrequire(req->noOfParameters == 0);

  jamEntry();

  const ndb_mgm_configuration_iterator * p = 
    theConfiguration.getOwnConfigIterator();
  ndbrequireErr(p != 0, NDBD_EXIT_INVALID_CONFIG);

  initData();

  ndbrequireErr(!ndb_mgm_get_int_parameter(p, CFG_DIH_API_CONNECT, 
					   &capiConnectFileSize),
		NDBD_EXIT_INVALID_CONFIG);
  ndbrequireErr(!ndb_mgm_get_int_parameter(p, CFG_DIH_CONNECT,
					   &cconnectFileSize),
		NDBD_EXIT_INVALID_CONFIG);
  ndbrequireErr(!ndb_mgm_get_int_parameter(p, CFG_DIH_FRAG_CONNECT, 
					   &cfragstoreFileSize),
		NDBD_EXIT_INVALID_CONFIG);
  ndbrequireErr(!ndb_mgm_get_int_parameter(p, CFG_DIH_REPLICAS, 
					   &creplicaFileSize),
		NDBD_EXIT_INVALID_CONFIG);
  ndbrequireErr(!ndb_mgm_get_int_parameter(p, CFG_DIH_TABLE, &ctabFileSize),
		NDBD_EXIT_INVALID_CONFIG);
  cfileFileSize = (2 * ctabFileSize) + 2;
  initRecords();
  initialiseRecordsLab(signal, 0, ref, senderData);
  return;
}//Dbdih::execSIZEALT_REP()

void Dbdih::execSTART_COPYREF(Signal* signal) 
{
  jamEntry();
  ndbrequire(false);
}//Dbdih::execSTART_COPYREF()

void Dbdih::execSTART_FRAGCONF(Signal* signal) 
{
  (void)signal;  // Don't want compiler warning
  /* ********************************************************************* */
  /*  If anyone wants to add functionality in this method, be aware that   */
  /*  for temporary tables no START_FRAGREQ is sent and therefore no       */
  /*  START_FRAGCONF signal will be received for those tables!!            */
  /* ********************************************************************* */
  jamEntry();
  return;
}//Dbdih::execSTART_FRAGCONF()

void Dbdih::execSTART_MEREF(Signal* signal) 
{
  jamEntry();
  ndbrequire(false);
}//Dbdih::execSTART_MEREF()

void Dbdih::execTAB_COMMITREQ(Signal* signal) 
{
  TabRecordPtr tabPtr;
  jamEntry();
  Uint32 tdictPtr = signal->theData[0];
  BlockReference tdictBlockref = signal->theData[1];
  tabPtr.i = signal->theData[2];
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);

  ndbrequire(tabPtr.p->tabStatus == TabRecord::TS_CREATING);
  tabPtr.p->tabStatus = TabRecord::TS_ACTIVE;
  signal->theData[0] = tdictPtr;
  signal->theData[1] = cownNodeId;
  signal->theData[2] = tabPtr.i;
  sendSignal(tdictBlockref, GSN_TAB_COMMITCONF, signal, 3, JBB);
  return;
}//Dbdih::execTAB_COMMITREQ()

/*
  3.2   S T A N D A R D   S U B P R O G R A M S   I N   P L E X
  *************************************************************
  */
/*
  3.2.1   S T A R T /  R E S T A R T
  **********************************
  */
/*****************************************************************************/
/* **********     START / RESTART MODULE                         *************/
/*****************************************************************************/
/*
  3.2.1.1    LOADING   O W N   B L O C K  R E F E R E N C E (ABSOLUTE PHASE 1)
  *****************************************************************************
  */
void Dbdih::execDIH_RESTARTREQ(Signal* signal) 
{
  jamEntry();
  cntrlblockref = signal->theData[0];
  if(theConfiguration.getInitialStart()){
    sendSignal(cntrlblockref, GSN_DIH_RESTARTREF, signal, 1, JBB);
  } else {
    readGciFileLab(signal);
  }
  return;
}//Dbdih::execDIH_RESTARTREQ()

void Dbdih::execSTTOR(Signal* signal) 
{
  jamEntry();

  signal->theData[0] = 0;
  signal->theData[1] = 0;
  signal->theData[2] = 0;
  signal->theData[3] = 1;   // Next start phase
  signal->theData[4] = 255; // Next start phase
  sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 5, JBB);
  return;
}//Dbdih::execSTTOR()

void Dbdih::initialStartCompletedLab(Signal* signal) 
{
  /*-------------------------------------------------------------------------*/
  /* NOW THAT (RE)START IS COMPLETED WE CAN START THE LCP.*/
  /*-------------------------------------------------------------------------*/
  return;
}//Dbdih::initialStartCompletedLab()

/*
 * ***************************************************************************
 * S E N D I N G   R E P L Y  T O  S T A R T /  R E S T A R T   R E Q U E S T S
 * ****************************************************************************
 */
void Dbdih::ndbsttorry10Lab(Signal* signal, Uint32 _line) 
{
  /*-------------------------------------------------------------------------*/
  // AN NDB START PHASE HAS BEEN COMPLETED. WHEN START PHASE 6 IS COMPLETED WE
  // RECORD THAT THE SYSTEM IS RUNNING.
  /*-------------------------------------------------------------------------*/
  signal->theData[0] = reference();
  sendSignal(cntrlblockref, GSN_NDB_STTORRY, signal, 1, JBB);
  return;
}//Dbdih::ndbsttorry10Lab()

/*
****************************************
I N T E R N A L  P H A S E S
****************************************
*/
/*---------------------------------------------------------------------------*/
/*NDB_STTOR                              START SIGNAL AT START/RESTART       */
/*---------------------------------------------------------------------------*/
void Dbdih::execNDB_STTOR(Signal* signal) 
{
  jamEntry();
  BlockReference cntrRef = signal->theData[0];    /* SENDERS BLOCK REFERENCE */
  Uint32 ownNodeId = signal->theData[1];          /* OWN PROCESSOR ID*/
  Uint32 phase = signal->theData[2];              /* INTERNAL START PHASE*/
  Uint32 typestart = signal->theData[3];

  cstarttype = typestart;
  cstartPhase = phase;

  switch (phase){
  case ZNDB_SPH1:
    jam();
    /*----------------------------------------------------------------------*/
    /* Set the delay between local checkpoints in ndb startphase 1.         */
    /*----------------------------------------------------------------------*/
    cownNodeId = ownNodeId;
    /*-----------------------------------------------------------------------*/
    // Compute all static block references in this node as part of
    // ndb start phase 1.    
    /*-----------------------------------------------------------------------*/
    cntrlblockref = cntrRef;
    clocaltcblockref = calcTcBlockRef(ownNodeId);
    clocallqhblockref = calcLqhBlockRef(ownNodeId);
    cdictblockref = calcDictBlockRef(ownNodeId);
    ndbsttorry10Lab(signal, __LINE__);
    break;
    
  case ZNDB_SPH2:
    jam();
    /*-----------------------------------------------------------------------*/
    // Set the number of replicas,  maximum is 4 replicas.
    // Read the ndb nodes from the configuration.
    /*-----------------------------------------------------------------------*/
    
    /*-----------------------------------------------------------------------*/
    // For node restarts we will also add a request for permission
    // to continue the system restart.
    // The permission is given by the master node in the alive set.  
    /*-----------------------------------------------------------------------*/
    createMutexes(signal, 0);
    break;
    
  case ZNDB_SPH3:
    jam();
    /*-----------------------------------------------------------------------*/
    // Non-master nodes performing an initial start will execute
    // the start request here since the
    // initial start do not synchronise so much from the master.
    // In the master nodes the start
    // request will be sent directly to dih (in ndb_startreq) when all
    // nodes have completed phase 3 of the start.    
    /*-----------------------------------------------------------------------*/
    cmasterState = MASTER_IDLE;
    if(cstarttype == NodeState::ST_INITIAL_START ||
       cstarttype == NodeState::ST_SYSTEM_RESTART){
      jam();
      cmasterState = isMaster() ? MASTER_ACTIVE : MASTER_IDLE;
    }
    if (!isMaster() && cstarttype == NodeState::ST_INITIAL_START) {
      jam();
      ndbStartReqLab(signal, cntrRef);
      return;
    }//if
    ndbsttorry10Lab(signal, __LINE__);
    break;
    
  case ZNDB_SPH4:
    jam();
    c_lcpState.setLcpStatus(LCP_STATUS_IDLE, __LINE__);
    cmasterTakeOverNode = ZNIL;
    switch(typestart){
    case NodeState::ST_INITIAL_START:
      jam();
      ndbsttorry10Lab(signal, __LINE__);
      return;
    case NodeState::ST_SYSTEM_RESTART:
      jam();
      if (isMaster()) {
	jam();
	systemRestartTakeOverLab(signal);
	if (anyActiveTakeOver() && false) {
	  jam();
	  ndbout_c("1 - anyActiveTakeOver == true");
	  return;
	}
      }
      ndbsttorry10Lab(signal, __LINE__);
      return;
    case NodeState::ST_INITIAL_NODE_RESTART:
    case NodeState::ST_NODE_RESTART:
      jam();
      /***********************************************************************
       * When starting nodes while system is operational we must be controlled
       * by the master since only one node restart is allowed at a time. 
       * When this signal is confirmed the master has also copied the 
       * dictionary and the distribution information.
       */
      StartMeReq * req = (StartMeReq*)&signal->theData[0];
      req->startingRef = reference();
      req->startingVersion = 0; // Obsolete
      sendSignal(cmasterdihref, GSN_START_MEREQ, signal, 
		 StartMeReq::SignalLength, JBB);
      return;
    }
    ndbrequire(false);
    break;
  case ZNDB_SPH5:
    jam();
    switch(typestart){
    case NodeState::ST_INITIAL_START:
    case NodeState::ST_SYSTEM_RESTART:
      jam();
      jam();
      /*---------------------------------------------------------------------*/
      // WE EXECUTE A LOCAL CHECKPOINT AS A PART OF A SYSTEM RESTART.
      // THE IDEA IS THAT WE NEED TO
      // ENSURE THAT WE CAN RECOVER FROM PROBLEMS CAUSED BY MANY NODE
      // CRASHES THAT CAUSES THE LOG
      // TO GROW AND THE NUMBER OF LOG ROUNDS TO EXECUTE TO GROW.
      // THIS CAN OTHERWISE GET US INTO
      // A SITUATION WHICH IS UNREPAIRABLE. THUS WE EXECUTE A CHECKPOINT
      // BEFORE ALLOWING ANY TRANSACTIONS TO START.
      /*---------------------------------------------------------------------*/
      if (!isMaster()) {
	jam();
	ndbsttorry10Lab(signal, __LINE__);
	return;
      }//if
      
      c_lcpState.immediateLcpStart = true;
      cwaitLcpSr = true;
      checkLcpStart(signal, __LINE__);
      return;
    case NodeState::ST_NODE_RESTART:
    case NodeState::ST_INITIAL_NODE_RESTART:
      jam();
      signal->theData[0] = cownNodeId;
      signal->theData[1] = reference();
      sendSignal(cmasterdihref, GSN_START_COPYREQ, signal, 2, JBB);
      return;
    }
    ndbrequire(false);
  case ZNDB_SPH6:
    jam();
    switch(typestart){
    case NodeState::ST_INITIAL_START:
    case NodeState::ST_SYSTEM_RESTART:
      jam();
      if(isMaster()){
	jam();
	startGcp(signal);
      }
      ndbsttorry10Lab(signal, __LINE__);
      return;
    case NodeState::ST_NODE_RESTART:
    case NodeState::ST_INITIAL_NODE_RESTART:
      ndbsttorry10Lab(signal, __LINE__);
      return;
    }
    ndbrequire(false);
    break;
  default:
    jam();
    ndbsttorry10Lab(signal, __LINE__);
    break;
  }//switch
}//Dbdih::execNDB_STTOR()

void
Dbdih::createMutexes(Signal * signal, Uint32 count){
  Callback c = { safe_cast(&Dbdih::createMutex_done), count };

  switch(count){
  case 0:{
    Mutex mutex(signal, c_mutexMgr, c_startLcpMutexHandle);
    mutex.create(c);
    return;
  }
  case 1:{
    Mutex mutex(signal, c_mutexMgr, c_switchPrimaryMutexHandle);
    mutex.create(c);
    return;
  }
  }    

  signal->theData[0] = reference();
  sendSignal(cntrlblockref, GSN_READ_NODESREQ, signal, 1, JBB);
}

void
Dbdih::createMutex_done(Signal* signal, Uint32 senderData, Uint32 retVal){
  jamEntry();
  ndbrequire(retVal == 0);

  switch(senderData){
  case 0:{
    Mutex mutex(signal, c_mutexMgr, c_startLcpMutexHandle);
    mutex.release();
  }
  case 1:{
    Mutex mutex(signal, c_mutexMgr, c_switchPrimaryMutexHandle);
    mutex.release();
  }
  }    
  
  createMutexes(signal, senderData + 1);
}

/*****************************************************************************/
/* ------------------------------------------------------------------------- */
/*       WE HAVE BEEN REQUESTED BY NDBCNTR TO PERFORM A RESTART OF THE       */
/*       DATABASE TABLES.                                                    */
/*       THIS SIGNAL IS SENT AFTER COMPLETING PHASE 3 IN ALL BLOCKS IN A     */
/*       SYSTEM RESTART. WE WILL ALSO JUMP TO THIS LABEL FROM PHASE 3 IN AN  */
/*       INITIAL START.                                                      */
/* ------------------------------------------------------------------------- */
/*****************************************************************************/
void Dbdih::execNDB_STARTREQ(Signal* signal) 
{
  jamEntry();
  BlockReference ref = signal->theData[0];
  cstarttype = signal->theData[1];
  ndbStartReqLab(signal, ref);
}//Dbdih::execNDB_STARTREQ()

void Dbdih::ndbStartReqLab(Signal* signal, BlockReference ref) 
{
  cndbStartReqBlockref = ref;
  if (cstarttype == NodeState::ST_INITIAL_START) {
    jam();
    initRestartInfo();
    initGciFilesLab(signal);
    return;
  }
  
  ndbrequire(isMaster());
  copyGciLab(signal, CopyGCIReq::RESTART); // We have already read the file!
}//Dbdih::ndbStartReqLab()

void Dbdih::execREAD_NODESCONF(Signal* signal) 
{
  unsigned i;
  ReadNodesConf * const readNodes = (ReadNodesConf *)&signal->theData[0];
  jamEntry();
  Uint32 nodeArray[MAX_NDB_NODES];

  csystemnodes  = readNodes->noOfNodes;
  cmasterNodeId = readNodes->masterNodeId;
  int index = 0;
  NdbNodeBitmask tmp; tmp.assign(2, readNodes->allNodes);
  for (i = 1; i < MAX_NDB_NODES; i++){
    jam();
    if(tmp.get(i)){
      jam();
      nodeArray[index] = i;
      if(NodeBitmask::get(readNodes->inactiveNodes, i) == false){
        jam();
        con_lineNodes++;        
      }//if      
      index++;
    }//if
  }//for  
  
  if(cstarttype == NodeState::ST_SYSTEM_RESTART || 
     cstarttype == NodeState::ST_NODE_RESTART){

    for(i = 1; i<MAX_NDB_NODES; i++){
      const Uint32 stat = Sysfile::getNodeStatus(i, SYSFILE->nodeStatus);
      if(stat == Sysfile::NS_NotDefined && !tmp.get(i)){
	jam();
	continue;
      }
      
      if(tmp.get(i) && stat != Sysfile::NS_NotDefined){
	jam();
	continue;
      }
      char buf[255];
      BaseString::snprintf(buf, sizeof(buf), 
	       "Illegal configuration change."
	       " Initial start needs to be performed "
	       " when changing no of storage nodes (node %d)", i);
      progError(__LINE__, NDBD_EXIT_INVALID_CONFIG, buf);
    }
  }
  
  ndbrequire(csystemnodes >= 1 && csystemnodes < MAX_NDB_NODES);  
  if (cstarttype == NodeState::ST_INITIAL_START) {
    jam();
    ndbrequire(cnoReplicas <= csystemnodes);
    calculateHotSpare();
    ndbrequire(cnoReplicas <= (csystemnodes - cnoHotSpare));
  }//if

  cmasterdihref = calcDihBlockRef(cmasterNodeId);
  /*-------------------------------------------------------------------------*/
  /* MAKE THE LIST OF PRN-RECORD WHICH IS ONE OF THE NODES-LIST IN THIS BLOCK*/
  /*-------------------------------------------------------------------------*/
  makePrnList(readNodes, nodeArray);
  if (cstarttype == NodeState::ST_INITIAL_START) {
    jam();
    /**----------------------------------------------------------------------
     * WHEN WE INITIALLY START A DATABASE WE WILL CREATE NODE GROUPS. 
     * ALL NODES ARE PUT INTO NODE GROUPS ALTHOUGH HOT SPARE NODES ARE PUT 
     * INTO A SPECIAL NODE GROUP. IN EACH NODE GROUP WE HAVE THE SAME AMOUNT 
     * OF NODES AS THERE ARE NUMBER OF REPLICAS. 
     * ONE POSSIBLE USAGE OF NODE GROUPS ARE TO MAKE A NODE GROUP A COMPLETE 
     * FRAGMENT OF THE DATABASE. THIS MEANS THAT ALL REPLICAS WILL BE STORED
     * IN THE NODE GROUP.
     *-----------------------------------------------------------------------*/
    makeNodeGroups(nodeArray);
  }//if
  ndbrequire(checkNodeAlive(cmasterNodeId));
  if (cstarttype == NodeState::ST_INITIAL_START) {
    jam();
    /**-----------------------------------------------------------------------
     * INITIALISE THE SECOND NODE-LIST AND SET NODE BITS AND SOME NODE STATUS.
     * VERY CONNECTED WITH MAKE_NODE_GROUPS. CHANGING ONE WILL AFFECT THE 
     * OTHER AS WELL.
     *-----------------------------------------------------------------------*/
    setInitialActiveStatus();
  } else if (cstarttype == NodeState::ST_SYSTEM_RESTART) {
    jam();
    /*empty*/;
  } else if ((cstarttype == NodeState::ST_NODE_RESTART) || 
             (cstarttype == NodeState::ST_INITIAL_NODE_RESTART)) {
    jam();
    nodeRestartPh2Lab(signal);
    return;
  } else {
    ndbrequire(false);
  }//if
  /**------------------------------------------------------------------------
   * ESTABLISH CONNECTIONS WITH THE OTHER DIH BLOCKS AND INITIALISE THIS 
   * NODE-LIST THAT HANDLES CONNECTION WITH OTHER DIH BLOCKS. 
   *-------------------------------------------------------------------------*/
  ndbsttorry10Lab(signal, __LINE__);
}//Dbdih::execREAD_NODESCONF()

/*---------------------------------------------------------------------------*/
/*                    START NODE LOGIC FOR NODE RESTART                      */
/*---------------------------------------------------------------------------*/
void Dbdih::nodeRestartPh2Lab(Signal* signal) 
{
  /*------------------------------------------------------------------------*/
  // REQUEST FOR PERMISSION FROM MASTER TO START A NODE IN AN ALREADY
  // RUNNING SYSTEM.
  /*------------------------------------------------------------------------*/
  StartPermReq * const req = (StartPermReq *)&signal->theData[0];

  req->blockRef  = reference();
  req->nodeId    = cownNodeId;
  req->startType = cstarttype;
  sendSignal(cmasterdihref, GSN_START_PERMREQ, signal, 3, JBB);
}//Dbdih::nodeRestartPh2Lab()

void Dbdih::execSTART_PERMCONF(Signal* signal) 
{
  jamEntry();
  CRASH_INSERTION(7121);
  Uint32 nodeId = signal->theData[0];
  cfailurenr = signal->theData[1];
  ndbrequire(nodeId == cownNodeId);
  ndbsttorry10Lab(signal, __LINE__);
}//Dbdih::execSTART_PERMCONF()

void Dbdih::execSTART_PERMREF(Signal* signal) 
{
  jamEntry();
  Uint32 errorCode = signal->theData[1];
  if (errorCode == ZNODE_ALREADY_STARTING_ERROR) {
    jam();
    /*-----------------------------------------------------------------------*/
    // The master was busy adding another node. We will wait for a second and
    // try again.
    /*-----------------------------------------------------------------------*/
    signal->theData[0] = DihContinueB::ZSTART_PERMREQ_AGAIN;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 3000, 1);
    return;
  }//if
  /*------------------------------------------------------------------------*/
  // Some node process in another node involving our node was still active. We
  // will recover from this by crashing here. 
  // This is controlled restart using the
  // already existing features of node crashes. It is not a bug getting here.
  /*-------------------------------------------------------------------------*/
  ndbrequire(false);
  return;
}//Dbdih::execSTART_PERMREF()

/*---------------------------------------------------------------------------*/
/*       THIS SIGNAL IS RECEIVED IN THE STARTING NODE WHEN THE START_MEREQ   */
/*       HAS BEEN EXECUTED IN THE MASTER NODE.                               */
/*---------------------------------------------------------------------------*/
void Dbdih::execSTART_MECONF(Signal* signal) 
{
  jamEntry();
  StartMeConf * const startMe = (StartMeConf *)&signal->theData[0];  
  Uint32 nodeId = startMe->startingNodeId;
  const Uint32 startWord = startMe->startWord;
  Uint32 i;
  
  CRASH_INSERTION(7130);
  ndbrequire(nodeId == cownNodeId);
  arrGuard(startWord + StartMeConf::DATA_SIZE, sizeof(cdata)/4);
  for(i = 0; i < StartMeConf::DATA_SIZE; i++)
    cdata[startWord+i] = startMe->data[i];
  
  if(startWord + StartMeConf::DATA_SIZE < Sysfile::SYSFILE_SIZE32){
    jam();
    /**
     * We are still waiting for data
     */
    return;
  }
  jam();

  /**
   * Copy into sysfile
   *
   * But dont copy lastCompletedGCI:s
   */
  Uint32 tempGCP[MAX_NDB_NODES];
  for(i = 0; i < MAX_NDB_NODES; i++)
    tempGCP[i] = SYSFILE->lastCompletedGCI[i];

  for(i = 0; i < Sysfile::SYSFILE_SIZE32; i++)
    sysfileData[i] = cdata[i];
  for(i = 0; i < MAX_NDB_NODES; i++)
    SYSFILE->lastCompletedGCI[i] = tempGCP[i];

  setNodeActiveStatus();
  setNodeGroups();
  ndbsttorry10Lab(signal, __LINE__);
}//Dbdih::execSTART_MECONF()

void Dbdih::execSTART_COPYCONF(Signal* signal) 
{
  jamEntry();
  Uint32 nodeId = signal->theData[0];
  ndbrequire(nodeId == cownNodeId);
  CRASH_INSERTION(7132);
  ndbsttorry10Lab(signal, __LINE__);
  return;
}//Dbdih::execSTART_COPYCONF()

/*---------------------------------------------------------------------------*/
/*                    MASTER LOGIC FOR NODE RESTART                          */
/*---------------------------------------------------------------------------*/
/*                    NODE RESTART PERMISSION REQUEST                        */
/*---------------------------------------------------------------------------*/
// A REQUEST FROM A STARTING NODE TO PERFORM A NODE RESTART. IF NO OTHER NODE
// IS ACTIVE IN PERFORMING A NODE RESTART AND THERE ARE NO ACTIVE PROCESSES IN
// THIS NODE INVOLVING THE STARTING NODE  THIS REQUEST WILL BE GRANTED.
/*---------------------------------------------------------------------------*/
void Dbdih::execSTART_PERMREQ(Signal* signal)
{
  StartPermReq * const req = (StartPermReq*)&signal->theData[0];  
  jamEntry();
  const BlockReference retRef = req->blockRef;
  const Uint32 nodeId   = req->nodeId;
  const Uint32 typeStart = req->startType;
  
  CRASH_INSERTION(7122);
  ndbrequire(isMaster());
  ndbrequire(refToNode(retRef) == nodeId);
  if ((c_nodeStartMaster.activeState) ||
      (c_nodeStartMaster.wait != ZFALSE)) {
    jam();
    signal->theData[0] = nodeId;
    signal->theData[1] = ZNODE_ALREADY_STARTING_ERROR;
    sendSignal(retRef, GSN_START_PERMREF, signal, 2, JBB);
    return;
  }//if
  if (getNodeStatus(nodeId) != NodeRecord::DEAD){
    ndbout << "nodeStatus in START_PERMREQ = " 
	   << (Uint32) getNodeStatus(nodeId) << endl;
    ndbrequire(false);
  }//if

  /*----------------------------------------------------------------------
   * WE START THE INCLUSION PROCEDURE 
   * ---------------------------------------------------------------------*/
  c_nodeStartMaster.failNr   = cfailurenr;
  c_nodeStartMaster.wait     = ZFALSE;
  c_nodeStartMaster.startInfoErrorCode = 0;
  c_nodeStartMaster.startNode = nodeId;
  c_nodeStartMaster.activeState = true;
  c_nodeStartMaster.m_outstandingGsn =  GSN_START_INFOREQ;
  
  setNodeStatus(nodeId, NodeRecord::STARTING);
  /**
   * But if it's a NodeState::ST_INITIAL_NODE_RESTART
   *
   * We first have to clear LCP's
   * For normal node restart we simply ensure that all nodes
   * are informed of the node restart
   */
  StartInfoReq *const r =(StartInfoReq*)&signal->theData[0];
  r->startingNodeId = nodeId;
  r->typeStart = typeStart;
  r->systemFailureNo = cfailurenr;
  sendLoopMacro(START_INFOREQ, sendSTART_INFOREQ);
}//Dbdih::execSTART_PERMREQ()

void Dbdih::execSTART_INFOREF(Signal* signal)
{
  StartInfoRef * ref = (StartInfoRef*)&signal->theData[0];
  if (getNodeStatus(ref->startingNodeId) != NodeRecord::STARTING) {
    jam();
    return;
  }//if
  ndbrequire(c_nodeStartMaster.startNode == ref->startingNodeId);
  c_nodeStartMaster.startInfoErrorCode = ref->errorCode;
  startInfoReply(signal, ref->sendingNodeId);
}//Dbdih::execSTART_INFOREF()

void Dbdih::execSTART_INFOCONF(Signal* signal)
{
  jamEntry();
  StartInfoConf * conf = (StartInfoConf*)&signal->theData[0];
  if (getNodeStatus(conf->startingNodeId) != NodeRecord::STARTING) {
    jam();
    return;
  }//if
  ndbrequire(c_nodeStartMaster.startNode == conf->startingNodeId);
  startInfoReply(signal, conf->sendingNodeId);
}//Dbdih::execSTART_INFOCONF()

void Dbdih::startInfoReply(Signal* signal, Uint32 nodeId)
{
  receiveLoopMacro(START_INFOREQ, nodeId);
  /**
   * We're finished with the START_INFOREQ's 
   */
  if (c_nodeStartMaster.startInfoErrorCode == 0) {
    jam();
    /**
     * Everything has been a success so far
     */
    StartPermConf * conf = (StartPermConf*)&signal->theData[0];
    conf->startingNodeId = c_nodeStartMaster.startNode;
    conf->systemFailureNo = cfailurenr;
    sendSignal(calcDihBlockRef(c_nodeStartMaster.startNode), 
	       GSN_START_PERMCONF, signal, StartPermConf::SignalLength, JBB);
    c_nodeStartMaster.m_outstandingGsn = GSN_START_PERMCONF;
  } else {
    jam();
    StartPermRef * ref = (StartPermRef*)&signal->theData[0];
    ref->startingNodeId = c_nodeStartMaster.startNode;
    ref->errorCode = c_nodeStartMaster.startInfoErrorCode;
    sendSignal(calcDihBlockRef(c_nodeStartMaster.startNode), 
	       GSN_START_PERMREF, signal, StartPermRef::SignalLength, JBB);
    nodeResetStart();
  }//if
}//Dbdih::startInfoReply()

/*---------------------------------------------------------------------------*/
/*                    NODE RESTART CONTINUE REQUEST                          */
/*---------------------------------------------------------------------------*/
// THIS SIGNAL AND THE CODE BELOW IS EXECUTED BY THE MASTER WHEN IT HAS BEEN
// REQUESTED TO START UP A NEW NODE. The master instructs the starting node
// how to set up its log for continued execution.
/*---------------------------------------------------------------------------*/
void Dbdih::execSTART_MEREQ(Signal* signal) 
{
  StartMeReq * req = (StartMeReq*)&signal->theData[0];
  jamEntry();
  const BlockReference Tblockref = req->startingRef;
  const Uint32 Tnodeid = refToNode(Tblockref);

  ndbrequire(isMaster());
  ndbrequire(c_nodeStartMaster.startNode == Tnodeid);
  ndbrequire(getNodeStatus(Tnodeid) == NodeRecord::STARTING);
  
  sendSTART_RECREQ(signal, Tnodeid);
}//Dbdih::execSTART_MEREQ()

void Dbdih::nodeRestartStartRecConfLab(Signal* signal) 
{
  c_nodeStartMaster.blockLcp = true;
  if ((c_lcpState.lcpStatus != LCP_STATUS_IDLE) &&
      (c_lcpState.lcpStatus != LCP_TCGET)) {
    jam();
    /*-----------------------------------------------------------------------*/
    // WE WILL NOT ALLOW A NODE RESTART TO COME IN WHEN A LOCAL CHECKPOINT IS
    // ONGOING. IT WOULD COMPLICATE THE LCP PROTOCOL TOO MUCH. WE WILL ADD THIS
    // LATER.
    /*-----------------------------------------------------------------------*/
    return;
  }//if
  lcpBlockedLab(signal);
}//Dbdih::nodeRestartStartRecConfLab()

void Dbdih::lcpBlockedLab(Signal* signal) 
{
  ndbrequire(getNodeStatus(c_nodeStartMaster.startNode)==NodeRecord::STARTING);
  /*------------------------------------------------------------------------*/
  // NOW WE HAVE COPIED ALL INFORMATION IN DICT WE ARE NOW READY TO COPY ALL
  // INFORMATION IN DIH TO THE NEW NODE.
  /*------------------------------------------------------------------------*/
  c_nodeStartMaster.wait = 10;
  signal->theData[0] = DihContinueB::ZCOPY_NODE;
  signal->theData[1] = 0;
  sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
  c_nodeStartMaster.m_outstandingGsn = GSN_COPY_TABREQ;
}//Dbdih::lcpBlockedLab()

void Dbdih::nodeDictStartConfLab(Signal* signal) 
{
  /*-------------------------------------------------------------------------*/
  // NOW WE HAVE COPIED BOTH DIH AND DICT INFORMATION. WE ARE NOW READY TO
  // INTEGRATE THE NODE INTO THE LCP AND GCP PROTOCOLS AND TO ALLOW UPDATES OF
  // THE DICTIONARY AGAIN.
  /*-------------------------------------------------------------------------*/
  c_nodeStartMaster.wait = ZFALSE;
  c_nodeStartMaster.blockGcp = true;
  if (cgcpStatus != GCP_READY) {
    /*-----------------------------------------------------------------------*/
    // The global checkpoint is executing. Wait until it is completed before we
    // continue processing the node recovery.
    /*-----------------------------------------------------------------------*/
    jam();
    return;
  }//if
  gcpBlockedLab(signal);

  /*-----------------------------------------------------------------*/
  // Report that node restart has completed copy of dictionary.
  /*-----------------------------------------------------------------*/
  signal->theData[0] = NDB_LE_NR_CopyDict;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 1, JBB);
}//Dbdih::nodeDictStartConfLab()

void Dbdih::dihCopyCompletedLab(Signal* signal)
{
  BlockReference ref = calcDictBlockRef(c_nodeStartMaster.startNode);
  DictStartReq * req = (DictStartReq*)&signal->theData[0];
  req->restartGci = cnewgcp;
  req->senderRef = reference();
  sendSignal(ref, GSN_DICTSTARTREQ,
             signal, DictStartReq::SignalLength, JBB);
  c_nodeStartMaster.m_outstandingGsn = GSN_DICTSTARTREQ;
  c_nodeStartMaster.wait = 0;
}//Dbdih::dihCopyCompletedLab()

void Dbdih::gcpBlockedLab(Signal* signal)
{
  /*-----------------------------------------------------------------*/
  // Report that node restart has completed copy of distribution info.
  /*-----------------------------------------------------------------*/
  signal->theData[0] = NDB_LE_NR_CopyDistr;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 1, JBB);

  /**
   * The node DIH will be part of LCP
   */
  NodeRecordPtr nodePtr;
  nodePtr.i = c_nodeStartMaster.startNode;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
  nodePtr.p->m_inclDihLcp = true;
  
  /*-------------------------------------------------------------------------*/
  // NOW IT IS TIME TO INFORM ALL OTHER NODES IN THE CLUSTER OF THE STARTED
  // NODE SUCH THAT THEY ALSO INCLUDE THE NODE IN THE NODE LISTS AND SO FORTH.
  /*------------------------------------------------------------------------*/
  sendLoopMacro(INCL_NODEREQ, sendINCL_NODEREQ);
  /*-------------------------------------------------------------------------*/
  // We also need to send to the starting node to ensure he is aware of the
  // global checkpoint id and the correct state. We do not wait for any reply
  // since the starting node will not send any.
  /*-------------------------------------------------------------------------*/
  sendINCL_NODEREQ(signal, c_nodeStartMaster.startNode);
}//Dbdih::gcpBlockedLab()

/*---------------------------------------------------------------------------*/
// THIS SIGNAL IS EXECUTED IN BOTH SLAVES AND IN THE MASTER
/*---------------------------------------------------------------------------*/
void Dbdih::execINCL_NODECONF(Signal* signal) 
{
  Uint32 TsendNodeId;
  Uint32 TstartNode_or_blockref;
  
  jamEntry();
  TstartNode_or_blockref = signal->theData[0];
  TsendNodeId = signal->theData[1];

  if (TstartNode_or_blockref == clocallqhblockref) {
    jam();
    /*-----------------------------------------------------------------------*/
    // THIS SIGNAL CAME FROM THE LOCAL LQH BLOCK. 
    // WE WILL NOW SEND INCLUDE TO THE TC BLOCK.
    /*-----------------------------------------------------------------------*/
    signal->theData[0] = reference();
    signal->theData[1] = c_nodeStartSlave.nodeId;
    sendSignal(clocaltcblockref, GSN_INCL_NODEREQ, signal, 2, JBB);
    return;
  }//if
  if (TstartNode_or_blockref == clocaltcblockref) {
    jam();
    /*----------------------------------------------------------------------*/
    // THIS SIGNAL CAME FROM THE LOCAL LQH BLOCK. 
    // WE WILL NOW SEND INCLUDE TO THE DICT BLOCK.
    /*----------------------------------------------------------------------*/
    signal->theData[0] = reference();
    signal->theData[1] = c_nodeStartSlave.nodeId;
    sendSignal(cdictblockref, GSN_INCL_NODEREQ, signal, 2, JBB);
    return;
  }//if
  if (TstartNode_or_blockref == cdictblockref) {
    jam();
    /*-----------------------------------------------------------------------*/
    // THIS SIGNAL CAME FROM THE LOCAL DICT BLOCK. WE WILL NOW SEND CONF TO THE
    // BACKUP.
    /*-----------------------------------------------------------------------*/
    signal->theData[0] = reference();
    signal->theData[1] = c_nodeStartSlave.nodeId;
    sendSignal(BACKUP_REF, GSN_INCL_NODEREQ, signal, 2, JBB);
    
    // Suma will not send response to this for now, later...
    sendSignal(SUMA_REF, GSN_INCL_NODEREQ, signal, 2, JBB);
    return;
  }//if
  if (TstartNode_or_blockref == numberToRef(BACKUP, getOwnNodeId())){
    jam();
    signal->theData[0] = c_nodeStartSlave.nodeId;
    signal->theData[1] = cownNodeId;
    sendSignal(cmasterdihref, GSN_INCL_NODECONF, signal, 2, JBB);
    c_nodeStartSlave.nodeId = 0;
    return;
  }
  
  ndbrequire(cmasterdihref = reference());
  receiveLoopMacro(INCL_NODEREQ, TsendNodeId);

  CRASH_INSERTION(7128);
  /*-------------------------------------------------------------------------*/
  // Now that we have included the starting node in the node lists in the
  // various blocks we are ready to start the global checkpoint protocol
  /*------------------------------------------------------------------------*/
  c_nodeStartMaster.wait = 11;
  c_nodeStartMaster.blockGcp = false;

  signal->theData[0] = reference();
  sendSignal(reference(), GSN_UNBLO_DICTCONF, signal, 1, JBB);
}//Dbdih::execINCL_NODECONF()

void Dbdih::execUNBLO_DICTCONF(Signal* signal) 
{
  jamEntry();
  c_nodeStartMaster.wait = ZFALSE;
  if (!c_nodeStartMaster.activeState) {
    jam();
    return;
  }//if

  CRASH_INSERTION(7129);
  /**-----------------------------------------------------------------------
   * WE HAVE NOW PREPARED IT FOR INCLUSION IN THE LCP PROTOCOL. 
   * WE CAN NOW START THE LCP PROTOCOL AGAIN. 
   * WE HAVE ALSO MADE THIS FOR THE GCP PROTOCOL. 
   * WE ARE READY TO START THE PROTOCOLS AND RESPOND TO THE START REQUEST 
   * FROM THE STARTING NODE. 
   *------------------------------------------------------------------------*/
  
  StartMeConf * const startMe = (StartMeConf *)&signal->theData[0];
  
  const Uint32 wordPerSignal = StartMeConf::DATA_SIZE;
  const int noOfSignals = ((Sysfile::SYSFILE_SIZE32 + (wordPerSignal - 1)) /
                           wordPerSignal);
  
  startMe->startingNodeId = c_nodeStartMaster.startNode;
  startMe->startWord = 0;
  
  const Uint32 ref = calcDihBlockRef(c_nodeStartMaster.startNode);
  for(int i = 0; i < noOfSignals; i++){
    jam();
    { // Do copy
      const int startWord = startMe->startWord;
      for(Uint32 j = 0; j < wordPerSignal; j++){
        startMe->data[j] = sysfileData[j+startWord];
      }
    }
    sendSignal(ref, GSN_START_MECONF, signal, StartMeConf::SignalLength, JBB);
    startMe->startWord += wordPerSignal;
  }//for
  c_nodeStartMaster.m_outstandingGsn = GSN_START_MECONF;
}//Dbdih::execUNBLO_DICTCONF()

/*---------------------------------------------------------------------------*/
/*                    NODE RESTART COPY REQUEST                              */
/*---------------------------------------------------------------------------*/
// A NODE RESTART HAS REACHED ITS FINAL PHASE WHEN THE DATA IS TO BE COPIED
// TO THE NODE. START_COPYREQ IS EXECUTED BY THE MASTER NODE.
/*---------------------------------------------------------------------------*/
void Dbdih::execSTART_COPYREQ(Signal* signal) 
{
  jamEntry();
  Uint32 startNodeId = signal->theData[0];
  //BlockReference startingRef = signal->theData[1];
  ndbrequire(c_nodeStartMaster.startNode == startNodeId);
  /*-------------------------------------------------------------------------*/
  // REPORT Copy process of node restart is now about to start up.
  /*-------------------------------------------------------------------------*/
  signal->theData[0] = NDB_LE_NR_CopyFragsStarted;
  signal->theData[1] = startNodeId;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);

  CRASH_INSERTION(7131);
  nodeRestartTakeOver(signal, startNodeId);
  //  BlockReference ref = calcQmgrBlockRef(startNodeId);
  //  signal->theData[0] = cownNodeId;
  // Remove comments as soon as I open up the Qmgr block
  // TODO_RONM
  //  sendSignal(ref, GSN_ALLOW_NODE_CRASHORD, signal, 1, JBB);
}//Dbdih::execSTART_COPYREQ()

/*---------------------------------------------------------------------------*/
/*                    SLAVE LOGIC FOR NODE RESTART                           */
/*---------------------------------------------------------------------------*/
void Dbdih::execSTART_INFOREQ(Signal* signal)
{
  jamEntry();
  StartInfoReq *const req =(StartInfoReq*)&signal->theData[0];
  Uint32 startNode = req->startingNodeId;
  if (cfailurenr != req->systemFailureNo) {
    jam();
    //---------------------------------------------------------------
    // A failure occurred since master sent this request. We will ignore
    // this request since the node is already dead that is starting.
    //---------------------------------------------------------------
    return;
  }//if
  CRASH_INSERTION(7123);
  if (isMaster()) {
    jam();
    ndbrequire(getNodeStatus(startNode) == NodeRecord::STARTING);
  } else {
    jam();
    ndbrequire(getNodeStatus(startNode) == NodeRecord::DEAD);
  }//if
  if ((!getAllowNodeStart(startNode)) ||
      (c_nodeStartSlave.nodeId != 0) ||
      (ERROR_INSERTED(7124))) {
    jam();
    StartInfoRef *const ref =(StartInfoRef*)&signal->theData[0];
    ref->startingNodeId = startNode;
    ref->sendingNodeId = cownNodeId;
    ref->errorCode = ZNODE_START_DISALLOWED_ERROR;
    sendSignal(cmasterdihref, GSN_START_INFOREF, signal, 
	       StartInfoRef::SignalLength, JBB);
    return;
  }//if
  setNodeStatus(startNode, NodeRecord::STARTING);
  if (req->typeStart == NodeState::ST_INITIAL_NODE_RESTART) {
    jam();
    setAllowNodeStart(startNode, false);
    invalidateNodeLCP(signal, startNode, 0);
  } else {
    jam();
    StartInfoConf * c = (StartInfoConf*)&signal->theData[0];
    c->sendingNodeId = cownNodeId;
    c->startingNodeId = startNode;
    sendSignal(cmasterdihref, GSN_START_INFOCONF, signal,
	       StartInfoConf::SignalLength, JBB);
    return;
  }//if
}//Dbdih::execSTART_INFOREQ()

void Dbdih::execINCL_NODEREQ(Signal* signal) 
{
  jamEntry();
  Uint32 retRef = signal->theData[0];
  Uint32 nodeId = signal->theData[1];
  Uint32 tnodeStartFailNr = signal->theData[2];
  currentgcp = signal->theData[4];
  CRASH_INSERTION(7127);
  cnewgcp = currentgcp;
  coldgcp = currentgcp -  1;
  if (!isMaster()) {
    jam();
    /*-----------------------------------------------------------------------*/
    // We don't want to change the state of the master since he can be in the
    // state LCP_TCGET at this time.
    /*-----------------------------------------------------------------------*/
    c_lcpState.setLcpStatus(LCP_STATUS_IDLE, __LINE__);
  }//if

  /*-------------------------------------------------------------------------*/
  // When a node is restarted we must ensure that a lcp will be run
  // as soon as possible and the reset the delay according to the original
  // configuration. 
  // Without an initial local checkpoint the new node will not be available.
  /*-------------------------------------------------------------------------*/
  if (getOwnNodeId() == nodeId) {
    jam();
    /*-----------------------------------------------------------------------*/
    // We are the starting node. We came here only to set the global checkpoint
    // id's and the lcp status.
    /*-----------------------------------------------------------------------*/
    CRASH_INSERTION(7171);
    return;
  }//if
  if (getNodeStatus(nodeId) != NodeRecord::STARTING) {
    jam();
    return;
  }//if
  ndbrequire(cfailurenr == tnodeStartFailNr);
  ndbrequire (c_nodeStartSlave.nodeId == 0);
  c_nodeStartSlave.nodeId = nodeId;
  
  ndbrequire (retRef == cmasterdihref);

  NodeRecordPtr nodePtr;
  nodePtr.i = nodeId;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);

  Sysfile::ActiveStatus TsaveState = nodePtr.p->activeStatus;
  Uint32 TnodeGroup = nodePtr.p->nodeGroup;

  new (nodePtr.p) NodeRecord();
  nodePtr.p->nodeGroup = TnodeGroup;
  nodePtr.p->activeStatus = TsaveState;
  nodePtr.p->nodeStatus = NodeRecord::ALIVE;
  nodePtr.p->useInTransactions = true;
  nodePtr.p->m_inclDihLcp = true;

  removeDeadNode(nodePtr);
  insertAlive(nodePtr);
  con_lineNodes++;

  /*-------------------------------------------------------------------------*/
  //      WE WILL ALSO SEND THE INCLUDE NODE REQUEST TO THE LOCAL LQH BLOCK.
  /*-------------------------------------------------------------------------*/
  signal->theData[0] = reference();
  signal->theData[1] = nodeId;
  signal->theData[2] = currentgcp;
  sendSignal(clocallqhblockref, GSN_INCL_NODEREQ, signal, 3, JBB);
}//Dbdih::execINCL_NODEREQ()

/* ------------------------------------------------------------------------- */
// execINCL_NODECONF() is found in the master logic part since it is used by
// both the master and the slaves.
/* ------------------------------------------------------------------------- */

/*****************************************************************************/
/***********     TAKE OVER DECISION  MODULE                      *************/
/*****************************************************************************/
// This module contains the subroutines that take the decision whether to take
// over a node now or not.
/* ------------------------------------------------------------------------- */
/*                       MASTER LOGIC FOR SYSTEM RESTART                     */
/* ------------------------------------------------------------------------- */
// WE ONLY COME HERE IF WE ARE THE MASTER AND WE ARE PERFORMING A SYSTEM
// RESTART. WE ALSO COME HERE DURING THIS SYSTEM RESTART ONE TIME PER NODE
// THAT NEEDS TAKE OVER.
/*---------------------------------------------------------------------------*/
// WE CHECK IF ANY NODE NEEDS TO BE TAKEN OVER AND THE TAKE OVER HAS NOT YET
// BEEN STARTED OR COMPLETED.
/*---------------------------------------------------------------------------*/
void
Dbdih::systemRestartTakeOverLab(Signal* signal) 
{
  NodeRecordPtr nodePtr;
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    jam();
    ptrAss(nodePtr, nodeRecord);
    switch (nodePtr.p->activeStatus) {
    case Sysfile::NS_Active:
    case Sysfile::NS_ActiveMissed_1:
      jam();
      break;
      /*---------------------------------------------------------------------*/
      // WE HAVE NOT REACHED A STATE YET WHERE THIS NODE NEEDS TO BE TAKEN OVER
      /*---------------------------------------------------------------------*/
    case Sysfile::NS_ActiveMissed_2:
    case Sysfile::NS_NotActive_NotTakenOver:
      jam();
      /*---------------------------------------------------------------------*/
      // THIS NODE IS IN TROUBLE. 
      // WE MUST SUCCEED WITH A LOCAL CHECKPOINT WITH THIS NODE TO REMOVE THE 
      // DANGER. IF THE NODE IS NOT ALIVE THEN THIS WILL NOT BE
      // POSSIBLE AND WE CAN START THE TAKE OVER IMMEDIATELY IF WE HAVE ANY 
      // NODES THAT CAN PERFORM A TAKE OVER.
      /*---------------------------------------------------------------------*/
      if (nodePtr.p->nodeStatus != NodeRecord::ALIVE) {
        jam();
        Uint32 ThotSpareNode = findHotSpare();
        if (ThotSpareNode != RNIL) {
          jam();
          startTakeOver(signal, RNIL, ThotSpareNode, nodePtr.i);
        }//if
      } else if(nodePtr.p->activeStatus == Sysfile::NS_NotActive_NotTakenOver){
        jam();
	/*-------------------------------------------------------------------*/
	// NOT ACTIVE NODES THAT HAVE NOT YET BEEN TAKEN OVER NEEDS TAKE OVER
	// IMMEDIATELY. IF WE ARE ALIVE WE TAKE OVER OUR OWN NODE.
	/*-------------------------------------------------------------------*/
	startTakeOver(signal, RNIL, nodePtr.i, nodePtr.i);
      }//if
      break;
    case Sysfile::NS_TakeOver:
      /**-------------------------------------------------------------------
       * WE MUST HAVE FAILED IN THE MIDDLE OF THE TAKE OVER PROCESS. 
       * WE WILL CONCLUDE THE TAKE OVER PROCESS NOW.
       *-------------------------------------------------------------------*/
      if (nodePtr.p->nodeStatus == NodeRecord::ALIVE) {
        jam();
        Uint32 takeOverNode = Sysfile::getTakeOverNode(nodePtr.i, 
						       SYSFILE->takeOver);
	if(takeOverNode == 0){
	  jam();
	  warningEvent("Bug in take-over code restarting");
	  takeOverNode = nodePtr.i;
	}
        startTakeOver(signal, RNIL, nodePtr.i, takeOverNode);
      } else {
        jam();
	/**-------------------------------------------------------------------
	 * We are not currently taking over, change our active status.
	 *-------------------------------------------------------------------*/
        nodePtr.p->activeStatus = Sysfile::NS_NotActive_NotTakenOver;
        setNodeRestartInfoBits();
      }//if
      break;
    case Sysfile::NS_HotSpare:
      jam();
      break;
      /*---------------------------------------------------------------------*/
      // WE NEED NOT TAKE OVER NODES THAT ARE HOT SPARE.
      /*---------------------------------------------------------------------*/
    case Sysfile::NS_NotDefined:
      jam();
      break;
      /*---------------------------------------------------------------------*/
      // WE NEED NOT TAKE OVER NODES THAT DO NOT EVEN EXIST IN THE CLUSTER.
      /*---------------------------------------------------------------------*/
    default:
      ndbrequire(false);
      break;
    }//switch
  }//for
  /*-------------------------------------------------------------------------*/
  /* NO TAKE OVER HAS BEEN INITIATED.                                        */
  /*-------------------------------------------------------------------------*/
}//Dbdih::systemRestartTakeOverLab()

/*---------------------------------------------------------------------------*/
// This subroutine is called as part of node restart in the master node.
/*---------------------------------------------------------------------------*/
void Dbdih::nodeRestartTakeOver(Signal* signal, Uint32 startNodeId)
{
  switch (getNodeActiveStatus(startNodeId)) {
  case Sysfile::NS_Active:
  case Sysfile::NS_ActiveMissed_1:
  case Sysfile::NS_ActiveMissed_2:
    jam();
    /*-----------------------------------------------------------------------*/
    // AN ACTIVE NODE HAS BEEN STARTED. THE ACTIVE NODE MUST THEN GET ALL DATA
    // IT HAD BEFORE ITS CRASH. WE START THE TAKE OVER IMMEDIATELY. 
    // SINCE WE ARE AN ACTIVE NODE WE WILL TAKE OVER OUR OWN NODE THAT 
    // PREVIOUSLY CRASHED.
    /*-----------------------------------------------------------------------*/
    startTakeOver(signal, RNIL, startNodeId, startNodeId);
    break;
  case Sysfile::NS_HotSpare:{
    jam();
    /*-----------------------------------------------------------------------*/
    // WHEN STARTING UP A HOT SPARE WE WILL CHECK IF ANY NODE NEEDS TO TAKEN 
    // OVER. IF SO THEN WE WILL START THE TAKE OVER.
    /*-----------------------------------------------------------------------*/
      bool takeOverStarted = false;
      NodeRecordPtr nodePtr;
      for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
	jam();
	ptrAss(nodePtr, nodeRecord);
	if (nodePtr.p->activeStatus == Sysfile::NS_NotActive_NotTakenOver) {
	  jam();
	  takeOverStarted = true;
	  startTakeOver(signal, RNIL, startNodeId, nodePtr.i);
	}//if
      }//for
      if (!takeOverStarted) {
	jam();
	/*-------------------------------------------------------------------*/
	// NO TAKE OVER WAS NEEDED AT THE MOMENT WE START-UP AND WAIT UNTIL A 
	// TAKE OVER IS NEEDED.
	/*-------------------------------------------------------------------*/
	BlockReference ref = calcDihBlockRef(startNodeId);
	signal->theData[0] = startNodeId;
	sendSignal(ref, GSN_START_COPYCONF, signal, 1, JBB);
      }//if
      break;
  }
  case Sysfile::NS_NotActive_NotTakenOver:
    jam();
    /*-----------------------------------------------------------------------*/
    // ALL DATA IN THE NODE IS LOST BUT WE HAVE NOT TAKEN OVER YET. WE WILL
    // TAKE OVER OUR OWN NODE
    /*-----------------------------------------------------------------------*/
    startTakeOver(signal, RNIL, startNodeId, startNodeId);
    break;
  case Sysfile::NS_TakeOver:{
    jam();
    /*--------------------------------------------------------------------
     * We were in the process of taking over but it was not completed.
     * We will complete it now instead.
     *--------------------------------------------------------------------*/
    Uint32 takeOverNode = Sysfile::getTakeOverNode(startNodeId, 
						   SYSFILE->takeOver);
    startTakeOver(signal, RNIL, startNodeId, takeOverNode);
    break;
  }
  default:
    ndbrequire(false);
    break;
  }//switch
  nodeResetStart();
}//Dbdih::nodeRestartTakeOver()

/*************************************************************************/
// Ths routine is called when starting a local checkpoint.
/*************************************************************************/
void Dbdih::checkStartTakeOver(Signal* signal) 
{
  NodeRecordPtr csoNodeptr;
  Uint32 tcsoHotSpareNode;
  Uint32 tcsoTakeOverNode;
  if (isMaster()) {
    /*-----------------------------------------------------------------*/
    /*       WE WILL ONLY START TAKE OVER IF WE ARE MASTER.            */
    /*-----------------------------------------------------------------*/
    /*       WE WILL ONLY START THE  TAKE OVER IF THERE WERE A NEED OF */
    /*       A TAKE OVER.                                              */
    /*-----------------------------------------------------------------*/
    /*       WE CAN ONLY PERFORM THE TAKE OVER IF WE HAVE A HOT SPARE  */
    /*       AVAILABLE.                                                */
    /*-----------------------------------------------------------------*/
    tcsoTakeOverNode = 0;
    tcsoHotSpareNode = 0;
    for (csoNodeptr.i = 1; csoNodeptr.i < MAX_NDB_NODES; csoNodeptr.i++) {
      ptrAss(csoNodeptr, nodeRecord);
      if (csoNodeptr.p->activeStatus == Sysfile::NS_NotActive_NotTakenOver) {
        jam();
        tcsoTakeOverNode = csoNodeptr.i;
      } else {
        jam();
        if (csoNodeptr.p->activeStatus == Sysfile::NS_HotSpare) {
          jam();
          tcsoHotSpareNode = csoNodeptr.i;
        }//if
      }//if
    }//for
    if ((tcsoTakeOverNode != 0) &&
        (tcsoHotSpareNode != 0)) {
      jam();
      startTakeOver(signal, RNIL, tcsoHotSpareNode, tcsoTakeOverNode);
    }//if
  }//if
}//Dbdih::checkStartTakeOver()

/*****************************************************************************/
/***********     NODE ADDING  MODULE                             *************/
/***********     CODE TO HANDLE TAKE OVER                        *************/
/*****************************************************************************/
// A take over can be initiated by a number of things:
// 1) A node restart, usually the node takes over itself but can also take
//    over somebody else if its own data was already taken over
// 2) At system restart it is necessary to use the take over code to recover
//    nodes which had too old checkpoints to be restorable by the usual
//    restoration from disk.
// 3) When a node has missed too many local checkpoints and is decided by the
//    master to be taken over by a hot spare node that sits around waiting
//    for this to happen.
//
// To support multiple node failures efficiently the code is written such that
// only one take over can handle transitions in state but during a copy 
// fragment other take over's can perform state transitions.
/*****************************************************************************/
void Dbdih::startTakeOver(Signal* signal,
                          Uint32 takeOverPtrI,
                          Uint32 startNode,
                          Uint32 nodeTakenOver)
{
  NodeRecordPtr toNodePtr;
  NodeGroupRecordPtr NGPtr;
  toNodePtr.i = nodeTakenOver;
  ptrCheckGuard(toNodePtr, MAX_NDB_NODES, nodeRecord);
  NGPtr.i = toNodePtr.p->nodeGroup;
  ptrCheckGuard(NGPtr, MAX_NDB_NODES, nodeGroupRecord);
  TakeOverRecordPtr takeOverPtr;
  if (takeOverPtrI == RNIL) {
    jam();
    setAllowNodeStart(startNode, false);
    seizeTakeOver(takeOverPtr);
    if (startNode == c_nodeStartMaster.startNode) {
      jam();
      takeOverPtr.p->toNodeRestart = true;
    }//if
    takeOverPtr.p->toStartingNode = startNode;
    takeOverPtr.p->toFailedNode = nodeTakenOver;
  } else {
    jam();
    RETURN_IF_TAKE_OVER_INTERRUPTED(takeOverPtrI, takeOverPtr);
    ndbrequire(takeOverPtr.p->toStartingNode == startNode);
    ndbrequire(takeOverPtr.p->toFailedNode == nodeTakenOver);
    ndbrequire(takeOverPtr.p->toMasterStatus == TakeOverRecord::TO_WAIT_START_TAKE_OVER);
  }//if
  if ((NGPtr.p->activeTakeOver) || (ERROR_INSERTED(7157))) {
    jam();
    /**------------------------------------------------------------------------
     * A take over is already active in this node group. We only allow one 
     * take over per node group. Otherwise we will overload the node group and 
     * also we will require much more checks when starting up copying of 
     * fragments. The parallelism for take over is mainly to ensure that we 
     * can handle take over efficiently in large systems with 4 nodes and above
     * A typical case is a 8 node system executing on two 8-cpu boxes. 
     * A box crash in one of the boxes will mean 4 nodes crashes. 
     * We want to be able to restart those four nodes to some 
     * extent in parallel.
     * 
     * We will wait for a few seconds and then try again.
     */
    takeOverPtr.p->toMasterStatus = TakeOverRecord::TO_WAIT_START_TAKE_OVER;
    signal->theData[0] = DihContinueB::ZSTART_TAKE_OVER;
    signal->theData[1] = takeOverPtr.i;
    signal->theData[2] = startNode;
    signal->theData[3] = nodeTakenOver;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 5000, 4);
    return;
  }//if
  NGPtr.p->activeTakeOver = true;
  if (startNode == nodeTakenOver) {
    jam();
    switch (getNodeActiveStatus(nodeTakenOver)) {
    case Sysfile::NS_Active:
    case Sysfile::NS_ActiveMissed_1:
    case Sysfile::NS_ActiveMissed_2:
      jam();
      break;
    case Sysfile::NS_NotActive_NotTakenOver:
    case Sysfile::NS_TakeOver:
      jam();
      setNodeActiveStatus(nodeTakenOver, Sysfile::NS_TakeOver);
      break;
    default:
      ndbrequire(false);
    }//switch
  } else {
    jam();
    setNodeActiveStatus(nodeTakenOver, Sysfile::NS_HotSpare);
    setNodeActiveStatus(startNode, Sysfile::NS_TakeOver);
    changeNodeGroups(startNode, nodeTakenOver);
  }//if
  setNodeRestartInfoBits();
  /* ---------------------------------------------------------------------- */
  /*  WE SET THE RESTART INFORMATION TO INDICATE THAT WE ARE ABOUT TO TAKE  */
  /*  OVER THE FAILED NODE. WE SET THIS INFORMATION AND WAIT UNTIL THE      */
  /*  GLOBAL CHECKPOINT HAS WRITTEN THE RESTART INFORMATION.                */
  /* ---------------------------------------------------------------------- */
  Sysfile::setTakeOverNode(takeOverPtr.p->toFailedNode, SYSFILE->takeOver,
			   startNode);
  takeOverPtr.p->toMasterStatus = TakeOverRecord::TO_START_COPY;
  
  cstartGcpNow = true;
}//Dbdih::startTakeOver()

void Dbdih::changeNodeGroups(Uint32 startNode, Uint32 nodeTakenOver)
{
  NodeRecordPtr startNodePtr;
  NodeRecordPtr toNodePtr;
  startNodePtr.i = startNode;
  ptrCheckGuard(startNodePtr, MAX_NDB_NODES, nodeRecord);
  toNodePtr.i = nodeTakenOver;
  ptrCheckGuard(toNodePtr, MAX_NDB_NODES, nodeRecord);
  ndbrequire(startNodePtr.p->nodeGroup == ZNIL);
  NodeGroupRecordPtr NGPtr;

  NGPtr.i = toNodePtr.p->nodeGroup;
  ptrCheckGuard(NGPtr, MAX_NDB_NODES, nodeGroupRecord);
  bool nodeFound = false;
  for (Uint32 i = 0; i < NGPtr.p->nodeCount; i++) {
    jam();
    if (NGPtr.p->nodesInGroup[i] == nodeTakenOver) {
      jam();
      NGPtr.p->nodesInGroup[i] = startNode;
      nodeFound = true;
    }//if
  }//for
  ndbrequire(nodeFound);
  Sysfile::setNodeGroup(startNodePtr.i, SYSFILE->nodeGroups, toNodePtr.p->nodeGroup);
  startNodePtr.p->nodeGroup = toNodePtr.p->nodeGroup;
  Sysfile::setNodeGroup(toNodePtr.i, SYSFILE->nodeGroups, NO_NODE_GROUP_ID);
  toNodePtr.p->nodeGroup = ZNIL;
}//Dbdih::changeNodeGroups()

void Dbdih::checkToCopy()
{
  TakeOverRecordPtr takeOverPtr;
  for (takeOverPtr.i = 0;takeOverPtr.i < MAX_NDB_NODES; takeOverPtr.i++) {
    ptrAss(takeOverPtr, takeOverRecord);
    /*----------------------------------------------------------------------*/
    // TAKE OVER HANDLING WRITES RESTART INFORMATION THROUGH 
    // THE GLOBAL CHECKPOINT
    // PROTOCOL. WE CHECK HERE BEFORE STARTING A WRITE OF THE RESTART 
    // INFORMATION.
    /*-----------------------------------------------------------------------*/
    if (takeOverPtr.p->toMasterStatus == TakeOverRecord::TO_START_COPY) {
      jam();
      takeOverPtr.p->toMasterStatus = TakeOverRecord::TO_START_COPY_ONGOING;
    } else if (takeOverPtr.p->toMasterStatus == TakeOverRecord::TO_END_COPY) {
      jam();
      takeOverPtr.p->toMasterStatus = TakeOverRecord::TO_END_COPY_ONGOING;
    }//if
  }//for
}//Dbdih::checkToCopy()

void Dbdih::checkToCopyCompleted(Signal* signal)
{
  /* ------------------------------------------------------------------------*/
  /*     WE CHECK HERE IF THE WRITING OF TAKE OVER INFORMATION ALSO HAS BEEN */
  /*     COMPLETED.                                                          */
  /* ------------------------------------------------------------------------*/
  TakeOverRecordPtr toPtr;
  for (toPtr.i = 0; toPtr.i < MAX_NDB_NODES; toPtr.i++) {
    ptrAss(toPtr, takeOverRecord);
    if (toPtr.p->toMasterStatus == TakeOverRecord::TO_START_COPY_ONGOING){
      jam();
      sendStartTo(signal, toPtr.i);
    } else if (toPtr.p->toMasterStatus == TakeOverRecord::TO_END_COPY_ONGOING){
      jam();
      sendEndTo(signal, toPtr.i);
    } else {
      jam();
    }//if
  }//for
}//Dbdih::checkToCopyCompleted()

bool Dbdih::checkToInterrupted(TakeOverRecordPtr& takeOverPtr)
{
  if (checkNodeAlive(takeOverPtr.p->toStartingNode)) {
    jam();
    return false;
  } else {
    jam();
    endTakeOver(takeOverPtr.i);
    return true;
  }//if
}//Dbdih::checkToInterrupted()

void Dbdih::sendStartTo(Signal* signal, Uint32 takeOverPtrI)
{
  TakeOverRecordPtr takeOverPtr;
  CRASH_INSERTION(7155);
  RETURN_IF_TAKE_OVER_INTERRUPTED(takeOverPtrI, takeOverPtr);
  if ((c_startToLock != RNIL) || (ERROR_INSERTED(7158))) {
    jam();
    takeOverPtr.p->toMasterStatus = TakeOverRecord::TO_WAIT_START;
    signal->theData[0] = DihContinueB::ZSEND_START_TO;
    signal->theData[1] = takeOverPtrI;
    signal->theData[2] = takeOverPtr.p->toStartingNode;
    signal->theData[3] = takeOverPtr.p->toFailedNode;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 30, 4);
    return;
  }//if
  c_startToLock = takeOverPtrI;
  StartToReq * const req = (StartToReq *)&signal->theData[0];
  req->userPtr = takeOverPtr.i;
  req->userRef = reference();
  req->startingNodeId = takeOverPtr.p->toStartingNode;
  req->nodeTakenOver = takeOverPtr.p->toFailedNode;
  req->nodeRestart = takeOverPtr.p->toNodeRestart;
  takeOverPtr.p->toMasterStatus = TakeOverRecord::STARTING;
  sendLoopMacro(START_TOREQ, sendSTART_TOREQ);
}//Dbdih::sendStartTo()

void Dbdih::execSTART_TOREQ(Signal* signal) 
{
  TakeOverRecordPtr takeOverPtr;
  jamEntry();
  const StartToReq * const req = (StartToReq *)&signal->theData[0];
  takeOverPtr.i = req->userPtr;
  BlockReference ref = req->userRef;
  Uint32 startingNode = req->startingNodeId;

  CRASH_INSERTION(7133);
  RETURN_IF_NODE_NOT_ALIVE(req->startingNodeId);
  ptrCheckGuard(takeOverPtr, MAX_NDB_NODES, takeOverRecord);
  allocateTakeOver(takeOverPtr);
  initStartTakeOver(req, takeOverPtr);
  
  StartToConf * const conf = (StartToConf *)&signal->theData[0];
  conf->userPtr = takeOverPtr.i;
  conf->sendingNodeId = cownNodeId;
  conf->startingNodeId = startingNode;
  sendSignal(ref, GSN_START_TOCONF, signal, StartToConf::SignalLength, JBB);
}//Dbdih::execSTART_TOREQ()

void Dbdih::execSTART_TOCONF(Signal* signal) 
{
  TakeOverRecordPtr takeOverPtr;
  jamEntry();
  const StartToConf * const conf = (StartToConf *)&signal->theData[0];

  CRASH_INSERTION(7147);

  RETURN_IF_NODE_NOT_ALIVE(conf->startingNodeId);
  
  takeOverPtr.i = conf->userPtr;
  ptrCheckGuard(takeOverPtr, MAX_NDB_NODES, takeOverRecord);
  ndbrequire(takeOverPtr.p->toMasterStatus == TakeOverRecord::STARTING);
  ndbrequire(takeOverPtr.p->toStartingNode == conf->startingNodeId);
  receiveLoopMacro(START_TOREQ, conf->sendingNodeId);
  CRASH_INSERTION(7134);
  c_startToLock = RNIL;

  startNextCopyFragment(signal, takeOverPtr.i);
}//Dbdih::execSTART_TOCONF()

void Dbdih::initStartTakeOver(const StartToReq * req, 
			      TakeOverRecordPtr takeOverPtr)
{
  takeOverPtr.p->toCurrentTabref = 0;
  takeOverPtr.p->toCurrentFragid = 0;
  takeOverPtr.p->toStartingNode = req->startingNodeId;
  takeOverPtr.p->toFailedNode = req->nodeTakenOver;
  takeOverPtr.p->toSlaveStatus = TakeOverRecord::TO_SLAVE_STARTED;
  takeOverPtr.p->toCopyNode = RNIL;
  takeOverPtr.p->toCurrentReplica = RNIL;
  takeOverPtr.p->toNodeRestart = req->nodeRestart;
}//Dbdih::initStartTakeOver()

void Dbdih::startNextCopyFragment(Signal* signal, Uint32 takeOverPtrI)
{
  TabRecordPtr tabPtr;
  TakeOverRecordPtr takeOverPtr;
  Uint32 loopCount;
  RETURN_IF_TAKE_OVER_INTERRUPTED(takeOverPtrI, takeOverPtr);
  takeOverPtr.p->toMasterStatus = TakeOverRecord::SELECTING_NEXT;
  loopCount = 0;
  if (ERROR_INSERTED(7159)) {
    loopCount = 100;
  }//if
  while (loopCount++ < 100) {
    tabPtr.i = takeOverPtr.p->toCurrentTabref;
    if (tabPtr.i >= ctabFileSize) {
      jam();
      CRASH_INSERTION(7136);
      sendUpdateTo(signal, takeOverPtr.i, UpdateToReq::TO_COPY_COMPLETED);
      return;
    }//if
    ptrAss(tabPtr, tabRecord);
    if (tabPtr.p->tabStatus != TabRecord::TS_ACTIVE){
      jam();
      takeOverPtr.p->toCurrentFragid = 0;
      takeOverPtr.p->toCurrentTabref++;
      continue;
    }//if
    Uint32 fragId = takeOverPtr.p->toCurrentFragid;
    if (fragId >= tabPtr.p->totalfragments) {
      jam();
      takeOverPtr.p->toCurrentFragid = 0;
      takeOverPtr.p->toCurrentTabref++;
      if (ERROR_INSERTED(7135)) {
        if (takeOverPtr.p->toCurrentTabref == 1) {
          ndbrequire(false);
        }//if
      }//if
      continue;
    }//if
    FragmentstorePtr fragPtr;
    getFragstore(tabPtr.p, fragId, fragPtr);
    ReplicaRecordPtr loopReplicaPtr;
    loopReplicaPtr.i = fragPtr.p->oldStoredReplicas;
    while (loopReplicaPtr.i != RNIL) {
      ptrCheckGuard(loopReplicaPtr, creplicaFileSize, replicaRecord);
      if (loopReplicaPtr.p->procNode == takeOverPtr.p->toFailedNode) {
        jam();
	/* ----------------------------------------------------------------- */
	/* WE HAVE FOUND A REPLICA THAT BELONGED THE FAILED NODE THAT NEEDS  */
	/* TAKE OVER. WE TAKE OVER THIS REPLICA TO THE NEW NODE.             */
	/* ----------------------------------------------------------------- */
        takeOverPtr.p->toCurrentReplica = loopReplicaPtr.i;
        toCopyFragLab(signal, takeOverPtr.i);
        return;
      } else if (loopReplicaPtr.p->procNode == takeOverPtr.p->toStartingNode) {
        jam();
	/* ----------------------------------------------------------------- */
	/* WE HAVE OBVIOUSLY STARTED TAKING OVER THIS WITHOUT COMPLETING IT. */
	/* WE     */
	/* NEED TO COMPLETE THE TAKE OVER OF THIS REPLICA.                   */
	/* ----------------------------------------------------------------- */
        takeOverPtr.p->toCurrentReplica = loopReplicaPtr.i;
        toCopyFragLab(signal, takeOverPtr.i);
        return;
      } else {
        jam();
        loopReplicaPtr.i = loopReplicaPtr.p->nextReplica;
      }//if
    }//while
    takeOverPtr.p->toCurrentFragid++;
  }//while
  signal->theData[0] = DihContinueB::ZTO_START_COPY_FRAG;
  signal->theData[1] = takeOverPtr.i;
  sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
}//Dbdih::startNextCopyFragment()

void Dbdih::toCopyFragLab(Signal* signal,
                          Uint32 takeOverPtrI) 
{
  TakeOverRecordPtr takeOverPtr;
  RETURN_IF_TAKE_OVER_INTERRUPTED(takeOverPtrI, takeOverPtr);

  CreateReplicaRecordPtr createReplicaPtr;
  createReplicaPtr.i = 0;
  ptrAss(createReplicaPtr, createReplicaRecord);

  ReplicaRecordPtr replicaPtr;
  replicaPtr.i = takeOverPtr.p->toCurrentReplica;
  ptrCheckGuard(replicaPtr, creplicaFileSize, replicaRecord);

  TabRecordPtr tabPtr;
  tabPtr.i = takeOverPtr.p->toCurrentTabref;
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
  /* ----------------------------------------------------------------------- */
  /* WE HAVE FOUND A REPLICA THAT NEEDS TAKE OVER. WE WILL START THIS TAKE   */
  /* OVER BY ADDING THE FRAGMENT WHEREAFTER WE WILL ORDER THE PRIMARY        */
  /* REPLICA TO COPY ITS CONTENT TO THE NEW STARTING REPLICA.                */
  /* THIS OPERATION IS A SINGLE USER OPERATION UNTIL WE HAVE SENT            */
  /* COPY_FRAGREQ. AFTER SENDING COPY_FRAGREQ WE ARE READY TO START A NEW    */
  /* FRAGMENT REPLICA. WE WILL NOT IMPLEMENT THIS IN THE FIRST PHASE.        */
  /* ----------------------------------------------------------------------- */
  cnoOfCreateReplicas = 1;
  createReplicaPtr.p->hotSpareUse = true;
  createReplicaPtr.p->dataNodeId = takeOverPtr.p->toStartingNode;

  prepareSendCreateFragReq(signal, takeOverPtrI);
}//Dbdih::toCopyFragLab()

void Dbdih::prepareSendCreateFragReq(Signal* signal, Uint32 takeOverPtrI)
{
  TakeOverRecordPtr takeOverPtr;
  RETURN_IF_TAKE_OVER_INTERRUPTED(takeOverPtrI, takeOverPtr);

  TabRecordPtr tabPtr;
  tabPtr.i = takeOverPtr.p->toCurrentTabref;
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
  FragmentstorePtr fragPtr;

  getFragstore(tabPtr.p, takeOverPtr.p->toCurrentFragid, fragPtr);
  Uint32 nodes[MAX_REPLICAS];
  extractNodeInfo(fragPtr.p, nodes);
  takeOverPtr.p->toCopyNode = nodes[0];
  sendCreateFragReq(signal, 0, CreateFragReq::STORED, takeOverPtr.i);
}//Dbdih::prepareSendCreateFragReq()

void Dbdih::sendCreateFragReq(Signal* signal,
                              Uint32 startGci,
                              Uint32 replicaType,
                              Uint32 takeOverPtrI) 
{
  TakeOverRecordPtr takeOverPtr;
  RETURN_IF_TAKE_OVER_INTERRUPTED(takeOverPtrI, takeOverPtr);
  if ((c_createFragmentLock != RNIL) ||
      ((ERROR_INSERTED(7161))&&(replicaType == CreateFragReq::STORED)) ||
      ((ERROR_INSERTED(7162))&&(replicaType == CreateFragReq::COMMIT_STORED))){
    if (replicaType == CreateFragReq::STORED) {
      jam();
      takeOverPtr.p->toMasterStatus = TakeOverRecord::TO_WAIT_PREPARE_CREATE;
    } else {
      ndbrequire(replicaType == CreateFragReq::COMMIT_STORED);
      jam();
      takeOverPtr.p->toMasterStatus = TakeOverRecord::TO_WAIT_COMMIT_CREATE;
    }//if
    signal->theData[0] = DihContinueB::ZSEND_CREATE_FRAG;
    signal->theData[1] = takeOverPtr.i;
    signal->theData[2] = replicaType;
    signal->theData[3] = startGci;
    signal->theData[4] = takeOverPtr.p->toStartingNode;
    signal->theData[5] = takeOverPtr.p->toFailedNode;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 50, 6);
    return;
  }//if
  c_createFragmentLock = takeOverPtr.i;
  sendLoopMacro(CREATE_FRAGREQ, nullRoutine);

  CreateFragReq * const req = (CreateFragReq *)&signal->theData[0];
  req->userPtr = takeOverPtr.i;
  req->userRef = reference();
  req->tableId = takeOverPtr.p->toCurrentTabref;
  req->fragId = takeOverPtr.p->toCurrentFragid;
  req->startingNodeId = takeOverPtr.p->toStartingNode;
  req->copyNodeId = takeOverPtr.p->toCopyNode;
  req->startGci = startGci;
  req->replicaType = replicaType;

  NodeRecordPtr nodePtr;
  nodePtr.i = cfirstAliveNode;
  do {
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
    BlockReference ref = calcDihBlockRef(nodePtr.i);
    sendSignal(ref, GSN_CREATE_FRAGREQ, signal, 
	       CreateFragReq::SignalLength, JBB);
    nodePtr.i = nodePtr.p->nextNode;
  } while (nodePtr.i != RNIL);

  if (replicaType == CreateFragReq::STORED) {
    jam();
    takeOverPtr.p->toMasterStatus = TakeOverRecord::PREPARE_CREATE;
  } else {
    ndbrequire(replicaType == CreateFragReq::COMMIT_STORED);
    jam();
    takeOverPtr.p->toMasterStatus = TakeOverRecord::COMMIT_CREATE;
  }
}//Dbdih::sendCreateFragReq()

/* --------------------------------------------------------------------------*/
/*       AN ORDER TO START OR COMMIT THE REPLICA CREATION ARRIVED FROM THE   */
/*       MASTER.                                                             */
/* --------------------------------------------------------------------------*/
void Dbdih::execCREATE_FRAGREQ(Signal* signal) 
{
  jamEntry();
  CreateFragReq * const req = (CreateFragReq *)&signal->theData[0];

  TakeOverRecordPtr takeOverPtr;
  takeOverPtr.i = req->userPtr;
  ptrCheckGuard(takeOverPtr, MAX_NDB_NODES, takeOverRecord);

  BlockReference retRef = req->userRef;

  TabRecordPtr tabPtr;
  tabPtr.i = req->tableId;
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);

  Uint32 fragId = req->fragId;
  Uint32 tdestNodeid = req->startingNodeId;
  Uint32 tsourceNodeid = req->copyNodeId;
  Uint32 startGci = req->startGci;
  Uint32 replicaType = req->replicaType;

  FragmentstorePtr fragPtr;
  getFragstore(tabPtr.p, fragId, fragPtr);
  RETURN_IF_NODE_NOT_ALIVE(tdestNodeid);
  ReplicaRecordPtr frReplicaPtr;
  findToReplica(takeOverPtr.p, replicaType, fragPtr, frReplicaPtr);
  ndbrequire(frReplicaPtr.i != RNIL);

  switch (replicaType) {
  case CreateFragReq::STORED:
    jam();
    CRASH_INSERTION(7138);
    /* ----------------------------------------------------------------------*/
    /*  HERE WE ARE INSERTING THE NEW BACKUP NODE IN THE EXECUTION OF ALL    */
    /*  OPERATIONS. FROM HERE ON ALL OPERATIONS ON THIS FRAGMENT WILL INCLUDE*/
    /*  USE OF THE NEW REPLICA.                                              */
    /* --------------------------------------------------------------------- */
    insertBackup(fragPtr, tdestNodeid);
    takeOverPtr.p->toCopyNode = tsourceNodeid;
    takeOverPtr.p->toSlaveStatus = TakeOverRecord::TO_SLAVE_CREATE_PREPARE;
    
    fragPtr.p->distributionKey++;
    fragPtr.p->distributionKey &= 255;
    break;
  case CreateFragReq::COMMIT_STORED:
    jam();
    CRASH_INSERTION(7139);
    /* ----------------------------------------------------------------------*/
    /*  HERE WE ARE MOVING THE REPLICA TO THE STORED SECTION SINCE IT IS NOW */
    /*  FULLY LOADED WITH ALL DATA NEEDED.                                   */
    // We also update the order of the replicas here so that if the new 
    // replica is the desired primary we insert it as primary.
    /* ----------------------------------------------------------------------*/
    takeOverPtr.p->toSlaveStatus = TakeOverRecord::TO_SLAVE_CREATE_COMMIT;
    removeOldStoredReplica(fragPtr, frReplicaPtr);
    linkStoredReplica(fragPtr, frReplicaPtr);
    updateNodeInfo(fragPtr);
    break;
  default:
    ndbrequire(false);
    break;
  }//switch

  /* ------------------------------------------------------------------------*/
  /*       THE NEW NODE OF THIS REPLICA IS THE STARTING NODE.                */
  /* ------------------------------------------------------------------------*/
  if (frReplicaPtr.p->procNode != takeOverPtr.p->toStartingNode) {
    jam();
    /* ---------------------------------------------------------------------*/
    /*  IF WE ARE STARTING A TAKE OVER NODE WE MUST INVALIDATE ALL LCP'S.   */
    /*  OTHERWISE WE WILL TRY TO START LCP'S THAT DO NOT EXIST.             */
    /* ---------------------------------------------------------------------*/
    frReplicaPtr.p->procNode = takeOverPtr.p->toStartingNode;
    frReplicaPtr.p->noCrashedReplicas = 0;
    frReplicaPtr.p->createGci[0] = startGci;
    ndbrequire(startGci != 0xF1F1F1F1);
    frReplicaPtr.p->replicaLastGci[0] = (Uint32)-1;
    for (Uint32 i = 0; i < MAX_LCP_STORED; i++) {
      frReplicaPtr.p->lcpStatus[i] = ZINVALID;
    }//for
  } else {
    jam();
    const Uint32 noCrashed = frReplicaPtr.p->noCrashedReplicas;
    arrGuard(noCrashed, 8);
    frReplicaPtr.p->createGci[noCrashed] = startGci;
    ndbrequire(startGci != 0xF1F1F1F1);
    frReplicaPtr.p->replicaLastGci[noCrashed] = (Uint32)-1;
  }//if
  takeOverPtr.p->toCurrentTabref = tabPtr.i;
  takeOverPtr.p->toCurrentFragid = fragId;
  CreateFragConf * const conf = (CreateFragConf *)&signal->theData[0];
  conf->userPtr = takeOverPtr.i;
  conf->tableId = tabPtr.i;
  conf->fragId = fragId;
  conf->sendingNodeId = cownNodeId;
  conf->startingNodeId = tdestNodeid;
  sendSignal(retRef, GSN_CREATE_FRAGCONF, signal,
             CreateFragConf::SignalLength, JBB);
}//Dbdih::execCREATE_FRAGREQ()

void Dbdih::execCREATE_FRAGCONF(Signal* signal) 
{
  jamEntry();
  CRASH_INSERTION(7148);
  const CreateFragConf * const conf = (CreateFragConf *)&signal->theData[0];
  Uint32 fragId = conf->fragId;

  RETURN_IF_NODE_NOT_ALIVE(conf->startingNodeId);

  TabRecordPtr tabPtr;
  tabPtr.i = conf->tableId;
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);

  TakeOverRecordPtr takeOverPtr;
  takeOverPtr.i = conf->userPtr;
  ptrCheckGuard(takeOverPtr, MAX_NDB_NODES, takeOverRecord);

  ndbrequire(tabPtr.i == takeOverPtr.p->toCurrentTabref);
  ndbrequire(fragId == takeOverPtr.p->toCurrentFragid);
  receiveLoopMacro(CREATE_FRAGREQ, conf->sendingNodeId);
  c_createFragmentLock = RNIL;

  if (takeOverPtr.p->toMasterStatus == TakeOverRecord::PREPARE_CREATE) {
    jam();
    CRASH_INSERTION(7140);
    /* --------------------------------------------------------------------- */
    /*   ALL NODES HAVE PREPARED THE INTRODUCTION OF THIS NEW NODE AND IT IS */
    /*   ALREADY IN USE. WE CAN NOW START COPYING THE FRAGMENT.              */
    /*---------------------------------------------------------------------- */
    FragmentstorePtr fragPtr;
    getFragstore(tabPtr.p, fragId, fragPtr);
    takeOverPtr.p->toMasterStatus = TakeOverRecord::COPY_FRAG;
    BlockReference ref = calcLqhBlockRef(takeOverPtr.p->toCopyNode);
    CopyFragReq * const copyFragReq = (CopyFragReq *)&signal->theData[0];
    copyFragReq->userPtr = takeOverPtr.i;
    copyFragReq->userRef = reference();
    copyFragReq->tableId = tabPtr.i;
    copyFragReq->fragId = fragId;
    copyFragReq->nodeId = takeOverPtr.p->toStartingNode;
    copyFragReq->schemaVersion = tabPtr.p->schemaVersion;
    copyFragReq->distributionKey = fragPtr.p->distributionKey;
    sendSignal(ref, GSN_COPY_FRAGREQ, signal, CopyFragReq::SignalLength, JBB);
  } else {
    ndbrequire(takeOverPtr.p->toMasterStatus == TakeOverRecord::COMMIT_CREATE);
    jam();
    CRASH_INSERTION(7141);
    /* --------------------------------------------------------------------- */
    // REPORT that copy of fragment has been completed.
    /* --------------------------------------------------------------------- */
    signal->theData[0] = NDB_LE_NR_CopyFragDone;
    signal->theData[1] = takeOverPtr.p->toStartingNode;
    signal->theData[2] = tabPtr.i;
    signal->theData[3] = takeOverPtr.p->toCurrentFragid;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 4, JBB);
    /* --------------------------------------------------------------------- */
    /*   WE HAVE NOW CREATED THIS NEW REPLICA AND WE ARE READY TO TAKE THE   */
    /*   THE NEXT REPLICA.                                                   */
    /* --------------------------------------------------------------------- */

    Mutex mutex(signal, c_mutexMgr, takeOverPtr.p->m_switchPrimaryMutexHandle);
    mutex.unlock(); // ignore result

    takeOverPtr.p->toCurrentFragid++;
    startNextCopyFragment(signal, takeOverPtr.i);
  }//if
}//Dbdih::execCREATE_FRAGCONF()

void Dbdih::execCOPY_FRAGREF(Signal* signal) 
{
  const CopyFragRef * const ref = (CopyFragRef *)&signal->theData[0];
  jamEntry();
  Uint32 takeOverPtrI = ref->userPtr;
  Uint32 startingNodeId = ref->startingNodeId;
  Uint32 errorCode = ref->errorCode;

  TakeOverRecordPtr takeOverPtr;
  RETURN_IF_TAKE_OVER_INTERRUPTED(takeOverPtrI, takeOverPtr);
  ndbrequire(errorCode != ZNODE_FAILURE_ERROR);
  ndbrequire(ref->tableId == takeOverPtr.p->toCurrentTabref);
  ndbrequire(ref->fragId == takeOverPtr.p->toCurrentFragid);
  ndbrequire(ref->startingNodeId == takeOverPtr.p->toStartingNode);
  ndbrequire(ref->sendingNodeId == takeOverPtr.p->toCopyNode);
  ndbrequire(takeOverPtr.p->toMasterStatus == TakeOverRecord::COPY_FRAG);
  endTakeOver(takeOverPtrI);
  //--------------------------------------------------------------------------
  // For some reason we did not succeed in copying a fragment. We treat this
  // as a serious failure and crash the starting node.
  //--------------------------------------------------------------------------
  BlockReference cntrRef = calcNdbCntrBlockRef(startingNodeId);
  SystemError * const sysErr = (SystemError*)&signal->theData[0];
  sysErr->errorCode = SystemError::CopyFragRefError;
  sysErr->errorRef = reference();
  sysErr->data1 = errorCode;
  sysErr->data2 = 0;
  sendSignal(cntrRef, GSN_SYSTEM_ERROR, signal, 
	     SystemError::SignalLength, JBB);
  return;
}//Dbdih::execCOPY_FRAGREF()

void Dbdih::execCOPY_FRAGCONF(Signal* signal) 
{
  const CopyFragConf * const conf = (CopyFragConf *)&signal->theData[0];
  jamEntry();
  CRASH_INSERTION(7142);

  TakeOverRecordPtr takeOverPtr;
  Uint32 takeOverPtrI = conf->userPtr;
  RETURN_IF_TAKE_OVER_INTERRUPTED(takeOverPtrI, takeOverPtr);

  ndbrequire(conf->tableId == takeOverPtr.p->toCurrentTabref);
  ndbrequire(conf->fragId == takeOverPtr.p->toCurrentFragid);
  ndbrequire(conf->startingNodeId == takeOverPtr.p->toStartingNode);
  ndbrequire(conf->sendingNodeId == takeOverPtr.p->toCopyNode);
  ndbrequire(takeOverPtr.p->toMasterStatus == TakeOverRecord::COPY_FRAG);
  sendUpdateTo(signal, takeOverPtr.i, 
	       (Uint32)UpdateToReq::TO_COPY_FRAG_COMPLETED);
}//Dbdih::execCOPY_FRAGCONF()

void Dbdih::sendUpdateTo(Signal* signal, 
			 Uint32 takeOverPtrI, Uint32 updateState)
{
  TakeOverRecordPtr takeOverPtr;
  RETURN_IF_TAKE_OVER_INTERRUPTED(takeOverPtrI, takeOverPtr);
  if ((c_updateToLock != RNIL) || 
      ((ERROR_INSERTED(7163)) && 
       (updateState == UpdateToReq::TO_COPY_FRAG_COMPLETED)) ||
      ((ERROR_INSERTED(7169)) && 
       (updateState == UpdateToReq::TO_COPY_COMPLETED))) {
    jam();
    takeOverPtr.p->toMasterStatus = TakeOverRecord::TO_WAIT_UPDATE_TO;
    signal->theData[0] = DihContinueB::ZSEND_UPDATE_TO;
    signal->theData[1] = takeOverPtrI;
    signal->theData[2] = takeOverPtr.p->toStartingNode;
    signal->theData[3] = takeOverPtr.p->toFailedNode;
    signal->theData[4] = updateState;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 30, 5);
    return;
  }//if
  c_updateToLock = takeOverPtrI;
  if (updateState == UpdateToReq::TO_COPY_FRAG_COMPLETED) {
    jam();
    takeOverPtr.p->toMasterStatus = TakeOverRecord::TO_UPDATE_TO;
  } else {
    jam();
    ndbrequire(updateState == UpdateToReq::TO_COPY_COMPLETED);
    takeOverPtr.p->toMasterStatus = TakeOverRecord::TO_COPY_COMPLETED;
  }//if

  UpdateToReq * const req = (UpdateToReq *)&signal->theData[0];
  req->userPtr = takeOverPtr.i;
  req->userRef = reference();
  req->updateState = (UpdateToReq::UpdateState)updateState;
  req->startingNodeId = takeOverPtr.p->toStartingNode;
  req->tableId = takeOverPtr.p->toCurrentTabref;
  req->fragmentNo = takeOverPtr.p->toCurrentFragid;
  sendLoopMacro(UPDATE_TOREQ, sendUPDATE_TOREQ);
}//Dbdih::sendUpdateTo()

void Dbdih::execUPDATE_TOREQ(Signal* signal)
{
  jamEntry();
  const UpdateToReq * const req = (UpdateToReq *)&signal->theData[0];
  BlockReference ref = req->userRef;
  ndbrequire(cmasterdihref == ref);

  CRASH_INSERTION(7154);
  RETURN_IF_NODE_NOT_ALIVE(req->startingNodeId);

  TakeOverRecordPtr takeOverPtr;
  takeOverPtr.i = req->userPtr;
  ptrCheckGuard(takeOverPtr, MAX_NDB_NODES, takeOverRecord);

  ndbrequire(req->startingNodeId == takeOverPtr.p->toStartingNode);
  if (req->updateState == UpdateToReq::TO_COPY_FRAG_COMPLETED) {
    jam();
    ndbrequire(takeOverPtr.p->toSlaveStatus == TakeOverRecord::TO_SLAVE_CREATE_PREPARE);
    takeOverPtr.p->toSlaveStatus = TakeOverRecord::TO_SLAVE_COPY_FRAG_COMPLETED;
    takeOverPtr.p->toCurrentTabref = req->tableId;
    takeOverPtr.p->toCurrentFragid = req->fragmentNo;
  } else {
    jam();
    ndbrequire(req->updateState == UpdateToReq::TO_COPY_COMPLETED);
    takeOverPtr.p->toSlaveStatus = TakeOverRecord::TO_SLAVE_COPY_COMPLETED;
    setNodeCopyCompleted(takeOverPtr.p->toStartingNode, true);
  }//if


  UpdateToConf * const conf = (UpdateToConf *)&signal->theData[0];
  conf->userPtr = takeOverPtr.i;
  conf->sendingNodeId = cownNodeId;
  conf->startingNodeId = takeOverPtr.p->toStartingNode;
  sendSignal(ref, GSN_UPDATE_TOCONF, signal, UpdateToConf::SignalLength, JBB);
}//Dbdih::execUPDATE_TOREQ()

void Dbdih::execUPDATE_TOCONF(Signal* signal)
{
  const UpdateToConf * const conf = (UpdateToConf *)&signal->theData[0];
  CRASH_INSERTION(7152);

  RETURN_IF_NODE_NOT_ALIVE(conf->startingNodeId);

  TakeOverRecordPtr takeOverPtr;
  takeOverPtr.i = conf->userPtr;
  ptrCheckGuard(takeOverPtr, MAX_NDB_NODES, takeOverRecord);

  receiveLoopMacro(UPDATE_TOREQ, conf->sendingNodeId);
  CRASH_INSERTION(7153);
  c_updateToLock = RNIL;

  if (takeOverPtr.p->toMasterStatus == TakeOverRecord::TO_COPY_COMPLETED) {
    jam();
    toCopyCompletedLab(signal, takeOverPtr);
    return;
  } else {
    ndbrequire(takeOverPtr.p->toMasterStatus == TakeOverRecord::TO_UPDATE_TO);
  }//if
  TabRecordPtr tabPtr;
  tabPtr.i = takeOverPtr.p->toCurrentTabref;
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);

  FragmentstorePtr fragPtr;
  getFragstore(tabPtr.p, takeOverPtr.p->toCurrentFragid, fragPtr);
  takeOverPtr.p->toMasterStatus = TakeOverRecord::COPY_ACTIVE;
  BlockReference lqhRef = calcLqhBlockRef(takeOverPtr.p->toStartingNode);
  CopyActiveReq * const req = (CopyActiveReq *)&signal->theData[0];
  req->userPtr = takeOverPtr.i;
  req->userRef = reference();
  req->tableId = takeOverPtr.p->toCurrentTabref;
  req->fragId = takeOverPtr.p->toCurrentFragid;
  req->distributionKey = fragPtr.p->distributionKey;

  sendSignal(lqhRef, GSN_COPY_ACTIVEREQ, signal,
             CopyActiveReq::SignalLength, JBB);
}//Dbdih::execUPDATE_TOCONF()

void Dbdih::execCOPY_ACTIVECONF(Signal* signal) 
{
  const CopyActiveConf * const conf = (CopyActiveConf *)&signal->theData[0];
  jamEntry();
  CRASH_INSERTION(7143);

  TakeOverRecordPtr takeOverPtr;
  takeOverPtr.i = conf->userPtr;
  ptrCheckGuard(takeOverPtr, MAX_NDB_NODES, takeOverRecord);

  ndbrequire(conf->tableId == takeOverPtr.p->toCurrentTabref);
  ndbrequire(conf->fragId == takeOverPtr.p->toCurrentFragid);
  ndbrequire(checkNodeAlive(conf->startingNodeId));
  ndbrequire(takeOverPtr.p->toMasterStatus == TakeOverRecord::COPY_ACTIVE);

  takeOverPtr.p->startGci = conf->startGci;
  takeOverPtr.p->toMasterStatus = TakeOverRecord::LOCK_MUTEX;
  
  Mutex mutex(signal, c_mutexMgr, takeOverPtr.p->m_switchPrimaryMutexHandle);
  Callback c = { safe_cast(&Dbdih::switchPrimaryMutex_locked), takeOverPtr.i };
  ndbrequire(mutex.lock(c));
}//Dbdih::execCOPY_ACTIVECONF()

void
Dbdih::switchPrimaryMutex_locked(Signal* signal, Uint32 toPtrI, Uint32 retVal){
  jamEntry();
  ndbrequire(retVal == 0);

  TakeOverRecordPtr takeOverPtr;
  takeOverPtr.i = toPtrI;
  ptrCheckGuard(takeOverPtr, MAX_NDB_NODES, takeOverRecord);

  ndbrequire(takeOverPtr.p->toMasterStatus == TakeOverRecord::LOCK_MUTEX);
  
  if (!checkNodeAlive((takeOverPtr.p->toStartingNode))) {
    // We have mutex
    Mutex mutex(signal, c_mutexMgr, takeOverPtr.p->m_switchPrimaryMutexHandle);
    mutex.unlock(); // Ignore result
    
    c_createFragmentLock = RNIL;
    c_CREATE_FRAGREQ_Counter.clearWaitingFor();
    endTakeOver(takeOverPtr.i);
    return;
  }
  
  takeOverPtr.p->toMasterStatus = TakeOverRecord::COMMIT_CREATE;
  sendCreateFragReq(signal, takeOverPtr.p->startGci, 
		    CreateFragReq::COMMIT_STORED, takeOverPtr.i);
}

void Dbdih::toCopyCompletedLab(Signal * signal, TakeOverRecordPtr takeOverPtr)
{
  signal->theData[0] = NDB_LE_NR_CopyFragsCompleted;
  signal->theData[1] = takeOverPtr.p->toStartingNode;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);

  c_lcpState.immediateLcpStart = true;
  takeOverPtr.p->toMasterStatus = TakeOverRecord::WAIT_LCP;
  
  /*-----------------------------------------------------------------------*/
  /* NOW WE CAN ALLOW THE NEW NODE TO PARTICIPATE IN LOCAL CHECKPOINTS.    */
  /* WHEN THE FIRST LOCAL CHECKPOINT IS READY WE DECLARE THE TAKE OVER AS  */
  /* COMPLETED. SINCE LOCAL CHECKPOINTS HAVE BEEN BLOCKED DURING THE COPY  */
  /* PROCESS WE MUST ALSO START A NEW LOCAL CHECKPOINT PROCESS BY ENSURING */
  /* THAT IT LOOKS LIKE IT IS TIME FOR A NEW LOCAL CHECKPOINT AND BY       */
  /* UNBLOCKING THE LOCAL CHECKPOINT AGAIN.                                */
  /* --------------------------------------------------------------------- */
}//Dbdih::toCopyCompletedLab()

void Dbdih::sendEndTo(Signal* signal, Uint32 takeOverPtrI)
{
  TakeOverRecordPtr takeOverPtr;
  CRASH_INSERTION(7156);
  RETURN_IF_TAKE_OVER_INTERRUPTED(takeOverPtrI, takeOverPtr);
  if ((c_endToLock != RNIL) || (ERROR_INSERTED(7164))) {
    jam();
    takeOverPtr.p->toMasterStatus = TakeOverRecord::TO_WAIT_ENDING;
    signal->theData[0] = DihContinueB::ZSEND_END_TO;
    signal->theData[1] = takeOverPtrI;
    signal->theData[2] = takeOverPtr.p->toStartingNode;
    signal->theData[3] = takeOverPtr.p->toFailedNode;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 30, 4);
    return;
  }//if
  c_endToLock = takeOverPtr.i;
  takeOverPtr.p->toMasterStatus = TakeOverRecord::ENDING;
  EndToReq * const req = (EndToReq *)&signal->theData[0];
  req->userPtr = takeOverPtr.i;
  req->userRef = reference();
  req->startingNodeId = takeOverPtr.p->toStartingNode;
  sendLoopMacro(END_TOREQ, sendEND_TOREQ);
}//Dbdih::sendStartTo()

void Dbdih::execEND_TOREQ(Signal* signal)
{
  jamEntry();
  const EndToReq * const req = (EndToReq *)&signal->theData[0];
  BlockReference ref = req->userRef;
  Uint32 startingNodeId = req->startingNodeId;

  CRASH_INSERTION(7144);
  RETURN_IF_NODE_NOT_ALIVE(startingNodeId);

  TakeOverRecordPtr takeOverPtr;
  takeOverPtr.i = req->userPtr;
  ptrCheckGuard(takeOverPtr, MAX_NDB_NODES, takeOverRecord);

  ndbrequire(startingNodeId == takeOverPtr.p->toStartingNode);
  takeOverPtr.p->toSlaveStatus = TakeOverRecord::TO_SLAVE_IDLE;
  
  if (!isMaster()) {
    jam();
    endTakeOver(takeOverPtr.i);
  }//if

  EndToConf * const conf = (EndToConf *)&signal->theData[0];
  conf->userPtr = takeOverPtr.i;
  conf->sendingNodeId = cownNodeId;
  conf->startingNodeId = startingNodeId;
  sendSignal(ref, GSN_END_TOCONF, signal, EndToConf::SignalLength, JBB);
}//Dbdih::execEND_TOREQ()

void Dbdih::execEND_TOCONF(Signal* signal) 
{
  const EndToConf * const conf = (EndToConf *)&signal->theData[0];
  jamEntry();

  const Uint32 nodeId = conf->startingNodeId;
  CRASH_INSERTION(7145);

  RETURN_IF_NODE_NOT_ALIVE(nodeId);

  TakeOverRecordPtr takeOverPtr;
  takeOverPtr.i = conf->userPtr;
  ptrCheckGuard(takeOverPtr, MAX_NDB_NODES, takeOverRecord);

  ndbrequire(takeOverPtr.p->toMasterStatus == TakeOverRecord::ENDING);
  ndbrequire(nodeId == takeOverPtr.p->toStartingNode);

  receiveLoopMacro(END_TOREQ, conf->sendingNodeId);
  CRASH_INSERTION(7146);
  c_endToLock = RNIL;

  /* -----------------------------------------------------------------------*/
  /*  WE HAVE FINALLY COMPLETED THE TAKE OVER. WE RESET THE STATUS AND CHECK*/
  /*  IF ANY MORE TAKE OVERS ARE NEEDED AT THE MOMENT.                      */
  /*  FIRST WE CHECK IF A RESTART IS ONGOING. IN THAT CASE WE RESTART PHASE */
  /*  4 AND CHECK IF ANY MORE TAKE OVERS ARE NEEDED BEFORE WE START NDB     */
  /*  CLUSTER. THIS CAN ONLY HAPPEN IN A SYSTEM RESTART.                    */
  /* ---------------------------------------------------------------------- */
  if (takeOverPtr.p->toNodeRestart) {
    jam();
    /* ----------------------------------------------------------------------*/
    /* THE TAKE OVER NODE WAS A STARTING NODE. WE WILL SEND START_COPYCONF   */
    /* TO THE STARTING NODE SUCH THAT THE NODE CAN COMPLETE THE START-UP.    */
    /* --------------------------------------------------------------------- */
    BlockReference ref = calcDihBlockRef(takeOverPtr.p->toStartingNode);
    signal->theData[0] = takeOverPtr.p->toStartingNode;
    sendSignal(ref, GSN_START_COPYCONF, signal, 1,JBB);
  }//if
  endTakeOver(takeOverPtr.i);

  ndbout_c("2 - endTakeOver");
  if (cstartPhase == ZNDB_SPH4) {
    jam();
    ndbrequire(false);
    if (anyActiveTakeOver()) {
      jam();
      ndbout_c("4 - anyActiveTakeOver == true");
      return;
    }//if
    ndbout_c("5 - anyActiveTakeOver == false -> ndbsttorry10Lab");
    ndbsttorry10Lab(signal, __LINE__);
    return;
  }//if
  checkStartTakeOver(signal);
}//Dbdih::execEND_TOCONF()

void Dbdih::allocateTakeOver(TakeOverRecordPtr& takeOverPtr)
{
  if (isMaster()) {
    jam();
    //--------------------------------------------
    // Master already seized the take over record.
    //--------------------------------------------
    return;
  }//if
  if (takeOverPtr.i == cfirstfreeTakeOver) {
    jam();
    seizeTakeOver(takeOverPtr);
  } else {
    TakeOverRecordPtr nextTakeOverptr;
    TakeOverRecordPtr prevTakeOverptr;
    nextTakeOverptr.i = takeOverPtr.p->nextTakeOver;
    prevTakeOverptr.i = takeOverPtr.p->prevTakeOver;
    if (prevTakeOverptr.i != RNIL) {
      jam();
      ptrCheckGuard(prevTakeOverptr, MAX_NDB_NODES, takeOverRecord);
      prevTakeOverptr.p->nextTakeOver = nextTakeOverptr.i;
    }//if
    if (nextTakeOverptr.i != RNIL) {
      jam();
      ptrCheckGuard(nextTakeOverptr, MAX_NDB_NODES, takeOverRecord);
      nextTakeOverptr.p->prevTakeOver = prevTakeOverptr.i;
    }//if
  }//if
}//Dbdih::allocateTakeOver()

void Dbdih::seizeTakeOver(TakeOverRecordPtr& takeOverPtr)
{
  TakeOverRecordPtr nextTakeOverptr;
  ndbrequire(cfirstfreeTakeOver != RNIL);
  takeOverPtr.i = cfirstfreeTakeOver;
  ptrCheckGuard(takeOverPtr, MAX_NDB_NODES, takeOverRecord);
  cfirstfreeTakeOver = takeOverPtr.p->nextTakeOver;
  nextTakeOverptr.i = takeOverPtr.p->nextTakeOver;
  if (nextTakeOverptr.i != RNIL) {
    jam();
    ptrCheckGuard(nextTakeOverptr, MAX_NDB_NODES, takeOverRecord);
    nextTakeOverptr.p->prevTakeOver = RNIL;
  }//if
  takeOverPtr.p->nextTakeOver = RNIL;
  takeOverPtr.p->prevTakeOver = RNIL;
}//Dbdih::seizeTakeOver()

void Dbdih::endTakeOver(Uint32 takeOverPtrI)
{
  TakeOverRecordPtr takeOverPtr;
  takeOverPtr.i = takeOverPtrI;
  ptrCheckGuard(takeOverPtr, MAX_NDB_NODES, takeOverRecord);

  releaseTakeOver(takeOverPtrI);
  if ((takeOverPtr.p->toMasterStatus != TakeOverRecord::IDLE) &&
      (takeOverPtr.p->toMasterStatus != TakeOverRecord::TO_WAIT_START_TAKE_OVER)) {
    jam();
    NodeGroupRecordPtr NGPtr;
    NodeRecordPtr nodePtr;
    nodePtr.i = takeOverPtr.p->toStartingNode;
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
    NGPtr.i = nodePtr.p->nodeGroup;
    ptrCheckGuard(NGPtr, MAX_NDB_NODES, nodeGroupRecord);
    NGPtr.p->activeTakeOver = false;
  }//if
  setAllowNodeStart(takeOverPtr.p->toStartingNode, true);
  initTakeOver(takeOverPtr);
}//Dbdih::endTakeOver()

void Dbdih::releaseTakeOver(Uint32 takeOverPtrI)
{
  TakeOverRecordPtr takeOverPtr;
  takeOverPtr.i = takeOverPtrI;
  ptrCheckGuard(takeOverPtr, MAX_NDB_NODES, takeOverRecord);

  takeOverPtr.p->nextTakeOver = cfirstfreeTakeOver;
  cfirstfreeTakeOver = takeOverPtr.i;
}//Dbdih::releaseTakeOver()

void Dbdih::initTakeOver(TakeOverRecordPtr takeOverPtr)
{
  takeOverPtr.p->toCopyNode = RNIL;
  takeOverPtr.p->toCurrentFragid = RNIL;
  takeOverPtr.p->toCurrentReplica = RNIL;
  takeOverPtr.p->toCurrentTabref = RNIL;
  takeOverPtr.p->toFailedNode = RNIL;
  takeOverPtr.p->toStartingNode = RNIL;
  takeOverPtr.p->prevTakeOver = RNIL;
  takeOverPtr.p->nextTakeOver = RNIL;
  takeOverPtr.p->toNodeRestart = false;
  takeOverPtr.p->toMasterStatus = TakeOverRecord::IDLE;
  takeOverPtr.p->toSlaveStatus = TakeOverRecord::TO_SLAVE_IDLE;
}//Dbdih::initTakeOver()

bool Dbdih::anyActiveTakeOver()
{
  TakeOverRecordPtr takeOverPtr;
  for (takeOverPtr.i = 0; takeOverPtr.i < MAX_NDB_NODES; takeOverPtr.i++) {
    ptrAss(takeOverPtr, takeOverRecord);
    if (takeOverPtr.p->toMasterStatus != TakeOverRecord::IDLE) {
      jam();
      return true;
    }//if
  }//for
  return false;
}//Dbdih::anyActiveTakeOver()

/*****************************************************************************/
/* ------------------------------------------------------------------------- */
/*       WE HAVE BEEN REQUESTED TO PERFORM A SYSTEM RESTART. WE START BY     */
/*       READING THE GCI FILES. THIS REQUEST WILL ONLY BE SENT TO THE MASTER */
/*       DIH. THAT MEANS WE HAVE TO REPLICATE THE INFORMATION WE READ FROM   */
/*       OUR FILES TO ENSURE THAT ALL NODES HAVE THE SAME DISTRIBUTION       */
/*       INFORMATION.                                                        */
/* ------------------------------------------------------------------------- */
/*****************************************************************************/
void Dbdih::readGciFileLab(Signal* signal) 
{
  FileRecordPtr filePtr;
  filePtr.i = crestartInfoFile[0];
  ptrCheckGuard(filePtr, cfileFileSize, fileRecord);
  filePtr.p->reqStatus = FileRecord::OPENING_GCP;

  openFileRo(signal, filePtr);
}//Dbdih::readGciFileLab()

void Dbdih::openingGcpLab(Signal* signal, FileRecordPtr filePtr) 
{
  /* ----------------------------------------------------------------------- */
  /*     WE HAVE SUCCESSFULLY OPENED A FILE CONTAINING INFORMATION ABOUT     */
  /*     THE GLOBAL CHECKPOINTS THAT ARE POSSIBLE TO RESTART.                */
  /* ----------------------------------------------------------------------- */
  readRestorableGci(signal, filePtr);
  filePtr.p->reqStatus = FileRecord::READING_GCP;
}//Dbdih::openingGcpLab()

void Dbdih::readingGcpLab(Signal* signal, FileRecordPtr filePtr) 
{
  /* ----------------------------------------------------------------------- */
  /*     WE HAVE NOW SUCCESSFULLY MANAGED TO READ IN THE GLOBAL CHECKPOINT   */
  /*     INFORMATION FROM FILE. LATER WE WILL ADD SOME FUNCTIONALITY THAT    */
  /*     CHECKS THE RESTART TIMERS TO DEDUCE FROM WHERE TO RESTART.          */
  /*     NOW WE WILL SIMPLY RESTART FROM THE NEWEST GLOBAL CHECKPOINT        */
  /*     POSSIBLE TO RESTORE.                                                */
  /*                                                                         */
  /*     BEFORE WE INVOKE DICT WE NEED TO COPY CRESTART_INFO TO ALL NODES.   */
  /*     WE ALSO COPY TO OUR OWN NODE. TO ENABLE US TO DO THIS PROPERLY WE   */
  /*     START BY CLOSING THIS FILE.                                         */
  /* ----------------------------------------------------------------------- */
  closeFile(signal, filePtr);
  filePtr.p->reqStatus = FileRecord::CLOSING_GCP;
}//Dbdih::readingGcpLab()

void Dbdih::closingGcpLab(Signal* signal, FileRecordPtr filePtr) 
{
  if (Sysfile::getInitialStartOngoing(SYSFILE->systemRestartBits) == false){
    jam();
    selectMasterCandidateAndSend(signal); 
    return;
  } else {
    jam();
    sendSignal(cntrlblockref, GSN_DIH_RESTARTREF, signal, 1, JBB);
    return;
  }//if
}//Dbdih::closingGcpLab()

/* ------------------------------------------------------------------------- */
/*       SELECT THE MASTER CANDIDATE TO BE USED IN SYSTEM RESTARTS.          */
/* ------------------------------------------------------------------------- */
void Dbdih::selectMasterCandidateAndSend(Signal* signal)
{
  Uint32 gci = 0;
  Uint32 masterCandidateId = 0;
  NodeRecordPtr nodePtr;
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    jam();
    ptrAss(nodePtr, nodeRecord);
    if (SYSFILE->lastCompletedGCI[nodePtr.i] > gci) {
      jam();
      masterCandidateId = nodePtr.i;
      gci = SYSFILE->lastCompletedGCI[nodePtr.i];
    }//if
  }//for
  ndbrequire(masterCandidateId != 0);
  setNodeGroups();
  signal->theData[0] = masterCandidateId;
  signal->theData[1] = gci;
  sendSignal(cntrlblockref, GSN_DIH_RESTARTCONF, signal, 2, JBB);

  Uint32 node_groups[MAX_NDB_NODES];
  memset(node_groups, 0, sizeof(node_groups));
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    jam();
    const Uint32 ng = Sysfile::getNodeGroup(nodePtr.i, SYSFILE->nodeGroups);
    if(ng != NO_NODE_GROUP_ID){
      ndbrequire(ng < MAX_NDB_NODES);
      node_groups[ng]++;
    }
  }
  
  for (nodePtr.i = 0; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    jam();
    Uint32 count = node_groups[nodePtr.i];
    if(count != 0 && count != cnoReplicas){
      char buf[255];
      BaseString::snprintf(buf, sizeof(buf), 
	       "Illegal configuration change."
	       " Initial start needs to be performed "
	       " when changing no of replicas (%d != %d)", 
	       node_groups[nodePtr.i], cnoReplicas);
      progError(__LINE__, NDBD_EXIT_INVALID_CONFIG, buf);
    }
  }
}//Dbdih::selectMasterCandidate()

/* ------------------------------------------------------------------------- */
/*       ERROR HANDLING DURING READING RESTORABLE GCI FROM FILE.             */
/* ------------------------------------------------------------------------- */
void Dbdih::openingGcpErrorLab(Signal* signal, FileRecordPtr filePtr) 
{
  filePtr.p->fileStatus = FileRecord::CRASHED;
  filePtr.p->reqStatus = FileRecord::IDLE;
  if (crestartInfoFile[0] == filePtr.i) {
    jam();
    /* --------------------------------------------------------------------- */
    /*   THE FIRST FILE WAS NOT ABLE TO BE OPENED. SET STATUS TO CRASHED AND */
    /*   TRY OPEN THE NEXT FILE.                                             */
    /* --------------------------------------------------------------------- */
    filePtr.i = crestartInfoFile[1];
    ptrCheckGuard(filePtr, cfileFileSize, fileRecord);
    openFileRo(signal, filePtr);
    filePtr.p->reqStatus = FileRecord::OPENING_GCP;
  } else {
    jam();
    /* --------------------------------------------------------------------- */
    /*   WE FAILED IN OPENING THE SECOND FILE. BOTH FILES WERE CORRUPTED. WE */
    /*   CANNOT CONTINUE THE RESTART IN THIS CASE. TELL NDBCNTR OF OUR       */
    /*   FAILURE.                                                            */
    /*---------------------------------------------------------------------- */
    sendSignal(cntrlblockref, GSN_DIH_RESTARTREF, signal, 1, JBB);
    return;
  }//if
}//Dbdih::openingGcpErrorLab()

void Dbdih::readingGcpErrorLab(Signal* signal, FileRecordPtr filePtr) 
{
  filePtr.p->fileStatus = FileRecord::CRASHED;
  /* ----------------------------------------------------------------------- */
  /*     WE FAILED IN READING THE FILE AS WELL. WE WILL CLOSE THIS FILE.     */
  /* ----------------------------------------------------------------------- */
  closeFile(signal, filePtr);
  filePtr.p->reqStatus = FileRecord::CLOSING_GCP_CRASH;
}//Dbdih::readingGcpErrorLab()

void Dbdih::closingGcpCrashLab(Signal* signal, FileRecordPtr filePtr) 
{
  if (crestartInfoFile[0] == filePtr.i) {
    jam();
    /* --------------------------------------------------------------------- */
    /*   ERROR IN FIRST FILE, TRY THE SECOND FILE.                           */
    /* --------------------------------------------------------------------- */
    filePtr.i = crestartInfoFile[1];
    ptrCheckGuard(filePtr, cfileFileSize, fileRecord);
    openFileRw(signal, filePtr);
    filePtr.p->reqStatus = FileRecord::OPENING_GCP;
    return;
  }//if
  /* ----------------------------------------------------------------------- */
  /*     WE DISCOVERED A FAILURE WITH THE SECOND FILE AS WELL. THIS IS A     */
  /*     SERIOUS PROBLEM. REPORT FAILURE TO NDBCNTR.                         */
  /* ----------------------------------------------------------------------- */
  sendSignal(cntrlblockref, GSN_DIH_RESTARTREF, signal, 1, JBB);
}//Dbdih::closingGcpCrashLab()

/*****************************************************************************/
/* ------------------------------------------------------------------------- */
/*       THIS IS AN INITIAL RESTART. WE WILL CREATE THE TWO FILES DESCRIBING */
/*       THE GLOBAL CHECKPOINTS THAT ARE RESTORABLE.                         */
/* ------------------------------------------------------------------------- */
/*****************************************************************************/
void Dbdih::initGciFilesLab(Signal* signal) 
{
  FileRecordPtr filePtr;
  filePtr.i = crestartInfoFile[0];
  ptrCheckGuard(filePtr, cfileFileSize, fileRecord);
  createFileRw(signal, filePtr);
  filePtr.p->reqStatus = FileRecord::CREATING_GCP;
}//Dbdih::initGciFilesLab()

/* ------------------------------------------------------------------------- */
/*       GLOBAL CHECKPOINT FILE HAVE BEEN SUCCESSFULLY CREATED.              */
/* ------------------------------------------------------------------------- */
void Dbdih::creatingGcpLab(Signal* signal, FileRecordPtr filePtr) 
{
  if (filePtr.i == crestartInfoFile[0]) {
    jam();
    /* --------------------------------------------------------------------- */
    /*   IF CREATED FIRST THEN ALSO CREATE THE SECOND FILE.                  */
    /* --------------------------------------------------------------------- */
    filePtr.i = crestartInfoFile[1];
    ptrCheckGuard(filePtr, cfileFileSize, fileRecord);
    createFileRw(signal, filePtr);
    filePtr.p->reqStatus = FileRecord::CREATING_GCP;
  } else {
    jam();
    /* --------------------------------------------------------------------- */
    /*   BOTH FILES HAVE BEEN CREATED. NOW WRITE THE INITIAL DATA TO BOTH    */
    /*   OF THE FILES.                                                       */
    /* --------------------------------------------------------------------- */
    filePtr.i = crestartInfoFile[0];
    ptrCheckGuard(filePtr, cfileFileSize, fileRecord);
    writeRestorableGci(signal, filePtr);
    filePtr.p->reqStatus = FileRecord::WRITE_INIT_GCP;
  }//if
}//Dbdih::creatingGcpLab()

/* ------------------------------------------------------------------------- */
/*       WE HAVE SUCCESSFULLY WRITTEN A GCI FILE.                            */
/* ------------------------------------------------------------------------- */
void Dbdih::writeInitGcpLab(Signal* signal, FileRecordPtr filePtr) 
{
  filePtr.p->reqStatus = FileRecord::IDLE;
  if (filePtr.i == crestartInfoFile[0]) {
    jam();
    /* --------------------------------------------------------------------- */
    /*   WE HAVE WRITTEN THE FIRST FILE NOW ALSO WRITE THE SECOND FILE.      */
    /* --------------------------------------------------------------------- */
    filePtr.i = crestartInfoFile[1];
    ptrCheckGuard(filePtr, cfileFileSize, fileRecord);
    writeRestorableGci(signal, filePtr);
    filePtr.p->reqStatus = FileRecord::WRITE_INIT_GCP;
  } else {
    /* --------------------------------------------------------------------- */
    /*   WE HAVE WRITTEN BOTH FILES. LEAVE BOTH FILES OPEN AND CONFIRM OUR   */
    /*   PART OF THE INITIAL START.                                          */
    /* --------------------------------------------------------------------- */
    if (isMaster()) {
      jam();
      /*---------------------------------------------------------------------*/
      // IN MASTER NODES THE START REQUEST IS RECEIVED FROM NDBCNTR AND WE MUST
      // RESPOND WHEN COMPLETED.
      /*---------------------------------------------------------------------*/
      signal->theData[0] = reference();
      sendSignal(cndbStartReqBlockref, GSN_NDB_STARTCONF, signal, 1, JBB);
    } else {
      jam();
      ndbsttorry10Lab(signal, __LINE__);
      return;
    }//if
  }//if
}//Dbdih::writeInitGcpLab()

/*****************************************************************************/
/* **********     NODES DELETION MODULE                          *************/
/*****************************************************************************/
/*---------------------------------------------------------------------------*/
/*                    LOGIC FOR NODE FAILURE                                 */
/*---------------------------------------------------------------------------*/
void Dbdih::execNODE_FAILREP(Signal* signal)
{
  Uint32 i;
  Uint32 failedNodes[MAX_NDB_NODES];
  jamEntry();
  NodeFailRep * const nodeFail = (NodeFailRep *)&signal->theData[0];

  cfailurenr = nodeFail->failNo;
  Uint32 newMasterId = nodeFail->masterNodeId;
  const Uint32 noOfFailedNodes = nodeFail->noOfNodes;

  /*-------------------------------------------------------------------------*/
  // The first step is to convert from a bit mask to an array of failed nodes.
  /*-------------------------------------------------------------------------*/
  Uint32 index = 0;
  for (i = 1; i < MAX_NDB_NODES; i++) {
    jam();
    if(NodeBitmask::get(nodeFail->theNodes, i)){
      jam();
      failedNodes[index] = i;
      index++;
    }//if
  }//for
  ndbrequire(noOfFailedNodes == index);
  ndbrequire(noOfFailedNodes - 1 < MAX_NDB_NODES);

  /*-------------------------------------------------------------------------*/
  // The second step is to update the node status of the failed nodes, remove
  // them from the alive node list and put them into the dead node list. Also
  // update the number of nodes on-line.
  // We also set certain state variables ensuring that the node no longer is 
  // used in transactions and also mark that we received this signal.
  /*-------------------------------------------------------------------------*/
  for (i = 0; i < noOfFailedNodes; i++) {
    jam();
    NodeRecordPtr TNodePtr;
    TNodePtr.i = failedNodes[i];
    ptrCheckGuard(TNodePtr, MAX_NDB_NODES, nodeRecord);
    TNodePtr.p->useInTransactions = false;
    TNodePtr.p->m_inclDihLcp = false;
    TNodePtr.p->recNODE_FAILREP = ZTRUE;
    if (TNodePtr.p->nodeStatus == NodeRecord::ALIVE) {
      jam();
      con_lineNodes--;
      TNodePtr.p->nodeStatus = NodeRecord::DIED_NOW;
      removeAlive(TNodePtr);
      insertDeadNode(TNodePtr);
    }//if
  }//for

  /*-------------------------------------------------------------------------*/
  // Verify that we can continue to operate the cluster. If we cannot we will
  // not return from checkEscalation. 
  /*-------------------------------------------------------------------------*/
  checkEscalation();

  /*------------------------------------------------------------------------*/
  // Verify that a starting node has also crashed. Reset the node start record.
  /*-------------------------------------------------------------------------*/
  if (c_nodeStartMaster.startNode != RNIL) {
    ndbrequire(getNodeStatus(c_nodeStartMaster.startNode)!= NodeRecord::ALIVE);
  }//if

  /*--------------------------------------------------*/
  /*                                                  */
  /*       WE CHANGE THE REFERENCE TO MASTER DIH      */
  /*       BLOCK AND POINTER AT THIS PLACE IN THE CODE*/
  /*--------------------------------------------------*/
  Uint32 oldMasterId = cmasterNodeId;
  BlockReference oldMasterRef = cmasterdihref;
  cmasterdihref = calcDihBlockRef(newMasterId);
  cmasterNodeId = newMasterId;

  const bool masterTakeOver = (oldMasterId != newMasterId);

  for(i = 0; i < noOfFailedNodes; i++) {
    NodeRecordPtr failedNodePtr;
    failedNodePtr.i = failedNodes[i];
    ptrCheckGuard(failedNodePtr, MAX_NDB_NODES, nodeRecord);
    Uint32 activeTakeOverPtr = findTakeOver(failedNodes[i]);
    if (oldMasterRef == reference()) {
      /*-------------------------------------------------------*/
      // Functions that need to be called only for master nodes.
      /*-------------------------------------------------------*/
      checkCopyTab(failedNodePtr);
      checkStopPermMaster(signal, failedNodePtr);
      checkWaitGCPMaster(signal, failedNodes[i]);
      checkTakeOverInMasterAllNodeFailure(signal, failedNodePtr);
      checkTakeOverInMasterCopyNodeFailure(signal, failedNodePtr.i);
      checkTakeOverInMasterStartNodeFailure(signal, activeTakeOverPtr);
      checkGcpOutstanding(signal, failedNodePtr.i);
    } else {
      jam();
      /*-----------------------------------------------------------*/
      // Functions that need to be called only for nodes that were
      // not master before these failures.
      /*-----------------------------------------------------------*/
      checkStopPermProxy(signal, failedNodes[i]);
      checkWaitGCPProxy(signal, failedNodes[i]);
      if (isMaster()) {
	/*-----------------------------------------------------------*/
	// We take over as master since old master has failed
	/*-----------------------------------------------------------*/
        handleTakeOverNewMaster(signal, activeTakeOverPtr);
      } else {
	/*-----------------------------------------------------------*/
	// We are not master and will not become master.
	/*-----------------------------------------------------------*/
        checkTakeOverInNonMasterStartNodeFailure(signal, activeTakeOverPtr);
      }//if
    }//if
    /*--------------------------------------------------*/
    // Functions that need to be called for all nodes.
    /*--------------------------------------------------*/
    checkStopMe(signal, failedNodePtr);
    failedNodeLcpHandling(signal, failedNodePtr);
    checkWaitDropTabFailedLqh(signal, failedNodePtr.i, 0); // 0 = start w/ tab 0
    startRemoveFailedNode(signal, failedNodePtr);

    /**
     * This is the last function called
     *   It modifies failedNodePtr.p->nodeStatus
     */
    failedNodeSynchHandling(signal, failedNodePtr);
  }//for
  
  if(masterTakeOver){
    jam();
    startLcpMasterTakeOver(signal, oldMasterId);
    startGcpMasterTakeOver(signal, oldMasterId);

    if(getNodeState().getNodeRestartInProgress()){
      jam();
      progError(__LINE__, NDBD_EXIT_MASTER_FAILURE_DURING_NR);
    }
  }

  
  if (isMaster()) {
    jam();
    setNodeRestartInfoBits();
  }//if
}//Dbdih::execNODE_FAILREP()

void Dbdih::checkCopyTab(NodeRecordPtr failedNodePtr)
{
  jam();

  if(c_nodeStartMaster.startNode != failedNodePtr.i){
    jam();
    return;
  }
  
  switch(c_nodeStartMaster.m_outstandingGsn){
  case GSN_COPY_TABREQ:
    jam();
    ndbrequire(c_COPY_TABREQ_Counter.isWaitingFor(failedNodePtr.i));
    releaseTabPages(failedNodePtr.p->activeTabptr);
    c_COPY_TABREQ_Counter.clearWaitingFor(failedNodePtr.i);
    c_nodeStartMaster.wait = ZFALSE;
    break;
  case GSN_START_INFOREQ:
  case GSN_START_PERMCONF:
  case GSN_DICTSTARTREQ:
  case GSN_START_MECONF:
    jam();
    break;
  default:
    ndbout_c("outstanding gsn: %s(%d)", 
	     getSignalName(c_nodeStartMaster.m_outstandingGsn), 
	     c_nodeStartMaster.m_outstandingGsn);
    ndbrequire(false);
  }
  
  nodeResetStart();  
}//Dbdih::checkCopyTab()

void Dbdih::checkStopMe(Signal* signal, NodeRecordPtr failedNodePtr)
{
  jam();
  if (c_STOP_ME_REQ_Counter.isWaitingFor(failedNodePtr.i)){
    jam();
    ndbrequire(c_stopMe.clientRef != 0);
    StopMeConf * const stopMeConf = (StopMeConf *)&signal->theData[0];
    stopMeConf->senderRef = calcDihBlockRef(failedNodePtr.i);
    stopMeConf->senderData = c_stopMe.clientData;
    sendSignal(reference(), GSN_STOP_ME_CONF, signal, 
	       StopMeConf::SignalLength, JBB);
  }//if
}//Dbdih::checkStopMe()

void Dbdih::checkStopPermMaster(Signal* signal, NodeRecordPtr failedNodePtr)
{
  DihSwitchReplicaRef* const ref = (DihSwitchReplicaRef*)&signal->theData[0];
  jam();
  if (c_DIH_SWITCH_REPLICA_REQ_Counter.isWaitingFor(failedNodePtr.i)){
    jam();
    ndbrequire(c_stopPermMaster.clientRef != 0);
    ref->senderNode = failedNodePtr.i;
    ref->errorCode = StopPermRef::NF_CausedAbortOfStopProcedure;
    sendSignal(reference(), GSN_DIH_SWITCH_REPLICA_REF, signal,
               DihSwitchReplicaRef::SignalLength, JBB);
    return;
  }//if
}//Dbdih::checkStopPermMaster()

void Dbdih::checkStopPermProxy(Signal* signal, NodeId failedNodeId)
{
  jam();
  if(c_stopPermProxy.clientRef != 0 && 
     refToNode(c_stopPermProxy.masterRef) == failedNodeId){
    
    /**
     * The master has failed report to proxy-client
     */
    jam();
    StopPermRef* const ref = (StopPermRef*)&signal->theData[0];
    
    ref->senderData = c_stopPermProxy.clientData;
    ref->errorCode  = StopPermRef::NF_CausedAbortOfStopProcedure;
    sendSignal(c_stopPermProxy.clientRef, GSN_STOP_PERM_REF, signal, 2, JBB);
    c_stopPermProxy.clientRef = 0;
  }//if
}//Dbdih::checkStopPermProxy()

void 
Dbdih::checkTakeOverInMasterAllNodeFailure(Signal* signal, 
					   NodeRecordPtr failedNodePtr)
{
  //------------------------------------------------------------------------
  // This code is used to handle the failure of "all" nodes during the 
  // take over when "all" nodes are informed about state changes in 
  // the take over protocol.
  //--------------------------------------------------------------------------
  if (c_START_TOREQ_Counter.isWaitingFor(failedNodePtr.i)){
    jam();
    StartToConf * const conf = (StartToConf *)&signal->theData[0];
    conf->userPtr = c_startToLock;
    conf->sendingNodeId = failedNodePtr.i;
    conf->startingNodeId = getStartNode(c_startToLock);
    sendSignal(reference(), GSN_START_TOCONF, signal, 
	       StartToConf::SignalLength, JBB);
  }//if
  if (c_CREATE_FRAGREQ_Counter.isWaitingFor(failedNodePtr.i)){
    jam();
    CreateFragConf * const conf = (CreateFragConf *)&signal->theData[0];
    TakeOverRecordPtr takeOverPtr;
    takeOverPtr.i = c_createFragmentLock;
    ptrCheckGuard(takeOverPtr, MAX_NDB_NODES, takeOverRecord);
    conf->userPtr = takeOverPtr.i;
    conf->tableId = takeOverPtr.p->toCurrentTabref;
    conf->fragId = takeOverPtr.p->toCurrentFragid;
    conf->sendingNodeId = failedNodePtr.i;
    conf->startingNodeId = takeOverPtr.p->toStartingNode;
    sendSignal(reference(), GSN_CREATE_FRAGCONF, signal,
               CreateFragConf::SignalLength, JBB);
  }//if
  if (c_UPDATE_TOREQ_Counter.isWaitingFor(failedNodePtr.i)){
    jam();
    UpdateToConf * const conf = (UpdateToConf *)&signal->theData[0];
    conf->userPtr = c_updateToLock;
    conf->sendingNodeId = failedNodePtr.i;
    conf->startingNodeId = getStartNode(c_updateToLock);
    sendSignal(reference(), GSN_UPDATE_TOCONF, signal, 
	       UpdateToConf::SignalLength, JBB);
  }//if
  
  if (c_END_TOREQ_Counter.isWaitingFor(failedNodePtr.i)){
    jam();
    EndToConf * const conf = (EndToConf *)&signal->theData[0];
    conf->userPtr = c_endToLock;
    conf->sendingNodeId = failedNodePtr.i;
    conf->startingNodeId = getStartNode(c_endToLock);
    sendSignal(reference(), GSN_END_TOCONF, signal, 
	       EndToConf::SignalLength, JBB);
  }//if
}//Dbdih::checkTakeOverInMasterAllNodeFailure()

void Dbdih::checkTakeOverInMasterCopyNodeFailure(Signal* signal, 
						 Uint32 failedNodeId)
{
  //---------------------------------------------------------------------------
  // This code is used to handle failure of the copying node during a take over
  //---------------------------------------------------------------------------
  TakeOverRecordPtr takeOverPtr;
  for (Uint32 i = 0; i < MAX_NDB_NODES; i++) {
    jam();
    takeOverPtr.i = i;
    ptrCheckGuard(takeOverPtr, MAX_NDB_NODES, takeOverRecord);
    if ((takeOverPtr.p->toMasterStatus == TakeOverRecord::COPY_FRAG) &&
        (takeOverPtr.p->toCopyNode == failedNodeId)) {
      jam();
      /**
       * The copying node failed but the system is still operational. 
       * We restart the copy process by selecting a new copy node. 
       * We do not need to add a fragment however since it is already added. 
       * We start again from the prepare create fragment phase.
       */
      prepareSendCreateFragReq(signal, takeOverPtr.i);
    }//if
  }//for
}//Dbdih::checkTakeOverInMasterCopyNodeFailure()

void Dbdih::checkTakeOverInMasterStartNodeFailure(Signal* signal, 
						  Uint32 takeOverPtrI)
{
  jam();
  if (takeOverPtrI == RNIL) {
    jam();
    return;
  }
  //-----------------------------------------------------------------------
  // We are the master and the starting node has failed during a take over.
  // We need to handle this failure in different ways depending on the state.
  //-----------------------------------------------------------------------

  TakeOverRecordPtr takeOverPtr;
  takeOverPtr.i = takeOverPtrI;
  ptrCheckGuard(takeOverPtr, MAX_NDB_NODES, takeOverRecord);

  bool ok = false;
  switch (takeOverPtr.p->toMasterStatus) {
  case TakeOverRecord::IDLE:
    //-----------------------------------------------------------------------
    // The state cannot be idle when it has a starting node.
    //-----------------------------------------------------------------------
    ndbrequire(false);
    break;
  case TakeOverRecord::TO_WAIT_START_TAKE_OVER:
    jam();
  case TakeOverRecord::TO_START_COPY:
    jam();
  case TakeOverRecord::TO_START_COPY_ONGOING:
    jam();
  case TakeOverRecord::TO_WAIT_START:
    jam();
  case TakeOverRecord::TO_WAIT_PREPARE_CREATE:
    jam();
  case TakeOverRecord::TO_WAIT_UPDATE_TO:
    jam();
  case TakeOverRecord::TO_WAIT_COMMIT_CREATE:
    jam();
  case TakeOverRecord::TO_END_COPY:
    jam();
  case TakeOverRecord::TO_END_COPY_ONGOING:
    jam();
  case TakeOverRecord::TO_WAIT_ENDING:
    jam();
    //-----------------------------------------------------------------------
    // We will not do anything since an internal signal process is outstanding.
    // When the signal arrives the take over will be released.
    //-----------------------------------------------------------------------
    ok = true;
    break;
  case TakeOverRecord::STARTING:
    jam();
    ok = true;
    c_startToLock = RNIL;
    c_START_TOREQ_Counter.clearWaitingFor();
    endTakeOver(takeOverPtr.i);
    break;
  case TakeOverRecord::TO_UPDATE_TO:
    jam();
    ok = true;
    c_updateToLock = RNIL;
    c_UPDATE_TOREQ_Counter.clearWaitingFor();
    endTakeOver(takeOverPtr.i);
    break;
  case TakeOverRecord::ENDING:
    jam();
    ok = true;
    c_endToLock = RNIL;
    c_END_TOREQ_Counter.clearWaitingFor();
    endTakeOver(takeOverPtr.i);
    break;
  case TakeOverRecord::COMMIT_CREATE:
    ok = true;
    jam();
    {// We have mutex
      Mutex m(signal, c_mutexMgr, takeOverPtr.p->m_switchPrimaryMutexHandle);
      m.unlock(); // Ignore result
    }
    // Fall through
  case TakeOverRecord::PREPARE_CREATE:
    ok = true;
    jam();
    c_createFragmentLock = RNIL;
    c_CREATE_FRAGREQ_Counter.clearWaitingFor();
    endTakeOver(takeOverPtr.i);
    break;
  case TakeOverRecord::LOCK_MUTEX:
    ok = true;
    jam();
    // Lock mutex will return and do endTakeOver
    break;
    
    //-----------------------------------------------------------------------
    // Signals are outstanding to external nodes. These signals carry the node
    // id of the starting node and will not use the take over record if the
    // starting node has failed.
    //-----------------------------------------------------------------------
  case TakeOverRecord::COPY_FRAG:
    ok = true;
    jam();
    //-----------------------------------------------------------------------
    // The starting node will discover the problem. We will receive either
    // COPY_FRAGREQ or COPY_FRAGCONF and then we can release the take over
    // record and end the process. If the copying node should also die then
    // we will try to send prepare create fragment and will then discover
    // that the starting node has failed.
    //-----------------------------------------------------------------------
    break;
  case TakeOverRecord::COPY_ACTIVE:
    ok = true;
    jam();
    //-----------------------------------------------------------------------
    // In this we are waiting for a signal from the starting node. Thus we
    // can release the take over record and end the process.
    //-----------------------------------------------------------------------
    endTakeOver(takeOverPtr.i);
    break;
  case TakeOverRecord::WAIT_LCP:
    ok = true;
    jam();
    //-----------------------------------------------------------------------
    //-----------------------------------------------------------------------
    endTakeOver(takeOverPtr.i);
    break;
    /**
     * The following are states that it should not be possible to "be" in
     */
  case TakeOverRecord::SELECTING_NEXT:
    jam();
  case TakeOverRecord::TO_COPY_COMPLETED:
    jam();
    ndbrequire(false);
  }
  if(!ok){
    jamLine(takeOverPtr.p->toSlaveStatus);
    ndbrequire(ok);
  }
}//Dbdih::checkTakeOverInMasterStartNodeFailure()

void Dbdih::checkTakeOverInNonMasterStartNodeFailure(Signal* signal, 
						     Uint32 takeOverPtrI)
{
  jam();
  if (takeOverPtrI == RNIL) {
    jam();
    return;
  }
  //-----------------------------------------------------------------------
  // We are not master and not taking over as master. A take over was ongoing
  // but the starting node has now failed. Handle it according to the state
  // of the take over.
  //-----------------------------------------------------------------------
  TakeOverRecordPtr takeOverPtr;
  takeOverPtr.i = takeOverPtrI;
  ptrCheckGuard(takeOverPtr, MAX_NDB_NODES, takeOverRecord);
  bool ok = false;
  switch (takeOverPtr.p->toSlaveStatus) {
  case TakeOverRecord::TO_SLAVE_IDLE:
    ndbrequire(false);
    break;
  case TakeOverRecord::TO_SLAVE_STARTED:
    jam();
  case TakeOverRecord::TO_SLAVE_CREATE_PREPARE:
    jam();
  case TakeOverRecord::TO_SLAVE_COPY_FRAG_COMPLETED:
    jam();
  case TakeOverRecord::TO_SLAVE_CREATE_COMMIT:
    jam();
  case TakeOverRecord::TO_SLAVE_COPY_COMPLETED:
    jam();
    ok = true;
    endTakeOver(takeOverPtr.i);
    break;
  }//switch
  if(!ok){
    jamLine(takeOverPtr.p->toSlaveStatus);
    ndbrequire(ok);
  }
}//Dbdih::checkTakeOverInNonMasterStartNodeFailure()

void Dbdih::failedNodeSynchHandling(Signal* signal, 
				    NodeRecordPtr failedNodePtr)
{
  jam();
  /*----------------------------------------------------*/
  /*       INITIALISE THE VARIABLES THAT KEEP TRACK OF  */
  /*       WHEN A NODE FAILURE IS COMPLETED.            */
  /*----------------------------------------------------*/
  failedNodePtr.p->dbdictFailCompleted = ZFALSE;
  failedNodePtr.p->dbtcFailCompleted = ZFALSE;
  failedNodePtr.p->dbdihFailCompleted = ZFALSE;
  failedNodePtr.p->dblqhFailCompleted = ZFALSE;
  
  failedNodePtr.p->m_NF_COMPLETE_REP.clearWaitingFor();

  NodeRecordPtr nodePtr;
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    ptrAss(nodePtr, nodeRecord);
    if (nodePtr.p->nodeStatus == NodeRecord::ALIVE) {
      jam();
      /**
       * We'r waiting for nodePtr.i to complete 
       * handling of failedNodePtr.i's death
       */

      failedNodePtr.p->m_NF_COMPLETE_REP.setWaitingFor(nodePtr.i);
    } else {
      jam();
      if ((nodePtr.p->nodeStatus == NodeRecord::DYING) &&
          (nodePtr.p->m_NF_COMPLETE_REP.isWaitingFor(failedNodePtr.i))){
        jam();
	/*----------------------------------------------------*/
	/*       THE NODE FAILED BEFORE REPORTING THE FAILURE */
	/*       HANDLING COMPLETED ON THIS FAILED NODE.      */
	/*       REPORT THAT NODE FAILURE HANDLING WAS        */
	/*       COMPLETED ON THE NEW FAILED NODE FOR THIS    */
	/*       PARTICULAR OLD FAILED NODE.                  */
	/*----------------------------------------------------*/
        NFCompleteRep * const nf = (NFCompleteRep *)&signal->theData[0];
        nf->blockNo = 0;
        nf->nodeId  = failedNodePtr.i;
        nf->failedNodeId = nodePtr.i;
	nf->from    = __LINE__;
        sendSignal(reference(), GSN_NF_COMPLETEREP, signal, 
                   NFCompleteRep::SignalLength, JBB);
      }//if
    }//if
  }//for
  if (failedNodePtr.p->nodeStatus == NodeRecord::DIED_NOW) {
    jam();
    failedNodePtr.p->nodeStatus = NodeRecord::DYING;
  } else {
    jam();
    /*----------------------------------------------------*/
    // No more processing needed when node not even started
    // yet. We give the node status to DEAD since we do not
    // care whether all nodes complete the node failure
    // handling. The node have not been included in the
    // node failure protocols.
    /*----------------------------------------------------*/
    failedNodePtr.p->nodeStatus = NodeRecord::DEAD;
    /**-----------------------------------------------------------------------
     * WE HAVE COMPLETED HANDLING THE NODE FAILURE IN DIH. WE CAN REPORT THIS 
     * TO DIH THAT WAIT FOR THE OTHER BLOCKS TO BE CONCLUDED AS WELL.
     *-----------------------------------------------------------------------*/
    NFCompleteRep * const nf = (NFCompleteRep *)&signal->theData[0];
    nf->blockNo      = DBDIH;
    nf->nodeId       = cownNodeId;
    nf->failedNodeId = failedNodePtr.i;
    nf->from         = __LINE__;
    sendSignal(reference(), GSN_NF_COMPLETEREP, signal, 
               NFCompleteRep::SignalLength, JBB);
  }//if
}//Dbdih::failedNodeSynchHandling()

Uint32 Dbdih::findTakeOver(Uint32 failedNodeId)
{
  for (Uint32 i = 0; i < MAX_NDB_NODES; i++) {
    jam();
    TakeOverRecordPtr takeOverPtr;
    takeOverPtr.i = i;
    ptrCheckGuard(takeOverPtr, MAX_NDB_NODES, takeOverRecord);
    if (takeOverPtr.p->toStartingNode == failedNodeId) {
      jam();
      return i;
    }//if
  }//for
  return RNIL;
}//Dbdih::findTakeOver()

Uint32 Dbdih::getStartNode(Uint32 takeOverPtrI)
{
  TakeOverRecordPtr takeOverPtr;
  takeOverPtr.i = takeOverPtrI;
  ptrCheckGuard(takeOverPtr, MAX_NDB_NODES, takeOverRecord);
  return takeOverPtr.p->toStartingNode;
}//Dbdih::getStartNode()

void Dbdih::failedNodeLcpHandling(Signal* signal, NodeRecordPtr failedNodePtr)
{
  jam();
  const Uint32 nodeId = failedNodePtr.i;

  if (c_lcpState.m_participatingLQH.get(failedNodePtr.i)){
    /*----------------------------------------------------*/
    /*  THE NODE WAS INVOLVED IN A LOCAL CHECKPOINT. WE   */
    /* MUST UPDATE THE ACTIVE STATUS TO INDICATE THAT     */
    /* THE NODE HAVE MISSED A LOCAL CHECKPOINT.           */
    /*----------------------------------------------------*/
    switch (failedNodePtr.p->activeStatus) {
    case Sysfile::NS_Active:
      jam();
      failedNodePtr.p->activeStatus = Sysfile::NS_ActiveMissed_1;
      break;
    case Sysfile::NS_ActiveMissed_1:
      jam();
      failedNodePtr.p->activeStatus = Sysfile::NS_ActiveMissed_2;
      break;
    case Sysfile::NS_ActiveMissed_2:
      jam();
      failedNodePtr.p->activeStatus = Sysfile::NS_NotActive_NotTakenOver;
      break;
    case Sysfile::NS_TakeOver:
      jam();
      failedNodePtr.p->activeStatus = Sysfile::NS_NotActive_NotTakenOver;
      break;
    default:
      ndbout << "activeStatus = " << (Uint32) failedNodePtr.p->activeStatus;
      ndbout << " at failure after NODE_FAILREP of node = ";
      ndbout << failedNodePtr.i << endl;
      ndbrequire(false);
      break;
    }//switch
  }//if

  c_lcpState.m_participatingDIH.clear(failedNodePtr.i);
  c_lcpState.m_participatingLQH.clear(failedNodePtr.i);

  if(c_lcpState.m_LCP_COMPLETE_REP_Counter_DIH.isWaitingFor(failedNodePtr.i)){
    jam();
    LcpCompleteRep * rep = (LcpCompleteRep*)signal->getDataPtrSend();
    rep->nodeId = failedNodePtr.i;
    rep->lcpId = SYSFILE->latestLCP_ID;
    rep->blockNo = DBDIH;
    sendSignal(reference(), GSN_LCP_COMPLETE_REP, signal, 
	       LcpCompleteRep::SignalLength, JBB);
  }

  /**
   * Check if we'r waiting for the failed node's LQH to complete
   *
   * Note that this is ran "before" LCP master take over
   */
  if(c_lcpState.m_LCP_COMPLETE_REP_Counter_LQH.isWaitingFor(nodeId)){
    jam();

    LcpCompleteRep * rep = (LcpCompleteRep*)signal->getDataPtrSend();
    rep->nodeId  = nodeId;
    rep->lcpId   = SYSFILE->latestLCP_ID;
    rep->blockNo = DBLQH;
    sendSignal(reference(), GSN_LCP_COMPLETE_REP, signal, 
	       LcpCompleteRep::SignalLength, JBB);

    if(c_lcpState.m_LAST_LCP_FRAG_ORD.isWaitingFor(nodeId)){
      jam();
      /**
       * Make sure we're ready to accept it
       */
      c_lcpState.m_LAST_LCP_FRAG_ORD.clearWaitingFor(nodeId);
    }
  }
  
  if (c_TCGETOPSIZEREQ_Counter.isWaitingFor(failedNodePtr.i)) {
    jam();
    signal->theData[0] = failedNodePtr.i;
    signal->theData[1] = 0;
    sendSignal(reference(), GSN_TCGETOPSIZECONF, signal, 2, JBB);
  }//if
  
  if (c_TC_CLOPSIZEREQ_Counter.isWaitingFor(failedNodePtr.i)) {
    jam();
    signal->theData[0] = failedNodePtr.i;
    sendSignal(reference(), GSN_TC_CLOPSIZECONF, signal, 1, JBB);
  }//if

  if (c_START_LCP_REQ_Counter.isWaitingFor(failedNodePtr.i)) {
    jam();
    StartLcpConf * conf = (StartLcpConf*)signal->getDataPtrSend();
    conf->senderRef = numberToRef(DBLQH, failedNodePtr.i);
    conf->lcpId = SYSFILE->latestLCP_ID;
    sendSignal(reference(), GSN_START_LCP_CONF, signal, 
	       StartLcpConf::SignalLength, JBB);
  }//if
  
  if (c_EMPTY_LCP_REQ_Counter.isWaitingFor(failedNodePtr.i)) {
    jam();
    EmptyLcpConf * const rep = (EmptyLcpConf *)&signal->theData[0];
    rep->senderNodeId = failedNodePtr.i;
    rep->tableId = ~0;
    rep->fragmentId = ~0;
    rep->lcpNo = 0;
    rep->lcpId = SYSFILE->latestLCP_ID;
    rep->idle = true;
    sendSignal(reference(), GSN_EMPTY_LCP_CONF, signal, 
	       EmptyLcpConf::SignalLength, JBB);
  }//if

  if (c_MASTER_LCPREQ_Counter.isWaitingFor(failedNodePtr.i)) {
    jam();
    MasterLCPRef * const ref = (MasterLCPRef *)&signal->theData[0];
    ref->senderNodeId = failedNodePtr.i;
    ref->failedNodeId = cmasterTakeOverNode;
    sendSignal(reference(), GSN_MASTER_LCPREF, signal, 
	       MasterLCPRef::SignalLength, JBB);
  }//if
  
}//Dbdih::failedNodeLcpHandling()

void Dbdih::checkGcpOutstanding(Signal* signal, Uint32 failedNodeId){
  if (c_GCP_PREPARE_Counter.isWaitingFor(failedNodeId)){
    jam();
    signal->theData[0] = failedNodeId;
    signal->theData[1] = cnewgcp;
    sendSignal(reference(), GSN_GCP_PREPARECONF, signal, 2, JBB);
  }//if

  if (c_GCP_COMMIT_Counter.isWaitingFor(failedNodeId)) {
    jam();
    signal->theData[0] = failedNodeId;
    signal->theData[1] = coldgcp;
    signal->theData[2] = cfailurenr;
    sendSignal(reference(), GSN_GCP_NODEFINISH, signal, 3, JBB);
  }//if

  if (c_GCP_SAVEREQ_Counter.isWaitingFor(failedNodeId)) {
    jam();
    GCPSaveRef * const saveRef = (GCPSaveRef*)&signal->theData[0];
    saveRef->dihPtr = failedNodeId;
    saveRef->nodeId = failedNodeId;
    saveRef->gci    = coldgcp;
    saveRef->errorCode = GCPSaveRef::FakedSignalDueToNodeFailure;
    sendSignal(reference(), GSN_GCP_SAVEREF, signal, 
	       GCPSaveRef::SignalLength, JBB);
  }//if

  if (c_COPY_GCIREQ_Counter.isWaitingFor(failedNodeId)) {
    jam();
    signal->theData[0] = failedNodeId;
    sendSignal(reference(), GSN_COPY_GCICONF, signal, 1, JBB);
  }//if
  
  if (c_MASTER_GCPREQ_Counter.isWaitingFor(failedNodeId)){
    jam();
    MasterGCPRef * const ref = (MasterGCPRef *)&signal->theData[0];
    ref->senderNodeId = failedNodeId;
    ref->failedNodeId = cmasterTakeOverNode;
    sendSignal(reference(), GSN_MASTER_GCPREF, signal, 
	       MasterGCPRef::SignalLength, JBB);
  }//if
}//Dbdih::handleGcpStateInMaster()
 
 
void
Dbdih::startLcpMasterTakeOver(Signal* signal, Uint32 nodeId){
  jam();

  c_lcpMasterTakeOverState.minTableId = ~0;
  c_lcpMasterTakeOverState.minFragId = ~0;
  c_lcpMasterTakeOverState.failedNodeId = nodeId;
  
  c_lcpMasterTakeOverState.set(LMTOS_WAIT_EMPTY_LCP, __LINE__);
  
  if(c_EMPTY_LCP_REQ_Counter.done()){
    jam();
    c_lcpState.m_LAST_LCP_FRAG_ORD.clearWaitingFor();

    EmptyLcpReq* req = (EmptyLcpReq*)signal->getDataPtrSend();
    req->senderRef = reference();
    sendLoopMacro(EMPTY_LCP_REQ, sendEMPTY_LCP_REQ);
    ndbrequire(!c_EMPTY_LCP_REQ_Counter.done());
  } else {
    /**
     * Node failure during master take over...
     */
    ndbout_c("Nodefail during master take over");
  }
  
  setLocalNodefailHandling(signal, nodeId, NF_LCP_TAKE_OVER);
}

void Dbdih::startGcpMasterTakeOver(Signal* signal, Uint32 oldMasterId){
  jam();
  /*--------------------------------------------------*/
  /*                                                  */
  /*       THE MASTER HAVE FAILED AND WE WERE ELECTED */
  /*       TO BE THE NEW MASTER NODE. WE NEED TO QUERY*/
  /*       ALL THE OTHER NODES ABOUT THEIR STATUS IN  */
  /*       ORDER TO BE ABLE TO TAKE OVER CONTROL OF   */
  /*       THE GLOBAL CHECKPOINT PROTOCOL AND THE     */
  /*       LOCAL CHECKPOINT PROTOCOL.                 */
  /*--------------------------------------------------*/
  if(!isMaster()){
    jam();
    return;
  }
  cmasterState = MASTER_TAKE_OVER_GCP;
  cmasterTakeOverNode = oldMasterId;
  MasterGCPReq * const req = (MasterGCPReq *)&signal->theData[0];  
  req->masterRef = reference();
  req->failedNodeId = oldMasterId;
  sendLoopMacro(MASTER_GCPREQ, sendMASTER_GCPREQ);
  cgcpMasterTakeOverState = GMTOS_INITIAL;
  
  signal->theData[0] = NDB_LE_GCP_TakeoverStarted;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 1, JBB);

  setLocalNodefailHandling(signal, oldMasterId, NF_GCP_TAKE_OVER);
}//Dbdih::handleNewMaster()

void Dbdih::handleTakeOverNewMaster(Signal* signal, Uint32 takeOverPtrI)
{
  jam();
  if (takeOverPtrI != RNIL) {
    jam();
    TakeOverRecordPtr takeOverPtr;
    takeOverPtr.i = takeOverPtrI;
    ptrCheckGuard(takeOverPtr, MAX_NDB_NODES, takeOverRecord);
    bool ok = false;
    switch (takeOverPtr.p->toSlaveStatus) {
    case TakeOverRecord::TO_SLAVE_IDLE:
      ndbrequire(false);
      break;
    case TakeOverRecord::TO_SLAVE_STARTED:
      jam();
    case TakeOverRecord::TO_SLAVE_CREATE_PREPARE:
      jam();
    case TakeOverRecord::TO_SLAVE_COPY_FRAG_COMPLETED:
      jam();
    case TakeOverRecord::TO_SLAVE_CREATE_COMMIT:
      jam();
      ok = true;
      infoEvent("Unhandled MasterTO of TO slaveStatus=%d killing node %d",
		takeOverPtr.p->toSlaveStatus,
		takeOverPtr.p->toStartingNode);
      takeOverPtr.p->toMasterStatus = TakeOverRecord::COPY_ACTIVE;
      
      {
	BlockReference cntrRef = calcNdbCntrBlockRef(takeOverPtr.p->toStartingNode);
	SystemError * const sysErr = (SystemError*)&signal->theData[0];
	sysErr->errorCode = SystemError::CopyFragRefError;
	sysErr->errorRef = reference();
	sysErr->data1= 0;
	sysErr->data2= __LINE__;
	sendSignal(cntrRef, GSN_SYSTEM_ERROR, signal, 
		   SystemError::SignalLength, JBB);
      }
      break;
    case TakeOverRecord::TO_SLAVE_COPY_COMPLETED:
      ok = true;
      jam();
      takeOverPtr.p->toMasterStatus = TakeOverRecord::WAIT_LCP;
      break;
    }
    ndbrequire(ok);
  }//if
}//Dbdih::handleTakeOverNewMaster()

void Dbdih::startRemoveFailedNode(Signal* signal, NodeRecordPtr failedNodePtr)
{
  Uint32 nodeId = failedNodePtr.i;
  if(failedNodePtr.p->nodeStatus != NodeRecord::DIED_NOW){
    jam();
    /**
     * Is node isn't alive. It can't be part of LCP
     */
    ndbrequire(!c_lcpState.m_LCP_COMPLETE_REP_Counter_LQH.isWaitingFor(nodeId));
    
    /**
     * And there is no point in removing any replicas
     *   It's dead...
     */
    return;
  }
  
  jam();
  signal->theData[0] = DihContinueB::ZREMOVE_NODE_FROM_TABLE;
  signal->theData[1] = failedNodePtr.i;
  signal->theData[2] = 0; // Tab id
  sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBB);
  
  setLocalNodefailHandling(signal, failedNodePtr.i, NF_REMOVE_NODE_FROM_TABLE);
}//Dbdih::startRemoveFailedNode()

/*--------------------------------------------------*/
/*       THE MASTER HAS FAILED AND THE NEW MASTER IS*/
/*       QUERYING THIS NODE ABOUT THE STATE OF THE  */
/*       GLOBAL CHECKPOINT PROTOCOL                 */
/*--------------------------------------------------*/
void Dbdih::execMASTER_GCPREQ(Signal* signal) 
{
  NodeRecordPtr failedNodePtr;
  MasterGCPReq * const masterGCPReq = (MasterGCPReq *)&signal->theData[0];  
  jamEntry();
  const BlockReference newMasterBlockref = masterGCPReq->masterRef;
  const Uint32 failedNodeId = masterGCPReq->failedNodeId;
  if (c_copyGCISlave.m_copyReason != CopyGCIReq::IDLE) {
    jam();
    /*--------------------------------------------------*/
    /*       WE ARE CURRENTLY WRITING THE RESTART INFO  */
    /*       IN THIS NODE. SINCE ONLY ONE PROCESS IS    */
    /*       ALLOWED TO DO THIS AT A TIME WE MUST ENSURE*/
    /*       THAT THIS IS NOT ONGOING WHEN THE NEW      */
    /*       MASTER TAKES OVER CONTROL. IF NOT ALL NODES*/
    /*       RECEIVE THE SAME RESTART INFO DUE TO THE   */
    /*       FAILURE OF THE MASTER IT IS TAKEN CARE OF  */
    /*       BY THE NEW MASTER.                         */
    /*--------------------------------------------------*/
    sendSignalWithDelay(reference(), GSN_MASTER_GCPREQ,
                        signal, 10, MasterGCPReq::SignalLength);
    return;
  }//if
  failedNodePtr.i = failedNodeId;
  ptrCheckGuard(failedNodePtr, MAX_NDB_NODES, nodeRecord);
  if (failedNodePtr.p->nodeStatus == NodeRecord::ALIVE) {
    jam();
    /*--------------------------------------------------*/
    /*       ENSURE THAT WE HAVE PROCESSED THE SIGNAL   */
    /*       NODE_FAILURE BEFORE WE PROCESS THIS REQUEST*/
    /*       FROM THE NEW MASTER. THIS ENSURES THAT WE  */
    /*       HAVE REMOVED THE FAILED NODE FROM THE LIST */
    /*       OF ACTIVE NODES AND SO FORTH.              */
    /*--------------------------------------------------*/
    sendSignalWithDelay(reference(), GSN_MASTER_GCPREQ,
                        signal, 10, MasterGCPReq::SignalLength);
    return;
  } else {
    ndbrequire(failedNodePtr.p->nodeStatus == NodeRecord::DYING);
  }//if
  MasterGCPConf::State gcpState;
  switch (cgcpParticipantState) {
  case GCP_PARTICIPANT_READY:
    jam();
    /*--------------------------------------------------*/
    /*       THE GLOBAL CHECKPOINT IS NOT ACTIVE SINCE  */
    /*       THE PREVIOUS GLOBAL CHECKPOINT IS COMPLETED*/
    /*       AND THE NEW HAVE NOT STARTED YET.          */
    /*--------------------------------------------------*/
    gcpState = MasterGCPConf::GCP_READY;
    break;
  case GCP_PARTICIPANT_PREPARE_RECEIVED:
    jam();
    /*--------------------------------------------------*/
    /*       GCP_PREPARE HAVE BEEN RECEIVED AND RESPONSE*/
    /*       HAVE BEEN SENT.                            */
    /*--------------------------------------------------*/
    gcpState = MasterGCPConf::GCP_PREPARE_RECEIVED;
    break;
  case GCP_PARTICIPANT_COMMIT_RECEIVED:
    jam();
    /*------------------------------------------------*/
    /*       GCP_COMMIT HAVE BEEN RECEIVED BUT NOT YET*/
    /*       GCP_TCFINISHED FROM LOCAL TC.            */
    /*------------------------------------------------*/
    gcpState = MasterGCPConf::GCP_COMMIT_RECEIVED;
    break;
  case GCP_PARTICIPANT_TC_FINISHED:
    jam();
    /*------------------------------------------------*/
    /*       GCP_COMMIT HAS BEEN RECEIVED AND ALSO    */
    /*       GCP_TCFINISHED HAVE BEEN RECEIVED.       */
    /*------------------------------------------------*/
    gcpState = MasterGCPConf::GCP_TC_FINISHED;
    break;
  case GCP_PARTICIPANT_COPY_GCI_RECEIVED:
    /*--------------------------------------------------*/
    /*       COPY RESTART INFORMATION HAS BEEN RECEIVED */
    /*       BUT NOT YET COMPLETED.                     */
    /*--------------------------------------------------*/
    ndbrequire(false);
    gcpState= MasterGCPConf::GCP_READY; // remove warning
    break;
  default:
    /*------------------------------------------------*/
    /*                                                */
    /*       THIS SHOULD NOT OCCUR SINCE THE ABOVE    */
    /*       STATES ARE THE ONLY POSSIBLE STATES AT A */
    /*       NODE WHICH WAS NOT A MASTER NODE.        */
    /*------------------------------------------------*/
    ndbrequire(false);
    gcpState= MasterGCPConf::GCP_READY; // remove warning
    break;
  }//switch
  MasterGCPConf * const masterGCPConf = (MasterGCPConf *)&signal->theData[0];  
  masterGCPConf->gcpState  = gcpState;
  masterGCPConf->senderNodeId = cownNodeId;
  masterGCPConf->failedNodeId = failedNodeId;
  masterGCPConf->newGCP = cnewgcp;
  masterGCPConf->latestLCP = SYSFILE->latestLCP_ID;
  masterGCPConf->oldestRestorableGCI = SYSFILE->oldestRestorableGCI;
  masterGCPConf->keepGCI = SYSFILE->keepGCI;  
  for(Uint32 i = 0; i < NdbNodeBitmask::Size; i++)
    masterGCPConf->lcpActive[i] = SYSFILE->lcpActive[i];
  sendSignal(newMasterBlockref, GSN_MASTER_GCPCONF, signal, 
             MasterGCPConf::SignalLength, JBB);
}//Dbdih::execMASTER_GCPREQ()

void Dbdih::execMASTER_GCPCONF(Signal* signal) 
{
  NodeRecordPtr senderNodePtr;
  MasterGCPConf * const masterGCPConf = (MasterGCPConf *)&signal->theData[0];
  jamEntry();
  senderNodePtr.i = masterGCPConf->senderNodeId;
  ptrCheckGuard(senderNodePtr, MAX_NDB_NODES, nodeRecord);
  
  MasterGCPConf::State gcpState = (MasterGCPConf::State)masterGCPConf->gcpState;
  const Uint32 failedNodeId = masterGCPConf->failedNodeId;
  const Uint32 newGcp = masterGCPConf->newGCP;
  const Uint32 latestLcpId = masterGCPConf->latestLCP;
  const Uint32 oldestRestorableGci = masterGCPConf->oldestRestorableGCI;
  const Uint32 oldestKeepGci = masterGCPConf->keepGCI;
  if (latestLcpId > SYSFILE->latestLCP_ID) {
    jam();
#if 0
    ndbout_c("Dbdih: Setting SYSFILE->latestLCP_ID to %d", latestLcpId);
    SYSFILE->latestLCP_ID = latestLcpId;
#endif
    SYSFILE->keepGCI = oldestKeepGci;
    SYSFILE->oldestRestorableGCI = oldestRestorableGci;
    for(Uint32 i = 0; i < NdbNodeBitmask::Size; i++)
      SYSFILE->lcpActive[i] = masterGCPConf->lcpActive[i];
  }//if
  switch (gcpState) {
  case MasterGCPConf::GCP_READY:
    jam();
    senderNodePtr.p->gcpstate = NodeRecord::READY;
    break;
  case MasterGCPConf::GCP_PREPARE_RECEIVED:
    jam();
    senderNodePtr.p->gcpstate = NodeRecord::PREPARE_RECEIVED;
    cnewgcp = newGcp;
    break;
  case MasterGCPConf::GCP_COMMIT_RECEIVED:
    jam();
    senderNodePtr.p->gcpstate = NodeRecord::COMMIT_SENT;
    break;
  case MasterGCPConf::GCP_TC_FINISHED:
    jam();
    senderNodePtr.p->gcpstate = NodeRecord::NODE_FINISHED;
    break;
  default:
    ndbrequire(false);
    break;
  }//switch
  switch (cgcpMasterTakeOverState) {
  case GMTOS_INITIAL:
    switch (gcpState) {
    case MasterGCPConf::GCP_READY:
      jam();
      cgcpMasterTakeOverState = ALL_READY;
      break;
    case MasterGCPConf::GCP_PREPARE_RECEIVED:
      jam();
      cgcpMasterTakeOverState = ALL_PREPARED;
      break;
    case MasterGCPConf::GCP_COMMIT_RECEIVED:
      jam();
      cgcpMasterTakeOverState = COMMIT_STARTED_NOT_COMPLETED;
      break;
    case MasterGCPConf::GCP_TC_FINISHED:
      jam();
      cgcpMasterTakeOverState = COMMIT_COMPLETED;
      break;
    default:
      ndbrequire(false);
      break;
    }//switch
    break;
  case ALL_READY:
    switch (gcpState) {
    case MasterGCPConf::GCP_READY:
      jam();
      /*empty*/;
      break;
    case MasterGCPConf::GCP_PREPARE_RECEIVED:
      jam();
      cgcpMasterTakeOverState = PREPARE_STARTED_NOT_COMMITTED;
      break;
    case MasterGCPConf::GCP_COMMIT_RECEIVED:
      ndbrequire(false);
      break;
    case MasterGCPConf::GCP_TC_FINISHED:
      jam();
      cgcpMasterTakeOverState = SAVE_STARTED_NOT_COMPLETED;
      break;
    default:
      ndbrequire(false);
      break;
    }//switch
    break;
  case PREPARE_STARTED_NOT_COMMITTED:
    switch (gcpState) {
    case MasterGCPConf::GCP_READY:
      jam();
      break;
    case MasterGCPConf::GCP_PREPARE_RECEIVED:
      jam();
      break;
    case MasterGCPConf::GCP_COMMIT_RECEIVED:
      ndbrequire(false);
      break;
    case MasterGCPConf::GCP_TC_FINISHED:
      ndbrequire(false);
      break;
    default:
      ndbrequire(false);
      break;
    }//switch
    break;
  case ALL_PREPARED:
    switch (gcpState) {
    case MasterGCPConf::GCP_READY:
      jam();
      cgcpMasterTakeOverState = PREPARE_STARTED_NOT_COMMITTED;
      break;
    case MasterGCPConf::GCP_PREPARE_RECEIVED:
      jam();
      break;
    case MasterGCPConf::GCP_COMMIT_RECEIVED:
      jam();
      cgcpMasterTakeOverState = COMMIT_STARTED_NOT_COMPLETED;
      break;
    case MasterGCPConf::GCP_TC_FINISHED:
      jam();
      cgcpMasterTakeOverState = COMMIT_STARTED_NOT_COMPLETED;
      break;
    default:
      ndbrequire(false);
      break;
    }//switch
    break;
  case COMMIT_STARTED_NOT_COMPLETED:
    switch (gcpState) {
    case MasterGCPConf::GCP_READY:
      ndbrequire(false);
      break;
    case MasterGCPConf::GCP_PREPARE_RECEIVED:
      jam();
      break;
    case MasterGCPConf::GCP_COMMIT_RECEIVED:
      jam();
      break;
    case MasterGCPConf::GCP_TC_FINISHED:
      jam();
      break;
    default:
      ndbrequire(false);
      break;
    }//switch
    break;
  case COMMIT_COMPLETED:
    switch (gcpState) {
    case MasterGCPConf::GCP_READY:
      cgcpMasterTakeOverState = SAVE_STARTED_NOT_COMPLETED;
      break;
    case MasterGCPConf::GCP_PREPARE_RECEIVED:
      jam();
      cgcpMasterTakeOverState = COMMIT_STARTED_NOT_COMPLETED;
      break;
    case MasterGCPConf::GCP_COMMIT_RECEIVED:
      jam();
      cgcpMasterTakeOverState = COMMIT_STARTED_NOT_COMPLETED;
      break;
    case MasterGCPConf::GCP_TC_FINISHED:
      jam();
      break;
    default:
      ndbrequire(false);
      break;
    }//switch
    break;
  case SAVE_STARTED_NOT_COMPLETED:
    switch (gcpState) {
    case MasterGCPConf::GCP_READY:
      jam();
      break;
    case MasterGCPConf::GCP_PREPARE_RECEIVED:
      ndbrequire(false);
      break;
    case MasterGCPConf::GCP_COMMIT_RECEIVED:
      ndbrequire(false);
      break;
    case MasterGCPConf::GCP_TC_FINISHED:
      jam();
      break;
    default:
      ndbrequire(false);
      break;
    }//switch
    break;
  default:
    ndbrequire(false);
    break;
  }//switch
  receiveLoopMacro(MASTER_GCPREQ, senderNodePtr.i);
  /*-------------------------------------------------------------------------*/
  // We have now received all responses and are ready to take over the GCP
  // protocol as master.
  /*-------------------------------------------------------------------------*/
  MASTER_GCPhandling(signal, failedNodeId);
  return;
}//Dbdih::execMASTER_GCPCONF()

void Dbdih::execMASTER_GCPREF(Signal* signal) 
{
  const MasterGCPRef * const ref = (MasterGCPRef *)&signal->theData[0];
  jamEntry();
  receiveLoopMacro(MASTER_GCPREQ, ref->senderNodeId);
  /*-------------------------------------------------------------------------*/
  // We have now received all responses and are ready to take over the GCP
  // protocol as master.
  /*-------------------------------------------------------------------------*/
  MASTER_GCPhandling(signal, ref->failedNodeId);
}//Dbdih::execMASTER_GCPREF()

void Dbdih::MASTER_GCPhandling(Signal* signal, Uint32 failedNodeId) 
{
  NodeRecordPtr failedNodePtr;
  cmasterState = MASTER_ACTIVE;
  /*----------------------------------------------------------*/
  /*       REMOVE ALL ACTIVE STATUS ON ALREADY FAILED NODES   */
  /*       THIS IS PERFORMED HERE SINCE WE GET THE LCP ACTIVE */
  /*       STATUS AS PART OF THE COPY RESTART INFO AND THIS IS*/
  /*       HANDLED BY THE MASTER GCP TAKE OVER PROTOCOL.      */
  /*----------------------------------------------------------*/
  
  failedNodePtr.i = failedNodeId;
  ptrCheckGuard(failedNodePtr, MAX_NDB_NODES, nodeRecord);
  switch (cgcpMasterTakeOverState) {
  case ALL_READY:
    jam();
    startGcp(signal);
    break;
  case PREPARE_STARTED_NOT_COMMITTED:
    {
      NodeRecordPtr nodePtr;
      jam();
      c_GCP_PREPARE_Counter.clearWaitingFor();
      nodePtr.i = cfirstAliveNode;
      do {
	jam();
	ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
	if (nodePtr.p->gcpstate == NodeRecord::READY) {
	  jam();
	  c_GCP_PREPARE_Counter.setWaitingFor(nodePtr.i);
	  sendGCP_PREPARE(signal, nodePtr.i);
	}//if
	nodePtr.i = nodePtr.p->nextNode;
      } while(nodePtr.i != RNIL);
      if (c_GCP_PREPARE_Counter.done()) {
	jam();
	gcpcommitreqLab(signal);
      }//if
      break;
    }
  case ALL_PREPARED:
    jam();
    gcpcommitreqLab(signal);
    break;
  case COMMIT_STARTED_NOT_COMPLETED:
    {
      NodeRecordPtr nodePtr;
      jam();
      c_GCP_COMMIT_Counter.clearWaitingFor();
      nodePtr.i = cfirstAliveNode;
      do {
	jam();
	ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
	if (nodePtr.p->gcpstate == NodeRecord::PREPARE_RECEIVED) {
	  jam();
	  sendGCP_COMMIT(signal, nodePtr.i);
	  c_GCP_COMMIT_Counter.setWaitingFor(nodePtr.i);
	} else {
	  ndbrequire((nodePtr.p->gcpstate == NodeRecord::NODE_FINISHED) ||
		     (nodePtr.p->gcpstate == NodeRecord::COMMIT_SENT));
	}//if
	nodePtr.i = nodePtr.p->nextNode;
      } while(nodePtr.i != RNIL);
      if (c_GCP_COMMIT_Counter.done()){
	jam();
	gcpsavereqLab(signal);
      }//if
      break;
    }
  case COMMIT_COMPLETED:
    jam();
    gcpsavereqLab(signal);
    break;
  case SAVE_STARTED_NOT_COMPLETED:
    {
      NodeRecordPtr nodePtr;
      jam();
      SYSFILE->newestRestorableGCI = coldgcp;
      nodePtr.i = cfirstAliveNode;
      do {
	jam();
	ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
	SYSFILE->lastCompletedGCI[nodePtr.i] = coldgcp;
	nodePtr.i = nodePtr.p->nextNode;
      } while (nodePtr.i != RNIL);
      /**-------------------------------------------------------------------
       * THE FAILED NODE DID ALSO PARTICIPATE IN THIS GLOBAL CHECKPOINT 
       * WHICH IS RECORDED.
       *-------------------------------------------------------------------*/
      SYSFILE->lastCompletedGCI[failedNodeId] = coldgcp;
      copyGciLab(signal, CopyGCIReq::GLOBAL_CHECKPOINT);
      break;
    }
  default:
    ndbrequire(false);
    break;
  }//switch

  signal->theData[0] = NDB_LE_GCP_TakeoverCompleted;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 1, JBB);

  /*--------------------------------------------------*/
  /*       WE SEPARATE HANDLING OF GLOBAL CHECKPOINTS */
  /*       AND LOCAL CHECKPOINTS HERE. LCP'S HAVE TO  */
  /*       REMOVE ALL FAILED FRAGMENTS BEFORE WE CAN  */
  /*       HANDLE THE LCP PROTOCOL.                   */
  /*--------------------------------------------------*/
  checkLocalNodefailComplete(signal, failedNodeId, NF_GCP_TAKE_OVER);
  
  return;
}//Dbdih::masterGcpConfFromFailedLab()

void
Dbdih::invalidateNodeLCP(Signal* signal, Uint32 nodeId, Uint32 tableId)
{
  jamEntry();
  TabRecordPtr tabPtr;
  tabPtr.i = tableId;  
  const Uint32 RT_BREAK = 64;
  if (ERROR_INSERTED(7125)) {
    return;
  }//if
  for (Uint32 i = 0; i<RT_BREAK; i++) {
    jam();
    if (tabPtr.i >= ctabFileSize){
      jam();
      /**
       * Ready with entire loop
       * Return to master
       */
      setAllowNodeStart(nodeId, true);
      if (getNodeStatus(nodeId) == NodeRecord::STARTING) {
        jam();
        StartInfoConf * conf = (StartInfoConf*)&signal->theData[0];
        conf->sendingNodeId = cownNodeId;
        conf->startingNodeId = nodeId;
        sendSignal(cmasterdihref, GSN_START_INFOCONF, signal,
                   StartInfoConf::SignalLength, JBB);
      }//if
      return;
    }//if
    ptrAss(tabPtr, tabRecord);
    if (tabPtr.p->tabStatus == TabRecord::TS_ACTIVE) {
      jam();
      invalidateNodeLCP(signal, nodeId, tabPtr);
      return;
    }//if
    tabPtr.i++;
  }//for
  signal->theData[0] = DihContinueB::ZINVALIDATE_NODE_LCP;
  signal->theData[1] = nodeId;
  signal->theData[2] = tabPtr.i;
  sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBB);
}//Dbdih::invalidateNodeLCP()

void
Dbdih::invalidateNodeLCP(Signal* signal, Uint32 nodeId, TabRecordPtr tabPtr)
{  
  /**
   * Check so that no one else is using the tab descriptior
   */
  if (tabPtr.p->tabCopyStatus != TabRecord::CS_IDLE) {
    jam();    
    signal->theData[0] = DihContinueB::ZINVALIDATE_NODE_LCP;
    signal->theData[1] = nodeId;
    signal->theData[2] = tabPtr.i;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 20, 3);
    return;
  }//if  

  /**
   * For each fragment
   */
  bool modified = false;
  FragmentstorePtr fragPtr;
  for(Uint32 fragNo = 0; fragNo < tabPtr.p->totalfragments; fragNo++){
    jam();
    getFragstore(tabPtr.p, fragNo, fragPtr);    
    /**
     * For each of replica record
     */
    ReplicaRecordPtr replicaPtr;
    for(replicaPtr.i = fragPtr.p->oldStoredReplicas; replicaPtr.i != RNIL;
        replicaPtr.i = replicaPtr.p->nextReplica) {
      jam();
      ptrCheckGuard(replicaPtr, creplicaFileSize, replicaRecord);
      if(replicaPtr.p->procNode == nodeId){
        jam();
        /**
         * Found one with correct node id
         */
        /**
         * Invalidate all LCP's
         */
        modified = true;
        for(int i = 0; i < MAX_LCP_STORED; i++) {
          replicaPtr.p->lcpStatus[i] = ZINVALID;       
        }//if
        /**
         * And reset nextLcp
         */
        replicaPtr.p->nextLcp = 0;
        replicaPtr.p->noCrashedReplicas = 0;
      }//if
    }//for
  }//for

  if (modified) {
    jam();
    /**
     * Save table description to disk
     */
    tabPtr.p->tabCopyStatus  = TabRecord::CS_INVALIDATE_NODE_LCP;
    tabPtr.p->tabUpdateState = TabRecord::US_INVALIDATE_NODE_LCP;
    tabPtr.p->tabRemoveNode  = nodeId;
    signal->theData[0] = DihContinueB::ZPACK_TABLE_INTO_PAGES;
    signal->theData[1] = tabPtr.i;
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
    return;
  } 
  
  jam();
  /**
   * Move to next table
   */
  tabPtr.i++;
  signal->theData[0] = DihContinueB::ZINVALIDATE_NODE_LCP;
  signal->theData[1] = nodeId;
  signal->theData[2] = tabPtr.i;
  sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBB);
  return;
}//Dbdih::invalidateNodeLCP()

/*------------------------------------------------*/
/*       INPUT:  TABPTR                           */
/*               TNODEID                          */
/*------------------------------------------------*/
void Dbdih::removeNodeFromTables(Signal* signal, 
				 Uint32 nodeId, Uint32 tableId) 
{
  jamEntry();
  TabRecordPtr tabPtr;
  tabPtr.i = tableId;  
  const Uint32 RT_BREAK = 64;
  for (Uint32 i = 0; i<RT_BREAK; i++) {
    jam();
    if (tabPtr.i >= ctabFileSize){
      jam();
      removeNodeFromTablesComplete(signal, nodeId);
      return;
    }//if

    ptrAss(tabPtr, tabRecord);
    if (tabPtr.p->tabStatus == TabRecord::TS_ACTIVE) {
      jam();
      removeNodeFromTable(signal, nodeId, tabPtr);
      return;
    }//if
    tabPtr.i++;
  }//for
  signal->theData[0] = DihContinueB::ZREMOVE_NODE_FROM_TABLE;
  signal->theData[1] = nodeId;
  signal->theData[2] = tabPtr.i;
  sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBB);
}

void Dbdih::removeNodeFromTable(Signal* signal, 
				Uint32 nodeId, TabRecordPtr tabPtr){ 
  
  /**
   * Check so that no one else is using the tab descriptior
   */
  if (tabPtr.p->tabCopyStatus != TabRecord::CS_IDLE) {
    jam();    
    signal->theData[0] = DihContinueB::ZREMOVE_NODE_FROM_TABLE;
    signal->theData[1] = nodeId;
    signal->theData[2] = tabPtr.i;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 20, 3);
    return;
  }//if  

  /**
   * For each fragment
   */
  Uint32 noOfRemovedReplicas = 0;     // No of replicas removed
  Uint32 noOfRemovedLcpReplicas = 0;  // No of replicas in LCP removed 
  Uint32 noOfRemainingLcpReplicas = 0;// No of replicas in LCP remaining

  //const Uint32 lcpId = SYSFILE->latestLCP_ID;
  const bool lcpOngoingFlag = (tabPtr.p->tabLcpStatus== TabRecord::TLS_ACTIVE);
  
  FragmentstorePtr fragPtr;
  for(Uint32 fragNo = 0; fragNo < tabPtr.p->totalfragments; fragNo++){
    jam();
    getFragstore(tabPtr.p, fragNo, fragPtr);    
    
    /**
     * For each of replica record
     */
    Uint32 replicaNo = 0;
    ReplicaRecordPtr replicaPtr;
    for(replicaPtr.i = fragPtr.p->storedReplicas; replicaPtr.i != RNIL;
        replicaPtr.i = replicaPtr.p->nextReplica, replicaNo++) {
      jam();

      ptrCheckGuard(replicaPtr, creplicaFileSize, replicaRecord);
      if(replicaPtr.p->procNode == nodeId){
        jam();
	noOfRemovedReplicas++;
	removeNodeFromStored(nodeId, fragPtr, replicaPtr);
	if(replicaPtr.p->lcpOngoingFlag){
	  jam();
	  /**
	   * This replica is currently LCP:ed
	   */
	  ndbrequire(fragPtr.p->noLcpReplicas > 0);
	  fragPtr.p->noLcpReplicas --;
	  
	  noOfRemovedLcpReplicas ++;
	  replicaPtr.p->lcpOngoingFlag = false;
	}
      }
    }
    noOfRemainingLcpReplicas += fragPtr.p->noLcpReplicas;
  }
  
  if(noOfRemovedReplicas == 0){
    jam();
    /**
     * The table had no replica on the failed node
     *   continue with next table
     */
    tabPtr.i++;
    signal->theData[0] = DihContinueB::ZREMOVE_NODE_FROM_TABLE;
    signal->theData[1] = nodeId;
    signal->theData[2] = tabPtr.i;
    sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBB);
    return;
  }
  
  /**
   * We did remove at least one replica
   */
  bool ok = false;
  switch(tabPtr.p->tabLcpStatus){
  case TabRecord::TLS_COMPLETED:
    ok = true;
    jam();
    /**
     * WE WILL WRITE THE TABLE DESCRIPTION TO DISK AT THIS TIME 
     * INDEPENDENT OF WHAT THE LOCAL CHECKPOINT NEEDED. 
     * THIS IS TO ENSURE THAT THE FAILED NODES ARE ALSO UPDATED ON DISK 
     * IN THE DIH DATA STRUCTURES BEFORE WE COMPLETE HANDLING OF THE 
     * NODE FAILURE.
     */
    ndbrequire(noOfRemovedLcpReplicas == 0);
    
    tabPtr.p->tabCopyStatus = TabRecord::CS_REMOVE_NODE;
    tabPtr.p->tabUpdateState = TabRecord::US_REMOVE_NODE;
    tabPtr.p->tabRemoveNode = nodeId;
    signal->theData[0] = DihContinueB::ZPACK_TABLE_INTO_PAGES;
    signal->theData[1] = tabPtr.i;
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
    return;
    break;
  case TabRecord::TLS_ACTIVE:
    ok = true;
    jam();
    /**
     * The table is participating in an LCP currently
     */
    // Fall through
    break;
  case TabRecord::TLS_WRITING_TO_FILE:
    ok = true;
    jam();
    /**
     * This should never happen since we in the beginning of this function
     * checks the tabCopyStatus
     */
    ndbrequire(lcpOngoingFlag);
    ndbrequire(false);
    break;
  }    
  ndbrequire(ok);

  /**
   * The table is participating in an LCP currently
   *   and we removed some replicas that should have been checkpointed
   */
  ndbrequire(c_lcpState.lcpStatus != LCP_STATUS_IDLE);
  ndbrequire(tabPtr.p->tabLcpStatus == TabRecord::TLS_ACTIVE);
  
  /**
   * Save the table
   */
  tabPtr.p->tabCopyStatus = TabRecord::CS_REMOVE_NODE;
  tabPtr.p->tabUpdateState = TabRecord::US_REMOVE_NODE;
  tabPtr.p->tabRemoveNode = nodeId;
  signal->theData[0] = DihContinueB::ZPACK_TABLE_INTO_PAGES;
  signal->theData[1] = tabPtr.i;
  sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
  
  if(noOfRemainingLcpReplicas == 0){
    jam();
    /**
     * The removal on the failed node made the LCP complete
     */
    tabPtr.p->tabLcpStatus = TabRecord::TLS_WRITING_TO_FILE;
    checkLcpAllTablesDoneInLqh();
  }
}
  
void
Dbdih::removeNodeFromTablesComplete(Signal* signal, Uint32 nodeId){
  jam();

  /**
   * Check if we "accidently" completed a LCP
   */
  checkLcpCompletedLab(signal);
  
  /**
   * Check if we (DIH) are finished with node fail handling
   */
  checkLocalNodefailComplete(signal, nodeId, NF_REMOVE_NODE_FROM_TABLE);
}

void
Dbdih::checkLocalNodefailComplete(Signal* signal, Uint32 failedNodeId,
				  NodefailHandlingStep step){
  jam();

  NodeRecordPtr nodePtr;
  nodePtr.i = failedNodeId;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
  
  ndbrequire(nodePtr.p->m_nodefailSteps.get(step));
  nodePtr.p->m_nodefailSteps.clear(step);

  if(nodePtr.p->m_nodefailSteps.count() > 0){
    jam();
    return;
  }

  NFCompleteRep * const nf = (NFCompleteRep *)&signal->theData[0];
  nf->blockNo = DBDIH;
  nf->nodeId = cownNodeId;
  nf->failedNodeId = failedNodeId;
  nf->from = __LINE__;
  sendSignal(reference(), GSN_NF_COMPLETEREP, signal, 
             NFCompleteRep::SignalLength, JBB);
}


void
Dbdih::setLocalNodefailHandling(Signal* signal, Uint32 failedNodeId,
				NodefailHandlingStep step){
  jam();
  
  NodeRecordPtr nodePtr;
  nodePtr.i = failedNodeId;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
  
  ndbrequire(!nodePtr.p->m_nodefailSteps.get(step));
  nodePtr.p->m_nodefailSteps.set(step);
}

void Dbdih::startLcpTakeOverLab(Signal* signal, Uint32 failedNodeId)
{
  /*--------------------------------------------------------------------*/
  // Start LCP master take over process. Consists of the following steps.
  // 1) Ensure that all LQH's have reported all fragments they have been
  // told to checkpoint. Can be a fairly long step time-wise.
  // 2) Query all nodes about their LCP status.
  // During the query process we do not want our own state to change.
  // This can change due to delayed reception of LCP_REPORT, completed
  // save of table on disk or reception of DIH_LCPCOMPLETE from other
  // node.
  /*--------------------------------------------------------------------*/
}//Dbdih::startLcpTakeOver()

void Dbdih::execEMPTY_LCP_CONF(Signal* signal)
{
  jamEntry();
 
  ndbrequire(c_lcpMasterTakeOverState.state == LMTOS_WAIT_EMPTY_LCP);
  
  const EmptyLcpConf * const conf = (EmptyLcpConf *)&signal->theData[0];
  Uint32 nodeId = conf->senderNodeId;

  if(!conf->idle){
    jam();
    if (conf->tableId < c_lcpMasterTakeOverState.minTableId) {
      jam();
      c_lcpMasterTakeOverState.minTableId = conf->tableId;
      c_lcpMasterTakeOverState.minFragId = conf->fragmentId;
    } else if (conf->tableId == c_lcpMasterTakeOverState.minTableId &&
	       conf->fragmentId < c_lcpMasterTakeOverState.minFragId) {
      jam();
      c_lcpMasterTakeOverState.minFragId = conf->fragmentId;
    }//if
    if(isMaster()){
      jam();
      c_lcpState.m_LAST_LCP_FRAG_ORD.setWaitingFor(nodeId);    
    }
  }
  
  receiveLoopMacro(EMPTY_LCP_REQ, nodeId);
  /*--------------------------------------------------------------------*/
  // Received all EMPTY_LCPCONF. We can continue with next phase of the
  // take over LCP master process.
  /*--------------------------------------------------------------------*/
  c_lcpMasterTakeOverState.set(LMTOS_WAIT_LCP_FRAG_REP, __LINE__);
  checkEmptyLcpComplete(signal);
  return;
}//Dbdih::execEMPTY_LCPCONF()

void
Dbdih::checkEmptyLcpComplete(Signal *signal){
  
  ndbrequire(c_lcpMasterTakeOverState.state == LMTOS_WAIT_LCP_FRAG_REP);
  
  if(c_lcpState.noOfLcpFragRepOutstanding > 0){
    jam();
    return;
  }
  
  if(isMaster()){
    jam();

    signal->theData[0] = NDB_LE_LCP_TakeoverStarted;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 1, JBB);
    
    signal->theData[0] = 7012;
    execDUMP_STATE_ORD(signal);
    
    c_lcpMasterTakeOverState.set(LMTOS_INITIAL, __LINE__);
    MasterLCPReq * const req = (MasterLCPReq *)&signal->theData[0];
    req->masterRef = reference();
    req->failedNodeId = c_lcpMasterTakeOverState.failedNodeId;
    sendLoopMacro(MASTER_LCPREQ, sendMASTER_LCPREQ);
  } else {
    sendMASTER_LCPCONF(signal);
  }
}

/*--------------------------------------------------*/
/*       THE MASTER HAS FAILED AND THE NEW MASTER IS*/
/*       QUERYING THIS NODE ABOUT THE STATE OF THE  */
/*       LOCAL CHECKPOINT PROTOCOL.                 */
/*--------------------------------------------------*/
void Dbdih::execMASTER_LCPREQ(Signal* signal) 
{
  const MasterLCPReq * const req = (MasterLCPReq *)&signal->theData[0];
  jamEntry();
  const BlockReference newMasterBlockref = req->masterRef;

  Uint32 failedNodeId = req->failedNodeId;

  /**
   * There can be no take over with the same master
   */
  ndbrequire(c_lcpState.m_masterLcpDihRef != newMasterBlockref);
  c_lcpState.m_masterLcpDihRef = newMasterBlockref;
  c_lcpState.m_MASTER_LCPREQ_Received = true;
  c_lcpState.m_MASTER_LCPREQ_FailedNodeId = failedNodeId;
  
  if(newMasterBlockref != cmasterdihref){
    jam();
    ndbrequire(0);
  }
  
  sendMASTER_LCPCONF(signal);
}//Dbdih::execMASTER_LCPREQ()

void
Dbdih::sendMASTER_LCPCONF(Signal * signal){

  if(!c_EMPTY_LCP_REQ_Counter.done()){
    /**
     * Have not received all EMPTY_LCP_REP 
     * dare not answer MASTER_LCP_CONF yet
     */
    jam();
    return;
  }

  if(!c_lcpState.m_MASTER_LCPREQ_Received){
    jam();
    /**
     * Has not received MASTER_LCPREQ yet
     */
    return;
  }
  
  if(c_lcpState.lcpStatus == LCP_INIT_TABLES){
    jam();
    /**
     * Still aborting old initLcpLab
     */
    return;
  }

  if(c_lcpState.lcpStatus == LCP_COPY_GCI){
    jam();
    /**
     * Restart it
     */
    //Uint32 lcpId = SYSFILE->latestLCP_ID;
    SYSFILE->latestLCP_ID--;
    c_lcpState.setLcpStatus(LCP_STATUS_IDLE, __LINE__);
#if 0
    if(c_copyGCISlave.m_copyReason == CopyGCIReq::LOCAL_CHECKPOINT){
      ndbout_c("Dbdih: Also resetting c_copyGCISlave");
      c_copyGCISlave.m_copyReason = CopyGCIReq::IDLE;
      c_copyGCISlave.m_expectedNextWord = 0;
    }
#endif
  }

  bool ok = false;
  MasterLCPConf::State lcpState;
  switch (c_lcpState.lcpStatus) {
  case LCP_STATUS_IDLE:
    ok = true;
    jam();
    /*------------------------------------------------*/
    /*       LOCAL CHECKPOINT IS CURRENTLY NOT ACTIVE */
    /*       SINCE NO COPY OF RESTART INFORMATION HAVE*/
    /*       BEEN RECEIVED YET. ALSO THE PREVIOUS     */
    /*       CHECKPOINT HAVE BEEN FULLY COMPLETED.    */
    /*------------------------------------------------*/
    lcpState = MasterLCPConf::LCP_STATUS_IDLE;
    break;
  case LCP_STATUS_ACTIVE:
    ok = true;
    jam();
    /*--------------------------------------------------*/
    /*       COPY OF RESTART INFORMATION HAS BEEN       */
    /*       PERFORMED AND ALSO RESPONSE HAVE BEEN SENT.*/
    /*--------------------------------------------------*/
    lcpState = MasterLCPConf::LCP_STATUS_ACTIVE;
    break;
  case LCP_TAB_COMPLETED:
    ok = true;
    jam();
    /*--------------------------------------------------------*/
    /*       ALL LCP_REPORT'S HAVE BEEN COMPLETED FOR         */
    /*       ALL TABLES.     SAVE OF AT LEAST ONE TABLE IS    */
    /*       ONGOING YET.                                     */
    /*--------------------------------------------------------*/
    lcpState = MasterLCPConf::LCP_TAB_COMPLETED;
    break;
  case LCP_TAB_SAVED:
    ok = true;
    jam();
    /*--------------------------------------------------------*/
    /*       ALL LCP_REPORT'S HAVE BEEN COMPLETED FOR         */
    /*       ALL TABLES.     ALL TABLES HAVE ALSO BEEN SAVED  */
    /*       ALL OTHER NODES ARE NOT YET FINISHED WITH        */
    /*       THE LOCAL CHECKPOINT.                            */
    /*--------------------------------------------------------*/
    lcpState = MasterLCPConf::LCP_TAB_SAVED;
    break;
  case LCP_TCGET:
  case LCP_CALCULATE_KEEP_GCI:
  case LCP_TC_CLOPSIZE:
  case LCP_START_LCP_ROUND:
    /**
     * These should only exists on the master
     *   but since this is master take over
     *   it not allowed
     */
    ndbrequire(false);
    lcpState= MasterLCPConf::LCP_STATUS_IDLE; // remove warning
    break;
  case LCP_COPY_GCI:
  case LCP_INIT_TABLES:
    ok = true;
    /**
     * These two states are handled by if statements above
     */
    ndbrequire(false);
    lcpState= MasterLCPConf::LCP_STATUS_IDLE; // remove warning
    break;
  }//switch
  ndbrequire(ok);

  Uint32 failedNodeId = c_lcpState.m_MASTER_LCPREQ_FailedNodeId;
  MasterLCPConf * const conf = (MasterLCPConf *)&signal->theData[0];
  conf->senderNodeId = cownNodeId;
  conf->lcpState = lcpState;
  conf->failedNodeId = failedNodeId;
  sendSignal(c_lcpState.m_masterLcpDihRef, GSN_MASTER_LCPCONF,
             signal, MasterLCPConf::SignalLength, JBB);

  // Answer to MASTER_LCPREQ sent, reset flag so 
  // that it's not sent again before another request comes in
  c_lcpState.m_MASTER_LCPREQ_Received = false;

  if(c_lcpState.lcpStatus == LCP_TAB_SAVED){
#ifdef VM_TRACE
    ndbout_c("Sending extra GSN_LCP_COMPLETE_REP to new master");    
#endif
    sendLCP_COMPLETE_REP(signal);
  }

  if(!isMaster()){
    c_lcpMasterTakeOverState.set(LMTOS_IDLE, __LINE__);
    checkLocalNodefailComplete(signal, failedNodeId, NF_LCP_TAKE_OVER);
  }
  
  return;
}

NdbOut&
operator<<(NdbOut& out, const Dbdih::LcpMasterTakeOverState state){
  switch(state){
  case Dbdih::LMTOS_IDLE:
    out << "LMTOS_IDLE";
    break;
  case Dbdih::LMTOS_WAIT_EMPTY_LCP:
    out << "LMTOS_WAIT_EMPTY_LCP";
    break;
  case Dbdih::LMTOS_WAIT_LCP_FRAG_REP:
    out << "LMTOS_WAIT_EMPTY_LCP";
    break;
  case Dbdih::LMTOS_INITIAL:
    out << "LMTOS_INITIAL";
    break;
  case Dbdih::LMTOS_ALL_IDLE:
    out << "LMTOS_ALL_IDLE";
    break;
  case Dbdih::LMTOS_ALL_ACTIVE:
    out << "LMTOS_ALL_ACTIVE";
    break;
  case Dbdih::LMTOS_LCP_CONCLUDING:
    out << "LMTOS_LCP_CONCLUDING";
    break;
  case Dbdih::LMTOS_COPY_ONGOING:
    out << "LMTOS_COPY_ONGOING";
    break;
  }
  return out;
}

struct MASTERLCP_StateTransitions {
  Dbdih::LcpMasterTakeOverState CurrentState;
  MasterLCPConf::State ParticipantState;
  Dbdih::LcpMasterTakeOverState NewState;
};

static const
MASTERLCP_StateTransitions g_masterLCPTakeoverStateTransitions[] = {
  /**
   * Current = LMTOS_INITIAL
   */
  { Dbdih::LMTOS_INITIAL, 
    MasterLCPConf::LCP_STATUS_IDLE, 
    Dbdih::LMTOS_ALL_IDLE },
  
  { Dbdih::LMTOS_INITIAL, 
    MasterLCPConf::LCP_STATUS_ACTIVE,
    Dbdih::LMTOS_ALL_ACTIVE },

  { Dbdih::LMTOS_INITIAL, 
    MasterLCPConf::LCP_TAB_COMPLETED,
    Dbdih::LMTOS_LCP_CONCLUDING },

  { Dbdih::LMTOS_INITIAL, 
    MasterLCPConf::LCP_TAB_SAVED,
    Dbdih::LMTOS_LCP_CONCLUDING },

  /**
   * Current = LMTOS_ALL_IDLE
   */
  { Dbdih::LMTOS_ALL_IDLE,
    MasterLCPConf::LCP_STATUS_IDLE,
    Dbdih::LMTOS_ALL_IDLE },

  { Dbdih::LMTOS_ALL_IDLE,
    MasterLCPConf::LCP_STATUS_ACTIVE,
    Dbdih::LMTOS_COPY_ONGOING },

  { Dbdih::LMTOS_ALL_IDLE,
    MasterLCPConf::LCP_TAB_COMPLETED,
    Dbdih::LMTOS_LCP_CONCLUDING },

  { Dbdih::LMTOS_ALL_IDLE,
    MasterLCPConf::LCP_TAB_SAVED,
    Dbdih::LMTOS_LCP_CONCLUDING },

  /**
   * Current = LMTOS_COPY_ONGOING
   */
  { Dbdih::LMTOS_COPY_ONGOING,
    MasterLCPConf::LCP_STATUS_IDLE,
    Dbdih::LMTOS_COPY_ONGOING },

  { Dbdih::LMTOS_COPY_ONGOING,
    MasterLCPConf::LCP_STATUS_ACTIVE,
    Dbdih::LMTOS_COPY_ONGOING },
  
  /**
   * Current = LMTOS_ALL_ACTIVE
   */
  { Dbdih::LMTOS_ALL_ACTIVE,
    MasterLCPConf::LCP_STATUS_IDLE,
    Dbdih::LMTOS_COPY_ONGOING },

  { Dbdih::LMTOS_ALL_ACTIVE,
    MasterLCPConf::LCP_STATUS_ACTIVE,
    Dbdih::LMTOS_ALL_ACTIVE },

  { Dbdih::LMTOS_ALL_ACTIVE,
    MasterLCPConf::LCP_TAB_COMPLETED,
    Dbdih::LMTOS_LCP_CONCLUDING },    
  
  { Dbdih::LMTOS_ALL_ACTIVE,
    MasterLCPConf::LCP_TAB_SAVED,
    Dbdih::LMTOS_LCP_CONCLUDING },    

  /**
   * Current = LMTOS_LCP_CONCLUDING
   */
  { Dbdih::LMTOS_LCP_CONCLUDING,
    MasterLCPConf::LCP_STATUS_IDLE, 
    Dbdih::LMTOS_LCP_CONCLUDING },
  
  { Dbdih::LMTOS_LCP_CONCLUDING, 
    MasterLCPConf::LCP_STATUS_ACTIVE,
    Dbdih::LMTOS_LCP_CONCLUDING },

  { Dbdih::LMTOS_LCP_CONCLUDING, 
    MasterLCPConf::LCP_TAB_COMPLETED,
    Dbdih::LMTOS_LCP_CONCLUDING },

  { Dbdih::LMTOS_LCP_CONCLUDING, 
    MasterLCPConf::LCP_TAB_SAVED,
    Dbdih::LMTOS_LCP_CONCLUDING }
};

const Uint32 g_masterLCPTakeoverStateTransitionsRows = 
sizeof(g_masterLCPTakeoverStateTransitions) / sizeof(struct MASTERLCP_StateTransitions);

void Dbdih::execMASTER_LCPCONF(Signal* signal) 
{
  const MasterLCPConf * const conf = (MasterLCPConf *)&signal->theData[0];
  jamEntry();
  Uint32 senderNodeId = conf->senderNodeId;
  MasterLCPConf::State lcpState = (MasterLCPConf::State)conf->lcpState;
  const Uint32 failedNodeId = conf->failedNodeId;
  NodeRecordPtr nodePtr;
  nodePtr.i = senderNodeId;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
  nodePtr.p->lcpStateAtTakeOver = lcpState;

#ifdef VM_TRACE
  ndbout_c("MASTER_LCPCONF");
  printMASTER_LCP_CONF(stdout, &signal->theData[0], 0, 0);
#endif  

  bool found = false;
  for(Uint32 i = 0; i<g_masterLCPTakeoverStateTransitionsRows; i++){
    const struct MASTERLCP_StateTransitions * valid = 
      &g_masterLCPTakeoverStateTransitions[i];
    
    if(valid->CurrentState == c_lcpMasterTakeOverState.state && 
       valid->ParticipantState == lcpState){
      jam();
      found = true;
      c_lcpMasterTakeOverState.set(valid->NewState, __LINE__);
      break;
    }
  }
  ndbrequire(found);

  bool ok = false;
  switch(lcpState){
  case MasterLCPConf::LCP_STATUS_IDLE:
    ok = true;
    break;
  case MasterLCPConf::LCP_STATUS_ACTIVE:
  case MasterLCPConf::LCP_TAB_COMPLETED:
  case MasterLCPConf::LCP_TAB_SAVED:
    ok = true;
    c_lcpState.m_LCP_COMPLETE_REP_Counter_DIH.setWaitingFor(nodePtr.i);
    break;
  }
  ndbrequire(ok);

  receiveLoopMacro(MASTER_LCPREQ, senderNodeId);
  /*-------------------------------------------------------------------------*/
  // We have now received all responses and are ready to take over the LCP
  // protocol as master.
  /*-------------------------------------------------------------------------*/
  MASTER_LCPhandling(signal, failedNodeId);
}//Dbdih::execMASTER_LCPCONF()

void Dbdih::execMASTER_LCPREF(Signal* signal) 
{
  const MasterLCPRef * const ref = (MasterLCPRef *)&signal->theData[0];
  jamEntry();
  receiveLoopMacro(MASTER_LCPREQ, ref->senderNodeId);
  /*-------------------------------------------------------------------------*/
  // We have now received all responses and are ready to take over the LCP
  // protocol as master.
  /*-------------------------------------------------------------------------*/
  MASTER_LCPhandling(signal, ref->failedNodeId);
}//Dbdih::execMASTER_LCPREF()

void Dbdih::MASTER_LCPhandling(Signal* signal, Uint32 failedNodeId) 
{
  /*-------------------------------------------------------------------------
   *
   * WE ARE NOW READY TO CONCLUDE THE TAKE OVER AS MASTER. 
   * WE HAVE ENOUGH INFO TO START UP ACTIVITIES IN THE PROPER PLACE. 
   * ALSO SET THE PROPER STATE VARIABLES.
   *------------------------------------------------------------------------*/
  c_lcpState.currentFragment.tableId = c_lcpMasterTakeOverState.minTableId;
  c_lcpState.currentFragment.fragmentId = c_lcpMasterTakeOverState.minFragId;
  c_lcpState.m_LAST_LCP_FRAG_ORD = c_lcpState.m_LCP_COMPLETE_REP_Counter_LQH;

  NodeRecordPtr failedNodePtr;  
  failedNodePtr.i = failedNodeId;
  ptrCheckGuard(failedNodePtr, MAX_NDB_NODES, nodeRecord);

  switch (c_lcpMasterTakeOverState.state) {
  case LMTOS_ALL_IDLE:
    jam();
    /* --------------------------------------------------------------------- */
    // All nodes were idle in the LCP protocol. Start checking for start of LCP
    // protocol.
    /* --------------------------------------------------------------------- */
#ifdef VM_TRACE
    ndbout_c("MASTER_LCPhandling:: LMTOS_ALL_IDLE -> checkLcpStart");
#endif
    checkLcpStart(signal, __LINE__);
    break;
  case LMTOS_COPY_ONGOING:
    jam();
    /* --------------------------------------------------------------------- */
    // We were in the starting process of the LCP protocol. We will restart the
    // protocol by calculating the keep gci and storing the new lcp id.
    /* --------------------------------------------------------------------- */
#ifdef VM_TRACE
    ndbout_c("MASTER_LCPhandling:: LMTOS_COPY_ONGOING -> storeNewLcpId");
#endif
    if (c_lcpState.lcpStatus == LCP_STATUS_ACTIVE) {
      jam();
      /*---------------------------------------------------------------------*/
      /*  WE NEED TO DECREASE THE LATEST LCP ID SINCE WE HAVE ALREADY        */
      /*  STARTED THIS */
      /*  LOCAL CHECKPOINT.                                                  */
      /*---------------------------------------------------------------------*/
      Uint32 lcpId = SYSFILE->latestLCP_ID;
#ifdef VM_TRACE
      ndbout_c("Decreasing latestLCP_ID from %d to %d", lcpId, lcpId - 1);
#endif
      SYSFILE->latestLCP_ID--;
    }//if
    storeNewLcpIdLab(signal);
    break;
  case LMTOS_ALL_ACTIVE:
    {
      jam();
      /* ------------------------------------------------------------------- 
       * Everybody was in the active phase. We will restart sending 
       * LCP_FRAGORD to the nodes from the new master. 
       * We also need to set dihLcpStatus to ZACTIVE
       * in the master node since the master will wait for all nodes to 
       * complete before finalising the LCP process.
       * ------------------------------------------------------------------ */
#ifdef VM_TRACE
      ndbout_c("MASTER_LCPhandling:: LMTOS_ALL_ACTIVE -> "
	       "startLcpRoundLoopLab(table=%u, fragment=%u)",
	       c_lcpMasterTakeOverState.minTableId, 
	       c_lcpMasterTakeOverState.minFragId);
#endif
    
      c_lcpState.keepGci = SYSFILE->keepGCI;
      c_lcpState.setLcpStatus(LCP_START_LCP_ROUND, __LINE__);
      startLcpRoundLoopLab(signal, 0, 0);
      break;
    }
  case LMTOS_LCP_CONCLUDING:
    {
      jam();
      /* ------------------------------------------------------------------- */
      // The LCP process is in the finalisation phase. We simply wait for it to
      // complete with signals arriving in. We need to check also if we should
      // change state due to table write completion during state 
      // collection phase.
      /* ------------------------------------------------------------------- */
      ndbrequire(c_lcpState.lcpStatus != LCP_STATUS_IDLE);
      startLcpRoundLoopLab(signal, 0, 0);
      break;
    }
  default:
    ndbrequire(false);
    break;
  }//switch
  signal->theData[0] = NDB_LE_LCP_TakeoverCompleted;
  signal->theData[1] = c_lcpMasterTakeOverState.state;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);
  
  signal->theData[0] = 7012;
  execDUMP_STATE_ORD(signal);

  signal->theData[0] = 7015;
  execDUMP_STATE_ORD(signal);

  c_lcpMasterTakeOverState.set(LMTOS_IDLE, __LINE__);

  checkLocalNodefailComplete(signal, failedNodePtr.i, NF_LCP_TAKE_OVER);
}

/* ------------------------------------------------------------------------- */
/*       A BLOCK OR A NODE HAS COMPLETED THE HANDLING OF THE NODE FAILURE.   */
/* ------------------------------------------------------------------------- */
void Dbdih::execNF_COMPLETEREP(Signal* signal) 
{
  NodeRecordPtr failedNodePtr;
  NFCompleteRep * const nfCompleteRep = (NFCompleteRep *)&signal->theData[0];
  jamEntry();
  const Uint32 blockNo = nfCompleteRep->blockNo;
  Uint32 nodeId       = nfCompleteRep->nodeId;
  failedNodePtr.i = nfCompleteRep->failedNodeId;

  ptrCheckGuard(failedNodePtr, MAX_NDB_NODES, nodeRecord);
  switch (blockNo) {
  case DBTC:
    jam();
    ndbrequire(failedNodePtr.p->dbtcFailCompleted == ZFALSE);
    /* -------------------------------------------------------------------- */
    // Report the event that DBTC completed node failure handling.
    /* -------------------------------------------------------------------- */
    signal->theData[0] = NDB_LE_NodeFailCompleted;
    signal->theData[1] = DBTC;
    signal->theData[2] = failedNodePtr.i;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 3, JBB);

    failedNodePtr.p->dbtcFailCompleted = ZTRUE;
    break;
  case DBDICT:
    jam();
    ndbrequire(failedNodePtr.p->dbdictFailCompleted == ZFALSE);
    /* --------------------------------------------------------------------- */
    // Report the event that DBDICT completed node failure handling.
    /* --------------------------------------------------------------------- */
    signal->theData[0] = NDB_LE_NodeFailCompleted;
    signal->theData[1] = DBDICT;
    signal->theData[2] = failedNodePtr.i;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 3, JBB);

    failedNodePtr.p->dbdictFailCompleted = ZTRUE;
    break;
  case DBDIH:
    jam();
    ndbrequire(failedNodePtr.p->dbdihFailCompleted == ZFALSE);
    /* --------------------------------------------------------------------- */
    // Report the event that DBDIH completed node failure handling.
    /* --------------------------------------------------------------------- */
    signal->theData[0] = NDB_LE_NodeFailCompleted;
    signal->theData[1] = DBDIH;
    signal->theData[2] = failedNodePtr.i;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 3, JBB);

    failedNodePtr.p->dbdihFailCompleted = ZTRUE;
    break;
  case DBLQH:
    jam();
    ndbrequire(failedNodePtr.p->dblqhFailCompleted == ZFALSE);
    /* --------------------------------------------------------------------- */
    // Report the event that DBDIH completed node failure handling.
    /* --------------------------------------------------------------------- */
    signal->theData[0] = NDB_LE_NodeFailCompleted;
    signal->theData[1] = DBLQH;
    signal->theData[2] = failedNodePtr.i;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 3, JBB);

    failedNodePtr.p->dblqhFailCompleted = ZTRUE;
    break;
  case 0: /* Node has finished */
    jam();
    ndbrequire(nodeId < MAX_NDB_NODES);

    if (failedNodePtr.p->recNODE_FAILREP == ZFALSE) {
      jam();
      /* ------------------------------------------------------------------- */
      // We received a report about completion of node failure before we 
      // received the message about the NODE failure ourselves. 
      // We will send the signal to ourselves with a small delay 
      // (10 milliseconds).
      /* ------------------------------------------------------------------- */
      //nf->from = __LINE__;
      sendSignalWithDelay(reference(), GSN_NF_COMPLETEREP, signal, 10,
			  signal->length());
      return;
    }//if
    
    if (!failedNodePtr.p->m_NF_COMPLETE_REP.isWaitingFor(nodeId)){
      jam();
      return;
    }
      
    failedNodePtr.p->m_NF_COMPLETE_REP.clearWaitingFor(nodeId);;
    
    /* -------------------------------------------------------------------- */
    // Report the event that nodeId has completed node failure handling.
    /* -------------------------------------------------------------------- */
    signal->theData[0] = NDB_LE_NodeFailCompleted;
    signal->theData[1] = 0;
    signal->theData[2] = failedNodePtr.i;
    signal->theData[3] = nodeId;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 4, JBB);
    
    nodeFailCompletedCheckLab(signal, failedNodePtr);
    return;
    break;
  default:
    ndbrequire(false);
    return;
    break;
  }//switch
  if (failedNodePtr.p->dbtcFailCompleted == ZFALSE) {
    jam();
    return;
  }//if
  if (failedNodePtr.p->dbdictFailCompleted == ZFALSE) {
    jam();
    return;
  }//if
  if (failedNodePtr.p->dbdihFailCompleted == ZFALSE) {
    jam();
    return;
  }//if
  if (failedNodePtr.p->dblqhFailCompleted == ZFALSE) {
    jam();
    return;
  }//if
  /* ----------------------------------------------------------------------- */
  /*     ALL BLOCKS IN THIS NODE HAVE COMPLETED THEIR PART OF HANDLING THE   */
  /*     NODE FAILURE. WE CAN NOW REPORT THIS COMPLETION TO ALL OTHER NODES. */
  /* ----------------------------------------------------------------------- */
  NodeRecordPtr nodePtr;
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    jam();
    ptrAss(nodePtr, nodeRecord);
    if (nodePtr.p->nodeStatus == NodeRecord::ALIVE) {
      jam();
      BlockReference ref = calcDihBlockRef(nodePtr.i);
      NFCompleteRep * const nf = (NFCompleteRep *)&signal->theData[0];
      nf->blockNo      = 0;
      nf->nodeId       = cownNodeId;
      nf->failedNodeId = failedNodePtr.i;
      nf->from = __LINE__;
      sendSignal(ref, GSN_NF_COMPLETEREP, signal, 
                 NFCompleteRep::SignalLength, JBB);
    }//if
  }//for
  return;
}//Dbdih::execNF_COMPLETEREP()

void Dbdih::nodeFailCompletedCheckLab(Signal* signal, 
				      NodeRecordPtr failedNodePtr) 
{
  jam();
  if (!failedNodePtr.p->m_NF_COMPLETE_REP.done()){
    jam(); 
    return;
  }//if
  /* ---------------------------------------------------------------------- */
  /*    ALL BLOCKS IN ALL NODES HAVE NOW REPORTED COMPLETION OF THE NODE    */
  /*    FAILURE HANDLING. WE ARE NOW READY TO ACCEPT THAT THIS NODE STARTS  */
  /*    AGAIN.                                                              */
  /* ---------------------------------------------------------------------- */
  jam();
  failedNodePtr.p->nodeStatus = NodeRecord::DEAD;
  failedNodePtr.p->recNODE_FAILREP = ZFALSE;
  
  /* ---------------------------------------------------------------------- */
  // Report the event that all nodes completed node failure handling.
  /* ---------------------------------------------------------------------- */
  signal->theData[0] = NDB_LE_NodeFailCompleted;
  signal->theData[1] = 0;
  signal->theData[2] = failedNodePtr.i;
  signal->theData[3] = 0;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 4, JBB);

  /* ---------------------------------------------------------------------- */
  // Report to QMGR that we have concluded recovery handling of this node.
  /* ---------------------------------------------------------------------- */
  signal->theData[0] = failedNodePtr.i;
  sendSignal(QMGR_REF, GSN_NDB_FAILCONF, signal, 1, JBB);
  
  if (isMaster()) {
    jam();
    /* --------------------------------------------------------------------- */
    /*   IF WE ARE MASTER WE MUST CHECK IF COPY FRAGMENT WAS INTERRUPTED     */
    /*   BY THE FAILED NODES.                                                */
    /* --------------------------------------------------------------------- */
    TakeOverRecordPtr takeOverPtr;
    takeOverPtr.i = 0;
    ptrAss(takeOverPtr, takeOverRecord);
    if ((takeOverPtr.p->toMasterStatus == TakeOverRecord::COPY_FRAG) &&
        (failedNodePtr.i == takeOverPtr.p->toCopyNode)) {
      jam();
#ifdef VM_TRACE
      ndbrequire("Tell jonas" == 0);
#endif
      /*------------------------------------------------------------------*/
      /*       WE ARE CURRENTLY IN THE PROCESS OF COPYING A FRAGMENT. WE  */
      /*       WILL CHECK IF THE COPY NODE HAVE FAILED.                   */
      /*------------------------------------------------------------------*/
      takeOverPtr.p->toMasterStatus = TakeOverRecord::SELECTING_NEXT;
      startNextCopyFragment(signal, takeOverPtr.i);
      return;
    }//if
    checkStartTakeOver(signal);
  }//if
  return;
}//Dbdih::nodeFailCompletedCheckLab()

/*****************************************************************************/
/* **********     SEIZING / RELEASING MODULE                     *************/
/*****************************************************************************/
/*
  3.4   L O C A L  N O D E   S E I Z E  
  ************************************
  */
/*
  3.4.1   L O C A L  N O D E   S E I Z E   R E Q U E S T
  ******************************************************
  */
void Dbdih::execDISEIZEREQ(Signal* signal) 
{
  ConnectRecordPtr connectPtr;
  jamEntry();
  Uint32 userPtr = signal->theData[0];
  BlockReference userRef = signal->theData[1];
  ndbrequire(cfirstconnect != RNIL);
  connectPtr.i = cfirstconnect;
  ptrCheckGuard(connectPtr, cconnectFileSize, connectRecord);
  cfirstconnect = connectPtr.p->nfConnect;
  connectPtr.p->nfConnect = RNIL;
  connectPtr.p->userpointer = userPtr;
  connectPtr.p->userblockref = userRef;
  connectPtr.p->connectState = ConnectRecord::INUSE;
  signal->theData[0] = connectPtr.p->userpointer;
  signal->theData[1] = connectPtr.i;
  sendSignal(userRef, GSN_DISEIZECONF, signal, 2, JBB);
}//Dbdih::execDISEIZEREQ()

/*
  3.5   L O C A L  N O D E   R E L E A S E
  ****************************************
  */
/*
  3.5.1   L O C A L  N O D E   R E L E A S E   R E Q U E S T
  *******************************************************=
  */
void Dbdih::execDIRELEASEREQ(Signal* signal) 
{
  ConnectRecordPtr connectPtr;
  jamEntry();
  connectPtr.i = signal->theData[0];
  Uint32 userRef = signal->theData[2];
  ptrCheckGuard(connectPtr, cconnectFileSize, connectRecord);
  ndbrequire(connectPtr.p->connectState != ConnectRecord::FREE);
  ndbrequire(connectPtr.p->userblockref == userRef);
  signal->theData[0] = connectPtr.p->userpointer;
  sendSignal(connectPtr.p->userblockref, GSN_DIRELEASECONF, signal, 1, JBB);
  release_connect(connectPtr);
}//Dbdih::execDIRELEASEREQ()

/*
  3.7   A D D   T A B L E
  **********************=
  */
/*****************************************************************************/
/* **********     TABLE ADDING MODULE                            *************/
/*****************************************************************************/
/*
  3.7.1   A D D   T A B L E   M A I N L Y
  ***************************************
  */
void Dbdih::execCREATE_FRAGMENTATION_REQ(Signal * signal){
  jamEntry();
  CreateFragmentationReq * const req = 
    (CreateFragmentationReq*)signal->getDataPtr();
  
  const Uint32 senderRef = req->senderRef;
  const Uint32 senderData = req->senderData;
  const Uint32 fragmentNode = req->fragmentNode;
  const Uint32 fragmentType = req->fragmentationType;
  //const Uint32 fragmentCount = req->noOfFragments;
  const Uint32 primaryTableId = req->primaryTableId;

  Uint32 err = 0;
  
  do {
    Uint32 noOfFragments = 0;
    Uint32 noOfReplicas = cnoReplicas;
    switch(fragmentType){
    case DictTabInfo::AllNodesSmallTable:
      jam();
      noOfFragments = csystemnodes;
      break;
    case DictTabInfo::AllNodesMediumTable:
      jam();
      noOfFragments = 2 * csystemnodes;
      break;
    case DictTabInfo::AllNodesLargeTable:
      jam();
      noOfFragments = 4 * csystemnodes;
      break;
    case DictTabInfo::SingleFragment:
      jam();
      noOfFragments = 1;
      break;
#if 0
    case DictTabInfo::SpecifiedFragmentCount:
      noOfFragments = (fragmentCount == 0 ? 1 : (fragmentCount + 1)/ 2);
      break;
#endif
    default:
      jam();
      err = CreateFragmentationRef::InvalidFragmentationType;
      break;
    }
    if(err)
      break;
   
    NodeGroupRecordPtr NGPtr;
    TabRecordPtr primTabPtr;
    if (primaryTableId == RNIL) {
      if(fragmentNode == 0){
        jam();
        NGPtr.i = 0; 
	if(noOfFragments < csystemnodes)
	{
	  NGPtr.i = c_nextNodeGroup; 
	  c_nextNodeGroup = (NGPtr.i + 1 == cnoOfNodeGroups ? 0 : NGPtr.i + 1);
	}
      } else if(! (fragmentNode < MAX_NDB_NODES)) {
        jam();
        err = CreateFragmentationRef::InvalidNodeId;
      } else {
        jam();
        const Uint32 stat = Sysfile::getNodeStatus(fragmentNode,
                                                   SYSFILE->nodeStatus);
        switch (stat) {
        case Sysfile::NS_Active:
        case Sysfile::NS_ActiveMissed_1:
        case Sysfile::NS_ActiveMissed_2:
        case Sysfile::NS_TakeOver:
          jam();
          break;
        case Sysfile::NS_NotActive_NotTakenOver:
          jam();
          break;
        case Sysfile::NS_HotSpare:
          jam();
        case Sysfile::NS_NotDefined:
          jam();
        default:
          jam();
          err = CreateFragmentationRef::InvalidNodeType;
          break;
        }
        if(err)
          break;
        NGPtr.i = Sysfile::getNodeGroup(fragmentNode,
                                        SYSFILE->nodeGroups);
        break;
      }
    } else {
      if (primaryTableId >= ctabFileSize) {
        jam();
        err = CreateFragmentationRef::InvalidPrimaryTable;
        break;
      }
      primTabPtr.i = primaryTableId;
      ptrAss(primTabPtr, tabRecord);
      if (primTabPtr.p->tabStatus != TabRecord::TS_ACTIVE) {
        jam();
        err = CreateFragmentationRef::InvalidPrimaryTable;
        break;
      }
      if (noOfFragments != primTabPtr.p->totalfragments) {
        jam();
        err = CreateFragmentationRef::InvalidFragmentationType;
        break;
      }
    }
    
    Uint32 count = 2;
    Uint16 *fragments = (Uint16*)(signal->theData+25);
    if (primaryTableId == RNIL) {
      jam();
      Uint8 next_replica_node[MAX_NDB_NODES];
      memset(next_replica_node,0,sizeof(next_replica_node));
      for(Uint32 fragNo = 0; fragNo<noOfFragments; fragNo++){
        jam();
        ptrCheckGuard(NGPtr, MAX_NDB_NODES, nodeGroupRecord);      
        const Uint32 max = NGPtr.p->nodeCount;
	
	Uint32 tmp= next_replica_node[NGPtr.i];
        for(Uint32 replicaNo = 0; replicaNo<noOfReplicas; replicaNo++)
        {
          jam();
          const Uint32 nodeId = NGPtr.p->nodesInGroup[tmp++];
          fragments[count++] = nodeId;
          tmp = (tmp >= max ? 0 : tmp);
        }
	tmp++;
	next_replica_node[NGPtr.i]= (tmp >= max ? 0 : tmp);
	
        /**
         * Next node group for next fragment
         */
        NGPtr.i++;
        NGPtr.i = (NGPtr.i == cnoOfNodeGroups ? 0 : NGPtr.i);
      }
    } else {
      for (Uint32 fragNo = 0;
           fragNo < primTabPtr.p->totalfragments; fragNo++) {
        jam();
        FragmentstorePtr fragPtr;
        ReplicaRecordPtr replicaPtr;
        getFragstore(primTabPtr.p, fragNo, fragPtr);
        fragments[count++] = fragPtr.p->preferredPrimary;
        for (replicaPtr.i = fragPtr.p->storedReplicas;
             replicaPtr.i != RNIL;
             replicaPtr.i = replicaPtr.p->nextReplica) {
          jam();
          ptrCheckGuard(replicaPtr, creplicaFileSize, replicaRecord);
          if (replicaPtr.p->procNode != fragPtr.p->preferredPrimary) {
            jam();
            fragments[count++] = replicaPtr.p->procNode;
          }//if
        }//for
        for (replicaPtr.i = fragPtr.p->oldStoredReplicas;
             replicaPtr.i != RNIL;
             replicaPtr.i = replicaPtr.p->nextReplica) {
          jam();
          ptrCheckGuard(replicaPtr, creplicaFileSize, replicaRecord);
          if (replicaPtr.p->procNode != fragPtr.p->preferredPrimary) {
            jam();
            fragments[count++] = replicaPtr.p->procNode;
          }//if
        }//for
      }
    }
    ndbrequire(count == (2 + noOfReplicas * noOfFragments)); 
    
    CreateFragmentationConf * const conf = 
      (CreateFragmentationConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = senderData;
    conf->noOfReplicas = noOfReplicas;
    conf->noOfFragments = noOfFragments;

    fragments[0] = noOfReplicas;
    fragments[1] = noOfFragments;
    
    if(senderRef != 0)
    {
      LinearSectionPtr ptr[3];
      ptr[0].p = (Uint32*)&fragments[0];
      ptr[0].sz = (count + 1) / 2;
      sendSignal(senderRef,
		 GSN_CREATE_FRAGMENTATION_CONF,
		 signal, 
		 CreateFragmentationConf::SignalLength,
		 JBB,
		 ptr,
		 1);
    }
    else
    {
      // Execute direct
      signal->theData[0] = 0;
    }
    return;
  } while(false);

  if(senderRef != 0)
  {
    CreateFragmentationRef * const ref = 
      (CreateFragmentationRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = senderData;
    ref->errorCode = err;
    sendSignal(senderRef, GSN_CREATE_FRAGMENTATION_REF, signal, 
	       CreateFragmentationRef::SignalLength, JBB);
  }
  else
  {
    // Execute direct
    signal->theData[0] = err;
  }
}

void Dbdih::execDIADDTABREQ(Signal* signal) 
{
  jamEntry();

  DiAddTabReq * const req = (DiAddTabReq*)signal->getDataPtr();

  // Seize connect record
  ndbrequire(cfirstconnect != RNIL);
  ConnectRecordPtr connectPtr;
  connectPtr.i = cfirstconnect;
  ptrCheckGuard(connectPtr, cconnectFileSize, connectRecord);
  cfirstconnect = connectPtr.p->nfConnect;
  
  const Uint32 userPtr = req->connectPtr;
  const BlockReference userRef = signal->getSendersBlockRef();
  connectPtr.p->nfConnect = RNIL;
  connectPtr.p->userpointer = userPtr;
  connectPtr.p->userblockref = userRef;
  connectPtr.p->connectState = ConnectRecord::INUSE;
  connectPtr.p->table = req->tableId;
  
  TabRecordPtr tabPtr;
  tabPtr.i = req->tableId;
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
  tabPtr.p->connectrec = connectPtr.i;
  tabPtr.p->tableType = req->tableType;
  tabPtr.p->schemaVersion = req->schemaVersion;
  tabPtr.p->primaryTableId = req->primaryTableId;

  if(tabPtr.p->tabStatus == TabRecord::TS_ACTIVE){
    jam();
    tabPtr.p->tabStatus = TabRecord::TS_CREATING;
    sendAddFragreq(signal, connectPtr, tabPtr, 0);
    return;
  }

  if(getNodeState().getSystemRestartInProgress() &&
     tabPtr.p->tabStatus == TabRecord::TS_IDLE){
    jam();
    
    ndbrequire(cmasterNodeId == getOwnNodeId());
    tabPtr.p->tabStatus = TabRecord::TS_CREATING;
    
    initTableFile(tabPtr);
    FileRecordPtr filePtr;
    filePtr.i = tabPtr.p->tabFile[0];
    ptrCheckGuard(filePtr, cfileFileSize, fileRecord);
    openFileRw(signal, filePtr);
    filePtr.p->reqStatus = FileRecord::OPENING_TABLE;
    return;
  }

  /*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/
  /* AT THE TIME OF INITIATING THE FILE OF TABLE         */
  /* DESCRIPTION IS CREATED FOR APPROPRIATE SIZE. EACH   */
  /* EACH RECORD IN THIS FILE HAS THE INFORMATION ABOUT  */
  /* ONE TABLE. THE POINTER TO THIS RECORD IS THE TABLE  */
  /* REFERENCE. IN THE BEGINNING ALL RECORDS ARE CREATED */
  /* BUT THEY DO NOT HAVE ANY INFORMATION ABOUT ANY TABLE*/
  /*%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%*/
  tabPtr.p->tabStatus = TabRecord::TS_CREATING;
  tabPtr.p->storedTable = req->storedTable;
  tabPtr.p->method = TabRecord::HASH;
  tabPtr.p->kvalue = req->kValue;

  union {
    Uint16 fragments[2 + MAX_FRAG_PER_NODE*MAX_REPLICAS*MAX_NDB_NODES];
    Uint32 align;
  };
  SegmentedSectionPtr fragDataPtr;
  signal->getSection(fragDataPtr, DiAddTabReq::FRAGMENTATION);
  copy((Uint32*)fragments, fragDataPtr);
  releaseSections(signal);
  
  const Uint32 noReplicas = fragments[0];
  const Uint32 noFragments = fragments[1];

  tabPtr.p->noOfBackups = noReplicas - 1;
  tabPtr.p->totalfragments = noFragments;
  ndbrequire(noReplicas == cnoReplicas); // Only allowed

  if (ERROR_INSERTED(7173)) {
    CLEAR_ERROR_INSERT_VALUE;
    addtabrefuseLab(signal, connectPtr, ZREPLERROR1);
    return;
  }
  if ((noReplicas * noFragments) > cnoFreeReplicaRec) {
    jam();
    addtabrefuseLab(signal, connectPtr, ZREPLERROR1);
    return;
  }//if
  if (noFragments > cremainingfrags) {
    jam();
    addtabrefuseLab(signal, connectPtr, ZREPLERROR1);
    return;
  }//if
  
  Uint32 logTotalFragments = 1;
  while (logTotalFragments <= tabPtr.p->totalfragments) {
    jam();
    logTotalFragments <<= 1;
  }
  logTotalFragments >>= 1;
  tabPtr.p->mask = logTotalFragments - 1;
  tabPtr.p->hashpointer = tabPtr.p->totalfragments - logTotalFragments;
  allocFragments(tabPtr.p->totalfragments, tabPtr);  

  Uint32 index = 2;
  for (Uint32 fragId = 0; fragId < noFragments; fragId++) {
    jam();
    FragmentstorePtr fragPtr;
    Uint32 activeIndex = 0;
    getFragstore(tabPtr.p, fragId, fragPtr);
    fragPtr.p->preferredPrimary = fragments[index];
    for (Uint32 i = 0; i<noReplicas; i++) {
      const Uint32 nodeId = fragments[index++];
      ReplicaRecordPtr replicaPtr;
      allocStoredReplica(fragPtr, replicaPtr, nodeId);
      if (getNodeStatus(nodeId) == NodeRecord::ALIVE) {
        jam();
        ndbrequire(activeIndex < MAX_REPLICAS);
        fragPtr.p->activeNodes[activeIndex] = nodeId;
        activeIndex++;
      } else {
        jam();
        removeStoredReplica(fragPtr, replicaPtr);
        linkOldStoredReplica(fragPtr, replicaPtr);
      }//if
    }//for
    fragPtr.p->fragReplicas = activeIndex;
    ndbrequire(activeIndex > 0 && fragPtr.p->storedReplicas != RNIL);
  }
  initTableFile(tabPtr);
  tabPtr.p->tabCopyStatus = TabRecord::CS_ADD_TABLE_MASTER;
  signal->theData[0] = DihContinueB::ZPACK_TABLE_INTO_PAGES;
  signal->theData[1] = tabPtr.i;
  sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
}

void
Dbdih::addTable_closeConf(Signal * signal, Uint32 tabPtrI){
  TabRecordPtr tabPtr;
  tabPtr.i = tabPtrI;
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);

  ConnectRecordPtr connectPtr;
  connectPtr.i = tabPtr.p->connectrec;
  ptrCheckGuard(connectPtr, cconnectFileSize, connectRecord);
  
  sendAddFragreq(signal, connectPtr, tabPtr, 0);
}

void
Dbdih::sendAddFragreq(Signal* signal, ConnectRecordPtr connectPtr, 
		      TabRecordPtr tabPtr, Uint32 fragId){
  jam();
  const Uint32 fragCount = tabPtr.p->totalfragments;
  ReplicaRecordPtr replicaPtr; replicaPtr.i = RNIL;
  for(; fragId<fragCount; fragId++){
    jam();
    FragmentstorePtr fragPtr;
    getFragstore(tabPtr.p, fragId, fragPtr);    
    
    replicaPtr.i = fragPtr.p->storedReplicas;
    while(replicaPtr.i != RNIL){
      jam();
      ptrCheckGuard(replicaPtr, creplicaFileSize, replicaRecord);      
      if(replicaPtr.p->procNode == getOwnNodeId()){
	break;
      }
      replicaPtr.i = replicaPtr.p->nextReplica;
    }
    
    if(replicaPtr.i != RNIL){
      jam();
      break;
    }
    
    replicaPtr.i = fragPtr.p->oldStoredReplicas;
    while(replicaPtr.i != RNIL){
      jam();
      ptrCheckGuard(replicaPtr, creplicaFileSize, replicaRecord);      
      if(replicaPtr.p->procNode == getOwnNodeId()){
	break;
      }
      replicaPtr.i = replicaPtr.p->nextReplica;
    }

    if(replicaPtr.i != RNIL){
      jam();
      break;
    }
  }
  
  if(replicaPtr.i != RNIL){
    jam();
    ndbrequire(fragId < fragCount);
    ndbrequire(replicaPtr.p->procNode == getOwnNodeId());

    Uint32 requestInfo = 0;
    if(!tabPtr.p->storedTable){
      requestInfo |= LqhFragReq::TemporaryTable;
    }
    
    if(getNodeState().getNodeRestartInProgress()){
      requestInfo |= LqhFragReq::CreateInRunning;
    }
    
    AddFragReq* const req = (AddFragReq*)signal->getDataPtr();
    req->dihPtr = connectPtr.i;
    req->senderData = connectPtr.p->userpointer;
    req->fragmentId = fragId;
    req->requestInfo = requestInfo;
    req->tableId = tabPtr.i;
    req->nextLCP = 0;
    req->nodeId = getOwnNodeId();
    req->totalFragments = fragCount;
    req->startGci = SYSFILE->newestRestorableGCI;
    sendSignal(DBDICT_REF, GSN_ADD_FRAGREQ, signal, 
	       AddFragReq::SignalLength, JBB);
    return;
  }
  
  // Done
  DiAddTabConf * const conf = (DiAddTabConf*)signal->getDataPtr();
  conf->senderData = connectPtr.p->userpointer;
  sendSignal(connectPtr.p->userblockref, GSN_DIADDTABCONF, signal, 
	     DiAddTabConf::SignalLength, JBB);  

  // Release
  release_connect(connectPtr);
}
void
Dbdih::release_connect(ConnectRecordPtr ptr)
{
  ptr.p->userblockref = ZNIL;
  ptr.p->userpointer = RNIL;
  ptr.p->connectState = ConnectRecord::FREE;
  ptr.p->nfConnect = cfirstconnect;
  cfirstconnect = ptr.i;
}

void
Dbdih::execADD_FRAGCONF(Signal* signal){
  jamEntry();
  AddFragConf * const conf = (AddFragConf*)signal->getDataPtr();

  ConnectRecordPtr connectPtr;
  connectPtr.i = conf->dihPtr;
  ptrCheckGuard(connectPtr, cconnectFileSize, connectRecord);

  TabRecordPtr tabPtr;
  tabPtr.i = connectPtr.p->table;
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);

  sendAddFragreq(signal, connectPtr, tabPtr, conf->fragId + 1);
}

void
Dbdih::execADD_FRAGREF(Signal* signal){
  jamEntry();
  AddFragRef * const ref = (AddFragRef*)signal->getDataPtr();

  ConnectRecordPtr connectPtr;
  connectPtr.i = ref->dihPtr;
  ptrCheckGuard(connectPtr, cconnectFileSize, connectRecord);

  {
    DiAddTabRef * const ref = (DiAddTabRef*)signal->getDataPtr();
    ref->senderData = connectPtr.p->userpointer;
    ref->errorCode = ~0;
    sendSignal(connectPtr.p->userblockref, GSN_DIADDTABREF, signal, 
	       DiAddTabRef::SignalLength, JBB);  
  }
  
  // Release
  release_connect(connectPtr);
}

/*
  3.7.1.3   R E F U S E
  *********************
  */
void Dbdih::addtabrefuseLab(Signal* signal, ConnectRecordPtr connectPtr, Uint32 errorCode) 
{
  signal->theData[0] = connectPtr.p->userpointer;
  signal->theData[1] = errorCode;
  sendSignal(connectPtr.p->userblockref, GSN_DIADDTABREF, signal, 2, JBB);
  release_connect(connectPtr);
  return;
}//Dbdih::addtabrefuseLab()

/*
  3.7.2   A D D   T A B L E   D U P L I C A T I O N
  *************************************************
  */
/*
  3.7.2.1    A D D   T A B L E   D U P L I C A T I O N   R E Q U E S T
  *******************************************************************=
  */

/*
  D E L E T E   T A B L E
  **********************=
  */
/*****************************************************************************/
/***********              DELETE TABLE  MODULE                   *************/
/*****************************************************************************/
void
Dbdih::execDROP_TAB_REQ(Signal* signal){
  jamEntry();
  DropTabReq* req = (DropTabReq*)signal->getDataPtr();

  TabRecordPtr tabPtr;
  tabPtr.i = req->tableId;
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
  
  tabPtr.p->m_dropTab.tabUserRef = req->senderRef;
  tabPtr.p->m_dropTab.tabUserPtr = req->senderData;

  DropTabReq::RequestType rt = (DropTabReq::RequestType)req->requestType;

  switch(rt){
  case DropTabReq::OnlineDropTab:
    jam();
    ndbrequire(tabPtr.p->tabStatus == TabRecord::TS_DROPPING);
    releaseTable(tabPtr);
    break;
  case DropTabReq::CreateTabDrop:
    jam();
    releaseTable(tabPtr);
    break;
  case DropTabReq::RestartDropTab:
    break;
  }
  
  startDeleteFile(signal, tabPtr);
}

void Dbdih::startDeleteFile(Signal* signal, TabRecordPtr tabPtr)
{
  if (tabPtr.p->tabFile[0] == RNIL) {
    jam();
    initTableFile(tabPtr);
  }//if
  openTableFileForDelete(signal, tabPtr.p->tabFile[0]);
}//Dbdih::startDeleteFile()

void Dbdih::openTableFileForDelete(Signal* signal, Uint32 fileIndex)
{
  FileRecordPtr filePtr;
  filePtr.i = fileIndex;
  ptrCheckGuard(filePtr, cfileFileSize, fileRecord);
  openFileRw(signal, filePtr);
  filePtr.p->reqStatus = FileRecord::TABLE_OPEN_FOR_DELETE;
}//Dbdih::openTableFileForDelete()

void Dbdih::tableOpenLab(Signal* signal, FileRecordPtr filePtr) 
{
  closeFileDelete(signal, filePtr);
  filePtr.p->reqStatus = FileRecord::TABLE_CLOSE_DELETE;
  return;
}//Dbdih::tableOpenLab()

void Dbdih::tableDeleteLab(Signal* signal, FileRecordPtr filePtr) 
{
  TabRecordPtr tabPtr;
  tabPtr.i = filePtr.p->tabRef;
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
  if (filePtr.i == tabPtr.p->tabFile[0]) {
    jam();
    openTableFileForDelete(signal, tabPtr.p->tabFile[1]);
    return;
  }//if
  ndbrequire(filePtr.i == tabPtr.p->tabFile[1]);
  
  releaseFile(tabPtr.p->tabFile[0]);
  releaseFile(tabPtr.p->tabFile[1]);
  tabPtr.p->tabFile[0] = tabPtr.p->tabFile[1] = RNIL;

  tabPtr.p->tabStatus = TabRecord::TS_IDLE;
  
  DropTabConf * const dropConf = (DropTabConf *)signal->getDataPtrSend();
  dropConf->senderRef = reference();
  dropConf->senderData = tabPtr.p->m_dropTab.tabUserPtr;
  dropConf->tableId = tabPtr.i;
  sendSignal(tabPtr.p->m_dropTab.tabUserRef, GSN_DROP_TAB_CONF, 
	     signal, DropTabConf::SignalLength, JBB);
  
  tabPtr.p->m_dropTab.tabUserPtr = RNIL;
  tabPtr.p->m_dropTab.tabUserRef = 0;
}//Dbdih::tableDeleteLab()


void Dbdih::releaseTable(TabRecordPtr tabPtr)
{
  FragmentstorePtr fragPtr;
  if (tabPtr.p->noOfFragChunks > 0) {
    for (Uint32 fragId = 0; fragId < tabPtr.p->totalfragments; fragId++) {
      jam();
      getFragstore(tabPtr.p, fragId, fragPtr);
      releaseReplicas(fragPtr.p->storedReplicas);
      releaseReplicas(fragPtr.p->oldStoredReplicas);
    }//for
    releaseFragments(tabPtr);
  }
  if (tabPtr.p->tabFile[0] != RNIL) {
    jam();
    releaseFile(tabPtr.p->tabFile[0]);
    releaseFile(tabPtr.p->tabFile[1]);
    tabPtr.p->tabFile[0] = tabPtr.p->tabFile[1] = RNIL;
  }//if
}//Dbdih::releaseTable()

void Dbdih::releaseReplicas(Uint32 replicaPtrI) 
{
  ReplicaRecordPtr replicaPtr;
  replicaPtr.i = replicaPtrI;
  jam();
  while (replicaPtr.i != RNIL) {
    jam();
    ptrCheckGuard(replicaPtr, creplicaFileSize, replicaRecord);
    Uint32 tmp = replicaPtr.p->nextReplica;
    replicaPtr.p->nextReplica = cfirstfreeReplica;
    cfirstfreeReplica = replicaPtr.i;
    replicaPtr.i = tmp;
    cnoFreeReplicaRec++;
  }//while
}//Dbdih::releaseReplicas()

void Dbdih::seizeReplicaRec(ReplicaRecordPtr& replicaPtr) 
{
  replicaPtr.i = cfirstfreeReplica;
  ptrCheckGuard(replicaPtr, creplicaFileSize, replicaRecord);
  cfirstfreeReplica = replicaPtr.p->nextReplica;
  cnoFreeReplicaRec--;
  replicaPtr.p->nextReplica = RNIL;
}//Dbdih::seizeReplicaRec()

void Dbdih::releaseFile(Uint32 fileIndex)
{
  FileRecordPtr filePtr;
  filePtr.i = fileIndex;
  ptrCheckGuard(filePtr, cfileFileSize, fileRecord);
  filePtr.p->nextFile = cfirstfreeFile;
  cfirstfreeFile = filePtr.i;
}//Dbdih::releaseFile()


void Dbdih::execALTER_TAB_REQ(Signal * signal)
{
  AlterTabReq* const req = (AlterTabReq*)signal->getDataPtr();
  const Uint32 senderRef = req->senderRef;
  const Uint32 senderData = req->senderData;
  const Uint32 changeMask = req->changeMask;
  const Uint32 tableId = req->tableId;
  const Uint32 tableVersion = req->tableVersion;
  const Uint32 gci = req->gci;
  AlterTabReq::RequestType requestType = 
    (AlterTabReq::RequestType) req->requestType;

  TabRecordPtr tabPtr;
  tabPtr.i = tableId;
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
  tabPtr.p->schemaVersion = tableVersion;

  // Request handled successfully 
  AlterTabConf * conf = (AlterTabConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = senderData;
  conf->changeMask = changeMask;
  conf->tableId = tableId;
  conf->tableVersion = tableVersion;
  conf->gci = gci;
  conf->requestType = requestType;
  sendSignal(senderRef, GSN_ALTER_TAB_CONF, signal, 
	     AlterTabConf::SignalLength, JBB);
}

/*
  G E T   N O D E S  
  **********************=
  */
/*****************************************************************************/
/* **********     TRANSACTION  HANDLING  MODULE                  *************/
/*****************************************************************************/
/*
  3.8.1    G E T   N O D E S   R E Q U E S T
  ******************************************
  Asks what nodes should be part of a transaction.
*/
void Dbdih::execDIGETNODESREQ(Signal* signal) 
{
  const DiGetNodesReq * const req = (DiGetNodesReq *)&signal->theData[0];
  FragmentstorePtr fragPtr;
  TabRecordPtr tabPtr;
  tabPtr.i = req->tableId;
  Uint32 hashValue = req->hashValue;
  Uint32 ttabFileSize = ctabFileSize;
  TabRecord* regTabDesc = tabRecord;
  jamEntry();
  ptrCheckGuard(tabPtr, ttabFileSize, regTabDesc);
  Uint32 fragId = hashValue & tabPtr.p->mask;
  ndbrequire(tabPtr.p->tabStatus == TabRecord::TS_ACTIVE);
  if (fragId < tabPtr.p->hashpointer) {
    jam();
    fragId = hashValue & ((tabPtr.p->mask << 1) + 1);
  }//if
  getFragstore(tabPtr.p, fragId, fragPtr);
  DiGetNodesConf * const conf = (DiGetNodesConf *)&signal->theData[0];
  Uint32 nodeCount = extractNodeInfo(fragPtr.p, conf->nodes);
  Uint32 sig2 = (nodeCount - 1) + 
    (fragPtr.p->distributionKey << 16);
  conf->zero = 0;
  conf->reqinfo = sig2;
  conf->fragId = fragId;
}//Dbdih::execDIGETNODESREQ()

Uint32 Dbdih::extractNodeInfo(const Fragmentstore * fragPtr, Uint32 nodes[]) 
{
  Uint32 nodeCount = 0;
  for (Uint32 i = 0; i < fragPtr->fragReplicas; i++) {
    jam();
    NodeRecordPtr nodePtr;
    ndbrequire(i < MAX_REPLICAS);
    nodePtr.i = fragPtr->activeNodes[i];
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
    if (nodePtr.p->useInTransactions) {
      jam();
      nodes[nodeCount] = nodePtr.i;
      nodeCount++;
    }//if
  }//for
  ndbrequire(nodeCount > 0);
  return nodeCount;
}//Dbdih::extractNodeInfo()

void 
Dbdih::getFragstore(TabRecord * tab,        //In parameter
                    Uint32 fragNo,              //In parameter
                    FragmentstorePtr & fragptr) //Out parameter
{
  FragmentstorePtr fragPtr;
  Uint32 chunkNo = fragNo >> LOG_NO_OF_FRAGS_PER_CHUNK;
  Uint32 chunkIndex = fragNo & (NO_OF_FRAGS_PER_CHUNK - 1);
  Uint32 TfragstoreFileSize = cfragstoreFileSize;
  Fragmentstore* TfragStore = fragmentstore;
  if (chunkNo < MAX_NDB_NODES) {
    fragPtr.i = tab->startFid[chunkNo] + chunkIndex;
    ptrCheckGuard(fragPtr, TfragstoreFileSize, TfragStore);
    fragptr = fragPtr;
    return;
  }//if
  ndbrequire(false);
}//Dbdih::getFragstore()

void Dbdih::allocFragments(Uint32 noOfFragments, TabRecordPtr tabPtr)
{
  FragmentstorePtr fragPtr;
  Uint32 noOfChunks = (noOfFragments + (NO_OF_FRAGS_PER_CHUNK - 1)) >> LOG_NO_OF_FRAGS_PER_CHUNK;
  ndbrequire(cremainingfrags >= noOfFragments);
  for (Uint32 i = 0; i < noOfChunks; i++) {
    jam();
    Uint32 baseFrag = cfirstfragstore;
    tabPtr.p->startFid[i] = baseFrag;
    fragPtr.i = baseFrag;
    ptrCheckGuard(fragPtr, cfragstoreFileSize, fragmentstore);
    cfirstfragstore = fragPtr.p->nextFragmentChunk;
    cremainingfrags -= NO_OF_FRAGS_PER_CHUNK;
    for (Uint32 j = 0; j < NO_OF_FRAGS_PER_CHUNK; j++) {
      jam();
      fragPtr.i = baseFrag + j;
      ptrCheckGuard(fragPtr, cfragstoreFileSize, fragmentstore);
      initFragstore(fragPtr);
    }//if
  }//for
  tabPtr.p->noOfFragChunks = noOfChunks;
}//Dbdih::allocFragments()

void Dbdih::releaseFragments(TabRecordPtr tabPtr)
{
  FragmentstorePtr fragPtr;
  for (Uint32 i = 0; i < tabPtr.p->noOfFragChunks; i++) {
    jam();
    Uint32 baseFrag = tabPtr.p->startFid[i];
    fragPtr.i = baseFrag;
    ptrCheckGuard(fragPtr, cfragstoreFileSize, fragmentstore);
    fragPtr.p->nextFragmentChunk = cfirstfragstore;
    cfirstfragstore = baseFrag;
    tabPtr.p->startFid[i] = RNIL;
    cremainingfrags += NO_OF_FRAGS_PER_CHUNK;
  }//for
  tabPtr.p->noOfFragChunks = 0;
}//Dbdih::releaseFragments()

void Dbdih::initialiseFragstore()
{
  Uint32 i;
  FragmentstorePtr fragPtr;
  for (i = 0; i < cfragstoreFileSize; i++) {
    fragPtr.i = i;
    ptrCheckGuard(fragPtr, cfragstoreFileSize, fragmentstore);
    initFragstore(fragPtr);
  }//for
  Uint32 noOfChunks = cfragstoreFileSize >> LOG_NO_OF_FRAGS_PER_CHUNK;
  fragPtr.i = 0;
  cfirstfragstore = RNIL;
  cremainingfrags = 0;
  for (i = 0; i < noOfChunks; i++) {
    refresh_watch_dog();
    ptrCheckGuard(fragPtr, cfragstoreFileSize, fragmentstore);
    fragPtr.p->nextFragmentChunk = cfirstfragstore;
    cfirstfragstore = fragPtr.i;
    fragPtr.i += NO_OF_FRAGS_PER_CHUNK;
    cremainingfrags += NO_OF_FRAGS_PER_CHUNK;
  }//for    
}//Dbdih::initialiseFragstore()

/*
  3.9   V E R I F I C A T I O N
  ****************************=
  */
/****************************************************************************/
/* **********     VERIFICATION SUB-MODULE                       *************/
/****************************************************************************/
/*
  3.9.1     R E C E I V I N G  O F  V E R I F I C A T I O N   R E Q U E S T
  *************************************************************************
  */
void Dbdih::execDIVERIFYREQ(Signal* signal) 
{

  jamEntry();
  if ((getBlockCommit() == false) &&
      (cfirstVerifyQueue == RNIL)) {
    jam();
    /*-----------------------------------------------------------------------*/
    // We are not blocked and the verify queue was empty currently so we can
    // simply reply back to TC immediately. The method was called with 
    // EXECUTE_DIRECT so we reply back by setting signal data and returning. 
    // theData[0] already contains the correct information so 
    // we need not touch it.
    /*-----------------------------------------------------------------------*/
    signal->theData[1] = currentgcp;
    signal->theData[2] = 0;
    return;
  }//if
  /*-------------------------------------------------------------------------*/
  // Since we are blocked we need to put this operation last in the verify
  // queue to ensure that operation starts up in the correct order.
  /*-------------------------------------------------------------------------*/
  ApiConnectRecordPtr tmpApiConnectptr;
  ApiConnectRecordPtr localApiConnectptr;

  cverifyQueueCounter++;
  localApiConnectptr.i = signal->theData[0];
  tmpApiConnectptr.i = clastVerifyQueue;
  ptrCheckGuard(localApiConnectptr, capiConnectFileSize, apiConnectRecord);
  localApiConnectptr.p->apiGci = cnewgcp;
  localApiConnectptr.p->nextApi = RNIL;
  clastVerifyQueue = localApiConnectptr.i;
  if (tmpApiConnectptr.i == RNIL) {
    jam();
    cfirstVerifyQueue = localApiConnectptr.i;
  } else {
    jam();
    ptrCheckGuard(tmpApiConnectptr, capiConnectFileSize, apiConnectRecord);
    tmpApiConnectptr.p->nextApi = localApiConnectptr.i;
  }//if
  emptyverificbuffer(signal, false);
  signal->theData[2] = 1; // Indicate no immediate return
  return;
}//Dbdih::execDIVERIFYREQ()

void Dbdih::execDI_FCOUNTREQ(Signal* signal) 
{
  ConnectRecordPtr connectPtr;
  TabRecordPtr tabPtr;
  jamEntry();
  connectPtr.i = signal->theData[0];
  tabPtr.i = signal->theData[1];
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);

  ndbrequire(tabPtr.p->tabStatus == TabRecord::TS_ACTIVE);

  if(connectPtr.i != RNIL){
    ptrCheckGuard(connectPtr, cconnectFileSize, connectRecord);
    if (connectPtr.p->connectState == ConnectRecord::INUSE) {
      jam();
      signal->theData[0] = connectPtr.p->userpointer;
      signal->theData[1] = tabPtr.p->totalfragments;
      sendSignal(connectPtr.p->userblockref, GSN_DI_FCOUNTCONF, signal,2, JBB);
      return;
    }//if
    signal->theData[0] = connectPtr.p->userpointer;
    signal->theData[1] = ZERRONOUSSTATE;
    sendSignal(connectPtr.p->userblockref, GSN_DI_FCOUNTREF, signal, 2, JBB);
    return;
  }//if

  //connectPtr.i == RNIL -> question without connect record
  const Uint32 senderData = signal->theData[2];
  const BlockReference senderRef = signal->senderBlockRef();
  signal->theData[0] = RNIL;
  signal->theData[1] = tabPtr.p->totalfragments;
  signal->theData[2] = tabPtr.i;
  signal->theData[3] = senderData;
  signal->theData[4] = tabPtr.p->noOfBackups;
  sendSignal(senderRef, GSN_DI_FCOUNTCONF, signal, 5, JBB);
}//Dbdih::execDI_FCOUNTREQ()

void Dbdih::execDIGETPRIMREQ(Signal* signal) 
{
  FragmentstorePtr fragPtr;
  ConnectRecordPtr connectPtr;
  TabRecordPtr tabPtr;
  jamEntry();
  Uint32 passThrough = signal->theData[1];
  tabPtr.i = signal->theData[2];
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
  if (DictTabInfo::isOrderedIndex(tabPtr.p->tableType)) {
    jam();
    tabPtr.i = tabPtr.p->primaryTableId;
    ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
  }
  Uint32 fragId = signal->theData[3];
  
  ndbrequire(tabPtr.p->tabStatus == TabRecord::TS_ACTIVE);
  connectPtr.i = signal->theData[0];
  if(connectPtr.i != RNIL)
  {
    jam();
    ptrCheckGuard(connectPtr, cconnectFileSize, connectRecord);
    signal->theData[0] = connectPtr.p->userpointer;
  }
  else
  {
    jam();
    signal->theData[0] = RNIL;
  }
  
  Uint32 nodes[MAX_REPLICAS];
  getFragstore(tabPtr.p, fragId, fragPtr);
  Uint32 count = extractNodeInfo(fragPtr.p, nodes);
  
  signal->theData[1] = passThrough;
  signal->theData[2] = nodes[0];
  signal->theData[3] = nodes[1];
  signal->theData[4] = nodes[2];
  signal->theData[5] = nodes[3];
  signal->theData[6] = count;
  signal->theData[7] = tabPtr.i;
  signal->theData[8] = fragId;

  const BlockReference senderRef = signal->senderBlockRef();
  sendSignal(senderRef, GSN_DIGETPRIMCONF, signal, 9, JBB);
}//Dbdih::execDIGETPRIMREQ()

/****************************************************************************/
/* **********     GLOBAL-CHECK-POINT HANDLING  MODULE           *************/
/****************************************************************************/
/*
  3.10   G L O B A L  C H E C K P O I N T ( IN  M A S T E R  R O L E)
  *******************************************************************
  */
void Dbdih::checkGcpStopLab(Signal* signal) 
{
  Uint32 tgcpStatus;

  tgcpStatus = cgcpStatus;
  if (tgcpStatus == coldGcpStatus) {
    jam();
    if (coldGcpId == cnewgcp) {
      jam();
      if (cgcpStatus != GCP_READY) {
        jam();
        cgcpSameCounter++;
        if (cgcpSameCounter == 1200) {
          jam();
#ifdef VM_TRACE
          ndbout << "System crash due to GCP Stop in state = ";
          ndbout << (Uint32) cgcpStatus << endl;
#endif
          crashSystemAtGcpStop(signal);
          return;
        }//if
      } else {
        jam();
        if (cgcpOrderBlocked == 0) {
          jam();
          cgcpSameCounter++;
          if (cgcpSameCounter == 1200) {
            jam();
#ifdef VM_TRACE
            ndbout << "System crash due to GCP Stop in state = ";
            ndbout << (Uint32) cgcpStatus << endl;
#endif
	    crashSystemAtGcpStop(signal);
            return;
          }//if
        } else {
          jam();
          cgcpSameCounter = 0;
        }//if
      }//if
    } else {
      jam();
      cgcpSameCounter = 0;
    }//if
  } else {
    jam();
    cgcpSameCounter = 0;
  }//if
  signal->theData[0] = DihContinueB::ZCHECK_GCP_STOP;
  signal->theData[1] = coldGcpStatus;
  signal->theData[2] = cgcpStatus;
  signal->theData[3] = coldGcpId;
  signal->theData[4] = cnewgcp;
  signal->theData[5] = cgcpSameCounter;
  sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 6);
  coldGcpStatus = cgcpStatus;
  coldGcpId = cnewgcp;
  return;
}//Dbdih::checkGcpStopLab()

void Dbdih::startGcpLab(Signal* signal, Uint32 aWaitTime) 
{
  if ((cgcpOrderBlocked == 1) ||
      (c_nodeStartMaster.blockGcp == true) ||
      (cfirstVerifyQueue != RNIL)) {
    /*************************************************************************/
    // 1: Global Checkpoint has been stopped by management command
    // 2: Global Checkpoint is blocked by node recovery activity
    // 3: Previous global checkpoint is not yet completed.
    // All this means that global checkpoint cannot start now.
    /*************************************************************************/
    jam();
    cgcpStartCounter++;
    signal->theData[0] = DihContinueB::ZSTART_GCP;
    signal->theData[1] = aWaitTime > 100 ? (aWaitTime - 100) : 0;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 2);
    return;
  }//if
  if (cstartGcpNow == false && aWaitTime > 100){
    /*************************************************************************/
    // We still have more than 100 milliseconds before we start the next and 
    // nobody has ordered immediate start of a global checkpoint.
    // During initial start we will use continuos global checkpoints to 
    // speed it up since we need to complete a global checkpoint after 
    // inserting a lot of records.
    /*************************************************************************/
    jam();
    cgcpStartCounter++;
    signal->theData[0] = DihContinueB::ZSTART_GCP;
    signal->theData[1] = (aWaitTime - 100);
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 2);
    return;
  }//if
  cgcpStartCounter = 0;
  cstartGcpNow = false;
  /***************************************************************************/
  // Report the event that a global checkpoint has started.
  /***************************************************************************/
  signal->theData[0] = NDB_LE_GlobalCheckpointStarted; //Event type
  signal->theData[1] = cnewgcp;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);

  CRASH_INSERTION(7000);
  cnewgcp++;
  signal->setTrace(TestOrd::TraceGlobalCheckpoint);
  sendLoopMacro(GCP_PREPARE, sendGCP_PREPARE);
  cgcpStatus = GCP_PREPARE_SENT;
}//Dbdih::startGcpLab()

void Dbdih::execGCP_PREPARECONF(Signal* signal) 
{
  jamEntry();
  Uint32 senderNodeId = signal->theData[0];
  Uint32 gci = signal->theData[1];
  ndbrequire(gci == cnewgcp);
  receiveLoopMacro(GCP_PREPARE, senderNodeId);
  //-------------------------------------------------------------
  // We have now received all replies. We are ready to continue
  // with committing the global checkpoint.
  //-------------------------------------------------------------
  gcpcommitreqLab(signal);
}//Dbdih::execGCP_PREPARECONF()

void Dbdih::gcpcommitreqLab(Signal* signal) 
{
  CRASH_INSERTION(7001);
  sendLoopMacro(GCP_COMMIT, sendGCP_COMMIT);
  cgcpStatus = GCP_COMMIT_SENT;
  return;
}//Dbdih::gcpcommitreqLab()

void Dbdih::execGCP_NODEFINISH(Signal* signal) 
{
  jamEntry();
  const Uint32 senderNodeId = signal->theData[0];
  const Uint32 gci = signal->theData[1];
  const Uint32 failureNr = signal->theData[2];
  if (!isMaster()) {
    jam();
    ndbrequire(failureNr > cfailurenr);
    //-------------------------------------------------------------
    // Another node thinks we are master. This could happen when he
    // has heard of a node failure which I have not heard of. Ignore
    // signal in this case since we will discover it by sending
    // MASTER_GCPREQ to the node.
    //-------------------------------------------------------------
    return;
  } else if (cmasterState == MASTER_TAKE_OVER_GCP) {
    jam();
    //-------------------------------------------------------------
    // We are currently taking over as master. We will delay the
    // signal until we have completed the take over gcp handling.
    //-------------------------------------------------------------
    sendSignalWithDelay(reference(), GSN_GCP_NODEFINISH, signal, 20, 3);
    return;
  } else {
    ndbrequire(cmasterState == MASTER_ACTIVE);
  }//if
  ndbrequire(gci == coldgcp);
  receiveLoopMacro(GCP_COMMIT, senderNodeId);
  //-------------------------------------------------------------
  // We have now received all replies. We are ready to continue
  // with saving the global checkpoint to disk.
  //-------------------------------------------------------------
  CRASH_INSERTION(7002);
  gcpsavereqLab(signal);
  return;
}//Dbdih::execGCP_NODEFINISH()

void Dbdih::gcpsavereqLab(Signal* signal) 
{
  sendLoopMacro(GCP_SAVEREQ, sendGCP_SAVEREQ);
  cgcpStatus = GCP_NODE_FINISHED;
}//Dbdih::gcpsavereqLab()

void Dbdih::execGCP_SAVECONF(Signal* signal) 
{
  jamEntry();  
  const GCPSaveConf * const saveConf = (GCPSaveConf*)&signal->theData[0];
  ndbrequire(saveConf->gci == coldgcp);
  ndbrequire(saveConf->nodeId == saveConf->dihPtr);
  SYSFILE->lastCompletedGCI[saveConf->nodeId] = saveConf->gci;  
  GCP_SAVEhandling(signal, saveConf->nodeId);
}//Dbdih::execGCP_SAVECONF()

void Dbdih::execGCP_SAVEREF(Signal* signal) 
{
  jamEntry();
  const GCPSaveRef * const saveRef = (GCPSaveRef*)&signal->theData[0];
  ndbrequire(saveRef->gci == coldgcp);
  ndbrequire(saveRef->nodeId == saveRef->dihPtr);
  /**
   * Only allow reason not to save
   */
  ndbrequire(saveRef->errorCode == GCPSaveRef::NodeShutdownInProgress ||
	     saveRef->errorCode == GCPSaveRef::FakedSignalDueToNodeFailure ||
	     saveRef->errorCode == GCPSaveRef::NodeRestartInProgress);
  GCP_SAVEhandling(signal, saveRef->nodeId);
}//Dbdih::execGCP_SAVEREF()

void Dbdih::GCP_SAVEhandling(Signal* signal, Uint32 nodeId) 
{
  receiveLoopMacro(GCP_SAVEREQ, nodeId);
  /*-------------------------------------------------------------------------*/
  // All nodes have replied. We are ready to update the system file.
  /*-------------------------------------------------------------------------*/
  cgcpStatus = GCP_SAVE_LQH_FINISHED;  
  CRASH_INSERTION(7003);
  checkToCopy();
  /**------------------------------------------------------------------------
   * SET NEW RECOVERABLE GCI. ALSO RESET RESTART COUNTER TO ZERO. 
   * THIS INDICATES THAT THE SYSTEM HAS BEEN RECOVERED AND SURVIVED AT 
   * LEAST ONE GLOBAL CHECKPOINT PERIOD. WE WILL USE THIS PARAMETER TO 
   * SET BACK THE RESTART GCI IF WE ENCOUNTER MORE THAN ONE UNSUCCESSFUL 
   * RESTART.
   *------------------------------------------------------------------------*/
  SYSFILE->newestRestorableGCI = coldgcp;
  if(Sysfile::getInitialStartOngoing(SYSFILE->systemRestartBits) &&
     getNodeState().startLevel == NodeState::SL_STARTED){
    jam();
#if 0
    ndbout_c("Dbdih: Clearing initial start ongoing");
#endif
    Sysfile::clearInitialStartOngoing(SYSFILE->systemRestartBits);
  }
  copyGciLab(signal, CopyGCIReq::GLOBAL_CHECKPOINT);
}//Dbdih::GCP_SAVEhandling()

/*
  3.11   G L O B A L  C H E C K P O I N T (N O T - M A S T E R)
  *************************************************************
  */
void Dbdih::execGCP_PREPARE(Signal* signal) 
{
  jamEntry();
  CRASH_INSERTION(7005);
  Uint32 masterNodeId = signal->theData[0];
  Uint32 gci = signal->theData[1];
  BlockReference retRef = calcDihBlockRef(masterNodeId);
                                                 
  ndbrequire (cmasterdihref == retRef);
  ndbrequire (cgcpParticipantState == GCP_PARTICIPANT_READY);
  ndbrequire (gci == (currentgcp + 1));
  
  cgckptflag = true;
  cgcpParticipantState = GCP_PARTICIPANT_PREPARE_RECEIVED;
  cnewgcp = gci;

  signal->theData[0] = cownNodeId;
  signal->theData[1] = gci;  
  sendSignal(retRef, GSN_GCP_PREPARECONF, signal, 2, JBA);
  return;
}//Dbdih::execGCP_PREPARE()

void Dbdih::execGCP_COMMIT(Signal* signal) 
{
  jamEntry();
  CRASH_INSERTION(7006);
  Uint32 masterNodeId = signal->theData[0];
  Uint32 gci = signal->theData[1];

  ndbrequire(gci == (currentgcp + 1));
  ndbrequire(masterNodeId = cmasterNodeId);
  ndbrequire(cgcpParticipantState == GCP_PARTICIPANT_PREPARE_RECEIVED);
  
  coldgcp = currentgcp;
  currentgcp = cnewgcp;  
  cgckptflag = false;
  emptyverificbuffer(signal, true);
  cgcpParticipantState = GCP_PARTICIPANT_COMMIT_RECEIVED;
  signal->theData[1] = coldgcp;
  sendSignal(clocaltcblockref, GSN_GCP_NOMORETRANS, signal, 2, JBB);
  return;
}//Dbdih::execGCP_COMMIT()

void Dbdih::execGCP_TCFINISHED(Signal* signal) 
{
  jamEntry();
  CRASH_INSERTION(7007);
  Uint32 gci = signal->theData[1];
  ndbrequire(gci == coldgcp);

  cgcpParticipantState = GCP_PARTICIPANT_TC_FINISHED;
  signal->theData[0] = cownNodeId;
  signal->theData[1] = coldgcp;
  signal->theData[2] = cfailurenr;
  sendSignal(cmasterdihref, GSN_GCP_NODEFINISH, signal, 3, JBB);
}//Dbdih::execGCP_TCFINISHED()

/*****************************************************************************/
//******     RECEIVING   TAMPER   REQUEST   FROM    NDBAPI             ******
/*****************************************************************************/
void Dbdih::execDIHNDBTAMPER(Signal* signal) 
{
  jamEntry();
  Uint32 tcgcpblocked = signal->theData[0];
  /* ACTION TO BE TAKEN BY DIH */
  Uint32 tuserpointer = signal->theData[1];
  BlockReference tuserblockref = signal->theData[2];
  switch (tcgcpblocked) {
  case 1:
    jam();
    if (isMaster()) {
      jam();
      cgcpOrderBlocked = 1;
    } else {
      jam();
      /* TRANSFER THE REQUEST */
      /* TO MASTER*/
      signal->theData[0] = tcgcpblocked;
      signal->theData[1] = tuserpointer;
      signal->theData[2] = tuserblockref;
      sendSignal(cmasterdihref, GSN_DIHNDBTAMPER, signal, 3, JBB);
    }//if
    break;
  case 2:
    jam();
    if (isMaster()) {
      jam();
      cgcpOrderBlocked = 0;
    } else {
      jam();
      /* TRANSFER THE REQUEST */
      /* TO MASTER*/
      signal->theData[0] = tcgcpblocked;
      signal->theData[1] = tuserpointer;
      signal->theData[2] = tuserblockref;
      sendSignal(cmasterdihref, GSN_DIHNDBTAMPER, signal, 3, JBB);
    }//if
    break;
  case 3:
    ndbrequire(false);
    return;
    break;
  case 4:
    jam();
    signal->theData[0] = tuserpointer;
    signal->theData[1] = crestartGci;
    sendSignal(tuserblockref, GSN_DIHNDBTAMPER, signal, 2, JBB);
    break;
#ifdef ERROR_INSERT
  case 5:
    jam();
    if(tuserpointer == 0)
    {
      jam();
      signal->theData[0] = 0;
      sendSignal(QMGR_REF, GSN_NDB_TAMPER, signal, 1, JBB);
      sendSignal(NDBCNTR_REF, GSN_NDB_TAMPER, signal, 1, JBB);
      sendSignal(NDBFS_REF, GSN_NDB_TAMPER, signal, 1, JBB);
      sendSignal(DBACC_REF, GSN_NDB_TAMPER, signal, 1, JBB);
      sendSignal(DBTUP_REF, GSN_NDB_TAMPER, signal, 1, JBB);
      sendSignal(DBLQH_REF, GSN_NDB_TAMPER, signal, 1, JBB);
      sendSignal(DBDICT_REF, GSN_NDB_TAMPER, signal, 1, JBB);
      sendSignal(DBDIH_REF, GSN_NDB_TAMPER, signal, 1, JBB);
      sendSignal(DBTC_REF, GSN_NDB_TAMPER, signal, 1, JBB);
      sendSignal(CMVMI_REF, GSN_NDB_TAMPER, signal, 1, JBB);
      return;
    }
    /*----------------------------------------------------------------------*/
    // Insert errors.
    /*----------------------------------------------------------------------*/
    if (tuserpointer < 1000) {
      /*--------------------------------------------------------------------*/
      // Insert errors into QMGR.
      /*--------------------------------------------------------------------*/
      jam();
      tuserblockref = QMGR_REF;
    } else if (tuserpointer < 2000) {
      /*--------------------------------------------------------------------*/
      // Insert errors into NDBCNTR.
      /*--------------------------------------------------------------------*/
      jam();
      tuserblockref = NDBCNTR_REF;
    } else if (tuserpointer < 3000) {
      /*--------------------------------------------------------------------*/
      // Insert errors into NDBFS.
      /*--------------------------------------------------------------------*/
      jam();
      tuserblockref = NDBFS_REF;
    } else if (tuserpointer < 4000) {
      /*--------------------------------------------------------------------*/
      // Insert errors into DBACC.
      /*--------------------------------------------------------------------*/
      jam();
      tuserblockref = DBACC_REF;
    } else if (tuserpointer < 5000) {
      /*--------------------------------------------------------------------*/
      // Insert errors into DBTUP.
      /*--------------------------------------------------------------------*/
      jam();
      tuserblockref = DBTUP_REF;
    } else if (tuserpointer < 6000) {
      /*---------------------------------------------------------------------*/
      // Insert errors into DBLQH.
      /*---------------------------------------------------------------------*/
      jam();
      tuserblockref = DBLQH_REF;
    } else if (tuserpointer < 7000) {
      /*---------------------------------------------------------------------*/
      // Insert errors into DBDICT.
      /*---------------------------------------------------------------------*/
      jam();
      tuserblockref = DBDICT_REF;
    } else if (tuserpointer < 8000) {
      /*---------------------------------------------------------------------*/
      // Insert errors into DBDIH.
      /*--------------------------------------------------------------------*/
      jam();
      tuserblockref = DBDIH_REF;
    } else if (tuserpointer < 9000) {
      /*--------------------------------------------------------------------*/
      // Insert errors into DBTC.
      /*--------------------------------------------------------------------*/
      jam();
      tuserblockref = DBTC_REF;
    } else if (tuserpointer < 10000) {
      /*--------------------------------------------------------------------*/
      // Insert errors into CMVMI.
      /*--------------------------------------------------------------------*/
      jam();
      tuserblockref = CMVMI_REF;
    } else if (tuserpointer < 11000) {
      jam();
      tuserblockref = BACKUP_REF;
    } else if (tuserpointer < 12000) {
      // DBUTIL_REF ?
      jam();
    } else if (tuserpointer < 13000) {
      jam();
      tuserblockref = DBTUX_REF;
    } else if (tuserpointer < 14000) {
      jam();
      tuserblockref = SUMA_REF;
    } else if (tuserpointer < 15000) {
      jam();
      tuserblockref = DBDICT_REF;
    } else if (tuserpointer < 30000) {
      /*--------------------------------------------------------------------*/
      // Ignore errors in the 20000-range.
      /*--------------------------------------------------------------------*/
      jam();
      return;
    } else if (tuserpointer < 40000) {
      jam();
      /*--------------------------------------------------------------------*/
      // Redirect errors to master DIH in the 30000-range.
      /*--------------------------------------------------------------------*/
      tuserblockref = cmasterdihref;
      tuserpointer -= 30000;
      signal->theData[0] = 5;
      signal->theData[1] = tuserpointer;
      signal->theData[2] = tuserblockref;
      sendSignal(tuserblockref, GSN_DIHNDBTAMPER, signal, 3, JBB);
      return;
    } else if (tuserpointer < 50000) {
      NodeRecordPtr localNodeptr;
      Uint32 Tfound = 0;
      jam();
      /*--------------------------------------------------------------------*/
      // Redirect errors to non-master DIH in the 40000-range.
      /*--------------------------------------------------------------------*/
      tuserpointer -= 40000;
      for (localNodeptr.i = 1; 
           localNodeptr.i < MAX_NDB_NODES;
           localNodeptr.i++) {
        jam();
        ptrAss(localNodeptr, nodeRecord);
        if ((localNodeptr.p->nodeStatus == NodeRecord::ALIVE) &&
            (localNodeptr.i != cmasterNodeId)) {
          jam();
          tuserblockref = calcDihBlockRef(localNodeptr.i);
          Tfound = 1;
          break;
        }//if
      }//for
      if (Tfound == 0) {
        jam();
	/*-------------------------------------------------------------------*/
	// Ignore since no non-master node existed.
	/*-------------------------------------------------------------------*/
        return;
      }//if
      signal->theData[0] = 5;
      signal->theData[1] = tuserpointer;
      signal->theData[2] = tuserblockref;
      sendSignal(tuserblockref, GSN_DIHNDBTAMPER, signal, 3, JBB);
      return;
    } else {
      jam();
      return;
    }//if
    signal->theData[0] = tuserpointer;
    if (tuserpointer != 0) {
      sendSignal(tuserblockref, GSN_NDB_TAMPER, signal, 1, JBB);
    } else {
      sendSignal(QMGR_REF, GSN_NDB_TAMPER, signal, 1, JBB);
      sendSignal(NDBCNTR_REF, GSN_NDB_TAMPER, signal, 1, JBB);
      sendSignal(NDBFS_REF, GSN_NDB_TAMPER, signal, 1, JBB);
      sendSignal(DBACC_REF, GSN_NDB_TAMPER, signal, 1, JBB);
      sendSignal(DBTUP_REF, GSN_NDB_TAMPER, signal, 1, JBB);
      sendSignal(DBLQH_REF, GSN_NDB_TAMPER, signal, 1, JBB);
      sendSignal(DBDICT_REF, GSN_NDB_TAMPER, signal, 1, JBB);
      sendSignal(DBDIH_REF, GSN_NDB_TAMPER, signal, 1, JBB);
      sendSignal(DBTC_REF, GSN_NDB_TAMPER, signal, 1, JBB);
      sendSignal(CMVMI_REF, GSN_NDB_TAMPER, signal, 1, JBB);
    }//if
    break;
#endif
  default:
    ndbrequire(false);
    break;
  }//switch
  return;
}//Dbdih::execDIHNDBTAMPER()

/*****************************************************************************/
/* **********     FILE HANDLING MODULE                           *************/
/*****************************************************************************/
void Dbdih::copyGciLab(Signal* signal, CopyGCIReq::CopyReason reason) 
{
  if(c_copyGCIMaster.m_copyReason != CopyGCIReq::IDLE){
    /**
     * There can currently only be one waiting
     */
    ndbrequire(c_copyGCIMaster.m_waiting == CopyGCIReq::IDLE);
    c_copyGCIMaster.m_waiting = reason;
    return;
  }
  c_copyGCIMaster.m_copyReason = reason;
  sendLoopMacro(COPY_GCIREQ, sendCOPY_GCIREQ);

}//Dbdih::copyGciLab()

/* ------------------------------------------------------------------------- */
/* COPY_GCICONF                           RESPONSE TO COPY_GCIREQ            */
/* ------------------------------------------------------------------------- */
void Dbdih::execCOPY_GCICONF(Signal* signal) 
{
  jamEntry();
  NodeRecordPtr senderNodePtr;
  senderNodePtr.i = signal->theData[0];
  receiveLoopMacro(COPY_GCIREQ, senderNodePtr.i);

  CopyGCIReq::CopyReason waiting = c_copyGCIMaster.m_waiting;
  CopyGCIReq::CopyReason current = c_copyGCIMaster.m_copyReason;

  c_copyGCIMaster.m_copyReason = CopyGCIReq::IDLE;
  c_copyGCIMaster.m_waiting = CopyGCIReq::IDLE;

  bool ok = false;
  switch(current){
  case CopyGCIReq::RESTART:{
    ok = true;
    jam();
    DictStartReq * req = (DictStartReq*)&signal->theData[0];
    req->restartGci = SYSFILE->newestRestorableGCI;
    req->senderRef = reference();
    sendSignal(cdictblockref, GSN_DICTSTARTREQ,
               signal, DictStartReq::SignalLength, JBB);
    break;
  }
  case CopyGCIReq::LOCAL_CHECKPOINT:{
    ok = true;
    jam();
    startLcpRoundLab(signal);
    break;
  }
  case CopyGCIReq::GLOBAL_CHECKPOINT:
    ok = true;
    jam();
    checkToCopyCompleted(signal);

    /************************************************************************/
    // Report the event that a global checkpoint has completed.
    /************************************************************************/
    signal->setTrace(0);
    signal->theData[0] = NDB_LE_GlobalCheckpointCompleted; //Event type
    signal->theData[1] = coldgcp;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);    

    CRASH_INSERTION(7004);
    emptyWaitGCPMasterQueue(signal);    
    cgcpStatus = GCP_READY;
    signal->theData[0] = DihContinueB::ZSTART_GCP;
    signal->theData[1] = cgcpDelay;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 2);
    if (c_nodeStartMaster.blockGcp == true) {
      jam();
      /* ------------------------------------------------------------------ */
      /*  A NEW NODE WANTS IN AND WE MUST ALLOW IT TO COME IN NOW SINCE THE */
      /*       GCP IS COMPLETED.                                            */
      /* ------------------------------------------------------------------ */
      gcpBlockedLab(signal);
    }//if
    break;
  case CopyGCIReq::INITIAL_START_COMPLETED:
    ok = true;
    jam();
    initialStartCompletedLab(signal);
    break;
  case CopyGCIReq::IDLE:
    ok = false;
    jam();
  }
  ndbrequire(ok);

  /**
   * Pop queue
   */
  if(waiting != CopyGCIReq::IDLE){
    c_copyGCIMaster.m_copyReason = waiting;
    signal->theData[0] = DihContinueB::ZCOPY_GCI;
    signal->theData[1] = waiting;
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
  }
}//Dbdih::execCOPY_GCICONF()

void Dbdih::invalidateLcpInfoAfterSr()
{
  NodeRecordPtr nodePtr;
  SYSFILE->latestLCP_ID--;
  Sysfile::clearLCPOngoing(SYSFILE->systemRestartBits);
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    jam();
    ptrAss(nodePtr, nodeRecord);
    if (!NdbNodeBitmask::get(SYSFILE->lcpActive, nodePtr.i)){
      jam();
      /* ------------------------------------------------------------------- */
      // The node was not active in the local checkpoint. 
      // To avoid that we step the active status too fast to not 
      // active we step back one step from Sysfile::NS_ActiveMissed_x.
      /* ------------------------------------------------------------------- */
      switch (nodePtr.p->activeStatus) {
      case Sysfile::NS_Active:
	/* ----------------------------------------------------------------- */
	// When not active in ongoing LCP and still active is a contradiction.
	/* ----------------------------------------------------------------- */
        ndbrequire(false);
      case Sysfile::NS_ActiveMissed_1:
        jam();
        nodePtr.p->activeStatus = Sysfile::NS_Active;
        break;
      case Sysfile::NS_ActiveMissed_2:
        jam();
        nodePtr.p->activeStatus = Sysfile::NS_ActiveMissed_1;
        break;
      default:
        jam();
        break;
      }//switch
    }//if
  }//for
  setNodeRestartInfoBits();
}//Dbdih::invalidateLcpInfoAfterSr()

/* ------------------------------------------------------------------------- */
/*       THE NEXT STEP IS TO WRITE THE FILE.                                 */
/* ------------------------------------------------------------------------- */
void Dbdih::openingCopyGciSkipInitLab(Signal* signal, FileRecordPtr filePtr) 
{
  writeRestorableGci(signal, filePtr);
  filePtr.p->reqStatus = FileRecord::WRITING_COPY_GCI;
  return;
}//Dbdih::openingCopyGciSkipInitLab()

void Dbdih::writingCopyGciLab(Signal* signal, FileRecordPtr filePtr) 
{
  /* ----------------------------------------------------------------------- */
  /*     WE HAVE NOW WRITTEN THIS FILE. WRITE ALSO NEXT FILE IF THIS IS NOT  */
  /*     ALREADY THE LAST.                                                   */
  /* ----------------------------------------------------------------------- */
  filePtr.p->reqStatus = FileRecord::IDLE;
  if (filePtr.i == crestartInfoFile[0]) {
    jam();
    filePtr.i = crestartInfoFile[1];
    ptrCheckGuard(filePtr, cfileFileSize, fileRecord);
    if (filePtr.p->fileStatus == FileRecord::OPEN) {
      jam();
      openingCopyGciSkipInitLab(signal, filePtr);
      return;
    }//if
    openFileRw(signal, filePtr);
    filePtr.p->reqStatus = FileRecord::OPENING_COPY_GCI;
    return;
  }//if
  /* ----------------------------------------------------------------------- */
  /*     WE HAVE COMPLETED WRITING BOTH FILES SUCCESSFULLY. NOW REPORT OUR   */
  /*     SUCCESS TO THE MASTER DIH. BUT FIRST WE NEED TO RESET A NUMBER OF   */
  /*     VARIABLES USED BY THE LOCAL CHECKPOINT PROCESS (ONLY IF TRIGGERED   */
  /*     BY LOCAL CHECKPOINT PROCESS.                                        */
  /* ----------------------------------------------------------------------- */
  CopyGCIReq::CopyReason reason = c_copyGCISlave.m_copyReason;
  
  if (reason == CopyGCIReq::GLOBAL_CHECKPOINT) {
    jam();
    cgcpParticipantState = GCP_PARTICIPANT_READY;
    
    SubGcpCompleteRep * const rep = (SubGcpCompleteRep*)signal->getDataPtr();
    rep->gci = coldgcp;
    rep->senderData = 0;
    sendSignal(SUMA_REF, GSN_SUB_GCP_COMPLETE_REP, signal, 
	       SubGcpCompleteRep::SignalLength, JBB);
  }
  
  jam();
  c_copyGCISlave.m_copyReason = CopyGCIReq::IDLE;
  
  if(c_copyGCISlave.m_senderRef == cmasterdihref){
    jam();
    /**
     * Only if same master
     */
    signal->theData[0] = c_copyGCISlave.m_senderData;
    sendSignal(c_copyGCISlave.m_senderRef, GSN_COPY_GCICONF, signal, 1, JBB);

  }
  return;
}//Dbdih::writingCopyGciLab()

void Dbdih::execSTART_LCP_REQ(Signal* signal){
  StartLcpReq * req = (StartLcpReq*)signal->getDataPtr();
 
  CRASH_INSERTION2(7021, isMaster());
  CRASH_INSERTION2(7022, !isMaster());

  ndbrequire(c_lcpState.m_masterLcpDihRef = req->senderRef);
  c_lcpState.m_participatingDIH = req->participatingDIH;
  c_lcpState.m_participatingLQH = req->participatingLQH;
  
  c_lcpState.m_LCP_COMPLETE_REP_Counter_LQH = req->participatingLQH;
  if(isMaster()){
    jam();
    ndbrequire(isActiveMaster());
    c_lcpState.m_LCP_COMPLETE_REP_Counter_DIH = req->participatingDIH;

  } else {
    c_lcpState.m_LCP_COMPLETE_REP_Counter_DIH.clearWaitingFor();
  }

  c_lcpState.m_LCP_COMPLETE_REP_From_Master_Received = false;  

  c_lcpState.setLcpStatus(LCP_INIT_TABLES, __LINE__);
  
  signal->theData[0] = DihContinueB::ZINIT_LCP;
  signal->theData[1] = c_lcpState.m_masterLcpDihRef;
  signal->theData[2] = 0;
  sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBB);
}

void Dbdih::initLcpLab(Signal* signal, Uint32 senderRef, Uint32 tableId) 
{
  TabRecordPtr tabPtr;
  tabPtr.i = tableId;

  if(c_lcpState.m_masterLcpDihRef != senderRef){
    jam();
    /**
     * This is LCP master takeover
     */
#ifdef VM_TRACE
    ndbout_c("initLcpLab aborted due to LCP master takeover - 1");
#endif
    c_lcpState.setLcpStatus(LCP_STATUS_IDLE, __LINE__);
    sendMASTER_LCPCONF(signal);
    return;
  }

  if(c_lcpState.m_masterLcpDihRef != cmasterdihref){
    jam();
    /**
     * Master take over but has not yet received MASTER_LCPREQ
     */
#ifdef VM_TRACE
    ndbout_c("initLcpLab aborted due to LCP master takeover - 2");    
#endif
    return;
  }

  //const Uint32 lcpId = SYSFILE->latestLCP_ID;

  for(; tabPtr.i < ctabFileSize; tabPtr.i++){

    ptrAss(tabPtr, tabRecord);

    if (tabPtr.p->tabStatus != TabRecord::TS_ACTIVE) {
      jam();
      tabPtr.p->tabLcpStatus = TabRecord::TLS_COMPLETED;
      continue;
    }

    if (tabPtr.p->storedTable == 0) {
      /**
       * Temporary table
       */
      jam();
      tabPtr.p->tabLcpStatus = TabRecord::TLS_COMPLETED;
      continue;
    }
    
    if (tabPtr.p->tabCopyStatus != TabRecord::CS_IDLE) {
      /* ----------------------------------------------------------------- */
      // We protect the updates of table data structures by this variable.
      /* ----------------------------------------------------------------- */
      jam();
      signal->theData[0] = DihContinueB::ZINIT_LCP;
      signal->theData[1] = senderRef;
      signal->theData[2] = tabPtr.i;
      sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 20, 3);
      return;
    }//if

    /**
     * Found a table
     */
    tabPtr.p->tabLcpStatus = TabRecord::TLS_ACTIVE;

    /**
     * For each fragment
     */
    for (Uint32 fragId = 0; fragId < tabPtr.p->totalfragments; fragId++) {
      jam();
      FragmentstorePtr fragPtr;
      getFragstore(tabPtr.p, fragId, fragPtr);

      /**
       * For each of replica record
       */
      Uint32 replicaCount = 0;
      ReplicaRecordPtr replicaPtr;
      for(replicaPtr.i = fragPtr.p->storedReplicas; replicaPtr.i != RNIL;
	  replicaPtr.i = replicaPtr.p->nextReplica) {
	jam();
	
	ptrCheckGuard(replicaPtr, creplicaFileSize, replicaRecord);
	Uint32 nodeId = replicaPtr.p->procNode;
	if(c_lcpState.m_participatingLQH.get(nodeId)){
	  jam();
	  replicaCount++;
	  replicaPtr.p->lcpOngoingFlag = true;
	}
      }
      
      fragPtr.p->noLcpReplicas = replicaCount;
    }//for
    
    signal->theData[0] = DihContinueB::ZINIT_LCP;
    signal->theData[1] = senderRef;
    signal->theData[2] = tabPtr.i + 1;
    sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBB);
    return;
  }

  /**
   * No more tables
   */
  jam();

  if (c_lcpState.m_masterLcpDihRef != reference()){
    jam();
    ndbrequire(!isMaster());
    c_lcpState.setLcpStatus(LCP_STATUS_ACTIVE, __LINE__);
  } else {
    jam();
    ndbrequire(isMaster());
  }

  CRASH_INSERTION2(7023, isMaster());
  CRASH_INSERTION2(7024, !isMaster());
  
  jam();
  StartLcpConf * conf = (StartLcpConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  sendSignal(c_lcpState.m_masterLcpDihRef, GSN_START_LCP_CONF, signal, 
	     StartLcpConf::SignalLength, JBB);
  return;
}//Dbdih::initLcpLab()

/* ------------------------------------------------------------------------- */
/*       ERROR HANDLING FOR COPY RESTORABLE GCI FILE.                        */
/* ------------------------------------------------------------------------- */
void Dbdih::openingCopyGciErrorLab(Signal* signal, FileRecordPtr filePtr) 
{
  createFileRw(signal, filePtr);
  /* ------------------------------------------------------------------------- */
  /*       ERROR IN OPENING FILE. WE WILL TRY BY CREATING FILE INSTEAD.        */
  /* ------------------------------------------------------------------------- */
  filePtr.p->reqStatus = FileRecord::CREATING_COPY_GCI;
  return;
}//Dbdih::openingCopyGciErrorLab()

/* ------------------------------------------------------------------------- */
/*       ENTER DICTSTARTCONF WITH                                            */
/*         TBLOCKREF                                                         */
/* ------------------------------------------------------------------------- */
void Dbdih::dictStartConfLab(Signal* signal) 
{
  /* ----------------------------------------------------------------------- */
  /*     WE HAVE NOW RECEIVED ALL THE TABLES TO RESTART.                     */
  /* ----------------------------------------------------------------------- */
  signal->theData[0] = DihContinueB::ZSTART_FRAGMENT;
  signal->theData[1] = 0;  /* START WITH TABLE 0    */
  signal->theData[2] = 0;  /* AND FRAGMENT 0        */
  sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBB);
  return;
}//Dbdih::dictStartConfLab()


void Dbdih::openingTableLab(Signal* signal, FileRecordPtr filePtr) 
{
  /* ---------------------------------------------------------------------- */
  /*    SUCCESSFULLY OPENED A FILE. READ THE FIRST PAGE OF THIS FILE.       */
  /* ---------------------------------------------------------------------- */
  TabRecordPtr tabPtr;
  PageRecordPtr pagePtr;

  tabPtr.i = filePtr.p->tabRef;
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
  tabPtr.p->noPages = 1;
  allocpage(pagePtr);
  tabPtr.p->pageRef[0] = pagePtr.i;
  readTabfile(signal, tabPtr.p, filePtr);
  filePtr.p->reqStatus = FileRecord::READING_TABLE;
  return;
}//Dbdih::openingTableLab()

void Dbdih::openingTableErrorLab(Signal* signal, FileRecordPtr filePtr) 
{
  TabRecordPtr tabPtr;
  tabPtr.i = filePtr.p->tabRef;
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
  /* ---------------------------------------------------------------------- */
  /*    WE FAILED IN OPENING A FILE. IF THE FIRST FILE THEN TRY WITH THE    */
  /*    DUPLICATE FILE, OTHERWISE WE REPORT AN ERROR IN THE SYSTEM RESTART. */
  /* ---------------------------------------------------------------------- */
  ndbrequire(filePtr.i == tabPtr.p->tabFile[0]);
  filePtr.i = tabPtr.p->tabFile[1];
  ptrCheckGuard(filePtr, cfileFileSize, fileRecord);
  openFileRw(signal, filePtr);
  filePtr.p->reqStatus = FileRecord::OPENING_TABLE;
}//Dbdih::openingTableErrorLab()

void Dbdih::readingTableLab(Signal* signal, FileRecordPtr filePtr) 
{
  TabRecordPtr tabPtr;
  PageRecordPtr pagePtr;
  /* ---------------------------------------------------------------------- */
  /*    WE HAVE SUCCESSFULLY READ A NUMBER OF PAGES IN THE TABLE FILE. IF   */
  /*    MORE PAGES EXIST IN THE FILE THEN READ ALL PAGES IN THE FILE.       */
  /* ---------------------------------------------------------------------- */
  filePtr.p->reqStatus = FileRecord::IDLE;
  tabPtr.i = filePtr.p->tabRef;
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
  pagePtr.i = tabPtr.p->pageRef[0];
  ptrCheckGuard(pagePtr, cpageFileSize, pageRecord);
  Uint32 noOfStoredPages = pagePtr.p->word[33];
  if (tabPtr.p->noPages < noOfStoredPages) {
    jam();
    ndbrequire(noOfStoredPages <= 8);
    for (Uint32 i = tabPtr.p->noPages; i < noOfStoredPages; i++) {
      jam();
      allocpage(pagePtr);
      tabPtr.p->pageRef[i] = pagePtr.i;
    }//for
    tabPtr.p->noPages = noOfStoredPages;
    readTabfile(signal, tabPtr.p, filePtr);
    filePtr.p->reqStatus = FileRecord::READING_TABLE;
  } else {
    ndbrequire(tabPtr.p->noPages == pagePtr.p->word[33]);
    ndbrequire(tabPtr.p->tabCopyStatus == TabRecord::CS_IDLE);
    jam();
    /* --------------------------------------------------------------------- */
    /*   WE HAVE READ ALL PAGES. NOW READ FROM PAGES INTO TABLE AND FRAGMENT */
    /*   DATA STRUCTURES.                                                    */
    /* --------------------------------------------------------------------- */
    tabPtr.p->tabCopyStatus = TabRecord::CS_SR_PHASE1_READ_PAGES;
    signal->theData[0] = DihContinueB::ZREAD_PAGES_INTO_TABLE;
    signal->theData[1] = tabPtr.i;
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
    return;
  }//if
  return;
}//Dbdih::readingTableLab()

void Dbdih::readTableFromPagesLab(Signal* signal, TabRecordPtr tabPtr) 
{
  FileRecordPtr filePtr;
  filePtr.i = tabPtr.p->tabFile[0];
  ptrCheckGuard(filePtr, cfileFileSize, fileRecord);
  /* ---------------------------------------------------------------------- */
  /*    WE HAVE NOW COPIED TO OUR NODE. WE HAVE NOW COMPLETED RESTORING     */
  /*    THIS TABLE. CONTINUE WITH THE NEXT TABLE.                           */
  /*    WE ALSO NEED TO CLOSE THE TABLE FILE.                               */
  /* ---------------------------------------------------------------------- */
  if (filePtr.p->fileStatus != FileRecord::OPEN) {
    jam();
    filePtr.i = tabPtr.p->tabFile[1];
    ptrCheckGuard(filePtr, cfileFileSize, fileRecord);
  }//if
  closeFile(signal, filePtr);
  filePtr.p->reqStatus = FileRecord::CLOSING_TABLE_SR;
  return;
}//Dbdih::readTableFromPagesLab()

void Dbdih::closingTableSrLab(Signal* signal, FileRecordPtr filePtr) 
{
  /**
   * Update table/fragment info
   */
  TabRecordPtr tabPtr;
  tabPtr.i = filePtr.p->tabRef;
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
  resetReplicaSr(tabPtr);

  signal->theData[0] = DihContinueB::ZCOPY_TABLE;
  signal->theData[1] = filePtr.p->tabRef;
  sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);

  return;
}//Dbdih::closingTableSrLab()

void
Dbdih::resetReplicaSr(TabRecordPtr tabPtr){

  const Uint32 newestRestorableGCI = SYSFILE->newestRestorableGCI;
  
  for(Uint32 i = 0; i<tabPtr.p->totalfragments; i++){
    FragmentstorePtr fragPtr;
    getFragstore(tabPtr.p, i, fragPtr);
    
    /**
     * 1) Start by moving all replicas into oldStoredReplicas
     */
    prepareReplicas(fragPtr);

    /**
     * 2) Move all "alive" replicas into storedReplicas
     *    + update noCrashedReplicas...
     */
    ReplicaRecordPtr replicaPtr;
    replicaPtr.i = fragPtr.p->oldStoredReplicas;
    while (replicaPtr.i != RNIL) {
      jam();
      ptrCheckGuard(replicaPtr, creplicaFileSize, replicaRecord);
      const Uint32 nextReplicaPtrI = replicaPtr.p->nextReplica;

      NodeRecordPtr nodePtr;
      nodePtr.i = replicaPtr.p->procNode;
      ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);

      const Uint32 noCrashedReplicas = replicaPtr.p->noCrashedReplicas;
      if (nodePtr.p->nodeStatus == NodeRecord::ALIVE) {
	jam();
	switch (nodePtr.p->activeStatus) {
	case Sysfile::NS_Active:
	case Sysfile::NS_ActiveMissed_1:
	case Sysfile::NS_ActiveMissed_2:{
	  jam();
	  /* --------------------------------------------------------------- */
	  /* THE NODE IS ALIVE AND KICKING AND ACTIVE, LET'S USE IT.         */
	  /* --------------------------------------------------------------- */
	  arrGuardErr(noCrashedReplicas, 8, NDBD_EXIT_MAX_CRASHED_REPLICAS);
	  Uint32 lastGci = replicaPtr.p->replicaLastGci[noCrashedReplicas];
	  if(lastGci >= newestRestorableGCI){
	    jam();
	    /** -------------------------------------------------------------
	     * THE REPLICA WAS ALIVE AT THE SYSTEM FAILURE. WE WILL SET THE 
	     * LAST REPLICA GCI TO MINUS ONE SINCE IT HASN'T FAILED YET IN THE
	     * NEW SYSTEM.                                                    
	     *-------------------------------------------------------------- */
	    replicaPtr.p->replicaLastGci[noCrashedReplicas] = (Uint32)-1;
	  } else {
	    jam();
	    /*--------------------------------------------------------------
	     * SINCE IT WAS NOT ALIVE AT THE TIME OF THE SYSTEM CRASH THIS IS 
	     * A COMPLETELY NEW REPLICA. WE WILL SET THE CREATE GCI TO BE THE 
	     * NEXT GCI TO BE EXECUTED.                                       
	     *--------_----------------------------------------------------- */
	    const Uint32 nextCrashed = noCrashedReplicas + 1;
	    replicaPtr.p->noCrashedReplicas = nextCrashed;
	    arrGuard(nextCrashed, 8);
	    replicaPtr.p->createGci[nextCrashed] = newestRestorableGCI + 1;
	    ndbrequire(newestRestorableGCI + 1 != 0xF1F1F1F1);
	    replicaPtr.p->replicaLastGci[nextCrashed] = (Uint32)-1;
	  }//if

	  resetReplicaLcp(replicaPtr.p, newestRestorableGCI);

	  /* -----------------------------------------------------------------
	   *   LINK THE REPLICA INTO THE STORED REPLICA LIST. WE WILL USE THIS
	   *   NODE AS A STORED REPLICA.                                      
	   *   WE MUST FIRST LINK IT OUT OF THE LIST OF OLD STORED REPLICAS.  
	   * --------------------------------------------------------------- */
	  removeOldStoredReplica(fragPtr, replicaPtr);
	  linkStoredReplica(fragPtr, replicaPtr);

	}
        default:
	  jam();
	  /*empty*/;
	  break;
	}
      }
      replicaPtr.i = nextReplicaPtrI;
    }//while
  }
}

void
Dbdih::resetReplicaLcp(ReplicaRecord * replicaP, Uint32 stopGci){

  Uint32 lcpNo = replicaP->nextLcp;
  const Uint32 startLcpNo = lcpNo;
  do {
    lcpNo = prevLcpNo(lcpNo);
    ndbrequire(lcpNo < MAX_LCP_STORED);
    if (replicaP->lcpStatus[lcpNo] == ZVALID) {
      if (replicaP->maxGciStarted[lcpNo] < stopGci) {
        jam();
	/* ----------------------------------------------------------------- */
	/*   WE HAVE FOUND A USEFUL LOCAL CHECKPOINT THAT CAN BE USED FOR    */
	/*   RESTARTING THIS FRAGMENT REPLICA.                               */
	/* ----------------------------------------------------------------- */
        return ;
      }//if
    }//if
    
    /**
     * WE COULD  NOT USE THIS LOCAL CHECKPOINT. IT WAS TOO
     * RECENT OR SIMPLY NOT A VALID CHECKPOINT.
     * WE SHOULD THUS REMOVE THIS LOCAL CHECKPOINT SINCE IT WILL NEVER
     * AGAIN BE USED. SET LCP_STATUS TO INVALID.
     */
    replicaP->nextLcp = lcpNo;
    replicaP->lcpId[lcpNo] = 0;
    replicaP->lcpStatus[lcpNo] = ZINVALID;
  } while (lcpNo != startLcpNo);
  
  replicaP->nextLcp = 0;
}

void Dbdih::readingTableErrorLab(Signal* signal, FileRecordPtr filePtr) 
{
  TabRecordPtr tabPtr;
  tabPtr.i = filePtr.p->tabRef;
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
  /* ---------------------------------------------------------------------- */
  /*    READING THIS FILE FAILED. CLOSE IT AFTER RELEASING ALL PAGES.       */
  /* ---------------------------------------------------------------------- */
  ndbrequire(tabPtr.p->noPages <= 8);
  for (Uint32 i = 0; i < tabPtr.p->noPages; i++) {
    jam();
    releasePage(tabPtr.p->pageRef[i]);
  }//for
  closeFile(signal, filePtr);
  filePtr.p->reqStatus = FileRecord::CLOSING_TABLE_CRASH;
  return;
}//Dbdih::readingTableErrorLab()

void Dbdih::closingTableCrashLab(Signal* signal, FileRecordPtr filePtr) 
{
  TabRecordPtr tabPtr;
  /* ---------------------------------------------------------------------- */
  /*    WE HAVE NOW CLOSED A FILE WHICH WE HAD A READ ERROR WITH. PROCEED   */
  /*    WITH NEXT FILE IF NOT THE LAST OTHERWISE REPORT ERROR.              */
  /* ---------------------------------------------------------------------- */
  tabPtr.i = filePtr.p->tabRef;
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
  ndbrequire(filePtr.i == tabPtr.p->tabFile[0]);
  filePtr.i = tabPtr.p->tabFile[1];
  ptrCheckGuard(filePtr, cfileFileSize, fileRecord);
  openFileRw(signal, filePtr);
  filePtr.p->reqStatus = FileRecord::OPENING_TABLE;
}//Dbdih::closingTableCrashLab()

/*****************************************************************************/
/* **********     COPY TABLE MODULE                              *************/
/*****************************************************************************/
void Dbdih::execCOPY_TABREQ(Signal* signal) 
{
  CRASH_INSERTION(7172);

  TabRecordPtr tabPtr;
  PageRecordPtr pagePtr;
  jamEntry();
  BlockReference ref = signal->theData[0];
  Uint32 reqinfo = signal->theData[1];
  tabPtr.i = signal->theData[2];
  Uint32 schemaVersion = signal->theData[3];
  Uint32 noOfWords = signal->theData[4];
  ndbrequire(ref == cmasterdihref);
  ndbrequire(!isMaster());
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
  if (reqinfo == 1) {
    jam();
    tabPtr.p->schemaVersion = schemaVersion;
    initTableFile(tabPtr);
  }//if
  ndbrequire(tabPtr.p->noPages < 8);
  if (tabPtr.p->noOfWords == 0) {
    jam();
    allocpage(pagePtr);
    tabPtr.p->pageRef[tabPtr.p->noPages] = pagePtr.i;
    tabPtr.p->noPages++;
  } else {
    jam();
    pagePtr.i = tabPtr.p->pageRef[tabPtr.p->noPages - 1];
    ptrCheckGuard(pagePtr, cpageFileSize, pageRecord);
  }//if
  ndbrequire(tabPtr.p->noOfWords + 15 < 2048);
  ndbrequire(tabPtr.p->noOfWords < 2048);
  MEMCOPY_NO_WORDS(&pagePtr.p->word[tabPtr.p->noOfWords], &signal->theData[5], 16);
  tabPtr.p->noOfWords += 16;
  if (tabPtr.p->noOfWords == 2048) {
    jam();
    tabPtr.p->noOfWords = 0;
  }//if
  if (noOfWords > 16) {
    jam();
    return;
  }//if
  tabPtr.p->noOfWords = 0;
  ndbrequire(tabPtr.p->tabCopyStatus == TabRecord::CS_IDLE);
  tabPtr.p->tabCopyStatus = TabRecord::CS_COPY_TAB_REQ;
  signal->theData[0] = DihContinueB::ZREAD_PAGES_INTO_TABLE;
  signal->theData[1] = tabPtr.i;
  sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
}//Dbdih::execCOPY_TABREQ()

void
Dbdih::copyTabReq_complete(Signal* signal, TabRecordPtr tabPtr){
  if (!isMaster()) {
    jam();
    //----------------------------------------------------------------------------
    // In this particular case we do not release table pages if we are master. The
    // reason is that the master could still be sending the table info to another
    // node.
    //----------------------------------------------------------------------------
    releaseTabPages(tabPtr.i);
    tabPtr.p->tabStatus = TabRecord::TS_ACTIVE;
    for (Uint32 fragId = 0; fragId < tabPtr.p->totalfragments; fragId++) {
      jam();
      FragmentstorePtr fragPtr;
      getFragstore(tabPtr.p, fragId, fragPtr);
      updateNodeInfo(fragPtr);
    }//for
  }//if
  signal->theData[0] = cownNodeId;
  signal->theData[1] = tabPtr.i;
  sendSignal(cmasterdihref, GSN_COPY_TABCONF, signal, 2, JBB);
}

/*****************************************************************************/
/* ******  READ FROM A NUMBER OF PAGES INTO THE TABLE DATA STRUCTURES ********/
/*****************************************************************************/
void Dbdih::readPagesIntoTableLab(Signal* signal, Uint32 tableId) 
{
  RWFragment rf;
  rf.wordIndex = 35;
  rf.pageIndex = 0;
  rf.rwfTabPtr.i = tableId;
  ptrCheckGuard(rf.rwfTabPtr, ctabFileSize, tabRecord);
  rf.rwfPageptr.i = rf.rwfTabPtr.p->pageRef[0];
  ptrCheckGuard(rf.rwfPageptr, cpageFileSize, pageRecord);
  rf.rwfTabPtr.p->totalfragments = readPageWord(&rf);
  rf.rwfTabPtr.p->noOfBackups = readPageWord(&rf);
  rf.rwfTabPtr.p->hashpointer = readPageWord(&rf);
  rf.rwfTabPtr.p->kvalue = readPageWord(&rf);
  rf.rwfTabPtr.p->mask = readPageWord(&rf);
  ndbrequire(readPageWord(&rf) == TabRecord::HASH);
  rf.rwfTabPtr.p->method = TabRecord::HASH;
  /* ---------------------------------- */
  /* Type of table, 2 = temporary table */
  /* ---------------------------------- */
  rf.rwfTabPtr.p->storedTable = readPageWord(&rf); 

  Uint32 noOfFrags = rf.rwfTabPtr.p->totalfragments;
  ndbrequire(noOfFrags > 0);
  ndbrequire((noOfFrags * (rf.rwfTabPtr.p->noOfBackups + 1)) <= cnoFreeReplicaRec);
  allocFragments(noOfFrags, rf.rwfTabPtr);
  
  signal->theData[0] = DihContinueB::ZREAD_PAGES_INTO_FRAG;
  signal->theData[1] = rf.rwfTabPtr.i;
  signal->theData[2] = 0;
  signal->theData[3] = rf.pageIndex;
  signal->theData[4] = rf.wordIndex;
  sendSignal(reference(), GSN_CONTINUEB, signal, 5, JBB);
  return;
}//Dbdih::readPagesIntoTableLab()

void Dbdih::readPagesIntoFragLab(Signal* signal, RWFragment* rf) 
{
  ndbrequire(rf->pageIndex < 8);
  rf->rwfPageptr.i = rf->rwfTabPtr.p->pageRef[rf->pageIndex];
  ptrCheckGuard(rf->rwfPageptr, cpageFileSize, pageRecord);
  FragmentstorePtr fragPtr;
  getFragstore(rf->rwfTabPtr.p, rf->fragId, fragPtr);
  readFragment(rf, fragPtr);
  readReplicas(rf, fragPtr);
  rf->fragId++;
  if (rf->fragId == rf->rwfTabPtr.p->totalfragments) {
    jam();
    switch (rf->rwfTabPtr.p->tabCopyStatus) {
    case TabRecord::CS_SR_PHASE1_READ_PAGES:
      jam();
      releaseTabPages(rf->rwfTabPtr.i);
      rf->rwfTabPtr.p->tabCopyStatus = TabRecord::CS_IDLE;
      signal->theData[0] = DihContinueB::ZREAD_TABLE_FROM_PAGES;
      signal->theData[1] = rf->rwfTabPtr.i;
      sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
      return;
      break;
    case TabRecord::CS_COPY_TAB_REQ:
      jam();
      rf->rwfTabPtr.p->tabCopyStatus = TabRecord::CS_IDLE;
      if(getNodeState().getSystemRestartInProgress()){
	jam();
	copyTabReq_complete(signal, rf->rwfTabPtr);
	return;
      }
      rf->rwfTabPtr.p->tabCopyStatus = TabRecord::CS_IDLE;
      rf->rwfTabPtr.p->tabUpdateState = TabRecord::US_COPY_TAB_REQ;
      signal->theData[0] = DihContinueB::ZTABLE_UPDATE;
      signal->theData[1] = rf->rwfTabPtr.i;
      sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
      return;
      break;
    default:
      ndbrequire(false);
      return;
      break;
    }//switch
  } else {
    jam();
    signal->theData[0] = DihContinueB::ZREAD_PAGES_INTO_FRAG;
    signal->theData[1] = rf->rwfTabPtr.i;
    signal->theData[2] = rf->fragId;
    signal->theData[3] = rf->pageIndex;
    signal->theData[4] = rf->wordIndex;
    sendSignal(reference(), GSN_CONTINUEB, signal, 5, JBB);
  }//if
  return;
}//Dbdih::readPagesIntoFragLab()

/*****************************************************************************/
/*****   WRITING FROM TABLE DATA STRUCTURES INTO A SET OF PAGES         ******/
// execCONTINUEB(ZPACK_TABLE_INTO_PAGES)
/*****************************************************************************/
void Dbdih::packTableIntoPagesLab(Signal* signal, Uint32 tableId) 
{
  RWFragment wf;
  TabRecordPtr tabPtr;
  allocpage(wf.rwfPageptr);
  tabPtr.i = tableId;
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
  tabPtr.p->pageRef[0] = wf.rwfPageptr.i;
  tabPtr.p->noPages = 1;
  wf.wordIndex = 35;
  wf.pageIndex = 0;
  writePageWord(&wf, tabPtr.p->totalfragments);
  writePageWord(&wf, tabPtr.p->noOfBackups);
  writePageWord(&wf, tabPtr.p->hashpointer);
  writePageWord(&wf, tabPtr.p->kvalue);
  writePageWord(&wf, tabPtr.p->mask);
  writePageWord(&wf, TabRecord::HASH);
  writePageWord(&wf, tabPtr.p->storedTable);

  signal->theData[0] = DihContinueB::ZPACK_FRAG_INTO_PAGES;
  signal->theData[1] = tabPtr.i;
  signal->theData[2] = 0;
  signal->theData[3] = wf.pageIndex;
  signal->theData[4] = wf.wordIndex;
  sendSignal(reference(), GSN_CONTINUEB, signal, 5, JBB);
}//Dbdih::packTableIntoPagesLab()

/*****************************************************************************/
// execCONTINUEB(ZPACK_FRAG_INTO_PAGES)
/*****************************************************************************/
void Dbdih::packFragIntoPagesLab(Signal* signal, RWFragment* wf) 
{
  ndbrequire(wf->pageIndex < 8);
  wf->rwfPageptr.i = wf->rwfTabPtr.p->pageRef[wf->pageIndex];
  ptrCheckGuard(wf->rwfPageptr, cpageFileSize, pageRecord);
  FragmentstorePtr fragPtr;
  getFragstore(wf->rwfTabPtr.p, wf->fragId, fragPtr);
  writeFragment(wf, fragPtr);
  writeReplicas(wf, fragPtr.p->storedReplicas);
  writeReplicas(wf, fragPtr.p->oldStoredReplicas);
  wf->fragId++;
  if (wf->fragId == wf->rwfTabPtr.p->totalfragments) {
    jam();
    PageRecordPtr pagePtr;
    pagePtr.i = wf->rwfTabPtr.p->pageRef[0];
    ptrCheckGuard(pagePtr, cpageFileSize, pageRecord);
    pagePtr.p->word[33] = wf->rwfTabPtr.p->noPages;
    pagePtr.p->word[34] = ((wf->rwfTabPtr.p->noPages - 1) * 2048) + wf->wordIndex;
    switch (wf->rwfTabPtr.p->tabCopyStatus) {
    case TabRecord::CS_SR_PHASE2_READ_TABLE:
      /* -------------------------------------------------------------------*/
      // We are performing a system restart and we are now ready to copy the
      // table from this node (the master) to all other nodes.
      /* -------------------------------------------------------------------*/
      jam();
      wf->rwfTabPtr.p->tabCopyStatus = TabRecord::CS_IDLE;
      signal->theData[0] = DihContinueB::ZSR_PHASE2_READ_TABLE;
      signal->theData[1] = wf->rwfTabPtr.i;
      sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
      return;
      break;
    case TabRecord::CS_COPY_NODE_STATE:
      jam();
      tableCopyNodeLab(signal, wf->rwfTabPtr);
      return;
      break;
    case TabRecord::CS_LCP_READ_TABLE:
      jam();
      signal->theData[0] = DihContinueB::ZTABLE_UPDATE;
      signal->theData[1] = wf->rwfTabPtr.i;
      sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
      return;
      break;
    case TabRecord::CS_REMOVE_NODE:
    case TabRecord::CS_INVALIDATE_NODE_LCP:
      jam();
      signal->theData[0] = DihContinueB::ZTABLE_UPDATE;
      signal->theData[1] = wf->rwfTabPtr.i;
      sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
      return;
      break;
    case TabRecord::CS_ADD_TABLE_MASTER:
      jam();
      wf->rwfTabPtr.p->tabCopyStatus = TabRecord::CS_IDLE;
      signal->theData[0] = DihContinueB::ZADD_TABLE_MASTER_PAGES;
      signal->theData[1] = wf->rwfTabPtr.i;
      sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
      return;
      break;
    case TabRecord::CS_ADD_TABLE_SLAVE:
      jam();
      wf->rwfTabPtr.p->tabCopyStatus = TabRecord::CS_IDLE;
      signal->theData[0] = DihContinueB::ZADD_TABLE_SLAVE_PAGES;
      signal->theData[1] = wf->rwfTabPtr.i;
      sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
      return;
      break;
    default:
      ndbrequire(false);
      return;
      break;
    }//switch
  } else {
    jam();
    signal->theData[0] = DihContinueB::ZPACK_FRAG_INTO_PAGES;
    signal->theData[1] = wf->rwfTabPtr.i;
    signal->theData[2] = wf->fragId;
    signal->theData[3] = wf->pageIndex;
    signal->theData[4] = wf->wordIndex;
    sendSignal(reference(), GSN_CONTINUEB, signal, 5, JBB);
  }//if
  return;
}//Dbdih::packFragIntoPagesLab()

/*****************************************************************************/
/* **********     START FRAGMENT MODULE                          *************/
/*****************************************************************************/
void Dbdih::startFragment(Signal* signal, Uint32 tableId, Uint32 fragId) 
{
  Uint32 TloopCount = 0;
  TabRecordPtr tabPtr;
  while (true) {
    if (TloopCount > 100) {
      jam();
      signal->theData[0] = DihContinueB::ZSTART_FRAGMENT;
      signal->theData[1] = tableId;
      signal->theData[2] = 0;
      sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBB);
      return;
    }
    
    if (tableId >= ctabFileSize) {
      jam();
      signal->theData[0] = DihContinueB::ZCOMPLETE_RESTART;
      sendSignal(reference(), GSN_CONTINUEB, signal, 1, JBB);
      return;
    }//if
    
    tabPtr.i = tableId;
    ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
    if (tabPtr.p->tabStatus != TabRecord::TS_ACTIVE){
      jam();
      TloopCount++;
      tableId++;
      fragId = 0;
      continue;
    }
    
    if(tabPtr.p->storedTable == 0){
      jam();
      TloopCount++;
      tableId++;
      fragId = 0;
      continue;
    }
    
    jam();
    break;
  }//while
  
  FragmentstorePtr fragPtr;
  getFragstore(tabPtr.p, fragId, fragPtr);
  /* ----------------------------------------------------------------------- */
  /*     WE NEED TO RESET THE REPLICA DATA STRUCTURES. THIS MEANS THAT WE    */
  /*     MUST REMOVE REPLICAS THAT WAS NOT STARTED AT THE GCI TO RESTORE. WE */
  /*     NEED TO PUT ALL STORED REPLICAS ON THE LIST OF OLD STORED REPLICAS  */
  /*     RESET THE NUMBER OF REPLICAS TO CREATE.                             */
  /* ----------------------------------------------------------------------- */
  cnoOfCreateReplicas = 0;
  /* ----------------------------------------------------------------------- */
  /*     WE WILL NEVER START MORE THAN FOUR FRAGMENT REPLICAS WHATEVER THE   */
  /*     DESIRED REPLICATION IS.                                             */
  /* ----------------------------------------------------------------------- */
  ndbrequire(tabPtr.p->noOfBackups < 4);
  /* ----------------------------------------------------------------------- */
  /*     SEARCH FOR STORED REPLICAS THAT CAN BE USED TO RESTART THE SYSTEM.  */
  /* ----------------------------------------------------------------------- */
  searchStoredReplicas(fragPtr);
  if (cnoOfCreateReplicas == 0) {
    /* --------------------------------------------------------------------- */
    /*   THERE WERE NO STORED REPLICAS AVAILABLE THAT CAN SERVE AS REPLICA TO*/
    /*   RESTART THE SYSTEM FROM. IN A LATER RELEASE WE WILL ADD             */
    /*   FUNCTIONALITY TO CHECK IF THERE ARE ANY STANDBY NODES THAT COULD DO */
    /*   THIS TASK INSTEAD IN THIS IMPLEMENTATION WE SIMPLY CRASH THE SYSTEM.*/
    /*   THIS WILL DECREASE THE GCI TO RESTORE WHICH HOPEFULLY WILL MAKE IT  */
    /*   POSSIBLE TO RESTORE THE SYSTEM.                                     */
    /* --------------------------------------------------------------------- */
    char buf[64];
    BaseString::snprintf(buf, sizeof(buf), "table: %d fragment: %d gci: %d",
			 tableId, fragId, SYSFILE->newestRestorableGCI);
    progError(__LINE__, NDBD_EXIT_NO_RESTORABLE_REPLICA, buf);
    ndbrequire(false);
    return;
  }//if
  
  /* ----------------------------------------------------------------------- */
  /*     WE HAVE CHANGED THE NODE TO BE PRIMARY REPLICA AND THE NODES TO BE  */
  /*     BACKUP NODES. WE MUST UPDATE THIS NODES DATA STRUCTURE SINCE WE     */
  /*     WILL NOT COPY THE TABLE DATA TO OURSELF.                            */
  /* ----------------------------------------------------------------------- */
  updateNodeInfo(fragPtr);
  /* ----------------------------------------------------------------------- */
  /*     NOW WE HAVE COLLECTED ALL THE REPLICAS WE COULD GET. WE WILL NOW    */
  /*     RESTART THE FRAGMENT REPLICAS WE HAVE FOUND IRRESPECTIVE OF IF THERE*/
  /*     ARE ENOUGH ACCORDING TO THE DESIRED REPLICATION.                    */
  /* ----------------------------------------------------------------------- */
  /*     WE START BY SENDING ADD_FRAGREQ FOR THOSE REPLICAS THAT NEED IT.    */
  /* ----------------------------------------------------------------------- */
  CreateReplicaRecordPtr createReplicaPtr;
  for (createReplicaPtr.i = 0; 
       createReplicaPtr.i < cnoOfCreateReplicas; 
       createReplicaPtr.i++) {
    jam();
    ptrCheckGuard(createReplicaPtr, 4, createReplicaRecord);
    createReplicaPtr.p->hotSpareUse = false;
  }//for

  sendStartFragreq(signal, tabPtr, fragId);

  /**
   * Don't wait for START_FRAGCONF
   */
  fragId++;
  if (fragId >= tabPtr.p->totalfragments) {
    jam();
    tabPtr.i++;
    fragId = 0;
  }//if
  signal->theData[0] = DihContinueB::ZSTART_FRAGMENT;
  signal->theData[1] = tabPtr.i;
  signal->theData[2] = fragId;
  sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBB);
  
  return;
}//Dbdih::startFragmentLab()


/*****************************************************************************/
/* **********     COMPLETE RESTART MODULE                        *************/
/*****************************************************************************/
void Dbdih::completeRestartLab(Signal* signal) 
{
  sendLoopMacro(START_RECREQ, sendSTART_RECREQ);
}//completeRestartLab()

/* ------------------------------------------------------------------------- */
//       SYSTEM RESTART:
/*         A NODE HAS COMPLETED RESTORING ALL DATABASE FRAGMENTS.            */
//       NODE RESTART:
//         THE STARTING NODE HAS PREPARED ITS LOG FILES TO ENABLE EXECUTION
//         OF TRANSACTIONS.
// Precondition:
//   This signal must be received by the master node.
/* ------------------------------------------------------------------------- */
void Dbdih::execSTART_RECCONF(Signal* signal) 
{
  jamEntry();
  Uint32 senderNodeId = signal->theData[0];
  ndbrequire(isMaster());
  if (getNodeState().startLevel >= NodeState::SL_STARTED){
    /* --------------------------------------------------------------------- */
    // Since our node is already up and running this must be a node restart.
    // This means that we should be the master node, 
    // otherwise we have a problem.
    /* --------------------------------------------------------------------- */
    jam();
    ndbrequire(senderNodeId == c_nodeStartMaster.startNode);
    nodeRestartStartRecConfLab(signal);
    return;
  } else {
    /* --------------------------------------------------------------------- */
    // This was the system restart case. We set the state indicating that the
    // node has completed restoration of all fragments.
    /* --------------------------------------------------------------------- */
    receiveLoopMacro(START_RECREQ, senderNodeId);

    signal->theData[0] = reference();
    sendSignal(cntrlblockref, GSN_NDB_STARTCONF, signal, 1, JBB);
    return;
  }//if
}//Dbdih::execSTART_RECCONF()

void Dbdih::copyNodeLab(Signal* signal, Uint32 tableId) 
{
  /* ----------------------------------------------------------------------- */
  // This code is executed by the master to assist a node restart in receiving
  // the data in the master.
  /* ----------------------------------------------------------------------- */
  Uint32 TloopCount = 0;

  if (!c_nodeStartMaster.activeState) {
    jam();
    /* --------------------------------------------------------------------- */
    // Obviously the node crashed in the middle of its node restart. We will 
    // stop this process simply by returning after resetting the wait indicator.
    /* ---------------------------------------------------------------------- */
    c_nodeStartMaster.wait = ZFALSE;
    return;
  }//if
  TabRecordPtr tabPtr;
  tabPtr.i = tableId;
  while (tabPtr.i < ctabFileSize) {
    ptrAss(tabPtr, tabRecord);
    if (tabPtr.p->tabStatus == TabRecord::TS_ACTIVE) {
      /* -------------------------------------------------------------------- */
      // The table is defined. We will start by packing the table into pages.
      // The tabCopyStatus indicates to the CONTINUEB(ZPACK_TABLE_INTO_PAGES)
      // who called it. After packing the table into page(s) it will be sent to 
      // the starting node by COPY_TABREQ signals. After returning from the 
      // starting node we will return to this subroutine and continue 
      // with the next table.
      /* -------------------------------------------------------------------- */
      ndbrequire(tabPtr.p->tabCopyStatus == TabRecord::CS_IDLE);
      tabPtr.p->tabCopyStatus = TabRecord::CS_COPY_NODE_STATE;
      signal->theData[0] = DihContinueB::ZPACK_TABLE_INTO_PAGES;
      signal->theData[1] = tabPtr.i;
      sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
      return;
    } else {
      jam();
      if (TloopCount > 100) {
	/* ------------------------------------------------------------------ */
	// Introduce real-time break after looping through 100 not copied tables
	/* ----------------------------------------------------------------- */
        jam();
        signal->theData[0] = DihContinueB::ZCOPY_NODE;
        signal->theData[1] = tabPtr.i + 1;
        sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
        return;
      } else {
        jam();
        TloopCount++;
        tabPtr.i++;
      }//if
    }//if
  }//while
  dihCopyCompletedLab(signal);
  return;
}//Dbdih::copyNodeLab()

void Dbdih::tableCopyNodeLab(Signal* signal, TabRecordPtr tabPtr) 
{
  /* ----------------------------------------------------------------------- */
  /*       COPY PAGES READ TO STARTING NODE.                                 */
  /* ----------------------------------------------------------------------- */
  if (!c_nodeStartMaster.activeState) {
    jam();
    releaseTabPages(tabPtr.i);
    c_nodeStartMaster.wait = ZFALSE;
    return;
  }//if
  NodeRecordPtr copyNodePtr;
  PageRecordPtr pagePtr;
  copyNodePtr.i = c_nodeStartMaster.startNode;
  ptrCheckGuard(copyNodePtr, MAX_NDB_NODES, nodeRecord);

  copyNodePtr.p->activeTabptr = tabPtr.i;
  pagePtr.i = tabPtr.p->pageRef[0];
  ptrCheckGuard(pagePtr, cpageFileSize, pageRecord);

  signal->theData[0] = DihContinueB::ZCOPY_TABLE_NODE;
  signal->theData[1] = tabPtr.i;
  signal->theData[2] = copyNodePtr.i;
  signal->theData[3] = 0;
  signal->theData[4] = 0;
  signal->theData[5] = pagePtr.p->word[34];
  sendSignal(reference(), GSN_CONTINUEB, signal, 6, JBB);
}//Dbdih::tableCopyNodeLab()

/* ------------------------------------------------------------------------- */
// execCONTINUEB(ZCOPY_TABLE)
// This routine is used to copy the table descriptions from the master to
// other nodes. It is used in the system restart to copy from master to all
// starting nodes.
/* ------------------------------------------------------------------------- */
void Dbdih::copyTableLab(Signal* signal, Uint32 tableId) 
{
  TabRecordPtr tabPtr;
  tabPtr.i = tableId;
  ptrAss(tabPtr, tabRecord);

  ndbrequire(tabPtr.p->tabCopyStatus == TabRecord::CS_IDLE);
  tabPtr.p->tabCopyStatus = TabRecord::CS_SR_PHASE2_READ_TABLE;
  signal->theData[0] = DihContinueB::ZPACK_TABLE_INTO_PAGES;
  signal->theData[1] = tabPtr.i;
  sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
  return;
}//Dbdih::copyTableLab()

/* ------------------------------------------------------------------------- */
// execCONTINUEB(ZSR_PHASE2_READ_TABLE)
/* ------------------------------------------------------------------------- */
void Dbdih::srPhase2ReadTableLab(Signal* signal, TabRecordPtr tabPtr) 
{
  /* ----------------------------------------------------------------------- */
  // We set the sendCOPY_TABREQState to ZACTIVE for all nodes since it is a long
  // process to send off all table descriptions. Thus we ensure that we do
  // not encounter race conditions where one node is completed before the
  // sending process is completed. This could lead to that we start off the
  // system before we actually finished all copying of table descriptions
  // and could lead to strange errors.
  /* ----------------------------------------------------------------------- */

  //sendLoopMacro(COPY_TABREQ, nullRoutine);

  breakCopyTableLab(signal, tabPtr, cfirstAliveNode);
  return;
}//Dbdih::srPhase2ReadTableLab()

/* ------------------------------------------------------------------------- */
/*       COPY PAGES READ TO ALL NODES.                                       */
/* ------------------------------------------------------------------------- */
void Dbdih::breakCopyTableLab(Signal* signal, TabRecordPtr tabPtr, Uint32 nodeId) 
{
  NodeRecordPtr nodePtr;
  nodePtr.i = nodeId;
  while (nodePtr.i != RNIL) {
    jam();
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
    if (nodePtr.i == getOwnNodeId()){
      jam();
      /* ------------------------------------------------------------------- */
      /* NOT NECESSARY TO COPY TO MY OWN NODE. I ALREADY HAVE THE PAGES.     */
      /* I DO HOWEVER NEED TO STORE THE TABLE DESCRIPTION ONTO DISK.         */
      /* ------------------------------------------------------------------- */
      /* IF WE ARE MASTER WE ONLY NEED TO SAVE THE TABLE ON DISK. WE ALREADY */
      /* HAVE THE TABLE DESCRIPTION IN THE DATA STRUCTURES.                  */
      // AFTER COMPLETING THE WRITE TO DISK THE MASTER WILL ALSO SEND
      // COPY_TABCONF AS ALL THE OTHER NODES.
      /* ------------------------------------------------------------------- */
      c_COPY_TABREQ_Counter.setWaitingFor(nodePtr.i);
      tabPtr.p->tabUpdateState = TabRecord::US_COPY_TAB_REQ;
      signal->theData[0] = DihContinueB::ZTABLE_UPDATE;
      signal->theData[1] = tabPtr.i;
      sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
      nodePtr.i = nodePtr.p->nextNode;
    } else {
      PageRecordPtr pagePtr;
      /* -------------------------------------------------------------------- */
      // RATHER THAN SENDING ALL COPY_TABREQ IN PARALLEL WE WILL SERIALISE THIS
      // ACTIVITY AND WILL THUS CALL breakCopyTableLab AGAIN WHEN COMPLETED THE
      // SENDING OF COPY_TABREQ'S.
      /* -------------------------------------------------------------------- */
      jam();
      tabPtr.p->tabCopyStatus = TabRecord::CS_SR_PHASE3_COPY_TABLE;
      pagePtr.i = tabPtr.p->pageRef[0];
      ptrCheckGuard(pagePtr, cpageFileSize, pageRecord);
      signal->theData[0] = DihContinueB::ZCOPY_TABLE_NODE;
      signal->theData[1] = tabPtr.i;
      signal->theData[2] = nodePtr.i;
      signal->theData[3] = 0;
      signal->theData[4] = 0;
      signal->theData[5] = pagePtr.p->word[34];
      sendSignal(reference(), GSN_CONTINUEB, signal, 6, JBB);
      return;
    }//if
  }//while
  /* ----------------------------------------------------------------------- */
  /*    WE HAVE NOW SENT THE TABLE PAGES TO ALL NODES. EXIT AND WAIT FOR ALL */
  /*    REPLIES.                                                             */
  /* ----------------------------------------------------------------------- */
  return;
}//Dbdih::breakCopyTableLab()

/* ------------------------------------------------------------------------- */
// execCONTINUEB(ZCOPY_TABLE_NODE)
/* ------------------------------------------------------------------------- */
void Dbdih::copyTableNode(Signal* signal, 
			  CopyTableNode* ctn, NodeRecordPtr nodePtr) 
{
  if (getNodeState().startLevel >= NodeState::SL_STARTED){
    /* --------------------------------------------------------------------- */
    // We are in the process of performing a node restart and are copying a
    // table description to a starting node. We will check that no nodes have
    // crashed in this process.
    /* --------------------------------------------------------------------- */
    if (!c_nodeStartMaster.activeState) {
      jam();
      /** ------------------------------------------------------------------
       * The starting node crashed. We will release table pages and stop this
       * copy process and allow new node restarts to start.
       * ------------------------------------------------------------------ */
      releaseTabPages(ctn->ctnTabPtr.i);
      c_nodeStartMaster.wait = ZFALSE;
      return;
    }//if
  }//if
  ndbrequire(ctn->pageIndex < 8);
  ctn->ctnPageptr.i = ctn->ctnTabPtr.p->pageRef[ctn->pageIndex];
  ptrCheckGuard(ctn->ctnPageptr, cpageFileSize, pageRecord);
  /**
   * If first page & firstWord reqinfo = 1 (first signal)
   */
  Uint32 reqinfo = (ctn->pageIndex == 0) && (ctn->wordIndex == 0);
  if(reqinfo == 1){
    c_COPY_TABREQ_Counter.setWaitingFor(nodePtr.i);
  }
  
  for (Uint32 i = 0; i < 16; i++) {
    jam();
    sendCopyTable(signal, ctn, calcDihBlockRef(nodePtr.i), reqinfo);
    reqinfo = 0;
    if (ctn->noOfWords <= 16) {
      jam();
      switch (ctn->ctnTabPtr.p->tabCopyStatus) {
      case TabRecord::CS_SR_PHASE3_COPY_TABLE:
	/* ------------------------------------------------------------------ */
	// We have copied the table description to this node. 
	// We will now proceed
	// with sending the table description to the next node in the node list.
	/* ------------------------------------------------------------------ */
        jam();
        ctn->ctnTabPtr.p->tabCopyStatus = TabRecord::CS_IDLE;
        breakCopyTableLab(signal, ctn->ctnTabPtr, nodePtr.p->nextNode);
        return;
        break;
      case TabRecord::CS_COPY_NODE_STATE:
        jam();
        ctn->ctnTabPtr.p->tabCopyStatus = TabRecord::CS_IDLE;
        return;
        break;
      default:
        ndbrequire(false);
        break;
      }//switch
    } else {
      jam();
      ctn->wordIndex += 16;
      if (ctn->wordIndex == 2048) {
        jam();
        ctn->wordIndex = 0;
        ctn->pageIndex++;
        ndbrequire(ctn->pageIndex < 8);
        ctn->ctnPageptr.i = ctn->ctnTabPtr.p->pageRef[ctn->pageIndex];
        ptrCheckGuard(ctn->ctnPageptr, cpageFileSize, pageRecord);
      }//if
      ctn->noOfWords -= 16;
    }//if
  }//for
  signal->theData[0] = DihContinueB::ZCOPY_TABLE_NODE;
  signal->theData[1] = ctn->ctnTabPtr.i;
  signal->theData[2] = nodePtr.i;
  signal->theData[3] = ctn->pageIndex;
  signal->theData[4] = ctn->wordIndex;
  signal->theData[5] = ctn->noOfWords;
  sendSignal(reference(), GSN_CONTINUEB, signal, 6, JBB);
}//Dbdih::copyTableNodeLab()

void Dbdih::sendCopyTable(Signal* signal, CopyTableNode* ctn,
                          BlockReference ref, Uint32 reqinfo) 
{
  signal->theData[0] = reference();
  signal->theData[1] = reqinfo;
  signal->theData[2] = ctn->ctnTabPtr.i;
  signal->theData[3] = ctn->ctnTabPtr.p->schemaVersion;
  signal->theData[4] = ctn->noOfWords;
  ndbrequire(ctn->wordIndex + 15 < 2048);
  MEMCOPY_NO_WORDS(&signal->theData[5], &ctn->ctnPageptr.p->word[ctn->wordIndex], 16);
  sendSignal(ref, GSN_COPY_TABREQ, signal, 21, JBB);
}//Dbdih::sendCopyTable()

void Dbdih::execCOPY_TABCONF(Signal* signal) 
{
  NodeRecordPtr nodePtr;
  jamEntry();
  nodePtr.i = signal->theData[0];
  Uint32 tableId = signal->theData[1];
  if (getNodeState().startLevel >= NodeState::SL_STARTED){
    /* --------------------------------------------------------------------- */
    // We are in the process of performing a node restart. Continue by copying
    // the next table to the starting node.
    /* --------------------------------------------------------------------- */
    jam();
    NodeRecordPtr nodePtr;
    nodePtr.i = signal->theData[0];
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
    c_COPY_TABREQ_Counter.clearWaitingFor(nodePtr.i);

    releaseTabPages(tableId);
    signal->theData[0] = DihContinueB::ZCOPY_NODE;
    signal->theData[1] = tableId + 1;
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
    return;
  } else {
    /* --------------------------------------------------------------------- */
    // We are in the process of performing a system restart. Check if all nodes
    // have saved the new table description to file and then continue with the
    // next table.
    /* --------------------------------------------------------------------- */
    receiveLoopMacro(COPY_TABREQ, nodePtr.i);
    /* --------------------------------------------------------------------- */
    /*   WE HAVE NOW COPIED TO ALL NODES. WE HAVE NOW COMPLETED RESTORING    */
    /*   THIS TABLE. CONTINUE WITH THE NEXT TABLE.                           */
    /*   WE NEED TO RELEASE THE PAGES IN THE TABLE IN THIS NODE HERE.        */
    /*   WE ALSO NEED TO CLOSE THE TABLE FILE.                               */
    /* --------------------------------------------------------------------- */
    releaseTabPages(tableId);

    TabRecordPtr tabPtr;
    tabPtr.i = tableId;
    ptrCheckGuard(tabPtr, ctabFileSize, tabRecord); 

    ConnectRecordPtr connectPtr;
    connectPtr.i = tabPtr.p->connectrec;
    ptrCheckGuard(connectPtr, cconnectFileSize, connectRecord); 
    
    sendAddFragreq(signal, connectPtr, tabPtr, 0);
    return;
  }//if
}//Dbdih::execCOPY_TABCONF()

/*
  3.13   L O C A L   C H E C K P O I N T  (M A S T E R)
  ****************************************************
  */
/*****************************************************************************/
/* **********     LOCAL-CHECK-POINT-HANDLING MODULE              *************/
/*****************************************************************************/
/* ------------------------------------------------------------------------- */
/*       IT IS TIME TO CHECK IF IT IS TIME TO START A LOCAL CHECKPOINT.      */
/*       WE WILL EITHER START AFTER 1 MILLION WORDS HAVE ARRIVED OR WE WILL  */
/*       EXECUTE AFTER ABOUT 16 MINUTES HAVE PASSED BY.                      */
/* ------------------------------------------------------------------------- */
void Dbdih::checkTcCounterLab(Signal* signal) 
{
  CRASH_INSERTION(7009);
  if (c_lcpState.lcpStatus != LCP_STATUS_IDLE) {
    ndbout << "lcpStatus = " << (Uint32) c_lcpState.lcpStatus;
    ndbout << "lcpStatusUpdatedPlace = " << 
      c_lcpState.lcpStatusUpdatedPlace << endl;
    ndbrequire(false);
    return;
  }//if
  c_lcpState.ctimer += 32;
  if ((c_nodeStartMaster.blockLcp == true) ||
      ((c_lcpState.lcpStartGcp + 1) > currentgcp)) {
    jam();
    /* --------------------------------------------------------------------- */
    // No reason to start juggling the states and checking for start of LCP if
    // we are blocked to start an LCP anyway.
    // We also block LCP start if we have not completed one global checkpoints
    // before starting another local checkpoint.
    /* --------------------------------------------------------------------- */
    signal->theData[0] = DihContinueB::ZCHECK_TC_COUNTER;
    signal->theData[1] = __LINE__;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 1 * 100, 2);
    return;
  }//if 
  c_lcpState.setLcpStatus(LCP_TCGET, __LINE__);
  
  c_lcpState.ctcCounter = c_lcpState.ctimer;
  sendLoopMacro(TCGETOPSIZEREQ, sendTCGETOPSIZEREQ);
}//Dbdih::checkTcCounterLab()

void Dbdih::checkLcpStart(Signal* signal, Uint32 lineNo)
{
  /* ----------------------------------------------------------------------- */
  // Verify that we are not attempting to start another instance of the LCP
  // when it is not alright to do so.
  /* ----------------------------------------------------------------------- */
  ndbrequire(c_lcpState.lcpStart == ZIDLE);
  c_lcpState.lcpStart = ZACTIVE;
  signal->theData[0] = DihContinueB::ZCHECK_TC_COUNTER;
  signal->theData[1] = lineNo;
  sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 1000, 2);
}//Dbdih::checkLcpStart()

/* ------------------------------------------------------------------------- */
/*TCGETOPSIZECONF          HOW MUCH OPERATION SIZE HAVE BEEN EXECUTED BY TC  */
/* ------------------------------------------------------------------------- */
void Dbdih::execTCGETOPSIZECONF(Signal* signal) 
{
  jamEntry();
  Uint32 senderNodeId = signal->theData[0];
  c_lcpState.ctcCounter += signal->theData[1];
  
  receiveLoopMacro(TCGETOPSIZEREQ, senderNodeId);

  ndbrequire(c_lcpState.lcpStatus == LCP_TCGET);
  ndbrequire(c_lcpState.lcpStart == ZACTIVE);
  /* ----------------------------------------------------------------------- */
  // We are not actively starting another LCP, still we receive this signal.
  // This is not ok.
  /* ---------------------------------------------------------------------- */
  /*    ALL TC'S HAVE RESPONDED NOW. NOW WE WILL CHECK IF ENOUGH OPERATIONS */
  /*    HAVE EXECUTED TO ENABLE US TO START A NEW LOCAL CHECKPOINT.         */
  /*    WHILE COPYING DICTIONARY AND DISTRIBUTION INFO TO A STARTING NODE   */
  /*    WE WILL ALSO NOT ALLOW THE LOCAL CHECKPOINT TO PROCEED.             */
  /*----------------------------------------------------------------------- */
  if (c_lcpState.immediateLcpStart == false) {
    if ((c_lcpState.ctcCounter < 
	 ((Uint32)1 << c_lcpState.clcpDelay)) ||
        (c_nodeStartMaster.blockLcp == true)) {
      jam();
      c_lcpState.setLcpStatus(LCP_STATUS_IDLE, __LINE__);

      signal->theData[0] = DihContinueB::ZCHECK_TC_COUNTER;
      signal->theData[1] = __LINE__;
      sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 1 * 100, 2);
      return;
    }//if
  }//if
  c_lcpState.lcpStart = ZIDLE;
  c_lcpState.immediateLcpStart = false;
  /* ----------------------------------------------------------------------- 
   * Now the initial lcp is started, 
   * we can reset the delay to its orginal value
   * --------------------------------------------------------------------- */
  CRASH_INSERTION(7010);
  /* ----------------------------------------------------------------------- */
  /*     IF MORE THAN 1 MILLION WORDS PASSED THROUGH THE TC'S THEN WE WILL   */
  /*     START A NEW LOCAL CHECKPOINT. CLEAR CTIMER. START CHECKPOINT        */
  /*     ACTIVITY BY CALCULATING THE KEEP GLOBAL CHECKPOINT.                 */
  // Also remember the current global checkpoint to ensure that we run at least
  // one global checkpoints between each local checkpoint that we start up.
  /* ----------------------------------------------------------------------- */
  c_lcpState.ctimer = 0;
  c_lcpState.keepGci = coldgcp;
  c_lcpState.lcpStartGcp = currentgcp;
  /* ----------------------------------------------------------------------- */
  /*       UPDATE THE NEW LATEST LOCAL CHECKPOINT ID.                        */
  /* ----------------------------------------------------------------------- */
  cnoOfActiveTables = 0;
  c_lcpState.setLcpStatus(LCP_CALCULATE_KEEP_GCI, __LINE__);
  c_lcpState.oldestRestorableGci = SYSFILE->oldestRestorableGCI;
  ndbrequire(((int)c_lcpState.oldestRestorableGci) > 0);

  if (ERROR_INSERTED(7011)) {
    signal->theData[0] = NDB_LE_LCPStoppedInCalcKeepGci;
    signal->theData[1] = 0;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);
    return;
  }//if
  signal->theData[0] = DihContinueB::ZCALCULATE_KEEP_GCI;
  signal->theData[1] = 0;  /* TABLE ID = 0          */
  signal->theData[2] = 0;  /* FRAGMENT ID = 0       */
  sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBB);
  return;
}//Dbdih::execTCGETOPSIZECONF()

/* ------------------------------------------------------------------------- */
/*       WE NEED TO CALCULATE THE OLDEST GLOBAL CHECKPOINT THAT WILL BE      */
/*       COMPLETELY RESTORABLE AFTER EXECUTING THIS LOCAL CHECKPOINT.        */
/* ------------------------------------------------------------------------- */
void Dbdih::calculateKeepGciLab(Signal* signal, Uint32 tableId, Uint32 fragId) 
{
  TabRecordPtr tabPtr;
  Uint32 TloopCount = 1;
  tabPtr.i = tableId;
  do {
    if (tabPtr.i >= ctabFileSize) {
      if (cnoOfActiveTables > 0) {
        jam();
        signal->theData[0] = DihContinueB::ZSTORE_NEW_LCP_ID;
        sendSignal(reference(), GSN_CONTINUEB, signal, 1, JBB);
        return;
      } else {
        jam();
	/* ------------------------------------------------------------------ */
	/* THERE ARE NO TABLES TO CHECKPOINT. WE STOP THE CHECKPOINT ALREADY  */
	/* HERE TO AVOID STRANGE PROBLEMS LATER.                              */
	/* ------------------------------------------------------------------ */
        c_lcpState.setLcpStatus(LCP_STATUS_IDLE, __LINE__);
        checkLcpStart(signal, __LINE__);
        return;
      }//if
    }//if
    ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
    if (tabPtr.p->tabStatus != TabRecord::TS_ACTIVE || 
	tabPtr.p->storedTable == 0) {
      if (TloopCount > 100) {
        jam();
        signal->theData[0] = DihContinueB::ZCALCULATE_KEEP_GCI;
        signal->theData[1] = tabPtr.i + 1;
        signal->theData[2] = 0;
        sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBB);
        return;
      } else {
        jam();
        TloopCount++;
        tabPtr.i++;
      }//if
    } else {
      jam();
      TloopCount = 0;
    }//if
  } while (TloopCount != 0);
  cnoOfActiveTables++;
  FragmentstorePtr fragPtr;
  getFragstore(tabPtr.p, fragId, fragPtr);
  checkKeepGci(fragPtr.p->storedReplicas);
  fragId++;
  if (fragId >= tabPtr.p->totalfragments) {
    jam();
    tabPtr.i++;
    fragId = 0;
  }//if
  signal->theData[0] = DihContinueB::ZCALCULATE_KEEP_GCI;
  signal->theData[1] = tabPtr.i;
  signal->theData[2] = fragId;
  sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBB);
  return;
}//Dbdih::calculateKeepGciLab()

/* ------------------------------------------------------------------------- */
/*       WE NEED TO STORE ON DISK THE FACT THAT WE ARE STARTING THIS LOCAL   */
/*       CHECKPOINT ROUND. THIS WILL INVALIDATE ALL THE LOCAL CHECKPOINTS    */
/*       THAT WILL EVENTUALLY BE OVERWRITTEN AS PART OF THIS LOCAL CHECKPOINT*/
/* ------------------------------------------------------------------------- */
void Dbdih::storeNewLcpIdLab(Signal* signal) 
{
  /***************************************************************************/
  // Report the event that a local checkpoint has started.
  /***************************************************************************/
  signal->theData[0] = NDB_LE_LocalCheckpointStarted; //Event type
  signal->theData[1] = SYSFILE->latestLCP_ID + 1;
  signal->theData[2] = c_lcpState.keepGci;
  signal->theData[3] = c_lcpState.oldestRestorableGci;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 4, JBB);
  
  signal->setTrace(TestOrd::TraceLocalCheckpoint);

  CRASH_INSERTION(7013);
  SYSFILE->keepGCI = c_lcpState.keepGci;
  //Uint32 lcpId = SYSFILE->latestLCP_ID;
  SYSFILE->latestLCP_ID++;
  SYSFILE->oldestRestorableGCI = c_lcpState.oldestRestorableGci;

  const Uint32 oldestRestorableGCI = SYSFILE->oldestRestorableGCI;
  //const Uint32 newestRestorableGCI = SYSFILE->newestRestorableGCI;
  //ndbrequire(newestRestorableGCI >= oldestRestorableGCI);

  Int32 val = oldestRestorableGCI;
  ndbrequire(val > 0);
  
  /* ----------------------------------------------------------------------- */
  /* SET BIT INDICATING THAT LOCAL CHECKPOINT IS ONGOING. THIS IS CLEARED    */
  /* AT THE END OF A LOCAL CHECKPOINT.                                       */
  /* ----------------------------------------------------------------------- */
  SYSFILE->setLCPOngoing(SYSFILE->systemRestartBits);
  /* ---------------------------------------------------------------------- */
  /*    CHECK IF ANY NODE MUST BE TAKEN OUT OF SERVICE AND REFILLED WITH    */
  /*    NEW FRESH DATA FROM AN ACTIVE NODE.                                 */
  /* ---------------------------------------------------------------------- */
  setLcpActiveStatusStart(signal);
  c_lcpState.setLcpStatus(LCP_COPY_GCI, __LINE__);
  //#ifdef VM_TRACE
  //  infoEvent("LocalCheckpoint %d started", SYSFILE->latestLCP_ID);
  //  signal->theData[0] = 7012;
  //  execDUMP_STATE_ORD(signal);
  //#endif
  
  copyGciLab(signal, CopyGCIReq::LOCAL_CHECKPOINT);
}//Dbdih::storeNewLcpIdLab()

void Dbdih::startLcpRoundLab(Signal* signal) {
  jam();

  Mutex mutex(signal, c_mutexMgr, c_startLcpMutexHandle);
  Callback c = { safe_cast(&Dbdih::startLcpMutex_locked), 0 };
  ndbrequire(mutex.lock(c));
}

void
Dbdih::startLcpMutex_locked(Signal* signal, Uint32 senderData, Uint32 retVal){
  jamEntry();
  ndbrequire(retVal == 0);
  
  StartLcpReq* req = (StartLcpReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->lcpId = SYSFILE->latestLCP_ID;
  req->participatingLQH = c_lcpState.m_participatingLQH;
  req->participatingDIH = c_lcpState.m_participatingDIH;
  sendLoopMacro(START_LCP_REQ, sendSTART_LCP_REQ);
}
void
Dbdih::sendSTART_LCP_REQ(Signal* signal, Uint32 nodeId){
  BlockReference ref = calcDihBlockRef(nodeId);
  sendSignal(ref, GSN_START_LCP_REQ, signal, StartLcpReq::SignalLength, JBB);
}

void
Dbdih::execSTART_LCP_CONF(Signal* signal){
  StartLcpConf * conf = (StartLcpConf*)signal->getDataPtr();
  
  Uint32 nodeId = refToNode(conf->senderRef);
  receiveLoopMacro(START_LCP_REQ, nodeId);  

  Mutex mutex(signal, c_mutexMgr, c_startLcpMutexHandle);
  Callback c = { safe_cast(&Dbdih::startLcpMutex_unlocked), 0 };
  mutex.unlock(c);
}

void
Dbdih::startLcpMutex_unlocked(Signal* signal, Uint32 data, Uint32 retVal){
  jamEntry();
  ndbrequire(retVal == 0);

  Mutex mutex(signal, c_mutexMgr, c_startLcpMutexHandle);
  mutex.release();
  
  CRASH_INSERTION(7014);
  c_lcpState.setLcpStatus(LCP_TC_CLOPSIZE, __LINE__);
  sendLoopMacro(TC_CLOPSIZEREQ, sendTC_CLOPSIZEREQ);
}

void Dbdih::execTC_CLOPSIZECONF(Signal* signal) {
  jamEntry();
  Uint32 senderNodeId = signal->theData[0];
  receiveLoopMacro(TC_CLOPSIZEREQ, senderNodeId);
  
  ndbrequire(c_lcpState.lcpStatus == LCP_TC_CLOPSIZE);
  /* ----------------------------------------------------------------------- */
  /*     ALL TC'S HAVE CLEARED THEIR OPERATION SIZE COUNTERS. NOW PROCEED BY */
  /*     STARTING THE LOCAL CHECKPOINT IN EACH LQH.                          */
  /* ----------------------------------------------------------------------- */
  c_lcpState.m_LAST_LCP_FRAG_ORD = c_lcpState.m_participatingLQH;

  CRASH_INSERTION(7015);
  c_lcpState.setLcpStatus(LCP_START_LCP_ROUND, __LINE__);
  startLcpRoundLoopLab(signal, 0, 0);
}//Dbdih::execTC_CLOPSIZECONF()

void Dbdih::startLcpRoundLoopLab(Signal* signal, 
				 Uint32 startTableId, Uint32 startFragId) 
{
  NodeRecordPtr nodePtr;
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    ptrAss(nodePtr, nodeRecord);
    if (nodePtr.p->nodeStatus == NodeRecord::ALIVE) {
      ndbrequire(nodePtr.p->noOfStartedChkpt == 0);
      ndbrequire(nodePtr.p->noOfQueuedChkpt == 0);
    }//if
  }//if
  c_lcpState.currentFragment.tableId = startTableId;
  c_lcpState.currentFragment.fragmentId = startFragId;
  startNextChkpt(signal);
}//Dbdih::startLcpRoundLoopLab()

void Dbdih::startNextChkpt(Signal* signal)
{
  Uint32 lcpId = SYSFILE->latestLCP_ID;

  NdbNodeBitmask busyNodes; 
  busyNodes.clear();
  const Uint32 lcpNodes = c_lcpState.m_participatingLQH.count();
  
  bool save = true;
  LcpState::CurrentFragment curr = c_lcpState.currentFragment;
  
  while (curr.tableId < ctabFileSize) {
    TabRecordPtr tabPtr;
    tabPtr.i = curr.tableId;
    ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
    if ((tabPtr.p->tabStatus != TabRecord::TS_ACTIVE) ||
        (tabPtr.p->tabLcpStatus != TabRecord::TLS_ACTIVE)) {
      curr.tableId++;
      curr.fragmentId = 0;
      continue;
    }//if
    
    FragmentstorePtr fragPtr;
    getFragstore(tabPtr.p, curr.fragmentId, fragPtr);
    
    ReplicaRecordPtr replicaPtr;
    for(replicaPtr.i = fragPtr.p->storedReplicas;
	replicaPtr.i != RNIL ;
	replicaPtr.i = replicaPtr.p->nextReplica){
      
      jam();
      ptrCheckGuard(replicaPtr, creplicaFileSize, replicaRecord);
      
      NodeRecordPtr nodePtr;
      nodePtr.i = replicaPtr.p->procNode;
      ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
      
      if (replicaPtr.p->lcpOngoingFlag &&
          replicaPtr.p->lcpIdStarted < lcpId) {
        jam();
	//-------------------------------------------------------------------
	// We have found a replica on a node that performs local checkpoint
	// that is alive and that have not yet been started.
	//-------------------------------------------------------------------

        if (nodePtr.p->noOfStartedChkpt < 2) {
          jam();
	  /**
	   * Send LCP_FRAG_ORD to LQH
	   */
	  
	  /**
	   * Mark the replica so with lcpIdStarted == true
	   */
          replicaPtr.p->lcpIdStarted = lcpId;

          Uint32 i = nodePtr.p->noOfStartedChkpt;
          nodePtr.p->startedChkpt[i].tableId = tabPtr.i;
          nodePtr.p->startedChkpt[i].fragId = curr.fragmentId;
          nodePtr.p->startedChkpt[i].replicaPtr = replicaPtr.i;
          nodePtr.p->noOfStartedChkpt = i + 1;

	  sendLCP_FRAG_ORD(signal, nodePtr.p->startedChkpt[i]);
        } else if (nodePtr.p->noOfQueuedChkpt < 2) {
          jam();
	  /**
	   * Put LCP_FRAG_ORD "in queue"
	   */
	  
	  /**
	   * Mark the replica so with lcpIdStarted == true
	   */
          replicaPtr.p->lcpIdStarted = lcpId;
	  
          Uint32 i = nodePtr.p->noOfQueuedChkpt;
          nodePtr.p->queuedChkpt[i].tableId = tabPtr.i;
          nodePtr.p->queuedChkpt[i].fragId = curr.fragmentId;
          nodePtr.p->queuedChkpt[i].replicaPtr = replicaPtr.i;
          nodePtr.p->noOfQueuedChkpt = i + 1;
        } else {
          jam();

	  if(save){
	    /**
	     * Stop increasing value on first that was "full"
	     */
	    c_lcpState.currentFragment = curr;
	    save = false;
	  }
	  
	  busyNodes.set(nodePtr.i);
	  if(busyNodes.count() == lcpNodes){
	    /**
	     * There were no possibility to start the local checkpoint 
	     * and it was not possible to queue it up. In this case we 
	     * stop the start of local checkpoints until the nodes with a 
	     * backlog have performed more checkpoints. We will return and 
	     * will not continue the process of starting any more checkpoints.
	     */
	    return;
	  }//if
	}//if
      }
    }//while
    curr.fragmentId++;
    if (curr.fragmentId >= tabPtr.p->totalfragments) {
      jam();
      curr.fragmentId = 0;
      curr.tableId++;
    }//if
  }//while
  
  sendLastLCP_FRAG_ORD(signal);
}//Dbdih::startNextChkpt()

void Dbdih::sendLastLCP_FRAG_ORD(Signal* signal)
{
  LcpFragOrd * const lcpFragOrd = (LcpFragOrd *)&signal->theData[0];
  lcpFragOrd->tableId = RNIL;
  lcpFragOrd->fragmentId = 0;
  lcpFragOrd->lcpId = SYSFILE->latestLCP_ID;
  lcpFragOrd->lcpNo = 0;
  lcpFragOrd->keepGci = c_lcpState.keepGci;
  lcpFragOrd->lastFragmentFlag = true;

  NodeRecordPtr nodePtr;
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    jam();
    ptrAss(nodePtr, nodeRecord);
    
    if(nodePtr.p->noOfQueuedChkpt == 0 &&
       nodePtr.p->noOfStartedChkpt == 0 &&
       c_lcpState.m_LAST_LCP_FRAG_ORD.isWaitingFor(nodePtr.i)){
      jam();

      CRASH_INSERTION(7028);
      
      /**
       * Nothing queued or started <=> Complete on that node
       *
       */
      c_lcpState.m_LAST_LCP_FRAG_ORD.clearWaitingFor(nodePtr.i);
      if(ERROR_INSERTED(7075)){
	continue;
      }
      BlockReference ref = calcLqhBlockRef(nodePtr.i);
      sendSignal(ref, GSN_LCP_FRAG_ORD, signal,LcpFragOrd::SignalLength, JBB);
    }
  }
  if(ERROR_INSERTED(7075)){
    if(c_lcpState.m_LAST_LCP_FRAG_ORD.done())
      CRASH_INSERTION(7075);
  }
}//Dbdih::sendLastLCP_FRAGORD()

/* ------------------------------------------------------------------------- */
/*       A FRAGMENT REPLICA HAS COMPLETED EXECUTING ITS LOCAL CHECKPOINT.    */
/*       CHECK IF ALL REPLICAS IN THE TABLE HAVE COMPLETED. IF SO STORE THE  */
/*       THE TABLE DISTRIBUTION ON DISK. ALSO SEND LCP_REPORT TO ALL OTHER   */
/*       NODES SO THAT THEY CAN STORE THE TABLE ONTO DISK AS WELL.           */
/* ------------------------------------------------------------------------- */
void Dbdih::execLCP_FRAG_REP(Signal* signal) 
{
  jamEntry();
  ndbrequire(c_lcpState.lcpStatus != LCP_STATUS_IDLE);
  
#if 0
  printLCP_FRAG_REP(stdout, 
		    signal->getDataPtr(),
		    signal->length(), number());
#endif  

  LcpFragRep * const lcpReport = (LcpFragRep *)&signal->theData[0];
  Uint32 nodeId = lcpReport->nodeId;
  Uint32 tableId = lcpReport->tableId;
  Uint32 fragId = lcpReport->fragId;
  
  jamEntry();
 
  CRASH_INSERTION2(7025, isMaster());
  CRASH_INSERTION2(7016, !isMaster());

  bool fromTimeQueue = (signal->senderBlockRef() == reference());

  TabRecordPtr tabPtr;
  tabPtr.i = tableId;
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
  if(tabPtr.p->tabCopyStatus != TabRecord::CS_IDLE) {
    jam();
    /*-----------------------------------------------------------------------*/
    // If the table is currently copied to disk we also 
    // stop already here to avoid strange half-way updates 
    // of the table data structures.
    /*-----------------------------------------------------------------------*/
    /*
      We need to send this signal without a delay since we have discovered
      that we have run out of space in the short time queue. This problem
      is very erunlikely to happen but it has and it results in a node crash. 
      This should be considered a "quick fix" and not a permanent solution. 
      A cleaner/better way would be to check the time queue if it is full or
      not before sending this signal.
    */
    sendSignal(reference(), GSN_LCP_FRAG_REP, signal, signal->length(), JBB);  
    /* Kept here for reference
       sendSignalWithDelay(reference(), GSN_LCP_FRAG_REP, 
       signal, 20, signal->length());
    */

    if(!fromTimeQueue){
      c_lcpState.noOfLcpFragRepOutstanding++;
    }    
    
    return;
  }//if
  
  if(fromTimeQueue){
    jam();
    
    ndbrequire(c_lcpState.noOfLcpFragRepOutstanding > 0);
    c_lcpState.noOfLcpFragRepOutstanding--;
  }

  bool tableDone = reportLcpCompletion(lcpReport);
  
  Uint32 started = lcpReport->maxGciStarted;
  Uint32 completed = lcpReport->maxGciCompleted;

  if(tableDone){
    jam();

    if(tabPtr.p->tabStatus == TabRecord::TS_DROPPING){
      jam();
      ndbout_c("TS_DROPPING - Neglecting to save Table: %d Frag: %d - ",
	       tableId,
	       fragId);
    } else {
      jam();
      /**
       * Write table description to file
       */
      tabPtr.p->tabLcpStatus = TabRecord::TLS_WRITING_TO_FILE;
      tabPtr.p->tabCopyStatus = TabRecord::CS_LCP_READ_TABLE;
      tabPtr.p->tabUpdateState = TabRecord::US_LOCAL_CHECKPOINT;
      signal->theData[0] = DihContinueB::ZPACK_TABLE_INTO_PAGES;
      signal->theData[1] = tabPtr.i;
      sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
      
      checkLcpAllTablesDoneInLqh();
    }
  }

#ifdef VM_TRACE
  /* --------------------------------------------------------------------- */
  // REPORT that local checkpoint have completed this fragment.
  /* --------------------------------------------------------------------- */
  signal->theData[0] = NDB_LE_LCPFragmentCompleted;
  signal->theData[1] = nodeId;
  signal->theData[2] = tableId;
  signal->theData[3] = fragId;
  signal->theData[4] = started;
  signal->theData[5] = completed;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 6, JBB);
#endif
  
  bool ok = false;
  switch(c_lcpMasterTakeOverState.state){
  case LMTOS_IDLE:
    ok = true;
    jam();
    /**
     * Fall through
     */
    break;
  case LMTOS_WAIT_EMPTY_LCP: // LCP Take over waiting for EMPTY_LCPCONF
    jam();
    return;
  case LMTOS_WAIT_LCP_FRAG_REP:
    jam();
    checkEmptyLcpComplete(signal);
    return;
  case LMTOS_INITIAL:
  case LMTOS_ALL_IDLE:
  case LMTOS_ALL_ACTIVE:
  case LMTOS_LCP_CONCLUDING:
  case LMTOS_COPY_ONGOING:
    ndbrequire(false);
  }
  ndbrequire(ok);
  
  /* ----------------------------------------------------------------------- */
  // Check if there are more LCP's to start up.
  /* ----------------------------------------------------------------------- */
  if(isMaster()){
    jam();

    /**
     * Remove from "running" array
     */
    NodeRecordPtr nodePtr;
    nodePtr.i = nodeId;
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
    
    const Uint32 outstanding = nodePtr.p->noOfStartedChkpt;
    ndbrequire(outstanding > 0);
    if(nodePtr.p->startedChkpt[0].tableId != tableId ||
       nodePtr.p->startedChkpt[0].fragId != fragId){
      jam();
      ndbrequire(outstanding > 1);
      ndbrequire(nodePtr.p->startedChkpt[1].tableId == tableId);
      ndbrequire(nodePtr.p->startedChkpt[1].fragId == fragId);
    } else {
      jam();
      nodePtr.p->startedChkpt[0] = nodePtr.p->startedChkpt[1];
    }
    nodePtr.p->noOfStartedChkpt--;
    checkStartMoreLcp(signal, nodeId);
  }
}

bool
Dbdih::checkLcpAllTablesDoneInLqh(){
  TabRecordPtr tabPtr;

  /**
   * Check if finished with all tables
   */
  for (tabPtr.i = 0; tabPtr.i < ctabFileSize; tabPtr.i++) {
    jam();
    ptrAss(tabPtr, tabRecord);
    if ((tabPtr.p->tabStatus == TabRecord::TS_ACTIVE) &&
        (tabPtr.p->tabLcpStatus == TabRecord::TLS_ACTIVE)) {
      jam();
      /**
       * Nope, not finished with all tables
       */
      return false;
    }//if
  }//for
  
  CRASH_INSERTION2(7026, isMaster());
  CRASH_INSERTION2(7017, !isMaster());
  
  c_lcpState.setLcpStatus(LCP_TAB_COMPLETED, __LINE__);
  return true;
}

void Dbdih::findReplica(ReplicaRecordPtr& replicaPtr, 
			Fragmentstore* fragPtrP, Uint32 nodeId)
{
  replicaPtr.i = fragPtrP->storedReplicas;
  while(replicaPtr.i != RNIL){
    ptrCheckGuard(replicaPtr, creplicaFileSize, replicaRecord);
    if (replicaPtr.p->procNode == nodeId) {
      jam();
      return;
    } else {
      jam();
      replicaPtr.i = replicaPtr.p->nextReplica;
    }//if
  };

#ifdef VM_TRACE
  ndbout_c("Fragment Replica(node=%d) not found", nodeId);
  replicaPtr.i = fragPtrP->oldStoredReplicas;
  while(replicaPtr.i != RNIL){
    ptrCheckGuard(replicaPtr, creplicaFileSize, replicaRecord);
    if (replicaPtr.p->procNode == nodeId) {
      jam();
      break;
    } else {
      jam();
      replicaPtr.i = replicaPtr.p->nextReplica;
    }//if
  };
  if(replicaPtr.i != RNIL){
    ndbout_c("...But was found in oldStoredReplicas");
  } else {
    ndbout_c("...And wasn't found in oldStoredReplicas");
  }
#endif
  ndbrequire(false);
}//Dbdih::findReplica()

/**
 * Return true  if table is all fragment replicas have been checkpointed
 *                 to disk (in all LQHs)
 *        false otherwise
 */
bool
Dbdih::reportLcpCompletion(const LcpFragRep* lcpReport)
{
  Uint32 lcpNo = lcpReport->lcpNo;
  Uint32 lcpId = lcpReport->lcpId;
  Uint32 maxGciStarted = lcpReport->maxGciStarted;
  Uint32 maxGciCompleted = lcpReport->maxGciCompleted;
  Uint32 tableId = lcpReport->tableId;
  Uint32 fragId = lcpReport->fragId;
  Uint32 nodeId = lcpReport->nodeId;

  TabRecordPtr tabPtr;
  tabPtr.i = tableId;
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
  
  FragmentstorePtr fragPtr;
  getFragstore(tabPtr.p, fragId, fragPtr);
  
  ReplicaRecordPtr replicaPtr;
  findReplica(replicaPtr, fragPtr.p, nodeId);
  
  ndbrequire(replicaPtr.p->lcpOngoingFlag == true);
  if(lcpNo != replicaPtr.p->nextLcp){
    ndbout_c("lcpNo = %d replicaPtr.p->nextLcp = %d", 
	     lcpNo, replicaPtr.p->nextLcp);
    ndbrequire(false);
  }
  ndbrequire(lcpNo == replicaPtr.p->nextLcp);
  ndbrequire(lcpNo < MAX_LCP_STORED);
  ndbrequire(replicaPtr.p->lcpId[lcpNo] != lcpId);
  
  replicaPtr.p->lcpIdStarted = lcpId;
  replicaPtr.p->lcpOngoingFlag = false;
  
  removeOldCrashedReplicas(replicaPtr);
  replicaPtr.p->lcpId[lcpNo] = lcpId;
  replicaPtr.p->lcpStatus[lcpNo] = ZVALID;
  replicaPtr.p->maxGciStarted[lcpNo] = maxGciStarted;
  gth(maxGciStarted + 1, 0);
  replicaPtr.p->maxGciCompleted[lcpNo] = maxGciCompleted;
  replicaPtr.p->nextLcp = nextLcpNo(replicaPtr.p->nextLcp);

  ndbrequire(fragPtr.p->noLcpReplicas > 0);
  fragPtr.p->noLcpReplicas --;
  
  if(fragPtr.p->noLcpReplicas > 0){
    jam();
    return false;
  }
  
  for (Uint32 fid = 0; fid < tabPtr.p->totalfragments; fid++) {
    jam();
    getFragstore(tabPtr.p, fid, fragPtr);
    if (fragPtr.p->noLcpReplicas > 0){
      jam();
      /* ----------------------------------------------------------------- */
      // Not all fragments in table have been checkpointed.
      /* ----------------------------------------------------------------- */
      if(0)
	ndbout_c("reportLcpCompletion: fragment %d not ready", fid);
      return false;
    }//if
  }//for
  return true;
}//Dbdih::reportLcpCompletion()

void Dbdih::checkStartMoreLcp(Signal* signal, Uint32 nodeId)
{
  ndbrequire(isMaster());

  NodeRecordPtr nodePtr;
  nodePtr.i = nodeId;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
  
  ndbrequire(nodePtr.p->noOfStartedChkpt < 2);
  
  if (nodePtr.p->noOfQueuedChkpt > 0) {
    jam();
    nodePtr.p->noOfQueuedChkpt--;
    Uint32 i = nodePtr.p->noOfStartedChkpt;
    nodePtr.p->startedChkpt[i] = nodePtr.p->queuedChkpt[0];
    nodePtr.p->queuedChkpt[0] = nodePtr.p->queuedChkpt[1];
    //-------------------------------------------------------------------
    // We can send a LCP_FRAGORD to the node ordering it to perform a
    // local checkpoint on this fragment replica.
    //-------------------------------------------------------------------
    nodePtr.p->noOfStartedChkpt = i + 1;
    
    sendLCP_FRAG_ORD(signal, nodePtr.p->startedChkpt[i]);
  }

  /* ----------------------------------------------------------------------- */
  // When there are no more outstanding LCP reports and there are no one queued
  // in at least one node, then we are ready to make sure all nodes have at
  // least two outstanding LCP requests per node and at least two queued for
  // sending.
  /* ----------------------------------------------------------------------- */
  startNextChkpt(signal);
}//Dbdih::checkStartMoreLcp()

void
Dbdih::sendLCP_FRAG_ORD(Signal* signal, 
			NodeRecord::FragmentCheckpointInfo info){ 
  
  ReplicaRecordPtr replicaPtr;
  replicaPtr.i = info.replicaPtr;
  ptrCheckGuard(replicaPtr, creplicaFileSize, replicaRecord);
  
  BlockReference ref = calcLqhBlockRef(replicaPtr.p->procNode);
  
  LcpFragOrd * const lcpFragOrd = (LcpFragOrd *)&signal->theData[0];
  lcpFragOrd->tableId    = info.tableId;
  lcpFragOrd->fragmentId = info.fragId;
  lcpFragOrd->lcpId      = SYSFILE->latestLCP_ID;
  lcpFragOrd->lcpNo      = replicaPtr.p->nextLcp;
  lcpFragOrd->keepGci    = c_lcpState.keepGci;
  lcpFragOrd->lastFragmentFlag = false;
  sendSignal(ref, GSN_LCP_FRAG_ORD, signal, LcpFragOrd::SignalLength, JBB);
}

void Dbdih::checkLcpCompletedLab(Signal* signal) 
{
  if(c_lcpState.lcpStatus < LCP_TAB_COMPLETED){
    jam();
    return;
  }
  
  TabRecordPtr tabPtr;
  for (tabPtr.i = 0; tabPtr.i < ctabFileSize; tabPtr.i++) {
    jam();
    ptrAss(tabPtr, tabRecord);
    if (tabPtr.p->tabStatus == TabRecord::TS_ACTIVE) {
      if (tabPtr.p->tabLcpStatus != TabRecord::TLS_COMPLETED) {
        jam();
        return;
      }//if
    }//if
  }//for

  CRASH_INSERTION2(7027, isMaster());
  CRASH_INSERTION2(7018, !isMaster());

  if(c_lcpState.lcpStatus == LCP_TAB_COMPLETED){
    /**
     * We'r done
     */
    c_lcpState.setLcpStatus(LCP_TAB_SAVED, __LINE__);
    sendLCP_COMPLETE_REP(signal);
    return;
  }

  ndbrequire(c_lcpState.lcpStatus == LCP_TAB_SAVED);
  allNodesLcpCompletedLab(signal);
  return;
}//Dbdih::checkLcpCompletedLab()

void
Dbdih::sendLCP_COMPLETE_REP(Signal* signal){
  jam();
  LcpCompleteRep * rep = (LcpCompleteRep*)signal->getDataPtrSend();
  rep->nodeId = getOwnNodeId();
  rep->lcpId = SYSFILE->latestLCP_ID;
  rep->blockNo = DBDIH;
  
  sendSignal(c_lcpState.m_masterLcpDihRef, GSN_LCP_COMPLETE_REP, signal, 
	     LcpCompleteRep::SignalLength, JBB);
}

/*-------------------------------------------------------------------------- */
/* COMP_LCP_ROUND                   A LQH HAS COMPLETED A LOCAL CHECKPOINT  */
/*------------------------------------------------------------------------- */
void Dbdih::execLCP_COMPLETE_REP(Signal* signal) 
{
  jamEntry();

#if 0
  ndbout_c("LCP_COMPLETE_REP"); 
  printLCP_COMPLETE_REP(stdout, 
			signal->getDataPtr(),
			signal->length(), number());
#endif

  LcpCompleteRep * rep = (LcpCompleteRep*)signal->getDataPtr();
  Uint32 lcpId = rep->lcpId;
  Uint32 nodeId = rep->nodeId;
  Uint32 blockNo = rep->blockNo;

  if(c_lcpMasterTakeOverState.state > LMTOS_WAIT_LCP_FRAG_REP){
    jam();
    /**
     * Don't allow LCP_COMPLETE_REP to arrive during
     * LCP master take over
     */
    ndbrequire(isMaster());
    ndbrequire(blockNo == DBDIH);
    sendSignalWithDelay(reference(), GSN_LCP_COMPLETE_REP, signal, 100, 
			signal->length());
    return;
  }

  ndbrequire(c_lcpState.lcpStatus != LCP_STATUS_IDLE);
  
  switch(blockNo){
  case DBLQH:
    jam();
    c_lcpState.m_LCP_COMPLETE_REP_Counter_LQH.clearWaitingFor(nodeId);
    ndbrequire(!c_lcpState.m_LAST_LCP_FRAG_ORD.isWaitingFor(nodeId));
    break;
  case DBDIH:
    jam();
    ndbrequire(isMaster());
    c_lcpState.m_LCP_COMPLETE_REP_Counter_DIH.clearWaitingFor(nodeId);
    break;
  case 0:
    jam();
    ndbrequire(!isMaster());
    ndbrequire(c_lcpState.m_LCP_COMPLETE_REP_From_Master_Received == false);
    c_lcpState.m_LCP_COMPLETE_REP_From_Master_Received = true;
    break;
  default:
    ndbrequire(false);
  }
  ndbrequire(lcpId == SYSFILE->latestLCP_ID);
  
  allNodesLcpCompletedLab(signal);
  return;
}

void Dbdih::allNodesLcpCompletedLab(Signal* signal)
{
  jam();
  
  if (c_lcpState.lcpStatus != LCP_TAB_SAVED) {
    jam();
    /**
     * We have not sent LCP_COMPLETE_REP to master DIH yet
     */
    return;
  }//if
  
  if (!c_lcpState.m_LCP_COMPLETE_REP_Counter_LQH.done()){
    jam();
    return;
  }

  if (!c_lcpState.m_LCP_COMPLETE_REP_Counter_DIH.done()){
    jam();
    return;
  }

  if (!isMaster() && 
      c_lcpState.m_LCP_COMPLETE_REP_From_Master_Received == false){
    jam();
    /**
     * Wait until master DIH has signaled lcp is complete
     */
    return;
  }

  if(c_lcpMasterTakeOverState.state != LMTOS_IDLE){
    jam();
#ifdef VM_TRACE
    ndbout_c("Exiting from allNodesLcpCompletedLab");
#endif
    return;
  }

  
  /*------------------------------------------------------------------------ */
  /*     WE HAVE NOW COMPLETED A LOCAL CHECKPOINT. WE ARE NOW READY TO WAIT  */
  /*     FOR THE NEXT LOCAL CHECKPOINT. SEND WITHOUT TIME-OUT SINCE IT MIGHT */
  /*     BE TIME TO START THE NEXT LOCAL CHECKPOINT IMMEDIATELY.             */
  /*     CLEAR BIT 3 OF SYSTEM RESTART BITS TO INDICATE THAT THERE IS NO     */
  /*     LOCAL CHECKPOINT ONGOING. THIS WILL BE WRITTEN AT SOME LATER TIME   */
  /*     DURING A GLOBAL CHECKPOINT. IT IS NOT NECESSARY TO WRITE IT         */
  /*     IMMEDIATELY. WE WILL ALSO CLEAR BIT 2 OF SYSTEM RESTART BITS IF ALL */
  /*     CURRENTLY ACTIVE NODES COMPLETED THE LOCAL CHECKPOINT.              */
  /*------------------------------------------------------------------------ */
  CRASH_INSERTION(7019);
  signal->setTrace(0);

  c_lcpState.setLcpStatus(LCP_STATUS_IDLE, __LINE__);
  setLcpActiveStatusEnd();
  Sysfile::clearLCPOngoing(SYSFILE->systemRestartBits);

  if(!isMaster()){
    jam();
    /**
     * We're not master, be content
     */
    return;
  }

  // Send LCP_COMPLETE_REP to all other nodes
  // allowing them to set their lcpStatus to LCP_STATUS_IDLE
  LcpCompleteRep * rep = (LcpCompleteRep*)signal->getDataPtrSend();
  rep->nodeId = getOwnNodeId();
  rep->lcpId = SYSFILE->latestLCP_ID;
  rep->blockNo = 0; // 0 = Sent from master
  
  NodeRecordPtr nodePtr;
  nodePtr.i = cfirstAliveNode;
  do {
    jam();
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);   
    if (nodePtr.i != cownNodeId){
      BlockReference ref = calcDihBlockRef(nodePtr.i);
      sendSignal(ref, GSN_LCP_COMPLETE_REP, signal, 
		 LcpCompleteRep::SignalLength, JBB); 
    }    
    nodePtr.i = nodePtr.p->nextNode;
  } while (nodePtr.i != RNIL);        

  
  jam();
  /***************************************************************************/
  // Report the event that a local checkpoint has completed.
  /***************************************************************************/
  signal->theData[0] = NDB_LE_LocalCheckpointCompleted; //Event type
  signal->theData[1] = SYSFILE->latestLCP_ID;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);
  
  /**
   * Start checking for next LCP
   */
  checkLcpStart(signal, __LINE__);
  
  if (cwaitLcpSr == true) {
    jam();
    cwaitLcpSr = false;
    ndbsttorry10Lab(signal, __LINE__);
    return;
  }//if
  
  if (c_nodeStartMaster.blockLcp == true) {
    jam();
    lcpBlockedLab(signal);
    return;
  }//if
  return;
}//Dbdih::allNodesLcpCompletedLab()

/******************************************************************************/
/* **********     TABLE UPDATE MODULE                             *************/
/* ****************************************************************************/
/* ------------------------------------------------------------------------- */
/*       THIS MODULE IS USED TO UPDATE THE TABLE DESCRIPTION. IT STARTS BY   */
/*       CREATING THE FIRST TABLE FILE, THEN UPDATES THIS FILE AND CLOSES IT.*/
/*       AFTER THAT THE SAME HAPPENS WITH THE SECOND FILE. AFTER THAT THE    */
/*       TABLE DISTRIBUTION HAS BEEN UPDATED.                                */
/*                                                                           */
/*       THE REASON FOR CREATING THE FILE AND NOT OPENING IT IS TO ENSURE    */
/*       THAT WE DO NOT GET A MIX OF OLD AND NEW INFORMATION IN THE FILE IN  */
/*       ERROR SITUATIONS.                                                   */
/* ------------------------------------------------------------------------- */
void Dbdih::tableUpdateLab(Signal* signal, TabRecordPtr tabPtr) {
  FileRecordPtr filePtr;
  filePtr.i = tabPtr.p->tabFile[0];
  ptrCheckGuard(filePtr, cfileFileSize, fileRecord);
  createFileRw(signal, filePtr);
  filePtr.p->reqStatus = FileRecord::TABLE_CREATE;
  return;
}//Dbdih::tableUpdateLab()

void Dbdih::tableCreateLab(Signal* signal, FileRecordPtr filePtr) 
{
  TabRecordPtr tabPtr;
  tabPtr.i = filePtr.p->tabRef;
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
  writeTabfile(signal, tabPtr.p, filePtr);
  filePtr.p->reqStatus = FileRecord::TABLE_WRITE;
  return;
}//Dbdih::tableCreateLab()

void Dbdih::tableWriteLab(Signal* signal, FileRecordPtr filePtr) 
{
  closeFile(signal, filePtr);
  filePtr.p->reqStatus = FileRecord::TABLE_CLOSE;
  return;
}//Dbdih::tableWriteLab()

void Dbdih::tableCloseLab(Signal* signal, FileRecordPtr filePtr) 
{
  TabRecordPtr tabPtr;
  tabPtr.i = filePtr.p->tabRef;
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
  if (filePtr.i == tabPtr.p->tabFile[0]) {
    jam();
    filePtr.i = tabPtr.p->tabFile[1];
    ptrCheckGuard(filePtr, cfileFileSize, fileRecord);
    createFileRw(signal, filePtr);
    filePtr.p->reqStatus = FileRecord::TABLE_CREATE;
    return;
  }//if
  switch (tabPtr.p->tabUpdateState) {
  case TabRecord::US_LOCAL_CHECKPOINT:
    jam();
    releaseTabPages(tabPtr.i);
    signal->theData[0] = DihContinueB::ZCHECK_LCP_COMPLETED;
    sendSignal(reference(), GSN_CONTINUEB, signal, 1, JBB);

    tabPtr.p->tabCopyStatus = TabRecord::CS_IDLE;
    tabPtr.p->tabUpdateState = TabRecord::US_IDLE;
    tabPtr.p->tabLcpStatus = TabRecord::TLS_COMPLETED;
    return;
    break;
  case TabRecord::US_REMOVE_NODE:
    jam();
    releaseTabPages(tabPtr.i);
    for (Uint32 fragId = 0; fragId < tabPtr.p->totalfragments; fragId++) {
      jam();
      FragmentstorePtr fragPtr;
      getFragstore(tabPtr.p, fragId, fragPtr);
      updateNodeInfo(fragPtr);
    }//for
    tabPtr.p->tabCopyStatus = TabRecord::CS_IDLE;
    tabPtr.p->tabUpdateState = TabRecord::US_IDLE;
    if (tabPtr.p->tabLcpStatus == TabRecord::TLS_WRITING_TO_FILE) {
      jam();
      tabPtr.p->tabLcpStatus = TabRecord::TLS_COMPLETED;
      signal->theData[0] = DihContinueB::ZCHECK_LCP_COMPLETED;
      sendSignal(reference(), GSN_CONTINUEB, signal, 1, JBB);
    }//if
    signal->theData[0] = DihContinueB::ZREMOVE_NODE_FROM_TABLE;
    signal->theData[1] = tabPtr.p->tabRemoveNode;
    signal->theData[2] = tabPtr.i + 1;
    sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBB);
    return;
    break;
  case TabRecord::US_INVALIDATE_NODE_LCP:
    jam();
    releaseTabPages(tabPtr.i);
    tabPtr.p->tabCopyStatus = TabRecord::CS_IDLE;
    tabPtr.p->tabUpdateState = TabRecord::US_IDLE;
    
    signal->theData[0] = DihContinueB::ZINVALIDATE_NODE_LCP;
    signal->theData[1] = tabPtr.p->tabRemoveNode;
    signal->theData[2] = tabPtr.i + 1;
    sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBB);
    return;
  case TabRecord::US_COPY_TAB_REQ:
    jam();
    tabPtr.p->tabUpdateState = TabRecord::US_IDLE;
    copyTabReq_complete(signal, tabPtr);
    return;
    break;
  case TabRecord::US_ADD_TABLE_MASTER:
    jam();
    releaseTabPages(tabPtr.i);
    tabPtr.p->tabUpdateState = TabRecord::US_IDLE;
    signal->theData[0] = DihContinueB::ZDIH_ADD_TABLE_MASTER;
    signal->theData[1] = tabPtr.i;
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
    return;
    break;
  case TabRecord::US_ADD_TABLE_SLAVE:
    jam();
    releaseTabPages(tabPtr.i);
    tabPtr.p->tabUpdateState = TabRecord::US_IDLE;
    signal->theData[0] = DihContinueB::ZDIH_ADD_TABLE_SLAVE;
    signal->theData[1] = tabPtr.i;
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
    return;
    break;
  default:
    ndbrequire(false);
    return;
    break;
  }//switch
}//Dbdih::tableCloseLab()

/**
 * GCP stop detected, 
 * send SYSTEM_ERROR to all other alive nodes
 */
void Dbdih::crashSystemAtGcpStop(Signal* signal)
{
  switch(cgcpStatus){
  case GCP_NODE_FINISHED:
  {
    /**
     * We're waiting for a GCP save conf
     */
    ndbrequire(!c_GCP_SAVEREQ_Counter.done());
    NodeReceiverGroup rg(DBLQH, c_GCP_SAVEREQ_Counter);
    signal->theData[0] = 2305;
    sendSignal(rg, GSN_DUMP_STATE_ORD, signal, 1, JBB);
    
    infoEvent("Detected GCP stop...sending kill to %s", 
	      c_GCP_SAVEREQ_Counter.getText());
    ndbout_c("Detected GCP stop...sending kill to %s", 
	     c_GCP_SAVEREQ_Counter.getText());
    return;
  }
  case GCP_SAVE_LQH_FINISHED:
    ndbout_c("m_copyReason: %d m_waiting: %d",
	     c_copyGCIMaster.m_copyReason,
	     c_copyGCIMaster.m_waiting);
    break;
  }
  
  ndbout_c("c_copyGCISlave: sender{Data, Ref} %d %x reason: %d nextWord: %d",
	   c_copyGCISlave.m_senderData,
	   c_copyGCISlave.m_senderRef,
	   c_copyGCISlave.m_copyReason,
	   c_copyGCISlave.m_expectedNextWord);

  FileRecordPtr file0Ptr;
  file0Ptr.i = crestartInfoFile[0];
  ptrCheckGuard(file0Ptr, cfileFileSize, fileRecord);
  FileRecordPtr file1Ptr;
  file1Ptr.i = crestartInfoFile[1];
  ptrCheckGuard(file1Ptr, cfileFileSize, fileRecord);

  ndbout_c("file[0] status: %d type: %d reqStatus: %d file1: %d %d %d",
	   file0Ptr.p->fileStatus, file0Ptr.p->fileType, file0Ptr.p->reqStatus,
	   file1Ptr.p->fileStatus, file1Ptr.p->fileType, file1Ptr.p->reqStatus
	   );

  signal->theData[0] = 404;
  signal->theData[1] = file0Ptr.p->fileRef;
  EXECUTE_DIRECT(NDBFS, GSN_DUMP_STATE_ORD, signal, 2);

  signal->theData[0] = 404;
  signal->theData[1] = file1Ptr.p->fileRef;
  EXECUTE_DIRECT(NDBFS, GSN_DUMP_STATE_ORD, signal, 2);

  ndbout_c("c_COPY_GCIREQ_Counter = %s", 
	   c_COPY_GCIREQ_Counter.getText());
  ndbout_c("c_COPY_TABREQ_Counter = %s", 
	   c_COPY_TABREQ_Counter.getText());
  ndbout_c("c_CREATE_FRAGREQ_Counter = %s", 
	   c_CREATE_FRAGREQ_Counter.getText());
  ndbout_c("c_DIH_SWITCH_REPLICA_REQ_Counter = %s", 
	   c_DIH_SWITCH_REPLICA_REQ_Counter.getText());
  ndbout_c("c_EMPTY_LCP_REQ_Counter = %s",c_EMPTY_LCP_REQ_Counter.getText());
  ndbout_c("c_END_TOREQ_Counter = %s", c_END_TOREQ_Counter.getText());
  ndbout_c("c_GCP_COMMIT_Counter = %s", c_GCP_COMMIT_Counter.getText());
  ndbout_c("c_GCP_PREPARE_Counter = %s", c_GCP_PREPARE_Counter.getText());
  ndbout_c("c_GCP_SAVEREQ_Counter = %s", c_GCP_SAVEREQ_Counter.getText());
  ndbout_c("c_INCL_NODEREQ_Counter = %s", c_INCL_NODEREQ_Counter.getText());
  ndbout_c("c_MASTER_GCPREQ_Counter = %s", 
	   c_MASTER_GCPREQ_Counter.getText());
  ndbout_c("c_MASTER_LCPREQ_Counter = %s", 
	   c_MASTER_LCPREQ_Counter.getText());
  ndbout_c("c_START_INFOREQ_Counter = %s", 
	   c_START_INFOREQ_Counter.getText());
  ndbout_c("c_START_RECREQ_Counter = %s", c_START_RECREQ_Counter.getText());
  ndbout_c("c_START_TOREQ_Counter = %s", c_START_TOREQ_Counter.getText());
  ndbout_c("c_STOP_ME_REQ_Counter = %s", c_STOP_ME_REQ_Counter.getText());
  ndbout_c("c_TC_CLOPSIZEREQ_Counter = %s", 
	   c_TC_CLOPSIZEREQ_Counter.getText());
  ndbout_c("c_TCGETOPSIZEREQ_Counter = %s", 
	   c_TCGETOPSIZEREQ_Counter.getText());
  ndbout_c("c_UPDATE_TOREQ_Counter = %s", c_UPDATE_TOREQ_Counter.getText());

  NodeRecordPtr nodePtr;
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    jam();
    ptrAss(nodePtr, nodeRecord);
    if (nodePtr.p->nodeStatus == NodeRecord::ALIVE) {
      jam();
      const BlockReference ref = 
	numberToRef(refToBlock(cntrlblockref), nodePtr.i);
      SystemError * const sysErr = (SystemError*)&signal->theData[0];
      sysErr->errorCode = SystemError::GCPStopDetected;
      sysErr->errorRef = reference();
      sysErr->data1 = cgcpStatus;
      sysErr->data2 = cgcpOrderBlocked;
      sendSignal(ref, GSN_SYSTEM_ERROR, signal, 
		 SystemError::SignalLength, JBA);
    }//if
  }//for
  return;
}//Dbdih::crashSystemAtGcpStop()

/*************************************************************************/
/*                                                                       */
/*       MODULE: ALLOCPAGE                                               */
/*       DESCRIPTION: THE SUBROUTINE IS CALLED WITH POINTER TO PAGE      */
/*                    RECORD. A PAGE  RECORD IS TAKEN FROM               */
/*                    THE FREE PAGE  LIST                                */
/*************************************************************************/
void Dbdih::allocpage(PageRecordPtr& pagePtr) 
{
  ndbrequire(cfirstfreepage != RNIL);
  pagePtr.i = cfirstfreepage;
  ptrCheckGuard(pagePtr, cpageFileSize, pageRecord);
  cfirstfreepage = pagePtr.p->nextfreepage;
  pagePtr.p->nextfreepage = RNIL;
}//Dbdih::allocpage()

/*************************************************************************/
/*                                                                       */
/*       MODULE: ALLOC_STORED_REPLICA                                    */
/*       DESCRIPTION: THE SUBROUTINE IS CALLED TO GET A REPLICA RECORD,  */
/*                    TO INITIALISE IT AND TO LINK IT INTO THE FRAGMENT  */
/*                    STORE RECORD. USED FOR STORED REPLICAS.            */
/*************************************************************************/
void Dbdih::allocStoredReplica(FragmentstorePtr fragPtr,
                               ReplicaRecordPtr& newReplicaPtr,
                               Uint32 nodeId) 
{
  Uint32 i;
  ReplicaRecordPtr arrReplicaPtr;
  ReplicaRecordPtr arrPrevReplicaPtr;

  seizeReplicaRec(newReplicaPtr);
  for (i = 0; i < MAX_LCP_STORED; i++) {
    newReplicaPtr.p->maxGciCompleted[i] = 0;
    newReplicaPtr.p->maxGciStarted[i] = 0;
    newReplicaPtr.p->lcpId[i] = 0;
    newReplicaPtr.p->lcpStatus[i] = ZINVALID;
  }//for
  newReplicaPtr.p->noCrashedReplicas = 0;
  newReplicaPtr.p->initialGci = currentgcp;
  for (i = 0; i < 8; i++) {
    newReplicaPtr.p->replicaLastGci[i] = (Uint32)-1;
    newReplicaPtr.p->createGci[i] = 0;
  }//for
  newReplicaPtr.p->createGci[0] = currentgcp;
  ndbrequire(currentgcp != 0xF1F1F1F1);
  newReplicaPtr.p->nextLcp = 0;
  newReplicaPtr.p->procNode = nodeId;
  newReplicaPtr.p->lcpOngoingFlag = false;
  newReplicaPtr.p->lcpIdStarted = 0;
  
  arrPrevReplicaPtr.i = RNIL;
  arrReplicaPtr.i = fragPtr.p->storedReplicas;
  while (arrReplicaPtr.i != RNIL) {
    jam();
    ptrCheckGuard(arrReplicaPtr, creplicaFileSize, replicaRecord);
    arrPrevReplicaPtr = arrReplicaPtr;
    arrReplicaPtr.i = arrReplicaPtr.p->nextReplica;
  }//while
  if (arrPrevReplicaPtr.i == RNIL) {
    jam();
    fragPtr.p->storedReplicas = newReplicaPtr.i;
  } else {
    jam();
    arrPrevReplicaPtr.p->nextReplica = newReplicaPtr.i;
  }//if
  fragPtr.p->noStoredReplicas++;
}//Dbdih::allocStoredReplica()

/*************************************************************************/
/*  CALCULATE HOW MANY HOT SPARES THAT ARE TO BE ASSIGNED IN THIS SYSTEM */
/*************************************************************************/
void Dbdih::calculateHotSpare() 
{
  Uint32 tchsTmp;
  Uint32 tchsNoNodes;

  switch (cnoReplicas) {
  case 1:
    jam();
    cnoHotSpare = 0;
    break;
  case 2:
  case 3:
  case 4:
    jam();
    if (csystemnodes > cnoReplicas) {
      jam();
      /* --------------------------------------------------------------------- */
      /*  WITH MORE NODES THAN REPLICAS WE WILL ALWAYS USE AT LEAST ONE HOT    */
      /*  SPARE IF THAT HAVE BEEN REQUESTED BY THE CONFIGURATION FILE. THE     */
      /*  NUMBER OF NODES TO BE USED FOR NORMAL OPERATION IS ALWAYS            */
      /*  A MULTIPLE OF THE NUMBER OF REPLICAS SINCE WE WILL ORGANISE NODES    */
      /*  INTO NODE GROUPS. THE REMAINING NODES WILL BE HOT SPARE NODES.       */
      /* --------------------------------------------------------------------- */
      if ((csystemnodes - cnoReplicas) >= cminHotSpareNodes) {
        jam();
	/* --------------------------------------------------------------------- */
	// We set the minimum number of hot spares according to users request
	// through the configuration file.
	/* --------------------------------------------------------------------- */
        tchsNoNodes = csystemnodes - cminHotSpareNodes;
        cnoHotSpare = cminHotSpareNodes;
      } else if (cminHotSpareNodes > 0) {
        jam();
	/* --------------------------------------------------------------------- */
	// The user requested at least one hot spare node and we will support him
	// in that.
	/* --------------------------------------------------------------------- */
        tchsNoNodes = csystemnodes - 1;
        cnoHotSpare = 1;
      } else {
        jam();
	/* --------------------------------------------------------------------- */
	// The user did not request any hot spare nodes so in this case we will
	// only use hot spare nodes if the number of nodes is such that we cannot
	// use all nodes as normal nodes.
	/* --------------------------------------------------------------------- */
        tchsNoNodes = csystemnodes;
        cnoHotSpare = 0;
      }//if
    } else {
      jam();
      /* --------------------------------------------------------------------- */
      // We only have enough to support the replicas. We will not have any hot
      // spares.
      /* --------------------------------------------------------------------- */
      tchsNoNodes = csystemnodes;
      cnoHotSpare = 0;
    }//if
    tchsTmp = tchsNoNodes - (cnoReplicas * (tchsNoNodes / cnoReplicas));
    cnoHotSpare = cnoHotSpare + tchsTmp;
    break;
  default:
    jam();
    ndbrequire(false);
    break;
  }//switch
}//Dbdih::calculateHotSpare()

/*************************************************************************/
/* CHECK IF THE NODE CRASH IS TO ESCALATE INTO A SYSTEM CRASH. WE COULD  */
/* DO THIS BECAUSE ALL REPLICAS OF SOME FRAGMENT ARE LOST. WE COULD ALSO */
/* DO IT AFTER MANY NODE FAILURES THAT MAKE IT VERY DIFFICULT TO RESTORE */
/* DATABASE AFTER A SYSTEM CRASH. IT MIGHT EVEN BE IMPOSSIBLE AND THIS   */
/* MUST BE AVOIDED EVEN MORE THAN AVOIDING SYSTEM CRASHES.               */
/*************************************************************************/
void Dbdih::checkEscalation() 
{
  Uint32 TnodeGroup[MAX_NDB_NODES];
  NodeRecordPtr nodePtr;
  Uint32 i;
  for (i = 0; i < MAX_NDB_NODES; i++) {
    TnodeGroup[i] = ZFALSE;
  }//for
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    jam();
    ptrAss(nodePtr, nodeRecord);
    if (nodePtr.p->nodeStatus == NodeRecord::ALIVE &&
	nodePtr.p->activeStatus == Sysfile::NS_Active){
      ndbrequire(nodePtr.p->nodeGroup < MAX_NDB_NODES);
      TnodeGroup[nodePtr.p->nodeGroup] = ZTRUE;
    }
  }
  for (i = 0; i < cnoOfNodeGroups; i++) {
    jam();
    if (TnodeGroup[i] == ZFALSE) {
      jam();
      progError(__LINE__, NDBD_EXIT_LOST_NODE_GROUP, "Lost node group");
    }//if
  }//for
}//Dbdih::checkEscalation()

/*************************************************************************/
/*                                                                       */
/*       MODULE: CHECK_KEEP_GCI                                          */
/*       DESCRIPTION: CHECK FOR MINIMUM GCI RESTORABLE WITH NEW LOCAL    */
/*                    CHECKPOINT.                                        */
/*************************************************************************/
void Dbdih::checkKeepGci(Uint32 replicaStartIndex) 
{
  ReplicaRecordPtr ckgReplicaPtr;
  ckgReplicaPtr.i = replicaStartIndex;
  while (ckgReplicaPtr.i != RNIL) {
    jam();
    ptrCheckGuard(ckgReplicaPtr, creplicaFileSize, replicaRecord);
    Uint32 keepGci;
    Uint32 oldestRestorableGci;
    findMinGci(ckgReplicaPtr, keepGci, oldestRestorableGci);
    if (keepGci < c_lcpState.keepGci) {
      jam();
      /* ------------------------------------------------------------------- */
      /* WE MUST KEEP LOG RECORDS SO THAT WE CAN USE ALL LOCAL CHECKPOINTS   */
      /* THAT ARE AVAILABLE. THUS WE NEED TO CALCULATE THE MINIMUM OVER ALL  */
      /* FRAGMENTS.                                                          */
      /* ------------------------------------------------------------------- */
      c_lcpState.keepGci = keepGci;
    }//if
    if (oldestRestorableGci > c_lcpState.oldestRestorableGci) {
      jam();
      c_lcpState.oldestRestorableGci = oldestRestorableGci;
      ndbrequire(((int)c_lcpState.oldestRestorableGci) >= 0);
    }//if
    ckgReplicaPtr.i = ckgReplicaPtr.p->nextReplica;
  }//while
}//Dbdih::checkKeepGci()

void Dbdih::closeFile(Signal* signal, FileRecordPtr filePtr) 
{
  signal->theData[0] = filePtr.p->fileRef;
  signal->theData[1] = reference();
  signal->theData[2] = filePtr.i;
  signal->theData[3] = ZCLOSE_NO_DELETE;
  sendSignal(NDBFS_REF, GSN_FSCLOSEREQ, signal, 4, JBA);
}//Dbdih::closeFile()

void Dbdih::closeFileDelete(Signal* signal, FileRecordPtr filePtr) 
{
  signal->theData[0] = filePtr.p->fileRef;
  signal->theData[1] = reference();
  signal->theData[2] = filePtr.i;
  signal->theData[3] = ZCLOSE_DELETE;
  sendSignal(NDBFS_REF, GSN_FSCLOSEREQ, signal, 4, JBA);
}//Dbdih::closeFileDelete()

void Dbdih::createFileRw(Signal* signal, FileRecordPtr filePtr) 
{
  signal->theData[0] = reference();
  signal->theData[1] = filePtr.i;
  signal->theData[2] = filePtr.p->fileName[0];
  signal->theData[3] = filePtr.p->fileName[1];
  signal->theData[4] = filePtr.p->fileName[2];
  signal->theData[5] = filePtr.p->fileName[3];
  signal->theData[6] = ZCREATE_READ_WRITE;
  sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, 7, JBA);
}//Dbdih::createFileRw()

void Dbdih::emptyverificbuffer(Signal* signal, bool aContinueB) 
{
  if(cfirstVerifyQueue == RNIL){
    jam();
    return;
  }//if
  ApiConnectRecordPtr localApiConnectptr;
  if(getBlockCommit() == false){
    jam();
    ndbrequire(cverifyQueueCounter > 0);
    cverifyQueueCounter--;
    localApiConnectptr.i = cfirstVerifyQueue;
    ptrCheckGuard(localApiConnectptr, capiConnectFileSize, apiConnectRecord);
    ndbrequire(localApiConnectptr.p->apiGci <= currentgcp);
    cfirstVerifyQueue = localApiConnectptr.p->nextApi;
    if (cfirstVerifyQueue == RNIL) {
      jam();
      ndbrequire(cverifyQueueCounter == 0);
      clastVerifyQueue = RNIL;
    }//if
    signal->theData[0] = localApiConnectptr.i;
    signal->theData[1] = currentgcp;
    sendSignal(clocaltcblockref, GSN_DIVERIFYCONF, signal, 2, JBB);
    if (aContinueB == true) {
      jam();
      //-----------------------------------------------------------------------
      // This emptying happened as part of a take-out process by continueb signals.
      // This ensures that we will empty the queue eventually. We will also empty
      // one item every time we insert one item to ensure that the list doesn't
      // grow when it is not blocked.
      //-----------------------------------------------------------------------
      signal->theData[0] = DihContinueB::ZEMPTY_VERIFY_QUEUE;
      sendSignal(reference(), GSN_CONTINUEB, signal, 1, JBB);
    }//if
  } else {
    jam();
    //-----------------------------------------------------------------------
    // We are blocked so it is no use in continuing the emptying of the
    // verify buffer. Whenever the block is removed the emptying will
    // restart.
    //-----------------------------------------------------------------------
  }  
  return;
}//Dbdih::emptyverificbuffer()

/*----------------------------------------------------------------*/
/*       FIND A FREE HOT SPARE IF AVAILABLE AND ALIVE.            */
/*----------------------------------------------------------------*/
Uint32 Dbdih::findHotSpare()
{
  NodeRecordPtr nodePtr;
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    jam();
    ptrAss(nodePtr, nodeRecord);
    if (nodePtr.p->nodeStatus == NodeRecord::ALIVE) {
      if (nodePtr.p->activeStatus == Sysfile::NS_HotSpare) {
        jam();
        return nodePtr.i;
      }//if
    }//if
  }//for
  return RNIL;
}//Dbdih::findHotSpare()

/*************************************************************************/
/*       FIND THE NODES FROM WHICH WE CAN EXECUTE THE LOG TO RESTORE THE */
/*       DATA NODE IN A SYSTEM RESTART.                                  */
/*************************************************************************/
bool Dbdih::findLogNodes(CreateReplicaRecord* createReplica,
                         FragmentstorePtr fragPtr,
                         Uint32 startGci,
                         Uint32 stopGci) 
{
  ConstPtr<ReplicaRecord> flnReplicaPtr;
  flnReplicaPtr.i = createReplica->replicaRec;
  ptrCheckGuard(flnReplicaPtr, creplicaFileSize, replicaRecord);
  /* --------------------------------------------------------------------- */
  /*       WE START BY CHECKING IF THE DATA NODE CAN HANDLE THE LOG ALL BY */
  /*       ITSELF. THIS IS THE DESIRED BEHAVIOUR. IF THIS IS NOT POSSIBLE  */
  /*       THEN WE SEARCH FOR THE BEST POSSIBLE NODES AMONG THE NODES THAT */
  /*       ARE PART OF THIS SYSTEM RESTART.                                */
  /*       THIS CAN ONLY BE HANDLED BY THE LAST CRASHED REPLICA.           */
  /*       The condition is that the replica was created before or at the  */
  /*       time of the starting gci, in addition it must have been alive   */
  /*       at the time of the stopping gci. This is checked by two         */
  /*       conditions, the first checks replicaLastGci and the second      */
  /*       checks that it is also smaller than the last gci the node was   */
  /*       involved in. This is necessary to check since createGci is set  */
  /*       Last + 1 and sometimes startGci = stopGci + 1 and in that case  */
  /*       it could happen that replicaLastGci is set to -1 with CreateGci */
  /*       set to LastGci + 1.                                             */
  /* --------------------------------------------------------------------- */
  arrGuard(flnReplicaPtr.p->noCrashedReplicas, 8);
  const Uint32 noCrashed = flnReplicaPtr.p->noCrashedReplicas;
  
  if (!(ERROR_INSERTED(7073) || ERROR_INSERTED(7074))&&
      (startGci >= flnReplicaPtr.p->createGci[noCrashed]) &&
      (stopGci <= flnReplicaPtr.p->replicaLastGci[noCrashed]) &&
      (stopGci <= SYSFILE->lastCompletedGCI[flnReplicaPtr.p->procNode])) {
    jam();
    /* --------------------------------------------------------------------- */
    /*       WE FOUND ALL THE LOG RECORDS NEEDED IN THE DATA NODE. WE WILL   */
    /*       USE THOSE.                                                      */
    /* --------------------------------------------------------------------- */
    createReplica->noLogNodes = 1;
    createReplica->logStartGci[0] = startGci;
    createReplica->logStopGci[0] = stopGci;
    createReplica->logNodeId[0] = flnReplicaPtr.p->procNode;
    return true;
  }//if
  Uint32 logNode = 0;
  do {
    Uint32 fblStopGci;
    jam();
    if(!findBestLogNode(createReplica,
			fragPtr,
			startGci,
			stopGci,
			logNode,
			fblStopGci)){
      jam();
      return false;
    }
       
    logNode++;
    if (fblStopGci >= stopGci) {
      jam();
      createReplica->noLogNodes = logNode;
      return true;
    }//if
    startGci = fblStopGci + 1;
    if (logNode >= 4) { // Why??
      jam();
      break;
    }//if
  } while (1);
  /* --------------------------------------------------------------------- */
  /*       IT WAS NOT POSSIBLE TO RESTORE THE REPLICA. THIS CAN EITHER BE  */
  /*       BECAUSE OF LACKING NODES OR BECAUSE OF A REALLY SERIOUS PROBLEM.*/
  /* --------------------------------------------------------------------- */
  return false;
}//Dbdih::findLogNodes()

/*************************************************************************/
/*       FIND THE BEST POSSIBLE LOG NODE TO EXECUTE THE LOG AS SPECIFIED */
/*       BY THE INPUT PARAMETERS. WE SCAN THROUGH ALL ALIVE REPLICAS.    */
/*       THIS MEANS STORED, OLD_STORED                                   */
/*************************************************************************/
bool
Dbdih::findBestLogNode(CreateReplicaRecord* createReplica,
		       FragmentstorePtr fragPtr,
		       Uint32 startGci,
		       Uint32 stopGci,
		       Uint32 logNode,
		       Uint32& fblStopGci) 
{
  ConstPtr<ReplicaRecord> fblFoundReplicaPtr;
  ConstPtr<ReplicaRecord> fblReplicaPtr;
  
  /* --------------------------------------------------------------------- */
  /*       WE START WITH ZERO AS FOUND TO ENSURE THAT FIRST HIT WILL BE    */
  /*       BETTER.                                                         */
  /* --------------------------------------------------------------------- */
  fblStopGci = 0;
  fblReplicaPtr.i = fragPtr.p->storedReplicas;
  while (fblReplicaPtr.i != RNIL) {
    jam();
    ptrCheckGuard(fblReplicaPtr, creplicaFileSize, replicaRecord);
    if (checkNodeAlive(fblReplicaPtr.p->procNode)) {
      jam();
      Uint32 fliStopGci = findLogInterval(fblReplicaPtr, startGci);
      if (fliStopGci > fblStopGci) {
        jam();
        fblStopGci = fliStopGci;
        fblFoundReplicaPtr = fblReplicaPtr;
      }//if
    }//if
    fblReplicaPtr.i = fblReplicaPtr.p->nextReplica;
  }//while
  fblReplicaPtr.i = fragPtr.p->oldStoredReplicas;
  while (fblReplicaPtr.i != RNIL) {
    jam();
    ptrCheckGuard(fblReplicaPtr, creplicaFileSize, replicaRecord);
    if (checkNodeAlive(fblReplicaPtr.p->procNode)) {
      jam();
      Uint32 fliStopGci = findLogInterval(fblReplicaPtr, startGci);
      if (fliStopGci > fblStopGci) {
        jam();
        fblStopGci = fliStopGci;
        fblFoundReplicaPtr = fblReplicaPtr;
      }//if
    }//if
    fblReplicaPtr.i = fblReplicaPtr.p->nextReplica;
  }//while
  if (fblStopGci != 0) {
    jam();
    ndbrequire(logNode < MAX_LOG_EXEC);
    createReplica->logNodeId[logNode] = fblFoundReplicaPtr.p->procNode;
    createReplica->logStartGci[logNode] = startGci;
    if (fblStopGci >= stopGci) {
      jam();
      createReplica->logStopGci[logNode] = stopGci;
    } else {
      jam();
      createReplica->logStopGci[logNode] = fblStopGci;
    }//if
  }//if

  return fblStopGci != 0;
}//Dbdih::findBestLogNode()

Uint32 Dbdih::findLogInterval(ConstPtr<ReplicaRecord> replicaPtr, 
			      Uint32 startGci)
{
  ndbrequire(replicaPtr.p->noCrashedReplicas <= 8);
  Uint32 loopLimit = replicaPtr.p->noCrashedReplicas + 1;
  for (Uint32 i = 0; i < loopLimit; i++) {
    jam();
    if (replicaPtr.p->createGci[i] <= startGci) {
      if (replicaPtr.p->replicaLastGci[i] >= startGci) {
        jam();
        return replicaPtr.p->replicaLastGci[i];
      }//if
    }//if
  }//for
  return 0;
}//Dbdih::findLogInterval()

/*************************************************************************/
/*                                                                       */
/*       MODULE: FIND THE MINIMUM GCI THAT THIS NODE HAS LOG RECORDS FOR.*/
/*************************************************************************/
void Dbdih::findMinGci(ReplicaRecordPtr fmgReplicaPtr,
                       Uint32& keepGci,
                       Uint32& oldestRestorableGci)
{
  Uint32 nextLcpNo;
  Uint32 lcpNo;
  for (Uint32 i = 0; i < MAX_LCP_STORED; i++) {
    jam();
    if ((fmgReplicaPtr.p->lcpStatus[i] == ZVALID) &&
        ((fmgReplicaPtr.p->lcpId[i] + MAX_LCP_STORED) <= (SYSFILE->latestLCP_ID + 1))) {
      jam();
      /*--------------------------------------------------------------------*/
      // We invalidate the checkpoint we are preparing to overwrite. 
      // The LCP id is still the old lcp id, 
      // this is the reason of comparing with lcpId + 1.
      /*---------------------------------------------------------------------*/
      fmgReplicaPtr.p->lcpStatus[i] = ZINVALID;
    }//if
  }//for
  keepGci = (Uint32)-1;
  oldestRestorableGci = 0;
  nextLcpNo = fmgReplicaPtr.p->nextLcp;
  lcpNo = fmgReplicaPtr.p->nextLcp;
  do {
    ndbrequire(lcpNo < MAX_LCP_STORED);
    if (fmgReplicaPtr.p->lcpStatus[lcpNo] == ZVALID &&
	fmgReplicaPtr.p->maxGciStarted[lcpNo] <= coldgcp)
    {
      jam();
      keepGci = fmgReplicaPtr.p->maxGciCompleted[lcpNo];
      oldestRestorableGci = fmgReplicaPtr.p->maxGciStarted[lcpNo];
      ndbrequire(((int)oldestRestorableGci) >= 0);      
      return;
    } else {
      jam();
      if (fmgReplicaPtr.p->createGci[0] == fmgReplicaPtr.p->initialGci) {
        jam();
	/*-------------------------------------------------------------------
	 * WE CAN STILL RESTORE THIS REPLICA WITHOUT ANY LOCAL CHECKPOINTS BY
	 * ONLY USING THE LOG. IF THIS IS NOT POSSIBLE THEN WE REPORT THE LAST
	 * VALID LOCAL CHECKPOINT AS THE MINIMUM GCI RECOVERABLE.
	 *-----------------------------------------------------------------*/
        keepGci = fmgReplicaPtr.p->createGci[0];
      }//if
    }//if
    lcpNo = prevLcpNo(lcpNo);
  } while (lcpNo != nextLcpNo);
  return;
}//Dbdih::findMinGci()

bool Dbdih::findStartGci(ConstPtr<ReplicaRecord> replicaPtr,
                         Uint32 stopGci,
                         Uint32& startGci,
                         Uint32& lcpNo) 
{
  lcpNo = replicaPtr.p->nextLcp;
  const Uint32 startLcpNo = lcpNo;
  do {
    lcpNo = prevLcpNo(lcpNo);
    ndbrequire(lcpNo < MAX_LCP_STORED);
    if (replicaPtr.p->lcpStatus[lcpNo] == ZVALID) {
      if (replicaPtr.p->maxGciStarted[lcpNo] < stopGci) {
        jam();
	/* ----------------------------------------------------------------- */
	/*   WE HAVE FOUND A USEFUL LOCAL CHECKPOINT THAT CAN BE USED FOR    */
	/*   RESTARTING THIS FRAGMENT REPLICA.                               */
	/* ----------------------------------------------------------------- */
        startGci = replicaPtr.p->maxGciCompleted[lcpNo] + 1;
        return true;
      } 
    }
  } while (lcpNo != startLcpNo);
  /* --------------------------------------------------------------------- */
  /*       NO VALID LOCAL CHECKPOINT WAS AVAILABLE. WE WILL ADD THE        */
  /*       FRAGMENT. THUS THE NEXT LCP MUST BE SET TO ZERO.                */
  /*       WE MUST EXECUTE THE LOG FROM THE INITIAL GLOBAL CHECKPOINT WHEN */
  /*       THE TABLE WAS CREATED.                                          */
  /* --------------------------------------------------------------------- */
  startGci = replicaPtr.p->initialGci;
  ndbrequire(replicaPtr.p->nextLcp == 0);
  return false;
}//Dbdih::findStartGci()

/**************************************************************************/
/* ---------------------------------------------------------------------- */
/*       FIND A TAKE OVER REPLICA WHICH IS TO BE STARTED OR COMMITTED WHEN*/
/*       TAKING OVER A FAILED NODE.                                       */
/* ---------------------------------------------------------------------- */
/*************************************************************************/
void Dbdih::findToReplica(TakeOverRecord* regTakeOver,
                          Uint32 replicaType,
                          FragmentstorePtr fragPtr,
                          ReplicaRecordPtr& ftrReplicaPtr)
{
  switch (replicaType) {
  case CreateFragReq::STORED:
  case CreateFragReq::COMMIT_STORED:
    /* ----------------------------------------------------------------------*/
    /* HERE WE SEARCH FOR STORED REPLICAS. THE REPLICA MUST BE STORED IN THE */
    /* SECTION FOR OLD STORED REPLICAS SINCE WE HAVE NOT TAKEN OVER YET.     */
    /* ----------------------------------------------------------------------*/
    ftrReplicaPtr.i = fragPtr.p->oldStoredReplicas;
    while (ftrReplicaPtr.i != RNIL) {
      ptrCheckGuard(ftrReplicaPtr, creplicaFileSize, replicaRecord);
      if (ftrReplicaPtr.p->procNode == regTakeOver->toStartingNode) {
        jam();
        return;
      } else {
        if (ftrReplicaPtr.p->procNode == regTakeOver->toFailedNode) {
          jam();
          return;
        } else {
          jam();
          ftrReplicaPtr.i = ftrReplicaPtr.p->nextReplica;
        }//if
      }//if
    }//while
    break;
  default:
    ndbrequire(false);
    break;
  }//switch
}//Dbdih::findToReplica()

void Dbdih::initCommonData()
{
  c_blockCommit = false;
  c_blockCommitNo = 0;
  c_createFragmentLock = RNIL;
  c_endToLock = RNIL;
  cfailurenr = 1;
  cfirstAliveNode = RNIL;
  cfirstDeadNode = RNIL;
  cfirstVerifyQueue = RNIL;
  cgckptflag = false;
  cgcpDelay = 0;
  cgcpMasterTakeOverState = GMTOS_IDLE; 
  cgcpOrderBlocked = 0;
  cgcpParticipantState = GCP_PARTICIPANT_READY;
  cgcpSameCounter = 0;
  cgcpStartCounter = 0;
  cgcpStatus = GCP_READY;

  clastVerifyQueue = RNIL;
  c_lcpMasterTakeOverState.set(LMTOS_IDLE, __LINE__);

  c_lcpState.clcpDelay = 0;
  c_lcpState.lcpStart = ZIDLE;
  c_lcpState.lcpStartGcp = 0;
  c_lcpState.setLcpStatus(LCP_STATUS_IDLE, __LINE__);
  c_lcpState.currentFragment.tableId = 0;
  c_lcpState.currentFragment.fragmentId = 0;
  c_lcpState.noOfLcpFragRepOutstanding = 0;
  c_lcpState.keepGci = 0;
  c_lcpState.oldestRestorableGci = 0;
  c_lcpState.ctcCounter = 0;
  c_lcpState.ctimer = 0;
  c_lcpState.immediateLcpStart = false;
  c_lcpState.m_MASTER_LCPREQ_Received = false;
    
  cmasterdihref = 0;
  cmasterNodeId = 0;
  cmasterState = MASTER_IDLE;
  cmasterTakeOverNode = 0;
  cnewgcp = 0;
  cnoHotSpare = 0;
  cnoOfActiveTables = 0;
  cnoOfNodeGroups = 0;
  cnoReplicas = 0;
  coldgcp = 0;
  coldGcpId = 0;
  coldGcpStatus = cgcpStatus;
  con_lineNodes = 0;
  creceivedfrag = 0;
  crestartGci = 0;
  crestartInfoFile[0] = RNIL;
  crestartInfoFile[1] = RNIL;
  cstartGcpNow = false;
  cstartPhase = 0;
  c_startToLock = RNIL;
  cstarttype = (Uint32)-1;
  csystemnodes = 0;
  c_updateToLock = RNIL;
  currentgcp = 0;
  cverifyQueueCounter = 0;
  cwaitLcpSr = false;

  nodeResetStart();
  c_nodeStartMaster.wait = ZFALSE;

  memset(&sysfileData[0], 0, sizeof(sysfileData));

  const ndb_mgm_configuration_iterator * p = 
    theConfiguration.getOwnConfigIterator();
  ndbrequire(p != 0);
  
  c_lcpState.clcpDelay = 20;
  ndb_mgm_get_int_parameter(p, CFG_DB_LCP_INTERVAL, &c_lcpState.clcpDelay);
  c_lcpState.clcpDelay = c_lcpState.clcpDelay > 31 ? 31 : c_lcpState.clcpDelay;
  
  cminHotSpareNodes = 0;
  //ndb_mgm_get_int_parameter(p, CFG_DB_MIN_HOT_SPARES, &cminHotSpareNodes);
  cminHotSpareNodes = cminHotSpareNodes > 2 ? 2 : cminHotSpareNodes;

  cnoReplicas = 1;
  ndb_mgm_get_int_parameter(p, CFG_DB_NO_REPLICAS, &cnoReplicas);
  cnoReplicas = cnoReplicas > 4 ? 4 : cnoReplicas;

  cgcpDelay = 2000;
  ndb_mgm_get_int_parameter(p, CFG_DB_GCP_INTERVAL, &cgcpDelay);
  cgcpDelay =  cgcpDelay > 60000 ? 60000 : (cgcpDelay < 10 ? 10 : cgcpDelay);
}//Dbdih::initCommonData()

void Dbdih::initFragstore(FragmentstorePtr fragPtr) 
{
  fragPtr.p->storedReplicas = RNIL;
  fragPtr.p->oldStoredReplicas = RNIL;
  
  fragPtr.p->noStoredReplicas = 0;
  fragPtr.p->noOldStoredReplicas = 0;
  fragPtr.p->fragReplicas = 0;
  fragPtr.p->preferredPrimary = 0;

  for (Uint32 i = 0; i < MAX_REPLICAS; i++)
    fragPtr.p->activeNodes[i] = 0;
  
  fragPtr.p->noLcpReplicas = 0;
  fragPtr.p->distributionKey = 0;
}//Dbdih::initFragstore()

/*************************************************************************/
/*                                                                       */
/*       MODULE: INIT_RESTART_INFO                                       */
/*       DESCRIPTION: INITIATE RESTART INFO VARIABLE AND VARIABLES FOR   */
/*                    GLOBAL CHECKPOINTS.                                */
/*************************************************************************/
void Dbdih::initRestartInfo() 
{
  Uint32 i;
  for (i = 0; i < MAX_NDB_NODES; i++) {
    SYSFILE->lastCompletedGCI[i] = 0;
  }//for
  NodeRecordPtr nodePtr;
  nodePtr.i = cfirstAliveNode;
  do {
    jam();
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
    SYSFILE->lastCompletedGCI[nodePtr.i] = 1;
    /* FIRST GCP = 1 ALREADY SET BY LQH */
    nodePtr.i = nodePtr.p->nextNode;
  } while (nodePtr.i != RNIL);
  coldgcp = 1;
  currentgcp = 2;
  cnewgcp = 2;
  crestartGci = 1;

  SYSFILE->keepGCI             = 1;
  SYSFILE->oldestRestorableGCI = 1;
  SYSFILE->newestRestorableGCI = 1;
  SYSFILE->systemRestartBits   = 0;
  for (i = 0; i < NodeBitmask::Size; i++) {
    SYSFILE->lcpActive[0]        = 0;
  }//for  
  for (i = 0; i < Sysfile::TAKE_OVER_SIZE; i++) {
    SYSFILE->takeOver[i] = 0;
  }//for
  Sysfile::setInitialStartOngoing(SYSFILE->systemRestartBits);
}//Dbdih::initRestartInfo()

/*--------------------------------------------------------------------*/
/*       NODE GROUP BITS ARE INITIALISED BEFORE THIS.                 */
/*       NODE ACTIVE BITS ARE INITIALISED BEFORE THIS.                */
/*--------------------------------------------------------------------*/
/*************************************************************************/
/*                                                                       */
/*       MODULE: INIT_RESTORABLE_GCI_FILES                               */
/*       DESCRIPTION: THE SUBROUTINE SETS UP THE FILES THAT REFERS TO THE*/
/*       FILES THAT KEEP THE VARIABLE CRESTART_INFO                      */
/*************************************************************************/
void Dbdih::initRestorableGciFiles() 
{
  Uint32 tirgTmp;
  FileRecordPtr filePtr;
  seizeFile(filePtr);
  filePtr.p->tabRef = RNIL;
  filePtr.p->fileType = FileRecord::GCP_FILE;
  filePtr.p->reqStatus = FileRecord::IDLE;
  filePtr.p->fileStatus = FileRecord::CLOSED;
  crestartInfoFile[0] = filePtr.i;
  filePtr.p->fileName[0] = (Uint32)-1;  /* T DIRECTORY NOT USED  */
  filePtr.p->fileName[1] = (Uint32)-1;  /* F DIRECTORY NOT USED  */
  filePtr.p->fileName[2] = (Uint32)-1;  /* S PART IGNORED        */
  tirgTmp = 1;  /* FILE NAME VERSION 1   */
  tirgTmp = (tirgTmp << 8) + 6; /* .SYSFILE              */
  tirgTmp = (tirgTmp << 8) + 1; /* D1 DIRECTORY          */
  tirgTmp = (tirgTmp << 8) + 0; /* P0 FILE NAME          */
  filePtr.p->fileName[3] = tirgTmp;
  /* --------------------------------------------------------------------- */
  /*       THE NAME BECOMES /D1/DBDICT/S0.SYSFILE                          */
  /* --------------------------------------------------------------------- */
  seizeFile(filePtr);
  filePtr.p->tabRef = RNIL;
  filePtr.p->fileType = FileRecord::GCP_FILE;
  filePtr.p->reqStatus = FileRecord::IDLE;
  filePtr.p->fileStatus = FileRecord::CLOSED;
  crestartInfoFile[1] = filePtr.i;
  filePtr.p->fileName[0] = (Uint32)-1;  /* T DIRECTORY NOT USED  */
  filePtr.p->fileName[1] = (Uint32)-1;  /* F DIRECTORY NOT USED  */
  filePtr.p->fileName[2] = (Uint32)-1;  /* S PART IGNORED        */
  tirgTmp = 1;  /* FILE NAME VERSION 1   */
  tirgTmp = (tirgTmp << 8) + 6; /* .SYSFILE              */
  tirgTmp = (tirgTmp << 8) + 2; /* D1 DIRECTORY          */
  tirgTmp = (tirgTmp << 8) + 0; /* P0 FILE NAME          */
  filePtr.p->fileName[3] = tirgTmp;
  /* --------------------------------------------------------------------- */
  /*       THE NAME BECOMES /D2/DBDICT/P0.SYSFILE                          */
  /* --------------------------------------------------------------------- */
}//Dbdih::initRestorableGciFiles()

void Dbdih::initTable(TabRecordPtr tabPtr)
{
  tabPtr.p->noOfFragChunks = 0;
  tabPtr.p->method = TabRecord::NOTDEFINED;
  tabPtr.p->tabStatus = TabRecord::TS_IDLE;
  tabPtr.p->noOfWords = 0;
  tabPtr.p->noPages = 0;
  tabPtr.p->tabLcpStatus = TabRecord::TLS_COMPLETED;
  tabPtr.p->tabCopyStatus = TabRecord::CS_IDLE;
  tabPtr.p->tabUpdateState = TabRecord::US_IDLE;
  tabPtr.p->noOfBackups = 0;
  tabPtr.p->kvalue = 0;
  tabPtr.p->hashpointer = (Uint32)-1;
  tabPtr.p->mask = 0;
  tabPtr.p->storedTable = 1;
  tabPtr.p->tabErrorCode = 0;
  tabPtr.p->schemaVersion = (Uint32)-1;
  tabPtr.p->tabRemoveNode = RNIL;
  tabPtr.p->totalfragments = (Uint32)-1;
  tabPtr.p->connectrec = RNIL;
  tabPtr.p->tabFile[0] = RNIL;
  tabPtr.p->tabFile[1] = RNIL;
  tabPtr.p->m_dropTab.tabUserRef = 0;
  tabPtr.p->m_dropTab.tabUserPtr = RNIL;
  Uint32 i;
  for (i = 0; i < MAX_NDB_NODES; i++) {
    tabPtr.p->startFid[i] = RNIL;
  }//for
  for (i = 0; i < 8; i++) {
    tabPtr.p->pageRef[i] = RNIL;
  }//for
  tabPtr.p->tableType = DictTabInfo::UndefTableType;
}//Dbdih::initTable()

/*************************************************************************/
/*                                                                       */
/*       MODULE: INIT_TABLE_FILES                                        */
/*       DESCRIPTION: THE SUBROUTINE SETS UP THE FILES THAT REFERS TO THE*/
/*       FILES THAT KEEP THE TABLE FRAGMENTATION DESCRIPTION.            */
/*************************************************************************/
void Dbdih::initTableFile(TabRecordPtr tabPtr)
{
  Uint32 titfTmp;
  FileRecordPtr filePtr;
  seizeFile(filePtr);
  filePtr.p->tabRef = tabPtr.i;
  filePtr.p->fileType = FileRecord::TABLE_FILE;
  filePtr.p->reqStatus = FileRecord::IDLE;
  filePtr.p->fileStatus = FileRecord::CLOSED;
  tabPtr.p->tabFile[0] = filePtr.i;
  filePtr.p->fileName[0] = (Uint32)-1;  /* T DIRECTORY NOT USED  */
  filePtr.p->fileName[1] = (Uint32)-1;  /* F DIRECTORY NOT USED  */
  filePtr.p->fileName[2] = tabPtr.i;    /* Stid FILE NAME        */
  titfTmp = 1;  /* FILE NAME VERSION 1   */
  titfTmp = (titfTmp << 8) + 3; /* .FRAGLIST             */
  titfTmp = (titfTmp << 8) + 1; /* D1 DIRECTORY          */
  titfTmp = (titfTmp << 8) + 255;       /* P PART IGNORED        */
  filePtr.p->fileName[3] = titfTmp;
  /* --------------------------------------------------------------------- */
  /*       THE NAME BECOMES /D1/DBDICT/Stid.FRAGLIST                       */
  /* --------------------------------------------------------------------- */
  seizeFile(filePtr);
  filePtr.p->tabRef = tabPtr.i;
  filePtr.p->fileType = FileRecord::TABLE_FILE;
  filePtr.p->reqStatus = FileRecord::IDLE;
  filePtr.p->fileStatus = FileRecord::CLOSED;
  tabPtr.p->tabFile[1] = filePtr.i;
  filePtr.p->fileName[0] = (Uint32)-1;  /* T DIRECTORY NOT USED  */
  filePtr.p->fileName[1] = (Uint32)-1;  /* F DIRECTORY NOT USED  */
  filePtr.p->fileName[2] = tabPtr.i;    /* Stid FILE NAME        */
  titfTmp = 1;  /* FILE NAME VERSION 1   */
  titfTmp = (titfTmp << 8) + 3; /* .FRAGLIST             */
  titfTmp = (titfTmp << 8) + 2; /* D2 DIRECTORY          */
  titfTmp = (titfTmp << 8) + 255;       /* P PART IGNORED        */
  filePtr.p->fileName[3] = titfTmp;
  /* --------------------------------------------------------------------- */
  /*       THE NAME BECOMES /D2/DBDICT/Stid.FRAGLIST                       */
  /* --------------------------------------------------------------------- */
}//Dbdih::initTableFile()

void Dbdih::initialiseRecordsLab(Signal* signal, 
				 Uint32 stepNo, Uint32 retRef, Uint32 retData) 
{
  switch (stepNo) {
  case 0:
    jam();
    initCommonData();
    break;
  case 1:{
    ApiConnectRecordPtr apiConnectptr;
    jam();
    /******** INTIALIZING API CONNECT RECORDS ********/
    for (apiConnectptr.i = 0; apiConnectptr.i < capiConnectFileSize; apiConnectptr.i++) {
      refresh_watch_dog();
      ptrAss(apiConnectptr, apiConnectRecord);
      apiConnectptr.p->nextApi = RNIL;
    }//for
    jam();
    break;
  }
  case 2:{
    ConnectRecordPtr connectPtr;
    jam();
    /****** CONNECT ******/
    for (connectPtr.i = 0; connectPtr.i < cconnectFileSize; connectPtr.i++) {
      refresh_watch_dog();
      ptrAss(connectPtr, connectRecord);
      connectPtr.p->userpointer = RNIL;
      connectPtr.p->userblockref = ZNIL;
      connectPtr.p->connectState = ConnectRecord::FREE;
      connectPtr.p->table = RNIL;
      connectPtr.p->nfConnect = connectPtr.i + 1;
    }//for
    connectPtr.i = cconnectFileSize - 1;
    ptrAss(connectPtr, connectRecord);
    connectPtr.p->nfConnect = RNIL;
    cfirstconnect = 0;
    break;
  }
  case 3:
    {
      FileRecordPtr filePtr;
      jam();
      /******** INTIALIZING FILE RECORDS ********/
      for (filePtr.i = 0; filePtr.i < cfileFileSize; filePtr.i++) {
	ptrAss(filePtr, fileRecord);
	filePtr.p->nextFile = filePtr.i + 1;
	filePtr.p->fileStatus = FileRecord::CLOSED;
	filePtr.p->reqStatus = FileRecord::IDLE;
      }//for
      filePtr.i = cfileFileSize - 1;
      ptrAss(filePtr, fileRecord);
      filePtr.p->nextFile = RNIL;
      cfirstfreeFile = 0;
      initRestorableGciFiles();
      break;
    }
  case 4:
    jam();
    initialiseFragstore();
    break;
  case 5:
    {
      jam();
      /******* NODE GROUP RECORD ******/
      /******* NODE RECORD       ******/
      NodeGroupRecordPtr loopNGPtr;
      for (loopNGPtr.i = 0; loopNGPtr.i < MAX_NDB_NODES; loopNGPtr.i++) {
	ptrAss(loopNGPtr, nodeGroupRecord);
	loopNGPtr.p->nodesInGroup[0] = RNIL;
	loopNGPtr.p->nodesInGroup[1] = RNIL;
	loopNGPtr.p->nodesInGroup[2] = RNIL;
	loopNGPtr.p->nodesInGroup[3] = RNIL;
	loopNGPtr.p->nextReplicaNode = 0;
	loopNGPtr.p->nodeCount = 0;
	loopNGPtr.p->activeTakeOver = false;
      }//for
      NodeRecordPtr nodePtr;
      for (nodePtr.i = 0; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
	ptrAss(nodePtr, nodeRecord);
	new (nodePtr.p) NodeRecord();
      }//for
      break;
    }
  case 6:
    {
      PageRecordPtr pagePtr;
      jam();
      /******* PAGE RECORD ******/
      for (pagePtr.i = 0; pagePtr.i < cpageFileSize; pagePtr.i++) {
        refresh_watch_dog();
	ptrAss(pagePtr, pageRecord);
	pagePtr.p->nextfreepage = pagePtr.i + 1;
      }//for
      pagePtr.i = cpageFileSize - 1;
      ptrAss(pagePtr, pageRecord);
      pagePtr.p->nextfreepage = RNIL;
      cfirstfreepage = 0;
      break;
    }
  case 7:
    {
      ReplicaRecordPtr initReplicaPtr;
      jam();
      /******* REPLICA RECORD ******/
      for (initReplicaPtr.i = 0; initReplicaPtr.i < creplicaFileSize;
	   initReplicaPtr.i++) {
        refresh_watch_dog();
	ptrAss(initReplicaPtr, replicaRecord);
	initReplicaPtr.p->lcpIdStarted = 0;
	initReplicaPtr.p->lcpOngoingFlag = false;
	initReplicaPtr.p->nextReplica = initReplicaPtr.i + 1;
      }//for
      initReplicaPtr.i = creplicaFileSize - 1;
      ptrAss(initReplicaPtr, replicaRecord);
      initReplicaPtr.p->nextReplica = RNIL;
      cnoFreeReplicaRec = creplicaFileSize;
      cfirstfreeReplica = 0;
      break;
    }
  case 8:
    {
      TabRecordPtr loopTabptr;
      jam();
      /********* TAB-DESCRIPTOR ********/
      for (loopTabptr.i = 0; loopTabptr.i < ctabFileSize; loopTabptr.i++) {
	ptrAss(loopTabptr, tabRecord);
        refresh_watch_dog();
	initTable(loopTabptr);
      }//for
      break;
    }
  case 9:
    {
      TakeOverRecordPtr takeOverPtr;
      jam();
      cfirstfreeTakeOver = RNIL;
      for (takeOverPtr.i = 0; takeOverPtr.i < MAX_NDB_NODES; takeOverPtr.i++) {
	ptrAss(takeOverPtr, takeOverRecord);
	initTakeOver(takeOverPtr);
	releaseTakeOver(takeOverPtr.i);
      }//for

      ReadConfigConf * conf = (ReadConfigConf*)signal->getDataPtrSend();
      conf->senderRef = reference();
      conf->senderData = retData;
      sendSignal(retRef, GSN_READ_CONFIG_CONF, signal, 
		 ReadConfigConf::SignalLength, JBB);
      return;
      break;
    }
  default:
    ndbrequire(false);
    break;
  }//switch
  jam();
  /* ---------------------------------------------------------------------- */
  /* SEND REAL-TIME BREAK DURING INIT OF VARIABLES DURING SYSTEM RESTART.   */
  /* ---------------------------------------------------------------------- */
  signal->theData[0] = DihContinueB::ZINITIALISE_RECORDS;
  signal->theData[1] = stepNo + 1;
  signal->theData[2] = retRef;
  signal->theData[3] = retData;
  sendSignal(reference(), GSN_CONTINUEB, signal, 4, JBB);
}//Dbdih::initialiseRecordsLab()

/*************************************************************************/
/*       INSERT THE NODE INTO THE LINKED LIST OF NODES INVOLVED ALL      */
/*       DISTRIBUTED PROTOCOLS (EXCEPT GCP PROTOCOL THAT USES THE DIH    */
/*       LINKED LIST INSTEAD).                                           */
/*************************************************************************/
void Dbdih::insertAlive(NodeRecordPtr newNodePtr) 
{
  NodeRecordPtr nodePtr;

  nodePtr.i = cfirstAliveNode;
  if (nodePtr.i == RNIL) {
    jam();
    cfirstAliveNode = newNodePtr.i;
  } else {
    do {
      ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
      if (nodePtr.p->nextNode == RNIL) {
        jam();
        nodePtr.p->nextNode = newNodePtr.i;
        break;
      } else {
        jam();
        nodePtr.i = nodePtr.p->nextNode;
      }//if
    } while (1);
  }//if
  newNodePtr.p->nextNode = RNIL;
}//Dbdih::insertAlive()

void Dbdih::insertBackup(FragmentstorePtr fragPtr, Uint32 nodeId)
{
  for (Uint32 i = fragPtr.p->fragReplicas; i > 1; i--) {
    jam();
    ndbrequire(i < MAX_REPLICAS && i > 0);
    fragPtr.p->activeNodes[i] = fragPtr.p->activeNodes[i - 1];
  }//for
  fragPtr.p->activeNodes[1] = nodeId;
  fragPtr.p->fragReplicas++;
}//Dbdih::insertBackup()

void Dbdih::insertDeadNode(NodeRecordPtr newNodePtr) 
{
  NodeRecordPtr nodePtr;

  nodePtr.i = cfirstDeadNode;
  if (nodePtr.i == RNIL) {
    jam();
    cfirstDeadNode = newNodePtr.i;
  } else {
    do {
      jam();
      ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
      if (nodePtr.p->nextNode == RNIL) {
        jam();
        nodePtr.p->nextNode = newNodePtr.i;
        break;
      } else {
        jam();
        nodePtr.i = nodePtr.p->nextNode;
      }//if
    } while (1);
  }//if
  newNodePtr.p->nextNode = RNIL;
}//Dbdih::insertDeadNode()

void Dbdih::linkOldStoredReplica(FragmentstorePtr fragPtr,
                                 ReplicaRecordPtr replicatePtr) 
{
  ReplicaRecordPtr losReplicaPtr;

  replicatePtr.p->nextReplica = RNIL;
  fragPtr.p->noOldStoredReplicas++;
  losReplicaPtr.i = fragPtr.p->oldStoredReplicas;
  if (losReplicaPtr.i == RNIL) {
    jam();
    fragPtr.p->oldStoredReplicas = replicatePtr.i;
    return;
  }//if
  ptrCheckGuard(losReplicaPtr, creplicaFileSize, replicaRecord);
  while (losReplicaPtr.p->nextReplica != RNIL) {
    jam();
    losReplicaPtr.i = losReplicaPtr.p->nextReplica;
    ptrCheckGuard(losReplicaPtr, creplicaFileSize, replicaRecord);
  }//if
  losReplicaPtr.p->nextReplica = replicatePtr.i;
}//Dbdih::linkOldStoredReplica()

void Dbdih::linkStoredReplica(FragmentstorePtr fragPtr,
                              ReplicaRecordPtr replicatePtr)
{
  ReplicaRecordPtr lsrReplicaPtr;

  fragPtr.p->noStoredReplicas++;
  replicatePtr.p->nextReplica = RNIL;
  lsrReplicaPtr.i = fragPtr.p->storedReplicas;
  if (fragPtr.p->storedReplicas == RNIL) {
    jam();
    fragPtr.p->storedReplicas = replicatePtr.i;
    return;
  }//if
  ptrCheckGuard(lsrReplicaPtr, creplicaFileSize, replicaRecord);
  while (lsrReplicaPtr.p->nextReplica != RNIL) {
    jam();
    lsrReplicaPtr.i = lsrReplicaPtr.p->nextReplica;
    ptrCheckGuard(lsrReplicaPtr, creplicaFileSize, replicaRecord);
  }//if
  lsrReplicaPtr.p->nextReplica = replicatePtr.i;
}//Dbdih::linkStoredReplica()

/*************************************************************************/
/*        MAKE NODE GROUPS BASED ON THE LIST OF NODES RECEIVED FROM CNTR */
/*************************************************************************/
void Dbdih::makeNodeGroups(Uint32 nodeArray[]) 
{
  NodeRecordPtr mngNodeptr;
  Uint32 tmngNode;
  Uint32 tmngNodeGroup;
  Uint32 tmngLimit;
  Uint32 i;

  /**-----------------------------------------------------------------------
   * ASSIGN ALL ACTIVE NODES INTO NODE GROUPS. HOT SPARE NODES ARE ASSIGNED 
   * TO NODE GROUP ZNIL
   *-----------------------------------------------------------------------*/
  tmngNodeGroup = 0;
  tmngLimit = csystemnodes - cnoHotSpare;
  ndbrequire(tmngLimit < MAX_NDB_NODES);
  for (i = 0; i < tmngLimit; i++) {
    NodeGroupRecordPtr NGPtr;
    jam();
    tmngNode = nodeArray[i];
    mngNodeptr.i = tmngNode;
    ptrCheckGuard(mngNodeptr, MAX_NDB_NODES, nodeRecord);
    mngNodeptr.p->nodeGroup = tmngNodeGroup;
    NGPtr.i = tmngNodeGroup;
    ptrCheckGuard(NGPtr, MAX_NDB_NODES, nodeGroupRecord);
    arrGuard(NGPtr.p->nodeCount, MAX_REPLICAS);
    NGPtr.p->nodesInGroup[NGPtr.p->nodeCount++] = mngNodeptr.i;
    if (NGPtr.p->nodeCount == cnoReplicas) {
      jam();
      tmngNodeGroup++;
    }//if
  }//for
  cnoOfNodeGroups = tmngNodeGroup;
  ndbrequire(csystemnodes < MAX_NDB_NODES);
  for (i = tmngLimit + 1; i < csystemnodes; i++) {
    jam();
    tmngNode = nodeArray[i];
    mngNodeptr.i = tmngNode;
    ptrCheckGuard(mngNodeptr, MAX_NDB_NODES, nodeRecord);
    mngNodeptr.p->nodeGroup = ZNIL;
  }//for
  for(i = 0; i < MAX_NDB_NODES; i++){
    jam();
    Sysfile::setNodeGroup(i, SYSFILE->nodeGroups, NO_NODE_GROUP_ID);
  }//for
  for (mngNodeptr.i = 1; mngNodeptr.i < MAX_NDB_NODES; mngNodeptr.i++) {
    jam();
    ptrAss(mngNodeptr, nodeRecord);
    if (mngNodeptr.p->nodeGroup != ZNIL) {
      jam();
      Sysfile::setNodeGroup(mngNodeptr.i, SYSFILE->nodeGroups, mngNodeptr.p->nodeGroup);
    }//if
  }//for
}//Dbdih::makeNodeGroups()

/**
 * On node failure QMGR asks DIH about node groups.  This is
 * a direct signal (function call in same process).  Input is
 * bitmask of surviving nodes.  The routine is not concerned
 * about node count.  Reply is one of:
 * 1) win - we can survive, and nobody else can
 * 2) lose - we cannot survive
 * 3) partition - we can survive but there could be others
 */
void Dbdih::execCHECKNODEGROUPSREQ(Signal* signal)
{
  jamEntry();
  CheckNodeGroups* sd = (CheckNodeGroups*)&signal->theData[0];

  bool direct = (sd->requestType & CheckNodeGroups::Direct);
  bool ok = false;
  switch(sd->requestType & ~CheckNodeGroups::Direct){
  case CheckNodeGroups::ArbitCheck:{
    ok = true;
    jam();
    unsigned missall = 0;
    unsigned haveall = 0;
    for (Uint32 i = 0; i < cnoOfNodeGroups; i++) {
      jam();
      NodeGroupRecordPtr ngPtr;
      ngPtr.i = i;
      ptrAss(ngPtr, nodeGroupRecord);
      Uint32 count = 0;
      for (Uint32 j = 0; j < ngPtr.p->nodeCount; j++) {
	jam();
	Uint32 nodeId = ngPtr.p->nodesInGroup[j];
	if (sd->mask.get(nodeId)) {
	  jam();
	  count++;
	}//if
      }//for
      if (count == 0) {
	jam();
	missall++;
      }//if
      if (count == ngPtr.p->nodeCount) {
	haveall++;
      }//if
    }//for

    if (missall) {
      jam();
      sd->output = CheckNodeGroups::Lose;
    } else if (haveall) {
      jam();
      sd->output = CheckNodeGroups::Win;
    } else {
      jam();
      sd->output = CheckNodeGroups::Partitioning;
    }//if
  }
    break;
  case CheckNodeGroups::GetNodeGroup:
    ok = true;
    sd->output = Sysfile::getNodeGroup(getOwnNodeId(), SYSFILE->nodeGroups);
    break;
  case CheckNodeGroups::GetNodeGroupMembers: {
    ok = true;
    Uint32 ownNodeGoup =
      Sysfile::getNodeGroup(sd->nodeId, SYSFILE->nodeGroups);

    sd->output = ownNodeGoup;
    sd->mask.clear();

    NodeGroupRecordPtr ngPtr;
    ngPtr.i = ownNodeGoup;
    ptrAss(ngPtr, nodeGroupRecord);
    for (Uint32 j = 0; j < ngPtr.p->nodeCount; j++) {
      jam();
      sd->mask.set(ngPtr.p->nodesInGroup[j]);
    }
#if 0
    for (int i = 0; i < MAX_NDB_NODES; i++) {
      if (ownNodeGoup == 
	  Sysfile::getNodeGroup(i, SYSFILE->nodeGroups)) {
	sd->mask.set(i);
      }
    }
#endif
  }
    break;
  }
  ndbrequire(ok);
  
  if (!direct)
    sendSignal(sd->blockRef, GSN_CHECKNODEGROUPSCONF, signal,
	       CheckNodeGroups::SignalLength, JBB);
}//Dbdih::execCHECKNODEGROUPSREQ()

void Dbdih::makePrnList(ReadNodesConf * readNodes, Uint32 nodeArray[]) 
{
  cfirstAliveNode = RNIL;
  ndbrequire(con_lineNodes > 0);
  ndbrequire(csystemnodes < MAX_NDB_NODES);
  for (Uint32 i = 0; i < csystemnodes; i++) {
    NodeRecordPtr nodePtr;
    jam();
    nodePtr.i = nodeArray[i];
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
    new (nodePtr.p) NodeRecord();
    if (NodeBitmask::get(readNodes->inactiveNodes, nodePtr.i) == false){
      jam();
      nodePtr.p->nodeStatus = NodeRecord::ALIVE;
      nodePtr.p->useInTransactions = true;
      nodePtr.p->copyCompleted = true;
      nodePtr.p->m_inclDihLcp = true;
      insertAlive(nodePtr);
    } else {
      jam();
      nodePtr.p->nodeStatus = NodeRecord::DEAD;
      insertDeadNode(nodePtr);
    }//if
  }//for
}//Dbdih::makePrnList()

/*************************************************************************/
/*       A NEW CRASHED REPLICA IS ADDED BY A NODE FAILURE.               */
/*************************************************************************/
void Dbdih::newCrashedReplica(Uint32 nodeId, ReplicaRecordPtr ncrReplicaPtr) 
{
  /*----------------------------------------------------------------------*/
  /*       SET THE REPLICA_LAST_GCI OF THE CRASHED REPLICA TO LAST GCI    */
  /*       EXECUTED BY THE FAILED NODE.                                   */
  /*----------------------------------------------------------------------*/
  /*       WE HAVE A NEW CRASHED REPLICA. INITIATE CREATE GCI TO INDICATE */
  /*       THAT THE NEW REPLICA IS NOT STARTED YET AND REPLICA_LAST_GCI IS*/
  /*       SET TO -1 TO INDICATE THAT IT IS NOT DEAD YET.                 */
  /*----------------------------------------------------------------------*/
  arrGuard(ncrReplicaPtr.p->noCrashedReplicas + 1, 8);
  ncrReplicaPtr.p->replicaLastGci[ncrReplicaPtr.p->noCrashedReplicas] = 
    SYSFILE->lastCompletedGCI[nodeId];
  ncrReplicaPtr.p->noCrashedReplicas = ncrReplicaPtr.p->noCrashedReplicas + 1;
  ncrReplicaPtr.p->createGci[ncrReplicaPtr.p->noCrashedReplicas] = 0;
  ncrReplicaPtr.p->replicaLastGci[ncrReplicaPtr.p->noCrashedReplicas] = 
    (Uint32)-1;
}//Dbdih::newCrashedReplica()

/*************************************************************************/
/*       AT NODE FAILURE DURING START OF A NEW NODE WE NEED TO RESET A   */
/*       SET OF VARIABLES CONTROLLING THE START AND INDICATING ONGOING   */
/*       START OF A NEW NODE.                                            */
/*************************************************************************/
void Dbdih::nodeResetStart()
{
  jam();
  c_nodeStartMaster.startNode = RNIL;
  c_nodeStartMaster.failNr = cfailurenr;
  c_nodeStartMaster.activeState = false;
  c_nodeStartMaster.blockGcp = false;
  c_nodeStartMaster.blockLcp = false;
  c_nodeStartMaster.m_outstandingGsn = 0;
}//Dbdih::nodeResetStart()

void Dbdih::openFileRw(Signal* signal, FileRecordPtr filePtr) 
{
  signal->theData[0] = reference();
  signal->theData[1] = filePtr.i;
  signal->theData[2] = filePtr.p->fileName[0];
  signal->theData[3] = filePtr.p->fileName[1];
  signal->theData[4] = filePtr.p->fileName[2];
  signal->theData[5] = filePtr.p->fileName[3];
  signal->theData[6] = FsOpenReq::OM_READWRITE;
  sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, 7, JBA);
}//Dbdih::openFileRw()

void Dbdih::openFileRo(Signal* signal, FileRecordPtr filePtr) 
{
  signal->theData[0] = reference();
  signal->theData[1] = filePtr.i;
  signal->theData[2] = filePtr.p->fileName[0];
  signal->theData[3] = filePtr.p->fileName[1];
  signal->theData[4] = filePtr.p->fileName[2];
  signal->theData[5] = filePtr.p->fileName[3];
  signal->theData[6] = FsOpenReq::OM_READONLY;
  sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, 7, JBA);
}//Dbdih::openFileRw()

/*************************************************************************/
/*       REMOVE A CRASHED REPLICA BY PACKING THE ARRAY OF CREATED GCI AND*/
/*       THE LAST GCI OF THE CRASHED REPLICA.                            */
/*************************************************************************/
void Dbdih::packCrashedReplicas(ReplicaRecordPtr replicaPtr)
{
  ndbrequire(replicaPtr.p->noCrashedReplicas > 0);
  ndbrequire(replicaPtr.p->noCrashedReplicas <= 8);
  for (Uint32 i = 0; i < replicaPtr.p->noCrashedReplicas; i++) {
    jam();
    replicaPtr.p->createGci[i] = replicaPtr.p->createGci[i + 1];
    replicaPtr.p->replicaLastGci[i] = replicaPtr.p->replicaLastGci[i + 1];
  }//for
  replicaPtr.p->noCrashedReplicas--;

#ifdef VM_TRACE
  for (Uint32 i = 0; i < replicaPtr.p->noCrashedReplicas; i++) {
    jam();
    ndbrequire(replicaPtr.p->createGci[i] != 0xF1F1F1F1);
    ndbrequire(replicaPtr.p->replicaLastGci[i] != 0xF1F1F1F1);
  }//for
#endif
}//Dbdih::packCrashedReplicas()

void Dbdih::prepareReplicas(FragmentstorePtr fragPtr)
{
  ReplicaRecordPtr prReplicaPtr;
  Uint32 prevReplica = RNIL;

  /* --------------------------------------------------------------------- */
  /*       BEGIN BY LINKING ALL REPLICA RECORDS ONTO THE OLD STORED REPLICA*/
  /*       LIST.                                                           */
  /*       AT A SYSTEM RESTART OBVIOUSLY ALL NODES ARE OLD.                */
  /* --------------------------------------------------------------------- */
  prReplicaPtr.i = fragPtr.p->storedReplicas;
  while (prReplicaPtr.i != RNIL) {
    jam();
    prevReplica = prReplicaPtr.i;
    ptrCheckGuard(prReplicaPtr, creplicaFileSize, replicaRecord);
    prReplicaPtr.i = prReplicaPtr.p->nextReplica;
  }//while
  /* --------------------------------------------------------------------- */
  /*       LIST OF STORED REPLICAS WILL BE EMPTY NOW.                      */
  /* --------------------------------------------------------------------- */
  if (prevReplica != RNIL) {
    prReplicaPtr.i = prevReplica;
    ptrCheckGuard(prReplicaPtr, creplicaFileSize, replicaRecord);
    prReplicaPtr.p->nextReplica = fragPtr.p->oldStoredReplicas;
    fragPtr.p->oldStoredReplicas = fragPtr.p->storedReplicas;
    fragPtr.p->storedReplicas = RNIL;
    fragPtr.p->noOldStoredReplicas += fragPtr.p->noStoredReplicas;
    fragPtr.p->noStoredReplicas = 0;
  }//if
}//Dbdih::prepareReplicas()

void Dbdih::readFragment(RWFragment* rf, FragmentstorePtr fragPtr)
{
  Uint32 TreadFid = readPageWord(rf);
  fragPtr.p->preferredPrimary = readPageWord(rf);
  fragPtr.p->noStoredReplicas = readPageWord(rf);
  fragPtr.p->noOldStoredReplicas = readPageWord(rf);
  Uint32 TdistKey = readPageWord(rf);

  ndbrequire(fragPtr.p->noStoredReplicas > 0);
  ndbrequire(TreadFid == rf->fragId);  
  ndbrequire(TdistKey < 256);
  if ((cstarttype == NodeState::ST_NODE_RESTART) || 
      (cstarttype == NodeState::ST_INITIAL_NODE_RESTART)) {
    jam();
    fragPtr.p->distributionKey = TdistKey;
  }//if
}//Dbdih::readFragment()

Uint32 Dbdih::readPageWord(RWFragment* rf) 
{
  if (rf->wordIndex >= 2048) {
    jam();
    ndbrequire(rf->wordIndex == 2048);
    rf->pageIndex++;
    ndbrequire(rf->pageIndex < 8);
    rf->rwfPageptr.i = rf->rwfTabPtr.p->pageRef[rf->pageIndex];
    ptrCheckGuard(rf->rwfPageptr, cpageFileSize, pageRecord);
    rf->wordIndex = 32;
  }//if
  Uint32 dataWord = rf->rwfPageptr.p->word[rf->wordIndex];
  rf->wordIndex++;
  return dataWord;
}//Dbdih::readPageWord()

void Dbdih::readReplica(RWFragment* rf, ReplicaRecordPtr readReplicaPtr) 
{
  Uint32 i;
  readReplicaPtr.p->procNode = readPageWord(rf);
  readReplicaPtr.p->initialGci = readPageWord(rf);
  readReplicaPtr.p->noCrashedReplicas = readPageWord(rf);
  readReplicaPtr.p->nextLcp = readPageWord(rf);

  for (i = 0; i < MAX_LCP_STORED; i++) {
    readReplicaPtr.p->maxGciCompleted[i] = readPageWord(rf);
    readReplicaPtr.p->maxGciStarted[i] = readPageWord(rf);
    readReplicaPtr.p->lcpId[i] = readPageWord(rf);
    readReplicaPtr.p->lcpStatus[i] = readPageWord(rf);
  }//for
  const Uint32 noCrashedReplicas = readReplicaPtr.p->noCrashedReplicas;
  ndbrequire(noCrashedReplicas < 8);
  for (i = 0; i < noCrashedReplicas; i++) {
    readReplicaPtr.p->createGci[i] = readPageWord(rf);
    readReplicaPtr.p->replicaLastGci[i] = readPageWord(rf);
    ndbrequire(readReplicaPtr.p->createGci[i] != 0xF1F1F1F1);
    ndbrequire(readReplicaPtr.p->replicaLastGci[i] != 0xF1F1F1F1);
  }//for
  for(i = noCrashedReplicas; i<8; i++){
    readReplicaPtr.p->createGci[i] = readPageWord(rf);
    readReplicaPtr.p->replicaLastGci[i] = readPageWord(rf);
    // They are not initialized...
    readReplicaPtr.p->createGci[i] = 0;
    readReplicaPtr.p->replicaLastGci[i] = ~0;
  }
  /* ---------------------------------------------------------------------- */
  /*       IF THE LAST COMPLETED LOCAL CHECKPOINT IS VALID AND LARGER THAN  */
  /*       THE LAST COMPLETED CHECKPOINT THEN WE WILL INVALIDATE THIS LOCAL */
  /*       CHECKPOINT FOR THIS REPLICA.                                     */
  /* ---------------------------------------------------------------------- */
  Uint32 trraLcp = prevLcpNo(readReplicaPtr.p->nextLcp);
  ndbrequire(trraLcp < MAX_LCP_STORED);
  if ((readReplicaPtr.p->lcpStatus[trraLcp] == ZVALID) &&
      (readReplicaPtr.p->lcpId[trraLcp] > SYSFILE->latestLCP_ID)) {
    jam();
    readReplicaPtr.p->lcpStatus[trraLcp] = ZINVALID;
  }//if
  /* ---------------------------------------------------------------------- */
  /*       WE ALSO HAVE TO INVALIDATE ANY LOCAL CHECKPOINTS THAT HAVE BEEN  */
  /*       INVALIDATED BY MOVING BACK THE RESTART GCI.                      */
  /* ---------------------------------------------------------------------- */
  for (i = 0; i < MAX_LCP_STORED; i++) {
    jam();
    if ((readReplicaPtr.p->lcpStatus[i] == ZVALID) &&
        (readReplicaPtr.p->maxGciStarted[i] > SYSFILE->newestRestorableGCI)) {
      jam();
      readReplicaPtr.p->lcpStatus[i] = ZINVALID;
    }//if
  }//for
  /* ---------------------------------------------------------------------- */
  /*       WE WILL REMOVE ANY OCCURRENCES OF REPLICAS THAT HAVE CRASHED     */
  /*       THAT ARE NO LONGER VALID DUE TO MOVING RESTART GCI BACKWARDS.    */
  /* ---------------------------------------------------------------------- */
  removeTooNewCrashedReplicas(readReplicaPtr);
  /* ---------------------------------------------------------------------- */
  /*       WE WILL REMOVE ANY OCCURRENCES OF REPLICAS THAT HAVE CRASHED     */
  /*       THAT ARE NO LONGER VALID SINCE THEY ARE NO LONGER RESTORABLE.    */
  /* ---------------------------------------------------------------------- */
  removeOldCrashedReplicas(readReplicaPtr);
  /* --------------------------------------------------------------------- */
  // We set the last GCI of the replica that was alive before the node
  // crashed last time. We set it to the last GCI which the node participated in.
  /* --------------------------------------------------------------------- */
  ndbrequire(readReplicaPtr.p->noCrashedReplicas < 8);
  readReplicaPtr.p->replicaLastGci[readReplicaPtr.p->noCrashedReplicas] = 
    SYSFILE->lastCompletedGCI[readReplicaPtr.p->procNode];
  /* ---------------------------------------------------------------------- */
  /*       FIND PROCESSOR RECORD                                            */
  /* ---------------------------------------------------------------------- */
}//Dbdih::readReplica()

void Dbdih::readReplicas(RWFragment* rf, FragmentstorePtr fragPtr)
{
  Uint32 i;
  ReplicaRecordPtr newReplicaPtr;
  Uint32 noStoredReplicas = fragPtr.p->noStoredReplicas;
  Uint32 noOldStoredReplicas = fragPtr.p->noOldStoredReplicas;
  /* ----------------------------------------------------------------------- */
  /*      WE CLEAR THE NUMBER OF STORED REPLICAS SINCE IT WILL BE CALCULATED */
  /*      BY THE LINKING SUBROUTINES.                                        */
  /* ----------------------------------------------------------------------- */
  fragPtr.p->noStoredReplicas = 0;
  fragPtr.p->noOldStoredReplicas = 0;
  Uint32 replicaIndex = 0;
  ndbrequire(noStoredReplicas + noOldStoredReplicas <= MAX_REPLICAS);
  for (i = 0; i < noStoredReplicas; i++) {
    seizeReplicaRec(newReplicaPtr);
    readReplica(rf, newReplicaPtr);
    if (checkNodeAlive(newReplicaPtr.p->procNode)) {
      jam();
      ndbrequire(replicaIndex < MAX_REPLICAS);
      fragPtr.p->activeNodes[replicaIndex] = newReplicaPtr.p->procNode;
      replicaIndex++;
      linkStoredReplica(fragPtr, newReplicaPtr);
    } else {
      jam();
      linkOldStoredReplica(fragPtr, newReplicaPtr);
    }//if
  }//for
  fragPtr.p->fragReplicas = noStoredReplicas;
  for (i = 0; i < noOldStoredReplicas; i++) {
    jam();
    seizeReplicaRec(newReplicaPtr);
    readReplica(rf, newReplicaPtr);
    linkOldStoredReplica(fragPtr, newReplicaPtr);
  }//for
}//Dbdih::readReplicas()

void Dbdih::readRestorableGci(Signal* signal, FileRecordPtr filePtr) 
{
  signal->theData[0] = filePtr.p->fileRef;
  signal->theData[1] = reference();
  signal->theData[2] = filePtr.i;
  signal->theData[3] = ZLIST_OF_PAIRS;
  signal->theData[4] = ZVAR_NO_CRESTART_INFO;
  signal->theData[5] = 1;
  signal->theData[6] = 0;
  signal->theData[7] = 0;
  sendSignal(NDBFS_REF, GSN_FSREADREQ, signal, 8, JBA);
}//Dbdih::readRestorableGci()

void Dbdih::readTabfile(Signal* signal, TabRecord* tab, FileRecordPtr filePtr) 
{
  signal->theData[0] = filePtr.p->fileRef;
  signal->theData[1] = reference();
  signal->theData[2] = filePtr.i;
  signal->theData[3] = ZLIST_OF_PAIRS;
  signal->theData[4] = ZVAR_NO_WORD;
  signal->theData[5] = tab->noPages;
  for (Uint32 i = 0; i < tab->noPages; i++) {
    signal->theData[6 + (2 * i)] = tab->pageRef[i];
    signal->theData[7 + (2 * i)] = i;
  }//for
  sendSignal(NDBFS_REF, GSN_FSREADREQ, signal, 22, JBA);
}//Dbdih::readTabfile()

void Dbdih::releasePage(Uint32 pageIndex)
{
  PageRecordPtr pagePtr;
  pagePtr.i = pageIndex;
  ptrCheckGuard(pagePtr, cpageFileSize, pageRecord);
  pagePtr.p->nextfreepage = cfirstfreepage;
  cfirstfreepage = pagePtr.i;
}//Dbdih::releasePage()

void Dbdih::releaseTabPages(Uint32 tableId) 
{
  TabRecordPtr tabPtr;
  tabPtr.i = tableId;
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
  ndbrequire(tabPtr.p->noPages <= 8);
  for (Uint32 i = 0; i < tabPtr.p->noPages; i++) {
    jam();
    releasePage(tabPtr.p->pageRef[i]);
  }//for
  tabPtr.p->noPages = 0;
}//Dbdih::releaseTabPages()

/*************************************************************************/
/*       REMOVE NODE FROM SET OF ALIVE NODES.                            */
/*************************************************************************/
void Dbdih::removeAlive(NodeRecordPtr removeNodePtr) 
{
  NodeRecordPtr nodePtr;

  nodePtr.i = cfirstAliveNode;
  if (nodePtr.i == removeNodePtr.i) {
    jam();
    cfirstAliveNode = removeNodePtr.p->nextNode;
    return;
  }//if
  do {
    jam();
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
    if (nodePtr.p->nextNode == removeNodePtr.i) {
      jam();
      nodePtr.p->nextNode = removeNodePtr.p->nextNode;
      break;
    } else {
      jam();
      nodePtr.i = nodePtr.p->nextNode;
    }//if
  } while (1);
}//Dbdih::removeAlive()

/*************************************************************************/
/*       REMOVE NODE FROM SET OF DEAD NODES.                             */
/*************************************************************************/
void Dbdih::removeDeadNode(NodeRecordPtr removeNodePtr) 
{
  NodeRecordPtr nodePtr;

  nodePtr.i = cfirstDeadNode;
  if (nodePtr.i == removeNodePtr.i) {
    jam();
    cfirstDeadNode = removeNodePtr.p->nextNode;
    return;
  }//if
  do {
    jam();
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
    if (nodePtr.p->nextNode == removeNodePtr.i) {
      jam();
      nodePtr.p->nextNode = removeNodePtr.p->nextNode;
      break;
    } else {
      jam();
      nodePtr.i = nodePtr.p->nextNode;
    }//if
  } while (1);
}//Dbdih::removeDeadNode()

/*---------------------------------------------------------------*/
/*       REMOVE REPLICAS OF A FAILED NODE FROM LIST OF STORED    */
/*       REPLICAS AND MOVE IT TO THE LIST OF OLD STORED REPLICAS.*/
/*       ALSO UPDATE THE CRASHED REPLICA INFORMATION.            */
/*---------------------------------------------------------------*/
void Dbdih::removeNodeFromStored(Uint32 nodeId,
                                 FragmentstorePtr fragPtr,
                                 ReplicaRecordPtr replicatePtr)
{
  newCrashedReplica(nodeId, replicatePtr);
  removeStoredReplica(fragPtr, replicatePtr);
  linkOldStoredReplica(fragPtr, replicatePtr);
  ndbrequire(fragPtr.p->storedReplicas != RNIL);
}//Dbdih::removeNodeFromStored()

/*************************************************************************/
/*       REMOVE ANY OLD CRASHED REPLICAS THAT ARE NOT RESTORABLE ANY MORE*/
/*************************************************************************/
void Dbdih::removeOldCrashedReplicas(ReplicaRecordPtr rocReplicaPtr) 
{
  while (rocReplicaPtr.p->noCrashedReplicas > 0) {
    jam();
    /* --------------------------------------------------------------------- */
    /*       ONLY IF THERE IS AT LEAST ONE REPLICA THEN CAN WE REMOVE ANY.   */
    /* --------------------------------------------------------------------- */
    if (rocReplicaPtr.p->replicaLastGci[0] < SYSFILE->oldestRestorableGCI){
      jam();
      /* ------------------------------------------------------------------- */
      /*     THIS CRASHED REPLICA HAS BECOME EXTINCT AND MUST BE REMOVED TO  */
      /*     GIVE SPACE FOR NEW CRASHED REPLICAS.                            */
      /* ------------------------------------------------------------------- */
      packCrashedReplicas(rocReplicaPtr);
    } else {
      break;
    }//if
  }//while
  if (rocReplicaPtr.p->createGci[0] < SYSFILE->keepGCI){
    jam();
    /* --------------------------------------------------------------------- */
    /*       MOVE FORWARD THE CREATE GCI TO A GCI THAT CAN BE USED. WE HAVE  */
    /*       NO CERTAINTY IN FINDING ANY LOG RECORDS FROM OLDER GCI'S.       */
    /* --------------------------------------------------------------------- */
    rocReplicaPtr.p->createGci[0] = SYSFILE->keepGCI;
    ndbrequire(SYSFILE->keepGCI != 0xF1F1F1F1);
  }//if
}//Dbdih::removeOldCrashedReplicas()

void Dbdih::removeOldStoredReplica(FragmentstorePtr fragPtr,
                                   ReplicaRecordPtr replicatePtr) 
{
  ReplicaRecordPtr rosTmpReplicaPtr;
  ReplicaRecordPtr rosPrevReplicaPtr;

  fragPtr.p->noOldStoredReplicas--;
  if (fragPtr.p->oldStoredReplicas == replicatePtr.i) {
    jam();
    fragPtr.p->oldStoredReplicas = replicatePtr.p->nextReplica;
  } else {
    rosPrevReplicaPtr.i = fragPtr.p->oldStoredReplicas;
    ptrCheckGuard(rosPrevReplicaPtr, creplicaFileSize, replicaRecord);
    rosTmpReplicaPtr.i = rosPrevReplicaPtr.p->nextReplica;
    while (rosTmpReplicaPtr.i != replicatePtr.i) {
      jam();
      rosPrevReplicaPtr.i = rosTmpReplicaPtr.i;
      ptrCheckGuard(rosPrevReplicaPtr, creplicaFileSize, replicaRecord);
      ptrCheckGuard(rosTmpReplicaPtr, creplicaFileSize, replicaRecord);
      rosTmpReplicaPtr.i = rosTmpReplicaPtr.p->nextReplica;
    }//if
    rosPrevReplicaPtr.p->nextReplica = replicatePtr.p->nextReplica;
  }//if
}//Dbdih::removeOldStoredReplica()

void Dbdih::removeStoredReplica(FragmentstorePtr fragPtr,
                                ReplicaRecordPtr replicatePtr)
{
  ReplicaRecordPtr rsrTmpReplicaPtr;
  ReplicaRecordPtr rsrPrevReplicaPtr;

  fragPtr.p->noStoredReplicas--;
  if (fragPtr.p->storedReplicas == replicatePtr.i) {
    jam();
    fragPtr.p->storedReplicas = replicatePtr.p->nextReplica;
  } else {
    jam();
    rsrPrevReplicaPtr.i = fragPtr.p->storedReplicas;
    rsrTmpReplicaPtr.i = fragPtr.p->storedReplicas;
    ptrCheckGuard(rsrTmpReplicaPtr, creplicaFileSize, replicaRecord);
    rsrTmpReplicaPtr.i = rsrTmpReplicaPtr.p->nextReplica;
    while (rsrTmpReplicaPtr.i != replicatePtr.i) {
      jam();
      rsrPrevReplicaPtr.i = rsrTmpReplicaPtr.i;
      ptrCheckGuard(rsrTmpReplicaPtr, creplicaFileSize, replicaRecord);
      rsrTmpReplicaPtr.i = rsrTmpReplicaPtr.p->nextReplica;
    }//while
    ptrCheckGuard(rsrPrevReplicaPtr, creplicaFileSize, replicaRecord);
    rsrPrevReplicaPtr.p->nextReplica = replicatePtr.p->nextReplica;
  }//if
}//Dbdih::removeStoredReplica()

/*************************************************************************/
/*       REMOVE ALL TOO NEW CRASHED REPLICAS THAT IS IN THIS REPLICA.    */
/*************************************************************************/
void Dbdih::removeTooNewCrashedReplicas(ReplicaRecordPtr rtnReplicaPtr) 
{
  while (rtnReplicaPtr.p->noCrashedReplicas > 0) {
    jam();
    /* --------------------------------------------------------------------- */
    /*       REMOVE ALL REPLICAS THAT ONLY LIVED IN A PERIOD THAT HAVE BEEN  */
    /*       REMOVED FROM THE RESTART INFORMATION SINCE THE RESTART FAILED   */
    /*       TOO MANY TIMES.                                                 */
    /* --------------------------------------------------------------------- */
    arrGuard(rtnReplicaPtr.p->noCrashedReplicas - 1, 8);
    if (rtnReplicaPtr.p->createGci[rtnReplicaPtr.p->noCrashedReplicas - 1] > 
        SYSFILE->newestRestorableGCI){
      jam();
      rtnReplicaPtr.p->createGci[rtnReplicaPtr.p->noCrashedReplicas - 1] = 
	(Uint32)-1;
      rtnReplicaPtr.p->replicaLastGci[rtnReplicaPtr.p->noCrashedReplicas - 1] = 
	(Uint32)-1;
      rtnReplicaPtr.p->noCrashedReplicas--;
    } else {
      break;
    }//if
  }//while
}//Dbdih::removeTooNewCrashedReplicas()

/*************************************************************************/
/*                                                                       */
/*       MODULE: SEARCH FOR POSSIBLE REPLICAS THAT CAN HANDLE THE GLOBAL */
/*               CHECKPOINT WITHOUT NEEDING ANY EXTRA LOGGING FACILITIES.*/
/*               A MAXIMUM OF FOUR NODES IS RETRIEVED.                   */
/*************************************************************************/
void Dbdih::searchStoredReplicas(FragmentstorePtr fragPtr) 
{
  Uint32 nextReplicaPtrI;
  ConstPtr<ReplicaRecord> replicaPtr;

  replicaPtr.i = fragPtr.p->storedReplicas;
  while (replicaPtr.i != RNIL) {
    jam();
    ptrCheckGuard(replicaPtr, creplicaFileSize, replicaRecord);
    nextReplicaPtrI = replicaPtr.p->nextReplica;
    NodeRecordPtr nodePtr;
    nodePtr.i = replicaPtr.p->procNode;
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
    if (nodePtr.p->nodeStatus == NodeRecord::ALIVE) {
      jam();
      switch (nodePtr.p->activeStatus) {
      case Sysfile::NS_Active:
      case Sysfile::NS_ActiveMissed_1:
      case Sysfile::NS_ActiveMissed_2:{
	/* ----------------------------------------------------------------- */
	/*   INITIALISE THE CREATE REPLICA STRUCTURE THAT IS USED FOR SENDING*/
	/*   TO LQH START_FRAGREQ.                                           */
	/*   SET THE DATA NODE WHERE THE LOCAL CHECKPOINT IS FOUND. ALSO     */
	/*   SET A REFERENCE TO THE REPLICA POINTER OF THAT.                 */
	/* ----------------------------------------------------------------- */
	CreateReplicaRecordPtr createReplicaPtr;
	createReplicaPtr.i = cnoOfCreateReplicas;
	ptrCheckGuard(createReplicaPtr, 4, createReplicaRecord);
	cnoOfCreateReplicas++;
	createReplicaPtr.p->dataNodeId = replicaPtr.p->procNode;
	createReplicaPtr.p->replicaRec = replicaPtr.i;
	/* ----------------------------------------------------------------- */
	/*   WE NEED TO SEARCH FOR A PROPER LOCAL CHECKPOINT TO USE FOR THE  */
	/*   SYSTEM RESTART.                                                 */
	/* ----------------------------------------------------------------- */
	Uint32 startGci;
	Uint32 startLcpNo;
	Uint32 stopGci = SYSFILE->newestRestorableGCI;
	bool result = findStartGci(replicaPtr,
				   stopGci,
				   startGci,
				   startLcpNo);
	if (!result) {
	  jam();
	  /* --------------------------------------------------------------- */
	  /* WE COULD NOT FIND ANY LOCAL CHECKPOINT. THE FRAGMENT THUS DO NOT*/
	  /* CONTAIN ANY VALID LOCAL CHECKPOINT. IT DOES HOWEVER CONTAIN A   */
	  /* VALID FRAGMENT LOG. THUS BY FIRST CREATING THE FRAGMENT AND THEN*/
	  /* EXECUTING THE FRAGMENT LOG WE CAN CREATE THE FRAGMENT AS        */
	  /* DESIRED. THIS SHOULD ONLY OCCUR AFTER CREATING A FRAGMENT.      */
	  /*                                                                 */
	  /* TO INDICATE THAT NO LOCAL CHECKPOINT IS TO BE USED WE SET THE   */
	  /* LOCAL CHECKPOINT TO ZNIL.                                       */
	  /* --------------------------------------------------------------- */
	  createReplicaPtr.p->lcpNo = ZNIL;
	} else {
	  jam();
	  /* --------------------------------------------------------------- */
	  /* WE FOUND A PROPER LOCAL CHECKPOINT TO RESTART FROM.             */
	  /* SET LOCAL CHECKPOINT ID AND LOCAL CHECKPOINT NUMBER.            */
	  /* --------------------------------------------------------------- */
	  createReplicaPtr.p->lcpNo = startLcpNo;
	  arrGuard(startLcpNo, MAX_LCP_STORED);
	  createReplicaPtr.p->createLcpId = replicaPtr.p->lcpId[startLcpNo];
	}//if

	if(ERROR_INSERTED(7073) || ERROR_INSERTED(7074)){
	  jam();
	  nodePtr.p->nodeStatus = NodeRecord::DEAD;
	}

	/* ----------------------------------------------------------------- */
	/*   WE HAVE EITHER FOUND A LOCAL CHECKPOINT OR WE ARE PLANNING TO   */
	/*   EXECUTE THE LOG FROM THE INITIAL CREATION OF THE TABLE. IN BOTH */
	/*   CASES WE NEED TO FIND A SET OF LOGS THAT CAN EXECUTE SUCH THAT  */
	/*   WE RECOVER TO THE SYSTEM RESTART GLOBAL CHECKPOINT.             */
	/* -_--------------------------------------------------------------- */
	if (!findLogNodes(createReplicaPtr.p, fragPtr, startGci, stopGci)) {
	  jam();
	  /* --------------------------------------------------------------- */
	  /* WE WERE NOT ABLE TO FIND ANY WAY OF RESTORING THIS REPLICA.     */
	  /* THIS IS A POTENTIAL SYSTEM ERROR.                               */
	  /* --------------------------------------------------------------- */
	  cnoOfCreateReplicas--;
	  return;
	}//if
	
	if(ERROR_INSERTED(7073) || ERROR_INSERTED(7074)){
	  jam();
	  nodePtr.p->nodeStatus = NodeRecord::ALIVE;
	}
	
	break;
      }
      default:
        jam();
        /*empty*/;
        break;
      }//switch
    }
    replicaPtr.i = nextReplicaPtrI;
  }//while
}//Dbdih::searchStoredReplicas()

/*************************************************************************/
/*                                                                       */
/*       MODULE: SEIZE_FILE                                              */
/*       DESCRIPTION: THE SUBROUTINE SEIZES A FILE RECORD FROM THE       */
/*                    FREE LIST.                                         */
/*************************************************************************/
void Dbdih::seizeFile(FileRecordPtr& filePtr)
{
  filePtr.i = cfirstfreeFile;
  ptrCheckGuard(filePtr, cfileFileSize, fileRecord);
  cfirstfreeFile = filePtr.p->nextFile;
  filePtr.p->nextFile = RNIL;
}//Dbdih::seizeFile()

/*************************************************************************/
/*       SEND CREATE_FRAGREQ TO ALL NODES IN THE NDB CLUSTER.            */
/*************************************************************************/
/*************************************************************************/
/*                                                                       */
/*       MODULE: FIND THE START GCI AND LOCAL CHECKPOINT TO USE.         */
/*************************************************************************/
void Dbdih::sendStartFragreq(Signal* signal, 
			     TabRecordPtr tabPtr, Uint32 fragId) 
{
  CreateReplicaRecordPtr replicaPtr;
  for (replicaPtr.i = 0; replicaPtr.i < cnoOfCreateReplicas; replicaPtr.i++) {
    jam();
    ptrAss(replicaPtr, createReplicaRecord);
    BlockReference ref = calcLqhBlockRef(replicaPtr.p->dataNodeId);
    StartFragReq * const startFragReq = (StartFragReq *)&signal->theData[0];
    startFragReq->userPtr = replicaPtr.p->replicaRec;
    startFragReq->userRef = reference();
    startFragReq->lcpNo = replicaPtr.p->lcpNo;
    startFragReq->lcpId = replicaPtr.p->createLcpId;
    startFragReq->tableId = tabPtr.i;
    startFragReq->fragId = fragId;

    if(ERROR_INSERTED(7072) || ERROR_INSERTED(7074)){
      jam();
      const Uint32 noNodes = replicaPtr.p->noLogNodes;
      Uint32 start = replicaPtr.p->logStartGci[noNodes - 1];
      const Uint32 stop  = replicaPtr.p->logStopGci[noNodes - 1];

      for(Uint32 i = noNodes; i < 4 && (stop - start) > 0; i++){
	replicaPtr.p->noLogNodes++;
	replicaPtr.p->logStopGci[i - 1] = start;
	
	replicaPtr.p->logNodeId[i] = replicaPtr.p->logNodeId[i-1];
	replicaPtr.p->logStartGci[i] = start + 1;
	replicaPtr.p->logStopGci[i] = stop;      
	start += 1;
      }
    }
    
    startFragReq->noOfLogNodes = replicaPtr.p->noLogNodes;
    
    for (Uint32 i = 0; i < 4 ; i++) {
      startFragReq->lqhLogNode[i] = replicaPtr.p->logNodeId[i];
      startFragReq->startGci[i] = replicaPtr.p->logStartGci[i];
      startFragReq->lastGci[i] = replicaPtr.p->logStopGci[i];
    }//for    

    sendSignal(ref, GSN_START_FRAGREQ, signal, 
	       StartFragReq::SignalLength, JBB);
  }//for
}//Dbdih::sendStartFragreq()

/*************************************************************************/
/*       SET THE INITIAL ACTIVE STATUS ON ALL NODES AND PUT INTO LISTS.  */
/*************************************************************************/
void Dbdih::setInitialActiveStatus()
{
  NodeRecordPtr siaNodeptr;
  Uint32 tsiaNodeActiveStatus;
  Uint32 tsiaNoActiveNodes;

  tsiaNoActiveNodes = csystemnodes - cnoHotSpare;
  for(Uint32 i = 0; i<Sysfile::NODE_STATUS_SIZE; i++)
    SYSFILE->nodeStatus[i] = 0;
  for (siaNodeptr.i = 1; siaNodeptr.i < MAX_NDB_NODES; siaNodeptr.i++) {
    ptrAss(siaNodeptr, nodeRecord);
    if (siaNodeptr.p->nodeStatus == NodeRecord::ALIVE) {
      if (tsiaNoActiveNodes == 0) {
        jam();
        siaNodeptr.p->activeStatus = Sysfile::NS_HotSpare;
      } else {
        jam();
        tsiaNoActiveNodes = tsiaNoActiveNodes - 1;
        siaNodeptr.p->activeStatus = Sysfile::NS_Active;
      }//if
    } else {
      jam();
      siaNodeptr.p->activeStatus = Sysfile::NS_NotDefined;
    }//if
    switch (siaNodeptr.p->activeStatus) {
    case Sysfile::NS_Active:
      jam();
      tsiaNodeActiveStatus = Sysfile::NS_Active;
      break;
    case Sysfile::NS_HotSpare:
      jam();
      tsiaNodeActiveStatus = Sysfile::NS_HotSpare;
      break;
    case Sysfile::NS_NotDefined:
      jam();
      tsiaNodeActiveStatus = Sysfile::NS_NotDefined;
      break;
    default:
      ndbrequire(false);
      return;
      break;
    }//switch
    Sysfile::setNodeStatus(siaNodeptr.i, SYSFILE->nodeStatus,
                           tsiaNodeActiveStatus);
  }//for
}//Dbdih::setInitialActiveStatus()

/*************************************************************************/
/*       SET LCP ACTIVE STATUS AT THE END OF A LOCAL CHECKPOINT.        */
/*************************************************************************/
void Dbdih::setLcpActiveStatusEnd()
{
  NodeRecordPtr nodePtr;

  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    jam();
    ptrAss(nodePtr, nodeRecord);
    if (c_lcpState.m_participatingLQH.get(nodePtr.i)){
      switch (nodePtr.p->activeStatus) {
      case Sysfile::NS_Active:
      case Sysfile::NS_ActiveMissed_1:
      case Sysfile::NS_ActiveMissed_2:
        jam();
	/*-------------------------------------------------------------------*/
	/* THE NODE PARTICIPATED IN THIS CHECKPOINT. 
	 * WE CAN SET ITS STATUS TO ACTIVE */
	/*-------------------------------------------------------------------*/
        nodePtr.p->activeStatus = Sysfile::NS_Active;
        takeOverCompleted(nodePtr.i);
        break;
      case Sysfile::NS_TakeOver:
        jam();
	/*-------------------------------------------------------------------*/
	/* THE NODE HAS COMPLETED A CHECKPOINT AFTER TAKE OVER. WE CAN NOW   */
	/* SET ITS STATUS TO ACTIVE. WE CAN ALSO COMPLETE THE TAKE OVER      */
	/* AND ALSO WE CLEAR THE TAKE OVER NODE IN THE RESTART INFO.         */
	/*-------------------------------------------------------------------*/
        nodePtr.p->activeStatus = Sysfile::NS_Active;
        takeOverCompleted(nodePtr.i);
        break;
      default:
        ndbrequire(false);
        return;
        break;
      }//switch
    }//if
  }//for

  if(getNodeState().getNodeRestartInProgress()){
    jam();
    if(c_lcpState.m_participatingLQH.get(getOwnNodeId())){
      nodePtr.i = getOwnNodeId();
      ptrAss(nodePtr, nodeRecord);
      ndbrequire(nodePtr.p->activeStatus == Sysfile::NS_Active);
      ndbout_c("NR: setLcpActiveStatusEnd - m_participatingLQH");
    } else {
      ndbout_c("NR: setLcpActiveStatusEnd - !m_participatingLQH");
    }
  }
  
  c_lcpState.m_participatingDIH.clear();
  c_lcpState.m_participatingLQH.clear();
  if (isMaster()) {
    jam();
    setNodeRestartInfoBits();
  }//if
}//Dbdih::setLcpActiveStatusEnd()

void Dbdih::takeOverCompleted(Uint32 aNodeId)
{
  TakeOverRecordPtr takeOverPtr;
  takeOverPtr.i = findTakeOver(aNodeId);
  if (takeOverPtr.i != RNIL) {
    jam();
    ptrCheckGuard(takeOverPtr, MAX_NDB_NODES, takeOverRecord);
    if (takeOverPtr.p->toMasterStatus != TakeOverRecord::WAIT_LCP) {
      jam();
      ndbrequire(!isMaster());
      return;
    }//if
    ndbrequire(isMaster());
    Sysfile::setTakeOverNode(aNodeId, SYSFILE->takeOver, 0);
    takeOverPtr.p->toMasterStatus = TakeOverRecord::TO_END_COPY;
    cstartGcpNow = true;
  }//if
}//Dbdih::takeOverCompleted()

/*************************************************************************/
/*       SET LCP ACTIVE STATUS BEFORE STARTING A LOCAL CHECKPOINT.       */
/*************************************************************************/
void Dbdih::setLcpActiveStatusStart(Signal* signal) 
{
  NodeRecordPtr nodePtr;

  c_lcpState.m_participatingLQH.clear();
  c_lcpState.m_participatingDIH.clear();
  
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    ptrAss(nodePtr, nodeRecord);
#if 0    
    if(nodePtr.p->nodeStatus != NodeRecord::NOT_IN_CLUSTER){
      infoEvent("Node %d nodeStatus=%d activeStatus=%d copyCompleted=%d lcp=%d",
		nodePtr.i, 
		nodePtr.p->nodeStatus,
		nodePtr.p->activeStatus,
		nodePtr.p->copyCompleted,
		nodePtr.p->m_inclDihLcp);
    }
#endif
    if(nodePtr.p->nodeStatus == NodeRecord::ALIVE && nodePtr.p->m_inclDihLcp){
      jam();
      c_lcpState.m_participatingDIH.set(nodePtr.i);
    }

    if ((nodePtr.p->nodeStatus == NodeRecord::ALIVE) &&
	(nodePtr.p->copyCompleted)) {
      switch (nodePtr.p->activeStatus) {
      case Sysfile::NS_Active:
        jam();
	/*-------------------------------------------------------------------*/
	// The normal case. Starting a LCP for a started node which hasn't
	// missed the previous LCP.
	/*-------------------------------------------------------------------*/
	c_lcpState.m_participatingLQH.set(nodePtr.i);
        break;
      case Sysfile::NS_ActiveMissed_1:
        jam();
	/*-------------------------------------------------------------------*/
	// The node is starting up and is participating in a local checkpoint
	// as the final phase of the start-up. We can still use the checkpoints
	// on the node after a system restart.
	/*-------------------------------------------------------------------*/
	c_lcpState.m_participatingLQH.set(nodePtr.i);
        break;
      case Sysfile::NS_ActiveMissed_2:
        jam();
	/*-------------------------------------------------------------------*/
	// The node is starting up and is participating in a local checkpoint
	// as the final phase of the start-up. We have missed so 
	// many checkpoints that we no longer can use this node to 
	// recreate fragments from disk.
	// It must be taken over with the copy fragment process after a system
	// crash. We indicate this by setting the active status to TAKE_OVER.
	/*-------------------------------------------------------------------*/
        nodePtr.p->activeStatus = Sysfile::NS_TakeOver;
        //break; // Fall through
      case Sysfile::NS_TakeOver:{
        TakeOverRecordPtr takeOverPtr;
        jam();
	/*-------------------------------------------------------------------*/
	/*      THIS NODE IS CURRENTLY TAKING OVER A FAILED NODE.            */
	/*-------------------------------------------------------------------*/
        takeOverPtr.i = findTakeOver(nodePtr.i);
        if (takeOverPtr.i != RNIL) {
          jam();
          ptrCheckGuard(takeOverPtr, MAX_NDB_NODES, takeOverRecord);
          if (takeOverPtr.p->toMasterStatus == TakeOverRecord::WAIT_LCP) {
            jam();
	    /*---------------------------------------------------------------
	     * ALL THE INFORMATION HAVE BEEN REPLICATED TO THE NEW 
	     * NODE AND WE ARE ONLY WAITING FOR A LOCAL CHECKPOINT TO BE 
	     * PERFORMED ON THE NODE TO SET ITS STATUS TO ACTIVE.   
	     */
	    infoEvent("Node %d is WAIT_LCP including in LCP", nodePtr.i);
	    c_lcpState.m_participatingLQH.set(nodePtr.i);
          }//if
        }//if
        break;
      }
      default:
        jam();
        /*empty*/;
        break;
      }//switch
    } else {
      switch (nodePtr.p->activeStatus) {
      case Sysfile::NS_Active:
        jam();
        nodePtr.p->activeStatus = Sysfile::NS_ActiveMissed_1;
        break;
      case Sysfile::NS_ActiveMissed_1:
        jam();
        nodePtr.p->activeStatus = Sysfile::NS_ActiveMissed_2;
        break;
      case Sysfile::NS_ActiveMissed_2:
        jam();
        if ((nodePtr.p->nodeStatus == NodeRecord::ALIVE) &&
            (!nodePtr.p->copyCompleted)) {
          jam();
	  /*-----------------------------------------------------------------*/
	  // The node is currently starting up and has not completed the 
	  // copy phase.
	  // It will thus be in the TAKE_OVER state.
	  /*-----------------------------------------------------------------*/
          ndbrequire(findTakeOver(nodePtr.i) != RNIL);
          nodePtr.p->activeStatus = Sysfile::NS_TakeOver;
        } else {
          jam();
	  /*-----------------------------------------------------------------*/
	  /* THE NODE IS ACTIVE AND HAS NOT COMPLETED ANY OF THE LAST 3 
	   * CHECKPOINTS */
	  /* WE MUST TAKE IT OUT OF ACTION AND START A NEW NODE TO TAKE OVER.*/
	  /*-----------------------------------------------------------------*/
          nodePtr.p->activeStatus = Sysfile::NS_NotActive_NotTakenOver;
        }//if
        break;
      case Sysfile::NS_TakeOver:
	jam();
	break;
      default:
        jam();
        /*empty*/;
        break;
      }//switch
    }//if
  }//for
  if (isMaster()) {
    jam();
    checkStartTakeOver(signal);
    setNodeRestartInfoBits();
  }//if
}//Dbdih::setLcpActiveStatusStart()

/*************************************************************************/
/* SET NODE ACTIVE STATUS AT SYSTEM RESTART AND WHEN UPDATED BY MASTER   */
/*************************************************************************/
void Dbdih::setNodeActiveStatus() 
{
  NodeRecordPtr snaNodeptr;

  for (snaNodeptr.i = 1; snaNodeptr.i < MAX_NDB_NODES; snaNodeptr.i++) {
    ptrAss(snaNodeptr, nodeRecord);
    const Uint32 tsnaNodeBits = Sysfile::getNodeStatus(snaNodeptr.i,
                                                       SYSFILE->nodeStatus);
    switch (tsnaNodeBits) {
    case Sysfile::NS_Active:
      jam();
      snaNodeptr.p->activeStatus = Sysfile::NS_Active;
      break;
    case Sysfile::NS_ActiveMissed_1:
      jam();
      snaNodeptr.p->activeStatus = Sysfile::NS_ActiveMissed_1;
      break;
    case Sysfile::NS_ActiveMissed_2:
      jam();
      snaNodeptr.p->activeStatus = Sysfile::NS_ActiveMissed_2;
      break;
    case Sysfile::NS_TakeOver:
      jam();
      snaNodeptr.p->activeStatus = Sysfile::NS_TakeOver;
      break;
    case Sysfile::NS_HotSpare:
      jam();
      snaNodeptr.p->activeStatus = Sysfile::NS_HotSpare;
      break;
    case Sysfile::NS_NotActive_NotTakenOver:
      jam();
      snaNodeptr.p->activeStatus = Sysfile::NS_NotActive_NotTakenOver;
      break;
    case Sysfile::NS_NotDefined:
      jam();
      snaNodeptr.p->activeStatus = Sysfile::NS_NotDefined;
      break;
    default:
      ndbrequire(false);
      break;
    }//switch
  }//for
}//Dbdih::setNodeActiveStatus()

/***************************************************************************/
/* SET THE NODE GROUP BASED ON THE RESTART INFORMATION OR AS SET BY MASTER */
/***************************************************************************/
void Dbdih::setNodeGroups()
{
  NodeGroupRecordPtr NGPtr;
  NodeRecordPtr sngNodeptr;
  Uint32 Ti;

  for (Ti = 0; Ti < MAX_NDB_NODES; Ti++) {
    NGPtr.i = Ti;
    ptrAss(NGPtr, nodeGroupRecord);
    NGPtr.p->nodeCount = 0;
  }//for    
  for (sngNodeptr.i = 1; sngNodeptr.i < MAX_NDB_NODES; sngNodeptr.i++) {
    ptrAss(sngNodeptr, nodeRecord);
    Sysfile::ActiveStatus s = 
      (Sysfile::ActiveStatus)Sysfile::getNodeStatus(sngNodeptr.i,
						    SYSFILE->nodeStatus);
    switch (s){
    case Sysfile::NS_Active:
    case Sysfile::NS_ActiveMissed_1:
    case Sysfile::NS_ActiveMissed_2:
    case Sysfile::NS_NotActive_NotTakenOver:
    case Sysfile::NS_TakeOver:
      jam();
      sngNodeptr.p->nodeGroup = Sysfile::getNodeGroup(sngNodeptr.i,
                                                      SYSFILE->nodeGroups);
      NGPtr.i = sngNodeptr.p->nodeGroup;
      ptrCheckGuard(NGPtr, MAX_NDB_NODES, nodeGroupRecord);
      NGPtr.p->nodesInGroup[NGPtr.p->nodeCount] = sngNodeptr.i;
      NGPtr.p->nodeCount++;
      break;
    case Sysfile::NS_HotSpare:
    case Sysfile::NS_NotDefined:
      jam();
      sngNodeptr.p->nodeGroup = ZNIL;
      break;
    default:
      ndbrequire(false);
      return;
      break;
    }//switch
  }//for
  cnoOfNodeGroups = 0;
  for (Ti = 0; Ti < MAX_NDB_NODES; Ti++) {
    jam();
    NGPtr.i = Ti;
    ptrAss(NGPtr, nodeGroupRecord);
    if (NGPtr.p->nodeCount != 0) {
      jam();
      cnoOfNodeGroups++;
    }//if
  }//for
  cnoHotSpare = csystemnodes - (cnoOfNodeGroups * cnoReplicas);
}//Dbdih::setNodeGroups()

/*************************************************************************/
/* SET NODE INFORMATION AFTER RECEIVING RESTART INFORMATION FROM MASTER. */
/* WE TAKE THE OPPORTUNITY TO SYNCHRONISE OUR DATA WITH THE MASTER. IT   */
/* IS ONLY THE MASTER THAT WILL ACT ON THIS DATA. WE WILL KEEP THEM      */
/* UPDATED FOR THE CASE WHEN WE HAVE TO BECOME MASTER.                   */
/*************************************************************************/
void Dbdih::setNodeInfo(Signal* signal) 
{
  setNodeActiveStatus();
  setNodeGroups();
  sendHOT_SPAREREP(signal);
}//Dbdih::setNodeInfo()

/*************************************************************************/
// Keep also DBDICT informed about the Hot Spare situation in the cluster.
/*************************************************************************/
void Dbdih::sendHOT_SPAREREP(Signal* signal)
{
  NodeRecordPtr locNodeptr;
  Uint32 Ti = 0;
  HotSpareRep * const hotSpare = (HotSpareRep*)&signal->theData[0];
  NodeBitmask::clear(hotSpare->theHotSpareNodes);
  for (locNodeptr.i = 1; locNodeptr.i < MAX_NDB_NODES; locNodeptr.i++) {
    ptrAss(locNodeptr, nodeRecord);
    switch (locNodeptr.p->activeStatus) {
    case Sysfile::NS_HotSpare:
      jam();
      NodeBitmask::set(hotSpare->theHotSpareNodes, locNodeptr.i);
      Ti++;
      break;
    default:
      jam();
      break;
    }//switch
  }//for
  hotSpare->noHotSpareNodes = Ti;
  sendSignal(DBDICT_REF, GSN_HOT_SPAREREP,
             signal, HotSpareRep::SignalLength, JBB);
}//Dbdih::sendHOT_SPAREREP()

/*************************************************************************/
/*       SET LCP ACTIVE STATUS FOR ALL NODES BASED ON THE INFORMATION IN */
/*       THE RESTART INFORMATION.                                        */
/*************************************************************************/
#if 0
void Dbdih::setNodeLcpActiveStatus()
{
  c_lcpState.m_lcpActiveStatus.clear();
  for (Uint32 i = 1; i < MAX_NDB_NODES; i++) {
    if (NodeBitmask::get(SYSFILE->lcpActive, i)) {
      jam();
      c_lcpState.m_lcpActiveStatus.set(i);
    }//if
  }//for
}//Dbdih::setNodeLcpActiveStatus()
#endif

/*************************************************************************/
/* SET THE RESTART INFO BITS BASED ON THE NODES ACTIVE STATUS.           */
/*************************************************************************/
void Dbdih::setNodeRestartInfoBits() 
{
  NodeRecordPtr nodePtr;
  Uint32 tsnrNodeGroup;
  Uint32 tsnrNodeActiveStatus;
  Uint32 i; 
  for(i = 1; i < MAX_NDB_NODES; i++){
    Sysfile::setNodeStatus(i, SYSFILE->nodeStatus, Sysfile::NS_Active);
  }//for
  for(i = 1; i < Sysfile::NODE_GROUPS_SIZE; i++){
    SYSFILE->nodeGroups[i] = 0;
  }//for
  NdbNodeBitmask::clear(SYSFILE->lcpActive);
  
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    ptrAss(nodePtr, nodeRecord);
    switch (nodePtr.p->activeStatus) {
    case Sysfile::NS_Active:
      jam();
      tsnrNodeActiveStatus = Sysfile::NS_Active;
      break;
    case Sysfile::NS_ActiveMissed_1:
      jam();
      tsnrNodeActiveStatus = Sysfile::NS_ActiveMissed_1;
      break;
    case Sysfile::NS_ActiveMissed_2:
      jam();
      tsnrNodeActiveStatus = Sysfile::NS_ActiveMissed_2;
      break;
    case Sysfile::NS_HotSpare:
      jam();
      tsnrNodeActiveStatus = Sysfile::NS_HotSpare;
      break;
    case Sysfile::NS_TakeOver:
      jam();
      tsnrNodeActiveStatus = Sysfile::NS_TakeOver;
      break;
    case Sysfile::NS_NotActive_NotTakenOver:
      jam();
      tsnrNodeActiveStatus = Sysfile::NS_NotActive_NotTakenOver;
      break;
    case Sysfile::NS_NotDefined:
      jam();
      tsnrNodeActiveStatus = Sysfile::NS_NotDefined;
      break;
    default:
      ndbrequire(false);
      tsnrNodeActiveStatus = Sysfile::NS_NotDefined; // remove warning
      break;
    }//switch
    Sysfile::setNodeStatus(nodePtr.i, SYSFILE->nodeStatus, 
                           tsnrNodeActiveStatus);
    if (nodePtr.p->nodeGroup == ZNIL) {
      jam();
      tsnrNodeGroup = NO_NODE_GROUP_ID;
    } else {
      jam();
      tsnrNodeGroup = nodePtr.p->nodeGroup;
    }//if
    Sysfile::setNodeGroup(nodePtr.i, SYSFILE->nodeGroups, tsnrNodeGroup);
    if (c_lcpState.m_participatingLQH.get(nodePtr.i)){
      jam();
      NodeBitmask::set(SYSFILE->lcpActive, nodePtr.i);
    }//if
  }//for
}//Dbdih::setNodeRestartInfoBits()

/*************************************************************************/
/*       START THE GLOBAL CHECKPOINT PROTOCOL IN MASTER AT START-UP      */
/*************************************************************************/
void Dbdih::startGcp(Signal* signal) 
{
  cgcpStatus = GCP_READY;
  coldGcpStatus = cgcpStatus;
  coldGcpId = cnewgcp;
  cgcpSameCounter = 0;
  signal->theData[0] = DihContinueB::ZSTART_GCP;
  signal->theData[1] = 0;
  sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
  signal->theData[0] = DihContinueB::ZCHECK_GCP_STOP;
  sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 1);
}//Dbdih::startGcp()

void Dbdih::updateNodeInfo(FragmentstorePtr fragPtr)
{
  ReplicaRecordPtr replicatePtr;
  Uint32 index = 0;
  replicatePtr.i = fragPtr.p->storedReplicas;
  do {
    jam();
    ptrCheckGuard(replicatePtr, creplicaFileSize, replicaRecord);
    ndbrequire(index < MAX_REPLICAS);
    fragPtr.p->activeNodes[index] = replicatePtr.p->procNode;
    index++;
    replicatePtr.i = replicatePtr.p->nextReplica;
  } while (replicatePtr.i != RNIL);
  fragPtr.p->fragReplicas = index;

  /* ----------------------------------------------------------------------- */
  // We switch primary to the preferred primary if the preferred primary is
  // in the list.
  /* ----------------------------------------------------------------------- */
  const Uint32 prefPrim = fragPtr.p->preferredPrimary;
  for (Uint32 i = 1; i < index; i++) {
    jam();
    ndbrequire(i < MAX_REPLICAS);
    if (fragPtr.p->activeNodes[i] == prefPrim){
      jam();
      Uint32 switchNode = fragPtr.p->activeNodes[0];
      fragPtr.p->activeNodes[0] = prefPrim;
      fragPtr.p->activeNodes[i] = switchNode;
      break;
    }//if
  }//for
}//Dbdih::updateNodeInfo()

void Dbdih::writeFragment(RWFragment* wf, FragmentstorePtr fragPtr) 
{
  writePageWord(wf, wf->fragId);
  writePageWord(wf, fragPtr.p->preferredPrimary);
  writePageWord(wf, fragPtr.p->noStoredReplicas);
  writePageWord(wf, fragPtr.p->noOldStoredReplicas);
  writePageWord(wf, fragPtr.p->distributionKey);
}//Dbdih::writeFragment()

void Dbdih::writePageWord(RWFragment* wf, Uint32 dataWord)
{
  if (wf->wordIndex >= 2048) {
    jam();
    ndbrequire(wf->wordIndex == 2048);
    allocpage(wf->rwfPageptr);
    wf->wordIndex = 32;
    wf->pageIndex++;
    ndbrequire(wf->pageIndex < 8);
    wf->rwfTabPtr.p->pageRef[wf->pageIndex] = wf->rwfPageptr.i;
    wf->rwfTabPtr.p->noPages++;
  }//if
  wf->rwfPageptr.p->word[wf->wordIndex] = dataWord;
  wf->wordIndex++;
}//Dbdih::writePageWord()

void Dbdih::writeReplicas(RWFragment* wf, Uint32 replicaStartIndex) 
{
  ReplicaRecordPtr wfReplicaPtr;
  wfReplicaPtr.i = replicaStartIndex;
  while (wfReplicaPtr.i != RNIL) {
    jam();
    ptrCheckGuard(wfReplicaPtr, creplicaFileSize, replicaRecord);
    writePageWord(wf, wfReplicaPtr.p->procNode);
    writePageWord(wf, wfReplicaPtr.p->initialGci);
    writePageWord(wf, wfReplicaPtr.p->noCrashedReplicas);
    writePageWord(wf, wfReplicaPtr.p->nextLcp);
    Uint32 i;
    for (i = 0; i < MAX_LCP_STORED; i++) {
      writePageWord(wf, wfReplicaPtr.p->maxGciCompleted[i]);
      writePageWord(wf, wfReplicaPtr.p->maxGciStarted[i]);
      writePageWord(wf, wfReplicaPtr.p->lcpId[i]);
      writePageWord(wf, wfReplicaPtr.p->lcpStatus[i]);
    }//if
    for (i = 0; i < 8; i++) {
      writePageWord(wf, wfReplicaPtr.p->createGci[i]);
      writePageWord(wf, wfReplicaPtr.p->replicaLastGci[i]);
    }//if

    wfReplicaPtr.i = wfReplicaPtr.p->nextReplica;
  }//while
}//Dbdih::writeReplicas()

void Dbdih::writeRestorableGci(Signal* signal, FileRecordPtr filePtr)
{
  for (Uint32 i = 0; i < Sysfile::SYSFILE_SIZE32; i++) {
    sysfileDataToFile[i] = sysfileData[i];
  }//for
  signal->theData[0] = filePtr.p->fileRef;
  signal->theData[1] = reference();
  signal->theData[2] = filePtr.i;
  signal->theData[3] = ZLIST_OF_PAIRS_SYNCH;
  signal->theData[4] = ZVAR_NO_CRESTART_INFO_TO_FILE;
  signal->theData[5] = 1; /* AMOUNT OF PAGES */
  signal->theData[6] = 0; /* MEMORY PAGE = 0 SINCE COMMON STORED VARIABLE  */
  signal->theData[7] = 0;
  sendSignal(NDBFS_REF, GSN_FSWRITEREQ, signal, 8, JBA);
}//Dbdih::writeRestorableGci()

void Dbdih::writeTabfile(Signal* signal, TabRecord* tab, FileRecordPtr filePtr) 
{
  signal->theData[0] = filePtr.p->fileRef;
  signal->theData[1] = reference();
  signal->theData[2] = filePtr.i;
  signal->theData[3] = ZLIST_OF_PAIRS;
  signal->theData[4] = ZVAR_NO_WORD;
  signal->theData[5] = tab->noPages;
  for (Uint32 i = 0; i < tab->noPages; i++) {
    jam();
    signal->theData[6 + (2 * i)] = tab->pageRef[i];
    signal->theData[7 + (2 * i)] = i;
  }//for
  Uint32 length = 6 + (2 * tab->noPages);
  sendSignal(NDBFS_REF, GSN_FSWRITEREQ, signal, length, JBA);
}//Dbdih::writeTabfile()

void Dbdih::execDEBUG_SIG(Signal* signal) 
{
  signal = signal; //Avoid compiler warnings
}//Dbdih::execDEBUG_SIG()

void
Dbdih::execDUMP_STATE_ORD(Signal* signal)
{
  DumpStateOrd * const & dumpState = (DumpStateOrd *)&signal->theData[0];
  if (dumpState->args[0] == DumpStateOrd::DihDumpNodeRestartInfo) {
    infoEvent("c_nodeStartMaster.blockLcp = %d, c_nodeStartMaster.blockGcp = %d, c_nodeStartMaster.wait = %d",
	      c_nodeStartMaster.blockLcp, c_nodeStartMaster.blockGcp, c_nodeStartMaster.wait);
    infoEvent("cstartGcpNow = %d, cgcpStatus = %d",
              cstartGcpNow, cgcpStatus);
    infoEvent("cfirstVerifyQueue = %d, cverifyQueueCounter = %d",
              cfirstVerifyQueue, cverifyQueueCounter);
    infoEvent("cgcpOrderBlocked = %d, cgcpStartCounter = %d",
              cgcpOrderBlocked, cgcpStartCounter);
  }//if  
  if (dumpState->args[0] == DumpStateOrd::DihDumpNodeStatusInfo) {
    NodeRecordPtr localNodePtr;
    infoEvent("Printing nodeStatus of all nodes");
    for (localNodePtr.i = 1; localNodePtr.i < MAX_NDB_NODES; localNodePtr.i++) {
      ptrAss(localNodePtr, nodeRecord);
      if (localNodePtr.p->nodeStatus != NodeRecord::NOT_IN_CLUSTER) {
        infoEvent("Node = %d has status = %d",
		  localNodePtr.i, localNodePtr.p->nodeStatus);
      }//if
    }//for
  }//if
  
  if (dumpState->args[0] == DumpStateOrd::DihPrintFragmentation){
    infoEvent("Printing fragmentation of all tables --");
    for(Uint32 i = 0; i<ctabFileSize; i++){
      TabRecordPtr tabPtr;
      tabPtr.i = i;
      ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
      
      if(tabPtr.p->tabStatus != TabRecord::TS_ACTIVE)
	continue;
      
      for(Uint32 j = 0; j < tabPtr.p->totalfragments; j++){
	FragmentstorePtr fragPtr;
	getFragstore(tabPtr.p, j, fragPtr);
	
	Uint32 nodeOrder[MAX_REPLICAS];
	const Uint32 noOfReplicas = extractNodeInfo(fragPtr.p, nodeOrder);
	char buf[100];
	BaseString::snprintf(buf, sizeof(buf), " Table %d Fragment %d - ", tabPtr.i, j);
	for(Uint32 k = 0; k < noOfReplicas; k++){
	  char tmp[100];
	  BaseString::snprintf(tmp, sizeof(tmp), "%d ", nodeOrder[k]);
	  strcat(buf, tmp);
	}
	infoEvent(buf);
      }
    }
  }
  
  if (signal->theData[0] == 7000) {
    infoEvent("ctimer = %d, cgcpParticipantState = %d, cgcpStatus = %d",
              c_lcpState.ctimer, cgcpParticipantState, cgcpStatus);
    infoEvent("coldGcpStatus = %d, coldGcpId = %d, cmasterState = %d",
              coldGcpStatus, coldGcpId, cmasterState);
    infoEvent("cmasterTakeOverNode = %d, ctcCounter = %d",
              cmasterTakeOverNode, c_lcpState.ctcCounter);
  }//if  
  if (signal->theData[0] == 7001) {
    infoEvent("c_lcpState.keepGci = %d",
              c_lcpState.keepGci);
    infoEvent("c_lcpState.lcpStatus = %d, clcpStartGcp = %d",
              c_lcpState.lcpStatus, 
	      c_lcpState.lcpStartGcp);
    infoEvent("cgcpStartCounter = %d, cimmediateLcpStart = %d",
              cgcpStartCounter, c_lcpState.immediateLcpStart);
  }//if  
  if (signal->theData[0] == 7002) {
    infoEvent("cnoOfActiveTables = %d, cgcpDelay = %d",
              cnoOfActiveTables, cgcpDelay);
    infoEvent("cdictblockref = %d, cfailurenr = %d",
              cdictblockref, cfailurenr);
    infoEvent("con_lineNodes = %d, reference() = %d, creceivedfrag = %d",
              con_lineNodes, reference(), creceivedfrag);
  }//if  
  if (signal->theData[0] == 7003) {
    infoEvent("cfirstAliveNode = %d, cgckptflag = %d",
              cfirstAliveNode, cgckptflag);
    infoEvent("clocallqhblockref = %d, clocaltcblockref = %d, cgcpOrderBlocked = %d",
              clocallqhblockref, clocaltcblockref, cgcpOrderBlocked);
    infoEvent("cstarttype = %d, csystemnodes = %d, currentgcp = %d",
              cstarttype, csystemnodes, currentgcp);
  }//if  
  if (signal->theData[0] == 7004) {
    infoEvent("cmasterdihref = %d, cownNodeId = %d, cnewgcp = %d",
              cmasterdihref, cownNodeId, cnewgcp);
    infoEvent("cndbStartReqBlockref = %d, cremainingfrags = %d",
              cndbStartReqBlockref, cremainingfrags);
    infoEvent("cntrlblockref = %d, cgcpSameCounter = %d, coldgcp = %d",
              cntrlblockref, cgcpSameCounter, coldgcp);
  }//if  
  if (signal->theData[0] == 7005) {
    infoEvent("crestartGci = %d",
              crestartGci);
  }//if  
  if (signal->theData[0] == 7006) {
    infoEvent("clcpDelay = %d, cgcpMasterTakeOverState = %d",
              c_lcpState.clcpDelay, cgcpMasterTakeOverState);
    infoEvent("cmasterNodeId = %d", cmasterNodeId);
    infoEvent("cnoHotSpare = %d, c_nodeStartMaster.startNode = %d, c_nodeStartMaster.wait = %d",
              cnoHotSpare, c_nodeStartMaster.startNode, c_nodeStartMaster.wait);
  }//if  
  if (signal->theData[0] == 7007) {
    infoEvent("c_nodeStartMaster.failNr = %d", c_nodeStartMaster.failNr);
    infoEvent("c_nodeStartMaster.startInfoErrorCode = %d",
              c_nodeStartMaster.startInfoErrorCode);
    infoEvent("c_nodeStartMaster.blockLcp = %d, c_nodeStartMaster.blockGcp = %d",
              c_nodeStartMaster.blockLcp, c_nodeStartMaster.blockGcp);
  }//if  
  if (signal->theData[0] == 7008) {
    infoEvent("cfirstDeadNode = %d, cstartPhase = %d, cnoReplicas = %d",
              cfirstDeadNode, cstartPhase, cnoReplicas);
    infoEvent("cwaitLcpSr = %d",cwaitLcpSr);
  }//if  
  if (signal->theData[0] == 7009) {
    infoEvent("ccalcOldestRestorableGci = %d, cnoOfNodeGroups = %d",
              c_lcpState.oldestRestorableGci, cnoOfNodeGroups);
    infoEvent("cstartGcpNow = %d",
              cstartGcpNow);
    infoEvent("crestartGci = %d",
              crestartGci);
  }//if  
  if (signal->theData[0] == 7010) {
    infoEvent("cminHotSpareNodes = %d, c_lcpState.lcpStatusUpdatedPlace = %d, cLcpStart = %d",
              cminHotSpareNodes, c_lcpState.lcpStatusUpdatedPlace, c_lcpState.lcpStart);
    infoEvent("c_blockCommit = %d, c_blockCommitNo = %d",
              c_blockCommit, c_blockCommitNo);
  }//if  
  if (signal->theData[0] == 7011){
    infoEvent("c_COPY_GCIREQ_Counter = %s", 
	      c_COPY_GCIREQ_Counter.getText());
    infoEvent("c_COPY_TABREQ_Counter = %s", 
	      c_COPY_TABREQ_Counter.getText());
    infoEvent("c_CREATE_FRAGREQ_Counter = %s", 
	      c_CREATE_FRAGREQ_Counter.getText());
    infoEvent("c_DIH_SWITCH_REPLICA_REQ_Counter = %s", 
	      c_DIH_SWITCH_REPLICA_REQ_Counter.getText());
    infoEvent("c_EMPTY_LCP_REQ_Counter = %s",c_EMPTY_LCP_REQ_Counter.getText());
    infoEvent("c_END_TOREQ_Counter = %s", c_END_TOREQ_Counter.getText());
    infoEvent("c_GCP_COMMIT_Counter = %s", c_GCP_COMMIT_Counter.getText());
    infoEvent("c_GCP_PREPARE_Counter = %s", c_GCP_PREPARE_Counter.getText());
    infoEvent("c_GCP_SAVEREQ_Counter = %s", c_GCP_SAVEREQ_Counter.getText());
    infoEvent("c_INCL_NODEREQ_Counter = %s", c_INCL_NODEREQ_Counter.getText());
    infoEvent("c_MASTER_GCPREQ_Counter = %s", 
	      c_MASTER_GCPREQ_Counter.getText());
    infoEvent("c_MASTER_LCPREQ_Counter = %s", 
	      c_MASTER_LCPREQ_Counter.getText());
    infoEvent("c_START_INFOREQ_Counter = %s", 
	      c_START_INFOREQ_Counter.getText());
    infoEvent("c_START_RECREQ_Counter = %s", c_START_RECREQ_Counter.getText());
    infoEvent("c_START_TOREQ_Counter = %s", c_START_TOREQ_Counter.getText());
    infoEvent("c_STOP_ME_REQ_Counter = %s", c_STOP_ME_REQ_Counter.getText());
    infoEvent("c_TC_CLOPSIZEREQ_Counter = %s", 
	      c_TC_CLOPSIZEREQ_Counter.getText());
    infoEvent("c_TCGETOPSIZEREQ_Counter = %s", 
	      c_TCGETOPSIZEREQ_Counter.getText());
    infoEvent("c_UPDATE_TOREQ_Counter = %s", c_UPDATE_TOREQ_Counter.getText());
  }

  if(signal->theData[0] == 7012){
    char buf[8*_NDB_NODE_BITMASK_SIZE+1];
    infoEvent("ParticipatingDIH = %s", c_lcpState.m_participatingDIH.getText(buf));
    infoEvent("ParticipatingLQH = %s", c_lcpState.m_participatingLQH.getText(buf));
    infoEvent("m_LCP_COMPLETE_REP_Counter_DIH = %s",
	      c_lcpState.m_LCP_COMPLETE_REP_Counter_DIH.getText());
    infoEvent("m_LCP_COMPLETE_REP_Counter_LQH = %s",
	      c_lcpState.m_LCP_COMPLETE_REP_Counter_LQH.getText());
    infoEvent("m_LAST_LCP_FRAG_ORD = %s",
	      c_lcpState.m_LAST_LCP_FRAG_ORD.getText());
    infoEvent("m_LCP_COMPLETE_REP_From_Master_Received = %d",
	      c_lcpState.m_LCP_COMPLETE_REP_From_Master_Received);
    
    NodeRecordPtr nodePtr;
    for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
      jam();
      ptrAss(nodePtr, nodeRecord);
      if(nodePtr.p->nodeStatus == NodeRecord::ALIVE){
        Uint32 i;
	for(i = 0; i<nodePtr.p->noOfStartedChkpt; i++){
	  infoEvent("Node %d: started: table=%d fragment=%d replica=%d",
		    nodePtr.i, 
		    nodePtr.p->startedChkpt[i].tableId,
		    nodePtr.p->startedChkpt[i].fragId,
		    nodePtr.p->startedChkpt[i].replicaPtr);
	}
	
	for(i = 0; i<nodePtr.p->noOfQueuedChkpt; i++){
	  infoEvent("Node %d: queued: table=%d fragment=%d replica=%d",
		    nodePtr.i, 
		    nodePtr.p->queuedChkpt[i].tableId,
		    nodePtr.p->queuedChkpt[i].fragId,
		    nodePtr.p->queuedChkpt[i].replicaPtr);
	}
      }
    }
  }

  if(dumpState->args[0] == 7019 && signal->getLength() == 2)
  {
    char buf2[8+1];
    NodeRecordPtr nodePtr;
    nodePtr.i = signal->theData[1];
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
    infoEvent("NF Node %d tc: %d lqh: %d dih: %d dict: %d recNODE_FAILREP: %d",
	      nodePtr.i,
	      nodePtr.p->dbtcFailCompleted,
	      nodePtr.p->dblqhFailCompleted,
	      nodePtr.p->dbdihFailCompleted,
	      nodePtr.p->dbdictFailCompleted,
	      nodePtr.p->recNODE_FAILREP);
    infoEvent(" m_NF_COMPLETE_REP: %s m_nodefailSteps: %s",
	      nodePtr.p->m_NF_COMPLETE_REP.getText(),
	      nodePtr.p->m_nodefailSteps.getText(buf2));
  }
  
  if(dumpState->args[0] == 7020 && signal->getLength() > 3)
  {
    Uint32 gsn= signal->theData[1];
    Uint32 block= signal->theData[2];
    Uint32 length= signal->length() - 3;
    memmove(signal->theData, signal->theData+3, 4*length);
    sendSignal(numberToRef(block, getOwnNodeId()), gsn, signal, length, JBB);
    
    warningEvent("-- SENDING CUSTOM SIGNAL --");
    char buf[100], buf2[100];
    buf2[0]= 0;
    for(Uint32 i = 0; i<length; i++)
    {
      snprintf(buf, 100, "%s %.8x", buf2, signal->theData[i]);
      snprintf(buf2, 100, "%s", buf);
    }
    warningEvent("gsn: %d block: %s, length: %d theData: %s", 
		 gsn, getBlockName(block, "UNKNOWN"), length, buf);

    g_eventLogger.warning("-- SENDING CUSTOM SIGNAL --");
    g_eventLogger.warning("gsn: %d block: %s, length: %d theData: %s", 
			  gsn, getBlockName(block, "UNKNOWN"), length, buf);
  }
  
  if(dumpState->args[0] == DumpStateOrd::DihDumpLCPState){
    infoEvent("-- Node %d LCP STATE --", getOwnNodeId());
    infoEvent("lcpStatus = %d (update place = %d) ",
	      c_lcpState.lcpStatus, c_lcpState.lcpStatusUpdatedPlace);
    infoEvent
      ("lcpStart = %d lcpStartGcp = %d keepGci = %d oldestRestorable = %d",
       c_lcpState.lcpStart, c_lcpState.lcpStartGcp, 
       c_lcpState.keepGci, c_lcpState.oldestRestorableGci);
    
    infoEvent
      ("immediateLcpStart = %d masterLcpNodeId = %d",
       c_lcpState.immediateLcpStart,
       refToNode(c_lcpState.m_masterLcpDihRef));
    infoEvent("-- Node %d LCP STATE --", getOwnNodeId());
  }

  if(dumpState->args[0] == DumpStateOrd::DihDumpLCPMasterTakeOver){
    infoEvent("-- Node %d LCP MASTER TAKE OVER STATE --", getOwnNodeId());
    infoEvent
      ("c_lcpMasterTakeOverState.state = %d updatePlace = %d failedNodeId = %d",
       c_lcpMasterTakeOverState.state,
       c_lcpMasterTakeOverState.updatePlace,
       c_lcpMasterTakeOverState.failedNodeId);
    
    infoEvent("c_lcpMasterTakeOverState.minTableId = %u minFragId = %u",
	      c_lcpMasterTakeOverState.minTableId,
	      c_lcpMasterTakeOverState.minFragId);
    
    infoEvent("-- Node %d LCP MASTER TAKE OVER STATE --", getOwnNodeId());
  }

  if (signal->theData[0] == 7015){
    for(Uint32 i = 0; i<ctabFileSize; i++){
      TabRecordPtr tabPtr;
      tabPtr.i = i;
      ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
      
      if(tabPtr.p->tabStatus != TabRecord::TS_ACTIVE)
	continue;
      
      infoEvent
	("Table %d: TabCopyStatus: %d TabUpdateStatus: %d TabLcpStatus: %d",
	 tabPtr.i, 
	 tabPtr.p->tabCopyStatus, 
	 tabPtr.p->tabUpdateState,
	 tabPtr.p->tabLcpStatus);

      FragmentstorePtr fragPtr;
      for (Uint32 fid = 0; fid < tabPtr.p->totalfragments; fid++) {
	jam();
	getFragstore(tabPtr.p, fid, fragPtr);
	
	char buf[100], buf2[100];
	BaseString::snprintf(buf, sizeof(buf), " Fragment %d: noLcpReplicas==%d ", 
		 fid, fragPtr.p->noLcpReplicas);
	
	Uint32 num=0;
	ReplicaRecordPtr replicaPtr;
	replicaPtr.i = fragPtr.p->storedReplicas;
	do {
	  ptrCheckGuard(replicaPtr, creplicaFileSize, replicaRecord);
	  BaseString::snprintf(buf2, sizeof(buf2), "%s %d(on %d)=%d(%s)",
		   buf, num, 
		   replicaPtr.p->procNode, 
		   replicaPtr.p->lcpIdStarted,
		   replicaPtr.p->lcpOngoingFlag ? "Ongoing" : "Idle");
	  BaseString::snprintf(buf, sizeof(buf), "%s", buf2);
	  
	  num++;
	  replicaPtr.i = replicaPtr.p->nextReplica;
	} while (replicaPtr.i != RNIL);
	infoEvent(buf);
      }
    }
  }

  if(dumpState->args[0] == DumpStateOrd::EnableUndoDelayDataWrite){
    ndbout << "Dbdih:: delay write of datapages for table = " 
	   << dumpState->args[1]<< endl;
    // Send this dump to ACC and TUP
    EXECUTE_DIRECT(DBACC, GSN_DUMP_STATE_ORD, signal, 2);
    EXECUTE_DIRECT(DBTUP, GSN_DUMP_STATE_ORD, signal, 2);
    
    // Start immediate LCP
    c_lcpState.ctimer += (1 << c_lcpState.clcpDelay);
    return;
  }

  if (signal->theData[0] == DumpStateOrd::DihAllAllowNodeStart) {
    for (Uint32 i = 1; i < MAX_NDB_NODES; i++)
      setAllowNodeStart(i, true);
    return;
  }//if
  if (signal->theData[0] == DumpStateOrd::DihMinTimeBetweenLCP) {
    // Set time between LCP to min value
    ndbout << "Set time between LCP to min value" << endl;
    c_lcpState.clcpDelay = 0; // TimeBetweenLocalCheckpoints.min
    return;
  }
  if (signal->theData[0] == DumpStateOrd::DihMaxTimeBetweenLCP) {
    // Set time between LCP to max value
    ndbout << "Set time between LCP to max value" << endl;
    c_lcpState.clcpDelay = 31; // TimeBetweenLocalCheckpoints.max
    return;
  }
  
  if(dumpState->args[0] == 7098){
    if(signal->length() == 3){
      jam();
      infoEvent("startLcpRoundLoopLab(tabel=%d, fragment=%d)",
		signal->theData[1], signal->theData[2]);
      startLcpRoundLoopLab(signal, signal->theData[1], signal->theData[2]);
      return;
    } else {
      infoEvent("Invalid no of arguments to 7098 - startLcpRoundLoopLab -"
		" expected 2 (tableId, fragmentId)");
    }
  }

  if(dumpState->args[0] == DumpStateOrd::DihStartLcpImmediately){
    c_lcpState.ctimer += (1 << c_lcpState.clcpDelay);
    return;
  }
}//Dbdih::execDUMP_STATE_ORD()

void
Dbdih::execPREP_DROP_TAB_REQ(Signal* signal){
  jamEntry();
  
  PrepDropTabReq* req = (PrepDropTabReq*)signal->getDataPtr();
  
  TabRecordPtr tabPtr;
  tabPtr.i = req->tableId;
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
  
  Uint32 senderRef = req->senderRef;
  Uint32 senderData = req->senderData;
  
  PrepDropTabRef::ErrorCode err = PrepDropTabRef::OK;
  { /**
     * Check table state
     */
    bool ok = false;
    switch(tabPtr.p->tabStatus){
    case TabRecord::TS_IDLE:
      ok = true;
      jam();
      err = PrepDropTabRef::NoSuchTable;
      break;
    case TabRecord::TS_DROPPING:
      ok = true;
      jam();
      err = PrepDropTabRef::PrepDropInProgress;
      break;
    case TabRecord::TS_CREATING:
      jam();
      ok = true;
      break;
    case TabRecord::TS_ACTIVE:
      ok = true;
      jam();
      break;
    }
    ndbrequire(ok);
  }

  if(err != PrepDropTabRef::OK){
    jam();
    PrepDropTabRef* ref = (PrepDropTabRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = senderData;
    ref->tableId = tabPtr.i;
    ref->errorCode = err;
    sendSignal(senderRef, GSN_PREP_DROP_TAB_REF, signal,
	       PrepDropTabRef::SignalLength, JBB);
    return;
  }

  tabPtr.p->tabStatus = TabRecord::TS_DROPPING;
  tabPtr.p->m_prepDropTab.senderRef = senderRef;
  tabPtr.p->m_prepDropTab.senderData = senderData;
  
  if(isMaster()){
    /**
     * Remove from queue
     */
    NodeRecordPtr nodePtr;
    for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
      jam();
      ptrAss(nodePtr, nodeRecord);
      if (c_lcpState.m_participatingLQH.get(nodePtr.i)){
	
	Uint32 index = 0;
	Uint32 count = nodePtr.p->noOfQueuedChkpt;
	while(index < count){
	  if(nodePtr.p->queuedChkpt[index].tableId == tabPtr.i){
	    jam();
	    //	    ndbout_c("Unqueuing %d", index);
	    
	    count--;
	    for(Uint32 i = index; i<count; i++){
	      jam();
	      nodePtr.p->queuedChkpt[i] = nodePtr.p->queuedChkpt[i + 1];
	    }
	  } else {
	    index++;
	  }
	}
	nodePtr.p->noOfQueuedChkpt = count;
      }
    }
  }
  
  { /**
     * Check table lcp state
     */
    
    bool ok = false;
    switch(tabPtr.p->tabLcpStatus){
    case TabRecord::TLS_COMPLETED:
    case TabRecord::TLS_WRITING_TO_FILE:
      ok = true;
      jam();
      break;
      return;
    case TabRecord::TLS_ACTIVE:
      ok = true;
      jam();
      
      tabPtr.p->tabLcpStatus = TabRecord::TLS_COMPLETED;
      
      /**
       * First check if all fragments are done
       */
      if(checkLcpAllTablesDoneInLqh()){
	jam();
	
	ndbout_c("This is the last table");
	
	/**
	 * Then check if saving of tab info is done for all tables
	 */
	LcpStatus a = c_lcpState.lcpStatus;
	checkLcpCompletedLab(signal);
	
	if(a != c_lcpState.lcpStatus){
	  ndbout_c("And all tables are written to already written disk");
	}
      }
      break;
    }
    ndbrequire(ok);
  }  
  
  { /**
     * Send WaitDropTabReq to all LQH
     */
    WaitDropTabReq * req = (WaitDropTabReq*)signal->getDataPtrSend();
    req->tableId = tabPtr.i;
    req->senderRef = reference();
    
    NodeRecordPtr nodePtr;
    nodePtr.i = cfirstAliveNode;
    tabPtr.p->m_prepDropTab.waitDropTabCount.clearWaitingFor();
    while(nodePtr.i != RNIL){
      jam();
      ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
      
      tabPtr.p->m_prepDropTab.waitDropTabCount.setWaitingFor(nodePtr.i);
      sendSignal(calcLqhBlockRef(nodePtr.i), GSN_WAIT_DROP_TAB_REQ,
		 signal, WaitDropTabReq::SignalLength, JBB);
      
      nodePtr.i = nodePtr.p->nextNode;
    }
  }
  
  waitDropTabWritingToFile(signal, tabPtr);
}

void
Dbdih::waitDropTabWritingToFile(Signal* signal, TabRecordPtr tabPtr){
  
  if(tabPtr.p->tabLcpStatus == TabRecord::TLS_WRITING_TO_FILE){
    jam();
    signal->theData[0] = DihContinueB::WAIT_DROP_TAB_WRITING_TO_FILE;
    signal->theData[1] = tabPtr.i;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 2);
    return;
  }

  ndbrequire(tabPtr.p->tabLcpStatus ==  TabRecord::TLS_COMPLETED);
  checkPrepDropTabComplete(signal, tabPtr);
}

void
Dbdih::checkPrepDropTabComplete(Signal* signal, TabRecordPtr tabPtr){
  
  if(tabPtr.p->tabLcpStatus !=  TabRecord::TLS_COMPLETED){
    jam();
    return;
  }
  
  if(!tabPtr.p->m_prepDropTab.waitDropTabCount.done()){
    jam();
    return;
  }
  
  const Uint32 ref = tabPtr.p->m_prepDropTab.senderRef;
  if(ref != 0){
    PrepDropTabConf* conf = (PrepDropTabConf*)signal->getDataPtrSend();
    conf->tableId = tabPtr.i;
    conf->senderRef = reference();
    conf->senderData = tabPtr.p->m_prepDropTab.senderData;
    sendSignal(tabPtr.p->m_prepDropTab.senderRef, GSN_PREP_DROP_TAB_CONF, 
	       signal, PrepDropTabConf::SignalLength, JBB);
    tabPtr.p->m_prepDropTab.senderRef = 0;
  }
}
			
void
Dbdih::execWAIT_DROP_TAB_REF(Signal* signal){
  jamEntry();
  WaitDropTabRef * ref = (WaitDropTabRef*)signal->getDataPtr();
  
  TabRecordPtr tabPtr;
  tabPtr.i = ref->tableId;
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
  
  ndbrequire(tabPtr.p->tabStatus == TabRecord::TS_DROPPING);
  Uint32 nodeId = refToNode(ref->senderRef);
 
  ndbrequire(ref->errorCode == WaitDropTabRef::NoSuchTable ||
	     ref->errorCode == WaitDropTabRef::NF_FakeErrorREF);

  tabPtr.p->m_prepDropTab.waitDropTabCount.clearWaitingFor(nodeId);
  checkPrepDropTabComplete(signal, tabPtr);
}

void
Dbdih::execWAIT_DROP_TAB_CONF(Signal* signal){
  jamEntry();
  WaitDropTabConf * conf = (WaitDropTabConf*)signal->getDataPtr();
  
  TabRecordPtr tabPtr;
  tabPtr.i = conf->tableId;
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
  
  ndbrequire(tabPtr.p->tabStatus == TabRecord::TS_DROPPING);
  Uint32 nodeId = refToNode(conf->senderRef);
  tabPtr.p->m_prepDropTab.waitDropTabCount.clearWaitingFor(nodeId);
  checkPrepDropTabComplete(signal, tabPtr);
}

void
Dbdih::checkWaitDropTabFailedLqh(Signal* signal, Uint32 nodeId, Uint32 tableId){
  
  TabRecordPtr tabPtr;
  tabPtr.i = tableId;

  WaitDropTabConf * conf = (WaitDropTabConf*)signal->getDataPtr();
  conf->tableId = tableId;

  const Uint32 RT_BREAK = 16;
  for(Uint32 i = 0; i<RT_BREAK && tabPtr.i < ctabFileSize; i++, tabPtr.i++){
    ptrAss(tabPtr, tabRecord);
    if(tabPtr.p->tabStatus == TabRecord::TS_DROPPING){
      if(tabPtr.p->m_prepDropTab.waitDropTabCount.isWaitingFor(nodeId)){
	conf->senderRef = calcLqhBlockRef(nodeId);
	execWAIT_DROP_TAB_CONF(signal);
	tabPtr.i++;
	break;
      }
    }
  }
  
  if(tabPtr.i == ctabFileSize){
    /**
     * Finished
     */
    jam();
    return;
  }
  
  signal->theData[0] = DihContinueB::CHECK_WAIT_DROP_TAB_FAILED_LQH;
  signal->theData[1] = nodeId;
  signal->theData[2] = tabPtr.i;
  sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBB);
}


void
Dbdih::execNDB_TAMPER(Signal* signal)
{
  if ((ERROR_INSERTED(7011)) &&
      (signal->theData[0] == 7012)) {
    CLEAR_ERROR_INSERT_VALUE;
    calculateKeepGciLab(signal, 0, 0);
    return;
  }//if
  SET_ERROR_INSERT_VALUE(signal->theData[0]);
  return;
}//Dbdih::execNDB_TAMPER()

void Dbdih::execSET_VAR_REQ(Signal* signal) {
#if 0
  SetVarReq* const setVarReq = (SetVarReq*)&signal->theData[0];
  ConfigParamId var = setVarReq->variable();
  int val = setVarReq->value();


  switch (var) {
  case TimeBetweenLocalCheckpoints:
    c_lcpState.clcpDelay = val;
    sendSignal(CMVMI_REF, GSN_SET_VAR_CONF, signal, 1, JBB);
    break;

  case TimeBetweenGlobalCheckpoints:
    cgcpDelay = val;
    sendSignal(CMVMI_REF, GSN_SET_VAR_CONF, signal, 1, JBB);
    break;

  default:
    sendSignal(CMVMI_REF, GSN_SET_VAR_REF, signal, 1, JBB);
  } // switch
#endif
}

void Dbdih::execBLOCK_COMMIT_ORD(Signal* signal){
  BlockCommitOrd* const block = (BlockCommitOrd *)&signal->theData[0];

  jamEntry();
#if 0
  ndbrequire(c_blockCommit == false || 
	     c_blockCommitNo == block->failNo);
#else
  if(!(c_blockCommit == false || c_blockCommitNo == block->failNo)){
    infoEvent("Possible bug in Dbdih::execBLOCK_COMMIT_ORD c_blockCommit = %d c_blockCommitNo = %d"
	      " sig->failNo = %d", c_blockCommit, c_blockCommitNo, block->failNo);
  }
#endif
  c_blockCommit = true;
  c_blockCommitNo = block->failNo;
}

void Dbdih::execUNBLOCK_COMMIT_ORD(Signal* signal){
  UnblockCommitOrd* const unblock = (UnblockCommitOrd *)&signal->theData[0];
  (void)unblock;

  jamEntry();
  
  if(c_blockCommit == true){
    jam();
    //    ndbrequire(c_blockCommitNo == unblock->failNo);
    
    c_blockCommit = false;
    emptyverificbuffer(signal, true);
  }
}

void Dbdih::execSTOP_PERM_REQ(Signal* signal){

  jamEntry();
  
  StopPermReq* const req = (StopPermReq*)&signal->theData[0];
  StopPermRef* const ref = (StopPermRef*)&signal->theData[0];
  
  const Uint32 senderData = req->senderData;
  const BlockReference senderRef = req->senderRef;
  const NodeId nodeId = refToNode(senderRef);
  
  if (isMaster()) {
    /**
     * Master
     */
    jam();
    CRASH_INSERTION(7065);
    if (c_stopPermMaster.clientRef != 0) {
      jam();

      ref->senderData = senderData;
      ref->errorCode  = StopPermRef::NodeShutdownInProgress;
      sendSignal(senderRef, GSN_STOP_PERM_REF, signal,
                 StopPermRef::SignalLength, JBB);
      return;
    }//if
    
    if (c_nodeStartMaster.activeState) {
      jam();
      ref->senderData = senderData;
      ref->errorCode  = StopPermRef::NodeStartInProgress;
      sendSignal(senderRef, GSN_STOP_PERM_REF, signal,
                 StopPermRef::SignalLength, JBB);
      return;
    }//if
    
    /**
     * Lock
     */
    c_nodeStartMaster.activeState = true;
    c_stopPermMaster.clientRef = senderRef;

    c_stopPermMaster.clientData = senderData;
    c_stopPermMaster.returnValue = 0;
    c_switchReplicas.clear();

    Mutex mutex(signal, c_mutexMgr, c_switchPrimaryMutexHandle);
    Callback c = { safe_cast(&Dbdih::switch_primary_stop_node), nodeId };
    ndbrequire(mutex.lock(c));
  } else { 
    /** 
     * Proxy part
     */
    jam();
    CRASH_INSERTION(7066);
    if(c_stopPermProxy.clientRef != 0){
      jam();
      ref->senderData = senderData;
      ref->errorCode = StopPermRef::NodeShutdownInProgress;
      sendSignal(senderRef, GSN_STOP_PERM_REF, signal, 2, JBB);
      return;
    }//if
    
    c_stopPermProxy.clientRef = senderRef;
    c_stopPermProxy.masterRef = cmasterdihref;
    c_stopPermProxy.clientData = senderData;
    
    req->senderRef = reference();
    req->senderData = senderData;
    sendSignal(cmasterdihref, GSN_STOP_PERM_REQ, signal,
	       StopPermReq::SignalLength, JBB);
  }//if
}//Dbdih::execSTOP_PERM_REQ()

void
Dbdih::switch_primary_stop_node(Signal* signal, Uint32 node_id, Uint32 ret_val) 
{
  ndbrequire(ret_val == 0);
  signal->theData[0] = DihContinueB::SwitchReplica;
  signal->theData[1] = node_id;
  signal->theData[2] = 0; // table id
  signal->theData[3] = 0; // fragment id
  sendSignal(reference(), GSN_CONTINUEB, signal, 4, JBB);
}

void Dbdih::execSTOP_PERM_REF(Signal* signal)
{
  jamEntry();
  ndbrequire(c_stopPermProxy.clientRef != 0);
  ndbrequire(c_stopPermProxy.masterRef == signal->senderBlockRef());
  sendSignal(c_stopPermProxy.clientRef, GSN_STOP_PERM_REF, signal, 2, JBB);  
  c_stopPermProxy.clientRef = 0;
}//Dbdih::execSTOP_PERM_REF()

void Dbdih::execSTOP_PERM_CONF(Signal* signal)
{
  jamEntry();
  ndbrequire(c_stopPermProxy.clientRef != 0);
  ndbrequire(c_stopPermProxy.masterRef == signal->senderBlockRef());
  sendSignal(c_stopPermProxy.clientRef, GSN_STOP_PERM_CONF, signal, 1, JBB);
  c_stopPermProxy.clientRef = 0;
}//Dbdih::execSTOP_PERM_CONF()

void Dbdih::execDIH_SWITCH_REPLICA_REQ(Signal* signal)
{
  jamEntry();
  DihSwitchReplicaReq* const req = (DihSwitchReplicaReq*)&signal->theData[0];
  const Uint32 tableId = req->tableId;
  const Uint32 fragNo = req->fragNo;
  const BlockReference senderRef = req->senderRef;

  CRASH_INSERTION(7067);
  TabRecordPtr tabPtr;
  tabPtr.i = tableId;
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord); 

  ndbrequire(tabPtr.p->tabStatus == TabRecord::TS_ACTIVE);
  if (tabPtr.p->tabCopyStatus != TabRecord::CS_IDLE) {
    jam();
    sendSignal(reference(), GSN_DIH_SWITCH_REPLICA_REQ, signal, 
	       DihSwitchReplicaReq::SignalLength, JBB);
    return;
  }//if
  FragmentstorePtr fragPtr;
  getFragstore(tabPtr.p, fragNo, fragPtr);
  
  /**
   * Do funky stuff
   */
  Uint32 oldOrder[MAX_REPLICAS];
  const Uint32 noOfReplicas = extractNodeInfo(fragPtr.p, oldOrder);
  
  if (noOfReplicas < req->noOfReplicas) {
    jam();
    //---------------------------------------------------------------------
    // A crash occurred in the middle of our switch handling.
    //---------------------------------------------------------------------
    DihSwitchReplicaRef* const ref = (DihSwitchReplicaRef*)&signal->theData[0];
    ref->senderNode = cownNodeId;
    ref->errorCode = StopPermRef::NF_CausedAbortOfStopProcedure;
    sendSignal(senderRef, GSN_DIH_SWITCH_REPLICA_REF, signal,
               DihSwitchReplicaRef::SignalLength, JBB);
  }//if
  for (Uint32 i = 0; i < noOfReplicas; i++) {
    jam();
    ndbrequire(i < MAX_REPLICAS);
    fragPtr.p->activeNodes[i] = req->newNodeOrder[i];
  }//for
  /**
   * Reply
   */
  DihSwitchReplicaConf* const conf = (DihSwitchReplicaConf*)&signal->theData[0];
  conf->senderNode = cownNodeId;
  sendSignal(senderRef, GSN_DIH_SWITCH_REPLICA_CONF, signal,
             DihSwitchReplicaConf::SignalLength, JBB);
}//Dbdih::execDIH_SWITCH_REPLICA_REQ()

void Dbdih::execDIH_SWITCH_REPLICA_CONF(Signal* signal)
{
  jamEntry();
  /**
   * Response to master
   */
  CRASH_INSERTION(7068);
  DihSwitchReplicaConf* const conf = (DihSwitchReplicaConf*)&signal->theData[0];
  switchReplicaReply(signal, conf->senderNode);
}//Dbdih::execDIH_SWITCH_REPLICA_CONF()

void Dbdih::execDIH_SWITCH_REPLICA_REF(Signal* signal)
{
  jamEntry();
  DihSwitchReplicaRef* const ref = (DihSwitchReplicaRef*)&signal->theData[0];  
  if(c_stopPermMaster.returnValue == 0){
    jam();
    c_stopPermMaster.returnValue = ref->errorCode;
  }//if
  switchReplicaReply(signal, ref->senderNode);
}//Dbdih::execDIH_SWITCH_REPLICA_REF()

void Dbdih::switchReplicaReply(Signal* signal, 
			       NodeId nodeId){
  jam();
  receiveLoopMacro(DIH_SWITCH_REPLICA_REQ, nodeId);
  //------------------------------------------------------
  // We have received all responses from the nodes. Thus
  // we have completed switching replica roles. Continue
  // with the next fragment.
  //------------------------------------------------------
  if(c_stopPermMaster.returnValue != 0){
    jam();
    c_switchReplicas.tableId = ctabFileSize + 1;
  }//if
  c_switchReplicas.fragNo++;

  signal->theData[0] = DihContinueB::SwitchReplica;
  signal->theData[1] = c_switchReplicas.nodeId;
  signal->theData[2] = c_switchReplicas.tableId;
  signal->theData[3] = c_switchReplicas.fragNo;
  sendSignal(reference(), GSN_CONTINUEB, signal, 4, JBB);
}//Dbdih::switchReplicaReply()

void
Dbdih::switchReplica(Signal* signal, 
		     Uint32 nodeId,
		     Uint32 tableId, 
		     Uint32 fragNo){
  jam();
  DihSwitchReplicaReq* const req = (DihSwitchReplicaReq*)&signal->theData[0];

  const Uint32 RT_BREAK = 64;
  
  for (Uint32 i = 0; i < RT_BREAK; i++) {
    jam();
    if (tableId >= ctabFileSize) {
      jam();
      StopPermConf* const conf = (StopPermConf*)&signal->theData[0];
      StopPermRef*  const ref  = (StopPermRef*)&signal->theData[0];
      /**
       * Finished with all tables
       */
      if(c_stopPermMaster.returnValue == 0) {
	jam();
	conf->senderData = c_stopPermMaster.clientData;
	sendSignal(c_stopPermMaster.clientRef, GSN_STOP_PERM_CONF, 
		   signal, 1, JBB);
      } else {
        jam();
        ref->senderData = c_stopPermMaster.clientData;
        ref->errorCode  = c_stopPermMaster.returnValue;
        sendSignal(c_stopPermMaster.clientRef, GSN_STOP_PERM_REF, signal, 2,JBB);
      }//if
      
      /**
       * UnLock
       */
      c_nodeStartMaster.activeState = false;
      c_stopPermMaster.clientRef = 0;
      c_stopPermMaster.clientData = 0;
      c_stopPermMaster.returnValue = 0;
      Mutex mutex(signal, c_mutexMgr, c_switchPrimaryMutexHandle);
      mutex.unlock(); // ignore result
      return;
    }//if
    
    TabRecordPtr tabPtr;
    tabPtr.i = tableId;
    ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);  
    
    if (tabPtr.p->tabStatus != TabRecord::TS_ACTIVE) {
      jam();
      tableId++;
      fragNo = 0;
      continue;
    }//if    
    if (fragNo >= tabPtr.p->totalfragments) {
      jam();
      tableId++;
      fragNo = 0;
      continue;
    }//if    
    FragmentstorePtr fragPtr;
    getFragstore(tabPtr.p, fragNo, fragPtr);
    
    Uint32 oldOrder[MAX_REPLICAS];
    const Uint32 noOfReplicas = extractNodeInfo(fragPtr.p, oldOrder);

    if(oldOrder[0] != nodeId) {
      jam();
      fragNo++;
      continue;
    }//if    
    req->tableId = tableId;
    req->fragNo = fragNo;
    req->noOfReplicas = noOfReplicas;
    for (Uint32 i = 0; i < (noOfReplicas - 1); i++) {
      req->newNodeOrder[i] = oldOrder[i+1];
    }//for
    req->newNodeOrder[noOfReplicas-1] = nodeId;
    req->senderRef = reference();
    
    /**
     * Initialize struct
     */
    c_switchReplicas.tableId = tableId;
    c_switchReplicas.fragNo = fragNo;
    c_switchReplicas.nodeId = nodeId;

    sendLoopMacro(DIH_SWITCH_REPLICA_REQ, sendDIH_SWITCH_REPLICA_REQ);
    return;
  }//for

  signal->theData[0] = DihContinueB::SwitchReplica;
  signal->theData[1] = nodeId;
  signal->theData[2] = tableId;
  signal->theData[3] = fragNo;
  sendSignal(reference(), GSN_CONTINUEB, signal, 4, JBB);
}//Dbdih::switchReplica()

void Dbdih::execSTOP_ME_REQ(Signal* signal)
{
  jamEntry();
  StopMeReq* const req = (StopMeReq*)&signal->theData[0];
  const BlockReference senderRef = req->senderRef; 
  const Uint32 senderData = req->senderData;
  const Uint32 nodeId = refToNode(senderRef);
  { 
    /**
     * Set node dead (remove from operations)
     */
    NodeRecordPtr nodePtr;
    nodePtr.i = nodeId;
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
    nodePtr.p->useInTransactions = false;
  }
  if (nodeId != getOwnNodeId()) {
    jam();
    StopMeConf * const stopMeConf = (StopMeConf *)&signal->theData[0];
    stopMeConf->senderData = senderData;
    stopMeConf->senderRef  = reference();
    sendSignal(senderRef, GSN_STOP_ME_CONF, signal, 
	       StopMeConf::SignalLength, JBB);
    return;
  }//if
  
  /**
   * Local signal
   */
  jam();
  ndbrequire(c_stopMe.clientRef == 0);
  
  c_stopMe.clientData  = senderData;
  c_stopMe.clientRef   = senderRef;
  
  req->senderData = senderData;
  req->senderRef  = reference();

  sendLoopMacro(STOP_ME_REQ, sendSTOP_ME_REQ);

  /**
   * Send conf to self
   */
  StopMeConf * const stopMeConf = (StopMeConf *)&signal->theData[0];
  stopMeConf->senderData = senderData;
  stopMeConf->senderRef  = reference();
  sendSignal(reference(), GSN_STOP_ME_CONF, signal, 
	     StopMeConf::SignalLength, JBB);
}//Dbdih::execSTOP_ME_REQ()

void Dbdih::execSTOP_ME_REF(Signal* signal)
{
  ndbrequire(false);
}

void Dbdih::execSTOP_ME_CONF(Signal* signal)
{
  jamEntry();
  StopMeConf * const stopMeConf = (StopMeConf *)&signal->theData[0];
  
  const Uint32 senderRef  = stopMeConf->senderRef;
  const Uint32 senderData = stopMeConf->senderData;
  const Uint32 nodeId     = refToNode(senderRef);

  ndbrequire(c_stopMe.clientRef != 0);
  ndbrequire(c_stopMe.clientData == senderData);

  receiveLoopMacro(STOP_ME_REQ, nodeId);
  //---------------------------------------------------------
  // All STOP_ME_REQ have been received. We will send the
  // confirmation back to the requesting block.
  //---------------------------------------------------------

  stopMeConf->senderRef = reference();
  stopMeConf->senderData = c_stopMe.clientData;
  sendSignal(c_stopMe.clientRef, GSN_STOP_ME_CONF, signal,
	     StopMeConf::SignalLength, JBB);
  c_stopMe.clientRef = 0;
}//Dbdih::execSTOP_ME_CONF()

void Dbdih::execWAIT_GCP_REQ(Signal* signal)
{
  jamEntry();
  WaitGCPReq* const req = (WaitGCPReq*)&signal->theData[0];
  WaitGCPRef* const ref = (WaitGCPRef*)&signal->theData[0];
  WaitGCPConf* const conf = (WaitGCPConf*)&signal->theData[0];
  const Uint32 senderData = req->senderData;
  const BlockReference senderRef = req->senderRef;
  const Uint32 requestType = req->requestType;

  if(requestType == WaitGCPReq::CurrentGCI) {
    jam();
    conf->senderData = senderData;
    conf->gcp = cnewgcp;
    sendSignal(senderRef, GSN_WAIT_GCP_CONF, signal, 
	       WaitGCPConf::SignalLength, JBB);
    return;
  }//if

  if(isMaster()) {
    /**
     * Master
     */
    jam();

    if((requestType == WaitGCPReq::CompleteIfRunning) &&
       (cgcpStatus == GCP_READY)) {
      jam();
      conf->senderData = senderData;
      conf->gcp = coldgcp;
      sendSignal(senderRef, GSN_WAIT_GCP_CONF, signal, 
		 WaitGCPConf::SignalLength, JBB);
      return;
    }//if

    WaitGCPMasterPtr ptr;
    if(c_waitGCPMasterList.seize(ptr) == false){
      jam();
      ref->senderData = senderData;
      ref->errorCode = WaitGCPRef::NoWaitGCPRecords;
      sendSignal(senderRef, GSN_WAIT_GCP_REF, signal, 
		 WaitGCPRef::SignalLength, JBB);
      return;
    }//if
    ptr.p->clientRef = senderRef;
    ptr.p->clientData = senderData;
    
    if((requestType == WaitGCPReq::CompleteForceStart) && 
       (cgcpStatus == GCP_READY)) {
      jam();
      cstartGcpNow = true;
    }//if
    return;
  } else { 
    /** 
     * Proxy part
     */
    jam();
    WaitGCPProxyPtr ptr;
    if (c_waitGCPProxyList.seize(ptr) == false) {
      jam();
      ref->senderData = senderData;
      ref->errorCode = WaitGCPRef::NoWaitGCPRecords;
      sendSignal(senderRef, GSN_WAIT_GCP_REF, signal, 
		 WaitGCPRef::SignalLength, JBB);
      return;
    }//if
    ptr.p->clientRef = senderRef;
    ptr.p->clientData = senderData;
    ptr.p->masterRef = cmasterdihref;

    req->senderData = ptr.i;
    req->senderRef = reference();
    req->requestType = requestType;

    sendSignal(cmasterdihref, GSN_WAIT_GCP_REQ, signal,
	       WaitGCPReq::SignalLength, JBB);
    return;
  }//if
}//Dbdih::execWAIT_GCP_REQ()

void Dbdih::execWAIT_GCP_REF(Signal* signal)
{
  jamEntry();
  ndbrequire(!isMaster());
  WaitGCPRef* const ref = (WaitGCPRef*)&signal->theData[0];  
  
  const Uint32 proxyPtr = ref->senderData;
  const Uint32 errorCode = ref->errorCode;
  
  WaitGCPProxyPtr ptr;
  ptr.i = proxyPtr;
  c_waitGCPProxyList.getPtr(ptr);

  ref->senderData = ptr.p->clientData;
  ref->errorCode = errorCode;
  sendSignal(ptr.p->clientRef, GSN_WAIT_GCP_REF, signal,
	     WaitGCPRef::SignalLength, JBB);

  c_waitGCPProxyList.release(ptr);
}//Dbdih::execWAIT_GCP_REF()

void Dbdih::execWAIT_GCP_CONF(Signal* signal)
{
  jamEntry();
  ndbrequire(!isMaster());  
  WaitGCPConf* const conf = (WaitGCPConf*)&signal->theData[0];
  const Uint32 proxyPtr = conf->senderData;
  const Uint32 gcp = conf->gcp;
  WaitGCPProxyPtr ptr;

  ptr.i = proxyPtr;
  c_waitGCPProxyList.getPtr(ptr);

  conf->senderData = ptr.p->clientData;
  conf->gcp = gcp;
  sendSignal(ptr.p->clientRef, GSN_WAIT_GCP_CONF, signal,
	     WaitGCPConf::SignalLength, JBB);
  
  c_waitGCPProxyList.release(ptr);
}//Dbdih::execWAIT_GCP_CONF()

void Dbdih::checkWaitGCPProxy(Signal* signal, NodeId failedNodeId)
{
  jam();
  WaitGCPRef* const ref = (WaitGCPRef*)&signal->theData[0];  
  ref->errorCode = WaitGCPRef::NF_CausedAbortOfProcedure;

  WaitGCPProxyPtr ptr;
  c_waitGCPProxyList.first(ptr);
  while(ptr.i != RNIL) {
    jam();    
    const Uint32 i = ptr.i;
    const Uint32 clientData = ptr.p->clientData;
    const BlockReference clientRef = ptr.p->clientRef;
    const BlockReference masterRef = ptr.p->masterRef;
    
    c_waitGCPProxyList.next(ptr);    
    if(refToNode(masterRef) == failedNodeId) {
      jam();      
      c_waitGCPProxyList.release(i);
      ref->senderData = clientData;
      sendSignal(clientRef, GSN_WAIT_GCP_REF, signal, 
		 WaitGCPRef::SignalLength, JBB);
    }//if
  }//while
}//Dbdih::checkWaitGCPProxy()

void Dbdih::checkWaitGCPMaster(Signal* signal, NodeId failedNodeId)
{
  jam();  
  WaitGCPMasterPtr ptr;
  c_waitGCPMasterList.first(ptr);

  while (ptr.i != RNIL) {
    jam();
    const Uint32 i = ptr.i;
    const NodeId nodeId = refToNode(ptr.p->clientRef);
    
    c_waitGCPMasterList.next(ptr);
    if (nodeId == failedNodeId) {
      jam()     
	c_waitGCPMasterList.release(i);
    }//if
  }//while
}//Dbdih::checkWaitGCPMaster()

void Dbdih::emptyWaitGCPMasterQueue(Signal* signal)
{
  jam();
  WaitGCPConf* const conf = (WaitGCPConf*)&signal->theData[0];
  conf->gcp = coldgcp;
  
  WaitGCPMasterPtr ptr;
  c_waitGCPMasterList.first(ptr);  
  while(ptr.i != RNIL) {
    jam();
    const Uint32 i = ptr.i;
    const Uint32 clientData = ptr.p->clientData;
    const BlockReference clientRef = ptr.p->clientRef;

    c_waitGCPMasterList.next(ptr);    
    conf->senderData = clientData;
    sendSignal(clientRef, GSN_WAIT_GCP_CONF, signal,
	       WaitGCPConf::SignalLength, JBB);
    
    c_waitGCPMasterList.release(i);
  }//while
}//Dbdih::emptyWaitGCPMasterQueue()

void Dbdih::setNodeStatus(Uint32 nodeId, NodeRecord::NodeStatus newStatus)
{
  NodeRecordPtr nodePtr;
  nodePtr.i = nodeId;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
  nodePtr.p->nodeStatus = newStatus;
}//Dbdih::setNodeStatus()

Dbdih::NodeRecord::NodeStatus Dbdih::getNodeStatus(Uint32 nodeId)
{
  NodeRecordPtr nodePtr;
  nodePtr.i = nodeId;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
  return nodePtr.p->nodeStatus;
}//Dbdih::getNodeStatus()

Sysfile::ActiveStatus 
Dbdih::getNodeActiveStatus(Uint32 nodeId)
{
  NodeRecordPtr nodePtr;
  nodePtr.i = nodeId;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
  return nodePtr.p->activeStatus;
}//Dbdih::getNodeActiveStatus()


void
Dbdih::setNodeActiveStatus(Uint32 nodeId, Sysfile::ActiveStatus newStatus)
{
  NodeRecordPtr nodePtr;
  nodePtr.i = nodeId;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
  nodePtr.p->activeStatus = newStatus;
}//Dbdih::setNodeActiveStatus()

void Dbdih::setAllowNodeStart(Uint32 nodeId, bool newState)
{
  NodeRecordPtr nodePtr;
  nodePtr.i = nodeId;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
  nodePtr.p->allowNodeStart = newState;
}//Dbdih::setAllowNodeStart()

void Dbdih::setNodeCopyCompleted(Uint32 nodeId, bool newState)
{
  NodeRecordPtr nodePtr;
  nodePtr.i = nodeId;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
  nodePtr.p->copyCompleted = newState;
}//Dbdih::setNodeCopyCompleted()

bool Dbdih::getAllowNodeStart(Uint32 nodeId)
{
  NodeRecordPtr nodePtr;
  nodePtr.i = nodeId;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
  return nodePtr.p->allowNodeStart;
}//Dbdih::getAllowNodeStart()

bool Dbdih::getNodeCopyCompleted(Uint32 nodeId)
{
  NodeRecordPtr nodePtr;
  nodePtr.i = nodeId;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
  return nodePtr.p->copyCompleted;
}//Dbdih::getNodeCopyCompleted()

bool Dbdih::checkNodeAlive(Uint32 nodeId)
{
  NodeRecordPtr nodePtr;
  nodePtr.i = nodeId;
  ndbrequire(nodeId > 0);
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
  if (nodePtr.p->nodeStatus != NodeRecord::ALIVE) {
    return false;
  } else {
    return true;
  }//if
}//Dbdih::checkNodeAlive()

bool Dbdih::isMaster()
{
  return (reference() == cmasterdihref);
}//Dbdih::isMaster()

bool Dbdih::isActiveMaster()
{
  return ((reference() == cmasterdihref) && (cmasterState == MASTER_ACTIVE));
}//Dbdih::isActiveMaster()

Dbdih::NodeRecord::NodeRecord(){
  m_nodefailSteps.clear();
  gcpstate = NodeRecord::READY;

  activeStatus = Sysfile::NS_NotDefined;
  recNODE_FAILREP = ZFALSE;
  nodeGroup = ZNIL;
  dbtcFailCompleted = ZTRUE;
  dbdictFailCompleted = ZTRUE;
  dbdihFailCompleted = ZTRUE;
  dblqhFailCompleted = ZTRUE;
  noOfStartedChkpt = 0;
  noOfQueuedChkpt = 0;
  lcpStateAtTakeOver = (MasterLCPConf::State)255;

  activeTabptr = RNIL;
  nodeStatus = NodeRecord::NOT_IN_CLUSTER;
  useInTransactions = false;
  copyCompleted = false;
  allowNodeStart = true;
}
