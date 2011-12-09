/*
   Copyright (c) 2005, 2011, Oracle and/or its affiliates. All rights reserved.

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

#include "lgman.hpp"
#include "diskpage.hpp"
#include <signaldata/FsRef.hpp>
#include <signaldata/FsConf.hpp>
#include <signaldata/FsOpenReq.hpp>
#include <signaldata/FsCloseReq.hpp>
#include <signaldata/CreateFilegroupImpl.hpp>
#include <signaldata/DropFilegroupImpl.hpp>
#include <signaldata/FsReadWriteReq.hpp>
#include <signaldata/LCP.hpp>
#include <signaldata/SumaImpl.hpp>
#include <signaldata/LgmanContinueB.hpp>
#include <signaldata/GetTabInfo.hpp>
#include <signaldata/NodeFailRep.hpp>
#include <signaldata/DbinfoScan.hpp>
#include "dbtup/Dbtup.hpp"

#include <EventLogger.hpp>
extern EventLogger * g_eventLogger;

#include <record_types.hpp>

/**
 * ---<a>-----<b>-----<c>-----<d>---> (time)
 *
 * <a> = start of lcp 1
 * <b> = stop of lcp 1
 * <c> = start of lcp 2
 * <d> = stop of lcp 2
 *
 * If ndb crashes before <d>
 *   the entire undo log from crash point until <a> has to be applied
 *
 * at <d> the undo log can be cut til <c> 
 */

#define DEBUG_UNDO_EXECUTION 0
#define DEBUG_SEARCH_LOG_HEAD 0

#define FREE_BUFFER_MARGIN (2 * File_formats::UNDO_PAGE_WORDS)

Lgman::Lgman(Block_context & ctx) :
  SimulatedBlock(LGMAN, ctx),
  m_tup(0),
  m_logfile_group_list(m_logfile_group_pool),
  m_logfile_group_hash(m_logfile_group_pool),
  m_client_mutex("lgman-client", 2, true)
{
  BLOCK_CONSTRUCTOR(Lgman);
  
  // Add received signals
  addRecSignal(GSN_STTOR, &Lgman::execSTTOR);
  addRecSignal(GSN_READ_CONFIG_REQ, &Lgman::execREAD_CONFIG_REQ);
  addRecSignal(GSN_DUMP_STATE_ORD, &Lgman::execDUMP_STATE_ORD);
  addRecSignal(GSN_DBINFO_SCANREQ, &Lgman::execDBINFO_SCANREQ);
  addRecSignal(GSN_CONTINUEB, &Lgman::execCONTINUEB);
  addRecSignal(GSN_NODE_FAILREP, &Lgman::execNODE_FAILREP);

  addRecSignal(GSN_CREATE_FILE_IMPL_REQ, &Lgman::execCREATE_FILE_IMPL_REQ);
  addRecSignal(GSN_CREATE_FILEGROUP_IMPL_REQ,
               &Lgman::execCREATE_FILEGROUP_IMPL_REQ);

  addRecSignal(GSN_DROP_FILE_IMPL_REQ, &Lgman::execDROP_FILE_IMPL_REQ);
  addRecSignal(GSN_DROP_FILEGROUP_IMPL_REQ,
               &Lgman::execDROP_FILEGROUP_IMPL_REQ);

  addRecSignal(GSN_FSWRITEREQ, &Lgman::execFSWRITEREQ);
  addRecSignal(GSN_FSWRITEREF, &Lgman::execFSWRITEREF, true);
  addRecSignal(GSN_FSWRITECONF, &Lgman::execFSWRITECONF);

  addRecSignal(GSN_FSOPENREF, &Lgman::execFSOPENREF, true);
  addRecSignal(GSN_FSOPENCONF, &Lgman::execFSOPENCONF);

  addRecSignal(GSN_FSCLOSECONF, &Lgman::execFSCLOSECONF);
  
  addRecSignal(GSN_FSREADREF, &Lgman::execFSREADREF, true);
  addRecSignal(GSN_FSREADCONF, &Lgman::execFSREADCONF);

  addRecSignal(GSN_LCP_FRAG_ORD, &Lgman::execLCP_FRAG_ORD);
  addRecSignal(GSN_END_LCP_REQ, &Lgman::execEND_LCP_REQ);
  addRecSignal(GSN_SUB_GCP_COMPLETE_REP, &Lgman::execSUB_GCP_COMPLETE_REP);
  addRecSignal(GSN_START_RECREQ, &Lgman::execSTART_RECREQ);
  
  addRecSignal(GSN_END_LCP_CONF, &Lgman::execEND_LCP_CONF);

  addRecSignal(GSN_GET_TABINFOREQ, &Lgman::execGET_TABINFOREQ);

  m_last_lsn = 1;
  m_logfile_group_hash.setSize(10);

  if (isNdbMtLqh()) {
    jam();
    int ret = m_client_mutex.create();
    ndbrequire(ret == 0);
  }

  {
    CallbackEntry& ce = m_callbackEntry[THE_NULL_CALLBACK];
    ce.m_function = TheNULLCallback.m_callbackFunction;
    ce.m_flags = 0;
  }
  {
    CallbackEntry& ce = m_callbackEntry[ENDLCP_CALLBACK];
    ce.m_function = safe_cast(&Lgman::endlcp_callback);
    ce.m_flags = 0;
  }
  {
    CallbackTable& ct = m_callbackTable;
    ct.m_count = COUNT_CALLBACKS;
    ct.m_entry = m_callbackEntry;
    m_callbackTableAddr = &ct;
  }
}
  
Lgman::~Lgman()
{
  if (isNdbMtLqh()) {
    (void)m_client_mutex.destroy();
  }
}

void
Lgman::client_lock(BlockNumber block, int line)
{
  if (isNdbMtLqh()) {
#ifdef VM_TRACE
    Uint32 bno = blockToMain(block);
    Uint32 ino = blockToInstance(block);
#endif
    D("try lock " << bno << "/" << ino << V(line));
    int ret = m_client_mutex.lock();
    ndbrequire(ret == 0);
    D("got lock " << bno << "/" << ino << V(line));
  }
}

void
Lgman::client_unlock(BlockNumber block, int line)
{
  if (isNdbMtLqh()) {
#ifdef VM_TRACE
    Uint32 bno = blockToMain(block);
    Uint32 ino = blockToInstance(block);
#endif
    D("unlock " << bno << "/" << ino << V(line));
    int ret = m_client_mutex.unlock();
    ndbrequire(ret == 0);
  }
}

BLOCK_FUNCTIONS(Lgman)

void 
Lgman::execREAD_CONFIG_REQ(Signal* signal)
{
  jamEntry();

  const ReadConfigReq * req = (ReadConfigReq*)signal->getDataPtr();

  Uint32 ref = req->senderRef;
  Uint32 senderData = req->senderData;

  const ndb_mgm_configuration_iterator * p = 
    m_ctx.m_config.getOwnConfigIterator();
  ndbrequire(p != 0);

  Pool_context pc;
  pc.m_block = this;
  m_log_waiter_pool.wo_pool_init(RT_LGMAN_LOG_WAITER, pc);
  m_file_pool.init(RT_LGMAN_FILE, pc);
  m_logfile_group_pool.init(RT_LGMAN_FILEGROUP, pc);
  // 10 -> 150M
  m_data_buffer_pool.setSize(40);

  ReadConfigConf * conf = (ReadConfigConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(ref, GSN_READ_CONFIG_CONF, signal, 
	     ReadConfigConf::SignalLength, JBB);
}

void
Lgman::execSTTOR(Signal* signal) 
{
  jamEntry();                            
  Uint32 startPhase = signal->theData[1];
  switch (startPhase) {
  case 1:
    m_tup = globalData.getBlock(DBTUP);
    ndbrequire(m_tup != 0);
    break;
  }
  sendSTTORRY(signal);
}

void
Lgman::sendSTTORRY(Signal* signal)
{
  signal->theData[0] = 0;
  signal->theData[3] = 1;
  signal->theData[4] = 2;
  signal->theData[5] = 3;
  signal->theData[6] = 4;
  signal->theData[7] = 5;
  signal->theData[8] = 6;
  signal->theData[9] = 255; // No more start phases from missra
  sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 10, JBB);
}

void
Lgman::execCONTINUEB(Signal* signal){
  jamEntry();

  Uint32 type= signal->theData[0];
  Uint32 ptrI = signal->theData[1];
  client_lock(number(), __LINE__);
  switch(type){
  case LgmanContinueB::FILTER_LOG:
    jam();
    break;
  case LgmanContinueB::CUT_LOG_TAIL:
  {
    jam();
    Ptr<Logfile_group> ptr;
    m_logfile_group_pool.getPtr(ptr, ptrI);
    cut_log_tail(signal, ptr);
    break;
  }
  case LgmanContinueB::FLUSH_LOG:
  {
    jam();
    Ptr<Logfile_group> ptr;
    m_logfile_group_pool.getPtr(ptr, ptrI);
    flush_log(signal, ptr, signal->theData[2]);
    break;
  }
  case LgmanContinueB::PROCESS_LOG_BUFFER_WAITERS:
  {
    jam();
    Ptr<Logfile_group> ptr;
    m_logfile_group_pool.getPtr(ptr, ptrI);
    process_log_buffer_waiters(signal, ptr);
    break;
  }
  case LgmanContinueB::FIND_LOG_HEAD:
    jam();
    Ptr<Logfile_group> ptr;
    if(ptrI != RNIL)
    {
      jam();
      m_logfile_group_pool.getPtr(ptr, ptrI);
      find_log_head(signal, ptr);
    }
    else
    {
      jam();
      init_run_undo_log(signal);
    }
    break;
  case LgmanContinueB::EXECUTE_UNDO_RECORD:
    jam();
    execute_undo_record(signal);
    break;
  case LgmanContinueB::STOP_UNDO_LOG:
    jam();
    stop_run_undo_log(signal);
    break;
  case LgmanContinueB::READ_UNDO_LOG:
  {
    jam();
    Ptr<Logfile_group> ptr;
    m_logfile_group_pool.getPtr(ptr, ptrI);
    read_undo_log(signal, ptr);
    break;
  }
  case LgmanContinueB::PROCESS_LOG_SYNC_WAITERS:
  {
    jam();
    Ptr<Logfile_group> ptr;
    m_logfile_group_pool.getPtr(ptr, ptrI);
    process_log_sync_waiters(signal, ptr);
    break;
  }
  case LgmanContinueB::FORCE_LOG_SYNC:
  {
    jam();
    Ptr<Logfile_group> ptr;
    m_logfile_group_pool.getPtr(ptr, ptrI);
    force_log_sync(signal, ptr, signal->theData[2], signal->theData[3]);
    break;
  }
  case LgmanContinueB::DROP_FILEGROUP:
  {
    jam();
    Ptr<Logfile_group> ptr;
    m_logfile_group_pool.getPtr(ptr, ptrI);
    if ((ptr.p->m_state & Logfile_group::LG_THREAD_MASK) ||
        ptr.p->m_outstanding_fs > 0)
    {
      jam();
      sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 
			  signal->length());
      break;
    }
    Uint32 ref = signal->theData[2];
    Uint32 data = signal->theData[3];
    drop_filegroup_drop_files(signal, ptr, ref, data);
    break;
  }
  }
  client_unlock(number(), __LINE__);
}

void
Lgman::execNODE_FAILREP(Signal* signal)
{
  jamEntry();
  const NodeFailRep * rep = (NodeFailRep*)signal->getDataPtr();
  NdbNodeBitmask failed; 
  failed.assign(NdbNodeBitmask::Size, rep->theNodes);

  /* Block level cleanup */
  for(unsigned i = 1; i < MAX_NDB_NODES; i++) {
    jam();
    if(failed.get(i)) {
      jam();
      Uint32 elementsCleaned = simBlockNodeFailure(signal, i); // No callback
      ndbassert(elementsCleaned == 0); // No distributed fragmented signals
      (void) elementsCleaned; // Remove compiler warning
    }//if
  }//for
}

void
Lgman::execDUMP_STATE_ORD(Signal* signal){
  jamEntry();
  if (signal->theData[0] == 12001 || signal->theData[0] == 12002)
  {
    char tmp[1024];
    Ptr<Logfile_group> ptr;
    m_logfile_group_list.first(ptr);
    while(!ptr.isNull())
    {
      BaseString::snprintf(tmp, sizeof(tmp),
                           "lfg %u state: %x fs: %u lsn "
                           " [ last: %llu s(req): %llu s:ed: %llu lcp: %llu ] "
                           " waiters: %d %d",
                           ptr.p->m_logfile_group_id, ptr.p->m_state,
                           ptr.p->m_outstanding_fs,
                           ptr.p->m_last_lsn, ptr.p->m_last_sync_req_lsn,
                           ptr.p->m_last_synced_lsn, ptr.p->m_last_lcp_lsn,
                           !ptr.p->m_log_buffer_waiters.isEmpty(),
                           !ptr.p->m_log_sync_waiters.isEmpty());
      if (signal->theData[0] == 12001)
        infoEvent("%s", tmp);
      ndbout_c("%s", tmp);

      BaseString::snprintf(tmp, sizeof(tmp),
                           "   callback_buffer_words: %u"
                           " free_buffer_words: %u free_file_words: %llu",
                           ptr.p->m_callback_buffer_words,
                           ptr.p->m_free_buffer_words,
                           ptr.p->m_free_file_words);
      if (signal->theData[0] == 12001)
        infoEvent("%s", tmp);
      ndbout_c("%s", tmp);
      if (!ptr.p->m_log_buffer_waiters.isEmpty())
      {
	Ptr<Log_waiter> waiter;
	Local_log_waiter_list 
	  list(m_log_waiter_pool, ptr.p->m_log_buffer_waiters);
	list.first(waiter);
        BaseString::snprintf(tmp, sizeof(tmp),
                             "  head(waiters).sz: %u %u",
                             waiter.p->m_size,
                             FREE_BUFFER_MARGIN);
        if (signal->theData[0] == 12001)
          infoEvent("%s", tmp);
        ndbout_c("%s", tmp);
      }
      if (!ptr.p->m_log_sync_waiters.isEmpty())
      {
	Ptr<Log_waiter> waiter;
	Local_log_waiter_list 
	  list(m_log_waiter_pool, ptr.p->m_log_sync_waiters);
	list.first(waiter);
        BaseString::snprintf(tmp, sizeof(tmp),
                             "  m_last_synced_lsn: %llu head(waiters %x).m_sync_lsn: %llu",
                             ptr.p->m_last_synced_lsn,
                             waiter.i,
                             waiter.p->m_sync_lsn);
        if (signal->theData[0] == 12001)
          infoEvent("%s", tmp);
        ndbout_c("%s", tmp);
	
	while(!waiter.isNull())
	{
	  ndbout_c("ptr: %x %p lsn: %llu next: %x",
		   waiter.i, waiter.p, waiter.p->m_sync_lsn, waiter.p->nextList);
	  list.next(waiter);
	}
      }
      m_logfile_group_list.next(ptr);
    }
  }
  if (signal->theData[0] == 12003)
  {
    bool crash = false;
    Ptr<Logfile_group> ptr;
    for (m_logfile_group_list.first(ptr); !ptr.isNull();
         m_logfile_group_list.next(ptr))
    {
      if (ptr.p->m_callback_buffer_words != 0)
      {
        crash = true;
        break;
      }
    }

    if (crash)
    {
      ndbout_c("Detected logfile-group with non zero m_callback_buffer_words");
      signal->theData[0] = 12002;
      execDUMP_STATE_ORD(signal);
      ndbrequire(false);
    }
#ifdef VM_TRACE
    else
    {
      ndbout_c("Check for non zero m_callback_buffer_words OK!");
    }
#endif
  }
}

