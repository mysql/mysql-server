/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

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

//****************************************************************************
// 
// NAME
//      Backup - Database backup / restore
//
//===========================================================================
#include "Backup.hpp"

#include <Properties.hpp>
#include <Configuration.hpp>
#include <signaldata/RedoStateRep.hpp>
#include <EventLogger.hpp>
extern EventLogger * g_eventLogger;

#define JAM_FILE_ID 472


//extern const unsigned Ndbcntr::g_sysTableCount;

Backup::Backup(Block_context& ctx, Uint32 instanceNumber) :
  SimulatedBlock(BACKUP, ctx, instanceNumber),
  c_nodes(c_nodePool),
  c_backups(c_backupPool)
{
  BLOCK_CONSTRUCTOR(Backup);
  
  c_masterNodeId = getOwnNodeId();
  
  // Add received signals
  addRecSignal(GSN_READ_CONFIG_REQ, &Backup::execREAD_CONFIG_REQ);
  addRecSignal(GSN_STTOR, &Backup::execSTTOR);
  addRecSignal(GSN_DUMP_STATE_ORD, &Backup::execDUMP_STATE_ORD);
  addRecSignal(GSN_READ_NODESCONF, &Backup::execREAD_NODESCONF);
  addRecSignal(GSN_NODE_FAILREP, &Backup::execNODE_FAILREP);
  addRecSignal(GSN_INCL_NODEREQ, &Backup::execINCL_NODEREQ);
  addRecSignal(GSN_CONTINUEB, &Backup::execCONTINUEB);
  addRecSignal(GSN_READ_CONFIG_REQ, &Backup::execREAD_CONFIG_REQ, true);  

  addRecSignal(GSN_SCAN_HBREP, &Backup::execSCAN_HBREP);
  addRecSignal(GSN_TRANSID_AI, &Backup::execTRANSID_AI);
  addRecSignal(GSN_SCAN_FRAGREF, &Backup::execSCAN_FRAGREF);
  addRecSignal(GSN_SCAN_FRAGCONF, &Backup::execSCAN_FRAGCONF);

  addRecSignal(GSN_BACKUP_TRIG_REQ, &Backup::execBACKUP_TRIG_REQ);
  addRecSignal(GSN_TRIG_ATTRINFO, &Backup::execTRIG_ATTRINFO);
  addRecSignal(GSN_FIRE_TRIG_ORD, &Backup::execFIRE_TRIG_ORD);

  addRecSignal(GSN_LIST_TABLES_CONF, &Backup::execLIST_TABLES_CONF);
  addRecSignal(GSN_GET_TABINFOREF, &Backup::execGET_TABINFOREF);
  addRecSignal(GSN_GET_TABINFO_CONF, &Backup::execGET_TABINFO_CONF);

  addRecSignal(GSN_CREATE_TRIG_IMPL_REF, &Backup::execCREATE_TRIG_IMPL_REF);
  addRecSignal(GSN_CREATE_TRIG_IMPL_CONF, &Backup::execCREATE_TRIG_IMPL_CONF);

  addRecSignal(GSN_DROP_TRIG_IMPL_REF, &Backup::execDROP_TRIG_IMPL_REF);
  addRecSignal(GSN_DROP_TRIG_IMPL_CONF, &Backup::execDROP_TRIG_IMPL_CONF);

  addRecSignal(GSN_DIH_SCAN_TAB_CONF, &Backup::execDIH_SCAN_TAB_CONF);

  addRecSignal(GSN_FSOPENREF, &Backup::execFSOPENREF, true);
  addRecSignal(GSN_FSOPENCONF, &Backup::execFSOPENCONF);

  addRecSignal(GSN_FSCLOSEREF, &Backup::execFSCLOSEREF, true);
  addRecSignal(GSN_FSCLOSECONF, &Backup::execFSCLOSECONF);

  addRecSignal(GSN_FSAPPENDREF, &Backup::execFSAPPENDREF, true);
  addRecSignal(GSN_FSAPPENDCONF, &Backup::execFSAPPENDCONF);

  addRecSignal(GSN_FSREMOVEREF, &Backup::execFSREMOVEREF, true);
  addRecSignal(GSN_FSREMOVECONF, &Backup::execFSREMOVECONF);

  addRecSignal(GSN_FSREADREF, &Backup::execFSREADREF, true);
  addRecSignal(GSN_FSREADCONF, &Backup::execFSREADCONF);

  addRecSignal(GSN_FSWRITEREF, &Backup::execFSWRITEREF, true);
  addRecSignal(GSN_FSWRITECONF, &Backup::execFSWRITECONF);

  /*****/
  addRecSignal(GSN_BACKUP_REQ, &Backup::execBACKUP_REQ);
  addRecSignal(GSN_ABORT_BACKUP_ORD, &Backup::execABORT_BACKUP_ORD);

  addRecSignal(GSN_DEFINE_BACKUP_REQ, &Backup::execDEFINE_BACKUP_REQ);
  addRecSignal(GSN_DEFINE_BACKUP_REF, &Backup::execDEFINE_BACKUP_REF);
  addRecSignal(GSN_DEFINE_BACKUP_CONF, &Backup::execDEFINE_BACKUP_CONF);

  addRecSignal(GSN_START_BACKUP_REQ, &Backup::execSTART_BACKUP_REQ);
  addRecSignal(GSN_START_BACKUP_REF, &Backup::execSTART_BACKUP_REF);
  addRecSignal(GSN_START_BACKUP_CONF, &Backup::execSTART_BACKUP_CONF);
  
  addRecSignal(GSN_BACKUP_FRAGMENT_REQ, &Backup::execBACKUP_FRAGMENT_REQ);
  addRecSignal(GSN_BACKUP_FRAGMENT_REF, &Backup::execBACKUP_FRAGMENT_REF);
  addRecSignal(GSN_BACKUP_FRAGMENT_CONF, &Backup::execBACKUP_FRAGMENT_CONF);

  addRecSignal(GSN_BACKUP_FRAGMENT_COMPLETE_REP,
               &Backup::execBACKUP_FRAGMENT_COMPLETE_REP);
  
  addRecSignal(GSN_STOP_BACKUP_REQ, &Backup::execSTOP_BACKUP_REQ);
  addRecSignal(GSN_STOP_BACKUP_REF, &Backup::execSTOP_BACKUP_REF);
  addRecSignal(GSN_STOP_BACKUP_CONF, &Backup::execSTOP_BACKUP_CONF);
  
  //addRecSignal(GSN_BACKUP_STATUS_REQ, &Backup::execBACKUP_STATUS_REQ);
  //addRecSignal(GSN_BACKUP_STATUS_CONF, &Backup::execBACKUP_STATUS_CONF);
  
  addRecSignal(GSN_UTIL_SEQUENCE_REF, &Backup::execUTIL_SEQUENCE_REF);
  addRecSignal(GSN_UTIL_SEQUENCE_CONF, &Backup::execUTIL_SEQUENCE_CONF);

  addRecSignal(GSN_REDO_STATE_REP, &Backup::execREDO_STATE_REP);

  addRecSignal(GSN_WAIT_GCP_REF, &Backup::execWAIT_GCP_REF);
  addRecSignal(GSN_WAIT_GCP_CONF, &Backup::execWAIT_GCP_CONF);
  addRecSignal(GSN_BACKUP_LOCK_TAB_CONF, &Backup::execBACKUP_LOCK_TAB_CONF);
  addRecSignal(GSN_BACKUP_LOCK_TAB_REF, &Backup::execBACKUP_LOCK_TAB_REF);

  addRecSignal(GSN_RESTORABLE_GCI_REP, &Backup::execRESTORABLE_GCI_REP);
  addRecSignal(GSN_INFORM_BACKUP_DROP_TAB_REQ,
               &Backup::execINFORM_BACKUP_DROP_TAB_REQ);

  addRecSignal(GSN_LCP_STATUS_REQ, &Backup::execLCP_STATUS_REQ);

  /**
   * Testing
   */
  addRecSignal(GSN_BACKUP_REF, &Backup::execBACKUP_REF);
  addRecSignal(GSN_BACKUP_CONF, &Backup::execBACKUP_CONF);
  addRecSignal(GSN_BACKUP_ABORT_REP, &Backup::execBACKUP_ABORT_REP);
  addRecSignal(GSN_BACKUP_COMPLETE_REP, &Backup::execBACKUP_COMPLETE_REP);
  
  addRecSignal(GSN_LCP_PREPARE_REQ, &Backup::execLCP_PREPARE_REQ);
  addRecSignal(GSN_END_LCPREQ, &Backup::execEND_LCPREQ);

  addRecSignal(GSN_SYNC_PAGE_WAIT_REP, &Backup::execSYNC_PAGE_WAIT_REP);
  addRecSignal(GSN_SYNC_PAGE_CACHE_CONF, &Backup::execSYNC_PAGE_CACHE_CONF);
  addRecSignal(GSN_SYNC_EXTENT_PAGES_CONF,
               &Backup::execSYNC_EXTENT_PAGES_CONF);

  addRecSignal(GSN_DBINFO_SCANREQ, &Backup::execDBINFO_SCANREQ);

  addRecSignal(GSN_CHECK_NODE_RESTARTCONF,
               &Backup::execCHECK_NODE_RESTARTCONF);
  {
    CallbackEntry& ce = m_callbackEntry[THE_NULL_CALLBACK];
    ce.m_function = TheNULLCallback.m_callbackFunction;
    ce.m_flags = 0;
  }
  { // 1
    CallbackEntry& ce = m_callbackEntry[SYNC_LOG_LCP_LSN];
    ce.m_function = safe_cast(&Backup::sync_log_lcp_lsn_callback);
    ce.m_flags = 0;
  }
  {
    CallbackTable& ct = m_callbackTable;
    ct.m_count = COUNT_CALLBACKS;
    ct.m_entry = m_callbackEntry;
    m_callbackTableAddr = &ct;
  }
  m_redo_alert_state = RedoStateRep::NO_REDO_ALERT;
  m_local_redo_alert_state = RedoStateRep::NO_REDO_ALERT;
  m_global_redo_alert_state = RedoStateRep::NO_REDO_ALERT;
  m_max_redo_speed_per_sec = Uint64(0);
  NdbTick_Invalidate(&m_lcp_start_time);
  NdbTick_Invalidate(&m_prev_lcp_start_time);
  NdbTick_Invalidate(&m_lcp_current_cut_point);
  m_last_redo_used_in_bytes = Uint64(0);
  m_last_lcp_exec_time_in_ms = Uint64(0);
  m_update_size_lcp[0] = Uint64(0);
  m_update_size_lcp[1] = Uint64(0);
  m_update_size_lcp_last = Uint64(0);
  m_insert_size_lcp[0] = Uint64(0);
  m_insert_size_lcp[1] = Uint64(0);
  m_insert_size_lcp_last = Uint64(0);
  m_delete_size_lcp[0] = Uint64(0);
  m_delete_size_lcp[1] = Uint64(0);
  m_delete_size_lcp_last = Uint64(0);
  m_proposed_disk_write_speed = Uint64(0);
  m_lcp_lag[0] = Int64(0);
  m_lcp_lag[1] = Int64(0);
  m_lcp_timing_counter = Uint64(0);
  m_lcp_change_rate = Uint64(0);
  m_lcp_timing_factor = Uint64(100);
}
  
