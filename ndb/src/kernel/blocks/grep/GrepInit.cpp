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

#include "Grep.hpp"
#include <new>
#include <Properties.hpp>
#include <Configuration.hpp>

/*****************************************************************************
 * Grep Participant
 *****************************************************************************/
#if 0
GrepParticipant::GrepParticipant(const Configuration & conf) :
  SimulatedBlock(GREP, conf)
{
  BLOCK_CONSTRUCTOR(Grep);
  //m_repRef = 0;
  m_latestSeenGCI = 0;
}

GrepParticipant::~GrepParticipant()
{
}

BLOCK_FUNCTIONS(GrepParticipant);
#endif

/*****************************************************************************
 * Grep Coordinator
 *****************************************************************************/
Grep::Grep(const Configuration & conf) : 
  //  GrepParticipant(conf),
  SimulatedBlock(GREP, conf),
  m_nodes(m_nodePool),
  pscoord(this),
  pspart(this)
{
  m_nodePool.setSize(MAX_NDB_NODES);
  m_masterNodeId = getOwnNodeId();

  /***************************************************************************
   * General Signals 
   ***************************************************************************/
  addRecSignal(GSN_STTOR,           &Grep::execSTTOR);
  addRecSignal(GSN_NDB_STTOR,       &Grep::execNDB_STTOR);
  addRecSignal(GSN_DUMP_STATE_ORD,  &Grep::execDUMP_STATE_ORD);
  addRecSignal(GSN_READ_NODESCONF,  &Grep::execREAD_NODESCONF);
  addRecSignal(GSN_NODE_FAILREP,    &Grep::execNODE_FAILREP);
  addRecSignal(GSN_INCL_NODEREQ,    &Grep::execINCL_NODEREQ);

  addRecSignal(GSN_GREP_REQ,    &Grep::execGREP_REQ);
  addRecSignal(GSN_API_FAILREQ, &Grep::execAPI_FAILREQ);


  /***************************************************************************
   * Grep::PSCoord Signal Interface 
   ***************************************************************************/
  /**
   * From Grep::PSPart
   */
  addRecSignal(GSN_GREP_CREATE_CONF,  &Grep::fwdGREP_CREATE_CONF);
  addRecSignal(GSN_GREP_START_CONF,   &Grep::fwdGREP_START_CONF);
  addRecSignal(GSN_GREP_SYNC_CONF,    &Grep::fwdGREP_SYNC_CONF);
  addRecSignal(GSN_GREP_REMOVE_CONF,  &Grep::fwdGREP_REMOVE_CONF);

  addRecSignal(GSN_GREP_CREATE_REF,   &Grep::fwdGREP_CREATE_REF);
  addRecSignal(GSN_GREP_START_REF,    &Grep::fwdGREP_START_REF);
  addRecSignal(GSN_GREP_REMOVE_REF,    &Grep::fwdGREP_REMOVE_REF);

  /**
   * From Grep::SSCoord to Grep::PSCoord 
   */
  addRecSignal(GSN_GREP_SUB_START_REQ,     &Grep::fwdGREP_SUB_START_REQ);
  addRecSignal(GSN_GREP_SUB_CREATE_REQ,    &Grep::fwdGREP_SUB_CREATE_REQ);
  addRecSignal(GSN_GREP_SUB_SYNC_REQ, 	   &Grep::fwdGREP_SUB_SYNC_REQ);
  addRecSignal(GSN_GREP_SUB_REMOVE_REQ,    &Grep::fwdGREP_SUB_REMOVE_REQ);
  addRecSignal(GSN_GREP_CREATE_SUBID_REQ,  &Grep::fwdGREP_CREATE_SUBID_REQ);

  /****************************************************************************
   * PSPart
   ***************************************************************************/
  /**
   * From SUMA to GREP PS Participant.  If suma is not a coodinator 
   */
  addRecSignal(GSN_SUB_START_CONF,   &Grep::fwdSUB_START_CONF);
  addRecSignal(GSN_SUB_CREATE_CONF,  &Grep::fwdSUB_CREATE_CONF);
  addRecSignal(GSN_SUB_SYNC_CONF,    &Grep::fwdSUB_SYNC_CONF);
  addRecSignal(GSN_SUB_REMOVE_CONF,  &Grep::fwdSUB_REMOVE_CONF);
  addRecSignal(GSN_SUB_CREATE_REF,   &Grep::fwdSUB_CREATE_REF);
  addRecSignal(GSN_SUB_START_REF,    &Grep::fwdSUB_START_REF);
  addRecSignal(GSN_SUB_SYNC_REF,     &Grep::fwdSUB_SYNC_REF);
  addRecSignal(GSN_SUB_REMOVE_REF,   &Grep::fwdSUB_REMOVE_REF);

  addRecSignal(GSN_SUB_SYNC_CONTINUE_REQ, 
	       &Grep::fwdSUB_SYNC_CONTINUE_REQ);

  /**
   * From Suma to Grep::PSPart.  Data signals.
   */
  addRecSignal(GSN_SUB_META_DATA, &Grep::fwdSUB_META_DATA);
  addRecSignal(GSN_SUB_TABLE_DATA, &Grep::fwdSUB_TABLE_DATA);
  addRecSignal(GSN_SUB_GCP_COMPLETE_REP, &Grep::fwdSUB_GCP_COMPLETE_REP);

  /**
   * From Grep::PSCoord to Grep::PSPart
   */
  addRecSignal(GSN_GREP_CREATE_REQ,   &Grep::fwdGREP_CREATE_REQ);
  addRecSignal(GSN_GREP_START_REQ,    &Grep::fwdGREP_START_REQ);
  addRecSignal(GSN_GREP_REMOVE_REQ,   &Grep::fwdGREP_REMOVE_REQ);
  addRecSignal(GSN_GREP_SYNC_REQ,     &Grep::fwdGREP_SYNC_REQ);
  addRecSignal(GSN_CREATE_SUBID_CONF, &Grep::fwdCREATE_SUBID_CONF);
  addRecSignal(GSN_GREP_START_ME, &Grep::fwdSTART_ME);
  addRecSignal(GSN_GREP_ADD_SUB_REQ,  &Grep::fwdGREP_ADD_SUB_REQ);
  addRecSignal(GSN_GREP_ADD_SUB_REF,  &Grep::fwdGREP_ADD_SUB_REF);
  addRecSignal(GSN_GREP_ADD_SUB_CONF, &Grep::fwdGREP_ADD_SUB_CONF);
}

Grep::~Grep()
{
}

BLOCK_FUNCTIONS(Grep);

Grep::PSPart::PSPart(Grep * sb) :
  BlockComponent(sb),
  c_subscriptions(c_subscriptionPool) 
{
  m_grep = sb;

  m_firstScanGCI = 1;  // Empty interval = [1,0]
  m_lastScanGCI = 0;

  m_latestSeenGCI = 0;

  c_subscriptions.setSize(10);
  c_subscriptionPool.setSize(10);  
}

Grep::PSCoord::PSCoord(Grep * sb) :
  BlockComponent(sb), 
  c_runningSubscriptions(c_subCoordinatorPool) 
{
  m_grep = sb;
  c_runningSubscriptions.setSize(10);
  c_subCoordinatorPool.setSize(2);
}

//BLOCK_FUNCTIONS(Grep::PSCoord);

BlockComponent::BlockComponent(SimulatedBlock * sb) {
  m_sb = sb;
}
