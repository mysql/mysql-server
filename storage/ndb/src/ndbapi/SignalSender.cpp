/*
   Copyright (c) 2005, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "SignalSender.hpp"
#include <kernel/GlobalSignalNumbers.h>
#include <NdbSleep.h>
#include <SignalLoggerManager.hpp>
#include <signaldata/NFCompleteRep.hpp>
#include <signaldata/NodeFailRep.hpp>
#include <signaldata/TestOrd.hpp>


SimpleSignal::SimpleSignal(bool dealloc)
  : header((BlockReference)0)
{
  memset(ptr, 0, sizeof(ptr));
  deallocSections = dealloc;
}

SimpleSignal::SimpleSignal(const SimpleSignal& src)
  : header(src.header)
{
  deallocSections = true;

  for (Uint32 i = 0; i<NDB_ARRAY_SIZE(ptr); i++)
  {
    ptr[i].p = 0;
    if (src.ptr[i].p != 0)
    {
      ptr[i].p = new Uint32[src.ptr[i].sz];
      ptr[i].sz = src.ptr[i].sz;
      memcpy(ptr[i].p, src.ptr[i].p, 4 * src.ptr[i].sz);
    }
  }
}


SimpleSignal&
SimpleSignal::operator=(const SimpleSignal& src)
{
  deallocSections = true;
  header = src.header;
  for (Uint32 i = 0; i<NDB_ARRAY_SIZE(ptr); i++)
  {
    ptr[i].p = 0;
    if (src.ptr[i].p != 0)
    {
      ptr[i].p = new Uint32[src.ptr[i].sz];
      ptr[i].sz = src.ptr[i].sz;
      memcpy(ptr[i].p, src.ptr[i].p, 4 * src.ptr[i].sz);
    }
  }
  return * this;
}

SimpleSignal::~SimpleSignal(){
  if(!deallocSections)
    return;

  for (Uint32 i = 0; i<NDB_ARRAY_SIZE(ptr); i++)
  {
    if (ptr[i].p != 0)
    {
      delete [] ptr[i].p;
    }
  }
}

void 
SimpleSignal::set(class SignalSender& ss,
		  Uint8  trace, Uint16 recBlock, Uint16 gsn, Uint32 len)
{
  header.set(trace, recBlock, gsn, len);
  header.theSendersBlockRef      = refToBlock(ss.getOwnRef());
}

void
SimpleSignal::print(FILE * out) const {
  fprintf(out, "---- Signal ----------------\n");
  SignalLoggerManager::printSignalHeader(out, header, 0, 0, false);
  SignalLoggerManager::printSignalData(out, header, getDataPtr());
  for(Uint32 i = 0; i<header.m_noOfSections; i++){
    Uint32 len = ptr[i].sz;
    fprintf(out, " --- Section %d size=%d ---\n", i, len);
    Uint32 * signalData = ptr[i].p;
    while(len >= 7){
      fprintf(out, 
              " H\'%.8x H\'%.8x H\'%.8x H\'%.8x H\'%.8x H\'%.8x H\'%.8x\n",
              signalData[0], signalData[1], signalData[2], signalData[3], 
              signalData[4], signalData[5], signalData[6]);
      len -= 7;
      signalData += 7;
    }
    if(len > 0){
      fprintf(out, " H\'%.8x", signalData[0]);
      for(Uint32 i = 1; i<len; i++)
        fprintf(out, " H\'%.8x", signalData[i]);
      fprintf(out, "\n");
    }
  }
}

SignalSender::SignalSender(TransporterFacade *facade, int blockNo)
{
  theFacade = facade;
  Uint32 res = open(theFacade, blockNo);
  assert(res != 0);
  m_blockNo = refToBlock(res);
  m_locked = false;
}

SignalSender::SignalSender(Ndb_cluster_connection* connection)
{
  theFacade = connection->m_impl.m_transporter_facade;
  Uint32 res = open(theFacade, -1);
  assert(res != 0);
  m_blockNo = refToBlock(res);
  m_locked = false;
}

SignalSender::~SignalSender(){
  int i;
  if (m_locked)
    unlock();
  close();

  // free these _after_ closing theFacade to ensure that
  // we delete all signals
  for (i= m_jobBuffer.size()-1; i>= 0; i--)
    delete m_jobBuffer[i];
  for (i= m_usedBuffer.size()-1; i>= 0; i--)
    delete m_usedBuffer[i];
}

int SignalSender::lock()
{
  prepare_poll();
  assert(m_locked == false);
  m_locked = true;
  return 0;
}

int SignalSender::unlock()
{
  assert(m_locked == true);
  if (m_locked)
  {
    complete_poll();
    m_locked = false;
  }
  return 0;
}

Uint32
SignalSender::getOwnRef() const {
  return numberToRef(m_blockNo, theFacade->ownId());
}

NodeBitmask
SignalSender::broadcastSignal(NodeBitmask mask,
                              SimpleSignal& sig,
                              Uint16 recBlock, Uint16 gsn,
                              Uint32 len)
{
  sig.set(*this, TestOrd::TraceAPI, recBlock, gsn, len);

  NodeBitmask result;
  for(Uint32 i = 0; i < MAX_NODES; i++)
  {
    if(mask.get(i) && sendSignal(i, &sig) == SEND_OK)
      result.set(i);
  }
  return result;
}


SendStatus
SignalSender::sendSignal(Uint16 nodeId,
                         SimpleSignal& sig,
                         Uint16 recBlock, Uint16 gsn,
                         Uint32 len)
{
  sig.set(*this, TestOrd::TraceAPI, recBlock, gsn, len);
  return sendSignal(nodeId, &sig);
}

int
SignalSender::sendFragmentedSignal(Uint16 nodeId,
                                   SimpleSignal& sig,
                                   Uint16 recBlock, Uint16 gsn,
                                   Uint32 len)
{
  sig.set(*this, TestOrd::TraceAPI, recBlock, gsn, len);

  int ret = raw_sendFragmentedSignal(&sig.header,
                                     nodeId,
                                     &sig.ptr[0],
                                     sig.header.m_noOfSections);
  if (ret == 0)
  {
    do_forceSend();
    return SEND_OK;
  }
  return SEND_DISCONNECTED;
}

SendStatus
SignalSender::sendSignal(Uint16 nodeId, const SimpleSignal * s)
{
  int ret = raw_sendSignal((NdbApiSignal*)&s->header,
                           nodeId,
                           s->ptr,
                           s->header.m_noOfSections);
  if (ret == 0)
  {
    do_forceSend();
    return SEND_OK;
  }
  return SEND_DISCONNECTED;
}

template<class T>
SimpleSignal *
SignalSender::waitFor(Uint32 timeOutMillis, T & t)
{
  SimpleSignal * s = t.check(m_jobBuffer);
  if(s != 0){
    if (m_usedBuffer.push_back(s))
    {
      return 0;
    }
    assert(s->header.theLength > 0);
    return s;
  }

  /* Remove old signals from usedBuffer */
  for (unsigned i= 0; i < m_usedBuffer.size(); i++)
    delete m_usedBuffer[i];
  m_usedBuffer.clear();

  const NDB_TICKS start = NdbTick_getCurrentTicks();
  Uint32 waited = 0; //ms waited since 'start'
  do {
    const Uint32 wait = (timeOutMillis == 0 ? 10 
                        : timeOutMillis-waited);
    do_poll(wait);
    
    SimpleSignal * s = t.check(m_jobBuffer);
    if(s != 0){
      if (m_usedBuffer.push_back(s))
      {
        return 0;
      }
      assert(s->header.theLength > 0);
      return s;
    }
    
    // Calculate total wait(ms) since 'start'
    const NDB_TICKS now = NdbTick_getCurrentTicks();
    waited = (Uint32)NdbTick_Elapsed(start,now).milliSec();

  } while(timeOutMillis == 0 || waited < timeOutMillis);
  
  return 0;
} 

