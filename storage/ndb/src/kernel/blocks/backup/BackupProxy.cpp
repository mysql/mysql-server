/* Copyright (C) 2008 MySQL AB
   Use is subject to license terms

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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#include "BackupProxy.hpp"
#include "Backup.hpp"

BackupProxy::BackupProxy(Block_context& ctx) :
  LocalProxy(BACKUP, ctx)
{
  // GSN_STTOR
  addRecSignal(GSN_UTIL_SEQUENCE_CONF, &BackupProxy::execUTIL_SEQUENCE_CONF);
  addRecSignal(GSN_UTIL_SEQUENCE_REF, &BackupProxy::execUTIL_SEQUENCE_REF);
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
  ndbrequire(false);
}

BLOCK_FUNCTIONS(BackupProxy)
