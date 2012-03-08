/*
   Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.

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


#include <ndb_global.h>

#include "WatchDog.hpp"
#include "GlobalData.hpp"
#include <NdbOut.hpp>
#include <NdbSleep.h>
#include <ErrorHandlingMacros.hpp>
#include <Configuration.hpp>
#include <EventLogger.hpp>

#include <NdbTick.h>

extern EventLogger * g_eventLogger;

extern "C" 
void* 
runWatchDog(void* w){
  ((WatchDog*)w)->run();
  return NULL;
}

WatchDog::WatchDog(Uint32 interval) : 
  m_watchedCount(0)
{
  setCheckInterval(interval);
  m_mutex = NdbMutex_Create();
  theStop = false;
  theThreadPtr = 0;
}

WatchDog::~WatchDog(){
  doStop();
  NdbMutex_Destroy(m_mutex);
}

Uint32
WatchDog::setCheckInterval(Uint32 interval){
  // An interval of less than 70ms is not acceptable
  return theInterval = (interval < 70 ? 70 : interval);
}

bool
WatchDog::registerWatchedThread(Uint32 *counter, Uint32 threadId)
{
  bool ret;

  NdbMutex_Lock(m_mutex);

  if (m_watchedCount >= MAX_WATCHED_THREADS)
  {
    ret = false;
  }
  else
  {
    m_watchedList[m_watchedCount].m_watchCounter = counter;
    m_watchedList[m_watchedCount].m_threadId = threadId;
    NdbTick_getMicroTimer(&(m_watchedList[m_watchedCount].m_startTime));
    m_watchedList[m_watchedCount].m_slowWarnDelay = theInterval;
    m_watchedList[m_watchedCount].m_lastCounterValue = 0;
    ++m_watchedCount;
    ret = true;
  }

  NdbMutex_Unlock(m_mutex);
  return ret;
}

void
WatchDog::unregisterWatchedThread(Uint32 threadId)
{
  Uint32 i;
  NdbMutex_Lock(m_mutex);

  for (i = 0; i < m_watchedCount; i++)
  {
    if (threadId == m_watchedList[i].m_threadId)
      break;
  }
  assert(i < m_watchedCount);
  m_watchedList[i] = m_watchedList[m_watchedCount - 1];
  --m_watchedCount;

  NdbMutex_Unlock(m_mutex);
}

struct NdbThread*
WatchDog::doStart()
{
  theStop = false;
  theThreadPtr = NdbThread_Create(runWatchDog,
				  (void**)this, 
                                  0, // default stack size
				  "ndb_watchdog",
                                  NDB_THREAD_PRIO_HIGH);

  return theThreadPtr;
}

void
WatchDog::doStop(){
  void *status;
  theStop = true;
  if(theThreadPtr){
    NdbThread_WaitFor(theThreadPtr, &status);
    NdbThread_Destroy(&theThreadPtr);
  }
}

const char *get_action(Uint32 IPValue)
{
  const char *action;
  switch (IPValue) {
  case 1:
    action = "Job Handling";
    break;
  case 2:
    action = "Scanning Timers";
    break;
  case 3:
    action = "External I/O";
    break;
  case 4:
    action = "Print Job Buffers at crash";
    break;
  case 5:
    action = "Checking connections";
    break;
  case 6:
    action = "Performing Send";
    break;
  case 7:
    action = "Polling for Receive";
    break;
  case 8:
    action = "Performing Receive";
    break;
  case 9:
    action = "Allocating memory";
    break;
  default:
    action = "Unknown place";
    break;
  }//switch
  return action;
}


#ifdef _WIN32
struct tms {
  clock_t tms_utime;  /* user time */
  clock_t tms_stime;  /* system time */
  clock_t tms_cutime; /* user time of children */
  clock_t tms_cstime; /* system time of children */
};

static clock_t
times(struct tms *buf)
{
  if (!buf)
  {
    errno = EINVAL;
    return -1;
  }

  FILETIME create, exit, kernel, user;
  if (GetProcessTimes(GetCurrentProcess(),
                      &create, &exit, &kernel, &user) == 0)
  {
    errno = GetLastError();
    return -1;
  }

  ULARGE_INTEGER ulint;
  ulint.LowPart = kernel.dwLowDateTime;
  ulint.HighPart = kernel.dwHighDateTime;
  buf->tms_stime = (clock_t)ulint.QuadPart;
  buf->tms_cstime = (clock_t)ulint.QuadPart;

  ulint.LowPart = user.dwLowDateTime;
  ulint.HighPart = user.dwHighDateTime;
  buf->tms_utime = (clock_t)ulint.QuadPart;
  buf->tms_cutime = (clock_t)ulint.QuadPart;

  LARGE_INTEGER ticks;
  if (QueryPerformanceCounter(&ticks) == 0)
  {
    errno = GetLastError();
    return -1;
  }

  return (clock_t)ticks.QuadPart;
}


