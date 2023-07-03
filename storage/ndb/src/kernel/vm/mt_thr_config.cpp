/*
<<<<<<< HEAD
   Copyright (c) 2011, 2022, Oracle and/or its affiliates.
=======
   Copyright (c) 2011, 2021, Oracle and/or its affiliates.
>>>>>>> pr/231

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
<<<<<<< HEAD
#include <NdbHW.hpp>
#include <EventLogger.hpp>
=======

static const struct ParseEntries m_parse_entries[] =
{
  //name     type
  { "main",  THRConfig::T_MAIN },
  { "ldm",   THRConfig::T_LDM  },
  { "recv",  THRConfig::T_RECV },
  { "rep",   THRConfig::T_REP  },
  { "io",    THRConfig::T_IO   },
  { "watchdog", THRConfig::T_WD},
  { "tc",    THRConfig::T_TC   },
  { "send",  THRConfig::T_SEND },
  { "idxbld",THRConfig::T_IXBLD}
};

static const struct THRConfig::Entries m_entries[] =
{
  //type               min max                        exec thread   permanent
  { THRConfig::T_MAIN,  1, 1,                         true,         true },
  { THRConfig::T_LDM,   1, MAX_NDBMT_LQH_THREADS,     true,         true },
  { THRConfig::T_RECV,  1, MAX_NDBMT_RECEIVE_THREADS, true,         true },
  { THRConfig::T_REP,   1, 1,                         true,         true },
  { THRConfig::T_IO,    1, 1,                         false,        true },
  { THRConfig::T_WD,    1, 1,                         false,        true },
  { THRConfig::T_TC,    0, MAX_NDBMT_TC_THREADS,      true,         true },
  { THRConfig::T_SEND,  0, MAX_NDBMT_SEND_THREADS,    true,         true },
  { THRConfig::T_IXBLD, 0, 1,                         false,        false}
};

static const struct ParseParams m_params[] =
{
  { "count",    ParseParams::S_UNSIGNED },
  { "cpubind",  ParseParams::S_BITMASK },
  { "cpubind_exclusive",  ParseParams::S_BITMASK },
  { "cpuset",   ParseParams::S_BITMASK },
  { "cpuset_exclusive",   ParseParams::S_BITMASK },
  { "realtime", ParseParams::S_UNSIGNED },
  { "spintime", ParseParams::S_UNSIGNED },
  { "thread_prio", ParseParams::S_UNSIGNED },
  { "nosend", ParseParams::S_UNSIGNED }
};

#define IX_COUNT    0
#define IX_CPUBIND 1
#define IX_CPUBIND_EXCLUSIVE 2
#define IX_CPUSET   3
#define IX_CPUSET_EXCLUSIVE   4
#define IX_REALTIME 5
#define IX_SPINTIME 6
#define IX_THREAD_PRIO 7
#define IX_NOSEND 8

static
unsigned
getMaxEntries(Uint32 type)
{
  for (Uint32 i = 0; i<NDB_ARRAY_SIZE(m_entries); i++)
  {
    if (m_entries[i].m_type == type)
      return m_entries[i].m_max_cnt;
  }
  return 0;
}

static
const char *
getEntryName(Uint32 type)
{
  for (unsigned int i = 0; i < NDB_ARRAY_SIZE(m_parse_entries); i++)
  {
    if (m_parse_entries[i].m_type == type)
    {
      return m_parse_entries[i].m_name;
    }
  }
  return 0;
}

THRConfig::THRConfig()
{
  m_classic = false;
}

THRConfig::~THRConfig()
{
}

int
THRConfig::setLockExecuteThreadToCPU(const char * mask)
{
  int res = parse_mask(mask, m_LockExecuteThreadToCPU);
  if (res < 0)
  {
    m_err_msg.assfmt("failed to parse 'LockExecuteThreadToCPU=%s' "
                     "(error: %d)",
                     mask, res);
    return -1;
  }
  else if (res == 0)
  {
    m_err_msg.assfmt("LockExecuteThreadToCPU: %s"
                     " with empty bitmask not allowed",
                     mask);
    return -1;
  }
  return 0;
}

int
THRConfig::setLockIoThreadsToCPU(unsigned val)
{
  m_LockIoThreadsToCPU.set(val);
  return 0;
}

void
THRConfig::add(T_Type t, unsigned realtime, unsigned spintime)
{
  T_Thread tmp;
  tmp.m_type = t;
  tmp.m_bind_type = T_Thread::B_UNBOUND;
  tmp.m_no = m_threads[t].size();
  tmp.m_realtime = realtime;
  tmp.m_thread_prio = NO_THREAD_PRIO_USED;
  tmp.m_nosend = 0;
  if (spintime > 9000)
    spintime = 9000;
  tmp.m_spintime = spintime;
  m_threads[t].push_back(tmp);
}

static
void
computeThreadConfig(Uint32 MaxNoOfExecutionThreads,
                    Uint32 & tcthreads,
                    Uint32 & lqhthreads,
                    Uint32 & sendthreads,
                    Uint32 & recvthreads)
{
  assert(MaxNoOfExecutionThreads >= 9);
  static const struct entry
  {
    Uint32 M;
    Uint32 lqh;
    Uint32 tc;
    Uint32 send;
    Uint32 recv;
  } table[] = {
    { 9, 4, 2, 0, 1 },
    { 10, 4, 2, 1, 1 },
    { 11, 4, 3, 1, 1 },
    { 12, 6, 2, 1, 1 },
    { 13, 6, 3, 1, 1 },
    { 14, 6, 3, 1, 2 },
    { 15, 6, 3, 2, 2 },
    { 16, 8, 3, 1, 2 },
    { 17, 8, 4, 1, 2 },
    { 18, 8, 4, 2, 2 },
    { 19, 8, 5, 2, 2 },
    { 20, 10, 4, 2, 2 },
    { 21, 10, 5, 2, 2 },
    { 22, 10, 5, 2, 3 },
    { 23, 10, 6, 2, 3 },
    { 24, 12, 5, 2, 3 },
    { 25, 12, 6, 2, 3 },
    { 26, 12, 6, 3, 3 },
    { 27, 12, 7, 3, 3 },
    { 28, 12, 7, 3, 4 },
    { 29, 12, 8, 3, 4 },
    { 30, 12, 8, 4, 4 },
    { 31, 12, 9, 4, 4 },
    { 32, 16, 8, 3, 3 },
    { 33, 16, 8, 3, 4 },
    { 34, 16, 8, 4, 4 },
    { 35, 16, 9, 4, 4 },
    { 36, 16, 10, 4, 4 },
    { 37, 16, 10, 4, 5 },
    { 38, 16, 11, 4, 5 },
    { 39, 16, 11, 5, 5 },
    { 40, 20, 10, 4, 4 },
    { 41, 20, 10, 4, 5 },
    { 42, 20, 11, 4, 5 },
    { 43, 20, 11, 5, 5 },
    { 44, 20, 12, 5, 5 },
    { 45, 20, 12, 5, 6 },
    { 46, 20, 13, 5, 6 },
    { 47, 20, 13, 6, 6 },
    { 48, 24, 12, 5, 5 },
    { 49, 24, 12, 5, 6 },
    { 50, 24, 13, 5, 6 },
    { 51, 24, 13, 6, 6 },
    { 52, 24, 14, 6, 6 },
    { 53, 24, 14, 6, 7 },
    { 54, 24, 15, 6, 7 },
    { 55, 24, 15, 7, 7 },
    { 56, 24, 16, 7, 7 },
    { 57, 24, 16, 7, 8 },
    { 58, 24, 17, 7, 8 },
    { 59, 24, 17, 8, 8 },
    { 60, 24, 18, 8, 8 },
    { 61, 24, 18, 8, 9 },
    { 62, 24, 19, 8, 9 },
    { 63, 24, 19, 9, 9 },
    { 64, 32, 16, 7, 7 },
    { 65, 32, 16, 7, 8 },
    { 66, 32, 17, 7, 8 },
    { 67, 32, 17, 8, 8 },
    { 68, 32, 18, 8, 8 },
    { 69, 32, 18, 8, 9 },
    { 70, 32, 19, 8, 9 },
    { 71, 32, 20, 8, 9 },
    { 72, 32, 20, 8, 10 }
  };

  Uint32 P = MaxNoOfExecutionThreads - 9;
  if (P >= NDB_ARRAY_SIZE(table))
  {
    P = NDB_ARRAY_SIZE(table) - 1;
  }

  lqhthreads = table[P].lqh;
  tcthreads = table[P].tc;
  sendthreads = table[P].send;
  recvthreads = table[P].recv;
}

int
THRConfig::do_parse(unsigned MaxNoOfExecutionThreads,
                    unsigned __ndbmt_lqh_threads,
                    unsigned __ndbmt_classic,
                    unsigned realtime,
                    unsigned spintime)
{
  /**
   * This is old ndbd.cpp : get_multithreaded_config
   */
  if (__ndbmt_classic)
  {
    m_classic = true;
    add(T_LDM, realtime, spintime);
    add(T_MAIN, realtime, spintime);
    add(T_IO, realtime, 0);
    add(T_WD, realtime, 0);
    const bool allow_too_few_cpus = true;
    return do_bindings(allow_too_few_cpus);
  }

  Uint32 tcthreads = 0;
  Uint32 lqhthreads = 0;
  Uint32 sendthreads = 0;
  Uint32 recvthreads = 1;
  switch(MaxNoOfExecutionThreads){
  case 0:
  case 1:
  case 2:
  case 3:
    lqhthreads = 1; // TC + receiver + SUMA + LQH
    break;
  case 4:
  case 5:
  case 6:
    lqhthreads = 2; // TC + receiver + SUMA + 2 * LQH
    break;
  case 7:
  case 8:
    lqhthreads = 4; // TC + receiver + SUMA + 4 * LQH
    break;
  default:
    computeThreadConfig(MaxNoOfExecutionThreads,
                        tcthreads,
                        lqhthreads,
                        sendthreads,
                        recvthreads);
  }

  if (__ndbmt_lqh_threads)
  {
    lqhthreads = __ndbmt_lqh_threads;
  }

  add(T_MAIN, realtime, spintime); /* Global */
  add(T_REP, realtime, spintime);  /* Local, main consumer is SUMA */
  for(Uint32 i = 0; i < recvthreads; i++)
  {
    add(T_RECV, realtime, spintime);
  }
  add(T_IO, realtime, 0);
  add(T_WD, realtime, 0);
  for(Uint32 i = 0; i < lqhthreads; i++)
  {
    add(T_LDM, realtime, spintime);
  }
  for(Uint32 i = 0; i < tcthreads; i++)
  {
    add(T_TC, realtime, spintime);
  }
  for(Uint32 i = 0; i < sendthreads; i++)
  {
    add(T_SEND, realtime, spintime);
  }

  // If we have set TC-threads...we say that this is "new" code
  // and give error for having too few CPU's in mask compared to #threads
  // started
  const bool allow_too_few_cpus = (tcthreads == 0 &&
                                   sendthreads == 0 &&
                                   recvthreads == 1);
  return do_bindings(allow_too_few_cpus) || do_validate();
}

