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

#define DBTC_C
#include "Dbtc.hpp"
#include <pc.hpp>
#include <ndb_limits.h>
#include <Properties.hpp>
#include <Configuration.hpp>
#include <EventLogger.hpp>


#define JAM_FILE_ID 349

extern EventLogger * g_eventLogger;

#if defined(VM_TRACE) || defined(ERROR_INSERT)
#define DEBUG_MEM
#endif

#define DEBUG(x) { ndbout << "TC::" << x << endl; }

Uint64 Dbtc::getTransactionMemoryNeed(
    const Uint32 dbtc_instance_count,
    const Uint32 MaxDMLOperationsPerTransaction,
    const Uint32 MaxNoOfConcurrentIndexOperations,
    const Uint32 MaxNoOfConcurrentOperations,
    const Uint32 MaxNoOfConcurrentScans,
    const Uint32 MaxNoOfConcurrentTransactions,
    const Uint32 MaxNoOfFiredTriggers,
    const Uint32 MaxNoOfLocalScans,
    const Uint32 TransactionBufferMemory)
{
  Uint64 byte_count = 0;
  Uint64 byte_count_to = 0; // Only one tc instance do tc take over.
  byte_count += ApiConnectRecord_pool::getMemoryNeed(2 * MaxNoOfConcurrentTransactions);
  byte_count_to += ApiConnectRecord_pool::getMemoryNeed(MaxNoOfConcurrentTransactions);
  byte_count += ApiConTimers_pool::getMemoryNeed((2 * MaxNoOfConcurrentTransactions + 5) / 6);
  byte_count_to += ApiConTimers_pool::getMemoryNeed((MaxNoOfConcurrentTransactions + 5) / 6);
  byte_count += AttributeBuffer_pool::getMemoryNeed(TransactionBufferMemory / (11 * sizeof(Uint32))); // sizeof(AttributeBuffer_pool::T::getSegmentSize()));
  byte_count += CacheRecord_pool::getMemoryNeed(MaxNoOfConcurrentTransactions);
  byte_count += CommitAckMarker_pool::getMemoryNeed(4 * MaxNoOfConcurrentTransactions);
  byte_count_to += CommitAckMarker_pool::getMemoryNeed(2 * MaxNoOfConcurrentTransactions);
  byte_count += CommitAckMarkerBuffer_pool::getMemoryNeed(8 * MaxNoOfConcurrentTransactions);
  byte_count_to += CommitAckMarkerBuffer_pool::getMemoryNeed(4 * MaxNoOfConcurrentTransactions);
  byte_count += TcConnectRecord_pool::getMemoryNeed(MaxNoOfConcurrentOperations + 16 + MaxNoOfConcurrentTransactions);
  byte_count_to += TcConnectRecord_pool::getMemoryNeed(MaxDMLOperationsPerTransaction);
  byte_count += TcFiredTriggerData_pool::getMemoryNeed(MaxNoOfFiredTriggers);
  byte_count += TcIndexOperation_pool::getMemoryNeed(MaxNoOfConcurrentIndexOperations);
  byte_count += ScanFragLocation_pool::getMemoryNeed(0);
  byte_count += ScanFragRec_pool::getMemoryNeed(MaxNoOfLocalScans);
  byte_count += ScanRecord_pool::getMemoryNeed(MaxNoOfConcurrentScans);
  byte_count += GcpRecord_pool::getMemoryNeed(1); // ZGCP_FILESIZE);

  Uint64 byte_total = dbtc_instance_count * byte_count + byte_count_to;

#ifdef DEBUG_MEM
  g_eventLogger->info("MaxDMLOperationsPerTransaction: %u",
      MaxDMLOperationsPerTransaction);
  g_eventLogger->info("MaxNoOfConcurrentIndexOperations: %u",
      MaxNoOfConcurrentIndexOperations);
  g_eventLogger->info("MaxNoOfConcurrentOperations: %u",
      MaxNoOfConcurrentOperations);
  g_eventLogger->info("MaxNoOfConcurrentScans: %u",
      MaxNoOfConcurrentScans);
  g_eventLogger->info("MaxNoOfConcurrentTransactions: %u",
      MaxNoOfConcurrentTransactions);
  g_eventLogger->info("MaxNoOfFiredTriggers: %u", MaxNoOfFiredTriggers);
  g_eventLogger->info("MaxNoOfLocalScans: %u", MaxNoOfLocalScans);
  g_eventLogger->info("TransactionBufferMemory: %u bytes %llu segments",
      TransactionBufferMemory,
      AttributeBuffer_pool::getMemoryNeed(
        TransactionBufferMemory /
          (sizeof(AttributeBufferSegment) * sizeof(Uint32))));
  g_eventLogger->info("Dbtc instances %u", dbtc_instance_count);
  g_eventLogger->info("Dbtc transaction memory per instance %llu bytes",
      byte_count);
  g_eventLogger->info("Dbtc transaction memory extra for take over %llu",
      byte_count_to);
  g_eventLogger->info("Dbtc total transaction memory %llu bytes %llu pages",
      byte_total,
      byte_total / 32768);
#endif

  return byte_total;
}

