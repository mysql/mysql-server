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

// can be removed if DBTUP continueB codes are moved to signaldata
#define DBTUP_C

#include "DbtupProxy.hpp"
#include "Dbtup.hpp"
#include <pgman.hpp>
#include <signaldata/LgmanContinueB.hpp>

#include <EventLogger.hpp>
extern EventLogger * g_eventLogger;

DbtupProxy::DbtupProxy(Block_context& ctx) :
  LocalProxy(DBTUP, ctx),
  c_pgman(0),
  c_tableRecSize(0),
  c_tableRec(0)
{
  // GSN_CREATE_TAB_REQ
  addRecSignal(GSN_CREATE_TAB_REQ, &DbtupProxy::execCREATE_TAB_REQ);
  addRecSignal(GSN_DROP_TAB_REQ, &DbtupProxy::execDROP_TAB_REQ);

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
  return new Dbtup(m_ctx, instanceNo);
}

// GSN_READ_CONFIG_REQ
void
DbtupProxy::callREAD_CONFIG_REQ(Signal* signal)
{
  const ReadConfigReq* req = (const ReadConfigReq*)signal->getDataPtr();
  ndbrequire(req->noOfParameters == 0);

  const ndb_mgm_configuration_iterator * p = 
    m_ctx.m_config.getOwnConfigIterator();
  ndbrequire(p != 0);
  
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_TUP_TABLE, &c_tableRecSize));
  c_tableRec = (Uint8*)allocRecord("TableRec", sizeof(Uint8), c_tableRecSize);
  D("proxy:" << V(c_tableRecSize));
  Uint32 i;
  for (i = 0; i < c_tableRecSize; i++)
    c_tableRec[i] = 0;
  backREAD_CONFIG_REQ(signal);
}

// GSN_STTOR

void
DbtupProxy::callSTTOR(Signal* signal)
{
  Uint32 startPhase = signal->theData[1];
  switch (startPhase) {
  case 1:
    c_pgman = (Pgman*)globalData.getBlock(PGMAN);
    ndbrequire(c_pgman != 0);
    break;
  }
  backSTTOR(signal);
}

// GSN_CREATE_TAB_REQ

void
DbtupProxy::execCREATE_TAB_REQ(Signal* signal)
{
  const CreateTabReq* req = (const CreateTabReq*)signal->getDataPtr();
  const Uint32 tableId = req->tableId;
  ndbrequire(tableId < c_tableRecSize);
  ndbrequire(c_tableRec[tableId] == 0);
  c_tableRec[tableId] = 1;
  D("proxy: created table" << V(tableId));
}

