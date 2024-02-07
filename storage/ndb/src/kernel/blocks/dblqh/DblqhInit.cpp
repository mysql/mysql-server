/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

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

#include <pc.hpp>
#include "util/require.h"
#define DBLQH_C
#include <ndb_limits.h>
#include "Dblqh.hpp"
#include "DblqhCommon.hpp"
#include "debugger/EventLogger.hpp"

#define JAM_FILE_ID 452

#define LQH_DEBUG(x) \
  { ndbout << "LQH::" << x << endl; }

Uint64 Dblqh::getTransactionMemoryNeed(
    const Uint32 ldm_instance_count,
    const ndb_mgm_configuration_iterator *mgm_cfg, const bool use_reserved) {
  Uint32 lqh_scan_recs = 0;
  Uint32 lqh_op_recs = 0;
  if (use_reserved) {
    require(!ndb_mgm_get_int_parameter(mgm_cfg, CFG_LDM_RESERVED_OPERATIONS,
                                       &lqh_op_recs));
    require(!ndb_mgm_get_int_parameter(mgm_cfg, CFG_LQH_RESERVED_SCAN_RECORDS,
                                       &lqh_scan_recs));
  } else {
    require(!ndb_mgm_get_int_parameter(mgm_cfg, CFG_LQH_SCAN, &lqh_scan_recs));
    require(
        !ndb_mgm_get_int_parameter(mgm_cfg, CFG_LQH_TC_CONNECT, &lqh_op_recs));
  }
  Uint64 scan_byte_count = 0;
  scan_byte_count += ScanRecord_pool::getMemoryNeed(lqh_scan_recs);
  scan_byte_count *= ldm_instance_count;

  Uint64 op_byte_count = 0;
  op_byte_count += TcConnectionrec_pool::getMemoryNeed(lqh_op_recs);
  op_byte_count *= ldm_instance_count;

  Uint32 lqh_commit_ack_markers = 4096;
  Uint64 commit_ack_marker_byte_count = 0;
  commit_ack_marker_byte_count +=
      CommitAckMarker_pool::getMemoryNeed(lqh_commit_ack_markers);
  commit_ack_marker_byte_count *= ldm_instance_count;
  return (op_byte_count + scan_byte_count + commit_ack_marker_byte_count);
}

void Dblqh::initData() {
#ifdef ERROR_INSERT
  c_master_node_id = RNIL;
#endif

  c_num_fragments_created_since_restart = 0;
  c_fragments_in_lcp = 0;

  m_update_size = 0;
  m_insert_size = 0;
  m_delete_size = 0;

  c_copy_fragment_ongoing = false;
  c_copy_active_ongoing = false;

  c_gcp_stop_timer = 0;
  c_is_io_lag_reported = false;
  c_wait_lcp_surfacing = false;
  c_executing_redo_log = 0;
  c_start_phase_9_waiting = false;
  c_outstanding_write_local_sysfile = false;
  c_send_gcp_saveref_needed = false;
  m_first_distributed_lcp_started = false;
  m_in_send_next_scan = 0;
  m_fragment_lock_status = FRAGMENT_UNLOCKED;
  m_old_fragment_lock_status = FRAGMENT_UNLOCKED;

  if (m_is_query_block) {
    caddfragrecFileSize = 0;
    cgcprecFileSize = 0;
    clcpFileSize = 0;
    cpageRefFileSize = 0;
    clogPartFileSize = 0;
  } else {
    caddfragrecFileSize = ZADDFRAGREC_FILE_SIZE;
    cgcprecFileSize = ZGCPREC_FILE_SIZE;
    clcpFileSize = ZNO_CONCURRENT_LCP;
    cpageRefFileSize = ZPAGE_REF_FILE_SIZE;

    NdbLogPartInfo lpinfo(instance());
    clogPartFileSize = lpinfo.partCount;
  }
  chostFileSize = MAX_NDB_NODES;
  clfoFileSize = 0;
  clogFileFileSize = 0;

  ctabrecFileSize = 0;
  ctcNodeFailrecFileSize = MAX_NDB_NODES;
  cTransactionDeadlockDetectionTimeout = 100;

  addFragRecord = 0;
  gcpRecord = 0;
  hostRecord = 0;
  lcpRecord = 0;
  logPartRecord = 0;
  logFileRecord = 0;
  logFileOperationRecord = 0;
  pageRefRecord = 0;
  tablerec = 0;
  tcNodeFailRecord = 0;

  // Records with constant sizes

  cLqhTimeOutCount = 1;
  cLqhTimeOutCheckCount = 0;
  cpackedListIndex = 0;
  m_backup_ptr = RNIL;

  clogFileSize = 16;
  cmaxLogFilesInPageZero = 40;
  cmaxValidLogFilesInPageZero = cmaxLogFilesInPageZero - 1;

#if defined VM_TRACE || defined ERROR_INSERT
  cmaxLogFilesInPageZero_DUMP = 0;
#endif

#if defined ERROR_INSERT
  delayOpenFilePtrI = 0;
#endif

  c_totalLogFiles = 0;
  c_logFileInitDone = 0;
  c_totallogMBytes = 0;
  c_logMBytesInitDone = 0;
  m_startup_report_frequency = 0;

  c_active_add_frag_ptr_i = RNIL;

  ctransidHash = NULL;
  ctransidHashSize = 0;

  c_last_force_lcp_time = 0;
  c_free_mb_force_lcp_limit = 16;
  c_free_mb_tail_problem_limit = 4;

  cTotalLqhKeyReqCount = 0;
  c_max_redo_lag = 30;         // seconds
  c_max_redo_lag_counter = 3;  // 3 strikes and you're out

  c_max_parallel_scans_per_frag = 32;

  c_lcpFragWatchdog.block = this;
  c_lcpFragWatchdog.reset();
  c_lcpFragWatchdog.thread_active = false;

  c_keyOverloads = 0;
  c_keyOverloadsTcNode = 0;
  c_keyOverloadsReaderApi = 0;
  c_keyOverloadsPeerNode = 0;
  c_keyOverloadsSubscriber = 0;
  c_scanSlowDowns = 0;

  c_fragmentsStarted = 0;
  c_fragmentsStartedWithCopy = 0;

  c_fragCopyTable = RNIL;
  c_fragCopyFrag = RNIL;
  c_fragCopyRowsIns = 0;
  c_fragCopyRowsDel = 0;
  c_fragBytesCopied = 0;

  c_fragmentCopyStart = 0;
  c_fragmentsCopied = 0;
  c_totalCopyRowsIns = 0;
  c_totalCopyRowsDel = 0;
  c_totalBytesCopied = 0;

  c_is_first_gcp_save_started = false;
  c_max_gci_in_lcp = 0;

  c_lcpId_sent_last_LCP_FRAG_ORD = 0;
  c_localLcpId_sent_last_LCP_FRAG_ORD = 0;

  c_current_local_lcp_instance = 0;
  c_local_lcp_started = false;
  c_full_local_lcp_started = false;
  c_current_local_lcp_table_id = 0;
  c_copy_frag_live_node_halted = false;
  c_copy_frag_live_node_performing_halt = false;
  c_tc_connect_rec_copy_frag = RNIL;
  memset(&c_halt_copy_fragreq_save, 0xFF, sizeof(c_halt_copy_fragreq_save));

  c_copy_frag_halted = false;
  c_copy_frag_halt_process_locked = false;
  c_undo_log_overloaded = false;
  c_copy_fragment_in_progress = false;
  c_copy_frag_halt_state = COPY_FRAG_HALT_STATE_IDLE;
  memset(&c_prepare_copy_fragreq_save, 0xFF,
         sizeof(c_prepare_copy_fragreq_save));

  m_node_restart_first_local_lcp_started = false;
  m_node_restart_lcp_second_phase_started = false;
  m_first_activate_fragment_ptr_i = RNIL;
  m_second_activate_fragment_ptr_i = RNIL;
  m_curr_lcp_id = 0;
  m_curr_local_lcp_id = 0;
  m_next_local_lcp_id = 0;
  c_max_gci_in_lcp = 0;
  c_local_lcp_sent_wait_complete_conf = false;
  c_local_lcp_sent_wait_all_complete_lcp_req = false;
  c_localLcpId = 0;
  c_keep_gci_for_lcp = 0;
  c_max_keep_gci_in_lcp = 0;
  c_first_set_min_keep_gci = false;
  m_restart_local_latest_lcp_id = 0;
}  // Dblqh::initData()

