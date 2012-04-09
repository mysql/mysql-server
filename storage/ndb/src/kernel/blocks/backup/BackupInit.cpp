/*
   Copyright (C) 2003-2008 MySQL AB, 2008 Sun Microsystems, Inc.
    All rights reserved. Use is subject to license terms.

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

//****************************************************************************
// 
// NAME
//      Backup - Database backup / restore
//
//===========================================================================
#include "Backup.hpp"

#include <Properties.hpp>
#include <Configuration.hpp>

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
  addRecSignal(GSN_DIH_SCAN_GET_NODES_CONF,
               &Backup::execDIH_SCAN_GET_NODES_CONF);

  addRecSignal(GSN_FSOPENREF, &Backup::execFSOPENREF, true);
  addRecSignal(GSN_FSOPENCONF, &Backup::execFSOPENCONF);

  addRecSignal(GSN_FSCLOSEREF, &Backup::execFSCLOSEREF, true);
  addRecSignal(GSN_FSCLOSECONF, &Backup::execFSCLOSECONF);

  addRecSignal(GSN_FSAPPENDREF, &Backup::execFSAPPENDREF, true);
  addRecSignal(GSN_FSAPPENDCONF, &Backup::execFSAPPENDCONF);

  addRecSignal(GSN_FSREMOVEREF, &Backup::execFSREMOVEREF, true);
  addRecSignal(GSN_FSREMOVECONF, &Backup::execFSREMOVECONF);

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

  addRecSignal(GSN_WAIT_GCP_REF, &Backup::execWAIT_GCP_REF);
  addRecSignal(GSN_WAIT_GCP_CONF, &Backup::execWAIT_GCP_CONF);
  addRecSignal(GSN_BACKUP_LOCK_TAB_CONF, &Backup::execBACKUP_LOCK_TAB_CONF);
  addRecSignal(GSN_BACKUP_LOCK_TAB_REF, &Backup::execBACKUP_LOCK_TAB_REF);

  /**
   * Testing
   */
  addRecSignal(GSN_BACKUP_REF, &Backup::execBACKUP_REF);
  addRecSignal(GSN_BACKUP_CONF, &Backup::execBACKUP_CONF);
  addRecSignal(GSN_BACKUP_ABORT_REP, &Backup::execBACKUP_ABORT_REP);
  addRecSignal(GSN_BACKUP_COMPLETE_REP, &Backup::execBACKUP_COMPLETE_REP);
  
  addRecSignal(GSN_LCP_PREPARE_REQ, &Backup::execLCP_PREPARE_REQ);
  addRecSignal(GSN_END_LCPREQ, &Backup::execEND_LCPREQ);

  addRecSignal(GSN_DBINFO_SCANREQ, &Backup::execDBINFO_SCANREQ);
}
  
Backup::~Backup()
{
}

BLOCK_FUNCTIONS(Backup)

template class ArrayPool<Backup::Page32>;
template class ArrayPool<Backup::Fragment>;

