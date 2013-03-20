/* Copyright (c) 2008, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#include "DbtuxProxy.hpp"
#include "Dbtux.hpp"
#include "../dblqh/DblqhCommon.hpp"

DbtuxProxy::DbtuxProxy(Block_context& ctx) :
  LocalProxy(DBTUX, ctx)
{
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

DbtuxProxy::~DbtuxProxy()
{
}

SimulatedBlock*
DbtuxProxy::newWorker(Uint32 instanceNo)
{
  return new Dbtux(m_ctx, instanceNo);
}

// GSN_ALTER_INDX_IMPL_REQ

void
DbtuxProxy::execALTER_INDX_IMPL_REQ(Signal* signal)
{
  const AlterIndxImplReq* req = (const AlterIndxImplReq*)signal->getDataPtr();
  Ss_ALTER_INDX_IMPL_REQ& ss = ssSeize<Ss_ALTER_INDX_IMPL_REQ>();
  ss.m_req = *req;
  ndbrequire(signal->getLength() == AlterIndxImplReq::SignalLength);
  sendREQ(signal, ss);
}

void
DbtuxProxy::sendALTER_INDX_IMPL_REQ(Signal* signal, Uint32 ssId,
                                    SectionHandle * handle)
{
  Ss_ALTER_INDX_IMPL_REQ& ss = ssFind<Ss_ALTER_INDX_IMPL_REQ>(ssId);

  AlterIndxImplReq* req = (AlterIndxImplReq*)signal->getDataPtrSend();
  *req = ss.m_req;
  req->senderRef = reference();
  req->senderData = ssId;
  sendSignalNoRelease(workerRef(ss.m_worker), GSN_ALTER_INDX_IMPL_REQ,
                      signal, AlterIndxImplReq::SignalLength, JBB, handle);
}

void
DbtuxProxy::execALTER_INDX_IMPL_CONF(Signal* signal)
{
  const AlterIndxImplConf* conf = (const AlterIndxImplConf*)signal->getDataPtr();
  Uint32 ssId = conf->senderData;
  Ss_ALTER_INDX_IMPL_REQ& ss = ssFind<Ss_ALTER_INDX_IMPL_REQ>(ssId);
  recvCONF(signal, ss);
}

void
DbtuxProxy::execALTER_INDX_IMPL_REF(Signal* signal)
{
  const AlterIndxImplRef* ref = (const AlterIndxImplRef*)signal->getDataPtr();
  Uint32 ssId = ref->senderData;
  Ss_ALTER_INDX_IMPL_REQ& ss = ssFind<Ss_ALTER_INDX_IMPL_REQ>(ssId);
  recvREF(signal, ss, ref->errorCode);
}

void
DbtuxProxy::sendALTER_INDX_IMPL_CONF(Signal* signal, Uint32 ssId)
{
  Ss_ALTER_INDX_IMPL_REQ& ss = ssFind<Ss_ALTER_INDX_IMPL_REQ>(ssId);
  BlockReference dictRef = ss.m_req.senderRef;

  if (!lastReply(ss))
    return;

  if (ss.m_error == 0) {
    jam();
    AlterIndxImplConf* conf = (AlterIndxImplConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = ss.m_req.senderData;
    sendSignal(dictRef, GSN_ALTER_INDX_IMPL_CONF,
               signal, AlterIndxImplConf::SignalLength, JBB);
  } else {
    AlterIndxImplRef* ref = (AlterIndxImplRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = ss.m_req.senderData;
    ref->errorCode = ss.m_error;
    sendSignal(dictRef, GSN_ALTER_INDX_IMPL_REF,
               signal, AlterIndxImplRef::SignalLength, JBB);
  }

  ssRelease<Ss_ALTER_INDX_IMPL_REQ>(ssId);
}

// GSN_INDEX_STAT_IMPL_REQ

void
DbtuxProxy::execINDEX_STAT_IMPL_REQ(Signal* signal)
{
  jamEntry();
  const IndexStatImplReq* req =
    (const IndexStatImplReq*)signal->getDataPtr();
  Ss_INDEX_STAT_IMPL_REQ& ss = ssSeize<Ss_INDEX_STAT_IMPL_REQ>();
  ss.m_req = *req;
  ndbrequire(signal->getLength() == IndexStatImplReq::SignalLength);
  sendREQ(signal, ss);
}

void
DbtuxProxy::sendINDEX_STAT_IMPL_REQ(Signal* signal, Uint32 ssId,
                                    SectionHandle*)
{
  Ss_INDEX_STAT_IMPL_REQ& ss = ssFind<Ss_INDEX_STAT_IMPL_REQ>(ssId);

  IndexStatImplReq* req = (IndexStatImplReq*)signal->getDataPtrSend();
  *req = ss.m_req;
  req->senderRef = reference();
  req->senderData = ssId;

  const Uint32 instance = workerInstance(ss.m_worker);
  NdbLogPartInfo lpinfo(instance);

  //XXX remove unused
  switch (req->requestType) {
  case IndexStatReq::RT_START_MON:
    /*
     * DICT sets fragId if assigned frag is on this node, or else ZNIL
     * to turn off any possible old assignment.  In MT-LQH we also have
     * to check which worker owns the frag.
     */
    if (req->fragId != ZNIL
        && !lpinfo.partNoOwner(req->indexId, req->fragId)) {
      jam();
      req->fragId = ZNIL;
    }
    break;
  case IndexStatReq::RT_STOP_MON:
    /*
     * DICT sets fragId to ZNIL always.  There is no (pointless) check
     * to see if the frag was ever assigned.
     */
    ndbrequire(req->fragId == ZNIL);
    break;
  case IndexStatReq::RT_SCAN_FRAG:
    ndbrequire(req->fragId != ZNIL);
    if (!lpinfo.partNoOwner(req->indexId, req->fragId)) {
      jam();
      skipReq(ss);
      return;
    }
    break;
  case IndexStatReq::RT_CLEAN_NEW:
  case IndexStatReq::RT_CLEAN_OLD:
  case IndexStatReq::RT_CLEAN_ALL:
    ndbrequire(req->fragId == ZNIL);
    break;
  case IndexStatReq::RT_DROP_HEAD:
    /*
     * Only one client can do the PK-delete of the head record.  We use
     * of course the worker which owns the assigned fragment.
     */
    ndbrequire(req->fragId != ZNIL);
    if (!lpinfo.partNoOwner(req->indexId, req->fragId)) {
      jam();
      skipReq(ss);
      return;
    }
    break;
  default:
    ndbrequire(false);
    break;
  }

  sendSignal(workerRef(ss.m_worker), GSN_INDEX_STAT_IMPL_REQ,
             signal, IndexStatImplReq::SignalLength, JBB);
}

