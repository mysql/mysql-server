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

#ifndef EXTNDB_HPP
#define EXTNDB_HPP

#include <ndb_global.h>

#include <TransporterDefinitions.hpp>
#include <TransporterFacade.hpp>
#include <ClusterMgr.hpp>
#include <API.hpp>
#include <Vector.hpp>

#include <signaldata/RepImpl.hpp>
#include <signaldata/GrepImpl.hpp>

#include <rep/SignalQueue.hpp>
#include <rep/ExtSender.hpp>

#include <rep/storage/GCIContainerPS.hpp>
#include "ExtAPI.hpp"

extern "C" {
static void * signalExecThread_C(void *);
}

/**
 * @class ExtNDB
 * @brief Class responsible for connection to primary system GREP
 */
class ExtNDB 
{
public:
  /***************************************************************************
   * Constructor / Destructor
   ***************************************************************************/
  ExtNDB(GCIContainerPS * gciContainer, ExtAPI * extAPI);
  ~ExtNDB();
  bool init(const char * connectString = NULL);
  
  /***************************************************************************
   * Public Methods
   ***************************************************************************/
  void           setGrepSender(ExtSender * es) { m_grepSender = es; };
  ExtSender *    getGrepSender() { return m_grepSender; };
  void           setRepSender(ExtSender * es) { 
    m_extAPI->setRepSender(es); m_repSender = es; };
  void           signalErrorHandler(NdbApiSignal * s, Uint32 nodeId);

private:
  friend void * signalExecThread_C(void *);
  void           signalExecThreadRun();
  
  static void    execSignal(void* signalSender, NdbApiSignal* signal, 
			    class LinearSectionPtr ptr[3]);
  
  static void    execNodeStatus(void* signalSender, NodeId, 
				bool alive, bool nfCompleted);
  
  void           sendSignalRep(NdbApiSignal *);
  void           sendDisconnectRep(Uint32 nodeId);

  /***************************************************************************
   * Signal Executors
   ***************************************************************************/
  void execSUB_GCP_COMPLETE_REP(NdbApiSignal*);
  void execGREP_SUB_CREATE_CONF(NdbApiSignal * signal);
  void execGREP_SUB_REMOVE_CONF(NdbApiSignal * signal);
  void execGREP_SUB_START_CONF(NdbApiSignal * signal);
  void sendGREP_SUB_START_CONF(NdbApiSignal * signal, Uint32 gci);
  void execSUB_TABLE_DATA(NdbApiSignal * signal,LinearSectionPtr ptr[3]);
  void execSUB_META_DATA(NdbApiSignal * signal,LinearSectionPtr ptr[3]);
  
  // Signals that are actually just fowarded to REP
  void execGREP_CREATE_SUBID_CONF(NdbApiSignal *);

  /***************************************************************************
   * Private Variables
   ***************************************************************************/
  struct NdbThread *      m_signalExecThread;
  class SignalQueue       m_signalRecvQueue;

  Uint32                  m_ownNodeId;          ///< NodeId of this node
  Uint32                  m_ownBlockNo;         ///< BlockNo of this "block"
  BlockReference          m_ownRef;             ///< Reference to this 
  
  ExtSender *             m_grepSender;         ///< Responsible send to GREP
  ExtSender *             m_repSender;          ///< Responsible send to SS REP
  
  NodeGroupInfo *	  m_nodeGroupInfo;
  GCIContainerPS *	  m_gciContainerPS;	///< Interface to GCICotainer
						///< seen by PS
  TransporterFacade *	  m_transporterFacade;

  bool                    m_doneSetGrepSender;    ///< Only done once
  bool                    m_dataLogStarted;
  Uint32                  m_subId;
  Uint32                  m_subKey;
  Uint32                  m_firstGCI;

  ExtAPI *                m_extAPI;
};

#endif
