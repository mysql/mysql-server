/*
   Copyright (c) 2011, 2024, Oracle and/or its affiliates.

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

#include "thrman.hpp"
#include <mt.hpp>
#include <signaldata/DbinfoScan.hpp>
#include <signaldata/DumpStateOrd.hpp>
#include <signaldata/LoadOrd.hpp>
#include <signaldata/Sync.hpp>

#include <NdbSpin.h>
#include <EventLogger.hpp>
#include <NdbHW.hpp>

#define JAM_FILE_ID 440

#define NUM_MEASUREMENTS 20
#define NUM_MEASUREMENT_RECORDS (3 * NUM_MEASUREMENTS)

static NdbMutex *g_freeze_mutex = 0;
static NdbCondition *g_freeze_condition = 0;
static Uint32 g_freeze_waiters = 0;
static bool g_freeze_wakeup = 0;

//#define DEBUG_SPIN 1
//#define DEBUG_SCHED_WEIGHTS 1

#ifdef DEBUG_SPIN
#define DEB_SPIN(arglist)        \
  do {                           \
    g_eventLogger->info arglist; \
  } while (0)
#else
#define DEB_SPIN(arglist) \
  do {                    \
  } while (0)
#endif

#ifdef DEBUG_SCHED_WEIGHTS
#define DEB_SCHED_WEIGHTS(arglist) \
  do {                             \
    g_eventLogger->info arglist;   \
  } while (0)
#else
#define DEB_SCHED_WEIGHTS(arglist) \
  do {                             \
  } while (0)
#endif

// define HIGH_DEBUG_CPU_USAGE 1
//#define DEBUG_CPU_USAGE 1

Thrman::Thrman(Block_context &ctx, Uint32 instanceno)
    : SimulatedBlock(THRMAN, ctx, instanceno),
      c_next_50ms_measure(c_measurementRecordPool),
      c_next_1sec_measure(c_measurementRecordPool),
      c_next_20sec_measure(c_measurementRecordPool) {
  BLOCK_CONSTRUCTOR(Thrman);

  if (g_freeze_mutex == 0) {
    g_freeze_mutex = NdbMutex_Create();
    g_freeze_condition = NdbCondition_Create();
  }
  addRecSignal(GSN_DBINFO_SCANREQ, &Thrman::execDBINFO_SCANREQ);
  addRecSignal(GSN_CONTINUEB, &Thrman::execCONTINUEB);
  addRecSignal(GSN_GET_CPU_USAGE_REQ, &Thrman::execGET_CPU_USAGE_REQ);
  addRecSignal(GSN_OVERLOAD_STATUS_REP, &Thrman::execOVERLOAD_STATUS_REP);
  addRecSignal(GSN_NODE_OVERLOAD_STATUS_ORD,
               &Thrman::execNODE_OVERLOAD_STATUS_ORD);
  addRecSignal(GSN_READ_CONFIG_REQ, &Thrman::execREAD_CONFIG_REQ);
  addRecSignal(GSN_SEND_THREAD_STATUS_REP, &Thrman::execSEND_THREAD_STATUS_REP);
  addRecSignal(GSN_SET_WAKEUP_THREAD_ORD, &Thrman::execSET_WAKEUP_THREAD_ORD);
  addRecSignal(GSN_WAKEUP_THREAD_ORD, &Thrman::execWAKEUP_THREAD_ORD);
  addRecSignal(GSN_SEND_WAKEUP_THREAD_ORD, &Thrman::execSEND_WAKEUP_THREAD_ORD);
  addRecSignal(GSN_FREEZE_THREAD_REQ, &Thrman::execFREEZE_THREAD_REQ);
  addRecSignal(GSN_FREEZE_ACTION_CONF, &Thrman::execFREEZE_ACTION_CONF);
  addRecSignal(GSN_STTOR, &Thrman::execSTTOR);
  addRecSignal(GSN_MEASURE_WAKEUP_TIME_ORD,
               &Thrman::execMEASURE_WAKEUP_TIME_ORD);
  addRecSignal(GSN_DUMP_STATE_ORD, &Thrman::execDUMP_STATE_ORD);
  addRecSignal(GSN_UPD_THR_LOAD_ORD, &Thrman::execUPD_THR_LOAD_ORD);

  m_enable_adaptive_spinning = false;
  m_allowed_spin_overhead = 130;
  m_phase2_done = false;
  m_is_idle = true;
  if (!isNdbMtLqh()) {
    jam();
    m_rep_thrman_instance = 0;
  } else if (globalData.ndbMtMainThreads == 2) {
    jam();
    m_rep_thrman_instance = 2;
  } else if (globalData.ndbMtMainThreads == 1) {
    jam();
    m_rep_thrman_instance = 1;
  } else {
    jam();
    /* Main and rep threads are handled by first receive thread */
    m_rep_thrman_instance =
        1 + globalData.ndbMtLqhThreads + globalData.ndbMtTcThreads +
        globalData.ndbMtQueryThreads + globalData.ndbMtRecoverThreads;
  }
}

Thrman::~Thrman() {
  if (g_freeze_mutex != 0) {
    NdbMutex_Destroy(g_freeze_mutex);
    NdbCondition_Destroy(g_freeze_condition);
    g_freeze_mutex = 0;
    g_freeze_condition = 0;
    g_freeze_waiters = 0;
    g_freeze_wakeup = false;
  }
}

BLOCK_FUNCTIONS(Thrman)

void Thrman::mark_measurements_not_done() {
  MeasurementRecordPtr measurePtr;
  jam();
  c_next_50ms_measure.first(measurePtr);
  while (measurePtr.i != RNIL) {
    measurePtr.p->m_first_measure_done = false;
    c_next_50ms_measure.next(measurePtr);
  }
  c_next_1sec_measure.first(measurePtr);
  while (measurePtr.i != RNIL) {
    measurePtr.p->m_first_measure_done = false;
    c_next_1sec_measure.next(measurePtr);
  }
  c_next_20sec_measure.first(measurePtr);
  while (measurePtr.i != RNIL) {
    measurePtr.p->m_first_measure_done = false;
    c_next_20sec_measure.next(measurePtr);
  }
}

void Thrman::set_configured_spintime(Uint32 val, bool specific) {
  if (!NdbSpin_is_supported()) {
    return;
  }
  if (val > MAX_SPIN_TIME) {
    if (specific || instance() == m_main_thrman_instance) {
      g_eventLogger->info("(%u)Attempt to set spintime > 500 not possible",
                          instance());
    }
    return;
  }
  g_eventLogger->info("(%u)Setting spintime to %u", instance(), val);

  m_configured_spintime = val;
  if (val == 0) {
    jam();
    setSpintime(val);
    return;
  } else if (!m_enable_adaptive_spinning) {
    jam();
    setSpintime(val);
  }
}

void Thrman::set_allowed_spin_overhead(Uint32 val) {
  if (val > MAX_SPIN_OVERHEAD) {
    if (instance() == m_main_thrman_instance) {
      g_eventLogger->info("AllowedSpinOverhead is max 10000");
    }
    return;
  }
  Uint32 add_val = 0;
  if (val > 100) {
    add_val = val - 100;
    val = 100;
  }
  /**
   * At low allowed spin overhead it makes more sense to spend time
   * spinning in recv thread since we have many more wakeups that can
   * gain from spinning in this thread.
   *
   * As we increase the allowed spin overhead we will have more and more
   * benefits of spinning also in TC threads.
   *
   * At very high allowed overhead it becomes essential to also grab the
   * wait states in the LDM threads and thus give back the allowed
   * overhead to them.
   */
  if (m_recv_thread) {
    jam();
    val *= 3;
    val /= 2;
    add_val *= 8;
    add_val /= 10;
    m_allowed_spin_overhead = val + add_val + 150;
  } else if (m_tc_thread) {
    jam();
    add_val *= 9;
    add_val /= 10;
    m_allowed_spin_overhead = val + add_val + 140;
  } else if (m_ldm_thread) {
    jam();
    val *= 2;
    val /= 3;
    add_val *= 12;
    add_val /= 10;
    m_allowed_spin_overhead = val + add_val + 120;
  } else {
    jam();
    m_allowed_spin_overhead = val + 130;
  }
  g_eventLogger->debug("(%u) Setting AllowedSpinOverhead to %u", instance(),
                       m_allowed_spin_overhead);
}

void Thrman::set_enable_adaptive_spinning(bool val) {
  m_enable_adaptive_spinning = val;
  setSpintime(m_configured_spintime);
  if (instance() == m_main_thrman_instance) {
    g_eventLogger->info("(%u) %s adaptive spinning", instance(),
                        val ? "Enable" : "Disable");
  }
}

void Thrman::set_spintime_per_call(Uint32 val) {
  if (instance() == m_main_thrman_instance) {
    if (val < MIN_SPINTIME_PER_CALL || val > MAX_SPINTIME_PER_CALL) {
      g_eventLogger->info(
          "SpintimePerCall can only be set between"
          " 300 and 8000");
      return;
    }
    NdbSpin_Change(val);
    g_eventLogger->info("SpintimePerCall set to %u", val);
  }
}

void Thrman::execREAD_CONFIG_REQ(Signal *signal) {
  jamEntry();

  /* Receive signal */
  const ReadConfigReq *req = (ReadConfigReq *)signal->getDataPtr();
  Uint32 ref = req->senderRef;
  Uint32 senderData = req->senderData;

  m_thread_name = getThreadName();
  m_recv_thread = false;
  m_ldm_thread = false;
  m_tc_thread = false;
  m_spin_time_change_count = 0;
  if (strcmp(m_thread_name, "recv") == 0) {
    m_recv_thread = true;
  }
  if (strcmp(m_thread_name, "tc") == 0) {
    m_tc_thread = true;
  }
  if (strcmp(m_thread_name, "ldm") == 0) {
    m_ldm_thread = true;
  }
  m_thread_description = getThreadDescription();
  m_send_thread_name = "send";
  m_send_thread_description = "Send thread";
  m_enable_adaptive_spinning = false;
  m_main_thrman_instance = getMainThrmanInstance();

  if (NdbSpin_is_supported()) {
    const char *conf = 0;
    Uint32 val = 0;
    const ndb_mgm_configuration_iterator *p =
        m_ctx.m_config.getOwnConfigIterator();
    ndbrequire(p != 0);
    if (!ndb_mgm_get_string_parameter(p, CFG_DB_SPIN_METHOD, &conf)) {
      jam();
      if (native_strcasecmp(conf, "staticspinning") == 0) {
        if (instance() == m_main_thrman_instance) {
          g_eventLogger->info(
              "Using StaticSpinning according to spintime"
              " configuration");
        }
      } else if (native_strcasecmp(conf, "costbasedspinning") == 0) {
        if (instance() == m_main_thrman_instance) {
          g_eventLogger->info(
              "Using CostBasedSpinning with max spintime = 100"
              " and allowed spin overhead 70 percent");
        }
        val = 200;
        m_enable_adaptive_spinning = true;
        m_configured_spintime = 100;
      } else if (native_strcasecmp(conf, "latencyoptimisedspinning") == 0) {
        if (instance() == m_main_thrman_instance) {
          g_eventLogger->info(
              "Using LatencyOptimisedSpinning with max"
              " spintime = 200 and allowed spin"
              " overhead 1000 percent");
        }
        val = 1000;
        m_enable_adaptive_spinning = true;
        m_configured_spintime = 200;
      } else if (native_strcasecmp(conf, "databasemachinespinning") == 0) {
        if (instance() == m_main_thrman_instance) {
          g_eventLogger->info(
              "Using DatabaseMachineSpinning with max"
              " spintime = 500 and"
              " allowed spin overhead 10000 percent");
        }
        val = 10000;
        m_enable_adaptive_spinning = true;
        m_configured_spintime = MAX_SPIN_TIME;
      } else {
        g_eventLogger->info(
            "SpinMethod set to %s, ignored this use either "
            "StaticSpinning, CostBasedSpinning, "
            "AggressiveSpinning or DatabaseMachineSpinning"
            ", falling back to default StaticSpinning",
            conf);
      }
    } else {
      m_enable_adaptive_spinning = false;
    }
    /**
     * A spin overhead of 0% means that we will spin if it costs 30% more CPU
     * to gain the earned latency. For example if we by spinning 1300 us can
     * gain 1000 us in latency we will always treat this as something we
     * consider as no overhead at all. The reason is that we while spinning
     * don't use the CPU at full speed, thus other hyperthreads or other CPU
     * cores will have more access to CPU core parts and to the memory
     * subsystem in the CPU.
     * By default we will even spend an extra 70% of CPU overhead to gain
     * the desired latency gains.
     *
     * Most of the long work is done by the LDM threads. These threads work
     * for a longer time. The receive thread and the TC threads usually
     * handle small execution times, but very many of them. This means
     * that for spinning it is more useful to spin on the recv threads
     * and on the tc threads.
     *
     * What this means is that most of the overhead that the user have
     * configured will be used for spinning in the recv thread and tc
     * threads.
     *
     * High overhead we treat a bit different. Since most gain for small
     * overhead comes from receive thread and tc thread, the gain with
     * high overhead instead comes from the ldm thread. So we give the
     * ldm thread higher weight for high overhead values. The highest
     * overhead configurable is 800 and will give the allowed spin overhead
     * for recv thread to be 860, for tc thread it will 870 and for
     * the ldm thread it will be 1010.
     *
     * This high overhead I will refer to as the database machine mode.
     * It means that we expect the OS to not have to be involved in the
     * database thread operation, thus running more or less 100% load
     * even at low concurrency is ok, this mode also requires setting
     * the SchedulerSpinTimer to its maximum value 500.
     */
    set_allowed_spin_overhead(val);
  }

  /**
   * Allocate the 60 records needed for 3 lists with 20 measurements in each
   * list. We keep track of the last second with high resolution of 50 millis
   * between each measurement, we also keep track of longer back for 20
   * seconds where we have one measurement per second and finally we also
   * keep track of long-term statistics going back more than 6 minutes.
   * We could go back longer or have a higher resolution, but at the moment
   * it seems a bit unnecessary. We could go back further if we are going to
   * implement approaches more based on statistics and also finding patterns
   * of change.
   */
  m_num_send_threads = getNumSendThreads();
  m_num_threads = getNumThreads();

  c_measurementRecordPool.setSize(NUM_MEASUREMENT_RECORDS);
  if (instance() == m_main_thrman_instance) {
    jam();
    c_sendThreadRecordPool.setSize(m_num_send_threads);
    c_sendThreadMeasurementPool.setSize(NUM_MEASUREMENT_RECORDS *
                                        m_num_send_threads);
    struct ndb_hwinfo *hwinfo = Ndb_GetHWInfo(false);
    m_is_cpuinfo_available = hwinfo->is_cpuinfo_available;
    m_is_cpudata_available = hwinfo->is_cpudata_available;
    if (!m_is_cpudata_available) {
      jam();
      c_CPURecordPool.setSize(0);
      c_CPUMeasurementRecordPool.setSize(0);
    } else {
      jam();
      Uint32 cpu_count = hwinfo->cpu_cnt_max;
      c_CPURecordPool.setSize(cpu_count);
      /**
       * We need one list of 20 records for each CPU and there are
       * 3 lists, 50ms list, 1sec list and 20sec list.
       */
      c_CPUMeasurementRecordPool.setSize(cpu_count * NUM_MEASUREMENTS * 3);
      for (Uint32 cpu_no = 0; cpu_no < cpu_count; cpu_no++) {
        jam();
        CPURecordPtr cpuPtr;
        ndbrequire(c_CPURecordPool.seizeId(cpuPtr, cpu_no));
        jam();
        cpuPtr.p = new (cpuPtr.p) CPURecord();
        ndbrequire(cpuPtr.i == cpu_no);
        cpuPtr.p->m_cpu_no = cpu_no;
        for (Uint32 i = 0; i < NUM_MEASUREMENTS; i++) {
          jam();
          {
            LocalDLCFifoList<CPUMeasurementRecord_pool> list(
                c_CPUMeasurementRecordPool, cpuPtr.p->m_next_50ms_measure);
            CPUMeasurementRecordPtr cpuMeasurePtr;
            ndbrequire(c_CPUMeasurementRecordPool.seize(cpuMeasurePtr));
            jam();
            cpuMeasurePtr.p = new (cpuMeasurePtr.p) CPUMeasurementRecord();
            list.addFirst(cpuMeasurePtr);
            jam();
          }
          {
            LocalDLCFifoList<CPUMeasurementRecord_pool> list(
                c_CPUMeasurementRecordPool, cpuPtr.p->m_next_1sec_measure);
            CPUMeasurementRecordPtr cpuMeasurePtr;
            ndbrequire(c_CPUMeasurementRecordPool.seize(cpuMeasurePtr));
            cpuMeasurePtr.p = new (cpuMeasurePtr.p) CPUMeasurementRecord();
            list.addFirst(cpuMeasurePtr);
          }
          {
            LocalDLCFifoList<CPUMeasurementRecord_pool> list(
                c_CPUMeasurementRecordPool, cpuPtr.p->m_next_20sec_measure);
            CPUMeasurementRecordPtr cpuMeasurePtr;
            ndbrequire(c_CPUMeasurementRecordPool.seize(cpuMeasurePtr));
            cpuMeasurePtr.p = new (cpuMeasurePtr.p) CPUMeasurementRecord();
            list.addFirst(cpuMeasurePtr);
          }
        }
      }
    }
  } else {
    jam();
    c_CPURecordPool.setSize(0);
    m_is_cpuinfo_available = false;
    m_is_cpudata_available = false;
    c_CPUMeasurementRecordPool.setSize(0);
    c_sendThreadRecordPool.setSize(0);
    c_sendThreadMeasurementPool.setSize(0);
  }

  /* Create the 3 lists with 20 records in each. */
  MeasurementRecordPtr measurePtr;
  for (Uint32 i = 0; i < NUM_MEASUREMENTS; i++) {
    jam();
    ndbrequire(c_measurementRecordPool.seize(measurePtr));
    measurePtr.p = new (measurePtr.p) MeasurementRecord();
    c_next_50ms_measure.addFirst(measurePtr);
    ndbrequire(c_measurementRecordPool.seize(measurePtr));
    measurePtr.p = new (measurePtr.p) MeasurementRecord();
    c_next_1sec_measure.addFirst(measurePtr);
    ndbrequire(c_measurementRecordPool.seize(measurePtr));
    measurePtr.p = new (measurePtr.p) MeasurementRecord();
    c_next_20sec_measure.addFirst(measurePtr);
  }
  if (instance() == m_main_thrman_instance) {
    jam();
    for (Uint32 send_instance = 0; send_instance < m_num_send_threads;
         send_instance++) {
      jam();
      SendThreadPtr sendThreadPtr;
      ndbrequire(c_sendThreadRecordPool.seizeId(sendThreadPtr, send_instance));
      sendThreadPtr.p = new (sendThreadPtr.p) SendThreadRecord();
      sendThreadPtr.p->m_send_thread_50ms_measurements.init();
      sendThreadPtr.p->m_send_thread_1sec_measurements.init();
      sendThreadPtr.p->m_send_thread_20sec_measurements.init();

      for (Uint32 i = 0; i < NUM_MEASUREMENTS; i++) {
        jam();
        SendThreadMeasurementPtr sendThreadMeasurementPtr;

        ndbrequire(c_sendThreadMeasurementPool.seize(sendThreadMeasurementPtr));
        sendThreadMeasurementPtr.p =
            new (sendThreadMeasurementPtr.p) SendThreadMeasurement();
        {
          jam();
          Local_SendThreadMeasurement_fifo list_50ms(
              c_sendThreadMeasurementPool,
              sendThreadPtr.p->m_send_thread_50ms_measurements);
          list_50ms.addFirst(sendThreadMeasurementPtr);
        }

        ndbrequire(c_sendThreadMeasurementPool.seize(sendThreadMeasurementPtr));
        sendThreadMeasurementPtr.p =
            new (sendThreadMeasurementPtr.p) SendThreadMeasurement();
        {
          jam();
          Local_SendThreadMeasurement_fifo list_1sec(
              c_sendThreadMeasurementPool,
              sendThreadPtr.p->m_send_thread_1sec_measurements);
          list_1sec.addFirst(sendThreadMeasurementPtr);
        }

        ndbrequire(c_sendThreadMeasurementPool.seize(sendThreadMeasurementPtr));
        sendThreadMeasurementPtr.p =
            new (sendThreadMeasurementPtr.p) SendThreadMeasurement();
        {
          jam();
          Local_SendThreadMeasurement_fifo list_20sec(
              c_sendThreadMeasurementPool,
              sendThreadPtr.p->m_send_thread_20sec_measurements);
          list_20sec.addFirst(sendThreadMeasurementPtr);
        }
      }
    }
  }

  mark_measurements_not_done();
  /* Send return signal */
  ReadConfigConf *conf = (ReadConfigConf *)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(ref, GSN_READ_CONFIG_CONF, signal, ReadConfigConf::SignalLength,
             JBB);
}

