/* Copyright (c) 2008, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "DbtuxProxy.hpp"
#include "../dblqh/DblqhCommon.hpp"
#include "Dbtux.hpp"

#define JAM_FILE_ID 370

DbtuxProxy::DbtuxProxy(Block_context &ctx) : LocalProxy(DBTUX, ctx) {
  // GSN_ALTER_INDX_IMPL_REQ
  addRecSignal(GSN_ALTER_INDX_IMPL_REQ, &DbtuxProxy::execALTER_INDX_IMPL_REQ);
  addRecSignal(GSN_ALTER_INDX_IMPL_CONF, &DbtuxProxy::execALTER_INDX_IMPL_CONF);
  addRecSignal(GSN_ALTER_INDX_IMPL_REF, &DbtuxProxy::execALTER_INDX_IMPL_REF);

  // GSN_INDEX_STAT_IMPL_REQ
  addRecSignal(GSN_INDEX_STAT_IMPL_REQ, &DbtuxProxy::execINDEX_STAT_IMPL_REQ);
  addRecSignal(GSN_INDEX_STAT_IMPL_CONF, &DbtuxProxy::execINDEX_STAT_IMPL_CONF);
  addRecSignal(GSN_INDEX_STAT_IMPL_REF, &DbtuxProxy::execINDEX_STAT_IMPL_REF);

  // GSN_INDEX_STAT_REP
  addRecSignal(GSN_INDEX_STAT_REP, &DbtuxProxy::execINDEX_STAT_REP);
}

DbtuxProxy::~DbtuxProxy() {}

SimulatedBlock *DbtuxProxy::newWorker(Uint32 instanceNo) {
  return new Dbtux(m_ctx, instanceNo);
}

// GSN_ALTER_INDX_IMPL_REQ

void DbtuxProxy::execALTER_INDX_IMPL_REQ(Signal *signal) {
  jam();
  const AlterIndxImplReq *req = (const AlterIndxImplReq *)signal->getDataPtr();
  Ss_ALTER_INDX_IMPL_REQ &ss = ssSeize<Ss_ALTER_INDX_IMPL_REQ>();
  ss.m_req = *req;
  ndbrequire(signal->getLength() == AlterIndxImplReq::SignalLength);
  sendREQ(signal, ss);
}

void DbtuxProxy::sendALTER_INDX_IMPL_REQ(Signal *signal, Uint32 ssId,
                                         SectionHandle *handle) {
  jam();
  Ss_ALTER_INDX_IMPL_REQ &ss = ssFind<Ss_ALTER_INDX_IMPL_REQ>(ssId);

  AlterIndxImplReq *req = (AlterIndxImplReq *)signal->getDataPtrSend();
  *req = ss.m_req;
  req->senderRef = reference();
  req->senderData = ssId;
  sendSignalNoRelease(workerRef(ss.m_worker), GSN_ALTER_INDX_IMPL_REQ, signal,
                      AlterIndxImplReq::SignalLength, JBB, handle);
}

void DbtuxProxy::execALTER_INDX_IMPL_CONF(Signal *signal) {
  jam();
  const AlterIndxImplConf *conf =
      (const AlterIndxImplConf *)signal->getDataPtr();
  Uint32 ssId = conf->senderData;
  Ss_ALTER_INDX_IMPL_REQ &ss = ssFind<Ss_ALTER_INDX_IMPL_REQ>(ssId);
  recvCONF(signal, ss);
}

void DbtuxProxy::execALTER_INDX_IMPL_REF(Signal *signal) {
  jam();
  const AlterIndxImplRef *ref = (const AlterIndxImplRef *)signal->getDataPtr();
  Uint32 ssId = ref->senderData;
  Ss_ALTER_INDX_IMPL_REQ &ss = ssFind<Ss_ALTER_INDX_IMPL_REQ>(ssId);
  recvREF(signal, ss, ref->errorCode);
}

void DbtuxProxy::sendALTER_INDX_IMPL_CONF(Signal *signal, Uint32 ssId) {
  jam();
  Ss_ALTER_INDX_IMPL_REQ &ss = ssFind<Ss_ALTER_INDX_IMPL_REQ>(ssId);
  BlockReference dictRef = ss.m_req.senderRef;

  if (!lastReply(ss)) {
    jam();
    return;
  }

  if (ss.m_error == 0) {
    jam();
    AlterIndxImplConf *conf = (AlterIndxImplConf *)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = ss.m_req.senderData;
    sendSignal(dictRef, GSN_ALTER_INDX_IMPL_CONF, signal,
               AlterIndxImplConf::SignalLength, JBB);
  } else {
    jam();
    AlterIndxImplRef *ref = (AlterIndxImplRef *)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = ss.m_req.senderData;
    ref->errorCode = ss.m_error;
    sendSignal(dictRef, GSN_ALTER_INDX_IMPL_REF, signal,
               AlterIndxImplRef::SignalLength, JBB);
  }

  ssRelease<Ss_ALTER_INDX_IMPL_REQ>(ssId);
}

// GSN_INDEX_STAT_IMPL_REQ

void DbtuxProxy::execINDEX_STAT_IMPL_REQ(Signal *signal) {
  jamEntry();
  const IndexStatImplReq *req = (const IndexStatImplReq *)signal->getDataPtr();
  Ss_INDEX_STAT_IMPL_REQ &ss = ssSeize<Ss_INDEX_STAT_IMPL_REQ>();
  ss.m_req = *req;
  ndbrequire(signal->getLength() == IndexStatImplReq::SignalLength);
  sendREQ(signal, ss);
}

void DbtuxProxy::sendINDEX_STAT_IMPL_REQ(Signal *signal, Uint32 ssId,
                                         SectionHandle *) {
  jam();
  Ss_INDEX_STAT_IMPL_REQ &ss = ssFind<Ss_INDEX_STAT_IMPL_REQ>(ssId);

  IndexStatImplReq *req = (IndexStatImplReq *)signal->getDataPtrSend();
  *req = ss.m_req;
  req->senderRef = reference();
  req->senderData = ssId;

  switch (req->requestType) {
    case IndexStatReq::RT_START_MON:
      /*
       * DICT sets fragId if assigned frag is on this node, or else ZNIL
       * to turn off any possible old assignment.  In MT-LQH we also have
       * to check which worker owns the frag.
       */
      jam();
      break;
    case IndexStatReq::RT_STOP_MON:
      /*
       * DICT sets fragId to ZNIL always.  There is no (pointless) check
       * to see if the frag was ever assigned.
       */
      jam();
      ndbrequire(req->fragId == ZNIL);
      break;
    default:
      ndbabort();
  }

  sendSignal(workerRef(ss.m_worker), GSN_INDEX_STAT_IMPL_REQ, signal,
             IndexStatImplReq::SignalLength, JBB);
}

