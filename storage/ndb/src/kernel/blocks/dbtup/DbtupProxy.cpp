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

DbtupProxy::DbtupProxy(Block_context& ctx) :
  LocalProxy(DBTUP, ctx)
{
  addRecSignal(GSN_SEND_PACKED, &DbtupProxy::execSEND_PACKED);

  // GSN_DROP_TAB_REQ
  addRecSignal(GSN_DROP_TAB_REQ, &DbtupProxy::execDROP_TAB_REQ);
  addRecSignal(GSN_DROP_TAB_CONF, &DbtupProxy::execDROP_TAB_CONF);
}

DbtupProxy::~DbtupProxy()
{
}

SimulatedBlock*
DbtupProxy::newWorker(Uint32 instanceNo)
{
  return new Dbtup(m_ctx, 0, instanceNo);
}

// GSN_SEND_PACKED

void
DbtupProxy::execSEND_PACKED(Signal* signal)
{
  Uint32 i;
  for (i = 0; i < c_workers; i++) {
    Dbtup* dbtup = static_cast<Dbtup*>(c_worker[i]);
    dbtup->execSEND_PACKED(signal);
  }
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

BLOCK_FUNCTIONS(DbtupProxy)