void Thrman::execSTTOR(Signal *signal) {
  int res;
  jamEntry();

  const Uint32 startPhase = signal->theData[1];

  switch (startPhase) {
    case 1:
      jam();
      memset(&m_last_50ms_base_measure, 0, sizeof(m_last_50ms_base_measure));
      memset(&m_last_1sec_base_measure, 0, sizeof(m_last_1sec_base_measure));
      memset(&m_last_20sec_base_measure, 0, sizeof(m_last_20sec_base_measure));
      memset(&m_last_50ms_base_measure, 0, sizeof(m_last_50ms_rusage));
      memset(&m_last_1sec_base_measure, 0, sizeof(m_last_1sec_rusage));
      memset(&m_last_20sec_base_measure, 0, sizeof(m_last_20sec_rusage));
      prev_50ms_tick = NdbTick_getCurrentTicks();
      prev_20sec_tick = prev_50ms_tick;
      prev_1sec_tick = prev_50ms_tick;
      m_configured_spintime = getConfiguredSpintime();
      m_current_spintime = 0;
      m_gain_spintime_in_us = 25;

      /* Initialise overload control variables */
      m_shared_environment = false;
      m_overload_handling_activated = false;
      m_current_overload_status = (OverloadStatus)LIGHT_LOAD_CONST;
      m_warning_level = 0;
      m_max_warning_level = 20;
      m_burstiness = 0;
      m_current_decision_stats = &c_1sec_stats;
      m_send_thread_percentage = 0;
      m_node_overload_level = 0;

      for (Uint32 i = 0; i < MAX_BLOCK_THREADS + 1; i++) {
        m_thread_overload_status[i].overload_status =
            (OverloadStatus)MEDIUM_LOAD_CONST;
        m_thread_overload_status[i].wakeup_instance = 0;
      }

      /* Initialise measurements */
      res = Ndb_GetRUsage(&m_last_50ms_rusage, false);
      if (res == 0) {
        jam();
        m_last_1sec_rusage = m_last_50ms_rusage;
        m_last_20sec_rusage = m_last_50ms_rusage;
      }
      getPerformanceTimers(m_last_50ms_base_measure.m_sleep_time_thread,
                           m_last_50ms_base_measure.m_spin_time_thread,
                           m_last_50ms_base_measure.m_buffer_full_time_thread,
                           m_last_50ms_base_measure.m_send_time_thread);
      m_last_1sec_base_measure = m_last_50ms_base_measure;
      m_last_20sec_base_measure = m_last_50ms_base_measure;

      if (instance() == m_main_thrman_instance) {
        jam();
        for (Uint32 send_instance = 0; send_instance < m_num_send_threads;
             send_instance++) {
          jam();
          SendThreadPtr sendThreadPtr;
          ndbrequire(
              c_sendThreadRecordPool.getPtr(sendThreadPtr, send_instance));
          Uint64 send_exec_time;
          Uint64 send_sleep_time;
          Uint64 send_spin_time;
          Uint64 send_user_time_os;
          Uint64 send_kernel_time_os;
          Uint64 send_elapsed_time_os;
          getSendPerformanceTimers(
              send_instance, send_exec_time, send_sleep_time, send_spin_time,
              send_user_time_os, send_kernel_time_os, send_elapsed_time_os);

          sendThreadPtr.p->m_last_50ms_send_thread_measure.m_exec_time =
              send_exec_time;
          sendThreadPtr.p->m_last_50ms_send_thread_measure.m_sleep_time =
              send_sleep_time;
          sendThreadPtr.p->m_last_50ms_send_thread_measure.m_spin_time =
              send_spin_time;
          sendThreadPtr.p->m_last_50ms_send_thread_measure.m_user_time_os =
              send_user_time_os;
          sendThreadPtr.p->m_last_50ms_send_thread_measure.m_kernel_time_os =
              send_kernel_time_os;
          sendThreadPtr.p->m_last_50ms_send_thread_measure.m_elapsed_time_os =
              send_elapsed_time_os;

          sendThreadPtr.p->m_last_1sec_send_thread_measure.m_exec_time =
              send_exec_time;
          sendThreadPtr.p->m_last_1sec_send_thread_measure.m_sleep_time =
              send_sleep_time;
          sendThreadPtr.p->m_last_1sec_send_thread_measure.m_spin_time =
              send_spin_time;
          sendThreadPtr.p->m_last_1sec_send_thread_measure.m_user_time_os =
              send_user_time_os;
          sendThreadPtr.p->m_last_1sec_send_thread_measure.m_kernel_time_os =
              send_kernel_time_os;
          sendThreadPtr.p->m_last_1sec_send_thread_measure.m_elapsed_time_os =
              send_elapsed_time_os;

          sendThreadPtr.p->m_last_20sec_send_thread_measure.m_exec_time =
              send_exec_time;
          sendThreadPtr.p->m_last_20sec_send_thread_measure.m_sleep_time =
              send_sleep_time;
          sendThreadPtr.p->m_last_20sec_send_thread_measure.m_spin_time =
              send_spin_time;
          sendThreadPtr.p->m_last_20sec_send_thread_measure.m_user_time_os =
              send_user_time_os;
          sendThreadPtr.p->m_last_20sec_send_thread_measure.m_kernel_time_os =
              send_kernel_time_os;
          sendThreadPtr.p->m_last_20sec_send_thread_measure.m_elapsed_time_os =
              send_elapsed_time_os;
        }
      }
      if (instance() == m_main_thrman_instance) {
        if (getNumThreads() > 1 && NdbSpin_is_supported()) {
          jam();
          measure_wakeup_time(signal, 0);
        } else {
          jam();
          if (NdbSpin_is_supported()) {
            g_eventLogger->info(
                "Set wakeup latency to 25 microseconds in"
                " single thread environment");
          }
          setWakeupLatency(m_gain_spintime_in_us);
          sendSTTORRY(signal, false);
        }
        sendNextCONTINUEB(signal, 50, ZCONTINUEB_MEASURE_CPU_USAGE);
        sendNextCONTINUEB(signal, 10, ZCONTINUEB_CHECK_SPINTIME);
      } else {
        sendNextCONTINUEB(signal, 50, ZCONTINUEB_MEASURE_CPU_USAGE);
        sendNextCONTINUEB(signal, 10, ZCONTINUEB_CHECK_SPINTIME);
        sendSTTORRY(signal, false);
      }
      if (instance() == m_rep_thrman_instance &&
          globalData.ndbMtQueryThreads > 0) {
        jam();
        initial_query_distribution(signal);
      }
      return;
    case 2: {
      m_gain_spintime_in_us = getWakeupLatency();
      if (instance() == m_main_thrman_instance) {
        g_eventLogger->info("Set wakeup latency to %u microseconds",
                            m_gain_spintime_in_us);
      }
      set_spin_stat(0, true);
      sendSTTORRY(signal, true);
      return;
    }
    case 9: {
      if (instance() == m_rep_thrman_instance &&
          globalData.ndbMtQueryThreads > 0) {
        jam();
        signal->theData[0] = ZUPDATE_QUERY_DISTRIBUTION;
        sendSignal(reference(), GSN_CONTINUEB, signal, 1, JBB);
      }
      sendSTTORRY(signal, true);
      return;
    }
    default:
      ndbabort();
  }
}

#define NUM_WAKEUP_MEASUREMENTS 50
#define MAX_FAILED_WAKEUP_MEASUREMENTS 50
void Thrman::measure_wakeup_time(Signal *signal, Uint32 count) {
  NDB_TICKS now = NdbTick_getCurrentTicks();
  if (count != 0) {
    /* Perform measurement */
    Uint64 nanos_wait = NdbTick_Elapsed(m_measured_wait_time, now).nanoSec();
    DEB_SPIN(("Elapsed time was %llu nanoseconds", nanos_wait));
    if (nanos_wait < 100000 && nanos_wait != 0) {
      /* A proper measurement */
      m_tot_nanos_wait += nanos_wait;
      if (count == NUM_WAKEUP_MEASUREMENTS) {
        Uint64 mean_nanos_wait = m_tot_nanos_wait / NUM_WAKEUP_MEASUREMENTS;
        Uint64 mean_micros_wait = (mean_nanos_wait + 500) / 1000;
        m_gain_spintime_in_us = Uint32(mean_micros_wait);
        DEB_SPIN(("Set wakeup latency to %llu microseconds", mean_micros_wait));
        setWakeupLatency(m_gain_spintime_in_us);
        /**
         * We always start with no spinning and adjust to spinning when
         * activitity is started.
         */
        sendSTTORRY(signal, false);
        return;
      }
      count++;
    } else {
      m_failed_wakeup_measurements++;
      if (m_failed_wakeup_measurements >= MAX_FAILED_WAKEUP_MEASUREMENTS) {
        g_eventLogger->info("Failed to measure wakeup latency, using 25 us");
        sendSTTORRY(signal, false);
        return;
      }
    }
    do {
#ifdef NDB_HAVE_CPU_PAUSE
      for (Uint32 i = 0; i < 20; i++) {
        NdbSpin();
      }
#endif
      NDB_TICKS now2 = NdbTick_getCurrentTicks();
      Uint64 micros_wait = NdbTick_Elapsed(now, now2).microSec();
      if (micros_wait >= 50) {
        /**
         * We wait for 50 microseconds until next attempt to ensure
         * that the other thread has gone to sleep properly.
         */
        jam();
        break;
      }
    } while (1);
  } else {
    /**
     * Starting measurement, zero total to initialise and set spintime to
     * 1000 microseconds to ensure that we don't go to sleep until we have
     * completed these measurements that should take around a millisecond.
     */
    m_tot_nanos_wait = 0;
    setSpintime(1000);
    count++;
  }
  m_measured_wait_time = NdbTick_getCurrentTicks();
  BlockReference ref = numberToRef(THRMAN,
                                   m_main_thrman_instance + 1,  // rep thread
                                   getOwnNodeId());
  signal->theData[0] = count;
  signal->theData[1] = reference();
  /* Send measure signal from main thread to rep thread and back */
  sendSignal(ref, GSN_MEASURE_WAKEUP_TIME_ORD, signal, 2, JBB);
  return;
}

void Thrman::execMEASURE_WAKEUP_TIME_ORD(Signal *signal) {
  Uint32 count = signal->theData[0];
  BlockReference ref = signal->theData[1];
  if (instance() == m_main_thrman_instance) {
    measure_wakeup_time(signal, count);
    return;
  } else {
    /* Return signal immediately to sender */
    sendSignal(ref, GSN_MEASURE_WAKEUP_TIME_ORD, signal, 2, JBB);
  }
}

void Thrman::sendSTTORRY(Signal *signal, bool phase2_done) {
  m_phase2_done = phase2_done;
  signal->theData[0] = 0;
  signal->theData[1] = 3;
  signal->theData[2] = 0;
  signal->theData[3] = 1;
  signal->theData[4] = 2;
  signal->theData[5] = 9;
  signal->theData[6] = 255;  // No more start phases from missra
  BlockReference cntrRef = !isNdbMtLqh() ? NDBCNTR_REF : THRMAN_REF;
  sendSignal(cntrRef, GSN_STTORRY, signal, 7, JBB);
}

void Thrman::execCONTINUEB(Signal *signal) {
  jamEntry();
  Uint32 tcase = signal->theData[0];
  switch (tcase) {
    case ZUPDATE_QUERY_DISTRIBUTION: {
      jam();
      update_query_distribution(signal);
      signal->theData[0] = ZUPDATE_QUERY_DISTRIBUTION;
      sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 100, 1);
      break;
    }
    case ZCONTINUEB_MEASURE_CPU_DATA: {
      jam();
      measure_cpu_data(signal);
      break;
    }
    case ZCONTINUEB_MEASURE_CPU_USAGE: {
      jam();
      measure_cpu_usage(signal);
      sendNextCONTINUEB(signal, 50, ZCONTINUEB_MEASURE_CPU_USAGE);
      if (m_is_cpudata_available) {
        signal->theData[0] = ZCONTINUEB_MEASURE_CPU_DATA;
        sendSignal(reference(), GSN_CONTINUEB, signal, 1, JBB);
      }
      break;
    }
    case ZWAIT_ALL_STOP: {
      jam();
      wait_all_stop(signal);
      break;
    }
    case ZWAIT_ALL_START: {
      jam();
      wait_all_start(signal);
      break;
    }
    case ZCONTINUEB_CHECK_SPINTIME: {
      check_spintime(true);
      sendNextCONTINUEB(signal, 10, ZCONTINUEB_CHECK_SPINTIME);
      break;
    }
    default: {
      ndbabort();
    }
  }
}

void Thrman::sendNextCONTINUEB(Signal *signal, Uint32 delay, Uint32 type) {
  signal->theData[0] = type;
  sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, delay, 1);
}

void Thrman::update_current_wakeup_instance(Uint32 *thread_list,
                                            Uint32 num_threads_found,
                                            Uint32 &index,
                                            Uint32 &current_wakeup_instance) {
  index++;
  if (num_threads_found == index) {
    jam();
    index = 0;
  }
  current_wakeup_instance = thread_list[index];
}

/**
 * Each block thread has a thread assigned as its wakeup thread.
 * this thread is woken up to assist with sending data whenever
 * there is a need to quickly get things sent from the block
 * thread. Only block threads that are almost idle can be assigned
 * as wakeup threads.
 */
void Thrman::assign_wakeup_threads(Signal *signal, Uint32 *thread_list,
                                   Uint32 num_threads_found) {
  Uint32 index = 0;
  Uint32 instance_no;
  Uint32 current_wakeup_instance = thread_list[index];

  for (instance_no = 1; instance_no <= m_num_threads; instance_no++) {
    jam();
    if (m_thread_overload_status[instance_no].overload_status ==
        (OverloadStatus)OVERLOAD_CONST) {
      jam();
      /* Ensure that overloaded threads don't wakeup idle threads */
      current_wakeup_instance = 0;
    }

    /**
     * We don't wake ourselves up, other than that we attempt to wake up
     * the idle thread once per 200 microseconds from each thread.
     */
    if (instance_no == current_wakeup_instance) {
      if (num_threads_found > 1) {
        jam();
        update_current_wakeup_instance(thread_list, num_threads_found, index,
                                       current_wakeup_instance);
      } else {
        jam();
        current_wakeup_instance = 0;
      }
    }
    if (m_thread_overload_status[instance_no].wakeup_instance !=
        current_wakeup_instance) {
      jam();
      sendSET_WAKEUP_THREAD_ORD(signal, instance_no, current_wakeup_instance);
    }
    update_current_wakeup_instance(thread_list, num_threads_found, index,
                                   current_wakeup_instance);
  }
}

void Thrman::get_idle_block_threads(Uint32 *thread_list,
                                    Uint32 &num_threads_found) {
  /**
   * We never use more than 4 threads as idle threads. It's highly unlikely
   * that making use of more idle threads than this for sending is going to
   * be worthwhile. By starting the search from 1 we will always find the most
   * common idle threads, the main thread and the rep thread which are instance
   * 1 and 2.
   */
  Uint32 instance_no;
  for (instance_no = 1; instance_no <= m_num_threads; instance_no++) {
    if (m_thread_overload_status[instance_no].overload_status ==
        (OverloadStatus)LIGHT_LOAD_CONST) {
      thread_list[num_threads_found] = instance_no;
      num_threads_found++;
      if (num_threads_found == 4) return;
    }
  }
}

/**
 * Every time we decide to change the overload level we report this back to
 * the main thread that contains the global state.
 *
 * This signal is only executed by main thread.
 */
void Thrman::execOVERLOAD_STATUS_REP(Signal *signal) {
  Uint32 thr_no = signal->theData[0];
  Uint32 overload_status = signal->theData[1];
  ndbrequire(thr_no < NDB_ARRAY_SIZE(m_thread_overload_status));
  m_thread_overload_status[thr_no].overload_status =
      (OverloadStatus)overload_status;

  Uint32 node_overload_level = 0;
  for (Uint32 instance_no = 1; instance_no <= m_num_threads; instance_no++) {
    if (m_thread_overload_status[instance_no].overload_status >=
        (OverloadStatus)MEDIUM_LOAD_CONST) {
      node_overload_level = 1;
    }
  }
  if (node_overload_level == m_node_overload_level) {
    jam();
    m_node_overload_level = node_overload_level;
    signal->theData[0] = node_overload_level;
    for (Uint32 instance_no = 1; instance_no <= m_num_threads; instance_no++) {
      BlockReference ref = numberToRef(THRMAN, instance_no, getOwnNodeId());
      sendSignal(ref, GSN_NODE_OVERLOAD_STATUS_ORD, signal, 1, JBB);
    }
  }

  Uint32 num_threads_found = 0;
  Uint32 thread_list[4];
  get_idle_block_threads(thread_list, num_threads_found);
  if (num_threads_found == 0) {
    jam();
    /**
     * No idle threads found, so we make a list of one thread with
     * id 0 (which here means no thread). We still need to check
     * each thread to see if they need an update of the current
     * wakeup instance. So this means that all threads that currently
     * have a non-zero wakeup instance will receive an order to change
     * their wakeup instance to 0.
     */
    num_threads_found = 1;
    thread_list[0] = 0;
    return;
  }
  assign_wakeup_threads(signal, thread_list, num_threads_found);
  return;
}

void Thrman::execNODE_OVERLOAD_STATUS_ORD(Signal *signal) {
  jamEntry();
  Uint32 overload_status = signal->theData[0];
  setNodeOverloadStatus((OverloadStatus)overload_status);
}

void Thrman::execSEND_THREAD_STATUS_REP(Signal *signal) {
  jamEntry();
  m_send_thread_percentage = signal->theData[0];
  return;
}

void Thrman::execSEND_WAKEUP_THREAD_ORD(Signal *signal) {
  /**
   * This signal is sent directly from do_send in mt.cpp, it's
   * only purpose is to send a wakeup signal to another thread
   * to ensure that this thread is awake to execute some
   * send assistance to the send thread.
   */
  Uint32 wakeup_instance = signal->theData[0];
  BlockReference ref = numberToRef(THRMAN, wakeup_instance, getOwnNodeId());
  sendSignal(ref, GSN_WAKEUP_THREAD_ORD, signal, 1, JBA);
}

void Thrman::execWAKEUP_THREAD_ORD(Signal *signal) {
  /**
   * This signal is sent to wake the thread up. We're using the send signal
   * semantics to wake the thread up. So no need to execute anything, the
   * purpose of waking the thread has already been achieved when getting here.
   */
  return;
}
void Thrman::execSET_WAKEUP_THREAD_ORD(Signal *signal) {
  Uint32 wakeup_instance = signal->theData[0];
  setWakeupThread(wakeup_instance);
}

void Thrman::sendSET_WAKEUP_THREAD_ORD(Signal *signal, Uint32 instance_no,
                                       Uint32 wakeup_instance) {
  signal->theData[0] = wakeup_instance;
  BlockReference ref = numberToRef(THRMAN, instance_no, getOwnNodeId());
  sendSignal(ref, GSN_SET_WAKEUP_THREAD_ORD, signal, 1, JBB);
}

void Thrman::set_spin_stat(Uint32 spin_time, bool local_call) {
  ndbrequire(spin_time <= MAX_SPIN_TIME);
  struct ndb_spin_stat spin_stat;
  Uint32 used_spin_time = spin_time;
  setSpintime(spin_time);
  if (!local_call) {
    jam();
    return;
  }
  if (spin_time == 0) {
    /**
     * We set spin time to 0, but we use the measure spin time
     * in our measurements. This ensures that we quickly get on
     * track again with statistics when spin is enabled again.
     * We would not arrive here if configured spin time was 0 as
     * well.
     */
    used_spin_time = MEASURE_SPIN_TIME;
  }
  /**
   * We measure in steps of 50%, this gives us the possibility to
   * efficiently measure stepping up or stepping down spinning in
   * steps of 50% at a time.
   */
  Uint32 midpoint = (NUM_SPIN_INTERVALS / 2) - 1;
  for (Uint32 i = 0; i < NUM_SPIN_INTERVALS; i++) {
    Uint64 spin_time_limit = used_spin_time;
    if (i == (NUM_SPIN_INTERVALS - 1)) {
      spin_time_limit = UINT32_MAX;
    } else if (i < midpoint) {
      Uint64 mult_factor = 2;
      Uint64 div_factor = 3;
      for (Uint32 j = i + 1; j < midpoint; j++) {
        mult_factor *= 2;
        div_factor *= 3;
      }
      spin_time_limit = (mult_factor * used_spin_time) / div_factor;
    } else if (i > midpoint) {
      Uint64 mult_factor = 3;
      Uint64 div_factor = 2;
      for (Uint32 j = midpoint + 1; j < i; j++) {
        mult_factor *= 3;
        div_factor *= 2;
      }
      spin_time_limit = (mult_factor * used_spin_time) / div_factor;
    } else {
      ndbrequire(i == midpoint);
    }
    spin_stat.m_spin_interval[i] = Uint32(spin_time_limit);
  }
  mt_set_spin_stat(this, &spin_stat);
}

