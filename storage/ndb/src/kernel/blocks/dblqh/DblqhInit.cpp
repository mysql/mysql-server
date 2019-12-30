/*
   Copyright (c) 2003, 2019, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/


#include <pc.hpp>
#define DBLQH_C
#include "Dblqh.hpp"
#include <ndb_limits.h>
#include "DblqhCommon.hpp"

#define JAM_FILE_ID 452


#define LQH_DEBUG(x) { ndbout << "LQH::" << x << endl; }

void Dblqh::initData() 
{
#ifdef ERROR_INSERT
  c_master_node_id = RNIL;
#endif

  c_num_fragments_created_since_restart = 0;
  c_fragments_in_lcp = 0;

  m_update_size = 0;
  m_insert_size = 0;
  m_delete_size = 0;

  c_gcp_stop_timer = 0;
  c_is_io_lag_reported = false;
  c_wait_lcp_surfacing = false;
  c_executing_redo_log = 0;
  c_start_phase_9_waiting = false;
  c_outstanding_write_local_sysfile = false;
  c_send_gcp_saveref_needed = false;
  m_first_distributed_lcp_started = false;
  m_in_send_next_scan = 0;

  caddfragrecFileSize = ZADDFRAGREC_FILE_SIZE;
  cgcprecFileSize = ZGCPREC_FILE_SIZE;
  chostFileSize = MAX_NDB_NODES;
  clcpFileSize = ZNO_CONCURRENT_LCP;
  clfoFileSize = 0;
  clogFileFileSize = 0;

  NdbLogPartInfo lpinfo(instance());
  clogPartFileSize = lpinfo.partCount;

  cpageRefFileSize = ZPAGE_REF_FILE_SIZE;
  cscanrecFileSize = 0;
  ctabrecFileSize = 0;
  ctcConnectrecFileSize = 0;
  ctcNodeFailrecFileSize = MAX_NDB_NODES;
  cTransactionDeadlockDetectionTimeout = 100;

  addFragRecord = 0;
  gcpRecord = 0;
  hostRecord = 0;
  lcpRecord = 0;
  logPartRecord = 0;
  logFileRecord = 0;
  logFileOperationRecord = 0;
  logPageRecord = 0;
  pageRefRecord = 0;
  tablerec = 0;
  tcConnectionrec = 0;
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

  totalLogFiles = 0;
  logFileInitDone = 0;
  totallogMBytes = 0;
  logMBytesInitDone = 0;
  m_startup_report_frequency = 0;

  c_active_add_frag_ptr_i = RNIL;
  for (Uint32 i = 0; i < 1024; i++) {
    ctransidHash[i] = RNIL;
  }//for

  c_last_force_lcp_time = 0;
  c_free_mb_force_lcp_limit = 16;
  c_free_mb_tail_problem_limit = 4;

  cTotalLqhKeyReqCount = 0;
  c_max_redo_lag = 30; // seconds
  c_max_redo_lag_counter = 3; // 3 strikes and you're out

  c_max_parallel_scans_per_frag = 32;

  c_lcpFragWatchdog.block = this;
  c_lcpFragWatchdog.reset();
  c_lcpFragWatchdog.thread_active = false;

  c_keyOverloads           = 0;
  c_keyOverloadsTcNode     = 0;
  c_keyOverloadsReaderApi  = 0;
  c_keyOverloadsPeerNode   = 0;
  c_keyOverloadsSubscriber = 0;
  c_scanSlowDowns          = 0;

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

  c_current_local_lcp_instance = 0;
  c_local_lcp_started = false;
  c_full_local_lcp_started = false;
  c_current_local_lcp_table_id = 0;
  c_copy_frag_live_node_halted = false;
  c_copy_frag_live_node_performing_halt = false;
  c_tc_connect_rec_copy_frag = RNIL;
  memset(&c_halt_copy_fragreq_save,
         0xFF,
         sizeof(c_halt_copy_fragreq_save));

  c_copy_frag_halted = false;
  c_copy_frag_halt_process_locked = false;
  c_undo_log_overloaded = false;
  c_copy_fragment_in_progress = false;
  c_copy_frag_halt_state = COPY_FRAG_HALT_STATE_IDLE;
  memset(&c_prepare_copy_fragreq_save,
         0xFF,
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
}//Dblqh::initData()

void Dblqh::initRecords() 
{
#if defined(USE_INIT_GLOBAL_VARIABLES)
  {
    void* tmp[] = { 
      &addfragptr,
      &fragptr,
      &prim_tab_fragptr,
      &gcpPtr,
      &lcpPtr,
      &logPartPtr,
      &logFilePtr,
      &lfoPtr,
      &logPagePtr,
      &pageRefPtr,
      &scanptr,
      &tabptr,
      &m_tc_connect_ptr,
    }; 
    init_global_ptrs(tmp, sizeof(tmp)/sizeof(tmp[0]));
  }
#endif
  // Records with dynamic sizes
  addFragRecord = (AddFragRecord*)allocRecord("AddFragRecord",
					      sizeof(AddFragRecord), 
					      caddfragrecFileSize);

  gcpRecord = (GcpRecord*)allocRecord("GcpRecord",
				      sizeof(GcpRecord), 
				      cgcprecFileSize);

  hostRecord = (HostRecord*)allocRecord("HostRecord",
					sizeof(HostRecord), 
					chostFileSize);

  lcpRecord = (LcpRecord*)allocRecord("LcpRecord",
				      sizeof(LcpRecord), 
				      clcpFileSize);

  for(Uint32 i = 0; i<clcpFileSize; i++){
    new (&lcpRecord[i])LcpRecord();
  }

  logPartRecord = (LogPartRecord*)allocRecord("LogPartRecord",
					      sizeof(LogPartRecord), 
					      NDB_MAX_LOG_PARTS);

  logFileRecord = (LogFileRecord*)allocRecord("LogFileRecord",
					      sizeof(LogFileRecord),
					      clogFileFileSize);

  logFileOperationRecord = (LogFileOperationRecord*)
    allocRecord("LogFileOperationRecord", 
		sizeof(LogFileOperationRecord), 
		clfoFileSize);

  {
    AllocChunk chunks[16];
    const Uint32 chunkcnt = allocChunks(chunks, 16, RG_FILE_BUFFERS,
                                        clogPageFileSize, CFG_DB_REDO_BUFFER);

    {
      Ptr<GlobalPage> pagePtr;
      m_shared_page_pool.getPtr(pagePtr, chunks[0].ptrI);
      logPageRecord = (LogPageRecord*)pagePtr.p;
    }

    cfirstfreeLogPage = RNIL;
    clogPageFileSize = 0;
    clogPageCount = 0;
    for (Int32 i = chunkcnt - 1; i >= 0; i--)
    {
      const Uint32 cnt = chunks[i].cnt;
      ndbrequire(cnt != 0);

      Ptr<GlobalPage> pagePtr;
      m_shared_page_pool.getPtr(pagePtr, chunks[i].ptrI);
      LogPageRecord * base = (LogPageRecord*)pagePtr.p;
      ndbrequire(base >= logPageRecord);
      const Uint32 ptrI = Uint32(base - logPageRecord);

      for (Uint32 j = 0; j<cnt; j++)
      {
        refresh_watch_dog();
        base[j].logPageWord[ZNEXT_PAGE] = ptrI + j + 1;
        base[j].logPageWord[ZPOS_IN_FREE_LIST]= 1;
        base[j].logPageWord[ZPOS_IN_WRITING]= 0;
      }

      base[cnt-1].logPageWord[ZNEXT_PAGE] = cfirstfreeLogPage;
      cfirstfreeLogPage = ptrI;

      clogPageCount += cnt;
      if (ptrI + cnt > clogPageFileSize)
        clogPageFileSize = ptrI + cnt;
    }
    cnoOfLogPages = clogPageCount;
  }

#ifndef NO_REDO_PAGE_CACHE
  m_redo_page_cache.m_pool.set((RedoCacheLogPageRecord*)logPageRecord,
                               clogPageFileSize);
  m_redo_page_cache.m_hash.setSize(63);

  const Uint32 * base = (Uint32*)logPageRecord;
  const RedoCacheLogPageRecord* tmp1 = (RedoCacheLogPageRecord*)logPageRecord;
  ndbrequire(&base[ZPOS_PAGE_NO] == &tmp1->m_page_no);
  ndbrequire(&base[ZPOS_PAGE_FILE_NO] == &tmp1->m_file_no);
#endif

#ifndef NO_REDO_OPEN_FILE_CACHE
  m_redo_open_file_cache.m_pool.set(logFileRecord, clogFileFileSize);
#endif

  pageRefRecord = (PageRefRecord*)allocRecord("PageRefRecord",
					      sizeof(PageRefRecord),
					      cpageRefFileSize);

  c_scanRecordPool.setSize(cscanrecFileSize);
  c_scanTakeOverHash.setSize(64);

  tablerec = (Tablerec*)allocRecord("Tablerec",
				    sizeof(Tablerec), 
				    ctabrecFileSize);

  tcConnectionrec = (TcConnectionrec*)allocRecord("TcConnectionrec",
						  sizeof(TcConnectionrec),
						  ctcConnectrecFileSize);
  
  m_commitAckMarkerPool.setSize(ctcConnectrecFileSize);
  m_commitAckMarkerHash.setSize(1024);
  
  tcNodeFailRecord = (TcNodeFailRecord*)allocRecord("TcNodeFailRecord",
						    sizeof(TcNodeFailRecord),
						    ctcNodeFailrecFileSize);
  
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

  // Initialize BAT for interface to file system
  NewVARIABLE* bat = allocateBat(2);
  bat[1].WA = &logPageRecord->logPageWord[0];
  bat[1].nrr = clogPageFileSize;
  bat[1].ClusterSize = sizeof(LogPageRecord);
  bat[1].bits.q = ZTWOLOG_PAGE_SIZE;
  bat[1].bits.v = 5;
}//Dblqh::initRecords()

bool
Dblqh::getParam(const char* name, Uint32* count)
{
  if (name != NULL && count != NULL)
  {
    /* FragmentInfoPool
     * We increase the size of the fragment info pool
     * to handle fragmented SCANFRAGREQ signals from 
     * TC
     */
    if (strcmp(name, "FragmentInfoPool") == 0)
    {
      /* Worst case is every TC block sending
       * a single fragmented request concurrently
       * This could change in future if TCs can
       * interleave fragments from different 
       * requests
       */
      const Uint32 TC_BLOCKS_PER_NODE = 1;
      *count= ((MAX_NDB_NODES -1) * TC_BLOCKS_PER_NODE) + 10;
      return true;
    }
  }
  return false;
}

