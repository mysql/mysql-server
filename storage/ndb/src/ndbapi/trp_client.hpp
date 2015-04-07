/*
   Copyright (c) 2010, 2014, Oracle and/or its affiliates. All rights reserved.

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

#ifndef trp_client_hpp
#define trp_client_hpp

#include <ndb_global.h>
#include <NdbCondition.h>
#include <TransporterRegistry.hpp>
#include <NodeBitmask.hpp>

struct trp_node;
class NdbApiSignal;
struct LinearSectionPtr;
struct GenericSectionPtr;
struct TFBuffer;
class trp_client;

class trp_client : TransporterSendBufferHandle
{
  friend class TransporterFacade;
  friend class ReceiveThreadClient;
public:
  trp_client();
  virtual ~trp_client();

  virtual void trp_deliver_signal(const NdbApiSignal *,
                                  const LinearSectionPtr ptr[3]) = 0;
  virtual void trp_wakeup()
    {};

  Uint32 open(class TransporterFacade*, int blockNo = -1,
              bool receive_thread = false);
  void close();

  void start_poll();
  void do_poll(Uint32);
  void complete_poll();
  void wakeup();

  void flush_send_buffers();
  int do_forceSend(int val = 1);

  int raw_sendSignal(const NdbApiSignal*, Uint32 nodeId);
  int raw_sendSignal(const NdbApiSignal*, Uint32 nodeId,
                     const LinearSectionPtr ptr[3], Uint32 secs);
  int raw_sendSignal(const NdbApiSignal*, Uint32 nodeId,
                     const GenericSectionPtr ptr[3], Uint32 secs);
  int raw_sendFragmentedSignal(const NdbApiSignal*, Uint32 nodeId,
                               const LinearSectionPtr ptr[3], Uint32 secs);
  int raw_sendFragmentedSignal(const NdbApiSignal*, Uint32 nodeId,
                               const GenericSectionPtr ptr[3], Uint32 secs);

  const trp_node & getNodeInfo(Uint32 i) const;

  virtual void recordWaitTimeNanos(Uint64 nanos)
    {};

  void lock();
  void unlock();
  /* Interface used by Multiple NDB waiter code */
  void lock_client();
  bool check_if_locked(void);

  Uint32 getOwnNodeId() const;

  /**
   * This sendSignal variant can be called on any trp_client
   *   but perform the send on the trp_client-object that
   *   is currently receiving (m_poll_owner)
   *
   * This variant does flush thread-local send-buffer
   */
  int safe_sendSignal(const NdbApiSignal*, Uint32 nodeId);

  /**
   * This sendSignal variant can be called on any trp_client
   *   but perform the send on the trp_client-object that
   *   is currently receiving (m_poll_owner)
   *
   * This variant does not flush thread-local send-buffer
   */
  int safe_noflush_sendSignal(const NdbApiSignal*, Uint32 nodeId);
private:
  /**
   * TransporterSendBufferHandle interface
   */
  virtual Uint32 *getWritePtr(NodeId node, Uint32 lenBytes, Uint32 prio,
                              Uint32 max_use);
  virtual Uint32 updateWritePtr(NodeId node, Uint32 lenBytes, Uint32 prio);
  virtual void getSendBufferLevel(NodeId node, SB_LevelType &level);
  virtual bool forceSend(NodeId node);

private:
  Uint32 m_blockNo;
  TransporterFacade * m_facade;

  /**
   * This is used for polling
   */
  bool m_locked_for_poll;
public:
  NdbMutex* m_mutex; // thread local mutex...
  void set_locked_for_poll(bool val)
  {
    m_locked_for_poll = val;
  }
  bool is_locked_for_poll()
  {
    return m_locked_for_poll;
  }
private:
  struct PollQueue
  {
    PollQueue();
    void assert_destroy() const;

    enum { PQ_WOKEN, PQ_IDLE, PQ_WAITING } m_waiting;
    bool m_locked;
    bool m_poll_owner;
    bool m_poll_queue;
    trp_client *m_prev;
    trp_client *m_next;
    NdbCondition * m_condition;

    /**
     * This is called by poll owner
     *   before doing external poll
     */
    void start_poll(trp_client* self);

    void lock_client(trp_client*);
    bool check_if_locked(const trp_client*,
                         const Uint32 start) const;
    Uint32 m_locked_cnt;
    Uint32 m_lock_array_size;
    trp_client ** m_locked_clients;
  } m_poll;

  /**
   * This is used for sending
   */
  Uint32 m_send_nodes_cnt;
  NodeId m_send_nodes_list[MAX_NODES];
  NodeBitmask m_send_nodes_mask;
  TFBuffer* m_send_buffers;
};