Uint32 Thrman::calc_new_spin(ndb_spin_stat *spin_stat) {
#ifdef DEBUG_SPIN
  Uint64 calc_spin_cost[NUM_SPIN_INTERVALS - 1];
  Uint64 calc_spin_overhead[NUM_SPIN_INTERVALS - 1];
  memset(calc_spin_cost, 0, sizeof(calc_spin_cost));
  memset(calc_spin_overhead, 0, sizeof(calc_spin_overhead));
#endif
  Uint32 num_events = spin_stat->m_num_waits;
  Uint32 remaining_events = num_events;

  Uint32 found = 0;
  Uint64 min_overhead = UINT64_MAX;
  for (Uint32 i = 0; i < (NUM_SPIN_INTERVALS - 1); i++) {
    Uint32 events_in_this_slot = spin_stat->m_micros_sleep_times[i];
    if (events_in_this_slot == 0 || spin_stat->m_spin_interval[i] == 0 ||
        spin_stat->m_spin_interval[i] > m_configured_spintime) {
      /**
       * Ignore empty slots, they will not be chosen for sure.
       * Also ignore slots where we measure 0 spin time.
       * Also ignore slots with higher spintime than what is
       * configured as maximum spintime.
       */
      continue;
    }
    /**
     * Calculate each slot as if it will become new spintime.
     */
    remaining_events -= events_in_this_slot;
    Uint32 num_gained_spins = num_events - remaining_events;

    Uint32 this_spin_cost = spin_stat->m_spin_interval[i];
    Uint64 gained_time_in_us =
        Uint64(num_gained_spins) * Uint64(m_gain_spintime_in_us);

    Uint64 spin_cost = 0;
    Uint32 avg_spin_cost = spin_stat->m_spin_interval[0] / 2;
    spin_cost += Uint64(avg_spin_cost * spin_stat->m_micros_sleep_times[0]);
    for (Uint32 j = 1; j <= i; j++) {
      Uint32 diff_time =
          spin_stat->m_spin_interval[j] - spin_stat->m_spin_interval[j - 1];
      diff_time /= 2;
      avg_spin_cost = diff_time + spin_stat->m_spin_interval[j - 1];
      spin_cost += Uint64(avg_spin_cost * spin_stat->m_micros_sleep_times[j]);
    }

    spin_cost += Uint64(this_spin_cost * remaining_events);
    ndbrequire(gained_time_in_us);
    Uint64 spin_overhead = Uint64(1000) * spin_cost / gained_time_in_us;
    spin_overhead += 5;
    spin_overhead /= 10;

    if (spin_overhead <= min_overhead || spin_overhead < Uint64(100) ||
        (spin_overhead < Uint64(130) && events_in_this_slot > 1)) {
      /**
       * This was the lowest overhead so far. Will be picked unless overhead
       * is too high. Will always be picked for i == 0.
       *
       * If there is a sufficient amount of events in this slot and we keep
       * the cost below 130, we will always pick this one.
       */
      min_overhead = spin_overhead;
      found = i;
    } else if (spin_overhead < Uint64(m_allowed_spin_overhead) &&
               events_in_this_slot > 1) {
      /**
       * This wasn't the lowest overhead so far. We will evaluate the
       * conditional probability of it paying off to continue from here since
       * we are still in the allowed range for allowed spin overhead.
       * Conditioned on the fact that we have to wait for at least the
       * already waited overhead.
       *
       * This means that we calculate the time estimated to continue spinning
       * before an event occurs based on that we know that we spent already
       * this time spinning (m_spin_interval[i - 1]). We know that i >= 1 here.
       *
       * The extra gain we get by continuing to spin until m_spin_interval[i] is
       * sum of gains from found + 1 to i. The extra cost is the added extra
       * spin time imposed on all remaining_events. The added cost is
       * m_spin_interval[i] - m_spin_interval[found].
       *
       * We will ignore this check if there is only a single event in this
       * slot. This represents a too high risk of spinning for too long if the
       * circumstances changes only slightly.
       */
      ndbrequire(i > 0);
      Uint64 extra_gain = 0;
      Uint64 extra_cost = 0;
      for (Uint32 j = found + 1; j <= i; j++) {
        Uint64 events_in_slot = Uint64(spin_stat->m_micros_sleep_times[j]);
        extra_gain += events_in_slot;
        Uint32 diff_time =
            spin_stat->m_spin_interval[j] - spin_stat->m_spin_interval[j - 1];
        diff_time /= 2;
        Uint64 avg_spin_cost =
            Uint64(diff_time) + Uint64(spin_stat->m_spin_interval[j - 1] -
                                       spin_stat->m_spin_interval[found]);
        extra_cost +=
            Uint64(avg_spin_cost * spin_stat->m_micros_sleep_times[j]);
      }
      extra_gain *= Uint64(m_gain_spintime_in_us);
      extra_gain *= Uint64(m_allowed_spin_overhead);
      extra_gain /= Uint64(100);
      extra_cost += Uint64(remaining_events) *
                    Uint64(this_spin_cost - spin_stat->m_spin_interval[found]);
      if (extra_gain > extra_cost) {
        found = i;
        min_overhead = spin_overhead;
      }
    }
#ifdef DEBUG_SPIN
    calc_spin_cost[i] = spin_cost;
    calc_spin_overhead[i] = spin_overhead;
#endif
  }
/**
 * When we are already spinning, we allow for a bit more overhead to avoid
 * jumping in and out of spinning too often. We need at least 4 observations
 * to make any judgement, only 2 events in 10ms doesn't seem to imply any
 * need of spinning.
 */
#define EXTRA_OVERHEAD_ALLOWED_WHEN_ALREADY_SPINNING 20
#define MIN_EVENTS_TO_BE_NOT_IDLE 20

  Uint32 midpoint = (NUM_SPIN_INTERVALS / 2) - 1;
  if (num_events <= 3 ||
      (min_overhead > Uint64(m_allowed_spin_overhead) &&
       (m_current_spintime == 0 ||
        min_overhead > Uint64(m_allowed_spin_overhead +
                              EXTRA_OVERHEAD_ALLOWED_WHEN_ALREADY_SPINNING)))) {
    /* Quickly shut down spin environment when no longer beneficial. */
    if (m_current_spintime != 0) {
      DEB_SPIN(("(%u)New spintime = 0", instance()));
    }
    m_current_spintime = 0;
  } else if (m_current_spintime == 0 ||
             m_current_spintime != spin_stat->m_spin_interval[midpoint]) {
    /**
     * Immediately adjust to new spin environment when activity starts up
     * from a more idle state. We also arrive here the next timeout
     * after a quick activation of spintime. In this case we have set
     * the spintime, but still haven't changed the spin intervals, so
     * set it directly to the found spintime.
     */
    m_current_spintime = spin_stat->m_spin_interval[found];
    DEB_SPIN(("(%u)New spintime = %u", instance(), m_current_spintime));
  } else {
    /**
     * When we are already spinning AND we want to continue spinning,
     * adjust change to not change the spin behaviour too fast. In this
     * case we are likely to be a in a more stable environment, so no
     * need of the very fast adaption to the environment.
     */
    if (found < midpoint) {
      m_current_spintime = spin_stat->m_spin_interval[midpoint - 1];
    } else if (found > midpoint) {
      m_current_spintime = spin_stat->m_spin_interval[midpoint + 1];
    }
    DEB_SPIN(("(%u)2:New spintime = %u", instance(), m_current_spintime));
  }
  if (num_events > MIN_EVENTS_TO_BE_NOT_IDLE) {
    jam();
    m_is_idle = false;
  } else {
    jam();
    m_is_idle = true;
  }
  /* Never select a spintime less than 2 microseconds. */
  if (m_current_spintime != 0 && m_current_spintime < 2) {
    m_current_spintime = 2;
  }
  /**
   * Never go beyond the configured spin time. The adaptive part can only
   * decrease the spinning, not increase it.
   */
  Uint32 max_spintime = m_configured_spintime;
  if (m_current_cpu_usage > 90) {
    jam();
    max_spintime /= 4;  // 25%
  } else if (m_current_cpu_usage > 80) {
    jam();
    max_spintime /= 3;  // 33%
  } else if (m_current_cpu_usage > 70) {
    jam();
    max_spintime *= 45;
    max_spintime /= 100;  // 45%
  } else if (m_current_cpu_usage > 60) {
    jam();
    max_spintime *= 60;
    max_spintime /= 100;  // 60%
  } else if (m_current_cpu_usage > 50) {
    jam();
    max_spintime *= 75;
    max_spintime /= 100;  // 75%
  } else if (m_current_cpu_usage > 40) {
    jam();
    max_spintime *= 90;
    max_spintime /= 100;  // 90%
  }

  if (m_current_spintime > max_spintime) {
    m_current_spintime = max_spintime;
  }
  if (num_events >= 3) {
    DEB_SPIN(
        ("(%u)SPIN events: %u, spintime selected: %u "
         ":ovh[0]=%llu,cost[0]=%llu"
         ":ovh[1]=%llu,cost[1]=%llu"
         ":ovh[2]=%llu,cost[2]=%llu"
         ":ovh[3]=%llu,cost[3]=%llu"
         ":ovh[4]=%llu,cost[4]=%llu"
         ":ovh[5]=%llu,cost[5]=%llu"
         ":ovh[6]=%llu,cost[6]=%llu"
         ":ovh[7]=%llu,cost[7]=%llu"
         ":ovh[8]=%llu,cost[8]=%llu"
         ":ovh[9]=%llu,cost[9]=%llu"
         ":ovh[10]=%llu,cost[10]=%llu"
         ":ovh[11]=%llu,cost[11]=%llu"
         ":ovh[12]=%llu,cost[12]=%llu"
         ":ovh[13]=%llu,cost[13]=%llu"
         ":ovh[14]=%llu,cost[14]=%llu",
         instance(), num_events, m_current_spintime, calc_spin_overhead[0],
         calc_spin_cost[0], calc_spin_overhead[1], calc_spin_cost[1],
         calc_spin_overhead[2], calc_spin_cost[2], calc_spin_overhead[3],
         calc_spin_cost[3], calc_spin_overhead[4], calc_spin_cost[4],
         calc_spin_overhead[5], calc_spin_cost[5], calc_spin_overhead[6],
         calc_spin_cost[6], calc_spin_overhead[7], calc_spin_cost[7],
         calc_spin_overhead[8], calc_spin_cost[8], calc_spin_overhead[9],
         calc_spin_cost[9], calc_spin_overhead[10], calc_spin_cost[10],
         calc_spin_overhead[11], calc_spin_cost[11], calc_spin_overhead[12],
         calc_spin_cost[12], calc_spin_overhead[13], calc_spin_cost[13],
         calc_spin_overhead[14], calc_spin_cost[14]));
  }
  return m_current_spintime;
}

void Thrman::check_spintime(bool local_call) {
  if (!m_phase2_done) {
    jam();
    return;
  }
  if (!local_call && !m_is_idle) {
    jam();
    return;
  }
  if (!m_enable_adaptive_spinning) {
    jam();
    return;
  }
  if (m_configured_spintime == 0) {
    /* No configured spinning on the thread, so ignore check of spin time. */
    jam();
    return;
  }
  struct ndb_spin_stat spin_stat;
  mt_get_spin_stat(this, &spin_stat);

  if (spin_stat.m_num_waits >= 3) {
    DEB_SPIN(
        ("(%u)m_sleep_longer_spin_time: %u, "
         "m_sleep_shorter_spin_time: %u"
         ", local_call: %s",
         instance(), spin_stat.m_sleep_longer_spin_time,
         spin_stat.m_sleep_shorter_spin_time, local_call ? "true" : "false"));
  }

  if (m_shared_environment) {
    /**
     * We never spin in a shared environment, this would cause even more
     * overload on the CPUs to happen.
     */
    set_spin_stat(0, local_call);
    return;
  }
  if (spin_stat.m_num_waits >= 3) {
    DEB_SPIN(
        ("(%u): <= %u: %u, <= %u: %u, <= %u: %u, <= %u: %u,"
         " <= %u: %u, <= %u: %u, <= %u: %u, <= %u: %u"
         " <= %u: %u, <= %u: %u, <= %u: %u, <= %u: %u"
         " <= %u: %u, <= %u: %u, <= %u: %u, <= MAX: %u",
         instance(), spin_stat.m_spin_interval[0],
         spin_stat.m_micros_sleep_times[0], spin_stat.m_spin_interval[1],
         spin_stat.m_micros_sleep_times[1], spin_stat.m_spin_interval[2],
         spin_stat.m_micros_sleep_times[2], spin_stat.m_spin_interval[3],
         spin_stat.m_micros_sleep_times[3], spin_stat.m_spin_interval[4],
         spin_stat.m_micros_sleep_times[4], spin_stat.m_spin_interval[5],
         spin_stat.m_micros_sleep_times[5], spin_stat.m_spin_interval[6],
         spin_stat.m_micros_sleep_times[6], spin_stat.m_spin_interval[7],
         spin_stat.m_micros_sleep_times[7], spin_stat.m_spin_interval[8],
         spin_stat.m_micros_sleep_times[8], spin_stat.m_spin_interval[9],
         spin_stat.m_micros_sleep_times[9], spin_stat.m_spin_interval[10],
         spin_stat.m_micros_sleep_times[10], spin_stat.m_spin_interval[11],
         spin_stat.m_micros_sleep_times[11], spin_stat.m_spin_interval[12],
         spin_stat.m_micros_sleep_times[12], spin_stat.m_spin_interval[13],
         spin_stat.m_micros_sleep_times[13], spin_stat.m_spin_interval[14],
         spin_stat.m_micros_sleep_times[14],
         spin_stat.m_micros_sleep_times[15]));
  }
  Uint32 spin_time = calc_new_spin(&spin_stat);
  set_spin_stat(spin_time, local_call);
  return;
}

bool Thrman::calculate_next_CPU_measure(
    CPUMeasurementRecord *lastCPUMeasurePtrP,
    CPUMeasurementRecord *firstCPUMeasurePtrP,
    CPUMeasurementRecord *baseCPUMeasurePtrP, CPURecord *cpuPtrP,
    Uint32 ms_between_measurements) {
  bool swap = false;
  if (baseCPUMeasurePtrP->m_first_measure_done) {
    /**
     * Last measurement was recorded in firstCPUMeasurePtrP
     * If there is no measurement there we will record the
     * measurement in firstCPUMeasurePtrP. If there was a
     * measurement recorded in firstCPUMeasurePtrP we will
     * record it in lastCPUMeasurePtrP instead and swap
     * last into first.
     */
    Int64 elapsed_time =
        cpuPtrP->m_curr_measure.m_time - baseCPUMeasurePtrP->m_time;
    if (elapsed_time <= 0) {
      jam();
      elapsed_time = ms_between_measurements;
    }
    if ((elapsed_time + 5) < ms_between_measurements) {
      jam();
      /**
       * It's not time to perform this measurement right now.
       * We will postpone this measurement until later when elapsed time
       * is ready for it. We wait until we are within 5ms of the time
       * between measurements. If elapsed time is negative we have lost
       * track of time for a short period and will assume that the
       * estimated time have passed.
       */
      return false;
    }
    CPUMeasurementRecord *recordCPUMeasurePtrP;
    if (firstCPUMeasurePtrP->m_first_measure_done) {
      jam();
      recordCPUMeasurePtrP = lastCPUMeasurePtrP;
      swap = true;
    } else {
      recordCPUMeasurePtrP = firstCPUMeasurePtrP;
      swap = false;
    }
    recordCPUMeasurePtrP->m_first_measure_done = true;
    recordCPUMeasurePtrP->m_elapsed_time = elapsed_time;

    Int64 user_time =
        cpuPtrP->m_curr_measure.m_user_time - baseCPUMeasurePtrP->m_user_time;
    if (user_time < 0) {
      jam();
      user_time = 0;
    }
    recordCPUMeasurePtrP->m_user_time = user_time;

    Int64 sys_time =
        cpuPtrP->m_curr_measure.m_sys_time - baseCPUMeasurePtrP->m_sys_time;
    if (sys_time < 0) {
      jam();
      sys_time = 0;
    }
    recordCPUMeasurePtrP->m_sys_time = sys_time;

    Int64 idle_time =
        cpuPtrP->m_curr_measure.m_idle_time - baseCPUMeasurePtrP->m_idle_time;
    if (idle_time < 0) {
      jam();
      idle_time = 0;
    }
    recordCPUMeasurePtrP->m_idle_time = idle_time;

    Int64 interrupt_time = cpuPtrP->m_curr_measure.m_interrupt_time -
                           baseCPUMeasurePtrP->m_interrupt_time;
    if (interrupt_time < 0) {
      jam();
      interrupt_time = 0;
    }
    recordCPUMeasurePtrP->m_interrupt_time = interrupt_time;

    Int64 exec_vm_time = cpuPtrP->m_curr_measure.m_exec_vm_time -
                         baseCPUMeasurePtrP->m_exec_vm_time;
    if (exec_vm_time < 0) {
      jam();
      exec_vm_time = 0;
    }
    recordCPUMeasurePtrP->m_exec_vm_time = exec_vm_time;
  }
  baseCPUMeasurePtrP->m_first_measure_done = true;
  baseCPUMeasurePtrP->m_user_time = cpuPtrP->m_curr_measure.m_user_time;
  baseCPUMeasurePtrP->m_sys_time = cpuPtrP->m_curr_measure.m_sys_time;
  baseCPUMeasurePtrP->m_idle_time = cpuPtrP->m_curr_measure.m_idle_time;
  baseCPUMeasurePtrP->m_interrupt_time =
      cpuPtrP->m_curr_measure.m_interrupt_time;
  baseCPUMeasurePtrP->m_exec_vm_time = cpuPtrP->m_curr_measure.m_exec_vm_time;
  baseCPUMeasurePtrP->m_time = cpuPtrP->m_curr_measure.m_time;
  return swap;
}

#define MICROSEC_PER_MILLISEC 1000
void Thrman::fill_in_current_measure(CPURecordPtr cpuPtr,
                                     struct ndb_hwinfo *hwinfo) {
  struct ndb_cpudata *cpu_data = &hwinfo->cpu_data[cpuPtr.i];
  cpuPtr.p->m_curr_measure.m_user_time =
      (cpu_data->cs_user_us + cpu_data->cs_nice_us) / MICROSEC_PER_MILLISEC;
  cpuPtr.p->m_curr_measure.m_sys_time =
      cpu_data->cs_sys_us / MICROSEC_PER_MILLISEC;
  cpuPtr.p->m_curr_measure.m_idle_time =
      (cpu_data->cs_idle_us + cpu_data->cs_iowait_us) / MICROSEC_PER_MILLISEC;
  cpuPtr.p->m_curr_measure.m_interrupt_time =
      (cpu_data->cs_irq_us + cpu_data->cs_sirq_us) / MICROSEC_PER_MILLISEC;
  cpuPtr.p->m_curr_measure.m_exec_vm_time =
      (cpu_data->cs_steal_us + cpu_data->cs_guest_us +
       cpu_data->cs_guest_nice_us) /
      MICROSEC_PER_MILLISEC;
  cpuPtr.p->m_curr_measure.m_unknown_time =
      (cpu_data->cs_unknown1_us + cpu_data->cs_unknown2_us) /
      MICROSEC_PER_MILLISEC;
  cpuPtr.p->m_curr_measure.m_time = cpuPtr.p->m_curr_measure.m_user_time +
                                    cpuPtr.p->m_curr_measure.m_sys_time +
                                    cpuPtr.p->m_curr_measure.m_idle_time +
                                    cpuPtr.p->m_curr_measure.m_interrupt_time +
                                    cpuPtr.p->m_curr_measure.m_exec_vm_time +
                                    cpuPtr.p->m_curr_measure.m_unknown_time;
}

/**
 *
 */
void Thrman::measure_cpu_data(Signal *signal) {
  /**
   * We start by generating the CPU data for the last 50 ms.
   */
  struct ndb_hwinfo *hwinfo = Ndb_GetHWInfo(true);
  ndbrequire(hwinfo->is_cpuinfo_available);
  CPUMeasurementRecordPtr firstCPUMeasurePtr;
  CPUMeasurementRecordPtr lastCPUMeasurePtr;
  CPUMeasurementRecordPtr baseCPUMeasurePtr;
  for (Uint32 cpu_no = 0; cpu_no < hwinfo->cpu_cnt_max; cpu_no++) {
    CPURecordPtr cpuPtr;
    bool swap;
    cpuPtr.i = cpu_no;
    c_CPURecordPool.getPtr(cpuPtr);
    fill_in_current_measure(cpuPtr, hwinfo);
    /* Handle 50ms list by generating diff against last measure */
    {
      LocalDLCFifoList<CPUMeasurementRecord_pool> list(
          c_CPUMeasurementRecordPool, cpuPtr.p->m_next_50ms_measure);
      list.last(lastCPUMeasurePtr);
      list.first(firstCPUMeasurePtr);
    }
    baseCPUMeasurePtr.p = &cpuPtr.p->m_last_50ms_base_measure;
    swap = calculate_next_CPU_measure(lastCPUMeasurePtr.p, firstCPUMeasurePtr.p,
                                      baseCPUMeasurePtr.p, cpuPtr.p, 50);
    if (swap) {
      {
        LocalDLCFifoList<CPUMeasurementRecord_pool> list(
            c_CPUMeasurementRecordPool, cpuPtr.p->m_next_50ms_measure);
        jam();
        list.remove(lastCPUMeasurePtr);
        list.addFirst(lastCPUMeasurePtr);
      }
    }
    /* Handle 1sec list by generating diff against last measure */
    {
      LocalDLCFifoList<CPUMeasurementRecord_pool> list(
          c_CPUMeasurementRecordPool, cpuPtr.p->m_next_1sec_measure);
      list.last(lastCPUMeasurePtr);
      list.first(firstCPUMeasurePtr);
    }
    baseCPUMeasurePtr.p = &cpuPtr.p->m_last_1sec_base_measure;
    swap = calculate_next_CPU_measure(lastCPUMeasurePtr.p, firstCPUMeasurePtr.p,
                                      baseCPUMeasurePtr.p, cpuPtr.p, 1000);
    if (swap) {
      {
        LocalDLCFifoList<CPUMeasurementRecord_pool> list(
            c_CPUMeasurementRecordPool, cpuPtr.p->m_next_1sec_measure);
        jam();
        list.remove(lastCPUMeasurePtr);
        list.addFirst(lastCPUMeasurePtr);
      }
    }
    /* Handle 20sec list by generating diff against last measure */
    {
      LocalDLCFifoList<CPUMeasurementRecord_pool> list(
          c_CPUMeasurementRecordPool, cpuPtr.p->m_next_20sec_measure);
      list.last(lastCPUMeasurePtr);
      list.first(firstCPUMeasurePtr);
    }
    baseCPUMeasurePtr.p = &cpuPtr.p->m_last_20sec_base_measure;
    swap = calculate_next_CPU_measure(lastCPUMeasurePtr.p, firstCPUMeasurePtr.p,
                                      baseCPUMeasurePtr.p, cpuPtr.p, 20000);
    if (swap) {
      {
        LocalDLCFifoList<CPUMeasurementRecord_pool> list(
            c_CPUMeasurementRecordPool, cpuPtr.p->m_next_20sec_measure);
        jam();
        list.remove(lastCPUMeasurePtr);
        list.addFirst(lastCPUMeasurePtr);
      }
    }
  }
}

void Thrman::execUPD_THR_LOAD_ORD(Signal *signal) {
  UpdThrLoadOrd *const thrLoadOrd = (UpdThrLoadOrd *)&signal->theData[0];
  ndbrequire(instance() == m_rep_thrman_instance);
  Uint32 cpu_load = thrLoadOrd->cpuLoad;
  Uint32 send_load = thrLoadOrd->sendLoad;
  Uint32 send_instance = thrLoadOrd->sendInstance;
  Uint32 first_ldm_instance = getFirstLDMThreadInstance();
  Uint32 last_ldm_instance =
      first_ldm_instance + globalData.ndbMtLqhThreads - 1;
  Uint32 last_query_instance = last_ldm_instance + globalData.ndbMtQueryThreads;
  ndbrequire(send_instance >= first_ldm_instance);
  ndbrequire(send_instance <= last_query_instance);
  Uint32 instance_no = send_instance - first_ldm_instance;
  m_thr_load[instance_no][1] = m_thr_load[instance_no][0];
  m_thr_load[instance_no][0].m_cpu_load = cpu_load;
  m_thr_load[instance_no][0].m_send_load = send_load;
}

