/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

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

#include <NdbThread.h>
#include <NdbOut.hpp>
#include <NdbSleep.h>

#include "CPCD.hpp"
#include "common.hpp"

static void *
monitor_thread_create_wrapper(void * arg) {
  CPCD::Monitor *mon = (CPCD::Monitor *)arg;
  mon->run();
  return NULL;
}

CPCD::Monitor::Monitor(CPCD *cpcd, int poll) {
  m_cpcd = cpcd;
  m_pollingInterval = poll;
  m_changeCondition = NdbCondition_Create();
  m_changeMutex = NdbMutex_Create();
  m_monitorThread = NdbThread_Create(monitor_thread_create_wrapper,
				     (NDB_THREAD_ARG*) this,
                                     0, // default stack size
				     "ndb_cpcd_monitor",
				     NDB_THREAD_PRIO_MEAN);
  m_monitorThreadQuitFlag = false;
}

CPCD::Monitor::~Monitor() {
  NdbThread_Destroy(&m_monitorThread);
  NdbCondition_Destroy(m_changeCondition);
  NdbMutex_Destroy(m_changeMutex);
}

void
CPCD::Monitor::run() {
  while(1) {
    NdbMutex_Lock(m_changeMutex);
    NdbCondition_WaitTimeout(m_changeCondition,
			     m_changeMutex,
			     m_pollingInterval * 1000);

    MutexVector<CPCD::Process *> &processes = *m_cpcd->getProcessList();

    processes.lock();

    for (unsigned i = 0; i < processes.size(); i++)
    {
      processes[i]->monitor();
    }

    // Erase in reverse order to let i always step down
    for (unsigned i = processes.size(); i > 0; i--)
    {
      CPCD::Process *proc = processes[i - 1];
      if (!proc->should_be_erased())
      {
        continue;
      }

      processes.erase(i - 1, false /* already locked */);
      delete proc;
    }

    processes.unlock();

    NdbMutex_Unlock(m_changeMutex);
  }
}

void
CPCD::Monitor::signal() {
  NdbCondition_Signal(m_changeCondition);
}

template class MutexVector<CPCD::Process*>;
