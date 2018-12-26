/*
   Copyright (c) 2011, 2018, Oracle and/or its affiliates. All rights reserved.

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
    T_IXBLD = 8, /* File thread during offline index build */

    T_END  = 9
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
    enum BType
    {
      B_UNBOUND,
      B_CPU_BIND,
      B_CPU_BIND_EXCLUSIVE,
      B_CPUSET_BIND,
      B_CPUSET_EXCLUSIVE_BIND
    } m_bind_type;
    unsigned m_bind_no; // cpu_no/cpuset_no
    unsigned m_thread_prio; // Between 0 and 10, 11 means not used
    unsigned m_realtime; //0 = no realtime, 1 = realtime
    unsigned m_spintime; //0 = no spinning, > 0 spintime in microseconds
    unsigned m_nosend; //0 = assist send thread, 1 = cannot assist send thread
  };
  bool m_classic;
  SparseBitmask m_LockExecuteThreadToCPU;
  SparseBitmask m_LockIoThreadsToCPU;
  Vector<SparseBitmask> m_cpu_sets;
  Vector<unsigned> m_perm_cpu_sets;
  Vector<T_Thread> m_threads[T_END];

  BaseString m_err_msg;
  BaseString m_info_msg;
  BaseString m_cfg_string;
  BaseString m_print_string;

  void add(T_Type, unsigned realtime, unsigned spintime);
  int handle_spec(char *ptr, unsigned real_time, unsigned spin_time);

  unsigned createCpuSet(const SparseBitmask&, bool permanent);
  int do_bindings(bool allow_too_few_cpus);
  int do_validate();

  unsigned count_unbound(const Vector<T_Thread>& vec) const;
  void bind_unbound(Vector<T_Thread> & vec, unsigned cpu);

public:
  struct Entries
  {
    unsigned m_type;
    unsigned m_min_cnt;
    unsigned m_max_cnt;
    bool     m_is_exec_thd; /* Is this a non-blocking execution thread type */
    bool     m_is_permanent;/* Is this a fixed thread type */
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

  int do_thread_prio_io(NdbThread*, unsigned &thread_prio);
  int do_thread_prio_watchdog(NdbThread*, unsigned &thread_prio);
  int do_thread_prio_send(NdbThread*,
                          unsigned instance,
                          unsigned &thread_prio);
  int do_thread_prio(NdbThread*,
                     const unsigned short list[],
                     unsigned cnt,
                     unsigned &thread_prio);
  int do_thread_prio(NdbThread*,
                     const T_Thread* thr,
                     unsigned &thread_prio);

  int do_bind(NdbThread*, const unsigned short list[], unsigned cnt);
  int do_bind_io(NdbThread*);
  int do_bind_idxbuild(NdbThread*);
  int do_bind_watchdog(NdbThread*);
  int do_bind_send(NdbThread*, unsigned);
  int do_unbind(NdbThread*);
  bool do_get_nosend(const unsigned short list[],
                     unsigned cnt) const;
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

/**
 * This class is used to temporarily change the thread
 * type during some task
 */
class THRConfigRebinder
{
public:
  THRConfigRebinder(THRConfigApplier*, THRConfig::T_Type, NdbThread*);
  ~THRConfigRebinder();
private:
  THRConfigApplier* m_config_applier;
  int m_state;
  NdbThread* m_thread;
};

#undef JAM_FILE_ID

#endif // THRConfig_H