void
Lgman::execDBINFO_SCANREQ(Signal *signal)
{
  DbinfoScanReq req= *(DbinfoScanReq*)signal->theData;
  const Ndbinfo::ScanCursor* cursor = 
    CAST_CONSTPTR(Ndbinfo::ScanCursor, DbinfoScan::getCursorPtr(&req));
  Ndbinfo::Ratelimit rl;

  jamEntry();

  switch(req.tableId) {
  case Ndbinfo::LOGSPACES_TABLEID:
  {
    jam();
    Uint32 startBucket = cursor->data[0];
    Logfile_group_hash_iterator iter;
    m_logfile_group_hash.next(startBucket, iter);

    while (!iter.curr.isNull())
    {
      jam();

      Uint32 currentBucket = iter.bucket;
      Ptr<Logfile_group> ptr = iter.curr;

      Uint64 free = ptr.p->m_free_file_words*4;

      Uint64 total = 0;
      Local_undofile_list list(m_file_pool, ptr.p->m_files);
      Ptr<Undofile> filePtr;
      for (list.first(filePtr); !filePtr.isNull(); list.next(filePtr))
      {
        jam();
        total += (Uint64)filePtr.p->m_file_size *
          (Uint64)File_formats::NDB_PAGE_SIZE;
      }

      Uint64 high = 0; // TODO

      Ndbinfo::Row row(signal, req);
      row.write_uint32(getOwnNodeId());
      row.write_uint32(1); // log type, 1 = DD-UNDO
      row.write_uint32(ptr.p->m_logfile_group_id); // log id
      row.write_uint32(0); // log part

      row.write_uint64(total);          // total allocated
      row.write_uint64((total-free));   // currently in use
      row.write_uint64(high);           // in use high water mark
      ndbinfo_send_row(signal, req, row, rl);

      // move to next
      if (m_logfile_group_hash.next(iter) == false)
      {
        jam(); // no more...
        break;
      }
      else if (iter.bucket == currentBucket)
      {
        jam();
        continue; // we need to iterate an entire bucket
      }
      else if (rl.need_break(req))
      {
        jam();
        ndbinfo_send_scan_break(signal, req, rl, iter.bucket);
        return;
      }
    }
    break;
  }

  case Ndbinfo::LOGBUFFERS_TABLEID:
  {
    jam();
    Uint32 startBucket = cursor->data[0];
    Logfile_group_hash_iterator iter;
    m_logfile_group_hash.next(startBucket, iter);

    while (!iter.curr.isNull())
    {
      jam();

      Uint32 currentBucket = iter.bucket;
      Ptr<Logfile_group> ptr = iter.curr;

      Uint64 free = ptr.p->m_free_buffer_words*4;
      Uint64 total = ptr.p->m_total_buffer_words*4;
      Uint64 high = 0; // TODO

      Ndbinfo::Row row(signal, req);
      row.write_uint32(getOwnNodeId());
      row.write_uint32(1); // log type, 1 = DD-UNDO
      row.write_uint32(ptr.p->m_logfile_group_id); // log id
      row.write_uint32(0); // log part

      row.write_uint64(total);          // total allocated
      row.write_uint64((total-free));   // currently in use
      row.write_uint64(high);           // in use high water mark
      ndbinfo_send_row(signal, req, row, rl);

      // move to next
      if (m_logfile_group_hash.next(iter) == false)
      {
        jam(); // no more...
        break;
      }
      else if (iter.bucket == currentBucket)
      {
        jam();
        continue; // we need to iterate an entire bucket
      }
      else if (rl.need_break(req))
      {
        jam();
        ndbinfo_send_scan_break(signal, req, rl, iter.bucket);
        return;
      }
    }
    break;
  }

  default:
    break;
  }

  ndbinfo_send_scan_conf(signal, req, rl);
}

void
Lgman::execCREATE_FILEGROUP_IMPL_REQ(Signal* signal){
  jamEntry();
  CreateFilegroupImplReq* req= (CreateFilegroupImplReq*)signal->getDataPtr();

  Uint32 senderRef = req->senderRef;
  Uint32 senderData = req->senderData;
  
  Ptr<Logfile_group> ptr;
  CreateFilegroupImplRef::ErrorCode err = CreateFilegroupImplRef::NoError;
  do {
    if (m_logfile_group_hash.find(ptr, req->filegroup_id))
    {
      jam();
      err = CreateFilegroupImplRef::FilegroupAlreadyExists;
      break;
    }
    
    if (!m_logfile_group_list.isEmpty())
    {
      jam();
      err = CreateFilegroupImplRef::OneLogfileGroupLimit;
      break;
    }

    if (!m_logfile_group_pool.seize(ptr))
    {
      jam();
      err = CreateFilegroupImplRef::OutOfFilegroupRecords;
      break;
    }

    new (ptr.p) Logfile_group(req);
    
    if (!alloc_logbuffer_memory(ptr, req->logfile_group.buffer_size))
    {
      jam();
      err= CreateFilegroupImplRef::OutOfLogBufferMemory;
      m_logfile_group_pool.release(ptr);
      break;
    }
    
    m_logfile_group_hash.add(ptr);
    m_logfile_group_list.add(ptr);

    if ((getNodeState().getNodeRestartInProgress() &&
         getNodeState().starting.restartType !=
         NodeState::ST_INITIAL_NODE_RESTART)||
        getNodeState().getSystemRestartInProgress())
    {
      ptr.p->m_state = Logfile_group::LG_STARTING;
    }
    
    CreateFilegroupImplConf* conf= 
      (CreateFilegroupImplConf*)signal->getDataPtr();
    conf->senderData = senderData;
    conf->senderRef = reference();
    sendSignal(senderRef, GSN_CREATE_FILEGROUP_IMPL_CONF, signal,
	       CreateFilegroupImplConf::SignalLength, JBB);
    
    return;
  } while(0);
  
  CreateFilegroupImplRef* ref= (CreateFilegroupImplRef*)signal->getDataPtr();
  ref->senderData = senderData;
  ref->senderRef = reference();
  ref->errorCode = err;
  sendSignal(senderRef, GSN_CREATE_FILEGROUP_IMPL_REF, signal,
	     CreateFilegroupImplRef::SignalLength, JBB);
}

void
Lgman::execDROP_FILEGROUP_IMPL_REQ(Signal* signal)
{
  jamEntry();

  Uint32 errorCode = 0;
  DropFilegroupImplReq req = *(DropFilegroupImplReq*)signal->getDataPtr();  
  do 
  {
    Ptr<Logfile_group> ptr;
    if (!m_logfile_group_hash.find(ptr, req.filegroup_id))
    {
      errorCode = DropFilegroupImplRef::NoSuchFilegroup;
      break;
    }
    
    if (ptr.p->m_version != req.filegroup_version)
    {
      errorCode = DropFilegroupImplRef::InvalidFilegroupVersion;
      break;
    }
    
    switch(req.requestInfo){
    case DropFilegroupImplReq::Prepare:
      break;
    case DropFilegroupImplReq::Commit:
      m_logfile_group_list.remove(ptr);
      ptr.p->m_state |= Logfile_group::LG_DROPPING;
      signal->theData[0] = LgmanContinueB::DROP_FILEGROUP;
      signal->theData[1] = ptr.i;
      signal->theData[2] = req.senderRef;
      signal->theData[3] = req.senderData;
      sendSignal(reference(), GSN_CONTINUEB, signal, 4, JBB);
      return;
    case DropFilegroupImplReq::Abort:
      break;
    default:
      ndbrequire(false);
    }
  } while(0);
  
  if (errorCode)
  {
    DropFilegroupImplRef* ref = 
      (DropFilegroupImplRef*)signal->getDataPtrSend();
    ref->senderRef = reference();
    ref->senderData = req.senderData;
    ref->errorCode = errorCode;
    sendSignal(req.senderRef, GSN_DROP_FILEGROUP_IMPL_REF, signal,
	       DropFilegroupImplRef::SignalLength, JBB);
  }
  else
  {
    DropFilegroupImplConf* conf = 
      (DropFilegroupImplConf*)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = req.senderData;
    sendSignal(req.senderRef, GSN_DROP_FILEGROUP_IMPL_CONF, signal,
	       DropFilegroupImplConf::SignalLength, JBB);
  }
}

void
Lgman::drop_filegroup_drop_files(Signal* signal,
				 Ptr<Logfile_group> ptr,
				 Uint32 ref, Uint32 data)
{
  jam();
  ndbrequire(! (ptr.p->m_state & Logfile_group::LG_THREAD_MASK));
  ndbrequire(ptr.p->m_outstanding_fs == 0);

  Local_undofile_list list(m_file_pool, ptr.p->m_files);
  Ptr<Undofile> file_ptr;

  if (list.first(file_ptr))
  {
    jam();
    ndbrequire(! (file_ptr.p->m_state & Undofile::FS_OUTSTANDING));
    file_ptr.p->m_create.m_senderRef = ref;
    file_ptr.p->m_create.m_senderData = data;
    create_file_abort(signal, ptr, file_ptr);
    return;
  }

  Local_undofile_list metalist(m_file_pool, ptr.p->m_meta_files);
  if (metalist.first(file_ptr))
  {
    jam();
    metalist.remove(file_ptr);
    list.add(file_ptr);
    file_ptr.p->m_create.m_senderRef = ref;
    file_ptr.p->m_create.m_senderData = data;
    create_file_abort(signal, ptr, file_ptr);
    return;
  }

  free_logbuffer_memory(ptr);
  m_logfile_group_hash.release(ptr);
  DropFilegroupImplConf *conf = (DropFilegroupImplConf*)signal->getDataPtr();  
  conf->senderData = data;
  conf->senderRef = reference();
  sendSignal(ref, GSN_DROP_FILEGROUP_IMPL_CONF, signal,
	     DropFilegroupImplConf::SignalLength, JBB);
}

void
Lgman::execCREATE_FILE_IMPL_REQ(Signal* signal)
{
  jamEntry();
  CreateFileImplReq* req= (CreateFileImplReq*)signal->getDataPtr();
  
  Uint32 senderRef = req->senderRef;
  Uint32 senderData = req->senderData;
  Uint32 requestInfo = req->requestInfo;
  
  Ptr<Logfile_group> ptr;
  CreateFileImplRef::ErrorCode err = CreateFileImplRef::NoError;
  SectionHandle handle(this, signal);
  do {
    if (!m_logfile_group_hash.find(ptr, req->filegroup_id))
    {
      jam();
      err = CreateFileImplRef::InvalidFilegroup;
      break;
    }

    if (ptr.p->m_version != req->filegroup_version)
    {
      jam();
      err = CreateFileImplRef::InvalidFilegroupVersion;
      break;
    }

    Ptr<Undofile> file_ptr;
    switch(requestInfo){
    case CreateFileImplReq::Commit:
    {
      jam();
      ndbrequire(find_file_by_id(file_ptr, ptr.p->m_meta_files, req->file_id));
      file_ptr.p->m_create.m_senderRef = req->senderRef;
      file_ptr.p->m_create.m_senderData = req->senderData;
      create_file_commit(signal, ptr, file_ptr);
      return;
    }
    case CreateFileImplReq::Abort:
    {
      Uint32 senderRef = req->senderRef;
      Uint32 senderData = req->senderData;
      if (find_file_by_id(file_ptr, ptr.p->m_meta_files, req->file_id))
      {
        jam();
	file_ptr.p->m_create.m_senderRef = senderRef;
	file_ptr.p->m_create.m_senderData = senderData;
	create_file_abort(signal, ptr, file_ptr);
      }
      else
      {
	CreateFileImplConf* conf= (CreateFileImplConf*)signal->getDataPtr();
        jam();
	conf->senderData = senderData;
	conf->senderRef = reference();
	sendSignal(senderRef, GSN_CREATE_FILE_IMPL_CONF, signal,
		   CreateFileImplConf::SignalLength, JBB);
      }
      return;
    }
    default: // prepare
      break;
    }
    
    if (!m_file_pool.seize(file_ptr))
    {
      jam();
      err = CreateFileImplRef::OutOfFileRecords;
      break;
    }

    if (!handle.m_cnt == 1)
    {
      ndbrequire(false);
    }
    
    if (ERROR_INSERTED(15000) ||
        (sizeof(void*) == 4 && req->file_size_hi & 0xFFFFFFFF))
    {
      jam();
      err = CreateFileImplRef::FileSizeTooLarge;
      break;
    }
    
    Uint64 sz = (Uint64(req->file_size_hi) << 32) + req->file_size_lo;
    if (sz < 1024*1024)
    {
      jam();
      err = CreateFileImplRef::FileSizeTooSmall;
      break;
    }

    new (file_ptr.p) Undofile(req, ptr.i);

    Local_undofile_list tmp(m_file_pool, ptr.p->m_meta_files);
    tmp.add(file_ptr);
    
    open_file(signal, file_ptr, req->requestInfo, &handle);
    return;
  } while(0);

  releaseSections(handle);
  CreateFileImplRef* ref= (CreateFileImplRef*)signal->getDataPtr();
  ref->senderData = senderData;
  ref->senderRef = reference();
  ref->errorCode = err;
  sendSignal(senderRef, GSN_CREATE_FILE_IMPL_REF, signal,
	     CreateFileImplRef::SignalLength, JBB);
}

void
Lgman::open_file(Signal* signal, Ptr<Undofile> ptr,
		 Uint32 requestInfo,
		 SectionHandle * handle)
{
  FsOpenReq* req = (FsOpenReq*)signal->getDataPtrSend();
  req->userReference = reference();
  req->userPointer = ptr.i;
  
  memset(req->fileNumber, 0, sizeof(req->fileNumber));
  FsOpenReq::setVersion(req->fileNumber, 4); // Version 4 = specified filename
  FsOpenReq::v4_setBasePath(req->fileNumber, FsOpenReq::BP_DD_UF);

  req->fileFlags = 0;
  req->fileFlags |= FsOpenReq::OM_READWRITE;
  req->fileFlags |= FsOpenReq::OM_DIRECT;
  req->fileFlags |= FsOpenReq::OM_SYNC;
  switch(requestInfo){
  case CreateFileImplReq::Create:
    req->fileFlags |= FsOpenReq::OM_CREATE_IF_NONE;
    req->fileFlags |= FsOpenReq::OM_INIT;
    ptr.p->m_state = Undofile::FS_CREATING;
    break;
  case CreateFileImplReq::CreateForce:
    req->fileFlags |= FsOpenReq::OM_CREATE;
    req->fileFlags |= FsOpenReq::OM_INIT;
    ptr.p->m_state = Undofile::FS_CREATING;
    break;
  case CreateFileImplReq::Open:
    req->fileFlags |= FsOpenReq::OM_CHECK_SIZE;
    ptr.p->m_state = Undofile::FS_OPENING;
    break;
  default:
    ndbrequire(false);
  }

  req->page_size = File_formats::NDB_PAGE_SIZE;
  Uint64 size = (Uint64)ptr.p->m_file_size * (Uint64)File_formats::NDB_PAGE_SIZE;
  req->file_size_hi = (Uint32)(size >> 32);
  req->file_size_lo = (Uint32)(size & 0xFFFFFFFF);

  sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, FsOpenReq::SignalLength, JBB,
	     handle);
}

void
Lgman::execFSWRITEREQ(Signal* signal)
{
  jamEntry();
  Ptr<Undofile> ptr;
  Ptr<GlobalPage> page_ptr;
  FsReadWriteReq* req= (FsReadWriteReq*)signal->getDataPtr();
  
  m_file_pool.getPtr(ptr, req->userPointer);
  m_shared_page_pool.getPtr(page_ptr, req->data.pageData[0]);

  if (req->varIndex == 0)
  {
    File_formats::Undofile::Zero_page* page = 
      (File_formats::Undofile::Zero_page*)page_ptr.p;
    page->m_page_header.init(File_formats::FT_Undofile, 
			     getOwnNodeId(),
			     ndbGetOwnVersion(),
			     (Uint32)time(0));
    page->m_file_id = ptr.p->m_file_id;
    page->m_logfile_group_id = ptr.p->m_create.m_logfile_group_id;
    page->m_logfile_group_version = ptr.p->m_create.m_logfile_group_version;
    page->m_undo_pages = ptr.p->m_file_size - 1; // minus zero page
  }
  else
  {
    File_formats::Undofile::Undo_page* page = 
      (File_formats::Undofile::Undo_page*)page_ptr.p;
    page->m_page_header.m_page_lsn_hi = 0;
    page->m_page_header.m_page_lsn_lo = 0;
    page->m_page_header.m_page_type = File_formats::PT_Undopage;
    page->m_words_used = 0;
  }
}

