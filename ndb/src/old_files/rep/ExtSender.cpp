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

#include "ExtSender.hpp"

/*****************************************************************************
 * Constructor / Destructor / Init / Get / Set
 *****************************************************************************/

/**
 * @todo: signalErrorHandler is not finished. Just infrastructure. 
 */

ExtSender::ExtSender() {
  m_tf = NULL;
  m_nodeId = 0;
  m_ownRef = 0;
}

ExtSender::~ExtSender() {
  if (m_tf) delete m_tf;
}

void    
ExtSender::setNodeId(Uint32 nodeId) 
{ 
#if 0
  ndbout_c("ExtSender: Set nodeid to %d", nodeId);
#endif

  m_nodeId = nodeId; 
}

Uint32 
ExtSender::getOwnRef() const 
{ 
  if(!m_ownRef) REPABORT("No m_ownRef set");

  return m_ownRef; 
}

void 
ExtSender::setOwnRef(Uint32 ref) 
{ 
  // Can only be set once
  if (m_ownRef != 0) REPABORT("Trying to change m_ownRef");

  m_ownRef = ref; 
}

/*****************************************************************************
 * Usage
 *****************************************************************************/

int
ExtSender::sendSignal(class NdbApiSignal * s) {
#if 0
  ndbout_c("ExtSender: Sending signal %d to %d", 
	   s->readSignalNumber(), m_nodeId);
#endif

  if (m_tf == NULL || m_nodeId == 0 || s==0) abort();
  m_tf->lock_mutex();
  int retvalue = m_tf->sendSignal(s, m_nodeId);
  if (retvalue) {
    RLOG(("sendSignal returned %d for send to node %d", retvalue, m_nodeId));
  }
#if 0
  ndbout_c("ExtSender: Sent signal to %d", m_nodeId);
#endif
  m_tf->unlock_mutex();
  return retvalue;
}

int
ExtSender::sendFragmentedSignal(NdbApiSignal * s, 
				LinearSectionPtr ptr[3],
				Uint32 sections) {
  if (m_tf == NULL || m_nodeId == 0) abort();
  m_tf->lock_mutex();
  int retvalue = m_tf->sendFragmentedSignal(s, m_nodeId, ptr, sections);
  if (retvalue) {
    RLOG(("sendFragmentedSignal returned %d for send to node %d",
	  retvalue, m_nodeId));
  }
  m_tf->unlock_mutex();
  return retvalue;
}

/**
 * Check that TransporterFacade is connected to at least one DB node
 */
bool
ExtSender::connected(Uint32 timeOutMillis){
#if 0
  ndbout_c("ExtSender: Waiting for remote component to be ready!");
#endif

  NDB_TICKS start = NdbTick_CurrentMillisecond();
  NDB_TICKS now = start;
  //  while(m_tf->theClusterMgr->getNoOfConnectedNodes() == 0 &&
  while((m_tf->get_an_alive_node() == 0) &&
	(timeOutMillis == 0 || (now - start) < timeOutMillis)){
    NdbSleep_MilliSleep(100);
    now = NdbTick_CurrentMillisecond();
 }
  return m_tf->theClusterMgr->getNoOfConnectedNodes() > 0;
}

bool
ExtSender::connected(Uint32 timeOutMillis, Uint32 nodeId){
  NDB_TICKS start = NdbTick_CurrentMillisecond();
  NDB_TICKS now = start;

  //  while(m_tf->theClusterMgr->getNoOfConnectedNodes() == 0 &&
  while((m_tf->get_node_alive(nodeId) != 0) &&
	(timeOutMillis == 0 || (now - start) < timeOutMillis)){
    NdbSleep_MilliSleep(100);
    now = NdbTick_CurrentMillisecond();
  }
  return m_tf->theClusterMgr->getNoOfConnectedNodes() > 0;
}

NdbApiSignal * 
ExtSender::getSignal() 
{
  /**
   * @todo  This should use some kind of list of NdbApiSignals,
   *        similar to the NDBAPI and the MGRSRVR.
   *        The best thing would be to have set of code 
   *        shared between the programs.
   *        Thus the NDBAPI and MGMSRVR should be refactored.
   *        /Lars
   */
  return new NdbApiSignal(getOwnRef());
}