int
THRConfig::do_bindings(bool allow_too_few_cpus)
{
  /* Track all cpus that we lock threads to */
  SparseBitmask allCpus; 
  allCpus.bitOR(m_LockIoThreadsToCPU);

  /**
   * Use LockIoThreadsToCPU also to lock to Watchdog, SocketServer
   * and SocketClient for backwards compatibility reasons, 
   * preferred manner is to only use ThreadConfig
   */
  if (m_LockIoThreadsToCPU.count() == 1)
  {
    m_threads[T_IO][0].m_bind_type = T_Thread::B_CPU_BIND;
    m_threads[T_IO][0].m_bind_no = m_LockIoThreadsToCPU.getBitNo(0);
    m_threads[T_WD][0].m_bind_type = T_Thread::B_CPU_BIND;
    m_threads[T_WD][0].m_bind_no = m_LockIoThreadsToCPU.getBitNo(0);
  }
  else if (m_LockIoThreadsToCPU.count() > 1)
  {
    unsigned no = createCpuSet(m_LockIoThreadsToCPU, true);
    m_threads[T_IO][0].m_bind_type = T_Thread::B_CPUSET_BIND;
    m_threads[T_IO][0].m_bind_no = no;
    m_threads[T_WD][0].m_bind_type = T_Thread::B_CPUSET_BIND;
    m_threads[T_WD][0].m_bind_no = no;
  }

  /**
   * Check that no permanent cpu_sets overlap
   */
  for (unsigned i = 0; i<m_perm_cpu_sets.size(); i++)
  {
    const SparseBitmask& a = m_cpu_sets[m_perm_cpu_sets[i]];
    allCpus.bitOR(a);

    for (unsigned j = i + 1; j < m_perm_cpu_sets.size(); j++)
    {
      const SparseBitmask& b = m_cpu_sets[m_perm_cpu_sets[j]];
      if (a.overlaps(b))
      {
        m_err_msg.assfmt("Overlapping cpuset's [ %s ] and [ %s ]",
                         a.str().c_str(),
                         b.str().c_str());
        return -1;
      }
    }
  }

  /**
   * Check that no permanent cpu_sets overlap with cpu_bound
   */
  for (unsigned i = 0; i < NDB_ARRAY_SIZE(m_threads); i++)
  {
    for (unsigned j = 0; j < m_threads[i].size(); j++)
    {
      if (m_threads[i][j].m_bind_type == T_Thread::B_CPU_BIND)
      {
        unsigned cpu = m_threads[i][j].m_bind_no;
        allCpus.set(cpu);
        for (unsigned k = 0; k < m_perm_cpu_sets.size(); k++)
        {
          const SparseBitmask& cpuSet = m_cpu_sets[m_perm_cpu_sets[k]];
          if (cpuSet.get(cpu))
          {
            m_err_msg.assfmt("Overlapping cpubind %u with cpuset [ %s ]",
                             cpu,
                             cpuSet.str().c_str());

            return -1;
          }
        }
      }
    }
  }

  /**
   * Remove all already bound threads from LockExecuteThreadToCPU-mask
   */
  for (unsigned i = 0; i < m_perm_cpu_sets.size(); i++)
  {
    const SparseBitmask& cpuSet = m_cpu_sets[m_perm_cpu_sets[i]];
    for (unsigned j = 0; j < cpuSet.count(); j++)
    {
      m_LockExecuteThreadToCPU.clear(cpuSet.getBitNo(j));
    }
  }

  unsigned cnt_unbound = 0;
  for (unsigned i = 0; i < NDB_ARRAY_SIZE(m_threads); i++)
  {
    if (!m_entries[i].m_is_exec_thd)
    {
      /* Only interested in execution threads here */
      continue;
    }
    for (unsigned j = 0; j < m_threads[i].size(); j++)
    {
      if (m_threads[i][j].m_bind_type == T_Thread::B_CPU_BIND)
      {
        unsigned cpu = m_threads[i][j].m_bind_no;
        m_LockExecuteThreadToCPU.clear(cpu);
      }
      else if (m_threads[i][j].m_bind_type == T_Thread::B_UNBOUND)
      {
        cnt_unbound ++;
      }
    }
  }

  if (m_LockExecuteThreadToCPU.count())
  {
    /**
     * This is old mt.cpp : setcpuaffinity
     */
    SparseBitmask& mask = m_LockExecuteThreadToCPU;
    unsigned cnt = mask.count();
    unsigned num_threads = cnt_unbound;
    bool isMtLqh = !m_classic;

    allCpus.bitOR(m_LockExecuteThreadToCPU);
    if (cnt < num_threads)
    {
      m_info_msg.assfmt("WARNING: Too few CPU's specified with "
                        "LockExecuteThreadToCPU. Only %u specified "
                        " but %u was needed, this may cause contention.\n",
                        cnt, num_threads);

      if (!allow_too_few_cpus)
      {
        m_err_msg.assfmt("Too few CPU's specifed with LockExecuteThreadToCPU. "
                         "This is not supported when using multiple TC threads");
        return -1;
      }
    }

    if (cnt >= num_threads)
    {
      m_info_msg.appfmt("Assigning each thread its own CPU\n");
      unsigned no = 0;
      for (unsigned i = 0; i < NDB_ARRAY_SIZE(m_threads); i++)
      {
        if (!m_entries[i].m_is_exec_thd)
          continue;
        for (unsigned j = 0; j < m_threads[i].size(); j++)
        {
          if (m_threads[i][j].m_bind_type == T_Thread::B_UNBOUND)
          {
            m_threads[i][j].m_bind_type = T_Thread::B_CPU_BIND;
            m_threads[i][j].m_bind_no = mask.getBitNo(no);
            no++;
          }
        }
      }
    }
    else if (cnt == 1)
    {
      unsigned cpu = mask.getBitNo(0);
      m_info_msg.appfmt("Assigning all threads to CPU %u\n", cpu);
      for (unsigned i = 0; i < NDB_ARRAY_SIZE(m_threads); i++)
      {
        if (!m_entries[i].m_is_exec_thd)
          continue;
        bind_unbound(m_threads[i], cpu);
      }
    }
    else if (isMtLqh)
    {
      unsigned unbound_ldm = count_unbound(m_threads[T_LDM]);
      if (cnt > unbound_ldm)
      {
        /**
         * let each LQH have it's own CPU and rest share...
         */
        m_info_msg.append("Assigning LQH threads to dedicated CPU(s) and "
                          "other threads will share remaining\n");
        unsigned cpu = mask.find(0);
        for (unsigned i = 0; i < m_threads[T_LDM].size(); i++)
        {
          if (m_threads[T_LDM][i].m_bind_type == T_Thread::B_UNBOUND)
          {
            m_threads[T_LDM][i].m_bind_type = T_Thread::B_CPU_BIND;
            m_threads[T_LDM][i].m_bind_no = cpu;
            mask.clear(cpu);
            cpu = mask.find(cpu + 1);
          }
        }

        cpu = mask.find(0);
        bind_unbound(m_threads[T_MAIN], cpu);
        bind_unbound(m_threads[T_REP], cpu);
        if ((cpu = mask.find(cpu + 1)) == mask.NotFound)
        {
          cpu = mask.find(0);
        }
        bind_unbound(m_threads[T_RECV], cpu);
      }
      else
      {
        // put receiver, tc, backup/suma in 1 thread,
        // and round robin LQH for rest
        unsigned cpu = mask.find(0);
        m_info_msg.appfmt("Assigning LQH threads round robin to CPU(s) and "
                          "other threads will share CPU %u\n", cpu);
        bind_unbound(m_threads[T_MAIN], cpu); // TC
        bind_unbound(m_threads[T_REP], cpu);
        bind_unbound(m_threads[T_RECV], cpu);
        mask.clear(cpu);

        cpu = mask.find(0);
        for (unsigned i = 0; i < m_threads[T_LDM].size(); i++)
        {
          if (m_threads[T_LDM][i].m_bind_type == T_Thread::B_UNBOUND)
          {
            m_threads[T_LDM][i].m_bind_type = T_Thread::B_CPU_BIND;
            m_threads[T_LDM][i].m_bind_no = cpu;
            if ((cpu = mask.find(cpu + 1)) == mask.NotFound)
            {
              cpu = mask.find(0);
            }
          }
        }
      }
    }
    else
    {
      unsigned cpu = mask.find(0);
      m_info_msg.appfmt("Assigning LQH thread to CPU %u and "
                        "other threads will share\n", cpu);
      bind_unbound(m_threads[T_LDM], cpu);
      cpu = mask.find(cpu + 1);
      bind_unbound(m_threads[T_MAIN], cpu);
      bind_unbound(m_threads[T_RECV], cpu);
    }
  }
  if (m_threads[T_IXBLD].size() == 0)
  {
    /**
     * No specific IDXBLD configuration from the user
     * In this case : IDXBLD should be :
     *  - Unbound if IO is unbound - use any core
     *  - Bound to the full set of bound threads if
     *    IO is bound - assumes nothing better for
     *    threads to do.
     */
    const T_Thread* io_thread = &m_threads[T_IO][0];
    add(T_IXBLD, 0, 0);

    if (io_thread->m_bind_type != T_Thread::B_UNBOUND)
    {
      /* IO thread is bound, we should be bound to 
       * all defined threads
       */
      BaseString allCpusString = allCpus.str();
      m_info_msg.appfmt("IO threads explicitly bound, "
                        "but IDX_BLD threads not.  "
                        "Binding IDX_BLD to %s.\n",
                        allCpusString.c_str());

      unsigned no = createCpuSet(allCpus, false);
      
      m_threads[T_IXBLD][0].m_bind_type = T_Thread::B_CPUSET_BIND;
      m_threads[T_IXBLD][0].m_bind_no = no;
    }
  }

  return 0;
}