void
Lgman::execFSOPENREF(Signal* signal)
{
  jamEntry();

  Ptr<Undofile> ptr;  
  Ptr<Logfile_group> lg_ptr;
  FsRef* ref = (FsRef*)signal->getDataPtr();

  Uint32 errCode = ref->errorCode;
  Uint32 osErrCode = ref->osErrorCode;

  m_file_pool.getPtr(ptr, ref->userPointer);
  m_logfile_group_pool.getPtr(lg_ptr, ptr.p->m_logfile_group_ptr_i);

  {
    CreateFileImplRef* ref= (CreateFileImplRef*)signal->getDataPtr();
    ref->senderData = ptr.p->m_create.m_senderData;
    ref->senderRef = reference();
    ref->errorCode = CreateFileImplRef::FileError;
    ref->fsErrCode = errCode;
    ref->osErrCode = osErrCode;

    sendSignal(ptr.p->m_create.m_senderRef, GSN_CREATE_FILE_IMPL_REF, signal,
	       CreateFileImplRef::SignalLength, JBB);
  }

  Local_undofile_list meta(m_file_pool, lg_ptr.p->m_meta_files);
  meta.release(ptr);
}

#define HEAD 0
#define TAIL 1

void
Lgman::execFSOPENCONF(Signal* signal)
{
  jamEntry();
  Ptr<Undofile> ptr;  

  FsConf* conf = (FsConf*)signal->getDataPtr();
  
  Uint32 fd = conf->filePointer;
  m_file_pool.getPtr(ptr, conf->userPointer);

  ptr.p->m_fd = fd;

  {
    Uint32 senderRef = ptr.p->m_create.m_senderRef;
    Uint32 senderData = ptr.p->m_create.m_senderData;
    
    CreateFileImplConf* conf= (CreateFileImplConf*)signal->getDataPtr();
    conf->senderData = senderData;
    conf->senderRef = reference();
    sendSignal(senderRef, GSN_CREATE_FILE_IMPL_CONF, signal,
	       CreateFileImplConf::SignalLength, JBB);
  }
}

bool 
Lgman::find_file_by_id(Ptr<Undofile>& ptr, 
		       Local_undofile_list::Head& head, Uint32 id)
{
  Local_undofile_list list(m_file_pool, head);
  for(list.first(ptr); !ptr.isNull(); list.next(ptr))
    if(ptr.p->m_file_id == id)
      return true;
  return false;
}

void
Lgman::create_file_commit(Signal* signal, 
			  Ptr<Logfile_group> lg_ptr, 
			  Ptr<Undofile> ptr)
{
  Uint32 senderRef = ptr.p->m_create.m_senderRef;
  Uint32 senderData = ptr.p->m_create.m_senderData;

  bool first= false;
  if(ptr.p->m_state == Undofile::FS_CREATING &&
     (lg_ptr.p->m_state & Logfile_group::LG_ONLINE))
  {
    jam();
    Local_undofile_list free(m_file_pool, lg_ptr.p->m_files);
    Local_undofile_list meta(m_file_pool, lg_ptr.p->m_meta_files);
    first= free.isEmpty();
    meta.remove(ptr);
    if(!first)
    {
      /**
       * Add log file next after current head
       */
      Ptr<Undofile> curr;
      m_file_pool.getPtr(curr, lg_ptr.p->m_file_pos[HEAD].m_ptr_i);
      if(free.next(curr))
	free.insert(ptr, curr); // inserts before (that's why the extra next)
      else
	free.add(ptr);

      ptr.p->m_state = Undofile::FS_ONLINE | Undofile::FS_EMPTY;
    }
    else
    {
      /**
       * First file isn't empty as it can be written to at any time
       */
      free.add(ptr);
      ptr.p->m_state = Undofile::FS_ONLINE;
      lg_ptr.p->m_state |= Logfile_group::LG_FLUSH_THREAD;
      signal->theData[0] = LgmanContinueB::FLUSH_LOG;
      signal->theData[1] = lg_ptr.i;
      signal->theData[2] = 0;
      sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBB);
    }
  }
  else
  {
    ptr.p->m_state = Undofile::FS_SORTING;
  }
  
  ptr.p->m_online.m_lsn = 0;
  ptr.p->m_online.m_outstanding = 0;
  
  Uint64 add= ptr.p->m_file_size - 1;
  lg_ptr.p->m_free_file_words += add * File_formats::UNDO_PAGE_WORDS;

  if(first)
  {
    jam();
    
    Buffer_idx tmp= { ptr.i, 0 };
    lg_ptr.p->m_file_pos[HEAD] = lg_ptr.p->m_file_pos[TAIL] = tmp;
    
    /**
     * Init log tail pointer
     */
    lg_ptr.p->m_tail_pos[0] = tmp;
    lg_ptr.p->m_tail_pos[1] = tmp;
    lg_ptr.p->m_tail_pos[2] = tmp;
    lg_ptr.p->m_next_reply_ptr_i = ptr.i;
  }

  validate_logfile_group(lg_ptr, "create_file_commit");

  CreateFileImplConf* conf= (CreateFileImplConf*)signal->getDataPtr();
  conf->senderData = senderData;
  conf->senderRef = reference();
  sendSignal(senderRef, GSN_CREATE_FILE_IMPL_CONF, signal,
	     CreateFileImplConf::SignalLength, JBB);
}

void
Lgman::create_file_abort(Signal* signal, 
			 Ptr<Logfile_group> lg_ptr, 
			 Ptr<Undofile> ptr)
{
  if (ptr.p->m_fd == RNIL)
  {
    ((FsConf*)signal->getDataPtr())->userPointer = ptr.i;
    execFSCLOSECONF(signal);
    return;
  }

  FsCloseReq *req= (FsCloseReq*)signal->getDataPtrSend();
  req->filePointer = ptr.p->m_fd;
  req->userReference = reference();
  req->userPointer = ptr.i;
  req->fileFlag = 0;
  FsCloseReq::setRemoveFileFlag(req->fileFlag, true);
  
  sendSignal(NDBFS_REF, GSN_FSCLOSEREQ, signal, 
	     FsCloseReq::SignalLength, JBB);
}

void
Lgman::execFSCLOSECONF(Signal* signal)
{
  Ptr<Undofile> ptr;
  Ptr<Logfile_group> lg_ptr;
  Uint32 ptrI = ((FsConf*)signal->getDataPtr())->userPointer;
  m_file_pool.getPtr(ptr, ptrI);
  
  Uint32 senderRef = ptr.p->m_create.m_senderRef;
  Uint32 senderData = ptr.p->m_create.m_senderData;
  
  m_logfile_group_pool.getPtr(lg_ptr, ptr.p->m_logfile_group_ptr_i);

  if (lg_ptr.p->m_state & Logfile_group::LG_DROPPING)
  {
    jam();
    {
      Local_undofile_list list(m_file_pool, lg_ptr.p->m_files);
      list.release(ptr);
    }
    drop_filegroup_drop_files(signal, lg_ptr, senderRef, senderData);
  }
  else
  {
    jam();
    Local_undofile_list list(m_file_pool, lg_ptr.p->m_meta_files);
    list.release(ptr);

    CreateFileImplConf* conf= (CreateFileImplConf*)signal->getDataPtr();
    conf->senderData = senderData;
    conf->senderRef = reference();
    sendSignal(senderRef, GSN_CREATE_FILE_IMPL_CONF, signal,
	       CreateFileImplConf::SignalLength, JBB);
  }
}

void
Lgman::execDROP_FILE_IMPL_REQ(Signal* signal)
{
  jamEntry();
  ndbrequire(false);
}

#define CONSUMER 0
#define PRODUCER 1

Lgman::Logfile_group::Logfile_group(const CreateFilegroupImplReq* req)
{
  m_logfile_group_id = req->filegroup_id;
  m_version = req->filegroup_version;
  m_state = LG_ONLINE;
  m_outstanding_fs = 0;
  m_next_reply_ptr_i = RNIL;
  
  m_last_lsn = 0;
  m_last_synced_lsn = 0;
  m_last_sync_req_lsn = 0;
  m_max_sync_req_lsn = 0;
  m_last_read_lsn = 0;
  m_file_pos[0].m_ptr_i= m_file_pos[1].m_ptr_i = RNIL;

  m_free_file_words = 0;
  m_total_buffer_words = 0;
  m_free_buffer_words = 0;
  m_callback_buffer_words = 0;

  m_pos[CONSUMER].m_current_page.m_ptr_i = RNIL;// { m_buffer_pages, idx }
  m_pos[CONSUMER].m_current_pos.m_ptr_i = RNIL; // { page ptr.i, m_words_used}
  m_pos[PRODUCER].m_current_page.m_ptr_i = RNIL;// { m_buffer_pages, idx }
  m_pos[PRODUCER].m_current_pos.m_ptr_i = RNIL; // { page ptr.i, m_words_used}

  m_tail_pos[2].m_ptr_i= RNIL;
  m_tail_pos[2].m_idx= ~0;
  
  m_tail_pos[0] = m_tail_pos[1] = m_tail_pos[2];
}

bool
Lgman::alloc_logbuffer_memory(Ptr<Logfile_group> ptr, Uint32 bytes)
{
  Uint32 pages= (((bytes + 3) >> 2) + File_formats::NDB_PAGE_SIZE_WORDS - 1)
    / File_formats::NDB_PAGE_SIZE_WORDS;
  Uint32 requested= pages;
  {
    Page_map map(m_data_buffer_pool, ptr.p->m_buffer_pages);
    while(pages)
    {
      Uint32 ptrI;
      Uint32 cnt = pages > 64 ? 64 : pages;
      m_ctx.m_mm.alloc_pages(RG_DISK_OPERATIONS, &ptrI, &cnt, 1);
      if (cnt)
      {
	Buffer_idx range;
	range.m_ptr_i= ptrI;
	range.m_idx = cnt;
        
	if (map.append((Uint32*)&range, 2) == false)
        {
          /**
           * Failed to append page-range...
           *   jump out of alloc routine
           */
          jam();
          m_ctx.m_mm.release_pages(RG_DISK_OPERATIONS, 
                                   range.m_ptr_i, range.m_idx);
          break;
        }
	pages -= range.m_idx;
      }
      else
      {
	break;
      }
    }
  }

  if(pages)
  {
    /* Could not allocate all of the requested memory.
     * So release that already allocated.
     */
    free_logbuffer_memory(ptr);
    return false;
  }
  
#if defined VM_TRACE || defined ERROR_INSERT
  ndbout << "DD lgman: fg id:" << ptr.p->m_logfile_group_id << " undo buffer pages/bytes:" << (requested-pages) << "/" << (requested-pages)*File_formats::NDB_PAGE_SIZE << endl;
#endif
  
  init_logbuffer_pointers(ptr);
  return true;
}

void
Lgman::init_logbuffer_pointers(Ptr<Logfile_group> ptr)
{
  Page_map map(m_data_buffer_pool, ptr.p->m_buffer_pages);
  Page_map::Iterator it;
  union {
    Uint32 tmp[2];
    Buffer_idx range;
  };
  
  map.first(it);
  tmp[0] = *it.data;
  ndbrequire(map.next(it));
  tmp[1] = *it.data;
  
  ptr.p->m_pos[CONSUMER].m_current_page.m_ptr_i = 0;      // Index in page map
  ptr.p->m_pos[CONSUMER].m_current_page.m_idx = range.m_idx - 1;// left range
  ptr.p->m_pos[CONSUMER].m_current_pos.m_ptr_i = range.m_ptr_i; // Which page
  ptr.p->m_pos[CONSUMER].m_current_pos.m_idx = 0;               // Page pos
  
  ptr.p->m_pos[PRODUCER].m_current_page.m_ptr_i = 0;      // Index in page map
  ptr.p->m_pos[PRODUCER].m_current_page.m_idx = range.m_idx - 1;// left range
  ptr.p->m_pos[PRODUCER].m_current_pos.m_ptr_i = range.m_ptr_i; // Which page
  ptr.p->m_pos[PRODUCER].m_current_pos.m_idx = 0;               // Page pos

  Uint32 pages= range.m_idx;
  while(map.next(it))
  {
    tmp[0] = *it.data;
    ndbrequire(map.next(it));
    tmp[1] = *it.data;
    pages += range.m_idx;
  }
  
  ptr.p->m_total_buffer_words =
    ptr.p->m_free_buffer_words = pages * File_formats::UNDO_PAGE_WORDS;
}

Uint32
Lgman::compute_free_file_pages(Ptr<Logfile_group> ptr)
{
  Buffer_idx head= ptr.p->m_file_pos[HEAD];
  Buffer_idx tail= ptr.p->m_file_pos[TAIL];
  Uint32 pages = 0;
  if (head.m_ptr_i == tail.m_ptr_i && head.m_idx < tail.m_idx)
  {
    pages += tail.m_idx - head.m_idx;
  }
  else
  {
    Ptr<Undofile> file;
    m_file_pool.getPtr(file, head.m_ptr_i);
    Local_undofile_list list(m_file_pool, ptr.p->m_files);
    
    do 
    {
      pages += (file.p->m_file_size - head.m_idx - 1);
      if(!list.next(file))
	list.first(file);
      head.m_idx = 0;
    } while(file.i != tail.m_ptr_i);
    
    pages += tail.m_idx - head.m_idx;
  }
  return pages;
}

void
Lgman::free_logbuffer_memory(Ptr<Logfile_group> ptr)
{
  union {
    Uint32 tmp[2];
    Buffer_idx range;
  };

  Page_map map(m_data_buffer_pool, ptr.p->m_buffer_pages);

  Page_map::Iterator it;
  map.first(it);
  while(!it.isNull())
  {
    tmp[0] = *it.data;
    ndbrequire(map.next(it));
    tmp[1] = *it.data;
    
    m_ctx.m_mm.release_pages(RG_DISK_OPERATIONS, range.m_ptr_i, range.m_idx);
    map.next(it);
  }
  map.release();
}

Lgman::Undofile::Undofile(const struct CreateFileImplReq* req, Uint32 ptrI)
{
  m_fd = RNIL;
  m_file_id = req->file_id;
  m_logfile_group_ptr_i= ptrI;
  
  Uint64 pages = req->file_size_hi;
  pages = (pages << 32) | req->file_size_lo;
  pages /= GLOBAL_PAGE_SIZE;
  m_file_size = Uint32(pages);
#if defined VM_TRACE || defined ERROR_INSERT
  ndbout << "DD lgman: file id:" << m_file_id << " undofile pages/bytes:" << m_file_size << "/" << m_file_size*GLOBAL_PAGE_SIZE << endl;
#endif

  m_create.m_senderRef = req->senderRef; // During META
  m_create.m_senderData = req->senderData; // During META
  m_create.m_logfile_group_id = req->filegroup_id;
}

Logfile_client::Logfile_client(SimulatedBlock* block, 
			       Lgman* lgman, Uint32 logfile_group_id,
                               bool lock)
{
  Uint32 bno = block->number();
  Uint32 ino = block->instance();
  m_client_block= block;
  m_block= numberToBlock(bno, ino);
  m_lgman= lgman;
  m_lock = lock;
  m_logfile_group_id= logfile_group_id;
  D("client ctor " << bno << "/" << ino);
  if (m_lock)
    m_lgman->client_lock(m_block, 0);
}

