/*
   Copyright (c) 2016, 2023, Oracle and/or its affiliates.

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

#include "ndb_global.h"
#include "ProcessInfo.hpp"
#include "OwnProcessInfo.hpp"
#include <NdbMutex.h>
#include <ndb_socket.h>

const char * ndb_basename(const char *path);

extern const char * my_progname;

/* Static storage; constructor called at process startup by C++ runtime. */
ProcessInfo singletonInfo;
NdbLockable theApiMutex;

/* Public API
 *
 */
void setOwnProcessInfoAngelPid(Uint32 pid)
{
  theApiMutex.lock();
  singletonInfo.setAngelPid(pid);
  theApiMutex.unlock();
}

void setOwnProcessInfoServerAddress(struct sockaddr * addr)
{
  theApiMutex.lock();
  sockaddr_in6 *addr_in6 = (sockaddr_in6 *)addr;
  singletonInfo.setHostAddress(&addr_in6->sin6_addr);
  theApiMutex.unlock();
}

void setOwnProcessInfoPort(Uint16 port)
{
  theApiMutex.lock();
  singletonInfo.setPort(port);
  theApiMutex.unlock();
}


/* Fill in missing parts of ProcessInfo before providing it to QMgr 
   or ClusterMgr
*/

#ifdef WIN32
#include "psapi.h"

void getNameFromEnvironment()
{
  HANDLE handle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                              false, singletonInfo.getPid());
  if (!handle)
    return;

  GetModuleFileNameEx(handle, 0, singletonInfo.process_name,
                      singletonInfo.ProcessNameLength);
  CloseHandle(handle);
}
#else
void getNameFromEnvironment()
{
  const char * path = getenv("_");
  if(path)
  {
    singletonInfo.setProcessName(ndb_basename(path));
  }
}
#endif


/* Return angel pid, or zero if no angel.
   On unix, if we are not a daemon, and also not a process group leader,
   set parent pid as angel pid.
   On Windows, return MYSQLD_PARENT_PID if set in the environment.
*/
static Uint32 getParentPidAsAngel()
{
#ifdef WIN32
  const char * monitor_pid = getenv("MYSQLD_PARENT_PID");
  if(monitor_pid)
  {
    return atoi(monitor_pid);
  }
#else
  pid_t parent_process_id = getppid();
  if((parent_process_id != 1)  && (getpgrp() != singletonInfo.getPid()))
  {
    return parent_process_id;
  }
#endif
  return 0;
}


/* Public API for QMgr and ClusterMgr.
*/
ProcessInfo * getOwnProcessInfo(Uint16 nodeId) {
  Guard locked(theApiMutex);
  if(singletonInfo.process_id == 0)
  {
    /* Finalize */
    singletonInfo.setPid();
    singletonInfo.node_id = nodeId;
    if(singletonInfo.angel_process_id == 0)
      singletonInfo.angel_process_id = getParentPidAsAngel();
    if(singletonInfo.process_name[0] == 0)
    {
      if(my_progname)
        singletonInfo.setProcessName(ndb_basename(my_progname));
      else
        getNameFromEnvironment();
    }
  }

  return & singletonInfo;
}