class WaitForAny {
public:
  WaitForAny() {}
  SimpleSignal * check(Vector<SimpleSignal*> & m_jobBuffer){
    if(m_jobBuffer.size() > 0){
      SimpleSignal * s = m_jobBuffer[0];
      m_jobBuffer.erase(0);
      return s;
    }
    return 0;
  }
};
  
SimpleSignal *
SignalSender::waitFor(Uint32 timeOutMillis){
  
  WaitForAny w;
  return waitFor(timeOutMillis, w);
}

#include <NdbApiSignal.hpp>

void
SignalSender::trp_deliver_signal(const NdbApiSignal* signal,
                                 const struct LinearSectionPtr ptr[3])
{
  if (signal->theVerId_signalNumber == GSN_CLOSE_COMREQ)
  {
    theFacade->perform_close_clnt(this);
    return;
  }
  SimpleSignal * s = new SimpleSignal(true);
  s->header = * signal;
  for(Uint32 i = 0; i<s->header.m_noOfSections; i++){
    s->ptr[i].p = new Uint32[ptr[i].sz];
    s->ptr[i].sz = ptr[i].sz;
    memcpy(s->ptr[i].p, ptr[i].p, 4 * ptr[i].sz);
  }
  m_jobBuffer.push_back(s);
  wakeup();
}

template<class T>
NodeId
SignalSender::find_node(const NodeBitmask& mask, T & t)
{
  unsigned n= 0;
  do {
     n= mask.find(n+1);

     if (n == NodeBitmask::NotFound)
       return 0;

    assert(n < MAX_NODES);

  } while (!t.found_ok(*this, getNodeInfo(n)));

  return n;
}


class FindConfirmedNode {
public:
  bool found_ok(const SignalSender& ss, const trp_node & node){
    return node.is_confirmed();
  }
};


NodeId
SignalSender::find_confirmed_node(const NodeBitmask& mask)
{
  FindConfirmedNode f;
  return find_node(mask, f);
}


class FindConnectedNode {
public:
  bool found_ok(const SignalSender& ss, const trp_node & node){
    return node.is_connected();
  }
};


NodeId
SignalSender::find_connected_node(const NodeBitmask& mask)
{
  FindConnectedNode f;
  return find_node(mask, f);
}


class FindAliveNode {
public:
  bool found_ok(const SignalSender& ss, const trp_node & node){
    return node.m_alive;
  }
};


NodeId
SignalSender::find_alive_node(const NodeBitmask& mask)
{
  FindAliveNode f;
  return find_node(mask, f);
}


template SimpleSignal* SignalSender::waitFor<WaitForAny>(unsigned, WaitForAny&);
template NodeId SignalSender::find_node<FindConfirmedNode>(const NodeBitmask&,
                                                           FindConfirmedNode&);
template NodeId SignalSender::find_node<FindAliveNode>(const NodeBitmask&,
                                                       FindAliveNode&);
template NodeId SignalSender::find_node<FindConnectedNode>(const NodeBitmask&,
                                                           FindConnectedNode&);
template class Vector<SimpleSignal*>;
  