void Dblqh::initRecords(const ndb_mgm_configuration_iterator *mgm_cfg,
                        Uint64 logPageFileSize) {
#if defined(USE_INIT_GLOBAL_VARIABLES)
  {
    void *tmp[] = {
        &addfragptr, &fragptr, &prim_tab_fragptr, &gcpPtr,
        &lcpPtr,     &scanptr, &tabptr,           &m_tc_connect_ptr,
    };
    init_global_ptrs(tmp, sizeof(tmp) / sizeof(tmp[0]));
  }
#endif
  // Records with dynamic sizes
  hostRecord = (HostRecord *)allocRecord("HostRecord", sizeof(HostRecord),
                                         chostFileSize);

  if (!m_is_query_block) {
    addFragRecord = (AddFragRecord *)allocRecord(
        "AddFragRecord", sizeof(AddFragRecord), caddfragrecFileSize);

    gcpRecord = (GcpRecord *)allocRecord("GcpRecord", sizeof(GcpRecord),
                                         cgcprecFileSize);

    lcpRecord =
        (LcpRecord *)allocRecord("LcpRecord", sizeof(LcpRecord), clcpFileSize);

    for (Uint32 i = 0; i < clcpFileSize; i++) {
      new (&lcpRecord[i]) LcpRecord();
    }

    logPartRecord = (LogPartRecord *)allocRecord(
        "LogPartRecord", sizeof(LogPartRecord), clogPartFileSize);

    logFileRecord = (LogFileRecord *)allocRecord(
        "LogFileRecord", sizeof(LogFileRecord), clogFileFileSize);

    logFileOperationRecord = (LogFileOperationRecord *)allocRecord(
        "LogFileOperationRecord", sizeof(LogFileOperationRecord), clfoFileSize);

    if (clogPartFileSize == 0) {
      /*
       * If the number of fragment log parts are fewer than number of LDMs,
       * some LDM will not own any log part.
       */
      ndbrequire(logPageFileSize == 0);
      ndbrequire(clogFileFileSize == 0);
    } else {
      ndbrequire(logPageFileSize % clogPartFileSize == 0);
    }
    const Uint32 target_pages_per_logpart =
        clogPartFileSize > 0 ? logPageFileSize / clogPartFileSize : 0;
    Uint32 total_logpart_pages = 0;
    LogPartRecordPtr logPartPtr;
    for (logPartPtr.i = 0; logPartPtr.i < clogPartFileSize; logPartPtr.i++) {
      ptrAss(logPartPtr, logPartRecord);
      new (logPartPtr.p) LogPartRecord();
      AllocChunk chunks[16];
      const Uint32 chunkcnt =
          allocChunks(chunks, 16, RG_FILE_BUFFERS, target_pages_per_logpart,
                      CFG_DB_REDO_BUFFER);
      require(chunkcnt > 0);
      const Uint32 endPageI =
          chunks[chunkcnt - 1].ptrI + chunks[chunkcnt - 1].cnt;
      if (chunkcnt > 1) {
        g_eventLogger->info(
            "Redo log part buffer memory %u was split over %u chunks.",
            logPartPtr.i, chunkcnt);
      }
      Ptr<GlobalPage> pagePtr;
      ndbrequire(m_shared_page_pool.getPtr(pagePtr, chunks[0].ptrI));
      logPartPtr.p->logPageRecord = (LogPageRecord *)pagePtr.p;
      /*
       * Since there can be gaps in page number range, logPageFileSize can be
       * bigger than the number of pages.
       */
      logPartPtr.p->logPageFileSize = endPageI - chunks[0].ptrI;
      logPartPtr.p->firstFreeLogPage = RNIL;
      logPartPtr.p->logPageCount = 0;
      for (Int32 i = chunkcnt - 1; i >= 0; i--) {
        const Uint32 cnt = chunks[i].cnt;
        ndbrequire(cnt != 0);

        Ptr<GlobalPage> pagePtr;
        ndbrequire(m_shared_page_pool.getPtr(pagePtr, chunks[i].ptrI));
        LogPageRecord *base = (LogPageRecord *)pagePtr.p;
        ndbrequire(base >= logPartPtr.p->logPageRecord);
        const Uint32 ptrI = Uint32(base - logPartPtr.p->logPageRecord);

        for (Uint32 j = 0; j < cnt; j++) {
          refresh_watch_dog();
          base[j].logPageWord[ZNEXT_PAGE] = ptrI + j + 1;
          base[j].logPageWord[ZPOS_IN_FREE_LIST] = 1;
          base[j].logPageWord[ZPOS_IN_WRITING] = 0;
        }

        base[cnt - 1].logPageWord[ZNEXT_PAGE] = logPartPtr.p->firstFreeLogPage;
        logPartPtr.p->firstFreeLogPage = ptrI;

        logPartPtr.p->logPageCount += cnt;
      }
      /**
       * We need to have one Redo Page cache per log part. This cache is
       * indexed with the i-value of the page minus the starting page of
       * the Redo Page Cache. It is important to separate those since
       * they can be accessed from multiple threads in parallel.
       */
      logPartPtr.p->noOfFreeLogPages = logPartPtr.p->logPageCount;
      /*
       * Wrap log part pages in ArrayPool for getPtr. May not use seize since
       * there may be holes in array.
       */
      logPartPtr.p->m_redo_page_cache.m_pool.set(
          (RedoCacheLogPageRecord *)&logPartPtr.p->logPageRecord[0],
          logPartPtr.p->logPageFileSize);
      logPartPtr.p->m_redo_page_cache.m_hash.setSize(1023);
      logPartPtr.p->m_redo_page_cache.m_first_page = 0;

      const Uint32 *base = (Uint32 *)logPartPtr.p->logPageRecord;
      const RedoCacheLogPageRecord *tmp1 =
          (RedoCacheLogPageRecord *)logPartPtr.p->logPageRecord;
      ndbrequire(&base[ZPOS_PAGE_NO] == &tmp1->m_page_no);
      ndbrequire(&base[ZPOS_PAGE_FILE_NO] == &tmp1->m_file_no);
      total_logpart_pages += logPartPtr.p->logPageCount;
    }
    if (total_logpart_pages < logPageFileSize) {
      g_eventLogger->warning(
          "Not all redo log buffer memory was allocated, got %u pages of %ju.",
          total_logpart_pages, uintmax_t{logPageFileSize});
    }

    m_redo_open_file_cache.m_pool.set(logFileRecord, clogFileFileSize);

    pageRefRecord = (PageRefRecord *)allocRecord(
        "PageRefRecord", sizeof(PageRefRecord), cpageRefFileSize);

    c_scanTakeOverHash.setSize(128);

    tablerec =
        (Tablerec *)allocRecord("Tablerec", sizeof(Tablerec), ctabrecFileSize);
  } else {
    tablerec = 0;
    ctabrecFileSize = 0;
    pageRefRecord = 0;
    cpageRefFileSize = 0;
    clogFileFileSize = 0;
    addFragRecord = 0;
    caddfragrecFileSize = 0;
    gcpRecord = 0;
    cgcprecFileSize = 0;
    lcpRecord = 0;
    clcpFileSize = 0;
    logPartRecord = 0;
    logFileRecord = 0;
    logFileOperationRecord = 0;
    clogFileFileSize = 0;
    clfoFileSize = 0;
  }
  Pool_context pc;
  pc.m_block = this;

  Uint32 reserveTcConnRecs = 0;
  ndbrequire(!ndb_mgm_get_int_parameter(mgm_cfg, CFG_LDM_RESERVED_OPERATIONS,
                                        &reserveTcConnRecs));

  if (m_is_query_block) {
    reserveTcConnRecs = 200;
  }
  ctcConnectReserved = reserveTcConnRecs;
  ctcNumFree = reserveTcConnRecs;
  tcConnect_pool.init(TcConnectionrec::TYPE_ID, pc, reserveTcConnRecs,
                      ((1 << 28) - 1));
  while (tcConnect_pool.startup()) {
    refresh_watch_dog();
  }

  Uint32 reserveScanRecs = 0;
  ndbrequire(!ndb_mgm_get_int_parameter(mgm_cfg, CFG_LQH_RESERVED_SCAN_RECORDS,
                                        &reserveScanRecs));
  if (m_is_query_block) {
    reserveScanRecs = 1;
  }
  c_scanRecordPool.init(ScanRecord::TYPE_ID, pc, reserveScanRecs, UINT32_MAX);
  while (c_scanRecordPool.startup()) {
    refresh_watch_dog();
  }

  Uint32 reserveCommitAckMarkers = 1024;

  if (m_is_query_block) {
    reserveCommitAckMarkers = 1;
  }
  m_commitAckMarkerPool.init(CommitAckMarker::TYPE_ID, pc,
                             reserveCommitAckMarkers, UINT32_MAX);
  while (m_commitAckMarkerPool.startup()) {
    refresh_watch_dog();
  }
  m_commitAckMarkerHash.setSize(4096);

  tcNodeFailRecord = (TcNodeFailRecord *)allocRecord(
      "TcNodeFailRecord", sizeof(TcNodeFailRecord), ctcNodeFailrecFileSize);

  /*
    ndbout << "FRAGREC SIZE = " << sizeof(Fragrecord) << endl;
    ndbout << "TAB SIZE = " << sizeof(Tablerec) << endl;
    ndbout << "GCP SIZE = " << sizeof(GcpRecord) << endl;
    ndbout << "LCP SIZE = " << sizeof(LcpRecord) << endl;
    ndbout << "LCPLOC SIZE = " << sizeof(LcpLocRecord) << endl;
    ndbout << "LOGPART SIZE = " << sizeof(LogPartRecord) << endl;
    ndbout << "LOGFILE SIZE = " << sizeof(LogFileRecord) << endl;
    ndbout << "TC SIZE = " << sizeof(TcConnectionrec) << endl;
    ndbout << "HOST SIZE = " << sizeof(HostRecord) << endl;
    ndbout << "LFO SIZE = " << sizeof(LogFileOperationRecord) << endl;
    ndbout << "PR SIZE = " << sizeof(PageRefRecord) << endl;
    ndbout << "SCAN SIZE = " << sizeof(ScanRecord) << endl;
  */

  if (!m_is_query_block) {
    LogPartRecordPtr logPartPtr;
    NewVARIABLE *bat = allocateBat(clogPartFileSize);
    // Initialize BAT for interface to file system
    for (logPartPtr.i = 0; logPartPtr.i < clogPartFileSize; logPartPtr.i++) {
      Uint32 i = logPartPtr.i;
      ptrAss(logPartPtr, logPartRecord);
      bat[i].WA = &logPartPtr.p->logPageRecord->logPageWord[0];
      bat[i].nrr = logPartPtr.p->logPageFileSize;
      bat[i].ClusterSize = sizeof(LogPageRecord);
      bat[i].bits.q = ZTWOLOG_PAGE_SIZE;
      bat[i].bits.v = 5;
    }
  }

  ctransidHashSize = tcConnect_pool.getSize();
  if (ctransidHashSize < 4096) {
    ctransidHashSize = 4096;
  }
  ctransidHash =
      (Uint32 *)allocRecord("TransIdHash", sizeof(Uint32), ctransidHashSize);

  for (Uint32 i = 0; i < ctransidHashSize; i++) {
    ctransidHash[i] = RNIL;
  }
}  // Dblqh::initRecords()

