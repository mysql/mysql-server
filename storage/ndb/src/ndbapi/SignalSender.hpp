/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#ifndef SIGNAL_SENDER_HPP
#define SIGNAL_SENDER_HPP

#include <ndb_global.h>
#include "TransporterFacade.hpp"
#include <Vector.hpp>

struct SimpleSignal {
public:
  SimpleSignal(bool dealloc = false);
  ~SimpleSignal();
  
  void set(class SignalSender&,
	   Uint8  trace, Uint16 recBlock, Uint16 gsn, Uint32 len);
  
  struct SignalHeader header;
  Uint32 theData[25];
  LinearSectionPtr ptr[3];

  int readSignalNumber() const {return header.theVerId_signalNumber; }
  Uint32 *getDataPtrSend() { return theData; }
  const Uint32 *getDataPtr() const { return theData; }

  void print(FILE * out = stdout);
private:
  bool deallocSections;
};

class SignalSender {
public:
  SignalSender(TransporterFacade *facade);
  virtual ~SignalSender();
  
  int lock();
  int unlock();

  Uint32 getOwnRef() const;
  Uint32 getAliveNode() const;
  const ClusterMgr::Node &getNodeInfo(Uint16 nodeId) const;
  Uint32 getNoOfConnectedNodes() const;

  SendStatus sendSignal(Uint16 nodeId, const SimpleSignal *);
  
  SimpleSignal * waitFor(Uint32 timeOutMillis = 0);
  SimpleSignal * waitFor(Uint16 nodeId, Uint32 timeOutMillis = 0);
  SimpleSignal * waitFor(Uint16 nodeId, Uint16 gsn, Uint32 timeOutMillis = 0);  
private:
  int m_blockNo;
  TransporterFacade * theFacade;
  
  static void execSignal(void* signalSender, 
			 NdbApiSignal* signal, 
			 class LinearSectionPtr ptr[3]);
  
  static void execNodeStatus(void* signalSender, Uint32 nodeId, 
			     bool alive, bool nfCompleted);
  
  int m_lock;
  struct NdbCondition * m_cond;
  Vector<SimpleSignal *> m_jobBuffer;
  Vector<SimpleSignal *> m_usedBuffer;

  template<class T>
  SimpleSignal * waitFor(Uint32 timeOutMillis, T & t);
};

#endif