Dblqh::Dblqh(Block_context& ctx, Uint32 instanceNumber):
  SimulatedBlock(DBLQH, ctx, instanceNumber),
  m_reserved_scans(c_scanRecordPool),
  c_lcp_waiting_fragments(c_fragment_pool),
  c_lcp_restoring_fragments(c_fragment_pool),
  c_lcp_complete_fragments(c_fragment_pool),
  c_queued_lcp_frag_ord(c_fragment_pool),
  m_commitAckMarkerHash(m_commitAckMarkerPool),
  c_scanTakeOverHash(c_scanRecordPool)
{
  BLOCK_CONSTRUCTOR(Dblqh);

  addRecSignal(GSN_LOCAL_LATEST_LCP_ID_REP,
               &Dblqh::execLOCAL_LATEST_LCP_ID_REP);
  addRecSignal(GSN_PACKED_SIGNAL, &Dblqh::execPACKED_SIGNAL);
  addRecSignal(GSN_DEBUG_SIG, &Dblqh::execDEBUG_SIG);
  addRecSignal(GSN_ATTRINFO, &Dblqh::execATTRINFO);
  addRecSignal(GSN_KEYINFO, &Dblqh::execKEYINFO);
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
  addRecSignal(GSN_TIME_SIGNAL,  &Dblqh::execTIME_SIGNAL);
  addRecSignal(GSN_FSSYNCCONF,  &Dblqh::execFSSYNCCONF);
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

  addRecSignal(GSN_READ_PSEUDO_REQ, &Dblqh::execREAD_PSEUDO_REQ);

  addRecSignal(GSN_DEFINE_BACKUP_REF, &Dblqh::execDEFINE_BACKUP_REF);
  addRecSignal(GSN_DEFINE_BACKUP_CONF, &Dblqh::execDEFINE_BACKUP_CONF);

  addRecSignal(GSN_BACKUP_FRAGMENT_REF, &Dblqh::execBACKUP_FRAGMENT_REF);
  addRecSignal(GSN_BACKUP_FRAGMENT_CONF, &Dblqh::execBACKUP_FRAGMENT_CONF);

  addRecSignal(GSN_RESTORE_LCP_REF, &Dblqh::execRESTORE_LCP_REF);
  addRecSignal(GSN_RESTORE_LCP_CONF, &Dblqh::execRESTORE_LCP_CONF);

  addRecSignal(GSN_UPDATE_FRAG_DIST_KEY_ORD, 
	       &Dblqh::execUPDATE_FRAG_DIST_KEY_ORD);
  
  addRecSignal(GSN_PREPARE_COPY_FRAG_REQ,
	       &Dblqh::execPREPARE_COPY_FRAG_REQ);
  
  addRecSignal(GSN_DROP_FRAG_REQ, &Dblqh::execDROP_FRAG_REQ);
  addRecSignal(GSN_DROP_FRAG_REF, &Dblqh::execDROP_FRAG_REF);
  addRecSignal(GSN_DROP_FRAG_CONF, &Dblqh::execDROP_FRAG_CONF);

  addRecSignal(GSN_SUB_GCP_COMPLETE_REP, &Dblqh::execSUB_GCP_COMPLETE_REP);
  addRecSignal(GSN_FSWRITEREQ,
               &Dblqh::execFSWRITEREQ);
  addRecSignal(GSN_DBINFO_SCANREQ, &Dblqh::execDBINFO_SCANREQ);

  addRecSignal(GSN_FIRE_TRIG_REQ, &Dblqh::execFIRE_TRIG_REQ);

  addRecSignal(GSN_LCP_STATUS_CONF, &Dblqh::execLCP_STATUS_CONF);
  addRecSignal(GSN_LCP_STATUS_REF, &Dblqh::execLCP_STATUS_REF);

  addRecSignal(GSN_INFO_GCP_STOP_TIMER, &Dblqh::execINFO_GCP_STOP_TIMER);

  addRecSignal(GSN_READ_LOCAL_SYSFILE_CONF,
               &Dblqh::execREAD_LOCAL_SYSFILE_CONF);
  addRecSignal(GSN_WRITE_LOCAL_SYSFILE_CONF,
               &Dblqh::execWRITE_LOCAL_SYSFILE_CONF);
  addRecSignal(GSN_UNDO_LOG_LEVEL_REP,
               &Dblqh::execUNDO_LOG_LEVEL_REP);
  addRecSignal(GSN_CUT_REDO_LOG_TAIL_REQ,
               &Dblqh::execCUT_REDO_LOG_TAIL_REQ);
  addRecSignal(GSN_COPY_FRAG_NOT_IN_PROGRESS_REP,
               &Dblqh::execCOPY_FRAG_NOT_IN_PROGRESS_REP);
  addRecSignal(GSN_SET_LOCAL_LCP_ID_CONF,
               &Dblqh::execSET_LOCAL_LCP_ID_CONF);
  addRecSignal(GSN_START_NODE_LCP_REQ,
               &Dblqh::execSTART_NODE_LCP_REQ);
  addRecSignal(GSN_START_LOCAL_LCP_ORD,
               &Dblqh::execSTART_LOCAL_LCP_ORD);
  addRecSignal(GSN_START_FULL_LOCAL_LCP_ORD,
               &Dblqh::execSTART_FULL_LOCAL_LCP_ORD);
  addRecSignal(GSN_HALT_COPY_FRAG_REQ,
               &Dblqh::execHALT_COPY_FRAG_REQ);
  addRecSignal(GSN_HALT_COPY_FRAG_CONF,
               &Dblqh::execHALT_COPY_FRAG_CONF);
  addRecSignal(GSN_RESUME_COPY_FRAG_REQ,
               &Dblqh::execRESUME_COPY_FRAG_REQ);
  addRecSignal(GSN_RESUME_COPY_FRAG_CONF,
               &Dblqh::execRESUME_COPY_FRAG_CONF);

  initData();

}//Dblqh::Dblqh()

