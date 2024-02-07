/*
   Copyright (c) 2022, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef THR_CONFIG_HPP
#define THR_CONFIG_HPP

#include <BaseString.hpp>
#include <SparseBitmask.hpp>
#include <Vector.hpp>

/**
 * This class contains thread configuration
 *   it supports parsing the ThreadConfig parameter
 *   and handling LockExecuteThreadToCPU etc...
 *
 * This is used in ndb_mgmd when verifying configuration
 *   and by ndbmtd
 *
 * TAP-TESTS are provided in thr_config.cpp
 */
class THRConfig {
 public:
  enum T_Type {
    T_MAIN = 0,     /* DIH/QMGR/TC/SPJ etc */
    T_LDM = 1,      /* LQH/ACC/TUP/TUX etc */
    T_RECV = 2,     /* CMVMI */
    T_REP = 3,      /* SUMA */
    T_IO = 4,       /* File threads */
    T_WD = 5,       /* SocketServer, Socket Client, Watchdog */
    T_TC = 6,       /* TC+SPJ */
    T_SEND = 7,     /* No blocks */
    T_IXBLD = 8,    /* File thread during offline index build */
    T_QUERY = 9,    /* Query threads */
    T_RECOVER = 10, /* Recover threads */
    T_END = 11
  };

  THRConfig();
  ~THRConfig();

  // NOTE: needs to be called before do_parse
  int setLockExecuteThreadToCPU(const char *val);
  int setLockIoThreadsToCPU(unsigned val);

  int do_parse(unsigned realtime, unsigned spintime, unsigned num_cpus,
               unsigned &num_rr_groups);
  int do_parse(const char *ThreadConfig, unsigned realtime, unsigned spintime);
  int do_parse(unsigned MaxNoOfExecutionThreads, unsigned __ndbmt_lqh_threads,
               unsigned __ndbmt_classic, unsigned realtime, unsigned spintime);

  const char *getConfigString();

  void append_name(const char *name, const char *sep, bool &append_name_flag);

  const char *getErrorMessage() const;
  const char *getInfoMessage() const;

  Uint32 getThreadCount() const;  // Don't count FS/IO thread
  Uint32 getThreadCount(T_Type) const;
  Uint32 getMtClassic() const { return m_classic; }
  static bool isThreadPermanent(T_Type type);

 protected:
  struct T_Thread {
    unsigned m_type;
    unsigned m_no;  // within type
    enum BType {
      B_UNBOUND,
      B_CPU_BIND,
      B_CPU_BIND_EXCLUSIVE,
      B_CPUSET_BIND,
      B_CPUSET_EXCLUSIVE_BIND
    } m_bind_type;
    unsigned m_bind_no;      // cpu_no/cpuset_no
    unsigned m_thread_prio;  // Between 0 and 10, 11 means not used
    unsigned m_realtime;     // 0 = no realtime, 1 = realtime
    unsigned m_spintime;     // 0 = no spinning, > 0 spintime in microseconds
    unsigned m_nosend;  // 0 = assist send thread, 1 = cannot assist send thread
    bool m_core_bind;   // Bind to all CPUs in CPU core
  };
  bool m_classic;

  SparseBitmask m_setInThreadConfig;
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
  int handle_spec(const char *ptr, unsigned real_time, unsigned spin_time);

  unsigned createCpuSet(const SparseBitmask &, bool permanent);
  void lock_io_threads();
  int do_bindings(bool allow_too_few_cpus);
  int do_validate();
  int do_validate_thread_counts();

  unsigned count_unbound(const Vector<T_Thread> &vec) const;
  void bind_unbound(Vector<T_Thread> &vec, unsigned cpu);

  void compute_automatic_thread_config(
      Uint32 num_cpus, Uint32 &tc_threads, Uint32 &ldm_threads,
      Uint32 &query_threads, Uint32 &recover_threads, Uint32 &main_threads,
      Uint32 &rep_threads, Uint32 &send_threads, Uint32 &recv_threads);

  static unsigned getMaxEntries(Uint32 type);
  static unsigned getMinEntries(Uint32 type);
  static const char *getEntryName(Uint32 type);

 public:
  struct Entries {
    unsigned m_type;
    unsigned m_min_cnt;
    unsigned m_max_cnt;
    bool m_is_exec_thd;  /* Is this a non-blocking execution thread type */
    bool m_is_permanent; /* Is this a fixed thread type */
    unsigned
        m_default_count; /* Default count of threads created implicitly (ignored
                            if thread type set in threadConfig string) */
  };
};

#undef JAM_FILE_ID

#endif
