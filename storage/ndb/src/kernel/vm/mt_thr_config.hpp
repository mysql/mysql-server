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

#ifndef THRConfig_H
#define THRConfig_H

struct NdbThread;
#include <Vector.hpp>
#include <SparseBitmask.hpp>
#include <BaseString.hpp>

/**
 * This class contains thread configuration
 *   it supports parsing the ThreadConfig parameter
 *   and handling LockExecuteThreadToCPU etc...
 *
 * This is used in ndb_mgmd when verifying configuration
 *   and by ndbmtd
 *
 * TAP-TESTS are provided in mt_thr_config.cpp
 */
class THRConfig
{
public:
  enum T_Type
  {
    T_MAIN  = 0, /* DIH/QMGR/TC/SPJ etc */
    T_LDM   = 1, /* LQH/ACC/TUP/TUX etc */
    T_RECV  = 2, /* CMVMI */
    T_REP   = 3, /* SUMA */
    T_IO    = 4, /* FS, SocketServer etc */
    T_TC    = 5, /* TC+SPJ */
    T_SEND  = 6, /* No blocks */

    T_END  = 7
  };

  THRConfig();
  ~THRConfig();

  // NOTE: needs to be called before do_parse
  int setLockExecuteThreadToCPU(const char * val);
  int setLockIoThreadsToCPU(unsigned val);

  int do_parse(const char * ThreadConfig);
  int do_parse(unsigned MaxNoOfExecutionThreads,
               unsigned __ndbmt_lqh_threads,
               unsigned __ndbmt_classic);

  const char * getConfigString();

  const char * getErrorMessage() const;
  const char * getInfoMessage() const;

  Uint32 getThreadCount() const; // Don't count FS/IO thread
  Uint32 getThreadCount(T_Type) const;
  Uint32 getMtClassic() const { return m_classic; }
protected:
  struct T_Thread
  {
    unsigned m_type;
    unsigned m_no; // within type
    enum BType { B_UNBOUND, B_CPU_BOUND, B_CPUSET_BOUND } m_bind_type;
    unsigned m_bind_no; // cpu_no/cpuset_no
  };
  bool m_classic;
  SparseBitmask m_LockExecuteThreadToCPU;
  SparseBitmask m_LockIoThreadsToCPU;
  Vector<SparseBitmask> m_cpu_sets;
  Vector<T_Thread> m_threads[T_END];

  BaseString m_err_msg;
  BaseString m_info_msg;
  BaseString m_cfg_string;
  BaseString m_print_string;

  void add(T_Type);
  Uint32 find_type(char *&);
  int find_spec(char *&, T_Type);
  int find_next(char *&);

  unsigned createCpuSet(const SparseBitmask&);
  int do_bindings(bool allow_too_few_cpus);
  int do_validate();

  unsigned count_unbound(const Vector<T_Thread>& vec) const;
  void bind_unbound(Vector<T_Thread> & vec, unsigned cpu);

public:
  struct Entries
  {
    const char * m_name;
    unsigned m_type;
    unsigned m_min_cnt;
    unsigned m_max_cnt;
  };

  struct Param
  {
    const char * name;
    enum { S_UNSIGNED, S_BITMASK } type;
  };
};

/**
 * This class is used by ndbmtd
 *   when setting up threads (and locking)
 */
class THRConfigApplier : public THRConfig
{
public:
  int create_cpusets();

  const char * getName(const unsigned short list[], unsigned cnt) const;
  void appendInfo(BaseString&, const unsigned short list[], unsigned cnt) const;
  void appendInfoSendThread(BaseString&, unsigned instance_no) const;
  int do_bind(NdbThread*, const unsigned short list[], unsigned cnt);
  int do_bind_io(NdbThread*);
  int do_bind_send(NdbThread*, unsigned);

protected:
  const T_Thread* find_thread(const unsigned short list[], unsigned cnt) const;
  void appendInfo(BaseString&, const T_Thread*) const;
  int do_bind(NdbThread*, const T_Thread*);
};

#endif // IPCConfig_H