Backup::~Backup()
{
}

BLOCK_FUNCTIONS(Backup)

void
Backup::execREAD_CONFIG_REQ(Signal* signal)
{
  const ReadConfigReq * req = (ReadConfigReq*)signal->getDataPtr();
  Uint32 ref = req->senderRef;
  Uint32 senderData = req->senderData;
  ndbrequire(req->noOfParameters == 0);
  jamEntry();

  const ndb_mgm_configuration_iterator * p = 
    m_ctx.m_config.getOwnConfigIterator();
  ndbrequire(p != 0);

  c_defaults.m_disk_write_speed_min = 10 * (1024 * 1024);
  c_defaults.m_disk_write_speed_max = 20 * (1024 * 1024);
  c_defaults.m_disk_write_speed_max_other_node_restart = 50 * (1024 * 1024);
  c_defaults.m_disk_write_speed_max_own_restart = 100 * (1024 * 1024);
  c_defaults.m_disk_synch_size = 4 * (1024 * 1024);
  c_defaults.m_o_direct = true;
  c_defaults.m_backup_disk_write_pct = 50;

  Uint32 noBackups = 0, noTables = 0, noFrags = 0;
  Uint32 noDeleteLcpFile = 0;
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_DB_DISCLESS, 
					&c_defaults.m_diskless));
  ndb_mgm_get_int_parameter(p, CFG_DB_O_DIRECT,
                            &c_defaults.m_o_direct);

  ndb_mgm_get_int64_parameter(p, CFG_DB_MIN_DISK_WRITE_SPEED,
			      &c_defaults.m_disk_write_speed_min);
  ndb_mgm_get_int64_parameter(p, CFG_DB_MAX_DISK_WRITE_SPEED,
			      &c_defaults.m_disk_write_speed_max);
  ndb_mgm_get_int64_parameter(p,
                CFG_DB_MAX_DISK_WRITE_SPEED_OTHER_NODE_RESTART,
                &c_defaults.m_disk_write_speed_max_other_node_restart);
  ndb_mgm_get_int64_parameter(p,
                CFG_DB_MAX_DISK_WRITE_SPEED_OWN_RESTART,
                &c_defaults.m_disk_write_speed_max_own_restart);
  ndb_mgm_get_int_parameter(p, CFG_DB_BACKUP_DISK_WRITE_PCT,
                            &c_defaults.m_backup_disk_write_pct);

  ndb_mgm_get_int_parameter(p, CFG_DB_DISK_SYNCH_SIZE,
			    &c_defaults.m_disk_synch_size);
  ndb_mgm_get_int_parameter(p, CFG_DB_COMPRESSED_BACKUP,
			    &c_defaults.m_compressed_backup);
  ndb_mgm_get_int_parameter(p, CFG_DB_COMPRESSED_LCP,
			    &c_defaults.m_compressed_lcp);

  m_enable_partial_lcp = 1; /* Default to enabled */
  ndb_mgm_get_int_parameter(p, CFG_DB_ENABLE_PARTIAL_LCP,
                            &m_enable_partial_lcp);

  m_enable_redo_control = 1; /* Default to enabled */
  ndb_mgm_get_int_parameter(p, CFG_DB_ENABLE_REDO_CONTROL,
                            &m_enable_redo_control);

  m_recovery_work = 60; /* Default to 60% */
  ndb_mgm_get_int_parameter(p, CFG_DB_RECOVERY_WORK, &m_recovery_work);

  m_insert_recovery_work = 40; /* Default to 40% */
  ndb_mgm_get_int_parameter(p,
                            CFG_DB_INSERT_RECOVERY_WORK,
                            &m_insert_recovery_work);

  calculate_real_disk_write_speed_parameters();

  jam();
  m_backup_report_frequency = 0;
  ndb_mgm_get_int_parameter(p, CFG_DB_BACKUP_REPORT_FREQUENCY, 
			    &m_backup_report_frequency);

  ndb_mgm_get_int_parameter(p, CFG_DB_PARALLEL_BACKUPS, &noBackups);
  //  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_DB_NO_TABLES, &noTables));
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_DICT_TABLE, &noTables));
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_DIH_FRAG_CONNECT, &noFrags));
  ndbrequire(!ndb_mgm_get_int_parameter(p,
                                        CFG_LQH_FRAG,
                                        &noDeleteLcpFile));

  ndbrequire(noBackups == 1); /* To make sure we fix things if we allow other values */

  /**
   * On top of Backup records we need for LCP:
   * 1 Backup record
   * 5 files
   *  2 CTL files for prepare and execute
   *  2 Data files for prepare and execute
   *  1 Data file for delete file process
   * 2 tables
   * 2 fragments
   */
  c_nodePool.setSize(MAX_NDB_NODES);
  c_backupPool.setSize(noBackups + 1);
  c_backupFilePool.setSize(3 * noBackups +
                           4 + (2*BackupFormat::NDB_MAX_FILES_PER_LCP));
  c_tablePool.setSize(noBackups * noTables + 2);
  c_triggerPool.setSize(noBackups * 3 * noTables);
  c_fragmentPool.setSize(noBackups * noFrags + 2);
  c_deleteLcpFilePool.setSize(noDeleteLcpFile);

  c_tableMap = (Uint32*)allocRecord("c_tableMap",
                                    sizeof(Uint32),
                                    noBackups * noTables);

  for (Uint32 i = 0; i < (noBackups * noTables); i++)
  {
    c_tableMap[i] = RNIL;
  }

  jam();

  Uint32 szLogBuf = BACKUP_DEFAULT_BUFFER_SIZE;
  Uint32 szWrite = BACKUP_DEFAULT_WRITE_SIZE;
  Uint32 szDataBuf = BACKUP_DEFAULT_BUFFER_SIZE;
  Uint32 maxWriteSize = szDataBuf;

  /**
   * We set the backup data buffer size to 2M as hard coded. We add new code
   * to ensure that we use as little as possible when performing LCP scans.
   * This means that we continue the LCP scan until at least one file is
   * ready to write. But if one file is ready to write and we have written
   * more than 512 kB we will not continue the scan. We will however start
   * a new LCP scan after a wait even if the buffer is full up to 512k.
   * We ignore this check for LCP scans when the REDO log is at alert level.
   * In this case we will continue writing until buffer is full.
   *
   * This behaviour ensures that we sustain optimal predictable latency as
   * long as the REDO log is we are not at risk of running out of REDO log
   * space. The higher buffer space is required to be able to keep up with
   * loading massive amounts of data into NDB even with a very limited REDO
   * log size.
   *
   * The minimum write size specifies the minimum size needed to collect
   * in order to even consider writing the buffer to disk. It is also used
   * when deciding to collect checkpoint data at priority A-level. If we
   * reached the minimum write size we will only collect more checkpoint
   * data at A-level if the REDO log is at any form of alert level.
   *
   * The maximum write size is the maximum size sent in one write to the
   * file system. This is set to the same as the data buffer size. No need
   * to make this configurable.
   *
   * We make the backup data buffer, write size and max write size hard coded.
   * The sizes are large enough to provide enough bandwidth on hard drives.
   * On SSD the defaults will be just fine. By limiting the backup data buffer
   * size we avoid that we spend a lot of CPU resources to fill up the data
   * buffer where there is anyways no room to write it out to the file.
   *
   * ndb_mgm_get_int_parameter(p, CFG_DB_BACKUP_DATA_BUFFER_MEM, &szDataBuf);
   * ndb_mgm_get_int_parameter(p, CFG_DB_BACKUP_WRITE_SIZE, &szWrite);
   * ndb_mgm_get_int_parameter(p, CFG_DB_BACKUP_MAX_WRITE_SIZE, &maxWriteSize);
   */
  if (maxWriteSize < szWrite)
  {
    /**
     * max can't be lower than min
     */
    maxWriteSize = szWrite;
  }
  if ((maxWriteSize % szWrite) != 0)
  {
    /**
     * max needs to be a multiple of min
     */
    maxWriteSize = (maxWriteSize + szWrite - 1) / szWrite;
    maxWriteSize *= szWrite;
  }

  /**
   * Data buffer size must at least be big enough for a max-sized 
   * scan batch.
   */
  ndbrequire(szDataBuf >= (BACKUP_MIN_BUFF_WORDS * 4));
    
  /**
   * add min writesize to buffer size...and the alignment added here and there
   * Need buffer size to be >= max-sized scan batch + min write size
   * to avoid 'deadlock' where there's not enough buffered bytes to
   * write, and too many bytes to fit another batch...
   */
  Uint32 extra = szWrite + 4 * (/* align * 512b */ 128);

  szDataBuf += extra;
  szLogBuf += extra;

  c_defaults.m_logBufferSize = szLogBuf;
  c_defaults.m_dataBufferSize = szDataBuf;
  c_defaults.m_minWriteSize = szWrite;
  c_defaults.m_maxWriteSize = maxWriteSize;
  c_defaults.m_lcp_buffer_size = szDataBuf;

  /* We deprecate the use of BackupMemory, it serves no purpose at all. */
  Uint32 szMem = 0;
  ndb_mgm_get_int_parameter(p, CFG_DB_BACKUP_MEM, &szMem);

  if (szMem != (32 * 1024 * 1024))
  {
    jam();
    g_eventLogger->info("BackupMemory parameter setting ignored,"
                        " BackupMemory deprecated");
  }

  /**
   * We allocate szDataBuf + szLogBuf pages for Backups and
   * szDataBuf * 16 pages for LCPs.
   * We also need pages for 3 CTL files for LCP and one file for
   * delete LCP process (2 per file),
   * for backups the meta data file uses NO_OF_PAGES_META_FILE.
   * We need to allocate an additional of 1 page because of a bug
   * in ArrayPool.
   */
  Uint32 noPages =
    (szDataBuf + sizeof(Page32) - 1) / sizeof(Page32) +
    (szLogBuf + sizeof(Page32) - 1) / sizeof(Page32) +
    ((2 * BackupFormat::NDB_MAX_FILES_PER_LCP) * 
      ((c_defaults.m_lcp_buffer_size + sizeof(Page32) - 1) /
           sizeof(Page32)));

  Uint32 seizeNumPages = noPages + (1*NO_OF_PAGES_META_FILE)+ 9;
  c_pagePool.setSize(seizeNumPages, true);

  jam();

  { // Init all tables
    Table_list tables(c_tablePool);
    TablePtr ptr;
    while (tables.seizeFirst(ptr)){
      new (ptr.p) Table(c_fragmentPool);
      ptr.p->backupPtrI = RNIL;
      ptr.p->tableId = RNIL;
    }
    jam();
    while (tables.releaseFirst())
    {
      ;
    }
    jam();
  }

  {
    BackupFile_list ops(c_backupFilePool);
    BackupFilePtr ptr;
    while (ops.seizeFirst(ptr)){
      new (ptr.p) BackupFile(* this, c_pagePool);
    }
    jam();
    while (ops.releaseFirst())
    {
      ;
    }
    jam();
  }
  
  {
    BackupRecord_sllist recs(c_backupPool);
    BackupRecordPtr ptr;
    while (recs.seizeFirst(ptr)){
      new (ptr.p) BackupRecord(* this, c_tablePool, 
			       c_backupFilePool, c_triggerPool);
    }
    jam();
    while (recs.releaseFirst())
    {
      ;
    }
    jam();
  }

  // Initialize BAT for interface to file system
  {
    Page32Ptr p;
    ndbrequire(c_pagePool.seizeId(p, 0));
    c_startOfPages = (Uint32 *)p.p;
    c_pagePool.release(p);
    
    NewVARIABLE* bat = allocateBat(1);
    bat[0].WA = c_startOfPages;
    bat[0].nrr = c_pagePool.getSize()*sizeof(Page32)/sizeof(Uint32);
  }

  ReadConfigConf * conf = (ReadConfigConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(ref, GSN_READ_CONFIG_CONF, signal, 
	     ReadConfigConf::SignalLength, JBB);
}

