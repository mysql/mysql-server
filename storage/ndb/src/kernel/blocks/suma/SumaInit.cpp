/*
   Copyright (c) 2003, 2013, Oracle and/or its affiliates. All rights reserved.

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

#include "Suma.hpp"

#include <Properties.hpp>
#include <Configuration.hpp>

#define JAM_FILE_ID 468


Suma::Suma(Block_context& ctx) :
  SimulatedBlock(SUMA, ctx),
  c_tables(c_tablePool),
  c_subscriptions(c_subscriptionPool),
  c_gcp_list(c_gcp_pool),
  m_current_gci(~(Uint64)0)
{
  BLOCK_CONSTRUCTOR(Suma);

  // Add received signals
  addRecSignal(GSN_READ_CONFIG_REQ, &Suma::execREAD_CONFIG_REQ);
  addRecSignal(GSN_STTOR, &Suma::execSTTOR);
  addRecSignal(GSN_NDB_STTOR, &Suma::execNDB_STTOR);
  addRecSignal(GSN_DUMP_STATE_ORD, &Suma::execDUMP_STATE_ORD);
  addRecSignal(GSN_DBINFO_SCANREQ, &Suma::execDBINFO_SCANREQ);
  addRecSignal(GSN_READ_NODESCONF, &Suma::execREAD_NODESCONF);
  addRecSignal(GSN_API_START_REP, &Suma::execAPI_START_REP, true);
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

  addRecSignal(GSN_SUB_START_CONF, &Suma::execSUB_START_CONF);
  addRecSignal(GSN_SUB_START_REF, &Suma::execSUB_START_REF);

  addRecSignal(GSN_SUMA_START_ME_REQ, &Suma::execSUMA_START_ME_REQ);
  addRecSignal(GSN_SUMA_START_ME_REF, &Suma::execSUMA_START_ME_REF);
  addRecSignal(GSN_SUMA_START_ME_CONF, &Suma::execSUMA_START_ME_CONF);
  addRecSignal(GSN_SUMA_HANDOVER_REQ, &Suma::execSUMA_HANDOVER_REQ);
  addRecSignal(GSN_SUMA_HANDOVER_REF, &Suma::execSUMA_HANDOVER_REF);
  addRecSignal(GSN_SUMA_HANDOVER_CONF, &Suma::execSUMA_HANDOVER_CONF);
  
  addRecSignal(GSN_SUB_GCP_COMPLETE_ACK, 
	       &Suma::execSUB_GCP_COMPLETE_ACK);
  
  addRecSignal(GSN_STOP_ME_REQ,
               &Suma::execSTOP_ME_REQ);

  /**
   * SUMA participant if
   */
  addRecSignal(GSN_SUB_CREATE_REQ, &Suma::execSUB_CREATE_REQ);
  addRecSignal(GSN_SUB_REMOVE_REQ, &Suma::execSUB_REMOVE_REQ);
  addRecSignal(GSN_SUB_START_REQ, &Suma::execSUB_START_REQ);
  addRecSignal(GSN_SUB_STOP_REQ, &Suma::execSUB_STOP_REQ);
  addRecSignal(GSN_SUB_SYNC_REQ, &Suma::execSUB_SYNC_REQ);

  /**
   * Dict interface
   */
  addRecSignal(GSN_DROP_TAB_CONF, &Suma::execDROP_TAB_CONF);
  addRecSignal(GSN_ALTER_TAB_REQ, &Suma::execALTER_TAB_REQ);
  addRecSignal(GSN_CREATE_TAB_CONF, &Suma::execCREATE_TAB_CONF);

  addRecSignal(GSN_GET_TABINFO_CONF, &Suma::execGET_TABINFO_CONF);
  addRecSignal(GSN_GET_TABINFOREF, &Suma::execGET_TABINFOREF);

  addRecSignal(GSN_DICT_LOCK_REF, &Suma::execDICT_LOCK_REF);
  addRecSignal(GSN_DICT_LOCK_CONF, &Suma::execDICT_LOCK_CONF);

  /**
   * Dih interface
   */
  addRecSignal(GSN_DIH_SCAN_TAB_REF, &Suma::execDIH_SCAN_TAB_REF);
  addRecSignal(GSN_DIH_SCAN_TAB_CONF, &Suma::execDIH_SCAN_TAB_CONF);
  addRecSignal(GSN_DIH_SCAN_GET_NODES_CONF, &Suma::execDIH_SCAN_GET_NODES_CONF);
  addRecSignal(GSN_CHECKNODEGROUPSCONF, &Suma::execCHECKNODEGROUPSCONF);
  addRecSignal(GSN_GCP_PREPARE, &Suma::execGCP_PREPARE);

  /**
   * Scan interface
   */
  addRecSignal(GSN_SCAN_HBREP, &Suma::execSCAN_HBREP);
  addRecSignal(GSN_TRANSID_AI, &Suma::execTRANSID_AI);
  addRecSignal(GSN_KEYINFO20, &Suma::execKEYINFO20);
  addRecSignal(GSN_SCAN_FRAGREF, &Suma::execSCAN_FRAGREF);
  addRecSignal(GSN_SCAN_FRAGCONF, &Suma::execSCAN_FRAGCONF);