void Dbtc::initData() 
{
  capiConnectFilesize = ZAPI_CONNECT_FILESIZE;
  chostFilesize = MAX_NODES;
  cscanrecFileSize = ZSCANREC_FILE_SIZE;
  ctabrecFilesize = ZTABREC_FILESIZE;
  cdihblockref = DBDIH_REF;
  cspjInstanceRR = 1;
  m_load_balancer_location = 0;

  c_lqhkeyconf_direct_sent = 0;
 
  // Records with constant sizes
  tcFailRecord = (TcFailRecord*)allocRecord("TcFailRecord",
					    sizeof(TcFailRecord), 1);

  // Variables
  ctcTimer = 0;

  // Trigger and index pools
  c_theDefinedTriggerPool.setSize(c_maxNumberOfDefinedTriggers);
  c_theIndexPool.setSize(c_maxNumberOfIndexes);
  c_firedTriggerHash.setSize(c_maxNumberOfFiredTriggers);
}//Dbtc::initData()

void Dbtc::initRecords() 
{
  void *p;
#if defined(USE_INIT_GLOBAL_VARIABLES)
  {
    void* tmp[] = { &tcConnectptr,
		    &hostptr,
		    &timeOutptr,
		    &scanFragptr, 
                    &tcNodeFailptr }; 
    init_global_ptrs(tmp, sizeof(tmp)/sizeof(tmp[0]));
  }
#endif
  // Records with dynamic sizes

  // Init all index records
  TcIndexData_list indexes(c_theIndexPool);
  TcIndexDataPtr iptr;
  while(indexes.seizeFirst(iptr) == true) {
    p= iptr.p;
    new (p) TcIndexData();
  }
  while (indexes.releaseFirst());

  m_commitAckMarkerHash.setSize(capiConnectFilesize);

  hostRecord = (HostRecord*)allocRecord("HostRecord",
					sizeof(HostRecord),
					chostFilesize);

  tableRecord = (TableRecord*)allocRecord("TableRecord",
					  sizeof(TableRecord),
					  ctabrecFilesize);

  Pool_context pc;
  pc.m_block = this;
  c_scan_frag_pool.init(RT_DBTC_SCAN_FRAGMENT, pc, 1, UINT32_MAX);
while(c_scan_frag_pool.startup()); // TODO(wl9756) watchdog
  m_fragLocationPool.init(RT_DBTC_FRAG_LOCATION, pc, 1, UINT32_MAX);
while(m_fragLocationPool.startup()); // TODO(wl9756) watchdog
  m_commitAckMarkerPool.init(CommitAckMarker::TYPE_ID, pc, 1000, UINT32_MAX);
while(m_commitAckMarkerPool.startup()); // TODO(wl9756) watchdog
  c_theIndexOperationPool.init(TcIndexOperation::TYPE_ID, pc, 10000, UINT32_MAX);
while(c_theIndexOperationPool.startup()); // TODO(wl9756) watchdog
  tcConnectRecord.init(TcConnectRecord::TYPE_ID, pc, 10000 + ctcConnectFailCount, UINT32_MAX);
while(tcConnectRecord.startup()); // TODO(wl9756) watchdog
  c_apiConTimersPool.init(ApiConTimers::TYPE_ID, pc, (1000 + capiConnectFailCount + 5)/6, UINT32_MAX);
while(c_apiConTimersPool.startup()); // TODO(wl9756) watchdog
  c_apiConTimersList.init();
  c_cacheRecordPool.init(CacheRecord::TYPE_ID, pc, 1, UINT32_MAX);
while(c_cacheRecordPool.startup()); // TODO(wl9756) watchdog
  c_gcpRecordPool.init(GcpRecord::TYPE_ID, pc, 1, UINT32_MAX);
while(c_gcpRecordPool.startup()); // TODO(wl9756) watchdog
  c_gcpRecordList.init();
  GcpRecordPtr gcpRecordptr;
  // Make sure that there is at least one page of GcpRecord
  ndbrequire(c_gcpRecordPool.seize(gcpRecordptr));
  c_gcpRecordPool.release(gcpRecordptr);
  c_theFiredTriggerPool.init(TcFiredTriggerData::TYPE_ID, pc, 10000, UINT32_MAX);
while(c_theFiredTriggerPool.startup()); // TODO(wl9756) watchdog
  c_theCommitAckMarkerBufferPool.init(RT_DBTC_COMMIT_ACK_MARKER_BUFFER, pc, 1000, UINT32_MAX);
while(c_theCommitAckMarkerBufferPool.startup()); // TODO(wl9756) watchdog
  c_theAttributeBufferPool.init(RT_DBTC_ATTRIBUTE_BUFFER, pc, 10000, UINT32_MAX);
while(c_theAttributeBufferPool.startup()); // TODO(wl9756) watchdog
  c_apiConnectRecordPool.init(ApiConnectRecord::TYPE_ID, pc, 1000 + capiConnectFailCount, UINT32_MAX);
while(c_apiConnectRecordPool.startup()); // TODO(wl9756) watchdog
  scanRecordPool.init(ScanRecord::TYPE_ID, pc, 0, UINT32_MAX);
while(scanRecordPool.startup()); // TODO(wl9757) watchdog
}//Dbtc::initRecords()

