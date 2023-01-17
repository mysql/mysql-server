/*
  Copyright (c) 2011, 2023, Oracle and/or its affiliates.

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

#include "util/require.h"
#include "WakeupHandler.hpp"
#include "Ndb.hpp"
#include "NdbImpl.hpp"
#include "trp_client.hpp"

// ***** Multiwait handler ****

/**
 * An instance of this class is used when a single thread
 * wants to wait for the asynchronous completion of transactions
 * on multiple Ndb objects.
 * When the thread starts waiting, all Ndb objects are checked
 * for CompletedTransactions, and their wakeHandler is set to
 * poin to the same MultiNdbWakeupHandler object.  The thread
 * is then put to sleep / polls on a designated Ndb object.
 *
 * As transactions complete, the MultiNdbWakeHandler object
 * moves their Ndb objects to the start of the passed Ndb
 * object list and determines whether enough have completed
 * to wake the waiting thread.
 * When enough have completed, the waiting thread is woken via
 * the designated Ndb object.
 *
 * The design only supports one instance of the MultiNdbWakeupHandler
 * object per ndb cluster connection and this can only be used from
 * one thread.
 */

MultiNdbWakeupHandler::MultiNdbWakeupHandler(Ndb* _wakeNdb)
  : wakeNdb(_wakeNdb)
{
  localWakeupMutexPtr = NdbMutex_Create();
  assert(localWakeupMutexPtr);
  /* Register the waiter Ndb to receive wakeups for all Ndbs in the group */
  PollGuard pg(* wakeNdb->theImpl);
  ignore_wakeups();
  bool rc = wakeNdb->theImpl->m_transporter_facade->registerForWakeup(wakeNdb->theImpl);
  require(rc);
  wakeNdb->theImpl->wakeHandler = this;
}


MultiNdbWakeupHandler::~MultiNdbWakeupHandler()
{
  if (localWakeupMutexPtr)
  {
    NdbMutex_Destroy(localWakeupMutexPtr);
    localWakeupMutexPtr = nullptr;
  }
  PollGuard pg(* wakeNdb->theImpl);
  bool rc = wakeNdb->theImpl->m_transporter_facade->
    unregisterForWakeup(wakeNdb->theImpl);
  require(rc);
}


void MultiNdbWakeupHandler::finalize_wait(int *nready)
{
  Uint32 num_completed_trans = 0;
  for (Uint32 i = 0; i < cnt; i++)
  {
    Ndb *obj = objs[i];

    NdbMutex_Lock(obj->theImpl->m_mutex);
    if (obj->theNoOfCompletedTransactions)
    {
      swapNdbsInArray(i, num_completed_trans);
      num_completed_trans++;
    }
    unregisterNdb(obj);
    NdbMutex_Unlock(obj->theImpl->m_mutex);
  }
  *nready = num_completed_trans;
}

void MultiNdbWakeupHandler::registerNdb(Ndb* obj)
{
  NdbMutex_Lock(obj->theImpl->m_mutex);
  obj->theImpl->wakeHandler = this;
  /* It may already have some completed transactions */
  if (obj->theNoOfCompletedTransactions)
  {
    NdbMutex_Lock(localWakeupMutexPtr);
    numNdbsWithCompletedTrans++;
    NdbMutex_Unlock(localWakeupMutexPtr);
  }
  NdbMutex_Unlock(obj->theImpl->m_mutex);
}


void MultiNdbWakeupHandler::unregisterNdb(Ndb *obj)
{
  obj->theImpl->wakeHandler = nullptr;
}