void
Backup::execREAD_CONFIG_REQ(Signal* signal)
{
  const ReadConfigReq * req = (ReadConfigReq*)signal->getDataPtr();
  Uint32 ref = req->senderRef;
  Uint32 senderData = req->senderData;
  ndbrequire(req->noOfParameters == 0);

  const ndb_mgm_configuration_iterator * p = 
    m_ctx.m_config.getOwnConfigIterator();
  ndbrequire(p != 0);

  c_defaults.m_disk_write_speed = 10 * (1024 * 1024);
  c_defaults.m_disk_write_speed_sr = 100 * (1024 * 1024);
  c_defaults.m_disk_synch_size = 4 * (1024 * 1024);
  c_defaults.m_o_direct = true;

  Uint32 noBackups = 0, noTables = 0, noAttribs = 0, noFrags = 0;
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_DB_DISCLESS, 
					&c_defaults.m_diskless));
  ndb_mgm_get_int_parameter(p, CFG_DB_O_DIRECT,
                            &c_defaults.m_o_direct);
  ndb_mgm_get_int_parameter(p, CFG_DB_CHECKPOINT_SPEED_SR,
			    &c_defaults.m_disk_write_speed_sr);
  ndb_mgm_get_int_parameter(p, CFG_DB_CHECKPOINT_SPEED,
			    &c_defaults.m_disk_write_speed);
  ndb_mgm_get_int_parameter(p, CFG_DB_DISK_SYNCH_SIZE,
			    &c_defaults.m_disk_synch_size);
  ndb_mgm_get_int_parameter(p, CFG_DB_COMPRESSED_BACKUP,
			    &c_defaults.m_compressed_backup);
  ndb_mgm_get_int_parameter(p, CFG_DB_COMPRESSED_LCP,
			    &c_defaults.m_compressed_lcp);

  m_backup_report_frequency = 0;
  ndb_mgm_get_int_parameter(p, CFG_DB_BACKUP_REPORT_FREQUENCY, 
			    &m_backup_report_frequency);
  /*
    We adjust the disk speed parameters from bytes per second to rather be
    words per 100 milliseconds. We convert disk synch size from bytes per
    second to words per second.
  */
  c_defaults.m_disk_write_speed /= (4 * 10);
  c_defaults.m_disk_write_speed_sr /= (4 * 10);

  ndb_mgm_get_int_parameter(p, CFG_DB_PARALLEL_BACKUPS, &noBackups);
  //  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_DB_NO_TABLES, &noTables));
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_DICT_TABLE, &noTables));
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_DB_NO_ATTRIBUTES, &noAttribs));
  ndbrequire(!ndb_mgm_get_int_parameter(p, CFG_DIH_FRAG_CONNECT, &noFrags));

  noAttribs++; //RT 527 bug fix

  c_nodePool.setSize(MAX_NDB_NODES);
  c_backupPool.setSize(noBackups + 1);
  c_backupFilePool.setSize(3 * noBackups + 1);
  c_tablePool.setSize(noBackups * noTables + 1);
  c_triggerPool.setSize(noBackups * 3 * noTables);
  c_fragmentPool.setSize(noBackups * noFrags + 1);
  
  Uint32 szDataBuf = (2 * 1024 * 1024);
  Uint32 szLogBuf = (2 * 1024 * 1024);
  Uint32 szWrite = 32768, maxWriteSize = (256 * 1024);
  ndb_mgm_get_int_parameter(p, CFG_DB_BACKUP_DATA_BUFFER_MEM, &szDataBuf);
  ndb_mgm_get_int_parameter(p, CFG_DB_BACKUP_LOG_BUFFER_MEM, &szLogBuf);
  ndb_mgm_get_int_parameter(p, CFG_DB_BACKUP_WRITE_SIZE, &szWrite);
  ndb_mgm_get_int_parameter(p, CFG_DB_BACKUP_MAX_WRITE_SIZE, &maxWriteSize);
  
  c_defaults.m_logBufferSize = szLogBuf;
  c_defaults.m_dataBufferSize = szDataBuf;
  c_defaults.m_minWriteSize = szWrite;
  c_defaults.m_maxWriteSize = maxWriteSize;
  c_defaults.m_lcp_buffer_size = szDataBuf;

  Uint32 szMem = 0;
  ndb_mgm_get_int_parameter(p, CFG_DB_BACKUP_MEM, &szMem);
  Uint32 noPages = (szMem + c_defaults.m_lcp_buffer_size + sizeof(Page32) - 1) 
    / sizeof(Page32);
  // We need to allocate an additional of 2 pages. 1 page because of a bug in
  // ArrayPool and another one for DICTTAINFO.
  c_pagePool.setSize(noPages + NO_OF_PAGES_META_FILE + 2, true); 
  
  { // Init all tables
    SLList<Table> tables(c_tablePool);
    TablePtr ptr;
    while(tables.seize(ptr)){
      new (ptr.p) Table(c_fragmentPool);
    }
    tables.release();
  }

  {
    SLList<BackupFile> ops(c_backupFilePool);
    BackupFilePtr ptr;
    while(ops.seize(ptr)){
      new (ptr.p) BackupFile(* this, c_pagePool);
    }
    ops.release();
  }
  
  {
    SLList<BackupRecord> recs(c_backupPool);
    BackupRecordPtr ptr;
    while(recs.seize(ptr)){
      new (ptr.p) BackupRecord(* this, c_tablePool, 
			       c_backupFilePool, c_triggerPool);
    }
    recs.release();
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

