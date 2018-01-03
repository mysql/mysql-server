/* Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.

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

// can be removed if DBTUP continueB codes are moved to signaldata
#define DBTUP_C

#include "DbtupProxy.hpp"
#include "Dbtup.hpp"
#include <pgman.hpp>
#include <signaldata/LgmanContinueB.hpp>

#include <EventLogger.hpp>

#define JAM_FILE_ID 413

extern EventLogger * g_eventLogger;

//#define DEBUG_TUP_RESTART_ 1
#ifdef DEBUG_TUP_RESTART
#define DEB_TUP_RESTART(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_TUP_RESTART(arglist) do { } while (0)
#endif

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
  const Uint32 create_table_schema_version = req->tableVersion & 0xFFFFFF;
  ndbrequire(tableId < c_tableRecSize);
  ndbrequire(create_table_schema_version != 0);
  ndbrequire(c_tableRec[tableId] == 0);
  c_tableRec[tableId] = create_table_schema_version;
  DEB_TUP_RESTART(("Create table: %u", tableId));
  D("proxy: created table" << V(tableId));
}

void
DbtupProxy::execDROP_TAB_REQ(Signal* signal)
{
  const DropTabReq* req = (const DropTabReq*)signal->getDataPtr();
  const Uint32 tableId = req->tableId;
  ndbrequire(tableId < c_tableRecSize);
  c_tableRec[tableId] = 0;
  DEB_TUP_RESTART(("Dropped table: %u", tableId));
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

  /**
   * All the logic about when to stop executing the UNDO log
   * is in lgman.cpp. So this code assumes that we haven't
   * yet reached the end of the UNDO log execution.
   */
  switch (undo.m_type) {
  case File_formats::Undofile::UNDO_LOCAL_LCP_FIRST:
  case File_formats::Undofile::UNDO_LOCAL_LCP:
  {
    undo.m_table_id = ptr[2] >> 16;
    undo.m_fragment_id = ptr[2] & 0xFFFF;
    undo.m_actions |= Proxy_undo::SendToAll;
    undo.m_actions |= Proxy_undo::SendUndoNext;
    break;
  }
  case File_formats::Undofile::UNDO_LCP_FIRST:
  case File_formats::Undofile::UNDO_LCP:
  {
    /**
     * This is the start of the UNDO log, this is the synchronisation
     * point, so we will UNDO information back to here. After this
     * we don't need any more UNDO logging, we do still however need
     * to use the UNDO logs to synchronize the extent bits with the
     * page information.
     */
    undo.m_table_id = ptr[1] >> 16;
    undo.m_fragment_id = ptr[1] & 0xFFFF;
    undo.m_actions |= Proxy_undo::SendToAll;
    undo.m_actions |= Proxy_undo::SendUndoNext;
    break;
  }
  case File_formats::Undofile::UNDO_TUP_ALLOC:
  {
    const Dbtup::Disk_undo::Alloc* rec =
      (const Dbtup::Disk_undo::Alloc*)ptr;
    undo.m_key.m_file_no = rec->m_file_no_page_idx >> 16;
    undo.m_key.m_page_no = rec->m_page_no;
    undo.m_key.m_page_idx = rec->m_file_no_page_idx & 0xFFFF;
    undo.m_actions |= Proxy_undo::ReadTupPage;
    undo.m_actions |= Proxy_undo::GetInstance;
    break;
  }
  case File_formats::Undofile::UNDO_TUP_UPDATE:
  case File_formats::Undofile::UNDO_TUP_FIRST_UPDATE_PART:
  {
    const Dbtup::Disk_undo::Update* rec =
      (const Dbtup::Disk_undo::Update*)ptr;
    undo.m_key.m_file_no = rec->m_file_no_page_idx >> 16;
    undo.m_key.m_page_no = rec->m_page_no;
    undo.m_key.m_page_idx = rec->m_file_no_page_idx & 0xFFFF;
    undo.m_actions |= Proxy_undo::ReadTupPage;
    undo.m_actions |= Proxy_undo::GetInstance;
    break;
  }
  case File_formats::Undofile::UNDO_TUP_UPDATE_PART:
  {
    const Dbtup::Disk_undo::UpdatePart* rec =
      (const Dbtup::Disk_undo::UpdatePart*)ptr;
    undo.m_key.m_file_no = rec->m_file_no_page_idx >> 16;
    undo.m_key.m_page_no = rec->m_page_no;
    undo.m_key.m_page_idx = rec->m_file_no_page_idx & 0xFFFF;
    undo.m_actions |= Proxy_undo::ReadTupPage;
    undo.m_actions |= Proxy_undo::GetInstance;
    break;
  }
  case File_formats::Undofile::UNDO_TUP_FREE:
  case File_formats::Undofile::UNDO_TUP_FREE_PART:
  {
    const Dbtup::Disk_undo::Free* rec =
      (const Dbtup::Disk_undo::Free*)ptr;
    undo.m_key.m_file_no = rec->m_file_no_page_idx >> 16;
    undo.m_key.m_page_no = rec->m_page_no;
    undo.m_key.m_page_idx = rec->m_file_no_page_idx & 0xFFFF;
    undo.m_actions |= Proxy_undo::ReadTupPage;
    undo.m_actions |= Proxy_undo::GetInstance;
    break;
  }
  case File_formats::Undofile::UNDO_TUP_DROP:
  {
    jam();
    /**
     * A table was dropped during UNDO log writing. This means that the
     * table is no longer present, if no LCP record or CREATE record have
     * occurred before this record then this is also a synch point. This
     * synch point also says that the table is empty, but here the table
     * as such should not be remaining either.
     */
    undo.m_actions |= Proxy_undo::SendToAll;
    undo.m_actions |= Proxy_undo::SendUndoNext;
    break;
  }
  case File_formats::Undofile::UNDO_END:
  {
    undo.m_actions |= Proxy_undo::SendToAll;
    break;
  }
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
     *
     * We need to read page in order to get table id and fragment id.
     * This is not part of the UNDO log information and this information
     * is required such that we can map this to the correct LDM
     * instance. We will not make page dirty, so it will be replaced
     * as soon as we need a dirty page or we're out of pages in this
     * PGMAN instance.
     */
    Page_cache_client pgman(this, c_pgman);
    Page_cache_client::Request req;

    /**
     * Ensure that we crash if we try to make a LCP of this page
     * later, should never happen since we never do any LCP of
     * pages connected to fragments in extra pgman worker.
     * page.
     */
    req.m_table_id = RNIL;
    req.m_fragment_id = 0;
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
      D("proxy: callback" << V(undo.m_table_id) <<
                             V(undo.m_fragment_id) <<
                             V(undo.m_create_table_version));
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
DbtupProxy::disk_restart_undo_send_next(Signal *signal)
{
  signal->theData[0] = LgmanContinueB::EXECUTE_UNDO_RECORD;
  signal->theData[1] = 0; /* Not applied flag */
  sendSignal(LGMAN_REF, GSN_CONTINUEB, signal, 2, JBB);
}

void
DbtupProxy::disk_restart_undo_finish(Signal* signal)
{
  Proxy_undo& undo = c_proxy_undo;

  if (undo.m_actions & Proxy_undo::SendUndoNext) {
    jam();
    disk_restart_undo_send_next(signal);
  }

  if (undo.m_actions & Proxy_undo::NoExecute) {
    jam();
    goto finish;
  }

  if (undo.m_actions & Proxy_undo::GetInstance) {
    jam();
    Uint32 instanceKey = getInstanceKeyCanFail(undo.m_table_id,
                                               undo.m_fragment_id);
    if (instanceKey == RNIL)
    {
      jam();
      /**
       * Ignore this record since no table with this table id and
       * fragment id is currently existing.
       */
      disk_restart_undo_send_next(signal);
      goto finish;
    }
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
DbtupProxy::disk_restart_alloc_extent(EmulatedJamBuffer *jamBuf,
                                      Uint32 tableId,
                                      Uint32 fragId,
                                      Uint32 create_table_version,
                                      const Local_key* key,
                                      Uint32 pages)
{
  if (tableId >= c_tableRecSize || c_tableRec[tableId] == 0)
  {
    thrjam(jamBuf);
    D("proxy: table dropped" << V(tableId));
    DEB_TUP_RESTART(("disk_restart_alloc_extent failed on tab(%u,%u):%u,"
                     " tableId missing",
                     tableId,
                     fragId,
                     create_table_version));
    return -1;
  }
  if (c_tableRec[tableId] != create_table_version)
  {
    thrjam(jamBuf);
    DEB_TUP_RESTART(("disk_restart_alloc_extent failed on tab(%u,%u):%u,"
                     " expected create_table_version: %u",
                     tableId,
                     fragId,
                     create_table_version,
                     c_tableRec[tableId]));
    return -1;
  }

  // local call so mapping instance key to number is ok
  thrjam(jamBuf);
  thrjamLine(jamBuf, Uint16(tableId));
  thrjamLine(jamBuf, Uint16(fragId));
  Uint32 instanceKey = getInstanceKeyCanFail(tableId, fragId);
  if (instanceKey == RNIL)
  {
    thrjam(jamBuf);
    DEB_TUP_RESTART(("disk_restart_alloc_extent failed, instanceKey = RNIL"));
    D("proxy: table either dropped, non-existent or fragment not existing"
      << V(tableId));
    return -1;
  }
  thrjam(jamBuf);
  Uint32 instanceNo = getInstanceFromKey(instanceKey);

  Uint32 i = workerIndex(instanceNo);
  Dbtup* dbtup = (Dbtup*)workerBlock(i);
  return dbtup->disk_restart_alloc_extent(jamBuf,
                                          tableId,
                                          fragId,
                                          create_table_version,
                                          key, 
                                          pages);
}

void
DbtupProxy::disk_restart_page_bits(Uint32 tableId,
                                   Uint32 fragId,
                                   Uint32 create_table_version,
                                   const Local_key* key,
                                   Uint32 bits)
{
  ndbrequire(tableId < c_tableRecSize &&
             c_tableRec[tableId] != 0 &&
             (create_table_version == 0 ||
              c_tableRec[tableId] == create_table_version));

  // local call so mapping instance key to number is ok
  /**
   * No need to use getInstanceKeyCanFail here since this call is
   * preceded by a call to disk_restart_alloc_extent above where
   * this is checked.
   */
  Uint32 instanceKey = getInstanceKey(tableId, fragId);
  Uint32 instanceNo = getInstanceFromKey(instanceKey);

  Uint32 i = workerIndex(instanceNo);
  Dbtup* dbtup = (Dbtup*)workerBlock(i);
  dbtup->disk_restart_page_bits(jamBuffer(),
                                tableId,
                                fragId,
                                create_table_version,
                                key,
                                bits);
}
BLOCK_FUNCTIONS(DbtupProxy)