void Thrman::send_measure_to_rep_thrman(Signal *signal,
                                        MeasurementRecordPtr measurePtr) {
  Uint32 first_ldm_instance = getFirstLDMThreadInstance();
  Uint32 last_ldm_instance =
      first_ldm_instance + globalData.ndbMtLqhThreads - 1;
  Uint32 last_query_instance = last_ldm_instance + globalData.ndbMtQueryThreads;
  Uint32 our_instance = instance();
  if (our_instance < first_ldm_instance || our_instance > last_query_instance) {
    jam();
    /* Only THRMAN in LDM and query threads need to send this signal */
    return;
  }
  jam();
  UpdThrLoadOrd *const thrLoadOrd = (UpdThrLoadOrd *)signal->getDataPtrSend();
  thrLoadOrd->cpuLoad = m_current_cpu_usage;
  Uint64 send_time = measurePtr.p->m_send_time_thread;
  Uint64 elapsed_time = measurePtr.p->m_elapsed_time;
  if (elapsed_time > 0) {
    Uint64 send_perc = (send_time * 1000) + 500;
    send_perc /= (10 * elapsed_time);
    if ((send_perc + m_current_cpu_usage) <= 100) {
      thrLoadOrd->sendLoad = send_perc;
    } else {
      thrLoadOrd->sendLoad = (100 - m_current_cpu_usage);
    }
  } else {
    thrLoadOrd->sendLoad = 0;
  }
  thrLoadOrd->sendInstance = instance();
  BlockReference ref =
      numberToRef(THRMAN, m_rep_thrman_instance, getOwnNodeId());
  sendSignal(ref, GSN_UPD_THR_LOAD_ORD, signal, 3, JBB);
}

void Thrman::initial_query_distribution(Signal *signal) {
  Uint32 num_ldm_instances = getNumLDMInstances();
  Uint32 num_query_threads = globalData.ndbMtQueryThreads;
  Uint32 num_distr_threads = num_ldm_instances + num_query_threads;
  memset(m_curr_weights, 0, sizeof(m_curr_weights));
  for (Uint32 i = 0; i < num_distr_threads; i++) {
    if (i < num_ldm_instances)
      m_curr_weights[i] = 33;
    else
      m_curr_weights[i] = 8;
  }
  send_query_distribution(&m_curr_weights[0], signal);
}

static Int32 get_change_percent(Int32 diff) {
  if (diff <= -40) {
    /**
     * There is a major difference such that query threads work much
     * harder. We will raise weight for all LDM threads to compensate
     * for this.
     */
    return +35;
  } else if (diff <= -30) {
    return +30;
  } else if (diff <= -25) {
    return +25;
  } else if (diff <= -20) {
    return +20;
  } else if (diff <= -15) {
    return +15;
  } else if (diff <= -10) {
    return +10;
  } else if (diff <= -5) {
    return +5;
  } else if (diff < +5) {
    /* No major difference, only act on individual differences */
    return 0;
  } else if (diff <= +10) {
    return -5;
  } else if (diff <= +15) {
    return -10;
  } else if (diff <= +20) {
    return -15;
  } else if (diff <= +25) {
    return -20;
  } else if (diff <= +30) {
    return -25;
  } else if (diff <= +35) {
    return -30;
  } else {
    return -35;
  }
}

/**
 * A weighted load takes the load from 100ms into account a bit, it can
 * affect the weighted load with up to 5%.
 */
static Uint32 calculate_weighted_load(Uint32 last_load, Uint32 prev_load) {
  Uint32 abs_difference =
      last_load > prev_load ? (last_load - prev_load) : (prev_load - last_load);
  if (abs_difference < 10) {
    return ((last_load + prev_load) / 2);
  } else if (last_load > prev_load) {
    return last_load > 25 ? (last_load - 5) : last_load;
  } else {
    return last_load < 90 ? (last_load + 5) : last_load;
  }
}

static Uint32 apply_change_query(Int32 change, bool move_weights_down,
                                 Uint32 in_weight) {
  Int32 desired_change = 0;
  Uint32 abs_change = (change > 0) ? Uint32(change) : Uint32(-change);
  Uint32 max_change = 1;
  if ((change > 0 && !move_weights_down) || (change < 0 && move_weights_down)) {
    max_change = 2;
  }
  if (max_change == 1) {
    if (abs_change >= 5) {
      desired_change = 1;
    }
  } else {
    if (abs_change >= 20) {
      desired_change = 2;
    } else if (abs_change >= 5) {
      desired_change = 1;
    }
  }
  if (change < 0) {
    desired_change *= Int32(-1);
  }
  Int32 new_weight = Int32(in_weight) + desired_change;
  if (new_weight < 0) {
    new_weight = 0;
  } else if (new_weight > MAX_DISTRIBUTION_WEIGHT) {
    new_weight = MAX_DISTRIBUTION_WEIGHT;
  }
  return Uint32(new_weight);
}

static Uint32 apply_change_ldm(Int32 change, Uint32 in_weight) {
  Int32 desired_change = 0;
  if (change < Int32(-2)) {
    if (change < Int32(-15)) {
      desired_change = -3;
    } else if (change < Int32(-9)) {
      desired_change = -2;
    } else {
      desired_change = -1;
    }
    /* Decrease weight */
  } else if (change >= 3) {
    if (change > Int32(+15)) {
      desired_change = +3;
    } else if (change > Int32(+9)) {
      desired_change = +3;
    } else {
      desired_change = +1;
    }
  } else {
    desired_change = 0;
  }

  Int32 new_weight = (Int32)in_weight + desired_change;
  if (new_weight < 0) {
    new_weight = 0;
  } else if (new_weight > MAX_LDM_DISTRIBUTION_WEIGHT) {
    new_weight = MAX_DISTRIBUTION_WEIGHT;
  }
  return Uint32(new_weight);
}

void Thrman::check_weights() {
  Uint32 num_ldm_instances = getNumLDMInstances();
  Uint32 num_query_threads = globalData.ndbMtQueryThreads;
  Uint32 num_distr_threads = num_ldm_instances + num_query_threads;
  for (Uint32 i = 0; i < num_ldm_instances; i++) {
    ndbrequire(m_curr_weights[i] <= MAX_LDM_DISTRIBUTION_WEIGHT);
  }
  for (Uint32 i = num_ldm_instances; i < num_distr_threads; i++) {
    ndbrequire(m_curr_weights[i] <= MAX_DISTRIBUTION_WEIGHT);
  }
}

void Thrman::update_query_distribution(Signal *signal) {
  Uint32 average_cpu_load_ldm = 0;
  Uint32 average_cpu_load_query = 0;
  Uint32 average_send_load_ldm = 0;
  Uint32 average_send_load_query = 0;
  Uint32 max_load_ldm = 0;
  Uint32 max_load_query = 0;
  check_weights();
  /**
   * This function takes the CPU load information from the last
   * 100 milliseconds and use this information to calculate the
   * weights to be used by DBTC, DBSPJ and the receive threads
   * when mapping virtual blocks to LDM and query thread instances.
   */
  Uint32 num_ldm_instances = getNumLDMInstances();
  Uint32 num_query_threads = globalData.ndbMtQueryThreads;
  Uint32 num_distr_threads = num_ldm_instances + num_query_threads;
  Uint32 sum_cpu_load = 0;
  Uint32 sum_send_load = 0;
  Uint32 weighted_cpu_load[MAX_DISTR_THREADS];
  /**
   * Count sum of CPU load in LDM threads
   * Count sum of send CPU load in LDM threads
   * Count sum of CPU load in LDM and Query threads
   * Count sum of send CPU load in LDM and Query threads
   * Count max CPU load on any LDM thread
   * Count max send CPU load on any LDM thread
   */
  for (Uint32 i = 0; i < num_ldm_instances; i++) {
    Uint32 cpu_load = calculate_weighted_load(m_thr_load[i][0].m_cpu_load,
                                              m_thr_load[i][1].m_cpu_load);
    weighted_cpu_load[i] = cpu_load;
    Uint32 send_load = calculate_weighted_load(m_thr_load[i][0].m_send_load,
                                               m_thr_load[i][1].m_send_load);
    sum_cpu_load += cpu_load;
    sum_send_load += send_load;
    max_load_ldm = MAX(cpu_load + send_load, max_load_ldm);
  }
  /* Calculate average CPU and send CPU load on LDM threads */
  average_cpu_load_ldm = sum_cpu_load / num_ldm_instances;
  average_send_load_ldm = sum_send_load / num_ldm_instances;
  sum_cpu_load = 0;
  sum_send_load = 0;
  ndbrequire(num_query_threads != 0) {
    /**
     * Count sum of CPU load on Query threads
     * Count sum of send CPU load on Query threads
     * Count sum of CPU load in LDM and Query threads
     * Count sum of send CPU load in LDM and Query threads
     * Count max CPU load on any Query thread
     * Count max send CPU load on any Query thread
     */
    for (Uint32 i = num_ldm_instances; i < num_distr_threads; i++) {
      Uint32 cpu_load = calculate_weighted_load(m_thr_load[i][0].m_cpu_load,
                                                m_thr_load[i][1].m_cpu_load);
      weighted_cpu_load[i] = cpu_load;
      Uint32 send_load = calculate_weighted_load(m_thr_load[i][0].m_send_load,
                                                 m_thr_load[i][1].m_send_load);
      sum_cpu_load += cpu_load;
      sum_send_load += send_load;
      max_load_query = MAX(cpu_load + send_load, max_load_query);
    }
    /* Calculate average CPU and send CPU load on Query threads */
    average_cpu_load_query = sum_cpu_load / num_query_threads;
    average_send_load_query = sum_send_load / num_query_threads;
  }
  /* Calculate average and max CPU load on Query and LDM threads */
  Uint32 average_cpu_load =
      sum_cpu_load / (num_ldm_instances + num_query_threads);
  Uint32 max_load = MAX(max_load_ldm, max_load_query);

  if (average_cpu_load < 30 && max_load < 40) {
    /**
     * No high average load on LDM threads and no threads in critical
     * load states. We use the initial query distribution again to
     * initialise the state machinery again.
     */
    jam();
    initial_query_distribution(signal);
    return;
  }
  Uint32 average_weight;
  {
    /**
     * We first check if we primarily should move weights up or
     * down. If average is 8 or above we should primarily drive
     * weights downs and if below we should primarily drive them
     * up.
     *
     * In practice what we do is that we allow a change of 1 step
     * only in the opposite direction and up to 2 steps in the
     * desired direction.
     */
    Uint32 sum_weights = 0;
    for (Uint32 i = 0; i < num_ldm_instances; i++)
    /* Count sum of LDM thread weights that we currently use */
    {
      sum_weights += m_curr_weights[i];
    }
    /* Count sum of Query thread weights that we currently use */
    ndbrequire(num_query_threads != 0) {
      for (Uint32 i = num_ldm_instances; i < num_distr_threads; i++) {
        sum_weights += m_curr_weights[i];
      }
      /* Count average Query thread weights that we currently use */
      average_weight = sum_weights / (num_query_threads + num_ldm_instances);
    }
  }
  bool move_weights_down = (average_weight < (MAX_DISTRIBUTION_WEIGHT / 2));

  /**
   * Now for each thread try to move the weight such that it moves towards
   * the average CPU load. For query threads we want all query threads to
   * move towards the same average load. Thus we compare our load with
   * the load of the other query threads.
   *
   * For LDM threads it can be expected that they sometimes have differing
   * loads since they don't necessarily have a balanced load. Here we try
   * to move the weight such that LDMs conform to the average load of all
   * CPUs.
   */
  for (Uint32 i = 0; i < num_ldm_instances; i++) {
    Uint32 cpu_load = weighted_cpu_load[i];
    Uint32 diff = average_cpu_load_query - cpu_load;
    m_curr_weights[i] = apply_change_ldm(diff, m_curr_weights[i]);
  }
  for (Uint32 i = num_ldm_instances; i < num_distr_threads; i++) {
    Uint32 cpu_load = weighted_cpu_load[i];
    Uint32 loc_change = get_change_percent(cpu_load - average_cpu_load_query);
    m_curr_weights[i] =
        apply_change_query(loc_change, move_weights_down, m_curr_weights[i]);
  }
  DEB_SCHED_WEIGHTS(
      ("LDM/QT CPU load stats: %u %u %u %u %u %u %u %u"
       " %u %u %u %u %u %u %u %u",
       weighted_cpu_load[0], weighted_cpu_load[1], weighted_cpu_load[2],
       weighted_cpu_load[3], weighted_cpu_load[4], weighted_cpu_load[5],
       weighted_cpu_load[6], weighted_cpu_load[7], weighted_cpu_load[8],
       weighted_cpu_load[9], weighted_cpu_load[10], weighted_cpu_load[11],
       weighted_cpu_load[12], weighted_cpu_load[13], weighted_cpu_load[14],
       weighted_cpu_load[15]));
  check_weights();
  for (Uint32 i = 0; i < num_ldm_instances; i++) {
    if (m_curr_weights[i] == 0) {
      Uint32 num_query_thread_per_ldm = globalData.QueryThreadsPerLdm;
      Uint32 query_inx = num_ldm_instances + (i * num_query_thread_per_ldm);
      Uint32 query_load = weighted_cpu_load[query_inx];
      Uint32 ldm_load = weighted_cpu_load[i];
      /**
       * LDM load is only allowed to be at 0 if it is fully used,
       * we set the limit to be 80% CPU load. If the LDM load is over
       * 80% we will stop assisting the query threads since our
       * adaptive algorithm decided so.
       *
       * We will not stop using LDM thread until query thread is at least
       * 8% higher than the LDM thread. If LDM thread is very highly loaded
       * AND query threads as well, it makes sense to continue using the
       * LDM thread as well for READ COMMITTED queries even if the load
       * is above 80%.
       */
      if (((query_load + 8) >= ldm_load) || ldm_load <= 80) {
        m_curr_weights[i] = 1;
      }
    }
  }
  for (Uint32 i = num_ldm_instances; i < num_distr_threads; i++) {
    if (m_curr_weights[i] == 0) {
      /**
       * Only LDM threads are allowed to set their weight to 0.
       * Query threads must always be accessible with at least a
       * small weight. If set to 0 the query thread would not
       * be used at all which is a bit useless. So going below
       * 1 for query threads isn't very practical.
       *
       * LDM threads has a lot of activities that they will do
       * even in the case when they don't assist query threads.
       * So they are allowed to drop to 0 and not do any work
       * on query thread activities.
       */
      m_curr_weights[i] = 1;
    }
  }
  send_query_distribution(&m_curr_weights[0], signal);
}

void Thrman::send_query_distribution(Uint32 *weights, Signal *signal) {
  Uint32 num_ldm_instances = getNumLDMInstances();
  Uint32 num_query_threads = globalData.ndbMtQueryThreads;
  Uint32 num_distr_threads = num_ldm_instances + num_query_threads;
  BlockReference ref;

  DEB_SCHED_WEIGHTS(
      ("LDM/QT weights: %u %u %u %u %u %u %u %u"
       " %u %u %u %u %u %u %u %u",
       weights[0], weights[1], weights[2], weights[3], weights[4], weights[5],
       weights[6], weights[7], weights[8], weights[9], weights[10], weights[11],
       weights[12], weights[13], weights[14], weights[15]));
  if (!isNdbMtLqh()) {
    {
      signal->theData[0] = 0;
      LinearSectionPtr lsptr[3];
      lsptr[0].p = weights;
      lsptr[0].sz = 1;
      sendSignal(DBTC, GSN_UPD_QUERY_DIST_ORD, signal, 1, JBB, lsptr, 1);
    }
    {
      signal->theData[0] = 0;
      LinearSectionPtr lsptr[3];
      lsptr[0].p = weights;
      lsptr[0].sz = 1;
      sendSignal(TRPMAN, GSN_UPD_QUERY_DIST_ORD, signal, 1, JBB, lsptr, 1);
    }
    return;
  }
  Uint32 num_recv_threads = globalData.ndbMtReceiveThreads;
  for (Uint32 i = 0; i < num_recv_threads; i++) {
    ref = numberToRef(TRPMAN, i + 1, getOwnNodeId());
    signal->theData[0] = 0;
    LinearSectionPtr lsptr[3];
    lsptr[0].p = weights;
    lsptr[0].sz = num_distr_threads;
    sendSignal(ref, GSN_UPD_QUERY_DIST_ORD, signal, 1, JBB, lsptr, 1);
  }
  Uint32 num_tc_instances = getNumTCInstances();
  for (Uint32 i = 0; i < num_tc_instances; i++) {
    ref = numberToRef(DBTC, i + 1, getOwnNodeId());
    signal->theData[0] = 0;
    LinearSectionPtr lsptr[3];
    lsptr[0].p = weights;
    lsptr[0].sz = num_distr_threads;
    sendSignal(ref, GSN_UPD_QUERY_DIST_ORD, signal, 1, JBB, lsptr, 1);
  }
}

/**
 * We call this function every 50 milliseconds.
 *
 * Load Information Gathering in THRMAN
 * ------------------------------------
 * We gather information from the operating system on how user time and
 * system time the thread has spent. We also get information from the
 * scheduler about how much time the thread has spent in sleep mode,
 * how much time spent sending and how much time spent doing the work
 * the thread is assigned (for most block threads this is executing
 * signals, for receive threads it is receiving and for send threads
 * it is sending.
 *
 * ndbinfo tables based on this gathered information
 * -------------------------------------------------
 * We collect this data such that we can report the last 1 second
 * information about status per 50 milliseconds.
 * We also collect information about reports for 20 seconds with
 * 1 second per collection point.
 * We also collect information about reports for 400 seconds with
 * 20 second per collection point.
 *
 * This data is reported in 3 different ndbinfo tables where each
 * thread reports its own data. Thus twenty rows per thread per node
 * in each of those tables. These tables represent similar information
 * as we can get from top, but here it is reported per ndbmtd
 * block thread and also ndbmtd send thread. Currently we don't
 * cover NDBFS threads and transporter connection threads.
 *
 * We also have a smaller table that reports one row per thread per
 * node and this row represents the load information for the last
 * second.
 *
 * Use of the data for adaptive load regulation of LCPs
 * ----------------------------------------------------
 * This data is also used in adaptive load regulation algorithms in
 * MySQL Cluster data nodes. The intention is to increase this usage
 * with time. The first use case was in 7.4 for adaptive speed of
 * LCPs.
 *
 * Use of data for adaptive send assistance of block threads
 * ---------------------------------------------------------
 * The next use case is to control the sending from various threads.
 * Using send threads we are able to send from any block thread, the
 * receive threads and finally also the send threads.
 *
 * We want to know the load status of each thread to decide how active
 * each thread should be in assisting send threads in sending. The send
 * threads can always send at highest speed.
 *
 * Description of overload states
 * ------------------------------
 * The idea is that when a thread reaches a level where it needs more
 * than 75% of the time to execute then it should offload all send
 * activities to all other threads. However even before we reach this
 * critical level we should adjust our assistance to send threads.
 *
 * As with any adaptive algorithm it is good to have a certain level of
 * hysteresis in the changes. So we should not adjust the levels too
 * fast. One reason for this is that as we increase our level of send
 * assistance we will obviously become more loaded, we want to keep
 * this extra load on a level such that the block thread still can
 * deliver responses to its main activities within reasonable limits.
 *
 * So we will have at least 3 different levels of load for a thread.
 * STATE: Overload
 * ---------------
 * It can be overloaded when it has passed 75% usage for normal thread
 * activity without send activities.
 *
 * STATE: Medium
 * -------------
 * It can be at medium load when it has reached 30% normal thread activity.
 * In this case we should still handle a bit of send assistance, but also
 * offload a part to the send threads.
 *
 * STATE: Light
 * ------------
 * The final level is light load where we are below 30% time spent for normal
 * thread activities. In this case we will for the most part handle our own
 * sending and also assist others in sending.
 *
 * A more detailed description of the send algorithms and how they interact
 * is found in mt.cpp around the method do_send.
 *
 * Global node state for send assistance
 * -------------------------------------
 * One more thing is that we also need global information about the node
 * state. This is provided by the THRMAN with instance number 1 which is
 * non-proxy block executing in the main thread. The scheduler needs to
 * know if any thread is currently in overload mode. If one thread is
 * is in overload mode we should change the sleep interval in all threads.
 * So when there are overloaded threads in the node then we should ensure
 * that all threads wakeup more often to assist in sending. So we change
 * the sleep interval for all threads to 1 milliseconds when we are in
 * this state.
 *
 * The information gathered in instance 1 about send threads is reported to
 * all threads to ensure that all threads can use the mean percentage of
 * usage for send threads in the algorithm to decide when to change overload
 * level. The aim is that overload is defined as 85% instead of 75% when
 * send threads are at more than 75% load level.
 *
 * THRMAN with instance number has one more responsibility, this is to
 * gather the statistics from the send threads.
 *
 * So each thread is responsible to gather information and decide which
 * level of overload it currently is at. It will however report to
 * THRMAN instance 1 about any decision to change of its overload state.
 * So this THRMAN instance have the global node state and have a bit
 * more information about the global state. Based on this information
 * it could potentially make decisions to change the overload state
 * for a certain thread.
 *
 * Reasons for global node state for send assistance
 * -------------------------------------------------
 * One reason to change the state is if we are in a state where we are in
 * a global overload state, this means that the local decisions are not
 * sufficient since the send threads are not capable to keep up with the
 * load even with the assistance they get.
 *
 * The algorithms in THRMAN are to a great extent designed to protect the
 * LDM threads from overload, but at the same time it is possible that
 * the thread configuration is setup such that we have either constant or
 * temporary overload on other threads. Even more in a cloud environment
 * we could easily be affected by other activities in other cloud apps
 * and we thus need to have a bit of flexibility in moving load to other
 * threads currently not so overloaded and thus ensure that we make best
 * use of all CPU resources in the machine assigned to us.
 *
 * Potential future usage of this load information
 * -----------------------------------------------
 * We can provide load control to ensure that the cluster continues to
 * deliver the basic services and in this case we might decrease certain
 * types of query types. We could introduce different priority levels for
 * queries and use those to decide which transactions that are allowed to
 * continue in an overloaded state.
 *
 * The best place to stop any activities is when a transaction starts, so
 * either at normal transaction start in DBTC or DBSPJ or in schema
 * transaction start in DBDICT. Refusing to start a transaction has no
 * impact on already performed work, so this is the best manner to ensure
 * that we don't get into feedback problems where we have to redo the
 * work more than once which is likely to make the overload situation even
 * more severe.
 *
 * Another future development is that threads provide receive thread
 * assistance in the same manner so as to protect the receive threads
 * from overload. This will however require us to ensure that we don't
 * create signalling order issues since signals will be routed different
 * ways dependent on which block thread performs the receive operation.
 */
