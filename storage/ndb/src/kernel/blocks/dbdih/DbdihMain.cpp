/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#define DBDIH_C
#include <ndb_global.h>
#include <ndb_limits.h>
#include <ndb_version.h>
#include <NdbOut.hpp>

#include "Dbdih.hpp"
#include "Configuration.hpp"

#include <signaldata/CopyTab.hpp>
#include <signaldata/DbinfoScan.hpp>
#include <signaldata/AllocNodeId.hpp>
#include <signaldata/NodeRecoveryStatusRep.hpp>
#include <signaldata/BlockCommitOrd.hpp>
#include <signaldata/CheckNodeGroups.hpp>
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
#include <signaldata/EventReport.hpp>
#include <signaldata/GCP.hpp>
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
#include <signaldata/StopPerm.hpp>
#include <signaldata/StopMe.hpp>
#include <signaldata/TestOrd.hpp>
#include <signaldata/WaitGCP.hpp>
#include <signaldata/DihStartTab.hpp>
#include <signaldata/LCP.hpp>
#include <signaldata/SystemError.hpp>

#include <signaldata/TakeOver.hpp>

#include <signaldata/DropTab.hpp>
#include <signaldata/AlterTab.hpp>
#include <signaldata/AlterTable.hpp>
#include <signaldata/PrepDropTab.hpp>
#include <signaldata/SumaImpl.hpp>
#include <signaldata/DictTabInfo.hpp>
#include <signaldata/CreateFragmentation.hpp>
#include <signaldata/LqhFrag.hpp>
#include <signaldata/FsOpenReq.hpp>
#include <signaldata/DihScanTab.hpp>
#include <signaldata/DictLock.hpp>
#include <DebuggerNames.hpp>
#include <signaldata/Upgrade.hpp>
#include <NdbEnv.h>
#include <signaldata/CreateNodegroup.hpp>
#include <signaldata/CreateNodegroupImpl.hpp>
#include <signaldata/DropNodegroup.hpp>
#include <signaldata/DropNodegroupImpl.hpp>
#include <signaldata/DihGetTabInfo.hpp>
#include <SectionReader.hpp>
#include <signaldata/DihRestart.hpp>
#include <signaldata/IsolateOrd.hpp>
#include <ndb_constants.h>

#include <EventLogger.hpp>

#define JAM_FILE_ID 354

static const Uint32 WaitTableStateChangeMillis = 10;

extern EventLogger * g_eventLogger;

#define SYSFILE ((Sysfile *)&sysfileData[0])
#define ZINIT_CREATE_GCI Uint32(0)
#define ZINIT_REPLICA_LAST_GCI Uint32(-1)

#define RETURN_IF_NODE_NOT_ALIVE(node) \
  if (!checkNodeAlive((node))) { \
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

#define sendLoopMacro(sigName, signalRoutine, extra)                    \
{                                                                       \
  c_##sigName##_Counter.clearWaitingFor();                              \
  NodeRecordPtr specNodePtr;                                            \
  specNodePtr.i = cfirstAliveNode;                                      \
  do {                                                                  \
    jam();                                                              \
    ptrCheckGuard(specNodePtr, MAX_NDB_NODES, nodeRecord);              \
    c_##sigName##_Counter.setWaitingFor(specNodePtr.i);                 \
    signalRoutine(signal, specNodePtr.i, extra);                        \
    specNodePtr.i = specNodePtr.p->nextNode;                            \
  } while (specNodePtr.i != RNIL);                                      \
}

static
Uint32
prevLcpNo(Uint32 lcpNo){
  if(lcpNo == 0)
    return MAX_LCP_USED - 1;
  return lcpNo - 1;
}

static
Uint32
nextLcpNo(Uint32 lcpNo){
  lcpNo++;
  if(lcpNo >= MAX_LCP_USED)
    return 0;
  return lcpNo;
}

void Dbdih::nullRoutine(Signal* signal, Uint32 nodeId, Uint32 extra)
{
}//Dbdih::nullRoutine()

void Dbdih::sendCOPY_GCIREQ(Signal* signal, Uint32 nodeId, Uint32 extra) 
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


void Dbdih::sendDIH_SWITCH_REPLICA_REQ(Signal* signal, Uint32 nodeId, 
                                       Uint32 extra)
{
  const BlockReference ref    = calcDihBlockRef(nodeId);
  sendSignal(ref, GSN_DIH_SWITCH_REPLICA_REQ, signal, 
             DihSwitchReplicaReq::SignalLength, JBB);
}//Dbdih::sendDIH_SWITCH_REPLICA_REQ()

void Dbdih::sendEMPTY_LCP_REQ(Signal* signal, Uint32 nodeId, Uint32 extra)
{
  BlockReference ref = calcLqhBlockRef(nodeId);
  sendSignal(ref, GSN_EMPTY_LCP_REQ, signal, EmptyLcpReq::SignalLength, JBB);
}//Dbdih::sendEMPTY_LCPREQ()

void Dbdih::sendGCP_COMMIT(Signal* signal, Uint32 nodeId, Uint32 extra)
{
  BlockReference ref = calcDihBlockRef(nodeId);
  GCPCommit *req = (GCPCommit*)signal->getDataPtrSend();
  req->nodeId = cownNodeId;
  req->gci_hi = Uint32(m_micro_gcp.m_master.m_new_gci >> 32);
  req->gci_lo = Uint32(m_micro_gcp.m_master.m_new_gci);
  sendSignal(ref, GSN_GCP_COMMIT, signal, GCPCommit::SignalLength, JBA);

  ndbassert(m_micro_gcp.m_enabled || Uint32(m_micro_gcp.m_new_gci) == 0);
}//Dbdih::sendGCP_COMMIT()

void Dbdih::sendGCP_PREPARE(Signal* signal, Uint32 nodeId, Uint32 extra)
{
  BlockReference ref = calcDihBlockRef(nodeId);
  GCPPrepare *req = (GCPPrepare*)signal->getDataPtrSend();
  req->nodeId = cownNodeId;
  req->gci_hi = Uint32(m_micro_gcp.m_master.m_new_gci >> 32);
  req->gci_lo = Uint32(m_micro_gcp.m_master.m_new_gci);

  if (! (ERROR_INSERTED(7201) || ERROR_INSERTED(7202)))
  {
    sendSignal(ref, GSN_GCP_PREPARE, signal, GCPPrepare::SignalLength, JBA);
  }
  else if (ERROR_INSERTED(7201))
  {
    sendSignal(ref, GSN_GCP_PREPARE, signal, GCPPrepare::SignalLength, JBB);
  } 
  else if (ERROR_INSERTED(7202))
  {
    ndbrequire(nodeId == getOwnNodeId());
    sendSignalWithDelay(ref, GSN_GCP_PREPARE, signal, 2000, 
                        GCPPrepare::SignalLength);    
  }
  else
  {
    ndbrequire(false); // should be dead code #ifndef ERROR_INSERT
  }

  ndbassert(m_micro_gcp.m_enabled || Uint32(m_micro_gcp.m_new_gci) == 0);
}//Dbdih::sendGCP_PREPARE()

void
Dbdih::sendSUB_GCP_COMPLETE_REP(Signal* signal, Uint32 nodeId, Uint32 extra)
{
  ndbassert(m_micro_gcp.m_enabled || Uint32(m_micro_gcp.m_new_gci) == 0);
  if (!ndbd_dih_sub_gcp_complete_ack(getNodeInfo(nodeId).m_version))
  {
    jam();
    c_SUB_GCP_COMPLETE_REP_Counter.clearWaitingFor(nodeId);
  }
  BlockReference ref = calcDihBlockRef(nodeId);
  sendSignal(ref, GSN_SUB_GCP_COMPLETE_REP, signal,
             SubGcpCompleteRep::SignalLength, JBA);
}

void Dbdih::sendGCP_SAVEREQ(Signal* signal, Uint32 nodeId, Uint32 extra)
{
  GCPSaveReq * const saveReq = (GCPSaveReq*)&signal->theData[0];
  BlockReference ref = calcDihBlockRef(nodeId);
  saveReq->dihBlockRef = reference();
  saveReq->dihPtr = nodeId;
  saveReq->gci = m_gcp_save.m_master.m_new_gci;
  sendSignal(ref, GSN_GCP_SAVEREQ, signal, GCPSaveReq::SignalLength, JBB);
}//Dbdih::sendGCP_SAVEREQ()

void Dbdih::sendINCL_NODEREQ(Signal* signal, Uint32 nodeId, Uint32 extra)
{
  BlockReference nodeDihRef = calcDihBlockRef(nodeId);
  signal->theData[0] = reference();
  signal->theData[1] = c_nodeStartMaster.startNode;
  signal->theData[2] = c_nodeStartMaster.failNr;
  signal->theData[3] = 0;
  signal->theData[4] = (Uint32)(m_micro_gcp.m_current_gci >> 32);
  signal->theData[5] = (Uint32)(m_micro_gcp.m_current_gci & 0xFFFFFFFF);
  sendSignal(nodeDihRef, GSN_INCL_NODEREQ, signal, 6, JBA);
}//Dbdih::sendINCL_NODEREQ()

void Dbdih::sendMASTER_GCPREQ(Signal* signal, Uint32 nodeId, Uint32 extra)
{
  BlockReference ref = calcDihBlockRef(nodeId);
  sendSignal(ref, GSN_MASTER_GCPREQ, signal, MasterGCPReq::SignalLength, JBB);
}//Dbdih::sendMASTER_GCPREQ()

void Dbdih::sendMASTER_LCPREQ(Signal* signal, Uint32 nodeId, Uint32 extra)
{
  BlockReference ref = calcDihBlockRef(nodeId);
  sendSignal(ref, GSN_MASTER_LCPREQ, signal, MasterLCPReq::SignalLength, JBB);
}//Dbdih::sendMASTER_LCPREQ()

void Dbdih::sendSTART_INFOREQ(Signal* signal, Uint32 nodeId, Uint32 extra)
{
  const BlockReference ref = calcDihBlockRef(nodeId);
  sendSignal(ref, GSN_START_INFOREQ, signal, StartInfoReq::SignalLength, JBB);
}//sendSTART_INFOREQ()

void Dbdih::sendSTART_RECREQ(Signal* signal, Uint32 nodeId, Uint32 extra)
{
  if (!m_sr_nodes.get(nodeId))
  {
    jam();
    c_START_RECREQ_Counter.clearWaitingFor(nodeId);
    return;
  }

  Uint32 keepGCI = SYSFILE->keepGCI;
  Uint32 lastCompletedGCI = SYSFILE->lastCompletedGCI[nodeId];
  if (keepGCI > lastCompletedGCI)
  {
    jam();
    keepGCI = lastCompletedGCI;
  }

  StartRecReq * const req = (StartRecReq*)&signal->theData[0];
  BlockReference ref = calcLqhBlockRef(nodeId);
  req->receivingNodeId = nodeId;
  req->senderRef = reference();
  req->keepGci = keepGCI;
  req->lastCompletedGci = lastCompletedGCI;
  req->newestGci = SYSFILE->newestRestorableGCI;
  req->senderData = extra;
  m_sr_nodes.copyto(NdbNodeBitmask::Size, req->sr_nodes);
  sendSignal(ref, GSN_START_RECREQ, signal, StartRecReq::SignalLength, JBB);

  signal->theData[0] = NDB_LE_StartREDOLog;
  signal->theData[1] = nodeId;
  signal->theData[2] = keepGCI;
  signal->theData[3] = lastCompletedGCI;
  signal->theData[4] = SYSFILE->newestRestorableGCI;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 5, JBB);
}//Dbdih::sendSTART_RECREQ()

void Dbdih::sendSTOP_ME_REQ(Signal* signal, Uint32 nodeId, Uint32 extra)
{
  if (nodeId != getOwnNodeId()) {
    jam();
    const BlockReference ref = calcDihBlockRef(nodeId);
    sendSignal(ref, GSN_STOP_ME_REQ, signal, StopMeReq::SignalLength, JBB);
  }//if
}//Dbdih::sendSTOP_ME_REQ()

void Dbdih::sendTC_CLOPSIZEREQ(Signal* signal, Uint32 nodeId, Uint32 extra)
{
  BlockReference ref = calcTcBlockRef(nodeId);
  signal->theData[0] = nodeId;
  signal->theData[1] = reference();
  sendSignal(ref, GSN_TC_CLOPSIZEREQ, signal, 2, JBB);
}//Dbdih::sendTC_CLOPSIZEREQ()

void Dbdih::sendTCGETOPSIZEREQ(Signal* signal, Uint32 nodeId, Uint32 extra)
{
  BlockReference ref = calcTcBlockRef(nodeId);
  signal->theData[0] = nodeId;
  signal->theData[1] = reference();
  sendSignal(ref, GSN_TCGETOPSIZEREQ, signal, 2, JBB);
}//Dbdih::sendTCGETOPSIZEREQ()

void Dbdih::sendUPDATE_TOREQ(Signal* signal, Uint32 nodeId, Uint32 extra)
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
      wf.totalfragments = signal->theData[5];
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
    startGcpLab(signal);
#endif
    return;
    break;
  case DihContinueB::ZCOPY_GCI:{
    jam();
    CopyGCIReq::CopyReason reason = (CopyGCIReq::CopyReason)signal->theData[1];
    ndbrequire(c_copyGCIMaster.m_copyReason == reason);

    // set to idle, to be able to reuse method
    c_copyGCIMaster.m_copyReason = CopyGCIReq::IDLE;
    copyGciLab(signal, reason);
    return;
  }
    break;
  case DihContinueB::ZEMPTY_VERIFY_QUEUE:
    jam();
    emptyverificbuffer(signal, signal->theData[1], true);
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
    nodeRestartPh2Lab2(signal);
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
  case DihContinueB::ZSEND_ADD_FRAG:
    {
      jam();
      Uint32 takeOverPtrI = signal->theData[1];
      toCopyFragLab(signal, takeOverPtrI);
      return;
    }
  case DihContinueB::ZSEND_START_TO:
    {
      jam();
      Ptr<TakeOverRecord> takeOverPtr;
      c_takeOverPool.getPtr(takeOverPtr, signal->theData[1]);
      sendStartTo(signal, takeOverPtr);
      return;
    }
  case DihContinueB::ZSEND_UPDATE_TO:
    {
      jam();
      Ptr<TakeOverRecord> takeOverPtr;
      c_takeOverPool.getPtr(takeOverPtr, signal->theData[1]);
      sendUpdateTo(signal, takeOverPtr);
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
  case DihContinueB::ZTO_START_FRAGMENTS:
  {
    TakeOverRecordPtr takeOverPtr;
    c_takeOverPool.getPtr(takeOverPtr, signal->theData[1]);
    nr_start_fragments(signal, takeOverPtr);
    return;
  }
  case DihContinueB::ZWAIT_OLD_SCAN:
  {
    jam();
    wait_old_scan(signal);
    return;
  }
  case DihContinueB::ZLCP_TRY_LOCK:
  {
    jam();
    Mutex mutex(signal, c_mutexMgr, c_fragmentInfoMutex_lcp);
    Callback c = { safe_cast(&Dbdih::lcpFragmentMutex_locked),
                   signal->theData[1] };
    ndbrequire(mutex.trylock(c, false));
    return;
  }
  case DihContinueB::ZTO_START_LOGGING:
  {
    jam();
    TakeOverRecordPtr takeOverPtr;
    c_takeOverPool.getPtr(takeOverPtr, signal->theData[1]);
    nr_start_logging(signal, takeOverPtr);
    return;
  }
  case DihContinueB::ZGET_TABINFO:
  {
    jam();
    getTabInfo(signal);
    return;
  }
  case DihContinueB::ZGET_TABINFO_SEND:
  {
    jam();
    TabRecordPtr tabPtr;
    tabPtr.i = signal->theData[1];
    ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
    getTabInfo_send(signal, tabPtr);
    return;
  }
  case DihContinueB::ZDEQUEUE_LCP_REP:
  {
    jam();
    dequeue_lcp_rep(signal);
    return;
  }
  }

  ndbrequire(false);
  return;
}//Dbdih::execCONTINUEB()

void Dbdih::execCOPY_GCIREQ(Signal* signal) 
{
  CopyGCIReq * const copyGCI = (CopyGCIReq *)&signal->theData[0];
  jamEntry();
  if (ERROR_INSERTED(7241))
  {
    jam();
    g_eventLogger->info("Delayed COPY_GCIREQ 5s");
    sendSignalWithDelay(reference(), GSN_COPY_GCIREQ,
                        signal, 5000,
                        signal->getLength());
    return;
  }

  CopyGCIReq::CopyReason reason = (CopyGCIReq::CopyReason)copyGCI->copyReason;
  const Uint32 tstart = copyGCI->startWord;
  
  ndbrequire(cmasterdihref == signal->senderBlockRef()) ;
  ndbrequire((reason == CopyGCIReq::GLOBAL_CHECKPOINT &&
              c_copyGCISlave.m_copyReason == CopyGCIReq::GLOBAL_CHECKPOINT) ||
             c_copyGCISlave.m_copyReason == CopyGCIReq::IDLE);
  ndbrequire(c_copyGCISlave.m_expectedNextWord == tstart);
  ndbrequire(reason != CopyGCIReq::IDLE);
  bool isdone = (tstart + CopyGCIReq::DATA_SIZE) >= Sysfile::SYSFILE_SIZE32;

  if (ERROR_INSERTED(7177))
  {
    jam();

    if (signal->getLength() == 3)
    {
      jam();
      goto done;
    }
  }

  arrGuard(tstart + CopyGCIReq::DATA_SIZE, sizeof(sysfileData)/4);
  for(Uint32 i = 0; i<CopyGCIReq::DATA_SIZE; i++)
    cdata[tstart+i] = copyGCI->data[i];
  
  if (ERROR_INSERTED(7177) && isMaster() && isdone)
  {
    sendSignalWithDelay(reference(), GSN_COPY_GCIREQ, signal, 1000, 3);
    return;
  }
  
done:  
  if (isdone)
  {
    jam();
    c_copyGCISlave.m_expectedNextWord = 0;
  } 
  else 
  {
    jam();
    c_copyGCISlave.m_expectedNextWord += CopyGCIReq::DATA_SIZE;
    return;
  }
  
  if (cmasterdihref != reference())
  {
    jam();
    Uint32 tmp= SYSFILE->m_restart_seq;
    memcpy(sysfileData, cdata, sizeof(sysfileData));
    SYSFILE->m_restart_seq = tmp;

    if (c_set_initial_start_flag)
    {
      jam();
      Sysfile::setInitialStartOngoing(SYSFILE->systemRestartBits);
    }
  }

  c_copyGCISlave.m_copyReason = reason;
  c_copyGCISlave.m_senderRef  = signal->senderBlockRef();
  c_copyGCISlave.m_senderData = copyGCI->anyData;

  CRASH_INSERTION2(7020, reason==CopyGCIReq::LOCAL_CHECKPOINT);
  CRASH_INSERTION2(7008, reason==CopyGCIReq::GLOBAL_CHECKPOINT);

  if (m_local_lcp_state.check_cut_log_tail(c_newest_restorable_gci))
  {
    jam();

#ifdef NOT_YET
    LcpCompleteRep* rep = (LcpCompleteRep*)signal->getDataPtrSend();
    rep->nodeId = getOwnNodeId();
    rep->blockNo = 0;
    rep->lcpId = m_local_lcp_state.m_start_lcp_req.lcpId;
    rep->keepGci = m_local_lcp_state.m_keep_gci;
    sendSignal(DBLQH_REF, GSN_LCP_COMPLETE_REP, signal, 
               LcpCompleteRep::SignalLength, JBB);

    warningEvent("CUT LOG TAIL: reason: %u lcp: %u m_keep_gci: %u stop: %u",
                 reason,
                 m_local_lcp_state.m_start_lcp_req.lcpId,
                 m_local_lcp_state.m_keep_gci,
                 m_local_lcp_state.m_stop_gci);
#endif
    m_local_lcp_state.reset();
  }
  
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
    setNodeActiveStatus();
    break;
  }
  case CopyGCIReq::RESTART: {
    ok = true;
    jam();
    Uint32 newest = SYSFILE->newestRestorableGCI;
    m_micro_gcp.m_old_gci = Uint64(newest) << 32;
    crestartGci = newest;
    c_newest_restorable_gci = newest;
    Sysfile::setRestartOngoing(SYSFILE->systemRestartBits);
    m_micro_gcp.m_current_gci = Uint64(newest + 1) << 32;
    setNodeActiveStatus();
    setNodeGroups();
    if ((Sysfile::getLCPOngoing(SYSFILE->systemRestartBits))) {
      jam();
      /* -------------------------------------------------------------------- */
      //  IF THERE WAS A LOCAL CHECKPOINT ONGOING AT THE CRASH MOMENT WE WILL
      //    INVALIDATE THAT LOCAL CHECKPOINT.
      /* -------------------------------------------------------------------- */
      invalidateLcpInfoAfterSr(signal);
    }//if

    if (m_micro_gcp.m_enabled == false && 
        m_micro_gcp.m_master.m_time_between_gcp)
    {
      /**
       * Micro GCP is disabled...but configured...
       */
      jam();
      m_micro_gcp.m_enabled = true;
      UpgradeProtocolOrd * ord = (UpgradeProtocolOrd*)signal->getDataPtrSend();
      ord->type = UpgradeProtocolOrd::UPO_ENABLE_MICRO_GCP;
      EXECUTE_DIRECT(QMGR,GSN_UPGRADE_PROTOCOL_ORD,signal,signal->getLength());
    }
    break;
  }
  case CopyGCIReq::GLOBAL_CHECKPOINT: {
    ok = true;
    jam();

    if (m_gcp_save.m_state == GcpSave::GCP_SAVE_COPY_GCI)
    {
      jam();
      /**
       * This must be master take over...and it already running...
       */
      ndbrequire(c_newest_restorable_gci == SYSFILE->newestRestorableGCI);
      m_gcp_save.m_master_ref = c_copyGCISlave.m_senderRef;
      return;
    }

    if (c_newest_restorable_gci == SYSFILE->newestRestorableGCI)
    {
      jam();

      /**
       * This must be master take over...and it already complete...
       */
      m_gcp_save.m_master_ref = c_copyGCISlave.m_senderRef;
      c_copyGCISlave.m_copyReason = CopyGCIReq::IDLE;
      signal->theData[0] = c_copyGCISlave.m_senderData;
      sendSignal(m_gcp_save.m_master_ref, GSN_COPY_GCICONF, signal, 1, JBB);
      return;
    }

    ndbrequire(m_gcp_save.m_state == GcpSave::GCP_SAVE_CONF);
    m_gcp_save.m_state = GcpSave::GCP_SAVE_COPY_GCI;
    m_gcp_save.m_master_ref = c_copyGCISlave.m_senderRef;
    c_newest_restorable_gci = SYSFILE->newestRestorableGCI;
    setNodeActiveStatus();
    break;
  }//if
  case CopyGCIReq::INITIAL_START_COMPLETED:
    ok = true;
    jam();
    break;
  case CopyGCIReq::RESTART_NR:
    jam();
    setNodeGroups();
    /**
     * We dont really need to make anything durable here...skip it
     */
    c_copyGCISlave.m_copyReason = CopyGCIReq::IDLE;
    signal->theData[0] = c_copyGCISlave.m_senderData;
    sendSignal(c_copyGCISlave.m_senderRef, GSN_COPY_GCICONF, signal, 1, JBB);
    return;
  }
  ndbrequire(ok);
  
  CRASH_INSERTION(7183);
  
  if (ERROR_INSERTED(7185) && reason==CopyGCIReq::GLOBAL_CHECKPOINT)
  {
    jam();
    return;
  }
#ifdef GCP_TIMER_HACK
  if (reason == CopyGCIReq::GLOBAL_CHECKPOINT) {
    jam();
    globalData.gcp_timer_copygci[0] = NdbTick_getCurrentTicks();
  }
#endif

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
    nodeDictStartConfLab(signal, nodeId);
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
    if (ERROR_INSERTED(7235))
    {
      jam();
      filePtr.p->reqStatus = status;
      /* Suspend processing of WRITECONFs */
      sendSignalWithDelay(reference(), GSN_FSWRITECONF, signal, 1000, signal->getLength());
      return;
    }
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
  Uint32 type = signal->theData[2];
  
  Uint32 gci_hi = 0;
  Uint32 gci_lo = 0;
  switch(type){
  case 0:
    jam();
    gci_hi = SYSFILE->newestRestorableGCI;
    break;
  case 1:
    jam();
    gci_hi = Uint32(m_micro_gcp.m_current_gci >> 32);
    gci_lo = Uint32(m_micro_gcp.m_current_gci);
    break;
  }
  
  signal->theData[0] = userPtr;
  signal->theData[1] = gci_hi;
  signal->theData[2] = gci_lo;
  
  if (userRef)
  {
    jam();
    sendSignal(userRef, GSN_GETGCICONF, signal, 3, JBB);
  }
  else
  {
    jam();
    // Execute direct
  }
}//Dbdih::execGETGCIREQ()

void Dbdih::execREAD_CONFIG_REQ(Signal* signal) 
{
  const ReadConfigReq * req = (ReadConfigReq*)signal->getDataPtr();
  Uint32 ref = req->senderRef;
  Uint32 senderData = req->senderData;
  ndbrequire(req->noOfParameters == 0);

  jamEntry();

  const ndb_mgm_configuration_iterator * p = 
    m_ctx.m_config.getOwnConfigIterator();
  ndbrequireErr(p != 0, NDBD_EXIT_INVALID_CONFIG);

  initData();

  cconnectFileSize = 256; // Only used for DDL

  ndbrequireErr(!ndb_mgm_get_int_parameter(p, CFG_DIH_API_CONNECT, 
					   &capiConnectFileSize),
		NDBD_EXIT_INVALID_CONFIG);
  capiConnectFileSize++; // Increase by 1...so that srsw queue never gets full

  ndbrequireErr(!ndb_mgm_get_int_parameter(p, CFG_DIH_FRAG_CONNECT, 
					   &cfragstoreFileSize),
		NDBD_EXIT_INVALID_CONFIG);
  ndbrequireErr(!ndb_mgm_get_int_parameter(p, CFG_DIH_REPLICAS, 
					   &creplicaFileSize),
		NDBD_EXIT_INVALID_CONFIG);
  ndbrequireErr(!ndb_mgm_get_int_parameter(p, CFG_DIH_TABLE, &ctabFileSize),
		NDBD_EXIT_INVALID_CONFIG);

  if (isNdbMtLqh())
  {
    jam();
    c_fragments_per_node_ = 0;
    // try to get some LQH workers which initially handle no fragments
    if (ERROR_INSERTED(7215)) {
      c_fragments_per_node_ = 1;
      ndbout_c("Using %u fragments per node", c_fragments_per_node_);
    }
  }
  ndb_mgm_get_int_parameter(p, CFG_DB_LCP_TRY_LOCK_TIMEOUT, 
                            &c_lcpState.m_lcp_trylock_timeout);

  cfileFileSize = (2 * ctabFileSize) + 2;
  initRecords();
  initialiseRecordsLab(signal, 0, ref, senderData);

  {
    Uint32 val = 0;
    ndb_mgm_get_int_parameter(p, CFG_DB_2PASS_INR,
                              &val);
    c_2pass_inr = val ? true : false;
  }

  /**
   * Set API assigned nodegroup(s)
   */
  {
    NodeRecordPtr nodePtr;
    for (nodePtr.i = 0; nodePtr.i < MAX_NDB_NODES; nodePtr.i++)
    {
      ptrAss(nodePtr, nodeRecord);
      initNodeRecord(nodePtr);
      nodePtr.p->nodeGroup = RNIL;
    }
    initNodeRecoveryStatus();

    ndb_mgm_configuration_iterator * iter =
      m_ctx.m_config.getClusterConfigIterator();
    for(ndb_mgm_first(iter); ndb_mgm_valid(iter); ndb_mgm_next(iter))
    {
      jam();
      Uint32 nodeId;
      Uint32 nodeType;

      ndbrequire(!ndb_mgm_get_int_parameter(iter,CFG_NODE_ID, &nodeId));
      ndbrequire(!ndb_mgm_get_int_parameter(iter,CFG_TYPE_OF_SECTION,
                                            &nodeType));

      if (nodeType == NodeInfo::DB)
      {
        jam();
        Uint32 ng;
        nodePtr.i = nodeId;
        ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
        setNodeRecoveryStatusInitial(nodePtr);
        if (ndb_mgm_get_int_parameter(iter, CFG_DB_NODEGROUP, &ng) == 0)
        {
          jam();
          nodePtr.p->nodeGroup = ng;
        }
        else
        {
          jam();
          nodePtr.p->nodeGroup = RNIL;
        }
      }
    }
  }
  return;
}

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

void Dbdih::execSTART_FRAGREF(Signal* signal) 
{
  jamEntry();
 
  /**
   * Kill starting node
   */
  Uint32 errCode = signal->theData[1];
  Uint32 nodeId = signal->theData[2];
  
  SystemError * const sysErr = (SystemError*)&signal->theData[0];
  sysErr->errorCode = SystemError::StartFragRefError;
  sysErr->errorRef = reference();
  sysErr->data[0] = errCode;
  sysErr->data[1] = 0;
  sendSignal(calcNdbCntrBlockRef(nodeId), GSN_SYSTEM_ERROR, signal, 
	     SystemError::SignalLength, JBB);
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

  commit_new_table(tabPtr);

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
  const DihRestartReq* req = CAST_CONSTPTR(DihRestartReq,
                                           signal->getDataPtr());
  if (req->senderRef != 0)
  {
    jam();
    cntrlblockref = req->senderRef;
    if(m_ctx.m_config.getInitialStart())
    {
      sendDihRestartRef(signal);
    } else {
      readGciFileLab(signal);
    }
  }
  else
  {
    /**
     * Precondition, (not checked)
     *   atleast 1 node in each node group
     */
    Uint32 i;
    NdbNodeBitmask mask;
    mask.assign(NdbNodeBitmask::Size, req->nodemask);
    const Uint32 *node_gcis = req->node_gcis;
    Uint32 node_group_gcis[MAX_NDB_NODES+1];
    memset(node_group_gcis, 0, sizeof(node_group_gcis));
    for (i = 0; i<MAX_NDB_NODES; i++)
    {
      if (mask.get(i))
      {
	jam();
	Uint32 ng = Sysfile::getNodeGroup(i, SYSFILE->nodeGroups);
        if (ng != NO_NODE_GROUP_ID)
        {
          ndbrequire(ng < MAX_NDB_NODE_GROUPS);
          Uint32 gci = node_gcis[i];
          if (gci > 0 && gci + 1 == SYSFILE->lastCompletedGCI[i])
          {
            jam();
            /**
             * Handle case, where *I* know that node complete GCI
             *   but node does not...bug#29167
             *   i.e node died before it wrote own sysfile
             *   and node it only one gci behind
             */
            gci = SYSFILE->lastCompletedGCI[i];
          }

          if (gci > node_group_gcis[ng])
          {
            jam();
            node_group_gcis[ng] = gci;
          }
        }
      }
    }
    for (i = 0; i<MAX_NDB_NODES && node_group_gcis[i] == 0; i++);
    
    Uint32 gci = node_group_gcis[i];
    for (i++ ; i<MAX_NDB_NODES; i++)
    {
      jam();
      if (node_group_gcis[i] && node_group_gcis[i] != gci)
      {
	jam();
	signal->theData[0] = i;
	return;
      }
    }
    signal->theData[0] = MAX_NDB_NODES;
    return;
  }
  return;
}//Dbdih::execDIH_RESTARTREQ()

void Dbdih::execSTTOR(Signal* signal) 
{
  jamEntry();

  Callback c = { safe_cast(&Dbdih::sendSTTORRY), 0 };
  m_sendSTTORRY = c;

  switch(signal->theData[1]){
  case 1:
    createMutexes(signal, 0);
    init_lcp_pausing_module();
    return;
  case 3:
    signal->theData[0] = reference();
    sendSignal(NDBCNTR_REF, GSN_READ_NODESREQ, signal, 1, JBB);
    return;
  }

  sendSTTORRY(signal);
}//Dbdih::execSTTOR()

void
Dbdih::sendSTTORRY(Signal* signal, Uint32 senderData, Uint32 retVal)
{
  signal->theData[0] = 0;
  signal->theData[1] = 0;
  signal->theData[2] = 0;
  signal->theData[3] = 1;   // Next start phase
  signal->theData[4] = 3;
  signal->theData[5] = 255; // Next start phase
  sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 6, JBB);
  return;
}

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
    /*-----------------------------------------------------------------------*/
    // Compute all static block references in this node as part of
    // ndb start phase 1.    
    /*-----------------------------------------------------------------------*/
    cownNodeId = ownNodeId;
    cntrlblockref = cntrRef;
    clocaltcblockref = calcTcBlockRef(ownNodeId);
    clocallqhblockref = calcLqhBlockRef(ownNodeId);
    cdictblockref = calcDictBlockRef(ownNodeId);
    c_lcpState.lcpStallStart = 0;
    c_lcpState.lcpManualStallStart = false;
    NdbTick_Invalidate(&c_lcpState.m_start_lcp_check_time);
    ndbsttorry10Lab(signal, __LINE__);
    break;
    
  case ZNDB_SPH2:
    jam();
    /*-----------------------------------------------------------------------*/
    // For node restarts we will also add a request for permission
    // to continue the system restart.
    // The permission is given by the master node in the alive set.  
    /*-----------------------------------------------------------------------*/
    if (cstarttype == NodeState::ST_INITIAL_NODE_RESTART)
    {
      jam();
      c_set_initial_start_flag = TRUE; // In sysfile...
    }

    if (cstarttype == NodeState::ST_INITIAL_START) {
      jam();
      // setInitialActiveStatus is moved into makeNodeGroups
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
    ndbsttorry10Lab(signal, __LINE__);
    return;

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
    cmasterTakeOverNode = ZNIL;
    switch(typestart){
    case NodeState::ST_INITIAL_START:
      jam();
      ndbassert(c_lcpState.lcpStatus == LCP_STATUS_IDLE);
      c_lcpState.setLcpStatus(LCP_STATUS_IDLE, __LINE__);
      ndbsttorry10Lab(signal, __LINE__);
      return;
    case NodeState::ST_SYSTEM_RESTART:
      jam();
      if (!c_performed_copy_phase)
      {
        jam();
        /**
         * We are not performing the copy phase, it is a normal
         * system restart, we initialise the LCP status to IDLE.
         *
         * When copy phase is performed the LCP processing have
         * already started when we arrive here.
         */
        ndbassert(c_lcpState.lcpStatus == LCP_STATUS_IDLE);
        c_lcpState.setLcpStatus(LCP_STATUS_IDLE, __LINE__);
      }
      ndbsttorry10Lab(signal, __LINE__);
      return;
    case NodeState::ST_INITIAL_NODE_RESTART:
    case NodeState::ST_NODE_RESTART:
      jam();

      /***********************************************************************
       * When starting nodes while system is operational we must be controlled
       * by the master. There can be multiple node restarts ongoing, but this
       * phase only allows for one node at a time. So it has to be controlled
       * from the master node.
       *
       * When this signal is confirmed the master has also copied the 
       * dictionary and the distribution information.
       */
      ndbassert(c_lcpState.lcpStatus == LCP_STATUS_IDLE);
      c_lcpState.setLcpStatus(LCP_STATUS_IDLE, __LINE__);
      g_eventLogger->info("Request copying of distribution and dictionary"
                          " information from master Starting");

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

      infoEvent("Make On-line Database recoverable by waiting for LCP"
                " Starting, LCP id = %u",
                SYSFILE->latestLCP_ID + 1);

      c_lcpState.immediateLcpStart = true;
      cwaitLcpSr = true;
      checkLcpStart(signal, __LINE__, 0);
      return;
    case NodeState::ST_NODE_RESTART:
    case NodeState::ST_INITIAL_NODE_RESTART:
      jam();
      {
        StartCopyReq* req = (StartCopyReq*)signal->getDataPtrSend();
        req->senderRef = reference();
        req->senderData = RNIL;
        req->flags = StartCopyReq::WAIT_LCP;
        req->startingNodeId = getOwnNodeId();
        if (!ndb_pnr(getNodeInfo(refToNode(cmasterdihref)).m_version))
        {
          jam();
          infoEvent("Detecting upgrade: Master(%u) does not support parallel"
                    " node recovery",
                    refToNode(cmasterdihref));
          sendSignal(cmasterdihref, GSN_START_COPYREQ, signal, 
                     StartCopyReq::SignalLength, JBB);
        }
        else
        {
          sendSignal(reference(), GSN_START_COPYREQ, signal, 
                     StartCopyReq::SignalLength, JBB);
        }
      }
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
Dbdih::execNODE_START_REP(Signal* signal)
{
  /*
   * Send DICT_UNLOCK_ORD when this node is SL_STARTED.
   *
   * Sending it before (sp 7) conflicts with code which assumes
   * SL_STARTING means we are in copy phase of NR.
   *
   * NodeState::starting.restartType is not supposed to be used
   * when SL_STARTED.  Also it seems NODE_START_REP can arrive twice.
   *
   * For these reasons there are no consistency checks and
   * we rely on c_dictLockSlavePtrI_nodeRestart alone.
   */
  if (signal->theData[0] == getOwnNodeId())
  {
    /**
     * With parallel node restart, only unlock self, if it's self that has
     *   started
     */
    jam();
    if (c_dictLockSlavePtrI_nodeRestart != RNIL) {
      sendDictUnlockOrd(signal, c_dictLockSlavePtrI_nodeRestart);
      c_dictLockSlavePtrI_nodeRestart = RNIL;
    }
  }
  setGCPStopTimeouts();
}

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
  case 2:{
    Mutex mutex(signal, c_mutexMgr, c_fragmentInfoMutex_lcp);
    mutex.create(c);
    return;
  }
  }

  execute(signal, m_sendSTTORRY, 0);
}

void
Dbdih::createMutex_done(Signal* signal, Uint32 senderData, Uint32 retVal){
  jamEntry();
  ndbrequire(retVal == 0);

  switch(senderData){
  case 0:{
    Mutex mutex(signal, c_mutexMgr, c_startLcpMutexHandle);
    mutex.release();
    break;
  }
  case 1:{
    Mutex mutex(signal, c_mutexMgr, c_switchPrimaryMutexHandle);
    mutex.release();
    break;
  }
  case 2:{
    Mutex mutex(signal, c_mutexMgr, c_fragmentInfoMutex_lcp);
    mutex.release();
    break;
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
    initRestartInfo(signal);
    initGciFilesLab(signal);
    return;
  }
  
  NodeRecordPtr nodePtr;
  Uint32 gci = SYSFILE->lastCompletedGCI[getOwnNodeId()];
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) 
  {
    jam();
    ptrAss(nodePtr, nodeRecord);
    if (SYSFILE->lastCompletedGCI[nodePtr.i] > gci) 
    {
      jam();
      /**
       * Since we're starting(is master) and there 
       *   there are other nodes with higher GCI...
       *   their gci's must be invalidated...
       *   and they _must_ do an initial start
       *   indicate this by setting lastCompletedGCI = 0
       */
      SYSFILE->lastCompletedGCI[nodePtr.i] = 0;
      ndbrequire(nodePtr.p->nodeStatus != NodeRecord::ALIVE);
      warningEvent("Making filesystem for node %d unusable (need --initial)",
		   nodePtr.i);
    }
    else if (nodePtr.p->nodeStatus == NodeRecord::ALIVE &&
	     SYSFILE->lastCompletedGCI[nodePtr.i] == 0)
    {
      jam();
      CRASH_INSERTION(7170);
      char buf[255];
      BaseString::snprintf(buf, sizeof(buf), 
			   "Cluster requires this node to be started "
			   " with --initial as partial start has been performed"
			   " and this filesystem is unusable");
      progError(__LINE__, 
		NDBD_EXIT_SR_RESTARTCONFLICT,
		buf);
      ndbrequire(false);
    }
  }

  /**
   * This set which GCI we will try to restart to
   */
  SYSFILE->newestRestorableGCI = gci;
  infoEvent("Restarting cluster to GCI: %u", gci);

  ndbrequire(isMaster());
  copyGciLab(signal, CopyGCIReq::RESTART); // We have already read the file!
}//Dbdih::ndbStartReqLab()

void Dbdih::execREAD_NODESCONF(Signal* signal) 
{
  unsigned i;
  ReadNodesConf * const readNodes = (ReadNodesConf *)&signal->theData[0];
  jamEntry();
  Uint32 nodeArray[MAX_NDB_NODES+1];

  csystemnodes  = readNodes->noOfNodes;
  cmasterNodeId = readNodes->masterNodeId;
  unsigned index = 0;
  NdbNodeBitmask tmp; tmp.assign(2, readNodes->allNodes);
  for (i = 1; i < MAX_NDB_NODES; i++){
    jam();
    if(tmp.get(i)){
      jam();
      nodeArray[index] = i;
      if(NdbNodeBitmask::get(readNodes->inactiveNodes, i) == false){
        jam();
        con_lineNodes++;        
      }//if      
      index++;
    }//if
  }//for  
  nodeArray[index] = RNIL; // terminate

  if (c_2pass_inr)
  {
    jam();
    Uint32 workers = getNodeInfo(getOwnNodeId()).m_lqh_workers;
#ifdef VM_TRACE
    printf("Checking 2-pass initial node restart: ");
#endif
    for (i = 0; i<index; i++)
    {
      if (NdbNodeBitmask::get(readNodes->inactiveNodes, nodeArray[i]))
        continue;

      if (!ndbd_non_trans_copy_frag_req(getNodeInfo(nodeArray[i]).m_version))
      {
        jam();
        c_2pass_inr = false;
#ifdef VM_TRACE
        printf("not ok (version node %u) => disabled\n", nodeArray[i]);
#endif
        break;
      }

      if (workers > 1 &&
          workers != getNodeInfo(nodeArray[i]).m_lqh_workers)
      {
        c_2pass_inr = false;
#ifdef VM_TRACE
        printf("not ok (different worker cnt node %u) => disabled\n", 
               nodeArray[i]);
#endif
        break;
      }
    }
    if (c_2pass_inr)
    {
#ifdef VM_TRACE
      ndbout_c("ok");
#endif
    }

    /**
     * Note: In theory it would be ok for just nodes that we plan to copy from
     *   supported this...but in e.g a 3/4-replica scenario,
     *      if one of the nodes does, and the other doesnt, we don't
     *      have enough infrastructure to easily check this...
     *      therefore we require all nodes to support it.
     */
  }

  if(cstarttype == NodeState::ST_SYSTEM_RESTART || 
     cstarttype == NodeState::ST_NODE_RESTART)
  {

    for(i = 1; i<MAX_NDB_NODES; i++){
      const Uint32 stat = Sysfile::getNodeStatus(i, SYSFILE->nodeStatus);
      if(stat == Sysfile::NS_NotDefined && !tmp.get(i))
      {
	jam();
	continue;
      }
      
      if(tmp.get(i) && stat != Sysfile::NS_NotDefined)
      {
	jam();
	continue;
      }

      if (stat == Sysfile::NS_NotDefined && tmp.get(i))
      {
        jam();
        infoEvent("Discovered new node %u", i);
        continue;
      }

      if (stat == Sysfile::NS_Configured && !tmp.get(i))
      {
        jam();
        infoEvent("Configured node %u not present, ignoring",
                  i);
        continue;
      }

      char buf[255];
      BaseString::snprintf(buf, sizeof(buf), 
                           "Illegal configuration change."
                           " Initial start needs to be performed "
                           " when removing nodes with nodegroup (node %d)", i);
      progError(__LINE__, NDBD_EXIT_INVALID_CONFIG, buf);
    }
  }
  
  ndbrequire(csystemnodes >= 1 && csystemnodes < MAX_NDB_NODES);  

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

  /**
   * Keep bitmap of nodes that can be restored...
   *   and nodes that need take-over
   *
   */
  m_sr_nodes.clear();
  m_to_nodes.clear();

  // Start with assumption that all can restore
  {
    NodeRecordPtr specNodePtr;
    specNodePtr.i = cfirstAliveNode;
    do {
      jam();
      m_sr_nodes.set(specNodePtr.i);
      ptrCheckGuard(specNodePtr, MAX_NDB_NODES, nodeRecord);
      specNodePtr.i = specNodePtr.p->nextNode;
    } while (specNodePtr.i != RNIL);
  }

  execute(signal, m_sendSTTORRY, 0);
}//Dbdih::execREAD_NODESCONF()

/*---------------------------------------------------------------------------*/
/*                    START NODE LOGIC FOR NODE RESTART                      */
/*---------------------------------------------------------------------------*/
void Dbdih::nodeRestartPh2Lab(Signal* signal) 
{
  /*
   * Lock master DICT to avoid metadata operations during INR/NR.
   * Done just before START_PERMREQ.
   *
   * It would be more elegant to do this just before START_MEREQ.
   * The problem is, on INR we end up in massive invalidateNodeLCP
   * which is not fully protected against metadata ops.
   */
  ndbrequire(c_dictLockSlavePtrI_nodeRestart == RNIL);

  // check that we are not yet taking part in schema ops
  CRASH_INSERTION(7174);

  Uint32 lockType = DictLockReq::NodeRestartLock;
  Callback c = { safe_cast(&Dbdih::recvDictLockConf_nodeRestart), 0 };
  sendDictLockReq(signal, lockType, c);
}

void Dbdih::recvDictLockConf_nodeRestart(Signal* signal, Uint32 data, Uint32 ret)
{
  ndbrequire(c_dictLockSlavePtrI_nodeRestart == RNIL);
  ndbrequire(data != RNIL);
  c_dictLockSlavePtrI_nodeRestart = data;

  nodeRestartPh2Lab2(signal);
}

void Dbdih::nodeRestartPh2Lab2(Signal* signal)
{
  /*------------------------------------------------------------------------*/
  // REQUEST FOR PERMISSION FROM MASTER TO START A NODE IN AN ALREADY
  // RUNNING SYSTEM.
  /*------------------------------------------------------------------------*/

  g_eventLogger->info("Request permission to start our node from master Starting");

  StartPermReq * const req = (StartPermReq *)&signal->theData[0];

  req->blockRef  = reference();
  req->nodeId    = cownNodeId;
  req->startType = cstarttype;
  sendSignal(cmasterdihref, GSN_START_PERMREQ, signal, 3, JBB);

  if (ERROR_INSERTED(7203))
  {
    signal->theData[0] = 9999;
    sendSignalWithDelay(CMVMI_REF, GSN_NDB_TAMPER, signal, 200, 1);
  }
}

void Dbdih::execSTART_PERMCONF(Signal* signal) 
{
  jamEntry();
  CRASH_INSERTION(7121);
  Uint32 nodeId = signal->theData[0];
  cfailurenr = signal->theData[1];
  
  bool microGCP = signal->theData[2];
  if (signal->getLength() < StartPermConf::SignalLength)
  {
    microGCP = false;
  }
  m_micro_gcp.m_enabled = microGCP;
  ndbrequire(nodeId == cownNodeId);
  ndbsttorry10Lab(signal, __LINE__);

  if (m_micro_gcp.m_enabled)
  {
    jam();
    UpgradeProtocolOrd * ord = (UpgradeProtocolOrd*)signal->getDataPtrSend();
    ord->type = UpgradeProtocolOrd::UPO_ENABLE_MICRO_GCP;
    EXECUTE_DIRECT(QMGR,GSN_UPGRADE_PROTOCOL_ORD,signal,signal->getLength());
  }
  else if(isMultiThreaded())
  {
    /**
     * Prevent this start, as there is some non-thread-safe upgrade code for
     * this case in LQH.
     */
    progError(__LINE__, NDBD_EXIT_SR_RESTARTCONFLICT,
              "Cluster requires that all old data nodes are upgraded "
              "while running single-threaded ndbd before starting "
              "multi-threaded ndbmtd data nodes.");
  }

  g_eventLogger->info("Request permission to start our node from master Completed");

}//Dbdih::execSTART_PERMCONF()

void Dbdih::execSTART_PERMREF(Signal* signal) 
{
  jamEntry();
  Uint32 errorCode = signal->theData[1];
  if (errorCode == StartPermRef::ZNODE_ALREADY_STARTING_ERROR ||
      errorCode == StartPermRef::ZNODE_START_DISALLOWED_ERROR) {
    jam();
    /*-----------------------------------------------------------------------*/
    // The master was busy adding another node. We will wait for a few
    // seconds and try again.
    /*-----------------------------------------------------------------------*/
    g_eventLogger->info("Did not get permission to start (%u) retry in 3s",
                        errorCode);
    signal->theData[0] = DihContinueB::ZSTART_PERMREQ_AGAIN;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 3000, 1);
    return;
  }//if

  if (errorCode == StartPermRef::InitialStartRequired)
  {
    CRASH_INSERTION(7170);
    char buf[255];
    BaseString::snprintf(buf, sizeof(buf), 
			 "Cluster requires this node to be started "
			 " with --initial as partial start has been performed"
			 " and this filesystem is unusable");
    progError(__LINE__, 
	      NDBD_EXIT_SR_RESTARTCONFLICT,
	      buf);
    ndbrequire(false);
  }

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
  Uint32 key = SYSFILE->m_restart_seq;
  Uint32 tempGCP[MAX_NDB_NODES];
  for (i = 0; i < MAX_NDB_NODES; i++)
    tempGCP[i] = SYSFILE->lastCompletedGCI[i];

  for (i = 0; i < Sysfile::SYSFILE_SIZE32; i++)
    sysfileData[i] = cdata[i];

  SYSFILE->m_restart_seq = key;
  for (i = 0; i < MAX_NDB_NODES; i++)
    SYSFILE->lastCompletedGCI[i] = tempGCP[i];

  setNodeActiveStatus();
  setNodeGroups();

  g_eventLogger->info("Request copying of distribution and dictionary"
                      " information from master Completed");

  ndbsttorry10Lab(signal, __LINE__);

  if (getNodeActiveStatus(getOwnNodeId()) == Sysfile::NS_Configured)
  {
    jam();
    c_set_initial_start_flag = FALSE;
  }
}//Dbdih::execSTART_MECONF()

void Dbdih::execSTART_COPYCONF(Signal* signal) 
{
  jamEntry();
  
  StartCopyConf* conf = (StartCopyConf*)signal->getDataPtr();
  Uint32 nodeId = conf->startingNodeId;
  Uint32 senderData = conf->senderData;

  if (!ndb_pnr(getNodeInfo(refToNode(signal->getSendersBlockRef())).m_version))
  {
    jam();
    senderData = RNIL;
  }
  
  if (senderData == RNIL)
  {
    /**
     * This is NR
     */
    jam();

    g_eventLogger->info("Make On-line Database recoverable by waiting for"
                        " LCP Completed, LCP id = %u",
                        SYSFILE->latestLCP_ID);
 
    ndbrequire(nodeId == cownNodeId);
    CRASH_INSERTION(7132);
    ndbsttorry10Lab(signal, __LINE__);
  }
  else
  {
    /**
     * This is TO during SR...waiting for all nodes
     */
    infoEvent("Make On-line Database recoverable by waiting for LCP Completed"
              " on node %u, LCP id = %u",
              nodeId,
              SYSFILE->latestLCP_ID);

    ndbrequire(senderData == getOwnNodeId());
    ndbrequire(m_to_nodes.get(nodeId));
    m_to_nodes.clear(nodeId);
    m_sr_nodes.set(nodeId);
    if (!m_to_nodes.isclear())
    {
      jam();
      return;
    }

    infoEvent("Restore Database from disk Completed");

    signal->theData[0] = reference();
    m_sr_nodes.copyto(NdbNodeBitmask::Size, signal->theData+1);
    sendSignal(cntrlblockref, GSN_NDB_STARTCONF, signal, 
               1 + NdbNodeBitmask::Size, JBB);
    return;
  }
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
  if (c_lcpMasterTakeOverState.state != LMTOS_IDLE)
  {
    jam();
    infoEvent("DIH : Denied request for start permission from %u "
              "while LCP Master takeover in progress.",
              nodeId);
    g_eventLogger->info("DIH : Denied request for start permission from %u "
                        "while LCP Master takeover in progress.",
                        nodeId);
    signal->theData[0] = nodeId;
    signal->theData[1] = StartPermRef::ZNODE_START_DISALLOWED_ERROR;
    sendSignal(retRef, GSN_START_PERMREF, signal, 2, JBB);
    return;
  }
  if ((c_nodeStartMaster.activeState) ||
      (c_nodeStartMaster.wait != ZFALSE) ||
      ERROR_INSERTED_CLEAR(7175)) {
    jam();
    signal->theData[0] = nodeId;
    signal->theData[1] = StartPermRef::ZNODE_ALREADY_STARTING_ERROR;
    sendSignal(retRef, GSN_START_PERMREF, signal, 2, JBB);
    return;
  }//if

  if (!getAllowNodeStart(nodeId))
  {
    jam();
    g_eventLogger->info("Rejecting attempt to start node %u", nodeId);
ref:
    signal->theData[0] = nodeId;
    signal->theData[1] = StartPermRef::ZNODE_START_DISALLOWED_ERROR;
    sendSignal(retRef, GSN_START_PERMREF, signal, 2, JBB);
    return;
  }
  if (getNodeStatus(nodeId) != NodeRecord::DEAD)
  {
    jam();
    g_eventLogger->error("nodeStatus in START_PERMREQ = %u",
                         (Uint32) getNodeStatus(nodeId));
    goto ref;
  }//if

  if (SYSFILE->lastCompletedGCI[nodeId] == 0 &&
      typeStart != NodeState::ST_INITIAL_NODE_RESTART)
  {
    jam();
    signal->theData[0] = nodeId;
    signal->theData[1] = StartPermRef::InitialStartRequired;
    sendSignal(retRef, GSN_START_PERMREF, signal, 2, JBB);
    return;
  }

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
  sendLoopMacro(START_INFOREQ, sendSTART_INFOREQ, RNIL);
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
  if (c_nodeStartMaster.startInfoErrorCode == 0)
  {
    jam();
    /**
     * Everything has been a success so far
     *
     * Update node recovery status that we now have received permission to
     * perform node restart from all live nodes. This code only executes
     * in the master node.
     */
    setNodeRecoveryStatus(c_nodeStartMaster.startNode,
                          NodeRecord::START_PERMITTED);

    StartPermConf * conf = (StartPermConf*)&signal->theData[0];
    conf->startingNodeId = c_nodeStartMaster.startNode;
    conf->systemFailureNo = cfailurenr;
    conf->microGCP = m_micro_gcp.m_enabled;
    sendSignal(calcDihBlockRef(c_nodeStartMaster.startNode), 
               GSN_START_PERMCONF, signal, StartPermConf::SignalLength, JBB);
    c_nodeStartMaster.m_outstandingGsn = GSN_START_PERMCONF;
  }
  else
  {
    /**
     * Failure of START_INFO protocol, another node wasn't ready to
     * start this node, some part of handling a previous node failure
     * hadn't completed yet. The node will have to wait a bit more.
     * We need to restore the state such that the retry is possible.
     */
    jam();
    StartPermRef * ref = (StartPermRef*)&signal->theData[0];
    ref->startingNodeId = c_nodeStartMaster.startNode;
    ref->errorCode = c_nodeStartMaster.startInfoErrorCode;
    sendSignal(calcDihBlockRef(c_nodeStartMaster.startNode), 
	       GSN_START_PERMREF, signal, StartPermRef::SignalLength, JBB);
    setNodeStatus(c_nodeStartMaster.startNode, NodeRecord::DEAD);
    nodeResetStart(signal);
  }//if
}//Dbdih::startInfoReply()

/**
 *---------------------------------------------------------------------------
 * LCP Pausing module
 * ------------------
 *
 * This module contains code that executes for the purpose of pausing
 * LCP reporting to our meta data for a short time while we are copying the
 * meta data to a new starting node.
 *
 * In order to better understand the handling of the LCP protocol we will
 * describe the LCP protocol, this includes both the old and the new protocol.
 *
 * The LCP protocol is controlled by the DIH in the master node.
 * When an LCP has been completed we will immediately start checking for
 * the need for a new LCP to be started.
 *
 * The first step here is to ensure that we have had sufficient activity in
 * the cluster to necessitate an LCP to be executed again.
 *
 * To check this we send TCGETOPSIZEREQ to all DBTCs in the cluster. This
 * will gather in an estimate of how much writes we've had in the cluster
 * since the last LCP was started. There are also various ways to ensure
 * that we start an LCP immediately if so needed.
 *
 * If the activity was sufficient we will start the LCP.
 * Before starting the LCP we will calculate a number of GCI values that
 * are important, oldest restorable GCI and so forth.
 * Next we will send TC_CLOPSIZEREQ to all DBTCs in the cluster to clear
 * the activity counter in DBTC as preparation for the next LCP start.
 *
 * In the old way we will then grab a mutex on the fragment info, this
 * mutex will be held until the LCP is completed. The mutex is held in
 * the master node, in a master takeover the mutex needs to be taken
 * also in the new master node. Since all LCPs goes through the master
 * node this has the same effect as a distributed mutex on the fragment
 * info.
 *
 * In the new way we will start the LCP immediately here without grabbing
 * the mutex.
 *
 * The first step in starting is to calculate the set of LQHs involved in
 * the LCP and the set of DIHs involved in the LCP. A node is involved in
 * the LCP in DIH if it has had the meta data copied to it. It will
 * participate in an LCP in LQH if the data has been restored and we're
 * ready to perform a full LCP.
 *
 * Next we update to the new LCP id of the new LCP.
 *
 * The next step is performed in the master node by walking through all
 * fragment replicas of all active tables to see how much of the REDO log
 * we can cut away when starting the new LCP. At the first order of a
 * LCP of a fragment in an LDM instance we will set the new log tail in
 * that LDM instance.
 * 
 * After calculating the new GCI values and setting the LCP id we will
 * synchronize this information with all other nodes in the cluster.
 * This information will also be synchronized to the file system in
 * the Sysfile. This file is where all restarts start by looking at
 * the state of the our database on files.
 * The COPY_GCIREQ signal is used to distribute this message.
 *
 * When all nodes have synchronized this information to disk and confirmed
 * this to the master then we are ready to start sending orders to perform
 * the individual checkpoints of the fragment replicas.
 * 
 * The next step is that we want to set the tables to be involved in the
 * LCP. At this point we want to ensure that the same set of tables is
 * calculated in all nodes. To ensure this we grab the mutex that ensures
 * no tables are able to commit their CREATE TABLE statements until we are
 * done with this step.
 * This is started by the signal START_LCP_REQ. This signal also contains
 * list of nodes involved in the LCP both for LQH and DIH.
 *
 * CREATE TABLE can create new tables prior to this point  which we will
 * include, and that's ok as they cannot possibly affect the new redo tail
 * position. DROP TABLE can drop tables prior to this point, which could
 * remove the need to maintain some old redo, but that will be handled in
 * the following LCP.
 *
 * Each table to execute the LCP on is marked with a proper state in the
 * variable tabLcpStatus. Also each fragment replica to execute the LCP
 * on is marked with true in the lcpOngoingFlag and we set the number of
 * replicas to perform LCP on per fragment as well.
 *
 * These preparatory steps are done in a synchronized manner, so all nodes
 * have received information about the COPY_GCIREQ and now all nodes have
 * heard the START_LCP_REQ signals. So in a master takeover we can ask all
 * nodes about their LCP state and we can derive if we sent the COPY_GCIREQ
 * to all nodes and similarly we can derive if we sent and completed the
 * START_LCP_REQ step. To derive this requires all nodes to have heard of
 * those signals, not just one of them since a crash can occur in the
 * middle of signal sending.
 *
 * In a master takeover if we haven't completed the COPY_GCIREQ step then
 * we can start the next LCP from the beginning again. If COPY_GCIREQ has
 * been completed but not the START_LCP_REQ, then we can restart the
 * START_LCP_REQ step. Finally if the START_LCP_REQ has been completed
 * then we know that the execution of checkpoints on individual fragment
 * replicas is ongoing. Obviously in a master take over we should ensure
 * that the processing of START_LCP_REQ is completed before we report
 * back our state to the master node to ensure that we make the master
 * takeover handling as simple as possible.
 *
 * So now that we know exactly which tables and fragment replicas to checkpoint
 * it is time to start the actual checkpoint phase.
 *
 * The master node will send LCP_FRAG_ORD to DBLQH for each of the fragment
 * replicas to execute the LCP on.
 *
 * In the old way there was a queue of such LCP_FRAG_ORD with limited size in
 * DBDIH (queue size was 2 in 7.3 and earlier and 128 in early 7.4 versions).
 * Also DBLQH had a queue for LCP_FRAG_ORDs, in 7.3 this was 2 in size and
 * in early versions of 7.4 it was 64.
 *
 * In the new version we can send LCP_FRAG_ORD to LQH as before, LQH has an
 * infinite queue size (it simply stores the LCP_FRAG_ORD on the fragment
 * record, so there is no limit to the queue size since all fragments can
 * be in the queue). In addition at master takeover we also support receiving
 * the same order two or more times. By ensuring that we keep track of that
 * we already received a LCP_FRAG_ORD on a fragment we can also easily discard
 * LCP_FRAG_ORDs that we already received.
 *
 * These features mean that LQH can process a Local Checkpoint without much
 * interaction with DIH / DIH Master, which enables simplifications at DIH
 * and DIH Master in later versions. In principle we could send off all
 * LCP_FRAG_ORDs immediately if we like and more or less turn the LDM
 * instances into independent LCP execution engines. This is a step in the
 * direction of more local control in LQH over LCP execution.
 *
 * When all LCP_FRAG_ORD have been sent, then a special LCP_FRAG_ORD to all
 * participating LQH nodes. This signal has the flag lastFragmentFlag set,
 * it doesn't contain any fragment to checkpoint, it is only a flag that
 * indicates that we've sent the last LCP_FRAG_ORD.
 *
 * LQH will execute orders to execute LCP on a fragment in the order they are
 * received. As a fragment is completing its LCP it will generate a new message
 * LCP_FRAG_REP. This message is broadcasted to all participating DIHs. First
 * the message is sent from DBLQH to the local DIH. Finally the local DIH will
 * broadcast it to all participating DIHs.
 * 
 * This new Pausing LCP module is involved here by being able to queue also
 * LCP_FRAG_REP before they are broadcast to the participating DIHs. They are
 * queued on the fragment replica records in the local DIH and thus we have
 * no limits on the queue size.
 *
 * This allows the DIH Master state to be stabilised as necessary during an
 * LCP, removing the need in some cases to wait for an LCP to complete before
 * performing some other activity.
 *
 * When LQH have executed all the LCP_FRAG_ORDs and have received the
 * last fragment flag, then the LDM will perform a number of activities to
 * complete the local checkpoint. These activities is mostly used by the
 * disk data tables.
 *
 * After all these activities have completed the LQH will send
 * LCP_COMPLETE_REP to the local DIH. The local DIH will broadcast it to all
 * participating DIHs.
 * 
 * When all LQHs have sent all LCP_FRAG_REP and it has also sent the
 * LCP_COMPLETE_REP, then the LCP is completed. So a node that has seen
 * LCP_COMPLETE_REP from all nodes participating in the LCP knows that
 * it has received all the LCP_FRAG_REP for the LCP.
 *
 * In a master takeover in the old way we could not resend the LCP_FRAG_ORD
 * to the LQH again. To avoid this we used an extra master takeover
 * protocol EMPTY_LCP_REQ. This protocol ensures that all LQHs have completed
 * the queues and that all LCP_FRAG_REPs have been sent to all participating
 * DIHs and likewise with the LCP_COMPLETE_REP such that the new master has
 * a precise view of which fragment replicas have completed the LCP execution
 * so far.
 *
 * Thus when the master takeover is completed we know that each DIH has all
 * the LCP_FRAG_REP for which an LCP_FRAG_ORD have been sent and also all
 * LCP_COMPLETE_REP that have been produced. This means that we are now
 * ready to restart the process of sending LCP_FRAG_ORD again.
 *
 * The problem with this approach is that can consume a very long time to
 * execute the entire LCP fragment queue in LQH if the queue size increases
 * (increased from 2 to 64 going from 7.3 to 7.4) and the size of the
 * fragments also increase. So the master takeover can take a substantial
 * time in this case.
 *
 * So the new manner is to allow for the LQH to get LCP_FRAG_ORD and also
 * the special last LCP_FRAG_ORD several times with the same LCP id and
 * discard those that it receives for a second time. In this manner we can
 * simply restart sending the LCP_FRAG_ORD from the beginning. When we are
 * done with this we can start checking for completion of the LCP in the
 * normal way.
 *
 * When the master has sent the last special LCP_FRAG_ORD and these have been
 * received by the receiving nodes, then the master will actually itself not
 * do anything more to execute the LCP. The non-master nodes will however send
 * LCP_COMPLETE_REP to the master node. So this means that a new LCP won't
 * start until all participating DIHs have completed the processing of the
 * last LCP.
 *
 * So effectively taking over as master in this phase doesn't really require
 * any specific work other than redirecting the LCP_COMPLETE_REP from the
 * non-masters to the new master. If it has already been sent it should be
 * seen in the response to the MASTER_LCPREQ from the node. So after
 * receiving the last MASTER_LCPCONF we have information enough about whether
 * we need to send more LCP_FRAG_ORDs or not.
 *
 * We can still optimise the sending of LCP_FRAG_ORD a little bit by avoiding
 * to send LCP_FRAG_ORD to a fragment replica where we have already received
 * a LCP_FRAG_REP for it. It would be possible to avoid sending extra
 * LCP_FRAG_ORDs in various ways, but it doesn't really cost much, LCP_FRAG_ORD
 * is a small signal and the number of signals sent is limited to the number
 * of fragment replicas. So this would make sense if we have to support
 * extremely large clusters and extremely many tables in combination.
 *
 * As this description shows some interesting places to test master failures
 * are:
 * 1) Master failure while clearing TC counters (TC_CLOPSIZEREQ).
 * 2) Master failure while distributing COPY_GCIREQ.
 * 3) Master failure while distributing START_LCP_REQ
 * 4) Master failure while processing the LCP and sending LCP_FRAG_ORDs
 * 4.1) Before any LCP_FRAG_REP received
 * 4.2) After receiving many LCP_FRAG_REPs, but not all
 * 4.3) After receiving all LCP_FRAG_REPs, but not all LCP_COMPLETE_REPs
 * 4.4) After receiving all LCP_FRAG_REPs, and all LCP_COMPLETE_REPs.
 *
 * While distributing above can be interpreted as one test case of before
 * distributing, one in the middle of distributing and one when all
 * responses have been received.
 * 
 * It is also important to similarly test PAUSE_LCP_REQ handling in all of
 * the above states. This can be handled by inserting an ERROR_INSERT that
 * effectively stops the process to copy meta data at some point and then
 * setting some variable that triggers the copying of meta data to continue
 * at a state that we wanted to accomplish.
 *---------------------------------------------------------------------------*/
/* Initialisation routine, called once at startup of the node */
void Dbdih::init_lcp_pausing_module(void)
{
  /* Master state variables */
  c_pause_lcp_master_state = PAUSE_LCP_IDLE;
  c_lcp_runs_with_pause_support = false;
  c_old_node_waiting_for_lcp_end = false;

  /* Pause participant state variables */
  c_dequeue_lcp_rep_ongoing = false;
  c_queued_lcp_complete_rep = false;
  c_lcp_id_paused = RNIL;
  c_pause_lcp_start_node = RNIL;
  c_last_id_lcp_complete_rep = RNIL;

  /* Starting node state variable */
  c_lcp_id_while_copy_meta_data = RNIL;
}

void Dbdih::check_pause_state_lcp_idle(void)
{
  /**
   * We should not be able to complete an LCP while still having
   * queued LCP_COMPLETE_REP and LCP_FRAG_REP.
   */
  ndbrequire(c_queued_lcp_frag_rep.isEmpty());
  ndbrequire(!c_queued_lcp_complete_rep);
}

/* Support function only called within ndbassert */
bool Dbdih::check_pause_state_sanity(void)
{
  if (is_lcp_paused())
  {
    ndbrequire(!c_dequeue_lcp_rep_ongoing);
  }
  ndbrequire(c_lcp_id_paused == RNIL ||
             is_lcp_paused() ||
             c_dequeue_lcp_rep_ongoing);
  ndbrequire(!c_old_node_waiting_for_lcp_end ||
             c_lcp_runs_with_pause_support);
  return true;
}

/* Support function for execLCP_FRAG_REP */
void Dbdih::queue_lcp_frag_rep(Signal *signal, LcpFragRep *lcpReport)
{
  Uint32 tableId = lcpReport->tableId;
  Uint32 fragId = lcpReport->fragId;

  TabRecordPtr tabPtr;
  tabPtr.i = tableId;
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
  
  if (tabPtr.p->tabStatus == TabRecord::TS_DROPPING ||
      tabPtr.p->tabStatus == TabRecord::TS_IDLE)
  {
    jam();
    return;
  }

  FragmentstorePtr fragPtr;
  getFragstore(tabPtr.p, fragId, fragPtr);
  
  ReplicaRecordPtr replicaPtr;
  findReplica(replicaPtr, fragPtr.p, lcpReport->nodeId);
  c_queued_lcp_frag_rep.addLast(replicaPtr);
  ndbrequire(replicaPtr.p->nextLcp == lcpReport->lcpNo);
  ndbrequire(replicaPtr.p->fragId == fragId);
  ndbrequire(replicaPtr.p->tableId == tableId);
  ndbrequire(replicaPtr.p->procNode == lcpReport->nodeId);
  ndbrequire(c_lcp_id_paused == RNIL ||
             c_lcp_id_paused == lcpReport->lcpId);
  c_lcp_id_paused = lcpReport->lcpId;
  replicaPtr.p->repMaxGciStarted = lcpReport->maxGciStarted;
  replicaPtr.p->repMaxGciCompleted = lcpReport->maxGciCompleted;
  ndbassert(check_pause_state_sanity());
}

/* Support function for execLCP_COMPLETE_REP */
void Dbdih::queue_lcp_complete_rep(Signal *signal, Uint32 lcpId)
{
  ndbrequire(!c_queued_lcp_complete_rep);
  c_queued_lcp_complete_rep = true;
  ndbrequire(c_lcp_id_paused == RNIL ||
             c_lcp_id_paused == lcpId);
  c_lcp_id_paused = lcpId;
  ndbassert(check_pause_state_sanity());
}

/* Support function to start copying of meta data */
void Dbdih::start_copy_meta_data(Signal *signal)
{
  /**
   * Now that we have locked both the DICT lock and the LCPs are locked from
   * starting we are ready to copy both the distribution information and the
   * dictionary information. We update the node recovery status indicating
   * this. This code only executes in the master node.
   */
  setNodeRecoveryStatus(c_nodeStartMaster.startNode,
                        NodeRecord::COPY_DICT_TO_STARTING_NODE);

  c_nodeStartMaster.wait = 10;
  signal->theData[0] = DihContinueB::ZCOPY_NODE;
  signal->theData[1] = 0;
  sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
  c_nodeStartMaster.m_outstandingGsn = GSN_COPY_TABREQ;
}

/**---------------------------------------------------------------
 * MASTER FUNCTIONALITY
 **--------------------------------------------------------------*/
/**
 * If all nodes that are currently running the LCP can support PAUSE of an
 * LCP then we can use this function to find this out. We compute this
 * variable at a point where we start the LCP. We can still not get an old
 * node up and running until we get to a natural pause between two LCPs.
 * 
 * If an old node comes around then it will block until the LCP is done,
 * this will also ensure that no other nodes will try to become part of
 * this LCP. However we could have new node already being included in
 * this LCP and then have more new nodes arriving that want to be included
 * and we can also have an old node arriving while we are including a new
 * node. But only one node at a time will be in the copy meta data phase
 * so this will work fine.
 */
bool Dbdih::check_if_pause_lcp_possible(void)
{
  NodeRecordPtr nodePtr;
  ndbrequire(isMaster());
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++)
  {
    ptrAss(nodePtr, nodeRecord);
    if (nodePtr.p->nodeStatus == NodeRecord::ALIVE)
    {
      if (getNodeInfo(nodePtr.i).m_version < NDBD_SUPPORT_PAUSE_LCP)
      {
        jam();
        return false;
      }
    }
  }
  return true;
}

/* Support function to check if LCP is still runnning */
bool Dbdih::check_if_lcp_idle(void)
{
  ndbrequire(isMaster());
  switch (c_lcpState.lcpStatus)
  {
  case LCP_STATUS_IDLE:
  case LCP_TCGET:
  case LCP_TC_CLOPSIZE:
  case LCP_WAIT_MUTEX:
    jam();
    check_pause_state_lcp_idle();
    return true;
  case LCP_STATUS_ACTIVE:
    jam();
    return false;
  case LCP_TAB_COMPLETED:
    jam();
  case LCP_TAB_SAVED:
    jam();
  /**
   * For LCP_TAB_COMPLETED and LCP_TAB_COMPLETED we have already received
   * all the table information and thus there is no need to get the new
   * node into the LCP, there won't be any updates to the LCP data until
   * the next LCP happens.
   */
    return true;
  default:
    jam();
    return false;
  }
}

/* Send PAUSE_LCP_REQ to pause or to unpause, master code */
void Dbdih::sendPAUSE_LCP_REQ(Signal *signal, bool pause)
{
  PauseLcpReq *req = (PauseLcpReq*)signal->getDataPtrSend();

  /**
   * Send to all DIHs that participate in the LCP, including ourselves.
   * We will set up waiting for all those signals such that we can also
   * handle node failures in the middle of the pause process.
   */
  ndbrequire(isMaster());
  if (pause)
  {
    jam();
    ndbrequire(c_pause_lcp_master_state == PAUSE_LCP_IDLE);
    c_pause_lcp_master_state = PAUSE_LCP_REQUESTED;
    req->pauseAction = PauseLcpReq::Pause;
    c_pause_participants = c_lcpState.m_participatingLQH;
    infoEvent("PAUSE LCP for starting node %u", c_nodeStartMaster.startNode);
  }
  else
  {
    /**
     * We are unpausing the LCP again after completing the copy of the meta
     * data, slightly different dependent on whether the starting node was
     * included into the LCP or not.
     */
    if (c_pause_lcp_master_state == PAUSE_COMPLETE_LCP_INCLUSION)
    {
      jam();
      ndbrequire(!check_if_lcp_idle());
      c_pause_lcp_master_state = PAUSE_IN_LCP_UNPAUSE;
      req->pauseAction = PauseLcpReq::UnPauseIncludedInLcp;
      infoEvent("UNPAUSE LCP for starting node %u, included in LCP",
                c_nodeStartMaster.startNode);
    }
    else if (c_pause_lcp_master_state == PAUSE_NOT_IN_LCP_COPY_META_DATA)
    {
      jam();
      ndbrequire(check_if_lcp_idle());
      c_pause_lcp_master_state = PAUSE_NOT_IN_LCP_UNPAUSE;
      req->pauseAction = PauseLcpReq::UnPauseNotIncludedInLcp;
      infoEvent("UNPAUSE LCP for starting node %u, not included in LCP",
                c_nodeStartMaster.startNode);
    }
    else
    {
      ndbrequire(false);
    }
  }
  /**
   * The blocks that do the pausing is the local DIH in the nodes that
   * generate LCP_FRAG_REPs and LCP_COMPLETE_REPs. These are the
   * m_participatingLQH nodes. This set is untouched by new starting
   * nodes for this LCP. New nodes can be added to the next LCP, but
   * not to this one.
   *
   * As part of the pause protocol the starting node must also participate
   * in the LCP completion protocol, so the pause also includes taking the
   * starting node into the DIH node set that participates in the LCP.
   * We do however wait including the node until we reach the UnPause
   * action. The reason is that it is possible that the LCP is completed
   * in the process of pausing. In this case we will continue
   * completing the pause in the normal manner, but we will not send
   * START_LCP_REQ to the new node and we will not include the new in the
   * m_participatingDIH bitmap in the DIH nodes already participating
   * in the LCP.
   *
   * For those nodes that existed previously in the m_participatingDIH
   * bitmap, but not in the m_participatingLQH bitmap we need not
   * worry since they won't make use of the m_participatingDIH bitmap.
   * So there is no need to add the starting node into those. The
   * m_participatingDIH bitmap is used by those nodes that generate
   * LCP_FRAG_REPs and LCP_COMPLETE_REPs, and these nodes are exactly
   * the nodes found in the m_participatingLQH bitmap.
   */

  req->senderRef = reference();
  req->startNodeId = c_nodeStartMaster.startNode;
  if (req->pauseAction == PauseLcpReq::UnPauseIncludedInLcp)
  {
    jam();
    c_lcpState.m_LCP_COMPLETE_REP_Counter_DIH.setWaitingFor(
      c_nodeStartMaster.startNode);
  }
  c_PAUSE_LCP_REQ_Counter.setWaitingFor(c_pause_participants);
  NodeReceiverGroup rg(DBDIH, c_pause_participants);
  rg.m_nodes.clear(getOwnNodeId());
  sendSignal(rg, GSN_PAUSE_LCP_REQ, signal,
             PauseLcpReq::SignalLength, JBB);
  /**
   * We execute the signal to ourself immediately, the reason is to
   * avoid having to add a specific state variable to detect when the
   * starting node have failed between now and receiving this signal.
   */
  execPAUSE_LCP_REQ(signal);
  ndbassert(check_pause_state_sanity());
}

/* Master code, other node has completed PAUSE_LCP_REQ */
void Dbdih::execPAUSE_LCP_CONF(Signal *signal)
{
  PauseLcpConf *conf = (PauseLcpConf*)&signal->theData[0];
  Uint32 nodeId = refToNode(conf->senderRef);
  Uint32 startNode = conf->startNodeId;

  ndbrequire(isMaster());
  
  if (!is_pause_for_this_node(startNode))
  {
    /* Ignore, node died in the process */
    jam();
    return;
  }
  ndbassert(check_pause_state_sanity());
  receiveLoopMacro(PAUSE_LCP_REQ, nodeId);

  if (c_pause_lcp_master_state == PAUSE_LCP_REQUESTED)
  {
    jam();
    /**
     * We have paused the reporting of LCPs, we are now ready to process the
     * copying of meta data. At this point in time we have sent PAUSE_LCP_REQ
     * to all LQH nodes participating in the LCP. Those in turn have sent
     * FLUSH_LCP_REP_REQ to all DIH participants and received a response
     * back from all nodes. This means that we have ensured that we have
     * absolutely no LCP_FRAG_REP and LCP_COMPLETE_REP signals in transit
     * in the entire cluster since we have sent a signal through every
     * link that could carry such a signal. We use the FIFO queue mechanism
     * of signals between two DIHs here as an important part of the protocol.
     *
     * This means that all DIHs now have the same view on the
     * LCP_FRAG_REPs they have seen and similarly for LCP_COMPLETE_REPs.
     * The LCP_COMPLETE_REPs could however still be sent back to ourselves
     * through a delayed signal since we don't want to process those
     * signals concurrently with pausing the LCP.
     *
     * We could end up in a situation where the LCP have completed here, but
     * this isn't a problem, we still hold the fragment info mutex, so no
     * new LCP can start until we are done with the copying and release the
     * fragment info mutex.
     */
    ndbassert(check_pause_state_sanity());
    check_for_pause_action(signal, StartLcpReq::PauseLcpStartFirst);
    return;
  }
  /**
   * UnPause
   * ------
   * This is the normal path for unpausing. At this point we have sent
   * PAUSE_LCP_REQ to all LQH nodes participating in the LCP. These nodes
   * have now started sending the LCP_FRAG_REPs and LCP_COMPLETE_REPs
   * again. The copying of meta data have been completed and we have
   * been included in the LCP handling. So we are now ready to proceed
   * with the node restart again. We will also perform the unpause
   * on the master node here to avoid interesting states between
   * stop pause and receiving the last PAUSE_LCP_CONF.
   */
  jam();
  ndbrequire(c_pause_lcp_master_state == PAUSE_NOT_IN_LCP_UNPAUSE ||
             c_pause_lcp_master_state == PAUSE_IN_LCP_UNPAUSE);
  if (c_pause_lcp_master_state == PAUSE_NOT_IN_LCP_UNPAUSE)
  {
    jam();
    end_pause(signal, PauseLcpReq::UnPauseNotIncludedInLcp);
  }
  else if (c_pause_lcp_master_state == PAUSE_IN_LCP_UNPAUSE)
  {
    jam();
    end_pause(signal, PauseLcpReq::UnPauseIncludedInLcp);
  }
  else
  {
    ndbrequire(false);
  }
  dihCopyCompletedLab(signal);
}

/**-------------------------------------------------------------------
  FUNCTIONS USED IN ALL NODES
--------------------------------------------------------------------*/
/**
 * PAUSE_LCP_REQ
 * -------------
 * This signal is sent from the master node to all DIHs to block distribution
 * of LCP_FRAG_REP signals. When we receive this signal we will queue all
 * signals that we receive from DBLQH about completed LCP fragments. The same
 * signal is also sent to stop the pause. The pauseAction is 0 for pause and
 * 1 for stop pause.
 *
 * After pausing locally in our own DBDIH, we will send a FLUSH_LCP_REP_REQ
 * to all nodes participating in the LCP. This ensures that any LCP_FRAG_REP
 * we have sent out has been received by the receiving node since we are
 * sending it on the same path and we have a guarantee that signals using
 * the same path won't race each other.
 */
void Dbdih::execPAUSE_LCP_REQ(Signal *signal)
{
  PauseLcpReq *req = (PauseLcpReq*) &signal->theData[0];
  PauseLcpReq::PauseAction pauseAction =
    (PauseLcpReq::PauseAction)req->pauseAction;
  Uint32 startNode = req->startNodeId;

  ndbrequire(req->senderRef == cmasterdihref);
  ndbassert(check_pause_state_sanity());

  /* TODO: Insert check that startNode is still alive here */
  if (pauseAction == PauseLcpReq::Pause)
  {
    jam();
    pause_lcp(signal, startNode, req->senderRef);
  }
  else
  {
    jam();
    unpause_lcp(signal,
                startNode,
                req->senderRef,
                pauseAction);
  }
  return;
}

void Dbdih::pause_lcp(Signal *signal,
                      Uint32 startNode,
                      BlockReference sender_ref)
{
  /**
   * Since the message comes from the master on behalf of the starting
   * node we need to ensure that the starting node hasn't failed already.
   * We handle stopping of pause at node failure, but if this arrives
   * after we already received NODE_FAILREP we need to ensure that we
   * don't proceed since this will cause havoc.
   */
  if (!isMaster())
  {
    /**
     * We should come here after getting permit to start node, but before
     * we the node is included into the LCP and GCP protocol, this happens
     * immediately after we copied the meta data which the PAUSE LCP
     * protocol is part of handling.
     */
    NodeRecordPtr nodePtr;
    nodePtr.i = startNode;
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
    if (!nodePtr.p->is_pausable)
    {
      jam();
      /* Ignore, node already died */
      return;
    }
    /**
     * Verify that the master isn't starting PAUSE protocol for old nodes
     * that doesn't support the PAUSE LCP protocol. We make it an assert mostly
     * to find bugs early on, a proper handling would probably be to shoot
     * down the master node.
     */
    ndbassert(getNodeInfo(startNode).m_version >= NDBD_SUPPORT_PAUSE_LCP);
  }

  ndbrequire(sender_ref == cmasterdihref);
  if (c_dequeue_lcp_rep_ongoing)
  {
    jam();
    /**
     * Stop unpause mechanism as we are starting a new pause action.
     */
    c_dequeue_lcp_rep_ongoing = false;
  }
  c_pause_lcp_start_node = startNode;

  /**
   * Send flush signal to all nodes participating in LCP.
   * We need not send to ourselves since we don't send LCP_FRAG_REP
   * to ourselves. We need to keep track of which nodes that have
   * replied to the message.
   */
  FlushLcpRepReq *req = (FlushLcpRepReq*) signal->getDataPtrSend();
  req->senderRef = reference();
  req->startNodeId = startNode;
  c_FLUSH_LCP_REP_REQ_Counter.setWaitingFor(c_lcpState.m_participatingDIH);
  NodeReceiverGroup rg(DBDIH, c_lcpState.m_participatingDIH);

  sendSignal(rg, GSN_FLUSH_LCP_REP_REQ, signal,
             FlushLcpRepReq::SignalLength, JBB);

  ndbassert(check_pause_state_sanity());
}

void Dbdih::check_for_pause_action(Signal *signal,
                                   StartLcpReq::PauseStart pauseStart)
{
  ndbrequire(is_lcp_paused());
  if (!check_if_lcp_idle())
  {
    jam();
    /**
     * A next step when we have paused the LCP execution is to get the
     * starting node active in the LCP handling. This means we need to send
     * START_LCP_REQ to the node. We won't track the reply here since a
     * missing reply is due to a crashed node and then the node failure
     * handling will ensure that the LCP is restarted and that the pause of
     * the LCP is unpaused.
     * (A test case for this is needed).
     *
     * At this point in time we have stalled all activity in the LCP.
     * This means that the bit maps on participating LQHs and DIHs is
     * stable, it also means that the bit maps for which LQHs and DIHs
     * that have completed is also stable (we have stopped LCP_COMPLETE_REP
     * to pass through in all nodes). There might be LQHs and DIHs that
     * have already completed and we need this information to also be
     * transferred to the starting node for it to be able to complete
     * the LCP processing properly.
     *
     * This means we actually have to send two signals with all four
     * bitmaps. After these signals have been sent over we will
     * be ready to copy the meta data and after that to unpause and
     * complete this LCP with the starting node as a new participant.
     *
     * It is vital to send this information before we copy the meta
     * data since the m_participatingLQH bitmap is needed to set
     * the lcpOngoing flag on the replicas set correctly.
     */
    StartLcpReq* req = (StartLcpReq*)signal->getDataPtrSend();
    BlockReference ref = calcDihBlockRef(c_nodeStartMaster.startNode);
    req->senderRef = reference();
    req->lcpId = SYSFILE->latestLCP_ID;
    req->pauseStart = pauseStart;
    if (pauseStart == StartLcpReq::PauseLcpStartFirst)
    {
      jam();
      ndbrequire(c_pause_lcp_master_state == PAUSE_LCP_REQUESTED);
      c_pause_lcp_master_state = PAUSE_START_LCP_INCLUSION;
      req->participatingLQH = c_lcpState.m_participatingLQH;
      req->participatingDIH = c_lcpState.m_participatingDIH;
      sendSignal(ref, GSN_START_LCP_REQ, signal,
                 StartLcpReq::SignalLength, JBB);
    }
    else
    {
      bool found = false;
      ndbrequire(pauseStart == StartLcpReq::PauseLcpStartSecond);
      ndbrequire(c_pause_lcp_master_state == PAUSE_IN_LCP_COPY_META_DATA);
      c_pause_lcp_master_state = PAUSE_COMPLETE_LCP_INCLUSION;
      req->participatingLQH.clear();
      for (Uint32 nodeId = 1; nodeId < MAX_NDB_NODES; nodeId++)
      {
        if (c_lcpState.m_LCP_COMPLETE_REP_Counter_LQH.isWaitingFor(nodeId))
        {
          jamLine(nodeId);
          req->participatingLQH.set(nodeId);
          found = true;
        }
      }
      /**
       * We should not be able to have all LQH sent completed, but not all
       * LCP_FRAG_REP yet received.
       */
      ndbrequire(found);
      sendSignal(ref, GSN_START_LCP_REQ, signal,
                 StartLcpReq::SignalLength, JBB);
      return;
    }
  }
  else
  {
    if (pauseStart == StartLcpReq::PauseLcpStartFirst)
    {
      jam();
      /**
       * The LCP completed while we paused, no need to prepare the starting
       * node for inclusion into the LCP protocol since we will continue
       * with the node restart immediately after completing the copy of the
       * meta data and the unpause action.
       */
      ndbrequire(c_pause_lcp_master_state == PAUSE_LCP_REQUESTED);
      c_pause_lcp_master_state = PAUSE_NOT_IN_LCP_COPY_META_DATA;
      start_copy_meta_data(signal);
    }
    else
    {
      jam();
      /**
       * The LCP completed while we paused and we have now copied the meta
       * data over. We are ready to unpause and need not include the new
       * node into the LCP protocol this time.
       */
      ndbrequire(pauseStart == StartLcpReq::PauseLcpStartSecond);
      ndbrequire(c_pause_lcp_master_state == PAUSE_NOT_IN_LCP_COPY_META_DATA);
      sendPAUSE_LCP_REQ(signal, false);
    }
    return;
  }
}

void Dbdih::unpause_lcp(Signal *signal,
                        Uint32 startNode,
                        BlockReference sender_ref,
                        PauseLcpReq::PauseAction pauseAction)
{
  if (!is_pause_for_this_node(startNode))
  {
    jam();
    /* Ignore, node already died */
    return;
  }
  /**
   * When we stop pausing we will set the dequeue flag, LCP_FRAG_REPs and
   * LCP_COMPLETE_REPs will continue to be queued while any of those two
   * flags are set to ensure that we keep the order of LCP_FRAG_REP. This
   * order isn't absolutely necessary, but it makes it easier to debug
   * the system.
   */
  PauseLcpConf *conf = (PauseLcpConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->startNodeId = startNode;
  sendSignal(cmasterdihref, GSN_PAUSE_LCP_CONF, signal,
             PauseLcpConf::SignalLength, JBB);

  if (isMaster())
  {
    jam();
    /**
     * We complete the Pause LCP protocol in master when all nodes
     * have returned. Too early here.
     */
    return;
  }
  end_pause(signal, pauseAction);
}

void Dbdih::end_pause(Signal *signal,
                      PauseLcpReq::PauseAction pauseAction)
{
  if (pauseAction == PauseLcpReq::UnPauseIncludedInLcp)
  {
    jam();
    c_lcpState.m_participatingDIH.set(c_pause_lcp_start_node);
  }
  stop_pause(signal);
}

void Dbdih::stop_pause(Signal *signal)
{
  if (isMaster())
  {
    jam();
    c_pause_participants.clear();
    c_pause_lcp_master_state = PAUSE_LCP_IDLE;
  }
  c_pause_lcp_start_node = RNIL;
  ndbrequire(!c_dequeue_lcp_rep_ongoing);
  c_dequeue_lcp_rep_ongoing = true;
  ndbassert(check_pause_state_sanity());
  dequeue_lcp_rep(signal);
}

/**
 * All node failures while being in LCP pause state leads to immediate based
 * on the assumption that all node failures will also automatically lead
 * to failures of any starting nodes while we are still in the starting
 * state.
 *
 * This means we need no code to handle unpausing at node failures.
 */
void Dbdih::handle_node_failure_in_pause(Signal *signal)
{
  c_FLUSH_LCP_REP_REQ_Counter.clearWaitingFor();
  c_PAUSE_LCP_REQ_Counter.clearWaitingFor();
  stop_pause(signal);
  ndbassert(check_pause_state_sanity());
}

/**
 * We have stopped pausing and we are working through the queue of blocked
 * LCP reports. When we reach the end of it we will unset the dequeue flag
 * such that we need no more queue the LCP reports.
 *
 * We will dequeue one LCP report per signal and continue sending CONTINUEB
 * to ourselves until we're through the LCP reports that have blocked while
 * we paused.
 *
 * NOTE: The queue might be empty for a short while we are waiting for a
 * CONTINUEB to arrive. We don't check for emptiness before sending
 * CONTINUEB. So if one wants to add asserts on queue not empty while
 * flag is set, then this needs to be checked before CONTINUEB is sent.
 */
void Dbdih::dequeue_lcp_rep(Signal *signal)
{
  ReplicaRecordPtr replicaPtr;
  bool empty;
  bool lcp_frag_rep_empty = c_queued_lcp_frag_rep.isEmpty();
  bool lcp_complete_rep_empty = !c_queued_lcp_complete_rep;
  if (!c_dequeue_lcp_rep_ongoing)
  {
    jam();
    ndbassert(check_pause_state_sanity());
    /**
     * We got a new pause signal before finishing off the queue, we will
     * stop dequeuing, the pause flag is already set and should continue
     * to be so.
     */
    return;
  }
  empty = lcp_frag_rep_empty && lcp_complete_rep_empty;
  /* Perform dequeueing of one LCP report */
  if (!empty)
  {
    if (!lcp_frag_rep_empty)
    {
      jam();
      /**
       * 1) Remove from queue
       * 2) Set up signal
       * 3) Send to all LCP DIH participants
       * 4) Send CONTINUEB for handling next in queue
       *
       * We also need to send to ourselves which is a bit different from
       * the normal LCP_FRAG_REP where we handle ourselves through a fall
       * through method. Here we come from a different place and we cannot
       * use the broadcast method since the dequeue flag is still set.
       * So we send the signals from here to all nodes in the DIH set
       * (including the starting node).
       */
      LcpFragRep *lcpFragRep = (LcpFragRep*)signal->getDataPtrSend();

      c_queued_lcp_frag_rep.first(replicaPtr);
      ndbrequire(replicaPtr.p != NULL);
      c_queued_lcp_frag_rep.removeFirst(replicaPtr);

      lcpFragRep->nodeId = getOwnNodeId();
      lcpFragRep->lcpId = c_lcp_id_paused;
      lcpFragRep->lcpNo = replicaPtr.p->nextLcp;
      lcpFragRep->tableId = replicaPtr.p->tableId;
      lcpFragRep->fragId = replicaPtr.p->fragId;
      lcpFragRep->maxGciCompleted = replicaPtr.p->repMaxGciCompleted;
      lcpFragRep->maxGciStarted = replicaPtr.p->repMaxGciStarted;

      NodeReceiverGroup rg(DBDIH, c_lcpState.m_participatingDIH);
      sendSignal(rg, GSN_LCP_FRAG_REP, signal,
                 LcpFragRep::SignalLength, JBB);

      signal->theData[0] = DihContinueB::ZDEQUEUE_LCP_REP;
      sendSignal(reference(), GSN_CONTINUEB, signal,
               1, JBB);
      return;
    }
    else
    {
      /**
       * 1) Reset c_queued_lcp_complete_rep
       * 2) Set up LCP_COMPLETE_REP signal
       * 3) Send signals to all LCP DIH participants
       * 4) Fall through to end queue removal
       */
      ndbassert(c_queued_lcp_complete_rep);
      LcpCompleteRep *lcpCompleteRep =
        (LcpCompleteRep*)signal->getDataPtrSend();

      c_queued_lcp_complete_rep = false;

      lcpCompleteRep->nodeId = getOwnNodeId();
      lcpCompleteRep->lcpId = c_lcp_id_paused;
      lcpCompleteRep->blockNo = DBLQH;

      NodeReceiverGroup rg(DBDIH, c_lcpState.m_participatingDIH);
      sendSignal(rg, GSN_LCP_COMPLETE_REP, signal,
                 LcpCompleteRep::SignalLength, JBB);
    }
  }
  jam();
  /**
   * We have completed dequeueing all queued LCP reports. This means we can
   * reset the dequeue flag and resume normal operation of LCP reporting.
   */
  c_dequeue_lcp_rep_ongoing = false;
  c_lcp_id_paused = RNIL;
  ndbassert(check_pause_state_sanity());
}

/**
 * FLUSH_LCP_REP_CONF
 * ------------------
 * When we have received this signal from all nodes that participates in the
 * LCP, then we can send the PAUSE_LCP_CONF reply to the requester of the
 * pause (always requested by the master, we can only handle one pause at the
 * the time). We do however send along the starting node id in the signal
 * to ensure that we don't have to wait with the next start in the case of
 * a crash in the middle of the pausing.
 *
 * We will not be able to reach this point with the same node again and
 * still receive a signal from the previous time the node was alive since
 * the node start contains a number of messages from the master to all
 * nodes and thus ensuring that no outstanding messages are from a previous
 * node instance with the same node id. The same applies to a number of
 * similar scenarios in the NDB code.
 */
void Dbdih::execFLUSH_LCP_REP_CONF(Signal *signal)
{
  FlushLcpRepConf *conf = (FlushLcpRepConf*)&signal->theData[0];
  jamEntry();

  Uint32 nodeId = refToNode(conf->senderRef);
  Uint32 startNode = conf->startNodeId;

  if (!is_pause_for_this_node(startNode))
  {
    /* Ignore, node died in the process */
    jam();
    return;
  }

  receiveLoopMacro(FLUSH_LCP_REP_REQ, nodeId);
  {
    jam();
   /* Normal path, master is still alive */
    PauseLcpConf *conf = (PauseLcpConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->startNodeId = startNode;
    sendSignal(cmasterdihref, GSN_PAUSE_LCP_CONF, signal,
               PauseLcpConf::SignalLength, JBB);
  }
  ndbassert(check_pause_state_sanity());
}

/**
 * FLUSH_LCP_REP_REQ
 * -----------------
 * The only purpose of this signal is to ensure that we don't have any
 * outstanding LCP_FRAG_REP signals or other LCP signals. These signals
 * are sent from the node producing them to all other nodes. This means that
 * potentially they could be stuck for a long time in various send buffers
 * in the system. So a simple manner to ensure all of those signals have
 * reached their destination is to send FLUSH_LCP_REP_REQ from each node to
 * all other nodes. This gives a safe condition that we don't have any
 * outstanding LCP_FRAG_REP signals in the cluster. So there is no logic to
 * execute when receiving this signal other than to send it back to the sender.
 *
 * It is quite ok to receive this signal in a node before the PAUSE_LCP_REQ
 * has arrived here. This signal doesn't cause any interaction with the
 * pause handling in this node, actually it doesn't do anything. It's only
 * purpose is to ensure that the signal links are flushed such that we know
 * that we don't have any outstanding LCP_FRAG_REPs and LCP_COMPLETE_REPs.
 */
void Dbdih::execFLUSH_LCP_REP_REQ(Signal *signal)
{
  FlushLcpRepReq *req = (FlushLcpRepReq*)&signal->theData[0];
  FlushLcpRepConf *conf = (FlushLcpRepConf*)signal->getDataPtrSend();
  jamEntry();
  ndbassert(check_pause_state_sanity());

  BlockReference sender_ref = req->senderRef;
  Uint32 startNode = req->startNodeId;
  conf->senderRef = reference();
  conf->startNodeId = startNode;
  sendSignal(sender_ref, GSN_FLUSH_LCP_REP_CONF, signal,
             FlushLcpRepConf::SignalLength, JBB);
}
/*---------------------------------------------------------------------------*/
/* END Pausing LCP Module */
/*---------------------------------------------------------------------------*/


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
  
  if (getNodeInfo(Tnodeid).m_version >= NDBD_COPY_GCI_RESTART_NR)
  {
    jam();
    /**
     * COPY sysfile to starting node here directly
     *   so that it gets nodegroups early on
     */

    /**
     * Note: only one node can be starting now, so we can use
     *       c_nodeStartMaster.startNode for determining where to send
     */
    c_nodeStartMaster.m_outstandingGsn = GSN_COPY_GCIREQ;
    copyGciLab(signal, CopyGCIReq::RESTART_NR);
  }
  else
  {
    jam();
    startme_copygci_conf(signal);
  }
}

/**
 * We have come to a point in the node restart where we need to copy
 * the meta data to the starting node.
 * 
 * In older versions we did this by acquiring a mutex that is held by
 * the following actions:
 * 1) Execution of LCP. The mutex is held for the entire time we are
 *   executing an LCP. This could be all the way up to hours.
 * 
 * 2) Take over a fragment. This action happens in the phase where we
 *   are synchronizing the starting node with the alive nodes. In order
 *   to do so we need to lock the meta data in DBDIH to ensure that we
 *   can change it by adding one more alive replica.
 *
 * The new version still requires that no one is updating the meta data
 * while we are copying it. So this means that we still need to grab this
 * mutex to copy the meta data. But to synchronize our copying towards
 * the execution of LCPs we will use a pausing mechanism instead of
 * the mutex. This means that we can avoid the long wait for an LCP to
 * complete before we can copy the meta data.
 *
 * The take over of a fragment only updates the set of active replicas,
 * this will not be a problem to do in parallel with updating it with
 * regard to LCPs. So these need not be protected against each other.
 *
 * There are 3 processes that need protection for each other.
 * 1) The start of an LCP.
 * 2) The copying of meta data
 * 3) The synchronization of a node for a fragment
 *
 * 1) and 2) cannot run concurrently since we want to ensure that the
 * start of an LCP has a clear point in connection to the meta data
 * status.
 * 1) and 3) can run concurrently without any problems.
 *
 * 2) and 3) cannot run concurrently, but it would be possible to
 * have more fine-grained mutexes. The reason is that 3) changes
 * a replica from being an old stored replica to being a stored
 * replica. This change is part of the copying of meta data.
 *
 * 3) and 3) for different fragments could run concurrently, but this
 * would require changes of the protocol to synchronize the nodes to
 * to ensure that the master can handle several parallel changes of
 * replica status.
 *
 * 2) and 2) can run concurrently to some extent, but this would
 * require changes to the pause lcp protocol.
 *
 * The current implementation makes it possible to only run 1 out of
 * 1), 2) and 3) at a time.
 *
 * Another improvement possible is to speed up the copy meta data by
 * allowing the master to send more than one table at a time. This
 * would remove the wait state where we wait for the starting node
 * to receive a table and synchronize it to disk.
 *
 * One could also consider doing less synch's to disk if somehow the
 * different tables could be synched at the same time. This might
 * require changing the table layout on disk for DIH and DICT tables.
 */
void
Dbdih::startme_copygci_conf(Signal* signal)
{
  jam();

  /**
   * We update the node recovery status to indicate we are now waiting to
   * complete a local checkpoint such that we can keep track of node restart
   * status to control the start of local checkpoints in a proper manner.
   * This code is only executed in master nodes.
   */
  setNodeRecoveryStatus(c_nodeStartMaster.startNode,
                        NodeRecord::WAIT_LCP_TO_COPY_DICT);

  Callback c = { safe_cast(&Dbdih::lcpBlockedLab), 
                 c_nodeStartMaster.startNode };
  Mutex mutex(signal, c_mutexMgr, c_nodeStartMaster.m_fragmentInfoMutex);
  mutex.lock(c, true, true);
}

void Dbdih::lcpBlockedLab(Signal* signal, Uint32 nodeId, Uint32 retVal)
{
  jamEntry();
  if (c_nodeStartMaster.startNode != nodeId)
  {
    jam();
    if (retVal == 0 || retVal == UtilLockRef::InLockQueue)
    {
      infoEvent("Releasing table/fragment info lock for node %u", nodeId);
      
      Mutex mutex(signal, c_mutexMgr, c_nodeStartMaster.m_fragmentInfoMutex);
      mutex.unlock();
      return;
    }
    return;
  }

  if (retVal == UtilLockRef::InLockQueue)
  {
    jam();
    infoEvent("Node %u enqueued is waiting to copy table/fragment info",
              c_nodeStartMaster.startNode);
    return;
  }

  ndbrequire(retVal == 0); // Mutex error
  ndbrequire(getNodeStatus(c_nodeStartMaster.startNode)==NodeRecord::STARTING);

  if (c_lcp_runs_with_pause_support)
  {
    if (getNodeInfo(c_nodeStartMaster.startNode).m_version >=
        NDBD_SUPPORT_PAUSE_LCP)
    {
      /**
       * All nodes running the LCP supports the PAUSE LCP protocol. Also the
       * new node support it.
       * This means we don't have to wait for the LCP to complete, we can
       * pause the LCP while we are copying the meta data.
       */
      jam();
      sendPAUSE_LCP_REQ(signal, true);
      return;
    }
    else
    {
      jam();
      /**
       * We can only come here trying to start an old version with a master of
       * a new version. In this case we cannot use the PAUSE LCP protocol since
       * the new node can only handle copying of meta data outside the LCP
       * protocol.
       *
       * We come here holding the Fragment Info mutex. We will keep this mutex
       * and this means that a new LCP cannot start. We also set an indicator
       * to ensure that the LCP finish will know that we're waiting to copy
       * the data.
       */
      ndbrequire(!c_old_node_waiting_for_lcp_end);
      c_old_node_waiting_for_lcp_end = true;
      return;
    }
  }
  /**
   * Either we don't support the PAUSE protocol or some other node doesn't. We 
   * can also arrive here simply because no LCP is ongoing. In this case we
   * can be sure that no LCP is ongoing in both cases. So we ensure that no
   * LCP starts up until we have completed the copying of meta data by keeping
   * the Fragment Info mutex until we have completed the copying of meta data.
   */
  start_copy_meta_data(signal);
}//Dbdih::lcpBlockedLab()

void Dbdih::nodeDictStartConfLab(Signal* signal, Uint32 nodeId)
{
  /*-----------------------------------------------------------------*/
  // Report that node restart has completed copy of dictionary.
  /*-----------------------------------------------------------------*/
  signal->theData[0] = NDB_LE_NR_CopyDict;
  signal->theData[1] = nodeId;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);

  /*-------------------------------------------------------------------------
   * NOW WE HAVE COPIED BOTH DIH AND DICT INFORMATION. WE ARE NOW READY TO
   * INTEGRATE THE NODE INTO THE LCP AND GCP PROTOCOLS AND TO ALLOW UPDATES OF
   * THE DICTIONARY AGAIN.
   *
   * We can release the PAUSE on LCP now since we are ready to update the
   * meta data again.
   *
   * We update the node recovery status with this information to be able to
   * track node restart status. This code only executes in the master node.
   */
  /*-------------------------------------------------------------------------*/
  setNodeRecoveryStatus(c_nodeStartMaster.startNode,
                        NodeRecord::INCLUDE_NODE_IN_LCP_AND_GCP);

  c_nodeStartMaster.wait = ZFALSE;
  c_nodeStartMaster.blockGcp = 1;

  return;
}//Dbdih::nodeDictStartConfLab()

void Dbdih::dihCopyCompletedLab(Signal* signal)
{
  signal->theData[0] = NDB_LE_NR_CopyDistr;
  signal->theData[1] = c_nodeStartMaster.startNode;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);

  BlockReference ref = calcDictBlockRef(c_nodeStartMaster.startNode);
  DictStartReq * req = (DictStartReq*)&signal->theData[0];
  req->restartGci = (Uint32)(m_micro_gcp.m_new_gci >> 32);
  req->senderRef = reference();
  sendSignal(ref, GSN_DICTSTARTREQ,
             signal, DictStartReq::SignalLength, JBB);
  c_nodeStartMaster.m_outstandingGsn = GSN_DICTSTARTREQ;
  c_nodeStartMaster.wait = 0;
}//Dbdih::dihCopyCompletedLab()

void Dbdih::gcpBlockedLab(Signal* signal)
{
  /**
   * The node DIH will be part of LCP
   */
  NodeRecordPtr nodePtr;
  nodePtr.i = c_nodeStartMaster.startNode;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
  nodePtr.p->m_inclDihLcp = true;
  
  /**
   * If node is new...this is the place to do things,
   *   gcp+lcp is blocked
   */
  if (getNodeActiveStatus(nodePtr.i) == Sysfile::NS_NotDefined)
  {
    jam();
    infoEvent("Adding node %d to sysfile, NS_Configured",
              nodePtr.i);
    setNodeActiveStatus(nodePtr.i, Sysfile::NS_Configured);
    Sysfile::setNodeGroup(nodePtr.i, SYSFILE->nodeGroups,
                          NO_NODE_GROUP_ID);
    Sysfile::setNodeStatus(nodePtr.i,
                           SYSFILE->nodeStatus, Sysfile::NS_Configured);
  }

  /*-------------------------------------------------------------------------*/
  // NOW IT IS TIME TO INFORM ALL OTHER NODES IN THE CLUSTER OF THE STARTED
  // NODE SUCH THAT THEY ALSO INCLUDE THE NODE IN THE NODE LISTS AND SO FORTH.
  /*------------------------------------------------------------------------*/
  sendLoopMacro(INCL_NODEREQ, sendINCL_NODEREQ, RNIL);
  /*-------------------------------------------------------------------------*/
  // We also need to send to the starting node to ensure he is aware of the
  // global checkpoint id and the correct state. We do not wait for any reply
  // since the starting node will not send any.
  /*-------------------------------------------------------------------------*/
  Uint32 startVersion = getNodeInfo(c_nodeStartMaster.startNode).m_version;
  
  if ((getMajor(startVersion) == 4 && 
       startVersion >= NDBD_INCL_NODECONF_VERSION_4) ||
      (getMajor(startVersion) == 5 && 
       startVersion >= NDBD_INCL_NODECONF_VERSION_5) ||
      (getMajor(startVersion) > 5))
  {
    c_INCL_NODEREQ_Counter.setWaitingFor(c_nodeStartMaster.startNode);
  }
  
  sendINCL_NODEREQ(signal, c_nodeStartMaster.startNode, RNIL);
}//Dbdih::gcpBlockedLab()

/*---------------------------------------------------------------------------*/
// THIS SIGNAL IS EXECUTED IN BOTH SLAVES AND IN THE MASTER
/*---------------------------------------------------------------------------*/
void Dbdih::execINCL_NODECONF(Signal* signal) 
{
  jamEntry();
  Uint32 TstartNode = signal->theData[0];
  Uint32 TsendNodeId_or_blockref = signal->theData[1];

  Uint32 blocklist[7];
  blocklist[0] = clocallqhblockref;
  blocklist[1] = clocaltcblockref;
  blocklist[2] = cdictblockref;
  blocklist[3] = numberToRef(BACKUP, getOwnNodeId());
  blocklist[4] = numberToRef(SUMA, getOwnNodeId());
  blocklist[5] = numberToRef(DBSPJ, getOwnNodeId());
  blocklist[6] = 0;
  
  for (Uint32 i = 0; blocklist[i] != 0; i++)
  {
    if (TsendNodeId_or_blockref == blocklist[i])
    {
      jam();

      if (TstartNode != c_nodeStartSlave.nodeId)
      {
        jam();
        warningEvent("Received INCL_NODECONF for %u from %s"
                     " while %u is starting",
                     TstartNode,
                     getBlockName(refToBlock(TsendNodeId_or_blockref)),
                     c_nodeStartSlave.nodeId);
        return;
      }
      
      if (getNodeStatus(c_nodeStartSlave.nodeId) == NodeRecord::ALIVE && 
	  blocklist[i+1] != 0)
      {
	/**
	 * Send to next in block list
	 */
	jam();
	signal->theData[0] = reference();
	signal->theData[1] = c_nodeStartSlave.nodeId;
	sendSignal(blocklist[i+1], GSN_INCL_NODEREQ, signal, 2, JBB);
	return;
      }
      else
      {
	/**
	 * All done, reply to master
	 */
	jam();
        if (!isMaster())
        {
          jam();
          setNodeRecoveryStatus(c_nodeStartSlave.nodeId,
                                NodeRecord::NODE_GETTING_INCLUDED);
        }
	signal->theData[0] = c_nodeStartSlave.nodeId;
	signal->theData[1] = cownNodeId;
	sendSignal(cmasterdihref, GSN_INCL_NODECONF, signal, 2, JBB);
	
	c_nodeStartSlave.nodeId = 0;
	return;
      }
    }
  }

  if (c_nodeStartMaster.startNode != TstartNode)
  {
    jam();
    warningEvent("Received INCL_NODECONF for %u from %u"
                 " while %u is starting",
                 TstartNode,
                 TsendNodeId_or_blockref,
                 c_nodeStartMaster.startNode);
    return;
  }
  
  ndbrequire(reference() == cmasterdihref);
  receiveLoopMacro(INCL_NODEREQ, TsendNodeId_or_blockref);
  
  CRASH_INSERTION(7128);
  /*-------------------------------------------------------------------------*/
  // Now that we have included the starting node in the node lists in the
  // various blocks we are ready to start the global checkpoint protocol
  /*------------------------------------------------------------------------*/
  c_nodeStartMaster.wait = 11;
  c_nodeStartMaster.blockGcp = 0;

  /**
   * Restart GCP
   */
  signal->theData[0] = reference();
  sendSignal(reference(), GSN_UNBLO_DICTCONF, signal, 1, JBB);

  signal->theData[0] = DihContinueB::ZSTART_GCP;
  sendSignal(reference(), GSN_CONTINUEB, signal, 1, JBB);

  Mutex mutex(signal, c_mutexMgr, c_nodeStartMaster.m_fragmentInfoMutex);
  mutex.unlock();
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
  
  Uint32 nodeId = startMe->startingNodeId = c_nodeStartMaster.startNode;
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
  nodeResetStart(signal);

  /**
   * At this point the master knows that the starting node will start executing
   * the Database Recovery. This can take a fair amount of time. At the end of
   * the recovery the starting node need to be part of a LCP. In order to
   * synchronize for several nodes restarting at the same time we need to keep
   * track of start times.
   *
   * We expect that in most parallel node restarts the nodes are restarted
   * immediately after a crash or as part of a rolling restart. In this case
   * the node restart times will be very similar. So we should be able to
   * roughly estimate when the node restart will reach the point where it
   * is ready to wait for an LCP.
   *
   * When the first node reaches this point and also later nodes reach this
   * phase, then they will be able to estimate whether it is worth it to
   * hold the LCP until the next node arrives to this phase.
   *
   * The similitude of a flight or a train waiting for passengers arriving
   * on other flights or trains can be used here. It is useful to wait for
   * some time since there is a high cost for passengers to miss the train.
   * At the same time it isn't worthwhile to hold it for a very long time
   * since then all other passengers will suffer greatly. In this case the
   * other nodes waiting will suffer, but also we will risk running out of
   * REDO log space if we wait for too long time.
   *
   * Given that we don't wait for more than a short time to synchronize
   * means that the case of heterogenous nodes will also work ok in this
   * context although we will optimize for the homogenous case.
   *
   * To get even better estimates of where we are and to give users even
   * better understanding of what takes time in node restarts we have also
   * adde that the LDMs report when they have completed the 3 local phases
   * of local recovery. These are completion of restore fragments,
   * completion of UNDO Disk data, completion of execution of REDO log and
   * the final phase executed in LDMs are the ordered index rebuilds which is
   * completed when the local recovery is completed.
   */
  setNodeRecoveryStatus(nodeId, NodeRecord::LOCAL_RECOVERY_STARTED);

  /**
   * Allow next node to start...
   */
  signal->theData[0] = nodeId;
  sendSignal(NDBCNTR_REF, GSN_START_PERMREP, signal, 1, JBB);
}//Dbdih::execUNBLO_DICTCONF()

/*---------------------------------------------------------------------------*/
/*                    NODE RESTART COPY REQUEST                              */
/*---------------------------------------------------------------------------*/
// A NODE RESTART HAS REACHED ITS FINAL PHASE WHEN THE DATA IS TO BE COPIED
// TO THE NODE. START_COPYREQ IS EXECUTED BY THE STARTING NODE.
/*---------------------------------------------------------------------------*/
void Dbdih::execSTART_COPYREQ(Signal* signal) 
{
  jamEntry();
  StartCopyReq req = *(StartCopyReq*)signal->getDataPtr();

  Uint32 startNodeId = req.startingNodeId;

  /*-------------------------------------------------------------------------*/
  /*
   * REPORT Copy process of node restart is now about to start up.
   * 
   * We will report this both in an internal state that can be used to
   * report progress in NDBINFO tables as well as being used to keep track of
   * node restart status to make correct decisions on when to start LCPs.
   * We also report it to cluster log and internal node log.
   *
   * This code is only executed in master node.
   */
  /*-------------------------------------------------------------------------*/
  signal->theData[0] = NDB_LE_NR_CopyFragsStarted;
  signal->theData[1] = req.startingNodeId;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);

  g_eventLogger->info("Restore Database Off-line Starting"); 
  infoEvent("Restore Database Off-line Starting on node %u", 
            startNodeId); 

  CRASH_INSERTION(7131);

  switch (getNodeActiveStatus(startNodeId)) {
  case Sysfile::NS_Active:
  case Sysfile::NS_ActiveMissed_1:
  case Sysfile::NS_ActiveMissed_2:
  case Sysfile::NS_NotActive_NotTakenOver:
  case Sysfile::NS_Configured:
    jam();
    /*-----------------------------------------------------------------------*/
    // AN ACTIVE NODE HAS BEEN STARTED. THE ACTIVE NODE MUST THEN GET ALL DATA
    // IT HAD BEFORE ITS CRASH. WE START THE TAKE OVER IMMEDIATELY. 
    // SINCE WE ARE AN ACTIVE NODE WE WILL TAKE OVER OUR OWN NODE THAT 
    // PREVIOUSLY CRASHED.
    /*-----------------------------------------------------------------------*/
    startTakeOver(signal, startNodeId, startNodeId, &req);
    break;
  case Sysfile::NS_TakeOver:{
    jam();
    /*--------------------------------------------------------------------
     * We were in the process of taking over but it was not completed.
     * We will complete it now instead.
     *--------------------------------------------------------------------*/
    Uint32 takeOverNode = Sysfile::getTakeOverNode(startNodeId, 
						   SYSFILE->takeOver);
    if(takeOverNode == 0){
      jam();
      warningEvent("Bug in take-over code restarting");
      takeOverNode = startNodeId;
    }

    startTakeOver(signal, startNodeId, takeOverNode, &req);
    break;
  }
  default:
    ndbrequire(false);
    break;
  }//switch
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
    if (getNodeStatus(startNode) == NodeRecord::STARTING)
    {
      /**
       * The master is sending out a new START_INFOREQ, obviously some
       * other node wasn't ready to start it yet, we are still ready.
       * We will report this fact without any additional state changes.
       */
      jam();
      NodeRecordPtr nodePtr;
      nodePtr.i = startNode;
      ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
      ndbrequire(nodePtr.p->nodeRecoveryStatus ==
                 NodeRecord::NODE_GETTING_PERMIT);
      ndbrequire(getAllowNodeStart(startNode));

      StartInfoConf * c = (StartInfoConf*)&signal->theData[0];
      c->sendingNodeId = cownNodeId;
      c->startingNodeId = startNode;
      sendSignal(cmasterdihref, GSN_START_INFOCONF, signal,
	         StartInfoConf::SignalLength, JBB);
      return;
    }
    else
    {
      jam();
      ndbrequire(getNodeStatus(startNode) == NodeRecord::DEAD);
    }
  }//if
  if ((!getAllowNodeStart(startNode)) ||
      (c_nodeStartSlave.nodeId != 0) ||
      (ERROR_INSERTED(7124))) {
    jam();
    if (!getAllowNodeStart(startNode))
    {
      jam();
      g_eventLogger->info("Not allowed to start now for node %u", startNode);
    }
    else if (c_nodeStartSlave.nodeId != 0)
    {
      jam();
      g_eventLogger->info("INCL_NODEREQ protocol still ongoing node = %u"
                          " c_nodeStartSlave.nodeId = %u",
                          startNode,
                          c_nodeStartSlave.nodeId);
    }
    else
    {
      jam();
      g_eventLogger->info("ERROR INSERT 7124");
    }
    StartInfoRef *const ref =(StartInfoRef*)&signal->theData[0];
    ref->startingNodeId = startNode;
    ref->sendingNodeId = cownNodeId;
    ref->errorCode = StartPermRef::ZNODE_START_DISALLOWED_ERROR;
    sendSignal(cmasterdihref, GSN_START_INFOREF, signal, 
	       StartInfoRef::SignalLength, JBB);
    return;
  }//if
  setNodeStatus(startNode, NodeRecord::STARTING);
  if (req->typeStart == NodeState::ST_INITIAL_NODE_RESTART) {
    jam();
    g_eventLogger->info("Started invalidation of node %u", startNode);
    setAllowNodeStart(startNode, false);
    invalidateNodeLCP(signal, startNode, 0);
  } else {
    jam();
    if (!isMaster())
    {
      jam();
      setNodeRecoveryStatus(startNode, NodeRecord::NODE_GETTING_PERMIT);
    }
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
  if (nodeId == getOwnNodeId() && ERROR_INSERTED(7165))
  {
    CLEAR_ERROR_INSERT_VALUE;
    sendSignalWithDelay(reference(), GSN_INCL_NODEREQ, signal, 5000, 
                        signal->getLength());
    return;
  }
  
  Uint32 tnodeStartFailNr = signal->theData[2];
  Uint32 gci_hi = signal->theData[4];
  Uint32 gci_lo = signal->theData[5];
  if (unlikely(signal->getLength() < 6))
  {
    jam();
    gci_lo = 0;
  }
  
  Uint64 gci = gci_lo | (Uint64(gci_hi) << 32);
  CRASH_INSERTION(7127);
  m_micro_gcp.m_current_gci = gci;
  m_micro_gcp.m_old_gci = gci - 1;

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
    Uint32 masterVersion = getNodeInfo(refToNode(cmasterdihref)).m_version;
    
    if ((NDB_VERSION_MAJOR == 4 && 
	 masterVersion >= NDBD_INCL_NODECONF_VERSION_4) ||
	(NDB_VERSION_MAJOR == 5 && 
	 masterVersion >= NDBD_INCL_NODECONF_VERSION_5) ||
	(NDB_VERSION_MAJOR > 5))
    {
      signal->theData[0] = getOwnNodeId();
      signal->theData[1] = getOwnNodeId();
      sendSignal(cmasterdihref, GSN_INCL_NODECONF, signal, 2, JBB);
    }
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

  initNodeRecord(nodePtr);
  nodePtr.p->nodeGroup = TnodeGroup;
  nodePtr.p->activeStatus = TsaveState;
  nodePtr.p->nodeStatus = NodeRecord::ALIVE;
  nodePtr.p->m_inclDihLcp = true;
  make_node_usable(nodePtr.p);
  removeDeadNode(nodePtr);
  insertAlive(nodePtr);
  con_lineNodes++;

  /*-------------------------------------------------------------------------*/
  //      WE WILL ALSO SEND THE INCLUDE NODE REQUEST TO THE LOCAL LQH BLOCK.
  /*-------------------------------------------------------------------------*/
  signal->theData[0] = reference();
  signal->theData[1] = nodeId;
  signal->theData[2] = Uint32(m_micro_gcp.m_current_gci >> 32);
  sendSignal(clocallqhblockref, GSN_INCL_NODEREQ, signal, 3, JBB);
}//Dbdih::execINCL_NODEREQ()

/* ------------------------------------------------------------------------- */
// execINCL_NODECONF() is found in the master logic part since it is used by
// both the master and the slaves.
/* ------------------------------------------------------------------------- */

/******************************************************************************
 *
 * Node takeover functionality
 * MASTER part
 *****************************************************************************/
void Dbdih::execSTART_TOREQ(Signal* signal) 
{
  jamEntry();
  StartToReq req = *(StartToReq *)&signal->theData[0];
  

  if (ndb_pnr(getNodeInfo(refToNode(req.senderRef)).m_version))
  {
    jam();
    TakeOverRecordPtr takeOverPtr;
    
    c_takeOverPool.seize(takeOverPtr);
    c_masterActiveTakeOverList.addFirst(takeOverPtr);
    takeOverPtr.p->toStartingNode = req.startingNodeId;
    takeOverPtr.p->m_senderRef = req.senderRef;
    takeOverPtr.p->m_senderData = req.senderData;
    takeOverPtr.p->toMasterStatus = TakeOverRecord::TO_MASTER_IDLE;
    takeOverPtr.p->toStartTime = c_current_time;
  }
  
  setNodeRecoveryStatus(req.startingNodeId,
                        NodeRecord::COPY_FRAGMENTS_STARTED);

  StartToConf * conf = (StartToConf *)&signal->theData[0];
  conf->senderData = req.senderData;
  conf->sendingNodeId = cownNodeId;
  conf->startingNodeId = req.startingNodeId;
  sendSignal(req.senderRef, GSN_START_TOCONF, 
             signal, StartToConf::SignalLength, JBB);
}//Dbdih::execSTART_TOREQ()

void Dbdih::execUPDATE_TOREQ(Signal* signal)
{
  jamEntry();
  UpdateToReq req = *(UpdateToReq *)&signal->theData[0];

  Uint32 errCode;
  Uint32 extra;
  g_eventLogger->debug("Received UPDATE_TOREQ for startnode: %u, copynode:%u",
                       req.startingNodeId, req.copyNodeId);
  if (ndb_pnr(getNodeInfo(refToNode(req.senderRef)).m_version))
  {
    jam();
    /**
     * 
     */
    TakeOverRecordPtr takeOverPtr;
    if (findTakeOver(takeOverPtr, req.startingNodeId) == false)
    {
      g_eventLogger->info("Unknown takeOver node: %u", req.startingNodeId);
      errCode = UpdateToRef::UnknownTakeOver;
      extra = RNIL;
      goto ref;
    }
    
    CRASH_INSERTION(7141);
    
    takeOverPtr.p->toCopyNode = req.copyNodeId;
    takeOverPtr.p->toCurrentTabref = req.tableId;
    takeOverPtr.p->toCurrentFragid = req.fragmentNo;

    NodeRecordPtr nodePtr;
    NodeGroupRecordPtr NGPtr;
    nodePtr.i = req.copyNodeId;
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
    NGPtr.i = nodePtr.p->nodeGroup;
    ptrCheckGuard(NGPtr, MAX_NDB_NODE_GROUPS, nodeGroupRecord);
    
    Mutex mutex(signal, c_mutexMgr, takeOverPtr.p->m_fragmentInfoMutex);
    Callback c = { safe_cast(&Dbdih::updateToReq_fragmentMutex_locked), 
                   takeOverPtr.i };
    
    switch(req.requestType){
    case UpdateToReq::BEFORE_STORED:
      jam();

      if (NGPtr.p->activeTakeOver == 0)
      {
        jam();
        NGPtr.p->activeTakeOver = req.startingNodeId;
        NGPtr.p->activeTakeOverCount = 1;
      }
      else if (NGPtr.p->activeTakeOver == req.startingNodeId)
      {
        NGPtr.p->activeTakeOverCount++;
      }
      else
      {
        jam();
        errCode = UpdateToRef::CopyFragInProgress;
        extra = NGPtr.p->activeTakeOver;
        g_eventLogger->info("takeOver node in progress: %u",
                            NGPtr.p->activeTakeOver);
        goto ref;
      }

      takeOverPtr.p->toMasterStatus = TakeOverRecord::TO_MUTEX_BEFORE_STORED;
      mutex.lock(c, false, true);
      return;
    case UpdateToReq::AFTER_STORED:
    {
      jam();
      mutex.unlock();
      takeOverPtr.p->toMasterStatus = TakeOverRecord::TO_AFTER_STORED;
      // Send conf
      break; 
    }
    case UpdateToReq::BEFORE_COMMIT_STORED:
      jam();
      takeOverPtr.p->toMasterStatus = TakeOverRecord::TO_MUTEX_BEFORE_COMMIT;
      mutex.lock(c, false, true);
      return;
    case UpdateToReq::AFTER_COMMIT_STORED:
    {
      jam();
      mutex.unlock();
      
      Mutex mutex2(signal, c_mutexMgr, 
                   takeOverPtr.p->m_switchPrimaryMutexHandle);
      mutex2.unlock();
      takeOverPtr.p->toMasterStatus = TakeOverRecord::TO_MASTER_IDLE;      
      break; // send conf
    }
    }
  }
  else
  {
    CRASH_INSERTION(7154);
    RETURN_IF_NODE_NOT_ALIVE(req.startingNodeId);
  }
  
  {
    UpdateToConf * conf = (UpdateToConf *)&signal->theData[0];
    conf->senderData = req.senderData;
    conf->sendingNodeId = cownNodeId;
    conf->startingNodeId = req.startingNodeId;
    sendSignal(req.senderRef, GSN_UPDATE_TOCONF, signal, 
               UpdateToConf::SignalLength, JBB);
  }
  return;

ref:
  UpdateToRef* ref = (UpdateToRef*)signal->getDataPtrSend();
  ref->senderData = req.senderData;
  ref->senderRef = reference();
  ref->errorCode = errCode;
  ref->extra = extra;
  sendSignal(req.senderRef, GSN_UPDATE_TOREF, signal,
             UpdateToRef::SignalLength, JBB);
}

void
Dbdih::updateToReq_fragmentMutex_locked(Signal * signal, 
                                        Uint32 toPtrI, Uint32 retVal)
{
  jamEntry();
  TakeOverRecordPtr takeOverPtr;
  c_takeOverPool.getPtr(takeOverPtr, toPtrI);
  
  Uint32 nodeId = takeOverPtr.p->toStartingNode;

  if (retVal == UtilLockRef::InLockQueue)
  {
    jam();
    infoEvent("Node %u waiting to continue copying table %u fragment: %u (%s)",
              nodeId,
              takeOverPtr.p->toCurrentTabref,
              takeOverPtr.p->toCurrentFragid,
              takeOverPtr.p->toMasterStatus ==
                TakeOverRecord::TO_MUTEX_BEFORE_STORED ? "STORED" : "COMMIT");
    return;
  }

  Uint32 errCode;
  Uint32 extra;
  
  NodeRecordPtr nodePtr;
  nodePtr.i = nodeId;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
  if (unlikely(nodePtr.p->nodeStatus != NodeRecord::ALIVE))
  {
    jam();
    /**
     * Node died while we waited for lock...
     */
    abortTakeOver(signal, takeOverPtr);
    return;
  }

  switch(takeOverPtr.p->toMasterStatus){
  case TakeOverRecord::TO_MUTEX_BEFORE_STORED:
  {
    jam();
    // send conf
    takeOverPtr.p->toMasterStatus = TakeOverRecord::TO_MUTEX_BEFORE_LOCKED;
    break; 
  }
  case TakeOverRecord::TO_MUTEX_BEFORE_COMMIT:
  {
    jam();

    NodeRecordPtr nodePtr;
    NodeGroupRecordPtr NGPtr;
    nodePtr.i = takeOverPtr.p->toCopyNode;
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
    NGPtr.i = nodePtr.p->nodeGroup;
    ptrCheckGuard(NGPtr, MAX_NDB_NODE_GROUPS, nodeGroupRecord);
    
    if (NGPtr.p->activeTakeOver != nodeId)
    {
      ndbassert(false);
      errCode = UpdateToRef::InvalidRequest;
      extra = NGPtr.p->activeTakeOver;
      goto ref;
    }
    ndbrequire(NGPtr.p->activeTakeOverCount > 0);
    NGPtr.p->activeTakeOverCount--;
    if (NGPtr.p->activeTakeOverCount == 0)
    {
      /**
       * Last active copy thread, give up activeTakeOver for now
       */
      jam();
      NGPtr.p->activeTakeOver = 0;
    }
    takeOverPtr.p->toCopyNode = RNIL;
    Mutex mutex(signal, c_mutexMgr, 
                takeOverPtr.p->m_switchPrimaryMutexHandle);
    Callback c = { safe_cast(&Dbdih::switchPrimaryMutex_locked), 
                   takeOverPtr.i };
    ndbrequire(mutex.lock(c));
    takeOverPtr.p->toMasterStatus = TakeOverRecord::TO_MUTEX_BEFORE_SWITCH_REPLICA;
    return;
    break;
  }
  default:
    jamLine(takeOverPtr.p->toMasterStatus);
    ndbrequire(false);
  }
  
  {
    UpdateToConf * conf = (UpdateToConf *)&signal->theData[0];
    conf->senderData = takeOverPtr.p->m_senderData;
    conf->sendingNodeId = cownNodeId;
    conf->startingNodeId = takeOverPtr.p->toStartingNode;
    sendSignal(takeOverPtr.p->m_senderRef, GSN_UPDATE_TOCONF, signal, 
               UpdateToConf::SignalLength, JBB);
  }
  return;

ref:
  {
    Mutex mutex(signal, c_mutexMgr, takeOverPtr.p->m_fragmentInfoMutex);
    mutex.unlock();
    
    UpdateToRef* ref = (UpdateToRef*)signal->getDataPtrSend();
    ref->senderData = takeOverPtr.p->m_senderData;
    ref->senderRef = reference();
    ref->errorCode = errCode;
    ref->extra = extra;
    sendSignal(takeOverPtr.p->m_senderRef, GSN_UPDATE_TOREF, signal,
               UpdateToRef::SignalLength, JBB);
    return;
  }
}

void
Dbdih::switchPrimaryMutex_locked(Signal* signal, Uint32 toPtrI, Uint32 retVal)
{
  jamEntry();
  ndbrequire(retVal == 0);
  
  TakeOverRecordPtr takeOverPtr;
  c_takeOverPool.getPtr(takeOverPtr, toPtrI);

  Uint32 nodeId = takeOverPtr.p->toStartingNode;
  NodeRecordPtr nodePtr;
  nodePtr.i = nodeId;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);

  if (unlikely(nodePtr.p->nodeStatus != NodeRecord::ALIVE))
  {
    jam();
    /**
     * Node died while we waited for lock...
     */
    abortTakeOver(signal, takeOverPtr);
    return;
  }

  takeOverPtr.p->toMasterStatus = TakeOverRecord::TO_MUTEX_AFTER_SWITCH_REPLICA;

  UpdateToConf * conf = (UpdateToConf *)&signal->theData[0];
  conf->senderData = takeOverPtr.p->m_senderData;
  conf->sendingNodeId = cownNodeId;
  conf->startingNodeId = takeOverPtr.p->toStartingNode;
  sendSignal(takeOverPtr.p->m_senderRef, GSN_UPDATE_TOCONF, signal, 
             UpdateToConf::SignalLength, JBB);
}

void
Dbdih::switchPrimaryMutex_unlocked(Signal* signal, Uint32 toPtrI, Uint32 retVal)
{
  jamEntry();
  ndbrequire(retVal == 0);
  
  TakeOverRecordPtr takeOverPtr;
  c_takeOverPool.getPtr(takeOverPtr, toPtrI);

  UpdateToConf * conf = (UpdateToConf *)&signal->theData[0];
  conf->senderData = takeOverPtr.p->m_senderData;
  conf->sendingNodeId = cownNodeId;
  conf->startingNodeId = takeOverPtr.p->toStartingNode;
  sendSignal(takeOverPtr.p->m_senderRef, GSN_UPDATE_TOCONF, signal, 
             UpdateToConf::SignalLength, JBB);
}

void
Dbdih::abortTakeOver(Signal* signal, TakeOverRecordPtr takeOverPtr)
{
  if (!takeOverPtr.p->m_switchPrimaryMutexHandle.isNull())
  {
    jam();
    Mutex mutex(signal, c_mutexMgr, 
                takeOverPtr.p->m_switchPrimaryMutexHandle);
    mutex.unlock();

  }
  
  if (!takeOverPtr.p->m_fragmentInfoMutex.isNull())
  {
    jam();
    Mutex mutex(signal, c_mutexMgr, 
                takeOverPtr.p->m_fragmentInfoMutex);
    mutex.unlock();
  }
  
  NodeRecordPtr nodePtr;
  nodePtr.i = takeOverPtr.p->toCopyNode;
  if (nodePtr.i != RNIL)
  {
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
    NodeGroupRecordPtr NGPtr;
    NGPtr.i = nodePtr.p->nodeGroup;
    ptrCheckGuard(NGPtr, MAX_NDB_NODE_GROUPS, nodeGroupRecord);
    if (NGPtr.p->activeTakeOver == takeOverPtr.p->toStartingNode)
    {
      jam();
      NGPtr.p->activeTakeOver = 0;
      NGPtr.p->activeTakeOverCount = 0;
    }
  }
  
  releaseTakeOver(takeOverPtr, true);
}

static 
void 
add_lcp_counter(Uint32 * counter, Uint32 add)
{
  Uint64 tmp = * counter;
  tmp += add;
  if (tmp > 0xFFFFFFFF)
    tmp = 0xFFFFFFFF;
  * counter = Uint32(tmp);
}

void Dbdih::execEND_TOREQ(Signal* signal)
{
  jamEntry();
  EndToReq req = *(EndToReq *)&signal->theData[0];

  Uint32 nodeId = refToNode(req.senderRef);
  TakeOverRecordPtr takeOverPtr;

  if (ndb_pnr(getNodeInfo(nodeId).m_version))
  {
    jam();
    /**
     * 
     */
    ndbrequire(findTakeOver(takeOverPtr, nodeId));
    NodeRecordPtr nodePtr;
    nodePtr.i = nodeId;
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);

    if (req.flags & StartCopyReq::WAIT_LCP)
    {
      /**
       * Wait for LCP
       */
      Uint32 latestLCP_ID = SYSFILE->latestLCP_ID;
      switch (c_lcpState.lcpStatus)
      {
        case LCP_STATUS_IDLE:
        case LCP_WAIT_MUTEX:
        case LCP_TCGET:
        case LCP_TC_CLOPSIZE:
          /**
           * We haven't started the next LCP yet, we haven't assigned the
           * nodes to participate in this LCP, so we will wait for the next
           * LCP started.
           */
         jam();
         latestLCP_ID++;
         break;
       default:
         /**
          * All the remaining status codes means that the LCP has been started
          * and that the participating nodes have been set. So if our node is
          * part of the participating nodes we will wait for this LCP,
          * otherwise we will wait for the next LCP to start.
          */
         jam();
         if (!c_lcpState.m_participatingLQH.get(nodeId))
         {
           jam();
           latestLCP_ID++;
         }
         break;
      }
      infoEvent("Make On-line Database recoverable by waiting"
                " for LCP Starting on node %u, LCP id %u",
                nodeId,
                latestLCP_ID);

      nodePtr.p->copyCompleted = 2;
      takeOverPtr.p->toMasterStatus = TakeOverRecord::TO_WAIT_LCP;

      /**
       * Make sure that node also participated in one GCP
       *   before running it's first LCP, so that GCI variables
       *   in LQH are set properly
       */
      c_lcpState.lcpStopGcp = c_newest_restorable_gci;

      /**
       * We want to keep track of how long time we wait for LCP to be able
       * to present it in an ndbinfo table. This information is also used
       * in deciding when to start LCPs.
       *
       * We ensure that we will not stall any LCPs in this state due to not
       * having had enough activity. We can still stall due to waiting for
       * other nodes to reach this state.
       */
      add_lcp_counter(&c_lcpState.ctimer, (1 << 31));
      setNodeRecoveryStatus(nodePtr.i, NodeRecord::WAIT_LCP_FOR_RESTART);
      return;
    }
    nodePtr.p->copyCompleted = 1;
    releaseTakeOver(takeOverPtr, true);
  }
  
  EndToConf * conf = (EndToConf *)&signal->theData[0];
  conf->senderData = req.senderData;
  conf->sendingNodeId = cownNodeId;
  conf->startingNodeId = req.startingNodeId;
  sendSignal(req.senderRef, GSN_END_TOCONF, signal, 
             EndToConf::SignalLength, JBB);
}//Dbdih::execEND_TOREQ()

/* --------------------------------------------------------------------------*/
/*       AN ORDER TO START OR COMMIT THE REPLICA CREATION ARRIVED FROM THE   */
/*       MASTER.                                                             */
/* --------------------------------------------------------------------------*/
void Dbdih::execUPDATE_FRAG_STATEREQ(Signal* signal) 
{
  jamEntry();
  UpdateFragStateReq * const req = (UpdateFragStateReq *)&signal->theData[0];

  Uint32 senderData = req->senderData;
  Uint32 senderRef = req->senderRef;

  TabRecordPtr tabPtr;
  tabPtr.i = req->tableId;
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);

  Uint32 fragId = req->fragId;
  Uint32 tdestNodeid = req->startingNodeId;
  //Uint32 tsourceNodeid = req->copyNodeId;
  Uint32 startGci = req->startGci;
  Uint32 replicaType = req->replicaType;
  Uint32 tFailedNodeId = req->failedNodeId;

  if (!ndb_pnr(getNodeInfo(refToNode(senderRef)).m_version))
  {
    jam();
    tFailedNodeId = tdestNodeid;
  }
  
  FragmentstorePtr fragPtr;
  getFragstore(tabPtr.p, fragId, fragPtr);
  RETURN_IF_NODE_NOT_ALIVE(tdestNodeid);
  ReplicaRecordPtr frReplicaPtr;
  findReplica(frReplicaPtr, fragPtr.p, tFailedNodeId,
              replicaType == UpdateFragStateReq::START_LOGGING ? false : true);
  if (frReplicaPtr.i == RNIL)
  {
    dump_replica_info(fragPtr.p);
  }
  ndbrequire(frReplicaPtr.i != RNIL);

  make_table_use_new_replica(tabPtr,
                             fragPtr,
                             frReplicaPtr,
                             replicaType,
                             tdestNodeid);

  /* ------------------------------------------------------------------------*/
  /*       THE NEW NODE OF THIS REPLICA IS THE STARTING NODE.                */
  /* ------------------------------------------------------------------------*/
  if (tFailedNodeId != tdestNodeid)
  {
    jam();
    /**
     * This is a Hot-spare or move partition
     */
    
    /*  IF WE ARE STARTING A TAKE OVER NODE WE MUST INVALIDATE ALL LCP'S.   */
    /*  OTHERWISE WE WILL TRY TO START LCP'S THAT DO NOT EXIST.             */
    /* ---------------------------------------------------------------------*/
    frReplicaPtr.p->procNode = tdestNodeid;
    frReplicaPtr.p->noCrashedReplicas = 0;
    frReplicaPtr.p->createGci[0] = startGci;
    frReplicaPtr.p->replicaLastGci[0] = (Uint32)-1;
    for (Uint32 i = 0; i < MAX_LCP_STORED; i++) 
    {
      frReplicaPtr.p->lcpStatus[i] = ZINVALID;
    }
  } 
  else 
  {
    jam();
    const Uint32 noCrashed = frReplicaPtr.p->noCrashedReplicas;
    arrGuard(noCrashed, 8);
    frReplicaPtr.p->createGci[noCrashed] = startGci;
    frReplicaPtr.p->replicaLastGci[noCrashed] = (Uint32)-1;
  }

  if (!isMaster())
  {
    jam();
    NodeRecordPtr nodePtr;
    nodePtr.i = tdestNodeid;
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
    if (nodePtr.p->nodeRecoveryStatus != NodeRecord::NODE_GETTING_SYNCHED)
    {
      jam();
      /**
       * We come here many times, we will call the state transition
       * code only the first time.
       */
      setNodeRecoveryStatus(tdestNodeid, NodeRecord::NODE_GETTING_SYNCHED);
    }
  }
  UpdateFragStateConf * const conf =
    (UpdateFragStateConf *)&signal->theData[0];
  conf->senderData = senderData;
  conf->tableId = tabPtr.i;
  conf->fragId = fragId;
  conf->sendingNodeId = cownNodeId;
  conf->startingNodeId = tdestNodeid;
  conf->failedNodeId = tFailedNodeId;
  sendSignal(senderRef, GSN_UPDATE_FRAG_STATECONF, signal,
             UpdateFragStateConf::SignalLength, JBB);
}//Dbdih::execUPDATE_FRAG_STATEREQ()

/**
 * Node Recovery Status Module
 * ---------------------------
 * This module is used to keep track of the restart progress in the master node
 * and also to report it to the user through a NDBINFO table. The module is
 * also used to estimate when a restart reaches certain critical checkpoints
 * in the restart execution. This is used to ensure that we hold up start of
 * those critical parts (e.g. LCPs) if there is a good chance that we will
 * reach there in reasonable time. Same principal as holding a train waiting
 * for a batch of important customers. One can wait for a while, but not
 * for too long time since this will affect many others as well.
 *
 * The only actions that are reported here happen in the master node. The only
 * exception to this is the node failure and node failure completed events
 * that happens in all nodes. Since the master node is the node that was
 * started first of all nodes, this means that the master node will contain
 * information about the node restarts of all nodes except those that
 * was started at the same time as the master node.
 */

/* Debug Node Recovery Status module */
#define DBG_NRS(a)
//#define DBG_NRS(a) ndbout << a << endl

void Dbdih::initNodeRecoveryStatus()
{
  NodeRecordPtr nodePtr;

  jam();
  for (nodePtr.i = 0; nodePtr.i < MAX_NDB_NODES; nodePtr.i++)
  {
    ptrAss(nodePtr, nodeRecord);
    nodePtr.p->nodeRecoveryStatus = NodeRecord::NOT_DEFINED_IN_CLUSTER;
    nodePtr.p->is_pausable = false;
    initNodeRecoveryTimers(nodePtr);
  }
}

void Dbdih::initNodeRecoveryTimers(NodeRecordPtr nodePtr)
{
  jam();
  NdbTick_Invalidate(&nodePtr.p->nodeFailTime);
  NdbTick_Invalidate(&nodePtr.p->nodeFailCompletedTime);
  NdbTick_Invalidate(&nodePtr.p->allocatedNodeIdTime);
  NdbTick_Invalidate(&nodePtr.p->includedInHBProtocolTime);
  NdbTick_Invalidate(&nodePtr.p->ndbcntrStartWaitTime);
  NdbTick_Invalidate(&nodePtr.p->ndbcntrStartedTime);
  NdbTick_Invalidate(&nodePtr.p->startPermittedTime);
  NdbTick_Invalidate(&nodePtr.p->waitLCPToCopyDictTime);
  NdbTick_Invalidate(&nodePtr.p->copyDictToStartingNodeTime);
  NdbTick_Invalidate(&nodePtr.p->includeNodeInLCPAndGCPTime);
  NdbTick_Invalidate(&nodePtr.p->startDatabaseRecoveryTime);
  NdbTick_Invalidate(&nodePtr.p->startUndoDDTime);
  NdbTick_Invalidate(&nodePtr.p->startExecREDOLogTime);
  NdbTick_Invalidate(&nodePtr.p->startBuildIndexTime);
  NdbTick_Invalidate(&nodePtr.p->copyFragmentsStartedTime);
  NdbTick_Invalidate(&nodePtr.p->waitLCPForRestartTime);
  NdbTick_Invalidate(&nodePtr.p->waitSumaHandoverTime);
  NdbTick_Invalidate(&nodePtr.p->restartCompletedTime);
  NdbTick_Invalidate(&nodePtr.p->nodeGettingPermitTime);
  NdbTick_Invalidate(&nodePtr.p->nodeGettingIncludedTime);
  NdbTick_Invalidate(&nodePtr.p->nodeGettingSynchedTime);
  NdbTick_Invalidate(&nodePtr.p->nodeInLCPWaitStateTime);
  NdbTick_Invalidate(&nodePtr.p->nodeActiveTime);
}

/**
 * A node has allocated a node id, this happens even before the angel starts
 * a new ndbd/ndbmtd process or in a very early phase of ndbd/ndbmtd startup.
 */
void Dbdih::execALLOC_NODEID_REP(Signal *signal)
{
  NodeRecordPtr nodePtr;
  AllocNodeIdRep *rep = (AllocNodeIdRep*)&signal->theData[0];

  jamEntry();
  if (rep->nodeId >= MAX_NDB_NODES)
  {
    jam();
    return;
  }
  nodePtr.i = rep->nodeId;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
  if (nodePtr.p->nodeStatus == NodeRecord::NOT_IN_CLUSTER)
  {
    jam();
    return;
  }
  setNodeRecoveryStatus(rep->nodeId, NodeRecord::ALLOCATED_NODE_ID);
}

/**
 * A node have been included in the heartbeat protocol. This happens very early
 * on in the restart, from here the node need to act as a real-time engine and
 * thus has to avoid extremely time consuming activities that block execution.
 */
void Dbdih::execINCL_NODE_HB_PROTOCOL_REP(Signal *signal)
{
  InclNodeHBProtocolRep *rep = (InclNodeHBProtocolRep*)&signal->theData[0];
  jamEntry();

  setNodeRecoveryStatus(rep->nodeId, NodeRecord::INCLUDED_IN_HB_PROTOCOL);
}

/**
 * The node is blocked to continue in its node restart handling since another
 * node is currently going through the stages to among other things copy the
 * meta data.
 */
void Dbdih::execNDBCNTR_START_WAIT_REP(Signal *signal)
{
  NdbcntrStartWaitRep *rep = (NdbcntrStartWaitRep*)&signal->theData[0];
  jamEntry();

  setNodeRecoveryStatus(rep->nodeId, NodeRecord::NDBCNTR_START_WAIT);
}

/**
 * The node wasn't blocked by another node restart anymore, we can now
 * continue processing the restart and soon go on to copy the meta data.
 */
void Dbdih::execNDBCNTR_STARTED_REP(Signal *signal)
{
  NdbcntrStartedRep *rep = (NdbcntrStartedRep*)&signal->theData[0];
  jamEntry();

  setNodeRecoveryStatus(rep->nodeId, NodeRecord::NDBCNTR_STARTED);
}

/**
 * SUMA handover for the node has completed, this is the very final step
 * of the node restart after which the node is fully up and running.
 */
void Dbdih::execSUMA_HANDOVER_COMPLETE_REP(Signal *signal)
{
  SumaHandoverCompleteRep *rep = (SumaHandoverCompleteRep*)&signal->theData[0];
  jamEntry();

  setNodeRecoveryStatus(rep->nodeId, NodeRecord::RESTART_COMPLETED);
}

void Dbdih::execLOCAL_RECOVERY_COMP_REP(Signal *signal)
{
  jamEntry();
  if (reference() != cmasterdihref)
  {
    jam();
    if (likely(getNodeInfo(refToNode(cmasterdihref)).m_version >=
               NDBD_NODE_RECOVERY_STATUS_VERSION))
    {
      jam();
      sendSignal(cmasterdihref, GSN_LOCAL_RECOVERY_COMP_REP, signal,
                 LocalRecoveryCompleteRep::SignalLengthMaster, JBB);
    }
    else
    {
      jam();
    }
    return;
  }
  LocalRecoveryCompleteRep *rep =
    (LocalRecoveryCompleteRep*)&signal->theData[0];
  LocalRecoveryCompleteRep::PhaseIds phaseId =
    (LocalRecoveryCompleteRep::PhaseIds)rep->phaseId;
  Uint32 nodeId = rep->nodeId;

  switch (phaseId)
  {
  case LocalRecoveryCompleteRep::RESTORE_FRAG_COMPLETED:
    jam();
    setNodeRecoveryStatus(nodeId, NodeRecord::RESTORE_FRAG_COMPLETED);
    break;
  case LocalRecoveryCompleteRep::UNDO_DD_COMPLETED:
    jam();
    setNodeRecoveryStatus(nodeId, NodeRecord::UNDO_DD_COMPLETED);
    break;
  case LocalRecoveryCompleteRep::EXECUTE_REDO_LOG_COMPLETED:
    jam();
    setNodeRecoveryStatus(nodeId, NodeRecord::EXECUTE_REDO_LOG_COMPLETED);
    break;
  default:
    ndbrequire(false);
  }
}

/**
 * Called by starting nodes to provide non-master nodes with an estimate of how
 * long time it takes to synchronize the starting node with the alive nodes.
 */
void Dbdih::sendEND_TOREP(Signal *signal, Uint32 startingNodeId)
{
  EndToRep *rep = (EndToRep*)signal->getDataPtrSend();
  NodeRecordPtr nodePtr;
  nodePtr.i = cfirstAliveNode;
  rep->nodeId = startingNodeId;

  do
  {
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
    if (likely(getNodeInfo(nodePtr.i).m_version >=
               NDBD_NODE_RECOVERY_STATUS_VERSION))
    {
      /**
       * Don't send to nodes with earlier versions that don't have support
       * for this code.
       */
      jamLine(nodePtr.i);
      BlockReference ref = calcDihBlockRef(nodePtr.i);
      if (ref != cmasterdihref)
      {
        jam();
        sendSignal(ref, GSN_END_TOREP, signal, 
	           EndToRep::SignalLength, JBB);
      }
    }
    nodePtr.i = nodePtr.p->nextNode;
  } while (nodePtr.i != RNIL);
}

/**
 * Received in non-master nodes, to ensure we get estimate on synch time
 * between starting node and alive nodes.
 */
void Dbdih::execEND_TOREP(Signal *signal)
{
  EndToRep *rep = (EndToRep*)&signal->theData[0];
  jamEntry();
  if (isMaster())
  {
    jam();
    return;
  }
  setNodeRecoveryStatus(rep->nodeId, NodeRecord::NODE_IN_LCP_WAIT_STATE);
}

/**
 * Called when setting state to ALLOCATED_NODE_ID or
 * INCLUDE_IN_HB_PROTOCOL since a node can be dead for a long time
 * while we've been master and potentially could even have allocated
 * its node id before we became master.
 */
void Dbdih::check_node_not_restarted_yet(NodeRecordPtr nodePtr)
{
  if (nodePtr.p->nodeRecoveryStatus ==
      NodeRecord::NODE_NOT_RESTARTED_YET)
  {
    jam();
    /**
     * A node which has been dead since we started is restarted.
     * We set node failure time and node failure completed time
     * to now in this case to initialise those unknown values, we
     * rather report zero time than an uninitialised time.
     */
    nodePtr.p->nodeFailTime = c_current_time;
    nodePtr.p->nodeFailCompletedTime = c_current_time;
  }
}

void Dbdih::setNodeRecoveryStatus(Uint32 nodeId,
                                  NodeRecord::NodeRecoveryStatus new_status)
{
  NodeRecordPtr nodePtr;
  NDB_TICKS current_time;

  c_current_time = NdbTick_getCurrentTicks();
  current_time = c_current_time;

  nodePtr.i = nodeId;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
  jam();
  jamLine(nodePtr.p->nodeRecoveryStatus);

  /**
   * We maintain the state NODE_GETTING_PERMIT in the
   * variable is_pausable independent of when it is
   * received since it is needed to be able to handle
   * PAUSE protocol properly. The node recovery status
   * isn't sufficiently developed to handle this using
   * the state variable alone yet since we cannot handle
   * all restart types yet.
   */
  if (new_status == NodeRecord::NODE_GETTING_PERMIT)
  {
    jam();
    nodePtr.p->is_pausable = true;
  }
  else
  {
    jam();
    nodePtr.p->is_pausable = false;
  }

  if (getNodeState().startLevel != NodeState::SL_STARTED)
  {
    jam();
    /**
     * We will ignore all state transitions until we are started ourselves
     * before we even attempt to record state transitions. This means we
     * have no view into system restarts currently and inital starts. We
     * only worry about node restarts for now.
     */
    return;
  }
  if (new_status != NodeRecord::NODE_FAILED &&
      new_status != NodeRecord::NODE_FAILURE_COMPLETED)
  {
    jam();
    /**
     * Given that QMGR, NDBCNTR, DBDICT and DBDIH executes in the same thread
     * the possibility of jumping over a state doesn't exist. If we split out
     * any of those into separate threads in the future it is important to
     * check that the ndbrequire's in this function still holds.
     */
    if (!isMaster())
    {
      if (getNodeInfo(nodePtr.i).m_version <
          NDBD_NODE_RECOVERY_STATUS_VERSION)
      {
        jam();
        /**
         * We ignore state changes for non-master nodes that are from
         * too old versions to support all state transitions.
         */
        return;
      }
      if (nodePtr.p->nodeRecoveryStatus == NodeRecord::NODE_NOT_RESTARTED_YET &&
          new_status != NodeRecord::NODE_GETTING_PERMIT)
      {
        jam();
        /**
         * We're getting into the game too late, we will ignore state changes
         * for this node restart since it won't provide any useful info
         * anyways.
         */
        return;
      }
    }
    else if (nodePtr.p->nodeRecoveryStatus ==
             NodeRecord::NODE_NOT_RESTARTED_YET)
    {
      jam();
      switch (new_status)
      {
        case NodeRecord::ALLOCATED_NODE_ID:
          jam();
        case NodeRecord::INCLUDED_IN_HB_PROTOCOL:
          jam();
          /**
           * These are the normal states to hear about as first states after
           * we completed our own start. We can either first hear a node
           * failure and then we are sure we will follow the right path
           * since we heard about the node failure after being started.
           * If we weren't there for the node failure we are also ok with
           * starting all the way from allocated node id and included in
           * heartbeat protocol.
           */
          break;
        default:
          jam();
          jamLine(new_status);
          /**
           * This was due to a partial system restart, we haven't gotten
           * around to supporting this yet. This requires more work
           * before we can support it, this would mean that we come into
           * the action midway, so this will be solved when we handle
           * system restarts properly, but this is more work needed and
           * not done yet. So for now we ignore those states and will
           * handle the next time the node starts up instead.
           * TODO
           */
          return;
      }
    }
  }
  switch (new_status)
  {
    case NodeRecord::NODE_FAILED:
    /* State generated in DBDIH */
      jam();
      /**
       * A node failure can happen at any time and from any state as long as
       * it is defined in the cluster.
       *
       * This state change will be reported in all nodes at all times.
       *
       * We will clear all timers when a node fails since we want to ensure
       * that we only have valid timers backwards in time to avoid reading
       * old timers.
       */
      ndbrequire((nodePtr.p->nodeRecoveryStatus !=
                  NodeRecord::NOT_DEFINED_IN_CLUSTER));
      initNodeRecoveryTimers(nodePtr);
      nodePtr.p->nodeFailTime = current_time;
      break;
    case NodeRecord::NODE_FAILURE_COMPLETED:
    /* State generated in DBDIH */
      jam();
      /* This state change will be reported in all nodes at all times */
      ndbrequire(nodePtr.p->nodeRecoveryStatus ==
                 NodeRecord::NODE_FAILED);
      nodePtr.p->nodeFailCompletedTime = current_time;
      break;
    case NodeRecord::ALLOCATED_NODE_ID:
    /* State generated in QMGR */
      jam();
      ndbrequire(isMaster());
      ndbrequire((nodePtr.p->nodeRecoveryStatus ==
                  NodeRecord::NODE_FAILURE_COMPLETED) ||
                 (nodePtr.p->nodeRecoveryStatus ==
                  NodeRecord::ALLOCATED_NODE_ID) ||
                 (nodePtr.p->nodeRecoveryStatus ==
                  NodeRecord::NODE_NOT_RESTARTED_YET));
      check_node_not_restarted_yet(nodePtr);
      if (nodePtr.p->nodeRecoveryStatus == NodeRecord::ALLOCATED_NODE_ID)
      {
        jam();
        /**
         * If a node first allocates a node id and then comes back again to
         * allocate it again, then start counting time from node failed
         * as from now since a long time might have passed since we actually
         * failed.
         */
        nodePtr.p->nodeFailTime = current_time;
        nodePtr.p->nodeFailCompletedTime = current_time;
      }
      nodePtr.p->allocatedNodeIdTime = current_time;
      break;
    case NodeRecord::INCLUDED_IN_HB_PROTOCOL:
    /* State generated in QMGR */
      jam();
      /**
       * We can come here from ALLOCATED_NODE_ID obviously,
       * but it seems that we should also be able to get
       * here from a state where the node has been able to
       * allocate a node id with an old master, now it is
       * using this old allocated node id to be included in
       * the heartbeat protocol. So the node could be in
       * node not restarted yet or node failure completed.
       */
      ndbrequire(isMaster());
      ndbrequire((nodePtr.p->nodeRecoveryStatus ==
                  NodeRecord::ALLOCATED_NODE_ID) ||
                 (nodePtr.p->nodeRecoveryStatus ==
                  NodeRecord::NODE_NOT_RESTARTED_YET) ||
                 (nodePtr.p->nodeRecoveryStatus ==
                  NodeRecord::NODE_FAILURE_COMPLETED));
      check_node_not_restarted_yet(nodePtr);
      if (nodePtr.p->nodeRecoveryStatus == NodeRecord::NODE_FAILURE_COMPLETED)
      {
        jam();
        nodePtr.p->allocatedNodeIdTime = current_time;
      }
      nodePtr.p->includedInHBProtocolTime = current_time;
      break;
    case NodeRecord::NDBCNTR_START_WAIT:
    /* State generated in NDBCNTR */
      jam();
      ndbrequire(isMaster());
      ndbrequire(nodePtr.p->nodeRecoveryStatus ==
                 NodeRecord::INCLUDED_IN_HB_PROTOCOL);
      nodePtr.p->ndbcntrStartWaitTime = current_time;
      break;
    case NodeRecord::NDBCNTR_STARTED:
    /* State generated in NDBCNTR */
      jam();
      ndbrequire(isMaster());
      ndbrequire((nodePtr.p->nodeRecoveryStatus ==
                  NodeRecord::NDBCNTR_START_WAIT) ||
                 (nodePtr.p->nodeRecoveryStatus ==
                  NodeRecord::INCLUDED_IN_HB_PROTOCOL));

      if (nodePtr.p->nodeRecoveryStatus ==
          NodeRecord::INCLUDED_IN_HB_PROTOCOL)
      {
        jam();
        nodePtr.p->ndbcntrStartWaitTime = current_time;
      }
      nodePtr.p->ndbcntrStartedTime = current_time;
      break;
    case NodeRecord::START_PERMITTED:
    /* State generated in DBDIH */
      jam();
      ndbrequire(isMaster());
      ndbrequire(nodePtr.p->nodeRecoveryStatus ==
                 NodeRecord::NDBCNTR_STARTED);
      nodePtr.p->startPermittedTime = current_time;
      break;
    case NodeRecord::WAIT_LCP_TO_COPY_DICT:
    /* State generated in DBDIH */
      jam();
      ndbrequire(isMaster());
      ndbrequire(nodePtr.p->nodeRecoveryStatus ==
                 NodeRecord::START_PERMITTED);
      nodePtr.p->waitLCPToCopyDictTime = current_time;
      break;
    case NodeRecord::COPY_DICT_TO_STARTING_NODE:
    /* State generated in DBDIH */
      jam();
      ndbrequire(isMaster());
      ndbrequire(nodePtr.p->nodeRecoveryStatus ==
                 NodeRecord::WAIT_LCP_TO_COPY_DICT);
      nodePtr.p->copyDictToStartingNodeTime = current_time;
      break;
    case NodeRecord::INCLUDE_NODE_IN_LCP_AND_GCP:
    /* State generated in DBDIH */
      jam();
      ndbrequire(isMaster());
      ndbrequire(nodePtr.p->nodeRecoveryStatus ==
                 NodeRecord::COPY_DICT_TO_STARTING_NODE);
      nodePtr.p->includeNodeInLCPAndGCPTime = current_time;
      break;
    case NodeRecord::LOCAL_RECOVERY_STARTED:
    /* State generated in DBDIH */
      jam();
      ndbrequire(isMaster());
      ndbrequire(nodePtr.p->nodeRecoveryStatus ==
                 NodeRecord::INCLUDE_NODE_IN_LCP_AND_GCP);
      nodePtr.p->startDatabaseRecoveryTime = current_time;
      break;
    case NodeRecord::RESTORE_FRAG_COMPLETED:
    /* State generated in DBLQH in starting node */
      jam();
      ndbrequire(isMaster());
      ndbrequire(nodePtr.p->nodeRecoveryStatus ==
                 NodeRecord::LOCAL_RECOVERY_STARTED);
      nodePtr.p->startUndoDDTime = current_time;
      break;
    case NodeRecord::UNDO_DD_COMPLETED:
    /* State generated in DBLQH in starting node */
      jam();
      ndbrequire(isMaster());
      ndbrequire(nodePtr.p->nodeRecoveryStatus ==
                 NodeRecord::RESTORE_FRAG_COMPLETED);
      nodePtr.p->startExecREDOLogTime = current_time;
      break;
    case NodeRecord::EXECUTE_REDO_LOG_COMPLETED:
    /* State generated in DBLQH in starting node */
      jam();
      ndbrequire(isMaster());
      ndbrequire(nodePtr.p->nodeRecoveryStatus ==
                 NodeRecord::UNDO_DD_COMPLETED);
      nodePtr.p->startBuildIndexTime = current_time;
      break;
    case NodeRecord::COPY_FRAGMENTS_STARTED:
    /* State generated in DBDIH */
      jam();
      ndbrequire(isMaster());
      /**
       * If the starting node doesn't support reporting its
       * local recovery status, then we come here from
       * LOCAL_RECOVERY_STARTED, in the normal case with a
       * new version of the starting node we come here rather from
       * EXECUTE_REDO_LOG_COMPLETED.
       */
      ndbrequire((nodePtr.p->nodeRecoveryStatus ==
                  NodeRecord::EXECUTE_REDO_LOG_COMPLETED) ||
                 ((nodePtr.p->nodeRecoveryStatus ==
                  NodeRecord::LOCAL_RECOVERY_STARTED) &&
                  (getNodeInfo(nodePtr.i).m_version <
                  NDBD_NODE_RECOVERY_STATUS_VERSION)));
      if (nodePtr.p->nodeRecoveryStatus ==
          NodeRecord::LOCAL_RECOVERY_STARTED)
      {
        /**
         * We handle this state transition even for old versions since
         * it still gives all the information we need to make the right
         * decision about the LCP start.
         */
        NDB_TICKS start_time = nodePtr.p->startDatabaseRecoveryTime;
        jam();
        /* Set all local times to 0 if node doesn't support sending those */
        nodePtr.p->startUndoDDTime = start_time;
        nodePtr.p->startExecREDOLogTime = start_time;
        nodePtr.p->startBuildIndexTime = start_time;
      }
      nodePtr.p->copyFragmentsStartedTime = current_time;
      break;
    case NodeRecord::WAIT_LCP_FOR_RESTART:
    /* State generated in DBDIH */
      jam();
      ndbrequire(isMaster());
      ndbrequire(nodePtr.p->nodeRecoveryStatus ==
                 NodeRecord::COPY_FRAGMENTS_STARTED);
      nodePtr.p->waitLCPForRestartTime = current_time;
      break;
    case NodeRecord::WAIT_SUMA_HANDOVER:
    /* State generated in DBDIH */
      jam();
      ndbrequire(isMaster());
      ndbrequire(nodePtr.p->nodeRecoveryStatus ==
                 NodeRecord::WAIT_LCP_FOR_RESTART);
      nodePtr.p->waitSumaHandoverTime = current_time;
      break;
    case NodeRecord::RESTART_COMPLETED:
    /* State generated in DBDICT */
      jam();
      ndbrequire(isMaster());
      ndbrequire(nodePtr.p->nodeRecoveryStatus ==
                 NodeRecord::WAIT_SUMA_HANDOVER);
      nodePtr.p->restartCompletedTime = current_time;
      break;

    /* Non-master states */
    case NodeRecord::NODE_GETTING_PERMIT:
    {
      jam();
      ndbrequire(!isMaster());
      /**
       * NODE_GETTING_PERMIT is the first state a non-master node sees.
       * So we can come here from seeing node failure state or node
       * failure completed state.
       *
       * For a non-master node we can always come to any state from the
       * state NODE_NOT_RESTARTED_YET since we don't record any states
       * until we have completed our own restart and at that time there
       * can be other nodes restarting in any state.
       *
       * In addition we won't even record states for a starting node if
       * we only seen the final phases of the restart. So the state
       * NODE_NOT_RESTARTED_YET can be there through a major part of
       * a node restart.
       */
      ndbrequire(nodePtr.p->nodeRecoveryStatus ==
                 NodeRecord::NODE_FAILURE_COMPLETED ||
                 nodePtr.p->nodeRecoveryStatus ==
                 NodeRecord::NODE_NOT_RESTARTED_YET);
      if (nodePtr.p->nodeRecoveryStatus ==
          NodeRecord::NODE_NOT_RESTARTED_YET)
      {
        jam();
        nodePtr.p->nodeFailTime = current_time;
        nodePtr.p->nodeFailCompletedTime = current_time;
      }
      nodePtr.p->nodeGettingPermitTime = current_time;
      break;
    }
    case NodeRecord::NODE_GETTING_INCLUDED:
    {
      jam();
      ndbrequire(!isMaster());
      ndbrequire(nodePtr.p->nodeRecoveryStatus ==
                  NodeRecord::NODE_GETTING_PERMIT);
      nodePtr.p->nodeGettingIncludedTime = current_time;
      break;
    }
    case NodeRecord::NODE_GETTING_SYNCHED:
    {
      jam();
      ndbrequire(!isMaster());
      ndbrequire(nodePtr.p->nodeRecoveryStatus ==
                  NodeRecord::NODE_GETTING_INCLUDED);
      nodePtr.p->nodeGettingSynchedTime = current_time;
      break;
    }
    case NodeRecord::NODE_IN_LCP_WAIT_STATE:
    {
      jam();
      ndbrequire(!isMaster());
      /**
       * A weird case for coming to here with NODE_GETTING_INCLUDED is if
       * there are no tables that require being synched. This is an
       * unusual case, but still possible.
       */
      ndbrequire((nodePtr.p->nodeRecoveryStatus ==
                  NodeRecord::NODE_GETTING_INCLUDED) ||
                 (nodePtr.p->nodeRecoveryStatus ==
                  NodeRecord::NODE_GETTING_SYNCHED));
      if (nodePtr.p->nodeRecoveryStatus == NodeRecord::NODE_GETTING_INCLUDED)
      {
        jam();
        /* No fragment updates, set time to 0 for synch */
        nodePtr.p->nodeGettingSynchedTime = nodePtr.p->nodeGettingIncludedTime;
      }
      nodePtr.p->nodeInLCPWaitStateTime = current_time;
      break;
    }
    case NodeRecord::NODE_ACTIVE:
      jam();
      ndbrequire(!isMaster());
      ndbrequire(nodePtr.p->nodeRecoveryStatus ==
                 NodeRecord::NODE_IN_LCP_WAIT_STATE);
      nodePtr.p->nodeActiveTime = current_time;
      break;
    default:
      ndbrequire(false);
  }

  infoEvent("NR Status: node=%u,OLD=%s,NEW=%s",
            nodeId,
            get_status_str(nodePtr.p->nodeRecoveryStatus),
            get_status_str(new_status));

  g_eventLogger->info("NR Status: node=%u,OLD=%s,NEW=%s",
                      nodeId,
                      get_status_str(nodePtr.p->nodeRecoveryStatus),
                      get_status_str(new_status));

  nodePtr.p->nodeRecoveryStatus = new_status;
  ndbassert(check_node_recovery_timers(nodePtr.i));
}

void Dbdih::setNodeRecoveryStatusInitial(NodeRecordPtr nodePtr)
{
  DBG_NRS("setNodeRecoveryStatusInitial: node= " << nodePtr.i << "state= " <<
          (Uint32)NodeRecord::NODE_NOT_RESTARTED_YET);
  nodePtr.p->nodeRecoveryStatus = NodeRecord::NODE_NOT_RESTARTED_YET;
}

/**
 * Define heuristic constants
 * --------------------------
 *
 * The base for the maximum wait is the time the last LCP execution took.
 * We will never wait for more than 35% of this time. We will check this
 * even before attempting to wait any further. We will also cap the wait
 * to never exceed an hour.
 * 
 * Next we will adjust the maximum wait time down to 85% of this value
 * when we are calculating the estimate based on node states. This means
 * that if we estimate that we will wait for more than around 30% of an
 * LCP execution time, then we will start the LCP.
 *
 * If the node we are waiting for is in the early start phases then we
 * even less inclined to wait and will decrease the time by another
 * 50% dropping it to around 15% of an LCP execution time.
 *
 * If we have no node with a proper estimate, then we will drop the
 * wait time even more to 25% of the previous value, so 7-8% for
 * nodes in later start phases and only 3-4% in early start phases.
 */
#define STALL_MAX_ONE_HOUR (60 * 60 * 1000)
#define MAX_PERCENTAGE_OF_LCP_TIME_WE_STALL 35
#define MAX_PERCENTAGE_ADJUSTMENT_FOR_ESTIMATE 85
#define MAX_PERCENTAGE_ADJUSTMENT_FOR_EARLY_START_PHASES 50
#define MAX_PERCENTAGE_ADJUSTMENT_FOR_NO_ESTIMATE 25

bool Dbdih::check_for_too_long_wait(Uint64 &lcp_max_wait_time,
                                    Uint64 &lcp_stall_time,
                                    NDB_TICKS now)
{
  /**
   * We first get the time of the latest LCP execution. We want to stall
   * execution of LCPs, but never for so long that we get into other
   * problems such as out of REDO log.
   */
  Uint64 lcp_proc_time;
  Uint64 lcp_time = c_lcpState.m_lcp_time;
  Uint32 lcp_start = c_lcpState.lcpStallStart;
  if (lcp_start == 0)
  {
    jam();
    lcp_stall_time = 0;
  }
  else
  {
    jam();
    lcp_stall_time = NdbTick_Elapsed(c_lcpState.m_start_lcp_check_time,
                                     now).milliSec();
  }

  /**
   * We never wait for more than 1 hour and at most 35% of the time it
   * takes to execute an LCP. We calculate the maximum stall time here
   * based on those two inputs.
   */
  lcp_proc_time = MAX_PERCENTAGE_OF_LCP_TIME_WE_STALL * lcp_time;
  lcp_proc_time /= 100;
  lcp_max_wait_time = STALL_MAX_ONE_HOUR;
  if (lcp_max_wait_time > lcp_proc_time)
  {
    jam();
    lcp_max_wait_time = lcp_proc_time;
  }

  DBG_NRS("lcp_stall_time is = " << lcp_stall_time
           << " lcp_max_wait_time is = " << lcp_max_wait_time);
  /**
   * If we have already stalled for longer time than the maximum wait we
   * will allow, then we need not check the states of node restarts, we
   * will start the LCP anyways.
   */
  if (lcp_stall_time > lcp_max_wait_time)
  {
    jam();
    return true;
  }

  /**
   * In the calculated delay we will allow for a slightly shorter calculated
   * delay than the maximum actual delay we will wait. This is to avoid that
   * we wait for a long time only to stop waiting right before the wait is
   * over.
   */
  lcp_max_wait_time *= MAX_PERCENTAGE_ADJUSTMENT_FOR_ESTIMATE;
  lcp_max_wait_time /= 100; /* Decrease max time by 15% */
  lcp_max_wait_time -= lcp_stall_time; /* Decrease by time we already waited */
  return false;
}

void Dbdih::calculate_time_remaining(
                                Uint32 nodeId,
                                NDB_TICKS state_start_time,
                                NDB_TICKS now,
                                NodeRecord::NodeRecoveryStatus state,
                                Uint32 *node_waited_for,
                                Uint64 *time_since_state_start,
                                NodeRecord::NodeRecoveryStatus *max_status)
{
  ndbassert(NdbTick_IsValid(now));
  ndbassert(NdbTick_IsValid(state_start_time));

  if (state > (*max_status))
  {
    jam();
    (*time_since_state_start) =
      NdbTick_Elapsed(state_start_time, now).milliSec();
    (*max_status) = state;
    (*node_waited_for) = nodeId;
  }
  else if (state == (*max_status))
  {
    jam();
    Uint64 loc_time_since_state_start;
    loc_time_since_state_start =
      NdbTick_Elapsed(state_start_time, now).milliSec();
    if (loc_time_since_state_start > (*time_since_state_start))
    {
      jam();
      (*time_since_state_start) = loc_time_since_state_start;
      (*node_waited_for) = nodeId;
    }
  }
}

void Dbdih::calculate_most_recent_node(
                        Uint32 nodeId,
                        NDB_TICKS state_start_time,
                        NodeRecord::NodeRecoveryStatus state,
                        Uint32 *most_recent_node,
                        NDB_TICKS *most_recent_start_time,
                        NodeRecord::NodeRecoveryStatus *most_recent_state)
{
  ndbassert(NdbTick_IsValid(state_start_time));
  if ((*most_recent_node) == 0)
  {
    /* No state set, set this as state */
    jam();
  }
  else if ((*most_recent_state) == state)
  {
    jam();
    /* Same state as before, use most recent */
    if (NdbTick_Compare((*most_recent_start_time),
                        state_start_time) > 0)
    {
      jam();
      return;
    }
    jam();
  }
  else if ((*most_recent_state) == NodeRecord::NODE_ACTIVE)
  {
    /* Old state from non-master, new from master, use this one */
    jam();
  }
  else if ((*most_recent_state) > state)
  {
    /**
     * Two master states, use the latest (this one)
     * Latest is the one with the lowest state since
     * the older one has progressed longer.
     */
    jam();
  }
  else
  {
    /* Ignore this state, we already have a better one */
    jam();
    return;
  }
  (*most_recent_state) = state;
  (*most_recent_start_time) = state_start_time;
  (*most_recent_node) = nodeId;
  return;
}

#if 0
/* Useful debug function when trying to find overwrite of node record */
void Dbdih::check_all_node_recovery_timers(void)
{
  Uint32 nodeId;
  for (nodeId = 1; nodeId < MAX_NDB_NODES; nodeId++)
  {
    ndbassert(check_node_recovery_timers(nodeId));
  }
}
#endif

bool Dbdih::check_node_recovery_timers(Uint32 nodeId)
{
  NodeRecordPtr nodePtr;
  nodePtr.i = nodeId;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);

  switch (nodePtr.p->nodeRecoveryStatus)
  {
  case NodeRecord::RESTART_COMPLETED:
    ndbassert(NdbTick_IsValid(nodePtr.p->restartCompletedTime));
  case NodeRecord::WAIT_SUMA_HANDOVER:
    ndbassert(NdbTick_IsValid(nodePtr.p->waitSumaHandoverTime));
  case NodeRecord::WAIT_LCP_FOR_RESTART:
    ndbassert(NdbTick_IsValid(nodePtr.p->waitLCPForRestartTime));
  case NodeRecord::COPY_FRAGMENTS_STARTED:
    ndbassert(NdbTick_IsValid(nodePtr.p->copyFragmentsStartedTime));
  case NodeRecord::EXECUTE_REDO_LOG_COMPLETED:
    ndbassert(NdbTick_IsValid(nodePtr.p->startBuildIndexTime));
  case NodeRecord::UNDO_DD_COMPLETED:
    ndbassert(NdbTick_IsValid(nodePtr.p->startExecREDOLogTime));
  case NodeRecord::RESTORE_FRAG_COMPLETED:
    ndbassert(NdbTick_IsValid(nodePtr.p->startUndoDDTime));
  case NodeRecord::LOCAL_RECOVERY_STARTED:
    ndbassert(NdbTick_IsValid(nodePtr.p->startDatabaseRecoveryTime));
  case NodeRecord::INCLUDE_NODE_IN_LCP_AND_GCP:
    ndbassert(NdbTick_IsValid(nodePtr.p->includeNodeInLCPAndGCPTime));
  case NodeRecord::COPY_DICT_TO_STARTING_NODE:
    ndbassert(NdbTick_IsValid(nodePtr.p->copyDictToStartingNodeTime));
  case NodeRecord::WAIT_LCP_TO_COPY_DICT:
    ndbassert(NdbTick_IsValid(nodePtr.p->waitLCPToCopyDictTime));
  case NodeRecord::START_PERMITTED:
    ndbassert(NdbTick_IsValid(nodePtr.p->startPermittedTime));
  case NodeRecord::NDBCNTR_STARTED:
    ndbassert(NdbTick_IsValid(nodePtr.p->ndbcntrStartedTime));
  case NodeRecord::NDBCNTR_START_WAIT:
    ndbassert(NdbTick_IsValid(nodePtr.p->ndbcntrStartWaitTime));
  case NodeRecord::INCLUDED_IN_HB_PROTOCOL:
    ndbassert(NdbTick_IsValid(nodePtr.p->includedInHBProtocolTime));
  case NodeRecord::ALLOCATED_NODE_ID:
    ndbassert(NdbTick_IsValid(nodePtr.p->allocatedNodeIdTime));
    ndbassert(NdbTick_IsValid(nodePtr.p->nodeFailCompletedTime));
    ndbassert(NdbTick_IsValid(nodePtr.p->nodeFailTime));
    break;
  case NodeRecord::NODE_ACTIVE:
    ndbassert(NdbTick_IsValid(nodePtr.p->nodeActiveTime));
  case NodeRecord::NODE_IN_LCP_WAIT_STATE:
    ndbassert(NdbTick_IsValid(nodePtr.p->nodeInLCPWaitStateTime));
  case NodeRecord::NODE_GETTING_SYNCHED:
    ndbassert(NdbTick_IsValid(nodePtr.p->nodeGettingSynchedTime));
  case NodeRecord::NODE_GETTING_INCLUDED:
    ndbassert(NdbTick_IsValid(nodePtr.p->nodeGettingIncludedTime));
  case NodeRecord::NODE_GETTING_PERMIT:
    ndbassert(NdbTick_IsValid(nodePtr.p->nodeGettingPermitTime));
    ndbassert(NdbTick_IsValid(nodePtr.p->nodeFailCompletedTime));
    ndbassert(NdbTick_IsValid(nodePtr.p->nodeFailTime));
    break;
  case NodeRecord::NODE_FAILURE_COMPLETED:
    ndbassert(NdbTick_IsValid(nodePtr.p->nodeFailCompletedTime));
  case NodeRecord::NODE_FAILED:
    ndbassert(NdbTick_IsValid(nodePtr.p->nodeFailTime));
    break;
  default:
    jam();
    break;
  }
  return true;
}
 
/**
 * We want to stall the LCP start if any node is encountering the place where
 * we need to participate in an LCP to complete our restart. If any node is
 * close to reaching this state we want to block the LCP until it has reached
 * this state.
 */
bool Dbdih::check_stall_lcp_start(void)
{
  const NDB_TICKS now = c_current_time = NdbTick_getCurrentTicks();
  /**
   * The following variables are calculated to measure the node closest to
   * reaching the WAIT_LCP_FOR_RESTART state.
   */
  NodeRecord::NodeRecoveryStatus max_status = NodeRecord::NOT_DEFINED_IN_CLUSTER;
  Uint64 time_since_state_start = 0;
  Uint32 node_waited_for = 0;
  NDB_TICKS state_start_time;

  /**
   * This is the node we will use to estimate the time remaining. If no such
   * node exists, then we have no measurements to use and we will have to
   * fall back to heuristics. We also store the state and time of this variable
   * to get the most recent estimate.
   */
  NodeRecord::NodeRecoveryStatus most_recent_node_status =
    NodeRecord::ALLOCATED_NODE_ID;
  Uint32 most_recent_node = 0;
  NDB_TICKS most_recent_node_start_time;

  /**
   * If the estimated time until we reach the WAIT_LCP_FOR_RESTART state is
   * higher than the below value, then we won't wait at all, we will start
   * the LCP immediately in this case.
   */
  Uint64 lcp_max_wait_time = 0;
  Uint64 lcp_stall_time = 0;

  /**
   * If we don't find any most recent node, then should we fall back to
   * heuristics?. We fall back to heuristics when we have nodes in early
   * stages of node restart that could potentially move through those
   * stages rapidly.
   */
  NodeRecordPtr nodePtr;

  Uint64 time_remaining;
  Uint64 estimated_time;

  NdbTick_Invalidate(&most_recent_node_start_time);
  NdbTick_Invalidate(&state_start_time);

  if (check_for_too_long_wait(lcp_max_wait_time,
                              lcp_stall_time,
                              now))
  {
    jam();
    goto immediate_start_label;
  }

  /**
   * It is ok to wait before starting the new LCP, we will go through the
   * data nodes and see if we have reasons to wait.
   */
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++)
  {
    ptrAss(nodePtr, nodeRecord);
    switch (nodePtr.p->nodeRecoveryStatus)
    {
      case NodeRecord::NOT_DEFINED_IN_CLUSTER:
      case NodeRecord::NODE_NOT_RESTARTED_YET:
      {
        jam();
        /**
         * We have no useful information about estimated time remaining
         * and we're not restarting this node currently. Simply continue.
         */
        break;
      }
      /**
       * The states NODE_ACTIVE, RESTART_COMPLETED, WAIT_LCP_FOR_RESTART and
       * WAIT_SUMA_HANDOVER can all be used to estimate the time remaining
       * for the node restarts still running. We use the most recent estimate,
       * the WAIT_LCP_FOR_RESTART being most recent, then WAIT_SUMA_HANDOVER,
       * then RESTART_COMPLETED and finally NODE_ACTIVE.
       */
      case NodeRecord::NODE_ACTIVE:
      {
        jam();
        state_start_time = nodePtr.p->nodeActiveTime;
        calculate_most_recent_node(nodePtr.i,
                                   state_start_time,
                                   nodePtr.p->nodeRecoveryStatus,
                                   &most_recent_node,
                                   &most_recent_node_start_time,
                                   &most_recent_node_status);
        break;
      }
      case NodeRecord::RESTART_COMPLETED:
      {
        jam();
        state_start_time = nodePtr.p->restartCompletedTime;
        calculate_most_recent_node(nodePtr.i,
                                   state_start_time,
                                   nodePtr.p->nodeRecoveryStatus,
                                   &most_recent_node,
                                   &most_recent_node_start_time,
                                   &most_recent_node_status);
        break;
      }
      case NodeRecord::WAIT_SUMA_HANDOVER:
      {
        jam();
        state_start_time = nodePtr.p->waitSumaHandoverTime;
        calculate_most_recent_node(nodePtr.i,
                                   state_start_time,
                                   nodePtr.p->nodeRecoveryStatus,
                                   &most_recent_node,
                                   &most_recent_node_start_time,
                                   &most_recent_node_status);
        break;
      }
      case NodeRecord::WAIT_LCP_FOR_RESTART:
      {
        jam();
        state_start_time = nodePtr.p->waitLCPForRestartTime;
        ndbassert(NdbTick_IsValid(nodePtr.p->includeNodeInLCPAndGCPTime));
        ndbassert(NdbTick_IsValid(nodePtr.p->copyDictToStartingNodeTime));
        calculate_most_recent_node(nodePtr.i,
                                   state_start_time,
                                   nodePtr.p->nodeRecoveryStatus,
                                   &most_recent_node,
                                   &most_recent_node_start_time,
                                   &most_recent_node_status);
        break;
      }
      /**
       * The following are states where we expect a node restart to either
       * be ongoing or to very soon start up.
       *
       * The states ranging from NDBCNTR_STARTED to COPY_FRAGMENTS_STARTED
       * are states that can be used to estimate the time remaining until
       * someone reaches the WAIT_LCP_FOR_RESTART state. We get the state
       * and time in this state for the node that has proceeded the
       * furthest in the restart. The other states are less good for
       * estimating the time remaining but will still be used with some
       * extra heuristics.
       */
      case NodeRecord::NODE_FAILED:
      {
        jam();
        state_start_time = nodePtr.p->nodeFailTime;
        calculate_time_remaining(nodePtr.i,
                                 state_start_time,
                                 now,
                                 nodePtr.p->nodeRecoveryStatus,
                                 &node_waited_for,
                                 &time_since_state_start,
                                 &max_status);
        break;
      }
      case NodeRecord::NODE_FAILURE_COMPLETED:
      {
        jam();
        state_start_time = nodePtr.p->nodeFailCompletedTime;
        calculate_time_remaining(nodePtr.i,
                                 state_start_time,
                                 now,
                                 nodePtr.p->nodeRecoveryStatus,
                                 &node_waited_for,
                                 &time_since_state_start,
                                 &max_status);
        break;
      }
      case NodeRecord::ALLOCATED_NODE_ID:
      {
        jam();
        state_start_time = nodePtr.p->allocatedNodeIdTime;
        calculate_time_remaining(nodePtr.i,
                                 state_start_time,
                                 now,
                                 nodePtr.p->nodeRecoveryStatus,
                                 &node_waited_for,
                                 &time_since_state_start,
                                 &max_status);
        break;
      }
      case NodeRecord::INCLUDED_IN_HB_PROTOCOL:
      {
        jam();
        state_start_time = nodePtr.p->includedInHBProtocolTime;
        calculate_time_remaining(nodePtr.i,
                                 state_start_time,
                                 now,
                                 nodePtr.p->nodeRecoveryStatus,
                                 &node_waited_for,
                                 &time_since_state_start,
                                 &max_status);
        break;
      }
      case NodeRecord::NDBCNTR_START_WAIT:
      {
        jam();
        state_start_time = nodePtr.p->ndbcntrStartWaitTime;
        calculate_time_remaining(nodePtr.i,
                                 state_start_time,
                                 now,
                                 nodePtr.p->nodeRecoveryStatus,
                                 &node_waited_for,
                                 &time_since_state_start,
                                 &max_status);
        break;
      }
      case NodeRecord::NDBCNTR_STARTED:
      {
        jam();
        state_start_time = nodePtr.p->ndbcntrStartedTime;
        calculate_time_remaining(nodePtr.i,
                                 state_start_time,
                                 now,
                                 nodePtr.p->nodeRecoveryStatus,
                                 &node_waited_for,
                                 &time_since_state_start,
                                 &max_status);
        break;
      }
      case NodeRecord::START_PERMITTED:
      {
        jam();
        state_start_time = nodePtr.p->startPermittedTime;
        calculate_time_remaining(nodePtr.i,
                                 state_start_time,
                                 now,
                                 nodePtr.p->nodeRecoveryStatus,
                                 &node_waited_for,
                                 &time_since_state_start,
                                 &max_status);
        break;
      }
      case NodeRecord::WAIT_LCP_TO_COPY_DICT:
      {
        jam();
        state_start_time = nodePtr.p->waitLCPToCopyDictTime;
        calculate_time_remaining(nodePtr.i,
                                 state_start_time,
                                 now,
                                 nodePtr.p->nodeRecoveryStatus,
                                 &node_waited_for,
                                 &time_since_state_start,
                                 &max_status);
        break;
      }
      case NodeRecord::COPY_DICT_TO_STARTING_NODE:
      {
        jam();
        state_start_time = nodePtr.p->copyDictToStartingNodeTime;
        calculate_time_remaining(nodePtr.i,
                                 state_start_time,
                                 now,
                                 nodePtr.p->nodeRecoveryStatus,
                                 &node_waited_for,
                                 &time_since_state_start,
                                 &max_status);
        break;
      }
      case NodeRecord::INCLUDE_NODE_IN_LCP_AND_GCP:
      {
        jam();
        state_start_time = nodePtr.p->includeNodeInLCPAndGCPTime;
        calculate_time_remaining(nodePtr.i,
                                 state_start_time,
                                 now,
                                 nodePtr.p->nodeRecoveryStatus,
                                 &node_waited_for,
                                 &time_since_state_start,
                                 &max_status);
        break;
      }
      case NodeRecord::LOCAL_RECOVERY_STARTED:
      {
        jam();
        state_start_time = nodePtr.p->startDatabaseRecoveryTime;
        calculate_time_remaining(nodePtr.i,
                                 state_start_time,
                                 now,
                                 nodePtr.p->nodeRecoveryStatus,
                                 &node_waited_for,
                                 &time_since_state_start,
                                 &max_status);
        break;
      }
      case NodeRecord::RESTORE_FRAG_COMPLETED:
      {
        jam();
        state_start_time = nodePtr.p->startUndoDDTime;
        calculate_time_remaining(nodePtr.i,
                                 state_start_time,
                                 now,
                                 nodePtr.p->nodeRecoveryStatus,
                                 &node_waited_for,
                                 &time_since_state_start,
                                 &max_status);
        break;
      }
      case NodeRecord::UNDO_DD_COMPLETED:
      {
        jam();
        state_start_time = nodePtr.p->startExecREDOLogTime;
        calculate_time_remaining(nodePtr.i,
                                 state_start_time,
                                 now,
                                 nodePtr.p->nodeRecoveryStatus,
                                 &node_waited_for,
                                 &time_since_state_start,
                                 &max_status);
        break;
      }
      case NodeRecord::EXECUTE_REDO_LOG_COMPLETED:
      {
        jam();
        state_start_time = nodePtr.p->startBuildIndexTime;
        calculate_time_remaining(nodePtr.i,
                                 state_start_time,
                                 now,
                                 nodePtr.p->nodeRecoveryStatus,
                                 &node_waited_for,
                                 &time_since_state_start,
                                 &max_status);
        break;
      }
      case NodeRecord::COPY_FRAGMENTS_STARTED:
      {
        jam();
        state_start_time = nodePtr.p->copyFragmentsStartedTime;
        calculate_time_remaining(nodePtr.i,
                                 state_start_time,
                                 now,
                                 nodePtr.p->nodeRecoveryStatus,
                                 &node_waited_for,
                                 &time_since_state_start,
                                 &max_status);
        break;
      }
      default:
      {
        jamLine(nodePtr.p->nodeRecoveryStatus);
        /* The states only used on non-masters should never occur here */
        ndbrequire(false);
      }
    }
  }
  if (node_waited_for == 0)
  {
    jam();
    /* No restart is ongoing, we can safely proceed with starting the LCP. */
    goto immediate_start_label;
  }
  if (most_recent_node == 0)
  {
    jam();
    /**
     * We have restarts ongoing, but we have no node that can be used to
     * estimate the remaining time. In this case we use a heuristic which
     * means we're willing to wait for 25% of the max wait time (about
     * 7% of the time to execute an LCP). If this wait is sufficient for a
     * node to reach WAIT_LCP_FOR_RESTART we immediately get more recent
     * estimate and can make more intelligent estimates at that time.
     */
    lcp_max_wait_time *= MAX_PERCENTAGE_ADJUSTMENT_FOR_NO_ESTIMATE;
    lcp_max_wait_time /= 100;
    if (lcp_stall_time > lcp_max_wait_time)
    {
      jam();
      goto immediate_start_label;
    }
    else
    {
      jam();
      goto wait_label;
    }
  }

  /**
   * A node exists which has estimates on times to execute the node restart.
   * A node restart exists as well. We will estimate whether it makes sense
   * to delay the LCP for a while more at this time.
   */
  nodePtr.i = most_recent_node;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
  jamLine(most_recent_node);
  jamLine(node_waited_for);

  if (nodePtr.p->nodeRecoveryStatus == NodeRecord::NODE_ACTIVE)
  {
    /**
     * We have only access to a node where we gathered measurements during
     * the time we were non-master node. We transfer times from non-master
     * timers to master timers as best estimates to use below in our
     * calculations. We also change the max_status to ensure that we read
     * the correct timer when doing the calculations.
     *
     * Also we don't measure any time since state start since our calculations
     * very rough and it would take a lot of logic to get a good estimate of
     * time since the state start according the stats gathered as non-master.
     *
     * Also given that our estimates are less accurate we will decrease the
     * maximum wait time by 50%.
     */
    if (max_status < NodeRecord::INCLUDE_NODE_IN_LCP_AND_GCP)
    {
      jam();
      max_status = NodeRecord::NDBCNTR_STARTED;
      nodePtr.p->ndbcntrStartedTime = nodePtr.p->nodeGettingPermitTime;
    }
    else if (max_status < NodeRecord::COPY_FRAGMENTS_STARTED)
    {
      jam();
      max_status = NodeRecord::INCLUDE_NODE_IN_LCP_AND_GCP;
      nodePtr.p->includeNodeInLCPAndGCPTime =
        nodePtr.p->nodeGettingIncludedTime;
    }
    else
    {
      jam();
      max_status = NodeRecord::COPY_FRAGMENTS_STARTED;
      nodePtr.p->copyFragmentsStartedTime = nodePtr.p->nodeGettingSynchedTime;
    }
    nodePtr.p->waitLCPForRestartTime = nodePtr.p->nodeInLCPWaitStateTime;
    time_since_state_start = 0;
    lcp_max_wait_time *= MAX_PERCENTAGE_ADJUSTMENT_FOR_EARLY_START_PHASES;
    lcp_max_wait_time /= 100;
  }

  /**
   * Calculate estimated time remaining from start of the max state we've seen.
   */
  switch (max_status)
  {
    case NodeRecord::NODE_FAILED:
    case NodeRecord::NODE_FAILURE_COMPLETED:
    case NodeRecord::ALLOCATED_NODE_ID:
    case NodeRecord::INCLUDED_IN_HB_PROTOCOL:
    case NodeRecord::NDBCNTR_START_WAIT:
    {
      jam();
      /**
       * Estimate a complete restart, these states have wait states that are
       * hard to estimate impact of. So here we simply want a measurement
       * whether it pays off to wait, we also decrease the maximum wait time
       * to decrease likelihood we will actually wait.
       */
      lcp_max_wait_time *= 50;
      lcp_max_wait_time /= 100;
      estimated_time = NdbTick_Elapsed(nodePtr.p->ndbcntrStartedTime,
                              nodePtr.p->waitLCPForRestartTime).milliSec();
      break;
    }
    case NodeRecord::NDBCNTR_STARTED:
    {
      jam();
      estimated_time = NdbTick_Elapsed(nodePtr.p->ndbcntrStartedTime,
                              nodePtr.p->waitLCPForRestartTime).milliSec();
      break;
    }
    case NodeRecord::START_PERMITTED:
    {
      jam();
      estimated_time = NdbTick_Elapsed(nodePtr.p->startPermittedTime,
                              nodePtr.p->waitLCPForRestartTime).milliSec();
      break;
    }
    case NodeRecord::WAIT_LCP_TO_COPY_DICT:
    {
      jam();
      estimated_time = NdbTick_Elapsed(nodePtr.p->waitLCPToCopyDictTime,
                              nodePtr.p->waitLCPForRestartTime).milliSec();
      break;
    }
    case NodeRecord::COPY_DICT_TO_STARTING_NODE:
    {
      jam();
      estimated_time = NdbTick_Elapsed(nodePtr.p->copyDictToStartingNodeTime,
                              nodePtr.p->waitLCPForRestartTime).milliSec();
      break;
    }
    case NodeRecord::INCLUDE_NODE_IN_LCP_AND_GCP:
    {
      jam();
      estimated_time = NdbTick_Elapsed(nodePtr.p->includeNodeInLCPAndGCPTime,
                              nodePtr.p->waitLCPForRestartTime).milliSec();
      break;
    }
    case NodeRecord::LOCAL_RECOVERY_STARTED:
    {
      jam();
      estimated_time = NdbTick_Elapsed(nodePtr.p->startDatabaseRecoveryTime,
                              nodePtr.p->waitLCPForRestartTime).milliSec();
      break;
    }
    case NodeRecord::RESTORE_FRAG_COMPLETED:
    {
      jam();
      estimated_time = NdbTick_Elapsed(nodePtr.p->startUndoDDTime,
                              nodePtr.p->waitLCPForRestartTime).milliSec();
      break;
    }
    case NodeRecord::UNDO_DD_COMPLETED:
    {
      jam();
      estimated_time = NdbTick_Elapsed(nodePtr.p->startExecREDOLogTime,
                              nodePtr.p->waitLCPForRestartTime).milliSec();
      break;
    }
    case NodeRecord::EXECUTE_REDO_LOG_COMPLETED:
    {
      jam();
      estimated_time = NdbTick_Elapsed(nodePtr.p->startBuildIndexTime,
                              nodePtr.p->waitLCPForRestartTime).milliSec();
      break;
    }
    case NodeRecord::COPY_FRAGMENTS_STARTED:
    {
      jam();
      estimated_time = NdbTick_Elapsed(nodePtr.p->copyFragmentsStartedTime,
                              nodePtr.p->waitLCPForRestartTime).milliSec();
      break;
    }
    default:
    {
      jamLine(max_status);
      ndbrequire(false);
      return true; /* Will never reach here, silence compiler warnings */
    }
  }

  if (estimated_time < time_since_state_start)
  {
    jam();
    time_remaining = 0;
  }
  else
  {
    jam();
    time_remaining = estimated_time - time_since_state_start;
  }
  if (time_remaining > lcp_max_wait_time)
  {
    jam();
    goto immediate_start_label;
  }

wait_label:
  /**
   * We exit from the routine to check for stalling LCPs with a decision
   * to stall or continue stalling. We ensure that we output proper logs
   * about this decision every now and then and that we record the proper
   * information about the stalling decisions.
   */
  jam();
  if (c_lcpState.lcpStallStart == 0)
  {
    jam();
    c_lcpState.m_start_lcp_check_time = now;
  }
  if (c_lcpState.lcpStallStart == 0 ||
      node_waited_for != c_lcpState.stall_node_waiting_for ||
      NdbTick_Elapsed(c_lcpState.lastLogTime, now).milliSec() >
      Uint64(1200000))
  {
    /**
     * Output a log message every time we start stalling
     * and every time we change node waiting for and every
     * time we have stalled for 2 mins.
     */
    jam();
    c_lcpState.lastLogTime = now;
    infoEvent("Stall LCP, LCP time = %u secs,"
              " wait for Node%u, state %s",
              Uint32(c_lcpState.m_lcp_time / 1000),
              node_waited_for,
              get_status_str(max_status));
    infoEvent("Stall LCP: current stall time: %u secs,"
              " max wait time:%u secs",
              Uint32(lcp_stall_time/1000),
              Uint32(lcp_max_wait_time/1000));
  }
  c_lcpState.lcpStallStart = 1;
  c_lcpState.stall_node_waiting_for = node_waited_for;
  return true;

immediate_start_label:
  /**
   * We quit waiting for starting the LCP, we will start immediately.
   * This will be recorded as a start LCP, so no need for special
   * logging message for this. Simply reset the stall state.
   */
  c_lcpState.lcpStallStart = 0;
  return false;
}

const char*
Dbdih::get_status_str(NodeRecord::NodeRecoveryStatus status)
{
  const char *status_str;
  switch (status)
  {
  case NodeRecord::ALLOCATED_NODE_ID:
    status_str="Allocated node id";
    break;
  case NodeRecord::INCLUDED_IN_HB_PROTOCOL:
    status_str="Included in heartbeat protocol";
    break;
  case NodeRecord::NDBCNTR_START_WAIT:
    status_str="Wait for NDBCNTR master permit";
    break;
  case NodeRecord::NDBCNTR_STARTED:
    status_str="NDBCNTR master permitted us";
    break;
  case NodeRecord::NODE_GETTING_PERMIT:
  case NodeRecord::START_PERMITTED:
    status_str="All nodes permitted us";
    break;
  case NodeRecord::WAIT_LCP_TO_COPY_DICT:
    status_str="Wait for LCP complete to copy meta data";
    break;
  case NodeRecord::COPY_DICT_TO_STARTING_NODE:
    status_str="Copy meta data to start node";
    break;
  case NodeRecord::NODE_GETTING_INCLUDED:
  case NodeRecord::INCLUDE_NODE_IN_LCP_AND_GCP:
    status_str="Include node in LCP/GCP protocols";
    break;
  case NodeRecord::LOCAL_RECOVERY_STARTED:
    status_str="Restore fragments ongoing";
    break;
  case NodeRecord::RESTORE_FRAG_COMPLETED:
    status_str="Undo Disk data ongoing";
    break;
  case NodeRecord::UNDO_DD_COMPLETED:
    status_str="Execute REDO logs ongoing";
    break;
  case NodeRecord::EXECUTE_REDO_LOG_COMPLETED:
    status_str="Build indexes ongoing";
    break;
  case NodeRecord::NODE_GETTING_SYNCHED:
  case NodeRecord::COPY_FRAGMENTS_STARTED:
    status_str="Synchronize start node with live nodes";
    break;
  case NodeRecord::NODE_IN_LCP_WAIT_STATE:
  case NodeRecord::WAIT_LCP_FOR_RESTART:
    status_str="Wait LCP to ensure durability";
    break;
  case NodeRecord::WAIT_SUMA_HANDOVER:
    status_str="Wait handover of subscriptions";
    break;
  case NodeRecord::NODE_ACTIVE:
  case NodeRecord::RESTART_COMPLETED:
    status_str="Restart completed";
    break;
  case NodeRecord::NODE_FAILED:
    status_str="Node failed, fail handling ongoing";
    break;
  case NodeRecord::NODE_FAILURE_COMPLETED:
    status_str="Node failure handling complete";
    break;
  case NodeRecord::NODE_NOT_RESTARTED_YET:
    status_str="Initial state";
    break;
  default:
    jamLine(status);
    ndbrequire(false);
    return NULL; /* Will never reach here, silence compiler warnings */
  }
  return status_str;
}

/**
 * Fill the table with the following data:
 * All the times are reported in seconds.
 *
 * NodeRestartStatus: This is a string which is derived from the
 *  nodeRecoveryStatus.
 *
 * CompleteFailTime: Time to complete the node failure.
 * AllocatedNodeIdTime: Time from completing node failure until we have
 *   allocated a node id again.
 * IncludeHeartbeatProtocolTime: Time from allocating node id until we
 *   have been included in the heartbeat protocol.
 * NdbcntrStartWaitTime: Time from being included in the heartbeat
 *   protocol until we have been set to wait for NDBCNTR master to
 *   allow us to continue starting.
 * NdbcntrStartedTime: Time from we start waiting for NDBCNTR master
 *   to accept us into the cluster until we are accepted into the cluster.
 * StartPermittedTime: Time from we are accepted by NDBCNTR master to
 *   start until we have received Start permit from all nodes.
 * WaitLCPToCopyDictTime: Time from all nodes permit us to start until we
 *   have finished waiting for LCP to complete before we copy the meta
 *   data in the cluster.
 * CopyToDictStartingNodeTime: Time from we have been allowed to start
 *   copying meta data until we have completed this.
 * IncludeNodeInLCPAndGCPTime: Time from we have copied the meta data
 *   until we have stopped the GCP protocol and have been included into
 *   the LCP and GCP protocol by all nodes.
 * LocalRecoveryTime: Time from being included until we have fully completed
 *   the Local Recovery in a node.
 * RestoreFragmentTime:
 * Time to restore all fragments from local files generated by the LCPs.
 * UndoDDTime:
 * Time to run Disk Data UNDO log on all restored fragments.
 * ExecREDOLogTime:
 * Time to execute the REDO log on all restored fragments.
 * BuildIndexTime:
 * Time to rebuild indexes on all restored fragments.
 * CopyFragmentsTime: Time from completing Local Recovery until all recent data
 *   have been copied from alive nodes to starting node.
 * WaitSumaHandoverTime: Time from being fully up-to-date until we have
 *   completed the handover of replication subscriptions.
 * Total recovery time:
 * Total time from node failure completed until we are started again.
 *
 * For nodes that have states set when we were not yet master we will only
 * report a few times:
 * StartPermittedTime: Time from node completed the node failure until our
 *   node permitted the node to start.
 * IncludeNodeInLCPAndGCPTime: Time from we permitted the node to start until
 *   we completed including the node in the LCP and GCP protocol.
 * LocalRecoveryTime: Time from we were included in the LCP and GCP protocol until
 *   we started copying the fragments.
 * CopyFragmentsTime: Time from we started synchronizing the starting node
 *   until we completed the node restart.
 *
 * Any time not happened yet will be reported as 0.
 */
void Dbdih::write_zero_columns(Ndbinfo::Row &row, Uint32 num_rows)
{
  for (Uint32 i = 0; i < num_rows; i++)
  {
    jam();
    row.write_uint32(Uint32(0));
  }
  return;
}

void Dbdih::fill_row_with_node_restart_status(NodeRecordPtr nodePtr,
                                              Ndbinfo::Row &row)
{
  Uint64 elapsed;
  NodeRecord::NodeRecoveryStatus status = nodePtr.p->nodeRecoveryStatus;
  row.write_uint32(nodePtr.i);
  const char *status_str = get_status_str(status);
  row.write_string(status_str);
  row.write_uint32(Uint32(nodePtr.p->nodeRecoveryStatus));

  if (status == NodeRecord::NODE_ACTIVE)
  {
    handle_before_master(nodePtr, row);
    return;
  }
  if (status == NodeRecord::NODE_FAILED)
  {
    write_zero_columns(row, 19);
    return;
  }
  elapsed = NdbTick_Elapsed(nodePtr.p->nodeFailTime,
                            nodePtr.p->nodeFailCompletedTime).milliSec();
  elapsed/= 1000;
  /* Time to complete node failure */
  row.write_uint32(Uint32(elapsed));

  if (status == NodeRecord::NODE_FAILURE_COMPLETED)
  {
    write_zero_columns(row, 18);
    return;
  }
  elapsed = NdbTick_Elapsed(nodePtr.p->nodeFailCompletedTime,
                            nodePtr.p->allocatedNodeIdTime).milliSec();
  elapsed/= 1000;
  /* Time to allocate node id */
  row.write_uint32(Uint32(elapsed));

  if (status == NodeRecord::ALLOCATED_NODE_ID)
  {
    write_zero_columns(row, 17);
    return;
  }
  elapsed = NdbTick_Elapsed(nodePtr.p->allocatedNodeIdTime,
                            nodePtr.p->includedInHBProtocolTime).milliSec();
  elapsed/= 1000;
  /* Time to include in HB Protocol */
  row.write_uint32(Uint32(elapsed));

  if (status == NodeRecord::INCLUDED_IN_HB_PROTOCOL)
  {
    write_zero_columns(row, 16);
    return;
  }
  elapsed = NdbTick_Elapsed(nodePtr.p->includedInHBProtocolTime,
                            nodePtr.p->ndbcntrStartWaitTime).milliSec();
  elapsed/= 1000;
  /* Time until wait for for ndbcntr master */
  row.write_uint32(Uint32(elapsed));

  if (status == NodeRecord::NDBCNTR_START_WAIT)
  {
    write_zero_columns(row, 15);
    return;
  }
  elapsed = NdbTick_Elapsed(nodePtr.p->ndbcntrStartWaitTime,
                            nodePtr.p->ndbcntrStartedTime).milliSec();
  elapsed/= 1000;
  /* Time wait for NDBCNTR master */
  row.write_uint32(Uint32(elapsed));

  if (status == NodeRecord::NDBCNTR_STARTED)
  {
    write_zero_columns(row, 14);
    return;
  }
  elapsed = NdbTick_Elapsed(nodePtr.p->ndbcntrStartedTime,
                            nodePtr.p->startPermittedTime).milliSec();
  elapsed/= 1000;
  /* Time to get start permitted */
  row.write_uint32(Uint32(elapsed));

  if (status == NodeRecord::START_PERMITTED)
  {
    write_zero_columns(row, 13);
    return;
  }
  elapsed = NdbTick_Elapsed(nodePtr.p->startPermittedTime,
                            nodePtr.p->waitLCPToCopyDictTime).milliSec();
  elapsed/= 1000;
  /* Time to wait for LCP to copy meta data */
  row.write_uint32(Uint32(elapsed));

  if (status == NodeRecord::WAIT_LCP_TO_COPY_DICT)
  {
    write_zero_columns(row, 12);
    return;
  }
  elapsed = NdbTick_Elapsed(nodePtr.p->waitLCPToCopyDictTime,
                            nodePtr.p->copyDictToStartingNodeTime).milliSec();
  elapsed/= 1000;
  /* Time to copy meta data */
  row.write_uint32(Uint32(elapsed));

  if (status == NodeRecord::COPY_DICT_TO_STARTING_NODE)
  {
    write_zero_columns(row, 11);
    return;
  }
  elapsed = NdbTick_Elapsed(nodePtr.p->copyDictToStartingNodeTime,
                            nodePtr.p->includeNodeInLCPAndGCPTime).milliSec();
  elapsed/= 1000;
  /* Time to include node in GCP+LCP protocols */
  row.write_uint32(Uint32(elapsed));

  if (status == NodeRecord::INCLUDE_NODE_IN_LCP_AND_GCP)
  {
    write_zero_columns(row, 10);
    return;
  }
  elapsed = NdbTick_Elapsed(nodePtr.p->includeNodeInLCPAndGCPTime,
                            nodePtr.p->startDatabaseRecoveryTime).milliSec();
  elapsed/= 1000;
  /* Time for starting node to request local recovery */
  row.write_uint32(Uint32(elapsed));

  if (status == NodeRecord::LOCAL_RECOVERY_STARTED)
  {
    write_zero_columns(row, 9);
    return;
  }

  /* Total time of local recovery */
  if (status < NodeRecord::COPY_FRAGMENTS_STARTED)
  {
    row.write_uint32(Uint32(0));
  }
  else
  {
    elapsed = NdbTick_Elapsed(nodePtr.p->startDatabaseRecoveryTime,
                              nodePtr.p->copyFragmentsStartedTime).milliSec();
    elapsed/= 1000;
    row.write_uint32(Uint32(elapsed));
  }

  elapsed = NdbTick_Elapsed(nodePtr.p->startDatabaseRecoveryTime,
                            nodePtr.p->startUndoDDTime).milliSec();
  elapsed/= 1000;
  /* Time to restore fragments */
  row.write_uint32(Uint32(elapsed));

  if (status == NodeRecord::RESTORE_FRAG_COMPLETED)
  {
    write_zero_columns(row, 7);
    return;
  }
  elapsed = NdbTick_Elapsed(nodePtr.p->startUndoDDTime,
                            nodePtr.p->startExecREDOLogTime).milliSec();
  elapsed/= 1000;
  /* Time to UNDO disk data parts */
  row.write_uint32(Uint32(elapsed));

  if (status == NodeRecord::UNDO_DD_COMPLETED)
  {
    write_zero_columns(row, 6);
    return;
  }
  elapsed = NdbTick_Elapsed(nodePtr.p->startExecREDOLogTime,
                            nodePtr.p->startBuildIndexTime).milliSec();
  elapsed/= 1000;
  /* Time to execute REDO logs */
  row.write_uint32(Uint32(elapsed));

  if (status == NodeRecord::EXECUTE_REDO_LOG_COMPLETED)
  {
    write_zero_columns(row, 5);
    return;
  }
  elapsed = NdbTick_Elapsed(nodePtr.p->startBuildIndexTime,
                            nodePtr.p->copyFragmentsStartedTime).milliSec();
  elapsed/= 1000;
  /* Time to build indexes */
  row.write_uint32(Uint32(elapsed));

  if (status == NodeRecord::COPY_FRAGMENTS_STARTED)
  {
    write_zero_columns(row, 4);
    return;
  }
  elapsed = NdbTick_Elapsed(nodePtr.p->copyFragmentsStartedTime,
                            nodePtr.p->waitLCPForRestartTime).milliSec();
  elapsed/= 1000;
  /* Time to synchronize starting node with alive nodes */
  row.write_uint32(Uint32(elapsed));

  if (status == NodeRecord::WAIT_LCP_FOR_RESTART)
  {
    write_zero_columns(row, 3);
    return;
  }
  elapsed = NdbTick_Elapsed(nodePtr.p->waitLCPForRestartTime,
                            nodePtr.p->waitSumaHandoverTime).milliSec();
  elapsed/= 1000;
  /* Time to wait for completion of LCPs */
  row.write_uint32(Uint32(elapsed));

  if (status == NodeRecord::WAIT_SUMA_HANDOVER)
  {
    write_zero_columns(row, 2);
    return;
  }
  elapsed = NdbTick_Elapsed(nodePtr.p->waitSumaHandoverTime,
                            nodePtr.p->restartCompletedTime).milliSec();
  elapsed/= 1000;
  /* Time to handover subscriptions to starting node */
  row.write_uint32(Uint32(elapsed));

  elapsed = NdbTick_Elapsed(nodePtr.p->nodeFailTime,
                            nodePtr.p->restartCompletedTime).milliSec();
  elapsed/= 1000;
  /* Total recovery time */
  row.write_uint32(Uint32(elapsed));

  return;
}

void Dbdih::handle_before_master(NodeRecordPtr nodePtr,
                                 Ndbinfo::Row &row)
{
  Uint64 elapsed;

  /* Time to complete node failure */
  elapsed = NdbTick_Elapsed(nodePtr.p->nodeFailTime,
                            nodePtr.p->nodeFailCompletedTime).milliSec();
  elapsed/= 1000;
  row.write_uint32(Uint32(elapsed));

  /**
   * No report on
   * 1) Allocate node id
   * 2) Include in heartbeat protocol
   * 3) Wait for NDBCNTR master
   * 4) Time until ok from NDBCNTR master
   */
  row.write_uint32(Uint32(0));
  row.write_uint32(Uint32(0));
  row.write_uint32(Uint32(0));
  row.write_uint32(Uint32(0));

  /* Time to get from failure to start permitted */
  elapsed = NdbTick_Elapsed(nodePtr.p->nodeFailTime,
                            nodePtr.p->nodeGettingPermitTime).milliSec();
  elapsed/= 1000;
  row.write_uint32(Uint32(elapsed));

  /**
   * No report on
   * 1) Time to wait for LCP to copy meta data
   * 2) Time to copy meta data
   */
  row.write_uint32(Uint32(0));
  row.write_uint32(Uint32(0));

  /* Time from getting start permitted to getting included */
  elapsed = NdbTick_Elapsed(nodePtr.p->nodeGettingPermitTime,
                            nodePtr.p->nodeGettingIncludedTime).milliSec();
  elapsed/= 1000;
  row.write_uint32(Uint32(elapsed));

  /**
   * No report on
   * 1) Time for starting node to request local recovery
   */
  row.write_uint32(Uint32(0));

  /* Time for local recovery */
  elapsed = NdbTick_Elapsed(nodePtr.p->nodeGettingIncludedTime,
                            nodePtr.p->nodeGettingSynchedTime).milliSec();
  elapsed/= 1000;
  row.write_uint32(Uint32(elapsed));

  /**
   * No report on
   * 1) Restore fragment time
   * 2) UNDO DD time
   * 3) Execute REDO log time
   * 4) Build index time
   */
  row.write_uint32(Uint32(0));
  row.write_uint32(Uint32(0));
  row.write_uint32(Uint32(0));
  row.write_uint32(Uint32(0));

  /* Time to synchronize starting node with alive nodes */
  elapsed = NdbTick_Elapsed(nodePtr.p->nodeGettingSynchedTime,
                            nodePtr.p->nodeInLCPWaitStateTime).milliSec();
  elapsed/= 1000;
  row.write_uint32(Uint32(elapsed));

  /**
   * No report on
   * 1) Time to wait for LCP to be restorable as a node
   * 2) Time to handover subscriptions
   */
  row.write_uint32(Uint32(0));
  row.write_uint32(Uint32(0));

  /* Total time from node failure to node restarted */
  elapsed = NdbTick_Elapsed(nodePtr.p->nodeFailTime,
                            nodePtr.p->nodeActiveTime).milliSec();
  elapsed/= 1000;
  row.write_uint32(Uint32(elapsed));

  return;
}

void Dbdih::execDBINFO_SCANREQ(Signal *signal)
{
  DbinfoScanReq req = *(DbinfoScanReq*)signal->theData;
  const Ndbinfo::ScanCursor *cursor =
    CAST_CONSTPTR(Ndbinfo::ScanCursor, DbinfoScan::getCursorPtr(&req));
  Ndbinfo::Ratelimit rl;
  bool sent_any = false;
  jamEntry();

  switch (req.tableId)
  {
  case Ndbinfo::RESTART_INFO_TABLEID:
  {
    if (isMaster() == false)
    {
      /* Only report from master node's view on restarts */
      break;
    }
    if (getNodeState().startLevel != NodeState::SL_STARTED)
    {
      jam();
      /* Ignore when we are starting up or shutting down */
      break;
    }

    NodeRecordPtr nodePtr;
    jam();
    nodePtr.i = cursor->data[0];
    if (nodePtr.i == 0)
    {
      nodePtr.i = 1; /* Ignore node 0 */
    }
    else if (nodePtr.i >= MAX_NDB_NODES)
    {
      break;
    }
    for (; nodePtr.i < MAX_NDB_NODES; nodePtr.i++)
    {
      ptrAss(nodePtr, nodeRecord);
      if (nodePtr.p->nodeRecoveryStatus == NodeRecord::NODE_NOT_RESTARTED_YET ||
          nodePtr.p->nodeRecoveryStatus == NodeRecord::NOT_DEFINED_IN_CLUSTER)
        continue;
      jamLine(nodePtr.i);
      sent_any = true;
      Ndbinfo::Row row(signal, req);
      fill_row_with_node_restart_status(nodePtr, row);
      ndbinfo_send_row(signal, req, row, rl);
      if (rl.need_break(req))
      {
        jam();
        ndbinfo_send_scan_break(signal, req, rl, nodePtr.i + 1);
        return;
      }
    }
    if (cursor->data[0] == 0 && !sent_any)
    {
      /* No nodes had any node restart data to report */
      jam();
      break;
    }
    break;
  }
  case Ndbinfo::TABLE_DIST_STATUS_TABLEID:
  case Ndbinfo::TABLE_DIST_STATUS_ALL_TABLEID:
  {
    jam();
    TabRecordPtr tabPtr;
    tabPtr.i = cursor->data[0];
    if (!isMaster() && req.tableId == Ndbinfo::TABLE_DIST_STATUS_TABLEID)
    {
      jam();
      break;
    }
    for ( ; tabPtr.i < ctabFileSize ; tabPtr.i++)
    {
      jamLine(tabPtr.i);
      ptrAss(tabPtr, tabRecord);
      if (tabPtr.p->tabStatus != TabRecord::TS_IDLE)
      {
        jam();
        Ndbinfo::Row row(signal, req);
        row.write_uint32(cownNodeId);
        row.write_uint32(tabPtr.i);
        row.write_uint32(tabPtr.p->tabCopyStatus);
        row.write_uint32(tabPtr.p->tabUpdateState);
        row.write_uint32(tabPtr.p->tabLcpStatus);
        row.write_uint32(tabPtr.p->tabStatus);
        row.write_uint32(tabPtr.p->tabStorage);
        row.write_uint32(tabPtr.p->tableType);
        row.write_uint32(tabPtr.p->partitionCount);
        row.write_uint32(tabPtr.p->totalfragments);
        row.write_uint32(tabPtr.p->m_scan_count[0]);
        row.write_uint32(tabPtr.p->m_scan_count[1]);
        row.write_uint32(tabPtr.p->m_scan_reorg_flag);
        ndbinfo_send_row(signal, req, row, rl);
        if (rl.need_break(req))
        {
          jam();
          ndbinfo_send_scan_break(signal, req, rl, tabPtr.i + 1);
          return;
        }
      }
    }
    break;
  }
  case Ndbinfo::TABLE_FRAGMENTS_TABLEID:
  case Ndbinfo::TABLE_FRAGMENTS_ALL_TABLEID:
  {
    jam();
    TabRecordPtr tabPtr;
    FragmentstorePtr fragPtr;
    tabPtr.i = cursor->data[0] & 0xFFFF;
    Uint32 fragId = cursor->data[0] >> 16;
    if (!isMaster() && req.tableId == Ndbinfo::TABLE_FRAGMENTS_TABLEID)
    {
      jam();
      break;
    }
    for ( ; tabPtr.i < ctabFileSize ; tabPtr.i++)
    {
      jamLine(tabPtr.i);
      ptrAss(tabPtr, tabRecord);
      if (tabPtr.p->tabStatus != TabRecord::TS_IDLE &&
          (DictTabInfo::isTable(tabPtr.p->tableType) ||
           DictTabInfo::isUniqueIndex(tabPtr.p->tableType)))
      {
        for ( ; fragId < tabPtr.p->totalfragments ; fragId++)
        {
          jamLine(fragId);
          getFragstore(tabPtr.p, fragId, fragPtr);
          Ndbinfo::Row row(signal, req);
          row.write_uint32(cownNodeId);
          row.write_uint32(tabPtr.i);
          row.write_uint32(fragPtr.p->partition_id);
          row.write_uint32(fragPtr.p->fragId);
          if ((tabPtr.p->m_flags & TabRecord::TF_FULLY_REPLICATED) == 0)
          {
            row.write_uint32(0);
          }
          else
          {
            row.write_uint32(findPartitionOrder(tabPtr.p, fragPtr));
          }

          row.write_uint32(fragPtr.p->m_log_part_id);
          row.write_uint32(fragPtr.p->fragReplicas);
          row.write_uint32(fragPtr.p->activeNodes[0]);
          row.write_uint32(fragPtr.p->preferredPrimary);

          if (fragPtr.p->noStoredReplicas > 1)
          {
            row.write_uint32(fragPtr.p->activeNodes[1]);
          }
          else
          {
            row.write_uint32(0);
          }

          if (fragPtr.p->noStoredReplicas > 2)
          {
            row.write_uint32(fragPtr.p->activeNodes[2]);
          }
          else
          {
            row.write_uint32(0);
          }

          if (fragPtr.p->noStoredReplicas > 3)
          {
            row.write_uint32(fragPtr.p->activeNodes[3]);
          }
          else
          {
            row.write_uint32(0);
          }

          row.write_uint32(fragPtr.p->noStoredReplicas);
          row.write_uint32(fragPtr.p->noOldStoredReplicas);
          row.write_uint32(fragPtr.p->noLcpReplicas);
          ndbinfo_send_row(signal, req, row, rl);
          if (rl.need_break(req))
          {
            jam();
            Uint32 new_cursor = tabPtr.i + ((fragId + 1) << 16);
            ndbinfo_send_scan_break(signal, req, rl, new_cursor);
            return;
          }
        }
      }
      fragId = 0;
    }
    break;
  }
  case Ndbinfo::TABLE_REPLICAS_TABLEID:
  case Ndbinfo::TABLE_REPLICAS_ALL_TABLEID:
  {
    jam();
    TabRecordPtr tabPtr;
    FragmentstorePtr fragPtr;
    ReplicaRecordPtr replicaPtr;
    tabPtr.i = cursor->data[0] & 0xFFFF;
    Uint32 fragId = cursor->data[0] >> 16;
    if (!isMaster() && req.tableId == Ndbinfo::TABLE_REPLICAS_TABLEID)
    {
      jam();
      break;
    }
    for ( ; tabPtr.i < ctabFileSize ; tabPtr.i++)
    {
      jamLine(tabPtr.i);
      ptrAss(tabPtr, tabRecord);
      if (tabPtr.p->tabStatus != TabRecord::TS_IDLE &&
          (DictTabInfo::isTable(tabPtr.p->tableType) ||
           DictTabInfo::isUniqueIndex(tabPtr.p->tableType)))
      {
        jamLine(fragId);
        jamLine(tabPtr.p->totalfragments);
        jamLine(tabPtr.p->partitionCount);
        for ( ; fragId < tabPtr.p->totalfragments ; fragId++)
        {
          jamLine(fragId);
          getFragstore(tabPtr.p, fragId, fragPtr);
          for (Uint32 i = 0; i < 2; i++)
          {
            if (i == 0)
            {
              jam();
              replicaPtr.i = fragPtr.p->storedReplicas;
            }
            else
            {
              jam();
              replicaPtr.i = fragPtr.p->oldStoredReplicas;
            }
            while (replicaPtr.i != RNIL)
            {
              jam();
              Ndbinfo::Row row(signal, req);
              c_replicaRecordPool.getPtr(replicaPtr);
              row.write_uint32(cownNodeId);
              row.write_uint32(tabPtr.i);
              row.write_uint32(fragPtr.p->fragId);
              row.write_uint32(replicaPtr.p->initialGci);
              row.write_uint32(replicaPtr.p->procNode);
              row.write_uint32(replicaPtr.p->lcpOngoingFlag);
              row.write_uint32(replicaPtr.p->noCrashedReplicas);
              Uint32 lastId = 0;
              Uint32 maxLcpId = 0;
              for (Uint32 j = 0; j < MAX_LCP_USED; j++)
              {
                jam();
                if (replicaPtr.p->lcpStatus[j] == ZVALID)
                {
                  jam();
                  if (replicaPtr.p->lcpId[j] > maxLcpId)
                  {
                    jam();
                    lastId = j;
                    maxLcpId = replicaPtr.p->lcpId[j];
                  }
                }
              }
              Uint32 prevId = prevLcpNo(lastId);
              row.write_uint32(replicaPtr.p->maxGciStarted[lastId]);
              row.write_uint32(replicaPtr.p->maxGciCompleted[lastId]);
              row.write_uint32(replicaPtr.p->lcpId[lastId]);
              row.write_uint32(replicaPtr.p->maxGciStarted[prevId]);
              row.write_uint32(replicaPtr.p->maxGciCompleted[prevId]);
              row.write_uint32(replicaPtr.p->lcpId[prevId]);
              Uint32 last_replica_id = replicaPtr.p->noCrashedReplicas;
              row.write_uint32(replicaPtr.p->createGci[last_replica_id]);
              row.write_uint32(replicaPtr.p->replicaLastGci[last_replica_id]);
              row.write_uint32(i == 0 ? 1 : 0);
              ndbinfo_send_row(signal, req, row, rl);
              replicaPtr.i = replicaPtr.p->nextPool;
            }
          }
          if (rl.need_break(req))
          {
            jam();
            Uint32 new_cursor = tabPtr.i + ((fragId + 1) << 16);
            ndbinfo_send_scan_break(signal, req, rl, new_cursor);
            return;
          }
        }
        fragId = 0;
      }
    }
    break;
  }
  default:
    break;
  }
  ndbinfo_send_scan_conf(signal, req, rl);
}
/* END Node Recovery Status Module */

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
//    for this to happen. (This is not yet implemented).
//
// To support multiple node failures efficiently the code is written such that
// only one take over can handle transitions in state but during a copy 
// fragment other take over's can perform state transitions.
/*****************************************************************************/
void Dbdih::startTakeOver(Signal* signal,
                          Uint32 startNode,
                          Uint32 nodeTakenOver,
                          const StartCopyReq* req)
{
  jam();

  TakeOverRecordPtr takeOverPtr;

  ndbrequire(c_takeOverPool.seize(takeOverPtr));
  takeOverPtr.p->startGci = SYSFILE->lastCompletedGCI[startNode];
  takeOverPtr.p->restorableGci = SYSFILE->lastCompletedGCI[startNode];
  takeOverPtr.p->toStartingNode = startNode;
  takeOverPtr.p->toFailedNode = nodeTakenOver;
  takeOverPtr.p->toCurrentTabref = 0;
  takeOverPtr.p->toCurrentFragid = 0;

  ndbrequire(req != NULL);
  takeOverPtr.p->m_flags = req->flags;
  takeOverPtr.p->m_senderData = req->senderData;
  takeOverPtr.p->m_senderRef = req->senderRef;

  takeOverPtr.p->toSlaveStatus = TakeOverRecord::TO_START_FRAGMENTS;
  nr_start_fragments(signal, takeOverPtr);
}//Dbdih::startTakeOver()

void
Dbdih::nr_start_fragments(Signal* signal, 
			  TakeOverRecordPtr takeOverPtr)
{
  Uint32 loopCount = 0 ;
  TabRecordPtr tabPtr;
  const Uint32 MaxFragsToSearch = 100;
  while (loopCount++ < MaxFragsToSearch) {
    tabPtr.i = takeOverPtr.p->toCurrentTabref;
    if (tabPtr.i >= ctabFileSize) {
      jam();
      nr_run_redo(signal, takeOverPtr);
      return;
    }//if
    ptrAss(tabPtr, tabRecord);
    if (tabPtr.p->tabStatus != TabRecord::TS_ACTIVE ||
	tabPtr.p->tabStorage != TabRecord::ST_NORMAL)
    {
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
      continue;
    }//if
    FragmentstorePtr fragPtr;
    getFragstore(tabPtr.p, fragId, fragPtr);
    ReplicaRecordPtr loopReplicaPtr;
    loopReplicaPtr.i = fragPtr.p->oldStoredReplicas;
    while (loopReplicaPtr.i != RNIL) {
      c_replicaRecordPool.getPtr(loopReplicaPtr);
      if (loopReplicaPtr.p->procNode == takeOverPtr.p->toStartingNode) {
        jam();
	nr_start_fragment(signal, takeOverPtr, loopReplicaPtr);
        loopCount+= MaxFragsToSearch; /* Take a break */
	break;
      } else {
        jam();
        loopReplicaPtr.i = loopReplicaPtr.p->nextPool;
      }//if
    }//while
    takeOverPtr.p->toCurrentFragid++;
  }//while
  signal->theData[0] = DihContinueB::ZTO_START_FRAGMENTS;
  signal->theData[1] = takeOverPtr.i;
  sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
}

void
Dbdih::nr_start_fragment(Signal* signal, 
			 TakeOverRecordPtr takeOverPtr,
			 ReplicaRecordPtr replicaPtr)
{
  Uint32 i;
  Uint32 maxLcpId = 0;
  Uint32 maxLcpIndex = ~0;
  
  Uint32 gci = 0;
  Uint32 restorableGCI = takeOverPtr.p->restorableGci;

#if defined VM_TRACE || defined ERROR_INSERT
  ndbout_c("tab: %d frag: %d replicaP->nextLcp: %d",
	   takeOverPtr.p->toCurrentTabref,
	   takeOverPtr.p->toCurrentFragid,
	   replicaPtr.p->nextLcp);
#endif

  /**
   * Search for an LCP that can be used to restore.
   * For each LCP that is VALID we need to check if
   * it is restorable. It is restorable if the
   * node has a REDO log interval that can be used
   * to restore some GCI. For this to happen we have
   * to have a REDO log in the node that starts
   * before the last completed GCI in the LCP and that
   * goes on until at least until the maximum GCI
   * started in the LCP.
   */
  Uint32 idx = prevLcpNo(replicaPtr.p->nextLcp);
  for(i = 0; i<MAX_LCP_USED; i++, idx = prevLcpNo(idx))
  {
    Int32 j = replicaPtr.p->noCrashedReplicas - 1;
#if defined VM_TRACE || defined ERROR_INSERT
    ndbout_c("scanning idx: %d lcpId: %d crashed replicas: %u %s", 
             idx, replicaPtr.p->lcpId[idx],
             replicaPtr.p->noCrashedReplicas,
             replicaPtr.p->lcpStatus[idx] == ZVALID ? "VALID" : "NOT VALID");
#endif
    if (replicaPtr.p->lcpStatus[idx] == ZVALID) 
    {
      Uint32 startGci = replicaPtr.p->maxGciCompleted[idx] + 1;
      Uint32 stopGci = replicaPtr.p->maxGciStarted[idx];
#if defined VM_TRACE || defined ERROR_INSERT
      ndbout_c(" maxGciCompleted: %u maxGciStarted: %u", startGci - 1, stopGci);
#endif
      /* The following error insert is for Bug #23602217.
       * It ensures that the most recent LCP is considered
       * non-restorable. This forces the older LCP to be
       * restored, which failed to happen previously.
       */
      if (ERROR_INSERTED(7248))
      {
        g_eventLogger->info("Inserting error to skip most recent LCP");
        if (i == 0)
        {
          continue;
        }
      }
      for (; j>= 0; j--)
      {
#if defined VM_TRACE || defined ERROR_INSERT
	ndbout_c("crashed replica: %d(%d) replica(createGci: %u lastGci: %d )",
		 j, 
		 replicaPtr.p->noCrashedReplicas,
                 replicaPtr.p->createGci[j],
		 replicaPtr.p->replicaLastGci[j]);
#endif
	if (replicaPtr.p->createGci[j] <= startGci &&
            replicaPtr.p->replicaLastGci[j] >= stopGci)
	{
	  maxLcpId = replicaPtr.p->lcpId[idx];
	  maxLcpIndex = idx;
          gci = replicaPtr.p->replicaLastGci[j];
	  goto done;
	}
      }
    }
    else
    {
#if defined VM_TRACE || defined ERROR_INSERT
      ndbout_c(" ");
#endif
    }
  }
  
  idx = 2; // backward compat code
#if defined VM_TRACE || defined ERROR_INSERT
  ndbout_c("- scanning idx: %d lcpId: %d", idx, replicaPtr.p->lcpId[idx]);
#endif
  if (replicaPtr.p->lcpStatus[idx] == ZVALID) 
  {
    Uint32 startGci = replicaPtr.p->maxGciCompleted[idx] + 1;
    Uint32 stopGci = replicaPtr.p->maxGciStarted[idx];
    Int32 j = replicaPtr.p->noCrashedReplicas - 1;
    for (;j >= 0; j--)
    {
#if defined VM_TRACE || defined ERROR_INSERT
      ndbout_c("crashed replica: %d(%d) replica(createGci: %u lastGci: %d )",
               j, 
               replicaPtr.p->noCrashedReplicas,
               replicaPtr.p->createGci[j],
               replicaPtr.p->replicaLastGci[j]);
#endif
      if (replicaPtr.p->createGci[j] <= startGci &&
          replicaPtr.p->replicaLastGci[j] >= stopGci)
      {
        maxLcpId = replicaPtr.p->lcpId[idx];
        maxLcpIndex = idx;
        gci = replicaPtr.p->replicaLastGci[j];
        goto done;
      }
    }
  }
  
done:
  
  StartFragReq *req = (StartFragReq *)signal->getDataPtrSend();
  req->requestInfo = StartFragReq::SFR_RESTORE_LCP;
  if (maxLcpIndex == ~ (Uint32) 0)
  {
    /**
     * we didn't find a local LCP that we can restore
     */
    jam();
    ndbassert(gci == 0);
    replicaPtr.p->m_restorable_gci = gci;

    req->userPtr = 0;
    req->userRef = reference();
    req->lcpNo = ZNIL;
    req->lcpId = 0;
    req->tableId = takeOverPtr.p->toCurrentTabref;
    req->fragId = takeOverPtr.p->toCurrentFragid;
    req->noOfLogNodes = 0;

    if (c_2pass_inr && cstarttype == NodeState::ST_INITIAL_NODE_RESTART)
    {
      /**
       * Check if we can make 2-phase copy
       *   1) non-transaction, (after we rebuild indexes)
       *   2) transaction (maintaining indexes during rebuild)
       *      where the transactional copies efterything >= startGci
       *
       * NOTE: c_2pass_inr is only set if all nodes in cluster currently
       *       supports this
       */

      if (takeOverPtr.p->startGci == 0)
      {
        jam();
        /**
         * Set a startGci to currently lastCompletedGCI of master
         *   any value will do...as long as subsequent transactional copy
         *   will be using it (scanning >= this value)
         */
        takeOverPtr.p->startGci = SYSFILE->lastCompletedGCI[cmasterNodeId];
      }

      TabRecordPtr tabPtr;
      tabPtr.i = takeOverPtr.p->toCurrentTabref;
      ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);

      FragmentstorePtr fragPtr;
      getFragstore(tabPtr.p, takeOverPtr.p->toCurrentFragid, fragPtr);
      Uint32 nodes[MAX_REPLICAS];
      extractNodeInfo(jamBuffer(), fragPtr.p, nodes);

      req->lqhLogNode[0] = nodes[0]; // Source
      req->requestInfo = StartFragReq::SFR_COPY_FRAG;
      replicaPtr.p->m_restorable_gci = takeOverPtr.p->startGci;
    }

    if (req->requestInfo == StartFragReq::SFR_RESTORE_LCP)
    {
      g_eventLogger->debug("node: %d tab: %d frag: %d no lcp to restore",
                           takeOverPtr.p->toStartingNode,
                           takeOverPtr.p->toCurrentTabref,
                           takeOverPtr.p->toCurrentFragid);
    }
    else
    {
      g_eventLogger->debug("node: %d tab: %d frag: %d copying data from %u"
                           " (gci: %u)",
                           takeOverPtr.p->toStartingNode,
                           takeOverPtr.p->toCurrentTabref,
                           takeOverPtr.p->toCurrentFragid,
                           req->lqhLogNode[0],
                           takeOverPtr.p->startGci);
    }

    BlockReference ref = numberToRef(DBLQH, takeOverPtr.p->toStartingNode);
    sendSignal(ref, GSN_START_FRAGREQ, signal, 
	       StartFragReq::SignalLength, JBB);
  }
  else
  {
    jam();
    if (gci != restorableGCI)
    {
      Ptr<TabRecord> tabPtr;
      tabPtr.i = takeOverPtr.p->toCurrentTabref;
      ptrAss(tabPtr, tabRecord);

      FragmentstorePtr fragPtr;
      getFragstore(tabPtr.p, takeOverPtr.p->toCurrentFragid, fragPtr);
      dump_replica_info(fragPtr.p);
    }
    ndbassert(gci == restorableGCI);
    replicaPtr.p->m_restorable_gci = gci;
    Uint32 startGci = replicaPtr.p->maxGciCompleted[maxLcpIndex] + 1;
    if (startGci > gci)
      startGci = gci;
    g_eventLogger->debug("Requesting start of fragment: "
             "node: %d tab: %d frag: %d restore lcp: %u(idx: %u)"
             " maxGciStarted: %u maxGciCompleted: %u (restorable:"
             " %u(%u) newestRestorableGCI: %u)",
             takeOverPtr.p->toStartingNode,
             takeOverPtr.p->toCurrentTabref,
             takeOverPtr.p->toCurrentFragid,
	     maxLcpId,
             maxLcpIndex,
	     replicaPtr.p->maxGciStarted[maxLcpIndex],
	     replicaPtr.p->maxGciCompleted[maxLcpIndex],
	     restorableGCI,
	     SYSFILE->lastCompletedGCI[takeOverPtr.p->toStartingNode],
	     SYSFILE->newestRestorableGCI);

    StartFragReq *req = (StartFragReq *)signal->getDataPtrSend();
    req->userPtr = 0;
    req->userRef = reference();
    req->lcpNo = maxLcpIndex;
    req->lcpId = maxLcpId;
    req->tableId = takeOverPtr.p->toCurrentTabref;
    req->fragId = takeOverPtr.p->toCurrentFragid;
    req->noOfLogNodes = 1;
    req->lqhLogNode[0] = takeOverPtr.p->toStartingNode;
    req->startGci[0] = startGci;
    req->lastGci[0] = gci;

    BlockReference ref = numberToRef(DBLQH, takeOverPtr.p->toStartingNode);
    sendSignal(ref, GSN_START_FRAGREQ, signal, 
	       StartFragReq::SignalLength, JBB);

    if (startGci < takeOverPtr.p->startGci)
    {
      jam();
      takeOverPtr.p->startGci = startGci;
    }
  }
}

void
Dbdih::nr_run_redo(Signal* signal, TakeOverRecordPtr takeOverPtr)
{
  /**
   * sendSTART_RECREQ uses m_sr_nodes
   *   and for TO during SR, we don't want to modify it
   *   so save/restore it
   */
  NdbNodeBitmask save = m_sr_nodes;
  m_sr_nodes.clear();
  m_sr_nodes.set(takeOverPtr.p->toStartingNode);

  Uint32 save_keepGCI = SYSFILE->keepGCI;
  if (takeOverPtr.p->startGci < SYSFILE->keepGCI)
  {
    jam();
    SYSFILE->keepGCI = takeOverPtr.p->startGci;
    g_eventLogger->info("GSN_START_RECREQ keepGci: %u (%u)",
                        takeOverPtr.p->startGci, save_keepGCI);
  }

  g_eventLogger->info("All start fragments sent, requesting LDM to restore"
                      " all fragments and to execute the REDO log to bring"
                      " the database to an off-line but consistent state");

  takeOverPtr.p->toCurrentTabref = 0;
  takeOverPtr.p->toCurrentFragid = 0;
  takeOverPtr.p->toSlaveStatus = TakeOverRecord::TO_RUN_REDO;
  sendSTART_RECREQ(signal, takeOverPtr.p->toStartingNode, takeOverPtr.i);

  m_sr_nodes = save; // restore
  SYSFILE->keepGCI = save_keepGCI;
}

void
Dbdih::nr_start_logging(Signal* signal, TakeOverRecordPtr takeOverPtr)
{
  Uint32 loopCount = 0 ;
  TabRecordPtr tabPtr;
  while (loopCount++ < 100)
  {
    tabPtr.i = takeOverPtr.p->toCurrentTabref;
    if (tabPtr.i >= ctabFileSize)
    {
      jam();
      g_eventLogger->debug("Copy thread %u complete",
                          takeOverPtr.p->m_copy_thread_id);
      if (!thread_takeover_completed(signal, takeOverPtr))
      {
        jam();
        return;
      }
      check_take_over_completed_correctly();
      g_eventLogger->info("Make On-line Database recoverable by waiting"
                          " for LCP Starting, all parallel threads have"
                          " now ceased their activity and we have a single"
                          " wait state here");

      takeOverPtr = c_mainTakeOverPtr;

      takeOverPtr.p->toSlaveStatus = TakeOverRecord::TO_END_TO;
      EndToReq* req = (EndToReq*)signal->getDataPtrSend();
      req->senderData = takeOverPtr.i;
      req->senderRef = reference();
      req->flags = takeOverPtr.p->m_flags;
      sendSignal(cmasterdihref, GSN_END_TOREQ,
                 signal, EndToReq::SignalLength, JBB);
      sendEND_TOREP(signal, takeOverPtr.p->toStartingNode);
      return;
    }
    ptrAss(tabPtr, tabRecord);
    if (tabPtr.p->tabStatus != TabRecord::TS_ACTIVE ||
	tabPtr.p->tabStorage != TabRecord::ST_NORMAL)
    {
      jam();
      takeOverPtr.p->toCurrentFragid = 0;
      takeOverPtr.p->toCurrentTabref++;
      continue;
    }

    Uint32 fragId = takeOverPtr.p->toCurrentFragid;
    if (fragId >= tabPtr.p->totalfragments)
    {
      jam();
      takeOverPtr.p->toCurrentFragid = 0;
      takeOverPtr.p->toCurrentTabref++;
      continue;
    }
    FragmentstorePtr fragPtr;
    getFragstore(tabPtr.p, fragId, fragPtr);

    Uint32 instanceKey = dihGetInstanceKey(fragPtr);
    if (!check_takeover_thread(takeOverPtr,
                               fragPtr,
                               instanceKey))
    {
      jam();
      /**
       * We are scanning for fragment replicas to take over, but this replica
       * was not ours to take over, it will be handled by another take over
       * thread.
       */
      takeOverPtr.p->toCurrentFragid++;
      continue;
    }

    ReplicaRecordPtr loopReplicaPtr;
    loopReplicaPtr.i = fragPtr.p->storedReplicas;
    while (loopReplicaPtr.i != RNIL)
    {
      c_replicaRecordPool.getPtr(loopReplicaPtr);
      if (loopReplicaPtr.p->procNode == takeOverPtr.p->toStartingNode)
      {
        jam();
        ndbrequire(loopReplicaPtr.p->procNode == getOwnNodeId());
        takeOverPtr.p->toSlaveStatus = TakeOverRecord::TO_SL_COPY_ACTIVE;

        BlockReference lqhRef = numberToRef(DBLQH, instanceKey,
                                            takeOverPtr.p->toStartingNode);

        CopyActiveReq * const req = (CopyActiveReq *)&signal->theData[0];
        req->userPtr = takeOverPtr.i;
        req->userRef = reference();
        req->tableId = takeOverPtr.p->toCurrentTabref;
        req->fragId = takeOverPtr.p->toCurrentFragid;
        req->distributionKey = fragPtr.p->distributionKey;
        req->flags = 0;
        sendSignal(lqhRef, GSN_COPY_ACTIVEREQ, signal,
                   CopyActiveReq::SignalLength, JBB);
        return;
      }
      else
      {
        jam();
        loopReplicaPtr.i = loopReplicaPtr.p->nextPool;
      }
    }
    takeOverPtr.p->toCurrentFragid++;
  }
  send_continueb_nr_start_logging(signal, takeOverPtr);
}

/**
 * Instance takeover uses a number of queues and variables to keep track of
 * the takeover threads.
 *
 * We start by sending START_TOREQ to the master. This is done by the
 * main takeover record. This is always placed in the variable
 * c_mainTakeOverPtr.
 *
 * After this we create a number of parallel threads. A record is created
 * and put into the queue:
 * c_activeTakeOverList
 * It stays there while we're scanning for fragments to take over in our
 * takeover thread.
 *
 * When we find an instance to take over we have two possibilities.
 * We can either be put into the active thread which is the variable:
 * c_activeThreadTakeOverPtr
 * If the active thread is already busy, then we are placed into the
 * queue:
 * c_queued_for_start_takeover_list
 * When we're taken out of the queue we are placed into the active thread.
 *
 * We are taken out of the active thread when we're sending COPY_FRAGREQ.
 * At this point our takeover thread is placed in the list
 * c_active_copy_threads_list
 * It stays in this list until we're done with the copying when we have
 * received COPY_ACTIVECONF back from the LDM instance in the starting node.
 *
 * At this point we need to update the fragment state again and we need to
 * become active thread again which is controlled by:
 * c_activeThreadTakeOverPtr
 * If the active thread is already busy then we use the queue
 * c_queued_for_commit_takeover_list
 * This queue has higher priority than the
 * c_queued_for_start_takeover_list
 *
 * After completing the update of the fragment state we are removed as active
 * thread and placed back in the list
 * c_activeTakeOverList
 * 
 * We proceed with the next fragment until we're out of fragments to handle
 * for this thread.
 *
 * At this point we are removed from
 * c_activeTakeOverList
 * and placed into
 * c_completed_copy_threads_list
 *
 * If this was a system restart we will then remove all threads from the
 * c_completed_copy_threads_list
 * and only the
 * c_mainTakeOverPtr
 * record still remains.
 *
 * For normal node recovery we start a process of activating the node. We
 * start this process by removing the takeover thread from
 * c_completed_copy_threads_list
 * and placing the takeover thread into the list
 * c_active_copy_threads_list
 * instead.
 *
 * At every point when we need to update the fragment state we remove the
 * takeover record from the
 * c_active_copy_threads_list
 * and place it as the active thread record. If the active thread is
 * already busy then we place the record in the list
 * c_queued_for_commit_takeover_list
 *
 * After completing the update of the fragment state we place the record
 * back into the list
 * c_active_copy_threads_list
 *
 * When we are finally done with activating the node instance in this final
 * process, then we're removing the record from the
 * c_active_copy_threads_list
 * and releasing the takeover thread record to the take over pool.
 *
 * When all node instances are completed then all lists should be empty and
 * no thread should be active and only the main record should remain.
 */


void
Dbdih::sendStartTo(Signal* signal, TakeOverRecordPtr takeOverPtr)
{
  takeOverPtr.p->toSlaveStatus = TakeOverRecord::TO_START_TO;
  
  StartToReq* req = (StartToReq*)signal->getDataPtrSend();
  req->senderData = takeOverPtr.i;
  req->senderRef = reference();
  req->startingNodeId = takeOverPtr.p->toStartingNode;
  sendSignal(cmasterdihref, GSN_START_TOREQ, 
             signal, StartToReq::SignalLength, JBB);
}

void
Dbdih::execSTART_TOREF(Signal* signal)
{
  jamEntry();

  StartToRef* ref = (StartToRef*)signal->getDataPtr();
  Uint32 errCode = ref->errorCode;
  (void)errCode; // TODO check for "valid" error

  TakeOverRecordPtr takeOverPtr;
  c_takeOverPool.getPtr(takeOverPtr, ref->senderData);
  
  signal->theData[0] = DihContinueB::ZSEND_START_TO;
  signal->theData[1] = takeOverPtr.i;
  
  sendSignalWithDelay(reference(), GSN_CONTINUEB,
                      signal, 5000, 2);
}

/**
 * We have completed one thread's communication with the master and we're
 * ready to start off another which have been queued.
 */
void
Dbdih::start_next_takeover_thread(Signal *signal)
{
  TakeOverRecordPtr takeOverPtr;
  bool dequeued_from_commit_take_over = true;
  bool dequeued_from_start_take_over = false;

  if (!c_queued_for_commit_takeover_list.removeFirst(takeOverPtr))
  {
    dequeued_from_commit_take_over = false;
    if (!c_queued_for_start_takeover_list.removeFirst(takeOverPtr))
    {
      jam();
      /**
       * No threads are queued up for master communication, so we can
       * set active to RNIL and wait for the next thread to be completed
       * with another step.
       */
      g_eventLogger->debug("No threads queued up");
      c_activeThreadTakeOverPtr.i = RNIL;
      return;
    }
    dequeued_from_start_take_over = true;
    jam();
  }
  c_activeThreadTakeOverPtr = takeOverPtr;
  g_eventLogger->debug("New active takeover thread: %u, state: %u",
                      takeOverPtr.i,
                      takeOverPtr.p->toSlaveStatus);
  if (takeOverPtr.p->toSlaveStatus ==
        TakeOverRecord::TO_QUEUED_UPDATE_BEFORE_STORED)
  {
    jam();
    ndbrequire(dequeued_from_start_take_over);
    takeOverPtr.p->toSlaveStatus = TakeOverRecord::TO_UPDATE_BEFORE_STORED;
    sendUpdateTo(signal, takeOverPtr);
  }
  else if (takeOverPtr.p->toSlaveStatus ==
             TakeOverRecord::TO_QUEUED_UPDATE_BEFORE_COMMIT)
  {
    jam();
    ndbrequire(dequeued_from_commit_take_over);
    takeOverPtr.p->toSlaveStatus = TakeOverRecord::TO_UPDATE_BEFORE_COMMIT;
    sendUpdateTo(signal, takeOverPtr);
  }
  else if (takeOverPtr.p->toSlaveStatus ==
             TakeOverRecord::TO_QUEUED_SL_UPDATE_FRAG_STATE)
  {
    jam();
    ndbrequire(dequeued_from_commit_take_over);
    takeOverPtr.p->toSlaveStatus = TakeOverRecord::TO_SL_UPDATE_FRAG_STATE;
    sendUpdateFragStateReq(signal,
                           takeOverPtr.p->startGci,
                           UpdateFragStateReq::START_LOGGING,
                           takeOverPtr);
    return;
  }
  else
  {
    ndbrequire(false);
  }
  return;
}

void
Dbdih::init_takeover_thread(TakeOverRecordPtr takeOverPtr,
                            TakeOverRecordPtr mainTakeOverPtr,
                            Uint32 number_of_copy_threads,
                            Uint32 thread_id)
{
  c_activeTakeOverList.addFirst(takeOverPtr);
  takeOverPtr.p->m_copy_thread_id = thread_id;
  takeOverPtr.p->m_number_of_copy_threads = number_of_copy_threads;

  takeOverPtr.p->m_flags = mainTakeOverPtr.p->m_flags;
  takeOverPtr.p->m_senderData = mainTakeOverPtr.p->m_senderData;
  takeOverPtr.p->m_senderRef = mainTakeOverPtr.p->m_senderRef;

  takeOverPtr.p->startGci = mainTakeOverPtr.p->startGci;
  takeOverPtr.p->restorableGci = mainTakeOverPtr.p->restorableGci;
  /* maxPage is received in PREPARE_COPY_FRAGCONF */

  takeOverPtr.p->toCopyNode = mainTakeOverPtr.p->toCopyNode;
  takeOverPtr.p->toFailedNode = mainTakeOverPtr.p->toFailedNode;
  takeOverPtr.p->toStartingNode = mainTakeOverPtr.p->toStartingNode;

  takeOverPtr.p->toStartTime = mainTakeOverPtr.p->toStartTime;
  takeOverPtr.p->toSlaveStatus = TakeOverRecord::TO_SELECTING_NEXT;
  takeOverPtr.p->toMasterStatus = TakeOverRecord::TO_MASTER_IDLE;

  takeOverPtr.p->toCurrentTabref = 0;
  takeOverPtr.p->toCurrentFragid = 0;
  takeOverPtr.p->toCurrentReplica = RNIL;
}

void
Dbdih::send_continueb_start_next_copy(Signal *signal,
                                      TakeOverRecordPtr takeOverPtr)
{
  signal->theData[0] = DihContinueB::ZTO_START_COPY_FRAG;
  signal->theData[1] = takeOverPtr.i;
  sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
}

void
Dbdih::execSTART_TOCONF(Signal* signal)
{
  jamEntry();
  StartToConf * conf = (StartToConf*)signal->getDataPtr();

  TakeOverRecordPtr takeOverPtr;
  c_takeOverPool.getPtr(takeOverPtr, conf->senderData);

  CRASH_INSERTION(7133);

  /**
   * We are now allowed to start copying
   *
   * It is time to start the parallelisation phase where we have a number
   * of take over threads where each take over thread takes care of
   * a set of LDM instances. This means that each take over thread can
   * execute in parallel towards DBLQH, but we have to serialise access
   * towards the master which is designed to handle one take over thread
   * request per node at a time. So we handle multiple take overs internally
   * and towards the LDM instances, but towards the master we appear as there
   * is only one take over thread.
   *
   * This means that we need no master specific take over code to parallelize
   * copying over several LDM instances. The take over can be made parallel as
   * soon as a version with this code is started as long as the master can
   * handle parallel node recovery in general.
   */

  c_mainTakeOverPtr = takeOverPtr;
  c_mainTakeOverPtr.p->m_number_of_copy_threads =
    c_max_takeover_copy_threads;
  c_mainTakeOverPtr.p->m_copy_threads_completed = 0;
  c_activeThreadTakeOverPtr.i = RNIL;
  check_take_over_completed_correctly();

  for (Uint32 i = 0; i < c_max_takeover_copy_threads; i++)
  {
    /**
     * We will break the rule of not starting more than 4 signals from one
     * signal here. The reason is that we know that eventually we will start
     * the same number of parallel threads anyways and also there won't be
     * anymore parallelisation after that internally in this thread. There
     * could potentially be further parallelisation in DBLQH, but this is
     * in a number of parallel threads and thus not DIH's concern to handle.
     */
    jam();
    ndbrequire(c_takeOverPool.seize(takeOverPtr));
    init_takeover_thread(takeOverPtr,
                         c_mainTakeOverPtr,
                         c_max_takeover_copy_threads,
                         i);
    send_continueb_start_next_copy(signal, takeOverPtr);
  }
}

bool
Dbdih::check_takeover_thread(TakeOverRecordPtr takeOverPtr,
                             FragmentstorePtr fragPtr,
                             Uint32 fragmentReplicaInstanceKey)
{
  ndbassert(fragmentReplicaInstanceKey != 0);
  fragmentReplicaInstanceKey--;
  /**
   * The instance key is in reality the log part id. The log part id
   * is often in ndbmtd the same as the instance id. But in ndbd and
   * in ndbmtd with 2 LDM instances there is a difference. The
   * instance id is mapped in the receiving node modulo the number
   * of LDM instances. So we take the instance key modulo the number
   * of LDM instances to get the thread id to handle this takeover
   * thread.
   *
   * For safety we will never run more parallelism than we have in the
   * minimum node of the starting node and the copying node.
   */
  Uint32 nodes[MAX_REPLICAS];
  extractNodeInfo(jamBuffer(), fragPtr.p, nodes);
  Uint32 lqhWorkers = getNodeInfo(takeOverPtr.p->toStartingNode).m_lqh_workers;
  lqhWorkers = MIN(lqhWorkers,
                   getNodeInfo(nodes[0]).m_lqh_workers);
  lqhWorkers = MAX(lqhWorkers, 1);
  Uint32 instanceId = fragmentReplicaInstanceKey % lqhWorkers;

  if (getNodeInfo(refToNode(cmasterdihref)).m_version <
      NDBD_SUPPORT_PARALLEL_SYNCH)
  {
    jam();
    /**
     * The master node has no support to receive multiple requests to copy a
     * fragment on the same node group. We fix this by ensuring that we only
     * use one thread in the parallel copy scheme.
     */
    instanceId = 0;
  }
  if ((instanceId % takeOverPtr.p->m_number_of_copy_threads) ==
      takeOverPtr.p->m_copy_thread_id)
  {
    jam();
    return true;
  }
  else
  {
    jam();
    return false;
  }
}

void Dbdih::startNextCopyFragment(Signal* signal, Uint32 takeOverPtrI)
{
  TabRecordPtr tabPtr;
  TakeOverRecordPtr takeOverPtr;
  c_takeOverPool.getPtr(takeOverPtr, takeOverPtrI);

  Uint32 loopCount;
  loopCount = 0;
  if (ERROR_INSERTED(7159)) {
    loopCount = 100;
  }//if
  while (loopCount++ < 100) {
    tabPtr.i = takeOverPtr.p->toCurrentTabref;
    if (tabPtr.i >= ctabFileSize) {
      jam();
      CRASH_INSERTION(7136);
      toCopyCompletedLab(signal, takeOverPtr);
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

    Uint32 instanceKey = dihGetInstanceKey(fragPtr);
    if (!check_takeover_thread(takeOverPtr,
                               fragPtr,
                               instanceKey))
    {
      /**
       * We are scanning for fragment replicas to take over, but this replica
       * was not ours to take over, it will be handled by another take over
       * thread.
       */
      jam();
      takeOverPtr.p->toCurrentFragid++;
      continue;
    }
    jam();

    ReplicaRecordPtr loopReplicaPtr;
    loopReplicaPtr.i = fragPtr.p->oldStoredReplicas;
    while (loopReplicaPtr.i != RNIL) {
      c_replicaRecordPool.getPtr(loopReplicaPtr);
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
	/* WE NEED TO COMPLETE THE TAKE OVER OF THIS REPLICA.                */
	/* ----------------------------------------------------------------- */
        takeOverPtr.p->toCurrentReplica = loopReplicaPtr.i;
        toCopyFragLab(signal, takeOverPtr.i);
        return;
      } else {
        jam();
        loopReplicaPtr.i = loopReplicaPtr.p->nextPool;
      }//if
    }//while
    takeOverPtr.p->toCurrentFragid++;
  }//while
  send_continueb_start_next_copy(signal, takeOverPtr);
}//Dbdih::startNextCopyFragment()

void Dbdih::toCopyFragLab(Signal* signal, Uint32 takeOverPtrI) 
{
  TakeOverRecordPtr takeOverPtr;
  c_takeOverPool.getPtr(takeOverPtr, takeOverPtrI);

  /**
   * Inform starting node that TakeOver is about to start
   */
  g_eventLogger->debug("PREPARE_COPY_FRAGREQ: tab: %u, frag: %u, thread: %u",
    takeOverPtr.p->toCurrentTabref,
    takeOverPtr.p->toCurrentFragid,
    takeOverPtr.i);
  TabRecordPtr tabPtr;
  tabPtr.i = takeOverPtr.p->toCurrentTabref;
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
  
  FragmentstorePtr fragPtr;
  getFragstore(tabPtr.p, takeOverPtr.p->toCurrentFragid, fragPtr);
  Uint32 nodes[MAX_REPLICAS];
  extractNodeInfo(jamBuffer(), fragPtr.p, nodes);
  takeOverPtr.p->toCopyNode = nodes[0];
  
  PrepareCopyFragReq* req= (PrepareCopyFragReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->senderData = takeOverPtrI;
  req->tableId = takeOverPtr.p->toCurrentTabref;
  req->fragId = takeOverPtr.p->toCurrentFragid;
  req->copyNodeId = takeOverPtr.p->toCopyNode;
  req->startingNodeId = takeOverPtr.p->toStartingNode; // Dst

  Uint32 instanceKey = dihGetInstanceKey(req->tableId, req->fragId);
  Uint32 ref = numberToRef(DBLQH, instanceKey, takeOverPtr.p->toStartingNode);
  
  sendSignal(ref, GSN_PREPARE_COPY_FRAG_REQ, signal, 
             PrepareCopyFragReq::SignalLength, JBB);
  
  takeOverPtr.p->toSlaveStatus = TakeOverRecord::TO_PREPARE_COPY;
}

void
Dbdih::execPREPARE_COPY_FRAG_REF(Signal* signal)
{
  jamEntry();
  PrepareCopyFragRef ref = *(PrepareCopyFragRef*)signal->getDataPtr();

  TakeOverRecordPtr takeOverPtr;
  c_takeOverPool.getPtr(takeOverPtr, ref.senderData);

  ndbrequire(takeOverPtr.p->toSlaveStatus == TakeOverRecord::TO_PREPARE_COPY);
  
  /**
   * Treat this as copy frag ref
   */
  CopyFragRef * cfref = (CopyFragRef*)signal->getDataPtrSend();
  cfref->userPtr = ref.senderData;
  cfref->startingNodeId = ref.startingNodeId;
  cfref->errorCode = ref.errorCode;
  cfref->tableId = ref.tableId;
  cfref->fragId = ref.fragId;
  cfref->sendingNodeId = ref.copyNodeId;
  takeOverPtr.p->toSlaveStatus = TakeOverRecord::TO_COPY_FRAG;
  execCOPY_FRAGREF(signal);
}

void
Dbdih::execPREPARE_COPY_FRAG_CONF(Signal* signal)
{
  jamEntry();
  PrepareCopyFragConf conf = *(PrepareCopyFragConf*)signal->getDataPtr();

  TakeOverRecordPtr takeOverPtr;
  c_takeOverPool.getPtr(takeOverPtr, conf.senderData);

  Uint32 version = getNodeInfo(refToNode(conf.senderRef)).m_version;
  ndbrequire(ndb_check_prep_copy_frag_version(version) >= 2);
  takeOverPtr.p->maxPage = conf.maxPageNo;

  c_activeTakeOverList.remove(takeOverPtr);

  if (c_activeThreadTakeOverPtr.i != RNIL)
  {
    /**
     * There is already an active take over thread that is performing an
     * update of its fragment replica state through the master. We will
     * put ourselves in the c_queued_for_start_take_over_list and be
     * started as soon as possible.
     */
    jam();
    g_eventLogger->debug("QUEUED_UPDATE_BEFORE_STORED, inst: %u",
                         takeOverPtr.i);
    takeOverPtr.p->toSlaveStatus =
      TakeOverRecord::TO_QUEUED_UPDATE_BEFORE_STORED;
    c_queued_for_start_takeover_list.addLast(takeOverPtr);
    return;
  }
  /* Mark master busy before proceeding */
  c_activeThreadTakeOverPtr = takeOverPtr;

  /**
   * We need to lock fragment info...in order to later run
   * UPDATE_FRAG_STATEREQ. We will mark ourselves as the active thread
   * such that other threads will be queued up until we are ready with
   * updating the fragment state.
   */
  takeOverPtr.p->toSlaveStatus = TakeOverRecord::TO_UPDATE_BEFORE_STORED;
  g_eventLogger->debug("PREPARE_COPY_FRAG_CONF: thread: %u", takeOverPtr.i);
  sendUpdateTo(signal, takeOverPtr);
}

void
Dbdih::sendUpdateTo(Signal* signal, TakeOverRecordPtr takeOverPtr)
{
  /**
   * We must refer to the main takeover thread towards the master node,
   * but we take the data from the thread which is currently active.
   */
  g_eventLogger->debug("UPDATE_TOREQ: tab:%u, frag:%u, thread:%u, state:%u",
    takeOverPtr.p->toCurrentTabref,
    takeOverPtr.p->toCurrentFragid,
    takeOverPtr.i,
    takeOverPtr.p->toSlaveStatus);
  UpdateToReq* req = (UpdateToReq*)signal->getDataPtrSend();
  req->senderData = c_mainTakeOverPtr.i;
  req->senderRef = reference();
  req->startingNodeId = takeOverPtr.p->toStartingNode;
  req->copyNodeId = takeOverPtr.p->toCopyNode;
  req->tableId = takeOverPtr.p->toCurrentTabref;
  req->fragmentNo = takeOverPtr.p->toCurrentFragid;
  switch(takeOverPtr.p->toSlaveStatus){
  case TakeOverRecord::TO_UPDATE_BEFORE_STORED:
    jam();
    req->requestType = UpdateToReq::BEFORE_STORED;
    break;
  case TakeOverRecord::TO_UPDATE_AFTER_STORED:
    req->requestType = UpdateToReq::AFTER_STORED;
    break;
  case TakeOverRecord::TO_UPDATE_BEFORE_COMMIT:
    jam();
    req->requestType = UpdateToReq::BEFORE_COMMIT_STORED;
    break;
  case TakeOverRecord::TO_UPDATE_AFTER_COMMIT:
    jam();
    req->requestType = UpdateToReq::AFTER_COMMIT_STORED;
    break;
  default:
    jamLine(takeOverPtr.p->toSlaveStatus);
    ndbrequire(false);
  }
  sendSignal(cmasterdihref, GSN_UPDATE_TOREQ, 
             signal, UpdateToReq::SignalLength, JBB);
}

void
Dbdih::execUPDATE_TOREF(Signal* signal)
{
  jamEntry();
  UpdateToRef* ref = (UpdateToRef*)signal->getDataPtr();
  Uint32 errCode = ref->errorCode;
  (void)errCode; // TODO check for "valid" error

  TakeOverRecordPtr takeOverPtr;

  ndbrequire(ref->senderData == c_mainTakeOverPtr.i);
  ndbrequire(c_activeThreadTakeOverPtr.i != RNIL);

  c_takeOverPool.getPtr(takeOverPtr, c_activeThreadTakeOverPtr.i);

  g_eventLogger->info("UPDATE_TOREF: thread: %u, state:%u",
                      takeOverPtr.i,
                      takeOverPtr.p->toSlaveStatus);
  signal->theData[0] = DihContinueB::ZSEND_UPDATE_TO;
  signal->theData[1] = takeOverPtr.i;
  
  sendSignalWithDelay(reference(), GSN_CONTINUEB,
                      signal, 5000, 2);
}

void
Dbdih::execUPDATE_TOCONF(Signal* signal)
{
  jamEntry();

  UpdateToConf* conf = (UpdateToConf*)signal->getDataPtr();

  TakeOverRecordPtr takeOverPtr;

  /**
   * We operate towards the master using the main takeover thread.
   * The CONF is however intended for the current active takeover
   * thread.
   */
  ndbrequire(conf->senderData == c_mainTakeOverPtr.i);
  ndbrequire(c_activeThreadTakeOverPtr.i != RNIL);

  c_takeOverPool.getPtr(takeOverPtr, c_activeThreadTakeOverPtr.i);
 
  g_eventLogger->debug("UPDATE_TOCONF: thread: %u, state:%u",
                       takeOverPtr.i,
                       takeOverPtr.p->toSlaveStatus);
  switch(takeOverPtr.p->toSlaveStatus){
  case TakeOverRecord::TO_UPDATE_BEFORE_STORED:
    jam();
    
    CRASH_INSERTION(7154);
    
    takeOverPtr.p->toSlaveStatus = TakeOverRecord::TO_UPDATE_FRAG_STATE_STORED;
    sendUpdateFragStateReq(signal,
                           ZINIT_CREATE_GCI,
                           UpdateFragStateReq::STORED,
                           takeOverPtr);
    return;
  case TakeOverRecord::TO_UPDATE_AFTER_STORED:
    jam();

    CRASH_INSERTION(7195);

    takeOverPtr.p->toSlaveStatus = TakeOverRecord::TO_COPY_FRAG;
    toStartCopyFrag(signal, takeOverPtr);
    return;
  case TakeOverRecord::TO_UPDATE_BEFORE_COMMIT:
    jam();

    CRASH_INSERTION(7196);

    takeOverPtr.p->toSlaveStatus = TakeOverRecord::TO_UPDATE_FRAG_STATE_COMMIT;
    sendUpdateFragStateReq(signal,
                           takeOverPtr.p->startGci, 
                           UpdateFragStateReq::COMMIT_STORED,
                           takeOverPtr);
    return;
  case TakeOverRecord::TO_UPDATE_AFTER_COMMIT:
    jam();

    CRASH_INSERTION(7197);

    start_next_takeover_thread(signal);
    c_activeTakeOverList.addFirst(takeOverPtr);
    takeOverPtr.p->toSlaveStatus = TakeOverRecord::TO_SELECTING_NEXT;
    startNextCopyFragment(signal, takeOverPtr.i);
    return;
  default:
    ndbrequire(false);
  }
}

void
Dbdih::toStartCopyFrag(Signal* signal, TakeOverRecordPtr takeOverPtr)
{
  TabRecordPtr tabPtr;
  tabPtr.i = takeOverPtr.p->toCurrentTabref;
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);

  Uint32 fragId = takeOverPtr.p->toCurrentFragid;

  FragmentstorePtr fragPtr;
  getFragstore(tabPtr.p, fragId, fragPtr);

  ReplicaRecordPtr replicaPtr;
  findReplica(replicaPtr, fragPtr.p, getOwnNodeId(), true);
  
  Uint32 gci = replicaPtr.p->m_restorable_gci;
  replicaPtr.p->m_restorable_gci = 0; // used in union...
  
  Uint32 instanceKey = dihGetInstanceKey(tabPtr.i, fragId);
  BlockReference ref = numberToRef(DBLQH, instanceKey,
                                   takeOverPtr.p->toCopyNode);
  CopyFragReq * const copyFragReq = (CopyFragReq *)&signal->theData[0];
  copyFragReq->userPtr = takeOverPtr.i;
  copyFragReq->userRef = reference();
  copyFragReq->tableId = tabPtr.i;
  copyFragReq->fragId = fragId;
  copyFragReq->nodeId = takeOverPtr.p->toStartingNode;
  copyFragReq->schemaVersion = tabPtr.p->schemaVersion;
  copyFragReq->distributionKey = fragPtr.p->distributionKey;
  copyFragReq->gci = gci;
  Uint32 len = copyFragReq->nodeCount = 
    extractNodeInfo(jamBuffer(), fragPtr.p, 
                    copyFragReq->nodeList);
  copyFragReq->nodeList[len] = takeOverPtr.p->maxPage;
  copyFragReq->nodeList[len+1] = CopyFragReq::CFR_TRANSACTIONAL;
  sendSignal(ref, GSN_COPY_FRAGREQ, signal,
             CopyFragReq::SignalLength + len, JBB);
  g_eventLogger->debug("COPY_FRAGREQ: thread: %u, tab: %u, frag: %u",
    takeOverPtr.i,
    takeOverPtr.p->toCurrentTabref,
    takeOverPtr.p->toCurrentFragid);
  start_next_takeover_thread(signal);
  c_active_copy_threads_list.addFirst(takeOverPtr);
}//Dbdih::toStartCopy()

void Dbdih::sendUpdateFragStateReq(Signal* signal,
                                   Uint32 startGci,
                                   Uint32 replicaType,
                                   TakeOverRecordPtr takeOverPtr)
{
  sendLoopMacro(UPDATE_FRAG_STATEREQ, nullRoutine, RNIL);
  
  g_eventLogger->debug("Update frag state for inst:%u,tab:%u,frag:%u",
                       takeOverPtr.i,
                       takeOverPtr.p->toCurrentTabref,
                       takeOverPtr.p->toCurrentFragid);
  UpdateFragStateReq * const req = (UpdateFragStateReq *)&signal->theData[0];
  req->senderData = takeOverPtr.i;
  req->senderRef = reference();
  req->tableId = takeOverPtr.p->toCurrentTabref;
  req->fragId = takeOverPtr.p->toCurrentFragid;
  req->startingNodeId = takeOverPtr.p->toStartingNode;
  req->copyNodeId = takeOverPtr.p->toCopyNode;
  req->failedNodeId = takeOverPtr.p->toFailedNode;
  req->startGci = startGci;
  req->replicaType = replicaType;
  
  NodeRecordPtr nodePtr;
  nodePtr.i = cfirstAliveNode;
  do {
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
    BlockReference ref = calcDihBlockRef(nodePtr.i);
    sendSignal(ref, GSN_UPDATE_FRAG_STATEREQ, signal, 
	       UpdateFragStateReq::SignalLength, JBB);
    nodePtr.i = nodePtr.p->nextNode;
  } while (nodePtr.i != RNIL);
}//Dbdih::sendUpdateFragStateReq()

void Dbdih::execUPDATE_FRAG_STATECONF(Signal* signal) 
{
  jamEntry();
  CRASH_INSERTION(7148);
  UpdateFragStateConf * conf = (UpdateFragStateConf *)&signal->theData[0];
  
  TakeOverRecordPtr takeOverPtr;

  c_takeOverPool.getPtr(takeOverPtr, conf->senderData);

  g_eventLogger->debug("Updated frag state for inst:%u,tab:%u,frag:%u,state:%u",
                       takeOverPtr.i,
                       takeOverPtr.p->toCurrentTabref,
                       takeOverPtr.p->toCurrentFragid,
                       takeOverPtr.p->toSlaveStatus);
  receiveLoopMacro(UPDATE_FRAG_STATEREQ, conf->sendingNodeId);
  
  switch(takeOverPtr.p->toSlaveStatus){
  case TakeOverRecord::TO_UPDATE_FRAG_STATE_STORED:
    jam();
    CRASH_INSERTION(7198);
    takeOverPtr.p->toSlaveStatus = TakeOverRecord::TO_UPDATE_AFTER_STORED;
    break;
  case TakeOverRecord::TO_UPDATE_FRAG_STATE_COMMIT:
    jam();
    CRASH_INSERTION(7199);
    takeOverPtr.p->toSlaveStatus = TakeOverRecord::TO_UPDATE_AFTER_COMMIT;
    break;
  case TakeOverRecord::TO_SL_UPDATE_FRAG_STATE:
    jam();
    //CRASH_INSERTION(
    start_next_takeover_thread(signal);
    c_active_copy_threads_list.addFirst(takeOverPtr);
    g_eventLogger->debug("UPDATE_FRAG_STATE completed: thread: %u",
      takeOverPtr.i);
    takeOverPtr.p->toSlaveStatus = TakeOverRecord::TO_START_LOGGING;
    takeOverPtr.p->toCurrentFragid++;
    signal->theData[0] = DihContinueB::ZTO_START_LOGGING;
    signal->theData[1] = takeOverPtr.i;
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
    return;
  default:
    jamLine(takeOverPtr.p->toSlaveStatus);
    ndbrequire(false);
  }
  sendUpdateTo(signal, takeOverPtr);
}//Dbdih::execUPDATE_FRAG_STATECONF()

void Dbdih::execCOPY_FRAGREF(Signal* signal) 
{
  const CopyFragRef * const ref = (CopyFragRef *)&signal->theData[0];
  jamEntry();
  Uint32 takeOverPtrI = ref->userPtr;
  Uint32 startingNodeId = ref->startingNodeId;
  Uint32 errorCode = ref->errorCode;

  TakeOverRecordPtr takeOverPtr;
  c_takeOverPool.getPtr(takeOverPtr, takeOverPtrI);  
  ndbrequire(ref->tableId == takeOverPtr.p->toCurrentTabref);
  ndbrequire(ref->fragId == takeOverPtr.p->toCurrentFragid);
  ndbrequire(ref->startingNodeId == takeOverPtr.p->toStartingNode);
  ndbrequire(ref->sendingNodeId == takeOverPtr.p->toCopyNode);
  ndbrequire(takeOverPtr.p->toSlaveStatus == TakeOverRecord::TO_COPY_FRAG);

  //--------------------------------------------------------------------------
  // For some reason we did not succeed in copying a fragment. We treat this
  // as a serious failure and crash the starting node.
  //--------------------------------------------------------------------------
  BlockReference cntrRef = calcNdbCntrBlockRef(startingNodeId);
  SystemError * const sysErr = (SystemError*)&signal->theData[0];
  sysErr->errorCode = SystemError::CopyFragRefError;
  sysErr->errorRef = reference();
  sysErr->data[0] = errorCode;
  sysErr->data[1] = 0;
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
  c_takeOverPool.getPtr(takeOverPtr, conf->userPtr);

  Uint32 rows_lo = conf->rows_lo;
  Uint32 bytes_lo = conf->bytes_lo;

  ndbrequire(conf->tableId == takeOverPtr.p->toCurrentTabref);
  ndbrequire(conf->fragId == takeOverPtr.p->toCurrentFragid);
  ndbrequire(conf->startingNodeId == takeOverPtr.p->toStartingNode);
  ndbrequire(conf->sendingNodeId == takeOverPtr.p->toCopyNode);
  ndbrequire(takeOverPtr.p->toSlaveStatus == TakeOverRecord::TO_COPY_FRAG);

  g_eventLogger->debug("COPY_FRAGCONF: thread: %u, tab: %u, frag: %u",
    takeOverPtr.i,
    takeOverPtr.p->toCurrentTabref,
    takeOverPtr.p->toCurrentFragid);

  TabRecordPtr tabPtr;
  tabPtr.i = takeOverPtr.p->toCurrentTabref;
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);

  FragmentstorePtr fragPtr;
  getFragstore(tabPtr.p, takeOverPtr.p->toCurrentFragid, fragPtr);
  Uint32 instanceKey = dihGetInstanceKey(fragPtr);
  BlockReference lqhRef = numberToRef(DBLQH, instanceKey,
                                      takeOverPtr.p->toStartingNode);
  CopyActiveReq * const req = (CopyActiveReq *)&signal->theData[0];
  req->userPtr = takeOverPtr.i;
  req->userRef = reference();
  req->tableId = takeOverPtr.p->toCurrentTabref;
  req->fragId = takeOverPtr.p->toCurrentFragid;
  req->distributionKey = fragPtr.p->distributionKey;
  req->flags = 0;

  Uint32 min_version = getNodeVersionInfo().m_type[NodeInfo::DB].m_min_version;
  if (ndb_delayed_copy_active_req(min_version))
  {
    jam();
    /**
     * Bug48474 - Don't start logging an fragment
     *            until all fragments has been copied
     *            Else it's easy to run out of REDO
     */
    req->flags |= CopyActiveReq::CAR_NO_WAIT | CopyActiveReq::CAR_NO_LOGGING;
  }
  
  sendSignal(lqhRef, GSN_COPY_ACTIVEREQ, signal,
             CopyActiveReq::SignalLength, JBB);
  g_eventLogger->debug("COPY_ACTIVEREQ: thread: %u, tab: %u, frag: %u",
    takeOverPtr.i,
    takeOverPtr.p->toCurrentTabref,
    takeOverPtr.p->toCurrentFragid);
  
  takeOverPtr.p->toSlaveStatus = TakeOverRecord::TO_COPY_ACTIVE;

  signal->theData[0] = NDB_LE_NR_CopyFragDone;
  signal->theData[1] = getOwnNodeId();
  signal->theData[2] = takeOverPtr.p->toCurrentTabref;
  signal->theData[3] = takeOverPtr.p->toCurrentFragid;
  signal->theData[4] = rows_lo;
  signal->theData[5] = 0;
  signal->theData[6] = bytes_lo;
  signal->theData[7] = 0;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 8, JBB);
}//Dbdih::execCOPY_FRAGCONF()

void Dbdih::execCOPY_ACTIVECONF(Signal* signal) 
{
  const CopyActiveConf * const conf = (CopyActiveConf *)&signal->theData[0];
  jamEntry();
  CRASH_INSERTION(7143);

  TakeOverRecordPtr takeOverPtr;
  c_takeOverPool.getPtr(takeOverPtr, conf->userPtr);

  ndbrequire(conf->tableId == takeOverPtr.p->toCurrentTabref);
  ndbrequire(conf->fragId == takeOverPtr.p->toCurrentFragid);
  ndbrequire(checkNodeAlive(conf->startingNodeId));

  g_eventLogger->debug("COPY_ACTIVECONF: thread: %u, tab: %u, frag: %u",
    takeOverPtr.i,
    takeOverPtr.p->toCurrentTabref,
    takeOverPtr.p->toCurrentFragid);

  takeOverPtr.p->startGci = conf->startGci;

  c_active_copy_threads_list.remove(takeOverPtr);

  if (takeOverPtr.p->toSlaveStatus == TakeOverRecord::TO_COPY_ACTIVE)
  {
    if (c_activeThreadTakeOverPtr.i != RNIL)
    {
      /**
       * There is already an active take over thread that is performing an
       * update of its fragment replica state through the master. We will
       * put ourselves in the c_queued_for_commit_take_over_list and be
       * started as soon as possible.
       */
      g_eventLogger->debug("QUEUED_UPDATE_BEFORE_COMMIT, inst: %u",
                          takeOverPtr.i);
      jam();
      takeOverPtr.p->toSlaveStatus =
        TakeOverRecord::TO_QUEUED_UPDATE_BEFORE_COMMIT;
      c_queued_for_commit_takeover_list.addLast(takeOverPtr);
      return;
    }
    g_eventLogger->debug("Copy frag active: tab:%u,frag:%u,inst:%u",
      takeOverPtr.p->toCurrentTabref,
      takeOverPtr.p->toCurrentFragid,
      takeOverPtr.i);
    jam();
    c_activeThreadTakeOverPtr = takeOverPtr; /* Mark master busy */
    takeOverPtr.p->toSlaveStatus = TakeOverRecord::TO_UPDATE_BEFORE_COMMIT;
    sendUpdateTo(signal, takeOverPtr);
  }
  else
  {
    jam();
    ndbrequire(takeOverPtr.p->toSlaveStatus==
               TakeOverRecord::TO_SL_COPY_ACTIVE);

    if (c_activeThreadTakeOverPtr.i != RNIL)
    {
      jam();
      g_eventLogger->debug("QUEUED_SL_UPDATE_FRAG_STATE, inst: %u",
                           takeOverPtr.i);
      takeOverPtr.p->toSlaveStatus =
        TakeOverRecord::TO_QUEUED_SL_UPDATE_FRAG_STATE;
      c_queued_for_commit_takeover_list.addLast(takeOverPtr);
      return;
    }
    c_activeThreadTakeOverPtr = takeOverPtr; /* Mark master busy */
    takeOverPtr.p->toSlaveStatus = TakeOverRecord::TO_SL_UPDATE_FRAG_STATE;
    g_eventLogger->debug("Update frag state:inst:%u,tab:%u,frag:%u,state:%u",
                         takeOverPtr.i,
                         takeOverPtr.p->toCurrentTabref,
                         takeOverPtr.p->toCurrentFragid,
                         takeOverPtr.p->toSlaveStatus);
    sendUpdateFragStateReq(signal,
                           takeOverPtr.p->startGci,
                           UpdateFragStateReq::START_LOGGING,
                           takeOverPtr);
  }
}//Dbdih::execCOPY_ACTIVECONF()

void
Dbdih::check_take_over_completed_correctly()
{
  ndbrequire(c_completed_copy_threads_list.isEmpty());
  ndbrequire(c_activeTakeOverList.isEmpty());
  ndbrequire(c_queued_for_start_takeover_list.isEmpty());
  ndbrequire(c_queued_for_commit_takeover_list.isEmpty());
  ndbrequire(c_active_copy_threads_list.isEmpty());
  ndbrequire(c_activeThreadTakeOverPtr.i == RNIL);
  ndbrequire(c_mainTakeOverPtr.i != RNIL);
  /**
   * We could be master in system restart where we had to
   * restart with aid of another node and thus perform
   * synchronize with this other node. In this case we
   * have 2 take over records, one for master part and
   * one for start copy part.
   */
  ndbrequire((c_takeOverPool.getUsed() == 1) ||
             (cmasterdihref == reference() &&
              c_takeOverPool.getUsed() == 2));
}

void
Dbdih::release_take_over_threads(void)
{
  TakeOverRecordPtr takeOverPtr;
  do
  {
    jam();
    if (!c_completed_copy_threads_list.removeFirst(takeOverPtr))
    {
      jam();
      break;
    }
    releaseTakeOver(takeOverPtr, false);
  } while (1);
  check_take_over_completed_correctly();
}

bool
Dbdih::thread_takeover_copy_completed(Signal *signal,
                                        TakeOverRecordPtr takeOverPtr)
{
  c_activeTakeOverList.remove(takeOverPtr);
  c_completed_copy_threads_list.addFirst(takeOverPtr);
  c_mainTakeOverPtr.p->m_copy_threads_completed++;
  if (c_mainTakeOverPtr.p->m_copy_threads_completed ==
      c_mainTakeOverPtr.p->m_number_of_copy_threads)
  {
    /* No more to do, just wait for more threads to complete */
    return true;
  }
  return false;
}

void Dbdih::toCopyCompletedLab(Signal * signal, TakeOverRecordPtr takeOverPtr)
{
  /**
   * One take over thread has completed its work. We will have to wait for
   * all of the threads to complete here before we can proceed.
   */
  g_eventLogger->debug("Thread %u copy completed", takeOverPtr.i);
  if (!thread_takeover_copy_completed(signal, takeOverPtr))
  {
    jam();
    return;
  }
  jam();
  c_mainTakeOverPtr.p->m_copy_threads_completed = 0;

  signal->theData[0] = NDB_LE_NR_CopyFragsCompleted;
  signal->theData[1] = takeOverPtr.p->toStartingNode;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);

  /* Ask LQH to dump CopyFrag stage statistics */
  signal->theData[0] = DumpStateOrd::LqhReportCopyInfo;
  sendSignal(DBLQH_REF, GSN_DUMP_STATE_ORD, signal, 1, JBB);

  g_eventLogger->info("Bring Database On-line Completed");
  infoEvent("Bring Database On-line Completed on node %u",
            takeOverPtr.p->toStartingNode);

  Uint32 min_version = getNodeVersionInfo().m_type[NodeInfo::DB].m_min_version;
  if (ndb_delayed_copy_active_req(min_version))
  {
    jam();
    g_eventLogger->info("Starting REDO logging");
    infoEvent("Starting REDO logging on node %u",
              takeOverPtr.p->toStartingNode);
    start_thread_takeover_logging(signal);
    return;
  }
  else
  {
    jam();

    /**
     * We won't need the threads anymore so we remove them from the
     * completed list and release them to the pool.
     */
    release_take_over_threads();
    g_eventLogger->info("Make On-line Database recoverable by waiting"
                        " for LCP Starting");
    infoEvent("Make On-line Database recoverable by waiting"
              " for LCP Starting on node %u",
              takeOverPtr.p->toStartingNode);

    takeOverPtr.p->toSlaveStatus = TakeOverRecord::TO_END_TO;

    EndToReq* req = (EndToReq*)signal->getDataPtrSend();
    req->senderData = takeOverPtr.i;
    req->senderRef = reference();
    req->flags = takeOverPtr.p->m_flags;
    sendSignal(cmasterdihref, GSN_END_TOREQ,
               signal, EndToReq::SignalLength, JBB);
    sendEND_TOREP(signal, takeOverPtr.p->toStartingNode);
    return;
  }
}//Dbdih::toCopyCompletedLab()

void
Dbdih::send_continueb_nr_start_logging(Signal *signal,
                                       TakeOverRecordPtr takeOverPtr)
{
  signal->theData[0] = DihContinueB::ZTO_START_LOGGING;
  signal->theData[1] = takeOverPtr.i;
  sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
}

void
Dbdih::start_thread_takeover_logging(Signal *signal)
{
  /**
   * Ensure no active thread, all thread takeover records are
   * placed into the c_completed_copy_threads_list and that
   * we have a main takeover thread and that all other lists are
   * empty at this point.
   */
  ndbrequire(c_activeThreadTakeOverPtr.i == RNIL);
  ndbrequire(c_activeTakeOverList.isEmpty());
  ndbrequire(c_queued_for_start_takeover_list.isEmpty());
  ndbrequire(c_queued_for_commit_takeover_list.isEmpty());
  ndbrequire(c_active_copy_threads_list.isEmpty());
  ndbrequire(c_mainTakeOverPtr.i != RNIL);
  ndbrequire(!c_completed_copy_threads_list.isEmpty());
  TakeOverRecordPtr takeOverPtr;
  do
  {
    jam();
    if (!c_completed_copy_threads_list.removeFirst(takeOverPtr))
    {
      jam();
      break;
    }
    c_active_copy_threads_list.addFirst(takeOverPtr);
    takeOverPtr.p->toSlaveStatus = TakeOverRecord::TO_START_LOGGING;
    takeOverPtr.p->toCurrentTabref = 0;
    takeOverPtr.p->toCurrentFragid = 0;
    takeOverPtr.p->toCurrentReplica = RNIL;
    send_continueb_nr_start_logging(signal, takeOverPtr);
  } while (1);
}

bool
Dbdih::thread_takeover_completed(Signal *signal,
                                   TakeOverRecordPtr takeOverPtr)
{
  c_active_copy_threads_list.remove(takeOverPtr);
  releaseTakeOver(takeOverPtr, false);
  c_mainTakeOverPtr.p->m_copy_threads_completed++;
  if (c_mainTakeOverPtr.p->m_copy_threads_completed ==
      c_mainTakeOverPtr.p->m_number_of_copy_threads)
  {
    return true;
  }
  return false;
}

void
Dbdih::execEND_TOREF(Signal* signal)
{
  jamEntry();
  EndToRef* ref = (EndToRef*)signal->getDataPtr();
  
  TakeOverRecordPtr takeOverPtr;
  c_takeOverPool.getPtr(takeOverPtr, ref->senderData);

  ndbrequire(false);
}

void
Dbdih::execEND_TOCONF(Signal* signal)
{
  jamEntry();
  EndToConf* conf = (EndToConf*)signal->getDataPtr();
  
  CRASH_INSERTION(7144);
  
  TakeOverRecordPtr takeOverPtr;
  c_takeOverPool.getPtr(takeOverPtr, conf->senderData);
  
  Uint32 senderData = takeOverPtr.p->m_senderData;
  Uint32 senderRef = takeOverPtr.p->m_senderRef;
  Uint32 nodeId = takeOverPtr.p->toStartingNode;

  releaseTakeOver(takeOverPtr, false);
  c_mainTakeOverPtr.i = RNIL;
  c_mainTakeOverPtr.p = NULL;
 
  StartCopyConf* ret = (StartCopyConf*)signal->getDataPtrSend();
  ret->startingNodeId = nodeId;
  ret->senderData = senderData;
  ret->senderRef = reference();
  sendSignal(senderRef, GSN_START_COPYCONF, signal, 
             StartCopyConf::SignalLength, JBB);
}

void Dbdih::releaseTakeOver(TakeOverRecordPtr takeOverPtr,
                            bool from_master)
{
  takeOverPtr.p->m_copy_threads_completed = 0;
  takeOverPtr.p->m_number_of_copy_threads = (Uint32)-1;
  takeOverPtr.p->m_copy_thread_id = (Uint32)-1;

  takeOverPtr.p->toCopyNode = RNIL;
  takeOverPtr.p->toCurrentFragid = RNIL;
  takeOverPtr.p->toCurrentReplica = RNIL;
  takeOverPtr.p->toCurrentTabref = RNIL;
  takeOverPtr.p->toFailedNode = RNIL;
  takeOverPtr.p->toStartingNode = RNIL;
  NdbTick_Invalidate(&takeOverPtr.p->toStartTime);
  takeOverPtr.p->toSlaveStatus = TakeOverRecord::TO_SLAVE_IDLE;
  takeOverPtr.p->toMasterStatus = TakeOverRecord::TO_MASTER_IDLE;

  if (from_master)
  {
    c_masterActiveTakeOverList.remove(takeOverPtr);
  }
  c_takeOverPool.release(takeOverPtr);
}//Dbdih::releaseTakeOver()

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
  globalData.m_restart_seq = ++SYSFILE->m_restart_seq;
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
    sendDihRestartRef(signal);
    return;
  }//if
}//Dbdih::closingGcpLab()

void
Dbdih::sendDihRestartRef(Signal* signal)
{
  jam();

  /**
   * We couldn't read P0.Sysfile...
   *   so compute no_nodegroup_mask from configuration
   */
  NdbNodeBitmask no_nodegroup_mask;

  ndb_mgm_configuration_iterator * iter =
    m_ctx.m_config.getClusterConfigIterator();
  for(ndb_mgm_first(iter); ndb_mgm_valid(iter); ndb_mgm_next(iter))
  {
    jam();
    Uint32 nodeId;
    Uint32 nodeType;

    ndbrequire(!ndb_mgm_get_int_parameter(iter,CFG_NODE_ID, &nodeId));
    ndbrequire(!ndb_mgm_get_int_parameter(iter,CFG_TYPE_OF_SECTION,
                                          &nodeType));

    if (nodeType == NodeInfo::DB)
    {
      jam();
      Uint32 ng;
      if (ndb_mgm_get_int_parameter(iter, CFG_DB_NODEGROUP, &ng) == 0)
      {
        jam();
        if (ng == NDB_NO_NODEGROUP)
        {
          no_nodegroup_mask.set(nodeId);
        }
      }
    }
  }
  DihRestartRef * ref = CAST_PTR(DihRestartRef, signal->getDataPtrSend());
  no_nodegroup_mask.copyto(NdbNodeBitmask::Size, ref->no_nodegroup_mask);
  sendSignal(cntrlblockref, GSN_DIH_RESTARTREF, signal,
             DihRestartRef::SignalLength, JBB);
}

/* ------------------------------------------------------------------------- */
/*       SELECT THE MASTER CANDIDATE TO BE USED IN SYSTEM RESTARTS.          */
/* ------------------------------------------------------------------------- */
void Dbdih::selectMasterCandidateAndSend(Signal* signal)
{
  setNodeGroups();

  NodeRecordPtr nodePtr;
  Uint32 node_groups[MAX_NDB_NODES];
  memset(node_groups, 0, sizeof(node_groups));
  NdbNodeBitmask no_nodegroup_mask;
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    jam();
    if (Sysfile::getNodeStatus(nodePtr.i, SYSFILE->nodeStatus) == Sysfile::NS_NotDefined)
    {
      jam();
      continue;
    }
    const Uint32 ng = Sysfile::getNodeGroup(nodePtr.i, SYSFILE->nodeGroups);
    if(ng != NO_NODE_GROUP_ID)
    {
      jam();
      jamLine(Uint16(ng));
      ndbrequire(ng < MAX_NDB_NODE_GROUPS);
      node_groups[ng]++;
    }
    else
    {
      jam();
      no_nodegroup_mask.set(nodePtr.i);
    }
  }

  DihRestartConf * conf = CAST_PTR(DihRestartConf, signal->getDataPtrSend());
  conf->unused = getOwnNodeId();
  conf->latest_gci = SYSFILE->lastCompletedGCI[getOwnNodeId()];
  no_nodegroup_mask.copyto(NdbNodeBitmask::Size, conf->no_nodegroup_mask);
  sendSignal(cntrlblockref, GSN_DIH_RESTARTCONF, signal,
             DihRestartConf::SignalLength, JBB);

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
    sendDihRestartRef(signal);
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
  sendDihRestartRef(signal);
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

  if (ERROR_INSERTED(7179) || ERROR_INSERTED(7217))
  {
    CLEAR_ERROR_INSERT_VALUE;
  }

  if (ERROR_INSERTED(7184))
  {
    SET_ERROR_INSERT_VALUE(7000);
  }

  c_increase_lcp_speed_after_nf = true;

  /*-------------------------------------------------------------------------*/
  // The first step is to convert from a bit mask to an array of failed nodes.
  /*-------------------------------------------------------------------------*/
  Uint32 index = 0;
  for (i = 1; i < MAX_NDB_NODES; i++) {
    if(NdbNodeBitmask::get(nodeFail->theNodes, i)){
      jamLine(i);
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
    setNodeRecoveryStatus(TNodePtr.i, NodeRecord::NODE_FAILED);
    make_node_not_usable(TNodePtr.p);
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
#if 0
  /**
   * Node will crash by itself...
   *   nodeRestart is run then...
   */
  if (false && c_nodeStartMaster.startNode != RNIL && getNodeStatus(c_nodeStartMaster.startNode) == NodeRecord::ALIVE)
  {
    BlockReference cntrRef = calcNdbCntrBlockRef(c_nodeStartMaster.startNode);
    SystemError * const sysErr = (SystemError*)&signal->theData[0];
    sysErr->errorCode = SystemError::StartInProgressError;
    sysErr->errorRef = reference();
    sysErr->data[0]= 0;
    sysErr->data[1]= __LINE__;
    sendSignal(cntrRef, GSN_SYSTEM_ERROR, signal,  SystemError::SignalLength, JBA);
    nodeResetStart(signal);
  }//if
#endif

  if (is_lcp_paused())
  {
    /**
     * Stop any LCP pausing, a node has crashed, this implies that also the
     * node that caused us to pause the LCP has crashed.
     */
    jam();
    handle_node_failure_in_pause(signal);
  }
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
    if (oldMasterRef == reference()) {
      /*-------------------------------------------------------*/
      // Functions that need to be called only for master nodes.
      /*-------------------------------------------------------*/
      checkCopyTab(signal, failedNodePtr);
      checkStopPermMaster(signal, failedNodePtr);
      checkWaitGCPMaster(signal, failedNodes[i]);

      {
        Ptr<TakeOverRecord> takeOverPtr;
        if (findTakeOver(takeOverPtr, failedNodePtr.i))
        {
          handleTakeOver(signal, takeOverPtr);
        }
      }
      checkGcpOutstanding(signal, failedNodePtr.i);
    } else {
      jam();
      /*-----------------------------------------------------------*/
      // Functions that need to be called only for nodes that were
      // not master before these failures.
      /*-----------------------------------------------------------*/
      checkStopPermProxy(signal, failedNodes[i]);
      checkWaitGCPProxy(signal, failedNodes[i]);
    }//if
    /*--------------------------------------------------*/
    // Functions that need to be called for all nodes.
    /*--------------------------------------------------*/
    checkStopMe(signal, failedNodePtr);
    failedNodeLcpHandling(signal, failedNodePtr);
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
    setNodeRestartInfoBits(signal);
  }//if

  setGCPStopTimeouts();
}//Dbdih::execNODE_FAILREP()

void Dbdih::checkCopyTab(Signal* signal, NodeRecordPtr failedNodePtr)
{
  jam();

  if(c_nodeStartMaster.startNode != failedNodePtr.i){
    jam();
    return;
  }

  switch(c_nodeStartMaster.m_outstandingGsn){
  case GSN_COPY_TABREQ:
    jam();
    releaseTabPages(failedNodePtr.p->activeTabptr);
    if (c_COPY_TABREQ_Counter.isWaitingFor(failedNodePtr.i))
    {
      jam();
      c_COPY_TABREQ_Counter.clearWaitingFor(failedNodePtr.i);
    }
    c_nodeStartMaster.wait = ZFALSE;
    break;
  case GSN_START_INFOREQ:
  case GSN_START_PERMCONF:
  case GSN_DICTSTARTREQ:
  case GSN_COPY_GCIREQ:
    jam();
    break;
  default:
    g_eventLogger->error("outstanding gsn: %s(%d)",
                         getSignalName(c_nodeStartMaster.m_outstandingGsn),
                         c_nodeStartMaster.m_outstandingGsn);
    ndbrequire(false);
  }
  
  if (!c_nodeStartMaster.m_fragmentInfoMutex.isNull())
  {
    jam();
    Mutex mutex(signal, c_mutexMgr, c_nodeStartMaster.m_fragmentInfoMutex);
    mutex.unlock();
  }

  nodeResetStart(signal);
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
Dbdih::handleTakeOver(Signal* signal, TakeOverRecordPtr takeOverPtr)
{
  jam();
  switch(takeOverPtr.p->toMasterStatus){
  case TakeOverRecord::TO_MASTER_IDLE:
    jam();
    releaseTakeOver(takeOverPtr, true);
    return;
  case TakeOverRecord::TO_MUTEX_BEFORE_STORED:
    jam();
    /**
     * Waiting for lock...
     *   do nothing...will be detected when lock is acquired
     */
    return;
  case TakeOverRecord::TO_MUTEX_BEFORE_LOCKED:
    jam();
    /**
     * Has lock...and NGPtr reservation...
     */
    abortTakeOver(signal, takeOverPtr);
    return;
  case TakeOverRecord::TO_AFTER_STORED:{
    jam();
    /**
     * No lock...but NGPtr reservation...remove NGPtr reservation
     */
    NodeRecordPtr nodePtr;
    NodeGroupRecordPtr NGPtr;
    nodePtr.i = takeOverPtr.p->toCopyNode;
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
    NGPtr.i = nodePtr.p->nodeGroup;
    ptrCheckGuard(NGPtr, MAX_NDB_NODE_GROUPS, nodeGroupRecord);
    
    ndbassert(NGPtr.p->activeTakeOver == takeOverPtr.p->toStartingNode);
    if (NGPtr.p->activeTakeOver == takeOverPtr.p->toStartingNode)
    {
      jam();
      NGPtr.p->activeTakeOver = 0;
    }
    releaseTakeOver(takeOverPtr, true);
    return;
  }
  case TakeOverRecord::TO_MUTEX_BEFORE_COMMIT:
    jam();
    /**
     * Waiting for lock...
     *   do nothing...will be detected when lock is acquired
     */
    return;
  case TakeOverRecord::TO_MUTEX_BEFORE_SWITCH_REPLICA:
    jam();
    /**
     * Waiting for lock...
     *   do nothing...will be detected when lock is acquired
     */
    return;
  case TakeOverRecord::TO_MUTEX_AFTER_SWITCH_REPLICA:
    jam();
    abortTakeOver(signal, takeOverPtr);
    return;
  case TakeOverRecord::TO_WAIT_LCP:{
    jam();
    /**
     * Waiting for LCP
     */
    NodeRecordPtr nodePtr;
    nodePtr.i = takeOverPtr.p->toStartingNode;
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);    
    nodePtr.p->copyCompleted = 0;
    releaseTakeOver(takeOverPtr, true);
    return;
  }
  default:
    jamLine(takeOverPtr.p->toMasterStatus);
    ndbrequire(false);
  }
}

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

bool
Dbdih::findTakeOver(Ptr<TakeOverRecord> & ptr, Uint32 failedNodeId)
{
  for (c_masterActiveTakeOverList.first(ptr); !ptr.isNull(); 
       c_masterActiveTakeOverList.next(ptr))
  {
    jam();
    if (ptr.p->toStartingNode == failedNodeId)
    {
      jam();
      return true;
    }
  }
  ptr.setNull();
  return false;
}//Dbdih::findTakeOver()

void Dbdih::failedNodeLcpHandling(Signal* signal, NodeRecordPtr failedNodePtr)
{
  jam();
  const Uint32 nodeId = failedNodePtr.i;

  if (isMaster() && c_lcpState.m_participatingLQH.get(failedNodePtr.i))
  {
    /*----------------------------------------------------*/
    /*  THE NODE WAS INVOLVED IN A LOCAL CHECKPOINT. WE   */
    /* MUST UPDATE THE ACTIVE STATUS TO INDICATE THAT     */
    /* THE NODE HAVE MISSED A LOCAL CHECKPOINT.           */
    /*----------------------------------------------------*/

    /**
     * Bug#28717, Only master should do this, as this status is copied
     *   to other nodes
     */
    switch (failedNodePtr.p->activeStatus) {
    case Sysfile::NS_Active:
      jam();
      failedNodePtr.p->activeStatus = Sysfile::NS_ActiveMissed_1;
      break;
    case Sysfile::NS_ActiveMissed_1:
      jam();
      failedNodePtr.p->activeStatus = Sysfile::NS_ActiveMissed_1;
      break;
    case Sysfile::NS_ActiveMissed_2:
      jam();
      failedNodePtr.p->activeStatus = Sysfile::NS_NotActive_NotTakenOver;
      break;
    case Sysfile::NS_TakeOver:
      jam();
      failedNodePtr.p->activeStatus = Sysfile::NS_NotActive_NotTakenOver;
      break;
    case Sysfile::NS_Configured:
      jam();
      break;
    default:
      g_eventLogger->error("activeStatus = %u "
                           "at failure after NODE_FAILREP of node = %u",
                           (Uint32) failedNodePtr.p->activeStatus,
                           failedNodePtr.i);
      ndbrequire(false);
      break;
    }//switch
  }//if

  c_lcpState.m_participatingDIH.clear(failedNodePtr.i);
  c_lcpState.m_participatingLQH.clear(failedNodePtr.i);

  bool wf = c_MASTER_LCPREQ_Counter.isWaitingFor(failedNodePtr.i);

  if(c_lcpState.m_LCP_COMPLETE_REP_Counter_DIH.isWaitingFor(failedNodePtr.i))
  {
    jam();
    /**
     * Mark the signal as a special signal to distinguish it from a signal
     * that arrives from time queue for a dead node that should not be
     * handled. The marking here makes it known to the LCP_COMPLETE_REP
     * that this is a special node failure handling signal which should
     * be allowed to pass through although the node is dead.
     */
    LcpCompleteRep * rep = (LcpCompleteRep*)signal->getDataPtrSend();
    rep->nodeId = failedNodePtr.i;
    rep->lcpId = SYSFILE->latestLCP_ID;
    rep->blockNo = DBDIH;
    rep->fromTQ = 0;
    sendSignal(reference(), GSN_LCP_COMPLETE_REP, signal, 
               LcpCompleteRep::SignalLengthTQ, JBB);
  }
   
  bool lcp_complete_rep = false;
  if (!wf)
  {
    jam();
 
    /**
     * Check if we're waiting for the failed node's LQH to complete
     *
     * Note that this is ran "before" LCP master take over
     */
    if(c_lcpState.m_LCP_COMPLETE_REP_Counter_LQH.isWaitingFor(nodeId)){
      jam();
      
      lcp_complete_rep = true;
      LcpCompleteRep * rep = (LcpCompleteRep*)signal->getDataPtrSend();
      rep->nodeId  = nodeId;
      rep->lcpId   = SYSFILE->latestLCP_ID;
      rep->blockNo = DBLQH;
      rep->fromTQ = 0;
      sendSignal(reference(), GSN_LCP_COMPLETE_REP, signal, 
                 LcpCompleteRep::SignalLengthTQ, JBB);
      
      if(c_lcpState.m_LAST_LCP_FRAG_ORD.isWaitingFor(nodeId)){
        jam();
        /**
         * Make sure we're ready to accept it
         */
        c_lcpState.m_LAST_LCP_FRAG_ORD.clearWaitingFor(nodeId);
      }
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
  
dosend:
  if (c_EMPTY_LCP_REQ_Counter.isWaitingFor(failedNodePtr.i))
  {
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
  }
  else if (!c_EMPTY_LCP_REQ_Counter.done() && lcp_complete_rep)
  {
    jam();
    c_EMPTY_LCP_REQ_Counter.setWaitingFor(failedNodePtr.i);
    goto dosend;
  }
  
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
    GCPPrepareConf* conf = (GCPPrepareConf*)signal->getDataPtrSend();
    conf->nodeId = failedNodeId;
    conf->gci_hi = Uint32(m_micro_gcp.m_master.m_new_gci >> 32);
    conf->gci_lo = Uint32(m_micro_gcp.m_master.m_new_gci);
    sendSignal(reference(), GSN_GCP_PREPARECONF, signal, 
               GCPPrepareConf::SignalLength, JBB);
  }//if

  if (c_GCP_COMMIT_Counter.isWaitingFor(failedNodeId)) 
  {
    jam();
    /* Record minimum failure number, will cause re-send of 
     * GCP_NOMORETRANS if local GCP_NODEFINISH arrives before
     * TC has handled the failure.
     */
    cMinTcFailNo = cfailurenr;

    /**
     * Waiting for GSN_GCP_NODEFINISH
     *   TC-take-over can generate new transactions
     *   that will be in this epoch
     *   re-run GCP_NOMORETRANS to master-TC (self) that will run
     *   take-over
     */
    c_GCP_COMMIT_Counter.clearWaitingFor(failedNodeId);
    
    /* Check to see whether we have already received GCP_NODEFINISH
     * from the local (Master) TC instance
     */ 
    if (!c_GCP_COMMIT_Counter.isWaitingFor(getOwnNodeId()))
    {
      jam();
      /* Already received GCP_NODEFINISH for this GCI, must
       * resend GCP_NOMORETRANS request now.
       * Otherwise we will re-send it when GCP_NODEFINISH
       * arrives.
       */
      c_GCP_COMMIT_Counter.setWaitingFor(getOwnNodeId());
      /* Reset DIH GCP state */
      m_micro_gcp.m_state = MicroGcp::M_GCP_COMMIT;

      GCPNoMoreTrans* req = (GCPNoMoreTrans*)signal->getDataPtrSend();
      req->senderRef = reference();
      req->senderData = m_micro_gcp.m_master_ref;
      req->gci_hi = Uint32(m_micro_gcp.m_old_gci >> 32);
      req->gci_lo = Uint32(m_micro_gcp.m_old_gci & 0xFFFFFFFF);
      sendSignal(clocaltcblockref, GSN_GCP_NOMORETRANS, signal,
                 GCPNoMoreTrans::SignalLength, JBB);
    }
  }

  if (c_GCP_SAVEREQ_Counter.isWaitingFor(failedNodeId)) {
    jam();
    GCPSaveRef * const saveRef = (GCPSaveRef*)&signal->theData[0];
    saveRef->dihPtr = failedNodeId;
    saveRef->nodeId = failedNodeId;
    saveRef->gci    = m_gcp_save.m_master.m_new_gci;
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

  if (c_SUB_GCP_COMPLETE_REP_Counter.isWaitingFor(failedNodeId))
  {
    jam();
    SubGcpCompleteAck* ack = CAST_PTR(SubGcpCompleteAck,
                                      signal->getDataPtrSend());
    ack->rep.senderRef = numberToRef(DBDIH, failedNodeId);
    sendSignal(reference(), GSN_SUB_GCP_COMPLETE_ACK, signal,
	       SubGcpCompleteAck::SignalLength, JBB);
  }
}

/**
 * This function checks if any node is started that doesn't support the
 * functionality to remove the need of the EMPTY_LCP_REQ protocol.
 */
bool Dbdih::check_if_empty_lcp_needed(void)
{
  NodeRecordPtr specNodePtr;
  specNodePtr.i = cfirstAliveNode;
  do
  {
    jam();
    if (getNodeInfo(specNodePtr.i).m_version < NDBD_EMPTY_LCP_NOT_NEEDED)
    {
      jam();
      return true;
    }
    ptrCheckGuard(specNodePtr, MAX_NDB_NODES, nodeRecord);
    specNodePtr.i = specNodePtr.p->nextNode;
  } while (specNodePtr.i != RNIL);

  /* Check amongst the dying, should be at least one */
  specNodePtr.i = cfirstDeadNode;
  do
  {
    jam();
    ptrCheckGuard(specNodePtr, MAX_NDB_NODES, nodeRecord);
    switch (specNodePtr.p->nodeStatus)
    {
    case NodeRecord::DIED_NOW:
      jam();
    case NodeRecord::DYING:
      jam();
      if (getNodeInfo(specNodePtr.i).m_version < NDBD_EMPTY_LCP_NOT_NEEDED)
      {
        jam();
        return true;
      }
      break;
    case NodeRecord::DEAD:
      jam();
      break;
    default:
      jamLine(specNodePtr.p->nodeStatus);
      ndbrequire(false);
    }
    specNodePtr.i = specNodePtr.p->nextNode;
  } while (specNodePtr.i != RNIL);

  return false;
}

void
Dbdih::startLcpMasterTakeOver(Signal* signal, Uint32 nodeId)
{
  jam();

  if (ERROR_INSERTED(7230))
  {
    return;
  }

  Uint32 oldNode = c_lcpMasterTakeOverState.failedNodeId;
  
  NodeRecordPtr nodePtr;
  nodePtr.i = oldNode;
  if (oldNode > 0 && oldNode < MAX_NDB_NODES)
  {
    jam();
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
    if (nodePtr.p->m_nodefailSteps.get(NF_LCP_TAKE_OVER))
    {
      jam();
      checkLocalNodefailComplete(signal, oldNode, NF_LCP_TAKE_OVER);
    }
  }

  c_lcpMasterTakeOverState.use_empty_lcp = check_if_empty_lcp_needed();
  if (!c_lcpMasterTakeOverState.use_empty_lcp)
  {
    jam();
    /**
     * As of NDBD_EMPTY_LCP_PROTOCOL_NOT_NEEDED version this is the
     * normal path through the code.
     *
     * We now ensures that LQH keeps track of which LCP_FRAG_ORD it has
     * received. So this means that we can be a bit more sloppy in master
     * take over. We need not worry if we resend LCP_FRAG_ORD since LQH will
     * simply drop it.
     *
     * So when we are done with the master take over we will simply start from
     * scratch from the first table and fragment. We have sufficient
     * information locally in the new master to skip resending all fragment
     * replicas where we already received LCP_FRAG_REP. For those where we sent
     * LCP_FRAG_ORD but not received LCP_FRAG_REP we simply send it again. If
     * it was sent before then LQH will discover it and drop it.
     *
     * We also don't need to worry about sending too many LCP_FRAG_ORDs to LQH
     * since we can send it for all fragment replicas given that we use the
     * fragment record as the queueing record. So in practice the queue is
     * always large enough.
     *
     * For old nodes we still have to run the EMPTY_LCP_REQ protocol to
     * ensure that all outstanding LCP_FRAG_ORD have come back to all
     * DBDIHs as LCP_FRAG_REPs to ensure that every DBDIH has a complete
     * understanding of the LCP state and can take it over. What we do here
     * is that if one node is old, then we run the old take over protocol
     * for all nodes to not mess the code up too much. Theoretically it
     * would suffice to send EMPTY_LCP_REQ to only old nodes, but we won't
     * handle this, we will simply run the old code as it was.
     */
    c_lcpMasterTakeOverState.minTableId = 0;
    c_lcpMasterTakeOverState.minFragId = 0;
    c_lcpMasterTakeOverState.failedNodeId = nodeId;
    c_lcpMasterTakeOverState.set(LMTOS_WAIT_LCP_FRAG_REP, __LINE__);
    setLocalNodefailHandling(signal, nodeId, NF_LCP_TAKE_OVER);
    checkEmptyLcpComplete(signal);
    return;
  }
  
  c_lcpMasterTakeOverState.minTableId = ~0;
  c_lcpMasterTakeOverState.minFragId = ~0;
  c_lcpMasterTakeOverState.failedNodeId = nodeId;
  c_lcpMasterTakeOverState.set(LMTOS_WAIT_EMPTY_LCP, __LINE__);
  
  EmptyLcpReq* req = (EmptyLcpReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  {
    NodeRecordPtr specNodePtr;
    specNodePtr.i = cfirstAliveNode;
    do {
      jam();
      ptrCheckGuard(specNodePtr, MAX_NDB_NODES, nodeRecord);
      if (!c_EMPTY_LCP_REQ_Counter.isWaitingFor(specNodePtr.i))
      {
        jam();
        c_EMPTY_LCP_REQ_Counter.setWaitingFor(specNodePtr.i);
        sendEMPTY_LCP_REQ(signal, specNodePtr.i, 0);
        if (c_lcpState.m_LAST_LCP_FRAG_ORD.isWaitingFor(specNodePtr.i))
        {
          jam();
          c_lcpState.m_LAST_LCP_FRAG_ORD.clearWaitingFor();
        }
      }
      specNodePtr.i = specNodePtr.p->nextNode;
    } while (specNodePtr.i != RNIL);
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
  sendLoopMacro(MASTER_GCPREQ, sendMASTER_GCPREQ, RNIL);

  signal->theData[0] = NDB_LE_GCP_TakeoverStarted;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 1, JBB);

  /**
   * save own value...
   *   to be able to check values returned in MASTER_GCPCONF
   */
  m_gcp_save.m_master.m_new_gci = m_gcp_save.m_gci;

  setLocalNodefailHandling(signal, oldMasterId, NF_GCP_TAKE_OVER);
}//Dbdih::handleNewMaster()

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
  
  /**
   * If node has node complete LCP
   *   we need to remove it as undo might not be complete
   *   bug#31257
   */
  failedNodePtr.p->m_remove_node_from_table_lcp_id = RNIL;
  if (c_lcpState.m_LCP_COMPLETE_REP_Counter_LQH.isWaitingFor(failedNodePtr.i))
  {
    jam();
    failedNodePtr.p->m_remove_node_from_table_lcp_id = SYSFILE->latestLCP_ID;
  }
  
  jam();

  if (!ERROR_INSERTED(7194) && !ERROR_INSERTED(7221))
  {
    signal->theData[0] = DihContinueB::ZREMOVE_NODE_FROM_TABLE;
    signal->theData[1] = failedNodePtr.i;
    signal->theData[2] = 0; // Tab id
    if (!ERROR_INSERTED(7233))
      sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBB);
    else
      sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 300, 3);
  }
  else
  {
    if (ERROR_INSERTED(7194))
    {
      ndbout_c("7194 Not starting ZREMOVE_NODE_FROM_TABLE");
    }
    else if (ERROR_INSERTED(7221))
    {
      ndbout_c("7221 Not starting ZREMOVE_NODE_FROM_TABLE");
    }
  }

  setLocalNodefailHandling(signal, failedNodePtr.i, NF_REMOVE_NODE_FROM_TABLE);
}//Dbdih::startRemoveFailedNode()

bool Dbdih::handle_master_take_over_copy_gci(Signal *signal, NodeId new_master_node_id)
{
  if (c_copyGCISlave.m_expectedNextWord != 0)
  {
    jam();
    c_copyGCISlave.m_expectedNextWord = 0;
    c_copyGCISlave.m_copyReason = CopyGCIReq::IDLE;
  }

  if (c_copyGCISlave.m_copyReason != CopyGCIReq::IDLE)
  {
    /**
     * Before we allow the new master to start up the new GCP protocols
     * we need to ensure that the activity started by the previous
     * failed master is completed before we process the master takeover.
     * By enforcing this in MASTER_GCPREQ and MASTER_LCPREQ we are
     * certain that the master takeover is ready to start up the new
     * COPY_GCIREQ protocols.
     */
    sendSignalWithDelay(reference(), GSN_MASTER_GCPREQ,
                        signal, 10, MasterGCPReq::SignalLength);
    return true;
  }
  c_handled_master_take_over_copy_gci = new_master_node_id;
  return false;
}

/*--------------------------------------------------*/
/*       THE MASTER HAS FAILED AND THE NEW MASTER IS*/
/*       QUERYING THIS NODE ABOUT THE STATE OF THE  */
/*       GLOBAL CHECKPOINT PROTOCOL                 */
/*--------------------------------------------------*/
void Dbdih::execMASTER_GCPREQ(Signal* signal) 
{
  NodeRecordPtr failedNodePtr;
  NodeRecordPtr newMasterNodePtr;
  MasterGCPReq * const masterGCPReq = (MasterGCPReq *)&signal->theData[0];  
  jamEntry();
  const BlockReference newMasterBlockref = masterGCPReq->masterRef;
  const Uint32 failedNodeId = masterGCPReq->failedNodeId;

  failedNodePtr.i = failedNodeId;
  ptrCheckGuard(failedNodePtr, MAX_NDB_NODES, nodeRecord);
  newMasterNodePtr.i = refToNode(newMasterBlockref);
  ptrCheckGuard(newMasterNodePtr, MAX_NDB_NODES, nodeRecord);

  if (newMasterNodePtr.p->nodeStatus != NodeRecord::ALIVE)
  {
    /**
     * We delayed the MASTER_GCPREQ signal and now it arrived after
     * the new master already died. We ignore this signal.
     */
#ifdef VM_TRACE
    g_eventLogger->info("Dropped MASTER_GCPREQ from node %u",
                        newMasterNodePtr.i);
#endif
    jam();
    return;
  }

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

  if (handle_master_take_over_copy_gci(signal, newMasterNodePtr.i))
  {
    return;
  }
#ifdef VM_TRACE
  g_eventLogger->info("Handle MASTER_GCPREQ from node %u",
                      newMasterNodePtr.i);
#endif
  if (ERROR_INSERTED(7181))
  {
    ndbout_c("execGCP_TCFINISHED in MASTER_GCPREQ");
    CLEAR_ERROR_INSERT_VALUE;
    signal->theData[0] = c_error_7181_ref;
    signal->theData[1] = (Uint32)(m_micro_gcp.m_old_gci >> 32);
    signal->theData[2] = (Uint32)(m_micro_gcp.m_old_gci & 0xFFFFFFFF);
    signal->theData[3] = cfailurenr;
    execGCP_TCFINISHED(signal);
  }

  MasterGCPConf::State gcpState;
  switch(m_micro_gcp.m_state){
  case MicroGcp::M_GCP_IDLE:
    jam();
    gcpState = MasterGCPConf::GCP_READY;
    break;
  case MicroGcp::M_GCP_PREPARE:
    jam();
    gcpState = MasterGCPConf::GCP_PREPARE_RECEIVED;
    break;
  case MicroGcp::M_GCP_COMMIT:
    jam();
    gcpState = MasterGCPConf::GCP_COMMIT_RECEIVED;
    break;
  case MicroGcp::M_GCP_COMMITTED:
    jam();
    gcpState = MasterGCPConf::GCP_COMMITTED;

    /**
     * Change state to GCP_COMMIT_RECEIVEDn and rerun GSN_GCP_NOMORETRANS
     */
    gcpState = MasterGCPConf::GCP_COMMIT_RECEIVED;
    m_micro_gcp.m_state = MicroGcp::M_GCP_COMMIT;

    {
      GCPNoMoreTrans* req2 = (GCPNoMoreTrans*)signal->getDataPtrSend();
      req2->senderRef = reference();
      req2->senderData = m_micro_gcp.m_master_ref;
      req2->gci_hi = (Uint32)(m_micro_gcp.m_old_gci >> 32);
      req2->gci_lo = (Uint32)(m_micro_gcp.m_old_gci & 0xFFFFFFFF);
      sendSignal(clocaltcblockref, GSN_GCP_NOMORETRANS, signal,
                 GCPNoMoreTrans::SignalLength, JBB);
    }
    break;
  case MicroGcp::M_GCP_COMPLETE:
    /**
     * This is a master only state...
     */
    ndbrequire(false);
  }

  MasterGCPConf::SaveState saveState;
  switch(m_gcp_save.m_state){
  case GcpSave::GCP_SAVE_IDLE:
    jam();
    saveState = MasterGCPConf::GCP_SAVE_IDLE;
    break;
  case GcpSave::GCP_SAVE_REQ:
    jam();
    saveState = MasterGCPConf::GCP_SAVE_REQ;
    break;
  case GcpSave::GCP_SAVE_CONF:
    jam();
    saveState = MasterGCPConf::GCP_SAVE_CONF;
    break;
  case GcpSave::GCP_SAVE_COPY_GCI:
    jam();
    saveState = MasterGCPConf::GCP_SAVE_COPY_GCI;
    break;
  }

  MasterGCPConf * const masterGCPConf = (MasterGCPConf *)&signal->theData[0];  
  masterGCPConf->gcpState  = gcpState;
  masterGCPConf->senderNodeId = cownNodeId;
  masterGCPConf->failedNodeId = failedNodeId;
  masterGCPConf->newGCP_hi = (Uint32)(m_micro_gcp.m_new_gci >> 32);
  masterGCPConf->latestLCP = SYSFILE->latestLCP_ID;
  masterGCPConf->oldestRestorableGCI = SYSFILE->oldestRestorableGCI;
  masterGCPConf->keepGCI = SYSFILE->keepGCI;  
  masterGCPConf->newGCP_lo = Uint32(m_micro_gcp.m_new_gci);
  masterGCPConf->saveState = saveState;
  masterGCPConf->saveGCI = m_gcp_save.m_gci;
  for(Uint32 i = 0; i < NdbNodeBitmask::Size; i++)
    masterGCPConf->lcpActive[i] = SYSFILE->lcpActive[i];

  if (ERROR_INSERTED(7225))
  {
    CLEAR_ERROR_INSERT_VALUE;
    ndbrequire(refToNode(newMasterBlockref) == getOwnNodeId());
    sendSignalWithDelay(newMasterBlockref, GSN_MASTER_GCPCONF, signal,
                        500, MasterGCPConf::SignalLength);
  }
  else
  {
    sendSignal(newMasterBlockref, GSN_MASTER_GCPCONF, signal,
               MasterGCPConf::SignalLength, JBB);
  }

  if (ERROR_INSERTED(7182))
  {
    ndbout_c("execGCP_TCFINISHED in MASTER_GCPREQ");
    CLEAR_ERROR_INSERT_VALUE;
    signal->theData[0] = c_error_7181_ref;
    signal->theData[1] = (Uint32)(m_micro_gcp.m_old_gci >> 32);
    signal->theData[2] = (Uint32)(m_micro_gcp.m_old_gci & 0xFFFFFFFF);
    signal->theData[3] = cfailurenr;
    execGCP_TCFINISHED(signal);
  }
}//Dbdih::execMASTER_GCPREQ()

void Dbdih::execMASTER_GCPCONF(Signal* signal) 
{
  NodeRecordPtr senderNodePtr;
  MasterGCPConf * const masterGCPConf = (MasterGCPConf *)&signal->theData[0];
  jamEntry();
  senderNodePtr.i = masterGCPConf->senderNodeId;
  ptrCheckGuard(senderNodePtr, MAX_NDB_NODES, nodeRecord);
 
#ifdef VM_TRACE
  g_eventLogger->info("MASTER_GCPCONF from node %u", senderNodePtr.i);
#endif

  MasterGCPConf::State gcpState = (MasterGCPConf::State)masterGCPConf->gcpState;
  MasterGCPConf::SaveState saveState =
    (MasterGCPConf::SaveState)masterGCPConf->saveState;
  const Uint32 failedNodeId = masterGCPConf->failedNodeId;
  const Uint32 newGcp_hi = masterGCPConf->newGCP_hi;
  const Uint32 newGcp_lo = masterGCPConf->newGCP_lo;
  Uint64 newGCI = newGcp_lo | (Uint64(newGcp_hi) << 32);
  const Uint32 latestLcpId = masterGCPConf->latestLCP;
  const Uint32 oldestRestorableGci = masterGCPConf->oldestRestorableGCI;
  const Uint32 oldestKeepGci = masterGCPConf->keepGCI;
  const Uint32 saveGCI = masterGCPConf->saveGCI;

  if (latestLcpId > SYSFILE->latestLCP_ID) {
    jam();
#if 0
    g_eventLogger->info("Dbdih: Setting SYSFILE->latestLCP_ID to %d",
                        latestLcpId);
    SYSFILE->latestLCP_ID = latestLcpId;
#endif
    SYSFILE->keepGCI = oldestKeepGci;
    SYSFILE->oldestRestorableGCI = oldestRestorableGci;
    for(Uint32 i = 0; i < NdbNodeBitmask::Size; i++)
      SYSFILE->lcpActive[i] = masterGCPConf->lcpActive[i];
  }//if

  bool ok = false;
  switch (gcpState) {
  case MasterGCPConf::GCP_READY:
    jam();
    ok = true;
    // Either not started or complete...
    break;
  case MasterGCPConf::GCP_PREPARE_RECEIVED:
    jam();
    ok = true;
    if (m_micro_gcp.m_master.m_state == MicroGcp::M_GCP_IDLE)
    {
      jam();
      m_micro_gcp.m_master.m_state = MicroGcp::M_GCP_PREPARE;
      m_micro_gcp.m_master.m_new_gci = newGCI;
    }
    else
    {
      jam();
      ndbrequire(m_micro_gcp.m_master.m_new_gci == newGCI);
    }
    break;
  case MasterGCPConf::GCP_COMMIT_RECEIVED:
    jam();
  case MasterGCPConf::GCP_COMMITTED:
    jam();
    ok = true;
    if (m_micro_gcp.m_master.m_state != MicroGcp::M_GCP_IDLE)
    {
      ndbrequire(m_micro_gcp.m_master.m_new_gci == newGCI);
    }
    m_micro_gcp.m_master.m_new_gci = newGCI;
    m_micro_gcp.m_master.m_state = MicroGcp::M_GCP_COMMIT;
    break;
#ifndef VM_TRACE
  default:
    jamLine(gcpState);
    ndbrequire(false);
#endif
  }
  ndbassert(ok); // Unhandled case...

  ok = false;
  /**
   * GCI should differ with atmost one
   */
  ndbrequire(saveGCI == m_gcp_save.m_gci ||
             saveGCI == m_gcp_save.m_gci + 1 ||
             saveGCI + 1 == m_gcp_save.m_gci);
  if (saveGCI > m_gcp_save.m_master.m_new_gci)
  {
    jam();
    m_gcp_save.m_master.m_new_gci = saveGCI;
  }
  switch(saveState){
  case MasterGCPConf::GCP_SAVE_IDLE:
    jam();
    break;
  case MasterGCPConf::GCP_SAVE_REQ:
    jam();
    if (m_gcp_save.m_master.m_state == GcpSave::GCP_SAVE_IDLE)
    {
      jam();
      m_gcp_save.m_master.m_state = GcpSave::GCP_SAVE_REQ;
    }
    break;
  case MasterGCPConf::GCP_SAVE_CONF:
    jam();
    if (m_gcp_save.m_master.m_state == GcpSave::GCP_SAVE_IDLE)
    {
      jam();
      m_gcp_save.m_master.m_state = GcpSave::GCP_SAVE_REQ;
    }
    break;
  case MasterGCPConf::GCP_SAVE_COPY_GCI:
    jam();
    if (m_gcp_save.m_master.m_state == GcpSave::GCP_SAVE_IDLE)
    {
      jam();
      m_gcp_save.m_master.m_state = GcpSave::GCP_SAVE_COPY_GCI;
    }
    break;
#ifndef VM_TRACE
  default:
    jamLine(saveState);
    ndbrequire(false);
#endif
  }
  //ndbassert(ok); // Unhandled case

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
  cmasterState = MASTER_ACTIVE;

  NdbTick_Invalidate(&m_micro_gcp.m_master.m_start_time);
  NdbTick_Invalidate(&m_gcp_save.m_master.m_start_time);
  if (m_gcp_monitor.m_micro_gcp.m_max_lag_ms > 0)
  {
    infoEvent("GCP Monitor: Computed max GCP_SAVE lag to %u seconds",
              m_gcp_monitor.m_gcp_save.m_max_lag_ms / 1000);
    infoEvent("GCP Monitor: Computed max GCP_COMMIT lag to %u seconds",
              m_gcp_monitor.m_micro_gcp.m_max_lag_ms / 1000);
  }
  else
  {
    infoEvent("GCP Monitor: unlimited lags allowed");
  }

  bool ok = false;
  switch(m_micro_gcp.m_master.m_state){
  case MicroGcp::M_GCP_IDLE:
    jam();
    ok = true;
    signal->theData[0] = DihContinueB::ZSTART_GCP;
    sendSignal(reference(), GSN_CONTINUEB, signal, 1, JBB);
    break;
  case MicroGcp::M_GCP_PREPARE:
  {
    jam();
    ok = true;

    /**
     * Restart GCP_PREPARE
     */
    sendLoopMacro(GCP_PREPARE, sendGCP_PREPARE, RNIL);
    break;
  }
  case MicroGcp::M_GCP_COMMIT:
  {
    jam();
    ok = true;

    /**
     * Restart GCP_COMMIT
     */
    sendLoopMacro(GCP_COMMIT, sendGCP_COMMIT, RNIL);
    break;
  }
  case MicroGcp::M_GCP_COMMITTED:
    jam();
    ndbrequire(false);
  case MicroGcp::M_GCP_COMPLETE:
    jam();
    ndbrequire(false);
#ifndef VM_TRACE
  default:
    jamLine(m_micro_gcp.m_master.m_state);
    ndbrequire(false);
#endif
  }
  ndbassert(ok);

  if (m_micro_gcp.m_enabled == false)
  {
    jam();
    m_gcp_save.m_master.m_state = GcpSave::GCP_SAVE_IDLE;
  }
  else
  {
    ok = false;
    switch(m_gcp_save.m_master.m_state){
    case GcpSave::GCP_SAVE_IDLE:
      jam();
      ok = true;
      break;
    case GcpSave::GCP_SAVE_REQ:
    {
      jam();
      ok = true;
      
      /**
       * Restart GCP_SAVE_REQ
       */
      sendLoopMacro(GCP_SAVEREQ, sendGCP_SAVEREQ, RNIL);
      break;
    }
    case GcpSave::GCP_SAVE_CONF:
      jam();
    case GcpSave::GCP_SAVE_COPY_GCI:
      jam();
      ok = true;
      copyGciLab(signal, CopyGCIReq::GLOBAL_CHECKPOINT);
      m_gcp_save.m_master.m_state = GcpSave::GCP_SAVE_COPY_GCI;
      break;
#ifndef VM_TRACE
    default:
      jamLine(m_gcp_save.m_master.m_state);
      ndbrequire(false);
#endif
    }
    ndbrequire(ok);
  }
  
  signal->theData[0] = NDB_LE_GCP_TakeoverCompleted;
  signal->theData[1] = m_micro_gcp.m_master.m_state;
  signal->theData[2] = m_gcp_save.m_master.m_state;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 1, JBB);

  infoEvent("kk: %u/%u %u %u",
            Uint32(m_micro_gcp.m_current_gci >> 32),
            Uint32(m_micro_gcp.m_current_gci),
            m_micro_gcp.m_master.m_state,
            m_gcp_save.m_master.m_state);

  /*--------------------------------------------------*/
  /*       WE SEPARATE HANDLING OF GLOBAL CHECKPOINTS */
  /*       AND LOCAL CHECKPOINTS HERE. LCP'S HAVE TO  */
  /*       REMOVE ALL FAILED FRAGMENTS BEFORE WE CAN  */
  /*       HANDLE THE LCP PROTOCOL.                   */
  /*--------------------------------------------------*/
  checkLocalNodefailComplete(signal, failedNodeId, NF_GCP_TAKE_OVER);

  startGcpMonitor(signal);
  
  return;
}//Dbdih::masterGcpConfFromFailedLab()

void
Dbdih::handle_send_continueb_invalidate_node_lcp(Signal *signal)
{
  if (ERROR_INSERTED(7204))
  {
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 2000, 3);
  }
  else if (ERROR_INSERTED(7245))
  {
    if (isMaster())
    {
      sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 2000, 3);
    }
    else
    {
      sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 3000, 3);
    }
  }
  else if (ERROR_INSERTED(7246))
  {
    /**
     * This error injection supports a special test case where we
     * delay node 1 and 2 more than other nodes to ensure that we
     * get some nodes that reply with START_INFOCONF and some that
     * reply with START_INFOREF to get the code tested for the case
     * some nodes reply with START_INFOREF and some with START_INFOCONF.
     */
    if (isMaster())
    {
      sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 2000, 3);
    }
    else if (cownNodeId == Uint32(1) ||
             (refToNode(cmasterdihref) == Uint32(1) &&
              cownNodeId == Uint32(2)))
    {
      sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 5000, 3);
    }
    else
    {
      sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 8000, 3);
    }
  }
  else
  {
    sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBB);
  }
}

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
      if (ERROR_INSERTED(7204) ||
          ERROR_INSERTED(7245) ||
          ERROR_INSERTED(7246))
      {
        CLEAR_ERROR_INSERT_VALUE;
      }
      setAllowNodeStart(nodeId, true);
      g_eventLogger->info("Completed invalidation of node %u", nodeId);
      if (getNodeStatus(nodeId) == NodeRecord::STARTING) {
        jam();
        if (!isMaster())
        {
          jam();
          setNodeRecoveryStatus(nodeId, NodeRecord::NODE_GETTING_PERMIT);
        }
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
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal,
                        WaitTableStateChangeMillis, 3);
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
        replicaPtr.i = replicaPtr.p->nextPool) {
      jam();
      c_replicaRecordPool.getPtr(replicaPtr);
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

  handle_send_continueb_invalidate_node_lcp(signal);

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
      if (ERROR_INSERTED(7233))
      {
        CLEAR_ERROR_INSERT_VALUE;
      }

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
  if (!ERROR_INSERTED(7233))
    sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBB);
  else
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 300, 3);
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
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal,
                        WaitTableStateChangeMillis, 3);
    return;
  }//if  

  NodeRecordPtr nodePtr;
  nodePtr.i = nodeId;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
  const Uint32 lcpId = nodePtr.p->m_remove_node_from_table_lcp_id;

  /**
   * For each fragment
   */
  Uint32 noOfRemovedReplicas = 0;     // No of replicas removed
  Uint32 noOfRemovedLcpReplicas = 0;  // No of replicas in LCP removed 
  Uint32 noOfRemainingLcpReplicas = 0;// No of replicas in LCP remaining

  const bool lcpOngoingFlag = (tabPtr.p->tabLcpStatus== TabRecord::TLS_ACTIVE);
  const bool unlogged = (tabPtr.p->tabStorage != TabRecord::ST_NORMAL);
  
  FragmentstorePtr fragPtr;
  for(Uint32 fragNo = 0; fragNo < tabPtr.p->totalfragments; fragNo++){
    jam();
    getFragstore(tabPtr.p, fragNo, fragPtr);    
    
    /**
     * For each of replica record
     */
    bool found = false;
    ReplicaRecordPtr replicaPtr;
    for(replicaPtr.i = fragPtr.p->storedReplicas; replicaPtr.i != RNIL;
        replicaPtr.i = replicaPtr.p->nextPool) {
      jam();

      c_replicaRecordPool.getPtr(replicaPtr);
      if(replicaPtr.p->procNode == nodeId){
        jam();
	found = true;
	noOfRemovedReplicas++;
	removeNodeFromStored(nodeId, fragPtr, replicaPtr, unlogged);
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

        if (lcpId != RNIL)
        {
          jam();
          Uint32 lcpNo = prevLcpNo(replicaPtr.p->nextLcp);
          if (replicaPtr.p->lcpStatus[lcpNo] == ZVALID && 
              replicaPtr.p->lcpId[lcpNo] == lcpId)
          {
            jam();
            replicaPtr.p->lcpStatus[lcpNo] = ZINVALID;       
            replicaPtr.p->lcpId[lcpNo] = 0;
            replicaPtr.p->nextLcp = lcpNo;
            g_eventLogger->debug("REMOVING lcp: %u from table: %u frag:"
                                 " %u node: %u",
                                 SYSFILE->latestLCP_ID,
                                 tabPtr.i,
                                 fragNo,
                                 nodeId);
          }
        }
      }
    }

    /**
     * Run updateNodeInfo to remove any dead nodes from list of activeNodes
     *  see bug#15587
     */
    updateNodeInfo(fragPtr);
    noOfRemainingLcpReplicas += fragPtr.p->noLcpReplicas;
  }
  
  if (noOfRemovedReplicas == 0)
  {
    jam();
    /**
     * The table had no replica on the failed node
     *   continue with next table
     */
    tabPtr.i++;
    signal->theData[0] = DihContinueB::ZREMOVE_NODE_FROM_TABLE;
    signal->theData[1] = nodeId;
    signal->theData[2] = tabPtr.i;
    if (!ERROR_INSERTED(7233))
      sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBB);
    else
      sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 300, 3);
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
  ndbrequire(tabPtr.p->tabLcpStatus == TabRecord::TLS_ACTIVE);
  
  tabPtr.p->tabCopyStatus = TabRecord::CS_REMOVE_NODE;
  tabPtr.p->tabUpdateState = TabRecord::US_REMOVE_NODE;
  tabPtr.p->tabRemoveNode = nodeId;
  signal->theData[0] = DihContinueB::ZPACK_TABLE_INTO_PAGES;
  signal->theData[1] = tabPtr.i;
  sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
    
  if (noOfRemainingLcpReplicas == 0)
  {
    jam();
    /**
     * Check if the removal on the failed node made the LCP complete
     */
    tabPtr.p->tabLcpStatus = TabRecord::TLS_WRITING_TO_FILE;
    checkLcpAllTablesDoneInLqh(__LINE__);
  }
}
  
void
Dbdih::removeNodeFromTablesComplete(Signal* signal, Uint32 nodeId)
{
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

  if (ERROR_INSERTED(7030))
  {
    g_eventLogger->info("Reenable GCP_PREPARE");
    CLEAR_ERROR_INSERT_VALUE;
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

void
Dbdih::execEMPTY_LCP_REP(Signal* signal)
{
  jamEntry();
  EmptyLcpRep* rep = (EmptyLcpRep*)signal->getDataPtr();
  
  Uint32 len = signal->getLength();
  ndbrequire(len > EmptyLcpRep::SignalLength);
  len -= EmptyLcpRep::SignalLength;

  NdbNodeBitmask nodes;
  nodes.assign(NdbNodeBitmask::Size, rep->receiverGroup);
  NodeReceiverGroup rg (DBDIH, nodes);
  memmove(signal->getDataPtrSend(), 
          signal->getDataPtr()+EmptyLcpRep::SignalLength, 4*len);
  
  sendSignal(rg, GSN_EMPTY_LCP_CONF, signal, len, JBB);
}

void Dbdih::execEMPTY_LCP_CONF(Signal* signal)
{
  jamEntry();
 
  ndbrequire(c_lcpMasterTakeOverState.state == LMTOS_WAIT_EMPTY_LCP);
  
  const EmptyLcpConf * const conf = (EmptyLcpConf *)&signal->theData[0];
  Uint32 nodeId = conf->senderNodeId;

  CRASH_INSERTION(7206);


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
Dbdih::checkEmptyLcpComplete(Signal *signal)
{
  
  ndbrequire(c_lcpMasterTakeOverState.state == LMTOS_WAIT_LCP_FRAG_REP);
  
  if(c_lcpState.noOfLcpFragRepOutstanding > 0 &&
     c_lcpMasterTakeOverState.use_empty_lcp)
  {
    jam();
    /**
     * In the EMPTY_LCP_REQ we need to ensure that we have received
     * LCP_FRAG_REP for all outstanding LCP_FRAG_ORDs. So we need to wait
     * here for all to complete before we are ready to move on.
     * 
     * This is not needed when LQH can remove duplicate LCP_FRAG_ORDs, so
     * we can proceed with the master takeover immediately.
     */
    return;
  }
  
  if(isMaster()){
    jam();

    signal->theData[0] = NDB_LE_LCP_TakeoverStarted;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 1, JBB);
    
    signal->theData[0] = 7012;
    execDUMP_STATE_ORD(signal);

    if (ERROR_INSERTED(7194))
    {
      ndbout_c("7194 starting ZREMOVE_NODE_FROM_TABLE");
      signal->theData[0] = DihContinueB::ZREMOVE_NODE_FROM_TABLE;
      signal->theData[1] = c_lcpMasterTakeOverState.failedNodeId;
      signal->theData[2] = 0; // Tab id
      sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBB);
    }
    
    c_lcpMasterTakeOverState.set(LMTOS_INITIAL, __LINE__);
    MasterLCPReq * const req = (MasterLCPReq *)&signal->theData[0];
    req->masterRef = reference();
    req->failedNodeId = c_lcpMasterTakeOverState.failedNodeId;
    sendLoopMacro(MASTER_LCPREQ, sendMASTER_LCPREQ, RNIL);

  }
  else
  {
    jam();
    sendMASTER_LCPCONF(signal, __LINE__);
  }
}

/*--------------------------------------------------*/
/*       THE MASTER HAS FAILED AND THE NEW MASTER IS*/
/*       QUERYING THIS NODE ABOUT THE STATE OF THE  */
/*       LOCAL CHECKPOINT PROTOCOL.                 */
/*--------------------------------------------------*/
void Dbdih::execMASTER_LCPREQ(Signal* signal) 
{
  NodeRecordPtr newMasterNodePtr;
  const MasterLCPReq * const req = (MasterLCPReq *)&signal->theData[0];
  jamEntry();
  const BlockReference newMasterBlockref = req->masterRef;

  newMasterNodePtr.i = refToNode(newMasterBlockref);
  ptrCheckGuard(newMasterNodePtr, MAX_NDB_NODES, nodeRecord);

  if (newMasterNodePtr.p->nodeStatus != NodeRecord::ALIVE)
  {
    /**
     * We delayed the MASTER_LCPREQ signal and now it arrived after
     * the new master already died. We ignore this signal.
     */
    jam();
    return;
  }

  CRASH_INSERTION(7205);

  if (ERROR_INSERTED(7207))
  {
    jam();
    SET_ERROR_INSERT_VALUE(7208);
    sendSignalWithDelay(reference(), GSN_MASTER_LCPREQ, signal,
			500, signal->getLength());
    return;
  }
  
  if (ERROR_INSERTED(7208))
  {
    jam();
    signal->theData[0] = 9999;
    sendSignal(numberToRef(CMVMI, refToNode(newMasterBlockref)), 
               GSN_NDB_TAMPER, signal, 1, JBB);
  }

  if (ERROR_INSERTED(7231))
  {
    CLEAR_ERROR_INSERT_VALUE;
    sendSignalWithDelay(reference(), GSN_MASTER_LCPREQ, signal,
			1500, signal->getLength());
    return;
  }

  if (newMasterBlockref != cmasterdihref)
  {
    /**
     * We haven't processed the NODE_FAILREP signal causing the new master
     * to be selected as the new master by this node.
     */
    jam();
    ndbout_c("resending GSN_MASTER_LCPREQ");
    sendSignalWithDelay(reference(), GSN_MASTER_LCPREQ, signal,
			50, signal->getLength());
    return;
  }

  if (c_handled_master_take_over_copy_gci != refToNode(newMasterNodePtr.i))
  {
    /**
     * We need to ensure that MASTER_GCPREQ has ensured that the COPY_GCIREQ
     * activity started by old master has been completed before we proceed
     * with handling the take over of the LCP protocol.
     */
    jam();
    sendSignalWithDelay(reference(), GSN_MASTER_LCPREQ, signal,
                        10, signal->getLength());
    return;
  }
  c_handled_master_take_over_copy_gci = 0;

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

  if (c_lcpState.lcpStatus == LCP_INIT_TABLES)
  {
    jam();
    c_lcpState.setLcpStatus(LCP_STATUS_IDLE, __LINE__);
  }
  sendMASTER_LCPCONF(signal, __LINE__);
}//Dbdih::execMASTER_LCPREQ()

void
Dbdih::sendMASTER_LCPCONF(Signal * signal, Uint32 from)
{
  if (!c_lcpState.m_MASTER_LCPREQ_Received)
  {
    jam();
    /**
     * Has not received MASTER_LCPREQ yet
     */
    return;
  }

#if defined VM_TRACE || defined ERROR_INSERT
  bool info = true;
#else
  bool info = false;
#endif

  if (ERROR_INSERTED(7230))
  {
    signal->theData[0] = 9999;
    sendSignalWithDelay(CMVMI_REF, GSN_NDB_TAMPER, signal, 100, 1);
    goto err7230;
  }

  if (!c_EMPTY_LCP_REQ_Counter.done())
  {
    /**
     * Have not received all EMPTY_LCP_REP
     * dare not answer MASTER_LCP_CONF yet
     */
    jam();
    if (info)
      infoEvent("from: %u : c_EMPTY_LCP_REQ_Counter.done() == false", from);
    return;
  }

  if (c_lcpState.lcpStatus == LCP_INIT_TABLES)
  {
    jam();
    /**
     * Still aborting old initLcpLab
     */
    if (info)
      infoEvent("from: %u : c_lcpState.lcpStatus == LCP_INIT_TABLES", from);
    return;
  }

err7230:
  if (info)
    infoEvent("from: %u : sendMASTER_LCPCONF", from);

  if (c_lcpState.lcpStatus == LCP_COPY_GCI)
  {
    jam();
    /**
     * Restart it
     */
    //Uint32 lcpId = SYSFILE->latestLCP_ID;
    SYSFILE->latestLCP_ID--;
    Sysfile::clearLCPOngoing(SYSFILE->systemRestartBits);
    c_lcpState.setLcpStatus(LCP_STATUS_IDLE, __LINE__);
#if 0
    if(c_copyGCISlave.m_copyReason == CopyGCIReq::LOCAL_CHECKPOINT){
      g_eventLogger->info("Dbdih: Also resetting c_copyGCISlave");
      c_copyGCISlave.m_copyReason = CopyGCIReq::IDLE;
      c_copyGCISlave.m_expectedNextWord = 0;
    }
#endif
  }

  MasterLCPConf::State lcpState;
  switch (c_lcpState.lcpStatus) {
  case LCP_STATUS_IDLE:
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
    jam();
    /*--------------------------------------------------*/
    /*       COPY OF RESTART INFORMATION HAS BEEN       */
    /*       PERFORMED AND ALSO RESPONSE HAVE BEEN SENT.*/
    /*--------------------------------------------------*/
    lcpState = MasterLCPConf::LCP_STATUS_ACTIVE;
    break;
  case LCP_TAB_COMPLETED:
    jam();
    /*--------------------------------------------------------*/
    /*       ALL LCP_REPORT'S HAVE BEEN COMPLETED FOR         */
    /*       ALL TABLES.     SAVE OF AT LEAST ONE TABLE IS    */
    /*       ONGOING YET.                                     */
    /*--------------------------------------------------------*/
    lcpState = MasterLCPConf::LCP_TAB_COMPLETED;
    break;
  case LCP_TAB_SAVED:
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
  case LCP_WAIT_MUTEX:
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
    /**
     * These two states are handled by if statements above
     */
    ndbrequire(false);
    lcpState= MasterLCPConf::LCP_STATUS_IDLE; // remove warning
    break;
  default:
    ndbrequire(false);
    lcpState= MasterLCPConf::LCP_STATUS_IDLE; // remove warning
  }//switch

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

  CRASH_INSERTION(7232);

  if (ERROR_INSERTED(7230))
  {
    return;
  }

  if(c_lcpState.lcpStatus == LCP_TAB_SAVED){
#ifdef VM_TRACE
    g_eventLogger->info("Sending extra GSN_LCP_COMPLETE_REP to new master");    
#endif
    sendLCP_COMPLETE_REP(signal);
  }

  if(!isMaster())
  {
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

  if (ERROR_INSERTED(7194))
  {
    ndbout_c("delaying MASTER_LCPCONF due to error 7194");
    sendSignalWithDelay(reference(), GSN_MASTER_LCPCONF, signal, 
                        300, signal->getLength());
    return;
  }

  if (ERROR_INSERTED(7230) &&
      refToNode(signal->getSendersBlockRef()) != getOwnNodeId())
  {
    infoEvent("delaying MASTER_LCPCONF due to error 7230 (from %u)",
              refToNode(signal->getSendersBlockRef()));
    sendSignalWithDelay(reference(), GSN_MASTER_LCPCONF, signal,
                        300, signal->getLength());
    return;
  }

  Uint32 senderNodeId = conf->senderNodeId;
  MasterLCPConf::State lcpState = (MasterLCPConf::State)conf->lcpState;
  const Uint32 failedNodeId = conf->failedNodeId;
  NodeRecordPtr nodePtr;
  nodePtr.i = senderNodeId;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
  nodePtr.p->lcpStateAtTakeOver = lcpState;

  CRASH_INSERTION(7180);
  
#ifdef VM_TRACE
  g_eventLogger->info("MASTER_LCPCONF from node %u", senderNodeId);
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

  Uint32 senderNodeId = ref->senderNodeId;
  Uint32 failedNodeId = ref->failedNodeId;
  
  if (c_lcpState.m_LCP_COMPLETE_REP_Counter_LQH.isWaitingFor(senderNodeId))
  {
    jam();
    c_lcpState.m_LCP_COMPLETE_REP_Counter_LQH.clearWaitingFor(senderNodeId);
  }

  receiveLoopMacro(MASTER_LCPREQ, senderNodeId);
  /*-------------------------------------------------------------------------*/
  // We have now received all responses and are ready to take over the LCP
  // protocol as master.
  /*-------------------------------------------------------------------------*/
  MASTER_LCPhandling(signal, failedNodeId);
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
    g_eventLogger->info("MASTER_LCPhandling:: LMTOS_ALL_IDLE -> checkLcpStart");
#endif
    checkLcpStart(signal, __LINE__, 0);
    break;
  case LMTOS_COPY_ONGOING:
    jam();
    /* --------------------------------------------------------------------- */
    // We were in the starting process of the LCP protocol. We will restart the
    // protocol by calculating the keep gci and storing the new lcp id.
    /* --------------------------------------------------------------------- */
#ifdef VM_TRACE
    g_eventLogger->info("MASTER_LCPhandling:: LMTOS_COPY_ONGOING -> storeNewLcpId");
#endif
    if (c_lcpState.lcpStatus == LCP_STATUS_ACTIVE) {
      jam();
      /*---------------------------------------------------------------------*/
      /*  WE NEED TO DECREASE THE LATEST LCP ID SINCE WE HAVE ALREADY        */
      /*  STARTED THIS */
      /*  LOCAL CHECKPOINT.                                                  */
      /*---------------------------------------------------------------------*/
#ifdef VM_TRACE
      Uint32 lcpId = SYSFILE->latestLCP_ID;
      g_eventLogger->info("Decreasing latestLCP_ID from %d to %d", lcpId, lcpId - 1);
#endif
      SYSFILE->latestLCP_ID--;
    }//if
    start_lcp_before_mutex(signal);
    break;
  case LMTOS_ALL_ACTIVE:
    {
      jam();
      /* ------------------------------------------------------------------- 
       * Everybody was in the active phase. We will restart sending 
       * LCP_FRAG_ORD to the nodes from the new master. 
       * We also need to set dihLcpStatus to ZACTIVE
       * in the master node since the master will wait for all nodes to 
       * complete before finalising the LCP process.
       * ------------------------------------------------------------------ */
#ifdef VM_TRACE
      g_eventLogger->info("MASTER_LCPhandling:: LMTOS_ALL_ACTIVE -> "
                          "startLcpRoundLoopLab(table=%u, fragment=%u)",
                          c_lcpMasterTakeOverState.minTableId,
                          c_lcpMasterTakeOverState.minFragId);
#endif
    
      c_lcpState.keepGci = SYSFILE->keepGCI;

      /**
       * We need not protect against ongoing copy of meta data here since
       * that cannot be ongoing while we are taking over as master. The
       * reason is that a starting node will always fail also if any node
       * fails in the middle of the start process.
       */
      c_lcp_runs_with_pause_support = check_if_pause_lcp_possible();
      if (!c_lcp_runs_with_pause_support)
      {
        jam();
        /**
         * We need to reaquire the mutex...
         */
        Mutex mutex(signal, c_mutexMgr, c_fragmentInfoMutex_lcp);
        Callback c = 
          { safe_cast(&Dbdih::master_lcp_fragmentMutex_locked),
            failedNodePtr.i
          };
        ndbrequire(mutex.lock(c, false));
      }
      else
      {
        jam();
        /* No mutex is needed, call callback function immediately */
        master_lcp_fragmentMutex_locked(signal, failedNodePtr.i, 0);
      }
      return;
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

      c_lcp_runs_with_pause_support = check_if_pause_lcp_possible();
      if (!c_lcp_runs_with_pause_support)
      {
        jam();
        /**
         * We need to reaquire the mutex...
         * We have nodes in the cluster without support of pause lcp.
         */
        Mutex mutex(signal, c_mutexMgr, c_fragmentInfoMutex_lcp);
        Callback c = 
          { safe_cast(&Dbdih::master_lcp_fragmentMutex_locked),
            failedNodePtr.i
          };
        ndbrequire(mutex.lock(c, false));
      }
      else
      {
        jam();
        /* No mutex is needed, call callback function immediately */
        master_lcp_fragmentMutex_locked(signal, failedNodePtr.i, 0);
      }
      return;
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
  setNodeRecoveryStatus(failedNodePtr.i, NodeRecord::NODE_FAILURE_COMPLETED);
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

static inline void inc_node_or_group(Uint32 &node, Uint32 max_node)
{
  Uint32 next = node + 1;
  node = (next == max_node ? 0 : next);
}

/*
  Spread fragments in backwards compatible mode
*/
static void set_default_node_groups(Signal *signal, Uint32 noFrags)
{
  Uint16 *node_group_array = (Uint16*)&signal->theData[25];
  Uint32 i;
  for (i = 0; i < noFrags; i++)
    node_group_array[i] = NDB_UNDEF_NODEGROUP;
}

static Uint32 find_min_index(const Uint16* array, Uint32 cnt, Uint32 start_pos)
{
  Uint32 m = start_pos;
  Uint32 min_value = array[start_pos];

  for (Uint32 i = start_pos + 1; i<cnt; i++)
  {
    if (array[i] < min_value)
    {
      m = i;
      min_value = array[i];
    }
  }
  for (Uint32 i = 0; i < start_pos; i++)
  {
    if (array[i] < min_value)
    {
      m = i;
      min_value = array[i];
    }
  }
  return m;
}

Uint32
Dbdih::getFragmentsPerNode()
{
  jam();
  if (c_fragments_per_node_ != 0)
  {
    return c_fragments_per_node_;
  }

  c_fragments_per_node_ = getLqhWorkers();
  if (c_fragments_per_node_ == 0)
    c_fragments_per_node_ = 1; // ndbd

  NodeRecordPtr nodePtr;
  nodePtr.i = cfirstAliveNode;
  do
  {
    jam();
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
    Uint32 workers = getNodeInfo(nodePtr.i).m_lqh_workers;
    if (workers == 0) // ndbd
      workers = 1;

    c_fragments_per_node_ = MIN(workers, c_fragments_per_node_);
    nodePtr.i = nodePtr.p->nextNode;
  } while (nodePtr.i != RNIL);

  if (c_fragments_per_node_ == 0)
  {
    ndbassert(false);
    c_fragments_per_node_ = 1;
  }
#ifdef VM_TRACE
  ndbout_c("Using %u fragments per node", c_fragments_per_node_);
#endif
  return c_fragments_per_node_;
}

void
Dbdih::init_next_replica_node(
  Uint16 (*next_replica_node)[MAX_NDB_NODE_GROUPS][NDBMT_MAX_WORKER_INSTANCES],
  Uint32 noOfReplicas)
{
  for (Uint32 i = 0; i < MAX_NDB_NODE_GROUPS; i++)
  {
    for (Uint32 j = 0; j < NDBMT_MAX_WORKER_INSTANCES; j++)
    {
      (*next_replica_node)[i][j] = (j % noOfReplicas);
    }
  }
}

/**
 * CREATE_FRAGMENTATION_REQ
 *
 * CREATE_FRAGMENTATION_REQ returns a FRAGMENTATION structure, a.k.a.
 * ReplicaData in Ndbapi.
 *
 * The FRAGMENTATION structure contains a mapping from fragment id to log part
 * id and a node id for each fragment replica, the first node id is for primary
 * replica.
 *
 * FRAGMENTATION contains of an array of Uint16 values:
 *
 * 0: #replicas
 * 1: #fragments
 * 2 + fragmentId*(1 + #replicas) + 0: log part id
 * 2 + fragmentId*(1 + #replicas) + 1: primary replica node id
 * 2 + fragmentId*(1 + #replicas) + 2: backup replica node id
 * ...
 *
 * CREATE_FRAGMENTATION_REQ supports three request types selected by setting
 * requestInfo in signal.
 *
 * requestInfo             | Description
 * ------------------------+----------------------------------------------
 * RI_CREATE_FRAGMENTATION | Create a new fragmentation.
 * RI_ADD_FRAGMENTS        | Adjust a fragmentation by adding fragments.
 * RI_GET_FRAGMENTATION    | Return the current fragmentation for a table.
 *
 * == Common parameters for all request types ==
 *
 *   senderRef - Used if response should be sent by signal, only used in old
 *       versions before and including 5.0.96, otherwise it must be zero.  New
 *       uses of GSN_CREATE_FRAGMENTATION_REQ must be executed using
 *       EXECUTE_DIRECT.
 *
 *   senderData - Used if senderRef is non-zero.
 *
 *   Fragmentation is returned in theData[25..] and caller must ensure theData
 *   is big enough for storing the fragmentation.
 *
 * == Values for unused parameters ==
 *
 *   senderRef         = 0
 *   senderData        = RNIL
 *   requestInfo  Must be set!
 *   fragmentationType = 0
 *   partitionBalance = 0
 *   primaryTableId    = RNIL
 *   noOfFragments     = 0
 *   partitionCount    = 0
 *   map_ptr_i         = RNIL
 *
 * == Create fragmentation (requestInfo RI_CREATE_FRAGMENTATION) ==
 *
 *   noOfFragments - Used by some fragmentation types, see fragmentationType
 *       below.
 *
 *   partitionCount - Must be same as noOfFragments, unless fragmentation is
 *       for a fully replicated table.  For fully replicated tables
 *       noOfFragments must be a multiple of partitionCount.
 *
 *   fragmentationType - Specifies how table is partitioned into fragments.
 *       Since MySQL Cluster 7.0 server only uses UserDefined and
 *       HashMapPartition.  Other types can occur from restoring old Ndb
 *       backups, or using Ndbapi directly.
 *
 *         AllNodesSmallTable - noOfFragments is set to 1 per LDM.
 *
 *         AllNodesMediumTable - noOfFragments is set to 2 per LDM.
 *
 *         AllNodesLargeTable - noOfFragments is set to 4 per LDM.
 *
 *         SingleFragment - noOfFragments is set to one.
 *
 *         DistrKeyHash
 *         DistrKeyLin
 *           If noOfFragments is zero, noOfFragments is set to 1 per LDM.
 *           FragmentData from theData[25..] is used if noOfFragments from
 *           signal is non-zero.
 *
 *         UserDefined - noOfFragment must be non zero.  FragmentData from
 *             theData[25..] is used.
 *
 *         HashMapPartition - Hashmap to use is given by map_ptr_i which must
 *             be set (not RNIL).  Both noOfFragments and partitionCount must
 *             be set.  Further more partitionCount must be equal to hashmaps
 *             partition count (m_fragments).
 *             For fully replicated tables, noOfFragments should be a multiple
 *             of partitionCount.
 *
 *   partitionBalance - Determines how the number of fragments depends on
 *       cluster configuration such as number of replicas, number of
 *       nodegroups, and, number of LDM per node.  The parameter is only used
 *       for HashMapPartition.
 *
 *   FragmentData theData[25..] - An array of Uint16 mapping each fragment to
 *       a nodegroup.  NDB_UNDEF_NODEGROUP is used to mark that no specific
 *       nodegroup is wanted for fragment.
 *
 * == Adjust fragmentation by adding fragments (requestInfo RI_ADD_PARTITION) ==
 *
 *   primaryTableId - Id of table fragmentation to adjust, must not be RNIL.
 *
 *   noOfFragments - New fragment count must be set (non zero).  Old fragment
 *       count is taken from old fragmentation for table.
 *
 *   partitionCount - New partition count.  For non fully replicated tables
 *       partitionCount must be same as noOfFragments.  For fully replicated
 *       tables partitionCount must be the same as the old partitionCount.
 *
 *   map_ptr_i - Is not used from signal but taken from old fragmentation.
 *
 *   fragmentationType - Must be HashMapPartition or DistrKeyOrderedIndex.
 *
 * == Get fragmentation (requestInfo RI_GET_FRAGMENTATION) ==
 *
 *   primaryTableId - Id of table whic fragmentation to return, must not be RNIL.
 *
 * No other parameters are used from signal (except for the common parameters).
 *
 */
void Dbdih::execCREATE_FRAGMENTATION_REQ(Signal * signal)
{
  jamEntry();
  CreateFragmentationReq * const req = 
    (CreateFragmentationReq*)signal->getDataPtr();
  
  const Uint32 senderRef = req->senderRef;
  const Uint32 senderData = req->senderData;
  Uint32 noOfFragments = req->noOfFragments;
  const Uint32 fragType = req->fragmentationType;
  const Uint32 primaryTableId = req->primaryTableId;
  const Uint32 map_ptr_i = req->map_ptr_i;
  const Uint32 flags = req->requestInfo;
  const Uint32 partitionBalance = req->partitionBalance;
  Uint32 partitionCount = req->partitionCount;
  Uint32 err = 0;
  bool use_specific_fragment_count = false;
  const Uint32 defaultFragments =
    getFragmentsPerNode() * cnoOfNodeGroups * cnoReplicas;
  const Uint32 maxFragments =
    MAX_FRAG_PER_LQH * getFragmentsPerNode() * cnoOfNodeGroups;

  if (flags != CreateFragmentationReq::RI_GET_FRAGMENTATION)
  {
    D("CREATE_FRAGMENTATION_REQ: " <<
      " primaryTableId: " << primaryTableId <<
      " partitionBalance: " <<
        getPartitionBalanceString(partitionBalance) <<
      " fragType: " << fragType <<
      " noOfFragments: " << noOfFragments);
  }

  do {
    NodeGroupRecordPtr NGPtr;
    TabRecordPtr primTabPtr;
    Uint32 count = 2;
    Uint16 noOfReplicas = cnoReplicas;
    Uint16 *fragments = (Uint16*)(signal->theData+25);
    if (primaryTableId == RNIL) {
      jam();
      switch ((DictTabInfo::FragmentType)fragType){
        /*
          Backward compatability and for all places in code not changed.
        */
      case DictTabInfo::AllNodesSmallTable:
        jam();
        noOfFragments = defaultFragments;
        partitionCount = noOfFragments;
        set_default_node_groups(signal, noOfFragments);
        break;
      case DictTabInfo::AllNodesMediumTable:
        jam();
        noOfFragments = 2 * defaultFragments;
        if (noOfFragments > maxFragments)
          noOfFragments = maxFragments;
        partitionCount = noOfFragments;
        set_default_node_groups(signal, noOfFragments);
        break;
      case DictTabInfo::AllNodesLargeTable:
        jam();
        noOfFragments = 4 * defaultFragments;
        if (noOfFragments > maxFragments)
          noOfFragments = maxFragments;
        partitionCount = noOfFragments;
        set_default_node_groups(signal, noOfFragments);
        break;
      case DictTabInfo::SingleFragment:
        jam();
        noOfFragments = 1;
        partitionCount = noOfFragments;
        use_specific_fragment_count = true;
        set_default_node_groups(signal, noOfFragments);
        break;
      case DictTabInfo::DistrKeyHash:
        jam();
      case DictTabInfo::DistrKeyLin:
        jam();
        if (noOfFragments == 0)
        {
          jam();
          noOfFragments = defaultFragments;
          partitionCount = noOfFragments;
          set_default_node_groups(signal, noOfFragments);
        }
        else
        {
          jam();
          ndbrequire(noOfFragments == partitionCount);
          use_specific_fragment_count = true;
        }
        break;
      case DictTabInfo::UserDefined:
        jam();
        use_specific_fragment_count = true;
        if (noOfFragments == 0)
        {
          jam();
          err = CreateFragmentationRef::InvalidFragmentationType;
        }
        break;
      case DictTabInfo::HashMapPartition:
      {
        jam();
        ndbrequire(map_ptr_i != RNIL);
        Ptr<Hash2FragmentMap> ptr;
        g_hash_map.getPtr(ptr, map_ptr_i);
        if (noOfFragments == 0 ||
            partitionCount != ptr.p->m_fragments ||
            noOfFragments % partitionCount != 0)
        {
          jam();
          err = CreateFragmentationRef::InvalidFragmentationType;
          break;
        }
        set_default_node_groups(signal, noOfFragments);
        break;
      }
      case DictTabInfo::DistrKeyOrderedIndex:
        jam();
      default:
        jam();
        err = CreateFragmentationRef::InvalidFragmentationType;
      }
      if (err)
        break;
      /*
        When we come here the the exact partition is specified
        and there is an array of node groups sent along as well.
      */
      memcpy(&tmp_node_group_id[0], &signal->theData[25], 2 * noOfFragments);
      Uint16 (*next_replica_node)[MAX_NDB_NODE_GROUPS][NDBMT_MAX_WORKER_INSTANCES] =
        &tmp_next_replica_node;
      init_next_replica_node(&tmp_next_replica_node, noOfReplicas);

      Uint32 default_node_group= 0;
      Uint32 next_log_part = 0;
      if ((DictTabInfo::FragmentType)fragType == DictTabInfo::HashMapPartition)
      {
        jam();
        if (partitionBalance != NDB_PARTITION_BALANCE_FOR_RP_BY_LDM)
        {
          jam();
          /**
           * The default partitioned table using FOR_RP_BY_LDM will
           * distribute exactly one primary replica to each LDM in each node,
           * so no need to use the information from other table creations to
           * define the primary replica node mapping. For all other tables
           * we will attempt to spread the replicas around by using a variable
           * in the master node that contains information about other tables
           * and how those have been distributed.
           */
          next_replica_node = &c_next_replica_node;
        }
        switch (partitionBalance)
        {
          case NDB_PARTITION_BALANCE_FOR_RP_BY_NODE:
          case NDB_PARTITION_BALANCE_FOR_RA_BY_NODE:
          {
            /**
             * Table will only use one log part, we will try spreading over
             * different log parts, however the variable isn't persistent, so
             * recommendation is to use only small tables for these
             * partition balances.
             *
             * One per node type will use one LDM per replica since fragment
             * count is higher.
             */
            jam();
            use_specific_fragment_count = true;
            break;
          }
          case NDB_PARTITION_BALANCE_FOR_RP_BY_LDM:
          case NDB_PARTITION_BALANCE_FOR_RA_BY_LDM:
          {
            /**
             * These tables will spread over all LDMs and over all node
             * groups. We will start with LDM 0 by setting next_log_part
             * to -1 and when we do ++ on first fragment in node group
             * 0 it will be set to 0.
             * We won't touch m_next_log_part in this case since it won't
             * change its value anyways.
             *
             * This is the same as the default behaviour except that the
             * old behaviour could be affected by previous tables. This
             * behaviour is now removed.
             */
            jam();
            next_log_part = (~0);
            break;
          }
          case NDB_PARTITION_BALANCE_SPECIFIC:
          {
            jam();
            use_specific_fragment_count = true;
            break;
          }
          default:
          {
            ndbrequire(false);
            break;
          }
        }
      }
      else
      {
        /**
         * The only table type supported is HashMaps, so we can change the
         * mapping of non-HashMap tables to a more stringent one. We will
         * still always start at LDM 0 except for tables defined to have
         * non-standard fragment counts. In this case we will start at
         * m_next_log_part to attempt in spreading out the use on the
         * LDMs although we won't perform a perfect job.
         */
        next_replica_node = &c_next_replica_node;
        if (!use_specific_fragment_count)
        {
          jam();
          next_log_part = (~0);
        }
      }
      /**
       * Fragments are spread out in 3 different dimensions.
       * 1) Node group dimension, each fragment belongs to a node group.
       * 2) LDM instance dimenstion, each fragment is mapped to one of the
       *    LDMs.
       * 3) Primary replica dimension, each fragment maps the primary replica
       *    to one of the nodes in the node group.
       *
       * Node group Dimension:
       * ---------------------
       * Here the fragments are spread out in easy manner by placing the first
       * fragment in Node Group 0, the next in Node Group 1 (if there is one).
       * When we have mapped a fragment into each node group, then we restart
       * from Node Group 0.
       *
       * LDM dimension:
       * --------------
       * The default behaviour in 7.4 and earlier was to spread those in the
       * same manner as node groups, one started at the next LDM to receive
       * a fragment, this is normally LDM 0. The next fragment is mapped to
       * next LDM, normally 1 (if it exists). One proceeds like this until
       * one reaches the last LDM, then one starts again from LDM 0.
       * A variable m_next_log_part is kept for as long as the node lives.
       * Thus we cannot really tell on beforehand where fragments will end
       * up in this fragmentation scheme.
       *
       * We have changed the behaviour for normal tables in 7.5. Now we will
       * always start from LDM 0, we will use LDM 0 until all node groups
       * have received one fragment in LDM 0. Then when we return to Node
       * Group 0 we will step to LDM 1. When we reach the last LDM we will
       * step back to LDM 0 again.
       *
       * For tables with specific fragment count we will use the same mapping
       * algorithm except that we will start on the next LDM that was saved
       * from creating the last table with specific fragment count.
       * This means that tables that have a small number of fragments we will
       * attempt to spread them and this has precedence before predictable
       * fragmentation.
       *
       * For fully replicated tables that use all LDMs we want the primary
       * fragments to be the first ones. Thus we ensure that the first
       * fragments are all stored in Node Group 0 with increasing LDM number.
       * If we only have one fragment per Node Group then no changes are
       * needed for this. We discover fully replicated tables through the
       * fact that noOfFragments != partitionCount. This actually only
       * differs with fully replicated tables that are created with more
       * than one node group. One node group will however work with the
       * traditional algorithm since it then becomes the same.
       *
       * Primary replica dimension:
       * --------------------------
       * We will start with the first node in each node group in the first
       * round of node groups and with LDM 0. In the second turn for LDM 1
       * we will use the second node in the node group. In this manner we
       * will get a decent spreading of primary replicas on the nodes in the
       * node groups. It won't be perfect, but when we support read from
       * backup replicas the need to handle primary replica and backup
       * replica is much smaller.
       *
       * We keep information about tables previously created to try to get
       * an even distribution of the primary replicas in different tables
       * in the cluster.
       */

      if (use_specific_fragment_count)
      {
        jam();
        default_node_group = c_nextNodeGroup;
      }
      for(Uint32 fragNo = 0; fragNo < noOfFragments; fragNo++)
      {
        jam();
        NGPtr.i = tmp_node_group_id[fragNo];
        ndbrequire(default_node_group < MAX_NDB_NODE_GROUPS);
        if (NGPtr.i == NDB_UNDEF_NODEGROUP)
        {
          jam();
	  NGPtr.i = c_node_groups[default_node_group];
        }
        if (NGPtr.i >= MAX_NDB_NODE_GROUPS)
        {
          jam();
          err = CreateFragmentationRef::InvalidNodeGroup;
          break;
        }
        ptrCheckGuard(NGPtr, MAX_NDB_NODE_GROUPS, nodeGroupRecord);
        if (NGPtr.p->nodegroupIndex == RNIL)
        {
          jam();
          err = CreateFragmentationRef::InvalidNodeGroup;
          break;
        }
        Uint32 logPart;
        if (use_specific_fragment_count)
        {
          jam();
          /**
           * Time to increment to next LDM
           * Most tables use one fragment per LDM, but if there are
           * tables that only use one LDM we make sure in this manner that
           * those tables are spread over different LDMs.
           *
           * This means that the first fragment can end up a bit
           * anywhere, but there will still be a good spread of
           * the fragments over the LDMs.
           */
          logPart = NGPtr.p->m_next_log_part++ % globalData.ndbLogParts;
        }
        else
        {
          jam();
          if (NGPtr.i == 0 ||
              (noOfFragments != partitionCount))
          {
            /** Fully replicated table with one fragment per LDM first
             * distributed over all LDMs before moving to the next
             * node group.
             */
            jam();
            next_log_part++;
          }
          logPart = next_log_part % globalData.ndbLogParts;
        }
        ndbrequire(logPart < NDBMT_MAX_WORKER_INSTANCES);
        fragments[count++] = logPart; // Store logpart first

        /* Select primary replica node as next index in double array */
        Uint32 node_index = (*next_replica_node)[NGPtr.i][logPart];
        ndbrequire(node_index < noOfReplicas);

        for(Uint32 replicaNo = 0; replicaNo < noOfReplicas; replicaNo++)
        {
          jam();
          const Uint16 nodeId = NGPtr.p->nodesInGroup[node_index];
          fragments[count++]= nodeId;
          inc_node_or_group(node_index, NGPtr.p->nodeCount);
          ndbrequire(node_index < noOfReplicas);
        }
        inc_node_or_group(node_index, NGPtr.p->nodeCount);
        ndbrequire(node_index < noOfReplicas);
        (*next_replica_node)[NGPtr.i][logPart] = node_index;

        /**
         * Next node group for next fragment
         */
        if (noOfFragments == partitionCount ||
            ((fragNo + 1) % partitionCount == 0))
        {
          /**
           * Change to new node group for
           * 1) Normal tables
           * 2) Tables not stored on all LDMs
           * 3) Fully replicated when at last LDM
           *
           * Thus always except for fully replicated using all LDMs and
           * not yet used all LDMs.
           */
          jam();
          inc_node_or_group(default_node_group, cnoOfNodeGroups);
        }
      }
      if (err)
      {
        jam();
        break;
      }
      if (use_specific_fragment_count)
      {
        jam();
        ndbrequire(default_node_group < MAX_NDB_NODE_GROUPS);
        c_nextNodeGroup = default_node_group;
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
      // Keep track of no of (primary) fragments per node
      Uint16 (*next_replica_node)[MAX_NDB_NODE_GROUPS][NDBMT_MAX_WORKER_INSTANCES] =
        &tmp_next_replica_node;

      memcpy(tmp_next_replica_node,
             c_next_replica_node,
             sizeof(tmp_next_replica_node));
      memset(tmp_next_replica_node_set, 0, sizeof(tmp_next_replica_node_set));
      memset(tmp_fragments_per_node, 0, sizeof(tmp_fragments_per_node));
      memset(tmp_fragments_per_ldm, 0, sizeof(tmp_fragments_per_ldm));
      for (Uint32 fragNo = 0; fragNo < primTabPtr.p->totalfragments; fragNo++) {
        jam();
        FragmentstorePtr fragPtr;
        ReplicaRecordPtr replicaPtr;
        getFragstore(primTabPtr.p, fragNo, fragPtr);
        Uint32 log_part_id = fragPtr.p->m_log_part_id;
        ndbrequire(log_part_id < NDBMT_MAX_WORKER_INSTANCES);
	fragments[count++] = log_part_id;
        fragments[count++] = fragPtr.p->preferredPrimary;

        /* Calculate current primary replica node double array */
        NGPtr.i = getNodeGroup(fragPtr.p->preferredPrimary);
        ptrCheckGuard(NGPtr, MAX_NDB_NODE_GROUPS, nodeGroupRecord);
        for(Uint32 replicaNo = 0; replicaNo < noOfReplicas; replicaNo++)
        {
          jam();
          if (fragPtr.p->preferredPrimary ==
              NGPtr.p->nodesInGroup[replicaNo])
          {
            Uint32 node_index = replicaNo;
            inc_node_or_group(node_index, NGPtr.p->nodeCount);
            ndbrequire(node_index < noOfReplicas);
            (*next_replica_node)[NGPtr.i][log_part_id] = node_index;
            tmp_next_replica_node_set[NGPtr.i][log_part_id] = TRUE;
            break;
          }
        }
        for (replicaPtr.i = fragPtr.p->storedReplicas;
             replicaPtr.i != RNIL;
             replicaPtr.i = replicaPtr.p->nextPool) {
          jam();
          c_replicaRecordPool.getPtr(replicaPtr);
          tmp_fragments_per_ldm[replicaPtr.p->procNode][log_part_id]++;
          tmp_fragments_per_node[replicaPtr.p->procNode]++;
          if (replicaPtr.p->procNode != fragPtr.p->preferredPrimary) {
            jam();
            fragments[count++]= replicaPtr.p->procNode;
          }
        }
        for (replicaPtr.i = fragPtr.p->oldStoredReplicas;
             replicaPtr.i != RNIL;
             replicaPtr.i = replicaPtr.p->nextPool) {
          jam();
          c_replicaRecordPool.getPtr(replicaPtr);
          tmp_fragments_per_ldm[replicaPtr.p->procNode][log_part_id]++;
          tmp_fragments_per_node[replicaPtr.p->procNode]++;
          if (replicaPtr.p->procNode != fragPtr.p->preferredPrimary) {
            jam();
            fragments[count++]= replicaPtr.p->procNode;
            tmp_fragments_per_node[replicaPtr.p->procNode]++;
          }
        }
      }
      if (flags == CreateFragmentationReq::RI_GET_FRAGMENTATION)
      {
        jam();
        noOfFragments = primTabPtr.p->totalfragments;
      }
      else if (flags == CreateFragmentationReq::RI_ADD_FRAGMENTS)
      {
        jam();
        ndbrequire(fragType == DictTabInfo::HashMapPartition ||
                   fragType == DictTabInfo::DistrKeyOrderedIndex);
        /**
         * All nodes that don't belong to a nodegroup to ~0
         * tmp_fragments_per_node so that they don't get any more...
         */
        for (Uint32 i = 0; i<MAX_NDB_NODES; i++)
        {
          if (getNodeStatus(i) == NodeRecord::NOT_IN_CLUSTER ||
              getNodeGroup(i) >= cnoOfNodeGroups)
          {
            jam();
            ndbassert(tmp_fragments_per_node[i] == 0);
            tmp_fragments_per_node[i] = ~(Uint16)0;
          }
        }

        /**
         * Fragments are also added in 3 dimensions.
         * Node group Dimension:
         * ---------------------
         * When we add fragments the algorithm strives to spread the fragments
         * in node group order first. If no new node groups exist to map the
         * table into then one will simply start up again at Node Group 0.
         *
         * So the next fragment always seeks out the most empty node group and
         * adds the fragment there. When new node groups exists and we haven't
         * changed the partition balance then all new fragments will end up
         * in the new node groups. If we change partition balance we will
         * also add new fragments to existing node groups.
         *
         * LDM Dimension:
         * --------------
         * We will ensure that we have an even distribution on the LDMs in the
         * nodes by ensuring that we have knowledge of which LDMs we primarily
         * used in the original table. This is necessary to support ALTER TABLE
         * from PARTITION_BALANCE_FOR_RP_BY_NODE to
         * PARTITION_BALANCE_FOR_RA_BY_NODE e.g. PARTITION_BALANCE_FOR_RP_BY_NODE
         * could have used any LDMs. So it is important to ensure that we
         * spread evenly over all LDMs also after the ALTER TABLE. We do this
         * by always finding the LDM in the node with the minimum number of
         * fragments.
         *
         * At the moment we don't support on-line add partition of for fully
         * replicated tables. We do however support adding more node groups.
         * In order to support adding partitions for fully replicated tables
         * it is necessary to provide a mapping from calculated main fragment
         * since they will then no longer be fragment id 0 to number of
         * main fragments minus one.
         *
         * Primary replica Dimension:
         * --------------------------
         * We make an effort to spread the primary replicas around amongst the
         * nodes in each node group and LDM. We need to spread both regarding
         * nodes and with regard to LDM. When we use partition balance
         * FOR_RP_BY_LDM we will spread on all LDMs in all nodes for
         * the table itself, so we don't need to use the DIH copy of the
         * next primary replica to use. For all other tables we will start by
         * reading what is already in the table, if the table itself has
         * already used an LDM in the node group to assign a primary replica,
         * then we will simply continue using the local copy. For new
         * partitions in a previously unused LDM in a node group we will
         * rather use the next based on what other tables have used in
         * creating and on-line altering tables.
         */

        Uint32 first_new_node = find_min_index(tmp_fragments_per_node, 
                                               NDB_ARRAY_SIZE(tmp_fragments_per_node),
                                               0);
        Uint32 firstNG = getNodeGroup(first_new_node);
        Uint32 next_log_part = 0;
        bool use_old_variant = true;

        bool const fully_replicated = (noOfFragments != partitionCount);

        switch(partitionBalance)
        {
          case NDB_PARTITION_BALANCE_SPECIFIC:
          case NDB_PARTITION_BALANCE_FOR_RP_BY_NODE:
          case NDB_PARTITION_BALANCE_FOR_RA_BY_NODE:
          {
            jam();
            break;
          }
          case NDB_PARTITION_BALANCE_FOR_RP_BY_LDM:
          case NDB_PARTITION_BALANCE_FOR_RA_BY_LDM:
          {
            jam();
            use_old_variant = false;
            next_log_part = (~0);
            break;
          }
          default:
          {
            ndbrequire(false);
            break;
          }
        }
        Uint32 node;
        NGPtr.i = RNIL;
        for (Uint32 i = primTabPtr.p->totalfragments; i<noOfFragments; i++)
        {
          jam();
          if (!fully_replicated || (i % partitionCount == 0))
          {
            node = find_min_index(tmp_fragments_per_node,
                                  NDB_ARRAY_SIZE(tmp_fragments_per_node),
                                  0);
            NGPtr.i = getNodeGroup(node);
          }
          ptrCheckGuard(NGPtr, MAX_NDB_NODE_GROUPS, nodeGroupRecord);
          Uint32 logPart;
          if (use_old_variant)
          {
            jam();
            logPart = (NGPtr.p->m_next_log_part++) % globalData.ndbLogParts;
          }
          else
          {
            jam();
            if (firstNG == NGPtr.i)
            {
              jam();
              next_log_part++;
            }
            logPart = next_log_part % globalData.ndbLogParts;
          }
          logPart = find_min_index(&tmp_fragments_per_ldm[node][0],
                                   globalData.ndbLogParts,
                                   logPart);
          ndbrequire(logPart < NDBMT_MAX_WORKER_INSTANCES);

          /* Select primary replica node */
          Uint32 primary_node;
          if (tmp_next_replica_node_set[NGPtr.i][logPart] ||
              partitionBalance == NDB_PARTITION_BALANCE_FOR_RP_BY_LDM)
          {
            jam();
            Uint32 node_index = (*next_replica_node)[NGPtr.i][logPart];
            primary_node = NGPtr.p->nodesInGroup[node_index];
            inc_node_or_group(node_index, NGPtr.p->nodeCount);
            ndbrequire(node_index < noOfReplicas);
            (*next_replica_node)[NGPtr.i][logPart] = node_index;
          }
          else
          {
            jam();
            Uint32 node_index = c_next_replica_node[NGPtr.i][logPart];
            primary_node = NGPtr.p->nodesInGroup[node_index];
            inc_node_or_group(node_index, NGPtr.p->nodeCount);
            c_next_replica_node[NGPtr.i][logPart] = node_index;
          }
          ndbrequire(primary_node < MAX_NDB_NODES);
          fragments[count++] = logPart;
          fragments[count++] = primary_node;
          tmp_fragments_per_ldm[primary_node][logPart]++;
          /* Ensure that we don't report this as min immediately again */
          tmp_fragments_per_node[primary_node]++;
          for (Uint32 r = 0; r < noOfReplicas; r++)
          {
            jam();
            if (NGPtr.p->nodesInGroup[r] != primary_node)
            {
              jam();
              Uint32 replicaNode = NGPtr.p->nodesInGroup[r];
              fragments[count++] = replicaNode;
              tmp_fragments_per_node[replicaNode]++;
              tmp_fragments_per_ldm[replicaNode][logPart]++;
            }
          }
        }
      }
    }
    if(count != (2U + (1 + noOfReplicas) * noOfFragments)){
        char buf[255];
        BaseString::snprintf(buf, sizeof(buf),
                           "Illegal configuration change: NoOfReplicas."
                           " Can't be applied online ");
        progError(__LINE__, NDBD_EXIT_INVALID_CONFIG, buf);
    }
    
    CreateFragmentationConf * const conf = 
      (CreateFragmentationConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = senderData;
    conf->noOfReplicas = (Uint32)noOfReplicas;
    conf->noOfFragments = (Uint32)noOfFragments;

    fragments[0]= noOfReplicas;
    fragments[1]= noOfFragments;

    if (flags == CreateFragmentationReq::RI_ADD_FRAGMENTS ||
        flags == CreateFragmentationReq::RI_CREATE_FRAGMENTATION)
    {
      if (!verify_fragmentation(fragments, partitionCount, partitionBalance, getFragmentsPerNode()))
      {
        err = CreateFragmentationRef::InvalidFragmentationType;
        break;
      }
    }

    if(senderRef != 0)
    {
      /**
       * Only possible serving old client with lower version than 7.0.4
       * (WL#3600)
       */
      jam();
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
    // Always ACK/NACK (here ACK)
    signal->theData[0] = 0;
    return;
  } while(false);
  // Always ACK/NACK (here NACK)
  signal->theData[0] = err;
}

bool Dbdih::verify_fragmentation(Uint16* fragments,
                                 Uint32 partition_count,
                                 Uint32 partition_balance,
                                 Uint32 ldm_count) const
{
  jam();
  bool fatal = false;
  bool suboptimal = false;

  Uint32 const replica_count = fragments[0];
  Uint32 const fragment_count = fragments[1];

  Uint16 fragments_per_node[MAX_NDB_NODES];
  Uint16 primary_replica_per_node[MAX_NDB_NODES];
  Uint16 fragments_per_ldm[MAX_NDB_NODES][NDBMT_MAX_WORKER_INSTANCES];
  Uint16 primary_replica_per_ldm[MAX_NDB_NODES][NDBMT_MAX_WORKER_INSTANCES];

  bzero(fragments_per_node, sizeof(fragments_per_node));
  bzero(fragments_per_ldm, sizeof(fragments_per_ldm));
  bzero(primary_replica_per_node, sizeof(primary_replica_per_node));
  bzero(primary_replica_per_ldm, sizeof(primary_replica_per_ldm));

  /**
   * For fully replicated tables one partition can have several copy fragments.
   * The following conditions must be satisfied:
   * 1) No node have two copy fragments for same partition.
   * 2) The partition id that a fragment belongs to is calculated as module
   *    partition count.
   * 3) The main copy fragment of a partition have the same id as the partition.
   * 4) Fragments with consequtive id belonging to partition 0 upto partition
   *    count - 1, are in this function called a partition set and should have
   *    its replicas in one nodegroup.
   * 1) must always be satisfied also in future implementations. 2) and 3) may
   * be relaxed in future. 4) is not necessary, but as long as 2) and 3) must
   * be satisfied ensuring 4) is an easy condition to remember.
   */

  /**
   * partition_nodes indicates for each partition what nodes have a copy
   * fragment.  This is used to detect if two fragments for same partition is
   * located on same node, ie breakage of condition 1) above.
   * This also depends on condition 2) above.
   */
  NdbNodeBitmask partition_nodes[MAX_NDB_PARTITIONS];

  /**
   * partition_set_for_node keep track what partition_set (as in condition 4)
   * above) are located on a node.  Only one partition set per node is allowed.
   * This toghether with the fact that all nodes in same nodegroup share
   * fragments ensures condition 4) above.
   * ~0 are used as a still unset partition set indicator.
   */
  Uint32 partition_set_for_node[MAX_NDB_NODES];
  for (Uint32 node = 0; node < MAX_NDB_NODES; node++)
  {
    partition_set_for_node[node] = ~Uint32(0);
  }

  for(Uint32 fragment_id = 0; fragment_id < fragment_count; fragment_id++)
  {
    jam();
    Uint32 const partition_id = fragment_id % partition_count;
    Uint32 const partition_set = fragment_id / partition_count;
    Uint32 const log_part_id = fragments[2 + fragment_id * (1 + replica_count)];
    Uint32 const ldm = (log_part_id % ldm_count);
    for(Uint32 replica_id = 0; replica_id < replica_count; replica_id++)
    {
      jam();
      Uint32 const node =
          fragments[2 + fragment_id * (1 + replica_count) + 1 + replica_id];
      fragments_per_node[node]++;
      fragments_per_ldm[node][ldm]++;
      if (replica_id == 0)
      {
        jam();
        primary_replica_per_node[node]++;
        primary_replica_per_ldm[node][ldm]++;
      }

      if (partition_set_for_node[node] == ~Uint32(0))
      {
        jam();
        partition_set_for_node[node] = partition_set;
      }
      if (partition_set_for_node[node] != partition_set)
      {
        jam();
        fatal = true;
        ndbassert(!"Copy fragments from different partition set on same node");
      }

      if (partition_nodes[partition_id].get(node))
      {
        jam();
        fatal = true;
        ndbassert(!"Two copy fragments for same partition on same node");
      }
      partition_nodes[partition_id].set(node);
    }
  }

  /**
   * Below counters for number of fragments (for ra) or primary replicas (for
   * rp) there are per ldm or node.
   *
   * ~0 is used to indicate unset value. 0 is used if there are conflicting
   * counts, in other word there is an unbalance.
   */

  Uint32 balance_for_ra_by_ldm_count = ~Uint32(0);
  Uint32 balance_for_ra_by_node_count = ~Uint32(0);
  Uint32 balance_for_rp_by_ldm_count = ~Uint32(0);
  Uint32 balance_for_rp_by_node_count = ~Uint32(0);
  for (Uint32 node = 1; node < MAX_NDB_NODES; node++)
  {
    jam();
    if (balance_for_ra_by_node_count != 0 &&
        fragments_per_node[node] != 0 &&
        fragments_per_node[node] != balance_for_ra_by_node_count)
    {
      if (balance_for_ra_by_node_count == ~Uint32(0))
        balance_for_ra_by_node_count = fragments_per_node[node];
      else
        balance_for_ra_by_node_count = 0;
    }
    if (balance_for_rp_by_node_count != 0 &&
        primary_replica_per_node[node] != 0 &&
        primary_replica_per_node[node] != balance_for_rp_by_node_count)
    {
      if (balance_for_rp_by_node_count == ~Uint32(0))
        balance_for_rp_by_node_count = primary_replica_per_node[node];
      else
        balance_for_rp_by_node_count = 0;
    }
    for (Uint32 ldm = 0; ldm < NDBMT_MAX_WORKER_INSTANCES; ldm ++)
    {
      if (balance_for_ra_by_ldm_count != 0 &&
          fragments_per_ldm[node][ldm] != 0 &&
          fragments_per_ldm[node][ldm] != balance_for_ra_by_ldm_count)
      {
        if (balance_for_ra_by_ldm_count == ~Uint32(0))
          balance_for_ra_by_ldm_count = fragments_per_ldm[node][ldm];
        else
          balance_for_ra_by_ldm_count = 0;
      }
      if (balance_for_rp_by_ldm_count != 0 &&
          primary_replica_per_ldm[node][ldm] != 0 &&
          primary_replica_per_ldm[node][ldm] != balance_for_rp_by_ldm_count)
      {
        if (balance_for_rp_by_ldm_count == ~Uint32(0))
          balance_for_rp_by_ldm_count = primary_replica_per_ldm[node][ldm];
        else
          balance_for_rp_by_ldm_count = 0;
      }
    }
  }
  switch (partition_balance)
  {
  case NDB_PARTITION_BALANCE_FOR_RA_BY_NODE:
    jam();
    suboptimal = (balance_for_ra_by_node_count == 0);
    break;
  case NDB_PARTITION_BALANCE_FOR_RA_BY_LDM:
    jam();
    suboptimal = (balance_for_ra_by_ldm_count == 0);
    break;
  case NDB_PARTITION_BALANCE_FOR_RP_BY_NODE:
    jam();
    suboptimal = (balance_for_rp_by_node_count == 0);
    break;
  case NDB_PARTITION_BALANCE_FOR_RP_BY_LDM:
    jam();
    suboptimal = (balance_for_rp_by_ldm_count == 0);
    break;
  default:
    jam();
  }
  ndbassert(!fatal);
  // Allow suboptimal until we have a way to choose to allow it or not
  return !fatal;
}

void Dbdih::insertCopyFragmentList(TabRecord *tabPtr,
                                   Fragmentstore *fragPtr,
                                   Uint32 my_fragid)
{
  Uint32 found_fragid = RNIL;
  FragmentstorePtr locFragPtr;
  Uint32 partition_id = fragPtr->partition_id;
  for (Uint32 i = 0; i < tabPtr->totalfragments; i++)
  {
    getFragstore(tabPtr, i, locFragPtr);
    if (locFragPtr.p->partition_id == partition_id)
    {
      if (fragPtr == locFragPtr.p)
      {
        /* We're inserting the main fragment */
        fragPtr->nextCopyFragment = RNIL;
        D("Inserting fragId " << my_fragid << " as main fragment");
        return;
      }
      jam();
      found_fragid = i;
      break;
    }
  }
  ndbrequire(found_fragid != RNIL);
  /**
   * We have now found the main copy fragment for this partition.
   * We will add the fragment last in this list. So we search for
   * end of list and add it to the list when we reach the end of
   * the list.
   */
  ndbrequire(locFragPtr.p != fragPtr);
  while (locFragPtr.p->nextCopyFragment != RNIL)
  {
    found_fragid = locFragPtr.p->nextCopyFragment;
    getFragstore(tabPtr, found_fragid, locFragPtr);
  }
  /**
   * We update in a safe manner here ensuring that the list is
   * always seen as a proper list by inserting a memory barrier
   * before setting the new nextCopyFragment. It isn't absolutely
   * necessary but is future proof given that we use a RCU
   * mechanism around this data.
   */
  fragPtr->nextCopyFragment = RNIL;
  mb();
  locFragPtr.p->nextCopyFragment = my_fragid;
  D("Insert fragId " << my_fragid << " after fragId " << found_fragid);
}

void Dbdih::execDIADDTABREQ(Signal* signal) 
{
  Uint32 fragType;
  jamEntry();

  DiAddTabReq * const req = (DiAddTabReq*)signal->getDataPtr();

  // Seize connect record
  ndbrequire(cfirstconnect != RNIL);
  ConnectRecordPtr connectPtr;
  connectPtr.i = cfirstconnect;
  ptrCheckGuard(connectPtr, cconnectFileSize, connectRecord);
  cfirstconnect = connectPtr.p->nextPool;
  
  const Uint32 userPtr = req->connectPtr;
  const BlockReference userRef = signal->getSendersBlockRef();
  connectPtr.p->nextPool = RNIL;
  connectPtr.p->userpointer = userPtr;
  connectPtr.p->userblockref = userRef;
  connectPtr.p->connectState = ConnectRecord::INUSE;
  connectPtr.p->table = req->tableId;
  connectPtr.p->m_alter.m_changeMask = 0;
  connectPtr.p->m_create.m_map_ptr_i = req->hashMapPtrI;

  TabRecordPtr tabPtr;
  tabPtr.i = req->tableId;
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);

  D("DIADDTABREQ: tableId = " << tabPtr.i);
  fragType= req->fragType;
  if (prepare_add_table(tabPtr, connectPtr, signal))
  {
    jam();
    return;
  }

  /**
   * When we get here the table is under definition and DBTC can still not
   * use the table. So there is no possibility for conflict with DBTC.
   * Thus no need for mutexes and RCU lock calls.
   */

  /* Only the master should read a table definition from disk during SR */
  if (getNodeState().getSystemRestartInProgress() &&
      tabPtr.p->tabStatus == TabRecord::TS_IDLE &&
      cmasterNodeId == getOwnNodeId())
  {
    jam();
    
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
  if(req->loggedTable)
    tabPtr.p->tabStorage= TabRecord::ST_NORMAL;
  else if(req->temporaryTable)
    tabPtr.p->tabStorage= TabRecord::ST_TEMPORARY;
  else
    tabPtr.p->tabStorage= TabRecord::ST_NOLOGGING;
  tabPtr.p->kvalue = req->kValue;

  switch ((DictTabInfo::FragmentType)fragType){
  case DictTabInfo::HashMapPartition:
    tabPtr.p->method = TabRecord::HASH_MAP;
    break;
  case DictTabInfo::AllNodesSmallTable:
  case DictTabInfo::AllNodesMediumTable:
  case DictTabInfo::AllNodesLargeTable:
  case DictTabInfo::SingleFragment:
    jam();
  case DictTabInfo::DistrKeyLin:
    jam();
    tabPtr.p->method = TabRecord::LINEAR_HASH;
    break;
  case DictTabInfo::DistrKeyHash:
    jam();
    tabPtr.p->method = TabRecord::NORMAL_HASH;
    break;
  case DictTabInfo::DistrKeyOrderedIndex:
  {
    TabRecordPtr primTabPtr;
    primTabPtr.i = req->primaryTableId;
    ptrCheckGuard(primTabPtr, ctabFileSize, tabRecord);
    tabPtr.p->method = primTabPtr.p->method;
    req->hashMapPtrI = primTabPtr.p->m_map_ptr_i;
    break;
  }
  case DictTabInfo::UserDefined:
    jam();
    tabPtr.p->method = TabRecord::USER_DEFINED;
    break;
  default:
    ndbrequire(false);
  }

  union {
    Uint16 fragments[MAX_FRAGMENT_DATA_ENTRIES];
    Uint32 align;
  };
  (void)align; // kill warning
  SectionHandle handle(this, signal);
  SegmentedSectionPtr fragDataPtr;
  ndbrequire(handle.getSection(fragDataPtr, DiAddTabReq::FRAGMENTATION));
  copy((Uint32*)fragments, fragDataPtr);
  releaseSections(handle);
  
  const Uint32 noReplicas = fragments[0];
  const Uint32 noFragments = fragments[1];

  if ((tabPtr.p->m_flags & TabRecord::TF_FULLY_REPLICATED) == 0)
  {
    jam();
    D("partitionCount for normal table set to = " << noFragments);
    tabPtr.p->partitionCount = noFragments;
  }
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
    addtabrefuseLab(signal, connectPtr, ZREPLERROR2);
    return;
  }//if
  
  Uint32 logTotalFragments = 1;
  ndbrequire(tabPtr.p->partitionCount < (1 << 16));
  while (logTotalFragments <= tabPtr.p->partitionCount) {
    jam();
    logTotalFragments <<= 1;
  }
  logTotalFragments >>= 1;
  tabPtr.p->mask = logTotalFragments - 1;
  tabPtr.p->hashpointer = tabPtr.p->partitionCount - logTotalFragments;
  allocFragments(tabPtr.p->totalfragments, tabPtr);  

  if (tabPtr.p->method == TabRecord::HASH_MAP)
  {
    jam();
    tabPtr.p->m_map_ptr_i = req->hashMapPtrI;
    tabPtr.p->m_new_map_ptr_i = RNIL;
    Ptr<Hash2FragmentMap> mapPtr;
    g_hash_map.getPtr(mapPtr, tabPtr.p->m_map_ptr_i);
    ndbrequire(tabPtr.p->totalfragments >= mapPtr.p->m_fragments);
  }

  Uint32 index = 2;
  for (Uint32 fragId = 0; fragId < noFragments; fragId++) {
    jam();
    FragmentstorePtr fragPtr;
    Uint32 activeIndex = 0;
    getFragstore(tabPtr.p, fragId, fragPtr);
    fragPtr.p->m_log_part_id = fragments[index++];
    fragPtr.p->preferredPrimary = fragments[index];
    fragPtr.p->partition_id = fragId % tabPtr.p->partitionCount;

    ndbrequire(fragPtr.p->m_log_part_id < NDBMT_MAX_WORKER_INSTANCES);

    inc_ng_refcount(getNodeGroup(fragPtr.p->preferredPrimary));
    
    for (Uint32 i = 0; i<noReplicas; i++) {
      const Uint32 nodeId = fragments[index++];
      ReplicaRecordPtr replicaPtr;
      allocStoredReplica(fragPtr,
                         replicaPtr,
                         nodeId,
                         fragId,
                         tabPtr.i);
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
    if ((tabPtr.p->m_flags & TabRecord::TF_FULLY_REPLICATED) != 0)
    {
      jam();
      insertCopyFragmentList(tabPtr.p, fragPtr.p, fragId);
    }
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
  connectPtr.p->m_alter.m_totalfragments = tabPtr.p->totalfragments;

  sendAddFragreq(signal, connectPtr, tabPtr, 0, false);
}

void
Dbdih::sendAddFragreq(Signal* signal,
                      ConnectRecordPtr connectPtr, 
                      TabRecordPtr tabPtr,
                      Uint32 fragId,
                      bool rcu_lock_held)
{
  jam();
  const Uint32 fragCount = connectPtr.p->m_alter.m_totalfragments;
  ReplicaRecordPtr replicaPtr;
  replicaPtr.i = RNIL;
  FragmentstorePtr fragPtr;
  for(; fragId<fragCount; fragId++){
    jam();
    getFragstore(tabPtr.p, fragId, fragPtr);    
    
    replicaPtr.i = fragPtr.p->storedReplicas;
    while(replicaPtr.i != RNIL){
      jam();
      c_replicaRecordPool.getPtr(replicaPtr);
      if(replicaPtr.p->procNode == getOwnNodeId()){
	break;
      }
      replicaPtr.i = replicaPtr.p->nextPool;
    }
    
    if(replicaPtr.i != RNIL){
      jam();
      break;
    }
    
    replicaPtr.i = fragPtr.p->oldStoredReplicas;
    while(replicaPtr.i != RNIL){
      jam();
      c_replicaRecordPool.getPtr(replicaPtr);
      if(replicaPtr.p->procNode == getOwnNodeId()){
	break;
      }
      replicaPtr.i = replicaPtr.p->nextPool;
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
    if(tabPtr.p->tabStorage != TabRecord::ST_NORMAL){
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
    req->logPartId = fragPtr.p->m_log_part_id;

    if (connectPtr.p->connectState != ConnectRecord::ALTER_TABLE)
    {
      jam();
      req->changeMask = 0;
      req->partitionId = fragId % tabPtr.p->partitionCount;
    }
    else /* connectState == ALTER_TABLE */
    {
      jam();
      req->changeMask = connectPtr.p->m_alter.m_changeMask;
      req->partitionId = fragId % connectPtr.p->m_alter.m_partitionCount;
    }

    sendSignal(DBDICT_REF, GSN_ADD_FRAGREQ, signal, 
	       AddFragReq::SignalLength, JBB);
    return;
  }
  
  if (connectPtr.p->connectState == ConnectRecord::ALTER_TABLE)
  {
    jam();
    // Request handled successfully

    if (AlterTableReq::getReorgFragFlag(connectPtr.p->m_alter.m_changeMask))
    {
      jam();
      make_new_table_writeable(tabPtr, connectPtr, rcu_lock_held);
    }

    if (AlterTableReq::getAddFragFlag(connectPtr.p->m_alter.m_changeMask))
    {
      jam();
      Callback cb;
      cb.m_callbackData = connectPtr.i;
      cb.m_callbackFunction = safe_cast(&Dbdih::alter_table_writeTable_conf);
      saveTableFile(signal, connectPtr, tabPtr, TabRecord::CS_ALTER_TABLE, cb);
      return;
    }

    send_alter_tab_conf(signal, connectPtr);
  }
  else
  {
    // Done

    /**
     * This code is only executed as part of CREATE TABLE, so at this point
     * in time DBTC hasn't been made aware of the table's usability yet, so
     * we rely on signal ordering to protect the data from DBTC here.
     * Naturally it could be executed as part of a CREATE INDEX as well, but
     * the principle is still the same.
     */

    /**
      * Don't expect to be adding tables due to e.g. user action
      * during NR or SR, so we init the CopyFragmentList here
      */
    if (( getNodeState().getSystemRestartInProgress() ||
          getNodeState().getNodeRestartInProgress() ) &&
        (tabPtr.p->m_flags & TabRecord::TF_FULLY_REPLICATED) != 0)
    {
      jam();
      for(Uint32 fragId = 0; fragId < tabPtr.p->totalfragments; fragId++)
      {
        jam();
        FragmentstorePtr fragPtr;
        getFragstore(tabPtr.p, fragId, fragPtr);
        fragPtr.p->partition_id = fragId % tabPtr.p->partitionCount;
        insertCopyFragmentList(tabPtr.p, fragPtr.p, fragId);
      }
    }

    DiAddTabConf * const conf = (DiAddTabConf*)signal->getDataPtr();
    conf->senderData = connectPtr.p->userpointer;
    sendSignal(connectPtr.p->userblockref, GSN_DIADDTABCONF, signal,
               DiAddTabConf::SignalLength, JBB);


    if (tabPtr.p->method == TabRecord::HASH_MAP)
    {
      Uint32 newValue = RNIL;
      if (DictTabInfo::isOrderedIndex(tabPtr.p->tableType))
      {
        jam();
        TabRecordPtr primTabPtr;
        primTabPtr.i = tabPtr.p->primaryTableId;
        ptrCheckGuard(primTabPtr, ctabFileSize, tabRecord);
        newValue = primTabPtr.p->m_map_ptr_i;
      }
      else
      {
        jam();
        newValue = connectPtr.p->m_create.m_map_ptr_i;
      }

      tabPtr.p->m_map_ptr_i = newValue;
    }
    // Release
    ndbrequire(tabPtr.p->connectrec == connectPtr.i);
    tabPtr.p->connectrec = RNIL;
    release_connect(connectPtr);
  }

}
void
Dbdih::release_connect(ConnectRecordPtr ptr)
{
  TabRecordPtr tabPtr;
  tabPtr.i = ptr.p->table;
  if (tabPtr.i != RNIL)
  {
    jam();
    ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
    if (tabPtr.p->connectrec == ptr.i)
    {
      ndbassert(false); // should be fixed elsewhere
      tabPtr.p->connectrec = RNIL;
    }
  }

  ptr.p->table = RNIL;
  ptr.p->userblockref = ZNIL;
  ptr.p->userpointer = RNIL;
  ptr.p->connectState = ConnectRecord::FREE;
  ptr.p->nextPool = cfirstconnect;
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

  sendAddFragreq(signal, connectPtr, tabPtr, conf->fragId + 1, false);
}

void
Dbdih::execADD_FRAGREF(Signal* signal){
  jamEntry();
  AddFragRef * const ref = (AddFragRef*)signal->getDataPtr();

  ConnectRecordPtr connectPtr;
  connectPtr.i = ref->dihPtr;
  ptrCheckGuard(connectPtr, cconnectFileSize, connectRecord);

  Ptr<TabRecord> tabPtr;
  tabPtr.i = connectPtr.p->table;
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
  ndbrequire(tabPtr.p->connectrec == connectPtr.i);

  if (connectPtr.p->connectState == ConnectRecord::ALTER_TABLE)
  {
    jam();

    if (AlterTableReq::getReorgFragFlag(connectPtr.p->m_alter.m_changeMask))
    {
      jam();
      make_new_table_non_writeable(tabPtr);
    }

    connectPtr.p->connectState = ConnectRecord::ALTER_TABLE_ABORT;
    drop_fragments(signal, connectPtr, connectPtr.p->m_alter.m_totalfragments);
    return;
  }
  else
  {
    DiAddTabRef * const ref = (DiAddTabRef*)signal->getDataPtr();
    ref->senderData = connectPtr.p->userpointer;
    ref->errorCode = ~0;
    sendSignal(connectPtr.p->userblockref, GSN_DIADDTABREF, signal, 
	       DiAddTabRef::SignalLength, JBB);  

    // Release
    tabPtr.p->connectrec = RNIL;
    release_connect(connectPtr);
  }
}

/*
  3.7.1.3   R E F U S E
  *********************
  */
void
Dbdih::addtabrefuseLab(Signal* signal,
                       ConnectRecordPtr connectPtr, Uint32 errorCode)
{
  signal->theData[0] = connectPtr.p->userpointer;
  signal->theData[1] = errorCode;
  sendSignal(connectPtr.p->userblockref, GSN_DIADDTABREF, signal, 2, JBB);

  Ptr<TabRecord> tabPtr;
  tabPtr.i = connectPtr.p->table;
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
  ndbrequire(tabPtr.p->connectrec == connectPtr.i);
  tabPtr.p->connectrec = RNIL;

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
Dbdih::execDROP_TAB_REQ(Signal* signal)
{
  jamEntry();
  DropTabReq* req = (DropTabReq*)signal->getDataPtr();

  D("DROP_TAB_REQ: " << req->tableId);
  CRASH_INSERTION(7248);

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
    break;
  case DropTabReq::CreateTabDrop:
    jam();
    break;
  case DropTabReq::RestartDropTab:
    break;
  }
  
  if (isMaster())
  {
    /**
     * Remove from queue
     */
    NodeRecordPtr nodePtr;
    for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++)
    {
      jam();
      ptrAss(nodePtr, nodeRecord);
      if (c_lcpState.m_participatingLQH.get(nodePtr.i))
      {
        Uint32 index = 0;
	Uint32 count = nodePtr.p->noOfQueuedChkpt;
	while (index < count)
        {
	  if (nodePtr.p->queuedChkpt[index].tableId == tabPtr.i)
          {
	    jam();
	    count--;
	    for (Uint32 i = index; i<count; i++)
            {
	      jam();
	      nodePtr.p->queuedChkpt[i] = nodePtr.p->queuedChkpt[i + 1];
	    }
	  }
          else
          {
	    index++;
	  }
	}
	nodePtr.p->noOfQueuedChkpt = count;
      }
    }
  }

  {
    /**
     * Check table lcp state
     */
    bool ok = false;
    switch(tabPtr.p->tabLcpStatus){
    case TabRecord::TLS_COMPLETED:
    case TabRecord::TLS_WRITING_TO_FILE:
      ok = true;
      jam();
      g_eventLogger->info("DROP_TAB_REQ: tab: %u, tabLcpStatus: %u",
                          tabPtr.i,
                          tabPtr.p->tabLcpStatus);
      break;
      return;
    case TabRecord::TLS_ACTIVE:
      ok = true;
      jam();

      tabPtr.p->tabLcpStatus = TabRecord::TLS_COMPLETED;

      g_eventLogger->info("DROP_TAB_REQ: tab: %u, tabLcpStatus set to %u",
                          tabPtr.i,
                          tabPtr.p->tabLcpStatus);
      /**
       * First check if all fragments are done
       */
      if (checkLcpAllTablesDoneInLqh(__LINE__))
      {
	jam();

        g_eventLogger->info("This is the last table");

	/**
	 * Then check if saving of tab info is done for all tables
	 */
	LcpStatus a = c_lcpState.lcpStatus;
	checkLcpCompletedLab(signal);

        if(a != c_lcpState.lcpStatus)
        {
          g_eventLogger->info("And all tables are written to already written disk");
        }
      }
      break;
    }
    ndbrequire(ok);
  }

  waitDropTabWritingToFile(signal, tabPtr);
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

  /**
   * Table has already been dropped from DBTC's view a long time
   * ago, we need not protect this change.
   */
  tabPtr.p->tabStatus = TabRecord::TS_IDLE;
  
  DropTabConf * const dropConf = (DropTabConf *)signal->getDataPtrSend();
  dropConf->senderRef = reference();
  dropConf->senderData = tabPtr.p->m_dropTab.tabUserPtr;
  dropConf->tableId = tabPtr.i;
  sendSignal(tabPtr.p->m_dropTab.tabUserRef, GSN_DROP_TAB_CONF, 
	     signal, DropTabConf::SignalLength, JBB);
  
  tabPtr.p->m_dropTab.tabUserPtr = RNIL;
  tabPtr.p->m_dropTab.tabUserRef = 0;
  releaseTable(tabPtr);
}//Dbdih::tableDeleteLab()


void Dbdih::releaseTable(TabRecordPtr tabPtr)
{
  FragmentstorePtr fragPtr;
  if (tabPtr.p->noOfFragChunks > 0) {
    for (Uint32 fragId = 0; fragId < tabPtr.p->totalfragments; fragId++) {
      jam();
      getFragstore(tabPtr.p, fragId, fragPtr);
      dec_ng_refcount(getNodeGroup(fragPtr.p->preferredPrimary));
      releaseReplicas(& fragPtr.p->storedReplicas);
      releaseReplicas(& fragPtr.p->oldStoredReplicas);
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

void Dbdih::releaseReplicas(Uint32 * replicaPtrI) 
{
  ReplicaRecordPtr replicaPtr;
  replicaPtr.i = * replicaPtrI;
  jam();
  while (replicaPtr.i != RNIL)
  {
    jam();
    c_replicaRecordPool.getPtr(replicaPtr);
    Uint32 tmp = replicaPtr.p->nextPool;
    c_replicaRecordPool.release(replicaPtr);
    replicaPtr.i = tmp;
    cnoFreeReplicaRec++;
  }//while

  * replicaPtrI = RNIL;
}//Dbdih::releaseReplicas()

void Dbdih::seizeReplicaRec(ReplicaRecordPtr& replicaPtr) 
{
  c_replicaRecordPool.seize(replicaPtr);
  cnoFreeReplicaRec--;
  replicaPtr.p->nextPool = RNIL;
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
  const AlterTabReq* req = (const AlterTabReq*)signal->getDataPtr();
  const Uint32 senderRef = req->senderRef;
  const Uint32 senderData = req->senderData;
  const Uint32 tableId = req->tableId;
  const Uint32 tableVersion = req->tableVersion;
  const Uint32 newTableVersion = req->newTableVersion;
  AlterTabReq::RequestType requestType = 
    (AlterTabReq::RequestType) req->requestType;
  D("ALTER_TAB_REQ(DIH)");

  TabRecordPtr tabPtr;
  tabPtr.i = tableId;
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);

  switch(requestType){
  case AlterTabReq::AlterTablePrepare:
    jam();
    // fall through
  case AlterTabReq::AlterTableRevert:
    jam();
    if (AlterTableReq::getAddFragFlag(req->changeMask) &&
        tabPtr.p->tabCopyStatus != TabRecord::CS_IDLE)
    {
      jam();
      SectionHandle handle(this, signal);
      sendSignalWithDelay(reference(), GSN_ALTER_TAB_REQ, signal, 10,
                          signal->getLength(), &handle);
      return;
    }
  case AlterTabReq::AlterTableCommit:
    jam();
  case AlterTabReq::AlterTableComplete:
    jam();
  case AlterTabReq::AlterTableWaitScan:
    jam();
    break;
  default:
    jamLine(requestType);
  }

  ConnectRecordPtr connectPtr;
  connectPtr.i = RNIL;
  switch (requestType) {
  case AlterTabReq::AlterTablePrepare:
    jam();

    D("AlterTabReq::AlterTablePrepare: tableId: " << tabPtr.i);
    ndbrequire(cfirstconnect != RNIL);
    connectPtr.i = cfirstconnect;
    ptrCheckGuard(connectPtr, cconnectFileSize, connectRecord);
    cfirstconnect = connectPtr.p->nextPool;

    connectPtr.p->m_alter.m_totalfragments = tabPtr.p->totalfragments;
    connectPtr.p->m_alter.m_org_totalfragments = tabPtr.p->totalfragments;
    connectPtr.p->m_alter.m_partitionCount = tabPtr.p->partitionCount;
    connectPtr.p->m_alter.m_changeMask = req->changeMask;
    connectPtr.p->m_alter.m_new_map_ptr_i = req->new_map_ptr_i;
    connectPtr.p->userpointer = senderData;
    connectPtr.p->userblockref = senderRef;
    connectPtr.p->connectState = ConnectRecord::ALTER_TABLE;
    connectPtr.p->table = tabPtr.i;
    tabPtr.p->connectrec = connectPtr.i;
    break;
  case AlterTabReq::AlterTableRevert:
    jam();
    D("AlterTabReq::AlterTableRevert: tableId: " << tabPtr.i);
    tabPtr.p->schemaVersion = tableVersion;

    connectPtr.i = req->connectPtr;
    ptrCheckGuard(connectPtr, cconnectFileSize, connectRecord);

    ndbrequire(connectPtr.p->connectState == ConnectRecord::ALTER_TABLE);

    connectPtr.p->userpointer = senderData;
    connectPtr.p->userblockref = senderRef;

    if (AlterTableReq::getReorgFragFlag(connectPtr.p->m_alter.m_changeMask))
    {
      jam();
      make_new_table_non_writeable(tabPtr);
    }

    if (AlterTableReq::getAddFragFlag(req->changeMask))
    {
      jam();
      tabPtr.p->tabCopyStatus = TabRecord::CS_ALTER_TABLE;
      connectPtr.p->connectState = ConnectRecord::ALTER_TABLE_REVERT;
      drop_fragments(signal, connectPtr,
                     connectPtr.p->m_alter.m_totalfragments);
      return;
    }

    send_alter_tab_conf(signal, connectPtr);

    ndbrequire(tabPtr.p->connectrec == connectPtr.i);
    tabPtr.p->connectrec = RNIL;
    release_connect(connectPtr);
    return;
    break;
  case AlterTabReq::AlterTableCommit:
  {
    jam();
    D("AlterTabReq::AlterTableCommit: tableId: " << tabPtr.i);
    tabPtr.p->schemaVersion = newTableVersion;

    connectPtr.i = req->connectPtr;
    ptrCheckGuard(connectPtr, cconnectFileSize, connectRecord);
    connectPtr.p->userpointer = senderData;
    connectPtr.p->userblockref = senderRef;
    ndbrequire(connectPtr.p->connectState == ConnectRecord::ALTER_TABLE);
    make_new_table_read_and_writeable(tabPtr, connectPtr, signal);
    return;
  }
  case AlterTabReq::AlterTableComplete:
    jam();
    D("AlterTabReq::AlterTableComplete: tableId: " << tabPtr.i);
    connectPtr.i = req->connectPtr;
    ptrCheckGuard(connectPtr, cconnectFileSize, connectRecord);
    connectPtr.p->userpointer = senderData;
    connectPtr.p->userblockref = senderRef;

    if (!make_old_table_non_writeable(tabPtr, connectPtr))
    {
      jam();
      send_alter_tab_conf(signal, connectPtr);
      return;
    }
    /**
     * This is a table reorg, we want to wait for scans with
     * REORG_NOT_MOVED flag set to ensure that those scans have
     * completed before we start up a new ALTER TABLE REORG in
     * which case these scans might miss to read rows.
     *
     * Fall through to make this happen.
     */
  case AlterTabReq::AlterTableWaitScan:{
    jam();
    const NDB_TICKS now = NdbTick_getCurrentTicks();
    signal->theData[0] = DihContinueB::ZWAIT_OLD_SCAN;
    signal->theData[1] = tabPtr.i;
    signal->theData[2] = senderRef;
    signal->theData[3] = senderData;
    signal->theData[4] = connectPtr.i;
    signal->theData[5] = Uint32(now.getUint64() >> 32);
    signal->theData[6] = Uint32(now.getUint64());
    signal->theData[7] = 3; // Seconds to wait
    sendSignal(reference(), GSN_CONTINUEB, signal, 8, JBB);
    return;
  }
  default:
    ndbrequire(false);
    break;
  }

  if (AlterTableReq::getAddFragFlag(req->changeMask))
  {
    jam();
    SegmentedSectionPtr ptr;
    SectionHandle handle(this, signal);
    handle.getSection(ptr, 0);
    union {
      Uint16 buf[2+2*MAX_NDB_PARTITIONS];
      Uint32 _align[1];
    };
    copy(_align, ptr);
    releaseSections(handle);
    start_add_fragments_in_new_table(tabPtr, connectPtr, buf, signal);
    return;
  }

  send_alter_tab_conf(signal, connectPtr);
}

Uint32
Dbdih::add_fragments_to_table(Ptr<TabRecord> tabPtr, const Uint16 buf[])
{
  Uint32 replicas = buf[0];
  Uint32 cnt = buf[1];

  Uint32 i = 0;
  Uint32 err = 0;
  Uint32 current = tabPtr.p->totalfragments;
  for (i = 0; i<cnt; i++)
  {
    FragmentstorePtr fragPtr;
    Uint32 fragId = current + i;
    if (ERROR_INSERTED(7212) && cnt)
    {
      err = 1;
      CLEAR_ERROR_INSERT_VALUE;
      goto error;
    }

    if ((err = add_fragment_to_table(tabPtr, fragId, fragPtr)))
      goto error;

    fragPtr.p->m_log_part_id = buf[2+(1 + replicas)*i];
    ndbrequire(fragPtr.p->m_log_part_id < NDBMT_MAX_WORKER_INSTANCES);
    fragPtr.p->preferredPrimary = buf[2+(1 + replicas)*i + 1];
    fragPtr.p->partition_id = fragId % tabPtr.p->partitionCount;

    inc_ng_refcount(getNodeGroup(fragPtr.p->preferredPrimary));

    Uint32 activeIndex = 0;
    for (Uint32 j = 0; j<replicas; j++)
    {
      const Uint32 nodeId = buf[2+(1 + replicas)*i + 1 + j];
      ReplicaRecordPtr replicaPtr;
      allocStoredReplica(fragPtr,
                         replicaPtr,
                         nodeId,
                         current + i,
                         tabPtr.i);
      if (getNodeStatus(nodeId) == NodeRecord::ALIVE) {
        jam();
        ndbrequire(activeIndex < MAX_REPLICAS);
        fragPtr.p->activeNodes[activeIndex] = nodeId;
        activeIndex++;
      } else {
        jam();
        removeStoredReplica(fragPtr, replicaPtr);
        linkOldStoredReplica(fragPtr, replicaPtr);
      }
    }
    fragPtr.p->fragReplicas = activeIndex;
  }

  return 0;
error:
  for(i = i + current; i != current; i--)
  {
    release_fragment_from_table(tabPtr, i);
  }

  return err;
}

void
Dbdih::wait_old_scan(Signal* signal)
{
  jam();

  TabRecordPtr tabPtr;
  tabPtr.i = signal->theData[1];
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);

  if (tabPtr.p->m_scan_count[1] == 0)
  {
    jam();
    Uint32 senderRef = signal->theData[2];
    Uint32 senderData = signal->theData[3];
    Uint32 connectPtrI = signal->theData[4];

    AlterTabConf* conf = (AlterTabConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = senderData;
    conf->connectPtr = connectPtrI;
    sendSignal(senderRef, GSN_ALTER_TAB_CONF, signal,
               AlterTabConf::SignalLength, JBB);
    return;
  }

  const Uint32 start_hi = signal->theData[5];
  const Uint32 start_lo = signal->theData[6];
  const Uint32 wait = signal->theData[7];

  const NDB_TICKS start((Uint64(start_hi) << 32) | start_lo);
  const NDB_TICKS now  = NdbTick_getCurrentTicks();
  const Uint32 elapsed = (Uint32)NdbTick_Elapsed(start,now).seconds();

  if (elapsed > wait)
  {
    infoEvent("Waiting(%u) for scans(%u) to complete on table %u",
              elapsed,
              tabPtr.p->m_scan_count[1],
              tabPtr.i);

    if (wait == 3)
    {
      signal->theData[7] = 3 + 7;
    }
    else
    {
      signal->theData[7] = 2 * wait;
    }
  }

  sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 7);
}

Uint32
Dbdih::add_fragment_to_table(Ptr<TabRecord> tabPtr,
                             Uint32 fragId,
                             Ptr<Fragmentstore>& fragPtr)
{
  Uint32 fragments = tabPtr.p->totalfragments;
  Uint32 chunks = tabPtr.p->noOfFragChunks;

  ndbrequire(fragId == fragments); // Only add at the end

  if (ERROR_INSERTED(7211))
  {
    CLEAR_ERROR_INSERT_VALUE;
    return 1;
  }

  Uint32 allocated = chunks << LOG_NO_OF_FRAGS_PER_CHUNK;
  if (fragId < allocated)
  {
    jam();
    tabPtr.p->totalfragments++;
    getFragstore(tabPtr.p, fragId, fragPtr);
    return 0;
  }

  /**
   * Allocate a new chunk
   */
  fragPtr.i = cfirstfragstore;
  if (fragPtr.i == RNIL)
  {
    jam();
    return -1;
  }

  ptrCheckGuard(fragPtr, cfragstoreFileSize, fragmentstore);
  cfirstfragstore = fragPtr.p->nextFragmentChunk;
  ndbrequire(cremainingfrags >= NO_OF_FRAGS_PER_CHUNK);
  cremainingfrags -= NO_OF_FRAGS_PER_CHUNK;

  ndbrequire(chunks < NDB_ARRAY_SIZE(tabPtr.p->startFid));
  tabPtr.p->startFid[chunks] = fragPtr.i;
  Uint32 init_fragid = fragId;
  for (Uint32 i = 0; i<NO_OF_FRAGS_PER_CHUNK; i++)
  {
    jam();
    Ptr<Fragmentstore> tmp;
    tmp.i = fragPtr.i + i;
    ptrCheckGuard(tmp, cfragstoreFileSize, fragmentstore);
    initFragstore(tmp, init_fragid);
    init_fragid++;
  }

  tabPtr.p->totalfragments++;
  tabPtr.p->noOfFragChunks++;

  return 0;
}

/**
 * Both table mutex and table RCU lock need be held when calling
 * this function.
 */
void
Dbdih::release_fragment_from_table(Ptr<TabRecord> tabPtr, Uint32 fragId)
{
  FragmentstorePtr fragPtr;
  Uint32 fragments = tabPtr.p->totalfragments;
  Uint32 chunks = tabPtr.p->noOfFragChunks;

  if (fragId >= fragments)
  {
    jam();
    return;
  }
  ndbrequire(fragId == fragments - 1); // only remove at end
  ndbrequire(fragments != 0);

  getFragstore(tabPtr.p, fragId, fragPtr);
  dec_ng_refcount(getNodeGroup(fragPtr.p->preferredPrimary));

  releaseReplicas(& fragPtr.p->storedReplicas);
  releaseReplicas(& fragPtr.p->oldStoredReplicas);

  if (fragId == ((chunks - 1) << LOG_NO_OF_FRAGS_PER_CHUNK))
  {
    jam();

    getFragstore(tabPtr.p, fragId, fragPtr);

    fragPtr.p->nextFragmentChunk = cfirstfragstore;
    cfirstfragstore = fragPtr.i;
    cremainingfrags += NO_OF_FRAGS_PER_CHUNK;
    tabPtr.p->noOfFragChunks = chunks - 1;
  }

  tabPtr.p->totalfragments--;
}

void
Dbdih::send_alter_tab_ref(Signal* signal,
                          Ptr<TabRecord> tabPtr,
                          Ptr<ConnectRecord> connectPtr,
                          Uint32 errCode)
{
  AlterTabRef* ref = (AlterTabRef*)signal->getDataPtrSend();
  ref->senderRef = reference();
  ref->senderData = connectPtr.p->userpointer;
  ref->errorCode = errCode;
  sendSignal(connectPtr.p->userblockref, GSN_ALTER_TAB_REF, signal,
             AlterTabRef::SignalLength, JBB);
}

void
Dbdih::send_alter_tab_conf(Signal* signal, Ptr<ConnectRecord> connectPtr)
{
  AlterTabConf* conf = (AlterTabConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = connectPtr.p->userpointer;
  conf->connectPtr = connectPtr.i;
  sendSignal(connectPtr.p->userblockref, GSN_ALTER_TAB_CONF, signal,
             AlterTabConf::SignalLength, JBB);
}

void
Dbdih::saveTableFile(Signal* signal,
                     Ptr<ConnectRecord> connectPtr,
                     Ptr<TabRecord> tabPtr,
                     TabRecord::CopyStatus expectedStatus,
                     Callback& cb)
{
  ndbrequire(connectPtr.i == cb.m_callbackData);         // required
  ndbrequire(tabPtr.p->tabCopyStatus == expectedStatus); // locking
  memcpy(&connectPtr.p->m_callback, &cb, sizeof(Callback));

  tabPtr.p->tabCopyStatus = TabRecord::CS_COPY_TO_SAVE;
  tabPtr.p->tabUpdateState = TabRecord::US_CALLBACK;
  signal->theData[0] = DihContinueB::ZPACK_TABLE_INTO_PAGES;
  signal->theData[1] = tabPtr.i;
  sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
}

void
Dbdih::alter_table_writeTable_conf(Signal* signal, Uint32 ptrI, Uint32 err)
{
  jamEntry();
  ndbrequire(err == 0);

  ConnectRecordPtr connectPtr;
  connectPtr.i = ptrI;
  ptrCheckGuard(connectPtr, cconnectFileSize, connectRecord);

  switch(connectPtr.p->connectState){
  case ConnectRecord::ALTER_TABLE_REVERT:
  {
    jam();
    send_alter_tab_conf(signal, connectPtr);

    Ptr<TabRecord> tabPtr;
    tabPtr.i = connectPtr.p->table;
    ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
    ndbrequire(tabPtr.p->connectrec == connectPtr.i);
    tabPtr.p->connectrec = RNIL;
    release_connect(connectPtr);
    return;
  }
  case ConnectRecord::ALTER_TABLE:
  {
    jam();
    send_alter_tab_conf(signal, connectPtr);
    return;
  }
  default:
    jamLine(connectPtr.p->connectState);
    ndbrequire(false);
  }
}

void
Dbdih::drop_fragments(Signal* signal, Ptr<ConnectRecord> connectPtr,
                      Uint32 curr)
{
  ndbrequire(curr >= connectPtr.p->m_alter.m_org_totalfragments);
  if (curr == connectPtr.p->m_alter.m_org_totalfragments)
  {
    /**
     * done...
     */
    jam();
    Ptr<TabRecord> tabPtr;
    tabPtr.i = connectPtr.p->table;
    ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
    
    drop_fragments_from_new_table_view(tabPtr, connectPtr);

    switch(connectPtr.p->connectState){
    case ConnectRecord::ALTER_TABLE_ABORT:
    {
      jam();
      ndbrequire(tabPtr.p->tabCopyStatus == TabRecord::CS_ALTER_TABLE);
      tabPtr.p->tabCopyStatus = TabRecord::CS_IDLE;
      send_alter_tab_ref(signal, tabPtr, connectPtr, ~0);

      connectPtr.p->connectState = ConnectRecord::ALTER_TABLE;
      return;
    }
    case ConnectRecord::ALTER_TABLE_REVERT:
    {
      jam();
      Callback cb;
      cb.m_callbackData = connectPtr.i;
      cb.m_callbackFunction = safe_cast(&Dbdih::alter_table_writeTable_conf);
      saveTableFile(signal, connectPtr, tabPtr, TabRecord::CS_ALTER_TABLE, cb);
      return;
    }
    default:
      jamLine(connectPtr.p->connectState);
      ndbrequire(false);
    }
    return;
  }

  ndbrequire(curr > 0);
  DropFragReq* req = (DropFragReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->senderData = connectPtr.i;
  req->tableId = connectPtr.p->table;
  req->fragId = curr - 1;
  req->requestInfo = DropFragReq::AlterTableAbort;
  sendSignal(DBLQH_REF, GSN_DROP_FRAG_REQ, signal,
             DropFragReq::SignalLength, JBB);
}

void
Dbdih::execDROP_FRAG_REF(Signal* signal)
{
  ndbrequire(false);
}

void
Dbdih::execDROP_FRAG_CONF(Signal* signal)
{
  DropFragConf* conf = (DropFragConf*)signal->getDataPtr();

  ConnectRecordPtr connectPtr;
  connectPtr.i = conf->senderData;
  ptrCheckGuard(connectPtr, cconnectFileSize, connectRecord);

  drop_fragments(signal, connectPtr, conf->fragId);
}

/*
  G E T   N O D E S  
  **********************=
  */
/*****************************************************************************/
/* **********     TRANSACTION  HANDLING  MODULE                  *************/
/*****************************************************************************/

/**
 * Transaction Handling Module
 * ---------------------------
 *
 * This module can to a great extent be described as the heart of the
 * distribution aspects of MySQL Cluster. It is an essential part of key
 * operations and scan operations. It will ensure that the TC block will get
 * the correct data about table distribution in all operations of the cluster.
 *
 * It is absolutely for one of the USPs (Unique Selling Points) of MySQL
 * Cluster which is its high availability and its ability to perform online
 * meta data changes while still providing both read and write services
 * using the old meta data and even being able to handle both new and old
 * meta data at the same time during the switch over phase.
 *
 * It is absolutely vital for the recovery aspects and this module is the
 * reason that we can support failover in a number of milliseconds. The
 * longest time is to discover the failure, when that is done it is a
 * matter of 2 signals back and forth to all nodes to reconfigure the
 * nodes. It has much help in this node failure handling from QMGR and
 * NDBCNTR blocks.
 *
 * As described in database theory a node failure is handled as a transaction
 * in itself. This transaction is executed by QMGR and NDBCNTR and when
 * the report about a failed node reaches DBDIH it will immediately switch
 * the replicas used to read and write using the data controlled by this
 * module.
 *
 * The problems we are facing in this module are the following:
 * -----------------------------------------------------------
 * 1) We need to quickly remove fragment replicas belonging to nodes that
 *    died.
 *
 * 2) We need to include new fragment replicas to be writeable and later
 *    to be both read and writeable. This as part of bringing new nodes
 *    up.
 *
 * 3) To be able to balance up the usage of nodes we need the ability to
 *    switch primary replica after completing node recovery.
 *
 * 4) We need to add new tables with a flexible table distribution.
 *
 * 5) We need the ability to reorganize a table to make it use new nodes
 *    that have been added to the cluster.
 *
 * 6) We need to handle fully replicated tables that can at times read
 *    from any node that contains the table.
 *
 * 7) Supporting updates of several fragments when fully replicated using
 *    an iterator over a copy fragments.
 *
 * 8) We need to handle long-running scans that need a consistent view of
 *    the table for its entire operation while at the same reorganising the
 *    table.
 *
 * 9) We need to support many different variants of table distributions,
 *    as an example we can have tables with one fragment per LDM per node,
 *    we could have tables with just one fragment per node group and so
 *    forth.
 *
 * 10)We need to support many different fragmentation types. This includes
 *    range partitioning, list partitioning, key partitioning, linear key
 *    partitioning. These variants are currently only supported when
 *    operating only with a MySQL Server, so no direct NDB API access for
 *    these tables is allowed. Also these tables have no ability for table
 *    reorganisation at this time.
 *
 *    The most important fragmentation types we currently support is based
 *    on the concept of hash maps. So the table is distributed with e.g.
 *    3840 hash parts. When the table has 8 fragments these 3840 is
 *    distributed among those 8 fragments. If the table is later is
 *    reorganised to have 12 fragments then some of those 3840 hash
 *    parts will be moved to the new fragments and a significant number of
 *    those parts will stay put and not need any move.
 *
 * 11)Finally we also have a programmatic problem. The code that changes
 *    these data structures is not critical in performance and is handled
 *    by a single thread in each data node.
 *
 *    However reading of those data structures happens in each key operation
 *    and several times in a scan operation. There are many readers of this
 *    data structure, it is read from all other threads in the data node
 *    although mostly from the TC threads.
 *
 *    Given the rarity of updates to those data structures we opted for an
 *    RCU mechanism. So we get a counter before reading, then we read,
 *    after reading we check that the counter is still the same, if not
 *    we retry. In addition there are a number of memory barriers used to
 *    support this properly in a highly parallel environment.
 *
 *    This mechanism makes for scaling which is almost unlimited. It also
 *    means that any updates of these data structures have to be done in
 *    a safe manner always avoiding that the user might trap on a pointer
 *    or reference which isn't properly set. This requires very careful
 *    programming. To support this carefulness we have gathered together
 *    all code performing those functions into one module here in DDBIH.
 *
 *    To solve 8) in a multithreaded environments we use a mutex such that
 *    scans increment a reference counter when they start and decrement it
 *    when done. In this manner we can always keep track of any still
 *    outstanding scan operations at table reorganisation time.
 *
 * Distinguish between READs and WRITEs in DIH interface
 * -----------------------------------------------------
 * DIH will always deliver a list of all nodes that have a replica of the
 * data. However some of those nodes could be write-only during node
 * recovery and during on-line table reorganisation. However the receiver
 * of this data is only allowed to use the list in one of two ways.
 * 
 * 1) Use entire list for write transactions
 * 2) Use any replica for reading in my own node (if read backup feature
 *    is active on the table AND READ COMMITTED is used to read the data.
 *
 * The reason that this works is that no one is allowed to use this
 * interface to read data while still in node recovery. So this is the manner
 * to ensure that we don't read any fragments that are not yet fully
 * recovered.
 *
 * For table reorg of a table we will only report back the fragments that
 * are readable. The fragments that are still in the build process will
 * be reported as new fragments and will only be used by special
 * transactions that perform the copy phase and the delete phase.
 *
 * Description of key algorithms DBDIH participates in
 * ---------------------------------------------------
 * One important feature in MySQL Cluster is ALTER TABLE REORG. This makes
 * it possible to reorganize the data in a table to make use of a new
 * node group that has been added. It also makes it possible to extend
 * the number of fragments in a table. It is still not supported to
 * decrease the number of fragments in a table.
 *
 * DBDIH participates in four very crucial points in this table reorg.
 * 1) start_add_fragments_in_new_table
 *    This phase is about creating new empty fragments and requires insertion
 *    of the new fragments into the shared data structures. The fragments are
 *    still not to be used, but it is imperative that we insert the data in
 *    a controlled manner.
 *
 * 2) make_new_table_writeable
 *    This method is called when all new fragments have been created, all
 *    triggers required to perform the copy phase has been installed. It is
 *    now time to make the new fragments participate in write transactions
 *    in a controlled manner.
 *    
 *    This means that we have 2 hash maps, one for the old table distribution
 *    and for the new table distribution. When a write happens we need to
 *    keep both in synch if the write goes to different fragments in the two
 *    table distributions.
 *
 *    The data is also used when copying data over from old fragments to the
 *    new fragments.
 *
 *    Fully replicated tables are a bit special, they cannot add new real
 *    fragments, but they can add new copy fragments and thus extend the
 *    number of replicas of the data. In this phase we have to distinguish
 *    between which fragments can be used for reading and which needs to
 *    be updated.
 *
 *    We handle this by always ensuring that new fragments are at the end of
 *    list of copy fragments and that we never report any fragments with
 *    higher fragment id than the current variable totalfragments states.
 *
 * 3) make_table_read_and_writeable
 *    This is called after the copy phase has been completed. The fragments
 *    are now filled with all data and are also available for reading. The
 *    old fragments are still kept up to date. So here we need to ensure
 *    that all writes goes to both old and new fragment of each row.
 *
 * 4) make_old_table_non_writeable
 *    Now all transactions using old table distribution have completed (a
 *    number of scan operations) and we remove the old hash map from the
 *    table. We are now ready to start deleting data from old fragments
 *    This data isn't required to stay in those fragments any more.
 *
 * MySQL Cluster also supports schema transactions, this means that schema
 * transactions can be rolled back if they fail for some reason. There are
 * two functions used to rollback some of the above.
 *
 * If we have passed 4 it is too late to rollback and thus recovery is about
 * ensuring that the schema transaction is completed. Between 3 and 4 we are
 * able to both roll backward and roll forward. So it depends on other
 * parts of the schema transaction which path is choosen. If we fail between
 * 2 and 3 then we will have to remove the new table as writeable.
 * This is performed by make_new_table_non_writeable.
 * If a failure happens between 1 and 2 then we have to drop the new
 * fragments, this happens in drop_fragments_from_new_table_view. This method
 * is called also during revert ALTER TABLE when failure occurred between 2
 * and 3.
 *
 * Description of copy phase of ALTER TABLE REORG
 * ----------------------------------------------
 * The copy phase of ALTER TABLE REORG involves a great number of blocks.
 * The below setup and tear down phase is a description of what happens
 * for each table being reorganized.
 *
 * The below process happens in all nodes in parallel. Each node will
 * take care of the fragment replicas for which it is the primary
 * replica. This makes most of the communication here be local to
 * a node. Only the sending of updates to the new fragments and
 * updates to the backup replicas in the same node group will be
 * done over the network.
 * 
 * DBDICT    DBDICT    TRIX          SUMA    DBUTIL      DBDIH   DBLQH
 * COPY_DATA_REQ
 * ------------>
 *   COPY_DATA_IMPL_REQ
 * --------------------->
 *                       UTIL_PREPARE_REQ
 *                       ---------------------->
 *   GET_TABINFOREQ
 * <--------------------------------------------
 *   GET_TABINFOCONF
 * -------------------------------------------->
 *                       UTIL_PREPARE_CONF
 *                       <----------------------
 *                       SUB_CREATE_REQ
 *                       ------------->
 *   GET_TABINFOREQ
 * <-----------------------------------
 *   GET_TABINFOCONF
 * ----------------------------------->
 *                       SUB_CREATE_CONF
 *                       <-------------
 *                       SUB_SYNC_REQ
 *                       ------------->
 *                                     DIH_SCAN_TAB_REQ (immediate)
 *                                     ---------------------->
 *                                     DIH_SCAN_TAB_CONF
 *                                     <----------------------
 *                                 Send DIH_SCAN_TAB_CONF to get rt break
 *
 *                                     DIGETNODESREQ (immediate)
 *                                     ---------------------->
 *                                     DIGETNODESCONF
 *                                     <----------------------
 *                         Get distribution data for each fragment
 *                         using DIGETNODESREQ possibly with
 *                         rt break through CONTINUEB. This builds
 *                         a list of fragments to handle.
 *
 *                                     SCAN_FRAGREQ
 *                                     -------------------------------->
 *                                     For each row we receive and send:
 *                                     TRANSID_AI
 *                                     <-------------------------------
 *                                     KEYINFO20
 *                                     <-------------------------------
 *                       SUB_TABLE_DATA
 *                       <-------------
 *                       UTIL_EXECUTE_REQ
 *                       --------------------->
 *                       TCKEYREQ to DBTC
 *                       ------------------------->
 *                       TCKEYCONF from DBTC
 *                       <-------------------------
 *                       UTIL_EXECUTE_CONF
 *                       <---------------------
 *
 * After 16 rows the scan will return (this will happen for each 16 row
 *                                         SCAN_FRAGCONF
 *                                      <--------------------------------
 *                       SUB_SYNC_CONTINUE_REQ
 *                       <--------------
 *                       wait for all outstanding transactions to complete
 *                       SUB_SYNC_CONTINUE_CONF
 *                       -------------->
 *                                         SCAN_NEXTREQ
 *                                       -------------------------------->
 *
 * Every now and then a fragment will have its scan completed. Then it will
 * receive SCAN_FRAGCONF with close flag set. Then it will send a new
 * SCAN_FRAGREQ for the next fragment to copy. When no more fragments is
 * available for copying then the copy action is completed.
 *
 * Copy phase completed after SCAN_FRAGCONF(close) from last fragment =>
 *                       SUB_SYNC_CONF
 *                       <-------------
 *                       WAIT_GCP_REQ
 *                       ----------------------------------->
 *
 *                       ..... wait for highest GCI to complete
 *
 *                       WAIT_GCP_CONF
 *                       <----------------------------------
 *                       SUB_REMOVE_REQ
 *                       ------------->
 *                       SUB_REMOVE_CONF
 *                       <-------------
 *                       UTIL_RELEASE_REQ
 *                       ------------------------>
 *                       UTIL_RELEASE_CONF
 *                       <------------------------
 * COPY_DATA_IMPL_CONF
 * <---------------------
 *
 * As can be seen the TRIX block is working with SUMA and DBUTIL to set up
 * the copy phase. The DBUTIL block is the block that performs the actual
 * read of the old fragments (through scans) and then copies the data to
 * the new fragments using write operations (key operations). Trix isn't
 * doing any real work, it is merely acting as a coordinator of the work
 * done.
 *
 * DBUTIL needs to set up generic data structures to enable receiving rows
 * from any table and pass them onto to be written from DBTC. There is fair
 * amount of code to do this, but it is straightforward code that doesn't
 * have much interaction issues, it is a fairly pure data structure problem.
 *
 * These data structures are released in UTIL_RELEASE_REQ.
 *
 * SUMA also reads the table metadata through the GET_TABINFO interface to
 * DICT, this is however only needed to read the number of attributes and
 * table version and verifying that the table exists.
 *
 * TRIX uses similar interfaces also to build indexes, create foreign keys
 * other basic operations. For COPY_DATA_IMPL_REQ TRIX receives the number
 * of real fragments from DBDICT. SUB_SYNC_REQ contains fragId == ZNIL which
 * means sync all fragments.
 *
 * Actually the copy phase is an exact replica of the also mentioned delete
 * phase. So when reorganising the data one first calls this functionality
 * using a few important flags. The first phase uses the flag REORG_COPY.
 * The second phase uses the flag called REORG_DELETE.
 *
 * COPY_DATA_IMPL_REQ always set the RF_WAIT_GCP, this means that when
 * TRIX receives SUB_SYNC_CONF we will wait for a GCP to complete to ensure
 * that the copy transactions are stable on disk through the REDO log.
 *
 * The SCAN_FRAGREQ uses TUP order if disk attributes in table. It always
 * scans using exclusive locks. This means that we will temporarily lock
 * each row when performing copy phase for the row, there should be no
 * risk of deadlocks due to this since only one row lock is required. So
 * deadlock cycles can form due to this. We use parallelism 16 in the
 * scanning.
 *
 * For each row we receive we get a TRANSID_AI with the attribute information
 * and KEYINFO20 with the key information. Based on this information we create
 * a SUB_TABLE_DATA signal and pass this to TRIX for execution by DBUTIL.
 * We send it to DBUTIL in a UTIL_EXECUTE_REQ signal referring to the prepared
 * transaction in DBUTIL. Each row is executed as a separate Scan Take Over
 * transaction. When the transaction is completed we get a UTIL_EXECUTE_CONF
 * response back. We record the GCI used to ensure we know the highest GCI
 * used as part of the Copy phase.
 *
 * The TCKEYREQ sent to DBTC is a Write operation and thus will either
 * overwrite the row or it will insert if it doesn't exist.
 *
 * There is a lot of logic in DBTC, DBLQH and DBTUP which is used to control
 * the upates on various fragments. During Copy phase and Delete phase all
 * fragments have a new reorg trigger installed. This trigger is fired for
 * all normal writes on tuples that are currently moving, nothing happens
 * for tuples that aren't moving. The trigger fires for moving tuples in
 * the old fragments and also in the new fragments when these are set to
 * online as having all data. In this phase we will make the new fragments
 * readable and also becomes the primary fragment for the tuples and in this
 * phase we still need to maintain the data in the old fragments until we
 * have completed the scans on those.
 *
 * This trigger will thus only fire during the time when we have two hash
 * maps here in DBDIH. As soon as we set the new hash map to RNIL the
 * reorg trigger won't fire anymore for writes going through this DIH.
 *
 * The copy phase and delete phase both sets the reorg flag in TCKEYREQ.
 * For the copy phase this means that the copy is only performed for
 * rows that are moving, for rows that aren't moving the action is
 * immediately completed. For moving rows the write is performed and will
 * either result in the row being inserted or the row being overwritten
 * with the same value (this will happen if an insert reorg trigger
 * inserted the row already).
 *
 * During the delete phase a delete action will be performed towards the
 * new hash map (which is actually now the old hash map since we have
 * switched to the new hash map as the original one and the old one is
 * the new one. This means that the delete will be performed only on
 * the old fragment and thus removing a row that has already completed
 * its move.
 *
 * When a reorg trigger is fired we only need to write the other fragment
 * with the same data as we did in the first fragment. However we have to
 * take into account that the fragments might have been swapped since
 * the original operation was here and when we come here to handle the
 * fired trigger. So the user of this interface have to verify that the
 * fragment id to update as new fragment isn't simply the same that the
 * trigger fired from, if it is then the other fragment is the one reported
 * as the current fragment from DIGETNODESREQ.
 *
 * How to handle ALTER TABLE REORG for fully replicated tables
 * -----------------------------------------------------------
 * First some observations. In fully replicated tables no data is moving.
 * We only need to copy the data to the new fragments. This means that
 * there is no need for reorg triggers. There is also no need for a
 * delete phase since no data has moved.
 *
 * The reorg triggers is avoided simply by never reporting REORG_MOVING
 * in the DIH interface. This ensures that no reorg trigger will ever
 * fire. Avoiding the delete phase isn't strictly necessary but it is
 * an easy optimisation and we can simply send COPY_DATA_IMPL_CONF
 * directly from COPY_DATA_IMPL_REQ in the delete phase to avoid it.
 *
 * The copy phase can be handled by DBTC putting a different meaning to
 * the reorg flag. Normall we would set SOF_REORG_COPY to ensure that
 * we only write the new fragment for those copy rows. Here we want to
 * perform an update that uses the fully replicated triggers to ensure
 * that all copy fragments are updated. One simple manner to do this is
 * to simply perform the update and let the fully replicated trigger
 * update all other copy fragments. However this means that we are
 * performing lots of unncessary writes.
 *
 * A very simple optimisation is to instead perform the write on the
 * first new copy fragment. In this case the trigger will fire and
 * since the initial fragment is the first new fragment and the
 * iterator only goes towards higher fragment ids, thus we thus
 * ensures that we won't write the old fragment that already has the
 * correct data. So this write becomes a perfectly normal update on
 * fully replicated table except that it uses a triggered operation
 * on a copy fragment which is normally not done. But triggers are
 * installed on also the copy fragments, so this is ok.
 *
 * This simple optimisation requires a new flag sent in the DIH
 * interface since DIH needs to be told to return the first
 * new fragment rather than the main fragment.
 *
 * More details about ALTER TABLE REORG
 * ------------------------------------
 * DBTUP has a bit in each tuple header called REORG_MOVE. This bit is set on
 * the first time that an update/delete/insert happens on the row after
 * calling make_new_table_writeable. After make_new_table_writeable has been
 * called we will set DiGetNodesConf::REORG_MOVING for rows that are to be
 * moved. So the first such a row has a write of it, this flag will be set
 * and also the reorg trigger will fire and send the update to the new
 * fragment. However the copy phase will copy this row even if this bit is
 * set since the bit can be set also by a transaction that is later aborted.
 * So there is no safe way of ensuring that a user transaction has actually
 * transferred this row. So when SUMA performs the scan in the copy phase it
 * will be a normal scan seeing all rows.
 *
 * When we have completed the copy phase and entered the delete phase then
 * we have set the m_scan_reorg_flag on the table and this means that all
 * transactions will have to set the flag ScanFragReq::REORG_NOT_MOVED to
 * ensure that they don't scan moved rows in both the new and the old
 * fragments. When all moved rows have been deleted from the old fragments
 * then we can stop reporting this flag to starting scans.
 *
 * A scan that is using the REORG_NOT_MOVED is safe unless we are moving
 * to yet another ALTER TABLE REORG of the same table very quickly. However
 * a potential problem could exist if we have a very long-running scan
 * and we start a new table reorg and user transactions start setting the
 * REORG_MOVE flag again. In that case the scan will actually miss those
 * rows. So effectively to close all possible problems we wait also for
 * all scans to complete also after completing the REORG_DELETE phase.
 * This ensures that we avoid this issue.
 */

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
  Uint32 distr_key_indicator = req->distr_key_indicator;
  Uint32 anyNode = req->anyNode;
  Uint32 scan_indicator = req->scan_indicator;
  Uint32 get_next_fragid_indicator = req->get_next_fragid_indicator;
  Uint32 ttabFileSize = ctabFileSize;
  Uint32 fragId;
  Uint32 newFragId = RNIL;
  Uint32 nodeCount;
  Uint32 sig2;
  Ptr<Hash2FragmentMap> ptr;
  DiGetNodesConf * const conf = (DiGetNodesConf *)&signal->theData[0];
  TabRecord* regTabDesc = tabRecord;
  EmulatedJamBuffer * jambuf = (EmulatedJamBuffer*)req->jamBufferPtr;
  thrjamEntry(jambuf);
  ptrCheckGuard(tabPtr, ttabFileSize, regTabDesc);

  /**
   * This check will be valid for the following reasons:
   * 1) If it is primary key operation we will have checked that the table
   *    is existing in DBTC before coming here and DBDIH is informed of new
   *    tables BEFORE DBTC and informed of dropping tables AFTER DBTC. So
   *    it is safe that if DBTC knows that a table exist then for sure we
   *    we will as well.
   *
   * 2) For ordered index scans we keep track of the number of scans working
   *    on the ordered index, so we won't be able to drop the index until
   *    all scans on the index has completed.
   */
  if (DictTabInfo::isOrderedIndex(tabPtr.p->tableType))
  {
    thrjam(jambuf);
    tabPtr.i = tabPtr.p->primaryTableId;
    ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
  }

loop:
  /**
   * To ensure we operate on a correct view of both table distribution and
   * alive nodes, we use an RCU mechanism to protect this call to
   * DIGETNODESREQ, this means that any changes in DBDIH will be reflected
   * in external DBTCs reading this data as well. These are variables
   * updated very seldomly and we only need to read them, thus a RCU is a
   * very powerful mechanism to achieve this.
   */
  Uint32 tab_val = tabPtr.p->m_lock.read_lock();
  Uint32 node_val = m_node_view_lock.read_lock();
  Uint32 map_ptr_i = tabPtr.p->m_map_ptr_i;
  Uint32 new_map_ptr_i = tabPtr.p->m_new_map_ptr_i;

  if (get_next_fragid_indicator != 0)
  {
    /**
     * The requester is interested in getting the next copy fragment.
     * This should only happen for Fully replicated tables atm.
     */
    thrjam(jambuf);
    fragId = hashValue;
    ndbassert((tabPtr.p->m_flags & TabRecord::TF_FULLY_REPLICATED) != 0);
    getFragstore(tabPtr.p, fragId, fragPtr);
    conf->fragId = fragPtr.p->nextCopyFragment;
    conf->zero = 0;
    goto check_exit;
  }
  /* When distr key indicator is set, regardless
   * of distribution algorithm in use, hashValue
   * IS fragment id.
   */
  if (distr_key_indicator)
  {
    thrjam(jambuf);
    fragId = hashValue;
    /**
     * This check isn't valid for scans, if we ever implement the possibility
     * to decrease the number of fragments then this can be true and still
     * be ok since we are using the old meta data and thus getFragstore
     * is still working even if we are reading a fragId out of range. We
     * keep track of such long-running scans to ensure we know when we
     * can remove the fragments completely.
     *
     * For execution of fully replicated triggers we come here with anyNode=3
     * In this case we have received the fragmentId from the code above with
     * get_next_fragid_indicator and we should also ensure that all writes
     * of fully replicated triggers also go to the new fragments.
     * 
     */
    if (unlikely((!scan_indicator) &&
                 fragId >= tabPtr.p->totalfragments &&
                 anyNode != 3))
    {
      thrjam(jambuf);
      conf->zero= 1; //Indicate error;
      signal->theData[1]= ZUNDEFINED_FRAGMENT_ERROR;
      goto error;
    }
  }
  else if (tabPtr.p->method == TabRecord::HASH_MAP)
  {
    if ((tabPtr.p->m_flags & TabRecord::TF_FULLY_REPLICATED) == 0)
    {
      thrjam(jambuf);
      g_hash_map.getPtr(ptr, map_ptr_i);
      fragId = ptr.p->m_map[hashValue % ptr.p->m_cnt];

      if (unlikely(new_map_ptr_i != RNIL))
      {
        thrjam(jambuf);
        g_hash_map.getPtr(ptr, new_map_ptr_i);
        newFragId = ptr.p->m_map[hashValue % ptr.p->m_cnt];
        if (newFragId == fragId)
        {
          thrjam(jambuf);
          newFragId = RNIL;
        }
      }
    }
    else
    {
      /**
       * Fully replicated table. There are 3 cases:
       * anyNode == 0
       *   This is a normal read or write. We want the main fragment.
       * anyNode == 1
       *   This is a committed read. We want any fragment which is readable.
       * anyNode == 2
       *   This is a write from the copy phase of ALTER TABLE REORG
       *   We want the first new fragment.
       */
      thrjam(jambuf);
      g_hash_map.getPtr(ptr, map_ptr_i);
      const Uint32 partId = ptr.p->m_map[hashValue % ptr.p->m_cnt];
      if (anyNode == 2)
      {
        thrjam(jambuf);
        fragId = findFirstNewFragment(tabPtr.p, fragPtr, partId, jambuf);
        if (fragId == RNIL)
        {
          conf->zero = 0;
          conf->fragId = fragId;
          conf->nodes[0] = 0;
          goto check_exit;
        }
      }
      else fragId = partId;
    }
  }
  else if (tabPtr.p->method == TabRecord::LINEAR_HASH)
  {
    thrjam(jambuf);
    fragId = hashValue & tabPtr.p->mask;
    if (fragId < tabPtr.p->hashpointer) {
      thrjam(jambuf);
      fragId = hashValue & ((tabPtr.p->mask << 1) + 1);
    }//if
  }
  else if (tabPtr.p->method == TabRecord::NORMAL_HASH)
  {
    thrjam(jambuf);
    fragId= hashValue % tabPtr.p->partitionCount;
  }
  else
  {
    thrjam(jambuf);
    ndbassert(tabPtr.p->method == TabRecord::USER_DEFINED);

    /* User defined partitioning, but no distribution key passed */
    conf->zero= 1; //Indicate error;
    signal->theData[1]= ZUNDEFINED_FRAGMENT_ERROR;
    goto error;
  }
  if (ERROR_INSERTED_CLEAR(7240))
  {
    /* Error inject bypass the RCU lock */
    thrjam(jambuf);
    conf->zero= 1; //Indicate error;
    signal->theData[1]= ZUNDEFINED_FRAGMENT_ERROR;
    return;
  }
  if (ERROR_INSERTED_CLEAR(7234))
  {
    /* Error inject bypass the RCU lock */
    thrjam(jambuf);
    conf->zero= 1; //Indicate error;
    signal->theData[1]= ZLONG_MESSAGE_ERROR;
    return;
  }
  getFragstore(tabPtr.p, fragId, fragPtr);
  if (anyNode == 1)
  {
    thrjam(jambuf);

    /* anyNode is currently only useful for fully replicated tables */
    ndbassert((tabPtr.p->m_flags & TabRecord::TF_FULLY_REPLICATED) != 0);

    /**
     * search fragments to see if local fragment can be found
     *
     */
    fragId = findLocalFragment(tabPtr.p, fragPtr, jambuf);
  }
  nodeCount = extractNodeInfo(jambuf, fragPtr.p, conf->nodes);
  sig2 = (nodeCount - 1) + 
    (fragPtr.p->distributionKey << 16) + 
    (dihGetInstanceKey(fragPtr) << 24);
  conf->zero = 0;
  conf->reqinfo = sig2;
  conf->fragId = fragId;

  if (unlikely(newFragId != RNIL))
  {
    thrjam(jambuf);
    conf->reqinfo |= DiGetNodesConf::REORG_MOVING;
    getFragstore(tabPtr.p, newFragId, fragPtr);
    nodeCount = extractNodeInfo(jambuf,
                               fragPtr.p,
                               conf->nodes + 2 + MAX_REPLICAS);
    conf->nodes[MAX_REPLICAS] = newFragId;
    conf->nodes[MAX_REPLICAS + 1] = (nodeCount - 1) +
      (fragPtr.p->distributionKey << 16) +
      (dihGetInstanceKey(fragPtr) << 24);
  }

check_exit:
  if (unlikely(!tabPtr.p->m_lock.read_unlock(tab_val)))
    goto loop;
  if (unlikely(!m_node_view_lock.read_unlock(node_val)))
    goto loop;

error:
  /**
   * Ensure that also error conditions are based on a consistent view of
   * the data. In this no need to check node view since it wasn't used.
   */
  if (unlikely(!tabPtr.p->m_lock.read_unlock(tab_val)))
    goto loop;
  return;

}//Dbdih::execDIGETNODESREQ()

void
Dbdih::make_node_usable(NodeRecord *nodePtr)
{
  /**
   * Called when a node is ready to be used in transactions.
   * This means that the node needs to participate in writes,
   * it isn't necessarily ready for reads yet.
   */
  m_node_view_lock.write_lock();
  nodePtr->useInTransactions = true;
  m_node_view_lock.write_unlock();
}

void
Dbdih::make_node_not_usable(NodeRecord *nodePtr)
{
  /**
   * Node is no longer to be used in neither read nor
   * writes. The node is dead.
   */
  m_node_view_lock.write_lock();
  nodePtr->useInTransactions = false;
  m_node_view_lock.write_unlock();
}

Uint32
Dbdih::findPartitionOrder(const TabRecord *tabPtrP,
                          FragmentstorePtr fragPtr)
{
  Uint32 order = 0;
  FragmentstorePtr tempFragPtr;
  Uint32 fragId = fragPtr.p->partition_id;
  do
  {
    jam();
    getFragstore(tabPtrP, fragId, tempFragPtr);
    if (fragPtr.p == tempFragPtr.p)
    {
      jam();
      return order;
    }
    fragId = tempFragPtr.p->nextCopyFragment;
    order++;
  } while (fragId != RNIL);
  return RNIL;
}

Uint32
Dbdih::findFirstNewFragment(const  TabRecord * tabPtrP,
                            FragmentstorePtr & fragPtr,
                            Uint32 fragId,
                            EmulatedJamBuffer *jambuf)
{
  /**
   * Used by fully replicated tables to find the first new fragment
   * to copy data to during the copy phase.
   */
  do
  {
    getFragstore(tabPtrP, fragId, fragPtr);
    if (fragPtr.p->fragId >= tabPtrP->totalfragments)
    {
      /* Found first new fragment */
      break;
    }
    fragId = fragPtr.p->nextCopyFragment;
    if (fragId == RNIL)
      return fragId;
  } while (1);
  return fragPtr.p->fragId;
}

Uint32
Dbdih::findLocalFragment(const  TabRecord * tabPtrP,
                         FragmentstorePtr & fragPtr,
                         EmulatedJamBuffer *jambuf)
{
  /**
   * We have found the main fragment, but we want to use any of the copy
   * fragments, so we search forward in the list of copy fragments until we
   * find a fragment that has a replica on our node. In rare cases (after
   * adding a node group and not yet reorganised all tables and performing
   * this on one of the new nodes in these new node groups, it could occur).
   *
   * Start searching the main fragment and then proceeding
   * forward until no more exists.
   */
  Uint32 fragId = fragPtr.p->fragId;
  do
  {
    thrjam(jambuf);
    if (check_if_local_fragment(jambuf, fragPtr.p))
    {
      thrjam(jambuf);
      return fragId;
    }
    /* Step to next copy fragment. */
    fragId = fragPtr.p->nextCopyFragment;
    if (fragId == RNIL || fragId > tabPtrP->totalfragments)
    {
      thrjam(jambuf);
      break;
    }
    getFragstore(tabPtrP, fragId, fragPtr);
  } while (1);
  /**
   * When no local fragment was found, simply use the last
   * copy fragment found, in this manner we avoid using
   * the main fragment during table reorg, this node group
   * has much to do in this phase.
   */
  return fragPtr.p->fragId;
}

bool
Dbdih::check_if_local_fragment(EmulatedJamBuffer *jambuf,
                               const Fragmentstore *fragPtr)
{
  for (Uint32 i = 0; i < fragPtr->fragReplicas; i++)
  {
    thrjam(jambuf);
    if (fragPtr->activeNodes[i] == getOwnNodeId())
    {
      thrjam(jambuf);
      return true;
    }
  }
  return false;
}

Uint32 Dbdih::extractNodeInfo(EmulatedJamBuffer *jambuf,
                              const Fragmentstore * fragPtr,
                              Uint32 nodes[]) 
{
  Uint32 nodeCount = 0;
  nodes[0] = nodes[1] = nodes[2] = nodes[3] = 0;
  for (Uint32 i = 0; i < fragPtr->fragReplicas; i++) {
    thrjam(jambuf);
    NodeRecordPtr nodePtr;
    ndbrequire(i < MAX_REPLICAS);
    nodePtr.i = fragPtr->activeNodes[i];
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
    if (nodePtr.p->useInTransactions) {
      thrjam(jambuf);
      nodes[nodeCount] = nodePtr.i;
      nodeCount++;
    }//if
  }//for
  ndbrequire(nodeCount > 0);
  return nodeCount;
}//Dbdih::extractNodeInfo()

#define DIH_TAB_WRITE_LOCK(tabPtrP) \
  do { assertOwnThread(); tabPtrP->m_lock.write_lock(); } while (0)

#define DIH_TAB_WRITE_UNLOCK(tabPtrP) \
  do { assertOwnThread(); tabPtrP->m_lock.write_unlock(); } while (0)

void
Dbdih::start_scan_on_table(TabRecordPtr tabPtr,
                           Signal *signal,
                           Uint32 schemaTransId,
                           EmulatedJamBuffer *jambuf)
{
  /**
   * This method is called from start of scans in TC threads. We need to
   * protect against calls from multiple threads. The state and the
   * m_scan_count is protected by the mutex.
   *
   * To avoid having to protect this code with both mutex and RCU code
   * we ensure that the mutex is also held anytime we update the
   * m_map_ptr_i, totalfragments, noOfBackups, m_scan_reorg_flag
   * and partitionCount.
   */
  NdbMutex_Lock(&tabPtr.p->theMutex);

  if (tabPtr.p->tabStatus != TabRecord::TS_ACTIVE)
  {
    if (! (tabPtr.p->tabStatus == TabRecord::TS_CREATING &&
           tabPtr.p->schemaTransId == schemaTransId))
    {
      thrjam(jambuf);
      goto error;
    }
  }

  tabPtr.p->m_scan_count[0]++;
  ndbrequire(tabPtr.p->m_map_ptr_i != DihScanTabConf::InvalidCookie);
  {
    DihScanTabConf* conf = (DihScanTabConf*)signal->getDataPtrSend();
    conf->tableId = tabPtr.i;
    conf->senderData = 0; /* 0 indicates success */
    /**
     * For Fully replicated tables the totalfragments means the total
     * number of fragments including the copy fragment. Here however
     * we should respond with the real fragment count which is either
     * 1 or the number of LDMs dependent on which partition balance
     * the table was created with.
     *
     * partitionCount works also for other tables. We always scan
     * the real fragments when scanning all fragments and those
     * are always the first fragments in the interface to DIH.
     */
    conf->fragmentCount = tabPtr.p->partitionCount;

    conf->noOfBackups = tabPtr.p->noOfBackups;
    conf->scanCookie = tabPtr.p->m_map_ptr_i;
    conf->reorgFlag = tabPtr.p->m_scan_reorg_flag;
    NdbMutex_Unlock(&tabPtr.p->theMutex);
    return;
  }

error:
  DihScanTabRef* ref = (DihScanTabRef*)signal->getDataPtrSend();
  ref->tableId = tabPtr.i;
  ref->senderData = 1; /* 1 indicates failure */
  ref->error = DihScanTabRef::ErroneousTableState;
  ref->tableStatus = tabPtr.p->tabStatus;
  ref->schemaTransId = schemaTransId;
  NdbMutex_Unlock(&tabPtr.p->theMutex);
  return;
}

void
Dbdih::complete_scan_on_table(TabRecordPtr tabPtr,
                              Uint32 map_ptr_i,
                              EmulatedJamBuffer *jambuf)
{
  /**
   * This method is called from other TC threads to signal that a
   * scan is completed. We keep track of number of outstanding scans
   * in two variables for old and new metadata (normally there is
   * only new metadata, but during changes we need this to ensure
   * that scans can continue also during schema changes).
   */

  NdbMutex_Lock(&tabPtr.p->theMutex);
  if (map_ptr_i == tabPtr.p->m_map_ptr_i)
  {
    thrjam(jambuf);
    ndbassert(tabPtr.p->m_scan_count[0]);
    tabPtr.p->m_scan_count[0]--;
  }
  else
  {
    thrjam(jambuf);
    ndbassert(tabPtr.p->m_scan_count[1]);
    tabPtr.p->m_scan_count[1]--;
  }
  NdbMutex_Unlock(&tabPtr.p->theMutex);
}

bool
Dbdih::prepare_add_table(TabRecordPtr tabPtr,
                         ConnectRecordPtr connectPtr,
                         Signal *signal)
{
  DiAddTabReq * const req = (DiAddTabReq*)signal->getDataPtr();
  D("prepare_add_table tableId = " << tabPtr.i << " primaryTableId: " <<
    req->primaryTableId);

  NdbMutex_Lock(&tabPtr.p->theMutex);
  tabPtr.p->connectrec = connectPtr.i;
  tabPtr.p->tableType = req->tableType;
  tabPtr.p->schemaVersion = req->schemaVersion;
  tabPtr.p->primaryTableId = req->primaryTableId;
  tabPtr.p->schemaTransId = req->schemaTransId;
  tabPtr.p->m_scan_count[0] = 0;
  tabPtr.p->m_scan_count[1] = 0;
  tabPtr.p->m_scan_reorg_flag = 0;
  tabPtr.p->m_flags = 0;

  if (req->fullyReplicated)
  {
    jam();
    tabPtr.p->m_flags |= TabRecord::TF_FULLY_REPLICATED;
    tabPtr.p->partitionCount = req->partitionCount;
    D("fully replicated, partitionCount = " <<
      tabPtr.p->partitionCount);
  }
  else if (req->primaryTableId != RNIL)
  {
    jam();
    TabRecordPtr primTabPtr;
    primTabPtr.i = req->primaryTableId;
    ptrCheckGuard(primTabPtr, ctabFileSize, tabRecord);
    tabPtr.p->m_flags |= (primTabPtr.p->m_flags&TabRecord::TF_FULLY_REPLICATED);
    tabPtr.p->partitionCount = primTabPtr.p->partitionCount;
    D("Non-primary, m_flags: " << tabPtr.p->m_flags <<
      " partitionCount: " << tabPtr.p->partitionCount);
  }
  else
  {
    jam();
    tabPtr.p->partitionCount = req->partitionCount;
  }

  if (tabPtr.p->tabStatus == TabRecord::TS_ACTIVE)
  {
    /**
     * This is the only code segment in DBDIH where we can change tabStatus
     * while DBTC also has access to the table. It can conflict with the
     * call to execDIH_SCAN_TAB_REQ from DBTC. So we need to protect this
     * particular segment of the this call.
     */
    jam();
    tabPtr.p->tabStatus = TabRecord::TS_CREATING;
    NdbMutex_Unlock(&tabPtr.p->theMutex);
    connectPtr.p->m_alter.m_totalfragments = tabPtr.p->totalfragments;
    sendAddFragreq(signal, connectPtr, tabPtr, 0, false);
    return true;
  }
  NdbMutex_Unlock(&tabPtr.p->theMutex);
  return false;
}

void
Dbdih::commit_new_table(TabRecordPtr tabPtr)
{
  /**
   * Normally this signal arrives as part of CREATE TABLE and then
   * DBTC haven't been informed of the table being available yet
   * and no protection is needed. It is however also used for
   * Table reorganisation and in that case the table is fully
   * available to DBTC and we need to protect the change here
   * to ensure that DIH_SCAN_TAB_REQ sees a correct view of
   * these variables.
   */
  D("commit_new_table: tableId = " << tabPtr.i);
  NdbMutex_Lock(&tabPtr.p->theMutex);
  tabPtr.p->tabStatus = TabRecord::TS_ACTIVE;
  tabPtr.p->schemaTransId = 0;
  NdbMutex_Unlock(&tabPtr.p->theMutex);
}

/**
 * start_add_fragments_in_new_table is called during prepare phase of
 * an ALTER TABLE reorg. It sets up new data structures for the new
 * fragments and starts up the calling of those to actually create
 * the new fragments. The only reason this method is protected is
 * because it touches some of the data structures used to get table
 * distribution.
 */
void
Dbdih::start_add_fragments_in_new_table(TabRecordPtr tabPtr,
                                        ConnectRecordPtr connectPtr,
                                        const Uint16 buf[],
                                        Signal *signal)
{
  /**
   * We need to protect these changes to the node and fragment view of
   * the table since DBTC can see the table through these changes
   * and thus both the mutex and the RCU mechanism is required here to
   * ensure that DBTC sees a consistent view of the data.
   */
  D("start_add_fragments_in_new_table: tableId = " << tabPtr.i);
  Uint32 err;
  NdbMutex_Lock(&tabPtr.p->theMutex);
  DIH_TAB_WRITE_LOCK(tabPtr.p);

  Uint32 save = tabPtr.p->totalfragments;
  if ((err = add_fragments_to_table(tabPtr, buf)))
  {
    jam();
    DIH_TAB_WRITE_UNLOCK(tabPtr.p);
    NdbMutex_Unlock(&tabPtr.p->theMutex);
    ndbrequire(tabPtr.p->totalfragments == save);
    ndbrequire(connectPtr.p->m_alter.m_org_totalfragments == save);
    send_alter_tab_ref(signal, tabPtr, connectPtr, err);

    ndbrequire(tabPtr.p->connectrec == connectPtr.i);
    tabPtr.p->connectrec = RNIL;
    release_connect(connectPtr);
    return;
  }

  tabPtr.p->tabCopyStatus = TabRecord::CS_ALTER_TABLE;
  connectPtr.p->m_alter.m_totalfragments = tabPtr.p->totalfragments;
  if ((tabPtr.p->m_flags & TabRecord::TF_FULLY_REPLICATED) == 0)
  {
    jam();
    connectPtr.p->m_alter.m_partitionCount = tabPtr.p->totalfragments;
  }
  /* Don't make the new fragments available just yet. */
  tabPtr.p->totalfragments = save;
  NdbMutex_Unlock(&tabPtr.p->theMutex);

  sendAddFragreq(signal,
                 connectPtr,
                 tabPtr,
                 connectPtr.p->m_alter.m_org_totalfragments,
                 true);

  DIH_TAB_WRITE_UNLOCK(tabPtr.p);
  return;
}

/**
 * make_new_table_writeable starts off the copy phase. From here on the
 * copy triggers for reorg is activated. The new hash map is installed.
 * The new copy fragments are installed for fully replicated tables to
 * ensure that they are replicated to during each update of rows in the
 * fully replicated table.
 *
 * The new fragments are still not readable, they are only writeable. This
 * is secured by not changing totalfragments.
 */
void
Dbdih::make_new_table_writeable(TabRecordPtr tabPtr,
                                ConnectRecordPtr connectPtr,
                                bool rcu_lock_held)
{
  D("make_new_table_writeable: tableId = " << tabPtr.i);
  if (!rcu_lock_held)
  {
    jam();
    DIH_TAB_WRITE_LOCK(tabPtr.p);
  }
  /**
   * At this point the new table fragments must be updated at proper times.
   * For tables without full replication this simply means setting the
   * value of the new_map_ptr_i referring to the new hash map. This hash
   * map will be used to point to new fragments for some rows.
   *
   * For fully replicated tables we must insert the new fragments into
   * list of copy fragments. These will still not be seen by readers
   * since we never return a fragment id larger than the totalfragments
   * variable.
   */
  if ((tabPtr.p->m_flags & TabRecord::TF_FULLY_REPLICATED) != 0 &&
       tabPtr.p->totalfragments <
       connectPtr.p->m_alter.m_totalfragments)
  {
    for (Uint32 i = tabPtr.p->totalfragments;
         i < connectPtr.p->m_alter.m_totalfragments;
         i++)
    {
      jam();
      FragmentstorePtr fragPtr;
      getFragstore(tabPtr.p, i, fragPtr);
      insertCopyFragmentList(tabPtr.p, fragPtr.p, i);
    }
  }
  mb();
  tabPtr.p->m_new_map_ptr_i = connectPtr.p->m_alter.m_new_map_ptr_i;
  if (!rcu_lock_held)
  {
    DIH_TAB_WRITE_UNLOCK(tabPtr.p);
    jam();
  }
}

/**
 * make_new_table_read_and_writeable
 * ---------------------------------
 * Here we need to protect both using the table mutex and the RCU
 * mechanism. We want DIH_SCAN_TAB_REQ to see a correct combination
 * of those variables as protected by the mutex and we want
 * DIGETNODESREQ to see a protected and consistent view of its variables.
 *
 * At this point for an ALTER TABLE reorg we have completed copying the
 * data, so the new table distribution is completely ok to use. We thus
 * change the totalfragments to make the new fragments available for
 * both read and write.
 * We swap in the new hash map (so far only hash-map tables have support
 * for on-line table reorg), the old still exists for a while more.
 *
 * At this point we need to start waiting for old scans using the old
 * number of fragments to complete.
*/
void
Dbdih::make_new_table_read_and_writeable(TabRecordPtr tabPtr,
                                         ConnectRecordPtr connectPtr,
                                         Signal *signal)
{
  jam();
  D("make_new_table_read_and_writeable tableId: " << tabPtr.i);
  NdbMutex_Lock(&tabPtr.p->theMutex);
  DIH_TAB_WRITE_LOCK(tabPtr.p);
  tabPtr.p->totalfragments = connectPtr.p->m_alter.m_totalfragments;
  tabPtr.p->partitionCount = connectPtr.p->m_alter.m_partitionCount;
  if (AlterTableReq::getReorgFragFlag(connectPtr.p->m_alter.m_changeMask))
  {
    jam();
    Uint32 save = tabPtr.p->m_map_ptr_i;
    tabPtr.p->m_map_ptr_i = tabPtr.p->m_new_map_ptr_i;
    tabPtr.p->m_new_map_ptr_i = save;

    for (Uint32 i = 0; i<tabPtr.p->totalfragments; i++)
    {
      jam();
      FragmentstorePtr fragPtr;
      getFragstore(tabPtr.p, i, fragPtr);
      fragPtr.p->distributionKey = (fragPtr.p->distributionKey + 1) & 0xFF;
    }
    DIH_TAB_WRITE_UNLOCK(tabPtr.p);

    /* These variables are only protected by mutex. */
    ndbassert(tabPtr.p->m_scan_count[1] == 0);
    tabPtr.p->m_scan_count[1] = tabPtr.p->m_scan_count[0];
    tabPtr.p->m_scan_count[0] = 0;
    tabPtr.p->m_scan_reorg_flag = 1;
    NdbMutex_Unlock(&tabPtr.p->theMutex);

    send_alter_tab_conf(signal, connectPtr);
    return;
  }

  DIH_TAB_WRITE_UNLOCK(tabPtr.p);
  NdbMutex_Unlock(&tabPtr.p->theMutex);
  send_alter_tab_conf(signal, connectPtr);
  ndbrequire(tabPtr.p->connectrec == connectPtr.i);
  tabPtr.p->connectrec = RNIL;
  release_connect(connectPtr);
}

/**
 * We need to ensure that all scans after this signal sees
 * the new m_scan_reorg_flag to ensure that we don't have
 * races where scans use this flag in an incorrect manner.
 * It is protected by mutex, so requires a mutex protecting
 * it, m_new_map_ptr_i is only protected by the RCU mechanism
 * and not by the mutex.
 *
 * At this point the ALTER TABLE is completed and any old scans
 * using the old table distribution is completed and we can
 * drop the old hash map.
 */
bool
Dbdih::make_old_table_non_writeable(TabRecordPtr tabPtr,
                                    ConnectRecordPtr connectPtr)
{
  bool wait_flag = false;
  D("make_old_table_non_writeable: tableId = " << tabPtr.i);
  NdbMutex_Lock(&tabPtr.p->theMutex);
  DIH_TAB_WRITE_LOCK(tabPtr.p);
  tabPtr.p->m_new_map_ptr_i = RNIL;
  tabPtr.p->m_scan_reorg_flag = 0;
  if (AlterTableReq::getReorgFragFlag(connectPtr.p->m_alter.m_changeMask))
  {
    /**
     * To ensure that we don't have any outstanding scans with
     * REORG_NOT_MOVED flag set we also start waiting for those
     * scans to complete here.
     */
    ndbassert(tabPtr.p->m_scan_count[1] == 0);
    tabPtr.p->m_scan_count[1] = tabPtr.p->m_scan_count[0];
    tabPtr.p->m_scan_count[0] = 0;
    wait_flag = true;
  }
  DIH_TAB_WRITE_UNLOCK(tabPtr.p);
  NdbMutex_Unlock(&tabPtr.p->theMutex);

  ndbrequire(tabPtr.p->connectrec == connectPtr.i);
  tabPtr.p->connectrec = RNIL;
  release_connect(connectPtr);
  return wait_flag;
}

/**
 * During node recovery a replica is first installed as
 * a new writeable replica. Then when committing this
 * the fragment replica is also readable.
 */
void
Dbdih::make_table_use_new_replica(TabRecordPtr tabPtr,
                                  FragmentstorePtr fragPtr,
                                  ReplicaRecordPtr replicaPtr,
                                  Uint32 replicaType,
                                  Uint32 destNodeId)
{
  D("make_table_use_new_replica: tableId: " << tabPtr.i <<
    " fragId = " << fragPtr.p->fragId <<
    " replicaType = " << replicaType <<
    " destNodeId = " << destNodeId);

  DIH_TAB_WRITE_LOCK(tabPtr.p);
  switch (replicaType) {
  case UpdateFragStateReq::STORED:
    jam();
    CRASH_INSERTION(7138);
    /* ----------------------------------------------------------------------*/
    /*  HERE WE ARE INSERTING THE NEW BACKUP NODE IN THE EXECUTION OF ALL    */
    /*  OPERATIONS. FROM HERE ON ALL OPERATIONS ON THIS FRAGMENT WILL INCLUDE*/
    /*  USE OF THE NEW REPLICA.                                              */
    /* --------------------------------------------------------------------- */
    insertBackup(fragPtr, destNodeId);
    
    fragPtr.p->distributionKey++;
    fragPtr.p->distributionKey &= 255;
    break;
  case UpdateFragStateReq::COMMIT_STORED:
    jam();
    CRASH_INSERTION(7139);
    /* ----------------------------------------------------------------------*/
    /*  HERE WE ARE MOVING THE REPLICA TO THE STORED SECTION SINCE IT IS NOW */
    /*  FULLY LOADED WITH ALL DATA NEEDED.                                   */
    // We also update the order of the replicas here so that if the new 
    // replica is the desired primary we insert it as primary.
    /* ----------------------------------------------------------------------*/
    removeOldStoredReplica(fragPtr, replicaPtr);
    linkStoredReplica(fragPtr, replicaPtr);
    updateNodeInfo(fragPtr);
    break;
  case UpdateFragStateReq::START_LOGGING:
    jam();
    break;
  default:
    ndbrequire(false);
    break;
  }//switch
  DIH_TAB_WRITE_UNLOCK(tabPtr.p);
}

/**
 * Switch in the new primary replica. This is used to ensure that
 * the primary replicas are balanced over all nodes.
 */
void
Dbdih::make_table_use_new_node_order(TabRecordPtr tabPtr,
                                     FragmentstorePtr fragPtr,
                                     Uint32 numReplicas,
                                     Uint32 *newNodeOrder)
{
  D("make_table_use_new_node_order: tableId = " << tabPtr.i <<
    " fragId = " << fragPtr.p->fragId);

  DIH_TAB_WRITE_LOCK(tabPtr.p);
  for (Uint32 i = 0; i < numReplicas; i++)
  {
    jam();
    ndbrequire(i < MAX_REPLICAS);
    fragPtr.p->activeNodes[i] = newNodeOrder[i];
  }//for
  DIH_TAB_WRITE_UNLOCK(tabPtr.p);
}

/**
 * Remove new hash map during rollback of ALTER TABLE REORG.
 */
void
Dbdih::make_new_table_non_writeable(TabRecordPtr tabPtr)
{
  D("make_new_table_non_writeable: tableId = " << tabPtr.i);
  DIH_TAB_WRITE_LOCK(tabPtr.p);
  tabPtr.p->m_new_map_ptr_i = RNIL;
  DIH_TAB_WRITE_UNLOCK(tabPtr.p);
}

/**
 * Drop fragments as part of rollback of ALTER TABLE REORG.
 */
void
Dbdih::drop_fragments_from_new_table_view(TabRecordPtr tabPtr,
                                          ConnectRecordPtr connectPtr)
{
  D("drop_fragments_from_new_table_view: tableId = " << tabPtr.i);
  Uint32 new_frags = connectPtr.p->m_alter.m_totalfragments;
  Uint32 org_frags = connectPtr.p->m_alter.m_org_totalfragments;

  /**
   * We need to manipulate the table distribution and we want to ensure
   * DBTC sees a consistent view of these changes. We affect both data
   * used by DIGETNODES and DIH_SCAN_TAB_REQ, so both mutex and RCU lock
   * need to be held.
   */
  NdbMutex_Lock(&tabPtr.p->theMutex);
  DIH_TAB_WRITE_LOCK(tabPtr.p);

  tabPtr.p->totalfragments = new_frags;
  for (Uint32 i = new_frags - 1; i >= org_frags; i--)
  {
    jam();
    release_fragment_from_table(tabPtr, i);
  }
  NdbMutex_Unlock(&tabPtr.p->theMutex);
  DIH_TAB_WRITE_UNLOCK(tabPtr.p);
  connectPtr.p->m_alter.m_totalfragments = org_frags;
  D("5: totalfragments = " << org_frags);
}

void 
Dbdih::getFragstore(const TabRecord * tab,      //In parameter
                    Uint32 fragNo,              //In parameter
                    FragmentstorePtr & fragptr) //Out parameter
{
  FragmentstorePtr fragPtr;
  Uint32 TfragstoreFileSize = cfragstoreFileSize;
  Fragmentstore* TfragStore = fragmentstore;
  Uint32 chunkNo = fragNo >> LOG_NO_OF_FRAGS_PER_CHUNK;
  Uint32 chunkIndex = fragNo & (NO_OF_FRAGS_PER_CHUNK - 1);
  fragPtr.i = tab->startFid[chunkNo] + chunkIndex;
  if (likely(chunkNo < NDB_ARRAY_SIZE(tab->startFid))) {
    ptrCheckGuard(fragPtr, TfragstoreFileSize, TfragStore);
    fragptr = fragPtr;
    return;
  }//if
  ndbrequire(false);
}//Dbdih::getFragstore()

/**
 * End of TRANSACTION MODULE
 * -------------------------
 */

/**
 * When this is called DBTC isn't made aware of the table just yet, so no
 * need to protect anything here from DBTC's view.
 */
void Dbdih::allocFragments(Uint32 noOfFragments, TabRecordPtr tabPtr)
{
  FragmentstorePtr fragPtr;
  Uint32 noOfChunks = (noOfFragments + (NO_OF_FRAGS_PER_CHUNK - 1)) >> LOG_NO_OF_FRAGS_PER_CHUNK;
  ndbrequire(cremainingfrags >= noOfFragments);
  Uint32 fragId = 0;
  for (Uint32 i = 0; i < noOfChunks; i++) {
    jam();
    Uint32 baseFrag = cfirstfragstore;
    ndbrequire(i < NDB_ARRAY_SIZE(tabPtr.p->startFid));
    tabPtr.p->startFid[i] = baseFrag;
    fragPtr.i = baseFrag;
    ptrCheckGuard(fragPtr, cfragstoreFileSize, fragmentstore);
    cfirstfragstore = fragPtr.p->nextFragmentChunk;
    cremainingfrags -= NO_OF_FRAGS_PER_CHUNK;
    for (Uint32 j = 0; j < NO_OF_FRAGS_PER_CHUNK; j++) {
      jam();
      fragPtr.i = baseFrag + j;
      ptrCheckGuard(fragPtr, cfragstoreFileSize, fragmentstore);
      initFragstore(fragPtr, fragId);
      fragId++;
    }//if
  }//for
  tabPtr.p->noOfFragChunks = noOfChunks;
}//Dbdih::allocFragments()

/**
 * No need to protect anything from DBTC here, table is in last part
 * of being dropped and has been removed from DBTC's view long time
 * ago.
 */
void Dbdih::releaseFragments(TabRecordPtr tabPtr)
{
  FragmentstorePtr fragPtr;
  for (Uint32 i = 0; i < tabPtr.p->noOfFragChunks; i++) {
    jam();
    ndbrequire(i < NDB_ARRAY_SIZE(tabPtr.p->startFid));
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
    initFragstore(fragPtr, 0);
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

#ifndef NDB_HAVE_RMB
#define rmb() do { } while (0)
#endif

#ifndef NDB_HAVE_WMB
#define wmb() do { } while (0)
#endif

inline
bool
Dbdih::isEmpty(const DIVERIFY_queue & q)
{
  return q.cfirstVerifyQueue == q.clastVerifyQueue;
}

inline
void
Dbdih::enqueue(DIVERIFY_queue & q, Uint32 senderData, Uint64 gci)
{
#ifndef NDEBUG
  /**
   * - assert only
   * - we must read first *before* "publishing last
   *   or else DIH-thread could already have consumed entry
   *   when we call assert
   */
  Uint32 first = q.cfirstVerifyQueue;
#endif

  Uint32 last = q.clastVerifyQueue;
  ApiConnectRecord * apiConnectRecord = q.apiConnectRecord;

  apiConnectRecord[last].senderData = senderData;
  apiConnectRecord[last].apiGci = gci;
  wmb();
  if (last + 1 == capiConnectFileSize)
  {
    q.clastVerifyQueue = 0;
  }
  else
  {
    q.clastVerifyQueue = last + 1;
  }
  assert(q.clastVerifyQueue != first);
}

inline
void
Dbdih::dequeue(DIVERIFY_queue & q, ApiConnectRecord & conRecord)
{
  Uint32 first = q.cfirstVerifyQueue;
  ApiConnectRecord * apiConnectRecord = q.apiConnectRecord;

  rmb();
  conRecord.senderData = apiConnectRecord[first].senderData;
  conRecord.apiGci = apiConnectRecord[first].apiGci;

  if (first + 1 == capiConnectFileSize)
  {
    q.cfirstVerifyQueue = 0;
  }
  else
  {
    q.cfirstVerifyQueue = first + 1;
  }
}

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
  EmulatedJamBuffer * jambuf = * (EmulatedJamBuffer**)(signal->theData+2);
  thrjamEntry(jambuf);
  Uint32 qno = signal->theData[1];
  ndbassert(qno < NDB_ARRAY_SIZE(c_diverify_queue));
  DIVERIFY_queue & q = c_diverify_queue[qno];
loop:
  Uint32 val = m_micro_gcp.m_lock.read_lock();
  Uint32 blocked = getBlockCommit() == true ? 1 : 0;
  if (blocked == 0 && isEmpty(q))
  {
    thrjam(jambuf);
    /*-----------------------------------------------------------------------*/
    // We are not blocked and the verify queue was empty currently so we can
    // simply reply back to TC immediately. The method was called with 
    // EXECUTE_DIRECT so we reply back by setting signal data and returning. 
    // theData[0] already contains the correct information so 
    // we need not touch it.
    /*-----------------------------------------------------------------------*/
    signal->theData[1] = (Uint32)(m_micro_gcp.m_current_gci >> 32);
    signal->theData[2] = (Uint32)(m_micro_gcp.m_current_gci & 0xFFFFFFFF);
    signal->theData[3] = 0;
    if (unlikely(! m_micro_gcp.m_lock.read_unlock(val)))
      goto loop;
    return;
  }//if
  /*-------------------------------------------------------------------------*/
  // Since we are blocked we need to put this operation last in the verify
  // queue to ensure that operation starts up in the correct order.
  /*-------------------------------------------------------------------------*/
  enqueue(q, signal->theData[0], m_micro_gcp.m_new_gci);
  if (blocked == 0 && jambuf == jamBuffer())
  {
    emptyverificbuffer(signal, 0, false);
  }
  signal->theData[3] = blocked + 1; // Indicate no immediate return
  return;
}//Dbdih::execDIVERIFYREQ()

void Dbdih::execDIH_SCAN_TAB_REQ(Signal* signal)
{
  DihScanTabReq * req = (DihScanTabReq*)signal->getDataPtr();
  EmulatedJamBuffer * jambuf = (EmulatedJamBuffer*)req->jamBufferPtr;

  thrjamEntry(jambuf);

  TabRecordPtr tabPtr;
  tabPtr.i = req->tableId;
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);

  start_scan_on_table(tabPtr, signal, req->schemaTransId, jambuf);
  return;
}//Dbdih::execDIH_SCAN_TAB_REQ()

void
Dbdih::execDIH_SCAN_TAB_COMPLETE_REP(Signal* signal)
{
  DihScanTabCompleteRep* rep = (DihScanTabCompleteRep*)signal->getDataPtr();
  EmulatedJamBuffer * jambuf = (EmulatedJamBuffer*)rep->jamBufferPtr;

  thrjamEntry(jambuf);

  TabRecordPtr tabPtr;
  tabPtr.i = rep->tableId;
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);

  complete_scan_on_table(tabPtr, rep->scanCookie, jambuf);
}


/****************************************************************************/
/* **********     GLOBAL-CHECK-POINT HANDLING  MODULE           *************/
/****************************************************************************/
/*
  3.10   G L O B A L  C H E C K P O I N T ( IN  M A S T E R  R O L E)
  *******************************************************************
  */

bool
Dbdih::check_enable_micro_gcp(Signal* signal, bool broadcast)
{
  ndbassert(m_micro_gcp.m_enabled == false);
  ndbassert(NodeVersionInfo::DataLength == 6);
  Uint32 min = ~(Uint32)0;
  const NodeVersionInfo& info = getNodeVersionInfo();
  for (Uint32 i = 0; i<3; i++)
  {
    Uint32 tmp = info.m_type[i].m_min_version;
    if (tmp)
    {
      min = (min < tmp) ? min : tmp; 
    }
  }

  if (ndb_check_micro_gcp(min))
  {
    jam();
    m_micro_gcp.m_enabled = true;

    infoEvent("Enabling micro GCP");
    if (broadcast)
    {
      jam();
      UpgradeProtocolOrd * ord = (UpgradeProtocolOrd*)signal->getDataPtrSend();
      ord->type = UpgradeProtocolOrd::UPO_ENABLE_MICRO_GCP;

      /**
       * We need to notify all ndbd's or they'll get confused!
       */
      NodeRecordPtr specNodePtr;
      specNodePtr.i = cfirstAliveNode;
      do {
        jam();
        ptrCheckGuard(specNodePtr, MAX_NDB_NODES, nodeRecord);
        sendSignal(calcDihBlockRef(specNodePtr.i), GSN_UPGRADE_PROTOCOL_ORD,
                   signal, UpgradeProtocolOrd::SignalLength, JBA);
        specNodePtr.i = specNodePtr.p->nextNode;
      } while (specNodePtr.i != RNIL);
      EXECUTE_DIRECT(QMGR,GSN_UPGRADE_PROTOCOL_ORD,signal,signal->getLength());
    }
  }
  return m_micro_gcp.m_enabled;
}

void
Dbdih::execUPGRADE_PROTOCOL_ORD(Signal* signal)
{
  const UpgradeProtocolOrd* ord = (UpgradeProtocolOrd*)signal->getDataPtr();
  switch(ord->type){
  case UpgradeProtocolOrd::UPO_ENABLE_MICRO_GCP:
    jam();
    m_micro_gcp.m_enabled = true;
    EXECUTE_DIRECT(QMGR, GSN_UPGRADE_PROTOCOL_ORD,signal, signal->getLength());
    return;
  }
}

void
Dbdih::startGcpLab(Signal* signal)
{
  if (ERROR_INSERTED(7242))
  {
    jam();
    g_eventLogger->info("Delayed GCP_COMMIT start 5s");
    signal->theData[0] = DihContinueB::ZSTART_GCP;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 5000, 1);
    return;
  }

  for (Uint32 i = 0; i < c_diverify_queue_cnt; i++)
  {
    if (c_diverify_queue[i].m_empty_done == 0)
    {
      // Previous global checkpoint is not yet completed.
      jam();
      signal->theData[0] = DihContinueB::ZSTART_GCP;
      sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 10, 1);
      return;
    }
  }

  emptyWaitGCPMasterQueue(signal,
                          m_micro_gcp.m_current_gci,
                          c_waitEpochMasterList);
  
  if (c_nodeStartMaster.blockGcp != 0 &&
      m_gcp_save.m_master.m_state == GcpSave::GCP_SAVE_IDLE)
  {
    jam();

    /* ------------------------------------------------------------------ */
    /*  A NEW NODE WANTS IN AND WE MUST ALLOW IT TO COME IN NOW SINCE THE */
    /*       GCP IS COMPLETED.                                            */
    /* ------------------------------------------------------------------ */

    if (ERROR_INSERTED(7217))
    {
      jam();
      
      signal->theData[0] = 9999;
      sendSignal(numberToRef(CMVMI, refToNode(c_nodeStartMaster.startNode)),
                 GSN_NDB_TAMPER, signal, 1, JBB);
      NdbTick_Invalidate(&m_micro_gcp.m_master.m_start_time); // Force start
      // fall through
    }
    else
    {
      jam();
      ndbrequire(c_nodeStartMaster.blockGcp == 1); // Ordered...
      c_nodeStartMaster.blockGcp = 2; // effective
      gcpBlockedLab(signal);
      return;
    }
  }

  if (cgcpOrderBlocked)
  {
    jam();
    signal->theData[0] = DihContinueB::ZSTART_GCP;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 10, 1);
    return;
  }

  const NDB_TICKS now = c_current_time = NdbTick_getCurrentTicks();

  /**
   * An invalid micro-GCP 'start_time' is used to force
   * a micro GCP to be started immediately.
   */
  if (NdbTick_IsValid(m_micro_gcp.m_master.m_start_time))
  {
    const Uint32 delayMicro = m_micro_gcp.m_enabled ? 
      m_micro_gcp.m_master.m_time_between_gcp : 
      m_gcp_save.m_master.m_time_between_gcp;
    const Uint64 elapsed = 
      NdbTick_Elapsed(m_micro_gcp.m_master.m_start_time, now).milliSec();

    if (elapsed < delayMicro)
    {
      jam();
      signal->theData[0] = DihContinueB::ZSTART_GCP;
      sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 10, 1);
      return;
    }
  }

  m_micro_gcp.m_master.m_start_time = now;
  
  if (m_micro_gcp.m_enabled == false && 
      m_micro_gcp.m_master.m_time_between_gcp)
  {
    /**
     * Micro GCP is disabled...but configured...
     */
    jam();
    check_enable_micro_gcp(signal, true);
  }
  
  /**
   * Check that there has not been more than 2^32 micro GCP wo/ any save
   */
  Uint64 currGCI = m_micro_gcp.m_current_gci;
  ndbrequire(Uint32(currGCI) != ~(Uint32)0);
  m_micro_gcp.m_master.m_new_gci = currGCI + 1;
  
  const Uint32 delaySave = m_gcp_save.m_master.m_time_between_gcp;
  const NDB_TICKS start  = m_gcp_save.m_master.m_start_time;
  const bool need_gcp_save = 
    (!NdbTick_IsValid(start) ||                              //First or forced GCP
     NdbTick_Elapsed(start, now).milliSec() >= delaySave) && //Reached time limit
    (!ERROR_INSERTED(7243));  /* 7243 = no GCP_SAVE initiation */

  if ((m_micro_gcp.m_enabled == false) ||
      (need_gcp_save &&
       m_gcp_save.m_master.m_state == GcpSave::GCP_SAVE_IDLE))
  {
    jam();
    /**
     * Time for save...switch gci_hi
     */
    m_gcp_save.m_master.m_start_time = now;
    m_micro_gcp.m_master.m_new_gci = Uint64((currGCI >> 32) + 1) << 32;

    signal->theData[0] = NDB_LE_GlobalCheckpointStarted; //Event type
    signal->theData[1] = Uint32(currGCI >> 32);
    signal->theData[2] = Uint32(currGCI);
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 3, JBB);
  }
  
  ndbassert(m_micro_gcp.m_enabled || Uint32(m_micro_gcp.m_new_gci) == 0);
  
  
  /***************************************************************************/
  // Report the event that a global checkpoint has started.
  /***************************************************************************/
  
  CRASH_INSERTION(7000);
  m_micro_gcp.m_master.m_state = MicroGcp::M_GCP_PREPARE;
  signal->setTrace(TestOrd::TraceGlobalCheckpoint);

#ifdef ERROR_INSERT
  if (ERROR_INSERTED(7186))
  {
    sendToRandomNodes("GCP_PREPARE",
                      signal, &c_GCP_PREPARE_Counter, &Dbdih::sendGCP_PREPARE);
    signal->theData[0] = 9999;
    sendSignalWithDelay(CMVMI_REF, GSN_NDB_TAMPER, signal, 1000, 1);
    return;
  }
  else if (ERROR_INSERTED(7200))
  {
    c_GCP_PREPARE_Counter.clearWaitingFor();
    NodeRecordPtr nodePtr;
    nodePtr.i = cfirstAliveNode;
    do {
      jam();
      ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
      c_GCP_PREPARE_Counter.setWaitingFor(nodePtr.i);
      if (nodePtr.i != getOwnNodeId())
      {
        SET_ERROR_INSERT_VALUE(7201);
        sendGCP_PREPARE(signal, nodePtr.i, RNIL);
      }
      else
      {
        SET_ERROR_INSERT_VALUE(7202);
        sendGCP_PREPARE(signal, nodePtr.i, RNIL);
      }
      nodePtr.i = nodePtr.p->nextNode;
    } while (nodePtr.i != RNIL);

    NodeReceiverGroup rg(CMVMI, c_GCP_PREPARE_Counter);
    rg.m_nodes.clear(getOwnNodeId());
    Uint32 victim = rg.m_nodes.find(0);
    
    signal->theData[0] = 9999;
    sendSignal(numberToRef(CMVMI, victim),
	       GSN_NDB_TAMPER, signal, 1, JBA);

    CLEAR_ERROR_INSERT_VALUE;
    return;
  }
  else if (ERROR_INSERTED(7227))
  {
    ndbout_c("Not sending GCP_PREPARE to %u", c_error_insert_extra);
    c_GCP_PREPARE_Counter.clearWaitingFor();
    NodeRecordPtr nodePtr;
    nodePtr.i = cfirstAliveNode;
    do {
      jam();
      ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
      c_GCP_PREPARE_Counter.setWaitingFor(nodePtr.i);
      if (nodePtr.i != c_error_insert_extra)
      {
        sendGCP_PREPARE(signal, nodePtr.i, RNIL);
      }
      nodePtr.i = nodePtr.p->nextNode;
    } while (nodePtr.i != RNIL);

    signal->theData[0] = 9999;
    sendSignalWithDelay(CMVMI_REF, GSN_NDB_TAMPER, signal, 200, 1);
    return;
  }
#endif

  sendLoopMacro(GCP_PREPARE, sendGCP_PREPARE, RNIL);
}//Dbdih::startGcpLab()

void Dbdih::execGCP_PREPARECONF(Signal* signal)
{
  jamEntry();
  Uint32 senderNodeId = signal->theData[0];
  Uint32 gci_hi = signal->theData[1];
  Uint32 gci_lo = signal->theData[2];

  if (unlikely(signal->getLength() < GCPPrepareConf::SignalLength))
  {
    gci_lo = 0;
    ndbassert(!ndb_check_micro_gcp(getNodeInfo(senderNodeId).m_version));
  }

  Uint64 gci = gci_lo | (Uint64(gci_hi) << 32);
  ndbrequire(gci == m_micro_gcp.m_master.m_new_gci);
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

  m_micro_gcp.m_master.m_state = MicroGcp::M_GCP_COMMIT;

#ifdef ERROR_INSERT
  if (ERROR_INSERTED(7187))
  {
    sendToRandomNodes("GCP_COMMIT",
                      signal, &c_GCP_COMMIT_Counter, &Dbdih::sendGCP_COMMIT);
    signal->theData[0] = 9999;
    sendSignalWithDelay(CMVMI_REF, GSN_NDB_TAMPER, signal, 1000, 1);
    return;
  }
#endif

  sendLoopMacro(GCP_COMMIT, sendGCP_COMMIT, RNIL);
  return;
}//Dbdih::gcpcommitreqLab()

void Dbdih::execGCP_NODEFINISH(Signal* signal)
{
  jamEntry();
  const Uint32 senderNodeId = signal->theData[0];
  const Uint32 gci_hi = signal->theData[1];
  const Uint32 tcFailNo = signal->theData[2];
  const Uint32 gci_lo = signal->theData[3];
  const Uint64 gci = gci_lo | (Uint64(gci_hi) << 32);

  /* Check that there has not been a node failure since TC
   * reported this GCP complete...
   */
  if ((senderNodeId == getOwnNodeId()) &&
      (tcFailNo < cMinTcFailNo))
  {
    jam();
    ndbrequire(c_GCP_COMMIT_Counter.isWaitingFor(getOwnNodeId()));
    
    /* We are master, and the local TC will takeover the transactions
     * of the failed node, which can add to the current GCP, so resend
     * GCP_NOMORETRANS to TC...
     */
    m_micro_gcp.m_state = MicroGcp::M_GCP_COMMIT; /* Reset DIH Slave GCP state */
    
    GCPNoMoreTrans* req = (GCPNoMoreTrans*)signal->getDataPtrSend();
    req->senderRef = reference();
    req->senderData = m_micro_gcp.m_master_ref;
    req->gci_hi = Uint32(m_micro_gcp.m_old_gci >> 32);
    req->gci_lo = Uint32(m_micro_gcp.m_old_gci & 0xFFFFFFFF);
    sendSignal(clocaltcblockref, GSN_GCP_NOMORETRANS, signal,
               GCPNoMoreTrans::SignalLength, JBB);

    return;
  }
  (void)gci; // TODO validate

  ndbrequire(m_micro_gcp.m_master.m_state == MicroGcp::M_GCP_COMMIT);
  receiveLoopMacro(GCP_COMMIT, senderNodeId);

  jam();
  
  if (m_micro_gcp.m_enabled)
  {
    jam();

    m_micro_gcp.m_master.m_state = MicroGcp::M_GCP_COMPLETE;

    SubGcpCompleteRep * rep = (SubGcpCompleteRep*)signal->getDataPtr();
    rep->senderRef = reference();
    rep->gci_hi = (Uint32)(m_micro_gcp.m_old_gci >> 32);
    rep->gci_lo = (Uint32)(m_micro_gcp.m_old_gci & 0xFFFFFFFF);
    rep->flags = SubGcpCompleteRep::IN_MEMORY;
    
#ifdef ERROR_INSERT
    if (ERROR_INSERTED(7190))
    {
      sendToRandomNodes("GCP_COMPLETE_REP", signal,
                        &c_SUB_GCP_COMPLETE_REP_Counter,
                        &Dbdih::sendSUB_GCP_COMPLETE_REP);
      signal->theData[0] = 9999;
      sendSignalWithDelay(CMVMI_REF, GSN_NDB_TAMPER, signal, 1000, 1);
    }
    else if (ERROR_INSERTED(7226))
    {
      ndbout_c("Not sending SUB_GCP_COMPLETE_REP to %u", c_error_insert_extra);
      c_SUB_GCP_COMPLETE_REP_Counter.clearWaitingFor();
      NodeRecordPtr nodePtr;
      nodePtr.i = cfirstAliveNode;
      do {
        jam();
        ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
        c_SUB_GCP_COMPLETE_REP_Counter.setWaitingFor(nodePtr.i);
        if (nodePtr.i != c_error_insert_extra)
        {
          sendSignal(calcDihBlockRef(nodePtr.i), GSN_SUB_GCP_COMPLETE_REP,
                     signal, SubGcpCompleteRep::SignalLength, JBA);
        }
        nodePtr.i = nodePtr.p->nextNode;
      } while (nodePtr.i != RNIL);
      SET_ERROR_INSERT_VALUE(7227);

      signal->theData[0] = 9999;
      sendSignalWithDelay(CMVMI_REF, GSN_NDB_TAMPER, signal, 200, 1);
    }
    else
#endif
    {
      jam();
      // Normal path...
      sendLoopMacro(SUB_GCP_COMPLETE_REP, sendSUB_GCP_COMPLETE_REP, RNIL);
    }
  }

  //-------------------------------------------------------------
  // We have now received all replies. We are ready to continue
  // with saving the global checkpoint to disk.
  //-------------------------------------------------------------
  CRASH_INSERTION(7002);

  Uint32 curr_hi = (Uint32)(m_micro_gcp.m_current_gci >> 32);
  Uint32 old_hi = (Uint32)(m_micro_gcp.m_old_gci >> 32);
  
  if (m_micro_gcp.m_enabled)
  {
    jam();
  }
  else
  {
    ndbrequire(curr_hi != old_hi);
  }
  
  if (curr_hi == old_hi)
  {
    jam();
    return;
  }

  /**
   * Start a save
   */
  Uint32 saveGCI = old_hi;
  m_gcp_save.m_master.m_state = GcpSave::GCP_SAVE_REQ;
  m_gcp_save.m_master.m_new_gci = saveGCI;
  
#ifdef ERROR_INSERT
  if (ERROR_INSERTED(7188))
  {
    sendToRandomNodes("GCP_SAVE",
                      signal, &c_GCP_SAVEREQ_Counter, &Dbdih::sendGCP_SAVEREQ);
    signal->theData[0] = 9999;
    sendSignalWithDelay(CMVMI_REF, GSN_NDB_TAMPER, signal, 1000, 1);
    return;
  }
  else if (ERROR_INSERTED(7216))
  {
    infoEvent("GCP_SAVE all/%u", c_error_insert_extra);
    NodeRecordPtr nodePtr;
    nodePtr.i = c_error_insert_extra;
    ptrAss(nodePtr, nodeRecord);

    removeAlive(nodePtr);
    sendLoopMacro(GCP_SAVEREQ, sendGCP_SAVEREQ, RNIL);
    insertAlive(nodePtr);
    signal->theData[0] = 9999;
    sendSignalWithDelay(CMVMI_REF, GSN_NDB_TAMPER, signal, 1000, 1);
    c_GCP_SAVEREQ_Counter.setWaitingFor(c_error_insert_extra);
    return;
  }
#endif
  
  sendLoopMacro(GCP_SAVEREQ, sendGCP_SAVEREQ, RNIL);
}

void
Dbdih::execSUB_GCP_COMPLETE_ACK(Signal* signal)
{
  jamEntry();
  SubGcpCompleteAck ack = * CAST_CONSTPTR(SubGcpCompleteAck,
                                          signal->getDataPtr());
  Uint32 senderNodeId = refToNode(ack.rep.senderRef);

  ndbrequire(m_micro_gcp.m_master.m_state == MicroGcp::M_GCP_COMPLETE);
  receiveLoopMacro(SUB_GCP_COMPLETE_REP, senderNodeId);

  m_micro_gcp.m_master.m_state = MicroGcp::M_GCP_IDLE;

  if (!ERROR_INSERTED(7190))
  {
    signal->theData[0] = DihContinueB::ZSTART_GCP;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 10, 1);
  }
}

void
Dbdih::execGCP_SAVEREQ(Signal* signal)
{
  jamEntry();
  GCPSaveReq * req = (GCPSaveReq*)&signal->theData[0];

  if (ERROR_INSERTED(7237))
  {
    jam();
    g_eventLogger->info("Delayed GCP_SAVEREQ 5s");
    sendSignalWithDelay(reference(), GSN_GCP_SAVEREQ,
                        signal, 5000,
                        signal->getLength());
    return;
  }

  if (m_gcp_save.m_state == GcpSave::GCP_SAVE_REQ)
  {
    jam();
    /**
     * This is master take over...
     * and SAVE_REQ is already running
     */
    ndbrequire(m_gcp_save.m_gci == req->gci);
    m_gcp_save.m_master_ref = req->dihBlockRef;
    return;
  }

  if (m_gcp_save.m_gci == req->gci)
  {
    jam();
    /**
     * This is master take over...
     * and SAVE_REQ is complete...
     */
    m_gcp_save.m_master_ref = req->dihBlockRef;

    GCPSaveReq save = (* req);
    GCPSaveConf * conf = (GCPSaveConf*)signal->getDataPtrSend();
    conf->dihPtr = save.dihPtr;
    conf->nodeId = getOwnNodeId();
    conf->gci    = save.gci;
    sendSignal(m_gcp_save.m_master_ref, GSN_GCP_SAVECONF, signal,
               GCPSaveConf::SignalLength, JBA);
    return;
  }

  ndbrequire(m_gcp_save.m_state == GcpSave::GCP_SAVE_IDLE);
  m_gcp_save.m_state = GcpSave::GCP_SAVE_REQ;
  m_gcp_save.m_master_ref = req->dihBlockRef;
  m_gcp_save.m_gci = req->gci;

  req->dihBlockRef = reference();
  sendSignal(DBLQH_REF, GSN_GCP_SAVEREQ, signal, signal->getLength(), JBA);
}

void Dbdih::execGCP_SAVECONF(Signal* signal) 
{
  jamEntry();  
  GCPSaveConf * saveConf = (GCPSaveConf*)&signal->theData[0];

  if (refToBlock(signal->getSendersBlockRef()) == DBLQH)
  {
    jam();

    ndbrequire(m_gcp_save.m_state == GcpSave::GCP_SAVE_REQ);
    m_gcp_save.m_state = GcpSave::GCP_SAVE_CONF;

    sendSignal(m_gcp_save.m_master_ref,
               GSN_GCP_SAVECONF, signal, signal->getLength(), JBA);
    return;
  }

  ndbrequire(saveConf->gci == m_gcp_save.m_master.m_new_gci);
  ndbrequire(saveConf->nodeId == saveConf->dihPtr);
  SYSFILE->lastCompletedGCI[saveConf->nodeId] = saveConf->gci;  
  GCP_SAVEhandling(signal, saveConf->nodeId);
}//Dbdih::execGCP_SAVECONF()

void Dbdih::execGCP_SAVEREF(Signal* signal) 
{
  jamEntry();
  GCPSaveRef * const saveRef = (GCPSaveRef*)&signal->theData[0];

  if (refToBlock(signal->getSendersBlockRef()) == DBLQH)
  {
    jam();

    ndbrequire(m_gcp_save.m_state == GcpSave::GCP_SAVE_REQ);
    m_gcp_save.m_state = GcpSave::GCP_SAVE_CONF;

    sendSignal(m_gcp_save.m_master_ref,
               GSN_GCP_SAVEREF, signal, signal->getLength(), JBA);
    return;
  }

  ndbrequire(saveRef->gci == m_gcp_save.m_master.m_new_gci);
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
  ndbrequire(m_gcp_save.m_master.m_state == GcpSave::GCP_SAVE_REQ);
  receiveLoopMacro(GCP_SAVEREQ, nodeId);
  /*-------------------------------------------------------------------------*/
  // All nodes have replied. We are ready to update the system file.
  /*-------------------------------------------------------------------------*/

  CRASH_INSERTION(7003);
  /**------------------------------------------------------------------------
   * SET NEW RECOVERABLE GCI. ALSO RESET RESTART COUNTER TO ZERO. 
   * THIS INDICATES THAT THE SYSTEM HAS BEEN RECOVERED AND SURVIVED AT 
   * LEAST ONE GLOBAL CHECKPOINT PERIOD. WE WILL USE THIS PARAMETER TO 
   * SET BACK THE RESTART GCI IF WE ENCOUNTER MORE THAN ONE UNSUCCESSFUL 
   * RESTART.
   *------------------------------------------------------------------------*/
  SYSFILE->newestRestorableGCI = m_gcp_save.m_gci;
  if(Sysfile::getInitialStartOngoing(SYSFILE->systemRestartBits) &&
     getNodeState().startLevel == NodeState::SL_STARTED){
    jam();
#if 0
    g_eventLogger->info("Dbdih: Clearing initial start ongoing");
#endif
    Sysfile::clearInitialStartOngoing(SYSFILE->systemRestartBits);
  }
  copyGciLab(signal, CopyGCIReq::GLOBAL_CHECKPOINT);

  m_gcp_save.m_master.m_state = GcpSave::GCP_SAVE_COPY_GCI;

}//Dbdih::GCP_SAVEhandling()

/*
  3.11   G L O B A L  C H E C K P O I N T (N O T - M A S T E R)
  *************************************************************
  */
void Dbdih::execGCP_PREPARE(Signal* signal) 
{
  jamEntry();
  CRASH_INSERTION(7005);

  if (ERROR_INSERTED(7030))
  {
    cgckptflag = true;
  }
  if (ERROR_INSERTED(7030) || 
      ERROR_INSERTED(7238))
  {
    g_eventLogger->info("Delayed GCP_PREPARE 5s");
    sendSignalWithDelay(reference(), GSN_GCP_PREPARE, signal, 5000,
			signal->getLength());
    return;
  }
  
  GCPPrepare* req = (GCPPrepare*)signal->getDataPtr();
  GCPPrepareConf * conf = (GCPPrepareConf*)signal->getDataPtrSend();
  Uint32 masterNodeId = req->nodeId;
  Uint32 gci_hi = req->gci_hi;
  Uint32 gci_lo = req->gci_lo;
  if (unlikely(signal->getLength() < GCPPrepare::SignalLength))
  {
    jam();
    gci_lo = 0;
    ndbassert(!ndb_check_micro_gcp(getNodeInfo(masterNodeId).m_version));
  }
  Uint64 gci = gci_lo | (Uint64(gci_hi) << 32);

  BlockReference retRef = calcDihBlockRef(masterNodeId);

  if (isMaster())
  {
    ndbrequire(m_micro_gcp.m_master.m_state == MicroGcp::M_GCP_PREPARE);
  }

  if (m_micro_gcp.m_state == MicroGcp::M_GCP_PREPARE)
  {
    jam();
    /**
     * This must be master take over
     *   Prepare is already complete
     */
    ndbrequire(m_micro_gcp.m_new_gci == gci);
    m_micro_gcp.m_master_ref = retRef;
    goto reply;
  }

  if (m_micro_gcp.m_new_gci == gci)
  {
    jam();
    /**
     * This GCP has already been prepared...
     *   Must be master takeover
     */
    m_micro_gcp.m_master_ref = retRef;
    goto reply;
  }
  
  ndbrequire(m_micro_gcp.m_state == MicroGcp::M_GCP_IDLE);

  m_micro_gcp.m_lock.write_lock();
  cgckptflag = true;
  m_micro_gcp.m_state = MicroGcp::M_GCP_PREPARE;
  m_micro_gcp.m_new_gci = gci;
  m_micro_gcp.m_master_ref = retRef;
  m_micro_gcp.m_lock.write_unlock();

  if (ERROR_INSERTED(7031))
  {
    g_eventLogger->info("Crashing delayed in GCP_PREPARE 3s");
    signal->theData[0] = 9999;
    sendSignalWithDelay(CMVMI_REF, GSN_NDB_TAMPER, signal, 3000, 1);
    return;
  }
#ifdef GCP_TIMER_HACK
  globalData.gcp_timer_commit[0] = NdbTick_getCurrentTicks();
#endif

reply:
  /**
   * Send the new gci to Suma.
   *
   * To get correct signal order and avoid races, this signal is sent on the
   * same prio as the SUB_GCP_COMPLETE_REP signal sent to SUMA in
   * execSUB_GCP_COMPLETE_REP().
   */
  sendSignal(SUMA_REF, GSN_GCP_PREPARE, signal, signal->length(), JBB);

  /* Send reply. */
  conf->nodeId = cownNodeId;
  conf->gci_hi = gci_hi;
  conf->gci_lo = gci_lo;
  sendSignal(retRef, GSN_GCP_PREPARECONF, signal, 
             GCPPrepareConf::SignalLength, JBA);
  return;
}

void Dbdih::execGCP_COMMIT(Signal* signal) 
{
  jamEntry();
  CRASH_INSERTION(7006);

  if (ERROR_INSERTED(7239))
  {
    g_eventLogger->info("Delayed GCP_COMMIT 5s");
    sendSignalWithDelay(reference(), GSN_GCP_COMMIT, signal, 5000,
                        signal->getLength());
    return;
  }

  GCPCommit * req = (GCPCommit*)signal->getDataPtr();
  Uint32 masterNodeId = req->nodeId;
  Uint32 gci_hi = req->gci_hi;
  Uint32 gci_lo = req->gci_lo;

  if (unlikely(signal->getLength() < GCPCommit::SignalLength))
  {
    gci_lo = 0;
    ndbassert(!ndb_check_micro_gcp(getNodeInfo(masterNodeId).m_version));
  }
  Uint64 gci = gci_lo | (Uint64(gci_hi) << 32);

#ifdef ERROR_INSERT
  if (ERROR_INSERTED(7213))
  {
    ndbout_c("err 7213 killing %d", c_error_insert_extra);
    Uint32 save = signal->theData[0];
    signal->theData[0] = 5048;
    sendSignal(numberToRef(DBLQH, c_error_insert_extra),
               GSN_NDB_TAMPER, signal, 1, JBB);
    signal->theData[0] = save;
    CLEAR_ERROR_INSERT_VALUE;

    signal->theData[0] = 9999;
    sendSignal(numberToRef(CMVMI, c_error_insert_extra),
               GSN_DUMP_STATE_ORD, signal, 1, JBB);

    signal->theData[0] = save;
    CLEAR_ERROR_INSERT_VALUE;

    return;
  }
#endif

  Uint32 masterRef = calcDihBlockRef(masterNodeId);
  ndbrequire(masterNodeId == cmasterNodeId);
  if (isMaster())
  {
    ndbrequire(m_micro_gcp.m_master.m_state == MicroGcp::M_GCP_COMMIT);
  }

  if (m_micro_gcp.m_state == MicroGcp::M_GCP_COMMIT)
  {
    jam();
    /**
     * This must be master take over
     *   Commit is already ongoing...
     */
    ndbrequire(m_micro_gcp.m_current_gci == gci);
    m_micro_gcp.m_master_ref = masterRef;
    return;
  }

  if (m_micro_gcp.m_current_gci == gci)
  {
    jam();
    /**
     * This must be master take over
     *   Commit has already completed
     */
    m_micro_gcp.m_master_ref = masterRef;
    
    GCPNodeFinished* conf = (GCPNodeFinished*)signal->getDataPtrSend();
    conf->nodeId = cownNodeId;
    conf->gci_hi = (Uint32)(m_micro_gcp.m_old_gci >> 32);
    conf->failno = cfailurenr;
    conf->gci_lo = (Uint32)(m_micro_gcp.m_old_gci & 0xFFFFFFFF);
    sendSignal(masterRef, GSN_GCP_NODEFINISH, signal,
               GCPNodeFinished::SignalLength, JBB);
    return;
  }

  ndbrequire(m_micro_gcp.m_new_gci == gci);
  ndbrequire(m_micro_gcp.m_state == MicroGcp::M_GCP_PREPARE);
  m_micro_gcp.m_state = MicroGcp::M_GCP_COMMIT;
  m_micro_gcp.m_master_ref = calcDihBlockRef(masterNodeId);
  
  m_micro_gcp.m_lock.write_lock();
  m_micro_gcp.m_old_gci = m_micro_gcp.m_current_gci;
  m_micro_gcp.m_current_gci = gci;
  cgckptflag = false;
  m_micro_gcp.m_lock.write_unlock();

  for (Uint32 i = 0; i < c_diverify_queue_cnt; i++)
  {
    jam();
    c_diverify_queue[i].m_empty_done = 0;
    emptyverificbuffer(signal, i, true);
  }

  GCPNoMoreTrans* req2 = (GCPNoMoreTrans*)signal->getDataPtrSend();
  req2->senderRef = reference();
  req2->senderData = calcDihBlockRef(masterNodeId);
  req2->gci_hi = (Uint32)(m_micro_gcp.m_old_gci >> 32);
  req2->gci_lo = (Uint32)(m_micro_gcp.m_old_gci & 0xFFFFFFFF);
  sendSignal(clocaltcblockref, GSN_GCP_NOMORETRANS, signal, 
             GCPNoMoreTrans::SignalLength, JBB);
  return;
}//Dbdih::execGCP_COMMIT()

void Dbdih::execGCP_TCFINISHED(Signal* signal) 
{
  jamEntry();
  CRASH_INSERTION(7007);
  GCPTCFinished* conf = (GCPTCFinished*)signal->getDataPtr();
  Uint32 retRef = conf->senderData;
  Uint32 gci_hi = conf->gci_hi;
  Uint32 gci_lo = conf->gci_lo;
  Uint32 tcFailNo = conf->tcFailNo;
  Uint64 gci = gci_lo | (Uint64(gci_hi) << 32);
  ndbrequire(gci == m_micro_gcp.m_old_gci);

  if (ERROR_INSERTED(7181) || ERROR_INSERTED(7182))
  {
    c_error_7181_ref = retRef; // Save ref
    ndbout_c("killing %d", refToNode(cmasterdihref));
    signal->theData[0] = 9999;
    sendSignal(numberToRef(CMVMI, refToNode(cmasterdihref)),
	       GSN_NDB_TAMPER, signal, 1, JBB);
    return;
  }

#ifdef ERROR_INSERT
  if (ERROR_INSERTED(7214))
  {
    ndbout_c("err 7214 killing %d", c_error_insert_extra);
    Uint32 save = signal->theData[0];
    signal->theData[0] = 9999;
    sendSignal(numberToRef(CMVMI, c_error_insert_extra),
               GSN_NDB_TAMPER, signal, 1, JBB);
    signal->theData[0] = save;
    CLEAR_ERROR_INSERT_VALUE;
  }
#endif

#ifdef GCP_TIMER_HACK
  globalData.gcp_timer_commit[1] = NdbTick_getCurrentTicks();
#endif

  ndbrequire(m_micro_gcp.m_state == MicroGcp::M_GCP_COMMIT);

  /**
   * Make sure that each LQH gets scheduled, so that they don't get out of sync
   * wrt to SUB_GCP_COMPLETE_REP
   */
  Callback cb;
  cb.m_callbackData = tcFailNo;  /* Pass fail-no triggering TC_FINISHED to callback */
  cb.m_callbackFunction = safe_cast(&Dbdih::execGCP_TCFINISHED_sync_conf);
  Uint32 path[] = { DBLQH, SUMA, 0 };
  synchronize_path(signal, path, cb);
}//Dbdih::execGCP_TCFINISHED()

void
Dbdih::execGCP_TCFINISHED_sync_conf(Signal* signal, Uint32 cb, Uint32 err)
{
  ndbrequire(m_micro_gcp.m_state == MicroGcp::M_GCP_COMMIT);

  m_micro_gcp.m_state = MicroGcp::M_GCP_COMMITTED;
  Uint32 retRef = m_micro_gcp.m_master_ref;

  GCPNodeFinished* conf2 = (GCPNodeFinished*)signal->getDataPtrSend();
  conf2->nodeId = cownNodeId;
  conf2->gci_hi = (Uint32)(m_micro_gcp.m_old_gci >> 32);
  conf2->failno = cb;  /* tcFailNo */
  conf2->gci_lo = (Uint32)(m_micro_gcp.m_old_gci & 0xFFFFFFFF);
  sendSignal(retRef, GSN_GCP_NODEFINISH, signal, 
             GCPNodeFinished::SignalLength, JBB);
}

void
Dbdih::execSUB_GCP_COMPLETE_REP(Signal* signal)
{
  jamEntry();

  CRASH_INSERTION(7228);

  if (ERROR_INSERTED(7244))
  {
    g_eventLogger->info("Delayed SUB_GCP_COMPLETE_REP 5s");
    sendSignalWithDelay(reference(), GSN_SUB_GCP_COMPLETE_REP, signal, 5000,
                        signal->getLength());
    return;
  }

  SubGcpCompleteRep rep = * (SubGcpCompleteRep*)signal->getDataPtr();
  if (isMaster())
  {
    ndbrequire(m_micro_gcp.m_master.m_state == MicroGcp::M_GCP_COMPLETE);
  }
  
  Uint32 masterRef = rep.senderRef;
  if (m_micro_gcp.m_state == MicroGcp::M_GCP_IDLE)
  {
    jam();
    /**
     * This must be master take over
     *   signal has already arrived
     */
    m_micro_gcp.m_master_ref = masterRef;
    goto reply;
  }

  ndbrequire(m_micro_gcp.m_state == MicroGcp::M_GCP_COMMITTED);
  m_micro_gcp.m_state = MicroGcp::M_GCP_IDLE;

  /**
   * To handle multiple LDM instances, this need to be passed though
   * each LQH...(so that no fire-trig-ord can arrive "too" late)
   */
  sendSignal(DBLQH_REF, GSN_SUB_GCP_COMPLETE_REP, signal,
             signal->length(), JBB);
reply:
  Uint32 nodeId = refToNode(masterRef);
  if (!ndbd_dih_sub_gcp_complete_ack(getNodeInfo(nodeId).m_version))
  {
    jam();
    return;
  }

  SubGcpCompleteAck* ack = CAST_PTR(SubGcpCompleteAck,
                                    signal->getDataPtrSend());
  ack->rep = rep;
  ack->rep.senderRef = reference();
  sendSignal(masterRef, GSN_SUB_GCP_COMPLETE_ACK,
             signal, SubGcpCompleteAck::SignalLength, JBA);
}

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
    if (tuserpointer >= 30000 && tuserpointer < 40000) {
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
    } else if (tuserpointer >= 40000 && tuserpointer < 50000) {
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
  if(c_copyGCIMaster.m_copyReason != CopyGCIReq::IDLE)
  {
    jam();
    /**
     * There can currently only be two waiting
     */
    for (Uint32 i = 0; i<CopyGCIMaster::WAIT_CNT; i++)
    {
      jam();
      if (c_copyGCIMaster.m_waiting[i] == CopyGCIReq::IDLE)
      {
        jam();
        c_copyGCIMaster.m_waiting[i] = reason;
        return;
      }
    }

    /**
     * Code should *not* request more than WAIT_CNT copy-gci's
     *   so this is an internal error
     */
    ndbrequire(false);
    return;
  }
  c_copyGCIMaster.m_copyReason = reason;

#ifdef ERROR_INSERT
  if (reason == CopyGCIReq::GLOBAL_CHECKPOINT && ERROR_INSERTED(7189))
  {
    sendToRandomNodes("COPY_GCI",
                      signal, &c_COPY_GCIREQ_Counter, &Dbdih::sendCOPY_GCIREQ);
    signal->theData[0] = 9999;
    sendSignalWithDelay(CMVMI_REF, GSN_NDB_TAMPER, signal, 1000, 1);
    return;
  }
#endif

  if (reason == CopyGCIReq::RESTART_NR)
  {
    jam();
    if (c_nodeStartMaster.startNode != RNIL)
    {
      jam();
      c_COPY_GCIREQ_Counter.clearWaitingFor();
      c_COPY_GCIREQ_Counter.setWaitingFor(c_nodeStartMaster.startNode);
      sendCOPY_GCIREQ(signal, c_nodeStartMaster.startNode, RNIL);
      return;
    }
    else
    {
      jam();
      reason = c_copyGCIMaster.m_copyReason = c_copyGCIMaster.m_waiting[0];
      for (Uint32 i = 1; i<CopyGCIMaster::WAIT_CNT; i++)
      {
        jam();
        c_copyGCIMaster.m_waiting[i-1] = c_copyGCIMaster.m_waiting[i];
      }
      c_copyGCIMaster.m_waiting[CopyGCIMaster::WAIT_CNT-1] =
        CopyGCIReq::IDLE;

      if (reason == CopyGCIReq::IDLE)
      {
        jam();
        return;
      }
      // fall-through
    }
  }

  sendLoopMacro(COPY_GCIREQ, sendCOPY_GCIREQ, RNIL);

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

  CopyGCIReq::CopyReason current = c_copyGCIMaster.m_copyReason;
  c_copyGCIMaster.m_copyReason = CopyGCIReq::IDLE;

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
  {
    ok = true;
    jam();

    /************************************************************************/
    // Report the event that a global checkpoint has completed.
    /************************************************************************/
    signal->setTrace(0);
    signal->theData[0] = NDB_LE_GlobalCheckpointCompleted; //Event type
    signal->theData[1] = m_gcp_save.m_gci;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);    

    c_newest_restorable_gci = m_gcp_save.m_gci;
#ifdef ERROR_INSERT
    if ((ERROR_INSERTED(7222) || ERROR_INSERTED(7223)) &&
        !Sysfile::getLCPOngoing(SYSFILE->systemRestartBits) &&
        c_newest_restorable_gci >= c_lcpState.lcpStopGcp)
    {
      if (ERROR_INSERTED(7222))
      {
        sendLoopMacro(COPY_TABREQ, nullRoutine, 0);
        NodeReceiverGroup rg(CMVMI, c_COPY_TABREQ_Counter);

        rg.m_nodes.clear(getOwnNodeId());
        if (!rg.m_nodes.isclear())
        {
          signal->theData[0] = 9999;
          sendSignal(rg, GSN_NDB_TAMPER, signal, 1, JBA);
        }
        signal->theData[0] = 9999;
        sendSignalWithDelay(CMVMI_REF, GSN_NDB_TAMPER, signal, 1000, 1);

        signal->theData[0] = 932;
        EXECUTE_DIRECT(QMGR, GSN_NDB_TAMPER, signal, 1);

        return;
      }
      if (ERROR_INSERTED(7223))
      {
        CLEAR_ERROR_INSERT_VALUE;
        signal->theData[0] = 9999;
        sendSignal(numberToRef(CMVMI, c_error_insert_extra)
                   , GSN_NDB_TAMPER, signal, 1, JBA);
      }
    }
#endif

    if (m_micro_gcp.m_enabled == false)
    {
      jam();
      /**
       * Running old protocol
       */
      signal->theData[0] = DihContinueB::ZSTART_GCP;
      sendSignal(reference(), GSN_CONTINUEB, signal, 1, JBB);
    }
    m_gcp_save.m_master.m_state = GcpSave::GCP_SAVE_IDLE;

    CRASH_INSERTION(7004);
    emptyWaitGCPMasterQueue(signal,
                            Uint64(m_gcp_save.m_gci) << 32,
                            c_waitGCPMasterList);
    break;
  }
  case CopyGCIReq::INITIAL_START_COMPLETED:
    ok = true;
    jam();
    break;
  case CopyGCIReq::IDLE:
    ok = false;
    jam();
    break;
  case CopyGCIReq::RESTART_NR:
    ok = true;
    jam();
    startme_copygci_conf(signal);
    break;
  }
  ndbrequire(ok);


  c_copyGCIMaster.m_copyReason = c_copyGCIMaster.m_waiting[0];
  for (Uint32 i = 1; i<CopyGCIMaster::WAIT_CNT; i++)
  {
    jam();
    c_copyGCIMaster.m_waiting[i-1] = c_copyGCIMaster.m_waiting[i];
  }
  c_copyGCIMaster.m_waiting[CopyGCIMaster::WAIT_CNT-1] = CopyGCIReq::IDLE;

  /**
   * Pop queue
   */
  if(c_copyGCIMaster.m_copyReason != CopyGCIReq::IDLE)
  {
    jam();

    signal->theData[0] = DihContinueB::ZCOPY_GCI;
    signal->theData[1] = c_copyGCIMaster.m_copyReason;
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
  }
}//Dbdih::execCOPY_GCICONF()

void
Dbdih::check_node_in_restart(Signal *signal,
                             BlockReference ref,
                             Uint32 nodeId)
{
  NodeRecordPtr nodePtr;
  for (nodePtr.i = nodeId; nodePtr.i < MAX_NDB_NODES; nodePtr.i++)
  {
    jam();
    ptrAss(nodePtr, nodeRecord);
    if (nodePtr.p->nodeGroup == RNIL ||
        nodePtr.p->nodeRecoveryStatus == NodeRecord::NOT_DEFINED_IN_CLUSTER ||
        nodePtr.p->nodeRecoveryStatus == NodeRecord::NODE_NOT_RESTARTED_YET ||
        nodePtr.p->nodeRecoveryStatus == NodeRecord::NODE_FAILED ||
        nodePtr.p->nodeRecoveryStatus == NodeRecord::NODE_FAILURE_COMPLETED ||
        nodePtr.p->nodeRecoveryStatus == NodeRecord::ALLOCATED_NODE_ID ||
        nodePtr.p->nodeRecoveryStatus == NodeRecord::RESTART_COMPLETED ||
        nodePtr.p->nodeRecoveryStatus == NodeRecord::NODE_ACTIVE)
    {
      /**
       * Nodes that aren't part of a node group won't be part of LCPs,
       * Nodes not defined in Cluster we can ignore
       * Nodes not restarted yet while we were started have no impact
       * on LCP speed, if they restart while we restart doesn't matter
       * since in this case we will run at a speed for starting nodes.
       * Nodes recently failed and even those that completed will speed
       * up LCPs temporarily but using the c_increase_lcp_speed_after_nf
       * variable instead.
       * Nodes that have allocated a node id haven't really started yet.
       * Nodes that have completed their restart also need no speed up.
       */
      continue;
    }
    /**
     * All other states indicate that the node is in some or the other
     * node restart state, so thus it is a good idea to speed up LCP
     * processing.
     */
    jam();
    jamLine(nodePtr.i);
    sendCHECK_NODE_RESTARTCONF(signal, ref, 1);
    return;
  }
  jam();
  /* All nodes are up and running, no restart is ongoing */
  sendCHECK_NODE_RESTARTCONF(signal, ref, 0);
  return;
}

void Dbdih::sendCHECK_NODE_RESTARTCONF(Signal *signal,
                                        BlockReference ref,
                                        Uint32 node_restart)
{
  signal->theData[0] = (m_local_lcp_state.m_state == LocalLCPState::LS_RUNNING)? 1 : 0;
  signal->theData[1] = node_restart;
  sendSignal(ref, GSN_CHECK_NODE_RESTARTCONF, signal, 2, JBB);
}

void Dbdih::execCHECK_NODE_RESTARTREQ(Signal *signal)
{
  NodeRecordPtr nodePtr;
  Uint32 ref = signal->theData[0];
  jamEntry();
  /**
   * No signal data sent, this signal is sent to
   * check if we have any nodes that are currently
   * part of a LCP which is not yet been started.
   */
  if (c_increase_lcp_speed_after_nf == true)
  {
    /**
     * A node recently failed, we will run LCP faster until this LCP
     * has completed to ensure that we quickly get to a point where
     * we can copy the distribution and dictionary information.
     */
    jam();
    sendCHECK_NODE_RESTARTCONF(signal, ref, 1);
    return;
  }
  Uint32 start_node = 1;
  check_node_in_restart(signal, ref, start_node);
  return;
}

void Dbdih::invalidateLcpInfoAfterSr(Signal* signal)
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
        nodePtr.p->activeStatus = Sysfile::NS_Active;
        break;
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
    }
    else
    {
      jam();
      /**
       * It is possible to get here with a number of different activeStatus
       * since the cluster crash could have occurred while a starting node
       * was participating in an LCP to get the node to the NS_Active state.
       */
    }
  }//for
  setNodeRestartInfoBits(signal);
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
  CRASH_INSERTION(7219);

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
    m_gcp_save.m_state = GcpSave::GCP_SAVE_IDLE;
    
    SubGcpCompleteRep * const rep = (SubGcpCompleteRep*)signal->getDataPtr();
    rep->gci_hi = SYSFILE->newestRestorableGCI;
    rep->gci_lo = 0;
    rep->flags = SubGcpCompleteRep::ON_DISK;

    sendSignal(LGMAN_REF, GSN_SUB_GCP_COMPLETE_REP, signal, 
               SubGcpCompleteRep::SignalLength, JBB);
    
    jamEntry();

    if (m_micro_gcp.m_enabled == false)
    {
      jam();
      sendSignal(DBLQH_REF, GSN_SUB_GCP_COMPLETE_REP, signal, 
                 SubGcpCompleteRep::SignalLength, JBB);
      jamEntry();
      ndbrequire(m_micro_gcp.m_state == MicroGcp::M_GCP_COMMITTED);
      m_micro_gcp.m_state = MicroGcp::M_GCP_IDLE;

      CRASH_INSERTION(7190);
    }
    
#ifdef GCP_TIMER_HACK
    globalData.gcp_timer_copygci[1] = NdbTick_getCurrentTicks();

    // this is last timer point so we send local report here
    {
      const GlobalData& g = globalData;
      const Uint32 ms_commit = NdbTick_Elapsed(
	  g.gcp_timer_commit[0], g.gcp_timer_commit[1]).milliSec();
      const Uint32 ms_save = NdbTick_Elapsed(
          g.gcp_timer_save[0], g.gcp_timer_save[1]).milliSec();
      const Uint32 ms_copygci = NdbTick_Elapsed(
          g.gcp_timer_copygci[0], g.gcp_timer_copygci[1]).milliSec();

      const Uint32 ms_total = ms_commit + ms_save + ms_copygci;

      // random formula to report excessive duration
      bool report =
        g.gcp_timer_limit != 0 ?
          (ms_total > g.gcp_timer_limit) :
          (ms_total > 3000 * (1 + cgcpDelay / 1000));
      if (report)
        infoEvent("GCP %u ms: total:%u commit:%u save:%u copygci:%u",
            coldgcp, ms_total, ms_commit, ms_save, ms_copygci);
    }
#endif
  }
  
  jam();
  c_copyGCISlave.m_copyReason = CopyGCIReq::IDLE;
  
  if (reason == CopyGCIReq::GLOBAL_CHECKPOINT)
  {
    jam();
    signal->theData[0] = c_copyGCISlave.m_senderData;
    sendSignal(m_gcp_save.m_master_ref, GSN_COPY_GCICONF, signal, 1, JBB);
  }
  else if (c_copyGCISlave.m_senderRef == cmasterdihref)
  {
    jam();
    /**
     * Only if same master
     */
    signal->theData[0] = c_copyGCISlave.m_senderData;
    sendSignal(c_copyGCISlave.m_senderRef, GSN_COPY_GCICONF, signal, 1, JBB);
  }
  return;
}//Dbdih::writingCopyGciLab()

void Dbdih::execSTART_LCP_REQ(Signal* signal)
{
  jamEntry();
  StartLcpReq * req = (StartLcpReq*)signal->getDataPtr();

  if (getNodeInfo(refToNode(req->senderRef)).m_version >=
      NDBD_SUPPORT_PAUSE_LCP)
  {
    if (req->pauseStart == StartLcpReq::PauseLcpStartFirst)
    {
      /**
       * The message was sent as part of start of LCPs when PAUSE LCP was used.
       * We have paused the LCP protocol and we are preparing to copy the
       * meta data. Before copying the metadata we need access to the
       * m_participatingLQH bitmap of nodes participating in the LCP.
       */
      jam();
      ndbrequire(cmasterdihref == req->senderRef);
      m_local_lcp_state.init(req);
      c_lcpState.m_participatingDIH = req->participatingDIH;
      c_lcpState.m_participatingLQH = req->participatingLQH;
      c_lcpState.m_masterLcpDihRef = cmasterdihref;
      c_lcpState.setLcpStatus(LCP_STATUS_ACTIVE, __LINE__);
      /**
       * We need to update the SYSFILE since it can take some time before we
       * have this number updated after a COPY_GCIREQ in connection to a
       * GCP.
       */
      SYSFILE->latestLCP_ID = req->lcpId;

      {
        char buf[100];
        g_eventLogger->info("c_lcpState.m_participatingLQH bitmap= %s",
            c_lcpState.m_participatingLQH.getText(buf));
        g_eventLogger->info("c_lcpState.m_participatingDIH bitmap= %s",
            c_lcpState.m_participatingDIH.getText(buf));
      }

      ndbrequire(!req->participatingDIH.get(getOwnNodeId()));
      c_lcpState.m_participatingDIH.set(getOwnNodeId());

      StartLcpConf * conf = (StartLcpConf*)signal->getDataPtrSend();
      conf->senderRef = reference();
      sendSignal(c_lcpState.m_masterLcpDihRef, GSN_START_LCP_CONF, signal,
                 StartLcpConf::SignalLength, JBB);
      return;
    }
    if (req->pauseStart == StartLcpReq::PauseLcpStartSecond)
    {
      /**
       * We get the set of already completed LQHs from the master node.
       * No need to know anything about completed DIHs since only the
       * master keeps this information.
       *
       * This signal arrives after copying the meta data. Since we are
       * included into the LCP we verify that there is at least one
       * fragment replica that still hasn't arrived being ready with
       * the LCP execution.
       */
      jam();
      ndbrequire(c_lcpState.lcpStatus == LCP_STATUS_ACTIVE);
      ndbrequire(cmasterdihref == req->senderRef);
      ndbrequire(c_lcpState.m_masterLcpDihRef == cmasterdihref);
      c_lcpState.m_LCP_COMPLETE_REP_Counter_LQH = req->participatingLQH;
      c_lcpState.m_LCP_COMPLETE_REP_Counter_DIH.clearWaitingFor();
      c_lcpState.m_LCP_COMPLETE_REP_From_Master_Received = false;  

      c_current_time = NdbTick_getCurrentTicks();
      c_lcpState.m_start_time = c_current_time;

      g_eventLogger->info("Our node now in LCP execution after pausing LCP");
      g_eventLogger->info("LCP_COMPLETE_REP_Counter_LQH bitmap= %s",
          c_lcpState.m_LCP_COMPLETE_REP_Counter_LQH.getText());

      ndbrequire(!checkLcpAllTablesDoneInLqh(__LINE__));

      StartLcpConf * conf = (StartLcpConf*)signal->getDataPtrSend();
      conf->senderRef = reference();
      sendSignal(c_lcpState.m_masterLcpDihRef, GSN_START_LCP_CONF, signal,
                 StartLcpConf::SignalLength, JBB);
      return;
    }
    ndbrequire(req->pauseStart == StartLcpReq::NormalLcpStart);
  }
  /**
   * Init m_local_lcp_state
   */
  m_local_lcp_state.init(req);

  if (!isMaster())
  {
    jam();
    c_current_time = NdbTick_getCurrentTicks();
    c_lcpState.m_start_time = c_current_time;
  }

  CRASH_INSERTION2(7021, isMaster());
  CRASH_INSERTION2(7022, !isMaster());

  for (Uint32 nodeId = 1; nodeId < MAX_NDB_NODES; nodeId++)
  {
    /**
     * We could have a race here, a node could die while the START_LCP_REQ
     * is in flight. We need remove the node from the set of nodes
     * participating in this case. Not removing it here could lead to a
     * potential LCP deadlock.
     *
     * For the PAUSE LCP code where we are included in the LCP we don't need
     * to worry about this. If any node fails in the state of me being
     * started, I will fail as well.
     */
    NodeRecordPtr nodePtr;
    if (req->participatingDIH.get(nodeId) ||
        req->participatingLQH.get(nodeId))
    {
      nodePtr.i = nodeId;
      ptrAss(nodePtr, nodeRecord);
      if (nodePtr.p->nodeStatus != NodeRecord::ALIVE)
      {
        jam();
        jamLine(nodeId);
        req->participatingDIH.clear(nodeId);
        req->participatingLQH.clear(nodeId);
      }
    }
  }
  c_lcpState.m_participatingDIH = req->participatingDIH;
  c_lcpState.m_participatingLQH = req->participatingLQH;

  for (Uint32 nodeId = 1; nodeId < MAX_NDB_NODES; nodeId++)
  {
    /**
     * We could have a race here, a node could die while the START_LCP_REQ
     * is in flight. We need remove the node from the set of nodes
     * participating in this case. Not removing it here could lead to a
     * potential LCP deadlock.
     *
     * For the PAUSE LCP code where we are included in the LCP we don't need
     * to worry about this. If any node fails in the state of me being
     * started, I will fail as well.
     */
    NodeRecordPtr nodePtr;
    if (req->participatingDIH.get(nodeId) ||
        req->participatingLQH.get(nodeId))
    {
      nodePtr.i = nodeId;
      ptrAss(nodePtr, nodeRecord);
      if (nodePtr.p->nodeStatus != NodeRecord::ALIVE)
      {
        jam();
        jamLine(nodeId);
        req->participatingDIH.clear(nodeId);
        req->participatingLQH.clear(nodeId);
      }
    }
  }

  c_lcpState.m_LCP_COMPLETE_REP_Counter_LQH = req->participatingLQH;
  if(isMaster())
  {
    jam();
    c_lcpState.m_LCP_COMPLETE_REP_Counter_DIH = req->participatingDIH;
  } 
  else
  {
    jam();
    c_lcpState.m_LCP_COMPLETE_REP_Counter_DIH.clearWaitingFor();
  }

  c_lcpState.m_LCP_COMPLETE_REP_From_Master_Received = false;  

  c_lcpState.setLcpStatus(LCP_INIT_TABLES, __LINE__);
  
  ndbrequire(c_lcpState.m_masterLcpDihRef == req->senderRef);

  signal->theData[0] = DihContinueB::ZINIT_LCP;
  signal->theData[1] = c_lcpState.m_masterLcpDihRef;
  signal->theData[2] = 0;
  if (ERROR_INSERTED(7021))
  {
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 3);
  }
  else
  {
    sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBB);
  }
}

void
Dbdih::LocalLCPState::reset()
{
  m_state = LS_INITIAL;
  m_keep_gci = RNIL;
  m_stop_gci = RNIL;
}

void
Dbdih::LocalLCPState::init(const StartLcpReq * req)
{
  m_state = LS_RUNNING;
  m_start_lcp_req = *req;
  m_keep_gci = ~(Uint32)0;
  m_stop_gci = 0;
}

void
Dbdih::LocalLCPState::lcp_frag_rep(const LcpFragRep * rep)
{
  assert(m_state == LS_RUNNING);
  if (rep->maxGciCompleted < m_keep_gci)
  {
    m_keep_gci = rep->maxGciCompleted;
  }

  if (rep->maxGciStarted > m_stop_gci)
  {
    m_stop_gci = rep->maxGciStarted;
  }
}

void
Dbdih::LocalLCPState::lcp_complete_rep(Uint32 gci)
{
  assert(m_state == LS_RUNNING);
  m_state = LS_COMPLETE;
  if (gci > m_stop_gci)
    m_stop_gci = gci;
}

bool
Dbdih::LocalLCPState::check_cut_log_tail(Uint32 gci) const
{
  if (m_state == LS_COMPLETE)
  {
    if (gci >= m_stop_gci)
      return true;
  }
  return false;
}

void Dbdih::initLcpLab(Signal* signal, Uint32 senderRef, Uint32 tableId) 
{
  TabRecordPtr tabPtr;
  tabPtr.i = tableId;

  if (c_lcpState.m_masterLcpDihRef != senderRef ||
      c_lcpState.m_masterLcpDihRef != cmasterdihref)
  {
    /**
     * This is LCP master takeover...abort
     */
    jam();
    return;
  }

  //const Uint32 lcpId = SYSFILE->latestLCP_ID;

  for(; tabPtr.i < ctabFileSize; tabPtr.i++){

    ptrAss(tabPtr, tabRecord);

    if (tabPtr.p->tabStatus != TabRecord::TS_ACTIVE)
    {
      jam();
      tabPtr.p->tabLcpStatus = TabRecord::TLS_COMPLETED;
      continue;
    }

    if (tabPtr.p->tabStorage != TabRecord::ST_NORMAL) {
      /**
       * Table is not logged
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
      sendSignalWithDelay(reference(), GSN_CONTINUEB, signal,
                          WaitTableStateChangeMillis, 3);
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
	  replicaPtr.i = replicaPtr.p->nextPool) {
	jam();
	
        c_replicaRecordPool.getPtr(replicaPtr);
	Uint32 nodeId = replicaPtr.p->procNode;
	if(c_lcpState.m_participatingLQH.get(nodeId)){
	  jam();
	  replicaCount++;
	  replicaPtr.p->lcpOngoingFlag = true;
	}
        else if (replicaPtr.p->lcpOngoingFlag)
        {
          jam();
          replicaPtr.p->lcpOngoingFlag = false;
        }
      }
      
      fragPtr.p->noLcpReplicas = replicaCount;
    }//for
    
    signal->theData[0] = DihContinueB::ZINIT_LCP;
    signal->theData[1] = senderRef;
    signal->theData[2] = tabPtr.i + 1;
    if (ERROR_INSERTED(7021))
    {
      sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 3);
    }
    else
    {
      sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBB);
    }
    return;
  }

  /**
   * No more tables
   */
  jam();
  if (ERROR_INSERTED(7236))
  {
    // delay 20s before completing last CONTINUEB(ZINIT_LCP)
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 20000, 3);
    CLEAR_ERROR_INSERT_VALUE;
    return;
  }

  c_lcpState.setLcpStatus(LCP_STATUS_ACTIVE, __LINE__);

  CRASH_INSERTION2(7023, isMaster());
  CRASH_INSERTION2(7024, !isMaster());

  StartLcpConf * conf = (StartLcpConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  sendSignal(c_lcpState.m_masterLcpDihRef, GSN_START_LCP_CONF, signal,
             StartLcpConf::SignalLength, JBB);
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
  infoEvent("Restore Database from disk Starting");
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
  if (filePtr.i == tabPtr.p->tabFile[0])
  {
    filePtr.i = tabPtr.p->tabFile[1];
    ptrCheckGuard(filePtr, cfileFileSize, fileRecord);
    openFileRw(signal, filePtr);
    filePtr.p->reqStatus = FileRecord::OPENING_TABLE;
  }
  else
  {
    char buf[256];
    BaseString::snprintf(buf, sizeof(buf),
			 "Error opening DIH schema files for table: %d",
			 tabPtr.i);
    progError(__LINE__, NDBD_EXIT_AFS_NO_SUCH_FILE, buf);
  }
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
    ndbrequire(noOfStoredPages <= NDB_ARRAY_SIZE(tabPtr.p->pageRef));
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
Dbdih::execDIH_GET_TABINFO_REQ(Signal* signal)
{
  jamEntry();

  DihGetTabInfoReq req = * (DihGetTabInfoReq*)signal->getDataPtr();

  Uint32 err = 0;
  do
  {
    TabRecordPtr tabPtr;
    tabPtr.i = req.tableId;
    ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);

    if (tabPtr.p->tabStatus != TabRecord::TS_ACTIVE)
    {
      jam();
      err = DihGetTabInfoRef::TableNotDefined;
      break;
    }

    if (cfirstconnect == RNIL)
    {
      jam();
      err = DihGetTabInfoRef::OutOfConnectionRecords;
      break;
    }

    if (tabPtr.p->connectrec != RNIL)
    {
      jam();

      ConnectRecordPtr connectPtr;
      connectPtr.i = tabPtr.p->connectrec;
      ptrCheckGuard(connectPtr, cconnectFileSize, connectRecord);

      if (connectPtr.p->connectState != ConnectRecord::GET_TABINFO)
      {
        jam();
        err = DihGetTabInfoRef::TableBusy;
        break;
      }
    }

    ConnectRecordPtr connectPtr;
    connectPtr.i = cfirstconnect;
    ptrCheckGuard(connectPtr, cconnectFileSize, connectRecord);
    cfirstconnect = connectPtr.p->nextPool;

    connectPtr.p->nextPool = tabPtr.p->connectrec;
    tabPtr.p->connectrec = connectPtr.i;

    connectPtr.p->m_get_tabinfo.m_requestInfo = req.requestInfo;
    connectPtr.p->userpointer = req.senderData;
    connectPtr.p->userblockref = req.senderRef;
    connectPtr.p->connectState = ConnectRecord::GET_TABINFO;
    connectPtr.p->table = tabPtr.i;

    if (connectPtr.p->nextPool == RNIL)
    {
      jam();

      /**
       * we're the first...start packing...
       */
      signal->theData[0] = DihContinueB::ZGET_TABINFO;
      signal->theData[1] = tabPtr.i;
      sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
    }

    return;
  } while (0);

  DihGetTabInfoRef * ref = (DihGetTabInfoRef*)signal->getDataPtrSend();
  ref->senderData = req.senderData;
  ref->senderRef = reference();
  ref->errorCode = err;
  sendSignal(req.senderRef, GSN_DIH_GET_TABINFO_REF, signal,
             DihGetTabInfoRef::SignalLength, JBB);
}

void
Dbdih::getTabInfo(Signal* signal)
{
  TabRecordPtr tabPtr;
  tabPtr.i = signal->theData[1];
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);

  if (tabPtr.p->tabCopyStatus != TabRecord::CS_IDLE)
  {
    jam();
    signal->theData[0] = DihContinueB::ZGET_TABINFO;
    signal->theData[1] = tabPtr.i;
    sendSignalWithDelay(reference(),
                        GSN_CONTINUEB,
                        signal,
                        WaitTableStateChangeMillis,
                        signal->length());
    return;
  }

  tabPtr.p->tabCopyStatus  = TabRecord::CS_GET_TABINFO;

  signal->theData[0] = DihContinueB::ZPACK_TABLE_INTO_PAGES;
  signal->theData[1] = tabPtr.i;
  sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
}

int
Dbdih::getTabInfo_copyTableToSection(SegmentedSectionPtr & ptr,
                                     CopyTableNode ctn)
{
  PageRecordPtr pagePtr;
  pagePtr.i = ctn.ctnTabPtr.p->pageRef[0];
  ptrCheckGuard(pagePtr, cpageFileSize, pageRecord);

  while (ctn.noOfWords > 2048)
  {
    jam();
    ndbrequire(import(ptr, pagePtr.p->word, 2048));
    ctn.noOfWords -= 2048;

    ctn.pageIndex++;
    pagePtr.i = ctn.ctnTabPtr.p->pageRef[ctn.pageIndex];
    ptrCheckGuard(pagePtr, cpageFileSize, pageRecord);
  }

  ndbrequire(import(ptr, pagePtr.p->word, ctn.noOfWords));
  return 0;
}

int
Dbdih::getTabInfo_copySectionToPages(TabRecordPtr tabPtr,
                                     SegmentedSectionPtr ptr)
{
  jam();
  Uint32 sz = ptr.sz;
  SectionReader reader(ptr, getSectionSegmentPool());

  while (sz)
  {
    jam();
    PageRecordPtr pagePtr;
    allocpage(pagePtr);
    tabPtr.p->pageRef[tabPtr.p->noPages] = pagePtr.i;
    tabPtr.p->noPages++;

    Uint32 len = sz > 2048 ? 2048 : sz;
    ndbrequire(reader.getWords(pagePtr.p->word, len));
    sz -= len;
  }
  return 0;
}

void
Dbdih::getTabInfo_send(Signal* signal,
                       TabRecordPtr tabPtr)
{
  ndbrequire(tabPtr.p->tabCopyStatus == TabRecord::CS_GET_TABINFO);

  ConnectRecordPtr connectPtr;
  connectPtr.i = tabPtr.p->connectrec;

  /**
   * Done
   */
  if (connectPtr.i == RNIL)
  {
    jam();
    tabPtr.p->tabCopyStatus = TabRecord::CS_IDLE;
    return;
  }

  ptrCheckGuard(connectPtr, cconnectFileSize, connectRecord);

  ndbrequire(connectPtr.p->connectState == ConnectRecord::GET_TABINFO);
  ndbrequire(connectPtr.p->table == tabPtr.i);

  /**
   * Copy into segmented sections here...
   * NOTE: A GenericSectionIterator would be nice inside kernel too
   *  or having a pack-method that writes directly into SegmentedSection
   */
  PageRecordPtr pagePtr;
  pagePtr.i = tabPtr.p->pageRef[0];
  ptrCheckGuard(pagePtr, cpageFileSize, pageRecord);
  Uint32 words = pagePtr.p->word[34];

  CopyTableNode ctn;
  ctn.ctnTabPtr = tabPtr;
  ctn.pageIndex = 0;
  ctn.wordIndex = 0;
  ctn.noOfWords = words;

  SegmentedSectionPtr ptr;
  ndbrequire(getTabInfo_copyTableToSection(ptr, ctn) == 0);

  Callback cb = { safe_cast(&Dbdih::getTabInfo_sendComplete), connectPtr.i };

  SectionHandle handle(this, signal);
  handle.m_ptr[0] = ptr;
  handle.m_cnt = 1;

  DihGetTabInfoConf* conf = (DihGetTabInfoConf*)signal->getDataPtrSend();
  conf->senderData = connectPtr.p->userpointer;
  conf->senderRef = reference();
  sendFragmentedSignal(connectPtr.p->userblockref, GSN_DIH_GET_TABINFO_CONF, signal,
                       DihGetTabInfoConf::SignalLength, JBB, &handle, cb);
}

void
Dbdih::getTabInfo_sendComplete(Signal * signal,
                               Uint32 senderData,
                               Uint32 retVal)
{
  ndbrequire(retVal == 0);

  ConnectRecordPtr connectPtr;
  connectPtr.i = senderData;
  ptrCheckGuard(connectPtr, cconnectFileSize, connectRecord);

  ndbrequire(connectPtr.p->connectState == ConnectRecord::GET_TABINFO);

  TabRecordPtr tabPtr;
  tabPtr.i = connectPtr.p->table;
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
  tabPtr.p->connectrec = connectPtr.p->nextPool;

  signal->theData[0] = DihContinueB::ZGET_TABINFO_SEND;
  signal->theData[1] = tabPtr.i;
  sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);

  release_connect(connectPtr);
}

void
Dbdih::resetReplicaSr(TabRecordPtr tabPtr){

  const Uint32 newestRestorableGCI = SYSFILE->newestRestorableGCI;
  
  for(Uint32 i = 0; i<tabPtr.p->totalfragments; i++)
  {
    FragmentstorePtr fragPtr;
    getFragstore(tabPtr.p, i, fragPtr);
    
    /**
     * During SR restart distributionKey from 0
     */
    fragPtr.p->distributionKey = 0;

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
    while (replicaPtr.i != RNIL)
    {
      jam();
      c_replicaRecordPool.getPtr(replicaPtr);

      /**
       * invalidate LCP's not usable
       */
      resetReplica(replicaPtr);

      const Uint32 nextReplicaPtrI = replicaPtr.p->nextPool;

      NodeRecordPtr nodePtr;
      nodePtr.i = replicaPtr.p->procNode;
      ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);

      const Uint32 noCrashedReplicas = replicaPtr.p->noCrashedReplicas;

      if (nodePtr.p->nodeStatus == NodeRecord::ALIVE)
      {
	jam();
	switch (nodePtr.p->activeStatus) {
	case Sysfile::NS_Active:
	case Sysfile::NS_ActiveMissed_1:
	case Sysfile::NS_ActiveMissed_2:{
	  jam();
	  /* --------------------------------------------------------------- */
	  /* THE NODE IS ALIVE AND KICKING AND ACTIVE, LET'S USE IT.         */
	  /* --------------------------------------------------------------- */
	  arrGuardErr(noCrashedReplicas, MAX_CRASHED_REPLICAS, NDBD_EXIT_MAX_CRASHED_REPLICAS);

          // Create new crashed replica
          newCrashedReplica(replicaPtr);

          // Create a new redo-interval
          Uint32 nextCrashed = replicaPtr.p->noCrashedReplicas;
          replicaPtr.p->createGci[nextCrashed] = newestRestorableGCI + 1;
          replicaPtr.p->replicaLastGci[nextCrashed] = ZINIT_REPLICA_LAST_GCI;

          // merge
          mergeCrashedReplicas(replicaPtr);

	  resetReplicaLcp(replicaPtr.p, newestRestorableGCI);

	  /**
	   * Make sure we can also find REDO for restoring replica...
	   */
	  {
	    CreateReplicaRecord createReplica;
	    ConstPtr<ReplicaRecord> constReplicaPtr;
	    constReplicaPtr.i = replicaPtr.i;
	    constReplicaPtr.p = replicaPtr.p;
	    if (tabPtr.p->tabStorage != TabRecord::ST_NORMAL ||
		setup_create_replica(fragPtr,
				     &createReplica, constReplicaPtr))
	    {
	      jam();
	      removeOldStoredReplica(fragPtr, replicaPtr);
	      linkStoredReplica(fragPtr, replicaPtr);
	    }
	    else
	    {
	      jam();
	      g_eventLogger->info("Forcing take-over of node %d due to insufficient REDO"
			" for table %d fragment: %d",
			nodePtr.i, tabPtr.i, i);
	      infoEvent("Forcing take-over of node %d due to insufficient REDO"
			" for table %d fragment: %d",
			nodePtr.i, tabPtr.i, i);
	      
              m_sr_nodes.clear(nodePtr.i);
              m_to_nodes.set(nodePtr.i);
	      setNodeActiveStatus(nodePtr.i, 
				  Sysfile::NS_NotActive_NotTakenOver);
	    }
	  }
	}
        default:
	  jam();
	  /*empty*/;
	  break;
	}
      }
      replicaPtr.i = nextReplicaPtrI;
    }//while
    if (fragPtr.p->storedReplicas == RNIL)
    {
      // This should have been caught in Dbdih::execDIH_RESTARTREQ
#ifdef ERROR_INSERT
      // Extra printouts for debugging
      g_eventLogger->info("newestRestorableGCI %u", newestRestorableGCI);
      ReplicaRecordPtr replicaPtr;
      replicaPtr.i = fragPtr.p->oldStoredReplicas;
      while (replicaPtr.i != RNIL)
      {
        c_replicaRecordPool.getPtr(replicaPtr);
        g_eventLogger->info("[1/3] frag %u, replica %u @%p, SYSFILE @%p",
          fragPtr.i, replicaPtr.i, replicaPtr.p, SYSFILE);
        g_eventLogger->info("[2/3] frag %u, replica %u, node %u, replicaLastGci %u,%u",
          fragPtr.i, replicaPtr.i, replicaPtr.p->procNode,
          replicaPtr.p->replicaLastGci[0], replicaPtr.p->replicaLastGci[1]);
        ndbrequire(replicaPtr.p->procNode < MAX_NDB_NODES)
        g_eventLogger->info("[3/3] frag %u, replica %u, node %u, lastCompletedGCI %u",
          fragPtr.i, replicaPtr.i, replicaPtr.p->procNode,
          SYSFILE->lastCompletedGCI[replicaPtr.p->procNode]);
        replicaPtr.i = replicaPtr.p->nextPool;
      }
#endif
      char buf[255];
      BaseString::snprintf
        (buf, sizeof(buf),
         "Nodegroup %u has not enough data on disk for restart.", i);
      progError(__LINE__,
                NDBD_EXIT_INSUFFICENT_NODES,
                buf);
    }
    updateNodeInfo(fragPtr);
  }
}

void
Dbdih::resetReplica(ReplicaRecordPtr readReplicaPtr)
{
  Uint32 i;
  /* ---------------------------------------------------------------------- */
  /*       IF THE LAST COMPLETED LOCAL CHECKPOINT IS VALID AND LARGER THAN  */
  /*       THE LAST COMPLETED CHECKPOINT THEN WE WILL INVALIDATE THIS LOCAL */
  /*       CHECKPOINT FOR THIS REPLICA.                                     */
  /* ---------------------------------------------------------------------- */
  for (i = 0; i < MAX_LCP_STORED; i++)
  {
    jam();
    if (readReplicaPtr.p->lcpStatus[i] == ZVALID &&
        readReplicaPtr.p->lcpId[i] > SYSFILE->latestLCP_ID)
    {
      jam();
      readReplicaPtr.p->lcpStatus[i] = ZINVALID;
    }
  }

  /* ---------------------------------------------------------------------- */
  /*       WE ALSO HAVE TO INVALIDATE ANY LOCAL CHECKPOINTS THAT HAVE BEEN  */
  /*       INVALIDATED BY MOVING BACK THE RESTART GCI.                      */
  /* ---------------------------------------------------------------------- */
  Uint32 lastCompletedGCI = SYSFILE->newestRestorableGCI;
  for (i = 0; i < MAX_LCP_STORED; i++)
  {
    jam();
    if (readReplicaPtr.p->lcpStatus[i] == ZVALID &&
        readReplicaPtr.p->maxGciStarted[i] > lastCompletedGCI)
    {
      jam();
      readReplicaPtr.p->lcpStatus[i] = ZINVALID;
    }
  }

  /* ---------------------------------------------------------------------- */
  /*       WE WILL REMOVE ANY OCCURRENCES OF REPLICAS THAT HAVE CRASHED     */
  /*       THAT ARE NO LONGER VALID DUE TO MOVING RESTART GCI BACKWARDS.    */
  /* ---------------------------------------------------------------------- */
  removeTooNewCrashedReplicas(readReplicaPtr, lastCompletedGCI);

  /**
   * Don't remove crashed replicas here,
   *   as 1) this will disable optimized NR
   *         if oldestRestorableGCI > GCI needed for local LCP's
   *      2) This is anyway done during LCP, which will be run during SR
   */
  //removeOldCrashedReplicas(readReplicaPtr);

  /* ---------------------------------------------------------------------- */
  /*       FIND PROCESSOR RECORD                                            */
  /* ---------------------------------------------------------------------- */
}

void
Dbdih::resetReplicaLcp(ReplicaRecord * replicaP, Uint32 stopGci){

  Uint32 lcpNo = replicaP->nextLcp;
  const Uint32 startLcpNo = lcpNo;
  do {
    lcpNo = prevLcpNo(lcpNo);
    ndbrequire(lcpNo < MAX_LCP_STORED);
    if (replicaP->lcpStatus[lcpNo] == ZVALID)
    {
      if (replicaP->maxGciStarted[lcpNo] <= stopGci)
      {
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
  ndbrequire(tabPtr.p->noPages <= NDB_ARRAY_SIZE(tabPtr.p->pageRef));
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
  CopyTabReq *req = (CopyTabReq*) &signal->theData[0];
  CRASH_INSERTION(7172);

  TabRecordPtr tabPtr;
  PageRecordPtr pagePtr;
  jamEntry();
  BlockReference ref = req->senderRef;
  Uint32 reqinfo = req->reqinfo;
  tabPtr.i = req->tableId;
  Uint32 schemaVersion = req->tableSchemaVersion;
  Uint32 noOfWords = req->noOfWords;
  ndbrequire(ref == cmasterdihref);
  ndbrequire(!isMaster());
  ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
  if (reqinfo == 1)
  {
    jam();
    tabPtr.p->schemaVersion = schemaVersion;
    initTableFile(tabPtr);

    /**
     * We need to set up the state of whether the table is actively writing
     * an LCP still. We can derive the state on replicas and fragments for
     * the LCP with the information that we get in the table by knowing the
     * currently executing LCP id. We also get the current LCP id fromt the
     * master here to ensure that we're up to date with this value.
     */
    c_lcp_id_while_copy_meta_data = req->currentLcpId;
    Uint32 masterNodeId = refToNode(ref);
    if (getNodeInfo(masterNodeId).m_version >= NDBD_SUPPORT_PAUSE_LCP)
    {
      if (req->tabLcpStatus == CopyTabReq::LcpCompleted)
      {
        jam();
        tabPtr.p->tabLcpStatus = TabRecord::TLS_COMPLETED;
      }
      else
      {
        jam();
        ndbrequire(req->tabLcpStatus == CopyTabReq::LcpActive);
        tabPtr.p->tabLcpStatus = TabRecord::TLS_ACTIVE;
      }
    }
    else
    {
      jam();
      tabPtr.p->tabLcpStatus = TabRecord::TLS_COMPLETED;
    }
  }//if
  ndbrequire(tabPtr.p->noPages < NDB_ARRAY_SIZE(tabPtr.p->pageRef));
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

    /**
     * No need to protect these changes as they occur while recovery is ongoing
     * and DBTC hasn't started using these tables yet.
     */
    tabPtr.p->tabStatus = TabRecord::TS_ACTIVE;
    for (Uint32 fragId = 0; fragId < tabPtr.p->totalfragments; fragId++) {
      jam();
      FragmentstorePtr fragPtr;
      getFragstore(tabPtr.p, fragId, fragPtr);
      updateNodeInfo(fragPtr);
    }//for
  }//if
  c_lcp_id_while_copy_meta_data = RNIL;
  CopyTabConf *conf = (CopyTabConf*) signal->getDataPtrSend();
  conf->nodeId = getOwnNodeId();
  conf->tableId = tabPtr.i;
  sendSignal(cmasterdihref, GSN_COPY_TABCONF, signal,
             CopyTabConf::SignalLength, JBB);
}

/*****************************************************************************/
/* ******  READ FROM A NUMBER OF PAGES INTO THE TABLE DATA STRUCTURES ********/
/*****************************************************************************/
void Dbdih::readPagesIntoTableLab(Signal* signal, Uint32 tableId) 
{
  /**
   * No need to protect these changes, they are only occuring during
   * recovery when DBTC hasn't accessibility to the table yet.
   */
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
  rf.rwfTabPtr.p->method = (TabRecord::Method)readPageWord(&rf);
  /* ------------- */
  /* Type of table */
  /* ------------- */
  rf.rwfTabPtr.p->tabStorage = (TabRecord::Storage)(readPageWord(&rf)); 

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
  ndbrequire(rf->pageIndex < NDB_ARRAY_SIZE(rf->rwfTabPtr.p->pageRef));
  rf->rwfPageptr.i = rf->rwfTabPtr.p->pageRef[rf->pageIndex];
  ptrCheckGuard(rf->rwfPageptr, cpageFileSize, pageRecord);
  FragmentstorePtr fragPtr;
  getFragstore(rf->rwfTabPtr.p, rf->fragId, fragPtr);
  readFragment(rf, fragPtr);
  readReplicas(rf, rf->rwfTabPtr.p, fragPtr);
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
      if (getNodeState().getSystemRestartInProgress() && 
          rf->rwfTabPtr.p->tabStorage == TabRecord::ST_NORMAL)
      {
        /**
         * avoid overwriting own table-definition...
         *   but this is not possible for no-logging tables
         */
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
  Uint32 totalfragments = tabPtr.p->totalfragments;
  if (tabPtr.p->connectrec != RNIL)
  {
    jam();
    Ptr<ConnectRecord> connectPtr;
    connectPtr.i = tabPtr.p->connectrec;
    ptrCheckGuard(connectPtr, cconnectFileSize, connectRecord);
    ndbrequire(connectPtr.p->table == tabPtr.i);
    if (connectPtr.p->connectState == ConnectRecord::ALTER_TABLE)
    {
      jam();
      totalfragments = connectPtr.p->m_alter.m_totalfragments;
    }
  }

  writePageWord(&wf, totalfragments);
  writePageWord(&wf, tabPtr.p->noOfBackups);
  writePageWord(&wf, tabPtr.p->hashpointer);
  writePageWord(&wf, tabPtr.p->kvalue);
  writePageWord(&wf, tabPtr.p->mask);
  writePageWord(&wf, tabPtr.p->method);
  writePageWord(&wf, tabPtr.p->tabStorage);

  signal->theData[0] = DihContinueB::ZPACK_FRAG_INTO_PAGES;
  signal->theData[1] = tabPtr.i;
  signal->theData[2] = 0;
  signal->theData[3] = wf.pageIndex;
  signal->theData[4] = wf.wordIndex;
  signal->theData[5] = totalfragments;
  sendSignal(reference(), GSN_CONTINUEB, signal, 6, JBB);
}//Dbdih::packTableIntoPagesLab()

/*****************************************************************************/
// execCONTINUEB(ZPACK_FRAG_INTO_PAGES)
/*****************************************************************************/
void Dbdih::packFragIntoPagesLab(Signal* signal, RWFragment* wf) 
{
  ndbrequire(wf->pageIndex < NDB_ARRAY_SIZE(wf->rwfTabPtr.p->pageRef));
  wf->rwfPageptr.i = wf->rwfTabPtr.p->pageRef[wf->pageIndex];
  ptrCheckGuard(wf->rwfPageptr, cpageFileSize, pageRecord);
  FragmentstorePtr fragPtr;
  getFragstore(wf->rwfTabPtr.p, wf->fragId, fragPtr);
  writeFragment(wf, fragPtr);
  writeReplicas(wf, fragPtr.p->storedReplicas);
  writeReplicas(wf, fragPtr.p->oldStoredReplicas);
  wf->fragId++;
  if (wf->fragId == wf->totalfragments) {
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
    case TabRecord::CS_COPY_TO_SAVE:
      signal->theData[0] = DihContinueB::ZTABLE_UPDATE;
      signal->theData[1] = wf->rwfTabPtr.i;
      sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
      return;
    case TabRecord::CS_GET_TABINFO:
      jam();
      signal->theData[0] = DihContinueB::ZGET_TABINFO_SEND;
      signal->theData[1] = wf->rwfTabPtr.i;
      sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
      return;
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
    signal->theData[5] = wf->totalfragments;
    sendSignal(reference(), GSN_CONTINUEB, signal, 6, JBB);
  }//if
  return;
}//Dbdih::packFragIntoPagesLab()

/*****************************************************************************/
/* **********     START FRAGMENT MODULE                          *************/
/*****************************************************************************/
void
Dbdih::dump_replica_info()
{
  TabRecordPtr tabPtr;
  FragmentstorePtr fragPtr;

  for(tabPtr.i = 0; tabPtr.i < ctabFileSize; tabPtr.i++)
  {
    ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
    if (tabPtr.p->tabStatus != TabRecord::TS_ACTIVE)
      continue;
    
    for(Uint32 fid = 0; fid<tabPtr.p->totalfragments; fid++)
    {
      getFragstore(tabPtr.p, fid, fragPtr);
      ndbout_c("tab: %d frag: %d gci: %d\n", 
	       tabPtr.i, fid, SYSFILE->newestRestorableGCI);
      
      dump_replica_info(fragPtr.p);
    }
  }
}

void
Dbdih::dump_replica_info(const Fragmentstore* fragPtrP)
{
  ndbout_c("  -- storedReplicas: ");
  Uint32 i;
  ReplicaRecordPtr replicaPtr;
  replicaPtr.i = fragPtrP->storedReplicas;
  for(; replicaPtr.i != RNIL; replicaPtr.i = replicaPtr.p->nextPool)
  {
    c_replicaRecordPool.getPtr(replicaPtr);
    ndbout_c("  node: %d initialGci: %d nextLcp: %d noCrashedReplicas: %d",
             replicaPtr.p->procNode,
             replicaPtr.p->initialGci,
             replicaPtr.p->nextLcp,
             replicaPtr.p->noCrashedReplicas);
    for(i = 0; i<MAX_LCP_STORED; i++)
    {
      ndbout_c("    i: %d %s : lcpId: %d maxGci Completed: %d Started: %d",
               i, 
               (replicaPtr.p->lcpStatus[i] == ZVALID ?"VALID":"INVALID"),
               replicaPtr.p->lcpId[i],
               replicaPtr.p->maxGciCompleted[i],
               replicaPtr.p->maxGciStarted[i]);
    }
    
    for (i = 0; i < 8; i++)
    {
      ndbout_c("    crashed replica: %d replicaLastGci: %d createGci: %d",
               i, 
               replicaPtr.p->replicaLastGci[i],
               replicaPtr.p->createGci[i]);
    }
  }
  ndbout_c("  -- oldStoredReplicas");
  replicaPtr.i = fragPtrP->oldStoredReplicas;
  for(; replicaPtr.i != RNIL; replicaPtr.i = replicaPtr.p->nextPool)
  {
    c_replicaRecordPool.getPtr(replicaPtr);
    ndbout_c("  node: %d initialGci: %d nextLcp: %d noCrashedReplicas: %d",
             replicaPtr.p->procNode,
             replicaPtr.p->initialGci,
             replicaPtr.p->nextLcp,
             replicaPtr.p->noCrashedReplicas);
    for(i = 0; i<MAX_LCP_STORED; i++)
    {
      ndbout_c("    i: %d %s : lcpId: %d maxGci Completed: %d Started: %d",
               i, 
               (replicaPtr.p->lcpStatus[i] == ZVALID ?"VALID":"INVALID"),
               replicaPtr.p->lcpId[i],
               replicaPtr.p->maxGciCompleted[i],
               replicaPtr.p->maxGciStarted[i]);
    }
    
    for (i = 0; i < 8; i++)
    {
      ndbout_c("    crashed replica: %d replicaLastGci: %d createGci: %d",
               i, 
               replicaPtr.p->replicaLastGci[i],
               replicaPtr.p->createGci[i]);
    }
  }
}

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
    
    if(tabPtr.p->tabStorage != TabRecord::ST_NORMAL){
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
  ndbrequire(tabPtr.p->noOfBackups < MAX_REPLICAS);
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

    ndbout_c("%s", buf);
    dump_replica_info();
    
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
  sendLoopMacro(START_RECREQ, sendSTART_RECREQ, RNIL);
}//completeRestartLab()

/* ------------------------------------------------------------------------- */
//       SYSTEM RESTART:
/*         A NODE HAS COMPLETED RESTORING ALL DATABASE FRAGMENTS.            */
//       NODE RESTART:
//         THE STARTING NODE HAS PREPARED ITS LOG FILES TO ENABLE EXECUTION
//         OF TRANSACTIONS.
// Precondition:
//   This signal is received by the master node for the system restart.
//   This signal is received by the starting node for node restart.
/* ------------------------------------------------------------------------- */
void Dbdih::execSTART_RECCONF(Signal* signal) 
{
  jamEntry();
  Uint32 senderNodeId = signal->theData[0];
  Uint32 senderData = signal->theData[1];

  if (senderData != RNIL)
  {
    jam();
    c_performed_copy_phase = true;
    /**
     * This is normally a node restart, but it could also be second
     * phase of a system restart where a node is restored from a more
     * alive node, in this case we could even be the master node although
     * we arrive here.
     */
    g_eventLogger->info("Restore Database Off-line Completed");
    infoEvent("Restore Database Off-line Completed on node %u",
              senderNodeId);

    g_eventLogger->info("Bring Database On-line Starting");
    infoEvent("Bring Database On-line Starting on node %u",
              senderNodeId);

    /**
     * This is node restart
     */
    Ptr<TakeOverRecord> takeOverPtr;
    c_takeOverPool.getPtr(takeOverPtr, senderData);
    sendStartTo(signal, takeOverPtr);
    return;
  }
  infoEvent("Restore Database from disk Completed on node %u",
            senderNodeId);

  /* No take over record in the system restart case here */
  ndbrequire(senderData == RNIL);
  /* --------------------------------------------------------------------- */
  // This was the system restart case. We set the state indicating that the
  // node has completed restoration of all fragments.
  /* --------------------------------------------------------------------- */
  receiveLoopMacro(START_RECREQ, senderNodeId);
  
  /**
   * Remove each node that has to TO from LCP/LQH
   */
  Uint32 i = 0;
  while ((i = m_to_nodes.find(i + 1)) != NdbNodeBitmask::NotFound)
  {
    jam();
    NodeRecordPtr nodePtr;
    nodePtr.i = i;
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
    nodePtr.p->copyCompleted = 0;
  }

  if (m_to_nodes.get(getOwnNodeId()))
  {
    /**
     * We (master) needs take-over
     *   run this directly to avoid strange confusion
     */
    jam();
    c_sr_wait_to = true;
  }

  if (!m_to_nodes.isclear() && c_sr_wait_to)
  {
    jam();

    StartCopyReq* req = (StartCopyReq*)signal->getDataPtrSend();
    req->senderRef = reference();
    req->senderData = getOwnNodeId();
    req->flags = 0; // Note dont wait for LCP

    i = 0;
    while ((i = m_to_nodes.find(i + 1)) != NdbNodeBitmask::NotFound)
    {
      jam();
      req->startingNodeId = i;
      sendSignal(calcDihBlockRef(i), GSN_START_COPYREQ, signal, 
                 StartCopyReq::SignalLength, JBB);
    }

    char buf[100];
    infoEvent("Starting take-over of %s", m_to_nodes.getText(buf));    
    return;
  }
  
  infoEvent("Restore Database from disk Completed");

  signal->theData[0] = reference();
  m_sr_nodes.copyto(NdbNodeBitmask::Size, signal->theData+1);
  sendSignal(cntrlblockref, GSN_NDB_STARTCONF, signal, 
             1 + NdbNodeBitmask::Size, JBB);
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
    // We also need to handle the pausing of LCPs if it was active.
    /* ---------------------------------------------------------------------- */
    c_nodeStartMaster.wait = ZFALSE;
    return;
  }//if
  TabRecordPtr tabPtr;
  tabPtr.i = tableId;
  while (tabPtr.i < ctabFileSize) {
    ptrAss(tabPtr, tabRecord);
    if (tabPtr.p->tabStatus == TabRecord::TS_ACTIVE)
    {
      /* -------------------------------------------------------------------- */
      // The table is defined. We will start by packing the table into pages.
      // The tabCopyStatus indicates to the CONTINUEB(ZPACK_TABLE_INTO_PAGES)
      // who called it. After packing the table into page(s) it will be sent to 
      // the starting node by COPY_TABREQ signals. After returning from the 
      // starting node we will return to this subroutine and continue 
      // with the next table.
      /* -------------------------------------------------------------------- */
      if (! (tabPtr.p->tabCopyStatus == TabRecord::CS_IDLE))
      {
        jam();
        signal->theData[0] = DihContinueB::ZCOPY_NODE;
        signal->theData[1] = tabPtr.i;
        sendSignalWithDelay(reference(), GSN_CONTINUEB, signal,
                            WaitTableStateChangeMillis, 2);
        return;
      }
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
  jam();
  if (is_lcp_paused())
  {
    jam();
    /**
     * Copying is done, we now need to tell the starting node about the
     * already completed LQHs and to ensure that the starting node
     * verifies that the copy was correct.
     */
    check_for_pause_action(signal, StartLcpReq::PauseLcpStartSecond);
    return;
  }
  else
  {
    jam();
    dihCopyCompletedLab(signal);
    return;
  }
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
  ndbrequire(ctn->pageIndex < NDB_ARRAY_SIZE(ctn->ctnTabPtr.p->pageRef));
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
        ndbrequire(ctn->pageIndex < NDB_ARRAY_SIZE(ctn->ctnTabPtr.p->pageRef));
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
  CopyTabReq *req = (CopyTabReq*) signal->getDataPtrSend();
  req->senderRef = reference();
  req->reqinfo = reqinfo;
  req->tableId = ctn->ctnTabPtr.i;
  req->tableSchemaVersion = ctn->ctnTabPtr.p->schemaVersion;
  req->noOfWords = ctn->noOfWords;
  ndbrequire(ctn->wordIndex + 15 < 2048);
  MEMCOPY_NO_WORDS(&req->tableWords[0],
                   &ctn->ctnPageptr.p->word[ctn->wordIndex],
                   16);
  Uint32 sig_len = CopyTabReq::SignalLength;
  if (reqinfo == 1)
  {
    if (ctn->ctnTabPtr.p->tabLcpStatus == TabRecord::TLS_ACTIVE)
    {
      jam();
      req->tabLcpStatus = CopyTabReq::LcpActive;
    }
    else
    {
      jam();
      /**
       * The state TLS_WRITING_TO_FILE means that the LCP is completed from the
       * viewpoint of the new starting node since it will start by writing the
       * table description to disk.
       */
      req->tabLcpStatus = CopyTabReq::LcpCompleted;
    }
    req->currentLcpId = SYSFILE->latestLCP_ID;
    sig_len = CopyTabReq::SignalLengthExtra;
  }
  sendSignal(ref, GSN_COPY_TABREQ, signal, sig_len, JBB);
}//Dbdih::sendCopyTable()

void Dbdih::execCOPY_TABCONF(Signal* signal) 
{
  CopyTabConf *conf = (CopyTabConf*) &signal->theData[0];
  jamEntry();
  Uint32 nodeId = conf->nodeId;
  Uint32 tableId = conf->tableId;
  if (getNodeState().startLevel >= NodeState::SL_STARTED){
    /* --------------------------------------------------------------------- */
    // We are in the process of performing a node restart. Continue by copying
    // the next table to the starting node.
    /* --------------------------------------------------------------------- */
    jam();
    ndbrequire(nodeId == c_nodeStartMaster.startNode);
    c_COPY_TABREQ_Counter.clearWaitingFor(nodeId);

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
    receiveLoopMacro(COPY_TABREQ, nodeId);
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

    /**
     * No need to protect this as it happens during recovery when DBTC isn't
     * acting on the tables yet. Also given that fragId is 0 we are sure that
     * this will only result in ADD_FRAGREQ being sent.
     */
    connectPtr.p->m_alter.m_totalfragments = tabPtr.p->totalfragments;
    D("6: totalfragments = " << tabPtr.p->totalfragments);
    sendAddFragreq(signal, connectPtr, tabPtr, 0, false);
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
    g_eventLogger->error("lcpStatus = %u"
                         "lcpStatusUpdatedPlace = %d",
                         (Uint32) c_lcpState.lcpStatus,
                         c_lcpState.lcpStatusUpdatedPlace);
    ndbrequire(false);
    return;
  }//if
  add_lcp_counter(&c_lcpState.ctimer, 32);
  if (c_lcpState.lcpStopGcp >= c_newest_restorable_gci) {
    jam();
    /* --------------------------------------------------------------------- */
    // We block LCP start if we have not completed one global checkpoints
    // before starting another local checkpoint.
    /* --------------------------------------------------------------------- */
    c_lcpState.setLcpStatus(LCP_STATUS_IDLE, __LINE__);
    checkLcpStart(signal, __LINE__, 100);
    return;
  }//if 
  c_lcpState.setLcpStatus(LCP_TCGET, __LINE__);
  
  c_lcpState.ctcCounter = c_lcpState.ctimer;
  sendLoopMacro(TCGETOPSIZEREQ, sendTCGETOPSIZEREQ, RNIL);
}//Dbdih::checkTcCounterLab()

void Dbdih::checkLcpStart(Signal* signal, Uint32 lineNo, Uint32 delay)
{
  /* ----------------------------------------------------------------------- */
  // Verify that we are not attempting to start another instance of the LCP
  // when it is not alright to do so.
  /* ----------------------------------------------------------------------- */
  c_lcpState.lcpStart = ZACTIVE;
  signal->theData[0] = DihContinueB::ZCHECK_TC_COUNTER;
  signal->theData[1] = lineNo;
  if (delay == 0)
  {
    jam();
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
  }
  else
  {
    jam();
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, delay, 2);
  }
}//Dbdih::checkLcpStart()

/* ------------------------------------------------------------------------- */
/*TCGETOPSIZECONF          HOW MUCH OPERATION SIZE HAVE BEEN EXECUTED BY TC  */
/* ------------------------------------------------------------------------- */
void Dbdih::execTCGETOPSIZECONF(Signal* signal) 
{
  jamEntry();
  Uint32 senderNodeId = signal->theData[0];
  add_lcp_counter(&c_lcpState.ctcCounter, signal->theData[1]);
  
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
  if (c_lcpState.immediateLcpStart == false) 
  {
    Uint64 cnt = Uint64(c_lcpState.ctcCounter);
    Uint64 limit = Uint64(1) << c_lcpState.clcpDelay;
    bool dostart = cnt >= limit; 
    if (dostart == false)
    {
      jam();
      c_lcpState.setLcpStatus(LCP_STATUS_IDLE, __LINE__);
      checkLcpStart(signal, __LINE__, 1000);
      return;
    }//if

    /**
     * Check if we have reason to stall the start of the LCP due to
     * outstanding node restarts that are reasonably close to
     * need a LCP to complete or to need a point in time where there
     * are no LCPs ongoing.
     */
    if (check_stall_lcp_start())
    {
      c_lcpState.setLcpStatus(LCP_STATUS_IDLE, __LINE__);
      checkLcpStart(signal, __LINE__, 3000);
      return;
    }
  }
  
  if (unlikely(c_lcpState.lcpManualStallStart))
  {
    jam();
    g_eventLogger->warning("LCP start triggered, but manually stalled (Immediate %u, Change %llu / %llu)",
                           c_lcpState.immediateLcpStart,
                           Uint64(c_lcpState.ctcCounter),
                           (Uint64(1) << c_lcpState.clcpDelay));
    c_lcpState.setLcpStatus(LCP_STATUS_IDLE, __LINE__);
    checkLcpStart(signal, __LINE__, 3000);
    return;
  }

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
  c_lcpState.keepGci = (Uint32)(m_micro_gcp.m_old_gci >> 32);
  c_lcpState.oldestRestorableGci = SYSFILE->oldestRestorableGCI;

  CRASH_INSERTION(7014);
  c_lcpState.setLcpStatus(LCP_TC_CLOPSIZE, __LINE__);
  sendLoopMacro(TC_CLOPSIZEREQ, sendTC_CLOPSIZEREQ, RNIL);
}

void Dbdih::execTC_CLOPSIZECONF(Signal* signal) 
{
  jamEntry();
  Uint32 senderNodeId = signal->theData[0];
  receiveLoopMacro(TC_CLOPSIZEREQ, senderNodeId);
  
  ndbrequire(c_lcpState.lcpStatus == LCP_TC_CLOPSIZE);

  /* ----------------------------------------------------------------------- */
  /*       UPDATE THE NEW LATEST LOCAL CHECKPOINT ID.                        */
  /* ----------------------------------------------------------------------- */
  cnoOfActiveTables = 0;
  c_lcpState.setLcpStatus(LCP_WAIT_MUTEX, __LINE__);
  ndbrequire(((int)c_lcpState.oldestRestorableGci) > 0);

  if (ERROR_INSERTED(7011)) {
    signal->theData[0] = NDB_LE_LCPStoppedInCalcKeepGci;
    signal->theData[1] = 0;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);
    return;
  }//if
  start_lcp_before_mutex(signal);
}

void Dbdih::start_lcp_before_mutex(Signal *signal)
{
  /**
   * We lock the Fragment Info for at least a short time. This ensures
   * that we don't start an LCP while we are copying meta data. If we
   * support PAUSE LCP protocol we can later release the mutex early
   * on.
   */
  jam();
  Mutex mutex(signal, c_mutexMgr, c_fragmentInfoMutex_lcp);
  Callback c = { safe_cast(&Dbdih::lcpFragmentMutex_locked), 0 };
  ndbrequire(mutex.trylock(c, false));
}

void
Dbdih::lcpFragmentMutex_locked(Signal* signal, 
                               Uint32 senderData, 
                               Uint32 retVal)
{
  jamEntry();

  if (retVal == UtilLockRef::LockAlreadyHeld)
  {
    jam();
    Mutex mutex(signal, c_mutexMgr, c_fragmentInfoMutex_lcp);
    mutex.release();

    if (senderData == 0)
    {
      jam();
      infoEvent("Local checkpoint blocked waiting for node-restart");
    }
    // 2* is as parameter is in seconds, and we sendSignalWithDelay 500ms
    if (senderData >= 2*c_lcpState.m_lcp_trylock_timeout)
    {
      jam();
      Callback c = { safe_cast(&Dbdih::lcpFragmentMutex_locked), 0 };
      ndbrequire(mutex.lock(c, false));
      return;
    }
    signal->theData[0] = DihContinueB::ZLCP_TRY_LOCK;
    signal->theData[1] = senderData + 1;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 500, 2);
    return;
  }

  ndbrequire(retVal == 0);
  start_lcp(signal);
}
 
void Dbdih::start_lcp(Signal *signal)
{
  c_lcpState.m_start_time = c_current_time = NdbTick_getCurrentTicks(); 
 
  setLcpActiveStatusStart(signal);

  c_lcpState.setLcpStatus(LCP_CALCULATE_KEEP_GCI, __LINE__);
  c_lcpState.keepGci = m_micro_gcp.m_old_gci >> 32;
  c_lcpState.oldestRestorableGci = SYSFILE->oldestRestorableGCI;
  SYSFILE->latestLCP_ID++;
  
  signal->theData[0] = DihContinueB::ZCALCULATE_KEEP_GCI;
  signal->theData[1] = 0;  /* TABLE ID = 0          */
  signal->theData[2] = 0;  /* FRAGMENT ID = 0       */
  sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBB);
  return;
}

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
        checkLcpStart(signal, __LINE__, 1000);
        return;
      }//if
    }//if
    ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
    if (tabPtr.p->tabStatus != TabRecord::TS_ACTIVE || 
	tabPtr.p->tabStorage != TabRecord::ST_NORMAL) {
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
  checkKeepGci(tabPtr, fragId, fragPtr.p, fragPtr.p->storedReplicas);
  checkKeepGci(tabPtr, fragId, fragPtr.p, fragPtr.p->oldStoredReplicas);
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
  signal->theData[0] = NDB_LE_LocalCheckpointStarted; //Event type
  signal->theData[1] = SYSFILE->latestLCP_ID;
  signal->theData[2] = c_lcpState.keepGci;
  signal->theData[3] = c_lcpState.oldestRestorableGci;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 4, JBB);

  /***************************************************************************/
  // Report the event that a local checkpoint has started.
  /***************************************************************************/
  
  signal->setTrace(TestOrd::TraceLocalCheckpoint);

  CRASH_INSERTION(7013);
  SYSFILE->keepGCI = c_lcpState.keepGci;
  SYSFILE->oldestRestorableGCI = c_lcpState.oldestRestorableGci;

  const Uint32 oldestRestorableGCI = SYSFILE->oldestRestorableGCI;

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

  /**
   * This used be done in setLcpActiveStatusStart
   *   but this function has been move "up" in the flow
   *   to just before calcKeepGci
   */
  setNodeRestartInfoBits(signal);

  c_lcpState.setLcpStatus(LCP_COPY_GCI, __LINE__);
  //#ifdef VM_TRACE
  //  infoEvent("LocalCheckpoint %d started", SYSFILE->latestLCP_ID);
  //  signal->theData[0] = 7012;
  //  execDUMP_STATE_ORD(signal);
  //#endif
  
  copyGciLab(signal, CopyGCIReq::LOCAL_CHECKPOINT);
}//Dbdih::storeNewLcpIdLab()

void Dbdih::startLcpRoundLab(Signal* signal)
{
  jam();

  CRASH_INSERTION(7218);

  /**
   * Next step in starting up a local checkpoint is to define which
   * tables that should participate in the local checkpoint, while
   * we are performing this step we don't want to have committing
   * schema transactions in the middle of this, this mutex ensures
   * that we will wait for a schema transaction to commit before we
   * proceed and once we acquired the mutex, then schema transaction
   * commits will block waiting for this LCP phase to complete.
   * 
   * The reason we need this mutex is to ensure that all nodes that
   * participate in the LCP have the same view on the tables involved
   * in the LCP. This makes it possible for a node to easily take
   * over the master role in executing a LCP if the master node that
   * controls the LCP fails.
   */
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
  req->pauseStart = StartLcpReq::NormalLcpStart; /* Normal LCP start */
  sendLoopMacro(START_LCP_REQ, sendSTART_LCP_REQ, RNIL);
}

void
Dbdih::sendSTART_LCP_REQ(Signal* signal, Uint32 nodeId, Uint32 extra)
{
  BlockReference ref = calcDihBlockRef(nodeId);
  if (ERROR_INSERTED(7021) && nodeId == getOwnNodeId())
  {
    sendSignalWithDelay(ref, GSN_START_LCP_REQ, signal, 500, 
                        StartLcpReq::SignalLength);
    return;
  }
  else if (ERROR_INSERTED(7021) && ((rand() % 10) > 4))
  {
    infoEvent("Don't send START_LCP_REQ to %u", nodeId);
    return;
  }
  sendSignal(ref, GSN_START_LCP_REQ, signal, StartLcpReq::SignalLength, JBB);
}

void
Dbdih::execSTART_LCP_CONF(Signal* signal)
{
  StartLcpConf * conf = (StartLcpConf*)signal->getDataPtr();
  
  Uint32 nodeId = refToNode(conf->senderRef);

  if (is_lcp_paused())
  {
    ndbrequire(isMaster());
    if (c_pause_lcp_master_state == PAUSE_START_LCP_INCLUSION)
    {
      jam();
      /**
       * We have completed including the starting node into the LCP.
       * We now need to copy the meta data.
       * 
       * We come here as part of starting up a new starting node, so
       * we don't come here as part of a normal LCP start. So the
       * bitmap for outstanding signals we should not use since we
       * haven't set it up in this case.
       */
      c_pause_lcp_master_state = PAUSE_IN_LCP_COPY_META_DATA;
      start_copy_meta_data(signal);
      return;
    }
    else
    {
      jam();
      ndbrequire(c_pause_lcp_master_state == PAUSE_COMPLETE_LCP_INCLUSION);
      /**
       * We have completed copying the meta data and now we have also
       * completed the inclusion of the new node into the LCP protocol.
       * We are now ready to continue to the next stage of the node
       * restart handling for the starting node.
       */
      sendPAUSE_LCP_REQ(signal, false);
      return;
    }
  }
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
  
  /* ----------------------------------------------------------------------- */
  /*     NOW PROCEED BY STARTING THE LOCAL CHECKPOINT IN EACH LQH.           */
  /* ----------------------------------------------------------------------- */
  c_lcpState.m_LAST_LCP_FRAG_ORD = c_lcpState.m_participatingLQH;

  c_lcp_runs_with_pause_support = check_if_pause_lcp_possible();
  if (c_lcp_runs_with_pause_support)
  {
    jam();
    /**
     * We can release the mutex now that we have started the LCP. Since we
     * hold the mutex we know that currently no copy of meta data is ongoing.
     * We have setup everything for the LCP to start we reach this call, so it
     * is safe to release the mutex and rely on the PAUSE LCP protocol to
     * handle the rest.
     *
     * We have held the fragment info mutex long enough to ensure that we have
     * copied the m_participatingDIH bitmap to all participants in the LCP.
     * This means that when we reach the participant nodes we can safely add
     * the starting node to m_participatingDIH to ensure that the starting
     * node also gets all the rest of the updates to the LCP data in DIH
     * while the LCP is completing. This phase of the LCP is fairly quick, so
     * the cost of holding the mutex here should be fairly small. The part of
     * the LCP that consumes most time is when we start performing the real
     * checkpointing on the m_participatingLQH nodes.
     */
    Mutex mutex(signal, c_mutexMgr, c_fragmentInfoMutex_lcp);
    mutex.unlock();
  }
  CRASH_INSERTION(7015);
  c_lcpState.setLcpStatus(LCP_START_LCP_ROUND, __LINE__);
  startLcpRoundLoopLab(signal, 0, 0);
}

void
Dbdih::master_lcp_fragmentMutex_locked(Signal* signal, 
                                       Uint32 failedNodePtrI, Uint32 retVal)
{
  jamEntry();
  ndbrequire(retVal == 0);

  signal->theData[0] = NDB_LE_LCP_TakeoverCompleted;
  signal->theData[1] = c_lcpMasterTakeOverState.state;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);
  
  signal->theData[0] = 7012;
  execDUMP_STATE_ORD(signal);
  
  c_lcpMasterTakeOverState.set(LMTOS_IDLE, __LINE__);
  
  checkLocalNodefailComplete(signal, failedNodePtrI, NF_LCP_TAKE_OVER);

  startLcpRoundLoopLab(signal, 0, 0);
}


//#define DIH_DEBUG_REPLICA_SEARCH
#ifdef DIH_DEBUG_REPLICA_SEARCH
static Uint32 totalScheduled;
static Uint32 totalExamined;
#endif

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
  c_lcpState.m_allReplicasQueuedLQH.clear();

#ifdef DIH_DEBUG_REPLICA_SEARCH
  totalScheduled = totalExamined = 0;
#endif

  startNextChkpt(signal);
}//Dbdih::startLcpRoundLoopLab()

void Dbdih::startNextChkpt(Signal* signal)
{
  jam();
  const bool allReplicaCheckpointsQueued = 
    c_lcpState.m_allReplicasQueuedLQH.
    contains(c_lcpState.m_participatingLQH);
  
  if (allReplicaCheckpointsQueued)
  {
    jam();

    /**
     * No need to find new checkpoints to start,
     * just waiting for completion
     */

    sendLastLCP_FRAG_ORD(signal);
    return;
  }

  Uint32 lcpId = SYSFILE->latestLCP_ID;

  /* Initialise handledNodes with those already fully queued */
  NdbNodeBitmask handledNodes = c_lcpState.m_allReplicasQueuedLQH; 
  
  /* Remove any that have failed in the interim */
  handledNodes.bitAND(c_lcpState.m_participatingLQH);
  
  const Uint32 lcpNodes = c_lcpState.m_participatingLQH.count();
  
  bool save = true;
  LcpState::CurrentFragment curr = c_lcpState.currentFragment;

#ifdef DIH_DEBUG_REPLICA_SEARCH
  Uint32 examined = 0;
  Uint32 started = 0;
  Uint32 queued = 0;
#endif
  
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
	replicaPtr.i = replicaPtr.p->nextPool){
      
      jam();
      c_replicaRecordPool.getPtr(replicaPtr);

#ifdef DIH_DEBUG_REPLICA_SEARCH
      examined++;
#endif
      
      NodeRecordPtr nodePtr;
      nodePtr.i = replicaPtr.p->procNode;
      ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
      
      if (c_lcpState.m_participatingLQH.get(nodePtr.i))
      {
	if (replicaPtr.p->lcpOngoingFlag &&
	    replicaPtr.p->lcpIdStarted < lcpId) 
	{
	  jam();
	  //-------------------------------------------------------------------
	  // We have found a replica on a node that performs local checkpoint
	  // that is alive and that have not yet been started.
	  //-------------------------------------------------------------------
	  
          if (nodePtr.p->noOfStartedChkpt <
              getMaxStartedFragCheckpointsForNode(nodePtr.i))
	  {
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

#ifdef DIH_DEBUG_REPLICA_SEARCH
            started++;
#endif
	  } 
          else if (nodePtr.p->noOfQueuedChkpt <
                   MAX_QUEUED_FRAG_CHECKPOINTS_PER_NODE)
	  {
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
#ifdef DIH_DEBUG_REPLICA_SEARCH
            queued++;
#endif
	  }
	  else 
	  {
	    jam();
	    
	    if(save)
	    {
              /**
               * Stop increasing value on first replica that 
               * we could not enqueue, so we don't miss it 
               * next time
               */
              c_lcpState.currentFragment = curr;
              save = false;
            }
	    
	    handledNodes.set(nodePtr.i);
	    if (handledNodes.count() == lcpNodes)
	    {
              /**
               * All participating nodes have either
               * - Full queues
               * - All available replica checkpoints queued
               *   (m_allReplicasQueuedLQH)
               *
               * Therefore, exit the search here.
               */
#ifdef DIH_DEBUG_REPLICA_SEARCH
              ndbout_c("Search : All nodes busy.  Examined %u Started %u Queued %u",
                       examined, started, queued);
              totalExamined+= examined;
              totalScheduled += (started + queued);
#endif
	      return;
	    }//if
	  }//if
	}
      }//while
    }
    curr.fragmentId++;
    if (curr.fragmentId >= tabPtr.p->totalfragments) {
      jam();
      curr.fragmentId = 0;
      curr.tableId++;
    }//if
  }//while

#ifdef DIH_DEBUG_REPLICA_SEARCH
  ndbout_c("Search : At least one node not busy.  Examined %u Started %u Queued %u",
           examined, started, queued);
  totalExamined+= examined;
  totalScheduled += (started + queued);
#endif

  /**
   * Have examined all replicas and attempted to 
   * enqueue as many replica LCPs as possible,
   * without filling all queues.
   * This means that some node(s) have no more
   * replica LCPs to be enqueued.
   * These are the node(s) which are *not* in
   * the handled bitmap on this round.
   * We keep track of these to allow the search
   * to exit early on future invocations.
   */
  
  /* Invert handled nodes to reveal newly finished nodes */
  handledNodes.bitXOR(c_lcpState.m_participatingLQH);
  
  /* Add newly finished nodes to the global state */
  c_lcpState.m_allReplicasQueuedLQH.bitOR(handledNodes);
  
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

      CRASH_INSERTION(7193);
      BlockReference ref = calcLqhBlockRef(nodePtr.i);
      sendSignal(ref, GSN_LCP_FRAG_ORD, signal,LcpFragOrd::SignalLength, JBB);
    }
  }
  if(ERROR_INSERTED(7075))
  {
    if(c_lcpState.m_LAST_LCP_FRAG_ORD.done())
    {
      CRASH_INSERTION(7075);
    }
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

  LcpFragRep * lcpReport = (LcpFragRep *)&signal->theData[0];

  /**
   * Proxying LCP_FRAG_REP
   */
  const bool broadcast_req = lcpReport->nodeId == LcpFragRep::BROADCAST_REQ;
  if (broadcast_req)
  {
    jam();
    ndbrequire(refToNode(signal->getSendersBlockRef()) == getOwnNodeId());

    /**
     * Set correct nodeId
     */
    lcpReport->nodeId = getOwnNodeId();

    if (is_lcp_paused() || c_dequeue_lcp_rep_ongoing)
    {
      jam();
      /**
       * We are currently pausing sending all information about LCP_FRAG_REP
       * from this node and also pausing any local processing of signals
       * received from LQH. We can still handle messages from other DIH
       * nodes. These will eventually stop due to pausing and we will wait
       * until we know that all those signals have arrived at their
       * destination.
       *
       * We won't send anything until we have completed the
       * PAUSE_LCP_REQ protocol which means until the starting node have
       * received all the meta data from the master node.
       */
      queue_lcp_frag_rep(signal, lcpReport);
      return;
    }
    NodeReceiverGroup rg(DBDIH, c_lcpState.m_participatingDIH);
    rg.m_nodes.clear(getOwnNodeId());
    sendSignal(rg, GSN_LCP_FRAG_REP, signal, signal->getLength(), JBB);  

    /**
     * and continue processing
     */
  }

  Uint32 nodeId = lcpReport->nodeId;
  Uint32 tableId = lcpReport->tableId;
  Uint32 fragId = lcpReport->fragId;

  /**
   * We can receive LCP_FRAG_REP in 2 different situations:
   * 1) signal->length() == SignalLength
   * A normal report of completion of a LCP on a specific fragment. This
   * cannot arrive when the node is down, the sending must be in
   * the m_participatingLQH set, in addition the node must be alive
   * in the DIH sense which means that it has passed the state where it
   * is included in all the LCP protocols and GCP protocols.
   *
   * 2) signal->length() == SignalLengthTQ && lcpReport->fromTQ == 1
   * This signal is sent when the table is in copy state when a signal
   * in 1) is received. In this case the node could die before we
   * arrive here. We check this by simply checking if the node is still
   * alive. If this happens we can simply drop the signal.
   */
  if (!checkNodeAlive(nodeId))
  {
    jam();
    ndbrequire(signal->length() == LcpFragRep::SignalLengthTQ &&
               lcpReport->fromTQ == Uint32(1));
    /**
     * Given that we can delay this signal during a table copy situation,
     * we can actually receive this signal when the node is already dead. If
     * the node is dead then we drop the signal as soon as possible, the node
     * failure handling will ensure that the node is properly handled anyways.
     */
    return;
  }

  ndbrequire(c_lcpState.lcpStatus != LCP_STATUS_IDLE);
  
#if 0
  printLCP_FRAG_REP(stdout, 
		    signal->getDataPtr(),
		    signal->length(), number());
#endif  

  jamEntry();

  if (ERROR_INSERTED(7178) && nodeId != getOwnNodeId())
  {
    jam();
    Uint32 owng =Sysfile::getNodeGroup(getOwnNodeId(), SYSFILE->nodeGroups);
    Uint32 nodeg = Sysfile::getNodeGroup(nodeId, SYSFILE->nodeGroups);
    if (owng == nodeg)
    {
      jam();
      ndbout_c("throwing away LCP_FRAG_REP from  (and killing) %d", nodeId);
      SET_ERROR_INSERT_VALUE(7179);
      signal->theData[0] = 9999;
      sendSignal(numberToRef(CMVMI, nodeId), 
		 GSN_NDB_TAMPER, signal, 1, JBA);  
      return;
    }
  }
 
  if (ERROR_INSERTED(7179) && nodeId != getOwnNodeId())
  {
    jam();
    Uint32 owng =Sysfile::getNodeGroup(getOwnNodeId(), SYSFILE->nodeGroups);
    Uint32 nodeg = Sysfile::getNodeGroup(nodeId, SYSFILE->nodeGroups);
    if (owng == nodeg)
    {
      jam();
      ndbout_c("throwing away LCP_FRAG_REP from %d", nodeId);
      return;
    }
  }    

  CRASH_INSERTION2(7025, isMaster());
  CRASH_INSERTION2(7016, !isMaster());
  CRASH_INSERTION2(7191, (!isMaster() && tableId));

  bool fromTimeQueue = (signal->length() == LcpFragRep::SignalLengthTQ &&
                        lcpReport->fromTQ == Uint32(1) &&
                        !broadcast_req);
  
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
      is very unlikely to happen but it has and it results in a node crash. 
      This should be considered a "quick fix" and not a permanent solution. 
      A cleaner/better way would be to check the time queue if it is full or
      not before sending this signal.
    */
    lcpReport->fromTQ = Uint32(1);
    sendSignal(reference(), GSN_LCP_FRAG_REP, signal,
               LcpFragRep::SignalLengthTQ, JBB);
    /* Kept here for reference
       sendSignalWithDelay(reference(), GSN_LCP_FRAG_REP, 
       signal, 20, signal->length());
    */

    if(!fromTimeQueue){
      c_lcpState.noOfLcpFragRepOutstanding++;
    }    
    
    return;
  }//if
  
  if(fromTimeQueue)
  {
    jam();
    ndbrequire(c_lcpState.noOfLcpFragRepOutstanding > 0);
    c_lcpState.noOfLcpFragRepOutstanding--;
  }

  bool tableDone = reportLcpCompletion(lcpReport);
  
  Uint32 started = lcpReport->maxGciStarted;
#ifdef VM_TRACE
  Uint32 completed = lcpReport->maxGciCompleted;
#endif

  if (started > c_lcpState.lcpStopGcp)
  {
    jam();
    c_lcpState.lcpStopGcp = started;
  }

  /**
   * Update m_local_lcp_state
   *
   * we could only look fragments that we have locally...
   *   but for now we look at all fragments
   */
  m_local_lcp_state.lcp_frag_rep(lcpReport);

  if (tableDone)
  {
    jam();

    if (tabPtr.p->tabStatus == TabRecord::TS_IDLE ||
        tabPtr.p->tabStatus == TabRecord::TS_DROPPING)
    {
      jam();
      g_eventLogger->info("TS_DROPPING - Neglecting to save Table: %d Frag: %d - ",
                          tableId, fragId);
    }
    else
    {
      jam();
      /**
       * Write table description to file
       */
      tabPtr.p->tabLcpStatus = TabRecord::TLS_WRITING_TO_FILE;
      tabPtr.p->tabCopyStatus = TabRecord::CS_LCP_READ_TABLE;

      /**
       * Check whether we should write immediately, or queue...
       */
      if (c_lcpTabDefWritesControl.requestMustQueue())
      {
        jam();
        //ndbout_c("DIH : Queueing tab def flush op on table %u", tabPtr.i);
        /* Mark as queued - will be started when an already running op completes */
        tabPtr.p->tabUpdateState = TabRecord::US_LOCAL_CHECKPOINT_QUEUED;
      }
      else
      {
        /* Run immediately */
        jam();
        tabPtr.p->tabUpdateState = TabRecord::US_LOCAL_CHECKPOINT;
        signal->theData[0] = DihContinueB::ZPACK_TABLE_INTO_PAGES;
        signal->theData[1] = tabPtr.i;
        sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
      }
      
      bool ret = checkLcpAllTablesDoneInLqh(__LINE__);
      if (ret && ERROR_INSERTED(7209))
      {
        jam();
        CLEAR_ERROR_INSERT_VALUE;
        signal->theData[0] = 9999;
        sendSignal(numberToRef(CMVMI, cmasterNodeId), 
                   GSN_NDB_TAMPER, signal, 1, JBB);
      }
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
    /**
     * In the old code we ensured that all outstanding LCP_FRAG_REPs
     * were handled before entering those states. So receiving an
     * LCP_FRAG_REP is ok in new code, even in new code will block
     * LCP_COMPLETE_REP such that we don't complete an LCP while
     * processing a master take over. But we can still receive
     * LCP_FRAG_REP while processing a master takeover.
     *
     * In old code we were blocked from coming here for LCP_FRAG_REPs since
     * we ensured that we don't proceed here until all nodes have sent
     * their EMPTY_LCP_CONF to us. So we keep ndbrequire to ensure that
     * we come here only when running the new master take over code.
     */
    ndbrequire(!c_lcpMasterTakeOverState.use_empty_lcp);
    return;
  }
  ndbrequire(ok);
  
  /* ----------------------------------------------------------------------- */
  // Check if there are more LCP's to start up.
  /* ----------------------------------------------------------------------- */
  if(isMaster())
  {
    jam();

    /**
     * Remove from "running" array
     */
    NodeRecordPtr nodePtr;
    nodePtr.i = nodeId;
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
    
    const Uint32 outstanding = nodePtr.p->noOfStartedChkpt;
    if (outstanding > 0)
    {
      jam();
      bool found = false;
      for (Uint32 i = 0; i < outstanding; i++)
      {
        if (found)
        {
          jam();
          nodePtr.p->startedChkpt[i - 1] = nodePtr.p->startedChkpt[i];
          continue;
        }
        if(nodePtr.p->startedChkpt[i].tableId != tableId ||
           nodePtr.p->startedChkpt[i].fragId != fragId)
        {
          jam();
          continue;
        }
        jam();
        found = true;
      }
      if (found)
      {
        jam();
        nodePtr.p->noOfStartedChkpt--;
        checkStartMoreLcp(signal, nodeId);
        return;
      }
    }
    const Uint32 outstanding_queued = nodePtr.p->noOfQueuedChkpt;
    if (outstanding_queued > 0)
    {
      jam();
      bool found = false;
      for (Uint32 i = 0; i < outstanding_queued; i++)
      {
        if (found)
        {
          jam();
          nodePtr.p->queuedChkpt[i - 1] = nodePtr.p->queuedChkpt[i];
          continue;
        }
        if(nodePtr.p->queuedChkpt[i].tableId != tableId ||
           nodePtr.p->queuedChkpt[i].fragId != fragId)
        {
          jam();
          continue;
        }
        jam();
        found = true;
      }
      if (found)
      {
        jam();
        nodePtr.p->noOfQueuedChkpt--;
        return;
      }
    }
    /**
     * In a master takeover situation we might have the fragment replica
     * placed in the queue as well. It is possible that the old master
     * did send LCP_FRAG_ORD and it is now arriving here.
     *
     * We start by checking the queued list, if it is in neither the
     * queued nor in the started list, then the table is dropped. There
     * is also one more obscure variant when the old master had a deeper
     * queue than we have, in that case we could come here, to handle
     * that we only assert on that the table is dropped.
     */
    ndbassert(tabPtr.p->tabStatus == TabRecord::TS_IDLE ||
              tabPtr.p->tabStatus == TabRecord::TS_DROPPING);
  }
}

bool
Dbdih::checkLcpAllTablesDoneInLqh(Uint32 line){
  TabRecordPtr tabPtr;

  /**
   * Check if finished with all tables
   */
  for (tabPtr.i = 0; tabPtr.i < ctabFileSize; tabPtr.i++) {
    //jam(); Removed as it flushed all other jam traces.
    ptrAss(tabPtr, tabRecord);
    if ((tabPtr.p->tabStatus == TabRecord::TS_ACTIVE) &&
        (tabPtr.p->tabLcpStatus == TabRecord::TLS_ACTIVE))
    {
      jam();
      /**
       * Nope, not finished with all tables
       */
      return false;
    }//if
  }//for
  
  CRASH_INSERTION2(7026, isMaster());
  CRASH_INSERTION2(7017, !isMaster());
  
  c_lcpState.setLcpStatus(LCP_TAB_COMPLETED, line);

  if (ERROR_INSERTED(7194))
  {
    ndbout_c("CLEARING 7194");
    CLEAR_ERROR_INSERT_VALUE;
  }

#ifdef DIH_DEBUG_REPLICA_SEARCH
  if (totalScheduled == 0)
  {
    totalScheduled = 1;
  }
  ndbout_c("LCP complete.  Examined %u replicas, scheduled %u.  Ratio : %u.%u",
           totalExamined,
           totalScheduled,
           totalExamined/totalScheduled,
           (10 * (totalExamined - 
                  ((totalExamined/totalScheduled) *
                   totalScheduled)))/
           totalScheduled);
#endif
  
  return true;
}

void Dbdih::findReplica(ReplicaRecordPtr& replicaPtr, 
			Fragmentstore* fragPtrP, 
			Uint32 nodeId,
			bool old)
{
  replicaPtr.i = old ? fragPtrP->oldStoredReplicas : fragPtrP->storedReplicas;
  while(replicaPtr.i != RNIL){
    c_replicaRecordPool.getPtr(replicaPtr);
    if (replicaPtr.p->procNode == nodeId) {
      jam();
      return;
    } else {
      jam();
      replicaPtr.i = replicaPtr.p->nextPool;
    }//if
  };

#ifdef VM_TRACE
  g_eventLogger->info("Fragment Replica(node=%d) not found", nodeId);
  replicaPtr.i = fragPtrP->oldStoredReplicas;
  while(replicaPtr.i != RNIL){
    c_replicaRecordPool.getPtr(replicaPtr);
    if (replicaPtr.p->procNode == nodeId) {
      jam();
      break;
    } else {
      jam();
      replicaPtr.i = replicaPtr.p->nextPool;
    }//if
  };
  if(replicaPtr.i != RNIL){
    g_eventLogger->info("...But was found in oldStoredReplicas");
  } else {
    g_eventLogger->info("...And wasn't found in oldStoredReplicas");
  }
#endif
  ndbrequire(false);
}//Dbdih::findReplica()


int
Dbdih::handle_invalid_lcp_no(const LcpFragRep* rep, 
			     ReplicaRecordPtr replicaPtr)
{
  ndbrequire(!isMaster());
  Uint32 lcpNo = rep->lcpNo;
  Uint32 lcpId = rep->lcpId;

  if (!ndb_pnr(getNodeInfo(refToNode(cmasterdihref)).m_version))
  {
  }
  else
  {
    warningEvent("Detected previous node failure of %d during lcp",
                 rep->nodeId);
  }
  
  replicaPtr.p->nextLcp = lcpNo;
  replicaPtr.p->lcpId[lcpNo] = 0;
  replicaPtr.p->lcpStatus[lcpNo] = ZINVALID;
  
  for (Uint32 i = lcpNo; i != lcpNo; i = nextLcpNo(i))
  {
    jam();
    if (replicaPtr.p->lcpStatus[i] == ZVALID &&
	replicaPtr.p->lcpId[i] >= lcpId)
    {
      ndbout_c("i: %d lcpId: %d", i, replicaPtr.p->lcpId[i]);
      ndbrequire(false);
    }
  }

  return 0;
}

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
  
  if (tabPtr.p->tabStatus == TabRecord::TS_DROPPING ||
      tabPtr.p->tabStatus == TabRecord::TS_IDLE)
  {
    jam();
    return true;
  }

  FragmentstorePtr fragPtr;
  getFragstore(tabPtr.p, fragId, fragPtr);
  
  ReplicaRecordPtr replicaPtr;
  findReplica(replicaPtr, fragPtr.p, nodeId);
  
  ndbrequire(replicaPtr.p->lcpOngoingFlag == true);
  if(lcpNo != replicaPtr.p->nextLcp){
    if (handle_invalid_lcp_no(lcpReport, replicaPtr))
    {
      g_eventLogger->error("lcpNo = %d replicaPtr.p->nextLcp = %d",
                           lcpNo, replicaPtr.p->nextLcp);
      ndbrequire(false);
    }
  }
  ndbrequire(lcpNo == replicaPtr.p->nextLcp);
  ndbrequire(lcpNo < MAX_LCP_STORED);
  ndbrequire(replicaPtr.p->lcpId[lcpNo] != lcpId);
  
  replicaPtr.p->lcpIdStarted = lcpId;
  replicaPtr.p->lcpOngoingFlag = false;
  
  removeOldCrashedReplicas(tableId, fragId, replicaPtr);
  replicaPtr.p->lcpId[lcpNo] = lcpId;
  replicaPtr.p->lcpStatus[lcpNo] = ZVALID;
  replicaPtr.p->maxGciStarted[lcpNo] = maxGciStarted;
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
        g_eventLogger->info("reportLcpCompletion: fragment %d not ready", fid);
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
  
  ndbrequire(nodePtr.p->noOfStartedChkpt <
             getMaxStartedFragCheckpointsForNode(nodePtr.i));
  
  if (nodePtr.p->noOfQueuedChkpt > 0) {
    jam();
    Uint32 startIndex = nodePtr.p->noOfStartedChkpt;
    nodePtr.p->startedChkpt[startIndex] = nodePtr.p->queuedChkpt[0];
    for (Uint32 i = 1; i < nodePtr.p->noOfQueuedChkpt; i++)
    {
      nodePtr.p->queuedChkpt[i - 1] = nodePtr.p->queuedChkpt[i];
    }
    nodePtr.p->noOfQueuedChkpt--;
    nodePtr.p->noOfStartedChkpt++;
    //-------------------------------------------------------------------
    // We can send a LCP_FRAG_ORD to the node ordering it to perform a
    // local checkpoint on this fragment replica.
    //-------------------------------------------------------------------
    
    sendLCP_FRAG_ORD(signal, nodePtr.p->startedChkpt[startIndex]);
    return;
  }

  /* ----------------------------------------------------------------------- */
  // If this node has no checkpoints queued up, then attempt to re-fill the
  // queues across all nodes.
  // The search for next replicas can be expensive, so we only do it when
  // the queues are empty.
  /* ----------------------------------------------------------------------- */
  startNextChkpt(signal);
}//Dbdih::checkStartMoreLcp()

void
Dbdih::sendLCP_FRAG_ORD(Signal* signal, 
			NodeRecord::FragmentCheckpointInfo info){ 
  
  ReplicaRecordPtr replicaPtr;
  replicaPtr.i = info.replicaPtr;
  c_replicaRecordPool.getPtr(replicaPtr);
  
  // MT LQH goes via proxy for DD reasons
  BlockReference ref = calcLqhBlockRef(replicaPtr.p->procNode);
  
  if (ERROR_INSERTED(7193) && replicaPtr.p->procNode == getOwnNodeId())
  {
    return;
  }
  
  if (replicaPtr.p->nextLcp >= MAX_LCP_USED)
  {
    jam();
    infoEvent("Updating nextLcp from %u to %u tab: %u", 
              replicaPtr.p->nextLcp, 0,
              info.tableId);
    replicaPtr.p->nextLcp = 0;
  }

  Uint32 keepGci = c_lcpState.keepGci;
  if (keepGci > SYSFILE->lastCompletedGCI[replicaPtr.p->procNode])
  {
    jam();
    keepGci = SYSFILE->lastCompletedGCI[replicaPtr.p->procNode];
  }
  
  LcpFragOrd * const lcpFragOrd = (LcpFragOrd *)&signal->theData[0];
  lcpFragOrd->tableId    = info.tableId;
  lcpFragOrd->fragmentId = info.fragId;
  lcpFragOrd->lcpId      = SYSFILE->latestLCP_ID;
  lcpFragOrd->lcpNo      = replicaPtr.p->nextLcp;
  lcpFragOrd->keepGci    = keepGci;
  lcpFragOrd->lastFragmentFlag = false;
  sendSignal(ref, GSN_LCP_FRAG_ORD, signal, LcpFragOrd::SignalLength, JBB);
}

void Dbdih::checkLcpCompletedLab(Signal* signal) 
{
  if(c_lcpState.lcpStatus < LCP_TAB_COMPLETED)
  {
    jam();
    return;
  }
  
  /**
   * We only wait for completion of tables that are not in a dropping state.
   * This is to avoid that LCPs are being blocked by dropped tables. There
   * could be bugs in reporting dropped tables properly.
   */
  TabRecordPtr tabPtr;
  for (tabPtr.i = 0; tabPtr.i < ctabFileSize; tabPtr.i++)
  {
    //jam(); Removed as it flushed all other jam traces.
    ptrAss(tabPtr, tabRecord);
    if (tabPtr.p->tabLcpStatus != TabRecord::TLS_COMPLETED)
    {
      jam();
      return;
    }
  }

  CRASH_INSERTION2(7027, isMaster());
  CRASH_INSERTION2(7018, !isMaster());

  if(c_lcpState.lcpStatus == LCP_TAB_COMPLETED)
  {
    /**
     * We're done
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

  /**
   * Quick and dirty fix for bug#36276 dont save
   * LCP_COMPLETE_REP to same node same LCP twice
   */
  bool alreadysent = 
    c_lcpState.m_lastLCP_COMPLETE_REP_id == SYSFILE->latestLCP_ID &&
    c_lcpState.m_lastLCP_COMPLETE_REP_ref == c_lcpState.m_masterLcpDihRef;

  if (!alreadysent)
  {
    LcpCompleteRep * rep = (LcpCompleteRep*)signal->getDataPtrSend();
    rep->nodeId = getOwnNodeId();
    rep->lcpId = SYSFILE->latestLCP_ID;
    rep->blockNo = DBDIH;
    
    sendSignal(c_lcpState.m_masterLcpDihRef, GSN_LCP_COMPLETE_REP, signal, 
               LcpCompleteRep::SignalLength, JBB);

    c_lcpState.m_lastLCP_COMPLETE_REP_id = SYSFILE->latestLCP_ID;
    c_lcpState.m_lastLCP_COMPLETE_REP_ref = c_lcpState.m_masterLcpDihRef;
  }

  /**
   * Say that an initial node restart does not need to be redone
   *   once node has been part of first LCP
   */
  if (c_set_initial_start_flag &&
      c_lcpState.m_participatingLQH.get(getOwnNodeId()))
  {
    jam();
    c_set_initial_start_flag = FALSE;
  }
}

/*-------------------------------------------------------------------------- */
/* COMP_LCP_ROUND                   A LQH HAS COMPLETED A LOCAL CHECKPOINT  */
/*------------------------------------------------------------------------- */
void Dbdih::execLCP_COMPLETE_REP(Signal* signal) 
{
  jamEntry();

  CRASH_INSERTION(7191);

#if 0
  g_eventLogger->info("LCP_COMPLETE_REP"); 
  printLCP_COMPLETE_REP(stdout, 
			signal->getDataPtr(),
			signal->length(), number());
#endif

  LcpCompleteRep * rep = (LcpCompleteRep*)signal->getDataPtr();

  if (rep->nodeId == LcpFragRep::BROADCAST_REQ)
  {
    jam();
    ndbrequire(refToNode(signal->getSendersBlockRef()) == getOwnNodeId());
    
    /**
     * Set correct nodeId
     */
    rep->nodeId = getOwnNodeId();

    /**
     * We want to ensure that we don't receive multiple LCP_COMPLETE_REP
     * from our LQH for the same LCP id. This wouldn't fly with the
     * PAUSE LCP protocol handling.
     */
    ndbrequire(rep->blockNo == DBLQH);
    ndbrequire(c_last_id_lcp_complete_rep != rep->lcpId ||
               c_last_id_lcp_complete_rep == RNIL);
    c_last_id_lcp_complete_rep = rep->lcpId;
    if (is_lcp_paused() || c_dequeue_lcp_rep_ongoing)
    {
      jam();
      /**
       * Also the LCP_COMPLETE_REP are queued when we pause the LCP reporting.
       */
      queue_lcp_complete_rep(signal, rep->lcpId);
      return;
    }
    NodeReceiverGroup rg(DBDIH, c_lcpState.m_participatingDIH);
    rg.m_nodes.clear(getOwnNodeId());
    sendSignal(rg, GSN_LCP_COMPLETE_REP, signal, signal->getLength(), JBB);  
    
    /**
     * and continue processing
     */
  }
  
  Uint32 lcpId = rep->lcpId;
  Uint32 nodeId = rep->nodeId;
  Uint32 blockNo = rep->blockNo;

  /**
   * We can arrive here in the following cases:
   * 1) blockNo == DBLQH and signal->length() == SignalLength
   *
   * This is a normal message from a node in the m_participatingLQH
   * bitmap. It indicates that the node has completed everything of
   * its processing in DBLQH, both sending all LCP_FRAG_REP and
   * handling the UNDO log. The sender must be in the set of
   * c_lcpState.m_LCP_COMPLETE_REP_Counter_LQH waited for.
   *
   * There is an exception for this during master takeover, another node
   * might send LCP_COMPLETE_REP after receiving MASTER_LCPREQ and finalising
   * its part of the master takeover protocol. This signal might arrive
   * before we have completed the master takeover protocol. In this case
   * the signal must be delayed until the master takeover handling is
   * completed. One reason for this is that we haven't finalised setting
   * up the master bitmaps yet.
   *
   * We know in this case that the node is alive by assumption that
   * we don't receive messages from dead nodes.
   *
   * 2) blockNo == DBLQH and signal->length() == SignalLengthTQ and
   *    rep->fromTQ == 0
   *
   * This signal is sent from NODE_FAILREP. It should be allowed to
   * pass through although the node is already declared dead and
   * no longer part of the m_participatingLQH set. It is a vital part
   * of the node failure handling. It should also not be blocked by
   * an early starting master takeover. It should however be dropped
   * if it isn't part of the set waited for (can happen if 3) arrives
   * after NODE_FAILREP but before this signal).
   *
   * This signal cannot be delayed by a master takeover. We know that
   * the master takeover state should not be possible to go beyond
   * LMTOS_INITIAL.
   *
   * 3) blockNo == DBLQH and signal->length() == SignalLengthTQ and
   *    rep->fromTQ == 1
   *
   * This signal is sent as a delayed signal when signal 1) above is
   * received in the middle of processing a master take over.
   * If it is received when the node is already dead (removed from
   * the m_participatingLQH set), then we should simply ignore it
   * and drop the signal since the node failure handling already
   * has handled it. We find this out by checking if the node is
   * part of the c_lcpState.m_LCP_COMPLETE_REP_Counter_LQH set or
   * not.
   *
   * This signal can be delayed by a master takeover if it is not
   * to be dropped.
   *
   * 4) blockNo == DBDIH and signal->length() == SignalLength
   *
   * This is a normal signal sent from one of the nodes when it has
   * received LCP_COMPLETE_REP from all participating LQHs. It is
   * received from a node in the set of
   * c_lcpState.m_LCP_COMPLETE_REP_DIH_Counter. This set ensures that we
   * only receive one of these. We should never receive this signal if
   * the node isn't in the above set. The duplication of this signal
   * happens as part of executing NODE_FAILREP, but here we set
   * signal->length() to SignalLengthTQ and fromTQ = 0, so only that
   * signal can be arriving with the node not being part of this set.
   * The sending node can both be an alive node and a starting node
   * which hasn't been set to alive yet.
   *
   * The same principle applies as in 1) here, the signal could arrive
   * during master takeover when we haven't yet formed the correct
   * c_lcpState.m_LCP_COMPLETE_REP_Counter_DIH set. In this case we need
   * to delay the signal until the master takeover is completed.
   *
   * 5) blockNo == DBDIH and signal->length() == SignalLengthTQ and
   *    rep->fromTQ == 0
   *
   * This is sent from node failure processing when the node has died.
   * The same logic as in 6) applies, the signal can be dropped if the
   * node isn't part of the c_lcpState.m_LCP_COMPLETE_REP_Counter_DIH set.
   * Otherwise it should be allowed to pass through.
   *
   * This signal cannot be delayed by the master takeover.
   *
   * 6) blockNo == DBDIH and signal->length() == SignalLengthTQ and
   *    rep->fromTQ == 1
   *
   * This is a signal sent as delayed after receiving 4) above in a master
   * takeover situation, if it arrives when the node is no
   * longer part of the c_lcpState.m_LCP_COMPLETE_REP_Counter_DIH set,
   * then we know that the signal is a duplicate and has already been
   * processed and we can safely ignore it.
   *
   * This signal can be delayed by a master takeover if it is not
   * to be dropped.
   *
   * 7) blockNo == 0 and signal->length() == SignalLength
   * This is a signal from the master indicating that the LCP is completely
   * done. It should not be possible to receive it during a master takeover
   * and thus should never be allowed to be delayed since if the master
   * takeover is being processed, then this signal cannot arrive from the
   * dead master and it is too early to receive it from the new master.
   */

  if (blockNo == DBLQH &&
      signal->length() == LcpCompleteRep::SignalLengthTQ &&
      rep->fromTQ == Uint32(0))
  {
    /* Handle case 2) above */
    ndbrequire(c_lcpMasterTakeOverState.state <= LMTOS_INITIAL);
    if (!c_lcpState.m_LCP_COMPLETE_REP_Counter_LQH.isWaitingFor(nodeId))
    {
      jam();
      return;
    }
    jam();
  }
  else if (blockNo == DBDIH &&
           signal->length() == LcpCompleteRep::SignalLengthTQ &&
           rep->fromTQ == Uint32(0))
  {
    /* Handle case 5) above */
    ndbrequire(c_lcpMasterTakeOverState.state <= LMTOS_INITIAL);
    if (!c_lcpState.m_LCP_COMPLETE_REP_Counter_DIH.isWaitingFor(nodeId))
    {
      jam();
      return;
    }
    jam();
  }
  else if (blockNo == 0)
  {
    /* Handle case 7) above) */
    jam();
    ndbrequire(signal->length() == LcpCompleteRep::SignalLength);
    /**
     * Always allowed free pass through for signals from master that LCP is
     * completed.
     * These signals should not be blocked by master takeover since the
     * master is the last node to complete master takeover and the master
     * is sending this signal.
     */
  }
  else
  {
    /* Handle case 1), case 3), case 4) and case 6) above */
    jam();
    ndbrequire(blockNo == DBDIH || blockNo == DBLQH);
    if(c_lcpMasterTakeOverState.state > LMTOS_WAIT_LCP_FRAG_REP)
    {
      jam();
      /**
       * Don't allow LCP_COMPLETE_REP to arrive during
       * LCP master take over. We haven't yet formed the set of
       * expected signals and we don't want the master state to go to
       * completed while we are forming the state.
       *
       * We keep this even when removing the need to use the EMPTY_LCP_REQ
       * protocol. The reason is that we don't want to handle code to
       * process LCP completion as part of master take over as a
       * simplification. It is perfectly doable but we opted for keeping
       * this variant.
       */
      ndbrequire(isMaster());
      rep->fromTQ = Uint32(1);
      sendSignalWithDelay(reference(), GSN_LCP_COMPLETE_REP, signal, 100,
                          LcpCompleteRep::SignalLengthTQ);
      return;
    }
    /**
     * We are not in a master takeover situation, so we should have the
     * signal expected by the sets, however this could have been handled
     * by the signal sent from NODE_FAILREP already. So we need to verify
     * we really are in those sets. Not being in those states when a master
     * takeover isn't ongoing should only happen for delayed signals.
     */
    if (blockNo == DBLQH &&
        !c_lcpState.m_LCP_COMPLETE_REP_Counter_LQH.isWaitingFor(nodeId))
    {
      /* Can happen in case 3) above */
      jam();
      ndbrequire(signal->length() == LcpCompleteRep::SignalLengthTQ &&
                 rep->fromTQ == Uint32(1));
      return;
    }
    if (blockNo == DBDIH &&
        !c_lcpState.m_LCP_COMPLETE_REP_Counter_DIH.isWaitingFor(nodeId))
    {
      /* Can happen in case 6) above */
      jam();
      ndbrequire(signal->length() == LcpCompleteRep::SignalLengthTQ &&
                 rep->fromTQ == Uint32(1));
      return;
    }
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
     * Wait until master DIH has signalled lcp is complete
     */
    return;
  }

  if(c_lcpMasterTakeOverState.state != LMTOS_IDLE){
    jam();
#ifdef VM_TRACE
    g_eventLogger->info("Exiting from allNodesLcpCompletedLab");
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

  /* Check pause states */
  check_pause_state_lcp_idle();
  c_lcpState.setLcpStatus(LCP_STATUS_IDLE, __LINE__);
  c_increase_lcp_speed_after_nf = false;

  /**
   * Update m_local_lcp_state
   */
  m_local_lcp_state.lcp_complete_rep(c_newest_restorable_gci);
  
  if (isMaster())
  {
    /**
     * Check for any "completed" TO
     */
    TakeOverRecordPtr takeOverPtr;
    for (c_masterActiveTakeOverList.first(takeOverPtr); !takeOverPtr.isNull();)
    {
      jam();

      // move to next, since takeOverPtr might be release below
      TakeOverRecordPtr nextPtr = takeOverPtr;
      c_masterActiveTakeOverList.next(nextPtr);

      Ptr<NodeRecord> nodePtr;
      nodePtr.i = takeOverPtr.p->toStartingNode;
      if (takeOverPtr.p->toMasterStatus == TakeOverRecord::TO_WAIT_LCP)
      {
        jam();
        if (c_lcpState.m_participatingLQH.get(nodePtr.i))
        {
          jam();
          ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);   
          ndbrequire(nodePtr.p->copyCompleted == 2);

          /**
           * We have completed the node restart for this node. We set the
           * node recovery status to completed. This is used also in
           * estimating times for other nodes to complete their restarts.
           * It is also used to build NDBINFO table about node restart
           * status.
           *
           * This code is only executed in master node.
           */
          setNodeRecoveryStatus(nodePtr.i, NodeRecord::WAIT_SUMA_HANDOVER);

          EndToConf * conf = (EndToConf *)signal->getDataPtrSend();
          conf->senderData = takeOverPtr.p->m_senderData;
          conf->sendingNodeId = cownNodeId;
          conf->startingNodeId = nodePtr.i;
          sendSignal(takeOverPtr.p->m_senderRef, GSN_END_TOCONF, signal, 
                     EndToConf::SignalLength, JBB);

          releaseTakeOver(takeOverPtr, true);
        }
      }

      takeOverPtr = nextPtr;
    }
    /**
     * We send the LCP_COMPLETE_REP from the master node to all nodes
     * that participated in the LCP in DIH, we could have alive nodes
     * here that didn't participate in the LCP because they became
     * alive so recently that they didn't need to participate in the
     * LCP since it was already closing when they entered through the
     * PAUSE LCP protocol. Sending to those nodes is not a good idea
     * since they are not at all set up to receive a LCP_COMPLETE_REP
     * message.
     */
    LcpCompleteRep * rep = (LcpCompleteRep*)signal->getDataPtrSend();
    rep->nodeId = getOwnNodeId();
    rep->lcpId = SYSFILE->latestLCP_ID;
    rep->blockNo = 0; // 0 = Sent from master
    NodeReceiverGroup rg(DBDIH, c_lcpState.m_participatingDIH);
    rg.m_nodes.clear(getOwnNodeId());
    sendSignal(rg, GSN_LCP_COMPLETE_REP, signal,
               LcpCompleteRep::SignalLength, JBB);

    jam();
  }
  
  Sysfile::clearLCPOngoing(SYSFILE->systemRestartBits);
  setLcpActiveStatusEnd(signal);

  /**
   * We calculate LCP time also in non-master although it's only used by
   * master nodes. The idea is to have an estimate of LCP execution time
   * already when the master node is running it's first LCP.
   */
  c_lcpState.m_lcp_time = 
    NdbTick_Elapsed(c_lcpState.m_start_time, c_current_time).milliSec();

  if(!isMaster()){
    jam();
    /**
     * We're not master, be content
     */
    return;
  }

  /***************************************************************************/
  // Report the event that a local checkpoint has completed.
  /***************************************************************************/
  signal->theData[0] = NDB_LE_LocalCheckpointCompleted; //Event type
  signal->theData[1] = SYSFILE->latestLCP_ID;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);

  if (c_newest_restorable_gci > c_lcpState.lcpStopGcp &&
      !(ERROR_INSERTED(7222) || ERROR_INSERTED(7223)))
  {
    jam();
    c_lcpState.lcpStopGcp = c_newest_restorable_gci;
  }

  /**
   * Start checking for next LCP
   */
  checkLcpStart(signal, __LINE__, 0);

  ndbassert(check_pause_state_sanity());
  if (!c_lcp_runs_with_pause_support)
  {
    jam();
    Mutex mutex(signal, c_mutexMgr, c_fragmentInfoMutex_lcp);
    mutex.unlock();
  }
  else if (c_old_node_waiting_for_lcp_end)
  {
    jam();
    c_old_node_waiting_for_lcp_end = false;
    start_copy_meta_data(signal);
  }

  c_lcp_runs_with_pause_support = false;
  ndbassert(check_pause_state_sanity());
  c_current_time = NdbTick_getCurrentTicks();
  
  if (cwaitLcpSr == true) {
    jam();
 
    infoEvent("Make On-line Database recoverable by waiting for LCP"
              " Completed, LCP id = %u",
              SYSFILE->latestLCP_ID);

    cwaitLcpSr = false;
    ndbsttorry10Lab(signal, __LINE__);
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
  if (tabPtr.p->tabStorage == TabRecord::ST_TEMPORARY)
  {
    // For temporary tables we do not write to disk. Mark both copies 0 and 1
    // as done, and go straight to the after-close code.
    filePtr.i = tabPtr.p->tabFile[1];
    ptrCheckGuard(filePtr, cfileFileSize, fileRecord);
    tableCloseLab(signal, filePtr);
    return;
  }
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

    tabPtr.p->tabCopyStatus = TabRecord::CS_IDLE;
    tabPtr.p->tabUpdateState = TabRecord::US_IDLE;
    tabPtr.p->tabLcpStatus = TabRecord::TLS_COMPLETED;

    /* Check whether there's some queued table definition flush op to start */
    if (c_lcpTabDefWritesControl.releaseMustStartQueued())
    {
      jam();
      /* Some table write is queued - let's kick it off */
      /* First find it...
       *   By using the tabUpdateState to 'queue' operations, we lose
       *   the original flush request order, which shouldn't matter.
       *   In any case, the checkpoint proceeds by table id, as does this
       *   search, so a similar order should result
       */
      TabRecordPtr tabPtr;
      for (tabPtr.i = 0; tabPtr.i < ctabFileSize; tabPtr.i++)
      {
        ptrAss(tabPtr, tabRecord);
        if (tabPtr.p->tabUpdateState == TabRecord::US_LOCAL_CHECKPOINT_QUEUED)
        {
          jam();
          //ndbout_c("DIH : Starting queued table def flush op on table %u", tabPtr.i);
          tabPtr.p->tabUpdateState = TabRecord::US_LOCAL_CHECKPOINT;
          signal->theData[0] = DihContinueB::ZPACK_TABLE_INTO_PAGES;
          signal->theData[1] = tabPtr.i;
          sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
          return;
        }
      }
      /* No queued table write found - error */
      g_eventLogger->warning("DIH : Error in queued table writes : inUse %u"
                             " queued %u total %u",
                             c_lcpTabDefWritesControl.inUse,
                             c_lcpTabDefWritesControl.queuedRequests,
                             c_lcpTabDefWritesControl.totalResources);
      ndbrequire(false);
    }
    jam();
    signal->theData[0] = DihContinueB::ZCHECK_LCP_COMPLETED;
    sendSignal(reference(), GSN_CONTINUEB, signal, 1, JBB);

    return;
    break;
  case TabRecord::US_REMOVE_NODE:
    jam();
    releaseTabPages(tabPtr.i);
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
    if (!ERROR_INSERTED(7233))
      sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBB);
    else
      sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 300, 3);
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

    handle_send_continueb_invalidate_node_lcp(signal);
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
  case TabRecord::US_CALLBACK:
  {
    jam();
    releaseTabPages(tabPtr.i);
    tabPtr.p->tabCopyStatus = TabRecord::CS_IDLE;
    tabPtr.p->tabUpdateState = TabRecord::US_IDLE;

    Ptr<ConnectRecord> connectPtr;
    connectPtr.i = tabPtr.p->connectrec;
    ptrCheckGuard(connectPtr, cconnectFileSize, connectRecord);
    execute(signal, connectPtr.p->m_callback, 0);
    return;
  }
  default:
    ndbrequire(false);
    return;
    break;
  }//switch
}//Dbdih::tableCloseLab()

void Dbdih::checkGcpStopLab(Signal* signal) 
{
  static const Uint32 GCPCheckPeriodMillis = 100;

  // Calculate real time elapsed since last check
  const NDB_TICKS now = NdbTick_getCurrentTicks();
  const NDB_TICKS last = m_gcp_monitor.m_last_check;
  m_gcp_monitor.m_last_check = now;

  /**
   * Avoid false GCP failures if timers misbehaves, 
   * (timer is non-monotonic, or OS/VM bugs which there are some of)
   * or we have scheduler problems due to being CPU starved:
   *
   * - If we overslept 'GCPCheckPeriodMillis', (CPU starved?) or 
   *   timer leapt forward for other reasons (Adjusted, or OS-bug)
   *   we never calculate an elapsed periode of more than 
   *   the requested sleep 'GCPCheckPeriodMillis'
   * - Else we add the real measured elapsed time to total.
   *   (Timers may fire prior to requested 'GCPCheckPeriodMillis')
   *
   * Note: If timer for some reason ticked backwards such that
   *       'now < last', NdbTick_Elapsed() will return '0' such
   *       that this is 'absorbed'
   */
  Uint32 elapsed_ms = (Uint32)NdbTick_Elapsed(last,now).milliSec();
  if (elapsed_ms > GCPCheckPeriodMillis)
    elapsed_ms = GCPCheckPeriodMillis;

  const Uint32 lag0 = (m_gcp_monitor.m_gcp_save.m_elapsed_ms  += elapsed_ms);
  const Uint32 lag1 = (m_gcp_monitor.m_micro_gcp.m_elapsed_ms += elapsed_ms);

  if (ERROR_INSERTED(7145))
  {
    static bool done = false;
    /* 
      Recalculate the timeouts the get the low values that the test
      needs.  This was initially done at startup, and at that point,
      the ERROR_INSERT was not set yet.
    */
    if (!done)
    {
      setGCPStopTimeouts();
      done = true;
    }
  }

  if (m_gcp_monitor.m_gcp_save.m_gci == m_gcp_save.m_gci)
  {
    jam();
    if (m_gcp_monitor.m_gcp_save.m_max_lag_ms &&
        lag0 >= m_gcp_monitor.m_gcp_save.m_max_lag_ms)
    {
      crashSystemAtGcpStop(signal, false);
      /* Continue monitoring */
    }

    /**
     * Will report a warning every time lag crosses 
     * a multiple of 'report_period_ms'
     */
    const Uint32 report_period_ms = 60*1000; // 60 seconds
    if (lag0 > 0 && (lag0 % report_period_ms) < elapsed_ms)
    {
      if (m_gcp_monitor.m_gcp_save.m_max_lag_ms)
      {
        warningEvent("GCP Monitor: GCP_SAVE lag %u seconds"
                     " (max lag: %us)",
                     lag0/1000, m_gcp_monitor.m_gcp_save.m_max_lag_ms/1000);
      }
      else
      {
        warningEvent("GCP Monitor: GCP_SAVE lag %u seconds"
                     " (no max lag)",
                     lag0/1000);
      }
    }
  }
  else
  {
    jam();
    m_gcp_monitor.m_gcp_save.m_gci = m_gcp_save.m_gci;
    m_gcp_monitor.m_gcp_save.m_elapsed_ms = 0;
  }

  if (m_gcp_monitor.m_micro_gcp.m_gci == m_micro_gcp.m_current_gci)
  {
    jam();
    const Uint32 cmp = m_micro_gcp.m_enabled ? 
      m_gcp_monitor.m_micro_gcp.m_max_lag_ms :
      m_gcp_monitor.m_gcp_save.m_max_lag_ms;
    
    if (cmp && lag1 >= cmp)
    {
      crashSystemAtGcpStop(signal, false);
      /* Continue monitoring */
    }

    /**
     * Will report a warning every time lag crosses 
     * a multiple of 'report_period_ms'
     */
    const Uint32 report_period_ms = 10*1000; // 10 seconds
    if (lag1 > 0 && (lag1 % report_period_ms) < elapsed_ms)
    {
      if (m_gcp_monitor.m_micro_gcp.m_max_lag_ms)
      {
        warningEvent("GCP Monitor: GCP_COMMIT lag %u seconds"
                     " (max lag: %u)",
                     lag1/1000, m_gcp_monitor.m_micro_gcp.m_max_lag_ms/1000);
      }
      else
      {
        warningEvent("GCP Monitor: GCP_COMMIT lag %u seconds"
                     " (no max lag)",
                     lag1/1000);
      }
    }
  }
  else
  {
    jam();
    m_gcp_monitor.m_micro_gcp.m_elapsed_ms = 0;
    m_gcp_monitor.m_micro_gcp.m_gci = m_micro_gcp.m_current_gci;
  }
  
  signal->theData[0] = DihContinueB::ZCHECK_GCP_STOP;
  sendSignalWithDelay(reference(), GSN_CONTINUEB, signal,
                      GCPCheckPeriodMillis, 1);
  return;
}//Dbdih::checkGcpStopLab()

void
Dbdih::dumpGcpStop()
{
  ndbout_c("c_nodeStartMaster.blockGcp: %u %u",
           c_nodeStartMaster.blockGcp,
           c_nodeStartMaster.startNode);
  ndbout_c("m_gcp_save.m_elapsed: %u(ms) m_gcp_save.m_max_lag: %u(ms)",
           m_gcp_monitor.m_gcp_save.m_elapsed_ms, 
           m_gcp_monitor.m_gcp_save.m_max_lag_ms);
  ndbout_c("m_micro_gcp.m_elapsed: %u(ms) m_micro_gcp.m_max_lag: %u(ms)",
           m_gcp_monitor.m_micro_gcp.m_elapsed_ms, 
           m_gcp_monitor.m_micro_gcp.m_max_lag_ms);
  
  
  ndbout_c("m_gcp_save.m_state: %u", m_gcp_save.m_state);
  ndbout_c("m_gcp_save.m_master.m_state: %u", m_gcp_save.m_master.m_state);
  ndbout_c("m_micro_gcp.m_state: %u", m_micro_gcp.m_state);
  ndbout_c("m_micro_gcp.m_master.m_state: %u", m_micro_gcp.m_master.m_state);
  
  ndbout_c("c_COPY_GCIREQ_Counter = %s", c_COPY_GCIREQ_Counter.getText());
  ndbout_c("c_COPY_TABREQ_Counter = %s", c_COPY_TABREQ_Counter.getText());
  ndbout_c("c_UPDATE_FRAG_STATEREQ_Counter = %s",
            c_UPDATE_FRAG_STATEREQ_Counter.getText());
  ndbout_c("c_DIH_SWITCH_REPLICA_REQ_Counter = %s", 
	   c_DIH_SWITCH_REPLICA_REQ_Counter.getText());
  ndbout_c("c_EMPTY_LCP_REQ_Counter = %s",c_EMPTY_LCP_REQ_Counter.getText());
  ndbout_c("c_GCP_COMMIT_Counter = %s", c_GCP_COMMIT_Counter.getText());
  ndbout_c("c_GCP_PREPARE_Counter = %s", c_GCP_PREPARE_Counter.getText());
  ndbout_c("c_GCP_SAVEREQ_Counter = %s", c_GCP_SAVEREQ_Counter.getText());
  ndbout_c("c_SUB_GCP_COMPLETE_REP_Counter = %s",
           c_SUB_GCP_COMPLETE_REP_Counter.getText());
  ndbout_c("c_INCL_NODEREQ_Counter = %s", c_INCL_NODEREQ_Counter.getText());
  ndbout_c("c_MASTER_GCPREQ_Counter = %s", c_MASTER_GCPREQ_Counter.getText());
  ndbout_c("c_MASTER_LCPREQ_Counter = %s", c_MASTER_LCPREQ_Counter.getText());
  ndbout_c("c_START_INFOREQ_Counter = %s", c_START_INFOREQ_Counter.getText());
  ndbout_c("c_START_RECREQ_Counter = %s", c_START_RECREQ_Counter.getText());
  ndbout_c("c_STOP_ME_REQ_Counter = %s", c_STOP_ME_REQ_Counter.getText());
  ndbout_c("c_TC_CLOPSIZEREQ_Counter = %s", c_TC_CLOPSIZEREQ_Counter.getText());
  ndbout_c("c_TCGETOPSIZEREQ_Counter = %s", c_TCGETOPSIZEREQ_Counter.getText());

  ndbout_c("m_copyReason: %d m_waiting: %u %u",
           c_copyGCIMaster.m_copyReason,
           c_copyGCIMaster.m_waiting[0],
           c_copyGCIMaster.m_waiting[1]);
  
  ndbout_c("c_copyGCISlave: sender{Data, Ref} %d %x reason: %d nextWord: %d",
	   c_copyGCISlave.m_senderData,
	   c_copyGCISlave.m_senderRef,
	   c_copyGCISlave.m_copyReason,
	   c_copyGCISlave.m_expectedNextWord);
}

/**
 * GCP stop detected, 
 * local == true means we must shutdown
 * local == false means we (GCP Master) are deciding what to 
 *  do - may involve requesting shut down of other nodes and/or 
 *  ourself.
 *
 * The action to take is generally :
 *   1.  Send 'Please log debug info + shutdown' signals to
 *       stalled nodes
 *   2,  Send ISOLATE_ORD with delay of X millis to *all*
 *       nodes (including self)
 *
 * Part 1 should result in a clean shutdown with debug
 * information and a clear cause
 * Part 2 ensures that if part 1 fails (as it might if the
 * nodes are 'ill'), the live nodes quickly exclude the
 * ill node and get on with their lives.
 *
 * Part 1 is implemented by various DUMP_STATE_ORD signals
 * and SYSTEM_ERROR
 * Part 2 is implemented using ISOLATE_ORD.
*/
void Dbdih::crashSystemAtGcpStop(Signal* signal, bool local)
{
  dumpGcpStop();
  const Uint32 save_elapsed = m_gcp_monitor.m_gcp_save.m_elapsed_ms;
  const Uint32 micro_elapsed = m_gcp_monitor.m_micro_gcp.m_elapsed_ms;
  m_gcp_monitor.m_gcp_save.m_elapsed_ms = 0;
  m_gcp_monitor.m_micro_gcp.m_elapsed_ms = 0;

  const Uint32 NodeIsolationTimeoutMillis = 100;

  if (local)
    goto dolocal;

  if (c_nodeStartMaster.blockGcp == 2)
  {
    jam();
    /**
     * Starting node...is delaying GCP to long...
     *   kill it
     */
    SystemError * const sysErr = (SystemError*)&signal->theData[0];
    sysErr->errorCode = SystemError::GCPStopDetected;
    sysErr->errorRef = reference();
    sysErr->data[0] = m_gcp_save.m_master.m_state;
    sysErr->data[1] = cgcpOrderBlocked;
    sysErr->data[2] = m_micro_gcp.m_master.m_state;
    sendSignal(calcNdbCntrBlockRef(c_nodeStartMaster.startNode), 
               GSN_SYSTEM_ERROR, signal, SystemError::SignalLength, JBA);

    {
      /* Isolate, just in case */
      NdbNodeBitmask victims;
      victims.set(c_nodeStartMaster.startNode);
      
      isolateNodes(signal,
                   NodeIsolationTimeoutMillis,
                   victims);
    }
    return;
  }

  if (save_elapsed >= m_gcp_monitor.m_gcp_save.m_max_lag_ms)
  {
    switch(m_gcp_save.m_master.m_state){
    case GcpSave::GCP_SAVE_IDLE:
    {
      /**
       * No switch for looong time...and we're idle...it *our* fault
       */
      /* Ask others to isolate me, just in case */
      {
        NdbNodeBitmask victims;
        victims.set(cownNodeId);
        
        isolateNodes(signal,
                     NodeIsolationTimeoutMillis,
                     victims);
      }
      local = true;
      break;
    }
    case GcpSave::GCP_SAVE_REQ:
    {
      jam();
      NodeReceiverGroup rg(DBLQH, c_GCP_SAVEREQ_Counter);
      signal->theData[0] = 2305;
      sendSignal(rg, GSN_DUMP_STATE_ORD, signal, 1, JBB);
      
      isolateNodes(signal,
                   NodeIsolationTimeoutMillis,
                   c_GCP_SAVEREQ_Counter.getNodeBitmask());

      warningEvent("Detected GCP stop(%d)...sending kill to %s", 
                m_gcp_save.m_master.m_state, c_GCP_SAVEREQ_Counter.getText());
      ndbout_c("Detected GCP stop(%d)...sending kill to %s", 
               m_gcp_save.m_master.m_state, c_GCP_SAVEREQ_Counter.getText());
      ndbrequire(!c_GCP_SAVEREQ_Counter.done());
      return;
    }
    case GcpSave::GCP_SAVE_COPY_GCI:
    {
      /**
       * We're waiting for a COPY_GCICONF
       */
      warningEvent("Detected GCP stop(%d)...sending kill to %s", 
                m_gcp_save.m_master.m_state, c_COPY_GCIREQ_Counter.getText());
      ndbout_c("Detected GCP stop(%d)...sending kill to %s", 
               m_gcp_save.m_master.m_state, c_COPY_GCIREQ_Counter.getText());
      
      {
        NodeReceiverGroup rg(DBDIH, c_COPY_GCIREQ_Counter);
        signal->theData[0] = 7022;
        sendSignal(rg, GSN_DUMP_STATE_ORD, signal, 1, JBA);
      }
      
      {
        NodeReceiverGroup rg(NDBCNTR, c_COPY_GCIREQ_Counter);
        SystemError * const sysErr = (SystemError*)&signal->theData[0];
        sysErr->errorCode = SystemError::GCPStopDetected;
        sysErr->errorRef = reference();
        sysErr->data[0] = m_gcp_save.m_master.m_state;
        sysErr->data[1] = cgcpOrderBlocked;
        sysErr->data[2] = m_micro_gcp.m_master.m_state;
        sendSignal(rg, GSN_SYSTEM_ERROR, signal, 
                   SystemError::SignalLength, JBA);
      }

      isolateNodes(signal,
                   NodeIsolationTimeoutMillis,
                   c_COPY_GCIREQ_Counter.getNodeBitmask());

      ndbrequire(!c_COPY_GCIREQ_Counter.done());
      return;
    }
    case GcpSave::GCP_SAVE_CONF:
      /**
       * This *should* not happen (not a master state)
       */
      local = true;
      break;
    }
  }

  if (micro_elapsed >= m_gcp_monitor.m_micro_gcp.m_max_lag_ms)
  {
    switch(m_micro_gcp.m_master.m_state){
    case MicroGcp::M_GCP_IDLE:
    {
      /**
       * No switch for looong time...and we're idle...it *our* fault
       */
      /* Ask others to isolate me, just in case */
      {
        NdbNodeBitmask victims;
        victims.set(cownNodeId);
        
        isolateNodes(signal,
                     NodeIsolationTimeoutMillis,
                     victims);
      }
      local = true;
      break;
    }
    case MicroGcp::M_GCP_PREPARE:
    {
    /**
     * We're waiting for a GCP PREPARE CONF
     */
      warningEvent("Detected GCP stop(%d)...sending kill to %s", 
                m_micro_gcp.m_state, c_GCP_PREPARE_Counter.getText());
      ndbout_c("Detected GCP stop(%d)...sending kill to %s", 
               m_micro_gcp.m_state, c_GCP_PREPARE_Counter.getText());
      
      {
        NodeReceiverGroup rg(DBDIH, c_GCP_PREPARE_Counter);
        signal->theData[0] = 7022;
        sendSignal(rg, GSN_DUMP_STATE_ORD, signal, 1, JBA);
      }
      
      {
        NodeReceiverGroup rg(NDBCNTR, c_GCP_PREPARE_Counter);
        SystemError * const sysErr = (SystemError*)&signal->theData[0];
        sysErr->errorCode = SystemError::GCPStopDetected;
        sysErr->errorRef = reference();
        sysErr->data[0] = m_gcp_save.m_master.m_state;
        sysErr->data[1] = cgcpOrderBlocked;
        sysErr->data[2] = m_micro_gcp.m_master.m_state;
        sendSignal(rg, GSN_SYSTEM_ERROR, signal, 
                   SystemError::SignalLength, JBA);
      }

      isolateNodes(signal,
                   NodeIsolationTimeoutMillis,
                   c_GCP_PREPARE_Counter.getNodeBitmask());

      ndbrequire(!c_GCP_PREPARE_Counter.done());
      return;
    }
    case MicroGcp::M_GCP_COMMIT:
    {
      warningEvent("Detected GCP stop(%d)...sending kill to %s", 
                m_micro_gcp.m_state, c_GCP_COMMIT_Counter.getText());
      ndbout_c("Detected GCP stop(%d)...sending kill to %s", 
               m_micro_gcp.m_state, c_GCP_COMMIT_Counter.getText());
      
      {
        NodeReceiverGroup rg(DBDIH, c_GCP_COMMIT_Counter);
        signal->theData[0] = 7022;
        sendSignal(rg, GSN_DUMP_STATE_ORD, signal, 1, JBA);
      }
      
      {
        NodeReceiverGroup rg(NDBCNTR, c_GCP_COMMIT_Counter);
        SystemError * const sysErr = (SystemError*)&signal->theData[0];
        sysErr->errorCode = SystemError::GCPStopDetected;
        sysErr->errorRef = reference();
        sysErr->data[0] = m_gcp_save.m_master.m_state;
        sysErr->data[1] = cgcpOrderBlocked;
        sysErr->data[2] = m_micro_gcp.m_master.m_state;
        sendSignal(rg, GSN_SYSTEM_ERROR, signal, 
                   SystemError::SignalLength, JBA);
      }

      isolateNodes(signal,
                   NodeIsolationTimeoutMillis,
                   c_GCP_COMMIT_Counter.getNodeBitmask());

      ndbrequire(!c_GCP_COMMIT_Counter.done());
      return;
    }
    case MicroGcp::M_GCP_COMMITTED:
      /**
       * This *should* not happen (not a master state)
       */
      local = true;
      break;
    case MicroGcp::M_GCP_COMPLETE:
      infoEvent("Detected GCP stop(%d)...sending kill to %s",
                m_micro_gcp.m_state, c_SUB_GCP_COMPLETE_REP_Counter.getText());
      ndbout_c("Detected GCP stop(%d)...sending kill to %s",
               m_micro_gcp.m_state, c_SUB_GCP_COMPLETE_REP_Counter.getText());

      {
        NodeReceiverGroup rg(DBDIH, c_SUB_GCP_COMPLETE_REP_Counter);
        signal->theData[0] = 7022;
        sendSignal(rg, GSN_DUMP_STATE_ORD, signal, 1, JBA);
      }

      {
        NodeReceiverGroup rg(NDBCNTR, c_SUB_GCP_COMPLETE_REP_Counter);
        SystemError * const sysErr = (SystemError*)&signal->theData[0];
        sysErr->errorCode = SystemError::GCPStopDetected;
        sysErr->errorRef = reference();
        sysErr->data[0] = m_gcp_save.m_master.m_state;
        sysErr->data[1] = cgcpOrderBlocked;
        sysErr->data[2] = m_micro_gcp.m_master.m_state;
        sendSignal(rg, GSN_SYSTEM_ERROR, signal,
                   SystemError::SignalLength, JBA);
      }

      isolateNodes(signal,
                   NodeIsolationTimeoutMillis,
                   c_SUB_GCP_COMPLETE_REP_Counter.getNodeBitmask());

      ndbrequire(!c_SUB_GCP_COMPLETE_REP_Counter.done());
      return;
    }
  }

dolocal:  
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

  /* Various GCP_STOP error insert codes */
  if (ERROR_INSERTED(7238) ||
      ERROR_INSERTED(7239) ||
      ERROR_INSERTED(7244) ||
      ERROR_INSERTED(7237) ||
      ERROR_INSERTED(7241) ||
      ERROR_INSERTED(7242) ||
      ERROR_INSERTED(7243))
  {
    jam();
    if (ERROR_INSERT_EXTRA == 1)
    {
      /* Testing GCP STOP handling via node isolation */
      jam();
      g_eventLogger->info("Not killing local due to GCP stop");
      return;
    }
    /* Otherwise fall through to SYSTEM_ERROR  */
  }

  jam();
  SystemError * const sysErr = (SystemError*)&signal->theData[0];
  sysErr->errorCode = SystemError::GCPStopDetected;
  sysErr->errorRef = reference();
  sysErr->data[0] = m_gcp_save.m_master.m_state;
  sysErr->data[1] = cgcpOrderBlocked;
  sysErr->data[2] = m_micro_gcp.m_master.m_state;
  EXECUTE_DIRECT(NDBCNTR, GSN_SYSTEM_ERROR, 
                 signal, SystemError::SignalLength);
  ndbrequire(false);
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
                               Uint32 nodeId,
                               Uint32 fragId,
                               Uint32 tableId)
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
  newReplicaPtr.p->fragId = fragId;
  newReplicaPtr.p->tableId = tableId;
  newReplicaPtr.p->noCrashedReplicas = 0;
  newReplicaPtr.p->initialGci = (Uint32)(m_micro_gcp.m_current_gci >> 32);
  for (i = 0; i < MAX_CRASHED_REPLICAS; i++) {
    newReplicaPtr.p->replicaLastGci[i] = ZINIT_REPLICA_LAST_GCI;
    newReplicaPtr.p->createGci[i] = ZINIT_CREATE_GCI;
  }//for
  newReplicaPtr.p->createGci[0] = (Uint32)(m_micro_gcp.m_current_gci >> 32);
  newReplicaPtr.p->nextLcp = 0;
  newReplicaPtr.p->procNode = nodeId;
  newReplicaPtr.p->lcpOngoingFlag = false;
  newReplicaPtr.p->lcpIdStarted = 0;
  
  arrPrevReplicaPtr.i = RNIL;
  arrReplicaPtr.i = fragPtr.p->storedReplicas;
  while (arrReplicaPtr.i != RNIL) {
    jam();
    c_replicaRecordPool.getPtr(arrReplicaPtr);
    arrPrevReplicaPtr = arrReplicaPtr;
    arrReplicaPtr.i = arrReplicaPtr.p->nextPool;
  }//while
  if (arrPrevReplicaPtr.i == RNIL) {
    jam();
    fragPtr.p->storedReplicas = newReplicaPtr.i;
  } else {
    jam();
    arrPrevReplicaPtr.p->nextPool = newReplicaPtr.i;
  }//if
  fragPtr.p->noStoredReplicas++;
}//Dbdih::allocStoredReplica()

/*************************************************************************/
/* CHECK IF THE NODE CRASH IS TO ESCALATE INTO A SYSTEM CRASH. WE COULD  */
/* DO THIS BECAUSE ALL REPLICAS OF SOME FRAGMENT ARE LOST. WE COULD ALSO */
/* DO IT AFTER MANY NODE FAILURES THAT MAKE IT VERY DIFFICULT TO RESTORE */
/* DATABASE AFTER A SYSTEM CRASH. IT MIGHT EVEN BE IMPOSSIBLE AND THIS   */
/* MUST BE AVOIDED EVEN MORE THAN AVOIDING SYSTEM CRASHES.               */
/*************************************************************************/
void Dbdih::checkEscalation() 
{
  Uint32 TnodeGroup[MAX_NDB_NODE_GROUPS];
  NodeRecordPtr nodePtr;
  Uint32 i;
  for (i = 0; i < cnoOfNodeGroups; i++) {
    TnodeGroup[i] = ZFALSE;
  }//for
  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    jam();
    ptrAss(nodePtr, nodeRecord);
    if (nodePtr.p->nodeStatus == NodeRecord::ALIVE &&
	nodePtr.p->activeStatus == Sysfile::NS_Active){
      ndbrequire(nodePtr.p->nodeGroup < MAX_NDB_NODE_GROUPS);
      TnodeGroup[nodePtr.p->nodeGroup] = ZTRUE;
    }
  }
  for (i = 0; i < cnoOfNodeGroups; i++) {
    jam();
    ndbrequire(c_node_groups[i] < MAX_NDB_NODE_GROUPS);
    if (TnodeGroup[c_node_groups[i]] == ZFALSE) {
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
void Dbdih::checkKeepGci(TabRecordPtr tabPtr, Uint32 fragId, Fragmentstore*, 
			 Uint32 replicaStartIndex) 
{
  ReplicaRecordPtr ckgReplicaPtr;
  ckgReplicaPtr.i = replicaStartIndex;
  while (ckgReplicaPtr.i != RNIL) {
    jam();
    c_replicaRecordPool.getPtr(ckgReplicaPtr);
    if (c_lcpState.m_participatingLQH.get(ckgReplicaPtr.p->procNode))
    {
      Uint32 keepGci;
      Uint32 oldestRestorableGci;
      findMinGci(ckgReplicaPtr, keepGci, oldestRestorableGci);
      if (keepGci < c_lcpState.keepGci) {
        jam();
        /* ----------------------------------------------------------------- */
        /* WE MUST KEEP LOG RECORDS SO THAT WE CAN USE ALL LOCAL CHECKPOINTS */
        /* THAT ARE AVAILABLE. THUS WE NEED TO CALCULATE THE MINIMUM OVER ALL*/
        /* FRAGMENTS.                                                        */
        /* ----------------------------------------------------------------- */
        c_lcpState.keepGci = keepGci;
      }//if
      if (oldestRestorableGci > c_lcpState.oldestRestorableGci) {
        jam();
        c_lcpState.oldestRestorableGci = oldestRestorableGci;
      }//if
    }
    ckgReplicaPtr.i = ckgReplicaPtr.p->nextPool;
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

void
Dbdih::emptyverificbuffer(Signal* signal, Uint32 q, bool aContinueB)
{
  if(unlikely(getBlockCommit() == true))
  {
    jam();
    return;
  }

  if (!isEmpty(c_diverify_queue[q]))
  {
    jam();

    ApiConnectRecord localApiConnect;
    dequeue(c_diverify_queue[q], localApiConnect);
    ndbrequire(localApiConnect.apiGci <= m_micro_gcp.m_current_gci);
    signal->theData[0] = localApiConnect.senderData;
    signal->theData[1] = (Uint32)(m_micro_gcp.m_current_gci >> 32);
    signal->theData[2] = (Uint32)(m_micro_gcp.m_current_gci & 0xFFFFFFFF);
    signal->theData[3] = 0;
    sendSignal(c_diverify_queue[q].m_ref, GSN_DIVERIFYCONF, signal, 4, JBB);
  }
  else if (aContinueB == true)
  {
    jam();
    /**
     * Make sure that we don't miss any pending transactions
     *   (transactions that are added to list by other thread
     *    while we execute this code)
     */
    Uint32 blocks[] = { DBTC, 0 };
    Callback c = { safe_cast(&Dbdih::emptyverificbuffer_check), q };
    synchronize_threads_for_blocks(signal, blocks, c);
    return;
  }

  if (aContinueB == true)
  {
    jam();
    //-----------------------------------------------------------------------
    // This emptying happened as part of a take-out process by continueb signals
    // This ensures that we will empty the queue eventually. We will also empty
    // one item every time we insert one item to ensure that the list doesn't
    // grow when it is not blocked.
    //-----------------------------------------------------------------------
    signal->theData[0] = DihContinueB::ZEMPTY_VERIFY_QUEUE;
    signal->theData[1] = q;
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
  }//if

  return;
}//Dbdih::emptyverificbuffer()

void
Dbdih::emptyverificbuffer_check(Signal* signal, Uint32 q, Uint32 retVal)
{
  ndbrequire(retVal == 0);
  if (!isEmpty(c_diverify_queue[q]))
  {
    jam();
    signal->theData[0] = DihContinueB::ZEMPTY_VERIFY_QUEUE;
    signal->theData[1] = q;
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
  }
  else
  {
    /**
     * Done with emptyverificbuffer
     */
    c_diverify_queue[q].m_empty_done = 1;
  }
}

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
  c_replicaRecordPool.getPtr(flnReplicaPtr);
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
  arrGuard(flnReplicaPtr.p->noCrashedReplicas, MAX_CRASHED_REPLICAS);
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
    if (logNode >= MAX_LOG_EXEC)
    {
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
    c_replicaRecordPool.getPtr(fblReplicaPtr);
    if (m_sr_nodes.get(fblReplicaPtr.p->procNode))
    {
      jam();
      Uint32 fliStopGci = findLogInterval(fblReplicaPtr, startGci);
      if (fliStopGci > fblStopGci)
      {
        jam();
        fblStopGci = fliStopGci;
        fblFoundReplicaPtr = fblReplicaPtr;
      }//if
    }//if
    fblReplicaPtr.i = fblReplicaPtr.p->nextPool;
  }//while
  fblReplicaPtr.i = fragPtr.p->oldStoredReplicas;
  while (fblReplicaPtr.i != RNIL) {
    jam();
    c_replicaRecordPool.getPtr(fblReplicaPtr);
    if (m_sr_nodes.get(fblReplicaPtr.p->procNode))
    {
      jam();
      Uint32 fliStopGci = findLogInterval(fblReplicaPtr, startGci);
      if (fliStopGci > fblStopGci)
      {
        jam();
        fblStopGci = fliStopGci;
        fblFoundReplicaPtr = fblReplicaPtr;
      }//if
    }//if
    fblReplicaPtr.i = fblReplicaPtr.p->nextPool;
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
  ndbrequire(replicaPtr.p->noCrashedReplicas <= MAX_CRASHED_REPLICAS);
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
  keepGci = (Uint32)-1;
  oldestRestorableGci = 0;

  Uint32 maxLcpId = 0;              // LcpId of latest valid LCP
  Uint32 maxLcpNo = MAX_LCP_STORED; // Index of latest valid LCP
  for (Uint32 i = 0; i < MAX_LCP_STORED; i++)
  {
    jam();
    if (fmgReplicaPtr.p->lcpStatus[i] == ZVALID)
    {
      if ((fmgReplicaPtr.p->lcpId[i] + MAX_LCP_STORED) <= SYSFILE->latestLCP_ID)
      {
        jam();
        /*-----------------------------------------------------------------*/
        // We invalidate the checkpoint we are preparing to overwrite.
        // The LCP id is still the old lcp id,
        // this is the reason of comparing with lcpId + 1.
        /*-----------------------------------------------------------------*/
        fmgReplicaPtr.p->lcpStatus[i] = ZINVALID;
      }
      else if (fmgReplicaPtr.p->lcpId[i] > maxLcpId)
      {
        jam();
        maxLcpId = fmgReplicaPtr.p->lcpId[i];
        maxLcpNo = i;
      }
    }
  }

  if (maxLcpNo < MAX_LCP_STORED)
  {
    /**
     * Only consider latest LCP (wrt to how to cut REDO)
     */
    jam();
    keepGci = fmgReplicaPtr.p->maxGciCompleted[maxLcpNo];
    oldestRestorableGci = fmgReplicaPtr.p->maxGciStarted[maxLcpNo];
  }
  
  if (oldestRestorableGci == 0 && keepGci == Uint32(-1))
  {
    jam();
    if (fmgReplicaPtr.p->createGci[0] == fmgReplicaPtr.p->initialGci)
    {
      keepGci = fmgReplicaPtr.p->createGci[0];
      // XXX Jonas
      //oldestRestorableGci = fmgReplicaPtr.p->createGci[0];
    }
  }
  else
  {
    ndbassert(oldestRestorableGci <= c_newest_restorable_gci);
  }
  return;
}//Dbdih::findMinGci()

bool Dbdih::findStartGci(ConstPtr<ReplicaRecord> replicaPtr,
                         Uint32 stopGci,
                         Uint32& startGci,
                         Uint32& lcpNo) 
{
  Uint32 cnt = 0;
  Uint32 tmp[MAX_LCP_STORED];
  for (Uint32 i = 0; i<MAX_LCP_STORED; i++)
  {
    jam();
    if (replicaPtr.p->lcpStatus[i] == ZVALID &&
        replicaPtr.p->maxGciStarted[i] <= stopGci)
    {
      /**
       * In order to use LCP
       *   we must be able to run REDO atleast up until maxGciStarted
       *   which is that highest GCI that
       */
      jam();
      tmp[cnt] = i;
      cnt++;
    }
  }
  
  if (cnt)
  {
    jam();
    /**
     * We found atleast one...get the highest
     */
    lcpNo = tmp[0];
    Uint32 lcpId = replicaPtr.p->lcpId[lcpNo];
    for (Uint32 i = 1; i<cnt; i++)
    {
      jam();
      if (replicaPtr.p->lcpId[tmp[i]] > lcpId)
      {
        jam();
        lcpNo = tmp[i];
        lcpId = replicaPtr.p->lcpId[lcpNo];
      }
    }
    startGci = replicaPtr.p->maxGciCompleted[lcpNo] + 1;
    return true;
  }

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

/**
 * Compute max time it can take to "resolve" cascading node-failures
 *   given hb-interval, arbit timeout and #db-nodes.
 */
Uint32
Dbdih::compute_max_failure_time()
{
  jam();
  Uint32 no_of_live_db_nodes = 0;
  
  // Count the number of live data nodes.
  NodeRecordPtr nodePtr(NULL, cfirstAliveNode);
  while (nodePtr.i != RNIL)
  {
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);

    ndbassert(nodePtr.p->nodeStatus == NodeRecord::ALIVE);

    no_of_live_db_nodes++;
    nodePtr.i = nodePtr.p->nextNode;
  }

  const ndb_mgm_configuration_iterator* cfgIter = 
    m_ctx.m_config.getOwnConfigIterator();

  Uint32 hbDBDB = 5000;
  ndb_mgm_get_int_parameter(cfgIter, CFG_DB_HEARTBEAT_INTERVAL, &hbDBDB);

  Uint32 arbit_timeout = 7500;
  ndb_mgm_get_int_parameter(cfgIter, CFG_DB_ARBIT_TIMEOUT, &arbit_timeout);

  /*
    A node is presumed dead if it is silent for four missed heartbeats, 
    meaning that the worst case is five heartbeat intervals.
  */
  const Uint32 heartbeat_fail_time = hbDBDB * 5;

  /*
    The worst case failure scenario works as follows:

    1) All data nodes are running.

    2) One in each node group fail. Detecting this takes:
    no_of_node_groups * heartbeat_fail_time

    3) Arbitration is started, as the failed nodes could have formed an 
    independent cluster. Arbitration make take up to arbit_timeout to
    complete.

    4) Just before arbitration completes, all remaining nodes except
    for the master fail. The remain node *could* have shut itself down
    as soon as the first of these failures are detected, but as it
    waits for outstanding PREP_FAILCONF messages before checking of
    the cluster is viable, it does not do so until all the failures
    have been detected. Detecting these failures thus takes:
    (no_of_nodes - no_of_node_groups - 1) * heartbeat_fail_time
    
    Combining these figure we get a total failure time of:
    (no_of_nodes - 1) * heartbeat_fail_time + arbit_timeout

    (For NoOfReplicas>2 there could be cases of nodes failing sequentially 
    that would require more than one round of arbitration. These have not 
    been considered here.)
  */

  return (MAX(no_of_live_db_nodes, 1) - 1) * heartbeat_fail_time 
    + arbit_timeout;
}

/*
  Calculate timeouts for detecting GCP stops. These must be set such that
  node failures are not falsely interpreted as GCP stops.
*/
void Dbdih::setGCPStopTimeouts()
{
  
  const ndb_mgm_configuration_iterator* cfgIter = 
    m_ctx.m_config.getOwnConfigIterator();

  const Uint32 max_failure_time = compute_max_failure_time();
  
  // Set time-between epochs timeout
  Uint32 micro_GCP_timeout = 4000;
  ndb_mgm_get_int_parameter(cfgIter, CFG_DB_MICRO_GCP_TIMEOUT, 
                            &micro_GCP_timeout);

  /* 
    Set minimum value for time-between global checkpoint timeout. 
    By default, this is 2 minutes.
  */
  Uint32 gcp_timeout = 120000;
  ndb_mgm_get_int_parameter(cfgIter, CFG_DB_GCP_TIMEOUT, &gcp_timeout);

  const Uint32 old_micro_GCP_max_lag = m_gcp_monitor.m_micro_gcp.m_max_lag_ms;
  const Uint32 old_GCP_save_max_lag = m_gcp_monitor.m_gcp_save.m_max_lag_ms;
    
  if (micro_GCP_timeout != 0)
  {
    jam();
    if (ERROR_INSERTED(7145))
    {
      /*
        We drop these lower limits in certain tests, to verify that the 
        calculated value for max_failure_time is sufficient.
       */
      ndbout << "Dbdih::setGCPStopTimeouts() setting minimal GCP timout values"
             << " for test purposes."  << endl;
      micro_GCP_timeout = 0;
      gcp_timeout = 0;
    }

    m_gcp_monitor.m_micro_gcp.m_max_lag_ms = 
      m_micro_gcp.m_master.m_time_between_gcp + micro_GCP_timeout 
      + max_failure_time;
    
    m_gcp_monitor.m_gcp_save.m_max_lag_ms = 
      m_gcp_save.m_master.m_time_between_gcp + 
      // Ensure that GCP-commit times out before GCP-save if both stops. 
      MAX(gcp_timeout, micro_GCP_timeout) + 
      max_failure_time;
  }
  else
  {
    jam();
    m_gcp_monitor.m_gcp_save.m_max_lag_ms = 0;
    m_gcp_monitor.m_micro_gcp.m_max_lag_ms = 0;
  }

  // If timeouts have changed, log it.
  if (old_micro_GCP_max_lag != m_gcp_monitor.m_micro_gcp.m_max_lag_ms ||
      old_GCP_save_max_lag != m_gcp_monitor.m_gcp_save.m_max_lag_ms)
  {
    if (m_gcp_monitor.m_micro_gcp.m_max_lag_ms > 0)
    {
      jam();
      if (isMaster())
      {
        jam();
        // Log to mgmd.
        infoEvent("GCP Monitor: Computed max GCP_COMMIT lag to %u seconds",
                  m_gcp_monitor.m_micro_gcp.m_max_lag_ms / 1000);
        infoEvent("GCP Monitor: Computed max GCP_SAVE lag to %u seconds",
                  m_gcp_monitor.m_gcp_save.m_max_lag_ms / 1000);
      }
      // Log locallly.
      g_eventLogger->info("GCP Monitor: Computed max GCP_COMMIT lag to %u"
                          " seconds",
                          m_gcp_monitor.m_micro_gcp.m_max_lag_ms / 1000);
      g_eventLogger->info("GCP Monitor: Computed max GCP_SAVE lag to %u"
                          " seconds", 
                          m_gcp_monitor.m_gcp_save.m_max_lag_ms / 1000);    
    }
    else
    {
      jam();
      if (isMaster())
      {
        jam();
        infoEvent("GCP Monitor: unlimited lags allowed");
      }
      g_eventLogger->info("GCP Monitor: unlimited lags allowed");
    }
  }
} // setGCPStopTimeouts()

void Dbdih::initCommonData()
{
  c_blockCommit = false;
  c_blockCommitNo = 0;
  cfailurenr = 1;
  cMinTcFailNo = 0; /* 0 as TC inits to 0 */
  cfirstAliveNode = RNIL;
  cfirstDeadNode = RNIL;
  cgckptflag = false;
  cgcpOrderBlocked = 0;
  c_performed_copy_phase = false;

  c_lcpMasterTakeOverState.set(LMTOS_IDLE, __LINE__);

  c_lcpState.clcpDelay = 0;
  c_lcpState.lcpStart = ZIDLE;
  c_lcpState.lcpStopGcp = 0;
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
  c_lcpState.m_lastLCP_COMPLETE_REP_ref = 0;
  cmasterdihref = 0;
  cmasterNodeId = 0;
  cmasterState = MASTER_IDLE;
  cmasterTakeOverNode = 0;
  cnoOfActiveTables = 0;
  cnoOfNodeGroups = 0;
  c_nextNodeGroup = 0;
  cnoReplicas = 0;
  con_lineNodes = 0;
  creceivedfrag = 0;
  crestartGci = 0;
  crestartInfoFile[0] = RNIL;
  crestartInfoFile[1] = RNIL;
  cstartPhase = 0;
  cstarttype = (Uint32)-1;
  csystemnodes = 0;
  c_newest_restorable_gci = 0;
  cwaitLcpSr = false;
  c_nodeStartMaster.blockGcp = 0;

  nodeResetStart(0);
  c_nodeStartMaster.wait = ZFALSE;

  memset(&sysfileData[0], 0, sizeof(sysfileData));

  const ndb_mgm_configuration_iterator * p = 
    m_ctx.m_config.getOwnConfigIterator();
  ndbrequire(p != 0);
  
  c_lcpState.clcpDelay = 20;

  /**
   * Get the configuration value for how many parallel fragment copy scans we
   * are going to do in parallel when we are requested to handle a node
   * recovery. If 0 set it to default value.
   */
  c_max_takeover_copy_threads = 0;
  ndb_mgm_get_int_parameter(p,
                            CFG_DB_PARALLEL_COPY_THREADS,
                            &c_max_takeover_copy_threads);
  if (c_max_takeover_copy_threads == 0)
  {
    jam();
    c_max_takeover_copy_threads = ZTAKE_OVER_THREADS;
  }

  ndb_mgm_get_int_parameter(p, CFG_DB_LCP_INTERVAL, &c_lcpState.clcpDelay);
  c_lcpState.clcpDelay = c_lcpState.clcpDelay > 31 ? 31 : c_lcpState.clcpDelay;
  
  //ndb_mgm_get_int_parameter(p, CFG_DB_MIN_HOT_SPARES, &cminHotSpareNodes);

  cnoReplicas = 1;
  ndb_mgm_get_int_parameter(p, CFG_DB_NO_REPLICAS, &cnoReplicas);
  if (cnoReplicas > MAX_REPLICAS)
  {
    progError(__LINE__, NDBD_EXIT_INVALID_CONFIG,
	      "Only up to four replicas are supported. Check NoOfReplicas.");
  }

  init_next_replica_node(&c_next_replica_node, cnoReplicas);
  bzero(&m_gcp_save, sizeof(m_gcp_save));
  bzero(&m_micro_gcp, sizeof(m_micro_gcp));
  NdbTick_Invalidate(&m_gcp_save.m_master.m_start_time);
  NdbTick_Invalidate(&m_micro_gcp.m_master.m_start_time);
  {
    { // Set time-between global checkpoint
      Uint32 tmp = 2000;
      ndb_mgm_get_int_parameter(p, CFG_DB_GCP_INTERVAL, &tmp);
      tmp = tmp > 60000 ? 60000 : (tmp < 10 ? 10 : tmp);
      m_gcp_save.m_master.m_time_between_gcp = tmp;
    }
    
    Uint32 tmp = 0;
    if (ndb_mgm_get_int_parameter(p, CFG_DB_MICRO_GCP_INTERVAL, &tmp) == 0 &&
        tmp)
    {
      /**
       * Set time-between epochs
       */
      if (tmp > m_gcp_save.m_master.m_time_between_gcp)
        tmp = m_gcp_save.m_master.m_time_between_gcp;
      if (tmp < 10)
        tmp = 10;
      m_micro_gcp.m_master.m_time_between_gcp = tmp;
    }

    // These will be set when nodes reach state 'started'.
    m_gcp_monitor.m_micro_gcp.m_max_lag_ms = 0;
    m_gcp_monitor.m_gcp_save.m_max_lag_ms = 0;
  }
}//Dbdih::initCommonData()

void Dbdih::initFragstore(FragmentstorePtr fragPtr, Uint32 fragId)
{
  fragPtr.p->fragId = fragId;
  fragPtr.p->nextCopyFragment = RNIL;
  fragPtr.p->storedReplicas = RNIL;
  fragPtr.p->oldStoredReplicas = RNIL;
  fragPtr.p->m_log_part_id = RNIL; /* To ensure not used uninited */
  fragPtr.p->partition_id = ~Uint32(0); /* To ensure not used uninited */
  
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
void Dbdih::initRestartInfo(Signal* signal) 
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

  Uint32 startGci = 1;
#ifndef DBUG_OFF
#ifdef NDB_USE_GET_ENV
  {
    char envBuf[256];
    const char* v = NdbEnv_GetEnv("NDB_START_GCI",
                                  envBuf,
                                  256);
    if (v && *v != 0)
    {
      startGci = my_strtoull(v, NULL, 0);

      ndbout_c("DbDih : Using value of %u from NDB_START_GCI",
               startGci);
    }
  }
#endif
#endif

  m_micro_gcp.m_old_gci = Uint64(startGci) << 32;
  m_micro_gcp.m_current_gci = Uint64(startGci + 1) << 32;
  crestartGci = startGci;
  c_newest_restorable_gci = startGci;

  SYSFILE->keepGCI             = startGci;
  SYSFILE->oldestRestorableGCI = startGci;
  SYSFILE->newestRestorableGCI = startGci;
  SYSFILE->systemRestartBits   = 0;
  for (i = 0; i < NdbNodeBitmask::Size; i++) {
    SYSFILE->lcpActive[0]        = 0;
  }//for  
  for (i = 0; i < Sysfile::TAKE_OVER_SIZE; i++) {
    SYSFILE->takeOver[i] = 0;
  }//for
  Sysfile::setInitialStartOngoing(SYSFILE->systemRestartBits);
  srand((unsigned int)time(0));
  globalData.m_restart_seq = SYSFILE->m_restart_seq = 0;

  if (m_micro_gcp.m_enabled == false && 
      m_micro_gcp.m_master.m_time_between_gcp)
  {
    /**
     * Micro GCP is disabled...but configured...
     */
    jam();
    m_micro_gcp.m_enabled = true;
    UpgradeProtocolOrd * ord = (UpgradeProtocolOrd*)signal->getDataPtrSend();
    ord->type = UpgradeProtocolOrd::UPO_ENABLE_MICRO_GCP;
    EXECUTE_DIRECT(QMGR,GSN_UPGRADE_PROTOCOL_ORD,signal,signal->getLength());
  }
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
  new (tabPtr.p) TabRecord();
  NdbMutex_Init(&tabPtr.p->theMutex);
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
  tabPtr.p->tabStorage = TabRecord::ST_NORMAL;
  tabPtr.p->tabErrorCode = 0;
  tabPtr.p->schemaVersion = (Uint32)-1;
  tabPtr.p->tabRemoveNode = RNIL;
  tabPtr.p->totalfragments = (Uint32)-1;
  tabPtr.p->partitionCount = (Uint32)-1;
  tabPtr.p->connectrec = RNIL;
  tabPtr.p->tabFile[0] = RNIL;
  tabPtr.p->tabFile[1] = RNIL;
  tabPtr.p->m_dropTab.tabUserRef = 0;
  tabPtr.p->m_dropTab.tabUserPtr = RNIL;
  Uint32 i;
  for (i = 0; i < NDB_ARRAY_SIZE(tabPtr.p->startFid); i++) {
    tabPtr.p->startFid[i] = RNIL;
  }//for
  for (i = 0; i < NDB_ARRAY_SIZE(tabPtr.p->pageRef); i++) {
    tabPtr.p->pageRef[i] = RNIL;
  }//for
  tabPtr.p->tableType = DictTabInfo::UndefTableType;
  tabPtr.p->schemaTransId = 0;
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
    c_diverify_queue[0].m_ref = calcTcBlockRef(getOwnNodeId());
    for (Uint32 i = 0; i < c_diverify_queue_cnt; i++)
    {
      if (globalData.ndbMtTcThreads > 0)
      {
        c_diverify_queue[i].m_ref = numberToRef(DBTC, i + 1, 0);
      }
      /******** INTIALIZING API CONNECT RECORDS ********/
      for (apiConnectptr.i = 0;
           apiConnectptr.i < capiConnectFileSize; apiConnectptr.i++)
      {
        refresh_watch_dog();
        ptrAss(apiConnectptr, c_diverify_queue[i].apiConnectRecord);
        apiConnectptr.p->senderData = RNIL;
        apiConnectptr.p->apiGci = ~(Uint64)0;
      }//for
    }
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
      connectPtr.p->nextPool = connectPtr.i + 1;
      bzero(connectPtr.p->nodes, sizeof(connectPtr.p->nodes));
    }//for
    connectPtr.i = cconnectFileSize - 1;
    ptrAss(connectPtr, connectRecord);
    connectPtr.p->nextPool = RNIL;
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
      for (loopNGPtr.i = 0; loopNGPtr.i < MAX_NDB_NODE_GROUPS; loopNGPtr.i++) {
	ptrAss(loopNGPtr, nodeGroupRecord);
        loopNGPtr.p->nodesInGroup[0] = RNIL;
        loopNGPtr.p->nodesInGroup[1] = RNIL;
        loopNGPtr.p->nodesInGroup[2] = RNIL;
        loopNGPtr.p->nodesInGroup[3] = RNIL;
        loopNGPtr.p->nextReplicaNode = 0;
        loopNGPtr.p->nodeCount = 0;
        loopNGPtr.p->activeTakeOver = false;
        loopNGPtr.p->nodegroupIndex = RNIL;
        loopNGPtr.p->m_ref_count = 0;
        loopNGPtr.p->m_next_log_part = 0;
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
        c_replicaRecordPool.seizeId(initReplicaPtr, initReplicaPtr.i);
	initReplicaPtr.p->lcpIdStarted = 0;
	initReplicaPtr.p->lcpOngoingFlag = false;
        c_replicaRecordPool.releaseLast(initReplicaPtr);
      }//for
      cnoFreeReplicaRec = creplicaFileSize;
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
      jam();
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

/**
 * RCU lock must be held on table while calling this method when
 * not in recovery.
 */
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

  replicatePtr.p->nextPool = RNIL;
  fragPtr.p->noOldStoredReplicas++;
  losReplicaPtr.i = fragPtr.p->oldStoredReplicas;
  if (losReplicaPtr.i == RNIL) {
    jam();
    fragPtr.p->oldStoredReplicas = replicatePtr.i;
    return;
  }//if
  c_replicaRecordPool.getPtr(losReplicaPtr);
  while (losReplicaPtr.p->nextPool != RNIL) {
    jam();
    losReplicaPtr.i = losReplicaPtr.p->nextPool;
    c_replicaRecordPool.getPtr(losReplicaPtr);
  }//if
  losReplicaPtr.p->nextPool = replicatePtr.i;
}//Dbdih::linkOldStoredReplica()

void Dbdih::linkStoredReplica(FragmentstorePtr fragPtr,
                              ReplicaRecordPtr replicatePtr)
{
  ReplicaRecordPtr lsrReplicaPtr;

  fragPtr.p->noStoredReplicas++;
  replicatePtr.p->nextPool = RNIL;
  lsrReplicaPtr.i = fragPtr.p->storedReplicas;
  if (fragPtr.p->storedReplicas == RNIL) {
    jam();
    fragPtr.p->storedReplicas = replicatePtr.i;
    return;
  }//if
  c_replicaRecordPool.getPtr(lsrReplicaPtr);
  while (lsrReplicaPtr.p->nextPool != RNIL) {
    jam();
    lsrReplicaPtr.i = lsrReplicaPtr.p->nextPool;
    c_replicaRecordPool.getPtr(lsrReplicaPtr);
  }//if
  lsrReplicaPtr.p->nextPool = replicatePtr.i;
}//Dbdih::linkStoredReplica()

/*************************************************************************/
/*        MAKE NODE GROUPS BASED ON THE LIST OF NODES RECEIVED FROM CNTR */
/*************************************************************************/
void
Dbdih::add_nodegroup(NodeGroupRecordPtr NGPtr)
{
  if (NGPtr.p->nodegroupIndex == RNIL)
  {
    jam();
    NGPtr.p->nodegroupIndex = cnoOfNodeGroups;
    c_node_groups[cnoOfNodeGroups++] = NGPtr.i;
  }
}

void
Dbdih::inc_ng_refcount(Uint32 i)
{
  NodeGroupRecordPtr NGPtr;
  NGPtr.i = i;
  ptrCheckGuard(NGPtr, MAX_NDB_NODE_GROUPS, nodeGroupRecord);
  NGPtr.p->m_ref_count++;
}

void
Dbdih::dec_ng_refcount(Uint32 i)
{
  NodeGroupRecordPtr NGPtr;
  NGPtr.i = i;
  ptrCheckGuard(NGPtr, MAX_NDB_NODE_GROUPS, nodeGroupRecord);
  ndbrequire(NGPtr.p->m_ref_count);
  NGPtr.p->m_ref_count--;
}

void Dbdih::makeNodeGroups(Uint32 nodeArray[]) 
{
  NodeGroupRecordPtr NGPtr;
  NodeRecordPtr mngNodeptr;
  Uint32 j;

  /**-----------------------------------------------------------------------
   * ASSIGN ALL ACTIVE NODES INTO NODE GROUPS. HOT SPARE NODES ARE ASSIGNED 
   * TO NODE GROUP ZNIL
   *-----------------------------------------------------------------------*/
  cnoOfNodeGroups = 0;
  for (Uint32 i = 0; nodeArray[i] != RNIL; i++)
  {
    jam();
    mngNodeptr.i = nodeArray[i];
    ptrCheckGuard(mngNodeptr, MAX_NDB_NODES, nodeRecord);
    if (mngNodeptr.p->nodeGroup == NDB_NO_NODEGROUP)
    {
      jam();
      mngNodeptr.p->nodeGroup = ZNIL;
      g_eventLogger->info("setting nodeGroup = ZNIL for node %u",
                          mngNodeptr.i);
    }
    else if (mngNodeptr.p->nodeGroup != RNIL)
    {
      jam();
      NGPtr.i = mngNodeptr.p->nodeGroup;
      ptrCheckGuard(NGPtr, MAX_NDB_NODE_GROUPS, nodeGroupRecord);
      arrGuard(NGPtr.p->nodeCount, MAX_REPLICAS);
      NGPtr.p->nodesInGroup[NGPtr.p->nodeCount++] = mngNodeptr.i;

      add_nodegroup(NGPtr);
    }
  }
  NGPtr.i = 0;
  for (; NGPtr.i < MAX_NDB_NODE_GROUPS; NGPtr.i++)
  {
    jam();
    ptrAss(NGPtr, nodeGroupRecord);
    if (NGPtr.p->nodeCount < cnoReplicas)
      break;
  }

  for (Uint32 i = 0; nodeArray[i] != RNIL; i++)
  {
    jam();
    mngNodeptr.i = nodeArray[i];
    ptrCheckGuard(mngNodeptr, MAX_NDB_NODES, nodeRecord);
    if (mngNodeptr.p->nodeGroup == RNIL)
    {
      mngNodeptr.p->nodeGroup = NGPtr.i;
      NGPtr.p->nodesInGroup[NGPtr.p->nodeCount++] = mngNodeptr.i;

      add_nodegroup(NGPtr);

      if (NGPtr.p->nodeCount == cnoReplicas)
      {
        jam();
        for (; NGPtr.i < MAX_NDB_NODE_GROUPS; NGPtr.i++)
        {
          jam();
          ptrAss(NGPtr, nodeGroupRecord);
          if (NGPtr.p->nodeCount < cnoReplicas)
            break;
        }
      }
    }
  }

  Uint32 maxNG = 0;
  for (Uint32 i = 0; i<cnoOfNodeGroups; i++)
  {
    jam();
    NGPtr.i = c_node_groups[i];
    ptrCheckGuard(NGPtr, MAX_NDB_NODE_GROUPS, nodeGroupRecord);
    if (NGPtr.p->nodeCount == 0)
    {
      jam();
    }
    else if (NGPtr.p->nodeCount != cnoReplicas)
    {
      ndbrequire(false);
    }
    else
    {
      if (NGPtr.i > maxNG)
      {
        maxNG = NGPtr.i;
      }
    }
  }

  ndbrequire(csystemnodes < MAX_NDB_NODES);

  /**
   * Init sysfile
   */
  for(Uint32 i = 0; i < MAX_NDB_NODES; i++)
  {
    jam();
    Sysfile::setNodeGroup(i, SYSFILE->nodeGroups, NO_NODE_GROUP_ID);
    Sysfile::setNodeStatus(i, SYSFILE->nodeStatus,Sysfile::NS_NotDefined);
  }

  for (Uint32 i = 0; nodeArray[i] != RNIL; i++)
  {
    jam();
    Uint32 nodeId = mngNodeptr.i = nodeArray[i];
    ptrCheckGuard(mngNodeptr, MAX_NDB_NODES, nodeRecord);

    if (mngNodeptr.p->nodeGroup != ZNIL)
    {
      jam();
      Sysfile::setNodeGroup(nodeId, SYSFILE->nodeGroups,
                            mngNodeptr.p->nodeGroup);

      if (mngNodeptr.p->nodeStatus == NodeRecord::ALIVE)
      {
        jam();
        mngNodeptr.p->activeStatus = Sysfile::NS_Active;
      }
      else
      {
        jam();
        mngNodeptr.p->activeStatus = Sysfile::NS_NotActive_NotTakenOver;
      }
    }
    else
    {
      jam();
      Sysfile::setNodeGroup(mngNodeptr.i, SYSFILE->nodeGroups,
                            NO_NODE_GROUP_ID);
      mngNodeptr.p->activeStatus = Sysfile::NS_Configured;
    }
    Sysfile::setNodeStatus(nodeId, SYSFILE->nodeStatus,
                           mngNodeptr.p->activeStatus);
  }

  for (Uint32 i = 0; i<cnoOfNodeGroups; i++)
  {
    jam();
    bool alive = false;
    NodeGroupRecordPtr NGPtr;
    NGPtr.i = c_node_groups[i];
    ptrCheckGuard(NGPtr, MAX_NDB_NODE_GROUPS, nodeGroupRecord);
    for (j = 0; j<NGPtr.p->nodeCount; j++)
    {
      jam();
      mngNodeptr.i = NGPtr.p->nodesInGroup[j];
      ptrCheckGuard(mngNodeptr, MAX_NDB_NODES, nodeRecord);
      if (checkNodeAlive(NGPtr.p->nodesInGroup[j]))
      {
	alive = true;
	break;
      }
    }
    
    if (!alive)
    {
      char buf[255];
      BaseString::snprintf
        (buf, sizeof(buf), 
         "Illegal initial start, no alive node in nodegroup %u", i);
      progError(__LINE__, 
                NDBD_EXIT_INSUFFICENT_NODES,
                buf);
    }
  }
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
  jamNoBlock();
  CheckNodeGroups* sd = (CheckNodeGroups*)&signal->theData[0];

  bool direct = (sd->requestType & CheckNodeGroups::Direct);
  bool ok = false;
  switch(sd->requestType & ~CheckNodeGroups::Direct){
  case CheckNodeGroups::ArbitCheck:{
    ok = true;
    jamNoBlock();
    unsigned missall = 0;
    unsigned haveall = 0;
    for (Uint32 i = 0; i < cnoOfNodeGroups; i++) {
      jamNoBlock();
      NodeGroupRecordPtr ngPtr;
      ngPtr.i = c_node_groups[i];
      ptrCheckGuard(ngPtr, MAX_NDB_NODE_GROUPS, nodeGroupRecord);
      Uint32 count = 0;
      for (Uint32 j = 0; j < ngPtr.p->nodeCount; j++) {
	jamNoBlock();
	Uint32 nodeId = ngPtr.p->nodesInGroup[j];
	if (sd->mask.get(nodeId)) {
	  jamNoBlock();
	  count++;
	}//if
      }//for
      if (count == 0) {
	jamNoBlock();
	missall++;
      }//if
      if (count == ngPtr.p->nodeCount) {
	haveall++;
      }//if
    }//for

    if (missall) {
      jamNoBlock();
      sd->output = CheckNodeGroups::Lose;
    } else if (haveall) {
      jamNoBlock();
      sd->output = CheckNodeGroups::Win;
    } else {
      jamNoBlock();
      sd->output = CheckNodeGroups::Partitioning;
    }//if
  }
    break;
  case CheckNodeGroups::GetNodeGroup:{
    ok = true;
    Uint32 ng = Sysfile::getNodeGroup(getOwnNodeId(), SYSFILE->nodeGroups);
    if (ng == NO_NODE_GROUP_ID)
      ng = RNIL;
    sd->output = ng;
    break;
  }
  case CheckNodeGroups::GetNodeGroupMembers: {
    ok = true;
    Uint32 ng = Sysfile::getNodeGroup(sd->nodeId, SYSFILE->nodeGroups);
    if (ng == NO_NODE_GROUP_ID)
      ng = RNIL;

    sd->output = ng;
    sd->mask.clear();

    NodeGroupRecordPtr ngPtr;
    ngPtr.i = ng;
    if (ngPtr.i != RNIL)
    {
      jamNoBlock();
      ptrAss(ngPtr, nodeGroupRecord);
      for (Uint32 j = 0; j < ngPtr.p->nodeCount; j++) {
        jamNoBlock();
        sd->mask.set(ngPtr.p->nodesInGroup[j]);
      }
    }
    break;
  }
  case CheckNodeGroups::GetDefaultFragments:
    jamNoBlock();
    ok = true;
    sd->output = getFragmentCount(sd->partitionBalance,
                                  cnoOfNodeGroups + sd->extraNodeGroups,
                                  cnoReplicas,
                                  getFragmentsPerNode());
    break;
  case CheckNodeGroups::GetDefaultFragmentsFullyReplicated:
    jamNoBlock();
    ok = true;
    sd->output = getFragmentCount(sd->partitionBalance,
                                  1,
                                  cnoReplicas,
                                  getFragmentsPerNode());
    break;
  }
  ndbrequire(ok);
  
  if (!direct)
    sendSignal(sd->blockRef, GSN_CHECKNODEGROUPSCONF, signal,
	       CheckNodeGroups::SignalLength, JBB);
}//Dbdih::execCHECKNODEGROUPSREQ()

Uint32
Dbdih::getFragmentCount(Uint32 partitionBalance,
                        Uint32 numOfNodeGroups,
                        Uint32 numOfReplicas,
                        Uint32 numOfLDMs) const
{
  switch (partitionBalance)
  {
  case NDB_PARTITION_BALANCE_FOR_RP_BY_LDM:
    return numOfNodeGroups * numOfReplicas * numOfLDMs;
  case NDB_PARTITION_BALANCE_FOR_RA_BY_LDM:
    return numOfNodeGroups * numOfLDMs;
  case NDB_PARTITION_BALANCE_FOR_RP_BY_NODE:
    return numOfNodeGroups * numOfReplicas;
  case NDB_PARTITION_BALANCE_FOR_RA_BY_NODE:
    return numOfNodeGroups;
  case NDB_PARTITION_BALANCE_FOR_RA_BY_LDM_X_2:
    return numOfNodeGroups * numOfLDMs * 2;
  case NDB_PARTITION_BALANCE_FOR_RA_BY_LDM_X_3:
    return numOfNodeGroups * numOfLDMs * 3;
  case NDB_PARTITION_BALANCE_FOR_RA_BY_LDM_X_4:
    return numOfNodeGroups * numOfLDMs * 4;

  case NDB_PARTITION_BALANCE_SPECIFIC:
  default:
    ndbrequire(false);
    return 0;
  }
}

void
Dbdih::makePrnList(ReadNodesConf * readNodes, Uint32 nodeArray[])
{
  cfirstAliveNode = RNIL;
  ndbrequire(con_lineNodes > 0);
  ndbrequire(csystemnodes < MAX_NDB_NODES);
  for (Uint32 i = 0; i < csystemnodes; i++) {
    NodeRecordPtr nodePtr;
    jam();
    nodePtr.i = nodeArray[i];
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
    initNodeRecord(nodePtr);
    if (NdbNodeBitmask::get(readNodes->inactiveNodes, nodePtr.i) == false){
      jam();
      nodePtr.p->nodeStatus = NodeRecord::ALIVE;
      nodePtr.p->useInTransactions = true;
      nodePtr.p->copyCompleted = 1;
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
void Dbdih::newCrashedReplica(ReplicaRecordPtr ncrReplicaPtr)
{
  /*----------------------------------------------------------------------*/
  /*       SET THE REPLICA_LAST_GCI OF THE CRASHED REPLICA TO LAST GCI    */
  /*       EXECUTED BY THE FAILED NODE.                                   */
  /*----------------------------------------------------------------------*/
  /*       WE HAVE A NEW CRASHED REPLICA. INITIATE CREATE GCI TO INDICATE */
  /*       THAT THE NEW REPLICA IS NOT STARTED YET AND REPLICA_LAST_GCI IS*/
  /*       SET TO -1 TO INDICATE THAT IT IS NOT DEAD YET.                 */
  /*----------------------------------------------------------------------*/
  Uint32 nodeId = ncrReplicaPtr.p->procNode;
  Uint32 lastGCI = SYSFILE->lastCompletedGCI[nodeId];
  if (ncrReplicaPtr.p->noCrashedReplicas + 1 == MAX_CRASHED_REPLICAS)
  {
    jam();
    packCrashedReplicas(ncrReplicaPtr);
  }
  
  Uint32 noCrashedReplicas = ncrReplicaPtr.p->noCrashedReplicas;
  arrGuardErr(ncrReplicaPtr.p->noCrashedReplicas + 1, MAX_CRASHED_REPLICAS,
              NDBD_EXIT_MAX_CRASHED_REPLICAS);

  if (noCrashedReplicas > 0 &&
      ncrReplicaPtr.p->replicaLastGci[noCrashedReplicas - 1] == lastGCI)
  {
    jam();
    /**
     * Don't add another redo-interval, that already exist
     *  instead initalize new
     */
    ncrReplicaPtr.p->createGci[ncrReplicaPtr.p->noCrashedReplicas] =
      ZINIT_CREATE_GCI;
    ncrReplicaPtr.p->replicaLastGci[ncrReplicaPtr.p->noCrashedReplicas] =
      ZINIT_REPLICA_LAST_GCI;
  }
  else if (ncrReplicaPtr.p->createGci[noCrashedReplicas] <= lastGCI)
  {
    jam();
    ncrReplicaPtr.p->replicaLastGci[ncrReplicaPtr.p->noCrashedReplicas] =
      lastGCI;
    ncrReplicaPtr.p->noCrashedReplicas = ncrReplicaPtr.p->noCrashedReplicas + 1;
    ncrReplicaPtr.p->createGci[ncrReplicaPtr.p->noCrashedReplicas] =
      ZINIT_CREATE_GCI;
    ncrReplicaPtr.p->replicaLastGci[ncrReplicaPtr.p->noCrashedReplicas] =
      ZINIT_REPLICA_LAST_GCI;
  }
  else
  {
    /**
     * This can happen if createGci is set
     *   (during sendUpdateFragStateReq(COMMIT_STORED))
     *   but SYSFILE->lastCompletedGCI[nodeId] has not been updated
     *   as node has not yet completed it's first LCP, causing it to return
     *   GCP_SAVEREF (which makes SYSFILE->lastCompletedGCI[nodeId] be left
     *   untouched)
     *
     * I.e crash during node-restart
     */
    ncrReplicaPtr.p->createGci[noCrashedReplicas] = ZINIT_CREATE_GCI;
  }
  
}//Dbdih::newCrashedReplica()

/*************************************************************************/
/*       AT NODE FAILURE DURING START OF A NEW NODE WE NEED TO RESET A   */
/*       SET OF VARIABLES CONTROLLING THE START AND INDICATING ONGOING   */
/*       START OF A NEW NODE.                                            */
/*************************************************************************/
void Dbdih::nodeResetStart(Signal *signal)
{
  jam();
  Uint32 startGCP = c_nodeStartMaster.blockGcp;

  c_nodeStartSlave.nodeId = 0;
  c_nodeStartMaster.startNode = RNIL;
  c_nodeStartMaster.failNr = cfailurenr;
  c_nodeStartMaster.activeState = false;
  c_nodeStartMaster.blockGcp = 0;
  c_nodeStartMaster.m_outstandingGsn = 0;

  if (startGCP == 2) // effective
  {
    jam();
    ndbrequire(isMaster());
    ndbrequire(m_micro_gcp.m_master.m_state == MicroGcp::M_GCP_IDLE);
    signal->theData[0] = DihContinueB::ZSTART_GCP;
    sendSignal(reference(), GSN_CONTINUEB, signal, 1, JBB);
  }
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
  ndbrequire(replicaPtr.p->noCrashedReplicas <= MAX_CRASHED_REPLICAS);
  for (Uint32 i = 0; i < replicaPtr.p->noCrashedReplicas; i++) {
    jam();
    replicaPtr.p->createGci[i] = replicaPtr.p->createGci[i + 1];
    replicaPtr.p->replicaLastGci[i] = replicaPtr.p->replicaLastGci[i + 1];
  }//for
  replicaPtr.p->noCrashedReplicas--;
  replicaPtr.p->createGci[replicaPtr.p->noCrashedReplicas + 1] =
    ZINIT_CREATE_GCI;
  replicaPtr.p->replicaLastGci[replicaPtr.p->noCrashedReplicas + 1] =
    ZINIT_REPLICA_LAST_GCI;
}//Dbdih::packCrashedReplicas()

void
Dbdih::mergeCrashedReplicas(ReplicaRecordPtr replicaPtr)
{
  /**
   * merge adjacent redo-intervals
   */
  for (Uint32 i = replicaPtr.p->noCrashedReplicas; i > 0; i--)
  {
    jam();
    if (replicaPtr.p->createGci[i] == 1 + replicaPtr.p->replicaLastGci[i-1])
    {
      jam();
      replicaPtr.p->replicaLastGci[i-1] = replicaPtr.p->replicaLastGci[i];
      replicaPtr.p->createGci[i] = ZINIT_CREATE_GCI;
      replicaPtr.p->replicaLastGci[i] = ZINIT_REPLICA_LAST_GCI;
      replicaPtr.p->noCrashedReplicas--;
    }
    else
    {
      jam();
      break;
    }
  }
}

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
    c_replicaRecordPool.getPtr(prReplicaPtr);
    prReplicaPtr.i = prReplicaPtr.p->nextPool;
  }//while
  /* --------------------------------------------------------------------- */
  /*       LIST OF STORED REPLICAS WILL BE EMPTY NOW.                      */
  /* --------------------------------------------------------------------- */
  if (prevReplica != RNIL) {
    prReplicaPtr.i = prevReplica;
    c_replicaRecordPool.getPtr(prReplicaPtr);
    prReplicaPtr.p->nextPool = fragPtr.p->oldStoredReplicas;
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
  fragPtr.p->distributionKey = TdistKey;

  fragPtr.p->m_log_part_id = readPageWord(rf);
  if (!ndbd_128_instances_address(getMinVersion()))
  {
    jam();
    /**
     * Limit log-part to 0-3 as older version didn't handle
     *   getting requests to instances > 4
     *   (in reality 7 i think...but that is useless as log-part dividor anyway)
     */
    fragPtr.p->m_log_part_id %= 4;
  }

  /* Older nodes stored unlimited log part ids in the fragment definition, 
   * now we constrain them to a valid range of actual values for this node.  
   * Here we ensure that unlimited log part ids fit in the value range for
   * this node.
   */
  ndbrequire(globalData.ndbLogParts <= NDBMT_MAX_WORKER_INSTANCES);

  fragPtr.p->m_log_part_id %= globalData.ndbLogParts;

  ndbrequire(fragPtr.p->m_log_part_id < NDBMT_MAX_WORKER_INSTANCES);

  inc_ng_refcount(getNodeGroup(fragPtr.p->preferredPrimary));
}//Dbdih::readFragment()

Uint32 Dbdih::readPageWord(RWFragment* rf) 
{
  if (rf->wordIndex >= 2048) {
    jam();
    ndbrequire(rf->wordIndex == 2048);
    rf->pageIndex++;
    ndbrequire(rf->pageIndex < NDB_ARRAY_SIZE(rf->rwfTabPtr.p->pageRef));
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

  /**
   * Initialise LCP inclusion data, this is to enable us to be included
   * in an LCP during a node restart.
   */
  readReplicaPtr.p->fragId = rf->fragId;
  readReplicaPtr.p->tableId = rf->rwfTabPtr.i;
  readReplicaPtr.p->lcpOngoingFlag = false;

  for (i = 0; i < MAX_LCP_STORED; i++) {
    readReplicaPtr.p->maxGciCompleted[i] = readPageWord(rf);
    readReplicaPtr.p->maxGciStarted[i] = readPageWord(rf);
    readReplicaPtr.p->lcpId[i] = readPageWord(rf);
    readReplicaPtr.p->lcpStatus[i] = readPageWord(rf);
  }//for
  const Uint32 noCrashedReplicas = readReplicaPtr.p->noCrashedReplicas;
  ndbrequire(noCrashedReplicas < MAX_CRASHED_REPLICAS);
  for (i = 0; i < noCrashedReplicas; i++) {
    readReplicaPtr.p->createGci[i] = readPageWord(rf);
    readReplicaPtr.p->replicaLastGci[i] = readPageWord(rf);
  }//for
  for(i = noCrashedReplicas; i<MAX_CRASHED_REPLICAS; i++){
    readReplicaPtr.p->createGci[i] = readPageWord(rf);
    readReplicaPtr.p->replicaLastGci[i] = readPageWord(rf);
  }
}//Dbdih::readReplica()

/**
 * This method is useful when we read the table distribution information from
 * the master node. In this case with the new PAUSE LCP protocol we need to
 * perform the functionality of the initLcpLab while copying the table to
 * ensure that we're a full DIH participant in the LCP when the copying of
 * the meta data has been completed.
 *
 * For all other cases the tabLcpStatus is TLS_COMPLETED and thus the method
 * will be ignored.
 */
void Dbdih::updateLcpInfo(TabRecord *regTabPtr,
                          Fragmentstore *regFragPtr,
                          ReplicaRecord *regReplicaPtr)
{
  if (regTabPtr->tabLcpStatus == TabRecord::TLS_ACTIVE)
  {
    jam();
    Uint32 lastLcpNo = prevLcpNo(regReplicaPtr->nextLcp);
    if (c_lcp_id_while_copy_meta_data != RNIL &&
        regReplicaPtr->lcpId[lastLcpNo] < c_lcp_id_while_copy_meta_data &&
        c_lcpState.m_participatingLQH.get(regReplicaPtr->procNode))
    {
      /**
       * If the copy table indicating that the table is participating in
       * an LCP, if the fragment replica hasn't performed this LCP yet,
       * and the replica node is participating in the LCP at hand now.
       *
       * This code executes in the starting node after the LCP being
       * paused and we are included into the LCP protocol immediately
       * after copying the meta data. We received the bitmap of
       * participating LCP nodes just before the copying of meta
       * data started.
       */
      jam();
      regReplicaPtr->lcpOngoingFlag = true;
      regFragPtr->noLcpReplicas++;
#if 0
      g_eventLogger->info("LCP Ongoing: TableId: %u, fragId: %u, node: %u"
                          " lastLcpNo: %u, lastLcpId: %u, lcpId: %u",
      regReplicaPtr->tableId,
      regReplicaPtr->fragId,
      regReplicaPtr->procNode,
      lastLcpNo,
      regReplicaPtr->lcpId[lastLcpNo],
      c_lcp_id_while_copy_meta_data);
#endif
    }
  }
}

void Dbdih::readReplicas(RWFragment* rf,
                         TabRecord *regTabPtr,
                         FragmentstorePtr fragPtr)
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
  fragPtr.p->noLcpReplicas = 0;
  Uint32 replicaIndex = 0;
  ndbrequire(noStoredReplicas + noOldStoredReplicas <= MAX_REPLICAS);
  for (i = 0; i < noStoredReplicas; i++) 
  {
    seizeReplicaRec(newReplicaPtr);
    readReplica(rf, newReplicaPtr);
    ndbrequire(replicaIndex < MAX_REPLICAS);
    fragPtr.p->activeNodes[replicaIndex] = newReplicaPtr.p->procNode;
    replicaIndex++;
    linkStoredReplica(fragPtr, newReplicaPtr);
    updateLcpInfo(regTabPtr, fragPtr.p, newReplicaPtr.p);
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
  Uint32 section[2 * NDB_ARRAY_SIZE(tab->pageRef)];
  for (Uint32 i = 0; i < tab->noPages; i++)
  {
    section[(2 * i) + 0] = tab->pageRef[i];
    section[(2 * i) + 1] = i;
  }
  LinearSectionPtr ptr[3];
  ptr[0].p = section;
  ptr[0].sz = 2 * tab->noPages;
  sendSignal(NDBFS_REF, GSN_FSREADREQ, signal, 6, JBA, ptr, 1);
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
  ndbrequire(tabPtr.p->noPages <= NDB_ARRAY_SIZE(tabPtr.p->pageRef));
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
                                 ReplicaRecordPtr replicatePtr,
				 bool temporary)
{
  if (!temporary)
  {
    jam();
    newCrashedReplica(replicatePtr);
  }
  else
  {
    jam();
  }
  removeStoredReplica(fragPtr, replicatePtr);
  linkOldStoredReplica(fragPtr, replicatePtr);
  ndbrequire(fragPtr.p->storedReplicas != RNIL);
}//Dbdih::removeNodeFromStored()

/*************************************************************************/
/*       REMOVE ANY OLD CRASHED REPLICAS THAT ARE NOT RESTORABLE ANY MORE*/
/*************************************************************************/
void Dbdih::removeOldCrashedReplicas(Uint32 tab, Uint32 frag,
                                     ReplicaRecordPtr rocReplicaPtr)
{
  mergeCrashedReplicas(rocReplicaPtr);
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

  while (rocReplicaPtr.p->createGci[0] < SYSFILE->keepGCI)
  {
    jam();
    /* --------------------------------------------------------------------- */
    /*       MOVE FORWARD THE CREATE GCI TO A GCI THAT CAN BE USED. WE HAVE  */
    /*       NO CERTAINTY IN FINDING ANY LOG RECORDS FROM OLDER GCI'S.       */
    /* --------------------------------------------------------------------- */
    rocReplicaPtr.p->createGci[0] = SYSFILE->keepGCI;

    if (rocReplicaPtr.p->noCrashedReplicas)
    {
      /**
       * a REDO interval while is from 78 to 14 is not usefull
       *   but rather harmful, remove it...
       */
      if (rocReplicaPtr.p->createGci[0] > rocReplicaPtr.p->replicaLastGci[0])
      {
        jam();
        packCrashedReplicas(rocReplicaPtr);
      }
    }
  }
}

void Dbdih::removeOldStoredReplica(FragmentstorePtr fragPtr,
                                   ReplicaRecordPtr replicatePtr) 
{
  ReplicaRecordPtr rosTmpReplicaPtr;
  ReplicaRecordPtr rosPrevReplicaPtr;

  fragPtr.p->noOldStoredReplicas--;
  if (fragPtr.p->oldStoredReplicas == replicatePtr.i) {
    jam();
    fragPtr.p->oldStoredReplicas = replicatePtr.p->nextPool;
  } else {
    rosPrevReplicaPtr.i = fragPtr.p->oldStoredReplicas;
    c_replicaRecordPool.getPtr(rosPrevReplicaPtr);
    rosTmpReplicaPtr.i = rosPrevReplicaPtr.p->nextPool;
    while (rosTmpReplicaPtr.i != replicatePtr.i) {
      jam();
      c_replicaRecordPool.getPtr(rosTmpReplicaPtr);
      rosPrevReplicaPtr = rosTmpReplicaPtr;
      rosTmpReplicaPtr.i = rosTmpReplicaPtr.p->nextPool;
    }//if
    rosPrevReplicaPtr.p->nextPool = replicatePtr.p->nextPool;
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
    fragPtr.p->storedReplicas = replicatePtr.p->nextPool;
  } else {
    jam();
    rsrPrevReplicaPtr.i = fragPtr.p->storedReplicas;
    rsrTmpReplicaPtr.i = fragPtr.p->storedReplicas;
    c_replicaRecordPool.getPtr(rsrTmpReplicaPtr);
    rsrTmpReplicaPtr.i = rsrTmpReplicaPtr.p->nextPool;
    while (rsrTmpReplicaPtr.i != replicatePtr.i) {
      jam();
      rsrPrevReplicaPtr.i = rsrTmpReplicaPtr.i;
      c_replicaRecordPool.getPtr(rsrTmpReplicaPtr);
      rsrTmpReplicaPtr.i = rsrTmpReplicaPtr.p->nextPool;
    }//while
    c_replicaRecordPool.getPtr(rsrPrevReplicaPtr);
    rsrPrevReplicaPtr.p->nextPool = replicatePtr.p->nextPool;
  }//if
}//Dbdih::removeStoredReplica()

/*************************************************************************/
/*       REMOVE ALL TOO NEW CRASHED REPLICAS THAT IS IN THIS REPLICA.    */
/*************************************************************************/
void Dbdih::removeTooNewCrashedReplicas(ReplicaRecordPtr rtnReplicaPtr, Uint32 lastCompletedGCI)
{
  while (rtnReplicaPtr.p->noCrashedReplicas > 0) {
    jam();
    /* --------------------------------------------------------------------- */
    /*       REMOVE ALL REPLICAS THAT ONLY LIVED IN A PERIOD THAT HAVE BEEN  */
    /*       REMOVED FROM THE RESTART INFORMATION SINCE THE RESTART FAILED   */
    /*       TOO MANY TIMES.                                                 */
    /* --------------------------------------------------------------------- */
    arrGuard(rtnReplicaPtr.p->noCrashedReplicas - 1, MAX_CRASHED_REPLICAS);
    if (rtnReplicaPtr.p->createGci[rtnReplicaPtr.p->noCrashedReplicas - 1] > lastCompletedGCI)
    {
      jam();
      rtnReplicaPtr.p->createGci[rtnReplicaPtr.p->noCrashedReplicas - 1] = 
	ZINIT_CREATE_GCI;
      rtnReplicaPtr.p->replicaLastGci[rtnReplicaPtr.p->noCrashedReplicas - 1] = 
	ZINIT_REPLICA_LAST_GCI;
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
bool
Dbdih::setup_create_replica(FragmentstorePtr fragPtr,
			    CreateReplicaRecord* createReplicaPtrP,
			    ConstPtr<ReplicaRecord> replicaPtr)
{
  createReplicaPtrP->dataNodeId = replicaPtr.p->procNode;
  createReplicaPtrP->replicaRec = replicaPtr.i;

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
  if (!result) 
  {
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
    createReplicaPtrP->lcpNo = ZNIL;
  } 
  else 
  {
    jam();
    /* --------------------------------------------------------------- */
    /* WE FOUND A PROPER LOCAL CHECKPOINT TO RESTART FROM.             */
    /* SET LOCAL CHECKPOINT ID AND LOCAL CHECKPOINT NUMBER.            */
    /* --------------------------------------------------------------- */
    createReplicaPtrP->lcpNo = startLcpNo;
    arrGuard(startLcpNo, MAX_LCP_STORED);
    createReplicaPtrP->createLcpId = replicaPtr.p->lcpId[startLcpNo];
  }//if
  
  
  /* ----------------------------------------------------------------- */
  /*   WE HAVE EITHER FOUND A LOCAL CHECKPOINT OR WE ARE PLANNING TO   */
  /*   EXECUTE THE LOG FROM THE INITIAL CREATION OF THE TABLE. IN BOTH */
  /*   CASES WE NEED TO FIND A SET OF LOGS THAT CAN EXECUTE SUCH THAT  */
  /*   WE RECOVER TO THE SYSTEM RESTART GLOBAL CHECKPOINT.             */
  /* -_--------------------------------------------------------------- */
  return findLogNodes(createReplicaPtrP, fragPtr, startGci, stopGci);
}			    

void Dbdih::searchStoredReplicas(FragmentstorePtr fragPtr) 
{
  Uint32 nextReplicaPtrI;
  Ptr<ReplicaRecord> replicaPtr;

  replicaPtr.i = fragPtr.p->storedReplicas;
  while (replicaPtr.i != RNIL) {
    jam();
    c_replicaRecordPool.getPtr(replicaPtr);
    nextReplicaPtrI = replicaPtr.p->nextPool;
    ConstPtr<ReplicaRecord> constReplicaPtr;
    constReplicaPtr.i = replicaPtr.i;
    constReplicaPtr.p = replicaPtr.p;
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
	
	/**
	 * Should have been checked in resetReplicaSr
	 */
	ndbrequire(setup_create_replica(fragPtr,
					createReplicaPtr.p, 
					constReplicaPtr));
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
/*       SEND UPDATE_FRAG_STATEREQ TO ALL NODES IN THE NDB CLUSTER.      */
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

    BlockReference ref = numberToRef(DBLQH, replicaPtr.p->dataNodeId);

    StartFragReq * const startFragReq = (StartFragReq *)&signal->theData[0];
    startFragReq->userPtr = replicaPtr.p->replicaRec;
    startFragReq->userRef = reference();
    startFragReq->lcpNo = replicaPtr.p->lcpNo;
    startFragReq->lcpId = replicaPtr.p->createLcpId;
    startFragReq->tableId = tabPtr.i;
    startFragReq->fragId = fragId;
    startFragReq->requestInfo = StartFragReq::SFR_RESTORE_LCP;

    if(ERROR_INSERTED(7072) || ERROR_INSERTED(7074)){
      jam();
      const Uint32 noNodes = replicaPtr.p->noLogNodes;
      Uint32 start = replicaPtr.p->logStartGci[noNodes - 1];
      const Uint32 stop  = replicaPtr.p->logStopGci[noNodes - 1];

      for(Uint32 i = noNodes; i < MAX_LOG_EXEC && (stop - start) > 0; i++){
	replicaPtr.p->noLogNodes++;
	replicaPtr.p->logStopGci[i - 1] = start;
	
	replicaPtr.p->logNodeId[i] = replicaPtr.p->logNodeId[i-1];
	replicaPtr.p->logStartGci[i] = start + 1;
	replicaPtr.p->logStopGci[i] = stop;      
	start += 1;
      }
    }
    
    startFragReq->noOfLogNodes = replicaPtr.p->noLogNodes;
    
    for (Uint32 i = 0; i < MAX_LOG_EXEC ; i++) {
      startFragReq->lqhLogNode[i] = replicaPtr.p->logNodeId[i];
      startFragReq->startGci[i] = replicaPtr.p->logStartGci[i];
      startFragReq->lastGci[i] = replicaPtr.p->logStopGci[i];
    }//for    

    sendSignal(ref, GSN_START_FRAGREQ, signal, 
	       StartFragReq::SignalLength, JBB);
  }//for
}//Dbdih::sendStartFragreq()

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
    if(nodePtr.p->nodeStatus == NodeRecord::ALIVE)
    {
      jam();
      if (nodePtr.p->m_inclDihLcp)
      {
        jam();
        c_lcpState.m_participatingDIH.set(nodePtr.i);
      }

      if (nodePtr.p->copyCompleted)
      {
        jam();
	c_lcpState.m_participatingLQH.set(nodePtr.i);        
      }
      else if (nodePtr.p->activeStatus == Sysfile::NS_Configured)
      {
        jam();
        continue;
      }
      else
      {
        jam();
        nodePtr.p->activeStatus = Sysfile::NS_ActiveMissed_1;
      }
    }
    else if (nodePtr.p->activeStatus == Sysfile::NS_Configured)
    {
      jam();
      continue;
    }
    else if (nodePtr.p->activeStatus != Sysfile::NS_NotDefined)
    {
      jam();
      nodePtr.p->activeStatus = Sysfile::NS_ActiveMissed_1;
    }
  }
}//Dbdih::setLcpActiveStatusStart()

/*************************************************************************/
/*       SET LCP ACTIVE STATUS AT THE END OF A LOCAL CHECKPOINT.        */
/*************************************************************************/
void Dbdih::setLcpActiveStatusEnd(Signal* signal)
{
  NodeRecordPtr nodePtr;

  for (nodePtr.i = 1; nodePtr.i < MAX_NDB_NODES; nodePtr.i++) {
    jam();
    ptrAss(nodePtr, nodeRecord);
    if (c_lcpState.m_participatingLQH.get(nodePtr.i))
    {
      jam();
      nodePtr.p->copyCompleted = 1;
      if (! (nodePtr.p->activeStatus == Sysfile::NS_Configured))
      {
        jam();
        nodePtr.p->activeStatus = Sysfile::NS_Active;
      }
      else
      {
        jam();
        // Do nothing
      }
      if (nodePtr.p->nodeRecoveryStatus == NodeRecord::NODE_IN_LCP_WAIT_STATE)
      {
        jam();
        /**
         * This is a non-master node and this is the first time we heard this
         * node is alive and active. We set the node recovery status, this
         * status is only used in printouts if this node later becomes master
         * and the node is still alive and kicking. This means we have no
         * detailed information about its restart status.
         */
        setNodeRecoveryStatus(nodePtr.i, NodeRecord::NODE_ACTIVE);
      }
    }
    else if (nodePtr.p->activeStatus == Sysfile::NS_Configured)
    {
      jam();
      continue;
    }
    else if (nodePtr.p->activeStatus != Sysfile::NS_NotDefined)
    {
      jam();
      nodePtr.p->activeStatus = Sysfile::NS_ActiveMissed_1;
    }
  }
  
  c_lcpState.m_participatingDIH.clear();
  c_lcpState.m_participatingLQH.clear();

  if (isMaster()) {
    jam();
    setNodeRestartInfoBits(signal);
  }//if
}//Dbdih::setLcpActiveStatusEnd()

/*************************************************************************/
/* SET NODE ACTIVE STATUS AT SYSTEM RESTART AND WHEN UPDATED BY MASTER   */
/*************************************************************************/
void Dbdih::setNodeActiveStatus() 
{
  NodeRecordPtr snaNodeptr;

  for (snaNodeptr.i = 1; snaNodeptr.i < MAX_NDB_NODES; snaNodeptr.i++)
  {
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
    case Sysfile::NS_NotActive_NotTakenOver:
      jam();
      snaNodeptr.p->activeStatus = Sysfile::NS_NotActive_NotTakenOver;
      break;
    case Sysfile::NS_NotDefined:
      jam();
      snaNodeptr.p->activeStatus = Sysfile::NS_NotDefined;
      break;
    case Sysfile::NS_Configured:
      jam();
      snaNodeptr.p->activeStatus = Sysfile::NS_Configured;
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
  for (Ti = 0; Ti < cnoOfNodeGroups; Ti++) {
    NGPtr.i = c_node_groups[Ti];
    ptrCheckGuard(NGPtr, MAX_NDB_NODE_GROUPS, nodeGroupRecord);
    NGPtr.p->nodeCount = 0;
    NGPtr.p->nodegroupIndex = RNIL;
  }//for
  cnoOfNodeGroups = 0;
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
      ptrCheckGuard(NGPtr, MAX_NDB_NODE_GROUPS, nodeGroupRecord);
      NGPtr.p->nodesInGroup[NGPtr.p->nodeCount] = sngNodeptr.i;
      NGPtr.p->nodeCount++;
      add_nodegroup(NGPtr);
      break;
    case Sysfile::NS_NotDefined:
    case Sysfile::NS_Configured:
      jam();
      sngNodeptr.p->nodeGroup = ZNIL;
      break;
    default:
      ndbrequire(false);
      return;
      break;
    }//switch
  }//for
  sngNodeptr.i = getOwnNodeId();
  ptrCheckGuard(sngNodeptr, MAX_NDB_NODES, nodeRecord);
  NGPtr.i = sngNodeptr.p->nodeGroup;
  if (NGPtr.i == ZNIL)
  {
    jam();
    return;
  }
  ptrCheckGuard(NGPtr, MAX_NDB_NODE_GROUPS, nodeGroupRecord);
  if (NGPtr.p->nodeCount <= 1)
  {
    /**
     * Only one replica in this node group, so no neighbour.
     * Could also be a node in a new nodegroup, so effectively
     * it is part of no nodegroup and thus has no neighbours
     * in this case either.
     */
    jam();
    return;
  }
  ndbrequire(NGPtr.p->nodeCount <= MAX_REPLICAS);
  /**
   * Inform scheduler of our neighbour node to ensure the best
   * possible communication with this node. If more than two
   * replicas we will still only have one neighbour, so we will
   * have most communication with this neighbour node.
   */
  for (Uint32 i = 0; i < NGPtr.p->nodeCount; i++)
  {
    jam();
    Uint32 nodeId = NGPtr.p->nodesInGroup[i];
    if (nodeId != getOwnNodeId())
    {
      jam();
      ndbrequire(nodeId != 0 && nodeId < MAX_NODES);
      setNeighbourNode(nodeId);
    }
  }
}//Dbdih::setNodeGroups()

/*************************************************************************/
/* SET THE RESTART INFO BITS BASED ON THE NODES ACTIVE STATUS.           */
/*************************************************************************/
void Dbdih::setNodeRestartInfoBits(Signal * signal)
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

#ifdef ERROR_INSERT
  NdbNodeBitmask tmp;
#endif

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
    case Sysfile::NS_Configured:
      jam();
      tsnrNodeActiveStatus = Sysfile::NS_Configured;
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
    if (c_lcpState.m_participatingLQH.get(nodePtr.i))
    {
      jam();
      NdbNodeBitmask::set(SYSFILE->lcpActive, nodePtr.i);
    }//if
#ifdef ERROR_INSERT
    else if (Sysfile::getLCPOngoing(SYSFILE->systemRestartBits))
    {
      jam();
      if (nodePtr.p->activeStatus == Sysfile::NS_Active)
        tmp.set(nodePtr.i);
    }
#endif
  }//for

#ifdef ERROR_INSERT
  if (ERROR_INSERTED(7220) && !tmp.isclear())
  {
    jam();

    NdbNodeBitmask all;
    nodePtr.i = cfirstAliveNode;
    do {
      jam();
      ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
      all.set(nodePtr.i);
      nodePtr.i = nodePtr.p->nextNode;
    } while (nodePtr.i != RNIL);


    NodeReceiverGroup rg(DBDIH, all);
    signal->theData[0] = 7219;
    sendSignal(rg, GSN_NDB_TAMPER, signal,  1, JBA);
  }
#endif
}//Dbdih::setNodeRestartInfoBits()

/*************************************************************************/
/*       START THE GLOBAL CHECKPOINT PROTOCOL IN MASTER AT START-UP      */
/*************************************************************************/
void Dbdih::startGcp(Signal* signal) 
{
  signal->theData[0] = DihContinueB::ZSTART_GCP;
  sendSignal(reference(), GSN_CONTINUEB, signal, 1, JBB);

  startGcpMonitor(signal);
}//Dbdih::startGcp()

void 
Dbdih::startGcpMonitor(Signal* signal)
{
  jam();
  m_gcp_monitor.m_gcp_save.m_gci = m_gcp_save.m_gci;
  m_gcp_monitor.m_gcp_save.m_elapsed_ms = 0;
  m_gcp_monitor.m_micro_gcp.m_gci = m_micro_gcp.m_current_gci;
  m_gcp_monitor.m_micro_gcp.m_elapsed_ms = 0;
  m_gcp_monitor.m_last_check = NdbTick_getCurrentTicks();

  signal->theData[0] = DihContinueB::ZCHECK_GCP_STOP;
  sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 1);
}

/**
 * This changes the table distribution and this can be seen by
 * DIGETNODES, so if this is called when we are not in recovery
 * we need to hold the table RCU lock.
 */
void Dbdih::updateNodeInfo(FragmentstorePtr fragPtr)
{
  ReplicaRecordPtr replicatePtr;
  Uint32 index = 0;
  replicatePtr.i = fragPtr.p->storedReplicas;
  do {
    jam();
    c_replicaRecordPool.getPtr(replicatePtr);
    ndbrequire(index < MAX_REPLICAS);
    fragPtr.p->activeNodes[index] = replicatePtr.p->procNode;
    index++;
    replicatePtr.i = replicatePtr.p->nextPool;
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
  writePageWord(wf, fragPtr.p->m_log_part_id);
}//Dbdih::writeFragment()

void Dbdih::writePageWord(RWFragment* wf, Uint32 dataWord)
{
  if (wf->wordIndex >= 2048) {
    jam();
    ndbrequire(wf->wordIndex == 2048);
    allocpage(wf->rwfPageptr);
    wf->wordIndex = 32;
    wf->pageIndex++;
    ndbrequire(wf->pageIndex < NDB_ARRAY_SIZE(wf->rwfTabPtr.p->pageRef));
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
    c_replicaRecordPool.getPtr(wfReplicaPtr);
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
    for (i = 0; i < MAX_CRASHED_REPLICAS; i++) {
      writePageWord(wf, wfReplicaPtr.p->createGci[i]);
      writePageWord(wf, wfReplicaPtr.p->replicaLastGci[i]);
    }//if

    wfReplicaPtr.i = wfReplicaPtr.p->nextPool;
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

  if (ERROR_INSERTED(7224) && filePtr.i == crestartInfoFile[1])
  {
    jam();
    SET_ERROR_INSERT_VALUE(7225);
    sendSignalWithDelay(NDBFS_REF, GSN_FSWRITEREQ, signal, 2000, 8);

    signal->theData[0] = 9999;
    sendSignal(numberToRef(CMVMI, refToNode(cmasterdihref)),
	       GSN_NDB_TAMPER, signal, 1, JBB);
    g_eventLogger->info("FS_WRITEREQ delay 2 second for COPY_GCIREQ");
    return;
  }
  sendSignal(NDBFS_REF, GSN_FSWRITEREQ, signal, 8, JBA);
}//Dbdih::writeRestorableGci()

void Dbdih::writeTabfile(Signal* signal, TabRecord* tab, FileRecordPtr filePtr) 
{
  signal->theData[0] = filePtr.p->fileRef;
  signal->theData[1] = reference();
  signal->theData[2] = filePtr.i;
  signal->theData[3] = ZLIST_OF_PAIRS_SYNCH;
  signal->theData[4] = ZVAR_NO_WORD;
  signal->theData[5] = tab->noPages;

  NDB_STATIC_ASSERT(NDB_ARRAY_SIZE(tab->pageRef) <= NDB_FS_RW_PAGES);
  Uint32 section[2 * NDB_ARRAY_SIZE(tab->pageRef)];
  for (Uint32 i = 0; i < tab->noPages; i++)
  {
    section[(2 * i) + 0] = tab->pageRef[i];
    section[(2 * i) + 1] = i;
  }
  LinearSectionPtr ptr[3];
  ptr[0].p = section;
  ptr[0].sz = 2 * tab->noPages;
  sendSignal(NDBFS_REF, GSN_FSWRITEREQ, signal, 6, JBA, ptr, 1);
}//Dbdih::writeTabfile()

void Dbdih::execDEBUG_SIG(Signal* signal) 
{
  (void)signal; //Avoid compiler warnings
}//Dbdih::execDEBUG_SIG()

void
Dbdih::execDUMP_STATE_ORD(Signal* signal)
{
  DumpStateOrd * const & dumpState = (DumpStateOrd *)&signal->theData[0];
  Uint32 arg = dumpState->args[0];

  if (arg == DumpStateOrd::DihFragmentsPerNode)
  {
    infoEvent("Fragments per node = %u", getFragmentsPerNode());
  }
  if (arg == DumpStateOrd::DihDumpNodeRestartInfo) {
    infoEvent("c_nodeStartMaster.blockGcp = %d, c_nodeStartMaster.wait = %d",
	      c_nodeStartMaster.blockGcp, c_nodeStartMaster.wait);
    for (Uint32 i = 0; i < c_diverify_queue_cnt; i++)
    {
      infoEvent("[ %u : cfirstVerifyQueue = %u clastVerifyQueue = %u sz: %u]",
                i,
                c_diverify_queue[i].cfirstVerifyQueue,
                c_diverify_queue[i].clastVerifyQueue,
                capiConnectFileSize);
    }
    infoEvent("cgcpOrderBlocked = %d",
              cgcpOrderBlocked);
  }//if
  if (arg == DumpStateOrd::DihDumpNodeStatusInfo) {
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
  
  if (arg == DumpStateOrd::DihPrintFragmentation)
  {
    Uint32 tableid = 0;
    Uint32 fragid = 0;
    if (signal->getLength() == 1)
    {
      infoEvent("Printing nodegroups --");
      for (Uint32 i = 0; i<cnoOfNodeGroups; i++)
      {
        jam();
        NodeGroupRecordPtr NGPtr;
        NGPtr.i = c_node_groups[i];
        ptrCheckGuard(NGPtr, MAX_NDB_NODE_GROUPS, nodeGroupRecord);

        infoEvent("NG %u(%u) ref: %u [ cnt: %u : %u %u %u %u ]",
                  NGPtr.i, NGPtr.p->nodegroupIndex, NGPtr.p->m_ref_count,
                  NGPtr.p->nodeCount,
                  NGPtr.p->nodesInGroup[0], NGPtr.p->nodesInGroup[1], 
                  NGPtr.p->nodesInGroup[2], NGPtr.p->nodesInGroup[3]);
      }
      infoEvent("Printing fragmentation of all tables --");
    }
    else if (signal->getLength() == 3)
    {
      jam();
      tableid = dumpState->args[1];
      fragid = dumpState->args[2];
    }
    else
    {
      return;
    }

    if (tableid >= ctabFileSize)
    {
      return;
    }

    TabRecordPtr tabPtr;
    tabPtr.i = tableid;
    ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);

    if (tabPtr.p->tabStatus == TabRecord::TS_ACTIVE &&
        fragid < tabPtr.p->totalfragments)
    {
      dumpState->args[0] = DumpStateOrd::DihPrintOneFragmentation;
      dumpState->args[1] = tableid;
      dumpState->args[2] = fragid;
      execDUMP_STATE_ORD(signal);
    }

    if (tabPtr.p->tabStatus != TabRecord::TS_ACTIVE ||
        ++fragid >= tabPtr.p->totalfragments)
    {
        tableid++;
        fragid = 0;
    }

    if (tableid < ctabFileSize)
    {
      dumpState->args[0] = DumpStateOrd::DihPrintFragmentation;
      dumpState->args[1] = tableid;
      dumpState->args[2] = fragid;
      sendSignal(reference(), GSN_DUMP_STATE_ORD, signal, 3, JBB);
    }
  }

  if (arg == DumpStateOrd::DihPrintOneFragmentation)
  {
    Uint32 tableid = RNIL;
    Uint32 fragid = RNIL;

    if (signal->getLength() == 3)
    {
      jam();
      tableid = dumpState->args[1];
      fragid = dumpState->args[2];
    }
    else
    {
      return;
    }

    if (tableid >= ctabFileSize)
    {
      return;
    }

    TabRecordPtr tabPtr;
    tabPtr.i = tableid;
    ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);

    if (fragid >= tabPtr.p->totalfragments)
    {
      return;
    }

    FragmentstorePtr fragPtr;
    getFragstore(tabPtr.p, fragid, fragPtr);

    Uint32 nodeOrder[MAX_REPLICAS];
    const Uint32 noOfReplicas = extractNodeInfo(jamBuffer(),
                                                fragPtr.p,
                                                nodeOrder);
    char buf[100];
    BaseString::snprintf(buf, sizeof(buf), 
                         " Table %d Fragment %d(%u) LP: %u - ",
                         tabPtr.i, fragid, dihGetInstanceKey(fragPtr),
                         fragPtr.p->m_log_part_id);

    for(Uint32 k = 0; k < noOfReplicas; k++)
    {
      char tmp[100];
      BaseString::snprintf(tmp, sizeof(tmp), "%d ", nodeOrder[k]);
      strcat(buf, tmp);
    }
    infoEvent("%s", buf);
  }
  
  if (signal->theData[0] == 7000) {
    infoEvent("ctimer = %d",
              c_lcpState.ctimer);
    infoEvent("cmasterState = %d", cmasterState);
    infoEvent("cmasterTakeOverNode = %d, ctcCounter = %d",
              cmasterTakeOverNode, c_lcpState.ctcCounter);
  }//if  
  if (signal->theData[0] == 7001) {
    infoEvent("c_lcpState.keepGci = %d",
              c_lcpState.keepGci);
    infoEvent("c_lcpState.lcpStatus = %d, clcpStopGcp = %d",
              c_lcpState.lcpStatus, 
	      c_lcpState.lcpStopGcp);
    infoEvent("cimmediateLcpStart = %d",
              c_lcpState.immediateLcpStart);
  }//if  
  if (signal->theData[0] == 7002) {
    infoEvent("cnoOfActiveTables = %d",
              cnoOfActiveTables);
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
    infoEvent("cstarttype = %d, csystemnodes = %d",
              cstarttype, csystemnodes);
  }//if  
  if (signal->theData[0] == 7004) {
    infoEvent("cmasterdihref = %d, cownNodeId = %d",
              cmasterdihref, cownNodeId);
    infoEvent("cndbStartReqBlockref = %d, cremainingfrags = %d",
              cndbStartReqBlockref, cremainingfrags);
  }//if  
  if (signal->theData[0] == 7005) {
    infoEvent("crestartGci = %d",
              crestartGci);
  }//if  
  if (signal->theData[0] == 7006) {
    infoEvent("clcpDelay = %d",
              c_lcpState.clcpDelay);
    infoEvent("cmasterNodeId = %d", cmasterNodeId);
    infoEvent("c_nodeStartMaster.startNode = %d, c_nodeStartMaster.wait = %d",
              c_nodeStartMaster.startNode, c_nodeStartMaster.wait);
  }//if  
  if (signal->theData[0] == 7007) {
    infoEvent("c_nodeStartMaster.failNr = %d", c_nodeStartMaster.failNr);
    infoEvent("c_nodeStartMaster.startInfoErrorCode = %d",
              c_nodeStartMaster.startInfoErrorCode);
    infoEvent("c_nodeStartMaster.blockGcp = %d",
              c_nodeStartMaster.blockGcp);
  }//if  
  if (signal->theData[0] == 7008) {
    infoEvent("cfirstDeadNode = %d, cstartPhase = %d, cnoReplicas = %d",
              cfirstDeadNode, cstartPhase, cnoReplicas);
    infoEvent("cwaitLcpSr = %d",cwaitLcpSr);
  }//if  
  if (signal->theData[0] == 7009) {
    infoEvent("ccalcOldestRestorableGci = %d, cnoOfNodeGroups = %d",
              c_lcpState.oldestRestorableGci, cnoOfNodeGroups);
    infoEvent("crestartGci = %d",
              crestartGci);
  }//if  
  if (signal->theData[0] == 7010) {
    infoEvent("c_lcpState.lcpStatusUpdatedPlace = %d, cLcpStart = %d",
              c_lcpState.lcpStatusUpdatedPlace, c_lcpState.lcpStart);
    infoEvent("c_blockCommit = %d, c_blockCommitNo = %d",
              c_blockCommit, c_blockCommitNo);
  }//if  
  if (signal->theData[0] == 7011){
    infoEvent("c_COPY_GCIREQ_Counter = %s", 
	      c_COPY_GCIREQ_Counter.getText());
    infoEvent("c_COPY_TABREQ_Counter = %s", 
	      c_COPY_TABREQ_Counter.getText());
    infoEvent("c_UPDATE_FRAG_STATEREQ_Counter = %s", 
	      c_UPDATE_FRAG_STATEREQ_Counter.getText());
    infoEvent("c_DIH_SWITCH_REPLICA_REQ_Counter = %s", 
	      c_DIH_SWITCH_REPLICA_REQ_Counter.getText());
    infoEvent("c_EMPTY_LCP_REQ_Counter = %s",c_EMPTY_LCP_REQ_Counter.getText());
    infoEvent("c_GCP_COMMIT_Counter = %s", c_GCP_COMMIT_Counter.getText());
    infoEvent("c_GCP_PREPARE_Counter = %s", c_GCP_PREPARE_Counter.getText());
    infoEvent("c_GCP_SAVEREQ_Counter = %s", c_GCP_SAVEREQ_Counter.getText());
    infoEvent("c_SUB_GCP_COMPLETE_REP_Counter = %s",
              c_SUB_GCP_COMPLETE_REP_Counter.getText());
    infoEvent("c_INCL_NODEREQ_Counter = %s", c_INCL_NODEREQ_Counter.getText());
    infoEvent("c_MASTER_GCPREQ_Counter = %s", 
	      c_MASTER_GCPREQ_Counter.getText());
    infoEvent("c_MASTER_LCPREQ_Counter = %s", 
	      c_MASTER_LCPREQ_Counter.getText());
    infoEvent("c_START_INFOREQ_Counter = %s", 
	      c_START_INFOREQ_Counter.getText());
    infoEvent("c_START_RECREQ_Counter = %s", c_START_RECREQ_Counter.getText());
    infoEvent("c_STOP_ME_REQ_Counter = %s", c_STOP_ME_REQ_Counter.getText());
    infoEvent("c_TC_CLOPSIZEREQ_Counter = %s", 
	      c_TC_CLOPSIZEREQ_Counter.getText());
    infoEvent("c_TCGETOPSIZEREQ_Counter = %s", 
	      c_TCGETOPSIZEREQ_Counter.getText());
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

  if(arg == DumpStateOrd::DihTcSumaNodeFailCompleted &&
     signal->getLength() == 2 &&
     signal->theData[1] < MAX_NDB_NODES)
  {
    jam();
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
  
  if(arg == 7020 && signal->getLength() > 3)
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
      BaseString::snprintf(buf, 100, "%s %.8x", buf2, signal->theData[i]);
      BaseString::snprintf(buf2, 100, "%s", buf);
    }
    warningEvent("gsn: %d block: %s, length: %d theData: %s", 
		 gsn, getBlockName(block, "UNKNOWN"), length, buf);

    g_eventLogger->warning("-- SENDING CUSTOM SIGNAL --");
    g_eventLogger->warning("gsn: %d block: %s, length: %d theData: %s", 
                           gsn, getBlockName(block, "UNKNOWN"), length, buf);
  }
  
  if(arg == DumpStateOrd::DihDumpLCPState){
    infoEvent("-- Node %d LCP STATE --", getOwnNodeId());
    infoEvent("lcpStatus = %d (update place = %d) ",
	      c_lcpState.lcpStatus, c_lcpState.lcpStatusUpdatedPlace);
    infoEvent
      ("lcpStart = %d lcpStopGcp = %d keepGci = %d oldestRestorable = %d",
       c_lcpState.lcpStart, c_lcpState.lcpStopGcp, 
       c_lcpState.keepGci, c_lcpState.oldestRestorableGci);
    
    infoEvent
      ("immediateLcpStart = %d masterLcpNodeId = %d",
       c_lcpState.immediateLcpStart,
       refToNode(c_lcpState.m_masterLcpDihRef));

    for (Uint32 i = 0; i<10; i++)
    {
      infoEvent("%u : status: %u place: %u", i, 
                c_lcpState.m_saveState[i].m_status,
                c_lcpState.m_saveState[i].m_place);
    }
    
    infoEvent("-- Node %d LCP STATE --", getOwnNodeId());
  }

  if(arg == DumpStateOrd::DihDumpLCPMasterTakeOver){
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

  if (signal->theData[0] == 7015)
  {
    if (signal->getLength() == 1)
    {
      signal->theData[1] = 0;
    }

    Uint32 tableId = signal->theData[1];
    if (tableId < ctabFileSize)
    {
      signal->theData[0] = 7021;
      execDUMP_STATE_ORD(signal);
      signal->theData[0] = 7015;
      signal->theData[1] = tableId + 1;
      sendSignal(reference(), GSN_DUMP_STATE_ORD, signal, 2, JBB);
    }
  }

  if(arg == DumpStateOrd::EnableUndoDelayDataWrite){
    g_eventLogger->info("Dbdih:: delay write of datapages for table = %d", 
                        dumpState->args[1]);
    // Send this dump to ACC and TUP
    sendSignal(DBACC_REF, GSN_DUMP_STATE_ORD, signal, 2, JBB);
    sendSignal(DBTUP_REF, GSN_DUMP_STATE_ORD, signal, 2, JBB);
    
    // Start immediate LCP
    add_lcp_counter(&c_lcpState.ctimer, (1 << 31));
    if (cmasterNodeId == getOwnNodeId())
    {
      jam();
      c_lcpState.immediateLcpStart = true;
    }
    return;
  }

  if (signal->theData[0] == DumpStateOrd::DihAllAllowNodeStart) {
    for (Uint32 i = 1; i < MAX_NDB_NODES; i++)
      setAllowNodeStart(i, true);
    return;
  }//if
  if (signal->theData[0] == DumpStateOrd::DihMinTimeBetweenLCP) {
    // Set time between LCP to min value
    if (signal->getLength() == 2)
    {
      Uint32 tmp;
      const ndb_mgm_configuration_iterator * p = 
	m_ctx.m_config.getOwnConfigIterator();
      ndbrequire(p != 0);
      ndb_mgm_get_int_parameter(p, CFG_DB_LCP_INTERVAL, &tmp);
      g_eventLogger->info("Reset time between LCP to %u", tmp);
      c_lcpState.clcpDelay = tmp;
    }
    else
    {
      g_eventLogger->info("Set time between LCP to min value");
      c_lcpState.clcpDelay = 0; // TimeBetweenLocalCheckpoints.min
    }
    return;
  }
  if (signal->theData[0] == DumpStateOrd::DihMaxTimeBetweenLCP) {
    // Set time between LCP to max value
    g_eventLogger->info("Set time between LCP to max value");
    c_lcpState.clcpDelay = 31; // TimeBetweenLocalCheckpoints.max
    return;
  }
  
  if(arg == 7098){
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

  if (arg == DumpStateOrd::DihStartLcpImmediately)
  {
    jam();
    if (cmasterNodeId == getOwnNodeId())
    {
      jam();
      c_lcpState.immediateLcpStart = true;
      return;
    }

    add_lcp_counter(&c_lcpState.ctimer, (1 << 31));
    /**
     * If sent from local LQH, forward to master
     */
    if (refToMain(signal->getSendersBlockRef()) == DBLQH)
    {
      jam();
      sendSignal(cmasterdihref, GSN_DUMP_STATE_ORD, signal, 1, JBB);
    }
    return;
  }

  if (arg == DumpStateOrd::DihSetTimeBetweenGcp)
  {
    Uint32 tmp = 0;
    if (signal->getLength() == 1)
    {
      const ndb_mgm_configuration_iterator * p = 
	m_ctx.m_config.getOwnConfigIterator();
      ndbrequire(p != 0);
      ndb_mgm_get_int_parameter(p, CFG_DB_GCP_INTERVAL, &tmp);
    }
    else
    {
      tmp = signal->theData[1];
    }
    m_gcp_save.m_master.m_time_between_gcp = tmp;
    g_eventLogger->info("Setting time between gcp : %d", tmp);
  }

  if (arg == 7021 && signal->getLength() == 2)
  {
    TabRecordPtr tabPtr;
    tabPtr.i = signal->theData[1];
    if (tabPtr.i >= ctabFileSize)
      return;

    ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);
    
    if(tabPtr.p->tabStatus != TabRecord::TS_ACTIVE)
      return;
    
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
        c_replicaRecordPool.getPtr(replicaPtr);
	BaseString::snprintf(buf2, sizeof(buf2), "%s %d(on %d)=%d(%s)",
			     buf, num, 
			     replicaPtr.p->procNode, 
			     replicaPtr.p->lcpIdStarted,
			     replicaPtr.p->lcpOngoingFlag ? "Ongoing" : "Idle");
	BaseString::snprintf(buf, sizeof(buf), "%s", buf2);
	
	num++;
	replicaPtr.i = replicaPtr.p->nextPool;
      } while (replicaPtr.i != RNIL);
      infoEvent("%s", buf);
    }
  }

  if (arg == 7022)
  {
    jam();
    crashSystemAtGcpStop(signal, true);
  }

  if (arg == 7025)
  {
    jam();
    dumpGcpStop();
    return;
  }

#ifdef GCP_TIMER_HACK
  if (signal->theData[0] == 7901)
    globalData.gcp_timer_limit = signal->theData[1];
#endif
  if (arg == 7023)
  {
    /**
     * Dump all active TakeOver
     */
    Ptr<TakeOverRecord> ptr;
    ptr.i = signal->theData[1];
    if (signal->getLength() == 1)
    {
      infoEvent("Starting dump all active take-over");
      c_masterActiveTakeOverList.first(ptr);
    }

    if (ptr.i == RNIL)
    {
      infoEvent("Dump all active take-over done");
      return;
    }

    c_masterActiveTakeOverList.getPtr(ptr);
    infoEvent("TakeOverPtr(%u) starting: %u flags: 0x%x ref: 0x%x, data: %u",
              ptr.i,
              ptr.p->toStartingNode,
              ptr.p->m_flags,
              ptr.p->m_senderRef,
              ptr.p->m_senderData);
    infoEvent("slaveState: %u masterState: %u",
              ptr.p->toSlaveStatus, ptr.p->toMasterStatus);
    infoEvent("restorableGci: %u startGci: %u tab: %u frag: %u src: %u max: %u",
              ptr.p->restorableGci, ptr.p->startGci, 
              ptr.p->toCurrentTabref, ptr.p->toCurrentFragid,
              ptr.p->toCopyNode, ptr.p->maxPage);
    
    c_masterActiveTakeOverList.next(ptr);
    signal->theData[0] = arg;
    signal->theData[1] = ptr.i;
  }

  if (arg == DumpStateOrd::DihDumpPageRecInfo)
  {
    jam();
    ndbout_c("MAX_CONCURRENT_LCP_TAB_DEF_FLUSHES %u", MAX_CONCURRENT_LCP_TAB_DEF_FLUSHES);
    ndbout_c("MAX_CONCURRENT_DIH_TAB_DEF_OPS %u", MAX_CONCURRENT_DIH_TAB_DEF_OPS);
    ndbout_c("MAX_CRASHED_REPLICAS %u", MAX_CRASHED_REPLICAS);
    ndbout_c("MAX_LCP_STORED %u", MAX_LCP_STORED);
    ndbout_c("MAX_REPLICAS %u", MAX_REPLICAS);
    ndbout_c("MAX_NDB_PARTITIONS %u", MAX_NDB_PARTITIONS);
    ndbout_c("PACK_REPLICAS_WORDS %u", PACK_REPLICAS_WORDS);
    ndbout_c("PACK_FRAGMENT_WORDS %u", PACK_FRAGMENT_WORDS);
    ndbout_c("PACK_TABLE_WORDS %u", PACK_TABLE_WORDS);
    ndbout_c("PACK_TABLE_PAGE_WORDS %u", PACK_TABLE_PAGE_WORDS);
    ndbout_c("PACK_TABLE_PAGES %u", PACK_TABLE_PAGES);
    ndbout_c("ZPAGEREC %u", ZPAGEREC);
    ndbout_c("Total bytes : %lu",
             (unsigned long) ZPAGEREC * sizeof(PageRecord));
    ndbout_c("LCP Tab def write ops inUse %u queued %u",
             c_lcpTabDefWritesControl.inUse,
             c_lcpTabDefWritesControl.queuedRequests);

    if (getNodeState().startLevel < NodeState::SL_STARTING)
      return ;

    Uint32 freeCount = 0;
    PageRecordPtr tmp;
    tmp.i = cfirstfreepage;
    while (tmp.i != RNIL)
    {
      jam();
      ptrCheckGuard(tmp, cpageFileSize, pageRecord);
      freeCount++;
      tmp.i = tmp.p->nextfreepage;
    };
    ndbout_c("Pages in use %u/%u", cpageFileSize - freeCount, cpageFileSize);
    return;
  }

  if (arg == DumpStateOrd::SchemaResourceSnapshot)
  {
    RSS_OP_SNAPSHOT_SAVE(cremainingfrags);
    RSS_OP_SNAPSHOT_SAVE(cnoFreeReplicaRec);

    {
      Uint32 cnghash = 0;
      NodeGroupRecordPtr NGPtr;
      for (Uint32 i = 0; i<cnoOfNodeGroups; i++)
      {
        NGPtr.i = c_node_groups[i];
        ptrCheckGuard(NGPtr, MAX_NDB_NODE_GROUPS, nodeGroupRecord);
        cnghash = (cnghash * 33) + NGPtr.p->m_ref_count;
      }
      RSS_OP_SNAPSHOT_SAVE(cnghash);
    }
    return;
  }

  if (arg == DumpStateOrd::SchemaResourceCheckLeak)
  {
    RSS_OP_SNAPSHOT_CHECK(cremainingfrags);
    RSS_OP_SNAPSHOT_SAVE(cnoFreeReplicaRec);

    {
      Uint32 cnghash = 0;
      NodeGroupRecordPtr NGPtr;
      for (Uint32 i = 0; i<cnoOfNodeGroups; i++)
      {
        NGPtr.i = c_node_groups[i];
        ptrCheckGuard(NGPtr, MAX_NDB_NODE_GROUPS, nodeGroupRecord);
        cnghash = (cnghash * 33) + NGPtr.p->m_ref_count;
      }
      RSS_OP_SNAPSHOT_CHECK(cnghash);
    }
  }

  /* Checks whether add frag failure was cleaned up.
   * Should NOT be used while commands involving addFragReq
   * are being performed.
   */
  if (arg == DumpStateOrd::DihAddFragFailCleanedUp && signal->length() == 2)
  {
    jam();
    TabRecordPtr tabPtr;
    tabPtr.i = signal->theData[1];
    if (tabPtr.i >= ctabFileSize)
      return;

    ptrCheckGuard(tabPtr, ctabFileSize, tabRecord);

    if (tabPtr.p->m_new_map_ptr_i == RNIL)
    {
      jam();
      infoEvent("DIH : Add frag fail clean up ok for table %u", tabPtr.i);
    }
    else
    {
      jam();
      warningEvent("new_map_ptr_i to table id %d is not NIL", tabPtr.i);
      /*
        This ndbrequire is needed by the runFailAddPartition() test case.
        This dump code is *not* intended for interactive usage, as the node
        is likely to crash.
      */
      ndbrequire(false);
    }
  }
  if (arg == DumpStateOrd::DihDisplayPauseState)
  {
    infoEvent("Pause LCP ref: %x, is_lcp_paused %u,"
              " c_dequeue_lcp_rep_ongoing %u",
              cmasterdihref,
              is_lcp_paused(),
              c_dequeue_lcp_rep_ongoing);
    infoEvent("c_pause_lcp_master_state: %u,"
              " c_old_node_waiting_for_lcp_end: %u",
              Uint32(c_pause_lcp_master_state),
              c_old_node_waiting_for_lcp_end);
    infoEvent("c_queued_lcp_complete_rep: %u,"
              " c_lcp_id_paused: %u",
              c_queued_lcp_complete_rep,
              c_lcp_id_paused);
    infoEvent("c_last_id_lcp_complete_rep: %u"
              " c_lcp_runs_with_pause_support: %u",
              c_last_id_lcp_complete_rep,
              c_lcp_runs_with_pause_support);
    infoEvent("c_lcp_id_while_copy_meta_data: %u, c_pause_lcp_start_node: %u",
              c_lcp_id_while_copy_meta_data,
              c_pause_lcp_start_node);
    infoEvent("c_PAUSE_LCP_REQ_Counter: %s",
              c_PAUSE_LCP_REQ_Counter.getText());
    infoEvent("c_FLUSH_LCP_REP_REQ_Counter: %s",
              c_FLUSH_LCP_REP_REQ_Counter.getText());
    if (isMaster())
    {
      char buf[100];
      infoEvent("c_lcpState.m_participatingLQH: %s",
                c_lcpState.m_participatingLQH.getText(buf));
      infoEvent("c_pause_participants: %s",
                c_pause_participants.getText(buf));
    }
  }

  DECLARE_DUMP0(DBDIH, 7213, "Set error 7213 with extra arg")
  {
    SET_ERROR_INSERT_VALUE2(7213, signal->theData[1]);
    return;
  }
  DECLARE_DUMP0(DBDIH, 7214, "Set error 7214 with extra arg")
  {
    SET_ERROR_INSERT_VALUE2(7214, signal->theData[1]);
    return;
  }

  DECLARE_DUMP0(DBDIH, 7216, "Set error 7216 with extra arg")
  {
    SET_ERROR_INSERT_VALUE2(7216, signal->theData[1]);
    return;
  }
  DECLARE_DUMP0(DBDIH, 6099, "Start microgcp")
  {
    if (isMaster())
    {
      jam();
      // Invalidating timestamp will force an immediate microGCP
      NdbTick_Invalidate(&m_micro_gcp.m_master.m_start_time);
    }
    else
    {
      jam();
      sendSignal(cmasterdihref, GSN_DUMP_STATE_ORD, signal, 1, JBB);
    }
    return;
  }
  DECLARE_DUMP0(DBDIH, 7999, "Set error code with extra arg")
  {
    SET_ERROR_INSERT_VALUE2(signal->theData[1],
                            signal->theData[2]);
  }

  if (arg == DumpStateOrd::DihSetGcpStopVals)
  {
    jam();
    if (signal->getLength() != 3)
    {
      jam();
      return;
    }
    if (signal->theData[1] == 0)
    {
      g_eventLogger->info("Changing GCP_COMMIT max_lag_millis from %u to %u",
                          m_gcp_monitor.m_micro_gcp.m_max_lag_ms,
                          signal->theData[2]);
      m_gcp_monitor.m_micro_gcp.m_max_lag_ms = signal->theData[2];
    }
    else
    {
      g_eventLogger->info("Changing GCP_SAVE max_lag_millis from %u to %u",
                          m_gcp_monitor.m_gcp_save.m_max_lag_ms,
                          signal->theData[2]);
      m_gcp_monitor.m_gcp_save.m_max_lag_ms = signal->theData[2];
    }
  }

  if (arg == DumpStateOrd::DihStallLcpStart)
  {
    jam();

    if (signal->getLength() != 2)
    {
      g_eventLogger->warning("Malformed DihStallLcpStart(%u) received, ignoring",
                             DumpStateOrd::DihStallLcpStart);
      return;
    }
    const Uint32 key = signal->theData[1];
    if (key == 91919191)
    {
      jam();
      g_eventLogger->warning("DihStallLcpStart(%u) received, stalling subsequent LCP starts",
                             DumpStateOrd::DihStallLcpStart);
      c_lcpState.lcpManualStallStart = true;
    }
    else
    {
      jam();
      g_eventLogger->warning("DihStallLcpStart(%u) received, clearing LCP stall state (%u)",
                             DumpStateOrd::DihStallLcpStart,
                             c_lcpState.lcpManualStallStart);
      c_lcpState.lcpManualStallStart = false;
    }
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

  if(err != PrepDropTabRef::OK)
  {
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

  /**
   * When we come here DBTC is already aware of the table being dropped,
   * so no requests for the table will arrive after this from DBTC, so
   * no need to protect this variable here, it is protected by the
   * signalling order of drop table signals instead.
   */
  tabPtr.p->tabStatus = TabRecord::TS_DROPPING;

  PrepDropTabConf* conf = (PrepDropTabConf*)signal->getDataPtrSend();
  conf->tableId = tabPtr.i;
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(senderRef, GSN_PREP_DROP_TAB_CONF,
             signal, PrepDropTabConf::SignalLength, JBB);
}

void
Dbdih::waitDropTabWritingToFile(Signal* signal, TabRecordPtr tabPtr){
  
  if (tabPtr.p->tabLcpStatus == TabRecord::TLS_WRITING_TO_FILE)
  {
    jam();
    signal->theData[0] = DihContinueB::WAIT_DROP_TAB_WRITING_TO_FILE;
    signal->theData[1] = tabPtr.i;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal,
                        WaitTableStateChangeMillis, 2);
    return;
  }

  if (tabPtr.p->tabUpdateState != TabRecord::US_IDLE)
  {
    jam();
    signal->theData[0] = DihContinueB::WAIT_DROP_TAB_WRITING_TO_FILE;
    signal->theData[1] = tabPtr.i;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal,
                        WaitTableStateChangeMillis, 2);
    return;
  }

  ndbrequire(tabPtr.p->tabLcpStatus ==  TabRecord::TLS_COMPLETED);
  checkDropTabComplete(signal, tabPtr);
}

void
Dbdih::checkDropTabComplete(Signal* signal, TabRecordPtr tabPtr)
{
  startDeleteFile(signal, tabPtr);
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
  if (signal->getLength() == 1)
  {
    SET_ERROR_INSERT_VALUE2(signal->theData[0],
                            0);
  }
  else
  {
    SET_ERROR_INSERT_VALUE2(signal->theData[0],
                            signal->theData[1]);
  }
  return;
}//Dbdih::execNDB_TAMPER()

void Dbdih::execBLOCK_COMMIT_ORD(Signal* signal){
  BlockCommitOrd* const block = (BlockCommitOrd *)&signal->theData[0];

  jamEntry();

  c_blockCommit = true;
  c_blockCommitNo = block->failNo;
}

void Dbdih::execUNBLOCK_COMMIT_ORD(Signal* signal){
  UnblockCommitOrd* const unblock = (UnblockCommitOrd *)&signal->theData[0];
  (void)unblock;

  jamEntry();
  
  if(c_blockCommit == true)
  {
    jam();
    
    c_blockCommit = false;
    for (Uint32 i = 0; i<c_diverify_queue_cnt; i++)
    {
      c_diverify_queue[i].m_empty_done = 0;
      emptyverificbuffer(signal, i, true);
    }
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
  const Uint32 noOfReplicas = extractNodeInfo(jamBuffer(),
                                              fragPtr.p,
                                              oldOrder);
  
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

  make_table_use_new_node_order(tabPtr,
                                fragPtr,
                                noOfReplicas,
                                &req->newNodeOrder[0]);

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
    const Uint32 noOfReplicas = extractNodeInfo(jamBuffer(),
                                                fragPtr.p,
                                                oldOrder);

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

    sendLoopMacro(DIH_SWITCH_REPLICA_REQ, sendDIH_SWITCH_REPLICA_REQ, RNIL);
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
    make_node_not_usable(nodePtr.p);
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

  sendLoopMacro(STOP_ME_REQ, sendSTOP_ME_REQ, RNIL);

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
  Uint32 errorCode = 0;
  if(ERROR_INSERTED(7247))
  {
    ndbout_c("Delaying WAIT_GCP_REQ");
    sendSignalWithDelay(reference(), GSN_WAIT_GCP_REQ, signal, 1000,
                        signal->getLength());
    return;
  }

  if(requestType == WaitGCPReq::CurrentGCI)
  {
    jam();
    conf->senderData = senderData;
    conf->gci_hi = Uint32(m_micro_gcp.m_current_gci >> 32);
    conf->gci_lo = Uint32(m_micro_gcp.m_current_gci);
    conf->blockStatus = cgcpOrderBlocked;
    sendSignal(senderRef, GSN_WAIT_GCP_CONF, signal, 
	       WaitGCPConf::SignalLength, JBB);
    return;
  }//if

  if(requestType == WaitGCPReq::RestartGCI)
  {
    jam();
    conf->senderData = senderData;
    conf->gci_hi = Uint32(crestartGci);
    conf->gci_lo = 0;
    conf->blockStatus = cgcpOrderBlocked;
    sendSignal(senderRef, GSN_WAIT_GCP_CONF, signal,
	       WaitGCPConf::SignalLength, JBB);
    return;
  }//if

  if (requestType == WaitGCPReq::BlockStartGcp)
  {
    jam();
    conf->senderData = senderData;
    conf->gci_hi = Uint32(m_micro_gcp.m_current_gci >> 32);
    conf->gci_lo = Uint32(m_micro_gcp.m_current_gci);
    conf->blockStatus = cgcpOrderBlocked;
    sendSignal(senderRef, GSN_WAIT_GCP_CONF, signal, 
	       WaitGCPConf::SignalLength, JBB);
    cgcpOrderBlocked = 1;
    return;
  }

  if (requestType == WaitGCPReq::UnblockStartGcp)
  {
    jam();
    conf->senderData = senderData;
    conf->gci_hi = Uint32(m_micro_gcp.m_current_gci >> 32);
    conf->gci_lo = Uint32(m_micro_gcp.m_current_gci);
    conf->blockStatus = cgcpOrderBlocked;
    sendSignal(senderRef, GSN_WAIT_GCP_CONF, signal, 
	       WaitGCPConf::SignalLength, JBB);
    cgcpOrderBlocked = 0;
    return;
  }

  ndbassert(requestType == WaitGCPReq::Complete ||
            requestType == WaitGCPReq::CompleteForceStart ||
            requestType == WaitGCPReq::CompleteIfRunning ||
            requestType == WaitGCPReq::WaitEpoch);
  
  /**
   * At this point, we wish to wait for some GCP/Epoch related
   * event
   *
   * Complete           : Wait for the next GCI completion,
   *                      and return its identity
   * CompleteForceStart : Same as complete, but force a GCI to
   *                      start ASAP
   * CompleteIfRunning  : Wait for any running GCI to complete
   *                      Return latest completed GCI
   * WaitEpoch          : Wait for the next epoch completion,
   *                      and return its identity
   *
   * Notes
   *   For GCIs, the 'next' GCI is generally next GCI to *start* 
   *   after the WAIT_GCP_REQ is received.
   *   This is generally used to ensure that changes prior to 
   *   WAIT_GCP_REQ are included in the GCI, which requires
   *   that any currently open epoch be included in the GCI
   *   waited for.
   *   Special care is required during epoch transitions.
   *
   *   Note that epochs are started and completed by the
   *   GCP_PREPARE/GCP_COMMIT protocols, but GCIs are completed
   *   by the GCP_SAVEREQ et al protocols.
   *   GCI completion is triggered as part of GCP_COMMIT processing,
   *   but does not stall further GCP_PREPARE/COMMIT rounds.
   *
   *   CompleteIfRunning waits for any running GCP_SAVEREQ,
   *   it is not currently checking GCP_PREPARE/COMMIT status
   *
   */
  if(isMaster())
  {
    /**
     * Master
     */

    if (!isActiveMaster())
    {
      ndbassert(cmasterState == MASTER_TAKE_OVER_GCP);
      errorCode = WaitGCPRef::NF_MasterTakeOverInProgress;
      goto error;
    }

    /**
     * Beware here : 
     *   - GCP_SAVE and GCP_PREPARE/COMMIT can run
     *     concurrently
     *   - GCP_SAVE can be running concurrently for
     *     quite an 'old' epoch
     *   - Care must be taken in each use case to 
     *     understand the significance of the 
     *     current state ('now')  when WAIT_GCP_REQ 
     *     reaches the Master
     */
    if((requestType == WaitGCPReq::CompleteIfRunning) &&
       (m_gcp_save.m_master.m_state == GcpSave::GCP_SAVE_IDLE))
    {
      jam();
      /* No GCP_SAVE running, return last durable GCI */
      conf->senderData = senderData;
      conf->gci_hi = Uint32(m_micro_gcp.m_old_gci >> 32);
      conf->gci_lo = Uint32(m_micro_gcp.m_old_gci);
      conf->blockStatus = cgcpOrderBlocked;
      sendSignal(senderRef, GSN_WAIT_GCP_CONF, signal, 
		 WaitGCPConf::SignalLength, JBB);
      return;
    }//if

    WaitGCPMasterPtr ptr;
    WaitGCPList * list = &c_waitGCPMasterList;
    if (requestType == WaitGCPReq::WaitEpoch)
    {
      jam();
      list = &c_waitEpochMasterList;
    }

    if (list->seizeFirst(ptr) == false)
    {
      jam();
      errorCode = WaitGCPRef::NoWaitGCPRecords;
      goto error;
      return;
    }

    ptr.p->clientRef = senderRef;
    ptr.p->clientData = senderData;

    switch (requestType)
    {
    case WaitGCPReq::WaitEpoch:
    {
      /* Wait for the next epoch completion (GCP_PREPARE/COMMIT) */
      ptr.p->waitGCI = 0;
      break;
    }
    case WaitGCPReq::CompleteIfRunning:
    {
      ndbrequire(m_gcp_save.m_master.m_state != GcpSave::GCP_SAVE_IDLE);
      /* Wait for GCI currently being saved to complete */
      ptr.p->waitGCI = m_gcp_save.m_gci;
      break;
    }
    case WaitGCPReq::Complete:
    case WaitGCPReq::CompleteForceStart:
    {
      /**
       * We need to block until the highest known epoch
       * in the cluster at *this* time has been included
       * in a subsequent GCP_SAVE round, then return that 
       * complete, saved GCI to the requestor.
       * If we are not changing epochs then we wait for
       * a GCI containing the current epoch.
       * If we are changing epochs then we wait for a GCI
       * containing the next epoch.
       */
      ptr.p->waitGCI = Uint32(m_micro_gcp.m_current_gci >> 32);
      
      if (m_micro_gcp.m_master.m_state == MicroGcp::M_GCP_COMMIT)
      {
        jam();
        /**
         * DIHs are currently committing the transition to 
         * a new epoch.
         * Some TCs may have started committing transactions
         * in that epoch, so to ensure that all previously
         * committed transactions from the point of view of the
         * sender of this signal are included, we will use
         * the new epoch as the epoch after which to send the
         * CONF.
         */
        ptr.p->waitGCI = Uint32(m_micro_gcp.m_master.m_new_gci >> 32);
      }

      if (requestType == WaitGCPReq::CompleteForceStart)
      {
        jam();
        // Invalidating timestamps will force GCP_PREPARE/COMMIT
        // and GCP_SAVEREQ et al ASAP
        NdbTick_Invalidate(&m_micro_gcp.m_master.m_start_time);
        NdbTick_Invalidate(&m_gcp_save.m_master.m_start_time);
      }//if
      
      break;
    }
    default:
      jamLine(requestType);
      ndbrequire(false);
    }
    
    return;
  }
  else
  {
    /** 
     * Proxy part
     */
    jam();
    WaitGCPProxyPtr ptr;
    if (c_waitGCPProxyList.seizeFirst(ptr) == false)
    {
      jam();
      errorCode = WaitGCPRef::NoWaitGCPRecords;
      goto error;
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

error:
  ref->senderData = senderData;
  ref->errorCode = errorCode;
  sendSignal(senderRef, GSN_WAIT_GCP_REF, signal,
             WaitGCPRef::SignalLength, JBB);
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
  const Uint32 gci_hi = conf->gci_hi;
  const Uint32 gci_lo = conf->gci_lo;
  WaitGCPProxyPtr ptr;

  ptr.i = proxyPtr;
  c_waitGCPProxyList.getPtr(ptr);

  conf->senderData = ptr.p->clientData;
  conf->gci_hi = gci_hi;
  conf->gci_lo = gci_lo;
  conf->blockStatus = cgcpOrderBlocked;
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
      jam();
      c_waitGCPMasterList.release(i);
    }//if
  }//while
}//Dbdih::checkWaitGCPMaster()

void Dbdih::emptyWaitGCPMasterQueue(Signal* signal,
                                    Uint64 gci,
                                    WaitGCPList & list)
{
  jam();
  WaitGCPConf* const conf = (WaitGCPConf*)&signal->theData[0];
  conf->gci_hi = Uint32(gci >> 32);
  conf->gci_lo = Uint32(gci);

  WaitGCPMasterPtr ptr;
  list.first(ptr);
  while(ptr.i != RNIL) {
    jam();
    const Uint32 i = ptr.i;
    const Uint32 clientData = ptr.p->clientData;
    const BlockReference clientRef = ptr.p->clientRef;
    const Uint32 waitGCI = ptr.p->waitGCI;

    c_waitGCPMasterList.next(ptr);
    
    if (waitGCI != 0)
    {
      jam();
      /* Waiting for a specific GCI */
      const Uint64 completedGci = (gci >> 32);
      ndbrequire(completedGci <= waitGCI)
      
      if (completedGci < waitGCI)
      {
        jam();
        /* Keep waiting */
        continue;
      }
    }

    conf->senderData = clientData;
    conf->blockStatus = cgcpOrderBlocked;
    sendSignal(clientRef, GSN_WAIT_GCP_CONF, signal,
               WaitGCPConf::SignalLength, JBB);
    
    list.release(i);
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

bool Dbdih::getAllowNodeStart(Uint32 nodeId)
{
  NodeRecordPtr nodePtr;
  nodePtr.i = nodeId;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
  return nodePtr.p->allowNodeStart;
}//Dbdih::getAllowNodeStart()

Uint32
Dbdih::getNodeGroup(Uint32 nodeId) const
{
  NodeRecordPtr nodePtr;
  nodePtr.i = nodeId;
  ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
  return nodePtr.p->nodeGroup;
}

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

void Dbdih::initNodeRecord(NodeRecordPtr nodePtr)
{
  nodePtr.p->m_nodefailSteps.clear();

  nodePtr.p->activeStatus = Sysfile::NS_NotDefined;
  nodePtr.p->recNODE_FAILREP = ZFALSE;
  nodePtr.p->dbtcFailCompleted = ZTRUE;
  nodePtr.p->dbdictFailCompleted = ZTRUE;
  nodePtr.p->dbdihFailCompleted = ZTRUE;
  nodePtr.p->dblqhFailCompleted = ZTRUE;
  nodePtr.p->noOfStartedChkpt = 0;
  nodePtr.p->noOfQueuedChkpt = 0;
  nodePtr.p->lcpStateAtTakeOver = (MasterLCPConf::State)255;

  nodePtr.p->activeTabptr = RNIL;
  nodePtr.p->nodeStatus = NodeRecord::NOT_IN_CLUSTER;
  nodePtr.p->useInTransactions = false;
  nodePtr.p->copyCompleted = 0;
  nodePtr.p->allowNodeStart = true;
}
// DICT lock slave

void
Dbdih::sendDictLockReq(Signal* signal, Uint32 lockType, Callback c)
{
  DictLockReq* req = (DictLockReq*)&signal->theData[0];
  DictLockSlavePtr lockPtr;

  c_dictLockSlavePool.seize(lockPtr);
  ndbrequire(lockPtr.i != RNIL);

  req->userPtr = lockPtr.i;
  req->lockType = lockType;
  req->userRef = reference();

  lockPtr.p->lockPtr = RNIL;
  lockPtr.p->lockType = lockType;
  lockPtr.p->locked = false;
  lockPtr.p->callback = c;

  // handle rolling upgrade
  {
    Uint32 masterVersion = getNodeInfo(cmasterNodeId).m_version;

    const unsigned int get_major = getMajor(masterVersion);
    const unsigned int get_minor = getMinor(masterVersion);
    const unsigned int get_build = getBuild(masterVersion);
    ndbrequire(get_major >= 4);
    
    if (masterVersion < NDBD_DICT_LOCK_VERSION_5 ||
        (masterVersion < NDBD_DICT_LOCK_VERSION_5_1 &&
         get_major == 5 && get_minor == 1) ||
        ERROR_INSERTED(7176)) {
      jam();

      infoEvent("DIH: detect upgrade: master node %u old version %u.%u.%u",
                (unsigned int)cmasterNodeId, get_major, get_minor, get_build);

      DictLockConf* conf = (DictLockConf*)&signal->theData[0];
      conf->userPtr = lockPtr.i;
      conf->lockType = lockType;
      conf->lockPtr = ZNIL;
      
      sendSignal(reference(), GSN_DICT_LOCK_CONF, signal,
                 DictLockConf::SignalLength, JBB);
      return;
    }
  }
  
  BlockReference dictMasterRef = calcDictBlockRef(cmasterNodeId);
  sendSignal(dictMasterRef, GSN_DICT_LOCK_REQ, signal,
      DictLockReq::SignalLength, JBB);
}

void
Dbdih::execDICT_LOCK_CONF(Signal* signal)
{
  jamEntry();
  recvDictLockConf(signal);
}

void
Dbdih::execDICT_LOCK_REF(Signal* signal)
{
  jamEntry();
  ndbrequire(false);
}

void
Dbdih::recvDictLockConf(Signal* signal)
{
  const DictLockConf* conf = (const DictLockConf*)&signal->theData[0];

  DictLockSlavePtr lockPtr;
  c_dictLockSlavePool.getPtr(lockPtr, conf->userPtr);
  
  lockPtr.p->lockPtr = conf->lockPtr;
  ndbrequire(lockPtr.p->lockType == conf->lockType);
  ndbrequire(lockPtr.p->locked == false);
  lockPtr.p->locked = true;

  lockPtr.p->callback.m_callbackData = lockPtr.i;
  execute(signal, lockPtr.p->callback, 0);
}

void
Dbdih::sendDictUnlockOrd(Signal* signal, Uint32 lockSlavePtrI)
{
  DictUnlockOrd* ord = (DictUnlockOrd*)&signal->theData[0];

  DictLockSlavePtr lockPtr;
  c_dictLockSlavePool.getPtr(lockPtr, lockSlavePtrI);

  ord->lockPtr = lockPtr.p->lockPtr;
  ord->lockType = lockPtr.p->lockType;
  ord->senderData = lockPtr.i;
  ord->senderRef = reference();

  c_dictLockSlavePool.release(lockPtr);

  // handle rolling upgrade
  {
    Uint32 masterVersion = getNodeInfo(cmasterNodeId).m_version;

    const unsigned int get_major = getMajor(masterVersion);
    const unsigned int get_minor = getMinor(masterVersion);
    ndbrequire(get_major >= 4);
    
    if (masterVersion < NDBD_DICT_LOCK_VERSION_5 ||
        (masterVersion < NDBD_DICT_LOCK_VERSION_5_1 &&
         get_major == 5 && get_minor == 1) ||
        ERROR_INSERTED(7176)) {
      return;
    }
  }

  Uint32 len = DictUnlockOrd::SignalLength;
  if (unlikely(getNodeInfo(cmasterNodeId).m_version < NDB_MAKE_VERSION(6,3,0)))
  {
    jam();
    len = 2;
  }

  BlockReference dictMasterRef = calcDictBlockRef(cmasterNodeId);
  sendSignal(dictMasterRef, GSN_DICT_UNLOCK_ORD, signal, len, JBB);
}

#ifdef ERROR_INSERT
void
Dbdih::sendToRandomNodes(const char * msg,
                         Signal* signal,
                         SignalCounter* counter,
                         SendFunction fun,
                         Uint32 extra,
                         Uint32 block,
                         Uint32 gsn,
                         Uint32 len,
                         JobBufferLevel level)
{

  if (counter)
    counter->clearWaitingFor();

  Vector<Uint32> nodes;
  NodeRecordPtr nodePtr;
  nodePtr.i = cfirstAliveNode;
  do {
    jam();
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
    if (nodePtr.i != getOwnNodeId())
    {
      nodes.push_back(nodePtr.i);
    }
    nodePtr.i = nodePtr.p->nextNode;
  } while (nodePtr.i != RNIL);


  NdbNodeBitmask masked;
  Uint32 cnt = nodes.size();
  if (cnt <= 1)
  {
    goto do_send;
  }

  {
    Uint32 remove = (rand() % cnt);
    if (remove == 0)
      remove = 1;

    for (Uint32 i = 0; i<remove; i++)
    {
      Uint32 rand_node = rand() % nodes.size();
      masked.set(nodes[rand_node]);
      nodes.erase(rand_node);
    }
  }

do_send:
  char bufpos = 0;
  char buf[256];

  nodePtr.i = cfirstAliveNode;
  do {
    jam();
    ptrCheckGuard(nodePtr, MAX_NDB_NODES, nodeRecord);
    if (counter)
      counter->setWaitingFor(nodePtr.i);
    if (!masked.get(nodePtr.i))
    {
      if (fun)
      {
        (this->*fun)(signal, nodePtr.i, extra);
      }
      else
      {
        Uint32 ref = numberToRef(block, nodePtr.i);
        sendSignal(ref, gsn, signal, len, level);
      }
      BaseString::snprintf(buf+bufpos, sizeof(buf)-bufpos, "%u ", nodePtr.i);
    }
    else
    {
      BaseString::snprintf(buf+bufpos, sizeof(buf)-bufpos, "[%u] ", nodePtr.i);
    }
    bufpos = strlen(buf);
    nodePtr.i = nodePtr.p->nextNode;
  } while (nodePtr.i != RNIL);
  infoEvent("%s %s", msg, buf);
}

#endif

// MT LQH

Uint32
Dbdih::dihGetInstanceKey(Uint32 tabId, Uint32 fragId)
{
  TabRecordPtr tTabPtr;
  tTabPtr.i = tabId;
  ptrCheckGuard(tTabPtr, ctabFileSize, tabRecord);
  FragmentstorePtr tFragPtr;
loop:
  Uint32 tab_val = tTabPtr.p->m_lock.read_lock();
  getFragstore(tTabPtr.p, fragId, tFragPtr);
  Uint32 instanceKey = dihGetInstanceKey(tFragPtr);
  if (unlikely(!tTabPtr.p->m_lock.read_unlock(tab_val)))
    goto loop;
  return instanceKey;
}

/**
 *
 */
void
Dbdih::execCREATE_NODEGROUP_IMPL_REQ(Signal* signal)
{
  jamEntry();
  CreateNodegroupImplReq reqCopy = *(CreateNodegroupImplReq*)signal->getDataPtr();
  CreateNodegroupImplReq *req = &reqCopy;

  Uint32 err = 0;
  Uint32 rt = req->requestType;
  Uint64 gci = 0;
  switch(rt){
  case CreateNodegroupImplReq::RT_ABORT:
    jam(); // do nothing
    break;
  case CreateNodegroupImplReq::RT_PARSE:
  case CreateNodegroupImplReq::RT_PREPARE:
  case CreateNodegroupImplReq::RT_COMMIT:
  {
    Uint32 cnt = 0;
    for (Uint32 i = 0; i<NDB_ARRAY_SIZE(req->nodes) && req->nodes[i] ; i++)
    {
      cnt++;
      if(req->nodes[i] >= MAX_NDB_NODES)
      {
        err = CreateNodegroupRef::NodeNotDefined;
        goto error;
      }
      if (getNodeActiveStatus(req->nodes[i]) != Sysfile::NS_Configured)
      {
        jam();
        err = CreateNodegroupRef::NodeAlreadyInNodegroup;
        goto error;
      }
    }

    if (cnt != cnoReplicas)
    {
      jam();
      err = CreateNodegroupRef::InvalidNoOfNodesInNodegroup;
      goto error;
    }

    Uint32 ng = req->nodegroupId;
    NdbNodeBitmask tmp;
    tmp.set();
    for (Uint32 i = 0; i<cnoOfNodeGroups; i++)
    {
      ndbrequire(c_node_groups[i] < MAX_NDB_NODE_GROUPS);
      tmp.clear(c_node_groups[i]);
    }

    if (ng == RNIL && rt == CreateNodegroupImplReq::RT_PARSE)
    {
      jam();
      ng = tmp.find(0);
    }

    if (ng > MAX_NDB_NODE_GROUPS)
    {
      jam();
      err = CreateNodegroupRef::InvalidNodegroupId;
      goto error;
    }

    if (tmp.get(ng) == false)
    {
      jam();
      err = CreateNodegroupRef::NodegroupInUse;
      goto error;
    }

    if (rt == CreateNodegroupImplReq::RT_PARSE || rt == CreateNodegroupImplReq::RT_PREPARE)
    {
      /**
       * Check that atleast one of the nodes are alive
       */
      bool alive = false;
      for (Uint32 i = 0; i<cnoReplicas; i++)
      {
        jam();
        Uint32 nodeId = req->nodes[i];
        if (getNodeStatus(nodeId) == NodeRecord::ALIVE)
        {
          jam();
          alive = true;
          break;
        }
      }
      
      jam();
      if (alive == false)
      {
        jam();
        err = CreateNodegroupRef::NoNodeAlive;
        goto error;
      }
    }
    
    if (rt == CreateNodegroupImplReq::RT_PARSE)
    {
      jam();
      signal->theData[0] = 0;
      signal->theData[1] = ng;
      return;
    }

    if (rt == CreateNodegroupImplReq::RT_PREPARE)
    {
      jam(); // do nothing
      break;
    }

    ndbrequire(rt == CreateNodegroupImplReq::RT_COMMIT);
    for (Uint32 i = 0; i<cnoReplicas; i++)
    {
      Uint32 nodeId = req->nodes[i];
      Sysfile::setNodeGroup(nodeId, SYSFILE->nodeGroups, req->nodegroupId);
      if (getNodeStatus(nodeId) == NodeRecord::ALIVE)
      {
        jam();
        Sysfile::setNodeStatus(nodeId, SYSFILE->nodeStatus, Sysfile::NS_Active);
      }
      else
      {
        jam();
        Sysfile::setNodeStatus(nodeId, SYSFILE->nodeStatus, Sysfile::NS_ActiveMissed_1);
      }
      setNodeActiveStatus();
      setNodeGroups();
    }
    break;
  }
  case CreateNodegroupImplReq::RT_COMPLETE:
    jam();
    gci = m_micro_gcp.m_current_gci;
    break;
  }

  {
    CreateNodegroupImplConf* conf = (CreateNodegroupImplConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = req->senderData;
    conf->gci_hi = Uint32(gci >> 32);
    conf->gci_lo = Uint32(gci);
    sendSignal(req->senderRef, GSN_CREATE_NODEGROUP_IMPL_CONF, signal,
               CreateNodegroupImplConf::SignalLength, JBB);
  }
  return;

error:
  if (rt == CreateNodegroupImplReq::RT_PARSE)
  {
    jam();
    signal->theData[0] = err;
    return;
  }

  if (rt == CreateNodegroupImplReq::RT_PREPARE)
  {
    jam();
    CreateNodegroupImplRef * ref = (CreateNodegroupImplRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = req->senderData;
    ref->errorCode = err;
    sendSignal(req->senderRef, GSN_CREATE_NODEGROUP_IMPL_REF, signal,
               CreateNodegroupImplRef::SignalLength, JBB);
    return;
  }

  jamLine(err);
  ndbrequire(false);
}

/**
 *
 */
void
Dbdih::execDROP_NODEGROUP_IMPL_REQ(Signal* signal)
{
  jamEntry();
  DropNodegroupImplReq reqCopy = *(DropNodegroupImplReq*)signal->getDataPtr();
  DropNodegroupImplReq *req = &reqCopy;

  NodeGroupRecordPtr NGPtr;

  Uint32 err = 0;
  Uint32 rt = req->requestType;
  Uint64 gci = 0;
  switch(rt){
  case DropNodegroupImplReq::RT_ABORT:
    jam(); // do nothing
    break;
  case DropNodegroupImplReq::RT_PARSE:
  case DropNodegroupImplReq::RT_PREPARE:
    jam();
    NGPtr.i = req->nodegroupId;
    if (NGPtr.i >= MAX_NDB_NODE_GROUPS)
    {
      jam();
      err = DropNodegroupRef::NoSuchNodegroup;
      goto error;
    }
    ptrCheckGuard(NGPtr, MAX_NDB_NODE_GROUPS, nodeGroupRecord);

    if (NGPtr.p->nodegroupIndex == RNIL)
    {
      jam();
      err = DropNodegroupRef::NoSuchNodegroup;
      goto error;
    }

    if (NGPtr.p->m_ref_count)
    {
      jam();
      err = DropNodegroupRef::NodegroupInUse;
      goto error;
    }
    break;
  case DropNodegroupImplReq::RT_COMMIT:
  {
    jam();
    gci = m_micro_gcp.m_current_gci;
    break;
  }
  case DropNodegroupImplReq::RT_COMPLETE:
  {
    NGPtr.i = req->nodegroupId;
    ptrCheckGuard(NGPtr, MAX_NDB_NODE_GROUPS, nodeGroupRecord);
    for (Uint32 i = 0; i<NGPtr.p->nodeCount; i++)
    {
      jam();
      Uint32 nodeId = NGPtr.p->nodesInGroup[i];
      Sysfile::setNodeGroup(nodeId, SYSFILE->nodeGroups, NO_NODE_GROUP_ID);
      Sysfile::setNodeStatus(nodeId, SYSFILE->nodeStatus, Sysfile::NS_Configured);
    }
    setNodeActiveStatus();
    setNodeGroups();
    break;
  }
  }

  {
    DropNodegroupImplConf* conf = (DropNodegroupImplConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = req->senderData;
    conf->gci_hi = Uint32(gci >> 32);
    conf->gci_lo = Uint32(gci);
    sendSignal(req->senderRef, GSN_DROP_NODEGROUP_IMPL_CONF, signal,
               DropNodegroupImplConf::SignalLength, JBB);
  }
  return;

error:
  DropNodegroupImplRef * ref = (DropNodegroupImplRef*)signal->getDataPtrSend();
  ref->senderRef = reference();
  ref->senderData = req->senderData;
  ref->errorCode = err;
  sendSignal(req->senderRef, GSN_DROP_NODEGROUP_IMPL_REF, signal,
             DropNodegroupImplRef::SignalLength, JBB);
}

Uint32
Dbdih::getMinVersion() const
{
  Uint32 ver = getNodeInfo(getOwnNodeId()).m_version;
  NodeRecordPtr specNodePtr;
  specNodePtr.i = cfirstAliveNode;
  do
  {
    jam();
    ptrCheckGuard(specNodePtr, MAX_NDB_NODES, nodeRecord);
    Uint32 v = getNodeInfo(specNodePtr.i).m_version;
    if (v < ver)
    {
      jam();
      ver = v;
    }
    specNodePtr.i = specNodePtr.p->nextNode;
  } while (specNodePtr.i != RNIL);

  return ver;
}

Uint8
Dbdih::getMaxStartedFragCheckpointsForNode(Uint32 nodeId) const
{
  if (likely(getNodeInfo(nodeId).m_version >= NDBD_EXTRA_PARALLEL_FRAG_LCP))
  {
    return MAX_STARTED_FRAG_CHECKPOINTS_PER_NODE;
  }
  else
  {
    /* Older node - only 2 parallel frag checkpoints supported */
    return 2;
  }
}
  
  
/**
 * isolateNodes
 *
 * Get all live nodes to disconnect the set of victims
 * in minDelayMillis.
 *
 * The signals are sent to live nodes immediately, and
 * those nodes perform the delay, to reduce the chance
 * of lag on this node causing problems
 */
void
Dbdih::isolateNodes(Signal* signal,
                    Uint32 delayMillis,
                    const NdbNodeBitmask& victims)
{
  jam();
  
  IsolateOrd* ord = (IsolateOrd*) signal->theData;
  
  ord->senderRef          = reference();
  ord->isolateStep        = IsolateOrd::IS_REQ;
  ord->delayMillis        = delayMillis;

  victims.copyto(NdbNodeBitmask::Size, ord->nodesToIsolate);
  
  /* QMGR handles this */
  sendSignal(QMGR_REF,
             GSN_ISOLATE_ORD,
             signal,
             IsolateOrd::SignalLength,
             JBA);
}
