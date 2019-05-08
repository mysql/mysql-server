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

#ifndef THRMAN_H
#define THRMAN_H

#include <SimulatedBlock.hpp>
#include <LocalProxy.hpp>
#include <NdbGetRUsage.h>
#include <NdbTick.h>
#include <mt.hpp>

#define JAM_FILE_ID 340

//#define DEBUG_CPU_USAGE 1
class Thrman : public SimulatedBlock
{
public:
  Thrman(Block_context& ctx, Uint32 instanceNumber = 0);
  virtual ~Thrman();
  BLOCK_DEFINES(Thrman);

  void execDBINFO_SCANREQ(Signal*);
  void execCONTINUEB(Signal*);
  void execGET_CPU_USAGE_REQ(Signal*);
  void execOVERLOAD_STATUS_REP(Signal*);
  void execNODE_OVERLOAD_STATUS_ORD(Signal*);
  void execREAD_CONFIG_REQ(Signal*);
  void execSEND_THREAD_STATUS_REP(Signal*);
  void execSET_WAKEUP_THREAD_ORD(Signal*);
  void execWAKEUP_THREAD_ORD(Signal*);
  void execSEND_WAKEUP_THREAD_ORD(Signal*);
  void execSTTOR(Signal*);
protected:

private:

  /* Private variables */
  Uint32 m_num_send_threads;
  Uint32 m_num_threads;
  Uint32 m_send_thread_percentage;
  Uint32 m_node_overload_level;

  const char *m_thread_name;
  const char *m_send_thread_name;
  const char *m_thread_description;
  const char *m_send_thread_description;

  struct ndb_rusage m_last_50ms_rusage;
  struct ndb_rusage m_last_1sec_rusage;
  struct ndb_rusage m_last_20sec_rusage;

  NDB_TICKS prev_50ms_tick;
  NDB_TICKS prev_1sec_tick;
  NDB_TICKS prev_20sec_tick;

  static const Uint32 ZCONTINUEB_MEASURE_CPU_USAGE = 1;
  static const Uint32 default_cpu_load = 95;

  struct MeasurementRecord
  {
    MeasurementRecord()
      : m_first_measure_done(false)
    {}

    /**
     * This represents one measurement and we collect the following
     * information:
     *
     * User time as reported by GetRUsage
     * Kernel time as reported by GetRUsage
     * Idle time as calculated by the above two
     *
     * Sleep time as reported by calls in thread itself
     * Send time as reported by calls in thread itself
     * Execution time as reported by calls in thread itself
     * Also time spent waiting for buffer full condition to
     * disappear.
     *
     * Elapsed time for this measurement
     */
    Uint64 m_user_time_os;
    Uint64 m_kernel_time_os;
    Uint64 m_idle_time_os;

    Uint64 m_exec_time_thread;
    Uint64 m_spin_time_thread;
    Uint64 m_sleep_time_thread;
    Uint64 m_send_time_thread;
    Uint64 m_buffer_full_time_thread;

    Uint64 m_elapsed_time;
    bool m_first_measure_done;

    union {
      Uint32 nextPool;
      Uint32 nextList;
    };
    Uint32 prevList;
  };
  typedef Ptr<MeasurementRecord> MeasurementRecordPtr;
  typedef ArrayPool<MeasurementRecord> MeasurementRecord_pool;
  typedef DLCFifoList<MeasurementRecord_pool> MeasurementRecord_fifo;

  MeasurementRecord_pool c_measurementRecordPool;

  MeasurementRecord_fifo c_next_50ms_measure;
  MeasurementRecord_fifo c_next_1sec_measure;
  MeasurementRecord_fifo c_next_20sec_measure;

  MeasurementRecord m_last_50ms_base_measure;
  MeasurementRecord m_last_1sec_base_measure;
  MeasurementRecord m_last_20sec_base_measure;

  struct SendThreadMeasurement
  {
    SendThreadMeasurement()
      : m_first_measure_done(false)
    {}

    bool m_first_measure_done;
    Uint64 m_elapsed_time;
    Uint64 m_exec_time;
    Uint64 m_spin_time;
    Uint64 m_sleep_time;
    Uint64 m_user_time_os;
    Uint64 m_kernel_time_os;
    Uint64 m_idle_time_os;
    Uint64 m_elapsed_time_os;
    union {
      Uint32 nextPool;
      Uint32 nextList;
    };
    Uint32 prevList;
  };
  typedef Ptr<SendThreadMeasurement> SendThreadMeasurementPtr;
  typedef ArrayPool<SendThreadMeasurement> SendThreadMeasurement_pool;
  typedef DLCFifoList<SendThreadMeasurement_pool> SendThreadMeasurement_fifo;
  typedef LocalDLCFifoList<SendThreadMeasurement_pool>
                               Local_SendThreadMeasurement_fifo;

  SendThreadMeasurement_pool c_sendThreadMeasurementPool;

  struct SendThreadRecord
  {
    SendThreadMeasurement m_last_50ms_send_thread_measure;
    SendThreadMeasurement m_last_1sec_send_thread_measure;
    SendThreadMeasurement m_last_20sec_send_thread_measure;

