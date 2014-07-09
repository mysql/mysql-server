/*
   Copyright (c) 2011, 2013, Oracle and/or its affiliates. All rights reserved.

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

#define JAM_FILE_ID 272


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
    T_IO    = 4, /* File threads */
    T_WD    = 5, /* SocketServer, Socket Client, Watchdog */
    T_TC    = 6, /* TC+SPJ */
    T_SEND  = 7, /* No blocks */

    T_END  = 8
  };

  THRConfig();
  ~THRConfig();

  // NOTE: needs to be called before do_parse
  int setLockExecuteThreadToCPU(const char * val);
  int setLockIoThreadsToCPU(unsigned val);

  int do_parse(const char * ThreadConfig,
               unsigned realtime,
               unsigned spintime);
  int do_parse(unsigned MaxNoOfExecutionThreads,
               unsigned __ndbmt_lqh_threads,
               unsigned __ndbmt_classic,
               unsigned realtime,
               unsigned spintime);

  const char * getConfigString();
  void append_name(const char *name,
                   const char *sep,
                   bool & append_name_flag);

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
    unsigned m_realtime; //0 = no realtime, 1 = realtime
    unsigned m_spintime; //0 = no spinning, > 0 spintime in microseconds
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

  void add(T_Type, unsigned realtime, unsigned spintime);
  Uint32 find_type(char *&);
  int find_spec(char *&, T_Type, unsigned real_time, unsigned spin_time);
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
  const char * getName(const unsigned short list[], unsigned cnt) const;
  void appendInfo(BaseString&, const unsigned short list[], unsigned cnt) const;
  void appendInfoSendThread(BaseString&, unsigned instance_no) const;
  int do_bind(NdbThread*, const unsigned short list[], unsigned cnt);
  int do_bind_io(NdbThread*);
  int do_bind_watchdog(NdbThread*);
  int do_bind_send(NdbThread*, unsigned);
  bool do_get_realtime_io() const;
  bool do_get_realtime_wd() const;
  bool do_get_realtime_send(unsigned) const;
  unsigned do_get_spintime_send(unsigned) const;
  bool do_get_realtime(const unsigned short list[],
                       unsigned cnt) const;
  unsigned do_get_spintime(const unsigned short list[],
                           unsigned cnt) const;

protected:
  const T_Thread* find_thread(const unsigned short list[], unsigned cnt) const;
  void appendInfo(BaseString&, const T_Thread*) const;
  int do_bind(NdbThread*, const T_Thread*);
};


#undef JAM_FILE_ID

#endif // IPCConfig_H