Logfile_client::~Logfile_client()
{
#ifdef VM_TRACE
  Uint32 bno = blockToMain(m_block);
  Uint32 ino = blockToInstance(m_block);
#endif
  D("client dtor " << bno << "/" << ino);
  if (m_lock)
    m_lgman->client_unlock(m_block, 0);
}

int
Logfile_client::sync_lsn(Signal* signal, 
			 Uint64 lsn, Request* req, Uint32 flags)
{
  Ptr<Lgman::Logfile_group> ptr;
  if(m_lgman->m_logfile_group_list.first(ptr))
  {
    if(ptr.p->m_last_synced_lsn >= lsn)
    {
      return 1;
    }
    
    bool empty= false;
    Ptr<Lgman::Log_waiter> wait;
    {
      Lgman::Local_log_waiter_list
	list(m_lgman->m_log_waiter_pool, ptr.p->m_log_sync_waiters);
      
      empty= list.isEmpty();
      if(!list.seize(wait))
	return -1;
      
      wait.p->m_block= m_block;
      wait.p->m_sync_lsn= lsn;
      memcpy(&wait.p->m_callback, &req->m_callback, 
	     sizeof(SimulatedBlock::CallbackPtr));

      ptr.p->m_max_sync_req_lsn = lsn > ptr.p->m_max_sync_req_lsn ?
	lsn : ptr.p->m_max_sync_req_lsn;
    }
    
    if(ptr.p->m_last_sync_req_lsn < lsn && 
       ! (ptr.p->m_state & Lgman::Logfile_group::LG_FORCE_SYNC_THREAD))
    { 
      ptr.p->m_state |= Lgman::Logfile_group::LG_FORCE_SYNC_THREAD;
      signal->theData[0] = LgmanContinueB::FORCE_LOG_SYNC;
      signal->theData[1] = ptr.i;
      signal->theData[2] = (Uint32)(lsn >> 32);
      signal->theData[3] = (Uint32)(lsn & 0xFFFFFFFF);
      m_client_block->sendSignalWithDelay(m_lgman->reference(), 
                                          GSN_CONTINUEB, signal, 10, 4);
    }
    return 0;
  }
  return -1;
}

void
Lgman::force_log_sync(Signal* signal, 
		      Ptr<Logfile_group> ptr, 
		      Uint32 lsn_hi, Uint32 lsn_lo)
{
  Local_log_waiter_list list(m_log_waiter_pool, ptr.p->m_log_sync_waiters);
  Uint64 force_lsn = lsn_hi; force_lsn <<= 32; force_lsn += lsn_lo;

  if(ptr.p->m_last_sync_req_lsn < force_lsn)
  {
    /**
     * Do force
     */
    Buffer_idx pos= ptr.p->m_pos[PRODUCER].m_current_pos;
    GlobalPage *page = m_shared_page_pool.getPtr(pos.m_ptr_i);
  
    Uint32 free= File_formats::UNDO_PAGE_WORDS - pos.m_idx;
    if(pos.m_idx) // don't flush empty page...
    {
      Uint64 lsn= ptr.p->m_last_lsn - 1;
      
      File_formats::Undofile::Undo_page* undo= 
	(File_formats::Undofile::Undo_page*)page;
      undo->m_page_header.m_page_lsn_lo = (Uint32)(lsn & 0xFFFFFFFF);
      undo->m_page_header.m_page_lsn_hi = (Uint32)(lsn >> 32);
      undo->m_words_used= File_formats::UNDO_PAGE_WORDS - free;
      
      /**
       * Update free space with extra NOOP
       */
      ndbrequire(ptr.p->m_free_file_words >= free);
      ndbrequire(ptr.p->m_free_buffer_words > free);
      ptr.p->m_free_file_words -= free;
      ptr.p->m_free_buffer_words -= free;
      
      validate_logfile_group(ptr, "force_log_sync");

      next_page(ptr.p, PRODUCER);
      ptr.p->m_pos[PRODUCER].m_current_pos.m_idx = 0;
    }
  }

  
  
  Uint64 max_req_lsn = ptr.p->m_max_sync_req_lsn;
  if(max_req_lsn > force_lsn && 
     max_req_lsn > ptr.p->m_last_sync_req_lsn)
  {
    ndbrequire(ptr.p->m_state & Lgman::Logfile_group::LG_FORCE_SYNC_THREAD);
    signal->theData[0] = LgmanContinueB::FORCE_LOG_SYNC;
    signal->theData[1] = ptr.i;
    signal->theData[2] = (Uint32)(max_req_lsn >> 32);
    signal->theData[3] = (Uint32)(max_req_lsn & 0xFFFFFFFF);
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 10, 4);
  }
  else
  {
    ptr.p->m_state &= ~(Uint32)Lgman::Logfile_group::LG_FORCE_SYNC_THREAD;
  }
}

void
Lgman::process_log_sync_waiters(Signal* signal, Ptr<Logfile_group> ptr)
{
  Local_log_waiter_list 
    list(m_log_waiter_pool, ptr.p->m_log_sync_waiters);

  if(list.isEmpty())
  {
    return;
  }

  bool removed= false;
  Ptr<Log_waiter> waiter;
  list.first(waiter);
  Uint32 logfile_group_id = ptr.p->m_logfile_group_id;

  if(waiter.p->m_sync_lsn <= ptr.p->m_last_synced_lsn)
  {
    removed= true;
    Uint32 block = waiter.p->m_block;
    CallbackPtr & callback = waiter.p->m_callback;
    sendCallbackConf(signal, block, callback, logfile_group_id);
    
    list.releaseFirst(waiter);
  }
  
  if(removed && !list.isEmpty())
  {
    ptr.p->m_state |= Logfile_group::LG_SYNC_WAITERS_THREAD;
    signal->theData[0] = LgmanContinueB::PROCESS_LOG_SYNC_WAITERS;
    signal->theData[1] = ptr.i;
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
  }
  else
  {
    ptr.p->m_state &= ~(Uint32)Logfile_group::LG_SYNC_WAITERS_THREAD;
  }
}


Uint32*
Lgman::get_log_buffer(Ptr<Logfile_group> ptr, Uint32 sz)
{
  GlobalPage *page;
  page=m_shared_page_pool.getPtr(ptr.p->m_pos[PRODUCER].m_current_pos.m_ptr_i);
  
  Uint32 total_free= ptr.p->m_free_buffer_words;
  assert(total_free >= sz);
  Uint32 pos= ptr.p->m_pos[PRODUCER].m_current_pos.m_idx;
  Uint32 free= File_formats::UNDO_PAGE_WORDS - pos;

  if(sz <= free)
  {
next:
    // fits this page wo/ problem
    ndbrequire(total_free >= sz);
    ptr.p->m_free_buffer_words = total_free - sz;
    ptr.p->m_pos[PRODUCER].m_current_pos.m_idx = pos + sz;
    return ((File_formats::Undofile::Undo_page*)page)->m_data + pos;
  }
  
  /**
   * It didn't fit page...fill page with a NOOP log entry
   */
  Uint64 lsn= ptr.p->m_last_lsn - 1;
  File_formats::Undofile::Undo_page* undo= 
    (File_formats::Undofile::Undo_page*)page;
  undo->m_page_header.m_page_lsn_lo = (Uint32)(lsn & 0xFFFFFFFF);
  undo->m_page_header.m_page_lsn_hi = (Uint32)(lsn >> 32);
  undo->m_words_used= File_formats::UNDO_PAGE_WORDS - free;
  
  /**
   * Update free space with extra NOOP
   */
  ndbrequire(ptr.p->m_free_file_words >= free);
  ptr.p->m_free_file_words -= free;

  validate_logfile_group(ptr, "get_log_buffer");
  
  pos= 0;
  assert(total_free >= free);
  total_free -= free;
  page= m_shared_page_pool.getPtr(next_page(ptr.p, PRODUCER));
  goto next;
}

Uint32 
Lgman::next_page(Logfile_group* ptrP, Uint32 i)
{
  Uint32 page_ptr_i= ptrP->m_pos[i].m_current_pos.m_ptr_i;
  Uint32 left_in_range= ptrP->m_pos[i].m_current_page.m_idx;
  if(left_in_range > 0)
  {
    ptrP->m_pos[i].m_current_page.m_idx = left_in_range - 1;
    ptrP->m_pos[i].m_current_pos.m_ptr_i = page_ptr_i + 1;
    return page_ptr_i + 1;
  }
  else
  {
    Lgman::Page_map map(m_data_buffer_pool, ptrP->m_buffer_pages);
    Uint32 pos= (ptrP->m_pos[i].m_current_page.m_ptr_i + 2) % map.getSize();
    Lgman::Page_map::Iterator it;
    map.position(it, pos);

    union {
      Uint32 tmp[2];
      Lgman::Buffer_idx range;
    };
    
    tmp[0] = *it.data; map.next(it);
    tmp[1] = *it.data;
    
    ptrP->m_pos[i].m_current_page.m_ptr_i = pos;           // New index in map
    ptrP->m_pos[i].m_current_page.m_idx = range.m_idx - 1; // Free pages 
    ptrP->m_pos[i].m_current_pos.m_ptr_i = range.m_ptr_i;  // Current page
    // No need to set ptrP->m_current_pos.m_idx, that is set "in higher"-func
    return range.m_ptr_i;
  }
}

int
Logfile_client::get_log_buffer(Signal* signal, Uint32 sz, 
			       SimulatedBlock::CallbackPtr* callback)
{
  sz += 2; // lsn
  Lgman::Logfile_group key;
  key.m_logfile_group_id= m_logfile_group_id;
  Ptr<Lgman::Logfile_group> ptr;
  if(m_lgman->m_logfile_group_hash.find(ptr, key))
  {
    Uint32 callback_buffer = ptr.p->m_callback_buffer_words;
    Uint32 free_buffer = ptr.p->m_free_buffer_words;
    if (free_buffer >= (sz + callback_buffer + FREE_BUFFER_MARGIN) &&
        ptr.p->m_log_buffer_waiters.isEmpty())
    {
      ptr.p->m_callback_buffer_words = callback_buffer + sz;
      return 1;
    }
    
    bool empty= false;
    {
      Ptr<Lgman::Log_waiter> wait;
      Lgman::Local_log_waiter_list
	list(m_lgman->m_log_waiter_pool, ptr.p->m_log_buffer_waiters);
      
      empty= list.isEmpty();
      if(!list.seize(wait))
      {
	return -1;
      }      

      wait.p->m_size= sz;
      wait.p->m_block= m_block;
      memcpy(&wait.p->m_callback, callback,sizeof(SimulatedBlock::CallbackPtr));
    }

    return 0;
  }
  return -1;
}

NdbOut&
operator<<(NdbOut& out, const Lgman::Buffer_idx& pos)
{
  out << "[ " 
      << pos.m_ptr_i << " "
      << pos.m_idx << " ]";
  return out;
}

NdbOut&
operator<<(NdbOut& out, const Lgman::Logfile_group::Position& pos)
{
  out << "[ (" 
      << pos.m_current_page.m_ptr_i << " "
      << pos.m_current_page.m_idx << ") ("
      << pos.m_current_pos.m_ptr_i << " "
      << pos.m_current_pos.m_idx << ") ]";
  return out;
}

void
Lgman::flush_log(Signal* signal, Ptr<Logfile_group> ptr, Uint32 force)
{
  Logfile_group::Position consumer= ptr.p->m_pos[CONSUMER];
  Logfile_group::Position producer= ptr.p->m_pos[PRODUCER];
 
  jamEntry();

  if (consumer.m_current_page == producer.m_current_page)
  {
    jam();
    Buffer_idx pos = producer.m_current_pos;

#if 0
    if (force)
    {
      ndbout_c("force: %d ptr.p->m_file_pos[HEAD].m_ptr_i= %x", 
	       force, ptr.p->m_file_pos[HEAD].m_ptr_i);
      ndbout_c("consumer.m_current_page: %d %d producer.m_current_page: %d %d",
	       consumer.m_current_page.m_ptr_i, consumer.m_current_page.m_idx,
	       producer.m_current_page.m_ptr_i, producer.m_current_page.m_idx);
    }
#endif
    if (! (ptr.p->m_state & Logfile_group::LG_DROPPING))
    {
      jam();

      if (ptr.p->m_log_buffer_waiters.isEmpty() || pos.m_idx == 0)
      {
        jam();
	force =  0;
      }
      else if (ptr.p->m_free_buffer_words < FREE_BUFFER_MARGIN)
      {
        jam();
        force = 2;
      }

      if (force < 2 || ptr.p->m_outstanding_fs)
      {
        jam();
	signal->theData[0] = LgmanContinueB::FLUSH_LOG;
	signal->theData[1] = ptr.i;
	signal->theData[2] = force + 1;
	sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 
			    force ? 10 : 100, 3);
	return;
      }
      else
      {
        jam();
	GlobalPage *page = m_shared_page_pool.getPtr(pos.m_ptr_i);
	
	Uint32 free= File_formats::UNDO_PAGE_WORDS - pos.m_idx;

	ndbout_c("force flush %d %d outstanding: %u isEmpty(): %u",
                 pos.m_idx, ptr.p->m_free_buffer_words,
                 ptr.p->m_outstanding_fs,
                 ptr.p->m_log_buffer_waiters.isEmpty());
	
	ndbrequire(pos.m_idx); // don't flush empty page...
	Uint64 lsn= ptr.p->m_last_lsn - 1;
	
	File_formats::Undofile::Undo_page* undo= 
	  (File_formats::Undofile::Undo_page*)page;
	undo->m_page_header.m_page_lsn_lo = (Uint32)(lsn & 0xFFFFFFFF);
	undo->m_page_header.m_page_lsn_hi = (Uint32)(lsn >> 32);
	undo->m_words_used= File_formats::UNDO_PAGE_WORDS - free;
	
	/**
	 * Update free space with extra NOOP
	 */
	ndbrequire(ptr.p->m_free_file_words >= free);
	ndbrequire(ptr.p->m_free_buffer_words > free);
	ptr.p->m_free_file_words -= free;
	ptr.p->m_free_buffer_words -= free;
         
	validate_logfile_group(ptr, "force_log_flush");
	
	next_page(ptr.p, PRODUCER);
	ptr.p->m_pos[PRODUCER].m_current_pos.m_idx = 0;
	producer = ptr.p->m_pos[PRODUCER];
	// break through
      }
    }
    else
    {
      jam();
      ptr.p->m_state &= ~(Uint32)Logfile_group::LG_FLUSH_THREAD;
      return;
    }
  }
  
  bool full= false;
  Uint32 tot= 0;
  while(!(consumer.m_current_page == producer.m_current_page) && !full)
  {
    jam();
    validate_logfile_group(ptr, "before flush log");

    Uint32 cnt; // pages written
    Uint32 page= consumer.m_current_pos.m_ptr_i;
    if(consumer.m_current_page.m_ptr_i == producer.m_current_page.m_ptr_i)
    {
      /**
       * In same range
       */
      jam();

      if(producer.m_current_pos.m_ptr_i > page)
      {
        /**
         * producer ahead of consumer in same chunk
         */
	jam();
	Uint32 tmp= producer.m_current_pos.m_ptr_i - page;
	cnt= write_log_pages(signal, ptr, page, tmp);
	assert(cnt <= tmp);
	
	consumer.m_current_pos.m_ptr_i += cnt;
	consumer.m_current_page.m_idx -= cnt;
	full= (tmp > cnt);
      }
      else
      {
        /**
         * consumer ahead of producer in same chunk
         */
	Uint32 tmp= consumer.m_current_page.m_idx + 1;
	cnt= write_log_pages(signal, ptr, page, tmp);
	assert(cnt <= tmp);

	if(cnt == tmp)
	{
	  jam();
	  /**
	   * Entire chunk is written
	   *   move to next
	   */
	  ptr.p->m_pos[CONSUMER].m_current_page.m_idx= 0;
	  next_page(ptr.p, CONSUMER);
	  consumer = ptr.p->m_pos[CONSUMER];
 	}
	else
	{
	  jam();
	  /**
	   * Failed to write entire chunk...
	   */
	  full= true;
	  consumer.m_current_page.m_idx -= cnt;
	  consumer.m_current_pos.m_ptr_i += cnt;
	}
      }
    }
    else
    {
      Uint32 tmp= consumer.m_current_page.m_idx + 1;
      cnt= write_log_pages(signal, ptr, page, tmp);
      assert(cnt <= tmp);

      if(cnt == tmp)
      {
	jam();
	/**
	 * Entire chunk is written
	 *   move to next
	 */
	ptr.p->m_pos[CONSUMER].m_current_page.m_idx= 0;
	next_page(ptr.p, CONSUMER);
	consumer = ptr.p->m_pos[CONSUMER];
      }
      else
      {
	jam();
	/**
	 * Failed to write entire chunk...
	 */
	full= true;
	consumer.m_current_page.m_idx -= cnt;
	consumer.m_current_pos.m_ptr_i += cnt;
      }
    }

    tot += cnt;
    if(cnt)
      validate_logfile_group(ptr, " after flush_log");
  }

  ptr.p->m_pos[CONSUMER]= consumer;
  
  if (! (ptr.p->m_state & Logfile_group::LG_DROPPING))
  {
    signal->theData[0] = LgmanContinueB::FLUSH_LOG;
    signal->theData[1] = ptr.i;
    signal->theData[2] = 0;
    sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBB);
  }
  else
  {
    ptr.p->m_state &= ~(Uint32)Logfile_group::LG_FLUSH_THREAD;
  }
}

