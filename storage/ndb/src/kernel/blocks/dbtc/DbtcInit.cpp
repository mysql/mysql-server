/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

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

#define DBTC_C
#include "Dbtc.hpp"
#include <pc.hpp>
#include <ndb_limits.h>
#include <Properties.hpp>
#include <Configuration.hpp>

#define JAM_FILE_ID 349


#define DEBUG(x) { ndbout << "TC::" << x << endl; }


void Dbtc::initData() 
{
  capiConnectFilesize = ZAPI_CONNECT_FILESIZE;
  ccacheFilesize = ZAPI_CONNECT_FILESIZE;
  chostFilesize = MAX_NODES;
  cgcpFilesize = ZGCP_FILESIZE;
  cscanrecFileSize = ZSCANREC_FILE_SIZE;
  cscanFragrecFileSize = ZSCAN_FRAGREC_FILE_SIZE;
  ctabrecFilesize = ZTABREC_FILESIZE;
  ctcConnectFilesize = ZTC_CONNECT_FILESIZE;
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
  c_theFiredTriggerPool.setSize(c_maxNumberOfFiredTriggers);
  c_theIndexPool.setSize(c_maxNumberOfIndexes);
  c_theIndexOperationPool.setSize(c_maxNumberOfIndexOperations);
  c_theAttributeBufferPool.setSize(c_transactionBufferSpace);
  c_firedTriggerHash.setSize((c_maxNumberOfFiredTriggers+10)/10);
}//Dbtc::initData()

