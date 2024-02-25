/*
   Copyright (c) 2011, 2023, Oracle and/or its affiliates.

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

#include "mt_thr_config.hpp"
#include "mgmcommon/thr_config.hpp"
#include <kernel/ndb_limits.h>
#include "../../common/util/parse_mask.hpp"
#include <NdbLockCpuUtil.h>
#include <NdbThread.h>
#include <NdbHW.hpp>
#include <EventLogger.hpp>
#include <BlockNumbers.h>


static
int
findBlock(Uint32 blockNo, const unsigned short list[], unsigned cnt)
{
  for (Uint32 i = 0; i < cnt; i++)
  {
    if (blockToMain(list[i]) == blockNo)
      return blockToInstance(list[i]);
  }
  return -1;
}

const THRConfig::T_Thread*
THRConfigApplier::find_thread(const unsigned short instancelist[], unsigned cnt) const
{
  int instanceNo;
  if ((instanceNo = findBlock(SUMA, instancelist, cnt)) >= 0)
  {
    Uint32 num_main_threads = getThreadCount(T_REP) +
                              getThreadCount(T_MAIN);
    if (num_main_threads == 2)
      return &m_threads[T_REP][instanceNo];
    else if (num_main_threads == 1)
      return &m_threads[T_MAIN][instanceNo];
    else if (num_main_threads == 0)
      return &m_threads[T_RECV][instanceNo];
    else
      abort();
  }
  else if ((instanceNo = findBlock(DBDIH, instancelist, cnt)) >= 0)
  {
    return &m_threads[T_MAIN][instanceNo];
  }
  else if ((instanceNo = findBlock(DBLQH, instancelist, cnt)) >= 0)
  {
    return &m_threads[T_LDM][instanceNo - 1]; // remove proxy...
  }
  else if ((instanceNo = findBlock(DBQLQH, instancelist, cnt)) >= 0)
  {
    int num_query_threads = (int)getThreadCount(T_QUERY);
    if ((instanceNo - 1) < num_query_threads)
    {
      return &m_threads[T_QUERY][instanceNo - 1]; // remove proxy...
    }
    else
    {
      instanceNo -= num_query_threads;
      return &m_threads[T_RECOVER][instanceNo - 1]; // remove proxy...
    }
  }
  else if ((instanceNo = findBlock(TRPMAN, instancelist, cnt)) >= 0)
  {
    return &m_threads[T_RECV][instanceNo - 1]; // remove proxy
  }
  else if ((instanceNo = findBlock(DBTC, instancelist, cnt)) >= 0)
  {
    return &m_threads[T_TC][instanceNo - 1]; // remove proxy
  }
  return 0;
}

void
THRConfigApplier::appendInfo(BaseString& str,
                             const unsigned short list[], unsigned cnt) const
{
  const T_Thread* thr = find_thread(list, cnt);
  appendInfo(str, thr);
}

void
THRConfigApplier::appendInfoSendThread(BaseString& str,
                                       unsigned instance_no) const
{
  const T_Thread* thr = &m_threads[T_SEND][instance_no];
  appendInfo(str, thr);
}

void
THRConfigApplier::appendInfo(BaseString& str,
                             const T_Thread* thr) const
{
  assert(thr != 0);
  str.appfmt("(%s) ", getEntryName(thr->m_type));
  if (thr->m_bind_type == T_Thread::B_CPU_BIND)
  {
    str.appfmt("cpubind: %u ", thr->m_bind_no);
  }
  else if (thr->m_bind_type == T_Thread::B_CPU_BIND_EXCLUSIVE)
  {
    str.appfmt("cpubind_exclusive: %u ", thr->m_bind_no);
  }
  else if (thr->m_bind_type == T_Thread::B_CPUSET_BIND)
  {
    str.appfmt("cpuset: [ %s ] ", m_cpu_sets[thr->m_bind_no].str().c_str());
  }
  else if (thr->m_bind_type == T_Thread::B_CPUSET_EXCLUSIVE_BIND)
  {
    str.appfmt("cpuset_exclusive: [ %s ] ",
      m_cpu_sets[thr->m_bind_no].str().c_str());
  }
}

const char *
THRConfigApplier::getName(const unsigned short list[], unsigned cnt) const
{
  const T_Thread* thr = find_thread(list, cnt);
  assert(thr != 0);
  return getEntryName(thr->m_type);
}

int
THRConfigApplier::do_bind(NdbThread* thread,
                          const unsigned short list[], unsigned cnt)
{
  const T_Thread* thr = find_thread(list, cnt);
  return do_bind(thread, thr);
}

int
THRConfigApplier::do_bind_idxbuild(NdbThread* thread)
{
  /* TODO : Assert IDX_BLD thread exists */
  assert(m_threads[T_IXBLD].size() > 0);
  const T_Thread* thr = &m_threads[T_IXBLD][0];
  
  return(do_bind(thread, thr));
}

int
THRConfigApplier::do_bind_io(NdbThread* thread)
{
  const T_Thread* thr = &m_threads[T_IO][0];
  return do_bind(thread, thr);
}