void
Lgman::process_log_buffer_waiters(Signal* signal, Ptr<Logfile_group> ptr)
{
  Uint32 free_buffer= ptr.p->m_free_buffer_words;
  Uint32 callback_buffer = ptr.p->m_callback_buffer_words;
  Local_log_waiter_list 
    list(m_log_waiter_pool, ptr.p->m_log_buffer_waiters);

  if (list.isEmpty())
  {
    jam();
    ptr.p->m_state &= ~(Uint32)Logfile_group::LG_WAITERS_THREAD;
    return;
  }
  
  bool removed= false;
  Ptr<Log_waiter> waiter;
  list.first(waiter);
  Uint32 sz  = waiter.p->m_size;
  Uint32 logfile_group_id = ptr.p->m_logfile_group_id;
  if (sz + callback_buffer + FREE_BUFFER_MARGIN < free_buffer)
  {
    jam();
    removed= true;
    Uint32 block = waiter.p->m_block;
    CallbackPtr & callback = waiter.p->m_callback;
    ptr.p->m_callback_buffer_words += sz;
    sendCallbackConf(signal, block, callback, logfile_group_id);

    list.releaseFirst(waiter);
  }
  
  if (removed && !list.isEmpty())
  {
    jam();
    ptr.p->m_state |= Logfile_group::LG_WAITERS_THREAD;
    signal->theData[0] = LgmanContinueB::PROCESS_LOG_BUFFER_WAITERS;
    signal->theData[1] = ptr.i;
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
  }
  else
  {
    jam();
    ptr.p->m_state &= ~(Uint32)Logfile_group::LG_WAITERS_THREAD;
  }
}

#define REALLY_SLOW_FS 0

Uint32
Lgman::write_log_pages(Signal* signal, Ptr<Logfile_group> ptr,
		       Uint32 pageId, Uint32 in_pages)
{
  assert(in_pages);
  Ptr<Undofile> filePtr;
  Buffer_idx head= ptr.p->m_file_pos[HEAD];
  Buffer_idx tail= ptr.p->m_file_pos[TAIL];
  m_file_pool.getPtr(filePtr, head.m_ptr_i);
  
  if(filePtr.p->m_online.m_outstanding > 0)
  {
    jam();
    return 0;
  }
  
  Uint32 sz= filePtr.p->m_file_size - 1; // skip zero
  Uint32 max, pages= in_pages;

  if(!(head.m_ptr_i == tail.m_ptr_i && head.m_idx < tail.m_idx))
  {
    max= sz - head.m_idx;
  }
  else
  {
    max= tail.m_idx - head.m_idx;
  }

  FsReadWriteReq* req= (FsReadWriteReq*)signal->getDataPtrSend();
  req->filePointer = filePtr.p->m_fd;
  req->userReference = reference();
  req->userPointer = filePtr.i;
  req->varIndex = 1+head.m_idx; // skip zero page
  req->numberOfPages = pages;
  req->data.pageData[0] = pageId;
  req->operationFlag = 0;
  FsReadWriteReq::setFormatFlag(req->operationFlag, 
				FsReadWriteReq::fsFormatSharedPage);

  if(max > pages)
  {
    jam();
    max= pages;
    head.m_idx += max;
    ptr.p->m_file_pos[HEAD] = head;  

    if (REALLY_SLOW_FS)
      sendSignalWithDelay(NDBFS_REF, GSN_FSWRITEREQ, signal, REALLY_SLOW_FS,
			  FsReadWriteReq::FixedLength + 1);
    else
      sendSignal(NDBFS_REF, GSN_FSWRITEREQ, signal, 
		 FsReadWriteReq::FixedLength + 1, JBA);

    ptr.p->m_outstanding_fs++;
    filePtr.p->m_online.m_outstanding = max;
    filePtr.p->m_state |= Undofile::FS_OUTSTANDING;

    File_formats::Undofile::Undo_page *page= (File_formats::Undofile::Undo_page*)
      m_shared_page_pool.getPtr(pageId + max - 1);
    Uint64 lsn = 0;
    lsn += page->m_page_header.m_page_lsn_hi; lsn <<= 32;
    lsn += page->m_page_header.m_page_lsn_lo;
    
    filePtr.p->m_online.m_lsn = lsn;  // Store last writereq lsn on file
    ptr.p->m_last_sync_req_lsn = lsn; // And logfile_group
  }
  else
  {
    jam();
    req->numberOfPages = max;
    FsReadWriteReq::setSyncFlag(req->operationFlag, 1);
    
    if (REALLY_SLOW_FS)
      sendSignalWithDelay(NDBFS_REF, GSN_FSWRITEREQ, signal, REALLY_SLOW_FS, 
			  FsReadWriteReq::FixedLength + 1);
    else
      sendSignal(NDBFS_REF, GSN_FSWRITEREQ, signal, 
		 FsReadWriteReq::FixedLength + 1, JBA);

    ptr.p->m_outstanding_fs++;
    filePtr.p->m_online.m_outstanding = max;
    filePtr.p->m_state |= Undofile::FS_OUTSTANDING;

    File_formats::Undofile::Undo_page *page= (File_formats::Undofile::Undo_page*)
      m_shared_page_pool.getPtr(pageId + max - 1);
    Uint64 lsn = 0;
    lsn += page->m_page_header.m_page_lsn_hi; lsn <<= 32;
    lsn += page->m_page_header.m_page_lsn_lo;
    
    filePtr.p->m_online.m_lsn = lsn;  // Store last writereq lsn on file
    ptr.p->m_last_sync_req_lsn = lsn; // And logfile_group
    
    Ptr<Undofile> next = filePtr;
    Local_undofile_list files(m_file_pool, ptr.p->m_files);
    if(!files.next(next))
    {
      jam();
      files.first(next);
    }
    ndbout_c("changing file from %d to %d", filePtr.i, next.i);
    filePtr.p->m_state |= Undofile::FS_MOVE_NEXT;
    next.p->m_state &= ~(Uint32)Undofile::FS_EMPTY;

    head.m_idx= 0;
    head.m_ptr_i= next.i;
    ptr.p->m_file_pos[HEAD] = head;
    if(max < pages)
      max += write_log_pages(signal, ptr, pageId + max, pages - max);
  }
  
  assert(max);
  return max;
}

void
Lgman::execFSWRITEREF(Signal* signal)
{
  jamEntry();
  SimulatedBlock::execFSWRITEREF(signal);
  ndbrequire(false);
}

void
Lgman::execFSWRITECONF(Signal* signal)
{
  jamEntry();
  client_lock(number(), __LINE__);
  FsConf * conf = (FsConf*)signal->getDataPtr();
  Ptr<Undofile> ptr;
  m_file_pool.getPtr(ptr, conf->userPointer);

  ndbrequire(ptr.p->m_state & Undofile::FS_OUTSTANDING);
  ptr.p->m_state &= ~(Uint32)Undofile::FS_OUTSTANDING;

  Ptr<Logfile_group> lg_ptr;
  m_logfile_group_pool.getPtr(lg_ptr, ptr.p->m_logfile_group_ptr_i);
  
  Uint32 cnt= lg_ptr.p->m_outstanding_fs;
  ndbrequire(cnt);
  
  if(lg_ptr.p->m_next_reply_ptr_i == ptr.i)
  {
    Uint32 tot= 0;
    Uint64 lsn = 0;
    {
      Local_undofile_list files(m_file_pool, lg_ptr.p->m_files);
      while(cnt && ! (ptr.p->m_state & Undofile::FS_OUTSTANDING))
      {
	Uint32 state= ptr.p->m_state;
	Uint32 pages= ptr.p->m_online.m_outstanding;
	ndbrequire(pages);
	ptr.p->m_online.m_outstanding= 0;
	ptr.p->m_state &= ~(Uint32)Undofile::FS_MOVE_NEXT;
	tot += pages;
	cnt--;
	
	lsn = ptr.p->m_online.m_lsn;
	
	if((state & Undofile::FS_MOVE_NEXT) && !files.next(ptr))
	  files.first(ptr);
      }
    }
    
    ndbassert(tot);
    lg_ptr.p->m_outstanding_fs = cnt;
    lg_ptr.p->m_free_buffer_words += (tot * File_formats::UNDO_PAGE_WORDS);
    lg_ptr.p->m_next_reply_ptr_i = ptr.i;
    lg_ptr.p->m_last_synced_lsn = lsn;

    if(! (lg_ptr.p->m_state & Logfile_group::LG_SYNC_WAITERS_THREAD))
    {
      process_log_sync_waiters(signal, lg_ptr);
    }

    if(! (lg_ptr.p->m_state & Logfile_group::LG_WAITERS_THREAD))
    {
      process_log_buffer_waiters(signal, lg_ptr);
    }
  }
  else
  {
    ndbout_c("miss matched writes");
  }
  client_unlock(number(), __LINE__);
  
  return;
}

void
Lgman::execLCP_FRAG_ORD(Signal* signal)
{
  jamEntry();
  client_lock(number(), __LINE__);
  exec_lcp_frag_ord(signal, this);
  client_unlock(number(), __LINE__);
}

void
Lgman::exec_lcp_frag_ord(Signal* signal, SimulatedBlock* client_block)
{
  jamEntry();

  LcpFragOrd * ord = (LcpFragOrd *)signal->getDataPtr();
  Uint32 lcp_id= ord->lcpId;
  Uint32 frag_id = ord->fragmentId;
  Uint32 table_id = ord->tableId;

  Ptr<Logfile_group> ptr;
  m_logfile_group_list.first(ptr);
  
  Uint32 entry= lcp_id == m_latest_lcp ? 
    File_formats::Undofile::UNDO_LCP : File_formats::Undofile::UNDO_LCP_FIRST;
  if(!ptr.isNull() && ! (ptr.p->m_state & Logfile_group::LG_CUT_LOG_THREAD))
  {
    jam();
    ptr.p->m_state |= Logfile_group::LG_CUT_LOG_THREAD;
    signal->theData[0] = LgmanContinueB::CUT_LOG_TAIL;
    signal->theData[1] = ptr.i;
    client_block->sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
  }
  
  if(!ptr.isNull() && ptr.p->m_last_lsn)
  {
    Uint32 undo[3];
    undo[0] = lcp_id;
    undo[1] = (table_id << 16) | frag_id;
    undo[2] = (entry << 16 ) | (sizeof(undo) >> 2);

    Uint64 last_lsn= m_last_lsn;
    
    if(ptr.p->m_last_lsn == last_lsn
#ifdef VM_TRACE
       && ((rand() % 100) > 50)
#endif
       )
    {
      undo[2] |= File_formats::Undofile::UNDO_NEXT_LSN << 16;
      Uint32 *dst= get_log_buffer(ptr, sizeof(undo) >> 2);
      memcpy(dst, undo, sizeof(undo));
      ndbrequire(ptr.p->m_free_file_words >= (sizeof(undo) >> 2));
      ptr.p->m_free_file_words -= (sizeof(undo) >> 2);
    }
    else
    {
      Uint32 *dst= get_log_buffer(ptr, (sizeof(undo) >> 2) + 2);      
      * dst++ = (Uint32)(last_lsn >> 32);
      * dst++ = (Uint32)(last_lsn & 0xFFFFFFFF);
      memcpy(dst, undo, sizeof(undo));
      ndbrequire(ptr.p->m_free_file_words >= (sizeof(undo) >> 2));
      ptr.p->m_free_file_words -= ((sizeof(undo) >> 2) + 2);
    }
    ptr.p->m_last_lcp_lsn = last_lsn;
    m_last_lsn = ptr.p->m_last_lsn = last_lsn + 1;

    validate_logfile_group(ptr, "execLCP_FRAG_ORD");
  }
  
  while(!ptr.isNull())
  {
    if (ptr.p->m_last_lsn)
    {
      /**
       * First LCP_FRAGORD for each LCP, sets tail pos
       */
      if(m_latest_lcp != lcp_id)
      {
	ptr.p->m_tail_pos[0] = ptr.p->m_tail_pos[1];
	ptr.p->m_tail_pos[1] = ptr.p->m_tail_pos[2];
	ptr.p->m_tail_pos[2] = ptr.p->m_file_pos[HEAD];
      }
      
      if(0)
	ndbout_c
	  ("execLCP_FRAG_ORD (%d %d) (%d %d) (%d %d) free pages: %ld", 
	   ptr.p->m_tail_pos[0].m_ptr_i, ptr.p->m_tail_pos[0].m_idx,
	   ptr.p->m_tail_pos[1].m_ptr_i, ptr.p->m_tail_pos[1].m_idx,
	   ptr.p->m_tail_pos[2].m_ptr_i, ptr.p->m_tail_pos[2].m_idx,
	   (long) (ptr.p->m_free_file_words / File_formats::UNDO_PAGE_WORDS));
    }
    m_logfile_group_list.next(ptr);
  }
  
  m_latest_lcp = lcp_id;
}

void
Lgman::execEND_LCP_REQ(Signal* signal)
{
  EndLcpReq* req= (EndLcpReq*)signal->getDataPtr();
  ndbrequire(m_latest_lcp == req->backupId);
  m_end_lcp_senderdata = req->senderData;

  Ptr<Logfile_group> ptr;
  m_logfile_group_list.first(ptr);
  bool wait= false;
  while(!ptr.isNull())
  {
    Uint64 lcp_lsn = ptr.p->m_last_lcp_lsn;
    if(ptr.p->m_last_synced_lsn < lcp_lsn)
    {
      wait= true;
      if(signal->getSendersBlockRef() != reference())
      {
        D("Logfile_client - execEND_LCP_REQ");
	Logfile_client tmp(this, this, ptr.p->m_logfile_group_id);
	Logfile_client::Request req;
	req.m_callback.m_callbackData = ptr.i;
	req.m_callback.m_callbackIndex = ENDLCP_CALLBACK;
	ndbrequire(tmp.sync_lsn(signal, lcp_lsn, &req, 0) == 0);
      }
    }
    else
    {
      ptr.p->m_last_lcp_lsn = 0;
    }
    m_logfile_group_list.next(ptr);
  }
  
  if(wait)
  {
    return;
  }

  EndLcpConf* conf = (EndLcpConf*)signal->getDataPtrSend();
  conf->senderData = m_end_lcp_senderdata;
  conf->senderRef = reference();
  sendSignal(DBLQH_REF, GSN_END_LCP_CONF,
             signal, EndLcpConf::SignalLength, JBB);
}

