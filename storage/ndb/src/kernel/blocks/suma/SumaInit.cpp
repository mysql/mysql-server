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

#include <Properties.hpp>
#include <Configuration.hpp>

Suma::Suma(const Configuration & conf) :
  SimulatedBlock(SUMA, conf),
  c_metaSubscribers(c_subscriberPool),
  c_removeDataSubscribers(c_subscriberPool),
  c_tables(c_tablePool),
  c_subscriptions(c_subscriptionPool),
  Restart(*this),
  c_gcp_list(c_gcp_pool)
{
  // Add received signals
  addRecSignal(GSN_READ_CONFIG_REQ, &Suma::execREAD_CONFIG_REQ);
  addRecSignal(GSN_STTOR, &Suma::execSTTOR);
  addRecSignal(GSN_NDB_STTOR, &Suma::execNDB_STTOR);
  addRecSignal(GSN_DUMP_STATE_ORD, &Suma::execDUMP_STATE_ORD);
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
  
  /**
   * SUMA participant if
   */
  addRecSignal(GSN_SUB_CREATE_REQ, &Suma::execSUB_CREATE_REQ);
  addRecSignal(GSN_SUB_REMOVE_REQ, &Suma::execSUB_REMOVE_REQ);
  addRecSignal(GSN_SUB_START_REQ, &Suma::execSUB_START_REQ);
  addRecSignal(GSN_SUB_STOP_REQ, &Suma::execSUB_STOP_REQ);
  addRecSignal(GSN_SUB_STOP_REF, &Suma::execSUB_STOP_REF);
  addRecSignal(GSN_SUB_STOP_CONF, &Suma::execSUB_STOP_CONF);
  addRecSignal(GSN_SUB_SYNC_REQ, &Suma::execSUB_SYNC_REQ);

  /**
   * Dict interface
   */
  addRecSignal(GSN_DROP_TAB_CONF, &Suma::execDROP_TAB_CONF);
  addRecSignal(GSN_ALTER_TAB_CONF, &Suma::execALTER_TAB_CONF);
  addRecSignal(GSN_CREATE_TAB_CONF, &Suma::execCREATE_TAB_CONF);

#if 0
  addRecSignal(GSN_LIST_TABLES_CONF, &Suma::execLIST_TABLES_CONF);
#endif
  addRecSignal(GSN_GET_TABINFO_CONF, &Suma::execGET_TABINFO_CONF);
  addRecSignal(GSN_GET_TABINFOREF, &Suma::execGET_TABINFOREF);
#if 0
  addRecSignal(GSN_GET_TABLEID_CONF, &Suma::execGET_TABLEID_CONF);
  addRecSignal(GSN_GET_TABLEID_REF, &Suma::execGET_TABLEID_REF);
#endif
  /**
   * Dih interface
   */
  addRecSignal(GSN_DI_FCOUNTCONF, &Suma::execDI_FCOUNTCONF);
  addRecSignal(GSN_DIGETPRIMCONF, &Suma::execDIGETPRIMCONF);

  /**
   * Scan interface
   */
  addRecSignal(GSN_SCAN_HBREP, &Suma::execSCAN_HBREP);
  addRecSignal(GSN_TRANSID_AI, &Suma::execTRANSID_AI);
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

  addRecSignal(GSN_CREATE_TRIG_REF, &Suma::execCREATE_TRIG_REF);
  addRecSignal(GSN_CREATE_TRIG_CONF, &Suma::execCREATE_TRIG_CONF);
  addRecSignal(GSN_DROP_TRIG_REF, &Suma::execDROP_TRIG_REF);
  addRecSignal(GSN_DROP_TRIG_CONF, &Suma::execDROP_TRIG_CONF);
  
  addRecSignal(GSN_SUB_GCP_COMPLETE_REP, 
	       &Suma::execSUB_GCP_COMPLETE_REP);
}

Suma::~Suma()
{
}

BLOCK_FUNCTIONS(Suma)