unsigned
THRConfig::count_unbound(const Vector<T_Thread>& vec) const
{
  unsigned cnt = 0;
  for (unsigned i = 0; i < vec.size(); i++)
  {
    if (vec[i].m_bind_type == T_Thread::B_UNBOUND)
      cnt ++;
  }
  return cnt;
}

void
THRConfig::bind_unbound(Vector<T_Thread>& vec, unsigned cpu)
{
  for (unsigned i = 0; i < vec.size(); i++)
  {
    if (vec[i].m_bind_type == T_Thread::B_UNBOUND)
    {
      vec[i].m_bind_type = T_Thread::B_CPU_BIND;
      vec[i].m_bind_no = cpu;
    }
  }
}

int
THRConfig::do_validate()
{
  /**
   * Check that there aren't too many of any thread type
   */
  for (unsigned i = 0; i< NDB_ARRAY_SIZE(m_threads); i++)
  {
    if (m_threads[i].size() > getMaxEntries(i))
    {
      m_err_msg.assfmt("Too many instances(%u) of %s max supported: %u",
                       m_threads[i].size(),
                       getEntryName(i),
                       getMaxEntries(i));
      return -1;
    }
  }

  /**
   * LDM can be 1 2 4 6 8 10 12 16 20 24 32
   */
  if (m_threads[T_LDM].size() != 1 &&
      m_threads[T_LDM].size() != 2 &&
      m_threads[T_LDM].size() != 4 &&
      m_threads[T_LDM].size() != 6 &&
      m_threads[T_LDM].size() != 8 &&
      m_threads[T_LDM].size() != 10 &&
      m_threads[T_LDM].size() != 12 &&
      m_threads[T_LDM].size() != 16 &&
      m_threads[T_LDM].size() != 20 &&
      m_threads[T_LDM].size() != 24 &&
      m_threads[T_LDM].size() != 32)
  {
    m_err_msg.assfmt("No of LDM-instances can be 1,2,4,6,8,12,16,24 or 32. Specified: %u",
                     m_threads[T_LDM].size());
    return -1;
  }

  return 0;
}