void
Lgman::endlcp_callback(Signal* signal, Uint32 ptr, Uint32 res)
{
  EndLcpReq* req= (EndLcpReq*)signal->getDataPtr();
  req->backupId = m_latest_lcp;
  req->senderData = m_end_lcp_senderdata;
  execEND_LCP_REQ(signal);
}

void
Lgman::cut_log_tail(Signal* signal, Ptr<Logfile_group> ptr)
{
  bool done= true;
  if (likely(ptr.p->m_last_lsn))
  {
    Buffer_idx tmp= ptr.p->m_tail_pos[0];
    Buffer_idx tail= ptr.p->m_file_pos[TAIL];
    
    Ptr<Undofile> filePtr;
    m_file_pool.getPtr(filePtr, tail.m_ptr_i);
    
    if(!(tmp == tail))
    {
      Uint32 free;
      if(tmp.m_ptr_i == tail.m_ptr_i && tail.m_idx < tmp.m_idx)
      {
	free= tmp.m_idx - tail.m_idx; 
	ptr.p->m_free_file_words += free * File_formats::UNDO_PAGE_WORDS;
	ptr.p->m_file_pos[TAIL] = tmp;
      }
      else
      {
	free= filePtr.p->m_file_size - tail.m_idx - 1;
	ptr.p->m_free_file_words += free * File_formats::UNDO_PAGE_WORDS;
	
	Ptr<Undofile> next = filePtr;
	Local_undofile_list files(m_file_pool, ptr.p->m_files);
	while(files.next(next) && (next.p->m_state & Undofile::FS_EMPTY))
	  ndbrequire(next.i != filePtr.i);
	if(next.isNull())
	{
	  jam();
	  files.first(next);
	  while((next.p->m_state & Undofile::FS_EMPTY) && files.next(next))
	    ndbrequire(next.i != filePtr.i);
	}
	
	tmp.m_idx= 0;
	tmp.m_ptr_i= next.i;
	ptr.p->m_file_pos[TAIL] = tmp;
	done= false;      
      }
    } 
    
    validate_logfile_group(ptr, "cut log");
  }
  
  if (done)
  {
    ptr.p->m_state &= ~(Uint32)Logfile_group::LG_CUT_LOG_THREAD;
    m_logfile_group_list.next(ptr); 
  }
  
  if(!done || !ptr.isNull())
  {
    ptr.p->m_state |= Logfile_group::LG_CUT_LOG_THREAD;
    signal->theData[0] = LgmanContinueB::CUT_LOG_TAIL;
    signal->theData[1] = ptr.i;
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
  }
}

void
Lgman::execSUB_GCP_COMPLETE_REP(Signal* signal)
{
  jamEntry();

  Ptr<Logfile_group> ptr;
  m_logfile_group_list.first(ptr);

  /**
   * Filter all logfile groups in parallell
   */
  return; // NOT IMPLETMENT YET
  
  signal->theData[0] = LgmanContinueB::FILTER_LOG;
  while(!ptr.isNull())
  {
    signal->theData[1] = ptr.i;
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
    m_logfile_group_list.next(ptr);
  }
}

int
Lgman::alloc_log_space(Uint32 ref, Uint32 words)
{
  ndbrequire(words);
  words += 2; // lsn
  Logfile_group key;
  key.m_logfile_group_id= ref;
  Ptr<Logfile_group> ptr;
  if(m_logfile_group_hash.find(ptr, key) && 
     ptr.p->m_free_file_words >= (words + (4 * File_formats::UNDO_PAGE_WORDS)))
  {
    ptr.p->m_free_file_words -= words;
    validate_logfile_group(ptr, "alloc_log_space");
    return 0;
  }
  
  if(ptr.isNull())
  {
    return -1;
  }
  
  return 1501;
}

int
Lgman::free_log_space(Uint32 ref, Uint32 words)
{
  ndbrequire(words);
  Logfile_group key;
  key.m_logfile_group_id= ref;
  Ptr<Logfile_group> ptr;
  if(m_logfile_group_hash.find(ptr, key))
  {
    ptr.p->m_free_file_words += (words + 2);
    validate_logfile_group(ptr, "free_log_space");
    return 0;
  }
  ndbrequire(false);
  return -1;
}

Uint64
Logfile_client::add_entry(const Change* src, Uint32 cnt)
{
  Uint32 i, tot= 0;
  for(i= 0; i<cnt; i++)
  {
    tot += src[i].len;
  }
  
  Uint32 *dst;
  Uint64 last_lsn= m_lgman->m_last_lsn;
  {
    Lgman::Logfile_group key;
    key.m_logfile_group_id= m_logfile_group_id;
    Ptr<Lgman::Logfile_group> ptr;
    if(m_lgman->m_logfile_group_hash.find(ptr, key))
    {
      Uint32 callback_buffer = ptr.p->m_callback_buffer_words;
      Uint64 last_lsn_filegroup= ptr.p->m_last_lsn;
      if(last_lsn_filegroup == last_lsn
#ifdef VM_TRACE
	 && ((rand() % 100) > 50)
#endif
	 )
      {
	dst= m_lgman->get_log_buffer(ptr, tot);
	for(i= 0; i<cnt; i++)
	{
	  memcpy(dst, src[i].ptr, 4*src[i].len);
	  dst += src[i].len;
	}
	* (dst - 1) |= File_formats::Undofile::UNDO_NEXT_LSN << 16;
	ptr.p->m_free_file_words += 2;
	m_lgman->validate_logfile_group(ptr);
      }
      else
      {
	dst= m_lgman->get_log_buffer(ptr, tot + 2);
	* dst++ = (Uint32)(last_lsn >> 32);
	* dst++ = (Uint32)(last_lsn & 0xFFFFFFFF);
	for(i= 0; i<cnt; i++)
	{
	  memcpy(dst, src[i].ptr, 4*src[i].len);
	  dst += src[i].len;
	}
      }
      /**
       * for callback_buffer, always allocats 2 extra...
       *   not knowing if LSN must be added or not
       */
      tot += 2;

      if (unlikely(! (tot <= callback_buffer)))
      {
        abort();
      }
      ptr.p->m_callback_buffer_words = callback_buffer - tot;
    }
    
    m_lgman->m_last_lsn = ptr.p->m_last_lsn = last_lsn + 1;
    
    return last_lsn;
  }
}

void
Lgman::execSTART_RECREQ(Signal* signal)
{
  m_latest_lcp = signal->theData[0];
  
  Ptr<Logfile_group> ptr;
  m_logfile_group_list.first(ptr);

  if(ptr.i != RNIL)
  {
    infoEvent("Applying undo to LCP: %d", m_latest_lcp);
    ndbout_c("Applying undo to LCP: %d", m_latest_lcp);
    find_log_head(signal, ptr);
    return;
  }
  
  signal->theData[0] = reference();
  sendSignal(DBLQH_REF, GSN_START_RECCONF, signal, 1, JBB);
}

void
Lgman::find_log_head(Signal* signal, Ptr<Logfile_group> ptr)
{
  ndbrequire(ptr.p->m_state & 
             (Logfile_group::LG_STARTING | Logfile_group::LG_SORTING));

  if(ptr.p->m_meta_files.isEmpty() && ptr.p->m_files.isEmpty())
  {
    jam();
    /**
     * Logfile_group wo/ any files 
     */
    ptr.p->m_state &= ~(Uint32)Logfile_group::LG_STARTING;
    ptr.p->m_state |= Logfile_group::LG_ONLINE;
    m_logfile_group_list.next(ptr);
    signal->theData[0] = LgmanContinueB::FIND_LOG_HEAD;
    signal->theData[1] = ptr.i;
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
    return;
  }

  ptr.p->m_state = Logfile_group::LG_SORTING;
  
  /**
   * Read first page from each undofile (1 file at a time...)
   */
  Local_undofile_list files(m_file_pool, ptr.p->m_meta_files);
  Ptr<Undofile> file_ptr;
  files.first(file_ptr);
  
  if(!file_ptr.isNull())
  {
    /**
     * Use log buffer memory when reading
     */
    Uint32 page_id = ptr.p->m_pos[CONSUMER].m_current_pos.m_ptr_i;
    file_ptr.p->m_online.m_outstanding= page_id;
    
    FsReadWriteReq* req= (FsReadWriteReq*)signal->getDataPtrSend();
    req->filePointer = file_ptr.p->m_fd;
    req->userReference = reference();
    req->userPointer = file_ptr.i;
    req->varIndex = 1; // skip zero page
    req->numberOfPages = 1;
    req->data.pageData[0] = page_id;
    req->operationFlag = 0;
    FsReadWriteReq::setFormatFlag(req->operationFlag, 
				  FsReadWriteReq::fsFormatSharedPage);
    
    sendSignal(NDBFS_REF, GSN_FSREADREQ, signal, 
	       FsReadWriteReq::FixedLength + 1, JBA);

    ptr.p->m_outstanding_fs++;
    file_ptr.p->m_state |= Undofile::FS_OUTSTANDING;
    return;
  }
  else
  {
    /**
     * All files have read first page
     *   and m_files is sorted acording to lsn
     */
    ndbrequire(!ptr.p->m_files.isEmpty());
    Local_undofile_list read_files(m_file_pool, ptr.p->m_files);
    read_files.last(file_ptr);
    

    /**
     * Init binary search
     */
    ptr.p->m_state = Logfile_group::LG_SEARCHING;
    file_ptr.p->m_state = Undofile::FS_SEARCHING;
    ptr.p->m_file_pos[TAIL].m_idx = 1;                   // left page
    ptr.p->m_file_pos[HEAD].m_idx = file_ptr.p->m_file_size;
    ptr.p->m_file_pos[HEAD].m_ptr_i = ((file_ptr.p->m_file_size - 1) >> 1) + 1;
    
    Uint32 page_id = ptr.p->m_pos[CONSUMER].m_current_pos.m_ptr_i;
    file_ptr.p->m_online.m_outstanding= page_id;

    FsReadWriteReq* req= (FsReadWriteReq*)signal->getDataPtrSend();
    req->filePointer = file_ptr.p->m_fd;
    req->userReference = reference();
    req->userPointer = file_ptr.i;
    req->varIndex = ptr.p->m_file_pos[HEAD].m_ptr_i;
    req->numberOfPages = 1;
    req->data.pageData[0] = page_id;
    req->operationFlag = 0;
    FsReadWriteReq::setFormatFlag(req->operationFlag, 
				  FsReadWriteReq::fsFormatSharedPage);
    
    sendSignal(NDBFS_REF, GSN_FSREADREQ, signal, 
	       FsReadWriteReq::FixedLength + 1, JBA);
    
    ptr.p->m_outstanding_fs++;
    file_ptr.p->m_state |= Undofile::FS_OUTSTANDING;
    return;
  }
}

void
Lgman::execFSREADCONF(Signal* signal)
{
  jamEntry();
  client_lock(number(), __LINE__);

  Ptr<Undofile> ptr;  
  Ptr<Logfile_group> lg_ptr;
  FsConf* conf = (FsConf*)signal->getDataPtr();
  
  m_file_pool.getPtr(ptr, conf->userPointer);
  m_logfile_group_pool.getPtr(lg_ptr, ptr.p->m_logfile_group_ptr_i);

  ndbrequire(ptr.p->m_state & Undofile::FS_OUTSTANDING);
  ptr.p->m_state &= ~(Uint32)Undofile::FS_OUTSTANDING;
  
  Uint32 cnt= lg_ptr.p->m_outstanding_fs;
  ndbrequire(cnt);
  
  if((ptr.p->m_state & Undofile::FS_EXECUTING)== Undofile::FS_EXECUTING)
  {
    jam();
    
    if(lg_ptr.p->m_next_reply_ptr_i == ptr.i)
    {
      Uint32 tot= 0;
      Local_undofile_list files(m_file_pool, lg_ptr.p->m_files);
      while(cnt && ! (ptr.p->m_state & Undofile::FS_OUTSTANDING))
      {
	Uint32 state= ptr.p->m_state;
	Uint32 pages= ptr.p->m_online.m_outstanding;
	ndbrequire(pages);
	ptr.p->m_online.m_outstanding= 0;
	ptr.p->m_state &= ~(Uint32)Undofile::FS_MOVE_NEXT;
	tot += pages;
	cnt--;
	
	if((state & Undofile::FS_MOVE_NEXT) && !files.prev(ptr))
	  files.last(ptr);
      }
      
      lg_ptr.p->m_outstanding_fs = cnt;
      lg_ptr.p->m_pos[PRODUCER].m_current_pos.m_idx += tot;
      lg_ptr.p->m_next_reply_ptr_i = ptr.i;
    }
    client_unlock(number(), __LINE__);
    return;
  }
  
  lg_ptr.p->m_outstanding_fs = cnt - 1;

  Ptr<GlobalPage> page_ptr;
  m_shared_page_pool.getPtr(page_ptr, ptr.p->m_online.m_outstanding);
  ptr.p->m_online.m_outstanding= 0;
  
  File_formats::Undofile::Undo_page* page = 
    (File_formats::Undofile::Undo_page*)page_ptr.p;
  
  Uint64 lsn = 0;
  lsn += page->m_page_header.m_page_lsn_hi; lsn <<= 32;
  lsn += page->m_page_header.m_page_lsn_lo;

  switch(ptr.p->m_state){
  case Undofile::FS_SORTING:
    jam();
    break;
  case Undofile::FS_SEARCHING:
    jam();
    find_log_head_in_file(signal, lg_ptr, ptr, lsn);
    client_unlock(number(), __LINE__);
    return;
  default:
  case Undofile::FS_EXECUTING:
  case Undofile::FS_CREATING:
  case Undofile::FS_DROPPING:
  case Undofile::FS_ONLINE:
  case Undofile::FS_OPENING:
  case Undofile::FS_EMPTY:
    jam();
    ndbrequire(false);
  }

  /**
   * Prepare for execution
   */
  ptr.p->m_state = Undofile::FS_EXECUTING;
  ptr.p->m_online.m_lsn = lsn;
  
  /**
   * Insert into m_files
   */
  {
    Local_undofile_list meta(m_file_pool, lg_ptr.p->m_meta_files);  
    Local_undofile_list files(m_file_pool, lg_ptr.p->m_files);
    meta.remove(ptr);

    Ptr<Undofile> loop;  
    files.first(loop);
    while(!loop.isNull() && loop.p->m_online.m_lsn <= lsn)
      files.next(loop);
    
    if(loop.isNull())
    {
      /**
       * File has highest lsn, add last
       */
      jam();
      files.add(ptr);
    }
    else
    {
      /**
       * Insert file in correct position in file list
       */
      files.insert(ptr, loop);
    }
  }
  find_log_head(signal, lg_ptr);
  client_unlock(number(), __LINE__);
}
  
void
Lgman::execFSREADREF(Signal* signal)
{
  jamEntry();
  SimulatedBlock::execFSREADREF(signal);
  ndbrequire(false);
}

