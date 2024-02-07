/*
  Copyright (c) 2000, 2024, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "DbgdmProxy.hpp"

#include <signaldata/AlterTab.hpp>
#include <signaldata/CreateTab.hpp>
#include <signaldata/DropTab.hpp>
#include <signaldata/PrepDropTab.hpp>
#include <signaldata/TabCommit.hpp>

#define JAM_FILE_ID 338

DbgdmProxy::DbgdmProxy(BlockNumber blockNumber, Block_context &ctx)
    : LocalProxy(blockNumber, ctx) {
  // GSN_TC_SCHVERREQ
  addRecSignal(GSN_TC_SCHVERREQ, &DbgdmProxy::execTC_SCHVERREQ);
  addRecSignal(GSN_TC_SCHVERCONF, &DbgdmProxy::execTC_SCHVERCONF);

  // GSN_TAB_COMMITREQ
  addRecSignal(GSN_TAB_COMMITREQ, &DbgdmProxy::execTAB_COMMITREQ);
  addRecSignal(GSN_TAB_COMMITCONF, &DbgdmProxy::execTAB_COMMITCONF);
  addRecSignal(GSN_TAB_COMMITREF, &DbgdmProxy::execTAB_COMMITREF);

  // GSN_PREP_DROP_TAB_REQ
  addRecSignal(GSN_PREP_DROP_TAB_REQ, &DbgdmProxy::execPREP_DROP_TAB_REQ);
  addRecSignal(GSN_PREP_DROP_TAB_CONF, &DbgdmProxy::execPREP_DROP_TAB_CONF);
  addRecSignal(GSN_PREP_DROP_TAB_REF, &DbgdmProxy::execPREP_DROP_TAB_REF);

  // GSN_DROP_TAB_REQ
  addRecSignal(GSN_DROP_TAB_REQ, &DbgdmProxy::execDROP_TAB_REQ);
  addRecSignal(GSN_DROP_TAB_CONF, &DbgdmProxy::execDROP_TAB_CONF);
  addRecSignal(GSN_DROP_TAB_REF, &DbgdmProxy::execDROP_TAB_REF);

  // GSN_ALTER_TAB_REQ
  addRecSignal(GSN_ALTER_TAB_REQ, &DbgdmProxy::execALTER_TAB_REQ);
  addRecSignal(GSN_ALTER_TAB_CONF, &DbgdmProxy::execALTER_TAB_CONF);
  addRecSignal(GSN_ALTER_TAB_REF, &DbgdmProxy::execALTER_TAB_REF);
}

DbgdmProxy::~DbgdmProxy() {}

// GSN_TC_SCHVERREQ

void DbgdmProxy::execTC_SCHVERREQ(Signal *signal) {
  jam();
  Ss_TC_SCHVERREQ &ss = ssSeize<Ss_TC_SCHVERREQ>(1);

  const TcSchVerReq *req = (const TcSchVerReq *)signal->getDataPtr();
  ss.m_req = *req;

  sendREQ(signal, ss);
}

void DbgdmProxy::sendTC_SCHVERREQ(Signal *signal, Uint32 ssId,
                                  SectionHandle *) {
  jam();
  Ss_TC_SCHVERREQ &ss = ssFind<Ss_TC_SCHVERREQ>(ssId);

  TcSchVerReq *req = (TcSchVerReq *)signal->getDataPtrSend();
  *req = ss.m_req;
  req->senderRef = reference();
  req->senderData = ssId;
  sendSignal(workerRef(ss.m_worker), GSN_TC_SCHVERREQ, signal,
             TcSchVerReq::SignalLength, JBB);
}

void DbgdmProxy::execTC_SCHVERCONF(Signal *signal) {
  jam();
  const TcSchVerConf *conf = (const TcSchVerConf *)signal->getDataPtr();
  Uint32 ssId = conf->senderData;
  Ss_TC_SCHVERREQ &ss = ssFind<Ss_TC_SCHVERREQ>(ssId);
  recvCONF(signal, ss);
}

void DbgdmProxy::sendTC_SCHVERCONF(Signal *signal, Uint32 ssId) {
  jam();
  Ss_TC_SCHVERREQ &ss = ssFind<Ss_TC_SCHVERREQ>(ssId);
  BlockReference dictRef = ss.m_req.senderRef;

  if (!lastReply(ss)) {
    jam();
    return;
  }

  TcSchVerConf *conf = (TcSchVerConf *)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = ss.m_req.senderData;
  sendSignal(dictRef, GSN_TC_SCHVERCONF, signal, TcSchVerConf::SignalLength,
             JBB);

  ssRelease<Ss_TC_SCHVERREQ>(ssId);
}

// GSN_TAB_COMMITREQ [ sub-op ]

void DbgdmProxy::execTAB_COMMITREQ(Signal *signal) {
  jam();
  Ss_TAB_COMMITREQ &ss = ssSeize<Ss_TAB_COMMITREQ>(1);

  const TabCommitReq *req = (const TabCommitReq *)signal->getDataPtr();
  ss.m_req = *req;
  sendREQ(signal, ss);
}

void DbgdmProxy::sendTAB_COMMITREQ(Signal *signal, Uint32 ssId,
                                   SectionHandle *) {
  jam();
  Ss_TAB_COMMITREQ &ss = ssFind<Ss_TAB_COMMITREQ>(ssId);

  TabCommitReq *req = (TabCommitReq *)signal->getDataPtrSend();
  req->senderRef = reference();
  req->senderData = ssId;
  req->tableId = ss.m_req.tableId;
  sendSignal(workerRef(ss.m_worker), GSN_TAB_COMMITREQ, signal,
             TabCommitReq::SignalLength, JBB);
}

void DbgdmProxy::execTAB_COMMITCONF(Signal *signal) {
  jam();
  const TabCommitConf *conf = (TabCommitConf *)signal->getDataPtr();
  Uint32 ssId = conf->senderData;
  Ss_TAB_COMMITREQ &ss = ssFind<Ss_TAB_COMMITREQ>(ssId);
  recvCONF(signal, ss);
}

void DbgdmProxy::execTAB_COMMITREF(Signal *signal) {
  jam();
  const TabCommitRef *ref = (TabCommitRef *)signal->getDataPtr();
  Uint32 ssId = ref->senderData;
  Ss_TAB_COMMITREQ &ss = ssFind<Ss_TAB_COMMITREQ>(ssId);

  recvREF(signal, ss, ref->errorCode);
}

void DbgdmProxy::sendTAB_COMMITCONF(Signal *signal, Uint32 ssId) {
  jam();
  Ss_TAB_COMMITREQ &ss = ssFind<Ss_TAB_COMMITREQ>(ssId);
  BlockReference dictRef = ss.m_req.senderRef;

  if (!lastReply(ss)) {
    jam();
    return;
  }

  if (ss.m_error == 0) {
    jam();
    TabCommitConf *conf = (TabCommitConf *)signal->getDataPtrSend();
    conf->senderData = ss.m_req.senderData;
    conf->nodeId = getOwnNodeId();
    conf->tableId = ss.m_req.tableId;
    sendSignal(dictRef, GSN_TAB_COMMITCONF, signal, TabCommitConf::SignalLength,
               JBB);
  } else {
    jam();
    TabCommitRef *ref = (TabCommitRef *)signal->getDataPtrSend();
    ref->senderData = ss.m_req.senderData;
    ref->nodeId = getOwnNodeId();
    ref->tableId = ss.m_req.tableId;
    sendSignal(dictRef, GSN_TAB_COMMITREF, signal, TabCommitRef::SignalLength,
               JBB);
    return;
  }

  ssRelease<Ss_TAB_COMMITREQ>(ssId);
}

// GSN_PREP_DROP_TAB_REQ

void DbgdmProxy::execPREP_DROP_TAB_REQ(Signal *signal) {
  jam();
  const PrepDropTabReq *req = (const PrepDropTabReq *)signal->getDataPtr();
  Uint32 ssId = getSsId(req);
  Ss_PREP_DROP_TAB_REQ &ss = ssSeize<Ss_PREP_DROP_TAB_REQ>(ssId);
  ss.m_req = *req;
  ndbrequire(signal->getLength() == PrepDropTabReq::SignalLength);
  sendREQ(signal, ss);
}

void DbgdmProxy::sendPREP_DROP_TAB_REQ(Signal *signal, Uint32 ssId,
                                       SectionHandle *) {
  jam();
  Ss_PREP_DROP_TAB_REQ &ss = ssFind<Ss_PREP_DROP_TAB_REQ>(ssId);

  PrepDropTabReq *req = (PrepDropTabReq *)signal->getDataPtrSend();
  *req = ss.m_req;
  req->senderRef = reference();
  req->senderData = ssId;  // redundant since tableId is used
  sendSignal(workerRef(ss.m_worker), GSN_PREP_DROP_TAB_REQ, signal,
             PrepDropTabReq::SignalLength, JBB);
}

void DbgdmProxy::execPREP_DROP_TAB_CONF(Signal *signal) {
  jam();
  const PrepDropTabConf *conf = (const PrepDropTabConf *)signal->getDataPtr();
  Uint32 ssId = getSsId(conf);
  Ss_PREP_DROP_TAB_REQ &ss = ssFind<Ss_PREP_DROP_TAB_REQ>(ssId);
  recvCONF(signal, ss);
}

void DbgdmProxy::execPREP_DROP_TAB_REF(Signal *signal) {
  jam();
  const PrepDropTabRef *ref = (const PrepDropTabRef *)signal->getDataPtr();
  Uint32 ssId = getSsId(ref);
  Ss_PREP_DROP_TAB_REQ &ss = ssFind<Ss_PREP_DROP_TAB_REQ>(ssId);
  recvREF(signal, ss, ref->errorCode);
}

void DbgdmProxy::sendPREP_DROP_TAB_CONF(Signal *signal, Uint32 ssId) {
  jam();
  Ss_PREP_DROP_TAB_REQ &ss = ssFind<Ss_PREP_DROP_TAB_REQ>(ssId);
  BlockReference dictRef = ss.m_req.senderRef;

  if (!lastReply(ss)) {
    jam();
    return;
  }

  if (ss.m_error == 0) {
    jam();
    PrepDropTabConf *conf = (PrepDropTabConf *)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = ss.m_req.senderData;
    conf->tableId = ss.m_req.tableId;
    sendSignal(dictRef, GSN_PREP_DROP_TAB_CONF, signal,
               PrepDropTabConf::SignalLength, JBB);
  } else {
    jam();
    PrepDropTabRef *ref = (PrepDropTabRef *)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = ss.m_req.senderData;
    ref->tableId = ss.m_req.tableId;
    ref->errorCode = ss.m_error;
    sendSignal(dictRef, GSN_PREP_DROP_TAB_REF, signal,
               PrepDropTabRef::SignalLength, JBB);
  }

  ssRelease<Ss_PREP_DROP_TAB_REQ>(ssId);
}

// GSN_DROP_TAB_REQ

void DbgdmProxy::execDROP_TAB_REQ(Signal *signal) {
  jam();
  const DropTabReq *req = (const DropTabReq *)signal->getDataPtr();
  Uint32 ssId = getSsId(req);
  Ss_DROP_TAB_REQ &ss = ssSeize<Ss_DROP_TAB_REQ>(ssId);
  ss.m_req = *req;
  ndbrequire(signal->getLength() == DropTabReq::SignalLength);
  sendREQ(signal, ss);
}

void DbgdmProxy::sendDROP_TAB_REQ(Signal *signal, Uint32 ssId,
                                  SectionHandle *) {
  jam();
  Ss_DROP_TAB_REQ &ss = ssFind<Ss_DROP_TAB_REQ>(ssId);

  DropTabReq *req = (DropTabReq *)signal->getDataPtrSend();
  *req = ss.m_req;
  req->senderRef = reference();
  req->senderData = ssId;  // redundant since tableId is used
  sendSignal(workerRef(ss.m_worker), GSN_DROP_TAB_REQ, signal,
             DropTabReq::SignalLength, JBB);
}

void DbgdmProxy::execDROP_TAB_CONF(Signal *signal) {
  jam();
  const DropTabConf *conf = (const DropTabConf *)signal->getDataPtr();
  Uint32 ssId = getSsId(conf);
  Ss_DROP_TAB_REQ &ss = ssFind<Ss_DROP_TAB_REQ>(ssId);
  recvCONF(signal, ss);
}

void DbgdmProxy::execDROP_TAB_REF(Signal *signal) {
  jam();
  const DropTabRef *ref = (const DropTabRef *)signal->getDataPtr();
  Uint32 ssId = getSsId(ref);
  Ss_DROP_TAB_REQ &ss = ssFind<Ss_DROP_TAB_REQ>(ssId);
  recvREF(signal, ss, ref->errorCode);
}

void DbgdmProxy::sendDROP_TAB_CONF(Signal *signal, Uint32 ssId) {
  jam();
  Ss_DROP_TAB_REQ &ss = ssFind<Ss_DROP_TAB_REQ>(ssId);
  BlockReference dictRef = ss.m_req.senderRef;

  if (!lastReply(ss)) {
    jam();
    return;
  }

  if (ss.m_error == 0) {
    jam();
    DropTabConf *conf = (DropTabConf *)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = ss.m_req.senderData;
    conf->tableId = ss.m_req.tableId;
    sendSignal(dictRef, GSN_DROP_TAB_CONF, signal, DropTabConf::SignalLength,
               JBB);
  } else {
    jam();
    DropTabRef *ref = (DropTabRef *)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = ss.m_req.senderData;
    ref->tableId = ss.m_req.tableId;
    ref->errorCode = ss.m_error;
    sendSignal(dictRef, GSN_DROP_TAB_REF, signal, DropTabConf::SignalLength,
               JBB);
  }

  ssRelease<Ss_DROP_TAB_REQ>(ssId);
}

// GSN_ALTER_TAB_REQ

void DbgdmProxy::execALTER_TAB_REQ(Signal *signal) {
  if (!assembleFragments(signal)) {
    jam();
    return;
  }

  jam();
  const AlterTabReq *req = (const AlterTabReq *)signal->getDataPtr();
  Uint32 ssId = getSsId(req);
  Ss_ALTER_TAB_REQ &ss = ssSeize<Ss_ALTER_TAB_REQ>(ssId);
  ss.m_req = *req;

  SectionHandle handle(this, signal);
  saveSections(ss, handle);

  sendREQ(signal, ss);
}

void DbgdmProxy::sendALTER_TAB_REQ(Signal *signal, Uint32 ssId,
                                   SectionHandle *handle) {
  jam();
  Ss_ALTER_TAB_REQ &ss = ssFind<Ss_ALTER_TAB_REQ>(ssId);

  AlterTabReq *req = (AlterTabReq *)signal->getDataPtrSend();
  *req = ss.m_req;
  req->senderRef = reference();
  req->senderData = ssId;
  sendSignalNoRelease(workerRef(ss.m_worker), GSN_ALTER_TAB_REQ, signal,
                      AlterTabReq::SignalLength, JBB, handle);
}

void DbgdmProxy::execALTER_TAB_CONF(Signal *signal) {
  jam();
  const AlterTabConf *conf = (const AlterTabConf *)signal->getDataPtr();
  Uint32 ssId = getSsId(conf);
  Ss_ALTER_TAB_REQ &ss = ssFind<Ss_ALTER_TAB_REQ>(ssId);
  recvCONF(signal, ss);
}

void DbgdmProxy::execALTER_TAB_REF(Signal *signal) {
  jam();
  const AlterTabRef *ref = (const AlterTabRef *)signal->getDataPtr();
  Uint32 ssId = getSsId(ref);
  Ss_ALTER_TAB_REQ &ss = ssFind<Ss_ALTER_TAB_REQ>(ssId);
  recvREF(signal, ss, ref->errorCode);
}

void DbgdmProxy::sendALTER_TAB_CONF(Signal *signal, Uint32 ssId) {
  jam();
  Ss_ALTER_TAB_REQ &ss = ssFind<Ss_ALTER_TAB_REQ>(ssId);
  BlockReference dictRef = ss.m_req.senderRef;

  if (!lastReply(ss)) {
    jam();
    return;
  }

  if (ss.m_error == 0) {
    jam();
    AlterTabConf *conf = (AlterTabConf *)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = ss.m_req.senderData;
    sendSignal(dictRef, GSN_ALTER_TAB_CONF, signal, AlterTabConf::SignalLength,
               JBB);
  } else {
    jam();
    AlterTabRef *ref = (AlterTabRef *)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = ss.m_req.senderData;
    ref->errorCode = ss.m_error;
    sendSignal(dictRef, GSN_ALTER_TAB_REF, signal, AlterTabConf::SignalLength,
               JBB);
  }

  ssRelease<Ss_ALTER_TAB_REQ>(ssId);
}

BLOCK_FUNCTIONS(DbgdmProxy)
