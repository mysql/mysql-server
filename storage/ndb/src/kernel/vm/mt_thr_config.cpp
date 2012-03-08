/*
   Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.

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

#include "mt_thr_config.hpp"
#include <kernel/ndb_limits.h>
#include "../../common/util/parse_mask.hpp"

#ifndef TEST_MT_THR_CONFIG
#define SUPPORT_CPU_SET 0
#else
#define SUPPORT_CPU_SET 1
#endif

static const struct THRConfig::Entries m_entries[] =
{
  // name    type              min  max
  { "main",  THRConfig::T_MAIN,  1, 1 },
  { "ldm",   THRConfig::T_LDM,   1, MAX_NDBMT_LQH_THREADS },
  { "recv",  THRConfig::T_RECV,  1, 1 },
  { "rep",   THRConfig::T_REP,   1, 1 },
  { "io",    THRConfig::T_IO,    1, 1 }
};

static const struct THRConfig::Param m_params[] =
{
  { "count",   THRConfig::Param::S_UNSIGNED },
  { "cpubind", THRConfig::Param::S_BITMASK },
  { "cpuset",  THRConfig::Param::S_BITMASK }
};

#define IX_COUNT    0
#define IX_CPUBOUND 1
#define IX_CPUSET   2

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
  for (Uint32 i = 0; i<NDB_ARRAY_SIZE(m_entries); i++)
  {
    if (m_entries[i].m_type == type)
      return m_entries[i].m_name;
  }
  return 0;
}

static
Uint32
getEntryType(const char * type)
{
  for (Uint32 i = 0; i<NDB_ARRAY_SIZE(m_entries); i++)
  {
    if (strcasecmp(type, m_entries[i].m_name) == 0)
      return i;
  }

  return THRConfig::T_END;
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
  return 0;
}

int
THRConfig::setLockIoThreadsToCPU(unsigned val)
{
  m_LockIoThreadsToCPU.set(val);
  return 0;
}

void
THRConfig::add(T_Type t)
{
  T_Thread tmp;
  tmp.m_type = t;
  tmp.m_bind_type = T_Thread::B_UNBOUND;
  tmp.m_no = m_threads[t].size();
  m_threads[t].push_back(tmp);
}

int
THRConfig::do_parse(unsigned MaxNoOfExecutionThreads,
                    unsigned __ndbmt_lqh_threads,
                    unsigned __ndbmt_classic)
{
  /**
   * This is old ndbd.cpp : get_multithreaded_config
   */
  if (__ndbmt_classic)
  {
    m_classic = true;
    add(T_LDM);
    add(T_MAIN);
    add(T_IO);
    return do_bindings();
  }

  Uint32 lqhthreads = 0;
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
  default:
    lqhthreads = 4; // TC + receiver + SUMA + 4 * LQH
  }

  if (__ndbmt_lqh_threads)
  {
    lqhthreads = __ndbmt_lqh_threads;
  }

  add(T_MAIN);
  add(T_REP);
  add(T_RECV);
  add(T_IO);
  for(Uint32 i = 0; i < lqhthreads; i++)
  {
    add(T_LDM);
  }

  return do_bindings() || do_validate();
}

