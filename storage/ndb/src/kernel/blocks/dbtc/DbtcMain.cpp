/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

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

#define DBTC_C

#include "Dbtc.hpp"
#include "md5_hash.hpp"
#include <RefConvert.hpp>
#include <ndb_limits.h>
#include <ndb_rand.h>

#include <signaldata/DiGetNodes.hpp>
#include <signaldata/EventReport.hpp>
#include <signaldata/TcKeyReq.hpp>
#include <signaldata/TcKeyConf.hpp>
#include <signaldata/TcKeyRef.hpp>
#include <signaldata/KeyInfo.hpp>
#include <signaldata/AttrInfo.hpp>
#include <signaldata/TransIdAI.hpp>
#include <signaldata/TcRollbackRep.hpp>
#include <signaldata/NodeFailRep.hpp>
#include <signaldata/ReadNodesConf.hpp>
#include <signaldata/NFCompleteRep.hpp>
#include <signaldata/LqhKey.hpp>
#include <signaldata/TcCommit.hpp>
#include <signaldata/TcContinueB.hpp>
#include <signaldata/TcKeyFailConf.hpp>
#include <signaldata/AbortAll.hpp>
#include <signaldata/DihScanTab.hpp>
#include <signaldata/ScanFrag.hpp>
#include <signaldata/ScanTab.hpp>
#include <signaldata/PrepDropTab.hpp>
#include <signaldata/DropTab.hpp>
#include <signaldata/AlterTab.hpp>
#include <signaldata/AlterTable.hpp>
#include <signaldata/CreateTrig.hpp>
#include <signaldata/CreateTrigImpl.hpp>
#include <signaldata/DropTrig.hpp>
#include <signaldata/DropTrigImpl.hpp>
#include <signaldata/FireTrigOrd.hpp>
#include <signaldata/TrigAttrInfo.hpp>
#include <signaldata/CreateIndx.hpp>
#include <signaldata/CreateIndxImpl.hpp>
#include <signaldata/DropIndx.hpp>
#include <signaldata/DropIndxImpl.hpp>
#include <signaldata/AlterIndx.hpp>
#include <signaldata/AlterIndxImpl.hpp>
#include <signaldata/ScanTab.hpp>
#include <signaldata/SystemError.hpp>
#include <signaldata/DumpStateOrd.hpp>
#include <signaldata/DisconnectRep.hpp>
#include <signaldata/TcHbRep.hpp>

#include <signaldata/PrepDropTab.hpp>
#include <signaldata/DropTab.hpp>
#include <signaldata/TcIndx.hpp>
#include <signaldata/IndxKeyInfo.hpp>
#include <signaldata/IndxAttrInfo.hpp>
#include <signaldata/PackedSignal.hpp>
#include <signaldata/SignalDroppedRep.hpp>
#include <signaldata/LqhTransReq.hpp>
#include <AttributeHeader.hpp>
#include <signaldata/DictTabInfo.hpp>
#include <AttributeDescriptor.hpp>
#include <SectionReader.hpp>
#include <KeyDescriptor.hpp>

#include <NdbOut.hpp>
#include <DebuggerNames.hpp>
#include <signaldata/CheckNodeGroups.hpp>

#include <signaldata/RouteOrd.hpp>
#include <signaldata/GCP.hpp>

#include <signaldata/DbinfoScan.hpp>
#include <signaldata/TransIdAI.hpp>
#include <signaldata/CreateTab.hpp>

#include <TransporterRegistry.hpp> // error 8035
#include <EventLogger.hpp>

#include <signaldata/CreateFKImpl.hpp>
#include <signaldata/DropFKImpl.hpp>
#include <kernel/Interpreter.hpp>
#include <signaldata/TuxBound.hpp>

#define JAM_FILE_ID 353

#define TC_TIME_SIGNAL_DELAY 50

extern EventLogger * g_eventLogger;

// Use DEBUG to print messages that should be
// seen only when we debug the product
#ifdef VM_TRACE
#define DEBUG(x) ndbout << "DBTC: "<< x << endl;
#else
#define DEBUG(x)
#endif
  
#define INTERNAL_TRIGGER_TCKEYREQ_JBA 0

#ifdef VM_TRACE
NdbOut &
operator<<(NdbOut& out, Dbtc::ConnectionState state){
  switch(state){
  case Dbtc::CS_CONNECTED: out << "CS_CONNECTED"; break;
  case Dbtc::CS_DISCONNECTED: out << "CS_DISCONNECTED"; break;
  case Dbtc::CS_STARTED: out << "CS_STARTED"; break;
  case Dbtc::CS_RECEIVING: out << "CS_RECEIVING"; break;
  case Dbtc::CS_RESTART: out << "CS_RESTART"; break;
  case Dbtc::CS_ABORTING: out << "CS_ABORTING"; break;
  case Dbtc::CS_COMPLETING: out << "CS_COMPLETING"; break;
  case Dbtc::CS_COMPLETE_SENT: out << "CS_COMPLETE_SENT"; break;
  case Dbtc::CS_PREPARE_TO_COMMIT: out << "CS_PREPARE_TO_COMMIT"; break;
  case Dbtc::CS_COMMIT_SENT: out << "CS_COMMIT_SENT"; break;
  case Dbtc::CS_START_COMMITTING: out << "CS_START_COMMITTING"; break;
  case Dbtc::CS_COMMITTING: out << "CS_COMMITTING"; break;
  case Dbtc::CS_REC_COMMITTING: out << "CS_REC_COMMITTING"; break;
  case Dbtc::CS_WAIT_ABORT_CONF: out << "CS_WAIT_ABORT_CONF"; break;
  case Dbtc::CS_WAIT_COMPLETE_CONF: out << "CS_WAIT_COMPLETE_CONF"; break;
  case Dbtc::CS_WAIT_COMMIT_CONF: out << "CS_WAIT_COMMIT_CONF"; break;
  case Dbtc::CS_FAIL_ABORTING: out << "CS_FAIL_ABORTING"; break;
  case Dbtc::CS_FAIL_ABORTED: out << "CS_FAIL_ABORTED"; break;
  case Dbtc::CS_FAIL_PREPARED: out << "CS_FAIL_PREPARED"; break;
  case Dbtc::CS_FAIL_COMMITTING: out << "CS_FAIL_COMMITTING"; break;
  case Dbtc::CS_FAIL_COMMITTED: out << "CS_FAIL_COMMITTED"; break;
  case Dbtc::CS_FAIL_COMPLETED: out << "CS_FAIL_COMPLETED"; break;
  case Dbtc::CS_START_SCAN: out << "CS_START_SCAN"; break;
  default:
    out << "Unknown: " << (int)state; break;
  }
  return out;
}
NdbOut &
operator<<(NdbOut& out, Dbtc::OperationState state){
  out << (int)state;
  return out;
}
NdbOut &
operator<<(NdbOut& out, Dbtc::AbortState state){
  out << (int)state;
  return out;
}
NdbOut &
operator<<(NdbOut& out, Dbtc::ReturnSignal state){
  out << (int)state;
  return out;
}
NdbOut &
operator<<(NdbOut& out, Dbtc::ScanRecord::ScanState state){
  out << (int)state;
  return out;
}
NdbOut &
operator<<(NdbOut& out, Dbtc::ScanFragRec::ScanFragState state){
  out << (int)state;
  return out;
}
#endif

extern Uint32 ErrorSignalReceive;
extern Uint32 ErrorMaxSegmentsToSeize;

void
Dbtc::updateBuddyTimer(ApiConnectRecordPtr apiPtr)
{
  if (apiPtr.p->buddyPtr != RNIL) {
    jamDebug();
    ApiConnectRecordPtr buddyApiPtr;
    buddyApiPtr.i = apiPtr.p->buddyPtr;
    ptrCheckGuard(buddyApiPtr, capiConnectFilesize, apiConnectRecord);
    if (getApiConTimer(buddyApiPtr.i) != 0) {
      if ((apiPtr.p->transid[0] == buddyApiPtr.p->transid[0]) &&
          (apiPtr.p->transid[1] == buddyApiPtr.p->transid[1])) {
        jam();
        setApiConTimer(buddyApiPtr.i, ctcTimer, __LINE__);
      } else {
        jam();
        // Not a buddy anymore since not the same transid
        apiPtr.p->buddyPtr = RNIL;
      }//if
    }//if
  }//if
}

static
inline
bool
tc_testbit(Uint32 flags, Uint32 flag)
{
  return (flags & flag) != 0;
}

static
inline
void
tc_clearbit(Uint32 & flags, Uint32 flag)
{
  flags &= ~(Uint32)flag;
}

void Dbtc::execCONTINUEB(Signal* signal) 
{
  UintR tcase;

  jamEntry();
  tcase = signal->theData[0];
  UintR Tdata0 = signal->theData[1];
  UintR Tdata1 = signal->theData[2];
  UintR Tdata2 = signal->theData[3];
  UintR Tdata3 = signal->theData[4];
#ifdef ERROR_INSERT
  UintR Tdata4 = signal->theData[5];
  UintR Tdata5 = signal->theData[6];
#endif
  switch (tcase) {
  case TcContinueB::ZSCAN_FOR_READ_BACKUP:
    jam();
    scan_for_read_backup(signal, Tdata0, Tdata1, Tdata2);
    return;
  case TcContinueB::ZRETURN_FROM_QUEUED_DELIVERY:
    jam();
    ndbabort();
    return;
  case TcContinueB::ZCOMPLETE_TRANS_AT_TAKE_OVER:
    jam();
    tcNodeFailptr.i = Tdata0;
    ptrCheckGuard(tcNodeFailptr, 1, tcFailRecord);
    completeTransAtTakeOverLab(signal, Tdata1);
    return;
  case TcContinueB::ZCONTINUE_TIME_OUT_CONTROL:
    jam();
    timeOutLoopStartLab(signal, Tdata0);
    return;
  case TcContinueB::ZNODE_TAKE_OVER_COMPLETED:
    jam();
    tcNodeFailptr.i = 0;
    ptrAss(tcNodeFailptr, tcFailRecord);
    nodeTakeOverCompletedLab(signal, Tdata0, 0);
    return;
  case TcContinueB::ZINITIALISE_RECORDS:
    jam();
    initialiseRecordsLab(signal, Tdata0, Tdata2, Tdata3);
    return;
  case TcContinueB::ZSEND_COMMIT_LOOP:
    jam();
    apiConnectptr.i = Tdata0;
    ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
    tcConnectptr.i = Tdata1;
    ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
    commit020Lab(signal);
    return;
  case TcContinueB::ZSEND_COMPLETE_LOOP:
    jam();
    apiConnectptr.i = Tdata0;
    ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
    tcConnectptr.i = Tdata1;
    ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
    complete010Lab(signal);
    return;
  case TcContinueB::ZHANDLE_FAILED_API_NODE:
    jam();
    handleFailedApiNode(signal, Tdata0, Tdata1);
    return;
  case TcContinueB::ZTRANS_EVENT_REP:
    jam();
    /* Send transaction counters report */
    {
      const Uint32 len = c_counters.build_event_rep(signal);
      sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, len, JBB);
    }

    {
      const Uint32 report_interval = 5000;
      const Uint32 len = c_counters.build_continueB(signal);
      signal->theData[0] = TcContinueB::ZTRANS_EVENT_REP;
      sendSignalWithDelay(cownref, GSN_CONTINUEB, signal, report_interval, len);
    }
    return;
  case TcContinueB::ZCONTINUE_TIME_OUT_FRAG_CONTROL:
    jam();
    timeOutLoopStartFragLab(signal, Tdata0);
    return;
  case TcContinueB::ZABORT_BREAK:
    jam();
    tcConnectptr.i = Tdata0;
    apiConnectptr.i = Tdata1;
    ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
    apiConnectptr.p->counter--;
    abort015Lab(signal);
    return;
  case TcContinueB::ZABORT_TIMEOUT_BREAK:
    jam();
    tcConnectptr.i = Tdata0;
    apiConnectptr.i = Tdata1;
    ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
    apiConnectptr.p->counter--;
    sendAbortedAfterTimeout(signal, 1);
    return;
  case TcContinueB::ZHANDLE_FAILED_API_NODE_REMOVE_MARKERS:
    jam();
    removeMarkerForFailedAPI(signal, Tdata0, Tdata1);
    return;
  case TcContinueB::ZWAIT_ABORT_ALL:
    jam();
    checkAbortAllTimeout(signal, Tdata0);
    return;
  case TcContinueB::ZCHECK_SCAN_ACTIVE_FAILED_LQH:
    jam();
    checkScanActiveInFailedLqh(signal, Tdata0, Tdata1);
    return;
  case TcContinueB::ZNF_CHECK_TRANSACTIONS:
    jam();
    nodeFailCheckTransactions(signal, Tdata0, Tdata1);
    return;
  case TcContinueB::TRIGGER_PENDING:
  {
    jam();
    ApiConnectRecordPtr transPtr;
    transPtr.i = Tdata0;
    ptrCheckGuard(transPtr, capiConnectFilesize, apiConnectRecord);
#ifdef ERROR_INSERT
    if (ERROR_INSERTED(8082))
    {
      /* Max of 100000 TRIGGER_PENDING TcContinueBs to 
       * single ApiConnectRecord
       * See testBlobs -bug 45768
       */
      if (++transPtr.p->continueBCount > 100000)
      {
        ndbabort();
      }
    }
#endif
    /* Check that ConnectRecord is for the same Trans Id */
    if (likely((transPtr.p->transid[0] == Tdata1) &&
               (transPtr.p->transid[1] == Tdata2)))
    {
      ndbrequire(tc_testbit(transPtr.p->m_flags,
                            ApiConnectRecord::TF_TRIGGER_PENDING));
      tc_clearbit(transPtr.p->m_flags, ApiConnectRecord::TF_TRIGGER_PENDING);
      /* Try executing triggers now */
      executeTriggers(signal, &transPtr);
    }
    return;
  }
  case TcContinueB::DelayTCKEYCONF:
    jam();
    apiConnectptr.i = Tdata0;
    ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
    sendtckeyconf(signal, Tdata1);
    return;
  case TcContinueB::ZSEND_FIRE_TRIG_REQ:
    jam();
    apiConnectptr.i = Tdata0;
    ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
    if (unlikely(! (apiConnectptr.p->transid[0] == Tdata1 &&
                    apiConnectptr.p->transid[1] == Tdata2 &&
                    apiConnectptr.p->apiConnectstate == CS_SEND_FIRE_TRIG_REQ)))
    {
      warningReport(signal, 29);
      return;
    }
    ndbrequire(apiConnectptr.p->lqhkeyreqrec > 0);
    apiConnectptr.p->lqhkeyreqrec--; // UNDO: prevent early completion

    sendFireTrigReq(signal, apiConnectptr,
                    signal->theData[4],
                    signal->theData[5]);
    return;
  case TcContinueB::ZSTART_FRAG_SCANS:
  {
    jam();
    ScanRecordPtr scanptr;
    scanptr.i = signal->theData[1];
    ptrCheckGuard(scanptr, cscanrecFileSize, scanRecord);
    apiConnectptr.i = scanptr.p->scanApiRec;
    ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
    sendDihGetNodesLab(signal, scanptr);
    return;
  }
  case TcContinueB::ZSEND_FRAG_SCANS:
  {
    jam();
    ScanRecordPtr scanptr;
    scanptr.i = signal->theData[1];
    ptrCheckGuard(scanptr, cscanrecFileSize, scanRecord);
    apiConnectptr.i = scanptr.p->scanApiRec;
    ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
    sendFragScansLab(signal, scanptr);
    return;
  }
#ifdef ERROR_INSERT
  case TcContinueB::ZDEBUG_DELAYED_ABORT:
  {
    char buf[128];
    BaseString::snprintf(buf, sizeof(buf), "Received CONTINUEB:ZDEBUG_DELAYED_ABORT");
    warningEvent("%s", buf);

    jam();
    apiConnectptr.i = Tdata0;
    ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
    apiConnectptr.p->returncode = Tdata3;
    apiConnectptr.p->returnsignal = (Dbtc::ReturnSignal)Tdata4;
    SET_ERROR_INSERT_VALUE(Tdata5);
    abort010Lab(signal);
    break;
  }
  case TcContinueB::ZDEBUG_DELAY_TCROLLBACKREP:
  {
    char buf[128];
    BaseString::snprintf(buf, sizeof(buf), "Received CONTINUEB:ZDEBUG_DELAY_TCROLLBACKREP");
    warningEvent("%s", buf);

    jam();
    if (ERROR_INSERTED(8101))
    {
      jam();
      sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 6);
    }
    else
    {
      TcRollbackRep * const tcRollbackRep = 
        (TcRollbackRep *) signal->getDataPtr();
      Uint32 blockRef = Tdata5;
      
      tcRollbackRep->connectPtr = Tdata0;
      tcRollbackRep->transId[0] = Tdata1;
      tcRollbackRep->transId[1] = Tdata2;
      tcRollbackRep->returnCode = Tdata3;
      tcRollbackRep->errorData = Tdata4;
      sendSignal(blockRef, GSN_TCROLLBACKREP, signal, 
                 TcRollbackRep::SignalLength, JBB);
    }
    break;
  }
#endif
  default:
    ndbabort();
  }//switch
}

void Dbtc::execDIGETNODESREF(Signal* signal) 
{
  jamEntry();
  terrorCode = signal->theData[1];
  releaseAtErrorLab(signal);
}

void Dbtc::execINCL_NODEREQ(Signal* signal) 
{
  jamEntry();
  tblockref = signal->theData[0];
  hostptr.i = signal->theData[1];
  ptrCheckGuard(hostptr, chostFilesize, hostRecord);
  hostptr.p->hostStatus = HS_ALIVE;
  c_alive_nodes.set(hostptr.i);

  signal->theData[0] = hostptr.i;
  signal->theData[1] = cownref;

  if (ERROR_INSERTED(8039))
  {
    CLEAR_ERROR_INSERT_VALUE;
    Uint32 save = signal->theData[0];
    signal->theData[0] = 9999;
    sendSignal(numberToRef(CMVMI, hostptr.i), 
	       GSN_NDB_TAMPER, signal, 1, JBB);
    signal->theData[0] = save;
    sendSignalWithDelay(tblockref, GSN_INCL_NODECONF, signal, 5000, 2);
    return;
  }

  Uint32 Tnode = hostptr.i;

  sendSignal(tblockref, GSN_INCL_NODECONF, signal, 2, JBB);

  if (m_deferred_enabled)
  {
    jam();
    if (!ndbd_deferred_unique_constraints(getNodeInfo(Tnode).m_version))
    {
      jam();
      m_deferred_enabled = 0;
    }
  }
}

void Dbtc::execREAD_NODESREF(Signal* signal) 
{
  jamEntry();
  ndbabort();
}

// create table prepare
void Dbtc::execTC_SCHVERREQ(Signal* signal) 
{
  jamEntry();
  if (! assembleFragments(signal)) {
    jam();
    return;
  }
  const TcSchVerReq* req = CAST_CONSTPTR(TcSchVerReq, signal->getDataPtr());
  tabptr.i = req->tableId;
  ptrCheckGuard(tabptr, ctabrecFilesize, tableRecord);
  tabptr.p->currentSchemaVersion = req->tableVersion;
  tabptr.p->m_flags = 0;
  tabptr.p->set_storedTable((bool)req->tableLogged);
  BlockReference retRef = req->senderRef;
  tabptr.p->tableType = (Uint8)req->tableType;
  BlockReference retPtr = req->senderData;
  Uint32 noOfKeyAttr = req->noOfPrimaryKeys;
  tabptr.p->singleUserMode = (Uint8)req->singleUserMode;
  Uint32 userDefinedPartitioning = (Uint8)req->userDefinedPartition;
  ndbrequire(noOfKeyAttr <= MAX_ATTRIBUTES_IN_INDEX);

  const KeyDescriptor* desc = g_key_descriptor_pool.getPtr(tabptr.i);
  ndbrequire(noOfKeyAttr == desc->noOfKeyAttr);

  ndbrequire(tabptr.p->get_prepared() == false);
  ndbrequire(tabptr.p->get_enabled() == false);
  tabptr.p->set_prepared(true);
  tabptr.p->set_enabled(false);
  tabptr.p->set_dropping(false);
  tabptr.p->noOfKeyAttr = desc->noOfKeyAttr;
  tabptr.p->hasCharAttr = desc->hasCharAttr;
  tabptr.p->noOfDistrKeys = desc->noOfDistrKeys;
  tabptr.p->hasVarKeys = desc->noOfVarKeys > 0;
  tabptr.p->set_user_defined_partitioning(userDefinedPartitioning);
  if (req->readBackup)
  {
    jam();
    D("Set ReadBackup Flag for table " << tabptr.i);
    tabptr.p->m_flags |= TableRecord::TR_READ_BACKUP;
    tabptr.p->m_flags |= TableRecord::TR_DELAY_COMMIT;
  }
  else
  {
    D("No ReadBackup Flag for table " << tabptr.i);
  }

  if (req->fullyReplicated)
  {
    jam();
    tabptr.p->m_flags |= TableRecord::TR_FULLY_REPLICATED;
  }

  TcSchVerConf * conf = (TcSchVerConf*)signal->getDataPtr();
  conf->senderRef = reference();
  conf->senderData = retPtr;
  sendSignal(retRef, GSN_TC_SCHVERCONF, signal,
             TcSchVerConf::SignalLength, JBB);
}//Dbtc::execTC_SCHVERREQ()

// create table commit
void Dbtc::execTAB_COMMITREQ(Signal* signal)
{
  jamEntry();
  Uint32 senderData = signal->theData[0];
  Uint32 senderRef = signal->theData[1];
  tabptr.i = signal->theData[2];
  ptrCheckGuard(tabptr, ctabrecFilesize, tableRecord);

  ndbrequire(tabptr.p->get_prepared() == true);
  ndbrequire(tabptr.p->get_enabled() == false);
  tabptr.p->set_enabled(true);
  tabptr.p->set_prepared(false);
  tabptr.p->set_dropping(false);

  signal->theData[0] = senderData;
  signal->theData[1] = reference();
  signal->theData[2] = tabptr.i;
  sendSignal(senderRef, GSN_TAB_COMMITCONF, signal, 3, JBB);
}

void
Dbtc::execPREP_DROP_TAB_REQ(Signal* signal)
{
  jamEntry();
  
  PrepDropTabReq* req = (PrepDropTabReq*)signal->getDataPtr();
  
  TableRecordPtr tabPtr;
  tabPtr.i = req->tableId;
  ptrCheckGuard(tabPtr, ctabrecFilesize, tableRecord);
  
  Uint32 senderRef = req->senderRef;
  Uint32 senderData = req->senderData;
  
  if(!tabPtr.p->get_enabled())
  {
    jam();
    PrepDropTabRef* ref = (PrepDropTabRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = senderData;
    ref->tableId = tabPtr.i;
    ref->errorCode = PrepDropTabRef::NoSuchTable;
    sendSignal(senderRef, GSN_PREP_DROP_TAB_REF, signal,
	       PrepDropTabRef::SignalLength, JBB);
    return;
  }

  if(tabPtr.p->get_dropping())
  {
    jam();
    PrepDropTabRef* ref = (PrepDropTabRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = senderData;
    ref->tableId = tabPtr.i;
    ref->errorCode = PrepDropTabRef::DropInProgress;
    sendSignal(senderRef, GSN_PREP_DROP_TAB_REF, signal,
	       PrepDropTabRef::SignalLength, JBB);
    return;
  }
  
  tabPtr.p->set_dropping(true);
  tabPtr.p->set_prepared(false);

  PrepDropTabConf* conf = (PrepDropTabConf*)signal->getDataPtrSend();
  conf->tableId = tabPtr.i;
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(senderRef, GSN_PREP_DROP_TAB_CONF, signal,
             PrepDropTabConf::SignalLength, JBB);
}

void
Dbtc::execDROP_TAB_REQ(Signal* signal)
{
  jamEntry();

  DropTabReq* req = (DropTabReq*)signal->getDataPtr();
  
  TableRecordPtr tabPtr;
  tabPtr.i = req->tableId;
  ptrCheckGuard(tabPtr, ctabrecFilesize, tableRecord);
  
  Uint32 senderRef = req->senderRef;
  Uint32 senderData = req->senderData;
  DropTabReq::RequestType rt = (DropTabReq::RequestType)req->requestType;

  if(!tabPtr.p->get_enabled() && rt == DropTabReq::OnlineDropTab){
    jam();
    DropTabRef* ref = (DropTabRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = senderData;
    ref->tableId = tabPtr.i;
    ref->errorCode = DropTabRef::NoSuchTable;
    sendSignal(senderRef, GSN_DROP_TAB_REF, signal,
	       DropTabRef::SignalLength, JBB);
    return;
  }

  if(!tabPtr.p->get_dropping() && rt == DropTabReq::OnlineDropTab){
    jam();
    DropTabRef* ref = (DropTabRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = senderData;
    ref->tableId = tabPtr.i;
    ref->errorCode = DropTabRef::DropWoPrep;
    sendSignal(senderRef, GSN_DROP_TAB_REF, signal,
	       DropTabRef::SignalLength, JBB);
    return;
  }
  
  tabPtr.p->set_enabled(false);
  tabPtr.p->set_prepared(false);
  tabPtr.p->set_dropping(false);
  
  DropTabConf * conf = (DropTabConf*)signal->getDataPtrSend();
  conf->tableId = tabPtr.i;
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(senderRef, GSN_DROP_TAB_CONF, signal,
	     PrepDropTabConf::SignalLength, JBB);
}

void Dbtc::scan_for_read_backup(Signal *signal,
                                Uint32 start_api_ptr,
                                Uint32 senderData,
                                Uint32 senderRef)
{
  ApiConnectRecord *localApiConnectRecord = apiConnectRecord;
  const Uint32 apiMaxIndex = capiConnectFilesize;
  Uint32 api_ptr;

  Uint32 loop_count = 0;
  for (api_ptr = start_api_ptr;
       api_ptr < apiMaxIndex && loop_count++ < 256;
       api_ptr++, loop_count++)
  {
    ApiConnectRecord * const regApiPtr = &localApiConnectRecord[api_ptr];
    switch (regApiPtr->apiConnectstate)
    {
      case CS_STARTED:
      {
        if (regApiPtr->firstTcConnect == RNIL)
        {
          /**
           * No transaction have started, safe to ignore these transactions.
           * If it starts up the delayed commit flag will be set.
           */
          jam();
        }
        else
        {
          /* Transaction is ongoing, set delayed commit flag. */
          jam();
          regApiPtr->m_flags |= ApiConnectRecord::TF_LATE_COMMIT;
        }
        break;
      }
      case CS_RECEIVING:
      {
        /**
         * Transaction is still ongoing and hasn't started commit, we will
         * set delayed commit flag.
         */
        jam();
        regApiPtr->m_flags |= ApiConnectRecord::TF_LATE_COMMIT;
        break;
      }
      default:
      {
        jam();
        /**
         * The transaction is either:
         * 1) Committing
         * 2) Scanning
         * 3) Aborting
         *
         * In neither of those cases we want to set the delayed commit flag.
         * Committing has already started, so we don't want to mix delayed
         * and not delayed in the same transaction. Scanning has no commit
         * phase so delayed commit flag is uninteresting. Aborting
         * transactions have no impact on data, so thus no delayed commit
         * is required.
         */
        break;
      }
    }
  }
  if (api_ptr == apiMaxIndex)
  {
    jam();
    /**
     * We're done and will return ALTER_TAB_CONF
     */
    AlterTabConf *conf = (AlterTabConf*)signal->getDataPtrSend();
    conf->senderRef = cownref;
    conf->senderData = senderData;
    conf->connectPtr = RNIL;
    sendSignal(senderRef, GSN_ALTER_TAB_CONF, signal,
               AlterTabConf::SignalLength, JBB);
    return;
  }
  signal->theData[0] = TcContinueB::ZSCAN_FOR_READ_BACKUP;
  signal->theData[1] = api_ptr;
  signal->theData[2] = senderData;
  signal->theData[3] = senderRef;
  sendSignal(cownref, GSN_CONTINUEB, signal, 4, JBB);
}

void Dbtc::execALTER_TAB_REQ(Signal * signal)
{
  const AlterTabReq* req = (const AlterTabReq*)signal->getDataPtr();
  const Uint32 senderRef = req->senderRef;
  const Uint32 senderData = req->senderData;
  const Uint32 tableVersion = req->tableVersion;
  const Uint32 newTableVersion = req->newTableVersion;
  AlterTabReq::RequestType requestType = 
    (AlterTabReq::RequestType) req->requestType;
  D("ALTER_TAB_REQ(TC)");

  TableRecordPtr tabPtr;
  tabPtr.i = req->tableId;
  ptrCheckGuard(tabPtr, ctabrecFilesize, tableRecord);

  switch (requestType) {
  case AlterTabReq::AlterTablePrepare:
    jam();
    if (AlterTableReq::getReadBackupFlag(req->changeMask))
    {
      if ((tabPtr.p->m_flags & TableRecord::TR_READ_BACKUP) != 0)
      {
        /**
         * Table had Read Backup flag set and is now clearing it.
         * No special handling we simply reset the flags in the
         * commit phase. No more handling is needed in prepare
         * phase.
         */
        D("Prepare to change ReadBackup Flag, already set for table"
          << tabPtr.i);
      }
      else
      {
        /**
         * Table have not set the Read Backup flag yet. We will set
         * it in a safe manner. We do this by first ensuring that
         * all current ongoing transactions delay their report of
         * commit (we do this since we don't know which transactions
         * are using this particular table and it is a very
         * temporary delay for these transactions currently ongoing
         * and the delay isn't very long either, no blocking will
         * occur here.
         *
         * We also set delayed commit flag on this table immediately
         * here to ensure that all transactions on this table is now
         * delayed.
         *
         * This means that when we return done on the prepare phase
         * the only transactions that are not delayed is the ones
         * that was committing already when we came here. These
         * have time until we're back here to commit to complete
         * that phase. So would be a very extreme case and possibly
         * even impossible case that not all these transactions
         * are completed yet.
         *
         * Finally in the commit phase we activate the Read Backup
         * flag also such that reads can start using the backup
         * replicas for reading.
         */
        D("Prepare to set ReadBackup Flag for table: " << tabPtr.i);
        tabPtr.p->m_flags |= TableRecord::TR_DELAY_COMMIT;
        scan_for_read_backup(signal, 0, senderData, senderRef);
        return;
      }
    }
    break;
  case AlterTabReq::AlterTableRevert:
    jam();
    tabPtr.p->currentSchemaVersion = tableVersion;

    if (AlterTableReq::getReadBackupFlag(req->changeMask))
    {
      if ((tabPtr.p->m_flags & TableRecord::TR_READ_BACKUP) == 0)
      {
        /**
         * We have set the DELAY_COMMIT flag, so need to reset here
         * since ALTER TABLE was reverted.
         */
        D("Revert ReadBackup Flag settings for table " << tabPtr.i);
        tabPtr.p->m_flags &= (~(TableRecord::TR_DELAY_COMMIT));
      }
    }
    break;
  case AlterTabReq::AlterTableCommit:
    jam();
    if (AlterTableReq::getReadBackupFlag(req->changeMask))
    {
      if ((tabPtr.p->m_flags & TableRecord::TR_READ_BACKUP) != 0)
      {
        jam();
        D("Commit clear ReadBackup Flag for table: " << tabPtr.i);
        tabPtr.p->m_flags &= (~(TableRecord::TR_DELAY_COMMIT));
        tabPtr.p->m_flags &= (~(TableRecord::TR_READ_BACKUP));
      }
      else
      {
        jam();
        D("Commit set ReadBackup Flag for table: " << tabPtr.i);
        tabPtr.p->m_flags |= TableRecord::TR_READ_BACKUP;
      }
    }
    tabPtr.p->currentSchemaVersion = newTableVersion;
    break;
  default:
    ndbabort();
  }

  // Request handled successfully 
  AlterTabConf* conf = (AlterTabConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = senderData;
  conf->connectPtr = RNIL;
  sendSignal(senderRef, GSN_ALTER_TAB_CONF, signal, 
	     AlterTabConf::SignalLength, JBB);
}

/* ***************************************************************************/
/*                                START / RESTART                            */
/* ***************************************************************************/
void Dbtc::execREAD_CONFIG_REQ(Signal* signal) 
{
  const ReadConfigReq * req = (ReadConfigReq*)signal->getDataPtr();
  Uint32 ref = req->senderRef;
  Uint32 senderData = req->senderData;
  ndbrequire(req->noOfParameters == 0);
  
  jamEntry();
  
  const ndb_mgm_configuration_iterator * p = 
    m_ctx.m_config.getOwnConfigIterator();
  ndbrequire(p != 0);
 
  initData();
  
  UintR apiConnect;
  UintR tcConnect;
  UintR tables;
  UintR localScan;
  UintR tcScan;

  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_TC_API_CONNECT, &apiConnect));
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_TC_TC_CONNECT, &tcConnect));
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_TC_TABLE, &tables));
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_TC_LOCAL_SCAN, &localScan));
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_TC_SCAN, &tcScan));

  ccacheFilesize = (apiConnect/3) + 1;
  capiConnectFilesize = apiConnect;
  ctcConnectFilesize  = tcConnect;
  ctabrecFilesize     = tables;
  cscanrecFileSize = tcScan;
  cscanFragrecFileSize = localScan;

  initRecords();
  initialiseRecordsLab(signal, 0, ref, senderData);

  Uint32 val = 3000;
  ndb_mgm_get_int_parameter(p, CFG_DB_TRANSACTION_DEADLOCK_TIMEOUT, &val);
  set_timeout_value(val);

  val = 1500;
  ndb_mgm_get_int_parameter(p, CFG_DB_HEARTBEAT_INTERVAL, &val);
  cDbHbInterval = (val < 10) ? 10 : val;

  val = 3000;
  ndb_mgm_get_int_parameter(p, CFG_DB_TRANSACTION_INACTIVE_TIMEOUT, &val);
  set_appl_timeout_value(val);

  val = 1;
  //ndb_mgm_get_int_parameter(p, CFG_DB_PARALLEL_TRANSACTION_TAKEOVER, &val);
  set_no_parallel_takeover(val);

  val = ~(Uint32)0;
  ndb_mgm_get_int_parameter(p, CFG_DB_MAX_DML_OPERATIONS_PER_TRANSACTION, &val);
  m_max_writes_per_trans = val;

  ctimeOutCheckDelay = 50; // 500ms
  ctimeOutCheckDelayScan = 40; // 400ms

  Pool_context pc;
  pc.m_block = this;

  c_fk_hash.setSize(16);
  c_fk_pool.init(RT_DBDICT_FILE, pc); // TODO

  time_track_init_histogram_limits();
}

void Dbtc::execSTTOR(Signal* signal) 
{
  Uint16 tphase;

  jamEntry();
                                                     /* START CASE */
  tphase = signal->theData[1];
  csignalKey = signal->theData[6];
  c_sttor_ref = signal->getSendersBlockRef();
  switch (tphase) {
  case ZSPH1:
    jam();
    startphase1x010Lab(signal);
    return;
  default:
    jam();
    sttorryLab(signal); /* START PHASE 255 */
    return;
  }//switch
}//Dbtc::execSTTOR()

void Dbtc::sttorryLab(Signal* signal) 
{
  signal->theData[0] = csignalKey;
  signal->theData[1] = 3;    /* BLOCK CATEGORY */
  signal->theData[2] = 2;    /* SIGNAL VERSION NUMBER */
  signal->theData[3] = ZSPH1;
  signal->theData[4] = 255;
  sendSignal(c_sttor_ref, GSN_STTORRY, signal, 5, JBB);
}//Dbtc::sttorryLab()

/* ***************************************************************************/
/*                          INTERNAL  START / RESTART                        */
/*****************************************************************************/
void Dbtc::execNDB_STTOR(Signal* signal) 
{
  Uint16 tndbstartphase;
  Uint16 tstarttype;

  jamEntry();
  NodeId nodeId = signal->theData[1];
  tndbstartphase = signal->theData[2];   /* START PHASE      */
  tstarttype = signal->theData[3];       /* START TYPE       */
  c_sttor_ref = signal->getSendersBlockRef();
  switch (tndbstartphase) {
  case ZINTSPH1:
    jam();
    intstartphase1x010Lab(signal, nodeId);
    return;
  case ZINTSPH2:
    jam();
    ndbsttorry010Lab(signal);
    return;
  case ZINTSPH3:
  {
    jam();
    intstartphase3x010Lab(signal);

    /* Start transaction counters event reporting. */
    const Uint32 len = c_counters.build_continueB(signal);
    signal->theData[0] = TcContinueB::ZTRANS_EVENT_REP;
    sendSignalWithDelay(cownref, GSN_CONTINUEB, signal, 10, len);
    return;
  }
  case ZINTSPH6:
  {
    jam();
    c_elapsed_time_millis = 0;
    init_elapsed_time(signal, c_latestTIME_SIGNAL);
    break;
  }
  default:
    jam();
    break;
  }//switch
  ndbsttorry010Lab(signal);
  return;
}//Dbtc::execNDB_STTOR()

void Dbtc::ndbsttorry010Lab(Signal* signal) 
{
  signal->theData[0] = cownref;
  sendSignal(c_sttor_ref, GSN_NDB_STTORRY, signal, 1, JBB);
}//Dbtc::ndbsttorry010Lab()

void
Dbtc::set_timeout_value(Uint32 timeOut)
{
  timeOut = timeOut / 10;
  if (timeOut < 2) {
    jam();
    timeOut = 100;
  }//if
  ctimeOutValue = timeOut;
}

void
Dbtc::set_appl_timeout_value(Uint32 timeOut)
{
  if (timeOut)
  {
    timeOut /= 10;
    if (timeOut < ctimeOutValue) {
      jam();
      c_appl_timeout_value = ctimeOutValue;
    }//if
  }
  c_appl_timeout_value = timeOut;
}

void
Dbtc::set_no_parallel_takeover(Uint32 noParallelTakeOver)
{
  if (noParallelTakeOver == 0) {
    jam();
    noParallelTakeOver = 1;
  } else if (noParallelTakeOver > MAX_NDB_NODES) {
    jam();
    noParallelTakeOver = MAX_NDB_NODES;
  }//if
  cnoParallelTakeOver = noParallelTakeOver;
}

/* ***************************************************************************/
/*                        S T A R T P H A S E 1 X                            */
/*                     INITIALISE BLOCKREF AND BLOCKNUMBERS                  */
/* ***************************************************************************/
void Dbtc::startphase1x010Lab(Signal* signal) 
{
  ctimeOutCheckCounter = 0;
  ctimeOutCheckFragCounter = 0;
  ctimeOutMissedHeartbeats = 0;
  ctimeOutCheckHeartbeat = 0;
  ctimeOutCheckLastHeartbeat = 0;
  ctimeOutMissedHeartbeatsScan = 0;
  ctimeOutCheckHeartbeatScan = 0;
  ctimeOutCheckLastHeartbeatScan = 0;
  ctimeOutCheckActive = TOCS_FALSE;
  ctimeOutCheckFragActive = TOCS_FALSE;
  sttorryLab(signal);
}//Dbtc::startphase1x010Lab()

/*****************************************************************************/
/*                        I N T S T A R T P H A S E 1 X                      */
/*                         INITIALISE ALL RECORDS.                           */
/*****************************************************************************/
void Dbtc::intstartphase1x010Lab(Signal* signal, NodeId nodeId)
{
  cownNodeid = nodeId;
  cownref =          reference();
  cdihblockref =     calcDihBlockRef(cownNodeid);
  cndbcntrblockref = calcNdbCntrBlockRef(cownNodeid);
  coperationsize = 0;
  cfailure_nr = 0;
  ndbsttorry010Lab(signal);
}//Dbtc::intstartphase1x010Lab()

/*****************************************************************************/
/*                         I N T S T A R T P H A S E 3 X                     */
/*****************************************************************************/
void Dbtc::intstartphase3x010Lab(Signal* signal) 
{
  signal->theData[0] = cownref;
  sendSignal(cndbcntrblockref, GSN_READ_NODESREQ, signal, 1, JBB);
}//Dbtc::intstartphase3x010Lab()

void Dbtc::execREAD_NODESCONF(Signal* signal) 
{
  UintR guard0;

  jamEntry();

  ReadNodesConf * const readNodes = (ReadNodesConf *)&signal->theData[0];

  csystemnodes  = readNodes->noOfNodes;
  cmasterNodeId = readNodes->masterNodeId;

  con_lineNodes = 0;
  arrGuard(csystemnodes, MAX_NDB_NODES);
  guard0 = csystemnodes - 1;
  arrGuard(guard0, MAX_NDB_NODES);       // Check not zero nodes

  for (unsigned i = 1; i < MAX_NDB_NODES; i++) {
    jam();
    if (NdbNodeBitmask::get(readNodes->allNodes, i)) {
      hostptr.i = i;
      ptrCheckGuard(hostptr, chostFilesize, hostRecord);
      hostptr.p->m_location_domain_id = 0;

      if (NdbNodeBitmask::get(readNodes->inactiveNodes, i)) {
        jam();
        hostptr.p->hostStatus = HS_DEAD;
      } else {
        jam();
        con_lineNodes++;
        hostptr.p->hostStatus = HS_ALIVE;
        c_alive_nodes.set(i);
        if (!ndbd_deferred_unique_constraints(getNodeInfo(i).m_version))
        {
          jam();
          m_deferred_enabled = 0;
        }
      }//if
    }//if
  }//for

  ndb_mgm_configuration *p =
    m_ctx.m_config.getClusterConfig();
  ndb_mgm_configuration_iterator *p_iter =
    ndb_mgm_create_configuration_iterator(p, CFG_SECTION_NODE);

  for (ndb_mgm_first(p_iter);
       ndb_mgm_valid(p_iter);
       ndb_mgm_next(p_iter))
  {
    jam();
    Uint32 location_domain_id = 0;
    Uint32 nodeId = 0;
    Uint32 nodeType = 0;
    ndbrequire(!ndb_mgm_get_int_parameter(p_iter, CFG_NODE_ID, &nodeId) &&
               nodeId != 0);
    ndbrequire(!ndb_mgm_get_int_parameter(p_iter,
                                          CFG_TYPE_OF_SECTION,
                                          &nodeType));
    jamLine(Uint16(nodeId));
    if (nodeType != NODE_TYPE_DB)
    {
      jam();
      continue;
    }
    hostptr.i = nodeId;
    ptrCheckGuard(hostptr, chostFilesize, hostRecord);

    ndb_mgm_get_int_parameter(p_iter,
                              CFG_LOCATION_DOMAIN_ID,
                              &location_domain_id);
    hostptr.p->m_location_domain_id = location_domain_id;
  }
  ndb_mgm_destroy_iterator(p_iter);
  {
    HostRecordPtr Town_hostptr;
    Town_hostptr.i = cownNodeid;
    ptrCheckGuard(Town_hostptr, chostFilesize, hostRecord);
    m_my_location_domain_id = Town_hostptr.p->m_location_domain_id;
  }
  ndbsttorry010Lab(signal);
}//Dbtc::execREAD_NODESCONF()

/*****************************************************************************/
/*                     A P I _ F A I L R E Q                                 */
// An API node has failed for some reason. We need to disconnect all API 
// connections to the API node. This also includes 
/*****************************************************************************/
void Dbtc::execAPI_FAILREQ(Signal* signal)
{
  /***************************************************************************
   * Set the block reference to return API_FAILCONF to. Set the number of api 
   * connects currently closing to one to indicate that we are still in the 
   * process of going through the api connect records. Thus checking for zero 
   * can only be true after all api connect records have been checked.
   **************************************************************************/
  jamEntry();  

  if (ERROR_INSERTED(8056))
  {
    CLEAR_ERROR_INSERT_VALUE;
    return;
  }
#ifdef ERROR_INSERT
  if (ERROR_INSERTED(8078))
  {
    c_lastFailedApi = signal->theData[0];
    SET_ERROR_INSERT_VALUE(8079);
  }
#endif

  capiFailRef = signal->theData[1];
  arrGuard(signal->theData[0], MAX_NODES);
  capiConnectClosing[signal->theData[0]] = 1;
  handleFailedApiNode(signal, signal->theData[0], (UintR)0);
}

/**
 * This function is used when we need to handle API disconnect/node failure
 * with waiting for a few signals.
 * In the case of API node failure we need to keep track of how many API
 * connect records are in the close down phase, we cannot complete the
 * API node failure handling until all nodes have completed this phase.
 *
 * For API disconnect (from receiving TCRELEASEREQ on active API connect)
 * we need no such counter, we maintain specific states for those two
 * variants.
 */
void
Dbtc::set_api_fail_state(Uint32 TapiFailedNode, bool apiNodeFailed)
{
  if (apiNodeFailed)
  {
    jam();
    capiConnectClosing[TapiFailedNode]++;
    apiConnectptr.p->apiFailState = ApiConnectRecord::AFS_API_FAILED;
  }
  else
  {
    jam();
    apiConnectptr.p->apiFailState = ApiConnectRecord::AFS_API_DISCONNECTED;
  }
}

bool Dbtc::handleFailedApiConnection(Signal *signal,
                                     Uint32 *TloopCount,
                                     Uint32 TapiFailedNode,
                                     bool apiNodeFailed)
{
  if (apiConnectptr.p->apiFailState == ApiConnectRecord::AFS_API_DISCONNECTED)
  {
    jam();
    /**
     * We are currently closing down the API connect record after receiving
     * a TCRELEASEREQ. Now we received an API node failure. We don't need
     * to restart the close down handling, it is already in progress. But
     * we need to add to counter and change state such that we are waiting
     * for API node fail handling to complete instead.
     *
     * We cannot come here from TCRELEASEREQ as we will simply send an
     * immediate CONF when receiving multiple TCRELEASEREQ.
     */
    ndbrequire(apiNodeFailed);
    set_api_fail_state(TapiFailedNode, apiNodeFailed);
    return true;
  }
#ifdef VM_TRACE
  if (apiConnectptr.p->apiFailState != ApiConnectRecord::AFS_API_OK)
  {
    ndbout << "Error in previous API fail handling discovered" << endl
           << "  apiConnectptr.i = " << apiConnectptr.i << endl
           << "  apiConnectstate = " << apiConnectptr.p->apiConnectstate 
           << endl
           << "  ndbapiBlockref = " << hex
           << apiConnectptr.p->ndbapiBlockref << endl
           << "  apiNode = " << refToNode(apiConnectptr.p->ndbapiBlockref) 
           << endl;
    if (apiConnectptr.p->lastTcConnect != RNIL)
    {
      jam();
      tcConnectptr.i = apiConnectptr.p->lastTcConnect;
      ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
      ndbout << "  tcConnectptr.i = " << tcConnectptr.i << endl
             << "  tcConnectstate = " << tcConnectptr.p->tcConnectstate 
             << endl;
    }
  }//if
#endif
      
  apiConnectptr.p->returnsignal = RS_NO_RETURN;
  /***********************************************************************/
  // The connected node is the failed node.
  /**********************************************************************/
  switch(apiConnectptr.p->apiConnectstate) {
  case CS_DISCONNECTED:
    /*********************************************************************/
    // These states do not need any special handling. 
    // Simply continue with the next.
    /*********************************************************************/
    jam();
    break;
  case CS_ABORTING:
    /*********************************************************************/
    // This could actually mean that the API connection is already 
    // ready to release if the abortState is IDLE.
    /*********************************************************************/
    if (apiConnectptr.p->abortState == AS_IDLE) {
      jam();
      releaseApiCon(signal, apiConnectptr.i);
    } else {
      jam();
      set_api_fail_state(TapiFailedNode, apiNodeFailed);
    }//if
    break;
  case CS_WAIT_ABORT_CONF:
  case CS_WAIT_COMMIT_CONF:
  case CS_START_COMMITTING:
  case CS_PREPARE_TO_COMMIT:
  case CS_COMMITTING:
  case CS_COMMIT_SENT:
    /*********************************************************************/
    // These states indicate that an abort process or commit process is 
    // already ongoing. We will set a state in the api record indicating 
    // that the API node has failed.
    // Also we will increase the number of outstanding api records to 
    // wait for before we can respond with API_FAILCONF.
    /*********************************************************************/
    jam();
    set_api_fail_state(TapiFailedNode, apiNodeFailed);
    break;
  case CS_START_SCAN:
  {
    /*********************************************************************/
    // The api record was performing a scan operation. We need to check 
    // on the scan state. Since completing a scan process might involve
    // sending several signals we will increase the loop count by 64.
    /*********************************************************************/
    jam();

    set_api_fail_state(TapiFailedNode, apiNodeFailed);

    ScanRecordPtr scanPtr;
    scanPtr.i = apiConnectptr.p->apiScanRec;
    ptrCheckGuard(scanPtr, cscanrecFileSize, scanRecord);
    close_scan_req(signal, scanPtr, true);

    (*TloopCount) += 64;
    break;
  }
  case CS_CONNECTED:
  case CS_REC_COMMITTING:
  case CS_RECEIVING:
  case CS_STARTED:
  case CS_SEND_FIRE_TRIG_REQ:
  case CS_WAIT_FIRE_TRIG_REQ:
    /*********************************************************************/
    // The api record was in the process of performing a transaction but 
    // had not yet sent all information. 
    // We need to initiate an ABORT since the API will not provide any 
    // more information. 
    // Since the abort can send many signals we will insert a real-time
    // break after checking this record.
    /*********************************************************************/
    jam();
    set_api_fail_state(TapiFailedNode, apiNodeFailed);
    abort010Lab(signal);
    (*TloopCount) = 256;
    break;
  case CS_RESTART:
  case CS_COMPLETING:
  case CS_COMPLETE_SENT:
  case CS_WAIT_COMPLETE_CONF:
  case CS_FAIL_ABORTING:
  case CS_FAIL_ABORTED:
  case CS_FAIL_PREPARED:
  case CS_FAIL_COMMITTING:
  case CS_FAIL_COMMITTED:
    /*********************************************************************/
    // These states are only valid on copy and fail API connections.
    /*********************************************************************/
  default:
    jam();
    jamLine(apiConnectptr.p->apiConnectstate);
    return false;
  }//switch
  return true;
}

void
Dbtc::handleFailedApiNode(Signal* signal, 
                          UintR TapiFailedNode, 
                          UintR TapiConnectPtr)
{
  UintR TloopCount = 0;
  arrGuard(TapiFailedNode, MAX_NODES);
  apiConnectptr.i = TapiConnectPtr;
  do {
    ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
    const UintR TapiNode = refToNode(apiConnectptr.p->ndbapiBlockref);
    if (TapiNode == TapiFailedNode)
    {
      bool handled = handleFailedApiConnection(signal,
                                               &TloopCount,
                                               TapiFailedNode,
                                               true);
      if (!handled)
      {
        systemErrorLab(signal, __LINE__);
        return;
      }
    }
    else
    {
      jam();
    }//if
    apiConnectptr.i++;
    if (apiConnectptr.i > ((capiConnectFilesize / 3) - 1)) {
      jam();
      /**
       * Finished with scanning connection record
       *
       * Now scan markers
       */
      removeMarkerForFailedAPI(signal, TapiFailedNode, 0);
      return;
    }//if
  } while (TloopCount++ < 256);
  signal->theData[0] = TcContinueB::ZHANDLE_FAILED_API_NODE;
  signal->theData[1] = TapiFailedNode;
  signal->theData[2] = apiConnectptr.i;
  sendSignal(cownref, GSN_CONTINUEB, signal, 3, JBB);
}//Dbtc::handleFailedApiNode()

void
Dbtc::removeMarkerForFailedAPI(Signal* signal, 
                               NodeId nodeId, 
                               Uint32 startBucket)
{
  TcFailRecordPtr node_fail_ptr;
  node_fail_ptr.i = 0;
  ptrAss(node_fail_ptr, tcFailRecord);
  if(node_fail_ptr.p->failStatus != FS_IDLE) {
    jam();
    DEBUG("Restarting removeMarkerForFailedAPI");
    /**
     * TC take-over in progress
     *   needs to restart as this
     *   creates new markers
     */
    signal->theData[0] = TcContinueB::ZHANDLE_FAILED_API_NODE_REMOVE_MARKERS;
    signal->theData[1] = nodeId;
    signal->theData[2] = 0;
    sendSignalWithDelay(cownref, GSN_CONTINUEB, signal, 500, 3);
    return;
  }

  CommitAckMarkerIterator iter;
  m_commitAckMarkerHash.next(startBucket, iter);
  
  const Uint32 RT_BREAK = 256;
  for(Uint32 i = 0; i<RT_BREAK || iter.bucket == startBucket; i++){ 
    jam();
    
    if(iter.curr.i == RNIL){
      jam();
      /**
       * Done with iteration
       */
      capiConnectClosing[nodeId]--;
      if (capiConnectClosing[nodeId] == 0) {
        jam();

        /********************************************************************/
        // No outstanding ABORT or COMMIT's of this failed API node. 
        // Perform SimulatedBlock level cleanup before sending
        // API_FAILCONF
        /********************************************************************/
        Callback cb = {safe_cast(&Dbtc::apiFailBlockCleanupCallback),
                       nodeId};
        simBlockNodeFailure(signal, nodeId, cb);
      }
      return;
    }
    
    if(iter.curr.p->apiNodeId == nodeId){
      jam();
      
      /**
       * Check so that the record is not still in use
       *
       */
      ApiConnectRecordPtr apiConnectPtr;
      apiConnectPtr.i = iter.curr.p->apiConnectPtr;
      ptrCheckGuard(apiConnectPtr, capiConnectFilesize, apiConnectRecord);
      if(apiConnectPtr.p->commitAckMarker == iter.curr.i){
	jam();
        /**
         * The record is still active, this means that the transaction is
         * currentlygoing through the abort or commit process as started
         * in the API node failure handling, we have to wait until this
         * phase is completed until we continue with our processing of this.
         * If it goes through the aborting process the record will disappear
         * while we're waiting for it. If it goes through the commit process
         * it will eventually reach copyApi whereafter we can remove the
         * marker from LQH.
         *
         * Don't remove it, but continueb retry with a short delay
         */
        signal->theData[0] = TcContinueB::ZHANDLE_FAILED_API_NODE_REMOVE_MARKERS;
        signal->theData[1] = nodeId;
        signal->theData[2] = iter.bucket;
        sendSignalWithDelay(cownref, GSN_CONTINUEB, signal, 1, 3);
        return;
      }
      /**
       * This should only happen if we have done copyApi and the referred
       * apiConnect record have already moved on to the free connections or
       * to a new transaction. The commit ack marker is still around,
       * normally the API would then send TC_COMMIT_ACK, but this hasn't
       * happened here since the API have failed. So we need to remove the
       * marker on behalf of the failed API, the failed API will not be able
       * to communicate its hearing of the commit, so we can simply remove
       * the commit ack marker in LQH.
       */
      sendRemoveMarkers(signal, iter.curr.p, 1);
      m_commitAckMarkerHash.release(iter.curr);
      
      break;
    } 
    m_commitAckMarkerHash.next(iter);
  } // for (... i<RT_BREAK ...)
  
  // Takes a RT-break to avoid starving other activity
  signal->theData[0] = TcContinueB::ZHANDLE_FAILED_API_NODE_REMOVE_MARKERS;
  signal->theData[1] = nodeId;
  signal->theData[2] = iter.bucket;
  sendSignal(cownref, GSN_CONTINUEB, signal, 3, JBB);
}

void Dbtc::handleApiFailState(Signal* signal, UintR TapiConnectptr)
{
  ApiConnectRecordPtr TlocalApiConnectptr;
  UintR TfailedApiNode;

  TlocalApiConnectptr.i = TapiConnectptr;
  ptrCheckGuard(TlocalApiConnectptr, capiConnectFilesize, apiConnectRecord);
  Uint32 apiFailState = TlocalApiConnectptr.p->apiFailState;
  TfailedApiNode = refToNode(TlocalApiConnectptr.p->ndbapiBlockref);
  arrGuard(TfailedApiNode, MAX_NODES);
  TlocalApiConnectptr.p->apiFailState = ApiConnectRecord::AFS_API_OK;
  releaseApiCon(signal, TapiConnectptr);
  if (apiFailState == ApiConnectRecord::AFS_API_FAILED)
  {
    capiConnectClosing[TfailedApiNode]--;
    if (capiConnectClosing[TfailedApiNode] == 0)
    {
      jam();

      /**
       * Perform block-level cleanups (e.g assembleFragments...)
       */
      Callback cb = {safe_cast(&Dbtc::apiFailBlockCleanupCallback),
                     TfailedApiNode};
      simBlockNodeFailure(signal, TfailedApiNode, cb);
    }
  }//if
}//Dbtc::handleApiFailState()

/****************************************************************************
 *                                T C S E I Z E R E Q
 * THE APPLICATION SENDS A REQUEST TO SEIZE A CONNECT RECORD TO CARRY OUT A 
 * TRANSACTION
 * TC BLOCK TAKE OUT A CONNECT RECORD FROM THE FREE LIST AND ESTABLISHES ALL 
 * NECESSARY CONNECTION BEFORE REPLYING TO THE APPLICATION BLOCK   
 ****************************************************************************/
void Dbtc::execTCSEIZEREQ(Signal* signal) 
{
  UintR tapiPointer;
  BlockReference tapiBlockref;       /* SENDER BLOCK REFERENCE*/
  
  jamEntry();
  tapiPointer = signal->theData[0]; /* REQUEST SENDERS CONNECT RECORD POINTER*/
  tapiBlockref = signal->theData[1]; /* SENDERS BLOCK REFERENCE*/

  if (signal->getLength() > 2)
  {
    ndbassert(instance() == signal->theData[2]);
  }
  
  const NodeState::StartLevel sl = 
    (NodeState::StartLevel)getNodeState().startLevel;

  const NodeId senderNodeId = refToNode(tapiBlockref);
  const bool local = senderNodeId == getOwnNodeId() || senderNodeId == 0;
  
  {
    {
      if (!(sl == NodeState::SL_STARTED ||
	    (sl == NodeState::SL_STARTING && local == true))) {
	jam();
	
	Uint32 errCode = 0;
	if(!local)
	  {
	    switch(sl){
	    case NodeState::SL_STARTING:
	      errCode = ZSYSTEM_NOT_STARTED_ERROR;
	      break;
	    case NodeState::SL_STOPPING_1:
	    case NodeState::SL_STOPPING_2:
              if (getNodeState().getSingleUserMode())
                break;
              // Fall through
            case NodeState::SL_STOPPING_3:
            case NodeState::SL_STOPPING_4:
              if(getNodeState().stopping.systemShutdown)
		errCode = ZCLUSTER_SHUTDOWN_IN_PROGRESS;
	      else
		errCode = ZNODE_SHUTDOWN_IN_PROGRESS;
	      break;
	    case NodeState::SL_SINGLEUSER:
	      break;
	    default:
	      errCode = ZWRONG_STATE;
	      break;
	    }
            if (errCode)
            {
              ndbout << "error: " << errCode << " on " << tapiPointer << endl;
              signal->theData[0] = tapiPointer;
              signal->theData[1] = errCode;
              sendSignal(tapiBlockref, GSN_TCSEIZEREF, signal, 2, JBB);
              return;
            }
	  }//if (!(sl == SL_SINGLEUSER))
      } //if
    }
  } 
  
  if (ERROR_INSERTED(8078) || ERROR_INSERTED(8079))
  {
    /* Clear testing of API_FAILREQ behaviour */
    CLEAR_ERROR_INSERT_VALUE;
  };

  seizeApiConnect(signal);
  if (terrorCode == ZOK) {
    jam();
    apiConnectptr.p->ndbapiConnect = tapiPointer;
    apiConnectptr.p->ndbapiBlockref = tapiBlockref;
    signal->theData[0] = apiConnectptr.p->ndbapiConnect;
    signal->theData[1] = apiConnectptr.i;
    signal->theData[2] = reference();
    sendSignal(tapiBlockref, GSN_TCSEIZECONF, signal, 3, JBB);
    return;
  }

  ndbout << "4006 on " << tapiPointer << endl;
  signal->theData[0] = tapiPointer;
  signal->theData[1] = terrorCode;
  sendSignal(tapiBlockref, GSN_TCSEIZEREF, signal, 2, JBB);
}//Dbtc::execTCSEIZEREQ()

/****************************************************************************/
/*                    T C R E L E A S E Q                                   */
/*                  REQUEST TO RELEASE A CONNECT RECORD                     */
/****************************************************************************/
void Dbtc::execTCRELEASEREQ(Signal* signal) 
{
  UintR tapiPointer;
  BlockReference tapiBlockref;     /* SENDER BLOCK REFERENCE*/

  jamEntry();
  tapiPointer = signal->theData[0]; /* REQUEST SENDERS CONNECT RECORD POINTER*/
  tapiBlockref = signal->theData[1];/* SENDERS BLOCK REFERENCE*/
  tuserpointer = signal->theData[2];
  if (tapiPointer >= capiConnectFilesize)
  {
    jam();
    ndbassert(false);
    signal->theData[0] = tuserpointer;
    signal->theData[1] = ZINVALID_CONNECTION;
    signal->theData[2] = __LINE__;
    sendSignal(tapiBlockref, GSN_TCRELEASEREF, signal, 3, JBB);
    return;
  }
  else
  {
    jam();
    apiConnectptr.i = tapiPointer;
  }//if
  ptrAss(apiConnectptr, apiConnectRecord);
  if (apiConnectptr.p->apiConnectstate == CS_DISCONNECTED ||
      apiConnectptr.p->apiFailState == ApiConnectRecord::AFS_API_DISCONNECTED)
  {
    jam();
    signal->theData[0] = tuserpointer;
    sendSignal(tapiBlockref, GSN_TCRELEASECONF, signal, 1, JBB);
  }
  else
  {
    if (tapiBlockref == apiConnectptr.p->ndbapiBlockref)
    {
      Uint32 dummy_loop_count = 0;
      Uint32 dummy_api_node = 0;
      /**
       * It isn't ok to receive a signal from a node that we're still
       * handling the API node failure, this must be some type of bug.
       * It is ok to receive multiple TCRELEASEREQ on the same connection.
       * We will handle it according to its state.
       */
      ndbrequire(apiConnectptr.p->apiFailState == ApiConnectRecord::AFS_API_OK);
      bool handled = handleFailedApiConnection(signal,
                                               &dummy_loop_count,
                                               dummy_api_node,
                                               false);
      ndbrequire(handled);
      /**
       * We have taken care of any necessary abort of ongoing statements.
       * We have ensured that if there is an ongoing transaction that
       * needs to be concluded that the API is not communicated to. The
       * transaction will be silently concluded and returned to free list
       * when concluded.
       * In most cases it will be immediately returned to
       * free list.
       *
       * NOTE: The NDB API can still get TRANSID_AI from LQHs and TCKEYCONF
       * from this TC on old Transaction ID's. So it is vital that the
       * NDB API does validate the signals before processing them.
       */
      jam();
      signal->theData[0] = tuserpointer;
      sendSignal(tapiBlockref,
                 GSN_TCRELEASECONF, signal, 1, JBB);
    }
    else
    {
      jam();
      ndbassert(false);
      signal->theData[0] = tuserpointer;
      signal->theData[1] = ZINVALID_CONNECTION;
      signal->theData[2] = __LINE__;
      signal->theData[3] = tapiBlockref;      
      signal->theData[4] = apiConnectptr.p->ndbapiBlockref;      
      sendSignal(tapiBlockref, GSN_TCRELEASEREF, signal, 5, JBB);
    }//if
  }//if
}//Dbtc::execTCRELEASEREQ()

/****************************************************************************/
// Error Handling for TCKEYREQ messages
/****************************************************************************/
void Dbtc::signalErrorRefuseLab(Signal* signal) 
{
  ptrGuard(apiConnectptr);
  if (apiConnectptr.p->apiConnectstate != CS_DISCONNECTED) {
    jam();
    apiConnectptr.p->abortState = AS_IDLE;
    apiConnectptr.p->apiConnectstate = CS_ABORTING;
  }//if
  sendSignalErrorRefuseLab(signal);
}//Dbtc::signalErrorRefuseLab()

void Dbtc::sendSignalErrorRefuseLab(Signal* signal) 
{
  ndbassert(false);
  ptrGuard(apiConnectptr);
  if (apiConnectptr.p->apiConnectstate != CS_DISCONNECTED) {
    jam();
    /* Force state print */
    printState(signal, 12, true);
    ndbabort();
    signal->theData[0] = apiConnectptr.p->ndbapiConnect;
    signal->theData[1] = signal->theData[ttransid_ptr];
    signal->theData[2] = signal->theData[ttransid_ptr + 1];
    signal->theData[3] = ZSIGNAL_ERROR;
    sendSignal(apiConnectptr.p->ndbapiBlockref, GSN_TCROLLBACKREP, 
	       signal, 4, JBB);
  }
}//Dbtc::sendSignalErrorRefuseLab()

void Dbtc::abortBeginErrorLab(Signal* signal) 
{
  apiConnectptr.p->transid[0] = signal->theData[ttransid_ptr];
  apiConnectptr.p->transid[1] = signal->theData[ttransid_ptr + 1];
  abortErrorLab(signal);
}//Dbtc::abortBeginErrorLab()

void Dbtc::printState(Signal* signal, int place, bool force_trace) 
{
  /* Always give minimal ApiConnectState trace via jam */
  jam();
  jamLine(apiConnectptr.p->apiConnectstate);
  jam();

#ifdef VM_TRACE
  /* Always trace in debug mode */
  force_trace = true;
#endif

  if (!force_trace)
    return;
  
  ndbout << "-- Dbtc::printState -- " << endl;
  ndbout << "Received from place = " << place
	 << " apiConnectptr.i = " << apiConnectptr.i
	 << " apiConnectstate = " << apiConnectptr.p->apiConnectstate << endl;
  ndbout << "ctcTimer = " << ctcTimer
	 << " ndbapiBlockref = " << hex <<apiConnectptr.p->ndbapiBlockref
	 << " Transid = " << apiConnectptr.p->transid[0]
	 << " " << apiConnectptr.p->transid[1] << endl;
  ndbout << " apiTimer = " << getApiConTimer(apiConnectptr.i)
	 << " counter = " << apiConnectptr.p->counter
	 << " lqhkeyconfrec = " << apiConnectptr.p->lqhkeyconfrec
	 << " lqhkeyreqrec = " << apiConnectptr.p->lqhkeyreqrec
         << " cascading_scans = " << apiConnectptr.p->cascading_scans_count << endl;
  ndbout << "abortState = " << apiConnectptr.p->abortState 
	 << " apiScanRec = " << apiConnectptr.p->apiScanRec
	 << " returncode = " << apiConnectptr.p->returncode << endl;
  ndbout << "tckeyrec = " << apiConnectptr.p->tckeyrec
	 << " returnsignal = " << apiConnectptr.p->returnsignal
	 << " apiFailState = " << apiConnectptr.p->apiFailState << endl;
  if (apiConnectptr.p->cachePtr != RNIL) {
    jam();
    CacheRecord *localCacheRecord = cacheRecord;
    UintR TcacheFilesize = ccacheFilesize;
    UintR TcachePtr = apiConnectptr.p->cachePtr;
    if (TcachePtr < TcacheFilesize) {
      jam();
      CacheRecord * const regCachePtr = &localCacheRecord[TcachePtr];
      ndbout << "currReclenAi = " << regCachePtr->currReclenAi
	     << " attrlength = " << regCachePtr->attrlength
	     << " tableref = " << regCachePtr->tableref
	     << " keylen = " << regCachePtr->keylen << endl;
    } else {
      jam();
      systemErrorLab(signal, __LINE__);
    }//if
  }//if
  return;
}//Dbtc::printState()

void
Dbtc::TCKEY_abort(Signal* signal, int place)
{
  switch (place) {
  case 0:
    jam();
    terrorCode = ZSTATE_ERROR;
    apiConnectptr.p->firstTcConnect = RNIL;
    printState(signal, 4);
    abortBeginErrorLab(signal);
    return;
  case 1:
    jam();
    printState(signal, 3);
    sendSignalErrorRefuseLab(signal);
    return;
  case 2:{
    printState(signal, 6);
    const TcKeyReq * const tcKeyReq = (TcKeyReq *)&signal->theData[0];
    const Uint32 t1 = tcKeyReq->transId1;
    const Uint32 t2 = tcKeyReq->transId2;
    signal->theData[0] = apiConnectptr.p->ndbapiConnect;
    signal->theData[1] = t1;
    signal->theData[2] = t2;
    signal->theData[3] = ZABORT_ERROR;
    ndbabort();
    sendSignal(apiConnectptr.p->ndbapiBlockref, GSN_TCROLLBACKREP, 
	       signal, 4, JBB);
    return;
  }
  case 3:
    jam();
    printState(signal, 7);
    noFreeConnectionErrorLab(signal);
    return;
  case 4:
    jam();
    terrorCode = ZERO_KEYLEN_ERROR;
    releaseAtErrorLab(signal);
    return;
  case 5:
    jam();
    terrorCode = ZNO_AI_WITH_UPDATE;
    releaseAtErrorLab(signal);
    return;
  case 6:
    jam();
    warningHandlerLab(signal, __LINE__);
    return;

  case 7:
    jam();
    tabStateErrorLab(signal);
    return;

  case 8:
    jam();
    wrongSchemaVersionErrorLab(signal);
    return;

  case 9:
    jam();
    terrorCode = ZSTATE_ERROR;
    releaseAtErrorLab(signal);
    return;

  case 10:
    jam();
    systemErrorLab(signal, __LINE__);
    return;

  case 11:
    jam();
    terrorCode = ZMORE_AI_IN_TCKEYREQ_ERROR;
    releaseAtErrorLab(signal);
    return;

  case 12:
    jam();
    terrorCode = ZSIMPLE_READ_WITHOUT_AI;
    releaseAtErrorLab(signal);
    return;

  case 13:
    jam();
    switch (tcConnectptr.p->tcConnectstate) {
    case OS_WAIT_KEYINFO:
      jam();
      printState(signal, 8);
      terrorCode = ZSTATE_ERROR;
      abortErrorLab(signal);
      return;
    default:
      jam();
      /********************************************************************/
      /*       MISMATCH BETWEEN STATE ON API CONNECTION AND THIS          */
      /*       PARTICULAR TC CONNECT RECORD. THIS MUST BE CAUSED BY NDB   */
      /*       INTERNAL ERROR.                                            */
      /********************************************************************/
      systemErrorLab(signal, __LINE__);
      return;
    }//switch
    return;

  case 15:
    jam();
    terrorCode = ZSCAN_NODE_ERROR;
    releaseAtErrorLab(signal);
    return;

  case 16:
    jam();
    systemErrorLab(signal, __LINE__);
    return;

  case 17:
    jam();
    systemErrorLab(signal, __LINE__);
    return;

  case 18:
    jam();
    warningHandlerLab(signal, __LINE__);
    return;

  case 19:
    jam();
    return;

  case 20:
    jam();
    warningHandlerLab(signal, __LINE__);
    return;

  case 21:
    jam();
    systemErrorLab(signal, __LINE__);
    return;

  case 22:
    jam();
    systemErrorLab(signal, __LINE__);
    return;

  case 23:
    jam();
    systemErrorLab(signal, __LINE__);
    return;

  case 24:
    jam();
    appendToSectionErrorLab(signal);
    return;

  case 25:
    jam();
    warningHandlerLab(signal, __LINE__);
    return;

  case 26:
    jam();
    return;

  case 27:
    systemErrorLab(signal, __LINE__);
    jam();
    return;

  case 28:
    jam();
    // NOT USED
    return;

  case 29:
    jam();
    systemErrorLab(signal, __LINE__);
    return;

  case 30:
    jam();
    systemErrorLab(signal, __LINE__);
    return;

  case 31:
    jam();
    systemErrorLab(signal, __LINE__);
    return;

  case 32:
    jam();
    systemErrorLab(signal, __LINE__);
    return;

  case 33:
    jam();
    systemErrorLab(signal, __LINE__);
    return;

  case 34:
    jam();
    systemErrorLab(signal, __LINE__);
    return;

  case 35:
    jam();
    systemErrorLab(signal, __LINE__);
    return;

  case 36:
    jam();
    systemErrorLab(signal, __LINE__);
    return;

  case 37:
    jam();
    systemErrorLab(signal, __LINE__);
    return;

  case 38:
    jam();
    systemErrorLab(signal, __LINE__);
    return;

  case 39:
    jam();
    systemErrorLab(signal, __LINE__);
    return;

  case 40:
    jam();
    systemErrorLab(signal, __LINE__);
    return;

  case 41:
    jam();
    systemErrorLab(signal, __LINE__);
    return;

  case 42:
    jam();
    systemErrorLab(signal, __LINE__);
    return;

  case 43:
    jam();
    systemErrorLab(signal, __LINE__);
    return;

  case 44:
    jam();
    systemErrorLab(signal, __LINE__);
    return;

  case 45:
    jam();
    systemErrorLab(signal, __LINE__);
    return;

  case 46:
    jam();
    systemErrorLab(signal, __LINE__);
    return;

  case 47:
    jam();
    terrorCode = apiConnectptr.p->returncode;
    releaseAtErrorLab(signal);
    return;

  case 48:
    jam();
    terrorCode = ZCOMMIT_TYPE_ERROR;
    releaseAtErrorLab(signal);
    return;

  case 49:
    jam();
    abortErrorLab(signal);
    return;

  case 50:
    jam();
    systemErrorLab(signal, __LINE__);
    return;

  case 51:
    jam();
    abortErrorLab(signal);
    return;

  case 52:
    jam();
    abortErrorLab(signal);
    return;

  case 53:
    jam();
    abortErrorLab(signal);
    return;

  case 54:
    jam();
    abortErrorLab(signal);
    return;

  case 55:
    jam();
    printState(signal, 5);
    sendSignalErrorRefuseLab(signal);
    return;
    
  case 56:{
    jam();
    terrorCode = ZNO_FREE_TC_MARKER;
    abortErrorLab(signal);
    return;
  }
  case 57:{
    jam();
    /**
     * Initialize object before starting error handling
     */
    initApiConnectRec(signal, apiConnectptr.p, true);
start_failure:
    switch(getNodeState().startLevel){
    case NodeState::SL_STOPPING_2:
      if (getNodeState().getSingleUserMode())
      {
        terrorCode  = ZCLUSTER_IN_SINGLEUSER_MODE;
        break;
      }
      // Fall through
    case NodeState::SL_STOPPING_3:
    case NodeState::SL_STOPPING_4:
      if(getNodeState().stopping.systemShutdown)
	terrorCode  = ZCLUSTER_SHUTDOWN_IN_PROGRESS;
      else
	terrorCode = ZNODE_SHUTDOWN_IN_PROGRESS;
      break;
    case NodeState::SL_SINGLEUSER:
      terrorCode  = ZCLUSTER_IN_SINGLEUSER_MODE;
      break;
    case NodeState::SL_STOPPING_1:
      if (getNodeState().getSingleUserMode())
      {
        terrorCode  = ZCLUSTER_IN_SINGLEUSER_MODE;
        break;
      }
      // Fall through
    default:
      terrorCode = ZWRONG_STATE;
      break;
    }
    abortErrorLab(signal);
    return;
  }

  case 58:{
    jam();
    releaseAtErrorLab(signal);
    return;
  }

  case 59:{
    jam();
    terrorCode = ZABORTINPROGRESS;
    abortErrorLab(signal);
    return;
  }
    
  case 60:
  {
    jam();
    initApiConnectRec(signal, apiConnectptr.p, true);
    apiConnectptr.p->m_flags |= ApiConnectRecord::TF_EXEC_FLAG;
    goto start_failure;
  }
  case 61:
  {
    jam();
    terrorCode = ZUNLOCKED_IVAL_TOO_HIGH;
    abortErrorLab(signal);
    return;
  }
  case 62:
  {
    jam();
    terrorCode = ZUNLOCKED_OP_HAS_BAD_STATE;
    abortErrorLab(signal);
    return;
  }
  case 63:
  {
    jam();
    /* Function not implemented yet */
    terrorCode = 4003;
    abortErrorLab(signal);
    return;
  }
  case 64:
  {
    jam();
    /* Invalid distribution key */
    terrorCode = ZBAD_DIST_KEY;
    abortErrorLab(signal);
    return;
  }
  case 65:
  {
    jam();
    terrorCode = ZTRANS_TOO_BIG;
    abortErrorLab(signal);
    return;
  }
  case 66:
  {
    jam();
    /* Function not implemented yet */
    terrorCode = 4003;
    abortErrorLab(signal);
    return;
  }
  case 67:
  {
    jam();
    terrorCode = ZNO_FREE_TC_MARKER_DATABUFFER;
    abortErrorLab(signal);
    return;
  }
  default:
    jam();
    systemErrorLab(signal, __LINE__);
    return;
  }//switch
}

static
inline
bool
compare_transid(Uint32* val0, Uint32* val1)
{
  Uint32 tmp0 = val0[0] ^ val1[0];
  Uint32 tmp1 = val0[1] ^ val1[1];
  return (tmp0 | tmp1) == 0;
}

void Dbtc::execKEYINFO(Signal* signal) 
{
  jamEntry();
  apiConnectptr.i = signal->theData[0];
  tmaxData = 20;
  if (apiConnectptr.i >= capiConnectFilesize) {
    TCKEY_abort(signal, 18);
    return;
  }//if
  ptrAss(apiConnectptr, apiConnectRecord);
  ttransid_ptr = 1;
  if (compare_transid(apiConnectptr.p->transid, signal->theData+1) == false) 
  {
    TCKEY_abort(signal, 19);
    return;
  }//if
  switch (apiConnectptr.p->apiConnectstate) {
  case CS_RECEIVING:
  case CS_REC_COMMITTING:
  case CS_START_SCAN:
    jam();
    /*empty*/;
    break;
  case CS_ABORTING:
    jam();
    return;     /* IGNORE */
  case CS_CONNECTED:
    jam();
    /****************************************************************>*/
    /*       MOST LIKELY CAUSED BY A MISSED SIGNAL. SEND REFUSE AND   */
    /*       SET STATE TO ABORTING.                                   */
    /****************************************************************>*/
    printState(signal, 11);
    signalErrorRefuseLab(signal);
    return;
  case CS_STARTED:
    jam();
    /****************************************************************>*/
    /*       MOST LIKELY CAUSED BY A MISSED SIGNAL. SEND REFUSE AND   */
    /*       SET STATE TO ABORTING. SINCE A TRANSACTION WAS STARTED   */
    /*       WE ALSO NEED TO ABORT THIS TRANSACTION.                  */
    /****************************************************************>*/
    terrorCode = ZSIGNAL_ERROR;
    printState(signal, 2);
    abortErrorLab(signal);
    return;
  default:
    jam();
    warningHandlerLab(signal, __LINE__);
    return;
  }//switch

  CacheRecord *localCacheRecord = cacheRecord;
  UintR TcacheFilesize = ccacheFilesize;
  UintR TcachePtr = apiConnectptr.p->cachePtr;
  UintR TtcTimer = ctcTimer;
  CacheRecord * const regCachePtr = &localCacheRecord[TcachePtr];
  if (TcachePtr >= TcacheFilesize) {
    TCKEY_abort(signal, 42);
    return;
  }//if
  setApiConTimer(apiConnectptr.i, TtcTimer, __LINE__);
  cachePtr.i = TcachePtr;
  cachePtr.p = regCachePtr;

  if (apiConnectptr.p->apiConnectstate == CS_START_SCAN)
  {
    jam();
    scanKeyinfoLab(signal);
    return;
  }

  tcConnectptr.i = apiConnectptr.p->lastTcConnect;
  ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
  switch (tcConnectptr.p->tcConnectstate) {
  case OS_WAIT_KEYINFO:
    jam();
    tckeyreq020Lab(signal);
    return;
  default:
    jam();
    terrorCode = ZSTATE_ERROR;
    abortErrorLab(signal);
    return;
  }//switch
}//Dbtc::execKEYINFO()

/**
 * sendKeyInfoTrain
 * Method to send a KeyInfo signal train from KeyInfo in the supplied
 * Section
 * KeyInfo will be taken from the section, starting at the supplied
 * offset
 */
void Dbtc::sendKeyInfoTrain(Signal* signal,
                            BlockReference TBRef,
                            Uint32 connectPtr,
                            Uint32 offset,
                            Uint32 sectionIVal)
{
  jam();

  signal->theData[0] = connectPtr;
  signal->theData[1] = apiConnectptr.p->transid[0];
  signal->theData[2] = apiConnectptr.p->transid[1];
  Uint32 * dst = signal->theData + KeyInfo::HeaderLength;

  ndbassert( sectionIVal != RNIL );
  SectionReader keyInfoReader(sectionIVal, getSectionSegmentPool());

  Uint32 totalLen= keyInfoReader.getSize();

  ndbassert( offset < totalLen );
  
  keyInfoReader.step(offset);
  totalLen-= offset;

  while(totalLen != 0)
  {
    Uint32 dataInSignal= MIN(KeyInfo::DataLength, totalLen); 
    keyInfoReader.getWords(dst, dataInSignal);
    totalLen-= dataInSignal;
    
    sendSignal(TBRef, GSN_KEYINFO, signal, 
               KeyInfo::HeaderLength + dataInSignal, JBB);
  } 
}//Dbtc::sendKeyInfoTrain()

/**
 * tckeyreq020Lab
 * Handle received KEYINFO signal
 */
void Dbtc::tckeyreq020Lab(Signal* signal) 
{
  CacheRecord * const regCachePtr = cachePtr.p;
  UintR TkeyLen = regCachePtr->keylen;
  UintR Tlen = regCachePtr->save1;
  UintR wordsInSignal= MIN(KeyInfo::DataLength,
                           (TkeyLen - Tlen));
  
  ndbassert(! regCachePtr->isLongTcKeyReq );
  ndbassert( regCachePtr->keyInfoSectionI != RNIL );

  /* Add received KeyInfo data to the existing KeyInfo section */
  if (! appendToSection(regCachePtr->keyInfoSectionI,
                        &signal->theData[KeyInfo::HeaderLength],
                        wordsInSignal))
  {
    jam();
    appendToSectionErrorLab(signal);
    return;
  }
  Tlen+= wordsInSignal;

  if (Tlen < TkeyLen)
  {
    /* More KeyInfo still to be read 
     * Set timer and state and wait
     */
    jam();
    setApiConTimer(apiConnectptr.i, ctcTimer, __LINE__);
    regCachePtr->save1 = Tlen;
    tcConnectptr.p->tcConnectstate = OS_WAIT_KEYINFO;
    return;
  }
  else
  {
    /* Have all the KeyInfo ... continue processing
     * TCKEYREQ
     */
    jam();
    tckeyreq050Lab(signal);
    return;
  }
}//Dbtc::tckeyreq020Lab()

void Dbtc::execATTRINFO(Signal* signal) 
{
  UintR Tdata1 = signal->theData[0];
  UintR Tlength = signal->length();
  UintR TapiConnectFilesize = capiConnectFilesize;
  ApiConnectRecord *localApiConnectRecord = apiConnectRecord;

  jamEntry();
  apiConnectptr.i = Tdata1;
  ttransid_ptr = 1;
  if (Tdata1 >= TapiConnectFilesize) {
    DEBUG("Drop ATTRINFO, wrong apiConnectptr");
    TCKEY_abort(signal, 18);
    return;
  }//if

  ApiConnectRecord * const regApiPtr = &localApiConnectRecord[Tdata1];
  apiConnectptr.p = regApiPtr;

  if (compare_transid(regApiPtr->transid, signal->theData+1) == false)
  {
    DEBUG("Drop ATTRINFO, wrong transid, lenght="<<Tlength
	  << " transid("<<hex<<signal->theData[1]<<", "<<signal->theData[2]);
    TCKEY_abort(signal, 19);
    return;
  }//if
  if (Tlength < 4) {
    DEBUG("Drop ATTRINFO, wrong length = " << Tlength);
    TCKEY_abort(signal, 20);
    return;
  }
  Tlength -= AttrInfo::HeaderLength;
  UintR TcompREC_COMMIT = (regApiPtr->apiConnectstate == CS_REC_COMMITTING);
  UintR TcompRECEIVING = (regApiPtr->apiConnectstate == CS_RECEIVING);
  UintR TcompBOTH = TcompREC_COMMIT | TcompRECEIVING;

  if (TcompBOTH) {
    jam();
    if (ERROR_INSERTED(8015)) {
      CLEAR_ERROR_INSERT_VALUE;
      return;
    }//if
    if (ERROR_INSERTED(8016)) {
      CLEAR_ERROR_INSERT_VALUE;
      return;
    }//if
    CacheRecord *localCacheRecord = cacheRecord;
    UintR TcacheFilesize = ccacheFilesize;
    UintR TcachePtr = regApiPtr->cachePtr;
    UintR TtcTimer = ctcTimer;
    CacheRecord * const regCachePtr = &localCacheRecord[TcachePtr];
    if (TcachePtr >= TcacheFilesize) {
      TCKEY_abort(signal, 43);
      return;
    }//if

    /* Update TC global cache ptr */
    cachePtr.i= TcachePtr;
    cachePtr.p= regCachePtr;

    regCachePtr->currReclenAi+= Tlength;
    int TattrlengthRemain = regCachePtr->attrlength - 
      regCachePtr->currReclenAi;
    
    /* Setup tcConnectptr to ensure that error handling etc.
     * can access required state
     */
    tcConnectptr.i = regApiPtr->lastTcConnect;
    ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);

    /* Add AttrInfo to any existing AttrInfo we have 
     * Some short TCKEYREQ signals have no ATTRINFO in
     * the TCKEYREQ itself
     */
    if (! appendToSection(regCachePtr->attrInfoSectionI,
                          &signal->theData[AttrInfo::HeaderLength],
                          Tlength))
    {
      DEBUG("No more section segments available");
      appendToSectionErrorLab(signal);
      return;
    }//if

    setApiConTimer(apiConnectptr.i, TtcTimer, __LINE__);

    if (TattrlengthRemain == 0) {
      /****************************************************************>*/
      /* HERE WE HAVE FOUND THAT THE LAST SIGNAL BELONGING TO THIS       */
      /* OPERATION HAVE BEEN RECEIVED. THIS MEANS THAT WE CAN NOW REUSE */
      /* THE API CONNECT RECORD. HOWEVER IF PREPARE OR COMMIT HAVE BEEN */
      /* RECEIVED THEN IT IS NOT ALLOWED TO RECEIVE ANY FURTHER          */
      /* OPERATIONS.                                                     */
      /****************************************************************>*/
      if (TcompRECEIVING) {
        jam();
        regApiPtr->apiConnectstate = CS_STARTED;
      } else {
        jam();
        regApiPtr->apiConnectstate = CS_START_COMMITTING;
      }//if
      attrinfoDihReceivedLab(signal);
    } else if (TattrlengthRemain < 0) {
      jam();
      DEBUG("ATTRINFO wrong total length="<<Tlength
	    <<", TattrlengthRemain="<<TattrlengthRemain
	    <<", TattrLen="<< regCachePtr->attrlength
	    <<", TcurrReclenAi="<< regCachePtr->currReclenAi);
      tcConnectptr.i = regApiPtr->lastTcConnect;
      ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
      aiErrorLab(signal);
    }//if
    return;
  } else if (regApiPtr->apiConnectstate == CS_START_SCAN) {
    jam();
    scanAttrinfoLab(signal, Tlength);
    return;
  } else {
    switch (regApiPtr->apiConnectstate) {
    case CS_ABORTING:
      jam();
      /* JUST IGNORE THE SIGNAL*/
      // DEBUG("Drop ATTRINFO, CS_ABORTING"); 
      return;
    case CS_CONNECTED:
      jam();
      /* MOST LIKELY CAUSED BY A MISSED SIGNAL.*/
      // DEBUG("Drop ATTRINFO, CS_CONNECTED"); 
      return;
    case CS_STARTED:
      jam();
      /****************************************************************>*/
      /*       MOST LIKELY CAUSED BY A MISSED SIGNAL. SEND REFUSE AND   */
      /*       SET STATE TO ABORTING. SINCE A TRANSACTION WAS STARTED   */
      /*       WE ALSO NEED TO ABORT THIS TRANSACTION.                  */
      /****************************************************************>*/
      terrorCode = ZSIGNAL_ERROR;
      printState(signal, 1);
      abortErrorLab(signal);
      return;
    default:
      jam();
      /****************************************************************>*/
      /*       SIGNAL RECEIVED IN AN UNEXPECTED STATE. WE IGNORE SIGNAL */
      /*       SINCE WE DO NOT REALLY KNOW WHERE THE ERROR OCCURRED.    */
      /****************************************************************>*/
      DEBUG("Drop ATTRINFO, illegal state="<<regApiPtr->apiConnectstate); 
      printState(signal, 9);
      return;
    }//switch
  }//if
}//Dbtc::execATTRINFO()

/* *********************************************************************>> */
/*                                                                        */
/*       MODULE: HASH MODULE                                              */
/*       DESCRIPTION: CONTAINS THE HASH VALUE CALCULATION                 */
/* *********************************************************************> */
void Dbtc::hash(Signal* signal)
{
  UintR*  Tdata32;
  
  CacheRecord * const regCachePtr = cachePtr.p;
  SegmentedSectionPtr keyInfoSection;
  UintR keylen = (UintR)regCachePtr->keylen;
  Uint32 distKey = regCachePtr->distributionKeyIndicator;
  
  getSection(keyInfoSection, regCachePtr->keyInfoSectionI);

  ndbassert( keyInfoSection.sz <= MAX_KEY_SIZE_IN_WORDS );
  ndbassert( keyInfoSection.sz == keylen );
  /* Copy KeyInfo section from segmented storage into linear storage
   * in signal->theData
   */
  if (keylen <= SectionSegment::DataLength)
  {
    /* No need to copy keyinfo into a linear space 
     * Note that we require that the data in the section is
     * 64-bit aligned for md5_hash below
     */
    ndbassert( keyInfoSection.p != NULL );

    Tdata32= &keyInfoSection.p->theData[0];
  }
  else
  {
    /* Copy segmented keyinfo into linear space in the signal */
    Tdata32= signal->theData;
    copy(Tdata32, keyInfoSection);
  }

  Uint32 tmp[4];
  if(!regCachePtr->m_special_hash)
  {
    md5_hash(tmp, (Uint64*)&Tdata32[0], keylen);
  }
  else
  {
    if (regCachePtr->m_no_hash)
    {
      /* No need for tuple key hash at LQH */
      ndbassert(distKey); /* User must supply distkey */
      Uint32 zero[4] = {0, 0, 0, 0};
      *tmp = *zero;
    }
    else
    {
      handle_special_hash(tmp, Tdata32, keylen, regCachePtr->tableref, !distKey);
    }
  }
  
  /* Primary key hash value is first word of hash on PK columns
   * Distribution key hash value is second word of hash on distribution
   * key columns, or a user defined value
   */
  thashValue = tmp[0];
  if (distKey){
    jam();
    tdistrHashValue = regCachePtr->distributionKey;
  } else {
    jamDebug();
    tdistrHashValue = tmp[1];
  }//if
}//Dbtc::hash()

bool
Dbtc::handle_special_hash(Uint32 dstHash[4], 
                          const Uint32* src, Uint32 srcLen, 
			  Uint32 tabPtrI,
			  bool distr)
{
  const Uint32 MAX_KEY_SIZE_IN_LONG_WORDS= 
    (MAX_KEY_SIZE_IN_WORDS + 1) / 2;
  Uint64 alignedWorkspace[MAX_KEY_SIZE_IN_LONG_WORDS * MAX_XFRM_MULTIPLY];
  Uint32* workspace= (Uint32*)alignedWorkspace;
  const TableRecord* tabPtrP = &tableRecord[tabPtrI];
  const bool hasVarKeys = tabPtrP->hasVarKeys;
  const bool hasCharAttr = tabPtrP->hasCharAttr;
  const bool compute_distkey = distr && (tabPtrP->noOfDistrKeys > 0);
  
  const Uint32 *hashInput = workspace;
  Uint32 inputLen = 0;
  Uint32 keyPartLen[MAX_ATTRIBUTES_IN_INDEX];
  Uint32 * keyPartLenPtr;

  /* Normalise KeyInfo into workspace if necessary */
  if(hasCharAttr || (compute_distkey && hasVarKeys))
  {
    keyPartLenPtr = keyPartLen;
    inputLen = xfrm_key_hash(tabPtrI,
                             src,
                             workspace,
                             sizeof(alignedWorkspace) >> 2,
                             keyPartLenPtr);
    if (unlikely(inputLen == 0))
    {
      goto error;
    }
  } 
  else 
  {
    /* Keyinfo already suitable for hash */
    hashInput = src;
    inputLen = srcLen;
    keyPartLenPtr = 0;
  }
  
  /* Calculate primary key hash */
  md5_hash(dstHash, (Uint64*)hashInput, inputLen);
  
  /* If the distribution key != primary key then we have to
   * form a distribution key from the primary key and calculate 
   * a separate distribution hash based on this
   */
  if(compute_distkey)
  {
    jam();
    
    Uint32 distrKeyHash[4];
    /* Reshuffle primary key columns to get just distribution key */
    Uint32 len = create_distr_key(tabPtrI, hashInput, workspace, keyPartLenPtr);
    /* Calculate distribution key hash */
    md5_hash(distrKeyHash, (Uint64*) workspace, len);

    /* Just one word used for distribution */
    dstHash[1] = distrKeyHash[1];
  }
  return true;  // success

error:
  terrorCode = ZINVALID_KEY;
  return false;
}

/*
INIT_API_CONNECT_REC
---------------------------
*/
/* ========================================================================= */
/* =======                       INIT_API_CONNECT_REC                ======= */
/*                                                                           */
/* ========================================================================= */
void Dbtc::initApiConnectRec(Signal* signal,
                             ApiConnectRecord * const regApiPtr,
			     bool releaseIndexOperations) 
{
  const TcKeyReq * const tcKeyReq = (TcKeyReq *)&signal->theData[0];
  UintR TfailureNr = cfailure_nr;
  UintR Ttransid0 = tcKeyReq->transId1;
  UintR Ttransid1 = tcKeyReq->transId2;

  tc_clearbit(regApiPtr->m_flags, ApiConnectRecord::TF_EXEC_FLAG);
  regApiPtr->returncode = 0;
  regApiPtr->returnsignal = RS_TCKEYCONF;
  ndbassert(regApiPtr->firstTcConnect == RNIL);
  regApiPtr->firstTcConnect = RNIL;
  regApiPtr->lastTcConnect = RNIL;
  regApiPtr->firedFragId = RNIL;
  regApiPtr->globalcheckpointid = 0;
  regApiPtr->lqhkeyconfrec = 0;
  regApiPtr->lqhkeyreqrec = 0;
  regApiPtr->tckeyrec = 0;
  regApiPtr->tcindxrec = 0;
  tc_clearbit(regApiPtr->m_flags,
              ApiConnectRecord::TF_COMMIT_ACK_MARKER_RECEIVED);
  regApiPtr->failureNr = TfailureNr;
  regApiPtr->transid[0] = Ttransid0;
  regApiPtr->transid[1] = Ttransid1;
  ndbrequire(regApiPtr->commitAckMarker == RNIL);
  ndbrequire(regApiPtr->num_commit_ack_markers == 0);
/**
 * We wanted to verify those variables around commitAckMarkers,
 * So we have an ndbrequire around them, this means we can
 * remove the setting of them since we already verified that
 * they already have the correct value.
 * regApiPtr->num_commit_ack_markers = 0;
 * regApiPtr->commitAckMarker = RNIL;
 */
  regApiPtr->buddyPtr = RNIL;
  regApiPtr->currSavePointId = 0;
  regApiPtr->m_transaction_nodes.clear();
  regApiPtr->singleUserMode = 0;
  regApiPtr->m_pre_commit_pass = 0;
  regApiPtr->cascading_scans_count = 0;
  // FiredTriggers should have been released when previous transaction ended. 
  ndbrequire(regApiPtr->theFiredTriggers.isEmpty());
  // Index data
  tc_clearbit(regApiPtr->m_flags,
              ApiConnectRecord::TF_INDEX_OP_RETURN);
  regApiPtr->noIndexOp = 0;
  if(releaseIndexOperations)
  {
    releaseAllSeizedIndexOperations(regApiPtr);
  }
  else
  {
    regApiPtr->m_start_ticks = getHighResTimer();
  }
  regApiPtr->immediateTriggerId = RNIL;

  tc_clearbit(regApiPtr->m_flags,
              ApiConnectRecord::TF_DEFERRED_CONSTRAINTS);
  tc_clearbit(regApiPtr->m_flags,
              ApiConnectRecord::TF_DISABLE_FK_CONSTRAINTS);
  c_counters.ctransCount++;

#ifdef ERROR_INSERT
  regApiPtr->continueBCount = 0;
#endif

  regApiPtr->m_write_count = 0;
}//Dbtc::initApiConnectRec()

int
Dbtc::seizeTcRecord(Signal* signal)
{
  ApiConnectRecord * const regApiPtr = apiConnectptr.p;
  TcConnectRecord *localTcConnectRecord = tcConnectRecord;
  UintR TfirstfreeTcConnect = cfirstfreeTcConnect;
  UintR TtcConnectFilesize = ctcConnectFilesize;
  tcConnectptr.i = TfirstfreeTcConnect;
  if (TfirstfreeTcConnect >= TtcConnectFilesize) {
    int place = 3;
    if (TfirstfreeTcConnect != RNIL) {
      place = 10;
    }//if
    TCKEY_abort(signal, place);
    return 1;
  }//if
  //--------------------------------------------------------------------------
  // Optimised version of ptrAss(tcConnectptr, tcConnectRecord)
  //--------------------------------------------------------------------------
  TcConnectRecord * const regTcPtr = 
                           &localTcConnectRecord[TfirstfreeTcConnect];

  UintR TlastTcConnect = regApiPtr->lastTcConnect;
  UintR TtcConnectptrIndex = tcConnectptr.i;
  TcConnectRecordPtr tmpTcConnectptr;

  cfirstfreeTcConnect = regTcPtr->nextTcConnect;
  tcConnectptr.p = regTcPtr;

  c_counters.cconcurrentOp++;

  regTcPtr->prevTcConnect = TlastTcConnect;
  regTcPtr->nextTcConnect = RNIL;
  regTcPtr->numFiredTriggers = 0;
  regTcPtr->numReceivedTriggers = 0;
  regTcPtr->triggerExecutionCount = 0;
  regTcPtr->triggeringOperation = RNIL;
  regTcPtr->m_special_op_flags = 0;
  regTcPtr->indexOp = RNIL;
  regTcPtr->currentTriggerId = RNIL;
  regTcPtr->tcConnectstate = OS_ABORTING;
  regTcPtr->noOfNodes = 0;

  regApiPtr->lastTcConnect = TtcConnectptrIndex;

  if (TlastTcConnect == RNIL) {
    jamDebug();
    regApiPtr->firstTcConnect = TtcConnectptrIndex;
  } else {
    tmpTcConnectptr.i = TlastTcConnect;
    jamDebug();
    ptrCheckGuard(tmpTcConnectptr, TtcConnectFilesize, localTcConnectRecord);
    tmpTcConnectptr.p->nextTcConnect = TtcConnectptrIndex;
  }//if
  return 0;
}//Dbtc::seizeTcRecord()

int
Dbtc::seizeCacheRecord(Signal* signal)
{
  ApiConnectRecord * const regApiPtr = apiConnectptr.p;
  UintR TfirstfreeCacheRec = cfirstfreeCacheRec;
  UintR TcacheFilesize = ccacheFilesize;
  CacheRecord *localCacheRecord = cacheRecord;
  if (TfirstfreeCacheRec >= TcacheFilesize) {
    TCKEY_abort(signal, 41);
    return 1;
  }//if
  CacheRecord * const regCachePtr = &localCacheRecord[TfirstfreeCacheRec];

  regApiPtr->cachePtr = TfirstfreeCacheRec;
  cfirstfreeCacheRec = regCachePtr->nextCacheRec;
  cachePtr.i = TfirstfreeCacheRec;
  cachePtr.p = regCachePtr;

  regCachePtr->currReclenAi = 0;
  regCachePtr->keyInfoSectionI = RNIL;
  regCachePtr->attrInfoSectionI = RNIL;
  return 0;
}//Dbtc::seizeCacheRecord()  

void
Dbtc::releaseCacheRecord(ApiConnectRecordPtr transPtr, CacheRecord* regCachePtr)
{
  ApiConnectRecord * const regApiPtr = transPtr.p;
  UintR TfirstfreeCacheRec = cfirstfreeCacheRec;
  UintR TCacheIndex = transPtr.p->cachePtr;
  regCachePtr->nextCacheRec = TfirstfreeCacheRec;
  cfirstfreeCacheRec = TCacheIndex;
  regApiPtr->cachePtr = RNIL;
}

void
Dbtc::dump_trans(ApiConnectRecordPtr transPtr)
{
  printf("transid %u [ 0x%x 0x%x ] state: %u flags: 0x%x m_pre_commit_pass: %u\n",
         transPtr.i,
         transPtr.p->transid[0],
         transPtr.p->transid[1],
         transPtr.p->apiConnectstate,
         transPtr.p->m_flags,
         transPtr.p->m_pre_commit_pass);

  Uint32 i = 0;
  tcConnectptr.i = transPtr.p->firstTcConnect;
  while (tcConnectptr.i != RNIL)
  {
    jam();
    ptrCheckGuard(tcConnectptr,
                  ctcConnectFilesize, tcConnectRecord);

    Ptr<TcDefinedTriggerData> trigPtr;
    if (tcConnectptr.p->currentTriggerId != RNIL)
    {
      c_theDefinedTriggers.getPtr(trigPtr, tcConnectptr.p->currentTriggerId);
    }

    printf(" %u : opPtrI: 0x%x op: %u state: %u triggeringOperation: 0x%x flags: 0x%x triggerType: %u apiConnect: %u\n",
           i++,
           tcConnectptr.i,
           tcConnectptr.p->operation,
           tcConnectptr.p->tcConnectstate,
           tcConnectptr.p->triggeringOperation,
           tcConnectptr.p->m_special_op_flags,
           tcConnectptr.p->currentTriggerId == RNIL ? RNIL :
           (Uint32)trigPtr.p->triggerType,
           tcConnectptr.p->apiConnect);

    tcConnectptr.i = tcConnectptr.p->nextTcConnect;
  }
}

bool
Dbtc::hasOp(ApiConnectRecordPtr transPtr, Uint32 opPtrI)
{
  TcConnectRecordPtr tcPtr;
  tcPtr.i = transPtr.p->firstTcConnect;
  while (tcPtr.i != RNIL)
  {
    jam();
    ptrCheckGuard(tcPtr,
                  ctcConnectFilesize, tcConnectRecord);
    if (tcPtr.i == opPtrI)
    {
      return tcPtr.p->apiConnect == transPtr.i;
    }

    tcPtr.i = tcPtr.p->nextTcConnect;
  }

  return false;
}

/*****************************************************************************/
/*                               T C K E Y R E Q                             */
/* AFTER HAVING ESTABLISHED THE CONNECT, THE APPLICATION BLOCK SENDS AN      */
/* OPERATION REQUEST TO TC. ALL NECESSARY INFORMATION TO CARRY OUT REQUEST   */
/* IS FURNISHED IN PARAMETERS. TC STORES THIS INFORMATION AND ENQUIRES       */
/* FROM DIH ABOUT THE NODES WHICH MAY HAVE THE REQUESTED DATA                */
/*****************************************************************************/
void Dbtc::execTCKEYREQ(Signal* signal) 
{
  Uint32 sendersBlockRef = signal->getSendersBlockRef();
  UintR compare_transid1, compare_transid2;
  const TcKeyReq * const tcKeyReq = (TcKeyReq *)signal->getDataPtr();
  UintR Treqinfo;
  SectionHandle handle(this, signal);

  jamEntry();
  /*-------------------------------------------------------------------------
   * Common error routines are used for several signals, they need to know 
   * where to find the transaction identifier in the signal.
   *-------------------------------------------------------------------------*/
  const UintR TapiIndex = tcKeyReq->apiConnectPtr;
  const UintR TapiMaxIndex = capiConnectFilesize;
  const UintR TtabIndex = tcKeyReq->tableId;
  const UintR TtabMaxIndex = ctabrecFilesize;
  ApiConnectRecord *localApiConnectRecord = apiConnectRecord;

  ttransid_ptr = 6; 
  apiConnectptr.i = TapiIndex;
  if (TapiIndex >= TapiMaxIndex) {
    releaseSections(handle);
    TCKEY_abort(signal, 6);
    return;
  }//if
  if (TtabIndex >= TtabMaxIndex) {
    releaseSections(handle);
    TCKEY_abort(signal, 7);
    return;
  }//if
  
#ifdef ERROR_INSERT
  if (ERROR_INSERTED(8079))
  {
    /* Test that no signals received after API_FAILREQ */
    if (refToNode(sendersBlockRef) == c_lastFailedApi)
    {
      /* Signal from API node received *after* API_FAILREQ */
      ndbabort();
    }
  }
#endif

  Treqinfo = tcKeyReq->requestInfo;
  //--------------------------------------------------------------------------
  // Optimised version of ptrAss(tabptr, tableRecord)
  // Optimised version of ptrAss(apiConnectptr, apiConnectRecord)
  //--------------------------------------------------------------------------
  ApiConnectRecord * const regApiPtr = &localApiConnectRecord[TapiIndex];
  apiConnectptr.i = TapiIndex;
  apiConnectptr.p = regApiPtr;

  Uint32 TstartFlag = TcKeyReq::getStartFlag(Treqinfo);
  Uint32 TexecFlag =
    TcKeyReq::getExecuteFlag(Treqinfo) ? ApiConnectRecord::TF_EXEC_FLAG : 0;

  Uint16 Tspecial_op_flags = regApiPtr->m_special_op_flags;
  bool isIndexOpReturn = tc_testbit(regApiPtr->m_flags,
                                    ApiConnectRecord::TF_INDEX_OP_RETURN);
  bool isExecutingTrigger = Tspecial_op_flags & TcConnectRecord::SOF_TRIGGER;
  regApiPtr->m_special_op_flags = 0; // Reset marker
  regApiPtr->m_flags |= TexecFlag;
  TableRecordPtr localTabptr;
  localTabptr.i = TtabIndex;
  localTabptr.p = &tableRecord[TtabIndex];
  switch (regApiPtr->apiConnectstate) {
  case CS_CONNECTED:{
    if (TstartFlag == 1 &&
        getAllowStartTransaction(refToNode(sendersBlockRef),
                                 localTabptr.p->singleUserMode) == true)
    {
      //---------------------------------------------------------------------
      // Initialise API connect record if transaction is started.
      //---------------------------------------------------------------------
      jam();
      initApiConnectRec(signal, regApiPtr);
      regApiPtr->m_flags |= TexecFlag;
    } else {
      releaseSections(handle);
      if (getAllowStartTransaction(refToNode(sendersBlockRef),
                                   localTabptr.p->singleUserMode) == true)
      {

	/*------------------------------------------------------------------
	 * WE EXPECTED A START TRANSACTION. SINCE NO OPERATIONS HAVE BEEN 
	 * RECEIVED WE INDICATE THIS BY SETTING FIRST_TC_CONNECT TO RNIL TO 
	 * ENSURE PROPER OPERATION OF THE COMMON ABORT HANDLING.
	 *-----------------------------------------------------------------*/
	TCKEY_abort(signal, 0);
	return;
      } else {
	/**
	 * getAllowStartTransaction(refToNode(sendersBlockRef)) == false
	 */
	TCKEY_abort(signal, TexecFlag ? 60 : 57);
	return;
      }//if
    }
  }
  break;
  case CS_STARTED:
    if(TstartFlag == 1 && regApiPtr->firstTcConnect == RNIL)
    {
      /**
       * If last operation in last transaction was a simple/dirty read
       *  it does not have to be committed or rollbacked hence,
       *  the state will be CS_STARTED
       */
      jam();
      if (unlikely(getNodeState().getSingleUserMode()) &&
          getNodeState().getSingleUserApi() != refToNode(sendersBlockRef) &&
          !localTabptr.p->singleUserMode)
      {
        releaseSections(handle);
	TCKEY_abort(signal, TexecFlag ? 60 : 57);
        return;
      }
      initApiConnectRec(signal, regApiPtr);
      regApiPtr->m_flags |= TexecFlag;
    } else { 
      //----------------------------------------------------------------------
      // Transaction is started already. 
      // Check that the operation is on the same transaction.
      //-----------------------------------------------------------------------
      compare_transid1 = regApiPtr->transid[0] ^ tcKeyReq->transId1;
      compare_transid2 = regApiPtr->transid[1] ^ tcKeyReq->transId2;
      jam();
      compare_transid1 = compare_transid1 | compare_transid2;
      if (compare_transid1 != 0) {
        releaseSections(handle);
	TCKEY_abort(signal, 1);
	return;
      }//if
    }
    break;
  case CS_ABORTING:
    if (regApiPtr->abortState == AS_IDLE) {
      if (TstartFlag == 1) {
        if(getAllowStartTransaction(refToNode(sendersBlockRef),
                                    localTabptr.p->singleUserMode) == false)
        {
          releaseSections(handle);
          TCKEY_abort(signal, TexecFlag ? 60 : 57);
          return;
        }
	//--------------------------------------------------------------------
	// Previous transaction had been aborted and the abort was completed. 
	// It is then OK to start a new transaction again.
	//--------------------------------------------------------------------
        jam();
        initApiConnectRec(signal, regApiPtr);
	regApiPtr->m_flags |= TexecFlag;
      } else if(TexecFlag) {
        releaseSections(handle);
	TCKEY_abort(signal, 59);
	return;
      } else { 
	//--------------------------------------------------------------------
	// The current transaction was aborted successfully. 
	// We will not do anything before we receive an operation 
	// with a start indicator. We will ignore this signal.
	//--------------------------------------------------------------------
	jam();
	DEBUG("Drop TCKEYREQ - apiConnectState=CS_ABORTING, ==AS_IDLE");
        releaseSections(handle);
        return;
      }//if
    } else {
      //----------------------------------------------------------------------
      // Previous transaction is still aborting
      //----------------------------------------------------------------------
      jam();
      releaseSections(handle);
      if (TstartFlag == 1) {
	//--------------------------------------------------------------------
	// If a new transaction tries to start while the old is 
	// still aborting, we will report this to the starting API.
	//--------------------------------------------------------------------
        TCKEY_abort(signal, 2);
        return;
      } else if(TexecFlag) {
        TCKEY_abort(signal, 59);
        return;
      }
      //----------------------------------------------------------------------
      // Ignore signals without start indicator set when aborting transaction.
      //----------------------------------------------------------------------
      DEBUG("Drop TCKEYREQ - apiConnectState=CS_ABORTING, !=AS_IDLE");
      return;
    }//if
    break;
  case CS_START_COMMITTING:
  case CS_SEND_FIRE_TRIG_REQ:
  case CS_WAIT_FIRE_TRIG_REQ:
    jam();
    if(isIndexOpReturn || isExecutingTrigger){
      break;
    }
    // Fall through
  default:
    jam();
    jamLine(regApiPtr->apiConnectstate);
    /*----------------------------------------------------------------------
     * IN THIS CASE THE NDBAPI IS AN UNTRUSTED ENTITY THAT HAS SENT A SIGNAL 
     * WHEN IT WAS NOT EXPECTED TO. 
     * WE MIGHT BE IN A PROCESS TO RECEIVE, PREPARE, 
     * COMMIT OR COMPLETE AND OBVIOUSLY THIS IS NOT A DESIRED EVENT.
     * WE WILL ALWAYS COMPLETE THE ABORT HANDLING BEFORE WE ALLOW 
     * ANYTHING TO HAPPEN ON THIS CONNECTION AGAIN. 
     * THUS THERE IS NO ACTION FROM THE API THAT CAN SPEED UP THIS PROCESS.
     *---------------------------------------------------------------------*/
    releaseSections(handle);
    TCKEY_abort(signal, 55);
    return;
  }//switch
  
  if (localTabptr.p->checkTable(tcKeyReq->tableSchemaVersion)) {
    ;
  } else {
    /*-----------------------------------------------------------------------*/
    /* THE API IS WORKING WITH AN OLD SCHEMA VERSION. IT NEEDS REPLACEMENT.  */
    /* COULD ALSO BE THAT THE TABLE IS NOT DEFINED.                          */
    /*-----------------------------------------------------------------------*/
    releaseSections(handle);
    TCKEY_abort(signal, 8);
    return;
  }//if
  
  //-------------------------------------------------------------------------
  // Error Insertion for testing purposes. Test to see what happens when no
  // more TC records available.
  //-------------------------------------------------------------------------
  if (ERROR_INSERTED(8032)) {
    releaseSections(handle);
    TCKEY_abort(signal, 3);
    return;
  }//if
  
  if (seizeTcRecord(signal) != 0) {
    releaseSections(handle);
    return;
  }//if
  
  if (seizeCacheRecord(signal) != 0) {
    releaseSections(handle);
    return;
  }//if

  CRASH_INSERTION(8063);
  
  TcConnectRecord * const regTcPtr = tcConnectptr.p;
  CacheRecord * const regCachePtr = cachePtr.p;

  /*
    INIT_TC_CONNECT_REC 
    -------------------------
  */
  /* ---------------------------------------------------------------------- */
  /* -------     INIT OPERATION RECORD WITH SIGNAL DATA AND RNILS   ------- */
  /*                                                                        */
  /* ---------------------------------------------------------------------- */

  UintR Tlqhkeyreqrec = regApiPtr->lqhkeyreqrec;
  regApiPtr->lqhkeyreqrec = Tlqhkeyreqrec + 1;

  /* If we have any sections at all then this is a long TCKEYREQ */
  regCachePtr->isLongTcKeyReq= ( handle.m_cnt != 0 );

  UintR TapiConnectptrIndex = apiConnectptr.i;
  UintR TsenderData = tcKeyReq->senderData;

  if (ERROR_INSERTED(8065))
  {
    ErrorSignalReceive= DBTC;
    ErrorMaxSegmentsToSeize= 10;
  }
  if (ERROR_INSERTED(8066))
  {
    ErrorSignalReceive= DBTC;
    ErrorMaxSegmentsToSeize= 1;
  }
  if (ERROR_INSERTED(8067))
  {
    ErrorSignalReceive= DBTC;
    ErrorMaxSegmentsToSeize= 0;
  }
  if (ERROR_INSERTED(8068))
  {
    ErrorSignalReceive= 0;
    ErrorMaxSegmentsToSeize= 0;
    CLEAR_ERROR_INSERT_VALUE;
    DEBUG("Max segments to seize cleared");
  }
#ifdef ERROR_INSERT
  if (ErrorSignalReceive == DBTC)
  {
    DEBUG("Max segments to seize : " 
          << ErrorMaxSegmentsToSeize);
  }
#endif

  /* Key and attribute lengths are passed in the header for 
   * short TCKEYREQ and  passed as section lengths for long 
   * TCKEYREQ
   */
  UintR TkeyLength = 0;
  UintR TattrLen = 0;
  UintR titcLenAiInTckeyreq = 0;

  if (regCachePtr->isLongTcKeyReq)
  {
    SegmentedSectionPtr keyInfoSec;
    if (handle.getSection(keyInfoSec, TcKeyReq::KeyInfoSectionNum))
      TkeyLength= keyInfoSec.sz;

    SegmentedSectionPtr attrInfoSec;
    if (handle.getSection(attrInfoSec, TcKeyReq::AttrInfoSectionNum))
      TattrLen= attrInfoSec.sz;

    if (TcKeyReq::getDeferredConstraints(Treqinfo))
    {
      regApiPtr->m_flags |= ApiConnectRecord::TF_DEFERRED_CONSTRAINTS;
    }

    if (TcKeyReq::getDisableFkConstraints(Treqinfo))
    {
      regApiPtr->m_flags |= ApiConnectRecord::TF_DISABLE_FK_CONSTRAINTS;
    }
    regCachePtr->m_read_committed_base =
                              TcKeyReq::getReadCommittedBaseFlag(Treqinfo);
  }
  else
  {
    TkeyLength = TcKeyReq::getKeyLength(Treqinfo);
    TattrLen= TcKeyReq::getAttrinfoLen(tcKeyReq->attrLen);
    titcLenAiInTckeyreq = TcKeyReq::getAIInTcKeyReq(Treqinfo);
    regCachePtr->m_read_committed_base = 0;
  }
  if (refToMain(sendersBlockRef) == DBUTIL)
  {
    jam();
    Tspecial_op_flags |= TcConnectRecord::SOF_UTIL_FLAG;
  }
  regCachePtr->keylen = TkeyLength;
  regCachePtr->lenAiInTckeyreq = titcLenAiInTckeyreq;
  regCachePtr->currReclenAi = titcLenAiInTckeyreq;

  regTcPtr->apiConnect = TapiConnectptrIndex;
  regTcPtr->clientData = TsenderData;
  regTcPtr->commitAckMarker = RNIL;
  regTcPtr->m_special_op_flags = Tspecial_op_flags;
  regTcPtr->indexOp = regApiPtr->executingIndexOp;
  regTcPtr->savePointId = regApiPtr->currSavePointId;
  regApiPtr->executingIndexOp = RNIL;

  regApiPtr->singleUserMode |= 1 << localTabptr.p->singleUserMode;

  if (isExecutingTrigger)
  {
    // Save the TcOperationPtr for fireing operation
    ndbrequire(TsenderData != RNIL);
    regTcPtr->triggeringOperation = TsenderData;
    // ndbrequire(hasOp(apiConnectptr, TsenderData));

    // Grab trigger Id from ApiConnectRecord
    ndbrequire(regApiPtr->immediateTriggerId != RNIL);
    regTcPtr->currentTriggerId= regApiPtr->immediateTriggerId;

    Ptr<TcDefinedTriggerData> trigPtr;
    c_theDefinedTriggers.getPtr(trigPtr, regTcPtr->currentTriggerId);
    trigPtr.p->refCount++;
  }

  ndbassert(isExecutingTrigger || 
            (regApiPtr->immediateTriggerId == RNIL));

  if (TexecFlag){
    Uint32 currSPId = regApiPtr->currSavePointId;
    regApiPtr->currSavePointId = ++currSPId;
  }

  regCachePtr->attrlength = TattrLen;
  c_counters.cattrinfoCount += TattrLen;

  UintR TtabptrIndex = localTabptr.i;
  UintR TtableSchemaVersion = tcKeyReq->tableSchemaVersion;
  Uint8 TOperationType = TcKeyReq::getOperationType(Treqinfo);
  regCachePtr->tableref = TtabptrIndex;
  regCachePtr->schemaVersion = TtableSchemaVersion;
  regTcPtr->operation = TOperationType;

  Uint8 TSimpleFlag         = TcKeyReq::getSimpleFlag(Treqinfo);
  Uint8 TDirtyFlag          = TcKeyReq::getDirtyFlag(Treqinfo);
  Uint8 TInterpretedFlag    = TcKeyReq::getInterpretedFlag(Treqinfo);
  Uint8 TDistrKeyFlag       = TcKeyReq::getDistributionKeyFlag(Treqinfo);
  Uint8 TNoDiskFlag         = TcKeyReq::getNoDiskFlag(Treqinfo);

  regTcPtr->dirtyOp  = TDirtyFlag;
  regTcPtr->opSimple = TSimpleFlag;
  regCachePtr->opExec   = TInterpretedFlag;
  regCachePtr->distributionKeyIndicator = TDistrKeyFlag;
  regCachePtr->m_no_disk_flag = TNoDiskFlag;

  Uint8 TexecuteFlag        = TexecFlag;
  Uint8 Treorg              = TcKeyReq::getReorgFlag(Treqinfo);
  if (Treorg)
  {
    if (TOperationType == ZWRITE)
      regTcPtr->m_special_op_flags = TcConnectRecord::SOF_REORG_COPY;
    else if (TOperationType == ZDELETE)
      regTcPtr->m_special_op_flags = TcConnectRecord::SOF_REORG_DELETE;
    else
    {
      ndbassert(false);
    }
  }
  
  const Uint8 TViaSPJFlag   = TcKeyReq::getViaSPJFlag(Treqinfo);
  const Uint8 Tqueue        = TcKeyReq::getQueueOnRedoProblemFlag(Treqinfo);

  regCachePtr->viaSPJFlag = TViaSPJFlag;
  regCachePtr->m_op_queue = Tqueue;

  //-------------------------------------------------------------
  // The next step is to read the upto three conditional words.
  //-------------------------------------------------------------
  Uint32 TkeyIndex;
  Uint32* TOptionalDataPtr = (Uint32*)&tcKeyReq->scanInfo;
  {
    Uint32  TDistrGHIndex    = TcKeyReq::getScanIndFlag(Treqinfo);
    Uint32  TDistrKeyIndex   = TDistrGHIndex;

    Uint32 TscanInfo = TcKeyReq::getTakeOverScanInfo(TOptionalDataPtr[0]);

    regCachePtr->scanTakeOverInd = TDistrGHIndex;
    regCachePtr->scanInfo = TscanInfo;

    regCachePtr->distributionKey = TOptionalDataPtr[TDistrKeyIndex];

    TkeyIndex = TDistrKeyIndex + TDistrKeyFlag;
  }

  regCachePtr->m_no_hash = false;

  if (TOperationType == ZUNLOCK)
  {
    /* Unlock op has distribution key containing
     * LQH nodeid and fragid
     */
    ndbassert( regCachePtr->distributionKeyIndicator );
    regCachePtr->m_no_hash = 1;
    regCachePtr->unlockNodeId = (regCachePtr->distributionKey >> 16);
    regCachePtr->distributionKey &= 0xffff;
  }
  
  regCachePtr->m_special_hash = 
    localTabptr.p->hasCharAttr | 
    (localTabptr.p->noOfDistrKeys > 0) |
    regCachePtr->m_no_hash;

  if (TkeyLength == 0)
  {
    releaseSections(handle);
    TCKEY_abort(signal, 4);
    return;
  }
  
  if (unlikely(TViaSPJFlag &&
               /* Check that all nodes can handle SPJ requests. */
               !ndbd_join_pushdown(getNodeVersionInfo().m_type[NodeInfo::DB]
                                   .m_min_version)))
  {
    jam();
    releaseSections(handle);
    TCKEY_abort(signal, 66);
    return;
  }

  /* KeyInfo and AttrInfo are buffered in segmented sections
   * If they arrived in segmented sections then there's nothing to do
   * If they arrived in short signals then they are appended into
   * segmented sections
   */
  if (regCachePtr->isLongTcKeyReq)
  {
    ndbassert( titcLenAiInTckeyreq == 0);
    /* Long TcKeyReq - KI and AI already in sections */
    SegmentedSectionPtr keyInfoSection, attrInfoSection;

    /* Store i value for first long section of KeyInfo
     * and AttrInfo in Cache Record
     */
    handle.getSection(keyInfoSection,
                      TcKeyReq::KeyInfoSectionNum);

    regCachePtr->keyInfoSectionI= keyInfoSection.i;
  
    if (regCachePtr->attrlength != 0)
    {
      ndbassert( handle.m_cnt == 2 );
      handle.getSection(attrInfoSection,
                        TcKeyReq::AttrInfoSectionNum);
      regCachePtr->attrInfoSectionI= attrInfoSection.i;
    }
    else
    {
      ndbassert( handle.m_cnt == 1 );
    }

    /* Detach sections from the handle, we are now responsible
     * for always freeing them before returning
     * For a long TcKeyReq, they will be freed at the end
     * of the processing this signal.
     */
    handle.clear();
  }
  else
  {
    /* Short TcKeyReq - need to receive KI and AI into 
     * segmented sections
     * We store any KI and AI from the TCKeyReq now and
     * will then wait for further signals if necessary
     */
    ndbassert( handle.m_cnt == 0 );
    Uint32 keyInfoInTCKeyReq= MIN(TkeyLength, TcKeyReq::MaxKeyInfo);

    bool ok= appendToSection(regCachePtr->keyInfoSectionI,
                             &TOptionalDataPtr[TkeyIndex],
                             keyInfoInTCKeyReq);
    if (!ok)
    {
      jam();
      appendToSectionErrorLab(signal);
      return;
    }
                   
    if (titcLenAiInTckeyreq != 0)
    {
      Uint32 TAIDataIndex= TkeyIndex + keyInfoInTCKeyReq;

      ok= appendToSection(regCachePtr->attrInfoSectionI,
                          &TOptionalDataPtr[TAIDataIndex],
                          titcLenAiInTckeyreq);
      if (!ok)
      {
        jam();
        appendToSectionErrorLab(signal);
        return;
      }
    }
  }

  if (TOperationType == ZUNLOCK)
  {
    jam();
    // TODO : Consider adding counter for unlock operations
  }
  else if (TOperationType == ZREAD || TOperationType == ZREAD_EX) {
    jam();
    c_counters.creadCount++;
  }
  else
  {
    /**
     * Insert, Update, Write, Delete
     * Markers are created on all nodes in a nodegroup 
     * (as changes affect all replicas)
     * Therefore any survivable node failure will have a live
     * node with a marker for any marked write transaction.
     * Therefore only need one nodegroup marked - after that
     * needn't bother.
     */
    if (!tc_testbit(regApiPtr->m_flags,
                    ApiConnectRecord::TF_COMMIT_ACK_MARKER_RECEIVED))
    {
      if(regApiPtr->commitAckMarker != RNIL)
        regTcPtr->commitAckMarker = regApiPtr->commitAckMarker;
      else
      {
        jam();
        CommitAckMarkerPtr tmp;
        if (ERROR_INSERTED(8087))
        {
          CLEAR_ERROR_INSERT_VALUE;
          TCKEY_abort(signal, 56);
          return;
        }

        if (!m_commitAckMarkerHash.seize(tmp))
        {
          TCKEY_abort(signal, 56);
          return;
        }
        else
        {
          regTcPtr->commitAckMarker = tmp.i;
          regApiPtr->commitAckMarker = tmp.i;
          new (tmp.p) CommitAckMarker();
          tmp.p->transid1      = tcKeyReq->transId1;
          tmp.p->transid2      = tcKeyReq->transId2;
          tmp.p->apiNodeId     = refToNode(regApiPtr->ndbapiBlockref);
          tmp.p->apiConnectPtr = TapiIndex;
#if defined VM_TRACE || defined ERROR_INSERT
	  {
	    CommitAckMarkerPtr check;
	    ndbrequire(!m_commitAckMarkerHash.find(check, *tmp.p));
          }
#endif
          m_commitAckMarkerHash.add(tmp);
        }
      }
      regApiPtr->num_commit_ack_markers++;
    }
    
    UintR Toperationsize = coperationsize;
    /* -------------------------------------------------------------------- 
     *   THIS IS A TEMPORARY TABLE, DON'T UPDATE coperationsize. 
     *   THIS VARIABLE CONTROLS THE INTERVAL BETWEEN LCP'S AND 
     *   TEMP TABLES DON'T PARTICIPATE.
     * -------------------------------------------------------------------- */
    if (localTabptr.p->get_storedTable()) {
      coperationsize = ((Toperationsize + TattrLen) + TkeyLength) + 17;
    }
    c_counters.cwriteCount++;
    switch (TOperationType) {
    case ZUPDATE:
    case ZINSERT:
    case ZDELETE:
    case ZWRITE:
    case ZREFRESH:
      jam();
      regApiPtr->m_write_count++;
      if (regApiPtr->m_flags & ApiConnectRecord::TF_DEFERRED_CONSTRAINTS)
      {
        /**
         * Allow slave applier to ignore m_max_writes_per_trans
         */
        break;
      }

      if (unlikely(regApiPtr->m_write_count > m_max_writes_per_trans))
      {
        TCKEY_abort(signal, 65);
        return;
      }
      break;
    default:
      TCKEY_abort(signal, 9);
      return;
    }//switch
  }//if
  
  Uint32 TabortOption = TcKeyReq::getAbortOption(Treqinfo);
  regTcPtr->m_execAbortOption = TabortOption;
  
  /*-------------------------------------------------------------------------
   * Check error handling per operation
   * If CommitFlag is set state accordingly and check for early abort
   *------------------------------------------------------------------------*/
  if (TcKeyReq::getCommitFlag(Treqinfo) == 1) {
    ndbrequire(TexecuteFlag);
    regApiPtr->apiConnectstate = CS_REC_COMMITTING;
  } else {
    /* ---------------------------------------------------------------------
     *       PREPARE TRANSACTION IS NOT IMPLEMENTED YET.
     * ---------------------------------------------------------------------
     *       ELSIF (TREQINFO => 3) (*) 1 = 1 THEN                
     * IF PREPARE TRANSACTION THEN
     *   API_CONNECTPTR:API_CONNECTSTATE = REC_PREPARING
     * SET STATE TO PREPARING
     * --------------------------------------------------------------------- */
    if (regApiPtr->apiConnectstate == CS_START_COMMITTING) {
      jam();
      // Trigger execution at commit
      regApiPtr->apiConnectstate = CS_REC_COMMITTING;
    } else if (!regApiPtr->isExecutingDeferredTriggers()) {
      jam();
      regApiPtr->apiConnectstate = CS_RECEIVING;
    }//if
  }//if

  if (regCachePtr->isLongTcKeyReq) 
  {
    jamDebug();
    /* Have all the KeyInfo (and AttrInfo), process now */
    tckeyreq050Lab(signal);
  } 
  else if (TkeyLength <= TcKeyReq::MaxKeyInfo) 
  {
    jam();
    /* Have all the KeyInfo, get any extra AttrInfo */
    tckeyreq050Lab(signal);
  }
  else 
  {
    jam();
    /* --------------------------------------------------------------------
     * THE TCKEYREQ DIDN'T CONTAIN ALL KEY DATA, 
     * SAVE STATE AND WAIT FOR KEYINFO 
     * --------------------------------------------------------------------*/
    setApiConTimer(apiConnectptr.i, ctcTimer, __LINE__);
    regCachePtr->save1 = 8;
    regTcPtr->tcConnectstate = OS_WAIT_KEYINFO;
    return;
  }//if
  
  return;
}//Dbtc::execTCKEYREQ()

static
void
handle_reorg_trigger(DiGetNodesConf * conf)
{
  if (conf->reqinfo & DiGetNodesConf::REORG_MOVING)
  {
    conf->fragId = conf->nodes[MAX_REPLICAS];
    conf->reqinfo = conf->nodes[MAX_REPLICAS+1];
    memcpy(conf->nodes, conf->nodes+MAX_REPLICAS+2,
           sizeof(Uint32)*MAX_REPLICAS);
  }
  else
  {
    conf->nodes[0] = 0; // Should not execute...
  }
}

bool
Dbtc::isRefreshSupported() const
{
  const NodeVersionInfo& nvi = getNodeVersionInfo();
  const Uint32 minVer = nvi.m_type[NodeInfo::DB].m_min_version;
  const Uint32 maxVer = nvi.m_type[NodeInfo::DB].m_max_version;

  if (likely (minVer == maxVer))
  {
    /* Normal case, use function */
    return ndb_refresh_tuple(minVer);
  }

  /* As refresh feature was introduced across three minor versions
   * we check that all data nodes support it.  This slow path
   * should only be hit during upgrades between versions
   */
  for (Uint32 i=1; i < MAX_NODES; i++)
  {
    const NodeInfo& nodeInfo = getNodeInfo(i);
    if ((nodeInfo.m_type == NODE_TYPE_DB) &&
        (nodeInfo.m_connected) &&
        (! ndb_refresh_tuple(nodeInfo.m_version)))
      return false;
  }
  return true;
}

/**
 * This method comes in with a list of nodes.
 * We have already verified that our own node
 * isn't in this list. If we have a node in this
 * list that is in the same location domain as
 * this node, it will be selected before any
 * other node. So we will always try to keep
 * the read coming from the same location domain.
 *
 * To avoid radical imbalances we provide a bit
 * of round robin on a node bases. It isn't
 * any perfect round robin. We simply rotate a
 * bit among the selected nodes instead of
 * always selecting the first one we find.
 */
Uint32
Dbtc::check_own_location_domain(Uint16 *nodes,
                                Uint32 end)
{
  Uint32 loc_nodes[MAX_NDB_NODES];
  Uint32 loc_node_count = 0;
  Uint32 my_location_domain_id = m_my_location_domain_id;

  if (my_location_domain_id == 0)
  {
    jam();
    return 0;
  }
  for (Uint32 i = 0; i < end; i++)
  {
    jam();
    Uint32 node = nodes[i];
    HostRecordPtr Tnode_hostptr;
    Tnode_hostptr.i = node;
    ptrCheckGuard(Tnode_hostptr, chostFilesize, hostRecord);
    if (my_location_domain_id ==
        Tnode_hostptr.p->m_location_domain_id)
    {
      jam();
      loc_nodes[loc_node_count++] = node;
    }
  }
  if (loc_node_count > 0)
  {
    return loc_nodes[m_load_balancer_location++ % loc_node_count];
  }
  return 0;
}

/**
 * tckeyreq050Lab
 * This method is executed once all KeyInfo has been obtained for
 * the TcKeyReq signal
 */
void Dbtc::tckeyreq050Lab(Signal* signal) 
{
  UintR tnoOfBackup;
  UintR tnoOfStandby;

  terrorCode = 0;

  hash(signal); /* NOW IT IS TIME TO CALCULATE THE HASH VALUE*/
  
  CacheRecord * const regCachePtr = cachePtr.p;
  TcConnectRecord * const regTcPtr = tcConnectptr.p;
  ApiConnectRecord * const regApiPtr = apiConnectptr.p;

  UintR TtcTimer = ctcTimer;
  UintR ThashValue = thashValue;
  UintR TdistrHashValue = tdistrHashValue;
  UintR Ttableref = regCachePtr->tableref;
  Uint16 Tspecial_op_flags = regTcPtr->m_special_op_flags;

  TableRecordPtr localTabptr;
  localTabptr.i = Ttableref;
  localTabptr.p = &tableRecord[localTabptr.i];
  Uint32 schemaVersion = regCachePtr->schemaVersion;
  if(localTabptr.p->checkTable(schemaVersion)){
    ;
  } else {
    terrorCode = localTabptr.p->getErrorCode(schemaVersion);
    TCKEY_abort(signal, 58);
    return;
  }

  setApiConTimer(apiConnectptr.i, TtcTimer, __LINE__);
  regCachePtr->hashValue = ThashValue;

  ndbassert( signal->getNoOfSections() == 0 );

  DiGetNodesReq * const req = (DiGetNodesReq *)&signal->theData[0];
  req->tableId = Ttableref;
  req->hashValue = TdistrHashValue;
  req->distr_key_indicator = regCachePtr->distributionKeyIndicator;
  req->scan_indicator = 0;
  req->anyNode = 0;
  req->get_next_fragid_indicator = 0;
  req->jamBufferPtr = jamBuffer();

  if (localTabptr.p->m_flags & TableRecord::TR_FULLY_REPLICATED)
  {
    if (regTcPtr->operation == ZREAD)
    {
      /**
       * inform DIH if TreadAny is even relevent...so it does not have to loop
       *   unless really needing to...
       */
      if (regTcPtr->dirtyOp || regTcPtr->opSimple ||
          regCachePtr->m_read_committed_base)
      {
        jam();
        req->anyNode = 1;
      }
    }
    else if (Tspecial_op_flags & TcConnectRecord::SOF_REORG_COPY)
    {
      /**
       * We want DIH to return the first new fragment, this is a copy
       * of the row, it has been read from a main fragment and needs
       * to be copied to the new fragments. We get the first new
       * fragment and trust the fully replicated triggers to ensure
       * that any more new fragments are also written using the
       * iterator on the fully replicated trigger.
       */
      jam();
      regTcPtr->m_special_op_flags &= ~TcConnectRecord::SOF_REORG_COPY;
      req->anyNode = 2;
    }
    else if (Tspecial_op_flags & TcConnectRecord::SOF_FULLY_REPLICATED_TRIGGER)
    {
      jam();
      req->anyNode = 3;
    }
  }

  /*-------------------------------------------------------------*/
  /* FOR EFFICIENCY REASONS WE AVOID THE SIGNAL SENDING HERE AND */
  /* PROCEED IMMEDIATELY TO DIH. IN MULTI-THREADED VERSIONS WE   */
  /* HAVE TO INSERT A MUTEX ON DIH TO ENSURE PROPER OPERATION.   */
  /* SINCE THIS SIGNAL AND DIVERIFYREQ ARE THE ONLY SIGNALS SENT */
  /* TO DIH IN TRAFFIC IT SHOULD BE OK (3% OF THE EXECUTION TIME */
  /* IS SPENT IN DIH AND EVEN LESS IN REPLICATED NDB.            */
  /*-------------------------------------------------------------*/
  EXECUTE_DIRECT_MT(DBDIH, GSN_DIGETNODESREQ, signal,
                    DiGetNodesReq::SignalLength, 0);
  DiGetNodesConf * conf = (DiGetNodesConf *)&signal->theData[0];
  UintR Tdata2 = conf->reqinfo;
  UintR TerrorIndicator = signal->theData[0];
  jamEntry();
  if (TerrorIndicator != 0) {
    execDIGETNODESREF(signal);
    return;
  }
  
  /****************>>*/
  /* DIGETNODESCONF >*/
  /* ***************>*/
  Tspecial_op_flags = regTcPtr->m_special_op_flags;
  if (Tspecial_op_flags & TcConnectRecord::SOF_REORG_TRIGGER)
  {
    if (conf->reqinfo & DiGetNodesConf::REORG_MOVING &&
        (conf->nodes[MAX_REPLICAS] == regApiPtr->firedFragId))
    {
      jam();
      /**
       * The new fragment id is what we already written. This can happen
       * if the change of table distribution between the first write and
       * the triggered write that comes here. We will swap and instead
       * write the now original fragment which is really the new
       * fragment.
       */
      Tdata2 = conf->reqinfo & (~DiGetNodesConf::REORG_MOVING);
    }
    else
    {
      jam();
      handle_reorg_trigger(conf);
      Tdata2 = conf->reqinfo;
    }
    /**
     * Ensure that firedFragId is restored to RNIL to ensure it has a defined
     * value when not used.
     */
    regApiPtr->firedFragId = RNIL;
  }
  else if (Tspecial_op_flags & TcConnectRecord::SOF_REORG_DELETE)
  {
    jam();
    handle_reorg_trigger(conf);
    Tdata2 = conf->reqinfo;
  }
  else if (Tdata2 & DiGetNodesConf::REORG_MOVING)
  {
    jam();
    regTcPtr->m_special_op_flags |= TcConnectRecord::SOF_REORG_MOVING;
  }
  else if (Tspecial_op_flags & TcConnectRecord::SOF_REORG_COPY)
  {
    jam();
    conf->nodes[0] = 0;
  }

  UintR Tdata1 = conf->fragId;
  UintR Tdata3 = conf->nodes[0];
  UintR Tdata4 = conf->nodes[1];
  UintR Tdata5 = conf->nodes[2];
  UintR Tdata6 = conf->nodes[3];

  regCachePtr->fragmentid = Tdata1;
  Uint32 tnodeinfo = Tdata2;

  regTcPtr->tcNodedata[0] = Tdata3;
  regTcPtr->tcNodedata[1] = Tdata4;
  regTcPtr->tcNodedata[2] = Tdata5;
  regTcPtr->tcNodedata[3] = Tdata6;

  regTcPtr->lqhInstanceKey = (Tdata2 >> 24) & 127;// 1 bit used for reorg moving

  tnoOfBackup = tnodeinfo & 3;
  tnoOfStandby = (tnodeinfo >> 8) & 3;
  Uint8 Toperation = regTcPtr->operation;

  regCachePtr->fragmentDistributionKey = (tnodeinfo >> 16) & 255;
  Uint32 TreadBackup = (localTabptr.p->m_flags & TableRecord::TR_READ_BACKUP);
  if (Toperation == ZREAD || Toperation == ZREAD_EX)
  {
    Uint8 TopSimple = regTcPtr->opSimple;
    Uint8 TopDirty = regTcPtr->dirtyOp;

    regTcPtr->m_special_op_flags &= ~TcConnectRecord::SOF_REORG_MOVING;
    /**
     * Allow reading from backup replica...if
     * 1) Simple-read, since this will wait in lock queue for any pending
     *    commit/complete
     * 2) Simple/CommittedRead if TreadBackup-table property is set
     *    (since transactions updating such table will wait with sending API
     *    commit until complete-phase has been run)
     * 3) Locking reads that have signalled that they have had their locks
     *    upgraded due to being read of a base table of BLOB table or
     *    reads of unique key for Committed reads.
     */
    if ((TopSimple != 0 && TopDirty == 0) ||
        (TreadBackup != 0 && (TopSimple != 0 || TopDirty != 0)) ||
        (TreadBackup != 0 && regCachePtr->m_read_committed_base))
    {
      jam();
      /*-------------------------------------------------------------*/
      /*       A SIMPLE READ CAN SELECT ANY OF THE PRIMARY AND       */
      /*       BACKUP NODES TO READ. WE WILL TRY TO SELECT THIS      */
      /*       NODE IF POSSIBLE TO AVOID UNNECESSARY COMMUNICATION   */
      /*       WITH SIMPLE READS.                                    */
      /*-------------------------------------------------------------*/
      arrGuard(tnoOfBackup, MAX_REPLICAS);
      UintR Tindex;
      UintR TownNode = cownNodeid;
      bool foundOwnNode = false;
      for (Tindex = 1; Tindex <= tnoOfBackup; Tindex++) {
        UintR Tnode = regTcPtr->tcNodedata[Tindex];
        jam();
        if (Tnode == TownNode) {
          jam();
          regTcPtr->tcNodedata[0] = Tnode;
          foundOwnNode = true;
        }//if
      }//for
      if (!foundOwnNode)
      {
        Uint32 node;
        jam();
        if ((node = check_own_location_domain(&regTcPtr->tcNodedata[0],
                                              tnoOfBackup+1)) != 0)
        {
          regTcPtr->tcNodedata[0] = node;
        }
      }
      if(ERROR_INSERTED(8048) || ERROR_INSERTED(8049))
      {
	for (Tindex = 0; Tindex <= tnoOfBackup; Tindex++) 
	{
	  UintR Tnode = regTcPtr->tcNodedata[Tindex];
	  jam();
	  if (Tnode != TownNode) {
	    jam();
	    regTcPtr->tcNodedata[0] = Tnode;
	    ndbout_c("Choosing %d", Tnode);
	  }//if
	}//for
      }
    }//if
    jam();
    regTcPtr->lastReplicaNo = 0;
    regTcPtr->noOfNodes = 1;

    if (regTcPtr->tcNodedata[0] == getOwnNodeId())
      c_counters.clocalReadCount++;
#ifdef ERROR_INSERT
    else if (ERROR_INSERTED(8083))
    {
      ndbabort();  // Only node-local reads
    }
#endif
  }
  else if (Toperation == ZUNLOCK)
  {
    regTcPtr->m_special_op_flags &= ~TcConnectRecord::SOF_REORG_MOVING;
   
    const Uint32 numNodes = tnoOfBackup + 1;
    /* Check that node from dist key is one of the nodes returned */
    bool found = false;
    for (Uint32 idx = 0; idx < numNodes; idx ++)
    {
      NodeId nodeId = regTcPtr->tcNodedata[ idx ];
      jam();
      if (nodeId == regCachePtr->unlockNodeId)
      {
        jam();
        found = true;
        break;
      }
    }

    if (unlikely(!found))
    {
      /* DIH says the specified node does not store the fragment
       * requested
       */
      jam();
      TCKEY_abort(signal, 64);
      return;
    }

    /* Check that the relevant LQH node can handle an unlock request */
    Uint32 lqhVersion = getNodeInfo(regCachePtr->unlockNodeId).m_version;
    
    if (unlikely( lqhVersion < NDBD_UNLOCK_OP_SUPPORTED ))
    {
      TCKEY_abort(signal, 63);
      return;
    }

    /* Select the specified node for the unlock op */ 
    regTcPtr->tcNodedata[0] = regCachePtr->unlockNodeId;
    regTcPtr->lastReplicaNo = 0;
    regTcPtr->noOfNodes = 1;
  }
  else
  {
    UintR TlastReplicaNo;
    jam();
    TlastReplicaNo = tnoOfBackup + tnoOfStandby;
    regTcPtr->lastReplicaNo = (Uint8)TlastReplicaNo;
    regTcPtr->noOfNodes = (Uint8)(TlastReplicaNo + 1);
    if (regTcPtr->tcNodedata[0] == getOwnNodeId())
      c_counters.clocalWriteCount++;

    if ((localTabptr.p->m_flags & TableRecord::TR_DELAY_COMMIT) != 0)
    {
      jam();
      /**
       * Set TF_LATE_COMMIT transaction property if INS/UPD/DEL is performed
       *   when running with readBackup property set.
       *
       * This causes api-commit to be sent after complete-phase has been run
       * (instead of when commit phase has run, but complete-phase has not yet
       * started)
       */
      regApiPtr->m_flags |= ApiConnectRecord::TF_LATE_COMMIT;
    }

    if (unlikely((Toperation == ZREFRESH) &&
                 (! isRefreshSupported())))
    {
      /* Function not implemented yet */
      TCKEY_abort(signal,63);
      return;
    }
  }//if

  if((ERROR_INSERTED(8071) || ERROR_INSERTED(8072)) &&
     (regTcPtr->m_special_op_flags & TcConnectRecord::SOF_INDEX_TABLE_READ) &&
     regTcPtr->tcNodedata[0] != getOwnNodeId())
  {
    ndbassert(false);
    signal->theData[1] = 626;
    execDIGETNODESREF(signal);
    return;
  }

  if((ERROR_INSERTED(8050) || ERROR_INSERTED(8072)) &&
     refToBlock(regApiPtr->ndbapiBlockref) != DBUTIL &&
     regTcPtr->m_special_op_flags == 0 &&
     (regTcPtr->tcNodedata[0] != getOwnNodeId()))
  {
    ndbassert(false);
    signal->theData[1] = 626;
    execDIGETNODESREF(signal);
    return;
  }

  if (regCachePtr->isLongTcKeyReq || 
      (regCachePtr->lenAiInTckeyreq == regCachePtr->attrlength)) {
    /****************************************************************>*/
    /* HERE WE HAVE FOUND THAT THE LAST SIGNAL BELONGING TO THIS      */
    /* OPERATION HAVE BEEN RECEIVED. THIS MEANS THAT WE CAN NOW REUSE */
    /* THE API CONNECT RECORD. HOWEVER IF PREPARE OR COMMIT HAVE BEEN */
    /* RECEIVED THEN IT IS NOT ALLOWED TO RECEIVE ANY FURTHER         */
    /* OPERATIONS. WE KNOW THAT WE WILL WAIT FOR DICT NEXT. IT IS NOT */
    /* POSSIBLE FOR THE TC CONNECTION TO BE READY YET.                */
    /****************************************************************>*/
    switch (regApiPtr->apiConnectstate) {
    case CS_RECEIVING:
      jam();
      regApiPtr->apiConnectstate = CS_STARTED;
      break;
    case CS_REC_COMMITTING:
      jam();
      regApiPtr->apiConnectstate = CS_START_COMMITTING;
      break;
    case CS_SEND_FIRE_TRIG_REQ:
    case CS_WAIT_FIRE_TRIG_REQ:
      jam();
      break;
    default:
      jam();
      systemErrorLab(signal, __LINE__);
      return;
    }//switch
    attrinfoDihReceivedLab(signal);
    return;
  } else {
    if (regCachePtr->lenAiInTckeyreq < regCachePtr->attrlength) {
      TtcTimer = ctcTimer;
      jam();
      setApiConTimer(apiConnectptr.i, TtcTimer, __LINE__);
      regTcPtr->tcConnectstate = OS_WAIT_ATTR;
      return;
    } else {
      TCKEY_abort(signal, 11);
      return;
    }//if
  }//if
  return;
}//Dbtc::tckeyreq050Lab()

void Dbtc::attrinfoDihReceivedLab(Signal* signal)
{
  CacheRecord * const regCachePtr = cachePtr.p;
  TcConnectRecord * const regTcPtr = tcConnectptr.p;
  ApiConnectRecord * const regApiPtr = apiConnectptr.p;
  Uint16 Tnode = regTcPtr->tcNodedata[0];

  TableRecordPtr localTabptr;
  localTabptr.i = regCachePtr->tableref;
  localTabptr.p = &tableRecord[localTabptr.i];

  if(localTabptr.p->checkTable(regCachePtr->schemaVersion)){
    ;
  } else {
    terrorCode = localTabptr.p->getErrorCode(regCachePtr->schemaVersion);
    TCKEY_abort(signal, 58);
    return;
  }
  if (Tnode != 0)
  {
    jam();
    arrGuard(Tnode, MAX_NDB_NODES);
    BlockReference lqhRef;
    if(regCachePtr->viaSPJFlag)
    {
      if (Tnode == getOwnNodeId())
      {
        //Node local req; send to SPJ-instance in own block-thread
        lqhRef = numberToRef(DBSPJ, instance(), Tnode);
      }
      else
      {
        /* Round robin the SPJ requests:
         *
         * Note that our protocol allows non existing block instances
         * to be adressed, in which case the receiver will modulo fold
         * among the existing SPJ instances.
         * Distribute among 120 'virtual' SPJ blocks as this is dividable
         * by most commom number of SPJ blocks (1,2,3,4,5,6,8,10...)
         */
        const Uint32 blockInstance = (cspjInstanceRR++ % 120) + 1;
        lqhRef = numberToRef(DBSPJ, blockInstance, Tnode);
      }
    }
    else
    {
      const Uint32 instanceKey = regTcPtr->lqhInstanceKey;
      lqhRef = numberToRef(DBLQH, instanceKey, Tnode);
    }
    packLqhkeyreq(signal, lqhRef);
  }
  else
  {
    /**
     * 1) This is when a reorg trigger fired...
     *   but the tuple should *not* move
     *   This should be prevented using the LqhKeyReq::setReorgFlag
     *
     * 2) This also happens during reorg copy, when a row should *not* be moved
     */
    jam();
    Uint32 trigOp = regTcPtr->triggeringOperation;
    Uint32 TclientData = regTcPtr->clientData;
    releaseKeys();
    releaseAttrinfo();
    regApiPtr->lqhkeyreqrec--;
    unlinkReadyTcCon(signal);
    clearCommitAckMarker(regApiPtr, regTcPtr);
    releaseTcCon();

    if (trigOp != RNIL)
    {
      jam();
      //ndbassert(false); // see above
      TcConnectRecordPtr opPtr;
      opPtr.i = trigOp;
      ptrCheckGuard(opPtr, ctcConnectFilesize, tcConnectRecord);
      trigger_op_finished(signal, apiConnectptr, RNIL, opPtr.p, 0);
      return;
    }
    else
    {
      jam();
      Uint32 Ttckeyrec = regApiPtr->tckeyrec;
      regApiPtr->tcSendArray[Ttckeyrec] = TclientData;
      regApiPtr->tcSendArray[Ttckeyrec + 1] = 0;
      regApiPtr->tckeyrec = Ttckeyrec + 2;
      lqhKeyConf_checkTransactionState(signal, apiConnectptr);
    }
  }
}//Dbtc::attrinfoDihReceivedLab()

void Dbtc::packLqhkeyreq(Signal* signal,
                         BlockReference TBRef)
{
  CacheRecord * const regCachePtr = cachePtr.p;
  UintR Tkeylen = regCachePtr->keylen;

  ndbassert( signal->getNoOfSections() == 0 );

  sendlqhkeyreq(signal, TBRef);

  /* Do we need to send a KeyInfo signal train? */
  if ((! regCachePtr->useLongLqhKeyReq) &&
      (Tkeylen > LqhKeyReq::MaxKeyInfo)) 
  {
    /* Build KeyInfo train from KeyInfo long signal section */
    sendKeyInfoTrain(signal, 
                     TBRef,
                     tcConnectptr.i,
                     LqhKeyReq::MaxKeyInfo,
                     regCachePtr->keyInfoSectionI);
  }//if

  /* Release key storage */ 
  releaseKeys();
  packLqhkeyreq040Lab(signal,
                      TBRef);
}//Dbtc::packLqhkeyreq()


void Dbtc::sendlqhkeyreq(Signal* signal,
                         BlockReference TBRef)
{
  UintR tslrAttrLen;
  UintR Tdata10;
  TcConnectRecord * const regTcPtr = tcConnectptr.p;
  ApiConnectRecord * const regApiPtr = apiConnectptr.p;
  CacheRecord * const regCachePtr = cachePtr.p;
  Uint32 version = getNodeInfo(refToNode(TBRef)).m_version;
  UintR sig0, sig1, sig2, sig3, sig4, sig5, sig6;
#ifdef ERROR_INSERT
  if (ERROR_INSERTED(8002)) {
    systemErrorLab(signal, __LINE__);
  }//if
  if (ERROR_INSERTED(8007)) {
    if (apiConnectptr.p->apiConnectstate == CS_STARTED) {
      CLEAR_ERROR_INSERT_VALUE;
      return;
    }//if
  }//if
  if (ERROR_INSERTED(8008)) {
    if (apiConnectptr.p->apiConnectstate == CS_START_COMMITTING) {
      CLEAR_ERROR_INSERT_VALUE;
      return;
    }//if
  }//if
  if (ERROR_INSERTED(8009)) {
    if (apiConnectptr.p->apiConnectstate == CS_STARTED) {
      return;
    }//if
  }//if
  if (ERROR_INSERTED(8010)) {
    if (apiConnectptr.p->apiConnectstate == CS_START_COMMITTING) {
      return;
    }//if
  }//if
#endif
  Uint32 Tdeferred = tc_testbit(regApiPtr->m_flags,
                                ApiConnectRecord::TF_DEFERRED_CONSTRAINTS);
  Uint32 Tdisable_fk = tc_testbit(regApiPtr->m_flags,
                                  ApiConnectRecord::TF_DISABLE_FK_CONSTRAINTS);
  Uint32 reorg = ScanFragReq::REORG_ALL;
  Uint32 Tspecial_op = regTcPtr->m_special_op_flags;
  if (Tspecial_op == 0)
  {
  }
  else if (Tspecial_op & (TcConnectRecord::SOF_REORG_TRIGGER |
                          TcConnectRecord::SOF_REORG_DELETE))
  {
    reorg = ScanFragReq::REORG_NOT_MOVED;
  }
  else if (Tspecial_op & TcConnectRecord::SOF_REORG_MOVING)
  {
    reorg = ScanFragReq::REORG_MOVED;
  }

  Uint32 inlineKeyLen= 0;
  Uint32 inlineAttrLen= 0;

  /* We normally send long LQHKEYREQ unless the
   * destination cannot handle it or we are 
   * testing
   */
  if (unlikely((version < NDBD_LONG_LQHKEYREQ) ||
               ERROR_INSERTED(8069)))
  {
    /* Short LQHKEYREQ, with some key/attr data inline */
    regCachePtr->useLongLqhKeyReq= 0;
    inlineKeyLen= regCachePtr->keylen;
    inlineAttrLen= regCachePtr->attrlength;
  }
  else
    /* Long LQHKEYREQ, with key/attr data in long sections */
    regCachePtr->useLongLqhKeyReq= 1;

  tslrAttrLen = 0;
  LqhKeyReq::setAttrLen(tslrAttrLen, inlineAttrLen);
  /* ---------------------------------------------------------------------- */
  // Bit16 == 0 since StoredProcedures are not yet supported.
  /* ---------------------------------------------------------------------- */
  LqhKeyReq::setDistributionKey(tslrAttrLen, regCachePtr->fragmentDistributionKey);
  LqhKeyReq::setScanTakeOverFlag(tslrAttrLen, regCachePtr->scanTakeOverInd);
  LqhKeyReq::setReorgFlag(tslrAttrLen, reorg);

  Tdata10 = 0;
  sig0 = regTcPtr->opSimple;
  sig1 = regTcPtr->operation;
  sig2 = regTcPtr->dirtyOp;
  bool dirtyRead = (sig1 == ZREAD && sig2 == ZTRUE);
  LqhKeyReq::setLastReplicaNo(Tdata10, regTcPtr->lastReplicaNo);
  if (unlikely(version < NDBD_ROWID_VERSION))
  {
    Uint32 op = regTcPtr->operation;
    Uint32 lock = (Operation_t) op == ZREAD_EX ?  ZUPDATE :
                                      (Operation_t) op == ZWRITE ?
                                      ZINSERT : (Operation_t) op;
    LqhKeyReq::setLockType(Tdata10, lock);
  }
  /* ---------------------------------------------------------------------- */
  // Indicate Application Reference is present in bit 15
  /* ---------------------------------------------------------------------- */
  LqhKeyReq::setApplicationAddressFlag(Tdata10, 1);
  LqhKeyReq::setDirtyFlag(Tdata10, sig2);
  LqhKeyReq::setInterpretedFlag(Tdata10, regCachePtr->opExec);
  LqhKeyReq::setSimpleFlag(Tdata10, sig0);
  LqhKeyReq::setOperation(Tdata10, sig1);
  LqhKeyReq::setNoDiskFlag(Tdata10, regCachePtr->m_no_disk_flag);
  LqhKeyReq::setQueueOnRedoProblemFlag(Tdata10, regCachePtr->m_op_queue);
  LqhKeyReq::setDeferredConstraints(Tdata10, (Tdeferred & m_deferred_enabled));
  LqhKeyReq::setDisableFkConstraints(Tdata10, Tdisable_fk);

  /* ----------------------------------------------------------------------- 
   * If we are sending a short LQHKEYREQ, then there will be some AttrInfo
   * in the LQHKEYREQ.
   * Work out how much we'll send
   * ----------------------------------------------------------------------- */
  UintR aiInLqhKeyReq= 0;

  if (regCachePtr->useLongLqhKeyReq)
  {
    LqhKeyReq::setNoTriggersFlag(Tdata10,
      !!(regTcPtr->m_special_op_flags &
         TcConnectRecord::SOF_FULLY_REPLICATED_TRIGGER));
  }
  else
  {
    /* Short LQHKEYREQ : 
     * Send max 5 words of AttrInfo in LQHKEYREQ 
     */
    aiInLqhKeyReq= MIN(LqhKeyReq::MaxAttrInfo, regCachePtr->attrlength);
    LqhKeyReq::setKeyLen(Tdata10, inlineKeyLen);
  }

  LqhKeyReq::setAIInLqhKeyReq(Tdata10, aiInLqhKeyReq);
  /* -----------------------------------------------------------------------
   * Bit 27 == 0 since TC record is the same as the client record.
   * Bit 28 == 0 since readLenAi can only be set after reading in LQH.
   * ----------------------------------------------------------------------- */
  LqhKeyReq::setMarkerFlag(Tdata10, regTcPtr->commitAckMarker != RNIL ? 1 : 0);
  
  if (regTcPtr->m_special_op_flags & TcConnectRecord::SOF_FK_READ_COMMITTED)
  {
    LqhKeyReq::setNormalProtocolFlag(Tdata10, 1);
    LqhKeyReq::setDirtyFlag(Tdata10, 1);
  }
  /* ************************************************************> */
  /* NO READ LENGTH SENT FROM TC. SEQUENTIAL NUMBER IS 1 AND IT    */
  /* IS SENT TO A PRIMARY NODE.                                    */
  /* ************************************************************> */

  LqhKeyReq * const lqhKeyReq = (LqhKeyReq *)signal->getDataPtrSend();

  sig0 = tcConnectptr.i;
  sig2 = regCachePtr->hashValue;
  sig4 = cownref;
  sig5 = regTcPtr->savePointId;

  lqhKeyReq->clientConnectPtr = sig0;
  lqhKeyReq->attrLen = tslrAttrLen;
  lqhKeyReq->hashValue = sig2;
  lqhKeyReq->requestInfo = Tdata10;
  lqhKeyReq->tcBlockref = sig4;
  lqhKeyReq->savePointId = sig5;

  sig0 = regCachePtr->tableref + ((regCachePtr->schemaVersion << 16) & 0xFFFF0000);
  sig1 = regCachePtr->fragmentid + (regTcPtr->tcNodedata[1] << 16);
  sig2 = regApiPtr->transid[0];
  sig3 = regApiPtr->transid[1];
  sig4 =
    (regTcPtr->m_special_op_flags & TcConnectRecord::SOF_INDEX_TABLE_READ) ?
    reference() : regApiPtr->ndbapiBlockref;
  sig5 = regTcPtr->clientData;
  sig6 = regCachePtr->scanInfo;

  if (! dirtyRead)
  {
    regApiPtr->m_transaction_nodes.set(regTcPtr->tcNodedata[0]);
    regApiPtr->m_transaction_nodes.set(regTcPtr->tcNodedata[1]);
    regApiPtr->m_transaction_nodes.set(regTcPtr->tcNodedata[2]);
    regApiPtr->m_transaction_nodes.set(regTcPtr->tcNodedata[3]);  
  }
  
  lqhKeyReq->tableSchemaVersion = sig0;
  lqhKeyReq->fragmentData = sig1;
  lqhKeyReq->transId1 = sig2;
  lqhKeyReq->transId2 = sig3;
  lqhKeyReq->scanInfo = sig6;

  lqhKeyReq->variableData[0] = sig4;
  lqhKeyReq->variableData[1] = sig5;

  UintR nextPos = 2;

  if (regTcPtr->lastReplicaNo > 1) {
    sig0 = (UintR)regTcPtr->tcNodedata[2] +
           (UintR)(regTcPtr->tcNodedata[3] << 16);
    lqhKeyReq->variableData[nextPos] = sig0;
    nextPos++;
  }//if

  // Reset trigger count
  regTcPtr->numFiredTriggers = 0;
  regTcPtr->triggerExecutionCount = 0;
  regTcPtr->m_start_ticks = getHighResTimer();

  if (regCachePtr->useLongLqhKeyReq)
  {
    /* Build long LQHKeyReq using Key + AttrInfo sections */
    SectionHandle handle(this);
    SegmentedSectionPtr keyInfoSection;
    
    getSection(keyInfoSection, regCachePtr->keyInfoSectionI);

    handle.m_ptr[ LqhKeyReq::KeyInfoSectionNum ]= keyInfoSection;
    handle.m_cnt= 1;

    if (regTcPtr->m_special_op_flags & TcConnectRecord::SOF_UTIL_FLAG)
    {
      LqhKeyReq::setUtilFlag(lqhKeyReq->requestInfo, 1);
    }
    if (regCachePtr->attrlength != 0)
    {
      SegmentedSectionPtr attrInfoSection;

      ndbassert(regCachePtr->attrInfoSectionI != RNIL);
      getSection(attrInfoSection, regCachePtr->attrInfoSectionI);
      
      handle.m_ptr[ LqhKeyReq::AttrInfoSectionNum ]= attrInfoSection;
      handle.m_cnt= 2;
    }
    sendSignal(TBRef, GSN_LQHKEYREQ, signal, 
               nextPos + LqhKeyReq::FixedSignalLength, JBB, 
               &handle);

    /* Long sections were freed as part of sendSignal */
    ndbassert( handle.m_cnt == 0 );
    regCachePtr->keyInfoSectionI= RNIL;
    regCachePtr->attrInfoSectionI= RNIL;
  }
  else
  {
    /* Build short LQHKeyReq from Key + AttrInfo sections
     *
     * Read upto 4 words of KeyInfo from TCKEYREQ KeyInfo section into
     * LqhKeyReq signal
     */
    SegmentedSectionPtr keyInfoSection;
    
    getSection(keyInfoSection, regCachePtr->keyInfoSectionI);
    SectionReader keyInfoReader(keyInfoSection, getSectionSegmentPool());
    
    UintR keyLenInLqhKeyReq= MIN(LqhKeyReq::MaxKeyInfo, regCachePtr->keylen);

    keyInfoReader.getWords(&lqhKeyReq->variableData[nextPos], keyLenInLqhKeyReq);

    nextPos+= keyLenInLqhKeyReq;
    
    if (aiInLqhKeyReq != 0)
    {
      /* Read upto 5 words of AttrInfo from TCKEYREQ KeyInfo section into
       * LqhKeyReq signal
       */
      SegmentedSectionPtr attrInfoSection;

      ndbassert(regCachePtr->attrInfoSectionI != RNIL);

      getSection(attrInfoSection, regCachePtr->attrInfoSectionI);
      SectionReader attrInfoReader(attrInfoSection, getSectionSegmentPool());
      
      attrInfoReader.getWords(&lqhKeyReq->variableData[nextPos], aiInLqhKeyReq);
      
      nextPos+= aiInLqhKeyReq;
    }

    sendSignal(TBRef, GSN_LQHKEYREQ, signal, 
               nextPos + LqhKeyReq::FixedSignalLength, JBB);
  }
}//Dbtc::sendlqhkeyreq()

void Dbtc::packLqhkeyreq040Lab(Signal* signal,
                               BlockReference TBRef)
{
  TcConnectRecord * const regTcPtr = tcConnectptr.p;
  CacheRecord * const regCachePtr = cachePtr.p;
#ifdef ERROR_INSERT
  ApiConnectRecord * const regApiPtr = apiConnectptr.p;

  if (ERROR_INSERTED(8009)) {
    if (regApiPtr->apiConnectstate == CS_STARTED) {
      CLEAR_ERROR_INSERT_VALUE;
      return;
    }//if
  }//if
  if (ERROR_INSERTED(8010)) {
    if (regApiPtr->apiConnectstate == CS_START_COMMITTING) {
      CLEAR_ERROR_INSERT_VALUE;
      return;
    }//if
  }//if
#endif

  /* Do we have an ATTRINFO train to send? */
  if (!regCachePtr->useLongLqhKeyReq)
  {
    /* Short LqhKeyReq */
    if (regCachePtr->attrlength > LqhKeyReq::MaxAttrInfo)
    {
      if (unlikely( !sendAttrInfoTrain(signal,
                                       TBRef,
                                       tcConnectptr.i,
                                       LqhKeyReq::MaxAttrInfo,
                                       regCachePtr->attrInfoSectionI)))
      {
        jam();
        TCKEY_abort(signal, 17);
        return;
      }
    }
  } // useLongLqhKeyReq

  /* Release AttrInfo related storage, and the Cache Record */
  releaseAttrinfo();

  UintR TtcTimer = ctcTimer;
  UintR Tread = (regTcPtr->operation == ZREAD);
  UintR Tdirty = (regTcPtr->dirtyOp == ZTRUE);
  UintR Tboth = Tread & Tdirty;
  setApiConTimer(apiConnectptr.i, TtcTimer, __LINE__);
  jamDebug();
  /*--------------------------------------------------------------------
   *   WE HAVE SENT ALL THE SIGNALS OF THIS OPERATION. SET STATE AND EXIT.
   *---------------------------------------------------------------------*/
  if (Tboth) {
    jam();
    releaseDirtyRead(signal, apiConnectptr, tcConnectptr.p);
    return;
  }//if
  regTcPtr->tcConnectstate = OS_OPERATING;
  return;
}//Dbtc::packLqhkeyreq040Lab()

/* ========================================================================= */
/* -------      RELEASE ALL ATTRINFO RECORDS IN AN OPERATION RECORD  ------- */
/* ========================================================================= */
void Dbtc::releaseAttrinfo() 
{
  CacheRecord * const regCachePtr = cachePtr.p;
  Uint32 attrInfoSectionI= cachePtr.p->attrInfoSectionI;

  /* Release AttrInfo section if there is one */
  releaseSection( attrInfoSectionI );
  cachePtr.p->attrInfoSectionI= RNIL;

  //---------------------------------------------------
  // Now we will release the cache record at the same
  // time as releasing the attrinfo records.
  //---------------------------------------------------
  ApiConnectRecord * const regApiPtr = apiConnectptr.p;
  UintR TfirstfreeCacheRec = cfirstfreeCacheRec;
  UintR TCacheIndex = cachePtr.i;
  regCachePtr->nextCacheRec = TfirstfreeCacheRec;
  cfirstfreeCacheRec = TCacheIndex;
  regApiPtr->cachePtr = RNIL;
  return;
}//Dbtc::releaseAttrinfo()

/* ========================================================================= */
/* -------   RELEASE ALL RECORDS CONNECTED TO A DIRTY OPERATION     ------- */
/* ========================================================================= */
void Dbtc::releaseDirtyRead(Signal* signal, 
                            ApiConnectRecordPtr regApiPtr,
                            TcConnectRecord* regTcPtr) 
{
  Uint32 Ttckeyrec = regApiPtr.p->tckeyrec;
  Uint32 TclientData = regTcPtr->clientData;
  Uint32 Tnode = regTcPtr->tcNodedata[0];
  Uint32 Tlqhkeyreqrec = regApiPtr.p->lqhkeyreqrec;
  ConnectionState state = regApiPtr.p->apiConnectstate;
  
  regApiPtr.p->tcSendArray[Ttckeyrec] = TclientData;
  regApiPtr.p->tcSendArray[Ttckeyrec + 1] = TcKeyConf::DirtyReadBit | Tnode;
  regApiPtr.p->tckeyrec = Ttckeyrec + 2;
  
  unlinkReadyTcCon(signal);
  releaseTcCon();

  /**
   * No LQHKEYCONF in Simple/Dirty read
   * Therefore decrese no LQHKEYCONF(REF) we are waiting for
   */
  c_counters.csimpleReadCount++;
  regApiPtr.p->lqhkeyreqrec = --Tlqhkeyreqrec;
  
  if(Tlqhkeyreqrec == 0)
  {
    /**
     * Special case of lqhKeyConf_checkTransactionState:
     * - commit with zero operations: handle only for simple read
     */
    sendtckeyconf(signal, state == CS_START_COMMITTING);
    regApiPtr.p->apiConnectstate = 
      (state == CS_START_COMMITTING ? CS_CONNECTED : state);
    setApiConTimer(regApiPtr.i, 0, __LINE__);

    return;
  }
  
  /**
   * Emulate LQHKEYCONF
   */
  lqhKeyConf_checkTransactionState(signal, regApiPtr);
}//Dbtc::releaseDirtyRead()

/* ------------------------------------------------------------------------- */
/* -------        CHECK IF ALL TC CONNECTIONS ARE COMPLETED          ------- */
/* ------------------------------------------------------------------------- */
void Dbtc::unlinkReadyTcCon(Signal* signal) 
{
  TcConnectRecordPtr urtTcConnectptr;

  TcConnectRecord * const regTcPtr = tcConnectptr.p;
  TcConnectRecord *localTcConnectRecord = tcConnectRecord;
  UintR TtcConnectFilesize = ctcConnectFilesize;
  ApiConnectRecord * const regApiPtr = apiConnectptr.p;
  if (regTcPtr->prevTcConnect != RNIL) {
    jam();
    urtTcConnectptr.i = regTcPtr->prevTcConnect;
    ptrCheckGuard(urtTcConnectptr, TtcConnectFilesize, localTcConnectRecord);
    urtTcConnectptr.p->nextTcConnect = regTcPtr->nextTcConnect;
  } else {
    jam();
    regApiPtr->firstTcConnect = regTcPtr->nextTcConnect;
  }//if
  if (regTcPtr->nextTcConnect != RNIL) {
    jam();
    urtTcConnectptr.i = regTcPtr->nextTcConnect;
    ptrCheckGuard(urtTcConnectptr, TtcConnectFilesize, localTcConnectRecord);
    urtTcConnectptr.p->prevTcConnect = regTcPtr->prevTcConnect;
  } else {
    jam();
    regApiPtr->lastTcConnect = tcConnectptr.p->prevTcConnect;
  }//if
}//Dbtc::unlinkReadyTcCon()

void Dbtc::releaseTcCon() 
{
  TcConnectRecord * const regTcPtr = tcConnectptr.p;
  UintR TfirstfreeTcConnect = cfirstfreeTcConnect;
  UintR TtcConnectptrIndex = tcConnectptr.i;

  ndbrequire(regTcPtr->commitAckMarker == RNIL);
  regTcPtr->tcConnectstate = OS_CONNECTED;
  regTcPtr->nextTcConnect = TfirstfreeTcConnect;
  regTcPtr->apiConnect = RNIL;
  regTcPtr->m_special_op_flags = 0;
  regTcPtr->indexOp = RNIL;
  cfirstfreeTcConnect = TtcConnectptrIndex;
  c_counters.cconcurrentOp--;

  if (regTcPtr->triggeringOperation != RNIL &&
      regTcPtr->currentTriggerId != RNIL)
  {
    jam();
    Ptr<TcDefinedTriggerData> trigPtr;
    c_theDefinedTriggers.getPtr(trigPtr, regTcPtr->currentTriggerId);
    ndbrequire(trigPtr.p->refCount > 0);
    trigPtr.p->refCount--;
  }

  if (!regTcPtr->thePendingTriggers.isEmpty())
  {
    Local_TcFiredTriggerData_fifo
      list(c_theFiredTriggerPool, regTcPtr->thePendingTriggers);
    releaseFiredTriggerData(&list);
  }

  ndbrequire(regTcPtr->thePendingTriggers.isEmpty());

}//Dbtc::releaseTcCon()

void Dbtc::execPACKED_SIGNAL(Signal* signal) 
{
  LqhKeyConf * const lqhKeyConf = (LqhKeyConf *)signal->getDataPtr();

  UintR Ti;
  UintR Tstep = 0;
  UintR Tlength;
  UintR TpackedData[28];
  UintR Tdata1, Tdata2, Tdata3, Tdata4;

  jamEntry();
  Tlength = signal->length();
  if (Tlength > 25) {
    jam();
    systemErrorLab(signal, __LINE__);
    return;
  }//if
  Uint32* TpackDataPtr;
  for (Ti = 0; Ti < Tlength; Ti += 4) {
    Uint32* TsigDataPtr = &signal->theData[Ti];
    Tdata1 = TsigDataPtr[0];
    Tdata2 = TsigDataPtr[1];
    Tdata3 = TsigDataPtr[2];
    Tdata4 = TsigDataPtr[3];

    TpackDataPtr = &TpackedData[Ti];
    TpackDataPtr[0] = Tdata1;
    TpackDataPtr[1] = Tdata2;
    TpackDataPtr[2] = Tdata3;
    TpackDataPtr[3] = Tdata4;
  }//for

  if (VERIFY_PACKED_RECEIVE)
  {
    ndbrequire(PackedSignal::verify(&TpackedData[0],
                                    Tlength,
                                    cownref,
                                    TC_RECEIVE_TYPES,
                                    0)); /* Irrelevant */
  }
  while (Tlength > Tstep) {

    TpackDataPtr = &TpackedData[Tstep];
    Tdata1 = TpackDataPtr[0];
    Tdata2 = TpackDataPtr[1];
    Tdata3 = TpackDataPtr[2];

    lqhKeyConf->connectPtr = Tdata1 & 0x0FFFFFFF;
    lqhKeyConf->opPtr = Tdata2;
    lqhKeyConf->userRef = Tdata3;

    switch (Tdata1 >> 28) {
    case ZCOMMITTED:
      signal->header.theLength = 3;
      jamBuffer()->markEndOfSigExec();
      execCOMMITTED(signal);
      Tstep += 3;
      break;
    case ZCOMPLETED:
      signal->header.theLength = 3;
      jamBuffer()->markEndOfSigExec();
      execCOMPLETED(signal);
      Tstep += 3;
      break;
    case ZLQHKEYCONF:
      jamDebug();
      Tdata1 = TpackDataPtr[3];
      Tdata2 = TpackDataPtr[4];
      Tdata3 = TpackDataPtr[5];
      Tdata4 = TpackDataPtr[6];

      lqhKeyConf->readLen = Tdata1;
      lqhKeyConf->transId1 = Tdata2;
      lqhKeyConf->transId2 = Tdata3;
      lqhKeyConf->numFiredTriggers = Tdata4;
      signal->header.theLength = LqhKeyConf::SignalLength;
      jamBuffer()->markEndOfSigExec();
      execLQHKEYCONF(signal);
      Tstep += LqhKeyConf::SignalLength;
      break;
    case ZFIRE_TRIG_CONF:
      jamDebug();
      signal->header.theLength = 4;
      signal->theData[3] = TpackDataPtr[3];
      jamBuffer()->markEndOfSigExec();
      execFIRE_TRIG_CONF(signal);
      Tstep += 4;
      break;
    default:
      systemErrorLab(signal, __LINE__);
      return;
    }//switch
  }//while
  return;
}//Dbtc::execPACKED_SIGNAL()


void Dbtc::execSIGNAL_DROPPED_REP(Signal* signal)
{
  /* An incoming signal was dropped, handle it 
   * Dropped signal really means that we ran out of 
   * long signal buffering to store its sections
   */
  jamEntry();

  if (!assembleDroppedFragments(signal))
  {
    jam();
    return;
  }

  const SignalDroppedRep* rep = (SignalDroppedRep*) &signal->theData[0];
  Uint32 originalGSN= rep->originalGsn;

  DEBUG("SignalDroppedRep received for GSN " << originalGSN);

  switch(originalGSN) {
  case GSN_TCKEYREQ:
    jam(); 
    /* Fall through */
  case GSN_TCINDXREQ:
  {
    jam();
    
    /* Get original signal data - unfortunately it may
     * have been truncated.  We must not read beyond
     * word # 22
     * We will send an Abort to the Api using info from
     * the received signal and clean up our transaction 
     * state
     */
    const TcKeyReq * const truncatedTcKeyReq = 
      (TcKeyReq *) &rep->originalData[0];
    
    const UintR apiIndex = truncatedTcKeyReq->apiConnectPtr;
    
    if (apiIndex >= capiConnectFilesize)
    {
      jam();
      warningHandlerLab(signal, __LINE__);
      return;
    }
    
    /* We have a valid Api ConnectPtr...
     * Ensure that we have the  necessary information 
     * to send a rollback to the client
     */
    apiConnectptr.i = apiIndex;
    ApiConnectRecord * const regApiPtr = &apiConnectRecord[apiIndex];
    apiConnectptr.p = regApiPtr;
    UintR transId1= truncatedTcKeyReq->transId1;
    UintR transId2= truncatedTcKeyReq->transId2;
    
    /* Ensure that the apiConnectptr global is initialised
     * may not be in cases where we drop the first signal of
     * a transaction
     */
    apiConnectptr.p->transid[0] = transId1;
    apiConnectptr.p->transid[1] = transId2;
    apiConnectptr.p->returncode = ZGET_DATAREC_ERROR;

    /* Set m_exec_flag according to the dropped request */
    apiConnectptr.p->m_flags |=
      TcKeyReq::getExecuteFlag(truncatedTcKeyReq->requestInfo) ?
      ApiConnectRecord::TF_EXEC_FLAG : 0;

    DEBUG(" Execute flag set to " << tc_testbit(apiConnectptr.p->m_flags,
                                                ApiConnectRecord::TF_EXEC_FLAG)
          );

    abortErrorLab(signal);

    break;
  }
  case GSN_SCAN_TABREQ:
  {
    jam();
    /* Get information necessary to send SCAN_TABREF back to client */
    // TODO : Handle dropped signal fragments
    const ScanTabReq * const truncatedScanTabReq = 
      (ScanTabReq *) &rep->originalData[0];

    Uint32 apiConnectPtr= truncatedScanTabReq->apiConnectPtr;
    Uint32 transId1= truncatedScanTabReq->transId1;
    Uint32 transId2= truncatedScanTabReq->transId2;
    
    if (apiConnectPtr >= capiConnectFilesize)
    {
      jam();
      warningHandlerLab(signal, __LINE__);
      return;
    }//if
    
    apiConnectptr.i = apiConnectPtr;
    ptrAss(apiConnectptr, apiConnectRecord);
    ApiConnectRecord * transP = apiConnectptr.p;
    
    /* Now send the SCAN_TABREF */
    ScanTabRef* ref= (ScanTabRef*)&signal->theData[0];
    ref->apiConnectPtr = transP->ndbapiConnect;
    ref->transId1= transId1;
    ref->transId2= transId2;
    ref->errorCode= ZGET_ATTRBUF_ERROR;
    ref->closeNeeded= 0;

    sendSignal(transP->ndbapiBlockref, GSN_SCAN_TABREF,
               signal, ScanTabRef::SignalLength, JBB);
    break;
  }
  case GSN_TRANSID_AI: //TUP -> TC
  {
    jam();
    /**
     * TRANSID_AI is received as a result of performing a read on 
     * the index table as part of a (unique) index operation.
     */
    const TransIdAI * const truncatedTransIdAI = 
      reinterpret_cast<const TransIdAI*>(&rep->originalData[0]);

    TcIndexOperationPtr indexOpPtr;
    indexOpPtr.i = truncatedTransIdAI->connectPtr;
    TcIndexOperation* indexOp = c_theIndexOperationPool.getPtr(indexOpPtr.i);
    indexOpPtr.p = indexOp;
    if (indexOp == NULL) {
      jam();
      // Missing index operation - ignore
      break;
    }

    /* No more TransIdAI will arrive, abort */
    apiConnectptr.i = indexOp->connectionIndex;
    ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);

    terrorCode = ZGET_DATAREC_ERROR;
    abortErrorLab(signal);
    break;
  }
  case GSN_TRANSID_AI_R:  //TODO
    jam();
    // Fall through
  default:
    jam();
    /* Don't expect dropped signals for other GSNs,
     * default handling
     * TODO : Can TC get long TRANSID_AI as part of 
     * Unique index operations?
     */
    SimulatedBlock::execSIGNAL_DROPPED_REP(signal);
  };
  
  return;
}

bool
Dbtc::CommitAckMarker::insert_in_commit_ack_marker(Dbtc *tc,
                                                   Uint32 instanceKey,
                                                   NodeId node_id)
{
  const NodeInfo& nodeInfo = tc->getNodeInfo(node_id);
  Uint32 workers = nodeInfo.m_lqh_workers;
  assert(instanceKey != 0);
  Uint32 instanceNo = workers == 0 ? 0 : 1 + (instanceKey - 1) % workers;
  Uint32 item = instanceNo + (node_id << 16);
  CommitAckMarkerBuffer::DataBufferPool & pool =
    tc->c_theCommitAckMarkerBufferPool;
  // check for duplicate (todo DataBuffer method find-or-append)
  {
    LocalCommitAckMarkerBuffer tmp(pool, this->theDataBuffer);
    CommitAckMarkerBuffer::Iterator iter;
    bool next_flag = tmp.first(iter);
    while (next_flag)
    {
      Uint32 dataWord = *iter.data;
      if (dataWord == item)
      {
        return true;
      }
      next_flag = tmp.next(iter, 1);
    }
  }
  LocalCommitAckMarkerBuffer tmp(pool, this->theDataBuffer);
  return tmp.append(&item, (Uint32)1);
}
bool
Dbtc::CommitAckMarker::insert_in_commit_ack_marker_all(Dbtc *tc,
                                                       NodeId node_id)
{
  for (Uint32 ikey = 1; ikey <= MAX_NDBMT_LQH_THREADS; ikey++)
  {
    if (!insert_in_commit_ack_marker(tc, ikey, node_id))
      return false;
  }
  return true;
}

void Dbtc::execLQHKEYCONF(Signal* signal) 
{
  const LqhKeyConf * lqhKeyConf = CAST_CONSTPTR(LqhKeyConf,
                                                signal->getDataPtr());
#ifdef UNUSED
  ndbout << "TC: Received LQHKEYCONF"
         << " transId1=" << lqhKeyConf-> transId1
         << " transId2=" << lqhKeyConf-> transId2
	 << endl;
#endif /*UNUSED*/
  UintR compare_transid1, compare_transid2;
  BlockReference tlastLqhBlockref;
  UintR tlastLqhConnect;
  UintR treadlenAi;
  UintR TtcConnectptrIndex;
  UintR TtcConnectFilesize = ctcConnectFilesize;

  tlastLqhConnect = lqhKeyConf->connectPtr;
  TtcConnectptrIndex = lqhKeyConf->opPtr;
  tlastLqhBlockref = lqhKeyConf->userRef;
  treadlenAi = lqhKeyConf->readLen;
  TcConnectRecord *localTcConnectRecord = tcConnectRecord;

  /*------------------------------------------------------------------------
   * NUMBER OF EXTERNAL TRIGGERS FIRED IN DATA[6] 
   * OPERATION IS NOW COMPLETED. CHECK FOR CORRECT OPERATION POINTER  
   * TO ENSURE NO CRASHES BECAUSE OF ERRONEUS NODES. CHECK STATE OF   
   * OPERATION. THEN SET OPERATION STATE AND RETRIEVE ALL POINTERS    
   * OF THIS OPERATION. PUT COMPLETED OPERATION IN LIST OF COMPLETED  
   * OPERATIONS ON THE LQH CONNECT RECORD.                          
   *------------------------------------------------------------------------
   * THIS SIGNAL ALWAYS ARRIVE BEFORE THE ABORTED SIGNAL ARRIVES SINCE IT USES
   * THE SAME PATH BACK TO TC AS THE ABORTED SIGNAL DO. WE DO HOWEVER HAVE A 
   * PROBLEM  WHEN WE ENCOUNTER A TIME-OUT WAITING FOR THE ABORTED SIGNAL.  
   * THEN THIS SIGNAL MIGHT ARRIVE WHEN THE TC CONNECT RECORD HAVE BEEN REUSED
   * BY OTHER TRANSACTION THUS WE CHECK THE TRANSACTION ID OF THE SIGNAL 
   * BEFORE ACCEPTING THIS SIGNAL.
   * Due to packing of LQHKEYCONF the ABORTED signal can now arrive before 
   * this. 
   * This is more reason to ignore the signal if not all states are correct.
   *------------------------------------------------------------------------*/
  if (TtcConnectptrIndex >= TtcConnectFilesize) {
    TCKEY_abort(signal, 25);
    return;
  }//if
  TcConnectRecord* const regTcPtr = &localTcConnectRecord[TtcConnectptrIndex];
  OperationState TtcConnectstate = regTcPtr->tcConnectstate;
  tcConnectptr.i = TtcConnectptrIndex;
  tcConnectptr.p = regTcPtr;
  if (TtcConnectstate != OS_OPERATING) {
    warningReport(signal, 23);
    return;
  }//if
  ApiConnectRecord *localApiConnectRecord = apiConnectRecord;
  UintR TapiConnectptrIndex = regTcPtr->apiConnect;
  UintR TapiConnectFilesize = capiConnectFilesize;
  UintR Ttrans1 = lqhKeyConf->transId1;
  UintR Ttrans2 = lqhKeyConf->transId2;
  Uint32 numFired = LqhKeyConf::getFiredCount(lqhKeyConf->numFiredTriggers);
  Uint32 deferreduk = LqhKeyConf::getDeferredUKBit(lqhKeyConf->numFiredTriggers);
  Uint32 deferredfk = LqhKeyConf::getDeferredFKBit(lqhKeyConf->numFiredTriggers);

  if (TapiConnectptrIndex >= TapiConnectFilesize) {
    TCKEY_abort(signal, 29);
    return;
  }//if
  Ptr<ApiConnectRecord> regApiPtr;
  regApiPtr.i = TapiConnectptrIndex;
  regApiPtr.p = &localApiConnectRecord[TapiConnectptrIndex];
  apiConnectptr.i = TapiConnectptrIndex;
  apiConnectptr.p = regApiPtr.p;
  compare_transid1 = regApiPtr.p->transid[0] ^ Ttrans1;
  compare_transid2 = regApiPtr.p->transid[1] ^ Ttrans2;
  compare_transid1 = compare_transid1 | compare_transid2;
  if (compare_transid1 != 0) {
    warningReport(signal, 24);
    return;
  }//if

#ifdef ERROR_INSERT
  if (ERROR_INSERTED(8029)) {
    systemErrorLab(signal, __LINE__);
  }//if
  if (ERROR_INSERTED(8003)) {
    if (regApiPtr.p->apiConnectstate == CS_STARTED) {
      CLEAR_ERROR_INSERT_VALUE;
      return;
    }//if
  }//if
  if (ERROR_INSERTED(8004)) {
    if (regApiPtr.p->apiConnectstate == CS_RECEIVING) {
      CLEAR_ERROR_INSERT_VALUE;
      return;
    }//if
  }//if
  if (ERROR_INSERTED(8005)) {
    if (regApiPtr.p->apiConnectstate == CS_REC_COMMITTING) {
      CLEAR_ERROR_INSERT_VALUE;
      return;
    }//if
  }//if
  if (ERROR_INSERTED(8006)) {
    if (regApiPtr.p->apiConnectstate == CS_START_COMMITTING) {
      CLEAR_ERROR_INSERT_VALUE;
      return;
    }//if
  }//if
  if (ERROR_INSERTED(8023)) {
    SET_ERROR_INSERT_VALUE(8024);
    return;
  }//if
  if (ERROR_INSERTED(8107))
  {
    jam();
    ndbout_c("Error 8107, timing out transaction");
    timeOutFoundLab(signal, apiConnectptr.i, ZTIME_OUT_ERROR);
    return;
  }
      
#endif
  UintR TtcTimer = ctcTimer;
  regTcPtr->lastLqhCon = tlastLqhConnect;
  regTcPtr->lastLqhNodeId = refToNode(tlastLqhBlockref);
  regTcPtr->numFiredTriggers = numFired;
  regTcPtr->m_special_op_flags |=
    ((deferreduk) ? TcConnectRecord::SOF_DEFERRED_UK_TRIGGER : 0 ) |
    ((deferredfk) ? TcConnectRecord::SOF_DEFERRED_FK_TRIGGER : 0 );
  regApiPtr.p->m_flags |=
    ((deferreduk) ? ApiConnectRecord::TF_DEFERRED_UK_TRIGGERS : 0) |
    ((deferredfk) ? ApiConnectRecord::TF_DEFERRED_FK_TRIGGERS : 0);

  UintR Ttckeyrec = (UintR)regApiPtr.p->tckeyrec;
  UintR TclientData = regTcPtr->clientData;
  UintR TdirtyOp = regTcPtr->dirtyOp;
  Uint32 TopSimple = regTcPtr->opSimple;
  Uint32 Toperation = regTcPtr->operation;
  ConnectionState TapiConnectstate = regApiPtr.p->apiConnectstate;

  if (TapiConnectstate == CS_ABORTING) {
    warningReport(signal, 27);
    return;
  }//if

  Uint32 lockingOpI = RNIL;
  if (Toperation == ZUNLOCK)
  {
    /* For unlock operations readlen in TCKEYCONF carries
     * the locking operation TC reference
     */
    lockingOpI = treadlenAi;
    treadlenAi = 0;
  }

  /* Handle case where LQHKEYREQ requested an LQH CommitAckMarker */
  Uint32 commitAckMarker = regTcPtr->commitAckMarker;
  setApiConTimer(apiConnectptr.i, TtcTimer, __LINE__);
  if (commitAckMarker != RNIL)
  {
    /* Update TC CommitAckMarker record to track LQH CommitAckMarkers */
    const Uint32 noOfLqhs = regTcPtr->noOfNodes;
    CommitAckMarker * tmp = m_commitAckMarkerHash.getPtr(commitAckMarker);
    jam();
    /**
     * Now that we have a marker in all nodes in at least one nodegroup, 
     * don't need to request any more for this transaction
     */
    regApiPtr.p->m_flags |= ApiConnectRecord::TF_COMMIT_ACK_MARKER_RECEIVED;
    
    /**
     * Populate LQH array
     */
    for(Uint32 i = 0; i < noOfLqhs; i++)
    {
      jamDebug();
      if (ERROR_INSERTED(8096) && i+1 == noOfLqhs)
      {
        CLEAR_ERROR_INSERT_VALUE;
        TCKEY_abort(signal, 67);
        return;
      }
      if (!tmp->insert_in_commit_ack_marker(this,
                                            regTcPtr->lqhInstanceKey,
                                            regTcPtr->tcNodedata[i]))
      {
        TCKEY_abort(signal, 67);
        return;
      }
    }
  }
  if (regTcPtr->isIndexOp(regTcPtr->m_special_op_flags)) {
    jam();
    // This was an internal TCKEYREQ
    // will be returned unpacked
    regTcPtr->attrInfoLen = treadlenAi;
  } else {
    if (numFired == 0 && regTcPtr->triggeringOperation == RNIL) {
      jam();

      if (Ttckeyrec > (ZTCOPCONF_SIZE - 2)) {
        TCKEY_abort(signal, 30);
        return;
      }

      /*
       * Skip counting triggering operations the first round
       * since they will enter execLQHKEYCONF a second time
       * Skip counting internally generated TcKeyReq
       */
      regApiPtr.p->tcSendArray[Ttckeyrec] = TclientData;
      regApiPtr.p->tcSendArray[Ttckeyrec + 1] = treadlenAi;
      regApiPtr.p->tckeyrec = Ttckeyrec + 2;
    }//if
  }//if
  if (TdirtyOp == ZTRUE) 
  {
    UintR Tlqhkeyreqrec = regApiPtr.p->lqhkeyreqrec;
    jam();
    releaseDirtyWrite(signal);
    regApiPtr.p->lqhkeyreqrec = Tlqhkeyreqrec - 1;
  } 
  else if (Toperation == ZREAD && TopSimple)
  {
    UintR Tlqhkeyreqrec = regApiPtr.p->lqhkeyreqrec;
    jam();
    unlinkReadyTcCon(signal);
    releaseTcCon();
    regApiPtr.p->lqhkeyreqrec = Tlqhkeyreqrec - 1;
  }
  else if (Toperation == ZUNLOCK)
  {
    jam();
    /* We've unlocked and released a read operation in LQH
     * The readLenAi member contains the TC OP reference
     * for the unlocked operation.
     * So here we :
     * 1) Validate the TC OP reference
     * 2) Release the referenced TC op
     * 3) Send TCKEYCONF back to the user
     * 4) Release our own TC op
     */
    Uint32 unlockOpI = tcConnectptr.i;

    ndbrequire( numFired == 0 );
    ndbrequire( regTcPtr->triggeringOperation == RNIL );

    /* Switch to the original locking operation */
    if (unlikely( lockingOpI >= ctcConnectFilesize ))
    {
      jam();
      TCKEY_abort(signal, 61);
      return;
    }
    tcConnectptr.i = lockingOpI;
    ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
    
    const TcConnectRecord * regLockTcPtr = tcConnectptr.p;
    
    /* Validate the locking operation's state */
    bool locking_op_ok = 
      ( ( regLockTcPtr->apiConnect == regTcPtr->apiConnect ) &&
        ( ( regLockTcPtr->operation == ZREAD ) ||
          ( regLockTcPtr->operation == ZREAD_EX ) ) &&
        ( regLockTcPtr->tcConnectstate == OS_PREPARED ) &&
        ( ! regLockTcPtr->dirtyOp ) &&
        ( ! regLockTcPtr->opSimple ) &&
        ( ! TcConnectRecord::isIndexOp(regLockTcPtr->m_special_op_flags) ) &&
        ( regLockTcPtr->commitAckMarker == RNIL ) );

    if (unlikely (! locking_op_ok ))
    {
      jam();
      TCKEY_abort(signal, 63);
      return;
    }
    
    /* Ok, all checks passed, release the original locking op */
    unlinkReadyTcCon(signal);
    releaseTcCon();

    /* Remove record of original locking op's LQHKEYREQ/CONF
     * etc.
     */
    ndbrequire( regApiPtr.p->lqhkeyreqrec );
    ndbrequire( regApiPtr.p->lqhkeyconfrec );
    regApiPtr.p->lqhkeyreqrec -= 1;
    regApiPtr.p->lqhkeyconfrec -= 1;

    /* Switch back to the unlock operation */
    tcConnectptr.i = unlockOpI;
    ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);

    /* Release the unlock operation */
    unlinkReadyTcCon(signal);
    releaseTcCon();

    /* Remove record of unlock op's LQHKEYREQ */
    ndbrequire( regApiPtr.p->lqhkeyreqrec );
    regApiPtr.p->lqhkeyreqrec -= 1;

    /* TCKEYCONF sent below */
  }
  else 
  {
    if (numFired == 0)
    {
      // No triggers to execute
      UintR Tlqhkeyconfrec = regApiPtr.p->lqhkeyconfrec;
      regApiPtr.p->lqhkeyconfrec = Tlqhkeyconfrec + 1;
      regTcPtr->tcConnectstate = OS_PREPARED;
    }
    else
    {
      jam();
    }
  }//if

  /**
   * And now decide what to do next
   * 1) First check if there are fired triggers
   * 2) Then check if it's a index-table read
   * 3) Then check if op was created by trigger
   * 4) Else it's a normal op
   *
   * - trigger op, can cause new trigger ops (cascade)
   * - trigger op can be using uk
   */
  if (numFired)
  {
    // We have fired triggers
    jam();
    time_track_complete_key_operation(regTcPtr,
                               refToNode(regApiPtr.p->ndbapiBlockref),
                               regTcPtr->tcNodedata[0]);
    saveTriggeringOpState(signal, regTcPtr);
    if (regTcPtr->numReceivedTriggers == numFired)
    {
      // We have received all data
      jam();
      regApiPtr.p->theFiredTriggers.appendList(regTcPtr->thePendingTriggers);
      executeTriggers(signal, &regApiPtr);
    }
    // else wait for more trigger data
  }
  else if (regTcPtr->isIndexOp(regTcPtr->m_special_op_flags))
  {
    // This is a index-table read
    jam();
    time_track_complete_index_key_operation(regTcPtr,
                                  refToNode(regApiPtr.p->ndbapiBlockref),
                                  regTcPtr->tcNodedata[0]);
    setupIndexOpReturn(regApiPtr.p, regTcPtr);
    lqhKeyConf_checkTransactionState(signal, regApiPtr);
  }
  else if (regTcPtr->triggeringOperation == RNIL)
  {
    // This is "normal" path
    jamDebug();
    time_track_complete_key_operation(regTcPtr,
                               refToNode(regApiPtr.p->ndbapiBlockref),
                               regTcPtr->tcNodedata[0]);
    lqhKeyConf_checkTransactionState(signal, regApiPtr);
  }
  else
  {
    jam();
    /**
     * This operation was created by a trigger executing operation
     * Thus no need to handle the operation any further, return to
     * the original operation instead. There can be many triggering
     * operations waiting on each other here.
     *
     * Restart the original operation if we have executed all it's
     * triggers.
     */
    TcConnectRecordPtr opPtr;

    time_track_complete_index_key_operation(regTcPtr,
                                  refToNode(regApiPtr.p->ndbapiBlockref),
                                  regTcPtr->tcNodedata[0]);
    opPtr.i = regTcPtr->triggeringOperation;
    ptrCheckGuard(opPtr, ctcConnectFilesize, localTcConnectRecord);
    trigger_op_finished(signal, regApiPtr, regTcPtr->currentTriggerId,
                        opPtr.p, 0);
  }
}//Dbtc::execLQHKEYCONF()
 
void Dbtc::setupIndexOpReturn(ApiConnectRecord* regApiPtr,
			      TcConnectRecord* regTcPtr) 
{
  regApiPtr->m_flags |= ApiConnectRecord::TF_INDEX_OP_RETURN;
  regApiPtr->indexOp = regTcPtr->indexOp;
  TcIndexOperationPtr indexOpPtr;
  indexOpPtr.i = regApiPtr->indexOp;
  TcIndexOperation* indexOp = c_theIndexOperationPool.getPtr(indexOpPtr.i);
  if (tc_testbit(indexOp->savedFlags,
                 ApiConnectRecord::TF_DEFERRED_CONSTRAINTS))
  {
    regApiPtr->m_flags |= ApiConnectRecord::TF_DEFERRED_CONSTRAINTS;
  }

  if (tc_testbit(indexOp->savedFlags,
                 ApiConnectRecord::TF_DISABLE_FK_CONSTRAINTS))
  {
    regApiPtr->m_flags |= ApiConnectRecord::TF_DISABLE_FK_CONSTRAINTS;
  }
  regApiPtr->clientData = regTcPtr->clientData;
  regApiPtr->attrInfoLen = regTcPtr->attrInfoLen;
}

/**
 * lqhKeyConf_checkTransactionState
 *
 * This functions checks state variables, and
 *   decides if it should wait for more LQHKEYCONF signals
 *   or if it should start commiting
 */
void
Dbtc::lqhKeyConf_checkTransactionState(Signal * signal,
				       Ptr<ApiConnectRecord> regApiPtr)
{
/*---------------------------------------------------------------*/
/* IF THE COMMIT FLAG IS SET IN SIGNAL TCKEYREQ THEN DBTC HAS TO */
/* SEND TCKEYCONF FOR ALL OPERATIONS EXCEPT THE LAST ONE. WHEN   */
/* THE TRANSACTION THEN IS COMMITTED TCKEYCONF IS SENT FOR THE   */
/* WHOLE TRANSACTION                                             */
/* IF THE COMMIT FLAG IS NOT RECECIVED DBTC WILL SEND TCKEYCONF  */
/* FOR ALL OPERATIONS, AND THEN WAIT FOR THE API TO CONCLUDE THE */
/* TRANSACTION                                                   */
/*---------------------------------------------------------------*/
  ConnectionState TapiConnectstate = regApiPtr.p->apiConnectstate;
  UintR Tlqhkeyconfrec = regApiPtr.p->lqhkeyconfrec;
  UintR Tlqhkeyreqrec = regApiPtr.p->lqhkeyreqrec;
  int TnoOfOutStanding = Tlqhkeyreqrec - Tlqhkeyconfrec;

  apiConnectptr = regApiPtr;
  switch (TapiConnectstate) {
  case CS_START_COMMITTING:
    if (TnoOfOutStanding == 0) {
      jam();
      diverify010Lab(signal);
      return;
    } else if (TnoOfOutStanding > 0) {
      if (regApiPtr.p->tckeyrec == ZTCOPCONF_SIZE) {
        jam();
        sendtckeyconf(signal, 0);
        return;
      }
      else if (tc_testbit(regApiPtr.p->m_flags,
                          ApiConnectRecord::TF_INDEX_OP_RETURN))
      {
	jam();
        sendtckeyconf(signal, 0);
        return;
      }//if
      jam();
      return;
    } else {
      TCKEY_abort(signal, 44);
      return;
    }//if
    return;
  case CS_STARTED:
  case CS_RECEIVING:
    if (TnoOfOutStanding == 0) {
      jam();
      sendtckeyconf(signal, 2);
      return;
    } else {
      if (regApiPtr.p->tckeyrec == ZTCOPCONF_SIZE) {
        jam();
        sendtckeyconf(signal, 0);
        return;
      }
      else if (tc_testbit(regApiPtr.p->m_flags,
                          ApiConnectRecord::TF_INDEX_OP_RETURN))
      {
	jam();
        sendtckeyconf(signal, 0);
        return;
      }//if
      jam();
    }//if
    return;
  case CS_REC_COMMITTING:
    if (TnoOfOutStanding > 0) {
      if (regApiPtr.p->tckeyrec == ZTCOPCONF_SIZE) {
        jam();
        sendtckeyconf(signal, 0);
        return;
      }
      else if (tc_testbit(regApiPtr.p->m_flags,
                          ApiConnectRecord::TF_INDEX_OP_RETURN))
      {
        jam();
        sendtckeyconf(signal, 0);
        return;
      }//if
      jam();
      return;
    }//if
    TCKEY_abort(signal, 45);
    return;
  case CS_CONNECTED:
    jam();
/*---------------------------------------------------------------*/
/*       WE HAVE CONCLUDED THE TRANSACTION SINCE IT WAS ONLY     */
/*       CONSISTING OF DIRTY WRITES AND ALL OF THOSE WERE        */
/*       COMPLETED. ENSURE TCKEYREC IS ZERO TO PREVENT ERRORS.   */
/*---------------------------------------------------------------*/
    regApiPtr.p->tckeyrec = 0;
    return;
  case CS_SEND_FIRE_TRIG_REQ:
    return;
  case CS_WAIT_FIRE_TRIG_REQ:
    if (tc_testbit(regApiPtr.p->m_flags,
                   ApiConnectRecord::TF_INDEX_OP_RETURN))
    {
      jam();
      sendtckeyconf(signal, 0);
      return;
    }
    else if (TnoOfOutStanding == 0 && regApiPtr.p->pendingTriggers == 0)
    {
      jam();
      regApiPtr.p->apiConnectstate = CS_START_COMMITTING;
      diverify010Lab(signal);
      return;
    }
    return;
  default:
    TCKEY_abort(signal, 46);
    return;
  }//switch
}//Dbtc::lqhKeyConf_checkTransactionState()

void Dbtc::sendtckeyconf(Signal* signal, UintR TcommitFlag)
{
  if(ERROR_INSERTED(8049)){
    CLEAR_ERROR_INSERT_VALUE;
    signal->theData[0] = TcContinueB::DelayTCKEYCONF;
    signal->theData[1] = apiConnectptr.i;
    signal->theData[2] = TcommitFlag;
    sendSignalWithDelay(cownref, GSN_CONTINUEB, signal, 3000, 3);
    return;
  }
  
  HostRecordPtr localHostptr;
  ApiConnectRecord * const regApiPtr = apiConnectptr.p;
  const UintR TopWords = (UintR)regApiPtr->tckeyrec;
  localHostptr.i = refToNode(regApiPtr->ndbapiBlockref);
  const Uint32 type = getNodeInfo(localHostptr.i).m_type;
  const bool is_api = (type >= NodeInfo::API && type <= NodeInfo::MGM);
  const BlockNumber TblockNum = refToBlock(regApiPtr->ndbapiBlockref);
  const Uint32 Tmarker = (regApiPtr->commitAckMarker == RNIL) ? 0 : 1;
  ptrAss(localHostptr, hostRecord);
  struct PackedWordsContainer * container = &localHostptr.p->packTCKEYCONF;
  UintR TcurrLen = container->noOfPackedWords;
  UintR confInfo = 0;
  TcKeyConf::setCommitFlag(confInfo, TcommitFlag == 1);
  TcKeyConf::setMarkerFlag(confInfo, Tmarker);
  const UintR TpacketLen = 6 + TopWords;
  regApiPtr->tckeyrec = 0;

  if (tc_testbit(regApiPtr->m_flags, ApiConnectRecord::TF_INDEX_OP_RETURN))
  {
    jam();
    // Return internally generated TCKEY
    TcKeyConf * const tcKeyConf = (TcKeyConf *)signal->getDataPtrSend();
    TcKeyConf::setNoOfOperations(confInfo, 1);
    tcKeyConf->apiConnectPtr = regApiPtr->indexOp;
    tcKeyConf->gci_hi = Uint32(regApiPtr->globalcheckpointid >> 32);
    Uint32* gci_lo = (Uint32*)&tcKeyConf->operations[1];
    * gci_lo = Uint32(regApiPtr->globalcheckpointid);
    tcKeyConf->confInfo = confInfo;
    tcKeyConf->transId1 = regApiPtr->transid[0];
    tcKeyConf->transId2 = regApiPtr->transid[1];
    tcKeyConf->operations[0].apiOperationPtr = regApiPtr->clientData;
    tcKeyConf->operations[0].attrInfoLen = regApiPtr->attrInfoLen;
    Uint32 sigLen = 1 /** gci_lo */ +
      TcKeyConf::StaticLength + TcKeyConf::OperationLength;
    EXECUTE_DIRECT(DBTC, GSN_TCKEYCONF, signal, sigLen);
    tc_clearbit(regApiPtr->m_flags, ApiConnectRecord::TF_INDEX_OP_RETURN);
    if (TopWords == 0) {
      jamDebug();
      return; // No queued TcKeyConf
    }//if
  }//if
  if(TcommitFlag)
  {
    jam();
    tc_clearbit(regApiPtr->m_flags, ApiConnectRecord::TF_EXEC_FLAG);
    if (TcommitFlag == 1)
    {
      time_track_complete_transaction(regApiPtr);
    }
  }
  TcKeyConf::setNoOfOperations(confInfo, (TopWords >> 1));
  if ((TpacketLen + 1 /** gci_lo */ > 25) ||!is_api){
    TcKeyConf * const tcKeyConf = (TcKeyConf *)signal->getDataPtrSend();
    
    jam();
    tcKeyConf->apiConnectPtr = regApiPtr->ndbapiConnect;
    tcKeyConf->gci_hi = Uint32(regApiPtr->globalcheckpointid >> 32);
    Uint32* gci_lo = (Uint32*)&tcKeyConf->operations[TopWords >> 1];
    tcKeyConf->confInfo = confInfo;
    tcKeyConf->transId1 = regApiPtr->transid[0];
    tcKeyConf->transId2 = regApiPtr->transid[1];
    copyFromToLen(&regApiPtr->tcSendArray[0],
		  (UintR*)&tcKeyConf->operations,
		  (UintR)ZTCOPCONF_SIZE);
    * gci_lo = Uint32(regApiPtr->globalcheckpointid);
    sendSignal(regApiPtr->ndbapiBlockref,
	       GSN_TCKEYCONF, signal, (TpacketLen - 1) + 1 /** gci_lo */, JBB);
    return;
  } else if (((TcurrLen + TpacketLen + 1 /** gci_lo */) > 25) &&
             (TcurrLen > 0)) {
    jam();
    sendPackedTCKEYCONF(signal, localHostptr.p, localHostptr.i);
    TcurrLen = 0;
  } else {
    jam();
    updatePackedList(signal, localHostptr.p, localHostptr.i);
  }//if
  // -------------------------------------------------------------------------
  // The header contains the block reference of receiver plus the real signal
  // length - 3, since we have the real signal length plus one additional word
  // for the header we have to do - 4.
  // -------------------------------------------------------------------------
  container->noOfPackedWords = TcurrLen + TpacketLen + 1 /** gci_lo */;

  UintR Tpack0 = (TblockNum << 16) + (TpacketLen - 4 + 1 /** gci_lo */);
  UintR Tpack1 = regApiPtr->ndbapiConnect;
  UintR Tpack2 = Uint32(regApiPtr->globalcheckpointid >> 32);
  UintR Tpack3 = confInfo;
  UintR Tpack4 = regApiPtr->transid[0];
  UintR Tpack5 = regApiPtr->transid[1];
  UintR Tpack6 = Uint32(regApiPtr->globalcheckpointid);
  
  container->packedWords[TcurrLen + 0] = Tpack0;
  container->packedWords[TcurrLen + 1] = Tpack1;
  container->packedWords[TcurrLen + 2] = Tpack2;
  container->packedWords[TcurrLen + 3] = Tpack3;
  container->packedWords[TcurrLen + 4] = Tpack4;
  container->packedWords[TcurrLen + 5] = Tpack5;

  container->packedWords[TcurrLen + TpacketLen] = Tpack6;
  
  for (Uint32 Ti = 6; Ti < TpacketLen; Ti++) {
    container->packedWords[TcurrLen + Ti] = regApiPtr->tcSendArray[Ti - 6];
  }//for

  if (unlikely(!ndb_check_micro_gcp(getNodeInfo(localHostptr.i).m_version)))
  {
    jam();
    ndbassert(Tpack6 == 0 || 
              getNodeInfo(localHostptr.i).m_version == 0); // Disconnected
  }
}//Dbtc::sendtckeyconf()

void Dbtc::copyFromToLen(UintR* sourceBuffer, UintR* destBuffer, UintR Tlen)
{
  UintR Tindex = 0;
  UintR Ti;
  while (Tlen >= 4) {
    UintR Tdata0 = sourceBuffer[Tindex + 0];
    UintR Tdata1 = sourceBuffer[Tindex + 1];
    UintR Tdata2 = sourceBuffer[Tindex + 2];
    UintR Tdata3 = sourceBuffer[Tindex + 3];
    Tlen -= 4;
    destBuffer[Tindex + 0] = Tdata0;
    destBuffer[Tindex + 1] = Tdata1;
    destBuffer[Tindex + 2] = Tdata2;
    destBuffer[Tindex + 3] = Tdata3;
    Tindex += 4;
  }//while
  for (Ti = 0; Ti < Tlen; Ti++, Tindex++) {
    destBuffer[Tindex] = sourceBuffer[Tindex];
  }//for
}//Dbtc::copyFromToLen()

void Dbtc::execSEND_PACKED(Signal* signal) 
{
  HostRecordPtr Thostptr;
  HostRecord *localHostRecord = hostRecord;
  UintR i;
  UintR TpackedListIndex = cpackedListIndex;
  jamEntryDebug();
  for (i = 0; i < TpackedListIndex; i++) {
    jam();
    Thostptr.i = cpackedList[i];
    ptrAss(Thostptr, localHostRecord);
    arrGuard(Thostptr.i - 1, MAX_NODES - 1);
    for (Uint32 j = 0; j < NDB_ARRAY_SIZE(Thostptr.p->lqh_pack); j++)
    {
      struct PackedWordsContainer * container = &Thostptr.p->lqh_pack[j];
      jamDebug();
      if (container->noOfPackedWords > 0) {
        jamDebug();
        sendPackedSignal(signal, container);
      }
    }
    struct PackedWordsContainer * container = &Thostptr.p->packTCKEYCONF;
    if (container->noOfPackedWords > 0) {
      jamDebug();
      sendPackedTCKEYCONF(signal, Thostptr.p, (Uint32)Thostptr.i);
    }//if
    Thostptr.p->inPackedList = false;
  }//for
  cpackedListIndex = 0;
  return;
}//Dbtc::execSEND_PACKED()

void 
Dbtc::updatePackedList(Signal* signal, HostRecord* ahostptr, Uint16 ahostIndex)
{
  if (ahostptr->inPackedList == false) {
    UintR TpackedListIndex = cpackedListIndex;
    jamDebug();
    ahostptr->inPackedList = true;
    cpackedList[TpackedListIndex] = ahostIndex;
    cpackedListIndex = TpackedListIndex + 1;
  }//if
}//Dbtc::updatePackedList()

void Dbtc::sendPackedSignal(Signal* signal,
                            struct PackedWordsContainer * container)
{
  UintR TnoOfWords = container->noOfPackedWords;
  ndbassert(TnoOfWords <= 25);
  container->noOfPackedWords = 0;
  memcpy(&signal->theData[0], &container->packedWords[0], 4 * TnoOfWords);
  if (VERIFY_PACKED_SEND)
  {
    ndbrequire(refToMain(container->hostBlockRef) == DBLQH);
    ndbrequire(PackedSignal::verify(&signal->theData[0],
                                    TnoOfWords,
                                    container->hostBlockRef,
                                    LQH_RECEIVE_TYPES,
                                    5)); /* Commit signal length */
  }
  sendSignal(container->hostBlockRef,
             GSN_PACKED_SIGNAL,
             signal,
             TnoOfWords,
             JBB);
}//Dbtc::sendPackedSignal()

void Dbtc::sendPackedTCKEYCONF(Signal* signal,
                               HostRecord * ahostptr,
                               UintR hostId)
{
  struct PackedWordsContainer * container = &ahostptr->packTCKEYCONF;
  UintR TnoOfWords = container->noOfPackedWords;
  ndbassert(TnoOfWords <= 25);
  container->noOfPackedWords = 0;
  BlockReference TBref = numberToRef(API_PACKED, hostId);
  memcpy(&signal->theData[0], &container->packedWords[0], 4 * TnoOfWords);
  sendSignal(TBref, GSN_TCKEYCONF, signal, TnoOfWords, JBB);
}//Dbtc::sendPackedTCKEYCONF()

void
Dbtc::startSendFireTrigReq(Signal* signal, Ptr<ApiConnectRecord> regApiPtr)
{
  const Uint32 pass = regApiPtr.p->m_pre_commit_pass;
  const Uint32 step = pass & Uint32(TriggerPreCommitPass::TPCP_PASS_MAX);
  regApiPtr.p->pendingTriggers = 0;
  if (tc_testbit(regApiPtr.p->m_flags,
                 ApiConnectRecord::TF_DEFERRED_UK_TRIGGERS))
  {
    jam();
    if (step == TriggerPreCommitPass::UK_PASS_1)
    {
      jam();
      // clear on second pass...
      tc_clearbit(regApiPtr.p->m_flags,
                  ApiConnectRecord::TF_DEFERRED_UK_TRIGGERS);
    }
  }
  else
  {
    ndbassert(tc_testbit(regApiPtr.p->m_flags,
                         ApiConnectRecord::TF_DEFERRED_FK_TRIGGERS));
    tc_clearbit(regApiPtr.p->m_flags,
                ApiConnectRecord::TF_DEFERRED_FK_TRIGGERS);

    Uint32 newpass = pass;
    newpass = newpass & ~Uint32(TriggerPreCommitPass::TPCP_PASS_MAX);
    newpass = newpass + TriggerPreCommitPass::FK_PASS_0;
    regApiPtr.p->m_pre_commit_pass = newpass;
  }
  sendFireTrigReq(signal, regApiPtr,
                  regApiPtr.p->firstTcConnect,
                  regApiPtr.p->lastTcConnect);
}

/*
4.3.11 DIVERIFY 
---------------
*/
/*****************************************************************************/
/*                               D I V E R I F Y                             */
/*                                                                           */
/*****************************************************************************/
void Dbtc::diverify010Lab(Signal* signal) 
{
  UintR TfirstfreeApiConnectCopy = cfirstfreeApiConnectCopy;
  ApiConnectRecord * const regApiPtr = apiConnectptr.p;
  signal->theData[0] = apiConnectptr.i;
  signal->theData[1] = instance() ? instance() - 1 : 0;
  if (ERROR_INSERTED(8022)) {
    jam();
    systemErrorLab(signal, __LINE__);
  }//if

  if (tc_testbit(regApiPtr->m_flags,
                 (ApiConnectRecord::TF_DEFERRED_UK_TRIGGERS +
                  ApiConnectRecord::TF_DEFERRED_FK_TRIGGERS)))
  {
    /**
     * If trans has deferred triggers, let them fire just before
     *   transaction starts to commit
     */
    startSendFireTrigReq(signal, apiConnectptr);
    return;
  }

  ndbassert(!tc_testbit(regApiPtr->m_flags,
                        (ApiConnectRecord::TF_INDEX_OP_RETURN |
                         ApiConnectRecord::TF_TRIGGER_PENDING)));

  ndbassert(!tc_testbit(regApiPtr->m_flags,
                        (ApiConnectRecord::TF_INDEX_OP_RETURN |
                         ApiConnectRecord::TF_TRIGGER_PENDING)));

  if (regApiPtr->lqhkeyreqrec)
  {
    if (TfirstfreeApiConnectCopy != RNIL) {
      seizeApiConnectCopy(signal);
      regApiPtr->apiConnectstate = CS_PREPARE_TO_COMMIT;
      /*-----------------------------------------------------------------------
       * WE COME HERE ONLY IF THE TRANSACTION IS PREPARED ON ALL TC CONNECTIONS
       * THUS WE CAN START THE COMMIT PHASE BY SENDING DIVERIFY ON ALL TC     
       * CONNECTIONS AND THEN WHEN ALL DIVERIFYCONF HAVE BEEN RECEIVED THE 
       * COMMIT MESSAGE CAN BE SENT TO ALL INVOLVED PARTS.
       *---------------------------------------------------------------------*/
      * (EmulatedJamBuffer**)(signal->theData+2) = jamBuffer();
      EXECUTE_DIRECT_MT(DBDIH, GSN_DIVERIFYREQ, signal,
                        2 + sizeof(void*)/sizeof(Uint32), 0);
      if (clastApiConnectPREPARE_TO_COMMIT != RNIL ||
          signal->theData[3] != 0)
      {
        /* Put transaction last in verification queue */
        ndbrequire(regApiPtr->nextApiConnect == RNIL);
        if (clastApiConnectPREPARE_TO_COMMIT != RNIL)
        {
          ApiConnectRecord* apiPtr;
          apiPtr = &apiConnectRecord[clastApiConnectPREPARE_TO_COMMIT];
          ndbrequire(apiPtr->nextApiConnect == RNIL);
          apiPtr->nextApiConnect = apiConnectptr.i;
        }
        else
        {
          ndbassert(cfirstApiConnectPREPARE_TO_COMMIT == RNIL);
          cfirstApiConnectPREPARE_TO_COMMIT = apiConnectptr.i;
        }
        clastApiConnectPREPARE_TO_COMMIT = apiConnectptr.i;
        /**
         * If execDIVERIFYCONF is called below, make it pop a transaction
         * from verifiction queue.
         * Note, that even if DBDIH says ok without queue, DBTC can still
         * have a queue since there can be DIVERIFYCONF still in flight.
         */
        signal->theData[0] = RNIL;
      }
      if (signal->theData[3] == 0)
      {
        execDIVERIFYCONF(signal);
      }
      return;
    } else {
      /*-----------------------------------------------------------------------
       * There were no free copy connections available. We must abort the 
       * transaction since otherwise we will have a problem with the report 
       * to the application.
       * This should more or less not happen but if it happens we do 
       * not want to crash and we do not want to create code to handle it 
       * properly since it is difficult to test it and will be complex to 
       * handle a problem more or less not occurring.
       *---------------------------------------------------------------------*/
      terrorCode = ZSEIZE_API_COPY_ERROR;
      abortErrorLab(signal);
      return;
    }
  }
  else
  {
    jam();
    sendtckeyconf(signal, 1);
    regApiPtr->apiConnectstate = CS_CONNECTED;
    regApiPtr->m_transaction_nodes.clear();
    setApiConTimer(apiConnectptr.i, 0,__LINE__);
  }
}//Dbtc::diverify010Lab()

/* ------------------------------------------------------------------------- */
/* -------                  SEIZE_API_CONNECT                        ------- */
/*                  SEIZE CONNECT RECORD FOR A REQUEST                       */
/* ------------------------------------------------------------------------- */
void Dbtc::seizeApiConnectCopy(Signal* signal) 
{
  ApiConnectRecordPtr locApiConnectptr;

  ApiConnectRecord *localApiConnectRecord = apiConnectRecord;
  UintR TapiConnectFilesize = capiConnectFilesize;
  ApiConnectRecord * const regApiPtr = apiConnectptr.p;

  locApiConnectptr.i = cfirstfreeApiConnectCopy;
  ptrCheckGuard(locApiConnectptr, TapiConnectFilesize, localApiConnectRecord);
  cfirstfreeApiConnectCopy = locApiConnectptr.p->nextApiConnect;
  locApiConnectptr.p->nextApiConnect = RNIL;
  ndbassert(regApiPtr->apiCopyRecord == RNIL);
  regApiPtr->apiCopyRecord = locApiConnectptr.i;
  tc_clearbit(regApiPtr->m_flags,
              ApiConnectRecord::TF_TRIGGER_PENDING);
  regApiPtr->m_special_op_flags = 0;
}//Dbtc::seizeApiConnectCopy()

void Dbtc::execDIVERIFYCONF(Signal* signal) 
{
  UintR TapiConnectptrIndex = signal->theData[0];
  UintR TapiConnectFilesize = capiConnectFilesize;
  UintR Tgci_hi = signal->theData[1];
  UintR Tgci_lo = signal->theData[2];
  Uint64 Tgci = Tgci_lo | (Uint64(Tgci_hi) << 32);
  ApiConnectRecord *localApiConnectRecord = apiConnectRecord;

  jamEntry();
  if (TapiConnectptrIndex == RNIL)
  {
    /**
     * DIVERIFYCONF from DBDIH
     * There should be transactions queue for verification.
     */
    if (cfirstApiConnectPREPARE_TO_COMMIT == RNIL)
    {
      ndbassert(cfirstApiConnectPREPARE_TO_COMMIT != RNIL);
      return;
    }
    TapiConnectptrIndex = cfirstApiConnectPREPARE_TO_COMMIT;
  }
  else
  {
    /**
     * DIVERIFYCONF from DBTC
     * There should be no transactions queue for verification.
     */
    ndbrequire(cfirstApiConnectPREPARE_TO_COMMIT == RNIL);
  }
  if (ERROR_INSERTED(8017)) {
    CLEAR_ERROR_INSERT_VALUE;
    return;
  }//if
  if (TapiConnectptrIndex >= TapiConnectFilesize) {
    TCKEY_abort(signal, 31);
    return;
  }//if
  ApiConnectRecord * const regApiPtr = 
                            &localApiConnectRecord[TapiConnectptrIndex];
  if (cfirstApiConnectPREPARE_TO_COMMIT == TapiConnectptrIndex)
  {
    /* Pop first transaction in verification queue */
    cfirstApiConnectPREPARE_TO_COMMIT = regApiPtr->nextApiConnect;
    if (cfirstApiConnectPREPARE_TO_COMMIT == RNIL)
    {
      clastApiConnectPREPARE_TO_COMMIT = RNIL;
    }
    regApiPtr->nextApiConnect = RNIL;
  }
  ConnectionState TapiConnectstate = regApiPtr->apiConnectstate;
  UintR TApifailureNr = regApiPtr->failureNr;
  UintR Tfailure_nr = cfailure_nr;
  apiConnectptr.i = TapiConnectptrIndex;
  apiConnectptr.p = regApiPtr;
  if (TapiConnectstate != CS_PREPARE_TO_COMMIT) {
    TCKEY_abort(signal, 32);
    return;
  }//if
  /*--------------------------------------------------------------------------
   * THIS IS THE COMMIT POINT. IF WE ARRIVE HERE THE TRANSACTION IS COMMITTED 
   * UNLESS EVERYTHING CRASHES BEFORE WE HAVE BEEN ABLE TO REPORT THE COMMIT 
   * DECISION. THERE IS NO TURNING BACK FROM THIS DECISION FROM HERE ON.     
   * WE WILL INSERT THE TRANSACTION INTO ITS PROPER QUEUE OF 
   * TRANSACTIONS FOR ITS GLOBAL CHECKPOINT.              
   *-------------------------------------------------------------------------*/
  if (TApifailureNr != Tfailure_nr ||
      ERROR_INSERTED(8094)) {
    jam();
    DIVER_node_fail_handling(signal, Tgci);
    return;
  }//if
  commitGciHandling(signal, Tgci);

  /**************************************************************************
   *                                 C O M M I T                      
   * THE TRANSACTION HAVE NOW BEEN VERIFIED AND NOW THE COMMIT PHASE CAN START 
   **************************************************************************/

  if (ERROR_INSERTED(8110))
  {
    jam();
    /* Disconnect API now that commit decided upon */
    const Uint32 apiNodeId = refToNode(regApiPtr->ndbapiBlockref);
    if (getNodeInfo(apiNodeId).getType() == NodeInfo::API)
    {
      ndbout_c("Error insert 8110, disconnecting API during COMMIT");
      signal->theData[0] = apiNodeId;
      sendSignal(QMGR_REF, GSN_API_FAILREQ, signal, 1, JBA);

      SET_ERROR_INSERT_VALUE(8089); /* Slow committing... */
    }
  }

  UintR TtcConnectptrIndex = regApiPtr->firstTcConnect;
  UintR TtcConnectFilesize = ctcConnectFilesize;
  TcConnectRecord *localTcConnectRecord = tcConnectRecord;

  regApiPtr->counter = regApiPtr->lqhkeyconfrec;
  regApiPtr->apiConnectstate = CS_COMMITTING;
  if (TtcConnectptrIndex >= TtcConnectFilesize) {
    TCKEY_abort(signal, 33);
    return;
  }//if
  TcConnectRecord* const regTcPtr = &localTcConnectRecord[TtcConnectptrIndex];
  tcConnectptr.i = TtcConnectptrIndex;
  tcConnectptr.p = regTcPtr;
  commit020Lab(signal);
}//Dbtc::execDIVERIFYCONF()

/*--------------------------------------------------------------------------*/
/*                             COMMIT_GCI_HANDLING                          */
/*       SET UP GLOBAL CHECKPOINT DATA STRUCTURE AT THE COMMIT POINT.       */
/*--------------------------------------------------------------------------*/
void Dbtc::commitGciHandling(Signal* signal, Uint64 Tgci)
{
  GcpRecordPtr localGcpPointer;

  UintR TgcpFilesize = cgcpFilesize;
  UintR Tfirstgcp = cfirstgcp;
  Ptr<ApiConnectRecord> regApiPtr = apiConnectptr;
  GcpRecord *localGcpRecord = gcpRecord;

  regApiPtr.p->globalcheckpointid = Tgci;
  if (Tfirstgcp != RNIL) {
    /* IF THIS GLOBAL CHECKPOINT ALREADY EXISTS */
    localGcpPointer.i = Tfirstgcp;
    ptrCheckGuard(localGcpPointer, TgcpFilesize, localGcpRecord);
    do {
      if (regApiPtr.p->globalcheckpointid == localGcpPointer.p->gcpId) {
        jam();
        linkApiToGcp(localGcpPointer, regApiPtr);
        return;
      } else {
        if (unlikely(! (regApiPtr.p->globalcheckpointid > localGcpPointer.p->gcpId)))
        {
          ndbout_c("%u/%u %u/%u",
                   Uint32(regApiPtr.p->globalcheckpointid >> 32),
                   Uint32(regApiPtr.p->globalcheckpointid),
                   Uint32(localGcpPointer.p->gcpId >> 32),
                   Uint32(localGcpPointer.p->gcpId));
          crash_gcp(__LINE__);
        }
        localGcpPointer.i = localGcpPointer.p->nextGcp;
        jam();
        if (localGcpPointer.i != RNIL) {
          jam();
          ptrCheckGuard(localGcpPointer, TgcpFilesize, localGcpRecord);
          continue;
        }//if
      }//if
      seizeGcp(localGcpPointer, Tgci);
      linkApiToGcp(localGcpPointer, regApiPtr);
      return;
    } while (1);
  } else {
    jam();
    seizeGcp(localGcpPointer, Tgci);
    linkApiToGcp(localGcpPointer, regApiPtr);
  }//if
}//Dbtc::commitGciHandling()

/* --------------------------------------------------------------------------*/
/* -LINK AN API CONNECT RECORD IN STATE PREPARED INTO THE LIST WITH GLOBAL - */
/* CHECKPOINTS. WHEN THE TRANSACTION I COMPLETED THE API CONNECT RECORD IS   */
/* LINKED OUT OF THE LIST.                                                   */
/*---------------------------------------------------------------------------*/
void Dbtc::linkApiToGcp(Ptr<GcpRecord> regGcpPtr,
                        Ptr<ApiConnectRecord> regApiPtr)
{
  ApiConnectRecordPtr localApiConnectptr;
  ApiConnectRecord *localApiConnectRecord = apiConnectRecord;

  regApiPtr.p->nextGcpConnect = RNIL;
  if (regGcpPtr.p->firstApiConnect == RNIL) {
    regGcpPtr.p->firstApiConnect = regApiPtr.i;
    jam();
  } else {
    UintR TapiConnectFilesize = capiConnectFilesize;
    localApiConnectptr.i = regGcpPtr.p->lastApiConnect;
    jam();
    ptrCheckGuard(localApiConnectptr,
                  TapiConnectFilesize, localApiConnectRecord);
    localApiConnectptr.p->nextGcpConnect = regApiPtr.i;
  }//if
  UintR TlastApiConnect = regGcpPtr.p->lastApiConnect;
  regApiPtr.p->gcpPointer = regGcpPtr.i;
  regApiPtr.p->prevGcpConnect = TlastApiConnect;
  regGcpPtr.p->lastApiConnect = regApiPtr.i;
}//Dbtc::linkApiToGcp()

void
Dbtc::crash_gcp(Uint32 line)
{
  GcpRecordPtr localGcpPointer;

  localGcpPointer.i = cfirstgcp;

  while (localGcpPointer.i != RNIL)
  {
    ptrCheckGuard(localGcpPointer, cgcpFilesize, gcpRecord);
    ndbout_c("%u : %u/%u nomoretrans: %u api %u %u next: %u",
             localGcpPointer.i,
             Uint32(localGcpPointer.p->gcpId >> 32),
             Uint32(localGcpPointer.p->gcpId),             
             localGcpPointer.p->gcpNomoretransRec,
             localGcpPointer.p->firstApiConnect,
             localGcpPointer.p->lastApiConnect,
             localGcpPointer.p->nextGcp);
    localGcpPointer.i = localGcpPointer.p->nextGcp;
  }
  progError(line, NDBD_EXIT_NDBREQUIRE);
  ndbabort();
}

void Dbtc::seizeGcp(Ptr<GcpRecord> & dst, Uint64 Tgci)
{
  GcpRecordPtr tmpGcpPointer;
  GcpRecordPtr localGcpPointer;

  UintR Tfirstgcp = cfirstgcp;
  UintR TgcpFilesize = cgcpFilesize;
  GcpRecord *localGcpRecord = gcpRecord;

  localGcpPointer.i = cfirstfreeGcp;
  if (unlikely(localGcpPointer.i > TgcpFilesize))
  {
    ndbout_c("%u/%u", Uint32(Tgci >> 32), Uint32(Tgci));
    crash_gcp(__LINE__);
  }
  ptrCheckGuard(localGcpPointer, TgcpFilesize, localGcpRecord);
  UintR TfirstfreeGcp = localGcpPointer.p->nextGcp;
  localGcpPointer.p->gcpId = Tgci;
  localGcpPointer.p->nextGcp = RNIL;
  localGcpPointer.p->firstApiConnect = RNIL;
  localGcpPointer.p->lastApiConnect = RNIL;
  localGcpPointer.p->gcpNomoretransRec = ZFALSE;
  cfirstfreeGcp = TfirstfreeGcp;

  if (Tfirstgcp == RNIL) {
    jam();
    cfirstgcp = localGcpPointer.i;
  } else {
    tmpGcpPointer.i = clastgcp;
    jam();
    ptrCheckGuard(tmpGcpPointer, TgcpFilesize, localGcpRecord);
    tmpGcpPointer.p->nextGcp = localGcpPointer.i;
  }//if
  clastgcp = localGcpPointer.i;
  dst = localGcpPointer;
}//Dbtc::seizeGcp()

/*---------------------------------------------------------------------------*/
// Send COMMIT messages to all LQH operations involved in the transaction.
/*---------------------------------------------------------------------------*/
void Dbtc::commit020Lab(Signal* signal) 
{
  TcConnectRecordPtr localTcConnectptr;
  ApiConnectRecord * const regApiPtr = apiConnectptr.p;
  UintR TtcConnectFilesize = ctcConnectFilesize;
  TcConnectRecord *localTcConnectRecord = tcConnectRecord;

  localTcConnectptr.p = tcConnectptr.p;
  setApiConTimer(apiConnectptr.i, ctcTimer, __LINE__);
  UintR Tcount = 0;
  do {
    /*-----------------------------------------------------------------------
     * WE ARE NOW READY TO RELEASE ALL OPERATIONS ON THE LQH                  
     *-----------------------------------------------------------------------*/
    /* *********< */
    /*  COMMIT  < */
    /* *********< */
    localTcConnectptr.i = localTcConnectptr.p->nextTcConnect;
    /* Clear per-operation record CommitAckMarker ref if necessary */
    if (localTcConnectptr.p->commitAckMarker != RNIL)
    {
      ndbassert(regApiPtr->commitAckMarker != RNIL);
      ndbrequire(regApiPtr->num_commit_ack_markers--);
      localTcConnectptr.p->commitAckMarker = RNIL;
    }
    localTcConnectptr.p->tcConnectstate = OS_COMMITTING;
    Tcount += sendCommitLqh(signal, localTcConnectptr.p);

    if (localTcConnectptr.i != RNIL) {
      if (Tcount < 16 &&
          ! (ERROR_INSERTED(8057) ||
             ERROR_INSERTED(8073) ||
             ERROR_INSERTED(8089)))
      {
        ptrCheckGuard(localTcConnectptr,
                      TtcConnectFilesize, localTcConnectRecord);
        jam();
        continue;
      } else {
        jam();
        if (ERROR_INSERTED(8014)) {
          CLEAR_ERROR_INSERT_VALUE;
          return;
        }//if
        
        if (ERROR_INSERTED(8073))
        {
          execSEND_PACKED(signal);
          signal->theData[0] = 9999;
          sendSignalWithDelay(CMVMI_REF, GSN_NDB_TAMPER, signal, 100, 1);
          return;
        }
        signal->theData[0] = TcContinueB::ZSEND_COMMIT_LOOP;
        signal->theData[1] = apiConnectptr.i;
        signal->theData[2] = localTcConnectptr.i;
        if (ERROR_INSERTED(8089))
        {
          sendSignalWithDelay(cownref, GSN_CONTINUEB, signal, 100, 3);
          return;
        }
        sendSignal(cownref, GSN_CONTINUEB, signal, 3, JBB);
        return;
      }//if
    } else {
      jam();
      if (ERROR_INSERTED(8057))
        CLEAR_ERROR_INSERT_VALUE;

      if (ERROR_INSERTED(8089))
        CLEAR_ERROR_INSERT_VALUE;

      regApiPtr->apiConnectstate = CS_COMMIT_SENT;
      ndbrequire(regApiPtr->num_commit_ack_markers == 0);
      return;
    }//if
  } while (1);
}//Dbtc::commit020Lab()

Uint32
Dbtc::sendCommitLqh(Signal* signal,
                    TcConnectRecord * const regTcPtr)
{
  HostRecordPtr Thostptr;
  UintR ThostFilesize = chostFilesize;
  Uint32 instanceKey = regTcPtr->lqhInstanceKey;
  ApiConnectRecord * const regApiPtr = apiConnectptr.p;
  Thostptr.i = regTcPtr->lastLqhNodeId;
  ptrCheckGuard(Thostptr, ThostFilesize, hostRecord);

  if (ERROR_INSERTED(8113))
  {
    jam();
    /* Don't actually send -> timeout */
    return 0;
  }

  Uint32 Tnode = Thostptr.i;
  Uint32 self = getOwnNodeId();
  Uint32 ret = (Tnode == self) ? 4 : 1;

  Uint32 Tdata[5];
  Tdata[0] = regTcPtr->lastLqhCon;
  Tdata[1] = Uint32(regApiPtr->globalcheckpointid >> 32);
  Tdata[2] = regApiPtr->transid[0];
  Tdata[3] = regApiPtr->transid[1];
  Tdata[4] = Uint32(regApiPtr->globalcheckpointid);
  Uint32 len = 5;

  if (unlikely(!ndb_check_micro_gcp(getNodeInfo(Thostptr.i).m_version)))
  {
    jam();
    ndbassert(Tdata[4] == 0 || getNodeInfo(Thostptr.i).m_version == 0);
    len = 4;
  }
  if (instanceKey > MAX_NDBMT_LQH_THREADS) {
    memcpy(&signal->theData[0], &Tdata[0], len << 2);
    BlockReference lqhRef = numberToRef(DBLQH, instanceKey, Tnode);
    sendSignal(lqhRef, GSN_COMMIT, signal, len, JBB);
    return ret;
  }

  struct PackedWordsContainer * container = &Thostptr.p->lqh_pack[instanceKey];

  if (container->noOfPackedWords > 25 - len) {
    jam();
    sendPackedSignal(signal, container);
  } else {
    jam();
    ret = 1;
    updatePackedList(signal, Thostptr.p, Thostptr.i);
  }

  Tdata[0] |= (ZCOMMIT << 28);
  UintR Tindex = container->noOfPackedWords;
  container->noOfPackedWords = Tindex + len;
  UintR* TDataPtr = &container->packedWords[Tindex];
  memcpy(TDataPtr, &Tdata[0], len << 2);
  return ret;
}

void
Dbtc::DIVER_node_fail_handling(Signal* signal, Uint64 Tgci)
{
  /*------------------------------------------------------------------------
   * AT LEAST ONE NODE HAS FAILED DURING THE TRANSACTION. WE NEED TO CHECK IF  
   * THIS IS SO SERIOUS THAT WE NEED TO ABORT THE TRANSACTION. IN BOTH THE     
   * ABORT AND THE COMMIT CASES WE NEED  TO SET-UP THE DATA FOR THE           
   * ABORT/COMMIT/COMPLETE  HANDLING AS ALSO USED BY TAKE OVER FUNCTIONALITY. 
   *------------------------------------------------------------------------*/
  tabortInd = ZFALSE;
  setupFailData(signal);
  if (false && tabortInd == ZFALSE) {
    jam();
    commitGciHandling(signal, Tgci);
    toCommitHandlingLab(signal);
  } else {
    jam();
    apiConnectptr.p->returnsignal = RS_TCROLLBACKREP;
    apiConnectptr.p->returncode = ZNODEFAIL_BEFORE_COMMIT;
    toAbortHandlingLab(signal);
  }//if
  return;
}//Dbtc::DIVER_node_fail_handling()


/* ------------------------------------------------------------------------- */
/* -------                       ENTER COMMITTED                     ------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void Dbtc::execCOMMITTED(Signal* signal) 
{
  TcConnectRecordPtr localTcConnectptr;
  ApiConnectRecordPtr localApiConnectptr;
  ApiConnectRecordPtr localCopyPtr;

  UintR TtcConnectFilesize = ctcConnectFilesize;
  UintR TapiConnectFilesize = capiConnectFilesize;
  TcConnectRecord *localTcConnectRecord = tcConnectRecord;
  ApiConnectRecord *localApiConnectRecord = apiConnectRecord;

#ifdef ERROR_INSERT
  if (ERROR_INSERTED(8018)) {
    CLEAR_ERROR_INSERT_VALUE;
    return;
  }//if
  CRASH_INSERTION(8030);
  if (ERROR_INSERTED(8025)) {
    SET_ERROR_INSERT_VALUE(8026);
    return;
  }//if
  if (ERROR_INSERTED(8041)) {
    CLEAR_ERROR_INSERT_VALUE;
    sendSignalWithDelay(cownref, GSN_COMMITTED, signal, 2000, 3);
    return;
  }//if
  if (ERROR_INSERTED(8042)) {
    SET_ERROR_INSERT_VALUE(8046);
    sendSignalWithDelay(cownref, GSN_COMMITTED, signal, 2000, 4);
    return;
  }//if
#endif
  localTcConnectptr.i = signal->theData[0];
  jamEntry();
  ptrCheckGuard(localTcConnectptr, TtcConnectFilesize, localTcConnectRecord);
  localApiConnectptr.i = localTcConnectptr.p->apiConnect;
  if (localTcConnectptr.p->tcConnectstate != OS_COMMITTING) {
    warningReport(signal, 4);
    return;
  }//if
  ptrCheckGuard(localApiConnectptr, TapiConnectFilesize, 
		localApiConnectRecord);
  UintR Tcounter = localApiConnectptr.p->counter - 1;
  ConnectionState TapiConnectstate = localApiConnectptr.p->apiConnectstate;
  UintR Tdata1 = localApiConnectptr.p->transid[0] - signal->theData[1];
  UintR Tdata2 = localApiConnectptr.p->transid[1] - signal->theData[2];
  Tdata1 = Tdata1 | Tdata2;
  bool TcheckCondition = 
    (TapiConnectstate != CS_COMMIT_SENT) || (Tcounter != 0);

  setApiConTimer(localApiConnectptr.i, ctcTimer, __LINE__);
  localApiConnectptr.p->counter = Tcounter;
  localTcConnectptr.p->tcConnectstate = OS_COMMITTED;
  if (Tdata1 != 0) {
    warningReport(signal, 5);
    return;
  }//if
  if (TcheckCondition) {
    jam();
    /*-------------------------------------------------------*/
    // We have not sent all COMMIT requests yet. We could be
    // in the state that all sent are COMMITTED but we are
    // still waiting for a CONTINUEB to send the rest of the
    // COMMIT requests.
    /*-------------------------------------------------------*/
    return;
  }//if
  if (ERROR_INSERTED(8020)) {
    jam();
    systemErrorLab(signal, __LINE__);
  }//if
  /*-------------------------------------------------------*/
  /* THE ENTIRE TRANSACTION IS NOW COMMITED                */
  /* NOW WE NEED TO SEND THE RESPONSE TO THE APPLICATION.  */
  /* THE APPLICATION CAN THEN REUSE THE API CONNECTION AND */
  /* THEREFORE WE NEED TO MOVE THE API CONNECTION TO A     */
  /* NEW API CONNECT RECORD.                               */
  /*-------------------------------------------------------*/

  apiConnectptr = localApiConnectptr;
  localCopyPtr = sendApiCommitAndCopy(signal);

  localTcConnectptr.i = localCopyPtr.p->firstTcConnect;
  UintR Tlqhkeyconfrec = localCopyPtr.p->lqhkeyconfrec;
  ptrCheckGuard(localTcConnectptr, TtcConnectFilesize, localTcConnectRecord);
  localCopyPtr.p->counter = Tlqhkeyconfrec;
  
  if (ERROR_INSERTED(8111))
  {
    jam();
    /* Disconnect API now that entering the Complete phase */
    const Uint32 apiNodeId = refToNode(apiConnectptr.p->ndbapiBlockref);
    if (getNodeInfo(apiNodeId).getType() == NodeInfo::API)
    {
      ndbout_c("Error insert 8111, disconnecting API during COMPLETE");
      signal->theData[0] = apiNodeId;
      sendSignal(QMGR_REF, GSN_API_FAILREQ, signal, 1, JBA);

      SET_ERROR_INSERT_VALUE(8112); /* Slow completing... */
    }
  }

  apiConnectptr = localCopyPtr;
  tcConnectptr = localTcConnectptr;
  complete010Lab(signal);
  return;

}//Dbtc::execCOMMITTED()

void
Dbtc::sendApiCommitSignal(Signal *signal, Ptr<ApiConnectRecord> regApiPtr)
{
  ReturnSignal save = regApiPtr.p->returnsignal;

  if (tc_testbit(regApiPtr.p->m_flags, ApiConnectRecord::TF_LATE_COMMIT))
  {
    jam();
    regApiPtr.p->returnsignal = RS_NO_RETURN;
  }
  if (regApiPtr.p->returnsignal == RS_TCKEYCONF)
  {
    if (ERROR_INSERTED(8054))
    {
      signal->theData[0] = 9999;
      sendSignalWithDelay(CMVMI_REF, GSN_NDB_TAMPER, signal, 5000, 1);
    }
    else
    {
      sendtckeyconf(signal, 1);
    }
  }
  else if (regApiPtr.p->returnsignal == RS_TC_COMMITCONF) 
  {
    jam();
    TcCommitConf * const commitConf = (TcCommitConf *)&signal->theData[0];
    if(regApiPtr.p->commitAckMarker == RNIL)
    {
      jam();
      commitConf->apiConnectPtr = regApiPtr.p->ndbapiConnect;
    } 
    else 
    {
      jam();
      commitConf->apiConnectPtr = regApiPtr.p->ndbapiConnect | 1;
    }
    commitConf->transId1 = regApiPtr.p->transid[0];
    commitConf->transId2 = regApiPtr.p->transid[1];
    commitConf->gci_hi = Uint32(regApiPtr.p->globalcheckpointid >> 32);
    commitConf->gci_lo = Uint32(regApiPtr.p->globalcheckpointid);

    if (!ERROR_INSERTED(8054) && !ERROR_INSERTED(8108))
    {
      sendSignal(regApiPtr.p->ndbapiBlockref, GSN_TC_COMMITCONF, signal,
                 TcCommitConf::SignalLength, JBB);
    }
    time_track_complete_transaction(regApiPtr.p);
  }
  else if (regApiPtr.p->returnsignal == RS_NO_RETURN) 
  {
    jam();
  } 
  else 
  {
    regApiPtr.p->returnsignal = save;
    TCKEY_abort(signal, 37);
    return;
  }//if
  regApiPtr.p->returnsignal = save;
}

/*-------------------------------------------------------*/
/*                       SEND_API_COMMIT                 */
/*       SEND COMMIT DECISION TO THE API.                */
/*-------------------------------------------------------*/
Ptr<Dbtc::ApiConnectRecord>
Dbtc::sendApiCommitAndCopy(Signal* signal)
{
  ApiConnectRecordPtr regApiPtr = apiConnectptr;

  if (ERROR_INSERTED(8055))
  {
    /**
     * 1) Kill self
     * 2) Disconnect API
     * 3) Prevent execAPI_FAILREQ from handling trans...
     */
    signal->theData[0] = 9999;
    sendSignalWithDelay(CMVMI_REF, GSN_NDB_TAMPER, signal, 1000, 1);
    Uint32 node = refToNode(regApiPtr.p->ndbapiBlockref);
    signal->theData[0] = node;
    sendSignal(QMGR_REF, GSN_API_FAILREQ, signal, 1, JBB);

    SET_ERROR_INSERT_VALUE(8056);

    goto err8055;
  }

  sendApiCommitSignal(signal, regApiPtr);

err8055:
  Ptr<ApiConnectRecord> copyPtr;
  UintR TapiConnectFilesize = capiConnectFilesize;
  /**
   * Unlink copy connect record from main connect record to allow main record 
   * re-use.
   */
  copyPtr.i = regApiPtr.p->apiCopyRecord;
  regApiPtr.p->apiCopyRecord = RNIL;
  UintR TapiFailState = regApiPtr.p->apiFailState;
  ApiConnectRecord *localApiConnectRecord = apiConnectRecord;

  c_counters.ccommitCount++;
  ptrCheckGuard(copyPtr, TapiConnectFilesize, localApiConnectRecord);
  copyApi(copyPtr, regApiPtr);
  if (TapiFailState == ApiConnectRecord::AFS_API_OK)
  {
    return copyPtr;
  }
  else
  {
    jam();
    handleApiFailState(signal, regApiPtr.i);
    return copyPtr;
  }//if
}//Dbtc::sendApiCommitAndCopy()

/* ========================================================================= */
/* =======                  COPY_API                                 ======= */
/*   COPY API RECORD ALSO RESET THE OLD API RECORD SO THAT IT                */
/*   IS PREPARED TO RECEIVE A NEW TRANSACTION.                               */
/*===========================================================================*/
void Dbtc::copyApi(ApiConnectRecordPtr copyPtr, ApiConnectRecordPtr regApiPtr)
{
  UintR TndbapiConnect = regApiPtr.p->ndbapiConnect;
  UintR TfirstTcConnect = regApiPtr.p->firstTcConnect;
  UintR Ttransid1 = regApiPtr.p->transid[0];
  UintR Ttransid2 = regApiPtr.p->transid[1];
  UintR Tlqhkeyconfrec = regApiPtr.p->lqhkeyconfrec;
  UintR TgcpPointer = regApiPtr.p->gcpPointer;
  UintR TgcpFilesize = cgcpFilesize;
  Uint32 Tmarker = regApiPtr.p->commitAckMarker;
  NdbNodeBitmask Tnodes = regApiPtr.p->m_transaction_nodes;
  GcpRecord *localGcpRecord = gcpRecord;

  copyPtr.p->ndbapiBlockref = regApiPtr.p->ndbapiBlockref;
  copyPtr.p->ndbapiConnect = TndbapiConnect;
  copyPtr.p->firstTcConnect = TfirstTcConnect;
  copyPtr.p->apiConnectstate = CS_COMPLETING;
  copyPtr.p->transid[0] = Ttransid1;
  copyPtr.p->transid[1] = Ttransid2;
  copyPtr.p->lqhkeyconfrec = Tlqhkeyconfrec;
  copyPtr.p->commitAckMarker = RNIL;
  copyPtr.p->m_transaction_nodes = Tnodes;
  copyPtr.p->num_commit_ack_markers = 0;
  copyPtr.p->singleUserMode = 0;

  Ptr<GcpRecord> gcpPtr;
  gcpPtr.i = TgcpPointer;
  ptrCheckGuard(gcpPtr, TgcpFilesize, localGcpRecord);
  unlinkApiConnect(gcpPtr, regApiPtr);
  linkApiToGcp(gcpPtr, copyPtr);
  setApiConTimer(regApiPtr.i, 0, __LINE__);
  regApiPtr.p->apiConnectstate = CS_CONNECTED;
  regApiPtr.p->commitAckMarker = RNIL;
  regApiPtr.p->firstTcConnect = RNIL;
  regApiPtr.p->lastTcConnect = RNIL;
  regApiPtr.p->m_transaction_nodes.clear();
  regApiPtr.p->singleUserMode = 0;
  regApiPtr.p->num_commit_ack_markers = 0;
  releaseAllSeizedIndexOperations(regApiPtr.p);

  ndbassert(!tc_testbit(regApiPtr.p->m_flags,
                        (ApiConnectRecord::TF_INDEX_OP_RETURN |
                         ApiConnectRecord::TF_TRIGGER_PENDING)));

  if (tc_testbit(regApiPtr.p->m_flags, ApiConnectRecord::TF_LATE_COMMIT))
  {
    jam();
    if (unlikely(regApiPtr.p->apiFailState != ApiConnectRecord::AFS_API_OK))
    {
      jam();
      /* API failed during commit, no need to bother with LATE_COMMIT */
      regApiPtr.p->m_flags &= ~ Uint32(ApiConnectRecord::TF_LATE_COMMIT);

      /* Normal API failure handling will find + remove COMMIT_ACK_MARKER */
    }
    else
    { 
      /**
       * Put pointer from copyPtr to regApiPtr
       */
      copyPtr.p->apiCopyRecord = regApiPtr.i;
      
      /**
       * Modify state of regApiPtr to prevent future operations...
       *   timeout is already reset to 0 above so it won't be found
       *   by timeOutLoopStartLab
       */
      regApiPtr.p->apiConnectstate = CS_COMMITTING;
      
      /**
       * save marker for sendApiCommitSignal
       */
      copyPtr.p->commitAckMarker = Tmarker;
      
      /**
       * Set ApiConnectRecord::TF_LATE_COMMIT on copyPtr so remember
       *  to call sendApiCommitSignal at receive of COMPLETED signal
       */
      copyPtr.p->m_flags |= ApiConnectRecord::TF_LATE_COMMIT;
    }
  }

}//Dbtc::copyApi()

void Dbtc::unlinkApiConnect(Ptr<GcpRecord> gcpPtr,
                            Ptr<ApiConnectRecord> regApiPtr)
{
  ApiConnectRecordPtr localApiConnectptr;
  UintR TapiConnectFilesize = capiConnectFilesize;
  UintR TprevGcpConnect = regApiPtr.p->prevGcpConnect;
  UintR TnextGcpConnect = regApiPtr.p->nextGcpConnect;
  ApiConnectRecord *localApiConnectRecord = apiConnectRecord;

  if (TprevGcpConnect == RNIL) {
    gcpPtr.p->firstApiConnect = TnextGcpConnect;
    jam();
  } else {
    localApiConnectptr.i = TprevGcpConnect;
    jam();
    ptrCheckGuard(localApiConnectptr,
                  TapiConnectFilesize, localApiConnectRecord);
    localApiConnectptr.p->nextGcpConnect = TnextGcpConnect;
  }//if
  if (TnextGcpConnect == RNIL) {
    gcpPtr.p->lastApiConnect = TprevGcpConnect;
    jam();
  } else {
    localApiConnectptr.i = TnextGcpConnect;
    jam();
    ptrCheckGuard(localApiConnectptr,
                  TapiConnectFilesize, localApiConnectRecord);
    localApiConnectptr.p->prevGcpConnect = TprevGcpConnect;
  }//if
}//Dbtc::unlinkApiConnect()

void Dbtc::complete010Lab(Signal* signal) 
{
  TcConnectRecordPtr localTcConnectptr;
  ApiConnectRecord * const regApiPtr = apiConnectptr.p;
  UintR TtcConnectFilesize = ctcConnectFilesize;
  TcConnectRecord *localTcConnectRecord = tcConnectRecord;

  localTcConnectptr.p = tcConnectptr.p;
  setApiConTimer(apiConnectptr.i, ctcTimer, __LINE__);
  UintR TapiConnectptrIndex = apiConnectptr.i;
  UintR Tcount = 0;
  do {
    localTcConnectptr.p->apiConnect = TapiConnectptrIndex;
    localTcConnectptr.p->tcConnectstate = OS_COMPLETING;

    /* ************ */
    /*  COMPLETE  < */
    /* ************ */
    const Uint32 nextTcConnect = localTcConnectptr.p->nextTcConnect;
    Tcount += sendCompleteLqh(signal, localTcConnectptr.p);
    localTcConnectptr.i = nextTcConnect;
    if (localTcConnectptr.i != RNIL) {
      if (Tcount < 16 &&
          !ERROR_INSERTED(8112)) 
      {
        ptrCheckGuard(localTcConnectptr,
                      TtcConnectFilesize, localTcConnectRecord);
        jam();
        continue;
      } else {
        jam();
        if (ERROR_INSERTED(8013)) {
          CLEAR_ERROR_INSERT_VALUE;
          return;
        }//if
        signal->theData[0] = TcContinueB::ZSEND_COMPLETE_LOOP;
        signal->theData[1] = apiConnectptr.i;
        signal->theData[2] = localTcConnectptr.i;
        if (ERROR_INSERTED(8112))
        {
          sendSignalWithDelay(cownref, GSN_CONTINUEB, signal, 100, 3);
          return;
        }
        sendSignal(cownref, GSN_CONTINUEB, signal, 3, JBB);
        return;
      }//if
    } else {
      jam();
      regApiPtr->apiConnectstate = CS_COMPLETE_SENT;
      
      if (ERROR_INSERTED(8112))
      {
        CLEAR_ERROR_INSERT_VALUE;
      }
      
      return;
    }//if
  } while (1);
}//Dbtc::complete010Lab()

Uint32
Dbtc::sendCompleteLqh(Signal* signal,
                      TcConnectRecord * const regTcPtr)
{
  HostRecordPtr Thostptr;
  UintR ThostFilesize = chostFilesize;
  Uint32 instanceKey = regTcPtr->lqhInstanceKey;
  ApiConnectRecord * const regApiPtr = apiConnectptr.p;
  Thostptr.i = regTcPtr->lastLqhNodeId;
  ptrCheckGuard(Thostptr, ThostFilesize, hostRecord);

  if (ERROR_INSERTED(8114))
  {
    jam();
    /* Don't send COMPLETE to LQH ---> TIMEOUT */
    return 0;
  }

  Uint32 Tnode = Thostptr.i;
  Uint32 self = getOwnNodeId();
  Uint32 ret = (Tnode == self) ? 4 : 1;

  Uint32 Tdata[3];
  Tdata[0] = regTcPtr->lastLqhCon;
  Tdata[1] = regApiPtr->transid[0];
  Tdata[2] = regApiPtr->transid[1];
  Uint32 len = 3;

  if (instanceKey > MAX_NDBMT_LQH_THREADS) {
    memcpy(&signal->theData[0], &Tdata[0], len << 2);
    BlockReference lqhRef = numberToRef(DBLQH, instanceKey, Tnode);
    sendSignal(lqhRef, GSN_COMPLETE, signal, 3, JBB);
    return ret;
  }
                          
  struct PackedWordsContainer * container = &Thostptr.p->lqh_pack[instanceKey];

  if (container->noOfPackedWords > 22) {
    jam();
    sendPackedSignal(signal, container);
  } else {
    jam();
    ret = 1;
    updatePackedList(signal, Thostptr.p, Thostptr.i);
  }

  Tdata[0] |= (ZCOMPLETE << 28);
  UintR Tindex = container->noOfPackedWords;
  container->noOfPackedWords = Tindex + len;
  UintR* TDataPtr = &container->packedWords[Tindex];
  memcpy(TDataPtr, &Tdata[0], len << 2);
  return ret;
}

static
inline
Uint32
getTcConnectRecordDeferredFlag(Uint32 pass)
{
  switch(pass & TriggerPreCommitPass::TPCP_PASS_MAX){
  case TriggerPreCommitPass::UK_PASS_0:
  case TriggerPreCommitPass::UK_PASS_1:
    return Dbtc::TcConnectRecord::SOF_DEFERRED_UK_TRIGGER;
  case TriggerPreCommitPass::FK_PASS_0:
    return Dbtc::TcConnectRecord::SOF_DEFERRED_FK_TRIGGER;
  }
  assert(false);
  return 0;
}

static
inline
Uint8
getNextDeferredPass(Uint32 pass)
{
  Uint32 loop = (pass & ~Uint32(TriggerPreCommitPass::TPCP_PASS_MAX));
  switch(pass & TriggerPreCommitPass::TPCP_PASS_MAX){
  case TriggerPreCommitPass::UK_PASS_0:
    return loop + TriggerPreCommitPass::UK_PASS_1;
  case TriggerPreCommitPass::UK_PASS_1:
    return loop + TriggerPreCommitPass::FK_PASS_0;
  case TriggerPreCommitPass::FK_PASS_0:
    return loop + Uint32(TriggerPreCommitPass::TPCP_PASS_MAX) + 1;
  }
  assert(false);
  return 255;
}

void
Dbtc::sendFireTrigReq(Signal* signal,
                      Ptr<ApiConnectRecord> regApiPtr,
                      Uint32 TopPtrI,
                      Uint32 TlastOpPtrI)
{
  UintR TtcConnectFilesize = ctcConnectFilesize;
  TcConnectRecord *localTcConnectRecord = tcConnectRecord;
  TcConnectRecordPtr localTcConnectptr;

  setApiConTimer(regApiPtr.i, ctcTimer, __LINE__);
  regApiPtr.p->apiConnectstate = CS_SEND_FIRE_TRIG_REQ;

  localTcConnectptr.i = TopPtrI;
  ndbassert(TopPtrI != RNIL);
  Uint32 Tlqhkeyreqrec = regApiPtr.p->lqhkeyreqrec;
  const Uint32 pass = regApiPtr.p->m_pre_commit_pass;
  const Uint32 passflag = getTcConnectRecordDeferredFlag(pass);
  Uint32 prevOpPtrI = RNIL;
#if defined VM_TRACE || defined ERROR_INSERT
  const Uint32 LIMIT = 1 + rand() % 15;
#else
  const Uint32 LIMIT = 16;
#endif
  for (Uint32 i = 0; prevOpPtrI != TlastOpPtrI && i < LIMIT; i++)
  {
    ptrCheckGuard(localTcConnectptr,
                  TtcConnectFilesize, localTcConnectRecord);

    const Uint32 nextTcConnect = localTcConnectptr.p->nextTcConnect;
    Uint32 flags = localTcConnectptr.p->m_special_op_flags;
    if (flags & passflag)
    {
      jam();
      tc_clearbit(flags, passflag);
      if (unlikely(! (localTcConnectptr.p->tcConnectstate == OS_PREPARED)))
      {
        ndbout_c("op: 0x%x trans [ 0x%.8x 0x%.8x ] state: %u (TopPtrI: %x)",
                 localTcConnectptr.i,
                 regApiPtr.p->transid[0],
                 regApiPtr.p->transid[1],
                 localTcConnectptr.p->tcConnectstate,
                 TopPtrI);
        dump_trans(regApiPtr);
      }

      ndbrequire(localTcConnectptr.p->tcConnectstate == OS_PREPARED);
      localTcConnectptr.p->tcConnectstate = OS_FIRE_TRIG_REQ;
      localTcConnectptr.p->m_special_op_flags = flags;
      i += sendFireTrigReqLqh(signal, localTcConnectptr, pass);
      Tlqhkeyreqrec++;
    }

    prevOpPtrI = localTcConnectptr.i;
    localTcConnectptr.i = nextTcConnect;
  }

  regApiPtr.p->lqhkeyreqrec = Tlqhkeyreqrec;
  if (prevOpPtrI == TlastOpPtrI)
  {
    /**
     * Now wait for FIRE_TRIG_CONF
     */
    jam();
    regApiPtr.p->apiConnectstate = CS_WAIT_FIRE_TRIG_REQ;
    ndbrequire(pass < 255);
    regApiPtr.p->m_pre_commit_pass = getNextDeferredPass(pass);

    /**
     * Check if we are already finished...
     */
    if (regApiPtr.p->lqhkeyreqrec == regApiPtr.p->lqhkeyconfrec &&
        regApiPtr.p->pendingTriggers == 0)
    {
      jam();
      lqhKeyConf_checkTransactionState(signal, regApiPtr);
    }
    return;
  }
  else
  {
    jam();
    regApiPtr.p->lqhkeyreqrec++; // prevent early completion
    signal->theData[0] = TcContinueB::ZSEND_FIRE_TRIG_REQ;
    signal->theData[1] = regApiPtr.i;
    signal->theData[2] = regApiPtr.p->transid[0];
    signal->theData[3] = regApiPtr.p->transid[1];
    signal->theData[4] = localTcConnectptr.i;
    signal->theData[5] = TlastOpPtrI;
    if (ERROR_INSERTED_CLEAR(8090))
    {
      sendSignalWithDelay(cownref, GSN_CONTINUEB, signal, 5000, 6);
    }
    else
    {
      sendSignal(cownref, GSN_CONTINUEB, signal, 6, JBB);
    }
  }
}

Uint32
Dbtc::sendFireTrigReqLqh(Signal* signal,
                         Ptr<TcConnectRecord> regTcPtr,
                         Uint32 pass)
{
  HostRecordPtr Thostptr;
  UintR ThostFilesize = chostFilesize;
  Uint32 instanceKey = regTcPtr.p->lqhInstanceKey;
  ApiConnectRecord * const regApiPtr = apiConnectptr.p;
  Thostptr.i = regTcPtr.p->tcNodedata[0];
  ptrCheckGuard(Thostptr, ThostFilesize, hostRecord);

  Uint32 Tnode = Thostptr.i;
  Uint32 self = getOwnNodeId();
  Uint32 ret = (Tnode == self) ? 4 : 1;

  Uint32 Tdata[FireTrigReq::SignalLength];
  FireTrigReq * req = CAST_PTR(FireTrigReq, Tdata);
  req->tcOpRec = regTcPtr.i;
  req->transId[0] = regApiPtr->transid[0];
  req->transId[1] = regApiPtr->transid[1];
  req->pass = pass;
  Uint32 len = FireTrigReq::SignalLength;

  if (instanceKey > MAX_NDBMT_LQH_THREADS) {
    memcpy(signal->theData, Tdata, len << 2);
    BlockReference lqhRef = numberToRef(DBLQH, instanceKey, Tnode);
    sendSignal(lqhRef, GSN_FIRE_TRIG_REQ, signal, len, JBB);
    return ret;
  }

  struct PackedWordsContainer * container = &Thostptr.p->lqh_pack[instanceKey];

  if (container->noOfPackedWords > 25 - len) {
    jam();
    sendPackedSignal(signal, container);
  } else {
    jam();
    ret = 1;
    updatePackedList(signal, Thostptr.p, Thostptr.i);
  }

  Tdata[0] |= (ZFIRE_TRIG_REQ << 28);
  UintR Tindex = container->noOfPackedWords;
  container->noOfPackedWords = Tindex + len;
  UintR* TDataPtr = &container->packedWords[Tindex];
  memcpy(TDataPtr, Tdata, len << 2);
  return ret;
}

void
Dbtc::execFIRE_TRIG_CONF(Signal* signal)
{
  TcConnectRecordPtr localTcConnectptr;
  ApiConnectRecordPtr regApiPtr;

  UintR TtcConnectFilesize = ctcConnectFilesize;
  UintR TapiConnectFilesize = capiConnectFilesize;
  TcConnectRecord *localTcConnectRecord = tcConnectRecord;
  ApiConnectRecord *localApiConnectRecord = apiConnectRecord;

  const FireTrigConf * conf = CAST_CONSTPTR(FireTrigConf, signal->theData);
  localTcConnectptr.i = conf->tcOpRec;
  jamEntry();
  ptrCheckGuard(localTcConnectptr, TtcConnectFilesize, localTcConnectRecord);
  regApiPtr.i = localTcConnectptr.p->apiConnect;
  if (localTcConnectptr.p->tcConnectstate != OS_FIRE_TRIG_REQ)
  {
    warningReport(signal, 28);
    return;
  }//if
  ptrCheckGuard(regApiPtr, TapiConnectFilesize,
                localApiConnectRecord);

  Uint32 Tlqhkeyreqrec = regApiPtr.p->lqhkeyreqrec;
  Uint32 TapiConnectstate = regApiPtr.p->apiConnectstate;
  UintR Tdata1 = regApiPtr.p->transid[0] - conf->transId[0];
  UintR Tdata2 = regApiPtr.p->transid[1] - conf->transId[1];
  Uint32 TcheckCondition =
    (TapiConnectstate != CS_SEND_FIRE_TRIG_REQ) &&
    (TapiConnectstate != CS_WAIT_FIRE_TRIG_REQ);

  Tdata1 = Tdata1 | Tdata2 | TcheckCondition;

  if (Tdata1 != 0) {
    warningReport(signal, 28);
    return;
  }//if

  if (ERROR_INSERTED_CLEAR(8091))
  {
    jam();
    return;
  }

  CRASH_INSERTION(8092);

  setApiConTimer(regApiPtr.i, ctcTimer, __LINE__);
  ndbassert(Tlqhkeyreqrec > 0);
  regApiPtr.p->lqhkeyreqrec = Tlqhkeyreqrec - 1;
  localTcConnectptr.p->tcConnectstate = OS_PREPARED;

  Uint32 numFired  = FireTrigConf::getFiredCount(conf->numFiredTriggers);
  Uint32 deferreduk = FireTrigConf::getDeferredUKBit(conf->numFiredTriggers);
  Uint32 deferredfk = FireTrigConf::getDeferredFKBit(conf->numFiredTriggers);

  regApiPtr.p->pendingTriggers += numFired;
  regApiPtr.p->m_flags |=
    ((deferreduk) ? ApiConnectRecord::TF_DEFERRED_UK_TRIGGERS : 0) |
    ((deferredfk) ? ApiConnectRecord::TF_DEFERRED_FK_TRIGGERS : 0);
  localTcConnectptr.p->m_special_op_flags |=
    ((deferreduk) ? TcConnectRecord::SOF_DEFERRED_UK_TRIGGER : 0) |
    ((deferredfk) ? TcConnectRecord::SOF_DEFERRED_FK_TRIGGER : 0);

  if (regApiPtr.p->pendingTriggers == 0)
  {
    jam();
    lqhKeyConf_checkTransactionState(signal, regApiPtr);
  }
}

void
Dbtc::execFIRE_TRIG_REF(Signal* signal)
{
  TcConnectRecordPtr localTcConnectptr;
  ApiConnectRecordPtr regApiPtr;

  UintR TtcConnectFilesize = ctcConnectFilesize;
  UintR TapiConnectFilesize = capiConnectFilesize;
  TcConnectRecord *localTcConnectRecord = tcConnectRecord;
  ApiConnectRecord *localApiConnectRecord = apiConnectRecord;

  const FireTrigRef * ref = CAST_CONSTPTR(FireTrigRef, signal->theData);
  localTcConnectptr.i = ref->tcOpRec;
  jamEntry();
  ptrCheckGuard(localTcConnectptr, TtcConnectFilesize, localTcConnectRecord);
  regApiPtr.i = localTcConnectptr.p->apiConnect;
  if (localTcConnectptr.p->tcConnectstate != OS_FIRE_TRIG_REQ)
  {
    warningReport(signal, 28);
    return;
  }//if
  ptrCheckGuard(regApiPtr, TapiConnectFilesize,
                localApiConnectRecord);

  apiConnectptr = regApiPtr;

  UintR Tdata1 = regApiPtr.p->transid[0] - ref->transId[0];
  UintR Tdata2 = regApiPtr.p->transid[1] - ref->transId[1];
  Tdata1 = Tdata1 | Tdata2;
  if (Tdata1 != 0) {
    warningReport(signal, 28);
    return;
  }//if

  if (regApiPtr.p->apiConnectstate != CS_SEND_FIRE_TRIG_REQ &&
      regApiPtr.p->apiConnectstate != CS_WAIT_FIRE_TRIG_REQ)
  {
    jam();
    warningReport(signal, 28);
    return;
  }

  terrorCode = ref->errCode;
  abortErrorLab(signal);
}

/**
 * The NDB API has now reported that it has heard about the commit of
 * transaction, this means that we're ready to remove the commit ack
 * markers, both here in DBTC and in all the participating DBLQH's.
 */
void
Dbtc::execTC_COMMIT_ACK(Signal* signal){
  jamEntry();

  CommitAckMarker key;
  key.transid1 = signal->theData[0];
  key.transid2 = signal->theData[1];

  CommitAckMarkerPtr removedMarker;
  m_commitAckMarkerHash.remove(removedMarker, key);
  if (removedMarker.i == RNIL) {
    jam();
    warningHandlerLab(signal, __LINE__);
    return;
  }//if
  sendRemoveMarkers(signal, removedMarker.p, 0);
  m_commitAckMarkerPool.release(removedMarker);
}

void
Dbtc::sendRemoveMarkers(Signal* signal,
                        CommitAckMarker * marker,
                        Uint32 removed_by_fail_api)
{
  jam();
  const Uint32 transId1 = marker->transid1;
  const Uint32 transId2 = marker->transid2;

  CommitAckMarkerBuffer::DataBufferPool & pool =
    c_theCommitAckMarkerBufferPool;
  LocalCommitAckMarkerBuffer commitAckMarkers(pool, marker->theDataBuffer);
  CommitAckMarkerBuffer::DataBufferIterator iter;
  bool next_flag = commitAckMarkers.first(iter);
  while (next_flag)
  {
    jam();
    Uint32 dataWord = *iter.data;
    NodeId nodeId = dataWord >> 16;
    Uint32 instanceKey = dataWord & 0xFFFF;
    sendRemoveMarker(signal,
                     nodeId,
                     instanceKey,
                     transId1,
                     transId2,
                     removed_by_fail_api);
    next_flag = commitAckMarkers.next(iter, 1);
  }
  commitAckMarkers.release();
}

void
Dbtc::sendRemoveMarker(Signal* signal, 
                       NodeId nodeId,
                       Uint32 instanceKey,
                       Uint32 transid1, 
                       Uint32 transid2,
                       Uint32 removed_by_fail_api){
  /**
   * Seize host ptr
   */
  HostRecordPtr hostPtr;
  const UintR ThostFilesize = chostFilesize;
  hostPtr.i = nodeId;
  ptrCheckGuard(hostPtr, ThostFilesize, hostRecord);

  Uint32 Tdata[3];
  Tdata[0] = removed_by_fail_api;
  Tdata[1] = transid1;
  Tdata[2] = transid2;
  Uint32 len = 3;

  if (instanceKey > MAX_NDBMT_LQH_THREADS) {
    jam();
    // first word omitted
    memcpy(&signal->theData[0], &Tdata[1], (len - 1) << 2);
    Uint32 Tnode = hostPtr.i;
    BlockReference ref = numberToRef(DBLQH, instanceKey, Tnode);
    sendSignal(ref, GSN_REMOVE_MARKER_ORD, signal, len - 1, JBB);
    return;
  }

  struct PackedWordsContainer * container = &hostPtr.p->lqh_pack[instanceKey];

  if (container->noOfPackedWords > (25 - 3)){
    jam();
    sendPackedSignal(signal, container);
  } else {
    jam();
    updatePackedList(signal, hostPtr.p, hostPtr.i);
  }
  
  UintR  numWord = container->noOfPackedWords;
  UintR* dataPtr = &container->packedWords[numWord];

  container->noOfPackedWords = numWord + len;
  Tdata[0] |= (ZREMOVE_MARKER << 28);
  memcpy(dataPtr, &Tdata[0], len << 2);
}

/**
 * sendApiLateCommitSignal
 *
 * This is called at the end of a transaction's 
 * COMPLETE phase when it has the TF_LATE_COMMIT flag set.
 * The Commit notification is sent to the API, and
 * the user ApiConnectRecord is returned to a ready state
 * for further API interaction.
 */
void 
Dbtc::sendApiLateCommitSignal(Signal* signal,
                              Ptr<ApiConnectRecord> apiCopy)
{
  jam();
  
  apiConnectptr.i = apiCopy.p->apiCopyRecord;
  ptrCheckGuard(apiConnectptr, capiConnectFilesize,
                apiConnectRecord);
  /**
   * CS_COMMITTING on user ApiCon is inferred by 
   * TF_LATE_COMMIT on user ApiCon
   */
  ndbrequire(apiConnectptr.p->apiConnectstate == CS_COMMITTING);
  ndbrequire(tc_testbit(apiConnectptr.p->m_flags,
                        ApiConnectRecord::TF_LATE_COMMIT));

  apiCopy.p->apiCopyRecord = RNIL;
  tc_clearbit(apiCopy.p->m_flags,
              ApiConnectRecord::TF_LATE_COMMIT);
  apiConnectptr.p->apiConnectstate = CS_CONNECTED;
  tc_clearbit(apiConnectptr.p->m_flags,
              ApiConnectRecord::TF_LATE_COMMIT);

  /* Transiently re-set the commitAckMarker on the user's
   * connectRecord so that we inform API of the need to 
   * send a COMMIT_ACK.
   * 
   * TODO : Don't actually need the full CommitAckMarker, just a bit
   */
  ndbassert(apiConnectptr.p->commitAckMarker == RNIL);
  apiConnectptr.p->commitAckMarker = apiCopy.p->commitAckMarker;
  
  sendApiCommitSignal(signal, apiConnectptr);
  
  apiConnectptr.p->commitAckMarker = RNIL;
  apiCopy.p->commitAckMarker = RNIL;
  
  if (apiConnectptr.p->apiFailState != ApiConnectRecord::AFS_API_OK)
  {
    jam();
    /**
     * handle if API failed while we were running complete phase
     */
    handleApiFailState(signal, apiConnectptr.i);
  }
}

void Dbtc::execCOMPLETED(Signal* signal) 
{
  TcConnectRecordPtr localTcConnectptr;
  ApiConnectRecordPtr localApiConnectptr;

  UintR TtcConnectFilesize = ctcConnectFilesize;
  UintR TapiConnectFilesize = capiConnectFilesize;
  TcConnectRecord *localTcConnectRecord = tcConnectRecord;
  ApiConnectRecord *localApiConnectRecord = apiConnectRecord;

#ifdef ERROR_INSERT
  if (ERROR_INSERTED(8031)) {
    systemErrorLab(signal, __LINE__);
  }//if
  if (ERROR_INSERTED(8019)) {
    CLEAR_ERROR_INSERT_VALUE;
    return;
  }//if
  if (ERROR_INSERTED(8027)) {
    SET_ERROR_INSERT_VALUE(8028);
    return;
  }//if
  if (ERROR_INSERTED(8043)) {
    CLEAR_ERROR_INSERT_VALUE;
    sendSignalWithDelay(cownref, GSN_COMPLETED, signal, 2000, 3);
    return;
  }//if
  if (ERROR_INSERTED(8044)) {
    SET_ERROR_INSERT_VALUE(8047);
    sendSignalWithDelay(cownref, GSN_COMPLETED, signal, 2000, 3);
    return;
  }//if
#endif
  localTcConnectptr.i = signal->theData[0];
  jamEntry();
  ptrCheckGuard(localTcConnectptr, TtcConnectFilesize, localTcConnectRecord);
  bool Tcond1 = (localTcConnectptr.p->tcConnectstate != OS_COMPLETING);
  localApiConnectptr.i = localTcConnectptr.p->apiConnect;
  if (Tcond1) {
    warningReport(signal, 6);
    return;
  }//if
  ptrCheckGuard(localApiConnectptr, TapiConnectFilesize, 
		localApiConnectRecord);
  UintR Tdata1 = localApiConnectptr.p->transid[0] - signal->theData[1];
  UintR Tdata2 = localApiConnectptr.p->transid[1] - signal->theData[2];
  UintR Tcounter = localApiConnectptr.p->counter - 1;
  ConnectionState TapiConnectstate = localApiConnectptr.p->apiConnectstate;
  Tdata1 = Tdata1 | Tdata2;
  bool TcheckCondition = 
    (TapiConnectstate != CS_COMPLETE_SENT) || (Tcounter != 0);
  if (Tdata1 != 0) {
    warningReport(signal, 7);
    return;
  }//if
  setApiConTimer(localApiConnectptr.i, ctcTimer, __LINE__);
  localApiConnectptr.p->counter = Tcounter;
  localTcConnectptr.p->tcConnectstate = OS_COMPLETED;
  localTcConnectptr.p->noOfNodes = 0; // == releaseNodes(signal)
  if (TcheckCondition) {
    jam();
    /*-------------------------------------------------------*/
    // We have not sent all COMPLETE requests yet. We could be
    // in the state that all sent are COMPLETED but we are
    // still waiting for a CONTINUEB to send the rest of the
    // COMPLETE requests.
    /*-------------------------------------------------------*/
    return;
  }//if
  if (ERROR_INSERTED(8021)) {
    jam();
    systemErrorLab(signal, __LINE__);
  }//if

  if (tc_testbit(localApiConnectptr.p->m_flags,
                 ApiConnectRecord::TF_LATE_COMMIT))
  {
    jam();
    
    sendApiLateCommitSignal(signal, localApiConnectptr);
  }
  apiConnectptr = localApiConnectptr;
  releaseTransResources(signal);
  CRASH_INSERTION(8054);
}//Dbtc::execCOMPLETED()

/*---------------------------------------------------------------------------*/
/*                               RELEASE_TRANS_RESOURCES                     */
/*       RELEASE ALL RESOURCES THAT ARE CONNECTED TO THIS TRANSACTION.       */
/*---------------------------------------------------------------------------*/
void Dbtc::releaseTransResources(Signal* signal) 
{
  TcConnectRecordPtr localTcConnectptr;
  UintR TtcConnectFilesize = ctcConnectFilesize;
  TcConnectRecord *localTcConnectRecord = tcConnectRecord;
  apiConnectptr.p->m_transaction_nodes.clear();
  localTcConnectptr.i = apiConnectptr.p->firstTcConnect;
  do {
    jam();
    ptrCheckGuard(localTcConnectptr, TtcConnectFilesize, localTcConnectRecord);
    UintR rtrTcConnectptrIndex = localTcConnectptr.p->nextTcConnect;
    tcConnectptr.i = localTcConnectptr.i;
    tcConnectptr.p = localTcConnectptr.p;
    localTcConnectptr.i = rtrTcConnectptrIndex;
    releaseTcCon();
  } while (localTcConnectptr.i != RNIL);
  handleGcp(signal, apiConnectptr);
  releaseFiredTriggerData(&apiConnectptr.p->theFiredTriggers);
  releaseAllSeizedIndexOperations(apiConnectptr.p);
  releaseApiConCopy(signal);
}//Dbtc::releaseTransResources()

/* *********************************************************************>> */
/*       MODULE: HANDLE_GCP                                                */
/*       DESCRIPTION: HANDLES GLOBAL CHECKPOINT HANDLING AT THE COMPLETION */
/*       OF THE COMMIT PHASE AND THE ABORT PHASE. WE MUST ENSURE THAT TC   */
/*       SENDS GCP_TCFINISHED WHEN ALL TRANSACTIONS BELONGING TO A CERTAIN */
/*       GLOBAL CHECKPOINT HAVE COMPLETED.                                 */
/* *********************************************************************>> */
void Dbtc::handleGcp(Signal* signal, Ptr<ApiConnectRecord> regApiPtr)
{
  GcpRecord *localGcpRecord = gcpRecord;
  GcpRecordPtr localGcpPtr;
  UintR TgcpFilesize = cgcpFilesize;
  localGcpPtr.i = apiConnectptr.p->gcpPointer;
  ptrCheckGuard(localGcpPtr, TgcpFilesize, localGcpRecord);
  unlinkApiConnect(localGcpPtr, regApiPtr);
  if (localGcpPtr.p->firstApiConnect == RNIL) {
    if (localGcpPtr.p->gcpNomoretransRec == ZTRUE) {
      if (c_ongoing_take_over_cnt == 0)
      {
        jam();
        gcpTcfinished(signal, localGcpPtr.p->gcpId);
        unlinkGcp(localGcpPtr);
      }
    }//if
  }
}//Dbtc::handleGcp()

void Dbtc::releaseApiConCopy(Signal* signal) 
{
  ApiConnectRecord * const regApiPtr = apiConnectptr.p;
  ndbassert(regApiPtr->nextApiConnect == RNIL);
  UintR TfirstfreeApiConnectCopyOld = cfirstfreeApiConnectCopy;
  cfirstfreeApiConnectCopy = apiConnectptr.i;
  regApiPtr->nextApiConnect = TfirstfreeApiConnectCopyOld;
  setApiConTimer(apiConnectptr.i, 0, __LINE__);
  regApiPtr->apiConnectstate = CS_RESTART;
  ndbrequire(regApiPtr->commitAckMarker == RNIL);
}//Dbtc::releaseApiConCopy()

/* ========================================================================= */
/* -------  RELEASE ALL RECORDS CONNECTED TO A DIRTY WRITE OPERATION ------- */
/* ========================================================================= */
void Dbtc::releaseDirtyWrite(Signal* signal) 
{
  clearCommitAckMarker(apiConnectptr.p, tcConnectptr.p);
  unlinkReadyTcCon(signal);
  releaseTcCon();
  ApiConnectRecord * const regApiPtr = apiConnectptr.p;
  if (regApiPtr->apiConnectstate == CS_START_COMMITTING) {
    if (regApiPtr->firstTcConnect == RNIL) {
      jam();
      regApiPtr->apiConnectstate = CS_CONNECTED;
      setApiConTimer(apiConnectptr.i, 0, __LINE__);
      sendtckeyconf(signal, 1);
    }//if
  }//if
}//Dbtc::releaseDirtyWrite()

/*****************************************************************************
 *                               L Q H K E Y R E F                          
 * WHEN LQHKEYREF IS RECEIVED DBTC WILL CHECK IF COMMIT FLAG WAS SENT FROM THE
 * APPLICATION. IF SO, THE WHOLE TRANSACTION WILL BE ROLLED BACK AND SIGNAL  
 * TCROLLBACKREP WILL BE SENT TO THE API.                                    
 *                                                                          
 * OTHERWISE TC WILL CHECK THE ERRORCODE. IF THE ERRORCODE IS INDICATING THAT
 * THE "ROW IS NOT FOUND" FOR UPDATE/READ/DELETE OPERATIONS AND "ROW ALREADY 
 * EXISTS" FOR INSERT OPERATIONS, DBTC WILL RELEASE THE OPERATION AND THEN   
 * SEND RETURN SIGNAL TCKEYREF TO THE USER. THE USER THEN HAVE TO SEND     
 * SIGNAL TC_COMMITREQ OR TC_ROLLBACKREQ TO CONCLUDE THE TRANSACTION. 
 * IF ANY TCKEYREQ WITH COMMIT IS RECEIVED AND API_CONNECTSTATE EQUALS 
 * "REC_LQHREFUSE",
 * THE OPERATION WILL BE TREATED AS AN OPERATION WITHOUT COMMIT. WHEN ANY    
 * OTHER FAULTCODE IS RECEIVED THE WHOLE TRANSACTION MUST BE ROLLED BACK     
 *****************************************************************************/
void Dbtc::execLQHKEYREF(Signal* signal) 
{
  const LqhKeyRef * const lqhKeyRef = (LqhKeyRef *)signal->getDataPtr();
  Uint32 indexId = 0;
  jamEntry();
  
  UintR compare_transid1, compare_transid2;
  UintR TtcConnectFilesize = ctcConnectFilesize;
  /*-------------------------------------------------------------------------
   *                                                                         
   * RELEASE NODE BUFFER(S) TO INDICATE THAT THIS OPERATION HAVE NO 
   * TRANSACTION PARTS ACTIVE ANYMORE. 
   * LQHKEYREF HAVE CLEARED ALL PARTS ON ITS PATH BACK TO TC.
   *-------------------------------------------------------------------------*/
  if (lqhKeyRef->connectPtr < TtcConnectFilesize) {
    /*-----------------------------------------------------------------------
     * WE HAVE TO CHECK THAT THE TRANSACTION IS STILL VALID. FIRST WE CHECK 
     * THAT THE LQH IS STILL CONNECTED TO A TC, IF THIS HOLDS TRUE THEN THE 
     * TC MUST BE CONNECTED TO AN API CONNECT RECORD. 
     * WE MUST ENSURE THAT THE TRANSACTION ID OF THIS API CONNECT 
     * RECORD IS STILL THE SAME AS THE ONE LQHKEYREF REFERS TO. 
     * IF NOT SIMPLY EXIT AND FORGET THE SIGNAL SINCE THE TRANSACTION IS    
     * ALREADY COMPLETED (ABORTED).
     *-----------------------------------------------------------------------*/
    tcConnectptr.i = lqhKeyRef->connectPtr;
    Uint32 errCode = terrorCode = lqhKeyRef->errorCode;
    ptrAss(tcConnectptr, tcConnectRecord);
    TcConnectRecord * const regTcPtr = tcConnectptr.p;
    if (regTcPtr->tcConnectstate == OS_OPERATING) {
      Uint32 save = apiConnectptr.i = regTcPtr->apiConnect;
      ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
      ApiConnectRecord * const regApiPtr = apiConnectptr.p;
      compare_transid1 = regApiPtr->transid[0] ^ lqhKeyRef->transId1;
      compare_transid2 = regApiPtr->transid[1] ^ lqhKeyRef->transId2;
      compare_transid1 = compare_transid1 | compare_transid2;
      if (compare_transid1 != 0) {
	warningReport(signal, 25);
	return;
      }//if

      const Uint32 triggeringOp = regTcPtr->triggeringOperation;
      ConnectionState TapiConnectstate = regApiPtr->apiConnectstate;

      time_track_complete_key_operation_error(regTcPtr,
                                    refToNode(regApiPtr->ndbapiBlockref),
                                    regTcPtr->tcNodedata[0]);

      if (unlikely(TapiConnectstate == CS_ABORTING))
      {
        jam();
        goto do_abort;
      }

      if (triggeringOp != RNIL) {
        jam();
	// This operation was created by a trigger execting operation
	TcConnectRecordPtr opPtr;
	TcConnectRecord *localTcConnectRecord = tcConnectRecord;

        opPtr.i = triggeringOp;
        ptrCheckGuard(opPtr, ctcConnectFilesize, localTcConnectRecord);

        const Uint32 opType = regTcPtr->operation;
        Ptr<TcDefinedTriggerData> trigPtr;
        c_theDefinedTriggers.getPtr(trigPtr, regTcPtr->currentTriggerId);
        switch(trigPtr.p->triggerType){
        case TriggerType::SECONDARY_INDEX:{
          jam();
	
          // The operation executed an index trigger
          TcIndexData* indexData = c_theIndexes.getPtr(trigPtr.p->indexId);
          indexId = indexData->indexId;
          regApiPtr->errorData = indexId;
          if (errCode == ZALREADYEXIST)
          {
            jam();
            errCode = terrorCode = ZNOTUNIQUE;
            goto do_abort;
          }
          else if (!(opType == ZDELETE && errCode == ZNOT_FOUND)) {
            jam();
            /**
             * "Normal path"
             */
            goto do_abort;
          }
          else
          {
            jam();
            /** ZDELETE && NOT_FOUND */
            if (indexData->indexState != IS_BUILDING)
            {
              jam();
              goto do_abort;
            }
          }
          goto do_ignore;
        }
        case TriggerType::FULLY_REPLICATED_TRIGGER:
        {
          jam();
          /**
           * We have successfully inserted/updated/deleted in main
           * fragment, but failed in a copy fragment. In this case
           * we need to abort transaction. There are no error codes
           * that makes this behaviour ok.
           */
          goto do_abort;
        }
        case TriggerType::REORG_TRIGGER:
          jam();
          if (opType == ZINSERT && errCode == ZALREADYEXIST)
          {
            jam();
            ndbout_c("reorg, ignore ZALREADYEXIST");
            goto do_ignore;
          }
          else if (errCode == ZNOT_FOUND)
          {
            jam();
            ndbout_c("reorg, ignore ZNOT_FOUND");
            goto do_ignore;
          }
          else if (errCode == 839)
          {
            jam();
            ndbout_c("reorg, ignore 839");
            goto do_ignore;
          }
          else
          {
            ndbout_c("reorg: opType: %u errCode: %u", opType, errCode);
          }
          // fall-through
          goto do_abort;
        case TriggerType::FK_CHILD:
          jam();
          if (errCode == ZNOT_FOUND)
          {
            errCode = terrorCode = ZFK_NO_PARENT_ROW_EXISTS;
            regApiPtr->errorData = trigPtr.p->fkId;
          }
          goto do_abort;
        case TriggerType::FK_PARENT:
          jam();
          /**
           * on delete/update of parent...
           *   we found no child row...
           *   that means FK not violated...
           */
          if (errCode == ZNOT_FOUND)
          {
            jam();
            if (regTcPtr->isIndexOp(regTcPtr->m_special_op_flags))
            {
              jam();
              /**
               * execTCINDXREQ adds an extra regApiPtr->lqhkeyreqrec++
               *   undo this...
               */
              ndbassert(regApiPtr->lqhkeyreqrec > 0);
              regApiPtr->lqhkeyreqrec--;
            }
            goto do_ignore;
          }
          else
          {
            goto do_abort;
          }
        default:
          jam();
          ndbassert(false);
          goto do_abort;
        }

    do_ignore:
        jam();
        /**
         * Ignore error
         */
        regApiPtr->lqhkeyreqrec--;

        /**
         * An failing op in LQH, never leaves the commit ack marker around
         * TODO: This can be bug in ordinary code too!!!
         */
        clearCommitAckMarker(regApiPtr, regTcPtr);

        unlinkReadyTcCon(signal);
        releaseTcCon();

        trigger_op_finished(signal, apiConnectptr, RNIL, opPtr.p, 0);

        if (ERROR_INSERTED(8105))
        {
          abortTransFromTrigger(signal, apiConnectptr, ZGET_DATAREC_ERROR);
        }
        return;
      }
      
  do_abort:
      markOperationAborted(regApiPtr, regTcPtr);
      
      if(regApiPtr->apiConnectstate == CS_ABORTING){
	/**
	 * We're already aborting' so don't send an "extra" TCKEYREF
	 */
	jam();
	return;
      }
      
      const Uint32 abort = regTcPtr->m_execAbortOption;
      if (abort == TcKeyReq::AbortOnError || triggeringOp != RNIL) {
	/**
	 * No error is allowed on this operation
	 */
	TCKEY_abort(signal, 49);
	return;
      }//if

      /* *************** */
      /*    TCKEYREF   < */
      /* *************** */
      TcKeyRef * const tcKeyRef = (TcKeyRef *) signal->getDataPtrSend();
      tcKeyRef->transId[0] = regApiPtr->transid[0];
      tcKeyRef->transId[1] = regApiPtr->transid[1];
      tcKeyRef->errorCode = terrorCode;
      bool isIndexOp = regTcPtr->isIndexOp(regTcPtr->m_special_op_flags);
      Uint32 indexOp = tcConnectptr.p->indexOp;
      Uint32 clientData = regTcPtr->clientData;
      unlinkReadyTcCon(signal);   /* LINK TC CONNECT RECORD OUT OF  */
      releaseTcCon();       /* RELEASE THE TC CONNECT RECORD  */
      setApiConTimer(apiConnectptr.i, ctcTimer, __LINE__);
      if (isIndexOp) {
        jam();
	regApiPtr->lqhkeyreqrec--; // Compensate for extra during read
	tcKeyRef->connectPtr = indexOp;
        tcKeyRef->errorData = indexId;
	EXECUTE_DIRECT(DBTC, GSN_TCKEYREF, signal, TcKeyRef::SignalLength);
	apiConnectptr.i = save;
	apiConnectptr.p = regApiPtr;
      } else {
        jam();
	tcKeyRef->connectPtr = clientData;
        tcKeyRef->errorData = indexId;
	sendSignal(regApiPtr->ndbapiBlockref, 
		   GSN_TCKEYREF, signal, TcKeyRef::SignalLength, JBB);
      }//if
      
      /*---------------------------------------------------------------------
       * SINCE WE ARE NOT ABORTING WE NEED TO UPDATE THE COUNT OF HOW MANY 
       * LQHKEYREQ THAT HAVE RETURNED. 
       * IF NO MORE OUTSTANDING LQHKEYREQ'S THEN WE NEED TO 
       * TCKEYCONF (IF THERE IS ANYTHING TO SEND).          
       *---------------------------------------------------------------------*/
      regApiPtr->lqhkeyreqrec--;
      if (regApiPtr->lqhkeyconfrec == regApiPtr->lqhkeyreqrec) {
	if (regApiPtr->apiConnectstate == CS_START_COMMITTING) {
          jam();
          diverify010Lab(signal);
	  return;
	}
        else if (regApiPtr->tckeyrec > 0 ||
                 tc_testbit(regApiPtr->m_flags, ApiConnectRecord::TF_EXEC_FLAG))
        {
	  jam();
	  sendtckeyconf(signal, 2);
	  return;
	}
      }//if
      return;
      
    } else {
      warningReport(signal, 26);
    }//if
  } else {
    errorReport(signal, 6);
  }//if
  return;
}//Dbtc::execLQHKEYREF()

void Dbtc::clearCommitAckMarker(ApiConnectRecord * const regApiPtr,
				TcConnectRecord * const regTcPtr)
{
  jam();
  const Uint32 commitAckMarker = regTcPtr->commitAckMarker;
  if (regApiPtr->commitAckMarker == RNIL)
  {
    jam();
    ndbassert(commitAckMarker == RNIL);
  }

  if(commitAckMarker != RNIL)
  {
    jam();
    ndbrequire(regApiPtr->commitAckMarker == commitAckMarker);
    ndbrequire(regApiPtr->num_commit_ack_markers > 0);
    regApiPtr->num_commit_ack_markers--;
    regTcPtr->commitAckMarker = RNIL;
    if (regApiPtr->num_commit_ack_markers == 0)
    {
      jam();
      tc_clearbit(regApiPtr->m_flags,
                  ApiConnectRecord::TF_COMMIT_ACK_MARKER_RECEIVED);
      releaseMarker(regApiPtr);
    }
  }
}

void Dbtc::markOperationAborted(ApiConnectRecord * const regApiPtr,
				TcConnectRecord * const regTcPtr)
{
  /*------------------------------------------------------------------------
   * RELEASE NODES TO INDICATE THAT THE OPERATION IS ALREADY ABORTED IN THE 
   * LQH'S ALSO SET STATE TO ABORTING TO INDICATE THE ABORT IS 
   * ALREADY COMPLETED.  
   *------------------------------------------------------------------------*/
  regTcPtr->noOfNodes = 0; // == releaseNodes(signal)
  regTcPtr->tcConnectstate = OS_ABORTING;
  clearCommitAckMarker(regApiPtr, regTcPtr);
}

/*--------------------------------------*/
/* EXIT AND WAIT FOR SIGNAL TCOMMITREQ  */
/* OR TCROLLBACKREQ FROM THE USER TO    */
/* CONTINUE THE TRANSACTION             */
/*--------------------------------------*/
void Dbtc::execTC_COMMITREQ(Signal* signal) 
{
  UintR compare_transid1, compare_transid2;

  jamEntry();
  apiConnectptr.i = signal->theData[0];
  if (apiConnectptr.i < capiConnectFilesize) {
    ptrAss(apiConnectptr, apiConnectRecord);
    compare_transid1 = apiConnectptr.p->transid[0] ^ signal->theData[1];
    compare_transid2 = apiConnectptr.p->transid[1] ^ signal->theData[2];
    compare_transid1 = compare_transid1 | compare_transid2;
    if (compare_transid1 != 0) {
      jam();
      return;
    }//if
    
    ApiConnectRecord * const regApiPtr = apiConnectptr.p;

    const Uint32 apiConnectPtr = regApiPtr->ndbapiConnect;
    const Uint32 apiBlockRef   = regApiPtr->ndbapiBlockref;
    const Uint32 transId1      = regApiPtr->transid[0];
    const Uint32 transId2      = regApiPtr->transid[1];
    Uint32 errorCode           = 0;

    regApiPtr->m_flags |= ApiConnectRecord::TF_EXEC_FLAG;
    switch (regApiPtr->apiConnectstate) {
    case CS_STARTED:
      tcConnectptr.i = regApiPtr->firstTcConnect;
      if (tcConnectptr.i != RNIL) {
        ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
        if (regApiPtr->lqhkeyconfrec == regApiPtr->lqhkeyreqrec) {
          jam();
          /*******************************************************************/
          // The proper case where the application is waiting for commit or 
          // abort order.
          // Start the commit order.
          /*******************************************************************/
          regApiPtr->returnsignal = RS_TC_COMMITCONF;
          setApiConTimer(apiConnectptr.i, ctcTimer, __LINE__);
          diverify010Lab(signal);
          return;
        } else {
          jam();
          /*******************************************************************/
          // The transaction is started but not all operations are completed. 
          // It is not possible to commit the transaction in this state. 
          // We will abort it instead.
          /*******************************************************************/
          regApiPtr->returnsignal = RS_NO_RETURN;
          errorCode = ZTRANS_STATUS_ERROR;
          abort010Lab(signal);
        }//if
      } else {
        jam();
        /**
         * No operations, accept commit
         */
        TcCommitConf * const commitConf = (TcCommitConf *)&signal->theData[0];
        commitConf->apiConnectPtr = apiConnectPtr;
        commitConf->transId1 = transId1;
        commitConf->transId2 = transId2;
        commitConf->gci_hi = 0;
        commitConf->gci_lo = 0;
        sendSignal(apiBlockRef, GSN_TC_COMMITCONF, signal, 
		   TcCommitConf::SignalLength, JBB);
        
        regApiPtr->returnsignal = RS_NO_RETURN;
        releaseAbortResources(signal);
        return;
      }//if
      break;
    case CS_RECEIVING:
      jam();
      /***********************************************************************/
      // A transaction is still receiving data. We cannot commit an unfinished 
      // transaction. We will abort it instead.
      /***********************************************************************/
      regApiPtr->returnsignal = RS_NO_RETURN;
      errorCode = ZPREPAREINPROGRESS;
      abort010Lab(signal);
      break;
      
    case CS_START_COMMITTING:
    case CS_COMMITTING:
    case CS_COMMIT_SENT:
    case CS_COMPLETING:
    case CS_COMPLETE_SENT:
    case CS_REC_COMMITTING:
    case CS_PREPARE_TO_COMMIT:
      jam();
      /***********************************************************************/
      // The transaction is already performing a commit but it is not concluded
      // yet.
      /***********************************************************************/
      errorCode = ZCOMMITINPROGRESS;
      break;
    case CS_ABORTING:
      jam();
      errorCode = regApiPtr->returncode ? 
	regApiPtr->returncode : ZABORTINPROGRESS;
      break;
    case CS_START_SCAN:
      jam();
      /***********************************************************************/
      // The transaction is a scan. Scans cannot commit
      /***********************************************************************/
      errorCode = ZSCANINPROGRESS;
      break;
    default:
      warningHandlerLab(signal, __LINE__);
      return;
    }//switch
    TcCommitRef * const commitRef = (TcCommitRef*)&signal->theData[0];
    commitRef->apiConnectPtr = apiConnectPtr;
    commitRef->transId1 = transId1;
    commitRef->transId2 = transId2;
    commitRef->errorCode = errorCode;
    sendSignal(apiBlockRef, GSN_TC_COMMITREF, signal, 
	       TcCommitRef::SignalLength, JBB);
    return;
  } else /** apiConnectptr.i < capiConnectFilesize */ {
    jam();
    warningHandlerLab(signal, __LINE__);
    return;
  }
}//Dbtc::execTC_COMMITREQ()

/**
 * TCROLLBACKREQ
 *
 * Format is:
 *
 * thedata[0] = apiconnectptr
 * thedata[1] = transid[0]
 * thedata[2] = transid[1]
 * OPTIONAL thedata[3] = flags
 *
 * Flags:
 *    0x1  =  potentiallyBad data from API (try not to assert)
 */
void Dbtc::execTCROLLBACKREQ(Signal* signal) 
{
  bool potentiallyBad= false;
  UintR compare_transid1, compare_transid2;

  jamEntry();

  if(unlikely((signal->getLength() >= 4) && (signal->theData[3] & 0x1)))
  {
    ndbout_c("Trying to roll back potentially bad txn\n");
    potentiallyBad= true;
  }

  apiConnectptr.i = signal->theData[0];
  if (apiConnectptr.i >= capiConnectFilesize) {
    goto TC_ROLL_warning;
  }//if
  ptrAss(apiConnectptr, apiConnectRecord);
  compare_transid1 = apiConnectptr.p->transid[0] ^ signal->theData[1];
  compare_transid2 = apiConnectptr.p->transid[1] ^ signal->theData[2];
  compare_transid1 = compare_transid1 | compare_transid2;
  if (compare_transid1 != 0) {
    jam();
    return;
  }//if

  apiConnectptr.p->m_flags |= ApiConnectRecord::TF_EXEC_FLAG;
  switch (apiConnectptr.p->apiConnectstate) {
  case CS_STARTED:
  case CS_RECEIVING:
    jam();
    apiConnectptr.p->returnsignal = RS_TCROLLBACKCONF;
    abort010Lab(signal);
    return;
  case CS_CONNECTED:
    jam();
    signal->theData[0] = apiConnectptr.p->ndbapiConnect;
    signal->theData[1] = apiConnectptr.p->transid[0];
    signal->theData[2] = apiConnectptr.p->transid[1];
    sendSignal(apiConnectptr.p->ndbapiBlockref, GSN_TCROLLBACKCONF, 
	       signal, 3, JBB);
    break;
  case CS_START_SCAN:
  case CS_PREPARE_TO_COMMIT:
  case CS_COMMITTING:
  case CS_COMMIT_SENT:
  case CS_COMPLETING:
  case CS_COMPLETE_SENT:
  case CS_WAIT_COMMIT_CONF:
  case CS_WAIT_COMPLETE_CONF:
  case CS_RESTART:
  case CS_DISCONNECTED:
  case CS_START_COMMITTING:
  case CS_REC_COMMITTING:
    jam();
    /* ***************< */
    /* TC_ROLLBACKREF < */
    /* ***************< */
    signal->theData[0] = apiConnectptr.p->ndbapiConnect;
    signal->theData[1] = apiConnectptr.p->transid[0];
    signal->theData[2] = apiConnectptr.p->transid[1];
    signal->theData[3] = ZROLLBACKNOTALLOWED;
    signal->theData[4] = apiConnectptr.p->apiConnectstate;
    sendSignal(apiConnectptr.p->ndbapiBlockref, GSN_TCROLLBACKREF, 
	       signal, 5, JBB);
    break;
                                                 /* SEND A REFUSAL SIGNAL*/
  case CS_ABORTING:
    jam();
    if (apiConnectptr.p->abortState == AS_IDLE) {
      jam();
      signal->theData[0] = apiConnectptr.p->ndbapiConnect;
      signal->theData[1] = apiConnectptr.p->transid[0];
      signal->theData[2] = apiConnectptr.p->transid[1];
      sendSignal(apiConnectptr.p->ndbapiBlockref, GSN_TCROLLBACKCONF, 
		 signal, 3, JBB);
    } else {
      jam();
      apiConnectptr.p->returnsignal = RS_TCROLLBACKCONF;
    }//if
    break;
  case CS_WAIT_ABORT_CONF:
    jam();
    apiConnectptr.p->returnsignal = RS_TCROLLBACKCONF;
    break;
  default:
    goto TC_ROLL_system_error;
    break;
  }//switch
  return;

TC_ROLL_warning:
  jam();
  if(likely(potentiallyBad==false))
    warningHandlerLab(signal, __LINE__);
  return;

TC_ROLL_system_error:
  jam();
  if(likely(potentiallyBad==false))
    systemErrorLab(signal, __LINE__);
  return;
}//Dbtc::execTCROLLBACKREQ()

void Dbtc::execTC_HBREP(Signal* signal) 
{
  const TcHbRep * const tcHbRep = 
    (TcHbRep *)signal->getDataPtr();

  jamEntry();
  apiConnectptr.i = tcHbRep->apiConnectPtr;
  ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);

  if (apiConnectptr.p->transid[0] == tcHbRep->transId1 &&
      apiConnectptr.p->transid[1] == tcHbRep->transId2){

    if (getApiConTimer(apiConnectptr.i) != 0){
      setApiConTimer(apiConnectptr.i, ctcTimer, __LINE__);
    } else {
      DEBUG("TCHBREP received when timer was off apiConnectptr.i=" 
	    << apiConnectptr.i);
    }
  }
}//Dbtc::execTCHBREP()

/*
4.3.15 ABORT 
-----------
*/
/*****************************************************************************/
/*                                  A B O R T                                */
/*                                                                           */
/*****************************************************************************/
void Dbtc::warningReport(Signal* signal, int place)
{
  switch (place) {
  case 0:
    jam();
#ifdef ABORT_TRACE
    ndbout << "ABORTED to not active TC record" << endl;
#endif
    break;
  case 1:
    jam();
#ifdef ABORT_TRACE
    ndbout << "ABORTED to TC record active with new transaction" << endl;
#endif
    break;
  case 2:
    jam();
#ifdef ABORT_TRACE
    ndbout << "ABORTED to active TC record not expecting ABORTED" << endl;
#endif
    break;
  case 3:
    jam();
#ifdef ABORT_TRACE
    ndbout << "ABORTED to TC rec active with trans but wrong node" << endl;
    ndbout << "This is ok when aborting in node failure situations" << endl;
#endif
    break;
  case 4:
    jam();
#ifdef ABORT_TRACE
    ndbout << "Received COMMITTED in wrong state in Dbtc" << endl;
#endif
    break;
  case 5:
    jam();
#ifdef ABORT_TRACE
    ndbout << "Received COMMITTED with wrong transid in Dbtc" << endl;
#endif
    break;
  case 6:
    jam();
#ifdef ABORT_TRACE
    ndbout << "Received COMPLETED in wrong state in Dbtc" << endl;
#endif
    break;
  case 7:
    jam();
#ifdef ABORT_TRACE
    ndbout << "Received COMPLETED with wrong transid in Dbtc" << endl;
#endif
    break;
  case 8:
    jam();
#ifdef ABORT_TRACE
    ndbout << "Received COMMITCONF with tc-rec in wrong state in Dbtc" << endl;
#endif
    break;
  case 9:
    jam();
#ifdef ABORT_TRACE
    ndbout << "Received COMMITCONF with api-rec in wrong state in Dbtc" <<endl;
#endif
    break;
  case 10:
    jam();
#ifdef ABORT_TRACE
    ndbout << "Received COMMITCONF with wrong transid in Dbtc" << endl;
#endif
    break;
  case 11:
    jam();
#ifdef ABORT_TRACE
    ndbout << "Received COMMITCONF from wrong nodeid in Dbtc" << endl;
#endif
    break;
  case 12:
    jam();
#ifdef ABORT_TRACE
    ndbout << "Received COMPLETECONF, tc-rec in wrong state in Dbtc" << endl;
#endif
    break;
  case 13:
    jam();
#ifdef ABORT_TRACE
    ndbout << "Received COMPLETECONF, api-rec in wrong state in Dbtc" << endl;
#endif
    break;
  case 14:
    jam();
#ifdef ABORT_TRACE
    ndbout << "Received COMPLETECONF with wrong transid in Dbtc" << endl;
#endif
    break;
  case 15:
    jam();
#ifdef ABORT_TRACE
    ndbout << "Received COMPLETECONF from wrong nodeid in Dbtc" << endl;
#endif
    break;
  case 16:
    jam();
#ifdef ABORT_TRACE
    ndbout << "Received ABORTCONF, tc-rec in wrong state in Dbtc" << endl;
#endif
    break;
  case 17:
    jam();
#ifdef ABORT_TRACE
    ndbout << "Received ABORTCONF, api-rec in wrong state in Dbtc" << endl;
#endif
    break;
  case 18:
    jam();
#ifdef ABORT_TRACE
    ndbout << "Received ABORTCONF with wrong transid in Dbtc" << endl;
#endif
    break;
  case 19:
    jam();
#ifdef ABORT_TRACE
    ndbout << "Received ABORTCONF from wrong nodeid in Dbtc" << endl;
#endif
    break;
  case 20:
    jam();
#ifdef ABORT_TRACE
    ndbout << "Time-out waiting for ABORTCONF in Dbtc" << endl;
#endif
    break;
  case 21:
    jam();
#ifdef ABORT_TRACE
    ndbout << "Time-out waiting for COMMITCONF in Dbtc" << endl;
#endif
    break;
  case 22:
    jam();
#ifdef ABORT_TRACE
    ndbout << "Time-out waiting for COMPLETECONF in Dbtc" << endl;
#endif
    break;
  case 23:
    jam();
#ifdef ABORT_TRACE
    ndbout << "Received LQHKEYCONF in wrong tc-state in Dbtc" << endl;
#endif
    break;
  case 24:
    jam();
#ifdef ABORT_TRACE
    ndbout << "Received LQHKEYREF to wrong transid in Dbtc" << endl;
#endif
    break;
  case 25:
    jam();
#ifdef ABORT_TRACE
    ndbout << "Received LQHKEYREF in wrong state in Dbtc" << endl;
#endif
    break;
  case 26:
    jam();
#ifdef ABORT_TRACE
    ndbout << "Received LQHKEYCONF to wrong transid in Dbtc" << endl;
#endif
    break;
  case 27:
    jam();
    // printState(signal, 27);
#ifdef ABORT_TRACE
    ndbout << "Received LQHKEYCONF in wrong api-state in Dbtc" << endl;
#endif
    break;
  case 28:
    jam();
#ifdef ABORT_TRACE
    ndbout << "Discarding FIRE_TRIG_REF/CONF in Dbtc" << endl;
#endif
    break;
  case 29:
    jam();
#ifdef ABORT_TRACE
    ndbout << "Discarding TcContinueB::ZSEND_FIRE_TRIG_REQ in Dbtc" << endl;
#endif
    break;
  default:
    jam();
    break;
  }//switch
  return;
}//Dbtc::warningReport()

void Dbtc::errorReport(Signal* signal, int place)
{
  switch (place) {
  case 0:
    jam();
    break;
  case 1:
    jam();
    break;
  case 2:
    jam();
    break;
  case 3:
    jam();
    break;
  case 4:
    jam();
    break;
  case 5:
    jam();
    break;
  case 6:
    jam();
    break;
  default:
    jam();
    break;
  }//switch
  systemErrorLab(signal, __LINE__);
  return;
}//Dbtc::errorReport()

/* ------------------------------------------------------------------------- */
/* -------                       ENTER ABORTED                       ------- */
/*                                                                           */
/*-------------------------------------------------------------------------- */
void Dbtc::execABORTED(Signal* signal) 
{
  UintR compare_transid1, compare_transid2;

  jamEntry();
  tcConnectptr.i = signal->theData[0];
  UintR Tnodeid = signal->theData[3];
  UintR TlastLqhInd = signal->theData[4];

  if (ERROR_INSERTED(8040)) {
    CLEAR_ERROR_INSERT_VALUE;
    sendSignalWithDelay(cownref, GSN_ABORTED, signal, 2000, 5);
    return;
  }//if
  /*------------------------------------------------------------------------
   *    ONE PARTICIPANT IN THE TRANSACTION HAS REPORTED THAT IT IS ABORTED.
   *------------------------------------------------------------------------*/
  if (tcConnectptr.i >= ctcConnectFilesize) {
    errorReport(signal, 0);
    return;
  }//if
  /*-------------------------------------------------------------------------
   *     WE HAVE TO CHECK THAT THIS IS NOT AN OLD SIGNAL BELONGING TO A     
   *     TRANSACTION ALREADY ABORTED. THIS CAN HAPPEN WHEN TIME-OUT OCCURS  
   *     IN TC WAITING FOR ABORTED.                                         
   *-------------------------------------------------------------------------*/
  ptrAss(tcConnectptr, tcConnectRecord);
  if (tcConnectptr.p->tcConnectstate != OS_ABORT_SENT) {
    warningReport(signal, 2);
    return;
    /*-----------------------------------------------------------------------*/
    // ABORTED reported on an operation not expecting ABORT.
    /*-----------------------------------------------------------------------*/
  }//if
  apiConnectptr.i = tcConnectptr.p->apiConnect;
  if (apiConnectptr.i >= capiConnectFilesize) {
    warningReport(signal, 0);
    return;
  }//if
  ptrAss(apiConnectptr, apiConnectRecord);
  compare_transid1 = apiConnectptr.p->transid[0] ^ signal->theData[1];
  compare_transid2 = apiConnectptr.p->transid[1] ^ signal->theData[2];
  compare_transid1 = compare_transid1 | compare_transid2;
  if (compare_transid1 != 0) {
    warningReport(signal, 1);
    return;
  }//if
  if (ERROR_INSERTED(8024)) {
    jam();
    systemErrorLab(signal, __LINE__);
  }//if

  /**
   * Release marker
   */
  clearCommitAckMarker(apiConnectptr.p, tcConnectptr.p);

  Uint32 i;
  Uint32 Tfound = 0;
  for (i = 0; i < tcConnectptr.p->noOfNodes; i++) {
    jam();
    if (tcConnectptr.p->tcNodedata[i] == Tnodeid) {
      /*---------------------------------------------------------------------
       * We have received ABORTED from one of the participants in this 
       * operation in this aborted transaction.
       * Record all nodes that have completed abort.
       * If last indicator is set it means that no more replica has 
       * heard of the operation and are thus also aborted.
       *---------------------------------------------------------------------*/
      jam();
      Tfound = 1;
      clearTcNodeData(signal, TlastLqhInd, i);
    }//if
  }//for
  if (Tfound == 0) {
    warningReport(signal, 3);
    return;
  }
  for (i = 0; i < tcConnectptr.p->noOfNodes; i++) {
    if (tcConnectptr.p->tcNodedata[i] != 0) {
      /*--------------------------------------------------------------------
       * There are still outstanding ABORTED's to wait for.
       *--------------------------------------------------------------------*/
      jam();
      return;
    }//if
  }//for
  tcConnectptr.p->noOfNodes = 0;
  tcConnectptr.p->tcConnectstate = OS_ABORTING;
  setApiConTimer(apiConnectptr.i, ctcTimer, __LINE__);
  apiConnectptr.p->counter--;
  if (apiConnectptr.p->counter > 0) {
    jam();
    /*----------------------------------------------------------------------
     *       WE ARE STILL WAITING FOR MORE PARTICIPANTS TO SEND ABORTED.    
     *----------------------------------------------------------------------*/
    return;
  }//if
  /*------------------------------------------------------------------------*/
  /*                                                                        */
  /*     WE HAVE NOW COMPLETED THE ABORT PROCESS. WE HAVE RECEIVED ABORTED  */
  /*     FROM ALL PARTICIPANTS IN THE TRANSACTION. WE CAN NOW RELEASE ALL   */
  /*     RESOURCES CONNECTED TO THE TRANSACTION AND SEND THE ABORT RESPONSE */
  /*------------------------------------------------------------------------*/
  releaseAbortResources(signal);
}//Dbtc::execABORTED()

void Dbtc::clearTcNodeData(Signal* signal, 
                           UintR TLastLqhIndicator,
                           UintR Tstart) 
{
  UintR Ti;
  if (TLastLqhIndicator == ZTRUE) {
    for (Ti = Tstart ; Ti < tcConnectptr.p->noOfNodes; Ti++) {
      jam();
      tcConnectptr.p->tcNodedata[Ti] = 0;
    }//for
  } else {
    jam();
    tcConnectptr.p->tcNodedata[Tstart] = 0;
  }//for
}//clearTcNodeData()

void Dbtc::abortErrorLab(Signal* signal) 
{
  ptrGuard(apiConnectptr);
  ApiConnectRecord * transP = apiConnectptr.p;
  if (transP->apiConnectstate == CS_ABORTING && transP->abortState != AS_IDLE){
    jam();
    return;
  }
  transP->returnsignal = RS_TCROLLBACKREP;
  if(transP->returncode == 0){
    jam();
    transP->returncode = terrorCode;
  }
  abort010Lab(signal);
}//Dbtc::abortErrorLab()

void Dbtc::abort010Lab(Signal* signal) 
{
  ApiConnectRecord * transP = apiConnectptr.p;
  if (transP->apiConnectstate == CS_ABORTING && transP->abortState != AS_IDLE){
    jam();
    return;
  }
  transP->apiConnectstate = CS_ABORTING;
  /*------------------------------------------------------------------------*/
  /*     AN ABORT DECISION HAS BEEN TAKEN FOR SOME REASON. WE NEED TO ABORT */
  /*     ALL PARTICIPANTS IN THE TRANSACTION.                               */
  /*------------------------------------------------------------------------*/
  transP->abortState = AS_ACTIVE;
  transP->counter = 0;

  if (transP->firstTcConnect == RNIL) {
    jam();
    /*--------------------------------------------------------------------*/
    /* WE HAVE NO PARTICIPANTS IN THE TRANSACTION.                        */
    /*--------------------------------------------------------------------*/
    releaseAbortResources(signal);
    return;
  }//if
  tcConnectptr.i = transP->firstTcConnect;
  abort015Lab(signal);
}//Dbtc::abort010Lab()

/*--------------------------------------------------------------------------*/
/*                                                                          */
/*       WE WILL ABORT ONE NODE PER OPERATION AT A TIME. THIS IS TO KEEP    */
/*       ERROR HANDLING OF THIS PROCESS FAIRLY SIMPLE AND TRACTABLE.        */
/*       EVEN IF NO NODE OF THIS PARTICULAR NODE NUMBER NEEDS ABORTION WE   */
/*       MUST ENSURE THAT ALL NODES ARE CHECKED. THUS A FAULTY NODE DOES    */
/*       NOT MEAN THAT ALL NODES IN AN OPERATION IS ABORTED. FOR THIS REASON*/
/*       WE SET THE TCONTINUE_ABORT TO TRUE WHEN A FAULTY NODE IS DETECTED. */
/*--------------------------------------------------------------------------*/
void Dbtc::abort015Lab(Signal* signal) 
{
  Uint32 TloopCount = 0;
ABORT020:
  jam();
  TloopCount++;
  ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
  switch (tcConnectptr.p->tcConnectstate) {
  case OS_WAIT_DIH:
  case OS_WAIT_KEYINFO:
  case OS_WAIT_ATTR:
    jam();
    /*----------------------------------------------------------------------*/
    /* WE ARE STILL WAITING FOR MORE KEYINFO/ATTRINFO. WE HAVE NOT CONTACTED*/
    /* ANY LQH YET AND SO WE CAN SIMPLY SET STATE TO ABORTING.              */
    /*----------------------------------------------------------------------*/
    tcConnectptr.p->noOfNodes = 0; // == releaseAbort(signal)
    tcConnectptr.p->tcConnectstate = OS_ABORTING;
    break;
  case OS_CONNECTED:
    jam();
    /*-----------------------------------------------------------------------
     *   WE ARE STILL IN THE INITIAL PHASE OF THIS OPERATION. 
     *   NEED NOT BOTHER ABOUT ANY LQH ABORTS.
     *-----------------------------------------------------------------------*/
    tcConnectptr.p->noOfNodes = 0; // == releaseAbort(signal)
    tcConnectptr.p->tcConnectstate = OS_ABORTING;
    break;
  case OS_PREPARED:
    jam();
    // Fall through
  case OS_OPERATING:
    jam();
    // Fall through
  case OS_FIRE_TRIG_REQ:
    jam();
    /*----------------------------------------------------------------------
     * WE HAVE SENT LQHKEYREQ AND ARE IN SOME STATE OF EITHER STILL       
     * SENDING THE OPERATION, WAITING FOR REPLIES, WAITING FOR MORE       
     * ATTRINFO OR OPERATION IS PREPARED. WE NEED TO ABORT ALL LQH'S.     
     *----------------------------------------------------------------------*/
    releaseAndAbort(signal);
    tcConnectptr.p->tcConnectstate = OS_ABORT_SENT;
    TloopCount += 127;
    break;
  case OS_ABORTING:
    jam();
    break;
  case OS_ABORT_SENT:
    jam();
    DEBUG("ABORT_SENT state in abort015Lab(), not expected");
    systemErrorLab(signal, __LINE__);
    return;
  default:
    jam();
    DEBUG("tcConnectstate = " << tcConnectptr.p->tcConnectstate);
    systemErrorLab(signal, __LINE__);
    return;
  }//switch

  if (tcConnectptr.p->nextTcConnect != RNIL) {
    jam();
    tcConnectptr.i = tcConnectptr.p->nextTcConnect;
    if (TloopCount < 1024 &&
        !ERROR_INSERTED(8089) &&
        !ERROR_INSERTED(8105))
    {
      goto ABORT020;
    }
    else
    {
      jam();
      /*---------------------------------------------------------------------
       * Reset timer to avoid time-out in real-time break.
       * Increase counter to ensure that we don't think that all ABORTED have
       * been received before all have been sent.
       *---------------------------------------------------------------------*/
      apiConnectptr.p->counter++;
      setApiConTimer(apiConnectptr.i, ctcTimer, __LINE__);
      signal->theData[0] = TcContinueB::ZABORT_BREAK;
      signal->theData[1] = tcConnectptr.i;
      signal->theData[2] = apiConnectptr.i;
      signal->theData[3] = apiConnectptr.p->transid[0];
      signal->theData[4] = apiConnectptr.p->transid[1];
      if (ERROR_INSERTED(8089))
      {
        sendSignalWithDelay(cownref, GSN_CONTINUEB, signal, 100, 3);
        return;
      }
      sendSignal(cownref, GSN_CONTINUEB, signal, 5, JBB);
      return;
    }//if
  }//if

  if (ERROR_INSERTED(8089))
  {
    CLEAR_ERROR_INSERT_VALUE;
  }

  if (apiConnectptr.p->counter > 0) {
    jam();
    setApiConTimer(apiConnectptr.i, ctcTimer, __LINE__);
    return;
  }//if
  /*-----------------------------------------------------------------------
   *    WE HAVE NOW COMPLETED THE ABORT PROCESS. WE HAVE RECEIVED ABORTED 
   *    FROM ALL PARTICIPANTS IN THE TRANSACTION. WE CAN NOW RELEASE ALL  
   *    RESOURCES CONNECTED TO THE TRANSACTION AND SEND THE ABORT RESPONSE
   *------------------------------------------------------------------------*/
  releaseAbortResources(signal);
}//Dbtc::abort015Lab()

/*--------------------------------------------------------------------------*/
/*       RELEASE KEY AND ATTRINFO OBJECTS AND SEND ABORT TO THE LQH BLOCK.  */
/*--------------------------------------------------------------------------*/
int Dbtc::releaseAndAbort(Signal* signal) 
{
  HostRecordPtr localHostptr;
  UintR TnoLoops = tcConnectptr.p->noOfNodes;
  
  apiConnectptr.p->counter++;
  bool prevAlive = false;
  for (Uint32 Ti = 0; Ti < TnoLoops ; Ti++) {
    localHostptr.i = tcConnectptr.p->tcNodedata[Ti];
    ptrCheckGuard(localHostptr, chostFilesize, hostRecord);
    if (localHostptr.p->hostStatus == HS_ALIVE) {
      jam();
      if (prevAlive) {
        // if previous is alive, its LQH forwards abort to this node
        jam();
        continue;
      }
      /* ************< */
      /*    ABORT    < */
      /* ************< */
      Uint32 instanceKey = tcConnectptr.p->lqhInstanceKey;
      tblockref = numberToRef(DBLQH, instanceKey, localHostptr.i);
      signal->theData[0] = tcConnectptr.i;
      signal->theData[1] = cownref;
      signal->theData[2] = apiConnectptr.p->transid[0];
      signal->theData[3] = apiConnectptr.p->transid[1];
      sendSignal(tblockref, GSN_ABORT, signal, 4, JBB);
      prevAlive = true;
    } else {
      jam();
      signal->theData[0] = tcConnectptr.i;
      signal->theData[1] = apiConnectptr.p->transid[0];
      signal->theData[2] = apiConnectptr.p->transid[1];
      signal->theData[3] = localHostptr.i;
      signal->theData[4] = ZFALSE;
      sendSignal(cownref, GSN_ABORTED, signal, 5, JBB);
      prevAlive = false;
    }//if
  }//for
  return 1;
}//Dbtc::releaseAndAbort()

/* ------------------------------------------------------------------------- */
/* -------                       ENTER TIME_SIGNAL                   ------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void Dbtc::execTIME_SIGNAL(Signal* signal) 
{
  jamEntry();

  const NDB_TICKS currentTime = NdbTick_getCurrentTicks();
  Uint64 num_ms_elapsed = elapsed_time(signal,
                                       currentTime,
                                       c_latestTIME_SIGNAL,
                                       Uint32(TC_TIME_SIGNAL_DELAY));
  sendTIME_SIGNAL(signal, currentTime, Uint32(TC_TIME_SIGNAL_DELAY));

  /**
   * The code here will make calls to timer_handling that appears to happen
   * every 10ms but in reality they come in batches of up to 10 10ms
   * intervals. The timer_handling will check if a new scan of PK timeouts
   * is to start, this happens after an idle check period of 500ms. So
   * we actually need only check this a divisor of 500 ms and 400ms. 400ms
   * is how long we are idle between scans to check for scan timeouts.
   * The reason to separate those is to make sure that they don't run
   * at the same time as much.
   * 
   * One reason to run the updates of the timer_handling as often as once
   * per 50ms is that it is also then that we update the timer that is used
   * to check for timeouts on transaction. So 50ms is the granularity of
   * when we see timeouts.
   *
   * The reason for keeping the 10ms appearance of timer_handling is to
   * decrease changes in this part of the code. This could be changed
   * in the future.
   */
  c_elapsed_time_millis += num_ms_elapsed;
  while (c_elapsed_time_millis > Uint64(10))
  {
    jam();
    c_elapsed_time_millis -= Uint64(10);
    timer_handling(signal);
  }
}//execTIME_SIGNAL()

void Dbtc::timer_handling(Signal *signal)
{
  ctcTimer++;
  checkStartTimeout(signal);
  checkStartFragTimeout(signal);
}

/*------------------------------------------------*/
/* Start timeout handling if not already going on */
/*------------------------------------------------*/
void Dbtc::checkStartTimeout(Signal* signal)
{
  ctimeOutCheckCounter++;
  if (ctimeOutCheckActive == TOCS_TRUE) {
    jam();
    // Check heartbeat of timeout loop
    if(ctimeOutCheckHeartbeat > ctimeOutCheckLastHeartbeat){
      jam();
      ctimeOutMissedHeartbeats = 0;
    }else{
      jam();
      /**
       * This code simply checks that the scan for PK timeouts haven't
       * lost itself, a sort of watchdog for that code. If that would
       * occur we would no longer have any timeout handling of
       * transactions which would render the system useless. This is
       * to protect ourselves for future bugs in the code that we
       * might introduce by mistake.
       * 
       * 100 is simply an arbitrary number that needs to be bigger
       * than the maximum number of calls to timer_handling that
       * we can have in one call to execTIME_SIGNAL. This is
       * currently 10 but could change in the future.
       */
      ctimeOutMissedHeartbeats++;
      if (ctimeOutMissedHeartbeats > 100){
	jam();
	systemErrorLab(signal, __LINE__);
      }
    }
    ctimeOutCheckLastHeartbeat = ctimeOutCheckHeartbeat;
    return;
  }//if
  if (ctimeOutCheckCounter < ctimeOutCheckDelay) {
    jam();
    /*------------------------------------------------------------------*/
    /*                                                                  */
    /*       NO TIME-OUT CHECKED THIS TIME. WAIT MORE.                  */
    /*------------------------------------------------------------------*/
    return;
  }//if
  ctimeOutCheckActive = TOCS_TRUE;
  ctimeOutCheckCounter = 0;
  ctimeOutMissedHeartbeats = 0;
  timeOutLoopStartLab(signal, 0); // 0 is first api connect record
  return;
}//checkStartTimeout()

/*----------------------------------------------------------------*/
/* Start fragment (scan) timeout handling if not already going on */
/*----------------------------------------------------------------*/
void Dbtc::checkStartFragTimeout(Signal* signal)
{
  ctimeOutCheckFragCounter++;
  if (ctimeOutCheckFragActive == TOCS_TRUE) {
    jam();
    // Check heartbeat of scan timeout loop
    if(ctimeOutCheckHeartbeatScan > ctimeOutCheckLastHeartbeatScan)
    {
      jam();
      ctimeOutMissedHeartbeatsScan = 0;
    }
    else
    {
      /**
       * Exactly the same code as for PK timeouts, but here to check the
       * scan for scan timeouts. Same comments apply as above.
       */
      jam();
      ctimeOutMissedHeartbeatsScan++;
      if (ctimeOutMissedHeartbeatsScan > 100){
	jam();
	systemErrorLab(signal, __LINE__);
      }
    }
    ctimeOutCheckLastHeartbeatScan = ctimeOutCheckHeartbeatScan;
    return;
  }//if
  if (ctimeOutCheckFragCounter < ctimeOutCheckDelayScan) {
    jam();
    /*------------------------------------------------------------------*/
    /*       NO TIME-OUT CHECKED THIS TIME. WAIT MORE.                  */
    /*------------------------------------------------------------------*/
    return;
  }//if

  // Go through the fragment records and look for timeout in a scan.
  ctimeOutCheckFragActive = TOCS_TRUE;
  ctimeOutCheckFragCounter = 0;
  ctimeOutMissedHeartbeatsScan = 0;
  timeOutLoopStartFragLab(signal, 0); // 0 means first scan record
}//checkStartFragTimeout()

/*------------------------------------------------------------------*/
/*       IT IS NOW TIME TO CHECK WHETHER ANY TRANSACTIONS HAVE      */
/*       BEEN DELAYED FOR SO LONG THAT WE ARE FORCED TO PERFORM     */
/*       SOME ACTION, EITHER ABORT OR RESEND OR REMOVE A NODE FROM  */
/*       THE WAITING PART OF A PROTOCOL.                            */
/*
The algorithm used here is to check 1024 transactions at a time before
doing a real-time break.
To avoid aborting both transactions in a deadlock detected by time-out
we insert a random extra time-out of upto 630 ms by using the lowest
six bits of the api connect reference.
We spread it out from 0 to 630 ms if base time-out is larger than 3 sec,
we spread it out from 0 to 70 ms if base time-out is smaller than 300 msec,
and otherwise we spread it out 310 ms.
*/
/*------------------------------------------------------------------*/
void Dbtc::timeOutLoopStartLab(Signal* signal, Uint32 api_con_ptr) 
{
  Uint32 end_ptr, time_passed, time_out_value, mask_value;
  Uint32 old_mask_value= 0;
  const Uint32 api_con_sz= capiConnectFilesize;
  const Uint32 tc_timer= ctcTimer;
  const Uint32 time_out_param= ctimeOutValue;
  const Uint32 old_time_out_param= c_abortRec.oldTimeOutValue;

  ctimeOutCheckHeartbeat = tc_timer;

  if (api_con_ptr + 1024 < api_con_sz) {
    jam();
    end_ptr= api_con_ptr + 1024;
  } else {
    jam();
    end_ptr= api_con_sz;
  }
  if (time_out_param > 300) {
    jam();
    mask_value= 63;
  } else if (time_out_param < 30) {
    jam();
    mask_value= 7;
  } else {
    jam();
    mask_value= 31;
  }
  if (time_out_param != old_time_out_param &&
      getNodeState().getSingleUserMode())
  {
    // abort during single user mode, use old_mask_value as flag
    // and calculate value to be used for connections with allowed api
    if (old_time_out_param > 300) {
      jam();
      old_mask_value= 63;
    } else if (old_time_out_param < 30) {
      jam();
      old_mask_value= 7;
    } else {
      jam();
      old_mask_value= 31;
    }
  }
  for ( ; api_con_ptr < end_ptr; api_con_ptr++) {
    Uint32 api_timer= getApiConTimer(api_con_ptr);
    if (api_timer != 0) {
      jam();
      jamLine(api_con_ptr & 0xFFFF);
      Uint32 error= ZTIME_OUT_ERROR;
      time_out_value= time_out_param + (ndb_rand() & mask_value);
      if (unlikely(old_mask_value)) // abort during single user mode
      {
        apiConnectptr.i = api_con_ptr;
        ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
        if ((getNodeState().getSingleUserApi() ==
             refToNode(apiConnectptr.p->ndbapiBlockref)) ||
            !(apiConnectptr.p->singleUserMode & (1 << NDB_SUM_LOCKED)))
        {
          // api allowed during single user, use original timeout
          time_out_value=
            old_time_out_param + (api_con_ptr & old_mask_value);
        }
        else
        {
          error= ZCLUSTER_IN_SINGLEUSER_MODE;
        }
      }
      time_passed= tc_timer - api_timer;
      if (time_passed > time_out_value) 
      {
        jam();
        timeOutFoundLab(signal, api_con_ptr, error);
	api_con_ptr++;
	break;
      }
    }
  }
  if (api_con_ptr == api_con_sz) {
    jam();
    /*------------------------------------------------------------------*/
    /*                                                                  */
    /*       WE HAVE NOW CHECKED ALL TRANSACTIONS FOR TIME-OUT AND ALSO */
    /*       STARTED TIME-OUT HANDLING OF THOSE WE FOUND. WE ARE NOW    */
    /*       READY AND CAN WAIT FOR THE NEXT TIME-OUT CHECK.            */
    /*------------------------------------------------------------------*/
    ctimeOutCheckActive = TOCS_FALSE;
  } else {
    jam();
    sendContinueTimeOutControl(signal, api_con_ptr);
  }
  return;
}//Dbtc::timeOutLoopStartLab()

void Dbtc::timeOutFoundLab(Signal* signal, Uint32 TapiConPtr, Uint32 errCode) 
{
  apiConnectptr.i = TapiConPtr;
  ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
  /*------------------------------------------------------------------*/
  /*                                                                  */
  /*       THIS TRANSACTION HAVE EXPERIENCED A TIME-OUT AND WE NEED TO*/
  /*       FIND OUT WHAT WE NEED TO DO BASED ON THE STATE INFORMATION.*/
  /*------------------------------------------------------------------*/
  DEBUG("[ H'" << hex << apiConnectptr.p->transid[0] 
	<< " H'" << apiConnectptr.p->transid[1] << "] " << dec 
	<< "Time-out in state = " << apiConnectptr.p->apiConnectstate
	<< " apiConnectptr.i = " << apiConnectptr.i 
	<< " - exec: "
        << tc_testbit(apiConnectptr.p->m_flags, ApiConnectRecord::TF_EXEC_FLAG)
	<< " - place: " << c_apiConTimer_line[apiConnectptr.i]
	<< " code: " << errCode
        << " lqhkeyreqrec: " << apiConnectptr.p->lqhkeyreqrec
        << " lqhkeyconfrec: " << apiConnectptr.p->lqhkeyconfrec
        << " pendingTriggers: " << apiConnectptr.p->pendingTriggers
        );
  switch (apiConnectptr.p->apiConnectstate) {
  case CS_STARTED:
    if(apiConnectptr.p->lqhkeyreqrec == apiConnectptr.p->lqhkeyconfrec &&
       errCode != ZCLUSTER_IN_SINGLEUSER_MODE){
      jam();
      /*
      We are waiting for application to continue the transaction. In this
      particular state we will use the application timeout parameter rather
      than the shorter Deadlock detection timeout.
      */
      if (c_appl_timeout_value == 0 ||
          (ctcTimer - getApiConTimer(apiConnectptr.i)) <= c_appl_timeout_value) {
        jam();
        return;
      }//if
    }
    apiConnectptr.p->returnsignal = RS_TCROLLBACKREP;      
    apiConnectptr.p->returncode = errCode;
    abort010Lab(signal);
    return;
  case CS_RECEIVING:
  case CS_REC_COMMITTING:
  case CS_START_COMMITTING:
  case CS_WAIT_FIRE_TRIG_REQ:
  case CS_SEND_FIRE_TRIG_REQ:
    jam();
    /*------------------------------------------------------------------*/
    /*       WE ARE STILL IN THE PREPARE PHASE AND THE TRANSACTION HAS  */
    /*       NOT YET REACHED ITS COMMIT POINT. THUS IT IS NOW OK TO     */
    /*       START ABORTING THE TRANSACTION. ALSO START CHECKING THE    */
    /*       REMAINING TRANSACTIONS.                                    */
    /*------------------------------------------------------------------*/
    terrorCode = errCode;
    abortErrorLab(signal);
    return;
  case CS_COMMITTING:
    jam();
    /*------------------------------------------------------------------*/
    // We are simply waiting for a signal in the job buffer. Only extreme
    // conditions should get us here. We ignore it.
    /*------------------------------------------------------------------*/
    // Fall through
  case CS_COMPLETING:
    jam();
    /*------------------------------------------------------------------*/
    // We are simply waiting for a signal in the job buffer. Only extreme
    // conditions should get us here. We ignore it.
    /*------------------------------------------------------------------*/
    // Fall through
  case CS_PREPARE_TO_COMMIT:
  {
    jam();
    /*------------------------------------------------------------------*/
    /*       WE ARE WAITING FOR DIH TO COMMIT THE TRANSACTION. WE SIMPLY*/
    /*       KEEP WAITING SINCE THERE IS NO BETTER IDEA ON WHAT TO DO.  */
    /*       IF IT IS BLOCKED THEN NO TRANSACTION WILL PASS THIS GATE.  */
    // To ensure against strange bugs we crash the system if we have passed
    // time-out period by a factor of 10 and it is also at least 5 seconds.
    /*------------------------------------------------------------------*/
    Uint32 time_passed = ctcTimer - getApiConTimer(apiConnectptr.i);
    if (time_passed > 500 &&
        time_passed > (5 * cDbHbInterval) &&
        time_passed > (10 * ctimeOutValue))
    {
      jam();
      ndbout_c("timeOutFoundLab trans: 0x%x 0x%x state: %u",
               apiConnectptr.p->transid[0],
               apiConnectptr.p->transid[1],
               (Uint32)apiConnectptr.p->apiConnectstate);

      // Reset timeout to not flood log...
      setApiConTimer(apiConnectptr.i, 0, __LINE__);
    }//if
    break;
  }
  case CS_COMMIT_SENT:
    jam();
    /*------------------------------------------------------------------*/
    /*       WE HAVE SENT COMMIT TO A NUMBER OF NODES. WE ARE CURRENTLY */
    /*       WAITING FOR THEIR REPLY. WITH NODE RECOVERY SUPPORTED WE   */
    /*       WILL CHECK FOR CRASHED NODES AND RESEND THE COMMIT SIGNAL  */
    /*       TO THOSE NODES THAT HAVE MISSED THE COMMIT SIGNAL DUE TO   */
    /*       A NODE FAILURE.                                            */
    /*------------------------------------------------------------------*/
    tabortInd = ZCOMMIT_SETUP;
    setupFailData(signal);
    toCommitHandlingLab(signal);
    return;
  case CS_COMPLETE_SENT:
    jam();
    /*--------------------------------------------------------------------*/
    /*       WE HAVE SENT COMPLETE TO A NUMBER OF NODES. WE ARE CURRENTLY */
    /*       WAITING FOR THEIR REPLY. WITH NODE RECOVERY SUPPORTED WE     */
    /*       WILL CHECK FOR CRASHED NODES AND RESEND THE COMPLETE SIGNAL  */
    /*       TO THOSE NODES THAT HAVE MISSED THE COMPLETE SIGNAL DUE TO   */
    /*       A NODE FAILURE.                                              */
    /*--------------------------------------------------------------------*/
    tabortInd = ZCOMMIT_SETUP;
    setupFailData(signal);
    toCompleteHandlingLab(signal);
    return;
  case CS_ABORTING:
    jam();
    /*------------------------------------------------------------------*/
    /*       TIME-OUT DURING ABORT. WE NEED TO SEND ABORTED FOR ALL     */
    /*       NODES THAT HAVE FAILED BEFORE SENDING ABORTED.             */
    /*------------------------------------------------------------------*/
    tcConnectptr.i = apiConnectptr.p->firstTcConnect;
    sendAbortedAfterTimeout(signal, 0);
    break;
  case CS_START_SCAN:{
    jam();

    /*
      We are waiting for application to continue the transaction. In this
      particular state we will use the application timeout parameter rather
      than the shorter Deadlock detection timeout.
    */
    if (c_appl_timeout_value == 0 ||
	(ctcTimer - getApiConTimer(apiConnectptr.i)) <= c_appl_timeout_value) {
      jam();
      return;
    }//if
    
    ScanRecordPtr scanPtr;
    scanPtr.i = apiConnectptr.p->apiScanRec;
    ptrCheckGuard(scanPtr, cscanrecFileSize, scanRecord);
    scanError(signal, scanPtr, ZSCANTIME_OUT_ERROR);
    break;
  }
  case CS_WAIT_ABORT_CONF:
    jam();
    tcConnectptr.i = apiConnectptr.p->currentTcConnect;
    ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
    arrGuard(apiConnectptr.p->currentReplicaNo, MAX_REPLICAS);
    hostptr.i = tcConnectptr.p->tcNodedata[apiConnectptr.p->currentReplicaNo];
    ptrCheckGuard(hostptr, chostFilesize, hostRecord);
    if (hostptr.p->hostStatus == HS_ALIVE) {
      /*------------------------------------------------------------------*/
      // Time-out waiting for ABORTCONF. We will resend the ABORTREQ just in
      // case.
      /*------------------------------------------------------------------*/
      warningReport(signal, 20);
      apiConnectptr.p->timeOutCounter++;
      if (apiConnectptr.p->timeOutCounter > 3) {
	/*------------------------------------------------------------------*/
	// 100 time-outs are not acceptable. We will shoot down the node
	// not responding.
	/*------------------------------------------------------------------*/
        reportNodeFailed(signal, hostptr.i);
      }//if
      apiConnectptr.p->currentReplicaNo++;
    }//if
    tcurrentReplicaNo = (Uint8)Z8NIL;
    toAbortHandlingLab(signal);
    return;
  case CS_WAIT_COMMIT_CONF:
    jam();
    CRASH_INSERTION(8053);
    tcConnectptr.i = apiConnectptr.p->currentTcConnect;
    ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
    arrGuard(apiConnectptr.p->currentReplicaNo, MAX_REPLICAS);
    hostptr.i = tcConnectptr.p->tcNodedata[apiConnectptr.p->currentReplicaNo];
    ptrCheckGuard(hostptr, chostFilesize, hostRecord);
    if (hostptr.p->hostStatus == HS_ALIVE) {
      /*------------------------------------------------------------------*/
      // Time-out waiting for COMMITCONF. We will resend the COMMITREQ just in
      // case.
      /*------------------------------------------------------------------*/
      warningReport(signal, 21);
      apiConnectptr.p->timeOutCounter++;
      if (apiConnectptr.p->timeOutCounter > 3) {
	/*------------------------------------------------------------------*/
	// 100 time-outs are not acceptable. We will shoot down the node
	// not responding.
	/*------------------------------------------------------------------*/
        reportNodeFailed(signal, hostptr.i);
      }//if
      apiConnectptr.p->currentReplicaNo++;
    }//if
    tcurrentReplicaNo = (Uint8)Z8NIL;
    toCommitHandlingLab(signal);
    return;
  case CS_WAIT_COMPLETE_CONF:
    jam();
    tcConnectptr.i = apiConnectptr.p->currentTcConnect;
    ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
    arrGuard(apiConnectptr.p->currentReplicaNo, MAX_REPLICAS);
    hostptr.i = tcConnectptr.p->tcNodedata[apiConnectptr.p->currentReplicaNo];
    ptrCheckGuard(hostptr, chostFilesize, hostRecord);
    if (hostptr.p->hostStatus == HS_ALIVE) {
      /*------------------------------------------------------------------*/
      // Time-out waiting for COMPLETECONF. We will resend the COMPLETEREQ
      // just in case.
      /*------------------------------------------------------------------*/
      warningReport(signal, 22);
      apiConnectptr.p->timeOutCounter++;
      if (apiConnectptr.p->timeOutCounter > 100) {
	/*------------------------------------------------------------------*/
	// 100 time-outs are not acceptable. We will shoot down the node
	// not responding.
	/*------------------------------------------------------------------*/
        reportNodeFailed(signal, hostptr.i);
      }//if
      apiConnectptr.p->currentReplicaNo++;
    }//if
    tcurrentReplicaNo = (Uint8)Z8NIL;
    toCompleteHandlingLab(signal);
    return;
  case CS_FAIL_PREPARED:
  case CS_FAIL_COMMITTING:
  case CS_FAIL_COMMITTED:
  case CS_RESTART:
  case CS_FAIL_ABORTED:
  case CS_DISCONNECTED:
  default:
    jam();
    jamLine(apiConnectptr.p->apiConnectstate);
    /*------------------------------------------------------------------*/
    /*       AN IMPOSSIBLE STATE IS SET. CRASH THE SYSTEM.              */
    /*------------------------------------------------------------------*/
    DEBUG("State = " << apiConnectptr.p->apiConnectstate);
    systemErrorLab(signal, __LINE__);
    return;
  }//switch
  return;
}//Dbtc::timeOutFoundLab()

void Dbtc::sendAbortedAfterTimeout(Signal* signal, int Tcheck)
{
  ApiConnectRecord * transP = apiConnectptr.p;
  if(transP->abortState == AS_IDLE){
    jam();
    warningEvent("TC: %d: %d state=%d abort==IDLE place: %d fop=%d t: %d", 
		 __LINE__,
		 apiConnectptr.i, 
		 transP->apiConnectstate,
		 c_apiConTimer_line[apiConnectptr.i],
		 transP->firstTcConnect,
		 c_apiConTimer[apiConnectptr.i]
		 );
    ndbout_c("TC: %d: %d state=%d abort==IDLE place: %d fop=%d t: %d", 
	     __LINE__,
	     apiConnectptr.i, 
	     transP->apiConnectstate,
	     c_apiConTimer_line[apiConnectptr.i],
	     transP->firstTcConnect,
	     c_apiConTimer[apiConnectptr.i]
	     );
    ndbabort();
    setApiConTimer(apiConnectptr.i, 0, __LINE__);
    return;
  }
  
  bool found = false;
  OperationState tmp[16];
  
  Uint32 TloopCount = 0;
  do {
    jam();
    if (tcConnectptr.i == RNIL) {
      jam();

#ifdef VM_TRACE
      ndbout_c("found: %d Tcheck: %d apiConnectptr.p->counter: %d",
	       found, Tcheck, apiConnectptr.p->counter);
#endif
      if (found || apiConnectptr.p->counter)
      {
	jam();
	/**
	 * We sent atleast one ABORT/ABORTED
	 *   or ZABORT_TIMEOUT_BREAK is in job buffer
	 *   wait for reception...
	 */
	return;
      }
      
      if (Tcheck == 1)
      {
	jam();
	releaseAbortResources(signal);
	return;
      }
      
      if (Tcheck == 0)
      {
        jam();
	/*------------------------------------------------------------------
	 * All nodes had already reported ABORTED for all tcConnect records.
	 * Crash since it is an error situation that we then received a 
	 * time-out.
	 *------------------------------------------------------------------*/
	char buf[96]; buf[0] = 0;
	char buf2[96];
	BaseString::snprintf(buf, sizeof(buf), "TC %d: %d counter: %d ops:",
			     __LINE__, apiConnectptr.i,
			     apiConnectptr.p->counter);
	for(Uint32 i = 0; i<TloopCount; i++)
	{
	  BaseString::snprintf(buf2, sizeof(buf2), "%s %d", buf, tmp[i]);
	  BaseString::snprintf(buf, sizeof(buf), "%s", buf2);
	}
	warningEvent("%s", buf);
	ndbout_c("%s", buf);
	ndbabort();
	releaseAbortResources(signal);
	return;
      }
      
      return;
    }//if
    TloopCount++;
    if (TloopCount >= 1024) {
      jam();
      /*------------------------------------------------------------------*/
      // Insert a real-time break for large transactions to avoid blowing
      // away the job buffer.
      /*------------------------------------------------------------------*/
      setApiConTimer(apiConnectptr.i, ctcTimer, __LINE__);
      apiConnectptr.p->counter++;
      signal->theData[0] = TcContinueB::ZABORT_TIMEOUT_BREAK;
      signal->theData[1] = tcConnectptr.i;
      signal->theData[2] = apiConnectptr.i;      
      if (ERROR_INSERTED(8080))
      {
	ndbout_c("sending ZABORT_TIMEOUT_BREAK delayed (%d %d)", 
		 Tcheck, apiConnectptr.p->counter);
	sendSignalWithDelay(cownref, GSN_CONTINUEB, signal, 2000, 3);
      }
      else
      {
	sendSignal(cownref, GSN_CONTINUEB, signal, 3, JBB);
      }
      return;
    }//if
    ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
    if(TloopCount < 16){
      jam();
      tmp[TloopCount-1] = tcConnectptr.p->tcConnectstate;
    }

    if (tcConnectptr.p->tcConnectstate == OS_ABORT_SENT) {
      jam();
      /*------------------------------------------------------------------*/
      // We have sent an ABORT signal to this node but not yet received any
      // reply. We have to send an ABORTED signal on our own in some cases.
      // If the node is declared as up and running and still do not respond
      // in time to the ABORT signal we will declare it as dead.
      /*------------------------------------------------------------------*/
      UintR Ti = 0;
      arrGuard(tcConnectptr.p->noOfNodes, MAX_REPLICAS+1);
      for (Ti = 0; Ti < tcConnectptr.p->noOfNodes; Ti++) {
        jam();
        if (tcConnectptr.p->tcNodedata[Ti] != 0) {
          TloopCount += 31;
	  found = true;
          hostptr.i = tcConnectptr.p->tcNodedata[Ti];
          ptrCheckGuard(hostptr, chostFilesize, hostRecord);
          if (hostptr.p->hostStatus == HS_ALIVE) {
            jam();
	    /*---------------------------------------------------------------
	     * A backup replica has not sent ABORTED. 
	     * Could be that a node before him has crashed. 
	     * Send an ABORT signal specifically to this node.
	     * We will not send to any more nodes after this 
	     * to avoid race problems.
	     * To also ensure that we use this message also as a heartbeat 
	     * we will move this node to the primary replica seat. 
	     * The primary replica and any failed node after it will 
	     * be removed from the node list. Update also number of nodes. 
	     * Finally break the loop to ensure we don't mess
	     * things up by executing another loop. 
	     * We also update the timer to ensure we don't get time-out 
	     * too early.
	     *--------------------------------------------------------------*/
            Uint32 instanceKey = tcConnectptr.p->lqhInstanceKey;
            BlockReference TBRef = numberToRef(DBLQH, instanceKey, hostptr.i);
            signal->theData[0] = tcConnectptr.i;
            signal->theData[1] = cownref;
            signal->theData[2] = apiConnectptr.p->transid[0];
            signal->theData[3] = apiConnectptr.p->transid[1];
            sendSignal(TBRef, GSN_ABORT, signal, 4, JBB);
            setApiConTimer(apiConnectptr.i, ctcTimer, __LINE__);
            break;
          } else {
            jam();
	    /*--------------------------------------------------------------
	     * The node we are waiting for is dead. We will send ABORTED to
	     * ourselves vicarious for the failed node.
	     *--------------------------------------------------------------*/
            setApiConTimer(apiConnectptr.i, ctcTimer, __LINE__);
            signal->theData[0] = tcConnectptr.i;
            signal->theData[1] = apiConnectptr.p->transid[0];
            signal->theData[2] = apiConnectptr.p->transid[1];
            signal->theData[3] = hostptr.i;
            signal->theData[4] = ZFALSE;
            sendSignal(cownref, GSN_ABORTED, signal, 5, JBB);
          }//if
        }//if
      }//for
    }//if
    tcConnectptr.i = tcConnectptr.p->nextTcConnect;
  } while (1);
}//Dbtc::sendAbortedAfterTimeout()

void Dbtc::reportNodeFailed(Signal* signal, NodeId nodeId)
{
  DisconnectRep * const rep = (DisconnectRep *)&signal->theData[0];
  rep->nodeId = nodeId;
  rep->err = DisconnectRep::TcReportNodeFailed;
  sendSignal(QMGR_REF, GSN_DISCONNECT_REP, signal, 
	     DisconnectRep::SignalLength, JBB);
}//Dbtc::reportNodeFailed()

/*-------------------------------------------------*/
/*      Timeout-loop for scanned fragments.        */
/*-------------------------------------------------*/
void Dbtc::timeOutLoopStartFragLab(Signal* signal, Uint32 TscanConPtr)
{
  ScanFragRecPtr timeOutPtr[8];
  UintR tfragTimer[8];
  UintR texpiredTime[8];
  UintR TloopCount = 0;
  Uint32 TtcTimer = ctcTimer;

  ctimeOutCheckHeartbeatScan = TtcTimer;

  while ((TscanConPtr + 8) < cscanFragrecFileSize) {
    jam();
    timeOutPtr[0].i  = TscanConPtr + 0;
    timeOutPtr[1].i  = TscanConPtr + 1;
    timeOutPtr[2].i  = TscanConPtr + 2;
    timeOutPtr[3].i  = TscanConPtr + 3;
    timeOutPtr[4].i  = TscanConPtr + 4;
    timeOutPtr[5].i  = TscanConPtr + 5;
    timeOutPtr[6].i  = TscanConPtr + 6;
    timeOutPtr[7].i  = TscanConPtr + 7;
    
    c_scan_frag_pool.getPtrForce(timeOutPtr[0]);
    c_scan_frag_pool.getPtrForce(timeOutPtr[1]);
    c_scan_frag_pool.getPtrForce(timeOutPtr[2]);
    c_scan_frag_pool.getPtrForce(timeOutPtr[3]);
    c_scan_frag_pool.getPtrForce(timeOutPtr[4]);
    c_scan_frag_pool.getPtrForce(timeOutPtr[5]);
    c_scan_frag_pool.getPtrForce(timeOutPtr[6]);
    c_scan_frag_pool.getPtrForce(timeOutPtr[7]);

    tfragTimer[0] = timeOutPtr[0].p->scanFragTimer;
    tfragTimer[1] = timeOutPtr[1].p->scanFragTimer;
    tfragTimer[2] = timeOutPtr[2].p->scanFragTimer;
    tfragTimer[3] = timeOutPtr[3].p->scanFragTimer;
    tfragTimer[4] = timeOutPtr[4].p->scanFragTimer;
    tfragTimer[5] = timeOutPtr[5].p->scanFragTimer;
    tfragTimer[6] = timeOutPtr[6].p->scanFragTimer;
    tfragTimer[7] = timeOutPtr[7].p->scanFragTimer;

    texpiredTime[0] = TtcTimer - tfragTimer[0];
    texpiredTime[1] = TtcTimer - tfragTimer[1];
    texpiredTime[2] = TtcTimer - tfragTimer[2];
    texpiredTime[3] = TtcTimer - tfragTimer[3];
    texpiredTime[4] = TtcTimer - tfragTimer[4];
    texpiredTime[5] = TtcTimer - tfragTimer[5];
    texpiredTime[6] = TtcTimer - tfragTimer[6];
    texpiredTime[7] = TtcTimer - tfragTimer[7];
    
    for (Uint32 Ti = 0; Ti < 8; Ti++) {
      jam();
      if (tfragTimer[Ti] != 0) {

        if (texpiredTime[Ti] > ctimeOutValue) {
	  jam();
	  DEBUG("Fragment timeout found:"<<
		" ctimeOutValue=" <<ctimeOutValue
		<<", texpiredTime="<<texpiredTime[Ti]<<endl
		<<"      tfragTimer="<<tfragTimer[Ti]
		<<", ctcTimer="<<ctcTimer);
          timeOutFoundFragLab(signal, TscanConPtr + Ti);
          return;
        }//if
      }//if
    }//for
    TscanConPtr += 8;
    /*----------------------------------------------------------------*/
    /* We split the process up checking 1024 fragmentrecords at a time*/
    /* to maintain real time behaviour.                               */
    /*----------------------------------------------------------------*/
    if (TloopCount++ > 128 ) {
      jam();
      signal->theData[0] = TcContinueB::ZCONTINUE_TIME_OUT_FRAG_CONTROL;
      signal->theData[1] = TscanConPtr;
      sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
      return;
    }//if
  }//while
  for ( ; TscanConPtr < cscanFragrecFileSize; TscanConPtr++){
    jam();
    timeOutPtr[0].i = TscanConPtr;
    c_scan_frag_pool.getPtrForce(timeOutPtr[0]);
    if (timeOutPtr[0].p->scanFragTimer != 0) {
      texpiredTime[0] = ctcTimer - timeOutPtr[0].p->scanFragTimer;
      if (texpiredTime[0] > ctimeOutValue) {
        jam();
	DEBUG("Fragment timeout found:"<<
	      " ctimeOutValue=" <<ctimeOutValue
	      <<", texpiredTime="<<texpiredTime[0]<<endl
		<<"      tfragTimer="<<tfragTimer[0]
		<<", ctcTimer="<<ctcTimer);
        timeOutFoundFragLab(signal, TscanConPtr);
        return;
      }//if
    }//if
  }//for  
  ctimeOutCheckFragActive = TOCS_FALSE;

  return;
}//timeOutLoopStartFragLab()

/*--------------------------------------------------------------------------*/
/*Handle the heartbeat signal from LQH in a scan process                    */
// (Set timer on fragrec.)
/*--------------------------------------------------------------------------*/
void Dbtc::execSCAN_HBREP(Signal* signal)
{
  jamEntry();

  scanFragptr.i = signal->theData[0];  
  c_scan_frag_pool.getPtr(scanFragptr);
  switch (scanFragptr.p->scanFragState){
  case ScanFragRec::LQH_ACTIVE:
    break;
  default:
    DEBUG("execSCAN_HBREP: scanFragState="<<scanFragptr.p->scanFragState);
    systemErrorLab(signal, __LINE__);
    break;
  }

  ScanRecordPtr scanptr;
  scanptr.i = scanFragptr.p->scanRec;
  ptrCheckGuard(scanptr, cscanrecFileSize, scanRecord);

  apiConnectptr.i = scanptr.p->scanApiRec;
  ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);

  if (!(apiConnectptr.p->transid[0] == signal->theData[1] &&
	apiConnectptr.p->transid[1] == signal->theData[2])){
    jam();
    /**
     * Send signal back to sender so that the crash occurs there
     */
    // Save original transid
    signal->theData[3] = signal->theData[0];
    signal->theData[4] = signal->theData[1];
    // Set transid to illegal values
    signal->theData[1] = RNIL;
    signal->theData[2] = RNIL;
    
    sendSignal(signal->senderBlockRef(), GSN_SCAN_HBREP, signal, 5, JBA);
    DEBUG("SCAN_HBREP with wrong transid("
	  <<signal->theData[3]<<", "<<signal->theData[4]<<")");
    return;
  }//if

  // Update timer on ScanFragRec
  if (scanFragptr.p->scanFragTimer != 0){
    updateBuddyTimer(apiConnectptr);
    scanFragptr.p->startFragTimer(ctcTimer);
  } else {
    ndbassert(false);
    DEBUG("SCAN_HBREP when scanFragTimer was turned off");
  }
}//execSCAN_HBREP()

/*--------------------------------------------------------------------------*/
/*      Timeout has occured on a fragment which means a scan has timed out. */
/*      If this is true we have an error in LQH/ACC.                        */
/*--------------------------------------------------------------------------*/
void Dbtc::timeOutFoundFragLab(Signal* signal, UintR TscanConPtr)
{
  ScanFragRecPtr ptr;
  c_scan_frag_pool.getPtr(ptr, TscanConPtr);
#ifdef VM_TRACE
  {
    ScanRecordPtr scanptr;
    scanptr.i = ptr.p->scanRec;
    ptrCheckGuard(scanptr, cscanrecFileSize, scanRecord);
    ApiConnectRecordPtr TlocalApiConnectptr;
    TlocalApiConnectptr.i = scanptr.p->scanApiRec;
    ptrCheckGuard(TlocalApiConnectptr, capiConnectFilesize, apiConnectRecord);

    DEBUG("[ H'" << hex << TlocalApiConnectptr.p->transid[0]
	<< " H'" << TlocalApiConnectptr.p->transid[1] << "] "
        << TscanConPtr << " timeOutFoundFragLab: scanFragState = "
        << ptr.p->scanFragState);
  }
#endif

  const Uint32 time_out_param= ctimeOutValue;
  const Uint32 old_time_out_param= c_abortRec.oldTimeOutValue;
  
  if (unlikely(time_out_param != old_time_out_param && 
	       getNodeState().getSingleUserMode()))
  {
    jam();
    ScanRecordPtr scanptr;
    scanptr.i = ptr.p->scanRec;
    ptrCheckGuard(scanptr, cscanrecFileSize, scanRecord);
    ApiConnectRecordPtr TlocalApiConnectptr;
    TlocalApiConnectptr.i = scanptr.p->scanApiRec;
    ptrCheckGuard(TlocalApiConnectptr, capiConnectFilesize, apiConnectRecord);
    
    if (refToNode(TlocalApiConnectptr.p->ndbapiBlockref) == 
	getNodeState().getSingleUserApi())
    {
      jam();
      Uint32 val = ctcTimer - ptr.p->scanFragTimer;
      if (val <= old_time_out_param)
      {
	jam();
	goto next;
      }
    }
  }

  /*-------------------------------------------------------------------------*/
  // The scan fragment has expired its timeout. Check its state to decide
  // what to do.
  /*-------------------------------------------------------------------------*/
  switch (ptr.p->scanFragState) {
  case ScanFragRec::WAIT_GET_PRIMCONF:
    jam();
    /**
     * Time-out waiting for local DIH, we will continue waiting forever.
     * This should only occur when we run in an unstable environment where
     * the main thread goes slow.
     * We reset timer to avoid getting here again immediately.
     */
    ptr.p->startFragTimer(ctcTimer);
#ifdef VM_TRACE
    g_eventLogger->warning("Time-out in WAIT_GET_PRIMCONF: scanRecord = %u",
                           ptr.i);
#endif
    break;
  case ScanFragRec::LQH_ACTIVE:{
    jam();

    /**  
     * The LQH expired it's timeout, try to close it
     */    
    NodeId nodeId = refToNode(ptr.p->lqhBlockref);
    Uint32 connectCount = getNodeInfo(nodeId).m_connectCount;
    ScanRecordPtr scanptr;
    scanptr.i = ptr.p->scanRec;
    ptrCheckGuard(scanptr, cscanrecFileSize, scanRecord);

    if(connectCount != ptr.p->m_connectCount){
      jam();
      /**
       * The node has died
       */
      ptr.p->scanFragState = ScanFragRec::COMPLETED;
      ptr.p->stopFragTimer();
      {
        Local_ScanFragRec_dllist run(c_scan_frag_pool, scanptr.p->m_running_scan_frags);
        run.release(ptr);
      }
    }
    
    scanError(signal, scanptr, ZSCAN_FRAG_LQH_ERROR);
    break;
  }
  case ScanFragRec::DELIVERED:
    jam();
    // Fall through
  case ScanFragRec::IDLE:
    jam();
    // Fall through
  case ScanFragRec::QUEUED_FOR_DELIVERY:
    jam();
    /*-----------------------------------------------------------------------
     * Should never occur. We will simply report set the timer to zero and
     * continue. In a debug version we should crash here but not in a release
     * version. In a release version we will simply set the time-out to zero.
     *-----------------------------------------------------------------------*/
#ifdef VM_TRACE
    systemErrorLab(signal, __LINE__);
#endif
    scanFragptr.p->stopFragTimer();
    break;
  default:
    jam();
    /*-----------------------------------------------------------------------
     * Non-existent state. Crash.
     *-----------------------------------------------------------------------*/
    systemErrorLab(signal, __LINE__);
    break;
  }//switch
  
next:  
  signal->theData[0] = TcContinueB::ZCONTINUE_TIME_OUT_FRAG_CONTROL;
  signal->theData[1] = TscanConPtr + 1;
  sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
  return;  
}//timeOutFoundFragLab()


/*
  4.3.16 GCP_NOMORETRANS  
  ----------------------
*/
/*****************************************************************************
 *                         G C P _ N O M O R E T R A N S                    
 *                                                                           
 *  WHEN DBTC RECEIVES SIGNAL GCP_NOMORETRANS A CHECK IS DONE TO FIND OUT IF  
 *  THERE ARE ANY GLOBAL CHECKPOINTS GOING ON - CFIRSTGCP /= RNIL. DBTC THEN 
 *  SEARCHES THE GCP_RECORD FILE TO FIND OUT IF THERE ARE ANY TRANSACTIONS NOT
 *  CONCLUDED WITH THIS SPECIFIC CHECKPOINT - GCP_PTR:GCP_ID = TCHECK_GCP_ID.
 *  FOR EACH TRANSACTION WHERE API_CONNECTSTATE EQUALS PREPARED, COMMITTING,
 *  COMMITTED OR COMPLETING SIGNAL CONTINUEB IS SENT WITH A DELAY OF 100 MS, 
 *  THE COUNTER GCP_PTR:OUTSTANDINGAPI IS INCREASED. WHEN CONTINUEB IS RECEIVED
 *  THE COUNTER IS DECREASED AND A CHECK IS DONE TO FIND OUT IF ALL 
 *  TRANSACTIONS ARE CONCLUDED. IF SO, SIGNAL GCP_TCFINISHED IS SENT.       
 *****************************************************************************/
void Dbtc::execGCP_NOMORETRANS(Signal* signal) 
{
  jamEntry();
  GCPNoMoreTrans* req = (GCPNoMoreTrans*)signal->getDataPtr();
  c_gcp_ref = req->senderRef;
  c_gcp_data = req->senderData;
  Uint32 gci_lo = req->gci_lo;
  Uint32 gci_hi = req->gci_hi;
  tcheckGcpId = gci_lo | (Uint64(gci_hi) << 32);
  const bool nfhandling = (c_ongoing_take_over_cnt > 0);

  Ptr<GcpRecord> gcpPtr;
  if (cfirstgcp != RNIL) {
    jam();
                                      /* A GLOBAL CHECKPOINT IS GOING ON */
    gcpPtr.i = cfirstgcp;             /* SET POINTER TO FIRST GCP IN QUEUE*/
    ptrCheckGuard(gcpPtr, cgcpFilesize, gcpRecord);
    if (gcpPtr.p->gcpId == tcheckGcpId)
    {
      jam();
      bool empty = gcpPtr.p->firstApiConnect == RNIL;

      if (nfhandling)
      {
        jam();
        g_eventLogger->info("DBTC %u: GCP completion %u/%u waiting "
                            "for node failure handling (%u) to complete "
                            "(empty=%u)",
                            instance(),
                            gci_hi,
                            gci_lo,
                            c_ongoing_take_over_cnt,
                            empty);

        gcpPtr.p->gcpNomoretransRec = ZTRUE;
      }
      else
      {
        if (!empty)
        {
          jam();
          /* Wait for transactions in GCP to commit + complete */
          gcpPtr.p->gcpNomoretransRec = ZTRUE;
        }
        else
        {
          jam();
          /* All transactions completed, no nfhandling, complete it now */
          gcpTcfinished(signal, tcheckGcpId);
          unlinkGcp(gcpPtr);
        }
      }
    }
    else if (!nfhandling)
    {
      jam();
      /*------------------------------------------------------------*/
      /*       IF IT IS NOT THE FIRST THEN THERE SHOULD BE NO       */
      /*       RECORD FOR THIS GLOBAL CHECKPOINT. WE ALWAYS REMOVE  */
      /*       THE GLOBAL CHECKPOINTS IN ORDER.                     */
      /*------------------------------------------------------------*/
      gcpTcfinished(signal, tcheckGcpId);
    }
    else
    {
      jam();
      goto outoforder;
    }
  }
  else if (!nfhandling)
  {
    jam();
    gcpTcfinished(signal, tcheckGcpId);
  }
  else
  {
    g_eventLogger->info("DBTC %u: GCP completion %u/%u waiting "
                        "for node failure handling (%u) to complete. "
                        "Seizing record for GCP.",
                        instance(),
                        gci_hi,
                        gci_lo,
                        c_ongoing_take_over_cnt);
seize:
    jam();
    seizeGcp(gcpPtr, tcheckGcpId);
    gcpPtr.p->gcpNomoretransRec = ZTRUE;
  }
  return;
  
outoforder:
  g_eventLogger->info("DBTC %u: GCP completion %u/%u waiting "
                      "for node failure handling (%u) to complete. "
                      "GCP received out of order.  First GCP %u/%u",
                      instance(),
                      gci_hi, gci_lo,
                      c_ongoing_take_over_cnt,
                      Uint32(gcpPtr.p->gcpId >> 32),
                      Uint32(gcpPtr.p->gcpId));

  if (tcheckGcpId < gcpPtr.p->gcpId)
  {
    jam();

    Ptr<GcpRecord> tmp;
    tmp.i = cfirstfreeGcp;
    ptrCheckGuard(tmp, cgcpFilesize, gcpRecord);
    cfirstfreeGcp = tmp.p->nextGcp;

    tmp.p->gcpId = tcheckGcpId;
    tmp.p->nextGcp = cfirstgcp;
    tmp.p->firstApiConnect = RNIL;
    tmp.p->lastApiConnect = RNIL;
    tmp.p->gcpNomoretransRec = ZTRUE;
    cfirstgcp = tmp.i;
    g_eventLogger->info("DBTC %u: Out of order GCP %u/%u linked first",
                        instance(),
                        gci_hi,
                        gci_lo);
    return;
  }
  else
  {
    Ptr<GcpRecord> prev = gcpPtr;
    while (tcheckGcpId > gcpPtr.p->gcpId)
    {
      jam();
      if (gcpPtr.p->nextGcp == RNIL)
      {
        g_eventLogger->info("DBTC %u: Out of order GCP %u/%u linked last",
                            instance(),
                            gci_hi,
                            gci_lo);
        goto seize;
      }

      prev = gcpPtr;
      gcpPtr.i = gcpPtr.p->nextGcp;
      ptrCheckGuard(gcpPtr, cgcpFilesize, gcpRecord);
    }

    if (tcheckGcpId == gcpPtr.p->gcpId)
    {
      jam();
      gcpPtr.p->gcpNomoretransRec = ZTRUE;
      g_eventLogger->info("DBTC %u: Out of order GCP %u/%u already linked",
                          instance(),
                          gci_hi,
                          gci_lo);
      return;
    }
    ndbrequire(prev.i != gcpPtr.i); // checked earlier with initial "<"
    ndbrequire(prev.p->gcpId < tcheckGcpId);
    ndbrequire(gcpPtr.p->gcpId > tcheckGcpId);

    Ptr<GcpRecord> tmp;
    tmp.i = cfirstfreeGcp;
    ptrCheckGuard(tmp, cgcpFilesize, gcpRecord);
    cfirstfreeGcp = tmp.p->nextGcp;

    tmp.p->gcpId = tcheckGcpId;
    tmp.p->nextGcp = gcpPtr.i;
    tmp.p->firstApiConnect = RNIL;
    tmp.p->lastApiConnect = RNIL;
    tmp.p->gcpNomoretransRec = ZTRUE;
    prev.p->nextGcp = tmp.i;
    g_eventLogger->info("DBTC %u: Out of order Gcp %u/%u linked mid-list. "
                        "%u/%u < %u/%u < %u/%u",
                        instance(),
                        gci_hi, gci_lo,
                        Uint32(prev.p->gcpId >> 32),
                        Uint32(prev.p->gcpId),
                        gci_hi, gci_lo,
                        Uint32(gcpPtr.p->gcpId >> 32),
                        Uint32(gcpPtr.p->gcpId));
    return;
  }
}//Dbtc::execGCP_NOMORETRANS()

/*****************************************************************************/
/*                                                                           */
/*                            TAKE OVER MODULE                               */
/*                                                                           */
/*****************************************************************************/
/*                                                                           */
/*    THIS PART OF TC TAKES OVER THE COMMIT/ABORT OF TRANSACTIONS WHERE THE  */
/*    NODE ACTING AS TC HAVE FAILED. IT STARTS BY QUERYING ALL NODES ABOUT   */
/*    ANY OPERATIONS PARTICIPATING IN A TRANSACTION WHERE THE TC NODE HAVE   */
/*    FAILED.                                                                */
/*                                                                           */
/*    AFTER RECEIVING INFORMATION FROM ALL NODES ABOUT OPERATION STATUS THIS */
/*    CODE WILL ENSURE THAT ALL AFFECTED TRANSACTIONS ARE PROPERLY ABORTED OR*/
/*    COMMITTED. THE ORIGINATING APPLICATION NODE WILL ALSO BE CONTACTED.    */
/*    IF THE ORIGINATING APPLICATION ALSO FAILED THEN THERE IS CURRENTLY NO  */
/*    WAY TO FIND OUT WHETHER A TRANSACTION WAS PERFORMED OR NOT.            */
/*****************************************************************************/
void Dbtc::execNODE_FAILREP(Signal* signal) 
{
  jamEntry();

  NodeFailRep * const nodeFail = (NodeFailRep *)&signal->theData[0];

  cfailure_nr = nodeFail->failNo;
  const Uint32 tnoOfNodes  = nodeFail->noOfNodes;
  const Uint32 tnewMasterId = nodeFail->masterNodeId;
  Uint32 cdata[MAX_NDB_NODES];

  arrGuard(tnoOfNodes, MAX_NDB_NODES);
  Uint32 i;
  int index = 0;
  for (i = 1; i< MAX_NDB_NODES; i++) 
  {
    if(NdbNodeBitmask::get(nodeFail->theNodes, i))
    {
      cdata[index] = i;
      index++;
    }//if
  }//for

  cmasterNodeId = tnewMasterId;

  if (ERROR_INSERTED(8098))
    SET_ERROR_INSERT_VALUE(8099);  /* Disable 8098 on node failure */
  
  HostRecordPtr myHostPtr;

  tcNodeFailptr.i = 0;
  ptrAss(tcNodeFailptr, tcFailRecord);
  for (i = 0; i < tnoOfNodes; i++) 
  {
    jam();
    myHostPtr.i = cdata[i];
    ptrCheckGuard(myHostPtr, chostFilesize, hostRecord);
    
    /*------------------------------------------------------------*/
    /*       SET STATUS OF THE FAILED NODE TO DEAD SINCE IT HAS   */
    /*       FAILED.                                              */
    /*------------------------------------------------------------*/
    myHostPtr.p->hostStatus = HS_DEAD;
    myHostPtr.p->m_nf_bits = HostRecord::NF_NODE_FAIL_BITS;
    c_ongoing_take_over_cnt++;
    c_alive_nodes.clear(myHostPtr.i);

    if (tcNodeFailptr.p->failStatus == FS_LISTENING) 
    {
      jam();
      /*------------------------------------------------------------*/
      /*       THE CURRENT TAKE OVER CAN BE AFFECTED BY THIS NODE   */
      /*       FAILURE.                                             */
      /*------------------------------------------------------------*/
      if (myHostPtr.p->lqhTransStatus == LTS_ACTIVE) 
      {
	jam();
	/*------------------------------------------------------------*/
	/*       WE WERE WAITING FOR THE FAILED NODE IN THE TAKE OVER */
	/*       PROTOCOL FOR TC.                                     */
	/*------------------------------------------------------------*/
	signal->theData[0] = TcContinueB::ZNODE_TAKE_OVER_COMPLETED;
	signal->theData[1] = myHostPtr.i;
	sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
      }//if
    }//if
    
    jam();
    g_eventLogger->info("DBTC %u: Started failure handling for node %u",
                        instance(),
                        myHostPtr.i);
    
    signal->theData[0] = myHostPtr.i;
    sendSignal(cownref, GSN_TAKE_OVERTCREQ, signal, 1, JBB);
    
    checkScanActiveInFailedLqh(signal, 0, myHostPtr.i);
    nodeFailCheckTransactions(signal, 0, myHostPtr.i);
    Callback cb = {safe_cast(&Dbtc::ndbdFailBlockCleanupCallback), 
                  myHostPtr.i};
    simBlockNodeFailure(signal, myHostPtr.i, cb);
  }

  if (m_deferred_enabled == 0)
  {
    jam();
    Uint32 ok = 1;
    for(Uint32 n = c_alive_nodes.find_first();
        n != c_alive_nodes.NotFound;
        n = c_alive_nodes.find_next(n+1))
    {
      if (!ndbd_deferred_unique_constraints(getNodeInfo(n).m_version))
      {
        jam();
        ok = 0;
        break;
      }
    }
    if (ok)
    {
      jam();
      m_deferred_enabled = ~Uint32(0);
    }
  }
}//Dbtc::execNODE_FAILREP()

static
const char* getNFBitName(const Uint32 bit)
{
  switch (bit)
  {
  case Dbtc::HostRecord::NF_TAKEOVER: return "NF_TAKEOVER";
  case Dbtc::HostRecord::NF_CHECK_SCAN: return "NF_CHECK_SCAN";
  case Dbtc::HostRecord::NF_CHECK_TRANSACTION: return "NF_CHECK_TRANSACTION";
  case Dbtc::HostRecord::NF_BLOCK_HANDLE: return "NF_BLOCK_HANDLE";
  default:
    return "Unknown";
  }
};

static
void getNFBitNames(char* buff, Uint32 buffLen, const Uint32 bits)
{
  Uint32 index = 1;
  while (index < Dbtc::HostRecord::NF_NODE_FAIL_BITS)
  {
    if (bits & index)
    {
      Uint32 chars = BaseString::snprintf(buff,
                                          buffLen,
                                          "%s",
                                          getNFBitName(index));
      buff+= chars;
      buffLen-= chars;
      if (bits > (index << 1))
      {
        chars = BaseString::snprintf(buff,
                                     buffLen,
                                     ", ");
        buff+= chars;
        buffLen-= chars;
      }
    }

    index = index << 1;
  }
}
                       

void
Dbtc::checkNodeFailComplete(Signal* signal, 
			    Uint32 failedNodeId,
			    Uint32 bit)
{
  hostptr.i = failedNodeId;
  ptrCheckGuard(hostptr, chostFilesize, hostRecord);
  hostptr.p->m_nf_bits &= ~bit;

  if (hostptr.p->m_nf_bits == 0)
  {
    g_eventLogger->info("DBTC %u: Step %s completed, failure handling for "
                        "node %u complete.",
                        instance(),
                        getNFBitName(bit),
                        failedNodeId);

    NFCompleteRep * const nfRep = (NFCompleteRep *)&signal->theData[0];
    nfRep->blockNo      = DBTC;
    nfRep->nodeId       = cownNodeid;
    nfRep->failedNodeId = hostptr.i;

    if (instance() == 0)
    {
      jam();
      sendSignal(cdihblockref, GSN_NF_COMPLETEREP, signal,
                 NFCompleteRep::SignalLength, JBB);
      sendSignal(QMGR_REF, GSN_NF_COMPLETEREP, signal,
                 NFCompleteRep::SignalLength, JBB);
    }
    else
    {
      /**
       * Send to proxy
       */
      sendSignal(DBTC_REF, GSN_NF_COMPLETEREP, signal,
                 NFCompleteRep::SignalLength, JBB);
    }
  }
  else
  {
    char buffer[100];
    getNFBitNames(buffer,
                  sizeof(buffer),
                  hostptr.p->m_nf_bits);
    
    g_eventLogger->info("DBTC %u: Step %s completed, failure handling "
                        "for node %u waiting for %s.",
                        instance(),
                        getNFBitName(bit),
                        failedNodeId,
                        buffer);
  }

  CRASH_INSERTION(8058);
  if (ERROR_INSERTED(8059))
  {
    signal->theData[0] = 9999;
    sendSignalWithDelay(numberToRef(CMVMI, hostptr.i), 
                        GSN_NDB_TAMPER, signal, 100, 1);
  }
}

void Dbtc::checkScanActiveInFailedLqh(Signal* signal, 
				      Uint32 scanPtrI, 
				      Uint32 failedNodeId){

  ScanRecordPtr scanptr;
  for (scanptr.i = scanPtrI; scanptr.i < cscanrecFileSize; scanptr.i++) {
    jam();
    ptrAss(scanptr, scanRecord);
    bool found = false;
    if (scanptr.p->scanState != ScanRecord::IDLE){
      jam();
      ScanFragRecPtr ptr;
      Local_ScanFragRec_dllist run(c_scan_frag_pool, scanptr.p->m_running_scan_frags);
      
      for(run.first(ptr); !ptr.isNull(); ){
	jam();
	ScanFragRecPtr curr = ptr;
	run.next(ptr);
	if (curr.p->scanFragState == ScanFragRec::LQH_ACTIVE && 
	    refToNode(curr.p->lqhBlockref) == failedNodeId){
	  jam();
	  
	  curr.p->scanFragState = ScanFragRec::COMPLETED;
	  curr.p->stopFragTimer();
	  run.release(curr);
	  found = true;
	}
      }

      Local_ScanFragRec_dllist deliv(c_scan_frag_pool, scanptr.p->m_delivered_scan_frags);
      for(deliv.first(ptr); !ptr.isNull(); deliv.next(ptr))
      {
	jam();
	if (refToNode(ptr.p->lqhBlockref) == failedNodeId)
	{
	  jam();
	  found = true;
	  break;
	}
      }
    }
    if(found){
      jam();
      scanError(signal, scanptr, ZSCAN_LQH_ERROR);	  
    }

    // Send CONTINUEB to continue later
    signal->theData[0] = TcContinueB::ZCHECK_SCAN_ACTIVE_FAILED_LQH;
    signal->theData[1] = scanptr.i + 1; // Check next scanptr
    signal->theData[2] = failedNodeId;
    sendSignal(cownref, GSN_CONTINUEB, signal, 3, JBB);
    return;
  }//for

  checkNodeFailComplete(signal, failedNodeId, HostRecord::NF_CHECK_SCAN);
}

void
Dbtc::nodeFailCheckTransactions(Signal* signal, 
				Uint32 transPtrI, 
				Uint32 failedNodeId)
{
  jam();
  Ptr<ApiConnectRecord> transPtr;
  Uint32 TtcTimer = ctcTimer;
  Uint32 TapplTimeout = c_appl_timeout_value;
  Uint32 RT_BREAK = 64;
  Uint32 endPtrI = transPtrI + RT_BREAK;
  if (endPtrI > capiConnectFilesize)
  {
    endPtrI = capiConnectFilesize;
  }

  for (transPtr.i = transPtrI; transPtr.i < endPtrI; transPtr.i++)
  {
    ptrCheckGuard(transPtr, capiConnectFilesize, apiConnectRecord); 
    if (transPtr.p->m_transaction_nodes.get(failedNodeId))
    {
      jam();

      // Force timeout regardless of state      
      c_appl_timeout_value = 1;
      setApiConTimer(transPtr.i, TtcTimer - 2, __LINE__);
      timeOutFoundLab(signal, transPtr.i, ZNODEFAIL_BEFORE_COMMIT);
      c_appl_timeout_value = TapplTimeout;
      
      transPtr.i++;
      break;
    }
  }
  
  if (transPtr.i == capiConnectFilesize)
  {
    jam();
    checkNodeFailComplete(signal, failedNodeId, 
                          HostRecord::NF_CHECK_TRANSACTION);
  }
  else
  {
    signal->theData[0] = TcContinueB::ZNF_CHECK_TRANSACTIONS;
    signal->theData[1] = transPtr.i;
    signal->theData[2] = failedNodeId;
    sendSignal(cownref, GSN_CONTINUEB, signal, 3, JBB);
  }
}

void
Dbtc::ndbdFailBlockCleanupCallback(Signal* signal,
                                   Uint32 failedNodeId,
                                   Uint32 ignoredRc)
{
  jamEntry();
  
  checkNodeFailComplete(signal, failedNodeId,
                        HostRecord::NF_BLOCK_HANDLE);
}

void
Dbtc::apiFailBlockCleanupCallback(Signal* signal,
                                  Uint32 failedNodeId,
                                  Uint32 ignoredRc)
{
  jamEntry();
  
  signal->theData[0] = failedNodeId;
  signal->theData[1] = reference();
  sendSignal(capiFailRef, GSN_API_FAILCONF, signal, 2, JBB);
}

void
Dbtc::checkScanFragList(Signal* signal,
			Uint32 failedNodeId,
			ScanRecord * scanP, 
                        Local_ScanFragRec_dllist::Head & head){
  
  DEBUG("checkScanActiveInFailedLqh: scanFragError");
}

void Dbtc::execTAKE_OVERTCCONF(Signal* signal)
{
  jamEntry();

  if (!checkNodeFailSequence(signal))
  {
    jam();
    return;
  }

  Uint32 failedNodeId = signal->theData[0];
  hostptr.i = failedNodeId;
  ptrCheckGuard(hostptr, chostFilesize, hostRecord);

  Uint32 senderRef = signal->theData[1];
  if (signal->getLength() < 2)
  {
    jam();
    senderRef = 0; // currently only used to see if it's from self
  }

  if (senderRef != reference())
  {
    jam();

    tcNodeFailptr.i = 0;
    ptrAss(tcNodeFailptr, tcFailRecord);

    /**
     * Node should be in queue
     */
    Uint32 i = 0;
    Uint32 end = tcNodeFailptr.p->queueIndex;
    for (; i<end; i++)
    {
      jam();
      if (tcNodeFailptr.p->queueList[i] == hostptr.i)
      {
        g_eventLogger->info("DBTC %u: Removed node %u"
          " from takeover queue, %u failed nodes remaining",
                            instance(),
                            hostptr.i,
                            end - 1);

        jam();
        break;
      }
    }
    ndbrequire(i != end);
    tcNodeFailptr.p->queueList[i] = tcNodeFailptr.p->queueList[end-1];
    tcNodeFailptr.p->queueIndex = end - 1;
  }

  Uint32 cnt = c_ongoing_take_over_cnt;
  ndbrequire(cnt);
  c_ongoing_take_over_cnt = cnt - 1;
  checkNodeFailComplete(signal, hostptr.i, HostRecord::NF_TAKEOVER);

  if (cnt == 1 && cfirstgcp != RNIL)
  {
    /**
     * Check if there are any hanging GCP_NOMORETRANS
     */
    GcpRecordPtr tmpGcpPointer;
    tmpGcpPointer.i = cfirstgcp;
    ptrCheckGuard(tmpGcpPointer, cgcpFilesize, gcpRecord);
    if (tmpGcpPointer.p->gcpNomoretransRec)
    {
      if (tmpGcpPointer.p->firstApiConnect == RNIL)
      {
        jam();
        g_eventLogger->info("DBTC %u: Completing GCP %u/%u "
                            "on node failure takeover completion.",
                            instance(),
                            Uint32(tmpGcpPointer.p->gcpId >> 32),
                            Uint32(tmpGcpPointer.p->gcpId));
        gcpTcfinished(signal, tmpGcpPointer.p->gcpId);
        unlinkGcp(tmpGcpPointer);
      }
      else
      {
        jam();
        /**
         * GCP completion underway, but not all transactions
         * completed.  Completion will be normal.
         * Log now for symmetry in NF handling logging
         */
        g_eventLogger->info("DBTC %u: GCP completion %u/%u waiting "
                            "for all included transactions to complete.",
                            instance(),
                            Uint32(tmpGcpPointer.p->gcpId >> 32),
                            Uint32(tmpGcpPointer.p->gcpId));
      }
    }
  }
}//Dbtc::execTAKE_OVERTCCONF()

void Dbtc::execTAKE_OVERTCREQ(Signal* signal) 
{
  jamEntry();
  Uint32 failedNodeId = signal->theData[0];
  tcNodeFailptr.i = 0;
  ptrAss(tcNodeFailptr, tcFailRecord);
  if (tcNodeFailptr.p->failStatus != FS_IDLE ||
      cmasterNodeId != getOwnNodeId() ||
      (! (instance() == 0 /* single TC */ ||
          instance() == TAKE_OVER_INSTANCE))) /* in mt-TC case let 1 instance
                                                 do take-over */
  {
    jam();
    /*------------------------------------------------------------*/
    /*       WE CAN CURRENTLY ONLY HANDLE ONE TAKE OVER AT A TIME */
    /*------------------------------------------------------------*/
    /*       IF MORE THAN ONE TAKE OVER IS REQUESTED WE WILL      */
    /*       QUEUE THE TAKE OVER AND START IT AS SOON AS THE      */
    /*       PREVIOUS ARE COMPLETED.                              */
    /*------------------------------------------------------------*/
    g_eventLogger->info("DBTC %u: Inserting failed node %u into"
                        " takeover queue, length %u",
                        instance(),
                        failedNodeId,
                        tcNodeFailptr.p->queueIndex + 1);
    arrGuard(tcNodeFailptr.p->queueIndex, MAX_NDB_NODES);
    tcNodeFailptr.p->queueList[tcNodeFailptr.p->queueIndex] = failedNodeId;
    tcNodeFailptr.p->queueIndex++;
    return;
  }//if
  ndbrequire(instance() == 0 || instance() == TAKE_OVER_INSTANCE);
  g_eventLogger->info("DBTC %u: Starting take over of node %u",
                      instance(),
                      failedNodeId);
  startTakeOverLab(signal, 0, failedNodeId);
}//Dbtc::execTAKE_OVERTCREQ()

/**
  TC Takeover protocol
  --------------------
  When a node fails it has a set of transactions ongoing in many cases. In
  order to complete those transactions we have implemented a take over
  protocol.

  The failed node was transaction controller for a set of transactions.
  The transaction controller has failed, so we cannot ask it for the
  state of the transactions. This means that we need to rebuild the
  transaction state of all transactions that was controlled by the failed
  node. If there are several node failures at the same time, then we'll
  take over one node at a time. The TC take over is only handled by the
  first TC instance, most of the work of the take over is handled by the
  LQH instances that need to scan all ongoing operations to send the
  operations to the TC instance that takes over. For the ndbd it is handled
  by the TC instance simply.

  With the introduction of multiple TC instances we have also seen it fit
  to handle also the take over of one TC instance in multiple steps. We use
  one step per TC instance we take over. If the TC take over fails due to
  not having sufficient amount of transaction records or operations records,
  we will also run several steps for each TC instance we take over.

  The TC take over process will eventually complete as long as there isn't
  any transaction that is bigger than the number of operation records
  available for TC take over.

  The protocol works as follows:

  Master TC (take over TC instance, starting at instance 0):
  Sends:
  LQH_TRANSREQ (Take over data reference,
                Master TC block reference,
                TC Take over node id,
                TC Take over instance id)

  This signal is sent to the LQH proxy block in each of the alive nodes.
  From the proxy block it is sent to each LQH instance. For ndbd's we
  send it directly to the LQH block.

  Each LQH instance scans the operations for any operations that has a
  TC reference that comes from the TC node and TC instance that has failed.
  For each such operation it sends a LQH_TRANSCONF signal that contains a
  lot of data about the transaction operation.

  When the LQH instance have completed the scan of its operations then it
  will send a special LQH_TRANSCONF with a last operation flag set. All
  LQH_TRANSCONF is sent through the LQH proxy such that we can check when
  all LQH instances have completed their scans. We will only send one
  LQH_TRANSCONF signal from each node.

  The Master TC that performs the take over builds the state of each
  transaction such that it can decide whether the transaction was
  committed or not, if it wasn't committed we will abort it.

  The design is certain that each step will at least handle one transaction
  for each take over step. Normally it will complete all transactions in
  one instance in each step, we can though handle also other uncommon cases.
*/

/*------------------------------------------------------------*/
/*       INITIALISE THE HASH TABLES FOR STORING TRANSACTIONS  */
/*       AND OPERATIONS DURING TC TAKE OVER.                  */
/*------------------------------------------------------------*/
void Dbtc::startTakeOverLab(Signal* signal,
                            Uint32 instanceId,
                            Uint32 failedNodeId)
{
  for (Uint32 i = 0; i < TRANSID_FAIL_HASH_SIZE; i++) {
    ctransidFailHash[i] = RNIL;
  }//for
  for (Uint32 i = 0; i < TC_FAIL_HASH_SIZE; i++) {
    ctcConnectFailHash[i] = RNIL;
  }//for
  tcNodeFailptr.p->failStatus = FS_LISTENING;
  tcNodeFailptr.p->takeOverNode = failedNodeId;
  tcNodeFailptr.p->takeOverInstanceId = instanceId;
  tcNodeFailptr.p->takeOverFailed = false;
  tcNodeFailptr.p->maxInstanceId = 0;
  tcNodeFailptr.p->handledOneTransaction = false;
  for (hostptr.i = 1; hostptr.i < MAX_NDB_NODES; hostptr.i++) {
    jam();
    ptrAss(hostptr, hostRecord);
    if (hostptr.p->hostStatus == HS_ALIVE)
    {
      LqhTransReq * const lqhTransReq  = (LqhTransReq *)&signal->theData[0];
      Uint32 sig_len;
      jam();
      tblockref = calcLqhBlockRef(hostptr.i);
      hostptr.p->lqhTransStatus = LTS_ACTIVE;
      lqhTransReq->senderData = tcNodeFailptr.i;
      lqhTransReq->senderRef = cownref;
      lqhTransReq->failedNodeId = failedNodeId;
      lqhTransReq->instanceId = instanceId;
      if (ndbd_multi_tc_instance_takeover(getNodeInfo(hostptr.i).m_version))
      {
        /* The node does support the new LQH_TRANSREQ protocol */
        sig_len = LqhTransReq::SignalLength;
      }
      else
      {
        sig_len = LqhTransReq::OldSignalLength;
      }
      if (ERROR_INSERTED(8064) && hostptr.i == getOwnNodeId())
      {
        ndbout_c("sending delayed GSN_LQH_TRANSREQ to self");
        sendSignalWithDelay(tblockref, GSN_LQH_TRANSREQ, signal, 100, sig_len);
        CLEAR_ERROR_INSERT_VALUE;
      }
      else
      {
        sendSignal(tblockref, GSN_LQH_TRANSREQ, signal, sig_len, JBB);
      }
    }//if
  }//for
}//Dbtc::startTakeOverLab()

Uint32
Dbtc::get_transid_fail_bucket(Uint32 transid1)
{
  return transid1 & (TRANSID_FAIL_HASH_SIZE - 1);
}

void
Dbtc::insert_transid_fail_hash(Uint32 transid1)
{
  Uint32 bucket = get_transid_fail_bucket(transid1);
  apiConnectptr.p->nextApiConnect = ctransidFailHash[bucket];
  ctransidFailHash[bucket] = apiConnectptr.i;
  return;
}

bool
Dbtc::findApiConnectFail(Signal *signal, Uint32 transid1, Uint32 transid2)
{
  Uint32 bucket = get_transid_fail_bucket(transid1);
  apiConnectptr.i = ctransidFailHash[bucket];

  while (apiConnectptr.i != RNIL)
  {
    jam();
    ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
    if (apiConnectptr.p->transid[0] == transid1 &&
        apiConnectptr.p->transid[1] == transid2)
    {
      jam();
      return true;
    }
    apiConnectptr.i = apiConnectptr.p->nextApiConnect;
  }
  jam();
  return false;
}

void Dbtc::releaseMarker(ApiConnectRecord * const regApiPtr)
{
  Uint32 marker = regApiPtr->commitAckMarker;
  if (marker != RNIL)
  {
    regApiPtr->commitAckMarker = RNIL;
    CommitAckMarkerBuffer::DataBufferPool & pool =
      c_theCommitAckMarkerBufferPool;
    CommitAckMarker * tmp = m_commitAckMarkerHash.getPtr(marker);
    LocalCommitAckMarkerBuffer commitAckMarkers(pool, tmp->theDataBuffer);
    commitAckMarkers.release();
    m_commitAckMarkerHash.release(marker);
  }
}

void Dbtc::remove_from_transid_fail_hash(Signal *signal, Uint32 transid1)
{
  ApiConnectRecordPtr locApiConnectptr;
  ApiConnectRecordPtr prevApiConptr;

  prevApiConptr.i = RNIL;
  Uint32 bucket = get_transid_fail_bucket(transid1);
  locApiConnectptr.i = ctransidFailHash[bucket];
  ndbrequire(locApiConnectptr.i != RNIL);
  do
  {
    ptrCheckGuard(locApiConnectptr, capiConnectFilesize, apiConnectRecord);
    if (locApiConnectptr.i == apiConnectptr.i)
    {
      if (prevApiConptr.i == RNIL)
      {
        jam();
        /* We were first in the hash linked list */
        ndbassert(ctransidFailHash[bucket] == apiConnectptr.i);
        ctransidFailHash[bucket] = apiConnectptr.p->nextApiConnect;
      }
      else
      {
        /* Link past our record in the hash list */
        jam();
        ptrCheckGuard(prevApiConptr, capiConnectFilesize, apiConnectRecord);
        prevApiConptr.p->nextApiConnect = apiConnectptr.p->nextApiConnect;
      }
      apiConnectptr.p->nextApiConnect = RNIL;
      return;
    }
    else
    {
      jam();
      prevApiConptr.i = locApiConnectptr.i;
      locApiConnectptr.i = locApiConnectptr.p->nextApiConnect;
    }
  } while (locApiConnectptr.i != RNIL);
  ndbabort();
}

Uint32
Dbtc::get_tc_fail_bucket(Uint32 transid1, Uint32 opRec)
{
  return (transid1 ^ opRec) & (TC_FAIL_HASH_SIZE - 1);
}

void
Dbtc::insert_tc_fail_hash(Uint32 transid1, Uint32 tcOprec)
{
  Uint32 bucket = get_tc_fail_bucket(transid1, tcOprec);
  tcConnectptr.p->nextTcFailHash = ctcConnectFailHash[bucket];
  ctcConnectFailHash[bucket] = tcConnectptr.i;
}

bool
Dbtc::findTcConnectFail(Signal* signal,
                        Uint32 transid1,
                        Uint32 transid2,
                        Uint32 tcOprec) 
{
  Uint32 bucket = get_tc_fail_bucket(transid1, tcOprec);

  tcConnectptr.i = ctcConnectFailHash[bucket];
  while (tcConnectptr.i != RNIL)
  {
    ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
    if (tcConnectptr.p->tcOprec == tcOprec &&
        tcConnectptr.p->apiConnect == apiConnectptr.i &&
        apiConnectptr.p->transid[0] == transid1 &&
        apiConnectptr.p->transid[1] == transid2)
    {
      jam();
      return true;
    }
    else
    {
      jam();
      tcConnectptr.i = tcConnectptr.p->nextTcFailHash;
    }
  }
  jam();
  return false;
}

void Dbtc::remove_transaction_from_tc_fail_hash(Signal *signal)
{
  TcConnectRecordPtr remTcConnectptr;

  remTcConnectptr.i = apiConnectptr.p->firstTcConnect;
  while (remTcConnectptr.i != RNIL)
  {
    jam();
    TcConnectRecordPtr loopTcConnectptr;
    TcConnectRecordPtr prevTcConnectptr;
    ptrCheckGuard(remTcConnectptr, ctcConnectFilesize, tcConnectRecord);
    bool found = false;
    Uint32 bucket = get_tc_fail_bucket(apiConnectptr.p->transid[0],
                                       remTcConnectptr.p->tcOprec);
    prevTcConnectptr.i = RNIL;
    loopTcConnectptr.i = ctcConnectFailHash[bucket];
    while (loopTcConnectptr.i != RNIL)
    {
      if (loopTcConnectptr.i == remTcConnectptr.i)
      {
        found = true;
        if (prevTcConnectptr.i == RNIL)
        {
          jam();
          /* We were first in the hash linked list */
          ndbassert(ctcConnectFailHash[bucket] == remTcConnectptr.i);
          ctcConnectFailHash[bucket] = remTcConnectptr.p->nextTcFailHash;
        }
        else
        {
          /* Link past our record in the hash list */
          jam();
          ptrCheckGuard(prevTcConnectptr, ctcConnectFilesize, tcConnectRecord);
          prevTcConnectptr.p->nextTcFailHash =
            remTcConnectptr.p->nextTcFailHash;
        }
        remTcConnectptr.p->nextTcFailHash = RNIL;
        break;
      }
      else
      {
        jam();
        ptrCheckGuard(loopTcConnectptr, ctcConnectFilesize, tcConnectRecord);
        prevTcConnectptr.i = loopTcConnectptr.i;
        loopTcConnectptr.i = loopTcConnectptr.p->nextTcFailHash;
      }
    }
    ndbrequire(found);
    remTcConnectptr.i = remTcConnectptr.p->nextTcConnect;
  }
}

/*------------------------------------------------------------*/
/*       A REPORT OF AN OPERATION WHERE TC FAILED HAS ARRIVED.*/
/*------------------------------------------------------------*/
void Dbtc::execLQH_TRANSCONF(Signal* signal) 
{
  jamEntry();
  LqhTransConf * const lqhTransConf = (LqhTransConf *)&signal->theData[0];

  if (ERROR_INSERTED(8102) ||
      ERROR_INSERTED(8118))
  {
    jam();
    if ((((LqhTransConf::OperationStatus)lqhTransConf->operationStatus) ==
         LqhTransConf::LastTransConf) &&
        ((signal->getSendersBlockRef() != reference()) ||
         ERROR_INSERTED(8118)))
    {
      jam();
      ndbout_c("Delaying final LQH_TRANSCONF from Lqh @ node %u",
               refToNode(signal->getSendersBlockRef()));
      // Force multi-tc takeover
      ((LqhTransConf*)lqhTransConf)->maxInstanceId = 4;
      sendSignalWithDelay(reference(),
                          GSN_LQH_TRANSCONF,
                          signal,
                          5000,
                          signal->getLength());
      return;
    }
  }

  CRASH_INSERTION(8060);

  tcNodeFailptr.i = lqhTransConf->tcRef;
  ptrCheckGuard(tcNodeFailptr, 1, tcFailRecord);
  NodeId nodeId = lqhTransConf->lqhNodeId;
  LqhTransConf::OperationStatus transStatus =
    (LqhTransConf::OperationStatus)lqhTransConf->operationStatus;
  Uint32 transid1    = lqhTransConf->transId1;
  Uint32 transid2    = lqhTransConf->transId2;
  Uint32 tcOprec     = lqhTransConf->oldTcOpRec;
  Uint32 reqinfo     = lqhTransConf->requestInfo;
  Uint64 gci         = Uint64(lqhTransConf->gci_hi) << 32;
  cnodes[0]    = lqhTransConf->nextNodeId1;
  cnodes[1]    = lqhTransConf->nextNodeId2;
  cnodes[2]    = lqhTransConf->nextNodeId3;
  const BlockReference ref = lqhTransConf->apiRef;
  BlockReference applRef = lqhTransConf->apiRef;
  Uint32 applOprec   = lqhTransConf->apiOpRec;
  const Uint32 tableId = lqhTransConf->tableId;
  Uint32 gci_lo = lqhTransConf->gci_lo;
  Uint32 fragId = lqhTransConf->fragId;

  if (transStatus == LqhTransConf::Committed && 
      unlikely(signal->getLength() < LqhTransConf::SignalLength_GCI_LO))
  {
    jam();
    gci_lo = 0;
    ndbassert(!ndb_check_micro_gcp(getNodeInfo(nodeId).m_version));
  }
  gci |= gci_lo;

  if (transStatus == LqhTransConf::LastTransConf){
    jam();
    Uint32 maxInstanceId = lqhTransConf->maxInstanceId;
    if (unlikely(signal->getLength() < LqhTransConf::SignalLength_INST_ID))
    {
      maxInstanceId = 0;
    }
    ndbassert(maxInstanceId < NDBMT_MAX_BLOCK_INSTANCES);
    /* A node has reported one phase of take over handling as completed. */
    nodeTakeOverCompletedLab(signal, nodeId, maxInstanceId);
    return;
  }//if
  if (transStatus == LqhTransConf::Marker){
    jam();
    reqinfo = 0;
    LqhTransConf::setMarkerFlag(reqinfo, 1);
  } else {
    TableRecordPtr tabPtr;
    tabPtr.i = tableId;
    ptrCheckGuard(tabPtr, ctabrecFilesize, tableRecord);
    switch((DictTabInfo::TableType)tabPtr.p->tableType){
    case DictTabInfo::SystemTable:
    case DictTabInfo::UserTable:
      break;
    default:
      applRef = 0;
      applOprec = 0;
    }
  }

  if (findApiConnectFail(signal, transid1, transid2))
  {
    /**
     * Found a transaction record, record the added info about this
     * transaction as received in this message
     */
    updateApiStateFail(signal,
                       transid1,
                       transid2,
                       transStatus,
                       reqinfo,
                       applRef,
                       gci,
                       nodeId);
  }
  else
  {
    if (cfirstfreeApiConnectFail == RNIL)
    {
      jam();
      /**
       * We failed to allocate a transaction record, we will record that
       * we failed to handle all transactions and we will not allocate
       * any more transaction records as part of taking over this instance
       * of the failed TC. The failed transactions will be taken over in
       * a second step of the TC take over of this instance and possibly
       * even more than two steps will be performed.
       *
       * This could happen if the master node has less transaction records
       * than the failed node which is possible but not recommended.
       */
      Uint32 apiConnect = capiConnectFilesize / 3;
      g_eventLogger->info("Need to do another round of TC takeover handling."
                          " The failed node had a higher setting of "
                          "MaxNoOfConcurrentTransactions than this node has, "
                          "this node has MaxNoOfConcurrentTransactions = %u",
                          apiConnect);
      tcNodeFailptr.p->takeOverFailed = true;
      return;
    }
    else if (tcNodeFailptr.p->takeOverFailed)
    {
      jam();
      /**
       * After we failed to allocate a transaction record we won't
       * try to allocate any new ones since it might mean that we
       * miss operations of a transaction.
       */
      return;
    }
    /**
     * A new transaction was found, allocate a record for it and fill
     * it with info as received in LQH_TRANSCONF message. Also insert
     * it in hash table for transaction records in TC take over.
     */
    seizeApiConnectFail(signal);
    insert_transid_fail_hash(transid1);
    initApiConnectFail(signal,
                       transid1,
                       transid2,
                       transStatus,
                       reqinfo,
                       applRef,
                       gci,
                       nodeId);
  }

  if(apiConnectptr.p->ndbapiBlockref == 0 && applRef != 0)
  {
    jam();
    apiConnectptr.p->ndbapiBlockref = ref;
    apiConnectptr.p->ndbapiConnect = applOprec;
  }
  
  if (transStatus != LqhTransConf::Marker)
  {
    Uint32 instanceKey;

    if (unlikely(signal->getLength() < LqhTransConf::SignalLength_FRAG_ID))
    {
      jam();
      instanceKey = 0;
    }
    else
    {
      jam();
      instanceKey = getInstanceKey(tableId, fragId);
    }
    if (findTcConnectFail(signal, transid1, transid2, tcOprec))
    {
      jam();
      updateTcStateFail(signal,
                        instanceKey,
                        tcOprec,
                        reqinfo,
                        transStatus,
                        nodeId);
    }
    else
    {
      if (cfirstfreeTcConnectFail == RNIL)
      {
        /**
         * We failed to allocate a TC record, given that we can no longer
         * complete take over of this transaction we must handle the failed
         * transaction take over. We also set takeOverFailed to true to
         * ensure that we cannot start any new transaction take over after
         * this one. This is imperative to do since otherwise a new operation
         * on the failed transaction can come along and find free resources.
         * This would not be a correct behavior so we need to protect against
         * it by not starting any transactions after any seize failure.
         * 
         * As long as we don't have transactions that are bigger than the
         * available number of TC records we should make progress at each
         * step and the algorithm should eventually complete.
         *
         * This could happen if the master node have fewer operation records
         * than the failed node had. This could happen but isn't
         * recommended.
         */
        Uint32 tcConnect = ctcConnectFilesize;
        jam();
        g_eventLogger->info("Need to do another round of TC takeover handling."
                            " The failed node had a higher setting of "
                            "MaxNoOfConcurrentOperations than this node has, "
                            "this node has MaxNoOfConcurrentOperations = %u",
                            tcConnect);
        tcNodeFailptr.p->takeOverFailed = true;
        remove_transaction_from_tc_fail_hash(signal);
        releaseMarker(apiConnectptr.p);
        remove_from_transid_fail_hash(signal, transid1);
        releaseTakeOver(signal);
      }
      else
      {
        jam();
        seizeTcConnectFail(signal);
        linkTcInConnectionlist(signal);
        insert_tc_fail_hash(transid1, tcOprec);
        initTcConnectFail(signal,
                          instanceKey,
                          tcOprec,
                          reqinfo,
                          transStatus,
                          nodeId);
      }
    }
  }
}//Dbtc::execLQH_TRANSCONF()

/*------------------------------------------------------------*/
/*       A NODE HAS REPORTED COMPLETION OF TAKE OVER REPORTING*/
/*------------------------------------------------------------*/
void Dbtc::nodeTakeOverCompletedLab(Signal* signal,
                                    NodeId nodeId,
                                    Uint32 maxInstanceId)
{
  Uint32 guard0;

  CRASH_INSERTION(8061);

  if (unlikely(maxInstanceId >= NDBMT_MAX_BLOCK_INSTANCES))
  {
    /**
     * bug# 19193927 : LQH sends junk max instance id
     * Bug is fixed, but handle upgrade
     */
    maxInstanceId = NDBMT_MAX_BLOCK_INSTANCES - 1;
  }
  if (tcNodeFailptr.p->maxInstanceId < maxInstanceId)
  {
    jam();
    tcNodeFailptr.p->maxInstanceId = maxInstanceId;
  }
  hostptr.i = nodeId;
  ptrCheckGuard(hostptr, chostFilesize, hostRecord);
  hostptr.p->lqhTransStatus = LTS_IDLE;
  for (hostptr.i = 1; hostptr.i < MAX_NDB_NODES; hostptr.i++) {
    jam();
    ptrAss(hostptr, hostRecord);
    if (hostptr.p->hostStatus == HS_ALIVE) {
      if (hostptr.p->lqhTransStatus == LTS_ACTIVE) {
        jam();
        /*------------------------------------------------------------*/
        /*       NOT ALL NODES ARE COMPLETED WITH REPORTING IN THE    */
        /*       TAKE OVER.                                           */
        /*------------------------------------------------------------*/
        return;
      }//if
    }//if
  }//for
  /*------------------------------------------------------------*/
  /*       ALL NODES HAVE REPORTED ON THE STATUS OF THE VARIOUS */
  /*       OPERATIONS THAT WAS CONTROLLED BY THE FAILED TC. WE  */
  /*       ARE NOW IN A POSITION TO COMPLETE ALL OF THOSE       */
  /*       TRANSACTIONS EITHER IN A SUCCESSFUL WAY OR IN AN     */
  /*       UNSUCCESSFUL WAY. WE WILL ALSO REPORT THIS CONCLUSION*/
  /*       TO THE APPLICATION IF THAT IS STILL ALIVE.           */
  /*------------------------------------------------------------*/
  tcNodeFailptr.p->currentHashIndexTakeOver = 0;
  tcNodeFailptr.p->completedTakeOver = 0;
  tcNodeFailptr.p->failStatus = FS_COMPLETING;
  guard0 = cnoParallelTakeOver - 1;
  /*------------------------------------------------------------*/
  /*       WE WILL COMPLETE THE TRANSACTIONS BY STARTING A      */
  /*       NUMBER OF PARALLEL ACTIVITIES. EACH ACTIVITY WILL    */
  /*       COMPLETE ONE TRANSACTION AT A TIME AND IN THAT       */
  /*       TRANSACTION IT WILL COMPLETE ONE OPERATION AT A TIME.*/
  /*       WHEN ALL ACTIVITIES ARE COMPLETED THEN THE TAKE OVER */
  /*       IS COMPLETED.                                        */
  /*------------------------------------------------------------*/
  arrGuard(guard0, MAX_NDB_NODES);
  for (tindex = 0; tindex <= guard0; tindex++) {
    jam();
    tcNodeFailptr.p->takeOverProcState[tindex] = ZTAKE_OVER_ACTIVE;
    signal->theData[0] = TcContinueB::ZCOMPLETE_TRANS_AT_TAKE_OVER;
    signal->theData[1] = tcNodeFailptr.i;
    signal->theData[2] = tindex;
    sendSignal(cownref, GSN_CONTINUEB, signal, 3, JBB);
  }//for
}//Dbtc::nodeTakeOverCompletedLab()

/*------------------------------------------------------------*/
/*       COMPLETE A NEW TRANSACTION FROM THE HASH TABLE OF    */
/*       TRANSACTIONS TO COMPLETE.                            */
/*------------------------------------------------------------*/
void Dbtc::completeTransAtTakeOverLab(Signal* signal, UintR TtakeOverInd) 
{
  jam();
  while (tcNodeFailptr.p->currentHashIndexTakeOver < TRANSID_FAIL_HASH_SIZE)
  {
    jam();
    apiConnectptr.i = 
        ctransidFailHash[tcNodeFailptr.p->currentHashIndexTakeOver];
    if (apiConnectptr.i != RNIL) {
      jam();
      /*------------------------------------------------------------*/
      /*       WE HAVE FOUND A TRANSACTION THAT NEEDS TO BE         */
      /*       COMPLETED. REMOVE IT FROM THE HASH TABLE SUCH THAT   */
      /*       NOT ANOTHER ACTIVITY ALSO TRIES TO COMPLETE THIS     */
      /*       TRANSACTION.                                         */
      /*------------------------------------------------------------*/
      ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
      ctransidFailHash[tcNodeFailptr.p->currentHashIndexTakeOver] = 
        apiConnectptr.p->nextApiConnect;
      tcNodeFailptr.p->handledOneTransaction = true;
      completeTransAtTakeOverDoOne(signal, TtakeOverInd);
      // One transaction taken care of, return from this function
      // and wait for the next CONTINUEB to continue processing
      break;

    } else {
      if (tcNodeFailptr.p->currentHashIndexTakeOver <
          (TRANSID_FAIL_HASH_SIZE -1)){
        jam();      
        tcNodeFailptr.p->currentHashIndexTakeOver++;
      } else {
        jam();
        completeTransAtTakeOverDoLast(signal, TtakeOverInd); 
        tcNodeFailptr.p->currentHashIndexTakeOver++;
      }//if
    }//if
  }//while
}//Dbtc::completeTransAtTakeOverLab()


void Dbtc::completeTransAtTakeOverDoLast(Signal* signal, UintR TtakeOverInd) 
{
  Uint32 guard0;
  /*------------------------------------------------------------*/
  /*       THERE ARE NO MORE TRANSACTIONS TO COMPLETE. THIS     */
  /*       ACTIVITY IS COMPLETED.                               */
  /*------------------------------------------------------------*/
  arrGuard(TtakeOverInd, MAX_NDB_NODES);
  if (tcNodeFailptr.p->takeOverProcState[TtakeOverInd] != ZTAKE_OVER_ACTIVE) {
    jam();
    systemErrorLab(signal, __LINE__);
    return;
  }//if
  /**
   * If we need to continue with take over processing it will still be with
   * the same node.
   */
  if (tcNodeFailptr.p->takeOverFailed == ZTRUE)
  {
    /**
     * We performed a take over and it didn't complete, we will try
     * again. The reason it failed was due to this TC instance
     * not having sufficient resources to handle all transactions,
     * we should however make progress and thus eventually we should
     * be able to complete it. The only reason that it could completely
     * fail is if there is one transaction which is so big that we
     * cannot calculate its outcome.
     *
     * Given that we haven't completed this round, we continue with
     * one more attempt on the same TC instance.
     */
    jam();
    g_eventLogger->info("DBTC %u: Continuing take over of "
                        "DBTC instance %u in node %u",
                        instance(),
                        tcNodeFailptr.p->takeOverInstanceId,
                        tcNodeFailptr.p->takeOverNode);
    ndbrequire(tcNodeFailptr.p->handledOneTransaction);
    startTakeOverLab(signal,
                     tcNodeFailptr.p->takeOverInstanceId,
                     tcNodeFailptr.p->takeOverNode);
    return;
  }
  if (tcNodeFailptr.p->takeOverInstanceId < tcNodeFailptr.p->maxInstanceId)
  {
    /**
     * We deal with one TC instance at a time, we have completed one
     * instance and there are more instances to handle. Let's continue
     * with the next instance.
     */
    jam();
    g_eventLogger->info("DBTC %u: Completed take over of "
                        "DBTC instance %u in failed node %u,"
                        " continuing with the next instance",
                        instance(),
                        tcNodeFailptr.p->takeOverInstanceId,
                        tcNodeFailptr.p->takeOverNode);
    startTakeOverLab(signal,
                     tcNodeFailptr.p->takeOverInstanceId + 1,
                     tcNodeFailptr.p->takeOverNode);
    return;
  }
  /**
   * We have completed take over of this node for all its TC instances.
   * We will continue taking over other nodes if there are any more
   * nodes to take over.
   */
  tcNodeFailptr.p->takeOverProcState[TtakeOverInd] = ZTAKE_OVER_IDLE;
  tcNodeFailptr.p->completedTakeOver++;

  g_eventLogger->info("DBTC %u: Completed take over"
                      " of failed node %u",
                      instance(),
                      tcNodeFailptr.p->takeOverNode);
  CRASH_INSERTION(8062);

  if (tcNodeFailptr.p->completedTakeOver == cnoParallelTakeOver) {
    jam();
    /*------------------------------------------------------------*/
    /*       WE WERE THE LAST ACTIVITY THAT WAS COMPLETED. WE NEED*/
    /*       TO REPORT THE COMPLETION OF THE TAKE OVER TO ALL     */
    /*       NODES THAT ARE ALIVE.                                */
    /*------------------------------------------------------------*/
    NodeReceiverGroup rg(DBTC, c_alive_nodes);
    signal->theData[0] = tcNodeFailptr.p->takeOverNode;
    signal->theData[1] = reference();
    sendSignal(rg, GSN_TAKE_OVERTCCONF, signal, 2, JBB);
    
    if (tcNodeFailptr.p->queueIndex > 0) {
      jam();
      /*------------------------------------------------------------*/
      /*       THERE ARE MORE NODES TO TAKE OVER. WE NEED TO START  */
      /*       THE TAKE OVER.                                       */
      /*------------------------------------------------------------*/
      Uint32 failedNodeId = tcNodeFailptr.p->queueList[0];
      guard0 = tcNodeFailptr.p->queueIndex - 1;
      arrGuard(guard0 + 1, MAX_NDB_NODES);
      for (Uint32 i = 0; i <= guard0; i++) {
        jam();
        tcNodeFailptr.p->queueList[i] = tcNodeFailptr.p->queueList[i+1];
      }//for
      tcNodeFailptr.p->queueIndex--;
      g_eventLogger->info("DBTC %u: Starting next DBTC node"
                          " take over for failed node %u,"
                          " %u failed nodes remaining in takeover queue",
                          instance(),
                          failedNodeId,
                          tcNodeFailptr.p->queueIndex);
      startTakeOverLab(signal, 0, failedNodeId);
      return;
    } else {
      jam();
      tcNodeFailptr.p->failStatus = FS_IDLE;
    }//if
  }//if
  return;
}//Dbtc::completeTransAtTakeOverDoLast()

void Dbtc::completeTransAtTakeOverDoOne(Signal* signal, UintR TtakeOverInd)
{
  apiConnectptr.p->takeOverRec = (Uint8)tcNodeFailptr.i;
  apiConnectptr.p->takeOverInd = TtakeOverInd;

  switch (apiConnectptr.p->apiConnectstate) {
  case CS_FAIL_COMMITTED:
    jam();
    /*------------------------------------------------------------*/
    /*       ALL PARTS OF THE TRANSACTIONS REPORTED COMMITTED. WE */
    /*       HAVE THUS COMPLETED THE COMMIT PHASE. WE CAN REPORT  */
    /*       COMMITTED TO THE APPLICATION AND CONTINUE WITH THE   */
    /*       COMPLETE PHASE.                                      */
    /*------------------------------------------------------------*/
    sendTCKEY_FAILCONF(signal, apiConnectptr.p);
    tcConnectptr.i = apiConnectptr.p->firstTcConnect;
    ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
    apiConnectptr.p->currentTcConnect = tcConnectptr.i;
    apiConnectptr.p->currentReplicaNo = tcConnectptr.p->lastReplicaNo;
    tcurrentReplicaNo = tcConnectptr.p->lastReplicaNo;
    commitGciHandling(signal, apiConnectptr.p->globalcheckpointid);
    toCompleteHandlingLab(signal);
    return;
  case CS_FAIL_COMMITTING:
    jam();
    /*------------------------------------------------------------*/
    /*       AT LEAST ONE PART WAS ONLY PREPARED AND AT LEAST ONE */
    /*       PART WAS COMMITTED. COMPLETE THE COMMIT PHASE FIRST. */
    /*       THEN CONTINUE AS AFTER COMMITTED.                    */
    /*------------------------------------------------------------*/
    tcConnectptr.i = apiConnectptr.p->firstTcConnect;
    ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
    apiConnectptr.p->currentTcConnect = tcConnectptr.i;
    apiConnectptr.p->currentReplicaNo = tcConnectptr.p->lastReplicaNo;
    tcurrentReplicaNo = tcConnectptr.p->lastReplicaNo;
    commitGciHandling(signal, apiConnectptr.p->globalcheckpointid);
    toCommitHandlingLab(signal);
    return;
  case CS_FAIL_ABORTING:
  case CS_FAIL_PREPARED:
    jam();
    /*------------------------------------------------------------*/
    /*       WE WILL ABORT THE TRANSACTION IF IT IS IN A PREPARED */
    /*       STATE IN THIS VERSION. IN LATER VERSIONS WE WILL     */
    /*       HAVE TO ADD CODE FOR HANDLING OF PREPARED-TO-COMMIT  */
    /*       TRANSACTIONS. THESE ARE NOT ALLOWED TO ABORT UNTIL WE*/
    /*       HAVE HEARD FROM THE TRANSACTION COORDINATOR.         */
    /*                                                            */
    /*       IT IS POSSIBLE TO COMMIT TRANSACTIONS THAT ARE       */
    /*       PREPARED ACTUALLY. WE WILL LEAVE THIS PROBLEM UNTIL  */
    /*       LATER VERSIONS.                                      */
    /*------------------------------------------------------------*/
    tcConnectptr.i = apiConnectptr.p->firstTcConnect;
    ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
    apiConnectptr.p->currentTcConnect = tcConnectptr.i;
    apiConnectptr.p->currentReplicaNo = tcConnectptr.p->lastReplicaNo;
    tcurrentReplicaNo = tcConnectptr.p->lastReplicaNo;
    toAbortHandlingLab(signal);
    return;
  case CS_FAIL_ABORTED:
    jam();
    sendTCKEY_FAILREF(signal, apiConnectptr.p);
    
    signal->theData[0] = TcContinueB::ZCOMPLETE_TRANS_AT_TAKE_OVER;
    signal->theData[1] = (UintR)apiConnectptr.p->takeOverRec;
    signal->theData[2] = apiConnectptr.p->takeOverInd;
    sendSignal(cownref, GSN_CONTINUEB, signal, 3, JBB);
    releaseTakeOver(signal);
    break;
  case CS_FAIL_COMPLETED:
    jam();
    sendTCKEY_FAILCONF(signal, apiConnectptr.p);
    
    signal->theData[0] = TcContinueB::ZCOMPLETE_TRANS_AT_TAKE_OVER;
    signal->theData[1] = (UintR)apiConnectptr.p->takeOverRec;
    signal->theData[2] = apiConnectptr.p->takeOverInd;
    sendSignal(cownref, GSN_CONTINUEB, signal, 3, JBB);
    releaseApiConnectFail(signal);
    break;
  default:
    jam();
    systemErrorLab(signal, __LINE__);
    return;
  }//switch
}//Dbtc::completeTransAtTakeOverDoOne()

void 
Dbtc::sendTCKEY_FAILREF(Signal* signal, ApiConnectRecord * regApiPtr){
  jam();

  const Uint32 ref = regApiPtr->ndbapiBlockref;
  const NodeId nodeId = refToNode(ref);
  if(ref != 0)
  {
    jam();
    bool connectedToNode = getNodeInfo(nodeId).m_connected;
    signal->theData[0] = regApiPtr->ndbapiConnect;
    signal->theData[1] = regApiPtr->transid[0];
    signal->theData[2] = regApiPtr->transid[1];
    
    if (likely(connectedToNode))
    {
      jam();
      sendSignal(ref, GSN_TCKEY_FAILREF, signal, 3, JBB);
    }
    else
    {
      routeTCKEY_FAILREFCONF(signal, regApiPtr, GSN_TCKEY_FAILREF, 3);
    }
  }
  releaseMarker(regApiPtr);
}

void 
Dbtc::sendTCKEY_FAILCONF(Signal* signal, ApiConnectRecord * regApiPtr){
  jam();
  TcKeyFailConf * const failConf = (TcKeyFailConf *)&signal->theData[0];
  
  const Uint32 ref = regApiPtr->ndbapiBlockref;
  const Uint32 marker = regApiPtr->commitAckMarker;
  const NodeId nodeId = refToNode(ref);
  if(ref != 0)
  {
    jam();
    failConf->apiConnectPtr = regApiPtr->ndbapiConnect | (marker != RNIL);
    failConf->transId1 = regApiPtr->transid[0];
    failConf->transId2 = regApiPtr->transid[1];
    
    bool connectedToNode = getNodeInfo(nodeId).m_connected;
    if (likely(connectedToNode))
    {
      jam();
      sendSignal(ref, GSN_TCKEY_FAILCONF, signal, 
		 TcKeyFailConf::SignalLength, JBB);
    }
    else
    {
      routeTCKEY_FAILREFCONF(signal, regApiPtr,
			     GSN_TCKEY_FAILCONF, TcKeyFailConf::SignalLength);
    }
  }
  regApiPtr->commitAckMarker = RNIL;
}

void
Dbtc::routeTCKEY_FAILREFCONF(Signal* signal, const ApiConnectRecord* regApiPtr,
			     Uint32 gsn, Uint32 len)
{
  jam();

  Uint32 ref = regApiPtr->ndbapiBlockref;

  /**
   * We're not connected
   *   so we find another node in same node group as died node
   *   and send to it, so that it can forward
   */
  tcNodeFailptr.i = regApiPtr->takeOverRec;
  ptrCheckGuard(tcNodeFailptr, 1, tcFailRecord);

  /**
   * Save signal
   */
  Uint32 save[25];
  ndbrequire(len <= 25);
  memcpy(save, signal->theData, 4*len);
  
  Uint32 node = tcNodeFailptr.p->takeOverNode;
  
  CheckNodeGroups * sd = (CheckNodeGroups*)signal->getDataPtrSend();
  sd->blockRef = reference();
  sd->requestType =
    CheckNodeGroups::Direct |
    CheckNodeGroups::GetNodeGroupMembers;
  sd->nodeId = node;
  EXECUTE_DIRECT_MT(DBDIH, GSN_CHECKNODEGROUPSREQ, signal, 
		    CheckNodeGroups::SignalLength, 0);
  jamEntry();
  
  NdbNodeBitmask mask;
  mask.assign(sd->mask);
  mask.clear(getOwnNodeId());
  memcpy(signal->theData, save, 4*len);
  
  Uint32 i = 0;
  while((i = mask.find(i + 1)) != NdbNodeBitmask::NotFound)
  {
    jam();
    HostRecordPtr localHostptr;
    localHostptr.i = i;
    ptrCheckGuard(localHostptr, chostFilesize, hostRecord);
    if (localHostptr.p->hostStatus == HS_ALIVE) 
    {
      jam();
      signal->theData[len] = gsn;
      signal->theData[len+1] = ref;
      sendSignal(calcTcBlockRef(i), GSN_TCKEY_FAILREFCONF_R, 
		 signal, len+2, JBB);
      return;
    }
  }
  
 
   /**
    * This code was 'unfinished' code for partially connected API's
    *   it does however not really work...
    *   and we seriously need to think about semantics for API connect
    */
#if 0
  ndbrequire(getNodeInfo(refToNode(ref)).m_type == NodeInfo::DB);
#endif
}

void
Dbtc::execTCKEY_FAILREFCONF_R(Signal* signal)
{
  jamEntry();
  Uint32 len = signal->getLength();
  Uint32 gsn = signal->theData[len-2];
  Uint32 ref = signal->theData[len-1];
  sendSignal(ref, gsn, signal, len-2, JBB);
}

/*------------------------------------------------------------*/
/*       THIS PART HANDLES THE ABORT PHASE IN THE CASE OF A   */
/*       NODE FAILURE BEFORE THE COMMIT DECISION.             */
/*------------------------------------------------------------*/
/*       ABORT REQUEST SUCCESSFULLY COMPLETED ON TNODEID      */
/*------------------------------------------------------------*/
void Dbtc::execABORTCONF(Signal* signal) 
{
  UintR compare_transid1, compare_transid2;

  jamEntry();
  tcConnectptr.i = signal->theData[0];
  NodeId nodeId = signal->theData[2];
  if (ERROR_INSERTED(8045)) {
    CLEAR_ERROR_INSERT_VALUE;
    sendSignalWithDelay(cownref, GSN_ABORTCONF, signal, 2000, 5);
    return;
  }//if
  if (tcConnectptr.i >= ctcConnectFilesize) {
    errorReport(signal, 5);
    return;
  }//if
  ptrAss(tcConnectptr, tcConnectRecord);
  if (tcConnectptr.p->tcConnectstate != OS_WAIT_ABORT_CONF) {
    warningReport(signal, 16);
    return;
  }//if
  apiConnectptr.i = tcConnectptr.p->apiConnect;
  ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
  if (apiConnectptr.p->apiConnectstate != CS_WAIT_ABORT_CONF) {
    warningReport(signal, 17);
    return;
  }//if
  compare_transid1 = apiConnectptr.p->transid[0] ^ signal->theData[3];
  compare_transid2 = apiConnectptr.p->transid[1] ^ signal->theData[4];
  compare_transid1 = compare_transid1 | compare_transid2;
  if (compare_transid1 != 0) {
    warningReport(signal, 18);
    return;
  }//if
  arrGuard(apiConnectptr.p->currentReplicaNo, MAX_REPLICAS);
  if (tcConnectptr.p->tcNodedata[apiConnectptr.p->currentReplicaNo] !=
      nodeId) {
    warningReport(signal, 19);
    return;
  }//if
  tcurrentReplicaNo = (Uint8)Z8NIL;
  tcConnectptr.p->tcConnectstate = OS_ABORTING;
  toAbortHandlingLab(signal);
}//Dbtc::execABORTCONF()

void Dbtc::toAbortHandlingLab(Signal* signal) 
{
  do {
    if (tcurrentReplicaNo != (Uint8)Z8NIL) {
      jam();
      arrGuard(tcurrentReplicaNo, MAX_REPLICAS);
      const LqhTransConf::OperationStatus stat = 
	(LqhTransConf::OperationStatus)
	tcConnectptr.p->failData[tcurrentReplicaNo];
      switch(stat){
      case LqhTransConf::InvalidStatus:
      case LqhTransConf::Aborted:
        jam();
        /*empty*/;
        break;
      case LqhTransConf::Prepared:
        jam();
        hostptr.i = tcConnectptr.p->tcNodedata[tcurrentReplicaNo];
        ptrCheckGuard(hostptr, chostFilesize, hostRecord);
        if (hostptr.p->hostStatus == HS_ALIVE) {
          jam();
          Uint32 instanceKey = tcConnectptr.p->lqhInstanceKey;
          tblockref = numberToRef(DBLQH, instanceKey, hostptr.i);
          setApiConTimer(apiConnectptr.i, ctcTimer, __LINE__);
          tcConnectptr.p->tcConnectstate = OS_WAIT_ABORT_CONF;
          apiConnectptr.p->apiConnectstate = CS_WAIT_ABORT_CONF;
          apiConnectptr.p->timeOutCounter = 0;
          signal->theData[0] = tcConnectptr.i;
          signal->theData[1] = cownref;
          signal->theData[2] = apiConnectptr.p->transid[0];
          signal->theData[3] = apiConnectptr.p->transid[1];
          signal->theData[4] = apiConnectptr.p->tcBlockref;
          signal->theData[5] = tcConnectptr.p->tcOprec;
          sendSignal(tblockref, GSN_ABORTREQ, signal, 6, JBB);
          return;
        }//if
        break;
      default:
        jam();
        systemErrorLab(signal, __LINE__);
        return;
      }//switch
    }//if
    if (apiConnectptr.p->currentReplicaNo > 0) {
      jam();
      /*------------------------------------------------------------*/
      /*       THERE IS STILL ANOTHER REPLICA THAT NEEDS TO BE      */
      /*       ABORTED.                                             */
      /*------------------------------------------------------------*/
      apiConnectptr.p->currentReplicaNo--;
      tcurrentReplicaNo = apiConnectptr.p->currentReplicaNo;
    } else {
      /*------------------------------------------------------------*/
      /*       THE LAST REPLICA IN THIS OPERATION HAVE COMMITTED.   */
      /*------------------------------------------------------------*/
      tcConnectptr.i = tcConnectptr.p->nextTcConnect;
      if (tcConnectptr.i == RNIL) {
	/*------------------------------------------------------------*/
	/*       WE HAVE COMPLETED THE ABORT PHASE. WE CAN NOW REPORT */
	/*       THE ABORT STATUS TO THE APPLICATION AND CONTINUE     */
	/*       WITH THE NEXT TRANSACTION.                           */
	/*------------------------------------------------------------*/
        if (apiConnectptr.p->takeOverRec != (Uint8)Z8NIL) {
          jam();
	  sendTCKEY_FAILREF(signal, apiConnectptr.p);
          
	  /*------------------------------------------------------------*/
	  /*       WE HAVE COMPLETED THIS TRANSACTION NOW AND CAN       */
	  /*       CONTINUE THE PROCESS WITH THE NEXT TRANSACTION.      */
	  /*------------------------------------------------------------*/
          signal->theData[0] = TcContinueB::ZCOMPLETE_TRANS_AT_TAKE_OVER;
          signal->theData[1] = (UintR)apiConnectptr.p->takeOverRec;
          signal->theData[2] = apiConnectptr.p->takeOverInd;
          sendSignal(cownref, GSN_CONTINUEB, signal, 3, JBB);
          releaseTakeOver(signal);
        } else {
          jam();
          releaseAbortResources(signal);
        }//if
        return;
      }//if
      apiConnectptr.p->currentTcConnect = tcConnectptr.i;
      ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
      apiConnectptr.p->currentReplicaNo = tcConnectptr.p->lastReplicaNo;
      tcurrentReplicaNo = tcConnectptr.p->lastReplicaNo;
    }//if
  } while (1);
}//Dbtc::toAbortHandlingLab()

/*------------------------------------------------------------*/
/*       THIS PART HANDLES THE COMMIT PHASE IN THE CASE OF A  */
/*       NODE FAILURE IN THE MIDDLE OF THE COMMIT PHASE.      */
/*------------------------------------------------------------*/
/*       COMMIT REQUEST SUCCESSFULLY COMPLETED ON TNODEID     */
/*------------------------------------------------------------*/
void Dbtc::execCOMMITCONF(Signal* signal) 
{
  UintR compare_transid1, compare_transid2;

  jamEntry();
  tcConnectptr.i = signal->theData[0];
  NodeId nodeId = signal->theData[1];
  if (ERROR_INSERTED(8046)) {
    CLEAR_ERROR_INSERT_VALUE;
    sendSignalWithDelay(cownref, GSN_COMMITCONF, signal, 2000, 4);
    return;
  }//if
  if (tcConnectptr.i >= ctcConnectFilesize) {
    errorReport(signal, 4);
    return;
  }//if
  ptrAss(tcConnectptr, tcConnectRecord);
  if (tcConnectptr.p->tcConnectstate != OS_WAIT_COMMIT_CONF) {
    warningReport(signal, 8);
    return;
  }//if
  apiConnectptr.i = tcConnectptr.p->apiConnect;
  ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
  if (apiConnectptr.p->apiConnectstate != CS_WAIT_COMMIT_CONF) {
    warningReport(signal, 9);
    return;
  }//if
  compare_transid1 = apiConnectptr.p->transid[0] ^ signal->theData[2];
  compare_transid2 = apiConnectptr.p->transid[1] ^ signal->theData[3];
  compare_transid1 = compare_transid1 | compare_transid2;
  if (compare_transid1 != 0) {
    warningReport(signal, 10);
    return;
  }//if
  arrGuard(apiConnectptr.p->currentReplicaNo, MAX_REPLICAS);
  if (tcConnectptr.p->tcNodedata[apiConnectptr.p->currentReplicaNo] !=
      nodeId) {
    warningReport(signal, 11);
    return;
  }//if
  if (ERROR_INSERTED(8026)) {
    jam();
    systemErrorLab(signal, __LINE__);
  }//if
  tcurrentReplicaNo = (Uint8)Z8NIL;
  tcConnectptr.p->tcConnectstate = OS_COMMITTED;
  toCommitHandlingLab(signal);
}//Dbtc::execCOMMITCONF()

void Dbtc::toCommitHandlingLab(Signal* signal) 
{
  do {
    if (tcurrentReplicaNo != (Uint8)Z8NIL) {
      jam();
      arrGuard(tcurrentReplicaNo, MAX_REPLICAS);
      switch (tcConnectptr.p->failData[tcurrentReplicaNo]) {
      case LqhTransConf::InvalidStatus:
        jam();
        /*empty*/;
        break;
      case LqhTransConf::Committed:
        jam();
        /*empty*/;
        break;
      case LqhTransConf::Prepared:
        jam();
        /*------------------------------------------------------------*/
        /*       THE NODE WAS PREPARED AND IS WAITING FOR ABORT OR    */
        /*       COMMIT REQUEST FROM TC.                              */
        /*------------------------------------------------------------*/
        hostptr.i = tcConnectptr.p->tcNodedata[tcurrentReplicaNo];
        ptrCheckGuard(hostptr, chostFilesize, hostRecord);
        if (hostptr.p->hostStatus == HS_ALIVE) {
          jam();
          Uint32 instanceKey = tcConnectptr.p->lqhInstanceKey;
          tblockref = numberToRef(DBLQH, instanceKey, hostptr.i);
          setApiConTimer(apiConnectptr.i, ctcTimer, __LINE__);
          apiConnectptr.p->apiConnectstate = CS_WAIT_COMMIT_CONF;
          apiConnectptr.p->timeOutCounter = 0;
          tcConnectptr.p->tcConnectstate = OS_WAIT_COMMIT_CONF;
          Uint64 gci = apiConnectptr.p->globalcheckpointid;
          signal->theData[0] = tcConnectptr.i;
          signal->theData[1] = cownref;
          signal->theData[2] = Uint32(gci >> 32); // XXX JON
          signal->theData[3] = apiConnectptr.p->transid[0];
          signal->theData[4] = apiConnectptr.p->transid[1];
          signal->theData[5] = apiConnectptr.p->tcBlockref;
          signal->theData[6] = tcConnectptr.p->tcOprec;
          signal->theData[7] = Uint32(gci);
          sendSignal(tblockref, GSN_COMMITREQ, signal, 8, JBB);
          return;
        }//if
        break;
      default:
        jam();
        systemErrorLab(signal, __LINE__);
        return;
        break;
      }//switch
    }//if
    if (apiConnectptr.p->currentReplicaNo > 0) {
      jam();
      /*------------------------------------------------------------*/
      /*       THERE IS STILL ANOTHER REPLICA THAT NEEDS TO BE      */
      /*       COMMITTED.                                           */
      /*------------------------------------------------------------*/
      apiConnectptr.p->currentReplicaNo--;
      tcurrentReplicaNo = apiConnectptr.p->currentReplicaNo;
    } else {
      /*------------------------------------------------------------*/
      /*       THE LAST REPLICA IN THIS OPERATION HAVE COMMITTED.   */
      /*------------------------------------------------------------*/
      tcConnectptr.i = tcConnectptr.p->nextTcConnect;
      if (tcConnectptr.i == RNIL) {
	/*------------------------------------------------------------*/
	/*       WE HAVE COMPLETED THE COMMIT PHASE. WE CAN NOW REPORT*/
	/*       THE COMMIT STATUS TO THE APPLICATION AND CONTINUE    */
	/*       WITH THE COMPLETE PHASE.                             */
	/*------------------------------------------------------------*/
        if (apiConnectptr.p->takeOverRec != (Uint8)Z8NIL) {
          jam();
	  sendTCKEY_FAILCONF(signal, apiConnectptr.p);
	} else {
          jam();
          apiConnectptr = sendApiCommitAndCopy(signal);
        }//if
        apiConnectptr.p->currentTcConnect = apiConnectptr.p->firstTcConnect;
        tcConnectptr.i = apiConnectptr.p->firstTcConnect;
        ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
        tcurrentReplicaNo = tcConnectptr.p->lastReplicaNo;
        apiConnectptr.p->currentReplicaNo = tcurrentReplicaNo;
        toCompleteHandlingLab(signal);
        return;
      }//if
      apiConnectptr.p->currentTcConnect = tcConnectptr.i;
      ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
      apiConnectptr.p->currentReplicaNo = tcConnectptr.p->lastReplicaNo;
      tcurrentReplicaNo = tcConnectptr.p->lastReplicaNo;
    }//if
  } while (1);
}//Dbtc::toCommitHandlingLab()

/*------------------------------------------------------------*/
/*       COMMON PART TO HANDLE COMPLETE PHASE WHEN ANY NODE   */
/*       HAVE FAILED.                                         */
/*------------------------------------------------------------*/
/*       THE NODE WITH TNODEID HAVE COMPLETED THE OPERATION   */
/*------------------------------------------------------------*/
void Dbtc::execCOMPLETECONF(Signal* signal) 
{
  UintR compare_transid1, compare_transid2;

  jamEntry();
  tcConnectptr.i = signal->theData[0];
  NodeId nodeId = signal->theData[1];
  if (ERROR_INSERTED(8047)) {
    CLEAR_ERROR_INSERT_VALUE;
    sendSignalWithDelay(cownref, GSN_COMPLETECONF, signal, 2000, 4);
    return;
  }//if
  if (tcConnectptr.i >= ctcConnectFilesize) {
    errorReport(signal, 3);
    return;
  }//if
  ptrAss(tcConnectptr, tcConnectRecord);
  if (tcConnectptr.p->tcConnectstate != OS_WAIT_COMPLETE_CONF) {
    warningReport(signal, 12);
    return;
  }//if
  apiConnectptr.i = tcConnectptr.p->apiConnect;
  ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
  if (apiConnectptr.p->apiConnectstate != CS_WAIT_COMPLETE_CONF) {
    warningReport(signal, 13);
    return;
  }//if
  compare_transid1 = apiConnectptr.p->transid[0] ^ signal->theData[2];
  compare_transid2 = apiConnectptr.p->transid[1] ^ signal->theData[3];
  compare_transid1 = compare_transid1 | compare_transid2;
  if (compare_transid1 != 0) {
    warningReport(signal, 14);
    return;
  }//if
  arrGuard(apiConnectptr.p->currentReplicaNo, MAX_REPLICAS);
  if (tcConnectptr.p->tcNodedata[apiConnectptr.p->currentReplicaNo] !=
      nodeId) {
    warningReport(signal, 15);
    return;
  }//if
  if (ERROR_INSERTED(8028)) {
    jam();
    systemErrorLab(signal, __LINE__);
  }//if
  tcConnectptr.p->tcConnectstate = OS_COMPLETED;
  tcurrentReplicaNo = (Uint8)Z8NIL;
  toCompleteHandlingLab(signal);
}//Dbtc::execCOMPLETECONF()

void Dbtc::toCompleteHandlingLab(Signal* signal) 
{
  do {
    if (tcurrentReplicaNo != (Uint8)Z8NIL) {
      jam();
      arrGuard(tcurrentReplicaNo, MAX_REPLICAS);
      switch (tcConnectptr.p->failData[tcurrentReplicaNo]) {
      case LqhTransConf::InvalidStatus:
        jam();
        /*empty*/;
        break;
      default:
        jam();
        /*------------------------------------------------------------*/
        /*       THIS NODE DID NOT REPORT ANYTHING FOR THIS OPERATION */
        /*       IT MUST HAVE FAILED.                                 */
        /*------------------------------------------------------------*/
        /*------------------------------------------------------------*/
        /*       SEND COMPLETEREQ TO THE NEXT REPLICA.                */
        /*------------------------------------------------------------*/
        hostptr.i = tcConnectptr.p->tcNodedata[tcurrentReplicaNo];
        ptrCheckGuard(hostptr, chostFilesize, hostRecord);
        if (hostptr.p->hostStatus == HS_ALIVE) {
          jam();
          Uint32 instanceKey = tcConnectptr.p->lqhInstanceKey;
          tblockref = numberToRef(DBLQH, instanceKey, hostptr.i);
          setApiConTimer(apiConnectptr.i, ctcTimer, __LINE__);
          tcConnectptr.p->tcConnectstate = OS_WAIT_COMPLETE_CONF;
          apiConnectptr.p->apiConnectstate = CS_WAIT_COMPLETE_CONF;
          apiConnectptr.p->timeOutCounter = 0;
          tcConnectptr.p->apiConnect = apiConnectptr.i;
          signal->theData[0] = tcConnectptr.i;
          signal->theData[1] = cownref;
          signal->theData[2] = apiConnectptr.p->transid[0];
          signal->theData[3] = apiConnectptr.p->transid[1];
          signal->theData[4] = apiConnectptr.p->tcBlockref;
          signal->theData[5] = tcConnectptr.p->tcOprec;
          sendSignal(tblockref, GSN_COMPLETEREQ, signal, 6, JBB);
          return;
        }//if
        break;
      }//switch
    }//if
    if (apiConnectptr.p->currentReplicaNo != 0) {
      jam();
      /*------------------------------------------------------------*/
      /*       THERE ARE STILL MORE REPLICAS IN THIS OPERATION. WE  */
      /*       NEED TO CONTINUE WITH THOSE REPLICAS.                */
      /*------------------------------------------------------------*/
      apiConnectptr.p->currentReplicaNo--;
      tcurrentReplicaNo = apiConnectptr.p->currentReplicaNo;
    } else {
      tcConnectptr.i = tcConnectptr.p->nextTcConnect;
      if (tcConnectptr.i == RNIL) {
        /*------------------------------------------------------------*/
        /*       WE HAVE COMPLETED THIS TRANSACTION NOW AND CAN       */
        /*       CONTINUE THE PROCESS WITH THE NEXT TRANSACTION.      */
        /*------------------------------------------------------------*/
        if (apiConnectptr.p->takeOverRec != (Uint8)Z8NIL) {
          jam();
          signal->theData[0] = TcContinueB::ZCOMPLETE_TRANS_AT_TAKE_OVER;
          signal->theData[1] = (UintR)apiConnectptr.p->takeOverRec;
          signal->theData[2] = apiConnectptr.p->takeOverInd;
          sendSignal(cownref, GSN_CONTINUEB, signal, 3, JBB);
          handleGcp(signal, apiConnectptr);
          releaseTakeOver(signal);
        } else {
          jam();

          if (tc_testbit(apiConnectptr.p->m_flags,
                         ApiConnectRecord::TF_LATE_COMMIT))
          {
            jam();

            ApiConnectRecordPtr apiCopy = apiConnectptr;
            
            sendApiLateCommitSignal(signal, apiCopy);
            
            apiConnectptr = apiCopy;
          }

          releaseTransResources(signal);
        }//if
        return;
      }//if
      /*------------------------------------------------------------*/
      /*       WE HAVE COMPLETED AN OPERATION AND THERE ARE MORE TO */
      /*       COMPLETE. TAKE THE NEXT OPERATION AND START WITH THE */
      /*       FIRST REPLICA SINCE IT IS THE COMPLETE PHASE.        */
      /*------------------------------------------------------------*/
      apiConnectptr.p->currentTcConnect = tcConnectptr.i;
      ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
      tcurrentReplicaNo = tcConnectptr.p->lastReplicaNo;
      apiConnectptr.p->currentReplicaNo = tcurrentReplicaNo;
    }//if
  } while (1);
}//Dbtc::toCompleteHandlingLab()

/*----------------------------------------------------------*/
/*       INITIALISE AN API CONNECT FAIL RECORD              */
/*----------------------------------------------------------*/
void Dbtc::initApiConnectFail(Signal* signal,
                              Uint32 transid1,
                              Uint32 transid2,
                              LqhTransConf::OperationStatus transStatus,
                              Uint32 reqinfo,
                              BlockReference applRef,
                              Uint64 gci,
                              NodeId nodeId)
{
  apiConnectptr.p->transid[0] = transid1;
  apiConnectptr.p->transid[1] = transid2;
  apiConnectptr.p->firstTcConnect = RNIL;
  apiConnectptr.p->currSavePointId = 0;
  apiConnectptr.p->lastTcConnect = RNIL;
  BlockReference blockRef = calcTcBlockRef(tcNodeFailptr.p->takeOverNode);

  apiConnectptr.p->tcBlockref = blockRef;
  apiConnectptr.p->ndbapiBlockref = 0;
  apiConnectptr.p->ndbapiConnect = 0;
  apiConnectptr.p->buddyPtr = RNIL;
  apiConnectptr.p->m_transaction_nodes.clear();
  apiConnectptr.p->singleUserMode = 0;
  setApiConTimer(apiConnectptr.i, 0, __LINE__);
  switch(transStatus){
  case LqhTransConf::Committed:
    jam();
    apiConnectptr.p->globalcheckpointid = gci;
    apiConnectptr.p->apiConnectstate = CS_FAIL_COMMITTED;
    break;
  case LqhTransConf::Prepared:
    jam();
    apiConnectptr.p->apiConnectstate = CS_FAIL_PREPARED;
    break;
  case LqhTransConf::Aborted:
    jam();
    apiConnectptr.p->apiConnectstate = CS_FAIL_ABORTED;
    break;
  case LqhTransConf::Marker:
    jam();
    apiConnectptr.p->apiConnectstate = CS_FAIL_COMPLETED;
    break;
  default:
    jam();
    systemErrorLab(signal, __LINE__);
  }//if
  apiConnectptr.p->commitAckMarker = RNIL;
  if(LqhTransConf::getMarkerFlag(reqinfo)){
    jam();
    CommitAckMarkerPtr tmp;

    {
      CommitAckMarker key;
      key.transid1 = transid1;
      key.transid2 = transid2;
      if (m_commitAckMarkerHash.find(tmp, key))
      {
        /**
         * We found a "lingering" marker...
         *   most probably from earlier node failure
         *   check consistency using lots of it-statements
         */
        jam();
        if (tmp.p->apiConnectPtr == RNIL)
        {
          jam();
          // ok - marker had no transaction, use it...
        }
        else
        {
          ApiConnectRecordPtr transPtr;
          transPtr.i = tmp.p->apiConnectPtr;
          ptrCheckGuard(transPtr, capiConnectFilesize, apiConnectRecord);
          if (transPtr.p->commitAckMarker == RNIL)
          {
            jam();
            // ok - marker had a transaction that had moved on, use it...
          }
          else if (transPtr.p->commitAckMarker != tmp.i)
          {
            jam();
            // ok - marker had a transaction that had moved on, use it...
          }
          else
          {
            jam();
            /**
             * marker pointed to a transaction that was pointing to marker...
             *   i.e both transaction + marker lingering...
             *   treat this as an updateApiStateFail and release already seize trans
             */
            releaseApiConnectFail(signal);
            apiConnectptr = transPtr;
            updateApiStateFail(signal,
                               transid1,
                               transid2,
                               transStatus,
                               reqinfo,
                               applRef,
                               gci,
                               nodeId);
            return;
          }
        }
      }
      else
      {
        jam();
        m_commitAckMarkerHash.seize(tmp);
        ndbrequire(tmp.i != RNIL);
        new (tmp.p) CommitAckMarker();
        tmp.p->transid1      = transid1;
        tmp.p->transid2      = transid2;
        m_commitAckMarkerHash.add(tmp);
      }
    }

    apiConnectptr.p->commitAckMarker = tmp.i;
    tmp.p->apiNodeId     = refToNode(applRef);
    tmp.p->apiConnectPtr = apiConnectptr.i;
    ndbrequire(tmp.p->insert_in_commit_ack_marker_all(this, nodeId));
  }
}//Dbtc::initApiConnectFail()

/*------------------------------------------------------------*/
/*       INITIALISE AT TC CONNECT AT TAKE OVER WHEN ALLOCATING*/
/*       THE TC CONNECT RECORD.                               */
/*------------------------------------------------------------*/
void Dbtc::initTcConnectFail(Signal* signal,
                             Uint32 instanceKey,
                             Uint32 tcOprec,
                             Uint32 reqinfo,
                             LqhTransConf::OperationStatus transStatus,
                             NodeId nodeId)
{
  TcConnectRecord * regTcPtr = tcConnectptr.p;
  regTcPtr->apiConnect = apiConnectptr.i;
  regTcPtr->tcOprec = tcOprec;
  Uint32 replicaNo = LqhTransConf::getReplicaNo(reqinfo);
  for (Uint32 i = 0; i < MAX_REPLICAS; i++) {
    regTcPtr->failData[i] = LqhTransConf::InvalidStatus;
  }//for
  regTcPtr->tcNodedata[replicaNo] = nodeId;
  regTcPtr->failData[replicaNo] = transStatus;
  regTcPtr->lastReplicaNo = LqhTransConf::getLastReplicaNo(reqinfo);
  regTcPtr->dirtyOp = LqhTransConf::getDirtyFlag(reqinfo);
  regTcPtr->lqhInstanceKey = instanceKey;
}//Dbtc::initTcConnectFail()

/*----------------------------------------------------------*/
/*       INITIALISE TC NODE FAIL RECORD.                    */
/*----------------------------------------------------------*/
void Dbtc::initTcFail(Signal* signal) 
{
  tcNodeFailptr.i = 0;
  ptrAss(tcNodeFailptr, tcFailRecord);
  tcNodeFailptr.p->queueIndex = 0;
  tcNodeFailptr.p->failStatus = FS_IDLE;
}//Dbtc::initTcFail()

/*----------------------------------------------------------*/
/*               RELEASE_TAKE_OVER                          */
/*----------------------------------------------------------*/
void Dbtc::releaseTakeOver(Signal* signal) 
{
  TcConnectRecordPtr rtoNextTcConnectptr;

  rtoNextTcConnectptr.i = apiConnectptr.p->firstTcConnect;
  do {
    jam();
    tcConnectptr.i = rtoNextTcConnectptr.i;
    ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
    rtoNextTcConnectptr.i = tcConnectptr.p->nextTcConnect;
    releaseTcConnectFail(signal);
  } while (rtoNextTcConnectptr.i != RNIL);
  releaseApiConnectFail(signal);
}//Dbtc::releaseTakeOver()

/*---------------------------------------------------------------------------*/
/*                               SETUP_FAIL_DATA                             */
/* SETUP DATA TO REUSE TAKE OVER CODE FOR HANDLING ABORT/COMMIT IN NODE      */
/* FAILURE SITUATIONS.                                                       */
/*---------------------------------------------------------------------------*/
void Dbtc::setupFailData(Signal* signal) 
{
  tcConnectptr.i = apiConnectptr.p->firstTcConnect;
  do {
    ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
    switch (tcConnectptr.p->tcConnectstate) {
    case OS_PREPARED:
    case OS_COMMITTING:
      jam();
      arrGuard(tcConnectptr.p->lastReplicaNo, MAX_REPLICAS);
      for (tindex = 0; tindex <= tcConnectptr.p->lastReplicaNo; tindex++) {
	jam();
	/*-------------------------------------------------------------------
	 * KEYDATA IS USED TO KEEP AN INDICATION OF STATE IN LQH. 
	 * IN THIS CASE ALL LQH'S ARE PREPARED AND WAITING FOR 
	 * COMMIT/ABORT DECISION.                 
	 *------------------------------------------------------------------*/
	tcConnectptr.p->failData[tindex] = LqhTransConf::Prepared;
      }//for
      break;
    case OS_COMMITTED:
    case OS_COMPLETING:
      jam();
      arrGuard(tcConnectptr.p->lastReplicaNo, MAX_REPLICAS);
      for (tindex = 0; tindex <= tcConnectptr.p->lastReplicaNo; tindex++) {
	jam();
	/*-------------------------------------------------------------------
	 * KEYDATA IS USED TO KEEP AN INDICATION OF STATE IN LQH. 
	 * IN THIS CASE ALL LQH'S ARE COMMITTED AND WAITING FOR 
	 * COMPLETE MESSAGE.                     
	 *------------------------------------------------------------------*/
	tcConnectptr.p->failData[tindex] = LqhTransConf::Committed;
      }//for
      break;
    case OS_COMPLETED:
      jam();
      arrGuard(tcConnectptr.p->lastReplicaNo, MAX_REPLICAS);
      for (tindex = 0; tindex <= tcConnectptr.p->lastReplicaNo; tindex++) {
	jam();
	/*-------------------------------------------------------------------
	 * KEYDATA IS USED TO KEEP AN INDICATION OF STATE IN LQH. 
	 * IN THIS CASE ALL LQH'S ARE COMPLETED.
	 *-------------------------------------------------------------------*/
	tcConnectptr.p->failData[tindex] = LqhTransConf::InvalidStatus;
      }//for
      break;
    default:
      jam();
      sendSystemError(signal, __LINE__);
      break;
    }//switch
    if (tabortInd != ZCOMMIT_SETUP) {
      jam();
      for (UintR Ti = 0; Ti <= tcConnectptr.p->lastReplicaNo; Ti++) {
        hostptr.i = tcConnectptr.p->tcNodedata[Ti];
        ptrCheckGuard(hostptr, chostFilesize, hostRecord);
        if (hostptr.p->hostStatus != HS_ALIVE) {
          jam();
	  /*-----------------------------------------------------------------
	   * FAILURE OF ANY INVOLVED NODE ALWAYS INVOKES AN ABORT DECISION. 
	   *-----------------------------------------------------------------*/
          tabortInd = ZTRUE;
        }//if
      }//for
    }//if
    tcConnectptr.p->tcConnectstate = OS_TAKE_OVER;
    tcConnectptr.p->tcOprec = tcConnectptr.i;
    tcConnectptr.i = tcConnectptr.p->nextTcConnect;
  } while (tcConnectptr.i != RNIL);
  apiConnectptr.p->tcBlockref = cownref;
  apiConnectptr.p->currentTcConnect = apiConnectptr.p->firstTcConnect;
  tcConnectptr.i = apiConnectptr.p->firstTcConnect;
  ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
  apiConnectptr.p->currentReplicaNo = tcConnectptr.p->lastReplicaNo;
  tcurrentReplicaNo = tcConnectptr.p->lastReplicaNo;
}//Dbtc::setupFailData()

/*----------------------------------------------------------*/
/*       UPDATE THE STATE OF THE API CONNECT FOR THIS PART.   */
/*----------------------------------------------------------*/
void Dbtc::updateApiStateFail(Signal* signal,
                              Uint32 transid1,
                              Uint32 transid2,
                              LqhTransConf::OperationStatus transStatus,
                              Uint32 reqinfo,
                              BlockReference applRef,
                              Uint64 gci,
                              NodeId nodeId)
{
  if(LqhTransConf::getMarkerFlag(reqinfo))
  {
    CommitAckMarkerPtr tmp;
    const Uint32 marker = apiConnectptr.p->commitAckMarker;
    if (marker == RNIL)
    {
      jam();

      m_commitAckMarkerHash.seize(tmp);
      ndbrequire(tmp.i != RNIL);
      
      new (tmp.p) CommitAckMarker();
      apiConnectptr.p->commitAckMarker = tmp.i;
      tmp.p->transid1      = transid1;
      tmp.p->transid2      = transid2;
      tmp.p->apiNodeId     = refToNode(applRef);
      tmp.p->apiConnectPtr = apiConnectptr.i;
#if defined VM_TRACE || defined ERROR_INSERT
      {
	CommitAckMarkerPtr check;
	ndbrequire(!m_commitAckMarkerHash.find(check, *tmp.p));
      }
#endif
      m_commitAckMarkerHash.add(tmp);
    } else {
      jam();
      tmp.i = marker;
      tmp.p = m_commitAckMarkerHash.getPtr(marker);

      ndbrequire(tmp.p->transid1 == transid1);
      ndbrequire(tmp.p->transid2 == transid2);
    }
    ndbrequire(tmp.p->insert_in_commit_ack_marker_all(this, nodeId));
  }

  switch (transStatus) {
  case LqhTransConf::Committed:
    jam();
    switch (apiConnectptr.p->apiConnectstate) {
    case CS_FAIL_COMMITTING:
    case CS_FAIL_COMMITTED:
      jam();
      ndbrequire(gci == apiConnectptr.p->globalcheckpointid);
      break;
    case CS_FAIL_PREPARED:
      jam();
      apiConnectptr.p->apiConnectstate = CS_FAIL_COMMITTING;
      apiConnectptr.p->globalcheckpointid = gci;
      break;
    case CS_FAIL_COMPLETED:
      jam();
      apiConnectptr.p->globalcheckpointid = gci;
      apiConnectptr.p->apiConnectstate = CS_FAIL_COMMITTED;
      break;
    default:
      jam();
      systemErrorLab(signal, __LINE__);
      break;
    }//switch
    break;
  case LqhTransConf::Prepared:
    jam();
    switch (apiConnectptr.p->apiConnectstate) {
    case CS_FAIL_COMMITTED:
      jam();
      apiConnectptr.p->apiConnectstate = CS_FAIL_COMMITTING;
      break;
    case CS_FAIL_ABORTED:
      jam();
      apiConnectptr.p->apiConnectstate = CS_FAIL_ABORTING;
      break;
    case CS_FAIL_COMMITTING:
    case CS_FAIL_PREPARED:
    case CS_FAIL_ABORTING:
      jam();
      /*empty*/;
      break;
    default:
      jam();
      systemErrorLab(signal, __LINE__);
      break;
    }//switch
    break;
  case LqhTransConf::Aborted:
    jam();
    switch (apiConnectptr.p->apiConnectstate) {
    case CS_FAIL_COMMITTING:
    case CS_FAIL_COMMITTED:
      jam();
      systemErrorLab(signal, __LINE__);
      break;
    case CS_FAIL_PREPARED:
      jam();
      apiConnectptr.p->apiConnectstate = CS_FAIL_ABORTING;
      break;
    case CS_FAIL_ABORTING:
    case CS_FAIL_ABORTED:
      jam();
      /*empty*/;
      break;
    default:
      jam();
      systemErrorLab(signal, __LINE__);
      break;
    }//switch
    break;
  case LqhTransConf::Marker:
    jam();
    break;
  default:
    jam();
    systemErrorLab(signal, __LINE__);
    break;
  }//switch
}//Dbtc::updateApiStateFail()

/*------------------------------------------------------------*/
/*               UPDATE_TC_STATE_FAIL                         */
/*                                                            */
/*       WE NEED TO UPDATE THE STATUS OF TC_CONNECT RECORD AND*/
/*       WE ALSO NEED TO CHECK THAT THERE IS CONSISTENCY      */
/*       BETWEEN THE DIFFERENT REPLICAS.                      */
/*------------------------------------------------------------*/
void Dbtc::updateTcStateFail(Signal* signal,
                             Uint32 instanceKey,
                             Uint32 tcOprec,
                             Uint32 reqinfo,
                             LqhTransConf::OperationStatus transStatus,
                             NodeId nodeId) 
{
  const Uint8 replicaNo     = LqhTransConf::getReplicaNo(reqinfo);
  const Uint8 lastReplicaNo = LqhTransConf::getLastReplicaNo(reqinfo);
  const Uint8 dirtyOp       = LqhTransConf::getDirtyFlag(reqinfo);

  TcConnectRecord * regTcPtr = tcConnectptr.p;
  
  ndbrequire(regTcPtr->apiConnect == apiConnectptr.i);
  ndbrequire(regTcPtr->failData[replicaNo] == LqhTransConf::InvalidStatus);
  ndbrequire(regTcPtr->lastReplicaNo == lastReplicaNo);
  ndbrequire(regTcPtr->dirtyOp == dirtyOp);
  
  regTcPtr->tcNodedata[replicaNo] = nodeId;
  regTcPtr->failData[replicaNo] = transStatus;
  ndbrequire(regTcPtr->tcOprec == tcOprec);
  ndbrequire(regTcPtr->lqhInstanceKey == instanceKey)
}//Dbtc::updateTcStateFail()

void Dbtc::execTCGETOPSIZEREQ(Signal* signal) 
{
  jamEntry();
  CRASH_INSERTION(8000);

  UintR Tuserpointer = signal->theData[0];         /* DBDIH POINTER         */
  BlockReference Tusersblkref = signal->theData[1];/* DBDIH BLOCK REFERENCE */
  signal->theData[0] = Tuserpointer;
  signal->theData[1] = coperationsize;
  if (refToNode(Tusersblkref) == getOwnNodeId())
  {
    /**
     * The message goes to the DBTC proxy before being processed by
     * DBDIH.
     */
    sendSignal(Tusersblkref, GSN_TCGETOPSIZECONF, signal, 2, JBB);
  }
  else
  {
    /**
     * No proxy used, so this is the only DBTC instance.
     * Thus we go directly to the DBDIH to ensure that we have
     * completed the LCP locally before allowing a new one to
     * start.
     */
    signal->theData[2] = Tusersblkref;
    sendSignal(DBDIH_REF, GSN_CHECK_LCP_IDLE_ORD, signal, 3, JBB);
  }
}//Dbtc::execTCGETOPSIZEREQ()

void Dbtc::execTC_CLOPSIZEREQ(Signal* signal) 
{
  jamEntry();
  CRASH_INSERTION(8001);

  tuserpointer = signal->theData[0];
  tusersblkref = signal->theData[1];
                                            /* DBDIH BLOCK REFERENCE         */
  coperationsize = 0;
  signal->theData[0] = tuserpointer;
  sendSignal(tusersblkref, GSN_TC_CLOPSIZECONF, signal, 1, JBB);
}//Dbtc::execTC_CLOPSIZEREQ()

/* ######################################################################### */
/* #######                        ERROR MODULE                       ####### */
/* ######################################################################### */
void Dbtc::tabStateErrorLab(Signal* signal) 
{
  terrorCode = ZSTATE_ERROR;
  releaseAtErrorLab(signal);
}//Dbtc::tabStateErrorLab()

void Dbtc::wrongSchemaVersionErrorLab(Signal* signal) 
{
  const TcKeyReq * const tcKeyReq = (TcKeyReq *)&signal->theData[0];

  TableRecordPtr tabPtr;
  tabPtr.i = tcKeyReq->tableId;
  const Uint32 schemVer = tcKeyReq->tableSchemaVersion;
  ptrCheckGuard(tabPtr, ctabrecFilesize, tableRecord);

  terrorCode = tabPtr.p->getErrorCode(schemVer);
  
  abortErrorLab(signal);
}//Dbtc::wrongSchemaVersionErrorLab()

void Dbtc::noFreeConnectionErrorLab(Signal* signal) 
{
  terrorCode = ZNO_FREE_TC_CONNECTION;
  abortErrorLab(signal);        /* RECORD. OTHERWISE GOTO ERRORHANDLING  */
}//Dbtc::noFreeConnectionErrorLab()

void Dbtc::aiErrorLab(Signal* signal) 
{
  terrorCode = ZLENGTH_ERROR;
  abortErrorLab(signal);
}//Dbtc::aiErrorLab()

void Dbtc::seizeDatabuferrorLab(Signal* signal) 
{
  terrorCode = ZGET_DATAREC_ERROR;
  releaseAtErrorLab(signal);
}//Dbtc::seizeDatabuferrorLab()

void Dbtc::appendToSectionErrorLab(Signal* signal)
{
  terrorCode = ZGET_DATAREC_ERROR;
  releaseAtErrorLab(signal);
}//Dbtc::appendToSectionErrorLab

void Dbtc::releaseAtErrorLab(Signal* signal) 
{
  ptrGuard(tcConnectptr);
  tcConnectptr.p->tcConnectstate = OS_ABORTING;
  /*-------------------------------------------------------------------------*
   * A FAILURE OF THIS OPERATION HAS OCCURRED. THIS FAILURE WAS EITHER A 
   * FAULTY PARAMETER OR A RESOURCE THAT WAS NOT AVAILABLE. 
   * WE WILL ABORT THE ENTIRE TRANSACTION SINCE THIS IS THE SAFEST PATH 
   * TO HANDLE THIS PROBLEM.      
   * SINCE WE HAVE NOT YET CONTACTED ANY LQH WE SET NUMBER OF NODES TO ZERO
   * WE ALSO SET THE STATE TO ABORTING TO INDICATE THAT WE ARE NOT EXPECTING 
   * ANY SIGNALS.                              
   *-------------------------------------------------------------------------*/
  tcConnectptr.p->noOfNodes = 0;
  abortErrorLab(signal);
}//Dbtc::releaseAtErrorLab()

void Dbtc::warningHandlerLab(Signal* signal, int line) 
{
  ndbassert(false);
}//Dbtc::warningHandlerLab()

void Dbtc::systemErrorLab(Signal* signal, int line) 
{
  progError(line, NDBD_EXIT_NDBREQUIRE);
}//Dbtc::systemErrorLab()


#ifdef ERROR_INSERT
bool Dbtc::testFragmentDrop(Signal* signal)
{
  Uint32 fragIdToDrop= ~0;
  /* Drop some fragments to test the dropped fragment handling code */
  if (ERROR_INSERTED(8074))
    fragIdToDrop= 1;
  else if (ERROR_INSERTED(8075))
    fragIdToDrop= 2;
  else if (ERROR_INSERTED(8076))
    fragIdToDrop= 3;
  
  if ((signal->header.m_fragmentInfo == fragIdToDrop) ||
      ERROR_INSERTED(8077)) // Drop all fragments
  {
    /* This signal fragment should be dropped 
     * Let's throw away the sections, and call the
     * signal dropped report handler
     * This code is replicating the effect of the code in
     * TransporterCallback::deliver_signal()
     */
    SectionHandle handle(this, signal);
    Uint32 secCount= handle.m_cnt;
    releaseSections(handle);
    SignalDroppedRep* rep = (SignalDroppedRep*)signal->theData;
    Uint32 gsn = signal->header.theVerId_signalNumber;
    Uint32 len = signal->header.theLength;
    Uint32 newLen= (len > 22 ? 22 : len);
    memmove(rep->originalData, signal->theData, (4 * newLen));
    rep->originalGsn = gsn;
    rep->originalLength = len;
    rep->originalSectionCount = secCount;
    signal->header.theVerId_signalNumber = GSN_SIGNAL_DROPPED_REP;
    signal->header.theLength = newLen + 3;
    signal->header.m_noOfSections = 0;

    executeFunction(GSN_SIGNAL_DROPPED_REP, signal);
    return true;
  }
  return false;
}
#endif

/* ######################################################################### *
 * #######                        SCAN MODULE                        ####### *
 * ######################################################################### *

  The application orders a scan of a table.  We divide the scan into a scan on
  each fragment.  The scan uses the primary replicas since the scan might be   
  used for an update in a separate transaction. 

  Scans are always done as a separate transaction.  Locks from the scan
  can be overtaken by another transaction.  Scans can never lock the entire
  table.  Locks are released immediately after the read has been verified 
  by the application. There is not even an option to leave the locks. 
  The reason is that this would hurt real-time behaviour too much.

  -#  The first step in handling a scan of a table is to receive all signals
      defining the scan. If failures occur during this step we release all 
      resource and reply with SCAN_TABREF providing the error code. 
      If system load is too high, the request will not be allowed.   
   
  -#  The second step retrieves the number of fragments that exist in the 
      table. It also ensures that the table actually exist.  After this,
      the scan is ready to be parallelised.  The idea is that the receiving 
      process (hereafter called delivery process) will start up a number 
      of scan processes.  Each of these scan processes will 
      independently scan one fragment at a time.  The delivery
      process object is the scan record and the scan process object is 
      the scan fragment record plus the scan operation record.
   
  -#  The third step is thus performed in parallel. In the third step each 
      scan process retrieves the primary replica of the fragment it will 
      scan.  Then it starts the scan as soon as the load on that node permits. 
   
  The LQH returns either when it retrieved the maximum number of tuples or 
  when it has retrived at least one tuple and is hindered by a lock to
  retrieve the next tuple.  This is to ensure that a scan process never 
  can be involved in a deadlock situation.   

  Tuples from each fragment scan are sent directly to API from TUP, and tuples
  from different fragments are delivered in parallel (so will be interleaved
  when received).

  When a batch of tuples from one fragment has been fully fetched, the scan of
  that fragment will not continue until the previous batch has been
  acknowledged by API with a SCAN_NEXTREQ signal.


  ERROR HANDLING
   
  As already mentioned it is rather easy to handle errors before the scan 
  processes have started.  In this case it is enough to release the resources 
  and send SCAN_TAB_REF.
   
  If an error occurs in any of the scan processes then we have to stop all 
  scan processes. We do however only stop the delivery process and ask 
  the api to order us to close the scan.  The reason is that we can easily 
  enter into difficult timing problems since the application and this 
  block is out of synch we will thus always start by report the error to
  the application and wait for a close request. This error report uses the 
  SCAN_TABREF signal with a special error code that the api must check for.   
   
   
  CLOSING AN ACTIVE SCAN
   
  The application can close a scan for several reasons before it is completed.
  One reason was mentioned above where an error in a scan process led to a  
  request to close the scan. Another reason could simply be that the 
  application found what it looked for and is thus not interested in the 
  rest of the scan.  

  IT COULD ALSO BE DEPENDENT ON INTERNAL ERRORS IN THE API. 
   
  When a close scan request is received, all scan processes are stopped and all
  resources belonging to those scan processes are released. Stopping the scan  
  processes most often includes communication with an LQH where the local scan 
  is controlled. Finally all resources belonging to the scan is released and 
  the SCAN_TABCONF is sent with an indication of that the scan is closed.   
   
   
  CLOSING A COMPLETED SCAN  
   
  When all scan processes are completed then a report is sent to the 
  application which indicates that no more tuples can be fetched. 
  The application will send a close scan and the same action as when 
  closing an active scan is performed. 
  In this case it will of course not find any active scan processes. 
  It will even find all scan processes already released.
   
  The reason for requiring the api to close the scan is the same as above. 
  It is to avoid any timing problems due to that the api and this block 
  is out of synch.
   
  * ######################################################################## */
void Dbtc::execSCAN_TABREQ(Signal* signal) 
{
  jamEntry();

#ifdef ERROR_INSERT
  /* Test fragmented + dropped signal handling */
  if (ERROR_INSERTED(8074) ||
      ERROR_INSERTED(8075) ||
      ERROR_INSERTED(8076) ||
      ERROR_INSERTED(8077))
  {
    jam();
    if (testFragmentDrop(signal)) {
      jam();
      return;
    }
  } /* End of test fragmented + dropped signal handling */
#endif  

  /* Reassemble if the request was fragmented */
  if (!assembleFragments(signal)){
    jam();
    return;
  }

  const BlockReference apiBlockRef = signal->getSendersBlockRef();
  const ScanTabReq * const scanTabReq = (ScanTabReq *)&signal->theData[0];
  const Uint32 ri = scanTabReq->requestInfo;
  const Uint32 schemaVersion = scanTabReq->tableSchemaVersion;
  const Uint32 transid1 = scanTabReq->transId1;
  const Uint32 transid2 = scanTabReq->transId2;
  const Uint32 tmpXX = scanTabReq->buddyConPtr;
  const Uint32 buddyPtr = (tmpXX == 0xFFFFFFFF ? RNIL : tmpXX);
  Uint32 currSavePointId = 0;
  
  Uint32 errCode;
  ScanRecordPtr scanptr;

  /* Short SCANTABREQ has 1 section, Long has 2 or 3.
   * Section 0 : NDBAPI receiver ids (Mandatory)
   * Section 1 : ATTRINFO section (Mandatory for long SCAN_TABREQ
   * Section 2 : KEYINFO section (Optional for long SCAN_TABREQ
   */
  Uint32 numSections= signal->getNoOfSections();
  ndbassert( numSections >= 1 );
  bool isLongReq= numSections >= 2;

  SectionHandle handle(this, signal);
  SegmentedSectionPtr api_op_ptr;
  handle.getSection(api_op_ptr, ScanTabReq::ReceiverIdSectionNum);

  // Scan parallelism is determined by the number of receiver ids sent
  Uint32 scanParallel = api_op_ptr.sz;
  Uint32 scanConcurrency = scanParallel;
  Uint32 * apiPtr = signal->theData+25; // temp storage
  copy(apiPtr, api_op_ptr);

  Uint32 aiLength= 0;
  Uint32 keyLen= 0;

  if (likely(isLongReq))
  {
    SegmentedSectionPtr attrInfoPtr, keyInfoPtr;
    /* Long SCANTABREQ, determine Ai and Key length from sections */
    handle.getSection(attrInfoPtr, ScanTabReq::AttrInfoSectionNum);
    aiLength= attrInfoPtr.sz;
    if (numSections == 3)
    {
      handle.getSection(keyInfoPtr, ScanTabReq::KeyInfoSectionNum);
      keyLen= keyInfoPtr.sz;
    }
  }
  else
  {
    /* Short SCANTABREQ, get Ai and Key length from signal */
    aiLength = (scanTabReq->attrLenKeyLen & 0xFFFF);
    keyLen = scanTabReq->attrLenKeyLen >> 16;
  }

  apiConnectptr.i = scanTabReq->apiConnectPtr;
  tabptr.i = scanTabReq->tableId;

  if (apiConnectptr.i >= capiConnectFilesize)
  {
    jam();
    releaseSections(handle);
    warningHandlerLab(signal, __LINE__);
    return;
  }//if

  ptrAss(apiConnectptr, apiConnectRecord);
  ApiConnectRecord * transP = apiConnectptr.p;

  if (transP->apiConnectstate != CS_CONNECTED) {
    jam();
    // could be left over from TCKEYREQ rollback
    if (transP->apiConnectstate == CS_ABORTING &&
	transP->abortState == AS_IDLE) {
      jam();
    } else if(transP->apiConnectstate == CS_STARTED && 
	      transP->firstTcConnect == RNIL){
      jam();
      // left over from simple/dirty read
    } else {
      jam();
      jamLine(transP->apiConnectstate);
      errCode = ZSTATE_ERROR;
      goto SCAN_TAB_error_no_state_change;
    }
  }
  ndbassert(transP->ndbapiBlockref == apiBlockRef);

  if(tabptr.i >= ctabrecFilesize)
  {
    errCode = ZUNKNOWN_TABLE_ERROR;
    goto SCAN_TAB_error;
  }

  if (unlikely (ScanTabReq::getViaSPJFlag(ri) &&
                /* Check that all nodes can handle SPJ requests. */
                !ndbd_join_pushdown(getNodeVersionInfo().m_type[NodeInfo::DB]
                                    .m_min_version)))
  {
    jam();
    errCode = 4003; // Function not implemented
    goto SCAN_TAB_error;
  }

  if (unlikely(ScanTabReq::getMultiFragFlag(ri) &&
               !ScanTabReq::getViaSPJFlag(ri)))
  {
    jam();
    errCode = 4003; // Function not implemented
    goto SCAN_TAB_error;
  }

  ptrAss(tabptr, tableRecord);
  if ((aiLength == 0) ||
      (!tabptr.p->checkTable(schemaVersion)) ||
      (scanConcurrency == 0) ||
      (cfirstfreeScanrec == RNIL)) {
    goto SCAN_error_check;
  }
  if (buddyPtr != RNIL) {
    ApiConnectRecordPtr buddyApiPtr;
    buddyApiPtr.i = buddyPtr;
    ptrCheckGuard(buddyApiPtr, capiConnectFilesize, apiConnectRecord);
    if ((transid1 == buddyApiPtr.p->transid[0]) &&
	(transid2 == buddyApiPtr.p->transid[1])) {
      jam();
      
      if (buddyApiPtr.p->apiConnectstate == CS_ABORTING) {
	// transaction has been aborted
	jam();
	errCode = buddyApiPtr.p->returncode;
	goto SCAN_TAB_error;
      }//if
      currSavePointId = buddyApiPtr.p->currSavePointId;
      buddyApiPtr.p->currSavePointId++;
    }
    else
    {
      jam();
    }
  }
  
  if (getNodeState().startLevel == NodeState::SL_SINGLEUSER &&
      getNodeState().getSingleUserApi() !=
      refToNode(apiConnectptr.p->ndbapiBlockref))
  {
    errCode = ZCLUSTER_IN_SINGLEUSER_MODE;
    goto SCAN_TAB_error;
  }

  scanptr = seizeScanrec(signal);
  ndbrequire(transP->apiScanRec == RNIL);
  ndbrequire(scanptr.p->scanApiRec == RNIL);

  errCode = initScanrec(scanptr, scanTabReq, scanParallel,
                        apiPtr);
  if (unlikely(errCode))
  {
    jam();
    transP->apiScanRec = scanptr.i;
    releaseScanResources(signal, scanptr, true /* NotStarted */);
    goto SCAN_TAB_error;
  }

  releaseSection(handle.m_ptr[ScanTabReq::ReceiverIdSectionNum].i);
  if (likely(isLongReq))
  {
    jamDebug();
    /* We keep the AttrInfo and KeyInfo sections */
    scanptr.p->scanAttrInfoPtr = handle.m_ptr[ScanTabReq::AttrInfoSectionNum].i;
    if (keyLen)
    {
      jamDebug();
      scanptr.p->scanKeyInfoPtr = handle.m_ptr[ScanTabReq::KeyInfoSectionNum].i;
    }
  }
  else
  {
    jam();
    seizeCacheRecord(signal);
    cachePtr.p->attrlength = aiLength;
    cachePtr.p->keylen = keyLen;
    cachePtr.p->save1 = 0;
  }
  handle.clear();

  scanptr.p->m_scan_dist_key = scanTabReq->distributionKey;
  scanptr.p->m_scan_dist_key_flag = ScanTabReq::getDistributionKeyFlag(ri);

  transP->apiScanRec = scanptr.i;
  transP->returncode = 0;
  transP->transid[0] = transid1;
  transP->transid[1] = transid2;
  transP->buddyPtr   = buddyPtr;

  // The scan is started
  transP->apiConnectstate = CS_START_SCAN;
  transP->currSavePointId = currSavePointId;

  /**********************************************************
  * We start the timer on scanRec to be able to discover a 
  * timeout in the API the API now is in charge!
  ***********************************************************/
  setApiConTimer(apiConnectptr.i, ctcTimer, __LINE__);
  updateBuddyTimer(apiConnectptr);

  /***********************************************************
   * WE HAVE NOW RECEIVED ALL REFERENCES TO SCAN OBJECTS IN 
   * THE API. WE ARE NOW READY TO RECEIVE THE ATTRIBUTE INFO 
   * IF ANY TO RECEIVE.
   **********************************************************/
  scanptr.p->scanState = ScanRecord::WAIT_AI;

  if (ERROR_INSERTED(8038))
  {
    /**
     * Force API_FAILREQ
     */
    signal->theData[0] = refToNode(apiConnectptr.p->ndbapiBlockref);
    sendSignal(QMGR_REF, GSN_API_FAILREQ, signal, 1, JBA);
    CLEAR_ERROR_INSERT_VALUE;
  }

  if (isLongReq)
  {
    /* All AttrInfo (and KeyInfo) has been received, continue
     * processing
     */
    diFcountReqLab(signal, scanptr);
  }
  
  return;

 SCAN_error_check:
  if (aiLength == 0) {
    jam();
    errCode = ZSCAN_AI_LEN_ERROR;
    goto SCAN_TAB_error;
  }//if
  if (!tabptr.p->checkTable(schemaVersion)){
    jam();
    errCode = tabptr.p->getErrorCode(schemaVersion);
    goto SCAN_TAB_error;
  }//if  
  if (scanConcurrency == 0) {
    jam();
    errCode = ZNO_CONCURRENCY_ERROR;
    goto SCAN_TAB_error;
  }//if
  ndbrequire(cfirstfreeScanrec == RNIL);
  ndbrequire(cConcScanCount == cscanrecFileSize);
  jam();
  errCode = ZNO_SCANREC_ERROR;
  goto SCAN_TAB_error;
 
SCAN_TAB_error:
  jam();
  /**
   * Prepare for up coming ATTRINFO/KEYINFO
   */
  transP->apiConnectstate = CS_ABORTING;
  transP->abortState = AS_IDLE;
  transP->transid[0] = transid1;
  transP->transid[1] = transid2;
 
SCAN_TAB_error_no_state_change:
  
  releaseSections(handle);

  ScanTabRef * ref = (ScanTabRef*)&signal->theData[0];
  ref->apiConnectPtr = transP->ndbapiConnect;
  ref->transId1 = transid1;
  ref->transId2 = transid2;
  ref->errorCode  = errCode;
  ref->closeNeeded = 0;
  sendSignal(apiBlockRef, GSN_SCAN_TABREF, 
	     signal, ScanTabRef::SignalLength, JBB);

  /**
   * If we are DISCONNECTED, an API_FAILREQ signals had
   * already arrived when we got the SCAN_TABREQ. Those
   * are required to arrive as the last signal from a failed
   * API-node. We want to detect that if it should happen.
   */
  ndbassert(transP->apiConnectstate != CS_DISCONNECTED);
  return;
}//Dbtc::execSCAN_TABREQ()

Uint32
Dbtc::initScanrec(ScanRecordPtr scanptr,
		  const ScanTabReq * scanTabReq,
		  UintR scanParallel,
                  const Uint32 apiPtr[])
{
  const UintR ri = scanTabReq->requestInfo;
  const Uint32 batchSizeRows = ScanTabReq::getScanBatch(ri);
  scanptr.p->scanApiRec = apiConnectptr.i;
  scanptr.p->scanTableref = tabptr.i;
  scanptr.p->scanSchemaVersion = scanTabReq->tableSchemaVersion;
  scanptr.p->scanNoFrag = 0;
  scanptr.p->scanParallel = scanParallel;
  scanptr.p->first_batch_size_rows = scanTabReq->first_batch_size;
  scanptr.p->batch_byte_size = scanTabReq->batch_byte_size;
  scanptr.p->batch_size_rows = batchSizeRows;
  scanptr.p->m_scan_block_no = DBLQH;
  scanptr.p->m_scan_dist_key_flag = 0;
  scanptr.p->m_start_ticks = getHighResTimer();

  Uint32 tmp = 0;
  ScanFragReq::setLockMode(tmp, ScanTabReq::getLockMode(ri));
  ScanFragReq::setHoldLockFlag(tmp, ScanTabReq::getHoldLockFlag(ri));
  ScanFragReq::setKeyinfoFlag(tmp, ScanTabReq::getKeyinfoFlag(ri));
  ScanFragReq::setReadCommittedFlag(tmp,ScanTabReq::getReadCommittedFlag(ri));
  ScanFragReq::setRangeScanFlag(tmp, ScanTabReq::getRangeScanFlag(ri));
  ScanFragReq::setDescendingFlag(tmp, ScanTabReq::getDescendingFlag(ri));
  ScanFragReq::setTupScanFlag(tmp, ScanTabReq::getTupScanFlag(ri));
  ScanFragReq::setNoDiskFlag(tmp, ScanTabReq::getNoDiskFlag(ri));
  ScanFragReq::setMultiFragFlag(tmp, ScanTabReq::getMultiFragFlag(ri));
  if (ScanTabReq::getViaSPJFlag(ri))
  {
    jam();
    scanptr.p->m_scan_block_no = DBSPJ;
  }

  scanptr.p->scanRequestInfo = tmp;
  scanptr.p->m_read_committed_base = ScanTabReq::getReadCommittedBaseFlag(ri);
  scanptr.p->scanStoredProcId = scanTabReq->storedProcId;
  scanptr.p->scanState = ScanRecord::RUNNING;
  scanptr.p->m_queued_count = 0;
  scanptr.p->m_booked_fragments_count = 0;
  scanptr.p->m_scan_cookie = DihScanTabConf::InvalidCookie;
  scanptr.p->m_close_scan_req = false;
  scanptr.p->m_pass_all_confs =  ScanTabReq::getPassAllConfsFlag(ri);
  scanptr.p->m_4word_conf = ScanTabReq::get4WordConf(ri);
  scanptr.p->scanKeyInfoPtr = RNIL;
  scanptr.p->scanAttrInfoPtr = RNIL;

  Local_ScanFragRec_dllist list(c_scan_frag_pool,
		    scanptr.p->m_running_scan_frags);
  for (Uint32 i = 0; i < scanParallel; i++) {
    jamDebug();
    ScanFragRecPtr ptr;
    if (unlikely((list.seizeFirst(ptr) == false) ||
                 ERROR_INSERTED(8093)))
    {
      jam();
      goto errout;
    }
    ptr.p->scanFragState = ScanFragRec::IDLE;
    ptr.p->scanRec = scanptr.i;
    ptr.p->lqhScanFragId = 0;
    ptr.p->lqhBlockref = 0;
    ptr.p->m_apiPtr = apiPtr[i];
  }//for
  scanptr.p->m_booked_fragments_count = scanParallel;

  (* (ScanTabReq::getRangeScanFlag(ri) ? 
      &c_counters.c_range_scan_count : 
      &c_counters.c_scan_count))++;
  return 0;
errout:
  {
    ScanFragRecPtr ptr;
    bool found = list.first(ptr);
    while (found)
    {
      ScanFragRecPtr old_ptr = ptr;
      ptr.p->scanFragState = ScanFragRec::COMPLETED;
      found = list.next(ptr);
      list.release(old_ptr);
    }
  }
  return ZSCAN_FRAGREC_ERROR;
}//Dbtc::initScanrec()

void Dbtc::scanTabRefLab(Signal* signal, Uint32 errCode) 
{
  ScanTabRef * ref = (ScanTabRef*)&signal->theData[0];
  ref->apiConnectPtr = apiConnectptr.p->ndbapiConnect;
  ref->transId1 = apiConnectptr.p->transid[0];
  ref->transId2 = apiConnectptr.p->transid[1];
  ref->errorCode  = errCode;
  ref->closeNeeded = 0;
  sendSignal(apiConnectptr.p->ndbapiBlockref, GSN_SCAN_TABREF, 
	     signal, ScanTabRef::SignalLength, JBB);
}//Dbtc::scanTabRefLab()

/**
 * scanKeyinfoLab
 * Handle reception of KeyInfo for a Scan
 */
void Dbtc::scanKeyinfoLab(Signal* signal)
{
  /* Receive KEYINFO for a SCAN operation 
   * Note that old NDBAPI nodes sometimes send header-only KEYINFO signals
   */
  CacheRecord * const regCachePtr = cachePtr.p;
  UintR TkeyLen = regCachePtr->keylen;
  UintR Tlen = regCachePtr->save1;

  Uint32 wordsInSignal= MIN(KeyInfo::DataLength,
                            (TkeyLen - Tlen));

  ndbassert( signal->getLength() == 
             (KeyInfo::HeaderLength + wordsInSignal) );

  if (unlikely ((! appendToSection(regCachePtr->keyInfoSectionI,
                                   &signal->theData[KeyInfo::HeaderLength],
                                   wordsInSignal)) ||
                 ERROR_INSERTED(8094)))
  {
    jam();
    Uint32 transid0 = apiConnectptr.p->transid[0];
    Uint32 transid1 = apiConnectptr.p->transid[1];
    ndbrequire(apiConnectptr.p->apiScanRec != RNIL);
    ScanRecordPtr scanPtr;
    scanPtr.i = apiConnectptr.p->apiScanRec;
    ptrCheckGuard(scanPtr, cscanrecFileSize, scanRecord);
    abortScanLab(signal, 
                 scanPtr,
                 ZGET_DATAREC_ERROR, 
                 true /* Not started */);
    
    /* Prepare for up coming ATTRINFO/KEYINFO */
    apiConnectptr.p->apiConnectstate = CS_ABORTING;
    apiConnectptr.p->abortState = AS_IDLE;
    apiConnectptr.p->transid[0] = transid0;
    apiConnectptr.p->transid[1] = transid1;

    return;
  }

  Tlen+= wordsInSignal;
  regCachePtr->save1 = Tlen;
  
  if (Tlen < TkeyLen)
  {
    jam();
    /* More KeyInfo still to come - continue waiting */
    setApiConTimer(apiConnectptr.i, ctcTimer, __LINE__);
    return;
  }

  /* All KeyInfo has been received, we will now start receiving
   * ATTRINFO
   */
  jam();
  ndbassert(Tlen == TkeyLen);
  return;
} // scanKeyinfoLab

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*       RECEPTION OF ATTRINFO FOR SCAN TABLE REQUEST.                       */
/*---------------------------------------------------------------------------*/
void Dbtc::scanAttrinfoLab(Signal* signal, UintR Tlen) 
{
  ScanRecordPtr scanptr;
  scanptr.i = apiConnectptr.p->apiScanRec;
  ptrCheckGuard(scanptr, cscanrecFileSize, scanRecord);
  cachePtr.i = apiConnectptr.p->cachePtr;
  ptrCheckGuard(cachePtr, ccacheFilesize, cacheRecord);
  CacheRecord * const regCachePtr = cachePtr.p;
  ndbrequire(scanptr.p->scanState == ScanRecord::WAIT_AI);

  regCachePtr->currReclenAi = regCachePtr->currReclenAi + Tlen;

  if (unlikely(! appendToSection(regCachePtr->attrInfoSectionI,
                                 &signal->theData[AttrInfo::HeaderLength],
                                 Tlen)))
  {
    jam();
    abortScanLab(signal, scanptr, ZGET_ATTRBUF_ERROR, true);
    return;
  }

  if (regCachePtr->currReclenAi == regCachePtr->attrlength)
  {
    jam();
    /* We have now received all the signals defining this
     * scan.  We are ready to start executing the scan
     */
    scanptr.p->scanAttrInfoPtr = regCachePtr->attrInfoSectionI;
    scanptr.p->scanKeyInfoPtr = regCachePtr->keyInfoSectionI;
    releaseCacheRecord(apiConnectptr, regCachePtr);
    diFcountReqLab(signal, scanptr);
    return;
  }
  else if (unlikely (regCachePtr->currReclenAi > regCachePtr->attrlength))
  {
    jam();
    abortScanLab(signal, scanptr, ZLENGTH_ERROR, true);
    return;
  }
  
  /* Still some ATTRINFO to arrive...*/
  return;
}//Dbtc::scanAttrinfoLab()

void Dbtc::diFcountReqLab(Signal* signal, ScanRecordPtr scanptr)
{
  /**
   * Check so that the table is not being dropped
   */
  TableRecordPtr tabPtr;
  tabPtr.i = scanptr.p->scanTableref;
  tabPtr.p = &tableRecord[tabPtr.i];
  if (tabPtr.p->checkTable(scanptr.p->scanSchemaVersion)){
    ;
  } else {
    abortScanLab(signal, scanptr, 
		 tabPtr.p->getErrorCode(scanptr.p->scanSchemaVersion),
		 true);
    return;
  }

  scanptr.p->scanNextFragId = 0;
  ndbassert(scanptr.p->m_booked_fragments_count == scanptr.p->scanParallel);
  scanptr.p->m_read_any_node =
    (ScanFragReq::getReadCommittedFlag(scanptr.p->scanRequestInfo) ||
       scanptr.p->m_read_committed_base) &&
    ((tabPtr.p->m_flags & TableRecord::TR_FULLY_REPLICATED) != 0);
  
  /*************************************************
   * THE FIRST STEP TO RECEIVE IS SUCCESSFULLY COMPLETED.
   ***************************************************/
  ndbassert(scanptr.p->m_scan_cookie == DihScanTabConf::InvalidCookie);
  DihScanTabReq * req = (DihScanTabReq*)signal->getDataPtrSend();
  req->tableId = scanptr.p->scanTableref;
  req->schemaTransId = 0;
  req->jamBufferPtr = jamBuffer();

  EXECUTE_DIRECT_MT(DBDIH, GSN_DIH_SCAN_TAB_REQ, signal,
                    DihScanTabReq::SignalLength, 0);

  DihScanTabConf * conf = (DihScanTabConf*)signal->getDataPtr();
  if (conf->senderData == 0)
  {
    execDIH_SCAN_TAB_CONF(signal, scanptr, tabPtr);
    return;
  }
  else
  {
    execDIH_SCAN_TAB_REF(signal, scanptr);
    return;
  }
}//Dbtc::diFcountReqLab()

/********************************************************************
 * execDIH_SCAN_TAB_CONF
 *
 * WE HAVE ASKED DIH ABOUT THE NUMBER OF FRAGMENTS IN THIS TABLE. 
 * WE WILL NOW GET THE LOCATION (NODE,INSTANCE) OF EACH OF THE
 * FRAGMENTS AS A PREPARATION FOR STARTING THE SCAN.
 ********************************************************************/
void Dbtc::execDIH_SCAN_TAB_CONF(Signal* signal,
                                 ScanRecordPtr scanptr,
                                 TableRecordPtr tabPtr)
{
  DihScanTabConf * conf = (DihScanTabConf*)signal->getDataPtr();
  jamEntryDebug();
  Uint32 tfragCount = conf->fragmentCount;
  ApiConnectRecord * const regApiPtr = apiConnectptr.p;
  scanptr.p->m_scan_cookie = conf->scanCookie;
  ndbrequire(scanptr.p->m_scan_cookie != DihScanTabConf::InvalidCookie);

  if (conf->reorgFlag)
  {
    jam();
    ScanFragReq::setReorgFlag(scanptr.p->scanRequestInfo, ScanFragReq::REORG_NOT_MOVED);
  }
  if (regApiPtr->apiFailState != ApiConnectRecord::AFS_API_OK)
  {
    jam();
    releaseScanResources(signal, scanptr, true);
    handleApiFailState(signal, apiConnectptr.i);
    return;
  }//if
  if (tfragCount == 0) {
    jam();
    abortScanLab(signal, scanptr, ZNO_FRAGMENT_ERROR, true);
    return;
  }//if
  
  /* We are in a EXECUTE_DIRECT reply from diFcountReqLab() 
   * where checkTable() already succeeded
   */
  ndbassert(tabptr.p->checkTable(scanptr.p->scanSchemaVersion));

  if (scanptr.p->m_scan_dist_key_flag)   //Pruned scan
  {
    jamDebug();
    ndbrequire(DictTabInfo::isOrderedIndex(tabPtr.p->tableType) ||
               tabPtr.p->get_user_defined_partitioning());

    /**
     * Prepare for sendDihGetNodeReq to request DBDIH info for
     * the single pruned-to fragId we got from NDB API.
     */
    tfragCount = 1;
    scanptr.p->scanNextFragId = scanptr.p->m_scan_dist_key;
  }
  else
  {
    ndbassert(scanptr.p->scanNextFragId == 0);
  }

  scanptr.p->scanParallel = tfragCount;
  scanptr.p->scanNoFrag = tfragCount;
  scanptr.p->scanState = ScanRecord::RUNNING;

  setApiConTimer(apiConnectptr.i, 0, __LINE__);
  updateBuddyTimer(apiConnectptr);
  
  /**
   * Request fragment info from DIH.
   */
  jam();
  sendDihGetNodesLab(signal, scanptr);
}//Dbtc::execDIH_SCAN_TAB_CONF()


struct ScanFragLocation
{
  Uint32 blockRef;
  Uint32 fragId;
};

static
int compareFragLocation(const void * a, const void * b)
{
  return (((ScanFragLocation*)a)->blockRef - ((ScanFragLocation*)b)->blockRef);
}

/********************************************************************
 * sendDihGetNodesLab
 *
 * Will request DBDIH for the LQH fragment location of all 'scanNoFrag'
 * fragments. Insert a fragment location record in the list
 * 'ScanRecord::m_fragLocations' for each fragment to be scanned.
 *
 * As the DBDIH signaling is direct executed, we can execute for quite
 * a while here. To avoid too much work in one request we send CONTINUEB
 * every MAX_DIGETNODESREQS'th signal to space out the signals a bit.
 ********************************************************************/
void Dbtc::sendDihGetNodesLab(Signal* signal, ScanRecordPtr scanptr)
{
  jam();
  Uint32 fragCnt = 0;
  ScanRecord* const scanP = scanptr.p;

  if (scanP->scanState == ScanRecord::CLOSING_SCAN)
  {
    jam();
    updateBuddyTimer(apiConnectptr);
    close_scan_req_send_conf(signal, scanptr);
    return;
  }

  TableRecordPtr tabPtr;
  tabPtr.i = scanP->scanTableref;
  ptrCheckGuard(tabPtr, ctabrecFilesize, tableRecord);

  /**
   * Check table, ERROR_INSERT verify scanError() failure handling.
   */
  const Uint32 schemaVersion = scanP->scanSchemaVersion;
  if (ERROR_INSERTED_CLEAR(8081))
  {
    jam();
    scanError(signal, scanptr, ZTIME_OUT_ERROR);
    return;
  }
  if (unlikely(!tabPtr.p->checkTable(schemaVersion)))
  {
    jam();
    scanError(signal, scanptr, tabPtr.p->getErrorCode(schemaVersion));
    return;
  }

  /**
   * Note that the above checkTable() and scanState checking is
   * sufficient for all sendDihGetNodeReq's below: As EXECUTE_DIRECT
   * is used, there cant be any other signals processed inbetween
   * which close the scan, or drop the table.
   */
  do
  {
    ndbassert(scanP->scanState == ScanRecord::RUNNING);
    ndbassert(tabPtr.p->checkTable(schemaVersion) == true);

    /**
     * We check for CONTINUEB sending here to limits how many
     * direct executed 'GSN_DIGETNODESREQ' we can do in
     * one signal to ensure we keep the rules of not executing
     * for more than 5-10 microseconds per signal.
     */
    if (fragCnt >= DiGetNodesReq::MAX_DIGETNODESREQS)
    {
      jam();
      signal->theData[0] = TcContinueB::ZSTART_FRAG_SCANS;
      signal->theData[1] = scanptr.i;
      sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
      return;
    }

    const bool success = sendDihGetNodeReq(signal, scanptr, scanP->scanNextFragId);
    if (!success)
    {
      jam();
      return;  //sendDihGetNodeReq called scanError() upon failure.
    }
    fragCnt++;
    scanP->scanNextFragId++;

  } while (scanP->scanNextFragId < scanP->scanNoFrag);

  /**
   * We got the 'fragLocations' for all fragments to be scanned.
   * For MultiFrag' scans the SPJ-instance has not been filled
   * in yet. All frag scans in this REQ is now set up to use the
   * same SPJ instance.
   *
   * Then sort the fragLocations[] on blockRef such that possible
   * multiFrag scans could easily find fragId's to be included in
   * the same multiFragment SCAN_FRAGREQ
   */
  if (ScanFragReq::getMultiFragFlag(scanP->scanRequestInfo))
  {
    jam();
    Uint32 i;
    ScanFragLocationPtr ptr;
    Local_ScanFragLocation_list frags(m_fragLocationPool, scanP->m_fragLocations);
    const Uint32 spjInstance = (cspjInstanceRR++ % 120) + 1;

    ScanFragLocation fragLocations[MAX_NDB_PARTITIONS];

    /* Construct SPJ blockRefs, fill in fragment locations into a temporary array */
    for (frags.first(ptr),i=0; !ptr.isNull(); frags.next(ptr),i++)
    {
      const BlockReference blockRef = ptr.p->blockRef;
      const NodeId nodeId = refToNode(blockRef);
      ndbassert(refToMain(blockRef) == DBSPJ);

      ndbassert(i < MAX_NDB_PARTITIONS);
      fragLocations[i].fragId = ptr.p->fragId;
      fragLocations[i].blockRef = numberToRef(DBSPJ, spjInstance, nodeId);
    }

    /* Sort fragment locations on 'blockRef' */
    qsort(fragLocations, scanP->scanNoFrag,
          sizeof(ScanFragLocation), compareFragLocation);

    /* Write back the blockRef-sorted fragment locations */
    for (frags.first(ptr),i=0; !ptr.isNull(); frags.next(ptr),i++)
    {
      ptr.p->fragId   = fragLocations[i].fragId;
      ptr.p->blockRef = fragLocations[i].blockRef;
    }
  }

  /* Start sending SCAN_FRAGREQ's, possibly interrupted with CONTINUEB */
  scanP->scanNextFragId = 0;
  sendFragScansLab(signal, scanptr);
}//Dbtc::sendDihGetNodesLab

/******************************************************
 * execDIH_SCAN_TAB_REF
 ******************************************************/
void Dbtc::execDIH_SCAN_TAB_REF(Signal* signal, ScanRecordPtr scanptr)
{
  jamEntry();
  DihScanTabRef * ref = (DihScanTabRef*)signal->getDataPtr();
  const Uint32 errCode = ref->error;
  if (apiConnectptr.p->apiFailState != ApiConnectRecord::AFS_API_OK)
  {
    jam();
    releaseScanResources(signal, scanptr, true);
    handleApiFailState(signal, apiConnectptr.i);
    return;
  }//if
  abortScanLab(signal, scanptr, errCode, true);
}//Dbtc::execDIH_SCAN_TAB_REF()

/**
 * Abort a scan not yet 'RUNNING' on any LDM's.
 *
 * Use ::scanError() instead for failing a scan which
 * is in 'RUNNING' or in 'CLOSING_SCAN' state.
 */
void Dbtc::abortScanLab(Signal* signal, ScanRecordPtr scanptr, Uint32 errCode,
			bool not_started) 
{
  ndbassert(scanptr.p->scanState != ScanRecord::RUNNING);
  ndbassert(scanptr.p->scanState != ScanRecord::CLOSING_SCAN);

  time_track_complete_scan_error(scanptr.p,
                                 refToNode(apiConnectptr.p->ndbapiBlockref));
  scanTabRefLab(signal, errCode);
  releaseScanResources(signal, scanptr, not_started);
}//Dbtc::abortScanLab()

void Dbtc::releaseScanResources(Signal* signal,
                                ScanRecordPtr scanPtr,
				bool not_started)
{
  if (apiConnectptr.p->cachePtr != RNIL) {
    cachePtr.i = apiConnectptr.p->cachePtr;
    ptrCheckGuard(cachePtr, ccacheFilesize, cacheRecord);
    releaseKeys();
    releaseAttrinfo();
  }//if

  if (not_started)
  {
    jam();
    Local_ScanFragRec_dllist run(c_scan_frag_pool, scanPtr.p->m_running_scan_frags);
    Local_ScanFragRec_dllist queue(c_scan_frag_pool, scanPtr.p->m_queued_scan_frags);
    ScanFragRecPtr ptr;
    bool found = run.first(ptr);
    while (found)
    {
      ScanFragRecPtr old_ptr = ptr;
      ptr.p->scanFragState = ScanFragRec::COMPLETED;
      found = run.next(ptr);
      run.release(old_ptr);
    }
    found = queue.first(ptr);
    while (found)
    {
      ScanFragRecPtr old_ptr = ptr;
      ptr.p->scanFragState = ScanFragRec::COMPLETED;
      found = queue.next(ptr);
      queue.release(old_ptr);
    }
  }

  {
    ScanFragLocationPtr ptr;
    Local_ScanFragLocation_list frags(m_fragLocationPool,
                                      scanPtr.p->m_fragLocations);
    while (frags.removeFirst(ptr))
    {
      m_fragLocationPool.release(ptr);
    }
  }

  if (scanPtr.p->scanKeyInfoPtr != RNIL)
  {
    releaseSection(scanPtr.p->scanKeyInfoPtr);
    scanPtr.p->scanKeyInfoPtr = RNIL;
  }

  if (scanPtr.p->scanAttrInfoPtr != RNIL)
  {
    releaseSection(scanPtr.p->scanAttrInfoPtr);
    scanPtr.p->scanAttrInfoPtr = RNIL;
  }

  ndbrequire(scanPtr.p->m_running_scan_frags.isEmpty());
  ndbrequire(scanPtr.p->m_queued_scan_frags.isEmpty());
  ndbrequire(scanPtr.p->m_delivered_scan_frags.isEmpty());

  ndbassert(scanPtr.p->scanApiRec == apiConnectptr.i);
  ndbassert(apiConnectptr.p->apiScanRec == scanPtr.i);

  if (scanPtr.p->m_scan_cookie != DihScanTabConf::InvalidCookie)
  {
    jam();
    /* Cookie was requested, 'return' it */
    DihScanTabCompleteRep* rep = (DihScanTabCompleteRep*)signal->getDataPtrSend();
    rep->tableId = scanPtr.p->scanTableref;
    rep->scanCookie = scanPtr.p->m_scan_cookie;
    rep->jamBufferPtr = jamBuffer();

    EXECUTE_DIRECT_MT(DBDIH, GSN_DIH_SCAN_TAB_COMPLETE_REP, signal,
                      DihScanTabCompleteRep::SignalLength, 0);
    jamEntryDebug();
    /* No return code, it will always succeed. */
    scanPtr.p->m_scan_cookie = DihScanTabConf::InvalidCookie;
  }
    
  // link into free list
  scanPtr.p->nextScan = cfirstfreeScanrec;
  scanPtr.p->scanState = ScanRecord::IDLE;
  scanPtr.p->scanApiRec = RNIL;
  cfirstfreeScanrec = scanPtr.i;
  ndbassert(cConcScanCount > 0);
  cConcScanCount--;
  
  apiConnectptr.p->apiScanRec = RNIL;
  apiConnectptr.p->apiConnectstate = CS_CONNECTED;
  setApiConTimer(apiConnectptr.i, 0, __LINE__);
}//Dbtc::releaseScanResources()

bool Dbtc::sendDihGetNodeReq(Signal* signal,
                             ScanRecordPtr scanptr,
                             Uint32 scanFragId)
{
  jamDebug();
  DiGetNodesReq * const req = (DiGetNodesReq *)&signal->theData[0];

  req->tableId = scanptr.p->scanTableref;
  req->hashValue = scanFragId;
  req->distr_key_indicator = ZTRUE;
  req->scan_indicator = ZTRUE;
  req->anyNode = scanptr.p->m_read_any_node;
  req->jamBufferPtr = jamBuffer();
  req->get_next_fragid_indicator = 0;

  if (scanptr.p->m_scan_dist_key_flag) //Scan pruned to specific fragment
  {
    jamDebug();
    ndbassert(scanFragId == scanptr.p->m_scan_dist_key);

    TableRecordPtr tabPtr;
    tabPtr.i = scanptr.p->scanTableref;
    ptrCheckGuard(tabPtr, ctabrecFilesize, tableRecord);

    req->distr_key_indicator = tabPtr.p->get_user_defined_partitioning();
  }

  EXECUTE_DIRECT_MT(DBDIH, GSN_DIGETNODESREQ, signal,
                    DiGetNodesReq::SignalLength, 0);

  jamEntryDebug();
  /**
   * theData[0] is always '0' in a DiGetNodesCONF,
   * else it is a REF, with errorCode in theData[1]
   */
  const Uint32 errorCode =
    (signal->theData[0] != 0)    ? signal->theData[1] : //DIH error
    (ERROR_INSERTED_CLEAR(8095)) ? ZGET_DATAREC_ERROR : //Fake error
    0;

  if (errorCode != 0)
  {
    scanError(signal, scanptr, errorCode);
    return false;
  }

  const DiGetNodesConf * conf = (DiGetNodesConf *)&signal->theData[0]; 
  const Uint32 lqhScanFragId = conf->fragId;
  NodeId nodeId = conf->nodes[0];
  const NodeId ownNodeId = getOwnNodeId();

  arrGuard(nodeId, MAX_NDB_NODES);
  {
    if (ERROR_INSERTED(8050) &&
        nodeId != ownNodeId)
    {
      /* Asked to scan a fragment which is not on the same node as the
       * TC - transaction hinting / scan partition pruning has failed
       * Used by testPartitioning.cpp
       */
      CRASH_INSERTION(8050);
    }
  }

  TableRecordPtr tabPtr;
  tabPtr.i = scanptr.p->scanTableref;
  ptrCheckGuard(tabPtr, ctabrecFilesize, tableRecord);

  /**
   * This must be false as select count(*) otherwise
   *   can "pass" committing on backup fragments and
   *   get incorrect row count
   *
   * The table property TR_READ_BACKUP guarantees that all commits will
   * release locks on all replicas before reporting the commit to the
   * application. This means that we are safe to also read from backups
   * for committed reads. We avoid doing it for locking reads to avoid
   * unnecessary deadlock scenarios. We do it however for those
   * operations that have flagged that the base is committed read.
   * Base operations on BLOB tables upgrade locks in this fashion and
   * they are safe to read from backup replicas without causing
   * more deadlocks than otherwise would happen.
   */
  Uint32 TreadBackup = (tabPtr.p->m_flags & TableRecord::TR_READ_BACKUP);
  if (TreadBackup &&
      (ScanFragReq::getReadCommittedFlag(scanptr.p->scanRequestInfo) ||
       scanptr.p->m_read_committed_base))
  {
    jam();
    /* Primary not counted in DIGETNODES signal */
    Uint32 count = (conf->reqinfo & 0xFFFF) + 1;
    for (Uint32 i = 1; i < count; i++)
    {
      if (conf->nodes[i] == ownNodeId)
      {
        jam();
        nodeId = ownNodeId;
        break;
      }
    }
    if (nodeId != ownNodeId)
    {
      Uint32 node;
      jam();
      Uint16 nodes[4];
      nodes[0] = (Uint16)conf->nodes[0];
      nodes[1] = (Uint16)conf->nodes[1];
      nodes[2] = (Uint16)conf->nodes[2];
      nodes[3] = (Uint16)conf->nodes[3];
      if ((node = check_own_location_domain(&nodes[0],
                                            count)) != 0)
      {
        nodeId = node;
      }
    }
  }

  if (ERROR_INSERTED(8083) &&
      nodeId != ownNodeId)
  {
    ndbabort();  // Only node-local reads 
  }

  /* Send SCANFRAGREQ directly to LQH block, or 'viaSPJ'
   * In the later case we may do a Round-Robin load distribution
   * among the SPJ instances if multiple SPJ requests are needed.
   */
  Uint32 blockInstance;
  if (scanptr.p->m_scan_block_no == DBLQH)
  {
    /**
     * Get instance key from upper bits except most significant bit
     * which is used for reorg moving flag.
     */
    blockInstance = (conf->reqinfo >> 24) & 127;
  }
  // Else, it is a 'viaSPJ request':
  else if (ScanFragReq::getMultiFragFlag(scanptr.p->scanRequestInfo))
  {
    //SPJ instance is set together with qsort'ing of m_fragLocations
    blockInstance = 0;
  }
  // Prefer own TC/SPJ intance if a single request to ownNodeId
  else if (nodeId == ownNodeId &&        //Request to ownNodeId
           scanptr.p->scanNoFrag == 1)   //Pruned to single fragment
  {
    blockInstance = instance();          //Choose SPJ-instance in own block-thread
  }
  else
  {
    //See comment for other usage of 'cspjInstanceRR'
    blockInstance = (cspjInstanceRR++ % 120) + 1;
  }

  /* SCANFRAGREQ with optional KEYINFO and mandatory ATTRINFO are
   * now sent to LQH / SPJ
   * This starts the scan on the given fragment(s).
   * If this is the last SCANFRAGREQ, sendScanFragReq will release
   * the KeyInfo and AttrInfo sections when sending.
   */
  const BlockReference blockRef = numberToRef(scanptr.p->m_scan_block_no,
                                              blockInstance, nodeId);

  //Build list of fragId locations.
  {
    ScanFragLocationPtr fragLocationPtr;
    Local_ScanFragLocation_list list(m_fragLocationPool,
                                     scanptr.p->m_fragLocations);

    if (unlikely(!m_fragLocationPool.seize(fragLocationPtr)))
    {
      jam();
      scanError(signal, scanptr, ZGET_DATAREC_ERROR);
      return false;
    }
    fragLocationPtr.p->fragId   = lqhScanFragId;
    fragLocationPtr.p->blockRef = blockRef;
    list.addLast(fragLocationPtr);
  }

  return true;
}//Dbtc::sendDihGetNodeReq

/********************************************************************
 * ::sendFragScansLab()
 *
 * WE WILL NOW START A NUMBER OF PARALLEL SCAN PROCESSES. EACH OF 
 * THESE WILL SCAN ONE FRAGMENT AT A TIME, OR A SET OF 'MULTI-FRAGMENTS'
 * LOCATED AT THE SAME NODE/INSTANCE. To avoid too much work in one
 * request we send CONTINUEB every 4th signal to space out the
 * signals a bit.
 ********************************************************************/
void Dbtc::sendFragScansLab(Signal* signal,
                            ScanRecordPtr scanptr)
{
  jamDebug();
  ScanFragRecPtr scanFragP;
  Uint32 fragCnt = 0;
  Uint32 cntLocSignals = 0;
  const NodeId ownNodeId = getOwnNodeId();

  TableRecordPtr tabPtr;
  tabPtr.i = scanptr.p->scanTableref;
  ptrCheckGuard(tabPtr, ctabrecFilesize, tableRecord);

  if (scanptr.p->scanState == ScanRecord::CLOSING_SCAN)
  {
    jam();
    updateBuddyTimer(apiConnectptr);
    close_scan_req_send_conf(signal, scanptr);
    return;
  }

  /**
   * Check table, ERROR_INSERT to verify scanError() failure handling.
   */
  const Uint32 schemaVersion = scanptr.p->scanSchemaVersion;
  if (ERROR_INSERTED_CLEAR(8115))
  {
    jam();
    scanError(signal, scanptr, ZTIME_OUT_ERROR);
    return;
  }
  if (unlikely(!tabPtr.p->checkTable(schemaVersion)))
  {
    jam();
    scanError(signal, scanptr, tabPtr.p->getErrorCode(schemaVersion));
    return;
  }

  /**
   * Note that the above checkTable() and scanState checking is
   * sufficient for all the sendScanFragReq's below:
   * Table or scanState can not change while we are in controll.
   * sendScanFragReq() itself can fail, possibly closing the scan.
   * However, that is caught by checking its return value.
   */
  scanFragP.i = RNIL;
  while (!scanptr.p->m_fragLocations.isEmpty())
  {
    ndbassert(tabPtr.p->checkTable(schemaVersion) == true);
    ndbassert(scanptr.p->scanState != ScanRecord::CLOSING_SCAN);

    { // running-list scope
      Local_ScanFragRec_dllist list(c_scan_frag_pool, scanptr.p->m_running_scan_frags);

      /**
       * Since we have to leave the running-list scope for the list while
       * calling sendScanFragReq we have to have the below logic to
       * restart the loop when coming back from sendScanFragReq.
       */
      if (scanFragP.i == RNIL)
      {
        jamDebug();
        list.first(scanFragP);
      }
      else
      {
        jamDebug();
        list.next(scanFragP);
      }
      for (; !scanFragP.isNull(); list.next(scanFragP))
      {
        if (scanFragP.p->scanFragState == ScanFragRec::IDLE) // Start it NOW!.
        {
          jam();
          /**
           * A max fanout of 1::4 of consumed::produced signals are allowed.
           * If we are about to produce more, we have to contine later.
           */
	  if (cntLocSignals > 4)
          {
            jam();
            signal->theData[0] = TcContinueB::ZSEND_FRAG_SCANS;
            signal->theData[1] = scanptr.i;
            sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
            return;
          }
          if (fragCnt > 0 &&
              scanptr.p->scanNextFragId == scanptr.p->scanNoFrag-1 && //Last FragId
              ERROR_INSERTED_CLEAR(8097))
          {
            jam();
            signal->theData[0] = TcContinueB::ZSEND_FRAG_SCANS;
            signal->theData[1] = scanptr.i;
            sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 2, 10);
            return;
          }

          /**
           * Need to break out of running-list scope before calling
           * sendScanFragReq.
           */
          fragCnt++;
          break;
        } // if IDLE
      }
    }
    /**
     * We have to distinguish between exiting the loop due to end of loop or
     * to get out of running-list scope to call sendScanFragReq.
     */
    if (scanFragP.isNull())
    {
      jam();
      return;
    }

    ndbassert(scanptr.p->m_booked_fragments_count > 0);
    scanptr.p->m_booked_fragments_count--;
    const bool success = sendScanFragReq(signal, scanptr, scanFragP);
    if (!success)
    {
      jam();
      return;
    }

    const NodeId nodeId = refToNode(scanFragP.p->lqhBlockref);
    const bool local = (nodeId == ownNodeId);
    if (local)
      cntLocSignals++;
  } //while (!scanptr.p->m_fragLocations.isEmpty());

  /**
   * There could possibly be some leftover ScanFragRec's if the
   * API prepared for a larger 'parallelism' than needed to scan
   * all fragments. We finish of any excess ScanFragRec's by
   * letting them return an 'empty' result set.
   */
  {
    Local_ScanFragRec_dllist list(c_scan_frag_pool, scanptr.p->m_running_scan_frags);
    Local_ScanFragRec_dllist queued(c_scan_frag_pool, scanptr.p->m_queued_scan_frags);

    list.next(scanFragP);
    while (!scanFragP.isNull())
    {
      jam();
      ndbassert(scanFragP.p->scanFragState == ScanFragRec::IDLE);

      // Prepare an 'empty result' reply for this 'scanFragP'
      scanFragP.p->m_ops = 0;
      scanFragP.p->m_totalLen = 0;
      scanFragP.p->m_scan_frag_conf_status = 1;
      scanFragP.p->scanFragState = ScanFragRec::QUEUED_FOR_DELIVERY;
      scanFragP.p->stopFragTimer();

      // Insert into 'queued' reply messages
      ScanFragRecPtr tmp(scanFragP);
      list.next(scanFragP);
      list.remove(tmp);
      queued.addFirst(tmp);
      scanptr.p->m_queued_count++;
      ndbassert(scanptr.p->m_booked_fragments_count > 0);
      scanptr.p->m_booked_fragments_count--;
    }
  }
  /*********************************************
   * WE HAVE NOW STARTED A FRAGMENT SCAN. NOW 
   * WAIT FOR THE FIRST SCANNED RECORDS
   *********************************************/
}//Dbtc::sendFragScansLab


/**
 * Dbtc::execSCAN_FRAGREF
 *  Our attempt to scan a fragment was refused
 *  set error code and close all other fragment 
 *  scan's belonging to this scan
 */
void Dbtc::execSCAN_FRAGREF(Signal* signal) 
{
  const ScanFragRef * const ref = (ScanFragRef *)&signal->theData[0];
  
  jamEntry();
  const Uint32 errCode = ref->errorCode;

  scanFragptr.i = ref->senderData;
  c_scan_frag_pool.getPtr(scanFragptr);

  ScanRecordPtr scanptr;
  scanptr.i = scanFragptr.p->scanRec;
  ptrCheckGuard(scanptr, cscanrecFileSize, scanRecord);

  apiConnectptr.i = scanptr.p->scanApiRec;
  ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);

  Uint32 transid1 = apiConnectptr.p->transid[0] ^ ref->transId1;
  Uint32 transid2 = apiConnectptr.p->transid[1] ^ ref->transId2;
  transid1 = transid1 | transid2;
  if (transid1 != 0) {
    jam();
    systemErrorLab(signal, __LINE__);
  }//if

  /**
   * Set errorcode, close connection to this lqh fragment,
   * stop fragment timer and call scanFragError to start 
   * close of the other fragment scans
   */
  ndbrequire(scanFragptr.p->scanFragState == ScanFragRec::LQH_ACTIVE);
  scanFragptr.p->scanFragState = ScanFragRec::COMPLETED;
  scanFragptr.p->stopFragTimer();
  time_track_complete_scan_frag_error(scanFragptr.p);
  {
    Local_ScanFragRec_dllist run(c_scan_frag_pool, scanptr.p->m_running_scan_frags);
    run.release(scanFragptr);
  }    
  scanError(signal, scanptr, errCode);
}//Dbtc::execSCAN_FRAGREF()

/**
 * Dbtc::scanError
 *
 * Called when an error occurs during
 */ 
void Dbtc::scanError(Signal* signal, ScanRecordPtr scanptr, Uint32 errorCode) 
{
  jam();
  ScanRecord* scanP = scanptr.p;
  
  DEBUG("scanError, errorCode = "<< errorCode << 
	", scanState = " << scanptr.p->scanState);

  apiConnectptr.i = scanP->scanApiRec;
  ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
  ndbrequire(apiConnectptr.p->apiScanRec == scanptr.i);

  if(scanP->scanState == ScanRecord::CLOSING_SCAN){
    jam();
    close_scan_req_send_conf(signal, scanptr);
    return;
  }
  
  ndbrequire(scanP->scanState == ScanRecord::RUNNING);
  
  /**
   * Close scan wo/ having received an order to do so
   */
  close_scan_req(signal, scanptr, false);
  
  if (apiConnectptr.p->apiFailState != ApiConnectRecord::AFS_API_OK)
  {
    jam();
    return;
  }
  
  ScanTabRef * ref = (ScanTabRef*)&signal->theData[0];
  ref->apiConnectPtr = apiConnectptr.p->ndbapiConnect;
  ref->transId1 = apiConnectptr.p->transid[0];
  ref->transId2 = apiConnectptr.p->transid[1];
  ref->errorCode  = errorCode;
  ref->closeNeeded = 1;
  sendSignal(apiConnectptr.p->ndbapiBlockref, GSN_SCAN_TABREF, 
	     signal, ScanTabRef::SignalLength, JBB);
}//Dbtc::scanError()

/************************************************************
 * execSCAN_FRAGCONF
 *
 * A NUMBER OF OPERATIONS HAVE BEEN COMPLETED IN THIS 
 * FRAGMENT. TAKE CARE OF AND ISSUE FURTHER ACTIONS.
 ************************************************************/
void Dbtc::execSCAN_FRAGCONF(Signal* signal) 
{
  Uint32 transid1, transid2, total_len;
  jamEntry();

  const ScanFragConf * const conf = (ScanFragConf*)&signal->theData[0];
  const Uint32 noCompletedOps = conf->completedOps;
  const Uint32 status = conf->fragmentCompleted;

  scanFragptr.i = conf->senderData;
  c_scan_frag_pool.getPtr(scanFragptr);
  
  ScanRecordPtr scanptr;
  scanptr.i = scanFragptr.p->scanRec;
  ptrCheckGuard(scanptr, cscanrecFileSize, scanRecord);
  
  apiConnectptr.i = scanptr.p->scanApiRec;
  ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);

  transid1 = apiConnectptr.p->transid[0] ^ conf->transId1;
  transid2 = apiConnectptr.p->transid[1] ^ conf->transId2;
  total_len= conf->total_len;
  transid1 = transid1 | transid2;
  if (transid1 != 0) {
    jam();
    systemErrorLab(signal, __LINE__);
  }//if

  ndbrequire(scanFragptr.p->scanFragState == ScanFragRec::LQH_ACTIVE);
  if (refToMain(scanFragptr.p->lqhBlockref) == DBLQH)
  {
    jamDebug();
    time_track_complete_scan_frag(scanFragptr.p);
  }

  if(scanptr.p->scanState == ScanRecord::CLOSING_SCAN){
    if(status == 0)
    {
      jam();
      /**
       * We have started closing = we sent a close -> ignore this
       */
      return;
    } else {
      jam();
      scanFragptr.p->stopFragTimer();
      scanFragptr.p->scanFragState = ScanFragRec::COMPLETED;
      {
        Local_ScanFragRec_dllist run(c_scan_frag_pool, scanptr.p->m_running_scan_frags);
        run.release(scanFragptr);
      }
    }
    close_scan_req_send_conf(signal, scanptr);
    return;
  }

  scanFragptr.p->stopFragTimer();

  if(noCompletedOps == 0 && status != 0 && 
     !scanptr.p->m_pass_all_confs &&
     scanptr.p->scanNextFragId+scanptr.p->m_booked_fragments_count < scanptr.p->scanNoFrag)
  {
    /**
     * Start on next fragment. Don't do this if we scan via the SPJ block. In
     * that case, dropping the last SCAN_TABCONF message for a fragment would
     * mean dropping the 'nodeMask' (which is sent in ScanFragConf::total_len).
     * This would confuse the API with respect to which pushed operations that
     * would get new tuples in the next batch. If we use SPJ, we must thus
     * send SCAN_TABCONF and let the API ask for the next batch.
     */
    jam();
    scanFragptr.p->scanFragState = ScanFragRec::IDLE;
    const bool ok = sendScanFragReq(signal, scanptr, scanFragptr);
    if (!ok)
    {
      jam();
    }
    return;
  }
 /* 
  Uint32 totalLen = 0;
  for(Uint32 i = 0; i<noCompletedOps; i++){
    Uint32 tmp = conf->opReturnDataLen[i];
    totalLen += tmp;
  }
 */ 
  {
    Local_ScanFragRec_dllist run(c_scan_frag_pool, scanptr.p->m_running_scan_frags);
    Local_ScanFragRec_dllist queued(c_scan_frag_pool, scanptr.p->m_queued_scan_frags);
    
    run.remove(scanFragptr);
    queued.addFirst(scanFragptr);
    scanptr.p->m_queued_count++;
  }

  if (status != 0 &&
      scanptr.p->m_pass_all_confs &&
      scanptr.p->scanNextFragId+scanptr.p->m_booked_fragments_count
      < scanptr.p->scanNoFrag){
    /**
     * nodeMask(=total_len) should be zero since there will be no more
     * rows from this fragment.
     */
    ndbrequire(total_len==0);
    /**
     * Now set it to one to tell the API that there may be more rows from
     * the next fragment.
     */
    total_len  = 1;
  }

  scanFragptr.p->m_scan_frag_conf_status = status;
  scanFragptr.p->m_ops = noCompletedOps;
  scanFragptr.p->m_totalLen = total_len;
  scanFragptr.p->scanFragState = ScanFragRec::QUEUED_FOR_DELIVERY;

  if(scanptr.p->m_queued_count > /** Min */ 0){
    jamDebug();
    sendScanTabConf(signal, scanptr);
  }
}//Dbtc::execSCAN_FRAGCONF()

/****************************************************************************
 * execSCAN_NEXTREQ
 *
 * THE APPLICATION HAS PROCESSED THE TUPLES TRANSFERRED AND IS NOW READY FOR
 * MORE. THIS SIGNAL IS ALSO USED TO CLOSE THE SCAN. 
 ****************************************************************************/
void Dbtc::execSCAN_NEXTREQ(Signal* signal) 
{
  const ScanNextReq * const req = (ScanNextReq *)&signal->theData[0];
  const UintR transid1 = req->transId1;
  const UintR transid2 = req->transId2;
  const UintR stopScan = req->stopScan;

  jamEntry();

  SectionHandle handle(this, signal);
  apiConnectptr.i = req->apiConnectPtr;
  if (apiConnectptr.i >= capiConnectFilesize) {
    jam();
    releaseSections(handle);
    warningHandlerLab(signal, __LINE__);
    return;
  }//if
  ptrAss(apiConnectptr, apiConnectRecord);

  /**
   * Check transid
   */
  const UintR ctransid1 = apiConnectptr.p->transid[0] ^ transid1;
  const UintR ctransid2 = apiConnectptr.p->transid[1] ^ transid2;
  if ((ctransid1 | ctransid2) != 0){
    releaseSections(handle);
    ScanTabRef * ref = (ScanTabRef*)&signal->theData[0];
    ref->apiConnectPtr = apiConnectptr.p->ndbapiConnect;
    ref->transId1 = transid1;
    ref->transId2 = transid2;
    ref->errorCode  = ZSTATE_ERROR;
    ref->closeNeeded = 0;
    sendSignal(signal->senderBlockRef(), GSN_SCAN_TABREF, 
	       signal, ScanTabRef::SignalLength, JBB);
    DEBUG("Wrong transid");
    return;
  }

  /**
   * Check state of API connection
   */
  if (apiConnectptr.p->apiConnectstate != CS_START_SCAN) {
    jam();
    releaseSections(handle);
    if (apiConnectptr.p->apiConnectstate == CS_CONNECTED) {
      jam();
      /*********************************************************************
       * The application sends a SCAN_NEXTREQ after experiencing a time-out.
       *  We will send a SCAN_TABREF to indicate a time-out occurred.
       *********************************************************************/
      DEBUG("scanTabRefLab: ZSCANTIME_OUT_ERROR2");
      ndbout_c("apiConnectptr(%d) -> abort", apiConnectptr.i);
      ndbabort(); //B2 indication of strange things going on
      scanTabRefLab(signal, ZSCANTIME_OUT_ERROR2);
      return;
    }
    DEBUG("scanTabRefLab: ZSTATE_ERROR");
    DEBUG("  apiConnectstate="<<apiConnectptr.p->apiConnectstate);
    ndbabort(); //B2 indication of strange things going on
    scanTabRefLab(signal, ZSTATE_ERROR);
    return;
  }//if
  
  /*******************************************************
   * START THE ACTUAL LOGIC OF SCAN_NEXTREQ. 
   ********************************************************/
  // Stop the timer that is used to check for timeout in the API 
  setApiConTimer(apiConnectptr.i, 0, __LINE__);
  ScanRecordPtr scanptr;
  scanptr.i = apiConnectptr.p->apiScanRec;
  ptrCheckGuard(scanptr, cscanrecFileSize, scanRecord);
  ScanRecord* scanP = scanptr.p;

  /* Copy ReceiverIds to working space past end of signal
   * so that we don't overwrite them when sending signals
   */
  Uint32 len = 0;
  if (handle.m_cnt > 0)
  {
    jam();
    /* TODO : Add Dropped signal handling for SCAN_NEXTREQ */
    /* Receiver ids are in a long section */
    ndbrequire(signal->getLength() == ScanNextReq::SignalLength);
    ndbrequire(handle.m_cnt == 1);
    SegmentedSectionPtr receiverIdsSection;
    ndbrequire(handle.getSection(receiverIdsSection, 
                                 ScanNextReq::ReceiverIdsSectionNum));
    len= receiverIdsSection.p->m_sz;
    ndbassert(len < (8192 - 25));
    
    copy(signal->getDataPtrSend()+25, receiverIdsSection);
    releaseSections(handle);
  }
  else
  {
    jam();
    len= signal->getLength() - ScanNextReq::SignalLength;
    memcpy(signal->getDataPtrSend()+25, 
           signal->getDataPtr()+ ScanNextReq::SignalLength, 
           4 * len);
  }

  if (stopScan == ZTRUE) {
    jam();
    /*********************************************************************
     * APPLICATION IS CLOSING THE SCAN.
     **********************************************************************/
    close_scan_req(signal, scanptr, true);
    return;
  }//if

  if (scanP->scanState == ScanRecord::CLOSING_SCAN){
    jam();
    /**
     * The scan is closing (typically due to error)
     *   but the API hasn't understood it yet
     *
     * Wait for API close request
     */
    return;
  }

  ScanFragNextReq tmp;
  tmp.requestInfo = 0;
  tmp.transId1 = apiConnectptr.p->transid[0];
  tmp.transId2 = apiConnectptr.p->transid[1];
  tmp.batch_size_rows = scanP->batch_size_rows;
  tmp.batch_size_bytes = scanP->batch_byte_size;

  for(Uint32 i = 0 ; i<len; i++)
  {
    jam();
    scanFragptr.i = signal->theData[i+25];
    c_scan_frag_pool.getPtr(scanFragptr);
    ndbrequire(scanFragptr.p->scanFragState == ScanFragRec::DELIVERED);
    
    scanFragptr.p->startFragTimer(ctcTimer);
    scanFragptr.p->m_ops = 0;

    {
      Local_ScanFragRec_dllist running(c_scan_frag_pool, scanP->m_running_scan_frags);
      Local_ScanFragRec_dllist delivered(c_scan_frag_pool, scanP->m_delivered_scan_frags);
      delivered.remove(scanFragptr);
      running.addFirst(scanFragptr);
    }

    if(scanFragptr.p->m_scan_frag_conf_status)
    {
      /**
       * Last scan was complete.
       * Reuse this ScanFragRec 'thread' for scanning 'scanNextFragId'
       */
      jam();
      ndbrequire(scanptr.p->scanNextFragId < scanptr.p->scanNoFrag);
      ndbassert(scanptr.p->m_booked_fragments_count);
      scanptr.p->m_booked_fragments_count--;

      scanFragptr.p->scanFragState = ScanFragRec::IDLE;
      const bool ok = sendScanFragReq(signal, scanptr, scanFragptr);
      if (!ok)
      {
        jam();
        return; //scanError() has already been called
      }
    }
    else
    {
      jam();
      scanFragptr.p->scanFragState = ScanFragRec::LQH_ACTIVE;
      scanFragptr.p->m_start_ticks = getHighResTimer();
      ScanFragNextReq * req = (ScanFragNextReq*)signal->getDataPtrSend();
      * req = tmp;
      req->senderData = scanFragptr.i;
      sendSignal(scanFragptr.p->lqhBlockref, GSN_SCAN_NEXTREQ, signal, 
		 ScanFragNextReq::SignalLength, JBB);
    }
  }//for
  
}//Dbtc::execSCAN_NEXTREQ()

void
Dbtc::close_scan_req(Signal* signal, ScanRecordPtr scanPtr, bool req_received){

  ScanRecord* scanP = scanPtr.p;
  ndbrequire(scanPtr.p->scanState != ScanRecord::IDLE);  
  ScanRecord::ScanState old = scanPtr.p->scanState;
  scanPtr.p->scanState = ScanRecord::CLOSING_SCAN;
  scanPtr.p->m_close_scan_req = req_received;

  if (old == ScanRecord::WAIT_FRAGMENT_COUNT)  //Dead state due to direct-exec
  {
    jam();
    scanPtr.p->scanState = old;
    return; // Will continue on execDI_FCOUNTCONF
  }
  
  /**
   * Queue         : Action
   * ============= : =================
   * completed     : -
   * running       : close -> LQH
   * delivered w/  : close -> LQH
   * delivered wo/ : move to completed
   * queued w/     : close -> LQH
   * queued wo/    : move to completed
   */
  
  ScanFragNextReq * nextReq = (ScanFragNextReq*)&signal->theData[0];
  nextReq->requestInfo = 0;
  ScanFragNextReq::setCloseFlag(nextReq->requestInfo, 1);
  nextReq->transId1 = apiConnectptr.p->transid[0];
  nextReq->transId2 = apiConnectptr.p->transid[1];
  
  {
    ScanFragRecPtr ptr;
    Local_ScanFragRec_dllist running(c_scan_frag_pool, scanP->m_running_scan_frags);
    Local_ScanFragRec_dllist delivered(c_scan_frag_pool, scanP->m_delivered_scan_frags);
    Local_ScanFragRec_dllist queued(c_scan_frag_pool, scanP->m_queued_scan_frags);
    
    // Close running
    for(running.first(ptr); !ptr.isNull(); ){
      ScanFragRecPtr curr = ptr; // Remove while iterating...
      running.next(ptr);

      switch(curr.p->scanFragState){
      case ScanFragRec::IDLE:
	jam(); // real early abort
	ndbrequire(old == ScanRecord::WAIT_AI || old == ScanRecord::RUNNING);
        curr.p->scanFragState = ScanFragRec::COMPLETED;
	running.release(curr);
	continue;
      case ScanFragRec::WAIT_GET_PRIMCONF:
	jam();
	continue;
      case ScanFragRec::LQH_ACTIVE:
	jam();
	break;
      default:
	jamLine(curr.p->scanFragState);
	ndbabort();
      }
      
      curr.p->startFragTimer(ctcTimer);
      curr.p->m_start_ticks = getHighResTimer();
      curr.p->scanFragState = ScanFragRec::LQH_ACTIVE;
      nextReq->senderData = curr.i;
      sendSignal(curr.p->lqhBlockref, GSN_SCAN_NEXTREQ, signal, 
		 ScanFragNextReq::SignalLength, JBB);
    }

    // Close delivered
    for(delivered.first(ptr); !ptr.isNull(); ){
      jam();
      ScanFragRecPtr curr = ptr; // Remove while iterating...
      delivered.next(ptr);

      ndbrequire(curr.p->scanFragState == ScanFragRec::DELIVERED);
      delivered.remove(curr);
      
      if (curr.p->m_scan_frag_conf_status == 0)
      {
	jam();
        running.addFirst(curr);
	curr.p->scanFragState = ScanFragRec::LQH_ACTIVE;
        curr.p->m_start_ticks = getHighResTimer();
	curr.p->startFragTimer(ctcTimer);
	nextReq->senderData = curr.i;
	sendSignal(curr.p->lqhBlockref, GSN_SCAN_NEXTREQ, signal, 
		   ScanFragNextReq::SignalLength, JBB);
      }
      else 
      {
	jam();
	curr.p->scanFragState = ScanFragRec::COMPLETED;
	curr.p->stopFragTimer();
	c_scan_frag_pool.release(curr);
      }
    }//for

    /**
     * All queued with data should be closed
     */
    for(queued.first(ptr); !ptr.isNull(); ){
      jam();
      ndbrequire(ptr.p->scanFragState == ScanFragRec::QUEUED_FOR_DELIVERY);
      ScanFragRecPtr curr = ptr; // Remove while iterating...
      queued.next(ptr);
      
      queued.remove(curr); 
      scanP->m_queued_count--;
      
      if (curr.p->m_scan_frag_conf_status == 0)
      {
	jam();
        running.addFirst(curr);
	curr.p->scanFragState = ScanFragRec::LQH_ACTIVE;
        curr.p->m_start_ticks = getHighResTimer();
	curr.p->startFragTimer(ctcTimer);
	nextReq->senderData = curr.i;
	sendSignal(curr.p->lqhBlockref, GSN_SCAN_NEXTREQ, signal, 
		   ScanFragNextReq::SignalLength, JBB);
      }
      else 
      {
	jam();
	curr.p->scanFragState = ScanFragRec::COMPLETED;
	curr.p->stopFragTimer();
	c_scan_frag_pool.release(curr);
      }
    }
  }
  close_scan_req_send_conf(signal, scanPtr);
}

void
Dbtc::close_scan_req_send_conf(Signal* signal, ScanRecordPtr scanPtr){

  jam();

  ndbrequire(scanPtr.p->m_queued_scan_frags.isEmpty());
  ndbrequire(scanPtr.p->m_delivered_scan_frags.isEmpty());
  //ndbrequire(scanPtr.p->m_running_scan_frags.isEmpty());

  if(!scanPtr.p->m_running_scan_frags.isEmpty()){
    jam();
    return;
  }

  const bool apiFail =
    (apiConnectptr.p->apiFailState != ApiConnectRecord::AFS_API_OK);
  
  if(!scanPtr.p->m_close_scan_req){
    jam();
    /**
     * The API hasn't order closing yet
     */
    return;
  }

  Uint32 ref = apiConnectptr.p->ndbapiBlockref;
  if(!apiFail && ref){
    jam();
    ScanTabConf * conf = (ScanTabConf*)&signal->theData[0];
    conf->apiConnectPtr = apiConnectptr.p->ndbapiConnect;
    conf->requestInfo = ScanTabConf::EndOfData;
    conf->transId1 = apiConnectptr.p->transid[0];
    conf->transId2 = apiConnectptr.p->transid[1];
    sendSignal(ref, GSN_SCAN_TABCONF, signal, ScanTabConf::SignalLength, JBB);
    time_track_complete_scan(scanPtr.p, refToNode(ref));
  }
  
  releaseScanResources(signal, scanPtr);
  
  if(apiFail){
    jam();
    /**
     * API has failed
     */
    handleApiFailState(signal, apiConnectptr.i);
  }
}

Dbtc::ScanRecordPtr
Dbtc::seizeScanrec(Signal* signal) {
  ScanRecordPtr scanptr;
  scanptr.i = cfirstfreeScanrec;
  ptrCheckGuard(scanptr, cscanrecFileSize, scanRecord);
  cfirstfreeScanrec = scanptr.p->nextScan;
  scanptr.p->nextScan = RNIL;
  cConcScanCount++;
  ndbrequire(scanptr.p->scanState == ScanRecord::IDLE);
  return scanptr;
}//Dbtc::seizeScanrec()

bool Dbtc::sendScanFragReq(Signal* signal,
                           ScanRecordPtr scanptr,
                           ScanFragRecPtr scanFragP)
{
  jam();
  ScanRecord* const scanP = scanptr.p;

  ScanFragLocationPtr ptr;
  Local_ScanFragLocation_list frags(m_fragLocationPool,
                                    scanP->m_fragLocations);
  ndbrequire(frags.removeFirst(ptr));

  const Uint32 fragId = ptr.p->fragId;
  const Uint32 lqhBlockRef = ptr.p->blockRef;
  scanP->scanNextFragId++;
  m_fragLocationPool.release(ptr);  //Consumed it

  const NodeId nodeId = refToNode(lqhBlockRef);
  Uint32 requestInfo = scanP->scanRequestInfo;

  ndbassert(scanFragP.p->scanFragState == ScanFragRec::IDLE);
  scanFragP.p->lqhBlockref = lqhBlockRef;
  scanFragP.p->lqhScanFragId = fragId;
  scanFragP.p->m_connectCount = getNodeInfo(nodeId).m_connectCount;

  SectionHandle sections(this);
  sections.m_ptr[0].i = scanP->scanAttrInfoPtr;
  sections.m_ptr[1].i = scanP->scanKeyInfoPtr;

  sections.m_cnt = 1; // there is always attrInfo

  if (scanP->scanKeyInfoPtr != RNIL)
  {
    jamDebug();
    sections.m_cnt = 2; // and sometimes keyinfo
  }

  if (ScanFragReq::getMultiFragFlag(scanP->scanRequestInfo))
  {
    jam();
    Uint32 *fragIds = &signal->theData[25]; // temp storage
    *(fragIds++) = scanFragP.p->lqhScanFragId;

    /**
     * The fragLocations are sorted on blockRef. 
     * Append all fragIds at same block to this SCAN_FRAGREQ.
     */
    while (frags.first(ptr) && ptr.p->blockRef == scanFragP.p->lqhBlockref)
    {
      jamDebug();
      *(fragIds++) = ptr.p->fragId;
      scanP->scanNextFragId++;
      frags.removeFirst(ptr);
      m_fragLocationPool.release(ptr);  //Consumed it
    }

    Ptr<SectionSegment> fragIdPtr;
    const Uint32 length = (fragIds-&signal->theData[25]);

    if ((ERROR_INSERTED(8116)) ||
        (ERROR_INSERTED(8117) && (rand() % 3) == 0) ||
	 !import(fragIdPtr, &signal->theData[25], length))
    { 
      jam();
      sections.clear();
      scanError(signal, scanptr, ZGET_DATAREC_ERROR);
      return false;
    }
    sections.m_ptr[sections.m_cnt++].i = fragIdPtr.i;
  } //MultiFrag

  /* Determine whether this is the last scanFragReq.
   * Handle normal scan-all-fragments and partition pruned
   * scan-one-fragment cases.
   */
  const bool isLastReq= (scanP->scanNextFragId >= scanP->scanNoFrag);
  if (isLastReq)
  {
    /* This send will release these sections, remove our
     * references to them
     */
    jamDebug();
    scanP->scanKeyInfoPtr = RNIL;
    scanP->scanAttrInfoPtr = RNIL;
  }

  getSections(sections.m_cnt, sections.m_ptr);

  ScanFragReq * const req = (ScanFragReq *)&signal->theData[0];
  ScanFragReq::setScanPrio(requestInfo, 1);
  req->tableId = scanP->scanTableref;
  req->schemaVersion = scanP->scanSchemaVersion;
  req->senderData = scanFragP.i;
  req->requestInfo = requestInfo;
  req->fragmentNoKeyLen = scanFragP.p->lqhScanFragId;
  req->resultRef = apiConnectptr.p->ndbapiBlockref;
  req->savePointId = apiConnectptr.p->currSavePointId;
  req->transId1 = apiConnectptr.p->transid[0];
  req->transId2 = apiConnectptr.p->transid[1];
  req->clientOpPtr = scanFragP.p->m_apiPtr;
  req->batch_size_rows= scanP->batch_size_rows;
  req->batch_size_bytes= scanP->batch_byte_size;

  // Encode variable part
  ndbassert(ScanFragReq::getCorrFactorFlag(requestInfo) == 0);

  const Uint32 version= getNodeInfo(nodeId).m_version;
  const bool longFragReq= ((version >= NDBD_LONG_SCANFRAGREQ) &&
                           (! ERROR_INSERTED(8070) &&
		            ! ERROR_INSERTED(8088)));
  if (likely(longFragReq))
  {
    jamDebug();
    /* Send long, possibly fragmented SCAN_FRAGREQ */

    // TODO : 
    //   1) Consider whether to adjust fragmentation threshold
    //      a) When to fragment signal vs fragment size
    //      b) Fragment size
    /* To reduce the copy burden we want to keep hold of the
     * AttrInfo and KeyInfo sections after sending them to 
     * LQH.  To do this we perform the fragmented send inline, 
     * so that all fragments are sent *now*.  This avoids any 
     * problems with the fragmented send CONTINUE 'thread' using 
     * the section while we hold or even release it.  The
     * signal receiver can still take realtime breaks when 
     * receiving.
     * 
     * Indicate to sendFirstFragment that we want to keep the
     * fragments, so it must not free them, unless this is the
     * last request in which case they can be freed.  If the
     * last request is a local send then a copy is avoided.
     */
    FragmentSendInfo fragSendInfo;

    sendFirstFragment(fragSendInfo,
                      NodeReceiverGroup(scanFragP.p->lqhBlockref),
                      GSN_SCAN_FRAGREQ,
                      signal,
                      ScanFragReq::SignalLength,
                      JBB,
                      &sections,
                      !isLastReq); // Keep sent sections unless
                                   // last send

    while (fragSendInfo.m_status != FragmentSendInfo::SendComplete)
    {
      jam();
      /* Send remaining fragments */
      sendNextSegmentedFragment(signal, fragSendInfo);
    }

    if (!isLastReq && ScanFragReq::getMultiFragFlag(requestInfo))
    {
      jamDebug();
      release(sections.m_ptr[sections.m_cnt-1]);
    }

    /* Clear handle, section deallocation handled elsewhere. */
    sections.clear();
  }
  else
  {
    jam();
    ndbassert(!ScanFragReq::getMultiFragFlag(requestInfo)); //Need 'longFragReq'

    /* Short SCANFRAGREQ with separate KeyInfo and AttrInfo trains 
     * Sent to older NDBD nodes during upgrade
     */
    Uint32 reqAttrLen = sections.m_ptr[0].sz;
    ScanFragReq::setAttrLen(req->requestInfo, reqAttrLen);
    if (sections.m_cnt > 1)
    {
      jam();
      /*
       * bug#13834481 missing shift, causing fragment not found
       * (error 1231) on 6.3 node.
       */
      req->fragmentNoKeyLen |= (sections.m_ptr[1].sz << 16);
    }
    sendSignal(scanFragP.p->lqhBlockref, GSN_SCAN_FRAGREQ, signal,
               ScanFragReq::SignalLength, JBB);
    if (sections.m_cnt > 1)
    {
      jam();
      /* Build KeyInfo train from KeyInfo long signal section */
      sendKeyInfoTrain(signal,
                       scanFragP.p->lqhBlockref,
                       scanFragP.i,
                       0, // Offset 0
                       sections.m_ptr[1].i);
    }
    
    if(ERROR_INSERTED(8035))
      globalTransporterRegistry.performSend();

    if (!ERROR_INSERTED(8088))
    {
      ndbrequire(sendAttrInfoTrain(signal,
                                   scanFragP.p->lqhBlockref,
                                   scanFragP.i,
                                   0, // Offset 0
                                   sections.m_ptr[0].i));
    }
        
    if(ERROR_INSERTED(8035))
      globalTransporterRegistry.performSend();

    if (isLastReq)
    {
      /* Free the sections here */
      releaseSections(sections);
    }
    else
    {
      sections.clear();
    }
  }

  if (ERROR_INSERTED(8088))
  {
    signal->theData[0] = 9999;
    sendSignalWithDelay(CMVMI_REF, GSN_NDB_TAMPER, signal, 100, 1);
  }

  scanFragP.p->scanFragState = ScanFragRec::LQH_ACTIVE;
  scanFragP.p->startFragTimer(ctcTimer);
  scanFragP.p->m_start_ticks = getHighResTimer();
  updateBuddyTimer(apiConnectptr);
  return true;
}//Dbtc::sendScanFragReq()


void Dbtc::sendScanTabConf(Signal* signal, ScanRecordPtr scanPtr) {
  jamDebug();
  Uint32* ops = signal->getDataPtrSend()+4;
  Uint32 op_count = scanPtr.p->m_queued_count;

  Uint32 words_per_op = 4;
  const Uint32 ref = apiConnectptr.p->ndbapiBlockref;
  if (!scanPtr.p->m_4word_conf)
  {
    jamDebug();
    words_per_op = 3;
  }

  if (4 + words_per_op * op_count > 25)
  {
    jamDebug();
    ops += 21;
  }
  
  int left = scanPtr.p->scanNoFrag - scanPtr.p->scanNextFragId;
  Uint32 booked = scanPtr.p->m_booked_fragments_count;
  
  ScanTabConf * conf = (ScanTabConf*)&signal->theData[0];
  conf->apiConnectPtr = apiConnectptr.p->ndbapiConnect;
  conf->requestInfo = op_count;
  conf->transId1 = apiConnectptr.p->transid[0];
  conf->transId2 = apiConnectptr.p->transid[1];
  ScanFragRecPtr ptr;
  {
    Local_ScanFragRec_dllist queued(c_scan_frag_pool, scanPtr.p->m_queued_scan_frags);
    Local_ScanFragRec_dllist delivered(c_scan_frag_pool, scanPtr.p->m_delivered_scan_frags);
    for(queued.first(ptr); !ptr.isNull(); ){
      ndbrequire(ptr.p->scanFragState == ScanFragRec::QUEUED_FOR_DELIVERY);
      ScanFragRecPtr curr = ptr; // Remove while iterating...
      queued.next(ptr);
      
      bool done = curr.p->m_scan_frag_conf_status && (left <= (int)booked);
      if(curr.p->m_scan_frag_conf_status)
	booked++;
      
      * ops++ = curr.p->m_apiPtr;
      * ops++ = done ? RNIL : curr.i;
      if (words_per_op == 4)
      {
        * ops++ = curr.p->m_ops;
        * ops++ = curr.p->m_totalLen;
      }
      else
      {
        * ops++ = (curr.p->m_totalLen << 10) + curr.p->m_ops;
      }

      curr.p->stopFragTimer();
      queued.remove(curr); 
      if(!done){
        delivered.addFirst(curr);
	curr.p->scanFragState = ScanFragRec::DELIVERED;
      } else {
	curr.p->scanFragState = ScanFragRec::COMPLETED;
	c_scan_frag_pool.release(curr);
      }
    }
  }
  
  bool release = false;
  scanPtr.p->m_booked_fragments_count = booked;
  if(scanPtr.p->m_delivered_scan_frags.isEmpty() && 
     scanPtr.p->m_running_scan_frags.isEmpty())
  {
    jam();
    release = true;
    conf->requestInfo = op_count | ScanTabConf::EndOfData;    
  }
  else
  {
    if (scanPtr.p->m_running_scan_frags.isEmpty())
    {
      jam();
      /**
       * All scan frags delivered...waiting for API
       */
      setApiConTimer(apiConnectptr.i, ctcTimer, __LINE__);
    }
    else
    {
      jam();
    }
  }
  
  if (4 + words_per_op * op_count > 25)
  {
    jamDebug();
    LinearSectionPtr ptr[3];
    ptr[0].p = signal->getDataPtrSend()+25;
    ptr[0].sz = words_per_op * op_count;
    sendSignal(ref, GSN_SCAN_TABCONF, signal,
               ScanTabConf::SignalLength, JBB, ptr, 1);
  }
  else
  {
    jamDebug();
    sendSignal(ref, GSN_SCAN_TABCONF, signal,
	       ScanTabConf::SignalLength + words_per_op * op_count, JBB);
  }
  scanPtr.p->m_queued_count = 0;

  if (release)
  {
    jamDebug();
    time_track_complete_scan(scanPtr.p, refToNode(ref));
    releaseScanResources(signal, scanPtr);
  }

}//Dbtc::sendScanTabConf()


void Dbtc::gcpTcfinished(Signal* signal, Uint64 gci)
{
  if (ERROR_INSERTED(8098))
  {
    if (cmasterNodeId == getOwnNodeId())
    {
      bool multi = ((gci&1) == 1);
      bool kill_me = ((gci&3) == 1);
      /* Kill multi 'assumes' NoOfReplicas=2, and skipping one
       * live node at a time avoids taking out a nodegroup
       */
      ndbout_c("TC killing multi : %u  me : %u", multi, kill_me);
      Uint32 start = getOwnNodeId();
      bool skip = !kill_me;
      Uint32 pos = start;
      do
      {
        NodeInfo ni = getNodeInfo(pos);
        if ((ni.getType() == NODE_TYPE_DB) &&
            ni.m_connected)
        {
          /* Found a db node... */
          if (!skip)
          {
            ndbout_c("TC : Killing node %u", pos);
            signal->theData[0] = 9999;
            sendSignal(numberToRef(CMVMI, pos), GSN_DUMP_STATE_ORD, signal,
                       1, JBA);
            if (!multi)
              break;
            skip = true;
          }
          else
          {
            ndbout_c("TC : Skipping node %u", pos);
            skip = false;
          }
        }

        pos++;
        if (pos == MAX_NDB_NODES)
          pos = 1;
      } while (pos != start);
    }

    /* Keep delaying GCP_TCFINISHED, but don't kill anymore */
    SET_ERROR_INSERT_VALUE(8099);
  }

  GCPTCFinished* conf = (GCPTCFinished*)signal->getDataPtrSend();
  conf->senderData = c_gcp_data;
  conf->gci_hi = Uint32(gci >> 32);
  conf->gci_lo = Uint32(gci);
  conf->tcFailNo = cfailure_nr; /* Indicate highest handled failno in GCP */

  if (ERROR_INSERTED(8099))
  {
    /* Slow it down */
    ndbout_c("TC : Sending delayed GCP_TCFINISHED (%u/%u), failNo %u to local DIH(%x)",
             conf->gci_hi, conf->gci_lo, cfailure_nr, cdihblockref);
    sendSignalWithDelay(c_gcp_ref, GSN_GCP_TCFINISHED, signal,
                        2000, GCPTCFinished::SignalLength);
    return;
  }

  sendSignal(c_gcp_ref, GSN_GCP_TCFINISHED, signal,
             GCPTCFinished::SignalLength, JBB);
}//Dbtc::gcpTcfinished()

void Dbtc::initApiConnect(Signal* signal) 
{
  Uint32 tiacTmp;
  Uint32 guard4;

  tiacTmp = capiConnectFilesize / 3;
  ndbrequire(tiacTmp > 0);
  guard4 = tiacTmp + 1;
  for (cachePtr.i = 0; cachePtr.i < guard4; cachePtr.i++) {
    refresh_watch_dog();
    ptrAss(cachePtr, cacheRecord);
    cachePtr.p->nextCacheRec = cachePtr.i + 1;
  }//for
  cachePtr.i = tiacTmp;
  ptrCheckGuard(cachePtr, ccacheFilesize, cacheRecord);
  cachePtr.p->nextCacheRec = RNIL;
  cfirstfreeCacheRec = 0;

  guard4 = tiacTmp - 1;
  for (apiConnectptr.i = 0; apiConnectptr.i <= guard4; apiConnectptr.i++) {
    refresh_watch_dog();
    jam();
    ptrAss(apiConnectptr, apiConnectRecord);
    apiConnectptr.p->apiConnectstate = CS_DISCONNECTED;
    apiConnectptr.p->apiFailState = ApiConnectRecord::AFS_API_OK;
    setApiConTimer(apiConnectptr.i, 0, __LINE__);
    apiConnectptr.p->takeOverRec = (Uint8)Z8NIL;
    apiConnectptr.p->cachePtr = RNIL;
    apiConnectptr.p->nextApiConnect = apiConnectptr.i + 1;
    apiConnectptr.p->ndbapiBlockref = 0xFFFFFFFF; // Invalid ref
    apiConnectptr.p->commitAckMarker = RNIL;
    apiConnectptr.p->num_commit_ack_markers = 0;
    apiConnectptr.p->firstTcConnect = RNIL;
    apiConnectptr.p->lastTcConnect = RNIL;
    apiConnectptr.p->m_flags = 0;
    apiConnectptr.p->m_special_op_flags = 0;
    apiConnectptr.p->accumulatingIndexOp = RNIL;
    apiConnectptr.p->executingIndexOp = RNIL;
    apiConnectptr.p->buddyPtr = RNIL;
    apiConnectptr.p->currSavePointId = 0;
    apiConnectptr.p->m_transaction_nodes.clear();
    apiConnectptr.p->singleUserMode = 0;
    apiConnectptr.p->apiCopyRecord = RNIL;
  }//for
  apiConnectptr.i = tiacTmp - 1;
  ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
  apiConnectptr.p->nextApiConnect = RNIL;
  cfirstfreeApiConnect = 0;
  guard4 = (2 * tiacTmp) - 1;
  for (apiConnectptr.i = tiacTmp; apiConnectptr.i <= guard4; apiConnectptr.i++)
    {
      refresh_watch_dog();
      jam();
      ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
      apiConnectptr.p->apiConnectstate = CS_RESTART;
      apiConnectptr.p->apiFailState = ApiConnectRecord::AFS_API_OK;
      setApiConTimer(apiConnectptr.i, 0, __LINE__);
      apiConnectptr.p->takeOverRec = (Uint8)Z8NIL;
      apiConnectptr.p->cachePtr = RNIL;
      apiConnectptr.p->nextApiConnect = apiConnectptr.i + 1;
      apiConnectptr.p->ndbapiBlockref = 0xFFFFFFFF; // Invalid ref
      apiConnectptr.p->commitAckMarker = RNIL;
      apiConnectptr.p->num_commit_ack_markers = 0;
      apiConnectptr.p->firstTcConnect = RNIL;
      apiConnectptr.p->lastTcConnect = RNIL;
      apiConnectptr.p->m_flags = 0;
      apiConnectptr.p->m_special_op_flags = 0;
      apiConnectptr.p->accumulatingIndexOp = RNIL;
      apiConnectptr.p->executingIndexOp = RNIL;
      apiConnectptr.p->buddyPtr = RNIL;
      apiConnectptr.p->currSavePointId = 0;
      apiConnectptr.p->m_transaction_nodes.clear();
      apiConnectptr.p->singleUserMode = 0;
      apiConnectptr.p->apiCopyRecord = RNIL;
    }//for
  apiConnectptr.i = (2 * tiacTmp) - 1;
  ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
  apiConnectptr.p->nextApiConnect = RNIL;
  cfirstfreeApiConnectCopy = tiacTmp;
  guard4 = (3 * tiacTmp) - 1;
  for (apiConnectptr.i = 2 * tiacTmp; apiConnectptr.i <= guard4; 
       apiConnectptr.i++) {
    refresh_watch_dog();
    jam();
    ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
    setApiConTimer(apiConnectptr.i, 0, __LINE__);
    apiConnectptr.p->apiFailState = ApiConnectRecord::AFS_API_OK;
    apiConnectptr.p->apiConnectstate = CS_RESTART;
    apiConnectptr.p->takeOverRec = (Uint8)Z8NIL;
    apiConnectptr.p->cachePtr = RNIL;
    apiConnectptr.p->nextApiConnect = apiConnectptr.i + 1;
    apiConnectptr.p->ndbapiBlockref = 0xFFFFFFFF; // Invalid ref
    apiConnectptr.p->commitAckMarker = RNIL;
    apiConnectptr.p->num_commit_ack_markers = 0;
    apiConnectptr.p->firstTcConnect = RNIL;
    apiConnectptr.p->lastTcConnect = RNIL;
    apiConnectptr.p->m_flags = 0;
    apiConnectptr.p->m_special_op_flags = 0;
    apiConnectptr.p->accumulatingIndexOp = RNIL;
    apiConnectptr.p->executingIndexOp = RNIL;
    apiConnectptr.p->buddyPtr = RNIL;
    apiConnectptr.p->currSavePointId = 0;
    apiConnectptr.p->m_transaction_nodes.clear();
    apiConnectptr.p->singleUserMode = 0;
    apiConnectptr.p->apiCopyRecord = RNIL;
  }//for
  apiConnectptr.i = (3 * tiacTmp) - 1;
  ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
  apiConnectptr.p->nextApiConnect = RNIL;
  cfirstfreeApiConnectFail = 2 * tiacTmp;
  cfirstApiConnectPREPARE_TO_COMMIT = RNIL;
  clastApiConnectPREPARE_TO_COMMIT = RNIL;
}//Dbtc::initApiConnect()

void Dbtc::initgcp(Signal* signal) 
{
  Ptr<GcpRecord> gcpPtr;
  ndbrequire(cgcpFilesize > 0);
  for (gcpPtr.i = 0; gcpPtr.i < cgcpFilesize; gcpPtr.i++) {
    ptrAss(gcpPtr, gcpRecord);
    gcpPtr.p->nextGcp = gcpPtr.i + 1;
  }//for
  gcpPtr.i = cgcpFilesize - 1;
  ptrCheckGuard(gcpPtr, cgcpFilesize, gcpRecord);
  gcpPtr.p->nextGcp = RNIL;
  cfirstfreeGcp = 0;
  cfirstgcp = RNIL;
  clastgcp = RNIL;
}//Dbtc::initgcp()

void Dbtc::inithost(Signal* signal) 
{
  cpackedListIndex = 0;
  ndbrequire(chostFilesize > 0);
  for (hostptr.i = 0; hostptr.i < chostFilesize; hostptr.i++) {
    jam();
    ptrAss(hostptr, hostRecord);
    hostptr.p->hostStatus = HS_DEAD;
    hostptr.p->inPackedList = false;
    hostptr.p->lqhTransStatus = LTS_IDLE;
    struct PackedWordsContainer * containerTCKEYCONF =
      &hostptr.p->packTCKEYCONF;
    containerTCKEYCONF->noOfPackedWords = 0;
    for (Uint32 i = 0; i < NDB_ARRAY_SIZE(hostptr.p->lqh_pack); i++)
    {
      struct PackedWordsContainer * container = &hostptr.p->lqh_pack[i];
      container->noOfPackedWords = 0;
      container->hostBlockRef = numberToRef(DBLQH, i, hostptr.i);
    }
    hostptr.p->m_nf_bits = 0;
  }//for
  c_alive_nodes.clear();
}//Dbtc::inithost()

void Dbtc::initialiseRecordsLab(Signal* signal, UintR Tdata0, 
				Uint32 retRef, Uint32 retData) 
{
  switch (Tdata0) {
  case 0:
    jam();
    initApiConnect(signal);
    break;
  case 1:
    jam();
    // UNUSED Free to initialise something
    break;
  case 2:
    jam();
    // UNUSED Free to initialise something
    break;
  case 3:
    jam();
    initgcp(signal);
    break;
  case 4:
    jam();
    inithost(signal);
    break;
  case 5:
    jam();
    // UNUSED Free to initialise something
    break;
  case 6:
    jam();
    initTable(signal);
    break;
  case 7:
    jam();
    initialiseScanrec(signal);
    break;
  case 8:
    jam();
    initialiseScanOprec(signal);
    break;
  case 9:
    jam();
    initialiseScanFragrec(signal);
    break;
  case 10:
    jam();
    initialiseTcConnect(signal);
    break;
  case 11:
    jam();
    initTcFail(signal);

    {
      ReadConfigConf * conf = (ReadConfigConf*)signal->getDataPtrSend();
      conf->senderRef = reference();
      conf->senderData = retData;
      sendSignal(retRef, GSN_READ_CONFIG_CONF, signal, 
		 ReadConfigConf::SignalLength, JBB);
    }
    return;
    break;
  default:
    jam();
    systemErrorLab(signal, __LINE__);
    return;
    break;
  }//switch

  signal->theData[0] = TcContinueB::ZINITIALISE_RECORDS;
  signal->theData[1] = Tdata0 + 1;
  signal->theData[2] = 0;
  signal->theData[3] = retRef;
  signal->theData[4] = retData;
  sendSignal(reference(), GSN_CONTINUEB, signal, 5, JBB);
}

/* ========================================================================= */
/* =======                       INITIALISE_SCANREC                  ======= */
/*                                                                           */
/* ========================================================================= */
void Dbtc::initialiseScanrec(Signal* signal) 
{
  ScanRecordPtr scanptr;
  ndbrequire(cscanrecFileSize > 0);
  for (scanptr.i = 0; scanptr.i < cscanrecFileSize; scanptr.i++) {
    refresh_watch_dog();
    jam();
    ptrAss(scanptr, scanRecord);
    new (scanptr.p) ScanRecord();
    scanptr.p->scanState = ScanRecord::IDLE;
    scanptr.p->scanApiRec = RNIL;
    scanptr.p->nextScan = scanptr.i + 1;
  }//for
  scanptr.i = cscanrecFileSize - 1;
  ptrAss(scanptr, scanRecord);
  scanptr.p->nextScan = RNIL;
  cfirstfreeScanrec = 0;
  cConcScanCount = 0;
}//Dbtc::initialiseScanrec()

void Dbtc::initialiseScanFragrec(Signal* signal) 
{
}//Dbtc::initialiseScanFragrec()

void Dbtc::initialiseScanOprec(Signal* signal) 
{
}//Dbtc::initialiseScanOprec()

void Dbtc::initTable(Signal* signal) 
{

  ndbrequire(ctabrecFilesize > 0);
  for (tabptr.i = 0; tabptr.i < ctabrecFilesize; tabptr.i++) {
    refresh_watch_dog();
    ptrAss(tabptr, tableRecord);
    tabptr.p->currentSchemaVersion = 0;
    tabptr.p->m_flags = 0;
    tabptr.p->set_storedTable(true);
    tabptr.p->tableType = 0;
    tabptr.p->set_enabled(false);
    tabptr.p->set_dropping(false);
    tabptr.p->noOfKeyAttr = 0;
    tabptr.p->hasCharAttr = 0;
    tabptr.p->noOfDistrKeys = 0;
    tabptr.p->hasVarKeys = 0;
  }//for
}//Dbtc::initTable()

void Dbtc::initialiseTcConnect(Signal* signal) 
{
  ndbrequire(ctcConnectFilesize >= 2);

  // Place half of tcConnectptr's in cfirstfreeTcConnectFail list
  Uint32 titcTmp = ctcConnectFilesize / 2;
  for (tcConnectptr.i = 0; tcConnectptr.i < titcTmp; tcConnectptr.i++) {
    refresh_watch_dog();
    jam();
    ptrAss(tcConnectptr, tcConnectRecord);
    new (tcConnectptr.p) TcConnectRecord();
    tcConnectptr.p->tcConnectstate = OS_CONNECTED;
    tcConnectptr.p->apiConnect = RNIL;
    tcConnectptr.p->noOfNodes = 0;
    tcConnectptr.p->nextTcConnect = tcConnectptr.i + 1;
    tcConnectptr.p->commitAckMarker = RNIL;
  }//for
  tcConnectptr.i = titcTmp - 1;
  ptrAss(tcConnectptr, tcConnectRecord);
  tcConnectptr.p->nextTcConnect = RNIL;
  cfirstfreeTcConnectFail = 0;

  // Place other half in cfirstfreeTcConnect list
  for (tcConnectptr.i = titcTmp; tcConnectptr.i < ctcConnectFilesize; 
       tcConnectptr.i++) {
    refresh_watch_dog();
    jam();
    ptrAss(tcConnectptr, tcConnectRecord);
    new (tcConnectptr.p) TcConnectRecord();
    tcConnectptr.p->tcConnectstate = OS_CONNECTED;
    tcConnectptr.p->apiConnect = RNIL;
    tcConnectptr.p->noOfNodes = 0;
    tcConnectptr.p->nextTcConnect = tcConnectptr.i + 1;
    tcConnectptr.p->commitAckMarker = RNIL;
  }//for
  tcConnectptr.i = ctcConnectFilesize - 1;
  ptrAss(tcConnectptr, tcConnectRecord);
  tcConnectptr.p->nextTcConnect = RNIL;
  cfirstfreeTcConnect = titcTmp;
  c_counters.cconcurrentOp = 0;
}//Dbtc::initialiseTcConnect()

/* ------------------------------------------------------------------------- */
/* ----     LINK A GLOBAL CHECKPOINT RECORD INTO THE LIST WITH TRANSACTIONS  */
/*          WAITING FOR COMPLETION.                                          */
/* ------------------------------------------------------------------------- */
void Dbtc::linkGciInGcilist(Ptr<GcpRecord> gcpPtr)
{
  GcpRecordPtr tmpGcpPointer;
  if (cfirstgcp == RNIL) {
    jam();
    cfirstgcp = gcpPtr.i;
  } else {
    jam();
    tmpGcpPointer.i = clastgcp;
    ptrCheckGuard(tmpGcpPointer, cgcpFilesize, gcpRecord);
    tmpGcpPointer.p->nextGcp = gcpPtr.i;
  }//if
  clastgcp = gcpPtr.i;
}//Dbtc::linkGciInGcilist()

/* ------------------------------------------------------------------------- */
/* ------- LINK A TC CONNECT RECORD INTO THE API LIST OF TC CONNECTIONS  --- */
/* ------------------------------------------------------------------------- */
void Dbtc::linkTcInConnectionlist(Signal* signal) 
{
  /* POINTER FOR THE CONNECT_RECORD */
  TcConnectRecordPtr ltcTcConnectptr;

  tcConnectptr.p->nextTcConnect = RNIL;
  ltcTcConnectptr.i = apiConnectptr.p->lastTcConnect;
  apiConnectptr.p->lastTcConnect = tcConnectptr.i;
  if (ltcTcConnectptr.i == RNIL) {
    jam();
    apiConnectptr.p->firstTcConnect = tcConnectptr.i;
  } else {
    jam();
    ptrCheckGuard(ltcTcConnectptr, ctcConnectFilesize, tcConnectRecord);
    ltcTcConnectptr.p->nextTcConnect = tcConnectptr.i;
  }//if
}//Dbtc::linkTcInConnectionlist()

/*---------------------------------------------------------------------------*/
/*                    RELEASE_ABORT_RESOURCES                                */
/* THIS CODE RELEASES ALL RESOURCES AFTER AN ABORT OF A TRANSACTION AND ALSO */
/* SENDS THE ABORT DECISION TO THE APPLICATION.                              */
/*---------------------------------------------------------------------------*/
void Dbtc::releaseAbortResources(Signal* signal) 
{
  TcConnectRecordPtr rarTcConnectptr;

  c_counters.cabortCount++;
  if (apiConnectptr.p->apiCopyRecord != RNIL)
  {
    // Put apiCopyRecord back in free list.
    jam();
    ApiConnectRecordPtr copyPtr;
    copyPtr.i = apiConnectptr.p->apiCopyRecord;
    ptrCheckGuard(copyPtr, capiConnectFilesize, apiConnectRecord);
    ndbassert(copyPtr.p->apiCopyRecord == RNIL);
    ndbassert(copyPtr.p->nextApiConnect == RNIL);
    copyPtr.p->nextApiConnect = cfirstfreeApiConnectCopy;
    cfirstfreeApiConnectCopy = copyPtr.i;
    apiConnectptr.p->apiCopyRecord = RNIL;
  }
  if (apiConnectptr.p->cachePtr != RNIL) {
    cachePtr.i = apiConnectptr.p->cachePtr;
    ptrCheckGuard(cachePtr, ccacheFilesize, cacheRecord);
    releaseAttrinfo();
    releaseKeys();
  }//if
  tcConnectptr.i = apiConnectptr.p->firstTcConnect;
  while (tcConnectptr.i != RNIL) {
    jam();
    ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
    // Clear any markers that have not already been cleared
    clearCommitAckMarker(apiConnectptr.p, tcConnectptr.p);
    rarTcConnectptr.i = tcConnectptr.p->nextTcConnect;
    releaseTcCon();
    tcConnectptr.i = rarTcConnectptr.i;
  }//while

  ndbrequire(apiConnectptr.p->num_commit_ack_markers == 0);
  releaseMarker(apiConnectptr.p);

  apiConnectptr.p->firstTcConnect = RNIL;
  apiConnectptr.p->lastTcConnect = RNIL;
  apiConnectptr.p->m_transaction_nodes.clear();
  apiConnectptr.p->singleUserMode = 0;

  // MASV let state be CS_ABORTING until all 
  // signals in the "air" have been received. Reset to CS_CONNECTED
  // will be done when a TCKEYREQ with start flag is received
  // or releaseApiCon is called
  // apiConnectptr.p->apiConnectstate = CS_CONNECTED;
  apiConnectptr.p->apiConnectstate = CS_ABORTING;
  apiConnectptr.p->abortState = AS_IDLE;
  releaseAllSeizedIndexOperations(apiConnectptr.p);
  releaseFiredTriggerData(&apiConnectptr.p->theFiredTriggers);

  if (tc_testbit(apiConnectptr.p->m_flags, ApiConnectRecord::TF_EXEC_FLAG) ||
      apiConnectptr.p->apiFailState != ApiConnectRecord::AFS_API_OK)
  {
    jam();
    bool ok = false;
    Uint32 blockRef = apiConnectptr.p->ndbapiBlockref;
    ReturnSignal ret = apiConnectptr.p->returnsignal;
    apiConnectptr.p->returnsignal = RS_NO_RETURN;
    tc_clearbit(apiConnectptr.p->m_flags, ApiConnectRecord::TF_EXEC_FLAG);
    switch(ret){
    case RS_TCROLLBACKCONF:
      jam();
      ok = true;
      signal->theData[0] = apiConnectptr.p->ndbapiConnect;
      signal->theData[1] = apiConnectptr.p->transid[0];
      signal->theData[2] = apiConnectptr.p->transid[1];
      sendSignal(blockRef, GSN_TCROLLBACKCONF, signal, 3, JBB);
      break;
    case RS_TCROLLBACKREP:{
      jam();
      ok = true;
#ifdef ERROR_INSERT
      if (ERROR_INSERTED(8101))
      {
        char buf[128];
        BaseString::snprintf(buf, sizeof(buf), "Sending CONTINUEB:ZDEBUG_DELAY_TCROLLBACKREP");
        warningEvent("%s", buf);

        jam();
        signal->theData[0] = TcContinueB::ZDEBUG_DELAY_TCROLLBACKREP;
        signal->theData[1] = apiConnectptr.p->ndbapiConnect;
        signal->theData[2] = apiConnectptr.p->transid[0];
        signal->theData[3] = apiConnectptr.p->transid[1];
        signal->theData[4] = apiConnectptr.p->returncode;
        signal->theData[5] = apiConnectptr.p->errorData;
        signal->theData[6] = blockRef;
        
        sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 7);
        break;
      }
#endif      
      TcRollbackRep * const tcRollbackRep = 
	(TcRollbackRep *) signal->getDataPtr();
      
      tcRollbackRep->connectPtr = apiConnectptr.p->ndbapiConnect;
      tcRollbackRep->transId[0] = apiConnectptr.p->transid[0];
      tcRollbackRep->transId[1] = apiConnectptr.p->transid[1];
      tcRollbackRep->returnCode = apiConnectptr.p->returncode;
      tcRollbackRep->errorData = apiConnectptr.p->errorData;
      sendSignal(blockRef, GSN_TCROLLBACKREP, signal, 
		 TcRollbackRep::SignalLength, JBB);
    }
      break;
    case RS_NO_RETURN:
      jam();
      ok = true;
      break;
    case RS_TCKEYCONF:
    case RS_TC_COMMITCONF:
      break;
    }    
    if(!ok){
      jam();
      ndbout_c("returnsignal = %d", apiConnectptr.p->returnsignal);
      sendSystemError(signal, __LINE__);
    }//if

  }
  setApiConTimer(apiConnectptr.i, 0, 
		 100000+c_apiConTimer_line[apiConnectptr.i]);
  time_track_complete_transaction_error(apiConnectptr.p);
  if (apiConnectptr.p->apiFailState != ApiConnectRecord::AFS_API_OK)
  {
    jam();
    handleApiFailState(signal, apiConnectptr.i);
    return;
  }//if
}//Dbtc::releaseAbortResources()

void Dbtc::releaseApiCon(Signal* signal, UintR TapiConnectPtr) 
{
  ApiConnectRecordPtr TlocalApiConnectptr;

  TlocalApiConnectptr.i = TapiConnectPtr;
  ptrCheckGuard(TlocalApiConnectptr, capiConnectFilesize, apiConnectRecord);
  ndbassert(TlocalApiConnectptr.p->apiCopyRecord == RNIL);
  ndbassert(TlocalApiConnectptr.p->nextApiConnect == RNIL);
  TlocalApiConnectptr.p->nextApiConnect = cfirstfreeApiConnect;
  cfirstfreeApiConnect = TlocalApiConnectptr.i;
  setApiConTimer(TlocalApiConnectptr.i, 0, __LINE__);
  TlocalApiConnectptr.p->apiConnectstate = CS_DISCONNECTED;
  ndbassert(TlocalApiConnectptr.p->m_transaction_nodes.isclear());
  ndbassert(TlocalApiConnectptr.p->apiScanRec == RNIL);
  /* ndbassert(TlocalApiConnectptr.p->cascading_scans_count == 0);
   * Not a valid assert, as we can abort a transaction, and release a connection
   * while triggered cascading scans are still in-flight
   */
  TlocalApiConnectptr.p->cascading_scans_count = 0;
  TlocalApiConnectptr.p->ndbapiBlockref = 0;
  TlocalApiConnectptr.p->transid[0] = 0;
  TlocalApiConnectptr.p->transid[1] = 0;
}//Dbtc::releaseApiCon()

void Dbtc::releaseApiConnectFail(Signal* signal) 
{
  apiConnectptr.p->apiConnectstate = CS_RESTART;
  apiConnectptr.p->takeOverRec = (Uint8)Z8NIL;
  setApiConTimer(apiConnectptr.i, 0, __LINE__);
  apiConnectptr.p->nextApiConnect = cfirstfreeApiConnectFail;
  cfirstfreeApiConnectFail = apiConnectptr.i;
  ndbrequire(apiConnectptr.p->commitAckMarker == RNIL);
}//Dbtc::releaseApiConnectFail()

void Dbtc::releaseKeys() 
{
  Uint32 keyInfoSectionI= cachePtr.p->keyInfoSectionI;

  /* Release KeyInfo section if there is one */
  releaseSection(keyInfoSectionI);
  cachePtr.p->keyInfoSectionI= RNIL;

}//Dbtc::releaseKeys()

void Dbtc::releaseTcConnectFail(Signal* signal) 
{
  ptrGuard(tcConnectptr);
  tcConnectptr.p->nextTcConnect = cfirstfreeTcConnectFail;
  tcConnectptr.p->tcConnectstate = OS_CONNECTED;
  cfirstfreeTcConnectFail = tcConnectptr.i;
}//Dbtc::releaseTcConnectFail()

void Dbtc::seizeApiConnect(Signal* signal) 
{
  if (cfirstfreeApiConnect != RNIL) {
    jam();
    terrorCode = ZOK;
    apiConnectptr.i = cfirstfreeApiConnect;     /* ASSIGN A FREE RECORD FROM */
    ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
    cfirstfreeApiConnect = apiConnectptr.p->nextApiConnect;
    apiConnectptr.p->nextApiConnect = RNIL;
    setApiConTimer(apiConnectptr.i, 0, __LINE__);
    apiConnectptr.p->apiConnectstate = CS_CONNECTED; /* STATE OF CONNECTION */
    tc_clearbit(apiConnectptr.p->m_flags,
                ApiConnectRecord::TF_TRIGGER_PENDING);
    apiConnectptr.p->m_special_op_flags = 0;
  } else {
    jam();
    terrorCode = ZNO_FREE_API_CONNECTION;
  }//if
}//Dbtc::seizeApiConnect()

void Dbtc::seizeApiConnectFail(Signal* signal) 
{
  apiConnectptr.i = cfirstfreeApiConnectFail;
  ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
  cfirstfreeApiConnectFail = apiConnectptr.p->nextApiConnect;
  apiConnectptr.p->nextApiConnect = RNIL;
}//Dbtc::seizeApiConnectFail()

void Dbtc::seizeTcConnect(Signal* signal) 
{
  tcConnectptr.i = cfirstfreeTcConnect;
  ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
  cfirstfreeTcConnect = tcConnectptr.p->nextTcConnect;
  c_counters.cconcurrentOp++;
  tcConnectptr.p->m_special_op_flags = 0;
  tcConnectptr.p->tcConnectstate = OS_ABORTING;
  tcConnectptr.p->noOfNodes = 0;
}//Dbtc::seizeTcConnect()

void Dbtc::seizeTcConnectFail(Signal* signal) 
{
  tcConnectptr.i = cfirstfreeTcConnectFail;
  ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
  cfirstfreeTcConnectFail = tcConnectptr.p->nextTcConnect;
}//Dbtc::seizeTcConnectFail()

/**
 * sendAttrInfoTrain
 * This method sends an ATTRINFO signal train using AttrInfo
 * from the section passed, starting at the supplied offset
 */
bool Dbtc::sendAttrInfoTrain(Signal* signal,
                             UintR TBRef,
                             Uint32 connectPtr,
                             Uint32 offset,
                             Uint32 attrInfoIVal)
{
  ApiConnectRecord * const regApiPtr = apiConnectptr.p;

  ndbassert( attrInfoIVal != RNIL );
  SectionReader attrInfoReader(attrInfoIVal, getSectionSegmentPool());  
  Uint32 attrInfoLength= attrInfoReader.getSize();
  
  ndbassert( offset < attrInfoLength );
  if (unlikely(! attrInfoReader.step( offset )))
    return false;
  attrInfoLength-= offset;

  signal->theData[0] = connectPtr;
  signal->theData[1] = regApiPtr->transid[0];
  signal->theData[2] = regApiPtr->transid[1];

  while (attrInfoLength != 0)
  {
    Uint32 dataInSignal= MIN(AttrInfo::DataLength, attrInfoLength);

    if (unlikely(! attrInfoReader.getWords(&signal->theData[3], 
                                           dataInSignal)))
      return false;

    sendSignal(TBRef, GSN_ATTRINFO, signal, 
               AttrInfo::HeaderLength + dataInSignal, JBB);
    
    attrInfoLength-= dataInSignal;
  }
  return true;
} //Dbtc::sendAttrInfoTrain()

void Dbtc::sendContinueTimeOutControl(Signal* signal, Uint32 TapiConPtr) 
{
  signal->theData[0] = TcContinueB::ZCONTINUE_TIME_OUT_CONTROL;
  signal->theData[1] = TapiConPtr;
  sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
}//Dbtc::sendContinueTimeOutControl()

void Dbtc::sendSystemError(Signal* signal, int line) 
{
  progError(line, NDBD_EXIT_NDBREQUIRE);
}//Dbtc::sendSystemError()

/* ========================================================================= */
/* -------             LINK ACTUAL GCP OUT OF LIST                   ------- */
/* ------------------------------------------------------------------------- */
void Dbtc::unlinkGcp(Ptr<GcpRecord> tmpGcpPtr)
{
  ndbrequire(cfirstgcp == tmpGcpPtr.i);

  cfirstgcp = tmpGcpPtr.p->nextGcp;
  if (tmpGcpPtr.i == clastgcp) {
    jam();
    clastgcp = RNIL;
  }//if

  tmpGcpPtr.p->nextGcp = cfirstfreeGcp;
  cfirstfreeGcp = tmpGcpPtr.i;
}//Dbtc::unlinkGcp()

void
Dbtc::execDUMP_STATE_ORD(Signal* signal)
{
  const Uint32 MAX_RECORDS_AT_A_TIME = 16;

  jamEntry();
  DumpStateOrd * const dumpState = (DumpStateOrd *)&signal->theData[0];
  Uint32 arg = signal->theData[0];

  // Dump set of ScanFragRecs
  if (dumpState->args[0] == DumpStateOrd::TcDumpSetOfScanFragRec){
    /**
     * DUMP 2500 12 10 1 1
     * Prints ScanFrag records 12 through 21 in instance 1.
     * The last parameter indicates whether to only print active instances.
     * If the parameter is missing it is set to 1 indicating printing only
     * active instances.
     *
     * Output to cluster log.
     */
    jam();
    Uint32 recordNo = 0;
    Uint32 numRecords = 0;
    Uint32 instanceId = 0;
    Uint32 includeOnlyActive = 1;

    if (signal->getLength() >= 4)
    {
      recordNo = dumpState->args[1];
      numRecords = dumpState->args[2];
      instanceId = dumpState->args[3];
      if (signal->getLength() >= 5)
      {
        includeOnlyActive = dumpState->args[4];
      }
    }
    else
    {
      return;
    }
    if (instance() != instanceId)
    {
      return;
    }
    if (numRecords > MAX_RECORDS_AT_A_TIME)
    {
      numRecords = MAX_RECORDS_AT_A_TIME;
    }
    if (recordNo >= cscanFragrecFileSize)
    {
      return;
    }
    
    ScanFragRecPtr sfp;
    sfp.i = recordNo;
    c_scan_frag_pool.getPtr(sfp);
    if (sfp.p->scanFragState != ScanFragRec::COMPLETED ||
        !includeOnlyActive)
    {
      dumpState->args[0] = DumpStateOrd::TcDumpOneScanFragRec;
      dumpState->args[1] = recordNo;
      dumpState->args[2] = instance();
      signal->setLength(3);
      execDUMP_STATE_ORD(signal);
    }
    numRecords--;
    recordNo++;

    if (recordNo < cscanFragrecFileSize && numRecords > 0)
    {
      dumpState->args[0] = DumpStateOrd::TcDumpSetOfScanFragRec;
      dumpState->args[1] = recordNo;
      dumpState->args[2] = numRecords;
      dumpState->args[3] = instance();
      dumpState->args[4] = includeOnlyActive;
      sendSignal(reference(), GSN_DUMP_STATE_ORD, signal, 5, JBB);
    }
    return;
  }

  // Dump one ScanFragRec
  if (dumpState->args[0] == DumpStateOrd::TcDumpOneScanFragRec){
    /**
     * DUMP 2501 12 1
     * Print ScanFrag record 12 in instance 1
     *
     * Output to cluster log.
     */
    jam();
    Uint32 recordNo = RNIL;
    Uint32 instanceId = RNIL;
    if (signal->getLength() == 3)
    {
      recordNo = dumpState->args[1];
      instanceId = dumpState->args[2];
    }
    else
    {
      return;
    }
    if (instanceId != instance())
    {
      return;
    }
    if (recordNo >= cscanFragrecFileSize)
    {
      return;
    }

    ScanFragRecPtr sfp;
    sfp.i = recordNo;
    c_scan_frag_pool.getPtr(sfp);
    infoEvent("Dbtc::ScanFragRec[%d]: state=%d, lqhFragId=%u",
	      sfp.i,
	      sfp.p->scanFragState,
              sfp.p->lqhScanFragId);
    infoEvent(" nodeid=%d, timer=%d",
	      refToNode(sfp.p->lqhBlockref),
	      sfp.p->scanFragTimer);
    return;
  }

  // Dump set of ScanRecords
  if (dumpState->args[0] == DumpStateOrd::TcDumpSetOfScanRec){
    /**
     * DUMP 2502 12 10 1 1
     * Print Scan records 12 through 21 in instance 1
     * Last parameter == 0 indicates that we will print all
     * records, otherwise we will only print active scan records.
     * Last parameter is defaulting to 1 if not provided
     *
     * Output to the cluster log
     */
    jam();
    Uint32 recordNo = 0;
    Uint32 numRecords = 0;
    Uint32 instanceId = 0;
    Uint32 includeOnlyActive = 1;

    if (signal->getLength() >= 4)
    {
      recordNo = dumpState->args[1];
      numRecords = dumpState->args[2];
      instanceId = dumpState->args[3];
      if (signal->getLength() >= 5)
      {
        includeOnlyActive = dumpState->args[4];
      }
    }
    else
    {
      return;
    }
    if (instance() != instanceId)
    {
      return;
    }
    if (numRecords > MAX_RECORDS_AT_A_TIME)
    {
      numRecords = MAX_RECORDS_AT_A_TIME;
    }
    if (recordNo >= cscanrecFileSize)
    {
      return;
    }

    ScanRecordPtr sp;
    sp.i = recordNo;
    ptrAss(sp, scanRecord);
    if (sp.p->scanState != ScanRecord::IDLE ||
        !includeOnlyActive)
    {
      dumpState->args[0] = DumpStateOrd::TcDumpOneScanRec;
      dumpState->args[1] = recordNo;
      dumpState->args[2] = instance();
      signal->setLength(3);
      execDUMP_STATE_ORD(signal);
    }
    
    numRecords--;
    recordNo++;
    if (recordNo < cscanrecFileSize && numRecords > 0)
    {
      dumpState->args[0] = DumpStateOrd::TcDumpSetOfScanRec;
      dumpState->args[1] = recordNo;
      dumpState->args[2] = numRecords;
      dumpState->args[3] = instance();
      dumpState->args[4] = includeOnlyActive;
      sendSignal(reference(), GSN_DUMP_STATE_ORD, signal, 5, JBB);
    }
    return;
  }

  // Dump one ScanRecord
  // and associated ScanFragRec and ApiConnectRecord
  if (dumpState->args[0] == DumpStateOrd::TcDumpOneScanRec){
    /**
     * DUMP 2504 12 1
     * This will print ScanRecord 12 in instance 1 and all associated
     * ScanFrag records and ApiConnect records.
     *
     * It is necessary to provide both record number and instance id
     * to get anything printed using this dump command.
     */
    jam();
    Uint32 recordNo = RNIL;
    Uint32 instanceId = 0;
    if (signal->getLength() == 3)
    {
      recordNo = dumpState->args[1];
      instanceId = dumpState->args[2];
    }
    else
    {
      return;
    }
    if (instanceId != instance())
    {
      return;
    }
    if (recordNo >= cscanrecFileSize)
    {
      return;
    }

    ScanRecordPtr sp;
    sp.i = recordNo;
    ptrAss(sp, scanRecord);
    infoEvent("Dbtc::ScanRecord[%d]: state=%d, "
	      "nextfrag=%d, nofrag=%d",
	      sp.i,
	      sp.p->scanState,
	      sp.p->scanNextFragId,
	      sp.p->scanNoFrag);
    infoEvent(" para=%d, receivedop=%d, noOprecPerFrag=%d",
	      sp.p->scanParallel,
	      sp.p->scanReceivedOperations,
	      sp.p->batch_size_rows);
    infoEvent(" schv=%d, tab=%d, sproc=%d",
	      sp.p->scanSchemaVersion,
	      sp.p->scanTableref,
	      sp.p->scanStoredProcId);
    infoEvent(" apiRec=%d, next=%d",
	      sp.p->scanApiRec, sp.p->nextScan);

    if (sp.p->scanState != ScanRecord::IDLE){
      // Request dump of ScanFragRec
      ScanFragRecPtr sfptr;
#define DUMP_SFR(x){\
      Local_ScanFragRec_dllist list(c_scan_frag_pool, x);\
      for(list.first(sfptr); !sfptr.isNull(); list.next(sfptr)){\
	dumpState->args[0] = DumpStateOrd::TcDumpOneScanFragRec; \
	dumpState->args[1] = sfptr.i;\
        dumpState->args[2] = instance(); \
        signal->setLength(3); \
	execDUMP_STATE_ORD(signal);\
      }}

      DUMP_SFR(sp.p->m_running_scan_frags);
      DUMP_SFR(sp.p->m_queued_scan_frags);
      DUMP_SFR(sp.p->m_delivered_scan_frags);

      // Request dump of ApiConnectRecord
      dumpState->args[0] = DumpStateOrd::TcDumpOneApiConnectRec;
      dumpState->args[1] = sp.p->scanApiRec;
      dumpState->args[2] = instance();
      signal->setLength(3);
      execDUMP_STATE_ORD(signal);      
    }
    return;
  }

  // Dump Set of TcConnectRecord(s)
  if (dumpState->args[0] == DumpStateOrd::TcDumpSetOfTcConnectRec)
  {
    /**
     * DUMP 2517 12 10 1 1
     * This will print recordNo from 12 to 21 in instance 1 (first
     * instance is number since instance 0 is the DBTC Proxy instance).
     * We will not print records that doesn't exist. The last parameter
     * indicates whether to include only active records or if we should
     * include all records. This last parameter defaults to 1 which means
     * we are only printing information about active records.
     *
     * It is necessary to specify record number, number of records
     * and instance id to get anything printed.
     *
     * Use this DUMP command with care and avoid printing more than
     * 10 records per dump command in production systems to avoid
     * overloading any parts of the system. Also print from instance
     * at a time since otherwise it will be almost impossible to know
     * which instance have generated what printout.
     * 
     * The printouts will end up in the cluster log.
     */
    jam();
    Uint32 recordNo = RNIL;
    Uint32 instanceId = RNIL;
    Uint32 numRecords = 1;
    Uint32 includeOnlyActive = 1;

    if (signal->getLength() >= 4)
    {
      recordNo = dumpState->args[1];
      numRecords = dumpState->args[2];
      instanceId = dumpState->args[3];
      if (signal->getLength() >= 5)
      {
        includeOnlyActive = dumpState->args[4];
      }
    }
    else
    {
      return;
    }
    if (instanceId != instance())
    {
      return;
    }
    if (numRecords > MAX_RECORDS_AT_A_TIME)
    {
      numRecords = MAX_RECORDS_AT_A_TIME;
    }
    if (recordNo >= ctcConnectFilesize)
    {
      return;
    }

    TcConnectRecordPtr tc;
    tc.i = recordNo;
    ptrAss(tc, tcConnectRecord);
    if (tc.p->tcConnectstate == OS_CONNECTED ||
        !includeOnlyActive)
    {
      dumpState->args[0] = DumpStateOrd::TcDumpOneTcConnectRec;
      dumpState->args[1] = recordNo;
      dumpState->args[2] = instanceId;
      signal->setLength(3);
      execDUMP_STATE_ORD(signal);
    }

    numRecords--;
    recordNo++;
    if (recordNo < ctcConnectFilesize && numRecords > 0)
    {
      dumpState->args[0] = DumpStateOrd::TcDumpSetOfTcConnectRec;
      dumpState->args[1] = recordNo;
      dumpState->args[2] = numRecords;
      dumpState->args[3] = instanceId;
      dumpState->args[4] = includeOnlyActive;
      sendSignal(reference(), GSN_DUMP_STATE_ORD, signal, 5, JBB);
    }
    return;
  }

  // Dump one TcConnectRecord
  if (dumpState->args[0] == DumpStateOrd::TcDumpOneTcConnectRec)
  {
    /**
     * This part is mostly used as an assistant function to dump a range
     * of TcConnect records. It can also be invoked directly in
     * the following manner.
     *
     * DUMP 2516 12 1
     * Record 12 will be printed in instance 1
     */
    jam();
    Uint32 recordNo = RNIL;
    Uint32 instanceId = RNIL;
    if (signal->getLength() == 3)
    {
      recordNo = dumpState->args[1];
      instanceId = dumpState->args[2];
    }
    else
    {
      return;
    }
    if (recordNo >= ctcConnectFilesize)
    {
      return;
    }
    if (instanceId != instance())
    {
      return;
    }

    TcConnectRecordPtr tc;
    tc.i = recordNo;
    ptrAss(tc, tcConnectRecord);
    infoEvent("Dbtc::TcConnectRecord[%d]: state=%d, apiCon=%d, "
	      "commitAckMarker=%d",
	      tc.i,
	      tc.p->tcConnectstate,
	      tc.p->apiConnect,
	      tc.p->commitAckMarker);
    infoEvent(" special flags=%x, noOfNodes=%d, operation=%d",
	      tc.p->m_special_op_flags,
	      tc.p->noOfNodes,
	      tc.p->operation);
    infoEvent(" clientData=%d, savePointId=%d, nodes(%d,%d,%d,%d), ",
	      tc.p->clientData,
	      tc.p->savePointId,
              tc.p->tcNodedata[0],
              tc.p->tcNodedata[1],
              tc.p->tcNodedata[2],
              tc.p->tcNodedata[3]);
    infoEvent(" next=%d, instance=%u ",
	      tc.p->nextTcConnect,
              instance());
    return;
  }

  // Dump Set of ApiConnectRecord(s)
  if (dumpState->args[0] == DumpStateOrd::TcDumpSetOfApiConnectRec)
  {
    /**
     * Print a range of ApiConnect records
     * This command is invoked by the following DUMP command:
     * DUMP 2515 12 2 1
     * Print ApiConnect record 12 through 13 in instance 1
     *
     * The output will end up in the cluster log.
     * 
     * WARNING: Don't print more than at most 10 records at a time
     * in a production system.
     * Use the full set of parameters such that we also specify the
     * instance id. This is necessary to know what is actually printed
     * since the printout from instance 1 and instance 2 doesn't
     * differ.
     */
    jam();
    Uint32 recordNo = RNIL;
    Uint32 instanceId = RNIL;
    Uint32 numRecords = 1;
    Uint32 includeOnlyActive = 1;

    if (signal->getLength() >= 4)
    {
      recordNo = dumpState->args[1];
      numRecords = dumpState->args[2];
      instanceId = dumpState->args[3];
      if (signal->getLength() >= 5)
      {
        includeOnlyActive = dumpState->args[4];
      }
    }
    else
    {
      return;
    }
    if (instanceId != instance())
    {
      return;
    }
    if (recordNo >= capiConnectFilesize)
    {
      return;
    }

    ApiConnectRecordPtr ap;
    ap.i = recordNo;
    ptrAss(ap, apiConnectRecord);

    if (!includeOnlyActive ||
        ((ap.p->apiConnectstate != CS_CONNECTED) &&
           (ap.p->apiConnectstate != CS_ABORTING ||
            ap.p->abortState != AS_IDLE)))
    {
      dumpState->args[0] = DumpStateOrd::TcDumpOneApiConnectRec;
      dumpState->args[1] = recordNo;
      dumpState->args[2] = instanceId;
      signal->setLength(3);
      execDUMP_STATE_ORD(signal);
    }

    numRecords--;
    recordNo++;
    if (recordNo < capiConnectFilesize && numRecords > 0)
    {
      dumpState->args[0] = DumpStateOrd::TcDumpSetOfApiConnectRec;
      dumpState->args[1] = recordNo;
      dumpState->args[2] = numRecords;
      dumpState->args[3] = instanceId;
      dumpState->args[4] = includeOnlyActive;
      sendSignal(reference(), GSN_DUMP_STATE_ORD, signal, 5, JBB);
    }
    return;
  }

  // Dump one ApiConnectRecord
  if (dumpState->args[0] == DumpStateOrd::TcDumpOneApiConnectRec){
    /**
     * Print one ApiConnect record.
     * This is a support function to DUMP 2515. It can also
     * be used directly using the following command.
     *
     * DUMP 2505 12 1
     * Print ApiConnect record 12 in instance 1
     *
     * The output will be printed to the cluster log.
     */
    jam();
    Uint32 recordNo = RNIL;
    Uint32 instanceId = RNIL;
    if (signal->getLength() == 3)
    {
      recordNo = dumpState->args[1];
      instanceId = dumpState->args[2];
    }
    else
    {
      return;
    }

    if (recordNo >= capiConnectFilesize)
    {
      return;
    }
    if (instanceId != instance())
    {
      return;
    }

    ApiConnectRecordPtr ap;
    ap.i = recordNo;
    ptrAss(ap, apiConnectRecord);
    infoEvent("Dbtc::ApiConnectRecord[%d]: state=%d, abortState=%d, "
	      "apiFailState=%d",
	      ap.i,
	      ap.p->apiConnectstate,
	      ap.p->abortState,
	      ap.p->apiFailState);
    infoEvent(" transid(0x%x, 0x%x), apiBref=0x%x, scanRec=%d",
	      ap.p->transid[0],
	      ap.p->transid[1],
	      ap.p->ndbapiBlockref,
	      ap.p->apiScanRec);
    infoEvent(" ctcTimer=%d, apiTimer=%d, counter=%d, retcode=%d, "
	      "retsig=%d",
	      ctcTimer, getApiConTimer(ap.i),
	      ap.p->counter,
	      ap.p->returncode,
	      ap.p->returnsignal);
    infoEvent(" lqhkeyconfrec=%d, lqhkeyreqrec=%d, "
	      "tckeyrec=%d",
	      ap.p->lqhkeyconfrec,
	      ap.p->lqhkeyreqrec,
	      ap.p->tckeyrec);
    infoEvent(" next=%d, instance=%u, firstTc=%d ",
	      ap.p->nextApiConnect,
              instance(),
              ap.p->firstTcConnect);
    return;
  }

  if (dumpState->args[0] == DumpStateOrd::TcDumpApiConnectRecSummary)
  {
    /**
     * This DUMP command is used to print a summary of ApiConnect record
     * connected to API nodes.
     * The following use of it is allowed:
     * DUMP 2514 1
     * This will print a summary of all ApiConnect records in instance 1
     *
     * Output is to the node log.
     */
    jam();
    Uint32 apiNode = 1;
    Uint32 pos = 0;

    Uint32 seized_count = 0;      /* Number seized by an Api */
    Uint32 stateless_count = 0;   /* Number 'started' with no ops */
    Uint32 stateful_count = 0;    /* Number running */
    Uint32 scan_count = 0;        /* Number used for scans */
    const Uint32 userVisibleConnectFilesize = capiConnectFilesize / 3;
    
    if (signal->getLength() == 2)
    {
      if (instance() != dumpState->args[1])
      {
        return;
      }
      ndbout_c("Start of ApiConnectRec summary (%u total allocated)",
               userVisibleConnectFilesize);
      /*
       * total allocated = MaxNoOfConcurrentTransactions
       * total allocated = unseized + SUM_over_Api_nodes(seized)
       *   unseized can be seized by any Api node
       *   seized are 'owned' by a particular api node, can't be used
       *     by others until they are manually released.
       * seized = seized_unused + stateless + stateful + scan
       *   seized_unused are 'idle' connection objects owned by an
       *     api node.
       *   stateless are potentially idle connection objects, last
       *     used in a TC-stateless operation (e.g. read committed)
       *   stateful are in use, handling some transaction
       *   scan are in use, handling a table or index scan
       */
    }
    else if (signal->getLength() == 7)
    {
      apiNode = dumpState->args[1];
      pos = dumpState->args[2];
      seized_count = dumpState->args[3];
      stateless_count = dumpState->args[4];
      stateful_count = dumpState->args[5];
      scan_count = dumpState->args[6];
    }
    else
    {
      return;
    }

    if (apiNode == 0)
      return;

    while (apiNode < MAX_NODES)
    {
      if (getNodeInfo(apiNode).getType() == NODE_TYPE_API &&
          getNodeInfo(apiNode).m_version != 0)
      {
        break;
      }
      apiNode ++;
    }
    
    if (apiNode >= MAX_NODES)
    {
      ndbout_c("End of ApiConnectRec summary");
      return;
    }

    Uint32 limit = MIN(pos + 16, userVisibleConnectFilesize);

    while(pos < limit)
    {
      apiConnectptr.i = pos;
      ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
      /* Following code mostly similar to that for NdbInfo transactions table */
      Uint32 conState = apiConnectptr.p->apiConnectstate;

      if (conState == CS_ABORTING && apiConnectptr.p->abortState == AS_IDLE)
      {
        /**
         * These is for all practical purposes equal
         */
        conState = CS_CONNECTED;
      }

      if ((refToNode(apiConnectptr.p->ndbapiBlockref) == apiNode) &&
          (conState != CS_DISCONNECTED))
      {
        seized_count++;
        
        if (conState == CS_STARTED && 
            apiConnectptr.p->lqhkeyreqrec == 0)
        {
          stateless_count++;
        }
        else if (conState == CS_START_SCAN)
        {
          scan_count++;
        }
        else if (conState != CS_CONNECTED)
        {
          stateful_count++;
        }
      }
      
      pos++;
    }

    if (pos >= userVisibleConnectFilesize)
    {
      /* Finished with this apiNode, output info, if any */
//      if (seized_count > 0)
      {
        ndbout_c("  Api node %u connect records seized : %u stateless : %u stateful : %u scan : %u",
                 apiNode, seized_count, stateless_count, stateful_count, scan_count);
        seized_count = 0;
        stateless_count = 0;
        stateful_count = 0;
        scan_count = 0;
      }
      
      apiNode++;
      pos = 0;
    }
    
    signal->theData[0] = DumpStateOrd::TcDumpApiConnectRecSummary;
    signal->theData[1] = apiNode;
    signal->theData[2] = pos;
    signal->theData[3] = seized_count;
    signal->theData[4] = stateless_count;
    signal->theData[5] = stateful_count;
    signal->theData[6] = scan_count;

    sendSignal(reference(), GSN_DUMP_STATE_ORD, signal, 7, JBB);
    return;
  }

  if (dumpState->args[0] == DumpStateOrd::TcSetTransactionTimeout){
    jam();
    if(signal->getLength() > 1){
      set_timeout_value(signal->theData[1]);
    }
    else
    {
      /* Reset to configured value */
      const ndb_mgm_configuration_iterator * p = 
        m_ctx.m_config.getOwnConfigIterator();
      ndbrequire(p != 0);
      
      Uint32 val = 3000;
      ndb_mgm_get_int_parameter(p, CFG_DB_TRANSACTION_DEADLOCK_TIMEOUT, &val);
      set_timeout_value(val);
    }
  }

  if (dumpState->args[0] == DumpStateOrd::TcSetApplTransactionTimeout){
    jam();
    if(signal->getLength() > 1){
      set_appl_timeout_value(signal->theData[1]);
    }
  }

  if (dumpState->args[0] == DumpStateOrd::TcStartDumpIndexOpCount)
  {
    static int frequency = 1;
    if (signal->getLength() > 1)
      frequency = signal->theData[1];
    else
      if (refToBlock(signal->getSendersBlockRef()) != DBTC)
	frequency = 1;
    
    if (frequency)
    {
      dumpState->args[0] = DumpStateOrd::TcDumpIndexOpCount;
      execDUMP_STATE_ORD(signal);
      dumpState->args[0] = DumpStateOrd::TcStartDumpIndexOpCount;
      
      Uint32 delay = 1000 * (frequency > 25 ? 25 : frequency);
      sendSignalWithDelay(cownref, GSN_DUMP_STATE_ORD, signal, delay, 1);
    }
  }
  
  if (dumpState->args[0] == DumpStateOrd::TcDumpIndexOpCount)
  {
    infoEvent("instance: %u, IndexOpCount: pool: %d free: %d",
              instance(),
	      c_theIndexOperationPool.getSize(),
	      c_theIndexOperationPool.getNoOfFree());
    return;
  }

  if (arg == 2550)
  {
    jam();
    Uint32 len = signal->getLength() - 1;
    if (len + 2 > 25)
    {
      jam();
      infoEvent("Too long filter");
      return;
    }
    if (validate_filter(signal))
    {
      jam();
      memmove(signal->theData + 2, signal->theData + 1, 4 * len);
      signal->theData[0] = 2551;
      signal->theData[1] = 0;    // record
      sendSignal(reference(), GSN_DUMP_STATE_ORD, signal, len + 2, JBB);
      
      infoEvent("Starting dump of transactions");
    }
    return;
  }

  if (arg == 2551)
  {
    jam();
    Uint32 record = signal->theData[1];
    Uint32 len = signal->getLength();
    ndbassert(len > 1);

    ApiConnectRecordPtr ap;
    ap.i = record;
    ptrAss(ap, apiConnectRecord);

    bool print = false;
    for (Uint32 i = 0; i<32; i++)
    {
      jam();
      print = match_and_print(signal, ap);

      ap.i++;
      if (ap.i == capiConnectFilesize || print)
      {
	jam();
	break;
      }
      
      ptrAss(ap, apiConnectRecord);
    }

    if (ap.i == capiConnectFilesize)
    {
      jam();
      infoEvent("End of transaction dump");
      return;
    }
    
    signal->theData[1] = ap.i;
    if (print)
    {
      jam();
      sendSignalWithDelay(reference(), GSN_DUMP_STATE_ORD, signal, 200, len);
    }
    else
    {
      jam();
      sendSignal(reference(), GSN_DUMP_STATE_ORD, signal, len, JBB);
    }
    return;
  }
#ifdef ERROR_INSERT
  if (arg == 2552 || arg == 4002 || arg == 4003)
  {
    ndbrequire(m_commitAckMarkerPool.getNoOfFree() == m_commitAckMarkerPool.getSize());
    return;
  }
#endif

  if (arg == DumpStateOrd::DihTcSumaNodeFailCompleted &&
      signal->getLength() == 2)
  {
    jam();
    NodeId nodeId = signal->theData[1];
    if (nodeId < MAX_NODES && nodeId < NDB_ARRAY_SIZE(capiConnectClosing))
    {
      warningEvent(" DBTC: capiConnectClosing[%u]: %u",
                   nodeId, capiConnectClosing[nodeId]);
    }
    else
    {
      warningEvent(" DBTC: dump-%u to unknown node: %u", arg, nodeId);
    }
  }

  if (arg == DumpStateOrd::TcResourceSnapshot)
  {
    RSS_OP_SNAPSHOT_SAVE(cConcScanCount);
    RSS_AP_SNAPSHOT_SAVE(c_scan_frag_pool);
    RSS_AP_SNAPSHOT_SAVE(c_theDefinedTriggerPool);
    RSS_AP_SNAPSHOT_SAVE(c_theFiredTriggerPool);
    RSS_AP_SNAPSHOT_SAVE(c_theIndexPool);
    RSS_AP_SNAPSHOT_SAVE(m_commitAckMarkerPool);
    RSS_AP_SNAPSHOT_SAVE(c_theIndexOperationPool);
#ifdef ERROR_INSERT
    rss_cconcurrentOp = c_counters.cconcurrentOp;
#endif
  }
  if (arg == DumpStateOrd::TcResourceCheckLeak)
  {
    RSS_OP_SNAPSHOT_CHECK(cConcScanCount);
    RSS_AP_SNAPSHOT_CHECK(c_scan_frag_pool);
    RSS_AP_SNAPSHOT_CHECK(c_theDefinedTriggerPool);
    RSS_AP_SNAPSHOT_CHECK(c_theFiredTriggerPool);
    RSS_AP_SNAPSHOT_CHECK(c_theIndexPool);
    RSS_AP_SNAPSHOT_CHECK(m_commitAckMarkerPool);
    RSS_AP_SNAPSHOT_CHECK(c_theIndexOperationPool);
#ifdef ERROR_INSERT
    ndbrequire(rss_cconcurrentOp == c_counters.cconcurrentOp);
#endif
  }

  if (arg == DumpStateOrd::TcDumpPoolLevels)
  {
    /**
     * DUMP 2555 1
     * Prints pool levels for instance 1 to cluster log
     */
    if (signal->getLength() == 2)
    {
      if (signal->theData[1] != instance())
      {
        return;
      }
      infoEvent("TC: instance: %u, Print pool levels", instance());
      signal->theData[4] = signal->theData[1];
      signal->theData[1] = 1;
      signal->theData[2] = 0;
      signal->theData[3] = 0;
      sendSignal(reference(), GSN_DUMP_STATE_ORD, signal, 5, JBB);
      return;
    }
    if (signal->getLength() != 5)
    {
      ndbout_c("DUMP TcDumpPoolLevels : Bad signal length : %u", signal->getLength());
      return;
    }
    if (signal->theData[4] != instance())
    {
      return;
    }
    
    Uint32 resource = signal->theData[1];
    Uint32 position = signal->theData[2];
    Uint32 sum = signal->theData[3];
    /* const Uint32 MAX_ITER = 200; */

    switch(resource)
    {
    case 1: 
      infoEvent("TC : Concurrent operations in use/total : %u/%u (%u bytes each)", 
                c_counters.cconcurrentOp, 
                ctcConnectFilesize,
                (Uint32) sizeof(TcConnectRecord));
      resource++;
      position = 0;
      sum = 0;
      break;
    case 2:
      infoEvent("TC : Concurrent scans in use/total : %u/%u (%u bytes each)", 
                cConcScanCount, 
                cscanrecFileSize,
                (Uint32) sizeof(ScanRecord));
      resource++;
      position = 0;
      sum = 0;
      break;
    case 3:
      infoEvent("TC : Scan Frag records in use/total : %u/%u (%u bytes each)",
                c_scan_frag_pool.getSize() - 
                c_scan_frag_pool.getNoOfFree(), 
                c_scan_frag_pool.getSize(),
                (Uint32) sizeof(ScanFragRec));
      resource++;
      position = 0;
      sum = 0;
      break;
    default:
      return;
    }

    signal->theData[0] = DumpStateOrd::TcDumpPoolLevels;
    signal->theData[1] = resource;
    signal->theData[2] = position;
    signal->theData[3] = sum;
    signal->theData[4] = instance();
    sendSignal(reference(), GSN_DUMP_STATE_ORD, signal, 5, JBB);
    return;
  }
}//Dbtc::execDUMP_STATE_ORD()

void Dbtc::execDBINFO_SCANREQ(Signal *signal)
{
  DbinfoScanReq req= *(DbinfoScanReq*)signal->theData;
  const Ndbinfo::ScanCursor* cursor =
    CAST_CONSTPTR(Ndbinfo::ScanCursor, DbinfoScan::getCursorPtr(&req));
  Ndbinfo::Ratelimit rl;

  jamEntry();

  switch(req.tableId){
  case Ndbinfo::POOLS_TABLEID:
  {
    Ndbinfo::pool_entry pools[] =
    {
      { "Defined Trigger",
        c_theDefinedTriggerPool.getUsed(),
        c_theDefinedTriggerPool.getSize(),
        c_theDefinedTriggerPool.getEntrySize(),
        c_theDefinedTriggerPool.getUsedHi(),
        { CFG_DB_NO_TRIGGERS,0,0,0 }},
      { "Fired Trigger",
        c_theFiredTriggerPool.getUsed(),
        c_theFiredTriggerPool.getSize(),
        c_theFiredTriggerPool.getEntrySize(),
        c_theFiredTriggerPool.getUsedHi(),
        { CFG_DB_NO_TRIGGER_OPS,0,0,0 }},
      { "Index",
        c_theIndexPool.getUsed(),
        c_theIndexPool.getSize(),
        c_theIndexPool.getEntrySize(),
        c_theIndexPool.getUsedHi(),
        { CFG_DB_NO_TABLES,
          CFG_DB_NO_ORDERED_INDEXES,
          CFG_DB_NO_UNIQUE_HASH_INDEXES,0 }},
      { "Scan Fragment",
        c_scan_frag_pool.getUsed(),
        c_scan_frag_pool.getSize(),
        c_scan_frag_pool.getEntrySize(),
        c_scan_frag_pool.getUsedHi(),
        { CFG_DB_NO_LOCAL_SCANS,0,0,0 }},
      { "Commit ACK Marker",
        m_commitAckMarkerPool.getUsed(),
        m_commitAckMarkerPool.getSize(),
        m_commitAckMarkerPool.getEntrySize(),
        m_commitAckMarkerPool.getUsedHi(),
        { CFG_DB_NO_TRANSACTIONS,0,0,0 }},
      { "Index Op",
        c_theIndexOperationPool.getUsed(),
        c_theIndexOperationPool.getSize(),
        c_theIndexOperationPool.getEntrySize(),
        c_theIndexOperationPool.getUsedHi(),
        { CFG_DB_NO_INDEX_OPS,0,0,0 }},
      { "Operations",
        c_counters.cconcurrentOp,
        ctcConnectFilesize,
        sizeof(TcConnectRecord),
        0,
        { CFG_DB_NO_TRANSACTIONS,
          CFG_DB_NO_OPS,0,0 }},
      { "TC Scan Record",  /* TC redundantly included to improve readability */
        cConcScanCount,
        cscanrecFileSize,
        sizeof(ScanRecord),
        0, /* No HWM */
        {CFG_DB_NO_SCANS, 0, 0, 0}},
      { NULL, 0,0,0,0,{0,0,0,0} }
    };

   const size_t num_config_params =
      sizeof(pools[0].config_params) / sizeof(pools[0].config_params[0]);
    Uint32 pool = cursor->data[0];
    BlockNumber bn = blockToMain(number());
    while(pools[pool].poolname)
    {
      jam();
      Ndbinfo::Row row(signal, req);
      row.write_uint32(getOwnNodeId());
      row.write_uint32(bn);           // block number
      row.write_uint32(instance());   // block instance
      row.write_string(pools[pool].poolname);

      row.write_uint64(pools[pool].used);
      row.write_uint64(pools[pool].total);
      row.write_uint64(pools[pool].used_hi);
      row.write_uint64(pools[pool].entry_size);
      for (size_t i = 0; i < num_config_params; i++)
        row.write_uint32(pools[pool].config_params[i]);
      ndbinfo_send_row(signal, req, row, rl);
      pool++;
      if (rl.need_break(req))
      {
        jam();
        ndbinfo_send_scan_break(signal, req, rl, pool);
        return;
      }
    }
    break;
  }
  case Ndbinfo::TC_TIME_TRACK_STATS_TABLEID:
  {
    Uint32 restore = cursor->data[0];
    HostRecordPtr hostPtr;
    Uint32 first_index = restore & 0xFFFF;
    hostPtr.i = restore >> 16;
    if (hostPtr.i == 0)
      hostPtr.i = 1;
    for ( ; hostPtr.i < MAX_NODES; hostPtr.i++)
    {
      ptrCheckGuard(hostPtr, chostFilesize, hostRecord);
      if (hostPtr.p->time_tracked == FALSE)
        continue;
      for (Uint32 i = first_index; i < TIME_TRACK_HISTOGRAM_RANGES; i++)
      {
        Ndbinfo::Row row(signal, req);
        row.write_uint32(getOwnNodeId());
        row.write_uint32(DBTC);
        row.write_uint32(instance());
        row.write_uint32(hostPtr.i);
        row.write_uint64(c_time_track_histogram_boundary[i]);
        row.write_uint64(hostPtr.p->time_track_scan_histogram[i]);
        row.write_uint64(hostPtr.p->time_track_scan_error_histogram[i]);
        row.write_uint64(hostPtr.p->time_track_scan_frag_histogram[i]);
        row.write_uint64(hostPtr.p->time_track_scan_frag_error_histogram[i]);
        row.write_uint64(hostPtr.p->time_track_transaction_histogram[i]);
        row.write_uint64(hostPtr.p->time_track_transaction_error_histogram[i]);
        row.write_uint64(hostPtr.p->time_track_read_key_histogram[i]);
        row.write_uint64(hostPtr.p->time_track_write_key_histogram[i]);
        row.write_uint64(hostPtr.p->time_track_index_key_histogram[i]);
        row.write_uint64(hostPtr.p->time_track_key_error_histogram[i]);
        ndbinfo_send_row(signal, req, row, rl);
        if (rl.need_break(req))
        {
          Uint32 save = i + 1 + (hostPtr.i << 16);
          jam();
          ndbinfo_send_scan_break(signal, req, rl, save);
          return;
        }
      }
      first_index = 0;
    }
    break;
  }
  case Ndbinfo::COUNTERS_TABLEID:
  {
    Ndbinfo::counter_entry counters[] = {
      { Ndbinfo::ATTRINFO_COUNTER, c_counters.cattrinfoCount },
      { Ndbinfo::TRANSACTIONS_COUNTER, c_counters.ctransCount },
      { Ndbinfo::COMMITS_COUNTER, c_counters.ccommitCount },
      { Ndbinfo::READS_COUNTER, c_counters.creadCount },
      { Ndbinfo::SIMPLE_READS_COUNTER, c_counters.csimpleReadCount },
      { Ndbinfo::WRITES_COUNTER, c_counters.cwriteCount },
      { Ndbinfo::ABORTS_COUNTER, c_counters.cabortCount },
      { Ndbinfo::TABLE_SCANS_COUNTER, c_counters.c_scan_count },
      { Ndbinfo::RANGE_SCANS_COUNTER, c_counters.c_range_scan_count },
      { Ndbinfo::LOCAL_READ_COUNTER, c_counters.clocalReadCount },
      { Ndbinfo::LOCAL_WRITE_COUNTER, c_counters.clocalWriteCount }
    };
    const size_t num_counters = sizeof(counters) / sizeof(counters[0]);

    Uint32 i = cursor->data[0];
    BlockNumber bn = blockToMain(number());
    while(i < num_counters)
    {
      jam();
      Ndbinfo::Row row(signal, req);
      row.write_uint32(getOwnNodeId());
      row.write_uint32(bn);           // block number
      row.write_uint32(instance());   // block instance
      row.write_uint32(counters[i].id);

      row.write_uint64(counters[i].val);
      ndbinfo_send_row(signal, req, row, rl);
      i++;
      if (rl.need_break(req))
      {
        jam();
        ndbinfo_send_scan_break(signal, req, rl, i);
        return;
      }
    }

    break;
  }
  case Ndbinfo::TRANSACTIONS_TABLEID:{
    ApiConnectRecordPtr ptr;
    ptr.i = cursor->data[0];
    const Uint32 maxloop = 256;
    for (Uint32 i = 0; i < maxloop; i++)
    {
      ptrCheckGuard(ptr, capiConnectFilesize, apiConnectRecord);
      Ndbinfo::Row row(signal, req);
      if (ndbinfo_write_trans(row, ptr))
      {
        jam();
        ndbinfo_send_row(signal, req, row, rl);
      }

      ptr.i ++;
      if (ptr.i == capiConnectFilesize)
      {
        goto done;
      }
      else if (rl.need_break(req))
      {
        break;
      }
    }
    ndbinfo_send_scan_break(signal, req, rl, ptr.i);
    return;
  }
  default:
    break;
  }

done:
  ndbinfo_send_scan_conf(signal, req, rl);
}

bool
Dbtc::ndbinfo_write_trans(Ndbinfo::Row & row, ApiConnectRecordPtr transPtr)
{
  Uint32 conState = transPtr.p->apiConnectstate;

  if (conState == CS_ABORTING && transPtr.p->abortState == AS_IDLE)
  {
    /**
     * These is for all practical purposes equal
     */
    conState = CS_CONNECTED;
  }

  if (conState == CS_CONNECTED ||
      conState == CS_DISCONNECTED ||
      conState == CS_RESTART)
  {
    return false;
  }

  row.write_uint32(getOwnNodeId());
  row.write_uint32(instance());   // block instance
  row.write_uint32(transPtr.i);
  row.write_uint32(transPtr.p->ndbapiBlockref);
  row.write_uint32(transPtr.p->transid[0]);
  row.write_uint32(transPtr.p->transid[1]);
  row.write_uint32(conState);
  row.write_uint32(transPtr.p->m_flags);
  row.write_uint32(transPtr.p->lqhkeyreqrec);
  Uint32 outstanding = 0;
  switch((ConnectionState)conState) {
  case CS_CONNECTED:
  case CS_DISCONNECTED:
    break;
  case CS_STARTED:
  case CS_RECEIVING:
  case CS_REC_COMMITTING:
  case CS_START_COMMITTING:
  case CS_SEND_FIRE_TRIG_REQ:
  case CS_WAIT_FIRE_TRIG_REQ:
    outstanding = transPtr.p->lqhkeyreqrec - transPtr.p->lqhkeyconfrec;
    break;
  case CS_COMMITTING:
  case CS_COMPLETING:
  case CS_COMMIT_SENT:
  case CS_COMPLETE_SENT:
  case CS_ABORTING:
    outstanding = transPtr.p->counter;
    break;
  case CS_PREPARE_TO_COMMIT:
    break;
  case CS_START_SCAN:
    // TODO
    break;
  case CS_WAIT_ABORT_CONF:
  case CS_WAIT_COMMIT_CONF:
  case CS_WAIT_COMPLETE_CONF:
    // not easily computed :-(
    break;
  case CS_FAIL_PREPARED:
  case CS_FAIL_ABORTED:
    // we're assembling a state...
    break;
  case CS_FAIL_COMMITTING:
  case CS_FAIL_COMMITTED:
  case CS_FAIL_ABORTING:
  case CS_FAIL_COMPLETED:
    // not easily computed :_(
    break;
  case CS_RESTART:
    break;
  }

  row.write_uint32(outstanding);

  Uint32 apiTimer = getApiConTimer(transPtr.i);
  row.write_uint32(apiTimer ? (ctcTimer - apiTimer) / 100 : 0);
  return true;
}

bool
Dbtc::validate_filter(Signal* signal)
{
  Uint32 * start = signal->theData + 1;
  Uint32 * end = signal->theData + signal->getLength();
  if (start == end)
  {
    infoEvent("No filter specified, not listing...");
    return false;
  }

  while(start < end)
  {
    switch(* start){
    case 1: // API Node
    case 4: // Inactive time
      start += 2;
      break;
    case 2: // Transid
      start += 3;
      break;
    default:
      infoEvent("Invalid filter op: 0x%x pos: %ld",
		* start,
		(long int)(start - (signal->theData + 1)));
      return false;
    }
  }

  if (start != end)
  {
    infoEvent("Invalid filter, unexpected end");
    return false;
  }

  return true;
}

bool
Dbtc::match_and_print(Signal* signal, ApiConnectRecordPtr apiPtr)
{
  Uint32 conState = apiPtr.p->apiConnectstate;
  if (conState == CS_CONNECTED ||
      conState == CS_DISCONNECTED ||
      conState == CS_RESTART)
    return false;

  Uint32 len = signal->getLength();
  Uint32* start = signal->theData + 2;
  Uint32* end = signal->theData + len;
  Uint32 apiTimer = getApiConTimer(apiPtr.i);
  while (start < end)
  {
    jam();
    switch(* start){
    case 1:
      jam();
      if (refToNode(apiPtr.p->ndbapiBlockref) != * (start + 1))
	return false;
      start += 2;
      break;
    case 2:
      jam();
      if (apiPtr.p->transid[0] != * (start + 1) ||
	  apiPtr.p->transid[1] != * (start + 2))
	return false;
      start += 3;
      break;
    case 4:{
      jam();
      if (apiTimer == 0 || ((ctcTimer - apiTimer) / 100) < * (start + 1))
	return false;
      start += 2;
      break;
    }
    default:
      ndbassert(false);
      return false;
    }
  }
  
  if (start != end)
  {
    ndbassert(false);
    return false;
  }

  /**
   * Do print
   */
  Uint32 *temp = signal->theData + 25;
  memcpy(temp, signal->theData, 4 * len);

  char state[10];
  const char *stateptr = "";
  
  switch(apiPtr.p->apiConnectstate){
  case CS_STARTED:
    stateptr = "Prepared";
    break;
  case CS_RECEIVING:
  case CS_REC_COMMITTING:
  case CS_START_COMMITTING:
    stateptr = "Running";
    break;
  case CS_COMMITTING:
    stateptr = "Committing";
    break;
  case CS_COMPLETING:
    stateptr = "Completing";
    break;
  case CS_PREPARE_TO_COMMIT:
    stateptr = "Prepare to commit";
    break;
  case CS_COMMIT_SENT:
    stateptr = "Commit sent";
    break;
  case CS_COMPLETE_SENT:
    stateptr = "Complete sent";
    break;
  case CS_ABORTING:
    stateptr = "Aborting";
    break;
  case CS_START_SCAN:
    stateptr = "Scanning";
    break;
  case CS_WAIT_ABORT_CONF:
  case CS_WAIT_COMMIT_CONF:
  case CS_WAIT_COMPLETE_CONF:
  case CS_FAIL_PREPARED:
  case CS_FAIL_COMMITTING:
  case CS_FAIL_COMMITTED:
  case CS_RESTART:
  case CS_FAIL_ABORTED:
  case CS_DISCONNECTED:
  default:
    BaseString::snprintf(state, sizeof(state), 
			 "%u", apiPtr.p->apiConnectstate);
    stateptr = state;
    break;
  }

  char buf[100];
  BaseString::snprintf(buf, sizeof(buf),
		       "TRX[%u]: API: %d(0x%x)"
		       "transid: 0x%x 0x%x inactive: %u(%d) state: %s",
		       apiPtr.i,
		       refToNode(apiPtr.p->ndbapiBlockref),
		       refToBlock(apiPtr.p->ndbapiBlockref),
		       apiPtr.p->transid[0],
		       apiPtr.p->transid[1],
		       apiTimer ? (ctcTimer - apiTimer) / 100 : 0,
		       c_apiConTimer_line[apiPtr.i],
		       stateptr);
  infoEvent("%s", buf);
  
  memcpy(signal->theData, temp, 4*len);
  return true;
}

void Dbtc::execABORT_ALL_REQ(Signal* signal)
{
  jamEntry();
  AbortAllReq * req = (AbortAllReq*)&signal->theData[0];
  AbortAllRef * ref = (AbortAllRef*)&signal->theData[0];
  
  const Uint32 senderData = req->senderData;
  const BlockReference senderRef = req->senderRef;
  
  if(getAllowStartTransaction(refToNode(senderRef), 0) == true && !getNodeState().getSingleUserMode()){
    jam();

    ref->senderData = senderData;
    ref->errorCode = AbortAllRef::InvalidState;
    sendSignal(senderRef, GSN_ABORT_ALL_REF, signal, 
	       AbortAllRef::SignalLength, JBB);
    return;
  }
  
  if(c_abortRec.clientRef != 0){
    jam();
    
    ref->senderData = senderData;
    ref->errorCode = AbortAllRef::AbortAlreadyInProgress;
    sendSignal(senderRef, GSN_ABORT_ALL_REF, signal, 
	       AbortAllRef::SignalLength, JBB);
    return;
  }

  if(refToNode(senderRef) != getOwnNodeId()){
    jam();
    
    ref->senderData = senderData;
    ref->errorCode = AbortAllRef::FunctionNotImplemented;
    sendSignal(senderRef, GSN_ABORT_ALL_REF, signal, 
	       AbortAllRef::SignalLength, JBB);
    return;
  }

  c_abortRec.clientRef = senderRef;
  c_abortRec.clientData = senderData;
  c_abortRec.oldTimeOutValue = ctimeOutValue;
  
  ctimeOutValue = 0;
  const Uint32 sleepTime = (2 * 10 * ctimeOutCheckDelay + 199) / 200;
  
  checkAbortAllTimeout(signal, (sleepTime == 0 ? 1 : sleepTime));
}

void Dbtc::checkAbortAllTimeout(Signal* signal, Uint32 sleepTime)
{
  
  ndbrequire(c_abortRec.clientRef != 0);
  
  if(sleepTime > 0){
    jam();
    
    sleepTime -= 1;
    signal->theData[0] = TcContinueB::ZWAIT_ABORT_ALL;
    signal->theData[1] = sleepTime;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 200, 2);
    return;
  }

  AbortAllConf * conf = (AbortAllConf*)&signal->theData[0];
  conf->senderData = c_abortRec.clientData;
  sendSignal(c_abortRec.clientRef, GSN_ABORT_ALL_CONF, signal, 
	     AbortAllConf::SignalLength, JBB);
  
  ctimeOutValue = c_abortRec.oldTimeOutValue;
  c_abortRec.clientRef = 0;
}

/* **************************************************************** */
/* ---------------------------------------------------------------- */
/* ------------------ TRIGGER AND INDEX HANDLING ------------------ */
/* ---------------------------------------------------------------- */
/* **************************************************************** */

void Dbtc::execCREATE_TRIG_IMPL_REQ(Signal* signal)
{
  jamEntry();
  if (!assembleFragments(signal))
  {
    jam();
    return;
  }

  const CreateTrigImplReq* req = (const CreateTrigImplReq*)signal->getDataPtr();
  const Uint32 senderRef = req->senderRef;
  const Uint32 senderData = req->senderData;

  SectionHandle handle(this, signal);
  releaseSections(handle); // Not using mask

  TcDefinedTriggerData* triggerData;
  DefinedTriggerPtr triggerPtr;

  triggerPtr.i = req->triggerId;
  if (ERROR_INSERTED(8033) ||
      !c_theDefinedTriggers.getPool().seizeId(triggerPtr, req->triggerId)) {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    // Failed to allocate trigger record
ref:
    CreateTrigImplRef* ref =  (CreateTrigImplRef*)signal->getDataPtrSend();
    
    ref->senderRef = reference();
    ref->senderData = senderData;
    ref->errorCode = CreateTrigImplRef::InconsistentTC;
    ref->errorLine = __LINE__;
    sendSignal(senderRef, GSN_CREATE_TRIG_IMPL_REF, 
               signal, CreateTrigImplRef::SignalLength, JBB);
    return;
  }
  c_theDefinedTriggers.addFirst(triggerPtr);

  triggerData = triggerPtr.p;
  triggerData->triggerId = req->triggerId;
  triggerData->triggerType = TriggerInfo::getTriggerType(req->triggerInfo);
  triggerData->triggerEvent = TriggerInfo::getTriggerEvent(req->triggerInfo);
  triggerData->oldTriggerIds[0] = RNIL;
  triggerData->oldTriggerIds[1] = RNIL;
  triggerData->refCount = 0;

  switch(triggerData->triggerType){
  case TriggerType::SECONDARY_INDEX:
    jam();
    triggerData->indexId = req->indexId;
    break;
  case TriggerType::REORG_TRIGGER:
  case TriggerType::FULLY_REPLICATED_TRIGGER:
    jam();
    triggerData->tableId = req->tableId;
    break;
  case TriggerType::FK_PARENT:
  case TriggerType::FK_CHILD:
    triggerData->fkId = req->indexId;
    break;
  default:
    c_theDefinedTriggers.release(triggerPtr);
    goto ref;
  }

  if (unlikely(req->triggerId != req->upgradeExtra[1]))
  {
    /**
     * This is nasty upgrade for unique indexes
     */
    jam();
    ndbrequire(req->triggerId == req->upgradeExtra[0]);
    ndbrequire(triggerData->triggerType == TriggerType::SECONDARY_INDEX);

    DefinedTriggerPtr insertPtr = triggerPtr;
    DefinedTriggerPtr updatePtr;
    DefinedTriggerPtr deletePtr;
    if (c_theDefinedTriggers.getPool().seizeId(updatePtr, req->upgradeExtra[1]) == false)
    {
      jam();
      c_theDefinedTriggers.release(insertPtr);
      goto ref;
    }
    c_theDefinedTriggers.addFirst(updatePtr);

    if (c_theDefinedTriggers.getPool().seizeId(deletePtr, req->upgradeExtra[2]) == false)
    {
      jam();
      c_theDefinedTriggers.release(insertPtr);
      c_theDefinedTriggers.release(updatePtr);
      goto ref;
    }
    c_theDefinedTriggers.addFirst(deletePtr);

    insertPtr.p->triggerEvent = TriggerEvent::TE_INSERT;

    updatePtr.p->triggerId = req->upgradeExtra[1];
    updatePtr.p->triggerType = TriggerType::SECONDARY_INDEX;
    updatePtr.p->triggerEvent = TriggerEvent::TE_UPDATE;
    updatePtr.p->oldTriggerIds[0] = RNIL;
    updatePtr.p->oldTriggerIds[1] = RNIL;
    updatePtr.p->indexId = req->indexId;
    updatePtr.p->refCount = 0;

    deletePtr.p->triggerId = req->upgradeExtra[2];
    deletePtr.p->triggerType = TriggerType::SECONDARY_INDEX;
    deletePtr.p->triggerEvent = TriggerEvent::TE_DELETE;
    deletePtr.p->oldTriggerIds[0] = RNIL;
    deletePtr.p->oldTriggerIds[1] = RNIL;
    deletePtr.p->indexId = req->indexId;
    deletePtr.p->refCount = 0;
  }

  CreateTrigImplConf* conf = (CreateTrigImplConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(senderRef, GSN_CREATE_TRIG_IMPL_CONF, 
             signal, CreateTrigImplConf::SignalLength, JBB);
}

void Dbtc::execDROP_TRIG_IMPL_REQ(Signal* signal)
{
  jamEntry();
  const DropTrigImplReq* req = (const DropTrigImplReq*)signal->getDataPtr();
  const Uint32 senderRef = req->senderRef;
  const Uint32 senderData = req->senderData;

  DefinedTriggerPtr triggerPtr;
  triggerPtr.i = req->triggerId;

  if (ERROR_INSERTED(8035) ||
      ((triggerPtr.p = c_theDefinedTriggers.getPtr(req->triggerId)) == NULL))
  {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    // Failed to find find trigger record
    DropTrigImplRef* ref = (DropTrigImplRef*)signal->getDataPtrSend();

    ref->senderRef = reference();
    ref->senderData = senderData;
    ref->errorCode = DropTrigImplRef::InconsistentTC;
    ref->errorLine = __LINE__;
    sendSignal(senderRef, GSN_DROP_TRIG_IMPL_REF, 
               signal, DropTrigImplRef::SignalLength, JBB);
    return;
  }

  if (triggerPtr.p->refCount > 0)
  {
    jam();
    sendSignalWithDelay(reference(), GSN_DROP_TRIG_IMPL_REQ,
                        signal, 100, signal->getLength());
    return;
  }

  if (unlikely(triggerPtr.p->oldTriggerIds[0] != RNIL))
  {
    jam();

    const Uint32 * oldId = triggerPtr.p->oldTriggerIds;
    if (c_theDefinedTriggers.getPtr(oldId[0])->refCount > 0 ||
        c_theDefinedTriggers.getPtr(oldId[1])->refCount > 0)
    {
      jam();
      sendSignalWithDelay(reference(), GSN_DROP_TRIG_IMPL_REQ,
                          signal, 100, signal->getLength());
      return;
    }

    c_theDefinedTriggers.release(triggerPtr.p->oldTriggerIds[0]);
    c_theDefinedTriggers.release(triggerPtr.p->oldTriggerIds[1]);
  }

  // Release trigger record
  c_theDefinedTriggers.release(triggerPtr);

  DropTrigImplConf* conf = (DropTrigImplConf*)signal->getDataPtrSend();

  conf->senderRef = reference();
  conf->senderData = senderData;
  
  sendSignal(senderRef, GSN_DROP_TRIG_IMPL_CONF, 
             signal, DropTrigImplConf::SignalLength, JBB);
}

void Dbtc::execCREATE_INDX_IMPL_REQ(Signal* signal)
{
  jamEntry();
  const CreateIndxImplReq * const req =  
    (const CreateIndxImplReq *)signal->getDataPtr();
  const Uint32 senderRef = req->senderRef;
  const Uint32 senderData = req->senderData;
  TcIndexData* indexData;
  TcIndexDataPtr indexPtr;

  SectionHandle handle(this, signal);
  if (ERROR_INSERTED(8034) ||
      !c_theIndexes.getPool().seizeId(indexPtr, req->indexId)) {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    // Failed to allocate index record
     CreateIndxImplRef * const ref =  
       (CreateIndxImplRef *)signal->getDataPtrSend();

     ref->senderRef = reference();
     ref->senderData = senderData;
     ref->errorCode = CreateIndxImplRef::InconsistentTC;
     ref->errorLine = __LINE__;
     releaseSections(handle);
     sendSignal(senderRef, GSN_CREATE_INDX_IMPL_REF, 
                signal, CreateIndxImplRef::SignalLength, JBB);
     return;
  }
  c_theIndexes.addFirst(indexPtr);
  indexData = indexPtr.p;
  // Indexes always start in state IS_BUILDING
  // Will become IS_ONLINE in execALTER_INDX_IMPL_REQ
  indexData->indexState = IS_BUILDING;
  indexData->indexId = indexPtr.i;
  indexData->primaryTableId = req->tableId;

  // So far need only attribute count
  SegmentedSectionPtr ssPtr;
  handle.getSection(ssPtr, CreateIndxReq::ATTRIBUTE_LIST_SECTION);
  SimplePropertiesSectionReader r0(ssPtr, getSectionSegmentPool());
  r0.reset(); // undo implicit first()
  if (!r0.getWord(&indexData->attributeList.sz) ||
      !r0.getWords(indexData->attributeList.id, indexData->attributeList.sz)) {
    ndbabort();
  }
  indexData->primaryKeyPos = indexData->attributeList.sz;

  releaseSections(handle);
  
  CreateIndxImplConf * const conf =  
    (CreateIndxImplConf *)signal->getDataPtrSend();

  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(senderRef, GSN_CREATE_INDX_IMPL_CONF, 
             signal, CreateIndxImplConf::SignalLength, JBB);
}

void Dbtc::execALTER_INDX_IMPL_REQ(Signal* signal)
{
  jamEntry();
  const AlterIndxImplReq * const req =
    (const AlterIndxImplReq *)signal->getDataPtr();
  const Uint32 senderRef = req->senderRef;
  const Uint32 senderData = req->senderData;
  TcIndexData* indexData;
  const Uint32 requestType = req->requestType;
  const Uint32 indexId = req->indexId;

  if ((indexData = c_theIndexes.getPtr(indexId)) == NULL) {
    jam();
    // Failed to find index record
    AlterIndxImplRef * const ref =  
      (AlterIndxImplRef *)signal->getDataPtrSend();
    
    ref->senderRef = reference();
    ref->senderData = senderData;
    ref->errorCode = AlterIndxImplRef::InconsistentTC;
    ref->errorLine = __LINE__;

    sendSignal(senderRef, GSN_ALTER_INDX_IMPL_REF, 
	       signal, AlterIndxImplRef::SignalLength, JBB);
    return;
  }
  // Found index record, alter it's state  
  switch (requestType) {
  case AlterIndxImplReq::AlterIndexOnline:
    jam();
    indexData->indexState = IS_ONLINE;
    break;
  case AlterIndxImplReq::AlterIndexOffline:
    jam();
    indexData->indexState = IS_BUILDING; // wl3600_todo ??
    break;
  default:
    ndbabort();
  }
  AlterIndxImplConf * const conf =  
    (AlterIndxImplConf *)signal->getDataPtrSend();
  
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(senderRef, GSN_ALTER_INDX_IMPL_CONF, 
	     signal, AlterIndxImplConf::SignalLength, JBB);
}

void
Dbtc::execCREATE_FK_IMPL_REQ(Signal* signal)
{
  jamEntry();
  CreateFKImplReq reqCopy = * CAST_CONSTPTR(CreateFKImplReq,
                                            signal->getDataPtr());
  CreateFKImplReq * req = &reqCopy;

  Uint32 errCode = 0;
  SectionHandle handle(this, signal);

  if (req->requestType == CreateFKImplReq::RT_PREPARE)
  {
    jam();
    Ptr<TcFKData> fkPtr;
    if (c_fk_hash.find(fkPtr, req->fkId))
    {
      jam();
      errCode = CreateFKImplRef::ObjectAlreadyExist;
      goto error;
    }
    if (!c_fk_pool.seize(fkPtr))
    {
      jam();
      errCode = CreateFKImplRef::NoMoreObjectRecords;
      goto error;
    }

    fkPtr.p->fkId = req->fkId;
    fkPtr.p->bits = req->bits;
    fkPtr.p->parentTableId = req->parentTableId;
    fkPtr.p->childTableId = req->childTableId;
    fkPtr.p->childIndexId = req->childIndexId;

    if (req->parentIndexId != RNIL)
    {
      /**
       * Ignore base-table here...we'll only use the index anyway
       */
      jam();
      fkPtr.p->parentTableId = req->parentIndexId;
    }

    if (req->childIndexId == RNIL)
    {
      jam();
      fkPtr.p->childIndexId = req->childTableId;
    }

    {
      SegmentedSectionPtr ssPtr;
      handle.getSection(ssPtr, CreateFKImplReq::PARENT_COLUMNS);
      fkPtr.p->parentTableColumns.sz = ssPtr.sz;
      if (ssPtr.sz > NDB_ARRAY_SIZE(fkPtr.p->parentTableColumns.id))
      {
        errCode = CreateFKImplRef::InvalidFormat;
        goto error;
      }
      copy(fkPtr.p->parentTableColumns.id, ssPtr);
    }

    {
      SegmentedSectionPtr ssPtr;
      handle.getSection(ssPtr, CreateFKImplReq::CHILD_COLUMNS);
      fkPtr.p->childTableColumns.sz = ssPtr.sz;
      if (ssPtr.sz > NDB_ARRAY_SIZE(fkPtr.p->childTableColumns.id))
      {
        errCode = CreateFKImplRef::InvalidFormat;
        goto error;
      }
      copy(fkPtr.p->childTableColumns.id, ssPtr);
    }

    c_fk_hash.add(fkPtr);
  }
  else if (req->requestType == CreateFKImplReq::RT_ABORT)
  {
    jam();
    Ptr<TcFKData> fkPtr;
    ndbassert(c_fk_hash.find(fkPtr, req->fkId));
    if (c_fk_hash.find(fkPtr, req->fkId))
    {
      jam();
      c_fk_hash.release(fkPtr);
    }
  }
  else
  {
    ndbabort(); // No other request should reach TC
  }

  releaseSections(handle);
  {
    CreateFKImplConf * conf = CAST_PTR(CreateFKImplConf,
                                       signal->getDataPtrSend());
    conf->senderRef = reference();
    conf->senderData = req->senderData;
    sendSignal(req->senderRef, GSN_CREATE_FK_IMPL_CONF,
               signal, CreateFKImplConf::SignalLength, JBB);
  }
  return;
error:
  releaseSections(handle);
  CreateFKImplRef * ref = CAST_PTR(CreateFKImplRef,
                                   signal->getDataPtrSend());
  ref->senderRef = reference();
  ref->senderData = req->senderData;
  ref->errorCode = errCode;
  sendSignal(req->senderRef, GSN_CREATE_FK_IMPL_REF,
             signal, CreateFKImplRef::SignalLength, JBB);
}

void
Dbtc::execDROP_FK_IMPL_REQ(Signal* signal)
{
  jamEntry();
  DropFKImplReq reqCopy = * CAST_CONSTPTR(DropFKImplReq,
                                          signal->getDataPtr());
  DropFKImplReq * req = &reqCopy;

  SectionHandle handle(this, signal);
  releaseSections(handle);

  Ptr<TcFKData> fkPtr;
  ndbassert(c_fk_hash.find(fkPtr, req->fkId));
  if (c_fk_hash.find(fkPtr, req->fkId))
  {
    jam();
    c_fk_hash.release(fkPtr);

    DropFKImplConf * conf = CAST_PTR(DropFKImplConf,
                                     signal->getDataPtrSend());
    conf->senderRef = reference();
    conf->senderData = req->senderData;
    sendSignal(req->senderRef, GSN_DROP_FK_IMPL_CONF,
               signal, DropFKImplConf::SignalLength, JBB);
  }
  else
  {
    jam();
    DropFKImplRef * ref = CAST_PTR(DropFKImplRef,
                                   signal->getDataPtrSend());
    ref->senderRef = reference();
    ref->senderData = req->senderData;
    ref->errorCode = DropFKImplRef::NoSuchObject;
    sendSignal(req->senderRef, GSN_DROP_FK_IMPL_REF,
               signal, DropFKImplRef::SignalLength, JBB);
  }
}

void Dbtc::execFIRE_TRIG_ORD(Signal* signal)
{
  jamEntry();
  FireTrigOrd * const fireOrd =  (FireTrigOrd *)signal->getDataPtr();
  SectionHandle handle(this, signal);
  const bool longsignal = handle.m_cnt > 0;

  ApiConnectRecordPtr transPtr;
  TcConnectRecordPtr opPtr;
  bool transIdOk = true;
  /* Check the received transaction id
   * Older nodes do not send transid info in FIRE_TRIG_ORD
   */
  const Uint32 sourceNode = refToNode(signal->getSendersBlockRef());
  const Uint32 sourceNodeVersion = getNodeInfo(sourceNode).m_version;
  bool sigContainsTransId = ndb_fire_trig_ord_transid(sourceNodeVersion);
  
  /* Get triggering operation record */
  opPtr.i = fireOrd->getConnectionPtr();
  ptrCheckGuard(opPtr, ctcConnectFilesize, tcConnectRecord);
  
  /* Get transaction record */
  transPtr.i = opPtr.p->apiConnect;
  if (unlikely(transPtr.i == RNIL))
  {
    /* Looks like the connect record was released
     * Treat as a bad transid
     */
    transIdOk = false;
  }
  else
  {
    ptrCheckGuard(transPtr, capiConnectFilesize, apiConnectRecord);
    
    /* Check if signal's trans id and operation's transid are aligned */
    transIdOk = (! sigContainsTransId) |
      (! ((fireOrd->m_transId1 ^ transPtr.p->transid[0]) |
          (fireOrd->m_transId2 ^ transPtr.p->transid[1])));
  } 

  TcFiredTriggerData key;
  key.fireingOperation = opPtr.i;
  key.nodeId = refToNode(signal->getSendersBlockRef());
  FiredTriggerPtr trigPtr;
  bool ok = transIdOk;

  if (likely(longsignal && transIdOk))
  {
    jam();
    if (likely(c_theFiredTriggerPool.seize(trigPtr)))
    {
      jam();
      trigPtr.p->nodeId = key.nodeId;
      trigPtr.p->fireingOperation = key.fireingOperation;
      trigPtr.p->triggerId = fireOrd->m_triggerId;
    }
    else
    {
      jam();
      ok = false;
    }
  }

  Uint32 errorCode = ZTOO_MANY_FIRED_TRIGGERS;
  if (likely((longsignal && ok) ||
             c_firedTriggerHash.find(trigPtr, key)))
  {
    jam();
    if (!longsignal)
    {
      jam();
      c_firedTriggerHash.remove(trigPtr);
    }

    trigPtr.p->triggerType = (TriggerType::Value)fireOrd->m_triggerType;
    trigPtr.p->triggerEvent = (TriggerEvent::Value)fireOrd->m_triggerEvent;

    if (unlikely(signal->getLength() < FireTrigOrd::SignalLength))
    {
      jam();
      ndbrequire(! sigContainsTransId );
      Ptr<TcDefinedTriggerData> ptr;
      c_theDefinedTriggers.getPtr(ptr, trigPtr.p->triggerId);
      trigPtr.p->triggerType = ptr.p->triggerType;
      trigPtr.p->triggerEvent = ptr.p->triggerEvent;
    }
    trigPtr.p->fragId= fireOrd->fragId;
    if (longsignal)
    {
      jam();
      AttributeBuffer::DataBufferPool & pool = c_theAttributeBufferPool;
      {
        //TODO: error insert to make e.g middle append fail
        LocalAttributeBuffer tmp1(pool, trigPtr.p->keyValues);
        LocalAttributeBuffer tmp2(pool, trigPtr.p->beforeValues);
        LocalAttributeBuffer tmp3(pool, trigPtr.p->afterValues);
        append(tmp1, handle.m_ptr[0], getSectionSegmentPool());
        append(tmp2, handle.m_ptr[1], getSectionSegmentPool());
        append(tmp3, handle.m_ptr[2], getSectionSegmentPool());
      }
      releaseSections(handle);
    }

    ok &= trigPtr.p->keyValues.getSize() == fireOrd->m_noPrimKeyWords;
    ok &= trigPtr.p->afterValues.getSize() == fireOrd->m_noAfterValueWords;
    ok &= trigPtr.p->beforeValues.getSize() == fireOrd->m_noBeforeValueWords;

    if (ERROR_INSERTED(8085))
    {
      ok = false;
    }

    if(likely( ok ))
    {
      jam();
      setApiConTimer(transPtr.i, ctcTimer, __LINE__);
      opPtr.p->numReceivedTriggers++;

      /**
       * Fully replicated trigger have a more complex scenario, so not
       * 1 LQHKEYREQ per trigger, rather one LQHKEYREQ per node group
       * the table is stored in. This is handled in
       * executeFullyReplicatedTrigger.
       *
       * We add one here also for fully replicated triggers, this
       * represents the operation to scan for fragments to update, so
       * if there are no triggers to fire then this will be decremented
       * even with no actual triggers being fired. In addition one will
       * be added for each fragment that the fully replicated trigger
       * will update.
       *
       * Foreign key child triggers will issue one read on the parent
       * table, so this will always be one operation.
       *
       * Foreign key parent triggers will either issue one operation
       * against a PK or UK index in which case this add is sufficient.
       * It can also issue an index scan in which case several rows might
       * be found. In this case the scan is the one represented by this
       * add of the triggerExecutionCount, in addition each resulting
       * PK UPDATE/DELETE will add one more to the triggerExecutionCount.
       *
       * Unique index triggers will issue one operation for DELETEs and
       * INSERTs which is represented by this add, for updates there will
       * be one INSERT and one DELETE issued which causes one more to be
       * added as part of trigger execution.
       *
       * Finally we also have REORG triggers that fires, in this case we
       * increment it here and if the TCKEYREQ execution decides that
       * the reorg trigger doesn't need to be executed it will call
       * trigger_op_finished before returning from execTCKEYREQ. Otherwise
       * it will be called as usual on receiving LQHKEYCONF for the
       * triggered operation.
       */
      opPtr.p->triggerExecutionCount++; // Default 1 LQHKEYREQ per trigger
      // Insert fired trigger in execution queue
      {
        Local_TcFiredTriggerData_fifo
          list(c_theFiredTriggerPool, opPtr.p->thePendingTriggers);
        list.addLast(trigPtr);
      }

      if (opPtr.p->numReceivedTriggers == opPtr.p->numFiredTriggers ||
          transPtr.p->isExecutingDeferredTriggers())
      {
        jam();
        transPtr.p->theFiredTriggers.appendList(opPtr.p->thePendingTriggers);
	executeTriggers(signal, &transPtr);
      }
      return;
    }

    /* Trigger entry found but either :
     *   - Overload resulted in loss of Trig_Attrinfo
     *     : Release resources + Abort transaction
     *   - Bad transaction id due to concurrent abort?
     *     : Release resources
     */
    jam();
    errorCode = ZINCONSISTENT_TRIGGER_STATE;
    // Release trigger records
    AttributeBuffer::DataBufferPool & pool = c_theAttributeBufferPool;
    LocalAttributeBuffer tmp1(pool, trigPtr.p->keyValues);
    tmp1.release();
    LocalAttributeBuffer tmp2(pool, trigPtr.p->beforeValues);
    tmp2.release();
    LocalAttributeBuffer tmp3(pool, trigPtr.p->afterValues);
    tmp3.release();
    c_theFiredTriggerPool.release(trigPtr);
  }

  releaseSections(handle);

  /* Either no trigger entry found, or overload, or
   * bad transid
   * If transid is ok, abort the transaction.
   * else, return.
   * (Note small risk of 'abort of innocent' in upgrade 
   *  scenario with no transid in FIRE_TRIG_ORD)
   */
  jam();
  if (transIdOk)
  {
    jam();
    abortTransFromTrigger(signal, transPtr, errorCode);
  }

  return;
}

void Dbtc::execTRIG_ATTRINFO(Signal* signal)
{
  jamEntry();
  TrigAttrInfo * const trigAttrInfo =  (TrigAttrInfo *)signal->getDataPtr();
  Uint32 attrInfoLength = signal->getLength() - TrigAttrInfo::StaticLength;
  const Uint32 *src = trigAttrInfo->getData();
  FiredTriggerPtr firedTrigPtr;
  
  TcFiredTriggerData key;
  key.fireingOperation = trigAttrInfo->getConnectionPtr();
  key.nodeId = refToNode(signal->getSendersBlockRef());
  if(!c_firedTriggerHash.find(firedTrigPtr, key)){
    jam();
    /* TODO : Node failure handling (use sig-train assembly) */
    if(!c_firedTriggerHash.seize(firedTrigPtr)){
      jam();
      /**
       * Will be handled when FIRE_TRIG_ORD arrives
       */
      ndbout_c("op: %d node: %d failed to seize",
	       key.fireingOperation, key.nodeId);
      return;
    }
    ndbrequire(firedTrigPtr.p->keyValues.getSize() == 0 &&
	       firedTrigPtr.p->beforeValues.getSize() == 0 &&
	       firedTrigPtr.p->afterValues.getSize() == 0);
    
    firedTrigPtr.p->nodeId = refToNode(signal->getSendersBlockRef());
    firedTrigPtr.p->fireingOperation = key.fireingOperation;
    firedTrigPtr.p->triggerId = trigAttrInfo->getTriggerId();
    c_firedTriggerHash.add(firedTrigPtr);
  }
  
  AttributeBuffer::DataBufferPool & pool = c_theAttributeBufferPool;
  switch (trigAttrInfo->getAttrInfoType()) {
  case(TrigAttrInfo::PRIMARY_KEY):
    jam();
    {
      LocalAttributeBuffer buf(pool, firedTrigPtr.p->keyValues);
      buf.append(src, attrInfoLength);
    }
    break;
  case(TrigAttrInfo::BEFORE_VALUES):
    jam();
    {
      LocalAttributeBuffer buf(pool, firedTrigPtr.p->beforeValues);
      buf.append(src, attrInfoLength);
    }
    break;
  case(TrigAttrInfo::AFTER_VALUES):
    jam();
    {
      LocalAttributeBuffer buf(pool, firedTrigPtr.p->afterValues);
      buf.append(src, attrInfoLength);
    }
    break;
  default:
    ndbabort();
  }
}

void Dbtc::execDROP_INDX_IMPL_REQ(Signal* signal)
{
  jamEntry();
  const DropIndxImplReq * const req =
    (const DropIndxImplReq *)signal->getDataPtr();
  const Uint32 senderRef = req->senderRef;
  const Uint32 senderData = req->senderData;
  TcIndexData* indexData;
  
  if (ERROR_INSERTED(8036) ||
      (indexData = c_theIndexes.getPtr(req->indexId)) == NULL) {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    // Failed to find index record
    DropIndxImplRef * const ref =  
      (DropIndxImplRef *)signal->getDataPtrSend();

    ref->senderRef = reference();
    ref->senderData = senderData;
    ref->errorCode = DropIndxImplRef::InconsistentTC;
    ref->errorLine = __LINE__;
    sendSignal(senderRef, GSN_DROP_INDX_IMPL_REF, 
               signal, DropIndxImplRef::SignalLength, JBB);
    return;
  }
  // Release index record
  indexData->indexState = IS_OFFLINE;
  c_theIndexes.release(req->indexId);

  DropIndxImplConf * const conf =  
    (DropIndxImplConf *)signal->getDataPtrSend();

  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(senderRef, GSN_DROP_INDX_IMPL_CONF, 
             signal, DropIndxImplConf::SignalLength, JBB);
}

void Dbtc::execTCINDXREQ(Signal* signal)
{
  jamEntry();

  TcKeyReq * const tcIndxReq =  (TcKeyReq *)signal->getDataPtr();
  const UintR TapiIndex = tcIndxReq->apiConnectPtr;
  Uint32 tcIndxRequestInfo = tcIndxReq->requestInfo;
  Uint32 startFlag = tcIndxReq->getStartFlag(tcIndxRequestInfo);
  ApiConnectRecordPtr transPtr;
  bool isLongTcIndxReq= (signal->getNoOfSections() != 0);
  SectionHandle handle(this, signal);

  transPtr.i = TapiIndex;
  if (transPtr.i >= capiConnectFilesize) {
    jam();
    warningHandlerLab(signal, __LINE__);
    releaseSections(handle);
    return;
  }//if
  ptrAss(transPtr, apiConnectRecord);
  ApiConnectRecord * const regApiPtr = transPtr.p;  
  // Seize index operation
  TcIndexOperationPtr indexOpPtr;

#ifdef ERROR_INSERT
  if (ERROR_INSERTED(8100))
  {
    char buf[128];
    BaseString::snprintf(buf, sizeof(buf), "Inserted 8100, startFlag %u, regApiPtr->apiConnectstate %u, regApiPtr->abortState %u", startFlag, regApiPtr->apiConnectstate, regApiPtr->abortState);
    warningEvent("%s", buf);
    
    if (startFlag == 1)
    {
      jam();
      /*
        Phase 1:
        Abort the transaction by simulating a fake node failure.
        Don't send any TCROLLBACKREP until in next TCINDXREQ call
        to simulate a slow signal still in the air.
      */
      Signal s = *signal;
      signal->theData[0] = TcContinueB::ZDEBUG_DELAYED_ABORT;
      signal->theData[1] = transPtr.i;
      signal->theData[2] = regApiPtr->transid[0];
      signal->theData[3] = regApiPtr->transid[1];
      signal->theData[4] = ZNODEFAIL_BEFORE_COMMIT;
      signal->theData[5] = RS_TCROLLBACKREP;
      signal->theData[6] = 8101;
      sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 6);
      *signal = s;
    }
  }
  else if (ERROR_INSERTED(8101))
  {
    char buf[128];
    BaseString::snprintf(buf, sizeof(buf), "Inserted 8101, startFlag %u, regApiPtr->apiConnectstate %u, regApiPtr->abortState %u", startFlag, regApiPtr->apiConnectstate, regApiPtr->abortState);
    warningEvent("%s", buf);

    jam();
    /*
      Phase 2:
      Transaction has been aborted by a fake node failure,
      but the API hasn't been informed yet. Force the state
      into waiting for pending signals.
    */
    if (regApiPtr->returncode != ZNODEFAIL_BEFORE_COMMIT ||
        regApiPtr->apiConnectstate != CS_ABORTING ||
        regApiPtr->abortState != AS_IDLE)
    {
      /*
        We are waiting for abort to complete, but have not reached
        the transaction state we want to test. Try again.
      */
      jam();
      sendSignalWithDelay(reference(), GSN_TCINDXREQ, signal, 100,
                          TcKeyReq::StaticLength, &handle);
      return;
    }
    CLEAR_ERROR_INSERT_VALUE;
  }
#endif

   if (startFlag == 1 &&
       (regApiPtr->apiConnectstate == CS_CONNECTED ||
        (regApiPtr->apiConnectstate == CS_STARTED && 
         regApiPtr->firstTcConnect == RNIL) ||
        (regApiPtr->apiConnectstate == CS_ABORTING && 
         regApiPtr->abortState == AS_IDLE)))
  {
    jam();
    // This is a newly started transaction, clean-up from any
    // previous transaction.
    initApiConnectRec(signal, regApiPtr, true);
    regApiPtr->apiConnectstate = CS_STARTED;
  }//if (startFlag == 1 && ...
  else if (regApiPtr->apiConnectstate == CS_ABORTING)
  {
    jam();
    /*
      Transaction has been aborted and we have to do error
      handling, but since when we read the index table
      the generated TCKEYREQ will do the proper error
      handling. Any received TCKEYREF will be forwarded
      to the original sender as a TCINDXREF.
     */
  }//if (regApiPtr->apiConnectstate == CS_ABORTING)

  if (getNodeState().startLevel == NodeState::SL_SINGLEUSER &&
      getNodeState().getSingleUserApi() !=
      refToNode(regApiPtr->ndbapiBlockref))
  {
    jam();
    releaseSections(handle);
    terrorCode = ZCLUSTER_IN_SINGLEUSER_MODE;
    regApiPtr->m_flags |=
      TcKeyReq::getExecuteFlag(tcIndxRequestInfo) ?
      ApiConnectRecord::TF_EXEC_FLAG : 0;
    apiConnectptr = transPtr;
    abortErrorLab(signal);
    return;
  }

  if (ERROR_INSERTED(8036) || !seizeIndexOperation(regApiPtr, indexOpPtr)) {
    jam();
    releaseSections(handle);
    // Failed to allocate index operation
    terrorCode = 288;
    regApiPtr->m_flags |=
      TcKeyReq::getExecuteFlag(tcIndxRequestInfo) ?
      ApiConnectRecord::TF_EXEC_FLAG : 0;
    apiConnectptr = transPtr;
    abortErrorLab(signal);
    return;
  }
  TcIndexOperation* indexOp = indexOpPtr.p;
  indexOp->indexOpId = indexOpPtr.i;

  // Save original signal
  indexOp->tcIndxReq = *tcIndxReq;
  indexOp->connectionIndex = TapiIndex;
  regApiPtr->accumulatingIndexOp = indexOp->indexOpId;

  if (isLongTcIndxReq)
  {
    jam();
    /* KeyInfo and AttrInfo already received into sections */
    SegmentedSectionPtr keyInfoSection, attrInfoSection;

    /* Store i value for first long section of KeyInfo
     * and AttrInfo in Index operation
     */
    handle.getSection(keyInfoSection,
                      TcKeyReq::KeyInfoSectionNum);

    indexOp->keyInfoSectionIVal= keyInfoSection.i;
  
    if (handle.m_cnt == 2)
    {
      handle.getSection(attrInfoSection,
                        TcKeyReq::AttrInfoSectionNum);
      indexOp->attrInfoSectionIVal= attrInfoSection.i;
    }

    if (TcKeyReq::getDeferredConstraints(tcIndxRequestInfo))
    {
      regApiPtr->m_flags |= ApiConnectRecord::TF_DEFERRED_CONSTRAINTS;
    }

    if (TcKeyReq::getDisableFkConstraints(tcIndxRequestInfo))
    {
      regApiPtr->m_flags |= ApiConnectRecord::TF_DISABLE_FK_CONSTRAINTS;
    }
    indexOp->savedFlags = regApiPtr->m_flags;

    /* Detach sections from the handle
     * Success path code, or index operation cleanup is
     * now responsible for freeing the sections
     */
    handle.clear();

    /* All data received, process */
    readIndexTable(signal, transPtr, indexOp, 0);
    return;
  }
  else
  {
    jam();
    /* Short TcIndxReq, build up KeyInfo and AttrInfo
     * sections from separate signals
     */
    Uint32 * dataPtr = &tcIndxReq->scanInfo;
    Uint32 indexLength = TcKeyReq::getKeyLength(tcIndxRequestInfo);
    Uint32 attrLength = TcKeyReq::getAttrinfoLen(tcIndxReq->attrLen);

    indexOp->pendingKeyInfo = indexLength;
    indexOp->pendingAttrInfo = attrLength;

    const Uint32 includedIndexLength = MIN(indexLength, TcKeyReq::MaxKeyInfo);
    const Uint32 includedAttrLength = MIN(attrLength, TcKeyReq::MaxAttrInfo);
    int ret;

    if ((ret = saveINDXKEYINFO(signal, 
                               indexOp, 
                               dataPtr, 
                               includedIndexLength)) == 0) 
    {
      jam();
      /* All KI + no AI received, process */
      readIndexTable(signal, transPtr, indexOp, 0);
      return;
    }
    else if (ret == -1)
    {
      jam();
      return;
    }
    
    dataPtr += includedIndexLength;

    if (saveINDXATTRINFO(signal, 
                         indexOp, 
                         dataPtr, 
                         includedAttrLength) == 0) {
      jam();
      /* All KI and AI received, process */
      readIndexTable(signal, transPtr, indexOp, 0);
      return;
    }
  }
}

void Dbtc::execINDXKEYINFO(Signal* signal)
{
  jamEntry();
  Uint32 keyInfoLength = signal->getLength() - IndxKeyInfo::HeaderLength;
  IndxKeyInfo * const indxKeyInfo =  (IndxKeyInfo *)signal->getDataPtr();
  const Uint32 *src = indxKeyInfo->getData();
  const UintR TconnectIndex = indxKeyInfo->connectPtr;
  ApiConnectRecordPtr transPtr;
  transPtr.i = TconnectIndex;
  if (transPtr.i >= capiConnectFilesize) {
    jam();
    warningHandlerLab(signal, __LINE__);
    return;
  }//if
  ptrAss(transPtr, apiConnectRecord);
  ApiConnectRecord * const regApiPtr = transPtr.p;
  TcIndexOperationPtr indexOpPtr;
  TcIndexOperation* indexOp;

  if (compare_transid(regApiPtr->transid, indxKeyInfo->transId) == false)
  {
    TCKEY_abort(signal, 19);
    return;
  }

  if (regApiPtr->apiConnectstate == CS_ABORTING)
  {
    jam();
    return;
  }

  if((indexOpPtr.i = regApiPtr->accumulatingIndexOp) != RNIL)
  {
    indexOp = c_theIndexOperationPool.getPtr(indexOpPtr.i);

    ndbassert( indexOp->pendingKeyInfo > 0 );

    if (saveINDXKEYINFO(signal,
			indexOp, 
			src, 
			keyInfoLength) == 0) {
      jam();
      /* All KI + AI received, process */
      readIndexTable(signal, transPtr, indexOp, 0);
    }
  }
}

void Dbtc::execINDXATTRINFO(Signal* signal)
{
  jamEntry();
  Uint32 attrInfoLength = signal->getLength() - IndxAttrInfo::HeaderLength;
  IndxAttrInfo * const indxAttrInfo =  (IndxAttrInfo *)signal->getDataPtr();
  const Uint32 *src = indxAttrInfo->getData();
  const UintR TconnectIndex = indxAttrInfo->connectPtr;
  ApiConnectRecordPtr transPtr;
  transPtr.i = TconnectIndex;
  if (transPtr.i >= capiConnectFilesize) {
    jam();
    warningHandlerLab(signal, __LINE__);
    return;
  }//if
  ptrAss(transPtr, apiConnectRecord);
  ApiConnectRecord * const regApiPtr = transPtr.p;  
  TcIndexOperationPtr indexOpPtr;
  TcIndexOperation* indexOp;

  if (compare_transid(regApiPtr->transid, indxAttrInfo->transId) == false)
  {
    TCKEY_abort(signal, 19);
    return;
  }

  if (regApiPtr->apiConnectstate == CS_ABORTING)
  {
    jam();
    return;
  }

  if((indexOpPtr.i = regApiPtr->accumulatingIndexOp) != RNIL)
  {
    indexOp = c_theIndexOperationPool.getPtr(indexOpPtr.i);

    ndbassert( indexOp->pendingAttrInfo > 0 );

    if (saveINDXATTRINFO(signal,
			 indexOp, 
			 src, 
			 attrInfoLength) == 0) {
      jam();
      /* All KI + AI received, process */
      readIndexTable(signal, transPtr, indexOp, 0);
      return;
    }
    return;
  }
}

/**
 * Save received KeyInfo
 * Return true if we have received all needed data
 */
int 
Dbtc::saveINDXKEYINFO(Signal* signal,
                      TcIndexOperation* indexOp,
                      const Uint32 *src, 
                      Uint32 len)
{
  if (ERROR_INSERTED(8052) || 
      ! appendToSection(indexOp->keyInfoSectionIVal,
                        src,
                        len)) 
  {
    jam();
    // Failed to seize keyInfo, abort transaction
#ifdef VM_TRACE
    ndbout_c("Dbtc::saveINDXKEYINFO: Failed to seize buffer for KeyInfo\n");
#endif
    // Abort transaction
    apiConnectptr.i = indexOp->connectionIndex;
    ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
    releaseIndexOperation(apiConnectptr.p, indexOp);
    terrorCode = 289;
    if(TcKeyReq::getExecuteFlag(indexOp->tcIndxReq.requestInfo))
      apiConnectptr.p->m_flags |= ApiConnectRecord::TF_EXEC_FLAG;
    abortErrorLab(signal);
    return -1;
  }
  indexOp->pendingKeyInfo-= len;

  if (receivedAllINDXKEYINFO(indexOp) && receivedAllINDXATTRINFO(indexOp)) {
    jam();
    return 0;
  }
  return 1;
}

bool Dbtc::receivedAllINDXKEYINFO(TcIndexOperation* indexOp)
{
  return (indexOp->pendingKeyInfo == 0);
}

/**
 * Save signal INDXATTRINFO
 * Return true if we have received all needed data
 */
int 
Dbtc::saveINDXATTRINFO(Signal* signal,
                       TcIndexOperation* indexOp,
                       const Uint32 *src, 
                       Uint32 len)
{
  if (ERROR_INSERTED(8051) || 
      ! appendToSection(indexOp->attrInfoSectionIVal,
                        src,
                        len))
  {
    jam();
#ifdef VM_TRACE
    ndbout_c("Dbtc::saveINDXATTRINFO: Failed to seize buffer for attrInfo\n");
#endif
    apiConnectptr.i = indexOp->connectionIndex;
    ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
    releaseIndexOperation(apiConnectptr.p, indexOp);
    terrorCode = 289;
    if(TcKeyReq::getExecuteFlag(indexOp->tcIndxReq.requestInfo))
      apiConnectptr.p->m_flags |= ApiConnectRecord::TF_EXEC_FLAG;
    abortErrorLab(signal);
    return -1;
  }

  indexOp->pendingAttrInfo-= len;

  if (receivedAllINDXKEYINFO(indexOp) && receivedAllINDXATTRINFO(indexOp)) {
    jam();
    return 0;
  }
  return 1;
}

bool Dbtc::receivedAllINDXATTRINFO(TcIndexOperation* indexOp)
{
  return (indexOp->pendingAttrInfo == 0);
}

#ifdef ERROR_INSERT
extern bool ErrorImportActive;
#endif

Uint32 Dbtc::saveTRANSID_AI(Signal* signal,
                            TcIndexOperation* indexOp, 
                            const Uint32 *src,
                            Uint32 len)
{
  /* TransID_AI is received as a result of looking up a
   * unique index table
   * The unique index table is looked up using the index
   * key to receive a single attribute containing the 
   * fragment holding the base table row and the base
   * table primary key.
   * This is later used to build a TCKEYREQ against the
   * base table.
   * In this method, we prepare a KEYINFO section for the
   * TCKEYREQ as we receive TRANSID_AI words.
   *
   * Expected TRANSID_AI words :
   *
   *   Word(s)  Description           States
   *
   *   0        Attribute header      ITAS_WAIT_HEADER
   *             containing length     -> ITAS_WAIT_FRAGID
   *
   *   1        Fragment Id           ITAS_WAIT_FRAGID
   *                                   -> ITAS_WAIT_KEY
   *
   *   [2..N]   Base table primary    ITAS_WAIT_KEY
   *            key info               -> [ ITAS_WAIT_KEY |
   *                                        ITAS_WAIT_KEY_FAIL ]
   *                                   -> ITAS_ALL_RECEIVED
   *
   * The outgoing KeyInfo section contains the base
   * table primary key info, with the fragment id passed
   * as the distribution key.
   * ITAS_WAIT_KEY_FAIL state is entered when there is no 
   * space to store received TRANSID_AI information and
   * key collection must fail.  Transaction abort is performed
   * once all TRANSID_AI is received, and the system waits in
   * ITAS_WAIT_KEY_FAIL state until then.
   *
   */
  Uint32 remain= len;

  while (remain != 0)
  {
    switch(indexOp->transIdAIState) {
    case ITAS_WAIT_HEADER:
    {
      jam();
      ndbassert(indexOp->transIdAISectionIVal == RNIL);
      /* Look at the first AttributeHeader to get the
       * expected size of the primary key attribute
       */
      AttributeHeader* head = (AttributeHeader *) src;
      ndbassert(head->getHeaderSize() == 1);
      indexOp->pendingTransIdAI = 1 + head->getDataSize();

      src++;
      remain--;
      indexOp->transIdAIState = ITAS_WAIT_FRAGID;
      break;
    }
    case ITAS_WAIT_FRAGID:
    {
      jam();
      ndbassert(indexOp->transIdAISectionIVal == RNIL);
      /* Grab the fragment Id word */
      indexOp->fragmentId= *src;

      src++;
      remain--;
      indexOp->transIdAIState = ITAS_WAIT_KEY; 
      break;
    }
    case ITAS_WAIT_KEY:
    {
      jam();
      /* Add key information to long section */
#ifdef ERROR_INSERT
      if (ERROR_INSERTED(8066))
      {
        ErrorImportActive = true;
      }
#endif

      bool res = appendToSection(indexOp->transIdAISectionIVal, src, remain);
#ifdef ERROR_INSERT
      if (ERROR_INSERTED(8066))
      {
        ErrorImportActive = false;
      }
#endif

      if (res)
      {
        jam();
        remain= 0;
        break;
      }
      else
      {
        jam();
#ifdef VM_TRACE
        ndbout_c("Dbtc::saveTRANSID_AI: Failed to seize buffer for TRANSID_AI\n");
#endif
        indexOp->transIdAIState= ITAS_WAIT_KEY_FAIL;
      }
    }
      // Fall through to ITAS_WAIT_KEY_FAIL state handling

    case ITAS_WAIT_KEY_FAIL:
    {
      /* Failed when collecting key previously - if we have all the
       * TRANSID_AI now then we abort
       */
      if (indexOp->pendingTransIdAI > len)
      {
        /* Still some TransIdAI to arrive, keep waiting as if we had
         * stored it
         */
        remain= 0;
        break;
      }

      /* All TransIdAI has arrived, abort */
      apiConnectptr.i = indexOp->connectionIndex;
      ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
      releaseIndexOperation(apiConnectptr.p, indexOp);
      terrorCode = ZGET_DATAREC_ERROR;
      abortErrorLab(signal);
      return ZGET_DATAREC_ERROR;
    }

    case ITAS_ALL_RECEIVED:
      jam();
      // Fall through
    default:
      jam();
      /* Bad state, or bad state to receive TransId_Ai in */
      // Todo : Check error handling here.
#ifdef VM_TRACE
      ndbout_c("Dbtc::saveTRANSID_AI: Bad state when receiving\n");
#endif
      apiConnectptr.i = indexOp->connectionIndex;
      ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
      releaseIndexOperation(apiConnectptr.p, indexOp);
      terrorCode = ZINCONSISTENT_INDEX_USE;
      abortErrorLab(signal);
      return ZINCONSISTENT_INDEX_USE;
    } // switch
  } // while

  if ((indexOp->pendingTransIdAI-= len) == 0)
    indexOp->transIdAIState = ITAS_ALL_RECEIVED;
  
  return ZOK;
}

bool Dbtc::receivedAllTRANSID_AI(TcIndexOperation* indexOp)
{
  return (indexOp->transIdAIState == ITAS_ALL_RECEIVED);
}

/**
 * Receive signal TCINDXCONF
 * This can be either the return of reading an index table
 * or performing an index operation
 */
void Dbtc::execTCKEYCONF(Signal* signal)
{
  TcKeyConf * const tcKeyConf =  (TcKeyConf *)signal->getDataPtr();
  TcIndexOperationPtr indexOpPtr;

  jamEntry();
  indexOpPtr.i = tcKeyConf->apiConnectPtr;
  TcIndexOperation* indexOp = c_theIndexOperationPool.getPtr(indexOpPtr.i);

  /**
   * Check on TCKEYCONF whether the the transaction was committed
   */
  ndbassert(TcKeyConf::getCommitFlag(tcKeyConf->confInfo) == false);

  indexOpPtr.p = indexOp;
  if (!indexOp) {
    jam();
    // Missing index operation
    return;
  }
  const UintR TconnectIndex = indexOp->connectionIndex;
  ApiConnectRecord * const regApiPtr = &apiConnectRecord[TconnectIndex];
  apiConnectptr.p = regApiPtr;
  apiConnectptr.i = TconnectIndex;
  switch(indexOp->indexOpState) {
  case(IOS_NOOP): {
    jam();
    // Should never happen, abort
    TcKeyRef * const tcIndxRef = (TcKeyRef *)signal->getDataPtrSend();

    tcIndxRef->connectPtr = indexOp->tcIndxReq.senderData;
    tcIndxRef->transId[0] = regApiPtr->transid[0];
    tcIndxRef->transId[1] = regApiPtr->transid[1];
    tcIndxRef->errorCode = ZINCONSISTENT_INDEX_USE;    
    tcIndxRef->errorData = 0;
    sendSignal(regApiPtr->ndbapiBlockref, GSN_TCINDXREF, signal, 
	       TcKeyRef::SignalLength, JBB);
    return;
  }
  case(IOS_INDEX_ACCESS): {
    jam();
    // Just waiting for the TRANSID_AI now
    indexOp->indexOpState = IOS_INDEX_ACCESS_WAIT_FOR_TRANSID_AI;
    break;
  }
  case(IOS_INDEX_ACCESS_WAIT_FOR_TRANSID_AI): {
    jam();
    // Double TCKEYCONF, should never happen, abort
    TcKeyRef * const tcIndxRef = (TcKeyRef *)signal->getDataPtrSend();

    tcIndxRef->connectPtr = indexOp->tcIndxReq.senderData;
    tcIndxRef->transId[0] = regApiPtr->transid[0];
    tcIndxRef->transId[1] = regApiPtr->transid[1];
    tcIndxRef->errorCode = ZINCONSISTENT_INDEX_USE;    
    tcIndxRef->errorData = 0;
    sendSignal(regApiPtr->ndbapiBlockref, GSN_TCINDXREF, signal, 
	       TcKeyRef::SignalLength, JBB);
    return;
  }
  case(IOS_INDEX_ACCESS_WAIT_FOR_TCKEYCONF): {  
    jam();
    // Continue with index operation
    executeIndexOperation(signal, regApiPtr, indexOp);
    break;
  }
  }
}

void Dbtc::execTCKEYREF(Signal* signal)
{
  TcKeyRef * const tcKeyRef = (TcKeyRef *)signal->getDataPtr();
  TcIndexOperationPtr indexOpPtr;

  jamEntry();
  indexOpPtr.i = tcKeyRef->connectPtr;
  TcIndexOperation* indexOp = c_theIndexOperationPool.getPtr(indexOpPtr.i);
  indexOpPtr.p = indexOp;
  if (!indexOp) {
    jam();    
    // Missing index operation
    return;
  }
  const UintR TconnectIndex = indexOp->connectionIndex;
  ApiConnectRecord * const regApiPtr = &apiConnectRecord[TconnectIndex];

  switch(indexOp->indexOpState) {
  case(IOS_NOOP): {
    jam();    
    // Should never happen, abort
    break;
  }
  case(IOS_INDEX_ACCESS):
  case(IOS_INDEX_ACCESS_WAIT_FOR_TRANSID_AI):
  case(IOS_INDEX_ACCESS_WAIT_FOR_TCKEYCONF): {
    jam();    
    // Send TCINDXREF 
    
    TcKeyReq * const tcIndxReq = &indexOp->tcIndxReq;
    TcKeyRef * const tcIndxRef = (TcKeyRef *)signal->getDataPtrSend();
    
    tcIndxRef->connectPtr = tcIndxReq->senderData;
    tcIndxRef->transId[0] = tcKeyRef->transId[0];
    tcIndxRef->transId[1] = tcKeyRef->transId[1];
    tcIndxRef->errorCode = tcKeyRef->errorCode;
    tcIndxRef->errorData = 0;

    releaseIndexOperation(regApiPtr, indexOp);

    sendSignal(regApiPtr->ndbapiBlockref, 
               GSN_TCINDXREF, signal, TcKeyRef::SignalLength, JBB);
    return;
  }
  }
}

void Dbtc::execTRANSID_AI_R(Signal* signal){
  TransIdAI * const transIdAI =  (TransIdAI *)signal->getDataPtr();
  Uint32 sigLen = signal->length();
  Uint32 dataLen = sigLen - TransIdAI::HeaderLength - 1;
  Uint32 recBlockref = transIdAI->attrData[dataLen];

  jamEntry();

  SectionHandle handle(this, signal);

  /**
   * Forward signal to final destination
   * Truncate last word since that was used to hold the final dest.
   */
  sendSignal(recBlockref, GSN_TRANSID_AI,
	     signal, sigLen - 1, JBB,
	     &handle);
}

void Dbtc::execKEYINFO20_R(Signal* signal){
  KeyInfo20 * const keyInfo =  (KeyInfo20 *)signal->getDataPtr();
  Uint32 sigLen = signal->length();
  Uint32 dataLen = sigLen - KeyInfo20::HeaderLength - 1;
  Uint32 recBlockref = keyInfo->keyData[dataLen];

  jamEntry();

  SectionHandle handle(this, signal);
  
  /**
   * Forward signal to final destination
   * Truncate last word since that was used to hold the final dest.
   */
  sendSignal(recBlockref, GSN_KEYINFO20,
	     signal, sigLen - 1, JBB,
	     &handle);
}


/** 
 * execTRANSID_AI
 * 
 * TRANSID_AI are received as a result of performing a read on 
 * the index table as part of a (unique) index operation.
 * The data received is the primary key of the base table 
 * which is then used to perform the index operation on the
 * base table.
 */
void Dbtc::execTRANSID_AI(Signal* signal)
{
  TransIdAI * const transIdAI =  (TransIdAI *)signal->getDataPtr();

  jamEntry();
  TcIndexOperationPtr indexOpPtr;
  indexOpPtr.i = transIdAI->connectPtr;
  TcIndexOperation* indexOp = c_theIndexOperationPool.getPtr(indexOpPtr.i);
  indexOpPtr.p = indexOp;
  if (!indexOp) {
    jam();
    // Missing index operation
  }
  const UintR TconnectIndex = indexOp->connectionIndex;
  ApiConnectRecordPtr transPtr;
  
  transPtr.i = TconnectIndex;
  ptrCheckGuard(transPtr, capiConnectFilesize, apiConnectRecord);
  ApiConnectRecord * const regApiPtr = transPtr.p;

  // Acccumulate attribute data
  SectionHandle handle(this, signal);
  bool longSignal = (handle.m_cnt == 1);
  Uint32 errorCode = ZOK;
  if (longSignal)
  {
    SegmentedSectionPtr dataPtr;
    Uint32 dataLen;
    ndbrequire(handle.getSection(dataPtr, 0));
    dataLen = dataPtr.sz;

    SectionSegment * ptrP = dataPtr.p;
    while (dataLen > NDB_SECTION_SEGMENT_SZ)
    {
      errorCode = saveTRANSID_AI(signal, indexOp, &ptrP->theData[0],
                                 NDB_SECTION_SEGMENT_SZ);
      if (errorCode != ZOK)
      {
        releaseSections(handle);
        goto save_error;
      }
      dataLen -= NDB_SECTION_SEGMENT_SZ;
      ptrP = g_sectionSegmentPool.getPtr(ptrP->m_nextSegment);
    }
    errorCode = saveTRANSID_AI(signal, indexOp, &ptrP->theData[0], dataLen);
    if (errorCode != ZOK)
    {
      releaseSections(handle);
      goto save_error;
    }

    releaseSections(handle);
  }
  else
  {
    /* Short TransId_AI signal */
    errorCode = saveTRANSID_AI(signal,
                               indexOp,
                               transIdAI->getData(),
                               signal->getLength() - TransIdAI::HeaderLength);
    if (errorCode != ZOK) {
    save_error:
      jam();
      // Failed to allocate space for TransIdAI
      // Todo : How will this behave when transaction already aborted
      // in saveTRANSID_AI call?
      TcKeyRef * const tcIndxRef = (TcKeyRef *)signal->getDataPtrSend();

      tcIndxRef->connectPtr = indexOp->tcIndxReq.senderData;
      tcIndxRef->transId[0] = regApiPtr->transid[0];
      tcIndxRef->transId[1] = regApiPtr->transid[1];
      tcIndxRef->errorCode = errorCode;
      tcIndxRef->errorData = 0;
      sendSignal(regApiPtr->ndbapiBlockref, GSN_TCINDXREF, signal,
                 TcKeyRef::SignalLength, JBB);
      return;
    }
  }

  switch(indexOp->indexOpState) {
  case(IOS_NOOP): {
    jam();
    // Should never happen, abort
    TcKeyRef * const tcIndxRef = (TcKeyRef *)signal->getDataPtrSend();
    
    tcIndxRef->connectPtr = indexOp->tcIndxReq.senderData;
    tcIndxRef->transId[0] = regApiPtr->transid[0];
    tcIndxRef->transId[1] = regApiPtr->transid[1];
    tcIndxRef->errorCode = ZINCONSISTENT_INDEX_USE;
    tcIndxRef->errorData = 0;
    sendSignal(regApiPtr->ndbapiBlockref, GSN_TCINDXREF, signal, 
	       TcKeyRef::SignalLength, JBB);
    return;
    break;
  }
  case(IOS_INDEX_ACCESS): {
    jam();
    // Check if all TRANSID_AI have been received
    if (receivedAllTRANSID_AI(indexOp)) {
      jam();
      // Just waiting for a TCKEYCONF now
      indexOp->indexOpState = IOS_INDEX_ACCESS_WAIT_FOR_TCKEYCONF;
    }
    // else waiting for either TRANSID_AI or TCKEYCONF
    break;
    }
  case(IOS_INDEX_ACCESS_WAIT_FOR_TCKEYCONF): {
    jam();
#ifdef VM_TRACE
    ndbout_c("Dbtc::execTRANSID_AI: Too many TRANSID_AI, ignore for now\n");
#endif
    /*
    // Too many TRANSID_AI
    TcKeyRef * const tcIndxRef = (TcKeyRef *)signal->getDataPtrSend();
    
    tcIndexRef->connectPtr = indexOp->tcIndxReq.senderData;
    tcIndxRef->transId[0] = regApiPtr->transid[0];
    tcIndxRef->transId[1] = regApiPtr->transid[1];
    tcIndxRef->errorCode = 4349;
    tcIndxRef->errorData = 0;
    sendSignal(regApiPtr->ndbapiBlockref, GSN_TCINDXREF, signal, 
               TcKeyRef::SignalLength, JBB);
    */
    break;
  }
  case(IOS_INDEX_ACCESS_WAIT_FOR_TRANSID_AI): { 
    jam();
    // Check if all TRANSID_AI have been received
    if (receivedAllTRANSID_AI(indexOp)) {
      jam();
      // Continue with index operation
      executeIndexOperation(signal, regApiPtr, indexOp);
    }
    // else continue waiting for more TRANSID_AI
    break;
  }
  }
}

void Dbtc::execTCROLLBACKREP(Signal* signal)
{
  TcRollbackRep* tcRollbackRep =  (TcRollbackRep *)signal->getDataPtr();
  jamEntry();
  TcIndexOperationPtr indexOpPtr;
  indexOpPtr.i = tcRollbackRep->connectPtr;
  TcIndexOperation* indexOp = c_theIndexOperationPool.getPtr(indexOpPtr.i);
  indexOpPtr.p = indexOp;
  tcRollbackRep =  (TcRollbackRep *)signal->getDataPtrSend();
  tcRollbackRep->connectPtr = indexOp->tcIndxReq.senderData;
  ApiConnectRecordPtr apiConnectptr;
  apiConnectptr.i = indexOp->tcIndxReq.apiConnectPtr;
  ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
  sendSignal(apiConnectptr.p->ndbapiBlockref, 
	     GSN_TCROLLBACKREP, signal, TcRollbackRep::SignalLength, JBB);
}

/**
 * Read index table with the index attributes as PK
 */
void Dbtc::readIndexTable(Signal* signal,
                          ApiConnectRecordPtr transPtr,
                          TcIndexOperation* indexOp,
                          Uint32 special_op_flags)
{
  TcKeyReq * const tcKeyReq = (TcKeyReq *)signal->getDataPtrSend();
  Uint32 tcKeyRequestInfo = indexOp->tcIndxReq.requestInfo; 
  TcIndexDataPtr indexDataPtr;
  Uint32 transId1 = indexOp->tcIndxReq.transId1;
  Uint32 transId2 = indexOp->tcIndxReq.transId2;
  ApiConnectRecord* regApiPtr = transPtr.p;

  const Operation_t opType = 
    (Operation_t)TcKeyReq::getOperationType(tcKeyRequestInfo);

  // Find index table
  indexDataPtr.i = indexOp->tcIndxReq.tableId;
  /* Using a IgnoreAlloc variant of getPtr to make the lookup safe.
   * The validity of the index is checked subsequently using indexState. */
  c_theIndexes.getPool().getPtrIgnoreAlloc(indexDataPtr);
  if (indexDataPtr.p == NULL ||
      indexDataPtr.p->indexState == IS_OFFLINE )
  {
    /* The index was either null or was already dropped.
     * Abort the operation and release the resources. */
    jam();
    terrorCode = ZNO_SUCH_TABLE;
    apiConnectptr = transPtr;
    /* If the signal is last in the batch, don't wait for more
     * and enable sending the reply signal in abortErrorLab */
    regApiPtr->m_flags |=
      TcKeyReq::getExecuteFlag(tcKeyRequestInfo) ?
      ApiConnectRecord::TF_EXEC_FLAG : 0;
    abortErrorLab(signal);
    return;
  }
  TcIndexData* indexData = indexDataPtr.p;
  tcKeyReq->transId1 = transId1;
  tcKeyReq->transId2 = transId2;
  tcKeyReq->tableId = indexData->indexId;
  tcKeyReq->tableSchemaVersion = indexOp->tcIndxReq.tableSchemaVersion;
  TcKeyReq::setOperationType(tcKeyRequestInfo, 
			     opType == ZREAD ? ZREAD : ZREAD_EX);
  TcKeyReq::setAIInTcKeyReq(tcKeyRequestInfo, 0); // No AI in long TCKEYREQ
  TcKeyReq::setInterpretedFlag(tcKeyRequestInfo, 0);
  tcKeyReq->senderData = indexOp->indexOpId;
  indexOp->indexOpState = IOS_INDEX_ACCESS;
  regApiPtr->executingIndexOp = regApiPtr->accumulatingIndexOp;
  regApiPtr->accumulatingIndexOp = RNIL;
  regApiPtr->m_special_op_flags =
    TcConnectRecord::SOF_INDEX_TABLE_READ | special_op_flags;

  if (ERROR_INSERTED(8037))
  {
    ndbout_c("shifting index version");
    tcKeyReq->tableSchemaVersion = ~(Uint32)indexOp->tcIndxReq.tableSchemaVersion;
  }
  tcKeyReq->attrLen = 1; // Primary key is stored as one attribute
  tcKeyReq->requestInfo = tcKeyRequestInfo;

  ndbassert(TcKeyReq::getDirtyFlag(tcKeyRequestInfo) == 0);
  ndbassert(TcKeyReq::getSimpleFlag(tcKeyRequestInfo) == 0);

  /* Long TCKEYREQ Signal sections
   * We attach the KeyInfo section received from the user, and
   * create a new AttrInfo section with just one AttributeHeader
   * to retrieve the base table primary key
   */
  Ptr<SectionSegment> indexLookupAttrInfoSection;
  Uint32 singleAIWord;
  
  AttributeHeader::init(&singleAIWord, indexData->primaryKeyPos, 0);
  if (! import(indexLookupAttrInfoSection,
               &singleAIWord,
               1))
  {
    jam();
    /* Error creating AttrInfo section to request primary
     * key from index table.
     */
    // TODO - verify error handling
#ifdef VM_TRACE
    ndbout_c("Dbtc::readIndexTable: Failed to create AttrInfo section");
#endif
    apiConnectptr.i = indexOp->connectionIndex;
    ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
    releaseIndexOperation(apiConnectptr.p, indexOp);
    terrorCode = 4000;
    abortErrorLab(signal);
    return;
  }

  ndbassert(signal->header.m_noOfSections == 0);

  signal->m_sectionPtrI[ TcKeyReq::KeyInfoSectionNum ] 
    = indexOp->keyInfoSectionIVal;
  
  /* We pass this section to TCKEYREQ next */
  indexOp->keyInfoSectionIVal= RNIL;

  signal->m_sectionPtrI[ TcKeyReq::AttrInfoSectionNum ]
    = indexLookupAttrInfoSection.i;
  signal->header.m_noOfSections= 2;
  
  /* Direct execute of long TCKEYREQ 
   * TCKEYREQ is responsible for freeing the KeyInfo and
   * AttrInfo sections passed to it
   */
  EXECUTE_DIRECT(DBTC, GSN_TCKEYREQ, signal, TcKeyReq::StaticLength);
  jamEntry();

  if (unlikely(regApiPtr->apiConnectstate == CS_ABORTING))
  {
    jam();
  }
  else
  {
    jam();
    /**
     * "Fool" TC not to start commiting transaction since it always will
     *   have one outstanding lqhkeyreq
     * This is later decreased when the index read is complete
     */ 
    regApiPtr->lqhkeyreqrec++;
    
    /**
     * Remember ptr to index read operation
     *   (used to set correct save point id on index operation later)
     */
    indexOp->indexReadTcConnect = regApiPtr->lastTcConnect;
  }

  return;
}

/**
 * Execute the index operation with the result from
 * the index table read as PK
 */
void Dbtc::executeIndexOperation(Signal* signal, 
				 ApiConnectRecord* regApiPtr,
				 TcIndexOperation* indexOp) {
  
  TcKeyReq * const tcIndxReq = &indexOp->tcIndxReq;
  TcKeyReq * const tcKeyReq = (TcKeyReq *)signal->getDataPtrSend();
  Uint32 tcKeyRequestInfo = tcIndxReq->requestInfo;
  TcIndexData* indexData;
      
  // Find index table
  if ((indexData = c_theIndexes.getPtr(tcIndxReq->tableId)) == NULL) {
    jam();
    // Failed to find index record 
    // TODO : How is this operation cleaned up?
    TcKeyRef * const tcIndxRef = (TcKeyRef *)signal->getDataPtrSend();

    tcIndxRef->connectPtr = indexOp->tcIndxReq.senderData;
    tcIndxRef->transId[0] = regApiPtr->transid[0];
    tcIndxRef->transId[1] = regApiPtr->transid[1];
    tcIndxRef->errorCode = ZINCONSISTENT_INDEX_USE;    
    tcIndxRef->errorData = 0;
    sendSignal(regApiPtr->ndbapiBlockref, GSN_TCINDXREF, signal, 
	       TcKeyRef::SignalLength, JBB);
    return;
  }

  // Find schema version of primary table
  TableRecordPtr tabPtr;
  tabPtr.i = indexData->primaryTableId;
  ptrCheckGuard(tabPtr, ctabrecFilesize, tableRecord);

  tcKeyReq->apiConnectPtr = tcIndxReq->apiConnectPtr;
  tcKeyReq->attrLen = tcIndxReq->attrLen;
  tcKeyReq->tableId = indexData->primaryTableId;
  tcKeyReq->tableSchemaVersion = tabPtr.p->currentSchemaVersion;
  tcKeyReq->transId1 = regApiPtr->transid[0];
  tcKeyReq->transId2 = regApiPtr->transid[1];
  tcKeyReq->senderData = tcIndxReq->senderData; // Needed for TRANSID_AI to API
  
  if (tabPtr.p->get_user_defined_partitioning())
  {
    jam();
    tcKeyReq->scanInfo = indexOp->fragmentId; // As read from Index table
    TcKeyReq::setDistributionKeyFlag(tcKeyRequestInfo, 1U);
  }
  regApiPtr->m_special_op_flags = 0;
  regApiPtr->executingIndexOp = 0;

  /* KeyInfo section
   * Get the KeyInfo we received from the index table lookup
   */
  SegmentedSectionPtr keyInfoFromTransIdAI;
  
  ndbassert( indexOp->transIdAISectionIVal != RNIL );
  getSection(keyInfoFromTransIdAI, indexOp->transIdAISectionIVal);
  
  ndbassert( signal->header.m_noOfSections == 0 );
  signal->m_sectionPtrI[ TcKeyReq::KeyInfoSectionNum ]
    = indexOp->transIdAISectionIVal;
  signal->header.m_noOfSections = 1;
  
  indexOp->transIdAISectionIVal = RNIL;
  
  /* AttrInfo section
   * Attach any AttrInfo section from original TCINDXREQ
   */
  if ( indexOp->attrInfoSectionIVal != RNIL )
  {
    jam();
    SegmentedSectionPtr attrInfoFromInitialReq;

    getSection(attrInfoFromInitialReq, indexOp->attrInfoSectionIVal);
    signal->m_sectionPtrI[ TcKeyReq::AttrInfoSectionNum ]
      = indexOp->attrInfoSectionIVal;
    signal->header.m_noOfSections = 2;
    indexOp->attrInfoSectionIVal = RNIL;
  }

  releaseIndexOperation(regApiPtr, indexOp);

  TcKeyReq::setKeyLength(tcKeyRequestInfo, keyInfoFromTransIdAI.sz);
  TcKeyReq::setAIInTcKeyReq(tcKeyRequestInfo, 0);
  TcKeyReq::setCommitFlag(tcKeyRequestInfo, 0);
  TcKeyReq::setExecuteFlag(tcKeyRequestInfo, 0);
  tcKeyReq->requestInfo = tcKeyRequestInfo;

  ndbassert(TcKeyReq::getDirtyFlag(tcKeyRequestInfo) == 0);
  ndbassert(TcKeyReq::getSimpleFlag(tcKeyRequestInfo) == 0);

  /**
   * Decrease lqhkeyreqrec to compensate for addition
   *   during read of index table
   * I.e. let TC start committing when other operations has completed
   */
  regApiPtr->lqhkeyreqrec--;

  /**
   * Fix savepoint id -
   *   fix so that index operation has the same savepoint id
   *   as the read of the index table (TCINDXREQ)
   */
  TcConnectRecordPtr tmp;
  tmp.i = indexOp->indexReadTcConnect;
  ptrCheckGuard(tmp, ctcConnectFilesize, tcConnectRecord);
  const Uint32 currSavePointId = regApiPtr->currSavePointId;
  const Uint32 triggeringOp = tmp.p->triggeringOperation;
  const Uint32 triggerId = tmp.p->currentTriggerId;
  regApiPtr->currSavePointId = tmp.p->savePointId;

#ifdef ERROR_INSERT
  bool err8072 = ERROR_INSERTED(8072);
  if (err8072)
  {
    CLEAR_ERROR_INSERT_VALUE;
  }
#endif

  if (triggeringOp != RNIL)
  {
    jam();
    /**
     * Carry forward info that this was caused by trigger
     *   (used by FK)
     */
    ndbassert(triggerId != RNIL);
    ndbassert(tcIndxReq->senderData == triggeringOp);
    regApiPtr->m_special_op_flags = TcConnectRecord::SOF_TRIGGER;
    regApiPtr->immediateTriggerId = triggerId;
  }

  /* Execute TCKEYREQ now - it is now responsible for freeing
   * the KeyInfo and AttrInfo sections 
   */
  EXECUTE_DIRECT(DBTC, GSN_TCKEYREQ, signal, TcKeyReq::StaticLength);
  jamEntry();

#ifdef ERROR_INSERT
  if (err8072)
  {
    SET_ERROR_INSERT_VALUE(8072);
  }
#endif

  if (unlikely(regApiPtr->apiConnectstate == CS_ABORTING))
  {
    // TODO : Presumably the abort cleans up the operation
    jam();
    return;
  }

  regApiPtr->currSavePointId = currSavePointId;
  regApiPtr->immediateTriggerId = RNIL;
}

bool Dbtc::seizeIndexOperation(ApiConnectRecord* regApiPtr,
			       TcIndexOperationPtr& indexOpPtr)
{
  if (regApiPtr->theSeizedIndexOperations.seizeFirst(indexOpPtr))
  {
    jam();
    ndbassert(indexOpPtr.p->pendingKeyInfo == 0);
    ndbassert(indexOpPtr.p->keyInfoSectionIVal == RNIL);
    ndbassert(indexOpPtr.p->pendingAttrInfo == 0);
    ndbassert(indexOpPtr.p->attrInfoSectionIVal == RNIL);
    ndbassert(indexOpPtr.p->transIdAIState == ITAS_WAIT_HEADER);
    ndbassert(indexOpPtr.p->pendingTransIdAI == 0);
    ndbassert(indexOpPtr.p->transIdAISectionIVal == RNIL);
    ndbassert(indexOpPtr.p->savedFlags == 0);
    return true;
  }
  jam();
  return false;
}

void Dbtc::releaseIndexOperation(ApiConnectRecord* regApiPtr,
				 TcIndexOperation* indexOp)
{
  indexOp->indexOpState = IOS_NOOP;
  indexOp->pendingKeyInfo = 0;
  releaseSection(indexOp->keyInfoSectionIVal);
  indexOp->keyInfoSectionIVal= RNIL;
  indexOp->pendingAttrInfo = 0;
  releaseSection(indexOp->attrInfoSectionIVal);
  indexOp->attrInfoSectionIVal= RNIL;
  indexOp->transIdAIState = ITAS_WAIT_HEADER;
  indexOp->pendingTransIdAI = 0;
  releaseSection(indexOp->transIdAISectionIVal);
  indexOp->transIdAISectionIVal= RNIL;
  indexOp->savedFlags= 0;
  regApiPtr->theSeizedIndexOperations.release(indexOp->indexOpId);
}

void Dbtc::releaseAllSeizedIndexOperations(ApiConnectRecord* regApiPtr)
{
  TcIndexOperationPtr seizedIndexOpPtr;

  regApiPtr->theSeizedIndexOperations.first(seizedIndexOpPtr);
  while(seizedIndexOpPtr.i != RNIL) {
    jam();
    TcIndexOperation* indexOp = seizedIndexOpPtr.p;

    indexOp->indexOpState = IOS_NOOP;
    indexOp->pendingKeyInfo = 0;
    releaseSection(indexOp->keyInfoSectionIVal);
    indexOp->keyInfoSectionIVal = RNIL;
    indexOp->pendingAttrInfo = 0;
    releaseSection(indexOp->attrInfoSectionIVal);
    indexOp->attrInfoSectionIVal = RNIL;
    indexOp->transIdAIState = ITAS_WAIT_HEADER;
    indexOp->pendingTransIdAI = 0;
    releaseSection(indexOp->transIdAISectionIVal);
    indexOp->transIdAISectionIVal = RNIL;
    indexOp->savedFlags= 0;
    regApiPtr->theSeizedIndexOperations.next(seizedIndexOpPtr);
  }
  jam();
  while (regApiPtr->theSeizedIndexOperations.releaseFirst())
  {
    ;
  }
  jam();
}

void Dbtc::saveTriggeringOpState(Signal* signal, TcConnectRecord* trigOp)
{
  LqhKeyConf * lqhKeyConf = (LqhKeyConf *)signal->getDataPtr();
  copyFromToLen((UintR*)lqhKeyConf,
		&trigOp->savedState[0],
                LqhKeyConf::SignalLength);  
}

void
Dbtc::trigger_op_finished(Signal* signal,
                          ApiConnectRecordPtr regApiPtr,
                          Uint32 trigPtrI,
                          TcConnectRecord* triggeringOp,
                          Uint32 errCode)
{
  if (trigPtrI != RNIL)
  {
    jam();
    Ptr<TcDefinedTriggerData> trigPtr;
    c_theDefinedTriggers.getPtr(trigPtr, trigPtrI);
    switch(trigPtr.p->triggerType){
    case TriggerType::FK_PARENT:
    {
      if (errCode == ZNOT_FOUND)
      {
        jam();
        break; // good!
      }

      Ptr<TcFKData> fkPtr;
      // TODO make it a pool.getPtr() instead
      // by also adding fk_ptr_i to definedTriggerData
      ndbrequire(c_fk_hash.find(fkPtr, trigPtr.p->fkId));
      if (errCode == 0 && ((fkPtr.p->bits&CreateFKImplReq::FK_ON_ACTION) == 0))
      {
        jam();
        // Only restrict
        terrorCode = ZFK_CHILD_ROW_EXISTS;
        regApiPtr.p->errorData = trigPtr.p->fkId;
      }
      else if (errCode == 0)
      {
        /**
         * Check action performed against expected result...
         */
        if (triggeringOp->operation == ZDELETE &&
            (fkPtr.p->bits & CreateFKImplReq::FK_DELETE_ACTION))
        {
          jam();
          // the on action succeeded, good!
          break;
        }
        else if ((triggeringOp->operation == ZUPDATE || triggeringOp->operation == ZWRITE) &&
                 (fkPtr.p->bits & CreateFKImplReq::FK_UPDATE_ACTION))
        {
          jam();
          // the on action succeeded, good!
          break;
        }
        jam();
        terrorCode = ZFK_CHILD_ROW_EXISTS;
        regApiPtr.p->errorData = trigPtr.p->fkId;
      }
      else
      {
        jam();
        jamLine(errCode);
        terrorCode = errCode;
      }
      apiConnectptr = regApiPtr;
      abortErrorLab(signal);
      return;
    }
    default:
      (void)1;
    }
  }
  if (!regApiPtr.p->isExecutingDeferredTriggers())
  {
    jam();
    if (unlikely((triggeringOp->triggerExecutionCount == 0)))
    {
      printf("%u : 0x%x->triggerExecutionCount == 0\n",
             __LINE__,
             Uint32(triggeringOp - this->tcConnectRecord));
      dump_trans(regApiPtr);
    }
    ndbrequire(triggeringOp->triggerExecutionCount > 0);
    triggeringOp->triggerExecutionCount--;
    if (triggeringOp->triggerExecutionCount == 0)
    {
      /**
       * We have completed current trigger execution
       * Continue triggering operation
       */
      jam();
      continueTriggeringOp(signal, triggeringOp, regApiPtr);
    }
  }
  else
  {
    jam();
    lqhKeyConf_checkTransactionState(signal, regApiPtr);
  }
}

void Dbtc::continueTriggeringOp(Signal* signal,
                                TcConnectRecord* trigOp,
                                ApiConnectRecordPtr regApiPtr)
{
  LqhKeyConf * lqhKeyConf = (LqhKeyConf *)signal->getDataPtr();
  copyFromToLen(&trigOp->savedState[0],
                (UintR*)lqhKeyConf,
		LqhKeyConf::SignalLength);

  if (unlikely(trigOp->savedState[LqhKeyConf::SignalLength-1] == ~Uint32(0)))
  {
    g_eventLogger->info("%u : savedState not set\n", __LINE__);
    printLQHKEYCONF(stderr,signal->getDataPtr(),LqhKeyConf::SignalLength,DBTC);
    dump_trans(regApiPtr);
  }
  ndbrequire(trigOp->savedState[LqhKeyConf::SignalLength-1] != ~Uint32(0));
  trigOp->savedState[LqhKeyConf::SignalLength-1] = ~Uint32(0);

  lqhKeyConf->numFiredTriggers = 0;
  trigOp->numReceivedTriggers = 0;

  /**
   * All triggers executed successfully, continue operation
   * 
   * We have to be careful here not sending too many direct signals in a row.
   * This has two consequences, first we are breaking the coding rules if we
   * do and thus other signals have a hard time to get their piece of the
   * CPU.
   * Secondly we can easily run out of stack which can cause all sorts of
   * weird errors. We cannot allow any type of completely recursive behaviour
   * in the NDB code.
   */
  c_lqhkeyconf_direct_sent++;
  if (c_lqhkeyconf_direct_sent <= 5)
  {
    jam();
    execLQHKEYCONF(signal);
  }
  else
  {
    jam();
    c_lqhkeyconf_direct_sent = 0;
    sendSignal(reference(),
               GSN_LQHKEYCONF,
               signal,
               LqhKeyConf::SignalLength,
               JBB);
  }
}

void Dbtc::executeTriggers(Signal* signal, ApiConnectRecordPtr* transPtr)
{
  ApiConnectRecord* regApiPtr = transPtr->p;
  TcConnectRecord *localTcConnectRecord = tcConnectRecord;
  TcConnectRecordPtr opPtr;
  FiredTriggerPtr trigPtr;

  if (!regApiPtr->theFiredTriggers.isEmpty()) {
    jam();
    if ((regApiPtr->apiConnectstate == CS_STARTED) ||
        (regApiPtr->apiConnectstate == CS_START_COMMITTING) ||
        (regApiPtr->apiConnectstate == CS_SEND_FIRE_TRIG_REQ) ||
        (regApiPtr->apiConnectstate == CS_WAIT_FIRE_TRIG_REQ))
    {
      jam();
      regApiPtr->theFiredTriggers.first(trigPtr);
      while (trigPtr.i != RNIL) {
        jam();
        if (regApiPtr->cascading_scans_count >=
            MaxCascadingScansPerTransaction)
        {
          jam();
          D("trans: cascading scans " << regApiPtr->cascading_scans_count);
          waitToExecutePendingTrigger(signal, *transPtr);
          // pause all trigger execution
          break;
        }
        // Execute all ready triggers in parallel
        opPtr.i = trigPtr.p->fireingOperation;
        ptrCheckGuard(opPtr, ctcConnectFilesize, localTcConnectRecord);
	FiredTriggerPtr nextTrigPtr = trigPtr;
	regApiPtr->theFiredTriggers.next(nextTrigPtr);
        ndbrequire(opPtr.p->apiConnect == transPtr->i);

        if (opPtr.p->numReceivedTriggers == opPtr.p->numFiredTriggers ||
            regApiPtr->isExecutingDeferredTriggers())
        {
          jam();
          // Fireing operation is ready to have a trigger executing
          while (executeTrigger(signal, trigPtr.p, transPtr, &opPtr) == false)
          {
            jam();
            /**
             * TODO: timeslice, e.g CONTINUE
             */
          }

          // Should allow for interleaving here by sending a CONTINUEB and 
	  // return
          // Release trigger records
	  AttributeBuffer::DataBufferPool & pool = c_theAttributeBufferPool;
	  LocalAttributeBuffer tmp1(pool, trigPtr.p->keyValues);
	  tmp1.release();
	  LocalAttributeBuffer tmp2(pool, trigPtr.p->beforeValues);
	  tmp2.release();
	  LocalAttributeBuffer tmp3(pool, trigPtr.p->afterValues);
	  tmp3.release();
          regApiPtr->theFiredTriggers.release(trigPtr);
        }
	trigPtr = nextTrigPtr;
      }
      return;
    // No more triggers, continue transaction after last executed trigger has
    // reurned (in execLQHKEYCONF or execLQHKEYREF)
    } else {

      jam();
      /* Not in correct state to fire triggers yet, need to wait
       * (or keep waiting)
       */

      if ((regApiPtr->apiConnectstate == CS_RECEIVING) ||
          (regApiPtr->apiConnectstate == CS_REC_COMMITTING))
      {
        // Wait until transaction is ready to execute a trigger
        jam();
        D("trans: apiConnectstate " << regApiPtr->apiConnectstate);
        waitToExecutePendingTrigger(signal, *transPtr);
      }
      else
      {
        /* Transaction has started aborting.  
         * Forget about unprocessed triggers
         */
        ndbrequire(regApiPtr->apiConnectstate == CS_ABORTING);
      }
    }
  }
}

void
Dbtc::waitToExecutePendingTrigger(Signal* signal, ApiConnectRecordPtr transPtr)
{
  if (!tc_testbit(transPtr.p->m_flags,
                  ApiConnectRecord::TF_TRIGGER_PENDING))
  {
    jam();
    D("trans: send trigger pending");
    c_lqhkeyconf_direct_sent = 0;
    transPtr.p->m_flags |= ApiConnectRecord::TF_TRIGGER_PENDING;
    signal->theData[0] = TcContinueB::TRIGGER_PENDING;
    signal->theData[1] = transPtr.i;
    signal->theData[2] = transPtr.p->transid[0];
    signal->theData[3] = transPtr.p->transid[1];
    sendSignal(reference(), GSN_CONTINUEB, signal, 4, JBB);
  }
  else
  {
    // We are already waiting for a pending trigger (CONTINUEB)
    D("trans: trigger pending already");
  }
}

bool Dbtc::executeTrigger(Signal* signal,
                          TcFiredTriggerData* firedTriggerData,
                          ApiConnectRecordPtr* transPtr,
                          TcConnectRecordPtr* opPtr)
{
  TcDefinedTriggerData* definedTriggerData;

  if ((definedTriggerData = 
       c_theDefinedTriggers.getPtr(firedTriggerData->triggerId)) 
      != NULL)
  {
    transPtr->p->pendingTriggers--;
    switch(firedTriggerData->triggerType) {
    case(TriggerType::SECONDARY_INDEX):
      jam();
      executeIndexTrigger(signal, definedTriggerData, firedTriggerData, 
                          transPtr, opPtr);
      break;
    case TriggerType::REORG_TRIGGER:
      jam();
      executeReorgTrigger(signal, definedTriggerData, firedTriggerData,
                          transPtr, opPtr);
      break;
    case TriggerType::FK_PARENT:
      jam();
      executeFKParentTrigger(signal, definedTriggerData, firedTriggerData,
                             transPtr, opPtr);
      break;
    case TriggerType::FK_CHILD:
      jam();
      executeFKChildTrigger(signal, definedTriggerData, firedTriggerData,
                            transPtr, opPtr);
      break;
    case TriggerType::FULLY_REPLICATED_TRIGGER:
      jam();
      return executeFullyReplicatedTrigger(signal,
                                           definedTriggerData, firedTriggerData,
                                           transPtr, opPtr);
      break;
    default:
      ndbabort();
    }
  }
  return true;
}

void Dbtc::executeIndexTrigger(Signal* signal,
                               TcDefinedTriggerData* definedTriggerData,
                               TcFiredTriggerData* firedTriggerData,
                               ApiConnectRecordPtr* transPtr,
                               TcConnectRecordPtr* opPtr)
{
  TcIndexData* indexData = c_theIndexes.getPtr(definedTriggerData->indexId);
  ndbassert(indexData != NULL);

  switch (firedTriggerData->triggerEvent) {
  case(TriggerEvent::TE_INSERT): {
    jam();
    insertIntoIndexTable(signal, firedTriggerData, transPtr, opPtr, indexData);
    break;
  }
  case(TriggerEvent::TE_DELETE): {
    jam();
    deleteFromIndexTable(signal, firedTriggerData, transPtr, opPtr, indexData);
    break;
  }
  case(TriggerEvent::TE_UPDATE): {
    jam();
    opPtr->p->triggerExecutionCount++; // One is already added...and this is 2
    deleteFromIndexTable(signal, firedTriggerData, transPtr, opPtr, indexData);
    insertIntoIndexTable(signal, firedTriggerData, transPtr, opPtr, indexData);
    break;
  }
  default:
    ndbabort();
  }
}

Uint32
Dbtc::fk_constructAttrInfoSetNull(const TcFKData * fkPtrP)
{
  Uint32 attrInfo[MAX_ATTRIBUTES_IN_INDEX];
  for (Uint32 i = 0; i<fkPtrP->childTableColumns.sz; i++)
  {
    AttributeHeader::init(attrInfo + i, fkPtrP->childTableColumns.id[i],
                          0 /* setNull */);
  }

  Uint32 tmp = RNIL;
  if (ERROR_INSERTED(8106))
  {
    return tmp;
  }

  appendToSection(tmp, attrInfo, fkPtrP->childTableColumns.sz);
  return tmp;
}

Uint32
Dbtc::fk_constructAttrInfoUpdateCascade(const TcFKData * fkPtrP,
                                        AttributeBuffer::Head & srchead)
{
  Uint32 tmp = RNIL;
  if (ERROR_INSERTED(8103))
  {
    return tmp;
  }

  /**
   * Construct an update based on the src-data
   *
   * NOTE: this assumes same order...
   */
  Uint32 pos = 0;
  AttributeBuffer::DataBufferIterator iter;
  LocalAttributeBuffer src(c_theAttributeBufferPool, srchead);
  bool moreData= src.first(iter);
  const Uint32 segSize= src.getSegmentSize(); // 11

  while (moreData)
  {
    AttributeHeader* attrHeader = (AttributeHeader *) iter.data;
    Uint32 dataSize = attrHeader->getDataSize();

    AttributeHeader ah(*iter.data);
    ah.setAttributeId(fkPtrP->childTableColumns.id[pos++]);// Renumber AttrIds
    if (unlikely(!appendToSection(tmp, &ah.m_value, 1)))
    {
      releaseSection(tmp);
      return RNIL;
    }

    moreData = src.next(iter, 1);
    while (dataSize)
    {
      ndbrequire(moreData);
      /* Copy as many contiguous words as possible */
      Uint32 contigLeft = segSize - iter.ind;
      ndbassert(contigLeft);
      Uint32 contigValid = MIN(dataSize, contigLeft);

      if (unlikely(!appendToSection(tmp, iter.data, contigValid)))
      {
        releaseSection(tmp);
        return RNIL;
      }
      moreData = src.next(iter, contigValid);
      dataSize -= contigValid;
    }
  }

  return tmp;
}

void
Dbtc::executeFKParentTrigger(Signal* signal,
                             TcDefinedTriggerData* definedTriggerData,
                             TcFiredTriggerData* firedTriggerData,
                             ApiConnectRecordPtr* transPtr,
                             TcConnectRecordPtr* opPtr)
{
  Ptr<TcFKData> fkPtr;
  // TODO make it a pool.getPtr() instead
  // by also adding fk_ptr_i to definedTriggerData
  ndbrequire(c_fk_hash.find(fkPtr, definedTriggerData->fkId));

  /**
   * The parent table (the referenced table in the foreign key definition
   * was updated or had a row (no parent trigger on insert) deleted. This
   * could lead to the action CASCADE or SET NULL.
   *
   * CASCADE means that in the case of an update we will update the
   * referenced row with the new updated reference attributes.
   * In the case of DELETE CASCADE a delete in the parent table will
   * also be followed by a DELETE in the referencing table (child
   * table).
   *
   * In the case of SET NULL for UPDATE and DELETE we will set the
   * referencing attributes in the child table to NULL as part of
   * trigger execution.
   *
   * SET DEFAULT isn't supported by MySQL at the moment, so no special
   * code is needed to handle that.
   *
   * RESTRICT and NO ACTION both leads to read operations of the child
   * table to ensure that the references are valid, if they are not
   * found then the transaction is aborted.
   */
  Uint32 op = ZREAD;
  Uint32 attrValuesPtrI = RNIL;
  switch(firedTriggerData->triggerEvent)
  {
  case TriggerEvent::TE_UPDATE:
    if (fkPtr.p->bits & CreateFKImplReq::FK_UPDATE_CASCADE)
    {
      jam();
      /**
       * Update child table with after values of parent
       */
      op = ZUPDATE;
      attrValuesPtrI =
        fk_constructAttrInfoUpdateCascade(fkPtr.p,
                                          firedTriggerData->afterValues);

      if (unlikely(attrValuesPtrI == RNIL))
        goto oom;
    }
    else if (fkPtr.p->bits & CreateFKImplReq::FK_UPDATE_SET_NULL)
    {
      jam();
      /**
       * Update child table set null
       */
      goto setnull;
    }
    break;
  case TriggerEvent::TE_DELETE:
    if (fkPtr.p->bits & CreateFKImplReq::FK_DELETE_CASCADE)
    {
      jam();
      /**
       * Delete from child table
       */
      op = ZDELETE;
    }
    else if (fkPtr.p->bits & CreateFKImplReq::FK_DELETE_SET_NULL)
    {
      jam();
      /**
       * Update child table set null
       */
      goto setnull;
    }
    break;
  default:
    ndbabort();
  setnull:
  {
    op = ZUPDATE;
    attrValuesPtrI = fk_constructAttrInfoSetNull(fkPtr.p);
    if (unlikely(attrValuesPtrI == RNIL))
      goto oom;
  }
  }

  /**
   * Foreign key parent triggers can only exist with indexes.
   * If no bit is set then the index is a primary key, if
   * no primary index exists for the reference then a unique
   * index is choosen in which case the bit FK_CHILD_UI is set.
   * If neither a primary index nor a unique index is present
   * then an ordered index is used in which case the
   * bit FK_CHILD_OI is set. If neither an ordered index is present
   * then the foreign key creation wil fail, so here this cannot
   * happen.
   *
   * The index is on the foreign key child table, that is it is
   * required on the table that defines the foreign key.
   */
  if (! (fkPtr.p->bits & CreateFKImplReq::FK_CHILD_OI))
  {
    jam();
    fk_readFromChildTable(signal, firedTriggerData, transPtr, opPtr,
                          fkPtr.p, op, attrValuesPtrI);
  }
  else
  {
    jam();
    fk_scanFromChildTable(signal, firedTriggerData, transPtr, opPtr->i,
                          fkPtr.p, op, attrValuesPtrI);
  }
  return;
oom:
  jam();
  abortTransFromTrigger(signal, *transPtr, ZGET_DATAREC_ERROR);
}

void
Dbtc::fk_readFromChildTable(Signal* signal,
                            TcFiredTriggerData* firedTriggerData,
                            ApiConnectRecordPtr* transPtr,
                            TcConnectRecordPtr* opPtr,
                            TcFKData* fkData,
                            Uint32 op,
                            Uint32 attrValuesPtrI)
{
  ApiConnectRecord* regApiPtr = transPtr->p;
  TcConnectRecord* opRecord = opPtr->p;
  TcKeyReq * const tcKeyReq =  (TcKeyReq *)signal->getDataPtrSend();
  Uint32 tcKeyRequestInfo = 0;
  TableRecordPtr childIndexPtr;

  childIndexPtr.i = fkData->childIndexId;
  ptrCheckGuard(childIndexPtr, ctabrecFilesize, tableRecord);
  tcKeyReq->apiConnectPtr = transPtr->i;
  tcKeyReq->senderData = opPtr->i;

  // Calculate key length and renumber attribute id:s
  AttributeBuffer::DataBufferPool & pool = c_theAttributeBufferPool;
  LocalAttributeBuffer beforeValues(pool, firedTriggerData->beforeValues);

  SegmentedSectionGuard guard(this, attrValuesPtrI);
  if (beforeValues.getSize() == 0)
  {
    jam();
    ndbrequire(tc_testbit(regApiPtr->m_flags,
                          ApiConnectRecord::TF_DEFERRED_CONSTRAINTS));
    trigger_op_finished(signal, *transPtr, RNIL, opRecord, 0);
    return;
  }

  Uint32 keyIVal= RNIL;
  bool hasNull= false;
  Uint32 err = fk_buildKeyInfo(keyIVal, hasNull, beforeValues, fkData, false);
  guard.add(keyIVal);
  if (unlikely(err != 0))
  {
    jam();
    abortTransFromTrigger(signal, *transPtr, err);
    return;
  }

  /* If there's Nulls in the values that become the index table's
   * PK then we skip this delete
   */
  if (hasNull)
  {
    jam();
    trigger_op_finished(signal, *transPtr, RNIL, opRecord, 0);
    return;
  }

  /**
   * Access child does never need lock...
   *   as parent is read with lock
   */
  Uint16 flags = TcConnectRecord::SOF_TRIGGER;
  const Uint32 currSavePointId = regApiPtr->currSavePointId;
  Uint32 usedSavePointId = currSavePointId;
  Uint32 gsn = GSN_TCKEYREQ;
  if (op == ZREAD)
  {
    jam();
    flags |= TcConnectRecord::SOF_FK_READ_COMMITTED;
    TcKeyReq::setSimpleFlag(tcKeyRequestInfo, 1);
  }
  else
  {
    jam();
    /**
     * Let any DML be made with same save point
     *   as original DML...but ZREAD reads latest
     */
    usedSavePointId = opRecord->savePointId;
    if (fkData->childTableId != fkData->childIndexId)
    {
      jam();
      gsn = GSN_TCINDXREQ;
    }
  }
  TcKeyReq::setKeyLength(tcKeyRequestInfo, 0);
  TcKeyReq::setAIInTcKeyReq(tcKeyRequestInfo, 0);
  tcKeyReq->attrLen = 0;
  tcKeyReq->tableId = childIndexPtr.i;
  TcKeyReq::setOperationType(tcKeyRequestInfo, op);
  tcKeyReq->tableSchemaVersion = childIndexPtr.p->currentSchemaVersion;
  tcKeyReq->transId1 = regApiPtr->transid[0];
  tcKeyReq->transId2 = regApiPtr->transid[1];
  tcKeyReq->requestInfo = tcKeyRequestInfo;

  /* Attach KeyInfo section to signal */
  ndbrequire(signal->header.m_noOfSections == 0);
  signal->m_sectionPtrI[ TcKeyReq::KeyInfoSectionNum ] = keyIVal;
  signal->header.m_noOfSections = 1;

  if (attrValuesPtrI != RNIL)
  {
    jam();
    signal->m_sectionPtrI[ TcKeyReq::AttrInfoSectionNum ] = attrValuesPtrI;
    signal->header.m_noOfSections = 2;
  }

  guard.clear(); // now sections will be handled...

  /**
   * Handle savepoint id - (see above)
   */
  regApiPtr->currSavePointId = usedSavePointId;
  regApiPtr->m_special_op_flags = flags;
  /* Pass trigger Id via ApiConnectRecord (nasty) */
  ndbrequire(regApiPtr->immediateTriggerId == RNIL);
  regApiPtr->immediateTriggerId= firedTriggerData->triggerId;
  if (gsn == GSN_TCKEYREQ)
  {
    EXECUTE_DIRECT(DBTC, GSN_TCKEYREQ, signal, TcKeyReq::StaticLength);
  }
  else
  {
    fk_execTCINDXREQ(signal, * transPtr, * opPtr, op);
  }
  jamEntry();

  /*
   * Restore ApiConnectRecord state
   */
  regApiPtr->immediateTriggerId = RNIL;
  regApiPtr->currSavePointId = currSavePointId;
}

void
Dbtc::fk_execTCINDXREQ(Signal* signal,
                       ApiConnectRecordPtr transPtr,
                       TcConnectRecordPtr opPtr,
                       Uint32 operation)
{
  jam();
  SectionHandle handle(this, signal);
  const TcKeyReq * tcIndxReq =  CAST_CONSTPTR(TcKeyReq,
                                              signal->getDataPtr());

  /**
   * this is a mockup of execTCINDXREQ...
   */
  TcIndexOperationPtr indexOpPtr;
  if (unlikely(!seizeIndexOperation(transPtr.p, indexOpPtr)))
  {
    jam();
    releaseSections(handle);
    abortTransFromTrigger(signal, transPtr, 288);
    return;
  }

  indexOpPtr.p->indexOpId = indexOpPtr.i;

  // Save original signal
  indexOpPtr.p->tcIndxReq = *tcIndxReq;
  indexOpPtr.p->connectionIndex = transPtr.i;
  transPtr.p->accumulatingIndexOp = indexOpPtr.i;

  /* KeyInfo and AttrInfo already received into sections */
  SegmentedSectionPtr keyInfoSection, attrInfoSection;

  /* Store i value for first long section of KeyInfo
   * and AttrInfo in Index operation
   */
  handle.getSection(keyInfoSection, TcKeyReq::KeyInfoSectionNum);
  indexOpPtr.p->keyInfoSectionIVal = keyInfoSection.i;

  if (handle.m_cnt == 2)
  {
    handle.getSection(attrInfoSection, TcKeyReq::AttrInfoSectionNum);
    indexOpPtr.p->attrInfoSectionIVal = attrInfoSection.i;
  }

  /* Detach sections from the handle
   * Success path code, or index operation cleanup is
   * now responsible for freeing the sections
   */
  handle.clear();

  /* All data received, process */
  readIndexTable(signal, transPtr, indexOpPtr.p,
                 transPtr.p->m_special_op_flags);

  if (unlikely(transPtr.p->apiConnectstate == CS_ABORTING))
  {
    jam();
  }
  else
  {
    /**
     * readIndexTable() sets senderData to indexOpPtr.i
     * and SOF_TRIGGER assumes triggerOperation is stored in senderData
     *   so set it correct afterwards...
     */
    jam();
    TcConnectRecordPtr tcPtr;
    tcPtr.i = indexOpPtr.p->indexReadTcConnect;
    ptrCheckGuard(tcPtr, ctcConnectFilesize, tcConnectRecord);
    tcPtr.p->triggeringOperation = opPtr.i;
    ndbrequire(hasOp(transPtr, opPtr.i));
  }
  return;
}

Uint32
Dbtc::fk_buildKeyInfo(Uint32& keyIVal, bool& hasNull,
                      LocalAttributeBuffer & values,
                      TcFKData * fkPtrP,
                      bool parent)
{
  if (ERROR_INSERTED(8104))
  {
    return ZGET_DATAREC_ERROR;
  }

  IndexAttributeList * list = 0;
  if (parent == true)
  {
    jam();
    list = &fkPtrP->childTableColumns;
  }
  else
  {
    jam();
    list = &fkPtrP->parentTableColumns;
  }

  AttributeBuffer::DataBufferIterator iter;
  bool eof = !values.first(iter);
  for (Uint32 i = 0; i < list->sz; i++)
  {
    Uint32 col = list->id[i];
    if (!eof && AttributeHeader(* iter.data).getAttributeId() == col)
    {
  found:
      Uint32 len = AttributeHeader(* iter.data).getDataSize();
      if (len == 0)
      {
        hasNull = true;
        return 0;
      }
      eof = !values.next(iter);
      Uint32 err = appendDataToSection(keyIVal, values, iter,
                                       len);
      if (unlikely(err != 0))
        return err;

      eof = iter.isNull();
    }
    else
    {
      /**
       * Search for column...
       */
      eof = !values.first(iter);
      while (!eof && AttributeHeader(* iter.data).getAttributeId() != col)
      {
        eof = !values.next(iter,
                           1 + AttributeHeader(* iter.data).getDataSize());
      }
      if (unlikely(eof))
      {
        return ZMISSING_TRIGGER_DATA;
      }
      ndbassert(AttributeHeader(* iter.data).getAttributeId() == col);
      goto found;
    }
  }

  return 0;
}

#define SCAN_FROM_CHILD_PARALLELISM 4

void
Dbtc::fk_scanFromChildTable(Signal* signal,
                            TcFiredTriggerData* firedTriggerData,
                            ApiConnectRecordPtr* transPtr,
                            const Uint32 opPtrI,
                            TcFKData* fkData,
                            Uint32 op,
                            Uint32 attrValuesPtrI)
{
  ApiConnectRecord* regApiPtr = transPtr->p;

  SegmentedSectionGuard guard(this, attrValuesPtrI);

  if (unlikely(cfirstfreeScanrec == RNIL))
  {
    jam();
    abortTransFromTrigger(signal, *transPtr, ZNO_SCANREC_ERROR);
    return;
  }

  if (unlikely(cfirstfreeTcConnect == RNIL))
  {
    jam();
    abortTransFromTrigger(signal, *transPtr, ZNO_FREE_TC_CONNECTION);
    return;
  }

  seizeApiConnect(signal);
  if (unlikely(terrorCode != ZOK))
  {
    jam();
    abortTransFromTrigger(signal, *transPtr, terrorCode);
    return;
  }

  // seize a TcConnectRecord to keep track of trigger stuff
  seizeTcConnect(0);
  TcConnectRecordPtr tcPtr = tcConnectptr;

  // Reuse the ApiConnectRecordPtr, set this TC as (internal) 'API-client' 
  ApiConnectRecordPtr scanApiConnectPtr = apiConnectptr;
  scanApiConnectPtr.p->ndbapiBlockref = reference();
  scanApiConnectPtr.p->ndbapiConnect = tcPtr.i;

  tcPtr.p->apiConnect = scanApiConnectPtr.i;
  ndbrequire(hasOp(* transPtr, opPtrI));
  tcPtr.p->triggeringOperation = opPtrI;
  tcPtr.p->currentTriggerId = firedTriggerData->triggerId;
  tcPtr.p->triggerErrorCode = ZNOT_FOUND;
  tcPtr.p->operation = op;
  tcPtr.p->indexOp = attrValuesPtrI;
  tcPtr.p->nextTcFailHash = transPtr->i;

  {
    Ptr<TcDefinedTriggerData> trigPtr;
    c_theDefinedTriggers.getPtr(trigPtr, tcPtr.p->currentTriggerId);
    trigPtr.p->refCount++;
  }

  TableRecordPtr childIndexPtr;
  childIndexPtr.i = fkData->childIndexId;
  ptrCheckGuard(childIndexPtr, ctabrecFilesize, tableRecord);

  /**
   * Construct an index-scan
   */
  const Uint32 parallelism = SCAN_FROM_CHILD_PARALLELISM;
  ScanTabReq * req = CAST_PTR(ScanTabReq, signal->getDataPtrSend());
  Uint32 ri = 0;
  ScanTabReq::setParallelism(ri, parallelism);
  ScanTabReq::setDescendingFlag(ri, 0);
  ScanTabReq::setRangeScanFlag(ri, 1);
  ScanTabReq::setTupScanFlag(ri, 0);
  ScanTabReq::setNoDiskFlag(ri, 1);
  if (op == ZREAD)
  {
    ScanTabReq::setScanBatch(ri, 1);
    ScanTabReq::setLockMode(ri, 0);
    ScanTabReq::setHoldLockFlag(ri, 0);
    ScanTabReq::setReadCommittedFlag(ri, 1);
    ScanTabReq::setKeyinfoFlag(ri, 0);
  }
  else
  {
    ScanTabReq::setScanBatch(ri, 16);
    ScanTabReq::setLockMode(ri, 1);
    ScanTabReq::setHoldLockFlag(ri, 1);
    ScanTabReq::setReadCommittedFlag(ri, 0);
    ScanTabReq::setKeyinfoFlag(ri, 1);
  }
  ScanTabReq::setDistributionKeyFlag(ri, 0);
  ScanTabReq::setViaSPJFlag(ri, 0);
  ScanTabReq::setPassAllConfsFlag(ri, 0);
  ScanTabReq::set4WordConf(ri, 0);
  req->requestInfo = ri;
  req->transId1 = regApiPtr->transid[0];
  req->transId2 = regApiPtr->transid[1];
  req->buddyConPtr = transPtr->i;
  req->tableId = childIndexPtr.i;
  req->tableSchemaVersion = childIndexPtr.p->currentSchemaVersion;
  req->apiConnectPtr = scanApiConnectPtr.i;
  req->storedProcId = 0xFFFF;
  req->batch_byte_size = 0;
  req->first_batch_size = 0;

  SegmentedSectionPtr ptr[3];
  ptr[0].i = ptr[1].i = ptr[2].i = RNIL;

  Uint32 optrs[parallelism];
  for (Uint32 i = 0; i<parallelism; i++)
    optrs[i] = tcPtr.i;

  Uint32 program[] = {
    0, 1, 0, 0, 0, Interpreter::ExitLastOK()
  };

  if (op != ZREAD)
  {
    program[5] = Interpreter::ExitOK();
  }

  Uint32 errorCode = ZGET_DATAREC_ERROR;
  if (ERROR_INSERTED(8102))
  {
    goto oom;
  }
  if (unlikely( !import(ptr[0], optrs, NDB_ARRAY_SIZE(optrs))))
  {
    jam();
    goto oom;
  }

  if (unlikely( !import(ptr[1], program, NDB_ARRAY_SIZE(program))))
  {
    jam();
    goto oom;
  }

  {
    AttributeBuffer::DataBufferPool & pool = c_theAttributeBufferPool;
    LocalAttributeBuffer beforeValues(pool, firedTriggerData->beforeValues);
    if (unlikely((errorCode=fk_buildBounds(ptr[2], beforeValues, fkData)) != 0))
    {
      jam();
      goto oom;
    }
  }

  guard.clear(); // now sections will be handled...

  signal->header.m_noOfSections= 3;
  signal->m_sectionPtrI[0] = ptr[0].i;
  signal->m_sectionPtrI[1] = ptr[1].i;
  signal->m_sectionPtrI[2] = ptr[2].i;

  signal->header.theSendersBlockRef = reference();
  execSCAN_TABREQ(signal);

  transPtr->p->lqhkeyreqrec++; // Make sure that execution is stalled
  D("trans: cascading scans++ " << transPtr->p->cascading_scans_count);
  ndbrequire(transPtr->p->cascading_scans_count < MaxCascadingScansPerTransaction);
  transPtr->p->cascading_scans_count++;
  return;

oom:
  jam();
  for (Uint32 i = 0; i < 3; i++)
  {
    if (ptr[i].i != RNIL)
    {
      release(ptr[i]);
    }
  }
  tcConnectptr = tcPtr;
  releaseTcCon();
  releaseApiCon(signal, scanApiConnectPtr.i);
  abortTransFromTrigger(signal, *transPtr, errorCode);
  return;
}

/**
 * Receive a row with key information from a foreign key scan for
 * either an CASCADE DELETE/UPDATE or a SET NULL.
 * For each such row we will issue an UPDATE or DELETE and this is
 * part of the trigger execution of the foreign key parent trigger.
 * Therefore we increment the trigger execution count afterwards.
 */
void
Dbtc::execKEYINFO20(Signal* signal)
{
  jamEntry();
  const KeyInfo20 * conf = CAST_CONSTPTR(KeyInfo20, signal->getDataPtr());

  Uint32 transId[] = {
    conf->transId1,
    conf->transId2
  };

  Uint32 keyLen = conf->keyLen;
  Uint32 scanInfo = conf->scanInfo_Node;

  TcConnectRecordPtr tcPtr;
  tcPtr.i = conf->clientOpPtr;
  ptrCheckGuard(tcPtr, ctcConnectFilesize, tcConnectRecord);

  ApiConnectRecordPtr scanApiConnectPtr;
  scanApiConnectPtr.i = tcPtr.p->apiConnect;
  ptrCheckGuard(scanApiConnectPtr, capiConnectFilesize, apiConnectRecord);

  if (! (transId[0] == scanApiConnectPtr.p->transid[0] &&
         transId[1] == scanApiConnectPtr.p->transid[1]))
  {
    jam();

    /**
     * incorrect transid...no known scenario where this can happen
     */
    ndbassert(false);
    return;
  }

  /**
   * Validate base transaction
   */
  Uint32 orgTransPtrI = tcPtr.p->nextTcFailHash;
  ApiConnectRecordPtr transPtr;
  transPtr.i = orgTransPtrI;
  ptrCheckGuard(transPtr, capiConnectFilesize, apiConnectRecord);

  if (unlikely(! (transId[0] == transPtr.p->transid[0] &&
                  transId[1] == transPtr.p->transid[1] &&
                  transPtr.p->apiConnectstate != CS_ABORTING)))
  {
    jam();

    /**
     * The "base" transaction has been aborted...
     *   terminate scan directly
     */
    fk_scanFromChildTable_abort(signal, tcPtr);
    return;
  }

  Ptr<TcDefinedTriggerData> trigPtr;
  c_theDefinedTriggers.getPtr(trigPtr, tcPtr.p->currentTriggerId);

  /* Extract KeyData */
  Uint32 keyInfoPtrI = RNIL;
  if (signal->header.m_noOfSections == 0)
  {
    jam();
    Ptr<SectionSegment> keyInfo;
    if (unlikely(! import(keyInfo, conf->keyData, keyLen)))
    {
      abortTransFromTrigger(signal, transPtr, ZGET_DATAREC_ERROR);
      return;
    }
    keyInfoPtrI = keyInfo.i;
  }
  else
  {
    jam();
    ndbrequire(signal->header.m_noOfSections == 1); // key is already here...
    keyInfoPtrI = signal->m_sectionPtrI[0];
    signal->header.m_noOfSections = 0;
  }

  Ptr<TcFKData> fkPtr;
  // TODO make it a pool.getPtr() instead
  // by also adding fk_ptr_i to definedTriggerData
  ndbrequire(c_fk_hash.find(fkPtr, trigPtr.p->fkId));

  /**
   * Construct a DELETE/UPDATE
   * NOTE: on table...not index
   */
  Uint32 op = tcPtr.p->operation;
  TcKeyReq * const tcKeyReq =  CAST_PTR(TcKeyReq, signal->getDataPtrSend());
  TableRecordPtr childTabPtr;

  childTabPtr.i = fkPtr.p->childTableId;
  ptrCheckGuard(childTabPtr, ctabrecFilesize, tableRecord);
  tcKeyReq->apiConnectPtr = orgTransPtrI;
  tcKeyReq->senderData = tcPtr.p->triggeringOperation;

  tcKeyReq->attrLen = 0;
  tcKeyReq->tableId = childTabPtr.i;
  tcKeyReq->tableSchemaVersion = childTabPtr.p->currentSchemaVersion;
  tcKeyReq->transId1 = transPtr.p->transid[0];
  tcKeyReq->transId2 = transPtr.p->transid[1];
  Uint32 tcKeyRequestInfo = 0;
  TcKeyReq::setKeyLength(tcKeyRequestInfo, 0);
  TcKeyReq::setAIInTcKeyReq(tcKeyRequestInfo, 0);
  TcKeyReq::setOperationType(tcKeyRequestInfo, op);
  const bool use_scan_takeover = false;
  if (use_scan_takeover)
  {
    TcKeyReq::setScanIndFlag(tcKeyRequestInfo, 1);
  }
  tcKeyReq->requestInfo = tcKeyRequestInfo;
  if (use_scan_takeover)
  {
    tcKeyReq->scanInfo = (scanInfo << 1) + 1; // TODO cleanup
  }
  signal->m_sectionPtrI[ TcKeyReq::KeyInfoSectionNum ] = keyInfoPtrI;
  signal->header.m_noOfSections= 1;

  if (tcPtr.p->indexOp != RNIL)
  {
    /**
     * attrValues save in indexOp
     */
    Uint32 tmp = RNIL;
    if (unlikely( !dupSection(tmp, tcPtr.p->indexOp)))
    {
      jam();
      releaseSection(keyInfoPtrI);
      signal->header.m_noOfSections = 0;
      abortTransFromTrigger(signal, transPtr, ZGET_DATAREC_ERROR);
      return;
    }
    signal->m_sectionPtrI[ TcKeyReq::AttrInfoSectionNum ] = tmp;
    signal->header.m_noOfSections= 2;
  }

  TcConnectRecordPtr opPtr; // triggering operation
  opPtr.i = tcPtr.p->triggeringOperation;
  ptrCheckGuard(opPtr, ctcConnectFilesize, tcConnectRecord);

  /**
   * Fix savepoint id -
   *   fix so that op has same savepoint id as triggering operation
   */
  Uint32 currSavePointId = transPtr.p->currSavePointId;
  transPtr.p->currSavePointId = opPtr.p->savePointId;
  transPtr.p->m_special_op_flags = TcConnectRecord::SOF_TRIGGER;
  /* Pass trigger Id via ApiConnectRecord (nasty) */
  ndbrequire(transPtr.p->immediateTriggerId == RNIL);
  transPtr.p->immediateTriggerId = tcPtr.p->currentTriggerId;
  EXECUTE_DIRECT(DBTC, GSN_TCKEYREQ, signal, TcKeyReq::StaticLength +
                 (use_scan_takeover ? 1 : 0));
  jamEntry();

  /*
   * Restore ApiConnectRecord state
   */
  transPtr.p->immediateTriggerId = RNIL;
  transPtr.p->currSavePointId = currSavePointId;

  /**
   * Update counter of how many trigger executed...
   */
  opPtr.p->triggerExecutionCount++;
}

void
Dbtc::execSCAN_TABCONF(Signal* signal)
{
  jamEntry();

  if (ERROR_INSERTED(8109))
  {
    jam();
    /* Hang around */
    sendSignalWithDelay(cownref, GSN_SCAN_TABCONF, signal, 100, signal->getLength());
    return;
  }

  const ScanTabConf * conf = CAST_CONSTPTR(ScanTabConf, signal->getDataPtr());

  Uint32 transId[] = {
    conf->transId1,
    conf->transId2
  };

  TcConnectRecordPtr tcPtr;
  tcPtr.i = conf->apiConnectPtr;
  ptrCheckGuard(tcPtr, ctcConnectFilesize, tcConnectRecord);

  ApiConnectRecordPtr scanApiConnectPtr;
  scanApiConnectPtr.i = tcPtr.p->apiConnect;
  ptrCheckGuard(scanApiConnectPtr, capiConnectFilesize, apiConnectRecord);

  if (! (transId[0] == scanApiConnectPtr.p->transid[0] &&
         transId[1] == scanApiConnectPtr.p->transid[1]))
  {
    jam();

    /**
     * incorrect transid...no known scenario where this can happen
     */
    ndbassert(false);
    return;
  }

  /**
   * Validate base transaction
   */
  Uint32 orgTransPtrI = tcPtr.p->nextTcFailHash;
  ApiConnectRecordPtr orgApiConnectPtr;
  orgApiConnectPtr.i = orgTransPtrI;
  ptrCheckGuard(orgApiConnectPtr, capiConnectFilesize, apiConnectRecord);

  if (unlikely(! (transId[0] == orgApiConnectPtr.p->transid[0] &&
                  transId[1] == orgApiConnectPtr.p->transid[1] &&
                  orgApiConnectPtr.p->apiConnectstate != CS_ABORTING)))
  {
    jam();

    /**
     * The "base" transaction has been aborted...
     *   terminate scan directly
     */
    if (conf->requestInfo & ScanTabConf::EndOfData)
    {
      jam();
      fk_scanFromChildTable_done(signal, tcPtr);
    }
    else
    {
      jam();
      fk_scanFromChildTable_abort(signal, tcPtr);
    }
    return;
  }

  Ptr<TcDefinedTriggerData> trigPtr;
  c_theDefinedTriggers.getPtr(trigPtr, tcPtr.p->currentTriggerId);

  Ptr<TcFKData> fkPtr;
  // TODO make it a pool.getPtr() instead
  // by also adding fk_ptr_i to definedTriggerData
  ndbrequire(c_fk_hash.find(fkPtr, trigPtr.p->fkId));

  Uint32 rows = 0;
  const Uint32 ops = (conf->requestInfo >> OPERATIONS_SHIFT) & OPERATIONS_MASK;
  for (Uint32 i = 0; i<ops; i++)
  {
    jam();
    ScanTabConf::OpData * op = (ScanTabConf::OpData*)
      (signal->getDataPtr() + ScanTabConf::SignalLength + 3 * i);
    rows += ScanTabConf::getRows(op->rows);
  }

  if (rows && tcPtr.p->operation != ZREAD)
  {
    jam();
    TcConnectRecordPtr opPtr; // triggering operation
    opPtr.i = tcPtr.p->triggeringOperation;
    ptrCheckGuard(opPtr, ctcConnectFilesize, tcConnectRecord);
    TcConnectRecord * triggeringOp = opPtr.p;
    if (triggeringOp->operation == ZDELETE &&
        (fkPtr.p->bits & (CreateFKImplReq::FK_DELETE_CASCADE | CreateFKImplReq::FK_DELETE_SET_NULL)))
    {
      /**
       * don't abort scan
       */
      jam();
      rows = 0;
    }
    else if ((triggeringOp->operation == ZUPDATE || triggeringOp->operation == ZWRITE) &&
             (fkPtr.p->bits & (CreateFKImplReq::FK_UPDATE_CASCADE | CreateFKImplReq::FK_UPDATE_SET_NULL)))
    {
      /**
       * don't abort scan
       */
      jam();
      rows = 0;
    }
  }

  if (rows)
  {
    jam();
    tcPtr.p->triggerErrorCode = ZFK_CHILD_ROW_EXISTS;
    orgApiConnectPtr.p->errorData = trigPtr.p->fkId;
  }

  if (conf->requestInfo & ScanTabConf::EndOfData)
  {
    jam();
    fk_scanFromChildTable_done(signal, tcPtr);
  }
  else if (rows)
  {
    jam();
    /**
     * Abort scan...we already know that ZFK_CHILD_ROW_EXISTS
     */
    fk_scanFromChildTable_abort(signal, tcPtr);
  }
  else
  {
    jam();
    /**
     * Continue scanning...
     */
    const Uint32 parallelism = SCAN_FROM_CHILD_PARALLELISM;
    Uint32 cnt = 0;
    Uint32 operations[parallelism];
    for (Uint32 i = 0; i<ops; i++)
    {
      jam();
      ScanTabConf::OpData * op = (ScanTabConf::OpData*)
        (signal->getDataPtr() + ScanTabConf::SignalLength + 3 * i);
      if (op->tcPtrI != RNIL)
      {
        ndbrequire(cnt < NDB_ARRAY_SIZE(operations));
        operations[cnt++] = op->tcPtrI;
      }
    }
    if (cnt)
    {
      jam();
      ScanNextReq* req = CAST_PTR(ScanNextReq, signal->getDataPtrSend());
      req->apiConnectPtr = scanApiConnectPtr.i;
      req->stopScan = 0;
      req->transId1 = scanApiConnectPtr.p->transid[0];
      req->transId2 = scanApiConnectPtr.p->transid[1];
      memcpy(signal->getDataPtrSend() + ScanNextReq::SignalLength,
             operations, 4 * cnt);
      sendSignal(reference(), GSN_SCAN_NEXTREQ, signal,
                 ScanNextReq::SignalLength + cnt, JBB);
    }
  }
}

void
Dbtc::fk_scanFromChildTable_abort(Signal* signal, TcConnectRecordPtr tcPtr)
{
  ApiConnectRecordPtr scanApiConnectPtr;
  scanApiConnectPtr.i = tcPtr.p->apiConnect;
  ptrCheckGuard(scanApiConnectPtr, capiConnectFilesize, apiConnectRecord);

  /**
   * Check that the scan has not already completed
   * - so we can abort aggresively
   */
  if (scanApiConnectPtr.p->apiConnectstate != CS_START_SCAN)
  {
    jam();
    /**
     * Scan has already completed...we just haven't received the EOD yet
     *
     *   wait for it...
     */
    return;
  }

  ScanNextReq* req = CAST_PTR(ScanNextReq, signal->getDataPtrSend());
  req->apiConnectPtr = scanApiConnectPtr.i;
  req->stopScan = 1;
  req->transId1 = scanApiConnectPtr.p->transid[0];
  req->transId2 = scanApiConnectPtr.p->transid[1];

  /**
   * Here we need to use EXECUTE_DIRECT
   *   or this signal will race with state-changes in scan-record
   *
   * NOTE: A better alternative would be to fix Dbtc/protocol to handle
   *       these races...
   */
  EXECUTE_DIRECT(DBTC, GSN_SCAN_NEXTREQ, signal, ScanNextReq::SignalLength);
}

void
Dbtc::fk_scanFromChildTable_done(Signal* signal, TcConnectRecordPtr tcPtr)
{
  ApiConnectRecordPtr scanApiConnectPtr;
  scanApiConnectPtr.i = tcPtr.p->apiConnect;
  ptrCheckGuard(scanApiConnectPtr, capiConnectFilesize, apiConnectRecord);

  ndbrequire(scanApiConnectPtr.p->ndbapiConnect == tcPtr.i);

  /**
   * save things needed to finish this trigger op
   */
  Uint32 transId[] = {
    scanApiConnectPtr.p->transid[0],
    scanApiConnectPtr.p->transid[1]
  };

  Uint32 errCode = tcPtr.p->triggerErrorCode;
  Uint32 triggerId = tcPtr.p->currentTriggerId;
  Uint32 orgTransPtrI = tcPtr.p->nextTcFailHash;

  TcConnectRecordPtr opPtr; // triggering operation
  opPtr.i = tcPtr.p->triggeringOperation;

  /**
   * release extra allocated resources
   */
  if (tcPtr.p->indexOp != RNIL)
  {
    releaseSection(tcPtr.p->indexOp);
  }
  tcConnectptr = tcPtr;
  releaseTcCon();
  releaseApiCon(signal, scanApiConnectPtr.i);

  ApiConnectRecordPtr orgApiConnectPtr;
  orgApiConnectPtr.i = orgTransPtrI;
  ptrCheckGuard(orgApiConnectPtr, capiConnectFilesize, apiConnectRecord);

  if (! (transId[0] == orgApiConnectPtr.p->transid[0] &&
         transId[1] == orgApiConnectPtr.p->transid[1] &&
         orgApiConnectPtr.p->apiConnectstate != CS_ABORTING))
  {
    jam();
    /**
     * The "base" transaction has been aborted...
     *   we need just throw away our scan...
     *   any DML caused by it...is anyway put onto "real" transaction
     */
    return;
  }

  ptrCheckGuard(opPtr, ctcConnectFilesize, tcConnectRecord);
  if (opPtr.p->apiConnect != orgApiConnectPtr.i)
  {
    jam();
    ndbabort();
    /**
     * this should not happen :-)
     *
     * triggering operation has been moved to different transaction...
     * this should not happen since then the original trans should be aborted
     *   (or aborted and restarted) this is checked above...
     *
     */
    return;
  }

  ndbrequire(orgApiConnectPtr.p->lqhkeyreqrec > 0);
  ndbrequire(orgApiConnectPtr.p->lqhkeyreqrec > orgApiConnectPtr.p->lqhkeyconfrec);
  orgApiConnectPtr.p->lqhkeyreqrec--;

  D("trans: cascading scans-- " << orgApiConnectPtr.p->cascading_scans_count);
  ndbrequire(orgApiConnectPtr.p->cascading_scans_count > 0);
  orgApiConnectPtr.p->cascading_scans_count--;

  trigger_op_finished(signal, orgApiConnectPtr, triggerId, opPtr.p, errCode);
}

void
Dbtc::execSCAN_TABREF(Signal* signal)
{
  jamEntry();

  if (ERROR_INSERTED(8109))
  {
    jam();
    /* Hang around */
    sendSignalWithDelay(cownref, GSN_SCAN_TABREF, signal, 100, signal->getLength());
    return;
  }


  const ScanTabRef * ref = CAST_CONSTPTR(ScanTabRef, signal->getDataPtr());

  Uint32 transId[] = {
    ref->transId1,
    ref->transId2
  };

  TcConnectRecordPtr tcPtr;
  tcPtr.i = ref->apiConnectPtr;
  ptrCheckGuard(tcPtr, ctcConnectFilesize, tcConnectRecord);

  ApiConnectRecordPtr scanApiConnectPtr;
  scanApiConnectPtr.i = tcPtr.p->apiConnect;
  ptrCheckGuard(scanApiConnectPtr, capiConnectFilesize, apiConnectRecord);

  if (! (transId[0] == scanApiConnectPtr.p->transid[0] &&
         transId[1] == scanApiConnectPtr.p->transid[1]))
  {
    jam();

    /**
     * incorrect transid...no known scenario where this can happen
     */
    ndbassert(false);
    return;
  }

  tcPtr.p->triggerErrorCode = ref->errorCode;
  if (ref->closeNeeded)
  {
    jam();
    fk_scanFromChildTable_abort(signal, tcPtr);
  }
  else
  {
    jam();
    fk_scanFromChildTable_done(signal, tcPtr);
  }
}

Uint32
Dbtc::fk_buildBounds(SegmentedSectionPtr & dst,
                     LocalAttributeBuffer & src,
                     TcFKData* fkData)
{
  dst.i = RNIL;
  Uint32 dstPtrI = RNIL;

  IndexAttributeList * list = &fkData->parentTableColumns;

  AttributeBuffer::DataBufferIterator iter;
  bool eof = !src.first(iter);
  for (Uint32 i = 0; i < list->sz; i++)
  {
    Uint32 col = list->id[i];
    if (!eof && AttributeHeader(* iter.data).getAttributeId() == col)
    {
  found:
      Uint32 byteSize = AttributeHeader(* iter.data).getByteSize();
      Uint32 len32 = (byteSize + 3) / 4;
      if (len32 == 0)
      {
        // ?? TODO what to do
      }
      eof = !src.next(iter);

      Uint32 data[2];
      data[0] = TuxBoundInfo::BoundEQ;
      AttributeHeader::init(data+1, i, byteSize);
      if (unlikely(!appendToSection(dstPtrI, data, NDB_ARRAY_SIZE(data))))
      {
        dst.i = dstPtrI;
        return ZGET_DATAREC_ERROR;
      }

      Uint32 err = appendDataToSection(dstPtrI, src, iter,
                                       len32);
      if (unlikely(err != 0))
      {
        dst.i = dstPtrI;
        return err;
      }

      eof = iter.isNull();
    }
    else
    {
      /**
       * Search for column...
       */
      eof = !src.first(iter);
      while (!eof && AttributeHeader(* iter.data).getAttributeId() != col)
      {
        eof = !src.next(iter,
                           1 + AttributeHeader(* iter.data).getDataSize());
      }
      if (unlikely(eof))
      {
        dst.i = dstPtrI;
        return ZMISSING_TRIGGER_DATA;
      }
      ndbassert(AttributeHeader(* iter.data).getAttributeId() == col);
      goto found;
    }
  }

  dst.i = dstPtrI;
  return 0;
}

void
Dbtc::executeFKChildTrigger(Signal* signal,
                            TcDefinedTriggerData* definedTriggerData,
                            TcFiredTriggerData* firedTriggerData,
                            ApiConnectRecordPtr* transPtr,
                            TcConnectRecordPtr* opPtr)
{
  Ptr<TcFKData> fkPtr;
  // TODO make it a pool.getPtr() instead
  // by also adding fk_ptr_i to definedTriggerData
  ndbrequire(c_fk_hash.find(fkPtr, definedTriggerData->fkId));

  /**
   * We are performing an INSERT or an UPDATE on the child table
   * (the table where the foreign key is defined), we need to ensure that
   * the referenced table have the row we are referring to here.
   *
   * The parent table reference must be the primary key of the table,
   * so this is always a key lookup.
   */
  switch (firedTriggerData->triggerEvent) {
  case(TriggerEvent::TE_INSERT):
    jam();
    /**
     * Check that after values exists in parent table
     */
    fk_readFromParentTable(signal, firedTriggerData, transPtr, opPtr, fkPtr.p);
    break;
  case(TriggerEvent::TE_UPDATE):
    jam();
    /**
     * Check that after values exists in parent table
     */
    fk_readFromParentTable(signal, firedTriggerData, transPtr, opPtr, fkPtr.p);
    break;
  default:
    ndbabort();
  }
}

void
Dbtc::fk_readFromParentTable(Signal* signal,
                             TcFiredTriggerData* firedTriggerData,
                             ApiConnectRecordPtr* transPtr,
                             TcConnectRecordPtr* opPtr,
                             TcFKData* fkData)
{
  ApiConnectRecord* regApiPtr = transPtr->p;
  TcConnectRecord* opRecord = opPtr->p;
  TcKeyReq * const tcKeyReq =  (TcKeyReq *)signal->getDataPtrSend();
  Uint32 tcKeyRequestInfo = 0;
  TableRecordPtr parentTabPtr;

  parentTabPtr.i = fkData->parentTableId;
  ptrCheckGuard(parentTabPtr, ctabrecFilesize, tableRecord);
  tcKeyReq->apiConnectPtr = transPtr->i;
  tcKeyReq->senderData = opPtr->i;

  // Calculate key length and renumber attribute id:s
  AttributeBuffer::DataBufferPool & pool = c_theAttributeBufferPool;
  LocalAttributeBuffer afterValues(pool, firedTriggerData->afterValues);

  if (afterValues.getSize() == 0)
  {
    jam();
    ndbrequire(tc_testbit(regApiPtr->m_flags,
                          ApiConnectRecord::TF_DEFERRED_CONSTRAINTS));
    trigger_op_finished(signal, *transPtr, RNIL, opRecord, 0);
    return;
  }

  Uint32 keyIVal= RNIL;
  bool hasNull= false;
  Uint32 err = fk_buildKeyInfo(keyIVal, hasNull, afterValues, fkData, true);
  SegmentedSectionGuard guard(this, keyIVal);
  if (unlikely(err != 0))
  {
    abortTransFromTrigger(signal, *transPtr, err);
    return;
  }

  /* If there's Nulls in the values that become the index table's
   * PK then we skip this delete
   */
  if (hasNull)
  {
    jam();
    trigger_op_finished(signal, *transPtr, RNIL, opRecord, 0);
    return;
  }

  Uint16 flags = TcConnectRecord::SOF_TRIGGER;
  if (((fkData->bits & CreateFKImplReq::FK_ACTION_MASK) == 0) ||
      transPtr->p->isExecutingDeferredTriggers())
  {
    jam();
    /**
     * We don't need any locks here
     */
    flags |= TcConnectRecord::SOF_FK_READ_COMMITTED;
    TcKeyReq::setSimpleFlag(tcKeyRequestInfo, 1);
  }

  TcKeyReq::setKeyLength(tcKeyRequestInfo, 0);
  TcKeyReq::setAIInTcKeyReq(tcKeyRequestInfo, 0);
  tcKeyReq->attrLen = 0;
  tcKeyReq->tableId = parentTabPtr.i;
  TcKeyReq::setOperationType(tcKeyRequestInfo, ZREAD);
  tcKeyReq->tableSchemaVersion = parentTabPtr.p->currentSchemaVersion;
  tcKeyReq->transId1 = regApiPtr->transid[0];
  tcKeyReq->transId2 = regApiPtr->transid[1];
  tcKeyReq->requestInfo = tcKeyRequestInfo;

  guard.clear(); // now sections will be handled...

  /* Attach KeyInfo section to signal */
  ndbrequire(signal->header.m_noOfSections == 0);
  signal->m_sectionPtrI[ TcKeyReq::KeyInfoSectionNum ] = keyIVal;
  signal->header.m_noOfSections = 1;

  /**
   * Don't fix savepoint id -
   *   read latest copy??
   */
  regApiPtr->m_special_op_flags = flags;
  /* Pass trigger Id via ApiConnectRecord (nasty) */
  ndbrequire(regApiPtr->immediateTriggerId == RNIL);
  regApiPtr->immediateTriggerId= firedTriggerData->triggerId;
  EXECUTE_DIRECT(DBTC, GSN_TCKEYREQ, signal, TcKeyReq::StaticLength);
  jamEntry();

  /*
   * Restore ApiConnectRecord state
   */
  regApiPtr->immediateTriggerId = RNIL;
}

void Dbtc::releaseFiredTriggerData(TcFiredTriggerData_fifo* triggers)
{
  FiredTriggerPtr trigPtr;

  triggers->first(trigPtr);
  while (trigPtr.i != RNIL) {
    jam();
    // Release trigger records

    AttributeBuffer::DataBufferPool & pool = c_theAttributeBufferPool;
    LocalAttributeBuffer tmp1(pool, trigPtr.p->keyValues);
    tmp1.release();
    LocalAttributeBuffer tmp2(pool, trigPtr.p->beforeValues);
    tmp2.release();
    LocalAttributeBuffer tmp3(pool, trigPtr.p->afterValues);
    tmp3.release();
    
    triggers->next(trigPtr);
  }
  while (triggers->releaseFirst());
}

void Dbtc::releaseFiredTriggerData(Local_TcFiredTriggerData_fifo*
                                   triggers)
{
  FiredTriggerPtr trigPtr;

  triggers->first(trigPtr);
  while (trigPtr.i != RNIL)
  {
    jam();
    // Release trigger records

    AttributeBuffer::DataBufferPool & pool = c_theAttributeBufferPool;
    LocalAttributeBuffer tmp1(pool, trigPtr.p->keyValues);
    tmp1.release();
    LocalAttributeBuffer tmp2(pool, trigPtr.p->beforeValues);
    tmp2.release();
    LocalAttributeBuffer tmp3(pool, trigPtr.p->afterValues);
    tmp3.release();
    FiredTriggerPtr save = trigPtr;
    triggers->next(trigPtr);
    triggers->release(save);
  }
}

/**
 * abortTransFromTrigger
 *
 * This method is called when there is a problem with trigger
 * handling and the transaction should be aborted with the
 * given error code
 */
void Dbtc::abortTransFromTrigger(Signal* signal,
                                 const ApiConnectRecordPtr& transPtr,
                                 Uint32 error)
{
  jam();
  terrorCode = error;
  
  apiConnectptr = transPtr;
  
  abortErrorLab(signal);
}

Uint32
Dbtc::appendDataToSection(Uint32& sectionIVal,
                          AttributeBuffer & src,
                          AttributeBuffer::DataBufferIterator & iter,
                          Uint32 len)
{
  const Uint32 segSize = src.getSegmentSize(); // 11
  while (len)
  {
    /* Copy as many contiguous words as possible */
    Uint32 contigLeft = segSize - iter.ind;
    ndbassert(contigLeft);
    Uint32 contigValid = MIN(len, contigLeft);

    if (unlikely(!appendToSection(sectionIVal,
                                  iter.data,
                                  contigValid)))
    {
      goto full;
    }
    len -= contigValid;
    bool hasMore = src.next(iter, contigValid);

    if (len == 0)
      break;

    if (unlikely(! hasMore))
    {
      ndbassert(false); // this is internal error...
      goto fail;
    }
  }
  return 0;

full:
  jam();
  releaseSection(sectionIVal);
  sectionIVal= RNIL;
  return ZGET_DATAREC_ERROR;

fail:
  jam();
  releaseSection(sectionIVal);
  sectionIVal= RNIL;
  return ZMISSING_TRIGGER_DATA;
}

/**
 * appendAttrDataToSection
 *
 * Copy data in AttrInfo form from the given databuffer
 * to the given section IVal (can be RNIL).
 * If attribute headers are to be copied they will be
 * renumbered consecutively, starting with the given
 * attrId.
 * hasNull is updated to indicate whether any Nulls 
 * were encountered.
 */
bool Dbtc::appendAttrDataToSection(Uint32& sectionIVal, 
                                   AttributeBuffer& values,
                                   bool withHeaders,
                                   Uint32& attrId,
                                   bool& hasNull)
{
  AttributeBuffer::DataBufferIterator iter;
  bool moreData= values.first(iter);
  hasNull= false;
  const Uint32 segSize= values.getSegmentSize(); // 11

  while (moreData)
  {
    AttributeHeader* attrHeader = (AttributeHeader *) iter.data;
    Uint32 dataSize= attrHeader->getDataSize();
    hasNull |= (dataSize == 0);
    
    if (withHeaders)
    {
      AttributeHeader ah(*iter.data);
      ah.setAttributeId(attrId); // Renumber AttrIds
      if (unlikely(!appendToSection(sectionIVal,
                                    &ah.m_value,
                                    1)))
      {
        releaseSection(sectionIVal);
        sectionIVal= RNIL;
        return false;
      }
    }

    moreData= values.next(iter, 1);
    
    while (dataSize)
    {
      ndbrequire(moreData);
      /* Copy as many contiguous words as possible */
      Uint32 contigLeft= segSize - iter.ind;
      ndbassert(contigLeft);
      Uint32 contigValid= MIN(dataSize, contigLeft);
      
      if (unlikely(!appendToSection(sectionIVal,
                                    iter.data,
                                    contigValid)))
      {
        releaseSection(sectionIVal);
        sectionIVal= RNIL;
        return false;
      }
      moreData= values.next(iter, contigValid);
      dataSize-= contigValid;
    }
    attrId++;
  }

  return true;
} 

void Dbtc::insertIntoIndexTable(Signal* signal, 
                                TcFiredTriggerData* firedTriggerData, 
                                ApiConnectRecordPtr* transPtr,
                                TcConnectRecordPtr* opPtr,
                                TcIndexData* indexData)
{
  ApiConnectRecord* regApiPtr = transPtr->p;
  TcConnectRecord* opRecord = opPtr->p;
  TcKeyReq * const tcKeyReq =  (TcKeyReq *)signal->getDataPtrSend();
  Uint32 tcKeyRequestInfo = 0;
  TableRecordPtr indexTabPtr;

  jam();
  
  indexTabPtr.i = indexData->indexId;
  ptrCheckGuard(indexTabPtr, ctabrecFilesize, tableRecord);
  tcKeyReq->apiConnectPtr = transPtr->i;
  tcKeyReq->senderData = opPtr->i;

  /* Key for insert to unique index table is the afterValues from the
   * base table operation (from update or insert on base).
   * Data for insert to unique index table is the afterValues from the
   * base table operation plus the fragment id and packed keyValues from 
   * the base table operation
   */
  // Calculate key length and renumber attribute id:s
  AttributeBuffer::DataBufferPool & pool = c_theAttributeBufferPool;
  LocalAttributeBuffer afterValues(pool, firedTriggerData->afterValues);
  LocalAttributeBuffer keyValues(pool, firedTriggerData->keyValues);

  if (afterValues.getSize() == 0)
  {
    jam();
    ndbrequire(tc_testbit(regApiPtr->m_flags,
                          ApiConnectRecord::TF_DEFERRED_CONSTRAINTS));
    trigger_op_finished(signal, *transPtr, RNIL, opRecord, 0);
    return;
  }

  Uint32 keyIVal= RNIL;
  Uint32 attrIVal= RNIL;
  bool appendOk= false;
  do
  {
    Uint32 attrId= 0;
    bool hasNull= false;

    /* Build Insert KeyInfo section from aftervalues */
    if (unlikely(! appendAttrDataToSection(keyIVal,
                                           afterValues,
                                           false, // No AttributeHeaders
                                           attrId,
                                           hasNull)))
    {
      jam();
      break;
    }

    if(ERROR_INSERTED(8086))
    {
      /* Simulate SS exhaustion */
      break;
    }
    
    /* If there's Nulls in the values that become the index table's
     * PK then we skip this insert
     */
    if (hasNull)
    {
      jam();
      releaseSection(keyIVal);
      trigger_op_finished(signal, *transPtr, RNIL, opRecord, 0);
      return;
    }
    
    /* Build Insert AttrInfo section from aftervalues, 
     * fragment id + keyvalues
     */
    AttributeHeader ah(attrId, 0); // Length tbd.
    attrId= 0;
    if (unlikely((! appendAttrDataToSection(attrIVal,
                                            afterValues,
                                            true, // Include AttributeHeaders,
                                            attrId,
                                            hasNull)) ||
                 (! appendToSection(attrIVal,
                                    &ah.m_value,
                                    1))))
    {
      jam();
      break;
    }
    
    AttributeHeader* pkHeader= (AttributeHeader*) getLastWordPtr(attrIVal);
    Uint32 startSz= getSectionSz(attrIVal);
    if (unlikely((! appendToSection(attrIVal,
                                    &firedTriggerData->fragId,
                                    1)) ||
                 (! appendAttrDataToSection(attrIVal,
                                            keyValues,
                                            false, // No AttributeHeaders
                                            attrId,
                                            hasNull))))
    {
      jam();
      break;
    }

    appendOk= true;

    /* Now go back and set pk header length */
    pkHeader->setDataSize(getSectionSz(attrIVal) - startSz);
  } while(0);

  if (unlikely(!appendOk))
  {
    /* Some failure building up KeyInfo and AttrInfo */
    releaseSection(keyIVal);
    releaseSection(attrIVal);
    abortTransFromTrigger(signal, *transPtr, ZGET_DATAREC_ERROR);
    return;
  }
  
  /* Now build TcKeyReq for insert */
  TcKeyReq::setKeyLength(tcKeyRequestInfo, 0);
  tcKeyReq->attrLen = 0;
  TcKeyReq::setAIInTcKeyReq(tcKeyRequestInfo, 0);
  tcKeyReq->tableId = indexData->indexId;
  TcKeyReq::setOperationType(tcKeyRequestInfo, ZINSERT);
  tcKeyReq->tableSchemaVersion = indexTabPtr.p->currentSchemaVersion;
  tcKeyReq->transId1 = regApiPtr->transid[0];
  tcKeyReq->transId2 = regApiPtr->transid[1];
  tcKeyReq->requestInfo = tcKeyRequestInfo;
  
  /* Attach Key and AttrInfo sections to signal */
  ndbrequire(signal->header.m_noOfSections == 0);
  signal->m_sectionPtrI[ TcKeyReq::KeyInfoSectionNum ] = keyIVal;
  signal->m_sectionPtrI[ TcKeyReq::AttrInfoSectionNum ] = attrIVal;
  signal->header.m_noOfSections= 2;

  /*
   * Fix savepoint id -
   *   fix so that insert has same savepoint id as triggering operation
   */
  const Uint32 currSavePointId = regApiPtr->currSavePointId;
  regApiPtr->currSavePointId = opRecord->savePointId;
  regApiPtr->m_special_op_flags = TcConnectRecord::SOF_TRIGGER;
  /* Pass trigger Id via ApiConnectRecord (nasty) */
  ndbrequire(regApiPtr->immediateTriggerId == RNIL);
  regApiPtr->immediateTriggerId= firedTriggerData->triggerId;

  EXECUTE_DIRECT(DBTC, GSN_TCKEYREQ, signal, TcKeyReq::StaticLength);
  jamEntry();

  /*
   * Restore ApiConnectRecord state
   */
  regApiPtr->currSavePointId = currSavePointId;
  regApiPtr->immediateTriggerId = RNIL;
}

void Dbtc::deleteFromIndexTable(Signal* signal, 
                                TcFiredTriggerData* firedTriggerData, 
                                ApiConnectRecordPtr* transPtr,
                                TcConnectRecordPtr* opPtr,
                                TcIndexData* indexData)
{
  ApiConnectRecord* regApiPtr = transPtr->p;
  TcConnectRecord* opRecord = opPtr->p;
  TcKeyReq * const tcKeyReq =  (TcKeyReq *)signal->getDataPtrSend();
  Uint32 tcKeyRequestInfo = 0;
  TableRecordPtr indexTabPtr;

  indexTabPtr.i = indexData->indexId;
  ptrCheckGuard(indexTabPtr, ctabrecFilesize, tableRecord);
  tcKeyReq->apiConnectPtr = transPtr->i;
  tcKeyReq->senderData = opPtr->i;

  // Calculate key length and renumber attribute id:s
  AttributeBuffer::DataBufferPool & pool = c_theAttributeBufferPool;
  LocalAttributeBuffer beforeValues(pool, firedTriggerData->beforeValues);
  
  Uint32 keyIVal= RNIL;
  Uint32 attrId= 0;
  bool hasNull= false;

  if (beforeValues.getSize() == 0)
  {
    jam();
    ndbrequire(tc_testbit(regApiPtr->m_flags,
                          ApiConnectRecord::TF_DEFERRED_CONSTRAINTS));
    trigger_op_finished(signal, *transPtr, RNIL, opRecord, 0);
    return;
  }

  /* Build Delete KeyInfo section from beforevalues */
  if (unlikely((! appendAttrDataToSection(keyIVal,
                                          beforeValues,
                                          false, // No AttributeHeaders
                                          attrId,
                                          hasNull)) ||
               ERROR_INSERTED(8086)))
  {
    jam();
    releaseSection(keyIVal);
    abortTransFromTrigger(signal, *transPtr, ZGET_DATAREC_ERROR);
    return;
  }
  
  /* If there's Nulls in the values that become the index table's
   * PK then we skip this delete
   */
  if (hasNull)
  {
    jam();
    releaseSection(keyIVal);
    trigger_op_finished(signal, *transPtr, RNIL, opRecord, 0);
    return;
  }

  TcKeyReq::setKeyLength(tcKeyRequestInfo, 0);
  TcKeyReq::setAIInTcKeyReq(tcKeyRequestInfo, 0);
  tcKeyReq->attrLen = 0;
  tcKeyReq->tableId = indexData->indexId;
  TcKeyReq::setOperationType(tcKeyRequestInfo, ZDELETE);
  tcKeyReq->tableSchemaVersion = indexTabPtr.p->currentSchemaVersion;
  tcKeyReq->transId1 = regApiPtr->transid[0];
  tcKeyReq->transId2 = regApiPtr->transid[1];
  tcKeyReq->requestInfo = tcKeyRequestInfo;

  /* Attach KeyInfo section to signal */
  ndbrequire(signal->header.m_noOfSections == 0);
  signal->m_sectionPtrI[ TcKeyReq::KeyInfoSectionNum ] = keyIVal;
  signal->header.m_noOfSections= 1;

  /**
   * Fix savepoint id -
   *   fix so that delete has same savepoint id as triggering operation
   */
  const Uint32 currSavePointId = regApiPtr->currSavePointId;
  regApiPtr->currSavePointId = opRecord->savePointId;
  regApiPtr->m_special_op_flags = TcConnectRecord::SOF_TRIGGER;
  /* Pass trigger Id via ApiConnectRecord (nasty) */
  ndbrequire(regApiPtr->immediateTriggerId == RNIL);
  regApiPtr->immediateTriggerId= firedTriggerData->triggerId;
  EXECUTE_DIRECT(DBTC, GSN_TCKEYREQ, signal, TcKeyReq::StaticLength);
  jamEntry();

  /*
   * Restore ApiConnectRecord state
   */
  regApiPtr->currSavePointId = currSavePointId;
  regApiPtr->immediateTriggerId = RNIL;
}

Uint32 
Dbtc::TableRecord::getErrorCode(Uint32 schemaVersion) const {
  if(!get_enabled())
    return ZNO_SUCH_TABLE;
  if(get_dropping())
    return ZDROP_TABLE_IN_PROGRESS;
  if(table_version_major(schemaVersion) != table_version_major(currentSchemaVersion))
    return ZWRONG_SCHEMA_VERSION_ERROR;
  ErrorReporter::handleAssert("Dbtc::TableRecord::getErrorCode",
			      __FILE__, __LINE__);
  return 0;
}

void Dbtc::executeReorgTrigger(Signal* signal,
                               TcDefinedTriggerData* definedTriggerData,
                               TcFiredTriggerData* firedTriggerData,
                               ApiConnectRecordPtr* transPtr,
                               TcConnectRecordPtr* opPtr)
{

  ApiConnectRecord* regApiPtr = transPtr->p;
  TcConnectRecord* opRecord = opPtr->p;
  TcKeyReq * tcKeyReq =  (TcKeyReq *)signal->getDataPtrSend();

  tcKeyReq->apiConnectPtr = transPtr->i;
  tcKeyReq->senderData = opPtr->i;

  AttributeBuffer::DataBufferPool & pool = c_theAttributeBufferPool;
  LocalAttributeBuffer keyValues(pool, firedTriggerData->keyValues);
  LocalAttributeBuffer attrValues(pool, firedTriggerData->afterValues);

  Uint32 optype;
  bool sendAttrInfo= true;

  switch (firedTriggerData->triggerEvent) {
  case TriggerEvent::TE_INSERT:
    optype = ZINSERT;
    break;
  case TriggerEvent::TE_UPDATE:
    /**
     * Only update should be write, as COPY is done as update
     *   a (maybe) better solution would be to have a different
     *   trigger event for COPY
     */
    optype = ZWRITE;
    break;
  case TriggerEvent::TE_DELETE:
    optype = ZDELETE;
    sendAttrInfo= false;
    break;
  default:
    ndbabort();
  }

  Ptr<TableRecord> tablePtr;
  tablePtr.i = definedTriggerData->tableId;
  ptrCheckGuard(tablePtr, ctabrecFilesize, tableRecord);
  Uint32 tableVersion = tablePtr.p->currentSchemaVersion;

  Uint32 tcKeyRequestInfo = 0;
  TcKeyReq::setKeyLength(tcKeyRequestInfo, 0);
  TcKeyReq::setOperationType(tcKeyRequestInfo, optype);
  TcKeyReq::setAIInTcKeyReq(tcKeyRequestInfo, 0);
  tcKeyReq->attrLen = 0;
  tcKeyReq->tableId = tablePtr.i;
  tcKeyReq->requestInfo = tcKeyRequestInfo;
  tcKeyReq->tableSchemaVersion = tableVersion;
  tcKeyReq->transId1 = regApiPtr->transid[0];
  tcKeyReq->transId2 = regApiPtr->transid[1];

  Uint32 keyIVal= RNIL;
  Uint32 attrIVal= RNIL;
  Uint32 attrId= 0;
  bool hasNull= false;
  
  /* Prepare KeyInfo section */
  if (unlikely(!appendAttrDataToSection(keyIVal,
                                        keyValues,
                                        false, // No AttributeHeaders
                                        attrId,
                                        hasNull)))
  {
    releaseSection(keyIVal);
    abortTransFromTrigger(signal, *transPtr, ZGET_DATAREC_ERROR);
    return;
  }
  
  ndbrequire(!hasNull);
  
  if (sendAttrInfo)
  {
    /* Prepare AttrInfo section from Key values and
     * After values
     */
    LocalAttributeBuffer::Iterator attrIter;
    LocalAttributeBuffer* buffers[2];
    buffers[0]= &keyValues;
    buffers[1]= &attrValues;
    const Uint32 segSize= keyValues.getSegmentSize(); // 11
    for (int buf=0; buf < 2; buf++)
    {
      Uint32 dataSize= buffers[buf]->getSize();
      bool moreData= buffers[buf]->first(attrIter);

      while(dataSize)
      {
        ndbrequire(moreData);
        Uint32 contigLeft= segSize - attrIter.ind;
        Uint32 contigValid= MIN(dataSize, contigLeft);
        
        if (unlikely(!appendToSection(attrIVal,
                                      attrIter.data,
                                      contigValid)))
        {
          releaseSection(keyIVal);
          releaseSection(attrIVal);
          abortTransFromTrigger(signal, *transPtr, ZGET_DATAREC_ERROR);
          return;
        }
        moreData= buffers[buf]->next(attrIter, contigValid);
        dataSize-= contigValid;
      }
      ndbassert(!moreData);
    }
  }

  /* Attach Key and optional AttrInfo sections to signal */
  ndbrequire(signal->header.m_noOfSections == 0);
  signal->m_sectionPtrI[ TcKeyReq::KeyInfoSectionNum ] = keyIVal;
  signal->m_sectionPtrI[ TcKeyReq::AttrInfoSectionNum ] = attrIVal;
  signal->header.m_noOfSections= sendAttrInfo? 2 : 1;

  /**
   * Fix savepoint id -
   *   fix so that the op has same savepoint id as triggering operation
   */
  const Uint32 currSavePointId = regApiPtr->currSavePointId;
  regApiPtr->currSavePointId = opRecord->savePointId;
  regApiPtr->m_special_op_flags =
    TcConnectRecord::SOF_REORG_TRIGGER |
    TcConnectRecord::SOF_TRIGGER;
  /**
   * The trigger is fired on this fragment id, ensure that the trigger
   * isn't executed on the same fragment id if the hash maps have been
   * swapped after starting the operation that sent FIRE_TRIG_ORD but
   * before we come here.
   */
  regApiPtr->firedFragId = firedTriggerData->fragId;
  /* Pass trigger Id via ApiConnectRecord (nasty) */
  ndbrequire(regApiPtr->immediateTriggerId == RNIL);

  regApiPtr->immediateTriggerId= firedTriggerData->triggerId;
  EXECUTE_DIRECT(DBTC, GSN_TCKEYREQ, signal, TcKeyReq::StaticLength);
  jamEntry();

  /*
   * Restore ApiConnectRecord state
   */
  regApiPtr->currSavePointId = currSavePointId;
  regApiPtr->immediateTriggerId = RNIL;
}

bool
Dbtc::executeFullyReplicatedTrigger(Signal* signal,
                                    TcDefinedTriggerData* definedTriggerData,
                                    TcFiredTriggerData* firedTriggerData,
                                    ApiConnectRecordPtr* transPtr,
                                    TcConnectRecordPtr* opPtr)
{
  /**
   * We use a special feature in DIGETNODESREQ to get the next copy fragment
   * given the current fragment. DIH keeps the copy fragments in an ordered
   * list to provide this feature for firing triggers on fully replicated
   * tables.
   *
   * There won't be any nodes returned in this special DIGETNODESREQ call,
   * we're only interested in getting the next fragment id.
   */
  jamLine((Uint16)firedTriggerData->fragId);
  DiGetNodesReq * diGetNodesReq =  (DiGetNodesReq *)signal->getDataPtrSend();
  diGetNodesReq->tableId = definedTriggerData->tableId;
  diGetNodesReq->hashValue = firedTriggerData->fragId;
  diGetNodesReq->distr_key_indicator = 0;
  diGetNodesReq->scan_indicator = 0;
  diGetNodesReq->get_next_fragid_indicator = 1;
  diGetNodesReq->anyNode = 0;
  diGetNodesReq->jamBufferPtr = jamBuffer();
  EXECUTE_DIRECT_MT(DBDIH, GSN_DIGETNODESREQ, signal,
                    DiGetNodesReq::SignalLength, 0);
  DiGetNodesConf * diGetNodesConf =  (DiGetNodesConf *)signal->getDataPtrSend();
  ndbrequire(diGetNodesConf->zero == 0);
  Uint32 fragId = diGetNodesConf->fragId;
  jamEntry();
  if (fragId == RNIL)
  {
    jam();
    /**
     * It is possible that the trigger is executed and done when arriving here.
     * This can happen in cases where there is only one node group, in this
     * case the fully replicated trigger will have nothing to do since there
     * are no more node groups to replicate to. We haven't incremented
     * triggerExecutionCount for fully replicated triggers, so need to check
     * if done here.
     */
    trigger_op_finished(signal, *transPtr, RNIL, opPtr->p, 0);
    return true;
  }
  /* Save fragId for next time we request the next fragId. */
  firedTriggerData->fragId = fragId;
  jamLine((Uint16)fragId);

  ApiConnectRecord* regApiPtr = transPtr->p;
  TcConnectRecord* opRecord = opPtr->p;
  TcKeyReq * tcKeyReq =  (TcKeyReq *)signal->getDataPtrSend();
  tcKeyReq->apiConnectPtr = transPtr->i;
  tcKeyReq->senderData = opPtr->i;

  AttributeBuffer::DataBufferPool & pool = c_theAttributeBufferPool;
  LocalAttributeBuffer keyValues(pool, firedTriggerData->keyValues);
  LocalAttributeBuffer attrValues(pool, firedTriggerData->afterValues);

  Uint32 optype;
  bool sendAttrInfo = true;

  switch (firedTriggerData->triggerEvent) {
  case TriggerEvent::TE_INSERT:
    jam();
    optype = ZINSERT;
    break;
  case TriggerEvent::TE_UPDATE:
    jam();
    optype = ZUPDATE;
    break;
  case TriggerEvent::TE_DELETE:
    jam();
    optype = ZDELETE;
    sendAttrInfo = false;
    break;
  default:
    ndbabort();
  }

  Ptr<TableRecord> tablePtr;
  tablePtr.i = definedTriggerData->tableId;
  ptrCheckGuard(tablePtr, ctabrecFilesize, tableRecord);
  Uint32 tableVersion = tablePtr.p->currentSchemaVersion;

  Uint32 tcKeyRequestInfo = 0;
  TcKeyReq::setKeyLength(tcKeyRequestInfo, 0);
  TcKeyReq::setOperationType(tcKeyRequestInfo, optype);
  TcKeyReq::setAIInTcKeyReq(tcKeyRequestInfo, 0);
  TcKeyReq::setDistributionKeyFlag(tcKeyRequestInfo, 1U);
  tcKeyReq->attrLen = 0;
  tcKeyReq->tableId = tablePtr.i;
  tcKeyReq->requestInfo = tcKeyRequestInfo;
  tcKeyReq->tableSchemaVersion = tableVersion;
  tcKeyReq->transId1 = regApiPtr->transid[0];
  tcKeyReq->transId2 = regApiPtr->transid[1];
  tcKeyReq->scanInfo = fragId;

  Uint32 keyIVal = RNIL;
  Uint32 attrIVal = RNIL;
  Uint32 attrId = 0;
  bool hasNull = false;

  /* Prepare KeyInfo section */
  if (unlikely(!appendAttrDataToSection(keyIVal,
                                        keyValues,
                                        false, // No AttributeHeaders
                                        attrId,
                                        hasNull)))
  {
    jam();
    abortTransFromTrigger(signal, *transPtr, ZGET_DATAREC_ERROR);
    return true;
  }

  ndbrequire(!hasNull);

  if (sendAttrInfo)
  {
    /* Prepare AttrInfo section from Key values and
     * After values
     */
    jam();
    LocalAttributeBuffer::Iterator attrIter;
    LocalAttributeBuffer* buffers[2];
    buffers[0] = &keyValues;
    buffers[1] = &attrValues;
    const Uint32 segSize = keyValues.getSegmentSize(); // 11
    for (int buf = 0; buf < 2; buf++)
    {
      jam();
      Uint32 dataSize = buffers[buf]->getSize();
      bool moreData = buffers[buf]->first(attrIter);

      while(dataSize)
      {
        jam();
        ndbrequire(moreData);
        Uint32 contigLeft = segSize - attrIter.ind;
        Uint32 contigValid = MIN(dataSize, contigLeft);

        if (unlikely(!appendToSection(attrIVal,
                                      attrIter.data,
                                      contigValid)))
        {
          jam();
          releaseSection(keyIVal);
          releaseSection(attrIVal);
          abortTransFromTrigger(signal, *transPtr, ZGET_DATAREC_ERROR);
          return true;
        }
        moreData = buffers[buf]->next(attrIter, contigValid);
        dataSize -= contigValid;
      }
      ndbassert(!moreData);
    }
  }

  /* Attach Key and optional AttrInfo sections to signal */
  ndbrequire(signal->header.m_noOfSections == 0);
  signal->m_sectionPtrI[ TcKeyReq::KeyInfoSectionNum ] = keyIVal;
  signal->m_sectionPtrI[ TcKeyReq::AttrInfoSectionNum ] = attrIVal;
  signal->header.m_noOfSections = sendAttrInfo? 2 : 1;

  Uint32 saveOp = regApiPtr->lastTcConnect;
  Uint32 saveState = regApiPtr->apiConnectstate;

  /**
   * Fix savepoint id -
   *   fix so that the op has same savepoint id as triggering operation
   */
  const Uint32 currSavePointId = regApiPtr->currSavePointId;
  regApiPtr->currSavePointId = opRecord->savePointId;
  regApiPtr->m_special_op_flags =
    TcConnectRecord::SOF_FULLY_REPLICATED_TRIGGER |
    TcConnectRecord::SOF_TRIGGER;
  /* Pass trigger Id via ApiConnectRecord (nasty) */
  ndbrequire(regApiPtr->immediateTriggerId == RNIL);

  regApiPtr->immediateTriggerId = firedTriggerData->triggerId;
  EXECUTE_DIRECT(DBTC, GSN_TCKEYREQ, signal, TcKeyReq::StaticLength + 1);
  jamEntry();

  /*
   * Restore ApiConnectRecord state
   */
  regApiPtr->currSavePointId = currSavePointId;
  regApiPtr->immediateTriggerId = RNIL;

  Uint32 currState = regApiPtr->apiConnectstate;

  bool ok =
    ((saveState == CS_SEND_FIRE_TRIG_REQ ||
      saveState == CS_WAIT_FIRE_TRIG_REQ ||
      saveState == CS_STARTED ||
      saveState == CS_START_COMMITTING) &&
      currState == saveState);

  if (ok && regApiPtr->lastTcConnect != saveOp)
  {
    jam();
    /**
     * We successfully added an op...continue iterating
     * ...and increment outstanding triggers
     */
    regApiPtr->apiConnectstate = (ConnectionState)saveState;
    opPtr->p->triggerExecutionCount++;
    return false;
  }

  ndbassert(terrorCode != 0);
  if (unlikely(terrorCode == 0))
  {
    jam();
    abortTransFromTrigger(signal, *transPtr, ZSTATE_ERROR);
    return true;
  }

  /**
   * Something went wrong...stop
   */
  return true;
}

void
Dbtc::execROUTE_ORD(Signal* signal)
{
  jamEntry();
  if(!assembleFragments(signal)){
    jam();
    return;
  }

  SectionHandle handle(this, signal);

  RouteOrd* ord = (RouteOrd*)signal->getDataPtr();
  Uint32 dstRef = ord->dstRef;
  Uint32 srcRef = ord->srcRef;
  Uint32 gsn = ord->gsn;

  if (likely(getNodeInfo(refToNode(dstRef)).m_connected))
  {
    jam();
    Uint32 secCount = handle.m_cnt;
    ndbrequire(secCount >= 1 && secCount <= 3);

    jamLine(secCount);

    /**
     * Put section 0 in signal->theData
     */
    Uint32 sigLen = handle.m_ptr[0].sz;
    ndbrequire(sigLen <= 25);
    copy(signal->theData, handle.m_ptr[0]);

    SegmentedSectionPtr save = handle.m_ptr[0];
    for (Uint32 i = 0; i < secCount - 1; i++)
      handle.m_ptr[i] = handle.m_ptr[i+1];
    handle.m_cnt--;

    sendSignal(dstRef, gsn, signal, sigLen, JBB, &handle);

    handle.m_cnt = 1;
    handle.m_ptr[0] = save;
    releaseSections(handle);
    return ;
  }

  releaseSections(handle);
  warningEvent("Unable to route GSN: %d from %x to %x",
	       gsn, srcRef, dstRef);

}

/**
 * Time track stats module
 * -----------------------
 * This module tracks response times of transactions, key operations,
 * scans, scans of fragments and errors of those operations.
 *
 * It tracks those times in a histogram with 32 ranges. This means that
 * we can get fairly detailed statistics about response times in the
 * system. We can also gather this information per API node or per
 * DB node and per TC thread.
 *
 * All data for this is currently gathered in the DBTC block. We could also
 * in the future make similar gathering for SPJ queries.
 *
 * The histogram ranges starts out at 50 microseconds and is incremented by
 * 50% for each subsequent range. We constantly increase the numbers in each
 * range, so in order to get the proper values for a time period one needs
 * to make two subsequent queries towards this table.
 *
 * The granularity of the timer for those signals depends on the granularity
 * of the scheduler.
 */
void
Dbtc::time_track_init_histogram_limits(void)
{
  HostRecordPtr hostPtr;
  /**
   * Calculate the boundary values, the first one is 50 microseconds,
   * after that we multiply by 1.5 consecutively.
   */
  Uint32 val = TIME_TRACK_INITIAL_RANGE_VALUE;
  for (Uint32 i = 0; i < TIME_TRACK_HISTOGRAM_RANGES; i++)
  {
    c_time_track_histogram_boundary[i] = val;
    val *= 3;
    val /= 2;
  }
  for (Uint32 node = 0; node < MAX_NODES; node++)
  {
    hostPtr.i = node;
    ptrCheckGuard(hostPtr, chostFilesize, hostRecord);
    hostPtr.p->time_tracked = FALSE;
    for (Uint32 i = 0; i < TIME_TRACK_HISTOGRAM_RANGES; i++)
    {
      hostPtr.p->time_track_scan_histogram[i] = 0;
      hostPtr.p->time_track_scan_error_histogram[i] = 0;
      hostPtr.p->time_track_read_key_histogram[i] = 0;
      hostPtr.p->time_track_write_key_histogram[i] = 0;
      hostPtr.p->time_track_index_key_histogram[i] = 0;
      hostPtr.p->time_track_key_error_histogram[i] = 0;
      hostPtr.p->time_track_scan_frag_histogram[i] = 0;
      hostPtr.p->time_track_scan_frag_error_histogram[i] = 0;
      hostPtr.p->time_track_transaction_histogram[i] = 0;
    }
  }
}

Uint32
Dbtc::time_track_calculate_histogram_position(NDB_TICKS & start_ticks)
{
  NDB_TICKS end_ticks = getHighResTimer();
  Uint32 micros_passed =
    (Uint32)NdbTick_Elapsed(start_ticks, end_ticks).microSec();

  NdbTick_Invalidate(&start_ticks);

  /* Perform a binary search for the correct histogram range */
  Uint32 pos;
  Uint32 upper_pos = TIME_TRACK_HISTOGRAM_RANGES - 1;
  Uint32 lower_pos = 0;
  /**
   * We use a small variant to the standard binary search where we
   * know that we will always end up with upper_pos == lower_pos
   * after 5 or 6 loops with 32 ranges. This saves us from comparing
   * lower_pos and upper_pos in each loop effectively halfing the
   * number of needed compares almost, we will do one extra loop
   * at times.
   */
  pos = (upper_pos + lower_pos) / 2;
  while (true)
  {
    if (micros_passed > c_time_track_histogram_boundary[pos])
    {
      /* Move to upper half in histogram ranges */
      lower_pos = pos + 1;
    }
    else
    {
      /* Move to lower half in histogram ranges */
      upper_pos = pos;
    }
    pos = (upper_pos + lower_pos) / 2;
    if (upper_pos == lower_pos)
      break;
  }
  return pos;
}

void
Dbtc::time_track_complete_scan(ScanRecord * const scanPtr,
                               Uint32 apiNodeId)
{
  HostRecordPtr hostPtr;
  /* Scans are recorded on the calling API node */
  Uint32 pos = time_track_calculate_histogram_position(scanPtr->m_start_ticks);
  hostPtr.i = apiNodeId;
  ptrCheckGuard(hostPtr, chostFilesize, hostRecord);
  hostPtr.p->time_track_scan_histogram[pos]++;
  hostPtr.p->time_tracked = TRUE;
}

void
Dbtc::time_track_complete_scan_error(ScanRecord * const scanPtr,
                                     Uint32 apiNodeId)
{
  HostRecordPtr hostPtr;
  /* Scans are recorded on the calling API node */
  Uint32 pos = time_track_calculate_histogram_position(scanPtr->m_start_ticks);
  hostPtr.i = apiNodeId;
  ptrCheckGuard(hostPtr, chostFilesize, hostRecord);
  hostPtr.p->time_track_scan_error_histogram[pos]++;
  hostPtr.p->time_tracked = TRUE;
}

void
Dbtc::time_track_complete_index_key_operation(
  TcConnectRecord * const regTcPtr,
  Uint32 apiNodeId,
  Uint32 dbNodeId)
{
  HostRecordPtr apiHostPtr, dbHostPtr;
  /* Key ops are recorded on the calling API node and the primary DB node */
  if (!NdbTick_IsValid(regTcPtr->m_start_ticks))
  {
    /**
     * We can come here after receiving a "false" LQHKEYCONF as part
     * of trigger operations.
     */
    return;
  }
  Uint32 pos =
    time_track_calculate_histogram_position(regTcPtr->m_start_ticks);

  apiHostPtr.i = apiNodeId;
  ptrCheckGuard(apiHostPtr, chostFilesize, hostRecord);
  apiHostPtr.p->time_track_index_key_histogram[pos]++;
  apiHostPtr.p->time_tracked = TRUE;

  dbHostPtr.i = dbNodeId;
  ptrCheckGuard(dbHostPtr, chostFilesize, hostRecord);
  dbHostPtr.p->time_track_index_key_histogram[pos]++;
  dbHostPtr.p->time_tracked = TRUE;
}

void
Dbtc::time_track_complete_key_operation(
  struct TcConnectRecord * const regTcPtr,
  Uint32 apiNodeId,
  Uint32 dbNodeId)
{
  HostRecordPtr apiHostPtr, dbHostPtr;
  /* Key ops are recorded on the calling API node and the primary DB node */
  if (!NdbTick_IsValid(regTcPtr->m_start_ticks))
  {
    /**
     * We can come here after receiving a "false" LQHKEYCONF as part
     * of trigger operations.
     */
    return;
  }
  Uint32 pos =
    time_track_calculate_histogram_position(regTcPtr->m_start_ticks);
  apiHostPtr.i = apiNodeId;
  ptrCheckGuard(apiHostPtr, chostFilesize, hostRecord);
  dbHostPtr.i = dbNodeId;
  ptrCheckGuard(dbHostPtr, chostFilesize, hostRecord);

  if (regTcPtr->operation == ZREAD ||
      regTcPtr->operation == ZUNLOCK)
  {
    apiHostPtr.p->time_track_read_key_histogram[pos]++;
    dbHostPtr.p->time_track_read_key_histogram[pos]++;
  }
  else
  {
    apiHostPtr.p->time_track_write_key_histogram[pos]++;
    dbHostPtr.p->time_track_write_key_histogram[pos]++;
  }
  apiHostPtr.p->time_tracked = TRUE;
  dbHostPtr.p->time_tracked = TRUE;
}

void
Dbtc::time_track_complete_key_operation_error(
  struct TcConnectRecord * const regTcPtr,
  Uint32 apiNodeId,
  Uint32 dbNodeId)
{
  HostRecordPtr apiHostPtr, dbHostPtr;
  /* Scans are recorded on the calling API node and the primary DB node */
  Uint32 pos =
    time_track_calculate_histogram_position(regTcPtr->m_start_ticks);
  apiHostPtr.i = apiNodeId;
  ptrCheckGuard(apiHostPtr, chostFilesize, hostRecord);
  dbHostPtr.i = dbNodeId;
  ptrCheckGuard(dbHostPtr, chostFilesize, hostRecord);

  apiHostPtr.p->time_track_key_error_histogram[pos]++;
  dbHostPtr.p->time_track_key_error_histogram[pos]++;
  apiHostPtr.p->time_tracked = TRUE;
  dbHostPtr.p->time_tracked = TRUE;
}

void
Dbtc::time_track_complete_scan_frag(
  ScanFragRec * const scanFragPtr)
{
  HostRecordPtr hostPtr;
  /* Scan frag operations are recorded on the DB node */
  if (!NdbTick_IsValid(scanFragPtr->m_start_ticks))
  {
    /**
     * We can come here immediately after a conf message with a closed
     * message. So essentially we can get two SCAN_FRAGCONF after each
     * other without sending any SCAN_NEXTREQ in between.
     */
    jam();
    return;
  }
  Uint32 pos =
    time_track_calculate_histogram_position(scanFragPtr->m_start_ticks);
  Uint32 dbNodeId = refToNode(scanFragPtr->lqhBlockref);
  hostPtr.i = dbNodeId;
  ptrCheckGuard(hostPtr, chostFilesize, hostRecord);
  hostPtr.p->time_track_scan_frag_histogram[pos]++;
  hostPtr.p->time_tracked = TRUE;
}

void
Dbtc::time_track_complete_scan_frag_error(
  ScanFragRec * const scanFragPtr)
{
  HostRecordPtr hostPtr;
  /* Scan frag operations are recorded on the DB node */
  if (!NdbTick_IsValid(scanFragPtr->m_start_ticks))
  {
    /**
     * We can come here after sending CLOSE flag on SCAN_FRAGREQ
     * while still waiting for SCAN_FRAGCONF, if SCAN_FRAGCONF
     * arrives before this returns and in addition we send
     * SCAN_FRAGREF we crash unless we check for this.
     */
    jam();
    return;
  }
  Uint32 pos =
    time_track_calculate_histogram_position(scanFragPtr->m_start_ticks);
  Uint32 dbNodeId = refToNode(scanFragPtr->lqhBlockref);
  hostPtr.i = dbNodeId;
  ptrCheckGuard(hostPtr, chostFilesize, hostRecord);
  hostPtr.p->time_track_scan_frag_error_histogram[pos]++;
  hostPtr.p->time_tracked = TRUE;
}

void
Dbtc::time_track_complete_transaction(
  ApiConnectRecord * const apiConnectPtr)
{
  HostRecordPtr hostPtr;
  /* Transactions are recorded on the API node */
  Uint32 pos =
    time_track_calculate_histogram_position(apiConnectPtr->m_start_ticks);
  Uint32 apiNodeId = refToNode(apiConnectptr.p->ndbapiBlockref);
  hostPtr.i = apiNodeId;
  ptrCheckGuard(hostPtr, chostFilesize, hostRecord);
  hostPtr.p->time_track_transaction_histogram[pos]++;
  hostPtr.p->time_tracked = TRUE;
}

void
Dbtc::time_track_complete_transaction_error(
  ApiConnectRecord * const apiConnectPtr)
{
  HostRecordPtr hostPtr;
  /* Transactions are recorded on the API node */
  if (!NdbTick_IsValid(apiConnectPtr->m_start_ticks))
  {
    /**
     * As part of handling API node failure we might abort transactions that
     * haven't actually started.
     */
    return;
  }
  Uint32 pos =
    time_track_calculate_histogram_position(apiConnectPtr->m_start_ticks);
  Uint32 apiNodeId = refToNode(apiConnectptr.p->ndbapiBlockref);
  hostPtr.i = apiNodeId;
  ptrCheckGuard(hostPtr, chostFilesize, hostRecord);
  hostPtr.p->time_track_transaction_error_histogram[pos]++;
  hostPtr.p->time_tracked = TRUE;
}