bool Dblqh::getParam(const char *name, Uint32 *count) {
  if (name != NULL && count != NULL) {
    /* FragmentInfoPool
     * We increase the size of the fragment info pool
     * to handle fragmented SCANFRAGREQ signals from
     * TC
     */
    if (strcmp(name, "FragmentInfoPool") == 0) {
      /* Worst case is every TC block sending
       * a single fragmented request concurrently
       * This could change in future if TCs can
       * interleave fragments from different
       * requests
       */
      const Uint32 TC_BLOCKS_PER_NODE = 1;
      *count = ((MAX_NDB_NODES - 1) * TC_BLOCKS_PER_NODE) + 10;
      return true;
    }
  }
  return false;
}

Dblqh::Dblqh(Block_context &ctx, Uint32 instanceNumber, Uint32 blockNo)
    : SimulatedBlock(blockNo, ctx, instanceNumber),
      m_reserved_scans(c_scanRecordPool),
      c_scanTakeOverHash(c_scanRecordPool),
      c_lcp_waiting_fragments(c_fragment_pool),
      c_lcp_restoring_fragments(c_fragment_pool),
      c_lcp_complete_fragments(c_fragment_pool),
      c_queued_lcp_frag_ord(c_fragment_pool),
      c_copy_fragment_queue(c_copy_fragment_pool),
      c_copy_active_queue(c_copy_active_pool),
      m_commitAckMarkerHash(m_commitAckMarkerPool) {
  BLOCK_CONSTRUCTOR(Dblqh);

  if (blockNo == DBLQH) {
    addRecSignal(GSN_LOCAL_LATEST_LCP_ID_REP,
                 &Dblqh::execLOCAL_LATEST_LCP_ID_REP);
    addRecSignal(GSN_PACKED_SIGNAL, &Dblqh::execPACKED_SIGNAL);
    addRecSignal(GSN_DEBUG_SIG, &Dblqh::execDEBUG_SIG);
    addRecSignal(GSN_LQHKEYREQ, &Dblqh::execLQHKEYREQ);
    addRecSignal(GSN_LQHKEYREF, &Dblqh::execLQHKEYREF);
    addRecSignal(GSN_COMMIT, &Dblqh::execCOMMIT);
    addRecSignal(GSN_COMPLETE, &Dblqh::execCOMPLETE);
    addRecSignal(GSN_LQHKEYCONF, &Dblqh::execLQHKEYCONF);
#ifdef VM_TRACE
    addRecSignal(GSN_TESTSIG, &Dblqh::execTESTSIG);
#endif
    addRecSignal(GSN_CONTINUEB, &Dblqh::execCONTINUEB);
    addRecSignal(GSN_START_RECREQ, &Dblqh::execSTART_RECREQ);
    addRecSignal(GSN_START_RECCONF, &Dblqh::execSTART_RECCONF);
    addRecSignal(GSN_EXEC_FRAGREQ, &Dblqh::execEXEC_FRAGREQ);
    addRecSignal(GSN_EXEC_FRAGCONF, &Dblqh::execEXEC_FRAGCONF);
    addRecSignal(GSN_EXEC_FRAGREF, &Dblqh::execEXEC_FRAGREF);
    addRecSignal(GSN_START_EXEC_SR, &Dblqh::execSTART_EXEC_SR);
    addRecSignal(GSN_EXEC_SRREQ, &Dblqh::execEXEC_SRREQ);
    addRecSignal(GSN_EXEC_SRCONF, &Dblqh::execEXEC_SRCONF);

    addRecSignal(GSN_ALTER_TAB_REQ, &Dblqh::execALTER_TAB_REQ);

    addRecSignal(GSN_SIGNAL_DROPPED_REP, &Dblqh::execSIGNAL_DROPPED_REP, true);

    // Trigger signals, transit to from TUP
    addRecSignal(GSN_CREATE_TRIG_IMPL_REQ, &Dblqh::execCREATE_TRIG_IMPL_REQ);
    addRecSignal(GSN_CREATE_TRIG_IMPL_CONF, &Dblqh::execCREATE_TRIG_IMPL_CONF);
    addRecSignal(GSN_CREATE_TRIG_IMPL_REF, &Dblqh::execCREATE_TRIG_IMPL_REF);

    addRecSignal(GSN_DROP_TRIG_IMPL_REQ, &Dblqh::execDROP_TRIG_IMPL_REQ);
    addRecSignal(GSN_DROP_TRIG_IMPL_CONF, &Dblqh::execDROP_TRIG_IMPL_CONF);
    addRecSignal(GSN_DROP_TRIG_IMPL_REF, &Dblqh::execDROP_TRIG_IMPL_REF);

    addRecSignal(GSN_BUILD_INDX_IMPL_REF, &Dblqh::execBUILD_INDX_IMPL_REF);
    addRecSignal(GSN_BUILD_INDX_IMPL_CONF, &Dblqh::execBUILD_INDX_IMPL_CONF);

    addRecSignal(GSN_DUMP_STATE_ORD, &Dblqh::execDUMP_STATE_ORD);
    addRecSignal(GSN_NODE_FAILREP, &Dblqh::execNODE_FAILREP);
    addRecSignal(GSN_CHECK_LCP_STOP, &Dblqh::execCHECK_LCP_STOP);
    addRecSignal(GSN_SEND_PACKED, &Dblqh::execSEND_PACKED, true);
    addRecSignal(GSN_TUP_ATTRINFO, &Dblqh::execTUP_ATTRINFO);
    addRecSignal(GSN_READ_CONFIG_REQ, &Dblqh::execREAD_CONFIG_REQ, true);
    addRecSignal(GSN_LQHFRAGREQ, &Dblqh::execLQHFRAGREQ);
    addRecSignal(GSN_LQHADDATTREQ, &Dblqh::execLQHADDATTREQ);
    addRecSignal(GSN_TUP_ADD_ATTCONF, &Dblqh::execTUP_ADD_ATTCONF);
    addRecSignal(GSN_TUP_ADD_ATTRREF, &Dblqh::execTUP_ADD_ATTRREF);
    addRecSignal(GSN_ACCFRAGCONF, &Dblqh::execACCFRAGCONF);
    addRecSignal(GSN_ACCFRAGREF, &Dblqh::execACCFRAGREF);
    addRecSignal(GSN_TUPFRAGCONF, &Dblqh::execTUPFRAGCONF);
    addRecSignal(GSN_TUPFRAGREF, &Dblqh::execTUPFRAGREF);
    addRecSignal(GSN_WAIT_LCP_IDLE_CONF, &Dblqh::execWAIT_LCP_IDLE_CONF);
    addRecSignal(GSN_TAB_COMMITREQ, &Dblqh::execTAB_COMMITREQ);
    addRecSignal(GSN_ACCSEIZECONF, &Dblqh::execACCSEIZECONF);
    addRecSignal(GSN_ACCSEIZEREF, &Dblqh::execACCSEIZEREF);
    addRecSignal(GSN_READ_NODESCONF, &Dblqh::execREAD_NODESCONF);
    addRecSignal(GSN_READ_NODESREF, &Dblqh::execREAD_NODESREF);
    addRecSignal(GSN_STTOR, &Dblqh::execSTTOR);
    addRecSignal(GSN_NDB_STTOR, &Dblqh::execNDB_STTOR);
    addRecSignal(GSN_TUPSEIZECONF, &Dblqh::execTUPSEIZECONF);
    addRecSignal(GSN_TUPSEIZEREF, &Dblqh::execTUPSEIZEREF);
    addRecSignal(GSN_ACCKEYCONF, &Dblqh::execACCKEYCONF);
    addRecSignal(GSN_ACCKEYREF, &Dblqh::execACCKEYREF);
    addRecSignal(GSN_TUPKEYREF, &Dblqh::execTUPKEYREF);
    addRecSignal(GSN_ABORT, &Dblqh::execABORT);
    addRecSignal(GSN_ABORTREQ, &Dblqh::execABORTREQ);
    addRecSignal(GSN_COMMITREQ, &Dblqh::execCOMMITREQ);
    addRecSignal(GSN_COMPLETEREQ, &Dblqh::execCOMPLETEREQ);
#ifdef VM_TRACE
    addRecSignal(GSN_MEMCHECKREQ, &Dblqh::execMEMCHECKREQ);
#endif
    addRecSignal(GSN_SCAN_FRAGREQ, &Dblqh::execSCAN_FRAGREQ);
    addRecSignal(GSN_SCAN_NEXTREQ, &Dblqh::execSCAN_NEXTREQ);
    addRecSignal(GSN_NEXT_SCANCONF, &Dblqh::execNEXT_SCANCONF);
    addRecSignal(GSN_NEXT_SCANREF, &Dblqh::execNEXT_SCANREF);
    addRecSignal(GSN_ACC_CHECK_SCAN, &Dblqh::execACC_CHECK_SCAN);
    addRecSignal(GSN_COPY_FRAGREQ, &Dblqh::execCOPY_FRAGREQ);
    addRecSignal(GSN_COPY_FRAGREF, &Dblqh::execCOPY_FRAGREF);
    addRecSignal(GSN_COPY_FRAGCONF, &Dblqh::execCOPY_FRAGCONF);
    addRecSignal(GSN_COPY_ACTIVEREQ, &Dblqh::execCOPY_ACTIVEREQ);
    addRecSignal(GSN_LQH_TRANSREQ, &Dblqh::execLQH_TRANSREQ);
    addRecSignal(GSN_TRANSID_AI, &Dblqh::execTRANSID_AI);
    addRecSignal(GSN_INCL_NODEREQ, &Dblqh::execINCL_NODEREQ);
    addRecSignal(GSN_LCP_PREPARE_REF, &Dblqh::execLCP_PREPARE_REF);
    addRecSignal(GSN_LCP_PREPARE_CONF, &Dblqh::execLCP_PREPARE_CONF);
    addRecSignal(GSN_END_LCPCONF, &Dblqh::execEND_LCPCONF);
    addRecSignal(GSN_WAIT_COMPLETE_LCP_REQ, &Dblqh::execWAIT_COMPLETE_LCP_REQ);
    addRecSignal(GSN_WAIT_ALL_COMPLETE_LCP_CONF,
                 &Dblqh::execWAIT_ALL_COMPLETE_LCP_CONF);
    addRecSignal(GSN_INFORM_BACKUP_DROP_TAB_CONF,
                 &Dblqh::execINFORM_BACKUP_DROP_TAB_CONF);
    addRecSignal(GSN_LCP_ALL_COMPLETE_CONF, &Dblqh::execLCP_ALL_COMPLETE_CONF);

    addRecSignal(GSN_LCP_FRAG_ORD, &Dblqh::execLCP_FRAG_ORD);

    addRecSignal(GSN_START_FRAGREQ, &Dblqh::execSTART_FRAGREQ);
    addRecSignal(GSN_START_RECREF, &Dblqh::execSTART_RECREF);
    addRecSignal(GSN_GCP_SAVEREQ, &Dblqh::execGCP_SAVEREQ);
    addRecSignal(GSN_FSOPENREF, &Dblqh::execFSOPENREF, true);
    addRecSignal(GSN_FSOPENCONF, &Dblqh::execFSOPENCONF);
    addRecSignal(GSN_FSCLOSECONF, &Dblqh::execFSCLOSECONF);
    addRecSignal(GSN_FSWRITECONF, &Dblqh::execFSWRITECONF);
    addRecSignal(GSN_FSWRITEREF, &Dblqh::execFSWRITEREF, true);
    addRecSignal(GSN_FSREADCONF, &Dblqh::execFSREADCONF);
    addRecSignal(GSN_FSREADREF, &Dblqh::execFSREADREF, true);
    addRecSignal(GSN_ACC_ABORTCONF, &Dblqh::execACC_ABORTCONF);
    addRecSignal(GSN_TIME_SIGNAL, &Dblqh::execTIME_SIGNAL);
    addRecSignal(GSN_FSSYNCCONF, &Dblqh::execFSSYNCCONF);
    addRecSignal(GSN_REMOVE_MARKER_ORD, &Dblqh::execREMOVE_MARKER_ORD);

    addRecSignal(GSN_CREATE_TAB_REQ, &Dblqh::execCREATE_TAB_REQ);
    addRecSignal(GSN_CREATE_TAB_REF, &Dblqh::execCREATE_TAB_REF);
    addRecSignal(GSN_CREATE_TAB_CONF, &Dblqh::execCREATE_TAB_CONF);

    addRecSignal(GSN_PREP_DROP_TAB_REQ, &Dblqh::execPREP_DROP_TAB_REQ);
    addRecSignal(GSN_DROP_TAB_REQ, &Dblqh::execDROP_TAB_REQ);
    addRecSignal(GSN_DROP_TAB_REF, &Dblqh::execDROP_TAB_REF);
    addRecSignal(GSN_DROP_TAB_CONF, &Dblqh::execDROP_TAB_CONF);

    addRecSignal(GSN_LQH_WRITELOG_REQ, &Dblqh::execLQH_WRITELOG_REQ);
    addRecSignal(GSN_TUP_DEALLOCREQ, &Dblqh::execTUP_DEALLOCREQ);

    // TUX
    addRecSignal(GSN_TUXFRAGCONF, &Dblqh::execTUXFRAGCONF);
    addRecSignal(GSN_TUXFRAGREF, &Dblqh::execTUXFRAGREF);
    addRecSignal(GSN_TUX_ADD_ATTRCONF, &Dblqh::execTUX_ADD_ATTRCONF);
    addRecSignal(GSN_TUX_ADD_ATTRREF, &Dblqh::execTUX_ADD_ATTRREF);

    addRecSignal(GSN_DEFINE_BACKUP_REF, &Dblqh::execDEFINE_BACKUP_REF);
    addRecSignal(GSN_DEFINE_BACKUP_CONF, &Dblqh::execDEFINE_BACKUP_CONF);

    addRecSignal(GSN_BACKUP_FRAGMENT_REF, &Dblqh::execBACKUP_FRAGMENT_REF);
    addRecSignal(GSN_BACKUP_FRAGMENT_CONF, &Dblqh::execBACKUP_FRAGMENT_CONF);

    addRecSignal(GSN_RESTORE_LCP_REF, &Dblqh::execRESTORE_LCP_REF);
    addRecSignal(GSN_RESTORE_LCP_CONF, &Dblqh::execRESTORE_LCP_CONF);

    addRecSignal(GSN_UPDATE_FRAG_DIST_KEY_ORD,
                 &Dblqh::execUPDATE_FRAG_DIST_KEY_ORD);

    addRecSignal(GSN_PREPARE_COPY_FRAG_REQ, &Dblqh::execPREPARE_COPY_FRAG_REQ);

    addRecSignal(GSN_DROP_FRAG_REQ, &Dblqh::execDROP_FRAG_REQ);
    addRecSignal(GSN_DROP_FRAG_REF, &Dblqh::execDROP_FRAG_REF);
    addRecSignal(GSN_DROP_FRAG_CONF, &Dblqh::execDROP_FRAG_CONF);

    addRecSignal(GSN_SUB_GCP_COMPLETE_REP, &Dblqh::execSUB_GCP_COMPLETE_REP);
    addRecSignal(GSN_DBINFO_SCANREQ, &Dblqh::execDBINFO_SCANREQ);

    addRecSignal(GSN_FIRE_TRIG_REQ, &Dblqh::execFIRE_TRIG_REQ);

    addRecSignal(GSN_LCP_STATUS_CONF, &Dblqh::execLCP_STATUS_CONF);
    addRecSignal(GSN_LCP_STATUS_REF, &Dblqh::execLCP_STATUS_REF);

    addRecSignal(GSN_INFO_GCP_STOP_TIMER, &Dblqh::execINFO_GCP_STOP_TIMER);

    addRecSignal(GSN_READ_LOCAL_SYSFILE_CONF,
                 &Dblqh::execREAD_LOCAL_SYSFILE_CONF);
    addRecSignal(GSN_WRITE_LOCAL_SYSFILE_CONF,
                 &Dblqh::execWRITE_LOCAL_SYSFILE_CONF);
    addRecSignal(GSN_UNDO_LOG_LEVEL_REP, &Dblqh::execUNDO_LOG_LEVEL_REP);
    addRecSignal(GSN_CUT_REDO_LOG_TAIL_REQ, &Dblqh::execCUT_REDO_LOG_TAIL_REQ);
    addRecSignal(GSN_COPY_FRAG_NOT_IN_PROGRESS_REP,
                 &Dblqh::execCOPY_FRAG_NOT_IN_PROGRESS_REP);
    addRecSignal(GSN_SET_LOCAL_LCP_ID_CONF, &Dblqh::execSET_LOCAL_LCP_ID_CONF);
    addRecSignal(GSN_START_NODE_LCP_REQ, &Dblqh::execSTART_NODE_LCP_REQ);
    addRecSignal(GSN_START_LOCAL_LCP_ORD, &Dblqh::execSTART_LOCAL_LCP_ORD);
    addRecSignal(GSN_START_FULL_LOCAL_LCP_ORD,
                 &Dblqh::execSTART_FULL_LOCAL_LCP_ORD);
    addRecSignal(GSN_HALT_COPY_FRAG_REQ, &Dblqh::execHALT_COPY_FRAG_REQ);
    addRecSignal(GSN_HALT_COPY_FRAG_CONF, &Dblqh::execHALT_COPY_FRAG_CONF);
    addRecSignal(GSN_RESUME_COPY_FRAG_REQ, &Dblqh::execRESUME_COPY_FRAG_REQ);
    addRecSignal(GSN_RESUME_COPY_FRAG_CONF, &Dblqh::execRESUME_COPY_FRAG_CONF);
    m_is_query_block = false;
    m_is_in_query_thread = false;
    m_ldm_instance_used = this;
    m_acc_block = DBACC;
    m_tup_block = DBTUP;
    m_lqh_block = DBLQH;
    m_tux_block = DBTUX;
    m_backup_block = BACKUP;
    m_restore_block = RESTORE;
  } else {
    ndbrequire(blockNo == DBQLQH);
    m_is_query_block = true;
    m_is_in_query_thread = true;
    m_acc_block = DBQACC;
    m_tup_block = DBQTUP;
    m_lqh_block = DBQLQH;
    m_tux_block = DBQTUX;
    m_backup_block = QBACKUP;
    m_restore_block = QRESTORE;
    m_ldm_instance_used = nullptr;
    addRecSignal(GSN_TUP_DEALLOCREQ, &Dblqh::execTUP_DEALLOCREQ);
    addRecSignal(GSN_READ_NODESCONF, &Dblqh::execREAD_NODESCONF);
    addRecSignal(GSN_READ_NODESREF, &Dblqh::execREAD_NODESREF);
    addRecSignal(GSN_LQHKEYREQ, &Dblqh::execLQHKEYREQ);
    addRecSignal(GSN_LQHKEYREF, &Dblqh::execLQHKEYREF);
    addRecSignal(GSN_LQHKEYCONF, &Dblqh::execLQHKEYCONF);
    addRecSignal(GSN_PACKED_SIGNAL, &Dblqh::execPACKED_SIGNAL);
    addRecSignal(GSN_CONTINUEB, &Dblqh::execCONTINUEB);
    addRecSignal(GSN_SIGNAL_DROPPED_REP, &Dblqh::execSIGNAL_DROPPED_REP, true);
    addRecSignal(GSN_DUMP_STATE_ORD, &Dblqh::execDUMP_STATE_ORD);
    addRecSignal(GSN_NODE_FAILREP, &Dblqh::execNODE_FAILREP);
    addRecSignal(GSN_CHECK_LCP_STOP, &Dblqh::execCHECK_LCP_STOP);
    addRecSignal(GSN_SEND_PACKED, &Dblqh::execSEND_PACKED, true);
    addRecSignal(GSN_TUP_ATTRINFO, &Dblqh::execTUP_ATTRINFO);
    addRecSignal(GSN_STTOR, &Dblqh::execSTTOR);
    addRecSignal(GSN_READ_CONFIG_REQ, &Dblqh::execREAD_CONFIG_REQ, true);
    addRecSignal(GSN_ACCSEIZECONF, &Dblqh::execACCSEIZECONF);
    addRecSignal(GSN_ACCSEIZEREF, &Dblqh::execACCSEIZEREF);
    addRecSignal(GSN_TUPSEIZECONF, &Dblqh::execTUPSEIZECONF);
    addRecSignal(GSN_TUPSEIZEREF, &Dblqh::execTUPSEIZEREF);
    addRecSignal(GSN_ACCKEYCONF, &Dblqh::execACCKEYCONF);
    addRecSignal(GSN_ACCKEYREF, &Dblqh::execACCKEYREF);
    addRecSignal(GSN_TUPKEYREF, &Dblqh::execTUPKEYREF);
    addRecSignal(GSN_ABORT, &Dblqh::execABORT);
    addRecSignal(GSN_ABORTREQ, &Dblqh::execABORTREQ);
    addRecSignal(GSN_SCAN_FRAGREQ, &Dblqh::execSCAN_FRAGREQ);
    addRecSignal(GSN_SCAN_NEXTREQ, &Dblqh::execSCAN_NEXTREQ);
    addRecSignal(GSN_NEXT_SCANCONF, &Dblqh::execNEXT_SCANCONF);
    addRecSignal(GSN_NEXT_SCANREF, &Dblqh::execNEXT_SCANREF);
    addRecSignal(GSN_ACC_CHECK_SCAN, &Dblqh::execACC_CHECK_SCAN);
    addRecSignal(GSN_TRANSID_AI, &Dblqh::execTRANSID_AI);
    addRecSignal(GSN_INCL_NODEREQ, &Dblqh::execINCL_NODEREQ);
    addRecSignal(GSN_TIME_SIGNAL, &Dblqh::execTIME_SIGNAL);
    addRecSignal(GSN_DBINFO_SCANREQ, &Dblqh::execDBINFO_SCANREQ);
  }
  m_is_recover_block = false;
  initData();

  init_restart_synch();
  m_restore_mutex = 0;
  m_lock_acc_page_mutex = 0;
  m_lock_tup_page_mutex = 0;
  c_restore_mutex_lqh = 0;
  m_num_recover_active = 0;
  m_num_restore_threads = 0;
  m_num_restores_active = 0;
  m_num_local_restores_active = 0;
  m_num_copy_restores_active = 0;
  m_current_ldm_instance = 0;

  c_transient_pools[DBLQH_OPERATION_RECORD_TRANSIENT_POOL_INDEX] =
      &tcConnect_pool;
  c_transient_pools[DBLQH_SCAN_RECORD_TRANSIENT_POOL_INDEX] = &c_scanRecordPool;
  c_transient_pools[DBLQH_COMMIT_ACK_MARKER_TRANSIENT_POOL_INDEX] =
      &m_commitAckMarkerPool;
  static_assert(c_transient_pool_count == 3);
  c_transient_pools_shrinking.clear();
}  // Dblqh::Dblqh()