void
THRConfig::append_name(const char *name,
                       const char *sep,
                       bool & append_name_flag)
{
  if (!append_name_flag)
  {
    m_cfg_string.append(sep);
    m_cfg_string.append(name);
    append_name_flag = true;
  }
}

const char *
THRConfig::getConfigString()
{
  m_cfg_string.clear();
  const char * sep = "";
  const char * end_sep;
  const char * start_sep;
  const char * between_sep;
  bool append_name_flag;
  for (unsigned i = 0; i < NDB_ARRAY_SIZE(m_threads); i++)
  {
    if (m_threads[i].size())
    {
      const char * name = getEntryName(i);
      for (unsigned j = 0; j < m_threads[i].size(); j++)
      {
        start_sep = "={";
        end_sep = "";
        between_sep="";
        append_name_flag = false;
        if (m_entries[i].m_is_exec_thd)
        {
          append_name(name, sep, append_name_flag);
          sep=",";
        }
        if (m_threads[i][j].m_bind_type != T_Thread::B_UNBOUND)
        {
          append_name(name, sep, append_name_flag);
          sep=",";
          m_cfg_string.append(start_sep);
          end_sep = "}";
          start_sep="";
          if (m_threads[i][j].m_bind_type == T_Thread::B_CPU_BIND)
          {
            m_cfg_string.appfmt("cpubind=%u", m_threads[i][j].m_bind_no);
            between_sep=",";
          }
          else if (m_threads[i][j].m_bind_type ==
                   T_Thread::B_CPU_BIND_EXCLUSIVE)
          {
            m_cfg_string.appfmt("cpubind_exclusive=%u", m_threads[i][j].m_bind_no);
            between_sep=",";
          }
          else if (m_threads[i][j].m_bind_type == T_Thread::B_CPUSET_BIND)
          {
            m_cfg_string.appfmt("cpuset=%s",
                                m_cpu_sets[m_threads[i][j].m_bind_no].str().c_str());
            between_sep=",";
          }
          else if (m_threads[i][j].m_bind_type == T_Thread::B_CPUSET_EXCLUSIVE_BIND)
          {
            m_cfg_string.appfmt("cpuset_exclusive=%s",
                                m_cpu_sets[m_threads[i][j].m_bind_no].str().c_str());
            between_sep=",";
          }
        }
        if (m_threads[i][j].m_spintime || m_threads[i][j].m_realtime)
        {
          append_name(name, sep, append_name_flag);
          sep=",";
          m_cfg_string.append(start_sep);
          end_sep = "}";
          if (m_threads[i][j].m_spintime)
          {
            m_cfg_string.append(between_sep);
            m_cfg_string.appfmt("spintime=%u",
                                m_threads[i][j].m_spintime);
            between_sep=",";
          }
          if (m_threads[i][j].m_realtime)
          {
            m_cfg_string.append(between_sep);
            m_cfg_string.appfmt("realtime=%u",
                                m_threads[i][j].m_realtime);
            between_sep=",";
          }
        }
        m_cfg_string.append(end_sep);
      }
    }
  }
  return m_cfg_string.c_str();
}