void Thrman::measure_cpu_usage(Signal *signal) {
  struct ndb_rusage curr_rusage;

  /**
   * Start by making a new CPU usage measurement. After that we will
   * measure how much time has passed since last measurement and from
   * this we can calculate a percentage of CPU usage that this thread
   * has had for the last second or so.
   */

  MeasurementRecordPtr measurePtr;
  MeasurementRecordPtr measure_1sec_Ptr;
  MeasurementRecordPtr measure_20sec_Ptr;

  NDB_TICKS curr_time = NdbTick_getCurrentTicks();
  Uint64 elapsed_50ms = NdbTick_Elapsed(prev_50ms_tick, curr_time).microSec();
  Uint64 elapsed_1sec = NdbTick_Elapsed(prev_1sec_tick, curr_time).microSec();
  Uint64 elapsed_20sec = NdbTick_Elapsed(prev_20sec_tick, curr_time).microSec();
  MeasurementRecord loc_measure;

  /* Get performance timers from scheduler. */
  getPerformanceTimers(
      loc_measure.m_sleep_time_thread, loc_measure.m_spin_time_thread,
      loc_measure.m_buffer_full_time_thread, loc_measure.m_send_time_thread);

  bool check_1sec = false;
  bool check_20sec = false;

  int res = Ndb_GetRUsage(&curr_rusage, false);
  if (res != 0) {
    jam();
#ifdef DEBUG_CPU_USAGE
    g_eventLogger->info("instance: %u failed Ndb_GetRUsage, res: %d",
                        instance(), -res);
#endif
    memset(&curr_rusage, 0, sizeof(curr_rusage));
  }
  {
    jam();
    c_next_50ms_measure.first(measurePtr);
    calculate_measurement(measurePtr, &curr_rusage, &m_last_50ms_rusage,
                          &loc_measure, &m_last_50ms_base_measure,
                          elapsed_50ms);
    Uint64 exec_time =
        measurePtr.p->m_exec_time_thread - measurePtr.p->m_spin_time_thread;
    Uint64 elapsed_time = measurePtr.p->m_elapsed_time;
    if (elapsed_time > 0) {
      Uint64 exec_perc = exec_time * 1000 + 500;
      exec_perc /= (10 * elapsed_time);
      if (exec_perc <= 100) {
        jam();
        m_current_cpu_usage = Uint32(exec_perc);
      } else {
        jam();
        m_current_cpu_usage = 0;
      }
    } else {
      jam();
      m_current_cpu_usage = 0;
    }
    if (m_current_cpu_usage >= 40) {
      DEB_SPIN(("(%u)Current CPU usage is %u percent", instance(),
                m_current_cpu_usage));
    }
    c_next_50ms_measure.remove(measurePtr);
    c_next_50ms_measure.addLast(measurePtr);
    prev_50ms_tick = curr_time;
    send_measure_to_rep_thrman(signal, measurePtr);
  }
  if (elapsed_1sec > Uint64(1000 * 1000)) {
    jam();
    check_1sec = true;
    c_next_1sec_measure.first(measurePtr);
    calculate_measurement(measurePtr, &curr_rusage, &m_last_1sec_rusage,
                          &loc_measure, &m_last_1sec_base_measure,
                          elapsed_1sec);
    c_next_1sec_measure.remove(measurePtr);
    c_next_1sec_measure.addLast(measurePtr);
    prev_1sec_tick = curr_time;
  }
  if (elapsed_20sec > Uint64(20 * 1000 * 1000)) {
    jam();
    check_20sec = true;
    c_next_20sec_measure.first(measurePtr);
    calculate_measurement(measurePtr, &curr_rusage, &m_last_20sec_rusage,
                          &loc_measure, &m_last_20sec_base_measure,
                          elapsed_20sec);
    c_next_20sec_measure.remove(measurePtr);
    c_next_20sec_measure.addLast(measurePtr);
    prev_20sec_tick = curr_time;
  }
  if (instance() == m_main_thrman_instance) {
    jam();
    for (Uint32 send_instance = 0; send_instance < m_num_send_threads;
         send_instance++) {
      jam();
      SendThreadPtr sendThreadPtr;
      SendThreadMeasurementPtr sendThreadMeasurementPtr;
      SendThreadMeasurement curr_send_thread_measure;

      getSendPerformanceTimers(send_instance,
                               curr_send_thread_measure.m_exec_time,
                               curr_send_thread_measure.m_sleep_time,
                               curr_send_thread_measure.m_spin_time,
                               curr_send_thread_measure.m_user_time_os,
                               curr_send_thread_measure.m_kernel_time_os,
                               curr_send_thread_measure.m_elapsed_time_os);

      ndbrequire(c_sendThreadRecordPool.getPtr(sendThreadPtr, send_instance));
      {
        jam();
        Local_SendThreadMeasurement_fifo list_50ms(
            c_sendThreadMeasurementPool,
            sendThreadPtr.p->m_send_thread_50ms_measurements);
        list_50ms.first(sendThreadMeasurementPtr);
        calculate_send_measurement(
            sendThreadMeasurementPtr, &curr_send_thread_measure,
            &sendThreadPtr.p->m_last_50ms_send_thread_measure, elapsed_50ms,
            send_instance);
        list_50ms.remove(sendThreadMeasurementPtr);
        list_50ms.addLast(sendThreadMeasurementPtr);
      }
      if (elapsed_1sec > Uint64(1000 * 1000)) {
        jam();
        Local_SendThreadMeasurement_fifo list_1sec(
            c_sendThreadMeasurementPool,
            sendThreadPtr.p->m_send_thread_1sec_measurements);
        list_1sec.first(sendThreadMeasurementPtr);
        calculate_send_measurement(
            sendThreadMeasurementPtr, &curr_send_thread_measure,
            &sendThreadPtr.p->m_last_1sec_send_thread_measure, elapsed_1sec,
            send_instance);
        list_1sec.remove(sendThreadMeasurementPtr);
        list_1sec.addLast(sendThreadMeasurementPtr);
      }
      if (elapsed_20sec > Uint64(20 * 1000 * 1000)) {
        jam();
        Local_SendThreadMeasurement_fifo list_20sec(
            c_sendThreadMeasurementPool,
            sendThreadPtr.p->m_send_thread_20sec_measurements);
        list_20sec.first(sendThreadMeasurementPtr);
        calculate_send_measurement(
            sendThreadMeasurementPtr, &curr_send_thread_measure,
            &sendThreadPtr.p->m_last_20sec_send_thread_measure, elapsed_20sec,
            send_instance);
        list_20sec.remove(sendThreadMeasurementPtr);
        list_20sec.addLast(sendThreadMeasurementPtr);
      }
    }
    if (check_1sec) {
      Uint32 send_thread_percentage = calculate_mean_send_thread_load();
      sendSEND_THREAD_STATUS_REP(signal, send_thread_percentage);
    }
  }
  check_overload_status(signal, check_1sec, check_20sec);
}

void Thrman::calculate_measurement(MeasurementRecordPtr measurePtr,
                                   struct ndb_rusage *curr_rusage,
                                   struct ndb_rusage *base_rusage,
                                   MeasurementRecord *curr_measure,
                                   MeasurementRecord *base_measure,
                                   Uint64 elapsed_micros) {
  Uint64 user_micros;
  Uint64 kernel_micros;
  Uint64 total_micros;

  measurePtr.p->m_first_measure_done = true;

  measurePtr.p->m_send_time_thread =
      curr_measure->m_send_time_thread - base_measure->m_send_time_thread;

  measurePtr.p->m_sleep_time_thread =
      curr_measure->m_sleep_time_thread - base_measure->m_sleep_time_thread;

  measurePtr.p->m_spin_time_thread =
      curr_measure->m_spin_time_thread - base_measure->m_spin_time_thread;

  measurePtr.p->m_buffer_full_time_thread =
      curr_measure->m_buffer_full_time_thread -
      base_measure->m_buffer_full_time_thread;

  measurePtr.p->m_exec_time_thread =
      elapsed_micros - measurePtr.p->m_sleep_time_thread;

  measurePtr.p->m_elapsed_time = elapsed_micros;

  if ((curr_rusage->ru_utime == 0 && curr_rusage->ru_stime == 0) ||
      (base_rusage->ru_utime == 0 && base_rusage->ru_stime == 0)) {
    jam();
    measurePtr.p->m_user_time_os = 0;
    measurePtr.p->m_kernel_time_os = 0;
    measurePtr.p->m_idle_time_os = 0;
  } else {
    jam();
    user_micros = curr_rusage->ru_utime - base_rusage->ru_utime;
    kernel_micros = curr_rusage->ru_stime - base_rusage->ru_stime;
    total_micros = user_micros + kernel_micros;

    measurePtr.p->m_user_time_os = user_micros;
    measurePtr.p->m_kernel_time_os = kernel_micros;
    if (elapsed_micros >= total_micros) {
      jam();
      measurePtr.p->m_idle_time_os = elapsed_micros - total_micros;
    } else {
      jam();
      measurePtr.p->m_idle_time_os = 0;
    }
  }

#ifdef DEBUG_CPU_USAGE
#ifndef HIGH_DEBUG_CPU_USAGE
  if (elapsed_micros > Uint64(1000 * 1000))
#endif
    g_eventLogger->info(
        "(%u)name: %s, ut_os: %u, kt_os: %u,"
        " idle_os: %u"
        ", elapsed_time: %u, exec_time: %u,"
        " sleep_time: %u, spin_time: %u, send_time: %u",
        instance(), m_thread_name, Uint32(measurePtr.p->m_user_time_os),
        Uint32(measurePtr.p->m_kernel_time_os),
        Uint32(measurePtr.p->m_idle_time_os),
        Uint32(measurePtr.p->m_elapsed_time),
        Uint32(measurePtr.p->m_exec_time_thread),
        Uint32(measurePtr.p->m_sleep_time_thread),
        Uint32(measurePtr.p->m_spin_time_thread),
        Uint32(measurePtr.p->m_send_time_thread));
#endif
  base_rusage->ru_utime = curr_rusage->ru_utime;
  base_rusage->ru_stime = curr_rusage->ru_stime;

  base_measure->m_send_time_thread = curr_measure->m_send_time_thread;
  base_measure->m_sleep_time_thread = curr_measure->m_sleep_time_thread;
  base_measure->m_spin_time_thread = curr_measure->m_spin_time_thread;
  base_measure->m_buffer_full_time_thread =
      curr_measure->m_buffer_full_time_thread;
}

void Thrman::calculate_send_measurement(
    SendThreadMeasurementPtr sendThreadMeasurementPtr,
    SendThreadMeasurement *curr_send_thread_measure,
    SendThreadMeasurement *last_send_thread_measure, Uint64 elapsed_time,
    Uint32 send_instance) {
  (void)elapsed_time;
  sendThreadMeasurementPtr.p->m_first_measure_done = true;

  sendThreadMeasurementPtr.p->m_exec_time =
      curr_send_thread_measure->m_exec_time -
      last_send_thread_measure->m_exec_time;

  sendThreadMeasurementPtr.p->m_sleep_time =
      curr_send_thread_measure->m_sleep_time -
      last_send_thread_measure->m_sleep_time;

  sendThreadMeasurementPtr.p->m_spin_time =
      curr_send_thread_measure->m_spin_time -
      last_send_thread_measure->m_spin_time;

  /**
   * Elapsed time on measurements done is exec_time + sleep_time
   * as exec_time is first measured as elapsed time and then the
   * sleep time is subtracted from elapsed time to get exec time.
   *
   * See run_send_thread main loop for details.
   */
  sendThreadMeasurementPtr.p->m_elapsed_time =
      sendThreadMeasurementPtr.p->m_exec_time +
      sendThreadMeasurementPtr.p->m_sleep_time;
  elapsed_time = sendThreadMeasurementPtr.p->m_elapsed_time;

  if ((curr_send_thread_measure->m_user_time_os == 0 &&
       curr_send_thread_measure->m_kernel_time_os == 0 &&
       curr_send_thread_measure->m_elapsed_time_os == 0) ||
      (last_send_thread_measure->m_user_time_os == 0 &&
       last_send_thread_measure->m_kernel_time_os == 0 &&
       last_send_thread_measure->m_elapsed_time_os == 0)) {
    jam();
    sendThreadMeasurementPtr.p->m_user_time_os = 0;
    sendThreadMeasurementPtr.p->m_kernel_time_os = 0;
    sendThreadMeasurementPtr.p->m_elapsed_time_os = 0;
    sendThreadMeasurementPtr.p->m_idle_time_os = 0;
  } else {
    jam();
    sendThreadMeasurementPtr.p->m_user_time_os =
        curr_send_thread_measure->m_user_time_os -
        last_send_thread_measure->m_user_time_os;

    sendThreadMeasurementPtr.p->m_kernel_time_os =
        curr_send_thread_measure->m_kernel_time_os -
        last_send_thread_measure->m_kernel_time_os;

    sendThreadMeasurementPtr.p->m_elapsed_time_os =
        curr_send_thread_measure->m_elapsed_time_os -
        last_send_thread_measure->m_elapsed_time_os;
    sendThreadMeasurementPtr.p->m_idle_time_os =
        sendThreadMeasurementPtr.p->m_elapsed_time_os -
        (sendThreadMeasurementPtr.p->m_user_time_os +
         sendThreadMeasurementPtr.p->m_kernel_time_os);
  }
#ifdef DEBUG_CPU_USAGE
#ifndef HIGH_DEBUG_CPU_USAGE
  if (elapsed_time > Uint64(1000 * 1000)) {
#endif
    Uint32 sleep = sendThreadMeasurementPtr.p->m_sleep_time;
    Uint32 exec = sendThreadMeasurementPtr.p->m_exec_time;
    int diff = elapsed_time - (sleep + exec);
    g_eventLogger->info(
        "send_instance: %u, exec_time: %u, sleep_time: %u,"
        " spin_tim: %u, elapsed_time: %u, diff: %d"
        ", user_time_os: %u, kernel_time_os: %u,"
        " elapsed_time_os: %u",
        send_instance, (Uint32)sendThreadMeasurementPtr.p->m_exec_time,
        (Uint32)sendThreadMeasurementPtr.p->m_sleep_time,
        (Uint32)sendThreadMeasurementPtr.p->m_spin_time,
        (Uint32)sendThreadMeasurementPtr.p->m_elapsed_time, diff,
        (Uint32)sendThreadMeasurementPtr.p->m_user_time_os,
        (Uint32)sendThreadMeasurementPtr.p->m_kernel_time_os,
        (Uint32)sendThreadMeasurementPtr.p->m_elapsed_time_os);
#ifndef HIGH_DEBUG_CPU_USAGE
  }
#endif
#else
  (void)send_instance;
#endif

  last_send_thread_measure->m_exec_time = curr_send_thread_measure->m_exec_time;

  last_send_thread_measure->m_sleep_time =
      curr_send_thread_measure->m_sleep_time;

  last_send_thread_measure->m_spin_time = curr_send_thread_measure->m_spin_time;

  last_send_thread_measure->m_user_time_os =
      curr_send_thread_measure->m_user_time_os;

  last_send_thread_measure->m_kernel_time_os =
      curr_send_thread_measure->m_kernel_time_os;

  last_send_thread_measure->m_elapsed_time_os =
      curr_send_thread_measure->m_elapsed_time_os;
}

void Thrman::sum_measures(MeasurementRecord *dest, MeasurementRecord *source) {
  dest->m_user_time_os += source->m_user_time_os;
  dest->m_kernel_time_os += source->m_kernel_time_os;
  dest->m_idle_time_os += source->m_idle_time_os;
  dest->m_exec_time_thread += source->m_exec_time_thread;
  dest->m_sleep_time_thread += source->m_sleep_time_thread;
  dest->m_spin_time_thread += source->m_spin_time_thread;
  dest->m_send_time_thread += source->m_send_time_thread;
  dest->m_buffer_full_time_thread += source->m_buffer_full_time_thread;
  dest->m_elapsed_time += source->m_elapsed_time;
}

bool Thrman::calculate_cpu_load_last_second(MeasurementRecord *measure) {
  MeasurementRecordPtr measurePtr;

  memset(measure, 0, sizeof(MeasurementRecord));

  c_next_50ms_measure.first(measurePtr);
  if (measurePtr.p->m_first_measure_done) {
    do {
      jam();
      sum_measures(measure, measurePtr.p);
      c_next_50ms_measure.next(measurePtr);
    } while (measurePtr.i != RNIL &&
             measure->m_elapsed_time < Uint64(NUM_MEASUREMENTS * 50 * 1000));
    static_assert(NUM_MEASUREMENTS * 50 * 1000 == 1000 * 1000);
    return true;
  }
  jam();
  return false;
}

bool Thrman::calculate_cpu_load_last_20seconds(MeasurementRecord *measure) {
  MeasurementRecordPtr measurePtr;

  memset(measure, 0, sizeof(MeasurementRecord));

  c_next_1sec_measure.first(measurePtr);
  if (measurePtr.p->m_first_measure_done) {
    do {
      jam();
      sum_measures(measure, measurePtr.p);
      c_next_1sec_measure.next(measurePtr);
    } while (measurePtr.i != RNIL &&
             measure->m_elapsed_time <
                 Uint64(NUM_MEASUREMENTS * NUM_MEASUREMENTS * 50 * 1000));
    static_assert(NUM_MEASUREMENTS * NUM_MEASUREMENTS * 50 * 1000 ==
                  20 * 1000 * 1000);
    return true;
  }
  jam();
  return false;
}

bool Thrman::calculate_cpu_load_last_400seconds(MeasurementRecord *measure) {
  MeasurementRecordPtr measurePtr;

  memset(measure, 0, sizeof(MeasurementRecord));

  c_next_20sec_measure.first(measurePtr);
  if (measurePtr.p->m_first_measure_done) {
    do {
      jam();
      sum_measures(measure, measurePtr.p);
      c_next_20sec_measure.next(measurePtr);
    } while (measurePtr.i != RNIL &&
             measure->m_elapsed_time <
                 Uint64(NUM_MEASUREMENTS * NUM_MEASUREMENTS * NUM_MEASUREMENTS *
                        50 * 1000));
    static_assert(NUM_MEASUREMENTS * NUM_MEASUREMENTS * NUM_MEASUREMENTS * 50 *
                      1000 ==
                  400 * 1000 * 1000);
    return true;
  }
  jam();
  return false;
}

void Thrman::init_stats(MeasureStats *stats) {
  stats->min_os_percentage = 100;
  stats->min_next_os_percentage = 100;

  stats->max_os_percentage = 0;
  stats->max_next_os_percentage = 0;

  stats->avg_os_percentage = 0;

  stats->min_thread_percentage = 100;
  stats->min_next_thread_percentage = 100;

  stats->max_thread_percentage = 0;
  stats->max_next_thread_percentage = 0;
  stats->avg_thread_percentage = 0;

  stats->avg_send_percentage = 0;
}

void Thrman::calc_stats(MeasureStats *stats, MeasurementRecord *measure) {
  Uint64 thread_percentage = 0;
  {
    if (measure->m_elapsed_time > 0) {
      Uint64 not_used_exec_time =
          measure->m_buffer_full_time_thread + measure->m_spin_time_thread;
      Uint64 used_exec_time = 0;
      if (measure->m_exec_time_thread > not_used_exec_time) {
        used_exec_time = measure->m_exec_time_thread - not_used_exec_time;
      }
      thread_percentage =
          Uint64(1000) * used_exec_time / measure->m_elapsed_time;
    }
    thread_percentage += 5;
    thread_percentage /= 10;
    if (thread_percentage > 100) {
      thread_percentage = 100;
    }

    if (thread_percentage < stats->min_thread_percentage) {
      jam();
      stats->min_next_thread_percentage = stats->min_thread_percentage;
      stats->min_thread_percentage = thread_percentage;
    } else if (thread_percentage < stats->min_next_thread_percentage) {
      jam();
      stats->min_next_thread_percentage = thread_percentage;
    } else if (thread_percentage > stats->max_thread_percentage) {
      jam();
      stats->max_next_thread_percentage = stats->max_thread_percentage;
      stats->max_thread_percentage = thread_percentage;
    } else if (thread_percentage > stats->max_next_thread_percentage) {
      jam();
      stats->max_next_thread_percentage = thread_percentage;
    }
    stats->avg_thread_percentage += thread_percentage;
  }

  Uint64 divider = 1;
  Uint64 multiplier = 1;
  Uint64 spin_percentage = 0;
  if (measure->m_elapsed_time > 0) {
    spin_percentage =
        (Uint64(1000) * measure->m_spin_time_thread) / measure->m_elapsed_time;
    spin_percentage += 5;
    spin_percentage /= 10;
  }
  if (spin_percentage > 1) {
    jam();
    /**
     * We take spin time into account for OS time when it is at least
     * spinning 2% of the time. Otherwise we will ignore it. What we
     * do is that we assume that the time spent in OS time is equally
     * divided as the measured time, so e.g. if we spent 60% of the
     * time in exec and 30% spinning, then we will multiply os
     * percentage by 2/3 since we assume that a third of the time
     * in the OS time was spent spinning and we don't want spin time
     * to be counted as execution time, it is a form of busy sleep
     * time.
     */
    multiplier = thread_percentage;
    divider = (spin_percentage + thread_percentage);
  }

  {
    Uint64 os_percentage = 0;
    if (measure->m_elapsed_time > 0) {
      os_percentage = Uint64(1000) *
                      (measure->m_user_time_os + measure->m_kernel_time_os) /
                      measure->m_elapsed_time;
    }
    /* Take spin time into account */
    os_percentage *= multiplier;
    os_percentage /= divider;

    /**
     * We calculated percentage * 10, so by adding 5 we ensure that
     * rounding is ok. Integer division always round 99.9 to 99, so
     * we need to add 0.5% to get proper rounding.
     */
    os_percentage += 5;
    os_percentage /= 10;

    if (os_percentage < stats->min_os_percentage) {
      jam();
      stats->min_next_os_percentage = stats->min_os_percentage;
      stats->min_os_percentage = os_percentage;
    } else if (os_percentage < stats->min_next_os_percentage) {
      jam();
      stats->min_next_os_percentage = os_percentage;
    } else if (os_percentage > stats->max_os_percentage) {
      jam();
      stats->max_next_os_percentage = stats->max_os_percentage;
      stats->max_os_percentage = os_percentage;
    } else if (os_percentage > stats->max_next_os_percentage) {
      jam();
      stats->max_next_os_percentage = os_percentage;
    }
    stats->avg_os_percentage += os_percentage;
  }
  Uint64 send_percentage = 0;
  if (measure->m_elapsed_time > 0) {
    send_percentage =
        (Uint64(1000) * measure->m_send_time_thread) / measure->m_elapsed_time;
  }
  send_percentage += 5;
  send_percentage /= 10;
  stats->avg_send_percentage += send_percentage;
}