Dblqh::~Dblqh() 
{
#ifndef NO_REDO_PAGE_CACHE
  m_redo_page_cache.m_pool.clear();
#endif

#ifndef NO_REDO_OPEN_FILE_CACHE
  m_redo_open_file_cache.m_pool.clear();
#endif

  // Records with dynamic sizes
  deallocRecord((void **)&addFragRecord, "AddFragRecord",
		sizeof(AddFragRecord), 
		caddfragrecFileSize);

  deallocRecord((void**)&gcpRecord,
		"GcpRecord",
		sizeof(GcpRecord), 
		cgcprecFileSize);
  
  deallocRecord((void**)&hostRecord,
		"HostRecord",
		sizeof(HostRecord), 
		chostFileSize);
  
  deallocRecord((void**)&lcpRecord,
		"LcpRecord",
		sizeof(LcpRecord), 
		clcpFileSize);

  deallocRecord((void**)&logPartRecord,
		"LogPartRecord",
		sizeof(LogPartRecord), 
		clogPartFileSize);
  
  deallocRecord((void**)&logFileRecord,
		"LogFileRecord",
		sizeof(LogFileRecord),
		clogFileFileSize);

  deallocRecord((void**)&logFileOperationRecord,
		"LogFileOperationRecord", 
		sizeof(LogFileOperationRecord), 
		clfoFileSize);
  
  deallocRecord((void**)&pageRefRecord,
		"PageRefRecord",
		sizeof(PageRefRecord),
		cpageRefFileSize);
  

  deallocRecord((void**)&tablerec,
		"Tablerec",
		sizeof(Tablerec), 
		ctabrecFileSize);
  
  deallocRecord((void**)&tcConnectionrec,
		"TcConnectionrec",
		sizeof(TcConnectionrec),
		ctcConnectrecFileSize);
  
  deallocRecord((void**)&tcNodeFailRecord,
		"TcNodeFailRecord",
		sizeof(TcNodeFailRecord),
		ctcNodeFailrecFileSize);
}//Dblqh::~Dblqh()

BLOCK_FUNCTIONS(Dblqh)

