/*
   Copyright (c) 2010, 2023, Oracle and/or its affiliates.

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
  ~trp_client() override;

  virtual void trp_deliver_signal(const NdbApiSignal *,
                                  const LinearSectionPtr ptr[3]) = 0;
  virtual void trp_wakeup()
    {}

  Uint32 open(class TransporterFacade*, int blockNo = -1);
  void close();

  void prepare_poll();
  void do_poll(Uint32);
  void complete_poll();
  void wakeup();

  // Called under m_mutex protection
  void set_enabled_send(const NodeBitmask &nodes);
  void enable_send(NodeId node);
  void disable_send(NodeId node);
  
  void flush_send_buffers();
  int do_forceSend(bool forceSend = true);

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

  virtual void recordWaitTimeNanos(Uint64 /*nanos*/) {}

  void lock();
  void unlock();
  /* Interface used by Multiple NDB waiter code */
  void lock_client();
  bool check_if_locked(void) const;

  Uint32 getOwnNodeId() const;

  /**
   * This sendSignal variant can be called on any trp_client
   *   but perform the send on the trp_client-object that
   *   is currently receiving (m_poll_owner)
   *
   * This variant does flush thread-local send-buffer
   */
  int safe_sendSignal(const NdbApiSignal*, Uint32 nodeId);
  int safe_sendSignal(const NdbApiSignal*, Uint32 nodeId,
                      const LinearSectionPtr ptr[3], Uint32 secs);

  /**
   * This sendSignal variant can be called on any trp_client
   *   but perform the send on the trp_client-object that
   *   is currently receiving (m_poll_owner)
   *
   * This variant does not flush thread-local send-buffer
   */
  int safe_noflush_sendSignal(const NdbApiSignal*, Uint32 nodeId);
  int safe_noflush_sendSignal(const NdbApiSignal*, Uint32 nodeId,
                              const LinearSectionPtr ptr[3], Uint32 secs);

private:
  /**
   * TransporterSendBufferHandle interface
   */
  bool isSendEnabled(NodeId node) const override;
  Uint32 *getWritePtr(NodeId nodeId,
                      TrpId trp_id,
                      Uint32 lenBytes,
                      Uint32 prio,
                      Uint32 max_use,
                      SendStatus *error) override;
  Uint32 updateWritePtr(NodeId nodeId,
                        TrpId trp_id,
                        Uint32 lenBytes,
                        Uint32 prio) override;
  void getSendBufferLevel(NodeId node, SB_LevelType &level) override;
  bool forceSend(NodeId nodeId, TrpId trp_id) override;

private:
  Uint32 m_blockNo;
  TransporterFacade * m_facade;

  /**
   * This is used for polling by the poll_owner:
   *   A client is 'locked_for_poll' iff it is registered in the 
   *   m_locked_clients[] array by the poll owner.
   *   'm_locked_for_poll' also implies 'm_mutex' is locked
   */
  bool m_locked_for_poll;
  bool m_is_receiver_thread;
public:
  bool is_receiver_thread()
  {
    return m_is_receiver_thread;
  }
  NdbMutex* m_mutex; // thread local mutex...
  void set_locked_for_poll(bool val)
  {
    m_locked_for_poll = val;
  }
  bool is_locked_for_poll() const
  {
    return m_locked_for_poll;
  }

  bool has_unflushed_sends() const
  {
    return m_send_nodes_cnt > 0;
  }
private:
  struct PollQueue
  {
    PollQueue();
    ~PollQueue();

    /**
     * PQ_WAITING - trp_client either waits in the poll-queue, or
     *              owns the poll-right. In both cases it waits 
     *              for a ::wakeup() in order to proceed.
     * PQ_WOKEN -   trp_client got a ::wakeup() while PQ_WAITING.
     *              Might be waiting in the poll-queue, needing to
     *              be signaled, or trp_client is the poller, which
     *              now can complete.
     * PQ_IDLE -    This *PollQueue instance* is idle/unused.
     *              (Thus, the client itself is likely active)
     */
    enum { PQ_WOKEN, PQ_IDLE, PQ_WAITING } m_waiting;
    bool m_locked;
    bool m_poll_owner;
    bool m_poll_queue;
    trp_client *m_prev;
    trp_client *m_next;
    NdbCondition * m_condition;
  } m_poll;

  NodeBitmask m_enabled_nodes_mask;

  /**
   * This is used for sending.
   * m_send_nodes_* are the nodes we have pending unflushed
   * messages to
   */
  NodeBitmask m_send_nodes_mask;
  Uint32 m_send_nodes_cnt;
  NodeId m_send_nodes_list[MAX_NODES];

  TFBuffer* m_send_buffers;

  /**
   * The m_flushed_nodes_mask are the aggregated set of nodes
   * we have flushed messages to, which are yet not known to
   * to have been (force-)sent by the transporter.
   */
  NodeBitmask m_flushed_nodes_mask;
};

class PollGuard
{
public:
  PollGuard(class NdbImpl&);
  ~PollGuard() { unlock_and_signal(); }
  int wait_n_unlock(int wait_time, Uint32 nodeId, Uint32 state,
                    bool forceSend= false);
  void wait_for_input(int wait_time);
  int wait_scan(int wait_time, Uint32 nodeId, bool forceSend);
  void unlock_and_signal();
private:
  int wait_for_input_in_loop(int wait_time, bool forceSend);
  class trp_client* m_clnt;
  class NdbWaiter *m_waiter;
  bool  m_complete_poll_called;
};

#include "TransporterFacade.hpp"

inline
bool
trp_client::check_if_locked() const
{
  return m_facade->check_if_locked(this, 0);
}

inline
void
trp_client::lock_client()
{
  if (!check_if_locked())
  {
    m_facade->lock_client(this);
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
  assert(has_unflushed_sends() == false); //Nothing unsent when unlocking...
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

#endif
