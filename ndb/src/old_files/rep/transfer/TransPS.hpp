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

#ifndef TransPS_HPP
#define TransPS_HPP

#include <NdbTick.h>
#include <NdbMain.h>
#include <NdbOut.hpp>
#include <NdbSleep.h>

#include <TransporterDefinitions.hpp>
#include <TransporterFacade.hpp>
#include <ClusterMgr.hpp>

#include <rep/storage/GCIContainerPS.hpp>

#include <signaldata/RepImpl.hpp>

#include <rep/SignalQueue.hpp>
#include <rep/ExtSender.hpp>

#include <rep/rep_version.hpp>

extern "C" {
static void * signalExecThread_C(void *);
}

/**
 * @class TransPS
 * @brief Responsible for REP-REP interface in Primary System role
 */
class TransPS {
public:
  /***************************************************************************
   * Constructor / Destructor
   ***************************************************************************/
  TransPS(GCIContainerPS * gciContainer);
  ~TransPS();

  void init(TransporterFacade * tf, const char * connectString = NULL);

  /***************************************************************************
   * Public Methods
   ***************************************************************************/
  ExtSender *  getRepSender()                { return m_repSender; };
  void         setGrepSender(ExtSender * es) { m_grepSender = es; };

private: 
  /***************************************************************************
   * Private Methods
   ***************************************************************************/
  /**
   *  SignalQueue executor thread
   */

  friend void * signalExecThread_C(void *);

  void signalExecThreadRun();

  static void execSignal(void* signalSender, NdbApiSignal* signal, 
			 class LinearSectionPtr ptr[3]);
  
  static void execNodeStatus(void* signalSender, NodeId, 
			     bool alive, bool nfCompleted);

  void sendSignalRep(NdbApiSignal * s);
  void sendSignalGrep(NdbApiSignal * s);

  void sendFragmentedSignalRep(NdbApiSignal * s, LinearSectionPtr ptr[3],
			       Uint32 sections );
  void sendFragmentedSignalGrep(NdbApiSignal * s, LinearSectionPtr ptr[3],
				Uint32 sections );
  
  /***************************************************************************
   * Signal executors
   ***************************************************************************/
  void execREP_CLEAR_PS_GCIBUFFER_REQ(NdbApiSignal*);
  void execREP_GET_GCI_REQ(NdbApiSignal*);
  void execREP_GET_GCIBUFFER_REQ(NdbApiSignal*);

  /***************************************************************************
   * Ref signal senders
   ***************************************************************************/
  void sendREP_GET_GCI_REF(NdbApiSignal* signal, Uint32 nodeGrp,
			   Uint32 firstPSGCI, Uint32 lastPSGCI,
			   GrepError::Code err);
  
  void sendREP_CLEAR_PS_GCIBUFFER_REF(NdbApiSignal* signal, 
				      Uint32 firstGCI, Uint32 lastGCI,
				      Uint32 currentGCI, Uint32 nodeGrp,
				      GrepError::Code err);

  void sendREP_GET_GCIBUFFER_REF(NdbApiSignal* signal,
				 Uint32 firstGCI, Uint32 lastGCI,
				 Uint32 nodeGrp,
				 GrepError::Code err);

  /***************************************************************************
   * Other Methods
   ***************************************************************************/
  void transferPages(Uint32 firstGCI, Uint32 lastGCI, Uint32 id, 
		     Uint32 nodeGrp, NdbApiSignal* signal);

  /*************
   * Variables
   *************/
  Uint32                  m_ownNodeId;          ///< NodeId of this node
  Uint32                  m_ownBlockNo;         ///< BlockNo of this "block"
  BlockReference          m_ownRef;             ///< Reference to this 

  BlockReference          m_extRepRef;          ///< Node ref of REP at SS

  ExtSender *             m_grepSender;         ///< Responsible send to GREP
  ExtSender *             m_repSender;          ///< Responsible send to REP
  
  struct NdbThread *      m_signalExecThread;
  class SignalQueue       m_signalRecvQueue;

  GCIContainerPS *	  m_gciContainerPS;     ///< Ref to gci container.
};

#endif
