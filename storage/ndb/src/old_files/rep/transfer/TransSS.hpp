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

#ifndef TransSS_HPP
#define TransSS_HPP

#include <NdbTick.h>
#include <NdbMain.h>
#include <NdbOut.hpp>
#include <NdbSleep.h>

#include <TransporterDefinitions.hpp>
#include <TransporterFacade.hpp>
#include <ClusterMgr.hpp>
#include <API.hpp>

#include <rep/storage/GCIContainer.hpp>

#include <rep/SignalQueue.hpp>
#include <rep/ExtSender.hpp>

#include <rep/state/RepState.hpp>

extern "C" {
static void *  signalExecThread_C(void *);
}

/**
 * @class TransSS
 * @brief Responsible for REP-REP interface in Standby System role
 */
class TransSS {
public:
  /***************************************************************************
   * Constructor / Destructor / Init
   ***************************************************************************/
  TransSS(GCIContainer * gciContainer, RepState * repState);
  ~TransSS();
   void init(const char * connectString = NULL);

  /***************************************************************************
   * Public Methods
   ***************************************************************************/
  ExtSender *  getRepSender()                { return m_repSender; };
  TransporterFacade * getTransporterFacade() { return m_transporterFacade; };

private:
  /***************************************************************************
   * Private Methods
   ***************************************************************************/
  friend void *  signalExecThread_C(void *);
  void           signalExecThreadRun();   ///< SignalQueue executor thread

  static void execSignal(void* executorObj, NdbApiSignal* signal, 
			 class LinearSectionPtr ptr[3]);
  static void execNodeStatus(void* executorObj, NodeId, bool alive, 
			     bool nfCompleted);
  
  void sendSignalRep(NdbApiSignal * s);

  /***************************************************************************
   * Signal receivers
   ***************************************************************************/
  void execREP_GET_GCI_REQ(NdbApiSignal*);  
  void execREP_GET_GCI_CONF(NdbApiSignal*);
  void execREP_GET_GCI_REF(NdbApiSignal*);

  void execREP_GET_GCIBUFFER_REQ(NdbApiSignal*);
  void execREP_GET_GCIBUFFER_CONF(NdbApiSignal*);
  void execREP_GET_GCIBUFFER_REF(NdbApiSignal*);

  void execGREP_SUB_REMOVE_CONF(NdbApiSignal *);
  void execGREP_SUB_REMOVE_REF(NdbApiSignal *);

  void execREP_INSERT_GCIBUFFER_REQ(NdbApiSignal*);
  void execREP_INSERT_GCIBUFFER_CONF(NdbApiSignal*);
  void execREP_INSERT_GCIBUFFER_REF(NdbApiSignal*);

  void execREP_DATA_PAGE(NdbApiSignal* signal, LinearSectionPtr ptr[3]);

  void execREP_GCIBUFFER_ACC_REP(NdbApiSignal*);
  void execREP_DISCONNECT_REP(NdbApiSignal*);


  void execREP_CLEAR_PS_GCIBUFFER_CONF(NdbApiSignal*);
  void execREP_CLEAR_PS_GCIBUFFER_REF(NdbApiSignal*);

  void execGREP_SUB_SYNC_CONF(NdbApiSignal*);
  void execGREP_SUB_SYNC_REF(NdbApiSignal*);

  /***************************************************************************
   * Signal receivers : Subscriptions
   ***************************************************************************/
  void execGREP_CREATE_SUBID_CONF(NdbApiSignal*);
  void execGREP_CREATE_SUBID_REF(NdbApiSignal*);
  void execGREP_SUB_CREATE_CONF(NdbApiSignal*);
  void execGREP_SUB_CREATE_REF(NdbApiSignal*);
  void execGREP_SUB_START_CONF(NdbApiSignal*);
  void execGREP_SUB_START_REF(NdbApiSignal*);

  /***************************************************************************
   * Ref signal senders
   ***************************************************************************/

  void sendREP_GET_GCI_REF(NdbApiSignal* signal, Uint32 nodeGrp,
			   Uint32 firstSSGCI, Uint32 lastSSGCI,
			   GrepError::Code err);
  
  void sendREP_GET_GCIBUFFER_REF(NdbApiSignal* signal,
				 Uint32 firstGCI, Uint32 lastGCI,
				 Uint32 nodeGrp, GrepError::Code err);

  /***************************************************************************
   * Private Variables
   ***************************************************************************/
  RepState *              m_repState;

  struct NdbThread *      m_signalExecThread;   ///< Signal Queue executor
  class SignalQueue       m_signalRecvQueue;

  ExtSender *             m_repSender;      ///< Obj responsible send to REP

  Uint32                  m_ownNodeId;      ///< NodeId of this node
  Uint32                  m_ownBlockNo;     ///< BlockNo of this "block"
  BlockReference          m_ownRef;         ///< Reference to this 

  GCIContainer *	  m_gciContainer;       ///< Ref to gci container.

  TransporterFacade *     m_transporterFacade;
};

#endif