void
Lgman::find_log_head_in_file(Signal* signal, 
			     Ptr<Logfile_group> ptr, 
			     Ptr<Undofile> file_ptr,
			     Uint64 last_lsn)
{ 
  //     a b
  // 3 4 5 0 1 
  Uint32 curr= ptr.p->m_file_pos[HEAD].m_ptr_i;
  Uint32 head= ptr.p->m_file_pos[HEAD].m_idx;
  Uint32 tail= ptr.p->m_file_pos[TAIL].m_idx;

  ndbrequire(head > tail);
  Uint32 diff = head - tail;
  
  if(DEBUG_SEARCH_LOG_HEAD)
    printf("tail: %d(%lld) head: %d last: %d(%lld) -> ", 
	   tail, file_ptr.p->m_online.m_lsn,
	   head, curr, last_lsn);
  if(last_lsn > file_ptr.p->m_online.m_lsn)
  {
    if(DEBUG_SEARCH_LOG_HEAD)
      printf("moving tail ");
    
    file_ptr.p->m_online.m_lsn = last_lsn;
    ptr.p->m_file_pos[TAIL].m_idx = tail = curr;
  }
  else
  {
    if(DEBUG_SEARCH_LOG_HEAD)
      printf("moving head ");

    ptr.p->m_file_pos[HEAD].m_idx = head = curr;
  }
  
  if(diff > 1)
  {
    // We need to find more pages to be sure...
    ptr.p->m_file_pos[HEAD].m_ptr_i = curr = ((head + tail) >> 1);

    if(DEBUG_SEARCH_LOG_HEAD)    
      ndbout_c("-> new search tail: %d(%lld) head: %d -> %d", 
	       tail, file_ptr.p->m_online.m_lsn,
	       head, curr);

    Uint32 page_id = ptr.p->m_pos[CONSUMER].m_current_pos.m_ptr_i;
    file_ptr.p->m_online.m_outstanding= page_id;
    
    FsReadWriteReq* req= (FsReadWriteReq*)signal->getDataPtrSend();
    req->filePointer = file_ptr.p->m_fd;
    req->userReference = reference();
    req->userPointer = file_ptr.i;
    req->varIndex = curr;
    req->numberOfPages = 1;
    req->data.pageData[0] = page_id;
    req->operationFlag = 0;
    FsReadWriteReq::setFormatFlag(req->operationFlag, 
				  FsReadWriteReq::fsFormatSharedPage);
    
    sendSignal(NDBFS_REF, GSN_FSREADREQ, signal, 
	       FsReadWriteReq::FixedLength + 1, JBA);
    
    ptr.p->m_outstanding_fs++;
    file_ptr.p->m_state |= Undofile::FS_OUTSTANDING;
    return;
  }
  
  ndbrequire(diff == 1);
  if(DEBUG_SEARCH_LOG_HEAD)    
    ndbout_c("-> found last page: %d", tail);
  
  ptr.p->m_state = 0;
  file_ptr.p->m_state = Undofile::FS_EXECUTING;
  ptr.p->m_last_lsn = file_ptr.p->m_online.m_lsn;
  ptr.p->m_last_read_lsn = file_ptr.p->m_online.m_lsn;
  ptr.p->m_last_synced_lsn = file_ptr.p->m_online.m_lsn;
  m_last_lsn = file_ptr.p->m_online.m_lsn;
  
  /**
   * Set HEAD position
   */
  ptr.p->m_file_pos[HEAD].m_ptr_i = file_ptr.i;
  ptr.p->m_file_pos[HEAD].m_idx = tail;
  
  ptr.p->m_file_pos[TAIL].m_ptr_i = file_ptr.i;
  ptr.p->m_file_pos[TAIL].m_idx = tail - 1;
  ptr.p->m_next_reply_ptr_i = file_ptr.i;
  
  {
    Local_undofile_list files(m_file_pool, ptr.p->m_files);
    if(tail == 1)
    {
      /**
       * HEAD is first page in a file...
       *   -> PREV should be in previous file
       */
      Ptr<Undofile> prev = file_ptr;
      if(!files.prev(prev))
      {
	files.last(prev);
      }
      ptr.p->m_file_pos[TAIL].m_ptr_i = prev.i;
      ptr.p->m_file_pos[TAIL].m_idx = prev.p->m_file_size - 1;
      ptr.p->m_next_reply_ptr_i = prev.i;
    }
    
    SimulatedBlock* fs = globalData.getBlock(NDBFS);
    infoEvent("Undo head - %s page: %d lsn: %lld",
	      fs->get_filename(file_ptr.p->m_fd), 
	      tail, file_ptr.p->m_online.m_lsn);
    g_eventLogger->info("Undo head - %s page: %d lsn: %lld",
                        fs->get_filename(file_ptr.p->m_fd),
                        tail, file_ptr.p->m_online.m_lsn);
    
    for(files.prev(file_ptr); !file_ptr.isNull(); files.prev(file_ptr))
    {
      infoEvent("   - next - %s(%lld)", 
		fs->get_filename(file_ptr.p->m_fd), 
		file_ptr.p->m_online.m_lsn);

      g_eventLogger->info("   - next - %s(%lld)", 
                          fs->get_filename(file_ptr.p->m_fd),
                          file_ptr.p->m_online.m_lsn);
    }
  }
  
  /**
   * Start next logfile group
   */
  m_logfile_group_list.next(ptr);
  signal->theData[0] = LgmanContinueB::FIND_LOG_HEAD;
  signal->theData[1] = ptr.i;
  sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
}

void
Lgman::init_run_undo_log(Signal* signal)
{
  /**
   * Perform initial sorting of logfile groups
   */
  Ptr<Logfile_group> group;
  Logfile_group_list& list= m_logfile_group_list;
  Logfile_group_list tmp(m_logfile_group_pool);

  bool found_any = false;

  list.first(group);
  while(!group.isNull())
  {
    Ptr<Logfile_group> ptr= group;
    list.next(group);
    list.remove(ptr);

    if (ptr.p->m_state & Logfile_group::LG_ONLINE)
    {
      /**
       * No logfiles in group
       */
      jam();
      tmp.addLast(ptr);
      continue;
    }
    
    found_any = true;

    {
      /**
       * Init buffer pointers
       */
      ptr.p->m_free_buffer_words -= File_formats::UNDO_PAGE_WORDS;
      ptr.p->m_pos[CONSUMER].m_current_page.m_idx = 0; // 0 more pages read
      ptr.p->m_pos[PRODUCER].m_current_page.m_idx = 0; // 0 more pages read
      
      Uint32 page = ptr.p->m_pos[CONSUMER].m_current_pos.m_ptr_i;
      File_formats::Undofile::Undo_page* pageP = 
	(File_formats::Undofile::Undo_page*)m_shared_page_pool.getPtr(page);
      
      ptr.p->m_pos[CONSUMER].m_current_pos.m_idx = pageP->m_words_used;
      ptr.p->m_pos[PRODUCER].m_current_pos.m_idx = 1;
      ptr.p->m_last_read_lsn++;
    }
    
    /**
     * Start producer thread
     */
    signal->theData[0] = LgmanContinueB::READ_UNDO_LOG;
    signal->theData[1] = ptr.i;
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
    
    /**
     * Insert in correct postion in list of logfile_group's
     */
    Ptr<Logfile_group> pos;
    for(tmp.first(pos); !pos.isNull(); tmp.next(pos))
      if(ptr.p->m_last_read_lsn >= pos.p->m_last_read_lsn)
	break;
    
    if(pos.isNull())
      tmp.add(ptr);
    else
      tmp.insert(ptr, pos);
    
    ptr.p->m_state = 
      Logfile_group::LG_EXEC_THREAD | Logfile_group::LG_READ_THREAD;
  }
  list = tmp;

  if (found_any == false)
  {
    /**
     * No logfilegroup had any logfiles
     */
    jam();
    signal->theData[0] = reference();
    sendSignal(DBLQH_REF, GSN_START_RECCONF, signal, 1, JBB);
    return;
  }
  
  execute_undo_record(signal);
}

void
Lgman::read_undo_log(Signal* signal, Ptr<Logfile_group> ptr)
{
  Uint32 cnt, free= ptr.p->m_free_buffer_words;

  if(! (ptr.p->m_state & Logfile_group::LG_EXEC_THREAD))
  {
    jam();
    /**
     * Logfile_group is done...
     */
    ptr.p->m_state &= ~(Uint32)Logfile_group::LG_READ_THREAD;
    stop_run_undo_log(signal);
    return;
  }
  
  if(free <= File_formats::UNDO_PAGE_WORDS)
  {
    signal->theData[0] = LgmanContinueB::READ_UNDO_LOG;
    signal->theData[1] = ptr.i;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 2);
    return;
  }

  Logfile_group::Position producer= ptr.p->m_pos[PRODUCER];
  Logfile_group::Position consumer= ptr.p->m_pos[CONSUMER];

  if(producer.m_current_page.m_idx == 0)
  {
    /**
     * zero pages left in range -> switch range
     */
    Lgman::Page_map::Iterator it;
    Page_map map(m_data_buffer_pool, ptr.p->m_buffer_pages);
    Uint32 sz = map.getSize();
    Uint32 pos= (producer.m_current_page.m_ptr_i + sz - 2) % sz;
    map.position(it, pos);
    union {
      Uint32 _tmp[2];
      Lgman::Buffer_idx range;
    };
    _tmp[0] = *it.data; map.next(it); _tmp[1] = *it.data;
    producer.m_current_page.m_ptr_i = pos;
    producer.m_current_page.m_idx = range.m_idx;
    producer.m_current_pos.m_ptr_i = range.m_ptr_i + range.m_idx;
  }
  
  if(producer.m_current_page.m_ptr_i == consumer.m_current_page.m_ptr_i &&
     producer.m_current_pos.m_ptr_i > consumer.m_current_pos.m_ptr_i)
  {
    Uint32 max= 
      producer.m_current_pos.m_ptr_i - consumer.m_current_pos.m_ptr_i - 1;
    ndbrequire(free >= max * File_formats::UNDO_PAGE_WORDS);
    cnt= read_undo_pages(signal, ptr, producer.m_current_pos.m_ptr_i, max);
    ndbrequire(cnt <= max);    
    producer.m_current_pos.m_ptr_i -= cnt;
    producer.m_current_page.m_idx -= cnt;
  } 
  else
  {
    Uint32 max= producer.m_current_page.m_idx;
    ndbrequire(free >= max * File_formats::UNDO_PAGE_WORDS);
    cnt= read_undo_pages(signal, ptr, producer.m_current_pos.m_ptr_i, max);
    ndbrequire(cnt <= max);
    producer.m_current_pos.m_ptr_i -= cnt;
    producer.m_current_page.m_idx -= cnt;
  } 
  
  ndbrequire(free >= cnt * File_formats::UNDO_PAGE_WORDS);
  free -= (cnt * File_formats::UNDO_PAGE_WORDS);
  ptr.p->m_free_buffer_words = free;
  ptr.p->m_pos[PRODUCER] = producer;  

  signal->theData[0] = LgmanContinueB::READ_UNDO_LOG;
  signal->theData[1] = ptr.i;

  if(free > File_formats::UNDO_PAGE_WORDS)
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
  else
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 2);
}

Uint32
Lgman::read_undo_pages(Signal* signal, Ptr<Logfile_group> ptr, 
		       Uint32 pageId, Uint32 pages)
{
  ndbrequire(pages);
  Ptr<Undofile> filePtr;
  Buffer_idx tail= ptr.p->m_file_pos[TAIL];
  m_file_pool.getPtr(filePtr, tail.m_ptr_i);
  
  if(filePtr.p->m_online.m_outstanding > 0)
  {
    jam();
    return 0;
  }

  Uint32 max= tail.m_idx;

  FsReadWriteReq* req= (FsReadWriteReq*)signal->getDataPtrSend();
  req->filePointer = filePtr.p->m_fd;
  req->userReference = reference();
  req->userPointer = filePtr.i;
  req->operationFlag = 0;
  FsReadWriteReq::setFormatFlag(req->operationFlag, 
				FsReadWriteReq::fsFormatSharedPage);


  if(max > pages)
  {
    jam();
    tail.m_idx -= pages;

    req->varIndex = 1 + tail.m_idx;
    req->numberOfPages = pages;
    req->data.pageData[0] = pageId - pages;
    ptr.p->m_file_pos[TAIL] = tail;
    
    if(DEBUG_UNDO_EXECUTION)
      ndbout_c("a reading from file: %d page(%d-%d) into (%d-%d)",
	       ptr.i, 1 + tail.m_idx, 1+tail.m_idx+pages-1,
	       pageId - pages, pageId - 1);

    sendSignal(NDBFS_REF, GSN_FSREADREQ, signal, 
	       FsReadWriteReq::FixedLength + 1, JBA);
    
    ptr.p->m_outstanding_fs++;
    filePtr.p->m_state |= Undofile::FS_OUTSTANDING;
    filePtr.p->m_online.m_outstanding = pages;
    max = pages;
  }
  else
  {
    jam();

    ndbrequire(tail.m_idx - max == 0);
    req->varIndex = 1;
    req->numberOfPages = max;
    req->data.pageData[0] = pageId - max;
    
    if(DEBUG_UNDO_EXECUTION)
      ndbout_c("b reading from file: %d page(%d-%d) into (%d-%d)",
	       ptr.i, 1 , 1+max-1,
	       pageId - max, pageId - 1);
    
    sendSignal(NDBFS_REF, GSN_FSREADREQ, signal, 
	       FsReadWriteReq::FixedLength + 1, JBA);
    
    ptr.p->m_outstanding_fs++;
    filePtr.p->m_online.m_outstanding = max;
    filePtr.p->m_state |= Undofile::FS_OUTSTANDING | Undofile::FS_MOVE_NEXT;
    
    Ptr<Undofile> prev = filePtr;
    {
      Local_undofile_list files(m_file_pool, ptr.p->m_files);
      if(!files.prev(prev))
      {
	jam();
	files.last(prev);
      }
    }
    if(DEBUG_UNDO_EXECUTION)
      ndbout_c("changing file from %d to %d", filePtr.i, prev.i);

    tail.m_idx= prev.p->m_file_size - 1;
    tail.m_ptr_i= prev.i;
    ptr.p->m_file_pos[TAIL] = tail;
    if(max < pages && filePtr.i != prev.i)
      max += read_undo_pages(signal, ptr, pageId - max, pages - max);
  }
  
  return max;

}

void
Lgman::execute_undo_record(Signal* signal)
{
  Uint64 lsn;
  const Uint32* ptr;
  if((ptr = get_next_undo_record(&lsn)))
  {
    Uint32 len= (* ptr) & 0xFFFF;
    Uint32 type= (* ptr) >> 16;
    Uint32 mask= type & ~(Uint32)File_formats::Undofile::UNDO_NEXT_LSN;
    switch(mask){
    case File_formats::Undofile::UNDO_END:
      stop_run_undo_log(signal);
      return;
    case File_formats::Undofile::UNDO_LCP:
    case File_formats::Undofile::UNDO_LCP_FIRST:
    {
      Uint32 lcp = * (ptr - len + 1);
      if(m_latest_lcp && lcp > m_latest_lcp)
      {
        if (0)
        {
	  const Uint32 * base = ptr - len + 1;
          Uint32 lcp = base[0];
          Uint32 tableId = base[1] >> 16;
          Uint32 fragId = base[1] & 0xFFFF;

	  ndbout_c("NOT! ignoring lcp: %u tab: %u frag: %u", 
		   lcp, tableId, fragId);
	}
      }

      if(m_latest_lcp == 0 || 
	 lcp < m_latest_lcp || 
	 (lcp == m_latest_lcp && 
	  mask == File_formats::Undofile::UNDO_LCP_FIRST))
      {
	stop_run_undo_log(signal);
	return;
      }
      // Fallthrough
    }
    case File_formats::Undofile::UNDO_TUP_ALLOC:
    case File_formats::Undofile::UNDO_TUP_UPDATE:
    case File_formats::Undofile::UNDO_TUP_FREE:
    case File_formats::Undofile::UNDO_TUP_CREATE:
    case File_formats::Undofile::UNDO_TUP_DROP:
    case File_formats::Undofile::UNDO_TUP_ALLOC_EXTENT:
    case File_formats::Undofile::UNDO_TUP_FREE_EXTENT:
      {
        Dbtup_client tup(this, m_tup);
        tup.disk_restart_undo(signal, lsn, mask, ptr - len + 1, len);
        jamEntry();
      }
      return;
    default:
      ndbrequire(false);
    }
  }
  signal->theData[0] = LgmanContinueB::EXECUTE_UNDO_RECORD;
  sendSignal(LGMAN_REF, GSN_CONTINUEB, signal, 1, JBB);
  
  return;
}

