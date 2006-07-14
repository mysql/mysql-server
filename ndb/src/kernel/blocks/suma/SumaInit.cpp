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
  addRecSignal(GSN_SUB_SYNC_REQ, &SumaParticipant::execSUB_SYNC_REQ);
  
  /**
   * Dict interface
   */
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
  
}
  
SumaParticipant::~SumaParticipant()
{
}

Suma::Suma(const Configuration & conf) :
  SumaParticipant(conf),
  c_nodes(c_nodePool),
  c_runningSubscriptions(c_subCoordinatorPool)
{
  // Add received signals
  addRecSignal(GSN_READ_CONFIG_REQ, &Suma::execREAD_CONFIG_REQ);
  addRecSignal(GSN_STTOR, &Suma::execSTTOR);
  addRecSignal(GSN_NDB_STTOR, &Suma::execNDB_STTOR);
  addRecSignal(GSN_DUMP_STATE_ORD, &Suma::execDUMP_STATE_ORD);
  addRecSignal(GSN_READ_NODESCONF, &Suma::execREAD_NODESCONF);
  addRecSignal(GSN_CONTINUEB, &Suma::execCONTINUEB);
  addRecSignal(GSN_SIGNAL_DROPPED_REP, &Suma::execSIGNAL_DROPPED_REP, true);
  addRecSignal(GSN_UTIL_SEQUENCE_CONF, &Suma::execUTIL_SEQUENCE_CONF);
  addRecSignal(GSN_UTIL_SEQUENCE_REF, &Suma::execUTIL_SEQUENCE_REF);
  addRecSignal(GSN_CREATE_SUBID_REQ, 
	       &Suma::execCREATE_SUBID_REQ);
}

Suma::~Suma()
{
}

BLOCK_FUNCTIONS(Suma)
BLOCK_FUNCTIONS(SumaParticipant)