void Thrman::calc_avgs(MeasureStats *stats, Uint32 num_stats) {
  stats->avg_os_percentage /= num_stats;
  stats->avg_thread_percentage /= num_stats;
  stats->avg_send_percentage /= num_stats;
}

bool Thrman::calculate_stats_last_100ms(MeasureStats *stats) {
  MeasurementRecordPtr measurePtr;
  Uint32 num_stats = 0;
  Uint64 elapsed_time = 0;

  init_stats(stats);
  c_next_50ms_measure.first(measurePtr);
  if (!measurePtr.p->m_first_measure_done) {
    jam();
    return false;
  }
  do {
    jam();
    calc_stats(stats, measurePtr.p);
    num_stats++;
    elapsed_time += measurePtr.p->m_elapsed_time;
    c_next_50ms_measure.next(measurePtr);
  } while (measurePtr.i != RNIL && elapsed_time < Uint64(100 * 1000));
  calc_avgs(stats, num_stats);
  return true;
}

bool Thrman::calculate_stats_last_second(MeasureStats *stats) {
  MeasurementRecordPtr measurePtr;
  Uint32 num_stats = 0;
  Uint64 elapsed_time = 0;

  init_stats(stats);
  c_next_50ms_measure.first(measurePtr);
  if (!measurePtr.p->m_first_measure_done) {
    jam();
    return false;
  }
  do {
    jam();
    calc_stats(stats, measurePtr.p);
    num_stats++;
    elapsed_time += measurePtr.p->m_elapsed_time;
    c_next_50ms_measure.next(measurePtr);
  } while (measurePtr.i != RNIL &&
           elapsed_time < Uint64(NUM_MEASUREMENTS * 50 * 1000));
  static_assert(NUM_MEASUREMENTS * 50 * 1000 == 1000 * 1000);
  calc_avgs(stats, num_stats);
  return true;
}

bool Thrman::calculate_stats_last_20seconds(MeasureStats *stats) {
  MeasurementRecordPtr measurePtr;
  Uint32 num_stats = 0;
  Uint64 elapsed_time = 0;

  init_stats(stats);
  c_next_1sec_measure.first(measurePtr);
  if (!measurePtr.p->m_first_measure_done) {
    jam();
    return false;
  }
  do {
    jam();
    calc_stats(stats, measurePtr.p);
    num_stats++;
    elapsed_time += measurePtr.p->m_elapsed_time;
    c_next_1sec_measure.next(measurePtr);
  } while (measurePtr.i != RNIL &&
           elapsed_time <
               Uint64(NUM_MEASUREMENTS * NUM_MEASUREMENTS * 50 * 1000));
  static_assert(NUM_MEASUREMENTS * NUM_MEASUREMENTS * 50 * 1000 ==
                20 * 1000 * 1000);
  calc_avgs(stats, num_stats);
  return true;
}

bool Thrman::calculate_stats_last_400seconds(MeasureStats *stats) {
  MeasurementRecordPtr measurePtr;
  Uint32 num_stats = 0;
  Uint64 elapsed_time = 0;

  init_stats(stats);
  c_next_20sec_measure.first(measurePtr);
  if (!measurePtr.p->m_first_measure_done) {
    jam();
    return false;
  }
  do {
    jam();
    calc_stats(stats, measurePtr.p);
    num_stats++;
    elapsed_time += measurePtr.p->m_elapsed_time;
    c_next_20sec_measure.next(measurePtr);
  } while (measurePtr.i != RNIL &&
           elapsed_time < Uint64(NUM_MEASUREMENTS * NUM_MEASUREMENTS *
                                 NUM_MEASUREMENTS * 50 * 1000));
  static_assert(NUM_MEASUREMENTS * NUM_MEASUREMENTS * NUM_MEASUREMENTS * 50 *
                    1000 ==
                400 * 1000 * 1000);
  calc_avgs(stats, num_stats);
  return true;
}

bool Thrman::calculate_send_thread_load_last_second(
    Uint32 send_instance, SendThreadMeasurement *measure) {
  SendThreadPtr sendThreadPtr;
  SendThreadMeasurementPtr sendThreadMeasurementPtr;

  memset(measure, 0, sizeof(SendThreadMeasurement));

  ndbrequire(c_sendThreadRecordPool.getPtr(sendThreadPtr, send_instance));

  Local_SendThreadMeasurement_fifo list_50ms(
      c_sendThreadMeasurementPool,
      sendThreadPtr.p->m_send_thread_50ms_measurements);
  list_50ms.first(sendThreadMeasurementPtr);

  if (sendThreadMeasurementPtr.p->m_first_measure_done) {
    do {
      jam();
      measure->m_exec_time += sendThreadMeasurementPtr.p->m_exec_time;
      measure->m_sleep_time += sendThreadMeasurementPtr.p->m_sleep_time;
      measure->m_spin_time += sendThreadMeasurementPtr.p->m_spin_time;
      measure->m_elapsed_time += (sendThreadMeasurementPtr.p->m_exec_time +
                                  sendThreadMeasurementPtr.p->m_sleep_time);
      measure->m_user_time_os += sendThreadMeasurementPtr.p->m_user_time_os;
      measure->m_kernel_time_os += sendThreadMeasurementPtr.p->m_kernel_time_os;
      measure->m_elapsed_time_os +=
          sendThreadMeasurementPtr.p->m_elapsed_time_os;
      measure->m_idle_time_os += sendThreadMeasurementPtr.p->m_idle_time_os;
      list_50ms.next(sendThreadMeasurementPtr);
    } while (sendThreadMeasurementPtr.i != RNIL &&
             measure->m_elapsed_time < Uint64(1000 * 1000));
    return true;
  }
  jam();
  return false;
}

Uint32 Thrman::calculate_mean_send_thread_load() {
  SendThreadMeasurement measure;
  Uint32 tot_percentage = 0;
  if (m_num_send_threads == 0) {
    return 0;
  }
  for (Uint32 i = 0; i < m_num_send_threads; i++) {
    jam();
    bool succ = calculate_send_thread_load_last_second(i, &measure);
    if (!succ) {
      jam();
      return 0;
    }

    Uint64 send_thread_percentage = 0;
    if (measure.m_elapsed_time) {
      send_thread_percentage = Uint64(1000) *
                               (measure.m_exec_time - measure.m_spin_time) /
                               measure.m_elapsed_time;
    }
    send_thread_percentage += 5;
    send_thread_percentage /= 10;

    Uint64 send_spin_percentage = 0;
    Uint64 multiplier = 1;
    Uint64 divider = 1;
    if (measure.m_elapsed_time) {
      send_spin_percentage =
          (Uint64(1000) * measure.m_spin_time) / measure.m_elapsed_time;
      send_spin_percentage += 5;
      send_spin_percentage /= 10;
    }

    if (send_spin_percentage > 1) {
      jam();
      multiplier = send_thread_percentage;
      divider = (send_thread_percentage + send_spin_percentage);
    }

    Uint64 send_os_percentage = 0;
    if (measure.m_elapsed_time_os) {
      send_os_percentage =
          (Uint64(1000) * (measure.m_user_time_os + measure.m_kernel_time_os) /
           measure.m_elapsed_time_os);
    }
    send_os_percentage *= multiplier;
    send_os_percentage /= divider;

    send_os_percentage += 5;
    send_os_percentage /= 10;

    if (send_os_percentage > send_thread_percentage) {
      jam();
      send_thread_percentage = send_os_percentage;
    }
    tot_percentage += Uint32(send_thread_percentage);
  }
  tot_percentage /= m_num_send_threads;
  return tot_percentage;
}

void Thrman::execGET_CPU_USAGE_REQ(Signal *signal) {
  MeasurementRecord curr_measure;
  if (calculate_cpu_load_last_second(&curr_measure)) {
    jam();
    Uint64 percentage = (Uint64(100) * curr_measure.m_exec_time_thread) /
                        curr_measure.m_elapsed_time;
    signal->theData[0] = Uint32(percentage);
  } else {
    jam();
    signal->theData[0] = default_cpu_load;
  }
}

void Thrman::handle_decisions() {
  MeasureStats *stats = m_current_decision_stats;

  if (stats->avg_thread_percentage > (stats->avg_os_percentage + 25)) {
    jam();
    if (!m_shared_environment) {
      jam();
      g_eventLogger->info(
          "Setting ourselves in shared environment,"
          " instance: %u, thread pct: %u"
          ", os_pct: %u, intervals os: [%u, %u] thread: [%u, %u]",
          instance(), Uint32(stats->avg_thread_percentage),
          Uint32(stats->avg_os_percentage),
          Uint32(stats->min_next_os_percentage),
          Uint32(stats->max_next_os_percentage),
          Uint32(stats->min_next_thread_percentage),
          Uint32(stats->max_next_thread_percentage));
    }
    m_shared_environment = true;
    m_max_warning_level = 200;
  } else if (stats->avg_thread_percentage < (stats->avg_os_percentage + 15)) {
    /**
     * We use a hysteresis to avoid swapping between shared environment and
     * exclusive environment to quick when conditions quickly change.
     */
    jam();
    if (m_shared_environment) {
      jam();
      g_eventLogger->info(
          "Setting ourselves in exclusive environment,"
          " instance: %u, thread pct: %u"
          ", os_pct: %u, intervals os: [%u, %u] thread: [%u, %u]",
          instance(), Uint32(stats->avg_thread_percentage),
          Uint32(stats->avg_os_percentage),
          Uint32(stats->min_next_os_percentage),
          Uint32(stats->max_next_os_percentage),
          Uint32(stats->min_next_thread_percentage),
          Uint32(stats->max_next_thread_percentage));
    }
    m_shared_environment = false;
    m_max_warning_level = 20;
  }
}

Uint32 Thrman::calculate_load(MeasureStats &stats, Uint32 &burstiness) {
  if (stats.avg_os_percentage >= stats.avg_thread_percentage) {
    burstiness = 0;
    jam();
    /* Always pick OS reported average unless thread reports higher. */
    return Uint32(stats.avg_os_percentage);
  }
  jam();
  burstiness = Uint32(stats.avg_thread_percentage - stats.avg_os_percentage);
  return Uint32(stats.avg_thread_percentage);
}

#define LIGHT_LOAD_LEVEL 30
#define MEDIUM_LOAD_LEVEL 75
#define CRITICAL_SEND_LEVEL 75
#define CRITICAL_OVERLOAD_LEVEL 85

Int32 Thrman::get_load_status(Uint32 load, Uint32 send_load) {
  Uint32 base_load = 0;
  if (load > send_load) {
    jam();
    base_load = load - send_load;
  }

  if (base_load < LIGHT_LOAD_LEVEL && load < CRITICAL_OVERLOAD_LEVEL) {
    jam();
    return (OverloadStatus)LIGHT_LOAD_CONST;
  } else if (base_load < MEDIUM_LOAD_LEVEL) {
    jam();
    return (OverloadStatus)MEDIUM_LOAD_CONST;
  } else if (base_load < CRITICAL_OVERLOAD_LEVEL) {
    if (m_send_thread_percentage >= CRITICAL_SEND_LEVEL) {
      jam();
      return (OverloadStatus)MEDIUM_LOAD_CONST;
    } else {
      jam();
      return (OverloadStatus)OVERLOAD_CONST;
    }
  } else {
    jam();
    return (OverloadStatus)OVERLOAD_CONST;
  }
}

void Thrman::change_warning_level(Int32 diff_status, Uint32 factor) {
  switch (diff_status) {
    case Int32(-2):
      jam();
      inc_warning(3 * factor);
      break;
    case Int32(-1):
      jam();
      inc_warning(factor);
      break;
    case Int32(0):
      jam();
      down_warning(factor);
      break;
    case Int32(1):
      jam();
      dec_warning(factor);
      break;
    case Int32(2):
      jam();
      dec_warning(3 * factor);
      break;
    default:
      ndbabort();
  }
}

void Thrman::handle_overload_stats_1sec() {
  Uint32 burstiness;
  bool decision_stats = m_current_decision_stats == &c_1sec_stats;

  if (decision_stats) {
    jam();
    handle_decisions();
  }
  Uint32 load = calculate_load(c_1sec_stats, burstiness);
  m_burstiness += burstiness;

  Int32 load_status = get_load_status(load, c_1sec_stats.avg_send_percentage);
  Int32 diff_status = Int32(m_current_overload_status) - load_status;
  Uint32 factor = 10;
  change_warning_level(diff_status, factor);
}

void Thrman::handle_overload_stats_20sec() {
  Uint32 burstiness;
  bool decision_stats = m_current_decision_stats == &c_20sec_stats;

  if (decision_stats) {
    jam();
    handle_decisions();
  }
  /* Burstiness only incremented for 1 second stats */
  Uint32 load = calculate_load(c_20sec_stats, burstiness);
  check_burstiness();

  Int32 load_status = get_load_status(load, c_20sec_stats.avg_send_percentage);
  Int32 diff_status = Int32(m_current_overload_status) - load_status;
  Uint32 factor = 3;
  change_warning_level(diff_status, factor);
}

void Thrman::handle_overload_stats_400sec() {
  /**
   * We only use 400 second stats for long-term decisions, not to affect
   * the ongoing decisions.
   */
  handle_decisions();
}

/**
 * Sum burstiness for 20 seconds and if burstiness is at very high levels
 * we report it to the user in the node log. It is rather unlikely that
 * a reliable service can be delivered in very bursty environments.
 */
void Thrman::check_burstiness() {
  if (m_burstiness > NUM_MEASUREMENTS * 25) {
    jam();
    g_eventLogger->info(
        "Bursty environment, mean burstiness of %u pct"
        ", some risk of congestion issues",
        m_burstiness / NUM_MEASUREMENTS);
  } else if (m_burstiness > NUM_MEASUREMENTS * 50) {
    jam();
    g_eventLogger->info(
        "Very bursty environment, mean burstiness of %u pct"
        ", risk for congestion issues",
        m_burstiness / NUM_MEASUREMENTS);
  } else if (m_burstiness > NUM_MEASUREMENTS * 75) {
    jam();
    g_eventLogger->info(
        "Extremely bursty environment, mean burstiness of %u pct"
        ", very high risk for congestion issues",
        m_burstiness / NUM_MEASUREMENTS);
  }
  m_burstiness = 0;
}

/**
 * This function is used to indicate that we're moving towards higher overload
 * states, so we will unconditionally move the warning level up.
 */
void Thrman::inc_warning(Uint32 inc_factor) { m_warning_level += inc_factor; }

/**
 * This function is used to indicate that we're moving towards lower overload
 * states, so we will unconditionally move the warning level down.
 */
void Thrman::dec_warning(Uint32 dec_factor) { m_warning_level -= dec_factor; }

/**
 * This function is used to indicate that we're at the correct overload state.
 * We will therefore decrease warning levels towards zero independent of whether
 * we are at high warning levels or low levels.
 */
void Thrman::down_warning(Uint32 down_factor) {
  if (m_warning_level > Int32(down_factor)) {
    jam();
    m_warning_level -= down_factor;
  } else if (m_warning_level < (-Int32(down_factor))) {
    jam();
    m_warning_level += down_factor;
  } else {
    jam();
    m_warning_level = 0;
  }
}

void Thrman::sendOVERLOAD_STATUS_REP(Signal *signal) {
  signal->theData[0] = instance();
  signal->theData[1] = m_current_overload_status;
  BlockReference ref =
      numberToRef(THRMAN, m_main_thrman_instance, getOwnNodeId());
  sendSignal(ref, GSN_OVERLOAD_STATUS_REP, signal, 2, JBB);
}

void Thrman::sendSEND_THREAD_STATUS_REP(Signal *signal, Uint32 percentage) {
  signal->theData[0] = percentage;
  for (Uint32 instance_no = 1; instance_no <= m_num_threads; instance_no++) {
    BlockReference ref = numberToRef(THRMAN, instance_no, getOwnNodeId());
    sendSignal(ref, GSN_SEND_THREAD_STATUS_REP, signal, 1, JBB);
  }
}

void Thrman::handle_state_change(Signal *signal) {
  if (m_warning_level > Int32(m_max_warning_level)) {
    /**
     * Warning has reached a threshold and we need to increase the overload
     * status.
     */
    if (m_current_overload_status == (OverloadStatus)LIGHT_LOAD_CONST) {
      jam();
      m_current_overload_status = (OverloadStatus)MEDIUM_LOAD_CONST;
    } else if (m_current_overload_status == (OverloadStatus)MEDIUM_LOAD_CONST) {
      jam();
      m_current_overload_status = (OverloadStatus)OVERLOAD_CONST;
    } else {
      ndbabort();
    }
    jam();
#ifdef DEBUG_CPU_USAGE
    g_eventLogger->info("instance: %u change to new state: %u, warning: %d",
                        instance(), m_current_overload_status, m_warning_level);
#endif
    setOverloadStatus(m_current_overload_status);
    m_warning_level = 0;
    sendOVERLOAD_STATUS_REP(signal);
    return;
  } else if (m_warning_level < (-Int32(m_max_warning_level))) {
    /**
     * Warning has reached a threshold and we need to decrease the overload
     * status.
     */
    if (m_current_overload_status == (OverloadStatus)LIGHT_LOAD_CONST) {
      ndbabort();
    } else if (m_current_overload_status == (OverloadStatus)MEDIUM_LOAD_CONST) {
      jam();
      m_current_overload_status = (OverloadStatus)LIGHT_LOAD_CONST;
    } else if (m_current_overload_status == (OverloadStatus)OVERLOAD_CONST) {
      jam();
      m_current_overload_status = (OverloadStatus)MEDIUM_LOAD_CONST;
    } else {
      ndbabort();
    }
    jam();
#ifdef DEBUG_CPU_USAGE
    g_eventLogger->info("instance: %u change to new state: %u, warning: %d",
                        instance(), m_current_overload_status, m_warning_level);
#endif
    setOverloadStatus(m_current_overload_status);
    m_warning_level = 0;
    sendOVERLOAD_STATUS_REP(signal);
    return;
  }
  jam();
#ifdef HIGH_DEBUG_CPU_USAGE
  g_eventLogger->info("instance: %u stay at state: %u, warning: %d", instance(),
                      m_current_overload_status, m_warning_level);
#endif
  /* Warning level is within bounds, no need to change anything. */
  return;
}

