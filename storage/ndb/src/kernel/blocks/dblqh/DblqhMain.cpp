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

#define DBLQH_C
#include "Dblqh.hpp"
#include <ndb_limits.h>
#include <md5_hash.hpp>

#include <ndb_version.h>
#include <signaldata/TuxBound.hpp>
#include <signaldata/AccScan.hpp>
#include <signaldata/CopyActive.hpp>
#include <signaldata/CopyFrag.hpp>
#include <signaldata/CreateTrig.hpp>
#include <signaldata/DropTrig.hpp>
#include <signaldata/EmptyLcp.hpp>
#include <signaldata/EventReport.hpp>
#include <signaldata/ExecFragReq.hpp>
#include <signaldata/GCPSave.hpp>
#include <signaldata/TcKeyRef.hpp>
#include <signaldata/LqhKey.hpp>
#include <signaldata/NextScan.hpp>
#include <signaldata/NFCompleteRep.hpp>
#include <signaldata/NodeFailRep.hpp>
#include <signaldata/ReadNodesConf.hpp>
#include <signaldata/RelTabMem.hpp>
#include <signaldata/ScanFrag.hpp>
#include <signaldata/SrFragidConf.hpp>
#include <signaldata/StartFragReq.hpp>
#include <signaldata/StartRec.hpp>
#include <signaldata/TupKey.hpp>
#include <signaldata/TupCommit.hpp>
#include <signaldata/LqhFrag.hpp>
#include <signaldata/AccFrag.hpp>
#include <signaldata/TupFrag.hpp>
#include <signaldata/DumpStateOrd.hpp>
#include <signaldata/PackedSignal.hpp>

#include <signaldata/PrepDropTab.hpp>
#include <signaldata/DropTab.hpp>

#include <signaldata/AlterTab.hpp>

#include <signaldata/LCP.hpp>
#include <KeyDescriptor.hpp>

// Use DEBUG to print messages that should be
// seen only when we debug the product
#ifdef VM_TRACE
#define DEBUG(x) ndbout << "DBLQH: "<< x << endl;
NdbOut &
operator<<(NdbOut& out, Dblqh::TcConnectionrec::TransactionState state){
  out << (int)state;
  return out;
}

NdbOut &
operator<<(NdbOut& out, Dblqh::TcConnectionrec::LogWriteState state){
  out << (int)state;
  return out;
}

NdbOut &
operator<<(NdbOut& out, Dblqh::TcConnectionrec::ListState state){
  out << (int)state;
  return out;
}

NdbOut &
operator<<(NdbOut& out, Dblqh::TcConnectionrec::AbortState state){
  out << (int)state;
  return out;
}

NdbOut &
operator<<(NdbOut& out, Dblqh::ScanRecord::ScanState state){
  out << (int)state;
  return out;
}

NdbOut &
operator<<(NdbOut& out, Dblqh::LogFileOperationRecord::LfoState state){
  out << (int)state;
  return out;
}

NdbOut &
operator<<(NdbOut& out, Dblqh::ScanRecord::ScanType state){
  out << (int)state;
  return out;
}

#else
#define DEBUG(x)
#endif

//#define MARKER_TRACE 1
//#define TRACE_SCAN_TAKEOVER 1

const Uint32 NR_ScanNo = 0;

void Dblqh::execACC_COM_BLOCK(Signal* signal)
{
  jamEntry();
/* ------------------------------------------------------------------------- */
// Undo log buffer in ACC is in critical sector of being full.
/* ------------------------------------------------------------------------- */
  cCounterAccCommitBlocked++;
  caccCommitBlocked = true;
  cCommitBlocked = true;
  return;
}//Dblqh::execACC_COM_BLOCK()

void Dblqh::execACC_COM_UNBLOCK(Signal* signal)
{
  jamEntry();
/* ------------------------------------------------------------------------- */
// Undo log buffer in ACC ok again.
/* ------------------------------------------------------------------------- */
  caccCommitBlocked = false;
  if (ctupCommitBlocked == false) {
    jam();
    cCommitBlocked = false;
  }//if
  return;
}//Dblqh::execACC_COM_UNBLOCK()

void Dblqh::execTUP_COM_BLOCK(Signal* signal)
{
  jamEntry();
/* ------------------------------------------------------------------------- */
// Undo log buffer in TUP is in critical sector of being full.
/* ------------------------------------------------------------------------- */
  cCounterTupCommitBlocked++;
  ctupCommitBlocked = true;
  cCommitBlocked = true;
  return;
}//Dblqh::execTUP_COM_BLOCK()

void Dblqh::execTUP_COM_UNBLOCK(Signal* signal)
{
  jamEntry();
/* ------------------------------------------------------------------------- */
// Undo log buffer in TUP ok again.
/* ------------------------------------------------------------------------- */
  ctupCommitBlocked = false;
  if (caccCommitBlocked == false) {
    jam();
    cCommitBlocked = false;
  }//if
  return;
}//Dblqh::execTUP_COM_UNBLOCK()

/* ------------------------------------------------------------------------- */
/* -------               SEND SYSTEM ERROR                           ------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void Dblqh::systemError(Signal* signal) 
{
  signal->theData[0] = 2304;
  execDUMP_STATE_ORD(signal);
  progError(0, 0);
}//Dblqh::systemError()

/* *************** */
/*  ACCSEIZEREF  > */
/* *************** */
void Dblqh::execACCSEIZEREF(Signal* signal) 
{
  jamEntry();
  ndbrequire(false);
}//Dblqh::execACCSEIZEREF()

/* ******************************************************>> */
/* THIS SIGNAL IS USED TO HANDLE REAL-TIME                  */
/* BREAKS THAT ARE NECESSARY TO ENSURE REAL-TIME            */
/* OPERATION OF LQH.                                        */
/* This signal is also used for signal loops, for example   */
/* the timeout handling for writing logs every second.      */
/* ******************************************************>> */
void Dblqh::execCONTINUEB(Signal* signal) 
{
  jamEntry();
  Uint32 tcase = signal->theData[0];
  Uint32 data0 = signal->theData[1];
  Uint32 data1 = signal->theData[2];
  Uint32 data2 = signal->theData[3];
#if 0
  if (tcase == RNIL) {
    tcConnectptr.i = data0;
    ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
    ndbout << "State = " << tcConnectptr.p->transactionState;
    ndbout << " seqNoReplica = " << tcConnectptr.p->seqNoReplica;
    ndbout << " tcNodeFailrec = " << tcConnectptr.p->tcNodeFailrec;
    ndbout << " activeCreat = " << tcConnectptr.p->activeCreat;
    ndbout << endl;
    ndbout << "tupkeyData0 = " << tcConnectptr.p->tupkeyData[0];
    ndbout << "tupkeyData1 = " << tcConnectptr.p->tupkeyData[1];
    ndbout << "tupkeyData2 = " << tcConnectptr.p->tupkeyData[2];
    ndbout << "tupkeyData3 = " << tcConnectptr.p->tupkeyData[3];
    ndbout << endl;
    ndbout << "abortState = " << tcConnectptr.p->abortState;
    ndbout << "listState = " << tcConnectptr.p->listState;
    ndbout << endl;
    return;
  }//if
#endif
  switch (tcase) {
  case ZLOG_LQHKEYREQ:
    if (cnoOfLogPages == 0) {
      jam();
      sendSignalWithDelay(cownref, GSN_CONTINUEB, signal, 10, 2);
      return;
    }//if
    logPartPtr.i = data0;
    ptrCheckGuard(logPartPtr, clogPartFileSize, logPartRecord);
    logFilePtr.i = logPartPtr.p->currentLogfile;
    ptrCheckGuard(logFilePtr, clogFileFileSize, logFileRecord);
    logPagePtr.i = logFilePtr.p->currentLogpage;
    ptrCheckGuard(logPagePtr, clogPageFileSize, logPageRecord);

    tcConnectptr.i = logPartPtr.p->firstLogQueue;
    ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
    fragptr.i = tcConnectptr.p->fragmentptr;
    ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
    if ((cCommitBlocked == true) &&
        (fragptr.p->fragActiveStatus == ZTRUE)) {
      jam();
      sendSignalWithDelay(cownref, GSN_CONTINUEB, signal, 10, 2);
      return;
    }//if
    logPartPtr.p->LogLqhKeyReqSent = ZFALSE;
    getFirstInLogQueue(signal);

    switch (tcConnectptr.p->transactionState) {
    case TcConnectionrec::LOG_QUEUED:
      if (tcConnectptr.p->abortState != TcConnectionrec::ABORT_IDLE) {
        jam();
        logNextStart(signal);
        abortCommonLab(signal);
        return;
      } else {
        jam();
/*------------------------------------------------------------*/
/*       WE MUST SET THE STATE OF THE LOG PART TO IDLE TO     */
/*       ENSURE THAT WE ARE NOT QUEUED AGAIN ON THE LOG PART  */
/*       WE WILL SET THE LOG PART STATE TO ACTIVE IMMEDIATELY */
/*       SO NO OTHER PROCESS WILL SEE THIS STATE. IT IS MERELY*/
/*       USED TO ENABLE REUSE OF CODE.                        */
/*------------------------------------------------------------*/
        if (logPartPtr.p->logPartState == LogPartRecord::ACTIVE) {
          jam();
          logPartPtr.p->logPartState = LogPartRecord::IDLE;
        }//if
        logLqhkeyreqLab(signal);
        return;
      }//if
      break;
    case TcConnectionrec::LOG_ABORT_QUEUED:
      jam();
      writeAbortLog(signal);
      removeLogTcrec(signal);
      logNextStart(signal);
      continueAfterLogAbortWriteLab(signal);
      return;
      break;
    case TcConnectionrec::LOG_COMMIT_QUEUED:
    case TcConnectionrec::LOG_COMMIT_QUEUED_WAIT_SIGNAL:
      jam();
      writeCommitLog(signal, logPartPtr);
      logNextStart(signal);
      if (tcConnectptr.p->transactionState == TcConnectionrec::LOG_COMMIT_QUEUED) {
        if (tcConnectptr.p->seqNoReplica != 0) {
          jam();
          commitReplyLab(signal);
        } else {
          jam();
          localCommitLab(signal);
        }//if
        return;
      } else {
        jam();
        tcConnectptr.p->transactionState = TcConnectionrec::LOG_COMMIT_WRITTEN_WAIT_SIGNAL;
        return;
      }//if
      break;
    case TcConnectionrec::COMMIT_QUEUED:
      jam();
      logNextStart(signal);
      localCommitLab(signal);
      break;
    case TcConnectionrec::ABORT_QUEUED:
      jam();
      logNextStart(signal);
      abortCommonLab(signal);
      break;
    default:
      ndbrequire(false);
      break;
    }//switch
    return;
    break;
  case ZSR_GCI_LIMITS:
    jam();
    signal->theData[0] = data0;
    srGciLimits(signal);
    return;
    break;
  case ZSR_LOG_LIMITS:
    jam();
    signal->theData[0] = data0;
    signal->theData[1] = data1;
    signal->theData[2] = data2;
    srLogLimits(signal);
    return;
    break;
  case ZSEND_EXEC_CONF:
    jam();
    signal->theData[0] = data0;
    sendExecConf(signal);
    return;
    break;
  case ZEXEC_SR:
    jam();
    signal->theData[0] = data0;
    execSr(signal);
    return;
    break;
  case ZSR_FOURTH_COMP:
    jam();
    signal->theData[0] = data0;
    srFourthComp(signal);
    return;
    break;
  case ZINIT_FOURTH:
    jam();
    signal->theData[0] = data0;
    initFourth(signal);
    return;
    break;
  case ZTIME_SUPERVISION:
    jam();
    signal->theData[0] = data0;
    timeSup(signal);
    return;
    break;
  case ZSR_PHASE3_START:
    jam();
    signal->theData[0] = data0;
    srPhase3Start(signal);
    return;
    break;
  case ZLQH_TRANS_NEXT:
    jam();
    tcNodeFailptr.i = data0;
    ptrCheckGuard(tcNodeFailptr, ctcNodeFailrecFileSize, tcNodeFailRecord);
    lqhTransNextLab(signal);
    return;
    break;
  case ZSCAN_TC_CONNECT:
    jam();
    tabptr.i = data1;
    ptrCheckGuard(tabptr, ctabrecFileSize, tablerec);
    scanTcConnectLab(signal, data0, data2);
    return;
    break;
  case ZINITIALISE_RECORDS:
    jam();
    initialiseRecordsLab(signal, data0, data2, signal->theData[4]);
    return;
    break;
  case ZINIT_GCP_REC:
    jam();
    gcpPtr.i = 0;
    ptrAss(gcpPtr, gcpRecord);
    initGcpRecLab(signal);
    return;
    break;
  case ZRESTART_OPERATIONS_AFTER_STOP:
    jam();
    tcConnectptr.i = data0;
    ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
    if (tcConnectptr.p->listState != TcConnectionrec::WAIT_QUEUE_LIST) {
      jam();
      return;
    }//if
    releaseWaitQueue(signal);
    linkActiveFrag(signal);
    restartOperationsAfterStopLab(signal);
    return;
    break;
  case ZCHECK_LCP_STOP_BLOCKED:
    jam();
    c_scanRecordPool.getPtr(scanptr, data0);
    tcConnectptr.i = scanptr.p->scanTcrec;
    ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
    fragptr.i = tcConnectptr.p->fragmentptr;
    ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
    checkLcpStopBlockedLab(signal);
    return;
  case ZSCAN_MARKERS:
    jam();
    scanMarkers(signal, data0, data1, data2);
    return;
    break;

  case ZOPERATION_EVENT_REP:
    jam();
    /* --------------------------------------------------------------------- */
    // Report information about transaction activity once per second.
    /* --------------------------------------------------------------------- */
    if (signal->theData[1] == 0) {
      signal->theData[0] = NDB_LE_OperationReportCounters;
      signal->theData[1] = c_Counters.operations;
      sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 2, JBB);
    }//if
    c_Counters.clear();
    signal->theData[0] = ZOPERATION_EVENT_REP;
    signal->theData[1] = 0;
    sendSignalWithDelay(cownref, GSN_CONTINUEB, signal, 5000, 2);
    break;
  case ZPREP_DROP_TABLE:
    jam();
    checkDropTab(signal);
    return;
    break;
  default:
    ndbrequire(false);
    break;
  }//switch
}//Dblqh::execCONTINUEB()

/* *********************************************************> */
/*  Request from DBDIH to include a new node in the node list */
/*  and so forth.                                             */
/* *********************************************************> */
void Dblqh::execINCL_NODEREQ(Signal* signal) 
{
  jamEntry();
  BlockReference retRef = signal->theData[0];
  Uint32 nodeId = signal->theData[1];
  cnewestGci = signal->theData[2];
  cnewestCompletedGci = signal->theData[2] - 1;
  ndbrequire(cnoOfNodes < MAX_NDB_NODES);
  for (Uint32 i = 0; i < cnoOfNodes; i++) {
    jam();
    if (cnodeData[i] == nodeId) {
      jam();
      cnodeStatus[i] = ZNODE_UP;
    }//if
  }//for
  signal->theData[0] = cownref; 
  sendSignal(retRef, GSN_INCL_NODECONF, signal, 1, JBB);
  return;
}//Dblqh::execINCL_NODEREQ()

void Dblqh::execTUPSEIZEREF(Signal* signal) 
{
  jamEntry();
  ndbrequire(false);
}//Dblqh::execTUPSEIZEREF()

/* ########################################################################## */
/* #######                  START / RESTART MODULE                    ####### */
/* ########################################################################## */
/* ************************************************************************>> */
/*  This is first signal that arrives in a start / restart. Sender is NDBCNTR_REF. */
/* ************************************************************************>> */
void Dblqh::execSTTOR(Signal* signal) 
{
  UintR tstartPhase;

  jamEntry();
                                                  /* START CASE */
  tstartPhase = signal->theData[1];
                                                  /* SYSTEM RESTART RANK */
  csignalKey = signal->theData[6];
  switch (tstartPhase) {
  case ZSTART_PHASE1:
    jam();
    cstartPhase = tstartPhase;
    sttorStartphase1Lab(signal);
    c_tup = (Dbtup*)globalData.getBlock(DBTUP);
    ndbrequire(c_tup != 0);
    return;
    break;
  default:
    jam();
    /*empty*/;
    sendsttorryLab(signal);
    return;
    break;
  }//switch
}//Dblqh::execSTTOR()

/* ***************************************> */
/*  Restart phases 1 - 6, sender is Ndbcntr */
/* ***************************************> */
void Dblqh::execNDB_STTOR(Signal* signal) 
{
  jamEntry();
  Uint32 ownNodeId = signal->theData[1];   /* START PHASE*/
  cstartPhase = signal->theData[2];  /* MY NODE ID */
  cstartType = signal->theData[3];   /* START TYPE */

  switch (cstartPhase) {
  case ZSTART_PHASE1:
    jam();
    preComputedRequestInfoMask = 0;
    LqhKeyReq::setKeyLen(preComputedRequestInfoMask, RI_KEYLEN_MASK);
    LqhKeyReq::setLastReplicaNo(preComputedRequestInfoMask, RI_LAST_REPL_MASK);
    LqhKeyReq::setLockType(preComputedRequestInfoMask, RI_LOCK_TYPE_MASK);
    // Dont LqhKeyReq::setApplicationAddressFlag
    LqhKeyReq::setDirtyFlag(preComputedRequestInfoMask, 1);
    // Dont LqhKeyReq::setInterpretedFlag
    LqhKeyReq::setSimpleFlag(preComputedRequestInfoMask, 1);
    LqhKeyReq::setOperation(preComputedRequestInfoMask, RI_OPERATION_MASK);
    // Dont setAIInLqhKeyReq
    // Dont setSeqNoReplica
    // Dont setSameClientAndTcFlag
    // Dont setReturnedReadLenAIFlag
    // Dont setAPIVersion
    LqhKeyReq::setMarkerFlag(preComputedRequestInfoMask, 1);
    //preComputedRequestInfoMask = 0x003d7fff;
    startphase1Lab(signal, /* dummy */ ~0, ownNodeId);

    signal->theData[0] = ZOPERATION_EVENT_REP;
    signal->theData[1] = 1;
    sendSignalWithDelay(cownref, GSN_CONTINUEB, signal, 10, 2);
    return;
    break;
  case ZSTART_PHASE2:
    jam();
    startphase2Lab(signal, /* dummy */ ~0);
    return;
    break;
  case ZSTART_PHASE3:
    jam();
    startphase3Lab(signal);
    return;
    break;
  case ZSTART_PHASE4:
    jam();
    startphase4Lab(signal);
    return;
    break;
  case ZSTART_PHASE6:
    jam();
    startphase6Lab(signal);
    return;
    break;
  default:
    jam();
    /*empty*/;
    sendNdbSttorryLab(signal);
    return;
    break;
  }//switch
}//Dblqh::execNDB_STTOR()

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* +++++++                     START PHASE 1                          +++++++ */
/*               LOAD OUR BLOCK REFERENCE AND OUR PROCESSOR ID                */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
void Dblqh::sttorStartphase1Lab(Signal* signal) 
{
  sendsttorryLab(signal);
  return;
}//Dblqh::sttorStartphase1Lab()

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* +++++++                           START PHASE 2                    +++++++ */
/*                                                                            */
/*               INITIATE ALL RECORDS WITHIN THE BLOCK                        */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
void Dblqh::startphase1Lab(Signal* signal, Uint32 _dummy, Uint32 ownNodeId) 
{
  UintR Ti;
  HostRecordPtr ThostPtr;

/* ------- INITIATE ALL RECORDS ------- */
  cownNodeid    = ownNodeId;
  caccBlockref  = calcAccBlockRef (cownNodeid);
  ctupBlockref  = calcTupBlockRef (cownNodeid);
  ctuxBlockref  = calcTuxBlockRef (cownNodeid);
  cownref       = calcLqhBlockRef (cownNodeid);
  for (Ti = 0; Ti < chostFileSize; Ti++) {
    ThostPtr.i = Ti;
    ptrCheckGuard(ThostPtr, chostFileSize, hostRecord);
    ThostPtr.p->hostLqhBlockRef = calcLqhBlockRef(ThostPtr.i);
    ThostPtr.p->hostTcBlockRef  = calcTcBlockRef(ThostPtr.i);
    ThostPtr.p->inPackedList = false;
    ThostPtr.p->noOfPackedWordsLqh = 0;
    ThostPtr.p->noOfPackedWordsTc  = 0;
  }//for
  cpackedListIndex = 0;
  sendNdbSttorryLab(signal);
  return;
}//Dblqh::startphase1Lab()

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* +++++++                           START PHASE 2                    +++++++ */
/*                                                                            */
/* CONNECT LQH WITH ACC AND TUP.                                              */
/* EVERY CONNECTION RECORD IN LQH IS ASSIGNED TO ONE ACC CONNECTION RECORD    */
/*       AND ONE TUP CONNECTION RECORD.                                       */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
void Dblqh::startphase2Lab(Signal* signal, Uint32 _dummy) 
{
  cmaxWordsAtNodeRec = MAX_NO_WORDS_OUTSTANDING_COPY_FRAGMENT;
/* -- ACC AND TUP CONNECTION PROCESS -- */
  tcConnectptr.i = 0;
  ptrAss(tcConnectptr, tcConnectionrec);
  moreconnectionsLab(signal);
  return;
}//Dblqh::startphase2Lab()

void Dblqh::moreconnectionsLab(Signal* signal) 
{
  tcConnectptr.p->tcAccBlockref = caccBlockref;
  // set TUX block here (no operation is seized in TUX)
  tcConnectptr.p->tcTuxBlockref = ctuxBlockref;
/* NO STATE CHECKING IS PERFORMED, ASSUMED TO WORK */
/* *************** */
/*  ACCSEIZEREQ  < */
/* *************** */
  signal->theData[0] = tcConnectptr.i;
  signal->theData[1] = cownref;
  sendSignal(caccBlockref, GSN_ACCSEIZEREQ, signal, 2, JBB);
  return;
}//Dblqh::moreconnectionsLab()

/* ***************> */
/*  ACCSEIZECONF  > */
/* ***************> */
void Dblqh::execACCSEIZECONF(Signal* signal) 
{
  jamEntry();
  tcConnectptr.i = signal->theData[0];
  ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
  tcConnectptr.p->accConnectrec = signal->theData[1];
/* *************** */
/*  TUPSEIZEREQ  < */
/* *************** */
  tcConnectptr.p->tcTupBlockref = ctupBlockref;
  signal->theData[0] = tcConnectptr.i;
  signal->theData[1] = cownref;
  sendSignal(ctupBlockref, GSN_TUPSEIZEREQ, signal, 2, JBB);
  return;
}//Dblqh::execACCSEIZECONF()

/* ***************> */
/*  TUPSEIZECONF  > */
/* ***************> */
void Dblqh::execTUPSEIZECONF(Signal* signal) 
{
  jamEntry();
  tcConnectptr.i = signal->theData[0];
  ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
  tcConnectptr.p->tupConnectrec = signal->theData[1];
/* ------- CHECK IF THERE ARE MORE CONNECTIONS TO BE CONNECTED ------- */
  tcConnectptr.i = tcConnectptr.p->nextTcConnectrec;
  if (tcConnectptr.i != RNIL) {
    jam();
    ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
    moreconnectionsLab(signal);
    return;
  }//if
/* ALL LQH_CONNECT RECORDS ARE CONNECTED TO ACC AND TUP ---- */
  sendNdbSttorryLab(signal);
  return;
}//Dblqh::execTUPSEIZECONF()

/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* +++++++                    START PHASE 4                          +++++++ */
/*                                                                           */
/*       CONNECT LQH WITH LQH.                                               */
/*       CONNECT EACH LQH WITH EVERY LQH IN THE DATABASE SYSTEM.             */
/*       IF INITIAL START THEN CREATE THE FRAGMENT LOG FILES                 */
/*IF SYSTEM RESTART OR NODE RESTART THEN OPEN THE FRAGMENT LOG FILES AND     */
/*FIND THE END OF THE LOG FILES.                                             */
/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/*        WAIT UNTIL ADD NODE PROCESSES ARE COMPLETED                        */
/*        IF INITIAL START ALSO WAIT FOR LOG FILES TO INITIALISED            */
/*START TIME SUPERVISION OF LOG FILES. WE HAVE TO WRITE LOG PAGES TO DISK    */
/*EVEN IF THE PAGES ARE NOT FULL TO ENSURE THAT THEY COME TO DISK ASAP.      */
/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
void Dblqh::startphase3Lab(Signal* signal) 
{
  LogFileRecordPtr prevLogFilePtr;
  LogFileRecordPtr zeroLogFilePtr;

  caddNodeState = ZTRUE;
/* ***************<< */
/*  READ_NODESREQ  < */
/* ***************<< */
  cinitialStartOngoing = ZTRUE;
  ndbrequire(cnoLogFiles != 0);

  for (logPartPtr.i = 0; logPartPtr.i < 4; logPartPtr.i++) {
    jam();
    ptrAss(logPartPtr, logPartRecord);
    initLogpart(signal);
    for (Uint32 fileNo = 0; fileNo < cnoLogFiles; fileNo++) {
      seizeLogfile(signal);
      if (fileNo != 0) {
        jam();
        prevLogFilePtr.p->nextLogFile = logFilePtr.i;
        logFilePtr.p->prevLogFile = prevLogFilePtr.i;
      } else {
        jam();
        logPartPtr.p->firstLogfile = logFilePtr.i;
        logPartPtr.p->currentLogfile = logFilePtr.i;
        zeroLogFilePtr.i = logFilePtr.i;
        zeroLogFilePtr.p = logFilePtr.p;
      }//if
      prevLogFilePtr.i = logFilePtr.i;
      prevLogFilePtr.p = logFilePtr.p;
      initLogfile(signal, fileNo);
      if ((cstartType == NodeState::ST_INITIAL_START) ||
	  (cstartType == NodeState::ST_INITIAL_NODE_RESTART)) {
        if (logFilePtr.i == zeroLogFilePtr.i) {
          jam();
/* ------------------------------------------------------------------------- */
/*IN AN INITIAL START WE START BY CREATING ALL LOG FILES AND SETTING THEIR   */
/*PROPER SIZE AND INITIALISING PAGE ZERO IN ALL FILES.                       */
/*WE START BY CREATING FILE ZERO IN EACH LOG PART AND THEN PROCEED           */
/*SEQUENTIALLY THROUGH ALL LOG FILES IN THE LOG PART.                        */
/* ------------------------------------------------------------------------- */
          openLogfileInit(signal);
        }//if
      }//if
    }//for
    zeroLogFilePtr.p->prevLogFile = logFilePtr.i;
    logFilePtr.p->nextLogFile = zeroLogFilePtr.i;
  }//for
  if (cstartType != NodeState::ST_INITIAL_START && 
      cstartType != NodeState::ST_INITIAL_NODE_RESTART) {
    jam();
    ndbrequire(cstartType == NodeState::ST_NODE_RESTART || 
	       cstartType == NodeState::ST_SYSTEM_RESTART);
    /** --------------------------------------------------------------------
     * THIS CODE KICKS OFF THE SYSTEM RESTART AND NODE RESTART. IT STARTS UP 
     * THE RESTART BY FINDING THE END OF THE LOG AND FROM THERE FINDING THE 
     * INFO ABOUT THE GLOBAL CHECKPOINTS IN THE FRAGMENT LOG. 
     --------------------------------------------------------------------- */
    for (logPartPtr.i = 0; logPartPtr.i < 4; logPartPtr.i++) {
      jam();
      LogFileRecordPtr locLogFilePtr;
      ptrAss(logPartPtr, logPartRecord);
      locLogFilePtr.i = logPartPtr.p->firstLogfile;
      ptrCheckGuard(locLogFilePtr, clogFileFileSize, logFileRecord);
      locLogFilePtr.p->logFileStatus = LogFileRecord::OPEN_SR_FRONTPAGE;
      openFileRw(signal, locLogFilePtr);
    }//for
  }//if

  signal->theData[0] = cownref;
  sendSignal(NDBCNTR_REF, GSN_READ_NODESREQ, signal, 1, JBB);
  return;
}//Dblqh::startphase3Lab()

/* ****************** */
/*  READ_NODESCONF  > */
/* ****************** */
void Dblqh::execREAD_NODESCONF(Signal* signal) 
{
  jamEntry();

  ReadNodesConf * const readNodes = (ReadNodesConf *)&signal->theData[0];
  cnoOfNodes = readNodes->noOfNodes;

  unsigned ind = 0;
  unsigned i = 0;
  for (i = 1; i < MAX_NDB_NODES; i++) {
    jam();
    if (NodeBitmask::get(readNodes->allNodes, i)) {
      jam();
      cnodeData[ind]    = i;
      cnodeStatus[ind]  = NodeBitmask::get(readNodes->inactiveNodes, i);
      //readNodes->getVersionId(i, readNodes->theVersionIds) not used
      ind++;
    }//if
  }//for
  ndbrequire(ind == cnoOfNodes);
  ndbrequire(cnoOfNodes >= 1 && cnoOfNodes < MAX_NDB_NODES);
  ndbrequire(!(cnoOfNodes == 1 && cstartType == NodeState::ST_NODE_RESTART));
  
  caddNodeState = ZFALSE;
  if (cstartType == NodeState::ST_SYSTEM_RESTART) {
    jam();
    sendNdbSttorryLab(signal);
    return;
  }//if
  checkStartCompletedLab(signal);
  return;
}//Dblqh::execREAD_NODESCONF()

void Dblqh::checkStartCompletedLab(Signal* signal) 
{
  if (caddNodeState == ZFALSE) {
    if (cinitialStartOngoing == ZFALSE) {
      jam();
      sendNdbSttorryLab(signal);
      return;
    }//if
  }//if
  return;
}//Dblqh::checkStartCompletedLab()

void Dblqh::startphase4Lab(Signal* signal) 
{
  sendNdbSttorryLab(signal);
  return;
}//Dblqh::startphase4Lab()

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* SET CONCURRENCY OF LOCAL CHECKPOINTS TO BE USED AFTER SYSTEM RESTART.      */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
void Dblqh::startphase6Lab(Signal* signal) 
{
  cstartPhase = ZNIL;
  cstartType = ZNIL;
  sendNdbSttorryLab(signal);
  return;
}//Dblqh::startphase6Lab()

void Dblqh::sendNdbSttorryLab(Signal* signal) 
{
  signal->theData[0] = cownref;
  sendSignal(NDBCNTR_REF, GSN_NDB_STTORRY, signal, 1, JBB);
  return;
}//Dblqh::sendNdbSttorryLab()

void Dblqh::sendsttorryLab(Signal* signal) 
{
/* *********<< */
/*  STTORRY  < */
/* *********<< */
  signal->theData[0] = csignalKey; /* SIGNAL KEY */
  signal->theData[1] = 3;          /* BLOCK CATEGORY */
  signal->theData[2] = 2;          /* SIGNAL VERSION NUMBER */
  signal->theData[3] = ZSTART_PHASE1;
  signal->theData[4] = 255;
  sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 5, JBB);
  return;
}//Dblqh::sendsttorryLab()

/* ***************>> */
/*  READ_NODESREF  > */
/* ***************>> */
void Dblqh::execREAD_NODESREF(Signal* signal) 
{
  jamEntry();
  ndbrequire(false);
}//Dblqh::execREAD_NODESREF()

/* *************** */
/*  SIZEALT_REP  > */
/* *************** */
void Dblqh::execREAD_CONFIG_REQ(Signal* signal) 
{
  const ReadConfigReq * req = (ReadConfigReq*)signal->getDataPtr();
  Uint32 ref = req->senderRef;
  Uint32 senderData = req->senderData;
  ndbrequire(req->noOfParameters == 0);

  jamEntry();

  const ndb_mgm_configuration_iterator * p = 
    theConfiguration.getOwnConfigIterator();
  ndbrequire(p != 0);
  
  cnoLogFiles = 8;
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_DB_NO_REDOLOG_FILES, 
					&cnoLogFiles));
  ndbrequire(cnoLogFiles > 0);

  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_LQH_FRAG, &cfragrecFileSize));
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_LQH_TABLE, &ctabrecFileSize));
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_LQH_TC_CONNECT, 
					&ctcConnectrecFileSize));
  clogFileFileSize       = 4 * cnoLogFiles;
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_LQH_SCAN, &cscanrecFileSize));
  cmaxAccOps = cscanrecFileSize * MAX_PARALLEL_OP_PER_SCAN;

  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_DB_DISCLESS, &c_diskless));
  
  initRecords();
  initialiseRecordsLab(signal, 0, ref, senderData);
  
  return;
}//Dblqh::execSIZEALT_REP()

/* ########################################################################## */
/* #######                          ADD/DELETE FRAGMENT MODULE        ####### */
/*       THIS MODULE IS USED BY DICTIONARY TO CREATE NEW FRAGMENTS AND DELETE */
/*       OLD FRAGMENTS.                                                       */
/*                                                                            */
/* ########################################################################## */
/* -------------------------------------------------------------- */
/*            FRAG REQ                                            */
/* -------------------------------------------------------------- */
/* *********************************************************> */
/*  LQHFRAGREQ: Create new fragments for a table. Sender DICT */
/* *********************************************************> */

// this unbelievable mess could be replaced by one signal to LQH
// and execute direct to local DICT to get everything at once

void Dblqh::execLQHFRAGREQ(Signal* signal) 
{
  jamEntry();
  LqhFragReq * req = (LqhFragReq*)signal->getDataPtr();
  
  Uint32 retPtr = req->senderData;
  BlockReference retRef = req->senderRef;
  Uint32 fragId = req->fragmentId;
  Uint32 reqinfo = req->requestInfo;
  tabptr.i = req->tableId;
  Uint16 tlocalKeylen = req->localKeyLength;
  Uint32 tmaxLoadFactor = req->maxLoadFactor;
  Uint32 tminLoadFactor = req->minLoadFactor;
  Uint8 tk = req->kValue;
  Uint8 tlhstar = req->lh3DistrBits;
  Uint8 tlh = req->lh3PageBits;
  Uint32 tnoOfAttr = req->noOfAttributes;
  Uint32 tnoOfNull = req->noOfNullAttributes;
  Uint32 noOfAlloc = req->noOfPagesToPreAllocate;
  Uint32 tschemaVersion = req->schemaVersion;
  Uint32 ttupKeyLength = req->keyLength;
  Uint32 nextLcp = req->nextLCP;
  Uint32 noOfKeyAttr = req->noOfKeyAttr;
  Uint32 noOfNewAttr = req->noOfNewAttr;
  Uint32 checksumIndicator = req->checksumIndicator;
  Uint32 noOfAttributeGroups = req->noOfAttributeGroups;
  Uint32 gcpIndicator = req->GCPIndicator;
  Uint32 startGci = req->startGci;
  Uint32 tableType = req->tableType;
  Uint32 primaryTableId = req->primaryTableId;

  ptrCheckGuard(tabptr, ctabrecFileSize, tablerec);
  bool tempTable = ((reqinfo & LqhFragReq::TemporaryTable) != 0);

  /* Temporary tables set to defined in system restart */
  if (tabptr.p->tableStatus == Tablerec::NOT_DEFINED){
    tabptr.p->tableStatus = Tablerec::ADD_TABLE_ONGOING;
    tabptr.p->tableType = tableType;
    tabptr.p->primaryTableId = primaryTableId;
    tabptr.p->schemaVersion = tschemaVersion;
  }//if
  
  if (tabptr.p->tableStatus != Tablerec::ADD_TABLE_ONGOING){
    jam();
    fragrefLab(signal, retRef, retPtr, ZTAB_STATE_ERROR);
    return;
  }//if
  //--------------------------------------------------------------------
  // We could arrive here if we create the fragment as part of a take
  // over by a hot spare node. The table is then is already created
  // and bit 31 is set, thus indicating that we are creating a fragment
  // by copy creation. Also since the node has already been started we
  // know that it is not a node restart ongoing.
  //--------------------------------------------------------------------

  if (getFragmentrec(signal, fragId)) {
    jam();
    fragrefLab(signal, retRef, retPtr, terrorCode);
    return;
  }//if
  if (!insertFragrec(signal, fragId)) {
    jam();
    fragrefLab(signal, retRef, retPtr, terrorCode);
    return;
  }//if
  Uint32 copyType = reqinfo & 3;
  initFragrec(signal, tabptr.i, fragId, copyType);
  fragptr.p->startGci = startGci;
  fragptr.p->newestGci = startGci;
  fragptr.p->tableType = tableType;

  if (DictTabInfo::isOrderedIndex(tableType)) {
    jam();
    // find corresponding primary table fragment
    TablerecPtr tTablePtr;
    tTablePtr.i = primaryTableId;
    ptrCheckGuard(tTablePtr, ctabrecFileSize, tablerec);
    FragrecordPtr tFragPtr;
    tFragPtr.i = RNIL;
    for (Uint32 i = 0; i < MAX_FRAG_PER_NODE; i++) {
      if (tTablePtr.p->fragid[i] == fragptr.p->fragId) {
        jam();
        tFragPtr.i = tTablePtr.p->fragrec[i];
        break;
      }
    }
    ndbrequire(tFragPtr.i != RNIL);
    // store it
    fragptr.p->tableFragptr = tFragPtr.i;
  } else {
    fragptr.p->tableFragptr = fragptr.i;
  }

  if (tempTable) {
//--------------------------------------------
// reqinfo bit 3-4 = 2 means temporary table
// without logging or checkpointing.
//--------------------------------------------
    jam();
    fragptr.p->logFlag = Fragrecord::STATE_FALSE;
    fragptr.p->lcpFlag = Fragrecord::LCP_STATE_FALSE;
  }//if
  
  fragptr.p->nextLcp = nextLcp; 
//----------------------------------------------
// For node restarts it is not necessarily zero 
//----------------------------------------------
  if (cfirstfreeAddfragrec == RNIL) {
    jam();
    deleteFragrec(fragId);
    fragrefLab(signal, retRef, retPtr, ZNO_ADD_FRAGREC);
    return;
  }//if
  seizeAddfragrec(signal);
  addfragptr.p->addFragid = fragId;
  addfragptr.p->fragmentPtr = fragptr.i;
  addfragptr.p->dictBlockref = retRef;
  addfragptr.p->dictConnectptr = retPtr;
  addfragptr.p->m_senderAttrPtr = RNIL;
  addfragptr.p->noOfAttr = tnoOfAttr;
  addfragptr.p->noOfNull = tnoOfNull;
  addfragptr.p->noOfAllocPages = noOfAlloc;
  addfragptr.p->tabId = tabptr.i;
  addfragptr.p->totalAttrReceived = 0;
  addfragptr.p->attrSentToTup = ZNIL;/* TO FIND PROGRAMMING ERRORS QUICKLY */
  addfragptr.p->schemaVer = tschemaVersion;
  Uint32 tmp = (reqinfo & LqhFragReq::CreateInRunning);
  addfragptr.p->fragCopyCreation = (tmp == 0 ? 0 : 1);
  addfragptr.p->addfragErrorCode = 0;
  addfragptr.p->noOfKeyAttr = noOfKeyAttr;
  addfragptr.p->noOfNewAttr = noOfNewAttr;
  addfragptr.p->checksumIndicator = checksumIndicator;
  addfragptr.p->noOfAttributeGroups = noOfAttributeGroups;
  addfragptr.p->GCPIndicator = gcpIndicator;
  addfragptr.p->lh3DistrBits = tlhstar;
  addfragptr.p->tableType = tableType;
  addfragptr.p->primaryTableId = primaryTableId;
  //
  addfragptr.p->tup1Connectptr = RNIL;
  addfragptr.p->tup2Connectptr = RNIL;
  addfragptr.p->tux1Connectptr = RNIL;
  addfragptr.p->tux2Connectptr = RNIL;

  if (DictTabInfo::isTable(tableType) ||
      DictTabInfo::isHashIndex(tableType)) {
    jam();
    AccFragReq* const accreq = (AccFragReq*)signal->getDataPtrSend();
    accreq->userPtr = addfragptr.i;
    accreq->userRef = cownref;
    accreq->tableId = tabptr.i;
    accreq->reqInfo = copyType << 4;
    accreq->fragId = fragId;
    accreq->localKeyLen = tlocalKeylen;
    accreq->maxLoadFactor = tmaxLoadFactor;
    accreq->minLoadFactor = tminLoadFactor;
    accreq->kValue = tk;
    accreq->lhFragBits = tlhstar;
    accreq->lhDirBits = tlh;
    accreq->keyLength = ttupKeyLength;
    /* ----------------------------------------------------------------------- */
    /* Send ACCFRAGREQ, when confirmation is received send 2 * TUPFRAGREQ to   */
    /* create 2 tuple fragments on this node.                                  */
    /* ----------------------------------------------------------------------- */
    addfragptr.p->addfragStatus = AddFragRecord::ACC_ADDFRAG;
    sendSignal(fragptr.p->accBlockref, GSN_ACCFRAGREQ,
        signal, AccFragReq::SignalLength, JBB);
    return;
  }
  if (DictTabInfo::isOrderedIndex(tableType)) {
    jam();
    // NOTE: next 2 lines stolen from ACC
    addfragptr.p->fragid1 = (fragId << 1) | 0;
    addfragptr.p->fragid2 = (fragId << 1) | 1;
    addfragptr.p->addfragStatus = AddFragRecord::WAIT_TWO_TUP;
    sendAddFragReq(signal);
    return;
  }
  ndbrequire(false);
}//Dblqh::execLQHFRAGREQ()

/* *************** */
/*  ACCFRAGCONF  > */
/* *************** */
void Dblqh::execACCFRAGCONF(Signal* signal) 
{
  jamEntry();
  addfragptr.i = signal->theData[0];
  Uint32 taccConnectptr = signal->theData[1];
  Uint32 fragId1 = signal->theData[2];
  Uint32 fragId2 = signal->theData[3];
  Uint32 accFragPtr1 = signal->theData[4];
  Uint32 accFragPtr2 = signal->theData[5];
  ptrCheckGuard(addfragptr, caddfragrecFileSize, addFragRecord);
  ndbrequire(addfragptr.p->addfragStatus == AddFragRecord::ACC_ADDFRAG);

  addfragptr.p->accConnectptr = taccConnectptr;
  addfragptr.p->fragid1 = fragId1;
  addfragptr.p->fragid2 = fragId2;
  fragptr.i = addfragptr.p->fragmentPtr;
  ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
  fragptr.p->accFragptr[0] = accFragPtr1;
  fragptr.p->accFragptr[1] = accFragPtr2;

  addfragptr.p->addfragStatus = AddFragRecord::WAIT_TWO_TUP;
  sendAddFragReq(signal);
}//Dblqh::execACCFRAGCONF()

/* *************** */
/*  TUPFRAGCONF  > */
/* *************** */
void Dblqh::execTUPFRAGCONF(Signal* signal) 
{
  jamEntry();
  addfragptr.i = signal->theData[0];
  Uint32 tupConnectptr = signal->theData[1];
  Uint32 tupFragPtr = signal->theData[2];  /* TUP FRAGMENT POINTER */
  Uint32 localFragId = signal->theData[3];  /* LOCAL FRAGMENT ID    */
  ptrCheckGuard(addfragptr, caddfragrecFileSize, addFragRecord);
  fragptr.i = addfragptr.p->fragmentPtr;
  ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
  if (localFragId == addfragptr.p->fragid1) {
    jam();
    fragptr.p->tupFragptr[0] = tupFragPtr;
  } else if (localFragId == addfragptr.p->fragid2) {
    jam();
    fragptr.p->tupFragptr[1] = tupFragPtr;
  } else {
    ndbrequire(false);
    return;
  }//if
  switch (addfragptr.p->addfragStatus) {
  case AddFragRecord::WAIT_TWO_TUP:
    jam();
    fragptr.p->tupFragptr[0] = tupFragPtr;
    addfragptr.p->tup1Connectptr = tupConnectptr;
    addfragptr.p->addfragStatus = AddFragRecord::WAIT_ONE_TUP;
    sendAddFragReq(signal);
    break;
  case AddFragRecord::WAIT_ONE_TUP:
    jam();
    fragptr.p->tupFragptr[1] = tupFragPtr;
    addfragptr.p->tup2Connectptr = tupConnectptr;
    if (DictTabInfo::isOrderedIndex(addfragptr.p->tableType)) {
      addfragptr.p->addfragStatus = AddFragRecord::WAIT_TWO_TUX;
      sendAddFragReq(signal);
      break;
    }
    goto done_with_frag;
    break;
  case AddFragRecord::WAIT_TWO_TUX:
    jam();
    fragptr.p->tuxFragptr[0] = tupFragPtr;
    addfragptr.p->tux1Connectptr = tupConnectptr;
    addfragptr.p->addfragStatus = AddFragRecord::WAIT_ONE_TUX;
    sendAddFragReq(signal);
    break;
  case AddFragRecord::WAIT_ONE_TUX:
    jam();
    fragptr.p->tuxFragptr[1] = tupFragPtr;
    addfragptr.p->tux2Connectptr = tupConnectptr;
    goto done_with_frag;
    break;
  done_with_frag:
    /* ---------------------------------------------------------------- */
    /* Finished create of fragments. Now ready for creating attributes. */
    /* ---------------------------------------------------------------- */
    addfragptr.p->addfragStatus = AddFragRecord::WAIT_ADD_ATTR;
    {
      LqhFragConf* conf = (LqhFragConf*)signal->getDataPtrSend();
      conf->senderData = addfragptr.p->dictConnectptr;
      conf->lqhFragPtr = addfragptr.i;
      sendSignal(addfragptr.p->dictBlockref, GSN_LQHFRAGCONF,
          signal, LqhFragConf::SignalLength, JBB);
    }
    break;
  default:
    ndbrequire(false);
    break;
  }
}//Dblqh::execTUPFRAGCONF()

/* *************** */
/*  TUXFRAGCONF  > */
/* *************** */
void Dblqh::execTUXFRAGCONF(Signal* signal) 
{
  jamEntry();
  execTUPFRAGCONF(signal);
}//Dblqh::execTUXFRAGCONF

/*
 * Add fragment in TUP or TUX.  Called up to 4 times.
 */
void
Dblqh::sendAddFragReq(Signal* signal)
{
  fragptr.i = addfragptr.p->fragmentPtr;
  ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
  if (addfragptr.p->addfragStatus == AddFragRecord::WAIT_TWO_TUP ||
      addfragptr.p->addfragStatus == AddFragRecord::WAIT_ONE_TUP) {
    if (DictTabInfo::isTable(addfragptr.p->tableType) ||
        DictTabInfo::isHashIndex(addfragptr.p->tableType)) {
      jam();
      signal->theData[0] = addfragptr.i;
      signal->theData[1] = cownref;
      signal->theData[2] = 0; /* ADD TABLE */
      signal->theData[3] = addfragptr.p->tabId;
      signal->theData[4] = addfragptr.p->noOfAttr;
      signal->theData[5] =
        addfragptr.p->addfragStatus == AddFragRecord::WAIT_TWO_TUP
        ? addfragptr.p->fragid1 : addfragptr.p->fragid2;
      signal->theData[6] = (addfragptr.p->noOfAllocPages >> 1) + 1;
      signal->theData[7] = addfragptr.p->noOfNull;
      signal->theData[8] = addfragptr.p->schemaVer;
      signal->theData[9] = addfragptr.p->noOfKeyAttr;
      signal->theData[10] = addfragptr.p->noOfNewAttr;
      signal->theData[11] = addfragptr.p->checksumIndicator;
      signal->theData[12] = addfragptr.p->noOfAttributeGroups;
      signal->theData[13] = addfragptr.p->GCPIndicator;
      sendSignal(fragptr.p->tupBlockref, GSN_TUPFRAGREQ,
          signal, TupFragReq::SignalLength, JBB);
      return;
    }
    if (DictTabInfo::isOrderedIndex(addfragptr.p->tableType)) {
      jam();
      signal->theData[0] = addfragptr.i;
      signal->theData[1] = cownref;
      signal->theData[2] = 0; /* ADD TABLE */
      signal->theData[3] = addfragptr.p->tabId;
      signal->theData[4] = 1; /* ordered index: one array attr */
      signal->theData[5] =
        addfragptr.p->addfragStatus == AddFragRecord::WAIT_TWO_TUP
        ? addfragptr.p->fragid1 : addfragptr.p->fragid2;
      signal->theData[6] = (addfragptr.p->noOfAllocPages >> 1) + 1;
      signal->theData[7] = 0; /* ordered index: no nullable */
      signal->theData[8] = addfragptr.p->schemaVer;
      signal->theData[9] = 1; /* ordered index: one key */
      signal->theData[10] = addfragptr.p->noOfNewAttr;
      signal->theData[11] = addfragptr.p->checksumIndicator;
      signal->theData[12] = addfragptr.p->noOfAttributeGroups;
      signal->theData[13] = addfragptr.p->GCPIndicator;
      sendSignal(fragptr.p->tupBlockref, GSN_TUPFRAGREQ,
          signal, TupFragReq::SignalLength, JBB);
      return;
    }
  }
  if (addfragptr.p->addfragStatus == AddFragRecord::WAIT_TWO_TUX ||
      addfragptr.p->addfragStatus == AddFragRecord::WAIT_ONE_TUX) {
    if (DictTabInfo::isOrderedIndex(addfragptr.p->tableType)) {
      jam();
      TuxFragReq* const tuxreq = (TuxFragReq*)signal->getDataPtrSend();
      tuxreq->userPtr = addfragptr.i;
      tuxreq->userRef = cownref;
      tuxreq->reqInfo = 0; /* ADD TABLE */
      tuxreq->tableId = addfragptr.p->tabId;
      ndbrequire(addfragptr.p->noOfAttr >= 2);
      tuxreq->noOfAttr = addfragptr.p->noOfAttr - 1; /* skip NDB$TNODE */
      tuxreq->fragId =
        addfragptr.p->addfragStatus == AddFragRecord::WAIT_TWO_TUX
        ? addfragptr.p->fragid1: addfragptr.p->fragid2;
      tuxreq->fragOff = addfragptr.p->lh3DistrBits;
      tuxreq->tableType = addfragptr.p->tableType;
      tuxreq->primaryTableId = addfragptr.p->primaryTableId;
      // pointer to index fragment in TUP
      tuxreq->tupIndexFragPtrI =
        addfragptr.p->addfragStatus == AddFragRecord::WAIT_TWO_TUX ?
        fragptr.p->tupFragptr[0] : fragptr.p->tupFragptr[1];
      // pointers to table fragments in TUP and ACC
      FragrecordPtr tFragPtr;
      tFragPtr.i = fragptr.p->tableFragptr;
      ptrCheckGuard(tFragPtr, cfragrecFileSize, fragrecord);
      tuxreq->tupTableFragPtrI[0] = tFragPtr.p->tupFragptr[0];
      tuxreq->tupTableFragPtrI[1] = tFragPtr.p->tupFragptr[1];
      tuxreq->accTableFragPtrI[0] = tFragPtr.p->accFragptr[0];
      tuxreq->accTableFragPtrI[1] = tFragPtr.p->accFragptr[1];
      sendSignal(fragptr.p->tuxBlockref, GSN_TUXFRAGREQ,
          signal, TuxFragReq::SignalLength, JBB);
      return;
    }
  }
  ndbrequire(false);
}//Dblqh::sendAddFragReq

/* ************************************************************************> */
/*  LQHADDATTRREQ: Request from DICT to create attributes for the new table. */
/* ************************************************************************> */
void Dblqh::execLQHADDATTREQ(Signal* signal) 
{
  jamEntry();
  LqhAddAttrReq * const req = (LqhAddAttrReq*)signal->getDataPtr();
  
  addfragptr.i = req->lqhFragPtr;
  const Uint32 tnoOfAttr = req->noOfAttributes;
  const Uint32 senderData = req->senderData;
  const Uint32 senderAttrPtr = req->senderAttrPtr;

  ptrCheckGuard(addfragptr, caddfragrecFileSize, addFragRecord);
  ndbrequire(addfragptr.p->addfragStatus == AddFragRecord::WAIT_ADD_ATTR);
  ndbrequire((tnoOfAttr != 0) && (tnoOfAttr <= LqhAddAttrReq::MAX_ATTRIBUTES));
  addfragptr.p->totalAttrReceived += tnoOfAttr;
  ndbrequire(addfragptr.p->totalAttrReceived <= addfragptr.p->noOfAttr);

  addfragptr.p->attrReceived = tnoOfAttr;
  for (Uint32 i = 0; i < tnoOfAttr; i++) {
    addfragptr.p->attributes[i] = req->attributes[i];
  }//for
  addfragptr.p->attrSentToTup = 0;
  ndbrequire(addfragptr.p->dictConnectptr == senderData);
  addfragptr.p->m_senderAttrPtr = senderAttrPtr;
  addfragptr.p->addfragStatus = AddFragRecord::TUP_ATTR_WAIT1;
  sendAddAttrReq(signal);
}//Dblqh::execLQHADDATTREQ()

/* *********************>> */
/*  TUP_ADD_ATTCONF      > */
/* *********************>> */
void Dblqh::execTUP_ADD_ATTCONF(Signal* signal) 
{
  jamEntry();
  addfragptr.i = signal->theData[0];
  // implies that operation was released on the other side
  const bool lastAttr = signal->theData[1];
  ptrCheckGuard(addfragptr, caddfragrecFileSize, addFragRecord);
  switch (addfragptr.p->addfragStatus) {
  case AddFragRecord::TUP_ATTR_WAIT1:
    jam();
    if (lastAttr)
      addfragptr.p->tup1Connectptr = RNIL;
    addfragptr.p->addfragStatus = AddFragRecord::TUP_ATTR_WAIT2;
    sendAddAttrReq(signal);
    break;
  case AddFragRecord::TUP_ATTR_WAIT2:
    jam();
    if (lastAttr)
      addfragptr.p->tup2Connectptr = RNIL;
    if (DictTabInfo::isOrderedIndex(addfragptr.p->tableType)) {
      addfragptr.p->addfragStatus = AddFragRecord::TUX_ATTR_WAIT1;
      sendAddAttrReq(signal);
      break;
    }
    goto done_with_attr;
    break;
  case AddFragRecord::TUX_ATTR_WAIT1:
    jam();
    if (lastAttr)
      addfragptr.p->tux1Connectptr = RNIL;
    addfragptr.p->addfragStatus = AddFragRecord::TUX_ATTR_WAIT2;
    sendAddAttrReq(signal);
    break;
  case AddFragRecord::TUX_ATTR_WAIT2:
    jam();
    if (lastAttr)
      addfragptr.p->tux2Connectptr = RNIL;
    goto done_with_attr;
    break;
  done_with_attr:
    addfragptr.p->attrSentToTup = addfragptr.p->attrSentToTup + 1;
    ndbrequire(addfragptr.p->attrSentToTup <= addfragptr.p->attrReceived);
    ndbrequire(addfragptr.p->totalAttrReceived <= addfragptr.p->noOfAttr);
    if (addfragptr.p->attrSentToTup < addfragptr.p->attrReceived) {
      // more in this batch
      jam();
      addfragptr.p->addfragStatus = AddFragRecord::TUP_ATTR_WAIT1;
      sendAddAttrReq(signal);
    } else if (addfragptr.p->totalAttrReceived < addfragptr.p->noOfAttr) {
      // more batches to receive
      jam();
      addfragptr.p->addfragStatus = AddFragRecord::WAIT_ADD_ATTR;
      LqhAddAttrConf *const conf = (LqhAddAttrConf*)signal->getDataPtrSend();
      conf->senderData = addfragptr.p->dictConnectptr;
      conf->senderAttrPtr = addfragptr.p->m_senderAttrPtr;
      conf->fragId = addfragptr.p->addFragid;
      sendSignal(addfragptr.p->dictBlockref, GSN_LQHADDATTCONF,
          signal, LqhAddAttrConf::SignalLength, JBB);
    } else {
      fragptr.i = addfragptr.p->fragmentPtr;
      ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
      /* ------------------------------------------------------------------ 
       * WE HAVE NOW COMPLETED ADDING THIS FRAGMENT. WE NOW NEED TO SET THE 
       * PROPER STATE IN FRAG_STATUS DEPENDENT ON IF WE ARE CREATING A NEW 
       * REPLICA OR IF WE ARE CREATING A TABLE. FOR FRAGMENTS IN COPY
       * PROCESS WE DO NOT WANT LOGGING ACTIVATED.      
       * ----------------------------------------------------------------- */
      if (addfragptr.p->fragCopyCreation == 1) {
        jam();
        if (! DictTabInfo::isOrderedIndex(addfragptr.p->tableType))
          fragptr.p->fragStatus = Fragrecord::ACTIVE_CREATION;
        else
          fragptr.p->fragStatus = Fragrecord::FSACTIVE;
        fragptr.p->logFlag = Fragrecord::STATE_FALSE;
      } else {
        jam();
        fragptr.p->fragStatus = Fragrecord::FSACTIVE;
      }//if
      LqhAddAttrConf *const conf = (LqhAddAttrConf*)signal->getDataPtrSend();
      conf->senderData = addfragptr.p->dictConnectptr;
      conf->senderAttrPtr = addfragptr.p->m_senderAttrPtr;
      conf->fragId = addfragptr.p->addFragid;
      sendSignal(addfragptr.p->dictBlockref, GSN_LQHADDATTCONF, signal, 
                 LqhAddAttrConf::SignalLength, JBB);
      releaseAddfragrec(signal);
    }//if
    break;
  default:
    ndbrequire(false);
    break;
  }
}

/* **********************>> */
/*  TUX_ADD_ATTRCONF      > */
/* **********************>> */
void Dblqh::execTUX_ADD_ATTRCONF(Signal* signal) 
{
  jamEntry();
  execTUP_ADD_ATTCONF(signal);
}//Dblqh::execTUX_ADD_ATTRCONF

/*
 * Add attribute in TUP or TUX.  Called up to 4 times.
 */
void
Dblqh::sendAddAttrReq(Signal* signal)
{
  arrGuard(addfragptr.p->attrSentToTup, LqhAddAttrReq::MAX_ATTRIBUTES);
  LqhAddAttrReq::Entry& entry =
    addfragptr.p->attributes[addfragptr.p->attrSentToTup];
  const Uint32 attrId = entry.attrId & 0xffff;
  const Uint32 primaryAttrId = entry.attrId >> 16;
  fragptr.i = addfragptr.p->fragmentPtr;
  ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
  if (addfragptr.p->addfragStatus == AddFragRecord::TUP_ATTR_WAIT1 ||
      addfragptr.p->addfragStatus == AddFragRecord::TUP_ATTR_WAIT2) {
    if (DictTabInfo::isTable(addfragptr.p->tableType) ||
        DictTabInfo::isHashIndex(addfragptr.p->tableType) ||
        (DictTabInfo::isOrderedIndex(addfragptr.p->tableType) &&
         primaryAttrId == ZNIL)) {
      jam();
      TupAddAttrReq* const tupreq = (TupAddAttrReq*)signal->getDataPtrSend();
      tupreq->tupConnectPtr =
        addfragptr.p->addfragStatus == AddFragRecord::TUP_ATTR_WAIT1
        ? addfragptr.p->tup1Connectptr : addfragptr.p->tup2Connectptr;
      tupreq->notused1 = 0;
      tupreq->attrId = attrId;
      tupreq->attrDescriptor = entry.attrDescriptor;
      tupreq->extTypeInfo = entry.extTypeInfo;
      sendSignal(fragptr.p->tupBlockref, GSN_TUP_ADD_ATTRREQ,
          signal, TupAddAttrReq::SignalLength, JBB);
      return;
    }
    if (DictTabInfo::isOrderedIndex(addfragptr.p->tableType) &&
        primaryAttrId != ZNIL) {
      // this attribute is not for TUP
      jam();
      TupAddAttrConf* tupconf = (TupAddAttrConf*)signal->getDataPtrSend();
      tupconf->userPtr = addfragptr.i;
      tupconf->lastAttr = false;
      sendSignal(reference(), GSN_TUP_ADD_ATTCONF,
          signal, TupAddAttrConf::SignalLength, JBB);
      return;
    }
  }
  if (addfragptr.p->addfragStatus == AddFragRecord::TUX_ATTR_WAIT1 ||
      addfragptr.p->addfragStatus == AddFragRecord::TUX_ATTR_WAIT2) {
    jam();
    if (DictTabInfo::isOrderedIndex(addfragptr.p->tableType) &&
        primaryAttrId != ZNIL) {
      jam();
      TuxAddAttrReq* const tuxreq = (TuxAddAttrReq*)signal->getDataPtrSend();
      tuxreq->tuxConnectPtr =
        addfragptr.p->addfragStatus == AddFragRecord::TUX_ATTR_WAIT1
        ? addfragptr.p->tux1Connectptr : addfragptr.p->tux2Connectptr;
      tuxreq->notused1 = 0;
      tuxreq->attrId = attrId;
      tuxreq->attrDescriptor = entry.attrDescriptor;
      tuxreq->extTypeInfo = entry.extTypeInfo;
      tuxreq->primaryAttrId = primaryAttrId;
      sendSignal(fragptr.p->tuxBlockref, GSN_TUX_ADD_ATTRREQ,
          signal, TuxAddAttrReq::SignalLength, JBB);
      return;
    }
    if (DictTabInfo::isOrderedIndex(addfragptr.p->tableType) &&
        primaryAttrId == ZNIL) {
      // this attribute is not for TUX
      jam();
      TuxAddAttrConf* tuxconf = (TuxAddAttrConf*)signal->getDataPtrSend();
      tuxconf->userPtr = addfragptr.i;
      tuxconf->lastAttr = false;
      sendSignal(reference(), GSN_TUX_ADD_ATTRCONF,
          signal, TuxAddAttrConf::SignalLength, JBB);
      return;
    }
  }
  ndbrequire(false);
}//Dblqh::sendAddAttrReq

/* ************************************************************************>> */
/*  TAB_COMMITREQ: Commit the new table for use in transactions. Sender DICT. */
/* ************************************************************************>> */
void Dblqh::execTAB_COMMITREQ(Signal* signal) 
{
  jamEntry();
  Uint32 dihPtr = signal->theData[0];
  BlockReference dihBlockref = signal->theData[1];
  tabptr.i = signal->theData[2];

  if (tabptr.i >= ctabrecFileSize) {
    jam();
    terrorCode = ZTAB_FILE_SIZE;
    signal->theData[0] = dihPtr;
    signal->theData[1] = cownNodeid;
    signal->theData[2] = tabptr.i;
    signal->theData[3] = terrorCode;
    sendSignal(dihBlockref, GSN_TAB_COMMITREF, signal, 4, JBB);
    return;
  }//if
  ptrAss(tabptr, tablerec);
  if (tabptr.p->tableStatus != Tablerec::ADD_TABLE_ONGOING) {
    jam();
    terrorCode = ZTAB_STATE_ERROR;
    signal->theData[0] = dihPtr;
    signal->theData[1] = cownNodeid;
    signal->theData[2] = tabptr.i;
    signal->theData[3] = terrorCode;
    signal->theData[4] = tabptr.p->tableStatus;
    sendSignal(dihBlockref, GSN_TAB_COMMITREF, signal, 5, JBB);
    ndbrequire(false);
    return;
  }//if
  tabptr.p->usageCount = 0;
  tabptr.p->tableStatus = Tablerec::TABLE_DEFINED;
  signal->theData[0] = dihPtr;
  signal->theData[1] = cownNodeid;
  signal->theData[2] = tabptr.i;
  sendSignal(dihBlockref, GSN_TAB_COMMITCONF, signal, 3, JBB);
  return;
}//Dblqh::execTAB_COMMITREQ()


void Dblqh::fragrefLab(Signal* signal,
                       BlockReference fragBlockRef,
                       Uint32 fragConPtr,
                       Uint32 errorCode) 
{
  LqhFragRef * ref = (LqhFragRef*)signal->getDataPtrSend();
  ref->senderData = fragConPtr;
  ref->errorCode = errorCode;
  sendSignal(fragBlockRef, GSN_LQHFRAGREF, signal, 
	     LqhFragRef::SignalLength, JBB);
  return;
}//Dblqh::fragrefLab()

/*
 * Abort on-going ops.
 */
void Dblqh::abortAddFragOps(Signal* signal)
{
  fragptr.i = addfragptr.p->fragmentPtr;
  ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
  signal->theData[0] = (Uint32)-1;
  if (addfragptr.p->tup1Connectptr != RNIL) {
    jam();
    signal->theData[1] = addfragptr.p->tup1Connectptr;
    sendSignal(fragptr.p->tupBlockref, GSN_TUPFRAGREQ, signal, 2, JBB);
    addfragptr.p->tup1Connectptr = RNIL;
  }
  if (addfragptr.p->tup2Connectptr != RNIL) {
    jam();
    signal->theData[1] = addfragptr.p->tup2Connectptr;
    sendSignal(fragptr.p->tupBlockref, GSN_TUPFRAGREQ, signal, 2, JBB);
    addfragptr.p->tup2Connectptr = RNIL;
  }
  if (addfragptr.p->tux1Connectptr != RNIL) {
    jam();
    signal->theData[1] = addfragptr.p->tux1Connectptr;
    sendSignal(fragptr.p->tuxBlockref, GSN_TUXFRAGREQ, signal, 2, JBB);
    addfragptr.p->tux1Connectptr = RNIL;
  }
  if (addfragptr.p->tux2Connectptr != RNIL) {
    jam();
    signal->theData[1] = addfragptr.p->tux2Connectptr;
    sendSignal(fragptr.p->tuxBlockref, GSN_TUXFRAGREQ, signal, 2, JBB);
    addfragptr.p->tux2Connectptr = RNIL;
  }
}

/* ************>> */
/*  ACCFRAGREF  > */
/* ************>> */
void Dblqh::execACCFRAGREF(Signal* signal) 
{
  jamEntry();
  addfragptr.i = signal->theData[0];
  ptrCheckGuard(addfragptr, caddfragrecFileSize, addFragRecord);
  terrorCode = signal->theData[1];
  ndbrequire(addfragptr.p->addfragStatus == AddFragRecord::ACC_ADDFRAG);
  addfragptr.p->addfragErrorCode = terrorCode;

  const Uint32 ref = addfragptr.p->dictBlockref;
  const Uint32 senderData = addfragptr.p->dictConnectptr;
  const Uint32 errorCode = addfragptr.p->addfragErrorCode;
  releaseAddfragrec(signal);
  fragrefLab(signal, ref, senderData, errorCode);

  return;
}//Dblqh::execACCFRAGREF()

/* ************>> */
/*  TUPFRAGREF  > */
/* ************>> */
void Dblqh::execTUPFRAGREF(Signal* signal) 
{
  jamEntry();
  addfragptr.i = signal->theData[0];
  ptrCheckGuard(addfragptr, caddfragrecFileSize, addFragRecord);
  terrorCode = signal->theData[1];
  fragptr.i = addfragptr.p->fragmentPtr;
  ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
  addfragptr.p->addfragErrorCode = terrorCode;

  // no operation to release, just add some jams
  switch (addfragptr.p->addfragStatus) {
  case AddFragRecord::WAIT_TWO_TUP:
    jam();
    break;
  case AddFragRecord::WAIT_ONE_TUP:
    jam();
    break;
  case AddFragRecord::WAIT_TWO_TUX:
    jam();
    break;
  case AddFragRecord::WAIT_ONE_TUX:
    jam();
    break;
  default:
    ndbrequire(false);
    break;
  }
  abortAddFragOps(signal);

  const Uint32 ref = addfragptr.p->dictBlockref;
  const Uint32 senderData = addfragptr.p->dictConnectptr;
  const Uint32 errorCode = addfragptr.p->addfragErrorCode;
  releaseAddfragrec(signal);
  fragrefLab(signal, ref, senderData, errorCode);

}//Dblqh::execTUPFRAGREF()

/* ************>> */
/*  TUXFRAGREF  > */
/* ************>> */
void Dblqh::execTUXFRAGREF(Signal* signal) 
{
  jamEntry();
  execTUPFRAGREF(signal);
}//Dblqh::execTUXFRAGREF

/* *********************> */
/*  TUP_ADD_ATTREF      > */
/* *********************> */
void Dblqh::execTUP_ADD_ATTRREF(Signal* signal) 
{
  jamEntry();
  addfragptr.i = signal->theData[0];
  ptrCheckGuard(addfragptr, caddfragrecFileSize, addFragRecord);
  terrorCode = signal->theData[1];
  addfragptr.p->addfragErrorCode = terrorCode;

  // operation was released on the other side
  switch (addfragptr.p->addfragStatus) {
  case AddFragRecord::TUP_ATTR_WAIT1:
    jam();
    ndbrequire(addfragptr.p->tup1Connectptr != RNIL);
    addfragptr.p->tup1Connectptr = RNIL;
    break;
  case AddFragRecord::TUP_ATTR_WAIT2:
    jam();
    ndbrequire(addfragptr.p->tup2Connectptr != RNIL);
    addfragptr.p->tup2Connectptr = RNIL;
    break;
  case AddFragRecord::TUX_ATTR_WAIT1:
    jam();
    ndbrequire(addfragptr.p->tux1Connectptr != RNIL);
    addfragptr.p->tux1Connectptr = RNIL;
    break;
  case AddFragRecord::TUX_ATTR_WAIT2:
    jam();
    ndbrequire(addfragptr.p->tux2Connectptr != RNIL);
    addfragptr.p->tux2Connectptr = RNIL;
    break;
  default:
    ndbrequire(false);
    break;
  }
  abortAddFragOps(signal);
  
  const Uint32 Ref = addfragptr.p->dictBlockref;
  const Uint32 senderData = addfragptr.p->dictConnectptr;
  const Uint32 errorCode = addfragptr.p->addfragErrorCode;
  releaseAddfragrec(signal);
  
  LqhAddAttrRef *const ref = (LqhAddAttrRef*)signal->getDataPtrSend();
  ref->senderData = senderData;
  ref->errorCode = errorCode;
  sendSignal(Ref, GSN_LQHADDATTREF, signal, 
	     LqhAddAttrRef::SignalLength, JBB);
  
}//Dblqh::execTUP_ADD_ATTRREF()

/* **********************> */
/*  TUX_ADD_ATTRREF      > */
/* **********************> */
void Dblqh::execTUX_ADD_ATTRREF(Signal* signal) 
{
  jamEntry();
  execTUP_ADD_ATTRREF(signal);
}//Dblqh::execTUX_ADD_ATTRREF

void
Dblqh::execPREP_DROP_TAB_REQ(Signal* signal){
  jamEntry();

  PrepDropTabReq* req = (PrepDropTabReq*)signal->getDataPtr();
  
  Uint32 senderRef = req->senderRef;
  Uint32 senderData = req->senderData;
  
  TablerecPtr tabPtr;
  tabPtr.i = req->tableId;
  ptrCheckGuard(tabPtr, ctabrecFileSize, tablerec);
  
  Uint32 errCode = 0;
  errCode = checkDropTabState(tabPtr.p->tableStatus, GSN_PREP_DROP_TAB_REQ);
  if(errCode != 0){
    jam();

    PrepDropTabRef* ref = (PrepDropTabRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = senderData;
    ref->tableId = tabPtr.i;
    ref->errorCode = errCode;
    sendSignal(senderRef, GSN_PREP_DROP_TAB_REF, signal,
	       PrepDropTabRef::SignalLength, JBB);
    return;
  }
  
  tabPtr.p->tableStatus = Tablerec::PREP_DROP_TABLE_ONGOING;
  tabPtr.p->waitingTC.clear();
  tabPtr.p->waitingDIH.clear();
  
  PrepDropTabConf * conf = (PrepDropTabConf*)signal->getDataPtrSend();
  conf->tableId = tabPtr.i;
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(senderRef, GSN_PREP_DROP_TAB_CONF, signal,
	     PrepDropTabConf::SignalLength, JBB);
  
  signal->theData[0] = ZPREP_DROP_TABLE;
  signal->theData[1] = tabPtr.i;
  signal->theData[2] = senderRef;
  signal->theData[3] = senderData;
  checkDropTab(signal);
}

void
Dblqh::checkDropTab(Signal* signal){

  TablerecPtr tabPtr;
  tabPtr.i = signal->theData[1];
  ptrCheckGuard(tabPtr, ctabrecFileSize, tablerec);
  
  ndbrequire(tabPtr.p->tableStatus == Tablerec::PREP_DROP_TABLE_ONGOING);
  
  if(tabPtr.p->usageCount > 0){
    jam();
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 4);
    return;
  }

  bool lcpDone = true;
  lcpPtr.i = 0;
  ptrAss(lcpPtr, lcpRecord);
  if(lcpPtr.p->lcpState != LcpRecord::LCP_IDLE){
    jam();

    if(lcpPtr.p->currentFragment.lcpFragOrd.tableId == tabPtr.i){
      jam();
      lcpDone = false;
    }
    
    if(lcpPtr.p->lcpQueued && 
       lcpPtr.p->queuedFragment.lcpFragOrd.tableId == tabPtr.i){
      jam();
      lcpDone = false;
    }
  }
  
  if(!lcpDone){
    jam();
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 4);
    return;
  }
  
  tabPtr.p->tableStatus = Tablerec::PREP_DROP_TABLE_DONE;

  WaitDropTabConf * conf = (WaitDropTabConf*)signal->getDataPtrSend();
  conf->tableId = tabPtr.i;
  conf->senderRef = reference();
  for(Uint32 i = 1; i<MAX_NDB_NODES; i++){
    if(tabPtr.p->waitingTC.get(i)){
      tabPtr.p->waitingTC.clear(i);
      sendSignal(calcTcBlockRef(i), GSN_WAIT_DROP_TAB_CONF, signal,
		 WaitDropTabConf::SignalLength, JBB);
    }
    if(tabPtr.p->waitingDIH.get(i)){
      tabPtr.p->waitingDIH.clear(i);
      sendSignal(calcDihBlockRef(i), GSN_WAIT_DROP_TAB_CONF, signal,
		 WaitDropTabConf::SignalLength, JBB);
    }
  }
}

void
Dblqh::execWAIT_DROP_TAB_REQ(Signal* signal){
  jamEntry();
  WaitDropTabReq * req = (WaitDropTabReq*)signal->getDataPtr();
  
  TablerecPtr tabPtr;
  tabPtr.i = req->tableId;
  ptrCheckGuard(tabPtr, ctabrecFileSize, tablerec);
  
  Uint32 senderRef = req->senderRef;
  Uint32 nodeId = refToNode(senderRef);
  Uint32 blockNo = refToBlock(senderRef);

  if(tabPtr.p->tableStatus == Tablerec::PREP_DROP_TABLE_ONGOING){
    jam();
    switch(blockNo){
    case DBTC:
      tabPtr.p->waitingTC.set(nodeId);
      break;
    case DBDIH:
      tabPtr.p->waitingDIH.set(nodeId);
      break;
    default:
      ndbrequire(false);
    }
    return;
  }

  if(tabPtr.p->tableStatus == Tablerec::PREP_DROP_TABLE_DONE){
    jam();
    WaitDropTabConf * conf = (WaitDropTabConf*)signal->getDataPtrSend();
    conf->tableId = tabPtr.i;
    conf->senderRef = reference();
    sendSignal(senderRef, GSN_WAIT_DROP_TAB_CONF, signal,
	       WaitDropTabConf::SignalLength, JBB);
    return;
  }

  WaitDropTabRef * ref = (WaitDropTabRef*)signal->getDataPtrSend();
  ref->tableId = tabPtr.i;
  ref->senderRef = reference();

  bool ok = false;
  switch(tabPtr.p->tableStatus){
  case Tablerec::TABLE_DEFINED:
    ok = true;
    ref->errorCode = WaitDropTabRef::IllegalTableState;
    break;
  case Tablerec::NOT_DEFINED:
    ok = true;
    ref->errorCode = WaitDropTabRef::NoSuchTable;
    break;
  case Tablerec::ADD_TABLE_ONGOING:
    ok = true;
    ref->errorCode = WaitDropTabRef::IllegalTableState;
    break;
  case Tablerec::PREP_DROP_TABLE_ONGOING:
  case Tablerec::PREP_DROP_TABLE_DONE:
    // Should have been take care of above
    ndbrequire(false);
  }
  ndbrequire(ok);
  ref->tableStatus = tabPtr.p->tableStatus;
  sendSignal(senderRef, GSN_WAIT_DROP_TAB_REF, signal,
	     WaitDropTabRef::SignalLength, JBB);
  return;
}

void
Dblqh::execDROP_TAB_REQ(Signal* signal){
  jamEntry();

  DropTabReq* req = (DropTabReq*)signal->getDataPtr();
  
  Uint32 senderRef = req->senderRef;
  Uint32 senderData = req->senderData;
  
  TablerecPtr tabPtr;
  tabPtr.i = req->tableId;
  ptrCheckGuard(tabPtr, ctabrecFileSize, tablerec);
  
  do {
    if(req->requestType == DropTabReq::RestartDropTab){
      jam();
      break;
    }
    
    if(req->requestType == DropTabReq::OnlineDropTab){
      jam();
      Uint32 errCode = 0;
      errCode = checkDropTabState(tabPtr.p->tableStatus, GSN_DROP_TAB_REQ);
      if(errCode != 0){
	jam();
	
	DropTabRef* ref = (DropTabRef*)signal->getDataPtrSend();
	ref->senderRef = reference();
	ref->senderData = senderData;
	ref->tableId = tabPtr.i;
	ref->errorCode = errCode;
	sendSignal(senderRef, GSN_DROP_TAB_REF, signal,
		   DropTabRef::SignalLength, JBB);
	return;
      }
    }

    removeTable(tabPtr.i);
    
  } while(false);
  
  ndbrequire(tabPtr.p->usageCount == 0);
  tabPtr.p->tableStatus = Tablerec::NOT_DEFINED;
  
  DropTabConf * const dropConf = (DropTabConf *)signal->getDataPtrSend();
  dropConf->senderRef = reference();
  dropConf->senderData = senderData;
  dropConf->tableId = tabPtr.i;
  sendSignal(senderRef, GSN_DROP_TAB_CONF,
             signal, DropTabConf::SignalLength, JBB);
}

Uint32
Dblqh::checkDropTabState(Tablerec::TableStatus status, Uint32 gsn) const{
  
  if(gsn == GSN_PREP_DROP_TAB_REQ){
    switch(status){
    case Tablerec::NOT_DEFINED:
      jam();
      // Fall through
    case Tablerec::ADD_TABLE_ONGOING:
      jam();
      return PrepDropTabRef::NoSuchTable;
      break;
    case Tablerec::PREP_DROP_TABLE_ONGOING:
      jam();
      return PrepDropTabRef::PrepDropInProgress;
      break;
    case Tablerec::PREP_DROP_TABLE_DONE:
      jam();
      return PrepDropTabRef::DropInProgress;
      break;
    case Tablerec::TABLE_DEFINED:
      jam();
      return 0;
      break;
    }
    ndbrequire(0);
  }

  if(gsn == GSN_DROP_TAB_REQ){
    switch(status){
    case Tablerec::NOT_DEFINED:
      jam();
      // Fall through
    case Tablerec::ADD_TABLE_ONGOING:
      jam();
      return DropTabRef::NoSuchTable;
      break;
    case Tablerec::PREP_DROP_TABLE_ONGOING:
      jam();
      return DropTabRef::PrepDropInProgress;
      break;
    case Tablerec::PREP_DROP_TABLE_DONE:
      jam();
      return 0;
      break;
    case Tablerec::TABLE_DEFINED:
      jam();
      return DropTabRef::DropWoPrep;
    }
    ndbrequire(0);
  }
  ndbrequire(0);
  return RNIL;
}

void Dblqh::removeTable(Uint32 tableId)
{
  tabptr.i = tableId;
  ptrCheckGuard(tabptr, ctabrecFileSize, tablerec);
  
  for (Uint32 i = (MAX_FRAG_PER_NODE - 1); (Uint32)~i; i--) {
    jam();
    if (tabptr.p->fragid[i] != ZNIL) {
      jam();
      deleteFragrec(tabptr.p->fragid[i]);
    }//if
  }//for
}//Dblqh::removeTable()

void
Dblqh::execALTER_TAB_REQ(Signal* signal)
{
  jamEntry();
  AlterTabReq* const req = (AlterTabReq*)signal->getDataPtr();
  const Uint32 senderRef = req->senderRef;
  const Uint32 senderData = req->senderData;
  const Uint32 changeMask = req->changeMask;
  const Uint32 tableId = req->tableId;
  const Uint32 tableVersion = req->tableVersion;
  const Uint32 gci = req->gci;
  AlterTabReq::RequestType requestType =
    (AlterTabReq::RequestType) req->requestType;

  TablerecPtr tablePtr;
  tablePtr.i = tableId;
  ptrCheckGuard(tablePtr, ctabrecFileSize, tablerec);
  tablePtr.p->schemaVersion = tableVersion;

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

/* ************************************************************************>> 
 * TIME_SIGNAL: Handles time-out of local operations. This is a clean-up 
 * handler. If no other measure has succeeded in cleaning up after time-outs 
 * or else then this routine will remove the transaction after 120 seconds of 
 * inactivity. The check is performed once per 10 second. Sender is QMGR.
 * ************************************************************************>> */
void Dblqh::execTIME_SIGNAL(Signal* signal)
{
  jamEntry();
  cLqhTimeOutCount++;
  cLqhTimeOutCheckCount++;
  if ((cCounterAccCommitBlocked > 0) ||
     (cCounterTupCommitBlocked > 0)) {
    jam();
    signal->theData[0] = NDB_LE_UndoLogBlocked;
    signal->theData[1] = cCounterTupCommitBlocked;
    signal->theData[2] = cCounterAccCommitBlocked;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 3, JBB);

    cCounterTupCommitBlocked = 0;
    cCounterAccCommitBlocked = 0;
  }//if
  if (cLqhTimeOutCheckCount < 10) {
    jam();
    return;
  }//if
  cLqhTimeOutCheckCount = 0;
#ifdef VM_TRACE
  TcConnectionrecPtr tTcConptr;
  
  for (tTcConptr.i = 0; tTcConptr.i < ctcConnectrecFileSize;
       tTcConptr.i++) {
    jam();
    ptrAss(tTcConptr, tcConnectionrec);
    if ((tTcConptr.p->tcTimer != 0) &&
	((tTcConptr.p->tcTimer + 120) < cLqhTimeOutCount)) {
      ndbout << "Dblqh::execTIME_SIGNAL"<<endl
	     << "Timeout found in tcConnectRecord " <<tTcConptr.i<<endl
	     << " cLqhTimeOutCount = " << cLqhTimeOutCount << endl
	     << " tcTimer="<<tTcConptr.p->tcTimer<<endl
	     << " tcTimer+120="<<tTcConptr.p->tcTimer + 120<<endl;
	  
      ndbout << " transactionState = " << tTcConptr.p->transactionState<<endl;
      ndbout << " operation = " << tTcConptr.p->operation<<endl;
      ndbout << " tcNodeFailrec = " << tTcConptr.p->tcNodeFailrec
	     << " seqNoReplica = " << tTcConptr.p->seqNoReplica
	     << " simpleRead = " << tTcConptr.p->simpleRead
	     << endl;
      ndbout << " replicaType = " << tTcConptr.p->replicaType
	     << " reclenAiLqhkey = " << tTcConptr.p->reclenAiLqhkey
	     << " opExec = " << tTcConptr.p->opExec
	     << endl;
      ndbout << " opSimple = " << tTcConptr.p->opSimple
	     << " nextSeqNoReplica = " << tTcConptr.p->nextSeqNoReplica
	     << " lockType = " << tTcConptr.p->lockType
	     << " localFragptr = " << tTcConptr.p->localFragptr
	     << endl;
      ndbout << " lastReplicaNo = " << tTcConptr.p->lastReplicaNo
	     << " indTakeOver = " << tTcConptr.p->indTakeOver
	     << " dirtyOp = " << tTcConptr.p->dirtyOp
	     << endl;
      ndbout << " activeCreat = " << tTcConptr.p->activeCreat
	     << " tcBlockref = " << hex << tTcConptr.p->tcBlockref
	     << " reqBlockref = " << hex << tTcConptr.p->reqBlockref
	     << " primKeyLen = " << tTcConptr.p->primKeyLen
	     << endl;
      ndbout << " nextReplica = " << tTcConptr.p->nextReplica
	     << " tcBlockref = " << hex << tTcConptr.p->tcBlockref
	     << " reqBlockref = " << hex << tTcConptr.p->reqBlockref
	     << " primKeyLen = " << tTcConptr.p->primKeyLen
	     << endl;
      ndbout << " logStopPageNo = " << tTcConptr.p->logStopPageNo
	     << " logStartPageNo = " << tTcConptr.p->logStartPageNo
	     << " logStartPageIndex = " << tTcConptr.p->logStartPageIndex
	     << endl;
      ndbout << " errorCode = " << tTcConptr.p->errorCode
	     << " clientBlockref = " << hex << tTcConptr.p->clientBlockref
	     << " applRef = " << hex << tTcConptr.p->applRef
	     << " totSendlenAi = " << tTcConptr.p->totSendlenAi
	     << endl;
      ndbout << " totReclenAi = " << tTcConptr.p->totReclenAi
	     << " tcScanRec = " << tTcConptr.p->tcScanRec
	     << " tcScanInfo = " << tTcConptr.p->tcScanInfo
	     << " tcOprec = " << hex << tTcConptr.p->tcOprec
	     << endl;
      ndbout << " tableref = " << tTcConptr.p->tableref
	     << " simpleTcConnect = " << tTcConptr.p->simpleTcConnect
	     << " storedProcId = " << tTcConptr.p->storedProcId
	     << " schemaVersion = " << tTcConptr.p->schemaVersion
	     << endl;
      ndbout << " reqinfo = " << tTcConptr.p->reqinfo
	     << " reqRef = " << tTcConptr.p->reqRef
	     << " readlenAi = " << tTcConptr.p->readlenAi
	     << " prevTc = " << tTcConptr.p->prevTc
	     << endl;
      ndbout << " prevLogTcrec = " << tTcConptr.p->prevLogTcrec
	     << " prevHashRec = " << tTcConptr.p->prevHashRec
	     << " nodeAfterNext0 = " << tTcConptr.p->nodeAfterNext[0]
	     << " nodeAfterNext1 = " << tTcConptr.p->nodeAfterNext[1]
	     << endl;
      ndbout << " nextTcConnectrec = " << tTcConptr.p->nextTcConnectrec
	     << " nextTc = " << tTcConptr.p->nextTc
	     << " nextTcLogQueue = " << tTcConptr.p->nextTcLogQueue
	     << " nextLogTcrec = " << tTcConptr.p->nextLogTcrec
	     << endl;
      ndbout << " nextHashRec = " << tTcConptr.p->nextHashRec
	     << " logWriteState = " << tTcConptr.p->logWriteState
	     << " logStartFileNo = " << tTcConptr.p->logStartFileNo
	     << " listState = " << tTcConptr.p->listState
	     << endl;
      ndbout << " lastAttrinbuf = " << tTcConptr.p->lastAttrinbuf
	     << " lastTupkeybuf = " << tTcConptr.p->lastTupkeybuf
	     << " hashValue = " << tTcConptr.p->hashValue
	     << endl;
      ndbout << " gci = " << tTcConptr.p->gci
	     << " fragmentptr = " << tTcConptr.p->fragmentptr
	     << " fragmentid = " << tTcConptr.p->fragmentid
	     << " firstTupkeybuf = " << tTcConptr.p->firstTupkeybuf
	     << endl;
      ndbout << " firstAttrinbuf = " << tTcConptr.p->firstAttrinbuf
	     << " currTupAiLen = " << tTcConptr.p->currTupAiLen
	     << " currReclenAi = " << tTcConptr.p->currReclenAi
	     << endl;
      ndbout << " tcTimer = " << tTcConptr.p->tcTimer
	     << " clientConnectrec = " << tTcConptr.p->clientConnectrec
	     << " applOprec = " << hex << tTcConptr.p->applOprec
	     << " abortState = " << tTcConptr.p->abortState
	     << endl;
      ndbout << " transid0 = " << hex << tTcConptr.p->transid[0]
	     << " transid1 = " << hex << tTcConptr.p->transid[1]
	     << " tupkeyData0 = " << tTcConptr.p->tupkeyData[0]
	     << " tupkeyData1 = " << tTcConptr.p->tupkeyData[1]
	     << endl;
      ndbout << " tupkeyData2 = " << tTcConptr.p->tupkeyData[2]
	     << " tupkeyData3 = " << tTcConptr.p->tupkeyData[3]
	     << endl;
      switch (tTcConptr.p->transactionState) {
	
      case TcConnectionrec::SCAN_STATE_USED:
	if (tTcConptr.p->tcScanRec < cscanrecFileSize){
	  ScanRecordPtr TscanPtr;
	  c_scanRecordPool.getPtr(TscanPtr, tTcConptr.p->tcScanRec);
	  ndbout << " scanState = " << TscanPtr.p->scanState << endl;
	  //TscanPtr.p->scanLocalref[2];
	  ndbout << " copyPtr="<<TscanPtr.p->copyPtr
		 << " scanAccPtr="<<TscanPtr.p->scanAccPtr
		 << " scanAiLength="<<TscanPtr.p->scanAiLength
		 << endl;
	  ndbout << " m_curr_batch_size_rows="<<
	    TscanPtr.p->m_curr_batch_size_rows
		 << " m_max_batch_size_rows="<<
	    TscanPtr.p->m_max_batch_size_rows
		 << " scanErrorCounter="<<TscanPtr.p->scanErrorCounter
		 << " scanLocalFragid="<<TscanPtr.p->scanLocalFragid
		 << endl;
	  ndbout << " scanSchemaVersion="<<TscanPtr.p->scanSchemaVersion
		 << "  scanStoredProcId="<<TscanPtr.p->scanStoredProcId
		 << "  scanTcrec="<<TscanPtr.p->scanTcrec
		 << endl;
	  ndbout << "  scanType="<<TscanPtr.p->scanType
		 << "  scanApiBlockref="<<TscanPtr.p->scanApiBlockref
		 << "  scanNodeId="<<TscanPtr.p->scanNodeId
		 << "  scanCompletedStatus="<<TscanPtr.p->scanCompletedStatus
		 << endl;
	  ndbout << "  scanFlag="<<TscanPtr.p->scanFlag
		 << "  scanLockHold="<<TscanPtr.p->scanLockHold
		 << "  scanLockMode="<<TscanPtr.p->scanLockMode
		 << "  scanNumber="<<TscanPtr.p->scanNumber
		 << endl;
	  ndbout << "  scanReleaseCounter="<<TscanPtr.p->scanReleaseCounter
		 << "  scanTcWaiting="<<TscanPtr.p->scanTcWaiting
		 << "  scanKeyinfoFlag="<<TscanPtr.p->scanKeyinfoFlag
		 << endl;
	}else{
	  ndbout << "No connected scan record found" << endl;
	}
	break;
      default:
        break;
      }//switch

      // Reset the timer 
      tTcConptr.p->tcTimer = 0;
    }//if
  }//for
#endif
#ifdef VM_TRACE
  for (lfoPtr.i = 0; lfoPtr.i < clfoFileSize; lfoPtr.i++) {
    ptrAss(lfoPtr, logFileOperationRecord);
    if ((lfoPtr.p->lfoTimer != 0) &&
        ((lfoPtr.p->lfoTimer + 120) < cLqhTimeOutCount)) {
      ndbout << "We have lost LFO record" << endl;
      ndbout << "index = " << lfoPtr.i;
      ndbout << "State = " << lfoPtr.p->lfoState;
      ndbout << " Page No = " << lfoPtr.p->lfoPageNo;
      ndbout << " noPagesRw = " << lfoPtr.p->noPagesRw;
      ndbout << "lfoWordWritten = " << lfoPtr.p->lfoWordWritten << endl;
      lfoPtr.p->lfoTimer = cLqhTimeOutCount;
    }//if
  }//for

#endif

#if 0
  LcpRecordPtr TlcpPtr;
  // Print information about the current local checkpoint
  TlcpPtr.i = 0;
  ptrAss(TlcpPtr, lcpRecord);
  ndbout << "Information about LCP in this LQH" << endl
	 << "  lcpState="<<TlcpPtr.p->lcpState<<endl
	 << "   firstLcpLocAcc="<<TlcpPtr.p->firstLcpLocAcc<<endl
	 << "   firstLcpLocTup="<<TlcpPtr.p->firstLcpLocTup<<endl
	 << "   lcpAccptr="<<TlcpPtr.p->lcpAccptr<<endl
	 << "   lastFragmentFlag="<<TlcpPtr.p->lastFragmentFlag<<endl
	 << "   lcpQueued="<<TlcpPtr.p->lcpQueued<<endl
	 << "   reportEmptyref="<< TlcpPtr.p->reportEmptyRef<<endl
	 << "   reportEmpty="<<TlcpPtr.p->reportEmpty<<endl;
#endif
}//Dblqh::execTIME_SIGNAL()

/* ######################################################################### */
/* #######                  EXECUTION MODULE                         ####### */
/* THIS MODULE HANDLES THE RECEPTION OF LQHKEYREQ AND ALL PROCESSING         */
/* OF OPERATIONS ON BEHALF OF THIS REQUEST. THIS DOES ALSO INVOLVE           */
/* RECEPTION OF VARIOUS TYPES OF ATTRINFO AND KEYINFO. IT DOES ALSO          */
/* INVOLVE COMMUNICATION WITH ACC AND TUP.                                   */
/* ######################################################################### */

void Dblqh::noFreeRecordLab(Signal* signal, 
			    const LqhKeyReq * lqhKeyReq,
			    Uint32 errCode) 
{
  jamEntry();
  const Uint32 transid1  = lqhKeyReq->transId1;
  const Uint32 transid2  = lqhKeyReq->transId2;
  const Uint32 reqInfo   = lqhKeyReq->requestInfo;
  
  if(errCode == ZNO_FREE_MARKER_RECORDS_ERROR ||
     errCode == ZNODE_SHUTDOWN_IN_PROGESS){
    releaseTcrec(signal, tcConnectptr);
  }

  if (LqhKeyReq::getSimpleFlag(reqInfo) && 
      LqhKeyReq::getOperation(reqInfo) == ZREAD){ 
    jam();
    ndbrequire(LqhKeyReq::getApplicationAddressFlag(reqInfo));
    const Uint32 apiRef   = lqhKeyReq->variableData[0];
    const Uint32 apiOpRec = lqhKeyReq->variableData[1];

    TcKeyRef * const tcKeyRef = (TcKeyRef *) signal->getDataPtrSend();
    
    tcKeyRef->connectPtr = apiOpRec;
    tcKeyRef->transId[0] = transid1;
    tcKeyRef->transId[1] = transid2;
    tcKeyRef->errorCode = errCode;
    sendSignal(apiRef, GSN_TCKEYREF, signal, TcKeyRef::SignalLength, JBB);
  } else {
    jam();

    const Uint32 clientPtr = lqhKeyReq->clientConnectPtr;
    Uint32 TcOprec = clientPtr;
    if(LqhKeyReq::getSameClientAndTcFlag(reqInfo) == 1){
      if(LqhKeyReq::getApplicationAddressFlag(reqInfo))
	TcOprec = lqhKeyReq->variableData[2];
      else
	TcOprec = lqhKeyReq->variableData[0];
    }

    LqhKeyRef * const ref = (LqhKeyRef*)signal->getDataPtrSend();
    ref->userRef = clientPtr;
    ref->connectPtr = TcOprec;
    ref->errorCode = errCode;
    ref->transId1 = transid1;
    ref->transId2 = transid2;
    sendSignal(signal->senderBlockRef(), GSN_LQHKEYREF, signal, 
	       LqhKeyRef::SignalLength, JBB);
  }//if
  return;
}//Dblqh::noFreeRecordLab()

void Dblqh::LQHKEY_abort(Signal* signal, int errortype)
{
  switch (errortype) {
  case 0:
    jam();
    terrorCode = ZCOPY_NODE_ERROR;
    break;
  case 1:
    jam();
    terrorCode = ZNO_FREE_LQH_CONNECTION;
    break;
  case 2:
    jam();
    terrorCode = signal->theData[1];
    break;
  case 3:
    jam();
    ndbrequire((tcConnectptr.p->transactionState == TcConnectionrec::WAIT_ACC_ABORT) ||
               (tcConnectptr.p->transactionState == TcConnectionrec::ABORT_STOPPED)  ||
               (tcConnectptr.p->transactionState == TcConnectionrec::ABORT_QUEUED));
    return;
    break;
  case 4:
    jam();
    if(tabptr.p->tableStatus == Tablerec::NOT_DEFINED){
      jam();
      terrorCode = ZTABLE_NOT_DEFINED;
    } else if (tabptr.p->tableStatus == Tablerec::PREP_DROP_TABLE_ONGOING ||
	       tabptr.p->tableStatus == Tablerec::PREP_DROP_TABLE_DONE){
      jam();
      terrorCode = ZDROP_TABLE_IN_PROGRESS;
    } else {
      ndbrequire(0);
    }
    break;
  case 5:
    jam();
    terrorCode = ZINVALID_SCHEMA_VERSION;
    break;
  default:
    ndbrequire(false);
    break;
  }//switch
  abortErrorLab(signal);
}//Dblqh::LQHKEY_abort()

void Dblqh::LQHKEY_error(Signal* signal, int errortype)
{
  switch (errortype) {
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
  ndbrequire(false);
}//Dblqh::LQHKEY_error()

void Dblqh::execLQHKEYREF(Signal* signal) 
{
  jamEntry();
  tcConnectptr.i = signal->theData[0];
  terrorCode = signal->theData[2];
  Uint32 transid1 = signal->theData[3];
  Uint32 transid2 = signal->theData[4];
  if (tcConnectptr.i >= ctcConnectrecFileSize) {
    errorReport(signal, 3);
    return;
  }//if
/*------------------------------------------------------------------*/
/*       WE HAVE TO CHECK THAT THE SIGNAL DO NOT BELONG TO SOMETHING*/
/*       REMOVED DUE TO A TIME-OUT.                                 */
/*------------------------------------------------------------------*/
  ptrAss(tcConnectptr, tcConnectionrec);
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  switch (regTcPtr->connectState) {
  case TcConnectionrec::CONNECTED:
    jam();
    if ((regTcPtr->transid[0] != transid1) ||
        (regTcPtr->transid[1] != transid2)) {
      warningReport(signal, 14);
      return;
    }//if
    if (regTcPtr->abortState != TcConnectionrec::ABORT_IDLE) {
      warningReport(signal, 15);
      return;
    }//if
    abortErrorLab(signal);
    return;
    break;
  case TcConnectionrec::LOG_CONNECTED:
    jam();
    logLqhkeyrefLab(signal);
    return;
    break;
  case TcConnectionrec::COPY_CONNECTED:
    jam();
    copyLqhKeyRefLab(signal);
    return;
    break;
  default:
    warningReport(signal, 16);
    return;
    break;
  }//switch
}//Dblqh::execLQHKEYREF()

/* -------------------------------------------------------------------------- */
/* -------                       ENTER PACKED_SIGNAL                  ------- */
/* Execution of packed signal. The packed signal can contain COMMIT, COMPLETE */
/* or LQHKEYCONF signals. These signals will be executed by their resp. exec  */
/* functions.                                                                 */
/* -------------------------------------------------------------------------- */
void Dblqh::execPACKED_SIGNAL(Signal* signal) 
{
  Uint32 Tstep = 0;
  Uint32 Tlength;
  Uint32 TpackedData[28];
  Uint32 sig0, sig1, sig2, sig3 ,sig4, sig5, sig6;

  jamEntry();
  Tlength = signal->length();
  ndbrequire(Tlength <= 25);
  MEMCOPY_NO_WORDS(&TpackedData[0], &signal->theData[0], Tlength);
  while (Tlength > Tstep) {
    switch (TpackedData[Tstep] >> 28) {
    case ZCOMMIT:
      jam();
      sig0 = TpackedData[Tstep + 0] & 0x0FFFFFFF;
      sig1 = TpackedData[Tstep + 1];
      sig2 = TpackedData[Tstep + 2];
      sig3 = TpackedData[Tstep + 3];
      signal->theData[0] = sig0;
      signal->theData[1] = sig1;
      signal->theData[2] = sig2;
      signal->theData[3] = sig3;
      signal->header.theLength = 4;
      execCOMMIT(signal);
      Tstep += 4;
      break;
    case ZCOMPLETE:
      jam();
      sig0 = TpackedData[Tstep + 0] & 0x0FFFFFFF;
      sig1 = TpackedData[Tstep + 1];
      sig2 = TpackedData[Tstep + 2];
      signal->theData[0] = sig0;
      signal->theData[1] = sig1;
      signal->theData[2] = sig2;
      signal->header.theLength = 3;
      execCOMPLETE(signal);
      Tstep += 3;
      break;
    case ZLQHKEYCONF: {
      jam();
      LqhKeyConf * const lqhKeyConf = (LqhKeyConf *)signal->getDataPtr();

      sig0 = TpackedData[Tstep + 0] & 0x0FFFFFFF;
      sig1 = TpackedData[Tstep + 1];
      sig2 = TpackedData[Tstep + 2];
      sig3 = TpackedData[Tstep + 3];
      sig4 = TpackedData[Tstep + 4];
      sig5 = TpackedData[Tstep + 5];
      sig6 = TpackedData[Tstep + 6];
      lqhKeyConf->connectPtr = sig0;
      lqhKeyConf->opPtr = sig1;
      lqhKeyConf->userRef = sig2;
      lqhKeyConf->readLen = sig3;
      lqhKeyConf->transId1 = sig4;
      lqhKeyConf->transId2 = sig5;
      lqhKeyConf->noFiredTriggers = sig6;
      execLQHKEYCONF(signal);
      Tstep += LqhKeyConf::SignalLength;
      break;
    }
    case ZREMOVE_MARKER:
      jam();
      sig0 = TpackedData[Tstep + 1];
      sig1 = TpackedData[Tstep + 2];
      signal->theData[0] = sig0;
      signal->theData[1] = sig1;
      signal->header.theLength = 2;
      execREMOVE_MARKER_ORD(signal);
      Tstep += 3;
      break;
    default:
      ndbrequire(false);
      return;
    }//switch
  }//while
  ndbrequire(Tlength == Tstep);
  return;
}//Dblqh::execPACKED_SIGNAL()

void
Dblqh::execREMOVE_MARKER_ORD(Signal* signal)
{  
  CommitAckMarker key;
  key.transid1 = signal->theData[0];
  key.transid2 = signal->theData[1];
  jamEntry();
  
  CommitAckMarkerPtr removedPtr;
  m_commitAckMarkerHash.release(removedPtr, key);
  ndbrequire(removedPtr.i != RNIL);
#ifdef MARKER_TRACE
  ndbout_c("Rem marker[%.8x %.8x]", key.transid1, key.transid2);
#endif
}


/* -------------------------------------------------------------------------- */
/* -------                 ENTER SEND_PACKED                          ------- */
/* Used to force a packed signal to be sent if local signal buffer is not     */
/* empty.                                                                     */
/* -------------------------------------------------------------------------- */
void Dblqh::execSEND_PACKED(Signal* signal) 
{
  HostRecordPtr Thostptr;
  UintR i;
  UintR TpackedListIndex = cpackedListIndex;
  jamEntry();
  for (i = 0; i < TpackedListIndex; i++) {
    Thostptr.i = cpackedList[i];
    ptrAss(Thostptr, hostRecord);
    jam();
    ndbrequire(Thostptr.i - 1 < MAX_NDB_NODES - 1);
    if (Thostptr.p->noOfPackedWordsLqh > 0) {
      jam();
      sendPackedSignalLqh(signal, Thostptr.p);
    }//if
    if (Thostptr.p->noOfPackedWordsTc > 0) {
      jam();
      sendPackedSignalTc(signal, Thostptr.p);
    }//if
    Thostptr.p->inPackedList = false;
  }//for
  cpackedListIndex = 0;
  return;
}//Dblqh::execSEND_PACKED()

void
Dblqh::updatePackedList(Signal* signal, HostRecord * ahostptr, Uint16 hostId)
{
  Uint32 TpackedListIndex = cpackedListIndex;
  if (ahostptr->inPackedList == false) {
    jam();
    ahostptr->inPackedList = true;
    cpackedList[TpackedListIndex] = hostId;
    cpackedListIndex = TpackedListIndex + 1;
  }//if
}//Dblqh::updatePackedList()

void
Dblqh::execREAD_PSEUDO_REQ(Signal* signal){
  jamEntry();
  TcConnectionrecPtr regTcPtr;
  regTcPtr.i = signal->theData[0];
  ptrCheckGuard(regTcPtr, ctcConnectrecFileSize, tcConnectionrec);
  
  if(signal->theData[1] != AttributeHeader::RANGE_NO)
  {
    jam();
    FragrecordPtr regFragptr;
    regFragptr.i = regTcPtr.p->fragmentptr;
    ptrCheckGuard(regFragptr, cfragrecFileSize, fragrecord);
    
    signal->theData[0] = regFragptr.p->accFragptr[regTcPtr.p->localFragptr];
    EXECUTE_DIRECT(DBACC, GSN_READ_PSEUDO_REQ, signal, 2);
  }
  else
  {
    signal->theData[0] = regTcPtr.p->m_scan_curr_range_no;
  }
}

/* ************>> */
/*  TUPKEYCONF  > */
/* ************>> */
void Dblqh::execTUPKEYCONF(Signal* signal) 
{
  TcConnectionrec *regTcConnectionrec = tcConnectionrec;
  Uint32 ttcConnectrecFileSize = ctcConnectrecFileSize;
  const TupKeyConf * const tupKeyConf = (TupKeyConf *)signal->getDataPtr();
  Uint32 tcIndex = tupKeyConf->userPtr;
  jamEntry();
  tcConnectptr.i = tcIndex;
  ptrCheckGuard(tcConnectptr, ttcConnectrecFileSize, regTcConnectionrec);
  switch (tcConnectptr.p->transactionState) {
  case TcConnectionrec::WAIT_TUP:
    jam();
    if (tcConnectptr.p->seqNoReplica == 0) // Primary replica
      tcConnectptr.p->noFiredTriggers = tupKeyConf->noFiredTriggers;
    tupkeyConfLab(signal);
    break;
  case TcConnectionrec::COPY_TUPKEY:
    jam();
    copyTupkeyConfLab(signal);
    break;
  case TcConnectionrec::SCAN_TUPKEY:
    jam();
    scanTupkeyConfLab(signal);
    break;
  case TcConnectionrec::WAIT_TUP_TO_ABORT:
    jam();
/* ------------------------------------------------------------------------- */
// Abort was not ready to start until this signal came back. Now we are ready
// to start the abort.
/* ------------------------------------------------------------------------- */
    releaseActiveFrag(signal);
    abortCommonLab(signal);
    break;
  case TcConnectionrec::WAIT_ACC_ABORT:
  case TcConnectionrec::ABORT_QUEUED:
    jam();
/* -------------------------------------------------------------------------- */
/*       IGNORE SINCE ABORT OF THIS OPERATION IS ONGOING ALREADY.             */
/* -------------------------------------------------------------------------- */
    break;
  default:
    ndbrequire(false);
    break;
  }//switch
}//Dblqh::execTUPKEYCONF()

/* ************> */
/*  TUPKEYREF  > */
/* ************> */
void Dblqh::execTUPKEYREF(Signal* signal) 
{
  const TupKeyRef * const tupKeyRef = (TupKeyRef *)signal->getDataPtr();

  jamEntry();
  tcConnectptr.i = tupKeyRef->userRef;
  terrorCode = tupKeyRef->errorCode;
  ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
  switch (tcConnectptr.p->transactionState) {
  case TcConnectionrec::WAIT_TUP:
    jam();
    releaseActiveFrag(signal);
    abortErrorLab(signal);
    break;
  case TcConnectionrec::COPY_TUPKEY:
    ndbrequire(false);
    break;
  case TcConnectionrec::SCAN_TUPKEY:
    jam();
    scanTupkeyRefLab(signal);
    break;
  case TcConnectionrec::WAIT_TUP_TO_ABORT:
    jam();
/* ------------------------------------------------------------------------- */
// Abort was not ready to start until this signal came back. Now we are ready
// to start the abort.
/* ------------------------------------------------------------------------- */
    releaseActiveFrag(signal);
    abortCommonLab(signal);
    break;
  case TcConnectionrec::WAIT_ACC_ABORT:
  case TcConnectionrec::ABORT_QUEUED:
    jam();
/* ------------------------------------------------------------------------- */
/*       IGNORE SINCE ABORT OF THIS OPERATION IS ONGOING ALREADY.            */
/* ------------------------------------------------------------------------- */
    break;
  default:
    ndbrequire(false);
    break;
  }//switch
}//Dblqh::execTUPKEYREF()

void Dblqh::sendPackedSignalLqh(Signal* signal, HostRecord * ahostptr)
{
  Uint32 noOfWords = ahostptr->noOfPackedWordsLqh;
  BlockReference hostRef = ahostptr->hostLqhBlockRef;
  MEMCOPY_NO_WORDS(&signal->theData[0],
                   &ahostptr->packedWordsLqh[0],
                   noOfWords);
  sendSignal(hostRef, GSN_PACKED_SIGNAL, signal, noOfWords, JBB);
  ahostptr->noOfPackedWordsLqh = 0;
}//Dblqh::sendPackedSignalLqh()

void Dblqh::sendPackedSignalTc(Signal* signal, HostRecord * ahostptr)
{
  Uint32 noOfWords = ahostptr->noOfPackedWordsTc;
  BlockReference hostRef = ahostptr->hostTcBlockRef;
  MEMCOPY_NO_WORDS(&signal->theData[0],
                   &ahostptr->packedWordsTc[0],
                   noOfWords);
  sendSignal(hostRef, GSN_PACKED_SIGNAL, signal, noOfWords, JBB);
  ahostptr->noOfPackedWordsTc = 0;
}//Dblqh::sendPackedSignalTc()

void Dblqh::sendCommitLqh(Signal* signal, BlockReference alqhBlockref)
{
  HostRecordPtr Thostptr;
  Thostptr.i = refToNode(alqhBlockref);
  ptrCheckGuard(Thostptr, chostFileSize, hostRecord);
  if (Thostptr.p->noOfPackedWordsLqh > 21) {
    jam();
    sendPackedSignalLqh(signal, Thostptr.p);
  } else {
    jam();
    updatePackedList(signal, Thostptr.p, Thostptr.i);
  }//if
  Uint32 pos = Thostptr.p->noOfPackedWordsLqh;
  Uint32 ptrAndType = tcConnectptr.p->clientConnectrec | (ZCOMMIT << 28);
  Uint32 gci = tcConnectptr.p->gci;
  Uint32 transid1 = tcConnectptr.p->transid[0];
  Uint32 transid2 = tcConnectptr.p->transid[1];
  Thostptr.p->packedWordsLqh[pos] = ptrAndType;
  Thostptr.p->packedWordsLqh[pos + 1] = gci;
  Thostptr.p->packedWordsLqh[pos + 2] = transid1;
  Thostptr.p->packedWordsLqh[pos + 3] = transid2;
  Thostptr.p->noOfPackedWordsLqh = pos + 4;
}//Dblqh::sendCommitLqh()

void Dblqh::sendCompleteLqh(Signal* signal, BlockReference alqhBlockref)
{
  HostRecordPtr Thostptr;
  Thostptr.i = refToNode(alqhBlockref);
  ptrCheckGuard(Thostptr, chostFileSize, hostRecord);
  if (Thostptr.p->noOfPackedWordsLqh > 22) {
    jam();
    sendPackedSignalLqh(signal, Thostptr.p);
  } else {
    jam();
    updatePackedList(signal, Thostptr.p, Thostptr.i);
  }//if
  Uint32 pos = Thostptr.p->noOfPackedWordsLqh;
  Uint32 ptrAndType = tcConnectptr.p->clientConnectrec | (ZCOMPLETE << 28);
  Uint32 transid1 = tcConnectptr.p->transid[0];
  Uint32 transid2 = tcConnectptr.p->transid[1];
  Thostptr.p->packedWordsLqh[pos] = ptrAndType;
  Thostptr.p->packedWordsLqh[pos + 1] = transid1;
  Thostptr.p->packedWordsLqh[pos + 2] = transid2;
  Thostptr.p->noOfPackedWordsLqh = pos + 3;
}//Dblqh::sendCompleteLqh()

void Dblqh::sendCommittedTc(Signal* signal, BlockReference atcBlockref)
{
  HostRecordPtr Thostptr;
  Thostptr.i = refToNode(atcBlockref);
  ptrCheckGuard(Thostptr, chostFileSize, hostRecord);
  if (Thostptr.p->noOfPackedWordsTc > 22) {
    jam();
    sendPackedSignalTc(signal, Thostptr.p);
  } else {
    jam();
    updatePackedList(signal, Thostptr.p, Thostptr.i);
  }//if
  Uint32 pos = Thostptr.p->noOfPackedWordsTc;
  Uint32 ptrAndType = tcConnectptr.p->clientConnectrec | (ZCOMMITTED << 28);
  Uint32 transid1 = tcConnectptr.p->transid[0];
  Uint32 transid2 = tcConnectptr.p->transid[1];
  Thostptr.p->packedWordsTc[pos] = ptrAndType;
  Thostptr.p->packedWordsTc[pos + 1] = transid1;
  Thostptr.p->packedWordsTc[pos + 2] = transid2;
  Thostptr.p->noOfPackedWordsTc = pos + 3;
}//Dblqh::sendCommittedTc()

void Dblqh::sendCompletedTc(Signal* signal, BlockReference atcBlockref)
{
  HostRecordPtr Thostptr;
  Thostptr.i = refToNode(atcBlockref);
  ptrCheckGuard(Thostptr, chostFileSize, hostRecord);
  if (Thostptr.p->noOfPackedWordsTc > 22) {
    jam();
    sendPackedSignalTc(signal, Thostptr.p);
  } else {
    jam();
    updatePackedList(signal, Thostptr.p, Thostptr.i);
  }//if
  Uint32 pos = Thostptr.p->noOfPackedWordsTc;
  Uint32 ptrAndType = tcConnectptr.p->clientConnectrec | (ZCOMPLETED << 28);
  Uint32 transid1 = tcConnectptr.p->transid[0];
  Uint32 transid2 = tcConnectptr.p->transid[1];
  Thostptr.p->packedWordsTc[pos] = ptrAndType;
  Thostptr.p->packedWordsTc[pos + 1] = transid1;
  Thostptr.p->packedWordsTc[pos + 2] = transid2;
  Thostptr.p->noOfPackedWordsTc = pos + 3;
}//Dblqh::sendCompletedTc()

void Dblqh::sendLqhkeyconfTc(Signal* signal, BlockReference atcBlockref)
{
  LqhKeyConf* lqhKeyConf;
  HostRecordPtr Thostptr;

  Thostptr.i = refToNode(atcBlockref);
  ptrCheckGuard(Thostptr, chostFileSize, hostRecord);
  if (refToBlock(atcBlockref) == DBTC) {
    jam();
/*******************************************************************
// This signal was intended for DBTC as part of the normal transaction
// execution.
********************************************************************/
    if (Thostptr.p->noOfPackedWordsTc > (25 - LqhKeyConf::SignalLength)) {
      jam();
      sendPackedSignalTc(signal, Thostptr.p);
    } else {
      jam();
      updatePackedList(signal, Thostptr.p, Thostptr.i);
    }//if
    lqhKeyConf = (LqhKeyConf *)
      &Thostptr.p->packedWordsTc[Thostptr.p->noOfPackedWordsTc];
    Thostptr.p->noOfPackedWordsTc += LqhKeyConf::SignalLength;
  } else {
    jam();
/*******************************************************************
// This signal was intended for DBLQH as part of log execution or
// node recovery.
********************************************************************/
    if (Thostptr.p->noOfPackedWordsLqh > (25 - LqhKeyConf::SignalLength)) {
      jam();
      sendPackedSignalLqh(signal, Thostptr.p);
    } else {
      jam();
      updatePackedList(signal, Thostptr.p, Thostptr.i);
    }//if
    lqhKeyConf = (LqhKeyConf *)
      &Thostptr.p->packedWordsLqh[Thostptr.p->noOfPackedWordsLqh];
    Thostptr.p->noOfPackedWordsLqh += LqhKeyConf::SignalLength;
  }//if
  Uint32 ptrAndType = tcConnectptr.i | (ZLQHKEYCONF << 28);
  Uint32 tcOprec = tcConnectptr.p->tcOprec;
  Uint32 ownRef = cownref;
  Uint32 readlenAi = tcConnectptr.p->readlenAi;
  Uint32 transid1 = tcConnectptr.p->transid[0];
  Uint32 transid2 = tcConnectptr.p->transid[1];
  Uint32 noFiredTriggers = tcConnectptr.p->noFiredTriggers;
  lqhKeyConf->connectPtr = ptrAndType;
  lqhKeyConf->opPtr = tcOprec;
  lqhKeyConf->userRef = ownRef;
  lqhKeyConf->readLen = readlenAi;
  lqhKeyConf->transId1 = transid1;
  lqhKeyConf->transId2 = transid2;
  lqhKeyConf->noFiredTriggers = noFiredTriggers;
}//Dblqh::sendLqhkeyconfTc()

/* ************************************************************************>>
 * KEYINFO: Get tuple request from DBTC. Next step is to contact DBACC to get 
 * key to tuple if all key/attrinfo has been received, else for more attrinfo 
 * signals.      
 * ************************************************************************>> */
void Dblqh::execKEYINFO(Signal* signal) 
{
  Uint32 tcOprec = signal->theData[0];
  Uint32 transid1 = signal->theData[1];
  Uint32 transid2 = signal->theData[2];
  jamEntry();
  if (findTransaction(transid1, transid2, tcOprec) != ZOK) {
    jam();
    return;
  }//if
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  TcConnectionrec::TransactionState state = regTcPtr->transactionState;
  if (state != TcConnectionrec::WAIT_TUPKEYINFO &&
      state != TcConnectionrec::WAIT_SCAN_AI)
  {
    jam();
/*****************************************************************************/
/* TRANSACTION WAS ABORTED, THIS IS MOST LIKELY A SIGNAL BELONGING TO THE    */
/* ABORTED TRANSACTION. THUS IGNORE THE SIGNAL.                              */
/*****************************************************************************/
    return;
  }//if
  Uint32 errorCode = handleLongTupKey(signal,
                                      (Uint32)regTcPtr->save1,
                                      (Uint32)regTcPtr->primKeyLen,
                                      &signal->theData[3]);
  if (errorCode != 0) {
    if (errorCode == 1) {
      jam();
      return;
    }//if
    jam();
    terrorCode = errorCode;
    if(state == TcConnectionrec::WAIT_TUPKEYINFO)
      abortErrorLab(signal);
    else
      abort_scan(signal, regTcPtr->tcScanRec, errorCode);
    return;
  }//if
  if(state == TcConnectionrec::WAIT_TUPKEYINFO)
  {
    FragrecordPtr regFragptr;
    regFragptr.i = regTcPtr->fragmentptr;
    ptrCheckGuard(regFragptr, cfragrecFileSize, fragrecord);
    fragptr = regFragptr;
    endgettupkeyLab(signal);
  }
  return;
}//Dblqh::execKEYINFO()

/* ------------------------------------------------------------------------- */
/* FILL IN KEY DATA INTO DATA BUFFERS.                                       */
/* ------------------------------------------------------------------------- */
Uint32 Dblqh::handleLongTupKey(Signal* signal,
			       Uint32 keyLength,
			       Uint32 primKeyLength,
			       Uint32* dataPtr) 
{
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  Uint32 dataPos = 0;
  while (true) {
    keyLength += 4;
    if (cfirstfreeDatabuf == RNIL) {
      jam();
      return ZGET_DATAREC_ERROR;
    }//if
    seizeTupkeybuf(signal);
    Databuf * const regDataPtr = databufptr.p;
    Uint32 data0 = dataPtr[dataPos];
    Uint32 data1 = dataPtr[dataPos + 1];
    Uint32 data2 = dataPtr[dataPos + 2];
    Uint32 data3 = dataPtr[dataPos + 3];
    regDataPtr->data[0] = data0;
    regDataPtr->data[1] = data1;
    regDataPtr->data[2] = data2;
    regDataPtr->data[3] = data3;
    dataPos += 4;
    if (keyLength < primKeyLength) {
      if (dataPos > 16) {
        jam();
/* SAVE STATE AND WAIT FOR KEYINFO */
        regTcPtr->save1 = keyLength;
        return 1;
      }//if
    } else {
      jam();
      return 0;
    }//if
  }//while
}//Dblqh::handleLongTupKey()

/* ------------------------------------------------------------------------- */
/* -------                HANDLE ATTRINFO SIGNALS                    ------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
/* ************************************************************************>> */
/*  ATTRINFO: Continuation of KEYINFO signal (except for scans that do not use*/
/*  any KEYINFO). When all key and attribute info is received we contact DBACC*/
/*  for index handling.                                                       */
/* ************************************************************************>> */
void Dblqh::execATTRINFO(Signal* signal) 
{
  Uint32 tcOprec = signal->theData[0];
  Uint32 transid1 = signal->theData[1];
  Uint32 transid2 = signal->theData[2];
  jamEntry();
  if (findTransaction(transid1,
                      transid2,
                      tcOprec) != ZOK) {
    jam();
    return;
  }//if
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  Uint32 length = signal->length() - 3;
  Uint32 totReclenAi = regTcPtr->totReclenAi;
  Uint32 currReclenAi = regTcPtr->currReclenAi + length;
  Uint32* dataPtr = &signal->theData[3];
  regTcPtr->currReclenAi = currReclenAi;
  if (totReclenAi == currReclenAi) {
    switch (regTcPtr->transactionState) {
    case TcConnectionrec::WAIT_ATTR:
    {
      Fragrecord *regFragrecord = fragrecord;
      Uint32 fragIndex = regTcPtr->fragmentptr;
      Uint32 tfragrecFileSize = cfragrecFileSize;
      jam();
      fragptr.i = fragIndex;
      ptrCheckGuard(fragptr, tfragrecFileSize, regFragrecord);
      lqhAttrinfoLab(signal, dataPtr, length);
      endgettupkeyLab(signal);
      return;
      break;
    }
    case TcConnectionrec::WAIT_SCAN_AI:
      jam();
      scanAttrinfoLab(signal, dataPtr, length);
      return;
      break;
    case TcConnectionrec::WAIT_TUP_TO_ABORT:
    case TcConnectionrec::LOG_ABORT_QUEUED:
    case TcConnectionrec::ABORT_QUEUED:
    case TcConnectionrec::ABORT_STOPPED:
    case TcConnectionrec::WAIT_ACC_ABORT:
    case TcConnectionrec::WAIT_AI_AFTER_ABORT:
      jam();
      aiStateErrorCheckLab(signal, dataPtr,length);
      return;
      break;
    default:
      jam();
      ndbrequire(regTcPtr->abortState != TcConnectionrec::ABORT_IDLE);
      break;
    }//switch
  } else if (currReclenAi < totReclenAi) {
    jam();
    switch (regTcPtr->transactionState) {
    case TcConnectionrec::WAIT_ATTR:
      jam();
      lqhAttrinfoLab(signal, dataPtr, length);
      return;
      break;
    case TcConnectionrec::WAIT_SCAN_AI:
      jam();
      scanAttrinfoLab(signal, dataPtr, length);
      return;
      break;
    case TcConnectionrec::WAIT_TUP_TO_ABORT:
    case TcConnectionrec::LOG_ABORT_QUEUED:
    case TcConnectionrec::ABORT_QUEUED:
    case TcConnectionrec::ABORT_STOPPED:
    case TcConnectionrec::WAIT_ACC_ABORT:
    case TcConnectionrec::WAIT_AI_AFTER_ABORT:
      jam();
      aiStateErrorCheckLab(signal, dataPtr, length);
      return;
      break;
    default:
      jam();
      ndbrequire(regTcPtr->abortState != TcConnectionrec::ABORT_IDLE);
      break;
    }//switch
  } else {
    switch (regTcPtr->transactionState) {
    case TcConnectionrec::WAIT_SCAN_AI:
      jam();
      scanAttrinfoLab(signal, dataPtr, length);
      return;
      break;
    default:
      ndbout_c("%d", regTcPtr->transactionState);
      ndbrequire(false);
      break;
    }//switch
  }//if
  return;
}//Dblqh::execATTRINFO()

/* ************************************************************************>> */
/*  TUP_ATTRINFO: Interpreted execution in DBTUP generates redo-log info      */
/*  which is sent back to DBLQH for logging. This is because the decision     */
/*  to execute or not is made in DBTUP and thus we cannot start logging until */
/*  DBTUP part has been run.                                                  */
/* ************************************************************************>> */
void Dblqh::execTUP_ATTRINFO(Signal* signal) 
{
  TcConnectionrec *regTcConnectionrec = tcConnectionrec;
  Uint32 length = signal->length() - 3;
  Uint32 tcIndex = signal->theData[0];
  Uint32 ttcConnectrecFileSize = ctcConnectrecFileSize;
  jamEntry();
  tcConnectptr.i = tcIndex;
  ptrCheckGuard(tcConnectptr, ttcConnectrecFileSize, regTcConnectionrec);
  ndbrequire(tcConnectptr.p->transactionState == TcConnectionrec::WAIT_TUP);
  if (saveTupattrbuf(signal, &signal->theData[3], length) == ZOK) {
    return;
  } else {
    jam();
/* ------------------------------------------------------------------------- */
/* WE ARE WAITING FOR RESPONSE FROM TUP HERE. THUS WE NEED TO                */
/* GO THROUGH THE STATE MACHINE FOR THE OPERATION.                           */
/* ------------------------------------------------------------------------- */
    localAbortStateHandlerLab(signal);
  }//if
}//Dblqh::execTUP_ATTRINFO()

/* ------------------------------------------------------------------------- */
/* -------                HANDLE ATTRINFO FROM LQH                   ------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void Dblqh::lqhAttrinfoLab(Signal* signal, Uint32* dataPtr, Uint32 length) 
{
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  if (regTcPtr->operation != ZREAD) {
    if (regTcPtr->opExec != 1) {
      if (saveTupattrbuf(signal, dataPtr, length) == ZOK) {
        ;
      } else {
        jam();
/* ------------------------------------------------------------------------- */
/* WE MIGHT BE WAITING FOR RESPONSE FROM SOME BLOCK HERE. THUS WE NEED TO    */
/* GO THROUGH THE STATE MACHINE FOR THE OPERATION.                           */
/* ------------------------------------------------------------------------- */
        localAbortStateHandlerLab(signal);
        return;
      }//if
    }//if
  }//if
  Uint32 sig0 = regTcPtr->tupConnectrec;
  Uint32 blockNo = refToBlock(regTcPtr->tcTupBlockref);
  signal->theData[0] = sig0;
  EXECUTE_DIRECT(blockNo, GSN_ATTRINFO, signal, length + 3);
  jamEntry();
}//Dblqh::lqhAttrinfoLab()

/* ------------------------------------------------------------------------- */
/* ------         FIND TRANSACTION BY USING HASH TABLE               ------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
int Dblqh::findTransaction(UintR Transid1, UintR Transid2, UintR TcOprec) 
{
  TcConnectionrec *regTcConnectionrec = tcConnectionrec;
  Uint32 ttcConnectrecFileSize = ctcConnectrecFileSize;
  TcConnectionrecPtr locTcConnectptr;

  Uint32 ThashIndex = (Transid1 ^ TcOprec) & 1023;
  locTcConnectptr.i = ctransidHash[ThashIndex];
  while (locTcConnectptr.i != RNIL) {
    ptrCheckGuard(locTcConnectptr, ttcConnectrecFileSize, regTcConnectionrec);
    if ((locTcConnectptr.p->transid[0] == Transid1) &&
        (locTcConnectptr.p->transid[1] == Transid2) &&
        (locTcConnectptr.p->tcOprec == TcOprec)) {
/* FIRST PART OF TRANSACTION CORRECT */
/* SECOND PART ALSO CORRECT */
/* THE OPERATION RECORD POINTER IN TC WAS ALSO CORRECT */
      jam();
      tcConnectptr.i = locTcConnectptr.i;
      tcConnectptr.p = locTcConnectptr.p;
      return (int)ZOK;
    }//if
    jam();
/* THIS WAS NOT THE TRANSACTION WHICH WAS SOUGHT */
    locTcConnectptr.i = locTcConnectptr.p->nextHashRec;
  }//while
/* WE DID NOT FIND THE TRANSACTION, REPORT NOT FOUND */
  return (int)ZNOT_FOUND;
}//Dblqh::findTransaction()

/* ------------------------------------------------------------------------- */
/* -------       SAVE ATTRINFO FROM TUP IN ATTRINBUF                 ------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
int Dblqh::saveTupattrbuf(Signal* signal, Uint32* dataPtr, Uint32 length) 
{
  Uint32 tfirstfreeAttrinbuf = cfirstfreeAttrinbuf;
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  Uint32 currTupAiLen = regTcPtr->currTupAiLen;
  if (tfirstfreeAttrinbuf == RNIL) {
    jam();
    terrorCode = ZGET_ATTRINBUF_ERROR;
    return ZGET_ATTRINBUF_ERROR;
  }//if
  seizeAttrinbuf(signal);
  Attrbuf * const regAttrPtr = attrinbufptr.p;
  MEMCOPY_NO_WORDS(&regAttrPtr->attrbuf[0], dataPtr, length);
  regTcPtr->currTupAiLen = currTupAiLen + length;
  regAttrPtr->attrbuf[ZINBUF_DATA_LEN] = length;
  return ZOK;
}//Dblqh::saveTupattrbuf()

/* ==========================================================================
 * =======                       SEIZE ATTRIBUTE IN BUFFER            ======= 
 *
 *       GET A NEW ATTRINBUF AND SETS ATTRINBUFPTR.
 * ========================================================================= */
void Dblqh::seizeAttrinbuf(Signal* signal) 
{
  AttrbufPtr tmpAttrinbufptr;
  AttrbufPtr regAttrinbufptr;
  Attrbuf *regAttrbuf = attrbuf;
  Uint32 tattrinbufFileSize = cattrinbufFileSize;

  regAttrinbufptr.i = seize_attrinbuf();
  tmpAttrinbufptr.i = tcConnectptr.p->lastAttrinbuf;
  ptrCheckGuard(regAttrinbufptr, tattrinbufFileSize, regAttrbuf);
  tcConnectptr.p->lastAttrinbuf = regAttrinbufptr.i;
  regAttrinbufptr.p->attrbuf[ZINBUF_DATA_LEN] = 0;
  if (tmpAttrinbufptr.i == RNIL) {
    jam();
    tcConnectptr.p->firstAttrinbuf = regAttrinbufptr.i;
  } else {
    jam();
    ptrCheckGuard(tmpAttrinbufptr, tattrinbufFileSize, regAttrbuf);
    tmpAttrinbufptr.p->attrbuf[ZINBUF_NEXT] = regAttrinbufptr.i;
  }//if
  regAttrinbufptr.p->attrbuf[ZINBUF_NEXT] = RNIL;
  attrinbufptr = regAttrinbufptr;
}//Dblqh::seizeAttrinbuf()

/* ==========================================================================
 * =======                        SEIZE TC CONNECT RECORD             ======= 
 * 
 *       GETS A NEW TC CONNECT RECORD FROM FREELIST.
 * ========================================================================= */
void Dblqh::seizeTcrec() 
{
  TcConnectionrecPtr locTcConnectptr;

  locTcConnectptr.i = cfirstfreeTcConrec;
  ptrCheckGuard(locTcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
  Uint32 nextTc = locTcConnectptr.p->nextTcConnectrec;
  locTcConnectptr.p->nextTcConnectrec = RNIL;
  locTcConnectptr.p->clientConnectrec = RNIL;
  locTcConnectptr.p->clientBlockref = RNIL;
  locTcConnectptr.p->abortState = TcConnectionrec::ABORT_IDLE;
  locTcConnectptr.p->tcTimer = cLqhTimeOutCount;
  locTcConnectptr.p->tableref = RNIL;
  locTcConnectptr.p->savePointId = 0;
  cfirstfreeTcConrec = nextTc;
  tcConnectptr = locTcConnectptr;
  locTcConnectptr.p->connectState = TcConnectionrec::CONNECTED;
}//Dblqh::seizeTcrec()

/* ==========================================================================
 * =======                          SEIZE DATA BUFFER                 ======= 
 * ========================================================================= */
void Dblqh::seizeTupkeybuf(Signal* signal) 
{
  Databuf *regDatabuf = databuf;
  DatabufPtr tmpDatabufptr;
  DatabufPtr regDatabufptr;
  Uint32 tdatabufFileSize = cdatabufFileSize;

/* ------- GET A DATABUF. ------- */
  regDatabufptr.i = cfirstfreeDatabuf;
  tmpDatabufptr.i = tcConnectptr.p->lastTupkeybuf;
  ptrCheckGuard(regDatabufptr, tdatabufFileSize, regDatabuf);
  Uint32 nextFirst = regDatabufptr.p->nextDatabuf;
  tcConnectptr.p->lastTupkeybuf = regDatabufptr.i;
  if (tmpDatabufptr.i == RNIL) {
    jam();
    tcConnectptr.p->firstTupkeybuf = regDatabufptr.i;
  } else {
    jam();
    ptrCheckGuard(tmpDatabufptr, tdatabufFileSize, regDatabuf);
    tmpDatabufptr.p->nextDatabuf = regDatabufptr.i;
  }//if
  cfirstfreeDatabuf = nextFirst;
  regDatabufptr.p->nextDatabuf = RNIL;
  databufptr = regDatabufptr;
}//Dblqh::seizeTupkeybuf()

/* ------------------------------------------------------------------------- */
/* -------                TAKE CARE OF LQHKEYREQ                     ------- */
/* LQHKEYREQ IS THE SIGNAL THAT STARTS ALL OPERATIONS IN THE LQH BLOCK       */
/* THIS SIGNAL CONTAINS A LOT OF INFORMATION ABOUT WHAT TYPE OF OPERATION,   */
/* KEY INFORMATION, ATTRIBUTE INFORMATION, NODE INFORMATION AND A LOT MORE   */
/* ------------------------------------------------------------------------- */
void Dblqh::execLQHKEYREQ(Signal* signal) 
{
  UintR sig0, sig1, sig2, sig3, sig4, sig5;
  Uint8 tfragDistKey;

  const LqhKeyReq * const lqhKeyReq = (LqhKeyReq *)signal->getDataPtr();

  sig0 = lqhKeyReq->clientConnectPtr;
  if (cfirstfreeTcConrec != RNIL && !ERROR_INSERTED(5031)) {
    jamEntry();
    seizeTcrec();
  } else {
/* ------------------------------------------------------------------------- */
/* NO FREE TC RECORD AVAILABLE, THUS WE CANNOT HANDLE THE REQUEST.           */
/* ------------------------------------------------------------------------- */
    if (ERROR_INSERTED(5031)) {
      CLEAR_ERROR_INSERT_VALUE;
    }
    noFreeRecordLab(signal, lqhKeyReq, ZNO_TC_CONNECT_ERROR);
    return;
  }//if

  if(ERROR_INSERTED(5038) && 
     refToNode(signal->getSendersBlockRef()) != getOwnNodeId()){
    jam();
    SET_ERROR_INSERT_VALUE(5039);
    return;
  }
  
  c_Counters.operations++;

  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  regTcPtr->clientBlockref = signal->senderBlockRef();
  regTcPtr->clientConnectrec = sig0;
  regTcPtr->tcOprec = sig0;
  regTcPtr->storedProcId = ZNIL;

  UintR TtotReclenAi = lqhKeyReq->attrLen;
  sig1 = lqhKeyReq->savePointId;
  sig2 = lqhKeyReq->hashValue;
  UintR Treqinfo = lqhKeyReq->requestInfo;
  sig4 = lqhKeyReq->tableSchemaVersion;
  sig5 = lqhKeyReq->tcBlockref;

  regTcPtr->savePointId = sig1;
  regTcPtr->hashValue = sig2;
  const Uint32 schemaVersion = regTcPtr->schemaVersion = LqhKeyReq::getSchemaVersion(sig4);
  tabptr.i = LqhKeyReq::getTableId(sig4);
  regTcPtr->tcBlockref = sig5;

  const Uint8 op = LqhKeyReq::getOperation(Treqinfo);
  if (op == ZREAD && !getAllowRead()){
    noFreeRecordLab(signal, lqhKeyReq, ZNODE_SHUTDOWN_IN_PROGESS);
    return;
  }

  regTcPtr->totReclenAi = LqhKeyReq::getAttrLen(TtotReclenAi);
  regTcPtr->tcScanInfo  = lqhKeyReq->scanInfo;
  regTcPtr->indTakeOver = LqhKeyReq::getScanTakeOverFlag(TtotReclenAi);

  regTcPtr->readlenAi = 0;
  regTcPtr->currTupAiLen = 0;
  regTcPtr->listState = TcConnectionrec::NOT_IN_LIST;
  regTcPtr->logWriteState = TcConnectionrec::NOT_STARTED;
  regTcPtr->fragmentptr = RNIL;

  sig0 = lqhKeyReq->fragmentData;
  sig1 = lqhKeyReq->transId1;
  sig2 = lqhKeyReq->transId2;
  sig3 = lqhKeyReq->variableData[0];
  sig4 = lqhKeyReq->variableData[1];

  regTcPtr->fragmentid = LqhKeyReq::getFragmentId(sig0);
  regTcPtr->nextReplica = LqhKeyReq::getNextReplicaNodeId(sig0);
  regTcPtr->transid[0] = sig1;
  regTcPtr->transid[1] = sig2;
  regTcPtr->applRef = sig3;
  regTcPtr->applOprec = sig4;

  regTcPtr->commitAckMarker = RNIL;
  if(LqhKeyReq::getMarkerFlag(Treqinfo)){
    jam();
    
    CommitAckMarkerPtr markerPtr;
    m_commitAckMarkerHash.seize(markerPtr);
    if(markerPtr.i == RNIL){
      noFreeRecordLab(signal, lqhKeyReq, ZNO_FREE_MARKER_RECORDS_ERROR);
      return;
    }
    markerPtr.p->transid1 = sig1;
    markerPtr.p->transid2 = sig2;
    markerPtr.p->apiRef   = sig3;
    markerPtr.p->apiOprec = sig4;
    const NodeId tcNodeId  = refToNode(sig5);
    markerPtr.p->tcNodeId = tcNodeId;
    
    CommitAckMarkerPtr tmp;
#ifdef VM_TRACE
#ifdef MARKER_TRACE
    ndbout_c("Add marker[%.8x %.8x]", markerPtr.p->transid1, markerPtr.p->transid2);
#endif
    ndbrequire(!m_commitAckMarkerHash.find(tmp, * markerPtr.p));
#endif
    m_commitAckMarkerHash.add(markerPtr);
    regTcPtr->commitAckMarker = markerPtr.i;
  } 
  
  regTcPtr->reqinfo = Treqinfo;
  regTcPtr->lastReplicaNo = LqhKeyReq::getLastReplicaNo(Treqinfo);
  regTcPtr->lockType      = LqhKeyReq::getLockType(Treqinfo);
  regTcPtr->dirtyOp       = LqhKeyReq::getDirtyFlag(Treqinfo);
  regTcPtr->opExec        = LqhKeyReq::getInterpretedFlag(Treqinfo);
  regTcPtr->opSimple      = LqhKeyReq::getSimpleFlag(Treqinfo);
  regTcPtr->operation     = LqhKeyReq::getOperation(Treqinfo);
  regTcPtr->simpleRead    = regTcPtr->operation == ZREAD && regTcPtr->opSimple;
  regTcPtr->seqNoReplica  = LqhKeyReq::getSeqNoReplica(Treqinfo);
  UintR TreclenAiLqhkey   = LqhKeyReq::getAIInLqhKeyReq(Treqinfo);
  regTcPtr->apiVersionNo  = 0; 
  
  CRASH_INSERTION2(5041, regTcPtr->simpleRead && 
		   refToNode(signal->senderBlockRef()) != cownNodeid);
  
  regTcPtr->reclenAiLqhkey = TreclenAiLqhkey;
  regTcPtr->currReclenAi = TreclenAiLqhkey;
  UintR TitcKeyLen = LqhKeyReq::getKeyLen(Treqinfo);
  regTcPtr->primKeyLen = TitcKeyLen;
  regTcPtr->noFiredTriggers = lqhKeyReq->noFiredTriggers;

  UintR TapplAddressInd = LqhKeyReq::getApplicationAddressFlag(Treqinfo);
  UintR nextPos = (TapplAddressInd << 1);
  UintR TsameClientAndTcOprec = LqhKeyReq::getSameClientAndTcFlag(Treqinfo);
  if (TsameClientAndTcOprec == 1) {
    regTcPtr->tcOprec = lqhKeyReq->variableData[nextPos];
    nextPos++;
  }//if
  UintR TnextReplicasIndicator = regTcPtr->lastReplicaNo - 
                                 regTcPtr->seqNoReplica;
  if (TnextReplicasIndicator > 1) {
    regTcPtr->nodeAfterNext[0] = lqhKeyReq->variableData[nextPos] & 0xFFFF;
    regTcPtr->nodeAfterNext[1] = lqhKeyReq->variableData[nextPos] >> 16;
    nextPos++;
  }//if
  UintR TstoredProcIndicator = LqhKeyReq::getStoredProcFlag(TtotReclenAi);
  if (TstoredProcIndicator == 1) {
    regTcPtr->storedProcId = lqhKeyReq->variableData[nextPos] & ZNIL;
    nextPos++;
  }//if
  UintR TreadLenAiIndicator = LqhKeyReq::getReturnedReadLenAIFlag(Treqinfo);
  if (TreadLenAiIndicator == 1) {
    regTcPtr->readlenAi = lqhKeyReq->variableData[nextPos] & ZNIL;
    nextPos++;
  }//if
  sig0 = lqhKeyReq->variableData[nextPos + 0];
  sig1 = lqhKeyReq->variableData[nextPos + 1];
  sig2 = lqhKeyReq->variableData[nextPos + 2];
  sig3 = lqhKeyReq->variableData[nextPos + 3];

  regTcPtr->tupkeyData[0] = sig0;
  regTcPtr->tupkeyData[1] = sig1;
  regTcPtr->tupkeyData[2] = sig2;
  regTcPtr->tupkeyData[3] = sig3;

  if (TitcKeyLen > 0) {
    if (TitcKeyLen < 4) {
      nextPos += TitcKeyLen;
    } else {
      nextPos += 4;
    }//if
  } else {
    LQHKEY_error(signal, 3);
    return;
  }//if

  if ((LqhKeyReq::FixedSignalLength + nextPos + TreclenAiLqhkey) != 
      signal->length()) {
    LQHKEY_error(signal, 2);
    return;
  }//if
  UintR TseqNoReplica = regTcPtr->seqNoReplica;
  UintR TlastReplicaNo = regTcPtr->lastReplicaNo;
  if (TseqNoReplica == TlastReplicaNo) {
    jam();
    regTcPtr->nextReplica = ZNIL;
  } else {
    if (TseqNoReplica < TlastReplicaNo) {
      jam();
      regTcPtr->nextSeqNoReplica = TseqNoReplica + 1;
      if ((regTcPtr->nextReplica == 0) ||
          (regTcPtr->nextReplica == cownNodeid)) {
        LQHKEY_error(signal, 0);
      }//if
    } else {
      LQHKEY_error(signal, 4);
      return;
    }//if
  }//if
  TcConnectionrecPtr localNextTcConnectptr;
  Uint32 hashIndex = (regTcPtr->transid[0] ^ regTcPtr->tcOprec) & 1023;
  localNextTcConnectptr.i = ctransidHash[hashIndex];
  ctransidHash[hashIndex] = tcConnectptr.i;
  regTcPtr->prevHashRec = RNIL;
  regTcPtr->nextHashRec = localNextTcConnectptr.i;
  if (localNextTcConnectptr.i != RNIL) {
/* -------------------------------------------------------------------------- */
/* ENSURE THAT THE NEXT RECORD HAS SET PREVIOUS TO OUR RECORD IF IT EXISTS    */
/* -------------------------------------------------------------------------- */
    ptrCheckGuard(localNextTcConnectptr, 
                  ctcConnectrecFileSize, tcConnectionrec);
    jam();
    localNextTcConnectptr.p->prevHashRec = tcConnectptr.i;
  }//if
  if (tabptr.i >= ctabrecFileSize) {
    LQHKEY_error(signal, 5);
    return;
  }//if
  ptrAss(tabptr, tablerec);
  if(tabptr.p->tableStatus != Tablerec::TABLE_DEFINED){
    LQHKEY_abort(signal, 4);
    return;
  }
  if(table_version_major(tabptr.p->schemaVersion) != 
     table_version_major(schemaVersion)){
    LQHKEY_abort(signal, 5);
    return;
  }

  regTcPtr->tableref = tabptr.i;
  tabptr.p->usageCount++;
  
  if (!getFragmentrec(signal, regTcPtr->fragmentid)) {
    LQHKEY_error(signal, 6);
    return;
  }//if
  regTcPtr->localFragptr = regTcPtr->hashValue & 1;
  Uint8 TcopyType = fragptr.p->fragCopy;
  tfragDistKey = fragptr.p->fragDistributionKey;
  if (fragptr.p->fragStatus == Fragrecord::ACTIVE_CREATION) {
    jam();
    regTcPtr->activeCreat = ZTRUE;
    CRASH_INSERTION(5002);
  } else {
    regTcPtr->activeCreat = ZFALSE;
  }//if
  regTcPtr->replicaType = TcopyType;
  regTcPtr->fragmentptr = fragptr.i;
  Uint8 TdistKey = LqhKeyReq::getDistributionKey(TtotReclenAi);
  if ((tfragDistKey != TdistKey) &&
      (regTcPtr->seqNoReplica == 0) &&
      (regTcPtr->dirtyOp == ZFALSE) &&
      (regTcPtr->simpleRead == ZFALSE)) {
    /* ----------------------------------------------------------------------
     * WE HAVE DIFFERENT OPINION THAN THE DIH THAT STARTED THE TRANSACTION. 
     * THE REASON COULD BE THAT THIS IS AN OLD DISTRIBUTION WHICH IS NO LONGER
     * VALID TO USE. THIS MUST BE CHECKED.
     * ONE IS ADDED TO THE DISTRIBUTION KEY EVERY TIME WE ADD A NEW REPLICA.
     * FAILED REPLICAS DO NOT AFFECT THE DISTRIBUTION KEY. THIS MEANS THAT THE 
     * MAXIMUM DEVIATION CAN BE ONE BETWEEN THOSE TWO VALUES.              
     * --------------------------------------------------------------------- */
    Int32 tmp = TdistKey - tfragDistKey;
    tmp = (tmp < 0 ? - tmp : tmp);
    if ((tmp <= 1) || (tfragDistKey == 0)) {
      LQHKEY_abort(signal, 0);
      return;
    }//if
    LQHKEY_error(signal, 1);
  }//if
  if (TreclenAiLqhkey != 0) {
    if (regTcPtr->operation != ZREAD) {
      if (regTcPtr->operation != ZDELETE) {
        if (regTcPtr->opExec != 1) {
          jam();
/*---------------------------------------------------------------------------*/
/*                                                                           */
/* UPDATES, WRITES AND INSERTS THAT ARE NOT INTERPRETED WILL USE THE         */
/* SAME ATTRINFO IN ALL REPLICAS. THUS WE SAVE THE ATTRINFO ALREADY          */
/* TO SAVE A SIGNAL FROM TUP TO LQH. INTERPRETED EXECUTION IN TUP            */
/* WILL CREATE NEW ATTRINFO FOR THE OTHER REPLICAS AND IT IS THUS NOT        */
/* A GOOD IDEA TO SAVE THE INFORMATION HERE. READS WILL ALSO BE              */
/* UNNECESSARY TO SAVE SINCE THAT ATTRINFO WILL NEVER BE SENT TO ANY         */
/* MORE REPLICAS.                                                            */
/*---------------------------------------------------------------------------*/
/* READS AND DELETES CAN ONLY HAVE INFORMATION ABOUT WHAT IS TO BE READ.     */
/* NO INFORMATION THAT NEEDS LOGGING.                                        */
/*---------------------------------------------------------------------------*/
          sig0 = lqhKeyReq->variableData[nextPos + 0];
          sig1 = lqhKeyReq->variableData[nextPos + 1];
          sig2 = lqhKeyReq->variableData[nextPos + 2];
          sig3 = lqhKeyReq->variableData[nextPos + 3];
          sig4 = lqhKeyReq->variableData[nextPos + 4];

          regTcPtr->firstAttrinfo[0] = sig0;
          regTcPtr->firstAttrinfo[1] = sig1;
          regTcPtr->firstAttrinfo[2] = sig2;
          regTcPtr->firstAttrinfo[3] = sig3;
          regTcPtr->firstAttrinfo[4] = sig4;
          regTcPtr->currTupAiLen = TreclenAiLqhkey;
        } else {
          jam();
          regTcPtr->reclenAiLqhkey = 0;
        }//if
      } else {
        jam();
        regTcPtr->reclenAiLqhkey = 0;
      }//if
    }//if
    sig0 = lqhKeyReq->variableData[nextPos + 0];
    sig1 = lqhKeyReq->variableData[nextPos + 1];
    sig2 = lqhKeyReq->variableData[nextPos + 2];
    sig3 = lqhKeyReq->variableData[nextPos + 3];
    sig4 = lqhKeyReq->variableData[nextPos + 4];

    signal->theData[0] = regTcPtr->tupConnectrec;
    signal->theData[3] = sig0;
    signal->theData[4] = sig1;
    signal->theData[5] = sig2;
    signal->theData[6] = sig3;
    signal->theData[7] = sig4;
    EXECUTE_DIRECT(refToBlock(regTcPtr->tcTupBlockref), GSN_ATTRINFO, 
		   signal, TreclenAiLqhkey + 3);
    jamEntry();
    if (signal->theData[0] == (UintR)-1) {
      LQHKEY_abort(signal, 2);
      return;
    }//if
  }//if
/* ------- TAKE CARE OF PRIM KEY DATA ------- */
  if (regTcPtr->primKeyLen <= 4) {
    endgettupkeyLab(signal);
    return;
  } else {
    jam();
/*--------------------------------------------------------------------*/
/*       KEY LENGTH WAS MORE THAN 4 WORDS (WORD = 4 BYTE). THUS WE    */
/*       HAVE TO ALLOCATE A DATA BUFFER TO STORE THE KEY DATA AND     */
/*       WAIT FOR THE KEYINFO SIGNAL.                                 */
/*--------------------------------------------------------------------*/
    regTcPtr->save1 = 4;
    regTcPtr->transactionState = TcConnectionrec::WAIT_TUPKEYINFO;
    return;
  }//if
  return;
}//Dblqh::execLQHKEYREQ()

void Dblqh::endgettupkeyLab(Signal* signal) 
{
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  if (regTcPtr->totReclenAi == regTcPtr->currReclenAi) {
    ;
  } else {
    jam();
    ndbrequire(regTcPtr->currReclenAi < regTcPtr->totReclenAi);
    regTcPtr->transactionState = TcConnectionrec::WAIT_ATTR;
    return;
  }//if
/* ---------------------------------------------------------------------- */
/*       NOW RECEPTION OF LQHKEYREQ IS COMPLETED THE NEXT STEP IS TO START*/
/*       PROCESSING THE MESSAGE. IF THE MESSAGE IS TO A STAND-BY NODE     */
/*       WITHOUT NETWORK REDUNDANCY OR PREPARE-TO-COMMIT ACTIVATED THE    */
/*       PREPARATION TO SEND TO THE NEXT NODE WILL START IMMEDIATELY.     */
/*                                                                        */
/*       OTHERWISE THE PROCESSING WILL START AFTER SETTING THE PROPER     */
/*       STATE. HOWEVER BEFORE PROCESSING THE MESSAGE                     */
/*       IT IS NECESSARY TO CHECK THAT THE FRAGMENT IS NOT PERFORMING     */
/*       A CHECKPOINT. THE OPERATION SHALL ALSO BE LINKED INTO THE        */
/*       FRAGMENT QUEUE OR LIST OF ACTIVE OPERATIONS.                     */
/*                                                                        */
/*       THE FIRST STEP IN PROCESSING THE MESSAGE IS TO CONTACT DBACC.    */
/*------------------------------------------------------------------------*/
  switch (fragptr.p->fragStatus) {
  case Fragrecord::FSACTIVE:
  case Fragrecord::CRASH_RECOVERING:
  case Fragrecord::ACTIVE_CREATION:
    linkActiveFrag(signal);
    prepareContinueAfterBlockedLab(signal);
    return;
    break;
  case Fragrecord::BLOCKED:
    jam();
    linkFragQueue(signal);
    regTcPtr->transactionState = TcConnectionrec::STOPPED;
    return;
    break;
  case Fragrecord::FREE:
    jam();
  case Fragrecord::DEFINED:
    jam();
  case Fragrecord::REMOVING:
    jam();
  default:
    ndbrequire(false);
    break;
  }//switch
  return;
}//Dblqh::endgettupkeyLab()

void Dblqh::prepareContinueAfterBlockedLab(Signal* signal) 
{
  UintR ttcScanOp;
  UintR taccreq;

/* -------------------------------------------------------------------------- */
/*       INPUT:          TC_CONNECTPTR           ACTIVE CONNECTION RECORD     */
/*                       FRAGPTR                 FRAGMENT RECORD              */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/*  CONTINUE HERE AFTER BEING BLOCKED FOR A WHILE DURING LOCAL CHECKPOINT.    */
/* -------------------------------------------------------------------------- */
/*       ALSO AFTER NORMAL PROCEDURE WE CONTINUE HERE                         */
/* -------------------------------------------------------------------------- */
  Uint32 tc_ptr_i = tcConnectptr.i;
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  if (regTcPtr->indTakeOver == ZTRUE) {
    jam();
    ttcScanOp = KeyInfo20::getScanOp(regTcPtr->tcScanInfo);
    scanptr.i = RNIL;
    {
      ScanRecord key;
      key.scanNumber = KeyInfo20::getScanNo(regTcPtr->tcScanInfo);
      key.fragPtrI = fragptr.i;
      c_scanTakeOverHash.find(scanptr, key);
#ifdef TRACE_SCAN_TAKEOVER
      if(scanptr.i == RNIL)
	ndbout_c("not finding (%d %d)", key.scanNumber, key.fragPtrI);
#endif
    }
    if (scanptr.i == RNIL) {
      jam();
      releaseActiveFrag(signal);
      takeOverErrorLab(signal);
      return;
    }//if
    Uint32 accOpPtr= get_acc_ptr_from_scan_record(scanptr.p,
                                                  ttcScanOp,
                                                  true);
    if (accOpPtr == RNIL) {
      jam();
      releaseActiveFrag(signal);
      takeOverErrorLab(signal);
      return;
    }//if
    signal->theData[1] = accOpPtr;
    signal->theData[2] = regTcPtr->transid[0];
    signal->theData[3] = regTcPtr->transid[1];
    EXECUTE_DIRECT(refToBlock(regTcPtr->tcAccBlockref), GSN_ACC_TO_REQ, 
		   signal, 4);
    if (signal->theData[0] == (UintR)-1) {
      execACC_TO_REF(signal);
      return;
    }//if
    jamEntry();
  }//if
/*-------------------------------------------------------------------*/
/*       IT IS NOW TIME TO CONTACT ACC. THE TUPLE KEY WILL BE SENT   */
/*       AND THIS WILL BE TRANSLATED INTO A LOCAL KEY BY USING THE   */
/*       LOCAL PART OF THE LH3-ALGORITHM. ALSO PROPER LOCKS ON THE   */
/*       TUPLE WILL BE SET. FOR INSERTS AND DELETES THE MESSAGE WILL */
/*       START AN INSERT/DELETE INTO THE HASH TABLE.                 */
/*                                                                   */
/*       BEFORE SENDING THE MESSAGE THE REQUEST INFORMATION IS SET   */
/*       PROPERLY.                                                   */
/* ----------------------------------------------------------------- */
#if 0
  if (regTcPtr->tableref != 0) {
    switch (regTcPtr->operation) {
    case ZREAD: ndbout << "Läsning "; break;
    case ZUPDATE: ndbout << " Uppdatering "; break;
    case ZWRITE: ndbout << "Write "; break;
    case ZINSERT: ndbout << "Inläggning "; break;
    case ZDELETE: ndbout << "Borttagning "; break;
    default: ndbout << "????"; break;
    }
    ndbout << "med nyckel = " << regTcPtr->tupkeyData[0] << endl;
  }
#endif
  
  regTcPtr->transactionState = TcConnectionrec::WAIT_ACC;
  taccreq = regTcPtr->operation;
  taccreq = taccreq + (regTcPtr->opSimple << 3);
  taccreq = taccreq + (regTcPtr->lockType << 4);
  taccreq = taccreq + (regTcPtr->dirtyOp << 6);
  taccreq = taccreq + (regTcPtr->replicaType << 7);
  taccreq = taccreq + (regTcPtr->apiVersionNo << 9);
/* ************ */
/*  ACCKEYREQ < */
/* ************ */
  ndbrequire(regTcPtr->localFragptr < 2);
  Uint32 sig0, sig1, sig2, sig3, sig4;
  sig0 = regTcPtr->accConnectrec;
  sig1 = fragptr.p->accFragptr[regTcPtr->localFragptr];
  sig2 = regTcPtr->hashValue;
  sig3 = regTcPtr->primKeyLen;
  sig4 = regTcPtr->transid[0];
  signal->theData[0] = sig0;
  signal->theData[1] = sig1;
  signal->theData[2] = taccreq;
  signal->theData[3] = sig2;
  signal->theData[4] = sig3;
  signal->theData[5] = sig4;

  sig0 = regTcPtr->transid[1];
  sig1 = regTcPtr->tupkeyData[0];
  sig2 = regTcPtr->tupkeyData[1];
  sig3 = regTcPtr->tupkeyData[2];
  sig4 = regTcPtr->tupkeyData[3];
  signal->theData[6] = sig0;
  signal->theData[7] = sig1;
  signal->theData[8] = sig2;
  signal->theData[9] = sig3;
  signal->theData[10] = sig4;
  if (regTcPtr->primKeyLen > 4) {
    sendKeyinfoAcc(signal, 11);
  }//if
  EXECUTE_DIRECT(refToBlock(regTcPtr->tcAccBlockref), GSN_ACCKEYREQ, 
		 signal, 7 + regTcPtr->primKeyLen);
  if (signal->theData[0] < RNIL) {
    signal->theData[0] = tc_ptr_i;
    execACCKEYCONF(signal);
    return;
  } else if (signal->theData[0] == RNIL) {
    ;
  } else {
    ndbrequire(signal->theData[0] == (UintR)-1);
    signal->theData[0] = tc_ptr_i;
    execACCKEYREF(signal);
  }//if
  return;
}//Dblqh::prepareContinueAfterBlockedLab()

/* ========================================================================== */
/* =======                  SEND KEYINFO TO ACC                       ======= */
/*                                                                            */
/* ========================================================================== */
void Dblqh::sendKeyinfoAcc(Signal* signal, Uint32 Ti) 
{
  DatabufPtr regDatabufptr;
  regDatabufptr.i = tcConnectptr.p->firstTupkeybuf;
  
  do {
    jam();
    ptrCheckGuard(regDatabufptr, cdatabufFileSize, databuf);
    Uint32 sig0 = regDatabufptr.p->data[0];
    Uint32 sig1 = regDatabufptr.p->data[1];
    Uint32 sig2 = regDatabufptr.p->data[2];
    Uint32 sig3 = regDatabufptr.p->data[3];
    signal->theData[Ti] = sig0;
    signal->theData[Ti + 1] = sig1;
    signal->theData[Ti + 2] = sig2;
    signal->theData[Ti + 3] = sig3;
    regDatabufptr.i = regDatabufptr.p->nextDatabuf;
    Ti += 4;
  } while (regDatabufptr.i != RNIL);
}//Dblqh::sendKeyinfoAcc()

void Dblqh::execLQH_ALLOCREQ(Signal* signal)
{
  TcConnectionrecPtr regTcPtr;  
  FragrecordPtr regFragptr;

  jamEntry();
  regTcPtr.i = signal->theData[0];
  ptrCheckGuard(regTcPtr, ctcConnectrecFileSize, tcConnectionrec);

  regFragptr.i = regTcPtr.p->fragmentptr;
  ptrCheckGuard(regFragptr, cfragrecFileSize, fragrecord);

  ndbrequire(regTcPtr.p->localFragptr < 2);
  signal->theData[0] = regTcPtr.p->tupConnectrec;
  signal->theData[1] = regFragptr.p->tupFragptr[regTcPtr.p->localFragptr];
  signal->theData[2] = regTcPtr.p->tableref;
  Uint32 tup = refToBlock(regTcPtr.p->tcTupBlockref);
  EXECUTE_DIRECT(tup, GSN_TUP_ALLOCREQ, signal, 3);
}//Dblqh::execTUP_ALLOCREQ()

/* ************>> */
/*  ACCKEYCONF  > */
/* ************>> */
void Dblqh::execACCKEYCONF(Signal* signal) 
{
  TcConnectionrec *regTcConnectionrec = tcConnectionrec;
  Uint32 ttcConnectrecFileSize = ctcConnectrecFileSize;
  Uint32 tcIndex = signal->theData[0];
  Uint32 Tfragid = signal->theData[2];
  Uint32 localKey1 = signal->theData[3];
  Uint32 localKey2 = signal->theData[4];
  Uint32 localKeyFlag = signal->theData[5];
  jamEntry();
  tcConnectptr.i = tcIndex;
  ptrCheckGuard(tcConnectptr, ttcConnectrecFileSize, regTcConnectionrec);
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  if (regTcPtr->transactionState != TcConnectionrec::WAIT_ACC) {
    LQHKEY_abort(signal, 3);
    return;
  }//if
  /* ------------------------------------------------------------------------ 
   * Set transaction state and also reset the activeCreat since that is only 
   * valid in cases where the record was not present.
   * ------------------------------------------------------------------------ */
  regTcPtr->transactionState = TcConnectionrec::WAIT_TUP;
  regTcPtr->activeCreat = ZFALSE;
  /* ------------------------------------------------------------------------
   * IT IS NOW TIME TO CONTACT THE TUPLE MANAGER. THE TUPLE MANAGER NEEDS THE
   * INFORMATION ON WHICH TABLE AND FRAGMENT, THE LOCAL KEY AND IT NEEDS TO
   * KNOW THE TYPE OF OPERATION TO PERFORM. TUP CAN SEND THE ATTRINFO DATA 
   * EITHER TO THE TC BLOCK OR DIRECTLY TO THE APPLICATION. THE SCHEMA VERSION
   * IS NEEDED SINCE TWO SCHEMA VERSIONS CAN BE ACTIVE SIMULTANEOUSLY ON A 
   * TABLE.
   * ----------------------------------------------------------------------- */
  if (regTcPtr->operation == ZWRITE) 
  {
    Uint32 op= signal->theData[1];
    if(likely(op == ZINSERT || op == ZUPDATE))
    {
      regTcPtr->operation = op;
    }
    else
    {
      warningEvent("Convering %d to ZUPDATE", op);
      regTcPtr->operation = ZUPDATE;
    }
  }//if
  
  ndbrequire(localKeyFlag == 1);
  localKey2 = localKey1 & MAX_TUPLES_PER_PAGE;
  localKey1 = localKey1 >> MAX_TUPLES_BITS;
  Uint32 Ttupreq = regTcPtr->dirtyOp;
  Ttupreq = Ttupreq + (regTcPtr->opSimple << 1);
  Ttupreq = Ttupreq + (regTcPtr->operation << 6);
  Ttupreq = Ttupreq + (regTcPtr->opExec << 10);
  Ttupreq = Ttupreq + (regTcPtr->apiVersionNo << 11);

  /* --------------------------------------------------------------------- 
   * Clear interpreted mode bit since we do not want the next replica to
   * use interpreted mode. The next replica will receive a normal write.
   * --------------------------------------------------------------------- */
  regTcPtr->opExec = 0;
  /* ************< */
  /*  TUPKEYREQ  < */
  /* ************< */
  TupKeyReq * const tupKeyReq = (TupKeyReq *)signal->getDataPtrSend();
  Uint32 sig0, sig1, sig2, sig3;

  sig0 = regTcPtr->tupConnectrec;
  sig1 = regTcPtr->tableref;
  tupKeyReq->connectPtr = sig0;
  tupKeyReq->request = Ttupreq;
  tupKeyReq->tableRef = sig1;
  tupKeyReq->fragId = Tfragid;
  tupKeyReq->keyRef1 = localKey1;
  tupKeyReq->keyRef2 = localKey2;

  sig0 = regTcPtr->totReclenAi;
  sig1 = regTcPtr->applOprec;
  sig2 = regTcPtr->applRef;
  sig3 = regTcPtr->schemaVersion;
  FragrecordPtr regFragptr;
  regFragptr.i = regTcPtr->fragmentptr;
  ptrCheckGuard(regFragptr, cfragrecFileSize, fragrecord);
  tupKeyReq->attrBufLen = sig0;
  tupKeyReq->opRef = sig1;
  tupKeyReq->applRef = sig2;
  tupKeyReq->schemaVersion = sig3;

  ndbrequire(regTcPtr->localFragptr < 2);
  sig0 = regTcPtr->storedProcId;
  sig1 = regTcPtr->transid[0];
  sig2 = regTcPtr->transid[1];
  sig3 = regFragptr.p->tupFragptr[regTcPtr->localFragptr];
  Uint32 tup = refToBlock(regTcPtr->tcTupBlockref);

  tupKeyReq->storedProcedure = sig0;
  tupKeyReq->transId1 = sig1;
  tupKeyReq->transId2 = sig2;
  tupKeyReq->fragPtr = sig3;
  tupKeyReq->primaryReplica = (tcConnectptr.p->seqNoReplica == 0)?true:false;
  tupKeyReq->coordinatorTC = tcConnectptr.p->tcBlockref;
  tupKeyReq->tcOpIndex = tcConnectptr.p->tcOprec;
  tupKeyReq->savePointId = tcConnectptr.p->savePointId;

  EXECUTE_DIRECT(tup, GSN_TUPKEYREQ, signal, TupKeyReq::SignalLength);
}//Dblqh::execACCKEYCONF()

/* --------------------------------------------------------------------------
 * -------                       ENTER TUP...                         ------- 
 * ENTER TUPKEYCONF WITH
 *           TC_CONNECTPTR,
 *           TDATA2,     LOCAL KEY REFERENCE 1, ONLY INTERESTING AFTER INSERT
 *           TDATA3,     LOCAL KEY REFERENCE 1, ONLY INTERESTING AFTER INSERT
 *           TDATA4,     TOTAL LENGTH OF READ DATA SENT TO TC/APPLICATION
 *           TDATA5      TOTAL LENGTH OF UPDATE DATA SENT TO/FROM TUP
 *        GOTO TUPKEY_CONF
 *
 *  TAKE CARE OF RESPONSES FROM TUPLE MANAGER.
 * -------------------------------------------------------------------------- */
void Dblqh::tupkeyConfLab(Signal* signal) 
{
/* ---- GET OPERATION TYPE AND CHECK WHAT KIND OF OPERATION IS REQUESTED ---- */
  const TupKeyConf * const tupKeyConf = (TupKeyConf *)&signal->theData[0];
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  if (regTcPtr->simpleRead) {
    jam();
    /* ----------------------------------------------------------------------
     * THE OPERATION IS A SIMPLE READ. WE WILL IMMEDIATELY COMMIT THE OPERATION.
     * SINCE WE HAVE NOT RELEASED THE FRAGMENT LOCK (FOR LOCAL CHECKPOINTS) YET 
     * WE CAN GO IMMEDIATELY TO COMMIT_CONTINUE_AFTER_BLOCKED.
     * WE HAVE ALREADY SENT THE RESPONSE SO WE ARE NOT INTERESTED IN READ LENGTH
     * ---------------------------------------------------------------------- */
    regTcPtr->gci = cnewestGci;
    releaseActiveFrag(signal);
    commitContinueAfterBlockedLab(signal);
    return;
  }//if
  if (tupKeyConf->readLength != 0) {
    jam();

    /* SET BIT 15 IN REQINFO */
    LqhKeyReq::setApplicationAddressFlag(regTcPtr->reqinfo, 1);

    regTcPtr->readlenAi = tupKeyConf->readLength;
  }//if
  regTcPtr->totSendlenAi = tupKeyConf->writeLength;
  ndbrequire(regTcPtr->totSendlenAi == regTcPtr->currTupAiLen);
  rwConcludedLab(signal);
  return;
}//Dblqh::tupkeyConfLab()

/* --------------------------------------------------------------------------
 *     THE CODE IS FOUND IN THE SIGNAL RECEPTION PART OF LQH                 
 * -------------------------------------------------------------------------- */
void Dblqh::rwConcludedLab(Signal* signal) 
{
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  /* ------------------------------------------------------------------------
   *  WE HAVE NOW CONCLUDED READING/WRITING IN ACC AND TUP FOR THIS OPERATION. 
   *  IT IS NOW TIME TO LOG THE OPERATION, SEND REQUEST TO NEXT NODE OR TC AND 
   *  FOR SOME TYPES OF OPERATIONS IT IS EVEN TIME TO COMMIT THE OPERATION.
   * ------------------------------------------------------------------------ */
  if (regTcPtr->operation == ZREAD) {
    jam();
    /* ---------------------------------------------------------------------- 
     * A NORMAL READ OPERATION IS NOT LOGGED BUT IS NOT COMMITTED UNTIL THE 
     * COMMIT SIGNAL ARRIVES. THUS WE CONTINUE PACKING THE RESPONSE.   
     * ---------------------------------------------------------------------- */
    releaseActiveFrag(signal);
    packLqhkeyreqLab(signal);
    return;
  } else {
    FragrecordPtr regFragptr;
    regFragptr.i = regTcPtr->fragmentptr;
    ptrCheckGuard(regFragptr, cfragrecFileSize, fragrecord);
    if (regFragptr.p->logFlag == Fragrecord::STATE_FALSE){
      if (regTcPtr->dirtyOp == ZTRUE) {
        jam();
	/* ------------------------------------------------------------------
	 * THIS OPERATION WAS A WRITE OPERATION THAT DO NOT NEED LOGGING AND 
	 * THAT CAN CAN  BE COMMITTED IMMEDIATELY.                     
	 * ------------------------------------------------------------------ */
        regTcPtr->gci = cnewestGci;
        releaseActiveFrag(signal);
        commitContinueAfterBlockedLab(signal);
        return;
      } else {
        jam();
	/* ------------------------------------------------------------------
	 * A NORMAL WRITE OPERATION ON A FRAGMENT WHICH DO NOT NEED LOGGING.
	 * WE WILL PACK THE REQUEST/RESPONSE TO THE NEXT NODE/TO TC.   
	 * ------------------------------------------------------------------ */
        regTcPtr->logWriteState = TcConnectionrec::NOT_WRITTEN;
        releaseActiveFrag(signal);
        packLqhkeyreqLab(signal);
        return;
      }//if
    } else {
      jam();
      /* --------------------------------------------------------------------
       * A DIRTY OPERATION WHICH NEEDS LOGGING. WE START BY LOGGING THE 
       * REQUEST. IN THIS CASE WE WILL RELEASE THE FRAGMENT LOCK FIRST.
       * -------------------------------------------------------------------- 
       * A NORMAL WRITE OPERATION THAT NEEDS LOGGING AND WILL NOT BE 
       * PREMATURELY COMMITTED.                                   
       * -------------------------------------------------------------------- */
      releaseActiveFrag(signal);
      logLqhkeyreqLab(signal);
      return;
    }//if
  }//if
}//Dblqh::rwConcludedLab()

void Dblqh::rwConcludedAiLab(Signal* signal) 
{
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  fragptr.i = regTcPtr->fragmentptr;
  /* ------------------------------------------------------------------------
   * WE HAVE NOW CONCLUDED READING/WRITING IN ACC AND TUP FOR THIS OPERATION. 
   * IT IS NOW TIME TO LOG THE OPERATION, SEND REQUEST TO NEXT NODE OR TC AND 
   * FOR SOME TYPES OF OPERATIONS IT IS EVEN TIME TO COMMIT THE OPERATION.
   * IN THIS CASE WE HAVE ALREADY RELEASED THE FRAGMENT LOCK.
   * ERROR CASES AT FRAGMENT CREATION AND STAND-BY NODES ARE THE REASONS FOR
   * COMING HERE.
   * ------------------------------------------------------------------------ */
  if (regTcPtr->operation == ZREAD) {
    if (regTcPtr->opSimple == 1) {
      jam();
      /* --------------------------------------------------------------------
       * THE OPERATION IS A SIMPLE READ. WE WILL IMMEDIATELY COMMIT THE 
       * OPERATION.   
       * -------------------------------------------------------------------- */
      regTcPtr->gci = cnewestGci;
      localCommitLab(signal);
      return;
    } else {
      jam();
      /* --------------------------------------------------------------------
       * A NORMAL READ OPERATION IS NOT LOGGED BUT IS NOT COMMITTED UNTIL 
       * THE COMMIT SIGNAL ARRIVES. THUS WE CONTINUE PACKING THE RESPONSE.
       * -------------------------------------------------------------------- */
      ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
      packLqhkeyreqLab(signal);
      return;
    }//if
  } else {
    jam();
    ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
    if (fragptr.p->logFlag == Fragrecord::STATE_FALSE) {
      if (regTcPtr->dirtyOp == ZTRUE) {
	/* ------------------------------------------------------------------
	 * THIS OPERATION WAS A WRITE OPERATION THAT DO NOT NEED LOGGING AND 
	 * THAT CAN CAN  BE COMMITTED IMMEDIATELY. 
	 * ------------------------------------------------------------------ */
        jam();
	  /* ----------------------------------------------------------------
	   * IT MUST BE ACTIVE CREATION OF A FRAGMENT.
	   * ---------------------------------------------------------------- */
        regTcPtr->gci = cnewestGci;
        localCommitLab(signal);
        return;
      } else {
	/* ------------------------------------------------------------------
	 * A NORMAL WRITE OPERATION ON A FRAGMENT WHICH DO NOT NEED LOGGING. 
	 * WE WILL PACK THE REQUEST/RESPONSE TO THE NEXT NODE/TO TC. 
	 * ------------------------------------------------------------------ */
        jam();
	  /* ---------------------------------------------------------------
	   * IT MUST BE ACTIVE CREATION OF A FRAGMENT.          
	   * NOT A DIRTY OPERATION THUS PACK REQUEST/RESPONSE.
	   * ---------------------------------------------------------------- */
        regTcPtr->logWriteState = TcConnectionrec::NOT_WRITTEN;
        packLqhkeyreqLab(signal);
        return;
      }//if
    } else {
      jam();
      /* -------------------------------------------------------------------- 
       * A DIRTY OPERATION WHICH NEEDS LOGGING. WE START BY LOGGING THE 
       * REQUEST. IN THIS CASE WE WILL RELEASE THE FRAGMENT LOCK FIRST.
       * -------------------------------------------------------------------- */
      /* A NORMAL WRITE OPERATION THAT NEEDS LOGGING AND WILL NOT BE 
       * PREMATURELY COMMITTED.
       * -------------------------------------------------------------------- */
      logLqhkeyreqLab(signal);
      return;
    }//if
  }//if
}//Dblqh::rwConcludedAiLab()

/* ########################################################################## 
 * #######                            LOG MODULE                      ####### 
 *
 * ########################################################################## 
 * -------------------------------------------------------------------------- 
 *       THE LOG MODULE HANDLES THE READING AND WRITING OF THE LOG
 *       IT IS ALSO RESPONSIBLE FOR HANDLING THE SYSTEM RESTART. 
 *       IT CONTROLS THE SYSTEM RESTART IN TUP AND ACC AS WELL.
 * -------------------------------------------------------------------------- */
void Dblqh::logLqhkeyreqLab(Signal* signal) 
{
  UintR tcurrentFilepage;
  TcConnectionrecPtr tmpTcConnectptr;

  if (cnoOfLogPages < ZMIN_LOG_PAGES_OPERATION || ERROR_INSERTED(5032)) {
    jam();
    if(ERROR_INSERTED(5032)){
      CLEAR_ERROR_INSERT_VALUE;
    }
/*---------------------------------------------------------------------------*/
// The log disk is having problems in catching up with the speed of execution. 
// We must wait with writing the log of this operation to ensure we do not 
// overload the log.
/*---------------------------------------------------------------------------*/
    terrorCode = ZTEMPORARY_REDO_LOG_FAILURE;
    abortErrorLab(signal);
    return;
  }//if
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  logPartPtr.i = regTcPtr->hashValue & 3;
  ptrCheckGuard(logPartPtr, clogPartFileSize, logPartRecord);
/* -------------------------------------------------- */
/*       THIS PART IS USED TO WRITE THE LOG           */
/* -------------------------------------------------- */
/* -------------------------------------------------- */
/*       CHECK IF A LOG OPERATION IS ONGOING ALREADY. */
/*       IF SO THEN QUEUE THE OPERATION FOR LATER     */
/*       RESTART WHEN THE LOG PART IS FREE AGAIN.     */
/* -------------------------------------------------- */
  LogPartRecord * const regLogPartPtr = logPartPtr.p;

  if(ERROR_INSERTED(5033)){
    jam();
    CLEAR_ERROR_INSERT_VALUE;

    if ((regLogPartPtr->firstLogQueue != RNIL) &&
        (regLogPartPtr->LogLqhKeyReqSent == ZFALSE)) {
      /* -------------------------------------------------- */
      /*       WE HAVE A PROBLEM IN THAT THE LOG HAS NO     */
      /*       ROOM FOR ADDITIONAL OPERATIONS AT THE MOMENT.*/
      /* -------------------------------------------------- */
      /* -------------------------------------------------- */
      /*       WE MUST STILL RESTART QUEUED OPERATIONS SO   */
      /*       THEY ALSO CAN BE ABORTED.                    */
      /* -------------------------------------------------- */
      regLogPartPtr->LogLqhKeyReqSent = ZTRUE;
      signal->theData[0] = ZLOG_LQHKEYREQ;
      signal->theData[1] = logPartPtr.i;
      sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
    }//if
    
    terrorCode = ZTAIL_PROBLEM_IN_LOG_ERROR;
    abortErrorLab(signal);
    return;
  }
  
  if (regLogPartPtr->logPartState == LogPartRecord::IDLE) {
    ;
  } else if (regLogPartPtr->logPartState == LogPartRecord::ACTIVE) {
    jam();
    linkWaitLog(signal, logPartPtr);
    regTcPtr->transactionState = TcConnectionrec::LOG_QUEUED;
    return;
  } else {
    if ((regLogPartPtr->firstLogQueue != RNIL) &&
        (regLogPartPtr->LogLqhKeyReqSent == ZFALSE)) {
/* -------------------------------------------------- */
/*       WE HAVE A PROBLEM IN THAT THE LOG HAS NO     */
/*       ROOM FOR ADDITIONAL OPERATIONS AT THE MOMENT.*/
/* -------------------------------------------------- */
/* -------------------------------------------------- */
/*       WE MUST STILL RESTART QUEUED OPERATIONS SO   */
/*       THEY ALSO CAN BE ABORTED.                    */
/* -------------------------------------------------- */
      regLogPartPtr->LogLqhKeyReqSent = ZTRUE;
      signal->theData[0] = ZLOG_LQHKEYREQ;
      signal->theData[1] = logPartPtr.i;
      sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
    }//if
    if (regLogPartPtr->logPartState == LogPartRecord::TAIL_PROBLEM) {
      jam();
      terrorCode = ZTAIL_PROBLEM_IN_LOG_ERROR;
    } else {
      ndbrequire(regLogPartPtr->logPartState == LogPartRecord::FILE_CHANGE_PROBLEM);
      jam();
      terrorCode = ZFILE_CHANGE_PROBLEM_IN_LOG_ERROR;
    }//if
    abortErrorLab(signal);
    return;
  }//if
  regLogPartPtr->logPartState = LogPartRecord::ACTIVE;
  logFilePtr.i = regLogPartPtr->currentLogfile;
  ptrCheckGuard(logFilePtr, clogFileFileSize, logFileRecord);
/* -------------------------------------------------- */
/*       CHECK IF A NEW MBYTE IS TO BE STARTED. IF    */
/*       SO INSERT A NEXT LOG RECORD, WRITE THE LOG   */
/*       AND PLACE THE LOG POINTER ON THE NEW POSITION*/
/*       IF A NEW FILE IS TO BE USED, CHANGE FILE AND */
/*       ALSO START OPENING THE NEXT LOG FILE. IF A   */
/*       LAP HAS BEEN COMPLETED THEN ADD ONE TO LAP   */
/*       COUNTER.                                     */
/* -------------------------------------------------- */
  checkNewMbyte(signal);
/* -------------------------------------------------- */
/*       INSERT THE OPERATION RECORD LAST IN THE LIST */
/*       OF NOT COMPLETED OPERATIONS. ALSO RECORD THE */
/*       FILE NO, PAGE NO AND PAGE INDEX OF THE START */
/*       OF THIS LOG RECORD.                          */
/*       IT IS NOT ALLOWED TO INSERT IT INTO THE LIST */
/*       BEFORE CHECKING THE NEW MBYTE SINCE THAT WILL*/
/*       CAUSE THE OLD VALUES OF TC_CONNECTPTR TO BE  */
/*       USED IN WRITE_FILE_DESCRIPTOR.               */
/* -------------------------------------------------- */
  Uint32 tcIndex = tcConnectptr.i;
  tmpTcConnectptr.i = regLogPartPtr->lastLogTcrec;
  regLogPartPtr->lastLogTcrec = tcIndex;
  if (tmpTcConnectptr.i == RNIL) {
    jam();
    regLogPartPtr->firstLogTcrec = tcIndex;
  } else {
    ptrCheckGuard(tmpTcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
    tmpTcConnectptr.p->nextLogTcrec = tcIndex;
  }//if
  Uint32 fileNo = logFilePtr.p->fileNo;
  tcurrentFilepage = logFilePtr.p->currentFilepage;
  logPagePtr.i = logFilePtr.p->currentLogpage;
  regTcPtr->nextLogTcrec = RNIL;
  regTcPtr->prevLogTcrec = tmpTcConnectptr.i;
  ptrCheckGuard(logPagePtr, clogPageFileSize, logPageRecord);
  Uint32 pageIndex = logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX];
  regTcPtr->logStartFileNo = fileNo;
  regTcPtr->logStartPageNo = tcurrentFilepage;
  regTcPtr->logStartPageIndex = pageIndex;
/* -------------------------------------------------- */
/*       WRITE THE LOG HEADER OF THIS OPERATION.      */
/* -------------------------------------------------- */
  writeLogHeader(signal);
/* -------------------------------------------------- */
/*       WRITE THE TUPLE KEY OF THIS OPERATION.       */
/* -------------------------------------------------- */
  writeKey(signal);
/* -------------------------------------------------- */
/*       WRITE THE ATTRIBUTE INFO OF THIS OPERATION.  */
/* -------------------------------------------------- */
  writeAttrinfoLab(signal);

  logNextStart(signal);
/* -------------------------------------------------- */
/*       RESET THE STATE OF THE LOG PART. IF ANY      */
/*       OPERATIONS HAVE QUEUED THEN START THE FIRST  */
/*       OF THESE.                                    */
/* -------------------------------------------------- */
/* -------------------------------------------------- */
/*       CONTINUE WITH PACKING OF LQHKEYREQ           */
/* -------------------------------------------------- */
  tcurrentFilepage = logFilePtr.p->currentFilepage;
  if (logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] == ZPAGE_HEADER_SIZE) {
    jam();
    tcurrentFilepage--;
  }//if
  regTcPtr->logStopPageNo = tcurrentFilepage;
  regTcPtr->logWriteState = TcConnectionrec::WRITTEN;
  if (regTcPtr->abortState != TcConnectionrec::ABORT_IDLE) {
/* -------------------------------------------------- */
/*       AN ABORT HAVE BEEN ORDERED. THE ABORT WAITED */
/*       FOR THE LOG WRITE TO BE COMPLETED. NOW WE    */
/*       CAN PROCEED WITH THE NORMAL ABORT HANDLING.  */
/* -------------------------------------------------- */
    abortCommonLab(signal);
    return;
  }//if
  if (regTcPtr->dirtyOp != ZTRUE) {
    packLqhkeyreqLab(signal);
  } else {
    /* ----------------------------------------------------------------------
     * I NEED TO INSERT A COMMIT LOG RECORD SINCE WE ARE WRITING LOG IN THIS
     * TRANSACTION. SINCE WE RELEASED THE LOG LOCK JUST NOW NO ONE ELSE CAN BE
     * ACTIVE IN WRITING THE LOG. WE THUS WRITE THE LOG WITHOUT GETTING A LOCK
     * SINCE WE ARE ONLY WRITING A COMMIT LOG RECORD.
     * ---------------------------------------------------------------------- */
    writeCommitLog(signal, logPartPtr);
    /* ----------------------------------------------------------------------
     * DIRTY OPERATIONS SHOULD COMMIT BEFORE THEY PACK THE REQUEST/RESPONSE.
     * ---------------------------------------------------------------------- */
    regTcPtr->gci = cnewestGci;
    localCommitLab(signal);
  }//if
}//Dblqh::logLqhkeyreqLab()

/* ------------------------------------------------------------------------- */
/* -------                        SEND LQHKEYREQ                             */
/*                                                                           */
/* NO STATE CHECKING SINCE THE SIGNAL IS A LOCAL SIGNAL. THE EXECUTION OF    */
/* THE OPERATION IS COMPLETED. IT IS NOW TIME TO SEND THE OPERATION TO THE   */
/* NEXT REPLICA OR TO TC.                                                    */
/* ------------------------------------------------------------------------- */
void Dblqh::packLqhkeyreqLab(Signal* signal) 
{
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  if (regTcPtr->nextReplica == ZNIL) {
/* ------------------------------------------------------------------------- */
/* -------               SEND LQHKEYCONF                             ------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
    sendLqhkeyconfTc(signal, regTcPtr->tcBlockref);
    if (regTcPtr->dirtyOp != ZTRUE) {
      jam();
      regTcPtr->transactionState = TcConnectionrec::PREPARED;
      releaseOprec(signal);
    } else {
      jam();
/*************************************************************>*/
/*       DIRTY WRITES ARE USED IN TWO SITUATIONS. THE FIRST    */
/*       SITUATION IS WHEN THEY ARE USED TO UPDATE COUNTERS AND*/
/*       OTHER ATTRIBUTES WHICH ARE NOT SENSITIVE TO CONSISTE- */
/*       NCY. THE SECOND SITUATION IS BY OPERATIONS THAT ARE   */
/*       SENT AS PART OF A COPY FRAGMENT PROCESS.              */
/*                                                             */
/*       DURING A COPY FRAGMENT PROCESS THERE IS NO LOGGING    */
/*       ONGOING SINCE THE FRAGMENT IS NOT COMPLETE YET. THE   */
/*       LOGGING STARTS AFTER COMPLETING THE LAST COPY TUPLE   */
/*       OPERATION. THE EXECUTION OF THE LAST COPY TUPLE DOES  */
/*       ALSO START A LOCAL CHECKPOINT SO THAT THE FRAGMENT    */
/*       REPLICA IS RECOVERABLE. THUS GLOBAL CHECKPOINT ID FOR */
/*       THOSE OPERATIONS ARE NOT INTERESTING.                 */
/*                                                             */
/*       A DIRTY WRITE IS BY DEFINITION NOT CONSISTENT. THUS   */
/*       IT CAN USE ANY GLOBAL CHECKPOINT. THE IDEA HERE IS TO */
/*       ALWAYS USE THE LATEST DEFINED GLOBAL CHECKPOINT ID IN */
/*       THIS NODE.                                            */
/*************************************************************>*/
      cleanUp(signal);
    }//if
    return;
  }//if
/* ------------------------------------------------------------------------- */
/* -------            SEND LQHKEYREQ                                 ------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* THERE ARE MORE REPLICAS TO SEND THE OPERATION TO. A NEW LQHKEYREQ WILL BE */
/* PREPARED FOR THE NEXT REPLICA.                                            */
/* ------------------------------------------------------------------------- */
/* CLEAR REPLICA TYPE, ATTRINFO INDICATOR (IN LQHKEYREQ),                    */
/* INTERPRETED EXECUTION, SEQUENTIAL NUMBER OF REPLICA.                      */
// Set bit indicating Client and TC record not the same.
// Set readlenAi indicator if readlenAi != 0
// Stored Procedure Indicator not set.
/* ------------------------------------------------------------------------- */
  LqhKeyReq * const lqhKeyReq = (LqhKeyReq *)&signal->theData[0];

  UintR Treqinfo;
  UintR sig0, sig1, sig2, sig3, sig4, sig5, sig6;
  Treqinfo = preComputedRequestInfoMask & regTcPtr->reqinfo;

  UintR TapplAddressIndicator = (regTcPtr->nextSeqNoReplica == 0 ? 0 : 1);
  LqhKeyReq::setApplicationAddressFlag(Treqinfo, TapplAddressIndicator);
  LqhKeyReq::setInterpretedFlag(Treqinfo, regTcPtr->opExec);
  LqhKeyReq::setSeqNoReplica(Treqinfo, regTcPtr->nextSeqNoReplica);
  LqhKeyReq::setAIInLqhKeyReq(Treqinfo, regTcPtr->reclenAiLqhkey);
  UintR TreadLenAiInd = (regTcPtr->readlenAi == 0 ? 0 : 1);
  UintR TsameLqhAndClient = (tcConnectptr.i == 
                             regTcPtr->tcOprec ? 0 : 1);
  LqhKeyReq::setSameClientAndTcFlag(Treqinfo, TsameLqhAndClient);
  LqhKeyReq::setReturnedReadLenAIFlag(Treqinfo, TreadLenAiInd);

  UintR TotReclenAi = regTcPtr->totSendlenAi;
/* ------------------------------------------------------------------------- */
/* WE ARE NOW PREPARED TO SEND THE LQHKEYREQ. WE HAVE TO DECIDE IF ATTRINFO  */
/* IS INCLUDED IN THE LQHKEYREQ SIGNAL AND THEN SEND IT.                     */
/* TAKE OVER SCAN OPERATION IS NEVER USED ON BACKUPS, LOG RECORDS AND START-UP*/
/* OF NEW REPLICA AND THUS ONLY TOT_SENDLEN_AI IS USED THE UPPER 16 BITS ARE */
/* ZERO.                                                                     */
/* ------------------------------------------------------------------------- */
  sig0 = tcConnectptr.i;
  sig1 = regTcPtr->savePointId;
  sig2 = regTcPtr->hashValue;
  sig4 = regTcPtr->tcBlockref;

  lqhKeyReq->clientConnectPtr = sig0;
  lqhKeyReq->attrLen = TotReclenAi;
  lqhKeyReq->savePointId = sig1;
  lqhKeyReq->hashValue = sig2;
  lqhKeyReq->requestInfo = Treqinfo;
  lqhKeyReq->tcBlockref = sig4;

  sig0 = regTcPtr->tableref + ((regTcPtr->schemaVersion << 16) & 0xFFFF0000);
  sig1 = regTcPtr->fragmentid + (regTcPtr->nodeAfterNext[0] << 16);
  sig2 = regTcPtr->transid[0];
  sig3 = regTcPtr->transid[1];
  sig4 = regTcPtr->applRef;
  sig5 = regTcPtr->applOprec;
  sig6 = regTcPtr->tcOprec;
  UintR nextPos = (TapplAddressIndicator << 1);

  lqhKeyReq->tableSchemaVersion = sig0;
  lqhKeyReq->fragmentData = sig1;
  lqhKeyReq->transId1 = sig2;
  lqhKeyReq->transId2 = sig3;
  lqhKeyReq->noFiredTriggers = regTcPtr->noFiredTriggers;
  lqhKeyReq->variableData[0] = sig4;
  lqhKeyReq->variableData[1] = sig5;
  lqhKeyReq->variableData[2] = sig6;

  nextPos += TsameLqhAndClient;

  if ((regTcPtr->lastReplicaNo - regTcPtr->nextSeqNoReplica) > 1) {
    sig0 = (UintR)regTcPtr->nodeAfterNext[1] +
           (UintR)(regTcPtr->nodeAfterNext[2] << 16);
    lqhKeyReq->variableData[nextPos] = sig0;
    nextPos++;
  }//if
  sig0 = regTcPtr->readlenAi;
  sig1 = regTcPtr->tupkeyData[0];
  sig2 = regTcPtr->tupkeyData[1];
  sig3 = regTcPtr->tupkeyData[2];
  sig4 = regTcPtr->tupkeyData[3];

  lqhKeyReq->variableData[nextPos] = sig0;
  nextPos += TreadLenAiInd;
  lqhKeyReq->variableData[nextPos] = sig1;
  lqhKeyReq->variableData[nextPos + 1] = sig2;
  lqhKeyReq->variableData[nextPos + 2] = sig3;
  lqhKeyReq->variableData[nextPos + 3] = sig4;
  UintR TkeyLen = LqhKeyReq::getKeyLen(Treqinfo);
  if (TkeyLen < 4) {
    nextPos += TkeyLen;
  } else {
    nextPos += 4;
  }//if

  sig0 = regTcPtr->firstAttrinfo[0];
  sig1 = regTcPtr->firstAttrinfo[1];
  sig2 = regTcPtr->firstAttrinfo[2];
  sig3 = regTcPtr->firstAttrinfo[3];
  sig4 = regTcPtr->firstAttrinfo[4];
  UintR TAiLen = regTcPtr->reclenAiLqhkey;
  BlockReference lqhRef = calcLqhBlockRef(regTcPtr->nextReplica);

  lqhKeyReq->variableData[nextPos] = sig0;
  lqhKeyReq->variableData[nextPos + 1] = sig1;
  lqhKeyReq->variableData[nextPos + 2] = sig2;
  lqhKeyReq->variableData[nextPos + 3] = sig3;
  lqhKeyReq->variableData[nextPos + 4] = sig4;

  nextPos += TAiLen;

  sendSignal(lqhRef, GSN_LQHKEYREQ, signal, 
             nextPos + LqhKeyReq::FixedSignalLength, JBB);
  if (regTcPtr->primKeyLen > 4) {
    jam();
/* ------------------------------------------------------------------------- */
/* MORE THAN 4 WORDS OF KEY DATA IS IN THE OPERATION. THEREFORE WE NEED TO   */
/* PREPARE A KEYINFO SIGNAL. MORE THAN ONE KEYINFO SIGNAL CAN BE SENT.       */
/* ------------------------------------------------------------------------- */
    sendTupkey(signal);
  }//if
/* ------------------------------------------------------------------------- */
/* NOW I AM PREPARED TO SEND ALL THE ATTRINFO SIGNALS. AT THE MOMENT A LOOP  */
/* SENDS ALL AT ONCE. LATER WE HAVE TO ADDRESS THE PROBLEM THAT THESE COULD  */
/* LEAD TO BUFFER EXPLOSION => NODE CRASH.                                   */
/* ------------------------------------------------------------------------- */
/*       NEW CODE TO SEND ATTRINFO IN PACK_LQHKEYREQ  */
/*       THIS CODE USES A REAL-TIME BREAK AFTER       */
/*       SENDING 16 SIGNALS.                          */
/* -------------------------------------------------- */
  sig0 = regTcPtr->tcOprec;
  sig1 = regTcPtr->transid[0];
  sig2 = regTcPtr->transid[1];
  signal->theData[0] = sig0;
  signal->theData[1] = sig1;
  signal->theData[2] = sig2;
  AttrbufPtr regAttrinbufptr;
  regAttrinbufptr.i = regTcPtr->firstAttrinbuf;
  while (regAttrinbufptr.i != RNIL) {
    ptrCheckGuard(regAttrinbufptr, cattrinbufFileSize, attrbuf);
    jam();
    Uint32 dataLen = regAttrinbufptr.p->attrbuf[ZINBUF_DATA_LEN];
    ndbrequire(dataLen != 0);
    MEMCOPY_NO_WORDS(&signal->theData[3], &regAttrinbufptr.p->attrbuf[0], dataLen);
    regAttrinbufptr.i = regAttrinbufptr.p->attrbuf[ZINBUF_NEXT];
    sendSignal(lqhRef, GSN_ATTRINFO, signal, dataLen + 3, JBB);
  }//while
  regTcPtr->transactionState = TcConnectionrec::PREPARED;
  if (regTcPtr->dirtyOp == ZTRUE) {
    jam();
/*************************************************************>*/
/*       DIRTY WRITES ARE USED IN TWO SITUATIONS. THE FIRST    */
/*       SITUATION IS WHEN THEY ARE USED TO UPDATE COUNTERS AND*/
/*       OTHER ATTRIBUTES WHICH ARE NOT SENSITIVE TO CONSISTE- */
/*       NCY. THE SECOND SITUATION IS BY OPERATIONS THAT ARE   */
/*       SENT AS PART OF A COPY FRAGMENT PROCESS.              */
/*                                                             */
/*       DURING A COPY FRAGMENT PROCESS THERE IS NO LOGGING    */
/*       ONGOING SINCE THE FRAGMENT IS NOT COMPLETE YET. THE   */
/*       LOGGING STARTS AFTER COMPLETING THE LAST COPY TUPLE   */
/*       OPERATION. THE EXECUTION OF THE LAST COPY TUPLE DOES  */
/*       ALSO START A LOCAL CHECKPOINT SO THAT THE FRAGMENT    */
/*       REPLICA IS RECOVERABLE. THUS GLOBAL CHECKPOINT ID FOR */
/*       THOSE OPERATIONS ARE NOT INTERESTING.                 */
/*                                                             */
/*       A DIRTY WRITE IS BY DEFINITION NOT CONSISTENT. THUS   */
/*       IT CAN USE ANY GLOBAL CHECKPOINT. THE IDEA HERE IS TO */
/*       ALWAYS USE THE LATEST DEFINED GLOBAL CHECKPOINT ID IN */
/*       THIS NODE.                                            */
/*************************************************************>*/
    cleanUp(signal);
    return;
  }//if
  /* ------------------------------------------------------------------------ 
   *   ALL INFORMATION NEEDED BY THE COMMIT PHASE AND COMPLETE PHASE IS 
   *   KEPT IN THE TC_CONNECT RECORD. TO ENSURE PROPER USE OF MEMORY 
   *   RESOURCES WE DEALLOCATE THE ATTRINFO RECORD AND KEY RECORDS 
   *   AS SOON AS POSSIBLE.
   * ------------------------------------------------------------------------ */
  releaseOprec(signal);
}//Dblqh::packLqhkeyreqLab()

/* ========================================================================= */
/* ==== CHECK IF THE LOG RECORD FITS INTO THE CURRENT MBYTE,         ======= */
/*      OTHERWISE SWITCH TO NEXT MBYTE.                                      */
/*                                                                           */
/* ========================================================================= */
void Dblqh::checkNewMbyte(Signal* signal) 
{
  UintR tcnmTmp;
  UintR ttotalLogSize;

/* -------------------------------------------------- */
/*       CHECK IF A NEW MBYTE OF LOG RECORD IS TO BE  */
/*       OPENED BEFORE WRITING THE LOG RECORD. NO LOG */
/*       RECORDS ARE ALLOWED TO SPAN A MBYTE BOUNDARY */
/*                                                    */
/*       INPUT:  TC_CONNECTPTR   THE OPERATION        */
/*               LOG_FILE_PTR    THE LOG FILE         */
/*       OUTPUT: LOG_FILE_PTR    THE NEW LOG FILE     */
/* -------------------------------------------------- */
  ttotalLogSize = ZLOG_HEAD_SIZE + tcConnectptr.p->currTupAiLen;
  ttotalLogSize = ttotalLogSize + tcConnectptr.p->primKeyLen;
  tcnmTmp = logFilePtr.p->remainingWordsInMbyte;
  if ((ttotalLogSize + ZNEXT_LOG_SIZE) <= tcnmTmp) {
    ndbrequire(tcnmTmp >= ttotalLogSize);
    logFilePtr.p->remainingWordsInMbyte = tcnmTmp - ttotalLogSize;
    return;
  } else {
    jam();
/* -------------------------------------------------- */
/*       IT WAS NOT ENOUGH SPACE IN THIS MBYTE FOR    */
/*       THIS LOG RECORD. MOVE TO NEXT MBYTE          */
/*       THIS MIGHT INCLUDE CHANGING LOG FILE         */
/* -------------------------------------------------- */
/*       WE HAVE TO INSERT A NEXT LOG RECORD FIRST    */
/* -------------------------------------------------- */
/*       THEN CONTINUE BY WRITING THE FILE DESCRIPTORS*/
/* -------------------------------------------------- */
    logPagePtr.i = logFilePtr.p->currentLogpage;
    ptrCheckGuard(logPagePtr, clogPageFileSize, logPageRecord);
    changeMbyte(signal);
    tcnmTmp = logFilePtr.p->remainingWordsInMbyte;
  }//if
  ndbrequire(tcnmTmp >= ttotalLogSize);
  logFilePtr.p->remainingWordsInMbyte = tcnmTmp - ttotalLogSize;
}//Dblqh::checkNewMbyte()

/* --------------------------------------------------------------------------
 * -------               WRITE OPERATION HEADER TO LOG                ------- 
 * 
 *       SUBROUTINE SHORT NAME: WLH
 * ------------------------------------------------------------------------- */
void Dblqh::writeLogHeader(Signal* signal) 
{
  Uint32 logPos = logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX];
  Uint32 hashValue = tcConnectptr.p->hashValue;
  Uint32 operation = tcConnectptr.p->operation;
  Uint32 keyLen = tcConnectptr.p->primKeyLen;
  Uint32 aiLen = tcConnectptr.p->currTupAiLen;
  Uint32 totLogLen = aiLen + keyLen + ZLOG_HEAD_SIZE;
  if ((logPos + ZLOG_HEAD_SIZE) < ZPAGE_SIZE) {
    Uint32* dataPtr = &logPagePtr.p->logPageWord[logPos];
    logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] = logPos + ZLOG_HEAD_SIZE;
    dataPtr[0] = ZPREP_OP_TYPE;
    dataPtr[1] = totLogLen;
    dataPtr[2] = hashValue;
    dataPtr[3] = operation;
    dataPtr[4] = aiLen;
    dataPtr[5] = keyLen;
  } else {
    writeLogWord(signal, ZPREP_OP_TYPE);
    writeLogWord(signal, totLogLen);
    writeLogWord(signal, hashValue);
    writeLogWord(signal, operation);
    writeLogWord(signal, aiLen);
    writeLogWord(signal, keyLen);
  }//if
}//Dblqh::writeLogHeader()

/* --------------------------------------------------------------------------
 * -------               WRITE TUPLE KEY TO LOG                       ------- 
 *
 *       SUBROUTINE SHORT NAME: WK
 * ------------------------------------------------------------------------- */
void Dblqh::writeKey(Signal* signal) 
{
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  Uint32 logPos, endPos, dataLen;
  Int32 remainingLen;
  logPos = logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX];
  remainingLen = regTcPtr->primKeyLen;
  dataLen = remainingLen;
  if (remainingLen > 4)
    dataLen = 4;
  remainingLen -= dataLen;
  endPos = logPos + dataLen;
  if (endPos < ZPAGE_SIZE) {
    MEMCOPY_NO_WORDS(&logPagePtr.p->logPageWord[logPos],
                     &regTcPtr->tupkeyData[0],
                     dataLen);
  } else {
    jam();
    for (Uint32 i = 0; i < dataLen; i++)
      writeLogWord(signal, regTcPtr->tupkeyData[i]);
    endPos = logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX];
  }//if
  DatabufPtr regDatabufptr;
  regDatabufptr.i = regTcPtr->firstTupkeybuf;
  while (remainingLen > 0) {
    logPos = endPos;
    ptrCheckGuard(regDatabufptr, cdatabufFileSize, databuf);
    dataLen = remainingLen;
    if (remainingLen > 4)
      dataLen = 4;
    remainingLen -= dataLen;
    endPos += dataLen;
    if (endPos < ZPAGE_SIZE) {
      MEMCOPY_NO_WORDS(&logPagePtr.p->logPageWord[logPos],
                       &regDatabufptr.p->data[0],
                       dataLen);
    } else {
      logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] = logPos;
      for (Uint32 i = 0; i < dataLen; i++)
        writeLogWord(signal, regDatabufptr.p->data[i]);
      endPos = logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX];
    }//if
    regDatabufptr.i = regDatabufptr.p->nextDatabuf;
  }//while
  logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] = endPos;
  ndbrequire(regDatabufptr.i == RNIL);
}//Dblqh::writeKey()

/* --------------------------------------------------------------------------
 * -------               WRITE ATTRINFO TO LOG                        ------- 
 *
 *       SUBROUTINE SHORT NAME: WA
 * ------------------------------------------------------------------------- */
void Dblqh::writeAttrinfoLab(Signal* signal) 
{
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  Uint32 totLen = regTcPtr->currTupAiLen;
  if (totLen == 0)
    return;
  Uint32 logPos = logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX];
  Uint32 lqhLen = regTcPtr->reclenAiLqhkey;
  ndbrequire(totLen >= lqhLen);
  Uint32 endPos = logPos + lqhLen;
  totLen -= lqhLen;
  if (endPos < ZPAGE_SIZE) {
    MEMCOPY_NO_WORDS(&logPagePtr.p->logPageWord[logPos],
                     &regTcPtr->firstAttrinfo[0],
                     lqhLen);
  } else {
    for (Uint32 i = 0; i < lqhLen; i++)
      writeLogWord(signal, regTcPtr->firstAttrinfo[i]);
    endPos = logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX];
  }//if
  AttrbufPtr regAttrinbufptr;
  regAttrinbufptr.i = regTcPtr->firstAttrinbuf;
  while (totLen > 0) {
    logPos = endPos;
    ptrCheckGuard(regAttrinbufptr, cattrinbufFileSize, attrbuf);
    Uint32 dataLen = regAttrinbufptr.p->attrbuf[ZINBUF_DATA_LEN];
    ndbrequire(totLen >= dataLen);
    ndbrequire(dataLen > 0);
    totLen -= dataLen;
    endPos += dataLen;
    if (endPos < ZPAGE_SIZE) {
      MEMCOPY_NO_WORDS(&logPagePtr.p->logPageWord[logPos],
                      &regAttrinbufptr.p->attrbuf[0],
                      dataLen);
    } else {
      logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] = logPos;
      for (Uint32 i = 0; i < dataLen; i++)
        writeLogWord(signal, regAttrinbufptr.p->attrbuf[i]);
      endPos = logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX];
    }//if
    regAttrinbufptr.i = regAttrinbufptr.p->attrbuf[ZINBUF_NEXT];
  }//while
  logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] = endPos;
  ndbrequire(regAttrinbufptr.i == RNIL);
}//Dblqh::writeAttrinfoLab()

/* ------------------------------------------------------------------------- */
/* -------          SEND TUPLE KEY IN KEYINFO SIGNAL(S)              ------- */
/*                                                                           */
/*       SUBROUTINE SHORT NAME: STU                                          */
/* ------------------------------------------------------------------------- */
void Dblqh::sendTupkey(Signal* signal) 
{
  UintR TdataPos = 3;
  BlockReference lqhRef = calcLqhBlockRef(tcConnectptr.p->nextReplica);
  signal->theData[0] = tcConnectptr.p->tcOprec;
  signal->theData[1] = tcConnectptr.p->transid[0];
  signal->theData[2] = tcConnectptr.p->transid[1];
  databufptr.i = tcConnectptr.p->firstTupkeybuf;
  do {
    ptrCheckGuard(databufptr, cdatabufFileSize, databuf);
    signal->theData[TdataPos] = databufptr.p->data[0];
    signal->theData[TdataPos + 1] = databufptr.p->data[1];
    signal->theData[TdataPos + 2] = databufptr.p->data[2];
    signal->theData[TdataPos + 3] = databufptr.p->data[3];

    databufptr.i = databufptr.p->nextDatabuf;
    TdataPos += 4;
    if (databufptr.i == RNIL) {
      jam();
      sendSignal(lqhRef, GSN_KEYINFO, signal, TdataPos, JBB);
      return;
    } else if (TdataPos == 23) {
      jam();
      sendSignal(lqhRef, GSN_KEYINFO, signal, 23, JBB);
      TdataPos = 3;
    }
  } while (1);
}//Dblqh::sendTupkey()

void Dblqh::cleanUp(Signal* signal) 
{
  releaseOprec(signal);
  deleteTransidHash(signal);
  releaseTcrec(signal, tcConnectptr);
}//Dblqh::cleanUp()

/* --------------------------------------------------------------------------
 * ---- RELEASE ALL RECORDS CONNECTED TO THE OPERATION RECORD AND THE    ---- 
 *      OPERATION RECORD ITSELF
 * ------------------------------------------------------------------------- */
void Dblqh::releaseOprec(Signal* signal) 
{
  UintR Tmpbuf;
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
/* ---- RELEASE DATA BUFFERS ------------------- */
  DatabufPtr regDatabufptr;
  regDatabufptr.i = regTcPtr->firstTupkeybuf;
/* --------------------------------------------------------------------------
 * -------       RELEASE DATA BUFFERS                                 ------- 
 * 
 * ------------------------------------------------------------------------- */

  while (regDatabufptr.i != RNIL) {
    jam();
    ptrCheckGuard(regDatabufptr, cdatabufFileSize, databuf);
    Tmpbuf = regDatabufptr.p->nextDatabuf;
    regDatabufptr.p->nextDatabuf = cfirstfreeDatabuf;
    cfirstfreeDatabuf = regDatabufptr.i;
    regDatabufptr.i = Tmpbuf;
  }//while
/* ---- RELEASE ATTRINFO BUFFERS ------------------- */
  AttrbufPtr regAttrinbufptr;
  regAttrinbufptr.i = regTcPtr->firstAttrinbuf;
  /* ########################################################################
   * #######                            RELEASE_ATTRINBUF             #######
   *
   * ####################################################################### */
  while (regAttrinbufptr.i != RNIL) {
    jam();
    regAttrinbufptr.i= release_attrinbuf(regAttrinbufptr.i);
  }//while
  regTcPtr->firstAttrinbuf = RNIL;
  regTcPtr->lastAttrinbuf = RNIL;
  regTcPtr->firstTupkeybuf = RNIL;
  regTcPtr->lastTupkeybuf = RNIL;
}//Dblqh::releaseOprec()

/* ------------------------------------------------------------------------- */
/* ------         DELETE TRANSACTION ID FROM HASH TABLE              ------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void Dblqh::deleteTransidHash(Signal* signal) 
{
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  TcConnectionrecPtr prevHashptr;
  TcConnectionrecPtr nextHashptr;

  prevHashptr.i = regTcPtr->prevHashRec;
  nextHashptr.i = regTcPtr->nextHashRec;
  if (prevHashptr.i != RNIL) {
    jam();
    ptrCheckGuard(prevHashptr, ctcConnectrecFileSize, tcConnectionrec);
    prevHashptr.p->nextHashRec = nextHashptr.i;
  } else {
    jam();
/* ------------------------------------------------------------------------- */
/* THE OPERATION WAS PLACED FIRST IN THE LIST OF THE HASH TABLE. NEED TO SET */
/* A NEW LEADER OF THE LIST.                                                 */
/* ------------------------------------------------------------------------- */
    Uint32 hashIndex = (regTcPtr->transid[0] ^ regTcPtr->tcOprec) & 1023;
    ctransidHash[hashIndex] = nextHashptr.i;
  }//if
  if (nextHashptr.i != RNIL) {
    jam();
    ptrCheckGuard(nextHashptr, ctcConnectrecFileSize, tcConnectionrec);
    nextHashptr.p->prevHashRec = prevHashptr.i;
  }//if
}//Dblqh::deleteTransidHash()

/* --------------------------------------------------------------------------
 * -------               LINK OPERATION IN ACTIVE LIST ON FRAGMENT    -------
 *
 *       SUBROUTINE SHORT NAME: LAF
// Input Pointers:
// tcConnectptr
// fragptr
 * ------------------------------------------------------------------------- */
void Dblqh::linkActiveFrag(Signal* signal) 
{
  TcConnectionrecPtr lafTcConnectptr;
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  Fragrecord * const regFragPtr = fragptr.p;
  Uint32 tcIndex = tcConnectptr.i;
  lafTcConnectptr.i = regFragPtr->activeList;
  regTcPtr->prevTc = RNIL;
  regFragPtr->activeList = tcIndex;
  ndbrequire(regTcPtr->listState == TcConnectionrec::NOT_IN_LIST);
  regTcPtr->nextTc = lafTcConnectptr.i;
  regTcPtr->listState = TcConnectionrec::IN_ACTIVE_LIST;
  if (lafTcConnectptr.i == RNIL) {
    return;
  } else {
    jam();
    ptrCheckGuard(lafTcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
    lafTcConnectptr.p->prevTc = tcIndex;
  }//if
  return;
}//Dblqh::linkActiveFrag()

/* -------------------------------------------------------------------------
 * -------       RELEASE OPERATION FROM ACTIVE LIST ON FRAGMENT      ------- 
 * 
 *       SUBROUTINE SHORT NAME = RAF
 * ------------------------------------------------------------------------- */
void Dblqh::releaseActiveFrag(Signal* signal) 
{
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  TcConnectionrecPtr ralTcNextConnectptr;
  TcConnectionrecPtr ralTcPrevConnectptr;
  fragptr.i = regTcPtr->fragmentptr;
  ralTcPrevConnectptr.i = regTcPtr->prevTc;
  ralTcNextConnectptr.i = regTcPtr->nextTc;
  ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
  Fragrecord * const regFragPtr = fragptr.p;
  ndbrequire(regTcPtr->listState == TcConnectionrec::IN_ACTIVE_LIST);
  regTcPtr->listState = TcConnectionrec::NOT_IN_LIST;

  if (ralTcNextConnectptr.i != RNIL) {
    jam();
    ptrCheckGuard(ralTcNextConnectptr, ctcConnectrecFileSize, tcConnectionrec);
    ralTcNextConnectptr.p->prevTc = ralTcPrevConnectptr.i;
  }//if
  if (ralTcPrevConnectptr.i != RNIL) {
    jam();
    ptrCheckGuard(ralTcPrevConnectptr, ctcConnectrecFileSize, tcConnectionrec);
    ralTcPrevConnectptr.p->nextTc = regTcPtr->nextTc;
  } else {
    jam();
    /* ----------------------------------------------------------------------
     *   OPERATION RECORD IS FIRST IN ACTIVE LIST    
     *   THIS MEANS THAT THERE EXISTS NO PREVIOUS TC THAT NEEDS TO BE UPDATED.
     * --------------------------------------------------------------------- */
    regFragPtr->activeList = ralTcNextConnectptr.i;
  }//if
  if (regFragPtr->lcpRef != RNIL) {
    jam();
    lcpPtr.i = regFragPtr->lcpRef;
    ptrCheckGuard(lcpPtr, clcpFileSize, lcpRecord);
    ndbrequire(lcpPtr.p->lcpState == LcpRecord::LCP_WAIT_ACTIVE_FINISH);

    /* --------------------------------------------------------------------
     *   IF A FRAGMENT IS CURRENTLY STARTING A LOCAL CHECKPOINT AND IT 
     *   IS WAITING FOR ACTIVE OPERATIONS TO BE COMPLETED WITH THE 
     *   CURRENT PHASE, THEN IT IS CHECKED WHETHER THE 
     *   LAST ACTIVE OPERATION WAS NOW COMPLETED.
     * ------------------------------------------------------------------- */
    if (regFragPtr->activeList == RNIL) {
      jam();
      /* ------------------------------------------------------------------
       *       ACTIVE LIST ON FRAGMENT IS EMPTY AND WE ARE WAITING FOR 
       *       THIS TO HAPPEN.    
       *       WE WILL NOW START THE CHECKPOINT IN TUP AND ACC.
       * ----------------------------------------------------------------- */
      /*      SEND START LOCAL CHECKPOINT TO ACC AND TUP                   */
      /* ----------------------------------------------------------------- */
      fragptr.p->lcpRef = RNIL;
      lcpPtr.p->lcpState = LcpRecord::LCP_START_CHKP;
      sendStartLcp(signal);
    }//if
  }//if
}//Dblqh::releaseActiveFrag()

/* ######################################################################### */
/* #######                   TRANSACTION MODULE                      ####### */
/*      THIS MODULE HANDLES THE COMMIT AND THE COMPLETE PHASE.               */
/* ######################################################################### */
void Dblqh::warningReport(Signal* signal, int place)
{
  switch (place) {
  case 0:
    jam();
#ifdef ABORT_TRACE
    ndbout << "W: Received COMMIT in wrong state in Dblqh" << endl;
#endif
    break;
  case 1:
    jam();
#ifdef ABORT_TRACE
    ndbout << "W: Received COMMIT with wrong transid in Dblqh" << endl;
#endif
    break;
  case 2:
    jam();
#ifdef ABORT_TRACE
    ndbout << "W: Received COMPLETE in wrong state in Dblqh" << endl;
#endif
    break;
  case 3:
    jam();
#ifdef ABORT_TRACE
    ndbout << "W: Received COMPLETE with wrong transid in Dblqh" << endl;
#endif
    break;
  case 4:
    jam();
#ifdef ABORT_TRACE
    ndbout << "W: Received COMMITREQ in wrong state in Dblqh" << endl;
#endif
    break;
  case 5:
    jam();
#ifdef ABORT_TRACE
    ndbout << "W: Received COMMITREQ with wrong transid in Dblqh" << endl;
#endif
    break;
  case 6:
    jam();
#ifdef ABORT_TRACE
    ndbout << "W: Received COMPLETEREQ in wrong state in Dblqh" << endl;
#endif
    break;
  case 7:
    jam();
#ifdef ABORT_TRACE
    ndbout << "W: Received COMPLETEREQ with wrong transid in Dblqh" << endl;
#endif
    break;
  case 8:
    jam();
#ifdef ABORT_TRACE
    ndbout << "W: Received ABORT with non-existing transid in Dblqh" << endl;
#endif
    break;
  case 9:
    jam();
#ifdef ABORT_TRACE
    ndbout << "W: Received ABORTREQ with non-existing transid in Dblqh" << endl;
#endif
    break;
  case 10:
    jam();
#ifdef ABORT_TRACE
    ndbout << "W: Received ABORTREQ in wrong state in Dblqh" << endl;
#endif
    break;
  case 11:
    jam();
#ifdef ABORT_TRACE
    ndbout << "W: Received COMMIT when tc-rec released in Dblqh" << endl;
#endif
    break;
  case 12:
    jam();
#ifdef ABORT_TRACE
    ndbout << "W: Received COMPLETE when tc-rec released in Dblqh" << endl;
#endif
    break;
  case 13:
    jam();
#ifdef ABORT_TRACE
    ndbout << "W: Received LQHKEYREF when tc-rec released in Dblqh" << endl;
#endif
    break;
  case 14:
    jam();
#ifdef ABORT_TRACE
    ndbout << "W: Received LQHKEYREF with wrong transid in Dblqh" << endl;
#endif
    break;
  case 15:
    jam();
#ifdef ABORT_TRACE
    ndbout << "W: Received LQHKEYREF when already aborting in Dblqh" << endl;
#endif
    break;
  case 16:
    jam();
    ndbrequire(cstartPhase == ZNIL);
#ifdef ABORT_TRACE
    ndbout << "W: Received LQHKEYREF in wrong state in Dblqh" << endl;
#endif
    break;
  default:
    jam();
    break;
  }//switch
  return;
}//Dblqh::warningReport()

void Dblqh::errorReport(Signal* signal, int place)
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
  default:
    jam();
    break;
  }//switch
  systemErrorLab(signal);
  return;
}//Dblqh::errorReport()

/* ************************************************************************>>
 *  COMMIT: Start commit request from TC. This signal is originally sent as a
 *  packed signal and this function is called from execPACKED_SIGNAL.
 *  This is the normal commit protocol where TC first send this signal to the
 *  backup node which then will send COMMIT to the primary node. If 
 *  everything is ok the primary node send COMMITTED back to TC.
 * ************************************************************************>> */
void Dblqh::execCOMMIT(Signal* signal) 
{
  TcConnectionrec *regTcConnectionrec = tcConnectionrec;
  Uint32 ttcConnectrecFileSize = ctcConnectrecFileSize;
  Uint32 tcIndex = signal->theData[0];
  Uint32 gci = signal->theData[1];
  Uint32 transid1 = signal->theData[2];
  Uint32 transid2 = signal->theData[3];
  jamEntry();
  if (tcIndex >= ttcConnectrecFileSize) {
    errorReport(signal, 0);
    return;
  }//if
  if (ERROR_INSERTED(5011)) {
    CLEAR_ERROR_INSERT_VALUE;
    sendSignalWithDelay(cownref, GSN_COMMIT, signal, 2000, 4);
    return;
  }//if
  if (ERROR_INSERTED(5012)) {
    SET_ERROR_INSERT_VALUE(5017);
    sendSignalWithDelay(cownref, GSN_COMMIT, signal, 2000, 4);
    return;
  }//if
  tcConnectptr.i = tcIndex;
  ptrAss(tcConnectptr, regTcConnectionrec);
  if ((tcConnectptr.p->transid[0] == transid1) &&
      (tcConnectptr.p->transid[1] == transid2)) {
    commitReqLab(signal, gci);
    return;
  }//if
  warningReport(signal, 1);
  return;
}//Dblqh::execCOMMIT()

/* ************************************************************************>> 
 *  COMMITREQ: Commit request from TC. This is the commit protocol used if
 *  one of the nodes is not behaving correctly. TC explicitly sends COMMITREQ 
 *  to both the backup and primary node and gets a COMMITCONF back if the 
 *  COMMIT was ok. 
 * ************************************************************************>> */
void Dblqh::execCOMMITREQ(Signal* signal) 
{
  jamEntry();
  Uint32 reqPtr = signal->theData[0];
  BlockReference reqBlockref = signal->theData[1];
  Uint32 gci = signal->theData[2];
  Uint32 transid1 = signal->theData[3];
  Uint32 transid2 = signal->theData[4];
  Uint32 tcOprec = signal->theData[6];
  if (ERROR_INSERTED(5004)) {
    systemErrorLab(signal);
  }
  if (ERROR_INSERTED(5017)) {
    CLEAR_ERROR_INSERT_VALUE;
    sendSignalWithDelay(cownref, GSN_COMMITREQ, signal, 2000, 7);
    return;
  }//if
  if (findTransaction(transid1,
                      transid2,
                      tcOprec) != ZOK) {
    warningReport(signal, 5);
    return;
  }//if
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  switch (regTcPtr->transactionState) {
  case TcConnectionrec::PREPARED:
  case TcConnectionrec::LOG_COMMIT_QUEUED_WAIT_SIGNAL:
  case TcConnectionrec::LOG_COMMIT_WRITTEN_WAIT_SIGNAL:
    jam();
/*-------------------------------------------------------*/
/*       THE NORMAL CASE.                                */
/*-------------------------------------------------------*/
    regTcPtr->reqBlockref = reqBlockref;
    regTcPtr->reqRef = reqPtr;
    regTcPtr->abortState = TcConnectionrec::REQ_FROM_TC;
    commitReqLab(signal, gci);
    return;
    break;
  case TcConnectionrec::COMMITTED:
    jam();
/*---------------------------------------------------------*/
/*       FOR SOME REASON THE COMMIT PHASE HAVE BEEN        */
/*       FINISHED AFTER A TIME OUT. WE NEED ONLY SEND A    */
/*       COMMITCONF SIGNAL.                                */
/*---------------------------------------------------------*/
    regTcPtr->reqBlockref = reqBlockref;
    regTcPtr->reqRef = reqPtr;
    regTcPtr->abortState = TcConnectionrec::REQ_FROM_TC;
    signal->theData[0] = regTcPtr->reqRef;
    signal->theData[1] = cownNodeid;
    signal->theData[2] = regTcPtr->transid[0];
    signal->theData[3] = regTcPtr->transid[1];
    sendSignal(regTcPtr->reqBlockref, GSN_COMMITCONF, signal, 4, JBB);
    break;
  case TcConnectionrec::COMMIT_STOPPED:
    jam();
    regTcPtr->reqBlockref = reqBlockref;
    regTcPtr->reqRef = reqPtr;
    regTcPtr->abortState = TcConnectionrec::REQ_FROM_TC;
    /*empty*/;
    break;
  default:
    jam();
    warningReport(signal, 4);
    return;
    break;
  }//switch
  return;
}//Dblqh::execCOMMITREQ()

/* ************************************************************************>>
 *  COMPLETE : Complete the transaction. Sent as a packed signal from TC.
 *  Works the same way as COMMIT protocol. This is the normal case with both 
 *  primary and backup working (See COMMIT).
 * ************************************************************************>> */
void Dblqh::execCOMPLETE(Signal* signal) 
{
  TcConnectionrec *regTcConnectionrec = tcConnectionrec;
  Uint32 ttcConnectrecFileSize = ctcConnectrecFileSize;
  Uint32 tcIndex = signal->theData[0];
  Uint32 transid1 = signal->theData[1];
  Uint32 transid2 = signal->theData[2];
  jamEntry();
  if (tcIndex >= ttcConnectrecFileSize) {
    errorReport(signal, 1);
    return;
  }//if
  if (ERROR_INSERTED(5013)) {
    CLEAR_ERROR_INSERT_VALUE;
    sendSignalWithDelay(cownref, GSN_COMPLETE, signal, 2000, 3);
    return;
  }//if
  if (ERROR_INSERTED(5014)) {
    SET_ERROR_INSERT_VALUE(5018);
    sendSignalWithDelay(cownref, GSN_COMPLETE, signal, 2000, 3);
    return;
  }//if
  tcConnectptr.i = tcIndex;
  ptrAss(tcConnectptr, regTcConnectionrec);
  if ((tcConnectptr.p->transactionState == TcConnectionrec::COMMITTED) &&
      (tcConnectptr.p->transid[0] == transid1) &&
      (tcConnectptr.p->transid[1] == transid2)) {
    if (tcConnectptr.p->seqNoReplica != 0) {
      jam();
      localCommitLab(signal);
      return;
    } else {
      jam();
      completeTransLastLab(signal);
      return;
    }//if
  }//if
  if (tcConnectptr.p->transactionState != TcConnectionrec::COMMITTED) {
    warningReport(signal, 2);
  } else {
    warningReport(signal, 3);
  }//if
}//Dblqh::execCOMPLETE()

/* ************************************************************************>>
 * COMPLETEREQ: Complete request from TC. Same as COMPLETE but used if one 
 * node is not working ok (See COMMIT).
 * ************************************************************************>> */
void Dblqh::execCOMPLETEREQ(Signal* signal) 
{
  jamEntry();
  Uint32 reqPtr = signal->theData[0];
  BlockReference reqBlockref = signal->theData[1];
  Uint32 transid1 = signal->theData[2];
  Uint32 transid2 = signal->theData[3];
  Uint32 tcOprec = signal->theData[5];
  if (ERROR_INSERTED(5005)) {
    systemErrorLab(signal);
  }
  if (ERROR_INSERTED(5018)) {
    CLEAR_ERROR_INSERT_VALUE;
    sendSignalWithDelay(cownref, GSN_COMPLETEREQ, signal, 2000, 6);
    return;
  }//if
  if (findTransaction(transid1,
                      transid2,
                      tcOprec) != ZOK) {
    jam();
/*---------------------------------------------------------*/
/*       FOR SOME REASON THE COMPLETE PHASE STARTED AFTER  */
/*       A TIME OUT. THE TRANSACTION IS GONE. WE NEED TO   */
/*       REPORT COMPLETION ANYWAY.                         */
/*---------------------------------------------------------*/
    signal->theData[0] = reqPtr;
    signal->theData[1] = cownNodeid;
    signal->theData[2] = transid1;
    signal->theData[3] = transid2;
    sendSignal(reqBlockref, GSN_COMPLETECONF, signal, 4, JBB);
    warningReport(signal, 7);
    return;
  }//if
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  switch (regTcPtr->transactionState) {
  case TcConnectionrec::COMMITTED:
    jam();
    regTcPtr->reqBlockref = reqBlockref;
    regTcPtr->reqRef = reqPtr;
    regTcPtr->abortState = TcConnectionrec::REQ_FROM_TC;
    /*empty*/;
    break;
/*---------------------------------------------------------*/
/*       THE NORMAL CASE.                                  */
/*---------------------------------------------------------*/
  case TcConnectionrec::COMMIT_STOPPED:
    jam();
/*---------------------------------------------------------*/
/*       FOR SOME REASON THE COMPLETE PHASE STARTED AFTER  */
/*       A TIME OUT. WE HAVE SET THE PROPER VARIABLES SUCH */
/*       THAT A COMPLETECONF WILL BE SENT WHEN COMPLETE IS */
/*       FINISHED.                                         */
/*---------------------------------------------------------*/
    regTcPtr->reqBlockref = reqBlockref;
    regTcPtr->reqRef = reqPtr;
    regTcPtr->abortState = TcConnectionrec::REQ_FROM_TC;
    return;
    break;
  default:
    jam();
    warningReport(signal, 6);
    return;
    break;
  }//switch
  if (regTcPtr->seqNoReplica != 0) {
    jam();
    localCommitLab(signal);
    return;
  } else {
    jam();
    completeTransLastLab(signal);
    return;
  }//if
}//Dblqh::execCOMPLETEREQ()

/* ************> */
/*  COMPLETED  > */
/* ************> */
void Dblqh::execLQHKEYCONF(Signal* signal) 
{
  LqhKeyConf * const lqhKeyConf = (LqhKeyConf *)signal->getDataPtr();
  Uint32 tcIndex = lqhKeyConf->opPtr;
  Uint32 ttcConnectrecFileSize = ctcConnectrecFileSize;
  TcConnectionrec *regTcConnectionrec = tcConnectionrec;
  jamEntry();
  if (tcIndex >= ttcConnectrecFileSize) {
    errorReport(signal, 2);
    return;
  }//if
  tcConnectptr.i = tcIndex;  
  ptrAss(tcConnectptr, regTcConnectionrec);
  switch (tcConnectptr.p->connectState) {
  case TcConnectionrec::LOG_CONNECTED:
    jam();
    completedLab(signal);
    return;
    break;
  case TcConnectionrec::COPY_CONNECTED:
    jam();
    copyCompletedLab(signal);
    return;
    break;
  default:
    jam();
    ndbrequire(false);
    break;
  }//switch
  return;
}//Dblqh::execLQHKEYCONF()

/* ------------------------------------------------------------------------- */
/* -------                       COMMIT PHASE                        ------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void Dblqh::commitReqLab(Signal* signal, Uint32 gci) 
{
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  TcConnectionrec::LogWriteState logWriteState = regTcPtr->logWriteState;
  TcConnectionrec::TransactionState transState = regTcPtr->transactionState;
  regTcPtr->gci = gci; 
  if (transState == TcConnectionrec::PREPARED) {
    if (logWriteState == TcConnectionrec::WRITTEN) {
      jam();
      regTcPtr->transactionState = TcConnectionrec::PREPARED_RECEIVED_COMMIT;
      TcConnectionrecPtr saveTcPtr = tcConnectptr;
      Uint32 blockNo = refToBlock(regTcPtr->tcTupBlockref);
      signal->theData[0] = regTcPtr->tupConnectrec;
      signal->theData[1] = gci;
      EXECUTE_DIRECT(blockNo, GSN_TUP_WRITELOG_REQ, signal, 2);
      jamEntry();
      if (regTcPtr->transactionState == TcConnectionrec::LOG_COMMIT_QUEUED) {
        jam();
        return;
      }//if
      ndbrequire(regTcPtr->transactionState == TcConnectionrec::LOG_COMMIT_WRITTEN);
      tcConnectptr = saveTcPtr;
    } else if (logWriteState == TcConnectionrec::NOT_STARTED) {
      jam();
    } else if (logWriteState == TcConnectionrec::NOT_WRITTEN) {
      jam();
/*---------------------------------------------------------------------------*/
/* IT IS A READ OPERATION OR OTHER OPERATION THAT DO NOT USE THE LOG.        */
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/* THE LOG HAS NOT BEEN WRITTEN SINCE THE LOG FLAG WAS FALSE. THIS CAN OCCUR */
/* WHEN WE ARE STARTING A NEW FRAGMENT.                                      */
/*---------------------------------------------------------------------------*/
      regTcPtr->logWriteState = TcConnectionrec::NOT_STARTED;
    } else {
      ndbrequire(logWriteState == TcConnectionrec::NOT_WRITTEN_WAIT);
      jam();
/*---------------------------------------------------------------------------*/
/* THE STATE WAS SET TO NOT_WRITTEN BY THE OPERATION BUT LATER A SCAN OF ALL */
/* OPERATION RECORD CHANGED IT INTO NOT_WRITTEN_WAIT. THIS INDICATES THAT WE */
/* ARE WAITING FOR THIS OPERATION TO COMMIT OR ABORT SO THAT WE CAN FIND THE */
/* STARTING GLOBAL CHECKPOINT OF THIS NEW FRAGMENT.                          */
/*---------------------------------------------------------------------------*/
      checkScanTcCompleted(signal);
    }//if
  } else if (transState == TcConnectionrec::LOG_COMMIT_QUEUED_WAIT_SIGNAL) {
    jam();
    regTcPtr->transactionState = TcConnectionrec::LOG_COMMIT_QUEUED;
    return;
  } else if (transState == TcConnectionrec::LOG_COMMIT_WRITTEN_WAIT_SIGNAL) {
    jam();
  } else {
    warningReport(signal, 0);
    return;
  }//if
  if (regTcPtr->seqNoReplica != 0) {
    jam();
    commitReplyLab(signal);
    return;
  }//if
  localCommitLab(signal);
  return;
}//Dblqh::commitReqLab()

void Dblqh::execLQH_WRITELOG_REQ(Signal* signal)
{
  jamEntry();
  tcConnectptr.i = signal->theData[0];
  ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  Uint32 gci = signal->theData[1];
  Uint32 newestGci = cnewestGci;
  TcConnectionrec::LogWriteState logWriteState = regTcPtr->logWriteState;
  TcConnectionrec::TransactionState transState = regTcPtr->transactionState;
  regTcPtr->gci = gci;
  if (gci > newestGci) {
    jam();
/* ------------------------------------------------------------------------- */
/*       KEEP TRACK OF NEWEST GLOBAL CHECKPOINT THAT LQH HAS HEARD OF.       */
/* ------------------------------------------------------------------------- */
    cnewestGci = gci;
  }//if
  if (logWriteState == TcConnectionrec::WRITTEN) {
/*---------------------------------------------------------------------------*/
/* I NEED TO INSERT A COMMIT LOG RECORD SINCE WE ARE WRITING LOG IN THIS     */
/* TRANSACTION.                                                              */
/*---------------------------------------------------------------------------*/
    jam();
    LogPartRecordPtr regLogPartPtr;
    Uint32 noOfLogPages = cnoOfLogPages;
    jam();
    regLogPartPtr.i = regTcPtr->hashValue & 3;
    ptrCheckGuard(regLogPartPtr, clogPartFileSize, logPartRecord);
    if ((regLogPartPtr.p->logPartState == LogPartRecord::ACTIVE) ||
        (noOfLogPages == 0)) {
      jam();
/*---------------------------------------------------------------------------*/
/* THIS LOG PART WAS CURRENTLY ACTIVE WRITING ANOTHER LOG RECORD. WE MUST    */
/* WAIT UNTIL THIS PART HAS COMPLETED ITS OPERATION.                         */
/*---------------------------------------------------------------------------*/
// We must delay the write of commit info to the log to safe-guard against
// a crash due to lack of log pages. We temporary stop all log writes to this
// log part to ensure that we don't get a buffer explosion in the delayed
// signal buffer instead.
/*---------------------------------------------------------------------------*/
      linkWaitLog(signal, regLogPartPtr);
      if (transState == TcConnectionrec::PREPARED) {
        jam();
        regTcPtr->transactionState = TcConnectionrec::LOG_COMMIT_QUEUED_WAIT_SIGNAL;
      } else {
        jam();
        ndbrequire(transState == TcConnectionrec::PREPARED_RECEIVED_COMMIT);
        regTcPtr->transactionState = TcConnectionrec::LOG_COMMIT_QUEUED;
      }//if
      if (regLogPartPtr.p->logPartState == LogPartRecord::IDLE) {
        jam();
        regLogPartPtr.p->logPartState = LogPartRecord::ACTIVE;
      }//if
      return;
    }//if
    writeCommitLog(signal, regLogPartPtr);
    if (transState == TcConnectionrec::PREPARED) {
      jam();
      regTcPtr->transactionState = TcConnectionrec::LOG_COMMIT_WRITTEN_WAIT_SIGNAL;
    } else {
      jam();
      ndbrequire(transState == TcConnectionrec::PREPARED_RECEIVED_COMMIT);
      regTcPtr->transactionState = TcConnectionrec::LOG_COMMIT_WRITTEN;
    }//if
  }//if
}//Dblqh::execLQH_WRITELOG_REQ()

void Dblqh::localCommitLab(Signal* signal) 
{
  FragrecordPtr regFragptr;
  regFragptr.i = tcConnectptr.p->fragmentptr;
  ptrCheckGuard(regFragptr, cfragrecFileSize, fragrecord);
  Fragrecord::FragStatus status = regFragptr.p->fragStatus;
  fragptr = regFragptr;
  switch (status) {
  case Fragrecord::FSACTIVE:
  case Fragrecord::CRASH_RECOVERING:
  case Fragrecord::ACTIVE_CREATION:
    jam();
    commitContinueAfterBlockedLab(signal);
    return;
    break;
  case Fragrecord::BLOCKED:
    jam();
    linkFragQueue(signal);
    tcConnectptr.p->transactionState = TcConnectionrec::COMMIT_STOPPED;
    break;
  case Fragrecord::FREE:
    jam();
  case Fragrecord::DEFINED:
    jam();
  case Fragrecord::REMOVING:
    jam();
  default:
    ndbrequire(false);
    break;
  }//switch
}//Dblqh::localCommitLab()

void Dblqh::commitContinueAfterBlockedLab(Signal* signal) 
{
/* ------------------------------------------------------------------------- */
/*INPUT:          TC_CONNECTPTR           ACTIVE OPERATION RECORD            */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/*CONTINUE HERE AFTER BEING BLOCKED FOR A WHILE DURING LOCAL CHECKPOINT.     */
/*The operation is already removed from the active list since there is no    */
/*chance for any real-time breaks before we need to release it.              */
/* ------------------------------------------------------------------------- */
/*ALSO AFTER NORMAL PROCEDURE WE CONTINUE                                    */
/*WE MUST COMMIT TUP BEFORE ACC TO ENSURE THAT NO ONE RACES IN AND SEES A    */
/*DIRTY STATE IN TUP.                                                        */
/* ------------------------------------------------------------------------- */
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  Fragrecord * const regFragptr = fragptr.p;
  Uint32 operation = regTcPtr->operation;
  Uint32 simpleRead = regTcPtr->simpleRead;
  Uint32 dirtyOp = regTcPtr->dirtyOp;
  if (regTcPtr->activeCreat == ZFALSE) {
    if ((cCommitBlocked == true) &&
        (regFragptr->fragActiveStatus == ZTRUE)) {
      jam();
/* ------------------------------------------------------------------------- */
// TUP and/or ACC have problems in writing the undo log to disk fast enough.
// We must avoid the commit at this time and try later instead. The fragment
// is also active with a local checkpoint and this commit can generate UNDO
// log records that overflow the UNDO log buffer.
/* ------------------------------------------------------------------------- */
/*---------------------------------------------------------------------------*/
// We must delay the write of commit info to the log to safe-guard against
// a crash due to lack of log pages. We temporary stop all log writes to this
// log part to ensure that we don't get a buffer explosion in the delayed
// signal buffer instead.
/*---------------------------------------------------------------------------*/
      logPartPtr.i = regTcPtr->hashValue & 3;
      ptrCheckGuard(logPartPtr, clogPartFileSize, logPartRecord);
      linkWaitLog(signal, logPartPtr);
      regTcPtr->transactionState = TcConnectionrec::COMMIT_QUEUED;
      if (logPartPtr.p->logPartState == LogPartRecord::IDLE) {
        jam();
        logPartPtr.p->logPartState = LogPartRecord::ACTIVE;
      }//if
      return;
    }//if
    if (operation != ZREAD) {
      TupCommitReq * const tupCommitReq = 
        (TupCommitReq *)signal->getDataPtrSend();
      Uint32 sig0 = regTcPtr->tupConnectrec;
      Uint32 tup = refToBlock(regTcPtr->tcTupBlockref);
      jam();
      tupCommitReq->opPtr = sig0;
      tupCommitReq->gci = regTcPtr->gci;
      tupCommitReq->hashValue = regTcPtr->hashValue;
      EXECUTE_DIRECT(tup, GSN_TUP_COMMITREQ, signal, 
		     TupCommitReq::SignalLength);
      Uint32 acc = refToBlock(regTcPtr->tcAccBlockref);
      signal->theData[0] = regTcPtr->accConnectrec;
      EXECUTE_DIRECT(acc, GSN_ACC_COMMITREQ, signal, 1);
    } else {
      if(!dirtyOp){
	Uint32 acc = refToBlock(regTcPtr->tcAccBlockref);
	signal->theData[0] = regTcPtr->accConnectrec;
	EXECUTE_DIRECT(acc, GSN_ACC_COMMITREQ, signal, 1);
      }
    }
    jamEntry();
    if (simpleRead) {
      jam();
/* ------------------------------------------------------------------------- */
/*THE OPERATION WAS A SIMPLE READ THUS THE COMMIT PHASE IS ONLY NEEDED TO    */
/*RELEASE THE LOCKS. AT THIS POINT IN THE CODE THE LOCKS ARE RELEASED AND WE */
/*ARE IN A POSITION TO SEND LQHKEYCONF TO TC. WE WILL ALSO RELEASE ALL       */
/*RESOURCES BELONGING TO THIS OPERATION SINCE NO MORE WORK WILL BE           */
/*PERFORMED.                                                                 */
/* ------------------------------------------------------------------------- */
      cleanUp(signal);
      return;
    }//if
  }//if
  Uint32 seqNoReplica = regTcPtr->seqNoReplica;
  if (regTcPtr->gci > regFragptr->newestGci) {
    jam();
/* ------------------------------------------------------------------------- */
/*IT IS THE FIRST TIME THIS GLOBAL CHECKPOINT IS INVOLVED IN UPDATING THIS   */
/*FRAGMENT. UPDATE THE VARIABLE THAT KEEPS TRACK OF NEWEST GCI IN FRAGMENT   */
/* ------------------------------------------------------------------------- */
    regFragptr->newestGci = regTcPtr->gci;
  }//if
  if (dirtyOp != ZTRUE) {
    if (seqNoReplica != 0) {
      jam();
      completeTransNotLastLab(signal);
      return;
    }//if
    commitReplyLab(signal);
    return;
  } else {
/* ------------------------------------------------------------------------- */
/*WE MUST HANDLE DIRTY WRITES IN A SPECIAL WAY. THESE OPERATIONS WILL NOT    */
/*SEND ANY COMMIT OR COMPLETE MESSAGES TO OTHER NODES. THEY WILL MERELY SEND */
/*THOSE SIGNALS INTERNALLY.                                                  */
/* ------------------------------------------------------------------------- */
    if (regTcPtr->abortState == TcConnectionrec::ABORT_IDLE) {
      jam();
      packLqhkeyreqLab(signal);
    } else {
      ndbrequire(regTcPtr->abortState != TcConnectionrec::NEW_FROM_TC);
      jam();
      sendLqhTransconf(signal, LqhTransConf::Committed);
      cleanUp(signal);
    }//if
  }//if
}//Dblqh::commitContinueAfterBlockedLab()

void Dblqh::commitReplyLab(Signal* signal) 
{
/* -------------------------------------------------------------- */
/* BACKUP AND STAND-BY REPLICAS ONLY UPDATE THE TRANSACTION STATE */
/* -------------------------------------------------------------- */
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  TcConnectionrec::AbortState abortState = regTcPtr->abortState;
  regTcPtr->transactionState = TcConnectionrec::COMMITTED;
  if (abortState == TcConnectionrec::ABORT_IDLE) {
    Uint32 clientBlockref = regTcPtr->clientBlockref;
    if (regTcPtr->seqNoReplica == 0) {
      jam();
      sendCommittedTc(signal, clientBlockref);
      return;
    } else {
      jam();
      sendCommitLqh(signal, clientBlockref);
      return;
    }//if
  } else if (regTcPtr->abortState == TcConnectionrec::REQ_FROM_TC) {
    jam();
    signal->theData[0] = regTcPtr->reqRef;
    signal->theData[1] = cownNodeid;
    signal->theData[2] = regTcPtr->transid[0];
    signal->theData[3] = regTcPtr->transid[1];
    sendSignal(tcConnectptr.p->reqBlockref, GSN_COMMITCONF, signal, 4, JBB);
  } else {
    ndbrequire(regTcPtr->abortState == TcConnectionrec::NEW_FROM_TC);
    jam();
    sendLqhTransconf(signal, LqhTransConf::Committed);
  }//if
  return;
}//Dblqh::commitReplyLab()

/* ------------------------------------------------------------------------- */
/* -------                COMPLETE PHASE                             ------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void Dblqh::completeTransNotLastLab(Signal* signal) 
{
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  if (regTcPtr->abortState == TcConnectionrec::ABORT_IDLE) {
    Uint32 clientBlockref = regTcPtr->clientBlockref;
    jam();
    sendCompleteLqh(signal, clientBlockref);
    cleanUp(signal);
    return;
  } else {
    jam();
    completeUnusualLab(signal);
    return;
  }//if
}//Dblqh::completeTransNotLastLab()

void Dblqh::completeTransLastLab(Signal* signal) 
{
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  if (regTcPtr->abortState == TcConnectionrec::ABORT_IDLE) {
    Uint32 clientBlockref = regTcPtr->clientBlockref;
    jam();
/* ------------------------------------------------------------------------- */
/*DIRTY WRITES WHICH ARE LAST IN THE CHAIN OF REPLICAS WILL SEND COMPLETED   */
/*INSTEAD OF SENDING PREPARED TO THE TC (OR OTHER INITIATOR OF OPERATION).   */
/* ------------------------------------------------------------------------- */
    sendCompletedTc(signal, clientBlockref);
    cleanUp(signal);
    return;
  } else {
    jam();
    completeUnusualLab(signal);
    return;
  }//if
}//Dblqh::completeTransLastLab()

void Dblqh::completeUnusualLab(Signal* signal) 
{
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  if (regTcPtr->abortState == TcConnectionrec::ABORT_FROM_TC) {
    jam();
    sendAborted(signal);
  } else if (regTcPtr->abortState == TcConnectionrec::NEW_FROM_TC) {
    jam();
    sendLqhTransconf(signal, LqhTransConf::Committed);
  } else {
    ndbrequire(regTcPtr->abortState == TcConnectionrec::REQ_FROM_TC);
    jam();
    signal->theData[0] = regTcPtr->reqRef;
    signal->theData[1] = cownNodeid;
    signal->theData[2] = regTcPtr->transid[0];
    signal->theData[3] = regTcPtr->transid[1];
    sendSignal(regTcPtr->reqBlockref,
               GSN_COMPLETECONF, signal, 4, JBB);
  }//if
  cleanUp(signal);
  return;
}//Dblqh::completeUnusualLab()

/* ========================================================================= */
/* =======                        RELEASE TC CONNECT RECORD          ======= */
/*                                                                           */
/*       RELEASE A TC CONNECT RECORD TO THE FREELIST.                        */
/* ========================================================================= */
void Dblqh::releaseTcrec(Signal* signal, TcConnectionrecPtr locTcConnectptr) 
{
  jam();
  locTcConnectptr.p->tcTimer = 0;
  locTcConnectptr.p->transactionState = TcConnectionrec::TC_NOT_CONNECTED;
  locTcConnectptr.p->nextTcConnectrec = cfirstfreeTcConrec;
  cfirstfreeTcConrec = locTcConnectptr.i;

  TablerecPtr tabPtr;
  tabPtr.i = locTcConnectptr.p->tableref;
  if(tabPtr.i == RNIL)
    return;

  ptrCheckGuard(tabPtr, ctabrecFileSize, tablerec);
  
  /**
   * Normal case
   */
  ndbrequire(tabPtr.p->usageCount > 0);
  tabPtr.p->usageCount--;
}//Dblqh::releaseTcrec()

void Dblqh::releaseTcrecLog(Signal* signal, TcConnectionrecPtr locTcConnectptr) 
{
  jam();
  locTcConnectptr.p->tcTimer = 0;
  locTcConnectptr.p->transactionState = TcConnectionrec::TC_NOT_CONNECTED;
  locTcConnectptr.p->nextTcConnectrec = cfirstfreeTcConrec;
  cfirstfreeTcConrec = locTcConnectptr.i;

  TablerecPtr tabPtr;
  tabPtr.i = locTcConnectptr.p->tableref;
  if(tabPtr.i == RNIL)
    return;

}//Dblqh::releaseTcrecLog()

/* ------------------------------------------------------------------------- */
/* -------                       ABORT PHASE                         ------- */
/*                                                                           */
/*THIS PART IS USED AT ERRORS THAT CAUSE ABORT OF TRANSACTION.               */
/* ------------------------------------------------------------------------- */
/* ***************************************************>> */
/*  ABORT: Abort transaction in connection. Sender TC.   */
/*  This is the normal protocol (See COMMIT)             */
/* ***************************************************>> */
void Dblqh::execABORT(Signal* signal) 
{
  jamEntry();
  Uint32 tcOprec = signal->theData[0];
  BlockReference tcBlockref = signal->theData[1];
  Uint32 transid1 = signal->theData[2];
  Uint32 transid2 = signal->theData[3];
  CRASH_INSERTION(5003);
  if (ERROR_INSERTED(5015)) {
    CLEAR_ERROR_INSERT_VALUE;
    sendSignalWithDelay(cownref, GSN_ABORT, signal, 2000, 4);
    return;
  }//if
  if (findTransaction(transid1,
                      transid2,
                      tcOprec) != ZOK) {
    jam();

    if(ERROR_INSERTED(5039) && 
       refToNode(signal->getSendersBlockRef()) != getOwnNodeId()){
      jam();
      SET_ERROR_INSERT_VALUE(5040);
      return;
    }

    if(ERROR_INSERTED(5040) && 
       refToNode(signal->getSendersBlockRef()) != getOwnNodeId()){
      jam();
      SET_ERROR_INSERT_VALUE(5003);
      return;
    }
    
/* ------------------------------------------------------------------------- */
// SEND ABORTED EVEN IF NOT FOUND.
//THE TRANSACTION MIGHT NEVER HAVE ARRIVED HERE.
/* ------------------------------------------------------------------------- */
    signal->theData[0] = tcOprec;
    signal->theData[1] = transid1;
    signal->theData[2] = transid2;
    signal->theData[3] = cownNodeid;
    signal->theData[4] = ZTRUE;
    sendSignal(tcBlockref, GSN_ABORTED, signal, 5, JBB);
    warningReport(signal, 8);
    return;
  }//if
/* ------------------------------------------------------------------------- */
/*A GUIDING DESIGN PRINCIPLE IN HANDLING THESE ERROR SITUATIONS HAVE BEEN    */
/*KEEP IT SIMPLE. THUS WE RATHER INSERT A WAIT AND SET THE ABORT_STATE TO    */
/*ACTIVE RATHER THAN WRITE NEW CODE TO HANDLE EVERY SPECIAL SITUATION.       */
/* ------------------------------------------------------------------------- */
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  if (regTcPtr->nextReplica != ZNIL) {
/* ------------------------------------------------------------------------- */
// We will immediately send the ABORT message also to the next LQH node in line.
/* ------------------------------------------------------------------------- */
    BlockReference TLqhRef = calcLqhBlockRef(regTcPtr->nextReplica);
    signal->theData[0] = regTcPtr->tcOprec;
    signal->theData[1] = regTcPtr->tcBlockref;
    signal->theData[2] = regTcPtr->transid[0];
    signal->theData[3] = regTcPtr->transid[1];
    sendSignal(TLqhRef, GSN_ABORT, signal, 4, JBB);
  }//if
  regTcPtr->abortState = TcConnectionrec::ABORT_FROM_TC;
  regTcPtr->activeCreat = ZFALSE;

  const Uint32 commitAckMarker = regTcPtr->commitAckMarker;
  if(commitAckMarker != RNIL){
    jam();
#ifdef MARKER_TRACE
    {
      CommitAckMarkerPtr tmp;
      m_commitAckMarkerHash.getPtr(tmp, commitAckMarker);
      ndbout_c("Ab2 marker[%.8x %.8x]", tmp.p->transid1, tmp.p->transid2);
    }
#endif
    m_commitAckMarkerHash.release(commitAckMarker);
    regTcPtr->commitAckMarker = RNIL;
  }

  abortStateHandlerLab(signal);

  return;
}//Dblqh::execABORT()

/* ************************************************************************>> 
 *  ABORTREQ: Same as ABORT but used in case one node isn't working ok. 
 *  (See COMMITREQ) 
 * ************************************************************************>> */
void Dblqh::execABORTREQ(Signal* signal) 
{
  jamEntry();
  Uint32 reqPtr = signal->theData[0];
  BlockReference reqBlockref = signal->theData[1];
  Uint32 transid1 = signal->theData[2];
  Uint32 transid2 = signal->theData[3];
  Uint32 tcOprec = signal->theData[5];
  if (ERROR_INSERTED(5006)) {
    systemErrorLab(signal);
  }
  if (ERROR_INSERTED(5016)) {
    CLEAR_ERROR_INSERT_VALUE;
    sendSignalWithDelay(cownref, GSN_ABORTREQ, signal, 2000, 6);
    return;
  }//if
  if (findTransaction(transid1,
                      transid2,
                      tcOprec) != ZOK) {
    signal->theData[0] = reqPtr;
    signal->theData[2] = cownNodeid;
    signal->theData[3] = transid1;
    signal->theData[4] = transid2;
    sendSignal(reqBlockref, GSN_ABORTCONF, signal, 5, JBB);
    warningReport(signal, 9);
    return;
  }//if
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  if (regTcPtr->transactionState != TcConnectionrec::PREPARED) {
    warningReport(signal, 10);
    return;
  }//if
  regTcPtr->reqBlockref = reqBlockref;
  regTcPtr->reqRef = reqPtr;
  regTcPtr->abortState = TcConnectionrec::REQ_FROM_TC;
  regTcPtr->activeCreat = ZFALSE;
  abortCommonLab(signal);
  return;
}//Dblqh::execABORTREQ()

/* ************>> */
/*  ACC_TO_REF  > */
/* ************>> */
void Dblqh::execACC_TO_REF(Signal* signal) 
{
  jamEntry();
  terrorCode = signal->theData[1];
  releaseActiveFrag(signal);
  abortErrorLab(signal);
  return;
}//Dblqh::execACC_TO_REF()

/* ************> */
/*  ACCKEYREF  > */
/* ************> */
void Dblqh::execACCKEYREF(Signal* signal) 
{
  jamEntry();
  tcConnectptr.i = signal->theData[0];
  terrorCode = signal->theData[1];
  ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
  TcConnectionrec * const tcPtr = tcConnectptr.p;
  switch (tcPtr->transactionState) {
  case TcConnectionrec::WAIT_ACC:
    jam();
    releaseActiveFrag(signal);
    break;
  case TcConnectionrec::WAIT_ACC_ABORT:
  case TcConnectionrec::ABORT_STOPPED:
  case TcConnectionrec::ABORT_QUEUED:
    jam();
/* ------------------------------------------------------------------------- */
/*IGNORE SINCE ABORT OF THIS OPERATION IS ONGOING ALREADY.                   */
/* ------------------------------------------------------------------------- */
    return;
    break;
  default:
    ndbrequire(false);
    break;
  }//switch
  const Uint32 errCode = terrorCode; 
  tcPtr->errorCode = errCode;
/* ------------------------------------------------------------------------- */
/*WHEN AN ABORT FROM TC ARRIVES IT COULD ACTUALLY BE A CORRECT BEHAVIOUR     */
/*SINCE THE TUPLE MIGHT NOT HAVE ARRIVED YET OR ALREADY HAVE BEEN INSERTED.  */
/* ------------------------------------------------------------------------- */
  if (tcPtr->activeCreat == ZTRUE) {
    jam();
/* ------------------------------------------------------------------------- */
/*THIS IS A NORMAL EVENT DURING CREATION OF A FRAGMENT. PERFORM ABORT IN     */
/*TUP AND ACC AND THEN CONTINUE WITH NORMAL COMMIT PROCESSING. IF THE ERROR  */
/*HAPPENS TO BE A SERIOUS ERROR THEN PERFORM ABORT PROCESSING AS NORMAL.     */
/* ------------------------------------------------------------------------- */
    switch (tcPtr->operation) {
    case ZUPDATE:
    case ZDELETE:
      jam();
      if (errCode != ZNO_TUPLE_FOUND) {
        jam();
/* ------------------------------------------------------------------------- */
/*A NORMAL ERROR WILL BE TREATED AS A NORMAL ABORT AND WILL ABORT THE        */
/*TRANSACTION. NO SPECIAL HANDLING IS NEEDED.                                */
/* ------------------------------------------------------------------------- */
        tcPtr->activeCreat = ZFALSE;
      }//if
      break;
    case ZINSERT:
      jam();
      if (errCode != ZTUPLE_ALREADY_EXIST) {
        jam();
/* ------------------------------------------------------------------------- */
/*A NORMAL ERROR WILL BE TREATED AS A NORMAL ABORT AND WILL ABORT THE        */
/*TRANSACTION. NO SPECIAL HANDLING IS NEEDED.                                */
/* ------------------------------------------------------------------------- */
        tcPtr->activeCreat = ZFALSE;
      }//if
      break;
    default:
      jam();
/* ------------------------------------------------------------------------- */
/*A NORMAL ERROR WILL BE TREATED AS A NORMAL ABORT AND WILL ABORT THE        */
/*TRANSACTION. NO SPECIAL HANDLING IS NEEDED.                                */
/* ------------------------------------------------------------------------- */
      tcPtr->activeCreat = ZFALSE;
      break;
    }//switch
  } else {
    /**
     * Only primary replica can get ZTUPLE_ALREADY_EXIST || ZNO_TUPLE_FOUND
     *
     * Unless it's a simple or dirty read
     *
     * NOT TRUE!
     * 1) op1 - primary insert ok
     * 2) op1 - backup insert fail (log full or what ever)
     * 3) op1 - delete ok @ primary
     * 4) op1 - delete fail @ backup
     *
     * -> ZNO_TUPLE_FOUND is possible
     */
    ndbrequire
      (tcPtr->seqNoReplica == 0 ||
       errCode != ZTUPLE_ALREADY_EXIST ||
       (tcPtr->operation == ZREAD && (tcPtr->dirtyOp || tcPtr->opSimple)));
  }
  tcPtr->abortState = TcConnectionrec::ABORT_FROM_LQH;
  abortCommonLab(signal);
  return;
}//Dblqh::execACCKEYREF()

void Dblqh::localAbortStateHandlerLab(Signal* signal) 
{
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  if (regTcPtr->abortState != TcConnectionrec::ABORT_IDLE) {
    jam();
    return;
  }//if
  regTcPtr->activeCreat = ZFALSE;
  regTcPtr->abortState = TcConnectionrec::ABORT_FROM_LQH;
  regTcPtr->errorCode = terrorCode;
  abortStateHandlerLab(signal);
  return;
}//Dblqh::localAbortStateHandlerLab()

void Dblqh::abortStateHandlerLab(Signal* signal) 
{
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  switch (regTcPtr->transactionState) {
  case TcConnectionrec::PREPARED:
    jam();
/* ------------------------------------------------------------------------- */
/*THE OPERATION IS ALREADY PREPARED AND SENT TO THE NEXT LQH OR BACK TO TC.  */
/*WE CAN SIMPLY CONTINUE WITH THE ABORT PROCESS.                             */
/*IF IT WAS A CHECK FOR TRANSACTION STATUS THEN WE REPORT THE STATUS TO THE  */
/*NEW TC AND CONTINUE WITH THE NEXT OPERATION IN LQH.                        */
/* ------------------------------------------------------------------------- */
    if (regTcPtr->abortState == TcConnectionrec::NEW_FROM_TC) {
      jam();
      sendLqhTransconf(signal, LqhTransConf::Prepared);
      return;
    }//if
    break;
  case TcConnectionrec::LOG_COMMIT_WRITTEN_WAIT_SIGNAL:
  case TcConnectionrec::LOG_COMMIT_QUEUED_WAIT_SIGNAL:
    jam();
/* ------------------------------------------------------------------------- */
// We can only reach these states for multi-updates on a record in a transaction.
// We know that at least one of those has received the COMMIT signal, thus we
// declare us only prepared since we then receive the expected COMMIT signal.
/* ------------------------------------------------------------------------- */
    ndbrequire(regTcPtr->abortState == TcConnectionrec::NEW_FROM_TC);
    sendLqhTransconf(signal, LqhTransConf::Prepared);
    break;
  case TcConnectionrec::WAIT_TUPKEYINFO:
  case TcConnectionrec::WAIT_ATTR:
    jam();
/* ------------------------------------------------------------------------- */
/* WE ARE CURRENTLY WAITING FOR MORE INFORMATION. WE CAN START THE ABORT     */
/* PROCESS IMMEDIATELY. THE KEYINFO AND ATTRINFO SIGNALS WILL BE DROPPED     */
/* SINCE THE ABORT STATE WILL BE SET.                                        */
/* ------------------------------------------------------------------------- */
    break;
  case TcConnectionrec::WAIT_TUP:
    jam();
/* ------------------------------------------------------------------------- */
// TUP is currently active. We have to wait for the TUPKEYREF or TUPKEYCONF
// to arrive since we might otherwise jeopardise the local checkpoint
// consistency in overload situations.
/* ------------------------------------------------------------------------- */
    regTcPtr->transactionState = TcConnectionrec::WAIT_TUP_TO_ABORT;
    return;
  case TcConnectionrec::WAIT_ACC:
    jam();
    if (regTcPtr->listState == TcConnectionrec::ACC_BLOCK_LIST) {
      jam();
/* ------------------------------------------------------------------------- */
// If the operation is in the ACC Blocked list the operation is not allowed
// to start yet. We release it from the ACC Blocked list and will go through
// the gate in abortCommonLab(..) where it will be blocked.
/* ------------------------------------------------------------------------- */
      fragptr.i = regTcPtr->fragmentptr;
      ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
      releaseAccList(signal);
    } else {
      jam();
/* ------------------------------------------------------------------------- */
// We start the abort immediately since the operation is still in the active
// list and the fragment cannot have been frozen yet. By sending LCP_HOLDOPCONF
// as direct signals we avoid the problem that we might find the operation
// in an unexpected list in ACC.
// We cannot accept being blocked before aborting ACC here since that would
// lead to seriously complex issues.
/* ------------------------------------------------------------------------- */
      abortContinueAfterBlockedLab(signal, false);
      return;
    }//if
    break;
  case TcConnectionrec::LOG_QUEUED:
    jam();
/* ------------------------------------------------------------------------- */
/*CURRENTLY QUEUED FOR LOGGING. WAIT UNTIL THE LOG RECORD HAVE BEEN INSERTED */
/*AND THEN CONTINUE THE ABORT PROCESS.                                       */
//Could also be waiting for an overloaded log disk. In this case it is easy
//to abort when CONTINUEB arrives.
/* ------------------------------------------------------------------------- */
    return;
    break;
  case TcConnectionrec::STOPPED:
    jam();
    /* ---------------------------------------------------------------------
     * WE ARE CURRENTLY QUEUED FOR ACCESS TO THE FRAGMENT BY A LCP
     * Since nothing has been done, just release operation
     * i.e. no prepare log record has been written 
     *      so no abort log records needs to be written
     */
    releaseWaitQueue(signal);
    continueAfterLogAbortWriteLab(signal);
    return;
    break;
  case TcConnectionrec::WAIT_AI_AFTER_ABORT:
    jam();
/* ------------------------------------------------------------------------- */
/* ABORT OF ACC AND TUP ALREADY COMPLETED. THIS STATE IS ONLY USED WHEN      */
/* CREATING A NEW FRAGMENT.                                                  */
/* ------------------------------------------------------------------------- */
    continueAbortLab(signal);
    return;
    break;
  case TcConnectionrec::WAIT_TUP_TO_ABORT:
  case TcConnectionrec::ABORT_STOPPED:
  case TcConnectionrec::LOG_ABORT_QUEUED:
  case TcConnectionrec::WAIT_ACC_ABORT:
  case TcConnectionrec::ABORT_QUEUED:
    jam();
/* ------------------------------------------------------------------------- */
/*ABORT IS ALREADY ONGOING DUE TO SOME ERROR. WE HAVE ALREADY SET THE STATE  */
/*OF THE ABORT SO THAT WE KNOW THAT TC EXPECTS A REPORT. WE CAN THUS SIMPLY  */
/*EXIT.                                                                      */
/* ------------------------------------------------------------------------- */
    return;
    break;
  case TcConnectionrec::COMMIT_STOPPED:
  case TcConnectionrec::LOG_COMMIT_QUEUED:
  case TcConnectionrec::COMMIT_QUEUED:
    jam();
/* ------------------------------------------------------------------------- */
/*THIS IS ONLY AN ALLOWED STATE IF A DIRTY WRITE OR SIMPLE READ IS PERFORMED.*/
/*IF WE ARE MERELY CHECKING THE TRANSACTION STATE IT IS ALSO AN ALLOWED STATE*/
/* ------------------------------------------------------------------------- */
    if (regTcPtr->dirtyOp == ZTRUE) {
      jam();
/* ------------------------------------------------------------------------- */
/*COMPLETE THE DIRTY WRITE AND THEN REPORT COMPLETED BACK TO TC. SINCE IT IS */
/*A DIRTY WRITE IT IS ALLOWED TO COMMIT EVEN IF THE TRANSACTION ABORTS.      */
/* ------------------------------------------------------------------------- */
      return;
    }//if
    if (regTcPtr->simpleRead) {
      jam();
/* ------------------------------------------------------------------------- */
/*A SIMPLE READ IS CURRENTLY RELEASING THE LOCKS OR WAITING FOR ACCESS TO    */
/*ACC TO CLEAR THE LOCKS. COMPLETE THIS PROCESS AND THEN RETURN AS NORMAL.   */
/*NO DATA HAS CHANGED DUE TO THIS SIMPLE READ ANYWAY.                        */
/* ------------------------------------------------------------------------- */
      return;
    }//if
    ndbrequire(regTcPtr->abortState == TcConnectionrec::NEW_FROM_TC);
    jam();
/* ------------------------------------------------------------------------- */
/*WE ARE ONLY CHECKING THE STATUS OF THE TRANSACTION. IT IS COMMITTING.      */
/*COMPLETE THE COMMIT LOCALLY AND THEN SEND REPORT OF COMMITTED TO THE NEW TC*/
/* ------------------------------------------------------------------------- */
    return;
    break;
  case TcConnectionrec::COMMITTED:
    jam();
    ndbrequire(regTcPtr->abortState == TcConnectionrec::NEW_FROM_TC);
/* ------------------------------------------------------------------------- */
/*WE ARE CHECKING TRANSACTION STATUS. REPORT COMMITTED AND CONTINUE WITH THE */
/*NEXT OPERATION.                                                            */
/* ------------------------------------------------------------------------- */
    sendLqhTransconf(signal, LqhTransConf::Committed);
    return;
    break;
  default:
    ndbrequire(false);
/* ------------------------------------------------------------------------- */
/*THE STATE WAS NOT AN ALLOWED STATE ON A NORMAL OPERATION. SCANS AND COPY   */
/*FRAGMENT OPERATIONS SHOULD HAVE EXECUTED IN ANOTHER PATH.                  */
/* ------------------------------------------------------------------------- */
    break;
  }//switch
  abortCommonLab(signal);
  return;
}//Dblqh::abortStateHandlerLab()

void Dblqh::abortErrorLab(Signal* signal) 
{
  ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  if (regTcPtr->abortState == TcConnectionrec::ABORT_IDLE) {
    jam();
    regTcPtr->abortState = TcConnectionrec::ABORT_FROM_LQH;
    regTcPtr->errorCode = terrorCode;
  }//if
  /* -----------------------------------------------------------------------
   *       ACTIVE CREATION IS RESET FOR ALL ERRORS WHICH SHOULD BE HANDLED 
   *       WITH NORMAL ABORT HANDLING.
   * ----------------------------------------------------------------------- */
  regTcPtr->activeCreat = ZFALSE;
  abortCommonLab(signal);
  return;
}//Dblqh::abortErrorLab()

void Dblqh::abortCommonLab(Signal* signal) 
{
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  const Uint32 commitAckMarker = regTcPtr->commitAckMarker;
  if(regTcPtr->activeCreat != ZTRUE && commitAckMarker != RNIL){
    /**
     * There is no NR ongoing and we have a marker
     */
    jam();
#ifdef MARKER_TRACE
    {
      CommitAckMarkerPtr tmp;
      m_commitAckMarkerHash.getPtr(tmp, commitAckMarker);
      ndbout_c("Abo marker[%.8x %.8x]", tmp.p->transid1, tmp.p->transid2);
    }
#endif
    m_commitAckMarkerHash.release(commitAckMarker);
    regTcPtr->commitAckMarker = RNIL;
  }
  
  fragptr.i = regTcPtr->fragmentptr;
  if (fragptr.i != RNIL) {
    jam();
    ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
    switch (fragptr.p->fragStatus) {
    case Fragrecord::FSACTIVE:
    case Fragrecord::CRASH_RECOVERING:
    case Fragrecord::ACTIVE_CREATION:
      jam();
      linkActiveFrag(signal);
      abortContinueAfterBlockedLab(signal, true);
      return;
      break;
    case Fragrecord::BLOCKED:
      jam();
      linkFragQueue(signal);
      regTcPtr->transactionState = TcConnectionrec::ABORT_STOPPED;
      return;
      break;
    case Fragrecord::FREE:
      jam();
    case Fragrecord::DEFINED:
      jam();
    case Fragrecord::REMOVING:
      jam();
    default:
      ndbrequire(false);
      break;
    }//switch
  } else {
    jam();
    continueAbortLab(signal);
  }//if
}//Dblqh::abortCommonLab()

void Dblqh::abortContinueAfterBlockedLab(Signal* signal, bool canBlock) 
{
  /* ------------------------------------------------------------------------
   *       INPUT:          TC_CONNECTPTR           ACTIVE OPERATION RECORD
   * ------------------------------------------------------------------------
   * ------------------------------------------------------------------------
   *       CAN COME HERE AS RESTART AFTER BEING BLOCKED BY A LOCAL CHECKPOINT.
   * ------------------------------------------------------------------------
   *       ALSO AS PART OF A NORMAL ABORT WITHOUT BLOCKING.
   *       WE MUST ABORT TUP BEFORE ACC TO ENSURE THAT NO ONE RACES IN 
   *       AND SEES A STATE IN TUP.
   * ------------------------------------------------------------------------ */
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  fragptr.i = regTcPtr->fragmentptr;
  ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
  if ((cCommitBlocked == true) &&
      (fragptr.p->fragActiveStatus == ZTRUE) &&
      (canBlock == true) &&
      (regTcPtr->operation != ZREAD)) {
    jam();
/* ------------------------------------------------------------------------- */
// TUP and/or ACC have problems in writing the undo log to disk fast enough.
// We must avoid the abort at this time and try later instead. The fragment
// is also active with a local checkpoint and this commit can generate UNDO
// log records that overflow the UNDO log buffer.
//
// In certain situations it is simply too complex to insert a wait state here
// since ACC is active and we cannot release the operation from the active
// list without causing great complexity.
/* ------------------------------------------------------------------------- */
/*---------------------------------------------------------------------------*/
// We must delay the write of abort info to the log to safe-guard against
// a crash due to lack of log pages. We temporary stop all log writes to this
// log part to ensure that we don't get a buffer explosion in the delayed
// signal buffer instead.
/*---------------------------------------------------------------------------*/
    releaseActiveFrag(signal);
    logPartPtr.i = regTcPtr->hashValue & 3;
    ptrCheckGuard(logPartPtr, clogPartFileSize, logPartRecord);
    linkWaitLog(signal, logPartPtr);
    regTcPtr->transactionState = TcConnectionrec::ABORT_QUEUED;
    if (logPartPtr.p->logPartState == LogPartRecord::IDLE) {
      jam();
      logPartPtr.p->logPartState = LogPartRecord::ACTIVE;
    }//if
    return;
  }//if
  signal->theData[0] = regTcPtr->tupConnectrec;
  EXECUTE_DIRECT(DBTUP, GSN_TUP_ABORTREQ, signal, 1);
  regTcPtr->transactionState = TcConnectionrec::WAIT_ACC_ABORT;
  signal->theData[0] = regTcPtr->accConnectrec;
  EXECUTE_DIRECT(DBACC, GSN_ACC_ABORTREQ, signal, 1);
  /* ------------------------------------------------------------------------
   * We need to insert a real-time break by sending ACC_ABORTCONF through the
   * job buffer to ensure that we catch any ACCKEYCONF or TUPKEYCONF or
   * TUPKEYREF that are in the job buffer but not yet processed. Doing 
   * everything without that would race and create a state error when they 
   * are executed.
   * ----------------------------------------------------------------------- */
  return;
}//Dblqh::abortContinueAfterBlockedLab()

/* ******************>> */
/*  ACC_ABORTCONF     > */
/* ******************>> */
void Dblqh::execACC_ABORTCONF(Signal* signal) 
{
  jamEntry();
  tcConnectptr.i = signal->theData[0];
  ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  ndbrequire(regTcPtr->transactionState == TcConnectionrec::WAIT_ACC_ABORT);
  if (regTcPtr->activeCreat == ZTRUE) {
    /* ----------------------------------------------------------------------
     * A NORMAL EVENT DURING CREATION OF A FRAGMENT. WE NOW NEED TO CONTINUE
     * WITH NORMAL COMMIT PROCESSING.
     * ---------------------------------------------------------------------- */
    if (regTcPtr->currTupAiLen == regTcPtr->totReclenAi) {
      jam();
      regTcPtr->abortState = TcConnectionrec::ABORT_IDLE;
      rwConcludedLab(signal);
      return;
    } else {
      ndbrequire(regTcPtr->currTupAiLen < regTcPtr->totReclenAi);
      jam();
      releaseActiveFrag(signal);
      regTcPtr->transactionState = TcConnectionrec::WAIT_AI_AFTER_ABORT;
      return;
    }//if
  }//if
  releaseActiveFrag(signal);
  continueAbortLab(signal);
  return;
}//Dblqh::execACC_ABORTCONF()

void Dblqh::continueAbortLab(Signal* signal) 
{
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  /* ------------------------------------------------------------------------
   *  AN ERROR OCCURED IN THE ACTIVE CREATION AFTER THE ABORT PHASE. 
   *  WE NEED TO CONTINUE WITH A NORMAL ABORT.
   * ------------------------------------------------------------------------ 
   *       ALSO USED FOR NORMAL CLEAN UP AFTER A NORMAL ABORT.
   * ------------------------------------------------------------------------
   *       ALSO USED WHEN NO FRAGMENT WAS SET UP ON OPERATION.
   * ------------------------------------------------------------------------ */
  if (regTcPtr->logWriteState == TcConnectionrec::WRITTEN) {
    jam();
    /* ----------------------------------------------------------------------
     * I NEED TO INSERT A ABORT LOG RECORD SINCE WE ARE WRITING LOG IN THIS
     * TRANSACTION.
     * ---------------------------------------------------------------------- */
    initLogPointers(signal);
    if (logPartPtr.p->logPartState == LogPartRecord::ACTIVE) {
      jam();
      /* --------------------------------------------------------------------
       * A PREPARE OPERATION IS CURRENTLY WRITING IN THE LOG. 
       * WE MUST WAIT ON OUR TURN TO WRITE THE LOG. 
       * IT IS NECESSARY TO WRITE ONE LOG RECORD COMPLETELY 
       * AT A TIME OTHERWISE WE WILL SCRAMBLE THE LOG.
       * -------------------------------------------------------------------- */
      linkWaitLog(signal, logPartPtr);
      regTcPtr->transactionState = TcConnectionrec::LOG_ABORT_QUEUED;
      return;
    }//if
    if (cnoOfLogPages == 0) {
      jam();
/*---------------------------------------------------------------------------*/
// We must delay the write of commit info to the log to safe-guard against
// a crash due to lack of log pages. We temporary stop all log writes to this
// log part to ensure that we don't get a buffer explosion in the delayed
// signal buffer instead.
/*---------------------------------------------------------------------------*/
      linkWaitLog(signal, logPartPtr);
      regTcPtr->transactionState = TcConnectionrec::LOG_ABORT_QUEUED;
      if (logPartPtr.p->logPartState == LogPartRecord::IDLE) {
        jam();
        logPartPtr.p->logPartState = LogPartRecord::ACTIVE;
      }//if
      return;
    }//if
    writeAbortLog(signal);
    removeLogTcrec(signal);
  } else if (regTcPtr->logWriteState == TcConnectionrec::NOT_STARTED) {
    jam();
  } else if (regTcPtr->logWriteState == TcConnectionrec::NOT_WRITTEN) {
    jam();
    /* ------------------------------------------------------------------
     * IT IS A READ OPERATION OR OTHER OPERATION THAT DO NOT USE THE LOG.
     * ------------------------------------------------------------------ */
    /* ------------------------------------------------------------------
     * THE LOG HAS NOT BEEN WRITTEN SINCE THE LOG FLAG WAS FALSE. 
     * THIS CAN OCCUR WHEN WE ARE STARTING A NEW FRAGMENT.
     * ------------------------------------------------------------------ */
    regTcPtr->logWriteState = TcConnectionrec::NOT_STARTED;
  } else {
    ndbrequire(regTcPtr->logWriteState == TcConnectionrec::NOT_WRITTEN_WAIT);
    jam();
    /* ----------------------------------------------------------------
     * THE STATE WAS SET TO NOT_WRITTEN BY THE OPERATION BUT LATER 
     * A SCAN OF ALL OPERATION RECORD CHANGED IT INTO NOT_WRITTEN_WAIT. 
     * THIS INDICATES THAT WE ARE WAITING FOR THIS OPERATION TO COMMIT 
     * OR ABORT SO THAT WE CAN FIND THE 
     * STARTING GLOBAL CHECKPOINT OF THIS NEW FRAGMENT.
     * ---------------------------------------------------------------- */
     checkScanTcCompleted(signal);
  }//if
  continueAfterLogAbortWriteLab(signal);
  return;
}//Dblqh::continueAbortLab()

void Dblqh::continueAfterLogAbortWriteLab(Signal* signal) 
{
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  if (regTcPtr->simpleRead) {
    jam();
    TcKeyRef * const tcKeyRef = (TcKeyRef *) signal->getDataPtrSend();
    
    tcKeyRef->connectPtr = regTcPtr->applOprec;
    tcKeyRef->transId[0] = regTcPtr->transid[0];
    tcKeyRef->transId[1] = regTcPtr->transid[1];
    tcKeyRef->errorCode = regTcPtr->errorCode;
    sendSignal(regTcPtr->applRef, 
               GSN_TCKEYREF, signal, TcKeyRef::SignalLength, JBB);
    cleanUp(signal);
    return;
  }//if
  if (regTcPtr->abortState == TcConnectionrec::ABORT_FROM_LQH) {
    LqhKeyRef * const lqhKeyRef = (LqhKeyRef *)signal->getDataPtrSend();

    jam();
    lqhKeyRef->userRef = regTcPtr->clientConnectrec;
    lqhKeyRef->connectPtr = regTcPtr->tcOprec;
    lqhKeyRef->errorCode = regTcPtr->errorCode;
    lqhKeyRef->transId1 = regTcPtr->transid[0];
    lqhKeyRef->transId2 = regTcPtr->transid[1];
    sendSignal(regTcPtr->clientBlockref, GSN_LQHKEYREF, signal, 
               LqhKeyRef::SignalLength, JBB);
  } else if (regTcPtr->abortState == TcConnectionrec::ABORT_FROM_TC) {
    jam();
    sendAborted(signal);
  } else if (regTcPtr->abortState == TcConnectionrec::NEW_FROM_TC) {
    jam();
    sendLqhTransconf(signal, LqhTransConf::Aborted);
  } else {
    ndbrequire(regTcPtr->abortState == TcConnectionrec::REQ_FROM_TC);
    jam();
    signal->theData[0] = regTcPtr->reqRef;
    signal->theData[1] = tcConnectptr.i;
    signal->theData[2] = cownNodeid;
    signal->theData[3] = regTcPtr->transid[0];
    signal->theData[4] = regTcPtr->transid[1];
    sendSignal(regTcPtr->reqBlockref, GSN_ABORTCONF, 
               signal, 5, JBB);
  }//if
  cleanUp(signal);
}//Dblqh::continueAfterLogAbortWriteLab()

/* ########################################################################## 
 * #######                       MODULE TO HANDLE TC FAILURE          ####### 
 *
 * ########################################################################## */

/* ************************************************************************>> 
 *  NODE_FAILREP: Node failure report. Sender Ndbcntr. Set status of failed 
 *  node to down and reply with NF_COMPLETEREP to DIH which will report that 
 *  LQH has completed failure handling.
 * ************************************************************************>> */
void Dblqh::execNODE_FAILREP(Signal* signal) 
{
  UintR TfoundNodes = 0;
  UintR TnoOfNodes;
  UintR Tdata[MAX_NDB_NODES];
  Uint32 i;

  NodeFailRep * const nodeFail = (NodeFailRep *)&signal->theData[0];

  TnoOfNodes = nodeFail->noOfNodes;
  UintR index = 0;
  for (i = 1; i < MAX_NDB_NODES; i++) {
    jam();
    if(NodeBitmask::get(nodeFail->theNodes, i)){
      jam();
      Tdata[index] = i;
      index++;
    }//if
  }//for

  lcpPtr.i = 0;
  ptrAss(lcpPtr, lcpRecord);
  
  ndbrequire(index == TnoOfNodes);
  ndbrequire(cnoOfNodes - 1 < MAX_NDB_NODES);
  for (i = 0; i < TnoOfNodes; i++) {
    const Uint32 nodeId = Tdata[i];
    lcpPtr.p->m_EMPTY_LCP_REQ.clear(nodeId);
    
    for (Uint32 j = 0; j < cnoOfNodes; j++) {
      jam();
      if (cnodeData[j] == nodeId){
        jam();
        cnodeStatus[j] = ZNODE_DOWN;
	
        TfoundNodes++;
      }//if
    }//for
    NFCompleteRep * const nfCompRep = (NFCompleteRep *)&signal->theData[0];
    nfCompRep->blockNo      = DBLQH;
    nfCompRep->nodeId       = cownNodeid;
    nfCompRep->failedNodeId = Tdata[i];
    sendSignal(DBDIH_REF, GSN_NF_COMPLETEREP, signal, 
	       NFCompleteRep::SignalLength, JBB);
  }//for
  ndbrequire(TnoOfNodes == TfoundNodes);
}//Dblqh::execNODE_FAILREP()

/* ************************************************************************>>
 *  LQH_TRANSREQ: Report status of all transactions where TC was coordinated 
 *  by a crashed TC 
 * ************************************************************************>> */
/* ************************************************************************>>
 *  THIS SIGNAL IS RECEIVED AFTER A NODE CRASH. 
 *  THE NODE HAD A TC AND COORDINATED A NUMBER OF TRANSACTIONS. 
 *  NOW THE MASTER NODE IS PICKING UP THOSE TRANSACTIONS
 *  TO COMPLETE THEM. EITHER ABORT THEM OR COMMIT THEM.
 * ************************************************************************>> */
void Dblqh::execLQH_TRANSREQ(Signal* signal) 
{
  jamEntry();
  Uint32 newTcPtr = signal->theData[0];
  BlockReference newTcBlockref = signal->theData[1];
  Uint32 oldNodeId = signal->theData[2];
  tcNodeFailptr.i = oldNodeId;
  ptrCheckGuard(tcNodeFailptr, ctcNodeFailrecFileSize, tcNodeFailRecord);
  if ((tcNodeFailptr.p->tcFailStatus == TcNodeFailRecord::TC_STATE_TRUE) ||
      (tcNodeFailptr.p->tcFailStatus == TcNodeFailRecord::TC_STATE_BREAK)) {
    jam();
    tcNodeFailptr.p->lastNewTcBlockref = newTcBlockref;
  /* ------------------------------------------------------------------------
   * WE HAVE RECEIVED A SIGNAL SPECIFYING THAT WE NEED TO HANDLE THE FAILURE
   * OF A TC.  NOW WE RECEIVE ANOTHER SIGNAL WITH THE SAME ORDER. THIS CAN
   * OCCUR IF THE NEW TC FAILS. WE MUST BE CAREFUL IN THIS CASE SO THAT WE DO
   * NOT START PARALLEL ACTIVITIES TRYING TO DO THE SAME THING. WE SAVE THE
   * NEW BLOCK REFERENCE TO THE LAST NEW TC IN A VARIABLE AND ASSIGN TO IT TO
   * NEW_TC_BLOCKREF WHEN THE OLD PROCESS RETURNS TO LQH_TRANS_NEXT. IT IS
   * CERTAIN TO COME THERE SINCE THIS IS THE ONLY PATH TO TAKE CARE OF THE
   * NEXT TC CONNECT RECORD. WE SET THE STATUS TO BREAK TO INDICATE TO THE OLD
   * PROCESS WHAT IS HAPPENING.
   * ------------------------------------------------------------------------ */
    tcNodeFailptr.p->lastNewTcRef = newTcPtr;
    tcNodeFailptr.p->tcFailStatus = TcNodeFailRecord::TC_STATE_BREAK;
    return;
  }//if
  tcNodeFailptr.p->oldNodeId = oldNodeId;
  tcNodeFailptr.p->newTcBlockref = newTcBlockref;
  tcNodeFailptr.p->newTcRef = newTcPtr;
  tcNodeFailptr.p->tcRecNow = 0;
  tcNodeFailptr.p->tcFailStatus = TcNodeFailRecord::TC_STATE_TRUE;
  signal->theData[0] = ZLQH_TRANS_NEXT;
  signal->theData[1] = tcNodeFailptr.i;
  sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
  return;
}//Dblqh::execLQH_TRANSREQ()

void Dblqh::lqhTransNextLab(Signal* signal) 
{
  UintR tend;
  UintR tstart;
  UintR guard0;

  if (tcNodeFailptr.p->tcFailStatus == TcNodeFailRecord::TC_STATE_BREAK) {
    jam();
    /* ----------------------------------------------------------------------
     *  AN INTERRUPTION TO THIS NODE FAIL HANDLING WAS RECEIVED AND A NEW 
     *  TC HAVE BEEN ASSIGNED TO TAKE OVER THE FAILED TC. PROBABLY THE OLD 
     *  NEW TC HAVE FAILED.
     * ---------------------------------------------------------------------- */
    tcNodeFailptr.p->newTcBlockref = tcNodeFailptr.p->lastNewTcBlockref;
    tcNodeFailptr.p->newTcRef = tcNodeFailptr.p->lastNewTcRef;
    tcNodeFailptr.p->tcRecNow = 0;
    tcNodeFailptr.p->tcFailStatus = TcNodeFailRecord::TC_STATE_TRUE;
  }//if
  tstart = tcNodeFailptr.p->tcRecNow;
  tend = tstart + 200;
  guard0 = tend;
  for (tcConnectptr.i = tstart; tcConnectptr.i <= guard0; tcConnectptr.i++) {
    jam();
    if (tcConnectptr.i >= ctcConnectrecFileSize) {
      jam();
      /**
       * Finished with scanning operation record
       *
       * now scan markers
       */
      scanMarkers(signal, tcNodeFailptr.i, 0, RNIL);
      return;
    }//if
    ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
    if (tcConnectptr.p->transactionState != TcConnectionrec::IDLE) {
      if (tcConnectptr.p->transactionState != TcConnectionrec::TC_NOT_CONNECTED) {
        if (tcConnectptr.p->tcScanRec == RNIL) {
          if (refToNode(tcConnectptr.p->tcBlockref) == tcNodeFailptr.p->oldNodeId) {
            if (tcConnectptr.p->operation != ZREAD) {
              jam();
              tcConnectptr.p->tcNodeFailrec = tcNodeFailptr.i;
              tcConnectptr.p->abortState = TcConnectionrec::NEW_FROM_TC;
              abortStateHandlerLab(signal);
              return;
            } else {
              jam();
              if (tcConnectptr.p->opSimple != ZTRUE) {
                jam();
                tcConnectptr.p->tcNodeFailrec = tcNodeFailptr.i;
                tcConnectptr.p->abortState = TcConnectionrec::NEW_FROM_TC;
                abortStateHandlerLab(signal);
                return;
              }//if
            }//if
          }//if
        } else {
          scanptr.i = tcConnectptr.p->tcScanRec;
	  c_scanRecordPool.getPtr(scanptr);
          if (scanptr.p->scanType == ScanRecord::COPY) {
            jam();
            if (scanptr.p->scanNodeId == tcNodeFailptr.p->oldNodeId) {
              jam();
	      /* ------------------------------------------------------------
	       * THE RECEIVER OF THE COPY HAVE FAILED. 
	       * WE HAVE TO CLOSE THE COPY PROCESS. 
	       * ------------------------------------------------------------ */
              tcConnectptr.p->tcNodeFailrec = tcNodeFailptr.i;
              tcConnectptr.p->abortState = TcConnectionrec::NEW_FROM_TC;
              closeCopyRequestLab(signal);
              return;
            }//if
          } else {
            if (scanptr.p->scanType == ScanRecord::SCAN) {
              jam();
              if (refToNode(tcConnectptr.p->tcBlockref) == 
		  tcNodeFailptr.p->oldNodeId) {
                jam();
                tcConnectptr.p->tcNodeFailrec = tcNodeFailptr.i;
                tcConnectptr.p->abortState = TcConnectionrec::NEW_FROM_TC;
                closeScanRequestLab(signal);
                return;
              }//if
            } else {
              jam();
	      /* ------------------------------------------------------------
	       * THIS IS AN ERROR THAT SHOULD NOT OCCUR. WE CRASH THE SYSTEM.
	       * ------------------------------------------------------------ */
               systemErrorLab(signal);
               return;
            }//if
          }//if
        }//if
      }//if
    }//if
  }//for
  tcNodeFailptr.p->tcRecNow = tend + 1;
  signal->theData[0] = ZLQH_TRANS_NEXT;
  signal->theData[1] = tcNodeFailptr.i;
  sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
  return;
}//Dblqh::lqhTransNextLab()

void
Dblqh::scanMarkers(Signal* signal, 
		   Uint32 tcNodeFail, 
		   Uint32 startBucket, 
		   Uint32 i){

  jam();
  
  TcNodeFailRecordPtr tcNodeFailPtr;
  tcNodeFailPtr.i = tcNodeFail;
  ptrCheckGuard(tcNodeFailPtr, ctcNodeFailrecFileSize, tcNodeFailRecord);
  const Uint32 crashedTcNodeId = tcNodeFailPtr.p->oldNodeId;
  
  CommitAckMarkerIterator iter;
  if(i == RNIL){
    m_commitAckMarkerHash.next(startBucket, iter);
  } else {
    jam();
    iter.curr.i = i;
    iter.bucket = startBucket;
    m_commitAckMarkerHash.getPtr(iter.curr);
    m_commitAckMarkerHash.next(iter);
  }

  const Uint32 RT_BREAK = 256;
  for(i = 0; i<RT_BREAK || iter.bucket == startBucket; i++){
    jam();
    
    if(iter.curr.i == RNIL){
      /**
       * Done with iteration
       */
      jam();
      
      tcNodeFailPtr.p->tcFailStatus = TcNodeFailRecord::TC_STATE_FALSE;
      signal->theData[0] = tcNodeFailPtr.p->newTcRef;
      signal->theData[1] = cownNodeid;
      signal->theData[2] = LqhTransConf::LastTransConf;
      sendSignal(tcNodeFailPtr.p->newTcBlockref, GSN_LQH_TRANSCONF, 
		 signal, 3, JBB);
      return;
    }
    
    if(iter.curr.p->tcNodeId == crashedTcNodeId){
      jam();
      
      /**
       * Found marker belonging to crashed node
       */
      LqhTransConf * const lqhTransConf = (LqhTransConf *)&signal->theData[0];
      lqhTransConf->tcRef     = tcNodeFailPtr.p->newTcRef;
      lqhTransConf->lqhNodeId = cownNodeid;
      lqhTransConf->operationStatus = LqhTransConf::Marker;
      lqhTransConf->transId1 = iter.curr.p->transid1;
      lqhTransConf->transId2 = iter.curr.p->transid2;
      lqhTransConf->apiRef   = iter.curr.p->apiRef;
      lqhTransConf->apiOpRec = iter.curr.p->apiOprec;
      sendSignal(tcNodeFailPtr.p->newTcBlockref, GSN_LQH_TRANSCONF, 
		 signal, 7, JBB);
      
      signal->theData[0] = ZSCAN_MARKERS;
      signal->theData[1] = tcNodeFailPtr.i;
      signal->theData[2] = iter.bucket;
      signal->theData[3] = iter.curr.i;
      sendSignal(cownref, GSN_CONTINUEB, signal, 4, JBB);
      return;
    }
    
    m_commitAckMarkerHash.next(iter);
  }
  
  signal->theData[0] = ZSCAN_MARKERS;
  signal->theData[1] = tcNodeFailPtr.i;
  signal->theData[2] = iter.bucket;
  signal->theData[3] = RNIL;
  sendSignal(cownref, GSN_CONTINUEB, signal, 4, JBB);
}  

/* #########################################################################
 * #######                       SCAN MODULE                         ####### 
 *
 * #########################################################################
 * -------------------------------------------------------------------------
 * THIS MODULE CONTAINS THE CODE THAT HANDLES A SCAN OF A PARTICULAR FRAGMENT
 * IT OPERATES UNDER THE CONTROL OF TC AND ORDERS ACC TO PERFORM A SCAN OF
 * ALL TUPLES IN THE FRAGMENT. TUP PERFORMS THE NECESSARY SEARCH CONDITIONS
 * TO ENSURE THAT ONLY VALID TUPLES ARE RETURNED TO THE APPLICATION.
 * ------------------------------------------------------------------------- */
/* *************** */
/*  ACC_SCANCONF > */
/* *************** */
void Dblqh::execACC_SCANCONF(Signal* signal) 
{
  AccScanConf * const accScanConf = (AccScanConf *)&signal->theData[0];
  jamEntry();
  scanptr.i = accScanConf->scanPtr;
  c_scanRecordPool.getPtr(scanptr);
  if (scanptr.p->scanState == ScanRecord::WAIT_ACC_SCAN) {
    accScanConfScanLab(signal);
  } else {
    ndbrequire(scanptr.p->scanState == ScanRecord::WAIT_ACC_COPY);
    accScanConfCopyLab(signal);
  }//if
}//Dblqh::execACC_SCANCONF()

/* ************>> */
/*  ACC_SCANREF > */
/* ************>> */
void Dblqh::execACC_SCANREF(Signal* signal) 
{
  jamEntry();
  ndbrequire(false);
}//Dblqh::execACC_SCANREF()

/* ***************>> */
/*  NEXT_SCANCONF  > */
/* ***************>> */
void Dblqh::execNEXT_SCANCONF(Signal* signal) 
{
  NextScanConf * const nextScanConf = (NextScanConf *)&signal->theData[0];
  jamEntry();
  scanptr.i = nextScanConf->scanPtr;
  c_scanRecordPool.getPtr(scanptr);
  if (nextScanConf->localKeyLength == 1) {
    jam();
    nextScanConf->localKey[1] = 
                  nextScanConf->localKey[0] & MAX_TUPLES_PER_PAGE;
    nextScanConf->localKey[0] = nextScanConf->localKey[0] >> MAX_TUPLES_BITS;
  }//if
  switch (scanptr.p->scanState) {
  case ScanRecord::WAIT_CLOSE_SCAN:
    jam();
    accScanCloseConfLab(signal);
    break;
  case ScanRecord::WAIT_CLOSE_COPY:
    jam();
    accCopyCloseConfLab(signal);
    break;
  case ScanRecord::WAIT_NEXT_SCAN:	      	       
    jam();     
    nextScanConfScanLab(signal);       
    break;
  case ScanRecord::WAIT_NEXT_SCAN_COPY:
    jam();
    nextScanConfCopyLab(signal);
    break;
  case ScanRecord::WAIT_RELEASE_LOCK:
    jam();
    ndbrequire(signal->length() == 1);
    scanLockReleasedLab(signal);
    break;
  default:
    ndbrequire(false);
  }//switch
}//Dblqh::execNEXT_SCANCONF()

/* ***************> */
/*  NEXT_SCANREF  > */
/* ***************> */
void Dblqh::execNEXT_SCANREF(Signal* signal) 
{
  jamEntry();
  systemErrorLab(signal);
  return;
}//Dblqh::execNEXT_SCANREF()

/* ******************> */
/*  STORED_PROCCONF  > */
/* ******************> */
void Dblqh::execSTORED_PROCCONF(Signal* signal) 
{
  jamEntry();
  tcConnectptr.i = signal->theData[0];
  Uint32 storedProcId = signal->theData[1];
  ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
  scanptr.i = tcConnectptr.p->tcScanRec;
  c_scanRecordPool.getPtr(scanptr);
  switch (scanptr.p->scanState) {
  case ScanRecord::WAIT_STORED_PROC_SCAN:
    jam();
    scanptr.p->scanStoredProcId = storedProcId;
    storedProcConfScanLab(signal);
    break;
  case ScanRecord::WAIT_DELETE_STORED_PROC_ID_SCAN:
    jam();
    releaseActiveFrag(signal);
    tupScanCloseConfLab(signal);
    break;
  case ScanRecord::WAIT_STORED_PROC_COPY:
    jam();
    scanptr.p->scanStoredProcId = storedProcId;
    storedProcConfCopyLab(signal);
    break;
  case ScanRecord::WAIT_DELETE_STORED_PROC_ID_COPY:
    jam();
    releaseActiveFrag(signal);
    tupCopyCloseConfLab(signal);
    break;
  default:
    ndbrequire(false);
  }//switch
}//Dblqh::execSTORED_PROCCONF()

/* ****************** */
/*  STORED_PROCREF  > */
/* ****************** */
void Dblqh::execSTORED_PROCREF(Signal* signal) 
{
  jamEntry();
  tcConnectptr.i = signal->theData[0];
  Uint32 errorCode  = signal->theData[1];
  ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
  scanptr.i = tcConnectptr.p->tcScanRec;
  c_scanRecordPool.getPtr(scanptr);
  switch (scanptr.p->scanState) {
  case ScanRecord::WAIT_STORED_PROC_SCAN:
    jam();
    scanptr.p->scanCompletedStatus = ZTRUE;
    scanptr.p->scanStoredProcId = signal->theData[2];
    tcConnectptr.p->errorCode = errorCode;
    closeScanLab(signal);
    break;
  default:
    ndbrequire(false);
  }//switch
}//Dblqh::execSTORED_PROCREF()

/* --------------------------------------------------------------------------
 *       ENTER SCAN_NEXTREQ
 * --------------------------------------------------------------------------
 *       PRECONDITION:
 *       TRANSACTION_STATE = SCAN_STATE
 *       SCAN_STATE = WAIT_SCAN_NEXTREQ
 *
 * Case scanLockHold: ZTRUE  = Unlock previous round of 
 *                             scanned row(s) and fetch next set of rows.
 *                    ZFALSE = Fetch new set of rows.
 * Number of rows to read depends on parallelism and how many rows 
 * left to scan in the fragment. SCAN_NEXTREQ can also be sent with 
 * closeFlag == ZTRUE to close the scan.
 * ------------------------------------------------------------------------- */
void Dblqh::execSCAN_NEXTREQ(Signal* signal) 
{
  jamEntry();
  const ScanFragNextReq * const nextReq = 
                                (ScanFragNextReq*)&signal->theData[0];
  const Uint32 transid1 = nextReq->transId1;
  const Uint32 transid2 = nextReq->transId2;
  const Uint32 senderData = nextReq->senderData;

  if (findTransaction(transid1, transid2, senderData) != ZOK){
    jam();
    DEBUG(senderData << 
	  " Received SCAN_NEXTREQ in LQH with close flag when closed");
    ndbrequire(nextReq->closeFlag == ZTRUE);
    return;
  }

  // Crash node if signal sender is same node
  CRASH_INSERTION2(5021, refToNode(signal->senderBlockRef()) == cownNodeid);
  // Crash node if signal sender is NOT same node
  CRASH_INSERTION2(5022, refToNode(signal->senderBlockRef()) != cownNodeid);

  if (ERROR_INSERTED(5023)){
    // Drop signal if sender is same node
    if (refToNode(signal->senderBlockRef()) == cownNodeid) {
      CLEAR_ERROR_INSERT_VALUE;
      return;
    }
  }//if
  if (ERROR_INSERTED(5024)){
    // Drop signal if sender is NOT same node
    if (refToNode(signal->senderBlockRef()) != cownNodeid) {
      CLEAR_ERROR_INSERT_VALUE;
      return;
    }
  }//if
  if (ERROR_INSERTED(5025)){
    // Delay signal if sender is NOT same node
    if (refToNode(signal->senderBlockRef()) != cownNodeid) {
      CLEAR_ERROR_INSERT_VALUE;
      sendSignalWithDelay(cownref, GSN_SCAN_NEXTREQ, signal, 1000,
			  signal->length());
      return;
    }
  }//if
  if (ERROR_INSERTED(5030)){
    ndbout << "ERROR 5030" << endl;
    CLEAR_ERROR_INSERT_VALUE;
    // Drop signal
    return;
  }//if

  if(ERROR_INSERTED(5036)){
    return;
  }

  scanptr.i = tcConnectptr.p->tcScanRec;
  ndbrequire(scanptr.i != RNIL);
  c_scanRecordPool.getPtr(scanptr);
  scanptr.p->scanTcWaiting = ZTRUE;

  /* ------------------------------------------------------------------
   * If close flag is set this scan should be closed
   * If we are waiting for SCAN_NEXTREQ set flag to stop scanning and 
   * continue execution else set flags and wait until the scan 
   * completes itself
   * ------------------------------------------------------------------ */
  if (nextReq->closeFlag == ZTRUE){
    jam();
    if(ERROR_INSERTED(5034)){
      CLEAR_ERROR_INSERT_VALUE;
    }
    if(ERROR_INSERTED(5036)){
      CLEAR_ERROR_INSERT_VALUE;
      return;
    }
    closeScanRequestLab(signal);
    return;
  }//if

  fragptr.i = tcConnectptr.p->fragmentptr;
  ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);

  /**
   * Change parameters while running
   *   (is currently not supported)
   */
  const Uint32 max_rows = nextReq->batch_size_rows;
  const Uint32 max_bytes = nextReq->batch_size_bytes;
  ndbrequire(scanptr.p->m_max_batch_size_rows == max_rows);
  ndbrequire(scanptr.p->m_max_batch_size_bytes == max_bytes);  

  /* --------------------------------------------------------------------
   * If scanLockHold = TRUE we need to unlock previous round of 
   * scanned records.
   * scanReleaseLocks will set states for this and send a NEXT_SCANREQ.
   * When confirm signal NEXT_SCANCONF arrives we call 
   * continueScanNextReqLab to continue scanning new rows and 
   * acquiring new locks.
   * -------------------------------------------------------------------- */  
  if ((scanptr.p->scanLockHold == ZTRUE) && 
      (scanptr.p->m_curr_batch_size_rows > 0)) {
    jam();
    scanptr.p->scanReleaseCounter = 1;
    scanReleaseLocksLab(signal);
    return;
  }//if

  /* -----------------------------------------------------------------------
   * We end up here when scanLockHold = FALSE or no rows was locked from 
   * previous round. 
   * Simply continue scanning.
   * ----------------------------------------------------------------------- */
  continueScanNextReqLab(signal);
}//Dblqh::execSCAN_NEXTREQ()

void Dblqh::continueScanNextReqLab(Signal* signal) 
{
  if (scanptr.p->scanCompletedStatus == ZTRUE) {
    jam();
    closeScanLab(signal);
    return;
  }//if
  
  if(scanptr.p->m_last_row){
    jam();
    scanptr.p->scanCompletedStatus = ZTRUE;
    scanptr.p->scanState = ScanRecord::WAIT_SCAN_NEXTREQ;
    sendScanFragConf(signal, ZFALSE);
    return;
  }

  // Update timer on tcConnectRecord
  tcConnectptr.p->tcTimer = cLqhTimeOutCount;
  init_acc_ptr_list(scanptr.p);
  scanptr.p->scanFlag = NextScanReq::ZSCAN_NEXT;
  scanNextLoopLab(signal);
}//Dblqh::continueScanNextReqLab()

/* -------------------------------------------------------------------------
 *       WE NEED TO RELEASE LOCKS BEFORE CONTINUING
 * ------------------------------------------------------------------------- */
void Dblqh::scanReleaseLocksLab(Signal* signal) 
{
  switch (fragptr.p->fragStatus) {
  case Fragrecord::FSACTIVE:
    jam();
    linkActiveFrag(signal);
    break;
  case Fragrecord::BLOCKED:
    jam();
    linkFragQueue(signal);
    tcConnectptr.p->transactionState = TcConnectionrec::SCAN_RELEASE_STOPPED;
    return;
    break;
  case Fragrecord::FREE:
    jam();
  case Fragrecord::ACTIVE_CREATION:
    jam();
  case Fragrecord::CRASH_RECOVERING:
    jam();
  case Fragrecord::DEFINED:
    jam();
  case Fragrecord::REMOVING:
    jam();
  default:
    ndbrequire(false);
  }//switch
  continueScanReleaseAfterBlockedLab(signal);
}//Dblqh::scanReleaseLocksLab()

void Dblqh::continueScanReleaseAfterBlockedLab(Signal* signal) 
{
  scanptr.i = tcConnectptr.p->tcScanRec;
  c_scanRecordPool.getPtr(scanptr);
  scanptr.p->scanState = ScanRecord::WAIT_RELEASE_LOCK;
  signal->theData[0] = scanptr.p->scanAccPtr;
  signal->theData[1]=
    get_acc_ptr_from_scan_record(scanptr.p,
                                scanptr.p->scanReleaseCounter -1,
                                false);
  signal->theData[2] = NextScanReq::ZSCAN_COMMIT;
  sendSignal(scanptr.p->scanBlockref, GSN_NEXT_SCANREQ, signal, 3, JBB);
}//Dblqh::continueScanReleaseAfterBlockedLab()

/* -------------------------------------------------------------------------
 *       ENTER SCAN_NEXTREQ
 * -------------------------------------------------------------------------
 *       SCAN_NEXT_REQ SIGNAL ARRIVED IN THE MIDDLE OF EXECUTION OF THE SCAN. 
 *       IT WAS A REQUEST TO CLOSE THE SCAN. WE WILL CLOSE THE SCAN IN A 
 *       CAREFUL MANNER TO ENSURE THAT NO ERROR OCCURS.
 * -------------------------------------------------------------------------
 *       PRECONDITION:
 *       TRANSACTION_STATE = SCAN_STATE_USED
 *       TSCAN_COMPLETED = ZTRUE
 * -------------------------------------------------------------------------
 *       WE CAN ALSO ARRIVE AT THIS LABEL AFTER A NODE CRASH OF THE SCAN
 *       COORDINATOR.
 * ------------------------------------------------------------------------- */
void Dblqh::closeScanRequestLab(Signal* signal) 
{
  DEBUG("transactionState = " << tcConnectptr.p->transactionState);
  switch (tcConnectptr.p->transactionState) {
  case TcConnectionrec::SCAN_STATE_USED:
    DEBUG("scanState = " << scanptr.p->scanState);
    switch (scanptr.p->scanState) {
    case ScanRecord::IN_QUEUE:
      jam();
      tupScanCloseConfLab(signal);
      break;
    case ScanRecord::WAIT_NEXT_SCAN:
      jam();
      /* -------------------------------------------------------------------
       *  SET COMPLETION STATUS AND WAIT FOR OPPORTUNITY TO STOP THE SCAN.
       * ------------------------------------------------------------------- */
      scanptr.p->scanCompletedStatus = ZTRUE;
      break;
    case ScanRecord::WAIT_ACC_SCAN:
    case ScanRecord::WAIT_STORED_PROC_SCAN:
      jam();
      /* -------------------------------------------------------------------
       *  WE ARE CURRENTLY STARTING UP THE SCAN. SET COMPLETED STATUS 
       *  AND WAIT FOR COMPLETION OF STARTUP.
       * ------------------------------------------------------------------- */
      scanptr.p->scanCompletedStatus = ZTRUE;
      break;
    case ScanRecord::WAIT_CLOSE_SCAN:
    case ScanRecord::WAIT_DELETE_STORED_PROC_ID_SCAN:
      jam();
      /*empty*/;
      break;
      /* -------------------------------------------------------------------
       *       CLOSE IS ALREADY ONGOING. WE NEED NOT DO ANYTHING.
       * ------------------------------------------------------------------- */
    case ScanRecord::WAIT_RELEASE_LOCK:
      jam();
      /* -------------------------------------------------------------------
       *  WE ARE CURRENTLY RELEASING RECORD LOCKS. AFTER COMPLETING THIS 
       *  WE WILL START TO CLOSE THE SCAN.
       * ------------------------------------------------------------------- */
      scanptr.p->scanCompletedStatus = ZTRUE;
      break;
    case ScanRecord::WAIT_SCAN_NEXTREQ:
      jam();
      /* -------------------------------------------------------------------
       * WE ARE WAITING FOR A SCAN_NEXTREQ FROM SCAN COORDINATOR(TC)
       * WICH HAVE CRASHED. CLOSE THE SCAN
       * ------------------------------------------------------------------- */
      scanptr.p->scanCompletedStatus = ZTRUE;

      fragptr.i = tcConnectptr.p->fragmentptr;
      ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);

      if (scanptr.p->scanLockHold == ZTRUE) {
	if (scanptr.p->m_curr_batch_size_rows > 0) {
	  jam();
	  scanptr.p->scanReleaseCounter = 1;
	  scanReleaseLocksLab(signal);
	  return;
	}//if
      }//if
      closeScanLab(signal);
      break;
    default:
      ndbrequire(false);
    }//switch
    break;
  case TcConnectionrec::WAIT_SCAN_AI:
    jam();
    /* ---------------------------------------------------------------------
     *  WE ARE STILL WAITING FOR THE ATTRIBUTE INFORMATION THAT 
     *  OBVIOUSLY WILL NOT ARRIVE. WE CAN QUIT IMMEDIATELY HERE.
     * --------------------------------------------------------------------- */
    //XXX jonas this have to be wrong...
    releaseOprec(signal);
    if (tcConnectptr.p->abortState == TcConnectionrec::NEW_FROM_TC) {
      jam();
      tcNodeFailptr.i = tcConnectptr.p->tcNodeFailrec;
      ptrCheckGuard(tcNodeFailptr, ctcNodeFailrecFileSize, tcNodeFailRecord);
      tcNodeFailptr.p->tcRecNow = tcConnectptr.i + 1;
      signal->theData[0] = ZLQH_TRANS_NEXT;
      signal->theData[1] = tcNodeFailptr.i;
      sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
      return;
    }//if
    tcConnectptr.p->abortState = TcConnectionrec::ABORT_ACTIVE;
    scanptr.p->m_curr_batch_size_rows = 0;
    scanptr.p->m_curr_batch_size_bytes= 0;
    sendScanFragConf(signal, ZTRUE);
    abort_scan(signal, scanptr.i, 0);
    return;
    break;
  case TcConnectionrec::SCAN_TUPKEY:
  case TcConnectionrec::SCAN_FIRST_STOPPED:
  case TcConnectionrec::SCAN_CHECK_STOPPED:
  case TcConnectionrec::SCAN_STOPPED:
    jam();
    /* ---------------------------------------------------------------------
     *       SET COMPLETION STATUS AND WAIT FOR OPPORTUNITY TO STOP THE SCAN.
     * --------------------------------------------------------------------- */
    scanptr.p->scanCompletedStatus = ZTRUE;
    break;
  case TcConnectionrec::SCAN_RELEASE_STOPPED:
    jam();
    /* ---------------------------------------------------------------------
     *  WE ARE CURRENTLY RELEASING RECORD LOCKS. AFTER COMPLETING 
     *  THIS WE WILL START TO CLOSE THE SCAN.
     * --------------------------------------------------------------------- */
    scanptr.p->scanCompletedStatus = ZTRUE;
    break;
  case TcConnectionrec::SCAN_CLOSE_STOPPED:
    jam();
    /* ---------------------------------------------------------------------
     *  CLOSE IS ALREADY ONGOING. WE NEED NOT DO ANYTHING.
     * --------------------------------------------------------------------- */
    /*empty*/;
    break;
  default:
    ndbrequire(false);
  }//switch
}//Dblqh::closeScanRequestLab()

/* -------------------------------------------------------------------------
 *       ENTER NEXT_SCANCONF
 * -------------------------------------------------------------------------
 *       PRECONDITION: SCAN_STATE = WAIT_RELEASE_LOCK
 * ------------------------------------------------------------------------- */
void Dblqh::scanLockReleasedLab(Signal* signal)
{
  tcConnectptr.i = scanptr.p->scanTcrec;
  ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);  
  releaseActiveFrag(signal);

  if (scanptr.p->scanReleaseCounter == scanptr.p->m_curr_batch_size_rows) {
    if ((scanptr.p->scanErrorCounter > 0) ||
        (scanptr.p->scanCompletedStatus == ZTRUE)) {
      jam();
      scanptr.p->m_curr_batch_size_rows = 0;
      scanptr.p->m_curr_batch_size_bytes = 0;
      closeScanLab(signal);
    } else if (scanptr.p->check_scan_batch_completed() &&
               scanptr.p->scanLockHold != ZTRUE) {
      jam();
      scanptr.p->scanState = ScanRecord::WAIT_SCAN_NEXTREQ;
      sendScanFragConf(signal, ZFALSE);
    } else if (scanptr.p->m_last_row && !scanptr.p->scanLockHold) {
      jam();
      closeScanLab(signal);
      return;
    } else {
      jam();
      /*
       * We came here after releasing locks after 
       * receiving SCAN_NEXTREQ from TC. We only come here 
       * when scanHoldLock == ZTRUE
       */
      scanptr.p->m_curr_batch_size_rows = 0;
      scanptr.p->m_curr_batch_size_bytes = 0;
      continueScanNextReqLab(signal);
    }//if
  } else if (scanptr.p->scanReleaseCounter < scanptr.p->m_curr_batch_size_rows) {
    jam();
    scanptr.p->scanReleaseCounter++;     
    scanReleaseLocksLab(signal);
  } else {
    jam();
    /*
    We come here when we have been scanning for a long time and not been able
    to find m_max_batch_size_rows records to return. We needed to release
    the record we didn't want, but now we are returning all found records to
    the API.
    */
    scanptr.p->scanState = ScanRecord::WAIT_SCAN_NEXTREQ;
    sendScanFragConf(signal, ZFALSE);
  }//if
}//Dblqh::scanLockReleasedLab()

bool
Dblqh::seize_acc_ptr_list(ScanRecord* scanP, Uint32 batch_size)
{
  Uint32 i;
  Uint32 attr_buf_recs= (batch_size + 30) / 32;

  if (batch_size > 1) {
    if (c_no_attrinbuf_recs < attr_buf_recs) {
      jam();
      return false;
    }
    for (i= 1; i <= attr_buf_recs; i++) {
      scanP->scan_acc_op_ptr[i]= seize_attrinbuf();
    }
  }
  scanP->scan_acc_attr_recs= attr_buf_recs;
  scanP->scan_acc_index = 0;
  return true;
}

void
Dblqh::release_acc_ptr_list(ScanRecord* scanP)
{
  Uint32 i, attr_buf_recs;
  attr_buf_recs= scanP->scan_acc_attr_recs;

  for (i= 1; i <= attr_buf_recs; i++) {
    release_attrinbuf(scanP->scan_acc_op_ptr[i]);
  }
  scanP->scan_acc_attr_recs= 0;
  scanP->scan_acc_index = 0;
}

Uint32
Dblqh::seize_attrinbuf()
{
  AttrbufPtr regAttrPtr;
  Uint32 ret_attr_buf;
  ndbrequire(c_no_attrinbuf_recs > 0);
  c_no_attrinbuf_recs--;
  ret_attr_buf= cfirstfreeAttrinbuf;
  regAttrPtr.i= ret_attr_buf;
  ptrCheckGuard(regAttrPtr, cattrinbufFileSize, attrbuf);
  cfirstfreeAttrinbuf= regAttrPtr.p->attrbuf[ZINBUF_NEXT];
  return ret_attr_buf;
}

Uint32
Dblqh::release_attrinbuf(Uint32 attr_buf_i)
{
  Uint32 next_buf;
  AttrbufPtr regAttrPtr;
  c_no_attrinbuf_recs++;
  regAttrPtr.i= attr_buf_i;
  ptrCheckGuard(regAttrPtr, cattrinbufFileSize, attrbuf);
  next_buf= regAttrPtr.p->attrbuf[ZINBUF_NEXT];
  regAttrPtr.p->attrbuf[ZINBUF_NEXT]= cfirstfreeAttrinbuf;
  cfirstfreeAttrinbuf= regAttrPtr.i;
  return next_buf;
}

void
Dblqh::init_acc_ptr_list(ScanRecord* scanP) 
{
  scanP->scan_acc_index = 0;
}

Uint32
Dblqh::get_acc_ptr_from_scan_record(ScanRecord* scanP,
                                    Uint32 index,
                                    bool crash_flag)
{
  Uint32* acc_ptr;
  Uint32 attr_buf_rec, attr_buf_index;
  if (!((index < MAX_PARALLEL_OP_PER_SCAN) &&
       index < scanP->scan_acc_index)) {
    ndbrequire(crash_flag);
    return RNIL;
  }
  i_get_acc_ptr(scanP, acc_ptr, index);
  return *acc_ptr;
}

void
Dblqh::set_acc_ptr_in_scan_record(ScanRecord* scanP,
                                  Uint32 index, Uint32 acc)
{
  Uint32 *acc_ptr;
  ndbrequire((index == 0 || scanP->scan_acc_index == index) &&
             (index < MAX_PARALLEL_OP_PER_SCAN));
  scanP->scan_acc_index= index + 1;
  i_get_acc_ptr(scanP, acc_ptr, index);
  *acc_ptr= acc;
}

/* -------------------------------------------------------------------------
 * SCAN_FRAGREQ: Request to start scanning the specified fragment of a table.
 * ------------------------------------------------------------------------- */
void Dblqh::execSCAN_FRAGREQ(Signal* signal) 
{
  ScanFragReq * const scanFragReq = (ScanFragReq *)&signal->theData[0];
  ScanFragRef * ref;
  const Uint32 transid1 = scanFragReq->transId1;
  const Uint32 transid2 = scanFragReq->transId2;
  Uint32 errorCode= 0;
  Uint32 senderData;
  Uint32 hashIndex;
  TcConnectionrecPtr nextHashptr;

  jamEntry();
  const Uint32 reqinfo = scanFragReq->requestInfo;
  const Uint32 fragId = (scanFragReq->fragmentNoKeyLen & 0xFFFF);
  const Uint32 keyLen = (scanFragReq->fragmentNoKeyLen >> 16);
  tabptr.i = scanFragReq->tableId;
  const Uint32 max_rows = scanFragReq->batch_size_rows;
  const Uint32 scanLockMode = ScanFragReq::getLockMode(reqinfo);
  const Uint8 keyinfo = ScanFragReq::getKeyinfoFlag(reqinfo);
  const Uint8 rangeScan = ScanFragReq::getRangeScanFlag(reqinfo);
  const Uint8 tupScan = ScanFragReq::getTupScanFlag(reqinfo);
  
  ptrCheckGuard(tabptr, ctabrecFileSize, tablerec);
  if(tabptr.p->tableStatus != Tablerec::TABLE_DEFINED){
    senderData = scanFragReq->senderData;
    goto error_handler_early_1;
  }
  
  if (cfirstfreeTcConrec != RNIL) {
    seizeTcrec();
    tcConnectptr.p->clientConnectrec = scanFragReq->senderData;
    tcConnectptr.p->clientBlockref = signal->senderBlockRef();
    tcConnectptr.p->savePointId = scanFragReq->savePointId;
  } else {
    jam();
    /* --------------------------------------------------------------------
     *      NO FREE TC RECORD AVAILABLE, THUS WE CANNOT HANDLE THE REQUEST.
     * -------------------------------------------------------------------- */
    errorCode = ZNO_TC_CONNECT_ERROR;
    senderData = scanFragReq->senderData;
    goto error_handler_early;
  }//if
  /**
   * A write allways have to get keyinfo
   */
  ndbrequire(scanLockMode == 0 || keyinfo);

  ndbrequire(max_rows > 0 && max_rows <= MAX_PARALLEL_OP_PER_SCAN);
  if (!getFragmentrec(signal, fragId)) {
    errorCode = 1231;
    goto error_handler;
  }//if

  // Verify scan type vs table type (both sides are boolean)
  if (rangeScan != DictTabInfo::isOrderedIndex(fragptr.p->tableType)) {
    errorCode = 1232;
    goto error_handler;
  }//if
  
  // 1 scan record is reserved for node recovery
  if (cscanNoFreeRec < 2) {
    jam();
    errorCode = ScanFragRef::ZNO_FREE_SCANREC_ERROR;
    goto error_handler;
  }

  // XXX adjust cmaxAccOps for range scans and remove this comment
  if ((cbookedAccOps + max_rows) > cmaxAccOps) {
    jam();
    errorCode = ScanFragRef::ZSCAN_BOOK_ACC_OP_ERROR;
    goto error_handler;
  }//if

  ndbrequire(c_scanRecordPool.seize(scanptr));
  initScanTc(signal,
             transid1,
             transid2,
             fragId,
             ZNIL);
  tcConnectptr.p->save1 = 4;
  tcConnectptr.p->primKeyLen = keyLen + 4; // hard coded in execKEYINFO
  errorCode = initScanrec(scanFragReq);
  if (errorCode != ZOK) {
    jam();
    goto error_handler2;
  }//if
  cscanNoFreeRec--;
  cbookedAccOps += max_rows;

  hashIndex = (tcConnectptr.p->transid[0] ^ tcConnectptr.p->tcOprec) & 1023;
  nextHashptr.i = ctransidHash[hashIndex];
  ctransidHash[hashIndex] = tcConnectptr.i;
  tcConnectptr.p->prevHashRec = RNIL;
  tcConnectptr.p->nextHashRec = nextHashptr.i;
  if (nextHashptr.i != RNIL) {
    jam();
    /* ---------------------------------------------------------------------
     *   ENSURE THAT THE NEXT RECORD HAS SET PREVIOUS TO OUR RECORD 
     *   IF IT EXISTS
     * --------------------------------------------------------------------- */
    ptrCheckGuard(nextHashptr, ctcConnectrecFileSize, tcConnectionrec);
    nextHashptr.p->prevHashRec = tcConnectptr.i;
  }//if
  if (scanptr.p->scanAiLength > 0) {
    jam();
    tcConnectptr.p->transactionState = TcConnectionrec::WAIT_SCAN_AI;
    return;
  }//if
  continueAfterReceivingAllAiLab(signal);
  return;

error_handler2:
  // no scan number allocated
  c_scanRecordPool.release(scanptr);
error_handler:
  ref = (ScanFragRef*)&signal->theData[0];
  tcConnectptr.p->abortState = TcConnectionrec::ABORT_ACTIVE;
  ref->senderData = tcConnectptr.p->clientConnectrec;
  ref->transId1 = transid1;
  ref->transId2 = transid2;
  ref->errorCode = errorCode;
  sendSignal(tcConnectptr.p->clientBlockref, GSN_SCAN_FRAGREF, signal, 
	     ScanFragRef::SignalLength, JBB);
  releaseOprec(signal);
  releaseTcrec(signal, tcConnectptr);
  return;

 error_handler_early_1:
  if(tabptr.p->tableStatus == Tablerec::NOT_DEFINED){
    jam();
    errorCode = ZTABLE_NOT_DEFINED;
  } else if (tabptr.p->tableStatus == Tablerec::PREP_DROP_TABLE_ONGOING ||
	     tabptr.p->tableStatus == Tablerec::PREP_DROP_TABLE_DONE){
    jam();
    errorCode = ZDROP_TABLE_IN_PROGRESS;
  } else {
    ndbrequire(0);
  }
 error_handler_early:
  ref = (ScanFragRef*)&signal->theData[0];
  ref->senderData = senderData;
  ref->transId1 = transid1;
  ref->transId2 = transid2;
  ref->errorCode = errorCode;
  sendSignal(signal->senderBlockRef(), GSN_SCAN_FRAGREF, signal,
	     ScanFragRef::SignalLength, JBB);
}//Dblqh::execSCAN_FRAGREQ()

void Dblqh::continueAfterReceivingAllAiLab(Signal* signal) 
{
  tcConnectptr.p->transactionState = TcConnectionrec::SCAN_STATE_USED;

  if(scanptr.p->scanState == ScanRecord::IN_QUEUE){
    jam();
    return;
  }
  
  scanptr.p->scanState = ScanRecord::WAIT_ACC_SCAN;
  AccScanReq * req = (AccScanReq*)&signal->theData[0];
  req->senderData = scanptr.i;
  req->senderRef = cownref;
  req->tableId = tcConnectptr.p->tableref;
  req->fragmentNo = tcConnectptr.p->fragmentid;
  req->requestInfo = 0;
  AccScanReq::setLockMode(req->requestInfo, scanptr.p->scanLockMode);
  AccScanReq::setReadCommittedFlag(req->requestInfo, scanptr.p->readCommitted);
  AccScanReq::setDescendingFlag(req->requestInfo, scanptr.p->descending);
  req->transId1 = tcConnectptr.p->transid[0];
  req->transId2 = tcConnectptr.p->transid[1];
  req->savePointId = tcConnectptr.p->savePointId;
  sendSignal(scanptr.p->scanBlockref, GSN_ACC_SCANREQ, signal, 
             AccScanReq::SignalLength, JBB);
}//Dblqh::continueAfterReceivingAllAiLab()

void Dblqh::scanAttrinfoLab(Signal* signal, Uint32* dataPtr, Uint32 length) 
{
  scanptr.i = tcConnectptr.p->tcScanRec;
  c_scanRecordPool.getPtr(scanptr);
  if (saveTupattrbuf(signal, dataPtr, length) == ZOK) {
    if (tcConnectptr.p->currTupAiLen < scanptr.p->scanAiLength) {
      jam();
    } else {
      jam();
      ndbrequire(tcConnectptr.p->currTupAiLen == scanptr.p->scanAiLength);
      continueAfterReceivingAllAiLab(signal);
    }//if
    return;
  }//if
  abort_scan(signal, scanptr.i, ZGET_ATTRINBUF_ERROR);
}

void Dblqh::abort_scan(Signal* signal, Uint32 scan_ptr_i, Uint32 errcode){
  jam();
  scanptr.i = scan_ptr_i;
  c_scanRecordPool.getPtr(scanptr);

  fragptr.i = tcConnectptr.p->fragmentptr;
  ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
  finishScanrec(signal);
  releaseScanrec(signal);
  tcConnectptr.p->transactionState = TcConnectionrec::IDLE;
  tcConnectptr.p->abortState = TcConnectionrec::ABORT_ACTIVE;

  if(errcode)
  {
    jam();
    ScanFragRef * ref = (ScanFragRef*)&signal->theData[0];
    ref->senderData = tcConnectptr.p->clientConnectrec;
    ref->transId1 = tcConnectptr.p->transid[0];
    ref->transId2 = tcConnectptr.p->transid[1];
    ref->errorCode = errcode;
    sendSignal(tcConnectptr.p->clientBlockref, GSN_SCAN_FRAGREF, signal, 
	       ScanFragRef::SignalLength, JBB);
  }
  deleteTransidHash(signal);
  releaseOprec(signal);
  releaseTcrec(signal, tcConnectptr);
}

/*---------------------------------------------------------------------*/
/* Send this 'I am alive' signal to TC when it is received from ACC    */
/* We include the scanPtr.i that comes from ACC in signalData[1], this */
/* tells TC which fragment record to check for a timeout.              */
/*---------------------------------------------------------------------*/
void Dblqh::execSCAN_HBREP(Signal* signal)
{
  jamEntry();
  scanptr.i = signal->theData[0];
  c_scanRecordPool.getPtr(scanptr);
  switch(scanptr.p->scanType){
  case ScanRecord::SCAN:
    if (scanptr.p->scanTcWaiting == ZTRUE) {
      jam();
      tcConnectptr.i = scanptr.p->scanTcrec;  
      ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);

      ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
      const Uint32 transid1  = signal->theData[1];
      const Uint32 transid2  = signal->theData[2];
      ndbrequire(transid1 == tcConnectptr.p->transid[0] && 
		 transid2 == tcConnectptr.p->transid[1]);

      // Update counter on tcConnectPtr
      if (tcConnectptr.p->tcTimer != 0){
	tcConnectptr.p->tcTimer = cLqhTimeOutCount;
      } else {
        jam();
	//ndbout << "SCAN_HBREP when tcTimer was off" << endl;	
      }
      
      signal->theData[0] = tcConnectptr.p->clientConnectrec;
      signal->theData[1] = tcConnectptr.p->transid[0];
      signal->theData[2] = tcConnectptr.p->transid[1];
      sendSignal(tcConnectptr.p->clientBlockref,
                 GSN_SCAN_HBREP, signal, 3, JBB);
    }//if
    break;
  case ScanRecord::COPY:
    //    ndbout << "Dblqh::execSCAN_HBREP Dropping SCAN_HBREP" << endl;
    break;
  default:
    ndbrequire(false);
  }
}

void Dblqh::accScanConfScanLab(Signal* signal) 
{
  AccScanConf * const accScanConf = (AccScanConf *)&signal->theData[0];
  tcConnectptr.i = scanptr.p->scanTcrec;
  ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
  /* -----------------------------------------------------------------------
   *       PRECONDITION: SCAN_STATE = WAIT_ACC_SCAN
   * ----------------------------------------------------------------------- */
  if (accScanConf->flag == AccScanConf::ZEMPTY_FRAGMENT) {
    jam();
    /* ---------------------------------------------------------------------
     *       THE FRAGMENT WAS EMPTY.
     *       REPORT SUCCESSFUL COPYING.
     * --------------------------------------------------------------------- */
    tupScanCloseConfLab(signal);
    return;
  }//if
  scanptr.p->scanAccPtr = accScanConf->accPtr;
  if (scanptr.p->rangeScan) {
    jam();
    TuxBoundInfo* req = (TuxBoundInfo*)signal->getDataPtrSend();
    req->errorCode = RNIL;
    req->tuxScanPtrI = scanptr.p->scanAccPtr;
    Uint32 len = req->boundAiLength = copy_bounds(req->data, tcConnectptr.p);
    EXECUTE_DIRECT(DBTUX, GSN_TUX_BOUND_INFO, signal, 
		   TuxBoundInfo::SignalLength + len);
    
    jamEntry();
    if (req->errorCode != 0) {
      jam();
      /*
       * Cannot use STORED_PROCREF to abort since even the REF
       * returns a stored proc id.  So record error and continue.
       * The scan is already Invalid in TUX and returns empty set.
       */
      tcConnectptr.p->errorCode = req->errorCode;
    }
  }

  scanptr.p->scanState = ScanRecord::WAIT_STORED_PROC_SCAN;
  if(scanptr.p->scanStoredProcId == RNIL)
  {
    jam();
    signal->theData[0] = tcConnectptr.p->tupConnectrec;
    signal->theData[1] = tcConnectptr.p->tableref;
    signal->theData[2] = scanptr.p->scanSchemaVersion;
    signal->theData[3] = ZSTORED_PROC_SCAN;
    
    signal->theData[4] = scanptr.p->scanAiLength;
    sendSignal(tcConnectptr.p->tcTupBlockref,
	       GSN_STORED_PROCREQ, signal, 5, JBB);
    
    signal->theData[0] = tcConnectptr.p->tupConnectrec;
    AttrbufPtr regAttrinbufptr;
    Uint32 firstAttr = regAttrinbufptr.i = tcConnectptr.p->firstAttrinbuf;
    while (regAttrinbufptr.i != RNIL) {
      ptrCheckGuard(regAttrinbufptr, cattrinbufFileSize, attrbuf);
      jam();
      Uint32 dataLen = regAttrinbufptr.p->attrbuf[ZINBUF_DATA_LEN];
      ndbrequire(dataLen != 0);
      // first 3 words already set in STORED_PROCREQ
      MEMCOPY_NO_WORDS(&signal->theData[3],
		       &regAttrinbufptr.p->attrbuf[0],
		       dataLen);
      sendSignal(tcConnectptr.p->tcTupBlockref,
		 GSN_ATTRINFO, signal, dataLen + 3, JBB);
      regAttrinbufptr.i = regAttrinbufptr.p->attrbuf[ZINBUF_NEXT];
      c_no_attrinbuf_recs++;
    }//while
    
    /**
     * Release attr info
     */
    if(firstAttr != RNIL)
    {
      regAttrinbufptr.p->attrbuf[ZINBUF_NEXT] = cfirstfreeAttrinbuf;
      cfirstfreeAttrinbuf = firstAttr;
      tcConnectptr.p->firstAttrinbuf = tcConnectptr.p->lastAttrinbuf = RNIL;
    }
  } 
  else 
  {
    jam();
    storedProcConfScanLab(signal);
  }
}//Dblqh::accScanConfScanLab()

#define print_buf(s,idx,len) {\
  printf(s); Uint32 t2=len; DatabufPtr t3; t3.i = idx; \
  while(t3.i != RNIL && t2-- > 0){\
    ptrCheckGuard(t3, cdatabufFileSize, databuf);\
    printf("%d ", t3.i); t3.i= t3.p->nextDatabuf;\
  } printf("\n"); }

Uint32
Dblqh::copy_bounds(Uint32 * dst, TcConnectionrec* tcPtrP)
{
  /**
   * copy_bounds handles multiple bounds by
   *   in the 16 upper bits of the first words (used to specify bound type)
   *   setting the length of this specific bound
   *
   */

  DatabufPtr regDatabufptr;
  Uint32 left = 4 - tcPtrP->m_offset_current_keybuf; // left in buf
  Uint32 totalLen = tcPtrP->primKeyLen - 4;
  regDatabufptr.i = tcPtrP->firstTupkeybuf;

  ndbassert(tcPtrP->primKeyLen >= 4);
  ndbassert(tcPtrP->m_offset_current_keybuf < 4);
  ndbassert(!(totalLen == 0 && regDatabufptr.i != RNIL)); 
  ndbassert(!(totalLen != 0 && regDatabufptr.i == RNIL));
  
  if(totalLen)
  {
    ptrCheckGuard(regDatabufptr, cdatabufFileSize, databuf);
    Uint32 sig0 = regDatabufptr.p->data[0];
    Uint32 sig1 = regDatabufptr.p->data[1];
    Uint32 sig2 = regDatabufptr.p->data[2];
    Uint32 sig3 = regDatabufptr.p->data[3];
    
    switch(left){
    case 4:
      * dst++ = sig0;
    case 3:
      * dst++ = sig1;
    case 2:
      * dst++ = sig2;
    case 1:
      * dst++ = sig3;
    }
    
    Uint32 first = (* (dst - left)); // First word in range
    
    // Length of this range
    Uint8 offset;
    const Uint32 len = (first >> 16) ? (first >> 16) : totalLen;
    tcPtrP->m_scan_curr_range_no = (first & 0xFFF0) >> 4;
    (* (dst - left)) = (first & 0xF); // Remove length & range no 
    
    if(len < left)
    {
      offset = len;
    }
    else
    {
      Databuf * lastP;
      left = (len - left);
      regDatabufptr.i = regDatabufptr.p->nextDatabuf;
      
      while(left >= 4)
      {
	left -= 4;
	lastP = regDatabufptr.p;
	ptrCheckGuard(regDatabufptr, cdatabufFileSize, databuf);
	sig0 = regDatabufptr.p->data[0];
	sig1 = regDatabufptr.p->data[1];
	sig2 = regDatabufptr.p->data[2];
	sig3 = regDatabufptr.p->data[3];
	regDatabufptr.i = regDatabufptr.p->nextDatabuf;

	* dst++ = sig0;
	* dst++ = sig1;
	* dst++ = sig2;
	* dst++ = sig3;
      }
  
      if(left > 0)
      {
	lastP = regDatabufptr.p;
	ptrCheckGuard(regDatabufptr, cdatabufFileSize, databuf);
	sig0 = regDatabufptr.p->data[0];
	sig1 = regDatabufptr.p->data[1];
	sig2 = regDatabufptr.p->data[2];
	sig3 = regDatabufptr.p->data[3];
	* dst++ = sig0;
	* dst++ = sig1;
	* dst++ = sig2;
	* dst++ = sig3;
      }
      else
      {
	lastP = regDatabufptr.p;
      }
      offset = left & 3;
      lastP->nextDatabuf = cfirstfreeDatabuf;
      cfirstfreeDatabuf = tcPtrP->firstTupkeybuf;
      ndbassert(cfirstfreeDatabuf != RNIL);
    }

    if(len == totalLen && regDatabufptr.i != RNIL)
    {
      regDatabufptr.p->nextDatabuf = cfirstfreeDatabuf;
      cfirstfreeDatabuf = regDatabufptr.i;
      tcPtrP->lastTupkeybuf = regDatabufptr.i = RNIL;
      ndbassert(cfirstfreeDatabuf != RNIL);
    }
    
    tcPtrP->m_offset_current_keybuf = offset;
    tcPtrP->firstTupkeybuf = regDatabufptr.i;
    tcPtrP->primKeyLen = 4 + totalLen - len;

    return len;
  }
  return totalLen;
}

/* -------------------------------------------------------------------------
 *       ENTER STORED_PROCCONF WITH
 *         TC_CONNECTPTR,
 *         TSTORED_PROC_ID
 * -------------------------------------------------------------------------
 *       PRECONDITION: SCAN_STATE = WAIT_STORED_PROC_SCAN
 * ------------------------------------------------------------------------- */
void Dblqh::storedProcConfScanLab(Signal* signal) 
{
  fragptr.i = tcConnectptr.p->fragmentptr;
  ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
  if (scanptr.p->scanCompletedStatus == ZTRUE) {
    jam();
    // STOP THE SCAN PROCESS IF THIS HAS BEEN REQUESTED.
    closeScanLab(signal);
    return;
  }//if
  switch (fragptr.p->fragStatus) {
  case Fragrecord::FSACTIVE:
    jam();
    linkActiveFrag(signal);
    break;
  case Fragrecord::BLOCKED:
    jam();
    linkFragQueue(signal);
    tcConnectptr.p->transactionState = TcConnectionrec::SCAN_FIRST_STOPPED;
    return;
    break;
  case Fragrecord::FREE:  
    jam();
  case Fragrecord::ACTIVE_CREATION:
    jam();
  case Fragrecord::CRASH_RECOVERING:
    jam();
  case Fragrecord::DEFINED:
    jam();
  case Fragrecord::REMOVING:
    jam();
  default:
    ndbrequire(false);
    break;
  }//switch
  continueFirstScanAfterBlockedLab(signal);
}//Dblqh::storedProcConfScanLab()

void Dblqh::continueFirstScanAfterBlockedLab(Signal* signal) 
{
  scanptr.i = tcConnectptr.p->tcScanRec;
  c_scanRecordPool.getPtr(scanptr);
  scanptr.p->scanState = ScanRecord::WAIT_NEXT_SCAN;
  signal->theData[0] = scanptr.p->scanAccPtr;
  signal->theData[1] = RNIL;
  signal->theData[2] = NextScanReq::ZSCAN_NEXT;
  sendSignal(scanptr.p->scanBlockref, GSN_NEXT_SCANREQ, signal, 3, JBB);
  return;
}//Dblqh::continueFirstScanAfterBlockedLab()

/* ------------------------------------------------------------------------- 
 * When executing a scan we must come up to the surface at times to make 
 * sure we can quickly start local checkpoints.
 * ------------------------------------------------------------------------- */
void Dblqh::execCHECK_LCP_STOP(Signal* signal)
{
  jamEntry();
  scanptr.i = signal->theData[0];
  c_scanRecordPool.getPtr(scanptr);
  tcConnectptr.i = scanptr.p->scanTcrec;
  ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
  fragptr.i = tcConnectptr.p->fragmentptr;
  ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
  if (signal->theData[1] == ZTRUE) {
    jam();
    releaseActiveFrag(signal);
    signal->theData[0] = ZCHECK_LCP_STOP_BLOCKED;
    signal->theData[1] = scanptr.i;
    sendSignalWithDelay(cownref, GSN_CONTINUEB, signal, 10, 2);
    signal->theData[0] = RNIL;
    return;
  }//if
  if (fragptr.p->fragStatus != Fragrecord::FSACTIVE) {
    ndbrequire(fragptr.p->fragStatus == Fragrecord::BLOCKED); 
    releaseActiveFrag(signal);
    linkFragQueue(signal);
    tcConnectptr.p->transactionState = TcConnectionrec::SCAN_CHECK_STOPPED;
    signal->theData[0] = RNIL;
  }//if
}//Dblqh::execCHECK_LCP_STOP()

void Dblqh::checkLcpStopBlockedLab(Signal* signal)
{
  switch (fragptr.p->fragStatus) {
  case Fragrecord::FSACTIVE:
    jam();
    linkActiveFrag(signal);
    continueAfterCheckLcpStopBlocked(signal);
    break;
  case Fragrecord::BLOCKED:
    jam();
    linkFragQueue(signal);
    tcConnectptr.p->transactionState = TcConnectionrec::SCAN_CHECK_STOPPED;
    return;
    break;
  case Fragrecord::FREE:
    jam();
  case Fragrecord::ACTIVE_CREATION:
    jam();
  case Fragrecord::CRASH_RECOVERING:
    jam();
  case Fragrecord::DEFINED:
    jam();
  case Fragrecord::REMOVING:
    jam();
  default:
    ndbrequire(false);
  }//switch
}//Dblqh::checkLcpStopBlockedLab()

void Dblqh::continueAfterCheckLcpStopBlocked(Signal* signal)
{
  scanptr.i = tcConnectptr.p->tcScanRec;
  c_scanRecordPool.getPtr(scanptr);
  signal->theData[0] = scanptr.p->scanAccPtr;
  signal->theData[1] = AccCheckScan::ZNOT_CHECK_LCP_STOP;
  EXECUTE_DIRECT(refToBlock(scanptr.p->scanBlockref), GSN_ACC_CHECK_SCAN,
      signal, 2);
}//Dblqh::continueAfterCheckLcpStopBlocked()

/* -------------------------------------------------------------------------
 *       ENTER NEXT_SCANCONF
 * -------------------------------------------------------------------------
 *       PRECONDITION: SCAN_STATE = WAIT_NEXT_SCAN
 * ------------------------------------------------------------------------- */
void Dblqh::nextScanConfScanLab(Signal* signal) 
{
  NextScanConf * const nextScanConf = (NextScanConf *)&signal->theData[0];
  tcConnectptr.i = scanptr.p->scanTcrec;
  ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
  if (nextScanConf->fragId == RNIL) {
    jam();
    /* ---------------------------------------------------------------------
     *       THERE ARE NO MORE TUPLES TO FETCH. IF WE HAVE ANY
     *       OPERATIONS STILL NEEDING A LOCK WE REPORT TO THE
     *       APPLICATION AND CLOSE THE SCAN WHEN THE NEXT SCAN
     *       REQUEST IS RECEIVED. IF WE DO NOT HAVE ANY NEED FOR
     *       LOCKS WE CAN CLOSE THE SCAN IMMEDIATELY.
     * --------------------------------------------------------------------- */
    releaseActiveFrag(signal);
    /*************************************************************
     *       STOP THE SCAN PROCESS IF THIS HAS BEEN REQUESTED.
     ************************************************************ */    
    if (!scanptr.p->scanLockHold)
    {
      jam();
      closeScanLab(signal);
      return;
    }

    if (scanptr.p->scanCompletedStatus == ZTRUE) {
      if ((scanptr.p->scanLockHold == ZTRUE) && 
	  (scanptr.p->m_curr_batch_size_rows > 0)) {
	jam();
	scanptr.p->scanReleaseCounter = 1;
	scanReleaseLocksLab(signal);
	return;
      }//if
      jam();
      closeScanLab(signal);
      return;
    }//if

    if (scanptr.p->m_curr_batch_size_rows > 0) {
      jam();

      if((tcConnectptr.p->primKeyLen - 4) == 0)
	scanptr.p->scanCompletedStatus = ZTRUE;
      
      scanptr.p->scanState = ScanRecord::WAIT_SCAN_NEXTREQ;
      sendScanFragConf(signal, ZFALSE);
      return;
    }//if
    closeScanLab(signal);
    return;
  }//if

  // If accOperationPtr == RNIL no record was returned by ACC
  if (nextScanConf->accOperationPtr == RNIL) {
    jam();
    /*************************************************************
     *       STOP THE SCAN PROCESS IF THIS HAS BEEN REQUESTED.
     ************************************************************ */    
    if (scanptr.p->scanCompletedStatus == ZTRUE) {
      releaseActiveFrag(signal);
      if ((scanptr.p->scanLockHold == ZTRUE) && 
	  (scanptr.p->m_curr_batch_size_rows > 0)) {
	jam();
	scanptr.p->scanReleaseCounter = 1;
	scanReleaseLocksLab(signal);
	return;
      }//if
      jam();
      closeScanLab(signal);
      return;
    }//if

    if (scanptr.p->m_curr_batch_size_rows > 0) {
      jam();
      releaseActiveFrag(signal);
      scanptr.p->scanState = ScanRecord::WAIT_SCAN_NEXTREQ;
      sendScanFragConf(signal, ZFALSE);
      return;
    }//if

    signal->theData[0] = scanptr.p->scanAccPtr;
    signal->theData[1] = AccCheckScan::ZCHECK_LCP_STOP;
    sendSignal(scanptr.p->scanBlockref,
               GSN_ACC_CHECK_SCAN, signal, 2, JBB);
    return;
  }//if
  jam();
  set_acc_ptr_in_scan_record(scanptr.p,
                             scanptr.p->m_curr_batch_size_rows,
                             nextScanConf->accOperationPtr);
  jam();
  scanptr.p->scanLocalref[0] = nextScanConf->localKey[0];
  scanptr.p->scanLocalref[1] = nextScanConf->localKey[1];
  scanptr.p->scanLocalFragid = nextScanConf->fragId;
  nextScanConfLoopLab(signal);
}//Dblqh::nextScanConfScanLab()

void Dblqh::nextScanConfLoopLab(Signal* signal) 
{
  /* ----------------------------------------------------------------------
   *       STOP THE SCAN PROCESS IF THIS HAS BEEN REQUESTED.
   * ---------------------------------------------------------------------- */
  if (scanptr.p->scanCompletedStatus == ZTRUE) {
    jam();
    releaseActiveFrag(signal);
    if ((scanptr.p->scanLockHold == ZTRUE) && 
        (scanptr.p->m_curr_batch_size_rows > 0)) {
      jam();
      scanptr.p->scanReleaseCounter = 1;
      scanReleaseLocksLab(signal);
      return;
    }//if
    closeScanLab(signal);
    return;
  }//if
  jam();
  Uint32 tableRef;
  Uint32 tupFragPtr;
  Uint32 reqinfo = (scanptr.p->scanLockHold == ZFALSE);
  reqinfo = reqinfo + (tcConnectptr.p->operation << 6);
  reqinfo = reqinfo + (tcConnectptr.p->opExec << 10);
  tcConnectptr.p->transactionState = TcConnectionrec::SCAN_TUPKEY;
  fragptr.i = tcConnectptr.p->fragmentptr;
  ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
  if (! scanptr.p->rangeScan) {
    tableRef = tcConnectptr.p->tableref;
    tupFragPtr = fragptr.p->tupFragptr[scanptr.p->scanLocalFragid & 1];
  } else {
    jam();
    // for ordered index use primary table
    FragrecordPtr tFragPtr;
    tFragPtr.i = fragptr.p->tableFragptr;
    ptrCheckGuard(tFragPtr, cfragrecFileSize, fragrecord);
    tableRef = tFragPtr.p->tabRef;
    tupFragPtr = tFragPtr.p->tupFragptr[scanptr.p->scanLocalFragid & 1];
  }
  {
    jam();
    TupKeyReq * const tupKeyReq = (TupKeyReq *)signal->getDataPtrSend(); 

    tupKeyReq->connectPtr = tcConnectptr.p->tupConnectrec;
    tupKeyReq->request = reqinfo;
    tupKeyReq->tableRef = tableRef;
    tupKeyReq->fragId = scanptr.p->scanLocalFragid;
    tupKeyReq->keyRef1 = scanptr.p->scanLocalref[0];
    tupKeyReq->keyRef2 = scanptr.p->scanLocalref[1];
    tupKeyReq->attrBufLen = 0;
    tupKeyReq->opRef = scanptr.p->scanApiOpPtr; 
    tupKeyReq->applRef = scanptr.p->scanApiBlockref;
    tupKeyReq->schemaVersion = scanptr.p->scanSchemaVersion;
    tupKeyReq->storedProcedure = scanptr.p->scanStoredProcId;
    tupKeyReq->transId1 = tcConnectptr.p->transid[0];
    tupKeyReq->transId2 = tcConnectptr.p->transid[1];
    tupKeyReq->fragPtr = tupFragPtr;
    tupKeyReq->primaryReplica = (tcConnectptr.p->seqNoReplica == 0)?true:false;
    tupKeyReq->coordinatorTC = tcConnectptr.p->tcBlockref;
    tupKeyReq->tcOpIndex = tcConnectptr.p->tcOprec;
    tupKeyReq->savePointId = tcConnectptr.p->savePointId;
    Uint32 blockNo = refToBlock(tcConnectptr.p->tcTupBlockref);
    EXECUTE_DIRECT(blockNo, GSN_TUPKEYREQ, signal, 
               TupKeyReq::SignalLength);
  }
}

/* -------------------------------------------------------------------------
 *       RECEPTION OF FURTHER KEY INFORMATION WHEN KEY SIZE > 16 BYTES.
 * -------------------------------------------------------------------------
 *       PRECONDITION:   SCAN_STATE = WAIT_SCAN_KEYINFO
 * ------------------------------------------------------------------------- */
void 
Dblqh::keyinfoLab(const Uint32 * src, const Uint32 * end) 
{
  do {
    jam();
    seizeTupkeybuf(0);
    databufptr.p->data[0] = * src ++;
    databufptr.p->data[1] = * src ++;
    databufptr.p->data[2] = * src ++;
    databufptr.p->data[3] = * src ++;
  } while (src < end);
}//Dblqh::keyinfoLab()

Uint32
Dblqh::readPrimaryKeys(ScanRecord *scanP, TcConnectionrec *tcConP, Uint32 *dst)
{
  Uint32 tableId = tcConP->tableref;
  Uint32 fragId = scanP->scanLocalFragid;
  Uint32 fragPageId = scanP->scanLocalref[0];
  Uint32 pageIndex = scanP->scanLocalref[1];

  if(scanP->rangeScan)
  {
    jam();
    // for ordered index use primary table
    FragrecordPtr tFragPtr;
    tFragPtr.i = fragptr.p->tableFragptr;
    ptrCheckGuard(tFragPtr, cfragrecFileSize, fragrecord);
    tableId = tFragPtr.p->tabRef;
  }

  int ret = c_tup->accReadPk(tableId, fragId, fragPageId, pageIndex, dst, false);
  if(0)
    ndbout_c("readPrimaryKeys(table: %d fragment: %d [ %d %d ] -> %d",
	     tableId, fragId, fragPageId, pageIndex, ret);
  ndbassert(ret > 0);

  return ret;
}

/* -------------------------------------------------------------------------
 *         ENTER TUPKEYCONF
 * -------------------------------------------------------------------------
 *       PRECONDITION:   TRANSACTION_STATE = SCAN_TUPKEY
 * ------------------------------------------------------------------------- */
void Dblqh::scanTupkeyConfLab(Signal* signal) 
{
  const TupKeyConf * conf = (TupKeyConf *)signal->getDataPtr();
  UintR tdata4 = conf->readLength;
  UintR tdata5 = conf->lastRow;

  tcConnectptr.p->transactionState = TcConnectionrec::SCAN_STATE_USED;
  scanptr.i = tcConnectptr.p->tcScanRec;
  releaseActiveFrag(signal);
  c_scanRecordPool.getPtr(scanptr);
  if (scanptr.p->scanCompletedStatus == ZTRUE) {
    /* ---------------------------------------------------------------------
     *       STOP THE SCAN PROCESS IF THIS HAS BEEN REQUESTED.
     * --------------------------------------------------------------------- */
    if ((scanptr.p->scanLockHold == ZTRUE) && 
        (scanptr.p->m_curr_batch_size_rows > 0)) {
      jam();
      scanptr.p->scanReleaseCounter = 1;
      scanReleaseLocksLab(signal);
      return;
    }//if
    jam();
    closeScanLab(signal);
    return;
  }//if
  if (scanptr.p->scanKeyinfoFlag) {
    jam();
    // Inform API about keyinfo len aswell
    tdata4 += sendKeyinfo20(signal, scanptr.p, tcConnectptr.p);
  }//if
  ndbrequire(scanptr.p->m_curr_batch_size_rows < MAX_PARALLEL_OP_PER_SCAN);
  scanptr.p->m_curr_batch_size_bytes+= tdata4;
  scanptr.p->m_curr_batch_size_rows++;
  scanptr.p->m_last_row = tdata5;
  if (scanptr.p->check_scan_batch_completed() | tdata5){
    if (scanptr.p->scanLockHold == ZTRUE) {
      jam();
      scanptr.p->scanState = ScanRecord::WAIT_SCAN_NEXTREQ;
      sendScanFragConf(signal, ZFALSE);
      return;
    } else {
      jam();
      scanptr.p->scanReleaseCounter = scanptr.p->m_curr_batch_size_rows;
      scanReleaseLocksLab(signal);
      return;
    }
  } else {
    if (scanptr.p->scanLockHold == ZTRUE) {
      jam();
      scanptr.p->scanFlag = NextScanReq::ZSCAN_NEXT;
    } else {
      jam();
      scanptr.p->scanFlag = NextScanReq::ZSCAN_NEXT_COMMIT;
    }
  }
  scanNextLoopLab(signal);
}//Dblqh::scanTupkeyConfLab()

void Dblqh::scanNextLoopLab(Signal* signal) 
{
  switch (fragptr.p->fragStatus) {
  case Fragrecord::FSACTIVE:
    jam();
    linkActiveFrag(signal);
    break;
  case Fragrecord::BLOCKED:
    jam();
    linkFragQueue(signal);
    tcConnectptr.p->transactionState = TcConnectionrec::SCAN_STOPPED;
    return;
    break;
  case Fragrecord::FREE:
    jam();
  case Fragrecord::ACTIVE_CREATION:
    jam();
  case Fragrecord::CRASH_RECOVERING:
    jam();
  case Fragrecord::DEFINED:
    jam();
  case Fragrecord::REMOVING:
    jam();
  default:
    ndbrequire(false);
  }//switch
  continueScanAfterBlockedLab(signal);
}//Dblqh::scanNextLoopLab()

void Dblqh::continueScanAfterBlockedLab(Signal* signal) 
{
  scanptr.i = tcConnectptr.p->tcScanRec;
  c_scanRecordPool.getPtr(scanptr);
  Uint32 accOpPtr;
  if (scanptr.p->scanFlag == NextScanReq::ZSCAN_NEXT_ABORT) {
    jam();
    scanptr.p->scanFlag = NextScanReq::ZSCAN_NEXT_COMMIT;
    accOpPtr= get_acc_ptr_from_scan_record(scanptr.p,
					   scanptr.p->m_curr_batch_size_rows,
					   false);
    scanptr.p->scan_acc_index--;
  } else if (scanptr.p->scanFlag == NextScanReq::ZSCAN_NEXT_COMMIT) {
    jam();
    accOpPtr= get_acc_ptr_from_scan_record(scanptr.p,
					   scanptr.p->m_curr_batch_size_rows-1,
					   false);
  } else {
    jam();
    accOpPtr = RNIL; // The value is not used in ACC
  }//if
  scanptr.p->scanState = ScanRecord::WAIT_NEXT_SCAN;
  signal->theData[0] = scanptr.p->scanAccPtr;
  signal->theData[1] = accOpPtr;
  signal->theData[2] = scanptr.p->scanFlag;
  sendSignal(scanptr.p->scanBlockref, GSN_NEXT_SCANREQ, signal, 3, JBB);
}//Dblqh::continueScanAfterBlockedLab()

/* -------------------------------------------------------------------------
 *         ENTER TUPKEYREF WITH
 *               TC_CONNECTPTR,
 *               TERROR_CODE
 * -------------------------------------------------------------------------
 *       PRECONDITION:   TRANSACTION_STATE = SCAN_TUPKEY
 * ------------------------------------------------------------------------- */
void Dblqh::scanTupkeyRefLab(Signal* signal) 
{
  tcConnectptr.p->transactionState = TcConnectionrec::SCAN_STATE_USED;
  scanptr.i = tcConnectptr.p->tcScanRec;
  releaseActiveFrag(signal);
  c_scanRecordPool.getPtr(scanptr);
  if (scanptr.p->scanCompletedStatus == ZTRUE) {
    /* ---------------------------------------------------------------------
     *       STOP THE SCAN PROCESS IF THIS HAS BEEN REQUESTED.
     * --------------------------------------------------------------------- */
    if ((scanptr.p->scanLockHold == ZTRUE) && 
        (scanptr.p->m_curr_batch_size_rows > 0)) {
      jam();
      scanptr.p->scanReleaseCounter = 1;
      scanReleaseLocksLab(signal);
      return;
    }//if
    jam();
    closeScanLab(signal);
    return;
  }//if
  if ((terrorCode != ZSEARCH_CONDITION_FALSE) &&
      (terrorCode != ZNO_TUPLE_FOUND) &&
      (terrorCode >= ZUSER_ERROR_CODE_LIMIT)) {
    scanptr.p->scanErrorCounter++;
    tcConnectptr.p->errorCode = terrorCode;

    if (scanptr.p->scanLockHold == ZTRUE) {
      jam();
      scanptr.p->scanReleaseCounter = 1;
    } else {
      jam();
      scanptr.p->m_curr_batch_size_rows++;
      scanptr.p->scanReleaseCounter = scanptr.p->m_curr_batch_size_rows;
    }//if
    /* --------------------------------------------------------------------
     *       WE NEED TO RELEASE ALL LOCKS CURRENTLY
     *       HELD BY THIS SCAN.
     * -------------------------------------------------------------------- */ 
    scanReleaseLocksLab(signal);
    return;
  }//if
  Uint32 time_passed= tcConnectptr.p->tcTimer - cLqhTimeOutCount;
  if (scanptr.p->m_curr_batch_size_rows > 0) {
    if (time_passed > 1) {
  /* -----------------------------------------------------------------------
   *  WE NEED TO ENSURE THAT WE DO NOT SEARCH FOR THE NEXT TUPLE FOR A 
   *  LONG TIME WHILE WE KEEP A LOCK ON A FOUND TUPLE. WE RATHER REPORT 
   *  THE FOUND TUPLE IF FOUND TUPLES ARE RARE. If more than 10 ms passed we
   *  send the found tuples to the API.
   * ----------------------------------------------------------------------- */
      scanptr.p->scanReleaseCounter = scanptr.p->m_curr_batch_size_rows + 1;
      scanReleaseLocksLab(signal);
      return;
    }
  } else {
    if (time_passed > 10) {
      jam();
      signal->theData[0]= scanptr.i;
      signal->theData[1]= tcConnectptr.p->transid[0];
      signal->theData[2]= tcConnectptr.p->transid[1];
      execSCAN_HBREP(signal);
    }
  }
  scanptr.p->scanFlag = NextScanReq::ZSCAN_NEXT_ABORT;
  scanNextLoopLab(signal);
}//Dblqh::scanTupkeyRefLab()

/* -------------------------------------------------------------------------
 *   THE SCAN HAS BEEN COMPLETED. EITHER BY REACHING THE END OR BY COMMAND 
 *   FROM THE APPLICATION OR BY SOME SORT OF ERROR CONDITION.                
 * ------------------------------------------------------------------------- */
void Dblqh::closeScanLab(Signal* signal) 
{
  fragptr.i = tcConnectptr.p->fragmentptr;
  ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
  switch (fragptr.p->fragStatus) {
  case Fragrecord::FSACTIVE:
    jam();
    linkActiveFrag(signal);
    break;
  case Fragrecord::BLOCKED:
    jam();
    linkFragQueue(signal);
    tcConnectptr.p->transactionState = TcConnectionrec::SCAN_CLOSE_STOPPED;
    return;
    break;
  case Fragrecord::FREE:
    jam();
  case Fragrecord::ACTIVE_CREATION:
    jam();
  case Fragrecord::CRASH_RECOVERING:
    jam();
  case Fragrecord::DEFINED:
    jam();
  case Fragrecord::REMOVING:
    jam();
  default:
    ndbrequire(false);
  }//switch
  continueCloseScanAfterBlockedLab(signal);
}//Dblqh::closeScanLab()

void Dblqh::continueCloseScanAfterBlockedLab(Signal* signal) 
{
  tcConnectptr.p->transactionState = TcConnectionrec::SCAN_STATE_USED;
  scanptr.i = tcConnectptr.p->tcScanRec;
  c_scanRecordPool.getPtr(scanptr);
  scanptr.p->scanState = ScanRecord::WAIT_CLOSE_SCAN;
  signal->theData[0] = scanptr.p->scanAccPtr;
  signal->theData[1] = RNIL;
  signal->theData[2] = NextScanReq::ZSCAN_CLOSE;
  sendSignal(scanptr.p->scanBlockref, GSN_NEXT_SCANREQ, signal, 3, JBB);
}//Dblqh::continueCloseScanAfterBlockedLab()

/* ------------------------------------------------------------------------- 
 *       ENTER NEXT_SCANCONF
 * -------------------------------------------------------------------------
 *       PRECONDITION: SCAN_STATE = WAIT_CLOSE_SCAN
 * ------------------------------------------------------------------------- */
void Dblqh::accScanCloseConfLab(Signal* signal) 
{
  tcConnectptr.i = scanptr.p->scanTcrec;
  ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);

  if((tcConnectptr.p->primKeyLen - 4) > 0 && 
    scanptr.p->scanCompletedStatus != ZTRUE)
  {
    jam();
    releaseActiveFrag(signal);
    continueAfterReceivingAllAiLab(signal);
    return;
  }
  
  scanptr.p->scanState = ScanRecord::WAIT_DELETE_STORED_PROC_ID_SCAN;
  signal->theData[0] = tcConnectptr.p->tupConnectrec;
  signal->theData[1] = tcConnectptr.p->tableref;
  signal->theData[2] = scanptr.p->scanSchemaVersion;
  signal->theData[3] = ZDELETE_STORED_PROC_ID;
  signal->theData[4] = scanptr.p->scanStoredProcId;
  sendSignal(tcConnectptr.p->tcTupBlockref,
             GSN_STORED_PROCREQ, signal, 5, JBB);
}//Dblqh::accScanCloseConfLab()

/* -------------------------------------------------------------------------
 *       ENTER STORED_PROCCONF WITH
 * -------------------------------------------------------------------------
 * PRECONDITION: SCAN_STATE = WAIT_DELETE_STORED_PROC_ID_SCAN
 * ------------------------------------------------------------------------- */
void Dblqh::tupScanCloseConfLab(Signal* signal) 
{
  fragptr.i = tcConnectptr.p->fragmentptr;
  ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
  if (tcConnectptr.p->abortState == TcConnectionrec::NEW_FROM_TC) {
    jam();
    tcNodeFailptr.i = tcConnectptr.p->tcNodeFailrec;
    ptrCheckGuard(tcNodeFailptr, ctcNodeFailrecFileSize, tcNodeFailRecord);
    tcNodeFailptr.p->tcRecNow = tcConnectptr.i + 1;
    signal->theData[0] = ZLQH_TRANS_NEXT;
    signal->theData[1] = tcNodeFailptr.i;
    sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
  } else if (tcConnectptr.p->errorCode != 0) {
    jam();
    ScanFragRef * ref = (ScanFragRef*)&signal->theData[0];
    ref->senderData = tcConnectptr.p->clientConnectrec;
    ref->transId1 = tcConnectptr.p->transid[0];
    ref->transId2 = tcConnectptr.p->transid[1];
    ref->errorCode = tcConnectptr.p->errorCode; 
    sendSignal(tcConnectptr.p->clientBlockref, GSN_SCAN_FRAGREF, signal, 
	 ScanFragRef::SignalLength, JBB);
  } else {
    jam();
    sendScanFragConf(signal, ZSCAN_FRAG_CLOSED);
  }//if
  finishScanrec(signal);
  releaseScanrec(signal);
  tcConnectptr.p->tcScanRec = RNIL;
  deleteTransidHash(signal);
  releaseOprec(signal);
  releaseTcrec(signal, tcConnectptr);
}//Dblqh::tupScanCloseConfLab()

/* ========================================================================= 
 * =======              INITIATE SCAN RECORD                         ======= 
 *
 *       SUBROUTINE SHORT NAME = ISC
 * ========================================================================= */
Uint32 Dblqh::initScanrec(const ScanFragReq* scanFragReq)
{
  const Uint32 reqinfo = scanFragReq->requestInfo;
  const Uint32 max_rows = scanFragReq->batch_size_rows;
  const Uint32 max_bytes = scanFragReq->batch_size_bytes;
  const Uint32 scanLockMode = ScanFragReq::getLockMode(reqinfo);
  const Uint32 scanLockHold = ScanFragReq::getHoldLockFlag(reqinfo);
  const Uint32 keyinfo = ScanFragReq::getKeyinfoFlag(reqinfo);
  const Uint32 readCommitted = ScanFragReq::getReadCommittedFlag(reqinfo);
  const Uint32 rangeScan = ScanFragReq::getRangeScanFlag(reqinfo);
  const Uint32 descending = ScanFragReq::getDescendingFlag(reqinfo);
  const Uint32 tupScan = ScanFragReq::getTupScanFlag(reqinfo);
  const Uint32 attrLen = ScanFragReq::getAttrLen(reqinfo);
  const Uint32 scanPrio = ScanFragReq::getScanPrio(reqinfo);

  scanptr.p->scanKeyinfoFlag = keyinfo;
  scanptr.p->scanLockHold = scanLockHold;
  scanptr.p->scanCompletedStatus = ZFALSE;
  scanptr.p->scanType = ScanRecord::SCAN;
  scanptr.p->scanApiBlockref = scanFragReq->resultRef;
  scanptr.p->scanAiLength = attrLen;
  scanptr.p->scanTcrec = tcConnectptr.i;
  scanptr.p->scanSchemaVersion = scanFragReq->schemaVersion;

  scanptr.p->m_curr_batch_size_rows = 0;
  scanptr.p->m_curr_batch_size_bytes= 0;
  scanptr.p->m_max_batch_size_rows = max_rows;
  scanptr.p->m_max_batch_size_bytes = max_bytes;

  if (! rangeScan && ! tupScan)
    scanptr.p->scanBlockref = tcConnectptr.p->tcAccBlockref;
  else if (! tupScan)
    scanptr.p->scanBlockref = tcConnectptr.p->tcTuxBlockref;
  else
    scanptr.p->scanBlockref = tcConnectptr.p->tcTupBlockref;

  scanptr.p->scanErrorCounter = 0;
  scanptr.p->scanLockMode = scanLockMode;
  scanptr.p->readCommitted = readCommitted;
  scanptr.p->rangeScan = rangeScan;
  scanptr.p->descending = descending;
  scanptr.p->tupScan = tupScan;
  scanptr.p->scanState = ScanRecord::SCAN_FREE;
  scanptr.p->scanFlag = ZFALSE;
  scanptr.p->scanLocalref[0] = 0;
  scanptr.p->scanLocalref[1] = 0;
  scanptr.p->scanLocalFragid = 0;
  scanptr.p->scanTcWaiting = ZTRUE;
  scanptr.p->scanNumber = ~0;
  scanptr.p->scanApiOpPtr = scanFragReq->clientOpPtr;
  scanptr.p->m_last_row = 0;
  scanptr.p->scanStoredProcId = RNIL;

  if (max_rows == 0 || (max_bytes > 0 && max_rows > max_bytes)){
    jam();
    return ScanFragRef::ZWRONG_BATCH_SIZE;
  }
  if (!seize_acc_ptr_list(scanptr.p, max_rows)){
    jam();
    return ScanFragRef::ZTOO_MANY_ACTIVE_SCAN_ERROR;
  }
  /**
   * Used for scan take over
   */
  FragrecordPtr tFragPtr;
  tFragPtr.i = fragptr.p->tableFragptr;
  ptrCheckGuard(tFragPtr, cfragrecFileSize, fragrecord);
  scanptr.p->fragPtrI = fragptr.p->tableFragptr;
  
  /**
   * !idx uses 1 - (MAX_PARALLEL_SCANS_PER_FRAG - 1)  =  1-11
   *  idx uses from MAX_PARALLEL_SCANS_PER_FRAG - MAX = 12-42)
   */
  Uint32 start = (rangeScan || tupScan ? MAX_PARALLEL_SCANS_PER_FRAG : 1 );
  Uint32 stop = (rangeScan || tupScan ? MAX_PARALLEL_INDEX_SCANS_PER_FRAG : MAX_PARALLEL_SCANS_PER_FRAG - 1);
  stop += start;
  Uint32 free = tFragPtr.p->m_scanNumberMask.find(start);
    
  if(free == Fragrecord::ScanNumberMask::NotFound || free >= stop){
    jam();
    
    if(scanPrio == 0){
      jam();
      return ScanFragRef::ZTOO_MANY_ACTIVE_SCAN_ERROR;
    }
    
    /**
     * Put on queue
     */
    scanptr.p->scanState = ScanRecord::IN_QUEUE;
    LocalDLFifoList<ScanRecord> queue(c_scanRecordPool,
				      fragptr.p->m_queuedScans);
    queue.add(scanptr);
    return ZOK;
  }
  
  scanptr.p->scanNumber = free;
  tFragPtr.p->m_scanNumberMask.clear(free);// Update mask  
  
  LocalDLList<ScanRecord> active(c_scanRecordPool, fragptr.p->m_activeScans);
  active.add(scanptr);
  if(scanptr.p->scanKeyinfoFlag){
    jam();
#ifdef VM_TRACE
    ScanRecordPtr tmp;
    ndbrequire(!c_scanTakeOverHash.find(tmp, * scanptr.p));
#endif
#ifdef TRACE_SCAN_TAKEOVER
    ndbout_c("adding (%d %d) table: %d fragId: %d frag.i: %d tableFragptr: %d",
	     scanptr.p->scanNumber, scanptr.p->fragPtrI,
	     tabptr.i, scanFragReq->fragmentNoKeyLen & 0xFFFF, 
	     fragptr.i, fragptr.p->tableFragptr);
#endif
    c_scanTakeOverHash.add(scanptr);
  }
  init_acc_ptr_list(scanptr.p);
  return ZOK;
}

/* =========================================================================
 * =======             INITIATE TC RECORD AT SCAN                    =======
 *
 *       SUBROUTINE SHORT NAME = IST
 * ========================================================================= */
void Dblqh::initScanTc(Signal* signal,
                       Uint32 transid1,
                       Uint32 transid2,
                       Uint32 fragId,
                       Uint32 nodeId) 
{
  tcConnectptr.p->transid[0] = transid1;
  tcConnectptr.p->transid[1] = transid2;
  tcConnectptr.p->tcScanRec = scanptr.i;
  tcConnectptr.p->tableref = tabptr.i;
  tcConnectptr.p->fragmentid = fragId;
  tcConnectptr.p->fragmentptr = fragptr.i;
  tcConnectptr.p->tcOprec = tcConnectptr.p->clientConnectrec;
  tcConnectptr.p->tcBlockref = tcConnectptr.p->clientBlockref;
  tcConnectptr.p->errorCode = 0;
  tcConnectptr.p->reclenAiLqhkey = 0;
  tcConnectptr.p->abortState = TcConnectionrec::ABORT_IDLE;
  tcConnectptr.p->nextReplica = nodeId;
  tcConnectptr.p->currTupAiLen = 0;
  tcConnectptr.p->opExec = 1;
  tcConnectptr.p->operation = ZREAD;
  tcConnectptr.p->listState = TcConnectionrec::NOT_IN_LIST;
  tcConnectptr.p->commitAckMarker = RNIL;
  tcConnectptr.p->m_offset_current_keybuf = 0;
  tcConnectptr.p->m_scan_curr_range_no = 0;

  tabptr.p->usageCount++;
}//Dblqh::initScanTc()

/* ========================================================================= 
 * =======                       FINISH  SCAN RECORD                 ======= 
 * 
 *       REMOVE SCAN RECORD FROM PER FRAGMENT LIST.
 * ========================================================================= */
void Dblqh::finishScanrec(Signal* signal)
{
  release_acc_ptr_list(scanptr.p);

  LocalDLFifoList<ScanRecord> queue(c_scanRecordPool,
				    fragptr.p->m_queuedScans);
  
  if(scanptr.p->scanState == ScanRecord::IN_QUEUE){
    jam();
    queue.release(scanptr);
    return;
  }

  if(scanptr.p->scanKeyinfoFlag){
    jam();
    ScanRecordPtr tmp;
#ifdef TRACE_SCAN_TAKEOVER
    ndbout_c("removing (%d %d)", scanptr.p->scanNumber, scanptr.p->fragPtrI);
#endif
    c_scanTakeOverHash.remove(tmp, * scanptr.p);
    ndbrequire(tmp.p == scanptr.p);
  }
  
  LocalDLList<ScanRecord> scans(c_scanRecordPool, fragptr.p->m_activeScans);
  scans.release(scanptr);
  
  FragrecordPtr tFragPtr;
  tFragPtr.i = scanptr.p->fragPtrI;
  ptrCheckGuard(tFragPtr, cfragrecFileSize, fragrecord);

  const Uint32 scanNumber = scanptr.p->scanNumber;
  ndbrequire(!tFragPtr.p->m_scanNumberMask.get(scanNumber));
  ScanRecordPtr restart;

  /**
   * Start on of queued scans
   */
  if(scanNumber == NR_ScanNo || !queue.first(restart)){
    jam();
    tFragPtr.p->m_scanNumberMask.set(scanNumber);
    return;
  }

  if(ERROR_INSERTED(5034)){
    jam();
    tFragPtr.p->m_scanNumberMask.set(scanNumber);
    return;
  }

  ndbrequire(restart.p->scanState == ScanRecord::IN_QUEUE);

  ScanRecordPtr tmpScan = scanptr;
  TcConnectionrecPtr tmpTc = tcConnectptr;
  
  tcConnectptr.i = restart.p->scanTcrec;
  ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
  restart.p->scanNumber = scanNumber;

  queue.remove(restart);
  scans.add(restart);
  if(restart.p->scanKeyinfoFlag){
    jam();
#ifdef VM_TRACE
    ScanRecordPtr tmp;
    ndbrequire(!c_scanTakeOverHash.find(tmp, * restart.p));
#endif
    c_scanTakeOverHash.add(restart);
#ifdef TRACE_SCAN_TAKEOVER
    ndbout_c("adding-r (%d %d)", restart.p->scanNumber, restart.p->fragPtrI);
#endif
  }

  restart.p->scanState = ScanRecord::SCAN_FREE; // set in initScanRec
  if(tcConnectptr.p->transactionState == TcConnectionrec::SCAN_STATE_USED)
  {
    jam();
    scanptr = restart;
    continueAfterReceivingAllAiLab(signal);  
  }
  else
  {
    ndbrequire(tcConnectptr.p->transactionState == TcConnectionrec::WAIT_SCAN_AI);
  }
  scanptr = tmpScan;
  tcConnectptr = tmpTc;
}//Dblqh::finishScanrec()

/* ========================================================================= 
 * =======                       RELEASE SCAN RECORD                 ======= 
 * 
 *       RELEASE A SCAN RECORD TO THE FREELIST.
 * ========================================================================= */
void Dblqh::releaseScanrec(Signal* signal) 
{
  scanptr.p->scanState = ScanRecord::SCAN_FREE;
  scanptr.p->scanType = ScanRecord::ST_IDLE;
  scanptr.p->scanTcWaiting = ZFALSE;
  cbookedAccOps -= scanptr.p->m_max_batch_size_rows;
  cscanNoFreeRec++;
}//Dblqh::releaseScanrec()

/* ------------------------------------------------------------------------
 * -------              SEND KEYINFO20 TO API                       ------- 
 *
 * ------------------------------------------------------------------------  */
Uint32 Dblqh::sendKeyinfo20(Signal* signal, 
			    ScanRecord * scanP, 
			    TcConnectionrec * tcConP)
{
  ndbrequire(scanP->m_curr_batch_size_rows < MAX_PARALLEL_OP_PER_SCAN);
  KeyInfo20 * keyInfo = (KeyInfo20 *)&signal->theData[0];
  
  /**
   * Note that this code requires signal->theData to be big enough for
   * a entire key
   */
  const BlockReference ref = scanP->scanApiBlockref;
  const Uint32 scanOp = scanP->m_curr_batch_size_rows;
  const Uint32 nodeId = refToNode(ref);
  const bool connectedToNode = getNodeInfo(nodeId).m_connected;
  const Uint32 type = getNodeInfo(nodeId).m_type;
  const bool is_api = (type >= NodeInfo::API && type <= NodeInfo::REP);
  const bool old_dest = (getNodeInfo(nodeId).m_version < MAKE_VERSION(3,5,0));
  const bool longable = true; // TODO is_api && !old_dest;

  Uint32 * dst = keyInfo->keyData;
  dst += nodeId == getOwnNodeId() ? 0 : KeyInfo20::DataLength;

  Uint32 keyLen = readPrimaryKeys(scanP, tcConP, dst);
  Uint32 fragId = tcConP->fragmentid;
  keyInfo->clientOpPtr   = scanP->scanApiOpPtr;
  keyInfo->keyLen        = keyLen;
  keyInfo->scanInfo_Node = 
    KeyInfo20::setScanInfo(scanOp, scanP->scanNumber) + (fragId << 20);
  keyInfo->transId1 = tcConP->transid[0];
  keyInfo->transId2 = tcConP->transid[1];
  
  Uint32 * src = signal->theData+25;
  if(connectedToNode){
    jam();
    
    if(nodeId != getOwnNodeId()){
      jam();
      
      if(keyLen <= KeyInfo20::DataLength || !longable) {
	while(keyLen > KeyInfo20::DataLength){
	  jam();
	  MEMCOPY_NO_WORDS(keyInfo->keyData, src, KeyInfo20::DataLength);
	  sendSignal(ref, GSN_KEYINFO20, signal, 25, JBB);
	  src += KeyInfo20::DataLength;;
	  keyLen -= KeyInfo20::DataLength;
	}
	
	MEMCOPY_NO_WORDS(keyInfo->keyData, src, keyLen);
	sendSignal(ref, GSN_KEYINFO20, signal, 
		   KeyInfo20::HeaderLength+keyLen, JBB);
	return keyLen;
      }
      
      LinearSectionPtr ptr[3];
      ptr[0].p = src;
      ptr[0].sz = keyLen;
      sendSignal(ref, GSN_KEYINFO20, signal, KeyInfo20::HeaderLength, 
		 JBB, ptr, 1);
      return keyLen;
    }
    
    EXECUTE_DIRECT(refToBlock(ref), GSN_KEYINFO20, signal, 
		   KeyInfo20::HeaderLength + keyLen);
    jamEntry();
    return keyLen;
  }
  
  /** 
   * If this node does not have a direct connection 
   * to the receiving node we want to send the signals 
   * routed via the node that controls this read
   */
  Uint32 routeBlockref = tcConP->clientBlockref;
  
  if(keyLen < KeyInfo20::DataLength || !longable){
    jam();
    
    while (keyLen > (KeyInfo20::DataLength - 1)) {
      jam();      
      MEMCOPY_NO_WORDS(keyInfo->keyData, src, KeyInfo20::DataLength - 1);
      keyInfo->keyData[KeyInfo20::DataLength-1] = ref;
      sendSignal(routeBlockref, GSN_KEYINFO20_R, signal, 25, JBB);
      src += KeyInfo20::DataLength - 1;
      keyLen -= KeyInfo20::DataLength - 1;
    }

    MEMCOPY_NO_WORDS(keyInfo->keyData, src, keyLen);
    keyInfo->keyData[keyLen] = ref;  
    sendSignal(routeBlockref, GSN_KEYINFO20_R, signal, 
	       KeyInfo20::HeaderLength+keyLen+1, JBB);    
    return keyLen;
  }

  keyInfo->keyData[0] = ref;
  LinearSectionPtr ptr[3];
  ptr[0].p = src;
  ptr[0].sz = keyLen;
  sendSignal(routeBlockref, GSN_KEYINFO20_R, signal, 
	     KeyInfo20::HeaderLength+1, JBB, ptr, 1);
  return keyLen;
}
  
/* ------------------------------------------------------------------------
 * -------        SEND SCAN_FRAGCONF TO TC THAT CONTROLS THE SCAN   ------- 
 *
 * ------------------------------------------------------------------------ */
void Dblqh::sendScanFragConf(Signal* signal, Uint32 scanCompleted) 
{
  Uint32 completed_ops= scanptr.p->m_curr_batch_size_rows;
  Uint32 total_len= scanptr.p->m_curr_batch_size_bytes;
  scanptr.p->scanTcWaiting = ZFALSE;

  if(ERROR_INSERTED(5037)){
    CLEAR_ERROR_INSERT_VALUE;
    return;
  }
  ScanFragConf * conf = (ScanFragConf*)&signal->theData[0];
  NodeId tc_node_id= refToNode(tcConnectptr.p->clientBlockref);
  Uint32 trans_id1= tcConnectptr.p->transid[0];
  Uint32 trans_id2= tcConnectptr.p->transid[1];

  conf->senderData = tcConnectptr.p->clientConnectrec;
  conf->completedOps = completed_ops;
  conf->fragmentCompleted = scanCompleted;
  conf->transId1 = trans_id1;
  conf->transId2 = trans_id2;
  conf->total_len= total_len;
  sendSignal(tcConnectptr.p->clientBlockref, GSN_SCAN_FRAGCONF, 
             signal, ScanFragConf::SignalLength, JBB);
  
  if(!scanptr.p->scanLockHold)
  {
    jam();
    scanptr.p->m_curr_batch_size_rows = 0;
    scanptr.p->m_curr_batch_size_bytes= 0;
  }
}//Dblqh::sendScanFragConf()

/* ######################################################################### */
/* #######                NODE RECOVERY MODULE                       ####### */
/*                                                                           */
/* ######################################################################### */
/*---------------------------------------------------------------------------*/
/*                                                                           */
/*   THIS MODULE IS USED WHEN A NODE HAS FAILED. IT PERFORMS A COPY OF A     */
/*   FRAGMENT TO A NEW REPLICA OF THE FRAGMENT. IT DOES ALSO SHUT DOWN ALL   */
/*   CONNECTIONS TO THE FAILED NODE.                                         */
/*---------------------------------------------------------------------------*/
Uint32 
Dblqh::calculateHash(Uint32 tableId, const Uint32* src) 
{
  jam();
  Uint64 Tmp[(MAX_KEY_SIZE_IN_WORDS*MAX_XFRM_MULTIPLY) >> 1];
  Uint32 keyPartLen[MAX_ATTRIBUTES_IN_INDEX];
  Uint32 keyLen = xfrm_key(tableId, src, (Uint32*)Tmp, sizeof(Tmp) >> 2, 
			   keyPartLen);
  ndbrequire(keyLen);
  
  return md5_hash(Tmp, keyLen);
}//Dblqh::calculateHash()

/* *************************************** */
/*  COPY_FRAGREQ: Start copying a fragment */
/* *************************************** */
void Dblqh::execCOPY_FRAGREQ(Signal* signal) 
{
  jamEntry();
  const CopyFragReq * const copyFragReq = (CopyFragReq *)&signal->theData[0];
  tabptr.i = copyFragReq->tableId;
  ptrCheckGuard(tabptr, ctabrecFileSize, tablerec);
  const Uint32 fragId = copyFragReq->fragId;
  const Uint32 copyPtr = copyFragReq->userPtr;
  const Uint32 userRef = copyFragReq->userRef;
  const Uint32 nodeId = copyFragReq->nodeId;

  ndbrequire(cnoActiveCopy < 3);
  ndbrequire(getFragmentrec(signal, fragId));
  ndbrequire(fragptr.p->copyFragState == ZIDLE);
  ndbrequire(cfirstfreeTcConrec != RNIL);
  ndbrequire(fragptr.p->m_scanNumberMask.get(NR_ScanNo));

  fragptr.p->fragDistributionKey = copyFragReq->distributionKey;

  if (DictTabInfo::isOrderedIndex(tabptr.p->tableType)) {
    jam();
    /**
     * Ordered index doesn't need to be copied
     */
    CopyFragConf * const conf = (CopyFragConf *)&signal->theData[0];
    conf->userPtr = copyPtr;
    conf->sendingNodeId = cownNodeid;
    conf->startingNodeId = nodeId;
    conf->tableId = tabptr.i;
    conf->fragId = fragId;
    sendSignal(userRef, GSN_COPY_FRAGCONF, signal,
	       CopyFragConf::SignalLength, JBB);
    return;
  }//if
  
  LocalDLList<ScanRecord> scans(c_scanRecordPool, fragptr.p->m_activeScans);
  ndbrequire(scans.seize(scanptr));
/* ------------------------------------------------------------------------- */
// We keep track of how many operation records in ACC that has been booked.
// Copy fragment has records always booked and thus need not book any. The
// most operations in parallel use is the m_max_batch_size_rows.
// This variable has to be set-up here since it is used by releaseScanrec
// to unbook operation records in ACC.
/* ------------------------------------------------------------------------- */
  scanptr.p->m_max_batch_size_rows = 0;
  scanptr.p->rangeScan = 0;
  scanptr.p->tupScan = 0;
  seizeTcrec();
  
  /**
   * Remove implicit cast/usage of CopyFragReq
   */
  //initCopyrec(signal);
  scanptr.p->copyPtr = copyPtr;
  scanptr.p->scanType = ScanRecord::COPY;
  scanptr.p->scanApiBlockref = userRef;
  scanptr.p->scanNodeId = nodeId;
  scanptr.p->scanTcrec = tcConnectptr.i;
  scanptr.p->scanSchemaVersion = copyFragReq->schemaVersion;
  scanptr.p->scanCompletedStatus = ZFALSE;
  scanptr.p->scanErrorCounter = 0;
  scanptr.p->scanNumber = NR_ScanNo;
  scanptr.p->scanKeyinfoFlag = 0; // Don't put into hash
  scanptr.p->fragPtrI = fragptr.i;
  fragptr.p->m_scanNumberMask.clear(NR_ScanNo);
  scanptr.p->scanBlockref = DBACC_REF;

  initScanTc(signal,
             0,
             (DBLQH << 20) + (cownNodeid << 8),
             fragId,
             copyFragReq->nodeId);
  cactiveCopy[cnoActiveCopy] = fragptr.i;
  cnoActiveCopy++;

  tcConnectptr.p->copyCountWords = 0;
  tcConnectptr.p->tcOprec = tcConnectptr.i;
  tcConnectptr.p->schemaVersion = scanptr.p->scanSchemaVersion;
  scanptr.p->scanState = ScanRecord::WAIT_ACC_COPY;
  AccScanReq * req = (AccScanReq*)&signal->theData[0];
  req->senderData = scanptr.i;
  req->senderRef = cownref;
  req->tableId = tabptr.i;
  req->fragmentNo = fragId;
  req->requestInfo = 0;
  AccScanReq::setLockMode(req->requestInfo, 0);
  AccScanReq::setReadCommittedFlag(req->requestInfo, 0);
  req->transId1 = tcConnectptr.p->transid[0];
  req->transId2 = tcConnectptr.p->transid[1];
  req->savePointId = tcConnectptr.p->savePointId;
  sendSignal(tcConnectptr.p->tcAccBlockref, GSN_ACC_SCANREQ, signal, 
	     AccScanReq::SignalLength, JBB);
  return;
}//Dblqh::execCOPY_FRAGREQ()

void Dblqh::accScanConfCopyLab(Signal* signal) 
{
  AccScanConf * const accScanConf = (AccScanConf *)&signal->theData[0];
  tcConnectptr.i = scanptr.p->scanTcrec;
  ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
/*--------------------------------------------------------------------------*/
/*  PRECONDITION: SCAN_STATE = WAIT_ACC_COPY                                */
/*--------------------------------------------------------------------------*/
  if (accScanConf->flag == AccScanConf::ZEMPTY_FRAGMENT) {
    jam();
/*---------------------------------------------------------------------------*/
/*   THE FRAGMENT WAS EMPTY.                                                 */
/*   REPORT SUCCESSFUL COPYING.                                              */
/*---------------------------------------------------------------------------*/
    tupCopyCloseConfLab(signal);
    return;
  }//if
  scanptr.p->scanAccPtr = accScanConf->accPtr;
  scanptr.p->scanState = ScanRecord::WAIT_STORED_PROC_COPY;
  signal->theData[0] = tcConnectptr.p->tupConnectrec;
  signal->theData[1] = tcConnectptr.p->tableref;
  signal->theData[2] = scanptr.p->scanSchemaVersion;
  signal->theData[3] = ZSTORED_PROC_COPY;
// theData[4] is not used in TUP with ZSTORED_PROC_COPY
  sendSignal(tcConnectptr.p->tcTupBlockref, GSN_STORED_PROCREQ, signal, 5, JBB);
  return;
}//Dblqh::accScanConfCopyLab()

/*---------------------------------------------------------------------------*/
/*   ENTER STORED_PROCCONF WITH                                              */
/*     TC_CONNECTPTR,                                                        */
/*     TSTORED_PROC_ID                                                       */
/*---------------------------------------------------------------------------*/
void Dblqh::storedProcConfCopyLab(Signal* signal) 
{
/*---------------------------------------------------------------------------*/
/*   PRECONDITION: SCAN_STATE = WAIT_STORED_PROC_COPY                        */
/*---------------------------------------------------------------------------*/
  fragptr.i = tcConnectptr.p->fragmentptr;
  ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
  if (scanptr.p->scanCompletedStatus == ZTRUE) {
    jam();
/*---------------------------------------------------------------------------*/
/*   THE COPY PROCESS HAVE BEEN COMPLETED, MOST LIKELY DUE TO A NODE FAILURE.*/
/*---------------------------------------------------------------------------*/
    closeCopyLab(signal);
    return;
  }//if
  scanptr.i = tcConnectptr.p->tcScanRec;
  c_scanRecordPool.getPtr(scanptr);
  scanptr.p->scanState = ScanRecord::WAIT_NEXT_SCAN_COPY;
  switch (fragptr.p->fragStatus) {
  case Fragrecord::FSACTIVE:
    jam();
    linkActiveFrag(signal);
    break;
  case Fragrecord::BLOCKED:
    jam();
    linkFragQueue(signal);
    tcConnectptr.p->transactionState = TcConnectionrec::COPY_FIRST_STOPPED;
    return;
    break;
  case Fragrecord::FREE:
    jam();
  case Fragrecord::ACTIVE_CREATION:
    jam();
  case Fragrecord::CRASH_RECOVERING:
    jam();
  case Fragrecord::DEFINED:
    jam();
  case Fragrecord::REMOVING:
    jam();
  default:
    jam();
    systemErrorLab(signal);
    return;
    break;
  }//switch
  continueFirstCopyAfterBlockedLab(signal);
  return;
}//Dblqh::storedProcConfCopyLab()

void Dblqh::continueFirstCopyAfterBlockedLab(Signal* signal) 
{
  scanptr.i = tcConnectptr.p->tcScanRec;
  c_scanRecordPool.getPtr(scanptr);
  signal->theData[0] = scanptr.p->scanAccPtr;
  signal->theData[1] = RNIL;
  signal->theData[2] = NextScanReq::ZSCAN_NEXT;
  sendSignal(tcConnectptr.p->tcAccBlockref, GSN_NEXT_SCANREQ, signal, 3, JBB);
  return;
}//Dblqh::continueFirstCopyAfterBlockedLab()

/*---------------------------------------------------------------------------*/
/*       ENTER NEXT_SCANCONF WITH                                            */
/*         SCANPTR,                                                          */
/*         TFRAGID,                                                          */
/*         TACC_OPPTR,                                                       */
/*         TLOCAL_KEY1,                                                      */
/*         TLOCAL_KEY2,                                                      */
/*         TKEY_LENGTH,                                                      */
/*         TKEY1,                                                            */
/*         TKEY2,                                                            */
/*         TKEY3,                                                            */
/*         TKEY4                                                             */
/*---------------------------------------------------------------------------*/
/*       PRECONDITION: SCAN_STATE = WAIT_NEXT_SCAN_COPY                      */
/*---------------------------------------------------------------------------*/
void Dblqh::nextScanConfCopyLab(Signal* signal) 
{
  NextScanConf * const nextScanConf = (NextScanConf *)&signal->theData[0];
  tcConnectptr.i = scanptr.p->scanTcrec;
  ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
  if (nextScanConf->fragId == RNIL) {
    jam();
/*---------------------------------------------------------------------------*/
/*   THERE ARE NO MORE TUPLES TO FETCH. WE NEED TO CLOSE                     */
/*   THE COPY IN ACC AND DELETE THE STORED PROCEDURE IN TUP                  */
/*---------------------------------------------------------------------------*/
    releaseActiveFrag(signal);
    if (tcConnectptr.p->copyCountWords == 0) {
      closeCopyLab(signal);
      return;
    }//if
/*---------------------------------------------------------------------------*/
// Wait until copying is completed also at the starting node before reporting
// completion. Signal completion through scanCompletedStatus-flag.
/*---------------------------------------------------------------------------*/
    scanptr.p->scanCompletedStatus = ZTRUE;
    return;
  }//if

  // If accOperationPtr == RNIL no record was returned by ACC
  if (nextScanConf->accOperationPtr == RNIL) {
    jam();
    signal->theData[0] = scanptr.p->scanAccPtr;
    signal->theData[1] = AccCheckScan::ZCHECK_LCP_STOP;
    sendSignal(tcConnectptr.p->tcAccBlockref, GSN_ACC_CHECK_SCAN, signal, 2, JBB);
    return;      
  }

  set_acc_ptr_in_scan_record(scanptr.p, 0, nextScanConf->accOperationPtr);
  initCopyTc(signal);
  copySendTupkeyReqLab(signal);
  return;
}//Dblqh::nextScanConfCopyLab()

void Dblqh::copySendTupkeyReqLab(Signal* signal) 
{
  Uint32 reqinfo = 0;
  Uint32 tupFragPtr;

  reqinfo = reqinfo + (tcConnectptr.p->operation << 6);
  reqinfo = reqinfo + (tcConnectptr.p->opExec << 10);
  tcConnectptr.p->transactionState = TcConnectionrec::COPY_TUPKEY;
  scanptr.p->scanState = ScanRecord::WAIT_TUPKEY_COPY;
  fragptr.i = tcConnectptr.p->fragmentptr;
  ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
  tupFragPtr = fragptr.p->tupFragptr[scanptr.p->scanLocalFragid & 1];
  {
    TupKeyReq * const tupKeyReq = (TupKeyReq *)signal->getDataPtrSend(); 

    tupKeyReq->connectPtr = tcConnectptr.p->tupConnectrec;
    tupKeyReq->request = reqinfo;
    tupKeyReq->tableRef = tcConnectptr.p->tableref;
    tupKeyReq->fragId = scanptr.p->scanLocalFragid;
    tupKeyReq->keyRef1 = scanptr.p->scanLocalref[0];
    tupKeyReq->keyRef2 = scanptr.p->scanLocalref[1];
    tupKeyReq->attrBufLen = 0;
    tupKeyReq->opRef = tcConnectptr.i;
    tupKeyReq->applRef = cownref;
    tupKeyReq->schemaVersion = scanptr.p->scanSchemaVersion;
    tupKeyReq->storedProcedure = scanptr.p->scanStoredProcId;
    tupKeyReq->transId1 = tcConnectptr.p->transid[0];
    tupKeyReq->transId2 = tcConnectptr.p->transid[1];
    tupKeyReq->fragPtr = tupFragPtr;
    tupKeyReq->primaryReplica = (tcConnectptr.p->seqNoReplica == 0)?true:false;
    tupKeyReq->coordinatorTC = tcConnectptr.p->tcBlockref;
    tupKeyReq->tcOpIndex = tcConnectptr.p->tcOprec;
    tupKeyReq->savePointId = tcConnectptr.p->savePointId;
    Uint32 blockNo = refToBlock(tcConnectptr.p->tcTupBlockref);
    EXECUTE_DIRECT(blockNo, GSN_TUPKEYREQ, signal, 
               TupKeyReq::SignalLength);
  }
}//Dblqh::copySendTupkeyReqLab()

/*---------------------------------------------------------------------------*/
/*   USED IN COPYING OPERATION TO RECEIVE ATTRINFO FROM TUP.                 */
/*---------------------------------------------------------------------------*/
/* ************>> */
/*  TRANSID_AI  > */
/* ************>> */
void Dblqh::execTRANSID_AI(Signal* signal) 
{
  jamEntry();
  tcConnectptr.i = signal->theData[0];
  ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
  Uint32 length = signal->length() - 3;
  ndbrequire(tcConnectptr.p->transactionState == TcConnectionrec::COPY_TUPKEY);
  Uint32 * src = &signal->theData[3];
  while(length > 22){
    if (saveTupattrbuf(signal, src, 22) == ZOK) {
      ;
    } else {
      jam();
      tcConnectptr.p->errorCode = ZGET_ATTRINBUF_ERROR;
      return;
    }//if
    src += 22;
    length -= 22;
  }
  if (saveTupattrbuf(signal, src, length) == ZOK) {
    return;
  }
  jam();
  tcConnectptr.p->errorCode = ZGET_ATTRINBUF_ERROR;
}//Dblqh::execTRANSID_AI()

/*--------------------------------------------------------------------------*/
/*     ENTER TUPKEYCONF WITH                                                */
/*          TC_CONNECTPTR,                                                  */
/*          TDATA2,                                                         */
/*          TDATA3,                                                         */
/*          TDATA4,                                                         */
/*          TDATA5                                                          */
/*--------------------------------------------------------------------------*/
/*  PRECONDITION:   TRANSACTION_STATE = COPY_TUPKEY                         */
/*--------------------------------------------------------------------------*/
void Dblqh::copyTupkeyConfLab(Signal* signal) 
{
  const TupKeyConf * const tupKeyConf = (TupKeyConf *)signal->getDataPtr();

  UintR readLength = tupKeyConf->readLength;
  Uint32 tableId = tcConnectptr.p->tableref;
  scanptr.i = tcConnectptr.p->tcScanRec;
  c_scanRecordPool.getPtr(scanptr);
  ScanRecord* scanP = scanptr.p;
  releaseActiveFrag(signal);
  if (tcConnectptr.p->errorCode != 0) {
    jam();
    closeCopyLab(signal);
    return;
  }//if
  if (scanptr.p->scanCompletedStatus == ZTRUE) {
    jam();
/*---------------------------------------------------------------------------*/
/*   THE COPY PROCESS HAVE BEEN CLOSED. MOST LIKELY A NODE FAILURE.          */
/*---------------------------------------------------------------------------*/
    closeCopyLab(signal);
    return;
  }//if
  TcConnectionrec * tcConP = tcConnectptr.p;
  tcConnectptr.p->totSendlenAi = readLength;
  tcConnectptr.p->connectState = TcConnectionrec::COPY_CONNECTED;

  // Read primary keys (used to get here via scan keyinfo)
  Uint32* tmp = signal->getDataPtrSend()+24;
  Uint32 len= tcConnectptr.p->primKeyLen = readPrimaryKeys(scanP, tcConP, tmp);
  
  // Calculate hash (no need to linearies key)
  if (g_key_descriptor_pool.getPtr(tableId)->hasCharAttr)
  {
    tcConnectptr.p->hashValue = calculateHash(tableId, tmp);
  }
  else
  {
    tcConnectptr.p->hashValue = md5_hash((Uint64*)tmp, len);
  }

  // Move into databuffer to make packLqhkeyreqLab happy
  memcpy(tcConP->tupkeyData, tmp, 4*4);
  if(len > 4)
    keyinfoLab(tmp+4, tmp + len);
  LqhKeyReq::setKeyLen(tcConP->reqinfo, len);

/*---------------------------------------------------------------------------*/
// To avoid using up to many operation records in ACC we will increase the
// constant to ensure that we never send more than 40 records at a time.
// This is where the constant 56 comes from. For long records this constant
// will not matter that much. The current maximum is 6000 words outstanding
// (including a number of those 56 words not really sent). We also have to
// ensure that there are never more simultaneous usage of these operation
// records to ensure that node recovery does not fail because of simultaneous
// scanning.
/*---------------------------------------------------------------------------*/
  UintR TnoOfWords = readLength + len;
  TnoOfWords = TnoOfWords + MAGIC_CONSTANT;
  TnoOfWords = TnoOfWords + (TnoOfWords >> 2);

  /*-----------------------------------------------------------------
   * NOTE for transid1!
   * Transid1 in the tcConnection record is used load regulate the 
   * copy(node recovery) process.
   * The number of outstanding words are written in the transid1 
   * variable. This will be sent to the starting node in the 
   * LQHKEYREQ signal and when the answer is returned in the LQHKEYCONF
   * we can reduce the number of outstanding words and check to see
   * if more LQHKEYREQ signals should be sent.
   * 
   * However efficient this method is rather unsafe in such way that
   * it overwrites the transid1 original data.
   *
   * Also see TR 587.
   *----------------------------------------------------------------*/
  tcConnectptr.p->transid[0] = TnoOfWords; // Data overload, see note!
  packLqhkeyreqLab(signal);
  tcConnectptr.p->copyCountWords += TnoOfWords;
  scanptr.p->scanState = ScanRecord::WAIT_LQHKEY_COPY;
  if (tcConnectptr.p->copyCountWords < cmaxWordsAtNodeRec) {
    nextRecordCopy(signal);
    return;
  }//if
  return;
}//Dblqh::copyTupkeyConfLab()

/*---------------------------------------------------------------------------*/
/*     ENTER LQHKEYCONF                                                      */
/*---------------------------------------------------------------------------*/
/*   PRECONDITION: CONNECT_STATE = COPY_CONNECTED                            */
/*---------------------------------------------------------------------------*/
void Dblqh::copyCompletedLab(Signal* signal) 
{
  const LqhKeyConf * const lqhKeyConf = (LqhKeyConf *)signal->getDataPtr();  

  ndbrequire(tcConnectptr.p->transid[1] == lqhKeyConf->transId2);
  scanptr.i = tcConnectptr.p->tcScanRec;
  c_scanRecordPool.getPtr(scanptr);
  if (tcConnectptr.p->copyCountWords >= cmaxWordsAtNodeRec) {
    tcConnectptr.p->copyCountWords -= lqhKeyConf->transId1; // Data overload, see note!
    if (scanptr.p->scanCompletedStatus == ZTRUE) {
      jam();
/*---------------------------------------------------------------------------*/
// Copy to complete, we will not start any new copying.
/*---------------------------------------------------------------------------*/
      closeCopyLab(signal);
      return;
    }//if
    if (tcConnectptr.p->copyCountWords < cmaxWordsAtNodeRec) {
      jam();
      nextRecordCopy(signal);
    }//if
    return;
  }//if
  tcConnectptr.p->copyCountWords -= lqhKeyConf->transId1; // Data overload, see note!
  ndbrequire(tcConnectptr.p->copyCountWords <= cmaxWordsAtNodeRec);
  if (tcConnectptr.p->copyCountWords > 0) {
    jam();
    return;
  }//if
/*---------------------------------------------------------------------------*/
// No more outstanding copies. We will only start new ones from here if it was
// stopped before and this only happens when copyCountWords is bigger than the
// threshold value. Since this did not occur we must be waiting for completion.
// Check that this is so. If not we crash to find out what is going on.
/*---------------------------------------------------------------------------*/
  if (scanptr.p->scanCompletedStatus == ZTRUE) {
    jam();
    closeCopyLab(signal);
    return;
  }//if
  if (scanptr.p->scanState == ScanRecord::WAIT_LQHKEY_COPY) {
    jam();
/*---------------------------------------------------------------------------*/
// Make sure that something is in progress. Otherwise we will simply stop
// and nothing more will happen.
/*---------------------------------------------------------------------------*/
    systemErrorLab(signal);
    return;
  }//if
  return;
}//Dblqh::copyCompletedLab()

void Dblqh::nextRecordCopy(Signal* signal)
{
  fragptr.i = tcConnectptr.p->fragmentptr;
  ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
  scanptr.i = tcConnectptr.p->tcScanRec;
  c_scanRecordPool.getPtr(scanptr);
  if (scanptr.p->scanState != ScanRecord::WAIT_LQHKEY_COPY) {
    jam();
/*---------------------------------------------------------------------------*/
// Make sure that nothing is in progress. Otherwise we will have to simultaneous
// scans on the same record and this will certainly lead to unexpected
// behaviour.
/*---------------------------------------------------------------------------*/
    systemErrorLab(signal);
    return;
  }//if
  scanptr.p->scanState = ScanRecord::WAIT_NEXT_SCAN_COPY;
  switch (fragptr.p->fragStatus) {
  case Fragrecord::FSACTIVE:
    jam();
    linkActiveFrag(signal);
    break;
  case Fragrecord::BLOCKED:
    jam();
    linkFragQueue(signal);
    tcConnectptr.p->transactionState = TcConnectionrec::COPY_STOPPED;
    return;
    break;
  case Fragrecord::FREE:
    jam();
  case Fragrecord::ACTIVE_CREATION:
    jam();
  case Fragrecord::CRASH_RECOVERING:
    jam();
  case Fragrecord::DEFINED:
    jam();
  case Fragrecord::REMOVING:
    jam();
  default:
    jam();
    systemErrorLab(signal);
    return;
    break;
  }//switch
  continueCopyAfterBlockedLab(signal);
  return;
}//Dblqh::nextRecordCopy()

void Dblqh::continueCopyAfterBlockedLab(Signal* signal) 
{
  scanptr.i = tcConnectptr.p->tcScanRec;
  c_scanRecordPool.getPtr(scanptr);
  tcConnectptr.p->errorCode = 0;
  Uint32 acc_op_ptr= get_acc_ptr_from_scan_record(scanptr.p, 0, false);
  signal->theData[0] = scanptr.p->scanAccPtr;
  signal->theData[1] = acc_op_ptr;
  signal->theData[2] = NextScanReq::ZSCAN_NEXT_COMMIT;
  sendSignal(tcConnectptr.p->tcAccBlockref, GSN_NEXT_SCANREQ, signal, 3, JBB);
  return;
}//Dblqh::continueCopyAfterBlockedLab()

void Dblqh::copyLqhKeyRefLab(Signal* signal) 
{
  ndbrequire(tcConnectptr.p->transid[1] == signal->theData[4]);
  tcConnectptr.p->copyCountWords -= signal->theData[3];
  scanptr.i = tcConnectptr.p->tcScanRec;
  c_scanRecordPool.getPtr(scanptr);
  scanptr.p->scanErrorCounter++;
  tcConnectptr.p->errorCode = terrorCode;
  closeCopyLab(signal);
  return;
}//Dblqh::copyLqhKeyRefLab()

void Dblqh::closeCopyLab(Signal* signal) 
{
  if (tcConnectptr.p->copyCountWords > 0) {
/*---------------------------------------------------------------------------*/
// We are still waiting for responses from the starting node.
// Wait until all of those have arrived until we start the
// close process.
/*---------------------------------------------------------------------------*/
    jam();
    return;
  }//if
  tcConnectptr.p->transid[0] = 0;
  tcConnectptr.p->transid[1] = 0;
  fragptr.i = tcConnectptr.p->fragmentptr;
  ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
  scanptr.i = tcConnectptr.p->tcScanRec;
  c_scanRecordPool.getPtr(scanptr);
  scanptr.p->scanState = ScanRecord::WAIT_CLOSE_COPY;
  switch (fragptr.p->fragStatus) {
  case Fragrecord::FSACTIVE:
    jam();
    linkActiveFrag(signal);
    break;
  case Fragrecord::BLOCKED:
    jam();
    linkFragQueue(signal);
    tcConnectptr.p->transactionState = TcConnectionrec::COPY_CLOSE_STOPPED;
    return;
    break;
  case Fragrecord::FREE:
    jam();
  case Fragrecord::ACTIVE_CREATION:
    jam();
  case Fragrecord::CRASH_RECOVERING:
    jam();
  case Fragrecord::DEFINED:
    jam();
  case Fragrecord::REMOVING:
    jam();
  default:
    jam();
    systemErrorLab(signal);
    return;
    break;
  }//switch
  continueCloseCopyAfterBlockedLab(signal);
  return;
}//Dblqh::closeCopyLab()

void Dblqh::continueCloseCopyAfterBlockedLab(Signal* signal) 
{
  scanptr.i = tcConnectptr.p->tcScanRec;
  c_scanRecordPool.getPtr(scanptr);
  signal->theData[0] = scanptr.p->scanAccPtr;
  signal->theData[1] = RNIL;
  signal->theData[2] = ZCOPY_CLOSE;
  sendSignal(tcConnectptr.p->tcAccBlockref, GSN_NEXT_SCANREQ, signal, 3, JBB);
  return;
}//Dblqh::continueCloseCopyAfterBlockedLab()

/*---------------------------------------------------------------------------*/
/*   ENTER NEXT_SCANCONF WITH                                                */
/*     SCANPTR,                                                              */
/*     TFRAGID,                                                              */
/*     TACC_OPPTR,                                                           */
/*     TLOCAL_KEY1,                                                          */
/*     TLOCAL_KEY2,                                                          */
/*     TKEY_LENGTH,                                                          */
/*     TKEY1,                                                                */
/*     TKEY2,                                                                */
/*     TKEY3,                                                                */
/*     TKEY4                                                                 */
/*---------------------------------------------------------------------------*/
/*   PRECONDITION: SCAN_STATE = WAIT_CLOSE_COPY                              */
/*---------------------------------------------------------------------------*/
void Dblqh::accCopyCloseConfLab(Signal* signal) 
{
  tcConnectptr.i = scanptr.p->scanTcrec;
  scanptr.p->scanState = ScanRecord::WAIT_DELETE_STORED_PROC_ID_COPY;
  ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
  signal->theData[0] = tcConnectptr.p->tupConnectrec;
  signal->theData[1] = tcConnectptr.p->tableref;
  signal->theData[2] = scanptr.p->scanSchemaVersion;
  signal->theData[3] = ZDELETE_STORED_PROC_ID;
  signal->theData[4] = scanptr.p->scanStoredProcId;
  sendSignal(tcConnectptr.p->tcTupBlockref, GSN_STORED_PROCREQ, signal, 5, JBB);
  return;
}//Dblqh::accCopyCloseConfLab()

/*---------------------------------------------------------------------------*/
/*   ENTER STORED_PROCCONF WITH                                              */
/*     TC_CONNECTPTR,                                                        */
/*     TSTORED_PROC_ID                                                       */
/*---------------------------------------------------------------------------*/
/* PRECONDITION: SCAN_STATE = WAIT_DELETE_STORED_PROC_ID_COPY                */
/*---------------------------------------------------------------------------*/
void Dblqh::tupCopyCloseConfLab(Signal* signal) 
{
  fragptr.i = tcConnectptr.p->fragmentptr;
  ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
  fragptr.p->copyFragState = ZIDLE;

  if (tcConnectptr.p->abortState == TcConnectionrec::NEW_FROM_TC) {
    jam();
    tcNodeFailptr.i = tcConnectptr.p->tcNodeFailrec;
    ptrCheckGuard(tcNodeFailptr, ctcNodeFailrecFileSize, tcNodeFailRecord);
    tcNodeFailptr.p->tcRecNow = tcConnectptr.i + 1;
    signal->theData[0] = ZLQH_TRANS_NEXT;
    signal->theData[1] = tcNodeFailptr.i;
    sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);

    CopyFragRef * const ref = (CopyFragRef *)&signal->theData[0];
    ref->userPtr = scanptr.p->copyPtr;
    ref->sendingNodeId = cownNodeid;
    ref->startingNodeId = scanptr.p->scanNodeId;
    ref->tableId = fragptr.p->tabRef;
    ref->fragId = fragptr.p->fragId;
    ref->errorCode = ZNODE_FAILURE_ERROR;
    sendSignal(scanptr.p->scanApiBlockref, GSN_COPY_FRAGREF, signal,
               CopyFragRef::SignalLength, JBB);
  } else {
    if (scanptr.p->scanErrorCounter > 0) {
      jam();
      CopyFragRef * const ref = (CopyFragRef *)&signal->theData[0];
      ref->userPtr = scanptr.p->copyPtr;
      ref->sendingNodeId = cownNodeid;
      ref->startingNodeId = scanptr.p->scanNodeId;
      ref->tableId = fragptr.p->tabRef;
      ref->fragId = fragptr.p->fragId;
      ref->errorCode = tcConnectptr.p->errorCode;
      sendSignal(scanptr.p->scanApiBlockref, GSN_COPY_FRAGREF, signal,
                 CopyFragRef::SignalLength, JBB);
    } else {
      jam();
      CopyFragConf * const conf = (CopyFragConf *)&signal->theData[0];
      conf->userPtr = scanptr.p->copyPtr;
      conf->sendingNodeId = cownNodeid;
      conf->startingNodeId = scanptr.p->scanNodeId;
      conf->tableId = tcConnectptr.p->tableref;
      conf->fragId = tcConnectptr.p->fragmentid;
      sendSignal(scanptr.p->scanApiBlockref, GSN_COPY_FRAGCONF, signal,
                 CopyFragConf::SignalLength, JBB);
    }//if
  }//if
  releaseActiveCopy(signal);
  tcConnectptr.p->tcScanRec = RNIL;
  finishScanrec(signal);
  releaseOprec(signal);
  releaseTcrec(signal, tcConnectptr);
  releaseScanrec(signal);
}//Dblqh::tupCopyCloseConfLab()

/*---------------------------------------------------------------------------*/
/*   A NODE FAILURE OCCURRED DURING THE COPY PROCESS. WE NEED TO CLOSE THE   */
/*   COPY PROCESS SINCE A NODE FAILURE DURING THE COPY PROCESS WILL ALSO     */
/*   FAIL THE NODE THAT IS TRYING TO START-UP.                               */
/*---------------------------------------------------------------------------*/
void Dblqh::closeCopyRequestLab(Signal* signal) 
{
  scanptr.p->scanErrorCounter++;
  switch (scanptr.p->scanState) {
  case ScanRecord::WAIT_TUPKEY_COPY:
  case ScanRecord::WAIT_NEXT_SCAN_COPY:
    jam();
/*---------------------------------------------------------------------------*/
/*   SET COMPLETION STATUS AND WAIT FOR OPPORTUNITY TO STOP THE SCAN.        */
//   ALSO SET NO OF WORDS OUTSTANDING TO ZERO TO AVOID ETERNAL WAIT.
/*---------------------------------------------------------------------------*/
    scanptr.p->scanCompletedStatus = ZTRUE;
    tcConnectptr.p->copyCountWords = 0;
    break;
  case ScanRecord::WAIT_ACC_COPY:
  case ScanRecord::WAIT_STORED_PROC_COPY:
    jam();
/*---------------------------------------------------------------------------*/
/*   WE ARE CURRENTLY STARTING UP THE SCAN. SET COMPLETED STATUS AND WAIT FOR*/
/*   COMPLETION OF STARTUP.                                                  */
/*---------------------------------------------------------------------------*/
    scanptr.p->scanCompletedStatus = ZTRUE;
    break;
  case ScanRecord::WAIT_CLOSE_COPY:
  case ScanRecord::WAIT_DELETE_STORED_PROC_ID_COPY:
    jam();
/*---------------------------------------------------------------------------*/
/*   CLOSE IS ALREADY ONGOING. WE NEED NOT DO ANYTHING.                      */
/*---------------------------------------------------------------------------*/
    break;
  case ScanRecord::WAIT_LQHKEY_COPY:
    jam();
/*---------------------------------------------------------------------------*/
/*   WE ARE WAITING FOR THE FAILED NODE. THE NODE WILL NEVER COME BACK.      */
//   WE NEED TO START THE FAILURE HANDLING IMMEDIATELY.
//   ALSO SET NO OF WORDS OUTSTANDING TO ZERO TO AVOID ETERNAL WAIT.
/*---------------------------------------------------------------------------*/
    tcConnectptr.p->copyCountWords = 0;
    closeCopyLab(signal);
    break;
  default:
    ndbrequire(false);
    break;
  }//switch
  return;
}//Dblqh::closeCopyRequestLab()

/* ****************************************************** */
/*  COPY_ACTIVEREQ: Change state of a fragment to ACTIVE. */
/* ****************************************************** */
void Dblqh::execCOPY_ACTIVEREQ(Signal* signal) 
{
  CRASH_INSERTION(5026);

  const CopyActiveReq * const req = (CopyActiveReq *)&signal->theData[0];
  jamEntry();
  Uint32 masterPtr = req->userPtr;
  BlockReference masterRef = req->userRef;
  tabptr.i = req->tableId;
  ptrCheckGuard(tabptr, ctabrecFileSize, tablerec);
  Uint32 fragId = req->fragId;
  ndbrequire(getFragmentrec(signal, fragId));

  fragptr.p->fragDistributionKey = req->distributionKey;
  
  ndbrequire(cnoActiveCopy < 3);
  cactiveCopy[cnoActiveCopy] = fragptr.i;
  cnoActiveCopy++;
  fragptr.p->masterBlockref = masterRef;
  fragptr.p->masterPtr = masterPtr;
  if (fragptr.p->fragStatus == Fragrecord::FSACTIVE) {
    jam();
/*------------------------------------------------------*/
/*       PROCESS HAVE ALREADY BEEN STARTED BY PREVIOUS  */
/*       MASTER. WE HAVE ALREADY SET THE PROPER MASTER  */
/*       BLOCK REFERENCE.                               */
/*------------------------------------------------------*/
    if (fragptr.p->activeTcCounter == 0) {
      jam();
/*------------------------------------------------------*/
/*       PROCESS WAS EVEN COMPLETED.                    */
/*------------------------------------------------------*/
      sendCopyActiveConf(signal, tabptr.i);
    }//if
    return;
  }//if
  fragptr.p->fragStatus = Fragrecord::FSACTIVE;
  if (fragptr.p->lcpFlag == Fragrecord::LCP_STATE_TRUE) {
    jam();
    fragptr.p->logFlag = Fragrecord::STATE_TRUE;
  }//if
  fragptr.p->activeTcCounter = 1;
/*------------------------------------------------------*/
/*       SET IT TO ONE TO ENSURE THAT IT IS NOT POSSIBLE*/
/*       TO DECREASE IT TO ZERO UNTIL WE HAVE COMPLETED */
/*       THE SCAN.                                      */
/*------------------------------------------------------*/
  signal->theData[0] = ZSCAN_TC_CONNECT;
  signal->theData[1] = 0;
  signal->theData[2] = tabptr.i;
  signal->theData[3] = fragId;
  sendSignal(cownref, GSN_CONTINUEB, signal, 4, JBB);
  return;
}//Dblqh::execCOPY_ACTIVEREQ()

void Dblqh::scanTcConnectLab(Signal* signal, Uint32 tstartTcConnect, Uint32 fragId) 
{
  Uint32 tendTcConnect;

  ndbrequire(getFragmentrec(signal, fragId));
  if ((tstartTcConnect + 200) >= ctcConnectrecFileSize) {
    jam();
    tendTcConnect = ctcConnectrecFileSize - 1;
  } else {
    jam();
    tendTcConnect = tstartTcConnect + 200;
  }//if
  for (tcConnectptr.i = tstartTcConnect; 
       tcConnectptr.i <= tendTcConnect; 
       tcConnectptr.i++) {
    jam();
    ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
    if (tcConnectptr.p->transactionState != TcConnectionrec::IDLE) {
      switch (tcConnectptr.p->logWriteState) {
      case TcConnectionrec::NOT_WRITTEN:
        jam();
        if (fragptr.i == tcConnectptr.p->fragmentptr) {
          jam();
          fragptr.p->activeTcCounter = fragptr.p->activeTcCounter + 1;
          tcConnectptr.p->logWriteState = TcConnectionrec::NOT_WRITTEN_WAIT;
        }//if
        break;
      default:
        jam();
        /*empty*/;
        break;
      }//switch
    }//if
  }//for
  if (tendTcConnect < (ctcConnectrecFileSize - 1)) {
    jam();
    signal->theData[0] = ZSCAN_TC_CONNECT;
    signal->theData[1] = tendTcConnect + 1;
    signal->theData[2] = tabptr.i;
    signal->theData[3] = fragId;
    sendSignal(cownref, GSN_CONTINUEB, signal, 4, JBB);
  } else {
    jam();
/*------------------------------------------------------*/
/*       THE SCAN HAVE BEEN COMPLETED. WE CHECK IF ALL  */
/*       OPERATIONS HAVE ALREADY BEEN COMPLETED.        */
/*------------------------------------------------------*/
    ndbrequire(fragptr.p->activeTcCounter > 0);
    fragptr.p->activeTcCounter--;
    if (fragptr.p->activeTcCounter == 0) {
      jam();
/*------------------------------------------------------*/
/*       SET START GLOBAL CHECKPOINT TO THE NEXT        */
/*       CHECKPOINT WE HAVE NOT YET HEARD ANYTHING ABOUT*/
/*       THIS GCP WILL BE COMPLETELY COVERED BY THE LOG.*/
/*------------------------------------------------------*/
      fragptr.p->startGci = cnewestGci + 1;
      sendCopyActiveConf(signal, tabptr.i);
    }//if
  }//if
  return;
}//Dblqh::scanTcConnectLab()

/*---------------------------------------------------------------------------*/
/*   A NEW MASTER IS REQUESTING THE STATE IN LQH OF THE COPY FRAGMENT PARTS. */
/*---------------------------------------------------------------------------*/
/* ***************>> */
/*  COPY_STATEREQ  > */
/* ***************>> */
void Dblqh::execCOPY_STATEREQ(Signal* signal) 
{
  jamEntry();
  ndbrequire(0)
#if 0
  Uint32* dataPtr = &signal->theData[2];
  BlockReference tmasterBlockref = signal->theData[0];
  Uint32 tnoCopy = 0;
  do {
    jam();
    arrGuard(tnoCopy, 4);
    fragptr.i = cactiveCopy[tnoCopy];
    if (fragptr.i == RNIL) {
      jam();
      break;
    }//if
    ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
    if (fragptr.p->copyFragState != ZIDLE) {
      jam();
/*---------------------------------------------------------------------------*/
/*   THIS FRAGMENT IS CURRENTLY ACTIVE IN COPYING THE FRAGMENT.              */
/*---------------------------------------------------------------------------*/
      scanptr.i = fragptr.p->fragScanRec[NR_ScanNo];
      c_scanRecordPool.getPtr(scanptr);
      if (scanptr.p->scanCompletedStatus == ZTRUE) {
        jam();
        dataPtr[3 + (tnoCopy << 2)] = ZCOPY_CLOSING;
      } else {
        jam();
        dataPtr[3 + (tnoCopy << 2)] = ZCOPY_ONGOING;
      }//if
      dataPtr[2 + (tnoCopy << 2)] = scanptr.p->scanSchemaVersion;
      scanptr.p->scanApiBlockref = tmasterBlockref;
    } else {
      ndbrequire(fragptr.p->activeTcCounter != 0);
/*---------------------------------------------------------------------------*/
/*   COPY FRAGMENT IS COMPLETED AND WE ARE CURRENTLY GETTING THE STARTING    */
/*   GCI OF THE NEW REPLICA OF THIS FRAGMENT.                                */
/*---------------------------------------------------------------------------*/
      fragptr.p->masterBlockref = tmasterBlockref;
      dataPtr[3 + (tnoCopy << 2)] = ZCOPY_ACTIVATION;
    }//if
    dataPtr[tnoCopy << 2] = fragptr.p->tabRef;
    dataPtr[1 + (tnoCopy << 2)] = fragptr.p->fragId;
    tnoCopy++;
  } while (tnoCopy < cnoActiveCopy);
  signal->theData[0] = cownNodeid;
  signal->theData[1] = tnoCopy;
  sendSignal(tmasterBlockref, GSN_COPY_STATECONF, signal, 18, JBB);
#endif
  return;
}//Dblqh::execCOPY_STATEREQ()

/* ========================================================================= */
/* =======              INITIATE TC RECORD AT COPY FRAGMENT          ======= */
/*                                                                           */
/*       SUBROUTINE SHORT NAME = ICT                                         */
/* ========================================================================= */
void Dblqh::initCopyTc(Signal* signal) 
{
  const NextScanConf * const nextScanConf = (NextScanConf *)&signal->theData[0];
  scanptr.p->scanLocalref[0] = nextScanConf->localKey[0];
  scanptr.p->scanLocalref[1] = nextScanConf->localKey[1];
  scanptr.p->scanLocalFragid = nextScanConf->fragId;
  tcConnectptr.p->operation = ZREAD;
  tcConnectptr.p->apiVersionNo = 0;
  tcConnectptr.p->opExec = 0;	/* NOT INTERPRETED MODE */
  tcConnectptr.p->schemaVersion = scanptr.p->scanSchemaVersion;
  Uint32 reqinfo = 0;
  LqhKeyReq::setLockType(reqinfo, ZINSERT);
  LqhKeyReq::setDirtyFlag(reqinfo, 1);
  LqhKeyReq::setSimpleFlag(reqinfo, 1);
  LqhKeyReq::setOperation(reqinfo, ZWRITE);
                                        /* AILen in LQHKEYREQ  IS ZERO */
  tcConnectptr.p->reqinfo = reqinfo;
/* ------------------------------------------------------------------------ */
/* THE RECEIVING NODE WILL EXPECT THAT IT IS THE LAST NODE AND WILL         */
/* SEND COMPLETED AS THE RESPONSE SIGNAL SINCE DIRTY_OP BIT IS SET.         */
/* ------------------------------------------------------------------------ */
  tcConnectptr.p->nodeAfterNext[0] = ZNIL;
  tcConnectptr.p->nodeAfterNext[1] = ZNIL;
  tcConnectptr.p->tcBlockref = cownref;
  tcConnectptr.p->readlenAi = 0;
  tcConnectptr.p->storedProcId = ZNIL;
  tcConnectptr.p->opExec = 0;
  tcConnectptr.p->nextSeqNoReplica = 0;
  tcConnectptr.p->dirtyOp = ZFALSE;
  tcConnectptr.p->lastReplicaNo = 0;
  tcConnectptr.p->currTupAiLen = 0;
  tcConnectptr.p->tcTimer = cLqhTimeOutCount;
}//Dblqh::initCopyTc()

/* ------------------------------------------------------------------------- */
/* -------               SEND COPY_ACTIVECONF TO MASTER DIH          ------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void Dblqh::sendCopyActiveConf(Signal* signal, Uint32 tableId) 
{
  releaseActiveCopy(signal);
  CopyActiveConf * const conf = (CopyActiveConf *)&signal->theData[0];
  conf->userPtr = fragptr.p->masterPtr;
  conf->tableId = tableId;
  conf->fragId = fragptr.p->fragId;
  conf->startingNodeId = cownNodeid;
  conf->startGci = fragptr.p->startGci;
  sendSignal(fragptr.p->masterBlockref, GSN_COPY_ACTIVECONF, signal,
             CopyActiveConf::SignalLength, JBB);
}//Dblqh::sendCopyActiveConf()

/* ########################################################################## 
 * #######                       LOCAL CHECKPOINT MODULE              #######
 *
 * ########################################################################## 
 * --------------------------------------------------------------------------
 *  THIS MODULE HANDLES THE EXECUTION AND CONTROL OF LOCAL CHECKPOINTS
 *  IT CONTROLS THE LOCAL CHECKPOINTS IN TUP AND ACC. IT DOES ALSO INTERACT
 *  WITH DIH TO CONTROL WHICH GLOBAL CHECKPOINTS THAT ARE RECOVERABLE
 * ------------------------------------------------------------------------- */
void Dblqh::execEMPTY_LCP_REQ(Signal* signal)
{
  jamEntry();
  CRASH_INSERTION(5008);
  EmptyLcpReq * const emptyLcpOrd = (EmptyLcpReq*)&signal->theData[0];

  lcpPtr.i = 0;
  ptrAss(lcpPtr, lcpRecord);
  
  Uint32 nodeId = refToNode(emptyLcpOrd->senderRef);

  lcpPtr.p->m_EMPTY_LCP_REQ.set(nodeId);
  lcpPtr.p->reportEmpty = true;

  if (lcpPtr.p->lcpState == LcpRecord::LCP_IDLE){ 
    jam();
    bool ok = false;
    switch(clcpCompletedState){
    case LCP_IDLE:
      ok = true;
      sendEMPTY_LCP_CONF(signal, true);
      break;
    case LCP_RUNNING:
      ok = true;
      sendEMPTY_LCP_CONF(signal, false);
      break;
    case LCP_CLOSE_STARTED:
      jam();
    case ACC_LCP_CLOSE_COMPLETED:
      jam();
    case TUP_LCP_CLOSE_COMPLETED:
      jam();
      ok = true;
      break;
    }
    ndbrequire(ok);
    
  }//if
  
  return;
}//Dblqh::execEMPTY_LCPREQ()

void Dblqh::execLCP_FRAG_ORD(Signal* signal)
{
  jamEntry();
  CRASH_INSERTION(5010);
  LcpFragOrd * const lcpFragOrd = (LcpFragOrd *)&signal->theData[0];
  Uint32 lcpId = lcpFragOrd->lcpId;

  lcpPtr.i = 0;
  ptrAss(lcpPtr, lcpRecord);
  
  lcpPtr.p->lastFragmentFlag = lcpFragOrd->lastFragmentFlag;
  if (lcpFragOrd->lastFragmentFlag) {
    jam();
    if (lcpPtr.p->lcpState == LcpRecord::LCP_IDLE) {
      jam();
      /* ----------------------------------------------------------
       *       NOW THE COMPLETE LOCAL CHECKPOINT ROUND IS COMPLETED.  
       * -------------------------------------------------------- */
      if (cnoOfFragsCheckpointed > 0) {
        jam();
        completeLcpRoundLab(signal);
      } else {
        jam();
        sendLCP_COMPLETE_REP(signal, lcpId);
      }//if
    }
    return;
  }//if
  tabptr.i = lcpFragOrd->tableId;
  ptrCheckGuard(tabptr, ctabrecFileSize, tablerec);
  
  ndbrequire(tabptr.p->tableStatus == Tablerec::PREP_DROP_TABLE_ONGOING ||
	     tabptr.p->tableStatus == Tablerec::PREP_DROP_TABLE_DONE ||
	     tabptr.p->tableStatus == Tablerec::TABLE_DEFINED);

  ndbrequire(getFragmentrec(signal, lcpFragOrd->fragmentId));
  
  lcpPtr.i = 0;
  ptrAss(lcpPtr, lcpRecord);
  ndbrequire(!lcpPtr.p->lcpQueued);
  if (c_lcpId < lcpFragOrd->lcpId) {
    jam();
    /**
     * A new LCP
     */
    c_lcpId = lcpFragOrd->lcpId;
    ndbrequire(lcpPtr.p->lcpState == LcpRecord::LCP_IDLE);
    setLogTail(signal, lcpFragOrd->keepGci);
    ndbrequire(clcpCompletedState == LCP_IDLE);
    clcpCompletedState = LCP_RUNNING;
  }//if
  cnoOfFragsCheckpointed++;
  
  if(tabptr.p->tableStatus == Tablerec::PREP_DROP_TABLE_DONE){
    jam();
    LcpRecord::FragOrd fragOrd;
    fragOrd.fragPtrI = fragptr.i;
    fragOrd.lcpFragOrd = * lcpFragOrd;
    sendLCP_FRAG_REP(signal, fragOrd);
    return;
  }

  if (lcpPtr.p->lcpState != LcpRecord::LCP_IDLE) {
    ndbrequire(lcpPtr.p->lcpQueued == false);
    lcpPtr.p->lcpQueued = true;
    lcpPtr.p->queuedFragment.fragPtrI = fragptr.i;
    lcpPtr.p->queuedFragment.lcpFragOrd = * lcpFragOrd;
    return;
  }//if
  
  lcpPtr.p->currentFragment.fragPtrI = fragptr.i;
  lcpPtr.p->currentFragment.lcpFragOrd = * lcpFragOrd;
  
  sendLCP_FRAGIDREQ(signal);
}//Dblqh::execLCP_FRAGORD()

/* --------------------------------------------------------------------------
 *       PRECONDITION: LCP_PTR:LCP_STATE = WAIT_FRAGID
 * -------------------------------------------------------------------------- 
 *       WE NOW HAVE THE LOCAL FRAGMENTS THAT THE LOCAL CHECKPOINT WILL USE.
 * -------------------------------------------------------------------------- */
void Dblqh::execLCP_FRAGIDCONF(Signal* signal) 
{
  UintR Tfragid[4];

  jamEntry();

  lcpPtr.i = signal->theData[0];
  
  Uint32 TaccPtr = signal->theData[1];
  Uint32 noLocfrag = signal->theData[2];
  Tfragid[0] = signal->theData[3];
  Tfragid[1] = signal->theData[4];

  ptrCheckGuard(lcpPtr, clcpFileSize, lcpRecord);
  ndbrequire(lcpPtr.p->lcpState == LcpRecord::LCP_WAIT_FRAGID);
  /* ------------------------------------------------------------------------
   * NO ERROR CHECKING OF TNO_LOCFRAG VALUE. OUT OF BOUND WILL IMPLY THAT AN
   * INDEX OUT OF RANGE WILL CAUSE A SYSTEM RESTART WHICH IS DESIRED.
   * ------------------------------------------------------------------------ */
  lcpPtr.p->lcpAccptr = TaccPtr;
  fragptr.i = lcpPtr.p->currentFragment.fragPtrI;
  ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
  ndbrequire(noLocfrag - 1 < 2);
  for (Uint32 Tindex = 0; Tindex < noLocfrag; Tindex++) {
    jam();
    Uint32 fragId = Tfragid[Tindex];
    /* ----------------------------------------------------------------------
     *  THERE IS NO ERROR CHECKING ON PURPOSE. IT IS POSSIBLE TO CALCULATE HOW
     *  MANY LOCAL LCP RECORDS THERE SHOULD BE. IT SHOULD NEVER HAPPEN THAT 
     *  THERE IS NO ONE FREE. IF THERE IS NO ONE IT WILL ALSO BE A POINTER 
     *  OUT OF RANGE WHICH IS AN ERROR CODE IN ITSELF. REUSES ERROR HANDLING 
     *  IN AXE VM.
     * ---------------------------------------------------------------------- */
    seizeLcpLoc(signal);
    initLcpLocAcc(signal, fragId);
    seizeLcpLoc(signal);
    initLcpLocTup(signal, fragId);
    signal->theData[0] = lcpLocptr.i;
    signal->theData[1] = cownref;
    signal->theData[2] = lcpPtr.p->currentFragment.lcpFragOrd.tableId;
    signal->theData[3] = lcpLocptr.p->locFragid;
    signal->theData[4] = lcpPtr.p->currentFragment.lcpFragOrd.lcpNo;
    signal->theData[5] = lcpPtr.p->currentFragment.lcpFragOrd.lcpId % MAX_LCP_STORED;
    sendSignal(fragptr.p->tupBlockref, GSN_TUP_PREPLCPREQ, signal, 6, JBB);
  }//for
  lcpPtr.p->lcpState = LcpRecord::LCP_WAIT_TUP_PREPLCP;
  return;
}//Dblqh::execLCP_FRAGIDCONF()

/* --------------------------------------------------------------------------
 *       PRECONDITION: LCP_LOCPTR:LCP_STATE = WAIT_TUPPREPLCP
 * --------------------------------------------------------------------------
 *       WE HAVE NOW PREPARED A LOCAL FRAGMENT IN TUP FOR LCP EXECUTION.
 * -------------------------------------------------------------------------- */
void Dblqh::execTUP_PREPLCPCONF(Signal* signal) 
{
  UintR ttupPtr;

  jamEntry();
  lcpLocptr.i = signal->theData[0];
  ttupPtr = signal->theData[1];
  ptrCheckGuard(lcpLocptr, clcpLocrecFileSize, lcpLocRecord);

  lcpPtr.i = lcpLocptr.p->masterLcpRec;
  ptrCheckGuard(lcpPtr, clcpFileSize, lcpRecord);
  ndbrequire(lcpLocptr.p->lcpLocstate == LcpLocRecord::WAIT_TUP_PREPLCP);

  lcpLocptr.p->tupRef = ttupPtr;
  lcpLocptr.p->lcpLocstate = LcpLocRecord::IDLE;
  checkLcpTupprep(signal);
  if (lcpPtr.p->lcpState != LcpRecord::LCP_WAIT_HOLDOPS) {
    jam();
    return;
  }//if
  fragptr.i = lcpPtr.p->currentFragment.fragPtrI;
  ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
  lcpLocptr.i = lcpPtr.p->firstLcpLocAcc;
  do {
    jam();
    ptrCheckGuard(lcpLocptr, clcpLocrecFileSize, lcpLocRecord);
    lcpLocptr.p->lcpLocstate = LcpLocRecord::WAIT_LCPHOLDOP;
    signal->theData[0] = lcpPtr.p->lcpAccptr;
    signal->theData[1] = lcpLocptr.p->locFragid;
    signal->theData[2] = 0;
    signal->theData[3] = lcpLocptr.i;
    sendSignal(fragptr.p->accBlockref, GSN_LCP_HOLDOPREQ, signal, 4, JBA);
    lcpLocptr.i = lcpLocptr.p->nextLcpLoc;
  } while (lcpLocptr.i != RNIL);
  /* ------------------------------------------------------------------------
   *   SET STATE ON FRAGMENT TO BLOCKED TO ENSURE THAT NO MORE OPERATIONS ARE
   *   STARTED FROM LQH IN TUP AND ACC UNTIL THE START CHECKPOINT HAS BEEN
   *   COMPLETED. ALSO SET THE LOCAL CHECKPOINT STATE TO WAIT FOR 
   *   LCP_HOLDOPCONF
   * ----------------------------------------------------------------------- */
  fragptr.p->fragStatus = Fragrecord::BLOCKED;
  fragptr.p->fragActiveStatus = ZTRUE;
  lcpPtr.p->lcpState = LcpRecord::LCP_WAIT_HOLDOPS;
  return;
}//Dblqh::execTUP_PREPLCPCONF()

void Dblqh::execTUP_PREPLCPREF(Signal* signal) 
{
  jamEntry();
  ndbrequire(false);
}//Dblqh::execTUP_PREPLCPREF()

void Dblqh::execLCP_FRAGIDREF(Signal* signal) 
{
  jamEntry();
  ndbrequire(false);
}//Dblqh::execLCP_FRAGIDREF()

/* --------------------------------------------------------------------------
 *   A NUMBER OF OPERATIONS THAT HAVE BEEN SET ON HOLD IN ACC. MOVE THOSE TO
 *   LIST OF BLOCKED ACC OPERATIONS. IF MORE OPERATIONS ARE BLOCKED GET THOSE
 *   OTHERWISE CONTINUE THE LOCAL CHECKPOINT BY REQUESTING TUP AND ACC TO
 *   WRITE THEIR START CHECKPOINT.
 * -------------------------------------------------------------------------- 
 *       PRECONDITION: LCP_LOCPTR:LCP_LOCSTATE = WAIT_LCPHOLDOP
 * ------------------------------------------------------------------------- */
/* ***************>> */
/*  LCP_HOLDOPCONF > */
/* ***************>> */
void Dblqh::execLCP_HOLDOPCONF(Signal* signal) 
{
  UintR tnoHoldops;
  Uint32 Tdata[23];
  Uint32 Tlength;

  jamEntry();
  lcpLocptr.i = signal->theData[0];
  Tlength = signal->theData[1];
  for (Uint32 i = 0; i < 23; i++)
    Tdata[i] = signal->theData[i + 2];
  ptrCheckGuard(lcpLocptr, clcpLocrecFileSize, lcpLocRecord);
  ndbrequire(lcpLocptr.p->lcpLocstate == LcpLocRecord::WAIT_LCPHOLDOP);

  lcpPtr.i = lcpLocptr.p->masterLcpRec;
  /* ------------------------------------------------------------------------
   *   NO ERROR CHECK ON USING VALUE IN MASTER_LCP_REC. ERROR IN THIS 
   *   REFERENCE WILL CAUSE POINTER OUT OF RANGE WHICH CAUSES A SYSTEM RESTART.
   * ----------------------------------------------------------------------- */
  tnoHoldops = Tlength & 65535;
  ptrCheckGuard(lcpPtr, clcpFileSize, lcpRecord);
  fragptr.i = lcpPtr.p->currentFragment.fragPtrI;
  ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
  ndbrequire(tnoHoldops <= 23);
  for (Uint32 Tindex = 0; Tindex < tnoHoldops; Tindex++) {
    jam();
    tcConnectptr.i = Tdata[Tindex];
    ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
    moveActiveToAcc(signal);
  }//for
  if ((Tlength >> 16) == 1) {
    jam();
                                        /* MORE HOLDOPS NEEDED */
    signal->theData[0] = lcpPtr.p->lcpAccptr;
    signal->theData[1] = lcpLocptr.p->locFragid;
    signal->theData[2] = 1;
    signal->theData[3] = lcpLocptr.i;
    sendSignal(fragptr.p->accBlockref, GSN_LCP_HOLDOPREQ, signal, 4, JBA);
    return;
  } else {
    jam();

                                        /* NO MORE HOLDOPS NEEDED */
    lcpLocptr.p->lcpLocstate = LcpLocRecord::HOLDOP_READY;
    checkLcpHoldop(signal);

    if (lcpPtr.p->lcpState == LcpRecord::LCP_WAIT_ACTIVE_FINISH) {
      if (fragptr.p->activeList == RNIL) {
        jam();
	/* ------------------------------------------------------------------ 
	 *  THERE ARE NO MORE ACTIVE OPERATIONS. IT IS NOW OK TO START THE 
	 *  LOCAL CHECKPOINT IN BOTH TUP AND ACC.
	 * ----------------------------------------------------------------- */
        sendStartLcp(signal);
        lcpPtr.p->lcpState = LcpRecord::LCP_START_CHKP;
      } else {
        jam();
	// Set this to signal releaseActiveFrag 
	// that it should check to see if itäs time to call sendStartLcp
	fragptr.p->lcpRef = lcpPtr.i;
      }//if
    }//if
  }//if

  /* ----------------------- */
  /*           ELSE          */
  /* ------------------------------------------------------------------------
   *   THERE ARE STILL MORE ACTIVE OPERATIONS. WAIT UNTIL THEY ARE FINSIHED.
   *   THIS IS DISCOVERED WHEN RELEASE_ACTIVE_FRAG IS EXECUTED.
   * ------------------------------------------------------------------------
   *       DO NOTHING, EXIT IS EXECUTED BELOW                                  
   * ----------------------------------------------------------------------- */
  return;
}//Dblqh::execLCP_HOLDOPCONF()

/* ***************> */
/*  LCP_HOLDOPREF > */
/* ***************> */
void Dblqh::execLCP_HOLDOPREF(Signal* signal) 
{
  jamEntry();
  ndbrequire(false);
}//Dblqh::execLCP_HOLDOPREF()

/* ************************************************************************>>
 *  ACC_LCPSTARTED: Confirm that ACC started local checkpoint and undo 
 *  logging is on. 
 * ************************************************************************>> 
 * --------------------------------------------------------------------------
 *       PRECONDITION: LCP_LOCPTR:LCP_LOCSTATE = ACC_WAIT_STARTED 
 * ------------------------------------------------------------------------- */
void Dblqh::execACC_LCPSTARTED(Signal* signal) 
{
  jamEntry();
  lcpLocptr.i = signal->theData[0];
  ptrCheckGuard(lcpLocptr, clcpLocrecFileSize, lcpLocRecord);
  ndbrequire(lcpLocptr.p->lcpLocstate == LcpLocRecord::ACC_WAIT_STARTED);

  lcpPtr.i = lcpLocptr.p->masterLcpRec;
  ptrCheckGuard(lcpPtr, clcpFileSize, lcpRecord);
  /* ------------------------------------------------------------------------ 
   * NO ERROR CHECK ON USING VALUE IN MASTER_LCP_REC. ERROR IN THIS 
   * REFERENCE  WILL CAUSE POINTER OUT OF RANGE WHICH CAUSES A SYSTEM RESTART.
   * ----------------------------------------------------------------------- */
  lcpLocptr.p->lcpLocstate = LcpLocRecord::ACC_STARTED;
  lcpStartedLab(signal);
  return;
}//Dblqh::execACC_LCPSTARTED()

/* ******************************************> */
/*  TUP_LCPSTARTED: Same as above but for TUP. */
/* ******************************************> */
/* -------------------------------------------------------------------------- 
 *       PRECONDITION: LCP_LOCPTR:LCP_LOCSTATE = TUP_WAIT_STARTED
 * ------------------------------------------------------------------------- */
void Dblqh::execTUP_LCPSTARTED(Signal* signal) 
{
  jamEntry();
  lcpLocptr.i = signal->theData[0];
  ptrCheckGuard(lcpLocptr, clcpLocrecFileSize, lcpLocRecord);
  ndbrequire(lcpLocptr.p->lcpLocstate == LcpLocRecord::TUP_WAIT_STARTED);

  lcpPtr.i = lcpLocptr.p->masterLcpRec;
  ptrCheckGuard(lcpPtr, clcpFileSize, lcpRecord);
  /* ------------------------------------------------------------------------
   *   NO ERROR CHECK ON USING VALUE IN MASTER_LCP_REC. ERROR IN THIS REFERENCE
   *   WILL CAUSE POINTER OUT OF RANGE WHICH CAUSES A SYSTEM RESTART.
   * ----------------------------------------------------------------------- */
  lcpLocptr.p->lcpLocstate = LcpLocRecord::TUP_STARTED;
  lcpStartedLab(signal);
  return;
}//Dblqh::execTUP_LCPSTARTED()

void Dblqh::lcpStartedLab(Signal* signal) 
{
  if (checkLcpStarted(signal))
  {
    jam();
    /* ----------------------------------------------------------------------
     *  THE LOCAL CHECKPOINT HAS BEEN STARTED. IT IS NOW TIME TO 
     *  RESTART THE TRANSACTIONS WHICH HAVE BEEN BLOCKED.
     * --------------------------------------------------------------------- */
    fragptr.i = lcpPtr.p->currentFragment.fragPtrI;
    ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
    /* ----------------------------------------------------------------------
     *    UPDATE THE MAX_GCI_IN_LCP AND MAX_GCI_COMPLETED_IN_LCP NOW BEFORE
     *    ACTIVATING THE FRAGMENT AGAIN.
     * --------------------------------------------------------------------- */
    ndbrequire(lcpPtr.p->currentFragment.lcpFragOrd.lcpNo < MAX_LCP_STORED);
    fragptr.p->maxGciInLcp = fragptr.p->newestGci;
    fragptr.p->maxGciCompletedInLcp = cnewestCompletedGci;
    sendAccContOp(signal);	/* START OPERATIONS IN ACC       */
    moveAccActiveFrag(signal);	/* MOVE FROM ACC BLOCKED LIST TO ACTIVE LIST 
				   ON FRAGMENT */
  }
  /*---------------*/
  /*       ELSE    */
  /*-------------------------------------------------------------------------*/
  /*    THE LOCAL CHECKPOINT HAS NOT BEEN STARTED. EXIT AND WAIT FOR 
   *    MORE SIGNALS */
  /*-------------------------------------------------------------------------*/
  /*       DO NOTHING, EXIT IS EXECUTED BELOW                                */
  /*-------------------------------------------------------------------------*/
  return;
}//Dblqh::lcpStartedLab()

/*---------------------------------------------------------------------------
 *     ACC HAVE RESTARTED THE BLOCKED OPERATIONS AGAIN IN ONE FRAGMENT PART. 
 *     IT IS NOW OUR TURN TO RESTART ALL OPERATIONS QUEUED IN LQH IF ALL 
 *     FRAGMENT PARTS ARE COMPLETED.
 *-------------------------------------------------------------------------- */
void Dblqh::execACC_CONTOPCONF(Signal* signal) 
{
  if(ERROR_INSERTED(5035) && signal->getSendersBlockRef() != reference()){
    sendSignalWithDelay(reference(), GSN_ACC_CONTOPCONF, signal, 1000, 
			signal->length());
    return;
  }

  jamEntry();
  lcpLocptr.i = signal->theData[0];
  ptrCheckGuard(lcpLocptr, clcpLocrecFileSize, lcpLocRecord);
  lcpLocptr.p->accContCounter = 1;

  lcpPtr.i = lcpLocptr.p->masterLcpRec;
  ptrCheckGuard(lcpPtr, clcpFileSize, lcpRecord);
  lcpLocptr.i = lcpPtr.p->firstLcpLocAcc;
  do {
    ptrCheckGuard(lcpLocptr, clcpLocrecFileSize, lcpLocRecord);
    if (lcpLocptr.p->accContCounter == 0) {
      jam();
      return;
    }//if
    lcpLocptr.i = lcpLocptr.p->nextLcpLoc;
  } while (lcpLocptr.i != RNIL);
  fragptr.i = lcpPtr.p->currentFragment.fragPtrI;
  ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
  restartOperationsLab(signal);
  return;
}//Dblqh::execACC_CONTOPCONF()

/* ********************************************************* */
/*  LQH_RESTART_OP: Restart operations after beeing blocked. */
/* ********************************************************* */
/*---------------------------------------------------------------------------*/
/*       PRECONDITION: FRAG_STATUS = BLOCKED AND LCP_STATE = STARTED         */
/*---------------------------------------------------------------------------*/
void Dblqh::execLQH_RESTART_OP(Signal* signal) 
{
  jamEntry();
  fragptr.i = signal->theData[0];
  ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);

  lcpPtr.i = signal->theData[1];
  ptrCheckGuard(lcpPtr, clcpFileSize, lcpRecord);
  ndbrequire(fragptr.p->fragStatus == Fragrecord::BLOCKED);
  restartOperationsLab(signal);
}//Dblqh::execLQH_RESTART_OP()

void Dblqh::restartOperationsLab(Signal* signal) 
{
  Uint32 loopCount = 0;
  tcConnectptr.i = fragptr.p->firstWaitQueue;
  do {
    if (tcConnectptr.i != RNIL) {
      jam();
/*---------------------------------------------------------------------------*/
/*    START UP THE TRANSACTION AGAIN. WE START IT AS A SEPARATE SIGNAL.      */
/*---------------------------------------------------------------------------*/
      signal->theData[0] = ZRESTART_OPERATIONS_AFTER_STOP;
      signal->theData[1] = tcConnectptr.i;
      signal->theData[2] = fragptr.i;
      sendSignal(cownref, GSN_CONTINUEB, signal, 3, JBB);
      ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
      tcConnectptr.i = tcConnectptr.p->nextTc;
    } else {
      jam();
/*--------------------------------------------------------------------------*/
/* NO MORE OPERATIONS TO RESTART. WE CAN NOW RESET THE STATE TO ACTIVE AND  */
/* RESTART NORMAL ACTIVITIES ON THE FRAGMENT WHILE THE FUZZY PART OF THE    */
/* LOCAL CHECKPOINT IS COMPLETING.                                          */
/* IF THE CHECKPOINT WAS COMPLETED ALREADY ON THIS FRAGMENT WE PROCEED WITH */
/* THE NEXT FRAGMENT NOW THAT WE HAVE COMPLETED THIS CHECKPOINT.            */
/*--------------------------------------------------------------------------*/
      fragptr.p->fragStatus = Fragrecord::FSACTIVE;
      if (lcpPtr.p->lcpState == LcpRecord::LCP_BLOCKED_COMP) {
        jam();
        contChkpNextFragLab(signal);
        return;
      }//if
      return;
    }//if
    loopCount++;
    if (loopCount > 16) {
      jam();
      signal->theData[0] = fragptr.i;
      signal->theData[1] = lcpPtr.i;
      sendSignal(cownref, GSN_LQH_RESTART_OP, signal, 2, JBB);
      return;
    }//if
  } while (1);
}//Dblqh::restartOperationsLab()

void Dblqh::restartOperationsAfterStopLab(Signal* signal) 
{
  /*-------------------------------------------------------------------------
   * WHEN ARRIVING HERE THE OPERATION IS ALREADY SET IN THE ACTIVE LIST. 
   * THUS WE CAN IMMEDIATELY CALL THE METHODS THAT EXECUTE FROM WHERE 
   * THE OPERATION WAS STOPPED.
   *------------------------------------------------------------------------ */
  switch (tcConnectptr.p->transactionState) {
  case TcConnectionrec::STOPPED:
    jam();
    /*-----------------------------------------------------------------------
     *       STOPPED BEFORE TRYING TO SEND ACCKEYREQ           
     *---------------------------------------------------------------------- */
    prepareContinueAfterBlockedLab(signal);
    return;
    break;
  case TcConnectionrec::COMMIT_STOPPED:
    jam();
    /* ----------------------------------------------------------------------
     *       STOPPED BEFORE TRYING TO SEND ACC_COMMITREQ
     * --------------------------------------------------------------------- */
    releaseActiveFrag(signal);
    commitContinueAfterBlockedLab(signal);
    return;
    break;
  case TcConnectionrec::ABORT_STOPPED:
    jam();
    /* ----------------------------------------------------------------------
     *       STOPPED BEFORE TRYING TO SEND ACC_ABORTREQ
     * --------------------------------------------------------------------- */
    abortContinueAfterBlockedLab(signal, true);
    return;
    break;
  case TcConnectionrec::COPY_STOPPED:
    jam();
    /* ----------------------------------------------------------------------
     *       STOPPED BEFORE TRYING TO SEND NEXT_SCANREQ DURING COPY FRAGMENT
     * --------------------------------------------------------------------- */
    continueCopyAfterBlockedLab(signal);
    return;
    break;
  case TcConnectionrec::COPY_FIRST_STOPPED:
    jam();
    /* ----------------------------------------------------------------------
     *       STOPPED BEFORE TRYING TO SEND NEXT_SCANREQ DURING COPY FRAGMENT
     * --------------------------------------------------------------------- */
    continueFirstCopyAfterBlockedLab(signal);
    return;
    break;
  case TcConnectionrec::SCAN_FIRST_STOPPED:
    jam();
    /* ----------------------------------------------------------------------
     *       STOPPED BEFORE TRYING TO SEND NEXT_SCANREQ DURING SCAN
     * --------------------------------------------------------------------- */
    tcConnectptr.p->transactionState = TcConnectionrec::SCAN_STATE_USED;
    continueFirstScanAfterBlockedLab(signal);
    return;
    break;
  case TcConnectionrec::SCAN_CHECK_STOPPED:
    jam();
    /* ----------------------------------------------------------------------
     *       STOPPED BEFORE TRYING TO SEND NEXT_SCANREQ DURING SCAN
     * --------------------------------------------------------------------- */
    tcConnectptr.p->transactionState = TcConnectionrec::SCAN_STATE_USED;
    continueAfterCheckLcpStopBlocked(signal);
    return;
    break;
  case TcConnectionrec::SCAN_STOPPED:
    jam();
    /* ----------------------------------------------------------------------
     *       STOPPED BEFORE TRYING TO SEND NEXT_SCANREQ DURING SCAN
     * --------------------------------------------------------------------- */
    tcConnectptr.p->transactionState = TcConnectionrec::SCAN_STATE_USED;
    continueScanAfterBlockedLab(signal);
    return;
    break;
  case TcConnectionrec::SCAN_RELEASE_STOPPED:
    jam();
    /* ----------------------------------------------------------------------
     *       STOPPED BEFORE TRYING TO SEND NEXT_SCANREQ DURING RELEASE 
     *       LOCKS IN SCAN  
     * --------------------------------------------------------------------- */
    tcConnectptr.p->transactionState = TcConnectionrec::SCAN_STATE_USED;
    continueScanReleaseAfterBlockedLab(signal);
    return;
    break;
  case TcConnectionrec::SCAN_CLOSE_STOPPED:
    jam();
    /* ----------------------------------------------------------------------
     *       STOPPED BEFORE TRYING TO SEND NEXT_SCANREQ DURING CLOSE OF SCAN
     * --------------------------------------------------------------------- */
    continueCloseScanAfterBlockedLab(signal);
    return;
    break;
  case TcConnectionrec::COPY_CLOSE_STOPPED:
    jam();
    /* ----------------------------------------------------------------------
     *       STOPPED BEFORE TRYING TO SEND NEXT_SCANREQ DURING CLOSE OF COPY
     * --------------------------------------------------------------------- */
    continueCloseCopyAfterBlockedLab(signal);
    return;
    break;
  default:
    jam();
    systemErrorLab(signal);
    return;
    break;
  }//switch
}//Dblqh::restartOperationsAfterStopLab()

/* *************** */
/*  ACC_LCPCONF  > */
/* *************** */
/*---------------------------------------------------------------------------
 *       PRECONDITION: LCP_LOCPTR:LCP_LOCSTATE = ACC_STARTED 
 *-------------------------------------------------------------------------- */
void Dblqh::execACC_LCPCONF(Signal* signal) 
{
  jamEntry();
  lcpLocptr.i = signal->theData[0];
  ptrCheckGuard(lcpLocptr, clcpLocrecFileSize, lcpLocRecord);
  ndbrequire(lcpLocptr.p->lcpLocstate == LcpLocRecord::ACC_STARTED);

  lcpPtr.i = lcpLocptr.p->masterLcpRec;
  ptrCheckGuard(lcpPtr, clcpFileSize, lcpRecord);
  /* ------------------------------------------------------------------------
   *   NO ERROR CHECK ON USING VALUE IN MASTER_LCP_REC. ERROR IN 
   *   THIS REFERENCE WILL CAUSE POINTER OUT OF RANGE WHICH CAUSES A 
   *   SYSTEM RESTART.
   * ----------------------------------------------------------------------- */
  lcpLocptr.p->lcpLocstate = LcpLocRecord::ACC_COMPLETED;
  lcpCompletedLab(signal);
  return;
}//Dblqh::execACC_LCPCONF()

/* *************** */
/*  TUP_LCPCONF  > */
/* *************** */
/* --------------------------------------------------------------------------
 *       PRECONDITION: LCP_LOCPTR:LCP_LOCSTATE = TUP_STARTED    
 * ------------------------------------------------------------------------- */
void Dblqh::execTUP_LCPCONF(Signal* signal) 
{
  jamEntry();
  lcpLocptr.i = signal->theData[0];
  ptrCheckGuard(lcpLocptr, clcpLocrecFileSize, lcpLocRecord);
  ndbrequire(lcpLocptr.p->lcpLocstate == LcpLocRecord::TUP_STARTED);

  lcpPtr.i = lcpLocptr.p->masterLcpRec;
  ptrCheckGuard(lcpPtr, clcpFileSize, lcpRecord);
  /* ------------------------------------------------------------------------
   *  NO ERROR CHECK ON USING VALUE IN MASTER_LCP_REC. ERROR IN THIS
   *  REFERENCE WILL CAUSE POINTER OUT OF RANGE WHICH CAUSES A SYSTEM RESTART.
   * ----------------------------------------------------------------------- */
  lcpLocptr.p->lcpLocstate = LcpLocRecord::TUP_COMPLETED;
  lcpCompletedLab(signal);
  return;
}//Dblqh::execTUP_LCPCONF()

void Dblqh::lcpCompletedLab(Signal* signal) 
{
  checkLcpCompleted(signal);
  if (lcpPtr.p->lcpState != LcpRecord::LCP_COMPLETED) {
    jam();
    /* ----------------------------------------------------------------------
     *       THE LOCAL CHECKPOINT HAS NOT BEEN COMPLETED, EXIT & WAIT 
     *       FOR MORE SIGNALS 
     * --------------------------------------------------------------------- */
    return;
  }//if
  /* ------------------------------------------------------------------------
   *   THE LOCAL CHECKPOINT HAS BEEN COMPLETED. IT IS NOW TIME TO START 
   *   A LOCAL CHECKPOINT ON THE NEXT FRAGMENT OR COMPLETE THIS LCP ROUND.
   * ------------------------------------------------------------------------ 
   *   WE START BY SENDING LCP_REPORT TO DIH TO REPORT THE COMPLETED LCP.
   *   TO CATER FOR NODE CRASHES WE SEND IT IN PARALLEL TO ALL NODES.
   * ----------------------------------------------------------------------- */
  fragptr.i = lcpPtr.p->currentFragment.fragPtrI;
  ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
  fragptr.p->fragActiveStatus = ZFALSE;

  contChkpNextFragLab(signal);
  return;
}//Dblqh::lcpCompletedLab()

void
Dblqh::sendLCP_FRAG_REP(Signal * signal, 
			const LcpRecord::FragOrd & fragOrd) const {
  
  FragrecordPtr fragPtr;
  fragPtr.i = fragOrd.fragPtrI;
  ptrCheckGuard(fragPtr, cfragrecFileSize, fragrecord);

  ndbrequire(fragOrd.lcpFragOrd.lcpNo < MAX_LCP_STORED);
  LcpFragRep * const lcpReport = (LcpFragRep *)&signal->theData[0];
  lcpReport->nodeId = cownNodeid;
  lcpReport->lcpId = fragOrd.lcpFragOrd.lcpId;
  lcpReport->lcpNo = fragOrd.lcpFragOrd.lcpNo;
  lcpReport->tableId = fragOrd.lcpFragOrd.tableId;
  lcpReport->fragId = fragOrd.lcpFragOrd.fragmentId;
  lcpReport->maxGciCompleted = fragPtr.p->maxGciCompletedInLcp;
  lcpReport->maxGciStarted = fragPtr.p->maxGciInLcp;
  
  for (Uint32 i = 0; i < cnoOfNodes; i++) {
    jam();
    Uint32 nodeId = cnodeData[i];
    if(cnodeStatus[i] == ZNODE_UP){
      jam();
      BlockReference Tblockref = calcDihBlockRef(nodeId);
      sendSignal(Tblockref, GSN_LCP_FRAG_REP, signal, 
		 LcpFragRep::SignalLength, JBB);
    }//if
  }//for
}

void Dblqh::contChkpNextFragLab(Signal* signal) 
{
  /* ------------------------------------------------------------------------ 
   *       UPDATE THE LATEST LOCAL CHECKPOINT COMPLETED ON FRAGMENT.
   *       UPDATE THE LCP_ID OF THIS CHECKPOINT.
   *       REMOVE THE LINK BETWEEN THE FRAGMENT RECORD AND THE LCP RECORD.
   * ----------------------------------------------------------------------- */
  if (fragptr.p->fragStatus == Fragrecord::BLOCKED) {
    jam();
    /**
     * LCP of fragment complete
     *   but restarting of operations isn't
     */
    lcpPtr.p->lcpState = LcpRecord::LCP_BLOCKED_COMP;
    //restartOperationsLab(signal);
    return;
  }//if

  /**
   * Send rep when fragment is done + unblocked
   */
  sendLCP_FRAG_REP(signal, lcpPtr.p->currentFragment);
  
  /* ------------------------------------------------------------------------
   *       WE ALSO RELEASE THE LOCAL LCP RECORDS.
   * ----------------------------------------------------------------------- */
  releaseLocalLcps(signal);
  if (lcpPtr.p->lcpQueued) {
    jam();
    /* ----------------------------------------------------------------------
     *  Transfer the state from the queued to the active LCP.
     * --------------------------------------------------------------------- */
    lcpPtr.p->lcpQueued = false;
    lcpPtr.p->currentFragment = lcpPtr.p->queuedFragment;
    
    /* ----------------------------------------------------------------------
     *       START THE QUEUED LOCAL CHECKPOINT.
     * --------------------------------------------------------------------- */
    sendLCP_FRAGIDREQ(signal);
    return;
  }//if
  
  lcpPtr.p->lcpState = LcpRecord::LCP_IDLE;
  if (lcpPtr.p->lastFragmentFlag){
    jam();
    /* ----------------------------------------------------------------------
     *       NOW THE COMPLETE LOCAL CHECKPOINT ROUND IS COMPLETED.  
     * --------------------------------------------------------------------- */
    completeLcpRoundLab(signal);
    return;
  }//if
  
  if (lcpPtr.p->reportEmpty) {
    jam();
    sendEMPTY_LCP_CONF(signal, false);
  }//if
  return;
}//Dblqh::contChkpNextFragLab()

void Dblqh::sendLCP_FRAGIDREQ(Signal* signal)
{
  ndbrequire(lcpPtr.p->firstLcpLocTup == RNIL);
  ndbrequire(lcpPtr.p->firstLcpLocAcc == RNIL);
  
  TablerecPtr tabPtr;
  tabPtr.i = lcpPtr.p->currentFragment.lcpFragOrd.tableId;
  ptrAss(tabPtr, tablerec);
  if(tabPtr.p->tableStatus == Tablerec::PREP_DROP_TABLE_ONGOING ||
     tabPtr.p->tableStatus == Tablerec::PREP_DROP_TABLE_DONE){
    jam();
    /**
     * Fake that the fragment is done
     */
    lcpCompletedLab(signal);
    return;
  }
  
  ndbrequire(tabPtr.p->tableStatus == Tablerec::TABLE_DEFINED);
  
  lcpPtr.p->lcpState = LcpRecord::LCP_WAIT_FRAGID;
  signal->theData[0] = lcpPtr.i;
  signal->theData[1] = cownref;
  signal->theData[2] = lcpPtr.p->currentFragment.lcpFragOrd.lcpNo;
  signal->theData[3] = lcpPtr.p->currentFragment.lcpFragOrd.tableId;
  signal->theData[4] = lcpPtr.p->currentFragment.lcpFragOrd.fragmentId;
  signal->theData[5] = lcpPtr.p->currentFragment.lcpFragOrd.lcpId % MAX_LCP_STORED;
  sendSignal(fragptr.p->accBlockref, GSN_LCP_FRAGIDREQ, signal, 6, JBB);
}//Dblqh::sendLCP_FRAGIDREQ()

void Dblqh::sendEMPTY_LCP_CONF(Signal* signal, bool idle)
{
  
  EmptyLcpConf * const rep = (EmptyLcpConf*)&signal->theData[0];
  /* ----------------------------------------------------------------------
   *       We have been requested to report when there are no more local
   *       waiting to be started or ongoing. In this signal we also report
   *       the last completed fragments state.
   * ---------------------------------------------------------------------- */
  rep->senderNodeId = getOwnNodeId();
  if(!idle){
    jam();
    rep->idle = 0 ;
    rep->tableId = lcpPtr.p->currentFragment.lcpFragOrd.tableId;
    rep->fragmentId = lcpPtr.p->currentFragment.lcpFragOrd.fragmentId;
    rep->lcpNo = lcpPtr.p->currentFragment.lcpFragOrd.lcpNo;
    rep->lcpId = lcpPtr.p->currentFragment.lcpFragOrd.lcpId;
  } else {
    jam();
    rep->idle = 1;
    rep->tableId = ~0;
    rep->fragmentId = ~0;
    rep->lcpNo = ~0;
    rep->lcpId = c_lcpId;
  }
  
  for (Uint32 i = 0; i < cnoOfNodes; i++) {
    jam();
    Uint32 nodeId = cnodeData[i];
    if (lcpPtr.p->m_EMPTY_LCP_REQ.get(nodeId)) {
      jam();
      
      BlockReference blockref = calcDihBlockRef(nodeId);
      sendSignal(blockref, GSN_EMPTY_LCP_CONF, signal, 
		 EmptyLcpConf::SignalLength, JBB);
    }//if
  }//for

  lcpPtr.p->reportEmpty = false;
  lcpPtr.p->m_EMPTY_LCP_REQ.clear();
}//Dblqh::sendEMPTY_LCPCONF()

void Dblqh::execACC_LCPREF(Signal* signal) 
{
  jamEntry();
  ndbrequire(false);
}//Dblqh::execACC_LCPREF()

void Dblqh::execTUP_LCPREF(Signal* signal) 
{
  jamEntry();
  ndbrequire(false);
}//Dblqh::execTUP_LCPREF()

/* --------------------------------------------------------------------------
 *       THE LOCAL CHECKPOINT ROUND IS NOW COMPLETED. SEND COMPLETED MESSAGE
 *       TO THE MASTER DIH.
 * ------------------------------------------------------------------------- */
void Dblqh::completeLcpRoundLab(Signal* signal)
{
  clcpCompletedState = LCP_CLOSE_STARTED;
  signal->theData[0] = caccBlockref;
  signal->theData[1] = cownref;
  sendSignal(caccBlockref, GSN_END_LCPREQ, signal, 2, JBB);
  signal->theData[0] = ctupBlockref;
  signal->theData[1] = cownref;
  sendSignal(ctupBlockref, GSN_END_LCPREQ, signal, 2, JBB);
  return;
}//Dblqh::completeLcpRoundLab()

void Dblqh::execEND_LCPCONF(Signal* signal) 
{
  jamEntry();
  BlockReference userpointer = signal->theData[0];
  if (userpointer == caccBlockref) {
    if (clcpCompletedState == LCP_CLOSE_STARTED) {
      jam();
      clcpCompletedState = ACC_LCP_CLOSE_COMPLETED;
      return;
    } else {
      jam();
      ndbrequire(clcpCompletedState == TUP_LCP_CLOSE_COMPLETED);
      clcpCompletedState = LCP_IDLE;
    }//if
  } else {
    ndbrequire(userpointer == ctupBlockref);
    if (clcpCompletedState == LCP_CLOSE_STARTED) {
      jam();
      clcpCompletedState = TUP_LCP_CLOSE_COMPLETED;
      return;
    } else {
      jam();
      ndbrequire(clcpCompletedState == ACC_LCP_CLOSE_COMPLETED);
      clcpCompletedState = LCP_IDLE;
    }//if
  }//if
  lcpPtr.i = 0;
  ptrAss(lcpPtr, lcpRecord);
  sendLCP_COMPLETE_REP(signal, lcpPtr.p->currentFragment.lcpFragOrd.lcpId);
}//Dblqh::execEND_LCPCONF()

void Dblqh::sendLCP_COMPLETE_REP(Signal* signal, Uint32 lcpId)
{
  cnoOfFragsCheckpointed = 0;
  ndbrequire((cnoOfNodes - 1) < (MAX_NDB_NODES - 1));
  /* ------------------------------------------------------------------------
   *       WE SEND COMP_LCP_ROUND TO ALL NODES TO PREPARE FOR NODE CRASHES.
   * ----------------------------------------------------------------------- */
  lcpPtr.i = 0;
  ptrAss(lcpPtr, lcpRecord);
  lcpPtr.p->lastFragmentFlag = false;
  
  LcpCompleteRep* rep = (LcpCompleteRep*)signal->getDataPtrSend();
  rep->nodeId = getOwnNodeId();
  rep->lcpId = lcpId;
  rep->blockNo = DBLQH;
  
  for (Uint32 i = 0; i < cnoOfNodes; i++) {
    jam();
    Uint32 nodeId = cnodeData[i];
    if(cnodeStatus[i] == ZNODE_UP){
      jam();
      
      BlockReference blockref = calcDihBlockRef(nodeId);
      sendSignal(blockref, GSN_LCP_COMPLETE_REP, signal, 
		 LcpCompleteRep::SignalLength, JBB);
    }//if
  }//for

  if(lcpPtr.p->reportEmpty){
    jam();
    sendEMPTY_LCP_CONF(signal, true);
  }
  return;
}//Dblqh::sendCOMP_LCP_ROUND()

/* ==========================================================================
 * =======  CHECK IF ALL PARTS OF A LOCAL CHECKPOINT ARE COMPLETED    ======= 
 *
 *       SUBROUTINE SHORT NAME = CLC
 * ========================================================================= */
void Dblqh::checkLcpCompleted(Signal* signal) 
{
  LcpLocRecordPtr clcLcpLocptr;

  clcLcpLocptr.i = lcpPtr.p->firstLcpLocAcc;
  while (clcLcpLocptr.i != RNIL) {
    ptrCheckGuard(clcLcpLocptr, clcpLocrecFileSize, lcpLocRecord);
    if (clcLcpLocptr.p->lcpLocstate != LcpLocRecord::ACC_COMPLETED) {
      jam();
      ndbrequire((clcLcpLocptr.p->lcpLocstate == LcpLocRecord::ACC_WAIT_STARTED) ||
                 (clcLcpLocptr.p->lcpLocstate == LcpLocRecord::ACC_STARTED));
      return;
    }//if
    clcLcpLocptr.i = clcLcpLocptr.p->nextLcpLoc;
  }

  clcLcpLocptr.i = lcpPtr.p->firstLcpLocTup;
  while (clcLcpLocptr.i != RNIL){
    ptrCheckGuard(clcLcpLocptr, clcpLocrecFileSize, lcpLocRecord);
    if (clcLcpLocptr.p->lcpLocstate != LcpLocRecord::TUP_COMPLETED) {
      jam();
      ndbrequire((clcLcpLocptr.p->lcpLocstate==LcpLocRecord::TUP_WAIT_STARTED) 
		 ||(clcLcpLocptr.p->lcpLocstate == LcpLocRecord::TUP_STARTED));
      return;
    }//if
    clcLcpLocptr.i = clcLcpLocptr.p->nextLcpLoc;
  }
  
  lcpPtr.p->lcpState = LcpRecord::LCP_COMPLETED;
}//Dblqh::checkLcpCompleted()

/* ========================================================================== 
 * =======              CHECK IF ALL HOLD OPERATIONS ARE COMPLETED    ======= 
 *
 *       SUBROUTINE SHORT NAME = CHO
 * ========================================================================= */
void Dblqh::checkLcpHoldop(Signal* signal) 
{
  LcpLocRecordPtr choLcpLocptr;

  choLcpLocptr.i = lcpPtr.p->firstLcpLocAcc;
  do {
    ptrCheckGuard(choLcpLocptr, clcpLocrecFileSize, lcpLocRecord);
    if (choLcpLocptr.p->lcpLocstate != LcpLocRecord::HOLDOP_READY) {
      ndbrequire(choLcpLocptr.p->lcpLocstate == LcpLocRecord::WAIT_LCPHOLDOP);
      return;
    }//if
    choLcpLocptr.i = choLcpLocptr.p->nextLcpLoc;
  } while (choLcpLocptr.i != RNIL);
  lcpPtr.p->lcpState = LcpRecord::LCP_WAIT_ACTIVE_FINISH;
}//Dblqh::checkLcpHoldop()

/* ========================================================================== 
 * =======  CHECK IF ALL PARTS OF A LOCAL CHECKPOINT ARE STARTED      ======= 
 *
 *       SUBROUTINE SHORT NAME = CLS
 * ========================================================================== */
bool
Dblqh::checkLcpStarted(Signal* signal) 
{
  LcpLocRecordPtr clsLcpLocptr;

  terrorCode = ZOK;
  clsLcpLocptr.i = lcpPtr.p->firstLcpLocAcc;
  int i = 0;
  do {
    ptrCheckGuard(clsLcpLocptr, clcpLocrecFileSize, lcpLocRecord);
    if (clsLcpLocptr.p->lcpLocstate == LcpLocRecord::ACC_WAIT_STARTED){
      return false;
    }//if
    clsLcpLocptr.i = clsLcpLocptr.p->nextLcpLoc;
    i++;
  } while (clsLcpLocptr.i != RNIL);

  i = 0;
  clsLcpLocptr.i = lcpPtr.p->firstLcpLocTup;
  do {
    ptrCheckGuard(clsLcpLocptr, clcpLocrecFileSize, lcpLocRecord);
    if (clsLcpLocptr.p->lcpLocstate == LcpLocRecord::TUP_WAIT_STARTED){
      return false;
    }//if
    clsLcpLocptr.i = clsLcpLocptr.p->nextLcpLoc;
    i++;
  } while (clsLcpLocptr.i != RNIL);
  
  return true;
}//Dblqh::checkLcpStarted()

/* ========================================================================== 
 * =======       CHECK IF ALL PREPARE TUP OPERATIONS ARE COMPLETED    =======
 *
 *       SUBROUTINE SHORT NAME = CLT
 * ========================================================================== */
void Dblqh::checkLcpTupprep(Signal* signal) 
{
  LcpLocRecordPtr cltLcpLocptr;
  cltLcpLocptr.i = lcpPtr.p->firstLcpLocTup;
  do {
    ptrCheckGuard(cltLcpLocptr, clcpLocrecFileSize, lcpLocRecord);
    if (cltLcpLocptr.p->lcpLocstate != LcpLocRecord::IDLE) {
      ndbrequire(cltLcpLocptr.p->lcpLocstate == LcpLocRecord::WAIT_TUP_PREPLCP);
      return;
    }//if
    cltLcpLocptr.i = cltLcpLocptr.p->nextLcpLoc;
  } while (cltLcpLocptr.i != RNIL);
  lcpPtr.p->lcpState = LcpRecord::LCP_WAIT_HOLDOPS;
}//Dblqh::checkLcpTupprep()

/* ========================================================================== 
 * =======            INITIATE LCP LOCAL RECORD USED TOWARDS ACC      ======= 
 *
 * ========================================================================== */
void Dblqh::initLcpLocAcc(Signal* signal, Uint32 fragId) 
{
  lcpLocptr.p->nextLcpLoc = lcpPtr.p->firstLcpLocAcc;
  lcpPtr.p->firstLcpLocAcc = lcpLocptr.i;
  lcpLocptr.p->locFragid = fragId;
  lcpLocptr.p->waitingBlock = LcpLocRecord::ACC;
  lcpLocptr.p->lcpLocstate = LcpLocRecord::IDLE;
  lcpLocptr.p->masterLcpRec = lcpPtr.i;
  lcpLocptr.p->tupRef = RNIL;
}//Dblqh::initLcpLocAcc()

/* ========================================================================== 
 * =======           INITIATE LCP LOCAL RECORD USED TOWARDS TUP       ======= 
 *
 * ========================================================================== */
void Dblqh::initLcpLocTup(Signal* signal, Uint32 fragId) 
{
  lcpLocptr.p->nextLcpLoc = lcpPtr.p->firstLcpLocTup;
  lcpPtr.p->firstLcpLocTup = lcpLocptr.i;
  lcpLocptr.p->locFragid = fragId;
  lcpLocptr.p->waitingBlock = LcpLocRecord::TUP;
  lcpLocptr.p->lcpLocstate = LcpLocRecord::WAIT_TUP_PREPLCP;
  lcpLocptr.p->masterLcpRec = lcpPtr.i;
  lcpLocptr.p->tupRef = RNIL;
}//Dblqh::initLcpLocTup()

/* --------------------------------------------------------------------------
 * -------         MOVE OPERATION FROM ACC WAITING LIST ON FRAGMENT   ------- 
 * -------               TO ACTIVE LIST ON FRAGMENT                   -------
 *
 *       SUBROUTINE SHORT NAME = MAA
 * -------------------------------------------------------------------------- */
void Dblqh::moveAccActiveFrag(Signal* signal) 
{
  UintR maaTcNextConnectptr;

  tcConnectptr.i = fragptr.p->accBlockedList;
  fragptr.p->accBlockedList = RNIL;
  /* ------------------------------------------------------------------------
   *       WE WILL MOVE ALL RECORDS FROM THE ACC BLOCKED LIST AT ONCE.
   * ------------------------------------------------------------------------ */
  while (tcConnectptr.i != RNIL) {
    jam();
    ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
    maaTcNextConnectptr = tcConnectptr.p->nextTc;
    ndbrequire(tcConnectptr.p->listState == TcConnectionrec::ACC_BLOCK_LIST);
    tcConnectptr.p->listState = TcConnectionrec::NOT_IN_LIST;
    linkActiveFrag(signal);
    tcConnectptr.i = maaTcNextConnectptr;
  }//while
}//Dblqh::moveAccActiveFrag()

/* -------------------------------------------------------------------------- 
 * -------               MOVE OPERATION FROM ACTIVE LIST ON FRAGMENT  ------- 
 * -------               TO ACC BLOCKED LIST ON FRAGMENT              -------
 *
 *       SUBROUTINE SHORT NAME = MAT
 * -------------------------------------------------------------------------- */
void Dblqh::moveActiveToAcc(Signal* signal) 
{
  TcConnectionrecPtr matTcNextConnectptr;

  releaseActiveList(signal);
  /* ------------------------------------------------------------------------
   *       PUT OPERATION RECORD FIRST IN ACC BLOCKED LIST.
   * ------------------------------------------------------------------------ */
  matTcNextConnectptr.i = fragptr.p->accBlockedList;
  tcConnectptr.p->nextTc = matTcNextConnectptr.i;
  tcConnectptr.p->prevTc = RNIL;
  tcConnectptr.p->listState = TcConnectionrec::ACC_BLOCK_LIST;
  fragptr.p->accBlockedList = tcConnectptr.i;
  if (matTcNextConnectptr.i != RNIL) {
    jam();
    ptrCheckGuard(matTcNextConnectptr, ctcConnectrecFileSize, tcConnectionrec);
    matTcNextConnectptr.p->prevTc = tcConnectptr.i;
  }//if
}//Dblqh::moveActiveToAcc()

/* ------------------------------------------------------------------------- */
/* ---- RELEASE LOCAL LCP RECORDS AFTER COMPLETION OF A LOCAL CHECKPOINT---- */
/*                                                                           */
/*       SUBROUTINE SHORT NAME = RLL                                         */
/* ------------------------------------------------------------------------- */
void Dblqh::releaseLocalLcps(Signal* signal) 
{
  lcpLocptr.i = lcpPtr.p->firstLcpLocAcc;
  while (lcpLocptr.i != RNIL){
    ptrCheckGuard(lcpLocptr, clcpLocrecFileSize, lcpLocRecord);
    Uint32 tmp = lcpLocptr.p->nextLcpLoc;
    releaseLcpLoc(signal);
    lcpLocptr.i = tmp;
  } 
  lcpPtr.p->firstLcpLocAcc = RNIL;
  
  lcpLocptr.i = lcpPtr.p->firstLcpLocTup;
  while (lcpLocptr.i != RNIL){
    ptrCheckGuard(lcpLocptr, clcpLocrecFileSize, lcpLocRecord);
    Uint32 tmp = lcpLocptr.p->nextLcpLoc;
    releaseLcpLoc(signal);
    lcpLocptr.i = tmp;
  } 
  lcpPtr.p->firstLcpLocTup = RNIL;
  
}//Dblqh::releaseLocalLcps()

/* ------------------------------------------------------------------------- */
/* -------       SEIZE LCP LOCAL RECORD                              ------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void Dblqh::seizeLcpLoc(Signal* signal) 
{
  lcpLocptr.i = cfirstfreeLcpLoc;
  ptrCheckGuard(lcpLocptr, clcpLocrecFileSize, lcpLocRecord);
  cfirstfreeLcpLoc = lcpLocptr.p->nextLcpLoc;
  lcpLocptr.p->nextLcpLoc = RNIL;
}//Dblqh::seizeLcpLoc()

/* ------------------------------------------------------------------------- */
/* -------               SEND ACC_CONT_OP                            ------- */
/*                                                                           */
/*       INPUT:          LCP_PTR         LOCAL CHECKPOINT RECORD             */
/*                       FRAGPTR         FRAGMENT RECORD                     */
/*                                                                           */
/*       SUBROUTINE SHORT NAME = SAC                                         */
/* ------------------------------------------------------------------------- */
void Dblqh::sendAccContOp(Signal* signal) 
{
  LcpLocRecordPtr sacLcpLocptr;

  int count = 0;
  sacLcpLocptr.i = lcpPtr.p->firstLcpLocAcc;
  do {
    ptrCheckGuard(sacLcpLocptr, clcpLocrecFileSize, lcpLocRecord);
    sacLcpLocptr.p->accContCounter = 0;
    /* ------------------------------------------------------------------- */
    /*SEND START OPERATIONS TO ACC AGAIN                                   */
    /* ------------------------------------------------------------------- */
    signal->theData[0] = lcpPtr.p->lcpAccptr;
    signal->theData[1] = sacLcpLocptr.p->locFragid;
    sendSignal(fragptr.p->accBlockref, GSN_ACC_CONTOPREQ, signal, 2, JBA);
    sacLcpLocptr.i = sacLcpLocptr.p->nextLcpLoc;
  } while (sacLcpLocptr.i != RNIL);
  
}//Dblqh::sendAccContOp()

/* ------------------------------------------------------------------------- */
/* -------               SEND ACC_LCPREQ AND TUP_LCPREQ              ------- */
/*                                                                           */
/*       INPUT:          LCP_PTR             LOCAL CHECKPOINT RECORD         */
/*                       FRAGPTR             FRAGMENT RECORD                 */
/*       SUBROUTINE SHORT NAME = STL                                         */
/* ------------------------------------------------------------------------- */
void Dblqh::sendStartLcp(Signal* signal) 
{
  LcpLocRecordPtr stlLcpLocptr;
  stlLcpLocptr.i = lcpPtr.p->firstLcpLocAcc;
  do {
    jam();
    ptrCheckGuard(stlLcpLocptr, clcpLocrecFileSize, lcpLocRecord);
    stlLcpLocptr.p->lcpLocstate = LcpLocRecord::ACC_WAIT_STARTED;
    signal->theData[0] = lcpPtr.p->lcpAccptr;
    signal->theData[1] = stlLcpLocptr.i;
    signal->theData[2] = stlLcpLocptr.p->locFragid;
    sendSignal(fragptr.p->accBlockref, GSN_ACC_LCPREQ, signal, 3, JBA);
    stlLcpLocptr.i = stlLcpLocptr.p->nextLcpLoc;
  } while (stlLcpLocptr.i != RNIL);

  stlLcpLocptr.i = lcpPtr.p->firstLcpLocTup;
  do {
    jam();
    ptrCheckGuard(stlLcpLocptr, clcpLocrecFileSize, lcpLocRecord);
    stlLcpLocptr.p->lcpLocstate = LcpLocRecord::TUP_WAIT_STARTED;
    signal->theData[0] = stlLcpLocptr.i;
    signal->theData[1] = cownref;
    signal->theData[2] = stlLcpLocptr.p->tupRef;
    if(ERROR_INSERTED(5077))
      sendSignalWithDelay(fragptr.p->tupBlockref, GSN_TUP_LCPREQ, 
			  signal, 5000, 3);
    else
      sendSignal(fragptr.p->tupBlockref, GSN_TUP_LCPREQ, signal, 3, JBA);
    stlLcpLocptr.i = stlLcpLocptr.p->nextLcpLoc;
  } while (stlLcpLocptr.i != RNIL);

  if(ERROR_INSERTED(5077))
  {
    ndbout_c("Delayed TUP_LCPREQ with 5 sec");
  }
}//Dblqh::sendStartLcp()

/* ------------------------------------------------------------------------- */
/* -------               SET THE LOG TAIL IN THE LOG FILES           ------- */
/*                                                                           */
/*THIS SUBROUTINE HAVE BEEN BUGGY AND IS RATHER COMPLEX. IT IS IMPORTANT TO  */
/*REMEMBER THAT WE SEARCH FROM THE TAIL UNTIL WE REACH THE HEAD (CURRENT).   */
/*THE TAIL AND HEAD CAN BE ON THE SAME MBYTE. WE SEARCH UNTIL WE FIND A MBYTE*/
/*THAT WE NEED TO KEEP. WE THEN SET THE TAIL TO BE THE PREVIOUS. IF WE DO    */
/*NOT FIND A MBYTE THAT WE NEED TO KEEP UNTIL WE REACH THE HEAD THEN WE USE  */
/*THE HEAD AS TAIL. FINALLY WE HAVE TO MOVE BACK THE TAIL TO ALSO INCLUDE    */
/*ALL PREPARE RECORDS. THIS MEANS THAT LONG-LIVED TRANSACTIONS ARE DANGEROUS */
/*FOR SHORT LOGS.                                                            */
/* ------------------------------------------------------------------------- */

// this function has not been verified yet
Uint32 Dblqh::remainingLogSize(const LogFileRecordPtr &sltCurrLogFilePtr,
			       const LogPartRecordPtr &sltLogPartPtr)
{
  Uint32 hf = sltCurrLogFilePtr.p->fileNo*ZNO_MBYTES_IN_FILE+sltCurrLogFilePtr.p->currentMbyte;
  Uint32 tf = sltLogPartPtr.p->logTailFileNo*ZNO_MBYTES_IN_FILE+sltLogPartPtr.p->logTailMbyte;
  Uint32 sz = sltLogPartPtr.p->noLogFiles*ZNO_MBYTES_IN_FILE;
  if (tf > hf) hf += sz;
  return sz-(hf-tf);
}

void Dblqh::setLogTail(Signal* signal, Uint32 keepGci) 
{
  LogPartRecordPtr sltLogPartPtr;
  LogFileRecordPtr sltLogFilePtr;
#if 0
  LogFileRecordPtr sltCurrLogFilePtr;
#endif
  UintR tsltMbyte;
  UintR tsltStartMbyte;
  UintR tsltIndex;
  UintR tsltFlag;

  for (sltLogPartPtr.i = 0; sltLogPartPtr.i < 4; sltLogPartPtr.i++) {
    jam();
    ptrAss(sltLogPartPtr, logPartRecord);
    findLogfile(signal, sltLogPartPtr.p->logTailFileNo,
                sltLogPartPtr, &sltLogFilePtr);

#if 0
    sltCurrLogFilePtr.i = sltLogPartPtr.p->currentLogfile;
    ptrCheckGuard(sltCurrLogFilePtr, clogFileFileSize, logFileRecord);
    infoEvent("setLogTail: Available log file %d size = %d[mbytes]+%d[words]", sltLogPartPtr.i,
	      remainingLogSize(sltCurrLogFilePtr, sltLogPartPtr), sltCurrLogFilePtr.p->remainingWordsInMbyte);
#endif

    tsltMbyte = sltLogPartPtr.p->logTailMbyte;
    tsltStartMbyte = tsltMbyte;
    tsltFlag = ZFALSE;
    if (sltLogFilePtr.i == sltLogPartPtr.p->currentLogfile) {
/* ------------------------------------------------------------------------- */
/*THE LOG AND THE TAIL IS ALREADY IN THE SAME FILE.                          */
/* ------------------------------------------------------------------------- */
      if (sltLogFilePtr.p->currentMbyte >= sltLogPartPtr.p->logTailMbyte) {
        jam();
/* ------------------------------------------------------------------------- */
/*THE CURRENT MBYTE IS AHEAD OF OR AT THE TAIL. THUS WE WILL ONLY LOOK FOR   */
/*THE TAIL UNTIL WE REACH THE CURRENT MBYTE WHICH IS IN THIS LOG FILE.       */
/*IF THE LOG TAIL IS AHEAD OF THE CURRENT MBYTE BUT IN THE SAME LOG FILE     */
/*THEN WE HAVE TO SEARCH THROUGH ALL FILES BEFORE WE COME TO THE CURRENT     */
/*MBYTE. WE ALWAYS STOP WHEN WE COME TO THE CURRENT MBYTE SINCE THE TAIL     */
/*CAN NEVER BE BEFORE THE HEAD.                                              */
/* ------------------------------------------------------------------------- */
        tsltFlag = ZTRUE;
      }//if
    }//if

/* ------------------------------------------------------------------------- */
/*NOW START SEARCHING FOR THE NEW TAIL, STARTING AT THE CURRENT TAIL AND     */
/*PROCEEDING UNTIL WE FIND A MBYTE WHICH IS NEEDED TO KEEP OR UNTIL WE REACH */
/*CURRENT MBYTE (THE HEAD).                                                  */
/* ------------------------------------------------------------------------- */
  SLT_LOOP:
    for (tsltIndex = tsltStartMbyte;
	 tsltIndex <= ZNO_MBYTES_IN_FILE - 1; 
	 tsltIndex++) {
      if (sltLogFilePtr.p->logMaxGciStarted[tsltIndex] >= keepGci) {
/* ------------------------------------------------------------------------- */
/*WE ARE NOT ALLOWED TO STEP THE LOG ANY FURTHER AHEAD                       */
/*SET THE NEW LOG TAIL AND CONTINUE WITH NEXT LOG PART.                      */
/*THIS MBYTE IS NOT TO BE INCLUDED SO WE NEED TO STEP BACK ONE MBYTE.        */
/* ------------------------------------------------------------------------- */
        if (tsltIndex != 0) {
          jam();
          tsltMbyte = tsltIndex - 1;
        } else {
          jam();
/* ------------------------------------------------------------------------- */
/*STEPPING BACK INCLUDES ALSO STEPPING BACK TO THE PREVIOUS LOG FILE.        */
/* ------------------------------------------------------------------------- */
          tsltMbyte = ZNO_MBYTES_IN_FILE - 1;
          sltLogFilePtr.i = sltLogFilePtr.p->prevLogFile;
          ptrCheckGuard(sltLogFilePtr, clogFileFileSize, logFileRecord);
        }//if
        goto SLT_BREAK;
      } else {
        jam();
        if (tsltFlag == ZTRUE) {
/* ------------------------------------------------------------------------- */
/*WE ARE IN THE SAME FILE AS THE CURRENT MBYTE AND WE CAN REACH THE CURRENT  */
/*MBYTE BEFORE WE REACH A NEW TAIL.                                          */
/* ------------------------------------------------------------------------- */
          if (tsltIndex == sltLogFilePtr.p->currentMbyte) {
            jam();
/* ------------------------------------------------------------------------- */
/*THE TAIL OF THE LOG IS ACTUALLY WITHIN THE CURRENT MBYTE. THUS WE SET THE  */
/*LOG TAIL TO BE THE CURRENT MBYTE.                                          */
/* ------------------------------------------------------------------------- */
            tsltMbyte = sltLogFilePtr.p->currentMbyte;
            goto SLT_BREAK;
          }//if
        }//if
      }//if
    }//for
    sltLogFilePtr.i = sltLogFilePtr.p->nextLogFile;
    ptrCheckGuard(sltLogFilePtr, clogFileFileSize, logFileRecord);
    if (sltLogFilePtr.i == sltLogPartPtr.p->currentLogfile) {
      jam();
      tsltFlag = ZTRUE;
    }//if
    tsltStartMbyte = 0;
    goto SLT_LOOP;
  SLT_BREAK:
    jam();
    {
      UintR ToldTailFileNo = sltLogPartPtr.p->logTailFileNo;
      UintR ToldTailMByte = sltLogPartPtr.p->logTailMbyte;

      arrGuard(tsltMbyte, 16);
      sltLogPartPtr.p->logTailFileNo = 
         sltLogFilePtr.p->logLastPrepRef[tsltMbyte] >> 16;
/* ------------------------------------------------------------------------- */
/*SINCE LOG_MAX_GCI_STARTED ONLY KEEP TRACK OF COMMIT LOG RECORDS WE ALSO    */
/*HAVE TO STEP BACK THE TAIL SO THAT WE INCLUDE ALL PREPARE RECORDS          */
/*NEEDED FOR THOSE COMMIT RECORDS IN THIS MBYTE. THIS IS A RATHER            */
/*CONSERVATIVE APPROACH BUT IT WORKS.                                        */
/* ------------------------------------------------------------------------- */
      sltLogPartPtr.p->logTailMbyte = 
        sltLogFilePtr.p->logLastPrepRef[tsltMbyte] & 65535;
      if ((ToldTailFileNo != sltLogPartPtr.p->logTailFileNo) ||
          (ToldTailMByte != sltLogPartPtr.p->logTailMbyte)) {
        jam();
        if (sltLogPartPtr.p->logPartState == LogPartRecord::TAIL_PROBLEM) {
          if (sltLogPartPtr.p->firstLogQueue == RNIL) {
            jam();
            sltLogPartPtr.p->logPartState = LogPartRecord::IDLE;
          } else {
            jam();
            sltLogPartPtr.p->logPartState = LogPartRecord::ACTIVE;
          }//if
        }//if
      }//if
    }
#if 0
    infoEvent("setLogTail: Available log file %d size = %d[mbytes]+%d[words]", sltLogPartPtr.i,
	      remainingLogSize(sltCurrLogFilePtr, sltLogPartPtr), sltCurrLogFilePtr.p->remainingWordsInMbyte);
#endif
  }//for

}//Dblqh::setLogTail()

/* ######################################################################### */
/* #######                       GLOBAL CHECKPOINT MODULE            ####### */
/*                                                                           */
/* ######################################################################### */
/*---------------------------------------------------------------------------*/
/* THIS MODULE HELPS DIH IN DISCOVERING WHEN GLOBAL CHECKPOINTS ARE          */
/* RECOVERABLE. IT HANDLES THE REQUEST GCP_SAVEREQ THAT REQUESTS LQH TO      */
/* SAVE A PARTICULAR GLOBAL CHECKPOINT TO DISK AND RESPOND WHEN COMPLETED.   */
/*---------------------------------------------------------------------------*/
/* *************** */
/*  GCP_SAVEREQ  > */
/* *************** */
void Dblqh::execGCP_SAVEREQ(Signal* signal) 
{
  jamEntry();
  const GCPSaveReq * const saveReq = (GCPSaveReq *)&signal->theData[0];

  if (ERROR_INSERTED(5000)) {
    systemErrorLab(signal);
  }

  if (ERROR_INSERTED(5007)){
    CLEAR_ERROR_INSERT_VALUE;
    sendSignalWithDelay(cownref, GSN_GCP_SAVEREQ, signal, 10000, 
			signal->length());
    return;
  }

  const Uint32 dihBlockRef = saveReq->dihBlockRef;
  const Uint32 dihPtr = saveReq->dihPtr;
  const Uint32 gci = saveReq->gci;
  
  ndbrequire(gci >= cnewestCompletedGci);
  
  if (gci == cnewestCompletedGci) {
/*---------------------------------------------------------------------------*/
/* GLOBAL CHECKPOINT HAVE ALREADY BEEN HANDLED. REQUEST MUST HAVE BEEN SENT  */
/* FROM NEW MASTER DIH.                                                      */
/*---------------------------------------------------------------------------*/
    if (ccurrentGcprec == RNIL) {
      jam();
/*---------------------------------------------------------------------------*/
/* THIS INDICATES THAT WE HAVE ALREADY SENT GCP_SAVECONF TO PREVIOUS MASTER. */
/* WE SIMPLY SEND IT ALSO TO THE NEW MASTER.                                 */
/*---------------------------------------------------------------------------*/
      GCPSaveConf * const saveConf = (GCPSaveConf*)&signal->theData[0];
      saveConf->dihPtr = dihPtr;
      saveConf->nodeId = getOwnNodeId();
      saveConf->gci    = cnewestCompletedGci;
      sendSignal(dihBlockRef, GSN_GCP_SAVECONF, signal, 
		 GCPSaveConf::SignalLength, JBA);
      return;
    }
    jam();
/*---------------------------------------------------------------------------*/
/* WE HAVE NOT YET SENT THE RESPONSE TO THE OLD MASTER. WE WILL SET THE NEW  */
/* RECEIVER OF THE RESPONSE AND THEN EXIT SINCE THE PROCESS IS ALREADY       */
/* STARTED.                                                                  */
/*---------------------------------------------------------------------------*/
    gcpPtr.i = ccurrentGcprec;
    ptrCheckGuard(gcpPtr, cgcprecFileSize, gcpRecord);
    gcpPtr.p->gcpUserptr = dihPtr;
    gcpPtr.p->gcpBlockref = dihBlockRef;
    return;
  }//if
  
  ndbrequire(ccurrentGcprec == RNIL);
  
  
  if(getNodeState().startLevel >= NodeState::SL_STOPPING_4){
    GCPSaveRef * const saveRef = (GCPSaveRef*)&signal->theData[0];
    saveRef->dihPtr = dihPtr;
    saveRef->nodeId = getOwnNodeId();
    saveRef->gci    = gci;
    saveRef->errorCode = GCPSaveRef::NodeShutdownInProgress;
    sendSignal(dihBlockRef, GSN_GCP_SAVEREF, signal, 
	       GCPSaveRef::SignalLength, JBB);
    return;
  }

  if(getNodeState().getNodeRestartInProgress()){
    GCPSaveRef * const saveRef = (GCPSaveRef*)&signal->theData[0];
    saveRef->dihPtr = dihPtr;
    saveRef->nodeId = getOwnNodeId();
    saveRef->gci    = gci;
    saveRef->errorCode = GCPSaveRef::NodeRestartInProgress;
    sendSignal(dihBlockRef, GSN_GCP_SAVEREF, signal, 
	       GCPSaveRef::SignalLength, JBB);
    return;
  }
  
  ccurrentGcprec = 0;
  gcpPtr.i = ccurrentGcprec;
  ptrCheckGuard(gcpPtr, cgcprecFileSize, gcpRecord);
  
  cnewestCompletedGci = gci;
  if (gci > cnewestGci) {
    jam();
    cnewestGci = gci;
  }//if
  
  gcpPtr.p->gcpBlockref = dihBlockRef;
  gcpPtr.p->gcpUserptr = dihPtr;
  gcpPtr.p->gcpId = gci;
  bool tlogActive = false;
  for (logPartPtr.i = 0; logPartPtr.i <= 3; logPartPtr.i++) {
    ptrAss(logPartPtr, logPartRecord);
    if (logPartPtr.p->logPartState == LogPartRecord::ACTIVE) {
      jam();
      logPartPtr.p->waitWriteGciLog = LogPartRecord::WWGL_TRUE;
      tlogActive = true;
    } else {
      jam();
      logPartPtr.p->waitWriteGciLog = LogPartRecord::WWGL_FALSE;
      logFilePtr.i = logPartPtr.p->currentLogfile;
      ptrCheckGuard(logFilePtr, clogFileFileSize, logFileRecord);
      logPagePtr.i = logFilePtr.p->currentLogpage;
      ptrCheckGuard(logPagePtr, clogPageFileSize, logPageRecord);
      writeCompletedGciLog(signal);
    }//if
  }//for
  if (tlogActive == true) {
    jam();
    return;
  }//if
  initGcpRecLab(signal);
  startTimeSupervision(signal);
  return;
}//Dblqh::execGCP_SAVEREQ()

/* ------------------------------------------------------------------------- */
/*  START TIME SUPERVISION OF THE LOG PARTS.                                 */
/* ------------------------------------------------------------------------- */
void Dblqh::startTimeSupervision(Signal* signal) 
{
  for (logPartPtr.i = 0; logPartPtr.i <= 3; logPartPtr.i++) {
    jam();
    ptrAss(logPartPtr, logPartRecord);
/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* WE HAVE TO START CHECKING IF THE LOG IS TO BE WRITTEN EVEN IF PAGES ARE   */
/* FULL. INITIALISE THE VALUES OF WHERE WE ARE IN THE LOG CURRENTLY.         */
/* +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
    logPartPtr.p->logPartTimer = 0;
    logPartPtr.p->logTimer = 1;
    signal->theData[0] = ZTIME_SUPERVISION;
    signal->theData[1] = logPartPtr.i;
    sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
  }//for
}//Dblqh::startTimeSupervision()

/*---------------------------------------------------------------------------*/
/* WE SET THE GLOBAL CHECKPOINT VARIABLES AFTER WRITING THE COMPLETED GCI LOG*/
/* RECORD. THIS ENSURES THAT WE WILL ENCOUNTER THE COMPLETED GCI RECORD WHEN */
/* WE EXECUTE THE FRAGMENT LOG.                                              */
/*---------------------------------------------------------------------------*/
void Dblqh::initGcpRecLab(Signal* signal) 
{
/* ======================================================================== */
/* =======               INITIATE GCP RECORD                        ======= */
/*                                                                          */
/*       SUBROUTINE SHORT NAME = IGR                                        */
/* ======================================================================== */
  for (logPartPtr.i = 0; logPartPtr.i <= 3; logPartPtr.i++) {
    jam();
    ptrAss(logPartPtr, logPartRecord);
/*--------------------------------------------------*/
/*       BY SETTING THE GCPREC = 0 WE START THE     */
/*       CHECKING BY CHECK_GCP_COMPLETED. THIS      */
/*       CHECKING MUST NOT BE STARTED UNTIL WE HAVE */
/*       INSERTED ALL COMPLETE GCI LOG RECORDS IN   */
/*       ALL LOG PARTS.                             */
/*--------------------------------------------------*/
    logPartPtr.p->gcprec = 0;
    gcpPtr.p->gcpLogPartState[logPartPtr.i] = ZWAIT_DISK;
    gcpPtr.p->gcpSyncReady[logPartPtr.i] = ZFALSE;
    logFilePtr.i = logPartPtr.p->currentLogfile;
    ptrCheckGuard(logFilePtr, clogFileFileSize, logFileRecord);
    gcpPtr.p->gcpFilePtr[logPartPtr.i] = logFilePtr.i;
    logPagePtr.i = logFilePtr.p->currentLogpage;
    ptrCheckGuard(logPagePtr, clogPageFileSize, logPageRecord);
    if (logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] == ZPAGE_HEADER_SIZE) {
      jam();
/*--------------------------------------------------*/
/*       SINCE THE CURRENT FILEPAGE POINTS AT THE   */
/*       NEXT WORD TO BE WRITTEN WE HAVE TO ADJUST  */
/*       FOR THIS BY DECREASING THE FILE PAGE BY ONE*/
/*       IF NO WORD HAS BEEN WRITTEN ON THE CURRENT */
/*       FILEPAGE.                                  */
/*--------------------------------------------------*/
      gcpPtr.p->gcpPageNo[logPartPtr.i] = logFilePtr.p->currentFilepage - 1;
      gcpPtr.p->gcpWordNo[logPartPtr.i] = ZPAGE_SIZE - 1;
    } else {
      jam();
      gcpPtr.p->gcpPageNo[logPartPtr.i] = logFilePtr.p->currentFilepage;
      gcpPtr.p->gcpWordNo[logPartPtr.i] = 
	logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] - 1;
    }//if
  }//for
  return;
}//Dblqh::initGcpRecLab()

/* ========================================================================= */
/* ==== CHECK IF ANY GLOBAL CHECKPOINTS ARE COMPLETED AFTER A COMPLETED===== */
/*      DISK WRITE.                                                          */
/*                                                                           */
/*       SUBROUTINE SHORT NAME = CGC                                         */
/* ========================================================================= */
void Dblqh::checkGcpCompleted(Signal* signal,
                              Uint32 tcgcPageWritten,
                              Uint32 tcgcWordWritten) 
{
  UintR tcgcFlag;
  UintR tcgcJ;

  gcpPtr.i = logPartPtr.p->gcprec;
  if (gcpPtr.i != RNIL) {
    jam();
/* ------------------------------------------------------------------------- */
/* IF THE GLOBAL CHECKPOINT IS NOT WAITING FOR COMPLETION THEN WE CAN QUIT   */
/* THE SEARCH IMMEDIATELY.                                                   */
/* ------------------------------------------------------------------------- */
    ptrCheckGuard(gcpPtr, cgcprecFileSize, gcpRecord);
    if (gcpPtr.p->gcpFilePtr[logPartPtr.i] == logFilePtr.i) {
/* ------------------------------------------------------------------------- */
/* IF THE COMPLETED DISK OPERATION WAS ON ANOTHER FILE THAN THE ONE WE ARE   */
/* WAITING FOR, THEN WE CAN ALSO QUIT THE SEARCH IMMEDIATELY.                */
/* ------------------------------------------------------------------------- */
      if (tcgcPageWritten < gcpPtr.p->gcpPageNo[logPartPtr.i]) {
        jam();
/* ------------------------------------------------------------------------- */
/* THIS LOG PART HAVE NOT YET WRITTEN THE GLOBAL CHECKPOINT TO DISK.         */
/* ------------------------------------------------------------------------- */
        return;
      } else {
        if (tcgcPageWritten == gcpPtr.p->gcpPageNo[logPartPtr.i]) {
          if (tcgcWordWritten < gcpPtr.p->gcpWordNo[logPartPtr.i]) {
            jam();
/* ------------------------------------------------------------------------- */
/* THIS LOG PART HAVE NOT YET WRITTEN THE GLOBAL CHECKPOINT TO DISK.         */
/* ------------------------------------------------------------------------- */
            return;
          }//if
        }//if
      }//if
/* ------------------------------------------------------------------------- */
/* THIS LOG PART HAVE WRITTEN THE GLOBAL CHECKPOINT TO DISK.                 */
/* ------------------------------------------------------------------------- */
      logPartPtr.p->gcprec = RNIL;
      gcpPtr.p->gcpLogPartState[logPartPtr.i] = ZON_DISK;
      tcgcFlag = ZTRUE;
      for (tcgcJ = 0; tcgcJ <= 3; tcgcJ++) {
        jam();
        if (gcpPtr.p->gcpLogPartState[tcgcJ] != ZON_DISK) {
          jam();
/* ------------------------------------------------------------------------- */
/*ALL LOG PARTS HAVE NOT SAVED THIS GLOBAL CHECKPOINT TO DISK YET. WAIT FOR  */
/*THEM TO COMPLETE.                                                          */
/* ------------------------------------------------------------------------- */
          tcgcFlag = ZFALSE;
        }//if
      }//for
      if (tcgcFlag == ZTRUE) {
        jam();
/* ------------------------------------------------------------------------- */
/*WE HAVE FOUND A COMPLETED GLOBAL CHECKPOINT OPERATION. WE NOW NEED TO SEND */
/*GCP_SAVECONF, REMOVE THE GCP RECORD FROM THE LIST OF WAITING GCP RECORDS   */
/*ON THIS LOG PART AND RELEASE THE GCP RECORD.                               */
// After changing the log implementation we need to perform a FSSYNCREQ on all
// log files where the last log word resided first before proceeding.
/* ------------------------------------------------------------------------- */
        UintR Ti;
        for (Ti = 0; Ti < 4; Ti++) {
          LogFileRecordPtr loopLogFilePtr;
          loopLogFilePtr.i = gcpPtr.p->gcpFilePtr[Ti];
          ptrCheckGuard(loopLogFilePtr, clogFileFileSize, logFileRecord);
          if (loopLogFilePtr.p->logFileStatus == LogFileRecord::OPEN) {
            jam();
            signal->theData[0] = loopLogFilePtr.p->fileRef;
            signal->theData[1] = cownref;
            signal->theData[2] = gcpPtr.p->gcpFilePtr[Ti];
            sendSignal(NDBFS_REF, GSN_FSSYNCREQ, signal, 3, JBA);
          } else {
            ndbrequire((loopLogFilePtr.p->logFileStatus == 
                        LogFileRecord::CLOSED) ||
                        (loopLogFilePtr.p->logFileStatus == 
                         LogFileRecord::CLOSING_WRITE_LOG) ||
                        (loopLogFilePtr.p->logFileStatus == 
                         LogFileRecord::OPENING_WRITE_LOG));
            signal->theData[0] = loopLogFilePtr.i;
            execFSSYNCCONF(signal);
          }//if
        }//for
        return;
      }//if
    }//if
  }//if
}//Dblqh::checkGcpCompleted()

void
Dblqh::execFSSYNCCONF(Signal* signal)
{
  GcpRecordPtr localGcpPtr;
  LogFileRecordPtr localLogFilePtr;
  LogPartRecordPtr localLogPartPtr;
  localLogFilePtr.i = signal->theData[0];
  ptrCheckGuard(localLogFilePtr, clogFileFileSize, logFileRecord);
  localLogPartPtr.i = localLogFilePtr.p->logPartRec;
  localGcpPtr.i = ccurrentGcprec;
  ptrCheckGuard(localGcpPtr, cgcprecFileSize, gcpRecord);
  localGcpPtr.p->gcpSyncReady[localLogPartPtr.i] = ZTRUE;
  UintR Ti;
  for (Ti = 0; Ti < 4; Ti++) {
    jam();
    if (localGcpPtr.p->gcpSyncReady[Ti] == ZFALSE) {
      jam();
      return;
    }//if
  }//for
  GCPSaveConf * const saveConf = (GCPSaveConf *)&signal->theData[0];
  saveConf->dihPtr = localGcpPtr.p->gcpUserptr;
  saveConf->nodeId = getOwnNodeId();
  saveConf->gci    = localGcpPtr.p->gcpId;
  sendSignal(localGcpPtr.p->gcpBlockref, GSN_GCP_SAVECONF, signal, 
	     GCPSaveConf::SignalLength, JBA);
  ccurrentGcprec = RNIL;
}//Dblqh::execFSSYNCCONF()


/* ######################################################################### */
/* #######                            FILE HANDLING MODULE           ####### */
/*                                                                           */
/* ######################################################################### */
/*       THIS MODULE HANDLES RESPONSE MESSAGES FROM THE FILE SYSTEM          */
/* ######################################################################### */
/* ######################################################################### */
/*       SIGNAL RECEPTION MODULE                                             */
/*       THIS MODULE IS A SUB-MODULE OF THE FILE SYSTEM HANDLING.            */
/*                                                                           */
/*  THIS MODULE CHECKS THE STATE AND JUMPS TO THE PROPER PART OF THE FILE    */
/*  HANDLING MODULE.                                                         */
/* ######################################################################### */
/* *************** */
/*  FSCLOSECONF  > */
/* *************** */
void Dblqh::execFSCLOSECONF(Signal* signal) 
{
  jamEntry();
  logFilePtr.i = signal->theData[0];
  ptrCheckGuard(logFilePtr, clogFileFileSize, logFileRecord);
  switch (logFilePtr.p->logFileStatus) {
  case LogFileRecord::CLOSE_SR_INVALIDATE_PAGES:
    jam();
    logFilePtr.p->logFileStatus = LogFileRecord::CLOSED;
    // Set the prev file to check if we shall close it.
    logFilePtr.i = logFilePtr.p->prevLogFile;
    ptrCheckGuard(logFilePtr, clogFileFileSize, logFileRecord);
    exitFromInvalidate(signal);
    return;
    break;
  case LogFileRecord::CLOSING_INIT:
    jam();
    closingInitLab(signal);
    return;
    break;
  case LogFileRecord::CLOSING_SR:
    jam();
    closingSrLab(signal);
    return;
    break;
  case LogFileRecord::CLOSING_EXEC_SR:
    jam();
    closeExecSrLab(signal);
    return;
    break;
  case LogFileRecord::CLOSING_EXEC_SR_COMPLETED:
    jam();
    closeExecSrCompletedLab(signal);
    return;
    break;
  case LogFileRecord::CLOSING_WRITE_LOG:
    jam();
    closeWriteLogLab(signal);
    return;
    break;
  case LogFileRecord::CLOSING_EXEC_LOG:
    jam();
    closeExecLogLab(signal);
    return;
    break;
  default:
    jam();
    systemErrorLab(signal);
    return;
    break;
  }//switch
}//Dblqh::execFSCLOSECONF()


/* ************>> */
/*  FSOPENCONF  > */
/* ************>> */
void Dblqh::execFSOPENCONF(Signal* signal) 
{
  jamEntry();
  initFsopenconf(signal);
  switch (logFilePtr.p->logFileStatus) {
  case LogFileRecord::OPEN_SR_INVALIDATE_PAGES:
    jam();
    logFilePtr.p->logFileStatus = LogFileRecord::OPEN;
    readFileInInvalidate(signal);
    return;
    break;
  case LogFileRecord::OPENING_INIT:
    jam();
    logFilePtr.p->logFileStatus = LogFileRecord::OPEN;
    openFileInitLab(signal);
    return;
    break;
  case LogFileRecord::OPEN_SR_FRONTPAGE:
    jam();
    logFilePtr.p->logFileStatus = LogFileRecord::OPEN;
    openSrFrontpageLab(signal);
    return;
    break;
  case LogFileRecord::OPEN_SR_LAST_FILE:
    jam();
    logFilePtr.p->logFileStatus = LogFileRecord::OPEN;
    openSrLastFileLab(signal);
    return;
    break;
  case LogFileRecord::OPEN_SR_NEXT_FILE:
    jam();
    logFilePtr.p->logFileStatus = LogFileRecord::OPEN;
    openSrNextFileLab(signal);
    return;
    break;
  case LogFileRecord::OPEN_EXEC_SR_START:
    jam();
    logFilePtr.p->logFileStatus = LogFileRecord::OPEN;
    openExecSrStartLab(signal);
    return;
    break;
  case LogFileRecord::OPEN_EXEC_SR_NEW_MBYTE:
    jam();
    logFilePtr.p->logFileStatus = LogFileRecord::OPEN;
    openExecSrNewMbyteLab(signal);
    return;
    break;
  case LogFileRecord::OPEN_SR_FOURTH_PHASE:
    jam();
    logFilePtr.p->logFileStatus = LogFileRecord::OPEN;
    openSrFourthPhaseLab(signal);
    return;
    break;
  case LogFileRecord::OPEN_SR_FOURTH_NEXT:
    jam();
    logFilePtr.p->logFileStatus = LogFileRecord::OPEN;
    openSrFourthNextLab(signal);
    return;
    break;
  case LogFileRecord::OPEN_SR_FOURTH_ZERO:
    jam();
    logFilePtr.p->logFileStatus = LogFileRecord::OPEN;
    openSrFourthZeroLab(signal);
    return;
    break;
  case LogFileRecord::OPENING_WRITE_LOG:
    jam();
    logFilePtr.p->logFileStatus = LogFileRecord::OPEN;
    return;
    break;
  case LogFileRecord::OPEN_EXEC_LOG:
    jam();
    logFilePtr.p->logFileStatus = LogFileRecord::OPEN;
    openExecLogLab(signal);
    return;
    break;
  default:
    jam();
    systemErrorLab(signal);
    return;
    break;
  }//switch
}//Dblqh::execFSOPENCONF()


/* ************>> */
/*  FSREADCONF  > */
/* ************>> */
void Dblqh::execFSREADCONF(Signal* signal) 
{
  jamEntry();
  initFsrwconf(signal);

  switch (lfoPtr.p->lfoState) {
  case LogFileOperationRecord::READ_SR_LAST_MBYTE:
    jam();
    releaseLfo(signal);
    readSrLastMbyteLab(signal);
    return;
    break;
  case LogFileOperationRecord::READ_SR_FRONTPAGE:
    jam();
    releaseLfo(signal);
    readSrFrontpageLab(signal);
    return;
    break;
  case LogFileOperationRecord::READ_SR_LAST_FILE:
    jam();
    releaseLfo(signal);
    readSrLastFileLab(signal);
    return;
    break;
  case LogFileOperationRecord::READ_SR_NEXT_FILE:
    jam();
    releaseLfo(signal);
    readSrNextFileLab(signal);
    return;
    break;
  case LogFileOperationRecord::READ_EXEC_SR:
    jam();
    readExecSrLab(signal);
    return;
    break;
  case LogFileOperationRecord::READ_EXEC_LOG:
    jam();
    readExecLogLab(signal);
    return;
    break;
  case LogFileOperationRecord::READ_SR_INVALIDATE_PAGES:
    jam();
    invalidateLogAfterLastGCI(signal);
    return;
    break;
  case LogFileOperationRecord::READ_SR_FOURTH_PHASE:
    jam();
    releaseLfo(signal);
    readSrFourthPhaseLab(signal);
    return;
    break;
  case LogFileOperationRecord::READ_SR_FOURTH_ZERO:
    jam();
    releaseLfo(signal);
    readSrFourthZeroLab(signal);
    return;
    break;
  default:
    jam();
    systemErrorLab(signal);
    return;
    break;
  }//switch
}//Dblqh::execFSREADCONF()

/* ************>> */
/*  FSREADCONF  > */
/* ************>> */
void Dblqh::execFSREADREF(Signal* signal) 
{
  jamEntry();
  lfoPtr.i = signal->theData[0];
  ptrCheckGuard(lfoPtr, clfoFileSize, logFileOperationRecord);
  switch (lfoPtr.p->lfoState) {
  case LogFileOperationRecord::READ_SR_LAST_MBYTE:
    jam();
    break;
  case LogFileOperationRecord::READ_SR_FRONTPAGE:
    jam();
    break;
  case LogFileOperationRecord::READ_SR_LAST_FILE:
    jam();
    break;
  case LogFileOperationRecord::READ_SR_NEXT_FILE:
    jam();
    break;
  case LogFileOperationRecord::READ_EXEC_SR:
    jam();
    break;
  case LogFileOperationRecord::READ_EXEC_LOG:
    jam();
    break;
  case LogFileOperationRecord::READ_SR_FOURTH_PHASE:
    jam();
    break;
  case LogFileOperationRecord::READ_SR_FOURTH_ZERO:
    jam();
    break;
  case LogFileOperationRecord::READ_SR_INVALIDATE_PAGES:
    jam()
    break;
  default:
    jam();
    break;
  }//switch
  {
    char msg[100];
    sprintf(msg, "File system read failed during LogFileOperationRecord state %d", (Uint32)lfoPtr.p->lfoState);
    fsRefError(signal,__LINE__,msg);
  }
}//Dblqh::execFSREADREF()

/* *************** */
/*  FSWRITECONF  > */
/* *************** */
void Dblqh::execFSWRITECONF(Signal* signal) 
{
  jamEntry();
  initFsrwconf(signal);
  switch (lfoPtr.p->lfoState) {
  case LogFileOperationRecord::WRITE_SR_INVALIDATE_PAGES:
    jam();
    invalidateLogAfterLastGCI(signal);
    return;
    break;
  case LogFileOperationRecord::WRITE_PAGE_ZERO:
    jam();
    writePageZeroLab(signal);
    return;
    break;
  case LogFileOperationRecord::LAST_WRITE_IN_FILE:
    jam();
    lastWriteInFileLab(signal);
    return;
    break;
  case LogFileOperationRecord::INIT_WRITE_AT_END:
    jam();
    initWriteEndLab(signal);
    return;
    break;
  case LogFileOperationRecord::INIT_FIRST_PAGE:
    jam();
    initFirstPageLab(signal);
    return;
    break;
  case LogFileOperationRecord::WRITE_GCI_ZERO:
    jam();
    writeGciZeroLab(signal);
    return;
    break;
  case LogFileOperationRecord::WRITE_DIRTY:
    jam();
    writeDirtyLab(signal);
    return;
    break;
  case LogFileOperationRecord::WRITE_INIT_MBYTE:
    jam();
    writeInitMbyteLab(signal);
    return;
    break;
  case LogFileOperationRecord::ACTIVE_WRITE_LOG:
    jam();
    writeLogfileLab(signal);
    return;
    break;
  case LogFileOperationRecord::FIRST_PAGE_WRITE_IN_LOGFILE:
    jam();
    firstPageWriteLab(signal);
    return;
    break;
  default:
    jam();
    systemErrorLab(signal);
    return;
    break;
  }//switch
}//Dblqh::execFSWRITECONF()

/* ************>> */
/*  FSWRITEREF  > */
/* ************>> */
void Dblqh::execFSWRITEREF(Signal* signal) 
{
  jamEntry();
  lfoPtr.i = signal->theData[0];
  ptrCheckGuard(lfoPtr, clfoFileSize, logFileOperationRecord);
  terrorCode = signal->theData[1];
  switch (lfoPtr.p->lfoState) {
  case LogFileOperationRecord::WRITE_PAGE_ZERO:
    jam();
    break;
  case LogFileOperationRecord::LAST_WRITE_IN_FILE:
    jam();
    break;
  case LogFileOperationRecord::INIT_WRITE_AT_END:
    jam();
    break;
  case LogFileOperationRecord::INIT_FIRST_PAGE:
    jam();
    break;
  case LogFileOperationRecord::WRITE_GCI_ZERO:
    jam();
    break;
  case LogFileOperationRecord::WRITE_DIRTY:
    jam();
    break;
  case LogFileOperationRecord::WRITE_INIT_MBYTE:
    jam();
    break;
  case LogFileOperationRecord::ACTIVE_WRITE_LOG:
    jam();
    break;
  case LogFileOperationRecord::FIRST_PAGE_WRITE_IN_LOGFILE:
    jam();
    break;
  case LogFileOperationRecord::WRITE_SR_INVALIDATE_PAGES:
    jam();
    systemErrorLab(signal);
  default:
    jam();
    break;
  }//switch
  {
    char msg[100];
    sprintf(msg, "File system write failed during LogFileOperationRecord state %d", (Uint32)lfoPtr.p->lfoState);
    fsRefError(signal,__LINE__,msg);
  }
}//Dblqh::execFSWRITEREF()


/* ========================================================================= */
/* =======              INITIATE WHEN RECEIVING FSOPENCONF           ======= */
/*                                                                           */
/* ========================================================================= */
void Dblqh::initFsopenconf(Signal* signal) 
{
  logFilePtr.i = signal->theData[0];
  ptrCheckGuard(logFilePtr, clogFileFileSize, logFileRecord);
  logFilePtr.p->fileRef = signal->theData[1];
  logPartPtr.i = logFilePtr.p->logPartRec;
  ptrCheckGuard(logPartPtr, clogPartFileSize, logPartRecord);
  logFilePtr.p->currentMbyte = 0;
  logFilePtr.p->filePosition = 0;
  logFilePtr.p->logFilePagesToDiskWithoutSynch = 0;
}//Dblqh::initFsopenconf()

/* ========================================================================= */
/* =======       INITIATE WHEN RECEIVING FSREADCONF AND FSWRITECONF  ======= */
/*                                                                           */
/* ========================================================================= */
void Dblqh::initFsrwconf(Signal* signal) 
{
  lfoPtr.i = signal->theData[0];
  ptrCheckGuard(lfoPtr, clfoFileSize, logFileOperationRecord);
  logFilePtr.i = lfoPtr.p->logFileRec;
  ptrCheckGuard(logFilePtr, clogFileFileSize, logFileRecord);
  logPartPtr.i = logFilePtr.p->logPartRec;
  ptrCheckGuard(logPartPtr, clogPartFileSize, logPartRecord);
  logPagePtr.i = lfoPtr.p->firstLfoPage;
  ptrCheckGuard(logPagePtr, clogPageFileSize, logPageRecord);
}//Dblqh::initFsrwconf()

/* ######################################################################### */
/*       NORMAL OPERATION MODULE                                             */
/*       THIS MODULE IS A SUB-MODULE OF THE FILE SYSTEM HANDLING.            */
/*                                                                           */
/*   THIS PART HANDLES THE NORMAL OPENING, CLOSING AND WRITING OF LOG FILES  */
/*   DURING NORMAL OPERATION.                                                */
/* ######################################################################### */
/*---------------------------------------------------------------------------*/
/* THIS SIGNAL IS USED TO SUPERVISE THAT THE LOG RECORDS ARE NOT KEPT IN MAIN*/
/* MEMORY FOR MORE THAN 1 SECOND TO ACHIEVE THE PROPER RELIABILITY.          */
/*---------------------------------------------------------------------------*/
void Dblqh::timeSup(Signal* signal) 
{
  LogPageRecordPtr origLogPagePtr;
  Uint32 wordWritten;

  jamEntry();
  logPartPtr.i = signal->theData[0];
  ptrCheckGuard(logPartPtr, clogPartFileSize, logPartRecord);
  logFilePtr.i = logPartPtr.p->currentLogfile;
  ptrCheckGuard(logFilePtr, clogFileFileSize, logFileRecord);
  logPagePtr.i = logFilePtr.p->currentLogpage;
  ptrCheckGuard(logPagePtr, clogPageFileSize, logPageRecord);
  if (logPartPtr.p->logPartTimer != logPartPtr.p->logTimer) {
    jam();
/*--------------------------------------------------------------------------*/
/*       THIS LOG PART HAS NOT WRITTEN TO DISK DURING THE LAST SECOND.      */
/*--------------------------------------------------------------------------*/
    switch (logPartPtr.p->logPartState) {
    case LogPartRecord::FILE_CHANGE_PROBLEM:
      jam();
/*--------------------------------------------------------------------------*/
/*       THIS LOG PART HAS PROBLEMS IN CHANGING FILES MAKING IT IMPOSSIBLE  */
//       TO WRITE TO THE FILE CURRENTLY. WE WILL COMEBACK LATER AND SEE IF
//       THE PROBLEM HAS BEEN FIXED.
/*--------------------------------------------------------------------------*/
    case LogPartRecord::ACTIVE:
      jam();
/*---------------------------------------------------------------------------*/
/* AN OPERATION IS CURRENTLY ACTIVE IN WRITING THIS LOG PART. WE THUS CANNOT */
/* WRITE ANYTHING TO DISK AT THIS MOMENT. WE WILL SEND A SIGNAL DELAYED FOR  */
/* 10 MS AND THEN TRY AGAIN. POSSIBLY THE LOG PART WILL HAVE BEEN WRITTEN    */
/* UNTIL THEN OR ELSE IT SHOULD BE FREE TO WRITE AGAIN.                      */
/*---------------------------------------------------------------------------*/
      signal->theData[0] = ZTIME_SUPERVISION;
      signal->theData[1] = logPartPtr.i;
      sendSignalWithDelay(cownref, GSN_CONTINUEB, signal, 10, 2);
      return;
      break;
    case LogPartRecord::IDLE:
    case LogPartRecord::TAIL_PROBLEM:
      jam();
/*---------------------------------------------------------------------------*/
/* IDLE AND NOT WRITTEN TO DISK IN A SECOND. ALSO WHEN WE HAVE A TAIL PROBLEM*/
/* WE HAVE TO WRITE TO DISK AT TIMES. WE WILL FIRST CHECK WHETHER ANYTHING   */
/* AT ALL HAVE BEEN WRITTEN TO THE PAGES BEFORE WRITING TO DISK.             */
/*---------------------------------------------------------------------------*/
/* WE HAVE TO WRITE TO DISK IN ALL CASES SINCE THERE COULD BE INFORMATION    */
/* STILL IN THE LOG THAT WAS GENERATED BEFORE THE PREVIOUS TIME SUPERVISION  */
/* BUT AFTER THE LAST DISK WRITE. THIS PREVIOUSLY STOPPED ALL DISK WRITES    */
/* WHEN NO MORE LOG WRITES WERE PERFORMED (THIS HAPPENED WHEN LOG GOT FULL   */
/* AND AFTER LOADING THE INITIAL RECORDS IN INITIAL START).                  */
/*---------------------------------------------------------------------------*/
      if (((logFilePtr.p->currentFilepage + 1) & (ZPAGES_IN_MBYTE -1)) == 0) {
        jam();
/*---------------------------------------------------------------------------*/
/* THIS IS THE LAST PAGE IN THIS MBYTE. WRITE NEXT LOG AND SWITCH TO NEXT    */
/* MBYTE.                                                                    */
/*---------------------------------------------------------------------------*/
        changeMbyte(signal);
      } else {
/*---------------------------------------------------------------------------*/
/* WRITE THE LOG PAGE TO DISK EVEN IF IT IS NOT FULL. KEEP PAGE AND WRITE A  */
/* COPY. THE ORIGINAL PAGE WILL BE WRITTEN AGAIN LATER ON.                   */
/*---------------------------------------------------------------------------*/
        wordWritten = logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] - 1;
        origLogPagePtr.i = logPagePtr.i;
        origLogPagePtr.p = logPagePtr.p;
        seizeLogpage(signal);
        MEMCOPY_NO_WORDS(&logPagePtr.p->logPageWord[0],
                         &origLogPagePtr.p->logPageWord[0],
                         wordWritten + 1);
        ndbrequire(wordWritten < ZPAGE_SIZE);
        if (logFilePtr.p->noLogpagesInBuffer > 0) {
          jam();
          completedLogPage(signal, ZENFORCE_WRITE);
/*---------------------------------------------------------------------------*/
/*SINCE WE ARE ONLY WRITING PART OF THE LAST PAGE WE HAVE TO UPDATE THE WORD */
/*WRITTEN TO REFLECT THE REAL LAST WORD WRITTEN. WE ALSO HAVE TO MOVE THE    */
/*FILE POSITION ONE STEP BACKWARDS SINCE WE ARE NOT WRITING THE LAST PAGE    */
/*COMPLETELY. IT WILL BE WRITTEN AGAIN.                                      */
/*---------------------------------------------------------------------------*/
          lfoPtr.p->lfoWordWritten = wordWritten;
          logFilePtr.p->filePosition = logFilePtr.p->filePosition - 1;
        } else {
          if (wordWritten == (ZPAGE_HEADER_SIZE - 1)) {
/*---------------------------------------------------------------------------*/
/*THIS IS POSSIBLE BUT VERY UNLIKELY. IF THE PAGE WAS COMPLETED AFTER THE LAST*/
/*WRITE TO DISK THEN NO_LOG_PAGES_IN_BUFFER > 0 AND IF NOT WRITTEN SINCE LAST*/
/*WRITE TO DISK THEN THE PREVIOUS PAGE MUST HAVE BEEN WRITTEN BY SOME        */
/*OPERATION AND THAT BECAME COMPLETELY FULL. IN ANY CASE WE NEED NOT WRITE AN*/
/*EMPTY PAGE TO DISK.                                                        */
/*---------------------------------------------------------------------------*/
            jam();
            releaseLogpage(signal);
          } else {
            jam();
            writeSinglePage(signal, logFilePtr.p->currentFilepage, wordWritten);
            lfoPtr.p->lfoState = LogFileOperationRecord::ACTIVE_WRITE_LOG;
          }//if
        }//if
      }//if
      break;
    default:
      ndbrequire(false);
      break;
    }//switch
  }//if
  logPartPtr.p->logTimer++;
  return;
}//Dblqh::timeSup()

void Dblqh::writeLogfileLab(Signal* signal) 
{
/*---------------------------------------------------------------------------*/
/* CHECK IF ANY GLOBAL CHECKPOINTS ARE COMPLETED DUE TO THIS COMPLETED DISK  */
/* WRITE.                                                                    */
/*---------------------------------------------------------------------------*/
  switch (logFilePtr.p->fileChangeState) {
  case LogFileRecord::NOT_ONGOING:
    jam();
    checkGcpCompleted(signal,
                      ((lfoPtr.p->lfoPageNo + lfoPtr.p->noPagesRw) - 1),
                      lfoPtr.p->lfoWordWritten);
    break;
#if 0
  case LogFileRecord::BOTH_WRITES_ONGOING:
    jam();
    ndbout_c("not crashing!!");
    // Fall-through
#endif
  case LogFileRecord::WRITE_PAGE_ZERO_ONGOING:
  case LogFileRecord::LAST_WRITE_ONGOING:
    jam();
    logFilePtr.p->lastPageWritten = (lfoPtr.p->lfoPageNo + lfoPtr.p->noPagesRw) - 1;
    logFilePtr.p->lastWordWritten = lfoPtr.p->lfoWordWritten;
    break;
  default:
    jam();
    systemErrorLab(signal);
    return;
    break;
  }//switch
  releaseLfoPages(signal);
  releaseLfo(signal);
  return;
}//Dblqh::writeLogfileLab()

void Dblqh::closeWriteLogLab(Signal* signal) 
{
  logFilePtr.p->logFileStatus = LogFileRecord::CLOSED;
  return;
}//Dblqh::closeWriteLogLab()

/* ######################################################################### */
/*       FILE CHANGE MODULE                                                  */
/*       THIS MODULE IS A SUB-MODULE OF THE FILE SYSTEM HANDLING.            */
/*                                                                           */
/*THIS PART OF THE FILE MODULE HANDLES WHEN WE ARE CHANGING LOG FILE DURING  */
/*NORMAL OPERATION. WE HAVE TO BE CAREFUL WHEN WE ARE CHANGING LOG FILE SO   */
/*THAT WE DO NOT COMPLICATE THE SYSTEM RESTART PROCESS TOO MUCH.             */
/*THE IDEA IS THAT WE START BY WRITING THE LAST WRITE IN THE OLD FILE AND WE */
/*ALSO WRITE THE FIRST PAGE OF THE NEW FILE CONCURRENT WITH THAT. THIS FIRST */
/*PAGE IN THE NEW FILE DO NOT CONTAIN ANY LOG RECORDS OTHER THAN A DESCRIPTOR*/
/*CONTAINING INFORMATION ABOUT GCI'S NEEDED AT SYSTEM RESTART AND A NEXT LOG */
/*RECORD.                                                                    */
/*                                                                           */
/*WHEN BOTH OF THOSE WRITES HAVE COMPLETED WE ALSO WRITE PAGE ZERO IN FILE   */
/*ZERO. THE ONLY INFORMATION WHICH IS INTERESTING HERE IS THE NEW FILE NUMBER*/
/*                                                                           */
/*IF OPTIMISATIONS ARE NEEDED OF THE LOG HANDLING THEN IT IS POSSIBLE TO     */
/*AVOID WRITING THE FIRST PAGE OF THE NEW PAGE IMMEDIATELY. THIS COMPLICATES */
/*THE SYSTEM RESTART AND ONE HAS TO TAKE SPECIAL CARE WITH FILE ZERO. IT IS  */
/*HOWEVER NO LARGE PROBLEM TO CHANGE INTO THIS SCENARIO. TO AVOID ALSO THE   */
/*WRITING OF PAGE ZERO IS ALSO POSSIBLE BUT COMPLICATES THE DESIGN EVEN      */
/*FURTHER. IT GETS FAIRLY COMPLEX TO FIND THE END OF THE LOG. SOME SORT OF   */
/*BINARY SEARCH IS HOWEVER MOST LIKELY A GOOD METHODOLOGY FOR THIS.          */
/* ######################################################################### */
void Dblqh::firstPageWriteLab(Signal* signal) 
{
  releaseLfo(signal);
/*---------------------------------------------------------------------------*/
/*       RELEASE PAGE ZERO IF THE FILE IS NOT FILE 0.                        */
/*---------------------------------------------------------------------------*/
  Uint32 fileNo = logFilePtr.p->fileNo;
  if (fileNo != 0) {
    jam();
    releaseLogpage(signal);
  }//if
/*---------------------------------------------------------------------------*/
/* IF A NEW FILE HAS BEEN OPENED WE SHALL ALWAYS ALSO WRITE TO PAGE O IN     */
/* FILE 0. THE AIM IS TO MAKE RESTARTS EASIER BY SPECIFYING WHICH IS THE     */
/* LAST FILE WHERE LOGGING HAS STARTED.                                      */
/*---------------------------------------------------------------------------*/
/* FIRST CHECK WHETHER THE LAST WRITE IN THE PREVIOUS FILE HAVE COMPLETED    */
/*---------------------------------------------------------------------------*/
  if (logFilePtr.p->fileChangeState == LogFileRecord::BOTH_WRITES_ONGOING) {
    jam();
/*---------------------------------------------------------------------------*/
/* THE LAST WRITE WAS STILL ONGOING.                                         */
/*---------------------------------------------------------------------------*/
    logFilePtr.p->fileChangeState = LogFileRecord::LAST_WRITE_ONGOING;
    return;
  } else {
    jam();
    ndbrequire(logFilePtr.p->fileChangeState == LogFileRecord::FIRST_WRITE_ONGOING);
/*---------------------------------------------------------------------------*/
/* WRITE TO PAGE 0 IN IN FILE 0 NOW.                                         */
/*---------------------------------------------------------------------------*/
    logFilePtr.p->fileChangeState = LogFileRecord::WRITE_PAGE_ZERO_ONGOING;
    if (fileNo == 0) {
      jam();
/*---------------------------------------------------------------------------*/
/* IF THE NEW FILE WAS 0 THEN WE HAVE ALREADY WRITTEN PAGE ZERO IN FILE 0.   */
/*---------------------------------------------------------------------------*/
      logFilePtr.p->fileChangeState = LogFileRecord::NOT_ONGOING;
      return;
    } else {
      jam();
/*---------------------------------------------------------------------------*/
/* WRITE PAGE ZERO IN FILE ZERO. LOG_FILE_REC WILL REFER TO THE LOG FILE WE  */
/* HAVE JUST WRITTEN PAGE ZERO IN TO GET HOLD OF LOG_FILE_PTR FOR THIS       */
/* RECORD QUICKLY. THIS IS NEEDED TO GET HOLD OF THE FILE_CHANGE_STATE.      */
/* THE ONLY INFORMATION WE WANT TO CHANGE IS THE LAST FILE NUMBER IN THE     */
/* FILE DESCRIPTOR. THIS IS USED AT SYSTEM RESTART TO FIND THE END OF THE    */
/* LOG PART.                                                                 */
/*---------------------------------------------------------------------------*/
      Uint32 currLogFile = logFilePtr.i;
      logFilePtr.i = logPartPtr.p->firstLogfile;
      ptrCheckGuard(logFilePtr, clogFileFileSize, logFileRecord);
      logPagePtr.i = logFilePtr.p->logPageZero;
      ptrCheckGuard(logPagePtr, clogPageFileSize, logPageRecord);
      logPagePtr.p->logPageWord[ZPAGE_HEADER_SIZE + ZPOS_FILE_NO] = fileNo;
      writeSinglePage(signal, 0, ZPAGE_SIZE - 1);
      lfoPtr.p->logFileRec = currLogFile;
      lfoPtr.p->lfoState = LogFileOperationRecord::WRITE_PAGE_ZERO;
      return;
    }//if
  }//if
}//Dblqh::firstPageWriteLab()

void Dblqh::lastWriteInFileLab(Signal* signal) 
{
  LogFileRecordPtr locLogFilePtr;
/*---------------------------------------------------------------------------*/
/* CHECK IF ANY GLOBAL CHECKPOINTS ARE COMPLETED DUE TO THIS COMPLETED DISK  */
/* WRITE.                                                                    */
/*---------------------------------------------------------------------------*/
  checkGcpCompleted(signal,
                    ((lfoPtr.p->lfoPageNo + lfoPtr.p->noPagesRw) - 1),
                    (ZPAGE_SIZE - 1));
  releaseLfoPages(signal);
  releaseLfo(signal);
/*---------------------------------------------------------------------------*/
/* IF THE FILE IS NOT IN USE OR THE NEXT FILE TO BE USED WE WILL CLOSE IT.   */
/*---------------------------------------------------------------------------*/
  locLogFilePtr.i = logPartPtr.p->currentLogfile;
  ptrCheckGuard(locLogFilePtr, clogFileFileSize, logFileRecord);
  if (logFilePtr.i != locLogFilePtr.i) {
    if (logFilePtr.i != locLogFilePtr.p->nextLogFile) {
      if (logFilePtr.p->fileNo != 0) {
        jam();
/*---------------------------------------------------------------------------*/
/* THE FILE IS NOT FILE ZERO EITHER. WE WILL NOT CLOSE FILE ZERO SINCE WE    */
/* USE IT TO KEEP TRACK OF THE CURRENT LOG FILE BY WRITING PAGE ZERO IN      */
/* FILE ZERO.                                                                */
/*---------------------------------------------------------------------------*/
/* WE WILL CLOSE THE FILE.                                                   */
/*---------------------------------------------------------------------------*/
        logFilePtr.p->logFileStatus = LogFileRecord::CLOSING_WRITE_LOG;
        closeFile(signal, logFilePtr);
      }//if
    }//if
  }//if
/*---------------------------------------------------------------------------*/
/* IF A NEW FILE HAS BEEN OPENED WE SHALL ALWAYS ALSO WRITE TO PAGE O IN     */
/* FILE 0. THE AIM IS TO MAKE RESTARTS EASIER BY SPECIFYING WHICH IS THE     */
/* LAST FILE WHERE LOGGING HAS STARTED.                                      */
/*---------------------------------------------------------------------------*/
/* FIRST CHECK WHETHER THE FIRST WRITE IN THE NEW FILE HAVE COMPLETED        */
/* THIS STATE INFORMATION IS IN THE NEW LOG FILE AND THUS WE HAVE TO MOVE    */
/* THE LOG FILE POINTER TO THIS LOG FILE.                                    */
/*---------------------------------------------------------------------------*/
  logFilePtr.i = logFilePtr.p->nextLogFile;
  ptrCheckGuard(logFilePtr, clogFileFileSize, logFileRecord);
  if (logFilePtr.p->fileChangeState == LogFileRecord::BOTH_WRITES_ONGOING) {
    jam();
/*---------------------------------------------------------------------------*/
/* THE FIRST WRITE WAS STILL ONGOING.                                        */
/*---------------------------------------------------------------------------*/
    logFilePtr.p->fileChangeState = LogFileRecord::FIRST_WRITE_ONGOING;
    return;
  } else {
    ndbrequire(logFilePtr.p->fileChangeState == LogFileRecord::LAST_WRITE_ONGOING);
/*---------------------------------------------------------------------------*/
/* WRITE TO PAGE 0 IN IN FILE 0 NOW.                                         */
/*---------------------------------------------------------------------------*/
    logFilePtr.p->fileChangeState = LogFileRecord::WRITE_PAGE_ZERO_ONGOING;
    Uint32 fileNo = logFilePtr.p->fileNo;
    if (fileNo == 0) {
      jam();
/*---------------------------------------------------------------------------*/
/* IF THE NEW FILE WAS 0 THEN WE HAVE ALREADY WRITTEN PAGE ZERO IN FILE 0.   */
/*---------------------------------------------------------------------------*/
      logFilePtr.p->fileChangeState = LogFileRecord::NOT_ONGOING;
      return;
    } else {
      jam();
/*---------------------------------------------------------------------------*/
/* WRITE PAGE ZERO IN FILE ZERO. LOG_FILE_REC WILL REFER TO THE LOG FILE WE  */
/* HAVE JUST WRITTEN PAGE ZERO IN TO GET HOLD OF LOG_FILE_PTR FOR THIS       */
/* RECORD QUICKLY. THIS IS NEEDED TO GET HOLD OF THE FILE_CHANGE_STATE.      */
/* THE ONLY INFORMATION WE WANT TO CHANGE IS THE LAST FILE NUMBER IN THE     */
/* FILE DESCRIPTOR. THIS IS USED AT SYSTEM RESTART TO FIND THE END OF THE    */
/* LOG PART.                                                                 */
/*---------------------------------------------------------------------------*/
      Uint32 currLogFile = logFilePtr.i;
      logFilePtr.i = logPartPtr.p->firstLogfile;
      ptrCheckGuard(logFilePtr, clogFileFileSize, logFileRecord);
      logPagePtr.i = logFilePtr.p->logPageZero;
      ptrCheckGuard(logPagePtr, clogPageFileSize, logPageRecord);
      logPagePtr.p->logPageWord[ZPAGE_HEADER_SIZE + ZPOS_FILE_NO] = fileNo;
      writeSinglePage(signal, 0, ZPAGE_SIZE - 1);
      lfoPtr.p->logFileRec = currLogFile;
      lfoPtr.p->lfoState = LogFileOperationRecord::WRITE_PAGE_ZERO;
      return;
    }//if
  }//if
}//Dblqh::lastWriteInFileLab()

void Dblqh::writePageZeroLab(Signal* signal) 
{
  if (false && logPartPtr.p->logPartState == LogPartRecord::FILE_CHANGE_PROBLEM) 
  {
    if (logPartPtr.p->firstLogQueue == RNIL) 
    {
      jam();
      logPartPtr.p->logPartState = LogPartRecord::IDLE;
      ndbout_c("resetting logPartState to IDLE");
    } 
    else 
    {
      jam();
      logPartPtr.p->logPartState = LogPartRecord::ACTIVE;
      ndbout_c("resetting logPartState to ACTIVE");
    }
  }
  
  logFilePtr.p->fileChangeState = LogFileRecord::NOT_ONGOING;
/*---------------------------------------------------------------------------*/
/* IT COULD HAVE ARRIVED PAGE WRITES TO THE CURRENT FILE WHILE WE WERE       */
/* WAITING FOR THIS DISK WRITE TO COMPLETE. THEY COULD NOT CHECK FOR         */
/* COMPLETED GLOBAL CHECKPOINTS. THUS WE SHOULD DO THAT NOW INSTEAD.         */
/*---------------------------------------------------------------------------*/
  checkGcpCompleted(signal,
                    logFilePtr.p->lastPageWritten,
                    logFilePtr.p->lastWordWritten);
  releaseLfo(signal);
  return;
}//Dblqh::writePageZeroLab()

/* ######################################################################### */
/*       INITIAL START MODULE                                                */
/*       THIS MODULE IS A SUB-MODULE OF THE FILE SYSTEM HANDLING.            */
/*                                                                           */
/*THIS MODULE INITIALISES ALL THE LOG FILES THAT ARE NEEDED AT A SYSTEM      */
/*RESTART AND WHICH ARE USED DURING NORMAL OPERATIONS. IT CREATES THE FILES  */
/*AND SETS A PROPER SIZE OF THEM AND INITIALISES THE FIRST PAGE IN EACH FILE */
/* ######################################################################### */
void Dblqh::openFileInitLab(Signal* signal) 
{
  logFilePtr.p->logFileStatus = LogFileRecord::OPEN_INIT;
  seizeLogpage(signal);
  writeSinglePage(signal, (ZNO_MBYTES_IN_FILE * ZPAGES_IN_MBYTE) - 1, ZPAGE_SIZE - 1);
  lfoPtr.p->lfoState = LogFileOperationRecord::INIT_WRITE_AT_END;
  return;
}//Dblqh::openFileInitLab()

void Dblqh::initWriteEndLab(Signal* signal) 
{
  releaseLfo(signal);
  initLogpage(signal);
  if (logFilePtr.p->fileNo == 0) {
    jam();
/*---------------------------------------------------------------------------*/
/* PAGE ZERO IN FILE ZERO MUST SET LOG LAP TO ONE SINCE IT HAS STARTED       */
/* WRITING TO THE LOG, ALSO GLOBAL CHECKPOINTS ARE SET TO ZERO.              */
/*---------------------------------------------------------------------------*/
    logPagePtr.p->logPageWord[ZPOS_LOG_LAP] = 1;
    logPagePtr.p->logPageWord[ZPOS_MAX_GCI_STARTED] = 0;
    logPagePtr.p->logPageWord[ZPOS_MAX_GCI_COMPLETED] = 0;
    logFilePtr.p->logMaxGciStarted[0] = 0;
    logFilePtr.p->logMaxGciCompleted[0] = 0;
  }//if
/*---------------------------------------------------------------------------*/
/* REUSE CODE FOR INITIALISATION OF FIRST PAGE IN ALL LOG FILES.             */
/*---------------------------------------------------------------------------*/
  writeFileHeaderOpen(signal, ZINIT);
  return;
}//Dblqh::initWriteEndLab()

void Dblqh::initFirstPageLab(Signal* signal) 
{
  releaseLfo(signal);
  if (logFilePtr.p->fileNo == 0) {
    jam();
/*---------------------------------------------------------------------------*/
/* IN FILE ZERO WE WILL INSERT A PAGE ONE WHERE WE WILL INSERT A COMPLETED   */
/* GCI RECORD FOR GCI = 0.                                                   */
/*---------------------------------------------------------------------------*/
    initLogpage(signal);
    logPagePtr.p->logPageWord[ZPOS_LOG_LAP] = 1;
    logPagePtr.p->logPageWord[ZPAGE_HEADER_SIZE] = ZCOMPLETED_GCI_TYPE;
    logPagePtr.p->logPageWord[ZPAGE_HEADER_SIZE + 1] = 1;
    writeSinglePage(signal, 1, ZPAGE_SIZE - 1);
    lfoPtr.p->lfoState = LogFileOperationRecord::WRITE_GCI_ZERO;
    return;
  }//if
  logFilePtr.p->currentMbyte = 1;
  writeInitMbyte(signal);
  return;
}//Dblqh::initFirstPageLab()

void Dblqh::writeGciZeroLab(Signal* signal) 
{
  releaseLfo(signal);
  logFilePtr.p->currentMbyte = 1;
  writeInitMbyte(signal);
  return;
}//Dblqh::writeGciZeroLab()

void Dblqh::writeInitMbyteLab(Signal* signal) 
{
  releaseLfo(signal);
  logFilePtr.p->currentMbyte = logFilePtr.p->currentMbyte + 1;
  if (logFilePtr.p->currentMbyte == ZNO_MBYTES_IN_FILE) {
    jam();
    releaseLogpage(signal);
    logFilePtr.p->logFileStatus = LogFileRecord::CLOSING_INIT;
    closeFile(signal, logFilePtr);
    return;
  }//if
  writeInitMbyte(signal);
  return;
}//Dblqh::writeInitMbyteLab()

void Dblqh::closingInitLab(Signal* signal) 
{
  logFilePtr.p->logFileStatus = LogFileRecord::CLOSED;
  logPartPtr.i = logFilePtr.p->logPartRec;
  ptrCheckGuard(logPartPtr, clogPartFileSize, logPartRecord);
  if (logFilePtr.p->nextLogFile == logPartPtr.p->firstLogfile) {
    jam();
    checkInitCompletedLab(signal);
    return;
  } else {
    jam();
    logFilePtr.i = logFilePtr.p->nextLogFile;
    ptrCheckGuard(logFilePtr, clogFileFileSize, logFileRecord);
    openLogfileInit(signal);
  }//if
  return;
}//Dblqh::closingInitLab()

void Dblqh::checkInitCompletedLab(Signal* signal) 
{
  logPartPtr.p->logPartState = LogPartRecord::SR_FIRST_PHASE_COMPLETED;
/*---------------------------------------------------------------------------*/
/* WE HAVE NOW INITIALISED ALL FILES IN THIS LOG PART. WE CAN NOW SET THE    */
/* THE LOG LAP TO ONE SINCE WE WILL START WITH LOG LAP ONE. LOG LAP = ZERO   */
/* MEANS THIS PART OF THE LOG IS NOT WRITTEN YET.                            */
/*---------------------------------------------------------------------------*/
  logPartPtr.p->logLap = 1;
  logPartPtr.i = 0;
CHECK_LOG_PARTS_LOOP:
  ptrAss(logPartPtr, logPartRecord);
  if (logPartPtr.p->logPartState != LogPartRecord::SR_FIRST_PHASE_COMPLETED) {
    jam();
/*---------------------------------------------------------------------------*/
/* THIS PART HAS STILL NOT COMPLETED. WAIT FOR THIS TO OCCUR.                */
/*---------------------------------------------------------------------------*/
    return;
  }//if
  if (logPartPtr.i == 3) {
    jam();
/*---------------------------------------------------------------------------*/
/* ALL LOG PARTS ARE COMPLETED. NOW WE CAN CONTINUE WITH THE RESTART         */
/* PROCESSING. THE NEXT STEP IS TO PREPARE FOR EXECUTING OPERATIONS. THUS WE */
/* NEED TO INITIALISE ALL NEEDED DATA AND TO OPEN FILE ZERO AND THE NEXT AND */
/* TO SET THE CURRENT LOG PAGE TO BE PAGE 1 IN FILE ZERO.                    */
/*---------------------------------------------------------------------------*/
    for (logPartPtr.i = 0; logPartPtr.i <= 3; logPartPtr.i++) {
      ptrAss(logPartPtr, logPartRecord);
      signal->theData[0] = ZINIT_FOURTH;
      signal->theData[1] = logPartPtr.i;
      sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
    }//for
    return;
  } else {
    jam();
    logPartPtr.i = logPartPtr.i + 1;
    goto CHECK_LOG_PARTS_LOOP;
  }//if
}//Dblqh::checkInitCompletedLab()

/* ========================================================================= */
/* =======       INITIATE LOG FILE OPERATION RECORD WHEN ALLOCATED   ======= */
/*                                                                           */
/* ========================================================================= */
void Dblqh::initLfo(Signal* signal) 
{
  lfoPtr.p->firstLfoPage = RNIL;
  lfoPtr.p->lfoState = LogFileOperationRecord::IDLE;
  lfoPtr.p->logFileRec = logFilePtr.i;
  lfoPtr.p->noPagesRw = 0;
  lfoPtr.p->lfoPageNo = ZNIL;
}//Dblqh::initLfo()

/* ========================================================================= */
/* =======              INITIATE LOG FILE WHEN ALLOCATED             ======= */
/*                                                                           */
/*       INPUT:  TFILE_NO        NUMBER OF THE FILE INITIATED                */
/*               LOG_PART_PTR    NUMBER OF LOG PART                          */
/*       SUBROUTINE SHORT NAME = IL                                          */
/* ========================================================================= */
void Dblqh::initLogfile(Signal* signal, Uint32 fileNo) 
{
  UintR tilTmp;
  UintR tilIndex;

  logFilePtr.p->currentFilepage = 0;
  logFilePtr.p->currentLogpage = RNIL;
  logFilePtr.p->fileName[0] = (UintR)-1;
  logFilePtr.p->fileName[1] = (UintR)-1;	/* = H'FFFFFFFF = -1 */
  logFilePtr.p->fileName[2] = fileNo;	        /* Sfile_no */
  tilTmp = 1;	                        /* VERSION 1 OF FILE NAME */
  tilTmp = (tilTmp << 8) + 1;	    /* FRAGMENT LOG => .FRAGLOG AS EXTENSION */
  tilTmp = (tilTmp << 8) + (8 + logPartPtr.i); /* DIRECTORY = D(8+Part)/DBLQH */
  tilTmp = (tilTmp << 8) + 255;	              /* IGNORE Pxx PART OF FILE NAME */
  logFilePtr.p->fileName[3] = tilTmp;
/* ========================================================================= */
/*       FILE NAME BECOMES /D2/DBLQH/Tpart_no/Sfile_no.FRAGLOG               */
/* ========================================================================= */
  logFilePtr.p->fileNo = fileNo;
  logFilePtr.p->filePosition = 0;
  logFilePtr.p->firstLfo = RNIL;
  logFilePtr.p->lastLfo = RNIL;
  logFilePtr.p->logFileStatus = LogFileRecord::CLOSED;
  logFilePtr.p->logPartRec = logPartPtr.i;
  logFilePtr.p->noLogpagesInBuffer = 0;
  logFilePtr.p->firstFilledPage = RNIL;
  logFilePtr.p->lastFilledPage = RNIL;
  logFilePtr.p->lastPageWritten = 0;
  logFilePtr.p->logPageZero = RNIL;
  logFilePtr.p->currentMbyte = 0;
  for (tilIndex = 0; tilIndex <= 15; tilIndex++) {
    logFilePtr.p->logMaxGciCompleted[tilIndex] = (UintR)-1;
    logFilePtr.p->logMaxGciStarted[tilIndex] = (UintR)-1;
    logFilePtr.p->logLastPrepRef[tilIndex] = 0;
  }//for
}//Dblqh::initLogfile()

/* ========================================================================= */
/* =======              INITIATE LOG PAGE WHEN ALLOCATED             ======= */
/*                                                                           */
/* ========================================================================= */
void Dblqh::initLogpage(Signal* signal) 
{
  TcConnectionrecPtr ilpTcConnectptr;

  logPagePtr.p->logPageWord[ZPOS_LOG_LAP] = logPartPtr.p->logLap;
  logPagePtr.p->logPageWord[ZPOS_MAX_GCI_COMPLETED] = 
        logPartPtr.p->logPartNewestCompletedGCI;
  logPagePtr.p->logPageWord[ZPOS_MAX_GCI_STARTED] = cnewestGci;
  logPagePtr.p->logPageWord[ZPOS_VERSION] = NDB_VERSION;
  logPagePtr.p->logPageWord[ZPOS_NO_LOG_FILES] = logPartPtr.p->noLogFiles;
  logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] = ZPAGE_HEADER_SIZE;
  ilpTcConnectptr.i = logPartPtr.p->firstLogTcrec;
  if (ilpTcConnectptr.i != RNIL) {
    jam();
    ptrCheckGuard(ilpTcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
    logPagePtr.p->logPageWord[ZLAST_LOG_PREP_REF] = 
      (ilpTcConnectptr.p->logStartFileNo << 16) +
      (ilpTcConnectptr.p->logStartPageNo >> ZTWOLOG_NO_PAGES_IN_MBYTE);
  } else {
    jam();
    logPagePtr.p->logPageWord[ZLAST_LOG_PREP_REF] = 
      (logFilePtr.p->fileNo << 16) + 
      (logFilePtr.p->currentFilepage >> ZTWOLOG_NO_PAGES_IN_MBYTE);
  }//if
}//Dblqh::initLogpage()

/* ------------------------------------------------------------------------- */
/* -------               OPEN LOG FILE FOR READ AND WRITE            ------- */
/*                                                                           */
/*       SUBROUTINE SHORT NAME = OFR                                         */
/* ------------------------------------------------------------------------- */
void Dblqh::openFileRw(Signal* signal, LogFileRecordPtr olfLogFilePtr) 
{
  signal->theData[0] = cownref;
  signal->theData[1] = olfLogFilePtr.i;
  signal->theData[2] = olfLogFilePtr.p->fileName[0];
  signal->theData[3] = olfLogFilePtr.p->fileName[1];
  signal->theData[4] = olfLogFilePtr.p->fileName[2];
  signal->theData[5] = olfLogFilePtr.p->fileName[3];
  signal->theData[6] = ZOPEN_READ_WRITE;
  sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, 7, JBA);
}//Dblqh::openFileRw()

/* ------------------------------------------------------------------------- */
/* -------               OPEN LOG FILE DURING INITIAL START          ------- */
/*                                                                           */
/*       SUBROUTINE SHORT NAME = OLI                                         */
/* ------------------------------------------------------------------------- */
void Dblqh::openLogfileInit(Signal* signal) 
{
  logFilePtr.p->logFileStatus = LogFileRecord::OPENING_INIT;
  signal->theData[0] = cownref;
  signal->theData[1] = logFilePtr.i;
  signal->theData[2] = logFilePtr.p->fileName[0];
  signal->theData[3] = logFilePtr.p->fileName[1];
  signal->theData[4] = logFilePtr.p->fileName[2];
  signal->theData[5] = logFilePtr.p->fileName[3];
  signal->theData[6] = 0x302;
  sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, 7, JBA);
}//Dblqh::openLogfileInit()

/* OPEN FOR READ/WRITE, DO CREATE AND DO TRUNCATE FILE */
/* ------------------------------------------------------------------------- */
/* -------               OPEN NEXT LOG FILE                          ------- */
/*                                                                           */
/*       SUBROUTINE SHORT NAME = ONL                                         */
/* ------------------------------------------------------------------------- */
void Dblqh::openNextLogfile(Signal* signal) 
{
  LogFileRecordPtr onlLogFilePtr;

  if (logPartPtr.p->noLogFiles > 2) {
    jam();
/* -------------------------------------------------- */
/*       IF ONLY 1 OR 2 LOG FILES EXIST THEN THEY ARE */
/*       ALWAYS OPEN AND THUS IT IS NOT NECESSARY TO  */
/*       OPEN THEM NOW.                               */
/* -------------------------------------------------- */
    onlLogFilePtr.i = logFilePtr.p->nextLogFile;
    ptrCheckGuard(onlLogFilePtr, clogFileFileSize, logFileRecord);
    if (onlLogFilePtr.p->logFileStatus != LogFileRecord::CLOSED) {
      ndbrequire(onlLogFilePtr.p->fileNo == 0);
      return;
    }//if
    onlLogFilePtr.p->logFileStatus = LogFileRecord::OPENING_WRITE_LOG;
    signal->theData[0] = cownref;
    signal->theData[1] = onlLogFilePtr.i;
    signal->theData[2] = onlLogFilePtr.p->fileName[0];
    signal->theData[3] = onlLogFilePtr.p->fileName[1];
    signal->theData[4] = onlLogFilePtr.p->fileName[2];
    signal->theData[5] = onlLogFilePtr.p->fileName[3];
    signal->theData[6] = 2;
    sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, 7, JBA);
  }//if
}//Dblqh::openNextLogfile()

        /* OPEN FOR READ/WRITE, DON'T CREATE AND DON'T TRUNCATE FILE */
/* ------------------------------------------------------------------------- */
/* -------                       RELEASE LFO RECORD                  ------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void Dblqh::releaseLfo(Signal* signal) 
{
#ifdef VM_TRACE
  // Check that lfo record isn't already in free list
  LogFileOperationRecordPtr TlfoPtr;
  TlfoPtr.i = cfirstfreeLfo;
  while (TlfoPtr.i != RNIL){
    ptrCheckGuard(TlfoPtr, clfoFileSize, logFileOperationRecord);
    ndbrequire(TlfoPtr.i != lfoPtr.i);
    TlfoPtr.i = TlfoPtr.p->nextLfo;
  }
#endif
  lfoPtr.p->nextLfo = cfirstfreeLfo;
  lfoPtr.p->lfoTimer = 0;
  cfirstfreeLfo = lfoPtr.i;
  lfoPtr.p->lfoState = LogFileOperationRecord::IDLE;
}//Dblqh::releaseLfo()

/* ------------------------------------------------------------------------- */
/* ------- RELEASE ALL LOG PAGES CONNECTED TO A LFO RECORD           ------- */
/*                                                                           */
/*       SUBROUTINE SHORT NAME = RLP                                         */
/* ------------------------------------------------------------------------- */
void Dblqh::releaseLfoPages(Signal* signal) 
{
  LogPageRecordPtr rlpLogPagePtr;

  logPagePtr.i = lfoPtr.p->firstLfoPage;
RLP_LOOP:
  ptrCheckGuard(logPagePtr, clogPageFileSize, logPageRecord);
  rlpLogPagePtr.i = logPagePtr.p->logPageWord[ZNEXT_PAGE];
  releaseLogpage(signal);
  if (rlpLogPagePtr.i != RNIL) {
    jam();
    logPagePtr.i = rlpLogPagePtr.i;
    ptrCheckGuard(logPagePtr, clogPageFileSize, logPageRecord);
    goto RLP_LOOP;
  }//if
  lfoPtr.p->firstLfoPage = RNIL;
}//Dblqh::releaseLfoPages()

/* ------------------------------------------------------------------------- */
/* -------                       RELEASE LOG PAGE                    ------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void Dblqh::releaseLogpage(Signal* signal) 
{
#ifdef VM_TRACE
  // Check that log page isn't already in free list
  LogPageRecordPtr TlogPagePtr;
  TlogPagePtr.i = cfirstfreeLogPage;
  while (TlogPagePtr.i != RNIL){
    ptrCheckGuard(TlogPagePtr, clogPageFileSize, logPageRecord);
    ndbrequire(TlogPagePtr.i != logPagePtr.i);
    TlogPagePtr.i = TlogPagePtr.p->logPageWord[ZNEXT_PAGE];
  }
#endif

  cnoOfLogPages++;
  logPagePtr.p->logPageWord[ZNEXT_PAGE] = cfirstfreeLogPage;
  cfirstfreeLogPage = logPagePtr.i;
}//Dblqh::releaseLogpage()

/* ------------------------------------------------------------------------- */
/* -------       SEIZE LFO RECORD                                    ------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void Dblqh::seizeLfo(Signal* signal) 
{
  lfoPtr.i = cfirstfreeLfo;
  ptrCheckGuard(lfoPtr, clfoFileSize, logFileOperationRecord);
  cfirstfreeLfo = lfoPtr.p->nextLfo;
  lfoPtr.p->nextLfo = RNIL;
  lfoPtr.p->lfoTimer = cLqhTimeOutCount;
}//Dblqh::seizeLfo()

/* ------------------------------------------------------------------------- */
/* -------       SEIZE LOG FILE RECORD                               ------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void Dblqh::seizeLogfile(Signal* signal) 
{
  logFilePtr.i = cfirstfreeLogFile;
  ptrCheckGuard(logFilePtr, clogFileFileSize, logFileRecord);
/* ------------------------------------------------------------------------- */
/*IF LIST IS EMPTY THEN A SYSTEM CRASH IS INVOKED SINCE LOG_FILE_PTR = RNIL  */
/* ------------------------------------------------------------------------- */
  cfirstfreeLogFile = logFilePtr.p->nextLogFile;
  logFilePtr.p->nextLogFile = RNIL;
}//Dblqh::seizeLogfile()

/* ------------------------------------------------------------------------- */
/* -------       SEIZE LOG PAGE RECORD                               ------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void Dblqh::seizeLogpage(Signal* signal) 
{
  cnoOfLogPages--;
  logPagePtr.i = cfirstfreeLogPage;
  ptrCheckGuard(logPagePtr, clogPageFileSize, logPageRecord);
/* ------------------------------------------------------------------------- */
/*IF LIST IS EMPTY THEN A SYSTEM CRASH IS INVOKED SINCE LOG_PAGE_PTR = RNIL  */
/* ------------------------------------------------------------------------- */
  cfirstfreeLogPage = logPagePtr.p->logPageWord[ZNEXT_PAGE];
  logPagePtr.p->logPageWord[ZNEXT_PAGE] = RNIL;
}//Dblqh::seizeLogpage()

/* ------------------------------------------------------------------------- */
/* -------               WRITE FILE DESCRIPTOR INFORMATION           ------- */
/*                                                                           */
/*       SUBROUTINE SHORT NAME: WFD                                          */
// Pointer handling:
// logFilePtr in
// logPartPtr in
/* ------------------------------------------------------------------------- */
void Dblqh::writeFileDescriptor(Signal* signal) 
{
  TcConnectionrecPtr wfdTcConnectptr;
  UintR twfdFileNo;
  UintR twfdMbyte;

/* -------------------------------------------------- */
/*       START BY WRITING TO LOG FILE RECORD          */
/* -------------------------------------------------- */
  arrGuard(logFilePtr.p->currentMbyte, 16);
  logFilePtr.p->logMaxGciCompleted[logFilePtr.p->currentMbyte] = 
    logPartPtr.p->logPartNewestCompletedGCI;
  logFilePtr.p->logMaxGciStarted[logFilePtr.p->currentMbyte] = cnewestGci;
  wfdTcConnectptr.i = logPartPtr.p->firstLogTcrec;
  if (wfdTcConnectptr.i != RNIL) {
    jam();
    ptrCheckGuard(wfdTcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
    twfdFileNo = wfdTcConnectptr.p->logStartFileNo;
    twfdMbyte = wfdTcConnectptr.p->logStartPageNo >> ZTWOLOG_NO_PAGES_IN_MBYTE;
    logFilePtr.p->logLastPrepRef[logFilePtr.p->currentMbyte] = 
      (twfdFileNo << 16) + twfdMbyte;
  } else {
    jam();
    logFilePtr.p->logLastPrepRef[logFilePtr.p->currentMbyte] = 
      (logFilePtr.p->fileNo << 16) + logFilePtr.p->currentMbyte;
  }//if
}//Dblqh::writeFileDescriptor()

/* ------------------------------------------------------------------------- */
/* -------               WRITE THE HEADER PAGE OF A NEW FILE         ------- */
/*                                                                           */
/*       SUBROUTINE SHORT NAME:  WMO                                         */
/* ------------------------------------------------------------------------- */
void Dblqh::writeFileHeaderOpen(Signal* signal, Uint32 wmoType) 
{
  LogFileRecordPtr wmoLogFilePtr;
  UintR twmoNoLogDescriptors;
  UintR twmoLoop;
  UintR twmoIndex;

/* -------------------------------------------------- */
/*       WRITE HEADER INFORMATION IN THE NEW FILE.    */
/* -------------------------------------------------- */
  logPagePtr.p->logPageWord[ZPAGE_HEADER_SIZE + ZPOS_LOG_TYPE] = ZFD_TYPE;
  logPagePtr.p->logPageWord[ZPAGE_HEADER_SIZE + ZPOS_FILE_NO] = 
    logFilePtr.p->fileNo;
  if (logPartPtr.p->noLogFiles > ZMAX_LOG_FILES_IN_PAGE_ZERO) {
    jam();
    twmoNoLogDescriptors = ZMAX_LOG_FILES_IN_PAGE_ZERO;
  } else {
    jam();
    twmoNoLogDescriptors = logPartPtr.p->noLogFiles;
  }//if
  logPagePtr.p->logPageWord[ZPAGE_HEADER_SIZE + ZPOS_NO_FD] = 
    twmoNoLogDescriptors;
  wmoLogFilePtr.i = logFilePtr.i;
  twmoLoop = 0;
WMO_LOOP:
  jam();
  if (twmoLoop < twmoNoLogDescriptors) {
    jam();
    ptrCheckGuard(wmoLogFilePtr, clogFileFileSize, logFileRecord);
    for (twmoIndex = 0; twmoIndex <= ZNO_MBYTES_IN_FILE - 1; twmoIndex++) {
      jam();
      arrGuard(((ZPAGE_HEADER_SIZE + ZFD_HEADER_SIZE) + 
                (twmoLoop * ZFD_PART_SIZE)) + twmoIndex, ZPAGE_SIZE);
      logPagePtr.p->logPageWord[((ZPAGE_HEADER_SIZE + ZFD_HEADER_SIZE) + 
                   (twmoLoop * ZFD_PART_SIZE)) + twmoIndex] = 
            wmoLogFilePtr.p->logMaxGciCompleted[twmoIndex];
      arrGuard((((ZPAGE_HEADER_SIZE + ZFD_HEADER_SIZE) +
                 (twmoLoop * ZFD_PART_SIZE)) + ZNO_MBYTES_IN_FILE) +
                  twmoIndex, ZPAGE_SIZE);
      logPagePtr.p->logPageWord[(((ZPAGE_HEADER_SIZE + ZFD_HEADER_SIZE) + 
           (twmoLoop * ZFD_PART_SIZE)) + ZNO_MBYTES_IN_FILE) + twmoIndex] = 
         wmoLogFilePtr.p->logMaxGciStarted[twmoIndex];
      arrGuard((((ZPAGE_HEADER_SIZE + ZFD_HEADER_SIZE) +
        (twmoLoop * ZFD_PART_SIZE)) + (2 * ZNO_MBYTES_IN_FILE)) +
         twmoIndex, ZPAGE_SIZE);
      logPagePtr.p->logPageWord[(((ZPAGE_HEADER_SIZE + ZFD_HEADER_SIZE) + 
        (twmoLoop * ZFD_PART_SIZE)) + (2 * ZNO_MBYTES_IN_FILE)) + twmoIndex] = 
          wmoLogFilePtr.p->logLastPrepRef[twmoIndex];
    }//for
    wmoLogFilePtr.i = wmoLogFilePtr.p->prevLogFile;
    twmoLoop = twmoLoop + 1;
    goto WMO_LOOP;
  }//if
  logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] = 
    (ZPAGE_HEADER_SIZE + ZFD_HEADER_SIZE) +
    (ZFD_PART_SIZE * twmoNoLogDescriptors);
  arrGuard(logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX], ZPAGE_SIZE);
  logPagePtr.p->logPageWord[logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX]] = 
       ZNEXT_LOG_RECORD_TYPE;
/* ------------------------------------------------------- */
/*       THIS IS A SPECIAL WRITE OF THE FIRST PAGE IN THE  */
/*       LOG FILE. THIS HAS SPECIAL SIGNIFANCE TO FIND     */
/*       THE END OF THE LOG AT SYSTEM RESTART.             */
/* ------------------------------------------------------- */
  writeSinglePage(signal, 0, ZPAGE_SIZE - 1);
  if (wmoType == ZINIT) {
    jam();
    lfoPtr.p->lfoState = LogFileOperationRecord::INIT_FIRST_PAGE;
  } else {
    jam();
    lfoPtr.p->lfoState = LogFileOperationRecord::FIRST_PAGE_WRITE_IN_LOGFILE;
  }//if
  logFilePtr.p->filePosition = 1;
  if (wmoType == ZNORMAL) {
    jam();
/* -------------------------------------------------- */
/*       ALLOCATE A NEW PAGE SINCE THE CURRENT IS     */
/*       WRITTEN.                                     */
/* -------------------------------------------------- */
    seizeLogpage(signal);
    initLogpage(signal);
    logFilePtr.p->currentLogpage = logPagePtr.i;
    logFilePtr.p->currentFilepage = logFilePtr.p->currentFilepage + 1;
  }//if
}//Dblqh::writeFileHeaderOpen()

/* -------------------------------------------------- */
/*       THE NEW FILE POSITION WILL ALWAYS BE 1 SINCE */
/*       WE JUST WROTE THE FIRST PAGE IN THE LOG FILE */
/* -------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/* -------               WRITE A MBYTE HEADER DURING INITIAL START   ------- */
/*                                                                           */
/*       SUBROUTINE SHORT NAME: WIM                                          */
/* ------------------------------------------------------------------------- */
void Dblqh::writeInitMbyte(Signal* signal) 
{
  initLogpage(signal);
  writeSinglePage(signal, logFilePtr.p->currentMbyte * ZPAGES_IN_MBYTE, ZPAGE_SIZE - 1);
  lfoPtr.p->lfoState = LogFileOperationRecord::WRITE_INIT_MBYTE;
}//Dblqh::writeInitMbyte()

/* ------------------------------------------------------------------------- */
/* -------               WRITE A SINGLE PAGE INTO A FILE             ------- */
/*                                                                           */
/*       INPUT:          TWSP_PAGE_NO    THE PAGE NUMBER WRITTEN             */
/*       SUBROUTINE SHORT NAME:  WSP                                         */
/* ------------------------------------------------------------------------- */
void Dblqh::writeSinglePage(Signal* signal, Uint32 pageNo, Uint32 wordWritten) 
{
  seizeLfo(signal);
  initLfo(signal);
  lfoPtr.p->firstLfoPage = logPagePtr.i;
  logPagePtr.p->logPageWord[ZNEXT_PAGE] = RNIL;

  // Calculate checksum for page
  logPagePtr.p->logPageWord[ZPOS_CHECKSUM] = calcPageCheckSum(logPagePtr);

  lfoPtr.p->lfoPageNo = pageNo;
  lfoPtr.p->lfoWordWritten = wordWritten;
  lfoPtr.p->noPagesRw = 1;
/* -------------------------------------------------- */
/*       SET TIMER ON THIS LOG PART TO SIGNIFY THAT A */
/*       LOG RECORD HAS BEEN SENT AT THIS TIME.       */
/* -------------------------------------------------- */
  logPartPtr.p->logPartTimer = logPartPtr.p->logTimer;
  signal->theData[0] = logFilePtr.p->fileRef;
  signal->theData[1] = cownref;
  signal->theData[2] = lfoPtr.i;
  signal->theData[3] = ZLIST_OF_PAIRS_SYNCH;
  signal->theData[4] = ZVAR_NO_LOG_PAGE_WORD;
  signal->theData[5] = 1;                     /* ONE PAGE WRITTEN */
  signal->theData[6] = logPagePtr.i;
  signal->theData[7] = pageNo;
  sendSignal(NDBFS_REF, GSN_FSWRITEREQ, signal, 8, JBA);
}//Dblqh::writeSinglePage()

/* ##########################################################################
 *     SYSTEM RESTART PHASE ONE MODULE
 *     THIS MODULE IS A SUB-MODULE OF THE FILE SYSTEM HANDLING.
 *
 *     THIS MODULE CONTAINS THE CODE FOR THE FIRST PHASE OF THE SYSTEM RESTART.
 *     THE AIM OF THIS PHASE IS TO FIND THE END OF THE LOG AND TO FIND 
 *     INFORMATION ABOUT WHERE GLOBAL CHECKPOINTS ARE COMPLETED AND STARTED 
 *     IN THE LOG. THIS INFORMATION IS NEEDED TO START PHASE THREE OF 
 *     THE SYSTEM RESTART.
 * ########################################################################## */
/* --------------------------------------------------------------------------
 *     A SYSTEM RESTART OR NODE RESTART IS ONGOING. WE HAVE NOW OPENED FILE 0
 *     NOW WE NEED TO READ PAGE 0 TO FIND WHICH LOG FILE THAT WAS OPEN AT 
 *     CRASH TIME.
 * -------------------------------------------------------------------------- */
void Dblqh::openSrFrontpageLab(Signal* signal) 
{
  readSinglePage(signal, 0);
  lfoPtr.p->lfoState = LogFileOperationRecord::READ_SR_FRONTPAGE;
  return;
}//Dblqh::openSrFrontpageLab()

/* -------------------------------------------------------------------------
 * WE HAVE NOW READ PAGE 0 IN FILE 0. CHECK THE LAST OPEN FILE. ACTUALLY THE
 * LAST OPEN FILE COULD BE THE NEXT AFTER THAT. CHECK THAT FIRST. WHEN THE  
 * LAST WAS FOUND WE CAN FIND ALL THE NEEDED INFORMATION WHERE TO START AND  
 * STOP READING THE LOG.
 * -------------------------------------------------------------------------- */
void Dblqh::readSrFrontpageLab(Signal* signal) 
{
  Uint32 fileNo = logPagePtr.p->logPageWord[ZPAGE_HEADER_SIZE + ZPOS_FILE_NO];
  if (fileNo == 0) {
    jam();
    /* ----------------------------------------------------------------------
     *       FILE 0 WAS ALSO LAST FILE SO WE DO NOT NEED TO READ IT AGAIN.
     * ---------------------------------------------------------------------- */
    readSrLastFileLab(signal);
    return;
  }//if
  /* ------------------------------------------------------------------------
   *    CLOSE FILE 0 SO THAT WE HAVE CLOSED ALL FILES WHEN STARTING TO READ 
   *    THE FRAGMENT LOG. ALSO RELEASE PAGE ZERO.
   * ------------------------------------------------------------------------ */
  releaseLogpage(signal);
  logFilePtr.p->logFileStatus = LogFileRecord::CLOSING_SR;
  closeFile(signal, logFilePtr);
  LogFileRecordPtr locLogFilePtr;
  findLogfile(signal, fileNo, logPartPtr, &locLogFilePtr);
  locLogFilePtr.p->logFileStatus = LogFileRecord::OPEN_SR_LAST_FILE;
  openFileRw(signal, locLogFilePtr);
  return;
}//Dblqh::readSrFrontpageLab()

void Dblqh::openSrLastFileLab(Signal* signal) 
{
  readSinglePage(signal, 0);
  lfoPtr.p->lfoState = LogFileOperationRecord::READ_SR_LAST_FILE;
  return;
}//Dblqh::openSrLastFileLab()

void Dblqh::readSrLastFileLab(Signal* signal) 
{
  logPartPtr.p->logLap = logPagePtr.p->logPageWord[ZPOS_LOG_LAP];
  if (logPartPtr.p->noLogFiles > ZMAX_LOG_FILES_IN_PAGE_ZERO) {
    jam();
    initGciInLogFileRec(signal, ZMAX_LOG_FILES_IN_PAGE_ZERO);
  } else {
    jam();
    initGciInLogFileRec(signal, logPartPtr.p->noLogFiles);
  }//if
  releaseLogpage(signal);
  /* ------------------------------------------------------------------------
   *    NOW WE HAVE FOUND THE LAST LOG FILE. WE ALSO NEED TO FIND THE LAST
   *    MBYTE THAT WAS LAST WRITTEN BEFORE THE SYSTEM CRASH.
   * ------------------------------------------------------------------------ */
  logPartPtr.p->lastLogfile = logFilePtr.i;
  readSinglePage(signal, 0);
  lfoPtr.p->lfoState = LogFileOperationRecord::READ_SR_LAST_MBYTE;
  logFilePtr.p->currentMbyte = 0;
  return;
}//Dblqh::readSrLastFileLab()

void Dblqh::readSrLastMbyteLab(Signal* signal) 
{
  if (logPartPtr.p->lastMbyte == ZNIL) {
    if (logPagePtr.p->logPageWord[ZPOS_LOG_LAP] < logPartPtr.p->logLap) {
      jam();
      logPartPtr.p->lastMbyte = logFilePtr.p->currentMbyte - 1;
    }//if
  }//if
  arrGuard(logFilePtr.p->currentMbyte, 16);
  logFilePtr.p->logMaxGciCompleted[logFilePtr.p->currentMbyte] = 
    logPagePtr.p->logPageWord[ZPOS_MAX_GCI_COMPLETED];
  logFilePtr.p->logMaxGciStarted[logFilePtr.p->currentMbyte] = 
    logPagePtr.p->logPageWord[ZPOS_MAX_GCI_STARTED];
  logFilePtr.p->logLastPrepRef[logFilePtr.p->currentMbyte] = 
    logPagePtr.p->logPageWord[ZLAST_LOG_PREP_REF];
  releaseLogpage(signal);
  if (logFilePtr.p->currentMbyte < (ZNO_MBYTES_IN_FILE - 1)) {
    jam();
    logFilePtr.p->currentMbyte++;
    readSinglePage(signal, ZPAGES_IN_MBYTE * logFilePtr.p->currentMbyte);
    lfoPtr.p->lfoState = LogFileOperationRecord::READ_SR_LAST_MBYTE;
    return;
  } else {
    jam();
    /* ----------------------------------------------------------------------
     *    THE LOG WAS IN THE LAST MBYTE WHEN THE CRASH OCCURRED SINCE ALL 
     *    LOG LAPS ARE EQUAL TO THE CURRENT LOG LAP.
     * ---------------------------------------------------------------------- */
    if (logPartPtr.p->lastMbyte == ZNIL) {
      jam();
      logPartPtr.p->lastMbyte = ZNO_MBYTES_IN_FILE - 1;
    }//if
  }//if
  logFilePtr.p->logFileStatus = LogFileRecord::CLOSING_SR;
  closeFile(signal, logFilePtr);
  if (logPartPtr.p->noLogFiles > ZMAX_LOG_FILES_IN_PAGE_ZERO) {
    Uint32 fileNo;
    if (logFilePtr.p->fileNo >= ZMAX_LOG_FILES_IN_PAGE_ZERO) {
      jam();
      fileNo = logFilePtr.p->fileNo - ZMAX_LOG_FILES_IN_PAGE_ZERO;
    } else {
      jam();
      fileNo = 
	(logPartPtr.p->noLogFiles + logFilePtr.p->fileNo) - 
	ZMAX_LOG_FILES_IN_PAGE_ZERO;
    }//if
    if (fileNo == 0) {
      jam();
      /* --------------------------------------------------------------------
       *  AVOID USING FILE 0 AGAIN SINCE THAT IS PROBABLY CLOSING AT THE 
       *  MOMENT.
       * -------------------------------------------------------------------- */
      fileNo = 1;
      logPartPtr.p->srRemainingFiles = 
	logPartPtr.p->noLogFiles - (ZMAX_LOG_FILES_IN_PAGE_ZERO - 1);
    } else {
      jam();
      logPartPtr.p->srRemainingFiles = 
	logPartPtr.p->noLogFiles - ZMAX_LOG_FILES_IN_PAGE_ZERO;
    }//if
    LogFileRecordPtr locLogFilePtr;
    findLogfile(signal, fileNo, logPartPtr, &locLogFilePtr);
    locLogFilePtr.p->logFileStatus = LogFileRecord::OPEN_SR_NEXT_FILE;
    openFileRw(signal, locLogFilePtr);
    return;
  }//if
  /* ------------------------------------------------------------------------
   *   THERE WERE NO NEED TO READ ANY MORE PAGE ZERO IN OTHER FILES. 
   *   WE NOW HAVE ALL THE NEEDED INFORMATION ABOUT THE GCI'S THAT WE NEED. 
   *   NOW JUST WAIT FOR CLOSE OPERATIONS TO COMPLETE.
   * ------------------------------------------------------------------------ */
  return;
}//Dblqh::readSrLastMbyteLab()

void Dblqh::openSrNextFileLab(Signal* signal) 
{
  readSinglePage(signal, 0);
  lfoPtr.p->lfoState = LogFileOperationRecord::READ_SR_NEXT_FILE;
  return;
}//Dblqh::openSrNextFileLab()

void Dblqh::readSrNextFileLab(Signal* signal) 
{
  if (logPartPtr.p->srRemainingFiles > ZMAX_LOG_FILES_IN_PAGE_ZERO) {
    jam();
    initGciInLogFileRec(signal, ZMAX_LOG_FILES_IN_PAGE_ZERO);
  } else {
    jam();
    initGciInLogFileRec(signal, logPartPtr.p->srRemainingFiles);
  }//if
  releaseLogpage(signal);
  logFilePtr.p->logFileStatus = LogFileRecord::CLOSING_SR;
  closeFile(signal, logFilePtr);
  if (logPartPtr.p->srRemainingFiles > ZMAX_LOG_FILES_IN_PAGE_ZERO) {
    Uint32 fileNo;
    if (logFilePtr.p->fileNo >= ZMAX_LOG_FILES_IN_PAGE_ZERO) {
      jam();
      fileNo = logFilePtr.p->fileNo - ZMAX_LOG_FILES_IN_PAGE_ZERO;
    } else {
      jam();
      fileNo = 
	(logPartPtr.p->noLogFiles + logFilePtr.p->fileNo) - 
	ZMAX_LOG_FILES_IN_PAGE_ZERO;
    }//if
    if (fileNo == 0) {
      jam();
      /* --------------------------------------------------------------------
       * AVOID USING FILE 0 AGAIN SINCE THAT IS PROBABLY CLOSING AT THE MOMENT.
       * -------------------------------------------------------------------- */
      fileNo = 1;
      logPartPtr.p->srRemainingFiles = 
	logPartPtr.p->srRemainingFiles - (ZMAX_LOG_FILES_IN_PAGE_ZERO - 1);
    } else {
      jam();
      logPartPtr.p->srRemainingFiles = 
	logPartPtr.p->srRemainingFiles - ZMAX_LOG_FILES_IN_PAGE_ZERO;
    }//if
    LogFileRecordPtr locLogFilePtr;
    findLogfile(signal, fileNo, logPartPtr, &locLogFilePtr);
    locLogFilePtr.p->logFileStatus = LogFileRecord::OPEN_SR_NEXT_FILE;
    openFileRw(signal, locLogFilePtr);
  }//if
  /* ------------------------------------------------------------------------
   *   THERE WERE NO NEED TO READ ANY MORE PAGE ZERO IN OTHER FILES. 
   *   WE NOW HAVE ALL THE NEEDED INFORMATION ABOUT THE GCI'S THAT WE NEED. 
   *   NOW JUST WAIT FOR CLOSE OPERATIONS TO COMPLETE.
   * ------------------------------------------------------------------------ */
  return;
}//Dblqh::readSrNextFileLab()

void Dblqh::closingSrLab(Signal* signal) 
{
  logFilePtr.p->logFileStatus = LogFileRecord::CLOSED;
  logPartPtr.i = logFilePtr.p->logPartRec;
  ptrCheckGuard(logPartPtr, clogPartFileSize, logPartRecord);
  logFilePtr.i = logPartPtr.p->firstLogfile;
  do {
    jam();
    ptrCheckGuard(logFilePtr, clogFileFileSize, logFileRecord);
    if (logFilePtr.p->logFileStatus != LogFileRecord::CLOSED) {
      jam();
      /* --------------------------------------------------------------------
       *  EXIT AND WAIT FOR REMAINING LOG FILES TO COMPLETE THEIR WORK.
       * -------------------------------------------------------------------- */
      return;
    }//if
    logFilePtr.i = logFilePtr.p->nextLogFile;
  } while (logFilePtr.i != logPartPtr.p->firstLogfile);
  /* ------------------------------------------------------------------------
   *  ALL FILES IN THIS PART HAVE BEEN CLOSED. THIS INDICATES THAT THE FIRST
   *  PHASE OF THE SYSTEM RESTART HAVE BEEN CONCLUDED FOR THIS LOG PART.
   *  CHECK IF ALL OTHER LOG PARTS ARE ALSO COMPLETED.
   * ------------------------------------------------------------------------ */
  logPartPtr.p->logPartState = LogPartRecord::SR_FIRST_PHASE_COMPLETED;
  for (logPartPtr.i = 0; logPartPtr.i <= 3; logPartPtr.i++) {
    jam();
    ptrAss(logPartPtr, logPartRecord);
    if (logPartPtr.p->logPartState != LogPartRecord::SR_FIRST_PHASE_COMPLETED) {
      jam();
      /* --------------------------------------------------------------------
       * EXIT AND WAIT FOR THE REST OF THE LOG PARTS TO COMPLETE.
       * -------------------------------------------------------------------- */
      return;
    }//if
  }//for
  /* ------------------------------------------------------------------------
   *       THE FIRST PHASE HAVE BEEN COMPLETED.
   * ------------------------------------------------------------------------ */
  signal->theData[0] = ZSR_PHASE3_START;
  signal->theData[1] = ZSR_PHASE1_COMPLETED;
  sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
  return;
}//Dblqh::closingSrLab()

/* ##########################################################################
 * #######                  SYSTEM RESTART PHASE TWO MODULE           ####### 
 *
 *  THIS MODULE HANDLES THE SYSTEM RESTART WHERE LQH CONTROLS TUP AND ACC TO
 *  ENSURE THAT THEY HAVE KNOWLEDGE OF ALL FRAGMENTS AND HAVE DONE THE NEEDED
 *  READING OF DATA FROM FILE AND EXECUTION OF LOCAL LOGS. THIS PROCESS
 *  EXECUTES CONCURRENTLY WITH PHASE ONE OF THE SYSTEM RESTART. THIS PHASE
 *  FINDS THE INFORMATION ABOUT THE FRAGMENT LOG NEEDED TO EXECUTE THE FRAGMENT
 *  LOG.
 *  WHEN TUP AND ACC HAVE PREPARED ALL FRAGMENTS THEN LQH ORDERS THOSE LQH'S
 *  THAT ARE RESPONSIBLE TO EXECUTE THE FRAGMENT LOGS TO DO SO. IT IS POSSIBLE 
 *  THAT ANOTHER NODE EXECUTES THE LOG FOR A FRAGMENT RESIDING AT THIS NODE.
 * ########################################################################## */
/* ***************>> */
/*  START_FRAGREQ  > */
/* ***************>> */
void Dblqh::execSTART_FRAGREQ(Signal* signal) 
{
  const StartFragReq * const startFragReq = (StartFragReq *)&signal->theData[0];
  jamEntry();

  tabptr.i = startFragReq->tableId;
  Uint32 fragId = startFragReq->fragId;

  ptrCheckGuard(tabptr, ctabrecFileSize, tablerec);
  if (!getFragmentrec(signal, fragId)) {
    startFragRefLab(signal);
    return;
  }//if
  tabptr.p->tableStatus = Tablerec::TABLE_DEFINED;
  
  initFragrecSr(signal);
  if (startFragReq->lcpNo == ZNIL) {
    jam();
    /* ----------------------------------------------------------------------
     *  THERE WAS NO LOCAL CHECKPOINT AVAILABLE FOR THIS FRAGMENT. WE DO 
     *  NOT NEED TO READ IN THE LOCAL FRAGMENT. WE HAVE ALREADY ADDED THE 
     *  FRAGMENT AS AN EMPTY FRAGMENT AT THIS POINT. THUS WE CAN SIMPLY 
     *  EXIT AND THE FRAGMENT WILL PARTICIPATE IN THE EXECUTION OF THE LOG.
     *  PUT FRAGMENT ON LIST OF COMPLETED FRAGMENTS FOR EXECUTION OF LOG.
     * ---------------------------------------------------------------------- */
    fragptr.p->nextFrag = cfirstCompletedFragSr;
    cfirstCompletedFragSr = fragptr.i;
    return;
  }//if
  if (cfirstWaitFragSr == RNIL) {
    jam();
      lcpPtr.i = 0;
    ptrAss(lcpPtr, lcpRecord);
    if (lcpPtr.p->lcpState == LcpRecord::LCP_IDLE) {
      jam();
      initLcpSr(signal, startFragReq->lcpNo,
                startFragReq->lcpId, tabptr.i,
                fragId, fragptr.i);
      signal->theData[0] = lcpPtr.i;
      signal->theData[1] = cownref;
      signal->theData[2] = lcpPtr.p->currentFragment.lcpFragOrd.lcpNo;
      signal->theData[3] = lcpPtr.p->currentFragment.lcpFragOrd.tableId;
      signal->theData[4] = lcpPtr.p->currentFragment.lcpFragOrd.fragmentId;
      sendSignal(fragptr.p->accBlockref, GSN_SR_FRAGIDREQ, signal, 5, JBB);
      return;
    }//if
  }//if
  fragptr.p->nextFrag = cfirstWaitFragSr;
  cfirstWaitFragSr = fragptr.i;
}//Dblqh::execSTART_FRAGREQ()

void Dblqh::startFragRefLab(Signal* signal) 
{
  const StartFragReq * const startFragReq = (StartFragReq *)&signal->theData[0];
  BlockReference userRef = startFragReq->userRef;
  Uint32 userPtr = startFragReq->userPtr;
  signal->theData[0] = userPtr;
  signal->theData[1] = terrorCode;
  signal->theData[2] = cownNodeid;
  sendSignal(userRef, GSN_START_FRAGREF, signal, 3, JBB);
  return;
}//Dblqh::startFragRefLab()

/* ***************>> */
/*  SR_FRAGIDCONF  > */
/* ***************>> */
/* --------------------------------------------------------------------------
 *       PRECONDITION: LCP_PTR:LCP_STATE = SR_WAIT_FRAGID 
 * -------------------------------------------------------------------------- */
void Dblqh::execSR_FRAGIDCONF(Signal* signal) 
{
  SrFragidConf * const srFragidConf = (SrFragidConf *)&signal->theData[0];
  jamEntry();

  lcpPtr.i = srFragidConf->lcpPtr;
  ptrCheckGuard(lcpPtr, clcpFileSize, lcpRecord);
  ndbrequire(lcpPtr.p->lcpState == LcpRecord::LCP_SR_WAIT_FRAGID);
  /* ------------------------------------------------------------------------
   *  NO ERROR CHECKING OF TNO_LOCFRAG VALUE. OUT OF BOUND WILL IMPLY THAT AN
   *  INDEX OUT OF RANGE WILL CAUSE A SYSTEM RESTART WHICH IS DESIRED.
   * ------------------------------------------------------------------------ */
  lcpPtr.p->lcpAccptr = srFragidConf->accPtr;
  fragptr.i = lcpPtr.p->currentFragment.fragPtrI;
  ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
  fragptr.p->accFragptr[0] = srFragidConf->fragPtr[0];
  fragptr.p->accFragptr[1] = srFragidConf->fragPtr[1];
  Uint32 noLocFrag = srFragidConf->noLocFrag;
  ndbrequire(noLocFrag == 2);
  Uint32 fragid[2];
  Uint32 i;
  for (i = 0; i < noLocFrag; i++) {
    fragid[i] = srFragidConf->fragId[i];
  }//for

  for (i = 0; i < noLocFrag; i++) {
    jam();
    Uint32 fragId = fragid[i];
    /* ----------------------------------------------------------------------
     *  THERE IS NO ERROR CHECKING ON PURPOSE. IT IS POSSIBLE TO CALCULATE HOW
     *  MANY LOCAL LCP RECORDS THERE SHOULD BE. IT SHOULD NEVER HAPPEN THAT 
     *  THERE IS NO ONE FREE. IF THERE IS NO ONE IT WILL ALSO BE A POINTER 
     *  OUT OF RANGE WHICH IS AN ERROR CODE IN ITSELF. REUSES ERROR 
     *  HANDLING IN AXE VM.
     * ---------------------------------------------------------------------- */
    seizeLcpLoc(signal);
    initLcpLocAcc(signal, fragId);
    lcpLocptr.p->lcpLocstate = LcpLocRecord::SR_ACC_STARTED;
    signal->theData[0] = lcpPtr.p->lcpAccptr;
    signal->theData[1] = lcpLocptr.i;
    signal->theData[2] = lcpLocptr.p->locFragid;
    signal->theData[3] = lcpPtr.p->currentFragment.lcpFragOrd.lcpId % MAX_LCP_STORED;
    sendSignal(fragptr.p->accBlockref, GSN_ACC_SRREQ, signal, 4, JBB);
    seizeLcpLoc(signal);
    initLcpLocTup(signal, fragId);
    lcpLocptr.p->lcpLocstate = LcpLocRecord::SR_TUP_STARTED;
    signal->theData[0] = lcpLocptr.i;
    signal->theData[1] = cownref;
    signal->theData[2] = lcpPtr.p->currentFragment.lcpFragOrd.tableId;
    signal->theData[3] = lcpLocptr.p->locFragid;
    signal->theData[4] = lcpPtr.p->currentFragment.lcpFragOrd.lcpNo;
    sendSignal(fragptr.p->tupBlockref, GSN_TUP_SRREQ, signal, 5, JBB);
  }//for
  lcpPtr.p->lcpState = LcpRecord::LCP_SR_STARTED;
  return;
}//Dblqh::execSR_FRAGIDCONF()

/* ***************> */
/*  SR_FRAGIDREF  > */
/* ***************> */
void Dblqh::execSR_FRAGIDREF(Signal* signal) 
{
  jamEntry();
  ndbrequire(false);
}//Dblqh::execSR_FRAGIDREF()

/* ************>> */
/*  ACC_SRCONF  > */
/* ************>> */
/* --------------------------------------------------------------------------
 *       PRECONDITION: LCP_LOCPTR:LCP_LOCSTATE = SR_ACC_STARTED
 * -------------------------------------------------------------------------- */
void Dblqh::execACC_SRCONF(Signal* signal) 
{
  jamEntry();
  lcpLocptr.i = signal->theData[0];
  ptrCheckGuard(lcpLocptr, clcpLocrecFileSize, lcpLocRecord);
  if (lcpLocptr.p->lcpLocstate != LcpLocRecord::SR_ACC_STARTED) {
    jam();
    systemErrorLab(signal);
    return;
  }//if

  lcpPtr.i = lcpLocptr.p->masterLcpRec;
  ptrCheckGuard(lcpPtr, clcpFileSize, lcpRecord);
  /* ------------------------------------------------------------------------
   *  NO ERROR CHECK ON USING VALUE IN MASTER_LCP_REC. ERROR IN THIS REFERENCE
   *  WILL CAUSE POINTER OUT OF RANGE WHICH CAUSES A SYSTEM RESTART.
   * ------------------------------------------------------------------------ */
  lcpLocptr.p->lcpLocstate = LcpLocRecord::SR_ACC_COMPLETED;
  srCompletedLab(signal);
  return;
}//Dblqh::execACC_SRCONF()

/* ************> */
/*  ACC_SRREF  > */
/* ************> */
void Dblqh::execACC_SRREF(Signal* signal) 
{
  jamEntry();
  terrorCode = signal->theData[1];
  systemErrorLab(signal);
  return;
}//Dblqh::execACC_SRREF()

/* ************>> */
/*  TUP_SRCONF  > */
/* ************>> */
/* --------------------------------------------------------------------------
 *       PRECONDITION: LCP_LOCPTR:LCP_LOCSTATE = SR_TUP_STARTED
 * -------------------------------------------------------------------------- */
void Dblqh::execTUP_SRCONF(Signal* signal) 
{
  jamEntry();
  lcpLocptr.i = signal->theData[0];
  ptrCheckGuard(lcpLocptr, clcpLocrecFileSize, lcpLocRecord);
  Uint32 tupFragPtr = signal->theData[1];
  ndbrequire(lcpLocptr.p->lcpLocstate == LcpLocRecord::SR_TUP_STARTED);

  lcpPtr.i = lcpLocptr.p->masterLcpRec;
  ptrCheckGuard(lcpPtr, clcpFileSize, lcpRecord);
  /* ------------------------------------------------------------------------
   *  NO ERROR CHECK ON USING VALUE IN MASTER_LCP_REC. ERROR IN THIS REFERENCE
   *  WILL CAUSE POINTER OUT OF RANGE WHICH CAUSES A SYSTEM RESTART.
   * ------------------------------------------------------------------------ */
  lcpLocptr.p->lcpLocstate = LcpLocRecord::SR_TUP_COMPLETED;
  fragptr.i = lcpPtr.p->currentFragment.fragPtrI;
  ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
  if (lcpLocptr.i == lcpPtr.p->firstLcpLocTup) {
    jam();
    fragptr.p->tupFragptr[1] = tupFragPtr;
  } else {
    jam();
    fragptr.p->tupFragptr[0] = tupFragPtr;
  }//if
  srCompletedLab(signal);
  return;
}//Dblqh::execTUP_SRCONF()

void Dblqh::srCompletedLab(Signal* signal) 
{
  checkSrCompleted(signal);
  if (lcpPtr.p->lcpState == LcpRecord::LCP_SR_COMPLETED) {
    jam();
    /* ----------------------------------------------------------------------
     *  THE SYSTEM RESTART OF THIS FRAGMENT HAS BEEN COMPLETED. IT IS NOW
     *  TIME TO START A SYSTEM RESTART ON THE NEXT FRAGMENT OR CONTINUE 
     *  WITH THE NEXT STEP OF THE SYSTEM RESTART. THIS STEP IS TO EXECUTE 
     *  THE FRAGMENT LOGS.
     * ----------------------------------------------------------------------
     *  WE RELEASE THE LOCAL LCP RECORDS.
     * --------------------------------------------------------------------- */
    releaseLocalLcps(signal);
    /* ----------------------------------------------------------------------
     *  PUT FRAGMENT ON LIST OF FRAGMENTS WHICH HAVE BEEN STARTED AS PART OF 
     *  THE SYSTEM RESTART. THEY ARE NOW WAITING TO EXECUTE THE FRAGMENT LOG.
     * --------------------------------------------------------------------- */
    fragptr.i = lcpPtr.p->currentFragment.fragPtrI;
    ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
    fragptr.p->nextFrag = cfirstCompletedFragSr;
    cfirstCompletedFragSr = fragptr.i;
    if (cfirstWaitFragSr != RNIL) {
      jam();
      /* --------------------------------------------------------------------
       *  ANOTHER FRAGMENT IS WAITING FOR SYSTEM RESTART. RESTART THIS 
       *  FRAGMENT AS WELL.
       * -------------------------------------------------------------------- */
      fragptr.i = cfirstWaitFragSr;
      ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
      cfirstWaitFragSr = fragptr.p->nextFrag;
      /* --------------------------------------------------------------------
       *  RETRIEVE DATA FROM THE FRAGMENT RECORD.
       * -------------------------------------------------------------------- */
      ndbrequire(fragptr.p->srChkpnr < MAX_LCP_STORED);
      initLcpSr(signal,
                fragptr.p->srChkpnr,
                fragptr.p->lcpId[fragptr.p->srChkpnr],
                fragptr.p->tabRef,
                fragptr.p->fragId,
                fragptr.i);
      signal->theData[0] = lcpPtr.i;
      signal->theData[1] = cownref;
      signal->theData[2] = lcpPtr.p->currentFragment.lcpFragOrd.lcpNo;
      signal->theData[3] = lcpPtr.p->currentFragment.lcpFragOrd.tableId;
      signal->theData[4] = lcpPtr.p->currentFragment.lcpFragOrd.fragmentId;
      sendSignal(fragptr.p->accBlockref, GSN_SR_FRAGIDREQ, signal, 5, JBB);
      return;
    } else {
      jam();
      /* --------------------------------------------------------------------
       *  NO MORE FRAGMENTS ARE WAITING FOR SYSTEM RESTART.
       * -------------------------------------------------------------------- */
      lcpPtr.p->lcpState = LcpRecord::LCP_IDLE;
      if (cstartRecReq == ZTRUE) {
        jam();
	/* ----------------------------------------------------------------
         *  WE HAVE ALSO RECEIVED AN INDICATION THAT NO MORE FRAGMENTS 
	 *  NEEDS RESTART.
	 *  NOW IT IS TIME TO START EXECUTING THE UNDO LOG.
	 * ----------------------------------------------------------------
	 *  WE ARE NOW IN A POSITION TO ORDER TUP AND ACC TO START 
	 *  EXECUTING THEIR UNDO LOGS. THIS MUST BE DONE BEFORE THE 
	 *  FRAGMENT LOGS CAN BE EXECUTED.
	 * ---------------------------------------------------------------- */
        csrExecUndoLogState = EULS_STARTED;
        signal->theData[0] = caccBlockref;
        signal->theData[1] = cownref;
        sendSignal(caccBlockref, GSN_START_RECREQ, signal, 2, JBB);
        signal->theData[0] = ctupBlockref;
        signal->theData[1] = cownref;
        sendSignal(ctupBlockref, GSN_START_RECREQ, signal, 2, JBB);
        return;
      } else {
        jam();
	/* ----------------------------------------------------------------
	 *  WE HAVE NOT RECEIVED ALL FRAGMENTS YET OR AT LEAST NOT WE 
	 *  HAVE NOT RECEIVED THE START_RECREQ SIGNAL. EXIT AND WAIT 
	 *  FOR MORE.
	 * ---------------------------------------------------------------- */
        return;
      }//if
    }//if
  }//if
  /*---------------*/
  /*       ELSE    */
  /*-------------------------------------------------------------------------
   *       THE SYSTEM RESTART ON THIS FRAGMENT HAS NOT BEEN COMPLETED, 
   *       EXIT AND WAIT FOR MORE SIGNALS
   *-------------------------------------------------------------------------
   *       DO NOTHING, EXIT IS EXECUTED BELOW
   *------------------------------------------------------------------------- */
  return;
}//Dblqh::srCompletedLab()

/* ************> */
/*  TUP_SRREF  > */
/* ************> */
void Dblqh::execTUP_SRREF(Signal* signal) 
{
  jamEntry();
  terrorCode = signal->theData[1];
  systemErrorLab(signal);
  return;
}//Dblqh::execTUP_SRREF()

/* ***************> */
/*  START_RECREQ  > */
/* ***************> */
void Dblqh::execSTART_RECREQ(Signal* signal) 
{
  CRASH_INSERTION(5027);

  jamEntry();
  StartRecReq * const req = (StartRecReq*)&signal->theData[0];
  cmasterDihBlockref = req->senderRef;

  crestartOldestGci = req->keepGci;
  crestartNewestGci = req->lastCompletedGci;
  cnewestGci = req->newestGci;

  ndbrequire(req->receivingNodeId == cownNodeid);

  cnewestCompletedGci = cnewestGci;
  cstartRecReq = ZTRUE;
  for (logPartPtr.i = 0; logPartPtr.i < 4; logPartPtr.i++) {
    ptrAss(logPartPtr, logPartRecord);
    logPartPtr.p->logPartNewestCompletedGCI = cnewestCompletedGci;
  }//for
  /* ------------------------------------------------------------------------
   *   WE HAVE TO SET THE OLDEST AND THE NEWEST GLOBAL CHECKPOINT IDENTITY 
   *   THAT WILL SURVIVE THIS SYSTEM RESTART. THIS IS NEEDED SO THAT WE CAN
   *   SET THE LOG HEAD AND LOG TAIL PROPERLY BEFORE STARTING THE SYSTEM AGAIN.
   *   WE ALSO NEED TO SET CNEWEST_GCI TO ENSURE THAT LOG RECORDS ARE EXECUTED
   *   WITH A PROPER GCI.
   *------------------------------------------------------------------------ */
  if (cstartType == NodeState::ST_NODE_RESTART) {
    jam();
    signal->theData[0] = ZSR_PHASE3_START;
    signal->theData[1] = ZSR_PHASE2_COMPLETED;
    sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
    return;
  }//if
  if(cstartType == NodeState::ST_INITIAL_NODE_RESTART){
    jam();
    StartRecConf * conf = (StartRecConf*)signal->getDataPtrSend();
    conf->startingNodeId = getOwnNodeId();
    sendSignal(cmasterDihBlockref, GSN_START_RECCONF, signal, 
	       StartRecConf::SignalLength, JBB);
    return;
  }//if
  if (cfirstWaitFragSr == RNIL) {
    /* ----------------------------------------------------------------------
     *      THERE ARE NO FRAGMENTS WAITING TO BE RESTARTED.
     * --------------------------------------------------------------------- */
    lcpPtr.i = 0;
    ptrAss(lcpPtr, lcpRecord);
    if (lcpPtr.p->lcpState == LcpRecord::LCP_IDLE) {
      jam();
      /* --------------------------------------------------------------------
       *    THERE ARE NO FRAGMENTS THAT ARE CURRENTLY PERFORMING THEIR 
       *    SYSTEM RESTART.
       * --------------------------------------------------------------------
       *    WE ARE NOW IN A POSITION TO ORDER TUP AND ACC TO START EXECUTING 
       *    THEIR UNDO LOGS. THIS MUST BE DONE BEFORE THE FRAGMENT LOGS 
       *    CAN BE EXECUTED.   
       * ------------------------------------------------------------------- */
      csrExecUndoLogState = EULS_STARTED;
      signal->theData[0] = caccBlockref;
      signal->theData[1] = cownref;
      sendSignal(caccBlockref, GSN_START_RECREQ, signal, 2, JBB);
      signal->theData[0] = ctupBlockref;
      signal->theData[1] = cownref;
      sendSignal(ctupBlockref, GSN_START_RECREQ, signal, 2, JBB);
    }//if
  }//if
  /* -----------------------------------------------------------------------
   *       EXIT AND WAIT FOR COMPLETION OF ALL FRAGMENTS.
   * ----------------------------------------------------------------------- */
  return;
}//Dblqh::execSTART_RECREQ()

/* ***************>> */
/*  START_RECCONF  > */
/* ***************>> */
void Dblqh::execSTART_RECCONF(Signal* signal) 
{
  jamEntry();
  BlockReference userRef = signal->theData[0];
  if (userRef == caccBlockref) {
    if (csrExecUndoLogState == EULS_STARTED) {
      jam();
      csrExecUndoLogState = EULS_ACC_COMPLETED;
    } else {
      ndbrequire(csrExecUndoLogState == EULS_TUP_COMPLETED);
      jam();
      csrExecUndoLogState = EULS_COMPLETED;
      /* --------------------------------------------------------------------
       *       START THE FIRST PHASE OF EXECUTION OF THE LOG.
       * ------------------------------------------------------------------- */
      startExecSr(signal);
    }//if
  } else {
    ndbrequire(userRef == ctupBlockref);
    if (csrExecUndoLogState == EULS_STARTED) {
      jam();
      csrExecUndoLogState = EULS_TUP_COMPLETED;
    } else {
      ndbrequire(csrExecUndoLogState == EULS_ACC_COMPLETED);
      jam();
      csrExecUndoLogState = EULS_COMPLETED;
      /* --------------------------------------------------------------------
       *       START THE FIRST PHASE OF EXECUTION OF THE LOG.
       * ------------------------------------------------------------------- */
      startExecSr(signal);
    }//if
  }//if
  return;
}//Dblqh::execSTART_RECCONF()

/* ***************> */
/*  START_RECREF  > */
/* ***************> */
void Dblqh::execSTART_RECREF(Signal* signal) 
{
  jamEntry();
  ndbrequire(false);
}//Dblqh::execSTART_RECREF()

/* ***************>> */
/*  START_EXEC_SR  > */
/* ***************>> */
void Dblqh::execSTART_EXEC_SR(Signal* signal) 
{
  FragrecordPtr prevFragptr;
  jamEntry();
  fragptr.i = signal->theData[0];
  prevFragptr.i = signal->theData[1];
  if (fragptr.i == RNIL) {
    jam();
    ndbrequire(cnoOfNodes < MAX_NDB_NODES);
    /* ----------------------------------------------------------------------
     *    NO MORE FRAGMENTS TO START EXECUTING THE LOG ON.
     *    SEND EXEC_SRREQ TO ALL LQH TO INDICATE THAT THIS NODE WILL 
     *    NOT REQUEST ANY MORE FRAGMENTS TO EXECUTE THE FRAGMENT LOG ON.
     * ---------------------------------------------------------------------- 
     *    WE NEED TO SEND THOSE SIGNALS EVEN IF WE HAVE NOT REQUESTED 
     *    ANY FRAGMENTS PARTICIPATE IN THIS PHASE.
     * --------------------------------------------------------------------- */
    for (Uint32 i = 0; i < cnoOfNodes; i++) {
      jam();
      if (cnodeStatus[i] == ZNODE_UP) {
        jam();
        ndbrequire(cnodeData[i] < MAX_NDB_NODES);
        BlockReference ref = calcLqhBlockRef(cnodeData[i]);
        signal->theData[0] = cownNodeid;
        sendSignal(ref, GSN_EXEC_SRREQ, signal, 1, JBB);
      }//if
    }//for
  } else {
    jam();
    ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
    if (fragptr.p->srNoLognodes > csrPhasesCompleted) {
      jam();
      Uint32 index = csrPhasesCompleted;
      arrGuard(index, 4);
      BlockReference ref = calcLqhBlockRef(fragptr.p->srLqhLognode[index]);
      fragptr.p->srStatus = Fragrecord::SS_STARTED;
      /* --------------------------------------------------------------------
       *  SINCE WE CAN HAVE SEVERAL LQH NODES PER FRAGMENT WE CALCULATE 
       *  THE LQH POINTER IN SUCH A WAY THAT WE CAN DEDUCE WHICH OF THE 
       *  LQH NODES THAT HAS RESPONDED WHEN EXEC_FRAGCONF IS RECEIVED.
       * ------------------------------------------------------------------- */
      ExecFragReq * const execFragReq = (ExecFragReq *)&signal->theData[0];
      execFragReq->userPtr = fragptr.i;
      execFragReq->userRef = cownref;
      execFragReq->tableId = fragptr.p->tabRef;
      execFragReq->fragId = fragptr.p->fragId;
      execFragReq->startGci = fragptr.p->srStartGci[index];
      execFragReq->lastGci = fragptr.p->srLastGci[index];
      sendSignal(ref, GSN_EXEC_FRAGREQ, signal, ExecFragReq::SignalLength, JBB);
      prevFragptr.i = fragptr.i;
      fragptr.i = fragptr.p->nextFrag;
    } else {
      jam();
      /* --------------------------------------------------------------------
       *  THIS FRAGMENT IS NOW FINISHED WITH THE SYSTEM RESTART. IT DOES 
       *  NOT NEED TO PARTICIPATE IN ANY MORE PHASES. REMOVE IT FROM THE 
       *  LIST OF COMPLETED FRAGMENTS TO EXECUTE THE LOG ON.
       *  ALSO SEND START_FRAGCONF TO DIH AND SET THE STATE TO ACTIVE ON THE
       *  FRAGMENT.
       * ------------------------------------------------------------------- */
      Uint32 next = fragptr.p->nextFrag;
      if (prevFragptr.i != RNIL) {
        jam();
        ptrCheckGuard(prevFragptr, cfragrecFileSize, fragrecord);
        prevFragptr.p->nextFrag = next;
      } else {
        jam();
        cfirstCompletedFragSr = next;
      }//if

      /**
       * Put fragment on list which has completed REDO log
       */
      fragptr.p->nextFrag = c_redo_log_complete_frags;
      c_redo_log_complete_frags = fragptr.i;
      
      fragptr.p->fragStatus = Fragrecord::FSACTIVE;
      fragptr.p->logFlag = Fragrecord::STATE_TRUE;
      signal->theData[0] = fragptr.p->srUserptr;
      signal->theData[1] = cownNodeid;
      sendSignal(fragptr.p->srBlockref, GSN_START_FRAGCONF, signal, 2, JBB);
      /* --------------------------------------------------------------------
       *  WE HAVE TO ENSURE THAT THIS FRAGMENT IS NOT PUT BACK ON THE LIST BY
       *  MISTAKE. WE DO THIS BY ALSO REMOVING IT AS PREVIOUS IN START_EXEC_SR
       *  THIS IS PERFORMED BY KEEPING PREV_FRAGPTR AS PREV_FRAGPTR BUT MOVING
       *  FRAGPTR TO THE NEXT FRAGMENT IN THE LIST.
       * ------------------------------------------------------------------- */
      fragptr.i = next;
    }//if
    signal->theData[0] = fragptr.i;
    signal->theData[1] = prevFragptr.i;
    sendSignal(cownref, GSN_START_EXEC_SR, signal, 2, JBB);
  }//if
  return;
}//Dblqh::execSTART_EXEC_SR()

/* ***************> */
/*  EXEC_FRAGREQ  > */
/* ***************> */
/* --------------------------------------------------------------------------
 *  THIS SIGNAL IS USED TO REQUEST THAT A FRAGMENT PARTICIPATES IN EXECUTING
 *  THE LOG IN THIS NODE.
 * ------------------------------------------------------------------------- */
void Dblqh::execEXEC_FRAGREQ(Signal* signal) 
{
  ExecFragReq * const execFragReq = (ExecFragReq *)&signal->theData[0];
  jamEntry();
  tabptr.i = execFragReq->tableId;
  Uint32 fragId = execFragReq->fragId;
  ptrCheckGuard(tabptr, ctabrecFileSize, tablerec);
  if (!getFragmentrec(signal, fragId)) {
    jam();
    if (!insertFragrec(signal, fragId)) {
      jam();
      sendExecFragRefLab(signal);
      return;
    }//if    
    initFragrec(signal, tabptr.i, fragId, ZLOG_NODE);
    fragptr.p->execSrStatus = Fragrecord::ACTIVE_REMOVE_AFTER;
  } else {
    jam();
    if (fragptr.p->execSrStatus == Fragrecord::ACTIVE_REMOVE_AFTER) {
      jam();
      fragptr.p->execSrStatus = Fragrecord::ACTIVE_REMOVE_AFTER;
    } else {
      jam();
    }//if
  }//if
  ndbrequire(fragptr.p->execSrNoReplicas < 4);
  fragptr.p->execSrBlockref[fragptr.p->execSrNoReplicas] = execFragReq->userRef;
  fragptr.p->execSrUserptr[fragptr.p->execSrNoReplicas] = execFragReq->userPtr;
  fragptr.p->execSrStartGci[fragptr.p->execSrNoReplicas] = execFragReq->startGci;
  fragptr.p->execSrLastGci[fragptr.p->execSrNoReplicas] = execFragReq->lastGci;
  fragptr.p->execSrStatus = Fragrecord::ACTIVE;
  fragptr.p->execSrNoReplicas++;
  cnoFragmentsExecSr++;
  return;
}//Dblqh::execEXEC_FRAGREQ()

void Dblqh::sendExecFragRefLab(Signal* signal) 
{
  ExecFragReq * const execFragReq = (ExecFragReq *)&signal->theData[0];
  BlockReference retRef = execFragReq->userRef;
  Uint32 retPtr = execFragReq->userPtr;

  signal->theData[0] = retPtr;
  signal->theData[1] = terrorCode;
  sendSignal(retRef, GSN_EXEC_FRAGREF, signal, 2, JBB);
  return;
}//Dblqh::sendExecFragRefLab()

/* ***************>> */
/*  EXEC_FRAGCONF  > */
/* ***************>> */
void Dblqh::execEXEC_FRAGCONF(Signal* signal) 
{
  jamEntry();
  fragptr.i = signal->theData[0];
  ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
  fragptr.p->srStatus = Fragrecord::SS_COMPLETED;
  return;
}//Dblqh::execEXEC_FRAGCONF()

/* ***************> */
/*  EXEC_FRAGREF  > */
/* ***************> */
void Dblqh::execEXEC_FRAGREF(Signal* signal) 
{
  jamEntry();
  terrorCode = signal->theData[1];
  systemErrorLab(signal);
  return;
}//Dblqh::execEXEC_FRAGREF()

/* *************** */
/*  EXEC_SRCONF  > */
/* *************** */
void Dblqh::execEXEC_SRCONF(Signal* signal) 
{
  jamEntry();
  Uint32 nodeId = signal->theData[0];
  arrGuard(nodeId, MAX_NDB_NODES);
  cnodeExecSrState[nodeId] = ZEXEC_SR_COMPLETED;
  ndbrequire(cnoOfNodes < MAX_NDB_NODES);
  for (Uint32 i = 0; i < cnoOfNodes; i++) {
    jam();
    if (cnodeStatus[i] == ZNODE_UP) {
      jam();
      nodeId = cnodeData[i];
      arrGuard(nodeId, MAX_NDB_NODES);
      if (cnodeExecSrState[nodeId] != ZEXEC_SR_COMPLETED) {
        jam();
	/* ------------------------------------------------------------------
	 *  ALL NODES HAVE NOT REPORTED COMPLETION OF EXECUTING FRAGMENT 
	 *  LOGS YET.
	 * ----------------------------------------------------------------- */
        return;
      }//if
    }//if
  }//for
  /* ------------------------------------------------------------------------
   *  CLEAR NODE SYSTEM RESTART EXECUTION STATE TO PREPARE FOR NEXT PHASE OF
   *  LOG EXECUTION.
   * ----------------------------------------------------------------------- */
  for (nodeId = 0; nodeId < MAX_NDB_NODES; nodeId++) {
    cnodeExecSrState[nodeId] = ZSTART_SR;
  }//for
  /* ------------------------------------------------------------------------
   *  NOW CHECK IF ALL FRAGMENTS IN THIS PHASE HAVE COMPLETED. IF SO START THE
   *  NEXT PHASE.
   * ----------------------------------------------------------------------- */
  fragptr.i = cfirstCompletedFragSr;
  if (fragptr.i == RNIL) {
    jam();
    execSrCompletedLab(signal);
    return;
  }//if
  do {
    jam();
    ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
    ndbrequire(fragptr.p->srStatus == Fragrecord::SS_COMPLETED);
    fragptr.i = fragptr.p->nextFrag;
  } while (fragptr.i != RNIL);
  execSrCompletedLab(signal);
  return;
}//Dblqh::execEXEC_SRCONF()

void Dblqh::execSrCompletedLab(Signal* signal) 
{
  csrPhasesCompleted++;
  /* ------------------------------------------------------------------------
   *  ALL FRAGMENTS WERE COMPLETED. THIS PHASE IS COMPLETED. IT IS NOW TIME TO
   *  START THE NEXT PHASE.
   * ----------------------------------------------------------------------- */
  if (csrPhasesCompleted >= 4) {
    jam();
    /* ----------------------------------------------------------------------
     *  THIS WAS THE LAST PHASE. WE HAVE NOW COMPLETED THE EXECUTION THE 
     *  FRAGMENT LOGS IN ALL NODES. BEFORE WE SEND START_RECCONF TO THE 
     *  MASTER DIH TO INDICATE A COMPLETED SYSTEM RESTART IT IS NECESSARY 
     *  TO FIND THE HEAD AND THE TAIL OF THE LOG WHEN NEW OPERATIONS START 
     *  TO COME AGAIN.
     * 
     * THE FIRST STEP IS TO FIND THE HEAD AND TAIL MBYTE OF EACH LOG PART.
     * TO DO THIS WE REUSE THE CONTINUEB SIGNAL SR_LOG_LIMITS. THEN WE 
     * HAVE TO FIND THE ACTUAL PAGE NUMBER AND PAGE INDEX WHERE TO 
     * CONTINUE WRITING THE LOG AFTER THE SYSTEM RESTART.
     * --------------------------------------------------------------------- */
    for (logPartPtr.i = 0; logPartPtr.i < 4; logPartPtr.i++) {
      jam();
      ptrAss(logPartPtr, logPartRecord);
      logPartPtr.p->logPartState = LogPartRecord::SR_FOURTH_PHASE_STARTED;
      logPartPtr.p->logLastGci = crestartNewestGci;
      logPartPtr.p->logStartGci = crestartOldestGci;
      logPartPtr.p->logExecState = LogPartRecord::LES_SEARCH_STOP;
      if (logPartPtr.p->headFileNo == ZNIL) {
        jam();
	/* -----------------------------------------------------------------
	 *  IF WE HAVEN'T FOUND ANY HEAD OF THE LOG THEN WE ARE IN SERIOUS 
	 *  PROBLEM.  THIS SHOULD NOT OCCUR. IF IT OCCURS ANYWAY THEN WE 
	 *  HAVE TO FIND A CURE FOR THIS PROBLEM.
	 * ----------------------------------------------------------------- */
        systemErrorLab(signal);
        return;
      }//if
      signal->theData[0] = ZSR_LOG_LIMITS;
      signal->theData[1] = logPartPtr.i;
      signal->theData[2] = logPartPtr.p->lastLogfile;
      signal->theData[3] = logPartPtr.p->lastMbyte;
      sendSignal(cownref, GSN_CONTINUEB, signal, 4, JBB);
    }//for
    return;
  } else {
    jam();
    /* ----------------------------------------------------------------------
     *   THERE ARE YET MORE PHASES TO RESTART.
     *   WE MUST INITIALISE DATA FOR NEXT PHASE AND SEND START SIGNAL.
     * --------------------------------------------------------------------- */
    startExecSr(signal);
  }//if
  return;
}//Dblqh::execSrCompletedLab()

/* ************>> */
/*  EXEC_SRREQ  > */
/* ************>> */
void Dblqh::execEXEC_SRREQ(Signal* signal) 
{
  jamEntry();
  Uint32 nodeId = signal->theData[0];
  ndbrequire(nodeId < MAX_NDB_NODES);
  cnodeSrState[nodeId] = ZEXEC_SR_COMPLETED;
  ndbrequire(cnoOfNodes < MAX_NDB_NODES);
  for (Uint32 i = 0; i < cnoOfNodes; i++) {
    jam();
    if (cnodeStatus[i] == ZNODE_UP) {
      jam();
      nodeId = cnodeData[i];
      if (cnodeSrState[nodeId] != ZEXEC_SR_COMPLETED) {
        jam();
	/* ------------------------------------------------------------------
	 *  ALL NODES HAVE NOT REPORTED COMPLETION OF SENDING EXEC_FRAGREQ YET.
	 * ----------------------------------------------------------------- */
        return;
      }//if
    }//if
  }//for
  /* ------------------------------------------------------------------------
   *  CLEAR NODE SYSTEM RESTART STATE TO PREPARE FOR NEXT PHASE OF LOG 
   *  EXECUTION
   * ----------------------------------------------------------------------- */
  for (nodeId = 0; nodeId < MAX_NDB_NODES; nodeId++) {
    cnodeSrState[nodeId] = ZSTART_SR;
  }//for
  if (csrPhasesCompleted != 0) {
    /* ----------------------------------------------------------------------
     *       THE FIRST PHASE MUST ALWAYS EXECUTE THE LOG.
     * --------------------------------------------------------------------- */
    if (cnoFragmentsExecSr == 0) {
      jam();
      /* --------------------------------------------------------------------
       *   THERE WERE NO FRAGMENTS THAT NEEDED TO EXECUTE THE LOG IN THIS PHASE.
       * ------------------------------------------------------------------- */
      srPhase3Comp(signal);
      return;
    }//if
  }//if
  /* ------------------------------------------------------------------------
   *  NOW ALL NODES HAVE SENT ALL EXEC_FRAGREQ. NOW WE CAN START EXECUTING THE
   *  LOG FROM THE MINIMUM GCI NEEDED UNTIL THE MAXIMUM GCI NEEDED.
   *
   *  WE MUST FIRST CHECK IF THE FIRST PHASE OF THE SYSTEM RESTART HAS BEEN
   *  COMPLETED. THIS HANDLING IS PERFORMED IN THE FILE SYSTEM MODULE
   * ----------------------------------------------------------------------- */
  signal->theData[0] = ZSR_PHASE3_START;
  signal->theData[1] = ZSR_PHASE2_COMPLETED;
  sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
  return;
}//Dblqh::execEXEC_SRREQ()

/* ######################################################################### */
/*       SYSTEM RESTART PHASE THREE MODULE                                   */
/*       THIS MODULE IS A SUB-MODULE OF THE FILE SYSTEM HANDLING.            */
/*                                                                           */
/* THIS MODULE IS CONCERNED WITH EXECUTING THE FRAGMENT LOG. IT DOES ALSO    */
/* CONTAIN SIGNAL RECEPTIONS LQHKEYCONF AND LQHKEYREF SINCE LQHKEYREQ IS USED*/
/* TO EXECUTE THE LOG RECORDS.                                               */
/*                                                                           */
/* BEFORE IT STARTS IT HAS BEEN DECIDED WHERE TO START AND WHERE TO STOP     */
/* READING THE FRAGMENT LOG BY USING THE INFORMATION ABOUT GCI DISCOVERED IN */
/* PHASE ONE OF THE SYSTEM RESTART.                                          */
/* ######################################################################### */
/*---------------------------------------------------------------------------*/
/* PHASE THREE OF THE SYSTEM RESTART CAN NOW START. ONE OF THE PHASES HAVE   */
/* COMPLETED.                                                                */
/*---------------------------------------------------------------------------*/
void Dblqh::srPhase3Start(Signal* signal) 
{
  UintR tsrPhaseStarted;
  
  jamEntry();
  tsrPhaseStarted = signal->theData[0];
  if (csrPhaseStarted == ZSR_NO_PHASE_STARTED) {
    jam();
    csrPhaseStarted = tsrPhaseStarted;
    if (cstartType == NodeState::ST_NODE_RESTART) {
      ndbrequire(cinitialStartOngoing == ZTRUE);
      cinitialStartOngoing = ZFALSE;
      checkStartCompletedLab(signal);
    }//if
    return;
  }//if  
  ndbrequire(csrPhaseStarted != tsrPhaseStarted);
  ndbrequire(csrPhaseStarted != ZSR_BOTH_PHASES_STARTED);

  csrPhaseStarted = ZSR_BOTH_PHASES_STARTED;
  for (logPartPtr.i = 0; logPartPtr.i < 4; logPartPtr.i++) {
    jam();
    ptrAss(logPartPtr, logPartRecord);
    logPartPtr.p->logPartState = LogPartRecord::SR_THIRD_PHASE_STARTED;
    logPartPtr.p->logStartGci = (UintR)-1;
    if (csrPhasesCompleted == 0) {
      jam();
      /* -------------------------------------------------------------------- 
       *  THE FIRST PHASE WE MUST ENSURE THAT IT REACHES THE END OF THE LOG.
       * ------------------------------------------------------------------- */
      logPartPtr.p->logLastGci = crestartNewestGci;
    } else {
      jam();
      logPartPtr.p->logLastGci = 2;
    }//if
  }//for
  if (cstartType == NodeState::ST_NODE_RESTART) {
    jam();
    /* ----------------------------------------------------------------------
     *  FOR A NODE RESTART WE HAVE NO FRAGMENTS DEFINED YET. 
     *  THUS WE CAN SKIP THAT PART
     * --------------------------------------------------------------------- */
    signal->theData[0] = ZSR_GCI_LIMITS;
    signal->theData[1] = RNIL;
    sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
  } else {
    jam();
    signal->theData[0] = ZSR_GCI_LIMITS;
    signal->theData[1] = 0;
    sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
  }//if
  return;
}//Dblqh::srPhase3Start()

/* --------------------------------------------------------------------------
 *   WE NOW WE NEED TO FIND THE LIMITS WITHIN WHICH TO EXECUTE 
 *   THE FRAGMENT LOG
 * ------------------------------------------------------------------------- */
void Dblqh::srGciLimits(Signal* signal) 
{
  LogPartRecordPtr tmpLogPartPtr;

  jamEntry();
  fragptr.i = signal->theData[0];
  Uint32 loopCount = 0;
  logPartPtr.i = 0;
  ptrAss(logPartPtr, logPartRecord);
  while (fragptr.i < cfragrecFileSize) {
    jam();
    ptrAss(fragptr, fragrecord);
    if (fragptr.p->execSrStatus != Fragrecord::IDLE) {
      jam();
      ndbrequire(fragptr.p->execSrNoReplicas - 1 < 4);
      for (Uint32 i = 0; i < fragptr.p->execSrNoReplicas; i++) {
        jam();
        if (fragptr.p->execSrStartGci[i] < logPartPtr.p->logStartGci) {
          jam();
          logPartPtr.p->logStartGci = fragptr.p->execSrStartGci[i];
        }//if
        if (fragptr.p->execSrLastGci[i] > logPartPtr.p->logLastGci) {
          jam();
          logPartPtr.p->logLastGci = fragptr.p->execSrLastGci[i];
        }//if
      }//for
    }//if
    loopCount++;
    if (loopCount > 20) {
      jam();
      signal->theData[0] = ZSR_GCI_LIMITS;
      signal->theData[1] = fragptr.i + 1;
      sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
      return;
    } else {
      jam();
      fragptr.i++;
    }//if
  }//while
  if (logPartPtr.p->logStartGci == (UintR)-1) {
    jam();
      /* --------------------------------------------------------------------
       *  THERE WERE NO FRAGMENTS TO INSTALL WE WILL EXECUTE THE LOG AS 
       *  SHORT AS POSSIBLE TO REACH THE END OF THE LOG. THIS WE DO BY 
       *  STARTING AT THE STOP GCI.
       * ------------------------------------------------------------------- */
    logPartPtr.p->logStartGci = logPartPtr.p->logLastGci;
  }//if
  for (tmpLogPartPtr.i = 1; tmpLogPartPtr.i < 4; tmpLogPartPtr.i++) {
    ptrAss(tmpLogPartPtr, logPartRecord);
    tmpLogPartPtr.p->logStartGci = logPartPtr.p->logStartGci;
    tmpLogPartPtr.p->logLastGci = logPartPtr.p->logLastGci;
  }//for
  for (logPartPtr.i = 0; logPartPtr.i < 4; logPartPtr.i++) {
    jam();
    ptrAss(logPartPtr, logPartRecord);
    logPartPtr.p->logExecState = LogPartRecord::LES_SEARCH_STOP;
    signal->theData[0] = ZSR_LOG_LIMITS;
    signal->theData[1] = logPartPtr.i;
    signal->theData[2] = logPartPtr.p->lastLogfile;
    signal->theData[3] = logPartPtr.p->lastMbyte;
    sendSignal(cownref, GSN_CONTINUEB, signal, 4, JBB);
  }//for
}//Dblqh::srGciLimits()

/* --------------------------------------------------------------------------
 *       IT IS NOW TIME TO FIND WHERE TO START EXECUTING THE LOG.
 *       THIS SIGNAL IS SENT FOR EACH LOG PART AND STARTS THE EXECUTION 
 *       OF THE LOG FOR THIS PART.
 *-------------------------------------------------------------------------- */
void Dblqh::srLogLimits(Signal* signal) 
{
  Uint32 tlastPrepRef;
  Uint32 tmbyte;

  jamEntry();
  logPartPtr.i = signal->theData[0];
  ptrCheckGuard(logPartPtr, clogPartFileSize, logPartRecord);
  logFilePtr.i = signal->theData[1];
  ptrCheckGuard(logFilePtr, clogFileFileSize, logFileRecord);
  tmbyte = signal->theData[2];
  Uint32 loopCount = 0;
  /* ------------------------------------------------------------------------
   *   WE ARE SEARCHING FOR THE START AND STOP MBYTE OF THE LOG THAT IS TO BE
   *   EXECUTED.
   * ----------------------------------------------------------------------- */
  while(true) {
    ndbrequire(tmbyte < 16);
    if (logPartPtr.p->logExecState == LogPartRecord::LES_SEARCH_STOP) {
      if (logFilePtr.p->logMaxGciCompleted[tmbyte] < logPartPtr.p->logLastGci) {
        jam();
      /* --------------------------------------------------------------------
       *  WE ARE STEPPING BACKWARDS FROM MBYTE TO MBYTE. THIS IS THE FIRST 
       *  MBYTE WHICH IS TO BE INCLUDED IN THE LOG EXECUTION. THE STOP GCI 
       *  HAS NOT BEEN COMPLETED BEFORE THIS MBYTE. THUS THIS MBYTE HAVE 
       *  TO BE EXECUTED.
       * ------------------------------------------------------------------- */
        logPartPtr.p->stopLogfile = logFilePtr.i;
        logPartPtr.p->stopMbyte = tmbyte;
        logPartPtr.p->logExecState = LogPartRecord::LES_SEARCH_START;
      }//if
    }//if
  /* ------------------------------------------------------------------------
   *  WHEN WE HAVEN'T FOUND THE STOP MBYTE IT IS NOT NECESSARY TO LOOK FOR THE
   *  START MBYTE. THE REASON IS THE FOLLOWING LOGIC CHAIN: 
   *    MAX_GCI_STARTED >= MAX_GCI_COMPLETED >= LAST_GCI >= START_GCI
   *  THUS MAX_GCI_STARTED >= START_GCI. THUS MAX_GCI_STARTED < START_GCI CAN
   *  NOT BE TRUE AS WE WILL CHECK OTHERWISE.
   * ----------------------------------------------------------------------- */
    if (logPartPtr.p->logExecState == LogPartRecord::LES_SEARCH_START) {
      if (logFilePtr.p->logMaxGciStarted[tmbyte] < logPartPtr.p->logStartGci) {
        jam();
      /* --------------------------------------------------------------------
       *  WE HAVE NOW FOUND THE START OF THE EXECUTION OF THE LOG. 
       *  WE STILL HAVE TO MOVE IT BACKWARDS TO ALSO INCLUDE THE 
       *  PREPARE RECORDS WHICH WERE STARTED IN A PREVIOUS MBYTE.
       * ------------------------------------------------------------------- */
        tlastPrepRef = logFilePtr.p->logLastPrepRef[tmbyte];
        logPartPtr.p->startMbyte = tlastPrepRef & 65535;
        LogFileRecordPtr locLogFilePtr;
        findLogfile(signal, tlastPrepRef >> 16, logPartPtr, &locLogFilePtr);
        logPartPtr.p->startLogfile = locLogFilePtr.i;
        logPartPtr.p->logExecState = LogPartRecord::LES_EXEC_LOG;
      }//if
    }//if
    if (logPartPtr.p->logExecState != LogPartRecord::LES_EXEC_LOG) {
      if (tmbyte == 0) {
        jam();
        tmbyte = ZNO_MBYTES_IN_FILE - 1;
        logFilePtr.i = logFilePtr.p->prevLogFile;
        ptrCheckGuard(logFilePtr, clogFileFileSize, logFileRecord);
      } else {
        jam();
        tmbyte--;
      }//if
      if (logPartPtr.p->lastLogfile == logFilePtr.i) {
        ndbrequire(logPartPtr.p->lastMbyte != tmbyte);
      }//if
      if (loopCount > 20) {
        jam();
        signal->theData[0] = ZSR_LOG_LIMITS;
        signal->theData[1] = logPartPtr.i;
        signal->theData[2] = logFilePtr.i;
        signal->theData[3] = tmbyte;
        sendSignal(cownref, GSN_CONTINUEB, signal, 4, JBB);
        return;
      }//if
      loopCount++;
    } else {
      jam();
      break;
    }//if
  }//while
  /* ------------------------------------------------------------------------
   *  WE HAVE NOW FOUND BOTH THE START AND THE STOP OF THE LOG. NOW START
   *  EXECUTING THE LOG. THE FIRST ACTION IS TO OPEN THE LOG FILE WHERE TO
   *  START EXECUTING THE LOG.
   * ----------------------------------------------------------------------- */
  if (logPartPtr.p->logPartState == LogPartRecord::SR_THIRD_PHASE_STARTED) {
    jam();
    logFilePtr.i = logPartPtr.p->startLogfile;
    ptrCheckGuard(logFilePtr, clogFileFileSize, logFileRecord);
    logFilePtr.p->logFileStatus = LogFileRecord::OPEN_EXEC_SR_START;
    openFileRw(signal, logFilePtr);
  } else {
    jam();
    ndbrequire(logPartPtr.p->logPartState == LogPartRecord::SR_FOURTH_PHASE_STARTED);
      /* --------------------------------------------------------------------
       *  WE HAVE NOW FOUND THE TAIL MBYTE IN THE TAIL FILE. 
       *  SET THOSE PARAMETERS IN THE LOG PART. 
       *  WE HAVE ALSO FOUND THE HEAD MBYTE. WE STILL HAVE TO SEARCH  
       *  FOR THE PAGE NUMBER AND PAGE INDEX WHERE TO SET THE HEAD.
       * ------------------------------------------------------------------- */
    logFilePtr.i = logPartPtr.p->startLogfile;
    ptrCheckGuard(logFilePtr, clogFileFileSize, logFileRecord);
    logPartPtr.p->logTailFileNo = logFilePtr.p->fileNo;
    logPartPtr.p->logTailMbyte = logPartPtr.p->startMbyte;
      /* --------------------------------------------------------------------
       *  THE HEAD WE ACTUALLY FOUND DURING EXECUTION OF LOG SO WE USE 
       *  THIS INFO HERE RATHER THAN THE MBYTE WE FOUND TO BE THE HEADER.
       * ------------------------------------------------------------------- */
    LogFileRecordPtr locLogFilePtr;
    findLogfile(signal, logPartPtr.p->headFileNo, logPartPtr, &locLogFilePtr);
    locLogFilePtr.p->logFileStatus = LogFileRecord::OPEN_SR_FOURTH_PHASE;
    openFileRw(signal, locLogFilePtr);
  }//if
  return;
}//Dblqh::srLogLimits()

void Dblqh::openExecSrStartLab(Signal* signal) 
{
  logPartPtr.p->currentLogfile = logFilePtr.i;
  logFilePtr.p->currentMbyte = logPartPtr.p->startMbyte;
  /* ------------------------------------------------------------------------
   *     WE NEED A TC CONNECT RECORD TO HANDLE EXECUTION OF LOG RECORDS.
   * ------------------------------------------------------------------------ */
  seizeTcrec();
  logPartPtr.p->logTcConrec = tcConnectptr.i;
  /* ------------------------------------------------------------------------
   *   THE FIRST LOG RECORD TO EXECUTE IS ALWAYS AT A NEW MBYTE.
   *   SET THE NUMBER OF PAGES IN THE MAIN MEMORY BUFFER TO ZERO AS AN INITIAL
   *   VALUE. THIS VALUE WILL BE UPDATED AND ENSURED THAT IT RELEASES PAGES IN
   *   THE SUBROUTINE READ_EXEC_SR.
   * ----------------------------------------------------------------------- */
  logPartPtr.p->mmBufferSize = 0;
  readExecSrNewMbyte(signal);
  return;
}//Dblqh::openExecSrStartLab()

/* ---------------------------------------------------------------------------
 *  WE WILL ALWAYS ENSURE THAT WE HAVE AT LEAST 16 KBYTE OF LOG PAGES WHEN WE
 *  START READING A LOG RECORD. THE ONLY EXCEPTION IS WHEN WE COME CLOSE TO A 
 *  MBYTE BOUNDARY. SINCE WE KNOW THAT LOG RECORDS ARE NEVER WRITTEN ACROSS A 
 *  MBYTE BOUNDARY THIS IS NOT A PROBLEM.
 *
 *  WE START BY READING 64 KBYTE BEFORE STARTING TO EXECUTE THE LOG RECORDS.
 *  WHEN WE COME BELOW 64 KBYTE WE READ ANOTHER SET OF LOG PAGES. WHEN WE 
 *  GO BELOW 16 KBYTE WE WAIT UNTIL THE READ PAGES HAVE ENTERED THE BLOCK.
 * ------------------------------------------------------------------------- */
/* --------------------------------------------------------------------------
 *       NEW PAGES FROM LOG FILE DURING EXECUTION OF LOG HAS ARRIVED.
 * ------------------------------------------------------------------------- */
void Dblqh::readExecSrLab(Signal* signal) 
{
  buildLinkedLogPageList(signal);
  /* ------------------------------------------------------------------------
   *   WE NEED TO SET THE CURRENT PAGE INDEX OF THE FIRST PAGE SINCE IT CAN BE 
   *   USED IMMEDIATELY WITHOUT ANY OTHER INITIALISATION. THE REST OF THE PAGES
   *   WILL BE INITIALISED BY READ_LOGWORD.
   * ----------------------------------------------------------------------- */
  logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] = ZPAGE_HEADER_SIZE;
  if (logPartPtr.p->logExecState == 
      LogPartRecord::LES_WAIT_READ_EXEC_SR_NEW_MBYTE) {
    jam();
    /* ----------------------------------------------------------------------
     *  THIS IS THE FIRST READ DURING THE EXECUTION OF THIS MBYTE. SET THE 
     *  NEW CURRENT LOG PAGE TO THE FIRST OF THESE PAGES. CHANGE 
     *  LOG_EXEC_STATE TO ENSURE THAT WE START EXECUTION OF THE LOG.
     * --------------------------------------------------------------------- */
    logFilePtr.p->currentFilepage = logFilePtr.p->currentMbyte * 
                                    ZPAGES_IN_MBYTE;
    logPartPtr.p->prevFilepage = logFilePtr.p->currentFilepage;
    logFilePtr.p->currentLogpage = lfoPtr.p->firstLfoPage;
    logPartPtr.p->prevLogpage = logFilePtr.p->currentLogpage;
  }//if
  moveToPageRef(signal);
  releaseLfo(signal);
  /* ------------------------------------------------------------------------
   *  NOW WE HAVE COMPLETED THE RECEPTION OF THESE PAGES. 
   *  NOW CHECK IF WE NEED TO READ MORE PAGES.
   * ----------------------------------------------------------------------- */
  checkReadExecSr(signal);
  if (logPartPtr.p->logExecState == LogPartRecord::LES_EXEC_LOG) {
    jam();
    signal->theData[0] = ZEXEC_SR;
    signal->theData[1] = logPartPtr.i;
    sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
    return;
  }//if
  return;
}//Dblqh::readExecSrLab()

void Dblqh::openExecSrNewMbyteLab(Signal* signal) 
{
  readExecSrNewMbyte(signal);
  return;
}//Dblqh::openExecSrNewMbyteLab()

void Dblqh::closeExecSrLab(Signal* signal) 
{
  LogFileRecordPtr locLogFilePtr;
  logFilePtr.p->logFileStatus = LogFileRecord::CLOSED;
  logPartPtr.i = logFilePtr.p->logPartRec;
  ptrCheckGuard(logPartPtr, clogPartFileSize, logPartRecord);
  locLogFilePtr.i = logPartPtr.p->currentLogfile;
  ptrCheckGuard(locLogFilePtr, clogFileFileSize, logFileRecord);
  locLogFilePtr.p->logFileStatus = LogFileRecord::OPEN_EXEC_SR_NEW_MBYTE;
  openFileRw(signal, locLogFilePtr);
  return;
}//Dblqh::closeExecSrLab()

void Dblqh::writeDirtyLab(Signal* signal) 
{
  releaseLfo(signal);
  signal->theData[0] = logPartPtr.i;
  execSr(signal);
  return;
}//Dblqh::writeDirtyLab()

/* --------------------------------------------------------------------------
 *       EXECUTE A LOG RECORD WITHIN THE CURRENT MBYTE.
 * ------------------------------------------------------------------------- */
void Dblqh::execSr(Signal* signal) 
{
  LogFileRecordPtr nextLogFilePtr;
  LogPageRecordPtr tmpLogPagePtr;
  Uint32 logWord;

  jamEntry();
  logPartPtr.i = signal->theData[0];
  ptrCheckGuard(logPartPtr, clogPartFileSize, logPartRecord);

  do {
    jam();
    logFilePtr.i = logPartPtr.p->currentLogfile;
    ptrCheckGuard(logFilePtr, clogFileFileSize, logFileRecord);
    logPagePtr.i = logPartPtr.p->prevLogpage;
    ptrCheckGuard(logPagePtr, clogPageFileSize, logPageRecord);
    if (logPagePtr.p->logPageWord[ZPOS_DIRTY] == ZDIRTY) {
      jam();
      switch (logPartPtr.p->logExecState) {
      case LogPartRecord::LES_EXEC_LOG_COMPLETED:
      case LogPartRecord::LES_EXEC_LOG_NEW_FILE:
      case LogPartRecord::LES_EXEC_LOG_NEW_MBYTE:
        jam();
	/* ------------------------------------------------------------------
	 *  IN THIS WE HAVE COMPLETED EXECUTION OF THE CURRENT LOG PAGE
	 *  AND CAN WRITE IT TO DISK SINCE IT IS DIRTY.
	 * ----------------------------------------------------------------- */
        writeDirty(signal);
        return;
        break;
      case LogPartRecord::LES_EXEC_LOG:
      jam();
      /* --------------------------------------------------------------------
       *  IN THIS CASE WE ONLY WRITE THE PAGE TO DISK IF WE HAVE COMPLETED 
       *  EXECUTION OF LOG RECORDS BELONGING TO THIS LOG PAGE.
       * ------------------------------------------------------------------- */
        if (logFilePtr.p->currentLogpage != logPartPtr.p->prevLogpage) {
          jam();
          writeDirty(signal);
          return;
        }//if
        break;
      default:
        ndbrequire(false);
        break;
      }//switch
    }//if
    if (logFilePtr.p->currentLogpage != logPartPtr.p->prevLogpage) {
      jam();
      logPartPtr.p->prevLogpage = logPagePtr.p->logPageWord[ZNEXT_PAGE];
      logPartPtr.p->prevFilepage++;
      continue;
    }//if
    switch (logPartPtr.p->logExecState) {
    case LogPartRecord::LES_EXEC_LOG_COMPLETED:
      jam();
      releaseMmPages(signal);
      logFilePtr.p->logFileStatus = LogFileRecord::CLOSING_EXEC_SR_COMPLETED;
      closeFile(signal, logFilePtr);
      return;
      break;
    case LogPartRecord::LES_EXEC_LOG_NEW_MBYTE:
      jam();
      logFilePtr.p->currentMbyte++;
      readExecSrNewMbyte(signal);
      return;
      break;
    case LogPartRecord::LES_EXEC_LOG_NEW_FILE:
      jam();
      nextLogFilePtr.i = logFilePtr.p->nextLogFile;
      logPartPtr.p->currentLogfile = nextLogFilePtr.i;
      ptrCheckGuard(nextLogFilePtr, clogFileFileSize, logFileRecord);
      nextLogFilePtr.p->currentMbyte = 0;
      logFilePtr.p->logFileStatus = LogFileRecord::CLOSING_EXEC_SR;
      closeFile(signal, logFilePtr);
      return;
      break;
    case LogPartRecord::LES_EXEC_LOG:
      jam();
      /*empty*/;
      break;
    default:
      jam();
      systemErrorLab(signal);
      return;
      break;
    }//switch
    logPagePtr.i = logFilePtr.p->currentLogpage;
    ptrCheckGuard(logPagePtr, clogPageFileSize, logPageRecord);
    logPartPtr.p->savePageIndex = logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX];
    if (logPartPtr.p->execSrPagesRead < ZMIN_READ_BUFFER_SIZE) {
      /* --------------------------------------------------------------------
       *  THERE WERE LESS THAN 16 KBYTE OF LOG PAGES REMAINING. WE WAIT UNTIL
       *  THE NEXT 64 KBYTE ARRIVES UNTIL WE CONTINUE AGAIN.
       * ------------------------------------------------------------------- */
      if ((logPartPtr.p->execSrPagesRead + 
	   logPartPtr.p->execSrPagesExecuted) < ZPAGES_IN_MBYTE) {
        jam();
	/* ------------------------------------------------------------------
	 *  WE ONLY STOP AND WAIT IF THERE MORE PAGES TO READ. IF IT IS NOT 
	 *  THEN IT IS THE END OF THE MBYTE AND WE WILL CONTINUE. IT IS NO 
	 *  RISK THAT A LOG RECORD WE FIND WILL NOT BE READ AT THIS TIME 
	 *  SINCE THE LOG RECORDS NEVER SPAN OVER A MBYTE BOUNDARY.
	 * ----------------------------------------------------------------- */
        readExecSr(signal);
        logPartPtr.p->logExecState = LogPartRecord::LES_WAIT_READ_EXEC_SR;
        return;
      }//if
    }//if
    logWord = readLogword(signal);
    switch (logWord) {
/* ========================================================================= */
/* ========================================================================= */
    case ZPREP_OP_TYPE:
    {
      logWord = readLogword(signal);
      stepAhead(signal, logWord - 2);
      break;
    }
/* ========================================================================= */
/* ========================================================================= */
    case ZINVALID_COMMIT_TYPE:
      jam();
      stepAhead(signal, ZCOMMIT_LOG_SIZE - 1);
      break;
/* ========================================================================= */
/* ========================================================================= */
    case ZCOMMIT_TYPE:
    {
      CommitLogRecord commitLogRecord;
      jam();
      tcConnectptr.i = logPartPtr.p->logTcConrec;
      ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
      readCommitLog(signal, &commitLogRecord);
      if (tcConnectptr.p->gci > crestartNewestGci) {
        jam();
/*---------------------------------------------------------------------------*/
/* THIS LOG RECORD MUST BE IGNORED. IT IS PART OF A GLOBAL CHECKPOINT WHICH  */
/* WILL BE INVALIDATED BY THE SYSTEM RESTART. IF NOT INVALIDATED IT MIGHT BE */
/* EXECUTED IN A FUTURE SYSTEM RESTART.                                      */
/*---------------------------------------------------------------------------*/
        tmpLogPagePtr.i = logPartPtr.p->prevLogpage;
        ptrCheckGuard(tmpLogPagePtr, clogPageFileSize, logPageRecord);
        arrGuard(logPartPtr.p->savePageIndex, ZPAGE_SIZE);
        tmpLogPagePtr.p->logPageWord[logPartPtr.p->savePageIndex] = 
                                                  ZINVALID_COMMIT_TYPE;
        tmpLogPagePtr.p->logPageWord[ZPOS_DIRTY] = ZDIRTY;
      } else {
        jam();
/*---------------------------------------------------------------------------*/
/* CHECK IF I AM SUPPOSED TO EXECUTE THIS LOG RECORD. IF I AM THEN SAVE PAGE */
/* INDEX IN CURRENT LOG PAGE SINCE IT WILL BE OVERWRITTEN WHEN EXECUTING THE */
/* LOG RECORD.                                                               */
/*---------------------------------------------------------------------------*/
        logPartPtr.p->execSrExecuteIndex = 0;
        Uint32 result = checkIfExecLog(signal);
        if (result == ZOK) {
          jam();
//*---------------------------------------------------------------------------*/
/* IN A NODE RESTART WE WILL NEVER END UP HERE SINCE NO FRAGMENTS HAVE BEEN  */
/* DEFINED YET. THUS NO EXTRA CHECKING FOR NODE RESTART IS NECESSARY.        */
/*---------------------------------------------------------------------------*/
          logPartPtr.p->savePageIndex = 
             logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX];
          tcConnectptr.p->fragmentptr = fragptr.i;
          findPageRef(signal, &commitLogRecord);
          logPartPtr.p->execSrLogPageIndex = commitLogRecord.startPageIndex;
          if (logPagePtr.i != RNIL) {
            jam();
            logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] = commitLogRecord.startPageIndex;
            logPartPtr.p->execSrLogPage = logPagePtr.i;
            execLogRecord(signal);
            return;
          }//if
          logPartPtr.p->execSrStartPageNo = commitLogRecord.startPageNo;
          logPartPtr.p->execSrStopPageNo = commitLogRecord.stopPageNo;
          findLogfile(signal, commitLogRecord.fileNo, logPartPtr, &logFilePtr);
          logPartPtr.p->execSrExecLogFile = logFilePtr.i;
          if (logFilePtr.i == logPartPtr.p->currentLogfile) {
            jam();
            readExecLog(signal);
            lfoPtr.p->lfoState = LogFileOperationRecord::READ_EXEC_LOG;
            return;
          } else {
            jam();
/*---------------------------------------------------------------------------*/
/* THE FILE IS CURRENTLY NOT OPEN. WE MUST OPEN IT BEFORE WE CAN READ FROM   */
/* THE FILE.                                                                 */
/*---------------------------------------------------------------------------*/
            logFilePtr.p->logFileStatus = LogFileRecord::OPEN_EXEC_LOG;
            openFileRw(signal, logFilePtr);
            return;
          }//if
        }//if
      }//if
      break;
    }
/* ========================================================================= */
/* ========================================================================= */
    case ZABORT_TYPE:
      jam();
      stepAhead(signal, ZABORT_LOG_SIZE - 1);
      break;
/* ========================================================================= */
/* ========================================================================= */
    case ZFD_TYPE:
      jam();
/*---------------------------------------------------------------------------*/
/* THIS IS THE FIRST ITEM WE ENCOUNTER IN A NEW FILE. AT THIS MOMENT WE SHALL*/
/* SIMPLY BYPASS IT. IT HAS NO SIGNIFANCE WHEN EXECUTING THE LOG. IT HAS ITS */
/* SIGNIFANCE WHEN FINDING THE START END THE END OF THE LOG.                 */
/* WE HARDCODE THE PAGE INDEX SINCE THIS SHOULD NEVER BE FOUND AT ANY OTHER  */
/* PLACE THAN IN THE FIRST PAGE OF A NEW FILE IN THE FIRST POSITION AFTER THE*/
/* HEADER.                                                                   */
/*---------------------------------------------------------------------------*/
      ndbrequire(logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] == 
	  (ZPAGE_HEADER_SIZE + ZPOS_NO_FD));
      {
        Uint32 noFdDescriptors = 
	  logPagePtr.p->logPageWord[ZPAGE_HEADER_SIZE + ZPOS_NO_FD];
          logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] = 
	      (ZPAGE_HEADER_SIZE + ZFD_HEADER_SIZE) + 
	      (noFdDescriptors * ZFD_PART_SIZE);
      }
      break;
/* ========================================================================= */
/* ========================================================================= */
    case ZNEXT_LOG_RECORD_TYPE:
      jam();
      stepAhead(signal, ZPAGE_SIZE - logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX]);
      break;
/* ========================================================================= */
/* ========================================================================= */
    case ZNEXT_MBYTE_TYPE:
/*---------------------------------------------------------------------------*/
/* WE WILL SKIP A PART OF THE LOG FILE. ACTUALLY THE NEXT POINTER IS TO      */
/* A NEW MBYTE. THEREFORE WE WILL START UP A NEW MBYTE. THIS NEW MBYTE IS    */
/* HOWEVER ONLY STARTED IF IT IS NOT AFTER THE STOP MBYTE.                   */
/* IF WE HAVE REACHED THE END OF THE STOP MBYTE THEN THE EXECUTION OF THE LOG*/
/* IS COMPLETED.                                                             */
/*---------------------------------------------------------------------------*/
      if (logPartPtr.p->currentLogfile == logPartPtr.p->stopLogfile) {
        if (logFilePtr.p->currentMbyte == logPartPtr.p->stopMbyte) {
          jam();
/*---------------------------------------------------------------------------*/
/* THIS WAS THE LAST MBYTE TO EXECUTE IN THIS LOG PART. WE SHOULD HAVE FOUND */
/* A COMPLETED GCI RECORD OF THE LAST GCI BEFORE THIS. FOR SOME REASON THIS  */
/* RECORD WAS NOT AVAILABLE ON THE LOG. CRASH THE SYSTEM, A VERY SERIOUS     */
/* ERROR WHICH WE MUST REALLY WORK HARD TO AVOID.                            */
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/* SEND A SIGNAL TO THE SIGNAL LOG AND THEN CRASH THE SYSTEM.                */
/*---------------------------------------------------------------------------*/
          signal->theData[0] = RNIL;
          signal->theData[1] = logPartPtr.i;
	  Uint32 tmp = logFilePtr.p->fileName[3];
	  tmp = (tmp >> 8) & 0xff;// To get the Directory, DXX.
	  signal->theData[2] = tmp;
          signal->theData[3] = logFilePtr.p->fileNo;
          signal->theData[4] = logFilePtr.p->currentFilepage;
          signal->theData[5] = logFilePtr.p->currentMbyte;
          signal->theData[6] = logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX];
          sendSignal(cownref, GSN_DEBUG_SIG, signal, 7, JBA);
          return;
        }//if
      }//if
/*---------------------------------------------------------------------------*/
/* START EXECUTION OF A NEW MBYTE IN THE LOG.                                */
/*---------------------------------------------------------------------------*/
      if (logFilePtr.p->currentMbyte < (ZNO_MBYTES_IN_FILE - 1)) {
        jam();
        logPartPtr.p->logExecState = LogPartRecord::LES_EXEC_LOG_NEW_MBYTE;
      } else {
        ndbrequire(logFilePtr.p->currentMbyte == (ZNO_MBYTES_IN_FILE - 1));
        jam();
/*---------------------------------------------------------------------------*/
/* WE HAVE TO CHANGE FILE. CLOSE THIS ONE AND THEN OPEN THE NEXT.            */
/*---------------------------------------------------------------------------*/
        logPartPtr.p->logExecState = LogPartRecord::LES_EXEC_LOG_NEW_FILE;
      }//if
      break;
/* ========================================================================= */
/* ========================================================================= */
    case ZCOMPLETED_GCI_TYPE:
      jam();
      logWord = readLogword(signal);
      if (logWord == logPartPtr.p->logLastGci) {
        jam();
/*---------------------------------------------------------------------------*/
/* IF IT IS THE LAST GCI TO LIVE AFTER SYSTEM RESTART THEN WE RECORD THE NEXT*/
/* WORD AS THE NEW HEADER OF THE LOG FILE. OTHERWISE WE SIMPLY IGNORE THIS   */
/* LOG RECORD.                                                               */
/*---------------------------------------------------------------------------*/
        if (csrPhasesCompleted == 0) {
          jam();
/*---------------------------------------------------------------------------*/
/*WE ONLY RECORD THE HEAD OF THE LOG IN THE FIRST LOG ROUND OF LOG EXECUTION.*/
/*---------------------------------------------------------------------------*/
          logPartPtr.p->headFileNo = logFilePtr.p->fileNo;
          logPartPtr.p->headPageNo = logFilePtr.p->currentFilepage;
          logPartPtr.p->headPageIndex = 
                  logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX];
        }//if
/*---------------------------------------------------------------------------*/
/* THERE IS NO NEED OF EXECUTING PAST THIS LINE SINCE THERE WILL ONLY BE LOG */
/* RECORDS THAT WILL BE OF NO INTEREST. THUS CLOSE THE FILE AND START THE    */
/* NEXT PHASE OF THE SYSTEM RESTART.                                         */
/*---------------------------------------------------------------------------*/
        logPartPtr.p->logExecState = LogPartRecord::LES_EXEC_LOG_COMPLETED;
      }//if
      break;
    default:
      jam();
/* ========================================================================= */
/* ========================================================================= */
/*---------------------------------------------------------------------------*/
/* SEND A SIGNAL TO THE SIGNAL LOG AND THEN CRASH THE SYSTEM.                */
/*---------------------------------------------------------------------------*/
      signal->theData[0] = RNIL;
      signal->theData[1] = logPartPtr.i;
      Uint32 tmp = logFilePtr.p->fileName[3];
      tmp = (tmp >> 8) & 0xff;// To get the Directory, DXX.
      signal->theData[2] = tmp;
      signal->theData[3] = logFilePtr.p->fileNo;
      signal->theData[4] = logFilePtr.p->currentMbyte;
      signal->theData[5] = logFilePtr.p->currentFilepage;
      signal->theData[6] = logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX];
      signal->theData[7] = logWord;
      sendSignal(cownref, GSN_DEBUG_SIG, signal, 8, JBA);
      return;
      break;
    }//switch
/*---------------------------------------------------------------------------*/
// We continue to execute log records until we find a proper one to execute or
// that we reach a new page.
/*---------------------------------------------------------------------------*/
  } while (1);
}//Dblqh::execSr()

/*---------------------------------------------------------------------------*/
/* THIS SIGNAL IS ONLY RECEIVED TO BE CAPTURED IN THE SIGNAL LOG. IT IS      */
/* ALSO USED TO CRASH THE SYSTEM AFTER SENDING A SIGNAL TO THE LOG.          */
/*---------------------------------------------------------------------------*/
void Dblqh::execDEBUG_SIG(Signal* signal) 
{
/*
2.5 TEMPORARY VARIABLES
-----------------------
*/
  UintR tdebug;

  jamEntry();
  logPagePtr.i = signal->theData[0];
  tdebug = logPagePtr.p->logPageWord[0];

  char buf[100];
  BaseString::snprintf(buf, 100, 
	   "Error while reading REDO log.\n"
	   "D=%d, F=%d Mb=%d FP=%d W1=%d W2=%d",
	   signal->theData[2], signal->theData[3], signal->theData[4],
	   signal->theData[5], signal->theData[6], signal->theData[7]);

  progError(__LINE__, ERR_SR_REDOLOG, buf);  

  return;
}//Dblqh::execDEBUG_SIG()

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
void Dblqh::closeExecLogLab(Signal* signal) 
{
  logFilePtr.p->logFileStatus = LogFileRecord::CLOSED;
  signal->theData[0] = ZEXEC_SR;
  signal->theData[1] = logFilePtr.p->logPartRec;
  sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
  return;
}//Dblqh::closeExecLogLab()

void Dblqh::openExecLogLab(Signal* signal) 
{
  readExecLog(signal);
  lfoPtr.p->lfoState = LogFileOperationRecord::READ_EXEC_LOG;
  return;
}//Dblqh::openExecLogLab()

void Dblqh::readExecLogLab(Signal* signal) 
{
  buildLinkedLogPageList(signal);
  logPartPtr.p->logExecState = LogPartRecord::LES_EXEC_LOGREC_FROM_FILE;
  logPartPtr.p->execSrLfoRec = lfoPtr.i;
  logPartPtr.p->execSrLogPage = logPagePtr.i;
  logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] = 
    logPartPtr.p->execSrLogPageIndex;
  execLogRecord(signal);
  return;
}//Dblqh::readExecLogLab()

/*---------------------------------------------------------------------------*/
/* THIS CODE IS USED TO EXECUTE A LOG RECORD WHEN IT'S DATA HAVE BEEN LOCATED*/
/* AND TRANSFERRED INTO MEMORY.                                              */
/*---------------------------------------------------------------------------*/
void Dblqh::execLogRecord(Signal* signal) 
{
  jamEntry();

  tcConnectptr.i = logPartPtr.p->logTcConrec;
  ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
  fragptr.i = tcConnectptr.p->fragmentptr;
  ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
  // Read a log record and prepare it for execution
  readLogHeader(signal);
  readKey(signal);
  readAttrinfo(signal);
  initReqinfoExecSr(signal);
  arrGuard(logPartPtr.p->execSrExecuteIndex, 4);
  BlockReference ref = fragptr.p->execSrBlockref[logPartPtr.p->execSrExecuteIndex];
  tcConnectptr.p->nextReplica = refToNode(ref);
  tcConnectptr.p->connectState = TcConnectionrec::LOG_CONNECTED;
  tcConnectptr.p->tcOprec = tcConnectptr.i;
  packLqhkeyreqLab(signal);
  return;
}//Dblqh::execLogRecord()

//----------------------------------------------------------------------------
// This function invalidates log pages after the last GCI record in a 
// system/node restart. This is to ensure that the end of the log is 
// consistent. This function is executed last in start phase 3.
// RT 450. EDTJAMO.
//----------------------------------------------------------------------------
void Dblqh::invalidateLogAfterLastGCI(Signal* signal) {
  
  jam();
  if (logPartPtr.p->logExecState != LogPartRecord::LES_EXEC_LOG_INVALIDATE) {
    jam();
    systemError(signal);
  }

  if (logFilePtr.p->fileNo != logPartPtr.p->invalidateFileNo) {
    jam();
    systemError(signal);
  }

  switch (lfoPtr.p->lfoState) {
  case LogFileOperationRecord::WRITE_SR_INVALIDATE_PAGES:
    jam();
    releaseLfo(signal);
    releaseLogpage(signal); 
    if (logPartPtr.p->invalidatePageNo < (ZNO_MBYTES_IN_FILE * ZPAGES_IN_MBYTE - 1)) {
      // We continue in this file.
      logPartPtr.p->invalidatePageNo++;
    } else {
      // We continue in the next file.
      logFilePtr.i = logFilePtr.p->nextLogFile;
      ptrCheckGuard(logFilePtr, clogFileFileSize, logFileRecord);
      logPartPtr.p->invalidateFileNo = logFilePtr.p->fileNo;
      // Page 0 is used for file descriptors.
      logPartPtr.p->invalidatePageNo = 1; 
      if (logFilePtr.p->logFileStatus != LogFileRecord::OPEN) {
	jam();
	logFilePtr.p->logFileStatus = LogFileRecord::OPEN_SR_INVALIDATE_PAGES;
	openFileRw(signal, logFilePtr);
	return;
	break;
      }
    }
    // Read a page from the log file. 
    readFileInInvalidate(signal);
    return;
    break;

  case LogFileOperationRecord::READ_SR_INVALIDATE_PAGES:
    jam();
    releaseLfo(signal);
    // Check if this page must be invalidated.
    // If the log lap number on a page after the head of the tail is the same 
    // as the actual log lap number we must invalidate this page. Otherwise it
    // could be impossible to find the end of the log in a later system/node 
    // restart.
    if (logPagePtr.p->logPageWord[ZPOS_LOG_LAP] == logPartPtr.p->logLap) {
      // This page must be invalidated.
      logPagePtr.p->logPageWord[ZPOS_LOG_LAP] = 0;
      // Contact NDBFS. Real time break.
      writeSinglePage(signal, logPartPtr.p->invalidatePageNo, ZPAGE_SIZE - 1);
      lfoPtr.p->lfoState = LogFileOperationRecord::WRITE_SR_INVALIDATE_PAGES;
    } else {
      // We are done with invalidating. Finish start phase 3.4. 
      exitFromInvalidate(signal);
    }
    return;
    break;

  default:
    jam();
    systemError(signal);
    return;
    break;
  }

  return;
}//Dblqh::invalidateLogAfterLastGCI

void Dblqh::readFileInInvalidate(Signal* signal) {
  jam();
  // Contact NDBFS. Real time break.
  readSinglePage(signal, logPartPtr.p->invalidatePageNo); 
  lfoPtr.p->lfoState = LogFileOperationRecord::READ_SR_INVALIDATE_PAGES;
}

void Dblqh::exitFromInvalidate(Signal* signal) {
  jam();
  // Close files if necessary. Current file and the next file should be 
  // left open.
  if (logFilePtr.i != logPartPtr.p->currentLogfile) {
    LogFileRecordPtr currentLogFilePtr;
    LogFileRecordPtr nextAfterCurrentLogFilePtr;

    currentLogFilePtr.i = logPartPtr.p->currentLogfile;
    ptrCheckGuard(currentLogFilePtr, clogFileFileSize, logFileRecord);

    nextAfterCurrentLogFilePtr.i = currentLogFilePtr.p->nextLogFile;

    if (logFilePtr.i != nextAfterCurrentLogFilePtr.i) {
      // This file should be closed.
      logFilePtr.p->logFileStatus = LogFileRecord::CLOSE_SR_INVALIDATE_PAGES;
      closeFile(signal, logFilePtr);  
      // Return from this function and wait for close confirm. Then come back 
      // and test the previous file for closing.
      return;
    }
  }    

  // We are done with closing files, send completed signal and exit this phase.
  signal->theData[0] = ZSR_FOURTH_COMP;
  signal->theData[1] = logPartPtr.i;
  sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
  return;
}


/*---------------------------------------------------------------------------*/
/* THE EXECUTION OF A LOG RECORD IS COMPLETED. RELEASE PAGES IF THEY WERE    */
/* READ FROM DISK FOR THIS PARTICULAR OPERATION.                             */
/*---------------------------------------------------------------------------*/
void Dblqh::completedLab(Signal* signal) 
{
  Uint32 result = returnExecLog(signal);
/*---------------------------------------------------------------------------*/
/*       ENTER COMPLETED WITH                                                */
/*         LQH_CONNECTPTR                                                    */
/*---------------------------------------------------------------------------*/
  if (result == ZOK) {
    jam();
    execLogRecord(signal);
    return;
  } else if (result == ZNOT_OK) {
    jam();
    signal->theData[0] = ZEXEC_SR;
    signal->theData[1] = logPartPtr.i;
    sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
  } else {
    jam();
    /*empty*/;
  }//if
/*---------------------------------------------------------------------------*/
/* WE HAVE TO WAIT FOR CLOSING OF THE EXECUTED LOG FILE BEFORE PROCEEDING IN */
/* RARE CASES.                                                               */
/*---------------------------------------------------------------------------*/
  return;
}//Dblqh::completedLab()

/*---------------------------------------------------------------------------*/
/* EXECUTION OF LOG RECORD WAS NOT SUCCESSFUL. CHECK IF IT IS OK ANYWAY,     */
/* THEN EXECUTE THE NEXT LOG RECORD.                                         */
/*---------------------------------------------------------------------------*/
void Dblqh::logLqhkeyrefLab(Signal* signal) 
{
  Uint32 result = returnExecLog(signal);
  switch (tcConnectptr.p->operation) {
  case ZUPDATE:
  case ZDELETE:
    jam();
    ndbrequire(terrorCode == ZNO_TUPLE_FOUND);
    break;
  case ZINSERT:
    jam();
    ndbrequire(terrorCode == ZTUPLE_ALREADY_EXIST);
    break;
  default:
    ndbrequire(false);
    return;
    break;
  }//switch
  if (result == ZOK) {
    jam();
    execLogRecord(signal);
    return;
  } else if (result == ZNOT_OK) {
    jam();
    signal->theData[0] = ZEXEC_SR;
    signal->theData[1] = logPartPtr.i;
    sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
  } else {
    jam();
    /*empty*/;
  }//if
  /* ------------------------------------------------------------------------
   *  WE HAVE TO WAIT FOR CLOSING OF THE EXECUTED LOG FILE BEFORE 
   *  PROCEEDING IN RARE CASES.
   * ----------------------------------------------------------------------- */
  return;
}//Dblqh::logLqhkeyrefLab()

void Dblqh::closeExecSrCompletedLab(Signal* signal) 
{
  logFilePtr.p->logFileStatus = LogFileRecord::CLOSED;
  signal->theData[0] = logFilePtr.p->logPartRec;
  execLogComp(signal);
  return;
}//Dblqh::closeExecSrCompletedLab()

/* --------------------------------------------------------------------------
 *  ONE OF THE LOG PARTS HAVE COMPLETED EXECUTING THE LOG. CHECK IF ALL LOG
 *  PARTS ARE COMPLETED. IF SO START SENDING EXEC_FRAGCONF AND EXEC_SRCONF.
 * ------------------------------------------------------------------------- */
void Dblqh::execLogComp(Signal* signal) 
{
  logPartPtr.i = signal->theData[0];
  ptrCheckGuard(logPartPtr, clogPartFileSize, logPartRecord);
  logPartPtr.p->logPartState = LogPartRecord::SR_THIRD_PHASE_COMPLETED;
  /* ------------------------------------------------------------------------
   *  WE MUST RELEASE THE TC CONNECT RECORD HERE SO THAT IT CAN BE REUSED.
   * ----------------------------------------------------------------------- */
  tcConnectptr.i = logPartPtr.p->logTcConrec;
  ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
  releaseTcrecLog(signal, tcConnectptr);
  for (logPartPtr.i = 0; logPartPtr.i <= 3; logPartPtr.i++) {
    jam();
    ptrAss(logPartPtr, logPartRecord);
    if (logPartPtr.p->logPartState != LogPartRecord::SR_THIRD_PHASE_COMPLETED) {
      if (logPartPtr.p->logPartState != LogPartRecord::SR_THIRD_PHASE_STARTED) {
        jam();
        systemErrorLab(signal);
        return;
      } else {
        jam();
	/* ------------------------------------------------------------------
	 *  THIS LOG PART WAS NOT COMPLETED YET. EXIT AND WAIT FOR IT 
	 *  TO COMPLETE     
	 * ----------------------------------------------------------------- */
        return;
      }//if
    }//if
  }//for
  /* ------------------------------------------------------------------------
   *   ALL LOG PARTS HAVE COMPLETED THE EXECUTION OF THE LOG. WE CAN NOW START
   *   SENDING THE EXEC_FRAGCONF SIGNALS TO ALL INVOLVED FRAGMENTS.
   * ----------------------------------------------------------------------- */
  if (cstartType != NodeState::ST_NODE_RESTART) {
    jam();
    signal->theData[0] = ZSEND_EXEC_CONF;
    signal->theData[1] = 0;
    sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
  } else {
    jam();
    /* ----------------------------------------------------------------------
     *  FOR NODE RESTART WE CAN SKIP A NUMBER OF STEPS SINCE WE HAVE NO 
     *  FRAGMENTS DEFINED AT THIS POINT. OBVIOUSLY WE WILL NOT NEED TO 
     *  EXECUTE ANY MORE LOG STEPS EITHER AND THUS WE CAN IMMEDIATELY 
     *  START FINDING THE END AND THE START OF THE LOG.
     * --------------------------------------------------------------------- */
    csrPhasesCompleted = 3;
    execSrCompletedLab(signal);
    return;
  }//if
  return;
}//Dblqh::execLogComp()

/* --------------------------------------------------------------------------
 *  GO THROUGH THE FRAGMENT RECORDS TO DEDUCE TO WHICH SHALL BE SENT
 *  EXEC_FRAGCONF AFTER COMPLETING THE EXECUTION OF THE LOG.
 * ------------------------------------------------------------------------- */
void Dblqh::sendExecConf(Signal* signal) 
{
  jamEntry();
  fragptr.i = signal->theData[0];
  Uint32 loopCount = 0;
  while (fragptr.i < cfragrecFileSize) {
    ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
    if (fragptr.p->execSrStatus != Fragrecord::IDLE) {
      jam();
      ndbrequire(fragptr.p->execSrNoReplicas - 1 < 4);
      for (Uint32 i = 0; i < fragptr.p->execSrNoReplicas; i++) {
        jam();
        signal->theData[0] = fragptr.p->execSrUserptr[i];
        sendSignal(fragptr.p->execSrBlockref[i], GSN_EXEC_FRAGCONF, 
		   signal, 1, JBB);
      }//for
      if (fragptr.p->execSrStatus == Fragrecord::ACTIVE) {
        jam();
        fragptr.p->execSrStatus = Fragrecord::IDLE;
      } else {
        ndbrequire(fragptr.p->execSrStatus == Fragrecord::ACTIVE_REMOVE_AFTER);
        jam();
        Uint32 fragId = fragptr.p->fragId;
        tabptr.i = fragptr.p->tabRef;
        ptrCheckGuard(tabptr, ctabrecFileSize, tablerec);
        deleteFragrec(fragId);
      }//if
      fragptr.p->execSrNoReplicas = 0;
    }//if
    loopCount++;
    if (loopCount > 20) {
      jam();
      signal->theData[0] = ZSEND_EXEC_CONF;
      signal->theData[1] = fragptr.i + 1;
      sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
      return;
    } else {
      jam();
      fragptr.i++;
    }//if
  }//while
    /* ----------------------------------------------------------------------
     *  WE HAVE NOW SENT ALL EXEC_FRAGCONF. NOW IT IS TIME TO SEND 
     *  EXEC_SRCONF TO ALL NODES.
     * --------------------------------------------------------------------- */
  srPhase3Comp(signal);
}//Dblqh::sendExecConf()

/* --------------------------------------------------------------------------
 *       PHASE 3 HAS NOW COMPLETED. INFORM ALL OTHER NODES OF THIS EVENT.
 * ------------------------------------------------------------------------- */
void Dblqh::srPhase3Comp(Signal* signal) 
{
  jamEntry();
  ndbrequire(cnoOfNodes < MAX_NDB_NODES);
  for (Uint32 i = 0; i < cnoOfNodes; i++) {
    jam();
    if (cnodeStatus[i] == ZNODE_UP) {
      jam();
      ndbrequire(cnodeData[i] < MAX_NDB_NODES);
      BlockReference ref = calcLqhBlockRef(cnodeData[i]);
      signal->theData[0] = cownNodeid;
      sendSignal(ref, GSN_EXEC_SRCONF, signal, 1, JBB);
    }//if
  }//for
  return;
}//Dblqh::srPhase3Comp()

/* ########################################################################## 
 *    SYSTEM RESTART PHASE FOUR MODULE
 *    THIS MODULE IS A SUB-MODULE OF THE FILE SYSTEM HANDLING.
 *
 *    THIS MODULE SETS UP THE HEAD AND TAIL POINTERS OF THE LOG PARTS IN THE
 *    FRAGMENT LOG. WHEN IT IS COMPLETED IT REPORTS TO THE MASTER DIH THAT
 *    IT HAS COMPLETED THE PART OF THE SYSTEM RESTART WHERE THE DATABASE IS
 *    LOADED.
 *    IT ALSO OPENS THE CURRENT LOG FILE AND THE NEXT AND SETS UP THE FIRST 
 *    LOG PAGE WHERE NEW LOG DATA IS TO BE INSERTED WHEN THE SYSTEM STARTS 
 *    AGAIN.
 *
 *    THIS PART IS ACTUALLY EXECUTED FOR ALL RESTART TYPES.
 * ######################################################################### */
void Dblqh::initFourth(Signal* signal) 
{
  LogFileRecordPtr locLogFilePtr;
  jamEntry();
  logPartPtr.i = signal->theData[0];
  ptrCheckGuard(logPartPtr, clogPartFileSize, logPartRecord);
  crestartNewestGci = 1;
  crestartOldestGci = 1;
  /* ------------------------------------------------------------------------
   *       INITIALISE LOG PART AND LOG FILES AS NEEDED.
   * ----------------------------------------------------------------------- */
  logPartPtr.p->headFileNo = 0;
  logPartPtr.p->headPageNo = 1;
  logPartPtr.p->headPageIndex = ZPAGE_HEADER_SIZE + 2;
  logPartPtr.p->logPartState = LogPartRecord::SR_FOURTH_PHASE_STARTED;
  logPartPtr.p->logTailFileNo = 0;
  logPartPtr.p->logTailMbyte = 0;
  locLogFilePtr.i = logPartPtr.p->firstLogfile;
  ptrCheckGuard(locLogFilePtr, clogFileFileSize, logFileRecord);
  locLogFilePtr.p->logFileStatus = LogFileRecord::OPEN_SR_FOURTH_PHASE;
  openFileRw(signal, locLogFilePtr);
  return;
}//Dblqh::initFourth()

void Dblqh::openSrFourthPhaseLab(Signal* signal) 
{
  /* ------------------------------------------------------------------------
   *  WE HAVE NOW OPENED THE HEAD LOG FILE WE WILL NOW START READING IT 
   *  FROM THE HEAD MBYTE TO FIND THE NEW HEAD OF THE LOG.
   * ----------------------------------------------------------------------- */
  readSinglePage(signal, logPartPtr.p->headPageNo);
  lfoPtr.p->lfoState = LogFileOperationRecord::READ_SR_FOURTH_PHASE;
  return;
}//Dblqh::openSrFourthPhaseLab()

void Dblqh::readSrFourthPhaseLab(Signal* signal) 
{
  if(c_diskless){
    jam();
    logPagePtr.p->logPageWord[ZPOS_LOG_LAP] = 1;
  }

  /* ------------------------------------------------------------------------
   *  INITIALISE ALL LOG PART INFO AND LOG FILE INFO THAT IS NEEDED TO 
   *  START UP THE SYSTEM.
   * ------------------------------------------------------------------------
   *  INITIALISE THE NEWEST GLOBAL CHECKPOINT IDENTITY AND THE NEWEST 
   *  COMPLETED GLOBAL CHECKPOINT IDENITY AS THE NEWEST THAT WAS RESTARTED.
   * ------------------------------------------------------------------------
   *  INITIALISE THE HEAD PAGE INDEX IN THIS PAGE.
   *  ASSIGN IT AS THE CURRENT LOGPAGE.
   *  ASSIGN THE FILE AS THE CURRENT LOG FILE.
   *  ASSIGN THE CURRENT FILE NUMBER FROM THE CURRENT LOG FILE AND THE NEXT
   *  FILE NUMBER FROM THE NEXT LOG FILE.
   *  ASSIGN THE CURRENT FILEPAGE FROM HEAD PAGE NUMBER.
   *  ASSIGN THE CURRENT MBYTE BY DIVIDING PAGE NUMBER BY 128.
   *  INITIALISE LOG LAP TO BE THE LOG LAP AS FOUND IN THE HEAD PAGE.
   *  WE HAVE TO CALCULATE THE NUMBER OF REMAINING WORDS IN THIS MBYTE.
   * ----------------------------------------------------------------------- */
  cnewestGci = crestartNewestGci;
  cnewestCompletedGci = crestartNewestGci;
  logPartPtr.p->logPartNewestCompletedGCI = cnewestCompletedGci;
  logPartPtr.p->currentLogfile = logFilePtr.i;
  logFilePtr.p->filePosition = logPartPtr.p->headPageNo;
  logFilePtr.p->currentMbyte = 
                  logPartPtr.p->headPageNo >> ZTWOLOG_NO_PAGES_IN_MBYTE;
  logFilePtr.p->fileChangeState = LogFileRecord::NOT_ONGOING;
  logPartPtr.p->logLap = logPagePtr.p->logPageWord[ZPOS_LOG_LAP];
  logFilePtr.p->currentFilepage = logPartPtr.p->headPageNo;
  logFilePtr.p->currentLogpage = logPagePtr.i;

  initLogpage(signal);
  logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] = logPartPtr.p->headPageIndex;
  logFilePtr.p->remainingWordsInMbyte = 
    ((
      ((logFilePtr.p->currentMbyte + 1) * ZPAGES_IN_MBYTE) -
     logFilePtr.p->currentFilepage) *
    (ZPAGE_SIZE - ZPAGE_HEADER_SIZE)) -
      (logPartPtr.p->headPageIndex - ZPAGE_HEADER_SIZE);
  /* ------------------------------------------------------------------------
   *     THE NEXT STEP IS TO OPEN THE NEXT LOG FILE (IF THERE IS ONE).
   * ----------------------------------------------------------------------- */
  if (logFilePtr.p->nextLogFile != logFilePtr.i) {
    LogFileRecordPtr locLogFilePtr;
    jam();
    locLogFilePtr.i = logFilePtr.p->nextLogFile;
    ptrCheckGuard(locLogFilePtr, clogFileFileSize, logFileRecord);
    locLogFilePtr.p->logFileStatus = LogFileRecord::OPEN_SR_FOURTH_NEXT;
    openFileRw(signal, locLogFilePtr);
  } else {
    jam();
    /* ----------------------------------------------------------------------
     *  THIS CAN ONLY OCCUR IF WE HAVE ONLY ONE LOG FILE. THIS LOG FILE MUST 
     *  BE LOG FILE ZERO AND THAT IS THE FILE WE CURRENTLY HAVE READ.
     *  THUS WE CAN CONTINUE IMMEDIATELY TO READ PAGE ZERO IN FILE ZERO.
     * --------------------------------------------------------------------- */
    openSrFourthZeroSkipInitLab(signal);
    return;
  }//if
  return;
}//Dblqh::readSrFourthPhaseLab()

void Dblqh::openSrFourthNextLab(Signal* signal) 
{
  /* ------------------------------------------------------------------------
   *       WE MUST ALSO HAVE FILE 0 OPEN ALL THE TIME.
   * ----------------------------------------------------------------------- */
  logFilePtr.i = logPartPtr.p->firstLogfile;
  ptrCheckGuard(logFilePtr, clogFileFileSize, logFileRecord);
  if (logFilePtr.p->logFileStatus == LogFileRecord::OPEN) {
    jam();
    openSrFourthZeroSkipInitLab(signal);
    return;
  } else {
    jam();
    logFilePtr.p->logFileStatus = LogFileRecord::OPEN_SR_FOURTH_ZERO;
    openFileRw(signal, logFilePtr);
  }//if
  return;
}//Dblqh::openSrFourthNextLab()

void Dblqh::openSrFourthZeroLab(Signal* signal) 
{
  openSrFourthZeroSkipInitLab(signal);
  return;
}//Dblqh::openSrFourthZeroLab()

void Dblqh::openSrFourthZeroSkipInitLab(Signal* signal) 
{
  if (logFilePtr.i == logPartPtr.p->currentLogfile) {
    if (logFilePtr.p->currentFilepage == 0) {
      jam();
      /* -------------------------------------------------------------------
       *  THE HEADER PAGE IN THE LOG IS PAGE ZERO IN FILE ZERO. 
       *  THIS SHOULD NEVER OCCUR.
       * ------------------------------------------------------------------- */
      systemErrorLab(signal);
      return;
    }//if
  }//if
  readSinglePage(signal, 0);
  lfoPtr.p->lfoState = LogFileOperationRecord::READ_SR_FOURTH_ZERO;
  return;
}//Dblqh::openSrFourthZeroSkipInitLab()

void Dblqh::readSrFourthZeroLab(Signal* signal) 
{
  logFilePtr.p->logPageZero = logPagePtr.i;
  // --------------------------------------------------------------------
  //   This is moved to invalidateLogAfterLastGCI(), RT453. 
  //   signal->theData[0] = ZSR_FOURTH_COMP;
  //   signal->theData[1] = logPartPtr.i;
  //   sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
  // --------------------------------------------------------------------
  
  // Need to invalidate log pages after the head of the log. RT 453. EDTJAMO.
  // Set the start of the invalidation.
  logFilePtr.i = logPartPtr.p->currentLogfile;
  ptrCheckGuard(logFilePtr, clogFileFileSize, logFileRecord);
  logPartPtr.p->invalidateFileNo = logPartPtr.p->headFileNo;
  logPartPtr.p->invalidatePageNo = logPartPtr.p->headPageNo;
   
  logPartPtr.p->logExecState = LogPartRecord::LES_EXEC_LOG_INVALIDATE;
  seizeLfo(signal);
  initLfo(signal);
  // The state here is a little confusing, but simulates that we return
  // to invalidateLogAfterLastGCI() from an invalidate write and are ready
  // to read a page from file. 
  lfoPtr.p->lfoState = LogFileOperationRecord::WRITE_SR_INVALIDATE_PAGES;

  invalidateLogAfterLastGCI(signal);
  return;
}//Dblqh::readSrFourthZeroLab()

/* -------------------------------------------------------------------------- 
 *     ONE OF THE LOG PARTS HAVE COMPLETED PHASE FOUR OF THE SYSTEM RESTART.
 *     CHECK IF ALL LOG PARTS ARE COMPLETED. IF SO SEND START_RECCONF
 * ------------------------------------------------------------------------- */
void Dblqh::srFourthComp(Signal* signal) 
{
  jamEntry();
  logPartPtr.i = signal->theData[0];
  ptrCheckGuard(logPartPtr, clogPartFileSize, logPartRecord);
  logPartPtr.p->logPartState = LogPartRecord::SR_FOURTH_PHASE_COMPLETED;
  for (logPartPtr.i = 0; logPartPtr.i <= 3; logPartPtr.i++) {
    jam();
    ptrAss(logPartPtr, logPartRecord);
    if (logPartPtr.p->logPartState != LogPartRecord::SR_FOURTH_PHASE_COMPLETED) {
      if (logPartPtr.p->logPartState != LogPartRecord::SR_FOURTH_PHASE_STARTED) {
        jam();
        systemErrorLab(signal);
        return;
      } else {
        jam();
	/* ------------------------------------------------------------------
	 *  THIS LOG PART WAS NOT COMPLETED YET. 
	 *  EXIT AND WAIT FOR IT TO COMPLETE
	 * ----------------------------------------------------------------- */
        return;
      }//if
    }//if
  }//for
  /* ------------------------------------------------------------------------
   *  ALL LOG PARTS HAVE COMPLETED PHASE FOUR OF THE SYSTEM RESTART. 
   *  WE CAN NOW SEND START_RECCONF TO THE MASTER DIH IF IT WAS A 
   *  SYSTEM RESTART. OTHERWISE WE WILL CONTINUE WITH AN INITIAL START. 
   *  SET LOG PART STATE TO IDLE TO
   *  INDICATE THAT NOTHING IS GOING ON IN THE LOG PART.
   * ----------------------------------------------------------------------- */
  for (logPartPtr.i = 0; logPartPtr.i <= 3; logPartPtr.i++) {
    ptrAss(logPartPtr, logPartRecord);
    logPartPtr.p->logPartState = LogPartRecord::IDLE;
  }//for

  if ((cstartType == NodeState::ST_INITIAL_START) || 
      (cstartType == NodeState::ST_INITIAL_NODE_RESTART)) {
    jam();
    
    ndbrequire(cinitialStartOngoing == ZTRUE);
    cinitialStartOngoing = ZFALSE;

    checkStartCompletedLab(signal);
    return;
  } else if ((cstartType == NodeState::ST_NODE_RESTART) ||
             (cstartType == NodeState::ST_SYSTEM_RESTART)) {
    jam();
    StartRecConf * conf = (StartRecConf*)signal->getDataPtrSend();
    conf->startingNodeId = getOwnNodeId();
    sendSignal(cmasterDihBlockref, GSN_START_RECCONF, signal, 
	       StartRecConf::SignalLength, JBB);

    if(cstartType == NodeState::ST_SYSTEM_RESTART){
      fragptr.i = c_redo_log_complete_frags;
      while(fragptr.i != RNIL){
	ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
	signal->theData[0] = fragptr.p->tabRef;
	signal->theData[1] = fragptr.p->fragId;
	sendSignal(DBACC_REF, GSN_EXPANDCHECK2, signal, 2, JBB);
	fragptr.i = fragptr.p->nextFrag;
      }
    }
  } else {
    ndbrequire(false);
  }//if
  return;
}//Dblqh::srFourthComp()

/* ######################################################################### */
/* #######                            ERROR MODULE                   ####### */
/*                                                                           */
/* ######################################################################### */
void Dblqh::warningHandlerLab(Signal* signal) 
{
  systemErrorLab(signal);
  return;
}//Dblqh::warningHandlerLab()

/*---------------------------------------------------------------------------*/
/* AN ERROR OCCURRED THAT WE WILL NOT TREAT AS SYSTEM ERROR. MOST OFTEN THIS */
/* WAS CAUSED BY AN ERRONEUS SIGNAL SENT BY ANOTHER NODE. WE DO NOT WISH TO  */
/* CRASH BECAUSE OF FAULTS IN OTHER NODES. THUS WE ONLY REPORT A WARNING.    */
/* THIS IS CURRENTLY NOT IMPLEMENTED AND FOR THE MOMENT WE GENERATE A SYSTEM */
/* ERROR SINCE WE WANT TO FIND FAULTS AS QUICKLY AS POSSIBLE IN A TEST PHASE.*/
/* IN A LATER PHASE WE WILL CHANGE THIS TO BE A WARNING MESSAGE INSTEAD.     */
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*      THIS TYPE OF ERROR SHOULD NOT GENERATE A SYSTEM ERROR IN A PRODUCT   */
/*      RELEASE. THIS IS A TEMPORARY SOLUTION DURING TEST PHASE TO QUICKLY   */
/*      FIND ERRORS. NORMALLY THIS SHOULD GENERATE A WARNING MESSAGE ONTO    */
/*      SOME ERROR LOGGER. THIS WILL LATER BE IMPLEMENTED BY SOME SIGNAL.    */
/*---------------------------------------------------------------------------*/
/* ------ SYSTEM ERROR SITUATIONS ------- */
/*      IN SITUATIONS WHERE THE STATE IS ERRONEOUS OR IF THE ERROR OCCURS IN */
/*      THE COMMIT, COMPLETE OR ABORT PHASE, WE PERFORM A CRASH OF THE AXE VM*/
/*---------------------------------------------------------------------------*/

void Dblqh::systemErrorLab(Signal* signal) 
{
  systemError(signal);
  progError(0, 0);
/*************************************************************************>*/
/*       WE WANT TO INVOKE AN IMMEDIATE ERROR HERE SO WE GET THAT BY       */
/*       INSERTING A CERTAIN POINTER OUT OF RANGE.                         */
/*************************************************************************>*/
}//Dblqh::systemErrorLab()

/* ------- ERROR SITUATIONS ------- */

void Dblqh::aiStateErrorCheckLab(Signal* signal, Uint32* dataPtr, Uint32 length) 
{
  ndbrequire(tcConnectptr.p->abortState != TcConnectionrec::ABORT_IDLE);
  if (tcConnectptr.p->transactionState != TcConnectionrec::IDLE) {
      jam();
/*************************************************************************>*/
/*       TRANSACTION ABORT IS ONGOING. IT CAN STILL BE A PART OF AN        */
/*       OPERATION THAT SHOULD CONTINUE SINCE THE TUPLE HAS NOT ARRIVED    */
/*       YET. THIS IS POSSIBLE IF ACTIVE CREATION OF THE FRAGMENT IS       */
/*       ONGOING.                                                          */
/*************************************************************************>*/
    if (tcConnectptr.p->activeCreat == ZTRUE) {
        jam();
/*************************************************************************>*/
/*       ONGOING ABORTS DURING ACTIVE CREATION MUST SAVE THE ATTRIBUTE INFO*/
/*       SO THAT IT CAN BE SENT TO THE NEXT NODE IN THE COMMIT CHAIN. THIS */
/*       IS NEEDED SINCE ALL ABORTS DURING CREATION OF A FRAGMENT ARE NOT  */
/*       REALLY ERRORS. A MISSING TUPLE TO BE UPDATED SIMPLY MEANS THAT    */
/*       IT HASN'T BEEN TRANSFERRED TO THE NEW REPLICA YET.                */
/*************************************************************************>*/
/*************************************************************************>*/
/*       AFTER THIS ERROR THE ABORT MUST BE COMPLETED. TO ENSURE THIS SET  */
/*       ACTIVE CREATION TO FALSE. THIS WILL ENSURE THAT THE ABORT IS      */
/*       COMPLETED.                                                        */
/*************************************************************************>*/
      if (saveTupattrbuf(signal, dataPtr, length) == ZOK) {
        jam();
        if (tcConnectptr.p->transactionState == 
            TcConnectionrec::WAIT_AI_AFTER_ABORT) {
          if (tcConnectptr.p->currTupAiLen == tcConnectptr.p->totReclenAi) {
            jam();
/*************************************************************************>*/
/*       WE WERE WAITING FOR MORE ATTRIBUTE INFO AFTER A SUCCESSFUL ABORT  */
/*       IN ACTIVE CREATION STATE. THE TRANSACTION SHOULD CONTINUE AS IF   */
/*       IT WAS COMMITTED. NOW ALL INFO HAS ARRIVED AND WE CAN CONTINUE    */
/*       WITH NORMAL PROCESSING AS IF THE TRANSACTION WAS PREPARED.        */
/*       SINCE THE FRAGMENT IS UNDER CREATION WE KNOW THAT LOGGING IS      */
/*       DISABLED. WE STILL HAVE TO CATER FOR DIRTY OPERATION OR NOT.      */
/*************************************************************************>*/
            tcConnectptr.p->abortState = TcConnectionrec::ABORT_IDLE;
            rwConcludedAiLab(signal);
            return;
          } else {
            ndbrequire(tcConnectptr.p->currTupAiLen < tcConnectptr.p->totReclenAi);
            jam();
            return;	/* STILL WAITING FOR MORE ATTRIBUTE INFO */
          }//if
        }//if
      } else {
        jam();
/*************************************************************************>*/
/*       AFTER THIS ERROR THE ABORT MUST BE COMPLETED. TO ENSURE THIS SET  */
/*       ACTIVE CREATION TO ABORT. THIS WILL ENSURE THAT THE ABORT IS      */
/*       COMPLETED AND THAT THE ERROR CODE IS PROPERLY SET                 */
/*************************************************************************>*/
        tcConnectptr.p->errorCode = terrorCode;
        tcConnectptr.p->activeCreat = ZFALSE;
        if (tcConnectptr.p->transactionState == 
	    TcConnectionrec::WAIT_AI_AFTER_ABORT) {
          jam();
/*************************************************************************>*/
/*       ABORT IS ALREADY COMPLETED. WE NEED TO RESTART IT FROM WHERE IT   */
/*       WAS INTERRUPTED.                                                  */
/*************************************************************************>*/
          continueAbortLab(signal);
          return;
        } else {
          jam();
          return;
/*************************************************************************>*/
// Abort is ongoing. It will complete since we set the activeCreat = ZFALSE
/*************************************************************************>*/
        }//if
      }//if
    }//if
  }//if
/*************************************************************************>*/
/* TRANSACTION HAVE BEEN ABORTED. THUS IGNORE ALL SIGNALS BELONGING TO IT. */
/*************************************************************************>*/
  return;
}//Dblqh::aiStateErrorCheckLab()

void Dblqh::takeOverErrorLab(Signal* signal) 
{
  terrorCode = ZTAKE_OVER_ERROR;
  abortErrorLab(signal);
  return;
}//Dblqh::takeOverErrorLab()

/* ##########################################################################
 *               TEST MODULE
 * ######################################################################### */
#ifdef VM_TRACE
void Dblqh::execTESTSIG(Signal* signal) 
{
  jamEntry();
  Uint32 userpointer = signal->theData[0];
  BlockReference userblockref = signal->theData[1];
  Uint32 testcase = signal->theData[2];

  signal->theData[0] = userpointer;
  signal->theData[1] = cownref;
  signal->theData[2] = testcase;
  sendSignal(userblockref, GSN_TESTSIG, signal, 25, JBB);
  return;
}//Dblqh::execTESTSIG()

/* *************** */
/*  MEMCHECKREQ  > */
/* *************** */
/* ************************************************************************>>
 * THIS SIGNAL IS PURELY FOR TESTING PURPOSES. IT CHECKS THE FREE LIST 
 * AND REPORTS THE NUMBER OF FREE RECORDS. 
 * THIS CAN BE DONE TO ENSURE THAT NO RECORDS HAS BEEN LOST
 * ************************************************************************> */
void Dblqh::execMEMCHECKREQ(Signal* signal) 
{
  Uint32* dataPtr = &signal->theData[0];
  jamEntry();
  BlockReference userblockref = signal->theData[0];
  Uint32 index = 0;
  for (Uint32 i = 0; i < 7; i++)
    dataPtr[i] = 0;
  addfragptr.i = cfirstfreeAddfragrec;
  while (addfragptr.i != RNIL) {
    ptrCheckGuard(addfragptr, caddfragrecFileSize, addFragRecord);
    addfragptr.i = addfragptr.p->nextAddfragrec;
    dataPtr[index]++;
  }//while
  index++;
  attrinbufptr.i = cfirstfreeAttrinbuf;
  while (attrinbufptr.i != RNIL) {
    ptrCheckGuard(attrinbufptr, cattrinbufFileSize, attrbuf);
    attrinbufptr.i = attrinbufptr.p->attrbuf[ZINBUF_NEXT];
    dataPtr[index]++;
  }//while
  index++;
  databufptr.i = cfirstfreeDatabuf;
  while (databufptr.i != RNIL) {
    ptrCheckGuard(databufptr, cdatabufFileSize, databuf);
    databufptr.i = databufptr.p->nextDatabuf;
    dataPtr[index]++;
  }//while
  index++;
  fragptr.i = cfirstfreeFragrec;
  while (fragptr.i != RNIL) {
    ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
    fragptr.i = fragptr.p->nextFrag;
    dataPtr[index]++;
  }//while
  index++;
  for (tabptr.i = 0;
       tabptr.i < ctabrecFileSize;
       tabptr.i++) {
    ptrAss(tabptr, tablerec);
    if (tabptr.p->tableStatus == Tablerec::NOT_DEFINED) {
      dataPtr[index]++;
    }//if
  }//for
  index++;
  tcConnectptr.i = cfirstfreeTcConrec;
  while (tcConnectptr.i != RNIL) {
    ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
    tcConnectptr.i = tcConnectptr.p->nextTcConnectrec;
    dataPtr[index]++;
  }//while
  sendSignal(userblockref, GSN_MEMCHECKCONF, signal, 10, JBB);
  return;
}//Dblqh::execMEMCHECKREQ()

#endif

/* ************************************************************************* */
/* ************************* STATEMENT BLOCKS ****************************** */
/* ************************************************************************* */
/* ========================================================================= */
/* ====== BUILD LINKED LIST OF LOG PAGES AFTER RECEIVING FSREADCONF  ======= */
/*                                                                           */
/* ========================================================================= */
void Dblqh::buildLinkedLogPageList(Signal* signal) 
{
  LogPageRecordPtr bllLogPagePtr;

  arrGuard(lfoPtr.p->noPagesRw - 1, 16);
  arrGuard(lfoPtr.p->noPagesRw, 16);
  for (UintR tbllIndex = 0; tbllIndex < lfoPtr.p->noPagesRw; tbllIndex++) {
    jam();
    /* ---------------------------------------------------------------------- 
     *  BUILD LINKED LIST BUT ALSO ENSURE THAT PAGE IS NOT SEEN AS DIRTY 
     *  INITIALLY.
     * --------------------------------------------------------------------- */
    bllLogPagePtr.i = lfoPtr.p->logPageArray[tbllIndex];
    ptrCheckGuard(bllLogPagePtr, clogPageFileSize, logPageRecord);

// #if VM_TRACE
//     // Check logPage checksum before modifying it
//     Uint32 calcCheckSum = calcPageCheckSum(bllLogPagePtr);
//     Uint32 checkSum = bllLogPagePtr.p->logPageWord[ZPOS_CHECKSUM];
//     if (checkSum != calcCheckSum) {
//       ndbout << "Redolog: Checksum failure." << endl;
//       progError(__LINE__, ERR_NDBREQUIRE, "Redolog: Checksum failure.");
//     }
// #endif

    bllLogPagePtr.p->logPageWord[ZNEXT_PAGE] = 
      lfoPtr.p->logPageArray[tbllIndex + 1];
    bllLogPagePtr.p->logPageWord[ZPOS_DIRTY] = ZNOT_DIRTY;
  }//for
  bllLogPagePtr.i = lfoPtr.p->logPageArray[lfoPtr.p->noPagesRw - 1];
  ptrCheckGuard(bllLogPagePtr, clogPageFileSize, logPageRecord);
  bllLogPagePtr.p->logPageWord[ZNEXT_PAGE] = RNIL;
}//Dblqh::buildLinkedLogPageList()

/* ========================================================================= 
 * =======                      CHANGE TO NEXT MBYTE IN LOG           ======= 
 *
 * ========================================================================= */
void Dblqh::changeMbyte(Signal* signal) 
{
  writeNextLog(signal);
  writeFileDescriptor(signal);
}//Dblqh::changeMbyte()

/* ========================================================================= */
/* ======       CHECK IF THIS COMMIT LOG RECORD IS TO BE EXECUTED    ======= */
/*                                                                           */
/*      SUBROUTINE SHORT NAME = CEL                                          */
/* ========================================================================= */
Uint32 Dblqh::checkIfExecLog(Signal* signal) 
{
  tabptr.i = tcConnectptr.p->tableref;
  ptrCheckGuard(tabptr, ctabrecFileSize, tablerec);
  if (getFragmentrec(signal, tcConnectptr.p->fragmentid) &&
      (table_version_major(tabptr.p->schemaVersion) == table_version_major(tcConnectptr.p->schemaVersion))) {
    if (fragptr.p->execSrStatus != Fragrecord::IDLE) {
      if (fragptr.p->execSrNoReplicas > logPartPtr.p->execSrExecuteIndex) {
        ndbrequire((fragptr.p->execSrNoReplicas - 1) < 4);
        for (Uint32 i = logPartPtr.p->execSrExecuteIndex; 
	     i < fragptr.p->execSrNoReplicas; 
	     i++) {
          jam();
          if (tcConnectptr.p->gci >= fragptr.p->execSrStartGci[i]) {
            if (tcConnectptr.p->gci <= fragptr.p->execSrLastGci[i]) {
              jam();
              logPartPtr.p->execSrExecuteIndex = i;
              return ZOK;
            }//if
          }//if
        }//for
      }//if
    }//if
  }//if
  return ZNOT_OK;
}//Dblqh::checkIfExecLog()

/* ========================================================================= */
/* == CHECK IF THERE IS LESS THAN 192 KBYTE IN THE BUFFER PLUS INCOMING  === */
/*      READS ALREADY STARTED. IF SO IS THE CASE THEN START ANOTHER READ IF  */
/*      THERE ARE MORE PAGES IN THIS MBYTE.                                  */
/*                                                                           */
/* ========================================================================= */
void Dblqh::checkReadExecSr(Signal* signal) 
{
  logPartPtr.p->logExecState = LogPartRecord::LES_EXEC_LOG;
  logPartPtr.p->execSrPagesRead = logPartPtr.p->execSrPagesRead + 8;
  logPartPtr.p->execSrPagesReading = logPartPtr.p->execSrPagesReading - 8;
  if ((logPartPtr.p->execSrPagesRead + logPartPtr.p->execSrPagesReading) < 
      ZREAD_AHEAD_SIZE) {
    jam();
    /* ----------------------------------------------------------------------
     *  WE HAVE LESS THAN 64 KBYTE OF LOG PAGES REMAINING IN MEMORY OR ON 
     *  ITS WAY TO MAIN MEMORY. READ IN 8 MORE PAGES.
     * --------------------------------------------------------------------- */
    if ((logPartPtr.p->execSrPagesRead + logPartPtr.p->execSrPagesExecuted) < 
	ZPAGES_IN_MBYTE) {
      jam();
      /* --------------------------------------------------------------------
       *  THERE ARE MORE PAGES TO READ IN THIS MBYTE. READ THOSE FIRST
       *  IF >= ZPAGES_IN_MBYTE THEN THERE ARE NO MORE PAGES TO READ. THUS
       *  WE PROCEED WITH EXECUTION OF THE LOG.
       * ------------------------------------------------------------------- */
      readExecSr(signal);
      logPartPtr.p->logExecState = LogPartRecord::LES_WAIT_READ_EXEC_SR;
    }//if
  }//if
}//Dblqh::checkReadExecSr()

/* ========================================================================= */
/* ==== CHECK IF START OF NEW FRAGMENT IS COMPLETED AND WE CAN       ======= */
/* ==== GET THE START GCI                                            ======= */
/*                                                                           */
/*      SUBROUTINE SHORT NAME = CTC                                          */
/* ========================================================================= */
void Dblqh::checkScanTcCompleted(Signal* signal) 
{
  tcConnectptr.p->logWriteState = TcConnectionrec::NOT_STARTED;
  fragptr.i = tcConnectptr.p->fragmentptr;
  ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
  fragptr.p->activeTcCounter = fragptr.p->activeTcCounter - 1;
  if (fragptr.p->activeTcCounter == 0) {
    jam();
    fragptr.p->startGci = cnewestGci + 1;
    tabptr.i = tcConnectptr.p->tableref;
    ptrCheckGuard(tabptr, ctabrecFileSize, tablerec);
    sendCopyActiveConf(signal, tcConnectptr.p->tableref);
  }//if
}//Dblqh::checkScanTcCompleted()

/* ========================================================================== 
 * === CHECK IF ALL PARTS OF A SYSTEM RESTART ON A FRAGMENT ARE COMPLETED === 
 *
 *       SUBROUTINE SHORT NAME = CSC
 * ========================================================================= */
void Dblqh::checkSrCompleted(Signal* signal) 
{
  LcpLocRecordPtr cscLcpLocptr;
  
  terrorCode = ZOK;
  ptrGuard(lcpPtr);
  cscLcpLocptr.i = lcpPtr.p->firstLcpLocAcc;
CSC_ACC_DOWHILE:
  ptrCheckGuard(cscLcpLocptr, clcpLocrecFileSize, lcpLocRecord);
  if (cscLcpLocptr.p->lcpLocstate != LcpLocRecord::SR_ACC_COMPLETED) {
    jam();
    if (cscLcpLocptr.p->lcpLocstate != LcpLocRecord::SR_ACC_STARTED) {
      jam();
      systemErrorLab(signal);
      return;
    }//if
    return;
  }//if
  cscLcpLocptr.i = cscLcpLocptr.p->nextLcpLoc;
  if (cscLcpLocptr.i != RNIL) {
    jam();
    goto CSC_ACC_DOWHILE;
  }//if
  cscLcpLocptr.i = lcpPtr.p->firstLcpLocTup;
CSC_TUP_DOWHILE:
  ptrCheckGuard(cscLcpLocptr, clcpLocrecFileSize, lcpLocRecord);
  if (cscLcpLocptr.p->lcpLocstate != LcpLocRecord::SR_TUP_COMPLETED) {
    jam();
    if (cscLcpLocptr.p->lcpLocstate != LcpLocRecord::SR_TUP_STARTED) {
      jam();
      systemErrorLab(signal);
      return;
    }//if
    return;
  }//if
  cscLcpLocptr.i = cscLcpLocptr.p->nextLcpLoc;
  if (cscLcpLocptr.i != RNIL) {
    jam();
    goto CSC_TUP_DOWHILE;
  }//if
  lcpPtr.p->lcpState = LcpRecord::LCP_SR_COMPLETED;
}//Dblqh::checkSrCompleted()

/* ------------------------------------------------------------------------- */
/* ------       CLOSE A FILE DURING EXECUTION OF FRAGMENT LOG        ------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void Dblqh::closeFile(Signal* signal, LogFileRecordPtr clfLogFilePtr) 
{
  signal->theData[0] = clfLogFilePtr.p->fileRef;
  signal->theData[1] = cownref;
  signal->theData[2] = clfLogFilePtr.i;
  signal->theData[3] = ZCLOSE_NO_DELETE;
  sendSignal(NDBFS_REF, GSN_FSCLOSEREQ, signal, 4, JBA);
}//Dblqh::closeFile()


/* ---------------------------------------------------------------- */
/* ---------------- A LOG PAGE HAVE BEEN COMPLETED ---------------- */
/*                                                                  */
/*       SUBROUTINE SHORT NAME = CLP                                */
// Input Pointers:
// logFilePtr
// logPagePtr
// logPartPtr
// Defines lfoPtr
/* ---------------------------------------------------------------- */
void Dblqh::completedLogPage(Signal* signal, Uint32 clpType) 
{
  LogPageRecordPtr clpLogPagePtr;
  LogPageRecordPtr wlpLogPagePtr;
  UintR twlpNoPages;
  UintR twlpType;

  if (logFilePtr.p->firstFilledPage == RNIL) {
    jam();
    logFilePtr.p->firstFilledPage = logPagePtr.i;
  } else {
    jam();
    clpLogPagePtr.i = logFilePtr.p->lastFilledPage;
    ptrCheckGuard(clpLogPagePtr, clogPageFileSize, logPageRecord);
    clpLogPagePtr.p->logPageWord[ZNEXT_PAGE] = logPagePtr.i;
  }//if
  logFilePtr.p->lastFilledPage = logPagePtr.i;
  logPagePtr.p->logPageWord[ZNEXT_PAGE] = RNIL;
  logFilePtr.p->noLogpagesInBuffer = logFilePtr.p->noLogpagesInBuffer + 1;
  if (logFilePtr.p->noLogpagesInBuffer != ZMAX_PAGES_WRITTEN) {
    if (clpType != ZLAST_WRITE_IN_FILE) {
      if (clpType != ZENFORCE_WRITE) {
        jam();
        return;
      }//if
    }//if
  }//if
  twlpType = clpType;
/* ------------------------------------------------------------------------- */
/* ------               WRITE A SET OF LOG PAGES TO DISK             ------- */
/*                                                                           */
/*      SUBROUTINE SHORT NAME: WLP                                           */
/* ------------------------------------------------------------------------- */
  seizeLfo(signal);
  initLfo(signal);
  Uint32* dataPtr = &signal->theData[6];
  twlpNoPages = 0;
  wlpLogPagePtr.i = logFilePtr.p->firstFilledPage;
  do {
    dataPtr[twlpNoPages] = wlpLogPagePtr.i;
    twlpNoPages++;
    ptrCheckGuard(wlpLogPagePtr, clogPageFileSize, logPageRecord);

    // Calculate checksum for page
    wlpLogPagePtr.p->logPageWord[ZPOS_CHECKSUM] = calcPageCheckSum(wlpLogPagePtr);
    wlpLogPagePtr.i = wlpLogPagePtr.p->logPageWord[ZNEXT_PAGE];
  } while (wlpLogPagePtr.i != RNIL);
  ndbrequire(twlpNoPages < 9);
  dataPtr[twlpNoPages] = logFilePtr.p->filePosition;
/* -------------------------------------------------- */
/*       SET TIMER ON THIS LOG PART TO SIGNIFY THAT A */
/*       LOG RECORD HAS BEEN SENT AT THIS TIME.       */
/* -------------------------------------------------- */
  logPartPtr.p->logPartTimer = logPartPtr.p->logTimer;
  signal->theData[0] = logFilePtr.p->fileRef;
  signal->theData[1] = cownref;
  signal->theData[2] = lfoPtr.i;
  logFilePtr.p->logFilePagesToDiskWithoutSynch += twlpNoPages;
  if (twlpType == ZLAST_WRITE_IN_FILE) {
    jam();
    logFilePtr.p->logFilePagesToDiskWithoutSynch = 0;
    signal->theData[3] = ZLIST_OF_MEM_PAGES_SYNCH;
  } else if (logFilePtr.p->logFilePagesToDiskWithoutSynch >
             MAX_REDO_PAGES_WITHOUT_SYNCH) {
    jam();
    logFilePtr.p->logFilePagesToDiskWithoutSynch = 0;
    signal->theData[3] = ZLIST_OF_MEM_PAGES_SYNCH;
  } else {
    jam();
    signal->theData[3] = ZLIST_OF_MEM_PAGES;
  }//if
  signal->theData[4] = ZVAR_NO_LOG_PAGE_WORD;
  signal->theData[5] = twlpNoPages;
  sendSignal(NDBFS_REF, GSN_FSWRITEREQ, signal, 15, JBA);
  if (twlpType == ZNORMAL) {
    jam();
    lfoPtr.p->lfoState = LogFileOperationRecord::ACTIVE_WRITE_LOG;
  } else if (twlpType == ZLAST_WRITE_IN_FILE) {
    jam();
    lfoPtr.p->lfoState = LogFileOperationRecord::LAST_WRITE_IN_FILE;
  } else {
    ndbrequire(twlpType == ZENFORCE_WRITE);
    jam();
    lfoPtr.p->lfoState = LogFileOperationRecord::ACTIVE_WRITE_LOG;
  }//if
  /* ----------------------------------------------------------------------- */
  /* ------       MOVE PAGES FROM LOG FILE TO LFO RECORD             ------- */
  /*                                                                         */
  /* ----------------------------------------------------------------------- */
  /* -------------------------------------------------- */
  /*       MOVE PAGES TO LFO RECORD AND REMOVE THEM     */
  /*       FROM LOG FILE RECORD.                        */
  /* -------------------------------------------------- */
  lfoPtr.p->firstLfoPage = logFilePtr.p->firstFilledPage;
  logFilePtr.p->firstFilledPage = RNIL;
  logFilePtr.p->lastFilledPage = RNIL;
  logFilePtr.p->noLogpagesInBuffer = 0;

  lfoPtr.p->noPagesRw = twlpNoPages;
  lfoPtr.p->lfoPageNo = logFilePtr.p->filePosition;
  lfoPtr.p->lfoWordWritten = ZPAGE_SIZE - 1;
  logFilePtr.p->filePosition += twlpNoPages;
}//Dblqh::completedLogPage()

/* ---------------------------------------------------------------- */
/* ---------------- DELETE FRAGMENT RECORD ------------------------ */
/*                                                                  */
/*       SUBROUTINE SHORT NAME = DFR                                */
/* ---------------------------------------------------------------- */
void Dblqh::deleteFragrec(Uint32 fragId) 
{
  Uint32 indexFound= RNIL;
  fragptr.i = RNIL;
  for (Uint32 i = (MAX_FRAG_PER_NODE - 1); (Uint32)~i; i--) {
    jam();
    if (tabptr.p->fragid[i] == fragId) {
      fragptr.i = tabptr.p->fragrec[i];
      indexFound = i;
      break;
    }//if
  }//for
  if (fragptr.i != RNIL) {
    jam();
    ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
    tabptr.p->fragid[indexFound] = ZNIL;
    tabptr.p->fragrec[indexFound] = RNIL;
    releaseFragrec();
  }//if
}//Dblqh::deleteFragrec()

/* ------------------------------------------------------------------------- */
/* -------          FIND LOG FILE RECORD GIVEN FILE NUMBER           ------- */
/*                                                                           */
/*       INPUT:          TFLF_FILE_NO    THE FILE NUMBER                     */
/*                       FLF_LOG_PART_PTR THE LOG PART RECORD                */
/*       OUTPUT:         FLF_LOG_FILE_PTR THE FOUND LOG FILE RECORD          */
/*       SUBROUTINE SHORT NAME = FLF                                         */
/* ------------------------------------------------------------------------- */
void Dblqh::findLogfile(Signal* signal,
                        Uint32 fileNo,
                        LogPartRecordPtr flfLogPartPtr,
                        LogFileRecordPtr* parLogFilePtr) 
{
  LogFileRecordPtr locLogFilePtr;
  locLogFilePtr.i = flfLogPartPtr.p->firstLogfile;
  Uint32 loopCount = 0;
  while (true) {
    ptrCheckGuard(locLogFilePtr, clogFileFileSize, logFileRecord);
    if (locLogFilePtr.p->fileNo == fileNo) {
      jam();
      ndbrequire(loopCount == fileNo);
      parLogFilePtr->i = locLogFilePtr.i;
      parLogFilePtr->p = locLogFilePtr.p;
      return;
    }//if
    locLogFilePtr.i = locLogFilePtr.p->nextLogFile;
    loopCount++;
    ndbrequire(loopCount < flfLogPartPtr.p->noLogFiles);
  }//while
}//Dblqh::findLogfile()

/* ------------------------------------------------------------------------- */
/* ------     FIND PAGE REFERENCE IN MEMORY BUFFER AT LOG EXECUTION  ------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void Dblqh::findPageRef(Signal* signal, CommitLogRecord* commitLogRecord) 
{
  UintR tfprIndex;

  logPagePtr.i = RNIL;
  if (ERROR_INSERTED(5020)) {
    // Force system to read page from disk
    return;
  }
  pageRefPtr.i = logPartPtr.p->lastPageRef;
  do {
    ptrCheckGuard(pageRefPtr, cpageRefFileSize, pageRefRecord);
    if (commitLogRecord->fileNo == pageRefPtr.p->prFileNo) {
      if (commitLogRecord->startPageNo >= pageRefPtr.p->prPageNo) {
        if (commitLogRecord->startPageNo < (Uint16) (pageRefPtr.p->prPageNo + 8)) {
          jam();
          tfprIndex = commitLogRecord->startPageNo - pageRefPtr.p->prPageNo;
          logPagePtr.i = pageRefPtr.p->pageRef[tfprIndex];
          ptrCheckGuard(logPagePtr, clogPageFileSize, logPageRecord);
          return;
        }//if
      }//if
    }//if
    pageRefPtr.i = pageRefPtr.p->prPrev;
  } while (pageRefPtr.i != RNIL);
}//Dblqh::findPageRef()

/* ------------------------------------------------------------------------- */
/* ------         GET FIRST OPERATION QUEUED FOR LOGGING             ------- */
/*                                                                           */
/*      SUBROUTINE SHORT NAME = GFL                                          */
/* ------------------------------------------------------------------------- */
void Dblqh::getFirstInLogQueue(Signal* signal) 
{
  TcConnectionrecPtr gflTcConnectptr;
/* -------------------------------------------------- */
/*       GET THE FIRST FROM THE LOG QUEUE AND REMOVE  */
/*       IT FROM THE QUEUE.                           */
/* -------------------------------------------------- */
  gflTcConnectptr.i = logPartPtr.p->firstLogQueue;
  ptrCheckGuard(gflTcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
  logPartPtr.p->firstLogQueue = gflTcConnectptr.p->nextTcLogQueue;
  if (logPartPtr.p->firstLogQueue == RNIL) {
    jam();
    logPartPtr.p->lastLogQueue = RNIL;
  }//if
}//Dblqh::getFirstInLogQueue()

/* ---------------------------------------------------------------- */
/* ---------------- GET FRAGMENT RECORD --------------------------- */
/*       INPUT:          TFRAGID         FRAGMENT ID LOOKING FOR    */
/*                       TABPTR          TABLE ID                   */
/*       SUBROUTINE SHORT NAME = GFR                                */
/* ---------------------------------------------------------------- */
bool Dblqh::getFragmentrec(Signal* signal, Uint32 fragId) 
{
  for (Uint32 i = (MAX_FRAG_PER_NODE - 1); (UintR)~i; i--) {
    jam();
    if (tabptr.p->fragid[i] == fragId) {
      fragptr.i = tabptr.p->fragrec[i];
      ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
      return true;
    }//if
  }//for
  return false;
}//Dblqh::getFragmentrec()

/* ========================================================================= */
/* ======                      INITIATE FRAGMENT RECORD              ======= */
/*                                                                           */
/* ========================================================================= */
void Dblqh::initialiseAddfragrec(Signal* signal) 
{
  if (caddfragrecFileSize != 0) {
    for (addfragptr.i = 0; addfragptr.i < caddfragrecFileSize; addfragptr.i++) {
      ptrAss(addfragptr, addFragRecord);
      addfragptr.p->addfragStatus = AddFragRecord::FREE;
      addfragptr.p->nextAddfragrec = addfragptr.i + 1;
    }//for
    addfragptr.i = caddfragrecFileSize - 1;
    ptrAss(addfragptr, addFragRecord);
    addfragptr.p->nextAddfragrec = RNIL;
    cfirstfreeAddfragrec = 0;
  } else {
    jam();
    cfirstfreeAddfragrec = RNIL;
  }//if
}//Dblqh::initialiseAddfragrec()

/* ========================================================================= */
/* ======              INITIATE ATTRIBUTE IN AND OUT DATA BUFFER     ======= */
/*                                                                           */
/* ========================================================================= */
void Dblqh::initialiseAttrbuf(Signal* signal) 
{
  if (cattrinbufFileSize != 0) {
    for (attrinbufptr.i = 0; 
	 attrinbufptr.i < cattrinbufFileSize; 
	 attrinbufptr.i++) {
      refresh_watch_dog();
      ptrAss(attrinbufptr, attrbuf);
      attrinbufptr.p->attrbuf[ZINBUF_NEXT] = attrinbufptr.i + 1;
    }//for
                                                  /* NEXT ATTRINBUF */
    attrinbufptr.i = cattrinbufFileSize - 1;
    ptrAss(attrinbufptr, attrbuf);
    attrinbufptr.p->attrbuf[ZINBUF_NEXT] = RNIL;	/* NEXT ATTRINBUF */
    cfirstfreeAttrinbuf = 0;
  } else {
    jam();
    cfirstfreeAttrinbuf = RNIL;
  }//if
}//Dblqh::initialiseAttrbuf()

/* ========================================================================= */
/* ======                  INITIATE DATA BUFFER                      ======= */
/*                                                                           */
/* ========================================================================= */
void Dblqh::initialiseDatabuf(Signal* signal) 
{
  if (cdatabufFileSize != 0) {
    for (databufptr.i = 0; databufptr.i < cdatabufFileSize; databufptr.i++) {
      refresh_watch_dog();
      ptrAss(databufptr, databuf);
      databufptr.p->nextDatabuf = databufptr.i + 1;
    }//for
    databufptr.i = cdatabufFileSize - 1;
    ptrAss(databufptr, databuf);
    databufptr.p->nextDatabuf = RNIL;
    cfirstfreeDatabuf = 0;
  } else {
    jam();
    cfirstfreeDatabuf = RNIL;
  }//if
}//Dblqh::initialiseDatabuf()

/* ========================================================================= */
/* ======                INITIATE FRAGMENT RECORD                    ======= */
/*                                                                           */
/* ========================================================================= */
void Dblqh::initialiseFragrec(Signal* signal) 
{
  if (cfragrecFileSize != 0) {
    for (fragptr.i = 0; fragptr.i < cfragrecFileSize; fragptr.i++) {
      refresh_watch_dog();
      ptrAss(fragptr, fragrecord);
      fragptr.p->fragStatus = Fragrecord::FREE;
      fragptr.p->fragActiveStatus = ZFALSE;
      fragptr.p->execSrStatus = Fragrecord::IDLE;
      fragptr.p->srStatus = Fragrecord::SS_IDLE;
      fragptr.p->nextFrag = fragptr.i + 1;
    }//for
    fragptr.i = cfragrecFileSize - 1;
    ptrAss(fragptr, fragrecord);
    fragptr.p->nextFrag = RNIL;
    cfirstfreeFragrec = 0;
  } else {
    jam();
    cfirstfreeFragrec = RNIL;
  }//if
}//Dblqh::initialiseFragrec()

/* ========================================================================= */
/* ======                INITIATE FRAGMENT RECORD                    ======= */
/*                                                                           */
/* ========================================================================= */
void Dblqh::initialiseGcprec(Signal* signal) 
{
  UintR tigpIndex;

  if (cgcprecFileSize != 0) {
    for (gcpPtr.i = 0; gcpPtr.i < cgcprecFileSize; gcpPtr.i++) {
      ptrAss(gcpPtr, gcpRecord);
      for (tigpIndex = 0; tigpIndex <= 3; tigpIndex++) {
        gcpPtr.p->gcpLogPartState[tigpIndex] = ZIDLE;
        gcpPtr.p->gcpSyncReady[tigpIndex] = ZFALSE;
      }//for
    }//for
  }//if
}//Dblqh::initialiseGcprec()

/* ========================================================================= */
/* ======                INITIATE LCP RECORD                         ======= */
/*                                                                           */
/* ========================================================================= */
void Dblqh::initialiseLcpRec(Signal* signal) 
{
  if (clcpFileSize != 0) {
    for (lcpPtr.i = 0; lcpPtr.i < clcpFileSize; lcpPtr.i++) {
      ptrAss(lcpPtr, lcpRecord);
      lcpPtr.p->lcpState = LcpRecord::LCP_IDLE;
      lcpPtr.p->lcpQueued = false;
      lcpPtr.p->firstLcpLocAcc = RNIL;
      lcpPtr.p->firstLcpLocTup = RNIL;
      lcpPtr.p->reportEmpty = false;
      lcpPtr.p->lastFragmentFlag = false;
    }//for
  }//if
}//Dblqh::initialiseLcpRec()

/* ========================================================================= */
/* ======                INITIATE LCP LOCAL RECORD                   ======= */
/*                                                                           */
/* ========================================================================= */
void Dblqh::initialiseLcpLocrec(Signal* signal) 
{
  if (clcpLocrecFileSize != 0) {
    for (lcpLocptr.i = 0; lcpLocptr.i < clcpLocrecFileSize; lcpLocptr.i++) {
      ptrAss(lcpLocptr, lcpLocRecord);
      lcpLocptr.p->nextLcpLoc = lcpLocptr.i + 1;
      lcpLocptr.p->lcpLocstate = LcpLocRecord::IDLE;
      lcpLocptr.p->masterLcpRec = RNIL;
      lcpLocptr.p->waitingBlock = LcpLocRecord::NONE;
    }//for
    lcpLocptr.i = clcpLocrecFileSize - 1;
    ptrAss(lcpLocptr, lcpLocRecord);
    lcpLocptr.p->nextLcpLoc = RNIL;
    cfirstfreeLcpLoc = 0;
  } else {
    jam();
    cfirstfreeLcpLoc = RNIL;
  }//if
}//Dblqh::initialiseLcpLocrec()

/* ========================================================================= */
/* ======         INITIATE LOG FILE OPERATION RECORD                 ======= */
/*                                                                           */
/* ========================================================================= */
void Dblqh::initialiseLfo(Signal* signal) 
{
  if (clfoFileSize != 0) {
    for (lfoPtr.i = 0; lfoPtr.i < clfoFileSize; lfoPtr.i++) {
      ptrAss(lfoPtr, logFileOperationRecord);
      lfoPtr.p->lfoState = LogFileOperationRecord::IDLE;
      lfoPtr.p->lfoTimer = 0;
      lfoPtr.p->nextLfo = lfoPtr.i + 1;
    }//for
    lfoPtr.i = clfoFileSize - 1;
    ptrAss(lfoPtr, logFileOperationRecord);
    lfoPtr.p->nextLfo = RNIL;
    cfirstfreeLfo = 0;
  } else {
    jam();
    cfirstfreeLfo = RNIL;
  }//if
}//Dblqh::initialiseLfo()

/* ========================================================================= */
/* ======                 INITIATE LOG FILE RECORD                   ======= */
/*                                                                           */
/* ========================================================================= */
void Dblqh::initialiseLogFile(Signal* signal) 
{
  if (clogFileFileSize != 0) {
    for (logFilePtr.i = 0; logFilePtr.i < clogFileFileSize; logFilePtr.i++) {
      ptrAss(logFilePtr, logFileRecord);
      logFilePtr.p->nextLogFile = logFilePtr.i + 1;
      logFilePtr.p->logFileStatus = LogFileRecord::LFS_IDLE;
    }//for
    logFilePtr.i = clogFileFileSize - 1;
    ptrAss(logFilePtr, logFileRecord);
    logFilePtr.p->nextLogFile = RNIL;
    cfirstfreeLogFile = 0;
  } else {
    jam();
    cfirstfreeLogFile = RNIL;
  }//if
}//Dblqh::initialiseLogFile()

/* ========================================================================= */
/* ======                  INITIATE LOG PAGES                        ======= */
/*                                                                           */
/* ========================================================================= */
void Dblqh::initialiseLogPage(Signal* signal) 
{
  if (clogPageFileSize != 0) {
    for (logPagePtr.i = 0; logPagePtr.i < clogPageFileSize; logPagePtr.i++) {
      refresh_watch_dog();
      ptrAss(logPagePtr, logPageRecord);
      logPagePtr.p->logPageWord[ZNEXT_PAGE] = logPagePtr.i + 1;
    }//for
    logPagePtr.i = clogPageFileSize - 1;
    ptrAss(logPagePtr, logPageRecord);
    logPagePtr.p->logPageWord[ZNEXT_PAGE] = RNIL;
    cfirstfreeLogPage = 0;
  } else {
    jam();
    cfirstfreeLogPage = RNIL;
  }//if
  cnoOfLogPages = clogPageFileSize;
}//Dblqh::initialiseLogPage()

/* ========================================================================= 
 * ======                       INITIATE LOG PART RECORD             =======
 *
 * ========================================================================= */
void Dblqh::initialiseLogPart(Signal* signal) 
{
  for (logPartPtr.i = 0; logPartPtr.i <= 3; logPartPtr.i++) {
    ptrAss(logPartPtr, logPartRecord);
    logPartPtr.p->waitWriteGciLog = LogPartRecord::WWGL_FALSE;
    logPartPtr.p->LogLqhKeyReqSent = ZFALSE;
    logPartPtr.p->logPartNewestCompletedGCI = (UintR)-1;
  }//for
}//Dblqh::initialiseLogPart()

void Dblqh::initialisePageRef(Signal* signal) 
{
  if (cpageRefFileSize != 0) {
    for (pageRefPtr.i = 0; 
	 pageRefPtr.i < cpageRefFileSize; 
	 pageRefPtr.i++) {
      ptrAss(pageRefPtr, pageRefRecord);
      pageRefPtr.p->prNext = pageRefPtr.i + 1;
    }//for
    pageRefPtr.i = cpageRefFileSize - 1;
    ptrAss(pageRefPtr, pageRefRecord);
    pageRefPtr.p->prNext = RNIL;
    cfirstfreePageRef = 0;
  } else {
    jam();
    cfirstfreePageRef = RNIL;
  }//if
}//Dblqh::initialisePageRef()

/* ========================================================================== 
 * =======                        INITIATE RECORDS                    ======= 
 * 
 *       TAKES CARE OF INITIATION OF ALL RECORDS IN THIS BLOCK.
 * ========================================================================= */
void Dblqh::initialiseRecordsLab(Signal* signal, Uint32 data,
				 Uint32 retRef, Uint32 retData) 
{
  Uint32 i;
  switch (data) {
  case 0:
    jam();
    for (i = 0; i < MAX_NDB_NODES; i++) {
      cnodeSrState[i] = ZSTART_SR;
      cnodeExecSrState[i] = ZSTART_SR;
    }//for
    for (i = 0; i < 1024; i++) {
      ctransidHash[i] = RNIL;
    }//for
    for (i = 0; i < 4; i++) {
      cactiveCopy[i] = RNIL;
    }//for
    cnoActiveCopy = 0;
    cCounterAccCommitBlocked = 0;
    cCounterTupCommitBlocked = 0;
    caccCommitBlocked = false;
    ctupCommitBlocked = false;
    cCommitBlocked = false;
    ccurrentGcprec = RNIL;
    caddNodeState = ZFALSE;
    cstartRecReq = ZFALSE;
    cnewestGci = (UintR)-1;
    cnewestCompletedGci = (UintR)-1;
    crestartOldestGci = 0;
    crestartNewestGci = 0;
    cfirstWaitFragSr = RNIL;
    cfirstCompletedFragSr = RNIL;
    csrPhaseStarted = ZSR_NO_PHASE_STARTED;
    csrPhasesCompleted = 0;
    cmasterDihBlockref = 0;
    cnoFragmentsExecSr = 0;
    clcpCompletedState = LCP_IDLE;
    csrExecUndoLogState = EULS_IDLE;
    c_lcpId = 0;
    cnoOfFragsCheckpointed = 0;
    break;
  case 1:
    jam();
    initialiseAddfragrec(signal);
    break;
  case 2:
    jam();
    initialiseAttrbuf(signal);
    break;
  case 3:
    jam();
    initialiseDatabuf(signal);
    break;
  case 4:
    jam();
    initialiseFragrec(signal);
    break;
  case 5:
    jam();
    initialiseGcprec(signal);
    initialiseLcpRec(signal);
    initialiseLcpLocrec(signal);
    break;
  case 6:
    jam();
    initialiseLogPage(signal);
    break;
  case 7:
    jam();
    initialiseLfo(signal);
    break;
  case 8:
    jam();
    initialiseLogFile(signal);
    initialiseLogPart(signal);
    break;
  case 9:
    jam();
    initialisePageRef(signal);
    break;
  case 10:
    jam();
    initialiseScanrec(signal);
    break;
  case 11:
    jam();
    initialiseTabrec(signal);
    break;
  case 12:
    jam();
    initialiseTcNodeFailRec(signal);
    initialiseTcrec(signal);
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
    ndbrequire(false);
    break;
  }//switch

  signal->theData[0] = ZINITIALISE_RECORDS;
  signal->theData[1] = data + 1;
  signal->theData[2] = 0;
  signal->theData[3] = retRef;
  signal->theData[4] = retData;
  sendSignal(DBLQH_REF, GSN_CONTINUEB, signal, 5, JBB);

  return;
}//Dblqh::initialiseRecordsLab()

/* ========================================================================== 
 * =======                      INITIATE TC CONNECTION RECORD         ======= 
 *
 * ========================================================================= */
void Dblqh::initialiseScanrec(Signal* signal) 
{
  ndbrequire(cscanrecFileSize > 1);
  DLList<ScanRecord> tmp(c_scanRecordPool);
  while (tmp.seize(scanptr)){
    //new (scanptr.p) ScanRecord();
    refresh_watch_dog();
    scanptr.p->scanType = ScanRecord::ST_IDLE;
    scanptr.p->scanState = ScanRecord::SCAN_FREE;
    scanptr.p->scanTcWaiting = ZFALSE;
    scanptr.p->nextHash = RNIL;
    scanptr.p->prevHash = RNIL;
    scanptr.p->scan_acc_index= 0;
    scanptr.p->scan_acc_attr_recs= 0;
  }
  tmp.release();
}//Dblqh::initialiseScanrec()

/* ========================================================================== 
 * =======                      INITIATE TABLE RECORD                 ======= 
 * 
 * ========================================================================= */
void Dblqh::initialiseTabrec(Signal* signal) 
{
  if (ctabrecFileSize != 0) {
    for (tabptr.i = 0; tabptr.i < ctabrecFileSize; tabptr.i++) {
      refresh_watch_dog();
      ptrAss(tabptr, tablerec);
      tabptr.p->tableStatus = Tablerec::NOT_DEFINED;
      tabptr.p->usageCount = 0;
      for (Uint32 i = 0; i <= (MAX_FRAG_PER_NODE - 1); i++) {
        tabptr.p->fragid[i] = ZNIL;
        tabptr.p->fragrec[i] = RNIL;
      }//for
    }//for
  }//if
}//Dblqh::initialiseTabrec()

/* ========================================================================== 
 * =======                      INITIATE TC CONNECTION RECORD         ======= 
 * 
 * ========================================================================= */
void Dblqh::initialiseTcrec(Signal* signal) 
{
  if (ctcConnectrecFileSize != 0) {
    for (tcConnectptr.i = 0; 
	 tcConnectptr.i < ctcConnectrecFileSize; 
	 tcConnectptr.i++) {
      refresh_watch_dog();
      ptrAss(tcConnectptr, tcConnectionrec);
      tcConnectptr.p->transactionState = TcConnectionrec::TC_NOT_CONNECTED;
      tcConnectptr.p->tcScanRec = RNIL;
      tcConnectptr.p->logWriteState = TcConnectionrec::NOT_STARTED;
      tcConnectptr.p->firstAttrinbuf = RNIL;
      tcConnectptr.p->lastAttrinbuf = RNIL;
      tcConnectptr.p->firstTupkeybuf = RNIL;
      tcConnectptr.p->lastTupkeybuf = RNIL;
      tcConnectptr.p->tcTimer = 0;
      tcConnectptr.p->nextTcConnectrec = tcConnectptr.i + 1;
    }//for
    tcConnectptr.i = ctcConnectrecFileSize - 1;
    ptrAss(tcConnectptr, tcConnectionrec);
    tcConnectptr.p->nextTcConnectrec = RNIL;
    cfirstfreeTcConrec = 0;
  } else {
    jam();
    cfirstfreeTcConrec = RNIL;
  }//if
}//Dblqh::initialiseTcrec()

/* ========================================================================== 
 * =======                      INITIATE TC CONNECTION RECORD         =======
 * 
 * ========================================================================= */
void Dblqh::initialiseTcNodeFailRec(Signal* signal) 
{
  if (ctcNodeFailrecFileSize != 0) {
    for (tcNodeFailptr.i = 0; 
	 tcNodeFailptr.i < ctcNodeFailrecFileSize; 
	 tcNodeFailptr.i++) {
      ptrAss(tcNodeFailptr, tcNodeFailRecord);
      tcNodeFailptr.p->tcFailStatus = TcNodeFailRecord::TC_STATE_FALSE;
    }//for
  }//if
}//Dblqh::initialiseTcNodeFailRec()

/* ==========================================================================
 * =======              INITIATE FRAGMENT RECORD                      ======= 
 *
 *       SUBROUTINE SHORT NAME = IF
 * ========================================================================= */
void Dblqh::initFragrec(Signal* signal,
                        Uint32 tableId,
                        Uint32 fragId,
                        Uint32 copyType) 
{
  new (fragptr.p) Fragrecord();
  fragptr.p->m_scanNumberMask.set(); // All is free
  fragptr.p->accBlockref = caccBlockref;
  fragptr.p->accBlockedList = RNIL;
  fragptr.p->activeList = RNIL;
  fragptr.p->firstWaitQueue = RNIL;
  fragptr.p->lastWaitQueue = RNIL;
  fragptr.p->fragStatus = Fragrecord::DEFINED;
  fragptr.p->fragCopy = copyType;
  fragptr.p->tupBlockref = ctupBlockref;
  fragptr.p->tuxBlockref = ctuxBlockref;
  fragptr.p->lcpRef = RNIL;
  fragptr.p->logFlag = Fragrecord::STATE_TRUE;
  fragptr.p->lcpFlag = Fragrecord::LCP_STATE_TRUE;
  for (Uint32 i = 0; i < MAX_LCP_STORED; i++) {
    fragptr.p->lcpId[i] = 0;
  }//for
  fragptr.p->maxGciCompletedInLcp = 0;
  fragptr.p->maxGciInLcp = 0;
  fragptr.p->copyFragState = ZIDLE;
  fragptr.p->nextFrag = RNIL;
  fragptr.p->newestGci = cnewestGci;
  fragptr.p->nextLcp = 0;
  fragptr.p->tabRef = tableId;
  fragptr.p->fragId = fragId;
  fragptr.p->srStatus = Fragrecord::SS_IDLE;
  fragptr.p->execSrStatus = Fragrecord::IDLE;
  fragptr.p->execSrNoReplicas = 0;
  fragptr.p->fragDistributionKey = 0;
  fragptr.p->activeTcCounter = 0;
  fragptr.p->tableFragptr = RNIL;
}//Dblqh::initFragrec()

/* ========================================================================== 
 * =======       INITIATE FRAGMENT RECORD FOR SYSTEM RESTART          ======= 
 *
 *       SUBROUTINE SHORT NAME = IFS
 * ========================================================================= */
void Dblqh::initFragrecSr(Signal* signal) 
{
  const StartFragReq * const startFragReq = (StartFragReq *)&signal->theData[0];
  Uint32 lcpNo = startFragReq->lcpNo;
  Uint32 noOfLogNodes = startFragReq->noOfLogNodes;
  ndbrequire(noOfLogNodes <= 4);
  fragptr.p->fragStatus = Fragrecord::CRASH_RECOVERING;
  fragptr.p->srBlockref = startFragReq->userRef;
  fragptr.p->srUserptr = startFragReq->userPtr;
  fragptr.p->srChkpnr = lcpNo;
  if (lcpNo == (MAX_LCP_STORED - 1)) {
    jam();
    fragptr.p->lcpId[lcpNo] = startFragReq->lcpId;
    fragptr.p->nextLcp = 0;
  } else if (lcpNo < (MAX_LCP_STORED - 1)) {
    jam();
    fragptr.p->lcpId[lcpNo] = startFragReq->lcpId;
    fragptr.p->nextLcp = lcpNo + 1;
  } else {
    ndbrequire(lcpNo == ZNIL);
    jam();
    fragptr.p->nextLcp = 0;
  }//if
  fragptr.p->srNoLognodes = noOfLogNodes;
  fragptr.p->logFlag = Fragrecord::STATE_FALSE;
  fragptr.p->srStatus = Fragrecord::SS_IDLE;
  if (noOfLogNodes > 0) {
    jam();
    for (Uint32 i = 0; i < noOfLogNodes; i++) {
      jam();
      fragptr.p->srStartGci[i] = startFragReq->startGci[i];
      fragptr.p->srLastGci[i] = startFragReq->lastGci[i];
      fragptr.p->srLqhLognode[i] = startFragReq->lqhLogNode[i];
    }//for
    fragptr.p->newestGci = startFragReq->lastGci[noOfLogNodes - 1];
  } else {
    fragptr.p->newestGci = cnewestGci;
  }//if
}//Dblqh::initFragrecSr()

/* ========================================================================== 
 * =======       INITIATE INFORMATION ABOUT GLOBAL CHECKPOINTS        ======= 
 *               IN LOG FILE RECORDS
 *
 *       INPUT:     LOG_FILE_PTR            CURRENT LOG FILE 
 *                  TNO_FD_DESCRIPTORS      THE NUMBER OF FILE DESCRIPTORS
 *                                          TO READ FROM THE LOG PAGE
 *                  LOG_PAGE_PTR            PAGE ZERO IN LOG FILE
 *       SUBROUTINE SHORT NAME = IGL
 * ========================================================================= */
void Dblqh::initGciInLogFileRec(Signal* signal, Uint32 noFdDescriptors) 
{
  LogFileRecordPtr iglLogFilePtr;
  UintR tiglLoop;
  UintR tiglIndex;

  tiglLoop = 0;
  iglLogFilePtr.i = logFilePtr.i;
  iglLogFilePtr.p = logFilePtr.p;
IGL_LOOP:
  for (tiglIndex = 0; tiglIndex <= ZNO_MBYTES_IN_FILE - 1; tiglIndex++) {
    arrGuard(((ZPAGE_HEADER_SIZE + ZFD_HEADER_SIZE) + 
	      (tiglLoop * ZFD_PART_SIZE)) + tiglIndex, ZPAGE_SIZE);
    iglLogFilePtr.p->logMaxGciCompleted[tiglIndex] = 
      logPagePtr.p->logPageWord[((ZPAGE_HEADER_SIZE + ZFD_HEADER_SIZE) + 
				 (tiglLoop * ZFD_PART_SIZE)) + tiglIndex];
    arrGuard((((ZPAGE_HEADER_SIZE + ZFD_HEADER_SIZE) + ZNO_MBYTES_IN_FILE) + 
	      (tiglLoop * ZFD_PART_SIZE)) + tiglIndex, ZPAGE_SIZE);
    iglLogFilePtr.p->logMaxGciStarted[tiglIndex] = 
      logPagePtr.p->logPageWord[(((ZPAGE_HEADER_SIZE + ZFD_HEADER_SIZE) + 
				  ZNO_MBYTES_IN_FILE) + 
				 (tiglLoop * ZFD_PART_SIZE)) + tiglIndex];
    arrGuard((((ZPAGE_HEADER_SIZE + ZFD_HEADER_SIZE) + 
	       (2 * ZNO_MBYTES_IN_FILE)) + (tiglLoop * ZFD_PART_SIZE)) + 
	     tiglIndex, ZPAGE_SIZE);
    iglLogFilePtr.p->logLastPrepRef[tiglIndex] = 
      logPagePtr.p->logPageWord[(((ZPAGE_HEADER_SIZE + ZFD_HEADER_SIZE) + 
				  (2 * ZNO_MBYTES_IN_FILE)) + 
				 (tiglLoop * ZFD_PART_SIZE)) + tiglIndex];
  }//for
  tiglLoop = tiglLoop + 1;
  if (tiglLoop < noFdDescriptors) {
    jam();
    iglLogFilePtr.i = iglLogFilePtr.p->prevLogFile;
    ptrCheckGuard(iglLogFilePtr, clogFileFileSize, logFileRecord);
    goto IGL_LOOP;
  }//if
}//Dblqh::initGciInLogFileRec()

/* ========================================================================== 
 * =======        INITIATE LCP RECORD WHEN USED FOR SYSTEM RESTART    ======= 
 *                                                                 
 *       SUBROUTINE SHORT NAME = ILS            
 * ========================================================================= */
void Dblqh::initLcpSr(Signal* signal,
                      Uint32 lcpNo,
                      Uint32 lcpId,
                      Uint32 tableId,
                      Uint32 fragId,
                      Uint32 fragPtr) 
{
  lcpPtr.p->lcpQueued = false;
  lcpPtr.p->currentFragment.fragPtrI = fragPtr;
  lcpPtr.p->currentFragment.lcpFragOrd.lcpNo = lcpNo;
  lcpPtr.p->currentFragment.lcpFragOrd.lcpId = lcpId;
  lcpPtr.p->currentFragment.lcpFragOrd.tableId = tableId;
  lcpPtr.p->currentFragment.lcpFragOrd.fragmentId = fragId;
  lcpPtr.p->lcpState = LcpRecord::LCP_SR_WAIT_FRAGID;
  lcpPtr.p->firstLcpLocAcc = RNIL;
  lcpPtr.p->firstLcpLocTup = RNIL;
  lcpPtr.p->lcpAccptr = RNIL;
}//Dblqh::initLcpSr()

/* ========================================================================== 
 * =======              INITIATE LOG PART                             ======= 
 *                             
 * ========================================================================= */
void Dblqh::initLogpart(Signal* signal) 
{
  logPartPtr.p->execSrLogPage = RNIL;
  logPartPtr.p->execSrLogPageIndex = ZNIL;
  logPartPtr.p->execSrExecuteIndex = 0;
  logPartPtr.p->noLogFiles = cnoLogFiles;
  logPartPtr.p->logLap = 0;
  logPartPtr.p->logTailFileNo = 0;
  logPartPtr.p->logTailMbyte = 0;
  logPartPtr.p->lastMbyte = ZNIL;
  logPartPtr.p->logPartState = LogPartRecord::SR_FIRST_PHASE;
  logPartPtr.p->logExecState = LogPartRecord::LES_IDLE;
  logPartPtr.p->firstLogTcrec = RNIL;
  logPartPtr.p->lastLogTcrec = RNIL;
  logPartPtr.p->firstLogQueue = RNIL;
  logPartPtr.p->lastLogQueue = RNIL;
  logPartPtr.p->gcprec = RNIL;
  logPartPtr.p->firstPageRef = RNIL;
  logPartPtr.p->lastPageRef = RNIL;
  logPartPtr.p->headFileNo = ZNIL;
  logPartPtr.p->headPageNo = ZNIL;
  logPartPtr.p->headPageIndex = ZNIL;
}//Dblqh::initLogpart()

/* ========================================================================== 
 * =======              INITIATE LOG POINTERS                         ======= 
 *
 * ========================================================================= */
void Dblqh::initLogPointers(Signal* signal) 
{
  logPartPtr.i = tcConnectptr.p->hashValue & 3;
  ptrCheckGuard(logPartPtr, clogPartFileSize, logPartRecord);
  logFilePtr.i = logPartPtr.p->currentLogfile;
  ptrCheckGuard(logFilePtr, clogFileFileSize, logFileRecord);
  logPagePtr.i = logFilePtr.p->currentLogpage;
  ptrCheckGuard(logPagePtr, clogPageFileSize, logPageRecord);
}//Dblqh::initLogPointers()

/* ------------------------------------------------------------------------- */
/* -------    INIT REQUEST INFO BEFORE EXECUTING A LOG RECORD        ------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void Dblqh::initReqinfoExecSr(Signal* signal) 
{
  UintR Treqinfo = 0;
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  LqhKeyReq::setKeyLen(Treqinfo, regTcPtr->primKeyLen);
/* ------------------------------------------------------------------------- */
/* NUMBER OF BACKUPS AND STANDBYS ARE ZERO AND NEED NOT BE SET.              */
/* REPLICA TYPE IS CLEARED BY SEND_LQHKEYREQ.                                */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/*       SET LAST REPLICA NUMBER TO ZERO (BIT 10-11)                         */
/* ------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------- */
/*       SET DIRTY FLAG                                                      */
/* ------------------------------------------------------------------------- */
  LqhKeyReq::setDirtyFlag(Treqinfo, 1);
/* ------------------------------------------------------------------------- */
/*       SET SIMPLE TRANSACTION                                              */
/* ------------------------------------------------------------------------- */
  LqhKeyReq::setSimpleFlag(Treqinfo, 1);
/* ------------------------------------------------------------------------- */
/* SET OPERATION TYPE AND LOCK MODE (NEVER READ OPERATION OR SCAN IN LOG)    */
/* ------------------------------------------------------------------------- */
  LqhKeyReq::setLockType(Treqinfo, regTcPtr->operation);
  LqhKeyReq::setOperation(Treqinfo, regTcPtr->operation);
  regTcPtr->reqinfo = Treqinfo;
/* ------------------------------------------------------------------------ */
/* NO OF BACKUP IS SET TO ONE AND NUMBER OF STANDBY NODES IS SET TO ZERO.   */
/* THUS THE RECEIVING NODE WILL EXPECT THAT IT IS THE LAST NODE AND WILL    */
/* SEND COMPLETED AS THE RESPONSE SIGNAL SINCE DIRTY_OP BIT IS SET.         */
/* ------------------------------------------------------------------------ */
/* ------------------------------------------------------------------------- */
/*       SET REPLICA TYPE TO PRIMARY AND NUMBER OF REPLICA TO ONE            */
/* ------------------------------------------------------------------------- */
  regTcPtr->lastReplicaNo = 0;
  regTcPtr->apiVersionNo = 0;
  regTcPtr->nextSeqNoReplica = 0;
  regTcPtr->opExec = 0;
  regTcPtr->storedProcId = ZNIL;
  regTcPtr->readlenAi = 0;
  regTcPtr->nodeAfterNext[0] = ZNIL;
  regTcPtr->nodeAfterNext[1] = ZNIL;
  regTcPtr->dirtyOp = ZFALSE;
  regTcPtr->tcBlockref = cownref;
}//Dblqh::initReqinfoExecSr()

/* -------------------------------------------------------------------------- 
 * -------               INSERT FRAGMENT                              ------- 
 *
 * ------------------------------------------------------------------------- */
bool Dblqh::insertFragrec(Signal* signal, Uint32 fragId) 
{
  terrorCode = ZOK;
  if (cfirstfreeFragrec == RNIL) {
    jam();
    terrorCode = ZNO_FREE_FRAGMENTREC;
    return false;
  }//if
  seizeFragmentrec(signal);
  for (Uint32 i = (MAX_FRAG_PER_NODE - 1); (Uint32)~i; i--) {
    jam();
    if (tabptr.p->fragid[i] == ZNIL) {
      jam();
      tabptr.p->fragid[i] = fragId;
      tabptr.p->fragrec[i] = fragptr.i;
      return true;
    }//if
  }//for
  terrorCode = ZTOO_MANY_FRAGMENTS;
  return false;
}//Dblqh::insertFragrec()

/* --------------------------------------------------------------------------
 * -------               LINK OPERATION IN ACTIVE LIST ON FRAGMENT    ------- 
 * 
 *       SUBROUTINE SHORT NAME: LFQ
// Input Pointers:
// tcConnectptr
// fragptr
* ------------------------------------------------------------------------- */
void Dblqh::linkFragQueue(Signal* signal) 
{
  TcConnectionrecPtr lfqTcConnectptr;
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  Fragrecord * const regFragPtr = fragptr.p;
  Uint32 tcIndex = tcConnectptr.i;

  lfqTcConnectptr.i = regFragPtr->lastWaitQueue;
  regTcPtr->nextTc = RNIL;
  regFragPtr->lastWaitQueue = tcIndex;
  regTcPtr->prevTc = lfqTcConnectptr.i;
  ndbrequire(regTcPtr->listState == TcConnectionrec::NOT_IN_LIST);
  regTcPtr->listState = TcConnectionrec::WAIT_QUEUE_LIST;
  if (lfqTcConnectptr.i != RNIL) {
    jam();
    ptrCheckGuard(lfqTcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
    lfqTcConnectptr.p->nextTc = tcIndex;
  } else {
    regFragPtr->firstWaitQueue = tcIndex;
  }//if
  return;
}//Dblqh::linkFragQueue()

/* ------------------------------------------------------------------------- 
 * -------               LINK OPERATION INTO WAITING FOR LOGGING     ------- 
 *                                             
 *       SUBROUTINE SHORT NAME = LWL
// Input Pointers:
// tcConnectptr
// logPartPtr
 * ------------------------------------------------------------------------- */
void Dblqh::linkWaitLog(Signal* signal, LogPartRecordPtr regLogPartPtr) 
{
  TcConnectionrecPtr lwlTcConnectptr;

/* -------------------------------------------------- */
/*       LINK ACTIVE OPERATION INTO QUEUE WAITING FOR */
/*       ACCESS TO THE LOG PART.                      */
/* -------------------------------------------------- */
  lwlTcConnectptr.i = regLogPartPtr.p->lastLogQueue;
  if (lwlTcConnectptr.i == RNIL) {
    jam();
    regLogPartPtr.p->firstLogQueue = tcConnectptr.i;
  } else {
    jam();
    ptrCheckGuard(lwlTcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
    lwlTcConnectptr.p->nextTcLogQueue = tcConnectptr.i;
  }//if
  regLogPartPtr.p->lastLogQueue = tcConnectptr.i;
  tcConnectptr.p->nextTcLogQueue = RNIL;
  if (regLogPartPtr.p->LogLqhKeyReqSent == ZFALSE) {
    jam();
    regLogPartPtr.p->LogLqhKeyReqSent = ZTRUE;
    signal->theData[0] = ZLOG_LQHKEYREQ;
    signal->theData[1] = regLogPartPtr.i;
    sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
  }//if
}//Dblqh::linkWaitLog()

/* --------------------------------------------------------------------------
 * -------          START THE NEXT OPERATION ON THIS LOG PART IF ANY  ------- 
 * -------               OPERATIONS ARE QUEUED.                       -------
 *
 *       SUBROUTINE SHORT NAME = LNS
// Input Pointers:
// tcConnectptr
// logPartPtr
 * ------------------------------------------------------------------------- */
void Dblqh::logNextStart(Signal* signal) 
{
  LogPartRecordPtr lnsLogPartPtr;
  UintR tlnsStillWaiting;
  LogPartRecord * const regLogPartPtr = logPartPtr.p;

  if ((regLogPartPtr->firstLogQueue == RNIL) &&
      (regLogPartPtr->logPartState == LogPartRecord::ACTIVE) &&
      (regLogPartPtr->waitWriteGciLog != LogPartRecord::WWGL_TRUE)) {
// --------------------------------------------------------------------------
// Optimised route for the common case
// -------------------------------------------------------------------------- 
    regLogPartPtr->logPartState = LogPartRecord::IDLE;
    return;
  }//if
  if (regLogPartPtr->firstLogQueue != RNIL) {
    jam();
    if (regLogPartPtr->LogLqhKeyReqSent == ZFALSE) {
      jam();
      regLogPartPtr->LogLqhKeyReqSent = ZTRUE;
      signal->theData[0] = ZLOG_LQHKEYREQ;
      signal->theData[1] = logPartPtr.i;
      sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
    }//if
  } else {
    if (regLogPartPtr->logPartState == LogPartRecord::ACTIVE) {
      jam();
      regLogPartPtr->logPartState = LogPartRecord::IDLE;
    } else {
      jam();
    }//if
  }//if
  if (regLogPartPtr->waitWriteGciLog != LogPartRecord::WWGL_TRUE) {
    jam();
    return;
  } else {
    jam();
/* -------------------------------------------------------------------------- 
 *   A COMPLETE GCI LOG RECORD IS WAITING TO BE WRITTEN. WE GIVE THIS HIGHEST
 *   PRIORITY AND WRITE IT IMMEDIATELY. AFTER WRITING IT WE CHECK IF ANY MORE
 *   LOG PARTS ARE WAITING. IF NOT WE SEND A SIGNAL THAT INITIALISES THE GCP 
 *   RECORD TO WAIT UNTIL ALL COMPLETE GCI LOG RECORDS HAVE REACHED TO DISK.
 * -------------------------------------------------------------------------- */
    writeCompletedGciLog(signal);
    logPartPtr.p->waitWriteGciLog = LogPartRecord::WWGL_FALSE;
    tlnsStillWaiting = ZFALSE;
    for (lnsLogPartPtr.i = 0; lnsLogPartPtr.i < 4; lnsLogPartPtr.i++) {
      jam();
      ptrAss(lnsLogPartPtr, logPartRecord);
      if (lnsLogPartPtr.p->waitWriteGciLog == LogPartRecord::WWGL_TRUE) {
        jam();
        tlnsStillWaiting = ZTRUE;
      }//if
    }//for
    if (tlnsStillWaiting == ZFALSE) {
      jam();
      signal->theData[0] = ZINIT_GCP_REC;
      sendSignal(cownref, GSN_CONTINUEB, signal, 1, JBB);
    }//if
  }//if
}//Dblqh::logNextStart()

/* -------------------------------------------------------------------------- 
 * -------       MOVE PAGES FROM LFO RECORD TO PAGE REFERENCE RECORD  ------- 
 *               WILL ALWAYS MOVE 8 PAGES TO A PAGE REFERENCE RECORD.
 *
 *       SUBROUTINE SHORT NAME = MPR 
 * ------------------------------------------------------------------------- */
void Dblqh::moveToPageRef(Signal* signal) 
{
  LogPageRecordPtr mprLogPagePtr;
  PageRefRecordPtr mprPageRefPtr;
  UintR tmprIndex;

/* -------------------------------------------------------------------------- 
 * -------       INSERT PAGE REFERENCE RECORD                         ------- 
 *
 *       INPUT:  LFO_PTR         LOG FILE OPERATION RECORD
 *               LOG_PART_PTR    LOG PART RECORD
 *               PAGE_REF_PTR    THE PAGE REFERENCE RECORD TO BE INSERTED.
 * ------------------------------------------------------------------------- */
  PageRefRecordPtr iprPageRefPtr;

  if ((logPartPtr.p->mmBufferSize + 8) >= ZMAX_MM_BUFFER_SIZE) {
    jam();
    pageRefPtr.i = logPartPtr.p->firstPageRef;
    ptrCheckGuard(pageRefPtr, cpageRefFileSize, pageRefRecord);
    releasePrPages(signal);
    removePageRef(signal);
  } else {
    jam();
    logPartPtr.p->mmBufferSize = logPartPtr.p->mmBufferSize + 8;
  }//if
  seizePageRef(signal);
  if (logPartPtr.p->firstPageRef == RNIL) {
    jam();
    logPartPtr.p->firstPageRef = pageRefPtr.i;
  } else {
    jam();
    iprPageRefPtr.i = logPartPtr.p->lastPageRef;
    ptrCheckGuard(iprPageRefPtr, cpageRefFileSize, pageRefRecord);
    iprPageRefPtr.p->prNext = pageRefPtr.i;
  }//if
  pageRefPtr.p->prPrev = logPartPtr.p->lastPageRef;
  logPartPtr.p->lastPageRef = pageRefPtr.i;

  pageRefPtr.p->prFileNo = logFilePtr.p->fileNo;
  pageRefPtr.p->prPageNo = lfoPtr.p->lfoPageNo;
  tmprIndex = 0;
  mprLogPagePtr.i = lfoPtr.p->firstLfoPage;
MPR_LOOP:
  arrGuard(tmprIndex, 8);
  pageRefPtr.p->pageRef[tmprIndex] = mprLogPagePtr.i;
  tmprIndex = tmprIndex + 1;
  ptrCheckGuard(mprLogPagePtr, clogPageFileSize, logPageRecord);
  mprLogPagePtr.i = mprLogPagePtr.p->logPageWord[ZNEXT_PAGE];
  if (mprLogPagePtr.i != RNIL) {
    jam();
    goto MPR_LOOP;
  }//if
  mprPageRefPtr.i = pageRefPtr.p->prPrev;
  if (mprPageRefPtr.i != RNIL) {
    jam();
    ptrCheckGuard(mprPageRefPtr, cpageRefFileSize, pageRefRecord);
    mprLogPagePtr.i = mprPageRefPtr.p->pageRef[7];
    ptrCheckGuard(mprLogPagePtr, clogPageFileSize, logPageRecord);
    mprLogPagePtr.p->logPageWord[ZNEXT_PAGE] = pageRefPtr.p->pageRef[0];
  }//if
}//Dblqh::moveToPageRef()

/* ------------------------------------------------------------------------- */
/* -------               READ THE ATTRINFO FROM THE LOG              ------- */
/*                                                                           */
/*       SUBROUTINE SHORT NAME = RA                                          */
/* ------------------------------------------------------------------------- */
void Dblqh::readAttrinfo(Signal* signal) 
{
  Uint32 remainingLen = tcConnectptr.p->totSendlenAi;
  if (remainingLen == 0) {
    jam();
    tcConnectptr.p->reclenAiLqhkey = 0;
    return;
  }//if
  Uint32 dataLen = remainingLen;
  if (remainingLen > 5)
    dataLen = 5;
  readLogData(signal, dataLen, &tcConnectptr.p->firstAttrinfo[0]);
  tcConnectptr.p->reclenAiLqhkey = dataLen;
  remainingLen -= dataLen;
  while (remainingLen > 0) {
    jam();
    dataLen = remainingLen;
    if (remainingLen > 22)
      dataLen = 22;
    seizeAttrinbuf(signal);
    readLogData(signal, dataLen, &attrinbufptr.p->attrbuf[0]);
    attrinbufptr.p->attrbuf[ZINBUF_DATA_LEN] = dataLen;
    remainingLen -= dataLen;
  }//while
}//Dblqh::readAttrinfo()

/* ------------------------------------------------------------------------- */
/* -------               READ COMMIT LOG                             ------- */
/*                                                                           */
/*       SUBROUTINE SHORT NAME = RCL                                         */
/* ------------------------------------------------------------------------- */
void Dblqh::readCommitLog(Signal* signal, CommitLogRecord* commitLogRecord) 
{
  Uint32 trclPageIndex = logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX];
  if ((trclPageIndex + (ZCOMMIT_LOG_SIZE - 1)) < ZPAGE_SIZE) {
    jam();
    tcConnectptr.p->tableref = logPagePtr.p->logPageWord[trclPageIndex + 0];
    tcConnectptr.p->schemaVersion = logPagePtr.p->logPageWord[trclPageIndex + 1];
    tcConnectptr.p->fragmentid = logPagePtr.p->logPageWord[trclPageIndex + 2];
    commitLogRecord->fileNo = logPagePtr.p->logPageWord[trclPageIndex + 3];
    commitLogRecord->startPageNo = logPagePtr.p->logPageWord[trclPageIndex + 4];
    commitLogRecord->startPageIndex = logPagePtr.p->logPageWord[trclPageIndex + 5];
    commitLogRecord->stopPageNo = logPagePtr.p->logPageWord[trclPageIndex + 6];
    tcConnectptr.p->gci = logPagePtr.p->logPageWord[trclPageIndex + 7];
    logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] = 
                            (trclPageIndex + ZCOMMIT_LOG_SIZE) - 1;
  } else {
    jam();
    tcConnectptr.p->tableref = readLogword(signal);
    tcConnectptr.p->schemaVersion = readLogword(signal);
    tcConnectptr.p->fragmentid = readLogword(signal);
    commitLogRecord->fileNo = readLogword(signal);
    commitLogRecord->startPageNo = readLogword(signal);
    commitLogRecord->startPageIndex = readLogword(signal);
    commitLogRecord->stopPageNo = readLogword(signal);
    tcConnectptr.p->gci = readLogword(signal);
  }//if
  tcConnectptr.p->transid[0] = logPartPtr.i + 65536;  
  tcConnectptr.p->transid[1] = (DBLQH << 20) + (cownNodeid << 8);  
}//Dblqh::readCommitLog()

/* ------------------------------------------------------------------------- */
/* -------        READ LOG PAGES FROM DISK IN ORDER TO EXECUTE A LOG ------- */
/*                RECORD WHICH WAS NOT FOUND IN MAIN MEMORY.                 */
/*                                                                           */
/*       SUBROUTINE SHORT NAME = REL                                         */
/* ------------------------------------------------------------------------- */
void Dblqh::readExecLog(Signal* signal) 
{
  UintR trelIndex;
  UintR trelI;

  seizeLfo(signal);
  initLfo(signal);
  trelI = logPartPtr.p->execSrStopPageNo - logPartPtr.p->execSrStartPageNo;
  arrGuard(trelI + 1, 16);
  lfoPtr.p->logPageArray[trelI + 1] = logPartPtr.p->execSrStartPageNo;
  for (trelIndex = logPartPtr.p->execSrStopPageNo; (trelIndex >= logPartPtr.p->execSrStartPageNo) && 
       (UintR)~trelIndex; trelIndex--) {
    jam();
    seizeLogpage(signal);
    arrGuard(trelI, 16);
    lfoPtr.p->logPageArray[trelI] = logPagePtr.i;
    trelI--;
  }//for
  lfoPtr.p->lfoPageNo = logPartPtr.p->execSrStartPageNo;
  lfoPtr.p->noPagesRw = (logPartPtr.p->execSrStopPageNo - 
			 logPartPtr.p->execSrStartPageNo) + 1;
  lfoPtr.p->firstLfoPage = lfoPtr.p->logPageArray[0];
  signal->theData[0] = logFilePtr.p->fileRef;
  signal->theData[1] = cownref;
  signal->theData[2] = lfoPtr.i;
  signal->theData[3] = ZLIST_OF_MEM_PAGES; // edtjamo TR509 //ZLIST_OF_PAIRS;
  signal->theData[4] = ZVAR_NO_LOG_PAGE_WORD;
  signal->theData[5] = lfoPtr.p->noPagesRw;
  signal->theData[6] = lfoPtr.p->logPageArray[0];
  signal->theData[7] = lfoPtr.p->logPageArray[1];
  signal->theData[8] = lfoPtr.p->logPageArray[2];
  signal->theData[9] = lfoPtr.p->logPageArray[3];
  signal->theData[10] = lfoPtr.p->logPageArray[4];
  signal->theData[11] = lfoPtr.p->logPageArray[5];
  signal->theData[12] = lfoPtr.p->logPageArray[6];
  signal->theData[13] = lfoPtr.p->logPageArray[7];
  signal->theData[14] = lfoPtr.p->logPageArray[8];
  signal->theData[15] = lfoPtr.p->logPageArray[9];
  sendSignal(NDBFS_REF, GSN_FSREADREQ, signal, 16, JBA);
}//Dblqh::readExecLog()

/* ------------------------------------------------------------------------- */
/* -------        READ 64 KBYTES WHEN EXECUTING THE FRAGMENT LOG     ------- */
/*                                                                           */
/*       SUBROUTINE SHORT NAME = RES                                         */
/* ------------------------------------------------------------------------- */
void Dblqh::readExecSrNewMbyte(Signal* signal) 
{
  logFilePtr.p->currentFilepage = logFilePtr.p->currentMbyte * ZPAGES_IN_MBYTE;
  logFilePtr.p->filePosition = logFilePtr.p->currentMbyte * ZPAGES_IN_MBYTE;
  logPartPtr.p->execSrPagesRead = 0;
  logPartPtr.p->execSrPagesReading = 0;
  logPartPtr.p->execSrPagesExecuted = 0;
  readExecSr(signal);
  logPartPtr.p->logExecState = LogPartRecord::LES_WAIT_READ_EXEC_SR_NEW_MBYTE;
}//Dblqh::readExecSrNewMbyte()

/* ------------------------------------------------------------------------- */
/* -------        READ 64 KBYTES WHEN EXECUTING THE FRAGMENT LOG     ------- */
/*                                                                           */
/*       SUBROUTINE SHORT NAME = RES                                         */
/* ------------------------------------------------------------------------- */
void Dblqh::readExecSr(Signal* signal) 
{
  UintR tresPageid;
  UintR tresIndex;

  tresPageid = logFilePtr.p->filePosition;
  seizeLfo(signal);
  initLfo(signal);
  for (tresIndex = 7; (UintR)~tresIndex; tresIndex--) {
    jam();
/* ------------------------------------------------------------------------- */
/* GO BACKWARDS SINCE WE INSERT AT THE BEGINNING AND WE WANT THAT FIRST PAGE */
/* SHALL BE FIRST AND LAST PAGE LAST.                                        */
/* ------------------------------------------------------------------------- */
    seizeLogpage(signal);
    lfoPtr.p->logPageArray[tresIndex] = logPagePtr.i;
  }//for
  lfoPtr.p->lfoState = LogFileOperationRecord::READ_EXEC_SR;
  lfoPtr.p->lfoPageNo = tresPageid;
  logFilePtr.p->filePosition = logFilePtr.p->filePosition + 8;
  logPartPtr.p->execSrPagesReading = logPartPtr.p->execSrPagesReading + 8;
  lfoPtr.p->noPagesRw = 8;
  lfoPtr.p->firstLfoPage = lfoPtr.p->logPageArray[0];
  signal->theData[0] = logFilePtr.p->fileRef;
  signal->theData[1] = cownref;
  signal->theData[2] = lfoPtr.i;
  signal->theData[3] = ZLIST_OF_MEM_PAGES;
  signal->theData[4] = ZVAR_NO_LOG_PAGE_WORD;
  signal->theData[5] = 8;
  signal->theData[6] = lfoPtr.p->logPageArray[0];
  signal->theData[7] = lfoPtr.p->logPageArray[1];
  signal->theData[8] = lfoPtr.p->logPageArray[2];
  signal->theData[9] = lfoPtr.p->logPageArray[3];
  signal->theData[10] = lfoPtr.p->logPageArray[4];
  signal->theData[11] = lfoPtr.p->logPageArray[5];
  signal->theData[12] = lfoPtr.p->logPageArray[6];
  signal->theData[13] = lfoPtr.p->logPageArray[7];
  signal->theData[14] = tresPageid;
  sendSignal(NDBFS_REF, GSN_FSREADREQ, signal, 15, JBA);
}//Dblqh::readExecSr()

/* ------------------------------------------------------------------------- */
/* ------------ READ THE PRIMARY KEY FROM THE LOG           ---------------- */
/*                                                                           */
/*       SUBROUTINE SHORT NAME = RK                                          */
/* --------------------------------------------------------------------------*/
void Dblqh::readKey(Signal* signal) 
{
  Uint32 remainingLen = tcConnectptr.p->primKeyLen;
  ndbrequire(remainingLen != 0);
  Uint32 dataLen = remainingLen;
  if (remainingLen > 4)
    dataLen = 4;
  readLogData(signal, dataLen, &tcConnectptr.p->tupkeyData[0]);
  remainingLen -= dataLen;
  while (remainingLen > 0) {
    jam();
    seizeTupkeybuf(signal);
    dataLen = remainingLen;
    if (dataLen > 4)
      dataLen = 4;
    readLogData(signal, dataLen, &databufptr.p->data[0]);
    remainingLen -= dataLen;
  }//while
}//Dblqh::readKey()

/* ------------------------------------------------------------------------- */
/* ------------ READ A NUMBER OF WORDS FROM LOG INTO CDATA  ---------------- */
/*                                                                           */
/*       SUBROUTINE SHORT NAME = RLD                                         */
/* --------------------------------------------------------------------------*/
void Dblqh::readLogData(Signal* signal, Uint32 noOfWords, Uint32* dataPtr) 
{
  ndbrequire(noOfWords < 32);
  Uint32 logPos = logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX];
  if ((logPos + noOfWords) >= ZPAGE_SIZE) {
    for (Uint32 i = 0; i < noOfWords; i++)
      dataPtr[i] = readLogwordExec(signal);
  } else {
    MEMCOPY_NO_WORDS(dataPtr, &logPagePtr.p->logPageWord[logPos], noOfWords);
    logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] = logPos + noOfWords;
  }//if
}//Dblqh::readLogData()

/* ------------------------------------------------------------------------- */
/* ------------ READ THE LOG HEADER OF A PREPARE LOG HEADER ---------------- */
/*                                                                           */
/*       SUBROUTINE SHORT NAME = RLH                                         */
/* --------------------------------------------------------------------------*/
void Dblqh::readLogHeader(Signal* signal) 
{
  Uint32 logPos = logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX];
  if ((logPos + ZLOG_HEAD_SIZE) < ZPAGE_SIZE) { 
    jam();
    tcConnectptr.p->hashValue = logPagePtr.p->logPageWord[logPos + 2];
    tcConnectptr.p->operation = logPagePtr.p->logPageWord[logPos + 3];
    tcConnectptr.p->totSendlenAi = logPagePtr.p->logPageWord[logPos + 4];
    tcConnectptr.p->primKeyLen = logPagePtr.p->logPageWord[logPos + 5];
    logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] = logPos + ZLOG_HEAD_SIZE;
  } else {
    jam();
    readLogwordExec(signal);	/* IGNORE PREPARE LOG RECORD TYPE */
    readLogwordExec(signal);	/* IGNORE LOG RECORD SIZE         */
    tcConnectptr.p->hashValue = readLogwordExec(signal);
    tcConnectptr.p->operation = readLogwordExec(signal);
    tcConnectptr.p->totSendlenAi = readLogwordExec(signal);
    tcConnectptr.p->primKeyLen = readLogwordExec(signal);
  }//if
}//Dblqh::readLogHeader()

/* ------------------------------------------------------------------------- */
/* -------               READ A WORD FROM THE LOG                    ------- */
/*                                                                           */
/*       OUTPUT:         TLOG_WORD                                           */
/*       SUBROUTINE SHORT NAME = RLW                                         */
/* ------------------------------------------------------------------------- */
Uint32 Dblqh::readLogword(Signal* signal) 
{
  Uint32 logPos = logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX];
  ndbrequire(logPos < ZPAGE_SIZE);
  Uint32 logWord = logPagePtr.p->logPageWord[logPos];
  logPos++;
  logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] = logPos;
  if (logPos >= ZPAGE_SIZE) {
    jam();
    logPagePtr.i = logPagePtr.p->logPageWord[ZNEXT_PAGE];
    ptrCheckGuard(logPagePtr, clogPageFileSize, logPageRecord);
    logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] = ZPAGE_HEADER_SIZE;
    logFilePtr.p->currentLogpage = logPagePtr.i;
    logFilePtr.p->currentFilepage++;
    logPartPtr.p->execSrPagesRead--;
    logPartPtr.p->execSrPagesExecuted++;
  }//if
  return logWord;
}//Dblqh::readLogword()

/* ------------------------------------------------------------------------- */
/* -------   READ A WORD FROM THE LOG WHEN EXECUTING A LOG RECORD    ------- */
/*                                                                           */
/*       OUTPUT:         TLOG_WORD                                           */
/*       SUBROUTINE SHORT NAME = RWE                                         */
/* ------------------------------------------------------------------------- */
Uint32 Dblqh::readLogwordExec(Signal* signal) 
{
  Uint32 logPos = logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX];
  ndbrequire(logPos < ZPAGE_SIZE);
  Uint32 logWord = logPagePtr.p->logPageWord[logPos];
  logPos++;
  logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] = logPos;
  if (logPos >= ZPAGE_SIZE) {
    jam();
    logPagePtr.i = logPagePtr.p->logPageWord[ZNEXT_PAGE];
    if (logPagePtr.i != RNIL){
      ptrCheckGuard(logPagePtr, clogPageFileSize, logPageRecord);
      logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] = ZPAGE_HEADER_SIZE;
    } else {
      // Reading word at the last pos in the last page
      // Don't step forward to next page!
      jam();
      logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX]++;
    }
  }//if
  return logWord;
}//Dblqh::readLogwordExec()

/* ------------------------------------------------------------------------- */
/* -------               READ A SINGLE PAGE FROM THE LOG             ------- */
/*                                                                           */
/*       INPUT:          TRSP_PAGE_NO                                        */
/*       SUBROUTINE SHORT NAME = RSP                                         */
/* ------------------------------------------------------------------------- */
void Dblqh::readSinglePage(Signal* signal, Uint32 pageNo) 
{
  seizeLfo(signal);
  initLfo(signal);
  seizeLogpage(signal);
  lfoPtr.p->firstLfoPage = logPagePtr.i;
  lfoPtr.p->lfoPageNo = pageNo;
  lfoPtr.p->noPagesRw = 1;
  signal->theData[0] = logFilePtr.p->fileRef;
  signal->theData[1] = cownref;
  signal->theData[2] = lfoPtr.i;
  signal->theData[3] = ZLIST_OF_PAIRS;
  signal->theData[4] = ZVAR_NO_LOG_PAGE_WORD;
  signal->theData[5] = 1;
  signal->theData[6] = logPagePtr.i;
  signal->theData[7] = pageNo;
  sendSignal(NDBFS_REF, GSN_FSREADREQ, signal, 8, JBA);
}//Dblqh::readSinglePage()

/* --------------------------------------------------------------------------
 * -------         RELEASE OPERATION FROM ACTIVE LIST ON FRAGMENT     ------- 
 * 
 *       SUBROUTINE SHORT NAME = RAC
 * ------------------------------------------------------------------------- */
void Dblqh::releaseAccList(Signal* signal) 
{
  TcConnectionrecPtr racTcNextConnectptr;
  TcConnectionrecPtr racTcPrevConnectptr;

  fragptr.i = tcConnectptr.p->fragmentptr;
  ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
  racTcPrevConnectptr.i = tcConnectptr.p->prevTc;
  racTcNextConnectptr.i = tcConnectptr.p->nextTc;
  if (tcConnectptr.p->listState != TcConnectionrec::ACC_BLOCK_LIST) {
    jam();
    systemError(signal);
  }//if
  tcConnectptr.p->listState = TcConnectionrec::NOT_IN_LIST;
  if (racTcNextConnectptr.i != RNIL) {
    jam();
    ptrCheckGuard(racTcNextConnectptr, ctcConnectrecFileSize, tcConnectionrec);
    racTcNextConnectptr.p->prevTc = racTcPrevConnectptr.i;
  }//if
  if (racTcPrevConnectptr.i != RNIL) {
    jam();
    ptrCheckGuard(racTcPrevConnectptr, ctcConnectrecFileSize, tcConnectionrec);
    racTcPrevConnectptr.p->nextTc = tcConnectptr.p->nextTc;
  } else {
    jam();
    /* ---------------------------------------------------------------------
     *    OPERATION RECORD IS FIRST IN ACTIVE LIST
     *    THIS MEANS THAT THERE EXISTS NO PREVIOUS TC THAT NEEDS TO BE UPDATED.
     * --------------------------------------------------------------------- */
    fragptr.p->accBlockedList = racTcNextConnectptr.i;
  }//if
}//Dblqh::releaseAccList()

/* -------------------------------------------------------------------------- 
 * -------       REMOVE COPY FRAGMENT FROM ACTIVE COPY LIST           ------- 
 *
 * ------------------------------------------------------------------------- */
void Dblqh::releaseActiveCopy(Signal* signal) 
{
                                                /* MUST BE 8 BIT */
  UintR tracFlag;
  UintR tracIndex;

  tracFlag = ZFALSE;
  for (tracIndex = 0; tracIndex < 4; tracIndex++) {
    if (tracFlag == ZFALSE) {
      jam();
      if (cactiveCopy[tracIndex] == fragptr.i) {
        jam();
        tracFlag = ZTRUE;
      }//if
    } else {
      if (tracIndex < 3) {
        jam();
        cactiveCopy[tracIndex - 1] = cactiveCopy[tracIndex];
      } else {
        jam();
        cactiveCopy[3] = RNIL;
      }//if
    }//if
  }//for
  ndbrequire(tracFlag == ZTRUE);
  cnoActiveCopy--;
}//Dblqh::releaseActiveCopy()

/* --------------------------------------------------------------------------
 * -------        RELEASE OPERATION FROM ACTIVE LIST ON FRAGMENT      ------- 
 * 
 *       SUBROUTINE SHORT NAME = RAL
 * ------------------------------------------------------------------------- */
void Dblqh::releaseActiveList(Signal* signal) 
{
  TcConnectionrecPtr ralTcNextConnectptr;
  TcConnectionrecPtr ralTcPrevConnectptr;
  ralTcPrevConnectptr.i = tcConnectptr.p->prevTc;
  ralTcNextConnectptr.i = tcConnectptr.p->nextTc;
  ndbrequire(tcConnectptr.p->listState == TcConnectionrec::IN_ACTIVE_LIST);
  tcConnectptr.p->listState = TcConnectionrec::NOT_IN_LIST;
  if (ralTcNextConnectptr.i != RNIL) {
    jam();
    ptrCheckGuard(ralTcNextConnectptr, ctcConnectrecFileSize, tcConnectionrec);
    ralTcNextConnectptr.p->prevTc = ralTcPrevConnectptr.i;
  }//if
  if (ralTcPrevConnectptr.i != RNIL) {
    jam();
    ptrCheckGuard(ralTcPrevConnectptr, ctcConnectrecFileSize, tcConnectionrec);
    ralTcPrevConnectptr.p->nextTc = tcConnectptr.p->nextTc;
  } else {
    jam();
    /* ----------------------------------------------------------------------
     *   OPERATION RECORD IS FIRST IN ACTIVE LIST
     *   THIS MEANS THAT THERE EXISTS NO PREVIOUS TC THAT NEEDS TO BE UPDATED.
     * --------------------------------------------------------------------- */
    fragptr.p->activeList = ralTcNextConnectptr.i;
  }//if
}//Dblqh::releaseActiveList()

/* --------------------------------------------------------------------------
 * -------       RELEASE ADD FRAGMENT RECORD                          ------- 
 *
 * ------------------------------------------------------------------------- */
void Dblqh::releaseAddfragrec(Signal* signal) 
{
  addfragptr.p->addfragStatus = AddFragRecord::FREE;
  addfragptr.p->nextAddfragrec = cfirstfreeAddfragrec;
  cfirstfreeAddfragrec = addfragptr.i;
}//Dblqh::releaseAddfragrec()

/* --------------------------------------------------------------------------
 * -------       RELEASE FRAGMENT RECORD                              -------
 *
 * ------------------------------------------------------------------------- */
void Dblqh::releaseFragrec() 
{
  fragptr.p->fragStatus = Fragrecord::FREE;
  fragptr.p->nextFrag = cfirstfreeFragrec;
  cfirstfreeFragrec = fragptr.i;
}//Dblqh::releaseFragrec()

/* --------------------------------------------------------------------------
 * -------       RELEASE LCP LOCAL RECORD                             ------- 
 * 
 * ------------------------------------------------------------------------- */
void Dblqh::releaseLcpLoc(Signal* signal) 
{
  lcpLocptr.p->lcpLocstate = LcpLocRecord::IDLE;
  lcpLocptr.p->nextLcpLoc = cfirstfreeLcpLoc;
  cfirstfreeLcpLoc = lcpLocptr.i;
}//Dblqh::releaseLcpLoc()

/* --------------------------------------------------------------------------
 * -------     RELEASE A PAGE REFERENCE RECORD.                       ------- 
 *
 * ------------------------------------------------------------------------- */
void Dblqh::releasePageRef(Signal* signal) 
{
  pageRefPtr.p->prNext = cfirstfreePageRef;
  cfirstfreePageRef = pageRefPtr.i;
}//Dblqh::releasePageRef()

/* --------------------------------------------------------------------------
 * --- RELEASE ALL PAGES IN THE MM BUFFER AFTER EXECUTING THE LOG ON IT. ---- 
 * 
 * ------------------------------------------------------------------------- */
void Dblqh::releaseMmPages(Signal* signal) 
{
RMP_LOOP:
  jam();
  pageRefPtr.i = logPartPtr.p->firstPageRef;
  if (pageRefPtr.i != RNIL) {
    jam();
    ptrCheckGuard(pageRefPtr, cpageRefFileSize, pageRefRecord);
    releasePrPages(signal);
    removePageRef(signal);
    goto RMP_LOOP;
  }//if
}//Dblqh::releaseMmPages()

/* --------------------------------------------------------------------------
 * -------     RELEASE A SET OF PAGES AFTER EXECUTING THE LOG ON IT.  ------- 
 * 
 * ------------------------------------------------------------------------- */
void Dblqh::releasePrPages(Signal* signal) 
{
  UintR trppIndex;

  for (trppIndex = 0; trppIndex <= 7; trppIndex++) {
    jam();
    logPagePtr.i = pageRefPtr.p->pageRef[trppIndex];
    ptrCheckGuard(logPagePtr, clogPageFileSize, logPageRecord);
    releaseLogpage(signal);
  }//for
}//Dblqh::releasePrPages()

/* --------------------------------------------------------------------------
 * -------  RELEASE OPERATION FROM WAIT QUEUE LIST ON FRAGMENT        ------- 
 *
 *       SUBROUTINE SHORT NAME : RWA
 * ------------------------------------------------------------------------- */
void Dblqh::releaseWaitQueue(Signal* signal) 
{
  TcConnectionrecPtr rwaTcNextConnectptr;
  TcConnectionrecPtr rwaTcPrevConnectptr;

  fragptr.i = tcConnectptr.p->fragmentptr;
  ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
  rwaTcPrevConnectptr.i = tcConnectptr.p->prevTc;
  rwaTcNextConnectptr.i = tcConnectptr.p->nextTc;
  if (tcConnectptr.p->listState != TcConnectionrec::WAIT_QUEUE_LIST) {
    jam();
    systemError(signal);
  }//if
  tcConnectptr.p->listState = TcConnectionrec::NOT_IN_LIST;
  if (rwaTcNextConnectptr.i != RNIL) {
    jam();
    ptrCheckGuard(rwaTcNextConnectptr, ctcConnectrecFileSize, tcConnectionrec);
    rwaTcNextConnectptr.p->prevTc = rwaTcPrevConnectptr.i;
  } else {
    jam();
    fragptr.p->lastWaitQueue = rwaTcPrevConnectptr.i;
  }//if
  if (rwaTcPrevConnectptr.i != RNIL) {
    jam();
    ptrCheckGuard(rwaTcPrevConnectptr, ctcConnectrecFileSize, tcConnectionrec);
    rwaTcPrevConnectptr.p->nextTc = rwaTcNextConnectptr.i;
  } else {
    jam();
    fragptr.p->firstWaitQueue = rwaTcNextConnectptr.i;
  }//if
}//Dblqh::releaseWaitQueue()

/* -------------------------------------------------------------------------- 
 * -------  REMOVE OPERATION RECORD FROM LIST ON LOG PART OF NOT      ------- 
 *               COMPLETED OPERATIONS IN THE LOG.
 *
 *       SUBROUTINE SHORT NAME = RLO
 * ------------------------------------------------------------------------- */
void Dblqh::removeLogTcrec(Signal* signal) 
{
  TcConnectionrecPtr rloTcNextConnectptr;
  TcConnectionrecPtr rloTcPrevConnectptr;
  rloTcPrevConnectptr.i = tcConnectptr.p->prevLogTcrec;
  rloTcNextConnectptr.i = tcConnectptr.p->nextLogTcrec;
  if (rloTcNextConnectptr.i != RNIL) {
    jam();
    ptrCheckGuard(rloTcNextConnectptr, ctcConnectrecFileSize, tcConnectionrec);
    rloTcNextConnectptr.p->prevLogTcrec = rloTcPrevConnectptr.i;
  } else {
    jam();
    logPartPtr.p->lastLogTcrec = rloTcPrevConnectptr.i;
  }//if
  if (rloTcPrevConnectptr.i != RNIL) {
    jam();
    ptrCheckGuard(rloTcPrevConnectptr, ctcConnectrecFileSize, tcConnectionrec);
    rloTcPrevConnectptr.p->nextLogTcrec = rloTcNextConnectptr.i;
  } else {
    jam();
    logPartPtr.p->firstLogTcrec = rloTcNextConnectptr.i;
  }//if
}//Dblqh::removeLogTcrec()

/* --------------------------------------------------------------------------
 * -------  REMOVE PAGE REFERENCE RECORD FROM LIST IN THIS LOG PART   ------- 
 * 
 *       SUBROUTINE SHORT NAME = RPR
 * ------------------------------------------------------------------------- */
void Dblqh::removePageRef(Signal* signal) 
{
  PageRefRecordPtr rprPageRefPtr;

  pageRefPtr.i = logPartPtr.p->firstPageRef;
  if (pageRefPtr.i != RNIL) {
    jam();
    ptrCheckGuard(pageRefPtr, cpageRefFileSize, pageRefRecord);
    if (pageRefPtr.p->prNext == RNIL) {
      jam();
      logPartPtr.p->lastPageRef = RNIL;
      logPartPtr.p->firstPageRef = RNIL;
    } else {
      jam();
      logPartPtr.p->firstPageRef = pageRefPtr.p->prNext;
      rprPageRefPtr.i = pageRefPtr.p->prNext;
      ptrCheckGuard(rprPageRefPtr, cpageRefFileSize, pageRefRecord);
      rprPageRefPtr.p->prPrev = RNIL;
    }//if
    releasePageRef(signal);
  }//if
}//Dblqh::removePageRef()

/* ------------------------------------------------------------------------- */
/* -------       RETURN FROM EXECUTION OF LOG                        ------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
Uint32 Dblqh::returnExecLog(Signal* signal) 
{
  tcConnectptr.p->connectState = TcConnectionrec::CONNECTED;
  initLogPointers(signal);
  logPartPtr.p->execSrExecuteIndex++;
  Uint32 result = checkIfExecLog(signal);
  if (result == ZOK) {
    jam();
/* ------------------------------------------------------------------------- */
/* THIS LOG RECORD WILL BE EXECUTED AGAIN TOWARDS ANOTHER NODE.              */
/* ------------------------------------------------------------------------- */
    logPagePtr.i = logPartPtr.p->execSrLogPage;
    ptrCheckGuard(logPagePtr, clogPageFileSize, logPageRecord);
    logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] = 
                  logPartPtr.p->execSrLogPageIndex;
  } else {
    jam();
/* ------------------------------------------------------------------------- */
/*       NO MORE EXECUTION OF THIS LOG RECORD.                               */
/* ------------------------------------------------------------------------- */
    if (logPartPtr.p->logExecState == 
	LogPartRecord::LES_EXEC_LOGREC_FROM_FILE) {
      jam();
/* ------------------------------------------------------------------------- */
/* THE LOG RECORD WAS READ FROM DISK. RELEASE ITS PAGES IMMEDIATELY.         */
/* ------------------------------------------------------------------------- */
      lfoPtr.i = logPartPtr.p->execSrLfoRec;
      ptrCheckGuard(lfoPtr, clfoFileSize, logFileOperationRecord);
      releaseLfoPages(signal);
      releaseLfo(signal);
      logPartPtr.p->logExecState = LogPartRecord::LES_EXEC_LOG;
      if (logPartPtr.p->execSrExecLogFile != logPartPtr.p->currentLogfile) {
        jam();
        LogFileRecordPtr clfLogFilePtr;
        clfLogFilePtr.i = logPartPtr.p->execSrExecLogFile;
        ptrCheckGuard(clfLogFilePtr, clogFileFileSize, logFileRecord);
        clfLogFilePtr.p->logFileStatus = LogFileRecord::CLOSING_EXEC_LOG;
        closeFile(signal, clfLogFilePtr);
        result = ZCLOSE_FILE;
      }//if
    }//if
    logPartPtr.p->execSrExecuteIndex = 0;
    logPartPtr.p->execSrLogPage = RNIL;
    logPartPtr.p->execSrLogPageIndex = ZNIL;
    logPagePtr.i = logFilePtr.p->currentLogpage;
    ptrCheckGuard(logPagePtr, clogPageFileSize, logPageRecord);
    logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] = logPartPtr.p->savePageIndex;
  }//if
  return result;
}//Dblqh::returnExecLog()

/* --------------------------------------------------------------------------
 * -------       SEIZE ADD FRAGMENT RECORD                             ------
 * 
 * ------------------------------------------------------------------------- */
void Dblqh::seizeAddfragrec(Signal* signal) 
{
  addfragptr.i = cfirstfreeAddfragrec;
  ptrCheckGuard(addfragptr, caddfragrecFileSize, addFragRecord);
  cfirstfreeAddfragrec = addfragptr.p->nextAddfragrec;
}//Dblqh::seizeAddfragrec()

/* --------------------------------------------------------------------------
 * -------       SEIZE FRAGMENT RECORD                                ------- 
 *
 * ------------------------------------------------------------------------- */
void Dblqh::seizeFragmentrec(Signal* signal) 
{
  fragptr.i = cfirstfreeFragrec;
  ptrCheckGuard(fragptr, cfragrecFileSize, fragrecord);
  cfirstfreeFragrec = fragptr.p->nextFrag;
  fragptr.p->nextFrag = RNIL;
}//Dblqh::seizeFragmentrec()

/* ------------------------------------------------------------------------- */
/* -------     SEIZE A PAGE REFERENCE RECORD.                        ------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void Dblqh::seizePageRef(Signal* signal) 
{
  pageRefPtr.i = cfirstfreePageRef;
  ptrCheckGuard(pageRefPtr, cpageRefFileSize, pageRefRecord);
  cfirstfreePageRef = pageRefPtr.p->prNext;
  pageRefPtr.p->prNext = RNIL;
}//Dblqh::seizePageRef()

/* --------------------------------------------------------------------------
 * -------               SEND ABORTED                                 ------- 
 *
 * ------------------------------------------------------------------------- */
void Dblqh::sendAborted(Signal* signal) 
{
  UintR TlastInd;
  if (tcConnectptr.p->nextReplica == ZNIL) {
    TlastInd = ZTRUE;
  } else {
    TlastInd = ZFALSE;
  }//if
  signal->theData[0] = tcConnectptr.p->tcOprec;
  signal->theData[1] = tcConnectptr.p->transid[0];
  signal->theData[2] = tcConnectptr.p->transid[1];
  signal->theData[3] = cownNodeid;
  signal->theData[4] = TlastInd;
  sendSignal(tcConnectptr.p->tcBlockref, GSN_ABORTED, signal, 5, JBB);
  return;
}//Dblqh::sendAborted()

/* --------------------------------------------------------------------------
 * -------               SEND LQH_TRANSCONF                           ------- 
 *
 * ------------------------------------------------------------------------- */
void Dblqh::sendLqhTransconf(Signal* signal, LqhTransConf::OperationStatus stat)
{
  tcNodeFailptr.i = tcConnectptr.p->tcNodeFailrec;
  ptrCheckGuard(tcNodeFailptr, ctcNodeFailrecFileSize, tcNodeFailRecord);

  Uint32 reqInfo = 0;
  LqhTransConf::setReplicaType(reqInfo, tcConnectptr.p->replicaType);
  LqhTransConf::setReplicaNo(reqInfo, tcConnectptr.p->seqNoReplica);
  LqhTransConf::setLastReplicaNo(reqInfo, tcConnectptr.p->lastReplicaNo);
  LqhTransConf::setSimpleFlag(reqInfo, tcConnectptr.p->opSimple);
  LqhTransConf::setDirtyFlag(reqInfo, tcConnectptr.p->dirtyOp);
  LqhTransConf::setOperation(reqInfo, tcConnectptr.p->operation);
  
  LqhTransConf * const lqhTransConf = (LqhTransConf *)&signal->theData[0];
  lqhTransConf->tcRef           = tcNodeFailptr.p->newTcRef;
  lqhTransConf->lqhNodeId       = cownNodeid;
  lqhTransConf->operationStatus = stat;
  lqhTransConf->lqhConnectPtr   = tcConnectptr.i;
  lqhTransConf->transId1        = tcConnectptr.p->transid[0];
  lqhTransConf->transId2        = tcConnectptr.p->transid[1];
  lqhTransConf->oldTcOpRec      = tcConnectptr.p->tcOprec;
  lqhTransConf->requestInfo     = reqInfo;
  lqhTransConf->gci             = tcConnectptr.p->gci;
  lqhTransConf->nextNodeId1     = tcConnectptr.p->nextReplica;
  lqhTransConf->nextNodeId2     = tcConnectptr.p->nodeAfterNext[0];
  lqhTransConf->nextNodeId3     = tcConnectptr.p->nodeAfterNext[1];
  lqhTransConf->apiRef          = tcConnectptr.p->applRef;
  lqhTransConf->apiOpRec        = tcConnectptr.p->applOprec;
  lqhTransConf->tableId         = tcConnectptr.p->tableref;
  sendSignal(tcNodeFailptr.p->newTcBlockref, GSN_LQH_TRANSCONF, 
	     signal, LqhTransConf::SignalLength, JBB);
  tcNodeFailptr.p->tcRecNow = tcConnectptr.i + 1;
  signal->theData[0] = ZLQH_TRANS_NEXT;
  signal->theData[1] = tcNodeFailptr.i;
  sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
}//Dblqh::sendLqhTransconf()

/* --------------------------------------------------------------------------
 * -------               START ANOTHER PHASE OF LOG EXECUTION         -------
 *       RESET THE VARIABLES NEEDED BY THIS PROCESS AND SEND THE START SIGNAL
 *
 * ------------------------------------------------------------------------- */
void Dblqh::startExecSr(Signal* signal) 
{
  cnoFragmentsExecSr = 0;
  signal->theData[0] = cfirstCompletedFragSr;
  signal->theData[1] = RNIL;
  sendSignal(cownref, GSN_START_EXEC_SR, signal, 2, JBB);
}//Dblqh::startExecSr()

/* ¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤ 
 * ¤¤¤¤¤¤¤                            LOG MODULE                      ¤¤¤¤¤¤¤ 
 * ¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤¤ */
/* -------------------------------------------------------------------------- 
 * -------       STEP FORWARD IN FRAGMENT LOG DURING LOG EXECUTION    ------- 
 * 
 * ------------------------------------------------------------------------- */
void Dblqh::stepAhead(Signal* signal, Uint32 stepAheadWords) 
{
  UintR tsaPos;

  tsaPos = logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX];
  while ((stepAheadWords + tsaPos) >= ZPAGE_SIZE) {
    jam();
    logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] = ZPAGE_SIZE;
    stepAheadWords = stepAheadWords - (ZPAGE_SIZE - tsaPos);
    logFilePtr.p->currentLogpage = logPagePtr.p->logPageWord[ZNEXT_PAGE];
    logPagePtr.i = logPagePtr.p->logPageWord[ZNEXT_PAGE];
    logFilePtr.p->currentFilepage++;
    ptrCheckGuard(logPagePtr, clogPageFileSize, logPageRecord);
    logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] = ZPAGE_HEADER_SIZE;
    logPartPtr.p->execSrPagesRead--;
    logPartPtr.p->execSrPagesExecuted++;
    tsaPos = ZPAGE_HEADER_SIZE;
  }//while
  logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] = stepAheadWords + tsaPos;
}//Dblqh::stepAhead()

/* --------------------------------------------------------------------------
 * -------               WRITE A ABORT LOG RECORD                     -------
 *
 *       SUBROUTINE SHORT NAME: WAL
 * ------------------------------------------------------------------------- */
void Dblqh::writeAbortLog(Signal* signal) 
{
  if ((ZABORT_LOG_SIZE + ZNEXT_LOG_SIZE) > 
      logFilePtr.p->remainingWordsInMbyte) {
    jam();
    changeMbyte(signal);
  }//if
  logFilePtr.p->remainingWordsInMbyte = 
    logFilePtr.p->remainingWordsInMbyte - ZABORT_LOG_SIZE;
  writeLogWord(signal, ZABORT_TYPE);
  writeLogWord(signal, tcConnectptr.p->transid[0]);
  writeLogWord(signal, tcConnectptr.p->transid[1]);
}//Dblqh::writeAbortLog()

/* --------------------------------------------------------------------------
 * -------               WRITE A COMMIT LOG RECORD                    ------- 
 *
 *       SUBROUTINE SHORT NAME: WCL
 * ------------------------------------------------------------------------- */
void Dblqh::writeCommitLog(Signal* signal, LogPartRecordPtr regLogPartPtr) 
{
  LogFileRecordPtr regLogFilePtr;
  LogPageRecordPtr regLogPagePtr;
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  regLogFilePtr.i = regLogPartPtr.p->currentLogfile;
  ptrCheckGuard(regLogFilePtr, clogFileFileSize, logFileRecord);
  regLogPagePtr.i = regLogFilePtr.p->currentLogpage;
  Uint32 twclTmp = regLogFilePtr.p->remainingWordsInMbyte;
  ptrCheckGuard(regLogPagePtr, clogPageFileSize, logPageRecord);
  logPartPtr = regLogPartPtr;
  logFilePtr = regLogFilePtr;
  logPagePtr = regLogPagePtr;
  if ((ZCOMMIT_LOG_SIZE + ZNEXT_LOG_SIZE) > twclTmp) {
    jam();
    changeMbyte(signal);
    twclTmp = logFilePtr.p->remainingWordsInMbyte;
  }//if

  Uint32 twclLogPos = logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX];
  Uint32 tableId = regTcPtr->tableref;
  Uint32 schemaVersion = regTcPtr->schemaVersion;
  Uint32 fragId = regTcPtr->fragmentid;
  Uint32 fileNo = regTcPtr->logStartFileNo;
  Uint32 startPageNo = regTcPtr->logStartPageNo;
  Uint32 pageIndex = regTcPtr->logStartPageIndex;
  Uint32 stopPageNo = regTcPtr->logStopPageNo;
  Uint32 gci = regTcPtr->gci;
  logFilePtr.p->remainingWordsInMbyte = twclTmp - ZCOMMIT_LOG_SIZE;

  if ((twclLogPos + ZCOMMIT_LOG_SIZE) >= ZPAGE_SIZE) {
    writeLogWord(signal, ZCOMMIT_TYPE);
    writeLogWord(signal, tableId);
    writeLogWord(signal, schemaVersion);
    writeLogWord(signal, fragId);
    writeLogWord(signal, fileNo);
    writeLogWord(signal, startPageNo);
    writeLogWord(signal, pageIndex);
    writeLogWord(signal, stopPageNo);
    writeLogWord(signal, gci);
  } else {
    Uint32* dataPtr = &logPagePtr.p->logPageWord[twclLogPos];
    logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] = twclLogPos + ZCOMMIT_LOG_SIZE;
    dataPtr[0] = ZCOMMIT_TYPE;
    dataPtr[1] = tableId;
    dataPtr[2] = schemaVersion;
    dataPtr[3] = fragId;
    dataPtr[4] = fileNo;
    dataPtr[5] = startPageNo;
    dataPtr[6] = pageIndex;
    dataPtr[7] = stopPageNo;
    dataPtr[8] = gci;
  }//if
  TcConnectionrecPtr rloTcNextConnectptr;
  TcConnectionrecPtr rloTcPrevConnectptr;
  rloTcPrevConnectptr.i = regTcPtr->prevLogTcrec;
  rloTcNextConnectptr.i = regTcPtr->nextLogTcrec;
  if (rloTcNextConnectptr.i != RNIL) {
    jam();
    ptrCheckGuard(rloTcNextConnectptr, ctcConnectrecFileSize, tcConnectionrec);
    rloTcNextConnectptr.p->prevLogTcrec = rloTcPrevConnectptr.i;
  } else {
    regLogPartPtr.p->lastLogTcrec = rloTcPrevConnectptr.i;
  }//if
  if (rloTcPrevConnectptr.i != RNIL) {
    jam();
    ptrCheckGuard(rloTcPrevConnectptr, ctcConnectrecFileSize, tcConnectionrec);
    rloTcPrevConnectptr.p->nextLogTcrec = rloTcNextConnectptr.i;
  } else {
    regLogPartPtr.p->firstLogTcrec = rloTcNextConnectptr.i;
  }//if
}//Dblqh::writeCommitLog()

/* -------------------------------------------------------------------------- 
 * -------               WRITE A COMPLETED GCI LOG RECORD             ------- 
 *
 *       SUBROUTINE SHORT NAME: WCG
// Input Pointers:
// logFilePtr
// logPartPtr
 * ------------------------------------------------------------------------- */
void Dblqh::writeCompletedGciLog(Signal* signal) 
{
  if ((ZCOMPLETED_GCI_LOG_SIZE + ZNEXT_LOG_SIZE) > 
      logFilePtr.p->remainingWordsInMbyte) {
    jam();
    changeMbyte(signal);
  }//if
  logFilePtr.p->remainingWordsInMbyte = 
    logFilePtr.p->remainingWordsInMbyte - ZCOMPLETED_GCI_LOG_SIZE;
  writeLogWord(signal, ZCOMPLETED_GCI_TYPE);
  writeLogWord(signal, cnewestCompletedGci);
  logPartPtr.p->logPartNewestCompletedGCI = cnewestCompletedGci;
}//Dblqh::writeCompletedGciLog()

/* --------------------------------------------------------------------------
 * -------         WRITE A DIRTY PAGE DURING LOG EXECUTION            ------- 
 * 
 *     SUBROUTINE SHORT NAME: WD
 * ------------------------------------------------------------------------- */
void Dblqh::writeDirty(Signal* signal) 
{
  logPagePtr.p->logPageWord[ZPOS_DIRTY] = ZNOT_DIRTY;

  // Calculate checksum for page
  logPagePtr.p->logPageWord[ZPOS_CHECKSUM] = calcPageCheckSum(logPagePtr);

  seizeLfo(signal);
  initLfo(signal);
  lfoPtr.p->lfoPageNo = logPartPtr.p->prevFilepage;
  lfoPtr.p->noPagesRw = 1;
  lfoPtr.p->lfoState = LogFileOperationRecord::WRITE_DIRTY;
  lfoPtr.p->firstLfoPage = logPagePtr.i;
  signal->theData[0] = logFilePtr.p->fileRef;
  signal->theData[1] = cownref;
  signal->theData[2] = lfoPtr.i;
  signal->theData[3] = ZLIST_OF_PAIRS_SYNCH;
  signal->theData[4] = ZVAR_NO_LOG_PAGE_WORD;
  signal->theData[5] = 1;
  signal->theData[6] = logPagePtr.i;
  signal->theData[7] = logPartPtr.p->prevFilepage;
  sendSignal(NDBFS_REF, GSN_FSWRITEREQ, signal, 8, JBA);
}//Dblqh::writeDirty()

/* --------------------------------------------------------------------------
 * -------          WRITE A WORD INTO THE LOG, CHECK FOR NEW PAGE     ------- 
 * 
 *       SUBROUTINE SHORT NAME:  WLW
 * ------------------------------------------------------------------------- */
void Dblqh::writeLogWord(Signal* signal, Uint32 data) 
{
  Uint32 logPos = logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX];
  ndbrequire(logPos < ZPAGE_SIZE);
  logPagePtr.p->logPageWord[logPos] = data;
  logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] = logPos + 1;
  if ((logPos + 1) == ZPAGE_SIZE) {
    jam();
    completedLogPage(signal, ZNORMAL);
    seizeLogpage(signal);
    initLogpage(signal);
    logFilePtr.p->currentLogpage = logPagePtr.i;
    logFilePtr.p->currentFilepage++;
  }//if
}//Dblqh::writeLogWord()

/* --------------------------------------------------------------------------
 * -------         WRITE A NEXT LOG RECORD AND CHANGE TO NEXT MBYTE   ------- 
 *
 *       SUBROUTINE SHORT NAME:  WNL
// Input Pointers:
// logFilePtr(Redefines)
// logPagePtr (Redefines)
// logPartPtr
 * ------------------------------------------------------------------------- */
void Dblqh::writeNextLog(Signal* signal) 
{
  LogFileRecordPtr wnlNextLogFilePtr;
  UintR twnlNextFileNo;
  UintR twnlNewMbyte;
  UintR twnlRemWords;
  UintR twnlNextMbyte;

/* -------------------------------------------------- */
/*       CALCULATE THE NEW NUMBER OF REMAINING WORDS  */
/*       AS 128*2036 WHERE 128 * 8 KBYTE = 1 MBYTE    */
/*       AND 2036 IS THE NUMBER OF WORDS IN A PAGE    */
/*       THAT IS USED FOR LOG INFORMATION.            */
/* -------------------------------------------------- */
  twnlRemWords = ZPAGE_SIZE - ZPAGE_HEADER_SIZE;
  twnlRemWords = twnlRemWords * ZPAGES_IN_MBYTE;
  wnlNextLogFilePtr.i = logFilePtr.p->nextLogFile;
  ptrCheckGuard(wnlNextLogFilePtr, clogFileFileSize, logFileRecord);
/* -------------------------------------------------- */
/*       WRITE THE NEXT LOG RECORD.                   */
/* -------------------------------------------------- */
  ndbrequire(logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] < ZPAGE_SIZE);
  logPagePtr.p->logPageWord[logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX]] = 
    ZNEXT_MBYTE_TYPE;
  if (logFilePtr.p->currentMbyte == (ZNO_MBYTES_IN_FILE - 1)) {
    jam();
/* -------------------------------------------------- */
/*       CALCULATE THE NEW REMAINING WORDS WHEN       */
/*       CHANGING LOG FILE IS PERFORMED               */
/* -------------------------------------------------- */
    twnlRemWords = twnlRemWords - (ZPAGE_SIZE - ZPAGE_HEADER_SIZE);
/* -------------------------------------------------- */
/*       ENSURE THAT THE LOG PAGES ARE WRITTEN AFTER  */
/*       WE HAVE CHANGED MBYTE.                       */
/* -------------------------------------------------- */
/*       ENSURE LAST PAGE IN PREVIOUS MBYTE IS        */
/*       WRITTEN AND THAT THE STATE OF THE WRITE IS   */
/*       PROPERLY SET.                                */
/* -------------------------------------------------- */
/*       WE HAVE TO CHANGE LOG FILE                   */
/* -------------------------------------------------- */
    completedLogPage(signal, ZLAST_WRITE_IN_FILE);
    if (wnlNextLogFilePtr.p->fileNo == 0) {
      jam();
/* -------------------------------------------------- */
/*       WE HAVE FINALISED A LOG LAP, START FROM LOG  */
/*       FILE 0 AGAIN                                 */
/* -------------------------------------------------- */
      logPartPtr.p->logLap++;
    }//if
    logPartPtr.p->currentLogfile = wnlNextLogFilePtr.i;
    logFilePtr.i = wnlNextLogFilePtr.i;
    logFilePtr.p = wnlNextLogFilePtr.p;
    twnlNewMbyte = 0;
  } else {
    jam();
/* -------------------------------------------------- */
/*       INCREMENT THE CURRENT MBYTE                  */
/*       SET PAGE INDEX TO PAGE HEADER SIZE           */
/* -------------------------------------------------- */
    completedLogPage(signal, ZENFORCE_WRITE);
    twnlNewMbyte = logFilePtr.p->currentMbyte + 1;
  }//if
/* -------------------------------------------------- */
/*       CHANGE TO NEW LOG FILE IF NECESSARY          */
/*       UPDATE THE FILE POSITION TO THE NEW MBYTE    */
/*       FOUND IN PAGE PART OF TNEXT_LOG_PTR          */
/*       ALLOCATE AND INITIATE A NEW PAGE SINCE WE    */
/*       HAVE SENT THE PREVIOUS PAGE TO DISK.         */
/*       SET THE NEW NUMBER OF REMAINING WORDS IN THE */
/*       NEW MBYTE ALLOCATED.                         */
/* -------------------------------------------------- */
  logFilePtr.p->currentMbyte = twnlNewMbyte;
  logFilePtr.p->filePosition = twnlNewMbyte * ZPAGES_IN_MBYTE;
  logFilePtr.p->currentFilepage = twnlNewMbyte * ZPAGES_IN_MBYTE;
  logFilePtr.p->remainingWordsInMbyte = twnlRemWords;
  seizeLogpage(signal);
  if (logFilePtr.p->currentMbyte == 0) {
    jam();
    logFilePtr.p->lastPageWritten = 0;
    if (logFilePtr.p->fileNo == 0) {
      jam();
      releaseLogpage(signal);
      logPagePtr.i = logFilePtr.p->logPageZero;
      ptrCheckGuard(logPagePtr, clogPageFileSize, logPageRecord);
    }//if
  }//if
  initLogpage(signal);
  logFilePtr.p->currentLogpage = logPagePtr.i;
  if (logFilePtr.p->currentMbyte == 0) {
    jam();
/* -------------------------------------------------- */
/*       THIS IS A NEW FILE, WRITE THE FILE DESCRIPTOR*/
/*       ALSO OPEN THE NEXT LOG FILE TO ENSURE THAT   */
/*       THIS FILE IS OPEN WHEN ITS TURN COMES.       */
/* -------------------------------------------------- */
    writeFileHeaderOpen(signal, ZNORMAL);
    openNextLogfile(signal);
    logFilePtr.p->fileChangeState = LogFileRecord::BOTH_WRITES_ONGOING;
  }//if
  if (logFilePtr.p->fileNo == logPartPtr.p->logTailFileNo) {
    if (logFilePtr.p->currentMbyte == logPartPtr.p->logTailMbyte) {
      jam();
/* -------------------------------------------------- */
/*       THE HEAD AND TAIL HAS MET. THIS SHOULD NEVER */
/*       OCCUR. CAN HAPPEN IF THE LOCAL CHECKPOINTS   */
/*       TAKE FAR TOO LONG TIME. SO TIMING PROBLEMS   */
/*       CAN INVOKE THIS SYSTEM CRASH. HOWEVER ONLY   */
/*       VERY SERIOUS TIMING PROBLEMS.                */
/* -------------------------------------------------- */
      systemError(signal);
    }//if
  }//if
  if (logFilePtr.p->currentMbyte == (ZNO_MBYTES_IN_FILE - 1)) {
    jam();
    twnlNextMbyte = 0;
    if (logFilePtr.p->fileChangeState != LogFileRecord::NOT_ONGOING) {
      jam();
      logPartPtr.p->logPartState = LogPartRecord::FILE_CHANGE_PROBLEM;
    }//if
    twnlNextFileNo = wnlNextLogFilePtr.p->fileNo;
  } else {
    jam();
    twnlNextMbyte = logFilePtr.p->currentMbyte + 1;
    twnlNextFileNo = logFilePtr.p->fileNo;
  }//if
  if (twnlNextFileNo == logPartPtr.p->logTailFileNo) {
    if (logPartPtr.p->logTailMbyte == twnlNextMbyte) {
      jam();
/* -------------------------------------------------- */
/*       THE NEXT MBYTE WILL BE THE TAIL. WE MUST     */
/*       STOP LOGGING NEW OPERATIONS. THIS OPERATION  */
/*       ALLOWED TO PASS. ALSO COMMIT, NEXT, COMPLETED*/
/*       GCI, ABORT AND FRAGMENT SPLIT IS ALLOWED.    */
/*       OPERATIONS ARE ALLOWED AGAIN WHEN THE TAIL   */
/*       IS MOVED FORWARD AS A RESULT OF A START_LCP  */
/*       _ROUND SIGNAL ARRIVING FROM DBDIH.           */
/* -------------------------------------------------- */
      logPartPtr.p->logPartState = LogPartRecord::TAIL_PROBLEM;
    }//if
  }//if
}//Dblqh::writeNextLog()

void
Dblqh::execDUMP_STATE_ORD(Signal* signal)
{
  DumpStateOrd * const dumpState = (DumpStateOrd *)&signal->theData[0];
  if(dumpState->args[0] == DumpStateOrd::CommitAckMarkersSize){
    infoEvent("LQH: m_commitAckMarkerPool: %d free size: %d",
	      m_commitAckMarkerPool.getNoOfFree(),
	      m_commitAckMarkerPool.getSize());
  }
  if(dumpState->args[0] == DumpStateOrd::CommitAckMarkersDump){
    infoEvent("LQH: m_commitAckMarkerPool: %d free size: %d",
	      m_commitAckMarkerPool.getNoOfFree(),
	      m_commitAckMarkerPool.getSize());
    
    CommitAckMarkerIterator iter;
    for(m_commitAckMarkerHash.first(iter); iter.curr.i != RNIL;
	m_commitAckMarkerHash.next(iter)){
      infoEvent("CommitAckMarker: i = %d (0x%x, 0x%x)"
		" ApiRef: 0x%x apiOprec: 0x%x TcNodeId: %d",
		iter.curr.i,
		iter.curr.p->transid1,
		iter.curr.p->transid2,
		iter.curr.p->apiRef,
		iter.curr.p->apiOprec,
		iter.curr.p->tcNodeId);
    }
  }

  // Dump info about number of log pages
  if(dumpState->args[0] == DumpStateOrd::LqhDumpNoLogPages){
    infoEvent("LQH: Log pages : %d Free: %d",
	      clogPageFileSize,
	      cnoOfLogPages);
  }

  // Dump all defined tables that LQH knowns about
  if(dumpState->args[0] == DumpStateOrd::LqhDumpAllDefinedTabs){
    for(Uint32 i = 0; i<ctabrecFileSize; i++){
      TablerecPtr tabPtr;
      tabPtr.i = i;
      ptrAss(tabPtr, tablerec);
      if(tabPtr.p->tableStatus != Tablerec::NOT_DEFINED){
	infoEvent("Table %d Status: %d Usage: %d",
		  i, tabPtr.p->tableStatus, tabPtr.p->usageCount);
      }
    }
    return;
  }

  // Dump all ScanRecords
  if (dumpState->args[0] == DumpStateOrd::LqhDumpAllScanRec){
    Uint32 recordNo = 0;
    if (signal->length() == 1)
      infoEvent("LQH: Dump all ScanRecords - size: %d",
		cscanrecFileSize);
    else if (signal->length() == 2)
      recordNo = dumpState->args[1];
    else
      return;
    
    dumpState->args[0] = DumpStateOrd::LqhDumpOneScanRec;
    dumpState->args[1] = recordNo;
    execDUMP_STATE_ORD(signal);
    
    if (recordNo < cscanrecFileSize-1){
      dumpState->args[0] = DumpStateOrd::LqhDumpAllScanRec;
      dumpState->args[1] = recordNo+1;
      sendSignal(reference(), GSN_DUMP_STATE_ORD, signal, 2, JBB);
    }
    return;
  }
  
  // Dump all active ScanRecords
  if (dumpState->args[0] == DumpStateOrd::LqhDumpAllActiveScanRec){
    Uint32 recordNo = 0;
    if (signal->length() == 1)
      infoEvent("LQH: Dump active ScanRecord - size: %d",
		cscanrecFileSize);
    else if (signal->length() == 2)
      recordNo = dumpState->args[1];
    else
      return;

    ScanRecordPtr sp;
    sp.i = recordNo;
    c_scanRecordPool.getPtr(scanptr);
    if (sp.p->scanState != ScanRecord::SCAN_FREE){
      dumpState->args[0] = DumpStateOrd::LqhDumpOneScanRec;
      dumpState->args[1] = recordNo;
      execDUMP_STATE_ORD(signal);
    }
    
    if (recordNo < cscanrecFileSize-1){
      dumpState->args[0] = DumpStateOrd::LqhDumpAllActiveScanRec;
      dumpState->args[1] = recordNo+1;
      sendSignal(reference(), GSN_DUMP_STATE_ORD, signal, 2, JBB);
    }
    return;
  }

  if(dumpState->args[0] == DumpStateOrd::LqhDumpOneScanRec){
    Uint32 recordNo = RNIL;
    if (signal->length() == 2)
      recordNo = dumpState->args[1];
    else
      return;

    if (recordNo >= cscanrecFileSize)
      return;

    ScanRecordPtr sp;
    sp.i = recordNo;
    c_scanRecordPool.getPtr(sp);
    infoEvent("Dblqh::ScanRecord[%d]: state=%d, type=%d, "
	      "complStatus=%d, scanNodeId=%d",
	      sp.i,
	      sp.p->scanState,
	      sp.p->scanType,
	      sp.p->scanCompletedStatus,
	      sp.p->scanNodeId);
    infoEvent(" apiBref=0x%x, scanAccPtr=%d",
	      sp.p->scanApiBlockref,
	      sp.p->scanAccPtr);
    infoEvent(" copyptr=%d, ailen=%d, complOps=%d, concurrOps=%d",
	      sp.p->copyPtr,
	      sp.p->scanAiLength,
	      sp.p->m_curr_batch_size_rows,
	      sp.p->m_max_batch_size_rows);
    infoEvent(" errCnt=%d, localFid=%d, schV=%d",
	      sp.p->scanErrorCounter,
	      sp.p->scanLocalFragid,
	      sp.p->scanSchemaVersion);
    infoEvent(" stpid=%d, flag=%d, lhold=%d, lmode=%d, num=%d",
	      sp.p->scanStoredProcId,
	      sp.p->scanFlag,
	      sp.p->scanLockHold,
	      sp.p->scanLockMode,
	      sp.p->scanNumber);
    infoEvent(" relCount=%d, TCwait=%d, TCRec=%d, KIflag=%d",
	      sp.p->scanReleaseCounter,
	      sp.p->scanTcWaiting,
	      sp.p->scanTcrec,
	      sp.p->scanKeyinfoFlag);
    return;
  }
  if(dumpState->args[0] == DumpStateOrd::LqhDumpLcpState){

    infoEvent("== LQH LCP STATE ==");
    infoEvent(" clcpCompletedState=%d, c_lcpId=%d, cnoOfFragsCheckpointed=%d",
	      clcpCompletedState,
	      c_lcpId,
	      cnoOfFragsCheckpointed);

    LcpRecordPtr TlcpPtr;
    // Print information about the current local checkpoint
    TlcpPtr.i = 0;
    ptrAss(TlcpPtr, lcpRecord);
    infoEvent(" lcpState=%d firstLcpLocTup=%d firstLcpLocAcc=%d",
	      TlcpPtr.p->lcpState,
	      TlcpPtr.p->firstLcpLocTup,
	      TlcpPtr.p->firstLcpLocAcc);
    infoEvent(" lcpAccptr=%d lastFragmentFlag=%d",
	      TlcpPtr.p->lcpAccptr,
	      TlcpPtr.p->lastFragmentFlag);
    infoEvent("currentFragment.fragPtrI=%d",
	        TlcpPtr.p->currentFragment.fragPtrI);
    infoEvent("currentFragment.lcpFragOrd.tableId=%d",
	      TlcpPtr.p->currentFragment.lcpFragOrd.tableId);
    infoEvent(" lcpQueued=%d reportEmpty=%d",
	      TlcpPtr.p->lcpQueued,
	      TlcpPtr.p->reportEmpty);
    char buf[8*_NDB_NODE_BITMASK_SIZE+1];
    infoEvent(" m_EMPTY_LCP_REQ=%d", 
	      TlcpPtr.p->m_EMPTY_LCP_REQ.getText(buf));
    
    return;
  }

  Uint32 arg= dumpState->args[0];
  if(arg == 2304 || arg == 2305)
  {
    jam();
    Uint32 i;
    GcpRecordPtr gcp; gcp.i = RNIL;
    for(i = 0; i<4; i++)
    {
      logPartPtr.i = i;
      ptrCheckGuard(logPartPtr, clogPartFileSize, logPartRecord);
      ndbout_c("LP %d state: %d WW_Gci: %d gcprec: %d flq: %d currfile: %d tailFileNo: %d logTailMbyte: %d", 
	       i,
	       logPartPtr.p->logPartState,
	       logPartPtr.p->waitWriteGciLog,
	       logPartPtr.p->gcprec,
	       logPartPtr.p->firstLogQueue,
	       logPartPtr.p->currentLogfile,
	       logPartPtr.p->logTailFileNo,
	       logPartPtr.p->logTailMbyte);
      
      if(gcp.i == RNIL && logPartPtr.p->gcprec != RNIL)
	gcp.i = logPartPtr.p->gcprec;

      LogFileRecordPtr logFilePtr;
      Uint32 first= logFilePtr.i= logPartPtr.p->firstLogfile;
      do
      {
	ptrCheckGuard(logFilePtr, clogFileFileSize, logFileRecord);
	ndbout_c("  file %d(%d) FileChangeState: %d logFileStatus: %d currentMbyte: %d currentFilepage", 
		 logFilePtr.p->fileNo,
		 logFilePtr.i,
		 logFilePtr.p->fileChangeState,
		 logFilePtr.p->logFileStatus,
		 logFilePtr.p->currentMbyte,
		 logFilePtr.p->currentFilepage);
	logFilePtr.i = logFilePtr.p->nextLogFile;
      } while(logFilePtr.i != first);
    }
    
    if(gcp.i != RNIL)
    {
      ptrCheckGuard(gcp, cgcprecFileSize, gcpRecord);
      for(i = 0; i<4; i++)
      {
	ndbout_c("  GCP %d file: %d state: %d sync: %d page: %d word: %d",
		 i, gcp.p->gcpFilePtr[i], gcp.p->gcpLogPartState[i],
		 gcp.p->gcpSyncReady[i],
		 gcp.p->gcpPageNo[i],
		 gcp.p->gcpWordNo[i]);      
      }
    }

    if(arg== 2305)
    {
      progError(__LINE__, ERR_SYSTEM_ERROR, 
		"Shutting down node due to failed handling of GCP_SAVEREQ");
      
    }
  }
}//Dblqh::execDUMP_STATE_ORD()

void Dblqh::execSET_VAR_REQ(Signal* signal) 
{
#if 0
  SetVarReq* const setVarReq = (SetVarReq*)&signal->theData[0];
  ConfigParamId var = setVarReq->variable();

  switch (var) {

  case NoOfConcurrentCheckpointsAfterRestart:
    sendSignal(CMVMI_REF, GSN_SET_VAR_CONF, signal, 1, JBB);
    break;

  case NoOfConcurrentCheckpointsDuringRestart:
    // Valid only during start so value not set.
    sendSignal(CMVMI_REF, GSN_SET_VAR_CONF, signal, 1, JBB);
    break;

  default:
    sendSignal(CMVMI_REF, GSN_SET_VAR_REF, signal, 1, JBB);
  } // switch
#endif
}//execSET_VAR_REQ()


/* **************************************************************** */
/* ---------------------------------------------------------------- */
/* ---------------------- TRIGGER HANDLING ------------------------ */
/* ---------------------------------------------------------------- */
/*                                                                  */
/*      All trigger signals from TRIX are forwarded top TUP         */
/* ---------------------------------------------------------------- */
/* **************************************************************** */

// Trigger signals
void
Dblqh::execCREATE_TRIG_REQ(Signal* signal)
{
  jamEntry();
  NodeId myNodeId = getOwnNodeId();
  BlockReference tupref = calcTupBlockRef(myNodeId);

  sendSignal(tupref, GSN_CREATE_TRIG_REQ, signal, CreateTrigReq::SignalLength, JBB);
}

void
Dblqh::execCREATE_TRIG_CONF(Signal* signal)
{
  jamEntry();
  NodeId myNodeId = getOwnNodeId();
  BlockReference dictref = calcDictBlockRef(myNodeId);

  sendSignal(dictref, GSN_CREATE_TRIG_CONF, signal, CreateTrigConf::SignalLength, JBB);
}

void
Dblqh::execCREATE_TRIG_REF(Signal* signal)
{
  jamEntry();
  NodeId myNodeId = getOwnNodeId();
  BlockReference dictref = calcDictBlockRef(myNodeId);

  sendSignal(dictref, GSN_CREATE_TRIG_REF, signal, CreateTrigRef::SignalLength, JBB);
}

void
Dblqh::execDROP_TRIG_REQ(Signal* signal)
{
  jamEntry();
  NodeId myNodeId = getOwnNodeId();
  BlockReference tupref = calcTupBlockRef(myNodeId);

  sendSignal(tupref, GSN_DROP_TRIG_REQ, signal, DropTrigReq::SignalLength, JBB);
}

void
Dblqh::execDROP_TRIG_CONF(Signal* signal)
{
  jamEntry();
  NodeId myNodeId = getOwnNodeId();
  BlockReference dictref = calcDictBlockRef(myNodeId);

  sendSignal(dictref, GSN_DROP_TRIG_CONF, signal, DropTrigConf::SignalLength, JBB);
}

void
Dblqh::execDROP_TRIG_REF(Signal* signal)
{
  jamEntry();
  NodeId myNodeId = getOwnNodeId();
  BlockReference dictref = calcDictBlockRef(myNodeId);

  sendSignal(dictref, GSN_DROP_TRIG_REF, signal, DropTrigRef::SignalLength, JBB);
}

Uint32 Dblqh::calcPageCheckSum(LogPageRecordPtr logP){
    Uint32 checkSum = 37;
#ifdef VM_TRACE
    for (Uint32 i = (ZPOS_CHECKSUM+1); i<ZPAGE_SIZE; i++)
      checkSum = logP.p->logPageWord[i] ^ checkSum;
#endif
    return checkSum;  
  }

