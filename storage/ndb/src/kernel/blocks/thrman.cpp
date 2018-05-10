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

#include "thrman.hpp"
#include <mt.hpp>
#include <signaldata/DbinfoScan.hpp>

#include <EventLogger.hpp>

#define JAM_FILE_ID 440

#define MAIN_THRMAN_INSTANCE 1
#define NUM_MEASUREMENTS 20
#define NUM_MEASUREMENT_RECORDS (3 * NUM_MEASUREMENTS)

//define HIGH_DEBUG_CPU_USAGE 1
//#define DEBUG_CPU_USAGE 1
extern EventLogger * g_eventLogger;

Thrman::Thrman(Block_context & ctx, Uint32 instanceno) :
  SimulatedBlock(THRMAN, ctx, instanceno),
  c_next_50ms_measure(c_measurementRecordPool),
  c_next_1sec_measure(c_measurementRecordPool),
  c_next_20sec_measure(c_measurementRecordPool)
{
  BLOCK_CONSTRUCTOR(Thrman);

  addRecSignal(GSN_DBINFO_SCANREQ, &Thrman::execDBINFO_SCANREQ);
  addRecSignal(GSN_CONTINUEB, &Thrman::execCONTINUEB);
  addRecSignal(GSN_GET_CPU_USAGE_REQ, &Thrman::execGET_CPU_USAGE_REQ);
  addRecSignal(GSN_OVERLOAD_STATUS_REP, &Thrman::execOVERLOAD_STATUS_REP);
  addRecSignal(GSN_NODE_OVERLOAD_STATUS_ORD, &Thrman::execNODE_OVERLOAD_STATUS_ORD);
  addRecSignal(GSN_READ_CONFIG_REQ, &Thrman::execREAD_CONFIG_REQ);
  addRecSignal(GSN_SEND_THREAD_STATUS_REP, &Thrman::execSEND_THREAD_STATUS_REP);
  addRecSignal(GSN_SET_WAKEUP_THREAD_ORD, &Thrman::execSET_WAKEUP_THREAD_ORD);
  addRecSignal(GSN_WAKEUP_THREAD_ORD, &Thrman::execWAKEUP_THREAD_ORD);
  addRecSignal(GSN_SEND_WAKEUP_THREAD_ORD, &Thrman::execSEND_WAKEUP_THREAD_ORD);
  addRecSignal(GSN_STTOR, &Thrman::execSTTOR);
}

Thrman::~Thrman()
{
}

BLOCK_FUNCTIONS(Thrman)

void Thrman::mark_measurements_not_done()
{
  MeasurementRecordPtr measurePtr;
  jam();
  c_next_50ms_measure.first(measurePtr);
  while (measurePtr.i != RNIL)
  {
    measurePtr.p->m_first_measure_done = false;
    c_next_50ms_measure.next(measurePtr);
  }
  c_next_1sec_measure.first(measurePtr);
  while (measurePtr.i != RNIL)
  {
    measurePtr.p->m_first_measure_done = false;
    c_next_1sec_measure.next(measurePtr);
  }
  c_next_20sec_measure.first(measurePtr);
  while (measurePtr.i != RNIL)
  {
    measurePtr.p->m_first_measure_done = false;
    c_next_20sec_measure.next(measurePtr);
  }
}