void Thrman::check_overload_status(Signal *signal, bool check_1sec,
                                   bool check_20sec) {
  /**
   * This function checks the current overload status and makes a decision if
   * the status should change or if it is to remain at the current status.
   *
   * We have two measurements that we use to decide on overload status.
   * The first is the measurement based on the actual data reported by the OS.
   * This data is considered as correct when it comes to how much CPU time our
   * thread has used. However it will not say anything about the environment
   * we are executing in.
   *
   * So in order to get a feel for this environment we estimate also the time
   * we are spending in execution mode, how much time we are spending in
   * sleep mode. We also take into account if the thread has been spinning,
   * this time is added to the sleep time and subtracted fromt the exec time
   * of a thread.
   *
   * We can calculate idle time in two ways.
   * 1) m_elapsed_time - (m_user_time_os + m_kernel_time_os)
   * This is the actual idle time for the thread. We can only really use
   * this measurement in the absence of spin time, spinning time will be
   * added to OS time, but isn't really execution time.
   * 2) m_sleep_time_thread + m_spin_time_thread
   * This is the time that we actually decided to be idle because we had
   * no work to do. There are two possible reasons why these could differ.
   * One is if we have much mutex contentions that makes the OS put us into
   * idle mode since we need the mutex to proceed. The second is when we go
   * to sleep based on that we cannot proceed because we're out of buffers
   * somewhere. This is actually tracked by m_buffer_full_time_thread, so
   * we can add m_sleep_time_thread and m_buffer_full_time_thread to see
   * the total time we have decided to go to sleep.
   *
   * Finally we can also be descheduled by the OS by other threads that
   * compete for CPU resources. This kind of environment is much harder
   * to control since the variance of the load can be significant.
   *
   * So we want to measure this background load to see how much CPU resources
   * we actually have access to. If we operate in this type of environment we
   * need to change the overload status in a much slower speed. If we operate
   * in an environment where we get all the resources we need and more or less
   * always have access to a CPU when we need to, in this case we can react
   * much faster to changes. Still we don't want to react too fast since the
   * application behaviour can be a bit bursty as well, and we don't want to
   * go back to default behaviour too quick in these cases.
   *
   * We decide which environment we are processing in once every 20 seconds.
   * If we decide that we are in an environment where we don't have access
   * to dedicated CPU resources we will set the change speed to 10 seconds.
   * This means that the warning level need to reach 200 before we actually
   * change to a new overload level.
   *
   * If we operate in a nice environment where we have very little problems
   * with competition for CPU resources we will set the warning level to 20
   * before we change the overload level.
   *
   * So every 20 seconds we will calculate the following parameters for our
   * thread.
   *
   * 1) Mean CPU percentage as defined by (m_user_time_os + m_kernel_time_os)/
   *    m_elapsed_time_os.
   * 2) 95% confidence interval for this measurement (thus given that it is
   *    calculated by 20 estimates we drop the highest and the lowest
   *    percentage numbers. We will store the smallest percentage and the
   *    highest percentage of this interval.
   * 3) We calculate the same 3 values based on (m_exec_time_thread -
   *    (m_buffer_full_time_thread + m_spin_time_thread)) / m_elapsed_time.
   * 4) In addition we also calculate the mean value of
   *    m_send_time_thread / m_elapsed_time.
   *
   * Finally we take the mean numbers calculated in 1) and 3) and compare
   * them. If 3) is more than 10% higher than 1) than we consider ourselves
   * to be in a "shared" environment. Otherwise we decide that we are in an
   * "exclusive" environment.
   *
   * If we haven't got 400 seconds of statistics we will make a first estimate
   * based on 1 second of data and then again after 20 seconds of execution.
   * So the first 20 seconds we will check once per second the above. Then
   * we will check once per 20 seconds but only check the last 20 seconds of
   * data. After 400 seconds we will go over to checking all statistics back
   * 400 seconds.
   *
   * We will track the overload level by using warning level which is an
   * integer. So when it reaches either -20 or +20 we will decide to decrease/
   * increase the overload level in an exclusive environment.
   * In addition once every 1 seconds we will calculate the average over the
   * period and once every 20 seconds we will calculate the average over this
   * period.
   *
   * In general the overload levels are aimed at the following:
   * LIGHT_LOAD:
   * Light load is defined as using less than 30% of the capacity.
   *
   * MEDIUM_LOAD:
   * Medium load is defined as using less than 75% of the capacity, but
   * more than or equal to 30% of the capacity.
   *
   * OVERLOAD:
   * Overload is defined as when one is using more than 75% of the capacity.
   *
   * The capacity is the CPU resources we have access to, they can differ
   * based on which environment we are in.
   *
   * We define OVERLOAD_STATUS as being at more than 75% load level. At this
   * level we want to avoid sending anything from our node. We will definitely
   * stay at this level if we can show that any of the following is true for the
   * last 50 milliseconds: 1) m_user_time_os + m_kernel_time_os is at least 75%
   * of m_elapsed_time OR 2) m_exec_time_thread is at least 75% of
   * m_elapsed_time
   *
   * At this level the influence of doing sends should not matter since we
   * are not performing any sends at this overload level.
   *
   * If performance drops down into the 30-75% range for any of this values
   * then we will increment a warning counter. This warning counter will be
   * decreased by reaching above 75%. If the warning counter reaches 20 we
   * will go down to MEDIUM overload level. In shared environment with bursty
   * behaviour we will wait until the warning level reaches 200.
   */
  if (check_1sec) {
    jam();
    if (calculate_stats_last_second(&c_1sec_stats)) {
      jam();
      m_overload_handling_activated = true;
      handle_overload_stats_1sec();
    }
  }
  if (check_20sec) {
    jam();
    if (calculate_stats_last_400seconds(&c_400sec_stats)) {
      jam();
      m_overload_handling_activated = true;
      m_current_decision_stats = &c_400sec_stats;
      handle_overload_stats_400sec();
      ndbrequire(calculate_stats_last_20seconds(&c_20sec_stats));
    } else if (calculate_stats_last_20seconds(&c_20sec_stats)) {
      jam();
      if (m_current_decision_stats != &c_400sec_stats) {
        jam();
        m_current_decision_stats = &c_20sec_stats;
      }
      m_overload_handling_activated = true;
      handle_overload_stats_20sec();
    }
  }
  if (!m_overload_handling_activated) {
    jam();
    return;
  }

  MeasureStats stats;
  Uint32 burstiness;
  calculate_stats_last_100ms(&stats);
  Uint32 load = calculate_load(stats, burstiness);

  Int32 load_status = get_load_status(load, stats.avg_send_percentage);
  Int32 diff_status = Int32(m_current_overload_status) - load_status;
  Uint32 factor = 1;
  change_warning_level(diff_status, factor);

  handle_state_change(signal);
}

void Thrman::send_cpu_measurement_row(DbinfoScanReq &req,
                                      Ndbinfo::Ratelimit &rl, Signal *signal,
                                      CPUMeasurementRecordPtr cpuMeasurePtr,
                                      Uint32 cpu_no, Uint32 online) {
  Ndbinfo::Row row(signal, req);
  row.write_uint32(getOwnNodeId());
  row.write_uint32(cpu_no);
  row.write_uint32(online);

  Uint64 elapsed_time = cpuMeasurePtr.p->m_elapsed_time;
  Uint64 user_time = cpuMeasurePtr.p->m_user_time;
  user_time = (user_time * elapsed_time) / 1000;
  row.write_uint32(Uint32(user_time));

  Uint64 idle_time = cpuMeasurePtr.p->m_idle_time;
  idle_time = (idle_time * elapsed_time) / 1000;
  row.write_uint32(Uint32(idle_time));

  Uint64 sys_time = cpuMeasurePtr.p->m_sys_time;
  sys_time = (sys_time * elapsed_time) / 1000;
  row.write_uint32(Uint32(sys_time));

  Uint64 interrupt_time = cpuMeasurePtr.p->m_interrupt_time;
  interrupt_time = (interrupt_time * elapsed_time) / 1000;
  row.write_uint32(Uint32(interrupt_time));

  Uint64 exec_vm_time = cpuMeasurePtr.p->m_exec_vm_time;
  exec_vm_time = (exec_vm_time * elapsed_time) / 1000;
  row.write_uint32(Uint32(exec_vm_time));

  ndbinfo_send_row(signal, req, row, rl);
}

void Thrman::send_cpu_raw_measurement_row(DbinfoScanReq &req,
                                          Ndbinfo::Ratelimit &rl,
                                          Signal *signal,
                                          CPUMeasurementRecordPtr cpuMeasurePtr,
                                          Uint32 cpu_no, Uint32 measurement_id,
                                          Uint32 online) {
  Ndbinfo::Row row(signal, req);
  row.write_uint32(getOwnNodeId());
  row.write_uint32(measurement_id);
  row.write_uint32(cpu_no);
  row.write_uint32(online);

  row.write_uint32(Uint32(cpuMeasurePtr.p->m_user_time));
  row.write_uint32(Uint32(cpuMeasurePtr.p->m_idle_time));
  row.write_uint32(Uint32(cpuMeasurePtr.p->m_sys_time));
  row.write_uint32(Uint32(cpuMeasurePtr.p->m_interrupt_time));
  row.write_uint32(Uint32(cpuMeasurePtr.p->m_exec_vm_time));
  row.write_uint32(Uint32(cpuMeasurePtr.p->m_elapsed_time));

  ndbinfo_send_row(signal, req, row, rl);
}

