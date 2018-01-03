/*
   Copyright (c) 2010, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include "trp_client.hpp"
#include "TransporterFacade.hpp"

trp_client::trp_client()
  : m_blockNo(~Uint32(0)),
    m_facade(NULL),
    m_locked_for_poll(false),
    m_mutex(NULL),
    m_poll(),
    m_send_nodes_mask(),
    m_send_nodes_cnt(0),
    m_send_buffers(NULL),
    m_flushed_nodes_mask()
{
  m_mutex = NdbMutex_Create();
  m_send_buffers = new TFBuffer[MAX_NODES];
}

trp_client::~trp_client()
{
  close();
  NdbMutex_Destroy(m_mutex);

  m_mutex = NULL;
  assert(m_send_nodes_cnt == 0);
  assert(m_locked_for_poll == false);
  delete [] m_send_buffers;
}

trp_client::PollQueue::PollQueue()
  : m_waiting(PQ_IDLE),
    m_locked(false),
    m_poll_owner(false),
    m_poll_queue(false),
    m_prev(NULL),
    m_next(NULL),
    m_condition(NULL)
{
  m_condition = NdbCondition_Create();
}

trp_client::PollQueue::~PollQueue()
{
  /**
   * Require that trp_client user
   * doesnt destroy object when holding any locks.
   */
  if (unlikely(
      m_waiting != PQ_IDLE ||
      m_locked == true ||
      m_poll_owner == true ||
      m_poll_queue == true ||
      m_next != 0 ||
      m_prev != 0))
  {
    ndbout << "ERR: ::~PollQueue: Deleting trp_clnt in use: waiting"
           << m_waiting
	   << " locked  " << m_locked
	   << " poll_owner " << m_poll_owner
	   << " poll_queue " << m_poll_queue
	   << " next " << m_next
	   << " prev " << m_prev
           << endl;
    require(false);
  }
  NdbCondition_Destroy(m_condition);
  m_condition = NULL;
}

