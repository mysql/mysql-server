/*
   Copyright (c) 2004, 2010, Oracle and/or its affiliates. All rights reserved.

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
#include <my_sys.h>
#include <NdbMutex.h>

class EventLogger *g_eventLogger = NULL;

NdbMutex *g_ndb_connection_mutex = NULL;

extern class EventLogger * create_event_logger();
extern void destroy_event_logger(class EventLogger ** g_eventLogger);

// Turn on monotonic timers and conditions by setting
// this flag before calling ndb_init 
int g_ndb_init_need_monotonic = 0;

static int ndb_init_called = 0;

extern "C" void NdbMutex_SysInit();
extern "C" void NdbMutex_SysEnd();
extern "C" void NdbCondition_initialize(int need_monotonic);
extern "C" void NdbTick_Init(int need_monotonic);
extern "C" int NdbThread_Init();
extern "C" void NdbThread_End();
extern void NdbOut_Init();

extern "C"
{

void
ndb_init_internal()
{
  NdbOut_Init();
  NdbMutex_SysInit();
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
  /* Always turn on monotonic unless on Solaris */  
#ifndef __sun
  g_ndb_init_need_monotonic = 1;
#endif
  NdbTick_Init(g_ndb_init_need_monotonic);
  NdbCondition_initialize(g_ndb_init_need_monotonic);
  NdbThread_Init();
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
    ndb_init_internal();
  }
  return 0;
}

void
ndb_end_internal()
{
  if (g_ndb_connection_mutex) 
  {
    NdbMutex_Destroy(g_ndb_connection_mutex);
    g_ndb_connection_mutex=NULL;
  }
  if (g_eventLogger)
    destroy_event_logger(&g_eventLogger);

  NdbThread_End();
  NdbMutex_SysEnd();
}

void
ndb_end(int flags)
{
  if (ndb_init_called == 1)
  {
    my_end(flags);
    ndb_end_internal();
    ndb_init_called = 0;
  }
}

} /* extern "C" */