int MultiNdbWakeupHandler::waitForInput(Ndb** _objs,
                                        int _cnt,
                                        int min_req,
                                        int timeout_millis,
                                        int *nready)
{
  /**
    Initialise object for waiting.

    numNdbsWithCompletedTrans: 
    Keeps track of number of transactions completed and is protected by
    localWakeupMutexPtr-mutex. It can be set to 0 without mutex protection
    since the poll owner thread will not access it until we have registered
    at least one NDB object.

    minNdbsToWake:
    This is used by both notifyWakeup and notifyTransactionsCompleted to
    see whether we're currently waiting to be woken up. We always access
    it protected by the Ndb mutex on the waiter object.

    objs:
    This is a local array set when waitForInput is called. It is only
    manipulated by the thread calling waitForInput. So it doesn't need
    any protection when used.

    cnt:
    This is a local counter of how many objects we're waiting for, only
    used by the thread calling waitForInput, so no need to protect it.

    woken:
    This is set by notifyWakeup to indicate we should wake up even if no
    NDB objects are done. This is protected by the Ndb mutex on the waiter
    object.
  */

  numNdbsWithCompletedTrans = 0;
  cnt = (Uint32)_cnt;
  objs = _objs;

  NdbMutex_Lock(wakeNdb->theImpl->m_mutex);
  ignore_wakeups();
  NdbMutex_Unlock(wakeNdb->theImpl->m_mutex);

  /*
    Before sleeping, we register each Ndb, and check whether it already
    has any completed transactions.
  */
  for (Uint32 i = 0; i < cnt; i++)
  {
    /* Register the Ndb's */
    registerNdb(objs[i]);
  }

  int ret = -1;
  bool first = true;
  const NDB_TICKS start = NdbTick_getCurrentTicks();
  const int maxTime = timeout_millis;
  {
    PollGuard pg(*wakeNdb->theImpl);
    do
    {
      if (first)
      {
        set_wakeup(min_req);
        if (isReadyToWake())  // already enough
        {
          pg.wait_for_input(0);
          woken = false;
          ignore_wakeups();
          ret = 0;
          break;
        }
        wakeNdb->theImpl->theWaiter.set_node(0);
        wakeNdb->theImpl->theWaiter.set_state(WAIT_TRANS);
        first = false;
      }
      /* PollGuard will put us to sleep until something relevant happens */
      pg.wait_for_input(timeout_millis);
      wakeNdb->theImpl->incClientStat(Ndb::WaitExecCompleteCount, 1);
 
      if (isReadyToWake())
      {
        woken = false;
        ignore_wakeups();
        ret = 0;
        break;
      }
      const NDB_TICKS now = NdbTick_getCurrentTicks();
      timeout_millis = (maxTime - (int)NdbTick_Elapsed(start,now).milliSec());
      if (timeout_millis <= 0)
      {
        ignore_wakeups();
        break;
      }
    } while (1);
  }
  finalize_wait(nready);
  return ret;
}


void MultiNdbWakeupHandler::swapNdbsInArray(Uint32 indexA, Uint32 indexB)
{
  /* Generally used to move an Ndb object down the list
   * (bubble sort), so that it is part of a contiguous
   * list of Ndbs with completed transactions to return
   * to caller.
   * If it's already in the given position, no effect
   */
  assert(indexA < cnt);
  assert(indexB < cnt);

  Ndb* a = objs[indexA];
  Ndb* b = objs[indexB];

  objs[indexA] = b;
  objs[indexB] = a;
}

void MultiNdbWakeupHandler::notifyTransactionCompleted(Ndb* from
                                                       [[maybe_unused]])
{
  Uint32 num_completed_trans;
  if (!wakeNdb->theImpl->is_locked_for_poll())
  {
    wakeNdb->theImpl->lock_client();
  }

  assert(wakeNdb->theImpl->wakeHandler == this);
  assert(from != wakeNdb);

  /* Some Ndb object has just completed another transaction.
     Ensure that it's in the completed Ndbs list
  */
  NdbMutex_Lock(localWakeupMutexPtr);
  numNdbsWithCompletedTrans++;
  num_completed_trans = numNdbsWithCompletedTrans;
  NdbMutex_Unlock(localWakeupMutexPtr);

  if (!is_wakeups_ignored() && num_completed_trans >= minNdbsToWake)
  {
    wakeNdb->theImpl->theWaiter.signal(NO_WAIT);    // wakeup client thread
  }
  return;
}


void MultiNdbWakeupHandler::notifyWakeup()
{
  if (!wakeNdb->theImpl->is_locked_for_poll())
  {
    wakeNdb->theImpl->lock_client();
  }
  assert(wakeNdb->theImpl->wakeHandler == this);

  woken = true;
  /* Wakeup client thread, using 'waiter' Ndb */
  if (!is_wakeups_ignored())
  {
    wakeNdb->theImpl->theWaiter.signal(NO_WAIT);
  }
}


void MultiNdbWakeupHandler::ignore_wakeups()
{
  /**
    We set minNdbsToWake to MAX value to ensure there won't be any
    attempts to wake us up until we're ready to be woken.
  */
  minNdbsToWake = ~Uint32(0);
}

bool MultiNdbWakeupHandler::is_wakeups_ignored()
{
  return (minNdbsToWake == (~Uint32(0)));
}


void MultiNdbWakeupHandler::set_wakeup(Uint32 wakeup_count)
{
  minNdbsToWake = wakeup_count;
}



bool MultiNdbWakeupHandler::isReadyToWake() const
{
  NdbMutex_Lock(localWakeupMutexPtr);
  bool ret = ((numNdbsWithCompletedTrans >= minNdbsToWake) || woken);
  NdbMutex_Unlock(localWakeupMutexPtr);
  return ret;
}