void
DbtuxProxy::execINDEX_STAT_IMPL_CONF(Signal* signal)
{
  jamEntry();
  const IndexStatImplConf* conf =
    (const IndexStatImplConf*)signal->getDataPtr();
  Uint32 ssId = conf->senderData;
  Ss_INDEX_STAT_IMPL_REQ& ss = ssFind<Ss_INDEX_STAT_IMPL_REQ>(ssId);
  recvCONF(signal, ss);
}

void
DbtuxProxy::execINDEX_STAT_IMPL_REF(Signal* signal)
{
  jamEntry();
  const IndexStatImplRef* ref = (const IndexStatImplRef*)signal->getDataPtr();
  Uint32 ssId = ref->senderData;
  Ss_INDEX_STAT_IMPL_REQ& ss = ssFind<Ss_INDEX_STAT_IMPL_REQ>(ssId);
  recvREF(signal, ss, ref->errorCode);
}

void
DbtuxProxy::sendINDEX_STAT_IMPL_CONF(Signal* signal, Uint32 ssId)
{
  Ss_INDEX_STAT_IMPL_REQ& ss = ssFind<Ss_INDEX_STAT_IMPL_REQ>(ssId);
  BlockReference dictRef = ss.m_req.senderRef;

  if (!lastReply(ss))
    return;

  if (ss.m_error == 0) {
    jam();
    IndexStatImplConf* conf = (IndexStatImplConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = ss.m_req.senderData;
    sendSignal(dictRef, GSN_INDEX_STAT_IMPL_CONF,
               signal, IndexStatImplConf::SignalLength, JBB);
  } else {
    IndexStatImplRef* ref = (IndexStatImplRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = ss.m_req.senderData;
    ref->errorCode = ss.m_error;
    sendSignal(dictRef, GSN_INDEX_STAT_IMPL_REF,
               signal, IndexStatImplRef::SignalLength, JBB);
  }

  ssRelease<Ss_INDEX_STAT_IMPL_REQ>(ssId);
}

// GSN_INDEX_STAT_REP

void
DbtuxProxy::execINDEX_STAT_REP(Signal* signal)
{
  jamEntry();
  const IndexStatRep* rep =
    (const IndexStatRep*)signal->getDataPtr();
  Ss_INDEX_STAT_REP& ss = ssSeize<Ss_INDEX_STAT_REP>();
  ss.m_rep = *rep;
  ndbrequire(signal->getLength() == IndexStatRep::SignalLength);
  sendREQ(signal, ss);
  ssRelease<Ss_INDEX_STAT_REP>(ss);
}

void
DbtuxProxy::sendINDEX_STAT_REP(Signal* signal, Uint32 ssId,
                               SectionHandle*)
{
  Ss_INDEX_STAT_REP& ss = ssFind<Ss_INDEX_STAT_REP>(ssId);

  IndexStatRep* rep = (IndexStatRep*)signal->getDataPtrSend();
  *rep = ss.m_rep;
  rep->senderData = reference();
  rep->senderData = ssId;

  const Uint32 instance = workerInstance(ss.m_worker);
  NdbLogPartInfo lpinfo(instance);

  ndbrequire(rep->fragId != ZNIL);
  if (!lpinfo.partNoOwner(rep->indexId, rep->fragId)) {
    jam();
    skipReq(ss);
    return;
  }

  sendSignal(workerRef(ss.m_worker), GSN_INDEX_STAT_REP,
             signal, IndexStatRep::SignalLength, JBB);
}

BLOCK_FUNCTIONS(DbtuxProxy)