class PollGuard
{
public:
  PollGuard(class NdbImpl&);
  ~PollGuard() { unlock_and_signal(); }
  int wait_n_unlock(int wait_time, Uint32 nodeId, Uint32 state,
                    bool forceSend= false);
  int wait_for_input_in_loop(int wait_time, bool forceSend);
  void wait_for_input(int wait_time);
  int wait_scan(int wait_time, Uint32 nodeId, bool forceSend);
  void unlock_and_signal();
private:
  class trp_client* m_clnt;
  class NdbWaiter *m_waiter;
  bool  m_complete_poll_called;
};

#include "TransporterFacade.hpp"

inline
bool
trp_client::check_if_locked()
{
  return m_facade->m_poll_owner->m_poll.check_if_locked(this, (Uint32)0);
}

inline
void
trp_client::lock_client()
{
  if (!check_if_locked())
  {
    m_facade->m_poll_owner->m_poll.lock_client(this);
  }
}

inline
void
trp_client::lock()
{
  NdbMutex_Lock(m_mutex);
  assert(m_poll.m_locked == false);
  m_poll.m_locked = true;
}

inline
void
trp_client::unlock()
{
  assert(m_send_nodes_mask.isclear()); // Nothing unsent when unlocking...
  assert(m_poll.m_locked == true);
  m_poll.m_locked = false;
  NdbMutex_Unlock(m_mutex);
}

inline
void
trp_client::wakeup()
{
  m_facade->wakeup(this);
}

inline
int
trp_client::raw_sendSignal(const NdbApiSignal * signal, Uint32 nodeId)
{
  assert(m_poll.m_locked);
  return m_facade->sendSignal(this, signal, nodeId);
}

inline
int
trp_client::raw_sendSignal(const NdbApiSignal * signal, Uint32 nodeId,
                           const LinearSectionPtr ptr[3], Uint32 secs)
{
  assert(m_poll.m_locked);
  return m_facade->sendSignal(this, signal, nodeId, ptr, secs);
}

inline
int
trp_client::raw_sendSignal(const NdbApiSignal * signal, Uint32 nodeId,
                           const GenericSectionPtr ptr[3], Uint32 secs)
{
  assert(m_poll.m_locked);
  return m_facade->sendSignal(this, signal, nodeId, ptr, secs);
}

inline
int
trp_client::raw_sendFragmentedSignal(const NdbApiSignal * signal, Uint32 nodeId,
                                     const LinearSectionPtr ptr[3], Uint32 secs)
{
  assert(m_poll.m_locked);
  return m_facade->sendFragmentedSignal(this, signal, nodeId, ptr, secs);
}

inline
int
trp_client::raw_sendFragmentedSignal(const NdbApiSignal * signal, Uint32 nodeId,
                                     const GenericSectionPtr ptr[3],
                                     Uint32 secs)
{
  assert(m_poll.m_locked);
  return m_facade->sendFragmentedSignal(this, signal, nodeId, ptr, secs);
}

inline
trp_client::PollQueue::PollQueue()
{
  m_waiting = PQ_IDLE;
  m_locked = false;
  m_poll_owner = false;
  m_poll_queue = false;
  m_next = 0;
  m_prev = 0;
  m_condition = NdbCondition_Create();
  m_locked_cnt = 0;
  m_lock_array_size = 0;
  m_locked_clients = NULL;
}

inline
void
trp_client::PollQueue::assert_destroy() const
{
  if (m_waiting != PQ_IDLE ||
      m_locked == true ||
      m_poll_owner == true ||
      m_poll_queue == true ||
      m_next != 0 ||
      m_prev != 0 ||
      m_locked_cnt != 0)
  {
    ndbout << "ERR: ~trp_client: Deleting trp_clnt in use: waiting"
	   << m_waiting
	   << " locked  " << m_locked
	   << " poll_owner " << m_poll_owner
	   << " poll_queue " << m_poll_queue
	   << " next " << m_next
	   << " prev " << m_prev
	   << " condition " << m_condition << endl;
    require(false);
  }
}

#endif
