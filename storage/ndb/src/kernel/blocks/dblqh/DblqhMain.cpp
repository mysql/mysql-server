/*
   Copyright (c) 2003, 2011, Oracle and/or its affiliates. All rights reserved.

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

#define DBLQH_C
#include "Dblqh.hpp"
#include <ndb_limits.h>
#include <md5_hash.hpp>

#include <ndb_version.h>
#include <signaldata/TuxBound.hpp>
#include <signaldata/AccScan.hpp>
#include <signaldata/CopyActive.hpp>
#include <signaldata/CopyFrag.hpp>
#include <signaldata/CreateTrigImpl.hpp>
#include <signaldata/DropTrigImpl.hpp>
#include <signaldata/EmptyLcp.hpp>
#include <signaldata/EventReport.hpp>
#include <signaldata/ExecFragReq.hpp>
#include <signaldata/GCP.hpp>
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

#include <signaldata/CreateTab.hpp>
#include <signaldata/CreateTable.hpp>
#include <signaldata/PrepDropTab.hpp>
#include <signaldata/DropTab.hpp>

#include <signaldata/AlterTab.hpp>
#include <signaldata/AlterTable.hpp>
#include <signaldata/DictTabInfo.hpp>

#include <signaldata/LCP.hpp>
#include <DebuggerNames.hpp>
#include <signaldata/BackupImpl.hpp>
#include <signaldata/RestoreImpl.hpp>
#include <signaldata/KeyInfo.hpp>
#include <signaldata/AttrInfo.hpp>
#include <signaldata/TransIdAI.hpp>
#include <KeyDescriptor.hpp>
#include <signaldata/RouteOrd.hpp>
#include <signaldata/FsRef.hpp>
#include <SectionReader.hpp>
#include <signaldata/SignalDroppedRep.hpp>
#include <signaldata/FsReadWriteReq.hpp>
#include <signaldata/DbinfoScan.hpp>
#include <signaldata/SystemError.hpp>
#include <signaldata/FireTrigOrd.hpp>
#include <NdbEnv.h>

#include "../suma/Suma.hpp"
#include "DblqhCommon.hpp"

/**
 * overload handling...
 * TODO: cleanup...from all sorts of perspective
 */
#include <TransporterRegistry.hpp>

#include <EventLogger.hpp>
extern EventLogger * g_eventLogger;

// Use DEBUG to print messages that should be
// seen only when we debug the product
#ifdef VM_TRACE
#define DEBUG(x) ndbout << "DBLQH: "<< x << endl;
static
NdbOut &
operator<<(NdbOut& out, Dblqh::TcConnectionrec::TransactionState state){
  out << (int)state;
  return out;
}

static
NdbOut &
operator<<(NdbOut& out, Dblqh::TcConnectionrec::LogWriteState state){
  out << (int)state;
  return out;
}

static
NdbOut &
operator<<(NdbOut& out, Dblqh::TcConnectionrec::ListState state){
  out << (int)state;
  return out;
}

static
NdbOut &
operator<<(NdbOut& out, Dblqh::TcConnectionrec::AbortState state){
  out << (int)state;
  return out;
}

static
NdbOut &
operator<<(NdbOut& out, Dblqh::ScanRecord::ScanState state){
  out << (int)state;
  return out;
}

static
NdbOut &
operator<<(NdbOut& out, Dblqh::LogFileOperationRecord::LfoState state){
  out << (int)state;
  return out;
}

static
NdbOut &
operator<<(NdbOut& out, Dblqh::ScanRecord::ScanType state){
  out << (int)state;
  return out;
}

static
NdbOut &
operator<<(NdbOut& out, Operation_t op)
{
  switch(op){
  case ZREAD: out << "READ"; break;
  case ZREAD_EX: out << "READ-EX"; break;
  case ZINSERT: out << "INSERT"; break;
  case ZUPDATE: out << "UPDATE"; break;
  case ZDELETE: out << "DELETE"; break;
  case ZWRITE: out << "WRITE"; break;
  case ZUNLOCK: out << "UNLOCK"; break;
  case ZREFRESH: out << "REFRESH"; break;
  }
  return out;
}

#else
#define DEBUG(x)
#endif

//#define MARKER_TRACE 0
//#define TRACE_SCAN_TAKEOVER 1

#ifdef VM_TRACE
#ifndef NDB_DEBUG_REDO
#define NDB_DEBUG_REDO
#endif
#endif

#ifdef NDB_DEBUG_REDO
static int DEBUG_REDO = 0;
#else
#define DEBUG_REDO 0
#endif

const Uint32 NR_ScanNo = 0;

#ifndef NDBD_TRACENR
#if defined VM_TRACE
#define NDBD_TRACENR
#endif
#endif

#ifdef NDBD_TRACENR
#include <NdbConfig.h>
static NdbOut * tracenrout = 0;
static int TRACENR_FLAG = 0;
#define TRACENR(x) (* tracenrout) << x
#define SET_TRACENR_FLAG TRACENR_FLAG = 1
#define CLEAR_TRACENR_FLAG TRACENR_FLAG = 0
#else
#define TRACENR_FLAG 0
#define TRACENR(x) do { } while(0)
#define SET_TRACENR_FLAG
#define CLEAR_TRACENR_FLAG
#endif

#ifdef NDBD_TRACENR
static NdbOut * traceopout = 0;
#define TRACE_OP(regTcPtr, place) do { if (TRACE_OP_CHECK(regTcPtr)) TRACE_OP_DUMP(regTcPtr, place); } while(0)
#else
#define TRACE_OP(x, y) { (void)x;}
#endif

struct LogPosition
{
  Uint32 m_file_no;
  Uint32 m_mbyte;
};

int
cmp(const LogPosition& pos1, const LogPosition& pos2)
{
  if (pos1.m_file_no > pos2.m_file_no)
    return 1;
  if (pos1.m_file_no < pos2.m_file_no)
    return -1;
  if (pos1.m_mbyte > pos2.m_mbyte)
    return 1;
  if (pos1.m_mbyte < pos2.m_mbyte)
    return -1;

  return 0;
}

/**
 * head - tail
 */
static
Uint64
free_log(const LogPosition& head, const LogPosition& tail, 
         Uint32 cnt, Uint32 size)
{
  Uint64 headmb = head.m_file_no*Uint64(size) + head.m_mbyte;
  Uint64 tailmb = tail.m_file_no*Uint64(size) + tail.m_mbyte;
  if (headmb >= tailmb)
  {
    return (cnt * Uint64(size)) - headmb + tailmb;
  }
  else
  {
    return tailmb - headmb;
  }
}

/* ------------------------------------------------------------------------- */
/* -------               SEND SYSTEM ERROR                           ------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void Dblqh::systemError(Signal* signal, int line)
{
  signal->theData[0] = 2304;
  execDUMP_STATE_ORD(signal);
  progError(line, NDBD_EXIT_NDBREQUIRE);
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
    ndbout << "abortState = " << tcConnectptr.p->abortState;
    ndbout << "listState = " << tcConnectptr.p->listState;
    ndbout << endl;
    return;
  }//if
#endif
  LogPartRecordPtr save;
  switch (tcase) {
  case ZLOG_LQHKEYREQ:
    if (cnoOfLogPages == 0) {
      jam();
  busywait:
      sendSignalWithDelay(cownref, GSN_CONTINUEB, signal, 10, 2);
      return;
    }//if
    logPartPtr.i = data0;
    ptrCheckGuard(logPartPtr, clogPartFileSize, logPartRecord);
    save = logPartPtr;

    logPartPtr.p->LogLqhKeyReqSent = ZFALSE;

    if (logPartPtr.p->waitWriteGciLog == LogPartRecord::WWGL_TRUE)
    {
      jam();
      goto startnext;
    }
    if (logPartPtr.p->m_log_complete_queue.isEmpty())
    {
      jam();
      /**
       * prepare is first in queue...check that it's ok to rock'n'roll
       */
      if (logPartPtr.p->m_log_problems != 0)
      {
        /**
         * It will be restarted when problems are cleared...
         */
        jam();
        return;
      }

      if (cnoOfLogPages < ZMIN_LOG_PAGES_OPERATION)
      {
        jam();
        logPartPtr.p->LogLqhKeyReqSent = ZTRUE;
        goto busywait;
      }
    }

    logFilePtr.i = logPartPtr.p->currentLogfile;
    ptrCheckGuard(logFilePtr, clogFileFileSize, logFileRecord);
    logPagePtr.i = logFilePtr.p->currentLogpage;
    ptrCheckGuard(logPagePtr, clogPageFileSize, logPageRecord);

    getFirstInLogQueue(signal, tcConnectptr);
    fragptr.i = tcConnectptr.p->fragmentptr;
    c_fragment_pool.getPtr(fragptr);

    // so that operation can continue...
    ndbrequire(logPartPtr.p->logPartState == LogPartRecord::ACTIVE);
    logPartPtr.p->logPartState = LogPartRecord::IDLE;
    switch (tcConnectptr.p->transactionState) {
    case TcConnectionrec::LOG_QUEUED:
      if (tcConnectptr.p->abortState != TcConnectionrec::ABORT_IDLE)
      {
        jam();
        abortCommonLab(signal);
      }
      else
      {
        jam();
        logLqhkeyreqLab(signal);
      }
      break;
    case TcConnectionrec::LOG_ABORT_QUEUED:
      jam();
      writeAbortLog(signal);
      removeLogTcrec(signal);
      continueAfterLogAbortWriteLab(signal);
      break;
    case TcConnectionrec::LOG_COMMIT_QUEUED:
    case TcConnectionrec::LOG_COMMIT_QUEUED_WAIT_SIGNAL:
      jam();
      writeCommitLog(signal, logPartPtr);
      if (tcConnectptr.p->transactionState == TcConnectionrec::LOG_COMMIT_QUEUED) {
        if (tcConnectptr.p->seqNoReplica == 0 ||
	    tcConnectptr.p->activeCreat == Fragrecord::AC_NR_COPY)
        {
          jam();
          localCommitLab(signal);
        }
        else
        {
          jam();
          commitReplyLab(signal);
        }
      }
      else
      {
        jam();
        tcConnectptr.p->transactionState = TcConnectionrec::LOG_COMMIT_WRITTEN_WAIT_SIGNAL;
      }
      break;
    case TcConnectionrec::COMMIT_QUEUED:
      jam();
      localCommitLab(signal);
      break;
    case TcConnectionrec::ABORT_QUEUED:
      jam();
      abortCommonLab(signal);
      break;
    default:
      ndbrequire(false);
      break;
    }//switch

    /**
     * LogFile/LogPage could have altered due to above
     */
  startnext:
    logPartPtr = save;
    logFilePtr.i = logPartPtr.p->currentLogfile;
    ptrCheckGuard(logFilePtr, clogFileFileSize, logFileRecord);
    logPagePtr.i = logFilePtr.p->currentLogpage;
    ptrCheckGuard(logPagePtr, clogPageFileSize, logPageRecord);
    logNextStart(signal);
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
    startTimeSupervision(signal);
    return;
    break;
  case ZCHECK_LCP_STOP_BLOCKED:
    jam();
    c_scanRecordPool.getPtr(scanptr, data0);
    tcConnectptr.i = scanptr.p->scanTcrec;
    ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
    fragptr.i = tcConnectptr.p->fragmentptr;
    c_fragment_pool.getPtr(fragptr);
    checkLcpStopBlockedLab(signal);
    return;
  case ZSCAN_MARKERS:
    jam();
    scanMarkers(signal, data0, data1, data2);
    return;
    break;

  case ZOPERATION_EVENT_REP:
    jam();
    /* Send counter event report */
    {
      const Uint32 len = c_Counters.build_event_rep(signal);
      sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, len, JBB);
    }

    {
      const Uint32 report_interval = 5000;
      const Uint32 len = c_Counters.build_continueB(signal);
      signal->theData[0] = ZOPERATION_EVENT_REP;
      sendSignalWithDelay(cownref, GSN_CONTINUEB, signal,
                          report_interval, len);
    }
    break;
  case ZDROP_TABLE_WAIT_USAGE:
    jam();
    dropTab_wait_usage(signal);
    return;
    break;
  case ZENABLE_EXPAND_CHECK:
  {
    jam();
    fragptr.i = signal->theData[1];
    if (fragptr.i != RNIL)
    {
      jam();
      c_lcp_complete_fragments.getPtr(fragptr);
      Ptr<Fragrecord> save = fragptr;

      c_lcp_complete_fragments.next(fragptr);
      signal->theData[0] = ZENABLE_EXPAND_CHECK;
      signal->theData[1] = fragptr.i;
      sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);	

      c_lcp_complete_fragments.remove(save);
      return;
    }
    else
    {
      jam();
      cstartRecReq = SRR_REDO_COMPLETE;
      ndbrequire(c_lcp_complete_fragments.isEmpty());

      rebuildOrderedIndexes(signal, 0);
      return;
    }
  }
  case ZRETRY_TCKEYREF:
  {
    jam();
    Uint32 cnt = signal->theData[1];
    Uint32 ref = signal->theData[2];
    if (cnt < (10 * 60 * 5))
    {
      jam();
      /**
       * Only retry for 5 minutes...then hope that API has handled it..somehow
       */
      memmove(signal->theData, signal->theData+3, 4*TcKeyRef::SignalLength);
      sendTCKEYREF(signal, ref, 0, cnt);
    }
    return;
  }
  case ZWAIT_REORG_SUMA_FILTER_ENABLED:
    jam();
    wait_reorg_suma_filter_enabled(signal);
    return;
  case ZREBUILD_ORDERED_INDEXES:
  {
    Uint32 tableId = signal->theData[1];
    rebuildOrderedIndexes(signal, tableId);
    return;
  }
  case ZWAIT_READONLY:
  {
    jam();
    wait_readonly(signal);
    return;
  }
  case ZLCP_FRAG_WATCHDOG:
  {
    jam();
    checkLcpFragWatchdog(signal);
    return;
  }
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

  {
    HostRecordPtr Thostptr;
    Thostptr.i = nodeId;
    ptrCheckGuard(Thostptr, chostFileSize, hostRecord);
    Thostptr.p->nodestatus = ZNODE_UP;
  }

  signal->theData[0] = nodeId;
  signal->theData[1] = cownref; 
  sendSignal(retRef, GSN_INCL_NODECONF, signal, 2, JBB);
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
#if defined VM_TRACE || defined ERROR_INSERT || defined NDBD_TRACENR
  char *name;
  FILE *out = 0;
#endif
  switch (tstartPhase) {
  case ZSTART_PHASE1:
    jam();
    cstartPhase = tstartPhase;
    c_tup = (Dbtup*)globalData.getBlock(DBTUP, instance());
    c_acc = (Dbacc*)globalData.getBlock(DBACC, instance());
    c_lgman = (Lgman*)globalData.getBlock(LGMAN);
    ndbrequire(c_tup != 0 && c_acc != 0 && c_lgman != 0);
    sendsttorryLab(signal);
    
#ifdef NDBD_TRACENR
#ifdef VM_TRACE
    out = globalSignalLoggers.getOutputStream();
#endif
    if (out == 0) {
      name = NdbConfig_SignalLogFileName(getOwnNodeId());
      out = fopen(name, "a");
    }
    tracenrout = new NdbOut(* new FileOutputStream(out));
#endif

#ifdef NDBD_TRACENR
    traceopout = &ndbout;
#endif
    
#ifdef NDB_DEBUG_REDO
    {
      char buf[100];
      if (NdbEnv_GetEnv("NDB_DEBUG_REDO", buf, sizeof(buf)))
      {
        DEBUG_REDO = 1;
      }
    }
#endif
    return;
    break;
  case 4:
    jam();
    define_backup(signal);
    break;
  default:
    jam();
    /*empty*/;
    sendsttorryLab(signal);
    return;
    break;
  }//switch
}//Dblqh::execSTTOR()

void
Dblqh::define_backup(Signal* signal)
{
  DefineBackupReq * req = (DefineBackupReq*)signal->getDataPtrSend();
  req->backupId = 0;
  req->clientRef = 0;
  req->clientData = 0;
  req->senderRef = reference();
  req->backupPtr = 0;
  req->backupKey[0] = 0;
  req->backupKey[1] = 0;
  req->nodes.clear();
  req->nodes.set(getOwnNodeId());
  req->backupDataLen = ~0;

  BlockReference backupRef = calcInstanceBlockRef(BACKUP);
  sendSignal(backupRef, GSN_DEFINE_BACKUP_REQ, signal, 
	     DefineBackupReq::SignalLength, JBB);
}

void
Dblqh::execDEFINE_BACKUP_REF(Signal* signal)
{
  jamEntry();
  m_backup_ptr = RNIL;
  DefineBackupRef* ref = (DefineBackupRef*)signal->getDataPtrSend();
  int err_code = 0;
  char * extra_msg = NULL;

  switch(ref->errorCode){
    case DefineBackupRef::Undefined:
    case DefineBackupRef::FailedToSetupFsBuffers:
    case DefineBackupRef::FailedToAllocateBuffers: 
    case DefineBackupRef::FailedToAllocateTables: 
    case DefineBackupRef::FailedAllocateTableMem: 
    case DefineBackupRef::FailedToAllocateFileRecord:
    case DefineBackupRef::FailedToAllocateAttributeRecord:
    case DefineBackupRef::FailedInsertFileHeader: 
    case DefineBackupRef::FailedInsertTableList: 
      jam();
      err_code = NDBD_EXIT_INVALID_CONFIG;
      extra_msg = (char*) "Probably Backup parameters configuration error, Please consult the manual";
      progError(__LINE__, err_code, extra_msg);
  }

  sendsttorryLab(signal);
}

void
Dblqh::execDEFINE_BACKUP_CONF(Signal* signal)
{
  jamEntry();
  DefineBackupConf * conf = (DefineBackupConf*)signal->getDataPtrSend();
  m_backup_ptr = conf->backupPtr;
  sendsttorryLab(signal);
}

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
    // Dont LqhKeyReq::setApplicationAddressFlag
    LqhKeyReq::setDirtyFlag(preComputedRequestInfoMask, 1);
    // Dont LqhKeyReq::setInterpretedFlag
    LqhKeyReq::setSimpleFlag(preComputedRequestInfoMask, 1);
    LqhKeyReq::setOperation(preComputedRequestInfoMask, RI_OPERATION_MASK);
    LqhKeyReq::setGCIFlag(preComputedRequestInfoMask, 1);
    LqhKeyReq::setNrCopyFlag(preComputedRequestInfoMask, 1);
    // Dont setAIInLqhKeyReq
    // Dont setSeqNoReplica
    // Dont setSameClientAndTcFlag
    // Dont setReturnedReadLenAIFlag
    // Dont setAPIVersion
    LqhKeyReq::setMarkerFlag(preComputedRequestInfoMask, 1);
    LqhKeyReq::setQueueOnRedoProblemFlag(preComputedRequestInfoMask, 1);
    //preComputedRequestInfoMask = 0x003d7fff;
    startphase1Lab(signal, /* dummy */ ~0, ownNodeId);

    {
      /* Start counter activity event reporting. */
      const Uint32 len = c_Counters.build_continueB(signal);
      signal->theData[0] = ZOPERATION_EVENT_REP;
      sendSignalWithDelay(cownref, GSN_CONTINUEB, signal, 10, len);
    }
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

/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
/* +++++++                         START PHASE 2                    +++++++ */
/*                                                                          */
/*             INITIATE ALL RECORDS WITHIN THE BLOCK                        */
/* ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++ */
void Dblqh::startphase1Lab(Signal* signal, Uint32 _dummy, Uint32 ownNodeId) 
{
  UintR Ti, Tj;
  HostRecordPtr ThostPtr;

/* ------- INITIATE ALL RECORDS ------- */
  cownNodeid    = ownNodeId;
  caccBlockref  = calcInstanceBlockRef(DBACC);
  ctupBlockref  = calcInstanceBlockRef(DBTUP);
  ctuxBlockref  = calcInstanceBlockRef(DBTUX);
  cownref       = calcInstanceBlockRef(DBLQH);
  ndbassert(cownref == reference());
  for (Ti = 0; Ti < chostFileSize; Ti++) {
    ThostPtr.i = Ti;
    ptrCheckGuard(ThostPtr, chostFileSize, hostRecord);
    /*
     * Valid only if receiver has same number of LQH workers.
     * In general full instance key of fragment must be used.
     */
    ThostPtr.p->inPackedList = false;
    for (Tj = 0; Tj < NDB_ARRAY_SIZE(ThostPtr.p->lqh_pack); Tj++)
    {
      ThostPtr.p->lqh_pack[Tj].noOfPackedWords = 0;
      ThostPtr.p->lqh_pack[Tj].hostBlockRef =
        numberToRef(DBLQH, Tj, ThostPtr.i);
    }
    for (Tj = 0; Tj < NDB_ARRAY_SIZE(ThostPtr.p->tc_pack); Tj++)
    {
      ThostPtr.p->tc_pack[Tj].noOfPackedWords = 0;
      ThostPtr.p->tc_pack[Tj].hostBlockRef =
        numberToRef(DBTC, Tj, ThostPtr.i);
    }
    ThostPtr.p->nodestatus = ZNODE_DOWN;
  }//for
  cpackedListIndex = 0;

  bool do_init =
    (cstartType == NodeState::ST_INITIAL_START) ||
    (cstartType == NodeState::ST_INITIAL_NODE_RESTART);

  LogFileRecordPtr prevLogFilePtr;
  LogFileRecordPtr zeroLogFilePtr;

  ndbrequire(cnoLogFiles != 0);
  for (logPartPtr.i = 0; logPartPtr.i < clogPartFileSize; logPartPtr.i++)
  {
    jam();
    ptrAss(logPartPtr, logPartRecord);
    initLogpart(signal);
    for (Uint32 fileNo = 0; fileNo < cnoLogFiles; fileNo++)
    {
      seizeLogfile(signal);
      if (fileNo != 0)
      {
        jam();
        prevLogFilePtr.p->nextLogFile = logFilePtr.i;
        logFilePtr.p->prevLogFile = prevLogFilePtr.i;
      }
      else
      {
        jam();
        logPartPtr.p->firstLogfile = logFilePtr.i;
        logPartPtr.p->currentLogfile = logFilePtr.i;
        zeroLogFilePtr.i = logFilePtr.i;
        zeroLogFilePtr.p = logFilePtr.p;
      }//if
      prevLogFilePtr.i = logFilePtr.i;
      prevLogFilePtr.p = logFilePtr.p;
      initLogfile(signal, fileNo);
      if (do_init)
      {
        jam();
        if (logFilePtr.i == zeroLogFilePtr.i)
        {
          jam();
/* ------------------------------------------------------------------------- */
/*IN AN INITIAL START WE START BY CREATING ALL LOG FILES AND SETTING THEIR   */
/*PROPER SIZE AND INITIALISING PAGE ZERO IN ALL FILES.                       */
/*WE START BY CREATING FILE ZERO IN EACH LOG PART AND THEN PROCEED           */
/*SEQUENTIALLY THROUGH ALL LOG FILES IN THE LOG PART.                        */
/* ------------------------------------------------------------------------- */
          if (m_use_om_init == 0 || logPartPtr.i == 0)
          {
            /**
             * initialize one file at a time if using OM_INIT
             */
            jam();
#ifdef VM_TRACE
            if (m_use_om_init)
            {
              jam();
              /**
               * FSWRITEREQ does cross-thread execute-direct
               *   which makes the clear_global_variables "unsafe"
               *   disable it until we're finished with init log-files
               */
              disable_global_variables();
            }
#endif
            openLogfileInit(signal);
          }
        }//if
      }//if
    }//for
    zeroLogFilePtr.p->prevLogFile = logFilePtr.i;
    logFilePtr.p->nextLogFile = zeroLogFilePtr.i;
  }

  initReportStatus(signal);
  if (!do_init)
  {
    jam();
    sendNdbSttorryLab(signal);
  }
  else
  {
    reportStatus(signal);
  }

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
  caddNodeState = ZTRUE;
/* ***************<< */
/*  READ_NODESREQ  < */
/* ***************<< */
  cinitialStartOngoing = ZTRUE;

  switch(cstartType){
  case NodeState::ST_NODE_RESTART:
  case NodeState::ST_SYSTEM_RESTART:
    jam();
    for (logPartPtr.i = 0; logPartPtr.i < clogPartFileSize; logPartPtr.i++)
    {
      jam();
      LogFileRecordPtr locLogFilePtr;
      ptrAss(logPartPtr, logPartRecord);
      locLogFilePtr.i = logPartPtr.p->firstLogfile;
      ptrCheckGuard(locLogFilePtr, clogFileFileSize, logFileRecord);
      locLogFilePtr.p->logFileStatus = LogFileRecord::OPEN_SR_FRONTPAGE;
      openFileRw(signal, locLogFilePtr);
    }//for
    break;
  case NodeState::ST_INITIAL_START:
  case NodeState::ST_INITIAL_NODE_RESTART:
    jam();
    for (logPartPtr.i = 0; logPartPtr.i < clogPartFileSize; logPartPtr.i++)
    {
      jam();
      signal->theData[0] = ZINIT_FOURTH;
      signal->theData[1] = logPartPtr.i;
      sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
    }
    break;
  }

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
    if (NdbNodeBitmask::get(readNodes->allNodes, i)) {
      jam();
      cnodeData[ind]    = i;
      cnodeStatus[ind]  = NdbNodeBitmask::get(readNodes->inactiveNodes, i);

      {
        HostRecordPtr Thostptr;
        Thostptr.i = i;
        ptrCheckGuard(Thostptr, chostFileSize, hostRecord);
        Thostptr.p->nodestatus = cnodeStatus[ind];
      }

      //readNodes->getVersionId(i, readNodes->theVersionIds) not used
      if (!NodeBitmask::get(readNodes->inactiveNodes, i))
      {
	jam();
	m_sr_nodes.set(i);
      }
      ind++;
    }//if
  }//for
  ndbrequire(ind == cnoOfNodes);
  ndbrequire(cnoOfNodes >= 1 && cnoOfNodes < MAX_NDB_NODES);
  ndbrequire(!(cnoOfNodes == 1 && cstartType == NodeState::ST_NODE_RESTART));

#ifdef ERROR_INSERT
  c_master_node_id = readNodes->masterNodeId;
#endif
  
  caddNodeState = ZFALSE;
  if (cstartType == NodeState::ST_SYSTEM_RESTART) 
  {
    jam();
    sendNdbSttorryLab(signal);
    return;
  } 
  else if (cstartType == NodeState::ST_NODE_RESTART)
  {
    jam();
    SET_TRACENR_FLAG;
    m_sr_nodes.clear();
    m_sr_nodes.set(getOwnNodeId());
    sendNdbSttorryLab(signal);
    return;
  }
  SET_TRACENR_FLAG;
  
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
  CLEAR_TRACENR_FLAG;
  sendNdbSttorryLab(signal);
  return;
}//Dblqh::startphase6Lab()

void Dblqh::sendNdbSttorryLab(Signal* signal) 
{
  signal->theData[0] = cownref;
  BlockReference cntrRef = !isNdbMtLqh() ? NDBCNTR_REF : DBLQH_REF;
  sendSignal(cntrRef, GSN_NDB_STTORRY, signal, 1, JBB);
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
  signal->theData[4] = 4;
  signal->theData[5] = 255;
  BlockReference cntrRef = !isNdbMtLqh() ? NDBCNTR_REF : DBLQH_REF;
  sendSignal(cntrRef, GSN_STTORRY, signal, 6, JBB);
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
    m_ctx.m_config.getOwnConfigIterator();
  ndbrequire(p != 0);


  /**
   * TODO move check of log-parts vs. ndbMtLqhWorkers to better place
   * (Configuration.cpp ??)
   */
  ndbrequire(globalData.ndbLogParts <= NDB_MAX_LOG_PARTS);
  if (globalData.ndbMtLqhWorkers > globalData.ndbLogParts)
  {
    char buf[255];
    BaseString::snprintf(buf, sizeof(buf),
      "Trying to start %d LQH workers with only %d log parts, try initial"
      " node restart to be able to use more LQH workers.",
      globalData.ndbMtLqhWorkers, globalData.ndbLogParts);
    progError(__LINE__, NDBD_EXIT_INVALID_CONFIG, buf);
  }

  if (globalData.ndbLogParts != 4 &&
      globalData.ndbLogParts != 8 &&
      globalData.ndbLogParts != 12 &&
      globalData.ndbLogParts != 16)
  {
    char buf[255];
    BaseString::snprintf(buf, sizeof(buf),
      "Trying to start with %d log parts, number of log parts can"
      " only be set to 4, 8, 12 or 16.",
      globalData.ndbLogParts);
    progError(__LINE__, NDBD_EXIT_INVALID_CONFIG, buf);
  }

  cnoLogFiles = 8;
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_DB_NO_REDOLOG_FILES, 
					&cnoLogFiles));
  ndbrequire(cnoLogFiles > 0);

  Uint32 log_page_size= 0;
  ndb_mgm_get_int_parameter(p, CFG_DB_REDO_BUFFER,  
			    &log_page_size);

  /**
   * Always set page size in half MBytes
   */
  clogPageFileSize= (log_page_size / sizeof(LogPageRecord));
  Uint32 mega_byte_part= clogPageFileSize & 15;
  if (mega_byte_part != 0) {
    jam();
    clogPageFileSize+= (16 - mega_byte_part);
  }

  /* maximum number of log file operations */
  clfoFileSize = clogPageFileSize;
  if (clfoFileSize < ZLFO_MIN_FILE_SIZE)
    clfoFileSize = ZLFO_MIN_FILE_SIZE;

  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_LQH_TABLE, &ctabrecFileSize));
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_LQH_TC_CONNECT, 
					&ctcConnectrecFileSize));
  clogFileFileSize = clogPartFileSize * cnoLogFiles;
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_LQH_SCAN, &cscanrecFileSize));
  cmaxAccOps = cscanrecFileSize * MAX_PARALLEL_OP_PER_SCAN;

  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_DB_DISCLESS, &c_diskless));
  c_o_direct = true;
  ndb_mgm_get_int_parameter(p, CFG_DB_O_DIRECT, &c_o_direct);
  
  m_use_om_init = 0;
  {
    const char * conf = 0;
    if (!ndb_mgm_get_string_parameter(p, CFG_DB_INIT_REDO, &conf) && conf)
    {
      jam();
      if (strcasecmp(conf, "sparse") == 0)
      {
        jam();
        m_use_om_init = 0;
      }
      else if (strcasecmp(conf, "full") == 0)
      {
        jam();
        m_use_om_init = 1;
      }
    }
  }

  Uint32 tmp= 0;
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_LQH_FRAG, &tmp));
  c_fragment_pool.setSize(tmp);

  if (!ndb_mgm_get_int_parameter(p, CFG_DB_REDOLOG_FILE_SIZE,
                                 &clogFileSize))
  {
    // convert to mbyte
    clogFileSize = (clogFileSize + 1024*1024 - 1) / (1024 * 1024);
    ndbrequire(clogFileSize >= 4 && clogFileSize <= 1024);
  }

  m_startup_report_frequency = 0;
  ndb_mgm_get_int_parameter(p,CFG_DB_STARTUP_REPORT_FREQUENCY,
                            &m_startup_report_frequency);
  totalLogFiles = 4 * cnoLogFiles;
  totallogMBytes = totalLogFiles * clogFileSize;

  cmaxLogFilesInPageZero = (ZPAGE_SIZE - ZPAGE_HEADER_SIZE - 128) /
    (ZFD_MBYTE_SIZE * clogFileSize);

  /**
   * "Old" cmaxLogFilesInPageZero was 40
   * Each FD need 3 words per mb, require that they can fit into 1 page
   *   (atleast 1 FD)
   * Is also checked in ConfigInfo.cpp (max FragmentLogFileSize = 1Gb)
   *   1Gb = 1024Mb => 3(ZFD_MBYTE_SIZE) * 1024 < 8192 (ZPAGE_SIZE)
   */
  if (cmaxLogFilesInPageZero > 40)
  {
    jam();
    cmaxLogFilesInPageZero = 40;
  }
  else
  {
    ndbrequire(cmaxLogFilesInPageZero);
  }

#if defined VM_TRACE || defined ERROR_INSERT
  if (cmaxLogFilesInPageZero_DUMP != 0)
  {
    ndbout << "LQH DUMP 2396 " << cmaxLogFilesInPageZero_DUMP;
    if (cmaxLogFilesInPageZero_DUMP > cmaxLogFilesInPageZero)
    {
      ndbout << ": max allowed is " << cmaxLogFilesInPageZero << endl;
      // do not continue with useless test
      ndbrequire(false);
    }
    cmaxLogFilesInPageZero = cmaxLogFilesInPageZero_DUMP;
    ndbout << endl;
  }
#endif

  /* How many file's worth of info is actually valid? */
  cmaxValidLogFilesInPageZero = cmaxLogFilesInPageZero - 1;

  /* Must be at least 1 */
  ndbrequire(cmaxValidLogFilesInPageZero > 0);

   {
    Uint32 config_val = 20;
    ndb_mgm_get_int_parameter(p, CFG_DB_LCP_INTERVAL, &config_val);
    config_val = config_val > 31 ? 31 : config_val;

    const Uint32 mb = 1024 * 1024;
    
    // perform LCP after this amout of mbytes written
    const Uint64 config_mbytes = ((Uint64(4) << config_val) + mb - 1) / mb;
    const Uint64 totalmb = Uint64(cnoLogFiles) * Uint64(clogFileSize);
    if (totalmb > config_mbytes)
    {
      c_free_mb_force_lcp_limit = Uint32(totalmb - config_mbytes);
    }
    else
    {
      c_free_mb_force_lcp_limit = 0;
    }

    // No less than 33%
    Uint32 limit = Uint32(totalmb / 3);
    if (c_free_mb_force_lcp_limit < limit)
    {
      c_free_mb_force_lcp_limit = limit;
    }
  }
  c_free_mb_tail_problem_limit = 4;  // If less than 4Mb set TAIL_PROBLEM

  ndb_mgm_get_int_parameter(p, CFG_DB_TRANSACTION_DEADLOCK_TIMEOUT, 
                            &cTransactionDeadlockDetectionTimeout);
  
  initRecords();
  initialiseRecordsLab(signal, 0, ref, senderData);

  c_max_redo_lag = 30;
  ndb_mgm_get_int_parameter(p, CFG_DB_REDO_OVERCOMMIT_LIMIT,
                            &c_max_redo_lag);

  c_max_redo_lag_counter = 3;
  ndb_mgm_get_int_parameter(p, CFG_DB_REDO_OVERCOMMIT_COUNTER,
                            &c_max_redo_lag_counter);

  c_max_parallel_scans_per_frag = 32;
  ndb_mgm_get_int_parameter(p, CFG_DB_PARALLEL_SCANS_PER_FRAG,
                            &c_max_parallel_scans_per_frag);

  if (c_max_parallel_scans_per_frag > (256 - MAX_PARALLEL_SCANS_PER_FRAG) / 2)
  {
    jam();
    c_max_parallel_scans_per_frag = (256 - MAX_PARALLEL_SCANS_PER_FRAG) / 2;
  }
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
void
Dblqh::execCREATE_TAB_REQ(Signal* signal)
{
  CreateTabReq* req = (CreateTabReq*)signal->getDataPtr();
  tabptr.i = req->tableId;
  ptrCheckGuard(tabptr, ctabrecFileSize, tablerec);

  Uint32 senderRef = req->senderRef;
  Uint32 senderData = req->senderData;

  if (tabptr.p->tableStatus != Tablerec::NOT_DEFINED)
  {
    jam();
    CreateTabRef* ref = (CreateTabRef*)signal->getDataPtrSend();
    ref->senderData = senderData;
    ref->senderRef = reference();
    ref->errorCode = CreateTableRef::TableAlreadyExist;
    sendSignal(senderRef, GSN_CREATE_TAB_REF, signal,
               CreateTabRef::SignalLength, JBB);
    return;
  }

  seizeAddfragrec(signal);
  addfragptr.p->m_createTabReq = *req;
  req = &addfragptr.p->m_createTabReq;

  tabptr.p->tableStatus = Tablerec::ADD_TABLE_ONGOING;
  tabptr.p->tableType = req->tableType;
  tabptr.p->primaryTableId = (req->primaryTableId == RNIL ? tabptr.i :
                              req->primaryTableId);
  tabptr.p->schemaVersion = req->tableVersion;
  tabptr.p->m_disk_table= 0;

  addfragptr.p->addfragStatus = AddFragRecord::WAIT_TUP;
  sendCreateTabReq(signal, addfragptr);
}

void
Dblqh::sendCreateTabReq(Signal* signal, AddFragRecordPtr addfragptr)
{
  TablerecPtr tabPtr;
  tabPtr.i = addfragptr.p->m_createTabReq.tableId;
  ptrCheckGuard(tabPtr, ctabrecFileSize, tablerec);

  CreateTabReq* req = (CreateTabReq*)signal->getDataPtrSend();
  * req = addfragptr.p->m_createTabReq;

  req->senderRef = reference();
  req->senderData = addfragptr.i;

  Uint32 ref = calcInstanceBlockRef(DBTUP);
  switch(addfragptr.p->addfragStatus){
  case AddFragRecord::WAIT_TUP:
    if (DictTabInfo::isOrderedIndex(tabPtr.p->tableType))
    {
      jam();
      req->noOfAttributes = 1;
      req->noOfKeyAttr = 1;
      req->noOfNullAttributes = 0;
    }
    break;
  case AddFragRecord::WAIT_TUX:
    jam();
    ndbrequire(req->noOfAttributes >= 2);
    req->noOfAttributes--;
    ref = calcInstanceBlockRef(DBTUX);
    break;
  default:
    jamLine(addfragptr.p->addfragStatus);
    ndbrequire(false);
  }

  sendSignal(ref, GSN_CREATE_TAB_REQ, signal,
             CreateTabReq::SignalLengthLDM, JBB);
}

void
Dblqh::execCREATE_TAB_REF(Signal* signal)
{
  jamEntry();

  CreateTabRef * ref = (CreateTabRef*)signal->getDataPtr();
  addfragptr.i = ref->senderData;
  ptrCheckGuard(addfragptr, caddfragrecFileSize, addFragRecord);

  abortAddFragOps(signal);

  ref->senderRef = reference();
  ref->senderData = addfragptr.p->m_createTabReq.senderData;
  sendSignal(addfragptr.p->m_createTabReq.senderRef,
             GSN_CREATE_TAB_REF, signal, CreateTabConf::SignalLength, JBB);

  releaseAddfragrec(signal);
}

void
Dblqh::execCREATE_TAB_CONF(Signal* signal)
{
  jamEntry();
  CreateTabConf* conf = (CreateTabConf*)signal->getDataPtr();
  addfragptr.i = conf->senderData;
  ptrCheckGuard(addfragptr, caddfragrecFileSize, addFragRecord);

  TablerecPtr tabPtr;
  tabPtr.i = addfragptr.p->m_createTabReq.tableId;
  ptrCheckGuard(tabPtr, ctabrecFileSize, tablerec);

  switch(addfragptr.p->addfragStatus){
  case AddFragRecord::WAIT_TUP:
    jam();
    addfragptr.p->tupConnectptr = conf->tupConnectPtr;
    if (DictTabInfo::isOrderedIndex(tabPtr.p->tableType))
    {
      jam();
      addfragptr.p->addfragStatus = AddFragRecord::WAIT_TUX;
      sendCreateTabReq(signal, addfragptr);
      return;
    }
    break;
  case AddFragRecord::WAIT_TUX:
    jam();
    addfragptr.p->tuxConnectptr = conf->tuxConnectPtr;
    break;
  default:
    jamLine(addfragptr.p->addfragStatus);
    ndbrequire(false);
  }

  addfragptr.p->addfragStatus = AddFragRecord::WAIT_ADD_ATTR;

  conf->senderRef = reference();
  conf->senderData = addfragptr.p->m_createTabReq.senderData;
  conf->lqhConnectPtr = addfragptr.i;
  sendSignal(addfragptr.p->m_createTabReq.senderRef,
             GSN_CREATE_TAB_CONF, signal, CreateTabConf::SignalLength, JBB);
}

/* ************************************************************************> */
/*  LQHADDATTRREQ: Request from DICT to create attributes for the new table. */
/* ************************************************************************> */
void Dblqh::execLQHADDATTREQ(Signal* signal)
{
  jamEntry();
  LqhAddAttrReq * req = (LqhAddAttrReq*)signal->getDataPtr();

  addfragptr.i = req->lqhFragPtr;
  ptrCheckGuard(addfragptr, caddfragrecFileSize, addFragRecord);

  addfragptr.p->m_addAttrReq = * req;

  const Uint32 tnoOfAttr = req->noOfAttributes;
  const Uint32 numSections = signal->getNoOfSections();
  bool isLongReq= ( numSections != 0 );
  addfragptr.p->defValSectionI = RNIL;
  addfragptr.p->defValNextPos = 0;

  if (isLongReq)
  {
    SectionHandle handle(this, signal);
    SegmentedSectionPtr defValSection;
    handle.getSection(defValSection, LqhAddAttrReq::DEFAULT_VALUE_SECTION_NUM);
    addfragptr.p->defValSectionI = defValSection.i;
    addfragptr.p->defValNextPos = 0;
    //Don't free Section here. Section is freed after default values are trasfered to TUP
    handle.clear();
  }

  ndbrequire(addfragptr.p->addfragStatus == AddFragRecord::WAIT_ADD_ATTR);
  ndbrequire((tnoOfAttr != 0) && (tnoOfAttr <= LqhAddAttrReq::MAX_ATTRIBUTES));
  addfragptr.p->totalAttrReceived += tnoOfAttr;
  ndbrequire(addfragptr.p->totalAttrReceived <=
             addfragptr.p->m_createTabReq.noOfAttributes);

  addfragptr.p->attrReceived = tnoOfAttr;

  TablerecPtr tabPtr;
  tabPtr.i = addfragptr.p->m_createTabReq.tableId;
  ptrCheckGuard(tabPtr, ctabrecFileSize, tablerec);

  for (Uint32 i = 0; i < tnoOfAttr; i++)
  {
    if(AttributeDescriptor::getDiskBased(req->attributes[i].attrDescriptor))
    {
      jam();
      tabPtr.p->m_disk_table = 1;
    }
  }//for

  addfragptr.p->attrSentToTup = 0;
  addfragptr.p->addfragStatus = AddFragRecord::TUP_ATTR_WAIT;
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

  tabptr.i = addfragptr.p->m_createTabReq.tableId;
  ptrCheckGuard(tabptr, ctabrecFileSize, tablerec);

  Uint32 noOfAttr = addfragptr.p->m_createTabReq.noOfAttributes;

  switch (addfragptr.p->addfragStatus) {
  case AddFragRecord::TUP_ATTR_WAIT:
    if (DictTabInfo::isOrderedIndex(tabptr.p->tableType))
    {
      addfragptr.p->addfragStatus = AddFragRecord::TUX_ATTR_WAIT;
      sendAddAttrReq(signal);
      break;
    }
    goto done_with_attr;
    break;
  case AddFragRecord::TUX_ATTR_WAIT:
    jam();
    if (lastAttr)
      addfragptr.p->tuxConnectptr = RNIL;
    goto done_with_attr;
    break;
  done_with_attr:
    addfragptr.p->attrSentToTup = addfragptr.p->attrSentToTup + 1;
    ndbrequire(addfragptr.p->attrSentToTup <= addfragptr.p->attrReceived);
    ndbrequire(addfragptr.p->totalAttrReceived <= noOfAttr);
    if (addfragptr.p->attrSentToTup < addfragptr.p->attrReceived)
    {
      // more in this batch
      jam();
      addfragptr.p->addfragStatus = AddFragRecord::TUP_ATTR_WAIT;
      sendAddAttrReq(signal);
      return;
    }

    if (addfragptr.p->defValSectionI != RNIL)
    {
      releaseSection(addfragptr.p->defValSectionI);
      addfragptr.p->defValNextPos = 0;
      addfragptr.p->defValSectionI = RNIL;
    }

    { // Reply
      LqhAddAttrConf *const conf = (LqhAddAttrConf*)signal->getDataPtrSend();
      conf->senderData = addfragptr.p->m_addAttrReq.senderData;
      conf->senderAttrPtr = addfragptr.p->m_addAttrReq.senderAttrPtr;
      sendSignal(addfragptr.p->m_createTabReq.senderRef,
                 GSN_LQHADDATTCONF, signal, LqhAddAttrConf::SignalLength, JBB);
    }
    if (addfragptr.p->totalAttrReceived < noOfAttr)
    {
      jam();
      addfragptr.p->addfragStatus = AddFragRecord::WAIT_ADD_ATTR;
    }
    else
    {
      jam();
      releaseAddfragrec(signal);
    }
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

/* *********************> */
/*  TUP_ADD_ATTREF      > */
/* *********************> */
void Dblqh::execTUP_ADD_ATTRREF(Signal* signal)
{
  jamEntry();
  addfragptr.i = signal->theData[0];
  ptrCheckGuard(addfragptr, caddfragrecFileSize, addFragRecord);
  Uint32 errorCode = terrorCode = signal->theData[1];

  abortAddFragOps(signal);

  // operation was released on the other side
  switch (addfragptr.p->addfragStatus) {
  case AddFragRecord::TUP_ATTR_WAIT:
    jam();
    break;
  case AddFragRecord::TUX_ATTR_WAIT:
    jam();
    break;
  default:
    ndbrequire(false);
    break;
  }

  if (addfragptr.p->defValSectionI != RNIL)
  {
    releaseSection(addfragptr.p->defValSectionI);
    addfragptr.p->defValNextPos = 0;
    addfragptr.p->defValSectionI = RNIL;
  }

  const Uint32 Ref = addfragptr.p->m_createTabReq.senderRef;
  const Uint32 senderData = addfragptr.p->m_addAttrReq.senderData;

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

/*
 * Add attribute in TUP or TUX.  Called up to 4 times.
 */
void
Dblqh::sendAddAttrReq(Signal* signal)
{
  arrGuard(addfragptr.p->attrSentToTup, LqhAddAttrReq::MAX_ATTRIBUTES);
  LqhAddAttrReq::Entry& entry =
    addfragptr.p->m_addAttrReq.attributes[addfragptr.p->attrSentToTup];

  const Uint32 attrId = entry.attrId & 0xffff;
  const Uint32 primaryAttrId = entry.attrId >> 16;

  tabptr.i = addfragptr.p->m_createTabReq.tableId;
  ptrCheckGuard(tabptr, ctabrecFileSize, tablerec);

  if (addfragptr.p->addfragStatus == AddFragRecord::TUP_ATTR_WAIT)
  {
    if (DictTabInfo::isTable(tabptr.p->tableType) ||
        DictTabInfo::isHashIndex(tabptr.p->tableType) ||
        (DictTabInfo::isOrderedIndex(tabptr.p->tableType) &&
         primaryAttrId == ZNIL)) {
      jam();
      TupAddAttrReq* const tupreq = (TupAddAttrReq*)signal->getDataPtrSend();
      tupreq->tupConnectPtr = addfragptr.p->tupConnectptr;
      tupreq->attrId = attrId;
      tupreq->attrDescriptor = entry.attrDescriptor;
      tupreq->extTypeInfo = entry.extTypeInfo;
      BlockReference tupRef = calcInstanceBlockRef(DBTUP);

      Uint32 sectionLen = 0;
      Uint32 startIndex = TupAddAttrReq::SignalLength;
      if (addfragptr.p->defValSectionI != RNIL)
      {
        SegmentedSectionPtr defValSection;
        getSection(defValSection, addfragptr.p->defValSectionI);

        SectionReader defValueReader(defValSection, getSectionSegmentPool());
        Uint32 defSectionWords = defValueReader.getSize();

        ndbrequire(defValueReader.step(addfragptr.p->defValNextPos));

        Uint32 defValueHeader;
        ndbrequire(defValueReader.peekWord(&defValueHeader));

        AttributeHeader ah(defValueHeader);
        Uint32 defValueLen = ah.getByteSize();
        Uint32 defValueWords = ((defValueLen +3)/4) + 1;
        Uint32 *dst = &signal->theData[startIndex];
        ndbassert(defSectionWords >= (addfragptr.p->defValNextPos + defValueWords));
        ndbrequire(defValueReader.getWords(dst, defValueWords));
        addfragptr.p->defValNextPos += defValueWords;
        sectionLen = defValueWords;
      }

      //A long section is attached when a default value is sent.
      if (sectionLen != 0)
      {
        LinearSectionPtr ptr[3];
        ptr[0].p= &signal->theData[startIndex];
        ptr[0].sz= sectionLen;
        sendSignal(tupRef, GSN_TUP_ADD_ATTRREQ,
                   signal, TupAddAttrReq::SignalLength, JBB, ptr, 1);
      }
      else
        sendSignal(tupRef, GSN_TUP_ADD_ATTRREQ,
                   signal, TupAddAttrReq::SignalLength, JBB);

      return;
    }
    if (DictTabInfo::isOrderedIndex(tabptr.p->tableType) &&
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

  if (addfragptr.p->addfragStatus == AddFragRecord::TUX_ATTR_WAIT)
  {
    jam();
    if (DictTabInfo::isOrderedIndex(tabptr.p->tableType) &&
        primaryAttrId != ZNIL) {
      jam();
      TuxAddAttrReq* const tuxreq = (TuxAddAttrReq*)signal->getDataPtrSend();
      tuxreq->tuxConnectPtr = addfragptr.p->tuxConnectptr;
      tuxreq->notused1 = 0;
      tuxreq->attrId = attrId;
      tuxreq->attrDescriptor = entry.attrDescriptor;
      tuxreq->extTypeInfo = entry.extTypeInfo;
      tuxreq->primaryAttrId = primaryAttrId;
      BlockReference tuxRef = calcInstanceBlockRef(DBTUX);
      sendSignal(tuxRef, GSN_TUX_ADD_ATTRREQ,
		 signal, TuxAddAttrReq::SignalLength, JBB);
      return;
    }
    if (DictTabInfo::isOrderedIndex(tabptr.p->tableType) &&
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

void Dblqh::execLQHFRAGREQ(Signal* signal)
{
  jamEntry();
  LqhFragReq copy = *(LqhFragReq*)signal->getDataPtr();
  LqhFragReq * req = &copy;

  tabptr.i = req->tableId;
  ptrCheckGuard(tabptr, ctabrecFileSize, tablerec);

  if (tabptr.p->tableStatus != Tablerec::ADD_TABLE_ONGOING &&
      (AlterTableReq::getAddFragFlag(req->changeMask) == 0))
  {
    jam();
    fragrefLab(signal, ZTAB_STATE_ERROR, req);
    return;
  }//if

  if (getFragmentrec(signal, req->fragId))
  {
    jam();
    fragrefLab(signal, terrorCode, req);
    return;
  }//if

  if (!insertFragrec(signal, req->fragId))
  {
    jam();
    fragrefLab(signal, terrorCode, req);
    return;
  }//if
  
  Uint32 copyType = req->requestInfo & 3;
  bool tempTable = ((req->requestInfo & LqhFragReq::TemporaryTable) != 0);
  initFragrec(signal, tabptr.i, req->fragId, copyType);
  fragptr.p->startGci = req->startGci;
  fragptr.p->newestGci = req->startGci;
  ndbrequire(tabptr.p->tableType < 256);
  fragptr.p->tableType = (Uint8)tabptr.p->tableType;

  {
    NdbLogPartInfo lpinfo(instance());
    Uint32 logPartNo = lpinfo.partNoFromId(req->logPartId);
    ndbrequire(lpinfo.partNoOwner(logPartNo));

    LogPartRecordPtr ptr;
    ptr.i = lpinfo.partNoIndex(logPartNo);
    ptrCheckGuard(ptr, clogPartFileSize, logPartRecord);
    ndbrequire(ptr.p->logPartNo == logPartNo);

    fragptr.p->m_log_part_ptr_i = ptr.i;
    fragptr.p->lqhInstanceKey = getInstanceKey(tabptr.i, req->fragId);
  }

  if (DictTabInfo::isOrderedIndex(tabptr.p->tableType)) {
    jam();
    // find corresponding primary table fragment
    TablerecPtr tTablePtr;
    tTablePtr.i = tabptr.p->primaryTableId;
    ptrCheckGuard(tTablePtr, ctabrecFileSize, tablerec);
    FragrecordPtr tFragPtr;
    tFragPtr.i = RNIL;
    for (Uint32 i = 0; i < NDB_ARRAY_SIZE(tTablePtr.p->fragid); i++) {
      if (tTablePtr.p->fragid[i] == fragptr.p->fragId) {
        jam();
        tFragPtr.i = tTablePtr.p->fragrec[i];
        break;
      }
    }
    ndbrequire(tFragPtr.i != RNIL);
    // store it
    fragptr.p->tableFragptr = tFragPtr.i;
  }
  else
  {
    jam();
    fragptr.p->tableFragptr = fragptr.i;
  }

  if (tempTable)
  {
//--------------------------------------------
// reqinfo bit 3-4 = 2 means temporary table
// without logging or checkpointing.
//--------------------------------------------
    jam();
    fragptr.p->logFlag = Fragrecord::STATE_FALSE;
    fragptr.p->lcpFlag = Fragrecord::LCP_STATE_FALSE;
  }//if

  seizeAddfragrec(signal);
  addfragptr.p->m_lqhFragReq = * req;
  addfragptr.p->fragmentPtr = fragptr.i;

  if (DictTabInfo::isTable(tabptr.p->tableType) ||
      DictTabInfo::isHashIndex(tabptr.p->tableType)) {
    jam();
    AccFragReq* const accreq = (AccFragReq*)signal->getDataPtrSend();
    accreq->userPtr = addfragptr.i;
    accreq->userRef = cownref;
    accreq->tableId = tabptr.i;
    accreq->reqInfo = 0;
    accreq->fragId = req->fragId;
    accreq->localKeyLen = addfragptr.p->m_lqhFragReq.localKeyLength;
    accreq->maxLoadFactor = addfragptr.p->m_lqhFragReq.maxLoadFactor;
    accreq->minLoadFactor = addfragptr.p->m_lqhFragReq.minLoadFactor;
    accreq->kValue = addfragptr.p->m_lqhFragReq.kValue;
    accreq->lhFragBits = addfragptr.p->m_lqhFragReq.lh3DistrBits;
    accreq->lhDirBits = addfragptr.p->m_lqhFragReq.lh3PageBits;
    accreq->keyLength = addfragptr.p->m_lqhFragReq.keyLength;
    /* --------------------------------------------------------------------- */
    /* Send ACCFRAGREQ, when confirmation is received send 2 * TUPFRAGREQ to */
    /* create 2 tuple fragments on this node.                                */
    /* --------------------------------------------------------------------- */
    addfragptr.p->addfragStatus = AddFragRecord::ACC_ADDFRAG;
    sendSignal(fragptr.p->accBlockref, GSN_ACCFRAGREQ,
	       signal, AccFragReq::SignalLength, JBB);
    return;
  }
  if (DictTabInfo::isOrderedIndex(tabptr.p->tableType)) {
    jam();
    addfragptr.p->addfragStatus = AddFragRecord::WAIT_TUP;
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
  //Uint32 fragId1 = signal->theData[2];
  Uint32 accFragPtr1 = signal->theData[4];
  ptrCheckGuard(addfragptr, caddfragrecFileSize, addFragRecord);
  ndbrequire(addfragptr.p->addfragStatus == AddFragRecord::ACC_ADDFRAG);

  addfragptr.p->accConnectptr = taccConnectptr;
  fragptr.i = addfragptr.p->fragmentPtr;
  c_fragment_pool.getPtr(fragptr);
  fragptr.p->accFragptr = accFragPtr1;

  addfragptr.p->addfragStatus = AddFragRecord::WAIT_TUP;
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
  //Uint32 localFragId = signal->theData[3];  /* LOCAL FRAGMENT ID    */
  ptrCheckGuard(addfragptr, caddfragrecFileSize, addFragRecord);
  fragptr.i = addfragptr.p->fragmentPtr;
  c_fragment_pool.getPtr(fragptr);
  tabptr.i = fragptr.p->tabRef;
  ptrCheckGuard(tabptr, ctabrecFileSize, tablerec);
  fragptr.p->tupFragptr = tupFragPtr;
  switch (addfragptr.p->addfragStatus) {
  case AddFragRecord::WAIT_TUP:
    jam();
    fragptr.p->tupFragptr = tupFragPtr;
    addfragptr.p->tupConnectptr = tupConnectptr;
    if (DictTabInfo::isOrderedIndex(tabptr.p->tableType))
    {
      addfragptr.p->addfragStatus = AddFragRecord::WAIT_TUX;
      sendAddFragReq(signal);
      break;
    }
    goto done_with_frag;
    break;
  case AddFragRecord::WAIT_TUX:
    jam();
    fragptr.p->tuxFragptr = tupFragPtr;
    addfragptr.p->tuxConnectptr = tupConnectptr;
    goto done_with_frag;
    break;
  done_with_frag:
    /* ---------------------------------------------------------------- */
    /* Finished create of fragments. Now ready for creating attributes. */
    /* ---------------------------------------------------------------- */
    fragptr.p->fragStatus = Fragrecord::FSACTIVE;
    {
      LqhFragConf* conf = (LqhFragConf*)signal->getDataPtrSend();
      conf->senderData = addfragptr.p->m_lqhFragReq.senderData;
      conf->lqhFragPtr = RNIL;
      conf->tableId = addfragptr.p->m_lqhFragReq.tableId;
      conf->fragId = fragptr.p->fragId;
      conf->changeMask = addfragptr.p->m_lqhFragReq.changeMask;
      sendSignal(addfragptr.p->m_lqhFragReq.senderRef, GSN_LQHFRAGCONF,
		 signal, LqhFragConf::SignalLength, JBB);
    }
    releaseAddfragrec(signal);
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
  c_fragment_pool.getPtr(fragptr);
  tabptr.i = fragptr.p->tabRef;
  ptrCheckGuard(tabptr, ctabrecFileSize, tablerec);
  if (addfragptr.p->addfragStatus == AddFragRecord::WAIT_TUP)
  {
    TupFragReq* const tupFragReq = (TupFragReq*)signal->getDataPtrSend();
    tupFragReq->userPtr = addfragptr.i;
    tupFragReq->userRef = cownref;
    tupFragReq->reqInfo = 0; /* ADD TABLE */
    tupFragReq->tableId = tabptr.i;
    tupFragReq->fragId = addfragptr.p->m_lqhFragReq.fragId;
    tupFragReq->tablespaceid = addfragptr.p->m_lqhFragReq.tablespace_id;
    tupFragReq->maxRowsHigh = addfragptr.p->m_lqhFragReq.maxRowsHigh;
    tupFragReq->maxRowsLow = addfragptr.p->m_lqhFragReq.maxRowsLow;
    tupFragReq->minRowsHigh = addfragptr.p->m_lqhFragReq.minRowsHigh;
    tupFragReq->minRowsLow = addfragptr.p->m_lqhFragReq.minRowsLow;
    tupFragReq->changeMask = addfragptr.p->m_lqhFragReq.changeMask;
    sendSignal(fragptr.p->tupBlockref, GSN_TUPFRAGREQ,
               signal, TupFragReq::SignalLength, JBB);
    return;
  }
  if (addfragptr.p->addfragStatus == AddFragRecord::WAIT_TUX)
  {
    jam();
    ndbrequire(DictTabInfo::isOrderedIndex(tabptr.p->tableType));
    TuxFragReq* const tuxreq = (TuxFragReq*)signal->getDataPtrSend();
    tuxreq->userPtr = addfragptr.i;
    tuxreq->userRef = cownref;
    tuxreq->reqInfo = 0; /* ADD TABLE */
    tuxreq->tableId = tabptr.i;
    tuxreq->fragId = addfragptr.p->m_lqhFragReq.fragId;
    tuxreq->primaryTableId = tabptr.p->primaryTableId;
    // pointer to index fragment in TUP
    tuxreq->tupIndexFragPtrI = fragptr.p->tupFragptr;
    // pointers to table fragments in TUP and ACC
    FragrecordPtr tFragPtr;
    tFragPtr.i = fragptr.p->tableFragptr;
    c_fragment_pool.getPtr(tFragPtr);
    tuxreq->tupTableFragPtrI = tFragPtr.p->tupFragptr;
    tuxreq->accTableFragPtrI = tFragPtr.p->accFragptr;
    sendSignal(fragptr.p->tuxBlockref, GSN_TUXFRAGREQ,
               signal, TuxFragReq::SignalLength, JBB);
    return;
  }
}//Dblqh::sendAddFragReq


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
  tabptr.p->usageCountR = 0;
  tabptr.p->usageCountW = 0;
  tabptr.p->tableStatus = Tablerec::TABLE_DEFINED;
  signal->theData[0] = dihPtr;
  signal->theData[1] = cownNodeid;
  signal->theData[2] = tabptr.i;
  sendSignal(dihBlockref, GSN_TAB_COMMITCONF, signal, 3, JBB);

  return;
}//Dblqh::execTAB_COMMITREQ()


void Dblqh::fragrefLab(Signal* signal,
                       Uint32 errorCode,
                       const LqhFragReq* req)
{
  LqhFragRef * ref = (LqhFragRef*)signal->getDataPtrSend();
  ref->senderData = req->senderData;
  ref->errorCode = errorCode;
  ref->requestInfo = req->requestInfo;
  ref->tableId = req->tableId;
  ref->fragId = req->fragId;
  ref->changeMask = req->changeMask;
  sendSignal(req->senderRef, GSN_LQHFRAGREF, signal,
	     LqhFragRef::SignalLength, JBB);
  return;
}//Dblqh::fragrefLab()

/*
 * Abort on-going ops.
 */
void Dblqh::abortAddFragOps(Signal* signal)
{
  if (addfragptr.p->tupConnectptr != RNIL) {
    jam();
    TupFragReq* const tupFragReq = (TupFragReq*)signal->getDataPtrSend();
    tupFragReq->userPtr = (Uint32)-1;
    tupFragReq->userRef = addfragptr.p->tupConnectptr;
    sendSignal(ctupBlockref, GSN_TUPFRAGREQ, signal, 2, JBB);
    addfragptr.p->tupConnectptr = RNIL;
  }
  if (addfragptr.p->tuxConnectptr != RNIL) {
    jam();
    TuxFragReq* const tuxFragReq = (TuxFragReq*)signal->getDataPtrSend();
    tuxFragReq->userPtr = (Uint32)-1;
    tuxFragReq->userRef = addfragptr.p->tuxConnectptr;
    sendSignal(ctuxBlockref, GSN_TUXFRAGREQ, signal, 2, JBB);
    addfragptr.p->tuxConnectptr = RNIL;
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
  Uint32 errorCode = terrorCode = signal->theData[1];
  ndbrequire(addfragptr.p->addfragStatus == AddFragRecord::ACC_ADDFRAG);

  fragrefLab(signal, errorCode, &addfragptr.p->m_lqhFragReq);
  releaseAddfragrec(signal);

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
  Uint32 errorCode = terrorCode = signal->theData[1];
  fragptr.i = addfragptr.p->fragmentPtr;
  c_fragment_pool.getPtr(fragptr);

  // no operation to release, just add some jams
  switch (addfragptr.p->addfragStatus) {
  case AddFragRecord::WAIT_TUP:
    jam();
    break;
  case AddFragRecord::WAIT_TUX:
    jam();
    break;
  default:
    ndbrequire(false);
    break;
  }

  fragrefLab(signal, errorCode, &addfragptr.p->m_lqhFragReq);
  releaseAddfragrec(signal);

}//Dblqh::execTUPFRAGREF()

void
Dblqh::execDROP_FRAG_REQ(Signal* signal)
{
  DropFragReq *req = (DropFragReq*)signal->getDataPtr();
  seizeAddfragrec(signal);
  addfragptr.p->m_dropFragReq = *req;

  /**
   * 1 - self
   * 2 - acc
   * 3 - tup
   * 4 - tux (optional)
   */
  tabptr.i = req->tableId;
  ptrCheckGuard(tabptr, ctabrecFileSize, tablerec);

  deleteFragrec(req->fragId);

  Uint32 ref = calcInstanceBlockRef(DBACC);
  if (DictTabInfo::isOrderedIndex(tabptr.p->tableType))
  {
    jam();
    ref = calcInstanceBlockRef(DBTUP);
  }

  req->senderRef = reference();
  req->senderData = addfragptr.i;
  sendSignal(ref, GSN_DROP_FRAG_REQ, signal, DropFragReq::SignalLength, JBB);
}

void
Dblqh::execDROP_FRAG_REF(Signal* signal)
{
  ndbrequire(false);
}

void
Dblqh::execDROP_FRAG_CONF(Signal* signal)
{
  DropFragConf* conf = (DropFragConf*)signal->getDataPtr();
  addfragptr.i = conf->senderData;
  ptrCheckGuard(addfragptr, caddfragrecFileSize, addFragRecord);

  Uint32 ref = RNIL;
  switch(refToMain(conf->senderRef)){
  case DBACC:
    jam();
    ref = calcInstanceBlockRef(DBTUP);
    break;
  case DBTUP:
  {
    tabptr.i = addfragptr.p->m_dropFragReq.tableId;
    ptrCheckGuard(tabptr, ctabrecFileSize, tablerec);
    if (DictTabInfo::isOrderedIndex(tabptr.p->tableType))
    {
      jam();
      ref = calcInstanceBlockRef(DBTUX);
    }
    break;
  }
  case DBTUX:
    break;
  default:
    ndbrequire(false);
  }

  if (ref != RNIL)
  {
    DropFragReq* req = (DropFragReq*)signal->getDataPtrSend();
    * req = addfragptr.p->m_dropFragReq;
    req->senderRef = reference();
    req->senderData = addfragptr.i;
    sendSignal(ref, GSN_DROP_FRAG_REQ, signal, DropFragReq::SignalLength,
               JBB);
    return;
  }

  conf->senderRef = reference();
  conf->senderData = addfragptr.p->m_dropFragReq.senderData;
  conf->tableId = addfragptr.p->m_dropFragReq.tableId;
  conf->fragId = addfragptr.p->m_dropFragReq.fragId;
  sendSignal(addfragptr.p->m_dropFragReq.senderRef, GSN_DROP_FRAG_CONF,
             signal, DropFragConf::SignalLength, JBB);

  releaseAddfragrec(signal);
}

/* ************>> */
/*  TUXFRAGREF  > */
/* ************>> */
void Dblqh::execTUXFRAGREF(Signal* signal) 
{
  jamEntry();
  execTUPFRAGREF(signal);
}//Dblqh::execTUXFRAGREF

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
  switch(tabPtr.p->tableStatus) {
  case Tablerec::TABLE_DEFINED:
    jam();
    break;
  case Tablerec::NOT_DEFINED:
    jam();
    // Fall through
  case Tablerec::ADD_TABLE_ONGOING:
    jam();
    errCode = PrepDropTabRef::NoSuchTable;
    break;
  case Tablerec::PREP_DROP_TABLE_DONE:
    jam();
    errCode = PrepDropTabRef::DropInProgress;
    break;
  case Tablerec::DROP_TABLE_WAIT_USAGE:
  case Tablerec::DROP_TABLE_WAIT_DONE:
  case Tablerec::DROP_TABLE_ACC:
  case Tablerec::DROP_TABLE_TUP:
  case Tablerec::DROP_TABLE_TUX:
    jam();
    errCode = PrepDropTabRef::DropInProgress;
  case Tablerec::TABLE_READ_ONLY:
    jam();
    errCode = PrepDropTabRef::InvalidTableState;
    break;
  }

  if(errCode != 0)
  {
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
  
  tabPtr.p->tableStatus = Tablerec::PREP_DROP_TABLE_DONE;
  
  PrepDropTabConf * conf = (PrepDropTabConf*)signal->getDataPtrSend();
  conf->tableId = tabPtr.i;
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(senderRef, GSN_PREP_DROP_TAB_CONF, signal,
	     PrepDropTabConf::SignalLength, JBB);
}

void
Dblqh::dropTab_wait_usage(Signal* signal){

  TablerecPtr tabPtr;
  tabPtr.i = signal->theData[1];
  ptrCheckGuard(tabPtr, ctabrecFileSize, tablerec);

  Uint32 senderRef = signal->theData[2];
  Uint32 senderData = signal->theData[3];
  
  ndbrequire(tabPtr.p->tableStatus == Tablerec::DROP_TABLE_WAIT_USAGE);
  
  if (tabPtr.p->usageCountR > 0 || tabPtr.p->usageCountW > 0)
  {
    jam();
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 4);
    return;
  }

  bool lcpDone = true;
  lcpPtr.i = 0;
  ptrAss(lcpPtr, lcpRecord);
  if(lcpPtr.p->lcpState != LcpRecord::LCP_IDLE)
  {
    jam();

    if(lcpPtr.p->currentFragment.lcpFragOrd.tableId == tabPtr.i)
    {
      jam();
      lcpDone = false;
    }
    
    if(lcpPtr.p->lcpQueued && 
       lcpPtr.p->queuedFragment.lcpFragOrd.tableId == tabPtr.i)
    {
      jam();
      lcpDone = false;
    }
  }
  
  if(!lcpDone)
  {
    jam();
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 4);
    return;
  }
  
  tabPtr.p->tableStatus = Tablerec::DROP_TABLE_WAIT_DONE;

  DropTabConf * conf = (DropTabConf*)signal->getDataPtrSend();
  conf->tableId = tabPtr.i;
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(senderRef, GSN_DROP_TAB_CONF, signal,
	     DropTabConf::SignalLength, JBB);
}

void
Dblqh::execDROP_TAB_REQ(Signal* signal){
  jamEntry();
  if (ERROR_INSERTED(5076))
  {
    /**
     * This error insert simulates a situation where it takes a long time
     * to execute DROP_TAB_REQ, such that we can crash the (dict) master
     * while there is an outstanding DROP_TAB_REQ.
     */
    jam();
    sendSignalWithDelay(reference(), GSN_DROP_TAB_REQ, signal, 1000,
                        signal->getLength());
    return;
  }
  if (ERROR_INSERTED(5077))
  {
    jam();
    CLEAR_ERROR_INSERT_VALUE;
    /** 
     * Kill this node 2 seconds from now. We wait for two seconds to make sure
     * that DROP_TAB_REQ messages have reached other nodes before this one
     * dies.
     */
    signal->theData[0] = 9999;
    sendSignalWithDelay(CMVMI_REF, GSN_NDB_TAMPER, signal, 2000, 1);
    return;
  }
  DropTabReq reqCopy = * (DropTabReq*)signal->getDataPtr();
  DropTabReq* req = &reqCopy;
  
  TablerecPtr tabPtr;
  tabPtr.i = req->tableId;
  ptrCheckGuard(tabPtr, ctabrecFileSize, tablerec);
  
  Uint32 errCode = 0;
  switch((DropTabReq::RequestType)req->requestType) {
  case DropTabReq::RestartDropTab:
    jam();
    tabPtr.p->tableStatus = Tablerec::DROP_TABLE_WAIT_DONE;
    break;
  case DropTabReq::CreateTabDrop:
    jam();
    tabPtr.p->tableStatus = Tablerec::DROP_TABLE_WAIT_DONE;
    break;
  case DropTabReq::OnlineDropTab:
    jam();
    switch(tabPtr.p->tableStatus) {
    case Tablerec::TABLE_DEFINED:
      jam();
      errCode = DropTabRef::DropWoPrep;
      break;
    case Tablerec::NOT_DEFINED:
      jam();
      errCode = DropTabRef::NoSuchTable;
      break;
    case Tablerec::ADD_TABLE_ONGOING:
      jam();
      ndbassert(false);
    case Tablerec::PREP_DROP_TABLE_DONE:
      jam();
      tabPtr.p->tableStatus = Tablerec::DROP_TABLE_WAIT_USAGE;
      signal->theData[0] = ZDROP_TABLE_WAIT_USAGE;
      signal->theData[1] = tabPtr.i;
      signal->theData[2] = req->senderRef;
      signal->theData[3] = req->senderData;
      dropTab_wait_usage(signal);
      return;
      break;
    case Tablerec::DROP_TABLE_WAIT_USAGE:
    case Tablerec::DROP_TABLE_ACC:
    case Tablerec::DROP_TABLE_TUP:
    case Tablerec::DROP_TABLE_TUX:
      ndbrequire(false);
    case Tablerec::DROP_TABLE_WAIT_DONE:
      jam();
      break;
    case Tablerec::TABLE_READ_ONLY:
      jam();
      errCode = DropTabRef::InvalidTableState;
      break;
    }
  }
  
  if (errCode)
  {
    jam();
    DropTabRef * ref = (DropTabRef*)signal->getDataPtrSend();
    ref->tableId = tabPtr.i;
    ref->senderRef = reference();
    ref->senderData = req->senderData;
    ref->errorCode = errCode;
    sendSignal(req->senderRef, GSN_DROP_TAB_REF, signal,
               DropTabRef::SignalLength, JBB);
    return;
  }

  ndbrequire(tabPtr.p->usageCountR == 0 && tabPtr.p->usageCountW == 0);
  seizeAddfragrec(signal);
  addfragptr.p->m_dropTabReq = * req;
  dropTable_nextStep(signal, addfragptr);
}

void
Dblqh::execDROP_TAB_REF(Signal* signal)
{
  jamEntry();
  DropTabRef * ref = (DropTabRef*)signal->getDataPtr();

#if defined ERROR_INSERT || defined VM_TRACE
  jamLine(ref->errorCode);
  ndbrequire(false);
#endif

  Ptr<AddFragRecord> addFragPtr;
  addFragPtr.i = ref->senderData;
  ptrCheckGuard(addFragPtr, caddfragrecFileSize, addFragRecord);
  dropTable_nextStep(signal, addFragPtr);
}

void
Dblqh::execDROP_TAB_CONF(Signal* signal)
{
  jamEntry();
  DropTabConf * conf = (DropTabConf*)signal->getDataPtr();

  Ptr<AddFragRecord> addFragPtr;
  addFragPtr.i = conf->senderData;
  ptrCheckGuard(addFragPtr, caddfragrecFileSize, addFragRecord);
  dropTable_nextStep(signal, addFragPtr);
}

void
Dblqh::dropTable_nextStep(Signal* signal, Ptr<AddFragRecord> addFragPtr)
{
  jam();

  TablerecPtr tabPtr;
  tabPtr.i = addFragPtr.p->m_dropTabReq.tableId;
  ptrCheckGuard(tabPtr, ctabrecFileSize, tablerec);

  Uint32 ref = 0;
  if (tabPtr.p->tableStatus == Tablerec::DROP_TABLE_WAIT_DONE)
  {
    jam();
    if (DictTabInfo::isTable(tabPtr.p->tableType) ||
        DictTabInfo::isHashIndex(tabPtr.p->tableType))
    {
      jam();
      ref = calcInstanceBlockRef(DBACC);
      tabPtr.p->tableStatus = Tablerec::DROP_TABLE_ACC;
    }
    else
    {
      jam();
      ref = calcInstanceBlockRef(DBTUP);
      tabPtr.p->tableStatus = Tablerec::DROP_TABLE_TUP;
    }
  }
  else if (tabPtr.p->tableStatus == Tablerec::DROP_TABLE_ACC)
  {
    jam();
    ref = calcInstanceBlockRef(DBTUP);
    tabPtr.p->tableStatus = Tablerec::DROP_TABLE_TUP;
  }
  else if (tabPtr.p->tableStatus == Tablerec::DROP_TABLE_TUP)
  {
    jam();
    if (DictTabInfo::isOrderedIndex(tabPtr.p->tableType))
    {
      jam();
      ref = calcInstanceBlockRef(DBTUX);
      tabPtr.p->tableStatus = Tablerec::DROP_TABLE_TUX;
    }
  }

  if (ref)
  {
    jam();
    DropTabReq* req = (DropTabReq*)signal->getDataPtrSend();
    req->senderData = addFragPtr.i;
    req->senderRef = reference();
    req->tableId = tabPtr.i;
    req->tableVersion = tabPtr.p->schemaVersion;
    req->requestType = addFragPtr.p->m_dropTabReq.requestType;
    sendSignal(ref, GSN_DROP_TAB_REQ, signal,
               DropTabReq::SignalLength, JBB);
    return;
  }

  removeTable(tabPtr.i);
  tabPtr.p->tableStatus = Tablerec::NOT_DEFINED;

  DropTabConf* conf = (DropTabConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = addFragPtr.p->m_dropTabReq.senderData;
  conf->tableId = tabPtr.i;
  sendSignal(addFragPtr.p->m_dropTabReq.senderRef, GSN_DROP_TAB_CONF, signal,
             DropTabConf::SignalLength, JBB);

  addfragptr = addFragPtr;
  releaseAddfragrec(signal);
}

void Dblqh::removeTable(Uint32 tableId)
{
  tabptr.i = tableId;
  ptrCheckGuard(tabptr, ctabrecFileSize, tablerec);
  
  for (Uint32 i = 0; i < NDB_ARRAY_SIZE(tabptr.p->fragid); i++) {
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

  if(!assembleFragments(signal))
    return;

  AlterTabReq copy = *(AlterTabReq*)signal->getDataPtr();
  const AlterTabReq* req = &copy;
  const Uint32 senderRef = req->senderRef;
  const Uint32 senderData = req->senderData;
  const Uint32 tableId = req->tableId;
  const Uint32 tableVersion = req->tableVersion;
  const Uint32 newTableVersion = req->newTableVersion;
  AlterTabReq::RequestType requestType =
    (AlterTabReq::RequestType) req->requestType;

  TablerecPtr tablePtr;
  tablePtr.i = tableId;
  ptrCheckGuard(tablePtr, ctabrecFileSize, tablerec);

  Uint32 len = signal->getLength();
  switch (requestType) {
  case AlterTabReq::AlterTablePrepare:
    jam();
    break;
  case AlterTabReq::AlterTableRevert:
    jam();
    tablePtr.p->schemaVersion = tableVersion;
    break;
  case AlterTabReq::AlterTableCommit:
    jam();
    tablePtr.p->schemaVersion = newTableVersion;
    if (AlterTableReq::getReorgFragFlag(req->changeMask))
    {
      jam();
      commit_reorg(tablePtr);
    }
    break;
  case AlterTabReq::AlterTableComplete:
    jam();
    break;
  case AlterTabReq::AlterTableSumaEnable:
    jam();
    break;
  case AlterTabReq::AlterTableSumaFilter:
    jam();
    signal->theData[len++] = cnewestGci + 3;
    break;
  case AlterTabReq::AlterTableReadOnly:
    jam();
    ndbrequire(tablePtr.p->tableStatus == Tablerec::TABLE_DEFINED);
    tablePtr.p->tableStatus = Tablerec::TABLE_READ_ONLY;
    signal->theData[0] = ZWAIT_READONLY;
    signal->theData[1] = tablePtr.i;
    signal->theData[2] = senderRef;
    signal->theData[3] = senderData;
    sendSignal(reference(), GSN_CONTINUEB, signal, 4, JBB);
    return;
  case AlterTabReq::AlterTableReadWrite:
    jam();
    ndbrequire(tablePtr.p->tableStatus == Tablerec::TABLE_READ_ONLY);
    tablePtr.p->tableStatus = Tablerec::TABLE_DEFINED;
    break;
  default:
    ndbrequire(false);
    break;
  }

  EXECUTE_DIRECT(DBTUP, GSN_ALTER_TAB_REQ, signal, len);
  jamEntry();

  Uint32 errCode = signal->theData[0];
  Uint32 connectPtr = signal->theData[1];
  if (errCode == 0)
  {
    // Request handled successfully
    AlterTabConf* conf = (AlterTabConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = senderData;
    conf->connectPtr = connectPtr;
    sendSignal(senderRef, GSN_ALTER_TAB_CONF, signal,
               AlterTabConf::SignalLength, JBB);
  }
  else if (errCode == ~Uint32(0))
  {
    /**
     * Wait
     */
    ndbrequire(requestType == AlterTabReq::AlterTableSumaFilter);
    signal->theData[0] = ZWAIT_REORG_SUMA_FILTER_ENABLED;
    signal->theData[1] = cnewestGci + 3;
    signal->theData[2] = senderData;
    signal->theData[3] = connectPtr;
    signal->theData[4] = senderRef;
    wait_reorg_suma_filter_enabled(signal);
    return;
  }
  else
  {
    jam();
    AlterTabRef* ref = (AlterTabRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = senderData;
    ref->connectPtr = connectPtr;
    ref->errorCode = errCode;
    sendSignal(senderRef, GSN_ALTER_TAB_REF, signal,
               AlterTabRef::SignalLength, JBB);
  }
}

void
Dblqh::wait_reorg_suma_filter_enabled(Signal* signal)
{
  if (cnewestCompletedGci >= signal->theData[1])
  {
    jam();
    Uint32 senderData = signal->theData[2];
    Uint32 connectPtr = signal->theData[3];
    Uint32 senderRef = signal->theData[4];

    AlterTabConf* conf = (AlterTabConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = senderData;
    conf->connectPtr = connectPtr;
    sendSignal(senderRef, GSN_ALTER_TAB_CONF, signal,
               AlterTabConf::SignalLength, JBB);
    return;
  }
  sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 500, 5);
}

void
Dblqh::commit_reorg(TablerecPtr tablePtr)
{
  for (Uint32 i = 0; i < NDB_ARRAY_SIZE(tablePtr.p->fragrec); i++)
  {
    jam();
    Ptr<Fragrecord> fragPtr;
    if ((fragPtr.i = tablePtr.p->fragrec[i]) != RNIL)
    {
      jam();
      c_fragment_pool.getPtr(fragPtr);
      fragPtr.p->fragDistributionKey = (fragPtr.p->fragDistributionKey+1)&0xFF;
    }
  }
}

void
Dblqh::wait_readonly(Signal* signal)
{
  jam();

  Uint32 tableId = signal->theData[1];

  TablerecPtr tablePtr;
  tablePtr.i = tableId;
  ptrCheckGuard(tablePtr, ctabrecFileSize, tablerec);
  ndbrequire(tablePtr.p->tableStatus == Tablerec::TABLE_READ_ONLY);

  if (tablePtr.p->usageCountW > 0)
  {
    jam();
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 3000,
                        signal->getLength());
    return;
  }

  Uint32 senderRef = signal->theData[2];
  Uint32 senderData = signal->theData[3];

  // Request handled successfully
  AlterTabConf * conf = (AlterTabConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = senderData;
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

  cLqhTimeOutCount ++;
  cLqhTimeOutCheckCount ++;

  for (logPartPtr.i = 0; logPartPtr.i < clogPartFileSize; logPartPtr.i++)
  {
    jam();
    ptrCheckGuard(logPartPtr, clogPartFileSize, logPartRecord);
    int ret = logPartPtr.p->m_io_tracker.tick(10 * cLqhTimeOutCount,
                                              c_max_redo_lag,
                                              c_max_redo_lag_counter);
    if (ret < 0)
    {
      /**
       * set problem
       */
      update_log_problem(signal, logPartPtr,
                         LogPartRecord::P_REDO_IO_PROBLEM, true);
    }
    else if (ret > 0)
    {
      /**
       * clear
       */
      update_log_problem(signal, logPartPtr,
                         LogPartRecord::P_REDO_IO_PROBLEM, false);
    }
  }

  if (cLqhTimeOutCheckCount < 1000) {
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
	((tTcConptr.p->tcTimer + 12000) < cLqhTimeOutCount)) {
      ndbout << "Dblqh::execTIME_SIGNAL"<<endl
	     << "Timeout found in tcConnectRecord " <<tTcConptr.i<<endl
	     << " cLqhTimeOutCount = " << cLqhTimeOutCount << endl
	     << " tcTimer="<<tTcConptr.p->tcTimer<<endl
	     << " tcTimer+12000="<<tTcConptr.p->tcTimer + 12000<<endl;

      signal->theData[0] = 2307;
      signal->theData[1] = tTcConptr.i;
      execDUMP_STATE_ORD(signal);
      
      // Reset the timer 
      tTcConptr.p->tcTimer = 0;
    }//if
  }//for
#endif
#ifdef VM_TRACE
  for (lfoPtr.i = 0; lfoPtr.i < clfoFileSize; lfoPtr.i++) {
    ptrAss(lfoPtr, logFileOperationRecord);
    if ((lfoPtr.p->lfoTimer != 0) &&
        ((lfoPtr.p->lfoTimer + 12000) < cLqhTimeOutCount)) {
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

/**
 * earlyKeyReqAbort
 *
 * Exit early from handling an LQHKEYREQ request.
 * Method determines which resources (if any) need freed, then
 * signals requestor with error response.
 * * Verify all required resources are freed if adding new callers *
 */
void Dblqh::earlyKeyReqAbort(Signal* signal, 
                             const LqhKeyReq * lqhKeyReq,
                             Uint32 errCode) 
{
  jamEntry();
  const Uint32 transid1  = lqhKeyReq->transId1;
  const Uint32 transid2  = lqhKeyReq->transId2;
  const Uint32 reqInfo   = lqhKeyReq->requestInfo;
  
  bool tcConnectRecAllocated = (tcConnectptr.i != RNIL);

  if (tcConnectRecAllocated)
  {
    jam();
    
    /* Could have a commit-ack marker allocated. */
    remove_commit_marker(tcConnectptr.p);
    
    /* Could have long key/attr sections linked */
    ndbrequire(tcConnectptr.p->m_dealloc == 0);
    releaseOprec(signal);
    
    
    /* 
     * Free the TcConnectRecord, ensuring that the
     * table reference counts have not been incremented and
     * so will not be decremented.
     * Also verify that we're not present in the transid 
     * hash
     */
    ndbrequire(tcConnectptr.p->tableref == RNIL);
    /* Following is not 100% check, but a reasonable guard */
    ndbrequire(tcConnectptr.p->nextHashRec == RNIL);
    ndbrequire(tcConnectptr.p->prevHashRec == RNIL);
    releaseTcrec(signal, tcConnectptr);
  }

  /* Now perform signalling */

  if (LqhKeyReq::getDirtyFlag(reqInfo) && 
      LqhKeyReq::getOperation(reqInfo) == ZREAD &&
      !LqhKeyReq::getNormalProtocolFlag(reqInfo)){
    jam();
    /* Dirty read sends TCKEYREF direct to client, and nothing to TC */
    ndbrequire(LqhKeyReq::getApplicationAddressFlag(reqInfo));
    const Uint32 apiRef   = lqhKeyReq->variableData[0];
    const Uint32 apiOpRec = lqhKeyReq->variableData[1];

    TcKeyRef * const tcKeyRef = (TcKeyRef *) signal->getDataPtrSend();
    
    tcKeyRef->connectPtr = apiOpRec;
    tcKeyRef->transId[0] = transid1;
    tcKeyRef->transId[1] = transid2;
    tcKeyRef->errorCode = errCode;
    sendTCKEYREF(signal, apiRef, signal->getSendersBlockRef(), 0);
  } else {
    jam();
    /* All ops apart from dirty read send LQHKEYREF to TC
     * (This includes simple read)
     */

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
}//Dblqh::earlyKeyReqAbort()

Uint32
Dblqh::get_table_state_error(Ptr<Tablerec> tabPtr) const
{
  switch(tabPtr.p->tableStatus){
  case Tablerec::NOT_DEFINED:
    jam();
    return ZTABLE_NOT_DEFINED;
    break;
  case Tablerec::ADD_TABLE_ONGOING:
    jam();
  case Tablerec::PREP_DROP_TABLE_DONE:
    jam();
  case Tablerec::DROP_TABLE_WAIT_USAGE:
    jam();
  case Tablerec::DROP_TABLE_WAIT_DONE:
    jam();
  case Tablerec::DROP_TABLE_ACC:
    jam();
  case Tablerec::DROP_TABLE_TUP:
    jam();
  case Tablerec::DROP_TABLE_TUX:
    jam();
    return ZDROP_TABLE_IN_PROGRESS;
    break;
  case Tablerec::TABLE_DEFINED:
  case Tablerec::TABLE_READ_ONLY:
    ndbassert(0);
    return ZTABLE_NOT_DEFINED;
    break;
  }
  ndbassert(0);
  return ~Uint32(0);
}

int
Dblqh::check_tabstate(Signal * signal, const Tablerec * tablePtrP, Uint32 op)
{
  if (tabptr.p->tableStatus == Tablerec::TABLE_READ_ONLY)
  {
    jam();
    if (op == ZREAD || op == ZREAD_EX || op == ZUNLOCK)
    {
      jam();
      return 0;
    }
    terrorCode = ZTABLE_READ_ONLY;
  }
  else
  {
    jam();
    terrorCode = get_table_state_error(tabptr);
  }
  abortErrorLab(signal);
  return 1;
}

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
    terrorCode = get_table_state_error(tabptr);
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
  Uint32 tcOprec  = signal->theData[1];
  terrorCode = signal->theData[2];
  Uint32 transid1 = signal->theData[3];
  Uint32 transid2 = signal->theData[4];
  if (tcConnectptr.i >= ctcConnectrecFileSize) {
    errorReport(signal, 3);
    return;
  }//if

  ptrAss(tcConnectptr, tcConnectionrec);
  TcConnectionrec * const regTcPtr = tcConnectptr.p;

  if (likely(! ((regTcPtr->connectState == TcConnectionrec::LOG_CONNECTED) ||
                (regTcPtr->connectState == TcConnectionrec::COPY_CONNECTED))))
  {
    /**
     * This...is unpleasant...
     *   LOG_CONNECTED and COPY_CONNECTED will not release there tcConnectptr
     *   before all outstanding is finished.
     *
     *   CONNECTED on the other hand can, (in ::execABORT)
     *     which means that findTransaction *should* be used
     *     to make sure that correct tcConnectptr is accessed.
     *
     *   However, as LOG_CONNECTED & COPY_CONNECTED only uses 1 tcConnectptr
     *     (and fiddles) with transid and other stuff, I could
     *     not find an easy way to modify the code so that findTransaction
     *     is usable also for them
     */
    if (findTransaction(transid1, transid2, tcOprec, 0) != ZOK)
    {
      jam();
      warningReport(signal, 14);
      return;
    }
  }

  switch (regTcPtr->connectState) {
  case TcConnectionrec::CONNECTED:
    jam();
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
  Uint32 TsenderRef = signal->getSendersBlockRef();
  Uint32 TcommitLen = 5;
  Uint32 Tgci_lo_mask = ~(Uint32)0;

  if (unlikely(!ndb_check_micro_gcp(getNodeInfo(refToNode(signal->getSendersBlockRef())).m_version)))
  {
    jam();
    TcommitLen = 4;
    Tgci_lo_mask = 0;
  }

#ifdef ERROR_INSERT
  Uint32 senderBlockRef = signal->getSendersBlockRef();
#endif

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
      sig4 = TpackedData[Tstep + 4];
      signal->theData[0] = sig0;
      signal->theData[1] = sig1;
      signal->theData[2] = sig2;
      signal->theData[3] = sig3;
      signal->theData[4] = sig4 & Tgci_lo_mask;
      signal->header.theLength = TcommitLen;
      execCOMMIT(signal);
      Tstep += TcommitLen;
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
      LqhKeyConf * lqhKeyConf = CAST_PTR(LqhKeyConf, signal->theData);
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
    case ZFIRE_TRIG_REQ:
      jam();
      ndbassert(FireTrigReq::SignalLength == 4);
      sig0 = TpackedData[Tstep + 0] & 0x0FFFFFFF;
      sig1 = TpackedData[Tstep + 1];
      sig2 = TpackedData[Tstep + 2];
      sig3 = TpackedData[Tstep + 3];
      signal->theData[0] = sig0;
      signal->theData[1] = sig1;
      signal->theData[2] = sig2;
      signal->theData[3] = sig3;
      signal->header.theLength = FireTrigReq::SignalLength;
      signal->header.theSendersBlockRef = TsenderRef;
      execFIRE_TRIG_REQ(signal);
      Tstep += FireTrigReq::SignalLength;
      break;
    default:
      ndbrequire(false);
      return;
    }//switch
#ifdef ERROR_INSERT
    signal->header.theSendersBlockRef = senderBlockRef;
#endif
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
  m_commitAckMarkerHash.remove(removedPtr, key);
#if (defined VM_TRACE || defined ERROR_INSERT) && defined(wl4391_todo)
  ndbrequire(removedPtr.i != RNIL);
  m_commitAckMarkerPool.release(removedPtr);
#else
  if (removedPtr.i != RNIL)
  {
    jam();
    m_commitAckMarkerPool.release(removedPtr);
  }
#endif
#ifdef MARKER_TRACE
  ndbout_c("%u Rem marker[%.8x %.8x]", instance(), key.transid1, key.transid2);
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
  UintR j;
  UintR TpackedListIndex = cpackedListIndex;
  jamEntry();
  for (i = 0; i < TpackedListIndex; i++) {
    Thostptr.i = cpackedList[i];
    ptrAss(Thostptr, hostRecord);
    jam();
    ndbrequire(Thostptr.i - 1 < MAX_NDB_NODES - 1);
    for (j = 0; j < NDB_ARRAY_SIZE(Thostptr.p->lqh_pack); j++)
    {
      struct PackedWordsContainer * container = &Thostptr.p->lqh_pack[j];
      if (container->noOfPackedWords > 0) {
        jam();
        sendPackedSignal(signal, container);
      }
    }
    for (j = 0; j < NDB_ARRAY_SIZE(Thostptr.p->tc_pack); j++)
    {
      struct PackedWordsContainer * container = &Thostptr.p->tc_pack[j];
      if (container->noOfPackedWords > 0) {
        jam();
        sendPackedSignal(signal, container);
      }
    }
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
  
  switch(signal->theData[1])
  {
  case AttributeHeader::RANGE_NO:
    signal->theData[0] = regTcPtr.p->m_scan_curr_range_no;
    break;
  case AttributeHeader::ROW_COUNT:
  case AttributeHeader::COMMIT_COUNT:
  {
    jam();
    FragrecordPtr regFragptr;
    regFragptr.i = regTcPtr.p->fragmentptr;
    c_fragment_pool.getPtr(regFragptr);
    
    signal->theData[0] = regFragptr.p->accFragptr;
    EXECUTE_DIRECT(DBACC, GSN_READ_PSEUDO_REQ, signal, 2);
    break;
  }
  case AttributeHeader::RECORDS_IN_RANGE:
  case AttributeHeader::INDEX_STAT_KEY:
  case AttributeHeader::INDEX_STAT_VALUE:
  {
    jam();
    // scanptr gets reset somewhere within the timeslice
    ScanRecordPtr tmp;
    tmp.i = regTcPtr.p->tcScanRec;
    c_scanRecordPool.getPtr(tmp);
    signal->theData[0] = tmp.p->scanAccPtr;
    EXECUTE_DIRECT(DBTUX, GSN_READ_PSEUDO_REQ, signal, 2);
    break;
  }
  case AttributeHeader::LOCK_REF:
  {
    /* Return 3x 32-bit words
     *  - LQH instance info
     *  - TC operation index
     *  - Bottom 32-bits of LQH-local key-request id (for uniqueness)
     */
    jam();
    signal->theData[0] = (getOwnNodeId() << 16) | regTcPtr.p->fragmentid; 
    signal->theData[1] = regTcPtr.p->tcOprec;
    signal->theData[2] = (Uint32) regTcPtr.p->lqhKeyReqId;
    break;
  }
  case AttributeHeader::OP_ID:
  {
    jam();
    memcpy(signal->theData, &regTcPtr.p->lqhKeyReqId, 8);
    break;
  }
  case AttributeHeader::CORR_FACTOR64:
  {
    Uint32 add = 0;
    ScanRecordPtr tmp;
    tmp.i = regTcPtr.p->tcScanRec;
    if (tmp.i != RNIL)
    {
      c_scanRecordPool.getPtr(tmp);
      add = tmp.p->m_curr_batch_size_rows;
    }

    signal->theData[0] = regTcPtr.p->m_corrFactorLo + add;
    signal->theData[1] = regTcPtr.p->m_corrFactorHi;
    break;
  }
  default:
    ndbrequire(false);
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
  TcConnectionrec * regTcPtr = tcConnectptr.p;
  Uint32 activeCreat = regTcPtr->activeCreat;

  FragrecordPtr regFragptr;
  regFragptr.i = tcConnectptr.p->fragmentptr;
  c_fragment_pool.getPtr(regFragptr);
  fragptr = regFragptr;
  
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
    if (unlikely(activeCreat == Fragrecord::AC_NR_COPY))
    {
      jam();
      ndbrequire(regTcPtr->m_nr_delete.m_cnt);
      regTcPtr->m_nr_delete.m_cnt--;
      if (regTcPtr->m_nr_delete.m_cnt)
      {
	jam();
	/**
	 * Let operation wait for pending NR operations
	 *   even for before writing log...(as it's simpler)
	 */
	
#ifdef VM_TRACE
	/**
	 * Only disk table can have pending ops...
	 */
	TablerecPtr tablePtr;
	tablePtr.i = regTcPtr->tableref;
	ptrCheckGuard(tablePtr, ctabrecFileSize, tablerec);
	ndbrequire(tablePtr.p->m_disk_table);
#endif
	return;
      }
    }

    abortCommonLab(signal);
    break;
  case TcConnectionrec::WAIT_ACC_ABORT:
  case TcConnectionrec::ABORT_QUEUED:
    jam();
/* ------------------------------------------------------------------------- */
/*      IGNORE SINCE ABORT OF THIS OPERATION IS ONGOING ALREADY.             */
/* ------------------------------------------------------------------------- */
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
  TcConnectionrec* regTcPtr = tcConnectptr.p;
  Uint32 activeCreat = regTcPtr->activeCreat;

  FragrecordPtr regFragptr;
  regFragptr.i = regTcPtr->fragmentptr;
  c_fragment_pool.getPtr(regFragptr);
  fragptr = regFragptr;

  TRACE_OP(regTcPtr, "TUPKEYREF");

  if (unlikely(activeCreat == Fragrecord::AC_NR_COPY))
  {
    jam();
    ndbrequire(regTcPtr->m_nr_delete.m_cnt);
    regTcPtr->m_nr_delete.m_cnt--;
    ndbassert(regTcPtr->transactionState == TcConnectionrec::WAIT_TUP ||
	      regTcPtr->transactionState ==TcConnectionrec::WAIT_TUP_TO_ABORT);
  }

  switch (tcConnectptr.p->transactionState) {
  case TcConnectionrec::WAIT_TUP:
    jam();
    abortErrorLab(signal);
    break;
  case TcConnectionrec::COPY_TUPKEY:
    copyTupkeyRefLab(signal);
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
    jamLine(tcConnectptr.p->transactionState);
    ndbrequire(false);
    break;
  }//switch
}//Dblqh::execTUPKEYREF()

void Dblqh::sendPackedSignal(Signal* signal,
                             struct PackedWordsContainer * container)
{
  Uint32 noOfWords = container->noOfPackedWords;
  BlockReference hostRef = container->hostBlockRef;
  container->noOfPackedWords = 0;
  MEMCOPY_NO_WORDS(&signal->theData[0],
                   &container->packedWords[0],
                   noOfWords);
  sendSignal(hostRef, GSN_PACKED_SIGNAL, signal, noOfWords, JBB);
}

void Dblqh::sendCommitLqh(Signal* signal, BlockReference alqhBlockref)
{
  Uint32 instanceKey = refToInstance(alqhBlockref);
  ndbassert(refToMain(alqhBlockref) == DBLQH);

  if (instanceKey > MAX_NDBMT_LQH_THREADS)
  {
    /* No send packed support in these cases */
    jam();
    signal->theData[0] = tcConnectptr.p->clientConnectrec;
    signal->theData[1] = tcConnectptr.p->transid[0];
    signal->theData[2] = tcConnectptr.p->transid[1];
    sendSignal(alqhBlockref, GSN_COMMIT, signal, 3, JBB);
    return;
  }

  HostRecordPtr Thostptr;

  Thostptr.i = refToNode(alqhBlockref);
  ptrCheckGuard(Thostptr, chostFileSize, hostRecord);
  struct PackedWordsContainer * container = &Thostptr.p->lqh_pack[instanceKey];

  Uint32 Tdata[5];
  Tdata[0] = tcConnectptr.p->clientConnectrec;
  Tdata[1] = tcConnectptr.p->gci_hi;
  Tdata[2] = tcConnectptr.p->transid[0];
  Tdata[3] = tcConnectptr.p->transid[1];
  Tdata[4] = tcConnectptr.p->gci_lo;
  Uint32 len = 5;

  if (unlikely(!ndb_check_micro_gcp(getNodeInfo(Thostptr.i).m_version)))
  {
    jam();
    ndbassert(Tdata[4] == 0 || getNodeInfo(Thostptr.i).m_version == 0);
    len = 4;
  }

  if (container->noOfPackedWords > 25 - len) {
    jam();
    sendPackedSignal(signal, container);
  } else {
    jam();
    updatePackedList(signal, Thostptr.p, Thostptr.i);
  }

  Tdata[0] |= (ZCOMMIT << 28);
  Uint32 pos = container->noOfPackedWords;
  container->noOfPackedWords = pos + len;
  memcpy(&container->packedWords[pos], &Tdata[0], len << 2);
}

void Dblqh::sendCompleteLqh(Signal* signal, BlockReference alqhBlockref)
{
  Uint32 instanceKey = refToInstance(alqhBlockref);
  ndbassert(refToMain(alqhBlockref) == DBLQH);

  if (instanceKey > MAX_NDBMT_LQH_THREADS)
  {
    /* No send packed support in these cases */
    jam();
    signal->theData[0] = tcConnectptr.p->clientConnectrec;
    signal->theData[1] = tcConnectptr.p->transid[0];
    signal->theData[2] = tcConnectptr.p->transid[1];
    sendSignal(alqhBlockref, GSN_COMPLETE, signal, 3, JBB);
    return;
  }

  HostRecordPtr Thostptr;

  Thostptr.i = refToNode(alqhBlockref);
  ptrCheckGuard(Thostptr, chostFileSize, hostRecord);
  struct PackedWordsContainer * container = &Thostptr.p->lqh_pack[instanceKey];

  Uint32 Tdata[3];
  Tdata[0] = tcConnectptr.p->clientConnectrec;
  Tdata[1] = tcConnectptr.p->transid[0];
  Tdata[2] = tcConnectptr.p->transid[1];
  Uint32 len = 3;

  if (container->noOfPackedWords > 22) {
    jam();
    sendPackedSignal(signal, container);
  } else {
    jam();
    updatePackedList(signal, Thostptr.p, Thostptr.i);
  }

  Tdata[0] |= (ZCOMPLETE << 28);
  Uint32 pos = container->noOfPackedWords;
  container->noOfPackedWords = pos + len;
  memcpy(&container->packedWords[pos], &Tdata[0], len << 2);
}

void Dblqh::sendCommittedTc(Signal* signal, BlockReference atcBlockref)
{
  Uint32 instanceKey = refToInstance(atcBlockref);

  ndbassert(refToMain(atcBlockref) == DBTC);
  if (instanceKey > MAX_NDBMT_TC_THREADS)
  {
    /* No send packed support in these cases */
    jam();
    signal->theData[0] = tcConnectptr.p->clientConnectrec;
    signal->theData[1] = tcConnectptr.p->transid[0];
    signal->theData[2] = tcConnectptr.p->transid[1];
    sendSignal(atcBlockref, GSN_COMMITTED, signal, 3, JBB);
    return;
  }

  HostRecordPtr Thostptr;
  Thostptr.i = refToNode(atcBlockref);
  ptrCheckGuard(Thostptr, chostFileSize, hostRecord);
  struct PackedWordsContainer * container = &Thostptr.p->tc_pack[instanceKey];

  Uint32 Tdata[3];
  Tdata[0] = tcConnectptr.p->clientConnectrec;
  Tdata[1] = tcConnectptr.p->transid[0];
  Tdata[2] = tcConnectptr.p->transid[1];
  Uint32 len = 3;

  if (container->noOfPackedWords > 22) {
    jam();
    sendPackedSignal(signal, container);
  } else {
    jam();
    updatePackedList(signal, Thostptr.p, Thostptr.i);
  }

  Tdata[0] |= (ZCOMMITTED << 28);
  Uint32 pos = container->noOfPackedWords;
  container->noOfPackedWords = pos + len;
  memcpy(&container->packedWords[pos], &Tdata[0], len << 2);
}

void Dblqh::sendCompletedTc(Signal* signal, BlockReference atcBlockref)
{
  Uint32 instanceKey = refToInstance(atcBlockref);

  ndbassert(refToMain(atcBlockref) == DBTC);
  if (instanceKey > MAX_NDBMT_TC_THREADS)
  {
    /* No handling of send packed in those cases */
    jam();
    signal->theData[0] = tcConnectptr.p->clientConnectrec;
    signal->theData[1] = tcConnectptr.p->transid[0];
    signal->theData[2] = tcConnectptr.p->transid[1];
    sendSignal(atcBlockref, GSN_COMPLETED, signal, 3, JBB);
    return;
  }

  HostRecordPtr Thostptr;
  Thostptr.i = refToNode(atcBlockref);
  ptrCheckGuard(Thostptr, chostFileSize, hostRecord);
  struct PackedWordsContainer * container = &Thostptr.p->tc_pack[instanceKey];

  Uint32 Tdata[3];
  Tdata[0] = tcConnectptr.p->clientConnectrec;
  Tdata[1] = tcConnectptr.p->transid[0];
  Tdata[2] = tcConnectptr.p->transid[1];
  Uint32 len = 3;

  if (container->noOfPackedWords > 22) {
    jam();
    sendPackedSignal(signal, container);
  } else {
    jam();
    updatePackedList(signal, Thostptr.p, Thostptr.i);
  }

  Tdata[0] |= (ZCOMPLETED << 28);
  Uint32 pos = container->noOfPackedWords;
  container->noOfPackedWords = pos + len;
  memcpy(&container->packedWords[pos], &Tdata[0], len << 2);
}

void Dblqh::sendLqhkeyconfTc(Signal* signal, BlockReference atcBlockref)
{
  LqhKeyConf* lqhKeyConf;
  struct PackedWordsContainer * container;
  bool send_packed = true;
  HostRecordPtr Thostptr;
  Thostptr.i = refToNode(atcBlockref);
  Uint32 instanceKey = refToInstance(atcBlockref);
  ptrCheckGuard(Thostptr, chostFileSize, hostRecord);
  Uint32 block = refToMain(atcBlockref);

  if (block == DBLQH)
  {
    if (instanceKey <= MAX_NDBMT_LQH_THREADS)
    {
      container = &Thostptr.p->lqh_pack[instanceKey];
    }
    else
    {
      send_packed = false;
    }
  }
  else if (block == DBTC)
  {
    if (instanceKey <= MAX_NDBMT_TC_THREADS)
    {
      container = &Thostptr.p->tc_pack[instanceKey];
    }
    else
    {
      send_packed = false;
    }
  }
  else
  {
    send_packed = false;
  }

/*******************************************************************
// Normal path
// This signal was intended for DBTC as part of the normal transaction
// execution.
// More unusual path
// This signal was intended for DBLQH as part of log execution or
// node recovery.
// Yet another path
// Intended for DBSPJ as part of join processing
********************************************************************/
  if (send_packed)
  {
    if (container->noOfPackedWords > (25 - LqhKeyConf::SignalLength)) {
      jam();
      sendPackedSignal(signal, container);
    } else {
      jam();
      updatePackedList(signal, Thostptr.p, Thostptr.i);
    }//if
    lqhKeyConf = (LqhKeyConf *)
      &container->packedWords[container->noOfPackedWords];
    container->noOfPackedWords += LqhKeyConf::SignalLength;
  }
  else
  {
    lqhKeyConf = (LqhKeyConf *)&signal->theData[0];
  }

  Uint32 ptrAndType = tcConnectptr.i | (ZLQHKEYCONF << 28);
  Uint32 tcOprec = tcConnectptr.p->tcOprec;
  Uint32 ownRef = cownref;
  lqhKeyConf->connectPtr = ptrAndType;
  lqhKeyConf->opPtr = tcOprec;
  lqhKeyConf->userRef = ownRef;

  Uint32 readlenAi = tcConnectptr.p->readlenAi;
  Uint32 transid1 = tcConnectptr.p->transid[0];
  Uint32 transid2 = tcConnectptr.p->transid[1];
  Uint32 noFiredTriggers = tcConnectptr.p->noFiredTriggers;
  lqhKeyConf->readLen = readlenAi;
  lqhKeyConf->transId1 = transid1;
  lqhKeyConf->transId2 = transid2;
  lqhKeyConf->noFiredTriggers = noFiredTriggers;

  if (!send_packed)
  {
    lqhKeyConf->connectPtr = tcConnectptr.i;
    sendSignal(atcBlockref, GSN_LQHKEYCONF,
               signal, LqhKeyConf::SignalLength, JBB);
  }
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
  if (findTransaction(transid1, transid2, tcOprec, 0) != ZOK) {
    jam();
    return;
  }//if

  receive_keyinfo(signal, 
		  signal->theData+KeyInfo::HeaderLength, 
		  signal->getLength()-KeyInfo::HeaderLength);
}

void
Dblqh::receive_keyinfo(Signal* signal, 
		       Uint32 * data, Uint32 len)
{
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

  Uint32 errorCode = 
    handleLongTupKey(signal, data, len);
  
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
    c_fragment_pool.getPtr(regFragptr);
    fragptr = regFragptr;
    endgettupkeyLab(signal);
  }
  return;
}//Dblqh::execKEYINFO()

/* ------------------------------------------------------------------------- */
/* FILL IN KEY DATA INTO DATA BUFFERS.                                       */
/* ------------------------------------------------------------------------- */
Uint32 Dblqh::handleLongTupKey(Signal* signal,
			       Uint32* dataPtr,
			       Uint32 len) 
{
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  Uint32 total = regTcPtr->save1 + len;
  Uint32 primKeyLen = regTcPtr->primKeyLen;

  if (unlikely(total > primKeyLen))
  {
    /**
     * DBLQH 6.3 has the bad taste to send more KEYINFO than what is
     *  really in the key...up to 3 words extra
     */
    Uint32 extra = total - primKeyLen;
    ndbrequire(extra <= 3);
    ndbrequire(len > extra);
    len -= extra;
  }

  bool ok= appendToSection(regTcPtr->keyInfoIVal,
                           dataPtr,
                           len);
  if (unlikely(!ok))
  {
    jam();
    return ZGET_DATAREC_ERROR;
  }
  
  regTcPtr->save1 = total;
  return (total >= primKeyLen ? 0 : 1);
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
                      tcOprec, 0) != ZOK) {
    jam();
    return;
  }//if

  receive_attrinfo(signal, 
		   signal->getDataPtrSend()+AttrInfo::HeaderLength,
		   signal->getLength()-AttrInfo::HeaderLength);
}//Dblqh::execATTRINFO()

void
Dblqh::receive_attrinfo(Signal* signal, Uint32 * dataPtr, Uint32 length)
{
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  Uint32 totReclenAi = regTcPtr->totReclenAi;
  Uint32 currReclenAi = regTcPtr->currReclenAi + length;
  regTcPtr->currReclenAi = currReclenAi;
  if (totReclenAi == currReclenAi) {
    switch (regTcPtr->transactionState) {
    case TcConnectionrec::WAIT_ATTR:
    {
      jam();
      fragptr.i = regTcPtr->fragmentptr;
      c_fragment_pool.getPtr(fragptr);
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
}

/* ************************************************************************>> */
/*  TUP_ATTRINFO: Interpreted execution in DBTUP generates redo-log info      */
/*  which is sent back to DBLQH for logging. This is because the decision     */
/*  to execute or not is made in DBTUP and thus we cannot start logging until */
/*  DBTUP part has been run.                                                  */
/* ************************************************************************>> */
void Dblqh::execTUP_ATTRINFO(Signal* signal) 
{
  TcConnectionrec *regTcConnectionrec = tcConnectionrec;
  Uint32 tcIndex = signal->theData[0];
  Uint32 ttcConnectrecFileSize = ctcConnectrecFileSize;
  jamEntry();
  tcConnectptr.i = tcIndex;
  ptrCheckGuard(tcConnectptr, ttcConnectrecFileSize, regTcConnectionrec);
  TcConnectionrec * const regTcPtr = tcConnectptr.p;

  ndbrequire(regTcPtr->transactionState == TcConnectionrec::WAIT_TUP);
  
  /* TUP_ATTRINFO signal is unrelated to ATTRINFO
   * It just transports a section IVAL from TUP back to 
   * LQH
   */
  ndbrequire(signal->header.theLength == 3);
  Uint32 tupAttrInfoWords= signal->theData[1];
  Uint32 tupAttrInfoIVal= signal->theData[2];

  ndbassert(tupAttrInfoWords > 0);
  ndbassert(tupAttrInfoIVal != RNIL);

  /* If we have stored ATTRINFO that we sent to TUP, 
   * free it now
   */
  if (regTcPtr->attrInfoIVal != RNIL)
  {
    /* We should be expecting to receive attrInfo back */
    ndbassert( !(regTcPtr->m_flags & 
                 TcConnectionrec::OP_SAVEATTRINFO) );
    releaseSection( regTcPtr->attrInfoIVal );
    regTcPtr->attrInfoIVal= RNIL;
  }

  /* Store reference to ATTRINFO from TUP */
  regTcPtr->attrInfoIVal= tupAttrInfoIVal;
  regTcPtr->currTupAiLen= tupAttrInfoWords;

}//Dblqh::execTUP_ATTRINFO()

/* ------------------------------------------------------------------------- */
/* -------                HANDLE ATTRINFO FROM LQH                   ------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void Dblqh::lqhAttrinfoLab(Signal* signal, Uint32* dataPtr, Uint32 length) 
{
  /* Store received AttrInfo in a long section */
  jam();
  if (saveAttrInfoInSection(dataPtr, length) == ZOK) {
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
}//Dblqh::lqhAttrinfoLab()

/* ------------------------------------------------------------------------- */
/* ------         FIND TRANSACTION BY USING HASH TABLE               ------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
int Dblqh::findTransaction(UintR Transid1, UintR Transid2, UintR TcOprec,
                           Uint32 hi)
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
        (locTcConnectptr.p->tcOprec == TcOprec) &&
        (locTcConnectptr.p->tcHashKeyHi == hi)) {
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
/* -------           SAVE ATTRINFO INTO ATTR SECTION                 ------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
int Dblqh::saveAttrInfoInSection(const Uint32* dataPtr, Uint32 len) 
{
  TcConnectionrec * const regTcPtr = tcConnectptr.p;

  bool ok= appendToSection(regTcPtr->attrInfoIVal,
                           dataPtr,
                           len);

  if (unlikely(!ok))
  {
    jam();
    terrorCode = ZGET_ATTRINBUF_ERROR;
    return ZGET_ATTRINBUF_ERROR;
  }//if

  if (regTcPtr->m_flags & TcConnectionrec::OP_SAVEATTRINFO)
    regTcPtr->currTupAiLen += len;
  
  return ZOK;
} // saveAttrInfoInSection


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
  locTcConnectptr.p->gci_hi = 0;
  locTcConnectptr.p->gci_lo = 0;
  cfirstfreeTcConrec = nextTc;
  tcConnectptr = locTcConnectptr;
  locTcConnectptr.p->connectState = TcConnectionrec::CONNECTED;
}//Dblqh::seizeTcrec()

bool
Dblqh::checkTransporterOverloaded(Signal* signal,
                                  const NodeBitmask& all,
                                  const LqhKeyReq* req)
{
  // nodes likely to be affected by this op
  NodeBitmask mask;
  // tc
  Uint32 tc_node = refToNode(req->tcBlockref);
  if (tc_node < MAX_NODES) // not worth to crash here
    mask.set(tc_node);
  const Uint8 op = LqhKeyReq::getOperation(req->requestInfo);
  if (op == ZREAD || op == ZREAD_EX || op == ZUNLOCK) {
    // the receiver
    Uint32 api_node = refToNode(req->variableData[0]);
    if (api_node < MAX_NODES) // not worth to crash here
      mask.set(api_node);
  } else {
    // next replica
    Uint32 replica_node = LqhKeyReq::getNextReplicaNodeId(req->fragmentData);
    if (replica_node < MAX_NODES) // could be ZNIL
      mask.set(replica_node);
    // event subscribers
    const Suma* suma = (Suma*)globalData.getBlock(SUMA);
    mask.bitOR(suma->getSubscriberNodes());
  }
  mask.bitAND(all);
  return !mask.isclear();
}

void Dblqh::execSIGNAL_DROPPED_REP(Signal* signal)
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
  case GSN_LQHKEYREQ:
  {
    jam(); 
    /* Get original signal data - unfortunately it may
     * have been truncated.  We must not read beyond
     * word # 22
     * We will notify the client that their LQHKEYREQ
     * failed
     */
    tcConnectptr.i = RNIL;
    const LqhKeyReq * const truncatedLqhKeyReq = 
      (LqhKeyReq *) &rep->originalData[0];
    
    earlyKeyReqAbort(signal, truncatedLqhKeyReq, ZGET_DATAREC_ERROR);

    break;
  }
  case GSN_SCAN_FRAGREQ:
  {
    jam();
    /* Get original signal data - unfortunately it may
     * have been truncated.  We must not read beyond
     * word # 22
     * We will notify the client that their SCAN_FRAGREQ
     * failed
     */
    // TODO : Handle fragmented failure
    const ScanFragReq* const truncatedScanFragReq = 
      (ScanFragReq*) &rep->originalData[0];
    const Uint32 senderData= truncatedScanFragReq->senderData;
    const Uint32 transid1= truncatedScanFragReq->transId1;
    const Uint32 transid2= truncatedScanFragReq->transId2;

    /* Send SCAN_FRAGREF back to the client */
    ScanFragRef* ref= (ScanFragRef*)&signal->theData[0];
    ref->senderData= senderData;
    ref->transId1= transid1;
    ref->transId2= transid2;
    ref->errorCode= ZGET_ATTRINBUF_ERROR;
    
    sendSignal(signal->senderBlockRef(), GSN_SCAN_FRAGREF, signal,
               ScanFragRef::SignalLength, JBB);
    break;
  }
  default:
    jam();
    /* Don't expect dropped signals for other GSNs,
     * default handling
     */
    SimulatedBlock::execSIGNAL_DROPPED_REP(signal);
  };
  
  return;
}

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
  SectionHandle handle(this, signal);
  tcConnectptr.i = RNIL;

  {
    const NodeBitmask& all = globalTransporterRegistry.get_status_overloaded();
    if (unlikely(!all.isclear()))
    {
      if (checkTransporterOverloaded(signal, all, lqhKeyReq))
      {
        /**
         * TODO: We should have counters for this...
         */
        jam();
        releaseSections(handle);
        earlyKeyReqAbort(signal, lqhKeyReq, ZTRANSPORTER_OVERLOADED_ERROR);
        return;
      }
    }
  }

  if (ERROR_INSERTED_CLEAR(5047))
  {
    jam();
    releaseSections(handle);
    earlyKeyReqAbort(signal, lqhKeyReq, ZTRANSPORTER_OVERLOADED_ERROR);
    return;
  }

  sig0 = lqhKeyReq->clientConnectPtr;
  if (cfirstfreeTcConrec != RNIL && !ERROR_INSERTED_CLEAR(5031)) {
    jamEntry();
    seizeTcrec();
  } else {
/* ------------------------------------------------------------------------- */
/* NO FREE TC RECORD AVAILABLE, THUS WE CANNOT HANDLE THE REQUEST.           */
/* ------------------------------------------------------------------------- */
    releaseSections(handle);
    earlyKeyReqAbort(signal, lqhKeyReq, ZNO_TC_CONNECT_ERROR);
    return;
  }//if

  if(ERROR_INSERTED(5038) && 
     refToNode(signal->getSendersBlockRef()) != getOwnNodeId()){
    jam();
    releaseSections(handle);
    SET_ERROR_INSERT_VALUE(5039);
    return;
  }
  
  cTotalLqhKeyReqCount++;
  c_Counters.operations++;

  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  Uint32 senderRef = regTcPtr->clientBlockref = signal->senderBlockRef();
  regTcPtr->clientConnectrec = sig0;
  regTcPtr->tcOprec = sig0;
  regTcPtr->tcHashKeyHi = 0;
  regTcPtr->storedProcId = ZNIL;
  regTcPtr->lqhKeyReqId = cTotalLqhKeyReqCount;
  regTcPtr->m_flags= 0;
  bool isLongReq= false;
  if (handle.m_cnt > 0)
  {
    isLongReq= true;
    regTcPtr->m_flags|= TcConnectionrec::OP_ISLONGREQ;
  }

  UintR attrLenFlags = lqhKeyReq->attrLen;
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
  if ((op == ZREAD || op == ZREAD_EX) && !getAllowRead()){
    releaseSections(handle);
    earlyKeyReqAbort(signal, lqhKeyReq, ZNODE_SHUTDOWN_IN_PROGESS);
    return;
  }

  if (unlikely(get_node_status(refToNode(sig5)) != ZNODE_UP))
  {
    releaseSections(handle);
    earlyKeyReqAbort(signal, lqhKeyReq, ZNODE_FAILURE_ERROR);
    return;
  }
  
  Uint32 senderVersion = getNodeInfo(refToNode(senderRef)).m_version;

  regTcPtr->tcScanInfo  = lqhKeyReq->scanInfo;
  regTcPtr->indTakeOver = LqhKeyReq::getScanTakeOverFlag(attrLenFlags);
  regTcPtr->m_reorg     = LqhKeyReq::getReorgFlag(attrLenFlags);

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
  if (LqhKeyReq::getMarkerFlag(Treqinfo))
  {
    struct CommitAckMarker check;
    CommitAckMarkerPtr markerPtr;
    jam();
    check.transid1 = regTcPtr->transid[0];
    check.transid2 = regTcPtr->transid[1];

    if (m_commitAckMarkerHash.find(markerPtr, check))
    {
      /*
        A commit ack marker was already placed here for this transaction.
        We increase the reference count to ensure we don't remove the
        commit ack marker prematurely.
      */
      markerPtr.p->reference_count++;
#ifdef MARKER_TRACE
      ndbout_c("Inc marker[%.8x %.8x] op: %u ref: %u", 
               markerPtr.p->transid1, markerPtr.p->transid2, 
               tcConnectptr.i, markerPtr.p->reference_count);
#endif
    }
    else
    {
      m_commitAckMarkerHash.seize(markerPtr);
      if (markerPtr.i == RNIL)
      {
        releaseSections(handle);
        earlyKeyReqAbort(signal, lqhKeyReq, ZNO_FREE_MARKER_RECORDS_ERROR);
        return;
      }
      markerPtr.p->transid1 = sig1;
      markerPtr.p->transid2 = sig2;
      markerPtr.p->apiRef   = sig3;
      markerPtr.p->apiOprec = sig4;
      const NodeId tcNodeId  = refToNode(sig5);
      markerPtr.p->tcNodeId = tcNodeId;
      markerPtr.p->reference_count = 1;
      m_commitAckMarkerHash.add(markerPtr);

#ifdef MARKER_TRACE
      ndbout_c("%u Add marker[%.8x %.8x] op: %u", instance(), markerPtr.p->transid1, markerPtr.p->transid2, tcConnectptr.i);
#endif
    }
    regTcPtr->commitAckMarker = markerPtr.i;
  } 
  
  regTcPtr->reqinfo = Treqinfo;
  regTcPtr->lastReplicaNo = LqhKeyReq::getLastReplicaNo(Treqinfo);
  regTcPtr->dirtyOp       = LqhKeyReq::getDirtyFlag(Treqinfo);
  regTcPtr->opExec        = LqhKeyReq::getInterpretedFlag(Treqinfo);
  regTcPtr->opSimple      = LqhKeyReq::getSimpleFlag(Treqinfo);
  regTcPtr->seqNoReplica  = LqhKeyReq::getSeqNoReplica(Treqinfo);
  regTcPtr->apiVersionNo  = 0; 
  regTcPtr->m_use_rowid   = LqhKeyReq::getRowidFlag(Treqinfo);
  regTcPtr->m_dealloc     = 0;
  if (unlikely(senderVersion < NDBD_ROWID_VERSION))
  {
    regTcPtr->operation = op;
    regTcPtr->lockType = LqhKeyReq::getLockType(Treqinfo);
  }
  else
  {
    regTcPtr->operation = (Operation_t) op == ZREAD_EX ? ZREAD : (Operation_t) op;
    regTcPtr->lockType = 
      op == ZREAD_EX ? ZUPDATE : 
      (Operation_t) op == ZWRITE ? ZINSERT : 
      (Operation_t) op == ZREFRESH ? ZINSERT :
      (Operation_t) op == ZUNLOCK ? ZREAD : // lockType not relevant for unlock req
      (Operation_t) op;
  }

  if (regTcPtr->dirtyOp)
  {
    ndbrequire(regTcPtr->opSimple);
  }
  
  CRASH_INSERTION2(5041, (op == ZREAD && 
                          (regTcPtr->opSimple || regTcPtr->dirtyOp) &&
                          refToNode(signal->senderBlockRef()) != cownNodeid));

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
  UintR TstoredProcIndicator = LqhKeyReq::getStoredProcFlag(attrLenFlags);
  if (TstoredProcIndicator == 1) {
    regTcPtr->storedProcId = lqhKeyReq->variableData[nextPos] & ZNIL;
    nextPos++;
  }//if
  UintR TreadLenAiIndicator = LqhKeyReq::getReturnedReadLenAIFlag(Treqinfo);
  if (TreadLenAiIndicator == 1) {
    regTcPtr->readlenAi = lqhKeyReq->variableData[nextPos] & ZNIL;
    nextPos++;
  }//if

  Uint32 TanyValueFlag = LqhKeyReq::getCorrFactorFlag(Treqinfo);
  if (isLongReq && TanyValueFlag == 1)
  {
    /**
     * For short lqhkeyreq, ai-length in-signal is stored in same pos...
     */
    regTcPtr->m_corrFactorLo = lqhKeyReq->variableData[nextPos + 0];
    regTcPtr->m_corrFactorHi = lqhKeyReq->variableData[nextPos + 1];
    nextPos += 2;
  }

  regTcPtr->m_fire_trig_pass = 0;
  Uint32 Tdeferred = LqhKeyReq::getDeferredConstraints(Treqinfo);
  if (isLongReq && Tdeferred)
  {
    regTcPtr->m_flags |= TcConnectionrec::OP_DEFERRED_CONSTRAINTS;
  }

  UintR TitcKeyLen = 0;
  Uint32 keyLenWithLQHReq = 0;
  UintR TreclenAiLqhkey   = 0;

  if (isLongReq)
  {
    /* Long LQHKEYREQ indicates Key and AttrInfo presence and
     * size via section lengths
     */
    SegmentedSectionPtr keyInfoSection, attrInfoSection;
    
    handle.getSection(keyInfoSection,
                      LqhKeyReq::KeyInfoSectionNum);

    ndbassert(keyInfoSection.i != RNIL);

    regTcPtr->keyInfoIVal= keyInfoSection.i;
    TitcKeyLen= keyInfoSection.sz;
    keyLenWithLQHReq= TitcKeyLen;

    Uint32 totalAttrInfoLen= 0;
    if (handle.getSection(attrInfoSection,
                          LqhKeyReq::AttrInfoSectionNum))
    {
      regTcPtr->attrInfoIVal= attrInfoSection.i;
      totalAttrInfoLen= attrInfoSection.sz;
    }

    regTcPtr->reclenAiLqhkey = 0;
    regTcPtr->currReclenAi = totalAttrInfoLen;
    regTcPtr->totReclenAi = totalAttrInfoLen;

    /* Detach sections from the handle, we are now responsible
     * for freeing them when appropriate
     */
    handle.clear();
  }
  else
  {
    /* Short LQHKEYREQ, Key and Attr sizes are in
     * signal, along with some data
     */
    TreclenAiLqhkey= LqhKeyReq::getAIInLqhKeyReq(Treqinfo);
    regTcPtr->reclenAiLqhkey = TreclenAiLqhkey;
    regTcPtr->currReclenAi = TreclenAiLqhkey;
    TitcKeyLen = LqhKeyReq::getKeyLen(Treqinfo);
    regTcPtr->totReclenAi = LqhKeyReq::getAttrLen(attrLenFlags);

    /* Note key can be length zero for NR when Rowid used */
    keyLenWithLQHReq= MIN(TitcKeyLen, LqhKeyReq::MaxKeyInfo);

    bool ok= appendToSection(regTcPtr->keyInfoIVal,
                             &lqhKeyReq->variableData[ nextPos ],
                             keyLenWithLQHReq);
    if (unlikely(!ok))
    {
      jam();
      earlyKeyReqAbort(signal, lqhKeyReq, ZGET_DATAREC_ERROR);
      return;
    }

    nextPos+= keyLenWithLQHReq;
  }
  
  regTcPtr->primKeyLen = TitcKeyLen;

  /* Only node restart copy allowed to send no KeyInfo */
  if (unlikely(keyLenWithLQHReq == 0))
  {
    if (refToMain(senderRef) == DBSPJ)
    {
      jam();
      ndbassert(! LqhKeyReq::getNrCopyFlag(Treqinfo));
      
      /* Reply with NO_TUPLE_FOUND */
      earlyKeyReqAbort(signal, lqhKeyReq, ZNO_TUPLE_FOUND);
      return;
    }

    if (! LqhKeyReq::getNrCopyFlag(Treqinfo))
    {
      LQHKEY_error(signal, 3);
      return;
    }//if
  }

  sig0 = lqhKeyReq->variableData[nextPos + 0];
  sig1 = lqhKeyReq->variableData[nextPos + 1];
  regTcPtr->m_row_id.m_page_no = sig0;
  regTcPtr->m_row_id.m_page_idx = sig1;
  nextPos += 2 * LqhKeyReq::getRowidFlag(Treqinfo);

  sig2 = lqhKeyReq->variableData[nextPos + 0];
  sig3 = cnewestGci;
  /* If gci_hi provided, take it and set gci_lo to max value
   * Otherwise, it will be decided by TUP at commit time as normal
   */
  regTcPtr->gci_hi = LqhKeyReq::getGCIFlag(Treqinfo) ? sig2 : sig3;
  regTcPtr->gci_lo = LqhKeyReq::getGCIFlag(Treqinfo) ? ~Uint32(0) : 0;
  nextPos += LqhKeyReq::getGCIFlag(Treqinfo);
  
  if (LqhKeyReq::getRowidFlag(Treqinfo))
  {
    ndbassert(refToMain(senderRef) != DBTC);
  }
  else if(op == ZINSERT)
  {
    ndbassert(refToMain(senderRef) == DBTC);
  }
  
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
  /* Check that no equal element exists */
  ndbassert(findTransaction(regTcPtr->transid[0], regTcPtr->transid[1], 
                            regTcPtr->tcOprec, 0) == ZNOT_FOUND);
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
    ndbassert(localNextTcConnectptr.p->prevHashRec == RNIL);
    localNextTcConnectptr.p->prevHashRec = tcConnectptr.i;
  }//if
  if (tabptr.i >= ctabrecFileSize) {
    LQHKEY_error(signal, 5);
    return;
  }//if
  ptrAss(tabptr, tablerec);
  if(table_version_major_lqhkeyreq(tabptr.p->schemaVersion) != 
     table_version_major_lqhkeyreq(schemaVersion)){
    LQHKEY_abort(signal, 5);
    return;
  }

  if (unlikely(tabptr.p->tableStatus != Tablerec::TABLE_DEFINED))
  {
    if (check_tabstate(signal, tabptr.p, op))
      return;
  }
  
  regTcPtr->tableref = tabptr.i;
  regTcPtr->m_disk_table = tabptr.p->m_disk_table;
  if(refToMain(signal->senderBlockRef()) == RESTORE)
    regTcPtr->m_disk_table &= !LqhKeyReq::getNoDiskFlag(Treqinfo);
  else if(op == ZREAD || op == ZREAD_EX || op == ZUPDATE)
    regTcPtr->m_disk_table &= !LqhKeyReq::getNoDiskFlag(Treqinfo);
  
  if (op == ZREAD || op == ZREAD_EX || op == ZUNLOCK)
    tabptr.p->usageCountR++;
  else
    tabptr.p->usageCountW++;
  
  if (!getFragmentrec(signal, regTcPtr->fragmentid)) {
    LQHKEY_error(signal, 6);
    return;
  }//if

  if (LqhKeyReq::getNrCopyFlag(Treqinfo))
  {
    ndbassert(refToMain(senderRef) == DBLQH);
    ndbassert(LqhKeyReq::getRowidFlag(Treqinfo));
    if (! (fragptr.p->fragStatus == Fragrecord::ACTIVE_CREATION))
    {
      ndbout_c("fragptr.p->fragStatus: %d",
	       fragptr.p->fragStatus);
      CRASH_INSERTION(5046);
    }
    ndbassert(fragptr.p->fragStatus == Fragrecord::ACTIVE_CREATION);
    fragptr.p->m_copy_started_state = Fragrecord::AC_NR_COPY;
  }
  
  Uint8 TcopyType = fragptr.p->fragCopy;
  Uint32 logPart = fragptr.p->m_log_part_ptr_i;
  tfragDistKey = fragptr.p->fragDistributionKey;
  if (fragptr.p->fragStatus == Fragrecord::ACTIVE_CREATION) {
    jam();
    regTcPtr->activeCreat = fragptr.p->m_copy_started_state;
    CRASH_INSERTION(5002);
    CRASH_INSERTION2(5042, tabptr.i == c_error_insert_table_id);
  } else {
    regTcPtr->activeCreat = Fragrecord::AC_NORMAL;
  }//if
  regTcPtr->replicaType = TcopyType;
  regTcPtr->fragmentptr = fragptr.i;
  regTcPtr->m_log_part_ptr_i = logPart;
  Uint8 TdistKey = LqhKeyReq::getDistributionKey(attrLenFlags);
  if ((tfragDistKey != TdistKey) &&
      (regTcPtr->seqNoReplica == 0) &&
      (regTcPtr->dirtyOp == ZFALSE)) 
  {
    /* ----------------------------------------------------------------------
     * WE HAVE DIFFERENT OPINION THAN THE DIH THAT STARTED THE TRANSACTION. 
     * THE REASON COULD BE THAT THIS IS AN OLD DISTRIBUTION WHICH IS NO LONGER
     * VALID TO USE. THIS MUST BE CHECKED.
     * ONE IS ADDED TO THE DISTRIBUTION KEY EVERY TIME WE ADD A NEW REPLICA.
     * FAILED REPLICAS DO NOT AFFECT THE DISTRIBUTION KEY. THIS MEANS THAT THE 
     * MAXIMUM DEVIATION CAN BE ONE BETWEEN THOSE TWO VALUES.              
     * --------------------------------------------------------------------- */
    Int8 tmp = (TdistKey - tfragDistKey);
    tmp = (tmp < 0 ? - tmp : tmp);
    if ((tmp <= 1) || (tfragDistKey == 0)) {
      LQHKEY_abort(signal, 0);
      return;
    }//if
    LQHKEY_error(signal, 1);
    // Never get here
  }//if

  /*
   * Interpreted updates and deletes may require different AttrInfo in 
   * different replicas, as only the primary executes the interpreted 
   * program, and the effect of the program rather than the program
   * should be logged.
   * Non interpreted inserts, updates, writes and deletes use the same
   * AttrInfo in all replicas.
   * All reads only run on one replica, and are not logged.
   * The AttrInfo section is passed to TUP attached to the TUPKEYREQ
   * signal below.
   *
   * Normal processing : 
   *   - LQH passes ATTRINFO section to TUP attached to direct TUPKEYREQ 
   *     signal
   *   - TUP processes request and sends direct TUPKEYCONF back to LQH
   *   - LQH continues processing (logging, forwarding LQHKEYREQ to other
   *     replicas as necessary)
   *   - LQH frees ATTRINFO section
   *   Note that TUP is not responsible for freeing the passed ATTRINFO
   *   section, LQH is.
   *
   * Interpreted Update / Delete processing 
   *   - LQH passes ATTRINFO section to TUP attached to direct TUPKEYREQ 
   *     signal
   *   - TUP processes request, generating new ATTRINFO data
   *   - If new AttrInfo data is > 0 words, TUP sends it back to LQH as
   *     a long section attached to a single ATTRINFO signal.
   *     - LQH frees the original AttrInfo section and stores a ref to 
   *       the new section
   *   - TUP sends direct TUPKEYCONF back to LQH with new ATTRINFO length
   *   - If the new ATTRINFO is > 0 words, 
   *       - LQH continues processing with it (logging, forwarding 
   *         LQHKEYREQ to other replicas as necessary)
   *       - LQH frees the new ATTRINFO section
   *   - If the new ATTRINFO is 0 words, LQH frees the original ATTRINFO
   *     section and continues processing (logging, forwarding LQHKEYREQ
   *     to other replicas as necessary)
   *
   */
  bool attrInfoToPropagate= 
    (regTcPtr->totReclenAi != 0) &&
    (regTcPtr->operation != ZREAD) &&
    (regTcPtr->operation != ZDELETE) &&
    (regTcPtr->operation != ZUNLOCK);
  bool tupCanChangePropagatedAttrInfo= (regTcPtr->opExec == 1);
  
  bool saveAttrInfo= 
    attrInfoToPropagate &&
    (! tupCanChangePropagatedAttrInfo);
  
  if (saveAttrInfo)
    regTcPtr->m_flags|= TcConnectionrec::OP_SAVEATTRINFO;
  
  /* Handle any AttrInfo we received with the LQHKEYREQ */
  if (regTcPtr->currReclenAi != 0)
  {
    jam();
    if (isLongReq)
    {
      /* Long LQHKEYREQ */
      jam();
      
      regTcPtr->currTupAiLen= saveAttrInfo ?
        regTcPtr->totReclenAi :
        0;
    }
    else
    {
      /* Short LQHKEYREQ */
      jam();

      /* Lets put the AttrInfo into a segmented section */
      bool ok= appendToSection(regTcPtr->attrInfoIVal,
                               lqhKeyReq->variableData + nextPos,
                               TreclenAiLqhkey);
      if (unlikely(!ok))
      {
        jam();
        terrorCode= ZGET_DATAREC_ERROR;
        abortErrorLab(signal);
        return;
      }
        
      if (saveAttrInfo)
        regTcPtr->currTupAiLen= TreclenAiLqhkey;
    }
  }//if

  /* If we've received all KeyInfo, proceed with processing,
   * otherwise wait for discrete KeyInfo signals
   */
  if (regTcPtr->primKeyLen == keyLenWithLQHReq) {
    endgettupkeyLab(signal);
    return;
  } else {
    jam();
    ndbassert(!isLongReq);
    /* Wait for remaining KeyInfo */
    regTcPtr->save1 = keyLenWithLQHReq;
    regTcPtr->transactionState = TcConnectionrec::WAIT_TUPKEYINFO;
    return;
  }//if
  return;
}//Dblqh::execLQHKEYREQ()



/**
 * endgettupkeyLab
 * Invoked when all KeyInfo and/or all AttrInfo has been 
 * received
 */
void Dblqh::endgettupkeyLab(Signal* signal) 
{
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  if (regTcPtr->totReclenAi == regTcPtr->currReclenAi) {
    ;
  } else {
    jam();
    /* Wait for discrete AttrInfo signals */
    ndbrequire(regTcPtr->currReclenAi < regTcPtr->totReclenAi);
    ndbassert( !(regTcPtr->m_flags & 
                 TcConnectionrec::OP_ISLONGREQ) );
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
  Uint32 activeCreat = regTcPtr->activeCreat;
  if (regTcPtr->operation == ZUNLOCK)
  {
    jam();
    handleUserUnlockRequest(signal);
    return;
  }

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
      takeOverErrorLab(signal);
      return;
    }//if
    Uint32 accOpPtr= get_acc_ptr_from_scan_record(scanptr.p,
                                                  ttcScanOp,
                                                  true);
    if (accOpPtr == RNIL) {
      jam();
      takeOverErrorLab(signal);
      return;
    }//if
    signal->theData[1] = accOpPtr;
    signal->theData[2] = regTcPtr->transid[0];
    signal->theData[3] = regTcPtr->transid[1];
    EXECUTE_DIRECT(refToMain(regTcPtr->tcAccBlockref), GSN_ACC_TO_REQ, 
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
  if (TRACENR_FLAG)
  {
    TRACE_OP(regTcPtr, "RECEIVED");
    switch (regTcPtr->operation) {
    case ZREAD: TRACENR("READ"); break;
    case ZUPDATE: TRACENR("UPDATE"); break;
    case ZWRITE: TRACENR("WRITE"); break;
    case ZINSERT: TRACENR("INSERT"); break;
    case ZDELETE: TRACENR("DELETE"); break;
    case ZUNLOCK: TRACENR("UNLOCK"); break;
    case ZREFRESH: TRACENR("REFRESH"); break;
    default: TRACENR("<Unknown: " << regTcPtr->operation << ">"); break;
    }
    
    TRACENR(" tab: " << regTcPtr->tableref 
	   << " frag: " << regTcPtr->fragmentid
	   << " activeCreat: " << (Uint32)activeCreat);
    if (LqhKeyReq::getNrCopyFlag(regTcPtr->reqinfo))
      TRACENR(" NrCopy");
    if (LqhKeyReq::getRowidFlag(regTcPtr->reqinfo))
      TRACENR(" rowid: " << regTcPtr->m_row_id);
    TRACENR(" key: " << getKeyInfoWordOrZero(regTcPtr, 0));
  }
  
  if (likely(activeCreat == Fragrecord::AC_NORMAL))
  {
    if (TRACENR_FLAG)
      TRACENR(endl);
    ndbassert(!LqhKeyReq::getNrCopyFlag(regTcPtr->reqinfo));
    exec_acckeyreq(signal, tcConnectptr);
  } 
  else if (activeCreat == Fragrecord::AC_NR_COPY)
  {
    regTcPtr->totSendlenAi = regTcPtr->totReclenAi;
    handle_nr_copy(signal, tcConnectptr);
  }
  else
  {
    ndbassert(activeCreat == Fragrecord::AC_IGNORED);
    if (TRACENR_FLAG)
      TRACENR(" IGNORING (activeCreat == 2)" << endl);
    
    signal->theData[0] = tc_ptr_i;
    regTcPtr->transactionState = TcConnectionrec::WAIT_ACC_ABORT;
    
    signal->theData[0] = regTcPtr->tupConnectrec;
    EXECUTE_DIRECT(DBTUP, GSN_TUP_ABORTREQ, signal, 1);
    jamEntry();

    regTcPtr->totSendlenAi = regTcPtr->totReclenAi;
    packLqhkeyreqLab(signal);
  }
}

void
Dblqh::exec_acckeyreq(Signal* signal, TcConnectionrecPtr regTcPtr)
{
  Uint32 taccreq;
  regTcPtr.p->transactionState = TcConnectionrec::WAIT_ACC;
  taccreq = regTcPtr.p->operation;
  taccreq = taccreq + (regTcPtr.p->lockType << 4);
  taccreq = taccreq + (regTcPtr.p->dirtyOp << 6);
  taccreq = taccreq + (regTcPtr.p->replicaType << 7);
  taccreq = taccreq + (regTcPtr.p->apiVersionNo << 9);
/* ************ */
/*  ACCKEYREQ < */
/* ************ */
  Uint32 sig0, sig1, sig2, sig3, sig4;
  sig0 = regTcPtr.p->accConnectrec;
  sig1 = fragptr.p->accFragptr;
  sig2 = regTcPtr.p->hashValue;
  sig3 = regTcPtr.p->primKeyLen;
  sig4 = regTcPtr.p->transid[0];
  signal->theData[0] = sig0;
  signal->theData[1] = sig1;
  signal->theData[2] = taccreq;
  signal->theData[3] = sig2;
  signal->theData[4] = sig3;
  signal->theData[5] = sig4;

  sig0 = regTcPtr.p->transid[1];
  signal->theData[6] = sig0;

  /* Copy KeyInfo to end of ACCKEYREQ signal, starting at offset 7 */
  sendKeyinfoAcc(signal, 7);

  TRACE_OP(regTcPtr.p, "ACC");
  
  EXECUTE_DIRECT(refToMain(regTcPtr.p->tcAccBlockref), GSN_ACCKEYREQ, 
		 signal, 7 + regTcPtr.p->primKeyLen);
  if (signal->theData[0] < RNIL) {
    signal->theData[0] = regTcPtr.i;
    execACCKEYCONF(signal);
    return;
  } else if (signal->theData[0] == RNIL) {
    ;
  } else {
    ndbrequire(signal->theData[0] == (UintR)-1);
    signal->theData[0] = regTcPtr.i;
    execACCKEYREF(signal);
  }//if
  return;
}//Dblqh::prepareContinueAfterBlockedLab()

void
Dblqh::handle_nr_copy(Signal* signal, Ptr<TcConnectionrec> regTcPtr)
{
  jam();
  Uint32 fragPtr = fragptr.p->tupFragptr;
  Uint32 op = regTcPtr.p->operation;

  const bool copy = LqhKeyReq::getNrCopyFlag(regTcPtr.p->reqinfo);

  if (!LqhKeyReq::getRowidFlag(regTcPtr.p->reqinfo))
  {
    /**
     * Rowid not set, that mean that primary has finished copying...
     */
    jam();
    if (TRACENR_FLAG)
      TRACENR(" Waiting for COPY_ACTIVEREQ" << endl);
    ndbassert(!LqhKeyReq::getNrCopyFlag(regTcPtr.p->reqinfo));
    regTcPtr.p->activeCreat = Fragrecord::AC_NORMAL;
    exec_acckeyreq(signal, regTcPtr);
    return;
  }

  regTcPtr.p->m_nr_delete.m_cnt = 1; // Wait for real op aswell
  Uint32* dst = signal->theData+24;
  bool uncommitted;
  const int len = c_tup->nr_read_pk(fragPtr, &regTcPtr.p->m_row_id, dst, 
				    uncommitted);
  const bool match = (len>0) ? compare_key(regTcPtr.p, dst, len) == 0 : false;
  
  if (TRACENR_FLAG)
    TRACENR(" len: " << len << " match: " << match 
	   << " uncommitted: " << uncommitted);

  if (copy)
  {
    ndbassert(LqhKeyReq::getGCIFlag(regTcPtr.p->reqinfo));
    if (match)
    {
      /**
       * Case 1
       */
      jam();
      ndbassert(op == ZINSERT);
      if (TRACENR_FLAG)
	TRACENR(" Changing from INSERT to ZUPDATE" << endl);
      regTcPtr.p->operation = ZUPDATE;
      goto run;
    }
    else if (len > 0 && op == ZDELETE)
    {
      /**
       * Case 4
       *   Perform delete using rowid
       *     primKeyLen == 0
       *     key[0] == rowid
       */
      jam();
      ndbassert(regTcPtr.p->primKeyLen == 0);
      if (TRACENR_FLAG)
	TRACENR(" performing DELETE key: " 
	       << dst[0] << endl); 

      nr_copy_delete_row(signal, regTcPtr, &regTcPtr.p->m_row_id, len);
      ndbassert(regTcPtr.p->m_nr_delete.m_cnt);
      regTcPtr.p->m_nr_delete.m_cnt--; // No real op is run
      if (regTcPtr.p->m_nr_delete.m_cnt)
      {
	jam();
	return;
      }
      packLqhkeyreqLab(signal);
      return;
    }
    else if (len == 0 && op == ZDELETE)
    {
      /**
       * Case 7
       */
      jam();
      if (TRACENR_FLAG)
	TRACENR(" UPDATE_GCI" << endl); 
      c_tup->nr_update_gci(fragPtr, &regTcPtr.p->m_row_id, regTcPtr.p->gci_hi);
      goto update_gci_ignore;
    }
    
    /**
     * 1) Delete row at specified rowid (if len > 0)
     * 2) Delete specified row at different rowid (if exists)
     * 3) Run insert
     */
    if (len > 0)
    {
      /**
       * 1) Delete row at specified rowid (if len > 0)
       */
      jam();
      nr_copy_delete_row(signal, regTcPtr, &regTcPtr.p->m_row_id, len);
    }
    /**
     * 2) Delete specified row at different rowid (if exists)    
     */
    jam();
    nr_copy_delete_row(signal, regTcPtr, 0, 0);
    if (TRACENR_FLAG)
      TRACENR(" RUN INSERT" << endl); 
    goto run;
  }
  else
  {
    if (!match && op != ZINSERT)
    {
      jam();
      if (TRACENR_FLAG)
	TRACENR(" IGNORE " << endl); 
      goto ignore;
    }
    if (match)
    {
      jam();
      if (op != ZDELETE && op != ZREFRESH)
      {
	if (TRACENR_FLAG)
	  TRACENR(" Changing from INSERT/UPDATE to ZWRITE" << endl);
	regTcPtr.p->operation = ZWRITE;
      }
      goto run;
    }

    ndbassert(!match && op == ZINSERT);

    /**
     * 1) Delete row at specified rowid (if len > 0)
     * 2) Delete specified row at different rowid (if exists)
     * 3) Run insert
     */
    if (len > 0)
    {
      /**
       * 1) Delete row at specified rowid (if len > 0)
       */
      jam();
      nr_copy_delete_row(signal, regTcPtr, &regTcPtr.p->m_row_id, len);
    }

    /**
     * 2) Delete specified row at different rowid (if exists)    
     */
    jam();
    nr_copy_delete_row(signal, regTcPtr, 0, 0);
    if (TRACENR_FLAG)
      TRACENR(" RUN op: " << op << endl); 
    goto run;
  }
  
run:
  jam();
  exec_acckeyreq(signal, regTcPtr);
  return;
  
ignore:
  jam();
  ndbassert(!LqhKeyReq::getNrCopyFlag(regTcPtr.p->reqinfo));
update_gci_ignore:
  regTcPtr.p->activeCreat = Fragrecord::AC_IGNORED;
  signal->theData[0] = regTcPtr.p->tupConnectrec;
  EXECUTE_DIRECT(DBTUP, GSN_TUP_ABORTREQ, signal, 1);

  packLqhkeyreqLab(signal);
}

/**
 * Compare received key data with the data supplied
 * returning 0 if they are the same, 1 otherwise
 */
int
Dblqh::compare_key(const TcConnectionrec* regTcPtr, 
		   const Uint32 * ptr, Uint32 len)
{
  if (regTcPtr->primKeyLen != len)
    return 1;
  
  ndbassert( regTcPtr->keyInfoIVal != RNIL );

  SectionReader keyInfoReader(regTcPtr->keyInfoIVal,
                              getSectionSegmentPool());
  
  ndbassert(regTcPtr->primKeyLen == keyInfoReader.getSize());

  while (len != 0)
  {
    const Uint32* keyChunk= NULL;
    Uint32 chunkSize= 0;

    /* Get a ptr to a chunk of contiguous words to compare */
    bool ok= keyInfoReader.getWordsPtr(len, keyChunk, chunkSize);

    ndbrequire(ok);

    if ( memcmp(ptr, keyChunk, chunkSize << 2))
      return 1;
    
    ptr+= chunkSize;
    len-= chunkSize;
  }

  return 0;
}

void
Dblqh::nr_copy_delete_row(Signal* signal, 
			  Ptr<TcConnectionrec> regTcPtr,
			  Local_key* rowid, Uint32 len)
{
  Ptr<Fragrecord> fragPtr = fragptr;

  Uint32 keylen;
  Uint32 tableId = regTcPtr.p->tableref;
  Uint32 accPtr = regTcPtr.p->accConnectrec;
  
  signal->theData[0] = accPtr;
  signal->theData[1] = fragptr.p->accFragptr;
  signal->theData[2] = ZDELETE + (ZDELETE << 4);
  signal->theData[5] = regTcPtr.p->transid[0];
  signal->theData[6] = regTcPtr.p->transid[1];
  
  if (rowid)
  {
    jam();
    keylen = 2;
    if (g_key_descriptor_pool.getPtr(tableId)->hasCharAttr)
    {
      signal->theData[3] = calculateHash(tableId, signal->theData+24);
    }
    else
    {
      signal->theData[3] = md5_hash((Uint64*)(signal->theData+24), len);
    }
    signal->theData[4] = 0; // seach by local key
    signal->theData[7] = rowid->m_page_no;
    signal->theData[8] = rowid->m_page_idx;
  }
  else
  {
    jam();
    keylen = regTcPtr.p->primKeyLen;
    signal->theData[3] = regTcPtr.p->hashValue;
    signal->theData[4] = keylen;

    /* Copy KeyInfo inline into the ACCKEYREQ signal, 
     * starting at word 7 
     */
    sendKeyinfoAcc(signal, 7);
  }
  const Uint32 ref = refToMain(regTcPtr.p->tcAccBlockref);
  EXECUTE_DIRECT(ref, GSN_ACCKEYREQ, signal, 7 + keylen);
  jamEntry();

  Uint32 retValue = signal->theData[0];
  ndbrequire(retValue != RNIL); // This should never block...
  ndbrequire(retValue != (Uint32)-1 || rowid == 0); // rowid should never fail

  if (retValue == (Uint32)-1)
  {
    /**
     * Only delete by pk, may fail
     */
    jam();
    ndbrequire(rowid == 0);
    signal->theData[0] = accPtr;
    signal->theData[1] = 0;
    EXECUTE_DIRECT(ref, GSN_ACC_ABORTREQ, signal, 2);
    jamEntry();
    return;
  }

  /**
   * We found row (and have it locked in ACC)
   */
  ndbrequire(regTcPtr.p->m_dealloc == 0);
  Local_key save = regTcPtr.p->m_row_id;

  c_acc->execACCKEY_ORD(signal, accPtr);
  signal->theData[0] = accPtr;
  EXECUTE_DIRECT(ref, GSN_ACC_COMMITREQ, signal, 1);
  jamEntry();
  
  ndbrequire(regTcPtr.p->m_dealloc == 1);  
  int ret = c_tup->nr_delete(signal, regTcPtr.i, 
			     fragPtr.p->tupFragptr, &regTcPtr.p->m_row_id, 
			     regTcPtr.p->gci_hi);
  jamEntry();
  
  if (ret)
  {
    ndbassert(ret == 1);
    Uint32 pos = regTcPtr.p->m_nr_delete.m_cnt - 1;
    memcpy(regTcPtr.p->m_nr_delete.m_disk_ref + pos, 
	   signal->theData, sizeof(Local_key));
    regTcPtr.p->m_nr_delete.m_page_id[pos] = RNIL;
    regTcPtr.p->m_nr_delete.m_cnt = pos + 2;
    if (0) ndbout << "PENDING DISK DELETE: " << 
      regTcPtr.p->m_nr_delete.m_disk_ref[pos] << endl;
  }
  
  TRACENR("DELETED: " << regTcPtr.p->m_row_id << endl);
  
  regTcPtr.p->m_dealloc = 0;
  regTcPtr.p->m_row_id = save;
  fragptr = fragPtr;
  tcConnectptr = regTcPtr;
}

void
Dblqh::get_nr_op_info(Nr_op_info* op, Uint32 page_id)
{
  Ptr<TcConnectionrec> tcPtr;
  tcPtr.i = op->m_ptr_i;
  ptrCheckGuard(tcPtr, ctcConnectrecFileSize, tcConnectionrec);
  
  Ptr<Fragrecord> fragPtr;
  c_fragment_pool.getPtr(fragPtr, tcPtr.p->fragmentptr);  

  op->m_gci_hi = tcPtr.p->gci_hi;
  op->m_gci_lo = tcPtr.p->gci_lo;
  op->m_tup_frag_ptr_i = fragPtr.p->tupFragptr;

  ndbrequire(tcPtr.p->activeCreat == Fragrecord::AC_NR_COPY);
  ndbrequire(tcPtr.p->m_nr_delete.m_cnt);
  
  
  if (page_id == RNIL)
  {
    // get log buffer callback
    for (Uint32 i = 0; i<2; i++)
    {
      if (tcPtr.p->m_nr_delete.m_page_id[i] != RNIL)
      {
	op->m_page_id = tcPtr.p->m_nr_delete.m_page_id[i];
	op->m_disk_ref = tcPtr.p->m_nr_delete.m_disk_ref[i];
	return;
      }
    }
  }
  else
  {
    // get page callback
    for (Uint32 i = 0; i<2; i++)
    {
      Local_key key = tcPtr.p->m_nr_delete.m_disk_ref[i];
      if (op->m_disk_ref.m_page_no == key.m_page_no &&
	  op->m_disk_ref.m_file_no == key.m_file_no &&
	  tcPtr.p->m_nr_delete.m_page_id[i] == RNIL)
      {
	op->m_disk_ref = key;
	tcPtr.p->m_nr_delete.m_page_id[i] = page_id;
	return;
      }
    }
  }
  ndbrequire(false);
}

void 
Dblqh::nr_delete_complete(Signal* signal, Nr_op_info* op)
{
  jamEntry();
  Ptr<TcConnectionrec> tcPtr;
  tcPtr.i = op->m_ptr_i;
  ptrCheckGuard(tcPtr, ctcConnectrecFileSize, tcConnectionrec);

  ndbrequire(tcPtr.p->activeCreat == Fragrecord::AC_NR_COPY);
  ndbrequire(tcPtr.p->m_nr_delete.m_cnt);
  
  tcPtr.p->m_nr_delete.m_cnt--;
  if (tcPtr.p->m_nr_delete.m_cnt == 0)
  {
    jam();
    tcConnectptr = tcPtr;
    c_fragment_pool.getPtr(fragptr, tcPtr.p->fragmentptr);
    
    if (tcPtr.p->abortState != TcConnectionrec::ABORT_IDLE) 
    {
      jam();
      tcPtr.p->activeCreat = Fragrecord::AC_NORMAL;
      abortCommonLab(signal);
    }
    else if (tcPtr.p->operation == ZDELETE && 
	     LqhKeyReq::getNrCopyFlag(tcPtr.p->reqinfo))
    {
      /**
       * This is run directly in handle_nr_copy
       */
      jam();
      packLqhkeyreqLab(signal);
    }
    else
    {
      jam();
      rwConcludedLab(signal);
    }
    return;
  }

  if (memcmp(&tcPtr.p->m_nr_delete.m_disk_ref[0], 
	     &op->m_disk_ref, sizeof(Local_key)) == 0)
  {
    jam();
    ndbassert(tcPtr.p->m_nr_delete.m_page_id[0] != RNIL);
    tcPtr.p->m_nr_delete.m_page_id[0] = tcPtr.p->m_nr_delete.m_page_id[1];
    tcPtr.p->m_nr_delete.m_disk_ref[0] = tcPtr.p->m_nr_delete.m_disk_ref[1];
  }
}

Uint32
Dblqh::readPrimaryKeys(Uint32 opPtrI, Uint32 * dst, bool xfrm)
{
  TcConnectionrecPtr regTcPtr;  
  Uint64 Tmp[MAX_KEY_SIZE_IN_WORDS >> 1];

  jamEntry();
  regTcPtr.i = opPtrI;
  ptrCheckGuard(regTcPtr, ctcConnectrecFileSize, tcConnectionrec);

  Uint32 tableId = regTcPtr.p->tableref;
  Uint32 keyLen = regTcPtr.p->primKeyLen;
  Uint32 * tmp = xfrm ? (Uint32*)Tmp : dst;

  copy(tmp, regTcPtr.p->keyInfoIVal);
  
  if (xfrm)
  {
    jam();
    Uint32 keyPartLen[MAX_ATTRIBUTES_IN_INDEX];
    return xfrm_key(tableId, (Uint32*)Tmp, dst, ~0, keyPartLen);
  }
  
  return keyLen;
}

/**
 * getKeyInfoWordOrZero
 * Get given word of KeyInfo, or zero if it's not available
 * Used for tracing
 */
Uint32
Dblqh::getKeyInfoWordOrZero(const TcConnectionrec* regTcPtr,
                            Uint32 offset)
{
  if (regTcPtr->keyInfoIVal != RNIL)
  {
    SectionReader keyInfoReader(regTcPtr->keyInfoIVal,
                                g_sectionSegmentPool);
    
    if (keyInfoReader.getSize() > offset)
    {
      if (offset)
        keyInfoReader.step(offset);
      
      Uint32 word;
      keyInfoReader.getWord(&word);
      return word;
    }
  }
  return 0;
}

void Dblqh::unlockError(Signal* signal, Uint32 error)
{
  terrorCode = error;
  abortErrorLab(signal);
}

/**
 * handleUserUnlockRequest
 *
 * This method handles an LQHKEYREQ unlock request from 
 * TC.
 */
void Dblqh::handleUserUnlockRequest(Signal* signal)
{
  jam();
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  Uint32 tcPtrI = tcConnectptr.i;

  /* Request to unlock (abort) an existing read operation
   *
   * 1) Get user's LOCK_REF from KeyInfo
   *    
   * 2) Lookup TC_OP_REF in hash
   *    
   * 3) Check state of found op : TransId, state, type, lock
   *    
   * 4) Check op_id portion
   * 
   * 5) Abort locking op in ACC
   * 
   * 6) Clean up locking op in LQH
   *    
   * 7) Send LQHKEYCONF to TC for user unlock op
   * 
   * 8) Clean up user unlock op
   */
  if (unlikely( regTcPtr->primKeyLen != LqhKeyReq::UnlockKeyLen ))
  {
    jam();
    unlockError(signal, 4109); /* Faulty primary key attribute length */
    return;
  }
    
  SectionReader keyInfoReader(regTcPtr->keyInfoIVal,
                              getSectionSegmentPool());

  ndbrequire( keyInfoReader.getSize() == regTcPtr->primKeyLen );
  
  /* Extract components of user lock reference */
  Uint32 tcOpRecIndex;
  Uint32 lqhOpIdWord;
  ndbrequire( keyInfoReader.getWord( &tcOpRecIndex ) ); // Locking op TC index
  ndbrequire( keyInfoReader.getWord( &lqhOpIdWord ) );  // Part of Locking op LQH id
  
  /* Use TC operation record index to find the operation record
   * This requires that this operation and the referenced 
   * operation are part of the same transaction.
   * On success this sets tcConnectptr.i and .p to the 
   * operation-to-unlock's record.
   */
  if (unlikely( findTransaction(regTcPtr->transid[0], 
                                regTcPtr->transid[1], 
                                tcOpRecIndex,
                                0) != ZOK))
  {
    jam();
    unlockError(signal, ZBAD_OP_REF);
    return;
  }
  
  TcConnectionrec * const regLockTcPtr = tcConnectptr.p;
  
  /* Validate that the bottom 32-bits of the operation id reference
   * we were given are in alignment
   */
  Uint32 lockOpKeyReqId = (Uint32) regLockTcPtr->lqhKeyReqId;
  if (unlikely( lockOpKeyReqId != lqhOpIdWord ))
  {
    jam();
    unlockError(signal, ZBAD_OP_REF);
    return;
  }

  /* Validate the state of the locking operation */
  bool lockingOpValid = 
    (( regLockTcPtr->operation == ZREAD ) && 
       // ZREAD_EX mapped to ZREAD above
     ( ! regLockTcPtr->dirtyOp ) &&
     ( ! regLockTcPtr->opSimple ) &&
     ( (regLockTcPtr->lockType == ZREAD) ||  // LM_Read
       (regLockTcPtr->lockType == ZUPDATE) ) // LM_Exclusive 
     &&
     ( regLockTcPtr->transactionState == TcConnectionrec::PREPARED ) &&
     ( regLockTcPtr->commitAckMarker == RNIL ) && 
       // No commit ack marker 
     ( regLockTcPtr->logWriteState == 
       TcConnectionrec::NOT_STARTED )); // No log written
  
  if (unlikely(! lockingOpValid))
  {
    jam();
    unlockError(signal, ZBAD_UNLOCK_STATE);
    return;
  }
  
  /* Ok, now we're ready to start 'aborting' this operation, to get the 
   * effect of unlocking it
   */
  signal->theData[0] = regLockTcPtr->accConnectrec;
  signal->theData[1] = 0; // For Execute_Direct
  EXECUTE_DIRECT(refToMain(regLockTcPtr->tcAccBlockref),
                 GSN_ACC_ABORTREQ,
                 signal,
                 2);
  jamEntry();
  
  /* Would be nice to handle non-success case somehow */
  ndbrequire(signal->theData[1] == 0); 
  
  /* Now we want to release LQH resources associated with the
   * locking operation
   */
  cleanUp(signal);
  
  /* Now that the locking operation has been 'disappeared', we need to 
   * send an LQHKEYCONF for the unlock operation and then 'disappear' it 
   * as well
   */
  tcConnectptr.i = tcPtrI;
  ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
  
  ndbrequire( regTcPtr == tcConnectptr.p );
  
  /* Set readlenAi to the unlocked operation's TC operation ref */
  regTcPtr->readlenAi = tcOpRecIndex;

  /* Clear number of fired triggers */
  regTcPtr->noFiredTriggers = 0;
  
  /* Now send the LQHKEYCONF to TC */
  sendLqhkeyconfTc(signal, regTcPtr->tcBlockref);
  
  /* Finally, clean up the unlock operation itself */
  cleanUp(signal);

  return;
}

/* =*======================================================================= */
/* =======                 SEND KEYINFO TO ACC                       ======= */
/*                                                                           */
/* ========================================================================= */
void Dblqh::sendKeyinfoAcc(Signal* signal, Uint32 Ti) 
{
  /* Copy all KeyInfo into the signal at offset Ti */
  copy(&signal->theData[Ti],
       tcConnectptr.p->keyInfoIVal);
}//Dblqh::sendKeyinfoAcc()

void Dblqh::execLQH_ALLOCREQ(Signal* signal)
{
  TcConnectionrecPtr regTcPtr;  
  FragrecordPtr regFragptr;

  jamEntry();
  regTcPtr.i = signal->theData[0];
  ptrCheckGuard(regTcPtr, ctcConnectrecFileSize, tcConnectionrec);

  regFragptr.i = regTcPtr.p->fragmentptr;
  c_fragment_pool.getPtr(regFragptr);

  signal->theData[0] = regTcPtr.p->tupConnectrec;
  signal->theData[1] = regFragptr.p->tupFragptr;
  signal->theData[2] = regTcPtr.p->tableref;
  Uint32 tup = refToMain(regTcPtr.p->tcTupBlockref);
  EXECUTE_DIRECT(tup, GSN_TUP_ALLOCREQ, signal, 3);
}//Dblqh::execTUP_ALLOCREQ()

void Dblqh::execTUP_DEALLOCREQ(Signal* signal)
{
  TcConnectionrecPtr regTcPtr;  
  
  jamEntry();
  regTcPtr.i = signal->theData[4];
  
  if (TRACENR_FLAG)
  {
    Local_key tmp;
    tmp.m_page_no = signal->theData[2];
    tmp.m_page_idx = signal->theData[3];
    TRACENR("TUP_DEALLOC: " << tmp << 
      (signal->theData[5] ? " DIRECT " : " DELAYED") << endl);
  }
  
  if (signal->theData[5])
  {
    jam();
    Local_key tmp;

    tmp.m_page_no = signal->theData[2];
    tmp.m_page_idx = signal->theData[3];

    if (ERROR_INSERTED(5712))
    {
      ndbout << "TUP_DEALLOC: " << tmp << endl;
    }

    EXECUTE_DIRECT(DBTUP, GSN_TUP_DEALLOCREQ, signal, signal->getLength());
    return;
  }
  else
  {
    jam();
    ptrCheckGuard(regTcPtr, ctcConnectrecFileSize, tcConnectionrec);
    regTcPtr.p->m_row_id.m_page_no = signal->theData[2];
    regTcPtr.p->m_row_id.m_page_idx = signal->theData[3];

    TRACE_OP(regTcPtr.p, "SET DEALLOC");
    
    ndbrequire(regTcPtr.p->m_dealloc == 0);
    regTcPtr.p->m_dealloc = 1;
  }
}//Dblqh::execTUP_ALLOCREQ()

/* ************>> */
/*  ACCKEYCONF  > */
/* ************>> */
void Dblqh::execACCKEYCONF(Signal* signal) 
{
  TcConnectionrec *regTcConnectionrec = tcConnectionrec;
  Uint32 ttcConnectrecFileSize = ctcConnectrecFileSize;
  Uint32 tcIndex = signal->theData[0];
  Uint32 localKey1 = signal->theData[3];
  Uint32 localKey2 = signal->theData[4];
  jamEntry();

  tcConnectptr.i = tcIndex;
  ptrCheckGuard(tcConnectptr, ttcConnectrecFileSize, regTcConnectionrec);
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  if (regTcPtr->transactionState != TcConnectionrec::WAIT_ACC) {
    LQHKEY_abort(signal, 3);
    return;
  }//if

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
    ndbassert(regTcPtr->seqNoReplica == 0 || 
	      regTcPtr->activeCreat == Fragrecord::AC_NR_COPY);
    Uint32 op= signal->theData[1];
    Uint32 requestInfo = regTcPtr->reqinfo;
    if(likely(op == ZINSERT || op == ZUPDATE))
    {
      jam();
      regTcPtr->operation = op;
    }
    else
    {
      jam();
      warningEvent("Convering %d to ZUPDATE", op);
      op = regTcPtr->operation = ZUPDATE;
    }
    if (regTcPtr->seqNoReplica == 0)
    {
      jam();
      requestInfo &= ~(RI_OPERATION_MASK <<  RI_OPERATION_SHIFT);
      LqhKeyReq::setOperation(requestInfo, op);
      regTcPtr->reqinfo = requestInfo;
    }
  }//if
  
  /* ------------------------------------------------------------------------
   * IT IS NOW TIME TO CONTACT THE TUPLE MANAGER. THE TUPLE MANAGER NEEDS THE
   * INFORMATION ON WHICH TABLE AND FRAGMENT, THE LOCAL KEY AND IT NEEDS TO
   * KNOW THE TYPE OF OPERATION TO PERFORM. TUP CAN SEND THE ATTRINFO DATA 
   * EITHER TO THE TC BLOCK OR DIRECTLY TO THE APPLICATION. THE SCHEMA VERSION
   * IS NEEDED SINCE TWO SCHEMA VERSIONS CAN BE ACTIVE SIMULTANEOUSLY ON A 
   * TABLE.
   * ----------------------------------------------------------------------- */
  FragrecordPtr regFragptr;
  regFragptr.i = regTcPtr->fragmentptr;
  c_fragment_pool.getPtr(regFragptr);

  if(!regTcPtr->m_disk_table)
    acckeyconf_tupkeyreq(signal, regTcPtr, regFragptr.p,
                         localKey1, localKey2, RNIL);
  else
    acckeyconf_load_diskpage(signal, tcConnectptr, regFragptr.p,
                             localKey1, localKey2);
}

void
Dblqh::acckeyconf_tupkeyreq(Signal* signal, TcConnectionrec* regTcPtr,
			    Fragrecord* regFragptrP,
			    Uint32 lkey1, Uint32 lkey2,
			    Uint32 disk_page)
{
  Uint32 op = regTcPtr->operation;
  regTcPtr->transactionState = TcConnectionrec::WAIT_TUP;
  /* ------------------------------------------------------------------------
   * IT IS NOW TIME TO CONTACT THE TUPLE MANAGER. THE TUPLE MANAGER NEEDS THE
   * INFORMATION ON WHICH TABLE AND FRAGMENT, THE LOCAL KEY AND IT NEEDS TO
   * KNOW THE TYPE OF OPERATION TO PERFORM. TUP CAN SEND THE ATTRINFO DATA 
   * EITHER TO THE TC BLOCK OR DIRECTLY TO THE APPLICATION. THE SCHEMA VERSION
   * IS NEEDED SINCE TWO SCHEMA VERSIONS CAN BE ACTIVE SIMULTANEOUSLY ON A 
   * TABLE.
   * ----------------------------------------------------------------------- */
  Uint32 page_idx = lkey2;
  Uint32 page_no = lkey1;
  Uint32 Ttupreq = regTcPtr->dirtyOp;
  Uint32 flags = regTcPtr->m_flags;
  Ttupreq = Ttupreq + (regTcPtr->opSimple << 1);
  Ttupreq = Ttupreq + (op << 6);
  Ttupreq = Ttupreq + (regTcPtr->opExec << 10);
  Ttupreq = Ttupreq + (regTcPtr->apiVersionNo << 11);
  Ttupreq = Ttupreq + (regTcPtr->m_use_rowid << 11);
  Ttupreq = Ttupreq + (regTcPtr->m_reorg << 12);

  /* --------------------------------------------------------------------- 
   * Clear interpreted mode bit since we do not want the next replica to
   * use interpreted mode. The next replica will receive a normal write.
   * --------------------------------------------------------------------- */
  regTcPtr->opExec = 0;
  /* ************< */
  /*  TUPKEYREQ  < */
  /* ************< */
  Uint32 sig0, sig1, sig2, sig3;
  sig0 = regTcPtr->tupConnectrec;

  TupKeyReq * const tupKeyReq = (TupKeyReq *)signal->getDataPtrSend();
  tupKeyReq->connectPtr = sig0;
  tupKeyReq->request = Ttupreq;
  tupKeyReq->keyRef1 = page_no;
  tupKeyReq->keyRef2 = page_idx;

  sig0 = regTcPtr->totReclenAi;
  sig1 = regTcPtr->applOprec;
  sig2 = regTcPtr->applRef;
  
  tupKeyReq->attrBufLen = sig0;
  tupKeyReq->opRef = sig1;
  tupKeyReq->applRef = sig2;

  sig0 = regTcPtr->storedProcId;
  sig1 = regTcPtr->transid[0];
  sig2 = regTcPtr->transid[1];
  sig3 = regFragptrP->tupFragptr;
  Uint32 tup = refToMain(regTcPtr->tcTupBlockref);

  tupKeyReq->storedProcedure = sig0;
  tupKeyReq->transId1 = sig1;
  tupKeyReq->transId2 = sig2;
  tupKeyReq->fragPtr = sig3;

  sig0 = regTcPtr->m_row_id.m_page_no;
  sig1 = regTcPtr->m_row_id.m_page_idx;
  
  tupKeyReq->primaryReplica = (tcConnectptr.p->seqNoReplica == 0)?true:false;
  tupKeyReq->coordinatorTC = tcConnectptr.p->tcBlockref;
  tupKeyReq->tcOpIndex = tcConnectptr.p->tcOprec;
  tupKeyReq->savePointId = tcConnectptr.p->savePointId;
  tupKeyReq->disk_page= disk_page;

  tupKeyReq->m_row_id_page_no = sig0;
  tupKeyReq->m_row_id_page_idx = sig1;
  
  TRACE_OP(regTcPtr, "TUPKEYREQ");
  
  regTcPtr->m_use_rowid |= (op == ZINSERT || op == ZREFRESH);
  regTcPtr->m_row_id.m_page_no = page_no;
  regTcPtr->m_row_id.m_page_idx = page_idx;
  
  tupKeyReq->attrInfoIVal= RNIL;
  tupKeyReq->deferred_constraints =
    (flags & TcConnectionrec::OP_DEFERRED_CONSTRAINTS) != 0;

  /* Pass AttrInfo section if available in the TupKeyReq signal
   * We are still responsible for releasing it, TUP is just
   * borrowing it
   */
  if (tupKeyReq->attrBufLen > 0)
  {
    ndbassert( regTcPtr->attrInfoIVal != RNIL );
    tupKeyReq->attrInfoIVal= regTcPtr->attrInfoIVal;
  }

  EXECUTE_DIRECT(tup, GSN_TUPKEYREQ, signal, TupKeyReq::SignalLength);
}//Dblqh::execACCKEYCONF()

void
Dblqh::acckeyconf_load_diskpage(Signal* signal, TcConnectionrecPtr regTcPtr,
				Fragrecord* regFragptrP,
                                Uint32 lkey1, Uint32 lkey2)
{
  int res;
  if((res= c_tup->load_diskpage(signal, 
				regTcPtr.p->tupConnectrec,
				regFragptrP->tupFragptr,
				lkey1, lkey2,
				regTcPtr.p->operation)) > 0)
  {
    acckeyconf_tupkeyreq(signal, regTcPtr.p, regFragptrP, lkey1, lkey2, res);
  }
  else if(res == 0)
  {
    regTcPtr.p->transactionState = TcConnectionrec::WAIT_TUP;
    regTcPtr.p->m_row_id.m_page_no = lkey1;
    regTcPtr.p->m_row_id.m_page_idx = lkey2;
  }
  else 
  {
    regTcPtr.p->transactionState = TcConnectionrec::WAIT_TUP;
    TupKeyRef * ref = (TupKeyRef *)signal->getDataPtr();
    ref->userRef= regTcPtr.i;
    ref->errorCode= ~0;
    execTUPKEYREF(signal);
  }
}

void
Dblqh::acckeyconf_load_diskpage_callback(Signal* signal, 
					 Uint32 callbackData,
					 Uint32 disk_page)
{
  jamEntry();
  tcConnectptr.i = callbackData;
  ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
  TcConnectionrec * const regTcPtr = tcConnectptr.p;

  TcConnectionrec::TransactionState state = regTcPtr->transactionState;
  if (likely(disk_page > 0 && state == TcConnectionrec::WAIT_TUP))
  {
    FragrecordPtr fragPtr;
    c_fragment_pool.getPtr(fragPtr, regTcPtr->fragmentptr);
    
    acckeyconf_tupkeyreq(signal, regTcPtr, fragPtr.p,
			 regTcPtr->m_row_id.m_page_no,
			 regTcPtr->m_row_id.m_page_idx,
			 disk_page);
  }
  else if (state != TcConnectionrec::WAIT_TUP)
  {
    ndbrequire(state == TcConnectionrec::WAIT_TUP_TO_ABORT);
    abortCommonLab(signal);
    return;
  }
  else
  {
    regTcPtr->transactionState = TcConnectionrec::WAIT_TUP;
    TupKeyRef * ref = (TupKeyRef *)signal->getDataPtr();
    ref->userRef= callbackData;
    ref->errorCode= disk_page;
    execTUPKEYREF(signal);
  }
}

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
/* ---- GET OPERATION TYPE AND CHECK WHAT KIND OF OPERATION IS REQUESTED --- */
  const TupKeyConf * const tupKeyConf = (TupKeyConf *)&signal->theData[0];
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  Uint32 activeCreat = regTcPtr->activeCreat;
  Uint32 readLen = tupKeyConf->readLength;
  Uint32 writeLen = tupKeyConf->writeLength;
  
  TRACE_OP(regTcPtr, "TUPKEYCONF");

  Uint32 accOp = regTcPtr->accConnectrec;
  c_acc->execACCKEY_ORD(signal, accOp);

  if (readLen != 0) 
  {
    jam();

    /* SET BIT 15 IN REQINFO */
    LqhKeyReq::setApplicationAddressFlag(regTcPtr->reqinfo, 1);
    regTcPtr->readlenAi = readLen;
  }//if

  if (regTcPtr->operation == ZREAD && 
      (regTcPtr->opSimple || regTcPtr->dirtyOp))
  {
    jam();
    /* ----------------------------------------------------------------------
     * THE OPERATION IS A SIMPLE READ. 
     * WE WILL IMMEDIATELY COMMIT THE OPERATION.
     * SINCE WE HAVE NOT RELEASED THE FRAGMENT LOCK 
     * (FOR LOCAL CHECKPOINTS) YET 
     * WE CAN GO IMMEDIATELY TO COMMIT_CONTINUE_AFTER_BLOCKED.
     * WE HAVE ALREADY SENT THE RESPONSE SO WE ARE NOT INTERESTED IN 
     * READ LENGTH
     * --------------------------------------------------------------------- */
    commitContinueAfterBlockedLab(signal);
    return;
  }//if

  regTcPtr->totSendlenAi = writeLen;
  /* We will propagate / log writeLen words
   * Check that that is how many we have available to 
   * propagate
   */
  ndbrequire(regTcPtr->totSendlenAi == regTcPtr->currTupAiLen);
  
  if (unlikely(activeCreat == Fragrecord::AC_NR_COPY))
  {
    jam();
    ndbrequire(regTcPtr->m_nr_delete.m_cnt);
    regTcPtr->m_nr_delete.m_cnt--;
    if (regTcPtr->m_nr_delete.m_cnt)
    {
      jam();
      /**
       * Let operation wait for pending NR operations
       *   even for before writing log...(as it's simpler)
       */
      
#ifdef VM_TRACE
      /**
       * Only disk table can have pending ops...
       */
      TablerecPtr tablePtr;
      tablePtr.i = regTcPtr->tableref;
      ptrCheckGuard(tablePtr, ctabrecFileSize, tablerec);
      ndbrequire(tablePtr.p->m_disk_table);
#endif
      
      return;
    }
  }

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
    packLqhkeyreqLab(signal);
    return;
  } else {
    FragrecordPtr regFragptr = fragptr;
    if (regFragptr.p->logFlag == Fragrecord::STATE_FALSE){
      if (regTcPtr->dirtyOp == ZTRUE) {
        jam();
	/* ------------------------------------------------------------------
	 * THIS OPERATION WAS A WRITE OPERATION THAT DO NOT NEED LOGGING AND 
	 * THAT CAN CAN  BE COMMITTED IMMEDIATELY.                     
	 * ----------------------------------------------------------------- */
        commitContinueAfterBlockedLab(signal);
        return;
      } else {
        jam();
	/* ------------------------------------------------------------------
	 * A NORMAL WRITE OPERATION ON A FRAGMENT WHICH DO NOT NEED LOGGING.
	 * WE WILL PACK THE REQUEST/RESPONSE TO THE NEXT NODE/TO TC.   
	 * ------------------------------------------------------------------ */
        regTcPtr->logWriteState = TcConnectionrec::NOT_WRITTEN;
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
      localCommitLab(signal);
      return;
    } else {
      jam();
      /* --------------------------------------------------------------------
       * A NORMAL READ OPERATION IS NOT LOGGED BUT IS NOT COMMITTED UNTIL 
       * THE COMMIT SIGNAL ARRIVES. THUS WE CONTINUE PACKING THE RESPONSE.
       * -------------------------------------------------------------------- */
      c_fragment_pool.getPtr(fragptr);
      packLqhkeyreqLab(signal);
      return;
    }//if
  } else {
    jam();
    c_fragment_pool.getPtr(fragptr);
    if (fragptr.p->logFlag == Fragrecord::STATE_FALSE) {
      if (regTcPtr->dirtyOp == ZTRUE) {
	/* ------------------------------------------------------------------
	 * THIS OPERATION WAS A WRITE OPERATION THAT DO NOT NEED LOGGING AND 
	 * THAT CAN CAN  BE COMMITTED IMMEDIATELY. 
	 * ----------------------------------------------------------------- */
        jam();
	/* ----------------------------------------------------------------
	 * IT MUST BE ACTIVE CREATION OF A FRAGMENT.
	 * ---------------------------------------------------------------- */
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

  const bool out_of_log_buffer = cnoOfLogPages < ZMIN_LOG_PAGES_OPERATION;

  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  logPartPtr.i = regTcPtr->m_log_part_ptr_i;
  ptrCheckGuard(logPartPtr, clogPartFileSize, logPartRecord);
  bool abort_on_redo_problems =
    (LqhKeyReq::getQueueOnRedoProblemFlag(regTcPtr->reqinfo) == 0);

/* -------------------------------------------------- */
/*       THIS PART IS USED TO WRITE THE LOG           */
/* -------------------------------------------------- */
/* -------------------------------------------------- */
/*       CHECK IF A LOG OPERATION IS ONGOING ALREADY. */
/*       IF SO THEN QUEUE THE OPERATION FOR LATER     */
/*       RESTART WHEN THE LOG PART IS FREE AGAIN.     */
/* -------------------------------------------------- */
  LogPartRecord * const regLogPartPtr = logPartPtr.p;
  const bool problem = out_of_log_buffer || regLogPartPtr->m_log_problems != 0;
  if (unlikely(problem))
  {
    if (abort_on_redo_problems)
    {
      logLqhkeyreqLab_problems(signal);
      return;
    }
    else
    {
      goto queueop;
    }
  }
  
  if (regLogPartPtr->logPartState == LogPartRecord::IDLE)
  {
    ;
  }
  else if (regLogPartPtr->logPartState == LogPartRecord::ACTIVE)
  {
queueop:
    jam();
    linkWaitLog(signal, logPartPtr, logPartPtr.p->m_log_prepare_queue);
    regTcPtr->transactionState = TcConnectionrec::LOG_QUEUED;
    return;
  }
  else
  {
    ndbrequire(false);
    return;
  }//if

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
    localCommitLab(signal);
  }//if
}//Dblqh::logLqhkeyreqLab()

void
Dblqh::logLqhkeyreqLab_problems(Signal * signal)
{
  jam();
  LogPartRecord * const regLogPartPtr = logPartPtr.p;
  Uint32 problems = regLogPartPtr->m_log_problems;

  if (cnoOfLogPages < ZMIN_LOG_PAGES_OPERATION)
  {
    jam();
    terrorCode = ZTEMPORARY_REDO_LOG_FAILURE;
  }
  else if ((problems & LogPartRecord::P_TAIL_PROBLEM) != 0)
  {
    jam();
    terrorCode = ZTAIL_PROBLEM_IN_LOG_ERROR;
  }
  else if ((problems & LogPartRecord::P_REDO_IO_PROBLEM) != 0)
  {
    jam();
    terrorCode = ZREDO_IO_PROBLEM;
  }
  else if ((problems & LogPartRecord::P_FILE_CHANGE_PROBLEM) != 0)
  {
    jam();
    terrorCode = ZFILE_CHANGE_PROBLEM_IN_LOG_ERROR;
  }
  abortErrorLab(signal);
}

void
Dblqh::update_log_problem(Signal* signal, Ptr<LogPartRecord> partPtr,
                          Uint32 problem, bool value)
{
  Uint32 problems = partPtr.p->m_log_problems;
  if (value)
  {
    /**
     * set
     */
    jam();
    if ((problems & problem) == 0)
    {
      jam();
      problems |= problem;
    }
  }
  else
  {
    /**
     * clear
     */
    jam();
    if ((problems & problem) != 0)
    {
      jam();
      problems &= ~(Uint32)problem;

      if (partPtr.p->LogLqhKeyReqSent == ZFALSE &&
          (!partPtr.p->m_log_prepare_queue.isEmpty() ||
           !partPtr.p->m_log_complete_queue.isEmpty()))
      {
        jam();

        partPtr.p->LogLqhKeyReqSent = ZTRUE;
        signal->theData[0] = ZLOG_LQHKEYREQ;
        signal->theData[1] = partPtr.i;
        sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
      }
    }
  }
  partPtr.p->m_log_problems = problems;
}

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
    if (! (regTcPtr->dirtyOp || 
           (regTcPtr->operation == ZREAD && regTcPtr->opSimple)))
    {
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

  Uint32 nextNodeId = regTcPtr->nextReplica;
  Uint32 nextVersion = getNodeInfo(nextNodeId).m_version;

  /* Send long LqhKeyReq to next replica if it can support it */
  bool sendLongReq= ! ((nextVersion < NDBD_LONG_LQHKEYREQ) || 
                       ERROR_INSERTED(5051));
  
  UintR TAiLen = sendLongReq ?
    0 :
    MIN(regTcPtr->totSendlenAi, LqhKeyReq::MaxAttrInfo);

  /* Long LQHKeyReq uses section size for key length */
  Uint32 lqhKeyLen= sendLongReq?
    0 :
    regTcPtr->primKeyLen;

  UintR TapplAddressIndicator = (regTcPtr->nextSeqNoReplica == 0 ? 0 : 1);
  LqhKeyReq::setApplicationAddressFlag(Treqinfo, TapplAddressIndicator);
  LqhKeyReq::setInterpretedFlag(Treqinfo, regTcPtr->opExec);
  LqhKeyReq::setSeqNoReplica(Treqinfo, regTcPtr->nextSeqNoReplica);
  LqhKeyReq::setAIInLqhKeyReq(Treqinfo, TAiLen);
  LqhKeyReq::setKeyLen(Treqinfo,lqhKeyLen);
  
  if (unlikely(nextVersion < NDBD_ROWID_VERSION))
  {
    LqhKeyReq::setLockType(Treqinfo, regTcPtr->lockType);
  }
  else
  {
    regTcPtr->m_use_rowid |= 
      fragptr.p->m_copy_started_state == Fragrecord::AC_NR_COPY;
    LqhKeyReq::setRowidFlag(Treqinfo, regTcPtr->m_use_rowid);
  }

  if (LqhKeyReq::getRowidFlag(Treqinfo))
  {
    //ndbassert(LqhKeyReq::getOperation(Treqinfo) == ZINSERT);
  }
  else
  {
    if (fragptr.p->m_copy_started_state != Fragrecord::AC_IGNORED)
    {
      ndbassert(LqhKeyReq::getOperation(Treqinfo) != ZINSERT ||
                get_node_status(nextNodeId) != ZNODE_UP);
    }
  }
  
  UintR TreadLenAiInd = (regTcPtr->readlenAi == 0 ? 0 : 1);
  UintR TsameLqhAndClient = (tcConnectptr.i == 
                             regTcPtr->tcOprec ? 0 : 1);
  LqhKeyReq::setSameClientAndTcFlag(Treqinfo, TsameLqhAndClient);
  LqhKeyReq::setReturnedReadLenAIFlag(Treqinfo, TreadLenAiInd);

  /* Long LQHKeyReq uses section size for AttrInfo length */
  UintR TotReclenAi = sendLongReq ? 
    0 :
    regTcPtr->totSendlenAi;

  LqhKeyReq::setReorgFlag(TotReclenAi, regTcPtr->m_reorg);

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
  lqhKeyReq->variableData[nextPos] = sig0;
  nextPos += TreadLenAiInd;

  if (!sendLongReq)
  {
    /* Short LQHKEYREQ to older LQH
     * First few words of KeyInfo go into LQHKEYREQ
     * Sometimes have no Keyinfo
     */
    if (regTcPtr->primKeyLen != 0)
    {
      SegmentedSectionPtr keyInfoSection;
      
      ndbassert(regTcPtr->keyInfoIVal != RNIL);

      getSection(keyInfoSection, regTcPtr->keyInfoIVal);
      SectionReader keyInfoReader(keyInfoSection, g_sectionSegmentPool);
      
      UintR keyLenInLqhKeyReq= MIN(LqhKeyReq::MaxKeyInfo, 
                                   regTcPtr->primKeyLen);
      
      keyInfoReader.getWords(&lqhKeyReq->variableData[nextPos], 
                             keyLenInLqhKeyReq);
      
      nextPos+= keyLenInLqhKeyReq;
    }
  }

  sig0 = regTcPtr->gci_hi;
  Local_key tmp = regTcPtr->m_row_id;
  
  lqhKeyReq->variableData[nextPos + 0] = tmp.m_page_no;
  lqhKeyReq->variableData[nextPos + 1] = tmp.m_page_idx;
  nextPos += 2*LqhKeyReq::getRowidFlag(Treqinfo);

  lqhKeyReq->variableData[nextPos + 0] = sig0;
  nextPos += LqhKeyReq::getGCIFlag(Treqinfo);

  // pass full instance key for remote to map to real instance
  BlockReference lqhRef = numberToRef(DBLQH,
                                      fragptr.p->lqhInstanceKey,
                                      regTcPtr->nextReplica);
  
  if (likely(sendLongReq))
  {
    /* Long LQHKEYREQ, attach KeyInfo and AttrInfo
     * sections to signal
     */
    SectionHandle handle(this);
    handle.m_cnt= 0;

    if (regTcPtr->primKeyLen > 0)
    {
      SegmentedSectionPtr keyInfoSection;
      
      ndbassert(regTcPtr->keyInfoIVal != RNIL);
      getSection(keyInfoSection, regTcPtr->keyInfoIVal);
      
      handle.m_ptr[ LqhKeyReq::KeyInfoSectionNum ]= keyInfoSection;
      handle.m_cnt= 1;

      if (regTcPtr->totSendlenAi > 0)
      {
        SegmentedSectionPtr attrInfoSection;
        
        ndbassert(regTcPtr->attrInfoIVal != RNIL);
        getSection(attrInfoSection, regTcPtr->attrInfoIVal);
        
        handle.m_ptr[ LqhKeyReq::AttrInfoSectionNum ]= attrInfoSection;
        handle.m_cnt= 2;
      }
      else
      {
        /* No AttrInfo to be sent on.  This can occur for delete
         * or with an interpreted update when no actual update 
         * is made
         * In this case, we free any attrInfo section now.
         */
        if (regTcPtr->attrInfoIVal != RNIL)
        {
          ndbassert(!( regTcPtr->m_flags & 
                       TcConnectionrec::OP_SAVEATTRINFO));
          releaseSection(regTcPtr->attrInfoIVal);
          regTcPtr->attrInfoIVal= RNIL;
        }
      }
    }
    else
    {
      /* Zero-length primary key, better not have any
       * AttrInfo
       */
      ndbrequire(regTcPtr->totSendlenAi == 0);
      ndbassert(regTcPtr->keyInfoIVal == RNIL);
      ndbassert(regTcPtr->attrInfoIVal == RNIL);
    }

    sendSignal(lqhRef, GSN_LQHKEYREQ, signal,
               LqhKeyReq::FixedSignalLength + nextPos,
               JBB,
               &handle);
    
    /* Long sections were freed as part of sendSignal */
    ndbassert( handle.m_cnt == 0);
    regTcPtr->keyInfoIVal= RNIL;
    regTcPtr->attrInfoIVal= RNIL;
  }
  else
  {
    /* Short LQHKEYREQ to older LQH
     * First few words of ATTRINFO go into LQHKEYREQ
     * (if they fit)
     */
    if (TAiLen > 0)
    {
      if (likely(nextPos + TAiLen + LqhKeyReq::FixedSignalLength <= 25))
      {
        jam();
        SegmentedSectionPtr attrInfoSection;
        
        ndbassert(regTcPtr->attrInfoIVal != RNIL);
        
        getSection(attrInfoSection, regTcPtr->attrInfoIVal);
        SectionReader attrInfoReader(attrInfoSection, getSectionSegmentPool());
        
        attrInfoReader.getWords(&lqhKeyReq->variableData[nextPos], 
                                TAiLen);
        
        nextPos+= TAiLen;
      }
      else
      {
        /* Not enough space in LQHKEYREQ, we'll send everything in
         * separate ATTRINFO signals
         */
        Treqinfo &= ~(Uint32)(RI_AI_IN_THIS_MASK << RI_AI_IN_THIS_SHIFT);
        lqhKeyReq->requestInfo = Treqinfo;
        TAiLen= 0;
      }
    }
  
    sendSignal(lqhRef, GSN_LQHKEYREQ, signal, 
               nextPos + LqhKeyReq::FixedSignalLength, JBB);

    /* Send extra KeyInfo signals if necessary... */
    if (regTcPtr->primKeyLen > LqhKeyReq::MaxKeyInfo) {
      jam();
      sendTupkey(signal);
    }//if

    /* Send extra AttrInfo signals if necessary... */
    Uint32 remainingAiLen= regTcPtr->totSendlenAi - TAiLen;
    
    if (remainingAiLen != 0)
    {
      sig0 = regTcPtr->tcOprec;
      sig1 = regTcPtr->transid[0];
      sig2 = regTcPtr->transid[1];
      signal->theData[0] = sig0;
      signal->theData[1] = sig1;
      signal->theData[2] = sig2;

      SectionReader attrInfoReader(regTcPtr->attrInfoIVal,
                                   g_sectionSegmentPool);

      ndbassert(attrInfoReader.getSize() == regTcPtr->totSendlenAi);

      /* Step over words already sent in LQHKEYREQ above */
      attrInfoReader.step(TAiLen);

      while (remainingAiLen != 0)
      {
        Uint32 dataInSignal= MIN(AttrInfo::DataLength, remainingAiLen);
        attrInfoReader.getWords(&signal->theData[3],
                                dataInSignal);
        remainingAiLen-= dataInSignal;
        sendSignal(lqhRef, GSN_ATTRINFO, signal, 
                   AttrInfo::HeaderLength + dataInSignal, JBB);
      }
    }
  }

  /* LQHKEYREQ sent */

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
  Local_key rowid = tcConnectptr.p->m_row_id;
  Uint32 totLogLen = ZLOG_HEAD_SIZE + aiLen + keyLen;
  
  if ((logPos + ZLOG_HEAD_SIZE) < ZPAGE_SIZE) {
    Uint32* dataPtr = &logPagePtr.p->logPageWord[logPos];
    logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] = logPos + ZLOG_HEAD_SIZE;
    dataPtr[0] = ZPREP_OP_TYPE;
    dataPtr[1] = totLogLen;
    dataPtr[2] = hashValue;
    dataPtr[3] = operation;
    dataPtr[4] = aiLen;
    dataPtr[5] = keyLen;
    dataPtr[6] = rowid.m_page_no;
    dataPtr[7] = rowid.m_page_idx;
  } else {
    writeLogWord(signal, ZPREP_OP_TYPE);
    writeLogWord(signal, totLogLen);
    writeLogWord(signal, hashValue);
    writeLogWord(signal, operation);
    writeLogWord(signal, aiLen);
    writeLogWord(signal, keyLen);
    writeLogWord(signal, rowid.m_page_no);
    writeLogWord(signal, rowid.m_page_idx);
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
  jam();
  SectionReader keyInfoReader(regTcPtr->keyInfoIVal,
                              g_sectionSegmentPool);
  const Uint32* srcPtr;
  Uint32 length;
  Uint32 wordsWritten= 0;

  /* Write contiguous chunks of words from the KeyInfo
   * section to the log 
   */
  while (keyInfoReader.getWordsPtr(srcPtr,
                                   length))
  {
    writeLogWords(signal, srcPtr, length);
    wordsWritten+= length;
  }

  ndbassert( wordsWritten == regTcPtr->primKeyLen );
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

  jam();
  ndbassert( regTcPtr->attrInfoIVal != RNIL );
  SectionReader attrInfoReader(regTcPtr->attrInfoIVal,
                               g_sectionSegmentPool);
  const Uint32* srcPtr;
  Uint32 length;
  Uint32 wordsWritten= 0;

  /* Write contiguous chunks of words from the 
   * AttrInfo section to the log 
   */
  while (attrInfoReader.getWordsPtr(srcPtr,
                                    length))
  {
    writeLogWords(signal, srcPtr, length);
    wordsWritten+= length;
  }

  ndbassert( wordsWritten == totLen );
}//Dblqh::writeAttrinfoLab()

/* ------------------------------------------------------------------------- */
/* -------          SEND TUPLE KEY IN KEYINFO SIGNAL(S)              ------- */
/*                                                                           */
/*       SUBROUTINE SHORT NAME: STU                                          */
/* ------------------------------------------------------------------------- */
void Dblqh::sendTupkey(Signal* signal) 
{
  BlockReference lqhRef = 0;
  {
    // wl4391_todo fragptr
    FragrecordPtr Tfragptr;
    Tfragptr.i = tcConnectptr.p->fragmentptr;
    c_fragment_pool.getPtr(Tfragptr);
    Uint32 Tnode = tcConnectptr.p->nextReplica;
    Uint32 instanceKey = Tfragptr.p->lqhInstanceKey;
    lqhRef = numberToRef(DBLQH, instanceKey, Tnode);
  }

  signal->theData[0] = tcConnectptr.p->tcOprec;
  signal->theData[1] = tcConnectptr.p->transid[0];
  signal->theData[2] = tcConnectptr.p->transid[1];

  Uint32 remainingLen= tcConnectptr.p->primKeyLen - 
    LqhKeyReq::MaxKeyInfo;

  SectionReader keyInfoReader(tcConnectptr.p->keyInfoIVal,
                              g_sectionSegmentPool);

  ndbassert(keyInfoReader.getSize() > LqhKeyReq::MaxKeyInfo);

  /* Step over the words already sent in LQHKEYREQ */
  keyInfoReader.step(LqhKeyReq::MaxKeyInfo);

  while (remainingLen != 0)
  {
    Uint32 dataInSignal= MIN(KeyInfo::DataLength, remainingLen);
    keyInfoReader.getWords(&signal->theData[3],
                           dataInSignal);
    remainingLen-= dataInSignal;
    sendSignal(lqhRef, GSN_KEYINFO, signal, 
               KeyInfo::HeaderLength + dataInSignal, JBB);
  }
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
  TcConnectionrec * const regTcPtr = tcConnectptr.p;

  /* Release long sections if present */
  releaseSection(regTcPtr->keyInfoIVal);
  regTcPtr->keyInfoIVal = RNIL;
  releaseSection(regTcPtr->attrInfoIVal);
  regTcPtr->attrInfoIVal = RNIL;

  if (regTcPtr->m_dealloc)
  {
    jam();
    regTcPtr->m_dealloc = 0;

    if (TRACENR_FLAG)
      TRACENR("DELETED: " << regTcPtr->m_row_id << endl);

    TRACE_OP(regTcPtr, "DO DEALLOC");
    
    signal->theData[0] = regTcPtr->fragmentid;
    signal->theData[1] = regTcPtr->tableref;
    signal->theData[2] = regTcPtr->m_row_id.m_page_no;
    signal->theData[3] = regTcPtr->m_row_id.m_page_idx;
    signal->theData[4] = RNIL;
    EXECUTE_DIRECT(DBTUP, GSN_TUP_DEALLOCREQ, signal, 5);
  }
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
  /* prevHashptr and nextHashptr may be RNIL when the bucket has 1 element */

  if (prevHashptr.i != RNIL) {
    jam();
    ptrCheckGuard(prevHashptr, ctcConnectrecFileSize, tcConnectionrec);
    ndbassert(prevHashptr.p->nextHashRec == tcConnectptr.i);
    prevHashptr.p->nextHashRec = nextHashptr.i;
  } else {
    jam();
/* ------------------------------------------------------------------------- */
/* THE OPERATION WAS PLACED FIRST IN THE LIST OF THE HASH TABLE. NEED TO SET */
/* A NEW LEADER OF THE LIST.                                                 */
/* ------------------------------------------------------------------------- */
    Uint32 hashIndex = (regTcPtr->transid[0] ^ regTcPtr->tcOprec) & 1023;
    ndbassert(ctransidHash[hashIndex] == tcConnectptr.i);
    ctransidHash[hashIndex] = nextHashptr.i;
  }//if
  if (nextHashptr.i != RNIL) {
    jam();
    ptrCheckGuard(nextHashptr, ctcConnectrecFileSize, tcConnectionrec);
    ndbassert(nextHashptr.p->prevHashRec == tcConnectptr.i);
    nextHashptr.p->prevHashRec = prevHashptr.i;
  }//if

  regTcPtr->prevHashRec = regTcPtr->nextHashRec = RNIL;
}//Dblqh::deleteTransidHash()

/* -------------------------------------------------------------------------
 * -------       RELEASE OPERATION FROM ACTIVE LIST ON FRAGMENT      ------- 
 * 
 *       SUBROUTINE SHORT NAME = RAF
 * ------------------------------------------------------------------------- */
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
  systemErrorLab(signal, __LINE__);
  return;
}//Dblqh::errorReport()

void
Dblqh::execFIRE_TRIG_REQ(Signal* signal)
{
  Uint32 tcOprec = signal->theData[0];
  Uint32 transid1 = signal->theData[1];
  Uint32 transid2 = signal->theData[2];
  Uint32 pass = signal->theData[3];
  Uint32 senderRef = signal->getSendersBlockRef();

  jamEntry();

  if (ERROR_INSERTED_CLEAR(5064))
  {
    // throw away...should cause timeout in TC
    return;
  }

  CRASH_INSERTION(5072);

  Uint32 err;
  if (findTransaction(transid1, transid2, tcOprec, 0) == ZOK &&
      !ERROR_INSERTED_CLEAR(5065) &&
      !ERROR_INSERTED(5070) &&
      !ERROR_INSERTED(5071))
  {
    TcConnectionrec * const regTcPtr = tcConnectptr.p;

    if (unlikely(regTcPtr->transactionState != TcConnectionrec::PREPARED ||
                 ERROR_INSERTED_CLEAR(5067)))
    {
      err = FireTrigRef::FTR_IncorrectState;
      goto do_err;
    }

    /**
     *
     */
    signal->theData[0] = regTcPtr->tupConnectrec;
    signal->theData[1] = regTcPtr->tcBlockref;
    signal->theData[2] = regTcPtr->tcOprec;
    signal->theData[3] = transid1;
    signal->theData[4] = transid2;
    signal->theData[5] = pass;
    Uint32 tup = refToMain(regTcPtr->tcTupBlockref);
    EXECUTE_DIRECT(tup, GSN_FIRE_TRIG_REQ, signal, 6);

    err = signal->theData[0];
    Uint32 cnt = signal->theData[1];

    if (ERROR_INSERTED_CLEAR(5066))
    {
      err = 5066;
    }

    if (ERROR_INSERTED_CLEAR(5068))
      tcOprec++;
    if (ERROR_INSERTED_CLEAR(5069))
      transid1++;

    if (err == 0)
    {
      jam();
      Uint32 Tdata[FireTrigConf::SignalLength];
      FireTrigConf * conf = CAST_PTR(FireTrigConf, Tdata);
      conf->tcOpRec = tcOprec;
      conf->transId[0] = transid1;
      conf->transId[1] = transid2;
      conf->noFiredTriggers = cnt;
      sendFireTrigConfTc(signal, regTcPtr->tcBlockref, Tdata);
      return;
    }
  }
  else
  {
    jam();
    err = FireTrigRef::FTR_UnknownOperation;
  }

do_err:
  if (ERROR_INSERTED_CLEAR(5070))
    tcOprec++;

  if (ERROR_INSERTED_CLEAR(5071))
    transid1++;

  FireTrigRef * ref = CAST_PTR(FireTrigRef, signal->getDataPtrSend());
  ref->tcOpRec = tcOprec;
  ref->transId[0] = transid1;
  ref->transId[1] = transid2;
  ref->errCode = err;
  sendSignal(senderRef, GSN_FIRE_TRIG_REF,
             signal, FireTrigRef::SignalLength, JBB);

  return;
}

void
Dblqh::sendFireTrigConfTc(Signal* signal,
                          BlockReference atcBlockref,
                          Uint32 Tdata[])
{
  Uint32 instanceKey = refToInstance(atcBlockref);

  ndbassert(refToMain(atcBlockref) == DBTC);
  if (instanceKey > MAX_NDBMT_TC_THREADS)
  {
    jam();
    memcpy(signal->theData, Tdata, 4 * FireTrigConf::SignalLength);
    sendSignal(atcBlockref, GSN_FIRE_TRIG_CONF,
               signal, FireTrigConf::SignalLength, JBB);
    return;
  }

  HostRecordPtr Thostptr;
  Thostptr.i = refToNode(atcBlockref);
  ptrCheckGuard(Thostptr, chostFileSize, hostRecord);
  Uint32 len = FireTrigConf::SignalLength;
  struct PackedWordsContainer * container = &Thostptr.p->tc_pack[instanceKey];

  if (container->noOfPackedWords > (25 - len))
  {
    jam();
    sendPackedSignal(signal, container);
  }
  else
  {
    jam();
    updatePackedList(signal, Thostptr.p, Thostptr.i);
  }

  ndbassert(FireTrigConf::SignalLength == 4);
  Uint32 * dst = &container->packedWords[container->noOfPackedWords];
  container->noOfPackedWords += len;
  dst[0] = Tdata[0] | (ZFIRE_TRIG_CONF << 28);
  dst[1] = Tdata[1];
  dst[2] = Tdata[2];
  dst[3] = Tdata[3];
}

bool
Dblqh::check_fire_trig_pass(Uint32 opId, Uint32 pass)
{
  /**
   * Check that trigger only fires once per pass
   *   (per primary key)
   */
  TcConnectionrecPtr regTcPtr;
  regTcPtr.i= opId;
  ptrCheckGuard(regTcPtr, ctcConnectrecFileSize, tcConnectionrec);
  if (regTcPtr.p->m_fire_trig_pass <= pass)
  {
    regTcPtr.p->m_fire_trig_pass = pass + 1;
    return true;
  }
  return false;
}

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
  Uint32 gci_hi = signal->theData[1];
  Uint32 transid1 = signal->theData[2];
  Uint32 transid2 = signal->theData[3];
  Uint32 gci_lo = signal->theData[4];
  jamEntry();
  if (tcIndex >= ttcConnectrecFileSize) {
    errorReport(signal, 0);
    return;
  }//if
  if (ERROR_INSERTED(5011)) {
    CLEAR_ERROR_INSERT_VALUE;
    sendSignalWithDelay(cownref, GSN_COMMIT, signal, 2000,signal->getLength());
    return;
  }//if
  if (ERROR_INSERTED(5012)) {
    SET_ERROR_INSERT_VALUE(5017);
    sendSignalWithDelay(cownref, GSN_COMMIT, signal, 2000,signal->getLength());
    return;
  }//if
  if (ERROR_INSERTED(5062) &&
      ((refToMain(signal->getSendersBlockRef()) == DBTC) ||
       signal->getSendersBlockRef() == reference()))
  {
    Uint32 save = signal->getSendersBlockRef();
    ndbout_c("Delaying execCOMMIT");
    sendSignalWithDelay(cownref, GSN_COMMIT, signal, 2000, signal->getLength());

    if (refToMain(save) == DBTC)
    {
      ndbout_c("killing %u", refToNode(save));
      signal->theData[0] = 9999;
      sendSignal(numberToRef(CMVMI, refToNode(save)),
                 GSN_NDB_TAMPER, signal, 1, JBB);
    }
    return;
  }

  tcConnectptr.i = tcIndex;
  ptrAss(tcConnectptr, regTcConnectionrec);
  if ((tcConnectptr.p->transid[0] == transid1) &&
      (tcConnectptr.p->transid[1] == transid2)) {

    TcConnectionrec * const regTcPtr = tcConnectptr.p;
    TRACE_OP(regTcPtr, "COMMIT");

    CRASH_INSERTION(5048);
    if (ERROR_INSERTED(5049))
    {
      SET_ERROR_INSERT_VALUE(5048);
    }
    
    commitReqLab(signal, gci_hi, gci_lo);
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
  Uint32 gci_hi = signal->theData[2];
  Uint32 transid1 = signal->theData[3];
  Uint32 transid2 = signal->theData[4];
  Uint32 tcOprec = signal->theData[6];
  Uint32 gci_lo = signal->theData[7];

  if (unlikely(signal->getLength() < 8))
  {
    jam();
    gci_lo = 0;
    ndbassert(!ndb_check_micro_gcp(getNodeInfo(refToNode(signal->getSendersBlockRef())).m_version));
  }

  if (ERROR_INSERTED(5004)) {
    systemErrorLab(signal, __LINE__);
  }
  if (ERROR_INSERTED(5017)) {
    CLEAR_ERROR_INSERT_VALUE;
    sendSignalWithDelay(cownref, GSN_COMMITREQ, signal, 2000,
                        signal->getLength());
    return;
  }//if
  if (findTransaction(transid1,
                      transid2,
                      tcOprec, 0) != ZOK) {
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
    commitReqLab(signal, gci_hi, gci_lo);
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
  case TcConnectionrec::WAIT_TUP_COMMIT:
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
  CRASH_INSERTION(5042);

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
  if (ERROR_INSERTED(5063) &&
      ((refToMain(signal->getSendersBlockRef()) == DBTC) ||
       signal->getSendersBlockRef() == reference()))
  {
    Uint32 save = signal->getSendersBlockRef();
    ndbout_c("Delaying execCOMPLETE");
    sendSignalWithDelay(cownref, GSN_COMPLETE,signal, 2000,signal->getLength());

    if (refToMain(save) == DBTC)
    {
      ndbout_c("killing %u", refToNode(save));
      signal->theData[0] = 9999;
      sendSignal(numberToRef(CMVMI, refToNode(save)),
                 GSN_NDB_TAMPER, signal, 1, JBB);
    }
    return;
  }

  tcConnectptr.i = tcIndex;
  ptrAss(tcConnectptr, regTcConnectionrec);
  if ((tcConnectptr.p->transactionState == TcConnectionrec::COMMITTED) &&
      (tcConnectptr.p->transid[0] == transid1) &&
      (tcConnectptr.p->transid[1] == transid2)) {

    TcConnectionrec * const regTcPtr = tcConnectptr.p;
    TRACE_OP(regTcPtr, "COMPLETE");

    if (tcConnectptr.p->seqNoReplica != 0 && 
	tcConnectptr.p->activeCreat == Fragrecord::AC_NORMAL) {
      jam();
      localCommitLab(signal);
      return;
    } 
    else if (tcConnectptr.p->seqNoReplica == 0)
    {
      jam();
      completeTransLastLab(signal);
      return;
    }
    else
    {
      jam();
      completeTransNotLastLab(signal);
      return;
    }
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
    systemErrorLab(signal, __LINE__);
  }
  if (ERROR_INSERTED(5018)) {
    CLEAR_ERROR_INSERT_VALUE;
    sendSignalWithDelay(cownref, GSN_COMPLETEREQ, signal, 2000, 6);
    return;
  }//if
  if (findTransaction(transid1,
                      transid2,
                      tcOprec, 0) != ZOK) {
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
  case TcConnectionrec::WAIT_TUP_COMMIT:
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
  if (regTcPtr->seqNoReplica != 0 && 
      regTcPtr->activeCreat != Fragrecord::AC_NR_COPY) {
    jam();
    localCommitLab(signal);
  } 
  else if (regTcPtr->seqNoReplica == 0)
  {
    jam();
    completeTransLastLab(signal);
  }
  else
  {
    jam();
    completeTransNotLastLab(signal);
  }
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
    jamLine(tcConnectptr.p->connectState);
    ndbrequire(false);
    break;
  }//switch
  return;
}//Dblqh::execLQHKEYCONF()

/* ------------------------------------------------------------------------- */
/* -------                       COMMIT PHASE                        ------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void Dblqh::commitReqLab(Signal* signal, Uint32 gci_hi, Uint32 gci_lo)
{
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  TcConnectionrec::LogWriteState logWriteState = regTcPtr->logWriteState;
  TcConnectionrec::TransactionState transState = regTcPtr->transactionState;
  regTcPtr->gci_hi = gci_hi;
  regTcPtr->gci_lo = gci_lo;
  if (transState == TcConnectionrec::PREPARED) {
    if (logWriteState == TcConnectionrec::WRITTEN) {
      jam();
      regTcPtr->transactionState = TcConnectionrec::PREPARED_RECEIVED_COMMIT;
      TcConnectionrecPtr saveTcPtr = tcConnectptr;
      Uint32 blockNo = refToMain(regTcPtr->tcTupBlockref);
      signal->theData[0] = regTcPtr->tupConnectrec;
      signal->theData[1] = gci_hi;
      signal->theData[2] = gci_lo;
      EXECUTE_DIRECT(blockNo, GSN_TUP_WRITELOG_REQ, signal, 3);
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
  if (regTcPtr->seqNoReplica == 0 ||
      regTcPtr->activeCreat == Fragrecord::AC_NR_COPY) {
    jam();
    localCommitLab(signal);
    return;
  }//if
  commitReplyLab(signal);
  return;
}//Dblqh::commitReqLab()

void Dblqh::execLQH_WRITELOG_REQ(Signal* signal)
{
  jamEntry();
  tcConnectptr.i = signal->theData[0];
  ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  Uint32 gci_hi = signal->theData[1];
  Uint32 gci_lo = signal->theData[2];
  Uint32 newestGci = cnewestGci;
  TcConnectionrec::LogWriteState logWriteState = regTcPtr->logWriteState;
  TcConnectionrec::TransactionState transState = regTcPtr->transactionState;
  regTcPtr->gci_hi = gci_hi;
  regTcPtr->gci_lo = gci_lo;
  if (gci_hi > newestGci) {
    jam();
/* ------------------------------------------------------------------------- */
/*       KEEP TRACK OF NEWEST GLOBAL CHECKPOINT THAT LQH HAS HEARD OF.       */
/* ------------------------------------------------------------------------- */
    cnewestGci = gci_hi;
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
    regLogPartPtr.i = regTcPtr->m_log_part_ptr_i;
    ptrCheckGuard(regLogPartPtr, clogPartFileSize, logPartRecord);
    if (!regLogPartPtr.p->m_log_complete_queue.isEmpty() ||
        (noOfLogPages == 0))
    {
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
      linkWaitLog(signal, regLogPartPtr, regLogPartPtr.p->m_log_complete_queue);
      if (transState == TcConnectionrec::PREPARED) {
        jam();
        regTcPtr->transactionState = TcConnectionrec::LOG_COMMIT_QUEUED_WAIT_SIGNAL;
      } else {
        jam();
        ndbrequire(transState == TcConnectionrec::PREPARED_RECEIVED_COMMIT);
        regTcPtr->transactionState = TcConnectionrec::LOG_COMMIT_QUEUED;
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
  c_fragment_pool.getPtr(regFragptr);
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
  Ptr<TcConnectionrec> regTcPtr = tcConnectptr;
  Ptr<Fragrecord> regFragptr = fragptr;
  Uint32 operation = regTcPtr.p->operation;
  Uint32 dirtyOp = regTcPtr.p->dirtyOp;
  Uint32 opSimple = regTcPtr.p->opSimple;
  Uint32 normalProtocol = LqhKeyReq::getNormalProtocolFlag(regTcPtr.p->reqinfo);

  if (regTcPtr.p->activeCreat != Fragrecord::AC_IGNORED) {
    if (operation != ZREAD) {
      TupCommitReq * const tupCommitReq = 
        (TupCommitReq *)signal->getDataPtrSend();
      Uint32 sig0 = regTcPtr.p->tupConnectrec;
      Uint32 tup = refToMain(regTcPtr.p->tcTupBlockref);
      jam();
      tupCommitReq->opPtr = sig0;
      tupCommitReq->gci_hi = regTcPtr.p->gci_hi;
      tupCommitReq->hashValue = regTcPtr.p->hashValue;
      tupCommitReq->diskpage = RNIL;
      tupCommitReq->gci_lo = regTcPtr.p->gci_lo;
      tupCommitReq->transId1 = regTcPtr.p->transid[0];
      tupCommitReq->transId2 = regTcPtr.p->transid[1];
      EXECUTE_DIRECT(tup, GSN_TUP_COMMITREQ, signal, 
		     TupCommitReq::SignalLength);

      if (TRACENR_FLAG)
      {
	TRACENR("COMMIT: ");
	switch (regTcPtr.p->operation) {
	case ZREAD: TRACENR("READ"); break;
	case ZUPDATE: TRACENR("UPDATE"); break;
	case ZWRITE: TRACENR("WRITE"); break;
	case ZINSERT: TRACENR("INSERT"); break;
	case ZDELETE: TRACENR("DELETE"); break;
        case ZUNLOCK: TRACENR("UNLOCK"); break;
	}

	TRACENR(" tab: " << regTcPtr.p->tableref 
	       << " frag: " << regTcPtr.p->fragmentid
	       << " activeCreat: " << (Uint32)regTcPtr.p->activeCreat);
	if (LqhKeyReq::getNrCopyFlag(regTcPtr.p->reqinfo))
	  TRACENR(" NrCopy");
	if (LqhKeyReq::getRowidFlag(regTcPtr.p->reqinfo))
	  TRACENR(" rowid: " << regTcPtr.p->m_row_id);
	TRACENR(" key: " << getKeyInfoWordOrZero(regTcPtr.p, 0));

        if (signal->theData[0] != 0)
          TRACENR(" TIMESLICE");
	TRACENR(endl);
      }

      if(signal->theData[0] != 0)
      {
        regTcPtr.p->transactionState = TcConnectionrec::WAIT_TUP_COMMIT;
        return; // TUP_COMMIT was timesliced
      }

      TRACE_OP(regTcPtr.p, "ACC_COMMITREQ");

      Uint32 acc = refToMain(regTcPtr.p->tcAccBlockref);
      signal->theData[0] = regTcPtr.p->accConnectrec;
      EXECUTE_DIRECT(acc, GSN_ACC_COMMITREQ, signal, 1);
      
    } else {
      if(!dirtyOp){
	TRACE_OP(regTcPtr.p, "ACC_COMMITREQ");

	Uint32 acc = refToMain(regTcPtr.p->tcAccBlockref);
	signal->theData[0] = regTcPtr.p->accConnectrec;
	EXECUTE_DIRECT(acc, GSN_ACC_COMMITREQ, signal, 1);
      }
      
      if (dirtyOp && normalProtocol == 0)
      {
	jam();
        /**
         * The dirtyRead does not send anything but TRANSID_AI from LDM
         */
	fragptr = regFragptr;
	tcConnectptr = regTcPtr;
	cleanUp(signal);
	return;
      }

      /**
       * The simpleRead will send a LQHKEYCONF
       *   but have already released the locks
       */
      if (opSimple)
      {
	fragptr = regFragptr;
	tcConnectptr = regTcPtr;
        packLqhkeyreqLab(signal);
        return;
      }
    }
  }//if
  jamEntry();
  fragptr = regFragptr;
  tcConnectptr = regTcPtr;
  tupcommit_conf(signal, regTcPtr.p, regFragptr.p);
}

void
Dblqh::tupcommit_conf_callback(Signal* signal, Uint32 tcPtrI)
{
  jamEntry();

  tcConnectptr.i = tcPtrI;
  ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
  TcConnectionrec * tcPtr = tcConnectptr.p;

  ndbrequire(tcPtr->transactionState == TcConnectionrec::WAIT_TUP_COMMIT);

  FragrecordPtr regFragptr;
  regFragptr.i = tcPtr->fragmentptr;
  c_fragment_pool.getPtr(regFragptr);
  fragptr = regFragptr;

  TRACE_OP(tcPtr, "ACC_COMMITREQ");
  
  Uint32 acc = refToMain(tcPtr->tcAccBlockref);
  signal->theData[0] = tcPtr->accConnectrec;
  EXECUTE_DIRECT(acc, GSN_ACC_COMMITREQ, signal, 1);
  jamEntry();

  tcConnectptr.i = tcPtrI;
  tcConnectptr.p = tcPtr;
  tupcommit_conf(signal, tcPtr, regFragptr.p);
}

void
Dblqh::tupcommit_conf(Signal* signal, 
		      TcConnectionrec * tcPtrP,
		      Fragrecord * regFragptr)
{
  Uint32 dirtyOp = tcPtrP->dirtyOp;
  Uint32 seqNoReplica = tcPtrP->seqNoReplica;
  Uint32 activeCreat = tcPtrP->activeCreat;
  if (tcPtrP->gci_hi > regFragptr->newestGci) {
    jam();
/* ------------------------------------------------------------------------- */
/*IT IS THE FIRST TIME THIS GLOBAL CHECKPOINT IS INVOLVED IN UPDATING THIS   */
/*FRAGMENT. UPDATE THE VARIABLE THAT KEEPS TRACK OF NEWEST GCI IN FRAGMENT   */
/* ------------------------------------------------------------------------- */
    regFragptr->newestGci = tcPtrP->gci_hi;
  }//if
  if (dirtyOp != ZTRUE) 
  {
    if (seqNoReplica == 0 || activeCreat == Fragrecord::AC_NR_COPY)
    {
      jam();
      commitReplyLab(signal);
      return;
    }//if
    if (seqNoReplica == 0)
    {
      jam();
      completeTransLastLab(signal);
    }
    else
    {      
      jam();
      completeTransNotLastLab(signal);
    }
    return;
  } else {
/* ------------------------------------------------------------------------- */
/*WE MUST HANDLE DIRTY WRITES IN A SPECIAL WAY. THESE OPERATIONS WILL NOT    */
/*SEND ANY COMMIT OR COMPLETE MESSAGES TO OTHER NODES. THEY WILL MERELY SEND */
/*THOSE SIGNALS INTERNALLY.                                                  */
/* ------------------------------------------------------------------------- */
    if (tcPtrP->abortState == TcConnectionrec::ABORT_IDLE) 
    {
      jam();
      if (activeCreat == Fragrecord::AC_NR_COPY)
      {
	jam();
	ndbrequire(LqhKeyReq::getNrCopyFlag(tcPtrP->reqinfo));
	ndbrequire(tcPtrP->m_nr_delete.m_cnt == 0);
      }
      packLqhkeyreqLab(signal);
    } 
    else 
    {
      ndbrequire(tcPtrP->abortState != TcConnectionrec::NEW_FROM_TC);
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
  Uint32 op = locTcConnectptr.p->operation;
  locTcConnectptr.p->tcTimer = 0;
  locTcConnectptr.p->transactionState = TcConnectionrec::TC_NOT_CONNECTED;
  locTcConnectptr.p->nextTcConnectrec = cfirstfreeTcConrec;
  cfirstfreeTcConrec = locTcConnectptr.i;

  ndbassert(locTcConnectptr.p->tcScanRec == RNIL);

  TablerecPtr tabPtr;
  tabPtr.i = locTcConnectptr.p->tableref;
  if(tabPtr.i == RNIL)
    return;

  ptrCheckGuard(tabPtr, ctabrecFileSize, tablerec);
  
  /**
   * Normal case
   */
  if (op == ZREAD || op == ZUNLOCK)
  {
    ndbrequire(tabPtr.p->usageCountR > 0);
    tabPtr.p->usageCountR--;
  }
  else
  {
    ndbrequire(tabPtr.p->usageCountW > 0);
    tabPtr.p->usageCountW--;
  }
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
void
Dblqh::remove_commit_marker(TcConnectionrec * const regTcPtr)
{
  Ptr<CommitAckMarker> tmp;
  Uint32 commitAckMarker = regTcPtr->commitAckMarker;
  regTcPtr->commitAckMarker = RNIL;
  if (commitAckMarker == RNIL)
    return;
  jam();
  m_commitAckMarkerHash.getPtr(tmp, commitAckMarker);
#ifdef MARKER_TRACE
  ndbout_c("%u remove marker[%.8x %.8x] op: %u ref: %u", 
           instance(), tmp.p->transid1, tmp.p->transid2,
           Uint32(regTcPtr - tcConnectionrec), tmp.p->reference_count);
#endif
  ndbrequire(tmp.p->reference_count > 0);
  tmp.p->reference_count--;
  if (tmp.p->reference_count == 0)
  {
    jam();
    m_commitAckMarkerHash.release(tmp);
  }
}

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
                      tcOprec, 0) != ZOK) {
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
  
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  if (ERROR_INSERTED(5100))
  {
    SET_ERROR_INSERT_VALUE(5101);
    return;
  }
  CRASH_INSERTION2(5101, regTcPtr->nextReplica != ZNIL);
  
/* ------------------------------------------------------------------------- */
/*A GUIDING DESIGN PRINCIPLE IN HANDLING THESE ERROR SITUATIONS HAVE BEEN    */
/*KEEP IT SIMPLE. THUS WE RATHER INSERT A WAIT AND SET THE ABORT_STATE TO    */
/*ACTIVE RATHER THAN WRITE NEW CODE TO HANDLE EVERY SPECIAL SITUATION.       */
/* ------------------------------------------------------------------------- */
  if (regTcPtr->nextReplica != ZNIL) {
/* ------------------------------------------------------------------------- */
// We will immediately send the ABORT message also to the next LQH node in line.
/* ------------------------------------------------------------------------- */
    FragrecordPtr Tfragptr;
    Tfragptr.i = regTcPtr->fragmentptr;
    c_fragment_pool.getPtr(Tfragptr);
    Uint32 Tnode = regTcPtr->nextReplica;
    Uint32 instanceKey = Tfragptr.p->lqhInstanceKey;
    BlockReference TLqhRef = numberToRef(DBLQH, instanceKey, Tnode);
    signal->theData[0] = regTcPtr->tcOprec;
    signal->theData[1] = regTcPtr->tcBlockref;
    signal->theData[2] = regTcPtr->transid[0];
    signal->theData[3] = regTcPtr->transid[1];
    sendSignal(TLqhRef, GSN_ABORT, signal, 4, JBB);
  }//if
  regTcPtr->abortState = TcConnectionrec::ABORT_FROM_TC;

  remove_commit_marker(regTcPtr);
  TRACE_OP(regTcPtr, "ABORT");

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
    systemErrorLab(signal, __LINE__);
  }
  if (ERROR_INSERTED(5016)) {
    CLEAR_ERROR_INSERT_VALUE;
    sendSignalWithDelay(cownref, GSN_ABORTREQ, signal, 2000, 6);
    return;
  }//if
  if (findTransaction(transid1,
                      transid2,
                      tcOprec, 0) != ZOK) {
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

  if (TRACENR_FLAG)
  {
    TRACENR("ACCKEYREF: " << errCode << " ");
    switch (tcPtr->operation) {
    case ZREAD: TRACENR("READ"); break;
    case ZUPDATE: TRACENR("UPDATE"); break;
    case ZWRITE: TRACENR("WRITE"); break;
    case ZINSERT: TRACENR("INSERT"); break;
    case ZDELETE: TRACENR("DELETE"); break;
    case ZUNLOCK: TRACENR("UNLOCK"); break;
    default: TRACENR("<Unknown: " << tcPtr->operation << ">"); break;
    }
    
    TRACENR(" tab: " << tcPtr->tableref 
	   << " frag: " << tcPtr->fragmentid
	   << " activeCreat: " << (Uint32)tcPtr->activeCreat);
    if (LqhKeyReq::getNrCopyFlag(tcPtr->reqinfo))
      TRACENR(" NrCopy");
    if (LqhKeyReq::getRowidFlag(tcPtr->reqinfo))
      TRACENR(" rowid: " << tcPtr->m_row_id);
    TRACENR(" key: " << getKeyInfoWordOrZero(tcPtr, 0));
    TRACENR(endl);
    
  }

  ndbrequire(tcPtr->activeCreat == Fragrecord::AC_NORMAL);
  ndbrequire(!LqhKeyReq::getNrCopyFlag(tcPtr->reqinfo));
  
  /**
   * Not only primary replica can get ZTUPLE_ALREADY_EXIST || ZNO_TUPLE_FOUND
   *
   * 1) op1 - primary insert ok
   * 2) op1 - backup insert fail (log full or what ever)
   * 3) op1 - delete ok @ primary
   * 4) op1 - delete fail @ backup
   *
   * -> ZNO_TUPLE_FOUND is possible
   *
   * 1) op1 primary delete ok
   * 2) op1 backup delete fail (log full or what ever)
   * 3) op2 insert ok @ primary
   * 4) op2 insert fail @ backup
   *
   * -> ZTUPLE_ALREADY_EXIST
   */
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
    return;
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
    abortContinueAfterBlockedLab(signal);
    return;
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
  case TcConnectionrec::WAIT_TUP_COMMIT:
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
    if (regTcPtr->opSimple) {
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
    sendLqhTransconf(signal, LqhTransConf::Committed);
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
  abortCommonLab(signal);
  return;
}//Dblqh::abortErrorLab()

void Dblqh::abortCommonLab(Signal* signal) 
{
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  const Uint32 activeCreat = regTcPtr->activeCreat;

  remove_commit_marker(regTcPtr);

  if (unlikely(activeCreat == Fragrecord::AC_NR_COPY))
  {
    jam();
    if (regTcPtr->m_nr_delete.m_cnt)
    {
      jam();
      /**
       * Let operation wait for pending NR operations
       */
      
#ifdef VM_TRACE
      /**
       * Only disk table can have pending ops...
       */
      TablerecPtr tablePtr;
      tablePtr.i = regTcPtr->tableref;
      ptrCheckGuard(tablePtr, ctabrecFileSize, tablerec);
      ndbrequire(tablePtr.p->m_disk_table);
#endif
      return;
    }
  }
  
  fragptr.i = regTcPtr->fragmentptr;
  if (fragptr.i != RNIL) {
    jam();
    c_fragment_pool.getPtr(fragptr);
    switch (fragptr.p->fragStatus) {
    case Fragrecord::FSACTIVE:
    case Fragrecord::CRASH_RECOVERING:
    case Fragrecord::ACTIVE_CREATION:
      jam();
      abortContinueAfterBlockedLab(signal);
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

void Dblqh::abortContinueAfterBlockedLab(Signal* signal) 
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
   * ----------------------------------------------------------------------- */
  TcConnectionrec * const regTcPtr = tcConnectptr.p;
  
  TRACE_OP(regTcPtr, "ACC ABORT");
  Uint32 canBlock = 2; // 2, block if needed
  switch(regTcPtr->transactionState){
  case TcConnectionrec::WAIT_TUP:
    jam();
    /**
     * This is when getting from execTUPKEYREF
     *   in which case we *do* have ACC lock
     *   and should not (need to) block
     */
    canBlock = 0;
    break;
  default:
    break;
  }

  regTcPtr->transactionState = TcConnectionrec::WAIT_ACC_ABORT;
  signal->theData[0] = regTcPtr->accConnectrec;
  signal->theData[1] = canBlock;
  EXECUTE_DIRECT(DBACC, GSN_ACC_ABORTREQ, signal, 2);

  if (signal->theData[1] == RNIL)
  {
    jam();
    /* ------------------------------------------------------------------------
     * We need to insert a real-time break by sending ACC_ABORTCONF through the
     * job buffer to ensure that we catch any ACCKEYCONF or TUPKEYCONF or
     * TUPKEYREF that are in the job buffer but not yet processed. Doing 
     * everything without that would race and create a state error when they 
     * are executed.
     * --------------------------------------------------------------------- */
    return;
  }
  
  execACC_ABORTCONF(signal);
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
  
  TRACE_OP(regTcPtr, "ACC_ABORTCONF");
  signal->theData[0] = regTcPtr->tupConnectrec;
  EXECUTE_DIRECT(DBTUP, GSN_TUP_ABORTREQ, signal, 1);

  jamEntry(); 
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
    if (cnoOfLogPages == 0 ||
        !logPartPtr.p->m_log_complete_queue.isEmpty())
    {
      jam();
      /* --------------------------------------------------------------------
       * A PREPARE OPERATION IS CURRENTLY WRITING IN THE LOG. 
       * WE MUST WAIT ON OUR TURN TO WRITE THE LOG. 
       * IT IS NECESSARY TO WRITE ONE LOG RECORD COMPLETELY 
       * AT A TIME OTHERWISE WE WILL SCRAMBLE THE LOG.
       * -------------------------------------------------------------------- */
      linkWaitLog(signal, logPartPtr, logPartPtr.p->m_log_complete_queue);
      regTcPtr->transactionState = TcConnectionrec::LOG_ABORT_QUEUED;
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

  remove_commit_marker(regTcPtr);

  if (regTcPtr->operation == ZREAD && regTcPtr->dirtyOp &&
      !LqhKeyReq::getNormalProtocolFlag(regTcPtr->reqinfo))
  {
    jam();
    TcKeyRef * const tcKeyRef = (TcKeyRef *) signal->getDataPtrSend();
    
    tcKeyRef->connectPtr = regTcPtr->applOprec;
    tcKeyRef->transId[0] = regTcPtr->transid[0];
    tcKeyRef->transId[1] = regTcPtr->transid[1];
    tcKeyRef->errorCode = regTcPtr->errorCode;
    sendTCKEYREF(signal, regTcPtr->applRef, regTcPtr->tcBlockref, 0);
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

void
Dblqh::sendTCKEYREF(Signal* signal, Uint32 ref, Uint32 routeRef, Uint32 cnt)
{
  const Uint32 nodeId = refToNode(ref);
  const bool connectedToNode = getNodeInfo(nodeId).m_connected;
  
  if (likely(connectedToNode))
  {
    jam();
    sendSignal(ref, GSN_TCKEYREF, signal, TcKeyRef::SignalLength, JBB);
  }
  else
  {
    if (routeRef && 
	getNodeInfo(refToNode(routeRef)).m_version >= MAKE_VERSION(5,1,14))
    {
      jam();
      memmove(signal->theData+25, signal->theData, 4*TcKeyRef::SignalLength);
      RouteOrd* ord = (RouteOrd*)signal->getDataPtrSend();
      ord->dstRef = ref;
      ord->srcRef = reference();
      ord->gsn = GSN_TCKEYREF;
      ord->cnt = 0;
      LinearSectionPtr ptr[3];
      ptr[0].p = signal->theData+25;
      ptr[0].sz = TcKeyRef::SignalLength;
      sendSignal(routeRef, GSN_ROUTE_ORD, signal, RouteOrd::SignalLength, JBB,
		 ptr, 1);
    }
    else
    {
      jam();
      memmove(signal->theData + 3, signal->theData, 4*TcKeyRef::SignalLength);
      signal->theData[0] = ZRETRY_TCKEYREF;
      signal->theData[1] = cnt + 1;
      signal->theData[2] = ref;
      sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100,
			  TcKeyRef::SignalLength + 3);
    }
  }
}

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
    if(NdbNodeBitmask::get(nodeFail->theNodes, i)){
      jam();
      Tdata[index] = i;
      index++;
    }//if
  }//for

#ifdef ERROR_INSERT
  c_master_node_id = nodeFail->masterNodeId;
#endif
  
  lcpPtr.i = 0;
  ptrAss(lcpPtr, lcpRecord);
  
  ndbrequire(index == TnoOfNodes);
  ndbrequire(cnoOfNodes - 1 < MAX_NDB_NODES);
  for (i = 0; i < TnoOfNodes; i++) {
    const Uint32 nodeId = Tdata[i];

    {
      HostRecordPtr Thostptr;
      Thostptr.i = nodeId;
      ptrCheckGuard(Thostptr, chostFileSize, hostRecord);
      Thostptr.p->nodestatus = ZNODE_DOWN;
    }

    lcpPtr.p->m_EMPTY_LCP_REQ.clear(nodeId);
    
    for (Uint32 j = 0; j < cnoOfNodes; j++) {
      jam();
      if (cnodeData[j] == nodeId){
        jam();
        cnodeStatus[j] = ZNODE_DOWN;
	
        TfoundNodes++;
      }//if
    }//for

    /* Perform block-level ndbd failure handling */
    Callback cb = { safe_cast(&Dblqh::ndbdFailBlockCleanupCallback),
                    Tdata[i] };
    simBlockNodeFailure(signal, Tdata[i], cb);
  }//for
  ndbrequire(TnoOfNodes == TfoundNodes);
}//Dblqh::execNODE_FAILREP()


void
Dblqh::ndbdFailBlockCleanupCallback(Signal* signal,
                                    Uint32 failedNodeId,
                                    Uint32 ignoredRc)
{
  jamEntry();

  NFCompleteRep * const nfCompRep = (NFCompleteRep *)&signal->theData[0];
  nfCompRep->blockNo      = DBLQH;
  nfCompRep->nodeId       = cownNodeid;
  nfCompRep->failedNodeId = failedNodeId;
  BlockReference dihRef = !isNdbMtLqh() ? DBDIH_REF : DBLQH_REF;
  sendSignal(dihRef, GSN_NF_COMPLETEREP, signal, 
             NFCompleteRep::SignalLength, JBB);
}

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

  if (!checkNodeFailSequence(signal))
  {
    jam();
    return;
  }
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
#ifdef ERROR_INSERT
      if (ERROR_INSERTED(5061))
      {
        CLEAR_ERROR_INSERT_VALUE;
        for (Uint32 i = 0; i < cnoOfNodes; i++)
        {
          Uint32 node = cnodeData[i];
          if (node != getOwnNodeId() && cnodeStatus[i] == ZNODE_UP)
          {
            ndbout_c("clearing ERROR_INSERT in LQH:%u", node);
            signal->theData[0] = 0;
            sendSignal(numberToRef(DBLQH, node), GSN_NDB_TAMPER,
                       signal, 1, JBB);
          }
        }
        
        signal->theData[0] = ZSCAN_MARKERS;
        signal->theData[1] = tcNodeFailptr.i;
        signal->theData[2] = 0;
        signal->theData[3] = RNIL;
        sendSignalWithDelay(cownref, GSN_CONTINUEB, signal, 5000, 4);
        return;
      }

      if (ERROR_INSERTED(5050))
      {
        ndbout_c("send ZSCAN_MARKERS with 5s delay and killing master: %u",
                 c_master_node_id);
        CLEAR_ERROR_INSERT_VALUE;
        signal->theData[0] = ZSCAN_MARKERS;
        signal->theData[1] = tcNodeFailptr.i;
        signal->theData[2] = 0;
        signal->theData[3] = RNIL;
        sendSignalWithDelay(cownref, GSN_CONTINUEB, signal, 5000, 4);
        
        signal->theData[0] = 9999;
        sendSignal(numberToRef(CMVMI, c_error_insert_extra),
                   GSN_NDB_TAMPER, signal, 1, JBB);
        return;
      }
#endif
      scanMarkers(signal, tcNodeFailptr.i, 0, RNIL);
      return;
    }//if
    ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
    if (tcConnectptr.p->transactionState != TcConnectionrec::IDLE) {
      if (tcConnectptr.p->transactionState != TcConnectionrec::TC_NOT_CONNECTED) {
        if (tcConnectptr.p->tcScanRec == RNIL) {
          if (refToNode(tcConnectptr.p->tcBlockref) == tcNodeFailptr.p->oldNodeId) {
            switch( tcConnectptr.p->operation ) {
            case ZUNLOCK :
              jam(); /* Skip over */
              break;
            case ZREAD :
              jam();
              if (tcConnectptr.p->opSimple == ZTRUE) {
                jam();
                break; /* Skip over */
              }
              /* Fall through */
            default :
              jam();
              tcConnectptr.p->tcNodeFailrec = tcNodeFailptr.i;
              tcConnectptr.p->abortState = TcConnectionrec::NEW_FROM_TC;
              abortStateHandlerLab(signal);
              return;
            } // switch
          }//if
        } else {
          scanptr.i = tcConnectptr.p->tcScanRec;
	  c_scanRecordPool.getPtr(scanptr);
	  switch(scanptr.p->scanType){
	  case ScanRecord::COPY: 
	  {
            jam();
            if (scanptr.p->scanNodeId == tcNodeFailptr.p->oldNodeId) {
              jam();
	      /* ------------------------------------------------------------
	       * THE RECEIVER OF THE COPY HAVE FAILED. 
	       * WE HAVE TO CLOSE THE COPY PROCESS. 
	       * ----------------------------------------------------------- */
	      if (0) ndbout_c("close copy");
              tcConnectptr.p->tcNodeFailrec = tcNodeFailptr.i;
              tcConnectptr.p->abortState = TcConnectionrec::NEW_FROM_TC;
              closeCopyRequestLab(signal);
              return;
            }
	    break;
	  }
	  case ScanRecord::SCAN:
	  {
	    jam();
	    if (refToNode(tcConnectptr.p->tcBlockref) == 
		tcNodeFailptr.p->oldNodeId) {
	      jam();
	      tcConnectptr.p->tcNodeFailrec = tcNodeFailptr.i;
	      tcConnectptr.p->abortState = TcConnectionrec::NEW_FROM_TC;
	      closeScanRequestLab(signal);
	      return;
	    }//if
	    break;
	  }
	  default:
            ndbout_c("scanptr.p->scanType: %u", scanptr.p->scanType);
            ndbout_c("tcConnectptr.p->transactionState: %u",
                     tcConnectptr.p->transactionState);
	    ndbrequire(false);
	  }
        }
      }
      else
      {
#if defined VM_TRACE || defined ERROR_INSERT
        jam();
        ndbrequire(tcConnectptr.p->tcScanRec == RNIL);
#endif
      }
    }
    else
    {
#if defined VM_TRACE || defined ERROR_INSERT
      jam();
      ndbrequire(tcConnectptr.p->tcScanRec == RNIL);
#endif
    }
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

  if (tcNodeFailPtr.p->tcFailStatus == TcNodeFailRecord::TC_STATE_BREAK)
  {
    jam();
    
    /* ----------------------------------------------------------------------
     *  AN INTERRUPTION TO THIS NODE FAIL HANDLING WAS RECEIVED AND A NEW 
     *  TC HAVE BEEN ASSIGNED TO TAKE OVER THE FAILED TC. PROBABLY THE OLD 
     *  NEW TC HAVE FAILED.
     * ---------------------------------------------------------------------- */
    tcNodeFailptr = tcNodeFailPtr;
    lqhTransNextLab(signal);
    return;
  }
  
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
  ndbrequire(refToMain(signal->getSendersBlockRef()) == DBTUX);
  const AccScanRef refCopy = *(const AccScanRef*)signal->getDataPtr();
  const AccScanRef* ref = &refCopy;
  ndbrequire(ref->errorCode != 0);

  scanptr.i = ref->scanPtr;
  c_scanRecordPool.getPtr(scanptr);
  tcConnectptr.i = scanptr.p->scanTcrec;
  ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
  tcConnectptr.p->errorCode = ref->errorCode;

  /*
   * MRR scan can hit this between 2 DBTUX scans.  Previous range has
   * terminated via last NEXT_SCANCONF, then index is set to Dropping,
   * and then next range is started and returns ACC_SCANREF.
   */
  if (scanptr.p->scanStoredProcId != RNIL) {
    jam();
    scanptr.p->scanCompletedStatus = ZTRUE;
    accScanCloseConfLab(signal);
    return;
  }
  tupScanCloseConfLab(signal);
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
  scanptr.p->m_row_id.m_page_no = nextScanConf->localKey[0];
  scanptr.p->m_row_id.m_page_idx = nextScanConf->localKey[1];

#ifdef VM_TRACE
  if (signal->getLength() > 2 && nextScanConf->accOperationPtr != RNIL)
  {
    Ptr<TcConnectionrec> regTcPtr;
    regTcPtr.i = scanptr.p->scanTcrec;
    ptrCheckGuard(regTcPtr, ctcConnectrecFileSize, tcConnectionrec);
    ndbassert(regTcPtr.p->fragmentid == nextScanConf->fragId);
  }
#endif
  
  fragptr.i = scanptr.p->fragPtrI;
  c_fragment_pool.getPtr(fragptr);
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
    ndbout_c("%d", scanptr.p->scanState);
    ndbrequire(false);
  }//switch
}//Dblqh::execNEXT_SCANCONF()

/* ***************> */
/*  NEXT_SCANREF  > */
/* ***************> */
void Dblqh::execNEXT_SCANREF(Signal* signal) 
{
  jamEntry();
  ndbrequire(refToMain(signal->getSendersBlockRef()) == DBTUX);
  const NextScanRef refCopy = *(const NextScanRef*)signal->getDataPtr();
  const NextScanRef* ref = &refCopy;
  ndbrequire(ref->errorCode != 0);

  scanptr.i = ref->scanPtr;
  c_scanRecordPool.getPtr(scanptr);
  tcConnectptr.i = scanptr.p->scanTcrec;
  ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
  tcConnectptr.p->errorCode = ref->errorCode;

  /*
   * MRR scan may have other ranges left.  But the scan has already
   * failed.  Terminate the scan now.
   */
  scanptr.p->scanCompletedStatus = ZTRUE;
  accScanCloseConfLab(signal);
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
    tupScanCloseConfLab(signal);
    break;
  case ScanRecord::WAIT_STORED_PROC_COPY:
    jam();
    scanptr.p->scanStoredProcId = storedProcId;
    storedProcConfCopyLab(signal);
    break;
  case ScanRecord::WAIT_DELETE_STORED_PROC_ID_COPY:
    jam();
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
  Uint32 hashHi = signal->getSendersBlockRef();
  // bug#13834481 hashHi!=0 caused timeout (tx not found)
  const NodeInfo& senderInfo = getNodeInfo(refToNode(hashHi));
  if (unlikely(senderInfo.m_version < NDBD_LONG_SCANFRAGREQ))
    hashHi = 0;

  if (findTransaction(transid1, transid2, senderData, hashHi) != ZOK){
    jam();
    DEBUG(senderData << 
	  " Received SCAN_NEXTREQ in LQH with close flag when closed");
    ndbrequire(nextReq->requestInfo == ScanFragNextReq::ZCLOSE);
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
  if (ERROR_INSERTED(5025))
  {
    /**
     * This does not work as signal->getSendersBlockRef() is used
     *   as "hashHi"...not having a real data-word for this is not optimal
     *   but it will work...summary: disable this ERROR_INSERT
     */
    CLEAR_ERROR_INSERT_VALUE;
  }

  if (ERROR_INSERTED(5030)){
    ndbout << "ERROR 5030" << endl;
    CLEAR_ERROR_INSERT_VALUE;
    // Drop signal
    return;
  }//if

  if(ERROR_INSERTED(5036)){
    return;
  }

  Uint32 pos = 0;
  if (ScanFragNextReq::getCorrFactorFlag(nextReq->requestInfo))
  {
    jam();
    Uint32 corrFactorLo = nextReq->variableData[pos++];
    tcConnectptr.p->m_corrFactorLo &= 0xFFFF0000;
    tcConnectptr.p->m_corrFactorLo |= corrFactorLo;
  }

  scanptr.i = tcConnectptr.p->tcScanRec;
  ndbrequire(scanptr.i != RNIL);
  c_scanRecordPool.getPtr(scanptr);
  scanptr.p->scanTcWaiting = cLqhTimeOutCount;

  /* ------------------------------------------------------------------
   * If close flag is set this scan should be closed
   * If we are waiting for SCAN_NEXTREQ set flag to stop scanning and 
   * continue execution else set flags and wait until the scan 
   * completes itself
   * ------------------------------------------------------------------ */
  if (nextReq->requestInfo == ScanFragNextReq::ZCLOSE)
  {
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
  c_fragment_pool.getPtr(fragptr);

  /**
   * Change parameters while running
   *   (is currently not supported)
   */
  const Uint32 max_rows = nextReq->batch_size_rows;
  const Uint32 max_bytes = nextReq->batch_size_bytes;
  scanptr.p->m_max_batch_size_bytes = max_bytes;

  if (max_rows > scanptr.p->m_max_batch_size_rows)
  {
    jam();
    /**
     * Extend list...
     */
    if (!seize_acc_ptr_list(scanptr.p, 
                            scanptr.p->m_max_batch_size_rows, max_rows))
    {
      jam();
      tcConnectptr.p->errorCode = ScanFragRef::ZTOO_MANY_ACTIVE_SCAN_ERROR;
      closeScanRequestLab(signal);
      return;
    }
    cbookedAccOps += (max_rows - scanptr.p->m_max_batch_size_rows);
    scanptr.p->m_max_batch_size_rows = max_rows;
  }
  else if (unlikely(max_rows < scanptr.p->m_max_batch_size_rows))
  {
    jam();
    cbookedAccOps -= (scanptr.p->m_max_batch_size_rows - max_rows);
    scanptr.p->m_max_batch_size_rows = max_rows;
  }
  
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
      jam();
      scanptr.p->scanCompletedStatus = ZTRUE;
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
      c_fragment_pool.getPtr(fragptr);

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
    tupScanCloseConfLab(signal);
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

  check_send_scan_hb_rep(signal, scanptr.p, tcConnectptr.p);

  if (scanptr.p->scanReleaseCounter == scanptr.p->m_curr_batch_size_rows) {
    if ((scanptr.p->scanErrorCounter > 0) ||
        (scanptr.p->scanCompletedStatus == ZTRUE)) {
      jam();
      scanptr.p->m_curr_batch_size_rows = 0;
      scanptr.p->m_curr_batch_size_bytes = 0;
      closeScanLab(signal);
    } else if (scanptr.p->m_last_row && !scanptr.p->scanLockHold) {
      jam();
      closeScanLab(signal);
      return;
    } else if (scanptr.p->check_scan_batch_completed() &&
               scanptr.p->scanLockHold != ZTRUE) {
      jam();
      scanptr.p->scanState = ScanRecord::WAIT_SCAN_NEXTREQ;
      sendScanFragConf(signal, ZFALSE);
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
  }
  else if (scanptr.p->scanCompletedStatus != ZTRUE)
  {
    jam();
    /*
    We come here when we have been scanning for a long time and not been able
    to find m_max_batch_size_rows records to return. We needed to release
    the record we didn't want, but now we are returning all found records to
    the API.
    */
    scanptr.p->scanState = ScanRecord::WAIT_SCAN_NEXTREQ;
    sendScanFragConf(signal, ZFALSE);
  }
  else
  {
    jam();
    closeScanLab(signal);
  }
}//Dblqh::scanLockReleasedLab()

bool
Dblqh::seize_acc_ptr_list(ScanRecord* scanP, 
                          Uint32 curr_batch_size, 
                          Uint32 new_batch_size)
{
  /*  1 maps to 0 segments
   * >1 maps to enough segments to store
   */
  Uint32 segments= (new_batch_size + (SectionSegment::DataLength -2 )) / 
    SectionSegment::DataLength;

  if (segments <= scanP->scan_acc_segments)
  {
    // No need to allocate more segments.
    return true;
  }

  if (new_batch_size > 1)
  {
    for (Uint32 i = 1 + scanP->scan_acc_segments; i <= segments; i++)
    {
      Uint32 seg= seizeSegment();
      if (unlikely(seg == RNIL))
      {
        jam();
        /* Cleanup any allocated segments and return */
        scanP->scan_acc_segments= (i-1);
        release_acc_ptr_list(scanP);
        return false;
      }
      scanP->scan_acc_op_ptr[i]= seg;
    }
  }
  scanP->scan_acc_segments= segments;
  return true;
}

void
Dblqh::release_acc_ptr_list(ScanRecord* scanP)
{
  Uint32 i, segments;
  segments= scanP->scan_acc_segments;

  for (i= 1; i <= segments; i++) {
    releaseSection(scanP->scan_acc_op_ptr[i]);
  }
  scanP->scan_acc_segments= 0;
  scanP->scan_acc_index = 0;
}

Uint32
Dblqh::seizeSegment()
{
  Uint32 junk= 0;
  Uint32 iVal= RNIL;
  
  /* Rather grungy way to grab a segment */
  if (!appendToSection(iVal, &junk, 1))
    return RNIL;
  
  return iVal;
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
  jamEntry();

  /* Reassemble if the request was fragmented */
  if (!assembleFragments(signal)){
    jam();
    return;
  }

  ScanFragReq * const scanFragReq = (ScanFragReq *)&signal->theData[0];
  ScanFragRef * ref;
  const Uint32 transid1 = scanFragReq->transId1;
  const Uint32 transid2 = scanFragReq->transId2;
  Uint32 errorCode= 0;
  Uint32 senderData;
  Uint32 hashIndex;
  TcConnectionrecPtr nextHashptr;
  Uint32 senderHi = signal->getSendersBlockRef();
  // bug#13834481 hashHi!=0 caused timeout (tx not found)
  const NodeInfo& senderInfo = getNodeInfo(refToNode(senderHi));
  if (unlikely(senderInfo.m_version < NDBD_LONG_SCANFRAGREQ))
    senderHi = 0;

  /* Short SCANFRAGREQ has no sections, Long SCANFRAGREQ has 1 or 2
   * Section 0 : Mandatory ATTRINFO section
   * Section 1 : Optional KEYINFO section
   */
  const Uint32 numSections= signal->getNoOfSections();
  bool isLongReq= ( numSections != 0 );
  
  SectionHandle handle(this, signal);

  SegmentedSectionPtr attrInfoPtr, keyInfoPtr;
  Uint32 aiLen= 0;
  Uint32 keyLen= 0;

  if (likely(isLongReq))
  {
    /* Long request, get Attr + Key len from section sizes */
    handle.getSection(attrInfoPtr, ScanFragReq::AttrInfoSectionNum);
    aiLen= attrInfoPtr.sz;
    
    if (numSections == 2)
    {
      handle.getSection(keyInfoPtr, ScanFragReq::KeyInfoSectionNum);
      keyLen= keyInfoPtr.sz;
    }
  }
  else
  {
    /* Short request, get Attr + Key len from signal */
    aiLen= ScanFragReq::getAttrLen(scanFragReq->requestInfo);
    keyLen= (scanFragReq->fragmentNoKeyLen >> 16);
    /*
     * bug#13834481.  Clear attribute length so that it is not
     * re-interpreted as new 7.x bits.  initScanrec() uses signal
     * data so we must modify signal data.
     */
    ScanFragReq::clearAttrLen(scanFragReq->requestInfo);
  }

  const Uint32 reqinfo = scanFragReq->requestInfo;
  
  const Uint32 fragId = (scanFragReq->fragmentNoKeyLen & 0xFFFF);
  tabptr.i = scanFragReq->tableId;
  const Uint32 max_rows = scanFragReq->batch_size_rows;
  const Uint32 scanLockMode = ScanFragReq::getLockMode(reqinfo);
  const Uint8 keyinfo = ScanFragReq::getKeyinfoFlag(reqinfo);
  const Uint8 rangeScan = ScanFragReq::getRangeScanFlag(reqinfo);
  
  ptrCheckGuard(tabptr, ctabrecFileSize, tablerec);
  if (tabptr.p->tableStatus != Tablerec::TABLE_DEFINED &&
      tabptr.p->tableStatus != Tablerec::TABLE_READ_ONLY)
  {
    senderData = scanFragReq->senderData;
    goto error_handler_early_1;
  }
  
  if (cfirstfreeTcConrec != RNIL && !ERROR_INSERTED_CLEAR(5055)) {
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
   * A write always has to get keyinfo
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
  
  // XXX adjust cmaxAccOps for range scans and remove this comment
  if ((cbookedAccOps + max_rows) > cmaxAccOps) {
    jam();
    errorCode = ScanFragRef::ZSCAN_BOOK_ACC_OP_ERROR;
    goto error_handler;
  }//if

  if (ScanFragReq::getLcpScanFlag(reqinfo))
  {
    jam();
    ndbrequire(m_reserved_scans.first(scanptr));
    m_reserved_scans.remove(scanptr);
  }
  else if (!c_scanRecordPool.seize(scanptr))
  {
    jam();
    errorCode = ScanFragRef::ZNO_FREE_SCANREC_ERROR;
    goto error_handler;
  }

  initScanTc(scanFragReq,
             transid1,
             transid2,
             fragId,
             ZNIL,
             senderHi);
  tcConnectptr.p->save1 = 0;
  tcConnectptr.p->primKeyLen = keyLen;
  tcConnectptr.p->applRef = scanFragReq->resultRef;
  if (likely(isLongReq))
  {
    tcConnectptr.p->attrInfoIVal= attrInfoPtr.i;
    if (keyLen)
      tcConnectptr.p->keyInfoIVal= keyInfoPtr.i;
    /* Scan state machine is now responsible for freeing 
     * these sections, usually via releaseOprec()
     */
    handle.clear();
  }

  if (ScanFragReq::getCorrFactorFlag(reqinfo))
  {
    /**
     * Correlattion factor for SPJ
     */
    tcConnectptr.p->m_corrFactorLo = scanFragReq->variableData[0];
    tcConnectptr.p->m_corrFactorHi = scanFragReq->variableData[1];
  }

  errorCode = initScanrec(scanFragReq, aiLen);
  if (errorCode != ZOK) {
    jam();
    goto error_handler2;
  }//if
  cbookedAccOps += max_rows;

  /* Check that no equal element already exists */
  ndbassert(findTransaction(tcConnectptr.p->transid[0],
                            tcConnectptr.p->transid[1],
                            tcConnectptr.p->tcOprec,
                            senderHi) == ZNOT_FOUND);
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
    ndbassert(nextHashptr.p->prevHashRec == RNIL);
    nextHashptr.p->prevHashRec = tcConnectptr.i;
  }//if
  if ((! isLongReq ) && 
      ( scanptr.p->scanAiLength > 0 )) {
    jam();
    tcConnectptr.p->transactionState = TcConnectionrec::WAIT_SCAN_AI;
    return;
  }//if
  continueAfterReceivingAllAiLab(signal);
  return;

error_handler2:
  // no scan number allocated
  if (scanptr.p->m_reserved == 0)
  {
    jam();
    c_scanRecordPool.release(scanptr);
  }
  else
  {
    jam();
    m_reserved_scans.add(scanptr);
  }
error_handler:
  ref = (ScanFragRef*)&signal->theData[0];
  tcConnectptr.p->abortState = TcConnectionrec::ABORT_ACTIVE;
  ref->senderData = tcConnectptr.p->clientConnectrec;
  ref->transId1 = transid1;
  ref->transId2 = transid2;
  ref->errorCode = errorCode;
  sendSignal(tcConnectptr.p->clientBlockref, GSN_SCAN_FRAGREF, signal, 
	     ScanFragRef::SignalLength, JBB);
  releaseSections(handle);
  tcConnectptr.p->tcScanRec = RNIL;
  releaseOprec(signal);
  releaseTcrec(signal, tcConnectptr);
  return;

 error_handler_early_1:
  errorCode = get_table_state_error(tabptr);

 error_handler_early:
  releaseSections(handle);
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
  AccScanReq::setStatScanFlag(req->requestInfo, scanptr.p->statScan);

  if (refToMain(tcConnectptr.p->clientBlockref) == BACKUP)
  {
    if (scanptr.p->lcpScan)
    {
      AccScanReq::setNoDiskScanFlag(req->requestInfo, 1);
      AccScanReq::setLcpScanFlag(req->requestInfo, 1);
    }
    else
    {
      /* If backup scan disktables in disk order */
      AccScanReq::setNoDiskScanFlag(req->requestInfo,
                                    !tcConnectptr.p->m_disk_table);
      AccScanReq::setLcpScanFlag(req->requestInfo, 0);
    }
  }
  else
  {
#if BUG_27776_FIXED
    AccScanReq::setNoDiskScanFlag(req->requestInfo,
                                  !tcConnectptr.p->m_disk_table);
#else
    AccScanReq::setNoDiskScanFlag(req->requestInfo, 1);
#endif
    AccScanReq::setLcpScanFlag(req->requestInfo, 0);
  }
  
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
  if (saveAttrInfoInSection(dataPtr, length) == ZOK) {
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

  tcConnectptr.p->errorCode = errcode;
  tupScanCloseConfLab(signal);
  return;
}

/*---------------------------------------------------------------------*/
/* Send this 'I am alive' signal to TC when it is received from ACC    */
/* We include the scanPtr.i that comes from ACC in signalData[1], this */
/* tells TC which fragment record to check for a timeout.              */
/*---------------------------------------------------------------------*/
void
Dblqh::check_send_scan_hb_rep(Signal* signal, 
                              ScanRecord* scanPtrP,
                              TcConnectionrec* tcPtrP)
{
  switch(scanPtrP->scanType){
  case ScanRecord::SCAN:
    break;
  case ScanRecord::COPY:
    return;
#ifdef NDEBUG
  case ScanRecord::ST_IDLE:
  default:
    return;
#else
  case ScanRecord::ST_IDLE:
    ndbrequire(false);
#endif
  }

  Uint64 now = cLqhTimeOutCount;         // measure in 10ms
  Uint64 last = scanPtrP->scanTcWaiting; // last time we reported to TC (10ms)
  Uint64 timeout = cTransactionDeadlockDetectionTimeout; // (ms)
  Uint64 limit = (3*timeout) / 4;

  bool alarm = 
    now >= ((10 * last + limit) / 10) || now < last; // wrap
    
  if (alarm)
  {
    jam();

    scanPtrP->scanTcWaiting = Uint32(now);
    if (tcPtrP->tcTimer != 0)
    {
      tcPtrP->tcTimer = Uint32(now);
    }      

    Uint32 save[3];
    save[0] = signal->theData[0];
    save[1] = signal->theData[1];
    save[2] = signal->theData[2];

    signal->theData[0] = tcPtrP->clientConnectrec;
    signal->theData[1] = tcPtrP->transid[0];
    signal->theData[2] = tcPtrP->transid[1];
    sendSignal(tcPtrP->clientBlockref,
               GSN_SCAN_HBREP, signal, 3, JBB);

    signal->theData[0] = save[0];
    signal->theData[1] = save[1];
    signal->theData[2] = save[2];
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
    /*
     * MRR scan + delete can hit this when the fragment was not
     * initially empty, but has become empty after previous range.
     */
    if (scanptr.p->scanStoredProcId != RNIL) {
      jam();
      scanptr.p->scanCompletedStatus = ZTRUE;
      accScanCloseConfLab(signal);
      return;
    }
    tupScanCloseConfLab(signal);
    return;
  }//if

  check_send_scan_hb_rep(signal, scanptr.p, tcConnectptr.p);

  scanptr.p->scanAccPtr = accScanConf->accPtr;
  if (scanptr.p->rangeScan) {
    jam();
    TuxBoundInfo* req = (TuxBoundInfo*)signal->getDataPtrSend();
    req->errorCode = RNIL;
    req->tuxScanPtrI = scanptr.p->scanAccPtr;
    // TODO : Any improvements to be made on this block boundary? 
    Uint32 len = req->boundAiLength = copyNextRange(req->data, tcConnectptr.p);
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
    /* Send AttrInfo to TUP to store as 'stored procedure'
     * and get storedProcId back for future reference
     */
    signal->theData[0] = tcConnectptr.p->tupConnectrec;
    signal->theData[1] = tcConnectptr.p->tableref;
    signal->theData[2] = scanptr.p->scanSchemaVersion;
    signal->theData[3] = ZSTORED_PROC_SCAN;
// theData[4] is not used
    signal->theData[5] = scanptr.p->scanApiBlockref;
    
    /* Pass ATTRINFO as long section, we don't need
     * it after this
     */
    SectionHandle handle(this);
    handle.m_ptr[0].i= tcConnectptr.p->attrInfoIVal;
    handle.m_cnt= 1;
    tcConnectptr.p->attrInfoIVal= RNIL;
    getSections(handle.m_cnt, handle.m_ptr);

    sendSignal(tcConnectptr.p->tcTupBlockref,
	       GSN_STORED_PROCREQ, signal, 6, JBB,
               &handle);
  } 
  else 
  {
    /* TUP already has the Stored procedure, continue */
    jam();
    storedProcConfScanLab(signal);
  }
}//Dblqh::accScanConfScanLab()

Uint32
Dblqh::copyNextRange(Uint32 * dst, TcConnectionrec* tcPtrP)
{
  /**
   * Copy the bound info for the next range from the KeyInfo
   * to *dst
   * There may be zero or more bounds
   * A SectionReader is used to read bound information, its
   * position is saved between calls
   * This method also extracts range numbers from the
   * KeyInfo
   */
  Uint32 totalLen = tcPtrP->primKeyLen;
  if (totalLen == 0)
  {
    return 0;
  }

  Uint32 * save = dst;
  do
  {
    ndbassert( tcPtrP->keyInfoIVal != RNIL );
    SectionReader keyInfoReader(tcPtrP->keyInfoIVal,
                                g_sectionSegmentPool);

    if (tcPtrP->m_flags & TcConnectionrec::OP_SCANKEYINFOPOSSAVED)
    {
      /* Second or higher range in an MRR scan
       * Restore SectionReader to the last position it was in
       */
      bool ok= keyInfoReader.setPos(tcPtrP->scanKeyInfoPos);
      ndbrequire(ok);
    }

    /* Get first word of next range and extract range
     * length, number from it.
     * For non MRR, these will be zero.
     */
    Uint32 firstWord;
    ndbrequire( keyInfoReader.getWord(&firstWord) );
    const Uint32 rangeLen= (firstWord >> 16) ? (firstWord >> 16) : totalLen;
    Uint32 range_no = (firstWord & 0xFFF0) >> 4;
    tcPtrP->m_scan_curr_range_no= range_no;
    tcPtrP->m_corrFactorLo &= 0x0000FFFF;
    tcPtrP->m_corrFactorLo |= (range_no << 16);
    firstWord &= 0xF; // Remove length+range num from first word

    /* Write range info to dst */
    *(dst++)= firstWord;
    bool ok= keyInfoReader.getWords(dst, rangeLen - 1);
    ndbassert(ok);
    if (unlikely(!ok))
      break;

    if (ERROR_INSERTED(5074))
      break;

    tcPtrP->primKeyLen-= rangeLen;

    if (rangeLen == totalLen)
    {
      /* All range information has been copied, free the section */
      releaseSection(tcPtrP->keyInfoIVal);
      tcPtrP->keyInfoIVal= RNIL;
    }
    else
    {
      /* Save position of SectionReader for next range (if any) */
      tcPtrP->scanKeyInfoPos= keyInfoReader.getPos();
      tcPtrP->m_flags|= TcConnectionrec::OP_SCANKEYINFOPOSSAVED;
    }

    return rangeLen;
  } while (0);

  /**
   * We enter here if there was some error in the keyinfo
   *   this has (once) been seen in customer lab,
   *   never at in the wild, and never in internal lab.
   *   root-cause unknown, maybe ndbapi application bug
   *
   * Crash in debug, or ERROR_INSERT (unless 5074)
   * else
   *   generate an incorrect bound...that will make TUX abort the scan
   */
#ifdef ERROR_INSERT
  ndbrequire(ERROR_INSERTED_CLEAR(5074));
#else
  ndbassert(false);
#endif

  * save = TuxBoundInfo::InvalidBound;
  return 1;
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
  c_fragment_pool.getPtr(fragptr);
  if (scanptr.p->scanCompletedStatus == ZTRUE) {
    jam();
    // STOP THE SCAN PROCESS IF THIS HAS BEEN REQUESTED.
    closeScanLab(signal);
    return;
  }//if
  switch (fragptr.p->fragStatus) {
  case Fragrecord::FSACTIVE:
    jam();
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
    jamLine(fragptr.p->fragStatus);
    ndbout_c("fragptr.p->fragStatus: %u",
             fragptr.p->fragStatus);
    // wl4391_todo SR 2-node CRASH_RECOVERING from BACKUP
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

  c_fragment_pool.getPtr(fragptr);
  if (signal->theData[1] == ZTRUE) {
    jam();
    signal->theData[0] = ZCHECK_LCP_STOP_BLOCKED;
    signal->theData[1] = scanptr.i;
    sendSignalWithDelay(cownref, GSN_CONTINUEB, signal, 10, 2);
    signal->theData[0] = RNIL;
    return;
  }//if
  if (fragptr.p->fragStatus != Fragrecord::FSACTIVE) {
    ndbrequire(fragptr.p->fragStatus == Fragrecord::BLOCKED); 
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
  EXECUTE_DIRECT(refToMain(scanptr.p->scanBlockref), GSN_ACC_CHECK_SCAN,
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

      if((tcConnectptr.p->primKeyLen) == 0)
	scanptr.p->scanCompletedStatus = ZTRUE;
      
      scanptr.p->scanState = ScanRecord::WAIT_SCAN_NEXTREQ;
      sendScanFragConf(signal, ZFALSE);
      return;
    }//if
    closeScanLab(signal);
    return;
  }//if

  // If accOperationPtr == RNIL no record was returned by ACC
  Uint32 accOpPtr = nextScanConf->accOperationPtr;
  if (accOpPtr == RNIL) 
  {
    jam();
    /*************************************************************
     *       STOP THE SCAN PROCESS IF THIS HAS BEEN REQUESTED.
     ************************************************************ */    
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
  check_send_scan_hb_rep(signal, scanptr.p, tcConnectptr.p);
  set_acc_ptr_in_scan_record(scanptr.p,
                             scanptr.p->m_curr_batch_size_rows,
                             accOpPtr);

  jam();
  nextScanConfLoopLab(signal);
}//Dblqh::nextScanConfScanLab()

void Dblqh::nextScanConfLoopLab(Signal* signal) 
{
  /* ----------------------------------------------------------------------
   *       STOP THE SCAN PROCESS IF THIS HAS BEEN REQUESTED.
   * ---------------------------------------------------------------------- */
  if (scanptr.p->scanCompletedStatus == ZTRUE) {
    jam();
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

  Fragrecord* fragPtrP= fragptr.p;
  if (scanptr.p->rangeScan) {
    jam();
    // for ordered index use primary table
    fragPtrP= c_fragment_pool.getPtr(fragPtrP->tableFragptr);
  }

  tcConnectptr.p->transactionState = TcConnectionrec::SCAN_TUPKEY;
  if(tcConnectptr.p->m_disk_table)
  {
    next_scanconf_load_diskpage(signal, scanptr, tcConnectptr,fragPtrP);
  }
  else
  {
    next_scanconf_tupkeyreq(signal, scanptr, tcConnectptr.p, fragPtrP, RNIL);
  }
}

void
Dblqh::next_scanconf_load_diskpage(Signal* signal, 
				   ScanRecordPtr scanPtr,
				   Ptr<TcConnectionrec> regTcPtr,
				   Fragrecord* fragPtrP)
{
  jam();

  int res;

  if((res= c_tup->load_diskpage_scan(signal,
				     regTcPtr.p->tupConnectrec,
				     fragPtrP->tupFragptr,
				     scanPtr.p->m_row_id.m_page_no,
				     scanPtr.p->m_row_id.m_page_idx,
				     0)) > 0)
  {
    next_scanconf_tupkeyreq(signal, scanptr, regTcPtr.p, fragPtrP, res);
  }
  else if(unlikely(res != 0))
  {
    jam();
    TupKeyRef * ref = (TupKeyRef *)signal->getDataPtr();
    ref->userRef= regTcPtr.i;
    ref->errorCode= ~0;
    execTUPKEYREF(signal);
  }
}

void
Dblqh::next_scanconf_load_diskpage_callback(Signal* signal, 
					    Uint32 callbackData,
					    Uint32 disk_page)
{
  jamEntry();

  Ptr<TcConnectionrec> regTcPtr;
  regTcPtr.i= callbackData;
  ptrCheckGuard(regTcPtr, ctcConnectrecFileSize, tcConnectionrec);
  
  ScanRecordPtr scanPtr;
  c_scanRecordPool.getPtr(scanPtr, regTcPtr.p->tcScanRec);

  if(disk_page > 0)
  {
    FragrecordPtr fragPtr;
    c_fragment_pool.getPtr(fragPtr, regTcPtr.p->fragmentptr);

    if (scanPtr.p->rangeScan) {
      jam();
      // for ordered index use primary table
      fragPtr.p = c_fragment_pool.getPtr(fragPtr.p->tableFragptr);
    }
    
    next_scanconf_tupkeyreq(signal, scanPtr, regTcPtr.p, fragPtr.p, disk_page);
  }
  else
  {
    TupKeyRef * ref = (TupKeyRef *)signal->getDataPtr();
    ref->userRef= callbackData;
    ref->errorCode= disk_page;
    execTUPKEYREF(signal);
  }
}

void
Dblqh::next_scanconf_tupkeyreq(Signal* signal, 
			       Ptr<ScanRecord> scanPtr,
			       TcConnectionrec * regTcPtr,
			       Fragrecord* fragPtrP,
			       Uint32 disk_page)
{
  jam();
  Uint32 reqinfo = (scanPtr.p->scanLockHold == ZFALSE);
  reqinfo = reqinfo + (regTcPtr->operation << 6);
  reqinfo = reqinfo + (regTcPtr->opExec << 10);
  reqinfo = reqinfo + (regTcPtr->m_reorg << 12);

  TupKeyReq * const tupKeyReq = (TupKeyReq *)signal->getDataPtrSend(); 
  
  tupKeyReq->connectPtr = regTcPtr->tupConnectrec;
  tupKeyReq->request = reqinfo;
  tupKeyReq->keyRef1 = scanPtr.p->m_row_id.m_page_no;
  tupKeyReq->keyRef2 = scanPtr.p->m_row_id.m_page_idx;
  tupKeyReq->attrBufLen = 0;
  tupKeyReq->opRef = scanPtr.p->scanApiOpPtr; 
  tupKeyReq->applRef = scanPtr.p->scanApiBlockref;
  tupKeyReq->storedProcedure = scanPtr.p->scanStoredProcId;
  tupKeyReq->transId1 = regTcPtr->transid[0];
  tupKeyReq->transId2 = regTcPtr->transid[1];
  tupKeyReq->fragPtr = fragPtrP->tupFragptr;
  tupKeyReq->primaryReplica = (regTcPtr->seqNoReplica == 0)?true:false;
  tupKeyReq->coordinatorTC = regTcPtr->tcBlockref;
  tupKeyReq->tcOpIndex = regTcPtr->tcOprec;
  tupKeyReq->savePointId = regTcPtr->savePointId;
  tupKeyReq->disk_page= disk_page;
  /* No AttrInfo sent to TUP, it uses a stored procedure */
  tupKeyReq->attrInfoIVal= RNIL;
  Uint32 blockNo = refToMain(regTcPtr->tcTupBlockref);
  EXECUTE_DIRECT(blockNo, GSN_TUPKEYREQ, signal, 
		 TupKeyReq::SignalLength);
}

/* -------------------------------------------------------------------------
 *       STORE KEYINFO IN A LONG SECTION PRIOR TO SENDING
 * -------------------------------------------------------------------------
 *       PRECONDITION:   SCAN_STATE = WAIT_SCAN_KEYINFO
 * ------------------------------------------------------------------------- */
bool 
Dblqh::keyinfoLab(const Uint32 * src, Uint32 len) 
{
  ndbassert( tcConnectptr.p->keyInfoIVal == RNIL );
  ndbassert( len > 0 );

  if (ERROR_INSERTED(5052) || ERROR_INSERTED_CLEAR(5060))
    return false;

  return(appendToSection(tcConnectptr.p->keyInfoIVal,
                         src,
                         len));
}//Dblqh::keyinfoLab()

Uint32
Dblqh::readPrimaryKeys(ScanRecord *scanP, TcConnectionrec *tcConP, Uint32 *dst)
{
  Uint32 tableId = tcConP->tableref;
  Uint32 fragId = tcConP->fragmentid;
  Uint32 fragPageId = scanP->m_row_id.m_page_no;
  Uint32 pageIndex = scanP->m_row_id.m_page_idx;

  if(scanP->rangeScan)
  {
    jam();
    // for ordered index use primary table
    FragrecordPtr tFragPtr;
    tFragPtr.i = fragptr.p->tableFragptr;
    c_fragment_pool.getPtr(tFragPtr);
    tableId = tFragPtr.p->tabRef;
  }

  int ret = c_tup->accReadPk(tableId, fragId, fragPageId, pageIndex, dst, false);
  jamEntry();
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
  c_scanRecordPool.getPtr(scanptr);

  Uint32 rows = scanptr.p->m_curr_batch_size_rows;
  Uint32 accOpPtr= get_acc_ptr_from_scan_record(scanptr.p, rows, false);
  if (accOpPtr != (Uint32)-1)
  {
    c_acc->execACCKEY_ORD(signal, accOpPtr);
    jamEntry();
  }
  else
  {
    ndbassert(refToBlock(scanptr.p->scanBlockref) != DBACC);
  }
  
  if (scanptr.p->scanCompletedStatus == ZTRUE) {
    /* ---------------------------------------------------------------------
     *       STOP THE SCAN PROCESS IF THIS HAS BEEN REQUESTED.
     * --------------------------------------------------------------------- */
    if ((scanptr.p->scanLockHold == ZTRUE) && rows)
    {
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
  scanptr.p->m_curr_batch_size_bytes+= tdata4 * sizeof(Uint32);
  scanptr.p->m_curr_batch_size_rows = rows + 1;
  scanptr.p->m_last_row = tdata5;

  const NodeBitmask& all = globalTransporterRegistry.get_status_slowdown();
  if (unlikely(!all.isclear()))
  {
    if (all.get(refToNode(scanptr.p->scanApiBlockref)))
    {
      /**
       * End scan batch if transporter-buffer are in slowdown state
       *
       * TODO: We should have counters for this...
       */
      scanptr.p->m_stop_batch = 1;
    }
  }

  if (scanptr.p->check_scan_batch_completed() | tdata5){
    if (scanptr.p->scanLockHold == ZTRUE) {
      jam();
      scanptr.p->scanState = ScanRecord::WAIT_SCAN_NEXTREQ;
      sendScanFragConf(signal, ZFALSE);
      return;
    } else {
      jam();
      scanptr.p->scanReleaseCounter = rows + 1;
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
  c_scanRecordPool.getPtr(scanptr);

  Uint32 rows = scanptr.p->m_curr_batch_size_rows;
  Uint32 accOpPtr= get_acc_ptr_from_scan_record(scanptr.p, rows, false);
  if (accOpPtr != (Uint32)-1)
  {
    c_acc->execACCKEY_ORD(signal, accOpPtr);
  }
  else
  {
    ndbassert(refToBlock(scanptr.p->scanBlockref) != DBACC);
  }

  if (scanptr.p->scanCompletedStatus == ZTRUE) {
    /* ---------------------------------------------------------------------
     *       STOP THE SCAN PROCESS IF THIS HAS BEEN REQUESTED.
     * --------------------------------------------------------------------- */
    if ((scanptr.p->scanLockHold == ZTRUE) && rows)
    {
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
      scanptr.p->m_curr_batch_size_rows = rows + 1;
      scanptr.p->scanReleaseCounter = rows + 1;
    }//if
    /* --------------------------------------------------------------------
     *       WE NEED TO RELEASE ALL LOCKS CURRENTLY
     *       HELD BY THIS SCAN.
     * -------------------------------------------------------------------- */ 
    scanReleaseLocksLab(signal);
    return;
  }//if
  Uint32 time_passed= cLqhTimeOutCount - tcConnectptr.p->tcTimer;
  if (rows)
  {
    if (time_passed > 1) 
    {
  /* -----------------------------------------------------------------------
   *  WE NEED TO ENSURE THAT WE DO NOT SEARCH FOR THE NEXT TUPLE FOR A 
   *  LONG TIME WHILE WE KEEP A LOCK ON A FOUND TUPLE. WE RATHER REPORT 
   *  THE FOUND TUPLE IF FOUND TUPLES ARE RARE. If more than 10 ms passed we
   *  send the found tuples to the API.
   * ----------------------------------------------------------------------- */
      scanptr.p->scanReleaseCounter = rows + 1;
      scanReleaseLocksLab(signal);
      return;
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
  c_fragment_pool.getPtr(fragptr);
  switch (fragptr.p->fragStatus) {
  case Fragrecord::FSACTIVE:
    jam();
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

  /* Do we have another range to scan? */
  if((tcConnectptr.p->primKeyLen > 0) && 
     (scanptr.p->scanCompletedStatus != ZTRUE))
  {
    jam();
    /* Start next range scan...*/
    continueAfterReceivingAllAiLab(signal);
    return;
  }
  
  scanptr.p->scanState = ScanRecord::WAIT_DELETE_STORED_PROC_ID_SCAN;
  signal->theData[0] = tcConnectptr.p->tupConnectrec;
  signal->theData[1] = tcConnectptr.p->tableref;
  signal->theData[2] = scanptr.p->scanSchemaVersion;
  signal->theData[3] = ZDELETE_STORED_PROC_ID;
  signal->theData[4] = scanptr.p->scanStoredProcId;
  signal->theData[5] = scanptr.p->scanApiBlockref;
  sendSignal(tcConnectptr.p->tcTupBlockref,
             GSN_STORED_PROCREQ, signal, 6, JBB);
}//Dblqh::accScanCloseConfLab()

/* -------------------------------------------------------------------------
 *       ENTER STORED_PROCCONF WITH
 * -------------------------------------------------------------------------
 * PRECONDITION: SCAN_STATE = WAIT_DELETE_STORED_PROC_ID_SCAN
 * ------------------------------------------------------------------------- */
void Dblqh::tupScanCloseConfLab(Signal* signal) 
{
  fragptr.i = tcConnectptr.p->fragmentptr;
  c_fragment_pool.getPtr(fragptr);
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
Uint32 Dblqh::initScanrec(const ScanFragReq* scanFragReq,
                          Uint32 aiLen)
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
  Uint32 tupScan = ScanFragReq::getTupScanFlag(reqinfo);
  const Uint32 scanPrio = ScanFragReq::getScanPrio(reqinfo);
  const Uint32 accScan = (rangeScan == 0) && (tupScan == 0);

  scanptr.p->scanKeyinfoFlag = keyinfo;
  scanptr.p->scanLockHold = scanLockHold;
  scanptr.p->scanCompletedStatus = ZFALSE;
  scanptr.p->scanType = ScanRecord::SCAN;
  scanptr.p->scanApiBlockref = scanFragReq->resultRef;
  scanptr.p->scanAiLength = aiLen;
  scanptr.p->scanTcrec = tcConnectptr.i;
  scanptr.p->scanSchemaVersion = scanFragReq->schemaVersion;

  scanptr.p->m_stop_batch = 0;
  scanptr.p->m_curr_batch_size_rows = 0;
  scanptr.p->m_curr_batch_size_bytes= 0;
  scanptr.p->m_max_batch_size_rows = max_rows;
  scanptr.p->m_max_batch_size_bytes = max_bytes;

  if (accScan)
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
  scanptr.p->lcpScan = ScanFragReq::getLcpScanFlag(reqinfo);
  scanptr.p->statScan = ScanFragReq::getStatScanFlag(reqinfo);
  scanptr.p->scanState = ScanRecord::SCAN_FREE;
  scanptr.p->scanFlag = ZFALSE;
  scanptr.p->m_row_id.setNull();
  scanptr.p->scanTcWaiting = cLqhTimeOutCount;
  scanptr.p->scanNumber = ~0;
  scanptr.p->scanApiOpPtr = scanFragReq->clientOpPtr;
  scanptr.p->m_last_row = 0;
  scanptr.p->scanStoredProcId = RNIL;
  scanptr.p->copyPtr = RNIL;
  if (max_rows == 0 || (max_bytes > 0 && max_rows > max_bytes)){
    jam();
    return ScanFragRef::ZWRONG_BATCH_SIZE;
  }

  if (ERROR_INSERTED(5057))
  {
    CLEAR_ERROR_INSERT_VALUE;
    return ScanFragRef::ZTOO_MANY_ACTIVE_SCAN_ERROR;
  }

  scanptr.p->scan_acc_segments = 0;
  if (!seize_acc_ptr_list(scanptr.p, 0, max_rows)){
    jam();
    return ScanFragRef::ZTOO_MANY_ACTIVE_SCAN_ERROR;
  }
  init_acc_ptr_list(scanptr.p);

  /**
   * Used for scan take over
   */
  FragrecordPtr tFragPtr;
  tFragPtr.i = fragptr.p->tableFragptr;
  c_fragment_pool.getPtr(tFragPtr);
  scanptr.p->fragPtrI = fragptr.p->tableFragptr;
  
  /**
   * !idx uses 1 - (MAX_PARALLEL_SCANS_PER_FRAG - 1)  =  1-11
   *  idx uses from MAX_PARALLEL_SCANS_PER_FRAG - MAX = 12-42)
   */

  /**
   * ACC only supports 12 parallel scans per fragment (hard limit)
   * TUP/TUX does not have any such limit...but when scanning with keyinfo
   *         (for take-over) no more than 255 such scans can be active
   *         at a fragment (dur to 8 bit number in scan-keyinfo protocol)
   *
   * TODO: Make TUP/TUX limits depend on scanKeyinfoFlag (possibly with
   *       other config limit too)
   */

  Uint32 start, stop;
  Uint32 max_parallel_scans_per_frag = c_max_parallel_scans_per_frag;
  if (accScan)
  {
    start = 1;
    stop = MAX_PARALLEL_SCANS_PER_FRAG - 1;
  }
  else if (rangeScan)
  {
    start = MAX_PARALLEL_SCANS_PER_FRAG;
    stop = start + max_parallel_scans_per_frag - 1;
  }
  else
  {
    ndbassert(tupScan);
    start = MAX_PARALLEL_SCANS_PER_FRAG + max_parallel_scans_per_frag;
    stop = start + max_parallel_scans_per_frag - 1;
  }
  ndbrequire((start < 32 * tFragPtr.p->m_scanNumberMask.Size) &&
             (stop < 32 * tFragPtr.p->m_scanNumberMask.Size));
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
				      tupScan == 0 ? 
                                      fragptr.p->m_queuedScans :
                                      fragptr.p->m_queuedTupScans);
    queue.add(scanptr);
    return ZOK;
  }
  
  scanptr.p->scanNumber = free;
  tFragPtr.p->m_scanNumberMask.clear(free);// Update mask  
  
  LocalDLList<ScanRecord> active(c_scanRecordPool, fragptr.p->m_activeScans);
  active.add(scanptr);
  if(scanptr.p->scanKeyinfoFlag){
    jam();
#if defined VM_TRACE || defined ERROR_INSERT
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
  return ZOK;
}

/* =========================================================================
 * =======             INITIATE TC RECORD AT SCAN                    =======
 *
 *       SUBROUTINE SHORT NAME = IST
 * ========================================================================= */
void Dblqh::initScanTc(const ScanFragReq* req,
                       Uint32 transid1,
                       Uint32 transid2,
                       Uint32 fragId,
                       Uint32 nodeId,
                       Uint32 hashHi)
{
  tcConnectptr.p->transid[0] = transid1;
  tcConnectptr.p->transid[1] = transid2;
  tcConnectptr.p->tcScanRec = scanptr.i;
  tcConnectptr.p->tableref = tabptr.i;
  tcConnectptr.p->fragmentid = fragId;
  tcConnectptr.p->fragmentptr = fragptr.i;
  tcConnectptr.p->tcOprec = tcConnectptr.p->clientConnectrec;
  tcConnectptr.p->tcHashKeyHi = hashHi;
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
  tcConnectptr.p->m_scan_curr_range_no = 0;
  tcConnectptr.p->m_dealloc = 0;
  tcConnectptr.p->activeCreat = Fragrecord::AC_NORMAL;
  // set TcConnectionrec::OP_SAVEATTRINFO so that a
  // "old" scan (short signals) update currTupAiLen which is checked
  // in scanAttrinfoLab
  tcConnectptr.p->m_flags = TcConnectionrec::OP_SAVEATTRINFO;
  TablerecPtr tTablePtr;
  tTablePtr.i = tabptr.p->primaryTableId;
  ptrCheckGuard(tTablePtr, ctabrecFileSize, tablerec);
  tcConnectptr.p->m_disk_table = tTablePtr.p->m_disk_table &&
    (!req || !ScanFragReq::getNoDiskFlag(req->requestInfo));  
  tcConnectptr.p->m_reorg = 
    req ? ScanFragReq::getReorgFlag(req->requestInfo) : 0;

  tabptr.p->usageCountR++;
}//Dblqh::initScanTc()

/* ========================================================================= 
 * =======                       FINISH  SCAN RECORD                 ======= 
 * 
 *       REMOVE SCAN RECORD FROM PER FRAGMENT LIST.
 * ========================================================================= */
void Dblqh::finishScanrec(Signal* signal)
{
  release_acc_ptr_list(scanptr.p);

  Uint32 tupScan = scanptr.p->tupScan;
  LocalDLFifoList<ScanRecord> queue(c_scanRecordPool,
                                    tupScan == 0 ? 
                                    fragptr.p->m_queuedScans :
                                    fragptr.p->m_queuedTupScans);
  
  if (scanptr.p->scanState == ScanRecord::IN_QUEUE)
  {
    jam();
    if (scanptr.p->m_reserved == 0)
    {
      jam();
      queue.release(scanptr);
    }
    else
    {
      jam();
      queue.remove(scanptr);
      m_reserved_scans.add(scanptr);
    }

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
  if (scanptr.p->m_reserved == 0)
  {
    jam();
    scans.release(scanptr);
  }
  else
  {
    jam();
    scans.remove(scanptr);
    m_reserved_scans.add(scanptr);
  }
  
  FragrecordPtr tFragPtr;
  tFragPtr.i = scanptr.p->fragPtrI;
  c_fragment_pool.getPtr(tFragPtr);

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
#if defined VM_TRACE || defined ERROR_INSERT
    ScanRecordPtr tmp;
    ndbrequire(!c_scanTakeOverHash.find(tmp, * restart.p));
#endif
    c_scanTakeOverHash.add(restart);
#ifdef TRACE_SCAN_TAKEOVER
    ndbout_c("adding-r (%d %d)", restart.p->scanNumber, restart.p->fragPtrI);
#endif
  }

  /**
   * This state is a bit weird, but that what set in initScanRec
   */
  restart.p->scanState = ScanRecord::SCAN_FREE;
  if(tcConnectptr.p->transactionState == TcConnectionrec::SCAN_STATE_USED)
  {
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
  scanptr.p->scanTcWaiting = 0;
  cbookedAccOps -= scanptr.p->m_max_batch_size_rows;
}//Dblqh::releaseScanrec()

/* ------------------------------------------------------------------------
 * -------              SEND KEYINFO20 TO API                       ------- 
 *
 * Return: Length in number of Uint32 words
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
  Uint32 nodeId = refToNode(ref);
  const bool connectedToNode = getNodeInfo(nodeId).m_connected;
#ifdef NOT_USED
  const Uint32 type = getNodeInfo(nodeId).m_type;
  const bool is_api= (type >= NodeInfo::API && type <= NodeInfo::REP);
  const bool old_dest= (getNodeInfo(nodeId).m_version < MAKE_VERSION(3,5,0));
#endif
  const bool longable = true; // TODO is_api && !old_dest;

  if (isNdbMtLqh())
  {
    jam();
    nodeId = 0; // prevent execute direct
  }

  Uint32 * dst = keyInfo->keyData;
  dst += nodeId == getOwnNodeId() ? 0 : KeyInfo20::DataLength;

  /**
   * This is ugly :-(
   *  currently only SUMA receives KEYINFO20 inside kernel..
   *  and it's not really interested in the actual keyinfo,
   *  only the scanInfo_Node...so send only that and avoid 
   *  messing with if's below...
   */
  Uint32 keyLen ;
  if (refToMain(ref) == SUMA && nodeId == getOwnNodeId())
  {
    keyLen = 0;
  }
  else
  {
    keyLen = readPrimaryKeys(scanP, tcConP, dst);
  }

  Uint32 fragId = tcConP->fragmentid;
  keyInfo->clientOpPtr   = scanP->scanApiOpPtr;
  keyInfo->keyLen        = keyLen;
  keyInfo->scanInfo_Node = 
    KeyInfo20::setScanInfo(scanOp, scanP->scanNumber) + (fragId << 20);
  keyInfo->transId1 = tcConP->transid[0];
  keyInfo->transId2 = tcConP->transid[1];
  
  Uint32 * src = signal->theData+25;
  if(connectedToNode)
  {
    jam();
    
    if (nodeId == getOwnNodeId())
    {
      EXECUTE_DIRECT(refToBlock(ref), GSN_KEYINFO20, signal,
                     KeyInfo20::HeaderLength + keyLen);
      jamEntry();
      return keyLen;
    }
    else
    {
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
  Uint32 total_len= scanptr.p->m_curr_batch_size_bytes / sizeof(Uint32);
  ndbassert((scanptr.p->m_curr_batch_size_bytes % sizeof(Uint32)) == 0);

  scanptr.p->scanTcWaiting = 0;

  if(ERROR_INSERTED(5037)){
    CLEAR_ERROR_INSERT_VALUE;
    return;
  }
  ScanFragConf * conf = (ScanFragConf*)&signal->theData[0];
#ifdef NOT_USED
  NodeId tc_node_id= refToNode(tcConnectptr.p->clientBlockref);
#endif
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

  scanptr.p->m_stop_batch = 0;
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

/**
 * PREPARE COPY FRAG REQ
 */
void
Dblqh::execPREPARE_COPY_FRAG_REQ(Signal* signal)
{
  jamEntry();
  PrepareCopyFragReq req = *(PrepareCopyFragReq*)signal->getDataPtr();

  CRASH_INSERTION(5045);

  tabptr.i = req.tableId;
  ptrCheckGuard(tabptr, ctabrecFileSize, tablerec);

  Uint32 max_page = RNIL;
  
  if (getOwnNodeId() != req.startingNodeId)
  {
    jam();
    /**
     * This is currently dead code...
     *   but is provided so we can impl. a better scan+delete on
     *   starting node wo/ having to change running node
     */
    ndbrequire(getOwnNodeId() == req.copyNodeId);
    c_tup->get_frag_info(req.tableId, req.fragId, &max_page);    

    PrepareCopyFragConf* conf = (PrepareCopyFragConf*)signal->getDataPtrSend();
    conf->senderData = req.senderData;
    conf->senderRef = reference();
    conf->tableId = req.tableId;
    conf->fragId = req.fragId;
    conf->copyNodeId = req.copyNodeId;
    conf->startingNodeId = req.startingNodeId;
    conf->maxPageNo = max_page;
    sendSignal(req.senderRef, GSN_PREPARE_COPY_FRAG_CONF,
               signal, PrepareCopyFragConf::SignalLength, JBB);  
    
    return;
  }
  
  if (! DictTabInfo::isOrderedIndex(tabptr.p->tableType))
  {
    jam();
    ndbrequire(getFragmentrec(signal, req.fragId));
    
    /**
     *
     */
    fragptr.p->m_copy_started_state = Fragrecord::AC_IGNORED;
    fragptr.p->fragStatus = Fragrecord::ACTIVE_CREATION;
    fragptr.p->logFlag = Fragrecord::STATE_FALSE;

    c_tup->get_frag_info(req.tableId, req.fragId, &max_page);
  }    
    
  PrepareCopyFragConf* conf = (PrepareCopyFragConf*)signal->getDataPtrSend();
  conf->senderData = req.senderData;
  conf->senderRef = reference();
  conf->tableId = req.tableId;
  conf->fragId = req.fragId;
  conf->copyNodeId = req.copyNodeId;
  conf->startingNodeId = req.startingNodeId;
  conf->maxPageNo = max_page;
  sendSignal(req.senderRef, GSN_PREPARE_COPY_FRAG_CONF,
             signal, PrepareCopyFragConf::SignalLength, JBB);  
}

/* *************************************** */
/*  COPY_FRAGREQ: Start copying a fragment */
/* *************************************** */
void Dblqh::execCOPY_FRAGREQ(Signal* signal) 
{
  jamEntry();
  const CopyFragReq * const copyFragReq = (CopyFragReq *)&signal->theData[0];
  tabptr.i = copyFragReq->tableId;
  ptrCheckGuard(tabptr, ctabrecFileSize, tablerec);
  Uint32 i;
  const Uint32 fragId = copyFragReq->fragId;
  const Uint32 copyPtr = copyFragReq->userPtr;
  const Uint32 userRef = copyFragReq->userRef;
  const Uint32 nodeId = copyFragReq->nodeId;
  const Uint32 gci = copyFragReq->gci;
  
  ndbrequire(cnoActiveCopy < 3);
  ndbrequire(getFragmentrec(signal, fragId));
  ndbrequire(fragptr.p->copyFragState == ZIDLE);
  ndbrequire(cfirstfreeTcConrec != RNIL);
  ndbrequire(fragptr.p->m_scanNumberMask.get(NR_ScanNo));

  Uint32 checkversion = NDB_VERSION >= MAKE_VERSION(5,1,0) ?
    NDBD_UPDATE_FRAG_DIST_KEY_51 :  NDBD_UPDATE_FRAG_DIST_KEY_50;
  
  Uint32 nodeCount = copyFragReq->nodeCount;
  NdbNodeBitmask nodemask;
  if (getNodeInfo(refToNode(userRef)).m_version >= checkversion)
  {
    ndbrequire(nodeCount <= MAX_REPLICAS);
    for (i = 0; i<nodeCount; i++)
      nodemask.set(copyFragReq->nodeList[i]);
  }
  Uint32 maxPage = copyFragReq->nodeList[nodeCount];
  Uint32 version = getNodeInfo(refToNode(userRef)).m_version;
  Uint32 requestInfo = copyFragReq->nodeList[nodeCount + 1];
  if (ndb_check_prep_copy_frag_version(version) < 2)
  {
    jam();
    maxPage = RNIL;
  }

  if (signal->getLength() < CopyFragReq::SignalLength + nodeCount)
  {
    jam();
    requestInfo = CopyFragReq::CFR_TRANSACTIONAL;
  }

  if (requestInfo == CopyFragReq::CFR_NON_TRANSACTIONAL)
  {
    jam();
  }
  else
  {
    fragptr.p->fragDistributionKey = copyFragReq->distributionKey;
  }
  Uint32 key = fragptr.p->fragDistributionKey;

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
  ndbrequire(m_reserved_scans.first(scanptr));
  m_reserved_scans.remove(scanptr);
  scans.add(scanptr);

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
  tcConnectptr.p->clientBlockref = userRef;
  
  /**
   * Remove implicit cast/usage of CopyFragReq
   */
  //initCopyrec(signal);
  scanptr.p->copyPtr = copyPtr;
  scanptr.p->scanType = ScanRecord::COPY;
  scanptr.p->scanNodeId = nodeId;
  scanptr.p->scanTcrec = tcConnectptr.i;
  scanptr.p->scanSchemaVersion = copyFragReq->schemaVersion;
  scanptr.p->scanCompletedStatus = ZFALSE;
  scanptr.p->scanErrorCounter = 0;
  scanptr.p->scanNumber = NR_ScanNo;
  scanptr.p->scanKeyinfoFlag = 0; // Don't put into hash
  scanptr.p->fragPtrI = fragptr.i;
  scanptr.p->scanApiOpPtr = tcConnectptr.i;
  scanptr.p->scanApiBlockref = reference();
  fragptr.p->m_scanNumberMask.clear(NR_ScanNo);
  scanptr.p->scanBlockref = ctupBlockref;
  scanptr.p->scanLockHold = ZFALSE;
  scanptr.p->m_curr_batch_size_rows = 0;
  scanptr.p->m_curr_batch_size_bytes= 0;
  scanptr.p->readCommitted = 0;
  
  initScanTc(0,
             0,
             (DBLQH << 20) + (cownNodeid << 8),
             fragId,
             copyFragReq->nodeId,
             0);
  cactiveCopy[cnoActiveCopy] = fragptr.i;
  cnoActiveCopy++;

  tcConnectptr.p->copyCountWords = 0;
  tcConnectptr.p->tcOprec = tcConnectptr.i;
  tcConnectptr.p->tcHashKeyHi = 0;
  tcConnectptr.p->schemaVersion = scanptr.p->scanSchemaVersion;
  tcConnectptr.p->savePointId = gci;
  tcConnectptr.p->applRef = 0;
  tcConnectptr.p->transactionState = TcConnectionrec::SCAN_STATE_USED;

  scanptr.p->scanState = ScanRecord::WAIT_ACC_COPY;
  AccScanReq * req = (AccScanReq*)&signal->theData[0];
  req->senderData = scanptr.i;
  req->senderRef = cownref;
  req->tableId = tabptr.i;
  req->fragmentNo = fragId;
  req->requestInfo = 0;

  if (requestInfo == CopyFragReq::CFR_TRANSACTIONAL)
  {
    jam();
    /**
     * An node-recovery scan, is shared lock
     *   and may not perform disk-scan (as it then can miss uncomitted inserts)
     */
    AccScanReq::setLockMode(req->requestInfo, 0);
    AccScanReq::setReadCommittedFlag(req->requestInfo, 0);
    AccScanReq::setNRScanFlag(req->requestInfo, 1);
    AccScanReq::setNoDiskScanFlag(req->requestInfo, 1);
  }
  else
  {
    jam();
    /**
     * The non-transaction scan is really only a "normal" tup scan
     *   committed read, and don't disable disk-scan
     */
    AccScanReq::setLockMode(req->requestInfo, 0);
    AccScanReq::setReadCommittedFlag(req->requestInfo, 1);
    scanptr.p->readCommitted = 1;
  }

  req->transId1 = tcConnectptr.p->transid[0];
  req->transId2 = tcConnectptr.p->transid[1];
  req->savePointId = tcConnectptr.p->savePointId;
  req->maxPage = maxPage;
  sendSignal(scanptr.p->scanBlockref, GSN_ACC_SCANREQ, signal, 
	     AccScanReq::SignalLength + 1, JBB);
  
  if (! nodemask.isclear())
  {
    ndbrequire(nodemask.get(getOwnNodeId()));
    ndbrequire(nodemask.get(nodeId)); // cpy dest
    nodemask.clear(getOwnNodeId());
    nodemask.clear(nodeId);
    
    UpdateFragDistKeyOrd* 
      ord = (UpdateFragDistKeyOrd*)signal->getDataPtrSend();
    ord->tableId = tabptr.i;
    ord->fragId = fragId;
    ord->fragDistributionKey = key;
    i = 0;
    while ((i = nodemask.find(i+1)) != NdbNodeBitmask::NotFound)
    {
      if (getNodeInfo(i).m_version >=  checkversion)
	sendSignal(calcInstanceBlockRef(number(), i),
                   GSN_UPDATE_FRAG_DIST_KEY_ORD,
		   signal, UpdateFragDistKeyOrd::SignalLength, JBB);
    }
  }
  return;
}//Dblqh::execCOPY_FRAGREQ()

void
Dblqh::execUPDATE_FRAG_DIST_KEY_ORD(Signal * signal)
{
  jamEntry();
  UpdateFragDistKeyOrd* ord =(UpdateFragDistKeyOrd*)signal->getDataPtr();

  tabptr.i = ord->tableId;
  ptrCheckGuard(tabptr, ctabrecFileSize, tablerec);
  ndbrequire(getFragmentrec(signal, ord->fragId));
  fragptr.p->fragDistributionKey = ord->fragDistributionKey;
}

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
  signal->theData[5] = scanptr.p->scanApiBlockref;
  sendSignal(scanptr.p->scanBlockref, GSN_STORED_PROCREQ, signal, 6, JBB);
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
  c_fragment_pool.getPtr(fragptr);
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
    systemErrorLab(signal, __LINE__);
    return;
    break;
  }//switch
  continueFirstCopyAfterBlockedLab(signal);
  return;
}//Dblqh::storedProcConfCopyLab()

void Dblqh::continueFirstCopyAfterBlockedLab(Signal* signal) 
{
  /**
   * Start sending ROWID for all operations from now on
   */
  fragptr.p->m_copy_started_state = Fragrecord::AC_NR_COPY;
  if (ERROR_INSERTED(5714))
  {
    ndbout_c("Starting copy of tab: %u frag: %u",
             fragptr.p->tabRef, fragptr.p->fragId);
  }

  scanptr.i = tcConnectptr.p->tcScanRec;
  c_scanRecordPool.getPtr(scanptr);
  
  if (false && fragptr.p->tabRef > 4)
  {
    ndbout_c("STOPPING COPY X = [ %d %d %d %d ]",
	     refToBlock(scanptr.p->scanBlockref),
	     scanptr.p->scanAccPtr, RNIL, NextScanReq::ZSCAN_NEXT);
    
    /**
     * RESTART: > DUMP 7020 332 X
     */
    return;
  }
  
  signal->theData[0] = scanptr.p->scanAccPtr;
  signal->theData[1] = RNIL;
  signal->theData[2] = NextScanReq::ZSCAN_NEXT;
  sendSignal(scanptr.p->scanBlockref, GSN_NEXT_SCANREQ, signal, 3, JBB);
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
    if (tcConnectptr.p->copyCountWords == 0) {
      closeCopyLab(signal);
      return;
    }//if
/*---------------------------------------------------------------------------*/
// Wait until copying is completed also at the starting node before reporting
// completion. Signal completion through scanCompletedStatus-flag.
/*---------------------------------------------------------------------------*/
    scanptr.p->scanCompletedStatus = ZTRUE;
    scanptr.p->scanState = ScanRecord::WAIT_LQHKEY_COPY;
    if (ERROR_INSERTED(5043))
    {
      CLEAR_ERROR_INSERT_VALUE;
      tcConnectptr.p->copyCountWords = ~0;
      signal->theData[0] = 9999;
      sendSignal(numberToRef(CMVMI, scanptr.p->scanNodeId),
		 GSN_NDB_TAMPER, signal, 1, JBA);
    }
    return;
  }//if

  TcConnectionrec * tcConP = tcConnectptr.p;
  
  tcConP->m_use_rowid = true;
  tcConP->m_row_id = scanptr.p->m_row_id;

  scanptr.p->m_curr_batch_size_rows++;
  
  if (signal->getLength() == NextScanConf::SignalLengthNoKeyInfo)
  {
    jam();
    ndbrequire(nextScanConf->accOperationPtr == RNIL);
    initCopyTc(signal, ZDELETE);
    set_acc_ptr_in_scan_record(scanptr.p, 0, RNIL);
    tcConP->gci_hi = nextScanConf->gci;
    tcConP->gci_lo = 0;

    tcConP->primKeyLen = 0;
    tcConP->totSendlenAi = 0;
    tcConP->connectState = TcConnectionrec::COPY_CONNECTED;

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
    UintR TnoOfWords = 8;
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
    tcConP->transid[0] = TnoOfWords; // Data overload, see note!
    packLqhkeyreqLab(signal);
    tcConP->copyCountWords += TnoOfWords;
    scanptr.p->scanState = ScanRecord::WAIT_LQHKEY_COPY;
    if (tcConP->copyCountWords < cmaxWordsAtNodeRec) {
      nextRecordCopy(signal);
    }
    return;
  }
  else
  {
    // If accOperationPtr == RNIL no record was returned by ACC
    if (nextScanConf->accOperationPtr == RNIL) {
      jam();
      signal->theData[0] = scanptr.p->scanAccPtr;
      signal->theData[1] = AccCheckScan::ZCHECK_LCP_STOP;
      sendSignal(scanptr.p->scanBlockref, GSN_ACC_CHECK_SCAN, signal, 2, JBB);
      return;      
    }
    
    initCopyTc(signal, ZINSERT);
    set_acc_ptr_in_scan_record(scanptr.p, 0, nextScanConf->accOperationPtr);
    
    Fragrecord* fragPtrP= fragptr.p;
    scanptr.p->scanState = ScanRecord::WAIT_TUPKEY_COPY;
    tcConP->transactionState = TcConnectionrec::COPY_TUPKEY;
    if(tcConP->m_disk_table)
    {
      next_scanconf_load_diskpage(signal, scanptr, tcConnectptr,fragPtrP);
    }
    else
    {
      next_scanconf_tupkeyreq(signal, scanptr, tcConP, fragPtrP, RNIL);
    }
  }
}//Dblqh::nextScanConfCopyLab()


/*---------------------------------------------------------------------------*/
/*   USED IN COPYING OPERATION TO RECEIVE ATTRINFO FROM TUP.                 */
/*---------------------------------------------------------------------------*/
/* ************>> */
/*  TRANSID_AI  > */
/* ************>> */
void Dblqh::execTRANSID_AI(Signal* signal) 
{
  jamEntry();
  /* TransID_AI received from local TUP, data is linear inline in 
   * signal buff 
   */
  tcConnectptr.i = signal->theData[0];
  ptrCheckGuard(tcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
  Uint32 length = signal->length() - TransIdAI::HeaderLength;
  ndbrequire(tcConnectptr.p->transactionState == TcConnectionrec::COPY_TUPKEY);
  Uint32 * src = &signal->theData[ TransIdAI::HeaderLength ];
  bool ok= appendToSection(tcConnectptr.p->attrInfoIVal,
                           src,
                           length);
  if (unlikely(! ok))
  {
    jam();
    tcConnectptr.p->errorCode = ZGET_ATTRINBUF_ERROR;
  }
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
void Dblqh::copyTupkeyRefLab(Signal* signal)
{
  //const TupKeyRef * tupKeyRef = (TupKeyRef *)signal->getDataPtr();

  scanptr.i = tcConnectptr.p->tcScanRec;
  c_scanRecordPool.getPtr(scanptr);
  ScanRecord* scanP = scanptr.p;

  if (scanP->readCommitted == 0)
  {
    jam();
    ndbrequire(false); // Should not be possibe...we read with lock
  }
  else
  {
    jam();
    /**
     * Any readCommitted scan, can get 626 if it finds a candidate record
     *   that is not visible to the scan (i.e uncommitted inserts)
     *   if scanning with locks (shared/exclusive) this is not visible
     *   to LQH as lock is taken earlier
     */
    ndbrequire(terrorCode == 626);
  }

  ndbrequire(scanptr.p->scanState == ScanRecord::WAIT_TUPKEY_COPY);
  if (tcConnectptr.p->errorCode != 0)
  {
    jam();
    closeCopyLab(signal);
    return;
  }

  if (scanptr.p->scanCompletedStatus == ZTRUE)
  {
    jam();
    closeCopyLab(signal);
    return;
  }

  ndbrequire(tcConnectptr.p->copyCountWords < cmaxWordsAtNodeRec);
  scanptr.p->scanState = ScanRecord::WAIT_LQHKEY_COPY;
  nextRecordCopy(signal);
}

void Dblqh::copyTupkeyConfLab(Signal* signal) 
{
  const TupKeyConf * const tupKeyConf = (TupKeyConf *)signal->getDataPtr();

  UintR readLength = tupKeyConf->readLength;
  Uint32 tableId = tcConnectptr.p->tableref;
  scanptr.i = tcConnectptr.p->tcScanRec;
  c_scanRecordPool.getPtr(scanptr);
  ScanRecord* scanP = scanptr.p;

  if (scanP->readCommitted == 0)
  {
    jam();
    Uint32 accOpPtr= get_acc_ptr_from_scan_record(scanP, 0, false);
    ndbassert(accOpPtr != (Uint32)-1);
    c_acc->execACCKEY_ORD(signal, accOpPtr);
  }

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

  /* Read primary keys from TUP into signal buffer space
   * (used to get here via scan keyinfo)
   */
  Uint32* tmp = signal->getDataPtrSend()+24;
  Uint32 len= tcConnectptr.p->primKeyLen = readPrimaryKeys(scanP, tcConP, tmp);
  
  tcConP->gci_hi = tmp[len];
  tcConP->gci_lo = 0;
  // Calculate hash (no need to linearise key)
  if (g_key_descriptor_pool.getPtr(tableId)->hasCharAttr)
  {
    tcConnectptr.p->hashValue = calculateHash(tableId, tmp);
  }
  else
  {
    tcConnectptr.p->hashValue = md5_hash((Uint64*)tmp, len);
  }

  // Copy keyinfo into long section for LQHKEYREQ below
  if (unlikely(!keyinfoLab(tmp, len)))
  {
    /* Failed to store keyInfo, fail copy 
     * This will result in a COPY_FRAGREF being sent to
     * the starting node, which will cause it to fail
     */
    scanptr.p->scanErrorCounter++;
    tcConP->errorCode= ZGET_DATAREC_ERROR;
    scanptr.p->scanCompletedStatus= ZTRUE;

    closeCopyLab(signal);
    return;
  }

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
  scanP->m_curr_batch_size_bytes += 4 * TnoOfWords;
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

  if (scanptr.p->scanState == ScanRecord::WAIT_LQHKEY_COPY &&
      scanptr.p->scanErrorCounter)
  {
    jam();
    closeCopyLab(signal);
    return;
  }
  
  if (scanptr.p->scanState == ScanRecord::WAIT_LQHKEY_COPY) {
    jam();
/*---------------------------------------------------------------------------*/
// Make sure that something is in progress. Otherwise we will simply stop
// and nothing more will happen.
/*---------------------------------------------------------------------------*/
    systemErrorLab(signal, __LINE__);
    return;
  }//if
  return;
}//Dblqh::copyCompletedLab()

void Dblqh::nextRecordCopy(Signal* signal)
{
  fragptr.i = tcConnectptr.p->fragmentptr;
  c_fragment_pool.getPtr(fragptr);
  scanptr.i = tcConnectptr.p->tcScanRec;
  c_scanRecordPool.getPtr(scanptr);
  if (scanptr.p->scanState != ScanRecord::WAIT_LQHKEY_COPY) {
    jam();
/*---------------------------------------------------------------------------*/
// Make sure that nothing is in progress. Otherwise we will have to simultaneous
// scans on the same record and this will certainly lead to unexpected
// behaviour.
/*---------------------------------------------------------------------------*/
    systemErrorLab(signal, __LINE__);
    return;
  }//if
  scanptr.p->scanState = ScanRecord::WAIT_NEXT_SCAN_COPY;
  switch (fragptr.p->fragStatus) {
  case Fragrecord::FSACTIVE:
    jam();
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
    systemErrorLab(signal, __LINE__);
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
  if (acc_op_ptr != RNIL)
  {
    signal->theData[0] = scanptr.p->scanAccPtr;
    signal->theData[1] = acc_op_ptr;
    signal->theData[2] = NextScanReq::ZSCAN_NEXT_COMMIT;
    sendSignal(scanptr.p->scanBlockref, GSN_NEXT_SCANREQ, signal, 3, JBB);
  }
  else
  {
    /**
     * No need to commit (unlock)
     */
    signal->theData[0] = scanptr.p->scanAccPtr;
    signal->theData[1] = RNIL;
    signal->theData[2] = NextScanReq::ZSCAN_NEXT;
    sendSignal(scanptr.p->scanBlockref, GSN_NEXT_SCANREQ, signal, 3, JBB);
  }
  return;
}//Dblqh::continueCopyAfterBlockedLab()

void Dblqh::copyLqhKeyRefLab(Signal* signal) 
{
  ndbrequire(tcConnectptr.p->transid[1] == signal->theData[4]);
  Uint32 copyWords = signal->theData[3];
  scanptr.i = tcConnectptr.p->tcScanRec;
  c_scanRecordPool.getPtr(scanptr);
  scanptr.p->scanErrorCounter++;
  tcConnectptr.p->errorCode = terrorCode;
  
  LqhKeyConf* conf = (LqhKeyConf*)signal->getDataPtrSend();
  conf->transId1 = copyWords;
  conf->transId2 = tcConnectptr.p->transid[1];
  copyCompletedLab(signal);
}//Dblqh::copyLqhKeyRefLab()

void Dblqh::closeCopyLab(Signal* signal) 
{
  if (tcConnectptr.p->copyCountWords > 0) {
/*---------------------------------------------------------------------------*/
// We are still waiting for responses from the starting node.
// Wait until all of those have arrived until we start the
// close process.
/*---------------------------------------------------------------------------*/
    scanptr.p->scanState = ScanRecord::WAIT_LQHKEY_COPY;
    jam();
    return;
  }//if
  tcConnectptr.p->transid[0] = 0;
  tcConnectptr.p->transid[1] = 0;
  fragptr.i = tcConnectptr.p->fragmentptr;
  c_fragment_pool.getPtr(fragptr);

  /**
   * Stop sending ROWID for all operations from now on
   */
  fragptr.p->m_copy_started_state = Fragrecord::AC_NORMAL;
  if (ERROR_INSERTED(5714))
  {
    ndbout_c("Copy of tab: %u frag: %u complete",
             fragptr.p->tabRef, fragptr.p->fragId);
  }

  scanptr.i = tcConnectptr.p->tcScanRec;
  c_scanRecordPool.getPtr(scanptr);
  scanptr.p->scanState = ScanRecord::WAIT_CLOSE_COPY;
  switch (fragptr.p->fragStatus) {
  case Fragrecord::FSACTIVE:
    jam();
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
    systemErrorLab(signal, __LINE__);
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
  signal->theData[2] = NextScanReq::ZSCAN_CLOSE;
  sendSignal(scanptr.p->scanBlockref, GSN_NEXT_SCANREQ, signal, 3, JBB);
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
  signal->theData[5] = scanptr.p->scanApiBlockref;
  sendSignal(tcConnectptr.p->tcTupBlockref, GSN_STORED_PROCREQ, signal, 6, JBB);
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
  c_fragment_pool.getPtr(fragptr);
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
    sendSignal(tcConnectptr.p->clientBlockref, GSN_COPY_FRAGREF, signal,
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
      sendSignal(tcConnectptr.p->clientBlockref, GSN_COPY_FRAGREF, signal,
                 CopyFragRef::SignalLength, JBB);
    } else {
      jam();
      CopyFragConf * const conf = (CopyFragConf *)&signal->theData[0];
      conf->userPtr = scanptr.p->copyPtr;
      conf->sendingNodeId = cownNodeid;
      conf->startingNodeId = scanptr.p->scanNodeId;
      conf->tableId = tcConnectptr.p->tableref;
      conf->fragId = tcConnectptr.p->fragmentid;
      conf->rows_lo = scanptr.p->m_curr_batch_size_rows;
      conf->bytes_lo = scanptr.p->m_curr_batch_size_bytes;
      sendSignal(tcConnectptr.p->clientBlockref, GSN_COPY_FRAGCONF, signal,
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
  if (0) ndbout_c("closeCopyRequestLab: scanState: %d", scanptr.p->scanState);
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
  Uint32 flags = req->flags;
  if (unlikely(signal->getLength() < CopyActiveReq::SignalLength))
  {
    jam();
    flags = 0;
  }

  ndbrequire(getFragmentrec(signal, fragId));

  fragptr.p->fragStatus = Fragrecord::FSACTIVE;
  fragptr.p->fragDistributionKey = req->distributionKey;
  
  if (TRACENR_FLAG)
    TRACENR("tab: " << tabptr.i
	    << " frag: " << fragId
	    << " COPY ACTIVE"
            << " flags: " << hex << flags << endl);

  ndbrequire(cnoActiveCopy < 3);
  cactiveCopy[cnoActiveCopy] = fragptr.i;
  cnoActiveCopy++;
  fragptr.p->masterBlockref = masterRef;
  fragptr.p->masterPtr = masterPtr;

  if ((flags & CopyActiveReq::CAR_NO_LOGGING) == 0)
  {
    jam();
    if (fragptr.p->lcpFlag == Fragrecord::LCP_STATE_TRUE)
    {
      jam();
      fragptr.p->logFlag = Fragrecord::STATE_TRUE;
    }
  }
  
  if (flags & CopyActiveReq::CAR_NO_WAIT)
  {
    jam();
    ndbrequire(fragptr.p->activeTcCounter == 0);
    Uint32 save = fragptr.p->startGci;
    fragptr.p->startGci = 0;
    sendCopyActiveConf(signal, tabptr.i);
    fragptr.p->startGci = save;
    return;
  }

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
    c_fragment_pool.getPtr(fragptr);
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
void Dblqh::initCopyTc(Signal* signal, Operation_t op) 
{
  tcConnectptr.p->operation = ZREAD;
  tcConnectptr.p->apiVersionNo = 0;
  tcConnectptr.p->opExec = 0;	/* NOT INTERPRETED MODE */
  tcConnectptr.p->schemaVersion = scanptr.p->scanSchemaVersion;
  Uint32 reqinfo = 0;
  LqhKeyReq::setDirtyFlag(reqinfo, 1);
  LqhKeyReq::setSimpleFlag(reqinfo, 1);
  LqhKeyReq::setOperation(reqinfo, op);
  LqhKeyReq::setGCIFlag(reqinfo, 1);
  LqhKeyReq::setNrCopyFlag(reqinfo, 1);
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

  ndbrequire(!isNdbMtLqh()); // Handled by DblqhProxy

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

#ifdef NDB_DEBUG_FULL
static struct TraceLCP {
  void sendSignal(Uint32 ref, Uint32 gsn, Signal* signal,
		  Uint32 len, Uint32 prio);
  void save(Signal*);
  void restore(SimulatedBlock&, Signal* sig);
  struct Sig {
    enum { 
      Sig_save = 0,
      Sig_send = 1
    } type;
    SignalHeader header;
    Uint32 theData[25];
  };
  Vector<Sig> m_signals;
} g_trace_lcp;
template class Vector<TraceLCP::Sig>;
#else
#endif

void
Dblqh::force_lcp(Signal* signal)
{
  if (cLqhTimeOutCount == c_last_force_lcp_time)
  {
    jam();
    return;
  }

  c_last_force_lcp_time = cLqhTimeOutCount;
  signal->theData[0] = 7099;
  sendSignal(DBDIH_REF, GSN_DUMP_STATE_ORD, signal, 1, JBB);
}

void Dblqh::execLCP_FRAG_ORD(Signal* signal)
{
  jamEntry();
  CRASH_INSERTION(5010);

  LcpFragOrd lcpFragOrdCopy = * (LcpFragOrd *)&signal->theData[0];
  LcpFragOrd * lcpFragOrd = &lcpFragOrdCopy;

  Uint32 lcpId = lcpFragOrd->lcpId;

  lcpPtr.i = 0;
  ptrAss(lcpPtr, lcpRecord);
  
  lcpPtr.p->lastFragmentFlag = lcpFragOrd->lastFragmentFlag;
  if (lcpFragOrd->lastFragmentFlag)
  {
    jam();
    CRASH_INSERTION(5054);
    if (lcpPtr.p->lcpState == LcpRecord::LCP_IDLE) {
      jam();
      /* ----------------------------------------------------------
       *       NOW THE COMPLETE LOCAL CHECKPOINT ROUND IS COMPLETED.  
       * -------------------------------------------------------- */
      if (cnoOfFragsCheckpointed > 0) {
        jam();
        completeLcpRoundLab(signal, lcpId);
      } else {
        jam();
        clcpCompletedState = LCP_IDLE;
        sendLCP_COMPLETE_REP(signal, lcpId);
      }//if
    }
    return;
  }//if
  tabptr.i = lcpFragOrd->tableId;
  ptrCheckGuard(tabptr, ctabrecFileSize, tablerec);
  
  lcpPtr.i = 0;
  ptrAss(lcpPtr, lcpRecord);
  ndbrequire(!lcpPtr.p->lcpQueued);

  if (c_lcpId < lcpFragOrd->lcpId)
  {
    jam();

    lcpPtr.p->firstFragmentFlag= true;

#ifdef ERROR_INSERT
    if (check_ndb_versions())
    {
      /**
       * Only (so-far) in error insert
       *   check that keepGci (tail of REDO) is smaller than of head of REDO
       *
       */
      if (! ((cnewestCompletedGci >= lcpFragOrd->keepGci) &&
             (cnewestGci >= lcpFragOrd->keepGci)))
      {
        ndbout_c("lcpFragOrd->keepGci: %u cnewestCompletedGci: %u cnewestGci: %u",
                 lcpFragOrd->keepGci, cnewestCompletedGci, cnewestGci);
      }
      ndbrequire(cnewestCompletedGci >= lcpFragOrd->keepGci);
      ndbrequire(cnewestGci >= lcpFragOrd->keepGci);
    }
#endif

    c_lcpId = lcpFragOrd->lcpId;
    ndbrequire(lcpPtr.p->lcpState == LcpRecord::LCP_IDLE);
    setLogTail(signal, lcpFragOrd->keepGci);
    ndbrequire(clcpCompletedState == LCP_IDLE);
    clcpCompletedState = LCP_RUNNING;
  }
  
  if (! (tabptr.p->tableStatus == Tablerec::TABLE_DEFINED || tabptr.p->tableStatus == Tablerec::TABLE_READ_ONLY))
  {
    jam();
    LcpRecord::FragOrd fragOrd;
    fragOrd.fragPtrI = RNIL;
    fragOrd.lcpFragOrd = * lcpFragOrd;

    Fragrecord tmp;
    tmp.maxGciInLcp = cnewestGci;
    tmp.maxGciCompletedInLcp = cnewestCompletedGci;
    sendLCP_FRAG_REP(signal, fragOrd, &tmp);
    return;
  }

  cnoOfFragsCheckpointed++;
  ndbrequire(getFragmentrec(signal, lcpFragOrd->fragmentId));

  if (lcpPtr.p->lcpState != LcpRecord::LCP_IDLE)
  {
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

void Dblqh::execLCP_PREPARE_REF(Signal* signal) 
{
  jamEntry();

  LcpPrepareRef* ref= (LcpPrepareRef*)signal->getDataPtr();
  
  lcpPtr.i = ref->senderData;
  ptrCheckGuard(lcpPtr, clcpFileSize, lcpRecord);
  ndbrequire(lcpPtr.p->lcpState == LcpRecord::LCP_WAIT_FRAGID);
  
  fragptr.i = lcpPtr.p->currentFragment.fragPtrI;
  c_fragment_pool.getPtr(fragptr);
  
  ndbrequire(ref->tableId == fragptr.p->tabRef);
  ndbrequire(ref->fragmentId == fragptr.p->fragId);

  tabptr.i = ref->tableId;
  ptrCheckGuard(tabptr, ctabrecFileSize, tablerec);
  
  ndbrequire(lcpPtr.p->m_outstanding);
  lcpPtr.p->m_outstanding--;

  /**
   * Only BACKUP is allowed to ref LCP_PREPARE
   */
  ndbrequire(refToMain(signal->getSendersBlockRef()) == BACKUP);
  lcpPtr.p->m_error = ref->errorCode;

  stopLcpFragWatchdog();

  if (lcpPtr.p->m_outstanding == 0)
  {
    jam();
    
    if(lcpPtr.p->firstFragmentFlag)
    {
      jam();
      LcpFragOrd *ord= (LcpFragOrd*)signal->getDataPtrSend();
      lcpPtr.p->firstFragmentFlag= false;

      if (!isNdbMtLqh())
      {
        jam();
        *ord = lcpPtr.p->currentFragment.lcpFragOrd;
        EXECUTE_DIRECT(PGMAN, GSN_LCP_FRAG_ORD, signal, signal->length());
        jamEntry();
      
        /**
         * First fragment mean that last LCP is complete :-)
         */
        jam();
        *ord = lcpPtr.p->currentFragment.lcpFragOrd;
        EXECUTE_DIRECT(TSMAN, GSN_LCP_FRAG_ORD,
                       signal, signal->length(), 0);
        jamEntry();
      }
      else
      {
        /**
         * Handle by LqhProxy
         */
      }
    }
    
    lcpPtr.p->lcpState = LcpRecord::LCP_COMPLETED;
    contChkpNextFragLab(signal);
  }
}

/* --------------------------------------------------------------------------
 *       PRECONDITION: LCP_PTR:LCP_STATE = WAIT_FRAGID
 * -------------------------------------------------------------------------- 
 *       WE NOW HAVE THE LOCAL FRAGMENTS THAT THE LOCAL CHECKPOINT WILL USE.
 * -------------------------------------------------------------------------- */
void Dblqh::execLCP_PREPARE_CONF(Signal* signal) 
{
  jamEntry();

  LcpPrepareConf* conf= (LcpPrepareConf*)signal->getDataPtr();
  
  lcpPtr.i = conf->senderData;
  ptrCheckGuard(lcpPtr, clcpFileSize, lcpRecord);
  ndbrequire(lcpPtr.p->lcpState == LcpRecord::LCP_WAIT_FRAGID);
  
  fragptr.i = lcpPtr.p->currentFragment.fragPtrI;
  c_fragment_pool.getPtr(fragptr);

  // wl4391_todo obsolete
  if (refToBlock(signal->getSendersBlockRef()) != PGMAN)
  {
    ndbrequire(conf->tableId == fragptr.p->tabRef);
    ndbrequire(conf->fragmentId == fragptr.p->fragId);
  }
  
  ndbrequire(lcpPtr.p->m_outstanding);
  lcpPtr.p->m_outstanding--;
  if (lcpPtr.p->m_outstanding == 0)
  {
    jam();

    if(lcpPtr.p->firstFragmentFlag)
    {
      jam();
      LcpFragOrd *ord= (LcpFragOrd*)signal->getDataPtrSend();
      lcpPtr.p->firstFragmentFlag= false;

      // proxy is used in MT LQH to handle also the extra pgman worker
      if (!isNdbMtLqh())
      {
        jam();
        *ord = lcpPtr.p->currentFragment.lcpFragOrd;
        EXECUTE_DIRECT(PGMAN, GSN_LCP_FRAG_ORD, signal, signal->length());
        jamEntry();
      
        /**
         * First fragment mean that last LCP is complete :-)
         */
        jam();
        *ord = lcpPtr.p->currentFragment.lcpFragOrd;
        EXECUTE_DIRECT(TSMAN, GSN_LCP_FRAG_ORD,
                       signal, signal->length(), 0);
        jamEntry();
      }
      else
      {
        /**
         * Handled by proxy
         */
      }
    }
    
    if (lcpPtr.p->m_error)
    {
      jam();

      lcpPtr.p->lcpState = LcpRecord::LCP_COMPLETED;
      contChkpNextFragLab(signal);
      return;
    }

    lcpPtr.p->lcpState = LcpRecord::LCP_WAIT_HOLDOPS;
    lcpPtr.p->lcpState = LcpRecord::LCP_START_CHKP;
    
    /* ----------------------------------------------------------------------
     *    UPDATE THE MAX_GCI_IN_LCP AND MAX_GCI_COMPLETED_IN_LCP NOW BEFORE
     *    ACTIVATING THE FRAGMENT AGAIN.
     * --------------------------------------------------------------------- */
    ndbrequire(lcpPtr.p->currentFragment.lcpFragOrd.lcpNo < MAX_LCP_STORED);
    fragptr.p->maxGciInLcp = fragptr.p->newestGci;
    fragptr.p->maxGciCompletedInLcp = cnewestCompletedGci;
    
    {
      LcpFragOrd *ord= (LcpFragOrd*)signal->getDataPtrSend();
      *ord = lcpPtr.p->currentFragment.lcpFragOrd;
      Logfile_client lgman(this, c_lgman, 0);
      lgman.exec_lcp_frag_ord(signal);
      jamEntry();
      
      *ord = lcpPtr.p->currentFragment.lcpFragOrd;
      EXECUTE_DIRECT(DBTUP, GSN_LCP_FRAG_ORD, signal, signal->length());
      jamEntry();
    }
    
    BackupFragmentReq* req= (BackupFragmentReq*)signal->getDataPtr();
    req->tableId = lcpPtr.p->currentFragment.lcpFragOrd.tableId;
    req->fragmentNo = 0; 
    req->backupPtr = m_backup_ptr;
    req->backupId = lcpPtr.p->currentFragment.lcpFragOrd.lcpId;
    req->count = 0;
    
#ifdef NDB_DEBUG_FULL
    if(ERROR_INSERTED(5904))
    {
    g_trace_lcp.sendSignal(BACKUP_REF, GSN_BACKUP_FRAGMENT_REQ, signal, 
			   BackupFragmentReq::SignalLength, JBB);
    }
    else
#endif
    {
      if (ERROR_INSERTED(5044) && 
	  (fragptr.p->tabRef == c_error_insert_table_id) && 
	  fragptr.p->fragId) // Not first frag
      {
	/**
	 * Force CRASH_INSERTION in 10s
	 */
	ndbout_c("table: %d frag: %d", fragptr.p->tabRef, fragptr.p->fragId);
	SET_ERROR_INSERT_VALUE(5027);
	sendSignalWithDelay(reference(), GSN_START_RECREQ, signal, 10000, 1);
      }
      else if (ERROR_INSERTED(5053))
      {
        BlockReference backupRef = calcInstanceBlockRef(BACKUP);
        sendSignalWithDelay(backupRef, GSN_BACKUP_FRAGMENT_REQ, signal,
                            150, BackupFragmentReq::SignalLength);
      }
      else
      {
        BlockReference backupRef = calcInstanceBlockRef(BACKUP);
	sendSignal(backupRef, GSN_BACKUP_FRAGMENT_REQ, signal, 
		   BackupFragmentReq::SignalLength, JBB);
      }
    }
  }
}

void Dblqh::execBACKUP_FRAGMENT_REF(Signal* signal) 
{
  BackupFragmentRef *ref= (BackupFragmentRef*)signal->getDataPtr();
  char buf[100];
  BaseString::snprintf(buf,sizeof(buf),
                       "Unable to store fragment during LCP. NDBFS Error: %u",
                       ref->errorCode);

  progError(__LINE__,
            (ref->errorCode & FsRef::FS_ERR_BIT)?
            NDBD_EXIT_AFS_UNKNOWN
            : ref->errorCode,
            buf);
}

void Dblqh::execBACKUP_FRAGMENT_CONF(Signal* signal) 
{
  jamEntry();

  if (ERROR_INSERTED(5073))
  {
    ndbout_c("Delaying BACKUP_FRAGMENT_CONF");
    sendSignalWithDelay(reference(), GSN_BACKUP_FRAGMENT_CONF, signal, 500,
                        signal->getLength());
    return;
  }

  //BackupFragmentConf* conf= (BackupFragmentConf*)signal->getDataPtr();

  lcpPtr.i = 0;
  ptrCheckGuard(lcpPtr, clcpFileSize, lcpRecord);
  ndbrequire(lcpPtr.p->lcpState == LcpRecord::LCP_START_CHKP);
  lcpPtr.p->lcpState = LcpRecord::LCP_COMPLETED;

  stopLcpFragWatchdog();

  /* ------------------------------------------------------------------------
   *   THE LOCAL CHECKPOINT HAS BEEN COMPLETED. IT IS NOW TIME TO START 
   *   A LOCAL CHECKPOINT ON THE NEXT FRAGMENT OR COMPLETE THIS LCP ROUND.
   * ------------------------------------------------------------------------ 
   *   WE START BY SENDING LCP_REPORT TO DIH TO REPORT THE COMPLETED LCP.
   *   TO CATER FOR NODE CRASHES WE SEND IT IN PARALLEL TO ALL NODES.
   * ----------------------------------------------------------------------- */
  fragptr.i = lcpPtr.p->currentFragment.fragPtrI;
  c_fragment_pool.getPtr(fragptr);

  /**
   * Update maxGciInLcp after scan has been performed
   */
#if defined VM_TRACE || defined ERROR_INSERT
  if (fragptr.p->newestGci != fragptr.p->maxGciInLcp)
  {
    ndbout_c("tab: %u frag: %u increasing maxGciInLcp from %u to %u",
             fragptr.p->tabRef,
             fragptr.p->fragId,
             fragptr.p->maxGciInLcp, fragptr.p->newestGci);
  }
#endif

  fragptr.p->maxGciInLcp = fragptr.p->newestGci;
  
  contChkpNextFragLab(signal);
  return;
}//Dblqh::lcpCompletedLab()

void
Dblqh::sendLCP_FRAG_REP(Signal * signal, 
			const LcpRecord::FragOrd & fragOrd,
                        const Fragrecord * fragPtrP) const
{
  ndbrequire(fragOrd.lcpFragOrd.lcpNo < MAX_LCP_STORED);
  LcpFragRep * const lcpReport = (LcpFragRep *)&signal->theData[0];
  lcpReport->nodeId = cownNodeid;
  lcpReport->lcpId = fragOrd.lcpFragOrd.lcpId;
  lcpReport->lcpNo = fragOrd.lcpFragOrd.lcpNo;
  lcpReport->tableId = fragOrd.lcpFragOrd.tableId;
  lcpReport->fragId = fragOrd.lcpFragOrd.fragmentId;
  lcpReport->maxGciCompleted = fragPtrP->maxGciCompletedInLcp;
  lcpReport->maxGciStarted = fragPtrP->maxGciInLcp;
  
  Uint32 ref = DBDIH_REF;
  if (isNdbMtLqh())
  {
    jam();
    ref = DBLQH_REF;
  }
  lcpReport->nodeId = LcpFragRep::BROADCAST_REQ;
  sendSignal(ref, GSN_LCP_FRAG_REP, signal,
             LcpFragRep::SignalLength, JBB);
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
    return;
  }//if

  /**
   * Send rep when fragment is done + unblocked
   */
  sendLCP_FRAG_REP(signal, lcpPtr.p->currentFragment,
                   c_fragment_pool.getPtr(lcpPtr.p->currentFragment.fragPtrI));
  
  /* ------------------------------------------------------------------------
   *       WE ALSO RELEASE THE LOCAL LCP RECORDS.
   * ----------------------------------------------------------------------- */
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
    completeLcpRoundLab(signal, lcpPtr.p->currentFragment.lcpFragOrd.lcpId);
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
  TablerecPtr tabPtr;
  tabPtr.i = lcpPtr.p->currentFragment.lcpFragOrd.tableId;
  ptrAss(tabPtr, tablerec);

  if(tabPtr.p->tableStatus != Tablerec::TABLE_DEFINED)
  {
    jam();
    /**
     * Fake that the fragment is done
     */
    contChkpNextFragLab(signal);
    return;
  }

  lcpPtr.p->m_error = 0;
  lcpPtr.p->m_outstanding = 1;

  ndbrequire(tabPtr.p->tableStatus == Tablerec::TABLE_DEFINED ||
             tabPtr.p->tableStatus == Tablerec::TABLE_READ_ONLY);
  
  lcpPtr.p->lcpState = LcpRecord::LCP_WAIT_FRAGID;
  LcpPrepareReq* req= (LcpPrepareReq*)signal->getDataPtr();
  req->senderData = lcpPtr.i;
  req->senderRef = reference();
  req->lcpNo = lcpPtr.p->currentFragment.lcpFragOrd.lcpNo;
  req->tableId = lcpPtr.p->currentFragment.lcpFragOrd.tableId;
  req->fragmentId = lcpPtr.p->currentFragment.lcpFragOrd.fragmentId;
  req->lcpId = lcpPtr.p->currentFragment.lcpFragOrd.lcpId % MAX_LCP_STORED;
  req->backupPtr = m_backup_ptr;
  req->backupId = lcpPtr.p->currentFragment.lcpFragOrd.lcpId;
  BlockReference backupRef = calcInstanceBlockRef(BACKUP);
  sendSignal(backupRef, GSN_LCP_PREPARE_REQ, signal, 
	     LcpPrepareReq::SignalLength, JBB);

  /* Now start the LCP fragment watchdog */
  startLcpFragWatchdog(signal);

}//Dblqh::sendLCP_FRAGIDREQ()

void Dblqh::sendEMPTY_LCP_CONF(Signal* signal, bool idle)
{
  EmptyLcpRep * sig = (EmptyLcpRep*)signal->getDataPtrSend();
  EmptyLcpConf * rep = (EmptyLcpConf*)sig->conf;

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

  lcpPtr.p->m_EMPTY_LCP_REQ.copyto(NdbNodeBitmask::Size, sig->receiverGroup);
  sendSignal(DBDIH_REF, GSN_EMPTY_LCP_REP, signal,
             EmptyLcpRep::SignalLength + EmptyLcpConf::SignalLength, JBB);

  lcpPtr.p->reportEmpty = false;
  lcpPtr.p->m_EMPTY_LCP_REQ.clear();
}//Dblqh::sendEMPTY_LCPCONF()

/* --------------------------------------------------------------------------
 *       THE LOCAL CHECKPOINT ROUND IS NOW COMPLETED. SEND COMPLETED MESSAGE
 *       TO THE MASTER DIH.
 * ------------------------------------------------------------------------- */
void Dblqh::completeLcpRoundLab(Signal* signal, Uint32 lcpId)
{
  clcpCompletedState = LCP_CLOSE_STARTED;

  lcpPtr.i = 0;
  ptrAss(lcpPtr, lcpRecord);
  lcpPtr.p->m_outstanding = 0;

  EndLcpReq* req= (EndLcpReq*)signal->getDataPtr();
  req->senderData= lcpPtr.i;
  req->senderRef= reference();
  req->backupPtr= m_backup_ptr;
  req->backupId= lcpId;

  BlockReference backupRef = calcInstanceBlockRef(BACKUP);

  lcpPtr.p->m_outstanding++;
  sendSignal(backupRef, GSN_END_LCP_REQ, signal, 
	     EndLcpReq::SignalLength, JBB);

  if (!isNdbMtLqh())
  {
    jam();
    lcpPtr.p->m_outstanding++;
    sendSignal(PGMAN_REF, GSN_END_LCP_REQ, signal, 
               EndLcpReq::SignalLength, JBB);

    lcpPtr.p->m_outstanding++;
    sendSignal(LGMAN_REF, GSN_END_LCP_REQ, signal, 
               EndLcpReq::SignalLength, JBB);

    EXECUTE_DIRECT(TSMAN, GSN_END_LCP_REQ,
                   signal, EndLcpReq::SignalLength, 0);
  }
  else
  {
    /**
     * This is all handled by LqhProxy
     */
  }
  return;
}//Dblqh::completeLcpRoundLab()

void Dblqh::execEND_LCPCONF(Signal* signal) 
{
  jamEntry();
  lcpPtr.i = 0;
  ptrAss(lcpPtr, lcpRecord);

  ndbrequire(clcpCompletedState == LCP_CLOSE_STARTED);
  ndbrequire(lcpPtr.p->m_outstanding);
  
  lcpPtr.p->m_outstanding--;
  if(lcpPtr.p->m_outstanding == 0)
  {
    jam();
    clcpCompletedState = LCP_IDLE;
    sendLCP_COMPLETE_REP(signal, lcpPtr.p->currentFragment.lcpFragOrd.lcpId);

    CRASH_INSERTION(5056);
  }
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
  lcpPtr.p->firstFragmentFlag = false;
  
  LcpCompleteRep* rep = (LcpCompleteRep*)signal->getDataPtrSend();
  rep->nodeId = getOwnNodeId();
  rep->lcpId = lcpId;
  rep->blockNo = DBLQH;
  
  Uint32 ref = DBDIH_REF;
  if (isNdbMtLqh())
  {
    jam();
    ref = DBLQH_REF;
  }
  rep->nodeId = LcpFragRep::BROADCAST_REQ;

  sendSignal(ref, GSN_LCP_COMPLETE_REP, signal,
             LcpCompleteRep::SignalLength, JBB);
  
  if(lcpPtr.p->reportEmpty){
    jam();
    sendEMPTY_LCP_CONF(signal, true);
  }
  
  if (cstartRecReq < SRR_FIRST_LCP_DONE)
  {
    jam();
    ndbrequire(cstartRecReq == SRR_REDO_COMPLETE);
    cstartRecReq = SRR_FIRST_LCP_DONE;
  }
  return;
  
}//Dblqh::sendCOMP_LCP_ROUND()

#if NOT_YET
void
Dblqh::execLCP_COMPLETE_REP(Signal* signal)
{
  /**
   * This is sent when last LCP is restorable
   */
  LcpCompleteRep * rep = (LcpCompleteRep*)signal->getDataPtr();
  Uint32 keepGci = rep->keepGci;
  setLogTail(signal, keepGci);
}
#endif

/* ------------------------------------------------------------------------- */
/* -------               SEND ACC_LCPREQ AND TUP_LCPREQ              ------- */
/*                                                                           */
/*       INPUT:          LCP_PTR             LOCAL CHECKPOINT RECORD         */
/*                       FRAGPTR             FRAGMENT RECORD                 */
/*       SUBROUTINE SHORT NAME = STL                                         */
/* ------------------------------------------------------------------------- */
void Dblqh::sendStartLcp(Signal* signal) 
{
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

void Dblqh::setLogTail(Signal* signal, Uint32 keepGci) 
{
  LogPartRecordPtr sltLogPartPtr;
  LogFileRecordPtr sltLogFilePtr;
  UintR tsltMbyte;
  UintR tsltStartMbyte;
  UintR tsltIndex;
  UintR tsltFlag;

  for (sltLogPartPtr.i = 0; sltLogPartPtr.i < clogPartFileSize; sltLogPartPtr.i++) {
    jam();
    bool TchangeMB = false;
retry:
    ptrAss(sltLogPartPtr, logPartRecord);
    findLogfile(signal, sltLogPartPtr.p->logTailFileNo,
                sltLogPartPtr, &sltLogFilePtr);

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
	 tsltIndex <= clogFileSize - 1;
	 tsltIndex++) {
      if (sltLogFilePtr.p->logMaxGciStarted[tsltIndex] >= keepGci) {
/* ------------------------------------------------------------------------- */
/*WE ARE NOT ALLOWED TO STEP THE LOG ANY FURTHER AHEAD                       */
/*SET THE NEW LOG TAIL AND CONTINUE WITH NEXT LOG PART.                      */
/*THIS MBYTE IS NOT TO BE INCLUDED SO WE NEED TO STEP BACK ONE MBYTE.        */
/* ------------------------------------------------------------------------- */
        /* Check keepGCI MB has a reasonable GCI value */
        ndbrequire(sltLogFilePtr.p->logMaxGciStarted[tsltIndex] != ((Uint32) -1));
        if (tsltIndex != 0) {
          jam();
          tsltMbyte = tsltIndex - 1;
        } else {
          jam();
/* ------------------------------------------------------------------------- */
/*STEPPING BACK INCLUDES ALSO STEPPING BACK TO THE PREVIOUS LOG FILE.        */
/* ------------------------------------------------------------------------- */
          tsltMbyte = clogFileSize - 1;
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

/* ------------------------------------------------------------------------- */
/*SINCE LOG_MAX_GCI_STARTED ONLY KEEP TRACK OF COMMIT LOG RECORDS WE ALSO    */
/*HAVE TO STEP BACK THE TAIL SO THAT WE INCLUDE ALL PREPARE RECORDS          */
/*NEEDED FOR THOSE COMMIT RECORDS IN THIS MBYTE. THIS IS A RATHER            */
/*CONSERVATIVE APPROACH BUT IT WORKS.                                        */
/* ------------------------------------------------------------------------- */
      arrGuard(tsltMbyte, clogFileSize);
      sltLogPartPtr.p->logTailFileNo =
        sltLogFilePtr.p->logLastPrepRef[tsltMbyte] >> 16;
      sltLogPartPtr.p->logTailMbyte = 
        sltLogFilePtr.p->logLastPrepRef[tsltMbyte] & 65535;

      if (DEBUG_REDO)
      {
        ndbout_c("part: %u setLogTail(gci: %u): file: %u mb: %u",
                 sltLogPartPtr.p->logPartNo, 
                 keepGci,
                 sltLogPartPtr.p->logTailFileNo,
                 sltLogPartPtr.p->logTailMbyte);
      }

      bool tailmoved = !(ToldTailFileNo == sltLogPartPtr.p->logTailFileNo &&
                         ToldTailMByte == sltLogPartPtr.p->logTailMbyte);

      LogFileRecordPtr tmpfile;
      tmpfile.i = sltLogPartPtr.p->currentLogfile;
      ptrCheckGuard(tmpfile, clogFileFileSize, logFileRecord);

      LogPosition head = { tmpfile.p->fileNo, tmpfile.p->currentMbyte };
      LogPosition tail = { sltLogPartPtr.p->logTailFileNo,
                           sltLogPartPtr.p->logTailMbyte};
      Uint64 mb = free_log(head, tail, sltLogPartPtr.p->noLogFiles,
                           clogFileSize);

      if (mb <= c_free_mb_force_lcp_limit)
      {
        /**
         * Force a new LCP
         */
        force_lcp(signal);
      }

      if (tailmoved && mb > c_free_mb_tail_problem_limit)
      {
        jam();
        update_log_problem(signal, sltLogPartPtr,
                           LogPartRecord::P_TAIL_PROBLEM, false);
      }
      else if (!tailmoved && mb <= c_free_mb_force_lcp_limit)
      {
        jam();
        /**
         * Tail didn't move...and we forced a new LCP
         *   This could be as currentMb, contains backreferences making it
         *   Check if changing mb forward will help situation
         */
        if (mb < 2)
        {
          /**
           * 0 or 1 mb free, no point in trying to changeMbyte forward...
           */
          jam();
          goto next;
        }

        if (TchangeMB)
        {
          jam();
          /**
           * We already did move forward...
           */
          goto next;
        }

        TcConnectionrecPtr tmp;
        tmp.i = sltLogPartPtr.p->firstLogTcrec;
        if (tmp.i != RNIL)
        {
          jam();
          ptrCheckGuard(tmp, ctcConnectrecFileSize, tcConnectionrec);
          Uint32 fileNo = tmp.p->logStartFileNo;
          Uint32 mbyte = tmp.p->logStartPageNo >> ZTWOLOG_NO_PAGES_IN_MBYTE;

          if (fileNo == sltLogPartPtr.p->logTailFileNo &&
              mbyte == sltLogPartPtr.p->logTailMbyte)
          {
            jam();
            /**
             * An uncommitted operation...still pending...
             *   with back-reference to tail...not much to do
             *   (theoretically we could rewrite log-entry here...
             *    but this is for future)
             * skip to next
             */
            goto next;
          }
        }

        {
          /**
           * Try forcing a changeMbyte
           */
          jam();
          logPartPtr = sltLogPartPtr;
          logFilePtr.i = logPartPtr.p->currentLogfile;
          ptrCheckGuard(logFilePtr, clogFileFileSize, logFileRecord);
          logPagePtr.i = logFilePtr.p->currentLogpage;
          ptrCheckGuard(logPagePtr, clogPageFileSize, logPageRecord);
          changeMbyte(signal);
          TchangeMB = true; // don't try this twice...
          goto retry;
        }
      }
    }
next:
    (void)1;
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

#if defined VM_TRACE || defined ERROR_INSERT
static Uint32 m_gcp_monitor = 0;
#endif

void Dblqh::execGCP_SAVEREQ(Signal* signal) 
{
  jamEntry();
  const GCPSaveReq * const saveReq = (GCPSaveReq *)&signal->theData[0];

  CRASH_INSERTION(5000);

  if (ERROR_INSERTED(5007)){
    CLEAR_ERROR_INSERT_VALUE;
    sendSignalWithDelay(cownref, GSN_GCP_SAVEREQ, signal, 10000, 
			signal->length());
    return;
  }

  if (unlikely(refToNode(signal->getSendersBlockRef()) != getOwnNodeId()))
  {
    /**
     * This code is only run during upgrade from pre-micro-gcp version.
     *
     * During startup, we make sure not to allow starting multi-threaded
     * NDBD while such an upgrade is taking place. So the EXECUTE_DIRECT()
     * below, which would be cross-thread in multi-threaded NDBD, is thus
     * safe since it never runs in the non-safe case.
     */
    ndbassert(!isMultiThreaded());
    jam();
    ndbassert(!ndb_check_micro_gcp
              (getNodeInfo(refToNode
                           (signal->getSendersBlockRef())).m_version));
    EXECUTE_DIRECT(DBDIH, GSN_GCP_SAVEREQ, signal, signal->getLength());
    return;
  }
  
  const Uint32 dihBlockRef = saveReq->dihBlockRef;
  const Uint32 dihPtr = saveReq->dihPtr;
  const Uint32 gci = saveReq->gci;

#if defined VM_TRACE || defined ERROR_INSERT
  if (!isNdbMtLqh()) { // wl4391_todo mt-safe
  ndbrequire(m_gcp_monitor == 0 || 
             (m_gcp_monitor == gci) || 
             (m_gcp_monitor + 1) == gci);
  }
  m_gcp_monitor = gci;
#endif
  
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

  Uint32 saveNewestCompletedGci = cnewestCompletedGci;
  cnewestCompletedGci = gci;

  if (cstartRecReq < SRR_REDO_COMPLETE)
  {
    /**
     * REDO running is not complete
     */
    GCPSaveRef * const saveRef = (GCPSaveRef*)&signal->theData[0];
    saveRef->dihPtr = dihPtr;
    saveRef->nodeId = getOwnNodeId();
    saveRef->gci    = gci;
    saveRef->errorCode = GCPSaveRef::NodeRestartInProgress;
    sendSignal(dihBlockRef, GSN_GCP_SAVEREF, signal, 
	       GCPSaveRef::SignalLength, JBB);
    return;
  }
  
  ndbrequire(gci >= saveNewestCompletedGci);
  
  if (gci == saveNewestCompletedGci) 
  {
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
  cnewestCompletedGci = gci;
  if (gci > cnewestGci) {
    jam();
    cnewestGci = gci;
  }//if

  if(cstartRecReq < SRR_FIRST_LCP_DONE)
  {
    /**
     * First LCP has not been done
     */
    GCPSaveRef * const saveRef = (GCPSaveRef*)&signal->theData[0];
    saveRef->dihPtr = dihPtr;
    saveRef->nodeId = getOwnNodeId();
    saveRef->gci    = gci;
    saveRef->errorCode = GCPSaveRef::NodeRestartInProgress;
    sendSignal(dihBlockRef, GSN_GCP_SAVEREF, signal, 
	       GCPSaveRef::SignalLength, JBB);

    if (ERROR_INSERTED(5052))
    {
      jam();
      signal->theData[0] = 9999;
      sendSignalWithDelay(CMVMI_REF, GSN_NDB_TAMPER, signal, 300, 1);
    }
    return;
  }

  CRASH_INSERTION(5052);

#ifdef GCP_TIMER_HACK
  NdbTick_getMicroTimer(&globalData.gcp_timer_save[0]);
#endif

  ccurrentGcprec = 0;
  gcpPtr.i = ccurrentGcprec;
  ptrCheckGuard(gcpPtr, cgcprecFileSize, gcpRecord);
  
  gcpPtr.p->gcpBlockref = dihBlockRef;
  gcpPtr.p->gcpUserptr = dihPtr;
  gcpPtr.p->gcpId = gci;
  bool tlogActive = false;
  for (logPartPtr.i = 0; logPartPtr.i < clogPartFileSize; logPartPtr.i++) {
    ptrAss(logPartPtr, logPartRecord);
    if (logPartPtr.p->logPartState == LogPartRecord::ACTIVE) {
      jam();
      logPartPtr.p->waitWriteGciLog = LogPartRecord::WWGL_TRUE;
      tlogActive = true;
      if (logPartPtr.p->LogLqhKeyReqSent == ZFALSE)
      {
        jam();
        logPartPtr.p->LogLqhKeyReqSent = ZTRUE;
        signal->theData[0] = ZLOG_LQHKEYREQ;
        signal->theData[1] = logPartPtr.i;
        sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
      }
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

/**
 * This is needed for ndbmtd to serialize
 * SUB_GCP_COMPLETE_REP vs FIRE_TRIG_ORD
 */
void
Dblqh::execSUB_GCP_COMPLETE_REP(Signal* signal)
{
  jamEntry();
  Uint32 len = signal->getLength();
  EXECUTE_DIRECT(DBTUP, GSN_SUB_GCP_COMPLETE_REP, signal, len);
  sendSignal(SUMA_REF, GSN_SUB_GCP_COMPLETE_REP, signal, len, JBB);
}

/* ------------------------------------------------------------------------- */
/*  START TIME SUPERVISION OF THE LOG PARTS.                                 */
/* ------------------------------------------------------------------------- */
void Dblqh::startTimeSupervision(Signal* signal) 
{
  for (logPartPtr.i = 0; logPartPtr.i < clogPartFileSize; logPartPtr.i++) {
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
  for (logPartPtr.i = 0; logPartPtr.i < clogPartFileSize; logPartPtr.i++) {
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
  // initialize un-used part
  Uint32 Ti;
  for (Ti = clogPartFileSize; Ti < NDB_MAX_LOG_PARTS; Ti++) {
    gcpPtr.p->gcpFilePtr[Ti] = ZNIL;
    gcpPtr.p->gcpPageNo[Ti] = ZNIL;
    gcpPtr.p->gcpSyncReady[Ti] = FALSE;
    gcpPtr.p->gcpWordNo[Ti] = ZNIL;
  }
  return;
}//Dblqh::initGcpRecLab()

/* ========================================================================= */
/* ==== CHECK IF ANY GLOBAL CHECKPOINTS ARE COMPLETED AFTER A COMPLETED===== */
/*      DISK WRITE.                                                          */
/*                                                                           */
/*       SUBROUTINE SHORT NAME = CGC                                         */
/* return: true if gcp was completed */
/* ========================================================================= */
bool
Dblqh::checkGcpCompleted(Signal* signal,
                         Uint32 tcgcPageWritten,
                         Uint32 tcgcWordWritten) 
{
  UintR tcgcFlag;
  UintR tcgcJ;

  gcpPtr.i = logPartPtr.p->gcprec;
  if (gcpPtr.i != RNIL)
  {
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
        return false;
      } else {
        if (tcgcPageWritten == gcpPtr.p->gcpPageNo[logPartPtr.i]) {
          if (tcgcWordWritten < gcpPtr.p->gcpWordNo[logPartPtr.i]) {
            jam();
/* ------------------------------------------------------------------------- */
/* THIS LOG PART HAVE NOT YET WRITTEN THE GLOBAL CHECKPOINT TO DISK.         */
/* ------------------------------------------------------------------------- */
            return false;
          }//if
        }//if
      }//if
/* ------------------------------------------------------------------------- */
/* THIS LOG PART HAVE WRITTEN THE GLOBAL CHECKPOINT TO DISK.                 */
/* ------------------------------------------------------------------------- */
      logPartPtr.p->gcprec = RNIL;
      gcpPtr.p->gcpLogPartState[logPartPtr.i] = ZON_DISK;
      tcgcFlag = ZTRUE;
      for (tcgcJ = 0; tcgcJ < clogPartFileSize; tcgcJ++) 
      {
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
      if (tcgcFlag == ZFALSE)
      {
        return false;
      }

      if (tcgcFlag == ZTRUE)
      {
        jam();
/* ------------------------------------------------------------------------- */
/*WE HAVE FOUND A COMPLETED GLOBAL CHECKPOINT OPERATION. WE NOW NEED TO SEND */
/*GCP_SAVECONF, REMOVE THE GCP RECORD FROM THE LIST OF WAITING GCP RECORDS   */
/*ON THIS LOG PART AND RELEASE THE GCP RECORD.                               */
// After changing the log implementation we need to perform a FSSYNCREQ on all
// log files where the last log word resided first before proceeding.
/* ------------------------------------------------------------------------- */
        UintR Ti;
        for (Ti = 0; Ti < clogPartFileSize; Ti++) {
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
      }//if
    }//if
    return true;
  }//if
  return false;
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
  ptrCheckGuard(localLogPartPtr, clogPartFileSize, logPartRecord);
  localGcpPtr.i = ccurrentGcprec;
  ptrCheckGuard(localGcpPtr, cgcprecFileSize, gcpRecord);
  localGcpPtr.p->gcpSyncReady[localLogPartPtr.i] = ZTRUE;
  UintR Ti;

  if (DEBUG_REDO)
  {
    ndbout_c("part: %u file: %u gci: %u SYNC CONF",
             localLogPartPtr.p->logPartNo,
             localLogFilePtr.p->fileNo,
             localGcpPtr.p->gcpId);
  }
  for (Ti = 0; Ti < clogPartFileSize; Ti++) {
    jam();
    if (localGcpPtr.p->gcpSyncReady[Ti] == ZFALSE) {
      jam();
      return;
    }//if
  }//for

#ifdef GCP_TIMER_HACK
  NdbTick_getMicroTimer(&globalData.gcp_timer_save[1]);
#endif

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
  logFilePtr.p->fileRef = RNIL;

  if (DEBUG_REDO)
  {
    logPartPtr.i = logFilePtr.p->logPartRec;
    ptrCheckGuard(logPartPtr, clogPartFileSize, logPartRecord);
    ndbout_c("part: %u file: %u CLOSE CONF",
             logPartPtr.p->logPartNo,
             logFilePtr.p->fileNo);
  }

  switch (logFilePtr.p->logFileStatus) {
  case LogFileRecord::CLOSE_SR_READ_INVALIDATE_PAGES:
    jam();
    logFilePtr.p->logFileStatus = LogFileRecord::CLOSED;

    logPartPtr.i = logFilePtr.p->logPartRec;
    ptrCheckGuard(logPartPtr, clogPartFileSize, logPartRecord);

    readFileInInvalidate(signal, 2);
    return;

  case LogFileRecord::CLOSE_SR_READ_INVALIDATE_SEARCH_FILES:
    jam();
    logFilePtr.p->logFileStatus = LogFileRecord::CLOSED;

    logPartPtr.i = logFilePtr.p->logPartRec;
    ptrCheckGuard(logPartPtr, clogPartFileSize, logPartRecord);

    readFileInInvalidate(signal, 4);
    return;
  case LogFileRecord::CLOSE_SR_READ_INVALIDATE_SEARCH_LAST_FILE:
    logFilePtr.p->logFileStatus = LogFileRecord::CLOSED;

    logPartPtr.i = logFilePtr.p->logPartRec;
    ptrCheckGuard(logPartPtr, clogPartFileSize, logPartRecord);

    readFileInInvalidate(signal, 7);
    return;
  case LogFileRecord::CLOSE_SR_WRITE_INVALIDATE_PAGES:
    jam();
    logFilePtr.p->logFileStatus = LogFileRecord::CLOSED;

    logPartPtr.i = logFilePtr.p->logPartRec;
    ptrCheckGuard(logPartPtr, clogPartFileSize, logPartRecord);

    writeFileInInvalidate(signal, 1);
    return;
  case LogFileRecord::CLOSING_INIT:
    jam();
    logFileInitDone++ ;
    closingInitLab(signal);
    return;
  case LogFileRecord::CLOSING_SR:
    jam();
    closingSrLab(signal);
    return;
  case LogFileRecord::CLOSING_EXEC_SR:
    jam();
    closeExecSrLab(signal);
    return;
  case LogFileRecord::CLOSING_EXEC_SR_COMPLETED:
    jam();
    closeExecSrCompletedLab(signal);
    return;
  case LogFileRecord::CLOSING_WRITE_LOG:
    jam();
    closeWriteLogLab(signal);
    return;
  case LogFileRecord::CLOSING_EXEC_LOG:
    jam();
    closeExecLogLab(signal);
    return;
#ifndef NO_REDO_OPEN_FILE_CACHE
  case LogFileRecord::CLOSING_EXEC_LOG_CACHED:
    jam();
    logFilePtr.p->logFileStatus = LogFileRecord::CLOSED;
    release(signal, m_redo_open_file_cache);
    return;
#endif
  case LogFileRecord::CLOSING_SR_FRONTPAGE:
    jam();
    closingSrFrontPage(signal);
    return;
  default:
    jam();
    systemErrorLab(signal, __LINE__);
    return;
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
  case LogFileRecord::OPEN_SR_READ_INVALIDATE_PAGES:
    jam();
    logFilePtr.p->logFileStatus = LogFileRecord::OPEN;
    readFileInInvalidate(signal, 0);
    return;
  case LogFileRecord::OPEN_SR_READ_INVALIDATE_SEARCH_FILES:
    jam();
    logFilePtr.p->logFileStatus = LogFileRecord::OPEN;
    readFileInInvalidate(signal, 5);
    return;
  case LogFileRecord::OPEN_SR_WRITE_INVALIDATE_PAGES:
    jam();
    logFilePtr.p->logFileStatus = LogFileRecord::OPEN;
    writeFileInInvalidate(signal, 0);
    return;
  case LogFileRecord::OPENING_INIT:
    jam();
    logFilePtr.p->logFileStatus = LogFileRecord::OPEN;
    openFileInitLab(signal);
    return;
  case LogFileRecord::OPEN_SR_FRONTPAGE:
    jam();
    logFilePtr.p->logFileStatus = LogFileRecord::OPEN;
    openSrFrontpageLab(signal);
    return;
  case LogFileRecord::OPEN_SR_LAST_FILE:
    jam();
    logFilePtr.p->logFileStatus = LogFileRecord::OPEN;
    openSrLastFileLab(signal);
    return;
  case LogFileRecord::OPEN_SR_NEXT_FILE:
    jam();
    logFilePtr.p->logFileStatus = LogFileRecord::OPEN;
    openSrNextFileLab(signal);
    return;
  case LogFileRecord::OPEN_EXEC_SR_START:
    jam();
    logFilePtr.p->logFileStatus = LogFileRecord::OPEN;
    openExecSrStartLab(signal);
    return;
  case LogFileRecord::OPEN_EXEC_SR_NEW_MBYTE:
    jam();
    logFilePtr.p->logFileStatus = LogFileRecord::OPEN;
    openExecSrNewMbyteLab(signal);
    return;
  case LogFileRecord::OPEN_SR_FOURTH_PHASE:
    jam();
    logFilePtr.p->logFileStatus = LogFileRecord::OPEN;
    openSrFourthPhaseLab(signal);
    return;
  case LogFileRecord::OPEN_SR_FOURTH_NEXT:
    jam();
    logFilePtr.p->logFileStatus = LogFileRecord::OPEN;
    openSrFourthNextLab(signal);
    return;
  case LogFileRecord::OPEN_SR_FOURTH_ZERO:
    jam();
    logFilePtr.p->logFileStatus = LogFileRecord::OPEN;
    openSrFourthZeroLab(signal);
    return;
  case LogFileRecord::OPENING_WRITE_LOG:
    jam();
    logFilePtr.p->logFileStatus = LogFileRecord::OPEN;
    return;
  case LogFileRecord::OPEN_EXEC_LOG:
    jam();
    logFilePtr.p->logFileStatus = LogFileRecord::OPEN;
#ifndef NO_REDO_OPEN_FILE_CACHE
    {
      jam();
      m_redo_open_file_cache.m_lru.addFirst(logFilePtr);
    }
    // Fall through
  case LogFileRecord::OPEN_EXEC_LOG_CACHED:
    jam();
#endif
    openExecLogLab(signal);
    return;
  default:
    jam();
    systemErrorLab(signal, __LINE__);
    return;
  }//switch
}//Dblqh::execFSOPENCONF()

void
Dblqh::execFSOPENREF(Signal* signal)
{
  jamEntry();
  FsRef* ref = (FsRef*)signal->getDataPtr();
  Uint32 err = ref->errorCode;
  if (err == FsRef::fsErrInvalidFileSize)
  {
    char buf[256];
    BaseString::snprintf(buf, sizeof(buf),
                         "Invalid file size for redo logfile, "
                         " size only changable with --initial");
    progError(__LINE__,
              NDBD_EXIT_INVALID_CONFIG,
              buf);
    return;
  }

  SimulatedBlock::execFSOPENREF(signal);
}

/* ************>> */
/*  FSREADCONF  > */
/* ************>> */
void Dblqh::execFSREADCONF(Signal* signal) 
{
  jamEntry();
  initFsrwconf(signal, false);

  switch (lfoPtr.p->lfoState) {
  case LogFileOperationRecord::READ_SR_LAST_MBYTE:
    jam();
    releaseLfo(signal);
    readSrLastMbyteLab(signal);
    return;
  case LogFileOperationRecord::READ_SR_FRONTPAGE:
    jam();
    releaseLfo(signal);
    readSrFrontpageLab(signal);
    return;
  case LogFileOperationRecord::READ_SR_LAST_FILE:
    jam();
    releaseLfo(signal);
    readSrLastFileLab(signal);
    return;
  case LogFileOperationRecord::READ_SR_NEXT_FILE:
    jam();
    releaseLfo(signal);
    readSrNextFileLab(signal);
    return;
  case LogFileOperationRecord::READ_EXEC_SR:
    jam();
    readExecSrLab(signal);
    return;
  case LogFileOperationRecord::READ_EXEC_LOG:
    jam();
    readExecLogLab(signal);
    return;
  case LogFileOperationRecord::READ_SR_INVALIDATE_PAGES:
    jam();
    invalidateLogAfterLastGCI(signal);
    return;
  case LogFileOperationRecord::READ_SR_INVALIDATE_SEARCH_FILES:
    jam();
    invalidateLogAfterLastGCI(signal);
    return;
  case LogFileOperationRecord::READ_SR_FOURTH_PHASE:
    jam();
    releaseLfo(signal);
    readSrFourthPhaseLab(signal);
    return;
  case LogFileOperationRecord::READ_SR_FOURTH_ZERO:
    jam();
    releaseLfo(signal);
    readSrFourthZeroLab(signal);
    return;
  default:
    jam();
    systemErrorLab(signal, __LINE__);
    return;
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
    jam();
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
  initFsrwconf(signal, true);
  switch (lfoPtr.p->lfoState) {
  case LogFileOperationRecord::WRITE_SR_INVALIDATE_PAGES:
    jam();
    invalidateLogAfterLastGCI(signal);
    CRASH_INSERTION(5047);
    return;
  case LogFileOperationRecord::WRITE_PAGE_ZERO:
    jam();
    writePageZeroLab(signal, __LINE__);
    releaseLfo(signal);
    return;
  case LogFileOperationRecord::LAST_WRITE_IN_FILE:
    jam();
    lastWriteInFileLab(signal);
    return;
  case LogFileOperationRecord::INIT_WRITE_AT_END:
    jam();
    initWriteEndLab(signal);
    return;
  case LogFileOperationRecord::INIT_FIRST_PAGE:
    jam();
    logMBytesInitDone++;
    initFirstPageLab(signal);
    return;
  case LogFileOperationRecord::WRITE_GCI_ZERO:
    jam();
    writeGciZeroLab(signal);
    return;
  case LogFileOperationRecord::WRITE_DIRTY:
    jam();
    writeDirtyLab(signal);
    return;
  case LogFileOperationRecord::WRITE_INIT_MBYTE:
    jam();
    logMBytesInitDone++;
    writeInitMbyteLab(signal);
    return;
  case LogFileOperationRecord::ACTIVE_WRITE_LOG:
    jam();
    writeLogfileLab(signal);
    return;
  case LogFileOperationRecord::FIRST_PAGE_WRITE_IN_LOGFILE:
    jam();
    firstPageWriteLab(signal);
    return;
  case LogFileOperationRecord::WRITE_SR_INVALIDATE_PAGES_UPDATE_PAGE0:
    jam();
    // We are done...send completed signal and exit this phase.
    releaseLfo(signal);
    signal->theData[0] = ZSR_FOURTH_COMP;
    signal->theData[1] = logPartPtr.i;
    sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
    return;
  default:
    jam();
    systemErrorLab(signal, __LINE__);
    return;
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
    systemErrorLab(signal, __LINE__);
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
}//Dblqh::initFsopenconf()

/* ========================================================================= */
/* =======       INITIATE WHEN RECEIVING FSREADCONF AND FSWRITECONF  ======= */
/*                                                                           */
/* ========================================================================= */
void Dblqh::initFsrwconf(Signal* signal, bool write) 
{
  LogPageRecordPtr logP;
  Uint32 noPages, totPages;
  lfoPtr.i = signal->theData[0];
  ptrCheckGuard(lfoPtr, clfoFileSize, logFileOperationRecord);
  totPages= lfoPtr.p->noPagesRw;
  logFilePtr.i = lfoPtr.p->logFileRec;
  ptrCheckGuard(logFilePtr, clogFileFileSize, logFileRecord);
  logPartPtr.i = logFilePtr.p->logPartRec;
  ptrCheckGuard(logPartPtr, clogPartFileSize, logPartRecord);
  logPagePtr.i = lfoPtr.p->firstLfoPage;
  ptrCheckGuard(logPagePtr, clogPageFileSize, logPageRecord);
  logP= logPagePtr;
  noPages= 1;
  ndbassert(totPages > 0);

  if (write)
  {
    Uint32 bytesWritten = totPages * 32768;
    logPartPtr.p->m_io_tracker.complete_io(bytesWritten);
  }

  for (;;)
  {
    logP.p->logPageWord[ZPOS_IN_WRITING]= 0;
    logP.p->logPageWord[ZPOS_IN_FREE_LIST]= 0;
    if (noPages == totPages)
      return;
    if (write)
      logP.i= logP.p->logPageWord[ZNEXT_PAGE];
    else
      logP.i= lfoPtr.p->logPageArray[noPages];
    ptrCheckGuard(logP, clogPageFileSize, logPageRecord);
    noPages++;
  }

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
    if (true) // less merge conflicts
    {
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
          completedLogPage(signal, ZENFORCE_WRITE, __LINE__);
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
            writeSinglePage(signal, logFilePtr.p->currentFilepage,
                            wordWritten, __LINE__);
            lfoPtr.p->lfoState = LogFileOperationRecord::ACTIVE_WRITE_LOG;
          }//if
        }//if
      }//if
    }
  }

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
    systemErrorLab(signal, __LINE__);
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
      // use writePageZeroLab to make sure that same code as normal is run
      writePageZeroLab(signal, __LINE__);
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
      writeSinglePage(signal, 0, ZPAGE_SIZE - 1, __LINE__);
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
        closeFile(signal, logFilePtr, __LINE__);
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
      // use writePageZeroLab to make sure that same code as normal is run
      writePageZeroLab(signal, __LINE__);
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
      writeSinglePage(signal, 0, ZPAGE_SIZE - 1, __LINE__);
      lfoPtr.p->logFileRec = currLogFile;
      lfoPtr.p->lfoState = LogFileOperationRecord::WRITE_PAGE_ZERO;
      return;
    }//if
  }//if
}//Dblqh::lastWriteInFileLab()

void Dblqh::writePageZeroLab(Signal* signal, Uint32 from) 
{
  if ((logPartPtr.p->m_log_problems & LogPartRecord::P_FILE_CHANGE_PROBLEM)!= 0)
  {
    jam();
    update_log_problem(signal, logPartPtr,
                       LogPartRecord::P_FILE_CHANGE_PROBLEM,
                       /* clear */ false);
  }

  logFilePtr.p->fileChangeState = LogFileRecord::NOT_ONGOING;

/*---------------------------------------------------------------------------*/
/* IT COULD HAVE ARRIVED PAGE WRITES TO THE CURRENT FILE WHILE WE WERE       */
/* WAITING FOR THIS DISK WRITE TO COMPLETE. THEY COULD NOT CHECK FOR         */
/* COMPLETED GLOBAL CHECKPOINTS. THUS WE SHOULD DO THAT NOW INSTEAD.         */
/*---------------------------------------------------------------------------*/
  bool res = checkGcpCompleted(signal,
                               logFilePtr.p->lastPageWritten,
                               logFilePtr.p->lastWordWritten);
  if (res && false)
  {
    gcpPtr.i = ccurrentGcprec;
    ptrCheckGuard(gcpPtr, cgcprecFileSize, gcpRecord);
    
    infoEvent("KESO completing GCP %u in writePageZeroLab from %u", 
              gcpPtr.p->gcpId, from);
  }
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
  if (m_use_om_init == 0)
  {
    jam();
    initLogpage(signal);
    writeSinglePage(signal, (clogFileSize * ZPAGES_IN_MBYTE) - 1,
                    ZPAGE_SIZE - 1, __LINE__, false);
    lfoPtr.p->lfoState = LogFileOperationRecord::INIT_WRITE_AT_END;
  }
  else
  {
    jam();
    seizeLfo(signal);
    initWriteEndLab(signal);
  }
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
/* Set number of log parts used to ensure we use correct number of log parts */
/* at system restart. Was previously hardcoded to 4.                         */
/*---------------------------------------------------------------------------*/
    logPagePtr.p->logPageWord[ZPOS_NO_LOG_PARTS]= globalData.ndbLogParts;
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
    writeSinglePage(signal, 1, ZPAGE_SIZE - 1, __LINE__, false);
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
  if (logFilePtr.p->currentMbyte == clogFileSize) {
    jam();
    releaseLogpage(signal);
    logFilePtr.p->logFileStatus = LogFileRecord::CLOSING_INIT;
    closeFile(signal, logFilePtr, __LINE__);
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

  if (m_use_om_init && ++logPartPtr.i != clogPartFileSize)
  {
    jam();
    ptrAss(logPartPtr, logPartRecord);
    logFilePtr.i = logPartPtr.p->firstLogfile;
    ptrCheckGuard(logFilePtr, clogFileFileSize, logFileRecord);
    openLogfileInit(signal);
    return;
  }

  for (logPartPtr.i = 0; logPartPtr.i < clogPartFileSize; logPartPtr.i++)
  {
    jam();
    ptrAss(logPartPtr, logPartRecord);
    if (logPartPtr.p->logPartState != LogPartRecord::SR_FIRST_PHASE_COMPLETED)
    {
      jam();
/*---------------------------------------------------------------------------*/
/* THIS PART HAS STILL NOT COMPLETED. WAIT FOR THIS TO OCCUR.                */
/*---------------------------------------------------------------------------*/
      return;
    }//if
  }

#ifdef VM_TRACE
  enable_global_variables();
#endif
  logfileInitCompleteReport(signal);
  sendNdbSttorryLab(signal);
}

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
  tilTmp = (tilTmp << 8) + (8 + logPartPtr.p->logPartNo); /* DIRECTORY = D(8+Part)/DBLQH */
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
  for (tilIndex = 0; tilIndex < clogFileSize; tilIndex++) {
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

  /* Ensure all non-used header words are zero */
  bzero(logPagePtr.p, sizeof(Uint32) * ZPAGE_HEADER_SIZE);
  logPagePtr.p->logPageWord[ZPOS_LOG_LAP] = logPartPtr.p->logLap;
  logPagePtr.p->logPageWord[ZPOS_MAX_GCI_COMPLETED] = 
        logPartPtr.p->logPartNewestCompletedGCI;
  logPagePtr.p->logPageWord[ZPOS_MAX_GCI_STARTED] = cnewestGci;
  logPagePtr.p->logPageWord[ZPOS_VERSION] = NDB_VERSION;
  logPagePtr.p->logPageWord[ZPOS_NO_LOG_FILES] = logPartPtr.p->noLogFiles;
  logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] = ZPAGE_HEADER_SIZE;
  logPagePtr.p->logPageWord[ZPOS_NO_LOG_PARTS]= globalData.ndbLogParts;
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
void Dblqh::openFileRw(Signal* signal,
                       LogFileRecordPtr olfLogFilePtr,
                       bool writeBuffer)
{
  FsOpenReq* req = (FsOpenReq*)signal->getDataPtrSend();
  signal->theData[0] = cownref;
  signal->theData[1] = olfLogFilePtr.i;
  signal->theData[2] = olfLogFilePtr.p->fileName[0];
  signal->theData[3] = olfLogFilePtr.p->fileName[1];
  signal->theData[4] = olfLogFilePtr.p->fileName[2];
  signal->theData[5] = olfLogFilePtr.p->fileName[3];
  signal->theData[6] = FsOpenReq::OM_READWRITE | FsOpenReq::OM_AUTOSYNC | FsOpenReq::OM_CHECK_SIZE;
  if (c_o_direct)
    signal->theData[6] |= FsOpenReq::OM_DIRECT;
  if (writeBuffer)
    signal->theData[6] |= FsOpenReq::OM_WRITE_BUFFER;

  req->auto_sync_size = MAX_REDO_PAGES_WITHOUT_SYNCH * sizeof(LogPageRecord);
  Uint64 sz = clogFileSize;
  sz *= 1024; sz *= 1024;
  req->file_size_hi = (Uint32)(sz >> 32);
  req->file_size_lo = (Uint32)(sz & 0xFFFFFFFF);
  sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, FsOpenReq::SignalLength, JBA);
}//Dblqh::openFileRw()

/* ------------------------------------------------------------------------- */
/* -------               OPEN LOG FILE DURING INITIAL START          ------- */
/*                                                                           */
/*       SUBROUTINE SHORT NAME = OLI                                         */
/* ------------------------------------------------------------------------- */
void Dblqh::openLogfileInit(Signal* signal) 
{
  logFilePtr.p->logFileStatus = LogFileRecord::OPENING_INIT;
  FsOpenReq* req = (FsOpenReq*)signal->getDataPtrSend();
  signal->theData[0] = cownref;
  signal->theData[1] = logFilePtr.i;
  signal->theData[2] = logFilePtr.p->fileName[0];
  signal->theData[3] = logFilePtr.p->fileName[1];
  signal->theData[4] = logFilePtr.p->fileName[2];
  signal->theData[5] = logFilePtr.p->fileName[3];
  signal->theData[6] = FsOpenReq::OM_READWRITE | FsOpenReq::OM_TRUNCATE | FsOpenReq::OM_CREATE | FsOpenReq::OM_AUTOSYNC | FsOpenReq::OM_WRITE_BUFFER;
  if (c_o_direct)
    signal->theData[6] |= FsOpenReq::OM_DIRECT;

  Uint64 sz = Uint64(clogFileSize) * 1024 * 1024;
  req->file_size_hi = Uint32(sz >> 32);
  req->file_size_lo = Uint32(sz);
  req->page_size = File_formats::NDB_PAGE_SIZE;
  if (m_use_om_init)
  {
    jam();
    signal->theData[6] |= FsOpenReq::OM_INIT;
  }

  req->auto_sync_size = MAX_REDO_PAGES_WITHOUT_SYNCH * sizeof(LogPageRecord);
  sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, FsOpenReq::SignalLength, JBA);
}//Dblqh::openLogfileInit()

void
Dblqh::execFSWRITEREQ(Signal* signal)
{
  /**
   * This is currently run in other thread -> no jam
   *   and no global variables
   */
  Ptr<GlobalPage> page_ptr;
  FsReadWriteReq* req= (FsReadWriteReq*)signal->getDataPtr();
  m_shared_page_pool.getPtr(page_ptr, req->data.pageData[0]);

  LogFileRecordPtr currLogFilePtr;
  currLogFilePtr.i = req->userPointer;
  ptrCheckGuard(currLogFilePtr, clogFileFileSize, logFileRecord);

  LogPartRecordPtr currLogPartPtr;
  currLogPartPtr.i = currLogFilePtr.p->logPartRec;
  ptrCheckGuard(currLogPartPtr, clogPartFileSize, logPartRecord);

  Uint32 page_no = req->varIndex;
  LogPageRecordPtr currLogPagePtr;
  currLogPagePtr.p = (LogPageRecord*)page_ptr.p;

  bzero(page_ptr.p, sizeof(LogPageRecord));
  if (page_no == 0)
  {
    // keep writing these afterwards
  }
  else if (((page_no % ZPAGES_IN_MBYTE) == 0) ||
           (page_no == ((clogFileSize * ZPAGES_IN_MBYTE) - 1)))
  {
    currLogPagePtr.p->logPageWord[ZPOS_LOG_LAP] = currLogPartPtr.p->logLap;
    currLogPagePtr.p->logPageWord[ZPOS_MAX_GCI_COMPLETED] =
      currLogPartPtr.p->logPartNewestCompletedGCI;
    currLogPagePtr.p->logPageWord[ZPOS_MAX_GCI_STARTED] = cnewestGci;
    currLogPagePtr.p->logPageWord[ZPOS_VERSION] = NDB_VERSION;
    currLogPagePtr.p->logPageWord[ZPOS_NO_LOG_FILES] =
      currLogPartPtr.p->noLogFiles;
    currLogPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] = ZPAGE_HEADER_SIZE;
    currLogPagePtr.p->logPageWord[ZLAST_LOG_PREP_REF] =
      (currLogFilePtr.p->fileNo << 16) +
      (currLogFilePtr.p->currentFilepage >> ZTWOLOG_NO_PAGES_IN_MBYTE);

    currLogPagePtr.p->logPageWord[ZNEXT_PAGE] = RNIL;
    currLogPagePtr.p->logPageWord[ZPOS_CHECKSUM] =
      calcPageCheckSum(currLogPagePtr);
  }
  else if (0)
  {
    currLogPagePtr.p->logPageWord[ZNEXT_PAGE] = RNIL;
    currLogPagePtr.p->logPageWord[ZPOS_CHECKSUM] =
      calcPageCheckSum(currLogPagePtr);
  }
}

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
    FsOpenReq* req = (FsOpenReq*)signal->getDataPtrSend();
    signal->theData[0] = cownref;
    signal->theData[1] = onlLogFilePtr.i;
    signal->theData[2] = onlLogFilePtr.p->fileName[0];
    signal->theData[3] = onlLogFilePtr.p->fileName[1];
    signal->theData[4] = onlLogFilePtr.p->fileName[2];
    signal->theData[5] = onlLogFilePtr.p->fileName[3];
    signal->theData[6] = FsOpenReq::OM_READWRITE | FsOpenReq::OM_AUTOSYNC | FsOpenReq::OM_CHECK_SIZE | FsOpenReq::OM_WRITE_BUFFER;
    if (c_o_direct)
      signal->theData[6] |= FsOpenReq::OM_DIRECT;
    req->auto_sync_size = MAX_REDO_PAGES_WITHOUT_SYNCH * sizeof(LogPageRecord);
    Uint64 sz = clogFileSize;
    sz *= 1024; sz *= 1024;
    req->file_size_hi = (Uint32)(sz >> 32);
    req->file_size_lo = (Uint32)(sz & 0xFFFFFFFF);
    sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, FsOpenReq::SignalLength, JBA);
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
  logPagePtr.i = lfoPtr.p->firstLfoPage;
  while (logPagePtr.i != RNIL)
  {
    ptrCheckGuard(logPagePtr, clogPageFileSize, logPageRecord);
    Uint32 tmp = logPagePtr.p->logPageWord[ZNEXT_PAGE];
    releaseLogpage(signal);
    logPagePtr.i = tmp;
  }
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
  ndbrequire(logPagePtr.p->logPageWord[ZPOS_IN_FREE_LIST] == 0);
#endif

  cnoOfLogPages++;
  logPagePtr.p->logPageWord[ZNEXT_PAGE] = cfirstfreeLogPage;
  logPagePtr.p->logPageWord[ZPOS_IN_WRITING]= 0;
  logPagePtr.p->logPageWord[ZPOS_IN_FREE_LIST]= 1;
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
#ifdef VM_TRACE
  bzero(logPagePtr.p, sizeof(LogPageRecord));
#endif
  logPagePtr.p->logPageWord[ZNEXT_PAGE] = RNIL;
  logPagePtr.p->logPageWord[ZPOS_IN_FREE_LIST] = 0;
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
  arrGuard(logFilePtr.p->currentMbyte, clogFileSize);
  if (DEBUG_REDO)
  {
    printf("part: %u file: %u setting logMaxGciCompleted[%u] = %u logMaxGciStarted[%u]: %u lastPrepRef[%u]: ",
           logPartPtr.p->logPartNo,
           logFilePtr.p->fileNo,
           logFilePtr.p->currentMbyte,
           logPartPtr.p->logPartNewestCompletedGCI,
           logFilePtr.p->currentMbyte,
           cnewestGci,
           logFilePtr.p->currentMbyte);
    if (logPartPtr.p->firstLogTcrec == RNIL)
    {
      ndbout_c("file: %u mb: %u (RNIL)",
               logFilePtr.p->fileNo,
               logFilePtr.p->currentMbyte);
    }
    else
    {
      wfdTcConnectptr.i = logPartPtr.p->firstLogTcrec;
      ptrCheckGuard(wfdTcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
      ndbout_c("file: %u mb: %u",
               wfdTcConnectptr.p->logStartFileNo,
               wfdTcConnectptr.p->logStartPageNo >> ZTWOLOG_NO_PAGES_IN_MBYTE);
    }
  }
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
  UintR twmoNoLogDescriptors;

/* -------------------------------------------------- */
/*       WRITE HEADER INFORMATION IN THE NEW FILE.    */
/* -------------------------------------------------- */
  logPagePtr.p->logPageWord[ZPAGE_HEADER_SIZE + ZPOS_LOG_TYPE] = ZFD_TYPE;
  logPagePtr.p->logPageWord[ZPAGE_HEADER_SIZE + ZPOS_FILE_NO] = 
    logFilePtr.p->fileNo;
  /* 
   * When writing a file header on open, we write cmaxLogFilesInPageZero,
   * though the entries for the first file (this file), will be invalid,
   * as we do not know e.g. which GCIs will be included by log records
   * in the MBs in this file.  On the first lap these will be initial values
   * on subsequent laps, they will be values from the previous lap.
   * We take care when reading these values back, not to use the values for
   * the current file.
   */
  if (logPartPtr.p->noLogFiles > cmaxLogFilesInPageZero) {
    jam();
    twmoNoLogDescriptors = cmaxLogFilesInPageZero;
  } else {
    jam();
    twmoNoLogDescriptors = logPartPtr.p->noLogFiles;
  }//if
  logPagePtr.p->logPageWord[ZPAGE_HEADER_SIZE + ZPOS_NO_FD] = 
    twmoNoLogDescriptors;

  {
    Uint32 pos = ZPAGE_HEADER_SIZE + ZFD_HEADER_SIZE;
    LogFileRecordPtr filePtr = logFilePtr;
    for (Uint32 fd = 0; fd < twmoNoLogDescriptors; fd++)
    {
      jam();
      ptrCheckGuard(filePtr, clogFileFileSize, logFileRecord);
      for (Uint32 mb = 0; mb < clogFileSize; mb ++)
      {
        jam();
        Uint32 pos0 = pos + fd * (ZFD_MBYTE_SIZE * clogFileSize) + mb;
        Uint32 pos1 = pos0 + clogFileSize;
        Uint32 pos2 = pos1 + clogFileSize;
        arrGuard(pos0, ZPAGE_SIZE);
        arrGuard(pos1, ZPAGE_SIZE);
        arrGuard(pos2, ZPAGE_SIZE);
        logPagePtr.p->logPageWord[pos0] = filePtr.p->logMaxGciCompleted[mb];
        logPagePtr.p->logPageWord[pos1] = filePtr.p->logMaxGciStarted[mb];
        logPagePtr.p->logPageWord[pos2] = filePtr.p->logLastPrepRef[mb];
      }
      filePtr.i = filePtr.p->prevLogFile;
    }
    pos += (twmoNoLogDescriptors * ZFD_MBYTE_SIZE * clogFileSize);
    arrGuard(pos, ZPAGE_SIZE);
    logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] = pos;
    logPagePtr.p->logPageWord[pos] = ZNEXT_LOG_RECORD_TYPE;
  }

/* ------------------------------------------------------- */
/*       THIS IS A SPECIAL WRITE OF THE FIRST PAGE IN THE  */
/*       LOG FILE. THIS HAS SPECIAL SIGNIFANCE TO FIND     */
/*       THE END OF THE LOG AT SYSTEM RESTART.             */
/* ------------------------------------------------------- */
  if (wmoType == ZINIT) {
    jam();
    writeSinglePage(signal, 0, ZPAGE_SIZE - 1, __LINE__, false);
    lfoPtr.p->lfoState = LogFileOperationRecord::INIT_FIRST_PAGE;
  } else {
    jam();
    writeSinglePage(signal, 0, ZPAGE_SIZE - 1, __LINE__, true);
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
  if (m_use_om_init == 0)
  {
    jam();
    initLogpage(signal);
    writeSinglePage(signal, logFilePtr.p->currentMbyte * ZPAGES_IN_MBYTE,
                    ZPAGE_SIZE - 1, __LINE__, false);
    lfoPtr.p->lfoState = LogFileOperationRecord::WRITE_INIT_MBYTE;
    checkReportStatus(signal);
  }
  else
  {
    jam();
    seizeLfo(signal);
    logFilePtr.p->currentMbyte = clogFileSize - 1;
    writeInitMbyteLab(signal);
  }
}//Dblqh::writeInitMbyte()

/* ------------------------------------------------------------------------- */
/* -------               WRITE A SINGLE PAGE INTO A FILE             ------- */
/*                                                                           */
/*       INPUT:          TWSP_PAGE_NO    THE PAGE NUMBER WRITTEN             */
/*       SUBROUTINE SHORT NAME:  WSP                                         */
/* ------------------------------------------------------------------------- */
void Dblqh::writeSinglePage(Signal* signal, Uint32 pageNo,
                            Uint32 wordWritten, Uint32 place,
                            bool sync) 
{
  seizeLfo(signal);
  initLfo(signal);
  lfoPtr.p->firstLfoPage = logPagePtr.i;
  logPagePtr.p->logPageWord[ZNEXT_PAGE] = RNIL;

  writeDbgInfoPageHeader(logPagePtr, place, pageNo, wordWritten);
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
  signal->theData[3] = sync ? ZLIST_OF_PAIRS_SYNCH : ZLIST_OF_PAIRS;
  signal->theData[4] = ZVAR_NO_LOG_PAGE_WORD;
  signal->theData[5] = 1;                     /* ONE PAGE WRITTEN */
  signal->theData[6] = logPagePtr.i;
  signal->theData[7] = pageNo;
  sendSignal(NDBFS_REF, GSN_FSWRITEREQ, signal, 8, JBA);

  ndbrequire(logFilePtr.p->fileRef != RNIL);

  logPartPtr.p->m_io_tracker.send_io(32768);

  if (DEBUG_REDO)
  {
    ndbout_c("writeSingle 1 page at part: %u file: %u page: %u (mb: %u)",
             logPartPtr.p->logPartNo,
             logFilePtr.p->fileNo,
             pageNo,
             pageNo >> ZTWOLOG_NO_PAGES_IN_MBYTE);
  }
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
  Uint32 num_parts_used;
  if (!ndb_configurable_log_parts(logPagePtr.p->logPageWord[ZPOS_VERSION])) {
    jam();
    num_parts_used= 4;
  }
  else
  {
    jam();
    num_parts_used = logPagePtr.p->logPageWord[ZPOS_NO_LOG_PARTS];
  }
  /* Verify that number of log parts >= number of LQH workers */
  if (globalData.ndbMtLqhWorkers > num_parts_used) {
    char buf[255];
    BaseString::snprintf(buf, sizeof(buf),
      "Trying to start %d LQH workers with only %d log parts, try initial"
      " node restart to be able to use more LQH workers.",
      globalData.ndbMtLqhWorkers, num_parts_used);
    progError(__LINE__, NDBD_EXIT_INVALID_CONFIG, buf);
  }
  if (num_parts_used != globalData.ndbLogParts)
  {
    char buf[255];
    BaseString::snprintf(buf, sizeof(buf),
      "Can only change NoOfLogParts through initial node restart, old"
      " value of NoOfLogParts = %d, tried using %d",
      num_parts_used, globalData.ndbLogParts);
    progError(__LINE__, NDBD_EXIT_INVALID_CONFIG, buf);
  }

  Uint32 fileNo = logPagePtr.p->logPageWord[ZPAGE_HEADER_SIZE + ZPOS_FILE_NO];
  /* ------------------------------------------------------------------------
   *    CLOSE FILE 0 SO THAT WE HAVE CLOSED ALL FILES WHEN STARTING TO READ 
   *    THE FRAGMENT LOG. ALSO RELEASE PAGE ZERO.
   * ------------------------------------------------------------------------ */
  releaseLogpage(signal);
  logFilePtr.p->logFileStatus = LogFileRecord::CLOSING_SR_FRONTPAGE;
  closeFile(signal, logFilePtr, __LINE__);
  /* Lookup index of last file */
  LogFileRecordPtr locLogFilePtr;
  findLogfile(signal, fileNo, logPartPtr, &locLogFilePtr);
  
  /* Store in logPart record for use once file 0 is closed */
  logPartPtr.p->srLastFileIndex = locLogFilePtr.i;
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
  if (DEBUG_REDO)
  {
    ndbout_c("readSrLastFileLab part: %u logExecState: %u logPartState: %u logLap: %u",
             logPartPtr.p->logPartNo,
             logPartPtr.p->logExecState,
             logPartPtr.p->logPartState,
             logPartPtr.p->logLap);
  }
  if (logPartPtr.p->noLogFiles > cmaxValidLogFilesInPageZero) {
    jam();
    initGciInLogFileRec(signal, cmaxValidLogFilesInPageZero);
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
  if (logPartPtr.p->lastMbyte == ZNIL)
  {
    if (logPagePtr.p->logPageWord[ZPOS_LOG_LAP] < logPartPtr.p->logLap) {
      jam();
      logPartPtr.p->lastMbyte = logFilePtr.p->currentMbyte - 1;
      if (DEBUG_REDO)
      {
        ndbout_c("readSrLastMbyteLab part: %u file: %u lastMbyte: %u",
                 logPartPtr.p->logPartNo, 
                 logFilePtr.p->fileNo,
                 logPartPtr.p->lastMbyte);
      }
    }//if
  }//if
  arrGuard(logFilePtr.p->currentMbyte, clogFileSize);
  logFilePtr.p->logMaxGciCompleted[logFilePtr.p->currentMbyte] = 
    logPagePtr.p->logPageWord[ZPOS_MAX_GCI_COMPLETED];
  logFilePtr.p->logMaxGciStarted[logFilePtr.p->currentMbyte] = 
    logPagePtr.p->logPageWord[ZPOS_MAX_GCI_STARTED];
  logFilePtr.p->logLastPrepRef[logFilePtr.p->currentMbyte] = 
    logPagePtr.p->logPageWord[ZLAST_LOG_PREP_REF];
  releaseLogpage(signal);
  if (logFilePtr.p->currentMbyte < (clogFileSize - 1)) {
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
      logPartPtr.p->lastMbyte = clogFileSize - 1;
    }//if
  }//if
  logFilePtr.p->logFileStatus = LogFileRecord::CLOSING_SR;
  closeFile(signal, logFilePtr, __LINE__);

  /* Head file is initialised by reading per-MB headers rather than per-file
   * headers.  Therefore, when stepping back through the redo files to get
   * the previous file's metadata, we must be careful not to read the 
   * per-file header info over the just-read per-MB headers, invalidating
   * the head metainfo.
   */
  Uint32 nonHeadFileCount = logPartPtr.p->noLogFiles - 1;

  if (logPartPtr.p->noLogFiles > cmaxValidLogFilesInPageZero) {
    /* Step back from head to get file:mb metadata from a 
     * previous file's page zero
     */
    Uint32 fileNo;
    if (logFilePtr.p->fileNo >= cmaxValidLogFilesInPageZero) {
      jam();
      fileNo = logFilePtr.p->fileNo - cmaxValidLogFilesInPageZero;
    } else {
      /* Wrap at 0:0 */
      jam();
      fileNo = 
	(logPartPtr.p->noLogFiles + logFilePtr.p->fileNo) - 
	cmaxValidLogFilesInPageZero;
    }//if

    jam();
    logPartPtr.p->srRemainingFiles = 
      nonHeadFileCount - cmaxValidLogFilesInPageZero;

    /* Check we're making progress */
    ndbrequire(fileNo != logFilePtr.p->fileNo);
    LogFileRecordPtr locLogFilePtr;
    findLogfile(signal, fileNo, logPartPtr, &locLogFilePtr);
    ndbrequire(locLogFilePtr.p->logFileStatus == LogFileRecord::CLOSED);
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
  if (logPartPtr.p->srRemainingFiles > cmaxValidLogFilesInPageZero) {
    jam();
    initGciInLogFileRec(signal, cmaxValidLogFilesInPageZero);
  } else {
    jam();
    initGciInLogFileRec(signal, logPartPtr.p->srRemainingFiles);
  }//if
  releaseLogpage(signal);
  logFilePtr.p->logFileStatus = LogFileRecord::CLOSING_SR;
  closeFile(signal, logFilePtr, __LINE__);
  if (logPartPtr.p->srRemainingFiles > cmaxValidLogFilesInPageZero) {
    /* Step back from head to get file:mb metadata from a
     * previous file's page zero
     */
    Uint32 fileNo;
    if (logFilePtr.p->fileNo >= cmaxValidLogFilesInPageZero) {
      jam();
      fileNo = logFilePtr.p->fileNo - cmaxValidLogFilesInPageZero;
    } else {
      /* Wrap at 0:0 */
      jam();
      fileNo = 
	(logPartPtr.p->noLogFiles + logFilePtr.p->fileNo) - 
	cmaxValidLogFilesInPageZero;
    }//if

    jam();
    logPartPtr.p->srRemainingFiles = 
      logPartPtr.p->srRemainingFiles - cmaxValidLogFilesInPageZero;
    
    /* Check we're making progress */
    ndbrequire(fileNo != logFilePtr.p->fileNo);
    LogFileRecordPtr locLogFilePtr;
    findLogfile(signal, fileNo, logPartPtr, &locLogFilePtr);
    ndbrequire(locLogFilePtr.p->logFileStatus == LogFileRecord::CLOSED);
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

void Dblqh::closingSrFrontPage(Signal* signal)
{
  jam();
  /* Front page (file 0) has closed, now it's safe to continue
   * to read any page (including file 0) as part of restoring
   * redo metadata
   */
  logFilePtr.p->logFileStatus = LogFileRecord::CLOSED;
  logPartPtr.i = logFilePtr.p->logPartRec;
  ptrCheckGuard(logPartPtr, clogPartFileSize, logPartRecord);
  logFilePtr.i = logPartPtr.p->firstLogfile;
  
  /* Pre-restart head file index was stored in logPartPtr.p->srLastFileIndex
   * prior to closing this file, now let's use it...
   */
  ndbrequire(logPartPtr.p->srLastFileIndex != RNIL);
  
  LogFileRecordPtr oldHead;
  oldHead.i = logPartPtr.p->srLastFileIndex;
  ptrCheckGuard(oldHead, clogFileFileSize, logFileRecord);

  /* Reset srLastFileIndex */
  logPartPtr.p->srLastFileIndex = RNIL;
  
  /* And now open the head file to begin redo meta reload */
  oldHead.p->logFileStatus = LogFileRecord::OPEN_SR_LAST_FILE;
  openFileRw(signal, oldHead);
  return;
}

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
  for (logPartPtr.i = 0; logPartPtr.i < clogPartFileSize; logPartPtr.i++) {
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
  
  Uint32 lcpNo = startFragReq->lcpNo;
  Uint32 noOfLogNodes = startFragReq->noOfLogNodes;
  Uint32 lcpId = startFragReq->lcpId;
  Uint32 requestInfo = startFragReq->requestInfo;
  if (signal->getLength() < StartFragReq::SignalLength)
  {
    jam();
    requestInfo = StartFragReq::SFR_RESTORE_LCP;
  }

  bool doprint = false;
#ifdef ERROR_INSERT
  /**
   * Always printSTART_FRAG_REQ (for debugging) if ERROR_INSERT is set
   */
  doprint = true;
#endif
  if (doprint || noOfLogNodes > 1)
  {
    printSTART_FRAG_REQ(stdout, signal->getDataPtr(), signal->getLength(),
                        number());
  }

  ndbrequire(noOfLogNodes <= MAX_LOG_EXEC);
  fragptr.p->fragStatus = Fragrecord::CRASH_RECOVERING;
  fragptr.p->srBlockref = startFragReq->userRef;
  fragptr.p->srUserptr = startFragReq->userPtr;
  fragptr.p->srChkpnr = lcpNo;
  if (lcpNo == (MAX_LCP_STORED - 1)) {
    jam();
    fragptr.p->lcpId[lcpNo] = lcpId;
  } else if (lcpNo < (MAX_LCP_STORED - 1)) {
    jam();
    fragptr.p->lcpId[lcpNo] = lcpId;
  } else {
    ndbrequire(lcpNo == ZNIL);
    jam();
  }//if
  fragptr.p->srNoLognodes = noOfLogNodes;
  fragptr.p->logFlag = Fragrecord::STATE_FALSE;
  fragptr.p->srStatus = Fragrecord::SS_IDLE;

  if (requestInfo == StartFragReq::SFR_COPY_FRAG)
  {
    ndbrequire(lcpNo == ZNIL);
    Uint32 n = fragptr.p->srLqhLognode[0] = startFragReq->lqhLogNode[0]; // src
    ndbrequire(ndbd_non_trans_copy_frag_req(getNodeInfo(n).m_version));

    // Magic no, meaning to COPY_FRAGREQ instead of read from disk
    fragptr.p->srChkpnr = Z8NIL;
  }

  if (noOfLogNodes > 0) 
  {
    jam();
    for (Uint32 i = 0; i < noOfLogNodes; i++) {
      jam();
      fragptr.p->srStartGci[i] = startFragReq->startGci[i];
      fragptr.p->srLastGci[i] = startFragReq->lastGci[i];
      fragptr.p->srLqhLognode[i] = startFragReq->lqhLogNode[i];
    }//for
    fragptr.p->newestGci = startFragReq->lastGci[noOfLogNodes - 1];
  } 
  else
  {
    jam();
    /**
     * This is a really weird piece of code
     *   it's probably incorrect, but seems to mask problems...
     */
    if (cnewestGci > fragptr.p->newestGci) 
    {
      jam();
      fragptr.p->newestGci = cnewestGci;
    }
  }//if
  
  if (requestInfo == StartFragReq::SFR_COPY_FRAG)
  {
    jam();
  }
  else if (lcpNo == ZNIL)
  {
    jam();
    /**
     *  THERE WAS NO LOCAL CHECKPOINT AVAILABLE FOR THIS FRAGMENT. WE DO 
     *  NOT NEED TO READ IN THE LOCAL FRAGMENT. 
     */
    /**
     * Or this is not "first" fragment in table
     *   RESTORE_LCP_REQ will currently restore all fragments
     */
    c_lcp_complete_fragments.add(fragptr);

    c_tup->disk_restart_lcp_id(tabptr.i, fragId, RNIL);
    jamEntry();
    return;
  }
  else
  {
    jam();
    c_tup->disk_restart_lcp_id(tabptr.i, fragId, lcpId);
    jamEntry();

    if (ERROR_INSERTED(5055))
    {
      ndbrequire(c_lcpId == 0 || lcpId == 0 || c_lcpId == lcpId);
    }

    /**
     * Keep track of minimal lcp-id
     */
    c_lcpId = (c_lcpId == 0 ? lcpId : c_lcpId);
    c_lcpId = (c_lcpId < lcpId ? c_lcpId : lcpId);
  }

  c_lcp_waiting_fragments.add(fragptr);
  if(c_lcp_restoring_fragments.isEmpty())
    send_restore_lcp(signal);
}//Dblqh::execSTART_FRAGREQ()

void
Dblqh::send_restore_lcp(Signal * signal)
{
  c_lcp_waiting_fragments.first(fragptr);
  c_lcp_waiting_fragments.remove(fragptr);
  c_lcp_restoring_fragments.add(fragptr);

  if (fragptr.p->srChkpnr != Z8NIL)
  {
    jam();
    RestoreLcpReq* req= (RestoreLcpReq*)signal->getDataPtrSend();
    req->senderData = fragptr.i;
    req->senderRef = reference();
    req->tableId = fragptr.p->tabRef;
    req->fragmentId = fragptr.p->fragId;
    req->lcpNo = fragptr.p->srChkpnr;
    req->lcpId = fragptr.p->lcpId[fragptr.p->srChkpnr];
    BlockReference restoreRef = calcInstanceBlockRef(RESTORE);
    sendSignal(restoreRef, GSN_RESTORE_LCP_REQ, signal,
               RestoreLcpReq::SignalLength, JBB);
  }
  else
  {
    jam();

    tabptr.i = fragptr.p->tabRef;
    ptrCheckGuard(tabptr, ctabrecFileSize, tablerec);

    fragptr.p->fragStatus = Fragrecord::ACTIVE_CREATION;
    CopyFragReq * req = CAST_PTR(CopyFragReq, signal->getDataPtrSend());
    req->senderData = fragptr.i;
    req->senderRef = reference();
    req->tableId = fragptr.p->tabRef;
    req->fragId = fragptr.p->fragId;
    req->nodeId = getOwnNodeId();
    req->schemaVersion = tabptr.p->schemaVersion;
    req->distributionKey = 0;
    req->gci = fragptr.p->lcpId[0];
    req->nodeCount = 0;
    req->nodeList[1] = CopyFragReq::CFR_NON_TRANSACTIONAL;
    Uint32 instanceKey = fragptr.p->lqhInstanceKey;
    BlockReference ref = numberToRef(DBLQH, instanceKey,
                                     fragptr.p->srLqhLognode[0]);

    sendSignal(ref, GSN_COPY_FRAGREQ, signal,
               CopyFragReq::SignalLength, JBB);
  }
}

void
Dblqh::execCOPY_FRAGREF(Signal* signal)
{
  jamEntry();

  const CopyFragRef * ref = CAST_CONSTPTR(CopyFragRef, signal->getDataPtr());
  Uint32 errorCode = ref->errorCode;

  SystemError * sysErr = (SystemError*)&signal->theData[0];
  sysErr->errorCode = SystemError::CopyFragRefError;
  sysErr->errorRef = reference();
  sysErr->data[0] = errorCode;
  sysErr->data[1] = 0;
  sendSignal(NDBCNTR_REF, GSN_SYSTEM_ERROR, signal,
             SystemError::SignalLength, JBB);
}

void
Dblqh::execCOPY_FRAGCONF(Signal* signal)
{
  jamEntry();
  {
    const CopyFragConf* conf = CAST_CONSTPTR(CopyFragConf,
                                             signal->getDataPtr());
    c_fragment_pool.getPtr(fragptr, conf->senderData);
    fragptr.p->fragStatus = Fragrecord::CRASH_RECOVERING;

    Uint32 rows_lo = conf->rows_lo;
    Uint32 bytes_lo = conf->bytes_lo;
    signal->theData[0] = NDB_LE_NR_CopyFragDone;
    signal->theData[1] = getOwnNodeId();
    signal->theData[2] = fragptr.p->tabRef;
    signal->theData[3] = fragptr.p->fragId;
    signal->theData[4] = rows_lo;
    signal->theData[5] = 0;
    signal->theData[6] = bytes_lo;
    signal->theData[7] = 0;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 8, JBB);
  }

  {
    RestoreLcpConf* conf= (RestoreLcpConf*)signal->getDataPtr();
    conf->senderData = fragptr.i;
    execRESTORE_LCP_CONF(signal);
  }
}

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

void Dblqh::execRESTORE_LCP_REF(Signal* signal) 
{
  jamEntry();
  ndbrequire(false);
  return;
}

void Dblqh::execRESTORE_LCP_CONF(Signal* signal) 
{
  jamEntry();
  RestoreLcpConf* conf= (RestoreLcpConf*)signal->getDataPtr();
  fragptr.i = conf->senderData;
  c_fragment_pool.getPtr(fragptr);

  c_lcp_restoring_fragments.remove(fragptr);
  c_lcp_complete_fragments.add(fragptr);

  tabptr.i = fragptr.p->tabRef;
  ptrCheckGuard(tabptr, ctabrecFileSize, tablerec);

  if (!c_lcp_waiting_fragments.isEmpty())
  {
    send_restore_lcp(signal);
    return;
  }

  if (c_lcp_restoring_fragments.isEmpty() && 
      cstartRecReq == SRR_START_REC_REQ_ARRIVED)
  {
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
    lcpPtr.i = 0;
    ptrAss(lcpPtr, lcpRecord);
    lcpPtr.p->m_outstanding = 1;

    if (cstartType == NodeState::ST_INITIAL_NODE_RESTART)
    {
      jam();
      /**
       * Skip lgman undo...
       */
      signal->theData[0] = LGMAN_REF;
      sendSignal(reference(), GSN_START_RECCONF, signal, 1, JBB);
      return;
    }

    if (!isNdbMtLqh())
    {
      jam();
      signal->theData[0] = c_lcpId;
      sendSignal(LGMAN_REF, GSN_START_RECREQ, signal, 1, JBB);
    }
    else
    {
      jam();
      signal->theData[0] = c_lcpId;
      signal->theData[1] = LGMAN;
      sendSignal(DBLQH_REF, GSN_START_RECREQ, signal, 2, JBB);
    }
    return;
  }
}

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
  cstartRecReqData = req->senderData;

  if (check_ndb_versions())
  {
    ndbrequire(crestartOldestGci <= crestartNewestGci);
  }

  ndbrequire(req->receivingNodeId == cownNodeid);

  cnewestCompletedGci = cnewestGci;
  cstartRecReq = SRR_START_REC_REQ_ARRIVED; // StartRecReq has arrived
  
  if (signal->getLength() == StartRecReq::SignalLength)
  {
    jam();
    NdbNodeBitmask tmp;
    tmp.assign(NdbNodeBitmask::Size, req->sr_nodes);
    if (!tmp.equal(m_sr_nodes))
    {
      char buf0[100], buf1[100];
      ndbout_c("execSTART_RECREQ chaning srnodes from %s to %s",
               m_sr_nodes.getText(buf0),
               tmp.getText(buf1));
      
    }
    m_sr_nodes.assign(NdbNodeBitmask::Size, req->sr_nodes);
  }
  else
  {
    jam();
    cstartRecReqData = RNIL;
  }
  
  for (logPartPtr.i = 0; logPartPtr.i < clogPartFileSize; logPartPtr.i++) {
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

  if (c_lcp_restoring_fragments.isEmpty())
  {
    jam();
    csrExecUndoLogState = EULS_STARTED;

    lcpPtr.i = 0;
    ptrAss(lcpPtr, lcpRecord);
    lcpPtr.p->m_outstanding = 1;

    if (cstartType == NodeState::ST_INITIAL_NODE_RESTART)
    {
      jam();
      /**
       * Skip lgman undo...
       */
      signal->theData[0] = LGMAN_REF;
      sendSignal(reference(), GSN_START_RECCONF, signal, 1, JBB);
      return;
    }

    if (!isNdbMtLqh())
    {
      jam();
      signal->theData[0] = c_lcpId;
      sendSignal(LGMAN_REF, GSN_START_RECREQ, signal, 1, JBB);
    }
    else
    {
      jam();
      signal->theData[0] = c_lcpId;
      signal->theData[1] = LGMAN;
      sendSignal(DBLQH_REF, GSN_START_RECREQ, signal, 2, JBB);
    }
  }//if
}//Dblqh::execSTART_RECREQ()

/* ***************>> */
/*  START_RECCONF  > */
/* ***************>> */
void Dblqh::execSTART_RECCONF(Signal* signal) 
{
  jamEntry();
  lcpPtr.i = 0;
  ptrAss(lcpPtr, lcpRecord);
  ndbrequire(csrExecUndoLogState == EULS_STARTED);
  ndbrequire(lcpPtr.p->m_outstanding);

  Uint32 sender= signal->theData[0];
  
  if (ERROR_INSERTED(5055))
  {
    CLEAR_ERROR_INSERT_VALUE;
  }

  lcpPtr.p->m_outstanding--;
  if(lcpPtr.p->m_outstanding)
  {
    jam();
    return;
  }

  switch(refToBlock(sender)){
  case TSMAN:
    jam();
    break;
  case LGMAN:
    jam();
    lcpPtr.p->m_outstanding++;
    if (!isNdbMtLqh())
    {
      jam();
      signal->theData[0] = c_lcpId;
      sendSignal(TSMAN_REF, GSN_START_RECREQ, signal, 1, JBB);
    }
    else
    {
      jam();
      signal->theData[0] = c_lcpId;
      signal->theData[1] = TSMAN;
      sendSignal(DBLQH_REF, GSN_START_RECREQ, signal, 2, JBB);
    }
    return;
    break;
  default:
    ndbrequire(false);
  }

  jam();
  csrExecUndoLogState = EULS_COMPLETED;

  if(cstartType == NodeState::ST_INITIAL_NODE_RESTART)
  {
    jam();
    cstartRecReq = SRR_REDO_COMPLETE; // REDO complete

    rebuildOrderedIndexes(signal, 0);
    return;
  }

  startExecSr(signal);
}

/* ***************> */
/*  START_RECREF  > */
/* ***************> */
void Dblqh::execSTART_RECREF(Signal* signal) 
{
  jamEntry();
  ndbrequire(false);
}//Dblqh::execSTART_RECREF()

void
Dblqh::rebuildOrderedIndexes(Signal* signal, Uint32 tableId)
{
  jamEntry();

  if (tableId == 0)
  {
    jam();
    infoEvent("LQH: Starting to rebuild ordered indexes");
  }

  if (tableId >= ctabrecFileSize)
  {
    jam();

    for (logPartPtr.i = 0; logPartPtr.i < clogPartFileSize; logPartPtr.i++)
    {
      jam();
      ptrCheckGuard(logPartPtr, clogPartFileSize, logPartRecord);
      LogFileRecordPtr logFile;
      logFile.i = logPartPtr.p->currentLogfile;
      ptrCheckGuard(logFile, clogFileFileSize, logFileRecord);
      
      LogPosition head = { logFile.p->fileNo, logFile.p->currentMbyte };
      LogPosition tail = { logPartPtr.p->logTailFileNo, 
                           logPartPtr.p->logTailMbyte};
      Uint64 mb = free_log(head, tail, logPartPtr.p->noLogFiles, clogFileSize);
      if (mb <= c_free_mb_tail_problem_limit)
      {
        jam();
        update_log_problem(signal, logPartPtr,
                           LogPartRecord::P_TAIL_PROBLEM, true);
      }
    }

    if (!isNdbMtLqh())
    {
      /**
       * There should be no disk-ops in flight here...check it
       */
      signal->theData[0] = 12003;
      sendSignal(LGMAN_REF, GSN_DUMP_STATE_ORD, signal, 1, JBB);
    }

    StartRecConf * conf = (StartRecConf*)signal->getDataPtrSend();
    conf->startingNodeId = getOwnNodeId();
    conf->senderData = cstartRecReqData;
    sendSignal(cmasterDihBlockref, GSN_START_RECCONF, signal,
               StartRecConf::SignalLength, JBB);

    infoEvent("LQH: Rebuild ordered indexes complete");
    return;
  }

  tabptr.i = tableId;
  ptrCheckGuard(tabptr, ctabrecFileSize, tablerec);
  if (! (DictTabInfo::isOrderedIndex(tabptr.p->tableType) &&
         tabptr.p->tableStatus == Tablerec::TABLE_DEFINED))
  {
    jam();
    signal->theData[0] = ZREBUILD_ORDERED_INDEXES;
    signal->theData[1] = tableId + 1;
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
    return;
  }

  signal->theData[0] = NDB_LE_RebuildIndex;
  signal->theData[1] = instance();
  signal->theData[2] = tableId;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 3, JBB);

  BuildIndxImplReq* const req = (BuildIndxImplReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->senderData = tableId;
  req->requestType = BuildIndxImplReq::RF_BUILD_OFFLINE;
  req->buildId = 0;     // not yet..
  req->buildKey = 0;    // ..in use
  req->transId = 0;
  req->indexType = tabptr.p->tableType;
  req->indexId = tableId;
  req->tableId = tabptr.p->primaryTableId;
  req->parallelism = 0;
  sendSignal(calcInstanceBlockRef(DBTUP), GSN_BUILD_INDX_IMPL_REQ, signal,
             BuildIndxImplReq::SignalLength, JBB);
}

void
Dblqh::execBUILD_INDX_IMPL_REF(Signal * signal)
{
  jamEntry();
  ndbrequire(false); // TODO error message
}

void
Dblqh::execBUILD_INDX_IMPL_CONF(Signal* signal)
{
  jamEntry();
  BuildIndxImplConf * conf = (BuildIndxImplConf*)signal->getDataPtr();
  Uint32 tableId = conf->senderData;
  rebuildOrderedIndexes(signal, tableId + 1);
  infoEvent("LQH: index %u rebuild done", tableId);
}

/* ***************>> */
/*  START_EXEC_SR  > */
/* ***************>> */
void Dblqh::execSTART_EXEC_SR(Signal* signal) 
{
  jamEntry();
  fragptr.i = signal->theData[0];
  Uint32 next = RNIL;
  
  if (fragptr.i == RNIL) 
  {
    jam();
    /* ----------------------------------------------------------------------
     *    NO MORE FRAGMENTS TO START EXECUTING THE LOG ON.
     *    SEND EXEC_SRREQ TO ALL LQH TO INDICATE THAT THIS NODE WILL 
     *    NOT REQUEST ANY MORE FRAGMENTS TO EXECUTE THE FRAGMENT LOG ON.
     * ---------------------------------------------------------------------- 
     *    WE NEED TO SEND THOSE SIGNALS EVEN IF WE HAVE NOT REQUESTED 
     *    ANY FRAGMENTS PARTICIPATE IN THIS PHASE.
     * --------------------------------------------------------------------- */
    signal->theData[0] = cownNodeid;
    if (!isNdbMtLqh())
    {
      jam();
      NodeReceiverGroup rg(DBLQH, m_sr_nodes);
      sendSignal(rg, GSN_EXEC_SRREQ, signal, 1, JBB);
    }
    else
    {
      jam();
      const Uint32 sz = NdbNodeBitmask::Size;
      m_sr_nodes.copyto(sz, &signal->theData[1]);
      sendSignal(DBLQH_REF, GSN_EXEC_SRREQ, signal, 1 + sz, JBB);
    }
    return;
  } else {
    jam();
    c_lcp_complete_fragments.getPtr(fragptr);
    next = fragptr.p->nextList;

    if (fragptr.p->srNoLognodes > csrPhasesCompleted) 
    {
      jam();
      cnoOutstandingExecFragReq++;
      
      Uint32 index = csrPhasesCompleted;
      arrGuard(index, MAX_LOG_EXEC);
      Uint32 Tnode = fragptr.p->srLqhLognode[index];
      Uint32 instanceKey = fragptr.p->lqhInstanceKey;
      BlockReference ref = numberToRef(DBLQH, instanceKey, Tnode);
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
      execFragReq->dst = ref;

      if (isNdbMtLqh())
      {
        jam();
        // send via local proxy
        sendSignal(DBLQH_REF, GSN_EXEC_FRAGREQ, signal,
                   ExecFragReq::SignalLength, JBB);
      }
      else if (ndb_route_exec_frag(getNodeInfo(refToNode(ref)).m_version))
      {
        jam();
        // send via remote proxy
        sendSignal(numberToRef(DBLQH, refToNode(ref)), GSN_EXEC_FRAGREQ, signal,
                   ExecFragReq::SignalLength, JBB);
      }
      else
      {
        jam();
        // send direct
        sendSignal(ref, GSN_EXEC_FRAGREQ, signal,
                   ExecFragReq::SignalLength, JBB);
      }
    }
    signal->theData[0] = next;
    sendSignal(cownref, GSN_START_EXEC_SR, signal, 1, JBB);
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
  ndbrequire(getFragmentrec(signal, fragId));

  ndbrequire(fragptr.p->execSrNoReplicas < MAX_REPLICAS);
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
  c_fragment_pool.getPtr(fragptr);
  fragptr.p->srStatus = Fragrecord::SS_COMPLETED;

  ndbrequire(cnoOutstandingExecFragReq);
  cnoOutstandingExecFragReq--;
  if (fragptr.p->srNoLognodes == csrPhasesCompleted + 1)
  {
    jam();
    
    fragptr.p->logFlag = Fragrecord::STATE_TRUE;
    fragptr.p->fragStatus = Fragrecord::FSACTIVE;
    
    signal->theData[0] = fragptr.p->srUserptr;
    signal->theData[1] = cownNodeid;
    sendSignal(fragptr.p->srBlockref, GSN_START_FRAGCONF, signal, 2, JBB);
  }
  
  return;
}//Dblqh::execEXEC_FRAGCONF()

/* ***************> */
/*  EXEC_FRAGREF  > */
/* ***************> */
void Dblqh::execEXEC_FRAGREF(Signal* signal) 
{
  jamEntry();
  terrorCode = signal->theData[1];
  systemErrorLab(signal, __LINE__);
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
  m_sr_exec_sr_conf.set(nodeId);

  if (!m_sr_nodes.equal(m_sr_exec_sr_conf))
  {
    jam();
    /* ------------------------------------------------------------------
     *  ALL NODES HAVE NOT REPORTED COMPLETION OF EXECUTING FRAGMENT 
     *  LOGS YET.
     * ----------------------------------------------------------------- */
    return;
  }

  if (cnoOutstandingExecFragReq != 0)
  {
    /**
     * This should now have been fixed!
     *   but could occur during upgrade
     * old: wl4391_todo workaround until timing fixed
     */
    jam();
    ndbassert(false);
    m_sr_exec_sr_conf.clear(nodeId);
    ndbout << "delay: reqs=" << cnoOutstandingExecFragReq << endl;
    sendSignalWithDelay(reference(), GSN_EXEC_SRCONF,
                        signal, 10, signal->getLength());
    return;
  }
  
  /* ------------------------------------------------------------------------
   *  CLEAR NODE SYSTEM RESTART EXECUTION STATE TO PREPARE FOR NEXT PHASE OF
   *  LOG EXECUTION.
   * ----------------------------------------------------------------------- */
  m_sr_exec_sr_conf.clear();
  cnoFragmentsExecSr = 0;

  /* ------------------------------------------------------------------------
   *  NOW CHECK IF ALL FRAGMENTS IN THIS PHASE HAVE COMPLETED. IF SO START THE
   *  NEXT PHASE.
   * ----------------------------------------------------------------------- */
  ndbrequire(cnoOutstandingExecFragReq == 0);

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
  if (csrPhasesCompleted >= MAX_LOG_EXEC) {
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
    for (logPartPtr.i = 0; logPartPtr.i < clogPartFileSize; logPartPtr.i++) {
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
        systemErrorLab(signal, __LINE__);
        return;
      }//if

      if (DEBUG_REDO)
      {
        ndbout_c("part: %u srLogLimits SR_FOURTH_PHASE %u-%u (file: %u mb: %u)",
                 logPartPtr.p->logPartNo, 
                 logPartPtr.p->logStartGci,
                 logPartPtr.p->logLastGci,
                 logPartPtr.p->lastLogfile,
                 logPartPtr.p->lastMbyte);
      }

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
    csrPhaseStarted = ZSR_PHASE1_COMPLETED; // Set correct state first...
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
  m_sr_exec_sr_req.set(nodeId);
  if (!m_sr_exec_sr_req.equal(m_sr_nodes))
  {
    jam();
    return;
  }

  /* ------------------------------------------------------------------------
   *  CLEAR NODE SYSTEM RESTART STATE TO PREPARE FOR NEXT PHASE OF LOG 
   *  EXECUTION
   * ----------------------------------------------------------------------- */
  m_sr_exec_sr_req.clear();

  if (csrPhasesCompleted != 0) {
    /* ----------------------------------------------------------------------
     *       THE FIRST PHASE MUST ALWAYS EXECUTE THE LOG.
     * --------------------------------------------------------------------- */
    if (cnoFragmentsExecSr == 0) {
      jam();
      /* --------------------------------------------------------------------
       *  THERE WERE NO FRAGMENTS THAT NEEDED TO EXECUTE THE LOG IN THIS PHASE.
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

  tsrPhaseStarted = signal->theData[1];
  if (csrPhaseStarted == ZSR_NO_PHASE_STARTED) {
    jam();
    csrPhaseStarted = tsrPhaseStarted;
    return;
  }//if  
  ndbrequire(csrPhaseStarted != tsrPhaseStarted);
  ndbrequire(csrPhaseStarted != ZSR_BOTH_PHASES_STARTED);

  csrPhaseStarted = ZSR_BOTH_PHASES_STARTED;
  for (logPartPtr.i = 0; logPartPtr.i < clogPartFileSize; logPartPtr.i++) {
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
  
  jam();
  c_lcp_complete_fragments.first(fragptr);
  signal->theData[0] = ZSR_GCI_LIMITS;
  signal->theData[1] = fragptr.i;
  sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
  return;
}//Dblqh::srPhase3Start()

/* --------------------------------------------------------------------------
 *   WE NOW WE NEED TO FIND THE LIMITS WITHIN WHICH TO EXECUTE 
 *   THE FRAGMENT LOG
 * ------------------------------------------------------------------------- */
void Dblqh::srGciLimits(Signal* signal) 
{
  jamEntry();
  fragptr.i = signal->theData[0];
  Uint32 loopCount = 0;
  logPartPtr.i = 0;
  ptrAss(logPartPtr, logPartRecord);
  while (fragptr.i != RNIL){
    jam();
    c_lcp_complete_fragments.getPtr(fragptr);
    ndbrequire(fragptr.p->execSrNoReplicas - 1 < MAX_REPLICAS);
    for (Uint32 i = 0; i < fragptr.p->execSrNoReplicas; i++) {
      jam();
      if (fragptr.p->execSrStartGci[i] < logPartPtr.p->logStartGci) {
	jam();
	logPartPtr.p->logStartGci = fragptr.p->execSrStartGci[i];
      }//if
      if (fragptr.p->execSrLastGci[i] > logPartPtr.p->logLastGci) {
	jam();
	logPartPtr.p->logLastGci = fragptr.p->execSrLastGci[i];
      }
    }
    
    loopCount++;
    if (loopCount > 20) {
      jam();
      signal->theData[0] = ZSR_GCI_LIMITS;
      signal->theData[1] = fragptr.p->nextList;
      sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
      return;
    } else {
      jam();
      fragptr.i = fragptr.p->nextList;
    }//if
  }

  if (logPartPtr.p->logStartGci == (UintR)-1) {
    jam();
      /* --------------------------------------------------------------------
       *  THERE WERE NO FRAGMENTS TO INSTALL WE WILL EXECUTE THE LOG AS 
       *  SHORT AS POSSIBLE TO REACH THE END OF THE LOG. THIS WE DO BY 
       *  STARTING AT THE STOP GCI.
       * ------------------------------------------------------------------- */
    logPartPtr.p->logStartGci = logPartPtr.p->logLastGci;
  }//if
  
  for(Uint32 i = 1; i < clogPartFileSize; i++)
  {
    LogPartRecordPtr tmp;
    tmp.i = i;
    ptrAss(tmp, logPartRecord);
    tmp.p->logStartGci = logPartPtr.p->logStartGci;
    tmp.p->logLastGci = logPartPtr.p->logLastGci;
  }

  for (logPartPtr.i = 0; logPartPtr.i < clogPartFileSize; logPartPtr.i++) {
    jam();
    ptrAss(logPartPtr, logPartRecord);
    logPartPtr.p->logExecState = LogPartRecord::LES_SEARCH_STOP;
    if (DEBUG_REDO)
    {
      ndbout_c("part: %u srLogLimits (srGciLimits) %u-%u (file: %u mb: %u)",
               logPartPtr.p->logPartNo, 
               logPartPtr.p->logStartGci,
               logPartPtr.p->logLastGci,
               logPartPtr.p->lastLogfile,
               logPartPtr.p->lastMbyte);
    }
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
    ndbrequire(tmbyte < clogFileSize);
    if (logPartPtr.p->logExecState == LogPartRecord::LES_SEARCH_STOP)
    {
      if (logFilePtr.p->logMaxGciCompleted[tmbyte] <= logPartPtr.p->logLastGci)
      {
        jam();
        /* --------------------------------------------------------------------
         *  WE ARE STEPPING BACKWARDS FROM MBYTE TO MBYTE. THIS IS THE FIRST 
         *  MBYTE WHICH IS TO BE INCLUDED IN THE LOG EXECUTION. THE STOP GCI 
         *  HAS NOT BEEN COMPLETED BEFORE THIS MBYTE. THUS THIS MBYTE HAVE 
         *  TO BE EXECUTED.
         * ------------------------------------------------------------------ */
        logPartPtr.p->stopLogfile = logFilePtr.i;
        logPartPtr.p->stopMbyte = tmbyte;
        logPartPtr.p->logExecState = LogPartRecord::LES_SEARCH_START;
        if (DEBUG_REDO)
        {
          ndbout_c("part: %u srLogLimits found stop pos file: %u mb: %u logMaxGciCompleted[tmbyte]: %u (lastGci: %u)",
                   logPartPtr.p->logPartNo,
                   logFilePtr.p->fileNo,
                   tmbyte,
                   logFilePtr.p->logMaxGciCompleted[tmbyte],
                   logPartPtr.p->logLastGci);
        }
      }//if
      else if (DEBUG_REDO)
      {
        ndbout_c("SEARCH STOP SKIP part: %u file: %u mb: %u "
                 "logMaxGciCompleted: %u > %u",
                 logPartPtr.p->logPartNo,
                 logFilePtr.p->fileNo,
                 tmbyte,
                 logFilePtr.p->logMaxGciCompleted[tmbyte],
                 logPartPtr.p->logLastGci);
      }
    }//if
    /* ------------------------------------------------------------------------
     *  WHEN WE HAVEN'T FOUND THE STOP MBYTE IT IS NOT NECESSARY TO LOOK FOR THE
     *  START MBYTE. THE REASON IS THE FOLLOWING LOGIC CHAIN: 
     *    MAX_GCI_STARTED >= MAX_GCI_COMPLETED >= LAST_GCI >= START_GCI
     *  THUS MAX_GCI_STARTED >= START_GCI. THUS MAX_GCI_STARTED < START_GCI CAN
     *  NOT BE TRUE AS WE WILL CHECK OTHERWISE.
     * ---------------------------------------------------------------------- */
    if (logPartPtr.p->logExecState == LogPartRecord::LES_SEARCH_START)
    {
      if (logFilePtr.p->logMaxGciStarted[tmbyte] < logPartPtr.p->logStartGci)
      {
        jam();
        /* --------------------------------------------------------------------
         *  WE HAVE NOW FOUND THE START OF THE EXECUTION OF THE LOG. 
         *  WE STILL HAVE TO MOVE IT BACKWARDS TO ALSO INCLUDE THE 
         *  PREPARE RECORDS WHICH WERE STARTED IN A PREVIOUS MBYTE.
         * ------------------------------------------------------------------ */
        if (DEBUG_REDO)
        {
          ndbout_c("part: %u srLogLimits found start pos file: %u mb: %u logMaxGciStarted[tmbyte]: %u (startGci: %u)",
                   logPartPtr.p->logPartNo,
                   logFilePtr.p->fileNo,
                   tmbyte,
                   logFilePtr.p->logMaxGciCompleted[tmbyte],
                   logPartPtr.p->logStartGci);
          ndbout_c("part: %u srLogLimits lastPrepRef => file: %u mb: %u",
                   logPartPtr.p->logPartNo, 
                   logFilePtr.p->logLastPrepRef[tmbyte] >> 16,
                   logFilePtr.p->logLastPrepRef[tmbyte] & 65535);
        }
        tlastPrepRef = logFilePtr.p->logLastPrepRef[tmbyte];
        logPartPtr.p->startMbyte = tlastPrepRef & 65535;
        LogFileRecordPtr locLogFilePtr;
        findLogfile(signal, tlastPrepRef >> 16, logPartPtr, &locLogFilePtr);
        logPartPtr.p->startLogfile = locLogFilePtr.i;
        logPartPtr.p->logExecState = LogPartRecord::LES_EXEC_LOG;
      }
      else if (DEBUG_REDO)
      {
        ndbout_c("SEARCH START SKIP part: %u file: %u mb: %u "
                 "logMaxGciCompleted: %u >= %u",
                 logPartPtr.p->logPartNo,
                 logFilePtr.p->fileNo,
                 tmbyte,
                 logFilePtr.p->logMaxGciStarted[tmbyte],
                 logPartPtr.p->logStartGci);
      }
    }//if
    if (logPartPtr.p->logExecState != LogPartRecord::LES_EXEC_LOG) {
      if (tmbyte == 0) {
        jam();
        tmbyte = clogFileSize - 1;
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

  if (DEBUG_REDO)
  {
    LogFileRecordPtr tmp;
    tmp.i = logPartPtr.p->stopLogfile;
    ptrCheckGuard(tmp, clogFileFileSize, logFileRecord);
    ndbout_c("srLogLimits part: %u gci: %u-%u start file: %u mb: %u stop file: %u mb: %u",
             logPartPtr.p->logPartNo,
             logPartPtr.p->logStartGci,
             logPartPtr.p->logLastGci,
             tlastPrepRef >> 16,
             tlastPrepRef & 65535,
             tmp.p->fileNo,
             logPartPtr.p->stopMbyte);           
  }



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
    send_runredo_event(signal, logPartPtr.p, logPartPtr.p->logStartGci);
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
  Uint32 line;
  const char * crash_msg = 0;

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
        writeDirty(signal, __LINE__);
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
          writeDirty(signal, __LINE__);
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
      closeFile(signal, logFilePtr, __LINE__);
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
      closeFile(signal, logFilePtr, __LINE__);
      return;
      break;
    case LogPartRecord::LES_EXEC_LOG:
      jam();
      /*empty*/;
      break;
    default:
      jam();
      systemErrorLab(signal, __LINE__);
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
      if (tcConnectptr.p->gci_hi > crestartNewestGci) {
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
#ifndef NO_REDO_PAGE_CACHE
            Uint32 cnt = 1 +
              logPartPtr.p->execSrStopPageNo - logPartPtr.p->execSrStartPageNo;
            evict(m_redo_page_cache, cnt);
#endif
            readExecLog(signal);
            lfoPtr.p->lfoState = LogFileOperationRecord::READ_EXEC_LOG;
            return;
          } else {
            jam();
/*---------------------------------------------------------------------------*/
/* THE FILE IS CURRENTLY NOT OPEN. WE MUST OPEN IT BEFORE WE CAN READ FROM   */
/* THE FILE.                                                                 */
/*---------------------------------------------------------------------------*/
#ifndef NO_REDO_OPEN_FILE_CACHE
            openFileRw_cache(signal, logFilePtr);
#else
            logFilePtr.p->logFileStatus = LogFileRecord::OPEN_EXEC_LOG;
            openFileRw(signal, logFilePtr);
#endif
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
      if (unlikely(logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] != 
		   (ZPAGE_HEADER_SIZE + ZPOS_NO_FD)))
      {
	line = __LINE__;
	logWord = logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX];
	crash_msg = "ZFD_TYPE at incorrect position!";
	goto crash;
      }
      {
        Uint32 noFdDescriptors = 
	  logPagePtr.p->logPageWord[ZPAGE_HEADER_SIZE + ZPOS_NO_FD];
          logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] = 
	      (ZPAGE_HEADER_SIZE + ZFD_HEADER_SIZE) + 
	      (noFdDescriptors * ZFD_MBYTE_SIZE * clogFileSize);
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
	  line = __LINE__;
	  logWord = ZNEXT_MBYTE_TYPE;
	  crash_msg = "end of log wo/ having found last GCI";
	  goto crash;
        }//if
      }//if
/*---------------------------------------------------------------------------*/
/* START EXECUTION OF A NEW MBYTE IN THE LOG.                                */
/*---------------------------------------------------------------------------*/
      if (logFilePtr.p->currentMbyte < (clogFileSize - 1)) {
        jam();
        logPartPtr.p->logExecState = LogPartRecord::LES_EXEC_LOG_NEW_MBYTE;
      } else {
        ndbrequire(logFilePtr.p->currentMbyte == (clogFileSize - 1));
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
      if (DEBUG_REDO)
      {
        ndbout_c("found gci: %u part: %u file: %u page: %u (mb: %u)",
                 logWord,
                 logPartPtr.p->logPartNo,
                 logFilePtr.p->fileNo,
                 logFilePtr.p->currentFilepage,
                 logFilePtr.p->currentFilepage >> ZTWOLOG_NO_PAGES_IN_MBYTE);
      }
      if (logWord == logPartPtr.p->logLastGci)
      {
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
	  logPartPtr.p->logLap = logPagePtr.p->logPageWord[ZPOS_LOG_LAP];
          if (DEBUG_REDO)
          {
            ndbout_c("execSr part: %u logLap: %u",
                     logPartPtr.p->logPartNo, logPartPtr.p->logLap);
          }
        }//if
/*---------------------------------------------------------------------------*/
/* THERE IS NO NEED OF EXECUTING PAST THIS LINE SINCE THERE WILL ONLY BE LOG */
/* RECORDS THAT WILL BE OF NO INTEREST. THUS CLOSE THE FILE AND START THE    */
/* NEXT PHASE OF THE SYSTEM RESTART.                                         */
/*---------------------------------------------------------------------------*/
        logPartPtr.p->logExecState = LogPartRecord::LES_EXEC_LOG_COMPLETED;
        send_runredo_event(signal, logPartPtr.p, logPartPtr.p->logLastGci);
      }//if
      break;
    default:
      jam();
/* ========================================================================= */
/* ========================================================================= */
/*---------------------------------------------------------------------------*/
/* SEND A SIGNAL TO THE SIGNAL LOG AND THEN CRASH THE SYSTEM.                */
/*---------------------------------------------------------------------------*/
      line = __LINE__;
      crash_msg = "Invalid logword";
      goto crash;
      break;
    }//switch
/*---------------------------------------------------------------------------*/
// We continue to execute log records until we find a proper one to execute or
// that we reach a new page.
/*---------------------------------------------------------------------------*/
  } while (1);
  return;
  
crash:
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
  signal->theData[8] = line;
  
  char buf[255];
  BaseString::snprintf(buf, sizeof(buf), 
		       "Error while reading REDO log. from %d\n"
		       "part: %u D=%d, F=%d Mb=%d FP=%d W1=%d W2=%d : %s gci: %u",
		       signal->theData[8],
                       logPartPtr.p->logPartNo,
		       signal->theData[2], 
		       signal->theData[3], 
		       signal->theData[4],
		       signal->theData[5], 
		       signal->theData[6], 
		       signal->theData[7],
		       crash_msg ? crash_msg : "",
		       logPartPtr.p->logLastGci);
  
  ndbout_c("%s", buf);
  ndbout_c("logPartPtr.p->logExecState: %u", logPartPtr.p->logExecState);
  ndbout_c("crestartOldestGci: %u", crestartOldestGci);
  ndbout_c("crestartNewestGci: %u", crestartNewestGci);
  ndbout_c("csrPhasesCompleted: %u", csrPhasesCompleted);
  ndbout_c("logPartPtr.p->logStartGci: %u", logPartPtr.p->logStartGci);
  ndbout_c("logPartPtr.p->logLastGci: %u", logPartPtr.p->logLastGci);
  
  progError(__LINE__, NDBD_EXIT_SR_REDOLOG, buf);  
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
  jamEntry();
  //logPagePtr.i = signal->theData[0];
  //tdebug = logPagePtr.p->logPageWord[0];

  char buf[100];
  BaseString::snprintf(buf, 100, 
	   "Error while reading REDO log. from %d\n"
	   "D=%d, F=%d Mb=%d FP=%d W1=%d W2=%d",
	   signal->theData[8],
	   signal->theData[2], signal->theData[3], signal->theData[4],
	   signal->theData[5], signal->theData[6], signal->theData[7]);

  progError(__LINE__, NDBD_EXIT_SR_REDOLOG, buf);  

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
#ifndef NO_REDO_PAGE_CACHE
  Uint32 cnt = 1 +
    logPartPtr.p->execSrStopPageNo - logPartPtr.p->execSrStartPageNo;

#if 0
  Uint32 MAX_EXTRA_READ = 9; // can be max 9 due to FSREADREQ formatting
  while (cnt < maxextraread && (logPartPtr.p->execSrStopPageNo % 32) != 31)
  {
    jam();
    cnt++;
    logPartPtr.p->execSrStopPageNo++;
  }
#endif

  evict(m_redo_page_cache, cnt);
#endif

  readExecLog(signal);
  lfoPtr.p->lfoState = LogFileOperationRecord::READ_EXEC_LOG;
  return;
}//Dblqh::openExecLogLab()

void Dblqh::readExecLogLab(Signal* signal) 
{
  buildLinkedLogPageList(signal);
#ifndef NO_REDO_PAGE_CACHE
  addCachePages(m_redo_page_cache,
                logPartPtr.p->logPartNo,
                logPartPtr.p->execSrStartPageNo,
                lfoPtr.p);
#endif
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
  c_fragment_pool.getPtr(fragptr);
  tcConnectptr.p->m_log_part_ptr_i = fragptr.p->m_log_part_ptr_i;

  // Read a log record and prepare it for execution
  readLogHeader(signal);
  readKey(signal);
  readAttrinfo(signal);
  initReqinfoExecSr(signal);
  arrGuard(logPartPtr.p->execSrExecuteIndex, MAX_REPLICAS);
  BlockReference ref = fragptr.p->execSrBlockref[logPartPtr.p->execSrExecuteIndex];
  tcConnectptr.p->nextReplica = refToNode(ref);
  tcConnectptr.p->connectState = TcConnectionrec::LOG_CONNECTED;
  tcConnectptr.p->tcOprec = tcConnectptr.i;
  tcConnectptr.p->tcHashKeyHi = 0;
  packLqhkeyreqLab(signal);
  return;
}//Dblqh::execLogRecord()

//----------------------------------------------------------------------------
// This function invalidates log pages after the last GCI record in a 
// system/node restart. This is to ensure that the end of the log is 
// consistent. This function is executed last in start phase 3.
// RT 450. EDTJAMO.
//----------------------------------------------------------------------------
Uint32
Dblqh::nextLogFilePtr(Uint32 logFilePtrI)
{
  LogFileRecordPtr tmp;
  tmp.i = logFilePtrI;
  ptrCheckGuard(tmp, clogFileFileSize, logFileRecord);
  return tmp.p->nextLogFile;
}

void
Dblqh::invalidateLogAfterLastGCI(Signal* signal)
{
  jam();
  if (logPartPtr.p->logExecState != LogPartRecord::LES_EXEC_LOG_INVALIDATE) {
    jam();
    systemError(signal, __LINE__);
  }

  if (logFilePtr.p->fileNo != logPartPtr.p->invalidateFileNo) {
    jam();
    systemError(signal, __LINE__);
  }

  switch (lfoPtr.p->lfoState) {
  case LogFileOperationRecord::READ_SR_INVALIDATE_SEARCH_FILES:
  {
    jam();
    // Check if this file contains pages needing to be invalidated
    ndbrequire(logPartPtr.p->invalidatePageNo == 1);
    bool ok = logPagePtr.p->logPageWord[ZPOS_LOG_LAP] == logPartPtr.p->logLap;
    releaseLfo(signal);
    releaseLogpage(signal);
    if (ok)
    {
      jam();
      // This page must be invalidated.
      // We search next file
      readFileInInvalidate(signal, 3);
      return;
    }
    else
    {
      jam();
      /**
       * This file doest not need to be invalidated...move to previous
       *   file and search forward linear
       */
      readFileInInvalidate(signal, 6);
      return;
    }
    break;
  }
  case LogFileOperationRecord::READ_SR_INVALIDATE_PAGES:
    jam();
    // Check if this page must be invalidated.
    // If the log lap number on a page after the head of the tail is the same 
    // as the actual log lap number we must invalidate this page. Otherwise it
    // could be impossible to find the end of the log in a later system/node 
    // restart.
    if (logPagePtr.p->logPageWord[ZPOS_LOG_LAP] == logPartPtr.p->logLap) 
    {
      jam();
      // This page must be invalidated.
      // We search for end
      // read next
      releaseLfo(signal);
      releaseLogpage(signal); 
      readFileInInvalidate(signal, 1);
      return;
    }

    /**
     * We found the "last" page to invalidate...
     *   Invalidate backwards until head...
     */

    // Fall through...
  case LogFileOperationRecord::WRITE_SR_INVALIDATE_PAGES:
    jam();

    releaseLfo(signal);
    releaseLogpage(signal); 

    // Step backwards...
    logPartPtr.p->invalidatePageNo--;

    if (logPartPtr.p->invalidatePageNo == 0)
    {
      jam();

      if (logFilePtr.p->fileNo == 0)
      {
        jam();
        /**
         * We're wrapping in the log...
         *   update logLap
         */
        logPartPtr.p->logLap--;
	ndbrequire(logPartPtr.p->logLap); // Should always be > 0
        if (DEBUG_REDO)
        {
          ndbout_c("invalidateLogAfterLastGCI part: %u wrap from file 0 -> logLap: %u",
                   logPartPtr.p->logPartNo, logPartPtr.p->logLap);
        }
      }
      
      if (invalidateCloseFile(signal, logPartPtr, logFilePtr,
                              LogFileRecord::CLOSE_SR_WRITE_INVALIDATE_PAGES))
      {
        jam();
        return;
      }
      writeFileInInvalidate(signal, 1); // step prev
      return;
    }
    writeFileInInvalidate(signal, 0);
    return;
  default:
    jamLine(lfoPtr.p->lfoState);
    ndbrequire(false);
  }
}

void
Dblqh::writeFileInInvalidate(Signal* signal, int stepPrev)
{
  /**
   * Move to prev file
   */
  if (stepPrev == 1)
  {
    jam();
    logFilePtr.i = logFilePtr.p->prevLogFile;
    ptrCheckGuard(logFilePtr, clogFileFileSize, logFileRecord);
    logPartPtr.p->invalidateFileNo = logFilePtr.p->fileNo;
    logPartPtr.p->invalidatePageNo = clogFileSize * ZPAGES_IN_MBYTE - 1;
  }

  if (logPartPtr.p->invalidateFileNo == logPartPtr.p->headFileNo &&
      logPartPtr.p->invalidatePageNo == logPartPtr.p->headPageNo)
  {
    jam();
    /**
     * Done...
     */
    logFilePtr.i = logPartPtr.p->currentLogfile;
    ptrCheckGuard(logFilePtr, clogFileFileSize, logFileRecord);

    logFilePtr.i = logFilePtr.p->nextLogFile;
    ptrCheckGuard(logFilePtr, clogFileFileSize, logFileRecord);

    exitFromInvalidate(signal);
    return;
  }

  if (stepPrev == 1 && logFilePtr.p->logFileStatus != LogFileRecord::OPEN)
  {
    jam();
    if (DEBUG_REDO)
    {
      ndbout_c("invalidate part: %u open for write %u",
               logPartPtr.p->logPartNo, logFilePtr.p->fileNo);
    }
    logFilePtr.p->logFileStatus =LogFileRecord::OPEN_SR_WRITE_INVALIDATE_PAGES;
    openFileRw(signal, logFilePtr);
    return;
  }

  seizeLogpage(signal);

  /**
   * Make page really empty
   */
  bzero(logPagePtr.p, sizeof(LogPageRecord));
  writeSinglePage(signal, logPartPtr.p->invalidatePageNo,
                  ZPAGE_SIZE - 1, __LINE__);

  lfoPtr.p->lfoState = LogFileOperationRecord::WRITE_SR_INVALIDATE_PAGES;
  return;
}//Dblqh::invalidateLogAfterLastGCI

bool
Dblqh::invalidateCloseFile(Signal* signal,
                           Ptr<LogPartRecord> partPtr,
                           Ptr<LogFileRecord> filePtr,
                           LogFileRecord::LogFileStatus status)
{
  jam();
  if (filePtr.p->fileNo != 0 &&
      filePtr.i != partPtr.p->currentLogfile &&
      filePtr.i != nextLogFilePtr(logPartPtr.p->currentLogfile))
  {
    jam();
    if (DEBUG_REDO)
    {
      ndbout_c("invalidate part: %u close %u(%u) state: %u (%u)",
               logPartPtr.p->logPartNo,
               logFilePtr.p->fileNo,
               logFilePtr.i,
               (Uint32)status,
               logPartPtr.p->currentLogfile);
    }
    filePtr.p->logFileStatus = status;
    closeFile(signal, filePtr, __LINE__);
    return true;
  }
  return false;
}

void Dblqh::readFileInInvalidate(Signal* signal, int stepNext)
{
  jam();

  if (DEBUG_REDO)
  {
    ndbout_c("readFileInInvalidate part: %u file: %u stepNext: %u",
             logPartPtr.p->logPartNo, logFilePtr.p->fileNo, stepNext);
  }

  if (stepNext == 0)
  {
    jam();
    // Contact NDBFS. Real time break.
    readSinglePage(signal, logPartPtr.p->invalidatePageNo);
    lfoPtr.p->lfoState = LogFileOperationRecord::READ_SR_INVALIDATE_PAGES;
    return;
  }

  if (stepNext == 1)
  {
    jam();
    logPartPtr.p->invalidatePageNo++;
    if (logPartPtr.p->invalidatePageNo == (clogFileSize * ZPAGES_IN_MBYTE))
    {
      if (invalidateCloseFile(signal, logPartPtr, logFilePtr,
                              LogFileRecord::CLOSE_SR_READ_INVALIDATE_PAGES))
      {
        jam();
        return;
      }
      else
      {
        jam();
        stepNext = 2; // After close
      }
    }
    else
    {
      jam();
      // Contact NDBFS. Real time break.
      readSinglePage(signal, logPartPtr.p->invalidatePageNo);
      lfoPtr.p->lfoState = LogFileOperationRecord::READ_SR_INVALIDATE_PAGES;
      return;
    }
  }
  
  if (stepNext == 2)
  {
    jam();
    // We continue in the next file.
    logFilePtr.i = logFilePtr.p->nextLogFile;
    ptrCheckGuard(logFilePtr, clogFileFileSize, logFileRecord);
    logPartPtr.p->invalidateFileNo = logFilePtr.p->fileNo;
    // Page 0 is used for file descriptors.
    logPartPtr.p->invalidatePageNo = 1;

    if (logFilePtr.p->fileNo == 0)
    {
      /**
       * We're wrapping in the log...
       *   update logLap
       */
      logPartPtr.p->logLap++;
      if (DEBUG_REDO)
      {
        ndbout_c("readFileInInvalidate part: %u step: %u wrap to file 0 -> logLap: %u",
                 logPartPtr.p->logPartNo, stepNext, logPartPtr.p->logLap);
      }
    }

stepNext_2:
    if (logFilePtr.p->logFileStatus != LogFileRecord::OPEN)
    {
      jam();
      if (DEBUG_REDO)
      {
        ndbout_c("invalidate part: %u step: %u open for read %u",
                 logPartPtr.p->logPartNo, stepNext, logFilePtr.p->fileNo);
      }
      logFilePtr.p->logFileStatus =LogFileRecord::OPEN_SR_READ_INVALIDATE_PAGES;
      openFileRw(signal, logFilePtr);
      return;
    }

    // Contact NDBFS. Real time break.
    readSinglePage(signal, logPartPtr.p->invalidatePageNo);
    lfoPtr.p->lfoState = LogFileOperationRecord::READ_SR_INVALIDATE_PAGES;
    return;
  }

  if (stepNext == 3)
  {
    jam();
    if (invalidateCloseFile
        (signal, logPartPtr, logFilePtr,
         LogFileRecord::CLOSE_SR_READ_INVALIDATE_SEARCH_FILES))
    {
      jam();
      return;
    }
    stepNext = 4;
  }

  if (stepNext == 4)
  {
    jam();
    logFilePtr.i = logFilePtr.p->nextLogFile;
    ptrCheckGuard(logFilePtr, clogFileFileSize, logFileRecord);
    logPartPtr.p->invalidateFileNo = logFilePtr.p->fileNo;
    // Page 0 is used for file descriptors.
    logPartPtr.p->invalidatePageNo = 1;

    if (logFilePtr.p->fileNo == 0)
    {
      /**
       * We're wrapping in the log...
       *   update logLap
       */
      logPartPtr.p->logLap++;
      if (DEBUG_REDO)
      {
        ndbout_c("readFileInInvalidate part: %u step: %u wrap to file 0 -> logLap: %u",
                 logPartPtr.p->logPartNo, stepNext, logPartPtr.p->logLap);
      }
    }

    if (logFilePtr.p->logFileStatus != LogFileRecord::OPEN)
    {
      jam();
      if (DEBUG_REDO)
      {
        ndbout_c("invalidate part: %u step: %u open for read %u",
                 logPartPtr.p->logPartNo, stepNext, logFilePtr.p->fileNo);
      }
      logFilePtr.p->logFileStatus =
        LogFileRecord::OPEN_SR_READ_INVALIDATE_SEARCH_FILES;
      openFileRw(signal, logFilePtr);
      return;
    }
    stepNext = 5;
  }

  if (stepNext == 5)
  {
    jam();
    // Contact NDBFS. Real time break.
    readSinglePage(signal, logPartPtr.p->invalidatePageNo);
    lfoPtr.p->lfoState =
      LogFileOperationRecord::READ_SR_INVALIDATE_SEARCH_FILES;
    return;
  }

  if (stepNext == 6)
  {
    jam();
    if (invalidateCloseFile
        (signal, logPartPtr, logFilePtr,
         LogFileRecord::CLOSE_SR_READ_INVALIDATE_SEARCH_LAST_FILE))
    {
      jam();
      return;
    }
    stepNext = 7;
  }

  if (stepNext == 7)
  {
    jam();

    if (logFilePtr.p->fileNo == 0)
    {
      jam();
      /**
       * We're wrapping in the log...
       *   update logLap
       */
      logPartPtr.p->logLap--;
      ndbrequire(logPartPtr.p->logLap); // Should always be > 0
      if (DEBUG_REDO)
      {
        ndbout_c("invalidateLogAfterLastGCI part: %u step: %u wrap from file 0 -> logLap: %u",
                 logPartPtr.p->logPartNo, stepNext, logPartPtr.p->logLap);
      }
    }

    logFilePtr.i = logFilePtr.p->prevLogFile;
    ptrCheckGuard(logFilePtr, clogFileFileSize, logFileRecord);

    logPartPtr.p->invalidateFileNo = logFilePtr.p->fileNo;
    // Page 0 is used for file descriptors.
    logPartPtr.p->invalidatePageNo = 1;

    if (logPartPtr.p->invalidateFileNo == logPartPtr.p->headFileNo)
    {
      jam();
      logPartPtr.p->invalidatePageNo = logPartPtr.p->headPageNo;

      if (! ((cstartType == NodeState::ST_INITIAL_START) ||
             (cstartType == NodeState::ST_INITIAL_NODE_RESTART)))
      {
        jam();
        if (logFilePtr.i == logPartPtr.p->lastLogfile)
        {
          jam();
          Uint32 lastMbytePageNo =
            logPartPtr.p->lastMbyte << ZTWOLOG_NO_PAGES_IN_MBYTE;
          if (logPartPtr.p->invalidatePageNo < lastMbytePageNo)
          {
            jam();
            if (DEBUG_REDO)
            {
              ndbout_c("readFileInInvalidate part: %u step: %u moving invalidatePageNo from %u to %u (lastMbyte)",
                       logPartPtr.p->logPartNo, stepNext,
                       logPartPtr.p->invalidatePageNo,
                       lastMbytePageNo);
            }
            logPartPtr.p->invalidatePageNo = lastMbytePageNo;
          }
        }
      }
      readFileInInvalidate(signal, 1);
      return;
    }

    goto stepNext_2;
  }
  ndbrequire(false);
}

void Dblqh::exitFromInvalidate(Signal* signal)
{
  jam();

  if (DEBUG_REDO)
  {
    jam();
    printf("exitFromInvalidate part: %u head file: %u page: %u open: ",
           logPartPtr.p->logPartNo,
           logPartPtr.p->headFileNo,
           logPartPtr.p->headPageNo);

    LogFileRecordPtr tmp;
    tmp.i = logPartPtr.p->currentLogfile;
    do
    {
      jam();
      ptrCheckGuard(tmp, clogFileFileSize, logFileRecord);
      if (tmp.p->logFileStatus != LogFileRecord::LFS_IDLE &&
          tmp.p->logFileStatus != LogFileRecord::CLOSED)
      {
        jam();
        printf("%u ", tmp.p->fileNo);
      }
      tmp.i = tmp.p->nextLogFile;
    } while (tmp.i != logPartPtr.p->currentLogfile && tmp.i != RNIL);
    printf("\n");

    tmp.i = logPartPtr.p->currentLogfile;
    ptrCheckGuard(tmp, clogFileFileSize, logFileRecord);
      
    LogPosition head = { tmp.p->fileNo, tmp.p->currentMbyte };
    LogPosition tail = { logPartPtr.p->logTailFileNo, 
                         logPartPtr.p->logTailMbyte};
    Uint64 mb = free_log(head, tail, logPartPtr.p->noLogFiles, clogFileSize);
    Uint64 total = logPartPtr.p->noLogFiles * Uint64(clogFileSize);
    ndbout_c("head: [ %u %u ] tail: [ %u %u ] free: %llu total: %llu",
             head.m_file_no, head.m_mbyte,
             tail.m_file_no, tail.m_mbyte,
             mb, total);
  }
  
  logFilePtr.i = logPartPtr.p->firstLogfile;
  ptrCheckGuard(logFilePtr, clogFileFileSize, logFileRecord);
  logPagePtr.i = logFilePtr.p->logPageZero;
  ptrCheckGuard(logPagePtr, clogPageFileSize, logPageRecord);
  logPagePtr.p->logPageWord[ZPAGE_HEADER_SIZE + ZPOS_FILE_NO] = 
    logPartPtr.p->headFileNo;
  writeSinglePage(signal, 0, ZPAGE_SIZE - 1, __LINE__);

  lfoPtr.p->logFileRec = logFilePtr.i;
  lfoPtr.p->lfoState = LogFileOperationRecord::WRITE_SR_INVALIDATE_PAGES_UPDATE_PAGE0;
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
    if (unlikely(terrorCode != ZNO_TUPLE_FOUND))
      goto error;
    break;
  case ZINSERT:
    jam();
    if (unlikely(terrorCode != ZTUPLE_ALREADY_EXIST && terrorCode != 899))
      goto error;
    
    break;
  default:
    goto error;
  }

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
error:
  BaseString tmp;
  tmp.appfmt("You have found a bug!"
	     " Failed op (%s) during REDO table: %d fragment: %d err: %d",
	     tcConnectptr.p->operation == ZINSERT ? "INSERT" :
	     tcConnectptr.p->operation == ZUPDATE ? "UPDATE" :
	     tcConnectptr.p->operation == ZDELETE ? "DELETE" :
	     tcConnectptr.p->operation == ZWRITE ? "WRITE" : "<unknown>",
	     tcConnectptr.p->tableref,
	     tcConnectptr.p->fragmentid,
	     terrorCode);
  progError(__LINE__, NDBD_EXIT_SYSTEM_ERROR, 
	    tmp.c_str());
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
  logPartPtr.p->logTcConrec = RNIL;
  releaseTcrecLog(signal, tcConnectptr);
  for (logPartPtr.i = 0; logPartPtr.i < clogPartFileSize; logPartPtr.i++) {
    jam();
    ptrAss(logPartPtr, logPartRecord);
    if (logPartPtr.p->logPartState != LogPartRecord::SR_THIRD_PHASE_COMPLETED) {
      if (logPartPtr.p->logPartState != LogPartRecord::SR_THIRD_PHASE_STARTED) {
        jam();
        systemErrorLab(signal, __LINE__);
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
  jam();

#ifndef NO_REDO_PAGE_CACHE
  release(m_redo_page_cache);
#endif

#ifndef NO_REDO_OPEN_FILE_CACHE
  release(signal, m_redo_open_file_cache);
#else
  execLogComp_extra_files_closed(signal);
#endif
}

void
Dblqh::execLogComp_extra_files_closed(Signal * signal)
{
  c_lcp_complete_fragments.first(fragptr);
  signal->theData[0] = ZSEND_EXEC_CONF;
  signal->theData[1] = fragptr.i;
  sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
  return;
}

/* --------------------------------------------------------------------------
 *  GO THROUGH THE FRAGMENT RECORDS TO DEDUCE TO WHICH SHALL BE SENT
 *  EXEC_FRAGCONF AFTER COMPLETING THE EXECUTION OF THE LOG.
 * ------------------------------------------------------------------------- */
void Dblqh::sendExecConf(Signal* signal) 
{
  jamEntry();
  fragptr.i = signal->theData[0];
  Uint32 loopCount = 0;
  while (fragptr.i != RNIL) {
    c_lcp_complete_fragments.getPtr(fragptr);
    Uint32 next = fragptr.p->nextList;
    if (fragptr.p->execSrStatus != Fragrecord::IDLE) {
      jam();
      ndbrequire(fragptr.p->execSrNoReplicas - 1 < MAX_REPLICAS);
      for (Uint32 i = 0; i < fragptr.p->execSrNoReplicas; i++) {
        jam();
        Uint32 ref = fragptr.p->execSrBlockref[i];
        signal->theData[0] = fragptr.p->execSrUserptr[i];

        if (isNdbMtLqh())
        {
          jam();
          // send via own proxy
          signal->theData[1] = ref;
          sendSignal(DBLQH_REF, GSN_EXEC_FRAGCONF, signal, 2, JBB);
        }
        else if (refToInstance(ref) != 0 &&
                 ndb_route_exec_frag(getNodeInfo(refToNode(ref)).m_version))
        {
          jam();
          // send via remote proxy
          signal->theData[1] = ref;
          sendSignal(numberToRef(refToMain(ref), refToNode(ref)),
                     GSN_EXEC_FRAGCONF, signal, 2, JBB);
        }
        else
        {
          jam();
          // send direct
          sendSignal(ref, GSN_EXEC_FRAGCONF, signal, 1, JBB);
        }
      }//for
      fragptr.p->execSrNoReplicas = 0;
    }//if
    loopCount++;
    if (loopCount > 20) {
      jam();
      signal->theData[0] = ZSEND_EXEC_CONF;
      signal->theData[1] = next;
      sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
      return;
    } else {
      jam();
      fragptr.i = next;
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

  signal->theData[0] = cownNodeid;
  if (!isNdbMtLqh())
  {
    jam();
    NodeReceiverGroup rg(DBLQH, m_sr_nodes);
    sendSignal(rg, GSN_EXEC_SRCONF, signal, 1, JBB);
  }
  else
  {
    jam();
    const Uint32 sz = NdbNodeBitmask::Size;
    m_sr_nodes.copyto(sz, &signal->theData[1]);
    sendSignal(DBLQH_REF, GSN_EXEC_SRCONF, signal, 1 + sz, JBB);
  }
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
  Uint32 gci = crestartNewestGci;
  if (crestartOldestGci > gci)
  {
    jam();
    /**
     * If "keepGci" is bigger than latest-completed-gci
     *   move cnewest/cnewestCompletedGci forward
     */
    ndbout_c("readSrFourthPhaseLab: gci %u => %u",
             gci, crestartOldestGci);
    gci = crestartOldestGci;
  }
  cnewestGci = gci;
  cnewestCompletedGci = gci;
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
      systemErrorLab(signal, __LINE__);
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
   
  readFileInInvalidate(signal, 3);
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
  for (logPartPtr.i = 0; logPartPtr.i < clogPartFileSize; logPartPtr.i++) {
    jam();
    ptrAss(logPartPtr, logPartRecord);
    if (logPartPtr.p->logPartState != LogPartRecord::SR_FOURTH_PHASE_COMPLETED) {
      if (logPartPtr.p->logPartState != LogPartRecord::SR_FOURTH_PHASE_STARTED) {
        jam();
        systemErrorLab(signal, __LINE__);
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
  for (logPartPtr.i = 0; logPartPtr.i < clogPartFileSize; logPartPtr.i++) {
    ptrAss(logPartPtr, logPartRecord);
    logPartPtr.p->logPartState = LogPartRecord::IDLE;
  }//for

  if ((cstartType == NodeState::ST_INITIAL_START) || 
      (cstartType == NodeState::ST_INITIAL_NODE_RESTART)) {
    jam();
    
    ndbrequire(cinitialStartOngoing == ZTRUE);
    cinitialStartOngoing = ZFALSE;
    cstartRecReq = SRR_REDO_COMPLETE;
    checkStartCompletedLab(signal);
    return;
  } else if ((cstartType == NodeState::ST_NODE_RESTART) ||
             (cstartType == NodeState::ST_SYSTEM_RESTART)) {
    jam();

    if(cstartType == NodeState::ST_SYSTEM_RESTART)
    {
      jam();
      if (c_lcp_complete_fragments.first(fragptr))
      {
	jam();
        signal->theData[0] = ZENABLE_EXPAND_CHECK;
        signal->theData[1] = fragptr.i;
        sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
	return;
      }
    }

    cstartRecReq = SRR_REDO_COMPLETE; // REDO complete

    rebuildOrderedIndexes(signal, 0);
    return;
  } else {
    ndbrequire(false);
  }//if
  return;
}//Dblqh::srFourthComp()

/* ######################################################################### */
/* #######                            ERROR MODULE                   ####### */
/*                                                                           */
/* ######################################################################### */

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

void Dblqh::systemErrorLab(Signal* signal, int line) 
{
  systemError(signal, line);
  progError(line, NDBD_EXIT_NDBREQUIRE);
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
    if (tcConnectptr.p->activeCreat == Fragrecord::AC_IGNORED) {
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
      if (saveAttrInfoInSection(dataPtr, length) == ZOK) {
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
        tcConnectptr.p->activeCreat = Fragrecord::AC_NORMAL;
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
  dataPtr[index]= 0;
  index++;
  dataPtr[index]= 0;
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
  Uint32 prev = RNIL;
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
//       progError(__LINE__, NDBD_EXIT_NDBREQUIRE, "Redolog: Checksum failure.");
//     }
// #endif

    bllLogPagePtr.p->logPageWord[ZPREV_PAGE] = prev;
    bllLogPagePtr.p->logPageWord[ZNEXT_PAGE] = 
      lfoPtr.p->logPageArray[tbllIndex + 1];
    bllLogPagePtr.p->logPageWord[ZPOS_DIRTY] = ZNOT_DIRTY;
    prev = bllLogPagePtr.i;
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
        ndbrequire((fragptr.p->execSrNoReplicas - 1) < MAX_REPLICAS);
        for (Uint32 i = logPartPtr.p->execSrExecuteIndex; 
	     i < fragptr.p->execSrNoReplicas; 
	     i++) {
          jam();
          if (tcConnectptr.p->gci_hi >= fragptr.p->execSrStartGci[i]) {
            if (tcConnectptr.p->gci_hi <= fragptr.p->execSrLastGci[i]) {
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
  c_fragment_pool.getPtr(fragptr);
  fragptr.p->activeTcCounter = fragptr.p->activeTcCounter - 1;
  if (fragptr.p->activeTcCounter == 0) {
    jam();
    fragptr.p->startGci = cnewestGci + 1;
    tabptr.i = tcConnectptr.p->tableref;
    ptrCheckGuard(tabptr, ctabrecFileSize, tablerec);
    sendCopyActiveConf(signal, tcConnectptr.p->tableref);
  }//if
}//Dblqh::checkScanTcCompleted()

/* ------------------------------------------------------------------------- */
/* ------       CLOSE A FILE DURING EXECUTION OF FRAGMENT LOG        ------- */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void Dblqh::closeFile(Signal* signal, 
		      LogFileRecordPtr clfLogFilePtr, Uint32 line) 
{
  signal->theData[0] = clfLogFilePtr.p->fileRef;
  signal->theData[1] = cownref;
  signal->theData[2] = clfLogFilePtr.i;
  signal->theData[3] = ZCLOSE_NO_DELETE;
  signal->theData[4] = line;
  sendSignal(NDBFS_REF, GSN_FSCLOSEREQ, signal, 5, JBA);
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
void Dblqh::completedLogPage(Signal* signal, Uint32 clpType, Uint32 place) 
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

    writeDbgInfoPageHeader(wlpLogPagePtr, place,
                           logFilePtr.p->filePosition + twlpNoPages - 1,
                           ZPAGE_SIZE);
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
  if (twlpType == ZLAST_WRITE_IN_FILE) {
    jam();
    signal->theData[3] = ZLIST_OF_MEM_PAGES_SYNCH;
  } else {
    jam();
    signal->theData[3] = ZLIST_OF_MEM_PAGES;
  }//if
  signal->theData[4] = ZVAR_NO_LOG_PAGE_WORD;
  signal->theData[5] = twlpNoPages;
  sendSignal(NDBFS_REF, GSN_FSWRITEREQ, signal, 15, JBA);

  ndbrequire(logFilePtr.p->fileRef != RNIL);

  logPartPtr.p->m_io_tracker.send_io(32768*twlpNoPages);

  if (DEBUG_REDO)
  {
    ndbout_c("writing %d pages at part: %u file: %u page: %u (mb: %u)",
             twlpNoPages,
             logPartPtr.p->logPartNo,
             logFilePtr.p->fileNo,
             logFilePtr.p->filePosition,
             logFilePtr.p->filePosition >> ZTWOLOG_NO_PAGES_IN_MBYTE);
  }

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
  for (Uint32 i = 0; i < NDB_ARRAY_SIZE(tabptr.p->fragid); i++) {
    jam();
    if (tabptr.p->fragid[i] == fragId) {
      fragptr.i = tabptr.p->fragrec[i];
      indexFound = i;
      break;
    }//if
  }//for
  if (fragptr.i != RNIL) {
    jam();
    c_fragment_pool.getPtr(fragptr);
    tabptr.p->fragid[indexFound] = ZNIL;
    tabptr.p->fragrec[indexFound] = RNIL;
    fragptr.p->fragStatus = Fragrecord::FREE;
    c_fragment_pool.release(fragptr);
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
    if (loopCount >= flfLogPartPtr.p->noLogFiles &&
	getNodeState().startLevel != NodeState::SL_STARTED)
    {
      goto error;
    }
    ndbrequire(loopCount < flfLogPartPtr.p->noLogFiles);
  }//while

error:
  char buf[255];
  BaseString::snprintf(buf, sizeof(buf), 
		       "Unable to restart, failed while reading redo."
		       " Likely invalid change of configuration");
  progError(__LINE__, 
	    NDBD_EXIT_INVALID_CONFIG,
	    buf);
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

#ifndef NO_REDO_PAGE_CACHE
  RedoPageCache& cache = m_redo_page_cache;
  RedoCacheLogPageRecord key;
  key.m_part_no = logPartPtr.p->logPartNo;
  key.m_file_no = commitLogRecord->fileNo;
  key.m_page_no = commitLogRecord->startPageNo;
  Ptr<RedoCacheLogPageRecord> pagePtr;
  if (cache.m_hash.find(pagePtr, key))
  {
    jam();
    if (cache.m_lru.hasPrev(pagePtr))
    {
      jam();
      cache.m_lru.remove(pagePtr);
      cache.m_lru.addFirst(pagePtr);
    }
    logPagePtr.i = pagePtr.i;
    ptrCheckGuard(logPagePtr, clogPageFileSize, logPageRecord);

    Ptr<LogPageRecord> loopPtr = logPagePtr;
    Uint32 extra = commitLogRecord->stopPageNo - commitLogRecord->startPageNo;
    for (Uint32 i = 0; i<extra; i++)
    {
      jam();
      Uint32 prevPtrI = loopPtr.i;
      loopPtr.i = loopPtr.p->logPageWord[ZNEXT_PAGE];
      if (loopPtr.i == RNIL)
      {
        jam();
        /**
         * next page is not linked
         *   check if it's added as a "single" page
         */
        key.m_page_no = commitLogRecord->startPageNo + i + 1;
        if (cache.m_hash.find(pagePtr, key))
        {
          jam();
          /**
           * Yes it is...link them
           */
          Ptr<LogPageRecord> tmp;
          tmp.i = pagePtr.i;
          tmp.p = reinterpret_cast<LogPageRecord*>(pagePtr.p);
          tmp.p->logPageWord[ZPREV_PAGE] = prevPtrI;
          loopPtr.p->logPageWord[ZNEXT_PAGE] = tmp.i;
          loopPtr.i = tmp.i;
        }
        else
        {
          jam();
          logPagePtr.i = RNIL;
          cache.m_multi_miss++;
          if (0)
          ndbout_c("Found part: %u file: %u page: %u but not next page(%u) %u",
                   key.m_part_no,
                   commitLogRecord->fileNo,
                   commitLogRecord->startPageNo,
                   (i + 1),
                   commitLogRecord->startPageNo + i + 1);
          return;
        }
      }

      ptrCheckGuard(loopPtr, clogPageFileSize, logPageRecord);
      pagePtr.i = loopPtr.i;
      pagePtr.p = reinterpret_cast<RedoCacheLogPageRecord*>(loopPtr.p);
      if (cache.m_lru.hasPrev(pagePtr))
      {
        jam();
        cache.m_lru.remove(pagePtr);
        cache.m_lru.addFirst(pagePtr);
      }
    }
    cache.m_hits++;
    if (extra)
    {
      jam();
      cache.m_multi_page++;
    }
  }
#endif
}//Dblqh::findPageRef()

/* ------------------------------------------------------------------------- */
/* ------         GET FIRST OPERATION QUEUED FOR LOGGING             ------- */
/*                                                                           */
/*      SUBROUTINE SHORT NAME = GFL                                          */
/* ------------------------------------------------------------------------- */
void
Dblqh::getFirstInLogQueue(Signal* signal,
                          Ptr<TcConnectionrec> & dst)
{
  TcConnectionrecPtr tmp;
/* -------------------------------------------------- */
/*       GET THE FIRST FROM THE LOG QUEUE AND REMOVE  */
/*       IT FROM THE QUEUE.                           */
/* -------------------------------------------------- */
  LogPartRecord::OperationQueue * queue = &logPartPtr.p->m_log_complete_queue;
  tmp.i = queue->firstElement;
  if (tmp.i == RNIL)
  {
    jam();
    queue = &logPartPtr.p->m_log_prepare_queue;
    tmp.i = queue->firstElement;
  }
  ptrCheckGuard(tmp, ctcConnectrecFileSize, tcConnectionrec);
  queue->firstElement = tmp.p->nextTcLogQueue;
  if (queue->firstElement == RNIL) {
    jam();
    queue->lastElement = RNIL;
  }//if
  dst = tmp;
}//Dblqh::getFirstInLogQueue()

/* ---------------------------------------------------------------- */
/* ---------------- GET FRAGMENT RECORD --------------------------- */
/*       INPUT:          TFRAGID         FRAGMENT ID LOOKING FOR    */
/*                       TABPTR          TABLE ID                   */
/*       SUBROUTINE SHORT NAME = GFR                                */
/* ---------------------------------------------------------------- */
bool Dblqh::getFragmentrec(Signal* signal, Uint32 fragId) 
{
  for (Uint32 i = 0; i < NDB_ARRAY_SIZE(tabptr.p->fragid); i++) {
    jam();
    if (tabptr.p->fragid[i] == fragId) {
      fragptr.i = tabptr.p->fragrec[i];
      c_fragment_pool.getPtr(fragptr);
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
/* ======                INITIATE FRAGMENT RECORD                    ======= */
/*                                                                           */
/* ========================================================================= */
void Dblqh::initialiseFragrec(Signal* signal) 
{
  
  SLList<Fragrecord> tmp(c_fragment_pool);
  while(tmp.seize(fragptr))
  {
    refresh_watch_dog();
    new (fragptr.p) Fragrecord();
    fragptr.p->fragStatus = Fragrecord::FREE;
    fragptr.p->execSrStatus = Fragrecord::IDLE;
    fragptr.p->srStatus = Fragrecord::SS_IDLE;
  }
  tmp.release();
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
      for (tigpIndex = 0; tigpIndex < NDB_MAX_LOG_PARTS; tigpIndex++) {
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
      lcpPtr.p->reportEmpty = false;
      lcpPtr.p->firstFragmentFlag = false;
      lcpPtr.p->lastFragmentFlag = false;
    }//for
  }//if
}//Dblqh::initialiseLcpRec()

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

      logFilePtr.p->logLastPrepRef = new Uint32[clogFileSize];
      logFilePtr.p->logMaxGciCompleted = new Uint32[clogFileSize];
      logFilePtr.p->logMaxGciStarted = new Uint32[clogFileSize];

      if (logFilePtr.p->logLastPrepRef == 0 ||
          logFilePtr.p->logMaxGciCompleted == 0 ||
          logFilePtr.p->logMaxGciStarted == 0)
      {
        char buf[256];
        BaseString::snprintf(buf, sizeof(buf),
                             "Failed to alloc mbyte(%u) arrays for logfile %u",
                             clogFileSize, logFilePtr.i);
        progError(__LINE__, NDBD_EXIT_MEMALLOC, buf);
      }

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
  /**
   * Moved into initRecords()
   */
}//Dblqh::initialiseLogPage()

/* ========================================================================= 
 * ======                       INITIATE LOG PART RECORD             =======
 *
 * ========================================================================= */
void Dblqh::initialiseLogPart(Signal* signal) 
{
  for (logPartPtr.i = 0; logPartPtr.i < clogPartFileSize; logPartPtr.i++) {
    ptrAss(logPartPtr, logPartRecord);
    logPartPtr.p->waitWriteGciLog = LogPartRecord::WWGL_FALSE;
    logPartPtr.p->LogLqhKeyReqSent = ZFALSE;
    logPartPtr.p->logPartNewestCompletedGCI = (UintR)-1;
    logPartPtr.p->logTcConrec = RNIL;
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
    m_sr_nodes.clear();
    m_sr_exec_sr_req.clear();
    m_sr_exec_sr_conf.clear();
    for (i = 0; i < 4; i++) {
      cactiveCopy[i] = RNIL;
    }//for
    cnoActiveCopy = 0;
    ccurrentGcprec = RNIL;
    caddNodeState = ZFALSE;
    cstartRecReq = SRR_INITIAL; // Initial
    cnewestGci = 0;
    cnewestCompletedGci = 0;
    crestartOldestGci = 0;
    crestartNewestGci = 0;
    csrPhaseStarted = ZSR_NO_PHASE_STARTED;
    csrPhasesCompleted = 0;
    cmasterDihBlockref = 0;
    cnoFragmentsExecSr = 0;
    cnoOutstandingExecFragReq = 0;
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
    /* Unused */
    break;
  case 3:
    jam();
    /* Unused */
    break;
  case 4:
    jam();
    initialiseFragrec(signal);
    break;
  case 5:
    jam();
    initialiseGcprec(signal);
    initialiseLcpRec(signal);
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
  sendSignal(reference(), GSN_CONTINUEB, signal, 5, JBB);

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
    scanptr.p->scanTcWaiting = 0;
    scanptr.p->nextHash = RNIL;
    scanptr.p->prevHash = RNIL;
    scanptr.p->scan_acc_index= 0;
    scanptr.p->scan_acc_segments= 0;
    scanptr.p->m_reserved = 0;
  }
  tmp.release();

  /**
   * just seize records from pool and put into
   *   dedicated list
   */
  m_reserved_scans.seize(scanptr); // LCP
  scanptr.p->m_reserved = 1;
  m_reserved_scans.seize(scanptr); // NR
  scanptr.p->m_reserved = 1;

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
      tabptr.p->usageCountR = 0;
      tabptr.p->usageCountW = 0;
      for (Uint32 i = 0; i < NDB_ARRAY_SIZE(tabptr.p->fragid); i++) {
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
      tcConnectptr.p->keyInfoIVal = RNIL;
      tcConnectptr.p->attrInfoIVal = RNIL;
      tcConnectptr.p->m_flags= 0;
      tcConnectptr.p->tcTimer = 0;
      tcConnectptr.p->nextHashRec = RNIL;
      tcConnectptr.p->prevHashRec = RNIL;
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
  fragptr.p->firstWaitQueue = RNIL;
  fragptr.p->lastWaitQueue = RNIL;
  fragptr.p->fragStatus = Fragrecord::DEFINED;
  fragptr.p->fragCopy = copyType;
  fragptr.p->tupBlockref = ctupBlockref;
  fragptr.p->tuxBlockref = ctuxBlockref;
  fragptr.p->logFlag = Fragrecord::STATE_TRUE;
  fragptr.p->lcpFlag = Fragrecord::LCP_STATE_TRUE;
  for (Uint32 i = 0; i < MAX_LCP_STORED; i++) {
    fragptr.p->lcpId[i] = 0;
  }//for
  fragptr.p->maxGciCompletedInLcp = 0;
  fragptr.p->maxGciInLcp = 0;
  fragptr.p->copyFragState = ZIDLE;
  fragptr.p->newestGci = cnewestGci;
  fragptr.p->tabRef = tableId;
  fragptr.p->fragId = fragId;
  fragptr.p->srStatus = Fragrecord::SS_IDLE;
  fragptr.p->execSrStatus = Fragrecord::IDLE;
  fragptr.p->execSrNoReplicas = 0;
  fragptr.p->fragDistributionKey = 0;
  fragptr.p->activeTcCounter = 0;
  fragptr.p->tableFragptr = RNIL;
  fragptr.p->m_copy_started_state = 0;
}//Dblqh::initFragrec()

/* ========================================================================== 
 * =======       INITIATE FRAGMENT RECORD FOR SYSTEM RESTART          ======= 
 *
 *       SUBROUTINE SHORT NAME = IFS
 * ========================================================================= */

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
  /* We are reading the per file:mb metadata from page zero in this file
   * We cannot use the data for this file (fd 0), but the data for
   * previous files is valid.
   * So we start reading at fd 1.
   * The metadata for this file (fd 0) is set either reading the next file,
   * or by probing the last megabytes.
   */
  LogFileRecordPtr filePtr = logFilePtr;
  Uint32 pos = ZPAGE_HEADER_SIZE + ZFD_HEADER_SIZE;
  ndbrequire(noFdDescriptors <= cmaxValidLogFilesInPageZero);

  /* We start by initialising the previous file's metadata, 
   * so lets move there now...
   */
  filePtr.i = filePtr.p->prevLogFile;
  ptrCheckGuard(filePtr, clogFileFileSize, logFileRecord);
    
           
  for (Uint32 fd = 1; fd <= noFdDescriptors; fd++)
  {
    jam();
    for (Uint32 mb = 0; mb < clogFileSize; mb++)
    {
      jam();
      Uint32 pos0 = pos + fd * (ZFD_MBYTE_SIZE * clogFileSize) + mb;
      Uint32 pos1 = pos0 + clogFileSize;
      Uint32 pos2 = pos1 + clogFileSize;
      arrGuard(pos0, ZPAGE_SIZE);
      arrGuard(pos1, ZPAGE_SIZE);
      arrGuard(pos2, ZPAGE_SIZE);
      filePtr.p->logMaxGciCompleted[mb] = logPagePtr.p->logPageWord[pos0];
      filePtr.p->logMaxGciStarted[mb] = logPagePtr.p->logPageWord[pos1];
      filePtr.p->logLastPrepRef[mb] = logPagePtr.p->logPageWord[pos2];
    }
    if (fd + 1 <= noFdDescriptors)
    {
      jam();
      filePtr.i = filePtr.p->prevLogFile;
      ptrCheckGuard(filePtr, clogFileFileSize, logFileRecord);
    }
  }
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
  logPartPtr.p->gcprec = RNIL;
  logPartPtr.p->firstPageRef = RNIL;
  logPartPtr.p->lastPageRef = RNIL;
  logPartPtr.p->headFileNo = ZNIL;
  logPartPtr.p->headPageNo = ZNIL;
  logPartPtr.p->headPageIndex = ZNIL;
  logPartPtr.p->m_log_problems = 0;
  NdbLogPartInfo lpinfo(instance());
  ndbrequire(lpinfo.partCount == clogPartFileSize);
  logPartPtr.p->logPartNo = lpinfo.partNo[logPartPtr.i];
  logPartPtr.p->m_io_tracker.init(logPartPtr.p->logPartNo);
  logPartPtr.p->m_log_prepare_queue.init();
  logPartPtr.p->m_log_complete_queue.init();
}//Dblqh::initLogpart()

/* ========================================================================== 
 * =======              INITIATE LOG POINTERS                         ======= 
 *
 * ========================================================================= */
void Dblqh::initLogPointers(Signal* signal) 
{
  logPartPtr.i = tcConnectptr.p->m_log_part_ptr_i;
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
  LqhKeyReq::setGCIFlag(Treqinfo, 1);
/* ------------------------------------------------------------------------- */
/* SET OPERATION TYPE AND LOCK MODE (NEVER READ OPERATION OR SCAN IN LOG)    */
/* ------------------------------------------------------------------------- */
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
  regTcPtr->m_reorg = 0;
}//Dblqh::initReqinfoExecSr()

/* -------------------------------------------------------------------------- 
 * -------               INSERT FRAGMENT                              ------- 
 *
 * ------------------------------------------------------------------------- */
bool Dblqh::insertFragrec(Signal* signal, Uint32 fragId) 
{
  terrorCode = ZOK;
  if(c_fragment_pool.seize(fragptr) == false)
  {
    terrorCode = ZNO_FREE_FRAGMENTREC;
    return false;
  }
  for (Uint32 i = 0; i < NDB_ARRAY_SIZE(tabptr.p->fragid); i++) {
    jam();
    if (tabptr.p->fragid[i] == ZNIL) {
      jam();
      tabptr.p->fragid[i] = fragId;
      tabptr.p->fragrec[i] = fragptr.i;
      return true;
    }//if
  }//for
  c_fragment_pool.release(fragptr);
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
void
Dblqh::linkWaitLog(Signal* signal,
                   LogPartRecordPtr regLogPartPtr,
                   LogPartRecord::OperationQueue & queue)
{
  TcConnectionrecPtr lwlTcConnectptr;
/* -------------------------------------------------- */
/*       LINK ACTIVE OPERATION INTO QUEUE WAITING FOR */
/*       ACCESS TO THE LOG PART.                      */
/* -------------------------------------------------- */
  lwlTcConnectptr.i = queue.lastElement;
  if (lwlTcConnectptr.i == RNIL) {
    jam();
    queue.firstElement = tcConnectptr.i;
  } else {
    jam();
    ptrCheckGuard(lwlTcConnectptr, ctcConnectrecFileSize, tcConnectionrec);
    lwlTcConnectptr.p->nextTcLogQueue = tcConnectptr.i;
  }//if
  queue.lastElement = tcConnectptr.i;
  tcConnectptr.p->nextTcLogQueue = RNIL;
  regLogPartPtr.p->logPartState = LogPartRecord::ACTIVE;
  if (regLogPartPtr.p->LogLqhKeyReqSent == ZFALSE)
  {
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

  if (regLogPartPtr->m_log_prepare_queue.isEmpty() &&
      regLogPartPtr->m_log_complete_queue.isEmpty() &&
      (regLogPartPtr->waitWriteGciLog != LogPartRecord::WWGL_TRUE))
  {
// --------------------------------------------------------------------------
// Optimised route for the common case
// -------------------------------------------------------------------------- 
    return;
  }//if

  if (!regLogPartPtr->m_log_prepare_queue.isEmpty() ||
      !regLogPartPtr->m_log_complete_queue.isEmpty())
  {
    jam();
    regLogPartPtr->logPartState = LogPartRecord::ACTIVE;
    if (regLogPartPtr->LogLqhKeyReqSent == ZFALSE)
    {
      jam();
      regLogPartPtr->LogLqhKeyReqSent = ZTRUE;
      signal->theData[0] = ZLOG_LQHKEYREQ;
      signal->theData[1] = logPartPtr.i;
      sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
    }//if
  }

  if (regLogPartPtr->waitWriteGciLog != LogPartRecord::WWGL_TRUE)
  {
    jam();
    return;
  }
  else
  {
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
    for (lnsLogPartPtr.i = 0; lnsLogPartPtr.i < clogPartFileSize; lnsLogPartPtr.i++) {
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
  tcConnectptr.p->reclenAiLqhkey = 0;
  if (remainingLen == 0) {
    jam();
    return;
  }//if

  readLogData(signal, remainingLen, tcConnectptr.p->attrInfoIVal);
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
    tcConnectptr.p->gci_hi = logPagePtr.p->logPageWord[trclPageIndex + 7];
    tcConnectptr.p->gci_lo = 0;
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
    tcConnectptr.p->gci_hi = readLogword(signal);
    tcConnectptr.p->gci_lo = 0;
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

  if (DEBUG_REDO)
  {
    ndbout_c("readExecLog %u page at part: %u file: %u page: %u (mb: %u)",
             lfoPtr.p->noPagesRw,
             logPartPtr.p->logPartNo,
             logFilePtr.p->fileNo,
             logPartPtr.p->execSrStartPageNo,
             logPartPtr.p->execSrStartPageNo >> ZTWOLOG_NO_PAGES_IN_MBYTE);
  }
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

  if (DEBUG_REDO)
  {
    ndbout_c("readExecSr %u page at part: %u file: %u page: %u (mb: %u)",
             8,
             logPartPtr.p->logPartNo,
             logFilePtr.p->fileNo,
             tresPageid,
             tresPageid >> ZTWOLOG_NO_PAGES_IN_MBYTE);
  }
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

  readLogData(signal, remainingLen, tcConnectptr.p->keyInfoIVal);
}//Dblqh::readKey()

/* ------------------------------------------------------------------------- */
/* ------------ READ A NUMBER OF WORDS FROM LOG INTO CDATA  ---------------- */
/*                                                                           */
/*       SUBROUTINE SHORT NAME = RLD                                         */
/* --------------------------------------------------------------------------*/
void Dblqh::readLogData(Signal* signal, Uint32 noOfWords, Uint32& sectionIVal) 
{
  Uint32 logPos = logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX];
  if ((logPos + noOfWords) >= ZPAGE_SIZE) {
    for (Uint32 i = 0; i < noOfWords; i++)
    {
      /* Todo : Consider reading > 1 word at a time */
      Uint32 word= readLogwordExec(signal);
      bool ok= appendToSection(sectionIVal,
                               &word,
                               1);
      ndbrequire(ok);
    }
  } else {
    /* In one bite */
    bool ok= appendToSection(sectionIVal,
                             &logPagePtr.p->logPageWord[logPos],
                             noOfWords);
    ndbrequire(ok);
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
    tcConnectptr.p->m_row_id.m_page_no = logPagePtr.p->logPageWord[logPos + 6];
    tcConnectptr.p->m_row_id.m_page_idx = logPagePtr.p->logPageWord[logPos+ 7];
    logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] = logPos + ZLOG_HEAD_SIZE;
  } else {
    jam();
    readLogwordExec(signal);	/* IGNORE PREPARE LOG RECORD TYPE */
    readLogwordExec(signal);	/* IGNORE LOG RECORD SIZE         */
    tcConnectptr.p->hashValue = readLogwordExec(signal);
    tcConnectptr.p->operation = readLogwordExec(signal);
    tcConnectptr.p->totSendlenAi = readLogwordExec(signal);
    tcConnectptr.p->primKeyLen = readLogwordExec(signal);
    tcConnectptr.p->m_row_id.m_page_no = readLogwordExec(signal);
    tcConnectptr.p->m_row_id.m_page_idx = readLogwordExec(signal);
  }//if

  tcConnectptr.p->m_use_rowid = (tcConnectptr.p->operation == ZINSERT);
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

  if (DEBUG_REDO)
  {
    ndbout_c("readSinglePage 1 page at part: %u file: %u page: %u (mb: %u)",
             logPartPtr.p->logPartNo,
             logFilePtr.p->fileNo,
             pageNo,
             pageNo >> ZTWOLOG_NO_PAGES_IN_MBYTE);
  }
}//Dblqh::readSinglePage()

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
  c_fragment_pool.getPtr(fragptr);
  rwaTcPrevConnectptr.i = tcConnectptr.p->prevTc;
  rwaTcNextConnectptr.i = tcConnectptr.p->nextTc;
  if (tcConnectptr.p->listState != TcConnectionrec::WAIT_QUEUE_LIST) {
    jam();
    systemError(signal, __LINE__);
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
#ifndef NO_REDO_OPEN_FILE_CACHE
        closeFile_cache(signal, clfLogFilePtr, __LINE__);
#else
        clfLogFilePtr.p->logFileStatus = LogFileRecord::CLOSING_EXEC_LOG;
        closeFile(signal, clfLogFilePtr, __LINE__);
#endif
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

  addfragptr.p->accConnectptr = RNIL;
  addfragptr.p->tupConnectptr = RNIL;
  addfragptr.p->tuxConnectptr = RNIL;
  addfragptr.p->defValSectionI = RNIL;
  addfragptr.p->defValNextPos = 0;
  bzero(&addfragptr.p->m_createTabReq, sizeof(addfragptr.p->m_createTabReq));
  bzero(&addfragptr.p->m_lqhFragReq, sizeof(addfragptr.p->m_lqhFragReq));
  bzero(&addfragptr.p->m_addAttrReq, sizeof(addfragptr.p->m_addAttrReq));
  bzero(&addfragptr.p->m_dropFragReq, sizeof(addfragptr.p->m_dropFragReq));
  bzero(&addfragptr.p->m_dropTabReq, sizeof(addfragptr.p->m_dropTabReq));
  addfragptr.p->addfragErrorCode = 0;
  addfragptr.p->attrSentToTup = 0;
  addfragptr.p->attrReceived = 0;
  addfragptr.p->totalAttrReceived = 0;
}//Dblqh::seizeAddfragrec()

/* --------------------------------------------------------------------------
 * -------       SEIZE FRAGMENT RECORD                                ------- 
 *
 * ------------------------------------------------------------------------- */
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
  lqhTransConf->gci_hi          = tcConnectptr.p->gci_hi;
  lqhTransConf->nextNodeId1     = tcConnectptr.p->nextReplica;
  lqhTransConf->nextNodeId2     = tcConnectptr.p->nodeAfterNext[0];
  lqhTransConf->nextNodeId3     = tcConnectptr.p->nodeAfterNext[1];
  lqhTransConf->apiRef          = tcConnectptr.p->applRef;
  lqhTransConf->apiOpRec        = tcConnectptr.p->applOprec;
  lqhTransConf->tableId         = tcConnectptr.p->tableref;
  lqhTransConf->gci_lo          = tcConnectptr.p->gci_lo;
  lqhTransConf->fragId          = tcConnectptr.p->fragmentid;
  sendSignal(tcNodeFailptr.p->newTcBlockref, GSN_LQH_TRANSCONF, 
	     signal, LqhTransConf::SignalLength, JBB);
  tcNodeFailptr.p->tcRecNow = tcConnectptr.i + 1;
  signal->theData[0] = ZLQH_TRANS_NEXT;
  signal->theData[1] = tcNodeFailptr.i;
  sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);

  if (0)
  {
    ndbout_c("sending LQH_TRANSCONF %u transid: H'%.8x, H'%.8x op: %u state: %u(%u) marker: %u",
             tcConnectptr.i, 
             tcConnectptr.p->transid[0],
             tcConnectptr.p->transid[1],
             tcConnectptr.p->operation,           
             tcConnectptr.p->transactionState,
             stat,
             tcConnectptr.p->commitAckMarker);
  }
}//Dblqh::sendLqhTransconf()

/* --------------------------------------------------------------------------
 * -------               START ANOTHER PHASE OF LOG EXECUTION         -------
 *       RESET THE VARIABLES NEEDED BY THIS PROCESS AND SEND THE START SIGNAL
 *
 * ------------------------------------------------------------------------- */
void Dblqh::startExecSr(Signal* signal) 
{
  c_lcp_complete_fragments.first(fragptr);
  signal->theData[0] = fragptr.i;
  sendSignal(cownref, GSN_START_EXEC_SR, signal, 1, JBB);
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
    ptrCheckGuardErr(logPagePtr, clogPageFileSize, logPageRecord,
                     NDBD_EXIT_SR_REDOLOG);
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
  Uint32 gci = regTcPtr->gci_hi;
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

  if (ERROR_INSERTED(5051) && (logFilePtr.p->currentFilepage > 0) &&
      (logFilePtr.p->currentFilepage % 32) == 0)
  {
    SET_ERROR_INSERT_VALUE(5000);
  }

  logFilePtr.p->remainingWordsInMbyte = 
    logFilePtr.p->remainingWordsInMbyte - ZCOMPLETED_GCI_LOG_SIZE;

  if (DEBUG_REDO)
  {
    ndbout_c("writeCompletedGciLog gci: %u part: %u file: %u page: %u (mb: %u)",
             cnewestCompletedGci,
             logPartPtr.p->logPartNo,
             logFilePtr.p->fileNo,
             logFilePtr.p->currentFilepage,
             logFilePtr.p->currentFilepage >> ZTWOLOG_NO_PAGES_IN_MBYTE);
  }

  writeLogWord(signal, ZCOMPLETED_GCI_TYPE);
  writeLogWord(signal, cnewestCompletedGci);
  logPartPtr.p->logPartNewestCompletedGCI = cnewestCompletedGci;
}//Dblqh::writeCompletedGciLog()

/* --------------------------------------------------------------------------
 * -------         WRITE A DIRTY PAGE DURING LOG EXECUTION            ------- 
 * 
 *     SUBROUTINE SHORT NAME: WD
 * ------------------------------------------------------------------------- */
void Dblqh::writeDirty(Signal* signal, Uint32 place) 
{
  logPagePtr.p->logPageWord[ZPOS_DIRTY] = ZNOT_DIRTY;

  ndbassert(logPartPtr.p->prevFilepage ==
            logPagePtr.p->logPageWord[ZPOS_PAGE_NO]);
  writeDbgInfoPageHeader(logPagePtr, place, logPartPtr.p->prevFilepage,
                         ZPAGE_SIZE);
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

  ndbrequire(logFilePtr.p->fileRef != RNIL);

  logPartPtr.p->m_io_tracker.send_io(32768);

  if (DEBUG_REDO)
  {
    ndbout_c("writeDirty 1 page at part: %u file: %u page: %u (mb: %u)",
             logPartPtr.p->logPartNo,
             logFilePtr.p->fileNo,
             logPartPtr.p->prevFilepage,
             logPartPtr.p->prevFilepage >> ZTWOLOG_NO_PAGES_IN_MBYTE);
  }
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
    completedLogPage(signal, ZNORMAL, __LINE__);
    seizeLogpage(signal);
    initLogpage(signal);
    logFilePtr.p->currentLogpage = logPagePtr.i;
    logFilePtr.p->currentFilepage++;
  }//if
}//Dblqh::writeLogWord()

/* --------------------------------------------------------------------------
 * -------   WRITE MULTIPLE WORDS INTO THE LOG, CHECK FOR NEW PAGES   ------- 
 * 
 * ------------------------------------------------------------------------- */

void Dblqh::writeLogWords(Signal* signal, const Uint32* data, Uint32 len)
{
  Uint32 logPos = logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX];
  ndbrequire(logPos < ZPAGE_SIZE);
  Uint32 wordsThisPage= ZPAGE_SIZE - logPos;

  while (len >= wordsThisPage)
  {
    /* Fill rest of the log page */
    MEMCOPY_NO_WORDS(&logPagePtr.p->logPageWord[logPos],
                     data,
                     wordsThisPage);
    logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] = ZPAGE_SIZE;
    data+= wordsThisPage;
    len-= wordsThisPage;
    
    /* Mark page completed and get a new one */
    jam();
    completedLogPage(signal, ZNORMAL, __LINE__);
    seizeLogpage(signal);
    initLogpage(signal);
    logFilePtr.p->currentLogpage = logPagePtr.i;
    logFilePtr.p->currentFilepage++;
    
    logPos = logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX];
    ndbrequire(logPos < ZPAGE_SIZE);
    wordsThisPage= ZPAGE_SIZE - logPos;
  }
  
  if (len > 0)
  {
    /* No need to worry about next page */
    ndbassert( len < wordsThisPage );
    /* Write partial log page */
    MEMCOPY_NO_WORDS(&logPagePtr.p->logPageWord[logPos],
                     data,
                     len);
    logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] = logPos + len;
  }

  ndbassert( logPagePtr.p->logPageWord[ZCURR_PAGE_INDEX] < ZPAGE_SIZE );
}

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
  if (logFilePtr.p->currentMbyte == (clogFileSize - 1)) {
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
    completedLogPage(signal, ZLAST_WRITE_IN_FILE, __LINE__);
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
    completedLogPage(signal, ZENFORCE_WRITE, __LINE__);
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
  if (logFilePtr.p->fileNo == logPartPtr.p->logTailFileNo) 
  {
    if (logFilePtr.p->currentMbyte == logPartPtr.p->logTailMbyte)
    {
      jam();
/* -------------------------------------------------- */
/*       THE HEAD AND TAIL HAS MET. THIS SHOULD NEVER */
/*       OCCUR. CAN HAPPEN IF THE LOCAL CHECKPOINTS   */
/*       TAKE FAR TOO LONG TIME. SO TIMING PROBLEMS   */
/*       CAN INVOKE THIS SYSTEM CRASH. HOWEVER ONLY   */
/*       VERY SERIOUS TIMING PROBLEMS.                */
/* -------------------------------------------------- */
      char buf[100];
      BaseString::snprintf(buf, sizeof(buf), 
                           "Head/Tail met in REDO log, logpart: %u"
                           " file: %u mbyte: %u state: %u log-problem: %u",
                           logPartPtr.p->logPartNo,
                           logFilePtr.p->fileNo,
                           logFilePtr.p->currentMbyte,
                           logPartPtr.p->logPartState,
                           logPartPtr.p->m_log_problems);


      signal->theData[0] = 2398;
      execDUMP_STATE_ORD(signal);
      progError(__LINE__, NDBD_EXIT_NO_MORE_REDOLOG, buf);
      systemError(signal, __LINE__);
    }//if
  }//if
  if (logFilePtr.p->currentMbyte == (clogFileSize - 1)) {
    jam();
    twnlNextMbyte = 0;
    if (logFilePtr.p->fileChangeState != LogFileRecord::NOT_ONGOING)
    {
      jam();
      update_log_problem(signal, logPartPtr,
                         LogPartRecord::P_FILE_CHANGE_PROBLEM,
                         /* set */ true);
    }//if
    twnlNextFileNo = wnlNextLogFilePtr.p->fileNo;
  } else {
    jam();
    twnlNextMbyte = logFilePtr.p->currentMbyte + 1;
    twnlNextFileNo = logFilePtr.p->fileNo;
  }//if

  LogPosition head = { twnlNextFileNo, twnlNextMbyte };
  LogPosition tail = { logPartPtr.p->logTailFileNo, logPartPtr.p->logTailMbyte};
  Uint64 free_mb = free_log(head, tail, logPartPtr.p->noLogFiles, clogFileSize);
  if (free_mb <= c_free_mb_force_lcp_limit)
  {
    jam();
    force_lcp(signal);
  }

  if (free_mb <= c_free_mb_tail_problem_limit)
  {
    jam();
    update_log_problem(signal, logPartPtr, LogPartRecord::P_TAIL_PROBLEM, true);
  }

  if (ERROR_INSERTED(5058) &&
      (twnlNextMbyte + 3 >= clogFileSize) &&
      logFilePtr.p->fileNo != 0 &&
      logFilePtr.p->nextLogFile != logPartPtr.p->firstLogfile)
  {
    jam();
    srand((int)time(0));
    Uint32 wait = 3 + (rand() % 5);

    suspendFile(signal, logFilePtr, /* forever */ 0);
    suspendFile(signal, logPartPtr.p->firstLogfile, /* forever */ 0);
    signal->theData[0] = 9999;
    sendSignalWithDelay(CMVMI_REF, GSN_NDB_TAMPER, signal, wait * 1000, 1);
    CLEAR_ERROR_INSERT_VALUE;
  }

  if (ERROR_INSERTED(5059) &&
      twnlNextMbyte == 4 &&
      logFilePtr.p->fileNo != 0)
  {
    signal->theData[0] = 9999;
    sendSignal(CMVMI_REF, GSN_NDB_TAMPER, signal, 1, JBA);
  }

}//Dblqh::writeNextLog()

bool
Dblqh::validate_filter(Signal* signal)
{
  Uint32 * start = signal->theData + 1;
  Uint32 * end = signal->theData + signal->getLength();
  if (start == end)
  {
    infoEvent("No filter specified, not listing...");
    if (!ERROR_INSERTED(4002))
      return false;
    else
      return true;
  }

  while(start < end)
  {
    switch(* start){
    case 0: // Table
    case 1: // API Node
    case 3: // TC Node
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
Dblqh::match_and_print(Signal* signal, Ptr<TcConnectionrec> tcRec)
{
  Uint32 len = signal->getLength();
  Uint32* start = signal->theData + 3;
  Uint32* end = signal->theData + len;
  while (start < end)
  {
    switch(* start){
    case 0:
      if (tcRec.p->tableref != * (start + 1))
	return false;
      start += 2;
      break;
    case 1:
      if (refToNode(tcRec.p->applRef) != * (start + 1))
	return false;
      start += 2;
      break;
    case 2:
      if (tcRec.p->transid[0] != * (start + 1) ||
	  tcRec.p->transid[1] != * (start + 2))
	return false;
      start += 3;
      break;
    case 3:
      if (refToNode(tcRec.p->tcBlockref) != * (start + 1))
	return false;
      start += 2;
      break;
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

  char state[20];
  const char* op = "<Unknown>";
  if (tcRec.p->tcScanRec != RNIL)
  {
    ScanRecordPtr sp;
    sp.i = tcRec.p->tcScanRec;
    c_scanRecordPool.getPtr(sp);

    if (sp.p->scanLockMode)
      op = "SCAN-EX";
    else if(sp.p->scanLockHold)
      op = "SCAN-SH";
    else
      op = "SCAN";
    
    switch(sp.p->scanState){
    case ScanRecord::WAIT_NEXT_SCAN:
      BaseString::snprintf(state, sizeof(state), "WaitNextScan");
      break;
    case ScanRecord::IN_QUEUE:
      BaseString::snprintf(state, sizeof(state), "InQueue");
      break;
    case ScanRecord::SCAN_FREE:
    case ScanRecord::WAIT_STORED_PROC_COPY:
    case ScanRecord::WAIT_STORED_PROC_SCAN:
    case ScanRecord::WAIT_NEXT_SCAN_COPY:
    case ScanRecord::WAIT_DELETE_STORED_PROC_ID_SCAN:
    case ScanRecord::WAIT_DELETE_STORED_PROC_ID_COPY:
    case ScanRecord::WAIT_ACC_COPY:
    case ScanRecord::WAIT_ACC_SCAN:
    case ScanRecord::WAIT_SCAN_NEXTREQ:
    case ScanRecord::WAIT_CLOSE_SCAN:
    case ScanRecord::WAIT_CLOSE_COPY:
    case ScanRecord::WAIT_RELEASE_LOCK:
    case ScanRecord::WAIT_TUPKEY_COPY:
    case ScanRecord::WAIT_LQHKEY_COPY:
      BaseString::snprintf(state, sizeof(state), "%u", sp.p->scanState);
    }
  }
  else
  {
    switch(tcRec.p->operation){
    case ZREAD: 
      if (tcRec.p->lockType)
	op = "READ-EX";
      else if(!tcRec.p->dirtyOp)
	op = "READ-SH";
      else
	op = "READ";
      break;
    case ZINSERT: op = "INSERT"; break;
    case ZUPDATE: op = "UPDATE"; break;
    case ZDELETE: op = "DELETE"; break;
    case ZWRITE: op = "WRITE"; break;
    case ZUNLOCK: op = "UNLOCK"; break;
    }
    
    switch(tcRec.p->transactionState){
    case TcConnectionrec::IDLE:
    case TcConnectionrec::WAIT_ACC:
      BaseString::snprintf(state, sizeof(state), "In lock queue");
      break;
    case TcConnectionrec::WAIT_TUPKEYINFO:
    case TcConnectionrec::WAIT_ATTR:
      BaseString::snprintf(state, sizeof(state), "WaitData");
      break;
    case TcConnectionrec::WAIT_TUP:
      BaseString::snprintf(state, sizeof(state), "Running");
      break;
    case TcConnectionrec::WAIT_TUP_COMMIT:
      BaseString::snprintf(state, sizeof(state), "Committing");
      break;
    case TcConnectionrec::PREPARED:
      BaseString::snprintf(state, sizeof(state), "Prepared");
      break;
    case TcConnectionrec::COMMITTED:
      BaseString::snprintf(state, sizeof(state), "Committed");
      break;
    case TcConnectionrec::STOPPED:
    case TcConnectionrec::LOG_QUEUED:
    case TcConnectionrec::LOG_COMMIT_WRITTEN_WAIT_SIGNAL:
    case TcConnectionrec::LOG_COMMIT_QUEUED_WAIT_SIGNAL:
    case TcConnectionrec::COMMIT_STOPPED:
    case TcConnectionrec::LOG_COMMIT_QUEUED:
    case TcConnectionrec::COMMIT_QUEUED:
    case TcConnectionrec::WAIT_ACC_ABORT:
    case TcConnectionrec::ABORT_QUEUED:
    case TcConnectionrec::ABORT_STOPPED:
    case TcConnectionrec::WAIT_AI_AFTER_ABORT:
    case TcConnectionrec::LOG_ABORT_QUEUED:
    case TcConnectionrec::WAIT_TUP_TO_ABORT:
    case TcConnectionrec::WAIT_SCAN_AI:
    case TcConnectionrec::SCAN_STATE_USED:
    case TcConnectionrec::SCAN_FIRST_STOPPED:
    case TcConnectionrec::SCAN_CHECK_STOPPED:
    case TcConnectionrec::SCAN_STOPPED:
    case TcConnectionrec::SCAN_RELEASE_STOPPED:
    case TcConnectionrec::SCAN_CLOSE_STOPPED:
    case TcConnectionrec::COPY_CLOSE_STOPPED:
    case TcConnectionrec::COPY_FIRST_STOPPED:
    case TcConnectionrec::COPY_STOPPED:
    case TcConnectionrec::SCAN_TUPKEY:
    case TcConnectionrec::COPY_TUPKEY:
    case TcConnectionrec::TC_NOT_CONNECTED:
    case TcConnectionrec::PREPARED_RECEIVED_COMMIT:
    case TcConnectionrec::LOG_COMMIT_WRITTEN:
      BaseString::snprintf(state, sizeof(state), "%u", 
			   tcRec.p->transactionState);
    }
  }
  
  char buf[100];
  BaseString::snprintf(buf, sizeof(buf),
		       "OP[%u]: Tab: %d frag: %d TC: %u API: %d(0x%x)"
		       "transid: H'%.8x H'%.8x op: %s state: %s",
		       tcRec.i,
		       tcRec.p->tableref, 
		       tcRec.p->fragmentid,
		       refToNode(tcRec.p->tcBlockref),
		       refToNode(tcRec.p->applRef),
		       refToBlock(tcRec.p->applRef),
		       tcRec.p->transid[0], tcRec.p->transid[1],
		       op,
		       state);

  if (!ERROR_INSERTED(4002))
    infoEvent("%s", buf);
  else
    ndbout_c("%s", buf);

  memcpy(signal->theData, temp, 4*len);
  return true;
}

void
Dblqh::execDUMP_STATE_ORD(Signal* signal)
{
  jamEntry();
  DumpStateOrd * const dumpState = (DumpStateOrd *)&signal->theData[0];
  Uint32 arg= dumpState->args[0];
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
      infoEvent("CommitAckMarker: i = %d (H'%.8x, H'%.8x)"
		" ApiRef: 0x%x apiOprec: 0x%x TcNodeId: %d, ref_count: %u",
		iter.curr.i,
		iter.curr.p->transid1,
		iter.curr.p->transid2,
		iter.curr.p->apiRef,
		iter.curr.p->apiOprec,
		iter.curr.p->tcNodeId,
                iter.curr.p->reference_count);
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
	infoEvent("Table %d Status: %d Usage: [ r: %u w: %u ]",
		  i, tabPtr.p->tableStatus,
                  tabPtr.p->usageCountR, tabPtr.p->usageCountW);

	for (Uint32 j = 0; j<NDB_ARRAY_SIZE(tabPtr.p->fragrec); j++)
	{
	  FragrecordPtr fragPtr;
	  if ((fragPtr.i = tabPtr.p->fragrec[j]) != RNIL)
	  {
	    c_fragment_pool.getPtr(fragPtr);
	    infoEvent("  frag: %d distKey: %u", 
		      tabPtr.p->fragid[j],
		      fragPtr.p->fragDistributionKey);
	  }
	}
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
    c_scanRecordPool.getPtr(sp);
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
    c_scanRecordPool.getPtrIgnoreAlloc(sp);
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
    infoEvent(" errCnt=%d, schV=%d",
	      sp.p->scanErrorCounter,
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
    infoEvent(" LcpScan=%d  RowId(%u:%u)",
              sp.p->lcpScan,
              sp.p->m_row_id.m_page_no,
              sp.p->m_row_id.m_page_idx);
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
    infoEvent(" lcpState=%d lastFragmentFlag=%d", 
	      TlcpPtr.p->lcpState, TlcpPtr.p->lastFragmentFlag);
    infoEvent("currentFragment.fragPtrI=%d",
	      TlcpPtr.p->currentFragment.fragPtrI);
    infoEvent("currentFragment.lcpFragOrd.tableId=%d",
	      TlcpPtr.p->currentFragment.lcpFragOrd.tableId);
    infoEvent(" lcpQueued=%d reportEmpty=%d",
	      TlcpPtr.p->lcpQueued,
	      TlcpPtr.p->reportEmpty);
    char buf[8*_NDB_NODE_BITMASK_SIZE+1];
    infoEvent(" m_EMPTY_LCP_REQ=%s",
	      TlcpPtr.p->m_EMPTY_LCP_REQ.getText(buf));
    
    if ((signal->length() == 2) &&
        (dumpState->args[1] == 0))
    {
      /* Dump reserved LCP scan rec */
      /* As there's only one, we'll do a tight loop here */
      infoEvent(" dumping reserved scan records");
      for (Uint32 rec=0; rec < cscanrecFileSize; rec++)
      {
        ScanRecordPtr sp;
        sp.i = rec;
        c_scanRecordPool.getPtrIgnoreAlloc(sp);

        if (sp.p->m_reserved &&
            sp.p->lcpScan)
        {
          dumpState->args[0] = DumpStateOrd::LqhDumpOneScanRec;
          dumpState->args[1] = rec;
          execDUMP_STATE_ORD(signal);
        }
      }
    }

    return;
  }
  if (dumpState->args[0] == DumpStateOrd::LQHLogFileInitStatus){
     reportStatus(signal);
     return;
  }

#ifdef ERROR_INSERT
#ifdef NDB_DEBUG_FULL
  if(dumpState->args[0] == DumpStateOrd::LCPContinue){
    switch(cerrorInsert){
    case 5904:
      CLEAR_ERROR_INSERT_VALUE;
      g_trace_lcp.restore(*globalData.getBlock(BACKUP), signal);
      return;
    default:
      return;
    }
  }
#endif
#endif

  if(arg == 2304 || arg == 2305)
  {
    jam();
    Uint32 i;
    void * logPartPtr = 0;
    (void)logPartPtr;
    GcpRecordPtr gcp; gcp.i = RNIL;
    for(i = 0; i < clogPartFileSize; i++)
    {
      Ptr<LogPartRecord> lp;
      lp.i = i;
      ptrCheckGuard(lp, clogPartFileSize, logPartRecord);
      ndbout_c("LP %d blockInstance: %d partNo: %d state: %d WW_Gci: %d gcprec: %d flq: %u %u currfile: %d tailFileNo: %d logTailMbyte: %d cnoOfLogPages: %u problems: 0x%x",
               i,
               instance(),
               lp.p->logPartNo,
	       lp.p->logPartState,
	       lp.p->waitWriteGciLog,
	       lp.p->gcprec,
	       lp.p->m_log_prepare_queue.firstElement,
	       lp.p->m_log_complete_queue.firstElement,
	       lp.p->currentLogfile,
	       lp.p->logTailFileNo,
	       lp.p->logTailMbyte,
               cnoOfLogPages,
               lp.p->m_log_problems);
      
      if(gcp.i == RNIL && lp.p->gcprec != RNIL)
	gcp.i = lp.p->gcprec;

      LogFileRecordPtr logFilePtr;
      Uint32 first= logFilePtr.i= lp.p->firstLogfile;
      do
      {
	ptrCheckGuard(logFilePtr, clogFileFileSize, logFileRecord);
	ndbout_c("  file %d(%d)  FileChangeState: %d  logFileStatus: %d  currentMbyte: %d  currentFilepage %d", 
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
      progError(__LINE__, NDBD_EXIT_SYSTEM_ERROR, 
		"Please report this as a bug. "
		"Provide as much info as possible, expecially all the "
		"ndb_*_out.log files, Thanks. "
		"Shutting down node due to failed handling of GCP_SAVEREQ");
      
    }
  }

  if (dumpState->args[0] == DumpStateOrd::LqhErrorInsert5042 && (signal->getLength() >= 2))
  {
    c_error_insert_table_id = dumpState->args[1];
    if (signal->getLength() == 2)
    {
      SET_ERROR_INSERT_VALUE(5042);
    }
    else
    {
      SET_ERROR_INSERT_VALUE(dumpState->args[2]);
    }
  }

  TcConnectionrec *regTcConnectionrec = tcConnectionrec;
  Uint32 ttcConnectrecFileSize = ctcConnectrecFileSize;
  if(arg == 2306)
  {
    Uint32 bucketLen[1024];
    for(Uint32 i = 0; i<1024; i++)
    {
      TcConnectionrecPtr tcRec;
      tcRec.i = ctransidHash[i];
      bucketLen[i] = 0;
      while(tcRec.i != RNIL)
      {
	ptrCheckGuard(tcRec, ttcConnectrecFileSize, regTcConnectionrec);
	ndbout << "TcConnectionrec " << tcRec.i;
	signal->theData[0] = 2307;
	signal->theData[1] = tcRec.i;
	execDUMP_STATE_ORD(signal);
	tcRec.i = tcRec.p->nextHashRec;
        bucketLen[i]++;
      }
    }
    ndbout << "LQH transid hash bucket lengths : " << endl;
    for (Uint32 i = 0; i < 1024; i++)
    {
      if (bucketLen[i] > 0)
      {
        ndbout << " bucket " << i << " len " << bucketLen[i] << endl;
      }
    }
    ndbout << "Done." << endl;
  }

  if(arg == 2307 || arg == 2308)
  {
    TcConnectionrecPtr tcRec;
    tcRec.i = signal->theData[1];
    ptrCheckGuard(tcRec, ttcConnectrecFileSize, regTcConnectionrec);
    
    ndbout << " transactionState = " << tcRec.p->transactionState<<endl;
    ndbout << " operation = " << tcRec.p->operation<<endl;
    ndbout << " tcNodeFailrec = " << tcRec.p->tcNodeFailrec
	   << " seqNoReplica = " << tcRec.p->seqNoReplica
	   << endl;
    ndbout << " replicaType = " << tcRec.p->replicaType
	   << " reclenAiLqhkey = " << tcRec.p->reclenAiLqhkey
	   << " opExec = " << tcRec.p->opExec
	   << endl;
    ndbout << " opSimple = " << tcRec.p->opSimple
	   << " nextSeqNoReplica = " << tcRec.p->nextSeqNoReplica
	   << " lockType = " << tcRec.p->lockType
	   << endl;
    ndbout << " lastReplicaNo = " << tcRec.p->lastReplicaNo
	   << " indTakeOver = " << tcRec.p->indTakeOver
	   << " dirtyOp = " << tcRec.p->dirtyOp
	   << endl;
    ndbout << " activeCreat = " << tcRec.p->activeCreat
	   << " tcBlockref = " << hex << tcRec.p->tcBlockref
	   << " reqBlockref = " << hex << tcRec.p->reqBlockref
	   << " primKeyLen = " << tcRec.p->primKeyLen
	   << " nrcopyflag = " << LqhKeyReq::getNrCopyFlag(tcRec.p->reqinfo) 
	   << endl;
    ndbout << " nextReplica = " << tcRec.p->nextReplica
	   << " tcBlockref = " << hex << tcRec.p->tcBlockref
	   << " reqBlockref = " << hex << tcRec.p->reqBlockref
	   << " primKeyLen = " << tcRec.p->primKeyLen
	   << endl;
    ndbout << " logStopPageNo = " << tcRec.p->logStopPageNo
	   << " logStartPageNo = " << tcRec.p->logStartPageNo
	   << " logStartPageIndex = " << tcRec.p->logStartPageIndex
	   << endl;
    ndbout << " errorCode = " << tcRec.p->errorCode
	   << " clientBlockref = " << hex << tcRec.p->clientBlockref
	   << " applRef = " << hex << tcRec.p->applRef
	   << " totSendlenAi = " << tcRec.p->totSendlenAi
	   << endl;
    ndbout << " totReclenAi = " << tcRec.p->totReclenAi
	   << " tcScanRec = " << tcRec.p->tcScanRec
	   << " tcScanInfo = " << tcRec.p->tcScanInfo
	   << " tcOprec = " << hex << tcRec.p->tcOprec
	   << endl;
    ndbout << " tableref = " << tcRec.p->tableref
	   << " simpleTcConnect = " << tcRec.p->simpleTcConnect
	   << " storedProcId = " << tcRec.p->storedProcId
	   << " schemaVersion = " << tcRec.p->schemaVersion
	   << endl;
    ndbout << " reqinfo = " << tcRec.p->reqinfo
	   << " reqRef = " << tcRec.p->reqRef
	   << " readlenAi = " << tcRec.p->readlenAi
	   << " prevTc = " << tcRec.p->prevTc
	   << endl;
    ndbout << " prevLogTcrec = " << tcRec.p->prevLogTcrec
	   << " prevHashRec = " << tcRec.p->prevHashRec
	   << " nodeAfterNext0 = " << tcRec.p->nodeAfterNext[0]
	   << " nodeAfterNext1 = " << tcRec.p->nodeAfterNext[1]
	   << endl;
    ndbout << " nextTcConnectrec = " << tcRec.p->nextTcConnectrec
	   << " nextTc = " << tcRec.p->nextTc
	   << " nextTcLogQueue = " << tcRec.p->nextTcLogQueue
	   << " nextLogTcrec = " << tcRec.p->nextLogTcrec
	   << endl;
    ndbout << " nextHashRec = " << tcRec.p->nextHashRec
	   << " logWriteState = " << tcRec.p->logWriteState
	   << " logStartFileNo = " << tcRec.p->logStartFileNo
	   << " listState = " << tcRec.p->listState
	   << endl;
    ndbout << " gci_hi = " << tcRec.p->gci_hi
           << " gci_lo = " << tcRec.p->gci_lo
	   << " fragmentptr = " << tcRec.p->fragmentptr
	   << " fragmentid = " << tcRec.p->fragmentid
	   << endl;
    ndbout << " hashValue = " << tcRec.p->hashValue
           << " currTupAiLen = " << tcRec.p->currTupAiLen
	   << " currReclenAi = " << tcRec.p->currReclenAi
	   << endl;
    ndbout << " tcTimer = " << tcRec.p->tcTimer
	   << " clientConnectrec = " << tcRec.p->clientConnectrec
	   << " applOprec = " << hex << tcRec.p->applOprec
	   << " abortState = " << tcRec.p->abortState
	   << endl;
    ndbout << " transid0 = " << hex << tcRec.p->transid[0]
	   << " transid1 = " << hex << tcRec.p->transid[1]
	   << " key[0] = " << getKeyInfoWordOrZero(tcRec.p, 0)
	   << " key[1] = " << getKeyInfoWordOrZero(tcRec.p, 1)
	   << endl;
    ndbout << " key[2] = " << getKeyInfoWordOrZero(tcRec.p, 2)
	   << " key[3] = " << getKeyInfoWordOrZero(tcRec.p, 3)
	   << " m_nr_delete.m_cnt = " << tcRec.p->m_nr_delete.m_cnt
	   << endl;
    switch (tcRec.p->transactionState) {
	
    case TcConnectionrec::SCAN_STATE_USED:
      if (tcRec.p->tcScanRec < cscanrecFileSize){
	ScanRecordPtr TscanPtr;
	c_scanRecordPool.getPtr(TscanPtr, tcRec.p->tcScanRec);
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
      } else{
	ndbout << "No connected scan record found" << endl;
      }
      break;
    default:
      break;
    }
    ndbrequire(arg != 2308);
  }

#ifdef NDBD_TRACENR
  if (arg == 5712 || arg == 5713)
  {
    if (arg == 5712)
    {
      traceopout = &ndbout;
    } 
    else if (arg == 5713)
    {
      traceopout = tracenrout;
    }
    SET_ERROR_INSERT_VALUE(arg);
  }
#endif
  
  if (arg == 2350)
  {
    jam();
    Uint32 len = signal->getLength() - 1;
    if (len + 3 > 25)
    {
      jam();
      infoEvent("Too long filter");
      return;
    }
    if (validate_filter(signal))
    {
      jam();
      memmove(signal->theData + 3, signal->theData + 1, 4 * len);
      signal->theData[0] = 2351;
      signal->theData[1] = 0;    // Bucket
      signal->theData[2] = RNIL; // Record
      sendSignal(reference(), GSN_DUMP_STATE_ORD, signal, len + 3, JBB);
      
      infoEvent("Starting dump of operations");
    }
    return;
  }

  if (arg == 2351)
  {
    jam();
    Uint32 bucket = signal->theData[1];
    Uint32 record = signal->theData[2];
    Uint32 len = signal->getLength();
    TcConnectionrecPtr tcRec;
    if (record != RNIL)
    {
      jam();
      /**
       * Check that record is still in use...
       */
      tcRec.i = record;
      ptrCheckGuard(tcRec, ttcConnectrecFileSize, regTcConnectionrec);

      Uint32 hashIndex = (tcRec.p->transid[0] ^ tcRec.p->tcOprec) & 1023;
      if (hashIndex != bucket)
      {
	jam();
	record = RNIL;
      }
      else
      {
	jam();
	if (tcRec.p->nextHashRec == RNIL && 
	    tcRec.p->prevHashRec == RNIL &&
	    ctransidHash[hashIndex] != record)
	{
	  jam();
	  record = RNIL;
	}
      }
      
      if (record == RNIL)
      {
	jam();
	signal->theData[2] = RNIL;
	sendSignal(reference(), GSN_DUMP_STATE_ORD, signal, 
		   signal->getLength(), JBB);	
	return;
      }
    }
    else if ((record = ctransidHash[bucket]) == RNIL)
    {
      jam();
      bucket++;
      if (bucket < 1024)
      {
	jam();
	signal->theData[1] = bucket;
	signal->theData[2] = RNIL;
	sendSignal(reference(), GSN_DUMP_STATE_ORD, signal, 
		   signal->getLength(), JBB);	
      }
      else
      {
	jam();
        infoEvent("End of operation dump");
        if (ERROR_INSERTED(4002))
        {
          ndbrequire(false);
        }
      }

      return;
    } 
    else
    {
      jam();
      tcRec.i = record;
      ptrCheckGuard(tcRec, ttcConnectrecFileSize, regTcConnectionrec);      
    }

    for (Uint32 i = 0; i<32; i++)
    {
      jam();
      bool print = match_and_print(signal, tcRec);
      
      tcRec.i = tcRec.p->nextHashRec;
      if (tcRec.i == RNIL || print)
      {
	jam();
	break;
      }
      
      ptrCheckGuard(tcRec, ttcConnectrecFileSize, regTcConnectionrec);
    }
    
    if (tcRec.i == RNIL)
    {
      jam();
      bucket++;
      if (bucket < 1024)
      {
	jam();
	signal->theData[1] = bucket;
	signal->theData[2] = RNIL;
	sendSignal(reference(), GSN_DUMP_STATE_ORD, signal, len, JBB);
      }
      else
      {
	jam();
        infoEvent("End of operation dump");
        if (ERROR_INSERTED(4002))
        {
          ndbrequire(false);
        }
      }
      
      return;
    }
    else
    {
      jam();
      signal->theData[2] = tcRec.i;
      sendSignalWithDelay(reference(), GSN_DUMP_STATE_ORD, signal, 200, len);
      return;
    }
  }

  if (arg == 2352 && signal->getLength() == 2)
  {
    jam();
    Uint32 opNo = signal->theData[1];
    TcConnectionrecPtr tcRec;
    if (opNo < ttcConnectrecFileSize)
    {
      jam();
      tcRec.i = opNo;
      ptrCheckGuard(tcRec, ttcConnectrecFileSize, regTcConnectionrec);

      BaseString key;
      if (tcRec.p->keyInfoIVal != RNIL)
      {
        jam();
        SectionReader keyInfoReader(tcRec.p->keyInfoIVal,
                                    g_sectionSegmentPool);
        
        Uint32 keyWord;
        while (keyInfoReader.getWord(&keyWord))
          key.appfmt("0x%x ", keyWord);
      }
      
      char buf[100];
      BaseString::snprintf(buf, sizeof(buf),
			   "OP[%u]: transid: 0x%x 0x%x key: %s",
			   tcRec.i,
			   tcRec.p->transid[0], tcRec.p->transid[1], key.c_str());
      infoEvent("%s", buf);
    }
  }
  
  if (arg == DumpStateOrd::SchemaResourceSnapshot)
  {
    RSS_AP_SNAPSHOT_SAVE(c_fragment_pool);
    return;
  }

  if (arg == DumpStateOrd::SchemaResourceCheckLeak)
  {
    RSS_AP_SNAPSHOT_CHECK(c_fragment_pool);
    return;
  }

  if (arg == 4002)
  {
    bool ops = false;
    for (Uint32 i = 0; i<1024; i++)
    {
      if (ctransidHash[i] != RNIL)
      {
        jam();
        ops = true;
        break;
      }
    }

    bool markers = m_commitAckMarkerPool.getNoOfFree() != 
      m_commitAckMarkerPool.getSize();
    if (unlikely(ops || markers))
    {

      if (markers)
      {
        ndbout_c("LQH: m_commitAckMarkerPool: %d free size: %d",
                 m_commitAckMarkerPool.getNoOfFree(),
                 m_commitAckMarkerPool.getSize());
        
        CommitAckMarkerIterator iter;
        for(m_commitAckMarkerHash.first(iter); iter.curr.i != RNIL;
            m_commitAckMarkerHash.next(iter))
        {
          ndbout_c("CommitAckMarker: i = %d (H'%.8x, H'%.8x)"
                   " ApiRef: 0x%x apiOprec: 0x%x TcNodeId: %d ref_count: %u",
                   iter.curr.i,
                   iter.curr.p->transid1,
                   iter.curr.p->transid2,
                   iter.curr.p->apiRef,
                   iter.curr.p->apiOprec,
                   iter.curr.p->tcNodeId,
                   iter.curr.p->reference_count);
        }
      }
      SET_ERROR_INSERT_VALUE(4002);
      signal->theData[0] = 2350;
      EXECUTE_DIRECT(DBLQH, GSN_DUMP_STATE_ORD, signal, 1);
    }
  }

  if(arg == 2399)
  {
    jam();

    if (cstartRecReq < SRR_REDO_COMPLETE)
    {
      jam();
      return;
    }

    for(Uint32 i = 0; i<4; i++)
    {
      logPartPtr.i = i;
      ptrCheckGuard(logPartPtr, clogPartFileSize, logPartRecord);
      LogFileRecordPtr logFile;
      logFile.i = logPartPtr.p->currentLogfile;
      ptrCheckGuard(logFile, clogFileFileSize, logFileRecord);
      
      LogPosition head = { logFile.p->fileNo, logFile.p->currentMbyte };
      LogPosition tail = { logPartPtr.p->logTailFileNo, 
                           logPartPtr.p->logTailMbyte};
      Uint64 mb = free_log(head, tail, logPartPtr.p->noLogFiles, clogFileSize);
      Uint64 total = logPartPtr.p->noLogFiles * Uint64(clogFileSize);
      signal->theData[0] = NDB_LE_RedoStatus;
      signal->theData[1] = i;
      signal->theData[2] = head.m_file_no;
      signal->theData[3] = head.m_mbyte;
      signal->theData[4] = tail.m_file_no;
      signal->theData[5] = tail.m_mbyte;
      signal->theData[6] = Uint32(total >> 32);
      signal->theData[7] = Uint32(total);
      signal->theData[8] = Uint32(mb >> 32);
      signal->theData[9] = Uint32(mb);
      signal->theData[10] = logPartPtr.p->noLogFiles;
      signal->theData[11] = clogFileSize;
      sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 12, JBB);
    }
  }

  if(arg == 2398)
  {
    jam();

    if (cstartRecReq < SRR_REDO_COMPLETE)
    {
      jam();
      return;
    }

    for(Uint32 i = 0; i<clogPartFileSize; i++)
    {
      logPartPtr.i = i;
      ptrCheckGuard(logPartPtr, clogPartFileSize, logPartRecord);
      LogFileRecordPtr logFile;
      logFile.i = logPartPtr.p->currentLogfile;
      ptrCheckGuard(logFile, clogFileFileSize, logFileRecord);
      
      LogPosition head = { logFile.p->fileNo, logFile.p->currentMbyte };
      LogPosition tail = { logPartPtr.p->logTailFileNo, 
                           logPartPtr.p->logTailMbyte};
      Uint64 mb = free_log(head, tail, logPartPtr.p->noLogFiles, clogFileSize);
      Uint64 total = logPartPtr.p->noLogFiles * Uint64(clogFileSize);
      ndbout_c("REDO part: %u HEAD: file: %u mbyte: %u TAIL: file: %u mbyte: %u total: %llu free: %llu (mb)",
               logPartPtr.p->logPartNo, 
               head.m_file_no, head.m_mbyte,
               tail.m_file_no, tail.m_mbyte,
               total, mb);
    }
  }

#if defined VM_TRACE || defined ERROR_INSERT
  if (arg == 2396 && signal->length() == 2)
      cmaxLogFilesInPageZero_DUMP = dumpState->args[1];
#endif

  if (arg == 2397)
  {
    /* Send LCP_STATUS_REQ to BACKUP */
    LcpStatusReq* req = (LcpStatusReq*) signal->getDataPtr();
    req->senderRef = reference();
    req->senderData = 0;
    
    BlockReference backupRef = calcInstanceBlockRef(BACKUP);
    sendSignal(backupRef, GSN_LCP_STATUS_REQ, signal,
               LcpStatusReq::SignalLength, JBB);
  }


  if(arg == 2309)
  {
    CRASH_INSERTION(5075);

    progError(__LINE__, NDBD_EXIT_LCP_SCAN_WATCHDOG_FAIL,
              "Please report this as a bug. "
              "Provide as much info as possible, expecially all the "
              "ndb_*_out.log files, Thanks. "
              "Shutting down node due to lack of LCP fragment scan progress");  
  }

  if (arg == 5050)
  {
#ifdef ERROR_INSERT
    SET_ERROR_INSERT_VALUE2(5050, c_master_node_id);
#endif
  }
  
  if (arg == DumpStateOrd::LqhDumpPoolLevels)
  {
    /* Dump some state info for internal buffers */
    if (signal->getLength() == 1)
    {
      signal->theData[1] = 1;
      signal->theData[2] = 0;
      signal->theData[3] = 0;
      sendSignal(reference(), GSN_DUMP_STATE_ORD, signal, 4, JBB);
      return;
    }
    if (signal->getLength() != 4)
    {
      ndbout_c("DUMP LqhDumpPoolLevels : Bad signal length : %u", signal->getLength());
      return;
    }

    Uint32 resource = signal->theData[1];
    Uint32 position = signal->theData[2];
    Uint32 sum = signal->theData[3];
    /*const Uint32 MAX_ITER = 200; */
    
    switch(resource)
    {
    case 1:
    {
      /* Must get all in one loop, as we're traversing a dynamic list */
      sum = 0;
      TcConnectionrecPtr tcp;    
      tcp.i = cfirstfreeTcConrec;
      while (tcp.i != RNIL)
      {
        sum++;
        ptrCheckGuard(tcp, ctcConnectrecFileSize, tcConnectionrec);
        tcp.i = tcp.p->nextTcConnectrec;
      }
      infoEvent("LQH : TcConnection (operation) records in use/total %u/%u (%u bytes each)",
                ctcConnectrecFileSize - sum, ctcConnectrecFileSize, (Uint32) sizeof(TcConnectionrec));
      resource++;
      position = 0;
      sum = 0;
      break;
    }
    case 2:
    {
      infoEvent("LQH : ScanRecord (Fragment) pool in use/total %u/%u (%u bytes each)",
                c_scanRecordPool.getSize()-
                c_scanRecordPool.getNoOfFree(),
                c_scanRecordPool.getSize(),
                (Uint32) sizeof(ScanRecord));
      resource++;
      position = 0;
      sum = 0;
      break;
    }
    default:
      return;
    }

    signal->theData[0] = DumpStateOrd::LqhDumpPoolLevels;
    signal->theData[1] = resource;
    signal->theData[2] = position;
    signal->theData[3] = sum;
    sendSignal(reference(), GSN_DUMP_STATE_ORD, signal, 4, JBB);
    return; 
  }

}//Dblqh::execDUMP_STATE_ORD()


void Dblqh::execDBINFO_SCANREQ(Signal *signal)
{
  DbinfoScanReq req= *(DbinfoScanReq*)signal->theData;
  const Ndbinfo::ScanCursor* cursor =
    CAST_CONSTPTR(Ndbinfo::ScanCursor, DbinfoScan::getCursorPtr(&req));
  Ndbinfo::Ratelimit rl;

  jamEntry();

  switch(req.tableId){
  case Ndbinfo::LOGSPACES_TABLEID:
  {
    Uint32 logpart = cursor->data[0];
    while(logpart < clogPartFileSize)
    {
      jam();

      logPartPtr.i = logpart;
      ptrCheckGuard(logPartPtr, clogPartFileSize, logPartRecord);

      LogFileRecordPtr logFile;
      logFile.i = logPartPtr.p->currentLogfile;
      ptrCheckGuard(logFile, clogFileFileSize, logFileRecord);

      LogPosition head = { logFile.p->fileNo, logFile.p->currentMbyte };
      LogPosition tail = { logPartPtr.p->logTailFileNo,
                           logPartPtr.p->logTailMbyte};
      Uint64 mb = free_log(head, tail, logPartPtr.p->noLogFiles, clogFileSize);
      Uint64 total = logPartPtr.p->noLogFiles * Uint64(clogFileSize);
      Uint64 high = 0; // TODO

      Ndbinfo::Row row(signal, req);
      row.write_uint32(getOwnNodeId());
      row.write_uint32(0);              // log type, 0 = REDO
      row.write_uint32(0);              // log id, always 0 in LQH
      row.write_uint32(logPartPtr.p->logPartNo); // log part

      row.write_uint64(total*1024*1024);          // total allocated
      row.write_uint64((total-mb)*1024*1024);     // currently in use
      row.write_uint64(high*1024*1024);           // in use high water mark
      ndbinfo_send_row(signal, req, row, rl);
      logpart++;
      if (rl.need_break(req))
      {
        jam();
        ndbinfo_send_scan_break(signal, req, rl, logpart);
        return;
      }
    }
    break;
  }

  case Ndbinfo::LOGBUFFERS_TABLEID:
  {
    const size_t entry_size = sizeof(LogPageRecord);
    const Uint64 free = cnoOfLogPages;
    const Uint64 total = clogPageCount;
    const Uint64 high = 0; // TODO

    Ndbinfo::Row row(signal, req);
    row.write_uint32(getOwnNodeId());
    row.write_uint32(0);              // log type, 0 = REDO
    row.write_uint32(0);              // log id, always 0 in LQH
    row.write_uint32(instance());     // log part, instance for ndbmtd

    row.write_uint64(total*entry_size);        // total allocated
    row.write_uint64((total-free)*entry_size); // currently in use
    row.write_uint64(high*entry_size);         // in use high water mark
    ndbinfo_send_row(signal, req, row, rl);

    break;
  }

  case Ndbinfo::COUNTERS_TABLEID:
  {
    Ndbinfo::counter_entry counters[] = {
      { Ndbinfo::OPERATIONS_COUNTER, c_Counters.operations }
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
  case Ndbinfo::OPERATIONS_TABLEID:{
    Uint32 bucket = cursor->data[0];

    while (true)
    {
      if (rl.need_break(req))
      {
        jam();
        ndbinfo_send_scan_break(signal, req, rl, bucket);
        return;
      }

      for (; bucket < NDB_ARRAY_SIZE(ctransidHash); bucket++)
      {
        if (ctransidHash[bucket] != RNIL)
          break;
      }

      if (bucket == NDB_ARRAY_SIZE(ctransidHash))
      {
        break;
      }

      TcConnectionrecPtr tcPtr;
      tcPtr.i = ctransidHash[bucket];
      while (tcPtr.i != RNIL)
      {
        jam();
        ptrCheckGuard(tcPtr, ctcConnectrecFileSize, tcConnectionrec);
        Ndbinfo::Row row(signal, req);
        ndbinfo_write_op(row, tcPtr);
        ndbinfo_send_row(signal, req, row, rl);
        tcPtr.i = tcPtr.p->nextHashRec;
      }
      bucket++;
    }
  }

  default:
    break;
  }

  ndbinfo_send_scan_conf(signal, req, rl);
}

void
Dblqh::ndbinfo_write_op(Ndbinfo::Row & row, TcConnectionrecPtr tcPtr)
{
  row.write_uint32(getOwnNodeId());
  row.write_uint32(instance());          // block instance
  row.write_uint32(tcPtr.i);             // objid
  row.write_uint32(tcPtr.p->tcBlockref); // tcref
  row.write_uint32(tcPtr.p->applRef);    // apiref

  row.write_uint32(tcPtr.p->transid[0]);
  row.write_uint32(tcPtr.p->transid[1]);
  row.write_uint32(tcPtr.p->tableref);
  row.write_uint32(tcPtr.p->fragmentid);

  if (tcPtr.p->tcScanRec != RNIL)
  {
    ScanRecordPtr sp;
    sp.i = tcPtr.p->tcScanRec;
    c_scanRecordPool.getPtr(sp);

    Uint32 op = NDB_INFO_OP_SCAN_UNKNOWN;
    if (sp.p->scanLockMode)
      op = NDB_INFO_OP_SCAN_EX;
    else if (sp.p->scanLockHold)
      op = NDB_INFO_OP_SCAN_SH;
    else
      op = NDB_INFO_OP_SCAN;

    row.write_uint32(op);
    row.write_uint32(sp.p->scanState);
    row.write_uint32(0);
  }
  else
  {
    Uint32 op = NDB_INFO_OP_UNKNOWN;
    switch(tcPtr.p->operation){
    case ZREAD:
      if (tcPtr.p->lockType)
	op = NDB_INFO_OP_READ_EX;
      else if (!tcPtr.p->dirtyOp)
	op = NDB_INFO_OP_READ_SH;
      else
        op = NDB_INFO_OP_READ;
      break;
    case ZINSERT:
      op = NDB_INFO_OP_INSERT;
      break;
    case ZUPDATE:
      op = NDB_INFO_OP_UPDATE;
      break;
    case ZDELETE:
      op = NDB_INFO_OP_DELETE;
      break;
    case ZWRITE:
      op = NDB_INFO_OP_WRITE;
      break;
    case ZUNLOCK:
      op = NDB_INFO_OP_UNLOCK;
      break;
    case ZREFRESH:
      op = NDB_INFO_OP_REFRESH;
      break;
    }
    row.write_uint32(op);
    row.write_uint32(tcPtr.p->transactionState);
    row.write_uint32(0);
  }
}


void
Dblqh::startLcpFragWatchdog(Signal* signal)
{
  jam();
  /* Must not already be running */
  /* Thread could still be active from a previous run */
  ndbrequire(c_lcpFragWatchdog.scan_running == false);
  c_lcpFragWatchdog.scan_running = true;
  
  /* If thread is not already active, start it */
  if (! c_lcpFragWatchdog.thread_active)
  {
    jam();
    invokeLcpFragWatchdogThread(signal);
  }

  ndbrequire(c_lcpFragWatchdog.thread_active == true);
}

void
Dblqh::invokeLcpFragWatchdogThread(Signal* signal)
{
  jam();
  ndbrequire(c_lcpFragWatchdog.scan_running);
  
  c_lcpFragWatchdog.thread_active = true;
  
  signal->getDataPtrSend()[0] = ZLCP_FRAG_WATCHDOG;
  sendSignalWithDelay(cownref, GSN_CONTINUEB, signal,
                      LCPFragWatchdog::PollingPeriodMillis, 1);
  
  LcpStatusReq* req = (LcpStatusReq*)signal->getDataPtr();
  req->senderRef = cownref;
  req->senderData = 1;
  BlockReference backupRef = calcInstanceBlockRef(BACKUP);
  sendSignal(backupRef, GSN_LCP_STATUS_REQ, signal,
             LcpStatusReq::SignalLength, JBB);
}

void
Dblqh::execLCP_STATUS_CONF(Signal* signal)
{
  jamEntry();
  LcpStatusConf* conf = (LcpStatusConf*) signal->getDataPtr();
  
  if (conf->senderData == 0)
  {
    /* DUMP STATE variant */
    ndbout_c("Received LCP_STATUS_CONF from %x", conf->senderRef);
    ndbout_c("  Status = %u, Table = %u, Frag = %u",
             conf->lcpState,
             conf->tableId,
             conf->fragId);
    ndbout_c("  Completion State %llu",
             (((Uint64)conf->completionStateHi) << 32) + conf->completionStateLo);
    ndbout_c("  Lcp done rows %llu, done bytes %llu",
             (((Uint64)conf->lcpDoneRowsHi) << 32) + conf->lcpDoneRowsLo,
             (((Uint64)conf->lcpDoneBytesHi) << 32) + conf->lcpDoneBytesLo);
  }
  
  /* We can ignore the LCP status as if it's complete then we should
   * promptly stop watching
   */
  c_lcpFragWatchdog.handleLcpStatusRep((LcpStatusConf::LcpState)conf->lcpState,
                                       conf->tableId,
                                       conf->fragId,
                                       (((Uint64)conf->completionStateHi) << 32) + 
                                       conf->completionStateLo);
}

void
Dblqh::execLCP_STATUS_REF(Signal* signal)
{
  jamEntry();
  LcpStatusRef* ref = (LcpStatusRef*) signal->getDataPtr();

  ndbout_c("Received LCP_STATUS_REF from %x, senderData = %u with error code %u",
           ref->senderRef, ref->senderData, ref->error);

  ndbrequire(false);
}

void
Dblqh::LCPFragWatchdog::reset()
{
  jamBlock(block);
  scan_running = false;
  lcpState = LcpStatusConf::LCP_IDLE;
  tableId = ~Uint32(0);
  fragId = ~Uint32(0);
  completionStatus = ~Uint64(0);
  pollCount = 0;
}

void
Dblqh::LCPFragWatchdog::handleLcpStatusRep(LcpStatusConf::LcpState repLcpState,
                                           Uint32 repTableId,
                                           Uint32 repFragId,
                                           Uint64 repCompletionStatus)
{
  jamBlock(block);
  if (scan_running)
  {
    jamBlock(block);
    if ((repCompletionStatus != completionStatus) ||
        (repFragId != fragId) ||
        (repTableId != tableId) ||
        (repLcpState != lcpState))
    {
      jamBlock(block);
      /* Something moved since last time, reset
       * poll counter and data.
       */
      pollCount = 0;
      lcpState = repLcpState;
      tableId = repTableId;
      fragId = repFragId;
      completionStatus = repCompletionStatus;
    }
  }
}


/**
 * checkLcpFragWatchdog
 *
 * This method implements the LCP Frag watchdog 'thread', periodically
 * checking for progress in the current LCP fragment scan
 */
void
Dblqh::checkLcpFragWatchdog(Signal* signal)
{
  jam();
  ndbrequire(c_lcpFragWatchdog.thread_active == true);

  if (!c_lcpFragWatchdog.scan_running)
  {
    jam();
    /* We've been asked to stop */
    c_lcpFragWatchdog.thread_active = false;
    return;
  }

  c_lcpFragWatchdog.pollCount++;

  /* Check how long we've been waiting for progress on this scan */
  if (c_lcpFragWatchdog.pollCount >=
      LCPFragWatchdog::WarnPeriodsWithNoProgress)
  {
    jam();
    const char* completionStatusString = 
      (c_lcpFragWatchdog.lcpState == LcpStatusConf::LCP_SCANNING?
       "rows completed":
       "bytes remaining.");
    
    warningEvent("LCP Frag watchdog : No progress on table %u, frag %u for %u s."
                 "  %llu %s",
                 c_lcpFragWatchdog.tableId,
                 c_lcpFragWatchdog.fragId,
                 (LCPFragWatchdog::PollingPeriodMillis * 
                  c_lcpFragWatchdog.pollCount) / 1000,
                 c_lcpFragWatchdog.completionStatus,
                 completionStatusString);
    ndbout_c("LCP Frag watchdog : No progress on table %u, frag %u for %u s."
             "  %llu %s",
             c_lcpFragWatchdog.tableId,
             c_lcpFragWatchdog.fragId,
             (LCPFragWatchdog::PollingPeriodMillis * 
              c_lcpFragWatchdog.pollCount) / 1000,
             c_lcpFragWatchdog.completionStatus,
             completionStatusString);
    
    if (c_lcpFragWatchdog.pollCount >= 
        LCPFragWatchdog::MaxPeriodsWithNoProgress)
    {
      jam();
      /* Too long with no progress... */
      
      warningEvent("LCP Frag watchdog : Checkpoint of table %u fragment %u "
                   "too slow (no progress for > %u s).",
                   c_lcpFragWatchdog.tableId,
                   c_lcpFragWatchdog.fragId,
                   (LCPFragWatchdog::PollingPeriodMillis * 
                    LCPFragWatchdog::MaxPeriodsWithNoProgress) / 1000);
      ndbout_c("LCP Frag watchdog : Checkpoint of table %u fragment %u "
               "too slow (no progress for > %u s).",
               c_lcpFragWatchdog.tableId,
               c_lcpFragWatchdog.fragId,
               (LCPFragWatchdog::PollingPeriodMillis * 
                LCPFragWatchdog::MaxPeriodsWithNoProgress) / 1000);
      
      /* Dump some LCP state for debugging... */
      {
        DumpStateOrd* ds = (DumpStateOrd*) signal->getDataPtrSend();

        /* DIH : */
        ds->args[0] = DumpStateOrd::DihDumpLCPState;
        sendSignal(DBDIH_REF, GSN_DUMP_STATE_ORD, signal, 1, JBA);
        
        ds->args[0] = 7012;
        sendSignal(DBDIH_REF, GSN_DUMP_STATE_ORD, signal, 1, JBA);
        
        /* BACKUP : */
        ds->args[0] = 23;
        sendSignal(BACKUP_REF, GSN_DUMP_STATE_ORD, signal, 1, JBA);
        
        ds->args[0] = 24;
        ds->args[1] = 2424;
        sendSignal(BACKUP_REF, GSN_DUMP_STATE_ORD, signal, 2, JBA);

        /* LQH : */
        ds->args[0] = DumpStateOrd::LqhDumpLcpState;
        sendSignal(cownref, GSN_DUMP_STATE_ORD, signal, 1, JBA);
        
        /* Delay self-execution to give time for dump output */
        ds->args[0] = 2309;
        sendSignalWithDelay(cownref, GSN_DUMP_STATE_ORD, signal, 5*1000, 1);
      }
      
      return;
    }
  }
  
  invokeLcpFragWatchdogThread(signal);
}

void
Dblqh::stopLcpFragWatchdog()
{
  jam();
  /* Mark watchdog as no longer running, 
   * If the 'thread' is active then it will 
   * stop at the next wakeup
   */
  ndbrequire(c_lcpFragWatchdog.scan_running);
  c_lcpFragWatchdog.reset();
};
    
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
Dblqh::execCREATE_TRIG_IMPL_REQ(Signal* signal)
{
  jamEntry();

  if (!assembleFragments(signal))
  {
    jam();
    return;
  }

  CreateTrigImplReq* req = (CreateTrigImplReq*)signal->getDataPtrSend();
  SectionHandle handle(this, signal);
  req->senderRef = reference();
  BlockReference tupRef = calcInstanceBlockRef(DBTUP);
  sendSignal(tupRef, GSN_CREATE_TRIG_IMPL_REQ, signal,
             signal->getLength(), JBB, &handle);
}

void
Dblqh::execCREATE_TRIG_IMPL_CONF(Signal* signal)
{
  jamEntry();

  BlockReference dictRef = !isNdbMtLqh() ? DBDICT_REF : DBLQH_REF;
  sendSignal(dictRef, GSN_CREATE_TRIG_IMPL_CONF, signal,
             CreateTrigImplConf::SignalLength, JBB);
}

void
Dblqh::execCREATE_TRIG_IMPL_REF(Signal* signal)
{
  jamEntry();

  BlockReference dictRef = !isNdbMtLqh() ? DBDICT_REF : DBLQH_REF;
  sendSignal(dictRef, GSN_CREATE_TRIG_IMPL_REF, signal,
             CreateTrigImplRef::SignalLength, JBB);
}

void
Dblqh::execDROP_TRIG_IMPL_REQ(Signal* signal)
{
  jamEntry();

  DropTrigImplReq* req = (DropTrigImplReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  BlockReference tupRef = calcInstanceBlockRef(DBTUP);
  sendSignal(tupRef, GSN_DROP_TRIG_IMPL_REQ, signal,
             DropTrigImplReq::SignalLength, JBB);
}

void
Dblqh::execDROP_TRIG_IMPL_CONF(Signal* signal)
{
  jamEntry();

  BlockReference dictRef = !isNdbMtLqh() ? DBDICT_REF : DBLQH_REF;
  sendSignal(dictRef, GSN_DROP_TRIG_IMPL_CONF, signal,
             DropTrigImplConf::SignalLength, JBB);
}

void
Dblqh::execDROP_TRIG_IMPL_REF(Signal* signal)
{
  jamEntry();

  BlockReference dictRef = !isNdbMtLqh() ? DBDICT_REF : DBLQH_REF;
  sendSignal(dictRef, GSN_DROP_TRIG_IMPL_REF, signal,
             DropTrigImplRef::SignalLength, JBB);
}

Uint32 Dblqh::calcPageCheckSum(LogPageRecordPtr logP){
    Uint32 checkSum = 37;
#ifdef VM_TRACE
    for (Uint32 i = (ZPOS_CHECKSUM+1); i<ZPAGE_SIZE; i++)
      checkSum = logP.p->logPageWord[i] ^ checkSum;
#endif
    return checkSum;  
  }

#ifdef NDB_DEBUG_FULL
#ifdef ERROR_INSERT
void
TraceLCP::sendSignal(Uint32 ref, Uint32 gsn, Signal* signal,
		     Uint32 len, Uint32 prio)
{
  Sig s;
  s.type = Sig::Sig_send;
  s.header = signal->header;
  s.header.theVerId_signalNumber = gsn;
  s.header.theReceiversBlockNumber = ref;
  s.header.theLength = len;
  memcpy(s.theData, signal->theData, 4 * len);
  m_signals.push_back(s);
  assert(signal->getNoOfSections() == 0);
}

void
TraceLCP::save(Signal* signal){
  Sig s;
  s.type = Sig::Sig_save;
  s.header = signal->header;
  memcpy(s.theData, signal->theData, 4 * signal->getLength());
  m_signals.push_back(s);
  assert(signal->getNoOfSections() == 0);
}

void
TraceLCP::restore(SimulatedBlock& lqh, Signal* sig){
  Uint32 cnt = m_signals.size();
  for(Uint32 i = 0; i<cnt; i++){
    sig->header = m_signals[i].header;
    memcpy(sig->theData, m_signals[i].theData, 4 * sig->getLength());
    switch(m_signals[i].type){
    case Sig::Sig_send:
      lqh.sendSignal(sig->header.theReceiversBlockNumber,
		     sig->header.theVerId_signalNumber,
		     sig,
		     sig->header.theLength,
		     JBB);
      break;
    case Sig::Sig_save:
      lqh.executeFunction(sig->header.theVerId_signalNumber, sig);
      break;
    }
  }
  m_signals.clear();
}
#endif
#endif

void Dblqh::writeDbgInfoPageHeader(LogPageRecordPtr logP, Uint32 place,
                                   Uint32 pageNo, Uint32 wordWritten)
{
  logP.p->logPageWord[ZPOS_LOG_TIMER]= logPartPtr.p->logTimer;
  logP.p->logPageWord[ZPOS_PREV_PAGE_NO]= logP.p->logPageWord[ZPOS_PAGE_NO];
  logP.p->logPageWord[ZPOS_PAGE_I]= logP.i;
  logP.p->logPageWord[ZPOS_PLACE_WRITTEN_FROM]= place;
  logP.p->logPageWord[ZPOS_PAGE_NO]= pageNo;
  logP.p->logPageWord[ZPOS_PAGE_FILE_NO]= logFilePtr.p->fileNo;
  logP.p->logPageWord[ZPOS_WORD_WRITTEN]= wordWritten;
  logP.p->logPageWord[ZPOS_IN_WRITING]= 1;
}

void Dblqh::initReportStatus(Signal* signal){
  NDB_TICKS current_time = NdbTick_CurrentMillisecond();
  m_next_report_time = current_time + 
                       ((NDB_TICKS)m_startup_report_frequency) * ((NDB_TICKS)1000);
}

void Dblqh::checkReportStatus(Signal* signal){
  if (m_startup_report_frequency == 0)
    return;

  NDB_TICKS current_time = NdbTick_CurrentMillisecond();
  if (current_time > m_next_report_time)
  {
    reportStatus(signal);
    m_next_report_time = current_time +
                         ((NDB_TICKS)m_startup_report_frequency) * ((NDB_TICKS)1000);
  }
}

void Dblqh::reportStatus(Signal* signal){
  const int signal_length = 6;

  signal->theData[0] = NDB_LE_LogFileInitStatus;
  signal->theData[1] = reference();
  for (int i = 2; i < signal_length; i++)
    signal->theData[i] = 0;
  if (getNodeState().startLevel < NodeState::SL_STARTED){
    signal->theData[2] = totalLogFiles;
    signal->theData[3] = logFileInitDone;
    signal->theData[4] = totallogMBytes;
    signal->theData[5] = logMBytesInitDone;
  }
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, signal_length, JBB);
}

void Dblqh::logfileInitCompleteReport(Signal* signal){
  const int signal_length = 6;

  signal->theData[0] = NDB_LE_LogFileInitCompStatus;
  signal->theData[1] = reference();
  signal->theData[2] = totalLogFiles;
  signal->theData[3] = logFileInitDone;
  signal->theData[4] = totallogMBytes;
  signal->theData[5] = logMBytesInitDone;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, signal_length, JBB);
}

#ifdef NDBD_TRACENR
void
Dblqh::TRACE_OP_DUMP(const Dblqh::TcConnectionrec* regTcPtr, const char * pos)
{
  (* traceopout) 
    << "[ " << hex << regTcPtr->transid[0]
    << " " << hex << regTcPtr->transid[1] << " ] " << dec
    << pos 
    << " " << (Operation_t)regTcPtr->operation
    << " " << regTcPtr->tableref
    << "(" << regTcPtr->fragmentid << ")"
    << "(" << (regTcPtr->seqNoReplica == 0 ? "P" : "B") << ")" ;
  
  {
    (* traceopout) << "key=[" << hex;
    if (regTcPtr->keyInfoIVal != RNIL)
    {
      SectionReader keyInfoReader(regTcPtr->keyInfoIVal,
                                  g_sectionSegmentPool);
      
      Uint32 keyWord;
      while (keyInfoReader.getWord(&keyWord))
        (* traceopout) << hex << keyWord << " ";
    }
    (* traceopout) << "] ";
  }
  
  if (regTcPtr->m_use_rowid)
    (* traceopout) << " " << regTcPtr->m_row_id;
  (* traceopout) << endl;
}
#endif

Uint32
Dblqh::get_node_status(Uint32 nodeId) const
{
  HostRecordPtr Thostptr;
  Thostptr.i = nodeId;
  ptrCheckGuard(Thostptr, chostFileSize, hostRecord);
  return Thostptr.p->nodestatus;
}

#ifndef NO_REDO_PAGE_CACHE
/**
 * Don't cache pages if less then 64 pages are free
 */
#define MIN_REDO_PAGES_FREE 64

void
Dblqh::do_evict(RedoPageCache& cache, Ptr<RedoCacheLogPageRecord> pagePtr)
{
  LogPageRecordPtr save = logPagePtr;
  cache.m_lru.remove(pagePtr);
  cache.m_hash.remove(pagePtr);
  if (0)
  ndbout_c("evict part: %u file: %u page: %u cnoOfLogPages: %u",
           pagePtr.p->m_part_no,
           pagePtr.p->m_file_no,
           pagePtr.p->m_page_no,
           cnoOfLogPages);

  logPagePtr.i = pagePtr.i;
  ptrCheckGuard(logPagePtr, clogPageFileSize, logPageRecord);

  Ptr<LogPageRecord> prevPagePtr, nextPagePtr;
  prevPagePtr.i = logPagePtr.p->logPageWord[ZPREV_PAGE];
  nextPagePtr.i = logPagePtr.p->logPageWord[ZNEXT_PAGE];
  if (prevPagePtr.i != RNIL)
  {
    jam();
    /**
     * Remove ZNEXT pointer from prevPagePtr
     *   so we don't try to "serve" multi-page request
     *   if next-page has been evicted
     */
    ptrCheckGuard(prevPagePtr, clogPageFileSize, logPageRecord);
    ndbrequire(prevPagePtr.p->logPageWord[ZNEXT_PAGE] == logPagePtr.i);
    prevPagePtr.p->logPageWord[ZNEXT_PAGE] = RNIL;
  }

  if (nextPagePtr.i != RNIL)
  {
    jam();
    /**
     * Remove ZPREV pointer from nextPagePtr
     *   so don't try to do above if prev has been evicted
     */
    ptrCheckGuard(nextPagePtr, clogPageFileSize, logPageRecord);
    ndbrequire(nextPagePtr.p->logPageWord[ZPREV_PAGE] == logPagePtr.i);
    nextPagePtr.p->logPageWord[ZPREV_PAGE] = RNIL;
  }

  releaseLogpage(0);
  logPagePtr = save;
}

void
Dblqh::evict(RedoPageCache& cache, Uint32 cnt)
{
  while (cnoOfLogPages < (cnt + MIN_REDO_PAGES_FREE) && !cache.m_lru.isEmpty())
  {
    jam();
    Ptr<RedoCacheLogPageRecord> pagePtr;
    cache.m_lru.last(pagePtr);
    do_evict(cache, pagePtr);
  }
}

void
Dblqh::addCachePages(RedoPageCache& cache,
                     Uint32 partNo,
                     Uint32 startPageNo,
                     LogFileOperationRecord* lfoPtrP)
{
  Uint32 cnt = lfoPtrP->noPagesRw;
  Ptr<LogFileRecord> filePtr;
  filePtr.i = lfoPtrP->logFileRec;
  ptrCheckGuard(filePtr, clogFileFileSize, logFileRecord);

  evict(cache, 0);

  if (cnoOfLogPages < cnt + MIN_REDO_PAGES_FREE)
  {
    /**
     * Don't cache if low on redo-buffer
     */
    return;
  }

  for (Uint32 i = 0; i<cnt ; i++)
  {
    Ptr<RedoCacheLogPageRecord> pagePtr;
    pagePtr.i = lfoPtrP->logPageArray[i];
    cache.m_pool.getPtr(pagePtr);
    pagePtr.p->m_part_no = partNo;
    pagePtr.p->m_page_no = startPageNo + i;
    pagePtr.p->m_file_no = filePtr.p->fileNo;

    bool found = false;
    {
      RedoCacheLogPageRecord key;
      key.m_part_no = partNo;
      key.m_page_no = startPageNo + i;
      key.m_file_no = filePtr.p->fileNo;
      Ptr<RedoCacheLogPageRecord> tmp;
      if (cache.m_hash.find(tmp, key))
      {
        jam();
        found = true;
        do_evict(cache, tmp);
      }
    }

    cache.m_hash.add(pagePtr);
    cache.m_lru.addFirst(pagePtr);
    if (0)
    ndbout_c("adding(%u) part: %u file: %u page: %u cnoOfLogPages: %u cnt: %u",
             found,
             pagePtr.p->m_part_no,
             pagePtr.p->m_file_no,
             pagePtr.p->m_page_no,
             cnoOfLogPages,
             cnt);
  }

  /**
   * Make sure pages are not released when prepare-record is executed
   * @see releaseLfoPages
   */
  lfoPtrP->firstLfoPage = RNIL;
}

void
Dblqh::release(RedoPageCache& cache)
{
  while (!cache.m_lru.isEmpty())
  {
    jam();
    Ptr<RedoCacheLogPageRecord> pagePtr;
    cache.m_lru.last(pagePtr);
    cache.m_lru.remove(pagePtr);

    logPagePtr.i = pagePtr.i;
    ptrCheckGuard(logPagePtr, clogPageFileSize, logPageRecord);
    releaseLogpage(0);
  }
  cache.m_hash.removeAll();

#if defined VM_TRACE || defined ERROR_INSERT || 1
  ndbout_c("RedoPageCache: avoided %u (%u/%u) page-reads",
           cache.m_hits, cache.m_multi_page, cache.m_multi_miss);
#endif
  cache.m_hits = 0;
  cache.m_multi_page = 0;
  cache.m_multi_miss = 0;
}
#endif

#ifndef NO_REDO_OPEN_FILE_CACHE

#define MAX_CACHED_OPEN_FILES 4

void
Dblqh::openFileRw_cache(Signal* signal,
                        LogFileRecordPtr filePtr)
{
  jam();

  LogFileRecord::LogFileStatus state = filePtr.p->logFileStatus;
  if (state != LogFileRecord::CLOSED)
  {
    jam();

    m_redo_open_file_cache.m_hits++;

    if (m_redo_open_file_cache.m_lru.hasPrev(filePtr))
    {
      jam();
      m_redo_open_file_cache.m_lru.remove(filePtr);
      m_redo_open_file_cache.m_lru.addFirst(filePtr);
    }

    filePtr.p->logFileStatus = LogFileRecord::OPEN_EXEC_LOG_CACHED;

    signal->theData[0] = filePtr.i;
    signal->theData[1] = filePtr.p->fileRef;
    sendSignal(reference(), GSN_FSOPENCONF, signal, 2, JBB);
    return;
  }

  filePtr.p->logFileStatus = LogFileRecord::OPEN_EXEC_LOG;
  openFileRw(signal, filePtr, false);
}

void
Dblqh::closeFile_cache(Signal* signal,
                       LogFileRecordPtr filePtr,
                       Uint32 line)
{
  jam();

  filePtr.p->logFileStatus = LogFileRecord::CLOSING_EXEC_LOG_CACHED;
  if (m_redo_open_file_cache.m_lru.count() >= MAX_CACHED_OPEN_FILES)
  {
    jam();
    Ptr<LogFileRecord> evictPtr;
    Uint32 logPartRec = filePtr.p->logPartRec;
    /**
     * Only evict file with same log-part, other redo-execution will continue
     *   for the log-part once file is closed
     *
     * Note: 1) loop is guaranteed to terminate as filePtr must be in list
     *       2) loop is ok as MAX_CACHED_OPEN_FILES is "small"
     *          (if it was big, the m_lru should be split per log-part)
     */
    m_redo_open_file_cache.m_lru.last(evictPtr);
    while (evictPtr.p->logPartRec != logPartRec)
    {
      jam();
      ndbrequire(m_redo_open_file_cache.m_lru.prev(evictPtr));
    }
    m_redo_open_file_cache.m_lru.remove(evictPtr);
    evictPtr.p->logFileStatus = LogFileRecord::CLOSING_EXEC_LOG;
    closeFile(signal, evictPtr, line);
  }
  else
  {
    jam();
    signal->theData[0] = ZEXEC_SR;
    signal->theData[1] = filePtr.p->logPartRec;
    sendSignal(cownref, GSN_CONTINUEB, signal, 2, JBB);
  }
}

void
Dblqh::release(Signal* signal, RedoOpenFileCache & cache)
{
  Ptr<LogFileRecord> closePtr;

  while (m_redo_open_file_cache.m_lru.first(closePtr))
  {
    jam();
    m_redo_open_file_cache.m_lru.remove(closePtr);
    if (closePtr.p->logFileStatus == LogFileRecord::CLOSING_EXEC_LOG_CACHED)
    {
      jam();
      closePtr.p->logFileStatus = LogFileRecord::CLOSING_EXEC_LOG_CACHED;
      m_redo_open_file_cache.m_close_cnt ++;
      signal->theData[0] = closePtr.p->fileRef;
      signal->theData[1] = reference();
      signal->theData[2] = closePtr.i;
      signal->theData[3] = ZCLOSE_NO_DELETE;
      signal->theData[4] = __LINE__;
      sendSignal(NDBFS_REF, GSN_FSCLOSEREQ, signal, 5, JBA);
      return;
    }
    else
    {
      ndbout_c("Found file with state: %u",
               closePtr.p->logFileStatus);
    }
  }

  ndbout_c("RedoOpenFileCache: Avoided %u file-open/close closed: %u",
           m_redo_open_file_cache.m_hits,
           m_redo_open_file_cache.m_close_cnt);
  m_redo_open_file_cache.m_hits = 0;
  m_redo_open_file_cache.m_close_cnt = 0;
  execLogComp_extra_files_closed(signal);
}

#endif

bool
Dblqh::check_ndb_versions() const
{
  Uint32 version = getNodeInfo(getOwnNodeId()).m_version;
  for (Uint32 i = 0; i < cnoOfNodes; i++) 
  {
    Uint32 node = cnodeData[i];
    if (cnodeStatus[i] == ZNODE_UP)
    {
      if(getNodeInfo(node).m_version != version)
      {
        return false;
      }
    }
  }
  return true;
}

void
Dblqh::suspendFile(Signal* signal, Uint32 filePtrI, Uint32 millis)
{
  Ptr<LogFileRecord> tmp;
  tmp.i = filePtrI;
  ptrCheckGuard(tmp, clogFileFileSize, logFileRecord);
  suspendFile(signal, tmp, millis);
}

void
Dblqh::suspendFile(Signal* signal, Ptr<LogFileRecord> logFilePtr, Uint32 millis)
{
  SaveSignal<FsSuspendOrd::SignalLength> tmp(signal);
  signal->theData[0] = logFilePtr.p->fileRef;
  signal->theData[1] = millis;
  sendSignal(NDBFS_REF, GSN_FSSUSPENDORD, signal, 2, JBA);
}

void
Dblqh::send_runredo_event(Signal* signal, LogPartRecord * lp, Uint32 gci)
{
  signal->theData[0] = NDB_LE_RunRedo;
  signal->theData[1] = lp->logPartNo;
  signal->theData[2] = csrPhasesCompleted;
  signal->theData[3] = lp->logStartGci;
  signal->theData[4] = gci;
  signal->theData[5] = lp->logLastGci;


  LogFileRecordPtr filePtr;
  filePtr.i = lp->startLogfile;
  ptrCheckGuard(filePtr, clogFileFileSize, logFileRecord);
  signal->theData[6] = filePtr.p->fileNo;
  signal->theData[7] = lp->startMbyte;

  filePtr.i = lp->currentLogfile;
  ptrCheckGuard(filePtr, clogFileFileSize, logFileRecord);
  signal->theData[8] = filePtr.p->fileNo;
  signal->theData[9] = filePtr.p->currentMbyte;

  filePtr.i = lp->stopLogfile;
  ptrCheckGuard(filePtr, clogFileFileSize, logFileRecord);
  signal->theData[10] = filePtr.p->fileNo;
  signal->theData[11] = lp->stopMbyte;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 12, JBB);
}

void
Dblqh::IOTracker::init(Uint32 partNo)
{
  bzero(this, sizeof(* this));
  m_log_part_no = partNo;
}

int
Dblqh::IOTracker::tick(Uint32 now, Uint32 maxlag, Uint32 maxlag_cnt)
{
  Uint32 t = m_current_time;

  if ((t / SAMPLE_TIME) == (now / SAMPLE_TIME))
    return 0;

  m_current_time = now;
  if (m_sample_completed_bytes >= m_sample_sent_bytes)
  {
    /**
     * If we completed all io we sent during current sample...
     *   we can't have any problem...and
     *   we can't measure io throughput, so don't add measurement
     *
     */
    m_sample_sent_bytes = 0;
    m_sample_completed_bytes = 0;
  }
  else
  {
    // io maxed out...
    Uint32 elapsed = now - t;
    m_save_written_bytes[m_save_pos] += m_sample_completed_bytes;
    m_save_elapsed_millis[m_save_pos] += elapsed;

    m_curr_written_bytes += m_sample_completed_bytes;
    m_curr_elapsed_millis += elapsed;

    Uint32 bps = (1000 * m_sample_completed_bytes) / elapsed;
    Uint32 lag = bps ? m_sum_outstanding_bytes / bps : 30;
    if (false && lag >= 30)
    {
      g_eventLogger->info("part: %u tick(%u) m_sample_completed_bytes: %u m_sample_sent_bytes: %u elapsed: %u kbps: %u lag: %u",
                          m_log_part_no,
                          now, m_sample_completed_bytes, m_sample_sent_bytes,
                          elapsed, bps/1000, lag);
    }

    m_sample_sent_bytes -= m_sample_completed_bytes;
    m_sample_completed_bytes = 0;
  }

  int retVal = 0;
  Uint32 save_lag_cnt = m_lag_cnt;
  if ((now / SLIDING_WINDOW_LEN) != (t / SLIDING_WINDOW_LEN))
  {
    Uint32 lag = m_curr_written_bytes ?
      ((Uint64(m_sum_outstanding_bytes) / 1000) * Uint64(m_curr_elapsed_millis)) / m_curr_written_bytes:
      0;

    if (lag > maxlag)
    {
      /**
       * We did have lag last second...
       *   increase m_lag_cnt and check if it has reached maxlag_cnt
       */
      Uint32 tmp = m_lag_cnt;
      m_lag_cnt += (lag / maxlag);
      if (tmp < maxlag_cnt && m_lag_cnt >= maxlag_cnt)
      {
        retVal = -1; // start aborting transaction
      }
    }
    else
    {
      /**
       * We did not have lag...reset m_lag_cnt
       */
      if (m_lag_cnt >= maxlag_cnt)
      {
        // stop aborting transcation
        retVal = 1;
      }
      m_lag_cnt = 0;
    }

#if 1
    if (m_lag_cnt == 0 && lag == 0)
    {
    }
    else if (lag > 0 && m_lag_cnt == 0)
    {
      g_eventLogger->info("part: %u : time to complete: %u",
                          m_log_part_no, lag);
    }
    else if (m_lag_cnt < maxlag_cnt && m_lag_cnt == save_lag_cnt)
    {
      g_eventLogger->info("part: %u : time to complete: %u lag_cnt: %u => %u => retVal: %d",
                          m_log_part_no,
                          lag,
                          save_lag_cnt,
                          m_lag_cnt,
                          retVal);
    }
    else
    {
      g_eventLogger->info("part: %u : sum_outstanding: %ukb avg_written: %ukb avg_elapsed: %ums time to complete: %u lag_cnt: %u => %u retVal: %d",
                          m_log_part_no, m_sum_outstanding_bytes / 1024, m_curr_written_bytes/1024, m_curr_elapsed_millis,
             lag, save_lag_cnt, m_lag_cnt, retVal);
    }
#endif

    /**
     * And finally rotate sliding window
     */
    Uint32 last = (m_save_pos + 1) % SLIDING_WINDOW_HISTORY_LEN;
    assert(m_curr_written_bytes >= m_save_written_bytes[last]);
    assert(m_curr_elapsed_millis >= m_save_elapsed_millis[last]);
    m_curr_written_bytes -= m_save_written_bytes[last];
    m_curr_elapsed_millis -= m_save_elapsed_millis[last];
    m_save_written_bytes[last] = 0;
    m_save_elapsed_millis[last] = 0;
    m_save_pos = last;
  }
  return retVal;
}

void
Dblqh::IOTracker::send_io(Uint32 bytes)
{
  m_sum_outstanding_bytes += bytes;
  m_sample_sent_bytes += bytes;
}

void
Dblqh::IOTracker::complete_io(Uint32 bytes)
{
  assert(m_sum_outstanding_bytes >= bytes);

  m_sum_outstanding_bytes -= bytes;
  m_sample_completed_bytes += bytes;
}