int
THRConfigApplier::do_bind_watchdog(NdbThread* thread)
{
  const T_Thread* thr = &m_threads[T_WD][0];
  return do_bind(thread, thr);
}

int
THRConfigApplier::do_unbind(NdbThread *thread)
{
  return Ndb_UnlockCPU(thread);
}

THRConfigRebinder::THRConfigRebinder(THRConfigApplier* tca,
                                     THRConfig::T_Type type [[maybe_unused]],
                                     NdbThread* thread)
    : m_config_applier(tca), m_state(0), m_thread(thread)
{
  /* Only for T_IXBLD currently */
  assert(type == THRConfig::T_IXBLD);
  assert(!THRConfig::isThreadPermanent(type));

  int rc = m_config_applier->do_unbind(m_thread);
  if (rc < 0)
  {
    g_eventLogger->info("THRConfigRebinder(%p) unbind failed: %u", m_thread,
                        rc);
    return;
  }
  /* Unbound */
  m_state = 1;

  rc = m_config_applier->do_bind_idxbuild(m_thread);
  if (rc < 0)
  {
    g_eventLogger->info("THRConfigRebinder(%p) bind failed : %u", m_thread, rc);
    return;
  }
  /* Bound */
  m_state = 2;

  return;
}

THRConfigRebinder::~THRConfigRebinder()
{
  switch(m_state)
  {
  case 2:    /* Bound */
  {
    int rc = m_config_applier->do_unbind(m_thread);
    if (rc < 0)
    {
      g_eventLogger->info("~THRConfigRebinder(%p) unbind failed: %u", m_thread,
                          rc);
      return;
    }
    break;
  }
  case 1:    /* Unbound */
  {
    int rc = m_config_applier->do_bind_io(m_thread);
    if (rc < 0)
    {
      g_eventLogger->info("~THRConfigRebinder(%p) bind failed : %u", m_thread,
                          rc);
    }
    break;
  }
  case 0:
    break;
  }
  return;
}

int
THRConfigApplier::do_bind_send(NdbThread* thread, unsigned instance)
{
  const T_Thread* thr = &m_threads[T_SEND][instance];
  return do_bind(thread, thr);
}

bool
THRConfigApplier::do_get_nosend(const unsigned short list[],
                                unsigned cnt) const
{
  const T_Thread* thr = find_thread(list, cnt);
  return (bool)thr->m_nosend;
}

bool
THRConfigApplier::do_get_realtime(const unsigned short list[],
                                  unsigned cnt) const
{
  const T_Thread* thr = find_thread(list, cnt);
  return (bool)thr->m_realtime;
}

unsigned
THRConfigApplier::do_get_spintime(const unsigned short list[],
                                  unsigned cnt) const
{
  const T_Thread* thr = find_thread(list, cnt);
  return thr->m_spintime;
}

bool
THRConfigApplier::do_get_realtime_io() const
{
  const T_Thread* thr = &m_threads[T_IO][0];
  return (bool)thr->m_realtime;
}

bool
THRConfigApplier::do_get_realtime_wd() const
{
  const T_Thread* thr = &m_threads[T_WD][0];
  return (bool)thr->m_realtime;
}

bool
THRConfigApplier::do_get_realtime_send(unsigned instance) const
{
  const T_Thread* thr = &m_threads[T_SEND][instance];
  return (bool)thr->m_realtime;
}

unsigned
THRConfigApplier::do_get_spintime_send(unsigned instance) const
{
  const T_Thread* thr = &m_threads[T_SEND][instance];
  return thr->m_spintime;
}

int
THRConfigApplier::do_thread_prio_io(NdbThread *thread,
                                    unsigned &thread_prio)
{
  const T_Thread* thr = &m_threads[T_IO][0];
  return do_thread_prio(thread, thr, thread_prio);
}

int
THRConfigApplier::do_thread_prio_watchdog(NdbThread *thread,
                                          unsigned &thread_prio)
{
  const T_Thread* thr = &m_threads[T_WD][0];
  return do_thread_prio(thread, thr, thread_prio);
}

int
THRConfigApplier::do_thread_prio_send(NdbThread *thread,
                                      unsigned instance,
                                      unsigned &thread_prio)
{
  const T_Thread* thr = &m_threads[T_SEND][instance];
  return do_thread_prio(thread, thr, thread_prio);
}

int
THRConfigApplier::do_thread_prio(NdbThread* thread,
                                 const unsigned short list[],
                                 unsigned cnt,
                                 unsigned &thread_prio)
{
  const T_Thread* thr = find_thread(list, cnt);
  return do_thread_prio(thread, thr, thread_prio);
}

int
THRConfigApplier::do_thread_prio(NdbThread* thread,
                                 const T_Thread* thr,
                                 unsigned &thread_prio)
{
  int res = 0;
  thread_prio = NO_THREAD_PRIO_USED;
  if (thr->m_thread_prio != NO_THREAD_PRIO_USED)
  {
    thread_prio = thr->m_thread_prio;
    res = NdbThread_SetThreadPrio(thread, thr->m_thread_prio);
    if (res == 0)
    {
      return 1;
    }
    thread_prio = NO_THREAD_PRIO_USED;
    return -res;
  }
  return res;
}

