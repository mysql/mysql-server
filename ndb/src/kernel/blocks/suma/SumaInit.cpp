/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include "Suma.hpp"

#include <new>
#include <Properties.hpp>
#include <Configuration.hpp>

SumaParticipant::SumaParticipant(const Configuration & conf) :
  SimulatedBlock(SUMA, conf),
  c_metaSubscribers(c_subscriberPool),
  c_dataSubscribers(c_subscriberPool),
  c_prepDataSubscribers(c_subscriberPool),
  c_removeDataSubscribers(c_subscriberPool),
  c_tables(c_tablePool_),
  c_subscriptions(c_subscriptionPool)
{
  BLOCK_CONSTRUCTOR(SumaParticipant);
  
  /**
   * SUMA participant if
   */
  addRecSignal(GSN_SUB_CREATE_REQ, &SumaParticipant::execSUB_CREATE_REQ);
  addRecSignal(GSN_SUB_REMOVE_REQ, &SumaParticipant::execSUB_REMOVE_REQ);
  addRecSignal(GSN_SUB_START_REQ, &SumaParticipant::execSUB_START_REQ);
  addRecSignal(GSN_SUB_STOP_REQ, &SumaParticipant::execSUB_STOP_REQ);
  addRecSignal(GSN_SUB_SYNC_REQ, &SumaParticipant::execSUB_SYNC_REQ);
  
  addRecSignal(GSN_SUB_STOP_CONF, &SumaParticipant::execSUB_STOP_CONF);
  addRecSignal(GSN_SUB_STOP_REF, &SumaParticipant::execSUB_STOP_REF);

  /**
   * Dict interface
   */
  //addRecSignal(GSN_LIST_TABLES_REF, &SumaParticipant::execLIST_TABLES_REF);
  addRecSignal(GSN_LIST_TABLES_CONF, &SumaParticipant::execLIST_TABLES_CONF);
  //addRecSignal(GSN_GET_TABINFOREF, &SumaParticipant::execGET_TABINFO_REF);
  addRecSignal(GSN_GET_TABINFO_CONF, &SumaParticipant::execGET_TABINFO_CONF);
  addRecSignal(GSN_GET_TABINFOREF, &SumaParticipant::execGET_TABINFOREF);
#if 0
  addRecSignal(GSN_GET_TABLEID_CONF, &SumaParticipant::execGET_TABLEID_CONF);
  addRecSignal(GSN_GET_TABLEID_REF, &SumaParticipant::execGET_TABLEID_REF);
#endif
  /**
   * Dih interface
   */
  //addRecSignal(GSN_DI_FCOUNTREF, &SumaParticipant::execDI_FCOUNTREF);
  addRecSignal(GSN_DI_FCOUNTCONF, &SumaParticipant::execDI_FCOUNTCONF);
  //addRecSignal(GSN_DIGETPRIMREF, &SumaParticipant::execDIGETPRIMREF);
  addRecSignal(GSN_DIGETPRIMCONF, &SumaParticipant::execDIGETPRIMCONF);

  /**
   * Scan interface
   */
  addRecSignal(GSN_SCAN_HBREP, &SumaParticipant::execSCAN_HBREP);
  addRecSignal(GSN_TRANSID_AI, &SumaParticipant::execTRANSID_AI);
  addRecSignal(GSN_SCAN_FRAGREF, &SumaParticipant::execSCAN_FRAGREF);
  addRecSignal(GSN_SCAN_FRAGCONF, &SumaParticipant::execSCAN_FRAGCONF);
#if 0
  addRecSignal(GSN_SUB_SYNC_CONTINUE_REF, 
	       &SumaParticipant::execSUB_SYNC_CONTINUE_REF);
#endif
  addRecSignal(GSN_SUB_SYNC_CONTINUE_CONF, 
	       &SumaParticipant::execSUB_SYNC_CONTINUE_CONF);
  
  /**
   * Trigger stuff
   */
  addRecSignal(GSN_TRIG_ATTRINFO, &SumaParticipant::execTRIG_ATTRINFO);
  addRecSignal(GSN_FIRE_TRIG_ORD, &SumaParticipant::execFIRE_TRIG_ORD);

  addRecSignal(GSN_CREATE_TRIG_REF, &Suma::execCREATE_TRIG_REF);
  addRecSignal(GSN_CREATE_TRIG_CONF, &Suma::execCREATE_TRIG_CONF);
  addRecSignal(GSN_DROP_TRIG_REF, &Suma::execDROP_TRIG_REF);
  addRecSignal(GSN_DROP_TRIG_CONF, &Suma::execDROP_TRIG_CONF);
  
  addRecSignal(GSN_SUB_GCP_COMPLETE_REP, 
	       &SumaParticipant::execSUB_GCP_COMPLETE_REP);
  
  /**
   * @todo: fix pool sizes
   */
  Uint32 noTables;
  const ndb_mgm_configuration_iterator * p = conf.getOwnConfigIterator();
  ndbrequire(p != 0);

  ndb_mgm_get_int_parameter(p, CFG_DB_NO_TABLES,  
			    &noTables);

  c_tablePool_.setSize(noTables);
  c_tables.setSize(noTables);
  
  c_subscriptions.setSize(20); //10
  c_subscriberPool.setSize(64);
  
  c_subscriptionPool.setSize(64); //2
  c_syncPool.setSize(20); //2
  c_dataBufferPool.setSize(128);
  
  {
    SLList<SyncRecord> tmp(c_syncPool);
    Ptr<SyncRecord> ptr;
    while(tmp.seize(ptr))
      new (ptr.p) SyncRecord(* this, c_dataBufferPool);
    tmp.release();
  }

  for( int i = 0; i < NO_OF_BUCKETS; i++) {
    c_buckets[i].active = false;
    c_buckets[i].handover = false;
    c_buckets[i].handover_started = false;
    c_buckets[i].handoverGCI = 0;
  }
  c_handoverToDo = false;
  c_lastInconsistentGCI = RNIL;
  c_lastCompleteGCI = RNIL;
  c_nodeFailGCI = 0;

  c_failedApiNodes.clear();
}
  