#if 0
  addRecSignal(GSN_SUB_SYNC_CONTINUE_REF, 
	       &Suma::execSUB_SYNC_CONTINUE_REF);
#endif
  addRecSignal(GSN_SUB_SYNC_CONTINUE_CONF, 
	       &Suma::execSUB_SYNC_CONTINUE_CONF);
  
  /**
   * Trigger stuff
   */
  addRecSignal(GSN_TRIG_ATTRINFO, &Suma::execTRIG_ATTRINFO);
  addRecSignal(GSN_FIRE_TRIG_ORD, &Suma::execFIRE_TRIG_ORD);
  addRecSignal(GSN_FIRE_TRIG_ORD_L, &Suma::execFIRE_TRIG_ORD_L);

  addRecSignal(GSN_CREATE_TRIG_IMPL_REF, &Suma::execCREATE_TRIG_IMPL_REF);
  addRecSignal(GSN_CREATE_TRIG_IMPL_CONF, &Suma::execCREATE_TRIG_IMPL_CONF);
  addRecSignal(GSN_DROP_TRIG_IMPL_REF, &Suma::execDROP_TRIG_IMPL_REF);
  addRecSignal(GSN_DROP_TRIG_IMPL_CONF, &Suma::execDROP_TRIG_IMPL_CONF);
  
  addRecSignal(GSN_SUB_GCP_COMPLETE_REP, 
	       &Suma::execSUB_GCP_COMPLETE_REP);

  addRecSignal(GSN_CREATE_NODEGROUP_IMPL_REQ,
               &Suma::execCREATE_NODEGROUP_IMPL_REQ);

  addRecSignal(GSN_DROP_NODEGROUP_IMPL_REQ,
               &Suma::execDROP_NODEGROUP_IMPL_REQ);

  c_current_seq = 0;
  c_outstanding_drop_trig_req = 0;
  c_restart.m_ref = 0;
  c_startup.m_restart_server_node_id = RNIL; // Server for my NR
  c_shutdown.m_wait_handover = false;

#ifdef VM_TRACE
  m_gcp_monitor = 0;
#endif
  m_missing_data = false;
  bzero(c_subscriber_per_node, sizeof(c_subscriber_per_node));

  m_gcp_rep_cnt = getLqhWorkers();
  m_min_gcp_rep_counter_index = 0;
  m_max_gcp_rep_counter_index = 0;
  bzero(m_gcp_rep_counter, sizeof(m_gcp_rep_counter));
}

Suma::~Suma()
{
  c_page_pool.clear();
}

bool
Suma::getParam(const char * param, Uint32 * retVal)
{
  if (param != NULL && retVal != NULL)
  {
    if (strcmp(param, "FragmentSendPool") == 0)
    {
      /* FragmentSendPool
       * We increase the size of the fragment send pool
       * to possibly handle max number of SQL nodes
       * being subscribers
       */

      *retVal= MAX_NODES;
      return true;
    }
  }
  return false;
}

BLOCK_FUNCTIONS(Suma)

