/*
   Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "trp_client.hpp"
#include "TransporterFacade.hpp"

trp_client::trp_client()
  : m_blockNo(~Uint32(0)), m_facade(0)
{
  m_poll.m_waiting = false;
  m_poll.m_locked = false;
  m_poll.m_poll_owner = false;
  m_poll.m_next = 0;
  m_poll.m_prev = 0;
  m_poll.m_condition = NdbCondition_Create();
}

trp_client::~trp_client()
{
  /**
   * require that trp_client user
   *  doesnt destroy object when holding any locks
   */
  assert(m_poll.m_locked == 0);
  assert(m_poll.m_poll_owner == false);
  assert(m_poll.m_next == 0);
  assert(m_poll.m_prev == 0);

  close();
  NdbCondition_Destroy(m_poll.m_condition);
}

Uint32
trp_client::open(TransporterFacade* tf, int blockNo)
{
  Uint32 res = 0;
  assert(m_facade == 0);
  if (m_facade == 0)
  {
    m_facade = tf;
    res = tf->open_clnt(this, blockNo);
    if (res != 0)
    {
      m_blockNo = refToBlock(res);
    }
    else
    {
      m_facade = 0;
    }
  }
  return res;
}

Uint32
trp_client::getOwnNodeId() const
{
  return m_facade->theOwnId;
}

void
trp_client::close()
{
  if (m_facade)
  {
    m_facade->close_clnt(this);

    m_facade = 0;
    m_blockNo = ~Uint32(0);
  }
}

void
trp_client::start_poll()
{
  m_facade->start_poll(this);
}

void
trp_client::do_poll(Uint32 to)
{
  m_facade->do_poll(this, to);
}

void
trp_client::complete_poll()
{
  m_facade->complete_poll(this);
}

int
trp_client::do_forceSend(int val)
{
  int did_send = 1;
  if (val == 0)
  {
    did_send = m_facade->checkForceSend(m_blockNo);
  }
  else if (val == 1)
  {
    m_facade->forceSend(m_blockNo);
  }
  return did_send;
}

int
trp_client::safe_sendSignal(const NdbApiSignal* signal, Uint32 nodeId)
{
  return m_facade->m_poll_owner->raw_sendSignal(signal, nodeId);
}

#include "NdbImpl.hpp"

PollGuard::PollGuard(NdbImpl& impl)
{
  m_clnt = &impl;
  m_waiter= &impl.theWaiter;
  m_clnt->start_poll();
}

/*
  This is a common routine for possibly forcing the send of buffered signals
  and receiving response the thread is waiting for. It is designed to be
  useful from:
  1) PK, UK lookups using the asynchronous interface
     This routine uses the wait_for_input routine instead since it has
     special end conditions due to the asynchronous nature of its usage.
  2) Scans
  3) dictSignal
  It uses a NdbWaiter object to wait on the events and this object is
  linked into the conditional wait queue. Thus this object contains
  a reference to its place in the queue.

  It replaces the method receiveResponse previously used on the Ndb object
*/
int PollGuard::wait_n_unlock(int wait_time, Uint32 nodeId, Uint32 state,
                             bool forceSend)
{
  int ret_val;
  m_waiter->set_node(nodeId);
  m_waiter->set_state(state);
  ret_val= wait_for_input_in_loop(wait_time, forceSend);
  unlock_and_signal();
  return ret_val;
}

int PollGuard::wait_scan(int wait_time, Uint32 nodeId, bool forceSend)
{
  m_waiter->set_node(nodeId);
  m_waiter->set_state(WAIT_SCAN);
  return wait_for_input_in_loop(wait_time, forceSend);
}

int PollGuard::wait_for_input_in_loop(int wait_time, bool forceSend)
{
  int ret_val;
  m_clnt->do_forceSend(forceSend ? 1 : 0);

  NDB_TICKS curr_time = NdbTick_CurrentNanosecond();
  /* Use nanosecond wait_time for max_time calculation */
  NDB_TICKS max_time = curr_time + ((NDB_TICKS)wait_time * 1000000);
  const int maxsleep = (wait_time == -1 || wait_time > 10) ? 10 : wait_time;
  do
  {
    wait_for_input(maxsleep);
    NDB_TICKS start_time_nanos = curr_time;
    curr_time = NdbTick_CurrentNanosecond();
    m_clnt->recordWaitTimeNanos(curr_time - start_time_nanos);
    Uint32 state= m_waiter->get_state();
    if (state == NO_WAIT)
    {
      return 0;
    }
    else if (state == WAIT_NODE_FAILURE)
    {
      ret_val= -2;
      break;
    }
    if (wait_time == -1)
    {
#ifdef NOT_USED
      ndbout << "Waited WAITFOR_RESPONSE_TIMEOUT, continuing wait" << endl;
#endif
      continue;
    }
    if (curr_time >= max_time)
    {
#ifdef VM_TRACE
      ndbout << "Time-out state is " << m_waiter->get_state() << endl;
#endif
      m_waiter->set_state(WST_WAIT_TIMEOUT);
      ret_val= -1;
      break;
    }
  } while (1);
#ifdef VM_TRACE
  ndbout << "ERR: receiveResponse - theImpl->theWaiter.m_state = ";
  ndbout << m_waiter->get_state() << endl;
#endif
  m_waiter->set_state(NO_WAIT);
  return ret_val;
}

void PollGuard::wait_for_input(int wait_time)
{
  m_clnt->do_poll(wait_time);
}

void PollGuard::unlock_and_signal()
{
  m_clnt->complete_poll();
}