SumaParticipant::~SumaParticipant()
{
}

Suma::Suma(const Configuration & conf) :
  SumaParticipant(conf),
  Restart(*this),
  c_nodes(c_nodePool),
  c_runningSubscriptions(c_subCoordinatorPool)
{
  
  c_nodePool.setSize(MAX_NDB_NODES);
  c_masterNodeId = getOwnNodeId();

  c_nodeGroup = c_noNodesInGroup = c_idInNodeGroup = 0;
  for (int i = 0; i < MAX_REPLICAS; i++) {
    c_nodesInGroup[i]   = 0;
  }

  c_subCoordinatorPool.setSize(10);
  
  // Add received signals
  addRecSignal(GSN_STTOR, &Suma::execSTTOR);
  addRecSignal(GSN_NDB_STTOR, &Suma::execNDB_STTOR);
  addRecSignal(GSN_DUMP_STATE_ORD, &Suma::execDUMP_STATE_ORD);
  addRecSignal(GSN_READ_NODESCONF, &Suma::execREAD_NODESCONF);
  addRecSignal(GSN_API_FAILREQ,  &Suma::execAPI_FAILREQ);
  addRecSignal(GSN_NODE_FAILREP, &Suma::execNODE_FAILREP);
  addRecSignal(GSN_INCL_NODEREQ, &Suma::execINCL_NODEREQ);
  addRecSignal(GSN_CONTINUEB, &Suma::execCONTINUEB);
  addRecSignal(GSN_SIGNAL_DROPPED_REP, &Suma::execSIGNAL_DROPPED_REP, true);
  addRecSignal(GSN_UTIL_SEQUENCE_CONF, &Suma::execUTIL_SEQUENCE_CONF);
  addRecSignal(GSN_UTIL_SEQUENCE_REF, &Suma::execUTIL_SEQUENCE_REF);
  addRecSignal(GSN_CREATE_SUBID_REQ, 
	       &Suma::execCREATE_SUBID_REQ);

  addRecSignal(GSN_SUB_CREATE_CONF, &Suma::execSUB_CREATE_CONF);
  addRecSignal(GSN_SUB_CREATE_REF, &Suma::execSUB_CREATE_REF);
  addRecSignal(GSN_SUB_SYNC_CONF, &Suma::execSUB_SYNC_CONF);
  addRecSignal(GSN_SUB_SYNC_REF, &Suma::execSUB_SYNC_REF);
  addRecSignal(GSN_SUB_START_CONF, &Suma::execSUB_START_CONF);
  addRecSignal(GSN_SUB_START_REF, &Suma::execSUB_START_REF);

  addRecSignal(GSN_SUMA_START_ME, &Suma::execSUMA_START_ME);
  addRecSignal(GSN_SUMA_HANDOVER_REQ, &Suma::execSUMA_HANDOVER_REQ);
  addRecSignal(GSN_SUMA_HANDOVER_CONF, &Suma::execSUMA_HANDOVER_CONF);

  addRecSignal(GSN_SUB_GCP_COMPLETE_ACC, 
	       &Suma::execSUB_GCP_COMPLETE_ACC);
}

Suma::~Suma()
{
}

BLOCK_FUNCTIONS(Suma);
BLOCK_FUNCTIONS(SumaParticipant);