void
DbtupProxy::execDROP_TAB_REQ(Signal* signal)
{
  const DropTabReq* req = (const DropTabReq*)signal->getDataPtr();
  const Uint32 tableId = req->tableId;
  ndbrequire(tableId < c_tableRecSize);
  c_tableRec[tableId] = 0;
  D("proxy: dropped table" << V(tableId));
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
DbtupProxy::sendBUILD_INDX_IMPL_REQ(Signal* signal, Uint32 ssId,
                                    SectionHandle* handle)
{
  Ss_BUILD_INDX_IMPL_REQ& ss = ssFind<Ss_BUILD_INDX_IMPL_REQ>(ssId);

  BuildIndxImplReq* req = (BuildIndxImplReq*)signal->getDataPtrSend();
  *req = ss.m_req;
  req->senderRef = reference();
  req->senderData = ssId;
  sendSignalNoRelease(workerRef(ss.m_worker), GSN_BUILD_INDX_IMPL_REQ,
                      signal, BuildIndxImplReq::SignalLength, JBB, handle);
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

// client methods

// LGMAN

DbtupProxy::Proxy_undo::Proxy_undo()
{
  m_type = 0;
  m_len = 0;
  m_ptr = 0;
  m_lsn = (Uint64)0;
  m_key.setNull();
  m_page_id = ~(Uint32)0;
  m_table_id = ~(Uint32)0;
  m_fragment_id = ~(Uint32)0;
  m_instance_no = ~(Uint32)0;
  m_actions = 0;
  m_in_use = false;
}

void
DbtupProxy::disk_restart_undo(Signal* signal, Uint64 lsn,
                              Uint32 type, const Uint32 * ptr, Uint32 len)
{
  Proxy_undo& undo = c_proxy_undo;
  ndbrequire(!undo.m_in_use);
  new (&undo) Proxy_undo;
  undo.m_in_use = true;

  D("proxy: disk_restart_undo" << V(type) << hex << V(ptr) << dec << V(len) << V(lsn));
  undo.m_type = type;
  undo.m_len = len;
  undo.m_ptr = ptr;
  undo.m_lsn = lsn;

  /*
   * The timeslice via PGMAN/5 gives LGMAN a chance to overwrite the
   * data pointed to by ptr.  So save it now and do not use ptr.
   */
  ndbrequire(undo.m_len <= Proxy_undo::MaxData);
  memcpy(undo.m_data, undo.m_ptr, undo.m_len << 2);

  switch (undo.m_type) {
  case File_formats::Undofile::UNDO_LCP_FIRST:
  case File_formats::Undofile::UNDO_LCP:
    {
      undo.m_table_id = ptr[1] >> 16;
      undo.m_fragment_id = ptr[1] & 0xFFFF;
      undo.m_actions |= Proxy_undo::SendToAll;
      undo.m_actions |= Proxy_undo::SendUndoNext;
    }
    break;
  case File_formats::Undofile::UNDO_TUP_ALLOC:
    {
      const Dbtup::Disk_undo::Alloc* rec =
        (const Dbtup::Disk_undo::Alloc*)ptr;
      undo.m_key.m_file_no = rec->m_file_no_page_idx >> 16;
      undo.m_key.m_page_no = rec->m_page_no;
      undo.m_key.m_page_idx = rec->m_file_no_page_idx & 0xFFFF;
      undo.m_actions |= Proxy_undo::ReadTupPage;
      undo.m_actions |= Proxy_undo::GetInstance;
    }
    break;
  case File_formats::Undofile::UNDO_TUP_UPDATE:
    {
      const Dbtup::Disk_undo::Update* rec =
        (const Dbtup::Disk_undo::Update*)ptr;
      undo.m_key.m_file_no = rec->m_file_no_page_idx >> 16;
      undo.m_key.m_page_no = rec->m_page_no;
      undo.m_key.m_page_idx = rec->m_file_no_page_idx & 0xFFFF;
      undo.m_actions |= Proxy_undo::ReadTupPage;
      undo.m_actions |= Proxy_undo::GetInstance;
    }
    break;
  case File_formats::Undofile::UNDO_TUP_FREE:
    {
      const Dbtup::Disk_undo::Free* rec =
        (const Dbtup::Disk_undo::Free*)ptr;
      undo.m_key.m_file_no = rec->m_file_no_page_idx >> 16;
      undo.m_key.m_page_no = rec->m_page_no;
      undo.m_key.m_page_idx = rec->m_file_no_page_idx & 0xFFFF;
      undo.m_actions |= Proxy_undo::ReadTupPage;
      undo.m_actions |= Proxy_undo::GetInstance;
    }
    break;

  case File_formats::Undofile::UNDO_TUP_CREATE:
  {
    jam();
    Dbtup::Disk_undo::Create* rec= (Dbtup::Disk_undo::Create*)ptr;
    Uint32 tableId = rec->m_table;
    if (tableId < c_tableRecSize)
    {
      jam();
      c_tableRec[tableId] = 0;
    }
    
    undo.m_actions |= Proxy_undo::SendToAll;
    undo.m_actions |= Proxy_undo::SendUndoNext;
    break;
  }
  case File_formats::Undofile::UNDO_TUP_DROP:
  {
    jam();
    Dbtup::Disk_undo::Drop* rec= (Dbtup::Disk_undo::Drop*)ptr;
    Uint32 tableId = rec->m_table;
    if (tableId < c_tableRecSize)
    {
      jam();
      c_tableRec[tableId] = 0;
    }
    
    undo.m_actions |= Proxy_undo::SendToAll;
    undo.m_actions |= Proxy_undo::SendUndoNext;
    break;
  }
#if NOT_YET_UNDO_ALLOC_EXTENT
  case File_formats::Undofile::UNDO_TUP_ALLOC_EXTENT:
    ndbrequire(false);
    break;
#endif
#if NOT_YET_UNDO_FREE_EXTENT
  case File_formats::Undofile::UNDO_TUP_FREE_EXTENT:
    ndbrequire(false);
    break;
#endif
  case File_formats::Undofile::UNDO_END:
    {
      undo.m_actions |= Proxy_undo::SendToAll;
    }
    break;
  default:
    ndbrequire(false);
    break;
  }

  if (undo.m_actions & Proxy_undo::ReadTupPage) {
    jam();
    /*
     * Page request goes to the extra PGMAN worker (our thread).
     * TUP worker reads same page again via another PGMAN worker.
     * MT-LGMAN is planned, do not optimize (pass page) now
     */
    Page_cache_client pgman(this, c_pgman);
    Page_cache_client::Request req;

    req.m_page = undo.m_key;
    req.m_callback.m_callbackData = 0;
    req.m_callback.m_callbackFunction = 
      safe_cast(&DbtupProxy::disk_restart_undo_callback);

    int ret = pgman.get_page(signal, req, 0);
    ndbrequire(ret >= 0);
    if (ret > 0) {
      jam();
      execute(signal, req.m_callback, (Uint32)ret);
    }
    return;
  }

  disk_restart_undo_finish(signal);
}

void
DbtupProxy::disk_restart_undo_callback(Signal* signal, Uint32, Uint32 page_id)
{
  Proxy_undo& undo = c_proxy_undo;
  undo.m_page_id = page_id;

  Ptr<GlobalPage> gptr;
  m_global_page_pool.getPtr(gptr, undo.m_page_id);

  ndbrequire(undo.m_actions & Proxy_undo::ReadTupPage);
  {
    jam();
    const Tup_page* page = (const Tup_page*)gptr.p;
    const File_formats::Page_header& header = page->m_page_header;
    const Uint32 page_type = header.m_page_type;

    if (page_type == 0) { // wl4391_todo ?
      jam();
      ndbrequire(header.m_page_lsn_hi == 0 && header.m_page_lsn_lo == 0);
      undo.m_actions |= Proxy_undo::NoExecute;
      undo.m_actions |= Proxy_undo::SendUndoNext;
      D("proxy: callback" << V(page_type));
    } else {
      ndbrequire(page_type == File_formats::PT_Tup_fixsize_page ||
                 page_type == File_formats::PT_Tup_varsize_page);

      Uint64 page_lsn = (Uint64(header.m_page_lsn_hi) << 32) + header.m_page_lsn_lo;
      if (! (undo.m_lsn <= page_lsn))
      {
        jam();
        undo.m_actions |= Proxy_undo::NoExecute;
        undo.m_actions |= Proxy_undo::SendUndoNext;
      }

      undo.m_table_id = page->m_table_id;
      undo.m_fragment_id = page->m_fragment_id;
      D("proxy: callback" << V(undo.m_table_id) << V(undo.m_fragment_id));
      const Uint32 tableId = undo.m_table_id;
      if (tableId >= c_tableRecSize || c_tableRec[tableId] == 0) {
        D("proxy: table dropped" << V(tableId));
        undo.m_actions |= Proxy_undo::NoExecute;
        undo.m_actions |= Proxy_undo::SendUndoNext;
      }
    }
  }

  disk_restart_undo_finish(signal);
}

void
DbtupProxy::disk_restart_undo_finish(Signal* signal)
{
  Proxy_undo& undo = c_proxy_undo;

  if (undo.m_actions & Proxy_undo::SendUndoNext) {
    jam();
    signal->theData[0] = LgmanContinueB::EXECUTE_UNDO_RECORD;
    sendSignal(LGMAN_REF, GSN_CONTINUEB, signal, 1, JBB);
  }

  if (undo.m_actions & Proxy_undo::NoExecute) {
    jam();
    goto finish;
  }

  if (undo.m_actions & Proxy_undo::GetInstance) {
    jam();
    Uint32 instanceKey = getInstanceKey(undo.m_table_id, undo.m_fragment_id);
    Uint32 instanceNo = getInstanceFromKey(instanceKey);
    undo.m_instance_no = instanceNo;
  }

  if (!(undo.m_actions & Proxy_undo::SendToAll)) {
    jam();
    ndbrequire(undo.m_instance_no != 0);
    Uint32 i = undo.m_instance_no - 1;
    disk_restart_undo_send(signal, i);
  } else {
    jam();
    Uint32 i;
    for (i = 0; i < c_workers; i++) {
      disk_restart_undo_send(signal, i);
    }
  }

finish:
  ndbrequire(undo.m_in_use);
  undo.m_in_use = false;
}

void
DbtupProxy::disk_restart_undo_send(Signal* signal, Uint32 i)
{
  /*
   * Send undo entry via long signal because:
   * 1) a method call would execute in another non-mutexed Pgman
   * 2) MT-LGMAN is planned, do not optimize (pass pointer) now
   */
  Proxy_undo& undo = c_proxy_undo;

  LinearSectionPtr ptr[3];
  ptr[0].p = undo.m_data;
  ptr[0].sz = undo.m_len;

  signal->theData[0] = ZDISK_RESTART_UNDO;
  signal->theData[1] = undo.m_type;
  signal->theData[2] = undo.m_len;
  signal->theData[3] = (Uint32)(undo.m_lsn >> 32);
  signal->theData[4] = (Uint32)(undo.m_lsn & 0xFFFFFFFF);
  sendSignal(workerRef(i), GSN_CONTINUEB, signal, 5, JBB, ptr, 1);
}

// TSMAN

int
DbtupProxy::disk_restart_alloc_extent(Uint32 tableId, Uint32 fragId, 
                                      const Local_key* key, Uint32 pages)
{
  if (tableId >= c_tableRecSize || c_tableRec[tableId] == 0) {
    jam();
    D("proxy: table dropped" << V(tableId));
    return -1;
  }

  // local call so mapping instance key to number is ok
  Uint32 instanceKey = getInstanceKey(tableId, fragId);
  Uint32 instanceNo = getInstanceFromKey(instanceKey);

  Uint32 i = workerIndex(instanceNo);
  Dbtup* dbtup = (Dbtup*)workerBlock(i);
  return dbtup->disk_restart_alloc_extent(tableId, fragId, key, pages);
}

void
DbtupProxy::disk_restart_page_bits(Uint32 tableId, Uint32 fragId,
                                   const Local_key* key, Uint32 bits)
{
  ndbrequire(tableId < c_tableRecSize && c_tableRec[tableId] == 1);

  // local call so mapping instance key to number is ok
  Uint32 instanceKey = getInstanceKey(tableId, fragId);
  Uint32 instanceNo = getInstanceFromKey(instanceKey);

  Uint32 i = workerIndex(instanceNo);
  Dbtup* dbtup = (Dbtup*)workerBlock(i);
  dbtup->disk_restart_page_bits(tableId, fragId, key, bits);
}

BLOCK_FUNCTIONS(DbtupProxy)