/* Broken out in its own routine to enable setting via DUMP command. */
void Backup::calculate_real_disk_write_speed_parameters(void)
{
  if (c_defaults.m_disk_write_speed_max < c_defaults.m_disk_write_speed_min)
  {
    /** 
     * By setting max disk write speed equal or smaller than the minimum
     * we will remove the adaptiveness of the LCP speed.
     */
    jam();
    ndbout << "Setting MaxDiskWriteSpeed to MinDiskWriteSpeed since max < min"
           << endl;
    c_defaults.m_disk_write_speed_max = c_defaults.m_disk_write_speed_min;
  }

  if (c_defaults.m_disk_write_speed_max_other_node_restart <
        c_defaults.m_disk_write_speed_max)
  {
    /** 
     * By setting max disk write speed during restart equal or smaller than
     * the maximum we will remove the extra adaptiveness of the LCP speed
     * at other nodes restarts.
     */
    jam();
    ndbout << "MaxDiskWriteSpeed larger than MaxDiskWriteSpeedOtherNodeRestart"
           << " setting both to MaxDiskWriteSpeed" << endl;
    c_defaults.m_disk_write_speed_max_other_node_restart =
      c_defaults.m_disk_write_speed_max;
  }

  if (c_defaults.m_disk_write_speed_max_own_restart <
        c_defaults.m_disk_write_speed_max_other_node_restart)
  {
    /** 
     * By setting restart disk write speed during our restart equal or
     * smaller than the maximum we will remove the extra adaptiveness of the
     * LCP speed at other nodes restarts.
     */
    jam();
    ndbout << "Setting MaxDiskWriteSpeedOwnRestart to "
           << " MaxDiskWriteSpeedOtherNodeRestart since it was smaller"
           << endl;
    c_defaults.m_disk_write_speed_max_own_restart =
      c_defaults.m_disk_write_speed_max_other_node_restart;
  }

  /*
    We adjust the disk speed parameters from bytes per second to rather be
    words per 100 milliseconds. We convert disk synch size from bytes per
    second to words per second.
  */
  c_defaults.m_disk_write_speed_min /=
    CURR_DISK_SPEED_CONVERSION_FACTOR_TO_SECONDS;
  c_defaults.m_disk_write_speed_max /=
    CURR_DISK_SPEED_CONVERSION_FACTOR_TO_SECONDS;
  c_defaults.m_disk_write_speed_max_other_node_restart /=
    CURR_DISK_SPEED_CONVERSION_FACTOR_TO_SECONDS;
  c_defaults.m_disk_write_speed_max_own_restart /=
    CURR_DISK_SPEED_CONVERSION_FACTOR_TO_SECONDS;

  Uint32 num_ldm_threads = globalData.ndbMtLqhThreads;
  if (num_ldm_threads == 0)
  {
    /* We are running with ndbd binary */
    jam();
    num_ldm_threads = 1;
  }
  c_defaults.m_disk_write_speed_min /= num_ldm_threads;
  c_defaults.m_disk_write_speed_max /= num_ldm_threads;
  c_defaults.m_disk_write_speed_max_other_node_restart /= num_ldm_threads;
  c_defaults.m_disk_write_speed_max_own_restart /= num_ldm_threads;
}

void Backup::restore_disk_write_speed_numbers(void)
{
  c_defaults.m_disk_write_speed_min *=
    CURR_DISK_SPEED_CONVERSION_FACTOR_TO_SECONDS;
  c_defaults.m_disk_write_speed_max *=
    CURR_DISK_SPEED_CONVERSION_FACTOR_TO_SECONDS;
  c_defaults.m_disk_write_speed_max_other_node_restart *=
    CURR_DISK_SPEED_CONVERSION_FACTOR_TO_SECONDS;
  c_defaults.m_disk_write_speed_max_own_restart *=
    CURR_DISK_SPEED_CONVERSION_FACTOR_TO_SECONDS;

  Uint32 num_ldm_threads = globalData.ndbMtLqhThreads;
  if (num_ldm_threads == 0)
  {
    /* We are running with ndbd binary */
    jam();
    num_ldm_threads = 1;
  }

  c_defaults.m_disk_write_speed_min *= num_ldm_threads;
  c_defaults.m_disk_write_speed_max *= num_ldm_threads;
  c_defaults.m_disk_write_speed_max_other_node_restart *= num_ldm_threads;
  c_defaults.m_disk_write_speed_max_own_restart *= num_ldm_threads;
}