Uint32
THRConfig::getThreadCount() const
{
  // Note! not counting T_IO
  Uint32 cnt = 0;
  for (Uint32 i = 0; i < NDB_ARRAY_SIZE(m_threads); i++)
  {
    if (m_entries[i].m_is_exec_thd)
    {
      cnt += m_threads[i].size();
    }
  }
  return cnt;
}

Uint32
THRConfig::getThreadCount(T_Type type) const
{
  for (Uint32 i = 0; i < NDB_ARRAY_SIZE(m_threads); i++)
  {
    if (i == (Uint32)type)
    {
      return m_threads[i].size();
    }
  }
  return 0;
}

const char *
THRConfig::getErrorMessage() const
{
  if (m_err_msg.empty())
    return 0;
  return m_err_msg.c_str();
}

const char *
THRConfig::getInfoMessage() const
{
  if (m_info_msg.empty())
    return 0;
  return m_info_msg.c_str();
}

int
THRConfig::handle_spec(char *str,
                       unsigned realtime,
                       unsigned spintime)
{
  ParseThreadConfiguration parser(str,
                                  &m_parse_entries[0],
                                  NDB_ARRAY_SIZE(m_parse_entries),
                                  &m_params[0],
                                  NDB_ARRAY_SIZE(m_params),
                                  m_err_msg);

  do
  {
    unsigned int loc_type;
    int ret_code;
    ParamValue values[NDB_ARRAY_SIZE(m_params)];
    values[IX_COUNT].unsigned_val = 1;
    values[IX_REALTIME].unsigned_val = realtime;
    values[IX_THREAD_PRIO].unsigned_val = NO_THREAD_PRIO_USED;
    values[IX_SPINTIME].unsigned_val = spintime;

    if (parser.read_params(values,
                           NDB_ARRAY_SIZE(m_params),
                           &loc_type,
                           &ret_code,
                           true) != 0)
    {
      /* Parser is done either successful or not */
      return ret_code;
    }
    T_Type type = (T_Type)loc_type;

    int cpu_values = 0;
    if (values[IX_CPUBIND].found)
      cpu_values++;
    if (values[IX_CPUBIND_EXCLUSIVE].found)
      cpu_values++;
    if (values[IX_CPUSET].found)
      cpu_values++;
    if (values[IX_CPUSET_EXCLUSIVE].found)
      cpu_values++;
    if (cpu_values > 1)
    {
      m_err_msg.assfmt("Only one of cpubind, cpuset and cpuset_exclusive"
                       " can be specified");
      return -1;
    }
    if (values[IX_REALTIME].found &&
        values[IX_THREAD_PRIO].found &&
        values[IX_REALTIME].unsigned_val != 0)
    {
      m_err_msg.assfmt("Only one of realtime and thread_prio can be used to"
                       " change thread priority in the OS scheduling");
      return -1;
    }
    if (values[IX_THREAD_PRIO].found &&
        values[IX_THREAD_PRIO].unsigned_val > MAX_THREAD_PRIO_NUMBER)
    {
      m_err_msg.assfmt("thread_prio must be between 0 and 10, where 10 is the"
                       " highest priority");
      return -1;
    }
    if (values[IX_SPINTIME].found && !m_entries[type].m_is_exec_thd)
    {
      m_err_msg.assfmt("Cannot set spintime on non-exec threads");
      return -1;
    }
    if (values[IX_NOSEND].found &&
        !(type == T_LDM ||
          type == T_TC ||
          type == T_MAIN ||
          type == T_REP))
    {
      m_err_msg.assfmt("Can only set nosend on main, ldm, tc and rep threads");
      return -1;
    }
    if (values[IX_THREAD_PRIO].found && type == T_IXBLD)
    {
      m_err_msg.assfmt("Cannot set threadprio on idxbld threads");
      return -1;
    }
    if (values[IX_REALTIME].found && type == T_IXBLD)
    {
      m_err_msg.assfmt("Cannot set realtime on idxbld threads");
      return -1;
    }
 
    unsigned cnt = values[IX_COUNT].unsigned_val;
    const int index = m_threads[type].size();
    for (unsigned i = 0; i < cnt; i++)
    {
      add(type,
          values[IX_REALTIME].unsigned_val,
          values[IX_SPINTIME].unsigned_val);
    }

    assert(m_threads[type].size() == index + cnt);
    if (values[IX_CPUSET].found)
    {
      const SparseBitmask & mask = values[IX_CPUSET].mask_val;
      unsigned no = createCpuSet(mask, m_entries[type].m_is_permanent);
      for (unsigned i = 0; i < cnt; i++)
      {
        m_threads[type][index+i].m_bind_type = T_Thread::B_CPUSET_BIND;
        m_threads[type][index+i].m_bind_no = no;
      }
    }
    else if (values[IX_CPUSET_EXCLUSIVE].found)
    {
      const SparseBitmask & mask = values[IX_CPUSET_EXCLUSIVE].mask_val;
      unsigned no = createCpuSet(mask, m_entries[type].m_is_permanent);
      for (unsigned i = 0; i < cnt; i++)
      {
        m_threads[type][index+i].m_bind_type =
          T_Thread::B_CPUSET_EXCLUSIVE_BIND;
        m_threads[type][index+i].m_bind_no = no;
      }
    }
    else if (values[IX_CPUBIND].found)
    {
      const SparseBitmask & mask = values[IX_CPUBIND].mask_val;
      if (mask.count() < cnt)
      {
        m_err_msg.assfmt("%s: trying to bind %u threads to %u cpus [%s]",
                         getEntryName(type),
                         cnt,
                         mask.count(),
                         mask.str().c_str());
        return -1;
      }
      for (unsigned i = 0; i < cnt; i++)
      {
        m_threads[type][index+i].m_bind_type = T_Thread::B_CPU_BIND;
        m_threads[type][index+i].m_bind_no = mask.getBitNo(i % mask.count());
      }
    }
    else if (values[IX_CPUBIND_EXCLUSIVE].found)
    {
      const SparseBitmask & mask = values[IX_CPUBIND_EXCLUSIVE].mask_val;
      if (mask.count() < cnt)
      {
        m_err_msg.assfmt("%s: trying to bind %u threads to %u cpus [%s]",
                         getEntryName(type),
                         cnt,
                         mask.count(),
                         mask.str().c_str());
        return -1;
      }
      for (unsigned i = 0; i < cnt; i++)
      {
        m_threads[type][index+i].m_bind_type = T_Thread::B_CPU_BIND_EXCLUSIVE;
        m_threads[type][index+i].m_bind_no = mask.getBitNo(i % mask.count());
      }
    }
    if (values[IX_THREAD_PRIO].found)
    {
      for (unsigned i = 0; i < cnt; i++)
      {
        m_threads[type][index+i].m_thread_prio =
          values[IX_THREAD_PRIO].unsigned_val;
      }
    }
    if (values[IX_NOSEND].found)
    {
      for (unsigned i = 0; i < cnt; i++)
      {
        m_threads[type][index+i].m_nosend =
          values[IX_NOSEND].unsigned_val;
      }
    }
  } while (1);
  return 0;
}

