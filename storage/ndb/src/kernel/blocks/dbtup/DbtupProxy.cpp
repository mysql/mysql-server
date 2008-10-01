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

#include "DbtupProxy.hpp"
#include "Dbtup.hpp"

DbtupProxy::DbtupProxy(Block_context& ctx, Pgman* pgman) :
  LocalProxy(DBTUP, ctx),
  m_pgman(pgman)
{
  ndbrequire(m_pgman != 0);

  // GSN_DROP_TAB_REQ
  addRecSignal(GSN_DROP_TAB_REQ, &DbtupProxy::execDROP_TAB_REQ);
  addRecSignal(GSN_DROP_TAB_CONF, &DbtupProxy::execDROP_TAB_CONF);

  // GSN_BUILD_INDX_IMPL_REQ
  addRecSignal(GSN_BUILD_INDX_IMPL_REQ, &DbtupProxy::execBUILD_INDX_IMPL_REQ);
  addRecSignal(GSN_BUILD_INDX_IMPL_CONF, &DbtupProxy::execBUILD_INDX_IMPL_CONF);
  addRecSignal(GSN_BUILD_INDX_IMPL_REF, &DbtupProxy::execBUILD_INDX_IMPL_REF);
}

DbtupProxy::~DbtupProxy()
{
}

SimulatedBlock*
DbtupProxy::newWorker(Uint32 instanceNo)
{
  return new Dbtup(m_ctx, m_pgman, instanceNo);
}

// GSN_DROP_TAB_REQ

void
DbtupProxy::execDROP_TAB_REQ(Signal* signal)
{
  const DropTabReq* req = (const DropTabReq*)signal->getDataPtr();
  Uint32 ssId = getSsId(req);
  Ss_DROP_TAB_REQ& ss = ssSeize<Ss_DROP_TAB_REQ>(ssId);
  ss.m_req = *req;
  ndbrequire(signal->getLength() == DropTabReq::SignalLength);
  sendREQ(signal, ss);
}

void
DbtupProxy::sendDROP_TAB_REQ(Signal* signal, Uint32 ssId)
{
  Ss_DROP_TAB_REQ& ss = ssFind<Ss_DROP_TAB_REQ>(ssId);

  DropTabReq* req = (DropTabReq*)signal->getDataPtrSend();
  *req = ss.m_req;
  req->senderRef = reference();
  req->senderData = ssId; // redundant since tableId is used
  sendSignal(workerRef(ss.m_worker), GSN_DROP_TAB_REQ,
             signal, DropTabReq::SignalLength, JBB);
}

void
DbtupProxy::execDROP_TAB_CONF(Signal* signal)
{
  const DropTabConf* conf = (const DropTabConf*)signal->getDataPtr();
  Uint32 ssId = getSsId(conf);
  Ss_DROP_TAB_REQ& ss = ssFind<Ss_DROP_TAB_REQ>(ssId);
  recvCONF(signal, ss);
}

void
DbtupProxy::sendDROP_TAB_CONF(Signal* signal, Uint32 ssId)
{
  Ss_DROP_TAB_REQ& ss = ssFind<Ss_DROP_TAB_REQ>(ssId);
  BlockReference dictRef = ss.m_req.senderRef;

  if (!lastReply(ss))
    return;

  if (ss.m_error == 0) {
    jam();
    DropTabConf* conf = (DropTabConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = ss.m_req.senderData;
    conf->tableId = ss.m_req.tableId;
    sendSignal(dictRef, GSN_DROP_TAB_CONF,
               signal, DropTabConf::SignalLength, JBB);
  } else {
    ndbrequire(false);
  }

  ssRelease<Ss_DROP_TAB_REQ>(ssId);
}

// GSN_BUILD_INDX_IMPL_REQ

void
DbtupProxy::execBUILD_INDX_IMPL_REQ(Signal* signal)
{
  const BuildIndxImplReq* req = (const BuildIndxImplReq*)signal->getDataPtr();
  Ss_BUILD_INDX_IMPL_REQ& ss = ssSeize<Ss_BUILD_INDX_IMPL_REQ>();
  ss.m_req = *req;
  ndbrequire(signal->getLength() == BuildIndxImplReq::SignalLength);
  sendREQ(signal, ss);
}

void
DbtupProxy::sendBUILD_INDX_IMPL_REQ(Signal* signal, Uint32 ssId)
{
  Ss_BUILD_INDX_IMPL_REQ& ss = ssFind<Ss_BUILD_INDX_IMPL_REQ>(ssId);

  BuildIndxImplReq* req = (BuildIndxImplReq*)signal->getDataPtrSend();
  *req = ss.m_req;
  req->senderRef = reference();
  req->senderData = ssId;
  sendSignal(workerRef(ss.m_worker), GSN_BUILD_INDX_IMPL_REQ,
             signal, BuildIndxImplReq::SignalLength, JBB);
}

void
DbtupProxy::execBUILD_INDX_IMPL_CONF(Signal* signal)
{
  const BuildIndxImplConf* conf = (const BuildIndxImplConf*)signal->getDataPtr();
  Uint32 ssId = conf->senderData;
  Ss_BUILD_INDX_IMPL_REQ& ss = ssFind<Ss_BUILD_INDX_IMPL_REQ>(ssId);
  recvCONF(signal, ss);
}

void
DbtupProxy::execBUILD_INDX_IMPL_REF(Signal* signal)
{
  const BuildIndxImplRef* ref = (const BuildIndxImplRef*)signal->getDataPtr();
  Uint32 ssId = ref->senderData;
  Ss_BUILD_INDX_IMPL_REQ& ss = ssFind<Ss_BUILD_INDX_IMPL_REQ>(ssId);
  recvREF(signal, ss, ref->errorCode);
}

void
DbtupProxy::sendBUILD_INDX_IMPL_CONF(Signal* signal, Uint32 ssId)
{
  Ss_BUILD_INDX_IMPL_REQ& ss = ssFind<Ss_BUILD_INDX_IMPL_REQ>(ssId);
  BlockReference dictRef = ss.m_req.senderRef;

  if (!lastReply(ss))
    return;

  if (ss.m_error == 0) {
    jam();
    BuildIndxImplConf* conf = (BuildIndxImplConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = ss.m_req.senderData;
    sendSignal(dictRef, GSN_BUILD_INDX_IMPL_CONF,
               signal, BuildIndxImplConf::SignalLength, JBB);
  } else {
    BuildIndxImplRef* ref = (BuildIndxImplRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = ss.m_req.senderData;
    ref->errorCode = ss.m_error;
    sendSignal(dictRef, GSN_BUILD_INDX_IMPL_REF,
               signal, BuildIndxImplRef::SignalLength, JBB);
  }

  ssRelease<Ss_BUILD_INDX_IMPL_REQ>(ssId);
}

BLOCK_FUNCTIONS(DbtupProxy)