    SendThreadMeasurement_fifo::Head m_send_thread_50ms_measurements;
    SendThreadMeasurement_fifo::Head m_send_thread_1sec_measurements;
    SendThreadMeasurement_fifo::Head m_send_thread_20sec_measurements;

    union {
      Uint32 nextPool;
      Uint32 nextList;
    };
  };
  typedef Ptr<SendThreadRecord> SendThreadPtr;
  typedef ArrayPool<SendThreadRecord> SendThreadRecord_pool;

  SendThreadRecord_pool c_sendThreadRecordPool;

  struct MeasureStats
  {
    Uint64 min_os_percentage;
    Uint64 min_next_os_percentage;
    Uint64 max_os_percentage;
    Uint64 max_next_os_percentage;
    Uint64 avg_os_percentage;

    Uint64 min_thread_percentage;
    Uint64 min_next_thread_percentage;
    Uint64 max_thread_percentage;
    Uint64 max_next_thread_percentage;
    Uint64 avg_thread_percentage;

    Uint64 avg_send_percentage;
  };
  /* Private variables used for handling overload control */
  bool m_shared_environment;
  bool m_overload_handling_activated;
  Int32 m_warning_level;
  Uint32 m_max_warning_level;
  Uint32 m_burstiness;
  OverloadStatus m_current_overload_status;

  struct ThreadOverloadStatus
  {
    OverloadStatus overload_status;
    Uint32 wakeup_instance;
  };

  ThreadOverloadStatus m_thread_overload_status[MAX_BLOCK_THREADS + 1];

  MeasureStats c_1sec_stats;
  MeasureStats c_20sec_stats;
  MeasureStats c_400sec_stats;
  MeasureStats *m_current_decision_stats;

  /* Private methods */
  void sendSTTORRY(Signal*);
  void sendNextCONTINUEB(Signal*);
  void measure_cpu_usage(Signal*);
  void mark_measurements_not_done();
  void check_overload_status(Signal*, bool, bool);

  Uint32 calculate_mean_send_thread_load();
  void calculate_measurement(MeasurementRecordPtr measurePtr,
                             struct ndb_rusage *curr_rusage,
                             struct ndb_rusage *base_rusage,
                             MeasurementRecord *curr_measure,
                             MeasurementRecord *base_measure,
                             Uint64 elapsed_micros);

  void calculate_send_measurement(
    SendThreadMeasurementPtr sendThreadMeasurementPtr,
    SendThreadMeasurement *curr_send_thread_measure,
    SendThreadMeasurement *last_send_thread_measure,
    Uint64 elapsed_time,
    Uint32 send_instance);

  void sum_measures(MeasurementRecord *dest, MeasurementRecord *source);
  void calc_stats(MeasureStats *stats, MeasurementRecord *measure);
  void calc_avgs(MeasureStats *stats, Uint32 num_stats);
  void init_stats(MeasureStats *stats);

  void handle_decisions();
  void check_burstiness();

  void inc_warning(Uint32 inc_factor);
  void dec_warning(Uint32 dec_factor);
  void down_warning(Uint32 down_factor);

  Int32 get_load_status(Uint32 load, Uint32 send_load);
  Uint32 calculate_load(MeasureStats & stats, Uint32 & burstiness);
  void change_warning_level(Int32 diff_status, Uint32 factor);
  void handle_overload_stats_1sec();
  void handle_overload_stats_20sec();
  void handle_overload_stats_400sec();

  void handle_state_change(Signal *signal);
  void sendOVERLOAD_STATUS_REP(Signal *signal);
  void sendSEND_THREAD_STATUS_REP(Signal *signal, Uint32 send_pct);
  void sendSET_WAKEUP_THREAD_ORD(Signal *signal,
                                 Uint32 instance_no,
                                 Uint32 wakeup_instance);
  void get_idle_block_threads(Uint32 *thread_list,
                              Uint32 & num_threads_found);
  void assign_wakeup_threads(Signal*, Uint32*, Uint32);
  void update_current_wakeup_instance(Uint32 * threads_list,
                                      Uint32 num_threads_found,
                                      Uint32 & index,
                                      Uint32 & current_wakeup_instance);

  bool calculate_stats_last_400seconds(MeasureStats *stats);
  bool calculate_stats_last_20seconds(MeasureStats *stats);
  bool calculate_stats_last_second(MeasureStats *stats);
  bool calculate_stats_last_100ms(MeasureStats *stats);

  bool calculate_cpu_load_last_second(MeasurementRecord *measure);
  bool calculate_cpu_load_last_20seconds(MeasurementRecord *measure);
  bool calculate_cpu_load_last_400seconds(MeasurementRecord *measure);

  bool calculate_send_thread_load_last_second(Uint32 send_instance,
                                              SendThreadMeasurement *measure);
};

class ThrmanProxy : public LocalProxy
{
public:
  ThrmanProxy(Block_context& ctx);
  virtual ~ThrmanProxy();
  BLOCK_DEFINES(ThrmanProxy);

protected:
  virtual SimulatedBlock* newWorker(Uint32 instanceNo);

};


#undef JAM_FILE_ID

#endif
