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

#ifndef REQUESTOR_HPP
#define REQUESTOR_HPP

#include <ndb_global.h>

#include <TransporterDefinitions.hpp>
#include <TransporterFacade.hpp>
#include <ClusterMgr.hpp>
#include <API.hpp>
#include <Vector.hpp>
#include <GrepError.hpp>

#include <rep/storage/GCIContainer.hpp>

/**
 * @todo Remove this dependency
 */
#include <rep/adapters/AppNDB.hpp>

#include <rep/SignalQueue.hpp>
#include <rep/ExtSender.hpp>


/**
 * @class  Requestor
 * @brief  Connects to GREP Coordinator on the standby system
 */
class Requestor {
public:
  /***************************************************************************
   * Constructor / Destructor / Init
   ***************************************************************************/
  Requestor(GCIContainer * gciContainer, AppNDB * applier, RepState * repSt);
  ~Requestor();
  bool init(const char * connectString = NULL);
  
  /***************************************************************************
   * Public Methods
   ***************************************************************************/
  void         setRepSender(ExtSender * es) { m_repSender = es; };

private:
  static void *  signalExecThread_C(void *);  ///< SignalQueue executor thread
  void           signalExecThreadRun();

  static void execSignal(void* executorObj, NdbApiSignal* signal, 
			 class LinearSectionPtr ptr[3]);
  static void execNodeStatus(void* executorObj, NodeId, bool alive, 
			     bool nfCompleted);

  void sendSignalRep(NdbApiSignal *);
  void sendSignalGrep(NdbApiSignal *);

  void connectToNdb();
  
  /***************************************************************************
   * Signal Executors
   ***************************************************************************/
  void execREP_GET_GCIBUFFER_CONF(NdbApiSignal*);
  void execREP_CLEAR_GCIBUFFER_REP(NdbApiSignal*);
  void execREP_INSERT_GCIBUFFER_REQ(NdbApiSignal*);
  void execREP_CLEAR_SS_GCIBUFFER_REQ(NdbApiSignal*);
  void execREP_DROP_TABLE_REQ(NdbApiSignal*);

  /***************************************************************************
   * Signal Executors 2
   ***************************************************************************/
  void execGREP_CREATE_SUBID_CONF(NdbApiSignal*);
  void execGREP_CREATE_SUBID_REF(NdbApiSignal*);
  void createSubscription(NdbApiSignal*);
  void createSubscriptionId(NdbApiSignal*);
  void execGREP_SUB_CREATE_CONF(NdbApiSignal*);
  void execGREP_SUB_CREATE_REF(NdbApiSignal*);
  void execGREP_SUB_START_CONF(NdbApiSignal*);
  void execGREP_SUB_START_REF(NdbApiSignal*);
  void removeSubscription(NdbApiSignal*);
  void execGREP_SUB_REMOVE_REF(NdbApiSignal*);
  void execGREP_SUB_SYNC_CONF(NdbApiSignal*);
  void execGREP_SUB_SYNC_REF(NdbApiSignal*);
  void execREP_CLEAR_SS_GCIBUFFER_CONF(NdbApiSignal*);
  void execREP_CLEAR_SS_GCIBUFFER_REF(NdbApiSignal*);
  void execREP_GET_GCIBUFFER_REF(NdbApiSignal*);
  void execREP_DISCONNECT_REP(NdbApiSignal*);

  /***************************************************************************
   * Ref signal senders
   ***************************************************************************/
  void sendREP_INSERT_GCIBUFFER_REF(NdbApiSignal * signal,
				    Uint32 gci,
				    Uint32 nodeGrp,
				    GrepError::Code err);

  void sendREP_CLEAR_SS_GCIBUFFER_REF(NdbApiSignal* signal, 
				      Uint32 firstGCI, 
				      Uint32 lastGCI,
				      Uint32 currentGCI,
				      Uint32 nodeGrp,
				      GrepError::Code err);
  
  /***************************************************************************
   * Private Variables
   ***************************************************************************/
  class SignalQueue	    m_signalRecvQueue;
  struct NdbThread *        m_signalExecThread;

  RepState *                m_repState;

  Uint32                    m_ownNodeId;          ///< NodeId of this node
  Uint32                    m_ownBlockNo;         ///< BlockNo of this "block"
  BlockReference            m_ownRef;             ///< Reference to this 

  TransporterFacade *	    m_transporterFacade;

  GCIContainer *	    m_gciContainer;

  AppNDB *		    m_applier;
  ExtSender *               m_repSender;

  friend void startSubscription(void * cbObj, NdbApiSignal* signal, int type);
  friend void scanSubscription(void * cbObj, NdbApiSignal* signal, int type);

  friend RepState::FuncRequestCreateSubscriptionId requestCreateSubscriptionId;
  friend RepState::FuncRequestCreateSubscription   requestCreateSubscription;
  friend RepState::FuncRequestRemoveSubscription   requestRemoveSubscription;

  friend RepState::FuncRequestTransfer       requestTransfer;
  friend RepState::FuncRequestApply          requestApply;
  friend RepState::FuncRequestDeleteSS       requestDeleteSS;
  friend RepState::FuncRequestDeletePS       requestDeletePS;

  friend RepState::FuncRequestStartMetaLog   requestStartMetaLog;
  friend RepState::FuncRequestStartDataLog   requestStartDataLog;
  friend RepState::FuncRequestStartMetaScan  requestStartMetaScan;
  friend RepState::FuncRequestStartDataScan  requestStartDataScan;
  friend RepState::FuncRequestEpochInfo      requestEpochInfo;
};

#endif
