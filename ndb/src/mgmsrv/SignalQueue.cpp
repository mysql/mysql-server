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

#include <ndb_global.h>
#include "SignalQueue.hpp"

SignalQueue::SignalQueue() {
  m_mutex = NdbMutex_Create();
  m_cond = NdbCondition_Create();
  m_signalQueueHead = NULL;
}

SignalQueue::~SignalQueue() {
  {
    Guard g(m_mutex);
    while(m_signalQueueHead != NULL)
      delete pop();
  }
  NdbMutex_Destroy(m_mutex);
  m_mutex = NULL;
  NdbCondition_Destroy(m_cond);
  m_cond = NULL;
}

NdbApiSignal *
SignalQueue::pop() {
  NdbApiSignal *ret;

  if(m_signalQueueHead == NULL)
    return NULL;

  ret = m_signalQueueHead->signal;

  QueueEntry *old = m_signalQueueHead;
  m_signalQueueHead = m_signalQueueHead->next;

  delete old;

  return ret;
}

void
SignalQueue::receive(void *me, NdbApiSignal *signal) {
  SignalQueue *q = (SignalQueue *)me;
  q->receive(signal);
}

void
SignalQueue::receive(NdbApiSignal *signal) {
  QueueEntry *n = new QueueEntry();
  n->signal = signal;
  n->next = NULL;

  Guard guard(m_mutex);

  if(m_signalQueueHead == NULL) {
    m_signalQueueHead = n;
    NdbCondition_Broadcast(m_cond);
    return;
  }

  QueueEntry *cur = m_signalQueueHead;

  while(cur->next != NULL)
    cur = cur->next;

  cur->next = n;

  NdbCondition_Broadcast(m_cond);
}

NdbApiSignal *
SignalQueue::waitFor(int gsn, NodeId nodeid, Uint32 timeout) {
  Guard g(m_mutex);

  if(m_signalQueueHead == NULL)
    NdbCondition_WaitTimeout(m_cond, m_mutex, timeout);

  if(m_signalQueueHead == NULL)
    return NULL;

  if(gsn != 0 && 
     m_signalQueueHead->signal->readSignalNumber() != gsn)
    return NULL;

  if(nodeid != 0 &&
     refToNode(m_signalQueueHead->signal->theSendersBlockRef) != nodeid)
    return NULL;

  return pop();
}