Dblqh::~Dblqh() {
  deinit_restart_synch();
  if (!m_is_query_block) {
    NdbMutex_Destroy(m_lock_tup_page_mutex);
    NdbMutex_Destroy(m_lock_acc_page_mutex);
    if (!isNdbMtLqh() || instance() == 1) {
      if ((globalData.ndbMtRecoverThreads + globalData.ndbMtQueryThreads) > 0) {
        NdbMutex_Destroy(m_restore_mutex);
      }
      m_restore_mutex = 0;
      ndbd_free((void *)m_num_recover_active,
                sizeof(Uint32) * (MAX_NDBMT_QUERY_THREADS + 1));
      m_num_recover_active = 0;
    }
    {
      LogPartRecordPtr logPartPtr;
      for (logPartPtr.i = 0; logPartPtr.i < clogPartFileSize; logPartPtr.i++) {
        ptrAss(logPartPtr, logPartRecord);
        logPartPtr.p->m_redo_page_cache.m_pool.clear();
        NdbMutex_Deinit(&logPartPtr.p->m_log_part_mutex);
      }
    }

    m_redo_open_file_cache.m_pool.clear();

    // Records with dynamic sizes
    deallocRecord((void **)&addFragRecord, "AddFragRecord",
                  sizeof(AddFragRecord), caddfragrecFileSize);

    deallocRecord((void **)&gcpRecord, "GcpRecord", sizeof(GcpRecord),
                  cgcprecFileSize);

    deallocRecord((void **)&lcpRecord, "LcpRecord", sizeof(LcpRecord),
                  clcpFileSize);

    deallocRecord((void **)&logPartRecord, "LogPartRecord",
                  sizeof(LogPartRecord), clogPartFileSize);

    deallocRecord((void **)&logFileRecord, "LogFileRecord",
                  sizeof(LogFileRecord), clogFileFileSize);

    deallocRecord((void **)&logFileOperationRecord, "LogFileOperationRecord",
                  sizeof(LogFileOperationRecord), clfoFileSize);

    deallocRecord((void **)&pageRefRecord, "PageRefRecord",
                  sizeof(PageRefRecord), cpageRefFileSize);

    deallocRecord((void **)&tablerec, "Tablerec", sizeof(Tablerec),
                  ctabrecFileSize);
  }

  deallocRecord((void **)&hostRecord, "HostRecord", sizeof(HostRecord),
                chostFileSize);

  deallocRecord((void **)&tcNodeFailRecord, "TcNodeFailRecord",
                sizeof(TcNodeFailRecord), ctcNodeFailrecFileSize);

  deallocRecord((void **)&ctransidHash, "TransIdHash", sizeof(Uint32),
                ctransidHashSize);
}  // Dblqh::~Dblqh()

BLOCK_FUNCTIONS(Dblqh)
