/*
   Copyright (c) 2004, 2017, Oracle and/or its affiliates. All rights reserved.

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
#include "my_sys.h"
#include <NdbMutex.h>
#include <NdbLockCpuUtil.h>

class EventLogger *g_eventLogger = NULL;

NdbMutex *g_ndb_connection_mutex = NULL;

extern class EventLogger * create_event_logger();
extern void destroy_event_logger(class EventLogger ** g_eventLogger);

static int ndb_init_called = 0;

extern "C" void NdbMutex_SysInit();
extern "C" void NdbMutex_SysEnd();
extern "C" void NdbCondition_initialize();
extern "C" int NdbThread_Init();
extern "C" void NdbThread_End();
extern "C" void NdbGetRUsage_Init();
extern "C" void NdbGetRUsage_End();
extern "C" int NdbLockCpu_Init();
extern "C" void NdbLockCpu_End();

extern void NdbTick_Init();
extern void NdbOut_Init();

extern "C"
{

#define NORMAL_USER 0
#define MYSQLD_USER 1
#define THREAD_REGISTER_USER 2
void
ndb_init_internal(Uint32 caller)
{
  bool init_all = true;
  if (caller != NORMAL_USER)
  {
    /**
     * This is called from MySQL Server, normally called
     * from ndbcluster_init, but can also be called from
     * thread_register plugin. In this case we can have
     * two calls, they should not run concurrently, at
     * startup all init calls comes from the init thread.
     * At close down from end thread. If the thread
     * register is dynamically loaded then the ndbcluster_init
     * will have been called before. If no NDB storage engine
     * is called then the thread register plugin might
     * initialise and end multiple times.
     */
    Uint32 init_called = ndb_init_called;
    ndb_init_called++;
    if (init_called > 0)
    {
      if (caller == THREAD_REGISTER_USER)
      {
        return;
      }
      init_all = false;
    }
  }
  if (caller != THREAD_REGISTER_USER)
    NdbOut_Init();
  if (init_all)
    NdbMutex_SysInit();
  if (caller != THREAD_REGISTER_USER)
  {
    if (!g_ndb_connection_mutex)
      g_ndb_connection_mutex = NdbMutex_Create();
    if (!g_eventLogger)
      g_eventLogger = create_event_logger();
    if ((g_ndb_connection_mutex == NULL) || (g_eventLogger == NULL))
    {
      {
        const char* err = "ndb_init() failed - exit\n";
        int res = (int)write(2, err, (unsigned)strlen(err));
        (void)res;
        exit(1);
      }
    }
    NdbTick_Init();
    NdbCondition_initialize();
    NdbGetRUsage_Init();
  }
  if (init_all)
  {
    NdbThread_Init();
    if (NdbLockCpu_Init() != 0)
    {
      const char* err = "ndbLockCpu_Init() failed - exit\n";
      int res = (int)write(2, err, (unsigned)strlen(err));
      (void)res;
      exit(1);
    }
  }
}

int
ndb_init()
{
  if (ndb_init_called == 0)
  {
    ndb_init_called = 1;
    if (my_init())
    {
      const char* err = "my_init() failed - exit\n";
      int res = (int)write(2, err, (unsigned)strlen(err));
      (void)res;
      exit(1);
    }
    /*
      Initialize time conversion information
       - the "time conversion information" structures are primarily
         used by localtime_r() when converting epoch time into
         broken-down local time representation.
    */
    tzset();

    ndb_init_internal(0);
  }
  return 0;
}

void
ndb_end_internal(Uint32 caller)
{
  bool end_all = true;
  if (caller != NORMAL_USER)
  {
    ndb_init_called--;
    if (ndb_init_called > 0)
    {
      if (caller == THREAD_REGISTER_USER)
      {
        return;
      }
      end_all = false;
    }
  }
  if (caller != THREAD_REGISTER_USER)
  {
    if (g_ndb_connection_mutex) 
    {
      NdbMutex_Destroy(g_ndb_connection_mutex);
      g_ndb_connection_mutex=NULL;
    }
    if (g_eventLogger)
      destroy_event_logger(&g_eventLogger);
    NdbGetRUsage_End();
  }
  if (end_all)
  {
    NdbLockCpu_End();
    NdbThread_End();
    NdbMutex_SysEnd();
  }
}

void
ndb_end(int flags)
{
  if (ndb_init_called == 1)
  {
    my_end(flags);
    ndb_end_internal(0);
    ndb_init_called = 0;
  }
}
} /* extern "C" */