bool
Dbtc::getParam(const char* name, Uint32* count)
{
  if (name != NULL && count != NULL)
  {
    /* FragmentInfoPool
     * We increase the size of the fragment info pool
     * to handle fragmented SCANTABREQ signals from 
     * the API
     */
    if (strcmp(name, "FragmentInfoPool") == 0)
    {
      /* Worst case is each API node sending a 
       * single fragmented request concurrently
       * This could change in future if APIs can
       * interleave fragments from different 
       * requests
       */
      *count= MAX_NODES + 10;
      return true;
    }
  }
  return false;
}

Dbtc::Dbtc(Block_context& ctx, Uint32 instanceNo):
  SimulatedBlock(DBTC, ctx, instanceNo),
  c_theDefinedTriggers(c_theDefinedTriggerPool),
  c_firedTriggerHash(c_theFiredTriggerPool),
  c_maxNumberOfDefinedTriggers(0),
  c_maxNumberOfFiredTriggers(0),
  c_theIndexes(c_theIndexPool),
  c_maxNumberOfIndexes(0),
  c_fk_hash(c_fk_pool),
  c_currentApiConTimers(NULL),
  m_commitAckMarkerHash(m_commitAckMarkerPool)
{
  BLOCK_CONSTRUCTOR(Dbtc);
  
  cfreeTcConnectFail.init();

  const ndb_mgm_configuration_iterator * p = 
    ctx.m_config.getOwnConfigIterator();
  ndbrequire(p != 0);

  Uint32 maxNoOfIndexes = 0;
  Uint32 maxNoOfTriggers = 0, maxNoOfFiredTriggers = 0;

  ndb_mgm_get_int_parameter(p, CFG_DICT_TABLE,
			    &maxNoOfIndexes);
  ndb_mgm_get_int_parameter(p, CFG_DB_NO_TRIGGERS, 
			    &maxNoOfTriggers);
  ndb_mgm_get_int_parameter(p, CFG_DB_NO_TRIGGER_OPS, 
			    &maxNoOfFiredTriggers);
  
  c_maxNumberOfIndexes = maxNoOfIndexes;
  c_maxNumberOfDefinedTriggers = maxNoOfTriggers;
  c_maxNumberOfFiredTriggers = maxNoOfFiredTriggers;

  // Transit signals
  addRecSignal(GSN_PACKED_SIGNAL, &Dbtc::execPACKED_SIGNAL); 
  addRecSignal(GSN_ABORTED, &Dbtc::execABORTED);
  addRecSignal(GSN_ATTRINFO, &Dbtc::execATTRINFO);
  addRecSignal(GSN_CONTINUEB, &Dbtc::execCONTINUEB);
  addRecSignal(GSN_KEYINFO, &Dbtc::execKEYINFO);
  addRecSignal(GSN_SCAN_NEXTREQ, &Dbtc::execSCAN_NEXTREQ);
  addRecSignal(GSN_TAKE_OVERTCCONF, &Dbtc::execTAKE_OVERTCCONF);
  addRecSignal(GSN_LQHKEYREF, &Dbtc::execLQHKEYREF);

  // Received signals

  addRecSignal(GSN_DUMP_STATE_ORD, &Dbtc::execDUMP_STATE_ORD);
  addRecSignal(GSN_DBINFO_SCANREQ, &Dbtc::execDBINFO_SCANREQ);
  addRecSignal(GSN_SEND_PACKED, &Dbtc::execSEND_PACKED, true);
  addRecSignal(GSN_SCAN_HBREP, &Dbtc::execSCAN_HBREP);
  addRecSignal(GSN_COMPLETED, &Dbtc::execCOMPLETED);
  addRecSignal(GSN_COMMITTED, &Dbtc::execCOMMITTED);
  addRecSignal(GSN_DIVERIFYCONF, &Dbtc::execDIVERIFYCONF);
  addRecSignal(GSN_GCP_NOMORETRANS, &Dbtc::execGCP_NOMORETRANS);
  addRecSignal(GSN_LQHKEYCONF, &Dbtc::execLQHKEYCONF);
  addRecSignal(GSN_NDB_STTOR, &Dbtc::execNDB_STTOR);
  addRecSignal(GSN_READ_NODESCONF, &Dbtc::execREAD_NODESCONF);
  addRecSignal(GSN_READ_NODESREF, &Dbtc::execREAD_NODESREF);
  addRecSignal(GSN_STTOR, &Dbtc::execSTTOR);
  addRecSignal(GSN_TC_COMMITREQ, &Dbtc::execTC_COMMITREQ);
  addRecSignal(GSN_TC_CLOPSIZEREQ, &Dbtc::execTC_CLOPSIZEREQ);
  addRecSignal(GSN_TCGETOPSIZEREQ, &Dbtc::execTCGETOPSIZEREQ);
  addRecSignal(GSN_TCKEYREQ, &Dbtc::execTCKEYREQ);
  addRecSignal(GSN_TCRELEASEREQ, &Dbtc::execTCRELEASEREQ);
  addRecSignal(GSN_TCSEIZEREQ, &Dbtc::execTCSEIZEREQ);
  addRecSignal(GSN_TCROLLBACKREQ, &Dbtc::execTCROLLBACKREQ);
  addRecSignal(GSN_TC_HBREP, &Dbtc::execTC_HBREP);
  addRecSignal(GSN_TC_SCHVERREQ, &Dbtc::execTC_SCHVERREQ);
  addRecSignal(GSN_TAB_COMMITREQ, &Dbtc::execTAB_COMMITREQ);
  addRecSignal(GSN_SCAN_TABREQ, &Dbtc::execSCAN_TABREQ);
  addRecSignal(GSN_SCAN_FRAGCONF, &Dbtc::execSCAN_FRAGCONF);
  addRecSignal(GSN_SCAN_FRAGREF, &Dbtc::execSCAN_FRAGREF);
  addRecSignal(GSN_READ_CONFIG_REQ, &Dbtc::execREAD_CONFIG_REQ, true);
  addRecSignal(GSN_LQH_TRANSCONF, &Dbtc::execLQH_TRANSCONF);
  addRecSignal(GSN_COMPLETECONF, &Dbtc::execCOMPLETECONF);
  addRecSignal(GSN_COMMITCONF, &Dbtc::execCOMMITCONF);
  addRecSignal(GSN_ABORTCONF, &Dbtc::execABORTCONF);
  addRecSignal(GSN_NODE_FAILREP, &Dbtc::execNODE_FAILREP);
  addRecSignal(GSN_INCL_NODEREQ, &Dbtc::execINCL_NODEREQ);
  addRecSignal(GSN_TIME_SIGNAL, &Dbtc::execTIME_SIGNAL);
  addRecSignal(GSN_API_FAILREQ, &Dbtc::execAPI_FAILREQ);

  addRecSignal(GSN_TC_COMMIT_ACK, &Dbtc::execTC_COMMIT_ACK);
  addRecSignal(GSN_ABORT_ALL_REQ, &Dbtc::execABORT_ALL_REQ);

  addRecSignal(GSN_CREATE_TRIG_IMPL_REQ, &Dbtc::execCREATE_TRIG_IMPL_REQ);
  addRecSignal(GSN_DROP_TRIG_IMPL_REQ, &Dbtc::execDROP_TRIG_IMPL_REQ);
  addRecSignal(GSN_FIRE_TRIG_ORD, &Dbtc::execFIRE_TRIG_ORD);
  addRecSignal(GSN_TRIG_ATTRINFO, &Dbtc::execTRIG_ATTRINFO);
  
  addRecSignal(GSN_CREATE_INDX_IMPL_REQ, &Dbtc::execCREATE_INDX_IMPL_REQ);
  addRecSignal(GSN_DROP_INDX_IMPL_REQ, &Dbtc::execDROP_INDX_IMPL_REQ);
  addRecSignal(GSN_TCINDXREQ, &Dbtc::execTCINDXREQ);
  addRecSignal(GSN_INDXKEYINFO, &Dbtc::execINDXKEYINFO);
  addRecSignal(GSN_INDXATTRINFO, &Dbtc::execINDXATTRINFO);
  addRecSignal(GSN_ALTER_INDX_IMPL_REQ, &Dbtc::execALTER_INDX_IMPL_REQ);

  addRecSignal(GSN_TRANSID_AI_R, &Dbtc::execTRANSID_AI_R);
  addRecSignal(GSN_KEYINFO20_R, &Dbtc::execKEYINFO20_R);
  addRecSignal(GSN_SIGNAL_DROPPED_REP, &Dbtc::execSIGNAL_DROPPED_REP, true);

  // Index table lookup
  addRecSignal(GSN_TCKEYCONF, &Dbtc::execTCKEYCONF);
  addRecSignal(GSN_TCKEYREF, &Dbtc::execTCKEYREF);
  addRecSignal(GSN_TRANSID_AI, &Dbtc::execTRANSID_AI);
  addRecSignal(GSN_TCROLLBACKREP, &Dbtc::execTCROLLBACKREP);
  
  //addRecSignal(GSN_CREATE_TAB_REQ, &Dbtc::execCREATE_TAB_REQ);
  addRecSignal(GSN_DROP_TAB_REQ, &Dbtc::execDROP_TAB_REQ);
  addRecSignal(GSN_PREP_DROP_TAB_REQ, &Dbtc::execPREP_DROP_TAB_REQ);
  
  addRecSignal(GSN_ALTER_TAB_REQ, &Dbtc::execALTER_TAB_REQ);
  addRecSignal(GSN_ROUTE_ORD, &Dbtc::execROUTE_ORD);
  addRecSignal(GSN_TCKEY_FAILREFCONF_R, &Dbtc::execTCKEY_FAILREFCONF_R);

  addRecSignal(GSN_FIRE_TRIG_REF, &Dbtc::execFIRE_TRIG_REF);
  addRecSignal(GSN_FIRE_TRIG_CONF, &Dbtc::execFIRE_TRIG_CONF);

  addRecSignal(GSN_CREATE_FK_IMPL_REQ, &Dbtc::execCREATE_FK_IMPL_REQ);
  addRecSignal(GSN_DROP_FK_IMPL_REQ, &Dbtc::execDROP_FK_IMPL_REQ);

  addRecSignal(GSN_SCAN_TABREF, &Dbtc::execSCAN_TABREF);
  addRecSignal(GSN_SCAN_TABCONF, &Dbtc::execSCAN_TABCONF);
  addRecSignal(GSN_KEYINFO20, &Dbtc::execKEYINFO20);

  hostRecord = 0;
  tableRecord = 0;
  tcFailRecord = 0;
  cpackedListIndex = 0;
  c_ongoing_take_over_cnt = 0;

  hostRecord = 0;
  tableRecord = 0;
  tcFailRecord = 0;
  m_deferred_enabled = ~Uint32(0);
  m_max_writes_per_trans = ~Uint32(0);

  c_transient_pools[DBTC_ATTRIBUTE_BUFFER_TRANSIENT_POOL_INDEX] =
    &c_theAttributeBufferPool;
  c_transient_pools[DBTC_COMMIT_ACK_MARKER_BUFFER_TRANSIENT_POOL_INDEX] =
    &c_theCommitAckMarkerBufferPool;
  c_transient_pools[DBTC_FIRED_TRIGGER_DATA_TRANSIENT_POOL_INDEX] =
    &c_theFiredTriggerPool;
  c_transient_pools[DBTC_INDEX_OPERATION_TRANSIENT_POOL_INDEX] =
    &c_theIndexOperationPool;
  c_transient_pools[DBTC_API_CONNECT_TIMERS_TRANSIENT_POOL_INDEX] =
    &c_apiConTimersPool;
  c_transient_pools[DBTC_FRAG_LOCATION_TRANSIENT_POOL_INDEX] =
    &m_fragLocationPool;
  c_transient_pools[DBTC_API_CONNECT_RECORD_TRANSIENT_POOL_INDEX] =
    &c_apiConnectRecordPool;
  c_transient_pools[DBTC_CONNECT_RECORD_TRANSIENT_POOL_INDEX] =
    &tcConnectRecord;
  c_transient_pools[DBTC_CACHE_RECORD_TRANSIENT_POOL_INDEX] =
    &c_cacheRecordPool;
  c_transient_pools[DBTC_GCP_RECORD_TRANSIENT_POOL_INDEX] =
    &c_gcpRecordPool;
  c_transient_pools[DBTC_SCAN_FRAGMENT_TRANSIENT_POOL_INDEX] =
    &c_scan_frag_pool;
  c_transient_pools[DBTC_SCAN_RECORD_TRANSIENT_POOL_INDEX] =
    &scanRecordPool;
  c_transient_pools[DBTC_COMMIT_ACK_MARKER_TRANSIENT_POOL_INDEX] =
    &m_commitAckMarkerPool;
  NDB_STATIC_ASSERT(c_transient_pool_count == 13);
  c_transient_pools_shrinking.clear();
}//Dbtc::Dbtc()

Dbtc::~Dbtc() 
{
  // Records with dynamic sizes
  
  deallocRecord((void **)&hostRecord, "HostRecord",
		sizeof(HostRecord),
		chostFilesize);
  
  deallocRecord((void **)&tableRecord, "TableRecord",
		sizeof(TableRecord),
		ctabrecFilesize);
  
  deallocRecord((void **)&tcFailRecord, "TcFailRecord",
		sizeof(TcFailRecord), 1);
  
}//Dbtc::~Dbtc()

BLOCK_FUNCTIONS(Dbtc)