void DbtuxProxy::execINDEX_STAT_IMPL_CONF(Signal *signal) {
  jamEntry();
  const IndexStatImplConf *conf =
      (const IndexStatImplConf *)signal->getDataPtr();
  Uint32 ssId = conf->senderData;
  Ss_INDEX_STAT_IMPL_REQ &ss = ssFind<Ss_INDEX_STAT_IMPL_REQ>(ssId);
  recvCONF(signal, ss);
}

void DbtuxProxy::execINDEX_STAT_IMPL_REF(Signal *signal) {
  jamEntry();
  const IndexStatImplRef *ref = (const IndexStatImplRef *)signal->getDataPtr();
  Uint32 ssId = ref->senderData;
  Ss_INDEX_STAT_IMPL_REQ &ss = ssFind<Ss_INDEX_STAT_IMPL_REQ>(ssId);
  recvREF(signal, ss, ref->errorCode);
}

void DbtuxProxy::sendINDEX_STAT_IMPL_CONF(Signal *signal, Uint32 ssId) {
  jam();
  Ss_INDEX_STAT_IMPL_REQ &ss = ssFind<Ss_INDEX_STAT_IMPL_REQ>(ssId);
  BlockReference dictRef = ss.m_req.senderRef;

  if (!lastReply(ss)) {
    jam();
    return;
  }

  if (ss.m_error == 0) {
    jam();
    IndexStatImplConf *conf = (IndexStatImplConf *)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = ss.m_req.senderData;
    sendSignal(dictRef, GSN_INDEX_STAT_IMPL_CONF, signal,
               IndexStatImplConf::SignalLength, JBB);
  } else {
    jam();
    IndexStatImplRef *ref = (IndexStatImplRef *)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = ss.m_req.senderData;
    ref->errorCode = ss.m_error;
    sendSignal(dictRef, GSN_INDEX_STAT_IMPL_REF, signal,
               IndexStatImplRef::SignalLength, JBB);
  }

  ssRelease<Ss_INDEX_STAT_IMPL_REQ>(ssId);
}

// GSN_INDEX_STAT_REP

void DbtuxProxy::execINDEX_STAT_REP(Signal *signal) {
  jamEntry();
  const IndexStatRep *rep = (const IndexStatRep *)signal->getDataPtr();

  Uint32 instanceKey = getInstanceKey(rep->indexId, rep->fragId);
  Uint32 instanceNo = getInstanceNo(getOwnNodeId(), instanceKey);

  sendSignal(numberToRef(DBTUX, instanceNo, getOwnNodeId()), GSN_INDEX_STAT_REP,
             signal, signal->getLength(), JBB);
}

BLOCK_FUNCTIONS(DbtuxProxy)
