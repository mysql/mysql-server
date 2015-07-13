/*
   Copyright (c) 2010, 2015, Oracle and/or its affiliates. All rights reserved.

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
#include <NdbMem.h>

trp_client::trp_client()
  : m_blockNo(~Uint32(0)), m_facade(0)
{
  m_mutex = NdbMutex_Create();

  m_locked_for_poll = false;
  m_send_nodes_cnt = 0;
  m_send_buffers = new TFBuffer[MAX_NODES];
}

trp_client::~trp_client()
{
  /**
   * require that trp_client user
   *  doesnt destroy object when holding any locks
   */
  m_poll.assert_destroy();

  close();
  NdbCondition_Destroy(m_poll.m_condition);
  m_poll.m_condition = NULL;
  NdbMutex_Destroy(m_mutex);
  m_mutex = NULL;

  assert(m_send_nodes_cnt == 0);
  assert(m_locked_for_poll == false);
  delete [] m_send_buffers;
}

Uint32
trp_client::open(TransporterFacade* tf, int blockNo,
                 bool receive_thread)
{
  Uint32 res = 0;
  assert(m_facade == 0);
  if (m_facade == 0)
  {
    m_facade = tf;
    /**
      For user threads we only store up to 16 threads before waking
      them up, for receiver threads we store up to 256 threads before
      waking them up.
    */
    if (receive_thread)
    {
      m_poll.m_lock_array_size = 256;
    }
    else
    {
      m_poll.m_lock_array_size = 16;
    }
    m_poll.m_locked_clients = (trp_client**)
      NdbMem_Allocate(sizeof(trp_client**) * m_poll.m_lock_array_size);
    if (m_poll.m_locked_clients == NULL)
    {
      return 0;
    }
    res = tf->open_clnt(this, blockNo);
    if (res != 0)
    {
      m_blockNo = refToBlock(res);
    }
    else
    {
      NdbMem_Free(m_poll.m_locked_clients);
      m_poll.m_locked_clients = NULL;
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
    if (m_poll.m_locked_clients)
    {
      NdbMem_Free(m_poll.m_locked_clients);
      m_poll.m_locked_clients = NULL;
    }
  }
}

void
trp_client::start_poll()
{
  NdbMutex_Lock(m_mutex);
  assert(m_poll.m_locked == false);
  m_poll.m_locked = true;
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
  assert(m_poll.m_locked == true);
  m_facade->complete_poll(this);
  m_poll.m_locked = false;
  NdbMutex_Unlock(m_mutex);
}

int
trp_client::do_forceSend(int val)
{
  /**
   * since force send is disabled in this "version"
   *   set forceSend=1 always...
   */
  val = 1;

  if (val == 0)
  {
    flush_send_buffers();
    return 0;
  }
  else if (val == 1)
  {
    for (Uint32 i = 0; i < m_send_nodes_cnt; i++)
    {
      Uint32 n = m_send_nodes_list[i];
      TFBuffer* b = m_send_buffers + n;
      TFBufferGuard g0(* b);
      m_facade->flush_and_send_buffer(n, b);
      b->clear();
    }
    m_send_nodes_cnt = 0;
    m_send_nodes_mask.clear();
    return 1;
  }
  return 0;
}

int
trp_client::safe_noflush_sendSignal(const NdbApiSignal* signal, Uint32 nodeId)
{
  return m_facade->m_poll_owner->raw_sendSignal(signal, nodeId);
}

int
trp_client::safe_sendSignal(const NdbApiSignal* signal, Uint32 nodeId)
{
  int res;
  if ((res = safe_noflush_sendSignal(signal, nodeId)) != -1)
  {
    m_facade->m_poll_owner->flush_send_buffers();
  }
  return res;
}

Uint32 *
trp_client::getWritePtr(NodeId node, Uint32 lenBytes, Uint32 prio,
                        Uint32 max_use)
{
  TFBuffer* b = m_send_buffers+node;
  TFBufferGuard g0(* b);
  bool found = m_send_nodes_mask.get(node);
  if (likely(found))
  {
    TFPage * page = b->m_tail;
    assert(page != 0);
    if (page->m_bytes + page->m_start + lenBytes <= page->max_data_bytes())
    {
      return (Uint32 *)(page->m_data + page->m_start + page->m_bytes);
    }
  }
  else
  {
    Uint32 cnt = m_send_nodes_cnt;
    m_send_nodes_mask.set(node);
    m_send_nodes_list[cnt] = node;
    m_send_nodes_cnt = cnt + 1;
  }

  TFPage* page = m_facade->alloc_sb_page();
  if (likely(page != 0))
  {
    page->init();

    if (b->m_tail == NULL)
    {
      assert(!found);
      b->m_head = page;
      b->m_tail = page;
    }
    else
    {
      assert(found);
      assert(b->m_head != NULL);
      b->m_tail->m_next = page;
      b->m_tail = page;
    }
    return (Uint32 *)(page->m_data);
  }

  if (b->m_tail == 0)
  {
    assert(!found);
    m_send_nodes_mask.clear(node);
    m_send_nodes_cnt--;
  }
  else
  {
    assert(found);
  }

  return NULL;
}

/**
 * This is the implementation used by the NDB API. I update the
 * current send buffer size every time a thread gets the send mutex and
 * links their buffers to the common pool of buffers. I recalculate the
 * buffer size also every time a send to the node has been completed.
 *
 * The values we read here are read unprotected, the idea is that the
 * value reported from here should only used for guidance. So it should
 * only implement throttling, it should not completely stop send activities,
 * merely delay it. So the harm in getting an inconsistent view of data
 * should not be high. Also we expect measures of slowing down to occur
 * at a fairly early stage, so not close to when the buffers are filling up.
 */
void
trp_client::getSendBufferLevel(NodeId node, SB_LevelType &level)
{
  Uint32 current_send_buffer_size = m_facade->get_current_send_buffer_size(node);
  Uint64 tot_send_buffer_size =
    m_facade->m_send_buffer.get_total_send_buffer_size();
  Uint64 tot_used_send_buffer_size =
    m_facade->m_send_buffer.get_total_used_send_buffer_size();
  calculate_send_buffer_level(current_send_buffer_size,
                              tot_send_buffer_size,
                              tot_used_send_buffer_size,
                              0,
                              level);
  return;
}

Uint32
trp_client::updateWritePtr(NodeId node, Uint32 lenBytes, Uint32 prio)
{
  TFBuffer* b = m_send_buffers+node;
  TFBufferGuard g0(* b);
  assert(m_send_nodes_mask.get(node));
  assert(b->m_head != 0);
  assert(b->m_tail != 0);

  TFPage *page = b->m_tail;
  assert(page->m_bytes + lenBytes <= page->max_data_bytes());
  page->m_bytes += lenBytes;
  b->m_bytes_in_buffer += lenBytes;
  return b->m_bytes_in_buffer;
}

void
trp_client::flush_send_buffers()
{
  assert(m_poll.m_locked);
  Uint32 cnt = m_send_nodes_cnt;
  for (Uint32 i = 0; i<cnt; i++)
  {
    Uint32 node = m_send_nodes_list[i];
    assert(m_send_nodes_mask.get(node));
    TFBuffer* b = m_send_buffers + node;
    TFBufferGuard g0(* b);
    m_facade->flush_send_buffer(node, b);
    b->clear();
  }

  m_send_nodes_cnt = 0;
  m_send_nodes_mask.clear();
}

bool
trp_client::forceSend(NodeId node)
{
  do_forceSend();
  return true;
}

#include "NdbImpl.hpp"

PollGuard::PollGuard(NdbImpl& impl)
{
  m_clnt = &impl;
  m_waiter= &impl.theWaiter;
  m_clnt->start_poll();
  m_complete_poll_called = false;
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

  NDB_TICKS curr_ticks = NdbTick_getCurrentTicks();
  /* Use nanosecond to calculate when wait_time has expired. */
  Int64 remain_wait_nano = ((Int64)wait_time) * 1000000;
  const int maxsleep = (wait_time == -1 || wait_time > 10) ? 10 : wait_time;
  do
  {
    wait_for_input(maxsleep);
    const NDB_TICKS start_ticks = curr_ticks;
    curr_ticks = NdbTick_getCurrentTicks();
    const Uint64 waited_nano = NdbTick_Elapsed(start_ticks,curr_ticks).nanoSec();
    m_clnt->recordWaitTimeNanos(waited_nano);
    Uint32 state= m_waiter->get_state();
    if (state == NO_WAIT)
    {
      return 0;
    }
    else if (state == WAIT_NODE_FAILURE)
    {
      ret_val= -2;
      m_waiter->set_state(NO_WAIT);
      break;
    }
    if (wait_time == -1)
    {
#ifdef NOT_USED
      ndbout << "Waited WAITFOR_RESPONSE_TIMEOUT, continuing wait" << endl;
#endif
      continue;
    }
    remain_wait_nano -= waited_nano;
    if (remain_wait_nano <= 0)
    {
#ifdef VM_TRACE
      ndbout << "Time-out state is " << m_waiter->get_state() << endl;
#endif
      m_waiter->set_state(WST_WAIT_TIMEOUT);
      ret_val= -1;
      break;
    }
    /**
     * Ensure any signals sent by receivers are sent by send thread
     * eventually by flushing buffers to global area.
     */
    m_clnt->flush_send_buffers();
  } while (1);
#ifdef VM_TRACE
  ndbout << "ERR: receiveResponse - theImpl->theWaiter.m_state = ";
  ndbout << m_waiter->get_state() << endl;
#endif
  return ret_val;
}

void PollGuard::wait_for_input(int wait_time)
{
  m_clnt->do_poll(wait_time);
}

void PollGuard::unlock_and_signal()
{
  if (!m_complete_poll_called)
  {
    m_clnt->complete_poll();
    m_complete_poll_called = true;
  }
}