#else
#include <sys/times.h>
#endif

void 
WatchDog::run()
{
  unsigned int sleep_time;
  struct MicroSecondTimer last_time, now;
  Uint32 numThreads;
  Uint32 counterValue[MAX_WATCHED_THREADS];
  Uint32 oldCounterValue[MAX_WATCHED_THREADS];
  Uint32 threadId[MAX_WATCHED_THREADS];
  struct MicroSecondTimer start_time[MAX_WATCHED_THREADS];
  Uint32 theIntervalCheck[MAX_WATCHED_THREADS];
  Uint32 elapsed[MAX_WATCHED_THREADS];

  NdbTick_getMicroTimer(&last_time);

  // WatchDog for the single threaded NDB
  while (!theStop)
  {
    sleep_time= 100;

    NdbSleep_MilliSleep(sleep_time);
    if(theStop)
      break;

    NdbTick_getMicroTimer(&now);
    if (NdbTick_getMicrosPassed(last_time, now)/1000 > sleep_time*2)
    {
      struct tms my_tms;
      times(&my_tms);
      g_eventLogger->info("Watchdog: User time: %llu  System time: %llu",
                          (Uint64)my_tms.tms_utime,
                          (Uint64)my_tms.tms_stime);
      g_eventLogger->warning("Watchdog: Warning overslept %llu ms, expected %u ms.",
                             NdbTick_getMicrosPassed(last_time, now)/1000,
                             sleep_time);
    }
    last_time = now;

    /*
      Copy out all active counters under locked mutex, then check them
      afterwards without holding the mutex.
    */
    NdbMutex_Lock(m_mutex);
    numThreads = m_watchedCount;
    for (Uint32 i = 0; i < numThreads; i++)
    {
      counterValue[i] = *(m_watchedList[i].m_watchCounter);
      if (counterValue[i] != 0)
      {
        /*
          The thread responded since last check, so just update state until
          next check.

          There is a small race here. If the thread changes the counter
          in-between the read and setting to zero here in the watchdog
          thread, then gets stuck immediately after, we may report the
          wrong action that it got stuck on.
          But there will be no reporting of non-stuck thread because of
          this race, nor will there be missed reporting.
        */
        *(m_watchedList[i].m_watchCounter) = 0;
        m_watchedList[i].m_startTime = now;
        m_watchedList[i].m_slowWarnDelay = theInterval;
        m_watchedList[i].m_lastCounterValue = counterValue[i];
      }
      else
      {
        start_time[i] = m_watchedList[i].m_startTime;
        threadId[i] = m_watchedList[i].m_threadId;
        oldCounterValue[i] = m_watchedList[i].m_lastCounterValue;
        theIntervalCheck[i] = m_watchedList[i].m_slowWarnDelay;
        elapsed[i] = (Uint32)NdbTick_getMicrosPassed(start_time[i], now)/1000;
        if (oldCounterValue[i] == 9 && elapsed[i] >= theIntervalCheck[i])
          m_watchedList[i].m_slowWarnDelay += theInterval;
      }
    }
    NdbMutex_Unlock(m_mutex);

    /*
      Now check each watched thread if it has reported progress since previous
      check. Warn about any stuck threads, and eventually force shutdown the
      server.
    */
    for (Uint32 i = 0; i < numThreads; i++)
    {
      if (counterValue[i] != 0)
        continue;

      /*
        Counter value == 9 indicates malloc going on, this can take some time
        so only warn if we pass the watchdog interval
      */
      if (oldCounterValue[i] != 9 || elapsed[i] >= theIntervalCheck[i])
      {
        const char *last_stuck_action = get_action(oldCounterValue[i]);
        g_eventLogger->warning("Ndb kernel thread %u is stuck in: %s "
                              "elapsed=%u",
                              threadId[i], last_stuck_action, elapsed[i]);
        {
          struct tms my_tms;
          times(&my_tms);
          g_eventLogger->info("Watchdog: User time: %llu  System time: %llu",
                              (Uint64)my_tms.tms_utime,
                              (Uint64)my_tms.tms_stime);
        }
        if (elapsed[i] > 3 * theInterval)
        {
          shutdownSystem(last_stuck_action);
        }
      }
    }
  }
  return;
}

void
WatchDog::shutdownSystem(const char *last_stuck_action){
  
  ErrorReporter::handleError(NDBD_EXIT_WATCHDOG_TERMINATE,
			     last_stuck_action,
			     __FILE__,
			     NST_Watchdog);
}