int
THRConfig::do_parse(const char * ThreadConfig,
                    unsigned realtime,
                    unsigned spintime)
{
  BaseString str(ThreadConfig);
  char * ptr = (char*)str.c_str();
  int ret = handle_spec(ptr, realtime, spintime);

  if (ret != 0)
    return ret;

  for (Uint32 i = 0; i < T_END; i++)
  {
    while (m_threads[i].size() < m_entries[i].m_min_cnt)
      add((T_Type)i, realtime, spintime);
  }

  const bool allow_too_few_cpus =
    m_threads[T_TC].size() == 0 &&
    m_threads[T_SEND].size() == 0 &&
    m_threads[T_RECV].size() == 1;

  int res = do_bindings(allow_too_few_cpus);
  if (res != 0)
  {
    return res;
  }

  return do_validate();
}

unsigned
THRConfig::createCpuSet(const SparseBitmask& mask, bool permanent)
{
  /**
   * Create a cpuset according to the passed mask, and return its number
   * If one with that mask already exists, just return the existing
   * number.
   * A subset of all cpusets are on a 'permanent' list.  Permanent
   * cpusets must be non-overlapping.
   * Non permanent cpusets can overlap with permanent cpusets
   */
  unsigned i = 0;
  for ( ; i < m_cpu_sets.size(); i++)
  {
    if (m_cpu_sets[i].equal(mask))
    {
      break;
    }
  }

  if (i == m_cpu_sets.size())
  {
    /* Not already present */
    m_cpu_sets.push_back(mask);
  }
  if (permanent)
  {
    /**
     * Add to permanent cpusets list, if not already there
     * (existing cpuset could be !permanent)
     */
    unsigned j = 0;
    for (; j< m_perm_cpu_sets.size(); j++)
    {
      if (m_perm_cpu_sets[j] == i)
      {
        break;
      }
    }
    
    if (j == m_perm_cpu_sets.size())
    {
      m_perm_cpu_sets.push_back(i);
    }
  }
  return i;
}

template class Vector<SparseBitmask>;
template class Vector<THRConfig::T_Thread>;

#ifndef TEST_MT_THR_CONFIG
>>>>>>> pr/231
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

