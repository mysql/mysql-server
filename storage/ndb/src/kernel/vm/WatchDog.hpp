/*
   Copyright (C) 2003, 2005, 2006, 2008 MySQL AB, 2008 Sun Microsystems, Inc.
    All rights reserved. Use is subject to license terms.

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

#ifndef WatchDog_H
#define WatchDog_H

#include <kernel_types.h>
#include <NdbThread.h>
#include <NdbMutex.h>
#include <NdbTick.h>

extern "C" void* runWatchDog(void* w);

class WatchDog{
  enum { MAX_WATCHED_THREADS = 64 };

  struct WatchedThread {
    Uint32 *m_watchCounter;
    Uint32 m_threadId;
    /* This is the time that activity was last registered from thread. */
    MicroSecondTimer m_startTime;
    /*
      During slow operation (memory allocation), warnings are output less
      frequently, and this is the point when the next warning should be given.
     */
    Uint32 m_slowWarnDelay;
    /*
      This is the last counter value update seen, telling us what the thread
      was doing when it got stuck.
    */
    Uint32 m_lastCounterValue;
  };

public:
  WatchDog(Uint32 interval = 3000);
  ~WatchDog();
 
  struct NdbThread* doStart();
  void doStop();

  Uint32 setCheckInterval(Uint32 interval);

  /*
    Register a thread for being watched.
    Returns true if ok, false if out of slots.
  */
  bool registerWatchedThread(Uint32 *counter, Uint32 threadId);
  /* Remove a thread from registration, identified by thread id. */
  void unregisterWatchedThread(Uint32 threadId);

protected:
  /**
   * Thread function
   */
  friend void* runWatchDog(void* w);
  
  /**
   * Thread pointer 
   */
  NdbThread* theThreadPtr;
  
private:
  Uint32 theInterval;
  /*
    List of watched threads.
    Threads are identified by the m_threadId.
    Active entries are kept at the start of the entries.
    Access to the list is protected by m_mutex.
  */
  WatchedThread m_watchedList[MAX_WATCHED_THREADS];
  /* Number of active entries in m_watchedList. */
  Uint32 m_watchedCount;
  NdbMutex *m_mutex;

  bool theStop;
  
  void run();
  void shutdownSystem(const char *last_stuck_action);
};

#endif // WatchDog_H