void Thrman::execREAD_CONFIG_REQ(Signal *signal)
{
  jamEntry();

  /* Receive signal */
  const ReadConfigReq * req = (ReadConfigReq*)signal->getDataPtr();
  Uint32 ref = req->senderRef;
  Uint32 senderData = req->senderData;

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
  if (instance() == MAIN_THRMAN_INSTANCE)
  {
    jam();
    c_sendThreadRecordPool.setSize(m_num_send_threads);
    c_sendThreadMeasurementPool.setSize(NUM_MEASUREMENT_RECORDS *
                                        m_num_send_threads);
  }
  else
  {
    jam();
    c_sendThreadRecordPool.setSize(0);
    c_sendThreadMeasurementPool.setSize(0);
  }

  /* Create the 3 lists with 20 records in each. */
  MeasurementRecordPtr measurePtr;
  for (Uint32 i = 0; i < NUM_MEASUREMENTS; i++)
  {
    jam();
    c_measurementRecordPool.seize(measurePtr);
    c_next_50ms_measure.addFirst(measurePtr);
    c_measurementRecordPool.seize(measurePtr);
    c_next_1sec_measure.addFirst(measurePtr);
    c_measurementRecordPool.seize(measurePtr);
    c_next_20sec_measure.addFirst(measurePtr);
  }
  if (instance() == MAIN_THRMAN_INSTANCE)
  {
    jam();
    for (Uint32 send_instance = 0;
         send_instance < m_num_send_threads;
         send_instance++)
    {
      jam();
      SendThreadPtr sendThreadPtr;
      c_sendThreadRecordPool.seizeId(sendThreadPtr, send_instance);
      sendThreadPtr.p->m_send_thread_50ms_measurements.init();
      sendThreadPtr.p->m_send_thread_1sec_measurements.init();
      sendThreadPtr.p->m_send_thread_20sec_measurements.init();

      for (Uint32 i = 0; i < NUM_MEASUREMENTS; i++)
      {
        jam();
        SendThreadMeasurementPtr sendThreadMeasurementPtr;

        c_sendThreadMeasurementPool.seize(sendThreadMeasurementPtr);
        {
          jam();
          Local_SendThreadMeasurement_fifo list_50ms(
            c_sendThreadMeasurementPool,
            sendThreadPtr.p->m_send_thread_50ms_measurements);
          list_50ms.addFirst(sendThreadMeasurementPtr);
        }

        c_sendThreadMeasurementPool.seize(sendThreadMeasurementPtr);
        {
          jam();
          Local_SendThreadMeasurement_fifo list_1sec(
            c_sendThreadMeasurementPool,
            sendThreadPtr.p->m_send_thread_1sec_measurements);
          list_1sec.addFirst(sendThreadMeasurementPtr);
        }

        c_sendThreadMeasurementPool.seize(sendThreadMeasurementPtr);
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
  m_thread_name = getThreadName();
  m_thread_description = getThreadDescription();
  m_send_thread_name = "send";
  m_send_thread_description = "Send thread";

  /* Send return signal */
  ReadConfigConf * conf = (ReadConfigConf*)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(ref, GSN_READ_CONFIG_CONF, signal,
             ReadConfigConf::SignalLength, JBB);
}

void
Thrman::execSTTOR(Signal *signal)
{
  int res;
  jamEntry();

  const Uint32 startPhase  = signal->theData[1];

  switch (startPhase)
  {
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

    for (Uint32 i = 0; i < MAX_BLOCK_THREADS + 1; i++)
    {
      m_thread_overload_status[i].overload_status =
        (OverloadStatus)MEDIUM_LOAD_CONST;
      m_thread_overload_status[i].wakeup_instance = 0;
    }

    /* Initialise measurements */
    res = Ndb_GetRUsage(&m_last_50ms_rusage);
    if (res == 0)
    {
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

    if (instance() == MAIN_THRMAN_INSTANCE)
    {
      jam();
      for (Uint32 send_instance = 0;
           send_instance < m_num_send_threads;
           send_instance++)
      {
        jam();
        SendThreadPtr sendThreadPtr;
        c_sendThreadRecordPool.getPtr(sendThreadPtr, send_instance);
        Uint64 send_exec_time;
        Uint64 send_sleep_time;
        Uint64 send_spin_time;
        Uint64 send_user_time_os;
        Uint64 send_kernel_time_os;
        Uint64 send_elapsed_time_os;
        getSendPerformanceTimers(send_instance,
                                 send_exec_time,
                                 send_user_time_os,
                                 send_sleep_time,
                                 send_spin_time,
                                 send_kernel_time_os,
                                 send_elapsed_time_os);

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
    sendNextCONTINUEB(signal);
    break;
  default:
    ndbabort();
  }
  sendSTTORRY(signal);
}

void
Thrman::sendSTTORRY(Signal* signal)
{
  signal->theData[0] = 0;
  signal->theData[3] = 1;
  signal->theData[4] = 255; // No more start phases from missra
  BlockReference cntrRef = !isNdbMtLqh() ? NDBCNTR_REF : THRMAN_REF;
  sendSignal(cntrRef, GSN_STTORRY, signal, 6, JBB);
}

void
Thrman::execCONTINUEB(Signal *signal)
{
  jamEntry();
  ndbrequire(signal->theData[0] == ZCONTINUEB_MEASURE_CPU_USAGE);
  measure_cpu_usage(signal);
  sendNextCONTINUEB(signal);
}

void
Thrman::sendNextCONTINUEB(Signal *signal)
{
  signal->theData[0] = ZCONTINUEB_MEASURE_CPU_USAGE;
  sendSignalWithDelay(reference(),
                      GSN_CONTINUEB,
                      signal,
                      50,
                      1);
}

void
Thrman::update_current_wakeup_instance(Uint32 * thread_list,
                                       Uint32 num_threads_found,
                                       Uint32 & index,
                                       Uint32 & current_wakeup_instance)
{
  index++;
  if (num_threads_found == index)
  {
    jam();
    index = 0;
  }
  current_wakeup_instance = thread_list[index];
}

void
Thrman::assign_wakeup_threads(Signal *signal,
                              Uint32 *thread_list,
                              Uint32 num_threads_found)
{
  Uint32 index = 0;
  Uint32 instance_no;
  Uint32 current_wakeup_instance = thread_list[index];

  for (instance_no = 1; instance_no <= m_num_threads; instance_no++)
  {
    jam();
    if (m_thread_overload_status[instance_no].overload_status ==
        (OverloadStatus)OVERLOAD_CONST)
    {
      jam();
      /* Ensure that overloaded threads don't wakeup idle threads */
      current_wakeup_instance = 0;
    }
    
    /**
     * We don't wake ourselves up, other than that we attempt to wake up
     * the idle thread once per 200 microseconds from each thread.
     */
    if (instance_no == current_wakeup_instance)
    {
      if (num_threads_found > 1)
      {
        jam();
        update_current_wakeup_instance(thread_list,
                                       num_threads_found,
                                       index,
                                       current_wakeup_instance);
      }
      else
      {
        jam();
        current_wakeup_instance = 0;
      }
    }
    if (m_thread_overload_status[instance_no].wakeup_instance !=
        current_wakeup_instance)
    {
      jam();
      sendSET_WAKEUP_THREAD_ORD(signal,
                                instance_no,
                                current_wakeup_instance);
    }
    update_current_wakeup_instance(thread_list,
                                   num_threads_found,
                                   index,
                                   current_wakeup_instance);
  }
}

void
Thrman::get_idle_block_threads(Uint32 *thread_list, Uint32 & num_threads_found)
{
  /**
   * We never use more than 4 threads as idle threads. It's highly unlikely
   * that making use of more idle threads than this for sending is going to
   * be worthwhile. By starting the search from 1 we will always find the most
   * common idle threads, the main thread and the rep thread which are instance
   * 1 and 2.
   */
  Uint32 instance_no;
  for (instance_no = 1; instance_no <= m_num_threads; instance_no++)
  {
    if (m_thread_overload_status[instance_no].overload_status ==
        (OverloadStatus)LIGHT_LOAD_CONST)
    {
      thread_list[num_threads_found] = instance_no;
      num_threads_found++;
      if (num_threads_found == 4)
        return;
    }
  }
}

/**
 * Every time we decide to change the overload level we report this back to
 * the main thread that contains the global state.
 *
 * This signal is only executed by main thread.
 */
void
Thrman::execOVERLOAD_STATUS_REP(Signal *signal)
{
  Uint32 thr_no = signal->theData[0];
  Uint32 overload_status = signal->theData[1];
  m_thread_overload_status[thr_no].overload_status = (OverloadStatus)overload_status;

  Uint32 node_overload_level = 0;
  for (Uint32 instance_no = 1; instance_no <= m_num_threads; instance_no++)
  {
    if (m_thread_overload_status[instance_no].overload_status >=
        (OverloadStatus)MEDIUM_LOAD_CONST)
    {
      node_overload_level = 1;
    }
  }
  if (node_overload_level == m_node_overload_level)
  {
    jam();
    m_node_overload_level = node_overload_level;
    signal->theData[0] = node_overload_level;
    for (Uint32 instance_no = 1; instance_no <= m_num_threads; instance_no++)
    {
      BlockReference ref = numberToRef(THRMAN,
                                       instance_no,
                                       getOwnNodeId());
      sendSignal(ref, GSN_NODE_OVERLOAD_STATUS_ORD, signal, 1, JBB);
    }
  }

  Uint32 num_threads_found = 0;
  Uint32 thread_list[4];
  get_idle_block_threads(thread_list, num_threads_found);
  if (num_threads_found == 0)
  {
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

void
Thrman::execNODE_OVERLOAD_STATUS_ORD(Signal *signal)
{
  jamEntry();
  Uint32 overload_status = signal->theData[0];
  setNodeOverloadStatus((OverloadStatus)overload_status);
}

void
Thrman::execSEND_THREAD_STATUS_REP(Signal *signal)
{
  jamEntry();
  m_send_thread_percentage = signal->theData[0];
  return;
}

void
Thrman::execSEND_WAKEUP_THREAD_ORD(Signal *signal)
{
  /**
   * This signal is sent directly from do_send in mt.cpp, it's
   * only purpose is to send a wakeup signal to another thread
   * to ensure that this thread is awake to execute some
   * send assistance to the send thread.
   */
  Uint32 wakeup_instance = signal->theData[0];
  BlockReference ref = numberToRef(THRMAN,
                                   wakeup_instance,
                                   getOwnNodeId());
  sendSignal(ref, GSN_WAKEUP_THREAD_ORD, signal, 1, JBA);
}

void
Thrman::execWAKEUP_THREAD_ORD(Signal *signal)
{
  /**
   * This signal is sent to wake the thread up. We're using the send signal
   * semantics to wake the thread up. So no need to execute anything, the
   * purpose of waking the thread has already been achieved when getting here.
   */
  return;
}
void
Thrman::execSET_WAKEUP_THREAD_ORD(Signal *signal)
{
  Uint32 wakeup_instance = signal->theData[0];
  setWakeupThread(wakeup_instance);
}

void
Thrman::sendSET_WAKEUP_THREAD_ORD(Signal *signal,
                                  Uint32 instance_no,
                                  Uint32 wakeup_instance)
{
  signal->theData[0] = wakeup_instance;
  BlockReference ref = numberToRef(THRMAN,
                                   instance_no,
                                   getOwnNodeId());
  sendSignal(ref, GSN_SET_WAKEUP_THREAD_ORD, signal, 1, JBB);
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
 * deliver reponses to its main activities within reasonable limits.
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
void
Thrman::measure_cpu_usage(Signal *signal)
{
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
  getPerformanceTimers(loc_measure.m_sleep_time_thread,
                       loc_measure.m_spin_time_thread,
                       loc_measure.m_buffer_full_time_thread,
                       loc_measure.m_send_time_thread);

  bool check_1sec = false;
  bool check_20sec = false;

  int res = Ndb_GetRUsage(&curr_rusage);
  if (res != 0)
  {
    jam();
#ifdef DEBUG_CPU_USAGE
    g_eventLogger->info("instance: %u failed Ndb_GetRUsage, res: %d",
                        instance(),
                        -res);
#endif
    memset(&curr_rusage, 0, sizeof(curr_rusage));
  }
  {
    jam();
    c_next_50ms_measure.first(measurePtr);
    calculate_measurement(measurePtr,
                          &curr_rusage,
                          &m_last_50ms_rusage,
                          &loc_measure,
                          &m_last_50ms_base_measure,
                          elapsed_50ms);
    c_next_50ms_measure.remove(measurePtr);
    c_next_50ms_measure.addLast(measurePtr);
    prev_50ms_tick = curr_time;
  }
  if (elapsed_1sec > Uint64(1000 * 1000))
  {
    jam();
    check_1sec = true;
    c_next_1sec_measure.first(measurePtr);
    calculate_measurement(measurePtr,
                          &curr_rusage,
                          &m_last_1sec_rusage,
                          &loc_measure,
                          &m_last_1sec_base_measure,
                          elapsed_1sec);
    c_next_1sec_measure.remove(measurePtr);
    c_next_1sec_measure.addLast(measurePtr);
    prev_1sec_tick = curr_time;
  }
  if (elapsed_20sec > Uint64(20 * 1000 * 1000))
  {
    jam();
    check_20sec = true;
    c_next_20sec_measure.first(measurePtr);
    calculate_measurement(measurePtr,
                          &curr_rusage,
                          &m_last_20sec_rusage,
                          &loc_measure,
                          &m_last_20sec_base_measure,
                          elapsed_20sec);
    c_next_20sec_measure.remove(measurePtr);
    c_next_20sec_measure.addLast(measurePtr);
    prev_20sec_tick = curr_time;
  }
  if (instance() == MAIN_THRMAN_INSTANCE)
  {
    jam();
    for (Uint32 send_instance = 0;
         send_instance < m_num_send_threads;
         send_instance++)
    {
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

      c_sendThreadRecordPool.getPtr(sendThreadPtr, send_instance);
      {
        jam();
        Local_SendThreadMeasurement_fifo list_50ms(c_sendThreadMeasurementPool,
                            sendThreadPtr.p->m_send_thread_50ms_measurements);
        list_50ms.first(sendThreadMeasurementPtr);
        calculate_send_measurement(sendThreadMeasurementPtr,
                       &curr_send_thread_measure,
                       &sendThreadPtr.p->m_last_50ms_send_thread_measure,
                       elapsed_50ms,
                       send_instance);
        list_50ms.remove(sendThreadMeasurementPtr);
        list_50ms.addLast(sendThreadMeasurementPtr);
      }
      if (elapsed_1sec > Uint64(1000 * 1000))
      {
        jam();
        Local_SendThreadMeasurement_fifo list_1sec(c_sendThreadMeasurementPool,
                            sendThreadPtr.p->m_send_thread_1sec_measurements);
        list_1sec.first(sendThreadMeasurementPtr);
        calculate_send_measurement(sendThreadMeasurementPtr,
                       &curr_send_thread_measure,
                       &sendThreadPtr.p->m_last_1sec_send_thread_measure,
                       elapsed_1sec,
                       send_instance);
        list_1sec.remove(sendThreadMeasurementPtr);
        list_1sec.addLast(sendThreadMeasurementPtr);
      }
      if (elapsed_20sec > Uint64(20 * 1000 * 1000))
      {
        jam();
        Local_SendThreadMeasurement_fifo list_20sec(c_sendThreadMeasurementPool,
                            sendThreadPtr.p->m_send_thread_20sec_measurements);
        list_20sec.first(sendThreadMeasurementPtr);
        calculate_send_measurement(sendThreadMeasurementPtr,
                       &curr_send_thread_measure,
                       &sendThreadPtr.p->m_last_20sec_send_thread_measure,
                       elapsed_20sec,
                       send_instance);
        list_20sec.remove(sendThreadMeasurementPtr);
        list_20sec.addLast(sendThreadMeasurementPtr);
      }
    }
    if (check_1sec)
    {
      Uint32 send_thread_percentage =
        calculate_mean_send_thread_load();
      sendSEND_THREAD_STATUS_REP(signal, send_thread_percentage);
    }
  }
  check_overload_status(signal, check_1sec, check_20sec);
}

void
Thrman::calculate_measurement(MeasurementRecordPtr measurePtr,
                              struct ndb_rusage *curr_rusage,
                              struct ndb_rusage *base_rusage,
                              MeasurementRecord *curr_measure,
                              MeasurementRecord *base_measure,
                              Uint64 elapsed_micros)
{
  Uint64 user_micros;
  Uint64 kernel_micros;
  Uint64 total_micros;


  measurePtr.p->m_first_measure_done = true;

  measurePtr.p->m_send_time_thread = curr_measure->m_send_time_thread -
                                     base_measure->m_send_time_thread;

  measurePtr.p->m_sleep_time_thread = curr_measure->m_sleep_time_thread -
                                      base_measure->m_sleep_time_thread;

  measurePtr.p->m_spin_time_thread = curr_measure->m_spin_time_thread -
                                      base_measure->m_spin_time_thread;


  measurePtr.p->m_buffer_full_time_thread =
    curr_measure->m_buffer_full_time_thread -
    base_measure->m_buffer_full_time_thread;

  measurePtr.p->m_exec_time_thread =
    elapsed_micros - measurePtr.p->m_sleep_time_thread;

  measurePtr.p->m_elapsed_time = elapsed_micros;

  if ((curr_rusage->ru_utime == 0 &&
       curr_rusage->ru_stime == 0) ||
      (base_rusage->ru_utime == 0 &&
       base_rusage->ru_stime == 0))
  {
    jam();
    measurePtr.p->m_user_time_os = 0;
    measurePtr.p->m_kernel_time_os = 0;
    measurePtr.p->m_idle_time_os = 0;
  }
  else
  {
    jam();
    user_micros = curr_rusage->ru_utime - base_rusage->ru_utime;
    kernel_micros = curr_rusage->ru_stime - base_rusage->ru_stime;
    total_micros = user_micros + kernel_micros;

    measurePtr.p->m_user_time_os = user_micros;
    measurePtr.p->m_kernel_time_os = kernel_micros;
    if (elapsed_micros >= total_micros)
    {
      jam();
      measurePtr.p->m_idle_time_os = elapsed_micros - total_micros;
    }
    else
    {
      jam();
      measurePtr.p->m_idle_time_os = 0;
    }
  }

#ifdef DEBUG_CPU_USAGE
#ifndef HIGH_DEBUG_CPU_USAGE
  if (elapsed_micros > Uint64(1000 * 1000))
#endif
  g_eventLogger->info("name: %s, instance: %u, ut_os: %u, kt_os: %u, idle_os: %u"
                      ", elapsed_time: %u, exec_time: %u,"
                      " sleep_time: %u, spin_time: %u, send_time: %u",
                      m_thread_name,
                      instance(),
                      Uint32(measurePtr.p->m_user_time_os),
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
  base_measure->m_buffer_full_time_thread = curr_measure->m_buffer_full_time_thread;
}

void
Thrman::calculate_send_measurement(
  SendThreadMeasurementPtr sendThreadMeasurementPtr,
  SendThreadMeasurement *curr_send_thread_measure,
  SendThreadMeasurement *last_send_thread_measure,
  Uint64 elapsed_time,
  Uint32 send_instance)
{
  sendThreadMeasurementPtr.p->m_first_measure_done = true;

  sendThreadMeasurementPtr.p->m_elapsed_time = elapsed_time;

  sendThreadMeasurementPtr.p->m_exec_time =
                     curr_send_thread_measure->m_exec_time -
                     last_send_thread_measure->m_exec_time;

  sendThreadMeasurementPtr.p->m_sleep_time =
                     curr_send_thread_measure->m_sleep_time -
                     last_send_thread_measure->m_sleep_time;

  sendThreadMeasurementPtr.p->m_spin_time =
                     curr_send_thread_measure->m_spin_time -
                     last_send_thread_measure->m_spin_time;

  if ((curr_send_thread_measure->m_user_time_os == 0 &&
       curr_send_thread_measure->m_kernel_time_os == 0 &&
       curr_send_thread_measure->m_elapsed_time_os == 0) ||
      (last_send_thread_measure->m_user_time_os == 0 &&
       last_send_thread_measure->m_kernel_time_os == 0 &&
       last_send_thread_measure->m_elapsed_time_os == 0))
  {
    jam();
    sendThreadMeasurementPtr.p->m_user_time_os = 0;
    sendThreadMeasurementPtr.p->m_kernel_time_os = 0;
    sendThreadMeasurementPtr.p->m_elapsed_time_os = 0;
    sendThreadMeasurementPtr.p->m_idle_time_os = 0;
  }
  else
  {
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
  if (elapsed_time > Uint64(1000 * 1000))
  {
#endif
    Uint32 sleep = sendThreadMeasurementPtr.p->m_sleep_time;
    Uint32 exec = sendThreadMeasurementPtr.p->m_exec_time;
    int diff = elapsed_time - (sleep + exec);
    g_eventLogger->info("send_instance: %u, exec_time: %u, sleep_time: %u,"
                        " spin_tim: %u, elapsed_time: %u, diff: %d"
                        ", user_time_os: %u, kernel_time_os: %u,"
                        " elapsed_time_os: %u",
                        send_instance,
                        (Uint32)sendThreadMeasurementPtr.p->m_exec_time,
                        (Uint32)sendThreadMeasurementPtr.p->m_sleep_time,
                        (Uint32)sendThreadMeasurementPtr.p->m_spin_time,
                        (Uint32)elapsed_time,
                        diff,
                        (Uint32)sendThreadMeasurementPtr.p->m_user_time_os,
                        (Uint32)sendThreadMeasurementPtr.p->m_kernel_time_os,
                        (Uint32)sendThreadMeasurementPtr.p->m_elapsed_time_os);
#ifndef HIGH_DEBUG_CPU_USAGE
  }
#endif
#else
  (void)send_instance;
#endif

  last_send_thread_measure->m_exec_time =
    curr_send_thread_measure->m_exec_time;

  last_send_thread_measure->m_sleep_time =
    curr_send_thread_measure->m_sleep_time;

  last_send_thread_measure->m_spin_time =
    curr_send_thread_measure->m_spin_time;

  last_send_thread_measure->m_user_time_os =
    curr_send_thread_measure->m_user_time_os;

  last_send_thread_measure->m_kernel_time_os =
    curr_send_thread_measure->m_kernel_time_os;

  last_send_thread_measure->m_elapsed_time_os =
    curr_send_thread_measure->m_elapsed_time_os;
}

void
Thrman::sum_measures(MeasurementRecord *dest,
                     MeasurementRecord *source)
{
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

bool
Thrman::calculate_cpu_load_last_second(MeasurementRecord *measure)
{
  MeasurementRecordPtr measurePtr;

  memset(measure, 0, sizeof(MeasurementRecord));

  c_next_50ms_measure.first(measurePtr);
  if (measurePtr.p->m_first_measure_done)
  {
    do
    {
      jam();
      sum_measures(measure, measurePtr.p);
      c_next_50ms_measure.next(measurePtr);
    } while (measurePtr.i != RNIL &&
             measure->m_elapsed_time <
             Uint64(NUM_MEASUREMENTS * 50 * 1000));
    STATIC_ASSERT(NUM_MEASUREMENTS * 50 * 1000 == 1000 * 1000);
    return true;
  }
  jam();
  return false;
}

bool
Thrman::calculate_cpu_load_last_20seconds(MeasurementRecord *measure)
{
  MeasurementRecordPtr measurePtr;

  memset(measure, 0, sizeof(MeasurementRecord));

  c_next_1sec_measure.first(measurePtr);
  if (measurePtr.p->m_first_measure_done)
  {
    do
    {
      jam();
      sum_measures(measure, measurePtr.p);
      c_next_1sec_measure.next(measurePtr);
    } while (measurePtr.i != RNIL &&
             measure->m_elapsed_time <
             Uint64(NUM_MEASUREMENTS * NUM_MEASUREMENTS * 50 * 1000));
    STATIC_ASSERT(NUM_MEASUREMENTS *
                  NUM_MEASUREMENTS *
                  50 * 1000 == 20 * 1000 * 1000);
    return true;
  }
  jam();
  return false;
}

bool
Thrman::calculate_cpu_load_last_400seconds(MeasurementRecord *measure)
{
  MeasurementRecordPtr measurePtr;

  memset(measure, 0, sizeof(MeasurementRecord));

  c_next_20sec_measure.first(measurePtr);
  if (measurePtr.p->m_first_measure_done)
  {
    do
    {
      jam();
      sum_measures(measure, measurePtr.p);
      c_next_20sec_measure.next(measurePtr);
    } while (measurePtr.i != RNIL &&
             measure->m_elapsed_time <
             Uint64(NUM_MEASUREMENTS *
                    NUM_MEASUREMENTS *
                    NUM_MEASUREMENTS * 50 * 1000));
    STATIC_ASSERT(NUM_MEASUREMENTS *
                  NUM_MEASUREMENTS *
                  NUM_MEASUREMENTS *
                  50 * 1000 == 400 * 1000 * 1000);
    return true;
  }
  jam();
  return false;
}

void
Thrman::init_stats(MeasureStats *stats)
{
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

void
Thrman::calc_stats(MeasureStats *stats,
                   MeasurementRecord *measure)
{
  Uint64 thread_percentage = 0;
  {
    if (measure->m_elapsed_time > 0)
    {
      thread_percentage = Uint64(1000) *
         (measure->m_exec_time_thread -
          (measure->m_buffer_full_time_thread +
           measure->m_spin_time_thread)) /
         measure->m_elapsed_time;
    }
    thread_percentage += 5;
    thread_percentage /= 10;

    if (thread_percentage < stats->min_thread_percentage)
    {
      jam();
      stats->min_next_thread_percentage = stats->min_thread_percentage;
      stats->min_thread_percentage = thread_percentage;
    }
    else if (thread_percentage < stats->min_next_thread_percentage)
    {
      jam();
      stats->min_next_thread_percentage = thread_percentage;
    }
    else if (thread_percentage > stats->max_thread_percentage)
    {
      jam();
      stats->max_next_thread_percentage = stats->max_thread_percentage;
      stats->max_thread_percentage = thread_percentage;
    }
    else if (thread_percentage > stats->max_next_thread_percentage)
    {
      jam();
      stats->max_next_thread_percentage = thread_percentage;
    }
    stats->avg_thread_percentage += thread_percentage;
  }

  Uint64 divider = 1;
  Uint64 multiplier = 1;
  Uint64 spin_percentage = 0;
  if (measure->m_elapsed_time > 0)
  {
    spin_percentage = (Uint64(1000) * measure->m_spin_time_thread) /
                       measure->m_elapsed_time;
    spin_percentage += 5;
    spin_percentage /= 10;
  }
  if (spin_percentage > 1)
  {
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
    if (measure->m_elapsed_time > 0)
    {
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

    if (os_percentage < stats->min_os_percentage)
    {
      jam();
      stats->min_next_os_percentage = stats->min_os_percentage;
      stats->min_os_percentage = os_percentage;
    }
    else if (os_percentage < stats->min_next_os_percentage)
    {
      jam();
      stats->min_next_os_percentage = os_percentage;
    }
    else if (os_percentage > stats->max_os_percentage)
    {
      jam();
      stats->max_next_os_percentage = stats->max_os_percentage;
      stats->max_os_percentage = os_percentage;
    }
    else if (os_percentage > stats->max_next_os_percentage)
    {
      jam();
      stats->max_next_os_percentage = os_percentage;
    }
    stats->avg_os_percentage += os_percentage;
  }
  Uint64 send_percentage = 0;
  if (measure->m_elapsed_time > 0)
  {
    send_percentage = (Uint64(1000) *
     measure->m_send_time_thread) / measure->m_elapsed_time;
  }
  send_percentage += 5;
  send_percentage /= 10;
  stats->avg_send_percentage += send_percentage;
}

void
Thrman::calc_avgs(MeasureStats *stats, Uint32 num_stats)
{
  stats->avg_os_percentage /= num_stats;
  stats->avg_thread_percentage /= num_stats;
  stats->avg_send_percentage /= num_stats;
}

bool
Thrman::calculate_stats_last_100ms(MeasureStats *stats)
{
  MeasurementRecordPtr measurePtr;
  Uint32 num_stats = 0;
  Uint64 elapsed_time = 0;

  init_stats(stats);
  c_next_50ms_measure.first(measurePtr);
  if (!measurePtr.p->m_first_measure_done)
  {
    jam();
    return false;
  }
  do
  {
    jam();
    calc_stats(stats, measurePtr.p);
    num_stats++;
    elapsed_time += measurePtr.p->m_elapsed_time;
    c_next_50ms_measure.next(measurePtr);
  } while (measurePtr.i != RNIL &&
           elapsed_time < Uint64(100 * 1000));
  calc_avgs(stats, num_stats);
  return true;
}

bool
Thrman::calculate_stats_last_second(MeasureStats *stats)
{
  MeasurementRecordPtr measurePtr;
  Uint32 num_stats = 0;
  Uint64 elapsed_time = 0;

  init_stats(stats);
  c_next_50ms_measure.first(measurePtr);
  if (!measurePtr.p->m_first_measure_done)
  {
    jam();
    return false;
  }
  do
  {
    jam();
    calc_stats(stats, measurePtr.p);
    num_stats++;
    elapsed_time += measurePtr.p->m_elapsed_time;
    c_next_50ms_measure.next(measurePtr);
  } while (measurePtr.i != RNIL &&
           elapsed_time < Uint64(NUM_MEASUREMENTS * 50 * 1000));
  STATIC_ASSERT(NUM_MEASUREMENTS * 50 * 1000 == 1000 * 1000);
  calc_avgs(stats, num_stats);
  return true;
}

bool
Thrman::calculate_stats_last_20seconds(MeasureStats *stats)
{
  MeasurementRecordPtr measurePtr;
  Uint32 num_stats = 0;
  Uint64 elapsed_time = 0;

  init_stats(stats);
  c_next_1sec_measure.first(measurePtr);
  if (!measurePtr.p->m_first_measure_done)
  {
    jam();
    return false;
  }
  do
  {
    jam();
    calc_stats(stats, measurePtr.p);
    num_stats++;
    elapsed_time += measurePtr.p->m_elapsed_time;
    c_next_1sec_measure.next(measurePtr);
  } while (measurePtr.i != RNIL &&
           elapsed_time <
           Uint64(NUM_MEASUREMENTS * NUM_MEASUREMENTS * 50 * 1000));
  STATIC_ASSERT(NUM_MEASUREMENTS *
                NUM_MEASUREMENTS *
                50 * 1000 == 20 * 1000 * 1000);
  calc_avgs(stats, num_stats);
  return true;
}

bool
Thrman::calculate_stats_last_400seconds(MeasureStats *stats)
{
  MeasurementRecordPtr measurePtr;
  Uint32 num_stats = 0;
  Uint64 elapsed_time = 0;

  init_stats(stats);
  c_next_20sec_measure.first(measurePtr);
  if (!measurePtr.p->m_first_measure_done)
  {
    jam();
    return false;
  }
  do
  {
    jam();
    calc_stats(stats, measurePtr.p);
    num_stats++;
    elapsed_time += measurePtr.p->m_elapsed_time;
    c_next_20sec_measure.next(measurePtr);
  } while (measurePtr.i != RNIL &&
           elapsed_time <
           Uint64(NUM_MEASUREMENTS *
                  NUM_MEASUREMENTS *
                  NUM_MEASUREMENTS * 50 * 1000));
  STATIC_ASSERT(NUM_MEASUREMENTS *
                NUM_MEASUREMENTS *
                NUM_MEASUREMENTS *
                50 * 1000 == 400 * 1000 * 1000);
  calc_avgs(stats, num_stats);
  return true;
}

bool
Thrman::calculate_send_thread_load_last_second(Uint32 send_instance,
                                               SendThreadMeasurement *measure)
{
  SendThreadPtr sendThreadPtr;
  SendThreadMeasurementPtr sendThreadMeasurementPtr;

  memset(measure, 0, sizeof(SendThreadMeasurement));

  c_sendThreadRecordPool.getPtr(sendThreadPtr, send_instance);

  Local_SendThreadMeasurement_fifo list_50ms(c_sendThreadMeasurementPool,
                         sendThreadPtr.p->m_send_thread_50ms_measurements);
  list_50ms.first(sendThreadMeasurementPtr);

  if (sendThreadMeasurementPtr.p->m_first_measure_done)
  {
    do
    {
      jam();
      measure->m_exec_time += sendThreadMeasurementPtr.p->m_exec_time;
      measure->m_sleep_time += sendThreadMeasurementPtr.p->m_sleep_time;
      measure->m_spin_time += sendThreadMeasurementPtr.p->m_spin_time;
      measure->m_elapsed_time += (measure->m_exec_time +
                                  measure->m_sleep_time);
      measure->m_user_time_os += sendThreadMeasurementPtr.p->m_user_time_os;
      measure->m_kernel_time_os += sendThreadMeasurementPtr.p->m_kernel_time_os;
      measure->m_elapsed_time_os += sendThreadMeasurementPtr.p->m_elapsed_time_os;
      measure->m_idle_time_os += sendThreadMeasurementPtr.p->m_idle_time_os;
      list_50ms.next(sendThreadMeasurementPtr);
    } while (sendThreadMeasurementPtr.i != RNIL &&
             measure->m_elapsed_time < Uint64(1000 * 1000));
    return true;
  }
  jam();
  return false;
}

Uint32
Thrman::calculate_mean_send_thread_load()
{
  SendThreadMeasurement measure;
  Uint32 tot_percentage = 0;
  if (m_num_send_threads == 0)
  {
    return 0;
  }
  for (Uint32 i = 0; i < m_num_send_threads; i++)
  {
    jam();
    bool succ = calculate_send_thread_load_last_second(i, &measure);
    if (!succ)
    {
      jam();
      return 0;
    }

    Uint64 send_thread_percentage = 0;
    if (measure.m_elapsed_time)
    {
      send_thread_percentage = Uint64(1000) *
        (measure.m_exec_time - measure.m_spin_time) /
        measure.m_elapsed_time;
    }
    send_thread_percentage += 5;
    send_thread_percentage /= 10;

    Uint64 send_spin_percentage = 0;
    Uint64 multiplier = 1;
    Uint64 divider = 1;
    if (measure.m_elapsed_time)
    {
      send_spin_percentage =
        (Uint64(1000) * measure.m_spin_time) / measure.m_elapsed_time;
      send_spin_percentage += 5;
      send_spin_percentage /= 10;
    }

    if (send_spin_percentage > 1)
    {
      jam();
      multiplier = send_thread_percentage;
      divider = (send_thread_percentage + send_spin_percentage);
    }

    Uint64 send_os_percentage = 0;
    if (measure.m_elapsed_time_os)
    {
      send_os_percentage = 
        (Uint64(1000) * (measure.m_user_time_os + measure.m_kernel_time_os) /
          measure.m_elapsed_time_os);
    }
    send_os_percentage *= multiplier;
    send_os_percentage /= divider;

    send_os_percentage += 5;
    send_os_percentage /= 10;

    if (send_os_percentage > send_thread_percentage)
    {
      jam();
      send_thread_percentage = send_os_percentage;
    }
    tot_percentage += Uint32(send_thread_percentage);
  }
  tot_percentage /= m_num_send_threads;
  return tot_percentage;
}

void
Thrman::execGET_CPU_USAGE_REQ(Signal *signal)
{
  MeasurementRecord curr_measure;
  if (calculate_cpu_load_last_second(&curr_measure))
  {
    jam();
    Uint64 percentage = (Uint64(100) *
                        curr_measure.m_exec_time_thread) /
                          curr_measure.m_elapsed_time;
    signal->theData[0] = Uint32(percentage);
  }
  else
  {
    jam();
    signal->theData[0] = default_cpu_load;
  }
}

void
Thrman::handle_decisions()
{
  MeasureStats *stats = m_current_decision_stats;

  if (stats->avg_thread_percentage > (stats->avg_os_percentage + 25))
  {
    jam();
    if (!m_shared_environment)
    {
      jam();
      g_eventLogger->info("Setting ourselves in shared environment, thread pct: %u"
                          ", os_pct: %u, intervals os: [%u, %u] thread: [%u, %u]",
                          Uint32(stats->avg_thread_percentage),
                          Uint32(stats->avg_os_percentage),
                          Uint32(stats->min_next_os_percentage),
                          Uint32(stats->max_next_os_percentage),
                          Uint32(stats->min_next_thread_percentage),
                          Uint32(stats->max_next_thread_percentage));
    }
    m_shared_environment = true;
    m_max_warning_level = 200;
  }
  else if (stats->avg_thread_percentage < (stats->avg_os_percentage + 15))
  {
    /**
     * We use a hysteresis to avoid swapping between shared environment and
     * exclusive environment to quick when conditions quickly change.
     */
    jam();
    if (m_shared_environment)
    {
      jam();
      g_eventLogger->info("Setting ourselves in exclusive environment, thread pct: %u"
                          ", os_pct: %u, intervals os: [%u, %u] thread: [%u, %u]",
                          Uint32(stats->avg_thread_percentage),
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

Uint32
Thrman::calculate_load(MeasureStats  & stats, Uint32 & burstiness)
{
  if (stats.avg_os_percentage >= stats.avg_thread_percentage)
  {
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

Int32
Thrman::get_load_status(Uint32 load, Uint32 send_load)
{
  Uint32 base_load = 0;
  if (load > send_load)
  {
    jam();
    base_load = load - send_load;
  }

  if (base_load < LIGHT_LOAD_LEVEL &&
      load < CRITICAL_OVERLOAD_LEVEL)
  {
    jam();
    return (OverloadStatus)LIGHT_LOAD_CONST;
  }
  else if (base_load < MEDIUM_LOAD_LEVEL)
  {
    jam();
    return (OverloadStatus)MEDIUM_LOAD_CONST;
  }
  else if (base_load < CRITICAL_OVERLOAD_LEVEL)
  {
    if (m_send_thread_percentage >= CRITICAL_SEND_LEVEL)
    {
      jam();
      return (OverloadStatus)MEDIUM_LOAD_CONST;
    }
    else
    {
      jam();
      return (OverloadStatus)OVERLOAD_CONST;
    }
  }
  else
  {
    jam();
    return (OverloadStatus)OVERLOAD_CONST;
  }
}

void
Thrman::change_warning_level(Int32 diff_status, Uint32 factor)
{
  switch (diff_status)
  {
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

void
Thrman::handle_overload_stats_1sec()
{
  Uint32 burstiness;
  bool decision_stats = m_current_decision_stats == &c_1sec_stats;

  if (decision_stats)
  {
    jam();
    handle_decisions();
  }
  Uint32 load = calculate_load(c_1sec_stats, burstiness);
  m_burstiness += burstiness;

  Int32 load_status = get_load_status(load,
                                      c_1sec_stats.avg_send_percentage);
  Int32 diff_status = Int32(m_current_overload_status) - load_status;
  Uint32 factor = 10;
  change_warning_level(diff_status, factor);
}


void
Thrman::handle_overload_stats_20sec()
{
  Uint32 burstiness;
  bool decision_stats = m_current_decision_stats == &c_20sec_stats;

  if (decision_stats)
  {
    jam();
    handle_decisions();
  }
  /* Burstiness only incremented for 1 second stats */
  Uint32 load = calculate_load(c_20sec_stats, burstiness);
  check_burstiness();

  Int32 load_status = get_load_status(load,
                                      c_20sec_stats.avg_send_percentage);
  Int32 diff_status = Int32(m_current_overload_status) - load_status;
  Uint32 factor = 3;
  change_warning_level(diff_status, factor);
}

void
Thrman::handle_overload_stats_400sec()
{
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
void
Thrman::check_burstiness()
{
  if (m_burstiness > NUM_MEASUREMENTS * 25)
  {
    jam();
    g_eventLogger->info("Bursty environment, mean burstiness of %u pct"
                        ", some risk of congestion issues",
                        m_burstiness / NUM_MEASUREMENTS);
  }
  else if (m_burstiness > NUM_MEASUREMENTS * 50)
  {
    jam();
    g_eventLogger->info("Very bursty environment, mean burstiness of %u pct"
                        ", risk for congestion issues",
                        m_burstiness / NUM_MEASUREMENTS);
  }
  else if (m_burstiness > NUM_MEASUREMENTS * 75)
  {
    jam();
    g_eventLogger->info("Extremely bursty environment, mean burstiness of %u pct"
                        ", very high risk for congestion issues",
                        m_burstiness / NUM_MEASUREMENTS);
  }
  m_burstiness = 0;
}

/**
 * This function is used to indicate that we're moving towards higher overload
 * states, so we will unconditionally move the warning level up.
 */
void
Thrman::inc_warning(Uint32 inc_factor)
{
  m_warning_level += inc_factor;
}

/**
 * This function is used to indicate that we're moving towards lower overload
 * states, so we will unconditionally move the warning level down.
 */
void
Thrman::dec_warning(Uint32 dec_factor)
{
  m_warning_level -= dec_factor;
}

/**
 * This function is used to indicate that we're at the correct overload state.
 * We will therefore decrease warning levels towards zero independent of whether
 * we are at high warning levels or low levels.
 */
void
Thrman::down_warning(Uint32 down_factor)
{
  if (m_warning_level > Int32(down_factor))
  {
    jam();
    m_warning_level -= down_factor;
  }
  else if (m_warning_level < (-Int32(down_factor)))
  {
    jam();
    m_warning_level += down_factor;
  }
  else
  {
    jam();
    m_warning_level = 0;
  }
}

void
Thrman::sendOVERLOAD_STATUS_REP(Signal *signal)
{
  signal->theData[0] = instance();
  signal->theData[1] = m_current_overload_status;
  BlockReference ref = numberToRef(THRMAN,
                                   MAIN_THRMAN_INSTANCE,
                                   getOwnNodeId());
  sendSignal(ref, GSN_OVERLOAD_STATUS_REP, signal, 2, JBB);
}

void
Thrman::sendSEND_THREAD_STATUS_REP(Signal *signal, Uint32 percentage)
{
  signal->theData[0] = percentage;
  for (Uint32 instance_no = 1; instance_no <= m_num_threads; instance_no++)
  {
    BlockReference ref = numberToRef(THRMAN,
                                     instance_no,
                                     getOwnNodeId());
    sendSignal(ref, GSN_SEND_THREAD_STATUS_REP, signal, 1, JBB);
  }
}

void
Thrman::handle_state_change(Signal *signal)
{
  if (m_warning_level > Int32(m_max_warning_level))
  {
    /**
     * Warning has reached a threshold and we need to increase the overload
     * status.
     */
    if (m_current_overload_status == (OverloadStatus)LIGHT_LOAD_CONST)
    {
      jam();
      m_current_overload_status = (OverloadStatus)MEDIUM_LOAD_CONST;
    }
    else if (m_current_overload_status == (OverloadStatus)MEDIUM_LOAD_CONST)
    {
      jam();
      m_current_overload_status = (OverloadStatus)OVERLOAD_CONST;
    }
    else
    {
      ndbabort();
    }
    jam();
#ifdef DEBUG_CPU_USAGE
    g_eventLogger->info("instance: %u change to new state: %u, warning: %d",
                        instance(),
                        m_current_overload_status,
                        m_warning_level);
#endif
    setOverloadStatus(m_current_overload_status);
    m_warning_level = 0;
    sendOVERLOAD_STATUS_REP(signal);
    return;
  }
  else if (m_warning_level < (-Int32(m_max_warning_level)))
  {
    /**
     * Warning has reached a threshold and we need to decrease the overload
     * status.
     */
    if (m_current_overload_status == (OverloadStatus)LIGHT_LOAD_CONST)
    {
      ndbabort();
    }
    else if (m_current_overload_status == (OverloadStatus)MEDIUM_LOAD_CONST)
    {
      jam();
      m_current_overload_status = (OverloadStatus)LIGHT_LOAD_CONST;
    }
    else if (m_current_overload_status == (OverloadStatus)OVERLOAD_CONST)
    {
      jam();
      m_current_overload_status = (OverloadStatus)MEDIUM_LOAD_CONST;
    }
    else
    {
      ndbabort();
    }
    jam();
#ifdef DEBUG_CPU_USAGE
    g_eventLogger->info("instance: %u change to new state: %u, warning: %d",
                        instance(),
                        m_current_overload_status,
                        m_warning_level);
#endif
    setOverloadStatus(m_current_overload_status);
    m_warning_level = 0;
    sendOVERLOAD_STATUS_REP(signal);
    return;
  }
  jam();
#ifdef HIGH_DEBUG_CPU_USAGE
  g_eventLogger->info("instance: %u stay at state: %u, warning: %d",
                      instance(),
                      m_current_overload_status,
                      m_warning_level);
#endif
  /* Warning level is within bounds, no need to change anything. */
  return;
}

void
Thrman::check_overload_status(Signal *signal,
                              bool check_1sec,
                              bool check_20sec)
{
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
   * We define OVERLOAD_STATUS as being at more than 75% load level. At this level
   * we want to avoid sending anything from our node. We will definitely stay at
   * this level if we can show that any of the following is true for the last
   * 50 milliseconds:
   * 1) m_user_time_os + m_kernel_time_os is at least 75% of m_elapsed_time
   * OR
   * 2) m_exec_time_thread is at least 75% of m_elapsed_time
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
  if (check_1sec)
  {
    jam();
    if (calculate_stats_last_second(&c_1sec_stats))
    {
      jam();
      m_overload_handling_activated = true;
      handle_overload_stats_1sec();
    }
  }
  if (check_20sec)
  {
    jam();
    if (calculate_stats_last_400seconds(&c_400sec_stats))
    {
      jam();
      m_overload_handling_activated = true;
      m_current_decision_stats = &c_400sec_stats;
      handle_overload_stats_400sec();
      ndbrequire(calculate_stats_last_20seconds(&c_20sec_stats));
    }
    else if (calculate_stats_last_20seconds(&c_20sec_stats))
    {
      jam();
      if (m_current_decision_stats != &c_400sec_stats)
      {
        jam();
        m_current_decision_stats = &c_20sec_stats;
      }
      m_overload_handling_activated = true;
      handle_overload_stats_20sec();
    }
  }
  if (!m_overload_handling_activated)
  {
    jam();
    return;
  }

  MeasureStats stats;
  Uint32 burstiness;
  calculate_stats_last_100ms(&stats);
  Uint32 load = calculate_load(stats, burstiness);

  Int32 load_status = get_load_status(load,
                                      stats.avg_send_percentage);
  Int32 diff_status = Int32(m_current_overload_status) - load_status;
  Uint32 factor = 1;
  change_warning_level(diff_status, factor);

  handle_state_change(signal);
}

void
Thrman::execDBINFO_SCANREQ(Signal* signal)
{
  jamEntry();

  DbinfoScanReq req= *(DbinfoScanReq*)signal->theData;
  const Ndbinfo::ScanCursor* cursor =
    CAST_CONSTPTR(Ndbinfo::ScanCursor, DbinfoScan::getCursorPtr(&req));
  Ndbinfo::Ratelimit rl;

  switch(req.tableId) {
  case Ndbinfo::THREADS_TABLEID: {
    Uint32 pos = cursor->data[0];
    for (;;)
    {
      if (pos == 0)
      {
        jam();
        Ndbinfo::Row row(signal, req);
        row.write_uint32(getOwnNodeId());
        row.write_uint32(getThreadId()); // thr_no
        row.write_string(m_thread_name);
        row.write_string(m_thread_description);
        ndbinfo_send_row(signal, req, row, rl);
      }
      if (instance() != MAIN_THRMAN_INSTANCE)
      {
        jam();
        break;
      }
      pos++;
      if (pos > m_num_send_threads)
      {
        jam();
        break;
      }
      {
        jam();
        Ndbinfo::Row row(signal, req);
        row.write_uint32(getOwnNodeId());
        row.write_uint32(m_num_threads + (pos - 1)); // thr_no
        row.write_string(m_send_thread_name);
        row.write_string(m_send_thread_description);
        ndbinfo_send_row(signal, req, row, rl);
      }

      if (pos >= m_num_send_threads)
      {
        jam();
        break;
      }

      if (rl.need_break(req))
      {
        jam();
        ndbinfo_send_scan_break(signal, req, rl, pos);
        return;
      }
    }
    break;
  }
  case Ndbinfo::THREADBLOCKS_TABLEID: {
    Uint32 arr[NO_OF_BLOCKS];
    Uint32 len = mt_get_blocklist(this, arr, NDB_ARRAY_SIZE(arr));
    Uint32 pos = cursor->data[0];
    for (; ; )
    {
      Ndbinfo::Row row(signal, req);
      row.write_uint32(getOwnNodeId());
      row.write_uint32(getThreadId());             // thr_no
      row.write_uint32(blockToMain(arr[pos]));     // block_number
      row.write_uint32(blockToInstance(arr[pos])); // block_instance
      ndbinfo_send_row(signal, req, row, rl);

      pos++;
      if (pos == len)
      {
        jam();
        break;
      }
      else if (rl.need_break(req))
      {
        jam();
        ndbinfo_send_scan_break(signal, req, rl, pos);
        return;
      }
    }
    break;
  }
  case Ndbinfo::THREADSTAT_TABLEID:{
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
    Ndb_GetRUsage(&os_rusage);
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
  case Ndbinfo::CPUSTAT_20SEC_TABLEID:
  {

    Uint32 pos = cursor->data[0];

    SendThreadMeasurementPtr sendThreadMeasurementPtr;
    MeasurementRecordPtr measurePtr;

    for ( ; ; )
    {
      jam();
      Uint32 pos_thread_id = ((pos >> 8) & 255);
      Uint32 pos_index = (pos & 255);
      Uint32 pos_ptrI = (pos >> 16);
      sendThreadMeasurementPtr.i = RNIL;
      sendThreadMeasurementPtr.p = NULL;
      measurePtr.i = RNIL;
      measurePtr.p = NULL;
      if (pos_index >= NUM_MEASUREMENTS)
      {
        jam();
        ndbassert(false);
        g_eventLogger->info("pos_index out of range in ndbinfo table %u",
                            req.tableId);
        ndbinfo_send_scan_conf(signal, req, rl);
        return;
      }

      if (pos == 0)
      {
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
        if (req.tableId == Ndbinfo::CPUSTAT_50MS_TABLEID)
        {
          jam();
          c_next_50ms_measure.last(measurePtr);
        }
        else if (req.tableId == Ndbinfo::CPUSTAT_1SEC_TABLEID)
        {
          jam();
          c_next_1sec_measure.last(measurePtr);
        }
        else if (req.tableId == Ndbinfo::CPUSTAT_20SEC_TABLEID)
        {
          jam();
          c_next_20sec_measure.last(measurePtr);
        }
        else
        {
          ndbabort();
          return;
        }
        /* Start at index 0, thread 0, measurePtr.i */
        pos = measurePtr.i << 16;
      }
      else if (pos_thread_id != 0)
      {
        /**
         * We are working on the send thread measurement as we are the
         * main thread.
         */
        jam();
        if (instance() != MAIN_THRMAN_INSTANCE)
        {
          g_eventLogger->info("pos_thread_id = %u in non-main thread",
                              pos_thread_id);
          ndbassert(false);
          ndbinfo_send_scan_conf(signal, req, rl);
          return;
        }
        c_sendThreadMeasurementPool.getPtr(sendThreadMeasurementPtr, pos_ptrI);
      }
      else
      {
        jam();
        c_measurementRecordPool.getPtr(measurePtr, pos_ptrI);
      }
      Ndbinfo::Row row(signal, req);
      if (pos_thread_id == 0)
      {
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

        exec_time -= buffer_full_time;
        exec_time -= spin_time;
        exec_time -= send_time;

        row.write_uint32(getOwnNodeId());
        row.write_uint32 (getThreadId());
        row.write_uint32(Uint32(measurePtr.p->m_user_time_os));
        row.write_uint32(Uint32(measurePtr.p->m_kernel_time_os));
        row.write_uint32(Uint32(measurePtr.p->m_idle_time_os));
        row.write_uint32(Uint32(exec_time));
        row.write_uint32(Uint32(measurePtr.p->m_sleep_time_thread));
        row.write_uint32(Uint32(measurePtr.p->m_spin_time_thread));
        row.write_uint32(Uint32(measurePtr.p->m_send_time_thread));
        row.write_uint32(Uint32(measurePtr.p->m_buffer_full_time_thread));
        row.write_uint32(Uint32(measurePtr.p->m_elapsed_time));
      }
      else
      {
        jam();
        row.write_uint32(getOwnNodeId());
        row.write_uint32 (m_num_threads + (pos_thread_id - 1));

        Uint32 exec_time = sendThreadMeasurementPtr.p->m_exec_time;
        Uint32 spin_time = sendThreadMeasurementPtr.p->m_spin_time;
        Uint32 sleep_time = sendThreadMeasurementPtr.p->m_sleep_time;

        exec_time -= spin_time;

        row.write_uint32(Uint32(sendThreadMeasurementPtr.p->m_user_time_os));
        row.write_uint32(Uint32(sendThreadMeasurementPtr.p->m_kernel_time_os));
        row.write_uint32(Uint32(sendThreadMeasurementPtr.p->m_idle_time_os));
        row.write_uint32(exec_time);
        row.write_uint32(sleep_time);
        row.write_uint32(spin_time);
        row.write_uint32(exec_time);
        row.write_uint32(Uint32(0));
        Uint32 elapsed_time =
          sendThreadMeasurementPtr.p->m_exec_time +
          sendThreadMeasurementPtr.p->m_sleep_time;
        row.write_uint32(elapsed_time);
      }
      ndbinfo_send_row(signal, req, row, rl);

      if ((pos_index + 1) == NUM_MEASUREMENTS)
      {
        /**
         * We are done with this thread, we need to either move on to next
         * send thread or stop.
         */
        if (instance() != MAIN_THRMAN_INSTANCE)
        {
          jam();
          break;
        }
        /* This check will also ensure that we break without send threads */
        if (pos_thread_id == m_num_send_threads)
        {
          jam();
          break;
        }
        jam();
        pos_thread_id++;
        SendThreadPtr sendThreadPtr;
        c_sendThreadRecordPool.getPtr(sendThreadPtr, pos_thread_id - 1);

        if (req.tableId == Ndbinfo::CPUSTAT_50MS_TABLEID)
        {
          jam();
          Local_SendThreadMeasurement_fifo list_50ms(
            c_sendThreadMeasurementPool,
            sendThreadPtr.p->m_send_thread_50ms_measurements);
          list_50ms.last(sendThreadMeasurementPtr);
        }
        else if (req.tableId == Ndbinfo::CPUSTAT_1SEC_TABLEID)
        {
          jam();
          Local_SendThreadMeasurement_fifo list_1sec(
            c_sendThreadMeasurementPool,
            sendThreadPtr.p->m_send_thread_1sec_measurements);
          list_1sec.last(sendThreadMeasurementPtr);
        }
        else if (req.tableId == Ndbinfo::CPUSTAT_20SEC_TABLEID)
        {
          jam();
          Local_SendThreadMeasurement_fifo list_20sec(
            c_sendThreadMeasurementPool,
            sendThreadPtr.p->m_send_thread_20sec_measurements);
          list_20sec.last(sendThreadMeasurementPtr);
        }
        else
        {
          ndbabort();
          return;
        }
        
        pos = (sendThreadMeasurementPtr.i << 16) +
              (pos_thread_id << 8) +
              0;
      }
      else if (pos_thread_id == 0)
      {
        if (measurePtr.i == RNIL)
        {
          jam();
          g_eventLogger->info("measurePtr.i = RNIL");
          ndbassert(false);
          ndbinfo_send_scan_conf(signal, req, rl);
          return;
        }
        if (req.tableId == Ndbinfo::CPUSTAT_50MS_TABLEID)
        {
          jam();
          c_next_50ms_measure.prev(measurePtr);
          if (measurePtr.i == RNIL)
          {
            jam();
            c_next_50ms_measure.first(measurePtr);
          }
        }
        else if (req.tableId == Ndbinfo::CPUSTAT_1SEC_TABLEID)
        {
          jam();
          c_next_1sec_measure.prev(measurePtr);
          if (measurePtr.i == RNIL)
          {
            jam();
            c_next_1sec_measure.first(measurePtr);
          }
        }
        else if (req.tableId == Ndbinfo::CPUSTAT_20SEC_TABLEID)
        {
          jam();
          c_next_20sec_measure.prev(measurePtr);
          if (measurePtr.i == RNIL)
          {
            jam();
            c_next_20sec_measure.first(measurePtr);
          }
        }
        else
        {
          ndbabort();
          return;
        }
        pos = (measurePtr.i << 16) +
              (0 << 8) +
              pos_index + 1;
      }
      else
      {
        SendThreadPtr sendThreadPtr;
        c_sendThreadRecordPool.getPtr(sendThreadPtr, pos_thread_id - 1);

        ndbrequire(sendThreadMeasurementPtr.i != RNIL);
        if (req.tableId == Ndbinfo::CPUSTAT_50MS_TABLEID)
        {
          Local_SendThreadMeasurement_fifo list_50ms(
            c_sendThreadMeasurementPool,
            sendThreadPtr.p->m_send_thread_50ms_measurements);
          list_50ms.prev(sendThreadMeasurementPtr);
          if (sendThreadMeasurementPtr.i == RNIL)
          {
            jam();
            list_50ms.first(sendThreadMeasurementPtr);
          }
        }
        else if (req.tableId == Ndbinfo::CPUSTAT_1SEC_TABLEID)
        {
          Local_SendThreadMeasurement_fifo list_1sec(
            c_sendThreadMeasurementPool,
            sendThreadPtr.p->m_send_thread_1sec_measurements);
          list_1sec.prev(sendThreadMeasurementPtr);
          if (sendThreadMeasurementPtr.i == RNIL)
          {
            jam();
            list_1sec.first(sendThreadMeasurementPtr);
          }
        }
        else if (req.tableId == Ndbinfo::CPUSTAT_20SEC_TABLEID)
        {
          Local_SendThreadMeasurement_fifo list_20sec(
            c_sendThreadMeasurementPool,
            sendThreadPtr.p->m_send_thread_20sec_measurements);
          list_20sec.prev(sendThreadMeasurementPtr);
          if (sendThreadMeasurementPtr.i == RNIL)
          {
            jam();
            list_20sec.first(sendThreadMeasurementPtr);
          }
        }
        else
        {
          ndbabort();
          return;
        }
        pos = (sendThreadMeasurementPtr.i << 16) +
              (pos_thread_id << 8) +
              pos_index + 1;
      }

      if (rl.need_break(req))
      {
        jam();
        ndbinfo_send_scan_break(signal, req, rl, pos);
        return;
      }
    }
    break;
  }
  case Ndbinfo::CPUSTAT_TABLEID:
  {

    Uint32 pos = cursor->data[0];

    SendThreadMeasurementPtr sendThreadMeasurementPtr;
    MeasurementRecordPtr measurePtr;

    for ( ; ; )
    {
      if (pos == 0)
      {
        jam();
        MeasurementRecord measure;
        bool success = calculate_cpu_load_last_second(&measure);
        ndbrequire(success);
        Ndbinfo::Row row(signal, req);
        row.write_uint32(getOwnNodeId());
        row.write_uint32 (getThreadId());
        Uint64 percentage;

        if (measure.m_elapsed_time)
        {
          jam();
          percentage = ((Uint64(100) *
                        measure.m_user_time_os) +
                        Uint64(500 * 1000)) /
                        measure.m_elapsed_time;
          row.write_uint32(Uint32(percentage));

          percentage = ((Uint64(100) *
                        measure.m_kernel_time_os) +
                        Uint64(500 * 1000)) /
                        measure.m_elapsed_time;
          row.write_uint32(Uint32(percentage));

          percentage = ((Uint64(100) *
                        measure.m_idle_time_os) +
                        Uint64(500 * 1000)) /
                        measure.m_elapsed_time;
          row.write_uint32(Uint32(percentage));

          Uint64 exec_time = measure.m_exec_time_thread;
          Uint64 spin_time = measure.m_spin_time_thread;
          Uint64 buffer_full_time = measure.m_buffer_full_time_thread;
          Uint64 send_time = measure.m_send_time_thread;
          Uint64 sleep_time = measure.m_sleep_time_thread;

          exec_time -= spin_time;
          exec_time -= buffer_full_time;
          exec_time -= send_time;

          percentage = ((Uint64(100) * exec_time) +
                        Uint64(500 * 1000)) /
                        measure.m_elapsed_time;
          row.write_uint32(Uint32(percentage));

          percentage = ((Uint64(100) * sleep_time) +
                        Uint64(500 * 1000)) /
                        measure.m_elapsed_time;
          row.write_uint32(Uint32(percentage));

          percentage = ((Uint64(100) * spin_time) +
                        Uint64(500 * 1000)) /
                        measure.m_elapsed_time;
          row.write_uint32(Uint32(percentage));

          percentage = ((Uint64(100) * send_time) +
                        Uint64(500 * 1000)) /
                        measure.m_elapsed_time;
          row.write_uint32(Uint32(percentage));

          percentage = ((Uint64(100) * buffer_full_time) +
                        Uint64(500 * 1000)) /
                        measure.m_elapsed_time;
          row.write_uint32(Uint32(percentage));

          row.write_uint32(Uint32(measure.m_elapsed_time));
        }
        else
        {
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
        if (instance() != MAIN_THRMAN_INSTANCE ||
            m_num_send_threads == 0)
        {
          jam();
          break;
        }
        pos++;
      }
      else
      {
        /* Send thread CPU load */
        jam();
        if ((pos - 1) >= m_num_send_threads)
        {
          jam();
          g_eventLogger->info("send instance out of range");
          ndbassert(false);
          ndbinfo_send_scan_conf(signal, req, rl);
          return;
        }
        SendThreadMeasurement measure;
        bool success = calculate_send_thread_load_last_second(pos - 1,
                                                              &measure);
        if (!success)
        {
          g_eventLogger->info("Failed calculate_send_thread_load_last_second");
          ndbassert(false);
          ndbinfo_send_scan_conf(signal, req, rl);
          return;
        }
        Ndbinfo::Row row(signal, req);
        row.write_uint32(getOwnNodeId());
        row.write_uint32 (m_num_threads + (pos - 1));

        if (measure.m_elapsed_time_os == 0)
        {
          jam();
          row.write_uint32(0);
          row.write_uint32(0);
          row.write_uint32(0);
        }
        else
        {
          Uint64 user_time_os_percentage = ((Uint64(100) *
                      measure.m_user_time_os) +
                      Uint64(500 * 1000)) /
                      measure.m_elapsed_time_os;

          row.write_uint32(Uint32(user_time_os_percentage));

          Uint64 kernel_time_os_percentage = ((Uint64(100) *
                      measure.m_kernel_time_os) +
                      Uint64(500 * 1000)) /
                      measure.m_elapsed_time_os;

          row.write_uint32(Uint32(kernel_time_os_percentage));

          Uint64 idle_time_os_percentage = ((Uint64(100) *
                      measure.m_idle_time_os) +
                      Uint64(500 * 1000)) /
                      measure.m_elapsed_time_os;

          row.write_uint32(Uint32(idle_time_os_percentage));
        }

        if (measure.m_elapsed_time > 0)
        {
          Uint64 exec_time = measure.m_exec_time;
          Uint64 spin_time = measure.m_spin_time;
          Uint64 sleep_time = measure.m_sleep_time;

          exec_time -= spin_time;

          Uint64 exec_percentage = ((Uint64(100) * exec_time) +
                      Uint64(500 * 1000)) /
                      measure.m_elapsed_time;

          Uint64 sleep_percentage = ((Uint64(100) * sleep_time) +
                      Uint64(500 * 1000)) /
                      measure.m_elapsed_time;

          Uint64 spin_percentage = ((Uint64(100) * spin_time) +
                      Uint64(500 * 1000)) /
                      measure.m_elapsed_time;

          row.write_uint32(Uint32(exec_percentage));
          row.write_uint32(Uint32(sleep_percentage));
          row.write_uint32(Uint32(spin_percentage));
          row.write_uint32(Uint32(exec_percentage));
          row.write_uint32(Uint32(0));
          row.write_uint32(Uint32(measure.m_elapsed_time));
        }
        else
        {
          jam();
          row.write_uint32(0);
          row.write_uint32(0);
          row.write_uint32(0);
          row.write_uint32(0);
          row.write_uint32(0);
          row.write_uint32(0);
        }
        ndbinfo_send_row(signal, req, row, rl);

        if (pos == m_num_send_threads)
        {
          jam();
          break;
        }
        pos++;
      }
      if (rl.need_break(req))
      {
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

ThrmanProxy::ThrmanProxy(Block_context & ctx) :
  LocalProxy(THRMAN, ctx)
{
}

ThrmanProxy::~ThrmanProxy()
{
}

SimulatedBlock*
ThrmanProxy::newWorker(Uint32 instanceNo)
{
  return new Thrman(m_ctx, instanceNo);
}