int
THRConfig::do_bindings()
{
  if (m_LockIoThreadsToCPU.count() == 1)
  {
    m_threads[T_IO][0].m_bind_type = T_Thread::B_CPU_BOUND;
    m_threads[T_IO][0].m_bind_no = m_LockIoThreadsToCPU.getBitNo(0);
  }
  else if (m_LockIoThreadsToCPU.count() > 1)
  {
    unsigned no = createCpuSet(m_LockIoThreadsToCPU);
    m_threads[T_IO][0].m_bind_type = T_Thread::B_CPUSET_BOUND;
    m_threads[T_IO][0].m_bind_no = no;
  }

  /**
   * Check that no cpu_sets overlap
   */
  for (unsigned i = 0; i<m_cpu_sets.size(); i++)
  {
    for (unsigned j = i + 1; j < m_cpu_sets.size(); j++)
    {
      if (m_cpu_sets[i].overlaps(m_cpu_sets[j]))
      {
        m_err_msg.assfmt("Overlapping cpuset's [ %s ] and [ %s ]",
                         m_cpu_sets[i].str().c_str(),
                         m_cpu_sets[j].str().c_str());
        return -1;
      }
    }
  }

  /**
   * Check that no cpu_sets overlap
   */
  for (unsigned i = 0; i < NDB_ARRAY_SIZE(m_threads); i++)
  {
    for (unsigned j = 0; j < m_threads[i].size(); j++)
    {
      if (m_threads[i][j].m_bind_type == T_Thread::B_CPU_BOUND)
      {
        unsigned cpu = m_threads[i][j].m_bind_no;
        for (unsigned k = 0; k<m_cpu_sets.size(); k++)
        {
          if (m_cpu_sets[k].get(cpu))
          {
            m_err_msg.assfmt("Overlapping cpubind %u with cpuset [ %s ]",
                             cpu,
                             m_cpu_sets[k].str().c_str());

            return -1;
          }
        }
      }
    }
  }

  /**
   * Remove all already bound threads from LockExecuteThreadToCPU-mask
   */
  for (unsigned i = 0; i<m_cpu_sets.size(); i++)
  {
    for (unsigned j = 0; j < m_cpu_sets[i].count(); j++)
    {
      m_LockExecuteThreadToCPU.clear(m_cpu_sets[i].getBitNo(j));
    }
  }

  unsigned cnt_unbound = 0;
  for (unsigned i = 0; i < NDB_ARRAY_SIZE(m_threads); i++)
  {
    for (unsigned j = 0; j < m_threads[i].size(); j++)
    {
      if (m_threads[i][j].m_bind_type == T_Thread::B_CPU_BOUND)
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

  if (m_threads[T_IO][0].m_bind_type == T_Thread::B_UNBOUND)
  {
    /**
     * don't count this one...
     */
    cnt_unbound--;
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

    if (cnt < num_threads)
    {
      m_info_msg.assfmt("WARNING: Too few CPU's specified with "
                        "LockExecuteThreadToCPU. Only %u specified "
                        " but %u was needed, this may cause contention.\n",
                        cnt, num_threads);
    }

    if (cnt >= num_threads)
    {
      m_info_msg.appfmt("Assigning each thread its own CPU\n");
      unsigned no = 0;
      for (unsigned i = 0; i < NDB_ARRAY_SIZE(m_threads); i++)
      {
        if (i == T_IO)
          continue;
        for (unsigned j = 0; j < m_threads[i].size(); j++)
        {
          if (m_threads[i][j].m_bind_type == T_Thread::B_UNBOUND)
          {
            m_threads[i][j].m_bind_type = T_Thread::B_CPU_BOUND;
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
        if (i == T_IO)
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
            m_threads[T_LDM][i].m_bind_type = T_Thread::B_CPU_BOUND;
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
            m_threads[T_LDM][i].m_bind_type = T_Thread::B_CPU_BOUND;
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
      vec[i].m_bind_type = T_Thread::B_CPU_BOUND;
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
   * LDM can be 1 2 4
   */
  if (m_threads[T_LDM].size() == 3)
  {
    m_err_msg.assfmt("No of LDM-instances can be 1,2,4. Specified: %u",
                     m_threads[T_LDM].size());
    return -1;
  }

  return 0;
}

const char *
THRConfig::getConfigString()
{
  m_cfg_string.clear();
  const char * sep = "";
  for (unsigned i = 0; i < NDB_ARRAY_SIZE(m_threads); i++)
  {
    if (m_threads[i].size())
    {
      const char * name = getEntryName(i);
      if (i != T_IO)
      {
        for (unsigned j = 0; j < m_threads[i].size(); j++)
        {
          m_cfg_string.append(sep);
          sep=",";
          m_cfg_string.append(name);
          if (m_threads[i][j].m_bind_type != T_Thread::B_UNBOUND)
          {
            m_cfg_string.append("={");
            if (m_threads[i][j].m_bind_type == T_Thread::B_CPU_BOUND)
            {
              m_cfg_string.appfmt("cpubind=%u", m_threads[i][j].m_bind_no);
            }
            else if (m_threads[i][j].m_bind_type == T_Thread::B_CPUSET_BOUND)
            {
              m_cfg_string.appfmt("cpuset=%s",
                                  m_cpu_sets[m_threads[i][j].m_bind_no].str().c_str());
            }
            m_cfg_string.append("}");
          }
        }
      }
      else
      {
        for (unsigned j = 0; j < m_threads[i].size(); j++)
        {
          if (m_threads[i][j].m_bind_type != T_Thread::B_UNBOUND)
          {
            m_cfg_string.append(sep);
            sep=",";
            m_cfg_string.append(name);
            m_cfg_string.append("={");
            if (m_threads[i][j].m_bind_type == T_Thread::B_CPU_BOUND)
            {
              m_cfg_string.appfmt("cpubind=%u", m_threads[i][j].m_bind_no);
            }
            else if (m_threads[i][j].m_bind_type == T_Thread::B_CPUSET_BOUND)
            {
              m_cfg_string.appfmt("cpuset=%s",
                                  m_cpu_sets[m_threads[i][j].m_bind_no].str().c_str());
            }
            m_cfg_string.append("}");
          }
        }
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
    if (i != T_IO)
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

static
char *
skipblank(char * str)
{
  while (isspace(* str))
    str++;
  return str;
}

Uint32
THRConfig::find_type(char *& str)
{
  str = skipblank(str);

  char * name = str;
  if (* name == 0)
  {
    m_err_msg.assfmt("empty thread specification");
    return 0;
  }
  char * end = name;
  while(isalpha(* end))
    end++;

  char save = * end;
  * end = 0;
  Uint32 t = getEntryType(name);
  if (t == T_END)
  {
    m_err_msg.assfmt("unknown thread type '%s'", name);
  }
  * end = save;
  str = end;
  return t;
}

struct ParamValue
{
  ParamValue() { found = false;}
  bool found;
  const char * string_val;
  unsigned unsigned_val;
  SparseBitmask mask_val;
};

static
int
parseUnsigned(char *& str, unsigned * dst)
{
  str = skipblank(str);
  char * endptr = 0;
  errno = 0;
  long val = strtol(str, &endptr, 0);
  if (errno == ERANGE)
    return -1;
  if (val < 0 || Int64(val) > 0xFFFFFFFF)
    return -1;
  if (endptr == str)
    return -1;
  str = endptr;
  *dst = (unsigned)val;
  return 0;
}

static
int
parseBitmask(char *& str, SparseBitmask * mask)
{
  str = skipblank(str);
  size_t len = strspn(str, "0123456789-, ");
  if (len == 0)
    return -1;

  while (isspace(str[len-1]))
    len--;
  if (str[len-1] == ',')
    len--;
  char save = str[len];
  str[len] = 0;
  int res = parse_mask(str, *mask);
  str[len] = save;
  str = str + len;
  return res;
}

static
int
parseParams(char * str, ParamValue values[], BaseString& err)
{
  const char * const save = str;
  while (* str)
  {
    str = skipblank(str);

    unsigned idx = 0;
    for (; idx < NDB_ARRAY_SIZE(m_params); idx++)
    {

#if ! SUPPORT_CPU_SET
      if (idx == IX_CPUSET)
        continue;
#endif

      if (strncasecmp(str, m_params[idx].name, strlen(m_params[idx].name)) == 0)
      {
        str += strlen(m_params[idx].name);
        break;
      }
    }

    if (idx == NDB_ARRAY_SIZE(m_params))
    {
      err.assfmt("Unknown param near: '%s'", str);
      return -1;
    }

    if (values[idx].found == true)
    {
      err.assfmt("Param '%s' found twice", m_params[idx].name);
      return -1;
    }

    str = skipblank(str);
    if (* str != '=')
    {
      err.assfmt("Missing '=' after %s in '%s'", m_params[idx].name, save);
      return -1;
    }
    str++;
    str = skipblank(str);

    int res = 0;
    switch(m_params[idx].type){
    case THRConfig::Param::S_UNSIGNED:
      res = parseUnsigned(str, &values[idx].unsigned_val);
      break;
    case THRConfig::Param::S_BITMASK:
      res = parseBitmask(str, &values[idx].mask_val);
      break;
    default:
      err.assfmt("Internal error, unknown type for param: '%s'",
                 m_params[idx].name);
      return -1;
    }
    if (res == -1)
    {
      err.assfmt("Unable to parse %s=%s", m_params[idx].name, str);
      return -1;
    }
    values[idx].found = true;
    str = skipblank(str);

    if (* str == 0)
      break;

    if (* str != ',')
    {
      err.assfmt("Unable to parse near '%s'", str);
      return -1;
    }
    str++;
  }
  return 0;
}

int
THRConfig::find_spec(char *& str, T_Type type)
{
  str = skipblank(str);

  switch(* str){
  case ',':
  case 0:
    add(type);
    return 0;
  }

  if (* str != '=')
  {
err:
    int len = (int)strlen(str);
    m_err_msg.assfmt("Invalid format near: '%.*s'",
                     (len > 10) ? 10 : len, str);
    return -1;
  }

  str++; // skip over =
  str = skipblank(str);

  if (* str != '{')
  {
    goto err;
  }

  str++;
  char * start = str;

  /**
   * Find end
   */
  while (* str && (* str) != '}')
    str++;

  if (* str != '}')
  {
    goto err;
  }

  char * end = str;
  char save = * end;
  * end = 0;

  ParamValue values[NDB_ARRAY_SIZE(m_params)];
  values[IX_COUNT].unsigned_val = 1;
  int res = parseParams(start, values, m_err_msg);
  * end = save;

  if (res != 0)
  {
    return -1;
  }

  if (values[IX_CPUBOUND].found && values[IX_CPUSET].found)
  {
    m_err_msg.assfmt("Both cpuset and cpubind specified!");
    return -1;
  }

  unsigned cnt = values[IX_COUNT].unsigned_val;
  const int index = m_threads[type].size();
  for (unsigned i = 0; i < cnt; i++)
  {
    add(type);
  }

  assert(m_threads[type].size() == index + cnt);
  if (values[IX_CPUSET].found)
  {
    SparseBitmask & mask = values[IX_CPUSET].mask_val;
    unsigned no = createCpuSet(mask);
    for (unsigned i = 0; i < cnt; i++)
    {
      m_threads[type][index+i].m_bind_type = T_Thread::B_CPUSET_BOUND;
      m_threads[type][index+i].m_bind_no = no;
    }
  }
  else if (values[IX_CPUBOUND].found)
  {
    SparseBitmask & mask = values[IX_CPUBOUND].mask_val;
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
      m_threads[type][index+i].m_bind_type = T_Thread::B_CPU_BOUND;
      m_threads[type][index+i].m_bind_no = mask.getBitNo(i % mask.count());
    }
  }

  str++; // skip over }
  return 0;
}

int
THRConfig::find_next(char *& str)
{
  str = skipblank(str);

  if (* str == 0)
  {
    return 0;
  }
  else if (* str == ',')
  {
    str++;
    return 1;
  }

  int len = (int)strlen(str);
  m_err_msg.assfmt("Invalid format near: '%.*s'",
                   (len > 10) ? 10 : len, str);
  return -1;
}

int
THRConfig::do_parse(const char * ThreadConfig)
{
  BaseString str(ThreadConfig);
  char * ptr = (char*)str.c_str();
  while (* ptr)
  {
    Uint32 type = find_type(ptr);
    if (type == T_END)
      return -1;

    if (find_spec(ptr, (T_Type)type) < 0)
      return -1;

    int ret = find_next(ptr);
    if (ret < 0)
      return ret;

    if (ret == 0)
      break;
  }

  for (Uint32 i = 0; i < T_END; i++)
  {
    while (m_threads[i].size() < m_entries[i].m_min_cnt)
      add((T_Type)i);
  }

  return do_bindings() || do_validate();
}

unsigned
THRConfig::createCpuSet(const SparseBitmask& mask)
{
  for (unsigned i = 0; i < m_cpu_sets.size(); i++)
    if (m_cpu_sets[i].equal(mask))
      return i;

  m_cpu_sets.push_back(mask);
  return m_cpu_sets.size() - 1;
}

template class Vector<SparseBitmask>;
template class Vector<THRConfig::T_Thread>;

#ifndef TEST_MT_THR_CONFIG
#include <BlockNumbers.h>
#include <NdbThread.h>

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
    return &m_threads[T_REP][instanceNo];
  }
  else if ((instanceNo = findBlock(CMVMI, instancelist, cnt)) >= 0)
  {
    return &m_threads[T_RECV][instanceNo];
  }
  else if ((instanceNo = findBlock(DBDIH, instancelist, cnt)) >= 0)
  {
    return &m_threads[T_MAIN][instanceNo];
  }
  else if ((instanceNo = findBlock(DBLQH, instancelist, cnt)) >= 0)
  {
    return &m_threads[T_LDM][instanceNo - 1]; // remove proxy...
  }
  return 0;
}

void
THRConfigApplier::appendInfo(BaseString& str,
                             const unsigned short list[], unsigned cnt) const
{
  const T_Thread* thr = find_thread(list, cnt);
  assert(thr != 0);
  str.appfmt("(%s) ", getEntryName(thr->m_type));
  if (thr->m_bind_type == T_Thread::B_CPU_BOUND)
  {
    str.appfmt("cpu: %u ", thr->m_bind_no);
  }
  else if (thr->m_bind_type == T_Thread::B_CPUSET_BOUND)
  {
    str.appfmt("cpuset: [ %s ] ", m_cpu_sets[thr->m_bind_no].str().c_str());
  }
}

int
THRConfigApplier::create_cpusets()
{
  return 0;
}

int
THRConfigApplier::do_bind(NdbThread* thread,
                          const unsigned short list[], unsigned cnt)
{
  const T_Thread* thr = find_thread(list, cnt);
  if (thr->m_bind_type == T_Thread::B_CPU_BOUND)
  {
    int res = NdbThread_LockCPU(thread, thr->m_bind_no);
    if (res == 0)
      return 1;
    else
      return -res;
  }
#if TODO
  else if (thr->m_bind_type == T_Thread::B_CPUSET_BOUND)
  {
  }
#endif

  return 0;
}

int
THRConfigApplier::do_bind_io(NdbThread* thread)
{
  const T_Thread* thr = &m_threads[T_IO][0];
  if (thr->m_bind_type == T_Thread::B_CPU_BOUND)
  {
    int res = NdbThread_LockCPU(thread, thr->m_bind_no);
    if (res == 0)
      return 1;
    else
      return -res;
  }
#if TODO
  else if (thr->m_bind_type == T_Thread::B_CPUSET_BOUND)
  {
  }
#endif

  return 0;
}
#endif

#ifdef TEST_MT_THR_CONFIG

#include <NdbTap.hpp>

TAPTEST(mt_thr_config)
{
  {
    THRConfig tmp;
    OK(tmp.do_parse(8, 0, 0) == 0);
  }

  /**
   * BASIC test
   */
  {
    const char * ok[] =
      {
        "ldm,ldm",
        "ldm={count=3},ldm",
        "ldm={cpubind=1-2,5,count=3},ldm",
        "ldm={ cpubind = 1- 2, 5 , count = 3 },ldm",
        "ldm={count=3,cpubind=1-2,5 },  ldm",
        "ldm={cpuset=1-3,count=3 },ldm",
        "main,ldm={},ldm",
        0
      };

    const char * fail [] =
      {
        "ldm,ldm,ldm",
        "ldm={cpubind= 1 , cpuset=2 },ldm",
        "ldm={count=4,cpubind=1-3},ldm",
        "main,main,ldm,ldm",
        "main={ keso=88, count=23},ldm,ldm",
        "main={ cpuset=1-3 }, ldm={cpuset=3-4}",
        "main={ cpuset=1-3 }, ldm={cpubind=2}",
        0
      };

    for (Uint32 i = 0; ok[i]; i++)
    {
      THRConfig tmp;
      int res = tmp.do_parse(ok[i]);
      printf("do_parse(%s) => %s - %s\n", ok[i],
             res == 0 ? "OK" : "FAIL",
             res == 0 ? "" : tmp.getErrorMessage());
      OK(res == 0);
      {
        BaseString out(tmp.getConfigString());
        THRConfig check;
        OK(check.do_parse(out.c_str()) == 0);
        OK(strcmp(out.c_str(), check.getConfigString()) == 0);
      }
    }

    for (Uint32 i = 0; fail[i]; i++)
    {
      THRConfig tmp;
      int res = tmp.do_parse(fail[i]);
      printf("do_parse(%s) => %s - %s\n", fail[i],
             res == 0 ? "OK" : "FAIL",
             res == 0 ? "" : tmp.getErrorMessage());
      OK(res != 0);
    }
  }

  {
    /**
     * Test interaction with LockExecuteThreadToCPU
     */
    const char * t[] =
    {
      /** threads, LockExecuteThreadToCPU, answer */
      "1-8",
      "ldm={count=4}",
      "main={cpubind=1},ldm={cpubind=2},ldm={cpubind=3},ldm={cpubind=4},ldm={cpubind=5},recv={cpubind=6},rep={cpubind=7}",

      "1-5",
      "ldm={count=4}",
      "main={cpubind=5},ldm={cpubind=1},ldm={cpubind=2},ldm={cpubind=3},ldm={cpubind=4},recv={cpubind=5},rep={cpubind=5}",

      "1-3",
      "ldm={count=4}",
      "main={cpubind=1},ldm={cpubind=2},ldm={cpubind=3},ldm={cpubind=2},ldm={cpubind=3},recv={cpubind=1},rep={cpubind=1}",

      "1-4",
      "ldm={count=4}",
      "main={cpubind=1},ldm={cpubind=2},ldm={cpubind=3},ldm={cpubind=4},ldm={cpubind=2},recv={cpubind=1},rep={cpubind=1}",

      "1-8",
      "ldm={count=4},io={cpubind=8}",
      "main={cpubind=1},ldm={cpubind=2},ldm={cpubind=3},ldm={cpubind=4},ldm={cpubind=5},recv={cpubind=6},rep={cpubind=7},io={cpubind=8}",

      "1-8",
      "ldm={count=4,cpubind=1,4,5,6}",
      "main={cpubind=2},ldm={cpubind=1},ldm={cpubind=4},ldm={cpubind=5},ldm={cpubind=6},recv={cpubind=3},rep={cpubind=7}",

      // END
      0
    };

    for (unsigned i = 0; t[i]; i+= 3)
    {
      THRConfig tmp;
      tmp.setLockExecuteThreadToCPU(t[i+0]);
      int res = tmp.do_parse(t[i+1]);
      int ok = strcmp(tmp.getConfigString(), t[i+2]) == 0;
      printf("mask: %s conf: %s => %s(%s) - %s - %s\n",
             t[i+0],
             t[i+1],
             res == 0 ? "OK" : "FAIL",
             res == 0 ? "" : tmp.getErrorMessage(),
             tmp.getConfigString(),
             ok == 1 ? "CORRECT" : "INCORRECT");
      OK(res == 0);
      OK(ok == 1);
    }
  }

  return 1;
}

#endif