const Uint32*
Lgman::get_next_undo_record(Uint64 * this_lsn)
{
  Ptr<Logfile_group> ptr;
  m_logfile_group_list.first(ptr);

  Logfile_group::Position consumer= ptr.p->m_pos[CONSUMER];
  Logfile_group::Position producer= ptr.p->m_pos[PRODUCER];
  if(producer.m_current_pos.m_idx < 2)
  {
    jam();
    /**
     * Wait for fetching pages...
     */
    return 0;
  }
  
  Uint32 pos = consumer.m_current_pos.m_idx;
  Uint32 page = consumer.m_current_pos.m_ptr_i;
  
  File_formats::Undofile::Undo_page* pageP=(File_formats::Undofile::Undo_page*)
    m_shared_page_pool.getPtr(page);

  if(pos == 0)
  {
    /**
     * End of log
     */
    pageP->m_data[0] = (File_formats::Undofile::UNDO_END << 16) | 1 ;
    pageP->m_page_header.m_page_lsn_hi = 0;
    pageP->m_page_header.m_page_lsn_lo = 0;
    pos= consumer.m_current_pos.m_idx= pageP->m_words_used = 1;
    this_lsn = 0;
    return pageP->m_data;
  }

  Uint32 *record= pageP->m_data + pos - 1;
  Uint32 len= (* record) & 0xFFFF;
  ndbrequire(len);
  Uint32 *prev= record - len;
  Uint64 lsn = 0;

  // Same page
  if(((* record) >> 16) & File_formats::Undofile::UNDO_NEXT_LSN)
  {
    lsn = ptr.p->m_last_read_lsn - 1;
    ndbrequire((Int64)lsn >= 0);
  }
  else
  {
    ndbrequire(pos >= 3);
    lsn += * (prev - 1); lsn <<= 32;
    lsn += * (prev - 0);
    len += 2;
    ndbrequire((Int64)lsn >= 0);
  }
  

  ndbrequire(pos >= len);
  
  if(pos == len)
  {
    /**
     * Switching page
     */
    ndbrequire(producer.m_current_pos.m_idx);
    ptr.p->m_pos[PRODUCER].m_current_pos.m_idx --;

    if(consumer.m_current_page.m_idx)
    {
      consumer.m_current_page.m_idx--;   // left in range
      consumer.m_current_pos.m_ptr_i --; // page
    }
    else
    {
      // 0 pages left in range...switch range
      Lgman::Page_map::Iterator it;
      Page_map map(m_data_buffer_pool, ptr.p->m_buffer_pages);
      Uint32 sz = map.getSize();
      Uint32 tmp = (consumer.m_current_page.m_ptr_i + sz - 2) % sz;
      
      map.position(it, tmp);
      union {
	Uint32 _tmp[2];
	Lgman::Buffer_idx range;
      };
      
      _tmp[0] = *it.data; map.next(it); _tmp[1] = *it.data;
      
      consumer.m_current_page.m_idx = range.m_idx - 1; // left in range
      consumer.m_current_page.m_ptr_i = tmp;           // pos in map

      consumer.m_current_pos.m_ptr_i = range.m_ptr_i + range.m_idx - 1; // page
    }

    if(DEBUG_UNDO_EXECUTION)
      ndbout_c("reading from %d", consumer.m_current_pos.m_ptr_i);

    pageP=(File_formats::Undofile::Undo_page*)
      m_shared_page_pool.getPtr(consumer.m_current_pos.m_ptr_i);
    
    pos= consumer.m_current_pos.m_idx= pageP->m_words_used;
    
    Uint64 tmp = 0;
    tmp += pageP->m_page_header.m_page_lsn_hi; tmp <<= 32;
    tmp += pageP->m_page_header.m_page_lsn_lo;
    
    prev = pageP->m_data + pos - 1;
    
    if(((* prev) >> 16) & File_formats::Undofile::UNDO_NEXT_LSN)
    {
      ndbrequire(lsn + 1 == ptr.p->m_last_read_lsn);
    }

    ptr.p->m_pos[CONSUMER] = consumer;
    ptr.p->m_free_buffer_words += File_formats::UNDO_PAGE_WORDS;
  }
  else
  {
    ptr.p->m_pos[CONSUMER].m_current_pos.m_idx -= len;
  }
  
  * this_lsn = ptr.p->m_last_read_lsn = lsn;

  /**
   * Re-sort log file groups
   */
  Ptr<Logfile_group> sort = ptr;
  if(m_logfile_group_list.next(sort))
  {
    while(!sort.isNull() && sort.p->m_last_read_lsn > lsn)
      m_logfile_group_list.next(sort);
    
    if(sort.i != ptr.p->nextList)
    {
      m_logfile_group_list.remove(ptr);
      if(sort.isNull())
	m_logfile_group_list.add(ptr);
      else
	m_logfile_group_list.insert(ptr, sort);
    }
  }
  return record;
}

void
Lgman::stop_run_undo_log(Signal* signal)
{
  bool running = false, outstanding = false;
  Ptr<Logfile_group> ptr;
  m_logfile_group_list.first(ptr);
  while(!ptr.isNull())
  {
    /**
     * Mark exec thread as completed
     */
    ptr.p->m_state &= ~(Uint32)Logfile_group::LG_EXEC_THREAD;

    if(ptr.p->m_state & Logfile_group::LG_READ_THREAD)
    {
      /**
       * Thread is still running...wait for it to complete
       */
      running = true;
    }
    else if(ptr.p->m_outstanding_fs)
    {
      outstanding = true; // a FSREADREQ is outstanding...wait for it
    }
    else if(ptr.p->m_state != Logfile_group::LG_ONLINE)
    {
      /**
       * Fix log TAIL
       */
      ndbrequire(ptr.p->m_state == 0);
      ptr.p->m_state = Logfile_group::LG_ONLINE;
      Buffer_idx tail= ptr.p->m_file_pos[TAIL];
      Uint32 pages= ptr.p->m_pos[PRODUCER].m_current_pos.m_idx;
      
      while(pages)
      {
	Ptr<Undofile> file;
	m_file_pool.getPtr(file, tail.m_ptr_i);
	Uint32 page= tail.m_idx;
	Uint32 size= file.p->m_file_size;
	ndbrequire(size >= page);
	Uint32 diff= size - page;
	
	if(pages >= diff)
	{
	  pages -= diff;
	  Local_undofile_list files(m_file_pool, ptr.p->m_files);
	  if(!files.next(file))
	    files.first(file);
	  tail.m_idx = 1;
	  tail.m_ptr_i= file.i;
	}
	else
	{
	  tail.m_idx += pages;
	  pages= 0;
	}
      }
      ptr.p->m_tail_pos[0] = tail;
      ptr.p->m_tail_pos[1] = tail;
      ptr.p->m_tail_pos[2] = tail;
      ptr.p->m_file_pos[TAIL] = tail;

      init_logbuffer_pointers(ptr);

      {
	Buffer_idx head= ptr.p->m_file_pos[HEAD];
	Ptr<Undofile> file;
	m_file_pool.getPtr(file, head.m_ptr_i);
	if (head.m_idx == file.p->m_file_size - 1)
	{
	  Local_undofile_list files(m_file_pool, ptr.p->m_files);
	  if(!files.next(file))
	  {
	    jam();
	    files.first(file);
	  }
	  head.m_idx = 0;
	  head.m_ptr_i = file.i;
	  ptr.p->m_file_pos[HEAD] = head;
	}
      }
      
      client_lock(number(), __LINE__);
      ptr.p->m_free_file_words = (Uint64)File_formats::UNDO_PAGE_WORDS * 
	(Uint64)compute_free_file_pages(ptr);
      client_unlock(number(), __LINE__);
      ptr.p->m_next_reply_ptr_i = ptr.p->m_file_pos[HEAD].m_ptr_i;
      
      ptr.p->m_state |= Logfile_group::LG_FLUSH_THREAD;
      signal->theData[0] = LgmanContinueB::FLUSH_LOG;
      signal->theData[1] = ptr.i;
      signal->theData[2] = 0;
      sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBB);

      if(1)
      {
	SimulatedBlock* fs = globalData.getBlock(NDBFS);
	Ptr<Undofile> hf, tf;
	m_file_pool.getPtr(tf, tail.m_ptr_i);
	m_file_pool.getPtr(hf,  ptr.p->m_file_pos[HEAD].m_ptr_i);
	infoEvent("Logfile group: %d ", ptr.p->m_logfile_group_id);
        g_eventLogger->info("Logfile group: %d ", ptr.p->m_logfile_group_id);
	infoEvent("  head: %s page: %d",
                  fs->get_filename(hf.p->m_fd),
                  ptr.p->m_file_pos[HEAD].m_idx);
        g_eventLogger->info("  head: %s page: %d",
                            fs->get_filename(hf.p->m_fd),
                            ptr.p->m_file_pos[HEAD].m_idx);
	infoEvent("  tail: %s page: %d",
		  fs->get_filename(tf.p->m_fd), tail.m_idx);
        g_eventLogger->info("  tail: %s page: %d",
                            fs->get_filename(tf.p->m_fd), tail.m_idx);
      }
    }
    
    m_logfile_group_list.next(ptr);
  }
  
  if(running)
  {
    jam();
    return;
  }
  
  if(outstanding)
  {
    jam();
    signal->theData[0] = LgmanContinueB::STOP_UNDO_LOG;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 1);
    return;
  }
  
  infoEvent("Flushing page cache after undo completion");
  g_eventLogger->info("Flushing page cache after undo completion");

  /**
   * Start flushing pages (local, LCP)
   */
  LcpFragOrd * ord = (LcpFragOrd *)signal->getDataPtr();
  ord->lcpId = m_latest_lcp;
  sendSignal(PGMAN_REF, GSN_LCP_FRAG_ORD, signal, 
	     LcpFragOrd::SignalLength, JBB);
  
  EndLcpReq* req= (EndLcpReq*)signal->getDataPtr();
  req->senderData = 0;
  req->senderRef = reference();
  req->backupId = m_latest_lcp;
  sendSignal(PGMAN_REF, GSN_END_LCP_REQ, signal, 
	     EndLcpReq::SignalLength, JBB);
}

void
Lgman::execEND_LCP_CONF(Signal* signal)
{
  {
    Dbtup_client tup(this, m_tup);
    tup.disk_restart_undo(signal, 0, File_formats::Undofile::UNDO_END, 0, 0);
    jamEntry();
  }
  
  /**
   * pgman has completed flushing all pages
   *
   *   insert "fake" LCP record preventing undo to be "rerun"
   */
  Uint32 undo[3];
  undo[0] = m_latest_lcp;
  undo[1] = (0 << 16) | 0;
  undo[2] = (File_formats::Undofile::UNDO_LCP_FIRST << 16 ) 
    | (sizeof(undo) >> 2);
  
  Ptr<Logfile_group> ptr;
  ndbrequire(m_logfile_group_list.first(ptr));

  Uint64 last_lsn= m_last_lsn;
  if(ptr.p->m_last_lsn == last_lsn
#ifdef VM_TRACE
     && ((rand() % 100) > 50)
#endif
     )
  {
    undo[2] |= File_formats::Undofile::UNDO_NEXT_LSN << 16;
    Uint32 *dst= get_log_buffer(ptr, sizeof(undo) >> 2);
    memcpy(dst, undo, sizeof(undo));
    ndbrequire(ptr.p->m_free_file_words >= (sizeof(undo) >> 2));
    ptr.p->m_free_file_words -= (sizeof(undo) >> 2);
  }
  else
  {
    Uint32 *dst= get_log_buffer(ptr, (sizeof(undo) >> 2) + 2);      
    * dst++ = (Uint32)(last_lsn >> 32);
    * dst++ = (Uint32)(last_lsn & 0xFFFFFFFF);
    memcpy(dst, undo, sizeof(undo));
    ndbrequire(ptr.p->m_free_file_words >= ((sizeof(undo) >> 2) + 2));
    ptr.p->m_free_file_words -= ((sizeof(undo) >> 2) + 2);
  }
  m_last_lsn = ptr.p->m_last_lsn = last_lsn + 1;

  ptr.p->m_last_synced_lsn = last_lsn;
  while(m_logfile_group_list.next(ptr))
    ptr.p->m_last_synced_lsn = last_lsn;
  
  infoEvent("Flushing complete");
  g_eventLogger->info("Flushing complete");

  signal->theData[0] = reference();
  sendSignal(DBLQH_REF, GSN_START_RECCONF, signal, 1, JBB);
}

#ifdef VM_TRACE
void 
Lgman::validate_logfile_group(Ptr<Logfile_group> ptr, const char * heading)
{
  do 
  {
    if (ptr.p->m_file_pos[HEAD].m_ptr_i == RNIL)
      break;
    
    Uint32 pages = compute_free_file_pages(ptr);
    
    Uint32 group_pages = 
      ((ptr.p->m_free_file_words + File_formats::UNDO_PAGE_WORDS - 1)/ File_formats::UNDO_PAGE_WORDS) ;
    Uint32 last = ptr.p->m_free_file_words % File_formats::UNDO_PAGE_WORDS;
    
    if(! (pages >= group_pages))
    {
      ndbout << heading << " Tail: " << ptr.p->m_file_pos[TAIL] 
	     << " Head: " << ptr.p->m_file_pos[HEAD] 
	     << " free: " << group_pages << "(" << last << ")" 
	     << " found: " << pages;
      for(Uint32 i = 0; i<3; i++)
      {
	ndbout << " - " << ptr.p->m_tail_pos[i];
      }
      ndbout << endl;
      
      ndbrequire(pages >= group_pages);
    }
  } while(0);
}
#endif

void Lgman::execGET_TABINFOREQ(Signal* signal)
{
  jamEntry();

  if(!assembleFragments(signal))
  {
    return;
  }

  GetTabInfoReq * const req = (GetTabInfoReq *)&signal->theData[0];

  const Uint32 reqType = req->requestType & (~GetTabInfoReq::LongSignalConf);
  BlockReference retRef= req->senderRef;
  Uint32 senderData= req->senderData;
  Uint32 tableId= req->tableId;

  if(reqType == GetTabInfoReq::RequestByName)
  {
    jam();
    SectionHandle handle(this, signal);
    releaseSections(handle);

    sendGET_TABINFOREF(signal, req, GetTabInfoRef::NoFetchByName);
    return;
  }

  Logfile_group key;
  key.m_logfile_group_id= tableId;
  Ptr<Logfile_group> ptr;
  m_logfile_group_hash.find(ptr, key);

  if(ptr.p->m_logfile_group_id != tableId)
  {
    jam();

    sendGET_TABINFOREF(signal, req, GetTabInfoRef::InvalidTableId);
    return;
  }


  GetTabInfoConf *conf = (GetTabInfoConf *)&signal->theData[0];

  conf->senderData= senderData;
  conf->tableId= tableId;
  conf->freeWordsHi= (Uint32)(ptr.p->m_free_file_words >> 32);
  conf->freeWordsLo= (Uint32)(ptr.p->m_free_file_words & 0xFFFFFFFF);
  conf->tableType= DictTabInfo::LogfileGroup;
  conf->senderRef= reference();
  sendSignal(retRef, GSN_GET_TABINFO_CONF, signal,
	     GetTabInfoConf::SignalLength, JBB);
}

void Lgman::sendGET_TABINFOREF(Signal* signal,
			       GetTabInfoReq * req,
			       GetTabInfoRef::ErrorCode errorCode)
{
  jamEntry();
  GetTabInfoRef * const ref = (GetTabInfoRef *)&signal->theData[0];
  /**
   * The format of GetTabInfo Req/Ref is the same
   */
  BlockReference retRef = req->senderRef;
  ref->errorCode = errorCode;

  sendSignal(retRef, GSN_GET_TABINFOREF, signal, signal->length(), JBB);
}
