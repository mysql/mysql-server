/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#define DBTC_C

#include "Dbtc.hpp"
#include "md5_hash.hpp"
#include <RefConvert.hpp>
#include <ndb_limits.h>
#include <my_sys.h>
#include <ndb_rand.h>

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
#include <signaldata/DihFragCount.hpp>
#include <signaldata/ScanFrag.hpp>
#include <signaldata/ScanTab.hpp>
#include <signaldata/PrepDropTab.hpp>
#include <signaldata/DropTab.hpp>
#include <signaldata/AlterTab.hpp>
#include <signaldata/CreateTrig.hpp>
#include <signaldata/DropTrig.hpp>
#include <signaldata/FireTrigOrd.hpp>
#include <signaldata/TrigAttrInfo.hpp>
#include <signaldata/CreateIndx.hpp>
#include <signaldata/DropIndx.hpp>
#include <signaldata/AlterIndx.hpp>
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
  case Dbtc::CS_PREPARED: out << "CS_PREPARED"; break;
  case Dbtc::CS_START_PREPARING: out << "CS_START_PREPARING"; break;
  case Dbtc::CS_REC_PREPARING: out << "CS_REC_PREPARING"; break;
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

void
Dbtc::updateBuddyTimer(ApiConnectRecordPtr apiPtr)
{
  if (apiPtr.p->buddyPtr != RNIL) {
    jam();
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

void Dbtc::execCONTINUEB(Signal* signal) 
{
  UintR tcase;

  jamEntry();
  tcase = signal->theData[0];
  UintR Tdata0 = signal->theData[1];
  UintR Tdata1 = signal->theData[2];
  UintR Tdata2 = signal->theData[3];
  switch (tcase) {
  case TcContinueB::ZRETURN_FROM_QUEUED_DELIVERY:
    jam();
    ndbrequire(false);
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
    tnodeid = Tdata0;
    tcNodeFailptr.i = 0;
    ptrAss(tcNodeFailptr, tcFailRecord);
    nodeTakeOverCompletedLab(signal);
    return;
  case TcContinueB::ZINITIALISE_RECORDS:
    jam();
    initialiseRecordsLab(signal, Tdata0, Tdata2, signal->theData[4]);
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
    /* -------------------------------------------------------------------- */
    // Report information about transaction activity once per second.
    /* -------------------------------------------------------------------- */
    if (c_counters.c_trans_status == TransCounters::Timer){
      Uint32 len = c_counters.report(signal);
      sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, len, JBB);
      
      c_counters.reset();
      signal->theData[0] = TcContinueB::ZTRANS_EVENT_REP;
      sendSignalWithDelay(cownref, GSN_CONTINUEB, signal, 5000, 1);
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
  case TcContinueB::CHECK_WAIT_DROP_TAB_FAILED_LQH:
    jam();
    checkWaitDropTabFailedLqh(signal, Tdata0, Tdata1);
    return;
  case TcContinueB::TRIGGER_PENDING:
    jam();
    ApiConnectRecordPtr transPtr;
    transPtr.i = Tdata0;
    ptrCheckGuard(transPtr, capiConnectFilesize, apiConnectRecord);
    transPtr.p->triggerPending = false;
    executeTriggers(signal, &transPtr);
    return;
  case TcContinueB::DelayTCKEYCONF:
    jam();
    apiConnectptr.i = Tdata0;
    ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
    sendtckeyconf(signal, Tdata1);
    return;
  default:
    ndbrequire(false);
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

  sendSignal(tblockref, GSN_INCL_NODECONF, signal, 2, JBB);
}

void Dbtc::execREAD_NODESREF(Signal* signal) 
{
  jamEntry();
  ndbrequire(false);
}

void Dbtc::execTC_SCHVERREQ(Signal* signal) 
{
  jamEntry();
  if (! assembleFragments(signal)) {
    jam();
    return;
  }
  tabptr.i = signal->theData[0];
  ptrCheckGuard(tabptr, ctabrecFilesize, tableRecord);
  tabptr.p->currentSchemaVersion = signal->theData[1];
  tabptr.p->m_flags = 0;
  tabptr.p->set_storedTable((bool)signal->theData[2]);
  BlockReference retRef = signal->theData[3];
  tabptr.p->tableType = (Uint8)signal->theData[4];
  BlockReference retPtr = signal->theData[5];
  Uint32 noOfKeyAttr = signal->theData[6];
  tabptr.p->singleUserMode = (Uint8)signal->theData[7];
  ndbrequire(noOfKeyAttr <= MAX_ATTRIBUTES_IN_INDEX);

  const KeyDescriptor* desc = g_key_descriptor_pool.getPtr(tabptr.i);
  ndbrequire(noOfKeyAttr == desc->noOfKeyAttr);

  ndbrequire(tabptr.p->get_enabled() == false);
  tabptr.p->set_enabled(true);
  tabptr.p->set_dropping(false);
  tabptr.p->noOfKeyAttr = desc->noOfKeyAttr;
  tabptr.p->hasCharAttr = desc->hasCharAttr;
  tabptr.p->noOfDistrKeys = desc->noOfDistrKeys;
  tabptr.p->hasVarKeys = desc->noOfVarKeys > 0;
  signal->theData[0] = tabptr.i;
  signal->theData[1] = retPtr;
  sendSignal(retRef, GSN_TC_SCHVERCONF, signal, 2, JBB);
}//Dbtc::execTC_SCHVERREQ()

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
  
  if(!tabPtr.p->get_enabled()){
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

  if(tabPtr.p->get_dropping()){
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
  tabPtr.p->dropTable.senderRef = senderRef;
  tabPtr.p->dropTable.senderData = senderData;

  {
    WaitDropTabReq * req = (WaitDropTabReq*)signal->getDataPtrSend();
    req->tableId = tabPtr.i;
    req->senderRef = reference();
    
    HostRecordPtr hostPtr;
    tabPtr.p->dropTable.waitDropTabCount.clearWaitingFor();
    for (hostPtr.i = 1; hostPtr.i < MAX_NDB_NODES; hostPtr.i++) {
      jam();
      ptrAss(hostPtr, hostRecord);
      if (hostPtr.p->hostStatus == HS_ALIVE) {
	jam();
	tabPtr.p->dropTable.waitDropTabCount.setWaitingFor(hostPtr.i);
	sendSignal(calcLqhBlockRef(hostPtr.i), GSN_WAIT_DROP_TAB_REQ,
		   signal, WaitDropTabReq::SignalLength, JBB);
      }//for
    }//if
    
    ndbrequire(tabPtr.p->dropTable.waitDropTabCount.done() != true);
  }
}

void
Dbtc::execWAIT_DROP_TAB_CONF(Signal* signal)
{
  jamEntry();
  WaitDropTabConf * conf = (WaitDropTabConf*)signal->getDataPtr();

  TableRecordPtr tabPtr;
  tabPtr.i = conf->tableId;
  ptrCheckGuard(tabPtr, ctabrecFilesize, tableRecord);
  
  ndbrequire(tabPtr.p->get_dropping() == true);
  Uint32 nodeId = refToNode(conf->senderRef);
  tabPtr.p->dropTable.waitDropTabCount.clearWaitingFor(nodeId);
  
  if(!tabPtr.p->dropTable.waitDropTabCount.done()){
    jam();
    return;
  }
  
  {
    PrepDropTabConf* conf = (PrepDropTabConf*)signal->getDataPtrSend();
    conf->tableId = tabPtr.i;
    conf->senderRef = reference();
    conf->senderData = tabPtr.p->dropTable.senderData;
    sendSignal(tabPtr.p->dropTable.senderRef, GSN_PREP_DROP_TAB_CONF, signal,
	       PrepDropTabConf::SignalLength, JBB);
    tabPtr.p->dropTable.senderRef = 0;
  }
}

void
Dbtc::execWAIT_DROP_TAB_REF(Signal* signal)
{
  jamEntry();
  WaitDropTabRef * ref = (WaitDropTabRef*)signal->getDataPtr();

  TableRecordPtr tabPtr;
  tabPtr.i = ref->tableId;
  ptrCheckGuard(tabPtr, ctabrecFilesize, tableRecord);
  
  ndbrequire(tabPtr.p->get_dropping() == true);
  Uint32 nodeId = refToNode(ref->senderRef);
  tabPtr.p->dropTable.waitDropTabCount.clearWaitingFor(nodeId);
  
  ndbrequire(ref->errorCode == WaitDropTabRef::NoSuchTable ||
	     ref->errorCode == WaitDropTabRef::NF_FakeErrorREF);
  
  if(!tabPtr.p->dropTable.waitDropTabCount.done()){
    jam();
    return;
  }
  
  {
    PrepDropTabConf* conf = (PrepDropTabConf*)signal->getDataPtrSend();
    conf->tableId = tabPtr.i;
    conf->senderRef = reference();
    conf->senderData = tabPtr.p->dropTable.senderData;
    sendSignal(tabPtr.p->dropTable.senderRef, GSN_PREP_DROP_TAB_CONF, signal,
	       PrepDropTabConf::SignalLength, JBB);
    tabPtr.p->dropTable.senderRef = 0;
  }
}  

void
Dbtc::checkWaitDropTabFailedLqh(Signal* signal, Uint32 nodeId, Uint32 tableId)
{
  
  TableRecordPtr tabPtr;
  tabPtr.i = tableId;

  WaitDropTabConf * conf = (WaitDropTabConf*)signal->getDataPtr();
  conf->tableId = tableId;

  const Uint32 RT_BREAK = 16;
  for(Uint32 i = 0; i<RT_BREAK && tabPtr.i < ctabrecFilesize; i++, tabPtr.i++){
    jam();
    ptrAss(tabPtr, tableRecord);
    if(tabPtr.p->get_enabled() && tabPtr.p->get_dropping()){
      if(tabPtr.p->dropTable.waitDropTabCount.isWaitingFor(nodeId)){
        jam();
	conf->senderRef = calcLqhBlockRef(nodeId);
	execWAIT_DROP_TAB_CONF(signal);
	tabPtr.i++;
	break;
      }
    }
  }
  
  if(tabPtr.i == ctabrecFilesize){
    /**
     * Finished
     */
    jam();
    checkNodeFailComplete(signal, nodeId, HostRecord::NF_CHECK_DROP_TAB);
    return;
  }
  
  signal->theData[0] = TcContinueB::CHECK_WAIT_DROP_TAB_FAILED_LQH;
  signal->theData[1] = nodeId;
  signal->theData[2] = tabPtr.i;
  sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBB);
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
  tabPtr.p->set_dropping(false);
  
  DropTabConf * conf = (DropTabConf*)signal->getDataPtrSend();
  conf->tableId = tabPtr.i;
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(senderRef, GSN_DROP_TAB_CONF, signal,
	     PrepDropTabConf::SignalLength, JBB);
}

void Dbtc::execALTER_TAB_REQ(Signal * signal)
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

  TableRecordPtr tabPtr;
  tabPtr.i = req->tableId;
  ptrCheckGuard(tabPtr, ctabrecFilesize, tableRecord);
  tabPtr.p->currentSchemaVersion = tableVersion;

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

  ctimeOutCheckDelay = 50; // 500ms
}//Dbtc::execSIZEALT_REP()

void Dbtc::execSTTOR(Signal* signal) 
{
  Uint16 tphase;

  jamEntry();
                                                     /* START CASE */
  tphase = signal->theData[1];
  csignalKey = signal->theData[6];
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
  sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 5, JBB);
}//Dbtc::sttorryLab()

/* ***************************************************************************/
/*                          INTERNAL  START / RESTART                        */
/*****************************************************************************/
void Dbtc::execNDB_STTOR(Signal* signal) 
{
  Uint16 tndbstartphase;
  Uint16 tstarttype;

  jamEntry();
  tusersblkref = signal->theData[0];
  tnodeid = signal->theData[1];
  tndbstartphase = signal->theData[2];   /* START PHASE      */
  tstarttype = signal->theData[3];       /* START TYPE       */
  switch (tndbstartphase) {
  case ZINTSPH1:
    jam();
    intstartphase1x010Lab(signal);
    return;
  case ZINTSPH2:
    jam();
    intstartphase2x010Lab(signal);
    return;
  case ZINTSPH3:
    jam();
    intstartphase3x010Lab(signal);      /* SEIZE CONNECT RECORD IN EACH LQH*/
// Start transaction event reporting.
    c_counters.c_trans_status = TransCounters::Timer;
    c_counters.reset();
    signal->theData[0] = TcContinueB::ZTRANS_EVENT_REP;
    sendSignalWithDelay(cownref, GSN_CONTINUEB, signal, 10, 1);
    return;
  case ZINTSPH6:
    jam();
    csystemStart = SSS_TRUE;
    break;
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
  sendSignal(cndbcntrblockref, GSN_NDB_STTORRY, signal, 1, JBB);
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
  csystemStart = SSS_FALSE;
  ctimeOutCheckCounter = 0;
  ctimeOutCheckFragCounter = 0;
  ctimeOutMissedHeartbeats = 0;
  ctimeOutCheckHeartbeat = 0;
  ctimeOutCheckLastHeartbeat = 0;
  ctimeOutCheckActive = TOCS_FALSE;
  ctimeOutCheckFragActive = TOCS_FALSE;
  sttorryLab(signal);
}//Dbtc::startphase1x010Lab()

/*****************************************************************************/
/*                        I N T S T A R T P H A S E 1 X                      */
/*                         INITIALISE ALL RECORDS.                           */
/*****************************************************************************/
void Dbtc::intstartphase1x010Lab(Signal* signal) 
{
  cownNodeid = tnodeid;
  cownref =          calcTcBlockRef(cownNodeid);
  clqhblockref =     calcLqhBlockRef(cownNodeid);
  cdihblockref =     calcDihBlockRef(cownNodeid);
  cdictblockref =    calcDictBlockRef(cownNodeid);
  cndbcntrblockref = calcNdbCntrBlockRef(cownNodeid);
  cerrorBlockref   = calcNdbCntrBlockRef(cownNodeid);
  coperationsize = 0;
  cfailure_nr = 0;
  ndbsttorry010Lab(signal);
}//Dbtc::intstartphase1x010Lab()

/*****************************************************************************/
/*                         I N T S T A R T P H A S E 2 X                     */
/*                          SET-UP LOCAL CONNECTIONS.                        */
/*****************************************************************************/
void Dbtc::intstartphase2x010Lab(Signal* signal) 
{
  tcConnectptr.i = cfirstfreeTcConnect;
  intstartphase2x020Lab(signal);
}//Dbtc::intstartphase2x010Lab()

void Dbtc::intstartphase2x020Lab(Signal* signal) 
{
  if (tcConnectptr.i == RNIL) {
    jam();
    ndbsttorry010Lab(signal);
    return;
  }//if
  ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
  tcConnectptr.p->tcConnectstate = OS_CONNECTING_DICT;
/* ****************** */
/*     DISEIZEREQ   < */
/* ****************** */
  signal->theData[0] = tcConnectptr.i;
  signal->theData[1] = cownref;
  sendSignal(cdihblockref, GSN_DISEIZEREQ, signal, 2, JBB);
}//Dbtc::intstartphase2x020Lab()

void Dbtc::execDISEIZECONF(Signal* signal) 
{
  jamEntry();
  tcConnectptr.i = signal->theData[0];
  ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
  tcConnectptr.p->dihConnectptr = signal->theData[1];
  tcConnectptr.i = tcConnectptr.p->nextTcConnect;
  intstartphase2x020Lab(signal);
}//Dbtc::execDISEIZECONF()

/*****************************************************************************/
/*                         I N T S T A R T P H A S E 3 X                     */
/*                        PREPARE DISTRIBUTED CONNECTIONS                    */
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

      if (NdbNodeBitmask::get(readNodes->inactiveNodes, i)) {
        jam();
        hostptr.p->hostStatus = HS_DEAD;
      } else {
        jam();
        con_lineNodes++;
        hostptr.p->hostStatus = HS_ALIVE;
	c_alive_nodes.set(i);
      }//if
    }//if
  }//for
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

  capiFailRef = signal->theData[1];
  arrGuard(signal->theData[0], MAX_NODES);
  capiConnectClosing[signal->theData[0]] = 1;
  handleFailedApiNode(signal, signal->theData[0], (UintR)0);
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
    if (TapiNode == TapiFailedNode) {
#ifdef VM_TRACE
      if (apiConnectptr.p->apiFailState != ZFALSE) {
        ndbout << "Error in previous API fail handling discovered" << endl
	       << "  apiConnectptr.i = " << apiConnectptr.i << endl
	       << "  apiConnectstate = " << apiConnectptr.p->apiConnectstate 
	       << endl
	       << "  ndbapiBlockref = " << hex
	       << apiConnectptr.p->ndbapiBlockref << endl
	       << "  apiNode = " << refToNode(apiConnectptr.p->ndbapiBlockref) 
	       << endl;
	if (apiConnectptr.p->lastTcConnect != RNIL){	  
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
          capiConnectClosing[TapiFailedNode]++;
          apiConnectptr.p->apiFailState = ZTRUE;
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
        capiConnectClosing[TapiFailedNode]++;
        apiConnectptr.p->apiFailState = ZTRUE;
        break;
      case CS_START_SCAN:
        /*********************************************************************/
        // The api record was performing a scan operation. We need to check 
        // on the scan state. Since completing a scan process might involve
        // sending several signals we will increase the loop count by 64.
        /*********************************************************************/
        jam();

	apiConnectptr.p->apiFailState = ZTRUE;
	capiConnectClosing[TapiFailedNode]++;

	ScanRecordPtr scanPtr;
	scanPtr.i = apiConnectptr.p->apiScanRec;
	ptrCheckGuard(scanPtr, cscanrecFileSize, scanRecord);
	close_scan_req(signal, scanPtr, true);
	
        TloopCount += 64;
        break;
      case CS_CONNECTED:
      case CS_REC_COMMITTING:
      case CS_RECEIVING:
      case CS_STARTED:
        /*********************************************************************/
        // The api record was in the process of performing a transaction but 
        // had not yet sent all information. 
        // We need to initiate an ABORT since the API will not provide any 
        // more information. 
        // Since the abort can send many signals we will insert a real-time
        // break after checking this record.
        /*********************************************************************/
        jam();
        apiConnectptr.p->apiFailState = ZTRUE;
        capiConnectClosing[TapiFailedNode]++;
        abort010Lab(signal);
        TloopCount = 256;
        break;
      case CS_PREPARED:
        jam();
      case CS_REC_PREPARING:
        jam();
      case CS_START_PREPARING:
        jam();
        /*********************************************************************/
        // Not implemented yet.
        /*********************************************************************/
        systemErrorLab(signal, __LINE__);
        break;
      case CS_RESTART:
        jam();
      case CS_COMPLETING:
        jam();
      case CS_COMPLETE_SENT:
        jam();
      case CS_WAIT_COMPLETE_CONF:
        jam();
      case CS_FAIL_ABORTING:
        jam();
      case CS_FAIL_ABORTED:
        jam();
      case CS_FAIL_PREPARED:
        jam();
      case CS_FAIL_COMMITTING:
        jam();
      case CS_FAIL_COMMITTED:
        /*********************************************************************/
        // These states are only valid on copy and fail API connections.
        /*********************************************************************/
      default:
        jam();
        systemErrorLab(signal, __LINE__);
        break;
      }//switch
    } else {
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
                               Uint32 nodeId, 
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
        // We can respond with API_FAILCONF
        /********************************************************************/
        signal->theData[0] = nodeId;
        signal->theData[1] = cownref;
        sendSignal(capiFailRef, GSN_API_FAILCONF, signal, 2, JBB);
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
         * The record is still active
         *
         * Don't remove it, but continueb instead
         */
	break;
      }
      sendRemoveMarkers(signal, iter.curr.p);
      m_commitAckMarkerHash.release(iter.curr);
      
      break;
    } 
    m_commitAckMarkerHash.next(iter);
  }
  
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
  TfailedApiNode = refToNode(TlocalApiConnectptr.p->ndbapiBlockref);
  arrGuard(TfailedApiNode, MAX_NODES);
  capiConnectClosing[TfailedApiNode]--;
  releaseApiCon(signal, TapiConnectptr);
  TlocalApiConnectptr.p->apiFailState = ZFALSE;
  if (capiConnectClosing[TfailedApiNode] == 0) {
    jam();
    signal->theData[0] = TfailedApiNode;
    signal->theData[1] = cownref;
    sendSignal(capiFailRef, GSN_API_FAILCONF, signal, 2, JBB);
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
              signal->theData[0] = tapiPointer;
              signal->theData[1] = errCode;
              sendSignal(tapiBlockref, GSN_TCSEIZEREF, signal, 2, JBB);
              return;
            }
	  }//if (!(sl == SL_SINGLEUSER))
      } //if
    }
  } 
  
  seizeApiConnect(signal);
  if (terrorCode == ZOK) {
    jam();
    apiConnectptr.p->ndbapiConnect = tapiPointer;
    apiConnectptr.p->ndbapiBlockref = tapiBlockref;
    signal->theData[0] = apiConnectptr.p->ndbapiConnect;
    signal->theData[1] = apiConnectptr.i;
    sendSignal(tapiBlockref, GSN_TCSEIZECONF, signal, 2, JBB);
    return;
  }

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
  if (tapiPointer >= capiConnectFilesize) {
    jam();
    signal->theData[0] = tuserpointer;
    signal->theData[1] = ZINVALID_CONNECTION;
    signal->theData[2] = __LINE__;
    sendSignal(tapiBlockref, GSN_TCRELEASEREF, signal, 3, JBB);
    return;
  } else {
    jam();
    apiConnectptr.i = tapiPointer;
  }//if
  ptrAss(apiConnectptr, apiConnectRecord);
  if (apiConnectptr.p->apiConnectstate == CS_DISCONNECTED) {
    jam();
    signal->theData[0] = tuserpointer;
    sendSignal(tapiBlockref, GSN_TCRELEASECONF, signal, 1, JBB);
  } else {
    if (tapiBlockref == apiConnectptr.p->ndbapiBlockref) {
      if (apiConnectptr.p->apiConnectstate == CS_CONNECTED ||
	  (apiConnectptr.p->apiConnectstate == CS_ABORTING &&
	   apiConnectptr.p->abortState == AS_IDLE) ||
	  (apiConnectptr.p->apiConnectstate == CS_STARTED &&
	   apiConnectptr.p->firstTcConnect == RNIL))
      {
        jam();                                   /* JUST REPLY OK */
	apiConnectptr.p->m_transaction_nodes.clear();
        releaseApiCon(signal, apiConnectptr.i);
        signal->theData[0] = tuserpointer;
        sendSignal(tapiBlockref,
                   GSN_TCRELEASECONF, signal, 1, JBB);
      } else {
        jam();
        signal->theData[0] = tuserpointer;
        signal->theData[1] = ZINVALID_CONNECTION;
	signal->theData[2] = __LINE__;
	signal->theData[3] = apiConnectptr.p->apiConnectstate;
        sendSignal(tapiBlockref,
                   GSN_TCRELEASEREF, signal, 4, JBB);
      }
    } else {
      jam();
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
    ndbrequire(false);
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

void Dbtc::printState(Signal* signal, int place) 
{
#ifdef VM_TRACE // Change to if 0 to disable these printouts
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
	 << " lqhkeyreqrec = " << apiConnectptr.p->lqhkeyreqrec << endl;
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
#endif
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
    ndbrequire(false);
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
    seizeAttrbuferrorLab(signal);
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
    apiConnectptr.p->m_exec_flag = 1;
    goto start_failure;
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
                /* OK */
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

  tcConnectptr.i = apiConnectptr.p->lastTcConnect;
  ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
  switch (tcConnectptr.p->tcConnectstate) {
  case OS_WAIT_KEYINFO:
    jam();
    tckeyreq020Lab(signal);
    return;
  case OS_WAIT_SCAN:
    break;
  default:
    jam();
    terrorCode = ZSTATE_ERROR;
    abortErrorLab(signal);
    return;
  }//switch

  UintR TdataPos = 0;
  UintR TkeyLen = regCachePtr->keylen;
  UintR Tlen = regCachePtr->save1;

  do {
    if (cfirstfreeDatabuf == RNIL) {
      jam();
      seizeDatabuferrorLab(signal);
      return;
    }//if
    linkKeybuf(signal);
    arrGuard(TdataPos, 19);
    databufptr.p->data[0] = signal->theData[TdataPos + 3];
    databufptr.p->data[1] = signal->theData[TdataPos + 4];
    databufptr.p->data[2] = signal->theData[TdataPos + 5];
    databufptr.p->data[3] = signal->theData[TdataPos + 6];
    Tlen = Tlen + 4;
    TdataPos = TdataPos + 4;
    if (Tlen < TkeyLen) {
      jam();
      if (TdataPos >= tmaxData) {
        jam();
	/*----------------------------------------------------*/
	/** EXIT AND WAIT FOR SIGNAL KEYINFO OR KEYINFO9     **/
	/** WHEN EITHER OF THE SIGNALS IS RECEIVED A JUMP    **/
	/** TO LABEL "KEYINFO_LABEL" IS DONE. THEN THE       **/
	/** PROGRAM RETURNS TO LABEL TCKEYREQ020             **/
	/*----------------------------------------------------*/
        setApiConTimer(apiConnectptr.i, ctcTimer, __LINE__);
        regCachePtr->save1 = Tlen;
        return;
      }//if
    } else {
      jam();
      return;
    }//if
  } while (1);
  return;
}//Dbtc::execKEYINFO()

/*---------------------------------------------------------------------------*/
/*                                                                           */
/* MORE THAN FOUR WORDS OF KEY DATA. WE NEED TO PACK THIS IN KEYINFO SIGNALS.*/
/* WE WILL ALWAYS PACK 4 WORDS AT A TIME.                                    */
/*---------------------------------------------------------------------------*/
void Dbtc::packKeyData000Lab(Signal* signal,
                             BlockReference TBRef,
			     Uint32 totalLen) 
{
  CacheRecord * const regCachePtr = cachePtr.p;

  jam();
  Uint32 len = 0;
  databufptr.i = regCachePtr->firstKeybuf;
  signal->theData[0] = tcConnectptr.i;
  signal->theData[1] = apiConnectptr.p->transid[0];
  signal->theData[2] = apiConnectptr.p->transid[1];
  Uint32 * dst = signal->theData+3;
  ptrCheckGuard(databufptr, cdatabufFilesize, databufRecord);
  
  do {
    jam();
    databufptr.i = databufptr.p->nextDatabuf;
    dst[len + 0] = databufptr.p->data[0];
    dst[len + 1] = databufptr.p->data[1];
    dst[len + 2] = databufptr.p->data[2];
    dst[len + 3] = databufptr.p->data[3];
    len += 4;
    if (totalLen <= 4) {
      jam();
      /*---------------------------------------------------------------------*/
      /*       LAST PACK OF KEY DATA HAVE BEEN SENT                          */
      /*---------------------------------------------------------------------*/
      /*       THERE WERE UNSENT INFORMATION, SEND IT.                       */
      /*---------------------------------------------------------------------*/
      sendSignal(TBRef, GSN_KEYINFO, signal, 3 + len, JBB);
      return;
    } else if(len == KeyInfo::DataLength){
      jam();
      len = 0;
      sendSignal(TBRef, GSN_KEYINFO, signal, 3 + KeyInfo::DataLength, JBB);
    }
    totalLen -= 4;
    ptrCheckGuard(databufptr, cdatabufFilesize, databufRecord);
  } while (1);
}//Dbtc::packKeyData000Lab()

void Dbtc::tckeyreq020Lab(Signal* signal) 
{
  CacheRecord * const regCachePtr = cachePtr.p;
  UintR TdataPos = 0;
  UintR TkeyLen = regCachePtr->keylen;
  UintR Tlen = regCachePtr->save1;

  do {
    if (cfirstfreeDatabuf == RNIL) {
      jam();
      seizeDatabuferrorLab(signal);
      return;
    }//if
    linkKeybuf(signal);
    arrGuard(TdataPos, 19);
    databufptr.p->data[0] = signal->theData[TdataPos + 3];
    databufptr.p->data[1] = signal->theData[TdataPos + 4];
    databufptr.p->data[2] = signal->theData[TdataPos + 5];
    databufptr.p->data[3] = signal->theData[TdataPos + 6];
    Tlen = Tlen + 4;
    TdataPos = TdataPos + 4;
    if (Tlen < TkeyLen) {
      jam();
      if (TdataPos >= tmaxData) {
        jam();
	/*----------------------------------------------------*/
	/** EXIT AND WAIT FOR SIGNAL KEYINFO OR KEYINFO9     **/
	/** WHEN EITHER OF THE SIGNALS IS RECEIVED A JUMP    **/
	/** TO LABEL "KEYINFO_LABEL" IS DONE. THEN THE       **/
	/** PROGRAM RETURNS TO LABEL TCKEYREQ020             **/
	/*----------------------------------------------------*/
        setApiConTimer(apiConnectptr.i, ctcTimer, __LINE__);
        regCachePtr->save1 = Tlen;
        tcConnectptr.p->tcConnectstate = OS_WAIT_KEYINFO;
        return;
      }//if
    } else {
      jam();
      tckeyreq050Lab(signal);
      return;
    }//if
  } while (1);
  return;
}//Dbtc::tckeyreq020Lab()

/* ------------------------------------------------------------------------- */
/* -------        SAVE ATTRIBUTE INFORMATION IN OPERATION RECORD     ------- */
/* ------------------------------------------------------------------------- */
void Dbtc::saveAttrbuf(Signal* signal) 
{
  CacheRecord * const regCachePtr = cachePtr.p;
  UintR TfirstfreeAttrbuf = cfirstfreeAttrbuf;
  UintR TattrbufFilesize = cattrbufFilesize;
  UintR TTcfirstAttrbuf = regCachePtr->firstAttrbuf;
  UintR Tlen = signal->length() - 3;
  AttrbufRecord *localAttrbufRecord = attrbufRecord;

  AttrbufRecord * const regAttrPtr = &localAttrbufRecord[TfirstfreeAttrbuf];
  if (TfirstfreeAttrbuf >= TattrbufFilesize) {
    TCKEY_abort(signal, 21);
    return;
  }//if
  UintR Tnext = regAttrPtr->attrbuf[ZINBUF_NEXT];
  if (TTcfirstAttrbuf == RNIL) {
    jam();
    regCachePtr->firstAttrbuf = TfirstfreeAttrbuf;
  } else {
    AttrbufRecordPtr saAttrbufptr;

    saAttrbufptr.i = regCachePtr->lastAttrbuf;
    jam();
    if (saAttrbufptr.i >= TattrbufFilesize) {
      TCKEY_abort(signal, 22);
      return;
    }//if
    saAttrbufptr.p = &localAttrbufRecord[saAttrbufptr.i];
    saAttrbufptr.p->attrbuf[ZINBUF_NEXT] = TfirstfreeAttrbuf;
  }//if

  cfirstfreeAttrbuf = Tnext;
  regAttrPtr->attrbuf[ZINBUF_NEXT] = RNIL;
  regCachePtr->lastAttrbuf = TfirstfreeAttrbuf;
  regAttrPtr->attrbuf[ZINBUF_DATA_LEN] = Tlen;

  UintR Tdata1 = signal->theData[3];
  UintR Tdata2 = signal->theData[4];
  UintR Tdata3 = signal->theData[5];
  UintR Tdata4 = signal->theData[6];
  UintR Tdata5 = signal->theData[7];
  UintR Tdata6 = signal->theData[8];
  UintR Tdata7 = signal->theData[9];
  UintR Tdata8 = signal->theData[10];

  regAttrPtr->attrbuf[0] = Tdata1;
  regAttrPtr->attrbuf[1] = Tdata2;
  regAttrPtr->attrbuf[2] = Tdata3;
  regAttrPtr->attrbuf[3] = Tdata4;
  regAttrPtr->attrbuf[4] = Tdata5;
  regAttrPtr->attrbuf[5] = Tdata6;
  regAttrPtr->attrbuf[6] = Tdata7;
  regAttrPtr->attrbuf[7] = Tdata8;

  if (Tlen > 8) {

    Tdata1 = signal->theData[11];
    Tdata2 = signal->theData[12];
    Tdata3 = signal->theData[13];
    Tdata4 = signal->theData[14];
    Tdata5 = signal->theData[15];
    Tdata6 = signal->theData[16];
    Tdata7 = signal->theData[17];

    regAttrPtr->attrbuf[8] = Tdata1;
    regAttrPtr->attrbuf[9] = Tdata2;
    regAttrPtr->attrbuf[10] = Tdata3;
    regAttrPtr->attrbuf[11] = Tdata4;
    regAttrPtr->attrbuf[12] = Tdata5;
    regAttrPtr->attrbuf[13] = Tdata6;
    regAttrPtr->attrbuf[14] = Tdata7;
    jam();
    if (Tlen > 15) {

      Tdata1 = signal->theData[18];
      Tdata2 = signal->theData[19];
      Tdata3 = signal->theData[20];
      Tdata4 = signal->theData[21];
      Tdata5 = signal->theData[22];
      Tdata6 = signal->theData[23];
      Tdata7 = signal->theData[24];

      jam();
      regAttrPtr->attrbuf[15] = Tdata1;
      regAttrPtr->attrbuf[16] = Tdata2;
      regAttrPtr->attrbuf[17] = Tdata3;
      regAttrPtr->attrbuf[18] = Tdata4;
      regAttrPtr->attrbuf[19] = Tdata5;
      regAttrPtr->attrbuf[20] = Tdata6;
      regAttrPtr->attrbuf[21] = Tdata7;
    }//if
  }//if
}//Dbtc::saveAttrbuf()

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
  Tlength -= 3;
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
    UintR TfirstfreeAttrbuf = cfirstfreeAttrbuf;
    UintR TcurrReclenAi = regCachePtr->currReclenAi;
    UintR TattrLen = regCachePtr->attrlength;

    setApiConTimer(apiConnectptr.i, TtcTimer, __LINE__);
    cachePtr.i = TcachePtr;
    cachePtr.p = regCachePtr;
    TcurrReclenAi = TcurrReclenAi + Tlength;
    regCachePtr->currReclenAi = TcurrReclenAi;
    int TattrlengthRemain = TattrLen - TcurrReclenAi;

    if (TfirstfreeAttrbuf == RNIL) {
      DEBUG("No more attrinfo buffers");
      TCKEY_abort(signal, 24);
      return;
    }//if
    saveAttrbuf(signal);
    if (TattrlengthRemain == 0) {
      /****************************************************************>*/
      /* HERE WE HAVE FOUND THAT THE LAST SIGNAL BELONGING TO THIS       */
      /* OPERATION HAVE BEEN RECEIVED. THIS MEANS THAT WE CAN NOW REUSE */
      /* THE API CONNECT RECORD. HOWEVER IF PREPARE OR COMMIT HAVE BEEN */
      /* RECEIVED THEN IT IS NOT ALLOWED TO RECEIVE ANY FURTHER          */
      /* OPERATIONS.                                                     */
      /****************************************************************>*/
      UintR TlastConnect = regApiPtr->lastTcConnect;
      if (TcompRECEIVING) {
        jam();
        regApiPtr->apiConnectstate = CS_STARTED;
      } else {
        jam();
        regApiPtr->apiConnectstate = CS_START_COMMITTING;
      }//if
      tcConnectptr.i = TlastConnect;
      ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
      attrinfoDihReceivedLab(signal);
    } else if (TattrlengthRemain < 0) {
      jam();
      DEBUG("ATTRINFO wrong total length="<<Tlength
	    <<", TattrlengthRemain="<<TattrlengthRemain
	    <<", TattrLen="<<TattrLen
	    <<", TcurrReclenAi="<<TcurrReclenAi);
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
  DatabufRecordPtr locDatabufptr;
  UintR ti;
  UintR  Tdata0;
  UintR  Tdata1;
  UintR  Tdata2;
  UintR  Tdata3;
  UintR*  Tdata32;
  
  CacheRecord * const regCachePtr = cachePtr.p;
  Tdata32 = signal->theData;
  
  Tdata0 = regCachePtr->keydata[0];
  Tdata1 = regCachePtr->keydata[1];
  Tdata2 = regCachePtr->keydata[2];
  Tdata3 = regCachePtr->keydata[3];
  Tdata32[0] = Tdata0;
  Tdata32[1] = Tdata1;
  Tdata32[2] = Tdata2;
  Tdata32[3] = Tdata3;
  if (regCachePtr->keylen > 4) {
    locDatabufptr.i = regCachePtr->firstKeybuf;
    ti = 4;
    while (locDatabufptr.i != RNIL) {
      ptrCheckGuard(locDatabufptr, cdatabufFilesize, databufRecord);
      Tdata0 = locDatabufptr.p->data[0];
      Tdata1 = locDatabufptr.p->data[1];
      Tdata2 = locDatabufptr.p->data[2];
      Tdata3 = locDatabufptr.p->data[3];
      Tdata32[ti    ] = Tdata0;
      Tdata32[ti + 1] = Tdata1;
      Tdata32[ti + 2] = Tdata2;
      Tdata32[ti + 3] = Tdata3;
      locDatabufptr.i = locDatabufptr.p->nextDatabuf;
      ti += 4;
    }//while
  }//if

  UintR keylen = (UintR)regCachePtr->keylen;
  Uint32 distKey = regCachePtr->distributionKeyIndicator;
  
  Uint32 tmp[4];
  if(!regCachePtr->m_special_hash)
  {
    md5_hash(tmp, (Uint64*)&Tdata32[0], keylen);
  }
  else
  {
    handle_special_hash(tmp, Tdata32, keylen, regCachePtr->tableref, !distKey);
  }
  
  thashValue = tmp[0];
  if (distKey){
    jam();
    tdistrHashValue = regCachePtr->distributionKey;
  } else {
    jam();
    tdistrHashValue = tmp[1];
  }//if
}//Dbtc::hash()

bool
Dbtc::handle_special_hash(Uint32 dstHash[4], Uint32* src, Uint32 srcLen, 
			  Uint32 tabPtrI,
			  bool distr)
{
  Uint64 Tmp[MAX_KEY_SIZE_IN_WORDS * MAX_XFRM_MULTIPLY];
  const TableRecord* tabPtrP = &tableRecord[tabPtrI];
  const bool hasVarKeys = tabPtrP->hasVarKeys;
  const bool hasCharAttr = tabPtrP->hasCharAttr;
  const bool compute_distkey = distr && (tabPtrP->noOfDistrKeys > 0);
  
  Uint32 *dst = (Uint32*)Tmp;
  Uint32 dstPos = 0;
  Uint32 keyPartLen[MAX_ATTRIBUTES_IN_INDEX];
  Uint32 * keyPartLenPtr;
  if(hasCharAttr || (compute_distkey && hasVarKeys))
  {
    keyPartLenPtr = keyPartLen;
    dstPos = xfrm_key(tabPtrI, src, dst, sizeof(Tmp) >> 2, keyPartLenPtr);
    if (unlikely(dstPos == 0))
    {
      goto error;
    }
  } 
  else 
  {
    dst = src;
    dstPos = srcLen;
    keyPartLenPtr = 0;
  }
  
  md5_hash(dstHash, (Uint64*)dst, dstPos);
  
  if(compute_distkey)
  {
    jam();
    
    Uint32 tmp[4];
    Uint32 len = create_distr_key(tabPtrI, dst, keyPartLenPtr);
    md5_hash(tmp, (Uint64*)dst, len);
    dstHash[1] = tmp[1];
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
  UintR TtransCount = c_counters.ctransCount;
  UintR Ttransid0 = tcKeyReq->transId1;
  UintR Ttransid1 = tcKeyReq->transId2;

  regApiPtr->m_exec_flag = 0;
  regApiPtr->returncode = 0;
  regApiPtr->returnsignal = RS_TCKEYCONF;
  ndbassert(regApiPtr->firstTcConnect == RNIL);
  regApiPtr->firstTcConnect = RNIL;
  regApiPtr->lastTcConnect = RNIL;
  regApiPtr->globalcheckpointid = 0;
  regApiPtr->lqhkeyconfrec = 0;
  regApiPtr->lqhkeyreqrec = 0;
  regApiPtr->tckeyrec = 0;
  regApiPtr->tcindxrec = 0;
  regApiPtr->failureNr = TfailureNr;
  regApiPtr->transid[0] = Ttransid0;
  regApiPtr->transid[1] = Ttransid1;
  regApiPtr->commitAckMarker = RNIL;
  regApiPtr->buddyPtr = RNIL;
  regApiPtr->currSavePointId = 0;
  regApiPtr->m_transaction_nodes.clear();
  regApiPtr->singleUserMode = 0;
  // Trigger data
  releaseFiredTriggerData(&regApiPtr->theFiredTriggers),
  // Index data
  regApiPtr->indexOpReturn = false;
  regApiPtr->noIndexOp = 0;
  if(releaseIndexOperations)
    releaseAllSeizedIndexOperations(regApiPtr);

  c_counters.ctransCount = TtransCount + 1;
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

  UintR TconcurrentOp = c_counters.cconcurrentOp;
  UintR TlastTcConnect = regApiPtr->lastTcConnect;
  UintR TtcConnectptrIndex = tcConnectptr.i;
  TcConnectRecordPtr tmpTcConnectptr;

  cfirstfreeTcConnect = regTcPtr->nextTcConnect;
  tcConnectptr.p = regTcPtr;

  c_counters.cconcurrentOp = TconcurrentOp + 1;
  regTcPtr->prevTcConnect = TlastTcConnect;
  regTcPtr->nextTcConnect = RNIL;
  regTcPtr->accumulatingTriggerData.i = RNIL;  
  regTcPtr->accumulatingTriggerData.p = NULL;  
  regTcPtr->noFiredTriggers = 0;
  regTcPtr->noReceivedTriggers = 0;
  regTcPtr->triggerExecutionCount = 0;
  regTcPtr->triggeringOperation = RNIL;
  regTcPtr->isIndexOp = false;
  regTcPtr->indexOp = RNIL;
  regTcPtr->currentIndexId = RNIL;

  regApiPtr->lastTcConnect = TtcConnectptrIndex;

  if (TlastTcConnect == RNIL) {
    jam();
    regApiPtr->firstTcConnect = TtcConnectptrIndex;
  } else {
    tmpTcConnectptr.i = TlastTcConnect;
    jam();
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

#ifdef VM_TRACE
  // This is a good place to check that resources have 
  // been properly released from CacheRecord
  ndbrequire(regCachePtr->firstKeybuf == RNIL);
  ndbrequire(regCachePtr->lastKeybuf == RNIL);
#endif
  regCachePtr->firstKeybuf = RNIL;
  regCachePtr->lastKeybuf = RNIL;
  regCachePtr->firstAttrbuf = RNIL;
  regCachePtr->lastAttrbuf = RNIL;
  regCachePtr->currReclenAi = 0;
  return 0;
}//Dbtc::seizeCacheRecord()  

/*****************************************************************************/
/*                               T C K E Y R E Q                             */
/* AFTER HAVING ESTABLISHED THE CONNECT, THE APPLICATION BLOCK SENDS AN      */
/* OPERATION REQUEST TO TC. ALL NECESSARY INFORMATION TO CARRY OUT REQUEST   */
/* IS FURNISHED IN PARAMETERS. TC STORES THIS INFORMATION AND ENQUIRES       */
/* FROM DIH ABOUT THE NODES WHICH MAY HAVE THE REQUESTED DATA                */
/*****************************************************************************/
void Dbtc::execTCKEYREQ(Signal* signal) 
{
  Uint32 sendersNodeId = refToNode(signal->getSendersBlockRef());
  UintR compare_transid1, compare_transid2;
  UintR titcLenAiInTckeyreq;
  UintR TkeyLength;
  const TcKeyReq * const tcKeyReq = (TcKeyReq *)signal->getDataPtr();
  UintR Treqinfo;

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
    TCKEY_abort(signal, 6);
    return;
  }//if
  if (TtabIndex >= TtabMaxIndex) {
    TCKEY_abort(signal, 7);
    return;
  }//if

  Treqinfo = tcKeyReq->requestInfo;
  //--------------------------------------------------------------------------
  // Optimised version of ptrAss(tabptr, tableRecord)
  // Optimised version of ptrAss(apiConnectptr, apiConnectRecord)
  //--------------------------------------------------------------------------
  ApiConnectRecord * const regApiPtr = &localApiConnectRecord[TapiIndex];
  apiConnectptr.p = regApiPtr;

  Uint32 TstartFlag = TcKeyReq::getStartFlag(Treqinfo);
  Uint32 TexecFlag = TcKeyReq::getExecuteFlag(Treqinfo);

  Uint8 isIndexOp = regApiPtr->isIndexOp;
  bool isIndexOpReturn = regApiPtr->indexOpReturn;
  regApiPtr->isIndexOp = false; // Reset marker
  regApiPtr->m_exec_flag |= TexecFlag;
  TableRecordPtr localTabptr;
  localTabptr.i = TtabIndex;
  localTabptr.p = &tableRecord[TtabIndex];
  switch (regApiPtr->apiConnectstate) {
  case CS_CONNECTED:{
    if (TstartFlag == 1 && getAllowStartTransaction(sendersNodeId, localTabptr.p->singleUserMode) == true){
      //---------------------------------------------------------------------
      // Initialise API connect record if transaction is started.
      //---------------------------------------------------------------------
      jam();
      initApiConnectRec(signal, regApiPtr);
      regApiPtr->m_exec_flag = TexecFlag;
    } else {
      if(getAllowStartTransaction(sendersNodeId, localTabptr.p->singleUserMode) == true){
	/*------------------------------------------------------------------
	 * WE EXPECTED A START TRANSACTION. SINCE NO OPERATIONS HAVE BEEN 
	 * RECEIVED WE INDICATE THIS BY SETTING FIRST_TC_CONNECT TO RNIL TO 
	 * ENSURE PROPER OPERATION OF THE COMMON ABORT HANDLING.
	 *-----------------------------------------------------------------*/
	TCKEY_abort(signal, 0);
	return;
      } else {
	/**
	 * getAllowStartTransaction(sendersNodeId) == false
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
          getNodeState().getSingleUserApi() != sendersNodeId &&
          !localTabptr.p->singleUserMode)
      {
	TCKEY_abort(signal, TexecFlag ? 60 : 57);
        return;
      }
      initApiConnectRec(signal, regApiPtr);
      regApiPtr->m_exec_flag = TexecFlag;
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
	TCKEY_abort(signal, 1);
	return;
      }//if
    }
    break;
  case CS_ABORTING:
    if (regApiPtr->abortState == AS_IDLE) {
      if (TstartFlag == 1) {
        if(getAllowStartTransaction(sendersNodeId, localTabptr.p->singleUserMode) == false){
          TCKEY_abort(signal, TexecFlag ? 60 : 57);
          return;
        }
	//--------------------------------------------------------------------
	// Previous transaction had been aborted and the abort was completed. 
	// It is then OK to start a new transaction again.
	//--------------------------------------------------------------------
        jam();
        initApiConnectRec(signal, regApiPtr);
	regApiPtr->m_exec_flag = TexecFlag;
      } else if(TexecFlag) {
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
        return;
      }//if
    } else {
      //----------------------------------------------------------------------
      // Previous transaction is still aborting
      //----------------------------------------------------------------------
      jam();
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
    jam();
    if(isIndexOpReturn || TcKeyReq::getExecutingTrigger(Treqinfo)){
      break;
    }
  default:
    jam();
    /*----------------------------------------------------------------------
     * IN THIS CASE THE NDBAPI IS AN UNTRUSTED ENTITY THAT HAS SENT A SIGNAL 
     * WHEN IT WAS NOT EXPECTED TO. 
     * WE MIGHT BE IN A PROCESS TO RECEIVE, PREPARE, 
     * COMMIT OR COMPLETE AND OBVIOUSLY THIS IS NOT A DESIRED EVENT.
     * WE WILL ALWAYS COMPLETE THE ABORT HANDLING BEFORE WE ALLOW 
     * ANYTHING TO HAPPEN ON THIS CONNECTION AGAIN. 
     * THUS THERE IS NO ACTION FROM THE API THAT CAN SPEED UP THIS PROCESS.
     *---------------------------------------------------------------------*/
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
    TCKEY_abort(signal, 8);
    return;
  }//if
  
  //-------------------------------------------------------------------------
  // Error Insertion for testing purposes. Test to see what happens when no
  // more TC records available.
  //-------------------------------------------------------------------------
  if (ERROR_INSERTED(8032)) {
    TCKEY_abort(signal, 3);
    return;
  }//if
  
  if (seizeTcRecord(signal) != 0) {
    return;
  }//if
  
  if (seizeCacheRecord(signal) != 0) {
    return;
  }//if
  
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

  UintR TapiVersionNo = TcKeyReq::getAPIVersion(tcKeyReq->attrLen);
  UintR Tlqhkeyreqrec = regApiPtr->lqhkeyreqrec;
  regApiPtr->lqhkeyreqrec = Tlqhkeyreqrec + 1;
  regCachePtr->apiVersionNo = TapiVersionNo;

  UintR TapiConnectptrIndex = apiConnectptr.i;
  UintR TsenderData = tcKeyReq->senderData;
  UintR TattrLen = TcKeyReq::getAttrinfoLen(tcKeyReq->attrLen);
  UintR TattrinfoCount = c_counters.cattrinfoCount;

  regTcPtr->apiConnect = TapiConnectptrIndex;
  regTcPtr->clientData = TsenderData;
  regTcPtr->commitAckMarker = RNIL;
  regTcPtr->isIndexOp = isIndexOp;
  regTcPtr->indexOp = regApiPtr->executingIndexOp;
  regTcPtr->savePointId = regApiPtr->currSavePointId;
  regApiPtr->executingIndexOp = RNIL;

  regApiPtr->singleUserMode |= 1 << localTabptr.p->singleUserMode;

  if (TcKeyReq::getExecutingTrigger(Treqinfo)) {
    // Save the TcOperationPtr for fireing operation
    regTcPtr->triggeringOperation = TsenderData;
  }

  if (TexecFlag){
    Uint32 currSPId = regApiPtr->currSavePointId;
    regApiPtr->currSavePointId = ++currSPId;
  }

  regCachePtr->attrlength = TattrLen;
  c_counters.cattrinfoCount = TattrinfoCount + TattrLen;

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
  Uint8 TexecuteFlag        = TexecFlag;
  
  regTcPtr->dirtyOp  = TDirtyFlag;
  regTcPtr->opSimple = TSimpleFlag;
  regCachePtr->opExec   = TInterpretedFlag;
  regCachePtr->distributionKeyIndicator = TDistrKeyFlag;
  regCachePtr->m_no_disk_flag = TNoDiskFlag;

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
  Uint32* TkeyDataPtr = &TOptionalDataPtr[TkeyIndex];

  UintR Tdata1 = TkeyDataPtr[0];
  UintR Tdata2 = TkeyDataPtr[1];
  UintR Tdata3 = TkeyDataPtr[2];
  UintR Tdata4 = TkeyDataPtr[3];
  UintR Tdata5;

  regCachePtr->keydata[0] = Tdata1;
  regCachePtr->keydata[1] = Tdata2;
  regCachePtr->keydata[2] = Tdata3;
  regCachePtr->keydata[3] = Tdata4;

  TkeyLength = TcKeyReq::getKeyLength(Treqinfo);
  Uint32 TAIDataIndex;
  if (TkeyLength > 8) {
    TAIDataIndex = TkeyIndex + 8;
  } else {
    if (TkeyLength == 0) {
      TCKEY_abort(signal, 4);
      return;
    }//if
    TAIDataIndex = TkeyIndex + TkeyLength;
  }//if
  Uint32* TAIDataPtr = &TOptionalDataPtr[TAIDataIndex];

  titcLenAiInTckeyreq = TcKeyReq::getAIInTcKeyReq(Treqinfo);
  regCachePtr->keylen = TkeyLength;
  regCachePtr->lenAiInTckeyreq = titcLenAiInTckeyreq;
  regCachePtr->currReclenAi = titcLenAiInTckeyreq;
  regCachePtr->m_special_hash = 
    localTabptr.p->hasCharAttr | (localTabptr.p->noOfDistrKeys > 0);
  Tdata1 = TAIDataPtr[0];
  Tdata2 = TAIDataPtr[1];
  Tdata3 = TAIDataPtr[2];
  Tdata4 = TAIDataPtr[3];
  Tdata5 = TAIDataPtr[4];

  regCachePtr->attrinfo0     = Tdata1;
  regCachePtr->attrinfo15[0] = Tdata2;
  regCachePtr->attrinfo15[1] = Tdata3;
  regCachePtr->attrinfo15[2] = Tdata4;
  regCachePtr->attrinfo15[3] = Tdata5;

  if (TOperationType == ZREAD || TOperationType == ZREAD_EX) {
    Uint32 TreadCount = c_counters.creadCount;
    jam();
    c_counters.creadCount = TreadCount + 1;
  } else {
    if(regApiPtr->commitAckMarker == RNIL){
      jam();
      CommitAckMarkerPtr tmp;
      if(!m_commitAckMarkerHash.seize(tmp)){
        TCKEY_abort(signal, 56);
        return;
      } else {
        regTcPtr->commitAckMarker = tmp.i;
        regApiPtr->commitAckMarker = tmp.i;
        tmp.p->transid1      = tcKeyReq->transId1;
        tmp.p->transid2      = tcKeyReq->transId2;
        tmp.p->apiNodeId     = refToNode(regApiPtr->ndbapiBlockref);
        tmp.p->apiConnectPtr = TapiIndex;
        tmp.p->noOfLqhs      = 0;
#if defined VM_TRACE || defined ERROR_INSERT
	{
	  CommitAckMarkerPtr check;
	  ndbrequire(!m_commitAckMarkerHash.find(check, *tmp.p));
	}
#endif
        m_commitAckMarkerHash.add(tmp);
      }
    }
    
    UintR TwriteCount = c_counters.cwriteCount;
    UintR Toperationsize = coperationsize;
    /* -------------------------------------------------------------------- 
     *   THIS IS A TEMPORARY TABLE, DON'T UPDATE coperationsize. 
     *   THIS VARIABLE CONTROLS THE INTERVAL BETWEEN LCP'S AND 
     *   TEMP TABLES DON'T PARTICIPATE.
     * -------------------------------------------------------------------- */
    if (localTabptr.p->get_storedTable()) {
      coperationsize = ((Toperationsize + TattrLen) + TkeyLength) + 17;
    }
    c_counters.cwriteCount = TwriteCount + 1;
    switch (TOperationType) {
    case ZUPDATE:
    case ZINSERT:
    case ZDELETE:
    case ZWRITE:
      jam();
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
    } else {
      jam();
      regApiPtr->apiConnectstate = CS_RECEIVING;
    }//if
  }//if
  if (TkeyLength <= 4) {
    tckeyreq050Lab(signal);
    return;
  } else {
    if (cfirstfreeDatabuf != RNIL) {
      jam();
      linkKeybuf(signal);
      Tdata1 = TkeyDataPtr[4];
      Tdata2 = TkeyDataPtr[5];
      Tdata3 = TkeyDataPtr[6];
      Tdata4 = TkeyDataPtr[7];

      DatabufRecord * const regDataPtr = databufptr.p;
      regDataPtr->data[0] = Tdata1;
      regDataPtr->data[1] = Tdata2;
      regDataPtr->data[2] = Tdata3;
      regDataPtr->data[3] = Tdata4;
    } else {
      jam();
      seizeDatabuferrorLab(signal);
      return;
    }//if
    if (TkeyLength <= 8) {
      jam();
      tckeyreq050Lab(signal);
      return;
    } else {
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
  }//if
  return;
}//Dbtc::execTCKEYREQ()

void Dbtc::tckeyreq050Lab(Signal* signal) 
{
  UintR tnoOfBackup;
  UintR tnoOfStandby;
  UintR tnodeinfo;

  terrorCode = 0;

  hash(signal); /* NOW IT IS TIME TO CALCULATE THE HASH VALUE*/
  
  if (unlikely(terrorCode))
  {
    releaseAtErrorLab(signal);
    return;
  }

  CacheRecord * const regCachePtr = cachePtr.p;
  TcConnectRecord * const regTcPtr = tcConnectptr.p;
  ApiConnectRecord * const regApiPtr = apiConnectptr.p;

  UintR TtcTimer = ctcTimer;
  UintR ThashValue = thashValue;
  UintR TdistrHashValue = tdistrHashValue;
  UintR TdihConnectptr = regTcPtr->dihConnectptr;
  UintR Ttableref = regCachePtr->tableref;
  
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

  signal->theData[0] = TdihConnectptr;
  signal->theData[1] = Ttableref;
  signal->theData[2] = TdistrHashValue;
  signal->theData[3] = 0;
  signal->theData[4] = 0;
  signal->theData[5] = 0;
  signal->theData[6] = 0;

  /*-------------------------------------------------------------*/
  /* FOR EFFICIENCY REASONS WE AVOID THE SIGNAL SENDING HERE AND */
  /* PROCEED IMMEDIATELY TO DIH. IN MULTI-THREADED VERSIONS WE   */
  /* HAVE TO INSERT A MUTEX ON DIH TO ENSURE PROPER OPERATION.   */
  /* SINCE THIS SIGNAL AND DIVERIFYREQ ARE THE ONLY SIGNALS SENT */
  /* TO DIH IN TRAFFIC IT SHOULD BE OK (3% OF THE EXECUTION TIME */
  /* IS SPENT IN DIH AND EVEN LESS IN REPLICATED NDB.            */
  /*-------------------------------------------------------------*/
  EXECUTE_DIRECT(DBDIH, GSN_DIGETNODESREQ, signal, 3);
  UintR TerrorIndicator = signal->theData[0];
  jamEntry();
  if (TerrorIndicator != 0) {
    execDIGETNODESREF(signal);
    return;
  }
  
  if(ERROR_INSERTED(8050) && signal->theData[3] != getOwnNodeId())
  {
    ndbassert(false);
    signal->theData[1] = 626;
    execDIGETNODESREF(signal);
    return;
  }
  
  /****************>>*/
  /* DIGETNODESCONF >*/
  /* ***************>*/

  UintR Tdata1 = signal->theData[1];
  UintR Tdata2 = signal->theData[2];
  UintR Tdata3 = signal->theData[3];
  UintR Tdata4 = signal->theData[4];
  UintR Tdata5 = signal->theData[5];
  UintR Tdata6 = signal->theData[6];

  regCachePtr->fragmentid = Tdata1;
  tnodeinfo = Tdata2;

  regTcPtr->tcNodedata[0] = Tdata3;
  regTcPtr->tcNodedata[1] = Tdata4;
  regTcPtr->tcNodedata[2] = Tdata5;
  regTcPtr->tcNodedata[3] = Tdata6;

  Uint8 Toperation = regTcPtr->operation;
  Uint8 Tdirty = regTcPtr->dirtyOp;
  tnoOfBackup = tnodeinfo & 3;
  tnoOfStandby = (tnodeinfo >> 8) & 3;
 
  regCachePtr->fragmentDistributionKey = (tnodeinfo >> 16) & 255;
  if (Toperation == ZREAD || Toperation == ZREAD_EX) {
    if (Tdirty == 1) {
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
      for (Tindex = 1; Tindex <= tnoOfBackup; Tindex++) {
        UintR Tnode = regTcPtr->tcNodedata[Tindex];
        jam();
        if (Tnode == TownNode) {
          jam();
          regTcPtr->tcNodedata[0] = Tnode;
        }//if
      }//for
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
  } else {
    UintR TlastReplicaNo;
    jam();
    TlastReplicaNo = tnoOfBackup + tnoOfStandby;
    regTcPtr->lastReplicaNo = (Uint8)TlastReplicaNo;
    regTcPtr->noOfNodes = (Uint8)(TlastReplicaNo + 1);
  }//if
  if (regCachePtr->lenAiInTckeyreq == regCachePtr->attrlength) {
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
  arrGuard(Tnode, MAX_NDB_NODES);
  packLqhkeyreq(signal, calcLqhBlockRef(Tnode));
}//Dbtc::attrinfoDihReceivedLab()

void Dbtc::packLqhkeyreq(Signal* signal,
                         BlockReference TBRef) 
{
  CacheRecord * const regCachePtr = cachePtr.p;
  UintR Tkeylen = regCachePtr->keylen;
  UintR TfirstAttrbuf = regCachePtr->firstAttrbuf;
  sendlqhkeyreq(signal, TBRef);
  if (Tkeylen > 4) {
    packKeyData000Lab(signal, TBRef, Tkeylen - 4);
    releaseKeys();
  }//if
  packLqhkeyreq040Lab(signal,
                      TfirstAttrbuf,
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

  tslrAttrLen = 0;
  LqhKeyReq::setAttrLen(tslrAttrLen, regCachePtr->attrlength);
  /* ---------------------------------------------------------------------- */
  // Bit16 == 0 since StoredProcedures are not yet supported.
  /* ---------------------------------------------------------------------- */
  LqhKeyReq::setDistributionKey(tslrAttrLen, regCachePtr->fragmentDistributionKey);
  LqhKeyReq::setScanTakeOverFlag(tslrAttrLen, regCachePtr->scanTakeOverInd);

  Tdata10 = 0;
  sig0 = regTcPtr->opSimple;
  sig1 = regTcPtr->operation;
  sig2 = regTcPtr->dirtyOp;
  bool dirtyRead = (sig1 == ZREAD && sig2 == ZTRUE);
  LqhKeyReq::setKeyLen(Tdata10, regCachePtr->keylen);
  LqhKeyReq::setLastReplicaNo(Tdata10, regTcPtr->lastReplicaNo);
  if (unlikely(version < NDBD_ROWID_VERSION))
  {
    Uint32 op = regTcPtr->operation;
    Uint32 lock = (Operation_t) op == ZREAD_EX ? ZUPDATE : (Operation_t) op == ZWRITE ? ZINSERT : (Operation_t) op;
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

  /* ----------------------------------------------------------------------- 
   * Sequential Number of first LQH = 0, bit 22-23                           
   * IF ATTRIBUTE INFORMATION IS SENT IN TCKEYREQ,
   * IT IS ALSO SENT IN LQHKEYREQ
   * ----------------------------------------------------------------------- */
  LqhKeyReq::setAIInLqhKeyReq(Tdata10, regCachePtr->lenAiInTckeyreq);
  /* -----------------------------------------------------------------------
   * Bit 27 == 0 since TC record is the same as the client record.
   * Bit 28 == 0 since readLenAi can only be set after reading in LQH.
   * ----------------------------------------------------------------------- */
  //LqhKeyReq::setAPIVersion(Tdata10, regCachePtr->apiVersionNo);
  Uint32 commitAckMarker = regTcPtr->commitAckMarker;
  const Uint32 noOfLqhs = regTcPtr->noOfNodes;
  if(commitAckMarker != RNIL){
    jam();
    LqhKeyReq::setMarkerFlag(Tdata10, 1);

    CommitAckMarker * tmp = m_commitAckMarkerHash.getPtr(commitAckMarker);
    
    /**
     * Populate LQH array
     */
    tmp->noOfLqhs = noOfLqhs;
    for(Uint32 i = 0; i<noOfLqhs; i++){
      tmp->lqhNodeId[i] = regTcPtr->tcNodedata[i];
    }
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
  sig4 = (regTcPtr->isIndexOp == 2) ? reference() : regApiPtr->ndbapiBlockref;
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

  sig0 = regCachePtr->keydata[0];
  sig1 = regCachePtr->keydata[1];
  sig2 = regCachePtr->keydata[2];
  sig3 = regCachePtr->keydata[3];
  UintR Tkeylen = regCachePtr->keylen;

  lqhKeyReq->variableData[nextPos + 0] = sig0;
  lqhKeyReq->variableData[nextPos + 1] = sig1;
  lqhKeyReq->variableData[nextPos + 2] = sig2;
  lqhKeyReq->variableData[nextPos + 3] = sig3;

  if (Tkeylen < 4) {
    nextPos += Tkeylen;
  } else {
    nextPos += 4;
  }//if

  sig0 = regCachePtr->attrinfo0;
  sig1 = regCachePtr->attrinfo15[0];
  sig2 = regCachePtr->attrinfo15[1];
  sig3 = regCachePtr->attrinfo15[2];
  sig4 = regCachePtr->attrinfo15[3];
  UintR TlenAi = regCachePtr->lenAiInTckeyreq;

  lqhKeyReq->variableData[nextPos + 0] = sig0;
  lqhKeyReq->variableData[nextPos + 1] = sig1;
  lqhKeyReq->variableData[nextPos + 2] = sig2;
  lqhKeyReq->variableData[nextPos + 3] = sig3;
  lqhKeyReq->variableData[nextPos + 4] = sig4;

  nextPos += TlenAi;

  // Reset trigger count
  regTcPtr->accumulatingTriggerData.i = RNIL;  
  regTcPtr->accumulatingTriggerData.p = NULL;  
  regTcPtr->noFiredTriggers = 0;
  regTcPtr->triggerExecutionCount = 0;

  sendSignal(TBRef, GSN_LQHKEYREQ, signal, 
             nextPos + LqhKeyReq::FixedSignalLength, JBB);
}//Dbtc::sendlqhkeyreq()

void Dbtc::packLqhkeyreq040Lab(Signal* signal,
                               UintR anAttrBufIndex,
                               BlockReference TBRef) 
{
  TcConnectRecord * const regTcPtr = tcConnectptr.p;
#ifdef ERROR_INSERT
  ApiConnectRecord * const regApiPtr = apiConnectptr.p;
  if (ERROR_INSERTED(8009)) {
    if (regApiPtr->apiConnectstate == CS_STARTED) {
      attrbufptr.i = RNIL;
      CLEAR_ERROR_INSERT_VALUE;
      return;
    }//if
  }//if
  if (ERROR_INSERTED(8010)) {
    if (regApiPtr->apiConnectstate == CS_START_COMMITTING) {
      attrbufptr.i = RNIL;
      CLEAR_ERROR_INSERT_VALUE;
      return;
    }//if
  }//if
#endif

  UintR TattrbufFilesize = cattrbufFilesize;
  AttrbufRecord *localAttrbufRecord = attrbufRecord;
  while (1) {
    if (anAttrBufIndex == RNIL) {
      UintR TtcTimer = ctcTimer;
      UintR Tread = (regTcPtr->operation == ZREAD);
      UintR Tdirty = (regTcPtr->dirtyOp == ZTRUE);
      UintR Tboth = Tread & Tdirty;
      setApiConTimer(apiConnectptr.i, TtcTimer, __LINE__);
      jam();
      /*--------------------------------------------------------------------
       *   WE HAVE SENT ALL THE SIGNALS OF THIS OPERATION. SET STATE AND EXIT.
       *---------------------------------------------------------------------*/
      releaseAttrinfo();
      if (Tboth) {
        jam();
        releaseDirtyRead(signal, apiConnectptr, tcConnectptr.p);
        return;
      }//if
      regTcPtr->tcConnectstate = OS_OPERATING;
      return;
    }//if
    if (anAttrBufIndex < TattrbufFilesize) {
      AttrbufRecord * const regAttrPtr = &localAttrbufRecord[anAttrBufIndex];
      anAttrBufIndex = regAttrPtr->attrbuf[ZINBUF_NEXT];
      sendAttrinfo(signal,
                   tcConnectptr.i,
                   regAttrPtr,
                   TBRef);
    } else {
      TCKEY_abort(signal, 17);
      return;
    }//if
  }//while
}//Dbtc::packLqhkeyreq040Lab()

/* ========================================================================= */
/* -------      RELEASE ALL ATTRINFO RECORDS IN AN OPERATION RECORD  ------- */
/* ========================================================================= */
void Dbtc::releaseAttrinfo() 
{
  UintR Tmp;
  AttrbufRecordPtr Tattrbufptr;
  CacheRecord * const regCachePtr = cachePtr.p;
  UintR TattrbufFilesize = cattrbufFilesize;
  UintR TfirstfreeAttrbuf = cfirstfreeAttrbuf;
  Tattrbufptr.i = regCachePtr->firstAttrbuf;
  AttrbufRecord *localAttrbufRecord = attrbufRecord;

  while (Tattrbufptr.i < TattrbufFilesize) {
    Tattrbufptr.p = &localAttrbufRecord[Tattrbufptr.i];
    Tmp = Tattrbufptr.p->attrbuf[ZINBUF_NEXT];
    Tattrbufptr.p->attrbuf[ZINBUF_NEXT] = TfirstfreeAttrbuf;
    TfirstfreeAttrbuf = Tattrbufptr.i;
    Tattrbufptr.i = Tmp;
    jam();
  }//while
  if (Tattrbufptr.i == RNIL) {
//---------------------------------------------------
// Now we will release the cache record at the same
// time as releasing the attrinfo records.
//---------------------------------------------------
    ApiConnectRecord * const regApiPtr = apiConnectptr.p;
    UintR TfirstfreeCacheRec = cfirstfreeCacheRec;
    UintR TCacheIndex = cachePtr.i;
    cfirstfreeAttrbuf = TfirstfreeAttrbuf;
    regCachePtr->nextCacheRec = TfirstfreeCacheRec;
    cfirstfreeCacheRec = TCacheIndex;
    regApiPtr->cachePtr = RNIL;
    return;
  }//if
  systemErrorLab(0, __LINE__);
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
  Uint32 TsimpleReadCount = c_counters.csimpleReadCount;
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
  c_counters.csimpleReadCount = TsimpleReadCount + 1;
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
  UintR TconcurrentOp = c_counters.cconcurrentOp;
  UintR TtcConnectptrIndex = tcConnectptr.i;

  regTcPtr->tcConnectstate = OS_CONNECTED;
  regTcPtr->nextTcConnect = TfirstfreeTcConnect;
  regTcPtr->apiConnect = RNIL;
  regTcPtr->isIndexOp = false;
  regTcPtr->indexOp = RNIL;
  cfirstfreeTcConnect = TtcConnectptrIndex;
  c_counters.cconcurrentOp = TconcurrentOp - 1;
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
      execCOMMITTED(signal);
      Tstep += 3;
      break;
    case ZCOMPLETED:
      signal->header.theLength = 3;
      execCOMPLETED(signal);
      Tstep += 3;
      break;
    case ZLQHKEYCONF:
      jam();
      Tdata1 = TpackDataPtr[3];
      Tdata2 = TpackDataPtr[4];
      Tdata3 = TpackDataPtr[5];
      Tdata4 = TpackDataPtr[6];

      lqhKeyConf->readLen = Tdata1;
      lqhKeyConf->transId1 = Tdata2;
      lqhKeyConf->transId2 = Tdata3;
      lqhKeyConf->noFiredTriggers = Tdata4;
      signal->header.theLength = LqhKeyConf::SignalLength;
      execLQHKEYCONF(signal);
      Tstep += LqhKeyConf::SignalLength;
      break;
    default:
      systemErrorLab(signal, __LINE__);
      return;
    }//switch
  }//while
  return;
}//Dbtc::execPACKED_SIGNAL()

void Dbtc::execLQHKEYCONF(Signal* signal) 
{
  const LqhKeyConf * const lqhKeyConf = (LqhKeyConf *)signal->getDataPtr();
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
  Uint32 noFired = lqhKeyConf->noFiredTriggers;

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
#endif
  UintR TtcTimer = ctcTimer;
  regTcPtr->lastLqhCon = tlastLqhConnect;
  regTcPtr->lastLqhNodeId = refToNode(tlastLqhBlockref);
  regTcPtr->noFiredTriggers = noFired;

  UintR Ttckeyrec = (UintR)regApiPtr.p->tckeyrec;
  UintR TclientData = regTcPtr->clientData;
  UintR TdirtyOp = regTcPtr->dirtyOp;
  Uint32 TopSimple = regTcPtr->opSimple;
  Uint32 Toperation = regTcPtr->operation;
  ConnectionState TapiConnectstate = regApiPtr.p->apiConnectstate;
  if (Ttckeyrec > (ZTCOPCONF_SIZE - 2)) {
    TCKEY_abort(signal, 30);
    return;
  }
  if (TapiConnectstate == CS_ABORTING) {
    warningReport(signal, 27);
    return;
  }//if

  setApiConTimer(apiConnectptr.i, TtcTimer, __LINE__);

  if (regTcPtr->isIndexOp) {
    jam();
    // This was an internal TCKEYREQ
    // will be returned unpacked
    regTcPtr->attrInfoLen = treadlenAi;
  } else {
    if (noFired == 0 && regTcPtr->triggeringOperation == RNIL) {
      jam();
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
  else 
  {
    jam();
    if (noFired == 0) {
      jam();
      // No triggers to execute
      UintR Tlqhkeyconfrec = regApiPtr.p->lqhkeyconfrec;
      regApiPtr.p->lqhkeyconfrec = Tlqhkeyconfrec + 1;
      regTcPtr->tcConnectstate = OS_PREPARED;
    }
  }//if

  /**
   * And now decide what to do next
   */
  if (regTcPtr->triggeringOperation != RNIL) {
    jam();
    // This operation was created by a trigger execting operation
    // Restart it if we have executed all it's triggers
    TcConnectRecordPtr opPtr;

    opPtr.i = regTcPtr->triggeringOperation;
    ptrCheckGuard(opPtr, ctcConnectFilesize, localTcConnectRecord);
    opPtr.p->triggerExecutionCount--;
    if (opPtr.p->triggerExecutionCount == 0) {
      /*
      We have completed current trigger execution
      Continue triggering operation
      */
      jam();
      continueTriggeringOp(signal, opPtr.p);
    }
  } else if (noFired == 0) {
    // This operation did not fire any triggers, finish operation
    jam();
    if (regTcPtr->isIndexOp) {
      jam();
      setupIndexOpReturn(regApiPtr.p, regTcPtr);
    }
    lqhKeyConf_checkTransactionState(signal, regApiPtr);
  } else {
    // We have fired triggers
    jam();
    saveTriggeringOpState(signal, regTcPtr);
    if (regTcPtr->noReceivedTriggers == noFired) 
    {
      // We have received all data
      jam();
      executeTriggers(signal, &regApiPtr);
    }
    // else wait for more trigger data
  }
}//Dbtc::execLQHKEYCONF()
 
void Dbtc::setupIndexOpReturn(ApiConnectRecord* regApiPtr,
			      TcConnectRecord* regTcPtr) 
{
  regApiPtr->indexOpReturn = true;
  regApiPtr->indexOp = regTcPtr->indexOp;
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
      } else if (regApiPtr.p->indexOpReturn) {
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
      } else if (regApiPtr.p->indexOpReturn) {
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
      } else if (regApiPtr.p->indexOpReturn) {
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
  UintR TcurrLen = localHostptr.p->noOfWordsTCKEYCONF;
  UintR confInfo = 0;
  TcKeyConf::setCommitFlag(confInfo, TcommitFlag == 1);
  TcKeyConf::setMarkerFlag(confInfo, Tmarker);
  const UintR TpacketLen = 6 + TopWords;
  regApiPtr->tckeyrec = 0;

  if (regApiPtr->indexOpReturn) {
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
    regApiPtr->indexOpReturn = false;
    if (TopWords == 0) {
      jam();
      return; // No queued TcKeyConf
    }//if
  }//if
  if(TcommitFlag){
    jam();
    regApiPtr->m_exec_flag = 0;
  }
  TcKeyConf::setNoOfOperations(confInfo, (TopWords >> 1));
  if ((TpacketLen + 1 /** gci_lo */ > 25) || !is_api){
    TcKeyConf * const tcKeyConf = (TcKeyConf *)signal->getDataPtrSend();
    
    jam();
    tcKeyConf->apiConnectPtr = regApiPtr->ndbapiConnect;
    tcKeyConf->gci_hi = Uint32(regApiPtr->globalcheckpointid >> 32);
    Uint32* gci_lo = (Uint32*)&tcKeyConf->operations[TopWords >> 1];
    * gci_lo = Uint32(regApiPtr->globalcheckpointid);
    tcKeyConf->confInfo = confInfo;
    tcKeyConf->transId1 = regApiPtr->transid[0];
    tcKeyConf->transId2 = regApiPtr->transid[1];
    copyFromToLen(&regApiPtr->tcSendArray[0],
		  (UintR*)&tcKeyConf->operations,
		  (UintR)ZTCOPCONF_SIZE);
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
  UintR Tpack0 = (TblockNum << 16) + (TpacketLen - 4 + 1 /** gci_lo */);
  UintR Tpack1 = regApiPtr->ndbapiConnect;
  UintR Tpack2 = Uint32(regApiPtr->globalcheckpointid >> 32);
  UintR Tpack3 = confInfo;
  UintR Tpack4 = regApiPtr->transid[0];
  UintR Tpack5 = regApiPtr->transid[1];
  UintR Tpack6 = Uint32(regApiPtr->globalcheckpointid);
  
  localHostptr.p->noOfWordsTCKEYCONF = TcurrLen + TpacketLen + 1 /** gci_lo */;
  
  localHostptr.p->packedWordsTCKEYCONF[TcurrLen + 0] = Tpack0;
  localHostptr.p->packedWordsTCKEYCONF[TcurrLen + 1] = Tpack1;
  localHostptr.p->packedWordsTCKEYCONF[TcurrLen + 2] = Tpack2;
  localHostptr.p->packedWordsTCKEYCONF[TcurrLen + 3] = Tpack3;
  localHostptr.p->packedWordsTCKEYCONF[TcurrLen + 4] = Tpack4;
  localHostptr.p->packedWordsTCKEYCONF[TcurrLen + 5] = Tpack5;
  
  UintR Ti;
  for (Ti = 6; Ti < TpacketLen; Ti++) {
    localHostptr.p->packedWordsTCKEYCONF[TcurrLen + Ti] = 
      regApiPtr->tcSendArray[Ti - 6];
  }//for
  localHostptr.p->packedWordsTCKEYCONF[TcurrLen + TpacketLen] = Tpack6;

  if (unlikely(!ndb_check_micro_gcp(getNodeInfo(localHostptr.i).m_version)))
  {
    jam();
    ndbassert(Tpack6 == 0 || getNodeInfo(localHostptr.i).m_connected == false);
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
  jamEntry();
  for (i = 0; i < TpackedListIndex; i++) {
    Thostptr.i = cpackedList[i];
    ptrAss(Thostptr, localHostRecord);
    arrGuard(Thostptr.i - 1, MAX_NODES - 1);
    UintR TnoOfPackedWordsLqh = Thostptr.p->noOfPackedWordsLqh;
    UintR TnoOfWordsTCKEYCONF = Thostptr.p->noOfWordsTCKEYCONF;
    UintR TnoOfWordsTCINDXCONF = Thostptr.p->noOfWordsTCINDXCONF;
    jam();
    if (TnoOfPackedWordsLqh > 0) {
      jam();
      sendPackedSignalLqh(signal, Thostptr.p);
    }//if
    if (TnoOfWordsTCKEYCONF > 0) {
      jam();
      sendPackedTCKEYCONF(signal, Thostptr.p, (Uint32)Thostptr.i);
    }//if
    if (TnoOfWordsTCINDXCONF > 0) {
      jam();
      sendPackedTCINDXCONF(signal, Thostptr.p, (Uint32)Thostptr.i);
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
    jam();
    ahostptr->inPackedList = true;
    cpackedList[TpackedListIndex] = ahostIndex;
    cpackedListIndex = TpackedListIndex + 1;
  }//if
}//Dbtc::updatePackedList()

void Dbtc::sendPackedSignalLqh(Signal* signal, HostRecord * ahostptr)
{
  UintR Tj;
  UintR TnoOfWords = ahostptr->noOfPackedWordsLqh;
  for (Tj = 0; Tj < TnoOfWords; Tj += 4) {
    UintR sig0 = ahostptr->packedWordsLqh[Tj + 0];
    UintR sig1 = ahostptr->packedWordsLqh[Tj + 1];
    UintR sig2 = ahostptr->packedWordsLqh[Tj + 2];
    UintR sig3 = ahostptr->packedWordsLqh[Tj + 3];
    signal->theData[Tj + 0] = sig0;
    signal->theData[Tj + 1] = sig1;
    signal->theData[Tj + 2] = sig2;
    signal->theData[Tj + 3] = sig3;
  }//for
  ahostptr->noOfPackedWordsLqh = 0;
  sendSignal(ahostptr->hostLqhBlockRef,
             GSN_PACKED_SIGNAL,
             signal,
             TnoOfWords,
             JBB);
}//Dbtc::sendPackedSignalLqh()

void Dbtc::sendPackedTCKEYCONF(Signal* signal,
                               HostRecord * ahostptr,
                               UintR hostId)
{
  UintR Tj;
  UintR TnoOfWords = ahostptr->noOfWordsTCKEYCONF;
  BlockReference TBref = numberToRef(API_PACKED, hostId);
  for (Tj = 0; Tj < ahostptr->noOfWordsTCKEYCONF; Tj += 4) {
    UintR sig0 = ahostptr->packedWordsTCKEYCONF[Tj + 0];
    UintR sig1 = ahostptr->packedWordsTCKEYCONF[Tj + 1];
    UintR sig2 = ahostptr->packedWordsTCKEYCONF[Tj + 2];
    UintR sig3 = ahostptr->packedWordsTCKEYCONF[Tj + 3];
    signal->theData[Tj + 0] = sig0;
    signal->theData[Tj + 1] = sig1;
    signal->theData[Tj + 2] = sig2;
    signal->theData[Tj + 3] = sig3;
  }//for
  ahostptr->noOfWordsTCKEYCONF = 0;
  sendSignal(TBref, GSN_TCKEYCONF, signal, TnoOfWords, JBB);
}//Dbtc::sendPackedTCKEYCONF()

void Dbtc::sendPackedTCINDXCONF(Signal* signal,
                                HostRecord * ahostptr,
                                UintR hostId)
{
  UintR Tj;
  UintR TnoOfWords = ahostptr->noOfWordsTCINDXCONF;
  BlockReference TBref = numberToRef(API_PACKED, hostId);
  for (Tj = 0; Tj < ahostptr->noOfWordsTCINDXCONF; Tj += 4) {
    UintR sig0 = ahostptr->packedWordsTCINDXCONF[Tj + 0];
    UintR sig1 = ahostptr->packedWordsTCINDXCONF[Tj + 1];
    UintR sig2 = ahostptr->packedWordsTCINDXCONF[Tj + 2];
    UintR sig3 = ahostptr->packedWordsTCINDXCONF[Tj + 3];
    signal->theData[Tj + 0] = sig0;
    signal->theData[Tj + 1] = sig1;
    signal->theData[Tj + 2] = sig2;
    signal->theData[Tj + 3] = sig3;
  }//for
  ahostptr->noOfWordsTCINDXCONF = 0;
  sendSignal(TBref, GSN_TCINDXCONF, signal, TnoOfWords, JBB);
}//Dbtc::sendPackedTCINDXCONF()

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
  if (ERROR_INSERTED(8022)) {
    jam();
    systemErrorLab(signal, __LINE__);
  }//if

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
      EXECUTE_DIRECT(DBDIH, GSN_DIVERIFYREQ, signal, 1);
      if (signal->theData[3] == 0) {
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
  regApiPtr->apiCopyRecord = locApiConnectptr.i;
  regApiPtr->triggerPending = false;
  regApiPtr->isIndexOp = false;
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
  if (TApifailureNr != Tfailure_nr) {
    DIVER_node_fail_handling(signal, Tgci);
    return;
  }//if
  commitGciHandling(signal, Tgci);

  /**************************************************************************
   *                                 C O M M I T                      
   * THE TRANSACTION HAVE NOW BEEN VERIFIED AND NOW THE COMMIT PHASE CAN START 
   **************************************************************************/

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
  ApiConnectRecord * const regApiPtr = apiConnectptr.p;
  GcpRecord *localGcpRecord = gcpRecord;

  regApiPtr->globalcheckpointid = Tgci;
  if (Tfirstgcp != RNIL) {
    /* IF THIS GLOBAL CHECKPOINT ALREADY EXISTS */
    localGcpPointer.i = Tfirstgcp;
    ptrCheckGuard(localGcpPointer, TgcpFilesize, localGcpRecord);
    do {
      if (regApiPtr->globalcheckpointid == localGcpPointer.p->gcpId) {
        jam();
        gcpPtr.i = localGcpPointer.i;
        gcpPtr.p = localGcpPointer.p;
        linkApiToGcp(signal);
        return;
      } else {
        localGcpPointer.i = localGcpPointer.p->nextGcp;
        jam();
        if (localGcpPointer.i != RNIL) {
          jam();
          ptrCheckGuard(localGcpPointer, TgcpFilesize, localGcpRecord);
          continue;
        }//if
      }//if
      seizeGcp(signal);
      linkApiToGcp(signal);
      return;
    } while (1);
  } else {
    jam();
    seizeGcp(signal);
    linkApiToGcp(signal);
  }//if
}//Dbtc::commitGciHandling()

/* --------------------------------------------------------------------------*/
/* -LINK AN API CONNECT RECORD IN STATE PREPARED INTO THE LIST WITH GLOBAL - */
/* CHECKPOINTS. WHEN THE TRANSACTION I COMPLETED THE API CONNECT RECORD IS   */
/* LINKED OUT OF THE LIST.                                                   */
/*---------------------------------------------------------------------------*/
void Dbtc::linkApiToGcp(Signal* signal) 
{
  ApiConnectRecordPtr localApiConnectptr;
  ApiConnectRecord * const regApiPtr = apiConnectptr.p;
  GcpRecord * const regGcpPtr = gcpPtr.p;
  UintR TapiConnectptrIndex = apiConnectptr.i;
  ApiConnectRecord *localApiConnectRecord = apiConnectRecord;

  regApiPtr->nextGcpConnect = RNIL;
  if (regGcpPtr->firstApiConnect == RNIL) {
    regGcpPtr->firstApiConnect = TapiConnectptrIndex;
    jam();
  } else {
    UintR TapiConnectFilesize = capiConnectFilesize;
    localApiConnectptr.i = regGcpPtr->lastApiConnect;
    jam();
    ptrCheckGuard(localApiConnectptr,
                  TapiConnectFilesize, localApiConnectRecord);
    localApiConnectptr.p->nextGcpConnect = TapiConnectptrIndex;
  }//if
  UintR TlastApiConnect = regGcpPtr->lastApiConnect;
  regApiPtr->gcpPointer = gcpPtr.i;
  regApiPtr->prevGcpConnect = TlastApiConnect;
  regGcpPtr->lastApiConnect = TapiConnectptrIndex;
}//Dbtc::linkApiToGcp()

void Dbtc::seizeGcp(Signal* signal) 
{
  GcpRecordPtr tmpGcpPointer;
  GcpRecordPtr localGcpPointer;

  UintR Tfirstgcp = cfirstgcp;
  Uint64 Tglobalcheckpointid = apiConnectptr.p->globalcheckpointid;
  UintR TgcpFilesize = cgcpFilesize;
  GcpRecord *localGcpRecord = gcpRecord;

  localGcpPointer.i = cfirstfreeGcp;
  ptrCheckGuard(localGcpPointer, TgcpFilesize, localGcpRecord);
  UintR TfirstfreeGcp = localGcpPointer.p->nextGcp;
  localGcpPointer.p->gcpId = Tglobalcheckpointid;
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
  gcpPtr = localGcpPointer;
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
    localTcConnectptr.p->tcConnectstate = OS_COMMITTING;
    sendCommitLqh(signal, localTcConnectptr.p);

    if (localTcConnectptr.i != RNIL) {
      Tcount = Tcount + 1;
      if (Tcount < 16) {
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
        signal->theData[0] = TcContinueB::ZSEND_COMMIT_LOOP;
        signal->theData[1] = apiConnectptr.i;
        signal->theData[2] = localTcConnectptr.i;
        sendSignal(cownref, GSN_CONTINUEB, signal, 3, JBB);
        return;
      }//if
    } else {
      jam();
      regApiPtr->apiConnectstate = CS_COMMIT_SENT;
      return;
    }//if
  } while (1);
}//Dbtc::commit020Lab()

void Dbtc::sendCommitLqh(Signal* signal,
                         TcConnectRecord * const regTcPtr)
{
  HostRecordPtr Thostptr;
  UintR ThostFilesize = chostFilesize;
  ApiConnectRecord * const regApiPtr = apiConnectptr.p;
  Thostptr.i = regTcPtr->lastLqhNodeId;
  ptrCheckGuard(Thostptr, ThostFilesize, hostRecord);
  if (Thostptr.p->noOfPackedWordsLqh > 25 - 5) {
    jam();
    sendPackedSignalLqh(signal, Thostptr.p);
  } else {
    jam();
    updatePackedList(signal, Thostptr.p, Thostptr.i);
  }//if
  UintR Tindex = Thostptr.p->noOfPackedWordsLqh;
  UintR* TDataPtr = &Thostptr.p->packedWordsLqh[Tindex];
  UintR Tdata1 = regTcPtr->lastLqhCon;
  UintR Tdata2 = Uint32(regApiPtr->globalcheckpointid >> 32);
  UintR Tdata3 = regApiPtr->transid[0];
  UintR Tdata4 = regApiPtr->transid[1];
  UintR Tdata5 = Uint32(regApiPtr->globalcheckpointid);

  TDataPtr[0] = Tdata1 | (ZCOMMIT << 28);
  TDataPtr[1] = Tdata2;
  TDataPtr[2] = Tdata3;
  TDataPtr[3] = Tdata4;
  TDataPtr[4] = Tdata5;
  Thostptr.p->noOfPackedWordsLqh = Tindex + 5;

  if (unlikely(!ndb_check_micro_gcp(getNodeInfo(Thostptr.i).m_version)))
  {
    jam();
    ndbassert(Tdata5 == 0 || getNodeInfo(Thostptr.i).m_connected == false);
    Thostptr.p->noOfPackedWordsLqh = Tindex + 4; // no gci_lo
  }
}//Dbtc::sendCommitLqh()

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

  UintR TtcConnectFilesize = ctcConnectFilesize;
  UintR TapiConnectFilesize = capiConnectFilesize;
  TcConnectRecord *localTcConnectRecord = tcConnectRecord;
  ApiConnectRecord *localApiConnectRecord = apiConnectRecord;

#ifdef ERROR_INSERT
  if (ERROR_INSERTED(8018)) {
    CLEAR_ERROR_INSERT_VALUE;
    return;
  }//if
  if (ERROR_INSERTED(8030)) {
    systemErrorLab(signal, __LINE__);
  }//if
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
  sendApiCommit(signal);

  ApiConnectRecord * const regApiPtr = apiConnectptr.p;
  localTcConnectptr.i = regApiPtr->firstTcConnect;
  UintR Tlqhkeyconfrec = regApiPtr->lqhkeyconfrec;
  ptrCheckGuard(localTcConnectptr, TtcConnectFilesize, localTcConnectRecord);
  regApiPtr->counter = Tlqhkeyconfrec;

  tcConnectptr = localTcConnectptr;
  complete010Lab(signal);
  return;

}//Dbtc::execCOMMITTED()

/*-------------------------------------------------------*/
/*                       SEND_API_COMMIT                 */
/*       SEND COMMIT DECISION TO THE API.                */
/*-------------------------------------------------------*/
void Dbtc::sendApiCommit(Signal* signal) 
{
  ApiConnectRecord * const regApiPtr = apiConnectptr.p;

  if (ERROR_INSERTED(8055))
  {
    /**
     * 1) Kill self
     * 2) Disconnect API
     * 3) Prevent execAPI_FAILREQ from handling trans...
     */
    signal->theData[0] = 9999;
    sendSignalWithDelay(CMVMI_REF, GSN_NDB_TAMPER, signal, 1000, 1);
    
    Uint32 node = refToNode(regApiPtr->ndbapiBlockref);
    signal->theData[0] = node;
    sendSignal(QMGR_REF, GSN_API_FAILREQ, signal, 1, JBB);

    SET_ERROR_INSERT_VALUE(8056);
    return;
  }
  
  if (regApiPtr->returnsignal == RS_TCKEYCONF) {
    sendtckeyconf(signal, 1);
  } else if (regApiPtr->returnsignal == RS_TC_COMMITCONF) {
    jam();
    TcCommitConf * const commitConf = (TcCommitConf *)&signal->theData[0];
    if(regApiPtr->commitAckMarker == RNIL){
      jam();
      commitConf->apiConnectPtr = regApiPtr->ndbapiConnect;
    } else {
      jam();
      commitConf->apiConnectPtr = regApiPtr->ndbapiConnect | 1;
    }
    commitConf->transId1 = regApiPtr->transid[0];
    commitConf->transId2 = regApiPtr->transid[1];
    commitConf->gci_hi = Uint32(regApiPtr->globalcheckpointid >> 32);
    commitConf->gci_lo = Uint32(regApiPtr->globalcheckpointid);

    sendSignal(regApiPtr->ndbapiBlockref, GSN_TC_COMMITCONF, signal,
	       TcCommitConf::SignalLength, JBB);
  } else if (regApiPtr->returnsignal == RS_NO_RETURN) {
    jam();
  } else {
    TCKEY_abort(signal, 37);
    return;
  }//if
  UintR TapiConnectFilesize = capiConnectFilesize;
  UintR TcommitCount = c_counters.ccommitCount;
  UintR TapiIndex = apiConnectptr.i;
  UintR TnewApiIndex = regApiPtr->apiCopyRecord;
  UintR TapiFailState = regApiPtr->apiFailState;
  ApiConnectRecord *localApiConnectRecord = apiConnectRecord;

  tmpApiConnectptr.p = apiConnectptr.p;
  tmpApiConnectptr.i = TapiIndex;
  c_counters.ccommitCount = TcommitCount + 1;
  apiConnectptr.i = TnewApiIndex;
  ptrCheckGuard(apiConnectptr, TapiConnectFilesize, localApiConnectRecord);
  copyApi(signal);
  if (TapiFailState != ZTRUE) {
    return;
  } else {
    jam();
    handleApiFailState(signal, tmpApiConnectptr.i);
    return;
  }//if
}//Dbtc::sendApiCommit()

/* ========================================================================= */
/* =======                  COPY_API                                 ======= */
/*   COPY API RECORD ALSO RESET THE OLD API RECORD SO THAT IT                */
/*   IS PREPARED TO RECEIVE A NEW TRANSACTION.                               */
/*===========================================================================*/
void Dbtc::copyApi(Signal* signal) 
{
  ApiConnectRecord * const regApiPtr = apiConnectptr.p;
  ApiConnectRecord * const regTmpApiPtr = tmpApiConnectptr.p;

  UintR TndbapiConnect = regTmpApiPtr->ndbapiConnect;
  UintR TfirstTcConnect = regTmpApiPtr->firstTcConnect;
  UintR Ttransid1 = regTmpApiPtr->transid[0];
  UintR Ttransid2 = regTmpApiPtr->transid[1];
  UintR Tlqhkeyconfrec = regTmpApiPtr->lqhkeyconfrec;
  UintR TgcpPointer = regTmpApiPtr->gcpPointer;
  UintR TgcpFilesize = cgcpFilesize;
  UintR TcommitAckMarker = regTmpApiPtr->commitAckMarker;
  NdbNodeBitmask Tnodes = regTmpApiPtr->m_transaction_nodes;
  GcpRecord *localGcpRecord = gcpRecord;

  regApiPtr->ndbapiBlockref = regTmpApiPtr->ndbapiBlockref;
  regApiPtr->ndbapiConnect = TndbapiConnect;
  regApiPtr->firstTcConnect = TfirstTcConnect;
  regApiPtr->apiConnectstate = CS_COMPLETING;
  regApiPtr->transid[0] = Ttransid1;
  regApiPtr->transid[1] = Ttransid2;
  regApiPtr->lqhkeyconfrec = Tlqhkeyconfrec;
  regApiPtr->commitAckMarker = TcommitAckMarker;
  regApiPtr->m_transaction_nodes = Tnodes;
  regApiPtr->singleUserMode = 0;

  gcpPtr.i = TgcpPointer;
  ptrCheckGuard(gcpPtr, TgcpFilesize, localGcpRecord);
  unlinkApiConnect(signal);
  linkApiToGcp(signal);
  setApiConTimer(tmpApiConnectptr.i, 0, __LINE__);
  regTmpApiPtr->apiConnectstate = CS_CONNECTED;
  regTmpApiPtr->commitAckMarker = RNIL;
  regTmpApiPtr->firstTcConnect = RNIL;
  regTmpApiPtr->lastTcConnect = RNIL;
  regTmpApiPtr->m_transaction_nodes.clear();
  regTmpApiPtr->singleUserMode = 0;
  releaseAllSeizedIndexOperations(regTmpApiPtr);
}//Dbtc::copyApi()

void Dbtc::unlinkApiConnect(Signal* signal) 
{
  ApiConnectRecordPtr localApiConnectptr;
  ApiConnectRecord * const regTmpApiPtr = tmpApiConnectptr.p;
  UintR TapiConnectFilesize = capiConnectFilesize;
  UintR TprevGcpConnect = regTmpApiPtr->prevGcpConnect;
  UintR TnextGcpConnect = regTmpApiPtr->nextGcpConnect;
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
    sendCompleteLqh(signal, localTcConnectptr.p);
    localTcConnectptr.i = nextTcConnect;
    if (localTcConnectptr.i != RNIL) {
      Tcount++;
      if (Tcount < 16) {
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
        sendSignal(cownref, GSN_CONTINUEB, signal, 3, JBB);
        return;
      }//if
    } else {
      jam();
      regApiPtr->apiConnectstate = CS_COMPLETE_SENT;
      return;
    }//if
  } while (1);
}//Dbtc::complete010Lab()

void Dbtc::sendCompleteLqh(Signal* signal,
                           TcConnectRecord * const regTcPtr)
{
  HostRecordPtr Thostptr;
  UintR ThostFilesize = chostFilesize;
  ApiConnectRecord * const regApiPtr = apiConnectptr.p;
  Thostptr.i = regTcPtr->lastLqhNodeId;
  ptrCheckGuard(Thostptr, ThostFilesize, hostRecord);
  if (Thostptr.p->noOfPackedWordsLqh > 22) {
    jam();
    sendPackedSignalLqh(signal, Thostptr.p);
  } else {
    jam();
    updatePackedList(signal, Thostptr.p, Thostptr.i);
  }//if

  UintR Tindex = Thostptr.p->noOfPackedWordsLqh;
  UintR* TDataPtr = &Thostptr.p->packedWordsLqh[Tindex];
  UintR Tdata1 = regTcPtr->lastLqhCon | (ZCOMPLETE << 28);
  UintR Tdata2 = regApiPtr->transid[0];
  UintR Tdata3 = regApiPtr->transid[1];
  
  TDataPtr[0] = Tdata1;
  TDataPtr[1] = Tdata2;
  TDataPtr[2] = Tdata3;
  Thostptr.p->noOfPackedWordsLqh = Tindex + 3;
}//Dbtc::sendCompleteLqh()

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
  sendRemoveMarkers(signal, removedMarker.p);
  m_commitAckMarkerPool.release(removedMarker);
}

void
Dbtc::sendRemoveMarkers(Signal* signal, const CommitAckMarker * marker){
  jam();
  const Uint32 noOfLqhs = marker->noOfLqhs;
  const Uint32 transId1 = marker->transid1;
  const Uint32 transId2 = marker->transid2;
  
  for(Uint32 i = 0; i<noOfLqhs; i++){
    jam();
    const NodeId nodeId = marker->lqhNodeId[i];
    sendRemoveMarker(signal, nodeId, transId1, transId2);
  }
}

void
Dbtc::sendRemoveMarker(Signal* signal, 
                       NodeId nodeId,
                       Uint32 transid1, 
                       Uint32 transid2){
  /**
   * Seize host ptr
   */
  HostRecordPtr hostPtr;
  const UintR ThostFilesize = chostFilesize;
  hostPtr.i = nodeId;
  ptrCheckGuard(hostPtr, ThostFilesize, hostRecord);

  if (hostPtr.p->noOfPackedWordsLqh > (25 - 3)){
    jam();
    sendPackedSignalLqh(signal, hostPtr.p);
  } else {
    jam();
    updatePackedList(signal, hostPtr.p, hostPtr.i);
  }//if
  
  UintR  numWord = hostPtr.p->noOfPackedWordsLqh;
  UintR* dataPtr = &hostPtr.p->packedWordsLqh[numWord];

  dataPtr[0] = (ZREMOVE_MARKER << 28);
  dataPtr[1] = transid1;
  dataPtr[2] = transid2;
  hostPtr.p->noOfPackedWordsLqh = numWord + 3;
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
  apiConnectptr = localApiConnectptr;
  releaseTransResources(signal);
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
  handleGcp(signal);
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
void Dbtc::handleGcp(Signal* signal) 
{
  GcpRecord *localGcpRecord = gcpRecord;
  GcpRecordPtr localGcpPtr;
  UintR TapiConnectptrIndex = apiConnectptr.i;
  UintR TgcpFilesize = cgcpFilesize;
  localGcpPtr.i = apiConnectptr.p->gcpPointer;
  tmpApiConnectptr.i = TapiConnectptrIndex;
  tmpApiConnectptr.p = apiConnectptr.p;
  ptrCheckGuard(localGcpPtr, TgcpFilesize, localGcpRecord);
  gcpPtr.i = localGcpPtr.i;
  gcpPtr.p = localGcpPtr.p;
  unlinkApiConnect(signal);
  if (localGcpPtr.p->firstApiConnect == RNIL) {
    if (localGcpPtr.p->gcpNomoretransRec == ZTRUE) {
      jam();
      tcheckGcpId = localGcpPtr.p->gcpId;
      gcpTcfinished(signal);
      unlinkGcp(signal);
    }//if
  }//if
}//Dbtc::handleGcp()

void Dbtc::releaseApiConCopy(Signal* signal) 
{
  ApiConnectRecord * const regApiPtr = apiConnectptr.p;
  UintR TfirstfreeApiConnectCopyOld = cfirstfreeApiConnectCopy;
  cfirstfreeApiConnectCopy = apiConnectptr.i;
  regApiPtr->nextApiConnect = TfirstfreeApiConnectCopyOld;
  setApiConTimer(apiConnectptr.i, 0, __LINE__);
  regApiPtr->apiConnectstate = CS_RESTART;
}//Dbtc::releaseApiConCopy()

/* ========================================================================= */
/* -------  RELEASE ALL RECORDS CONNECTED TO A DIRTY WRITE OPERATION ------- */
/* ========================================================================= */
void Dbtc::releaseDirtyWrite(Signal* signal) 
{
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

      const ConnectionState state = regApiPtr->apiConnectstate;
      const Uint32 triggeringOp = regTcPtr->triggeringOperation;
      if (triggeringOp != RNIL) {
        jam();
	// This operation was created by a trigger execting operation
	TcConnectRecordPtr opPtr;
	TcConnectRecord *localTcConnectRecord = tcConnectRecord;
       
	const Uint32 currentIndexId = regTcPtr->currentIndexId;
	ndbassert(currentIndexId != 0); // Only index triggers so far
	
	opPtr.i = triggeringOp;
	ptrCheckGuard(opPtr, ctcConnectFilesize, localTcConnectRecord);
	
	// The operation executed an index trigger
        TcIndexData* indexData = c_theIndexes.getPtr(currentIndexId);
        indexId = indexData->indexId;
        regApiPtr->errorData = indexId;
	const Uint32 opType = regTcPtr->operation;
	if (errCode == ZALREADYEXIST)
	  errCode = terrorCode = ZNOTUNIQUE;
	else if (!(opType == ZDELETE && errCode == ZNOT_FOUND)) {
	  jam();
	  /**
	   * "Normal path"
	   */
	  // fall-through
	} else {
	  jam();
	  /** ZDELETE && NOT_FOUND */
	  if(indexData->indexState == IS_BUILDING && state != CS_ABORTING){
	    jam();
	    /**
	     * Ignore error
	     */
	    regApiPtr->lqhkeyconfrec++;
	    
	    unlinkReadyTcCon(signal);
	    releaseTcCon();
	    
	    opPtr.p->triggerExecutionCount--;
	    if (opPtr.p->triggerExecutionCount == 0) {
	      /**
	       * We have completed current trigger execution
	       * Continue triggering operation
	       */
	      jam();
	      continueTriggeringOp(signal, opPtr.p);
	    }
	    return;
	  }
	}
      }
      
      Uint32 marker = regTcPtr->commitAckMarker;
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

      if (marker != RNIL){
	/**
	 * This was an insert/update/delete/write which failed
	 *   that contained the marker
	 * Currently unsupported to place new marker
	 */
	TCKEY_abort(signal, 49);
	return;
      }
      
      /* Only ref in certain situations */
      {
	const Uint32 opType = regTcPtr->operation;
	if (   (opType == ZDELETE && errCode != ZNOT_FOUND)
	    || (opType == ZINSERT && errCode != ZALREADYEXIST)
	    || (opType == ZUPDATE && errCode != ZNOT_FOUND)
	    || (opType == ZWRITE  && errCode != 839 && errCode != 840))
	{
	  TCKEY_abort(signal, 49);
	  return;
	}
      }

      /* *************** */
      /*    TCKEYREF   < */
      /* *************** */
      TcKeyRef * const tcKeyRef = (TcKeyRef *) signal->getDataPtrSend();
      tcKeyRef->transId[0] = regApiPtr->transid[0];
      tcKeyRef->transId[1] = regApiPtr->transid[1];
      tcKeyRef->errorCode = terrorCode;
      bool isIndexOp = regTcPtr->isIndexOp;
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
	} else if (regApiPtr->tckeyrec > 0 || regApiPtr->m_exec_flag) {
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
  const Uint32 commitAckMarker = regTcPtr->commitAckMarker;
  if (regApiPtr->commitAckMarker == RNIL)
    ndbassert(commitAckMarker == RNIL);
  if (commitAckMarker != RNIL)
    ndbassert(regApiPtr->commitAckMarker != RNIL);
  if(commitAckMarker != RNIL){
    jam();
    m_commitAckMarkerHash.release(commitAckMarker);
    regTcPtr->commitAckMarker = RNIL;
    regApiPtr->commitAckMarker = RNIL;
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

    regApiPtr->m_exec_flag = 1;
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
    case CS_PREPARED:
      jam();
      return;
    case CS_START_PREPARING:
      jam();
      return;
    case CS_REC_PREPARING:
      jam();
      return;
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

  apiConnectptr.p->m_exec_flag = 1;
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
  case CS_START_PREPARING:
    jam();
  case CS_PREPARED:
    jam();
  case CS_REC_PREPARING:
    jam();
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
  case OS_OPERATING:
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
    if (TloopCount < 1024) {
      goto ABORT020;
    } else {
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
      sendSignal(cownref, GSN_CONTINUEB, signal, 3, JBB);
      return;
    }//if
  }//if
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
      tblockref = calcLqhBlockRef(localHostptr.i);
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
  ctcTimer++;
  if (csystemStart != SSS_TRUE) {
    jam();
    return;
  }//if
  checkStartTimeout(signal);
  checkStartFragTimeout(signal);
}//Dbtc::execTIME_SIGNAL()

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
  timeOutLoopStartLab(signal, 0); // 0 is first api connect record
  return;
}//Dbtc::execTIME_SIGNAL()

/*----------------------------------------------------------------*/
/* Start fragment (scan) timeout handling if not already going on */
/*----------------------------------------------------------------*/
void Dbtc::checkStartFragTimeout(Signal* signal)
{
  ctimeOutCheckFragCounter++;
  if (ctimeOutCheckFragActive == TOCS_TRUE) {
    jam();
    return;
  }//if
  if (ctimeOutCheckFragCounter < ctimeOutCheckDelay) {
    jam();
    /*------------------------------------------------------------------*/
    /*       NO TIME-OUT CHECKED THIS TIME. WAIT MORE.                  */
    /*------------------------------------------------------------------*/
    return;
  }//if

  // Go through the fragment records and look for timeout in a scan.
  ctimeOutCheckFragActive = TOCS_TRUE;
  ctimeOutCheckFragCounter = 0;
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
    jam();
    if (api_timer != 0) {
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
	<< " - exec: " << apiConnectptr.p->m_exec_flag
	<< " - place: " << c_apiConTimer_line[apiConnectptr.i]
	<< " code: " << errCode);
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
  case CS_COMPLETING:
    jam();
    /*------------------------------------------------------------------*/
    // We are simply waiting for a signal in the job buffer. Only extreme
    // conditions should get us here. We ignore it.
    /*------------------------------------------------------------------*/
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
      systemErrorLab(signal, __LINE__);
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
    jam();
  case CS_FAIL_COMMITTING:
    jam();
  case CS_FAIL_COMMITTED:
    jam();
  case CS_REC_PREPARING:
    jam();
  case CS_START_PREPARING:
    jam();
  case CS_PREPARED:
    jam();
  case CS_RESTART:
    jam();
  case CS_FAIL_ABORTED:
    jam();
  case CS_DISCONNECTED:
    jam();
  default:
    jam();
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
    ndbrequire(false);
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
	  BaseString::snprintf(buf, sizeof(buf), buf2);
	}
	warningEvent(buf);
	ndbout_c(buf);
	ndbrequire(false);
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
      if (ERROR_INSERTED(8050))
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
            BlockReference TBRef = calcLqhBlockRef(hostptr.i);
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

void Dbtc::reportNodeFailed(Signal* signal, Uint32 nodeId)
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
  DEBUG(TscanConPtr << " timeOutFoundFragLab: scanFragState = "<< ptr.p->scanFragState);

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
    ndbrequire(false);
    break;
  case ScanFragRec::LQH_ACTIVE:{
    jam();

    /**  
     * The LQH expired it's timeout, try to close it
     */    
    Uint32 nodeId = refToNode(ptr.p->lqhBlockref);
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
      ScanFragList run(c_scan_frag_pool, scanptr.p->m_running_scan_frags);
      
      run.release(ptr);
      ptr.p->stopFragTimer();
    }
    
    scanError(signal, scanptr, ZSCAN_FRAG_LQH_ERROR);
    break;
  }
  case ScanFragRec::DELIVERED:
    jam();
  case ScanFragRec::IDLE:
    jam();
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
  c_gcp_ref = req->senderData;
  Uint32 gci_lo = req->gci_lo;
  Uint32 gci_hi = req->gci_hi;
  tcheckGcpId = gci_lo | (Uint64(gci_hi) << 32);
  if (cfirstgcp != RNIL) {
    jam();
                                      /* A GLOBAL CHECKPOINT IS GOING ON */
    gcpPtr.i = cfirstgcp;             /* SET POINTER TO FIRST GCP IN QUEUE*/
    ptrCheckGuard(gcpPtr, cgcpFilesize, gcpRecord);
    if (gcpPtr.p->gcpId == tcheckGcpId) {
      jam();
      if (gcpPtr.p->firstApiConnect != RNIL) {
        jam();
        gcpPtr.p->gcpNomoretransRec = ZTRUE;
      } else {
        jam();
        gcpTcfinished(signal);
        unlinkGcp(signal);
      }//if
    } else {
      jam();
      /*------------------------------------------------------------*/
      /*       IF IT IS NOT THE FIRST THEN THERE SHOULD BE NO       */
      /*       RECORD FOR THIS GLOBAL CHECKPOINT. WE ALWAYS REMOVE  */
      /*       THE GLOBAL CHECKPOINTS IN ORDER.                     */
      /*------------------------------------------------------------*/
      gcpTcfinished(signal);
    }//if
  } else {
    jam();
    gcpTcfinished(signal);
  }//if
  return;
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
  
  tcNodeFailptr.i = 0;
  ptrAss(tcNodeFailptr, tcFailRecord);
  for (i = 0; i < tnoOfNodes; i++) 
  {
    jam();
    hostptr.i = cdata[i];
    ptrCheckGuard(hostptr, chostFilesize, hostRecord);
    
    /*------------------------------------------------------------*/
    /*       SET STATUS OF THE FAILED NODE TO DEAD SINCE IT HAS   */
    /*       FAILED.                                              */
    /*------------------------------------------------------------*/
    hostptr.p->hostStatus = HS_DEAD;
    hostptr.p->m_nf_bits = HostRecord::NF_NODE_FAIL_BITS;
    c_alive_nodes.clear(hostptr.i);

    if (tcNodeFailptr.p->failStatus == FS_LISTENING) 
    {
      jam();
      /*------------------------------------------------------------*/
      /*       THE CURRENT TAKE OVER CAN BE AFFECTED BY THIS NODE   */
      /*       FAILURE.                                             */
      /*------------------------------------------------------------*/
      if (hostptr.p->lqhTransStatus == LTS_ACTIVE) 
      {
	jam();
	/*------------------------------------------------------------*/
	/*       WE WERE WAITING FOR THE FAILED NODE IN THE TAKE OVER */
	/*       PROTOCOL FOR TC.                                     */
	/*------------------------------------------------------------*/
	signal->theData[0] = TcContinueB::ZNODE_TAKE_OVER_COMPLETED;
	signal->theData[1] = hostptr.i;
	sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
      }//if
    }//if
    
    if (getOwnNodeId() != tnewMasterId)
    {
      jam();
      /**
       * Only master does takeover currently
       */
      hostptr.p->m_nf_bits &= ~HostRecord::NF_TAKEOVER;
    }
    else
    {
      jam();
      signal->theData[0] = hostptr.i;
      sendSignal(cownref, GSN_TAKE_OVERTCREQ, signal, 1, JBB);
    }

    checkScanActiveInFailedLqh(signal, 0, hostptr.i);
    checkWaitDropTabFailedLqh(signal, hostptr.i, 0); // nodeid, tableid
    nodeFailCheckTransactions(signal, 0, hostptr.i);
  }
}//Dbtc::execNODE_FAILREP()

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
    NFCompleteRep * const nfRep = (NFCompleteRep *)&signal->theData[0];
    nfRep->blockNo      = DBTC;
    nfRep->nodeId       = cownNodeid;
    nfRep->failedNodeId = hostptr.i;
    sendSignal(cdihblockref, GSN_NF_COMPLETEREP, signal, 
	       NFCompleteRep::SignalLength, JBB);
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
      ScanFragList run(c_scan_frag_pool, scanptr.p->m_running_scan_frags);
      
      for(run.first(ptr); !ptr.isNull(); ){
	jam();
	ScanFragRecPtr curr = ptr;
	run.next(ptr);
	if (curr.p->scanFragState == ScanFragRec::LQH_ACTIVE && 
	    refToNode(curr.p->lqhBlockref) == failedNodeId){
	  jam();
	  
	  run.release(curr);
	  curr.p->scanFragState = ScanFragRec::COMPLETED;
	  curr.p->stopFragTimer();
	  found = true;
	}
      }

      ScanFragList deliv(c_scan_frag_pool, scanptr.p->m_delivered_scan_frags);
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
  for (transPtr.i = transPtrI; transPtr.i < capiConnectFilesize; transPtr.i++)
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
    }
    
    // Send CONTINUEB to continue later
    signal->theData[0] = TcContinueB::ZNF_CHECK_TRANSACTIONS;
    signal->theData[1] = transPtr.i + 1; // Check next
    signal->theData[2] = failedNodeId;
    sendSignal(cownref, GSN_CONTINUEB, signal, 3, JBB);
    return;
  }

  checkNodeFailComplete(signal, failedNodeId, 
			HostRecord::NF_CHECK_TRANSACTION);
}


void
Dbtc::checkScanFragList(Signal* signal,
			Uint32 failedNodeId,
			ScanRecord * scanP, 
			ScanFragList::Head & head){
  
  DEBUG("checkScanActiveInFailedLqh: scanFragError");
}

void Dbtc::execTAKE_OVERTCCONF(Signal* signal) 
{
  jamEntry();
  tfailedNodeId = signal->theData[0];
  hostptr.i = tfailedNodeId;
  ptrCheckGuard(hostptr, chostFilesize, hostRecord);

  if (signal->getSendersBlockRef() != reference())
  {
    jam();
    return;
  }
  
  checkNodeFailComplete(signal, hostptr.i, HostRecord::NF_TAKEOVER);
}//Dbtc::execTAKE_OVERTCCONF()

void Dbtc::execTAKE_OVERTCREQ(Signal* signal) 
{
  jamEntry();
  tfailedNodeId = signal->theData[0];
  tcNodeFailptr.i = 0;
  ptrAss(tcNodeFailptr, tcFailRecord);
  if (tcNodeFailptr.p->failStatus != FS_IDLE) {
    jam();
    /*------------------------------------------------------------*/
    /*       WE CAN CURRENTLY ONLY HANDLE ONE TAKE OVER AT A TIME */
    /*------------------------------------------------------------*/
    /*       IF MORE THAN ONE TAKE OVER IS REQUESTED WE WILL      */
    /*       QUEUE THE TAKE OVER AND START IT AS SOON AS THE      */
    /*       PREVIOUS ARE COMPLETED.                              */
    /*------------------------------------------------------------*/
    arrGuard(tcNodeFailptr.p->queueIndex, MAX_NDB_NODES);
    tcNodeFailptr.p->queueList[tcNodeFailptr.p->queueIndex] = tfailedNodeId;
    tcNodeFailptr.p->queueIndex = tcNodeFailptr.p->queueIndex + 1;
    return;
  }//if
  startTakeOverLab(signal);
}//Dbtc::execTAKE_OVERTCREQ()

/*------------------------------------------------------------*/
/*       INITIALISE THE HASH TABLES FOR STORING TRANSACTIONS  */
/*       AND OPERATIONS DURING TC TAKE OVER.                  */
/*------------------------------------------------------------*/
void Dbtc::startTakeOverLab(Signal* signal) 
{
  for (tindex = 0; tindex <= 511; tindex++) {
    ctransidFailHash[tindex] = RNIL;
  }//for
  for (tindex = 0; tindex <= 1023; tindex++) {
    ctcConnectFailHash[tindex] = RNIL;
  }//for
  tcNodeFailptr.p->failStatus = FS_LISTENING;
  tcNodeFailptr.p->takeOverNode = tfailedNodeId;
  for (hostptr.i = 1; hostptr.i < MAX_NDB_NODES; hostptr.i++) {
    jam();
    ptrAss(hostptr, hostRecord);
    if (hostptr.p->hostStatus == HS_ALIVE) {
      jam();
      tblockref = calcLqhBlockRef(hostptr.i);
      hostptr.p->lqhTransStatus = LTS_ACTIVE;
      signal->theData[0] = tcNodeFailptr.i;
      signal->theData[1] = cownref;
      signal->theData[2] = tfailedNodeId;
      sendSignal(tblockref, GSN_LQH_TRANSREQ, signal, 3, JBB);
    }//if
  }//for
}//Dbtc::startTakeOverLab()

/*------------------------------------------------------------*/
/*       A REPORT OF AN OPERATION WHERE TC FAILED HAS ARRIVED.*/
/*------------------------------------------------------------*/
void Dbtc::execLQH_TRANSCONF(Signal* signal) 
{
  jamEntry();
  LqhTransConf * const lqhTransConf = (LqhTransConf *)&signal->theData[0];
  
  tcNodeFailptr.i = lqhTransConf->tcRef;
  ptrCheckGuard(tcNodeFailptr, 1, tcFailRecord);
  tnodeid = lqhTransConf->lqhNodeId;
  ttransStatus = (LqhTransConf::OperationStatus)lqhTransConf->operationStatus;
  ttransid1    = lqhTransConf->transId1;
  ttransid2    = lqhTransConf->transId2;
  ttcOprec     = lqhTransConf->oldTcOpRec;
  treqinfo     = lqhTransConf->requestInfo;
  tgci         = Uint64(lqhTransConf->gci_hi) << 32;
  cnodes[0]    = lqhTransConf->nextNodeId1;
  cnodes[1]    = lqhTransConf->nextNodeId2;
  cnodes[2]    = lqhTransConf->nextNodeId3;
  const Uint32 ref = tapplRef = lqhTransConf->apiRef;
  tapplOprec   = lqhTransConf->apiOpRec;
  const Uint32 tableId = lqhTransConf->tableId;
  Uint32 gci_lo = lqhTransConf->gci_lo;
  if (ttransStatus == LqhTransConf::Committed && 
      unlikely(signal->getLength() < LqhTransConf::SignalLength))
  {
    jam();
    gci_lo = 0;
    ndbassert(!ndb_check_micro_gcp(getNodeInfo(tnodeid).m_version));
  }
  tgci |= gci_lo;

  if (ttransStatus == LqhTransConf::LastTransConf){
    jam();
    /*------------------------------------------------------------*/
    /*       A NODE HAS REPORTED COMPLETION OF TAKE OVER REPORTING*/
    /*------------------------------------------------------------*/
    nodeTakeOverCompletedLab(signal);
    return;
  }//if
  if (ttransStatus == LqhTransConf::Marker){
    jam();
    treqinfo = 0;
    LqhTransConf::setMarkerFlag(treqinfo, 1);
  } else {
    TableRecordPtr tabPtr;
    tabPtr.i = tableId;
    ptrCheckGuard(tabPtr, ctabrecFilesize, tableRecord);
    switch((DictTabInfo::TableType)tabPtr.p->tableType){
    case DictTabInfo::SystemTable:
    case DictTabInfo::UserTable:
      break;
    default:
      tapplRef = 0;
      tapplOprec = 0;
    }
  }

  findApiConnectFail(signal);

  if(apiConnectptr.p->ndbapiBlockref == 0 && tapplRef != 0){
    apiConnectptr.p->ndbapiBlockref = ref;
    apiConnectptr.p->ndbapiConnect = tapplOprec;
  }
  
  if (ttransStatus != LqhTransConf::Marker){
    jam();
    findTcConnectFail(signal);
  }
}//Dbtc::execLQH_TRANSCONF()

/*------------------------------------------------------------*/
/*       A NODE HAS REPORTED COMPLETION OF TAKE OVER REPORTING*/
/*------------------------------------------------------------*/
void Dbtc::nodeTakeOverCompletedLab(Signal* signal) 
{
  Uint32 guard0;

  hostptr.i = tnodeid;
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
  while (tcNodeFailptr.p->currentHashIndexTakeOver < 512){
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

      completeTransAtTakeOverDoOne(signal, TtakeOverInd);
      // One transaction taken care of, return from this function
      // and wait for the next CONTINUEB to continue processing
      break;

    } else {
      if (tcNodeFailptr.p->currentHashIndexTakeOver < 511){
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
  tcNodeFailptr.p->takeOverProcState[TtakeOverInd] = ZTAKE_OVER_IDLE;
  tcNodeFailptr.p->completedTakeOver++;

  if (tcNodeFailptr.p->completedTakeOver == cnoParallelTakeOver) {
    jam();
    /*------------------------------------------------------------*/
    /*       WE WERE THE LAST ACTIVITY THAT WAS COMPLETED. WE NEED*/
    /*       TO REPORT THE COMPLETION OF THE TAKE OVER TO ALL     */
    /*       NODES THAT ARE ALIVE.                                */
    /*------------------------------------------------------------*/
    NodeReceiverGroup rg(DBTC, c_alive_nodes);
    signal->theData[0] = tcNodeFailptr.p->takeOverNode;
    sendSignal(rg, GSN_TAKE_OVERTCCONF, signal, 1, JBB);
    
    if (tcNodeFailptr.p->queueIndex > 0) {
      jam();
      /*------------------------------------------------------------*/
      /*       THERE ARE MORE NODES TO TAKE OVER. WE NEED TO START  */
      /*       THE TAKE OVER.                                       */
      /*------------------------------------------------------------*/
      tfailedNodeId = tcNodeFailptr.p->queueList[0];
      guard0 = tcNodeFailptr.p->queueIndex - 1;
      arrGuard(guard0 + 1, MAX_NDB_NODES);
      for (tindex = 0; tindex <= guard0; tindex++) {
        jam();
        tcNodeFailptr.p->queueList[tindex] = 
          tcNodeFailptr.p->queueList[tindex + 1];
      }//for
      tcNodeFailptr.p->queueIndex--;
      startTakeOverLab(signal);
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
Dbtc::sendTCKEY_FAILREF(Signal* signal, const ApiConnectRecord * regApiPtr){
  jam();

  const Uint32 ref = regApiPtr->ndbapiBlockref;
  const Uint32 nodeId = refToNode(ref);
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
}

void 
Dbtc::sendTCKEY_FAILCONF(Signal* signal, ApiConnectRecord * regApiPtr){
  jam();
  TcKeyFailConf * const failConf = (TcKeyFailConf *)&signal->theData[0];
  
  const Uint32 ref = regApiPtr->ndbapiBlockref;
  const Uint32 marker = regApiPtr->commitAckMarker;
  const Uint32 nodeId = refToNode(ref);
  if(ref != 0)
  {
    jam()
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
  EXECUTE_DIRECT(DBDIH, GSN_CHECKNODEGROUPSREQ, signal, 
		 CheckNodeGroups::SignalLength);
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
  tnodeid = signal->theData[2];
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
      tnodeid) {
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
          tblockref = calcLqhBlockRef(hostptr.i);
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
	  const Uint32 marker = apiConnectptr.p->commitAckMarker;
          if(marker != RNIL){
	    jam();

            CommitAckMarkerPtr tmp;
            tmp.i = marker;
            tmp.p = m_commitAckMarkerHash.getPtr(tmp.i);
            
            m_commitAckMarkerHash.release(tmp);
            apiConnectptr.p->commitAckMarker = RNIL;
          }
          
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
  tnodeid = signal->theData[1];
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
      tnodeid) {
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
          tblockref = calcLqhBlockRef(hostptr.i);
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
          sendApiCommit(signal);
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
  tnodeid = signal->theData[1];
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
      tnodeid) {
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
          tblockref = calcLqhBlockRef(hostptr.i);
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
          releaseTakeOver(signal);
        } else {
          jam();
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

/*------------------------------------------------------------*/
/*                                                            */
/*       FIND THE API CONNECT RECORD FOR THIS TRANSACTION     */
/*       DURING TAKE OVER FROM A FAILED TC. IF NONE EXISTS    */
/*       YET THEN SEIZE A NEW API CONNECT RECORD AND LINK IT  */
/*       INTO THE HASH TABLE.                                 */
/*------------------------------------------------------------*/
void Dbtc::findApiConnectFail(Signal* signal) 
{
  ApiConnectRecordPtr fafPrevApiConnectptr;
  ApiConnectRecordPtr fafNextApiConnectptr;
  UintR tfafHashNumber;

  tfafHashNumber = ttransid1 & 511;
  fafPrevApiConnectptr.i = RNIL;
  ptrNull(fafPrevApiConnectptr);
  arrGuard(tfafHashNumber, 512);
  fafNextApiConnectptr.i = ctransidFailHash[tfafHashNumber];
  ptrCheck(fafNextApiConnectptr, capiConnectFilesize, apiConnectRecord);
FAF_LOOP:
  jam();
  if (fafNextApiConnectptr.i == RNIL) {
    jam();
    if (cfirstfreeApiConnectFail == RNIL) {
      jam();
      systemErrorLab(signal, __LINE__);
      return;
    }//if
    seizeApiConnectFail(signal);
    if (fafPrevApiConnectptr.i == RNIL) {
      jam();
      ctransidFailHash[tfafHashNumber] = apiConnectptr.i;
    } else {
      jam();
      ptrGuard(fafPrevApiConnectptr);
      fafPrevApiConnectptr.p->nextApiConnect = apiConnectptr.i;
    }//if
    apiConnectptr.p->nextApiConnect = RNIL;
    initApiConnectFail(signal);
  } else {
    jam();
    fafPrevApiConnectptr.i = fafNextApiConnectptr.i;
    fafPrevApiConnectptr.p = fafNextApiConnectptr.p;
    apiConnectptr.i = fafNextApiConnectptr.i;
    ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
    fafNextApiConnectptr.i = apiConnectptr.p->nextApiConnect;
    ptrCheck(fafNextApiConnectptr, capiConnectFilesize, apiConnectRecord);
    if ((apiConnectptr.p->transid[1] != ttransid2) ||
        (apiConnectptr.p->transid[0] != ttransid1)) {
      goto FAF_LOOP;
    }//if
    updateApiStateFail(signal);
  }//if
}//Dbtc::findApiConnectFail()

/*----------------------------------------------------------*/
/*       FIND THE TC CONNECT AND IF NOT FOUND ALLOCATE A NEW  */
/*----------------------------------------------------------*/
void Dbtc::findTcConnectFail(Signal* signal) 
{
  UintR tftfHashNumber;

  tftfHashNumber = (ttransid1 ^ ttcOprec) & 1023;
  tcConnectptr.i = ctcConnectFailHash[tftfHashNumber];
  do {
    if (tcConnectptr.i == RNIL) {
      jam();
      if (cfirstfreeTcConnectFail == RNIL) {
        jam();
        systemErrorLab(signal, __LINE__);
        return;
      }//if
      seizeTcConnectFail(signal);
      linkTcInConnectionlist(signal);
      tcConnectptr.p->nextTcFailHash = ctcConnectFailHash[tftfHashNumber];
      ctcConnectFailHash[tftfHashNumber] = tcConnectptr.i;
      initTcConnectFail(signal);
      return;
    } else {
      ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
      if (tcConnectptr.p->tcOprec != ttcOprec) {
        jam();  /* FRAGMENTID = TC_OPREC HERE, LOOP ANOTHER TURN */
        tcConnectptr.i = tcConnectptr.p->nextTcFailHash;
      } else {
        updateTcStateFail(signal);
        return;
      }//if
    }//if
  } while (1);
}//Dbtc::findTcConnectFail()

/*----------------------------------------------------------*/
/*       INITIALISE AN API CONNECT FAIL RECORD              */
/*----------------------------------------------------------*/
void Dbtc::initApiConnectFail(Signal* signal) 
{
  apiConnectptr.p->transid[0] = ttransid1;
  apiConnectptr.p->transid[1] = ttransid2;
  apiConnectptr.p->firstTcConnect = RNIL;
  apiConnectptr.p->currSavePointId = 0;
  apiConnectptr.p->lastTcConnect = RNIL;
  tblockref = calcTcBlockRef(tcNodeFailptr.p->takeOverNode);

  apiConnectptr.p->tcBlockref = tblockref;
  apiConnectptr.p->ndbapiBlockref = 0;
  apiConnectptr.p->ndbapiConnect = 0;
  apiConnectptr.p->buddyPtr = RNIL;
  apiConnectptr.p->m_transaction_nodes.clear();
  apiConnectptr.p->singleUserMode = 0;
  setApiConTimer(apiConnectptr.i, 0, __LINE__);
  switch(ttransStatus){
  case LqhTransConf::Committed:
    jam();
    apiConnectptr.p->globalcheckpointid = tgci;
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
  if(LqhTransConf::getMarkerFlag(treqinfo)){
    jam();
    CommitAckMarkerPtr tmp;
    m_commitAckMarkerHash.seize(tmp);
    
    ndbrequire(tmp.i != RNIL);
    
    apiConnectptr.p->commitAckMarker = tmp.i;
    tmp.p->transid1      = ttransid1;
    tmp.p->transid2      = ttransid2;
    tmp.p->apiNodeId     = refToNode(tapplRef);
    tmp.p->noOfLqhs      = 1;
    tmp.p->lqhNodeId[0]  = tnodeid;
    tmp.p->apiConnectPtr = apiConnectptr.i;

#if defined VM_TRACE || defined ERROR_INSERT
    {
      CommitAckMarkerPtr check;
      ndbrequire(!m_commitAckMarkerHash.find(check, *tmp.p));
    }
#endif
    m_commitAckMarkerHash.add(tmp);
  } 
}//Dbtc::initApiConnectFail()

/*------------------------------------------------------------*/
/*       INITIALISE AT TC CONNECT AT TAKE OVER WHEN ALLOCATING*/
/*       THE TC CONNECT RECORD.                               */
/*------------------------------------------------------------*/
void Dbtc::initTcConnectFail(Signal* signal) 
{
  tcConnectptr.p->apiConnect = apiConnectptr.i;
  tcConnectptr.p->tcOprec = ttcOprec;
  Uint32 treplicaNo = LqhTransConf::getReplicaNo(treqinfo);
  for (Uint32 i = 0; i < MAX_REPLICAS; i++) {
    tcConnectptr.p->failData[i] = LqhTransConf::InvalidStatus;
  }//for
  tcConnectptr.p->tcNodedata[treplicaNo] = tnodeid;
  tcConnectptr.p->failData[treplicaNo] = ttransStatus;
  tcConnectptr.p->lastReplicaNo = LqhTransConf::getLastReplicaNo(treqinfo);
  tcConnectptr.p->dirtyOp = LqhTransConf::getDirtyFlag(treqinfo);
  
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
void Dbtc::updateApiStateFail(Signal* signal) 
{
  if(LqhTransConf::getMarkerFlag(treqinfo)){
    jam();
    const Uint32 marker = apiConnectptr.p->commitAckMarker;
    if(marker == RNIL){
      jam();

      CommitAckMarkerPtr tmp;
      m_commitAckMarkerHash.seize(tmp);
      ndbrequire(tmp.i != RNIL);
      
      apiConnectptr.p->commitAckMarker = tmp.i;
      tmp.p->transid1      = ttransid1;
      tmp.p->transid2      = ttransid2;
      tmp.p->apiNodeId     = refToNode(tapplRef);
      tmp.p->noOfLqhs      = 1;
      tmp.p->lqhNodeId[0]  = tnodeid;
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
      
      CommitAckMarkerPtr tmp;
      tmp.i = marker;
      tmp.p = m_commitAckMarkerHash.getPtr(marker);

      const Uint32 noOfLqhs = tmp.p->noOfLqhs;
      ndbrequire(noOfLqhs < MAX_REPLICAS);
      tmp.p->lqhNodeId[noOfLqhs] = tnodeid;
      tmp.p->noOfLqhs = (noOfLqhs + 1);
    }
  }

  switch (ttransStatus) {
  case LqhTransConf::Committed:
    jam();
    switch (apiConnectptr.p->apiConnectstate) {
    case CS_FAIL_COMMITTING:
    case CS_FAIL_COMMITTED:
      jam();
      ndbrequire(tgci == apiConnectptr.p->globalcheckpointid);
      break;
    case CS_FAIL_PREPARED:
      jam();
      apiConnectptr.p->apiConnectstate = CS_FAIL_COMMITTING;
      apiConnectptr.p->globalcheckpointid = tgci;
      break;
    case CS_FAIL_COMPLETED:
      jam();
      apiConnectptr.p->globalcheckpointid = tgci;
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
void Dbtc::updateTcStateFail(Signal* signal) 
{
  const Uint8 treplicaNo     = LqhTransConf::getReplicaNo(treqinfo);
  const Uint8 tlastReplicaNo = LqhTransConf::getLastReplicaNo(treqinfo);
  const Uint8 tdirtyOp       = LqhTransConf::getDirtyFlag(treqinfo);

  TcConnectRecord * regTcPtr = tcConnectptr.p;
  
  ndbrequire(regTcPtr->apiConnect == apiConnectptr.i);
  ndbrequire(regTcPtr->failData[treplicaNo] == LqhTransConf::InvalidStatus);
  ndbrequire(regTcPtr->lastReplicaNo == tlastReplicaNo);
  ndbrequire(regTcPtr->dirtyOp == tdirtyOp);
  
  regTcPtr->tcNodedata[treplicaNo] = tnodeid;
  regTcPtr->failData[treplicaNo] = ttransStatus;  
}//Dbtc::updateTcStateFail()

void Dbtc::execTCGETOPSIZEREQ(Signal* signal) 
{
  jamEntry();
  CRASH_INSERTION(8000);

  UintR Tuserpointer = signal->theData[0];         /* DBDIH POINTER         */
  BlockReference Tusersblkref = signal->theData[1];/* DBDIH BLOCK REFERENCE */
  signal->theData[0] = Tuserpointer;
  signal->theData[1] = coperationsize;
  sendSignal(Tusersblkref, GSN_TCGETOPSIZECONF, signal, 2, JBB);
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

void Dbtc::seizeAttrbuferrorLab(Signal* signal) 
{
  terrorCode = ZGET_ATTRBUF_ERROR;
  abortErrorLab(signal);
}//Dbtc::seizeAttrbuferrorLab()

void Dbtc::seizeDatabuferrorLab(Signal* signal) 
{
  terrorCode = ZGET_DATAREC_ERROR;
  releaseAtErrorLab(signal);
}//Dbtc::seizeDatabuferrorLab()

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
  const ScanTabReq * const scanTabReq = (ScanTabReq *)&signal->theData[0];
  const Uint32 ri = scanTabReq->requestInfo;
  const Uint32 aiLength = (scanTabReq->attrLenKeyLen & 0xFFFF);
  const Uint32 keyLen = scanTabReq->attrLenKeyLen >> 16;
  const Uint32 schemaVersion = scanTabReq->tableSchemaVersion;
  const Uint32 transid1 = scanTabReq->transId1;
  const Uint32 transid2 = scanTabReq->transId2;
  const Uint32 tmpXX = scanTabReq->buddyConPtr;
  const Uint32 buddyPtr = (tmpXX == 0xFFFFFFFF ? RNIL : tmpXX);
  Uint32 currSavePointId = 0;
  
  Uint32 scanConcurrency = scanTabReq->getParallelism(ri);
  Uint32 noOprecPerFrag = ScanTabReq::getScanBatch(ri);
  Uint32 scanParallel = scanConcurrency;
  Uint32 errCode;
  ScanRecordPtr scanptr;

  jamEntry();

  SegmentedSectionPtr api_op_ptr;
  signal->getSection(api_op_ptr, 0);
  copy(&cdata[0], api_op_ptr);
  releaseSections(signal);

  apiConnectptr.i = scanTabReq->apiConnectPtr;
  tabptr.i = scanTabReq->tableId;

  if (apiConnectptr.i >= capiConnectFilesize)
  {
    jam();
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

  if(tabptr.i >= ctabrecFilesize)
  {
    errCode = ZUNKNOWN_TABLE_ERROR;
    goto SCAN_TAB_error;
  }

  ptrAss(tabptr, tableRecord);
  if ((aiLength == 0) ||
      (!tabptr.p->checkTable(schemaVersion)) ||
      (scanConcurrency == 0) ||
      (cfirstfreeTcConnect == RNIL) ||
      (cfirstfreeScanrec == RNIL)) {
    goto SCAN_error_check;
  }
  if (buddyPtr != RNIL) {
    jam();
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
  }
  
  if (getNodeState().startLevel == NodeState::SL_SINGLEUSER &&
      getNodeState().getSingleUserApi() !=
      refToNode(apiConnectptr.p->ndbapiBlockref))
  {
    errCode = ZCLUSTER_IN_SINGLEUSER_MODE;
    goto SCAN_TAB_error;
  }

  seizeTcConnect(signal);
  tcConnectptr.p->apiConnect = apiConnectptr.i;
  tcConnectptr.p->tcConnectstate = OS_WAIT_SCAN;
  apiConnectptr.p->lastTcConnect = tcConnectptr.i;

  seizeCacheRecord(signal);
  cachePtr.p->keylen = keyLen;
  cachePtr.p->save1 = 0;
  cachePtr.p->distributionKey = scanTabReq->distributionKey;
  cachePtr.p->distributionKeyIndicator= ScanTabReq::getDistributionKeyFlag(ri);
  scanptr = seizeScanrec(signal);

  ndbrequire(transP->apiScanRec == RNIL);
  ndbrequire(scanptr.p->scanApiRec == RNIL);

  initScanrec(scanptr, scanTabReq, scanParallel, noOprecPerFrag);

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
    DisconnectRep * const  rep = (DisconnectRep *)signal->getDataPtrSend();
    rep->nodeId = refToNode(apiConnectptr.p->ndbapiBlockref);
    rep->err = 8038;
    
    EXECUTE_DIRECT(CMVMI, GSN_DISCONNECT_REP, signal, 2);
    CLEAR_ERROR_INSERT_VALUE;
  }
  
  return;

 SCAN_error_check:
  if (aiLength == 0) {
    jam()
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
  if (cfirstfreeTcConnect == RNIL) {
    jam();
    errCode = ZNO_FREE_TC_CONNECTION;
    goto SCAN_TAB_error;
  }//if
  ndbrequire(cfirstfreeScanrec == RNIL);
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
  
  ScanTabRef * ref = (ScanTabRef*)&signal->theData[0];
  ref->apiConnectPtr = transP->ndbapiConnect;
  ref->transId1 = transid1;
  ref->transId2 = transid2;
  ref->errorCode  = errCode;
  ref->closeNeeded = 0;
  sendSignal(transP->ndbapiBlockref, GSN_SCAN_TABREF, 
	     signal, ScanTabRef::SignalLength, JBB);
  return;
}//Dbtc::execSCAN_TABREQ()

void Dbtc::initScanrec(ScanRecordPtr scanptr,
		       const ScanTabReq * scanTabReq,
		       UintR scanParallel,
		       UintR noOprecPerFrag) 
{
  const UintR ri = scanTabReq->requestInfo;
  scanptr.p->scanTcrec = tcConnectptr.i;
  scanptr.p->scanApiRec = apiConnectptr.i;
  scanptr.p->scanAiLength = scanTabReq->attrLenKeyLen & 0xFFFF;
  scanptr.p->scanKeyLen = scanTabReq->attrLenKeyLen >> 16;
  scanptr.p->scanTableref = tabptr.i;
  scanptr.p->scanSchemaVersion = scanTabReq->tableSchemaVersion;
  scanptr.p->scanParallel = scanParallel;
  scanptr.p->first_batch_size_rows = scanTabReq->first_batch_size;
  scanptr.p->batch_byte_size = scanTabReq->batch_byte_size;
  scanptr.p->batch_size_rows = noOprecPerFrag;

  Uint32 tmp = 0;
  ScanFragReq::setLockMode(tmp, ScanTabReq::getLockMode(ri));
  ScanFragReq::setHoldLockFlag(tmp, ScanTabReq::getHoldLockFlag(ri));
  ScanFragReq::setKeyinfoFlag(tmp, ScanTabReq::getKeyinfoFlag(ri));
  ScanFragReq::setReadCommittedFlag(tmp,ScanTabReq::getReadCommittedFlag(ri));
  ScanFragReq::setRangeScanFlag(tmp, ScanTabReq::getRangeScanFlag(ri));
  ScanFragReq::setDescendingFlag(tmp, ScanTabReq::getDescendingFlag(ri));
  ScanFragReq::setTupScanFlag(tmp, ScanTabReq::getTupScanFlag(ri));
  ScanFragReq::setAttrLen(tmp, scanTabReq->attrLenKeyLen & 0xFFFF);
  ScanFragReq::setNoDiskFlag(tmp, ScanTabReq::getNoDiskFlag(ri));
  
  scanptr.p->scanRequestInfo = tmp;
  scanptr.p->scanStoredProcId = scanTabReq->storedProcId;
  scanptr.p->scanState = ScanRecord::RUNNING;
  scanptr.p->m_queued_count = 0;

  ScanFragList list(c_scan_frag_pool, 
		    scanptr.p->m_running_scan_frags);
  for (Uint32 i = 0; i < scanParallel; i++) {
    jam();
    ScanFragRecPtr ptr;
    ndbrequire(list.seize(ptr));
    ptr.p->scanFragState = ScanFragRec::IDLE;
    ptr.p->scanRec = scanptr.i;
    ptr.p->scanFragId = 0;
    ptr.p->m_apiPtr = cdata[i];
  }//for

  (* (ScanTabReq::getRangeScanFlag(ri) ? 
      &c_counters.c_range_scan_count : 
      &c_counters.c_scan_count))++;
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

/*---------------------------------------------------------------------------*/
/*                                                                           */
/*       RECEPTION OF ATTRINFO FOR SCAN TABLE REQUEST.                       */
/*---------------------------------------------------------------------------*/
void Dbtc::scanAttrinfoLab(Signal* signal, UintR Tlen) 
{
  ScanRecordPtr scanptr;
  scanptr.i = apiConnectptr.p->apiScanRec;
  ptrCheckGuard(scanptr, cscanrecFileSize, scanRecord);
  tcConnectptr.i = scanptr.p->scanTcrec;
  ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
  cachePtr.i = apiConnectptr.p->cachePtr;
  ptrCheckGuard(cachePtr, ccacheFilesize, cacheRecord);
  CacheRecord * const regCachePtr = cachePtr.p;
  ndbrequire(scanptr.p->scanState == ScanRecord::WAIT_AI);

  regCachePtr->currReclenAi = regCachePtr->currReclenAi + Tlen;
  if (regCachePtr->currReclenAi < scanptr.p->scanAiLength) {
    if (cfirstfreeAttrbuf == RNIL) {
      goto scanAttrinfo_attrbuf_error;
    }//if
    saveAttrbuf(signal);
  } else {
    if (regCachePtr->currReclenAi > scanptr.p->scanAiLength) {
      goto scanAttrinfo_len_error;
    } else {
      /* CURR_RECLEN_AI = SCAN_AI_LENGTH */
      if (cfirstfreeAttrbuf == RNIL) {
        goto scanAttrinfo_attrbuf2_error;
      }//if
      saveAttrbuf(signal);
      /**************************************************
       * WE HAVE NOW RECEIVED ALL INFORMATION CONCERNING 
       * THIS SCAN. WE ARE READY TO START THE ACTUAL 
       * EXECUTION OF THE SCAN QUERY
       **************************************************/
      diFcountReqLab(signal, scanptr);
      return;
    }//if
  }//if
  return;

scanAttrinfo_attrbuf_error:
  jam();
  abortScanLab(signal, scanptr, ZGET_ATTRBUF_ERROR, true);
  return;

scanAttrinfo_attrbuf2_error:
  jam();
  abortScanLab(signal, scanptr, ZGET_ATTRBUF_ERROR, true);
  return;

scanAttrinfo_len_error:
  jam();
  abortScanLab(signal, scanptr, ZLENGTH_ERROR, true);
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
  scanptr.p->m_booked_fragments_count= 0;
  scanptr.p->scanState = ScanRecord::WAIT_FRAGMENT_COUNT;
  
  if(!cachePtr.p->distributionKeyIndicator)
  {
    jam();
    /*************************************************
     * THE FIRST STEP TO RECEIVE IS SUCCESSFULLY COMPLETED. 
     * WE MUST FIRST GET THE NUMBER OF  FRAGMENTS IN THE TABLE.
     ***************************************************/
    DihFragCountReq * const req = (DihFragCountReq*)signal->getDataPtrSend();
    req->m_connectionData = tcConnectptr.p->dihConnectptr;
    req->m_tableRef = scanptr.p->scanTableref;
    sendSignal(cdihblockref, GSN_DI_FCOUNTREQ, signal, 
               DihFragCountReq::SignalLength, JBB);
  }
  else 
  {
    signal->theData[0] = tcConnectptr.p->dihConnectptr;
    signal->theData[1] = tabPtr.i;
    signal->theData[2] = cachePtr.p->distributionKey;
    EXECUTE_DIRECT(DBDIH, GSN_DIGETNODESREQ, signal, 3);
    UintR TerrorIndicator = signal->theData[0];
    jamEntry();
    if (TerrorIndicator != 0) {
      DihFragCountRef * const ref = (DihFragCountRef*)signal->getDataPtr();
      ref->m_connectionData = tcConnectptr.i;
      ref->m_error = signal->theData[1];
      execDI_FCOUNTREF(signal);
      return;
    }
    
    UintR Tdata1 = signal->theData[1];
    scanptr.p->scanNextFragId = Tdata1;
    DihFragCountConf * const conf = (DihFragCountConf*)signal->getDataPtr();
    conf->m_connectionData = tcConnectptr.i;
    conf->m_fragmentCount = 1; // Frag count
    execDI_FCOUNTCONF(signal);
  }
  return;
}//Dbtc::diFcountReqLab()

/********************************************************************
 * execDI_FCOUNTCONF
 *
 * WE HAVE ASKED DIH ABOUT THE NUMBER OF FRAGMENTS IN THIS TABLE. 
 * WE WILL NOW START A NUMBER OF PARALLEL SCAN PROCESSES. EACH OF 
 * THESE WILL SCAN ONE FRAGMENT AT A TIME. THEY WILL CONTINUE THIS 
 * UNTIL THERE ARE NO MORE FRAGMENTS TO SCAN OR UNTIL THE APPLICATION 
 * CLOSES THE SCAN.
 ********************************************************************/
void Dbtc::execDI_FCOUNTCONF(Signal* signal) 
{
  jamEntry();
  DihFragCountConf * const conf = (DihFragCountConf*)signal->getDataPtr();
  tcConnectptr.i = conf->m_connectionData;
  Uint32 tfragCount = conf->m_fragmentCount;
  ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
  apiConnectptr.i = tcConnectptr.p->apiConnect;
  ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
  ScanRecordPtr scanptr;
  scanptr.i = apiConnectptr.p->apiScanRec;
  ptrCheckGuard(scanptr, cscanrecFileSize, scanRecord);
  ndbrequire(scanptr.p->scanState == ScanRecord::WAIT_FRAGMENT_COUNT);
  if (apiConnectptr.p->apiFailState == ZTRUE) {
    jam();
    releaseScanResources(scanptr, true);
    handleApiFailState(signal, apiConnectptr.i);
    return;
  }//if
  if (tfragCount == 0) {
    jam();
    abortScanLab(signal, scanptr, ZNO_FRAGMENT_ERROR, true);
    return;
  }//if
  
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

  scanptr.p->scanParallel = tfragCount;
  scanptr.p->scanNoFrag = tfragCount;
  scanptr.p->scanState = ScanRecord::RUNNING;

  setApiConTimer(apiConnectptr.i, 0, __LINE__);
  updateBuddyTimer(apiConnectptr);
  
  ScanFragRecPtr ptr;
  ScanFragList list(c_scan_frag_pool, scanptr.p->m_running_scan_frags);
  for (list.first(ptr); !ptr.isNull() && tfragCount; 
       list.next(ptr), tfragCount--){
    jam();

    ptr.p->lqhBlockref = 0;
    ptr.p->startFragTimer(ctcTimer);
    ptr.p->scanFragId = scanptr.p->scanNextFragId++;
    ptr.p->scanFragState = ScanFragRec::WAIT_GET_PRIMCONF;
    ptr.p->startFragTimer(ctcTimer);

    signal->theData[0] = tcConnectptr.p->dihConnectptr;
    signal->theData[1] = ptr.i;
    signal->theData[2] = scanptr.p->scanTableref;
    signal->theData[3] = ptr.p->scanFragId;
    sendSignal(cdihblockref, GSN_DIGETPRIMREQ, signal, 4, JBB);
  }//for

  ScanFragList queued(c_scan_frag_pool, scanptr.p->m_queued_scan_frags);
  for (; !ptr.isNull();)
  {
    ptr.p->m_ops = 0;
    ptr.p->m_totalLen = 0;
    ptr.p->m_scan_frag_conf_status = 1;
    ptr.p->scanFragState = ScanFragRec::QUEUED_FOR_DELIVERY;
    ptr.p->stopFragTimer();

    ScanFragRecPtr tmp = ptr;
    list.next(ptr);
    list.remove(tmp);
    queued.add(tmp);
    scanptr.p->m_queued_count++;
  }
}//Dbtc::execDI_FCOUNTCONF()

/******************************************************
 * execDI_FCOUNTREF
 ******************************************************/
void Dbtc::execDI_FCOUNTREF(Signal* signal) 
{
  jamEntry();
  DihFragCountRef * const ref = (DihFragCountRef*)signal->getDataPtr();
  tcConnectptr.i = ref->m_connectionData;
  ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
  const Uint32 errCode = ref->m_error;
  apiConnectptr.i = tcConnectptr.p->apiConnect;
  ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
  ScanRecordPtr scanptr;
  scanptr.i = apiConnectptr.p->apiScanRec;
  ptrCheckGuard(scanptr, cscanrecFileSize, scanRecord);
  ndbrequire(scanptr.p->scanState == ScanRecord::WAIT_FRAGMENT_COUNT);
  if (apiConnectptr.p->apiFailState == ZTRUE) {
    jam();
    releaseScanResources(scanptr, true);
    handleApiFailState(signal, apiConnectptr.i);
    return;
  }//if
  abortScanLab(signal, scanptr, errCode, true);
}//Dbtc::execDI_FCOUNTREF()

void Dbtc::abortScanLab(Signal* signal, ScanRecordPtr scanptr, Uint32 errCode,
			bool not_started) 
{
  scanTabRefLab(signal, errCode);
  releaseScanResources(scanptr, not_started);
}//Dbtc::abortScanLab()

void Dbtc::releaseScanResources(ScanRecordPtr scanPtr,
				bool not_started)
{
  if (apiConnectptr.p->cachePtr != RNIL) {
    cachePtr.i = apiConnectptr.p->cachePtr;
    ptrCheckGuard(cachePtr, ccacheFilesize, cacheRecord);
    releaseKeys();
    releaseAttrinfo();
  }//if
  tcConnectptr.i = scanPtr.p->scanTcrec;
  ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
  releaseTcCon();

  if (not_started)
  {
    jam();
    ScanFragList run(c_scan_frag_pool, scanPtr.p->m_running_scan_frags);
    ScanFragList queue(c_scan_frag_pool, scanPtr.p->m_queued_scan_frags);
    run.release();
    queue.release();
  }
  
  ndbrequire(scanPtr.p->m_running_scan_frags.isEmpty());
  ndbrequire(scanPtr.p->m_queued_scan_frags.isEmpty());
  ndbrequire(scanPtr.p->m_delivered_scan_frags.isEmpty());

  ndbassert(scanPtr.p->scanApiRec == apiConnectptr.i);
  ndbassert(apiConnectptr.p->apiScanRec == scanPtr.i);
  
  // link into free list
  scanPtr.p->nextScan = cfirstfreeScanrec;
  scanPtr.p->scanState = ScanRecord::IDLE;
  scanPtr.p->scanTcrec = RNIL;
  scanPtr.p->scanApiRec = RNIL;
  cfirstfreeScanrec = scanPtr.i;
  
  apiConnectptr.p->apiScanRec = RNIL;
  apiConnectptr.p->apiConnectstate = CS_CONNECTED;
  setApiConTimer(apiConnectptr.i, 0, __LINE__);
}//Dbtc::releaseScanResources()


/****************************************************************
 * execDIGETPRIMCONF
 *
 * WE HAVE RECEIVED THE PRIMARY NODE OF THIS FRAGMENT. 
 * WE ARE NOW READY TO ASK  FOR PERMISSION TO LOAD THIS 
 * SPECIFIC NODE WITH A SCAN OPERATION.
 ****************************************************************/
void Dbtc::execDIGETPRIMCONF(Signal* signal) 
{
  jamEntry();
  //  tcConnectptr.i in theData[0] is not used
  scanFragptr.i = signal->theData[1];
  c_scan_frag_pool.getPtr(scanFragptr);

  tnodeid = signal->theData[2];
  arrGuard(tnodeid, MAX_NDB_NODES);

  ndbrequire(scanFragptr.p->scanFragState == ScanFragRec::WAIT_GET_PRIMCONF);
  scanFragptr.p->stopFragTimer();

  ScanRecordPtr scanptr;
  scanptr.i = scanFragptr.p->scanRec;
  ptrCheckGuard(scanptr, cscanrecFileSize, scanRecord);

  /**
   * This must be false as select count(*) otherwise
   *   can "pass" committing on backup fragments and
   *   get incorrect row count
   */
  if(false && ScanFragReq::getReadCommittedFlag(scanptr.p->scanRequestInfo))
  {
    jam();
    Uint32 max = 3+signal->theData[6];
    Uint32 nodeid = getOwnNodeId();
    for(Uint32 i = 3; i<max; i++)
      if(signal->theData[i] ==  nodeid)
      {
	jam();
	tnodeid = nodeid;
	break;
      }
  }
  
  {
    /**
     * Check table
     */
    TableRecordPtr tabPtr;
    tabPtr.i = scanptr.p->scanTableref;
    ptrAss(tabPtr, tableRecord);
    Uint32 schemaVersion = scanptr.p->scanSchemaVersion;
    if(tabPtr.p->checkTable(schemaVersion) == false){
      jam();
      ScanFragList run(c_scan_frag_pool, scanptr.p->m_running_scan_frags);
      
      run.release(scanFragptr);
      scanError(signal, scanptr, tabPtr.p->getErrorCode(schemaVersion));
      return;
    }
  }
  
  tcConnectptr.i = scanptr.p->scanTcrec;
  ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
  apiConnectptr.i = scanptr.p->scanApiRec;
  ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
  cachePtr.i = apiConnectptr.p->cachePtr;
  ptrCheckGuard(cachePtr, ccacheFilesize, cacheRecord);
  switch (scanptr.p->scanState) {
  case ScanRecord::CLOSING_SCAN:
    jam();
    updateBuddyTimer(apiConnectptr);
    {
      ScanFragList run(c_scan_frag_pool, scanptr.p->m_running_scan_frags);
      
      run.release(scanFragptr);
    }
    close_scan_req_send_conf(signal, scanptr);
    return;
  default:
    jam();
    /*empty*/;
    break;
  }//switch
  Uint32 ref = calcLqhBlockRef(tnodeid);
  scanFragptr.p->lqhBlockref = ref;
  scanFragptr.p->m_connectCount = getNodeInfo(tnodeid).m_connectCount;
  sendScanFragReq(signal, scanptr.p, scanFragptr.p);
  if(ERROR_INSERTED(8035))
    globalTransporterRegistry.performSend();
  attrbufptr.i = cachePtr.p->firstAttrbuf;
  while (attrbufptr.i != RNIL) {
    jam();
    ptrCheckGuard(attrbufptr, cattrbufFilesize, attrbufRecord);
    sendAttrinfo(signal,
                 scanFragptr.i,
                 attrbufptr.p,
                 ref);
    attrbufptr.i = attrbufptr.p->attrbuf[ZINBUF_NEXT];
    if(ERROR_INSERTED(8035))
      globalTransporterRegistry.performSend();
  }//while
  scanFragptr.p->scanFragState = ScanFragRec::LQH_ACTIVE;
  scanFragptr.p->startFragTimer(ctcTimer);
  updateBuddyTimer(apiConnectptr);
  /*********************************************
   * WE HAVE NOW STARTED A FRAGMENT SCAN. NOW 
   * WAIT FOR THE FIRST SCANNED RECORDS
   *********************************************/
}//Dbtc::execDIGETPRIMCONF

/***************************************************
 * execDIGETPRIMREF
 *
 * WE ARE NOW FORCED TO STOP THE SCAN. THIS ERROR 
 * IS NOT RECOVERABLE SINCE THERE IS A PROBLEM WITH 
 * FINDING A PRIMARY REPLICA OF A CERTAIN FRAGMENT.
 ***************************************************/
void Dbtc::execDIGETPRIMREF(Signal* signal) 
{
  jamEntry();
  // tcConnectptr.i in theData[0] is not used.
  scanFragptr.i = signal->theData[1];
  const Uint32 errCode = signal->theData[2];
  c_scan_frag_pool.getPtr(scanFragptr);
  ndbrequire(scanFragptr.p->scanFragState == ScanFragRec::WAIT_GET_PRIMCONF);

  ScanRecordPtr scanptr;
  scanptr.i = scanFragptr.p->scanRec;
  ptrCheckGuard(scanptr, cscanrecFileSize, scanRecord);

  ScanFragList run(c_scan_frag_pool, scanptr.p->m_running_scan_frags);
  
  run.release(scanFragptr);

  scanError(signal, scanptr, errCode);
}//Dbtc::execDIGETPRIMREF()

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
  {
    scanFragptr.p->scanFragState = ScanFragRec::COMPLETED;
    ScanFragList run(c_scan_frag_pool, scanptr.p->m_running_scan_frags);
    
    run.release(scanFragptr);
    scanFragptr.p->stopFragTimer();
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
  
  const bool apiFail = (apiConnectptr.p->apiFailState == ZTRUE);
  if(apiFail){
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
  
  if(scanptr.p->scanState == ScanRecord::CLOSING_SCAN){
    jam();
    if(status == 0){
      /**
       * We have started closing = we sent a close -> ignore this
       */
      return;
    } else {
      jam();
      ScanFragList run(c_scan_frag_pool, scanptr.p->m_running_scan_frags);
      
      run.release(scanFragptr);
      scanFragptr.p->stopFragTimer();
      scanFragptr.p->scanFragState = ScanFragRec::COMPLETED;
    }
    close_scan_req_send_conf(signal, scanptr);
    return;
  }

  if(noCompletedOps == 0 && status != 0 && 
     scanptr.p->scanNextFragId+scanptr.p->m_booked_fragments_count < scanptr.p->scanNoFrag){
    /**
     * Start on next fragment
     */
    scanFragptr.p->scanFragState = ScanFragRec::WAIT_GET_PRIMCONF; 
    scanFragptr.p->startFragTimer(ctcTimer);

    tcConnectptr.i = scanptr.p->scanTcrec;
    ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
    scanFragptr.p->scanFragId = scanptr.p->scanNextFragId++;
    signal->theData[0] = tcConnectptr.p->dihConnectptr;
    signal->theData[1] = scanFragptr.i;
    signal->theData[2] = scanptr.p->scanTableref;
    signal->theData[3] = scanFragptr.p->scanFragId;
    sendSignal(cdihblockref, GSN_DIGETPRIMREQ, signal, 4, JBB);
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
    ScanFragList run(c_scan_frag_pool, scanptr.p->m_running_scan_frags);
    ScanFragList queued(c_scan_frag_pool, scanptr.p->m_queued_scan_frags);
    
    run.remove(scanFragptr);
    queued.add(scanFragptr);
    scanptr.p->m_queued_count++;
  }

  scanFragptr.p->m_scan_frag_conf_status = status;
  scanFragptr.p->m_ops = noCompletedOps;
  scanFragptr.p->m_totalLen = total_len;
  scanFragptr.p->scanFragState = ScanFragRec::QUEUED_FOR_DELIVERY;
  scanFragptr.p->stopFragTimer();
  
  if(scanptr.p->m_queued_count > /** Min */ 0){
    jam();
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

  apiConnectptr.i = req->apiConnectPtr;
  if (apiConnectptr.i >= capiConnectFilesize) {
    jam();
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
    if (apiConnectptr.p->apiConnectstate == CS_CONNECTED) {
      jam();
      /*********************************************************************
       * The application sends a SCAN_NEXTREQ after experiencing a time-out.
       *  We will send a SCAN_TABREF to indicate a time-out occurred.
       *********************************************************************/
      DEBUG("scanTabRefLab: ZSCANTIME_OUT_ERROR2");
      ndbout_c("apiConnectptr(%d) -> abort", apiConnectptr.i);
      ndbrequire(false); //B2 indication of strange things going on
      scanTabRefLab(signal, ZSCANTIME_OUT_ERROR2);
      return;
    }
    DEBUG("scanTabRefLab: ZSTATE_ERROR");
    DEBUG("  apiConnectstate="<<apiConnectptr.p->apiConnectstate);
    ndbrequire(false); //B2 indication of strange things going on
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

  const Uint32 len = signal->getLength() - 4;

  if (stopScan == ZTRUE) {
    jam();
    /*********************************************************************
     * APPLICATION IS CLOSING THE SCAN.
     **********************************************************************/
    close_scan_req(signal, scanptr, true);
    return;
  }//if

  if (scanptr.p->scanState == ScanRecord::CLOSING_SCAN){
    jam();
    /**
     * The scan is closing (typically due to error)
     *   but the API hasn't understood it yet
     *
     * Wait for API close request
     */
    return;
  }

  // Copy op ptrs so I dont overwrite them when sending...
  memcpy(signal->getDataPtrSend()+25, signal->getDataPtr()+4, 4 * len);

  ScanFragNextReq tmp;
  tmp.closeFlag = ZFALSE;
  tmp.transId1 = apiConnectptr.p->transid[0];
  tmp.transId2 = apiConnectptr.p->transid[1];
  tmp.batch_size_rows = scanP->batch_size_rows;
  tmp.batch_size_bytes = scanP->batch_byte_size;

  ScanFragList running(c_scan_frag_pool, scanP->m_running_scan_frags);
  ScanFragList delivered(c_scan_frag_pool, scanP->m_delivered_scan_frags);
  for(Uint32 i = 0 ; i<len; i++){
    jam();
    scanFragptr.i = signal->theData[i+25];
    c_scan_frag_pool.getPtr(scanFragptr);
    ndbrequire(scanFragptr.p->scanFragState == ScanFragRec::DELIVERED);
    
    scanFragptr.p->startFragTimer(ctcTimer);
    scanFragptr.p->m_ops = 0;

    if(scanFragptr.p->m_scan_frag_conf_status)
    {
      /**
       * last scan was complete
       */
      jam();
      ndbrequire(scanptr.p->scanNextFragId < scanptr.p->scanNoFrag);
      jam();
      ndbassert(scanptr.p->m_booked_fragments_count);
      scanptr.p->m_booked_fragments_count--;
      scanFragptr.p->scanFragState = ScanFragRec::WAIT_GET_PRIMCONF; 
      
      tcConnectptr.i = scanptr.p->scanTcrec;
      ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
      scanFragptr.p->scanFragId = scanptr.p->scanNextFragId++;
      signal->theData[0] = tcConnectptr.p->dihConnectptr;
      signal->theData[1] = scanFragptr.i;
      signal->theData[2] = scanptr.p->scanTableref;
      signal->theData[3] = scanFragptr.p->scanFragId;
      sendSignal(cdihblockref, GSN_DIGETPRIMREQ, signal, 4, JBB);
    }
    else
    {
      jam();
      scanFragptr.p->scanFragState = ScanFragRec::LQH_ACTIVE;
      ScanFragNextReq * req = (ScanFragNextReq*)signal->getDataPtrSend();
      * req = tmp;
      req->senderData = scanFragptr.i;
      sendSignal(scanFragptr.p->lqhBlockref, GSN_SCAN_NEXTREQ, signal, 
		 ScanFragNextReq::SignalLength, JBB);
    }
    delivered.remove(scanFragptr);
    running.add(scanFragptr);
  }//for
  
}//Dbtc::execSCAN_NEXTREQ()

void
Dbtc::close_scan_req(Signal* signal, ScanRecordPtr scanPtr, bool req_received){

  ScanRecord* scanP = scanPtr.p;
  ndbrequire(scanPtr.p->scanState != ScanRecord::IDLE);  
  ScanRecord::ScanState old = scanPtr.p->scanState;
  scanPtr.p->scanState = ScanRecord::CLOSING_SCAN;
  scanPtr.p->m_close_scan_req = req_received;

  if (old == ScanRecord::WAIT_FRAGMENT_COUNT)
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
  nextReq->closeFlag = ZTRUE;
  nextReq->transId1 = apiConnectptr.p->transid[0];
  nextReq->transId2 = apiConnectptr.p->transid[1];
  
  {
    ScanFragRecPtr ptr;
    ScanFragList running(c_scan_frag_pool, scanP->m_running_scan_frags);
    ScanFragList delivered(c_scan_frag_pool, scanP->m_delivered_scan_frags);
    ScanFragList queued(c_scan_frag_pool, scanP->m_queued_scan_frags);
    
    // Close running
    for(running.first(ptr); !ptr.isNull(); ){
      ScanFragRecPtr curr = ptr; // Remove while iterating...
      running.next(ptr);

      switch(curr.p->scanFragState){
      case ScanFragRec::IDLE:
	jam(); // real early abort
	ndbrequire(old == ScanRecord::WAIT_AI);
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
	ndbrequire(false);
      }
      
      curr.p->startFragTimer(ctcTimer);
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
      
      if(curr.p->m_ops > 0 && curr.p->m_scan_frag_conf_status == 0){
	jam();
	running.add(curr);
	curr.p->scanFragState = ScanFragRec::LQH_ACTIVE;
	curr.p->startFragTimer(ctcTimer);
	nextReq->senderData = curr.i;
	sendSignal(curr.p->lqhBlockref, GSN_SCAN_NEXTREQ, signal, 
		   ScanFragNextReq::SignalLength, JBB);
	
      } else {
	jam();
	c_scan_frag_pool.release(curr);
	curr.p->scanFragState = ScanFragRec::COMPLETED;
	curr.p->stopFragTimer();
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
      
      if(curr.p->m_ops > 0){
	jam();
	running.add(curr);
	curr.p->scanFragState = ScanFragRec::LQH_ACTIVE;
	curr.p->startFragTimer(ctcTimer);
	nextReq->senderData = curr.i;
	sendSignal(curr.p->lqhBlockref, GSN_SCAN_NEXTREQ, signal, 
		   ScanFragNextReq::SignalLength, JBB);
      } else {
	jam();
	c_scan_frag_pool.release(curr);
	curr.p->scanFragState = ScanFragRec::COMPLETED;
	curr.p->stopFragTimer();
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

#if 0
  {
    ScanFragList comp(c_scan_frag_pool, scanPtr.p->m_completed_scan_frags);
    ScanFragRecPtr ptr;
    for(comp.first(ptr); !ptr.isNull(); comp.next(ptr)){
      ndbrequire(ptr.p->scanFragTimer == 0);
      ndbrequire(ptr.p->scanFragState == ScanFragRec::COMPLETED);
    } 
  }
#endif
  
  if(!scanPtr.p->m_running_scan_frags.isEmpty()){
    jam();
    return;
  }

  const bool apiFail = (apiConnectptr.p->apiFailState == ZTRUE);
  
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
  }
  
  releaseScanResources(scanPtr);
  
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
  ndbrequire(scanptr.p->scanState == ScanRecord::IDLE);
  return scanptr;
}//Dbtc::seizeScanrec()

void Dbtc::sendScanFragReq(Signal* signal, 
			   ScanRecord* scanP, 
			   ScanFragRec* scanFragP)
{
  ScanFragReq * const req = (ScanFragReq *)&signal->theData[0];
  Uint32 requestInfo = scanP->scanRequestInfo;
  ScanFragReq::setScanPrio(requestInfo, 1);
  apiConnectptr.i = scanP->scanApiRec;
  req->tableId = scanP->scanTableref;
  req->schemaVersion = scanP->scanSchemaVersion;
  ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
  req->senderData = scanFragptr.i;
  req->requestInfo = requestInfo;
  req->fragmentNoKeyLen = scanFragP->scanFragId | (scanP->scanKeyLen << 16);
  req->resultRef = apiConnectptr.p->ndbapiBlockref;
  req->savePointId = apiConnectptr.p->currSavePointId;
  req->transId1 = apiConnectptr.p->transid[0];
  req->transId2 = apiConnectptr.p->transid[1];
  req->clientOpPtr = scanFragP->m_apiPtr;
  req->batch_size_rows= scanP->batch_size_rows;
  req->batch_size_bytes= scanP->batch_byte_size;
  sendSignal(scanFragP->lqhBlockref, GSN_SCAN_FRAGREQ, signal,
             ScanFragReq::SignalLength, JBB);
  if(scanP->scanKeyLen > 0)
  {
    tcConnectptr.i = scanFragptr.i;
    packKeyData000Lab(signal, scanFragP->lqhBlockref, scanP->scanKeyLen);
  }
  updateBuddyTimer(apiConnectptr);
  scanFragP->startFragTimer(ctcTimer);
}//Dbtc::sendScanFragReq()


void Dbtc::sendScanTabConf(Signal* signal, ScanRecordPtr scanPtr) {
  jam();
  Uint32* ops = signal->getDataPtrSend()+4;
  Uint32 op_count = scanPtr.p->m_queued_count;
  if(4 + 3 * op_count > 25){
    jam();
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
    ScanFragList queued(c_scan_frag_pool, scanPtr.p->m_queued_scan_frags);
    ScanFragList delivered(c_scan_frag_pool,scanPtr.p->m_delivered_scan_frags);
    for(queued.first(ptr); !ptr.isNull(); ){
      ndbrequire(ptr.p->scanFragState == ScanFragRec::QUEUED_FOR_DELIVERY);
      ScanFragRecPtr curr = ptr; // Remove while iterating...
      queued.next(ptr);
      
      bool done = curr.p->m_scan_frag_conf_status && (left <= (int)booked);
      if(curr.p->m_scan_frag_conf_status)
	booked++;
      
      * ops++ = curr.p->m_apiPtr;
      * ops++ = done ? RNIL : curr.i;
      * ops++ = (curr.p->m_totalLen << 10) + curr.p->m_ops;
      
      queued.remove(curr); 
      if(!done){
	delivered.add(curr);
	curr.p->scanFragState = ScanFragRec::DELIVERED;
	curr.p->stopFragTimer();
      } else {
	c_scan_frag_pool.release(curr);
	curr.p->scanFragState = ScanFragRec::COMPLETED;
	curr.p->stopFragTimer();
      }
    }
  }
  
  scanPtr.p->m_booked_fragments_count = booked;
  if(scanPtr.p->m_delivered_scan_frags.isEmpty() && 
     scanPtr.p->m_running_scan_frags.isEmpty())
  {
    conf->requestInfo = op_count | ScanTabConf::EndOfData;    
    releaseScanResources(scanPtr);
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
  }
  
  if(4 + 3 * op_count > 25){
    jam();
    LinearSectionPtr ptr[3];
    ptr[0].p = signal->getDataPtrSend()+25;
    ptr[0].sz = 3 * op_count;
    sendSignal(apiConnectptr.p->ndbapiBlockref, GSN_SCAN_TABCONF, signal, 
	       ScanTabConf::SignalLength, JBB, ptr, 1);
  } else {
    jam();
    sendSignal(apiConnectptr.p->ndbapiBlockref, GSN_SCAN_TABCONF, signal, 
	       ScanTabConf::SignalLength + 3 * op_count, JBB);
  }
  scanPtr.p->m_queued_count = 0;
}//Dbtc::sendScanTabConf()


void Dbtc::gcpTcfinished(Signal* signal) 
{
  GCPTCFinished* conf = (GCPTCFinished*)signal->getDataPtrSend();
  conf->senderData = c_gcp_ref;
  conf->gci_hi = Uint32(tcheckGcpId >> 32);
  conf->gci_lo = Uint32(tcheckGcpId);
  sendSignal(cdihblockref, GSN_GCP_TCFINISHED, signal, 
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
    cachePtr.p->firstAttrbuf = RNIL;
    cachePtr.p->lastAttrbuf = RNIL;
    cachePtr.p->firstKeybuf = RNIL;
    cachePtr.p->lastKeybuf = RNIL;
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
    apiConnectptr.p->apiFailState = ZFALSE;
    setApiConTimer(apiConnectptr.i, 0, __LINE__);
    apiConnectptr.p->takeOverRec = (Uint8)Z8NIL;
    apiConnectptr.p->cachePtr = RNIL;
    apiConnectptr.p->nextApiConnect = apiConnectptr.i + 1;
    apiConnectptr.p->ndbapiBlockref = 0xFFFFFFFF; // Invalid ref
    apiConnectptr.p->commitAckMarker = RNIL;
    apiConnectptr.p->firstTcConnect = RNIL;
    apiConnectptr.p->lastTcConnect = RNIL;
    apiConnectptr.p->triggerPending = false;
    apiConnectptr.p->isIndexOp = false;
    apiConnectptr.p->accumulatingIndexOp = RNIL;
    apiConnectptr.p->executingIndexOp = RNIL;
    apiConnectptr.p->buddyPtr = RNIL;
    apiConnectptr.p->currSavePointId = 0;
    apiConnectptr.p->m_transaction_nodes.clear();
    apiConnectptr.p->singleUserMode = 0;
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
      apiConnectptr.p->apiFailState = ZFALSE;
      setApiConTimer(apiConnectptr.i, 0, __LINE__);
      apiConnectptr.p->takeOverRec = (Uint8)Z8NIL;
      apiConnectptr.p->cachePtr = RNIL;
      apiConnectptr.p->nextApiConnect = apiConnectptr.i + 1;
      apiConnectptr.p->ndbapiBlockref = 0xFFFFFFFF; // Invalid ref
      apiConnectptr.p->commitAckMarker = RNIL;
      apiConnectptr.p->firstTcConnect = RNIL;
      apiConnectptr.p->lastTcConnect = RNIL;
      apiConnectptr.p->triggerPending = false;
      apiConnectptr.p->isIndexOp = false;
      apiConnectptr.p->accumulatingIndexOp = RNIL;
      apiConnectptr.p->executingIndexOp = RNIL;
      apiConnectptr.p->buddyPtr = RNIL;
      apiConnectptr.p->currSavePointId = 0;
      apiConnectptr.p->m_transaction_nodes.clear();
      apiConnectptr.p->singleUserMode = 0;
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
    apiConnectptr.p->apiFailState = ZFALSE;
    apiConnectptr.p->apiConnectstate = CS_RESTART;
    apiConnectptr.p->takeOverRec = (Uint8)Z8NIL;
    apiConnectptr.p->cachePtr = RNIL;
    apiConnectptr.p->nextApiConnect = apiConnectptr.i + 1;
    apiConnectptr.p->ndbapiBlockref = 0xFFFFFFFF; // Invalid ref
    apiConnectptr.p->commitAckMarker = RNIL;
    apiConnectptr.p->firstTcConnect = RNIL;
    apiConnectptr.p->lastTcConnect = RNIL;
    apiConnectptr.p->triggerPending = false;
    apiConnectptr.p->isIndexOp = false;
    apiConnectptr.p->accumulatingIndexOp = RNIL;
    apiConnectptr.p->executingIndexOp = RNIL;
    apiConnectptr.p->buddyPtr = RNIL;
    apiConnectptr.p->currSavePointId = 0;
    apiConnectptr.p->m_transaction_nodes.clear();
    apiConnectptr.p->singleUserMode = 0;
  }//for
  apiConnectptr.i = (3 * tiacTmp) - 1;
  ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
  apiConnectptr.p->nextApiConnect = RNIL;
  cfirstfreeApiConnectFail = 2 * tiacTmp;
}//Dbtc::initApiConnect()

void Dbtc::initattrbuf(Signal* signal) 
{
  ndbrequire(cattrbufFilesize > 0);
  for (attrbufptr.i = 0; attrbufptr.i < cattrbufFilesize; attrbufptr.i++) {
    refresh_watch_dog();
    jam();
    ptrAss(attrbufptr, attrbufRecord);
    attrbufptr.p->attrbuf[ZINBUF_NEXT] = attrbufptr.i + 1;  /* NEXT ATTRBUF */
  }//for
  attrbufptr.i = cattrbufFilesize - 1;
  ptrAss(attrbufptr, attrbufRecord);
  attrbufptr.p->attrbuf[ZINBUF_NEXT] = RNIL;    /* NEXT ATTRBUF */
  cfirstfreeAttrbuf = 0;
}//Dbtc::initattrbuf()

void Dbtc::initdatabuf(Signal* signal) 
{
  ndbrequire(cdatabufFilesize > 0);
  for (databufptr.i = 0; databufptr.i < cdatabufFilesize; databufptr.i++) {
    refresh_watch_dog();
    ptrAss(databufptr, databufRecord);
    databufptr.p->nextDatabuf = databufptr.i + 1;
  }//for
  databufptr.i = cdatabufFilesize - 1;
  ptrCheckGuard(databufptr, cdatabufFilesize, databufRecord);
  databufptr.p->nextDatabuf = RNIL;
  cfirstfreeDatabuf = 0;
}//Dbtc::initdatabuf()

void Dbtc::initgcp(Signal* signal) 
{
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
    hostptr.p->noOfWordsTCKEYCONF = 0;
    hostptr.p->noOfWordsTCINDXCONF = 0;
    hostptr.p->noOfPackedWordsLqh = 0;
    hostptr.p->hostLqhBlockRef = calcLqhBlockRef(hostptr.i);
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
    initattrbuf(signal);
    break;
  case 2:
    jam();
    initdatabuf(signal);
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
  sendSignal(DBTC_REF, GSN_CONTINUEB, signal, 5, JBB);
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
    tcConnectptr.p->tcConnectstate = OS_RESTART;
    tcConnectptr.p->apiConnect = RNIL;
    tcConnectptr.p->noOfNodes = 0;
    tcConnectptr.p->nextTcConnect = tcConnectptr.i + 1;
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
    tcConnectptr.p->tcConnectstate = OS_RESTART;
    tcConnectptr.p->apiConnect = RNIL;
    tcConnectptr.p->noOfNodes = 0;
    tcConnectptr.p->nextTcConnect = tcConnectptr.i + 1;
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
void Dbtc::linkGciInGcilist(Signal* signal) 
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
/* -------        LINK SECONDARY KEY BUFFER IN OPERATION RECORD      ------- */
/* ------------------------------------------------------------------------- */
void Dbtc::linkKeybuf(Signal* signal) 
{
  seizeDatabuf(signal);
  tmpDatabufptr.i = cachePtr.p->lastKeybuf;
  cachePtr.p->lastKeybuf = databufptr.i;
  if (tmpDatabufptr.i == RNIL) {
    jam();
    cachePtr.p->firstKeybuf = databufptr.i;
  } else {
    jam();
    ptrCheckGuard(tmpDatabufptr, cdatabufFilesize, databufRecord);
    tmpDatabufptr.p->nextDatabuf = databufptr.i;
  }//if
}//Dbtc::linkKeybuf()

/* ------------------------------------------------------------------------- */
/* ------- LINK A TC CONNECT RECORD INTO THE API LIST OF TC CONNECTIONS  --- */
/* ------------------------------------------------------------------------- */
void Dbtc::linkTcInConnectionlist(Signal* signal) 
{
  /* POINTER FOR THE CONNECT_RECORD */
  TcConnectRecordPtr ltcTcConnectptr;

  tcConnectptr.p->nextTcConnect = RNIL;
  ltcTcConnectptr.i = apiConnectptr.p->lastTcConnect;
  ptrCheck(ltcTcConnectptr, ctcConnectFilesize, tcConnectRecord);
  apiConnectptr.p->lastTcConnect = tcConnectptr.i;
  if (ltcTcConnectptr.i == RNIL) {
    jam();
    apiConnectptr.p->firstTcConnect = tcConnectptr.i;
  } else {
    jam();
    ptrGuard(ltcTcConnectptr);
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
    // Clear any markers that were set in CS_RECEIVING state
    clearCommitAckMarker(apiConnectptr.p, tcConnectptr.p);
    rarTcConnectptr.i = tcConnectptr.p->nextTcConnect;
    releaseTcCon();
    tcConnectptr.i = rarTcConnectptr.i;
  }//while
  apiConnectptr.p->firstTcConnect = RNIL;
  apiConnectptr.p->lastTcConnect = RNIL;
  apiConnectptr.p->m_transaction_nodes.clear();
  apiConnectptr.p->singleUserMode = 0;

  // MASV let state be CS_ABORTING until all 
  // signals in the "air" have been received. Reset to CS_CONNECTED
  // will be done when a TCKEYREQ with start flag is recieved
  // or releaseApiCon is called
  // apiConnectptr.p->apiConnectstate = CS_CONNECTED;
  apiConnectptr.p->apiConnectstate = CS_ABORTING;
  apiConnectptr.p->abortState = AS_IDLE;
  releaseAllSeizedIndexOperations(apiConnectptr.p);
  if(apiConnectptr.p->m_exec_flag || apiConnectptr.p->apiFailState == ZTRUE){
    jam();
    bool ok = false;
    Uint32 blockRef = apiConnectptr.p->ndbapiBlockref;
    ReturnSignal ret = apiConnectptr.p->returnsignal;
    apiConnectptr.p->returnsignal = RS_NO_RETURN;
    apiConnectptr.p->m_exec_flag = 0;
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
  if (apiConnectptr.p->apiFailState == ZTRUE) {
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
  TlocalApiConnectptr.p->nextApiConnect = cfirstfreeApiConnect;
  cfirstfreeApiConnect = TlocalApiConnectptr.i;
  setApiConTimer(TlocalApiConnectptr.i, 0, __LINE__);
  TlocalApiConnectptr.p->apiConnectstate = CS_DISCONNECTED;
  ndbassert(TlocalApiConnectptr.p->m_transaction_nodes.isclear());
  ndbassert(TlocalApiConnectptr.p->apiScanRec == RNIL);
  TlocalApiConnectptr.p->ndbapiBlockref = 0;
}//Dbtc::releaseApiCon()

void Dbtc::releaseApiConnectFail(Signal* signal) 
{
  apiConnectptr.p->apiConnectstate = CS_RESTART;
  apiConnectptr.p->takeOverRec = (Uint8)Z8NIL;
  setApiConTimer(apiConnectptr.i, 0, __LINE__);
  apiConnectptr.p->nextApiConnect = cfirstfreeApiConnectFail;
  cfirstfreeApiConnectFail = apiConnectptr.i;
}//Dbtc::releaseApiConnectFail()

void Dbtc::releaseGcp(Signal* signal) 
{
  ptrGuard(gcpPtr);
  gcpPtr.p->nextGcp = cfirstfreeGcp;
  cfirstfreeGcp = gcpPtr.i;
}//Dbtc::releaseGcp()

void Dbtc::releaseKeys() 
{
  UintR Tmp;
  databufptr.i = cachePtr.p->firstKeybuf;
  while (databufptr.i != RNIL) {
    jam();
    ptrCheckGuard(databufptr, cdatabufFilesize, databufRecord);
    Tmp = databufptr.p->nextDatabuf;
    databufptr.p->nextDatabuf = cfirstfreeDatabuf;
    cfirstfreeDatabuf = databufptr.i;
    databufptr.i = Tmp;
  }//while
  cachePtr.p->firstKeybuf = RNIL;
  cachePtr.p->lastKeybuf = RNIL;
}//Dbtc::releaseKeys()

void Dbtc::releaseTcConnectFail(Signal* signal) 
{
  ptrGuard(tcConnectptr);
  tcConnectptr.p->nextTcConnect = cfirstfreeTcConnectFail;
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
    apiConnectptr.p->triggerPending = false;
    apiConnectptr.p->isIndexOp = false;
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
}//Dbtc::seizeApiConnectFail()

void Dbtc::seizeDatabuf(Signal* signal) 
{
  databufptr.i = cfirstfreeDatabuf;
  ptrCheckGuard(databufptr, cdatabufFilesize, databufRecord);
  cfirstfreeDatabuf = databufptr.p->nextDatabuf;
  databufptr.p->nextDatabuf = RNIL;
}//Dbtc::seizeDatabuf()

void Dbtc::seizeTcConnect(Signal* signal) 
{
  tcConnectptr.i = cfirstfreeTcConnect;
  ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
  cfirstfreeTcConnect = tcConnectptr.p->nextTcConnect;
  c_counters.cconcurrentOp++;
  tcConnectptr.p->isIndexOp = false;
}//Dbtc::seizeTcConnect()

void Dbtc::seizeTcConnectFail(Signal* signal) 
{
  tcConnectptr.i = cfirstfreeTcConnectFail;
  ptrCheckGuard(tcConnectptr, ctcConnectFilesize, tcConnectRecord);
  cfirstfreeTcConnectFail = tcConnectptr.p->nextTcConnect;
}//Dbtc::seizeTcConnectFail()

void Dbtc::sendAttrinfo(Signal* signal,
                        UintR TattrinfoPtr,
                        AttrbufRecord * const regAttrPtr,
                        UintR TBref) 
{
  UintR TdataPos;
  UintR sig0, sig1, sig2, sig3, sig4, sig5, sig6, sig7;
  ApiConnectRecord * const regApiPtr = apiConnectptr.p;
  TdataPos = regAttrPtr->attrbuf[ZINBUF_DATA_LEN];
  sig0 = TattrinfoPtr;
  sig1 = regApiPtr->transid[0];
  sig2 = regApiPtr->transid[1];

  signal->theData[0] = sig0;
  signal->theData[1] = sig1;
  signal->theData[2] = sig2;

  sig0 = regAttrPtr->attrbuf[0];
  sig1 = regAttrPtr->attrbuf[1];
  sig2 = regAttrPtr->attrbuf[2];
  sig3 = regAttrPtr->attrbuf[3];
  sig4 = regAttrPtr->attrbuf[4];
  sig5 = regAttrPtr->attrbuf[5];
  sig6 = regAttrPtr->attrbuf[6];
  sig7 = regAttrPtr->attrbuf[7];

  signal->theData[3] = sig0;
  signal->theData[4] = sig1;
  signal->theData[5] = sig2;
  signal->theData[6] = sig3;
  signal->theData[7] = sig4;
  signal->theData[8] = sig5;
  signal->theData[9] = sig6;
  signal->theData[10] = sig7;

  if (TdataPos > 8) {
    sig0 = regAttrPtr->attrbuf[8];
    sig1 = regAttrPtr->attrbuf[9];
    sig2 = regAttrPtr->attrbuf[10];
    sig3 = regAttrPtr->attrbuf[11];
    sig4 = regAttrPtr->attrbuf[12];
    sig5 = regAttrPtr->attrbuf[13];
    sig6 = regAttrPtr->attrbuf[14];

    jam();
    signal->theData[11] = sig0;
    signal->theData[12] = sig1;
    signal->theData[13] = sig2;
    signal->theData[14] = sig3;
    signal->theData[15] = sig4;
    signal->theData[16] = sig5;
    signal->theData[17] = sig6;

    if (TdataPos > 15) {

      sig0 = regAttrPtr->attrbuf[15];
      sig1 = regAttrPtr->attrbuf[16];
      sig2 = regAttrPtr->attrbuf[17];
      sig3 = regAttrPtr->attrbuf[18];
      sig4 = regAttrPtr->attrbuf[19];
      sig5 = regAttrPtr->attrbuf[20];
      sig6 = regAttrPtr->attrbuf[21];

      jam();
      signal->theData[18] = sig0;
      signal->theData[19] = sig1;
      signal->theData[20] = sig2;
      signal->theData[21] = sig3;
      signal->theData[22] = sig4;
      signal->theData[23] = sig5;
      signal->theData[24] = sig6;
    }//if
  }//if
  sendSignal(TBref, GSN_ATTRINFO, signal, TdataPos + 3, JBB);
}//Dbtc::sendAttrinfo()

void Dbtc::sendContinueTimeOutControl(Signal* signal, Uint32 TapiConPtr) 
{
  signal->theData[0] = TcContinueB::ZCONTINUE_TIME_OUT_CONTROL;
  signal->theData[1] = TapiConPtr;
  sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
}//Dbtc::sendContinueTimeOutControl()

void Dbtc::sendKeyinfo(Signal* signal, BlockReference TBRef, Uint32 len) 
{
  signal->theData[0] = tcConnectptr.i;
  signal->theData[1] = apiConnectptr.p->transid[0];
  signal->theData[2] = apiConnectptr.p->transid[1];
  signal->theData[3] = cdata[0];
  signal->theData[4] = cdata[1];
  signal->theData[5] = cdata[2];
  signal->theData[6] = cdata[3];
  signal->theData[7] = cdata[4];
  signal->theData[8] = cdata[5];
  signal->theData[9] = cdata[6];
  signal->theData[10] = cdata[7];
  signal->theData[11] = cdata[8];
  signal->theData[12] = cdata[9];
  signal->theData[13] = cdata[10];
  signal->theData[14] = cdata[11];
  signal->theData[15] = cdata[12];
  signal->theData[16] = cdata[13];
  signal->theData[17] = cdata[14];
  signal->theData[18] = cdata[15];
  signal->theData[19] = cdata[16];
  signal->theData[20] = cdata[17];
  signal->theData[21] = cdata[18];
  signal->theData[22] = cdata[19];
  sendSignal(TBRef, GSN_KEYINFO, signal, 3 + len, JBB);
}//Dbtc::sendKeyinfo()

void Dbtc::sendSystemError(Signal* signal, int line) 
{
  progError(line, NDBD_EXIT_NDBREQUIRE);
}//Dbtc::sendSystemError()

/* ========================================================================= */
/* -------             LINK ACTUAL GCP OUT OF LIST                   ------- */
/* ------------------------------------------------------------------------- */
void Dbtc::unlinkGcp(Signal* signal) 
{
  if (cfirstgcp == gcpPtr.i) {
    jam();
    cfirstgcp = gcpPtr.p->nextGcp;
    if (gcpPtr.i == clastgcp) {
      jam();
      clastgcp = RNIL;
    }//if
  } else {
    jam();
    /* -------------------------------------------------------------------- 
     * WE ARE TRYING TO REMOVE A GLOBAL CHECKPOINT WHICH WAS NOT THE OLDEST. 
     * THIS IS A SYSTEM ERROR.                                              
     * ------------------------------------------------------------------- */
    sendSystemError(signal, __LINE__);
  }//if
  gcpPtr.p->nextGcp = cfirstfreeGcp;
  cfirstfreeGcp = gcpPtr.i;
}//Dbtc::unlinkGcp()

void
Dbtc::execDUMP_STATE_ORD(Signal* signal)
{
  jamEntry();
  DumpStateOrd * const dumpState = (DumpStateOrd *)&signal->theData[0];
  Uint32 arg = signal->theData[0];
  if(signal->theData[0] == DumpStateOrd::CommitAckMarkersSize){
    infoEvent("TC: m_commitAckMarkerPool: %d free size: %d",
              m_commitAckMarkerPool.getNoOfFree(),
              m_commitAckMarkerPool.getSize());
  }
  if(signal->theData[0] == DumpStateOrd::CommitAckMarkersDump){
    infoEvent("TC: m_commitAckMarkerPool: %d free size: %d",
              m_commitAckMarkerPool.getNoOfFree(),
              m_commitAckMarkerPool.getSize());
    
    CommitAckMarkerIterator iter;
    for(m_commitAckMarkerHash.first(iter); iter.curr.i != RNIL;
        m_commitAckMarkerHash.next(iter)){
      infoEvent("CommitAckMarker: i = %d (0x%x, 0x%x)"
                " Api: %d Lghs(%d): %d %d %d %d bucket = %d",
                iter.curr.i,
                iter.curr.p->transid1,
                iter.curr.p->transid2,
                iter.curr.p->apiNodeId,
                iter.curr.p->noOfLqhs,
                iter.curr.p->lqhNodeId[0],
                iter.curr.p->lqhNodeId[1],
                iter.curr.p->lqhNodeId[2],
                iter.curr.p->lqhNodeId[3],
                iter.bucket);
    }
  }
  // Dump all ScanFragRecs
  if (dumpState->args[0] == DumpStateOrd::TcDumpAllScanFragRec){
    Uint32 recordNo = 0;
    if (signal->getLength() == 1)
      infoEvent("TC: Dump all ScanFragRec - size: %d",
		cscanFragrecFileSize);
    else if (signal->getLength() == 2)
      recordNo = dumpState->args[1];
    else
      return;
    
    dumpState->args[0] = DumpStateOrd::TcDumpOneScanFragRec;
    dumpState->args[1] = recordNo;
    execDUMP_STATE_ORD(signal);

    if (recordNo < cscanFragrecFileSize-1){
      dumpState->args[0] = DumpStateOrd::TcDumpAllScanFragRec;
      dumpState->args[1] = recordNo+1;
      sendSignal(reference(), GSN_DUMP_STATE_ORD, signal, 2, JBB);
    }
  }

  // Dump one ScanFragRec
  if (dumpState->args[0] == DumpStateOrd::TcDumpOneScanFragRec){
    Uint32 recordNo = RNIL;
    if (signal->getLength() == 2)
      recordNo = dumpState->args[1];
    else
      return;

    if (recordNo >= cscanFragrecFileSize)
      return;

    ScanFragRecPtr sfp;
    sfp.i = recordNo;
    c_scan_frag_pool.getPtr(sfp);
    infoEvent("Dbtc::ScanFragRec[%d]: state=%d fragid=%d",
	      sfp.i,
	      sfp.p->scanFragState,
	      sfp.p->scanFragId);
    infoEvent(" nodeid=%d, timer=%d",
	      refToNode(sfp.p->lqhBlockref),
	      sfp.p->scanFragTimer);
  }

  // Dump all ScanRecords
  if (dumpState->args[0] == DumpStateOrd::TcDumpAllScanRec){
    Uint32 recordNo = 0;
    if (signal->getLength() == 1)
      infoEvent("TC: Dump all ScanRecord - size: %d",
		cscanrecFileSize);
    else if (signal->getLength() == 2)
      recordNo = dumpState->args[1];
    else
      return;  

    dumpState->args[0] = DumpStateOrd::TcDumpOneScanRec;
    dumpState->args[1] = recordNo;
    execDUMP_STATE_ORD(signal);
    
    if (recordNo < cscanrecFileSize-1){
      dumpState->args[0] = DumpStateOrd::TcDumpAllScanRec;
      dumpState->args[1] = recordNo+1;
      sendSignal(reference(), GSN_DUMP_STATE_ORD, signal, 2, JBB);
    }
  }

  // Dump all active ScanRecords
  if (dumpState->args[0] == DumpStateOrd::TcDumpAllActiveScanRec){
    Uint32 recordNo = 0;     
    if (signal->getLength() == 1)
      infoEvent("TC: Dump active ScanRecord - size: %d",
		cscanrecFileSize);
    else if (signal->getLength() == 2)
      recordNo = dumpState->args[1];
    else
      return;

    ScanRecordPtr sp;
    sp.i = recordNo;
    ptrAss(sp, scanRecord);
    if (sp.p->scanState != ScanRecord::IDLE){
      dumpState->args[0] = DumpStateOrd::TcDumpOneScanRec;
      dumpState->args[1] = recordNo;
      execDUMP_STATE_ORD(signal);
    }

    if (recordNo < cscanrecFileSize-1){
      dumpState->args[0] = DumpStateOrd::TcDumpAllActiveScanRec;
      dumpState->args[1] = recordNo+1;
      sendSignal(reference(), GSN_DUMP_STATE_ORD, signal, 2, JBB);
    }
  }

  // Dump one ScanRecord
  // and associated ScanFragRec and ApiConnectRecord
  if (dumpState->args[0] == DumpStateOrd::TcDumpOneScanRec){
    Uint32 recordNo = RNIL;
    if (signal->getLength() == 2)
      recordNo = dumpState->args[1];
    else
      return;

    if (recordNo >= cscanrecFileSize)
      return;

    ScanRecordPtr sp;
    sp.i = recordNo;
    ptrAss(sp, scanRecord);
    infoEvent("Dbtc::ScanRecord[%d]: state=%d"
	      "nextfrag=%d, nofrag=%d",
	      sp.i,
	      sp.p->scanState,
	      sp.p->scanNextFragId,
	      sp.p->scanNoFrag);
    infoEvent(" ailen=%d, para=%d, receivedop=%d, noOprePperFrag=%d",
	      sp.p->scanAiLength,
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
      ScanFragList list(c_scan_frag_pool, x);\
      for(list.first(sfptr); !sfptr.isNull(); list.next(sfptr)){\
	dumpState->args[0] = DumpStateOrd::TcDumpOneScanFragRec; \
	dumpState->args[1] = sfptr.i;\
	execDUMP_STATE_ORD(signal);\
      }}

      DUMP_SFR(sp.p->m_running_scan_frags);
      DUMP_SFR(sp.p->m_queued_scan_frags);
      DUMP_SFR(sp.p->m_delivered_scan_frags);

      // Request dump of ApiConnectRecord
      dumpState->args[0] = DumpStateOrd::TcDumpOneApiConnectRec;
      dumpState->args[1] = sp.p->scanApiRec;
      execDUMP_STATE_ORD(signal);      
    }

  }

  // Dump all ApiConnectRecord(s)
  if (dumpState->args[0] == DumpStateOrd::TcDumpAllApiConnectRec){
    Uint32 recordNo = 0;
    if (signal->getLength() == 1)
      infoEvent("TC: Dump all ApiConnectRecord - size: %d",
		capiConnectFilesize);
    else if (signal->getLength() == 2)
      recordNo = dumpState->args[1];
    else
      return;

    dumpState->args[0] = DumpStateOrd::TcDumpOneApiConnectRec;
    dumpState->args[1] = recordNo;
    execDUMP_STATE_ORD(signal);
    
    if (recordNo < capiConnectFilesize-1){
      dumpState->args[0] = DumpStateOrd::TcDumpAllApiConnectRec;
      dumpState->args[1] = recordNo+1;
      sendSignal(reference(), GSN_DUMP_STATE_ORD, signal, 2, JBB);
    }
  }

  // Dump one ApiConnectRecord
  if (dumpState->args[0] == DumpStateOrd::TcDumpOneApiConnectRec){
    Uint32 recordNo = RNIL;
    if (signal->getLength() == 2)
      recordNo = dumpState->args[1];
    else
      return;

    if (recordNo >= capiConnectFilesize)
      return;

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
    infoEvent(" next=%d ",
	      ap.p->nextApiConnect);
  }

  if (dumpState->args[0] == DumpStateOrd::TcSetTransactionTimeout){
    jam();
    if(signal->getLength() > 1){
      set_timeout_value(signal->theData[1]);
    }
  }

  if (dumpState->args[0] == DumpStateOrd::TcSetApplTransactionTimeout){
    jam();
    if(signal->getLength() > 1){
      set_appl_timeout_value(signal->theData[1]);
    }
  }

  if (dumpState->args[0] == DumpStateOrd::StartTcTimer){
    c_counters.c_trans_status = TransCounters::Started;
    c_counters.reset();
  }

  if (dumpState->args[0] == DumpStateOrd::StopTcTimer){
    c_counters.c_trans_status = TransCounters::Off;
    Uint32 len = c_counters.report(signal);
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, len, JBB);
    c_counters.reset();
  }
  
  if (dumpState->args[0] == DumpStateOrd::StartPeriodicTcTimer){
    c_counters.c_trans_status = TransCounters::Timer;
    c_counters.reset();
    signal->theData[0] = TcContinueB::ZTRANS_EVENT_REP;
    sendSignalWithDelay(cownref, GSN_CONTINUEB, signal, 5000, 1);
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
    infoEvent("IndexOpCount: pool: %d free: %d", 
	      c_theIndexOperationPool.getSize(),
	      c_theIndexOperationPool.getNoOfFree());
  }

  if (dumpState->args[0] == 2514)
  {
    if (signal->getLength() == 2)
    {
      dumpState->args[0] = DumpStateOrd::TcDumpOneApiConnectRec;
      execDUMP_STATE_ORD(signal);
    }

    NodeReceiverGroup rg(CMVMI, c_alive_nodes);
    dumpState->args[0] = 15;
    sendSignal(rg, GSN_DUMP_STATE_ORD, signal, 1, JBB);

    signal->theData[0] = 2515;
    sendSignalWithDelay(cownref, GSN_DUMP_STATE_ORD, signal, 1000, 1);    
    return;
  }

  if (dumpState->args[0] == 2515)
  {
    NdbNodeBitmask mask = c_alive_nodes;
    mask.clear(getOwnNodeId());
    NodeReceiverGroup rg(NDBCNTR, mask);
    
    sendSignal(rg, GSN_SYSTEM_ERROR, signal, 1, JBB);
    sendSignalWithDelay(cownref, GSN_SYSTEM_ERROR, signal, 300, 1);    
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
}//Dbtc::execDUMP_STATE_ORD()

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
      infoEvent("Invalid filter op: 0x%x pos: %d",
		* start,
		start - (signal->theData + 1));
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
  case CS_REC_PREPARING:
  case CS_START_PREPARING:
  case CS_PREPARED:
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
  infoEvent(buf);
  
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

void Dbtc::execCREATE_TRIG_REQ(Signal* signal)
{
  jamEntry();
  CreateTrigReq * const createTrigReq = 
    (CreateTrigReq *)&signal->theData[0];
  TcDefinedTriggerData* triggerData;
  DefinedTriggerPtr triggerPtr;
  BlockReference sender = signal->senderBlockRef();

  releaseSections(signal);
  
  triggerPtr.i = createTrigReq->getTriggerId();
  if (ERROR_INSERTED(8033) ||
      !c_theDefinedTriggers.seizeId(triggerPtr, 
				    createTrigReq->getTriggerId())) {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    // Failed to allocate trigger record
    CreateTrigRef * const createTrigRef =  
      (CreateTrigRef *)&signal->theData[0];
    
    createTrigRef->setConnectionPtr(createTrigReq->getConnectionPtr());
    createTrigRef->setErrorCode(CreateTrigRef::TooManyTriggers);
    sendSignal(sender, GSN_CREATE_TRIG_REF, 
               signal, CreateTrigRef::SignalLength, JBB);
    return;
  }

  triggerData = triggerPtr.p;
  triggerData->triggerId = createTrigReq->getTriggerId();
  triggerData->triggerType = createTrigReq->getTriggerType();
  triggerData->triggerEvent = createTrigReq->getTriggerEvent();
  triggerData->attributeMask = createTrigReq->getAttributeMask();
  if (triggerData->triggerType == TriggerType::SECONDARY_INDEX)
    triggerData->indexId = createTrigReq->getIndexId();
  CreateTrigConf * const createTrigConf =  
    (CreateTrigConf *)&signal->theData[0];
  
  createTrigConf->setConnectionPtr(createTrigReq->getConnectionPtr());
  sendSignal(sender, GSN_CREATE_TRIG_CONF, 
             signal, CreateTrigConf::SignalLength, JBB);
}


void Dbtc::execDROP_TRIG_REQ(Signal* signal)
{
  jamEntry();
  DropTrigReq * const dropTrigReq =  (DropTrigReq *)&signal->theData[0];
  BlockReference sender = signal->senderBlockRef();

  if (ERROR_INSERTED(8035) ||
      (c_theDefinedTriggers.getPtr(dropTrigReq->getTriggerId())) == NULL) {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    // Failed to find find trigger record
    DropTrigRef * const dropTrigRef =  (DropTrigRef *)&signal->theData[0];

    dropTrigRef->setConnectionPtr(dropTrigReq->getConnectionPtr());
    dropTrigRef->setErrorCode(DropTrigRef::TriggerNotFound);
    sendSignal(sender, GSN_DROP_TRIG_REF, 
               signal, DropTrigRef::SignalLength, JBB);
    return;
  }

  // Release trigger record
  c_theDefinedTriggers.release(dropTrigReq->getTriggerId());

  DropTrigConf * const dropTrigConf =  (DropTrigConf *)&signal->theData[0];
  
  dropTrigConf->setConnectionPtr(dropTrigReq->getConnectionPtr());
  sendSignal(sender, GSN_DROP_TRIG_CONF, 
             signal, DropTrigConf::SignalLength, JBB);
}

void Dbtc::execCREATE_INDX_REQ(Signal* signal)
{
  jamEntry();
  CreateIndxReq * const createIndxReq =  
    (CreateIndxReq *)signal->getDataPtr();
  TcIndexData* indexData;
  TcIndexDataPtr indexPtr;
  BlockReference sender = signal->senderBlockRef();
  
  if (ERROR_INSERTED(8034) ||
      !c_theIndexes.seizeId(indexPtr, createIndxReq->getIndexId())) {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    // Failed to allocate index record
     CreateIndxRef * const createIndxRef =  
       (CreateIndxRef *)&signal->theData[0];

     createIndxRef->setConnectionPtr(createIndxReq->getConnectionPtr());
     createIndxRef->setErrorCode(CreateIndxRef::TooManyIndexes);
     releaseSections(signal);
     sendSignal(sender, GSN_CREATE_INDX_REF, 
                signal, CreateIndxRef::SignalLength, JBB);
     return;
  }
  indexData = indexPtr.p;
  // Indexes always start in state IS_BUILDING
  // Will become IS_ONLINE in execALTER_INDX_REQ
  indexData->indexState = IS_BUILDING;
  indexData->indexId = indexPtr.i;
  indexData->primaryTableId = createIndxReq->getTableId();

  // So far need only attribute count
  SegmentedSectionPtr ssPtr;
  signal->getSection(ssPtr, CreateIndxReq::ATTRIBUTE_LIST_SECTION);
  SimplePropertiesSectionReader r0(ssPtr, getSectionSegmentPool());
  r0.reset(); // undo implicit first()
  if (!r0.getWord(&indexData->attributeList.sz) ||
      !r0.getWords(indexData->attributeList.id, indexData->attributeList.sz)) {
    ndbrequire(false);
  }
  indexData->primaryKeyPos = indexData->attributeList.sz;

  releaseSections(signal);
  
  CreateIndxConf * const createIndxConf =  
    (CreateIndxConf *)&signal->theData[0];

  createIndxConf->setConnectionPtr(createIndxReq->getConnectionPtr());
  createIndxConf->setTableId(createIndxReq->getTableId());
  createIndxConf->setIndexId(createIndxReq->getIndexId());
  sendSignal(sender, GSN_CREATE_INDX_CONF, 
             signal, CreateIndxConf::SignalLength, JBB);
}

void Dbtc::execALTER_INDX_REQ(Signal* signal)
{
  jamEntry();
  AlterIndxReq * const alterIndxReq =  (AlterIndxReq *)signal->getDataPtr();
  TcIndexData* indexData;
  //BlockReference sender = signal->senderBlockRef();
  BlockReference sender = (BlockReference) alterIndxReq->getUserRef();
  Uint32 connectionPtr = alterIndxReq->getConnectionPtr();
  AlterIndxReq::RequestType requestType = alterIndxReq->getRequestType();
  Uint32 tableId = alterIndxReq->getTableId();
  Uint32 indexId = alterIndxReq->getIndexId();
  bool online = (alterIndxReq->getOnline() == 1) ? true : false;

  if ((indexData = c_theIndexes.getPtr(indexId)) == NULL) {
    jam();
    // Failed to find index record
    AlterIndxRef * const alterIndxRef =  
      (AlterIndxRef *)signal->getDataPtrSend();
    
    alterIndxRef->setUserRef(reference());
    alterIndxRef->setConnectionPtr(connectionPtr);
    alterIndxRef->setRequestType(requestType);
    alterIndxRef->setTableId(tableId);
    alterIndxRef->setIndexId(indexId);
    alterIndxRef->setErrorCode(AlterIndxRef::IndexNotFound);
    alterIndxRef->setErrorLine(__LINE__);
    alterIndxRef->setErrorNode(getOwnNodeId());
    sendSignal(sender, GSN_ALTER_INDX_REF, 
	       signal, AlterIndxRef::SignalLength, JBB);
    return;
  }
  // Found index record, alter it's state  
  if (online) {
    jam();
    indexData->indexState = IS_ONLINE;
  } else {
    jam();
    indexData->indexState = IS_BUILDING;
  }//if 
  AlterIndxConf * const alterIndxConf =  
    (AlterIndxConf *)signal->getDataPtrSend();
  
  alterIndxConf->setUserRef(reference());
  alterIndxConf->setConnectionPtr(connectionPtr);
  alterIndxConf->setRequestType(requestType);
  alterIndxConf->setTableId(tableId);
  alterIndxConf->setIndexId(indexId);
  sendSignal(sender, GSN_ALTER_INDX_CONF, 
	     signal, AlterIndxConf::SignalLength, JBB);
}

void Dbtc::execFIRE_TRIG_ORD(Signal* signal)
{
  jamEntry();
  FireTrigOrd * const fireOrd =  (FireTrigOrd *)signal->getDataPtr();
  ApiConnectRecord *localApiConnectRecord = apiConnectRecord;
  ApiConnectRecordPtr transPtr;
  TcConnectRecord *localTcConnectRecord = tcConnectRecord;
  TcConnectRecordPtr opPtr;
  /**
   * TODO
   * Check transid,
   * Fix overload i.e invalid word count
   */
  TcFiredTriggerData key;
  key.fireingOperation = fireOrd->getConnectionPtr();
  key.nodeId = refToNode(signal->getSendersBlockRef());
  FiredTriggerPtr trigPtr;
  if(c_firedTriggerHash.find(trigPtr, key)){
    
    c_firedTriggerHash.remove(trigPtr);

    trigPtr.p->fragId= fireOrd->fragId;
    bool ok = trigPtr.p->keyValues.getSize() == fireOrd->m_noPrimKeyWords;
    ok &= trigPtr.p->afterValues.getSize() == fireOrd->m_noAfterValueWords;
    ok &= trigPtr.p->beforeValues.getSize() == fireOrd->m_noBeforeValueWords;
    if(ok){
      opPtr.i = key.fireingOperation;
      ptrCheckGuard(opPtr, ctcConnectFilesize, localTcConnectRecord);
      transPtr.i = opPtr.p->apiConnect;
      transPtr.p = &localApiConnectRecord[transPtr.i];
      
      opPtr.p->noReceivedTriggers++;
      opPtr.p->triggerExecutionCount++;
    
      // Insert fired trigger in execution queue
      transPtr.p->theFiredTriggers.add(trigPtr);
      if (opPtr.p->noReceivedTriggers == opPtr.p->noFiredTriggers) {
	executeTriggers(signal, &transPtr);
      }
      return;
    }
    jam();
    c_theFiredTriggerPool.release(trigPtr);
  }
  jam();
  /**
   * Failed to find record or invalid word counts
   */
  ndbrequire(false);
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
      LocalDataBuffer<11> buf(pool, firedTrigPtr.p->keyValues);
      buf.append(src, attrInfoLength);
    }
    break;
  case(TrigAttrInfo::BEFORE_VALUES):
    jam();
    {
      LocalDataBuffer<11> buf(pool, firedTrigPtr.p->beforeValues);
      buf.append(src, attrInfoLength);
    }
    break;
  case(TrigAttrInfo::AFTER_VALUES):
    jam();
    {
      LocalDataBuffer<11> buf(pool, firedTrigPtr.p->afterValues);
      buf.append(src, attrInfoLength);
    }
    break;
  default:
    ndbrequire(false);
  }
}

void Dbtc::execDROP_INDX_REQ(Signal* signal)
{
  jamEntry();
  DropIndxReq * const dropIndxReq =  (DropIndxReq *)signal->getDataPtr();
  TcIndexData* indexData;
  BlockReference sender = signal->senderBlockRef();
  
  if (ERROR_INSERTED(8036) ||
      (indexData = c_theIndexes.getPtr(dropIndxReq->getIndexId())) == NULL) {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    // Failed to find index record
    DropIndxRef * const dropIndxRef =  
      (DropIndxRef *)signal->getDataPtrSend();

    dropIndxRef->setConnectionPtr(dropIndxReq->getConnectionPtr());
    dropIndxRef->setErrorCode(DropIndxRef::IndexNotFound);
    sendSignal(sender, GSN_DROP_INDX_REF, 
               signal, DropIndxRef::SignalLength, JBB);
    return;
  }
  // Release index record
  c_theIndexes.release(dropIndxReq->getIndexId());

  DropIndxConf * const dropIndxConf =  
    (DropIndxConf *)signal->getDataPtrSend();

  dropIndxConf->setConnectionPtr(dropIndxReq->getConnectionPtr());
  sendSignal(sender, GSN_DROP_INDX_CONF, 
             signal, DropIndxConf::SignalLength, JBB);
}

void Dbtc::execTCINDXREQ(Signal* signal)
{
  jamEntry();

  TcKeyReq * const tcIndxReq =  (TcKeyReq *)signal->getDataPtr();
  const UintR TapiIndex = tcIndxReq->apiConnectPtr;
  Uint32 tcIndxRequestInfo = tcIndxReq->requestInfo;
  Uint32 startFlag = tcIndxReq->getStartFlag(tcIndxRequestInfo);
  Uint32 * dataPtr = &tcIndxReq->scanInfo;
  Uint32 indexBufSize = 8; // Maximum for index in TCINDXREQ
  Uint32 attrBufSize = 5;  // Maximum for attrInfo in TCINDXREQ
  ApiConnectRecordPtr transPtr;
  transPtr.i = TapiIndex;
  if (transPtr.i >= capiConnectFilesize) {
    jam();
    warningHandlerLab(signal, __LINE__);
    return;
  }//if
  ptrAss(transPtr, apiConnectRecord);
  ApiConnectRecord * const regApiPtr = transPtr.p;  
  // Seize index operation
  TcIndexOperationPtr indexOpPtr;
  if ((startFlag == 1) &&
      (regApiPtr->apiConnectstate == CS_CONNECTED ||
       (regApiPtr->apiConnectstate == CS_STARTED && 
	regApiPtr->firstTcConnect == RNIL)) ||
      (regApiPtr->apiConnectstate == CS_ABORTING && 
       regApiPtr->abortState == AS_IDLE)) {
    jam();
    // This is a newly started transaction, clean-up
    releaseAllSeizedIndexOperations(regApiPtr);

    regApiPtr->apiConnectstate = CS_STARTED;
    regApiPtr->transid[0] = tcIndxReq->transId1;
    regApiPtr->transid[1] = tcIndxReq->transId2;
  }//if

  if (getNodeState().startLevel == NodeState::SL_SINGLEUSER &&
      getNodeState().getSingleUserApi() !=
      refToNode(regApiPtr->ndbapiBlockref))
  {
    terrorCode = ZCLUSTER_IN_SINGLEUSER_MODE;
    regApiPtr->m_exec_flag |= TcKeyReq::getExecuteFlag(tcIndxRequestInfo);
    apiConnectptr = transPtr;
    abortErrorLab(signal);
    return;
  }

  if (ERROR_INSERTED(8036) || !seizeIndexOperation(regApiPtr, indexOpPtr)) {
    jam();
    // Failed to allocate index operation
    terrorCode = 288;
    regApiPtr->m_exec_flag |= TcKeyReq::getExecuteFlag(tcIndxRequestInfo);
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

  // If operation is readTupleExclusive or updateTuple then read index 
  // table with exclusive lock
  Uint32 indexLength = TcKeyReq::getKeyLength(tcIndxRequestInfo);
  Uint32 attrLength = TcKeyReq::getAttrinfoLen(tcIndxReq->attrLen);
  indexOp->expectedKeyInfo = indexLength;
  Uint32 includedIndexLength = MIN(indexLength, indexBufSize);
  indexOp->expectedAttrInfo = attrLength;
  Uint32 includedAttrLength = MIN(attrLength, attrBufSize);

  int ret;
  if ((ret = saveINDXKEYINFO(signal, 
                             indexOp, 
                             dataPtr, 
                             includedIndexLength)) == 0) 
  {
    jam();
    // We have received all we need
    readIndexTable(signal, regApiPtr, indexOp);
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
    // We have received all we need
    readIndexTable(signal, regApiPtr, indexOp);
    return;
  }
}


void Dbtc::sendTcIndxConf(Signal* signal, UintR TcommitFlag) 
{
  HostRecordPtr localHostptr;
  ApiConnectRecord * const regApiPtr = apiConnectptr.p;
  const UintR TopWords = (UintR)regApiPtr->tcindxrec;
  localHostptr.i = refToNode(regApiPtr->ndbapiBlockref);
  const Uint32 type = getNodeInfo(localHostptr.i).m_type;
  const bool is_api = (type >= NodeInfo::API && type <= NodeInfo::MGM);
  const BlockNumber TblockNum = refToBlock(regApiPtr->ndbapiBlockref);
  const Uint32 Tmarker = (regApiPtr->commitAckMarker == RNIL ? 0 : 1);
  ptrAss(localHostptr, hostRecord);
  UintR TcurrLen = localHostptr.p->noOfWordsTCINDXCONF;
  UintR confInfo = 0;
  TcIndxConf::setNoOfOperations(confInfo, (TopWords >> 1));
  TcIndxConf::setCommitFlag(confInfo, TcommitFlag == 1);
  TcIndxConf::setMarkerFlag(confInfo, Tmarker);
  const UintR TpacketLen = 6 + TopWords;
  regApiPtr->tcindxrec = 0;

  if(TcommitFlag || (regApiPtr->lqhkeyreqrec == regApiPtr->lqhkeyconfrec)){
    jam();
    regApiPtr->m_exec_flag = 0;
  }

  if ((TpacketLen + 1 /** gci_lo */ > 25) || !is_api){
    TcIndxConf * const tcIndxConf = (TcIndxConf *)signal->getDataPtrSend();
    
    jam();
    tcIndxConf->apiConnectPtr = regApiPtr->ndbapiConnect;
    tcIndxConf->gci_hi = Uint32(regApiPtr->globalcheckpointid >> 32);
    Uint32 *gci_lo = (Uint32*)&tcIndxConf->operations[TopWords >> 1];
    * gci_lo = Uint32(regApiPtr->globalcheckpointid);
    tcIndxConf->confInfo = confInfo;
    tcIndxConf->transId1 = regApiPtr->transid[0];
    tcIndxConf->transId2 = regApiPtr->transid[1];
    copyFromToLen(&regApiPtr->tcIndxSendArray[0],
                  (UintR*)&tcIndxConf->operations,
                  (UintR)ZTCOPCONF_SIZE);
    sendSignal(regApiPtr->ndbapiBlockref,
               GSN_TCINDXCONF, signal,
               (TpacketLen - 1) + 1 /** gci_lo */, JBB);
    return;
  } else if (((TcurrLen + TpacketLen + 1 /** gci_lo */) > 25) &&
             (TcurrLen > 0)) {
    jam();
    sendPackedTCINDXCONF(signal, localHostptr.p, localHostptr.i);
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
  UintR Tpack0 = (TblockNum << 16) + (TpacketLen - 4 + 1 /** gci_lo */);
  UintR Tpack1 = regApiPtr->ndbapiConnect;
  UintR Tpack2 = Uint32(regApiPtr->globalcheckpointid >> 32);
  UintR Tpack3 = confInfo;
  UintR Tpack4 = regApiPtr->transid[0];
  UintR Tpack5 = regApiPtr->transid[1];
  UintR Tpack6 = Uint32(regApiPtr->globalcheckpointid);

  localHostptr.p->noOfWordsTCINDXCONF = TcurrLen + TpacketLen + 1 /* gci_lo */;

  localHostptr.p->packedWordsTCINDXCONF[TcurrLen + 0] = Tpack0;
  localHostptr.p->packedWordsTCINDXCONF[TcurrLen + 1] = Tpack1;
  localHostptr.p->packedWordsTCINDXCONF[TcurrLen + 2] = Tpack2;
  localHostptr.p->packedWordsTCINDXCONF[TcurrLen + 3] = Tpack3;
  localHostptr.p->packedWordsTCINDXCONF[TcurrLen + 4] = Tpack4;
  localHostptr.p->packedWordsTCINDXCONF[TcurrLen + 5] = Tpack5;

  UintR Ti;
  for (Ti = 6; Ti < TpacketLen; Ti++) {
    localHostptr.p->packedWordsTCINDXCONF[TcurrLen + Ti] = 
          regApiPtr->tcIndxSendArray[Ti - 6];
  }//for
  localHostptr.p->packedWordsTCINDXCONF[TcurrLen + TpacketLen] = Tpack6;

  if (unlikely(!ndb_check_micro_gcp(getNodeInfo(localHostptr.i).m_version)))
  {
    jam();
    ndbassert(Tpack6 == 0 || getNodeInfo(localHostptr.i).m_connected == false);
  }
}//Dbtc::sendTcIndxConf()

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
    if (saveINDXKEYINFO(signal,
			indexOp, 
			src, 
			keyInfoLength) == 0) {
      jam();
      // We have received all we need
      readIndexTable(signal, regApiPtr, indexOp);
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
    if (saveINDXATTRINFO(signal,
			 indexOp, 
			 src, 
			 attrInfoLength) == 0) {
      jam();
      // We have received all we need
      readIndexTable(signal, regApiPtr, indexOp);
      return;
    }
    return;
  }
}

/**
 * Save signal INDXKEYINFO
 * Return true if we have received all needed data
 */
int 
Dbtc::saveINDXKEYINFO(Signal* signal,
                      TcIndexOperation* indexOp,
                      const Uint32 *src, 
                      Uint32 len)
{
  if (ERROR_INSERTED(8052) || !indexOp->keyInfo.append(src, len)) {
    jam();
    // Failed to seize keyInfo, abort transaction
#ifdef VM_TRACE
    ndbout_c("Dbtc::saveINDXKEYINFO: Failed to seize keyinfo\n");
#endif
    // Abort transaction
    apiConnectptr.i = indexOp->connectionIndex;
    ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
    releaseIndexOperation(apiConnectptr.p, indexOp);
    terrorCode = 289;
    if(TcKeyReq::getExecuteFlag(indexOp->tcIndxReq.requestInfo))
      apiConnectptr.p->m_exec_flag= 1;
    abortErrorLab(signal);
    return -1;
  }
  if (receivedAllINDXKEYINFO(indexOp) && receivedAllINDXATTRINFO(indexOp)) {
    jam();
    return 0;
  }
  return 1;
}

bool Dbtc::receivedAllINDXKEYINFO(TcIndexOperation* indexOp)
{
  return (indexOp->keyInfo.getSize() == indexOp->expectedKeyInfo);
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
  if (ERROR_INSERTED(8051) || !indexOp->attrInfo.append(src, len)) {
    jam();
#ifdef VM_TRACE
    ndbout_c("Dbtc::saveINDXATTRINFO: Failed to seize attrInfo\n");
#endif
    apiConnectptr.i = indexOp->connectionIndex;
    ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
    releaseIndexOperation(apiConnectptr.p, indexOp);
    terrorCode = 289;
    if(TcKeyReq::getExecuteFlag(indexOp->tcIndxReq.requestInfo))
      apiConnectptr.p->m_exec_flag= 1;
    abortErrorLab(signal);
    return -1;
  }
  if (receivedAllINDXKEYINFO(indexOp) && receivedAllINDXATTRINFO(indexOp)) {
    jam();
    return 0;
  }
  return 1;
}

bool Dbtc::receivedAllINDXATTRINFO(TcIndexOperation* indexOp)
{
  return (indexOp->attrInfo.getSize() == indexOp->expectedAttrInfo);
}

bool  Dbtc::saveTRANSID_AI(Signal* signal,
			   TcIndexOperation* indexOp, 
                           const Uint32 *src,
                           Uint32 len)
{
  Uint32 currentTransIdAILength = indexOp->transIdAI.getSize();
    
  if (currentTransIdAILength == 0) {
    jam();
    // Read first AttributeHeader to get expected size
    // of the single key attribute expected
    AttributeHeader* head = (AttributeHeader *) src;
    indexOp->expectedTransIdAI = head->getHeaderSize() + head->getDataSize();
  }
  if (!indexOp->transIdAI.append(src, len)) {
    jam();
#ifdef VM_TRACE
    ndbout_c("Dbtc::saveTRANSID_AI: Failed to seize transIdAI\n");
#endif
    apiConnectptr.i = indexOp->connectionIndex;
    ptrCheckGuard(apiConnectptr, capiConnectFilesize, apiConnectRecord);
    releaseIndexOperation(apiConnectptr.p, indexOp);
    terrorCode = 4000;
    abortErrorLab(signal);
    return false;
  }
  return true;
}

bool Dbtc::receivedAllTRANSID_AI(TcIndexOperation* indexOp)
{
  return (indexOp->transIdAI.getSize() == indexOp->expectedTransIdAI);
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
  Uint32 confInfo = tcKeyConf->confInfo;

  /**
   * Check on TCKEYCONF wheater the the transaction was committed
   */
  Uint32 Tcommit = TcKeyConf::getCommitFlag(confInfo);

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
    tcIndxRef->errorCode = 4349;    
    tcIndxRef->errorData = 0;
    sendSignal(regApiPtr->ndbapiBlockref, GSN_TCINDXREF, signal, 
	       TcKeyRef::SignalLength, JBB);
    return;
  }
  case(IOS_INDEX_ACCESS): {
    jam();
    // Wait for TRANSID_AI
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
    tcIndxRef->errorCode = 4349;    
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
  case(IOS_INDEX_OPERATION): {
    // We are done, send TCINDXCONF
    jam();    
    Uint32 Ttcindxrec = regApiPtr->tcindxrec;
    // Copy reply from TcKeyConf

    ndbassert(regApiPtr->noIndexOp);
    regApiPtr->noIndexOp--; // Decrease count
    regApiPtr->tcIndxSendArray[Ttcindxrec] = indexOp->tcIndxReq.senderData;
    regApiPtr->tcIndxSendArray[Ttcindxrec + 1] = 
      tcKeyConf->operations[0].attrInfoLen;
    regApiPtr->tcindxrec = Ttcindxrec + 2;
    if (regApiPtr->noIndexOp == 0) {
      jam();
      sendTcIndxConf(signal, Tcommit);
    } else if (regApiPtr->tcindxrec == ZTCOPCONF_SIZE) {
      jam();
      sendTcIndxConf(signal, 0);
    }
    releaseIndexOperation(regApiPtr, indexOp);
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
    /**
     * Increase count as it will be decreased below...
     *   (and the code is written to handle failing lookup on "real" table
     *    not lookup on index table)
     */
    regApiPtr->noIndexOp++;
    // else continue
  }
  case(IOS_INDEX_OPERATION): {
    // Send TCINDXREF 
    
    jam();
    TcKeyReq * const tcIndxReq = &indexOp->tcIndxReq;
    TcKeyRef * const tcIndxRef = (TcKeyRef *)signal->getDataPtrSend();
    
    ndbassert(regApiPtr->noIndexOp);
    regApiPtr->noIndexOp--; // Decrease count
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

  /**
   * Forward signal to final destination
   * Truncate last word since that was used to hold the final dest.
   */
  sendSignal(recBlockref, GSN_TRANSID_AI,
	     signal, sigLen - 1, JBB);
}

void Dbtc::execKEYINFO20_R(Signal* signal){
  KeyInfo20 * const keyInfo =  (KeyInfo20 *)signal->getDataPtr();
  Uint32 sigLen = signal->length();
  Uint32 dataLen = sigLen - KeyInfo20::HeaderLength - 1;
  Uint32 recBlockref = keyInfo->keyData[dataLen];

  jamEntry();
  
  /**
   * Forward signal to final destination
   * Truncate last word since that was used to hold the final dest.
   */
  sendSignal(recBlockref, GSN_KEYINFO20,
	     signal, sigLen - 1, JBB);
}


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
  //  ApiConnectRecord * const regApiPtr = &apiConnectRecord[TconnectIndex];
  ApiConnectRecordPtr transPtr;
  
  transPtr.i = TconnectIndex;
  ptrCheckGuard(transPtr, capiConnectFilesize, apiConnectRecord);
  ApiConnectRecord * const regApiPtr = transPtr.p;

  // Acccumulate attribute data
  if (!saveTRANSID_AI(signal,
		      indexOp, 
                      transIdAI->getData(), 
                      signal->getLength() - TransIdAI::HeaderLength)) {
    jam();
    // Failed to allocate space for TransIdAI
    TcKeyRef * const tcIndxRef = (TcKeyRef *)signal->getDataPtrSend();
    
    tcIndxRef->connectPtr = indexOp->tcIndxReq.senderData;
    tcIndxRef->transId[0] = regApiPtr->transid[0];
    tcIndxRef->transId[1] = regApiPtr->transid[1];
    tcIndxRef->errorCode = 4000;
    tcIndxRef->errorData = 0;
    sendSignal(regApiPtr->ndbapiBlockref, GSN_TCINDXREF, signal, 
	       TcKeyRef::SignalLength, JBB);
    return;
  }

  switch(indexOp->indexOpState) {
  case(IOS_NOOP): {
    jam();
    // Should never happen, abort
    TcKeyRef * const tcIndxRef = (TcKeyRef *)signal->getDataPtrSend();
    
    tcIndxRef->connectPtr = indexOp->tcIndxReq.senderData;
    tcIndxRef->transId[0] = regApiPtr->transid[0];
    tcIndxRef->transId[1] = regApiPtr->transid[1];
    tcIndxRef->errorCode = 4349;
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
      // Wait for TRANSID_AI
      indexOp->indexOpState = IOS_INDEX_ACCESS_WAIT_FOR_TCKEYCONF;
    }
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
  case(IOS_INDEX_OPERATION): {
    // Should never receive TRANSID_AI in this state!!
    jam();    
    TcKeyRef * const tcIndxRef = (TcKeyRef *)signal->getDataPtrSend();

    tcIndxRef->connectPtr = indexOp->tcIndxReq.senderData;
    tcIndxRef->transId[0] = regApiPtr->transid[0];
    tcIndxRef->transId[1] = regApiPtr->transid[1];
    tcIndxRef->errorCode = 4349;
    tcIndxRef->errorData = regApiPtr->errorData;
    sendSignal(regApiPtr->ndbapiBlockref, GSN_TCINDXREF, signal, 
	       TcKeyRef::SignalLength, JBB);
    return;
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
  sendSignal(apiConnectptr.p->ndbapiBlockref, 
	     GSN_TCROLLBACKREP, signal, TcRollbackRep::SignalLength, JBB);
}

/**
 * Read index table with the index attributes as PK
 */
void Dbtc::readIndexTable(Signal* signal, 
			  ApiConnectRecord* regApiPtr,
			  TcIndexOperation* indexOp) 
{
  Uint32 keyBufSize = 8; // Maximum for key in TCKEYREQ
  Uint32 dataPos = 0;
  TcKeyReq * const tcKeyReq = (TcKeyReq *)signal->getDataPtrSend();
  Uint32 * dataPtr = &tcKeyReq->scanInfo;
  Uint32 tcKeyLength = TcKeyReq::StaticLength;
  Uint32 tcKeyRequestInfo = indexOp->tcIndxReq.requestInfo; 
  AttributeBuffer::DataBufferIterator keyIter;
  Uint32 keyLength = TcKeyReq::getKeyLength(tcKeyRequestInfo);
  TcIndexData* indexData;
  Uint32 transId1 = indexOp->tcIndxReq.transId1;
  Uint32 transId2 = indexOp->tcIndxReq.transId2;

  const Operation_t opType = 
    (Operation_t)TcKeyReq::getOperationType(tcKeyRequestInfo);

  // Find index table
  if ((indexData = c_theIndexes.getPtr(indexOp->tcIndxReq.tableId)) == NULL) {
    jam();
    // Failed to find index record
    TcKeyRef * const tcIndxRef = (TcKeyRef *)signal->getDataPtrSend();

    tcIndxRef->connectPtr = indexOp->tcIndxReq.senderData;
    tcIndxRef->transId[0] = regApiPtr->transid[0];
    tcIndxRef->transId[1] = regApiPtr->transid[1];
    tcIndxRef->errorCode = 4000;    
    // tcIndxRef->errorData = ??; Where to find indexId
    sendSignal(regApiPtr->ndbapiBlockref, GSN_TCINDXREF, signal, 
	       TcKeyRef::SignalLength, JBB);
    return;
  }
  tcKeyReq->transId1 = transId1;
  tcKeyReq->transId2 = transId2;
  tcKeyReq->tableId = indexData->indexId;
  tcKeyLength += MIN(keyLength, keyBufSize);
  tcKeyReq->tableSchemaVersion = indexOp->tcIndxReq.tableSchemaVersion;
  TcKeyReq::setOperationType(tcKeyRequestInfo, 
			     opType == ZREAD ? ZREAD : ZREAD_EX);
  TcKeyReq::setAIInTcKeyReq(tcKeyRequestInfo, 1); // Allways send one AttrInfo
  TcKeyReq::setExecutingTrigger(tcKeyRequestInfo, 0);
  tcKeyReq->senderData = indexOp->indexOpId;
  indexOp->indexOpState = IOS_INDEX_ACCESS;
  regApiPtr->executingIndexOp = regApiPtr->accumulatingIndexOp;
  regApiPtr->accumulatingIndexOp = RNIL;
  regApiPtr->isIndexOp = 2;

  if (ERROR_INSERTED(8037))
  {
    ndbout_c("shifting index version");
    tcKeyReq->tableSchemaVersion = ~(Uint32)indexOp->tcIndxReq.tableSchemaVersion;
  }
  
  Uint32 remainingKey = indexOp->keyInfo.getSize();
  bool moreKeyData = indexOp->keyInfo.first(keyIter);
  // *********** KEYINFO in TCKEYREQ ***********
  while((dataPos < keyBufSize) &&
        (remainingKey-- != 0)) {
    *dataPtr++ = *keyIter.data;
    dataPos++;
    moreKeyData = indexOp->keyInfo.next(keyIter);
  }
  // *********** ATTRINFO in TCKEYREQ ***********  
  tcKeyReq->attrLen = 1; // Primary key is stored as one attribute
  AttributeHeader::init(dataPtr, indexData->primaryKeyPos, 0);
  tcKeyLength++;
  tcKeyReq->requestInfo = tcKeyRequestInfo;

  ndbassert(TcKeyReq::getDirtyFlag(tcKeyRequestInfo) == 0);
  ndbassert(TcKeyReq::getSimpleFlag(tcKeyRequestInfo) == 0);
  EXECUTE_DIRECT(DBTC, GSN_TCKEYREQ, signal, tcKeyLength);
  jamEntry();

  if (unlikely(regApiPtr->apiConnectstate == CS_ABORTING))
  {
    goto err;
  }

  // *********** KEYINFO ***********
  if (moreKeyData) {
    jam();
    // Send KEYINFO sequence
    KeyInfo * const keyInfo =  (KeyInfo *)signal->getDataPtrSend();
    
    keyInfo->connectPtr = indexOp->tcIndxReq.apiConnectPtr;
    keyInfo->transId[0] = transId1;
    keyInfo->transId[1] = transId2;
    dataPtr = (Uint32 *) &keyInfo->keyData;
    dataPos = 0;
    while(remainingKey-- != 0) {// If we have not read complete key
      *dataPtr++ = *keyIter.data;
      dataPos++;
      if (dataPos == KeyInfo::DataLength) {
	// Flush KEYINFO
	EXECUTE_DIRECT(DBTC, GSN_KEYINFO, signal, 
		       KeyInfo::HeaderLength + KeyInfo::DataLength);
        jamEntry();
	if (unlikely(regApiPtr->apiConnectstate == CS_ABORTING))
	{
	  goto err;
	}
	dataPos = 0;
	dataPtr = (Uint32 *) &keyInfo->keyData;
      }       
      moreKeyData = indexOp->keyInfo.next(keyIter);
    }
    if (dataPos != 0) {
      // Flush last KEYINFO
      EXECUTE_DIRECT(DBTC, GSN_KEYINFO, signal,
		     KeyInfo::HeaderLength + dataPos);
      jamEntry();
      if (unlikely(regApiPtr->apiConnectstate == CS_ABORTING))
      {
	goto err;
      }
    }
  }
  
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
  
done:  
  return;
  
err:
  jam();
  goto done;
}

/**
 * Execute the index operation with the result from
 * the index table read as PK
 */
void Dbtc::executeIndexOperation(Signal* signal, 
				 ApiConnectRecord* regApiPtr,
				 TcIndexOperation* indexOp) {
  
  Uint32 keyBufSize = 8; // Maximum for key in TCKEYREQ
  Uint32 attrBufSize = 5;
  Uint32 dataPos = 0;
  TcKeyReq * const tcIndxReq = &indexOp->tcIndxReq;
  TcKeyReq * const tcKeyReq = (TcKeyReq *)signal->getDataPtrSend();
  /*
    Data points to distrGroupHashValue since scanInfo is used to send
    fragment id of receiving fragment
  */
  Uint32 * dataPtr = &tcKeyReq->distrGroupHashValue;
  Uint32 tcKeyLength = TcKeyReq::StaticLength;
  Uint32 tcKeyRequestInfo = tcIndxReq->requestInfo;
  TcIndexData* indexData;
  AttributeBuffer::DataBufferIterator attrIter;
  AttributeBuffer::DataBufferIterator aiIter;
  bool moreKeyData = indexOp->transIdAI.first(aiIter);
      
  // Find index table
  if ((indexData = c_theIndexes.getPtr(tcIndxReq->tableId)) == NULL) {
    jam();
    // Failed to find index record 
    TcKeyRef * const tcIndxRef = (TcKeyRef *)signal->getDataPtrSend();

    tcIndxRef->connectPtr = indexOp->tcIndxReq.senderData;
    tcIndxRef->transId[0] = regApiPtr->transid[0];
    tcIndxRef->transId[1] = regApiPtr->transid[1];
    tcIndxRef->errorCode = 4349;    
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
  indexOp->indexOpState = IOS_INDEX_OPERATION;
  regApiPtr->isIndexOp = 1;
  regApiPtr->executingIndexOp = indexOp->indexOpId;;
  regApiPtr->noIndexOp++; // Increase count

  /*
    Filter out AttributeHeader:s since this should not be in key.
    Also filter out fragment id from primary key and handle that
    separately by setting it as Distribution Key and set indicator.
  */

  AttributeHeader* attrHeader = (AttributeHeader *) aiIter.data;
    
  Uint32 headerSize = attrHeader->getHeaderSize();
  Uint32 keySize = attrHeader->getDataSize() - 1;
  TcKeyReq::setKeyLength(tcKeyRequestInfo, keySize);
  // Skip header
  if (headerSize == 1) {
    jam();
    moreKeyData = indexOp->transIdAI.next(aiIter);
  } else {
    jam();
    moreKeyData = indexOp->transIdAI.next(aiIter, headerSize - 1);
  }//if
  tcKeyReq->scanInfo = *aiIter.data; //Fragment Id
  moreKeyData = indexOp->transIdAI.next(aiIter);
  TcKeyReq::setDistributionKeyFlag(tcKeyRequestInfo, 1U);
  while(// If we have not read complete key
	(keySize != 0) &&
	(dataPos < keyBufSize)) {
    *dataPtr++ = *aiIter.data;
    dataPos++;
    keySize--;
    moreKeyData = indexOp->transIdAI.next(aiIter);
  }
  tcKeyLength += dataPos;

  Uint32 attributesLength = indexOp->attrInfo.getSize();
  if (attributesLength <= attrBufSize) {
    jam();
    // ATTRINFO fits in TCKEYREQ
    // Pack ATTRINFO IN TCKEYREQ
    TcKeyReq::setAIInTcKeyReq(tcKeyRequestInfo, indexOp->attrInfo.getSize());
    // Insert IndxAttrInfo
    for(bool moreAttrData = indexOp->attrInfo.first(attrIter);
        moreAttrData;
        moreAttrData = indexOp->attrInfo.next(attrIter)) {      
      *dataPtr++ = *attrIter.data;
    }
    tcKeyLength += attributesLength;
  } else {
    jam();
    // No ATTRINFO in TCKEYREQ
    TcKeyReq::setAIInTcKeyReq(tcKeyRequestInfo, 0);
  }

  TcKeyReq::setCommitFlag(tcKeyRequestInfo, 0);
  TcKeyReq::setExecuteFlag(tcKeyRequestInfo, 0);
  TcKeyReq::setExecutingTrigger(tcKeyRequestInfo, 0);
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
  regApiPtr->currSavePointId = tmp.p->savePointId;
  EXECUTE_DIRECT(DBTC, GSN_TCKEYREQ, signal, tcKeyLength);
  jamEntry();
  
  if (unlikely(regApiPtr->apiConnectstate == CS_ABORTING))
  {
    jam();
    return;
  }

  regApiPtr->currSavePointId = currSavePointId;
  
  // *********** KEYINFO ***********
  if (moreKeyData) {
    jam();
    // Send KEYINFO sequence
    KeyInfo * const keyInfo =  (KeyInfo *)signal->getDataPtrSend();
    
    keyInfo->connectPtr = indexOp->tcIndxReq.apiConnectPtr;
    keyInfo->transId[0] = regApiPtr->transid[0];
    keyInfo->transId[1] = regApiPtr->transid[1];
    dataPtr = (Uint32 *) &keyInfo->keyData;
    dataPos = 0;
    // Pack any part of a key attribute that did no fit TCKEYREQ
    while(keySize-- != 0) {// If we have not read complete key
      *dataPtr++ = *aiIter.data;
      dataPos++;
      if (dataPos == KeyInfo::DataLength) {
	// Flush KEYINFO
	EXECUTE_DIRECT(DBTC, GSN_KEYINFO, signal, 
		       KeyInfo::HeaderLength + KeyInfo::DataLength);
        jamEntry();

	if (unlikely(regApiPtr->apiConnectstate == CS_ABORTING))
	{
	  jam();
	  return;
	}

	dataPos = 0;
	dataPtr = (Uint32 *) &keyInfo->keyData;
      }
      moreKeyData = indexOp->transIdAI.next(aiIter);
    }
    if (dataPos != 0) {
      // Flush last KEYINFO
      EXECUTE_DIRECT(DBTC, GSN_KEYINFO, signal,
		     KeyInfo::HeaderLength + dataPos);
      jamEntry();

      if (unlikely(regApiPtr->apiConnectstate == CS_ABORTING))
      {
	jam();
	return;
      }
    }
  }
  
  // *********** ATTRINFO ***********
  if (attributesLength > attrBufSize) {
    jam();
    // No ATTRINFO in TcKeyReq
    TcKeyReq::setAIInTcKeyReq(tcKeyReq->requestInfo, 0);
    // Send ATTRINFO sequence
    AttrInfo * const attrInfo =  (AttrInfo *)signal->getDataPtrSend();
    Uint32 attrInfoPos = 0;
    
    attrInfo->connectPtr = indexOp->tcIndxReq.apiConnectPtr;
    attrInfo->transId[0] = regApiPtr->transid[0];
    attrInfo->transId[1] = regApiPtr->transid[1];
    dataPtr = (Uint32 *) &attrInfo->attrData;

 
    // Insert attribute values (insert key values of primary table)
    for(bool moreAttrData = indexOp->attrInfo.first(attrIter);
        moreAttrData;
        moreAttrData = indexOp->attrInfo.next(attrIter)) {      
      *dataPtr++ = *attrIter.data;
      attrInfoPos++;
      if (attrInfoPos == AttrInfo::DataLength) {
        // Flush ATTRINFO
        EXECUTE_DIRECT(DBTC, GSN_ATTRINFO, signal,
		       AttrInfo::HeaderLength + AttrInfo::DataLength);
        jamEntry();

	if (unlikely(regApiPtr->apiConnectstate == CS_ABORTING))
	{
	  jam();
	  return;
	}

        attrInfoPos = 0;
	dataPtr = (Uint32 *) &attrInfo->attrData;
      }
    }
    if (attrInfoPos != 0) {
      // Send last ATTRINFO
      EXECUTE_DIRECT(DBTC, GSN_ATTRINFO, signal, 
		     AttrInfo::HeaderLength + attrInfoPos);
      jamEntry();
    }
  }
}

bool Dbtc::seizeIndexOperation(ApiConnectRecord* regApiPtr,
			       TcIndexOperationPtr& indexOpPtr)
{
  if (regApiPtr->theSeizedIndexOperations.seize(indexOpPtr))
  {
    ndbassert(indexOpPtr.p->expectedKeyInfo == 0);
    ndbassert(indexOpPtr.p->keyInfo.getSize() == 0);
    ndbassert(indexOpPtr.p->expectedAttrInfo == 0);
    ndbassert(indexOpPtr.p->attrInfo.getSize() == 0);
    ndbassert(indexOpPtr.p->expectedTransIdAI == 0);
    ndbassert(indexOpPtr.p->transIdAI.getSize() == 0);
    return true;
  }
  
  return false;
}

void Dbtc::releaseIndexOperation(ApiConnectRecord* regApiPtr,
				 TcIndexOperation* indexOp)
{
  indexOp->indexOpState = IOS_NOOP;
  indexOp->expectedKeyInfo = 0;
  indexOp->keyInfo.release();
  indexOp->expectedAttrInfo = 0;
  indexOp->attrInfo.release();
  indexOp->expectedTransIdAI = 0;
  indexOp->transIdAI.release();
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
    indexOp->expectedKeyInfo = 0;
    indexOp->keyInfo.release();
    indexOp->expectedAttrInfo = 0;
    indexOp->attrInfo.release();
    indexOp->expectedTransIdAI = 0;
    indexOp->transIdAI.release();
    regApiPtr->theSeizedIndexOperations.next(seizedIndexOpPtr);    
  }
  regApiPtr->theSeizedIndexOperations.release();
}

void Dbtc::saveTriggeringOpState(Signal* signal, TcConnectRecord* trigOp)
{
  LqhKeyConf * lqhKeyConf = (LqhKeyConf *)signal->getDataPtr();
  copyFromToLen((UintR*)lqhKeyConf,
		&trigOp->savedState[0],
                LqhKeyConf::SignalLength);  
}

void Dbtc::continueTriggeringOp(Signal* signal, TcConnectRecord* trigOp)
{
  LqhKeyConf * lqhKeyConf = (LqhKeyConf *)signal->getDataPtr();
  copyFromToLen(&trigOp->savedState[0],
                (UintR*)lqhKeyConf,
		LqhKeyConf::SignalLength);

  lqhKeyConf->noFiredTriggers = 0;
  trigOp->noReceivedTriggers = 0;

  // All triggers executed successfully, continue operation
  execLQHKEYCONF(signal);
}

void Dbtc::scheduleFiredTrigger(ApiConnectRecordPtr* transPtr,
                                TcConnectRecordPtr* opPtr)
{
  // Set initial values for trigger fireing operation
  opPtr->p->triggerExecutionCount++;

  // Insert fired trigger in execution queue
  transPtr->p->theFiredTriggers.add(opPtr->p->accumulatingTriggerData);
  opPtr->p->accumulatingTriggerData.i = RNIL;
  opPtr->p->accumulatingTriggerData.p = NULL;
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
        (regApiPtr->apiConnectstate == CS_START_COMMITTING)) {
      jam();
      regApiPtr->theFiredTriggers.first(trigPtr);
      while (trigPtr.i != RNIL) {
        jam();
        // Execute all ready triggers in parallel
        opPtr.i = trigPtr.p->fireingOperation;
        ptrCheckGuard(opPtr, ctcConnectFilesize, localTcConnectRecord);
	FiredTriggerPtr nextTrigPtr = trigPtr;
	regApiPtr->theFiredTriggers.next(nextTrigPtr);
        if (opPtr.p->noReceivedTriggers == opPtr.p->noFiredTriggers) {
          jam();
          // Fireing operation is ready to have a trigger executing
          executeTrigger(signal, trigPtr.p, transPtr, &opPtr);
          // Should allow for interleaving here by sending a CONTINUEB and 
	  // return
          // Release trigger records
	  AttributeBuffer::DataBufferPool & pool = c_theAttributeBufferPool;
	  LocalDataBuffer<11> tmp1(pool, trigPtr.p->keyValues);
	  tmp1.release();
	  LocalDataBuffer<11> tmp2(pool, trigPtr.p->beforeValues);
	  tmp2.release();
	  LocalDataBuffer<11> tmp3(pool, trigPtr.p->afterValues);
	  tmp3.release();
          regApiPtr->theFiredTriggers.release(trigPtr);
        }
	trigPtr = nextTrigPtr;
      }
      return;
    // No more triggers, continue transaction after last executed trigger has
    // reurned (in execLQHKEYCONF or execLQHKEYREF)
    } else {
      // Wait until transaction is ready to execute a trigger
      jam();
      if (!regApiPtr->triggerPending) {
        jam();
        regApiPtr->triggerPending = true;
        signal->theData[0] = TcContinueB::TRIGGER_PENDING;
        signal->theData[1] = transPtr->i;
        sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBB);
      }
      // else  
      // We are already waiting for a pending trigger (CONTINUEB)
    }
  }
}

void Dbtc::executeTrigger(Signal* signal,
                          TcFiredTriggerData* firedTriggerData,
                          ApiConnectRecordPtr* transPtr,
                          TcConnectRecordPtr* opPtr)
{
  TcDefinedTriggerData* definedTriggerData;

  if ((definedTriggerData = 
       c_theDefinedTriggers.getPtr(firedTriggerData->triggerId)) 
      != NULL) {
    switch(definedTriggerData->triggerType) {
    case(TriggerType::SECONDARY_INDEX):
      jam();
      executeIndexTrigger(signal, definedTriggerData, firedTriggerData, 
                          transPtr, opPtr);
      break;
    default:
      ndbrequire(false);
    }
  }
}

void Dbtc::executeIndexTrigger(Signal* signal,
                               TcDefinedTriggerData* definedTriggerData,
                               TcFiredTriggerData* firedTriggerData,
                               ApiConnectRecordPtr* transPtr,
                               TcConnectRecordPtr* opPtr)
{
  TcIndexData* indexData;

  indexData = c_theIndexes.getPtr(definedTriggerData->indexId);
  ndbassert(indexData != NULL);

  switch (definedTriggerData->triggerEvent) {
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
    deleteFromIndexTable(signal, firedTriggerData, transPtr, opPtr, 
			 indexData, true); // Hold the triggering operation
    insertIntoIndexTable(signal, firedTriggerData, transPtr, opPtr, indexData);
    break;
  }
  default:
    ndbrequire(false);
  }
}

void Dbtc::releaseFiredTriggerData(DLFifoList<TcFiredTriggerData>* triggers)
{
  FiredTriggerPtr trigPtr;

  triggers->first(trigPtr);
  while (trigPtr.i != RNIL) {
    jam();
    // Release trigger records

    AttributeBuffer::DataBufferPool & pool = c_theAttributeBufferPool;
    LocalDataBuffer<11> tmp1(pool, trigPtr.p->keyValues);
    tmp1.release();
    LocalDataBuffer<11> tmp2(pool, trigPtr.p->beforeValues);
    tmp2.release();
    LocalDataBuffer<11> tmp3(pool, trigPtr.p->afterValues);
    tmp3.release();
    
    triggers->next(trigPtr);
  }
  triggers->release();
}

void Dbtc::insertIntoIndexTable(Signal* signal, 
                                TcFiredTriggerData* firedTriggerData, 
                                ApiConnectRecordPtr* transPtr,
                                TcConnectRecordPtr* opPtr,
                                TcIndexData* indexData,
                                bool holdOperation)
{
  ApiConnectRecord* regApiPtr = transPtr->p;
  TcConnectRecord* opRecord = opPtr->p;
  TcKeyReq * const tcKeyReq =  (TcKeyReq *)signal->getDataPtrSend();
  Uint32 tcKeyRequestInfo = 0;
  Uint32 tcKeyLength = TcKeyReq::StaticLength;
  TableRecordPtr indexTabPtr;
  AttributeBuffer::DataBufferIterator iter;
  Uint32 attrId = 0;
  Uint32 keyLength = 0;
  Uint32 totalPrimaryKeyLength = 1; // fragment length
  Uint32 hops;

  indexTabPtr.i = indexData->indexId;
  ptrCheckGuard(indexTabPtr, ctabrecFilesize, tableRecord);
  tcKeyReq->apiConnectPtr = transPtr->i;
  tcKeyReq->senderData = opPtr->i;
  if (holdOperation) {
    jam();
    opRecord->triggerExecutionCount++;
  }//if
  // Calculate key length and renumber attribute id:s
  AttributeBuffer::DataBufferPool & pool = c_theAttributeBufferPool;
  LocalDataBuffer<11> afterValues(pool, firedTriggerData->afterValues);
  bool skipNull = false;
  for(bool moreKeyAttrs = afterValues.first(iter); moreKeyAttrs; attrId++) {
    jam();
    AttributeHeader* attrHeader = (AttributeHeader *) iter.data;

    // Filter out NULL valued attributes
    if (attrHeader->isNULL()) {
      skipNull = true;
      break;
    }
    attrHeader->setAttributeId(attrId);      
    keyLength += attrHeader->getDataSize();
    hops = attrHeader->getHeaderSize() + attrHeader->getDataSize();
    moreKeyAttrs = afterValues.next(iter, hops);
  }
  if (skipNull) {
    jam();
    opRecord->triggerExecutionCount--;
    if (opRecord->triggerExecutionCount == 0) {
      /*
	We have completed current trigger execution
	Continue triggering operation
      */
      jam();
      continueTriggeringOp(signal, opRecord);	
    }//if
    return;
  }//if

  // Calculate total length of primary key to be stored in index table
  LocalDataBuffer<11> keyValues(pool, firedTriggerData->keyValues);
  for(bool moreAttrData = keyValues.first(iter); moreAttrData; ) {
    jam();
    AttributeHeader* attrHeader = (AttributeHeader *) iter.data;
    
    totalPrimaryKeyLength += attrHeader->getDataSize();
    hops = attrHeader->getHeaderSize() + attrHeader->getDataSize();
    moreAttrData = keyValues.next(iter, hops);
  }
  AttributeHeader pkAttrHeader(attrId, totalPrimaryKeyLength << 2);
  Uint32 attributesLength = afterValues.getSize() + 
    pkAttrHeader.getHeaderSize() + pkAttrHeader.getDataSize();
  
  TcKeyReq::setKeyLength(tcKeyRequestInfo, keyLength);
  tcKeyReq->attrLen = attributesLength;
  tcKeyReq->tableId = indexData->indexId;
  TcKeyReq::setOperationType(tcKeyRequestInfo, ZINSERT);
  TcKeyReq::setExecutingTrigger(tcKeyRequestInfo, true);
  tcKeyReq->tableSchemaVersion = indexTabPtr.p->currentSchemaVersion;
  tcKeyReq->transId1 = regApiPtr->transid[0];
  tcKeyReq->transId2 = regApiPtr->transid[1];
  Uint32 * dataPtr = &tcKeyReq->scanInfo;
  // Write first part of key in TCKEYREQ
  Uint32 keyBufSize = 8; // Maximum for key in TCKEYREQ
  Uint32 attrBufSize = 5; // Maximum for key in TCKEYREQ
  Uint32 dataPos = 0;
  // Filter out AttributeHeader:s since this should no be in key
  bool moreKeyData = afterValues.first(iter);
  Uint32 headerSize = 0, keyAttrSize = 0, dataSize = 0, headAndData = 0;

  while (moreKeyData && (dataPos < keyBufSize)) {
    /*
     * If we have not read complete key
     * and it fits in the signal
     */
    jam();
    AttributeHeader* attrHeader = (AttributeHeader *) iter.data;
    
    headerSize = attrHeader->getHeaderSize();
    keyAttrSize = attrHeader->getDataSize();
    headAndData = headerSize + attrHeader->getDataSize();
    // Skip header
    if (headerSize == 1) {
      jam();
      moreKeyData = afterValues.next(iter);
    } else {
      jam();
      moreKeyData = afterValues.next(iter, headerSize - 1);
    }//if
    while((keyAttrSize != 0) && (dataPos < keyBufSize)) {
      // If we have not read complete key
      jam();
      *dataPtr++ = *iter.data;
      dataPos++;
      keyAttrSize--;
      moreKeyData = afterValues.next(iter);
    }
    if (keyAttrSize != 0) {
      jam();
      break;
    }//if
  }

  tcKeyLength += dataPos;
  /*
    Size of attrinfo is unique index attributes one by one, header for each
    of them (all contained in the afterValues data structure), plus a header,
    the primary key (compacted) and the fragment id before the primary key
  */
  if (attributesLength <= attrBufSize) {
    jam();
    // ATTRINFO fits in TCKEYREQ
    // Pack ATTRINFO IN TCKEYREQ as one attribute
    TcKeyReq::setAIInTcKeyReq(tcKeyRequestInfo, attributesLength);
    bool moreAttrData;
    // Insert primary key attributes (insert after values of primary table)
    for(moreAttrData = afterValues.first(iter);
	moreAttrData;
	moreAttrData = afterValues.next(iter)) {      
      *dataPtr++ = *iter.data;
    }
    // Insert attribute values (insert key values of primary table)
    // as one attribute
    pkAttrHeader.insertHeader(dataPtr);
    dataPtr += pkAttrHeader.getHeaderSize();
    /*
      Insert fragment id before primary key as part of reference to tuple
    */
    *dataPtr++ = firedTriggerData->fragId;
    moreAttrData = keyValues.first(iter);
    while(moreAttrData) {
      jam();
      AttributeHeader* attrHeader = (AttributeHeader *) iter.data;
      
      headerSize = attrHeader->getHeaderSize();
      dataSize = attrHeader->getDataSize();
      // Skip header
      if (headerSize == 1) {
        jam();
        moreAttrData = keyValues.next(iter);
      } else {
        jam();
        moreAttrData = keyValues.next(iter, headerSize - 1);
      }//if
      // Copy attribute data
      while(dataSize-- != 0) {
	*dataPtr++ = *iter.data;
	moreAttrData = keyValues.next(iter);
      }
    }
    tcKeyLength += attributesLength;
  } else {
    jam();
    // No ATTRINFO in TCKEYREQ
    TcKeyReq::setAIInTcKeyReq(tcKeyRequestInfo, 0);
  }
  tcKeyReq->requestInfo = tcKeyRequestInfo;

  /**
   * Fix savepoint id -
   *   fix so that insert has same savepoint id as triggering operation
   */
  const Uint32 currSavePointId = regApiPtr->currSavePointId;
  regApiPtr->currSavePointId = opRecord->savePointId;
  EXECUTE_DIRECT(DBTC, GSN_TCKEYREQ, signal, tcKeyLength);
  jamEntry();

  if (unlikely(regApiPtr->apiConnectstate == CS_ABORTING))
  {
    jam();
    return;
  }

  regApiPtr->currSavePointId = currSavePointId;
  tcConnectptr.p->currentIndexId = indexData->indexId;

  // *********** KEYINFO ***********
  if (moreKeyData) {
    jam();
    // Send KEYINFO sequence
    KeyInfo * const keyInfo =  (KeyInfo *)signal->getDataPtrSend();
    
    keyInfo->connectPtr = transPtr->i;
    keyInfo->transId[0] = regApiPtr->transid[0];
    keyInfo->transId[1] = regApiPtr->transid[1];
    dataPtr = (Uint32 *) &keyInfo->keyData;
    dataPos = 0;
    // Pack any part of a key attribute that did no fit TCKEYREQ
    while((keyAttrSize != 0) && (dataPos < KeyInfo::DataLength)) {
      // If we have not read complete key
      *dataPtr++ = *iter.data;
      dataPos++;
      keyAttrSize--;
      if (dataPos == KeyInfo::DataLength) {
        jam();
	// Flush KEYINFO
#if INTERNAL_TRIGGER_TCKEYREQ_JBA
	sendSignal(reference(), GSN_KEYINFO, signal, 
		   KeyInfo::HeaderLength + KeyInfo::DataLength, JBA);
#else
	EXECUTE_DIRECT(DBTC, GSN_KEYINFO, signal,
		       KeyInfo::HeaderLength + KeyInfo::DataLength);
        jamEntry();
#endif
	if (unlikely(regApiPtr->apiConnectstate == CS_ABORTING))
	{
	  jam();
	  return;
	}
	
	dataPtr = (Uint32 *) &keyInfo->keyData;
	dataPos = 0;
      }
      moreKeyData = afterValues.next(iter);
    }
    
    while(moreKeyData) {
      jam();
      AttributeHeader* attrHeader = (AttributeHeader *) iter.data;
      
      headerSize = attrHeader->getHeaderSize();
      keyAttrSize = attrHeader->getDataSize();
      headAndData = headerSize + attrHeader->getDataSize();
      // Skip header
      if (headerSize == 1) {
        jam();
        moreKeyData = afterValues.next(iter);
      } else {
        jam();
        moreKeyData = afterValues.next(iter, headerSize - 1);
      }//if
      while (keyAttrSize-- != 0) {
        *dataPtr++ = *iter.data;
        dataPos++;
        if (dataPos == KeyInfo::DataLength) {
          jam();
          // Flush KEYINFO
#if INTERNAL_TRIGGER_TCKEYREQ_JBA
	  sendSignal(reference(), GSN_KEYINFO, signal, 
		     KeyInfo::HeaderLength + KeyInfo::DataLength, JBA);
#else
          EXECUTE_DIRECT(DBTC, GSN_KEYINFO, signal,
			 KeyInfo::HeaderLength + KeyInfo::DataLength);
          jamEntry();
#endif

	  if (unlikely(regApiPtr->apiConnectstate == CS_ABORTING))
	  {
	    jam();
	    return;
	  }

	  dataPtr = (Uint32 *) &keyInfo->keyData;	  
          dataPos = 0;
        }       
        moreKeyData = afterValues.next(iter);
      }
    }
    if (dataPos != 0) {
      jam();
      // Flush last KEYINFO
#if INTERNAL_TRIGGER_TCKEYREQ_JBA
      sendSignal(reference(), GSN_KEYINFO, signal, 
		 KeyInfo::HeaderLength + dataPos, JBA);
#else
      EXECUTE_DIRECT(DBTC, GSN_KEYINFO, signal,
		     KeyInfo::HeaderLength + dataPos);
      jamEntry();
#endif
      if (unlikely(regApiPtr->apiConnectstate == CS_ABORTING))
      {
	jam();
	return;
      }
    }
  }
  
  // *********** ATTRINFO ***********
  if (attributesLength > attrBufSize) {
    jam();
    // No ATTRINFO in TcKeyReq
    TcKeyReq::setAIInTcKeyReq(tcKeyReq->requestInfo, 0);
    // Send ATTRINFO sequence
    AttrInfo * const attrInfo =  (AttrInfo *)signal->getDataPtrSend();
    Uint32 attrInfoPos = 0;
    
    attrInfo->connectPtr = transPtr->i;
    attrInfo->transId[0] = regApiPtr->transid[0];
    attrInfo->transId[1] = regApiPtr->transid[1];
    dataPtr = (Uint32 *) &attrInfo->attrData;
    
    bool moreAttrData;
    // Insert primary key attributes (insert after values of primary table)
    for(moreAttrData = afterValues.first(iter);
	moreAttrData;
	moreAttrData = afterValues.next(iter)) {      
      *dataPtr++ = *iter.data;
      attrInfoPos++;
      if (attrInfoPos == AttrInfo::DataLength) {
        jam();
	// Flush ATTRINFO
#if INTERNAL_TRIGGER_TCKEYREQ_JBA
	sendSignal(reference(), GSN_ATTRINFO, signal, 
		   AttrInfo::HeaderLength + AttrInfo::DataLength, JBA);
#else
	EXECUTE_DIRECT(DBTC, GSN_ATTRINFO, signal,
		       AttrInfo::HeaderLength + AttrInfo::DataLength);
        jamEntry();
#endif
	if (unlikely(regApiPtr->apiConnectstate == CS_ABORTING))
	{
	  jam();
	  return;
	}

	dataPtr = (Uint32 *) &attrInfo->attrData;
	attrInfoPos = 0;
      }
    }
    // Insert attribute values (insert key values of primary table)
    // as one attribute
    pkAttrHeader.insertHeader(dataPtr);
    dataPtr += pkAttrHeader.getHeaderSize();
    attrInfoPos += pkAttrHeader.getHeaderSize();
    /*
      Add fragment id before primary key
      TODO: This code really needs to be made into a long signal
      to remove this messy code.
    */
    if (attrInfoPos == AttrInfo::DataLength)
    {
      jam();
      // Flush ATTRINFO
#if INTERNAL_TRIGGER_TCKEYREQ_JBA
      sendSignal(reference(), GSN_ATTRINFO, signal, 
                 AttrInfo::HeaderLength + AttrInfo::DataLength, JBA);
#else
      EXECUTE_DIRECT(DBTC, GSN_ATTRINFO, signal,
                     AttrInfo::HeaderLength + AttrInfo::DataLength);
      jamEntry();
#endif
      dataPtr = (Uint32 *) &attrInfo->attrData;	  
      attrInfoPos = 0;
    }
    attrInfoPos++;
    *dataPtr++ = firedTriggerData->fragId;

    moreAttrData = keyValues.first(iter);
    while(moreAttrData) {
      jam();
      AttributeHeader* attrHeader = (AttributeHeader *) iter.data;
      
      headerSize = attrHeader->getHeaderSize();
      dataSize = attrHeader->getDataSize();
      // Skip header
      if (headerSize == 1) {
        jam();
        moreAttrData = keyValues.next(iter);
      } else {
        jam();
        moreAttrData = keyValues.next(iter, headerSize - 1);
      }//if
      while(dataSize-- != 0) { // If we have not read complete key
        if (attrInfoPos == AttrInfo::DataLength) {
          jam();
          // Flush ATTRINFO
#if INTERNAL_TRIGGER_TCKEYREQ_JBA
	  sendSignal(reference(), GSN_ATTRINFO, signal, 
		     AttrInfo::HeaderLength + AttrInfo::DataLength, JBA);
#else
	  EXECUTE_DIRECT(DBTC, GSN_ATTRINFO, signal,
			 AttrInfo::HeaderLength + AttrInfo::DataLength);
          jamEntry();
#endif
	  if (unlikely(regApiPtr->apiConnectstate == CS_ABORTING))
	  {
	    jam();
	    return;
	  }
	  
	  dataPtr = (Uint32 *) &attrInfo->attrData;	  
          attrInfoPos = 0;
        }       
        *dataPtr++ = *iter.data;
        attrInfoPos++;
        moreAttrData = keyValues.next(iter);
      }
    }
    if (attrInfoPos != 0) {
      jam();
      // Flush last ATTRINFO
#if INTERNAL_TRIGGER_TCKEYREQ_JBA
      sendSignal(reference(), GSN_ATTRINFO, signal, 
		 AttrInfo::HeaderLength + attrInfoPos, JBA);
#else
      EXECUTE_DIRECT(DBTC, GSN_ATTRINFO, signal,
		     AttrInfo::HeaderLength + attrInfoPos);
      jamEntry();
#endif
    }
  }
}

void Dbtc::deleteFromIndexTable(Signal* signal, 
                                TcFiredTriggerData* firedTriggerData, 
                                ApiConnectRecordPtr* transPtr,
                                TcConnectRecordPtr* opPtr,
                                TcIndexData* indexData,
                                bool holdOperation)
{
  ApiConnectRecord* regApiPtr = transPtr->p;
  TcConnectRecord* opRecord = opPtr->p;
  TcKeyReq * const tcKeyReq =  (TcKeyReq *)signal->getDataPtrSend();
  Uint32 tcKeyRequestInfo = 0;
  Uint32 tcKeyLength = 12; // Static length
  TableRecordPtr indexTabPtr;
  AttributeBuffer::DataBufferIterator iter;
  Uint32 attrId = 0;
  Uint32 keyLength = 0;
  Uint32 hops;

  indexTabPtr.i = indexData->indexId;
  ptrCheckGuard(indexTabPtr, ctabrecFilesize, tableRecord);
  tcKeyReq->apiConnectPtr = transPtr->i;
  tcKeyReq->senderData = opPtr->i;
  if (holdOperation) {
    jam();
    opRecord->triggerExecutionCount++;
  }//if
  // Calculate key length and renumber attribute id:s
  AttributeBuffer::DataBufferPool & pool = c_theAttributeBufferPool;
  LocalDataBuffer<11> beforeValues(pool, firedTriggerData->beforeValues);
  bool skipNull = false;
  for(bool moreKeyAttrs = beforeValues.first(iter);
      (moreKeyAttrs);
      attrId++) {
    jam();
    AttributeHeader* attrHeader = (AttributeHeader *) iter.data;
    
    // Filter out NULL valued attributes
    if (attrHeader->isNULL()) {
      skipNull = true;
      break;
    }
    attrHeader->setAttributeId(attrId);      
    keyLength += attrHeader->getDataSize();
    hops = attrHeader->getHeaderSize() + attrHeader->getDataSize();
    moreKeyAttrs = beforeValues.next(iter, hops);
  }

  if (skipNull) {
    jam();
    opRecord->triggerExecutionCount--;
    if (opRecord->triggerExecutionCount == 0) {
      /*
        We have completed current trigger execution
	Continue triggering operation
      */
      jam();
      continueTriggeringOp(signal, opRecord);	
    }//if
    return;
  }//if

  TcKeyReq::setKeyLength(tcKeyRequestInfo, keyLength);
  tcKeyReq->attrLen = 0;
  tcKeyReq->tableId = indexData->indexId;
  TcKeyReq::setOperationType(tcKeyRequestInfo, ZDELETE);
  TcKeyReq::setExecutingTrigger(tcKeyRequestInfo, true);
  tcKeyReq->tableSchemaVersion = indexTabPtr.p->currentSchemaVersion;
  tcKeyReq->transId1 = regApiPtr->transid[0];
  tcKeyReq->transId2 = regApiPtr->transid[1];
  Uint32 * dataPtr = &tcKeyReq->scanInfo;
  // Write first part of key in TCKEYREQ
  Uint32 keyBufSize = 8; // Maximum for key in TCKEYREQ
  Uint32 dataPos = 0;
  // Filter out AttributeHeader:s since this should no be in key
  bool moreKeyData = beforeValues.first(iter);
  Uint32 headerSize = 0, keyAttrSize = 0, headAndData = 0;

  while (moreKeyData && 
         (dataPos < keyBufSize)) {
    /*
    If we have not read complete key
    and it fits in the signal
    */
    jam();
    AttributeHeader* attrHeader = (AttributeHeader *) iter.data;
    
    headerSize = attrHeader->getHeaderSize();
    keyAttrSize = attrHeader->getDataSize();
    headAndData = headerSize + attrHeader->getDataSize();
    // Skip header
    if (headerSize == 1) {
      jam();
      moreKeyData = beforeValues.next(iter);
    } else {
      jam();
      moreKeyData = beforeValues.next(iter, headerSize - 1);
    }//if
    while((keyAttrSize != 0) && 
          (dataPos < keyBufSize)) {
      // If we have not read complete key
      jam();
      *dataPtr++ = *iter.data;
      dataPos++;
      keyAttrSize--;
      moreKeyData = beforeValues.next(iter);
    }
    if (keyAttrSize != 0) {
      jam();
      break;
    }//if
  }

  tcKeyLength += dataPos;
  tcKeyReq->requestInfo = tcKeyRequestInfo;

  /**
   * Fix savepoint id -
   *   fix so that delete has same savepoint id as triggering operation
   */
  const Uint32 currSavePointId = regApiPtr->currSavePointId;
  regApiPtr->currSavePointId = opRecord->savePointId;
  EXECUTE_DIRECT(DBTC, GSN_TCKEYREQ, signal, tcKeyLength);
  jamEntry();

  if (unlikely(regApiPtr->apiConnectstate == CS_ABORTING))
  {
    jam();
    return;
  }

  regApiPtr->currSavePointId = currSavePointId;
  tcConnectptr.p->currentIndexId = indexData->indexId;

  // *********** KEYINFO ***********
  if (moreKeyData) {
    jam();
    // Send KEYINFO sequence
    KeyInfo * const keyInfo =  (KeyInfo *)signal->getDataPtrSend();
    
    keyInfo->connectPtr = transPtr->i;
    keyInfo->transId[0] = regApiPtr->transid[0];
    keyInfo->transId[1] = regApiPtr->transid[1];
    dataPtr = (Uint32 *) &keyInfo->keyData;
    dataPos = 0;
    // Pack any part of a key attribute that did no fit TCKEYREQ
    while((keyAttrSize != 0) &&
	  (dataPos < KeyInfo::DataLength)) {
      // If we have not read complete key
      *dataPtr++ = *iter.data;
      dataPos++;
      keyAttrSize--;
      if (dataPos == KeyInfo::DataLength) {
        jam();
	// Flush KEYINFO
#if INTERNAL_TRIGGER_TCKEYREQ_JBA
	sendSignal(reference(), GSN_KEYINFO, signal, 
		   KeyInfo::HeaderLength + KeyInfo::DataLength, JBA);
#else
	EXECUTE_DIRECT(DBTC, GSN_KEYINFO, signal,
		       KeyInfo::HeaderLength + KeyInfo::DataLength);
        jamEntry();
#endif
	if (unlikely(regApiPtr->apiConnectstate == CS_ABORTING))
	{
	  jam();
	  return;
	}

	dataPtr = (Uint32 *) &keyInfo->keyData;
	dataPos = 0;
      }
      moreKeyData = beforeValues.next(iter);
    }
    
    while(moreKeyData) {
      jam();
      AttributeHeader* attrHeader = (AttributeHeader *) iter.data;
      
      headerSize = attrHeader->getHeaderSize();
      keyAttrSize = attrHeader->getDataSize();
      headAndData = headerSize + attrHeader->getDataSize();
      // Skip header
      if (headerSize == 1) {
        jam();
        moreKeyData = beforeValues.next(iter);
      } else {
        jam();
        moreKeyData = beforeValues.next(iter, 
							  headerSize - 1);
      }//if
      while (keyAttrSize-- != 0) {
        *dataPtr++ = *iter.data;
        dataPos++;
        if (dataPos == KeyInfo::DataLength) {
          jam();
          // Flush KEYINFO
#if INTERNAL_TRIGGER_TCKEYREQ_JBA
	  sendSignal(reference(), GSN_KEYINFO, signal, 
		     KeyInfo::HeaderLength + KeyInfo::DataLength, JBA);
#else
          EXECUTE_DIRECT(DBTC, GSN_KEYINFO, signal,
			 KeyInfo::HeaderLength + KeyInfo::DataLength);
          jamEntry();
#endif
	  if (unlikely(regApiPtr->apiConnectstate == CS_ABORTING))
	  {
	    jam();
	    return;
	  }

	  dataPtr = (Uint32 *) &keyInfo->keyData;	  
          dataPos = 0;
        }       
        moreKeyData = beforeValues.next(iter);
      }
    }
    if (dataPos != 0) {
      jam();
      // Flush last KEYINFO
#if INTERNAL_TRIGGER_TCKEYREQ_JBA
      sendSignal(reference(), GSN_KEYINFO, signal, 
		 KeyInfo::HeaderLength + dataPos, JBA);
#else
      EXECUTE_DIRECT(DBTC, GSN_KEYINFO, signal,
		     KeyInfo::HeaderLength + dataPos);
      jamEntry();
#endif
    }
  }
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

void
Dbtc::execROUTE_ORD(Signal* signal)
{
  jamEntry();
  if(!assembleFragments(signal)){
    jam();
    return;
  }

  RouteOrd* ord = (RouteOrd*)signal->getDataPtr();
  Uint32 dstRef = ord->dstRef;
  Uint32 srcRef = ord->srcRef;
  Uint32 gsn = ord->gsn;

  if (likely(getNodeInfo(refToNode(dstRef)).m_connected))
  {
    jam();
    Uint32 secCount = signal->getNoOfSections();
    SegmentedSectionPtr ptr[3];
    ndbrequire(secCount >= 1 && secCount <= 3);

    jamLine(secCount);
    for (Uint32 i = 0; i<secCount; i++)
      signal->getSection(ptr[i], i);

    /**
     * Put section 0 in signal->theData
     */
    ndbrequire(ptr[0].sz <= 25);
    copy(signal->theData, ptr[0]);

    signal->header.m_noOfSections = 0;
    
    /**
     * Shift rest of sections
     */
    for(Uint32 i = 1; i<secCount; i++)
    {
      signal->setSection(ptr[i], i - 1);
    }

    sendSignal(dstRef, gsn, signal, ptr[0].sz, JBB);

    signal->header.m_noOfSections = 0;
    signal->setSection(ptr[0], 0);
    releaseSections(signal);
    return ;
  }

  warningEvent("Unable to route GSN: %d from %x to %x",
	       gsn, srcRef, dstRef);
}
