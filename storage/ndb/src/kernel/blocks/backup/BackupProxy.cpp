/* Copyright (c) 2008, 2018, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "BackupProxy.hpp"
#include "Backup.hpp"
#include <signaldata/DumpStateOrd.hpp>

#define JAM_FILE_ID 471


BackupProxy::BackupProxy(Block_context& ctx) :
  LocalProxy(BACKUP, ctx)
{
  // GSN_STTOR
  addRecSignal(GSN_UTIL_SEQUENCE_CONF, &BackupProxy::execUTIL_SEQUENCE_CONF);
  addRecSignal(GSN_UTIL_SEQUENCE_REF, &BackupProxy::execUTIL_SEQUENCE_REF);

  addRecSignal(GSN_DUMP_STATE_ORD, &BackupProxy::execDUMP_STATE_ORD, true);
  addRecSignal(GSN_EVENT_REP, &BackupProxy::execEVENT_REP);

  addRecSignal(GSN_RESTORABLE_GCI_REP, &BackupProxy::execRESTORABLE_GCI_REP);
}

BackupProxy::~BackupProxy()
{
}

SimulatedBlock*
BackupProxy::newWorker(Uint32 instanceNo)
{
  return new Backup(m_ctx, instanceNo);
}

// GSN_STTOR

void
BackupProxy::callSTTOR(Signal* signal)
{
  Ss_READ_NODES_REQ& ss = c_ss_READ_NODESREQ;
  ndbrequire(ss.m_gsn == 0);

  const Uint32 startPhase = signal->theData[1];
  switch (startPhase) {
  case 3:
    ss.m_gsn = GSN_STTOR;
    sendREAD_NODESREQ(signal);
    break;
  case 7:
    if (c_typeOfStart == NodeState::ST_INITIAL_START &&
        c_masterNodeId == getOwnNodeId()) {
      jam();
      sendUTIL_SEQUENCE_REQ(signal);
      return;
    }
    backSTTOR(signal);
    break;
  default:
    backSTTOR(signal);
    break;
  }
}

static const Uint32 BACKUP_SEQUENCE = 0x1F000000;

void
BackupProxy::sendUTIL_SEQUENCE_REQ(Signal* signal)
{
  UtilSequenceReq* req = (UtilSequenceReq*)signal->getDataPtrSend();

  req->senderData  = RNIL;
  req->sequenceId  = BACKUP_SEQUENCE;
  req->requestType = UtilSequenceReq::Create;
  
  sendSignal(DBUTIL_REF, GSN_UTIL_SEQUENCE_REQ, 
	     signal, UtilSequenceReq::SignalLength, JBB);
}

void
BackupProxy::execUTIL_SEQUENCE_CONF(Signal* signal)
{
  backSTTOR(signal);
}

void
BackupProxy::execUTIL_SEQUENCE_REF(Signal* signal)
{
  ndbabort();
}

/* 
 * DUMP_STATE_ORD (BackupStatus)
 *
 * This is used by the MGM Client REPORT BACKUP command.
 * It sends DUMP_STATE_ORD with a client block reference
 * BACKUP sends an EVENT_REP to the client block
 * To hide the multiple instances of BACKUP from the client
 * here we become the internal client of the BACKUP workers,
 * ask them for backup status, and sum(marise) across them.
 */
void BackupProxy::execDUMP_STATE_ORD(Signal* signal)
{
  /* Special handling of case used by ALL REPORT BACKUP
   * from MGMD, to ensure 1 result row per node
   */
  if (signal->length() == 2 && 
      signal->theData[0] == DumpStateOrd::BackupStatus)
  {
    /* Special case as part of ALL REPORT BACKUP,
     * which requires 1 report per node.
     */
    if (unlikely(c_ss_SUM_DUMP_STATE_ORD.m_usage != 0))
    {
      /* Got two concurrent DUMP_STATE_ORDs for BackupStatus,
       * let's busy-wait
       */
      sendSignalWithDelay(reference(), GSN_DUMP_STATE_ORD,
                          signal, 10, 2);
      return;
    }

    Ss_SUM_DUMP_STATE_ORD& ss = ssSeize<Ss_SUM_DUMP_STATE_ORD>(1);

    /* Grab request, and zero report */
    memcpy(&ss.m_request, signal->theData, 2 << 2);
    memset(ss.m_report, 0, Ss_SUM_DUMP_STATE_ORD::MAX_REP_SIZE << 2);

    sendREQ(signal, ss);
  }
  else
  {
    /* Use generic method */
    LocalProxy::execDUMP_STATE_ORD(signal);
  }
}

void 
BackupProxy::sendSUM_DUMP_STATE_ORD(Signal* signal, Uint32 ssId,
                                    SectionHandle* handle)
{
  Ss_SUM_DUMP_STATE_ORD& ss = ssFind<Ss_SUM_DUMP_STATE_ORD>(ssId);

  memcpy(signal->theData, ss.m_request, 2 << 2);
  /* We are the client now */
  signal->theData[1] = reference();
  
  sendSignal(workerRef(ss.m_worker), GSN_DUMP_STATE_ORD,
             signal, 2, JBB);
}

void 
BackupProxy::execEVENT_REP(Signal* signal)
{
  Ss_SUM_DUMP_STATE_ORD& ss = ssFind<Ss_SUM_DUMP_STATE_ORD>(1);
  
  recvCONF(signal, ss);
}

void
BackupProxy::sendSUM_EVENT_REP(Signal* signal, Uint32 ssId)
{
  Ss_SUM_DUMP_STATE_ORD& ss = ssFind<Ss_SUM_DUMP_STATE_ORD>(ssId);
  const Uint32 reportLen = 11;

  ndbrequire(signal->theData[0] == NDB_LE_BackupStatus);
  ss.m_report[0] = signal->theData[0];

  /* 1 = starting node */
  Uint32 startingNode = signal->theData[1];
  if (startingNode != 0)
  {
    ndbrequire(ss.m_report[1] == 0 || 
               ss.m_report[1] == startingNode);
    ss.m_report[1] = startingNode;
  };
  
  /* 2 = backup id */
  Uint32 backupId = signal->theData[2];
  if (backupId != 0)
  {
    ndbrequire(ss.m_report[2] == 0 || 
               ss.m_report[2] == backupId);
    ss.m_report[2] = backupId;
  };
  
  /* Words 3 -> 10 , various sums */
  for (Uint32 w = 3; w < reportLen; w++)
    ss.m_report[w] += signal->theData[w];
  
  if (!lastReply(ss))
    return;

  BlockReference clientRef = ss.m_request[1];
  memcpy(signal->theData, ss.m_report, reportLen << 2);
  sendSignal(clientRef, GSN_EVENT_REP,
             signal, reportLen, JBB);
  
  ssRelease<Ss_SUM_DUMP_STATE_ORD>(ssId);
}

void
BackupProxy::execRESTORABLE_GCI_REP(Signal *signal)
{
  for (Uint32 i = 0; i < c_workers; i++)
  {
    jam();
    sendSignal(workerRef(i), GSN_RESTORABLE_GCI_REP, signal,
               signal->getLength(), JBB);
  }
}
BLOCK_FUNCTIONS(BackupProxy)