Uint32
trp_client::open(TransporterFacade* tf, int blockNo)
{
  Uint32 res = 0;
  assert(m_facade == NULL);
  if (m_facade == NULL)
  {
    m_facade = tf;
    res = tf->open_clnt(this, blockNo);
    if (res != 0)
    {
      m_blockNo = refToBlock(res);
    }
    else
    {
      m_facade = NULL;
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

    m_facade = NULL;
    m_blockNo = ~Uint32(0);
  }
}

/**
 * The client has to 'poll' the transporter in order to receive
 * its result. The call to ::do_poll() should be encapsulate with
 * a ::prepare_poll() - ::complete_poll() pair.
 */
void
trp_client::prepare_poll()
{
  NdbMutex_Lock(m_mutex);
  assert(m_poll.m_locked == false);
  assert(m_poll.m_poll_queue == false);
  assert(m_poll.m_waiting == trp_client::PollQueue::PQ_IDLE);
  assert(has_unflushed_sends() == false); //Flushed prior to poll-wait
  m_poll.m_locked = true;
}

void
trp_client::do_poll(Uint32 to)
{
  assert(m_poll.m_locked == true);
  assert(m_poll.m_poll_queue == false);
  assert(m_poll.m_waiting == trp_client::PollQueue::PQ_IDLE);
  m_facade->do_poll(this, to);
}

void
trp_client::complete_poll()
{
  assert(m_poll.m_locked == true);
  assert(m_poll.m_poll_queue == false);
  assert(m_poll.m_waiting == trp_client::PollQueue::PQ_IDLE);
  /**
   * Ensure any signals sent by receiver/poll owner has been
   * flushed to the global Transporter buffers.
   * The send thread will eventually send the transporter buffers.
   */
  assert(has_unflushed_sends() == false);
  m_poll.m_locked = false;
  NdbMutex_Unlock(m_mutex);
}

/**
 * Send to the set of 'nodes' this client has produced messages to.
 * We either try to do the send immediately ourself if 'forceSend',
 * or we may choose an adaptive approach where (part of) the send
 * may be ofloaded to the send thread.
 */
int
trp_client::do_forceSend(bool forceSend)
{
  flush_send_buffers();

  if (forceSend)
  {
    m_facade->try_send_all(m_flushed_nodes_mask);
  }
  else
  {
    m_facade->do_send_adaptive(m_flushed_nodes_mask);
  }
  m_flushed_nodes_mask.clear();

  /**
   * Note that independent of whether we 'forceSend' or not, we *did*
   * send. Possibly with a small delay though, if we did the send
   * with assist from the send thread. However, that is the same 
   * whether the send was 'forced' or 'adaptive'
   *
   * So we always return '1' -> 'did_send'
   */
  return 1;
}

/**
 * The 'safe' sendSignal() methods has to be used instead of the
 * other sendSignal methods when a reply signal has to be
 * sent by the client getting a signal 'delivered'.
 *
 * See 'is_poll_owner_thread()'-comments for more details.
 */
int
trp_client::safe_noflush_sendSignal(const NdbApiSignal* signal, Uint32 nodeId)
{
  // This thread must be the poll owner
  assert(m_facade->is_poll_owner_thread());
  return m_facade->m_poll_owner->raw_sendSignal(signal, nodeId);
}

int trp_client::safe_noflush_sendSignal(const NdbApiSignal* signal, Uint32 nodeId,
                                        const LinearSectionPtr ptr[3], Uint32 secs)
{
  // This thread must be the poll owner
  assert(m_facade->is_poll_owner_thread());
  return m_facade->m_poll_owner->raw_sendSignal(signal, nodeId, ptr, secs);
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

int
trp_client::safe_sendSignal(const NdbApiSignal* signal, Uint32 nodeId,
                            const LinearSectionPtr ptr[3], Uint32 secs)
{
  int res;
  if ((res = safe_noflush_sendSignal(signal, nodeId, ptr, secs)) != -1)
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

  TFPage* page = m_facade->alloc_sb_page(node);
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

/**
 * Append the private client send buffers to the
 * TransporterFacade lists of prepared send buffers.
 * The TransporterFacade may then send these whenever
 * it find convienient.
 *
 * Build an aggregated bitmap 'm_flushed_nodes_mask'
 * of nodes this client has flushed messages to.
 * Client must ensure that the messages to these nodes
 * are force-sent before it starts waiting for any reply.
 *
 * Need to be called with the 'm_mutex' held
 */
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

  m_flushed_nodes_mask.bitOR(m_send_nodes_mask);
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
  m_clnt->prepare_poll();
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

int PollGuard::wait_for_input_in_loop(int max_wait_ms, bool forceSend)
{
  int ret_val;
  m_clnt->do_forceSend(forceSend);

  const NDB_TICKS start = NdbTick_getCurrentTicks();
  int remain_wait_ms = max_wait_ms;
#ifdef VM_TRACE
  const bool verbose = (m_waiter->get_state() != WAIT_EVENT);
#endif
  do
  {
    int maxsleep = (max_wait_ms == -1) ? 60*1000 : remain_wait_ms;
    DBUG_EXECUTE_IF("ndb_simulate_nodefail", {
      if (maxsleep > 10)
        maxsleep = 10;
    });
    wait_for_input(maxsleep);

    const NDB_TICKS now = NdbTick_getCurrentTicks();
    m_clnt->recordWaitTimeNanos(NdbTick_Elapsed(start,now).nanoSec());
    Uint32 state= m_waiter->get_state();

    DBUG_EXECUTE_IF("ndb_simulate_nodefail", {
      DBUG_PRINT("info", ("Simulating node failure while waiting for response"));
      state = WAIT_NODE_FAILURE;
    });

    if (likely(state == NO_WAIT))
    {
      return 0;
    }
    else if (state == WAIT_NODE_FAILURE)
    {
      ret_val= -2;
      m_waiter->set_state(NO_WAIT);
      break;
    }
    if (max_wait_ms == -1)
    {
#ifdef NOT_USED
      ndbout << "Waited WAITFOR_RESPONSE_TIMEOUT, continuing wait" << endl;
#endif
      continue;
    }
    remain_wait_ms = max_wait_ms -
      (int)NdbTick_Elapsed(start,now).milliSec();

    if (remain_wait_ms <= 0)
    {
#ifdef VM_TRACE
      if (verbose)
      {
        ndbout << "Time-out state is " << m_waiter->get_state() << endl;
      }
#endif
      m_waiter->set_state(WST_WAIT_TIMEOUT);
      ret_val= -1;
      break;
    }
    /**
     * Ensure no reply-signals sent by receivers remains unflushed.
     */
    assert(m_clnt->has_unflushed_sends() == false);
  } while (1);
#ifdef VM_TRACE
  if (verbose)
  {
    ndbout << "ERR: receiveResponse - theImpl->theWaiter.m_state = ";
    ndbout << m_waiter->get_state() << endl;
  }
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
