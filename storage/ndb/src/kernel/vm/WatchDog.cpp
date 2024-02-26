/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.

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


#include <ndb_global.h>

#include <time.h>

#include "portlib/mt-asm.h"
#include "WatchDog.hpp"
#include "GlobalData.hpp"
#include <NdbOut.hpp>
#include <NdbSleep.h>
#include <ErrorHandlingMacros.hpp>
#include <Configuration.hpp>
#include <EventLogger.hpp>

#include <NdbTick.h>


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
  killer = false;
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
    m_watchedList[m_watchedCount].m_startTicks = NdbTick_getCurrentTicks();
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

void
WatchDog::setKillSwitch(bool kill)
{
  g_eventLogger->info("Watchdog KillSwitch %s.",
                      (kill?"on":"off"));
  killer = kill;
}

static
const char *get_action(char *buf, Uint32 IPValue)
{
  const char *action;
  Uint32 place = IPValue & 255;
  switch (place) {
  case 1:
  {
    Uint32 bno = (IPValue >> 8) & 1023;
    Uint32 gsn = IPValue >> 20;
    BaseString::snprintf(buf,
                         128,
                         "JobHandling in block: %u, gsn: %u",
                         bno,
                         gsn);
    action = buf;
    break;
  }
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
  case 11:
    action = "Packing Send Buffers";
    break;
  case 12:
    action = "Looking for next job to execute";
    break;
  case 13:
    action = "Looking for next non-empty job buffer";
    break;
  case 14:
    action = "Scanning zero time queue";
    break;
  case 15:
    action = "Send packed signals";
    break;
  case 16:
    action = "Update scheduler configuration";
    break;
  case 17:
    action = "Check for input from NDBFS";
    break;
  case 18:
    action = "Yielding to OS";
    break;
  case 19:
    action = "Send thread main loop";
    break;
  case 20:
    action = "Returned from do_send";
    break;
  case 21:
    action = "Initial value in mt_job_thread_main";
    break;
  default:
    action = NULL;
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

#define JAM_FILE_ID 235

static void dump_memory_info();

void 
WatchDog::run()
{
  unsigned int sleep_time;
  NDB_TICKS last_ticks, now;
  Uint32 numThreads;
  Uint32 counterValue[MAX_WATCHED_THREADS];
  Uint32 oldCounterValue[MAX_WATCHED_THREADS];
  Uint32 threadId[MAX_WATCHED_THREADS];
  NDB_TICKS start_ticks[MAX_WATCHED_THREADS];
  Uint32 theIntervalCheck[MAX_WATCHED_THREADS];
  Uint32 elapsed[MAX_WATCHED_THREADS];

  if (!NdbTick_IsMonotonic())
  {
    g_eventLogger->warning("A monotonic timer was not available on this platform.");
    g_eventLogger->warning("Adjusting system time manually, or otherwise (e.g. NTP), "
              "may cause false watchdog alarms, temporary freeze, or node shutdown.");
  }

  last_ticks = NdbTick_getCurrentTicks();

  while (!theStop)
  {
    sleep_time= 100;

    NdbSleep_MilliSleep(sleep_time);
    if(theStop)
      break;

    now = NdbTick_getCurrentTicks();

    if (NdbTick_Compare(now, last_ticks) < 0)
    {
      g_eventLogger->warning("Watchdog: Time ticked backwards %llu ms.",
                             NdbTick_Elapsed(now, last_ticks).milliSec());
      /**
       * A backtick after sleeping 100ms, is considered a
       * fatal error if monotonic timers are used.
       */
      assert(!NdbTick_IsMonotonic());
    }
    // Print warnings if sleeping much longer than expected
    else if (NdbTick_Elapsed(last_ticks, now).milliSec() > sleep_time*2)
    {
      struct tms my_tms;
      if (times(&my_tms) != (clock_t)-1)
      {
        g_eventLogger->info("Watchdog: User time: %llu  System time: %llu",
                          (Uint64)my_tms.tms_utime,
                          (Uint64)my_tms.tms_stime);
      }
      else
      {
        g_eventLogger->info("Watchdog: User time: %llu System time: %llu (errno=%d)",
                          (Uint64)my_tms.tms_utime,
                          (Uint64)my_tms.tms_stime,
                          errno);
      }
      g_eventLogger->warning("Watchdog: Warning overslept %llu ms, expected %u ms.",
                             NdbTick_Elapsed(last_ticks, now).milliSec(),
                             sleep_time);
    }
    last_ticks = now;

    /*
      Copy out all active counters under locked mutex, then check them
      afterwards without holding the mutex.
    */
    NdbMutex_Lock(m_mutex);
    numThreads = m_watchedCount;
    for (Uint32 i = 0; i < numThreads; i++)
    {
#ifdef NDB_HAVE_XCNG
      /* atomically read and clear watchdog counter */
      counterValue[i] = xcng(m_watchedList[i].m_watchCounter, 0);
#else
      counterValue[i] = *(m_watchedList[i].m_watchCounter);
#endif
      if (likely(counterValue[i] != 0))
      {
        /*
          The thread responded since last check, so just update state until
          next check.
         */
#ifndef NDB_HAVE_XCNG
        /*
          There is a small race here. If the thread changes the counter
          in-between the read and setting to zero here in the watchdog
          thread, then gets stuck immediately after, we may report the
          wrong action that it got stuck on.
          But there will be no reporting of non-stuck thread because of
          this race, nor will there be missed reporting.
        */
        *(m_watchedList[i].m_watchCounter) = 0;
#endif
        m_watchedList[i].m_startTicks = now;
        m_watchedList[i].m_slowWarnDelay = theInterval;
        m_watchedList[i].m_lastCounterValue = counterValue[i];
      }
      else
      {
        start_ticks[i] = m_watchedList[i].m_startTicks;
        threadId[i] = m_watchedList[i].m_threadId;
        oldCounterValue[i] = m_watchedList[i].m_lastCounterValue;
        theIntervalCheck[i] = m_watchedList[i].m_slowWarnDelay;
        elapsed[i] = (Uint32)NdbTick_Elapsed(start_ticks[i], now).milliSec();
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
        char buf[128];
        const char *last_stuck_action = get_action(buf, oldCounterValue[i]);
        if (last_stuck_action != NULL)
        {
          g_eventLogger->warning("Ndb kernel thread %u is stuck in: %s "
                                 "elapsed=%u",
                                 threadId[i], last_stuck_action, elapsed[i]);
        }
        else
        {
          g_eventLogger->warning("Ndb kernel thread %u is stuck in: Unknown place %u "
                                 "elapsed=%u",
                                 threadId[i],  oldCounterValue[i], elapsed[i]);
        }
        {
          struct tms my_tms;
          if (times(&my_tms) != (clock_t)-1)
          {
            g_eventLogger->info("Watchdog: User time: %llu  System time: %llu",
                              (Uint64)my_tms.tms_utime,
                              (Uint64)my_tms.tms_stime);
          }
          else
          {
            g_eventLogger->info("Watchdog: User time: %llu System time: %llu (errno=%d)",
                              (Uint64)my_tms.tms_utime,
                              (Uint64)my_tms.tms_stime,
                              errno);
          }
        }
        if ((elapsed[i] > 3 * theInterval) || killer)
        {
          if (oldCounterValue[i] == 9)
          {
            dump_memory_info();
          }
          shutdownSystem(last_stuck_action);
        }
      }
    }
  }
  return;
}

#if defined(VM_TRACE) || defined(ERROR_INSERT)

static int dump_file(const char filename[]);

int dump_file(const char filename[])
{
  FILE* f = fopen(filename, "r");
  if (f != nullptr)
  {
    g_eventLogger->info("Watchdog: dump %s\n", filename);
    char buf[256];
    while (fgets(buf, sizeof(buf), f) != nullptr)
    {
      g_eventLogger->info("%s\n", buf);
    }
    fclose(f);
  }
  return f == nullptr ? -1 : 0;
}
#endif

void dump_memory_info()
{
#if defined(VM_TRACE) || defined(ERROR_INSERT)
  dump_file("/proc/meminfo");

  dump_file("/proc/self/numa_maps");

  char filename[] = "/sys/devices/system/node/node0/meminfo";
  char* node_number = strchr(filename, '0');
  for (int node = 0; node < 10; node++)
  {
    *node_number = '0' + node;
    if (dump_file(filename) == -1) break;
  }
#endif
}

void
WatchDog::shutdownSystem(const char *last_stuck_action){
  
  ErrorReporter::handleError(NDBD_EXIT_WATCHDOG_TERMINATE,
			     last_stuck_action,
			     __FILE__,
			     NST_Watchdog);
}