static void
print_core_bind_string(const char *type_str,
                       Uint32 thr_no,
                       Uint32 core_cpu_ids[MAX_NUM_CPUS],
                       Uint32 num_core_cpus)
{
  char num_buf[128];
  char buf[1024];
  Uint32 buf_inx = 0;
  snprintf(buf, sizeof(buf), "%s thread %u locked to CPUs: ",
           type_str,
           thr_no);
  buf_inx = strlen(buf);
  for (Uint32 i = 0; i < num_core_cpus; i++)
  {
    if (i != 0)
    {
      buf[buf_inx++] = ',';
      buf[buf_inx++] = ' ';
    }
    snprintf(num_buf, sizeof(num_buf), "%u", core_cpu_ids[i]);
    Uint32 len = strlen(num_buf);
    memcpy(&buf[buf_inx], num_buf, len);
    buf_inx += len;
  }
  buf[buf_inx] = 0;
  g_eventLogger->info("%s", buf);
}

int
THRConfigApplier::do_bind(NdbThread* thread,
                          const T_Thread* thr)
{
  int res = -1;
  const char *type_str = nullptr;
  switch (thr->m_type)
  {
    case T_MAIN:
      type_str = "main";
      break;
    case T_LDM:
      type_str = "ldm";
      break;
    case T_RECV:
      type_str = "recv";
      break;
    case T_REP:
      type_str = "rep";
      break;
    case T_IO:
      type_str = "io";
      break;
    case T_WD:
      type_str = "watchdog";
      break;
    case T_TC:
      type_str = "tc";
      break;
    case T_SEND:
      type_str = "send";
      break;
    case T_IXBLD:
      type_str = "ixbld";
      break;
    case T_QUERY:
      type_str = "query";
      break;
    case T_RECOVER:
      type_str = "recover";
      break;
    default:
      break;
  }
  if (thr->m_bind_type == T_Thread::B_CPU_BIND)
  {
    if (thr->m_core_bind)
    {
      Uint32 core_cpu_ids[MAX_NUM_CPUS];
      Uint32 num_core_cpus = 0;
      Ndb_GetCoreCPUIds(thr->m_bind_no, &core_cpu_ids[0], num_core_cpus);
      print_core_bind_string(type_str,
                             thr->m_no,
                             core_cpu_ids,
                             num_core_cpus);
      res = Ndb_LockCPUSet(thread,
                           &core_cpu_ids[0],
                           num_core_cpus,
                           false);
    }
    else
    {
      res = Ndb_LockCPU(thread, thr->m_bind_no);
    }
  }
  else if (thr->m_bind_type == T_Thread::B_CPU_BIND_EXCLUSIVE)
  {
    /**
     * Bind to a CPU set exclusively to ensure no other threads
     * can use these CPUs.
     */
    if (thr->m_core_bind)
    {
      Uint32 core_cpu_ids[MAX_NUM_CPUS];
      Uint32 num_core_cpus = 0;
      Ndb_GetCoreCPUIds(thr->m_bind_no, &core_cpu_ids[0], num_core_cpus);
      print_core_bind_string(type_str,
                             thr->m_no,
                             core_cpu_ids,
                             num_core_cpus);
      res = Ndb_LockCPUSet(thread,
                           &core_cpu_ids[0],
                           num_core_cpus,
                           true);
    }
    else
    {
      Uint32 cpu_ids = thr->m_bind_no;
      res = Ndb_LockCPUSet(thread,
                           &cpu_ids,
                           (Uint32)1,
                           true);
    }
  }
  else if (thr->m_bind_type == T_Thread::B_CPUSET_BIND ||
           thr->m_bind_type == T_Thread::B_CPUSET_EXCLUSIVE_BIND)
  {
    const SparseBitmask & tmp = m_cpu_sets[thr->m_bind_no];
    Uint32 num_cpu_ids = tmp.count();
    Uint32 *cpu_ids = (Uint32*)malloc(sizeof(Uint32) * num_cpu_ids);
    if (!cpu_ids)
    {
      return -errno;
    }
    /* Build cpu_ids set from SparseBitmask */
    for (unsigned i = 0; i < num_cpu_ids; i++)
    {
      cpu_ids[i] = tmp.getBitNo(i);
    }
    bool is_exclusive;
    if (thr->m_bind_type == T_Thread::B_CPUSET_EXCLUSIVE_BIND)
    {
      /* Bind to a CPU set exclusively */
      is_exclusive = true;
    }
    else
    {
      /* Bind to a CPU set non-exclusively */
      is_exclusive = false;
    }
    res = Ndb_LockCPUSet(thread,
                         cpu_ids,
                         num_cpu_ids, 
                         is_exclusive);
    free((void*)cpu_ids);
  }
  else
  {
    return 0;
  }
  if (res == 0)
    return 1;
  else
    return -res;
}

#define JAM_FILE_ID 297