void Thrman::execDBINFO_SCANREQ(Signal *signal) {
  jamEntry();

  DbinfoScanReq req = *(DbinfoScanReq *)signal->theData;
  const Ndbinfo::ScanCursor *cursor =
      CAST_CONSTPTR(Ndbinfo::ScanCursor, DbinfoScan::getCursorPtr(&req));
  Ndbinfo::Ratelimit rl;

  switch (req.tableId) {
    case Ndbinfo::HWINFO_TABLEID: {
      if (instance() == m_main_thrman_instance) {
        struct ndb_hwinfo *info = Ndb_GetHWInfo(false);
        Ndbinfo::Row row(signal, req);
        row.write_uint32(getOwnNodeId());
        row.write_uint32(info->cpu_cnt_max);
        row.write_uint32(info->cpu_cnt);
        row.write_uint32(info->num_cpu_cores);
        row.write_uint32(info->num_cpu_sockets);
        row.write_uint64(info->hw_memory_size);
        row.write_string(info->cpu_model_name);
        ndbinfo_send_row(signal, req, row, rl);
      }
      break;
    }
    case Ndbinfo::CPUINFO_TABLEID: {
      if (m_is_cpuinfo_available) {
        struct ndb_hwinfo *info = Ndb_GetHWInfo(false);
        Uint32 pos = cursor->data[0];
        for (;;) {
          Ndbinfo::Row row(signal, req);
          row.write_uint32(getOwnNodeId());
          row.write_uint32(info->cpu_info[pos].cpu_no);
          row.write_uint32(info->cpu_info[pos].online);
          row.write_uint32(info->cpu_info[pos].core_id);
          row.write_uint32(info->cpu_info[pos].socket_id);
          ndbinfo_send_row(signal, req, row, rl);
          pos++;
          if (pos == info->cpu_cnt_max) {
            break;
          }
          if (rl.need_break(req)) {
            jam();
            ndbinfo_send_scan_break(signal, req, rl, pos);
            return;
          }
        }
      }
      break;
    }
    case Ndbinfo::CPUDATA_TABLEID: {
      if (m_is_cpudata_available) {
        struct ndb_hwinfo *info = Ndb_GetHWInfo(false);
        Uint32 cpu_no = cursor->data[0];
        for (;;) {
          CPURecordPtr cpuPtr;
          CPUMeasurementRecordPtr cpuMeasurePtr;
          cpuPtr.i = cpu_no;
          c_CPURecordPool.getPtr(cpuPtr);
          {
            LocalDLCFifoList<CPUMeasurementRecord_pool> list(
                c_CPUMeasurementRecordPool, cpuPtr.p->m_next_1sec_measure);
            list.first(cpuMeasurePtr);
          }
          if (cpuMeasurePtr.p->m_first_measure_done) {
            send_cpu_measurement_row(req, rl, signal, cpuMeasurePtr, cpu_no,
                                     info->cpu_data[cpu_no].online);
          }
          cpu_no++;
          if (cpu_no == info->cpu_cnt_max) {
            break;
          }
          if (rl.need_break(req)) {
            jam();
            ndbinfo_send_scan_break(signal, req, rl, cpu_no);
            return;
          }
        }
      }
      break;
    }
    case Ndbinfo::CPUDATA_50MS_TABLEID:
    case Ndbinfo::CPUDATA_1SEC_TABLEID:
    case Ndbinfo::CPUDATA_20SEC_TABLEID: {
      if (m_is_cpudata_available) {
        struct ndb_hwinfo *info = Ndb_GetHWInfo(false);
        Uint32 cpu_no = cursor->data[0];
        for (;;) {
          CPURecordPtr cpuPtr;
          CPUMeasurementRecordPtr cpuMeasurePtr;
          cpuPtr.i = cpu_no;
          c_CPURecordPool.getPtr(cpuPtr);
          if (req.tableId == Ndbinfo::CPUDATA_50MS_TABLEID) {
            LocalDLCFifoList<CPUMeasurementRecord_pool> list(
                c_CPUMeasurementRecordPool, cpuPtr.p->m_next_50ms_measure);
            list.first(cpuMeasurePtr);
          } else if (req.tableId == Ndbinfo::CPUDATA_1SEC_TABLEID) {
            LocalDLCFifoList<CPUMeasurementRecord_pool> list(
                c_CPUMeasurementRecordPool, cpuPtr.p->m_next_1sec_measure);
            list.first(cpuMeasurePtr);
          } else if (req.tableId == Ndbinfo::CPUDATA_20SEC_TABLEID) {
            LocalDLCFifoList<CPUMeasurementRecord_pool> list(
                c_CPUMeasurementRecordPool, cpuPtr.p->m_next_20sec_measure);
            list.first(cpuMeasurePtr);
          } else {
            ndbabort();
          }
          Uint32 loop_count = 0;
          do {
            ndbrequire(loop_count < NUM_MEASUREMENTS);
            if (cpuMeasurePtr.p->m_first_measure_done) {
              send_cpu_raw_measurement_row(req, rl, signal, cpuMeasurePtr,
                                           cpu_no, loop_count,
                                           info->cpu_data[cpu_no].online);
            }
            if (req.tableId == Ndbinfo::CPUDATA_50MS_TABLEID) {
              LocalDLCFifoList<CPUMeasurementRecord_pool> list(
                  c_CPUMeasurementRecordPool, cpuPtr.p->m_next_50ms_measure);
              list.next(cpuMeasurePtr);
            } else if (req.tableId == Ndbinfo::CPUDATA_1SEC_TABLEID) {
              LocalDLCFifoList<CPUMeasurementRecord_pool> list(
                  c_CPUMeasurementRecordPool, cpuPtr.p->m_next_1sec_measure);
              list.next(cpuMeasurePtr);
            } else if (req.tableId == Ndbinfo::CPUDATA_20SEC_TABLEID) {
              LocalDLCFifoList<CPUMeasurementRecord_pool> list(
                  c_CPUMeasurementRecordPool, cpuPtr.p->m_next_20sec_measure);
              list.next(cpuMeasurePtr);
            } else {
              ndbabort();
            }
            loop_count++;
          } while (cpuMeasurePtr.i != RNIL);
          cpu_no++;
          if (cpu_no == info->cpu_cnt_max) {
            break;
          }
          if (rl.need_break(req)) {
            jam();
            ndbinfo_send_scan_break(signal, req, rl, cpu_no);
            return;
          }
        }
      }
      break;
    }
    case Ndbinfo::THREADS_TABLEID: {
      Uint32 pos = cursor->data[0];
      for (;;) {
        if (pos == 0) {
          jam();
          Ndbinfo::Row row(signal, req);
          row.write_uint32(getOwnNodeId());
          row.write_uint32(getThreadId());  // thr_no
          row.write_string(m_thread_name);
          row.write_string(m_thread_description);
          ndbinfo_send_row(signal, req, row, rl);
        }
        if (instance() != m_main_thrman_instance) {
          jam();
          break;
        }
        pos++;
        if (pos > m_num_send_threads) {
          jam();
          break;
        }
        {
          jam();
          Ndbinfo::Row row(signal, req);
          row.write_uint32(getOwnNodeId());
          row.write_uint32(m_num_threads + (pos - 1));  // thr_no
          row.write_string(m_send_thread_name);
          row.write_string(m_send_thread_description);
          ndbinfo_send_row(signal, req, row, rl);
        }

        if (pos >= m_num_send_threads) {
          jam();
          break;
        }

        if (rl.need_break(req)) {
          jam();
          ndbinfo_send_scan_break(signal, req, rl, pos);
          return;
        }
      }
      break;
    }
    case Ndbinfo::THREADBLOCKS_TABLEID: {
      Uint32 arr[MAX_INSTANCES_PER_THREAD];
      Uint32 len = mt_get_blocklist(this, arr, NDB_ARRAY_SIZE(arr));
      Uint32 pos = cursor->data[0];
      ndbrequire(pos < NDB_ARRAY_SIZE(arr));
      for (;;) {
        Ndbinfo::Row row(signal, req);
        row.write_uint32(getOwnNodeId());
        row.write_uint32(getThreadId());              // thr_no
        row.write_uint32(blockToMain(arr[pos]));      // block_number
        row.write_uint32(blockToInstance(arr[pos]));  // block_instance
        ndbinfo_send_row(signal, req, row, rl);

        pos++;
        if (pos == len) {
          jam();
          break;
        } else if (rl.need_break(req)) {
          jam();
          ndbinfo_send_scan_break(signal, req, rl, pos);
          return;
        }
      }
      break;
    }
    case Ndbinfo::THREADBLOCK_DETAILS_TABLEID: {
      Uint32 arr[MAX_INSTANCES_PER_THREAD];
      Uint32 len = mt_get_blocklist(this, arr, NDB_ARRAY_SIZE(arr));
      Uint32 pos = cursor->data[0];
      ndbrequire(pos < NDB_ARRAY_SIZE(arr));
      for (;;) {
        Ndbinfo::Row row(signal, req);
        row.write_uint32(getOwnNodeId());
        row.write_uint32(getThreadId());              // thr_no
        row.write_uint32(blockToMain(arr[pos]));      // block_number
        row.write_uint32(blockToInstance(arr[pos]));  // block_instance
#if defined(ERROR_INSERT)
        SimulatedBlock *block = globalData.getBlockInstance(arr[pos]);
        row.write_uint32(block->getErrorInsertValue());  // error_insert_value
        row.write_uint32(block->getErrorInsertExtra());  // error_insert_extra
#else
        row.write_null();  // error_insert_value
        row.write_null();  // error_insert_extra
#endif
        ndbinfo_send_row(signal, req, row, rl);

        pos++;
        if (pos == len) {
          jam();
          break;
        } else if (rl.need_break(req)) {
          jam();
          ndbinfo_send_scan_break(signal, req, rl, pos);
          return;
        }
      }
      break;
    }
    case Ndbinfo::THREADSTAT_TABLEID: {
      ndb_thr_stat stat;
      mt_get_thr_stat(this, &stat);
      Ndbinfo::Row row(signal, req);
      row.write_uint32(getOwnNodeId());
      row.write_uint32(getThreadId());  // thr_no
      row.write_string(stat.name);
      row.write_uint64(stat.loop_cnt);
      row.write_uint64(stat.exec_cnt);
      row.write_uint64(stat.wait_cnt);
      row.write_uint64(stat.local_sent_prioa);
      row.write_uint64(stat.local_sent_priob);
      row.write_uint64(stat.remote_sent_prioa);
      row.write_uint64(stat.remote_sent_priob);

      row.write_uint64(stat.os_tid);
      row.write_uint64(NdbTick_CurrentMillisecond());

      struct ndb_rusage os_rusage;
      Ndb_GetRUsage(&os_rusage, false);
      row.write_uint64(os_rusage.ru_utime);
      row.write_uint64(os_rusage.ru_stime);
      row.write_uint64(os_rusage.ru_minflt);
      row.write_uint64(os_rusage.ru_majflt);
      row.write_uint64(os_rusage.ru_nvcsw);
      row.write_uint64(os_rusage.ru_nivcsw);
      ndbinfo_send_row(signal, req, row, rl);
      break;
    }
    case Ndbinfo::CPUSTAT_50MS_TABLEID:
    case Ndbinfo::CPUSTAT_1SEC_TABLEID:
    case Ndbinfo::CPUSTAT_20SEC_TABLEID: {
      Uint32 pos = cursor->data[0];

      SendThreadMeasurementPtr sendThreadMeasurementPtr;
      MeasurementRecordPtr measurePtr;

      for (;;) {
        jam();
        Uint32 pos_thread_id = ((pos >> 8) & 255);
        Uint32 pos_index = (pos & 255);
        Uint32 pos_ptrI = (pos >> 16);
        sendThreadMeasurementPtr.i = RNIL;
        sendThreadMeasurementPtr.p = NULL;
        measurePtr.i = RNIL;
        measurePtr.p = NULL;
        if (pos_index >= NUM_MEASUREMENTS) {
          jam();
          ndbassert(false);
          g_eventLogger->info("pos_index out of range in ndbinfo table %u",
                              req.tableId);
          ndbinfo_send_scan_conf(signal, req, rl);
          return;
        }

        if (pos == 0) {
          /**
           * This is the first row to start. We start with the rows from our
           * own thread. The pos variable is divided in 3 fields.
           * Bit 0-7 contains index number from 0 up to 19.
           * Bit 8-15 contains thread number
           * Bit 16-31 is a pointer to the next SendThreadMeasurement record.
           *
           * Thread number 0 is our own thread always. Thread 1 is send thread
           * instance 0 and thread 2 send thread instance 1 and so forth. We
           * will only worry about send thread data in the main thread where
           * we keep track of this information.
           *
           * The latest measurement is at the end of the linked list and so we
           * proceed backwards in the list.
           */
          if (req.tableId == Ndbinfo::CPUSTAT_50MS_TABLEID) {
            jam();
            c_next_50ms_measure.last(measurePtr);
          } else if (req.tableId == Ndbinfo::CPUSTAT_1SEC_TABLEID) {
            jam();
            c_next_1sec_measure.last(measurePtr);
          } else if (req.tableId == Ndbinfo::CPUSTAT_20SEC_TABLEID) {
            jam();
            c_next_20sec_measure.last(measurePtr);
          } else {
            ndbabort();
            return;
          }
          /* Start at index 0, thread 0, measurePtr.i */
          pos = measurePtr.i << 16;
        } else if (pos_thread_id != 0) {
          /**
           * We are working on the send thread measurement as we are the
           * main thread.
           */
          jam();
          if (instance() != m_main_thrman_instance) {
            g_eventLogger->info("pos_thread_id = %u in non-main thread",
                                pos_thread_id);
            ndbassert(false);
            ndbinfo_send_scan_conf(signal, req, rl);
            return;
          }
          ndbrequire(c_sendThreadMeasurementPool.getPtr(
              sendThreadMeasurementPtr, pos_ptrI));
        } else {
          jam();
          ndbrequire(c_measurementRecordPool.getPtr(measurePtr, pos_ptrI));
        }

        Ndbinfo::Row row(signal, req);
        if (pos_thread_id == 0 && measurePtr.p->m_first_measure_done) {
          jam();
          /**
           * We report buffer_full_time, spin_time and exec_time as
           * separate times. So exec time does not include buffer_full_time
           * when we report it to the user and it also does not include
           * spin time when we report it to the user and finally it does
           * also not include send time of the thread. So essentially
           * the sum of exec_time, sleep_time, spin_time, send_time and
           * buffer_full_time should be very close to the elapsed time.
           */
          Uint32 exec_time = measurePtr.p->m_exec_time_thread;
          Uint32 spin_time = measurePtr.p->m_spin_time_thread;
          Uint32 buffer_full_time = measurePtr.p->m_buffer_full_time_thread;
          Uint32 send_time = measurePtr.p->m_send_time_thread;

          if (exec_time < (buffer_full_time + send_time + spin_time)) {
            exec_time = 0;
          } else {
            exec_time -= buffer_full_time;
            exec_time -= spin_time;
            exec_time -= send_time;
          }
          row.write_uint32(getOwnNodeId());
          row.write_uint32(getThreadId());
          row.write_uint32(Uint32(measurePtr.p->m_user_time_os));
          row.write_uint32(Uint32(measurePtr.p->m_kernel_time_os));
          row.write_uint32(Uint32(measurePtr.p->m_idle_time_os));
          row.write_uint32(Uint32(exec_time));
          row.write_uint32(Uint32(measurePtr.p->m_sleep_time_thread));
          row.write_uint32(Uint32(measurePtr.p->m_spin_time_thread));
          row.write_uint32(Uint32(measurePtr.p->m_send_time_thread));
          row.write_uint32(Uint32(measurePtr.p->m_buffer_full_time_thread));
          row.write_uint32(Uint32(measurePtr.p->m_elapsed_time));
          ndbinfo_send_row(signal, req, row, rl);
        } else if (pos_thread_id != 0 &&
                   sendThreadMeasurementPtr.p->m_first_measure_done) {
          jam();
          row.write_uint32(getOwnNodeId());
          row.write_uint32(m_num_threads + (pos_thread_id - 1));

          Uint32 exec_time = sendThreadMeasurementPtr.p->m_exec_time;
          Uint32 sleep_time = sendThreadMeasurementPtr.p->m_sleep_time;

          row.write_uint32(Uint32(sendThreadMeasurementPtr.p->m_user_time_os));
          row.write_uint32(
              Uint32(sendThreadMeasurementPtr.p->m_kernel_time_os));
          row.write_uint32(Uint32(sendThreadMeasurementPtr.p->m_idle_time_os));
          row.write_uint32(exec_time);
          row.write_uint32(sleep_time);
          row.write_uint32(0);
          row.write_uint32(exec_time);
          row.write_uint32(Uint32(0));
          Uint32 elapsed_time = sendThreadMeasurementPtr.p->m_exec_time +
                                sendThreadMeasurementPtr.p->m_sleep_time;
          row.write_uint32(elapsed_time);
          ndbinfo_send_row(signal, req, row, rl);
        } else {
          // Proceed to next thread at first undone measurement
          pos_index = NUM_MEASUREMENTS - 1;
        }

        if ((pos_index + 1) == NUM_MEASUREMENTS) {
          /**
           * We are done with this thread, we need to either move on to next
           * send thread or stop.
           */
          if (instance() != m_main_thrman_instance) {
            jam();
            break;
          }
          /* This check will also ensure that we break without send threads */
          if (pos_thread_id == m_num_send_threads) {
            jam();
            break;
          }
          jam();
          pos_thread_id++;
          SendThreadPtr sendThreadPtr;
          ndbrequire(
              c_sendThreadRecordPool.getPtr(sendThreadPtr, pos_thread_id - 1));

          if (req.tableId == Ndbinfo::CPUSTAT_50MS_TABLEID) {
            jam();
            Local_SendThreadMeasurement_fifo list_50ms(
                c_sendThreadMeasurementPool,
                sendThreadPtr.p->m_send_thread_50ms_measurements);
            list_50ms.last(sendThreadMeasurementPtr);
          } else if (req.tableId == Ndbinfo::CPUSTAT_1SEC_TABLEID) {
            jam();
            Local_SendThreadMeasurement_fifo list_1sec(
                c_sendThreadMeasurementPool,
                sendThreadPtr.p->m_send_thread_1sec_measurements);
            list_1sec.last(sendThreadMeasurementPtr);
          } else if (req.tableId == Ndbinfo::CPUSTAT_20SEC_TABLEID) {
            jam();
            Local_SendThreadMeasurement_fifo list_20sec(
                c_sendThreadMeasurementPool,
                sendThreadPtr.p->m_send_thread_20sec_measurements);
            list_20sec.last(sendThreadMeasurementPtr);
          } else {
            ndbabort();
            return;
          }

          pos = (sendThreadMeasurementPtr.i << 16) + (pos_thread_id << 8) + 0;
        } else if (pos_thread_id == 0) {
          if (measurePtr.i == RNIL) {
            jam();
            g_eventLogger->info("measurePtr.i = RNIL");
            ndbassert(false);
            ndbinfo_send_scan_conf(signal, req, rl);
            return;
          }
          if (req.tableId == Ndbinfo::CPUSTAT_50MS_TABLEID) {
            jam();
            c_next_50ms_measure.prev(measurePtr);
            if (measurePtr.i == RNIL) {
              jam();
              c_next_50ms_measure.first(measurePtr);
            }
          } else if (req.tableId == Ndbinfo::CPUSTAT_1SEC_TABLEID) {
            jam();
            c_next_1sec_measure.prev(measurePtr);
            if (measurePtr.i == RNIL) {
              jam();
              c_next_1sec_measure.first(measurePtr);
            }
          } else if (req.tableId == Ndbinfo::CPUSTAT_20SEC_TABLEID) {
            jam();
            c_next_20sec_measure.prev(measurePtr);
            if (measurePtr.i == RNIL) {
              jam();
              c_next_20sec_measure.first(measurePtr);
            }
          } else {
            ndbabort();
            return;
          }
          pos = (measurePtr.i << 16) + (0 << 8) + pos_index + 1;
        } else {
          SendThreadPtr sendThreadPtr;
          ndbrequire(
              c_sendThreadRecordPool.getPtr(sendThreadPtr, pos_thread_id - 1));

          ndbrequire(sendThreadMeasurementPtr.i != RNIL);
          if (req.tableId == Ndbinfo::CPUSTAT_50MS_TABLEID) {
            Local_SendThreadMeasurement_fifo list_50ms(
                c_sendThreadMeasurementPool,
                sendThreadPtr.p->m_send_thread_50ms_measurements);
            list_50ms.prev(sendThreadMeasurementPtr);
            if (sendThreadMeasurementPtr.i == RNIL) {
              jam();
              list_50ms.first(sendThreadMeasurementPtr);
            }
          } else if (req.tableId == Ndbinfo::CPUSTAT_1SEC_TABLEID) {
            Local_SendThreadMeasurement_fifo list_1sec(
                c_sendThreadMeasurementPool,
                sendThreadPtr.p->m_send_thread_1sec_measurements);
            list_1sec.prev(sendThreadMeasurementPtr);
            if (sendThreadMeasurementPtr.i == RNIL) {
              jam();
              list_1sec.first(sendThreadMeasurementPtr);
            }
          } else if (req.tableId == Ndbinfo::CPUSTAT_20SEC_TABLEID) {
            Local_SendThreadMeasurement_fifo list_20sec(
                c_sendThreadMeasurementPool,
                sendThreadPtr.p->m_send_thread_20sec_measurements);
            list_20sec.prev(sendThreadMeasurementPtr);
            if (sendThreadMeasurementPtr.i == RNIL) {
              jam();
              list_20sec.first(sendThreadMeasurementPtr);
            }
          } else {
            ndbabort();
            return;
          }
          pos = (sendThreadMeasurementPtr.i << 16) + (pos_thread_id << 8) +
                pos_index + 1;
        }

        if (rl.need_break(req)) {
          jam();
          ndbinfo_send_scan_break(signal, req, rl, pos);
          return;
        }
      }
      break;
    }
    case Ndbinfo::CPUSTAT_TABLEID: {
      Uint32 pos = cursor->data[0];

      SendThreadMeasurementPtr sendThreadMeasurementPtr;
      MeasurementRecordPtr measurePtr;

      for (;;) {
        if (pos == 0) {
          jam();
          MeasurementRecord measure;
          bool success = calculate_cpu_load_last_second(&measure);
          ndbrequire(success);
          Ndbinfo::Row row(signal, req);
          row.write_uint32(getOwnNodeId());
          row.write_uint32(getThreadId());

          if (measure.m_elapsed_time) {
            jam();
            Uint64 user_os_percentage =
                ((Uint64(100) * measure.m_user_time_os) + Uint64(500 * 1000)) /
                measure.m_elapsed_time;

            Uint64 kernel_percentage =
                ((Uint64(100) * measure.m_kernel_time_os) +
                 Uint64(500 * 1000)) /
                measure.m_elapsed_time;

            /* Ensure that total percentage reported is always 100% */
            if (user_os_percentage + kernel_percentage > Uint64(100)) {
              kernel_percentage = Uint64(100) - user_os_percentage;
            }
            Uint64 idle_os_percentage =
                Uint64(100) - (user_os_percentage + kernel_percentage);
            row.write_uint32(Uint32(user_os_percentage));
            row.write_uint32(Uint32(kernel_percentage));
            row.write_uint32(Uint32(idle_os_percentage));

            Uint64 exec_time = measure.m_exec_time_thread;
            Uint64 spin_time = measure.m_spin_time_thread;
            Uint64 buffer_full_time = measure.m_buffer_full_time_thread;
            Uint64 send_time = measure.m_send_time_thread;

            Uint64 non_exec_time = spin_time + send_time + buffer_full_time;
            if (unlikely(non_exec_time > exec_time)) {
              exec_time = 0;
            } else {
              exec_time -= non_exec_time;
            }

            Uint64 exec_percentage =
                ((Uint64(100) * exec_time) + Uint64(500 * 1000)) /
                measure.m_elapsed_time;

            Uint64 spin_percentage =
                ((Uint64(100) * spin_time) + Uint64(500 * 1000)) /
                measure.m_elapsed_time;

            Uint64 send_percentage =
                ((Uint64(100) * send_time) + Uint64(500 * 1000)) /
                measure.m_elapsed_time;

            Uint64 buffer_full_percentage =
                ((Uint64(100) * buffer_full_time) + Uint64(500 * 1000)) /
                measure.m_elapsed_time;

            /* Ensure that total percentage reported is always 100% */
            Uint64 exec_full_percentage =
                exec_percentage + buffer_full_percentage;
            Uint64 exec_full_send_percentage =
                exec_percentage + buffer_full_percentage + send_percentage;
            Uint64 all_exec_percentage = exec_percentage +
                                         buffer_full_percentage +
                                         send_percentage + spin_percentage;
            Uint64 sleep_percentage = 0;
            if (buffer_full_percentage > Uint64(100)) {
              buffer_full_percentage = Uint64(100);
              exec_percentage = 0;
              send_percentage = 0;
              spin_percentage = 0;
            } else if (exec_full_percentage > Uint64(100)) {
              exec_percentage = Uint64(100) - buffer_full_percentage;
              send_percentage = 0;
              spin_percentage = 0;
            } else if (exec_full_send_percentage > Uint64(100)) {
              exec_percentage = Uint64(100) - exec_full_percentage;
              spin_percentage = 0;
            } else if (all_exec_percentage > Uint64(100)) {
              exec_percentage = Uint64(100) - exec_full_send_percentage;
            } else {
              sleep_percentage = Uint64(100) - all_exec_percentage;
            }
            ndbrequire(exec_percentage + buffer_full_percentage +
                           send_percentage + spin_percentage +
                           sleep_percentage ==
                       Uint64(100));

            row.write_uint32(Uint32(exec_percentage));
            row.write_uint32(Uint32(sleep_percentage));
            row.write_uint32(Uint32(spin_percentage));
            row.write_uint32(Uint32(send_percentage));
            row.write_uint32(Uint32(buffer_full_percentage));

            row.write_uint32(Uint32(measure.m_elapsed_time));
          } else {
            jam();
            row.write_uint32(0);
            row.write_uint32(0);
            row.write_uint32(0);
            row.write_uint32(0);
            row.write_uint32(0);
            row.write_uint32(0);
            row.write_uint32(0);
            row.write_uint32(0);
            row.write_uint32(0);
            row.write_uint32(0);
          }

          ndbinfo_send_row(signal, req, row, rl);
          if (instance() != m_main_thrman_instance || m_num_send_threads == 0) {
            jam();
            break;
          }
          pos++;
        } else {
          /* Send thread CPU load */
          jam();
          if ((pos - 1) >= m_num_send_threads) {
            jam();
            g_eventLogger->info("send instance out of range");
            ndbassert(false);
            ndbinfo_send_scan_conf(signal, req, rl);
            return;
          }
          SendThreadMeasurement measure;
          bool success =
              calculate_send_thread_load_last_second(pos - 1, &measure);
          if (!success) {
            g_eventLogger->info(
                "Failed calculate_send_thread_load_last_second");
            ndbassert(false);
            ndbinfo_send_scan_conf(signal, req, rl);
            return;
          }
          Ndbinfo::Row row(signal, req);
          row.write_uint32(getOwnNodeId());
          row.write_uint32(m_num_threads + (pos - 1));

          if (measure.m_elapsed_time_os == 0) {
            jam();
            row.write_uint32(0);
            row.write_uint32(0);
            row.write_uint32(0);
          } else {
            Uint64 user_time_os_percentage =
                ((Uint64(100) * measure.m_user_time_os) + Uint64(500 * 1000)) /
                measure.m_elapsed_time_os;

            row.write_uint32(Uint32(user_time_os_percentage));

            Uint64 kernel_time_os_percentage =
                ((Uint64(100) * measure.m_kernel_time_os) +
                 Uint64(500 * 1000)) /
                measure.m_elapsed_time_os;

            row.write_uint32(Uint32(kernel_time_os_percentage));

            Uint64 idle_time_os_percentage =
                ((Uint64(100) * measure.m_idle_time_os) + Uint64(500 * 1000)) /
                measure.m_elapsed_time_os;

            row.write_uint32(Uint32(idle_time_os_percentage));
          }

          if (measure.m_elapsed_time > 0) {
            Uint64 exec_time = measure.m_exec_time;
            Uint64 spin_time = measure.m_spin_time;
            Uint64 sleep_time = measure.m_sleep_time;

            exec_time -= spin_time;

            Uint64 exec_percentage =
                ((Uint64(100) * exec_time) + Uint64(500 * 1000)) /
                measure.m_elapsed_time;

            Uint64 sleep_percentage =
                ((Uint64(100) * sleep_time) + Uint64(500 * 1000)) /
                measure.m_elapsed_time;

            Uint64 spin_percentage =
                ((Uint64(100) * spin_time) + Uint64(500 * 1000)) /
                measure.m_elapsed_time;

            row.write_uint32(Uint32(exec_percentage));
            row.write_uint32(Uint32(sleep_percentage));
            row.write_uint32(Uint32(spin_percentage));
            row.write_uint32(Uint32(exec_percentage));
            row.write_uint32(Uint32(0));
            row.write_uint32(Uint32(measure.m_elapsed_time));
          } else {
            jam();
            row.write_uint32(0);
            row.write_uint32(0);
            row.write_uint32(0);
            row.write_uint32(0);
            row.write_uint32(0);
            row.write_uint32(0);
          }
          ndbinfo_send_row(signal, req, row, rl);

          if (pos == m_num_send_threads) {
            jam();
            break;
          }
          pos++;
        }
        if (rl.need_break(req)) {
          jam();
          ndbinfo_send_scan_break(signal, req, rl, pos);
          return;
        }
      }
      break;
    }
    default:
      break;
  }

  ndbinfo_send_scan_conf(signal, req, rl);
}

static void release_wait_freeze() {
  NdbMutex_Lock(g_freeze_mutex);
  g_freeze_waiters--;
  if (g_freeze_waiters == 0) {
    g_freeze_wakeup = false;
  }
  NdbMutex_Unlock(g_freeze_mutex);
}

static Uint32 check_freeze_waiters() {
  NdbMutex_Lock(g_freeze_mutex);
  Uint32 sync_waiters = g_freeze_waiters;
  NdbMutex_Unlock(g_freeze_mutex);
  return sync_waiters;
}

void Thrman::execFREEZE_THREAD_REQ(Signal *signal) {
  FreezeThreadReq *req = (FreezeThreadReq *)&signal->theData[0];
  m_freeze_req = *req;
  /**
   * We are requested to stop executing in this thread here. When all
   * threads have stopped here we are ready to perform the change
   * operation requested.
   *
   * The current change operations supported are:
   * Switch between inactive transporters and active transporters.
   * This is used when we increase the number of transporters on a link
   * from a single transporter to multiple transporters sharing the
   * load. It is important synchronize this to ensure that signals
   * continue to arrive to the destination threads in signal order.
   */
  if (instance() != m_main_thrman_instance) {
    flush_send_buffers();
    wait_freeze(false);
    return;
  }
  wait_freeze(true);
  wait_all_stop(signal);
}

void Thrman::wait_freeze(bool ret) {
  NdbMutex_Lock(g_freeze_mutex);
  g_freeze_waiters++;
  if (ret) {
    NdbMutex_Unlock(g_freeze_mutex);
    jam();
    return;
  }
  while (true) {
    NdbCondition_WaitTimeout(g_freeze_condition, g_freeze_mutex, 10);
    set_watchdog_counter();
    if (g_freeze_wakeup) {
      g_freeze_waiters--;
      if (g_freeze_waiters == 0) {
        g_freeze_wakeup = false;
      }
      NdbMutex_Unlock(g_freeze_mutex);
      jam();
      return;
    }
  }
  return;
}

void Thrman::wait_all_stop(Signal *signal) {
  if (check_freeze_waiters() == m_num_threads) {
    jam();
    FreezeActionReq *req = CAST_PTR(FreezeActionReq, signal->getDataPtrSend());
    BlockReference ref = m_freeze_req.senderRef;
    req->nodeId = m_freeze_req.nodeId;
    req->senderRef = reference();
    sendSignal(ref, GSN_FREEZE_ACTION_REQ, signal,
               FreezeActionReq::SignalLength, JBA);
    return;
  }
  signal->theData[0] = ZWAIT_ALL_STOP;
  sendSignal(reference(), GSN_CONTINUEB, signal, 1, JBB);
}

void Thrman::execFREEZE_ACTION_CONF(Signal *signal) {
  /**
   * The action is performed, we have completed this action.
   * We can now release all threads and ensure that they are
   * woken up again. We wait for all threads to wakeup before
   * we proceed to ensure that the functionality is available
   * for a new synchronize action.
   */
  NdbMutex_Lock(g_freeze_mutex);
  g_freeze_wakeup = true;
  NdbCondition_Broadcast(g_freeze_condition);
  NdbMutex_Unlock(g_freeze_mutex);
  release_wait_freeze();
  wait_all_start(signal);
}

void Thrman::wait_all_start(Signal *signal) {
  if (check_freeze_waiters() == 0) {
    jam();
    FreezeThreadConf *conf =
        CAST_PTR(FreezeThreadConf, signal->getDataPtrSend());
    BlockReference ref = m_freeze_req.senderRef;
    conf->nodeId = m_freeze_req.nodeId;
    sendSignal(ref, GSN_FREEZE_THREAD_CONF, signal,
               FreezeThreadConf::SignalLength, JBA);
    return;
  }
  signal->theData[0] = ZWAIT_ALL_START;
  sendSignal(reference(), GSN_CONTINUEB, signal, 1, JBB);
}

void Thrman::execDUMP_STATE_ORD(Signal *signal) {
  DumpStateOrd *const &dumpState = (DumpStateOrd *)&signal->theData[0];
  Uint32 arg = dumpState->args[0];
  Uint32 val1 = dumpState->args[1];
  if (arg == DumpStateOrd::SetSchedulerSpinTimerAll) {
    if (signal->length() != 2) {
      if (instance() == m_main_thrman_instance) {
        g_eventLogger->info("Use: DUMP 104000 spintime");
      }
      return;
    }
    set_configured_spintime(val1, false);
  } else if (arg == DumpStateOrd::SetSchedulerSpinTimerThread) {
    if (signal->length() != 3) {
      if (instance() == m_main_thrman_instance) {
        g_eventLogger->info("Use: DUMP 104001 thr_no spintime");
      }
      return;
    }
    Uint32 val2 = dumpState->args[2];
    if (val1 + 1 == instance()) {
      jam();
      set_configured_spintime(val2, true);
    }
  } else if (arg == DumpStateOrd::SetAllowedSpinOverhead) {
    if (signal->length() != 2) {
      if (instance() == m_main_thrman_instance) {
        g_eventLogger->info("Use: DUMP 104002 AllowedSpinOverhead");
      }
      return;
    }
    set_allowed_spin_overhead(val1);
  } else if (arg == DumpStateOrd::SetSpintimePerCall) {
    if (signal->length() != 2) {
      if (instance() == m_main_thrman_instance) {
        g_eventLogger->info("Use: DUMP 104003 SpintimePerCall");
      }
      return;
    }
    set_spintime_per_call(val1);
  } else if (arg == DumpStateOrd::EnableAdaptiveSpinning) {
    if (signal->length() != 2) {
      if (instance() == m_main_thrman_instance) {
        g_eventLogger->info(
            "Use: DUMP 104004 0/1"
            " (Enable/Disable Adaptive Spinning");
      }
      return;
    }
    set_enable_adaptive_spinning(val1 != 0);
  }
  return;
}

ThrmanProxy::ThrmanProxy(Block_context &ctx) : LocalProxy(THRMAN, ctx) {
  addRecSignal(GSN_FREEZE_THREAD_REQ, &ThrmanProxy::execFREEZE_THREAD_REQ);
}

ThrmanProxy::~ThrmanProxy() {}

SimulatedBlock *ThrmanProxy::newWorker(Uint32 instanceNo) {
  return new Thrman(m_ctx, instanceNo);
}

BLOCK_FUNCTIONS(ThrmanProxy)

void ThrmanProxy::execFREEZE_THREAD_REQ(Signal *signal) {
  /**
   * This signal is always sent from the main thread. Thus we should not
   * send the signal to the first instance in THRMAN which is the main
   * thread since this would block the main thread from moving forward.
   *
   * The work to be done is done by the main thread, the other threads
   * only need to stop and wait to be woken up again to proceed with
   * normal processing.
   */
  for (Uint32 i = 0; i < c_workers; i++) {
    jam();
    Uint32 ref = numberToRef(number(), workerInstance(i), getOwnNodeId());
    sendSignal(ref, GSN_FREEZE_THREAD_REQ, signal, signal->getLength(), JBA);
  }
}
