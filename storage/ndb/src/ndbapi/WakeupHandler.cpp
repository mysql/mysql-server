/*
   Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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
 */

MultiNdbWakeupHandler::MultiNdbWakeupHandler(Ndb* _wakeNdb)
  : wakeNdb(_wakeNdb),
    woken(false)
{
  /* Register the waiter Ndb to receive wakeups for all Ndbs in the group */
  PollGuard pg(* wakeNdb->theImpl);   // Hold mutex before calling into Facade
  bool rc = wakeNdb->theImpl->m_transporter_facade->registerForWakeup(wakeNdb->theImpl);
  assert(rc);
  wakeNdb->theImpl->wakeHandler = this;
}


MultiNdbWakeupHandler::~MultiNdbWakeupHandler()
{
  PollGuard pg(* wakeNdb->theImpl); // Hold mutex before calling into Facade
  bool rc = wakeNdb->theImpl->m_transporter_facade->
    unregisterForWakeup(wakeNdb->theImpl);
  assert(rc);
}


bool MultiNdbWakeupHandler::ndbIsRegistered(Ndb *obj)
{
  return (obj->theImpl->wakeHandler == this);
}


bool MultiNdbWakeupHandler::unregisterNdb(Ndb *obj)
{
  if (obj->theImpl->wakeHandler == this)
  {
    obj->theImpl->wakeHandler = 0;
    obj->theImpl->wakeContext = ~ Uint32(0);
    return true;
  }
  return false;
}


Uint32 MultiNdbWakeupHandler::getNumReadyNdbs() const
{
  return numNdbsWithCompletedTrans;
}


int MultiNdbWakeupHandler::waitForInput(Ndb** _objs, int _cnt, int min_req,
                                        PollGuard* pg, int timeout_millis)
{
  woken = false;
  numNdbsWithCompletedTrans = 0;
  minNdbsToWake = min_req;
  objs = _objs;
  cnt = _cnt;

  /* Before sleeping, we register each Ndb, and check whether it already
     has any completed transactions.
  */
  for (Uint32 ndbcnt = 0; ndbcnt < cnt; ndbcnt ++)
  {
    Ndb* obj = objs [ndbcnt];

    /* Register the Ndb */
    obj->theImpl->wakeHandler = this;

    /* Store its list position */
    obj->theImpl->wakeContext = ndbcnt;

    /* It may already have some completed transactions */
    if (obj->theNoOfCompletedTransactions)
    {
      /* Move that ndb to the start of the array */
      swapNdbsInArray(ndbcnt, numNdbsWithCompletedTrans);
      numNdbsWithCompletedTrans++;
    }
  }

  if (isReadyToWake())  // already enough
  {
    woken = false;
    return 0;
  }

  wakeNdb->theImpl->theWaiter.set_node(0);
  wakeNdb->theImpl->theWaiter.set_state(WAIT_TRANS);

  NDB_TICKS currTime = NdbTick_CurrentMillisecond();
  NDB_TICKS maxTime = currTime + (NDB_TICKS) timeout_millis;

  do {
    /* PollGuard will put us to sleep until something relevant happens */
    pg->wait_for_input(timeout_millis > 10 ? 10 : timeout_millis);
    wakeNdb->theImpl->incClientStat(Ndb::WaitExecCompleteCount, 1);

    if (isReadyToWake())
    {
      woken = false;  // reset for next time
      return 0;
    }
    timeout_millis = (int) (maxTime - NdbTick_CurrentMillisecond());
  } while (timeout_millis > 0);

  return -1;  // timeout occured
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

  Ndb* a = objs[ indexA ];
  Ndb* b = objs[ indexB ];

  assert(a->theImpl->wakeContext == indexA);
  assert(b->theImpl->wakeContext == indexB);

  objs[ indexA ] = b;
  b->theImpl->wakeContext = indexA;

  objs[ indexB ] = a;
  a->theImpl->wakeContext = indexB;
}


void MultiNdbWakeupHandler::notifyTransactionCompleted(Ndb* from)
{
  Uint32 & completedNdbListPos = from->theImpl->wakeContext;

  /* TODO : assert that transporter lock is held */
  assert(completedNdbListPos < cnt);
  assert(wakeNdb->theImpl->wakeHandler == this);
  assert(from != wakeNdb);

  /* Some Ndb object has just completed another transaction.
     Ensure that it's in the completed Ndbs list
  */
  if (completedNdbListPos >= numNdbsWithCompletedTrans)
  {
    /* It's not, swap it with Ndb in 'next' position */
    swapNdbsInArray(completedNdbListPos, numNdbsWithCompletedTrans);
    numNdbsWithCompletedTrans ++;
  }

  if (numNdbsWithCompletedTrans >= minNdbsToWake)
  {
    wakeNdb->theImpl->theWaiter.signal(NO_WAIT);    // wakeup client thread
  }

  return;
}


void MultiNdbWakeupHandler::notifyWakeup()
{
  assert(wakeNdb->theImpl->wakeHandler == this);

  /* Wakeup client thread, using 'waiter' Ndb */
  woken = true;
  wakeNdb->theImpl->theWaiter.signal(NO_WAIT);
}