void Dbtc::initRecords() 
{
  void *p;
  // Records with dynamic sizes
  cacheRecord = (CacheRecord*)allocRecord("CacheRecord",
					  sizeof(CacheRecord), 
					  ccacheFilesize);

  apiConnectRecord = (ApiConnectRecord*)allocRecord("ApiConnectRecord",
						    sizeof(ApiConnectRecord),
						    capiConnectFilesize);

  for(unsigned i = 0; i<capiConnectFilesize; i++) {
    p = &apiConnectRecord[i];
    new (p) ApiConnectRecord(c_theFiredTriggerPool, 
			     c_theIndexOperationPool);
  }
  // Init all fired triggers
  TcFiredTriggerData_fifo triggers(c_theFiredTriggerPool);
  FiredTriggerPtr tptr;
  while (triggers.seizeLast(tptr) == true) {
    p= tptr.p;
    new (p) TcFiredTriggerData();
  }
  while (triggers.releaseFirst());
  /*
    The code above temporarily allocates all TcFiredTriggerData records.
    Therefore we need to reset freeMin now, to get meaningful values.
  */
  c_theFiredTriggerPool.resetFreeMin();

  // Init all index records
  TcIndexData_list indexes(c_theIndexPool);
  TcIndexDataPtr iptr;
  while(indexes.seizeFirst(iptr) == true) {
    p= iptr.p;
    new (p) TcIndexData();
  }
  while (indexes.releaseFirst());

  // Init all index operation records
  TcIndexOperation_sllist indexOps(c_theIndexOperationPool);
  TcIndexOperationPtr ioptr;
  while (indexOps.seizeFirst(ioptr) == true) {
    p= ioptr.p;
    new (p) TcIndexOperation(); // TODO : Modify alloc size of c_theAttributeBufferPool
  }
  while (indexOps.releaseFirst());

  c_apiConTimer = (UintR*)allocRecord("ApiConTimer",
				      sizeof(UintR),
				      capiConnectFilesize);
  
  c_apiConTimer_line = (UintR*)allocRecord("ApiConTimer_line",
					   sizeof(UintR),
					   capiConnectFilesize);

  tcConnectRecord = (TcConnectRecord*)allocRecord("TcConnectRecord",
						  sizeof(TcConnectRecord),
						  ctcConnectFilesize);
  
  m_commitAckMarkerPool.setSize(2 * capiConnectFilesize);
  m_commitAckMarkerHash.setSize(1024);
  c_theCommitAckMarkerBufferPool.setSize(4 * capiConnectFilesize);

  hostRecord = (HostRecord*)allocRecord("HostRecord",
					sizeof(HostRecord),
					chostFilesize);

  tableRecord = (TableRecord*)allocRecord("TableRecord",
					  sizeof(TableRecord),
					  ctabrecFilesize);

  scanRecord = (ScanRecord*)allocRecord("ScanRecord",
					sizeof(ScanRecord),
					cscanrecFileSize);


  c_scan_frag_pool.setSize(cscanFragrecFileSize);
  {
    ScanFragRecPtr ptr;
    ScanFragRec_sllist tmp(c_scan_frag_pool);
    while (tmp.seizeFirst(ptr)) {
      new (ptr.p) ScanFragRec();
    }
    while (tmp.releaseFirst());
  }

  for (Uint32 i = 0; i < cscanrecFileSize; i++)
  {
    ScanRecordPtr ptr;
    ptr.i = i;
    ptrAss(ptr, scanRecord);
    new (ptr.p) ScanRecord();
  }
  for (Uint32 i = 0; i < ctcConnectFilesize; i++)
  {
    TcConnectRecordPtr ptr;
    ptr.i = i;
    ptrAss(ptr, tcConnectRecord);
    new (ptr.p) TcConnectRecord();
  }
  while (indexOps.releaseFirst());
  
  gcpRecord = (GcpRecord*)allocRecord("GcpRecord",
				      sizeof(GcpRecord), 
				      cgcpFilesize);

  Pool_context pc;
  pc.m_block = this;
  m_fragLocationPool.init(RT_DBTC_FRAG_LOCATION, pc);
  
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
  c_maxNumberOfIndexOperations(0),
  c_fk_hash(c_fk_pool),
  m_commitAckMarkerHash(m_commitAckMarkerPool)
{
  BLOCK_CONSTRUCTOR(Dbtc);
  
  const ndb_mgm_configuration_iterator * p = 
    ctx.m_config.getOwnConfigIterator();
  ndbrequire(p != 0);

  Uint32 transactionBufferMemory = 0;
  Uint32 maxNoOfIndexes = 0, maxNoOfConcurrentIndexOperations = 0;
  Uint32 maxNoOfTriggers = 0, maxNoOfFiredTriggers = 0;

  ndb_mgm_get_int_parameter(p, CFG_DB_TRANS_BUFFER_MEM,  
			    &transactionBufferMemory);
  ndb_mgm_get_int_parameter(p, CFG_DICT_TABLE,
			    &maxNoOfIndexes);
  ndb_mgm_get_int_parameter(p, CFG_DB_NO_INDEX_OPS, 
			    &maxNoOfConcurrentIndexOperations);
  ndb_mgm_get_int_parameter(p, CFG_DB_NO_TRIGGERS, 
			    &maxNoOfTriggers);
  ndb_mgm_get_int_parameter(p, CFG_DB_NO_TRIGGER_OPS, 
			    &maxNoOfFiredTriggers);
  
  c_transactionBufferSpace = 
    transactionBufferMemory / AttributeBuffer::getSegmentSize();
  c_maxNumberOfIndexes = maxNoOfIndexes;
  c_maxNumberOfIndexOperations = maxNoOfConcurrentIndexOperations;
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

  cacheRecord = 0;
  apiConnectRecord = 0;
  tcConnectRecord = 0;
  hostRecord = 0;
  tableRecord = 0;
  scanRecord = 0;
  gcpRecord = 0;
  tcFailRecord = 0;
  c_apiConTimer = 0;
  c_apiConTimer_line = 0;
  cpackedListIndex = 0;
  c_ongoing_take_over_cnt = 0;

#ifdef VM_TRACE
  {
    void* tmp[] = { &apiConnectptr, 
		    &tcConnectptr,
		    &cachePtr,
		    &hostptr,
		    &timeOutptr,
		    &scanFragptr, 
                    &tcNodeFailptr }; 
    init_globals_list(tmp, sizeof(tmp)/sizeof(tmp[0]));
  }
#endif
  cacheRecord = 0;
  apiConnectRecord = 0;
  tcConnectRecord = 0;
  hostRecord = 0;
  tableRecord = 0;
  scanRecord = 0;
  gcpRecord = 0;
  tcFailRecord = 0;
  c_apiConTimer = 0;
  c_apiConTimer_line = 0;
  m_deferred_enabled = ~Uint32(0);
  m_max_writes_per_trans = ~Uint32(0);
}//Dbtc::Dbtc()

Dbtc::~Dbtc() 
{
  // Records with dynamic sizes
  deallocRecord((void **)&cacheRecord, "CacheRecord",
		sizeof(CacheRecord), 
		ccacheFilesize);
  
  deallocRecord((void **)&apiConnectRecord, "ApiConnectRecord",
		sizeof(ApiConnectRecord),
		capiConnectFilesize);
  
  deallocRecord((void **)&tcConnectRecord, "TcConnectRecord",
		sizeof(TcConnectRecord),
		ctcConnectFilesize);

  deallocRecord((void **)&hostRecord, "HostRecord",
		sizeof(HostRecord),
		chostFilesize);
  
  deallocRecord((void **)&tableRecord, "TableRecord",
		sizeof(TableRecord),
		ctabrecFilesize);
  
  deallocRecord((void **)&scanRecord, "ScanRecord",
		sizeof(ScanRecord),
		cscanrecFileSize);
    
  deallocRecord((void **)&gcpRecord, "GcpRecord",
		sizeof(GcpRecord), 
		cgcpFilesize);
  
  deallocRecord((void **)&tcFailRecord, "TcFailRecord",
		sizeof(TcFailRecord), 1);
  
  deallocRecord((void **)&c_apiConTimer, "ApiConTimer",
		sizeof(UintR),
		capiConnectFilesize);

  deallocRecord((void **)&c_apiConTimer_line, "ApiConTimer",
		sizeof(UintR),
		capiConnectFilesize);
}//Dbtc::~Dbtc()

BLOCK_FUNCTIONS(Dbtc)

