#include "trp_client.hpp"

#include "NdbImpl.hpp"

PollGuard::PollGuard(NdbImpl& impl)
{
  m_tp= impl.m_transporter_facade;
  m_waiter= &impl.theWaiter;
  m_locked= true;
  m_block_no= impl.m_ndb.theNdbBlockNumber;
  m_tp->lock_mutex();
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
  if (forceSend)
    m_tp->forceSend(m_block_no);
  else
    m_tp->checkForceSend(m_block_no);

  NDB_TICKS curr_time = NdbTick_CurrentMillisecond();
  NDB_TICKS max_time = curr_time + (NDB_TICKS)wait_time;
  const int maxsleep = (wait_time == -1 || wait_time > 10) ? 10 : wait_time;
  do
  {
    wait_for_input(maxsleep);
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
    wait_time= int(max_time - NdbTick_CurrentMillisecond());
    if (wait_time <= 0)
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
  NdbWaiter *t_poll_owner= m_tp->get_poll_owner();
  if (t_poll_owner != NULL && t_poll_owner != m_waiter)
  {
    /*
      We didn't get hold of the poll "right". We will sleep on a
      conditional mutex until the thread owning the poll "right"
      will wake us up after all data is received. If no data arrives
      we will wake up eventually due to the timeout.
      After receiving all data we take the object out of the cond wait
      queue if it hasn't happened already. It is usually already out of the
      queue but at time-out it could be that the object is still there.
    */
    (void) m_tp->put_in_cond_wait_queue(m_waiter);
    m_waiter->wait(wait_time);
    if (m_waiter->get_cond_wait_index() != TransporterFacade::MAX_NO_THREADS)
    {
      m_tp->remove_from_cond_wait_queue(m_waiter);
    }
  }
  else
  {
    /*
      We got the poll "right" and we poll until data is received. After
      receiving data we will check if all data is received, if not we
      poll again.
    */
#ifdef NDB_SHM_TRANSPORTER
    /*
      If shared memory transporters are used we need to set our sigmask
      such that we wake up also on interrupts on the shared memory
      interrupt signal.
    */
    NdbThread_set_shm_sigmask(FALSE);
#endif
    m_tp->set_poll_owner(m_waiter);
    m_waiter->set_poll_owner(true);
    m_tp->external_poll((Uint32)wait_time);
  }
}

void PollGuard::unlock_and_signal()
{
  NdbWaiter *t_signal_cond_waiter= 0;
  if (!m_locked)
    return;
  /*
   When completing the poll for this thread we must return the poll
   ownership if we own it. We will give it to the last thread that
   came here (the most recent) which is likely to be the one also
   last to complete. We will remove that thread from the conditional
   wait queue and set him as the new owner of the poll "right".
   We will wait however with the signal until we have unlocked the
   mutex for performance reasons.
   See Stevens book on Unix NetworkProgramming: The Sockets Networking
   API Volume 1 Third Edition on page 703-704 for a discussion on this
   subject.
  */
  if (m_tp->get_poll_owner() == m_waiter)
  {
#ifdef NDB_SHM_TRANSPORTER
    /*
      If shared memory transporters are used we need to reset our sigmask
      since we are no longer the thread to receive interrupts.
    */
    NdbThread_set_shm_sigmask(TRUE);
#endif
    m_waiter->set_poll_owner(false);
    t_signal_cond_waiter= m_tp->rem_last_from_cond_wait_queue();
    m_tp->set_poll_owner(t_signal_cond_waiter);
    if (t_signal_cond_waiter)
      t_signal_cond_waiter->set_poll_owner(true);
  }
  if (t_signal_cond_waiter)
    t_signal_cond_waiter->cond_signal();
  m_tp->unlock_mutex();
  m_locked=false;
}
