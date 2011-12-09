/*
   Copyright (C) 2003-2006 MySQL AB, 2009 Sun Microsystems, Inc.
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

    MutexVector<CPCD::Process *> &proc = *m_cpcd->getProcessList();

    proc.lock();

    for(size_t i = 0; i < proc.size(); i++) {
      proc[i]->monitor();
    }

    proc.unlock();

    NdbMutex_Unlock(m_changeMutex);
  }
}

void
CPCD::Monitor::signal() {
  NdbCondition_Signal(m_changeCondition);
}

template class MutexVector<CPCD::Process*>;
