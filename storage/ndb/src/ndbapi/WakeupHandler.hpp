/*
   Copyright (c) 2011, 2013, Oracle and/or its affiliates. All rights reserved.

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

#ifndef WakeupHandler_H
#define WakeupHandler_H

#include <ndb_types.h>
#include <NdbMutex.h>
class Ndb;
class Ndb_cluster_connection;
class PollGuard;

/**
 * WakeupHandler
 *
 * Help Ndb objects respond to wakeups from the TransporterFacade
 * when transactions have completed.
 *
 * Each Ndb will own an instance of the DefaultWakeupHandler,
 * and each NdbWaitGroup will create an instance of a more specialized
 * WakeupHandler.
 */

class WakeupHandler
{
public:
  virtual void notifyTransactionCompleted(Ndb* from) = 0;
  virtual void notifyWakeup() = 0;
  virtual ~WakeupHandler() {};
};

class MultiNdbWakeupHandler : public WakeupHandler
{
public:
  MultiNdbWakeupHandler(Ndb* _wakeNdb);
  ~MultiNdbWakeupHandler();
  void notifyTransactionCompleted(Ndb* from);
  void notifyWakeup();
  /** returns 0 on success, -1 on timeout: */
  int waitForInput(Ndb **objs,
                   int cnt,
                   int min_requested,
                   int timeout_millis,
                   int *nready);

private:   // private methods
  void ignore_wakeups();
  bool is_wakeups_ignored();
  void set_wakeup(Uint32 wakeup_count);
  void finalize_wait(int *nready);
  void registerNdb(Ndb *, Uint32);
  void unregisterNdb(Ndb *);
  void swapNdbsInArray(Uint32 indexA, Uint32 indexB);
  bool isReadyToWake() const;

private:   // private instance variables
  Uint32 numNdbsWithCompletedTrans;
  Uint32 minNdbsToWake;
  Ndb* wakeNdb;
  Ndb** objs;
  Uint32 cnt;
  NdbMutex* localWakeupMutexPtr;
  volatile bool woken;
};
#endif
