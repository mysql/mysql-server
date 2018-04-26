/*
   Copyright (c) 2003, 2018, Oracle and/or its affiliates. All rights reserved.

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

#include <my_global.h>
#include "Backup.hpp"

#include <ndb_version.h>

#include <NdbTCP.h>
#include <Bitmask.hpp>

#include <signaldata/NodeFailRep.hpp>
#include <signaldata/ReadNodesConf.hpp>

#include <signaldata/DihScanTab.hpp>
#include <signaldata/DiGetNodes.hpp>
#include <signaldata/ScanFrag.hpp>

#include <signaldata/GetTabInfo.hpp>
#include <signaldata/DictTabInfo.hpp>
#include <signaldata/ListTables.hpp>

#include <signaldata/FsOpenReq.hpp>
#include <signaldata/FsAppendReq.hpp>
#include <signaldata/FsCloseReq.hpp>
#include <signaldata/FsConf.hpp>
#include <signaldata/FsRef.hpp>
#include <signaldata/FsRemoveReq.hpp>
#include <signaldata/FsReadWriteReq.hpp>

#include <signaldata/BackupImpl.hpp>
#include <signaldata/BackupSignalData.hpp>
#include <signaldata/BackupContinueB.hpp>
#include <signaldata/EventReport.hpp>

#include <signaldata/UtilSequence.hpp>

#include <signaldata/CreateTrigImpl.hpp>
#include <signaldata/DropTrigImpl.hpp>
#include <signaldata/FireTrigOrd.hpp>
#include <signaldata/TrigAttrInfo.hpp>
#include <AttributeHeader.hpp>

#include <signaldata/WaitGCP.hpp>
#include <signaldata/LCP.hpp>
#include <signaldata/BackupLockTab.hpp>
#include <signaldata/DumpStateOrd.hpp>

#include <signaldata/DumpStateOrd.hpp>

#include <signaldata/DbinfoScan.hpp>
#include <signaldata/TransIdAI.hpp>

#include <NdbTick.h>
#include <dbtup/Dbtup.hpp>
#include <dbtup/Dbtup.hpp>

#include <EventLogger.hpp>
extern EventLogger * g_eventLogger;

#include <math.h>

#define JAM_FILE_ID 475

static const Uint32 WaitDiskBufferCapacityMillis = 1;
static const Uint32 WaitScanTempErrorRetryMillis = 10;

static NDB_TICKS startTime;

#ifdef VM_TRACE
//#define DEBUG_LCP 1
//#define DEBUG_LCP_ROW 1
//#define DEBUG_LCP_DEL_FILES 1
//#define DEBUG_LCP_DEL 1
//#define DEBUG_EXTRA_LCP 1
#define DEBUG_LCP_STAT 1
#define DEBUG_EXTENDED_LCP_STAT 1
//#define DEBUG_REDO_CONTROL 1
#endif

#ifdef DEBUG_REDO_CONTROL
#define DEB_REDO_CONTROL(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_REDO_CONTROL(arglist) do { } while (0)
#endif

#ifdef DEBUG_LCP
#define DEB_LCP(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_LCP(arglist) do { } while (0)
#endif

#ifdef DEBUG_LCP_DEL_FILES
#define DEB_LCP_DEL_FILES(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_LCP_DEL_FILES(arglist) do { } while (0)
#endif

#ifdef DEBUG_LCP_DEL
#define DEB_LCP_DEL(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_LCP_DEL(arglist) do { } while (0)
#endif

#ifdef DEBUG_LCP_STAT
#define DEB_LCP_STAT(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_LCP_STAT(arglist) do { } while (0)
#endif

#ifdef DEBUG_EXTRA_LCP
#define DEB_EXTRA_LCP(arglist) do { g_eventLogger->info arglist ; } while (0)
#else
#define DEB_EXTRA_LCP(arglist) do { } while (0)
#endif

#ifdef VM_TRACE
#define DEBUG_OUT(x) ndbout << x << endl
#else
#define DEBUG_OUT(x) 
#endif

//#define DEBUG_ABORT
//#define dbg globalSignalLoggers.log

static Uint32 g_TypeOfStart = NodeState::ST_ILLEGAL_TYPE;

#define SEND_BACKUP_STARTED_FLAG(A) (((A) & 0x3) > 0)
#define SEND_BACKUP_COMPLETED_FLAG(A) (((A) & 0x3) > 1)

/**
 * "Magic" constants used for adaptive LCP speed algorithm. These magic
 * constants tries to ensure a smooth LCP load which is high enough to
 * avoid slowing down LCPs such that we run out of REDO logs. Also low
 * enough to avoid that we use so much CPU on LCPs that we block out
 * most user transactions. We also want to avoid destroying real-time
 * characteristics due to LCPs.
 *
 * See much longer explanation of these values below.
 */
#define MAX_LCP_WORDS_PER_BATCH (1500)

#define HIGH_LOAD_LEVEL 32
#define VERY_HIGH_LOAD_LEVEL 48
#define NUMBER_OF_SIGNALS_PER_SCAN_BATCH 3
#define MAX_RAISE_PRIO_MEMORY 16

void
Backup::execSTTOR(Signal* signal) 
{
  jamEntry();                            

  const Uint32 startphase  = signal->theData[1];
  const Uint32 typeOfStart = signal->theData[7];

  if (startphase == 1)
  {
    ndbrequire((c_lqh = (Dblqh*)globalData.getBlock(DBLQH, instance())) != 0);
    ndbrequire((c_tup = (Dbtup*)globalData.getBlock(DBTUP, instance())) != 0);
    ndbrequire((c_lgman =
                (Lgman*)globalData.getBlock(LGMAN, instance())) != 0);

    m_words_written_this_period = 0;
    m_backup_words_written_this_period = 0;
    last_disk_write_speed_report = 0;
    next_disk_write_speed_report = 0;
    m_monitor_words_written = 0;
    m_backup_monitor_words_written = 0;
    m_periods_passed_in_monitor_period = 0;
    m_monitor_snapshot_start = NdbTick_getCurrentTicks();
    m_curr_lcp_id = 0;
    m_curr_disk_write_speed = c_defaults.m_disk_write_speed_max_own_restart;
    m_curr_backup_disk_write_speed =
      c_defaults.m_disk_write_speed_max_own_restart;
    m_overflow_disk_write = 0;
    m_backup_overflow_disk_write = 0;
    slowdowns_due_to_io_lag = 0;
    slowdowns_due_to_high_cpu = 0;
    disk_write_speed_set_to_min = 0;
    m_is_lcp_running = false;
    m_is_backup_running = false;
    m_is_any_node_restarting = false;
    m_node_restart_check_sent = false;
    m_our_node_started = false;
    m_lcp_ptr_i = RNIL;
    m_first_lcp_started = false;
    m_newestRestorableGci = 0;
    m_delete_lcp_files_ongoing = false;
    m_reset_disk_speed_time = NdbTick_getCurrentTicks();
    m_reset_delay_used = Backup::DISK_SPEED_CHECK_DELAY;
    c_initial_start_lcp_not_done_yet = false;
    m_last_redo_check_time = getHighResTimer();
    m_redo_alert_factor = 1;
    m_redo_alert_state = RedoStateRep::NO_REDO_ALERT;
    signal->theData[0] = BackupContinueB::RESET_DISK_SPEED_COUNTER;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal,
                        Backup::DISK_SPEED_CHECK_DELAY, 1);
  }
  if (startphase == 3)
  {
    jam();

    g_TypeOfStart = typeOfStart;
    if (g_TypeOfStart == NodeState::ST_INITIAL_START ||
        g_TypeOfStart == NodeState::ST_INITIAL_NODE_RESTART)
    {
      jam();
      c_initial_start_lcp_not_done_yet = true;
    }
    signal->theData[0] = reference();
    sendSignal(NDBCNTR_REF, GSN_READ_NODESREQ, signal, 1, JBB);
    return;
  }//if

  if (startphase == 7)
  {
    m_monitor_words_written = 0;
    m_backup_monitor_words_written = 0;
    m_periods_passed_in_monitor_period = 0;
    m_monitor_snapshot_start = NdbTick_getCurrentTicks();
    m_curr_disk_write_speed = c_defaults.m_disk_write_speed_min;
    m_curr_backup_disk_write_speed = c_defaults.m_disk_write_speed_min;
    m_our_node_started = true;
    c_initial_start_lcp_not_done_yet = false;
  }

  if(startphase == 7 && g_TypeOfStart == NodeState::ST_INITIAL_START &&
     c_masterNodeId == getOwnNodeId() && !isNdbMtLqh()){
    jam();
    createSequence(signal);
    return;
  }//if
  
  sendSTTORRY(signal);  
  return;
}//Dbdict::execSTTOR()

void
Backup::execREAD_NODESCONF(Signal* signal)
{
  jamEntry();
  ReadNodesConf * conf = (ReadNodesConf *)signal->getDataPtr();
 
  c_aliveNodes.clear();

  Uint32 count = 0;
  for (Uint32 i = 0; i<MAX_NDB_NODES; i++) {
    jam();
    if(NdbNodeBitmask::get(conf->allNodes, i)){
      jam();
      count++;

      NodePtr node;
      ndbrequire(c_nodes.seizeFirst(node));
      
      node.p->nodeId = i;
      if(NdbNodeBitmask::get(conf->inactiveNodes, i)) {
        jam();
	node.p->alive = 0;
      } else {
        jam();
	node.p->alive = 1;
	c_aliveNodes.set(i);
      }//if
    }//if
  }//for
  c_masterNodeId = conf->masterNodeId;
  ndbrequire(count == conf->noOfNodes);
  sendSTTORRY(signal);
}

void
Backup::sendSTTORRY(Signal* signal)
{
  signal->theData[0] = 0;
  signal->theData[3] = 1;
  signal->theData[4] = 3;
  signal->theData[5] = 7;
  signal->theData[6] = 255; // No more start phases from missra
  BlockReference cntrRef = !isNdbMtLqh() ? NDBCNTR_REF : BACKUP_REF;
  sendSignal(cntrRef, GSN_STTORRY, signal, 7, JBB);
}

void
Backup::createSequence(Signal* signal)
{
  UtilSequenceReq * req = (UtilSequenceReq*)signal->getDataPtrSend();
  
  req->senderData  = RNIL;
  req->sequenceId  = NDB_BACKUP_SEQUENCE;
  req->requestType = UtilSequenceReq::Create;
  
  sendSignal(DBUTIL_REF, GSN_UTIL_SEQUENCE_REQ, 
	     signal, UtilSequenceReq::SignalLength, JBB);
}

void
Backup::handle_overflow(Uint64& overflow_disk_write,
                        Uint64& words_written_this_period,
                        Uint64& curr_disk_write_speed)
{
  jam();
  /**
   * If we overflowed in the last period, count it in 
   * this new period, potentially overflowing again into
   * future periods...
   * 
   * The overflow can only come from the last write we did in this
   * period, but potentially this write is bigger than what we are
   * allowed to write during one period.
   *
   * Calculate the overflow to pass into the new period
   * (overflowThisPeriod). It can never be more than what is
   * allowed to be written during a period.
   *
   * We could rarely end up in the case that the overflow of the
   * last write in the period even overflows the entire next period.
   * If so we put this into the remainingOverFlow and put this into
   * overflow_disk_write (in this case nothing will be written in
   * this period so ready_to_write need not worry about this case
   * when setting overflow_disk_write since it isn't written any time
   * in this case and in all other cases only written by the last write
   * in a period.
   *
   * This routine is called both for collective LCP and Backup overflow
   * and for only Backup overflow.
   */
  Uint32 overflowThisPeriod = MIN(overflow_disk_write, 
                                  curr_disk_write_speed + 1);
    
  /* How much overflow remains after this period? */
  Uint32 remainingOverFlow = overflow_disk_write - overflowThisPeriod;
  
  if (overflowThisPeriod)
  {
    jam();
#ifdef DEBUG_CHECKPOINTSPEED
    ndbout_c("Overflow of %u bytes (max/period is %u bytes)",
             overflowThisPeriod * 4, curr_disk_write_speed * 4);
#endif
    if (remainingOverFlow)
    {
      jam();
#ifdef DEBUG_CHECKPOINTSPEED
      ndbout_c("  Extra overflow : %u bytes, will take %u further periods"
               " to clear", remainingOverFlow * 4,
                 remainingOverFlow / curr_disk_write_speed);
#endif
    }
  }
  words_written_this_period = overflowThisPeriod;
  overflow_disk_write = remainingOverFlow;
}

void
Backup::calculate_next_delay(const NDB_TICKS curr_time)
{
  /**
   * Adjust for upto 10 millisecond delay of this signal. Longer
   * delays will not be handled, in this case the system is most
   * likely under too high load and it won't matter very much that
   * we decrease the speed of checkpoints.
   *
   * We use a technique where we allow an overflow write in one
   * period. This overflow will be removed from the next period
   * such that the load will at average be as specified.
   * Calculate new delay time based on if we overslept or underslept
   * this time. We will never regulate more than 10ms, if the
   * oversleep is bigger than we will simply ignore it. We will
   * decrease the delay by as much as we overslept or increase it by
   * as much as we underslept.
   */
  int delay_time = m_reset_delay_used;
  int sig_delay = int(NdbTick_Elapsed(m_reset_disk_speed_time,
                                      curr_time).milliSec());
  if (sig_delay > delay_time + 10)
  {
    delay_time = Backup::DISK_SPEED_CHECK_DELAY - 10;
  }
  else if (sig_delay < delay_time - 10)
  {
    delay_time = Backup::DISK_SPEED_CHECK_DELAY + 10;
  }
  else
  {
    delay_time = Backup::DISK_SPEED_CHECK_DELAY -
                 (sig_delay - delay_time);
  }
  m_periods_passed_in_monitor_period++;
  m_reset_delay_used= delay_time;
  m_reset_disk_speed_time = curr_time;
#if 0
  ndbout << "Signal delay was = " << sig_delay;
  ndbout << " Current time = " << curr_time << endl;
  ndbout << " Delay time will be = " << delay_time << endl << endl;
#endif
}

void
Backup::report_disk_write_speed_report(Uint64 bytes_written_this_period,
                                       Uint64 backup_bytes_written_this_period,
                                       Uint64 millis_passed)
{
  Uint32 report = next_disk_write_speed_report;
  disk_write_speed_rep[report].backup_bytes_written =
    backup_bytes_written_this_period;
  disk_write_speed_rep[report].backup_lcp_bytes_written =
    bytes_written_this_period;
  disk_write_speed_rep[report].millis_passed =
    millis_passed;
  disk_write_speed_rep[report].redo_bytes_written =
    c_lqh->report_redo_written_bytes();
  disk_write_speed_rep[report].target_disk_write_speed =
    m_curr_disk_write_speed * CURR_DISK_SPEED_CONVERSION_FACTOR_TO_SECONDS;
  disk_write_speed_rep[report].target_backup_disk_write_speed =
    m_curr_backup_disk_write_speed * CURR_DISK_SPEED_CONVERSION_FACTOR_TO_SECONDS;

  next_disk_write_speed_report++;
  if (next_disk_write_speed_report == DISK_WRITE_SPEED_REPORT_SIZE)
  {
    next_disk_write_speed_report = 0;
  }
  if (next_disk_write_speed_report == last_disk_write_speed_report)
  {
    last_disk_write_speed_report++;
    if (last_disk_write_speed_report == DISK_WRITE_SPEED_REPORT_SIZE)
    {
      last_disk_write_speed_report = 0;
    }
  }
}

#define DELETE_RECOVERY_WORK 120
/**
 * This method is a check that we haven't been writing faster than we're
 * supposed to during the last interval.
 */
void
Backup::monitor_disk_write_speed(const NDB_TICKS curr_time,
                                 const Uint64 millisPassed)
{
  /**
   * Independent check of DiskCheckpointSpeed.
   * We check every second or so that we are roughly sticking
   * to our diet.
   */
  jam();
  const Uint64 periodsPassed =
    (millisPassed / DISK_SPEED_CHECK_DELAY) + 1;
  const Uint64 quotaWordsPerPeriod = m_curr_disk_write_speed;
  const Uint64 quotaWordsPerPeriodBackup = m_curr_backup_disk_write_speed;
  const Uint64 maxOverFlowWords = c_defaults.m_maxWriteSize / 4;
  const Uint64 maxExpectedWords = (periodsPassed * quotaWordsPerPeriod) +
                                  maxOverFlowWords;
  const Uint64 maxExpectedWordsBackup = (periodsPassed *
                                         quotaWordsPerPeriodBackup) +
                                         maxOverFlowWords;
        
  if (unlikely((m_monitor_words_written > maxExpectedWords) ||
               (m_backup_monitor_words_written > maxExpectedWordsBackup)))
  {
    jam();
    /**
     * In the last monitoring interval, we have written more words
     * than allowed by the quota (DiskCheckpointSpeed), including
     * transient spikes due to a single MaxBackupWriteSize write
     */
    ndbout << "Backup : Excessive Backup/LCP write rate in last"
           << " monitoring period - recorded = "
           << (m_monitor_words_written * 4 * 1000) / millisPassed
           << " bytes/s, "
           << endl
           << "Recorded writes to backup: "
           << (m_backup_monitor_words_written * 4 * 1000) / millisPassed
           << " bytes/s, "
           << endl;
    ndbout << "Current speed is = "
           << m_curr_disk_write_speed *
                CURR_DISK_SPEED_CONVERSION_FACTOR_TO_SECONDS
           << " bytes/s"
           << endl;
    ndbout << "Current backup speed is = "
           << m_curr_backup_disk_write_speed *
                CURR_DISK_SPEED_CONVERSION_FACTOR_TO_SECONDS
           << " bytes/s"
           << endl;
    ndbout << "Backup : Monitoring period : " << millisPassed
           << " millis. Bytes written : " << (m_monitor_words_written * 4)
           << ".  Max allowed : " << (maxExpectedWords * 4) << endl;
    ndbout << "Backup : Monitoring period : " << millisPassed
           << " millis. Bytes written : "
           << (m_backup_monitor_words_written * 4)
           << ".  Max allowed : " << (maxExpectedWordsBackup * 4) << endl;
    ndbout << "Actual number of periods in this monitoring interval: ";
    ndbout << m_periods_passed_in_monitor_period;
    ndbout << " calculated number was: " << periodsPassed << endl;
  }
  report_disk_write_speed_report(4 * m_monitor_words_written,
                                 4 * m_backup_monitor_words_written,
                                 millisPassed);
  /**
   * The LCP write rate is removed from the calculated LCP change rate to
   * derive the lag (a lag is a positive number, if we are ahead of the
   * calculated rate we report it as a negative number).
   * We keep track of the lag since the start of the LCP and since the
   * start of the previous LCP.
   */
  Int64 lag = m_lcp_change_rate - 
              ((4 * m_monitor_words_written) -
               (4 * m_backup_monitor_words_written));
  m_lcp_lag[1] += lag;

  m_monitor_words_written = 0;
  m_backup_monitor_words_written = 0;
  m_periods_passed_in_monitor_period = 0;
  m_monitor_snapshot_start = curr_time;
}

void
Backup::debug_report_redo_control(Uint32 cpu_usage)
{
#ifdef DEBUG_REDO_CONTROL
  {
    Uint64 millis_passed;
    Uint64 backup_lcp_bytes_written;
    Uint64 backup_bytes_written;
    Uint64 redo_bytes_written;
    calculate_disk_write_speed_seconds_back(1,
                                            millis_passed,
                                            backup_lcp_bytes_written,
                                            backup_bytes_written,
                                            redo_bytes_written,
                                            true);
    backup_bytes_written *= Uint64(1000);
    backup_bytes_written /= (millis_passed * Uint64(1024));
    backup_lcp_bytes_written *= Uint64(1000);
    backup_lcp_bytes_written /= (millis_passed * Uint64(1024));
    redo_bytes_written *= Uint64(1000);
    redo_bytes_written /= (millis_passed * Uint64(1024));

   /* Report new disk write speed and last seconds achievement on disk */
   DEB_REDO_CONTROL(("(%u)Current disk write speed is %llu kB/sec"
                     " and current backup disk write speed is %llu kB/sec"
                     ", last sec REDO write speed %llu kB/sec, "
                     "LCP+Backup write speed %llu kB/sec"
                     ", Backup write speed %llu kB/sec"
                     ", cpu_usage: %u",
                      instance(),
                      ((m_curr_disk_write_speed *
                        CURR_DISK_SPEED_CONVERSION_FACTOR_TO_SECONDS) /
                       Uint64(1024)),
                      ((m_curr_backup_disk_write_speed *
                        CURR_DISK_SPEED_CONVERSION_FACTOR_TO_SECONDS) /
                       Uint64(1024)),
                     redo_bytes_written,
                     backup_lcp_bytes_written,
                     backup_bytes_written,
                     cpu_usage));
 }
#else
 (void)cpu_usage;
#endif
}

void
Backup::execREDO_STATE_REP(Signal* signal)
{
  RedoStateRep *rep = (RedoStateRep*)signal->getDataPtr();
  ndbrequire(rep->receiverInfo == RedoStateRep::ToBackup);
  m_global_redo_alert_state = (RedoStateRep::RedoAlertState)rep->redoState;
}

/**
 * Initialise LCP timers at the time we hear of the first writes to the
 * REDO log. Could also be initialised by the start of the first LCP.
 */
void
Backup::init_lcp_timers(Uint64 redo_written_since_last_call)
{
  if (redo_written_since_last_call > 0)
  {
    if (!NdbTick_IsValid(m_lcp_start_time))
    {
      m_lcp_start_time = getHighResTimer();
      m_prev_lcp_start_time = m_lcp_start_time;
    }
  }
}

void
Backup::lcp_start_point()
{
  /**
   * A new LCP is starting up, we need to keep track of this to handle
   * REDO control.
   * The start and end points of LCPs currently only come with an
   * accuracy of about 1 second, so if the LCP time is shorter than
   * this we can definitely ignore any REDO alerts.
   */
  if (!NdbTick_IsValid(m_prev_lcp_start_time))
  {
    jam();
    m_prev_lcp_start_time = getHighResTimer();
  }
  else
  {
    m_prev_lcp_start_time = m_lcp_start_time;
  }
  m_first_lcp_started = true;
  m_lcp_start_time = getHighResTimer();
  ndbrequire(NdbTick_IsValid(m_lcp_start_time));
  m_lcp_current_cut_point = m_prev_lcp_start_time;
  m_update_size_lcp[0] = m_update_size_lcp[1];
  m_update_size_lcp[1] = m_update_size_lcp_last;
  m_insert_size_lcp[0] = m_insert_size_lcp[1];
  m_insert_size_lcp[1] = m_insert_size_lcp_last;
  m_delete_size_lcp[0] = m_delete_size_lcp[1];
  m_delete_size_lcp[1] = m_delete_size_lcp_last;
  DEB_REDO_CONTROL(("(%u)m_insert_size_lcp[0]: %llu, "
                    "m_insert_size_lcp[1]: %llu, "
                    "m_insert_size_lcp_last: %llu",
                    instance(),
                    m_insert_size_lcp[0],
                    m_insert_size_lcp[1],
                    m_insert_size_lcp_last));
}

void
Backup::lcp_end_point()
{
  NDB_TICKS current_time = getHighResTimer();
  ndbrequire(NdbTick_IsValid(m_lcp_start_time));
  m_last_lcp_exec_time_in_ms =
    NdbTick_Elapsed(m_lcp_start_time, current_time).milliSec();
  m_lcp_current_cut_point = m_lcp_start_time;
  m_update_size_lcp[0] = m_update_size_lcp[1];
  m_insert_size_lcp[0] = m_insert_size_lcp[1];
  m_delete_size_lcp[0] = m_delete_size_lcp[1];

  m_lcp_lag[0] = m_lcp_lag[1];
  m_lcp_lag[1] = Int64(0);

  reset_lcp_timing_factors();
  DEB_REDO_CONTROL(("(%u)LCP End: m_insert_size_lcp[0]: %llu",
                    instance(),
                    m_insert_size_lcp[0]));
}

Uint64
Backup::init_change_size(Uint64 update_size,
                         Uint64 insert_size,
                         Uint64 delete_size,
                         Uint64 total_memory)
{
  /**
   * The initial value for change_size is based on that the new
   * rows or deleted rows are always changes, but updates can
   * at times be updates of the same row. We use an exponential
   * probability distribution that a row has been updated or not.
   */
  Uint64 change_size = insert_size + delete_size;
  long double f_total_memory = (long double)total_memory;
  long double f_change_size = update_size;
  long double f_change_percentage = f_change_size / f_total_memory;
  long double f_real_change_percentage = ((long double)1) -
                                       exp(-f_change_percentage);
  long double f_real_change_size = f_real_change_percentage *
                                   f_total_memory;
  change_size += (Uint64)f_real_change_size;
  return change_size;
}

Uint64
Backup::modify_change_size(Uint64 update_size,
                           Uint64 insert_size,
                           Uint64 delete_size,
                           Uint64 total_size,
                           Uint64 change_size)
{
  /**
   * Now we have calculated an estimate that is comparable
   * to the row_change_count that we get per fragment before
   * calculating the number of parts to checkpoint.
   * 
   * The next step is now to modify this estimate based on
   * the amount of inserts and deletes compared to the updates.
   */
  Uint64 updates_percent = (update_size * Uint64(1005)) /
                          (Uint64(10) * total_size);
  Uint64 inserts_percent = (insert_size * Uint64(1005)) /
                          (Uint64(10) * total_size);
  Uint64 insert_recovery_work = (Uint64)get_insert_recovery_work();
  inserts_percent *= insert_recovery_work;
  inserts_percent /= Uint64(100);
  Uint64 deletes_percent = (delete_size * Uint64(1005)) /
                          (Uint64(10) * total_size);
  deletes_percent *= Uint64(DELETE_RECOVERY_WORK);
  deletes_percent /= Uint64(100);
  Uint64 change_factor = updates_percent +
                         inserts_percent +
                         deletes_percent;
  change_size *= change_factor;
  change_size /= Uint64(100);
  return change_size;
}

Uint32
Backup::calculate_parts(Uint64 change_size,
                        Uint64 total_memory)
{
  Uint64 part_total_memory = total_memory / Uint64(10);
  Uint32 min_parts = calculate_min_parts(total_memory,
                                         change_size,
                                         part_total_memory,
                                         total_memory);
  return min_parts;
}

void
Backup::calculate_seconds_since_lcp_cut(Uint64& seconds_since_lcp_cut)
{
  NDB_TICKS now = getHighResTimer();
  if (!NdbTick_IsValid(m_lcp_current_cut_point))
  {
    jam();
    seconds_since_lcp_cut = 0;
    return; 
  }
  seconds_since_lcp_cut =
    NdbTick_Elapsed(m_lcp_current_cut_point, now).seconds();
}

Uint64
Backup::calculate_change_rate(Uint64 change_size,
                              Uint64& seconds_since_lcp_cut)
{
  if (seconds_since_lcp_cut < 3)
  {
    jam();
    /**
     * We ignore very short LCPs, in this case it is hard to see
     * how we could run out of REDO log and need more disk write
     * speed.
     */
    return 0;
  }
  Uint64 change_size_per_sec = change_size / seconds_since_lcp_cut;
  return change_size_per_sec;
}

void
Backup::scale_write_sizes(Uint64& update_size,
                          Uint64& insert_size,
                          Uint64& delete_size,
                          Uint64& seconds_since_lcp_cut,
                          Uint64& lcp_time_in_secs)
{
  lcp_time_in_secs =
    m_last_lcp_exec_time_in_ms / Uint64(1000);
  calculate_seconds_since_lcp_cut(seconds_since_lcp_cut);
  if (seconds_since_lcp_cut == 0)
  {
    jam();
    update_size = 0;
    insert_size = 0;
    delete_size = 0;
    return;
  }
  update_size *= lcp_time_in_secs;
  insert_size *= lcp_time_in_secs;
  delete_size *= lcp_time_in_secs;
  update_size /= seconds_since_lcp_cut;
  insert_size /= seconds_since_lcp_cut;
  delete_size /= seconds_since_lcp_cut;
}

Uint64
Backup::calculate_checkpoint_rate(Uint64 update_size,
                                  Uint64 insert_size,
                                  Uint64 delete_size,
                                  Uint64 total_memory,
                                  Uint64& seconds_since_lcp_cut,
                                  Uint64& lcp_time_in_secs)
{
  Uint64 checkpoint_size = 0;
  Uint32 all_parts = 0;
  Uint64 all_size = 0;
  Uint64 change_size = 0;
  Uint64 mod_change_size = 0;
  Uint64 total_size = update_size + insert_size + delete_size;
  if (total_size != 0)
  {
    if (delete_size > insert_size)
    {
      update_size += insert_size;
      delete_size -= insert_size;
      insert_size = 0;
    }
    else
    {
      update_size += delete_size;
      insert_size -= delete_size;
      delete_size = 0;
    }
    scale_write_sizes(update_size,
                      insert_size,
                      delete_size,
                      seconds_since_lcp_cut,
                      lcp_time_in_secs);
    change_size = init_change_size(update_size,
                                   insert_size,
                                   delete_size,
                                   total_memory);
    mod_change_size = modify_change_size(update_size,
                                         insert_size,
                                         delete_size,
                                         total_size,
                                         change_size);
    all_parts = calculate_parts(mod_change_size, total_memory);
    all_size = total_memory * Uint64(all_parts);
    all_size /= Uint64(BackupFormat::NDB_MAX_LCP_PARTS);
    change_size = (BackupFormat::NDB_MAX_LCP_PARTS - all_parts) *
                  change_size;
    change_size /= BackupFormat::NDB_MAX_LCP_PARTS;
    checkpoint_size = all_size + change_size;
  }
  Uint64 change_rate = calculate_change_rate(checkpoint_size,
                                             lcp_time_in_secs);
  DEB_REDO_CONTROL(("(%u)update_size: %llu MB, insert_size: %llu MB,"
                    " delete_size: %llu MB, checkpoint_size: %llu MB"
                    ", all_parts: %u, total_memory: %llu MB, "
                    "all_size: %llu MB, change_size: %llu MB, "
                    "mod_change_size: %llu MB, "
                    "seconds_since_lcp_cut: %llu"
                    ", lcp_time_in_secs: %llu",
                    instance(),
                    update_size / (Uint64(1024) * Uint64(1024)),
                    insert_size / (Uint64(1024) * Uint64(1024)),
                    delete_size / (Uint64(1024) * Uint64(1024)),
                    checkpoint_size / (Uint64(1024) * Uint64(1024)),
                    all_parts,
                    total_memory / (Uint64(1024 * Uint64(1024))),
                    all_size / (Uint64(1024) * Uint64(1024)),
                    change_size / (Uint64(1024) * Uint64(1024)),
                    mod_change_size / (Uint64(1024) * Uint64(1024)),
                    seconds_since_lcp_cut,
                    lcp_time_in_secs));
  return change_rate;
}

void
Backup::calculate_redo_parameters(Uint64 redo_usage,
                                  Uint64 redo_size,
                                  Uint64 redo_written_since_last_call,
                                  Uint64 millis_since_last_call,
                                  Uint64& redo_percentage,
                                  Uint64& max_redo_used_before_cut,
                                  Uint64& mean_redo_used_before_cut,
                                  Uint64& mean_redo_speed_per_sec,
                                  Uint64& current_redo_speed_per_sec,
                                  Uint64& redo_available)
{
  /* redo_size and redo_usage is in MBytes, convert to bytes */
  redo_size *= (Uint64(1024) * Uint64(1024));
  redo_usage *= (Uint64(1024) * Uint64(1024));
  redo_available = redo_size - redo_usage;
  redo_percentage = redo_usage * Uint64(100);
  redo_percentage /= redo_size;
  current_redo_speed_per_sec = redo_written_since_last_call * Uint64(1000);
  current_redo_speed_per_sec /= millis_since_last_call;
  if (current_redo_speed_per_sec > m_max_redo_speed_per_sec)
  {
    jam();
    m_max_redo_speed_per_sec = current_redo_speed_per_sec;
  }
  mean_redo_speed_per_sec = 0;
  Uint64 seconds_since_lcp_cut = 0;
  if (NdbTick_IsValid(m_lcp_current_cut_point))
  {
    jam();
    NDB_TICKS current_time = getHighResTimer();
    seconds_since_lcp_cut =
      NdbTick_Elapsed(m_lcp_current_cut_point, current_time).seconds();
  }
  if (seconds_since_lcp_cut != 0)
  {
    jam();
    mean_redo_speed_per_sec = redo_usage / seconds_since_lcp_cut;
  }
  /**
   * We assume that LCP execution time is Poisson-distributed.
   * This means that our mean estimated time is the same even
   * if the LCP has been ongoing for a while (Poisson distribution
   * has no memory). It doesn't matter so much if this estimate
   * isn't 100% correct, it will at least not be overoptimistic.
   *
   * Thus we estimate the time to complete the next LCP to be
   * the time of the last LCP.
   */
  max_redo_used_before_cut = m_max_redo_speed_per_sec *
                             m_last_lcp_exec_time_in_ms;
  max_redo_used_before_cut /= Uint64(1000);

  mean_redo_used_before_cut = mean_redo_speed_per_sec *
                              m_last_lcp_exec_time_in_ms;
  mean_redo_used_before_cut /= Uint64(1000);
}

void
Backup::change_alert_state_redo_percent(Uint64 redo_percentage)
{
  /**
   * If the fill level of the REDO log reaches beyond 60% we set
   * it in critical state independent of calculations on REDO
   * speed. Similarly when going beyond 40% we set it in high
   * alert state. Using more than 40% of the REDO log is
   * not a desired state to run in. This is both too close to
   * the end to be comfortable and it also extends the time
   * to recover at a restart substantially.
   */
  m_redo_alert_state = RedoStateRep::NO_REDO_ALERT;
  if (redo_percentage > Uint64(60))
  {
    jam();
    m_redo_alert_state = RedoStateRep::REDO_ALERT_CRITICAL;
  }
  else if (redo_percentage > Uint64(40))
  {
    jam();
    m_redo_alert_state = RedoStateRep::REDO_ALERT_HIGH;
  }
  else if (redo_percentage > Uint64(25))
  {
    jam();
    m_redo_alert_state = RedoStateRep::REDO_ALERT_LOW;
  }
}

void
Backup::change_alert_state_redo_usage(Uint64 max_redo_used_before_cut,
                                      Uint64 mean_redo_used_before_cut,
                                      Uint64 redo_available)
{
  if (m_redo_alert_state != RedoStateRep::REDO_ALERT_CRITICAL)
  {
    jam();
    /**
     * We have estimated the REDO usage until the next LCP will cut it again.
     * The first estimate is based on the maximum speed we have seen so far.
     * The second estimate is based on the mean speed we have seen since
     * the first current REDO log record was generated.
     *
     * If we write at max speed and we estimate this to run out of REDO space
     * we are at a high alert state. If we can use only 40% of this to run out
     * of REDO log we are at a critical state.
     *
     * If we run at mean speed and we can run out of REDO space we are obviously
     * in a critical state, even with only an estimate to fill half of this we
     * are in a critical state and if we estimate to fill a third of this we are
     * in a high alert state.
     *
     * We don't even attempt those checks if we haven't got good measures of
     * times until the next REDO cut.
     */
    Uint64 max_critical_limit = (Uint64(2) * max_redo_used_before_cut) / Uint64(5);
    Uint64 max_high_limit = max_redo_used_before_cut;
    Uint64 mean_critical_limit = mean_redo_used_before_cut / Uint64(2);
    Uint64 mean_high_limit = mean_redo_used_before_cut / Uint64(3);

    if (redo_available < max_critical_limit)
    {
      jam();
      m_redo_alert_state = RedoStateRep::REDO_ALERT_CRITICAL;
    }
    else if (redo_available < mean_critical_limit)
    {
      jam();
      m_redo_alert_state = RedoStateRep::REDO_ALERT_CRITICAL;
    }
    else if (redo_available < max_high_limit)
    {
      jam();
      m_redo_alert_state = RedoStateRep::REDO_ALERT_HIGH;
    }
    else if (redo_available < mean_high_limit)
    {
      jam();
      m_redo_alert_state = RedoStateRep::REDO_ALERT_HIGH;
    }
  }
}

void
Backup::handle_global_alert_state(
  Signal *signal,
  RedoStateRep::RedoAlertState save_redo_alert_state)
{
  m_local_redo_alert_state = m_redo_alert_state;
  if (save_redo_alert_state != m_redo_alert_state)
  {
    jam();
    RedoStateRep *rep = (RedoStateRep*)signal->getDataPtrSend();
    rep->receiverInfo = RedoStateRep::ToNdbcntr;
    rep->redoState = m_redo_alert_state;
    //sendSignal(NDBCNTR_REF, GSN_REDO_STATE_REP, signal, 2, JBB);
  }
  if (m_global_redo_alert_state > m_redo_alert_state)
  {
    jam();
    m_redo_alert_state = m_global_redo_alert_state;
  }
}

void
Backup::set_redo_alert_factor(Uint64 redo_percentage)
{
  m_redo_alert_factor = 1;
  if (m_redo_alert_state == RedoStateRep::REDO_ALERT_CRITICAL)
  {
    jam();
    m_redo_alert_factor = 24;
  }
  else if (m_redo_alert_state == RedoStateRep::REDO_ALERT_HIGH)
  {
    jam();
    m_redo_alert_factor = 8;
  }
  else if (m_redo_alert_state == RedoStateRep::REDO_ALERT_LOW)
  {
    jam();
    m_redo_alert_factor = 4;
  }
}

void
Backup::set_lcp_timing_factors(Uint64 seconds_since_lcp_cut,
                               Uint64 lcp_time_in_secs)
{
  if (lcp_time_in_secs == 0)
  {
    return;
  }
  /**
   * seconds_since_lcp_cut normally goes to a bit more than
   * two times the LCP time. If the LCP time increases by more
   * than 6 seconds we try to increase the disk write speed to
   * handle this. If the seconds since last cut is increasing
   * even to double the LCP time we increase the factor even
   * more.
   *
   * There is no need to set those factors in a dramatic manner.
   * These factors are used to keep LCP times low to ensure that
   * recovery times are low. They assist in protecting the REDO
   * log from head meeting tail, but it isn't the main purpose.
   * There are many other mechanisms that take care of this
   * purpose.
   */
  Uint64 low_threshold = Uint64(2) * lcp_time_in_secs;
  low_threshold += Uint64(6);
  Uint64 high_threshold = Uint64(3) * lcp_time_in_secs;
  high_threshold += Uint64(6);
  if (seconds_since_lcp_cut > low_threshold)
  {
    jam();
    m_lcp_timing_counter = 2;
    Uint64 new_timing_factor = Uint64(110);
    if (seconds_since_lcp_cut > high_threshold)
    {
      jam();
      new_timing_factor = Uint64(120);
    }
    if (new_timing_factor > m_lcp_timing_factor)
    {
      jam();
      m_lcp_timing_factor = new_timing_factor;
    }
  }
  /**
   * Ensure that the effects of REDO Alert Level stick to some
   * level all through the next LCP as well. This will help
   * bringing us permanently down in REDO Alert levels.
   */
  if (m_redo_alert_state == RedoStateRep::REDO_ALERT_LOW)
  {
    jam();
    m_lcp_timing_counter = 2;
    Uint64 new_timing_factor = Uint64(115);
    if (new_timing_factor > m_lcp_timing_factor)
    {
      jam();
      m_lcp_timing_factor = new_timing_factor;
    }
  }
  else if (m_redo_alert_state == RedoStateRep::REDO_ALERT_HIGH)
  {
    jam();
    m_lcp_timing_counter = 2;
    Uint64 new_timing_factor = Uint64(125);
    if (new_timing_factor > m_lcp_timing_factor)
    {
      jam();
      m_lcp_timing_factor = new_timing_factor;
    }
  }
  else if (m_redo_alert_state == RedoStateRep::REDO_ALERT_CRITICAL)
  {
    jam();
    m_lcp_timing_counter = 2;
    Uint64 new_timing_factor = Uint64(135);
    if (new_timing_factor > m_lcp_timing_factor)
    {
      jam();
      m_lcp_timing_factor = new_timing_factor;
    }
  }
}

void
Backup::reset_lcp_timing_factors()
{
  if (m_lcp_timing_counter > 0)
  {
    jam();
    m_lcp_timing_counter--;
    if (m_lcp_timing_counter == 0)
    {
      jam();
      m_lcp_timing_factor = Uint64(100);
    }
  }
}

void
Backup::set_proposed_disk_write_speed(Uint64 current_redo_speed_per_sec,
                                      Uint64 mean_redo_speed_per_sec,
                                      Uint64 seconds_since_lcp_cut)
{
  /**
   * When LCPs are increasing the time it takes to execute an LCP we try to
   * get it back by increasing the disk write speed until the end of the
   * next LCP. This is controlled by the m_lcp_timing_factor variable. This
   * variable is set to 100 when no such issues are at hand.
   */
  m_proposed_disk_write_speed *= m_lcp_timing_factor;
  m_proposed_disk_write_speed /= Uint64(100);

  /**
   * We save the proposed disk write speed with multiplication of LCP timing
   * factor as the m_lcp_change_rate, this is the calculated change rate with
   * some long-term factors derived from m_lcp_timing_factor.
   *
   * The short-term proposed disk write speed in addition will contain
   * additional components to ensure that we actually deliver the calculated
   * LCP change rate.
   */
  m_lcp_change_rate = m_proposed_disk_write_speed;

  /**
   * The proposed disk write speed is not always achieved and we have some
   * level of slowness in responding to this setting, so we increase the
   * proposed disk write speed by 25% cater for this.
   *
   * There are many reasons why we won't achieve this speed. A few are:
   * 1) Variable completion of LCP execution in the LDMs in the cluster.
   * 2) High CPU usage when REDO log alert factor is still not activated
   * 3) Disk not keeping up temporarily
   * 4) Setting proposed disk write speed increases the maximum disk write
   *    speed, thus it can take a while before it affects the actual
   *    disk write speed since this is changed by an adaptive change
   *    algorithm.
   */
  m_proposed_disk_write_speed *= Uint64(125);
  m_proposed_disk_write_speed /= Uint64(100);

  Int64 lag = m_lcp_lag[0] + m_lcp_lag[1];
  Int64 lag_per_sec = 0;
  if (seconds_since_lcp_cut > 0)
  {
    lag_per_sec = lag / (Int64)seconds_since_lcp_cut;
  }
  if (current_redo_speed_per_sec > mean_redo_speed_per_sec)
  {
    jam();
    Uint64 factor = current_redo_speed_per_sec * Uint64(100);
    factor /= (mean_redo_speed_per_sec + 1);
    if (factor > Uint64(120))
    {
      jam();
      factor = Uint64(120);
    }
    /**
     * Increase the proposed disk write speed by up to 20% if we currently
     * generate more REDO logging compared to the mean. This is aiming to
     * cater for sudden increases in write activity to ensure that we start
     * acting quickly on those changes. At the same we put a dent on this
     * change to 20% increase. This avoids too high fluctuations in the
     * disk write speed.
     */
    m_proposed_disk_write_speed *= factor;
    m_proposed_disk_write_speed /= Uint64(100);
  }
  if (m_redo_alert_state == RedoStateRep::REDO_ALERT_LOW)
  {
    jam();
    /**
     * Add another 10% to proposed speed if we are at low
     * alert level.
     */
    m_proposed_disk_write_speed *= Uint64(110);
    m_proposed_disk_write_speed /= Uint64(100);
  }
  else if (m_redo_alert_state == RedoStateRep::REDO_ALERT_HIGH)
  {
    jam();
    /**
     * Add another 20% to proposed speed if we are at high
     * alert level.
     */
    m_proposed_disk_write_speed *= Uint64(120);
    m_proposed_disk_write_speed /= Uint64(100);
  }
  else if (m_redo_alert_state == RedoStateRep::REDO_ALERT_CRITICAL)
  {
    jam();
    /**
     * Add another 40% to proposed speed if we are at critical
     * alert level.
     */
    m_proposed_disk_write_speed *= Uint64(140);
    m_proposed_disk_write_speed /= Uint64(100);
  }
  else if (lag < Int64(0))
  {
    /**
     * There is no REDO Alert level and we are running faster than
     * necessary, we will slow down based on the calculated lag per
     * second (which when negative means that we are ahead). We will
     * never slow down more than 30%.
     */
    lag_per_sec = Int64(-1) * lag_per_sec; /* Make number positive */
    Uint64 percentage_decrease = Uint64(lag_per_sec) * Uint64(100);
    percentage_decrease /= (m_proposed_disk_write_speed + 1);
    if (percentage_decrease > Uint64(30))
    {
      jam();
      m_proposed_disk_write_speed *= Uint64(70);
      m_proposed_disk_write_speed /= Uint64(100);
    }
    else
    {
      jam();
      m_proposed_disk_write_speed -= lag_per_sec;
    }
  }
  if (lag > Int64(0))
  {
    /**
     * We don't keep up with the calculated LCP change rate.
     * We will increase the proposed disk write speed by up
     * to 100% to keep up with the LCP change rate.
     */
    jam();
    Uint64 percentage_increase = lag_per_sec * Uint64(100);
    percentage_increase /= (m_proposed_disk_write_speed + 1);
    if (percentage_increase > Uint64(100))
    {
      jam();
      m_proposed_disk_write_speed *= Uint64(2);
    }
    else
    {
      jam();
      m_proposed_disk_write_speed += lag_per_sec;
    }
  }
}

void
Backup::measure_change_speed(Signal *signal)
{
  if (true)
    return;
  /**
   * The aim of this function is to calculate the following values:
   * 1) m_redo_alert_state
   * 2) m_redo_alert_factor
   * 3) m_proposed_disk_write_speed
   *
   * The m_redo_alert_state variable is used to set the m_redo_alert_factor
   * that raises the priority of LCP writes towards other operation.
   *
   * The variable is kept consistent in the cluster to ensure that one
   * REDO log that is overloaded will also ensure that all other LDMs in
   * the cluster will speed up LCP execution.
   *
   * Based on this variable we raise the maximum speed based on the
   * configured disk write parameters.
   * This variable can also change the adaptive algorithm that slows down
   * LCP execution due to high CPU load. It ensures that we raise the
   * prio on LCP execution by ensuring that all LCP execution signals
   * are executed at A-level and we fill the buffers more actively when
   * set at alert levels.
   * Finally setting this variable to an alert level means that we speed up
   * handling of empty LCP fragments.
   *
   * The m_redo_alert_factor changes the amount of writes we will do in
   * one real-time break when executing at A-level.
   *
   * The proposed disk write speed is used to increase the maximum speed
   * used in the adaptive disk write speed algorithm if necessary.
   *
   * Calculation of the proposed disk write speed is fairly complicated.
   * The idea is to use the same mechanics used to decide how much an LCP
   * will execute on a fragment basis on a global level.
   *
   * get_redo_stats
   * --------------
   * To do this we keep track of the amount of changes we have done since
   * the start of the previous LCP. We keep track of this by adding the
   * average row size to a global update_size, insert_size and delete_size
   * in DBLQH. These variables are requested in the get_redo_stats call to
   * DBLQH.
   *
   * calculate_total_size
   * --------------------
   * To calculate the change size we use different change factors for
   * inserts and deletes. Deletes generate 20% more per byte compared
   * to updates and inserts generate less, 40% by default, compared to
   * updates. If we have both inserts and deletes we will only use
   * the larger of the two and the overlap is treated as updates.
   * This is the same mechanism used in the method calculate_row_change_count
   * used when deciding the number of parts to checkpoint for a specific
   * fragment.
   *
   * calculate_parts
   * ---------------
   * Updates can at times hit the same row, we estimate the number of updates
   * to the same row by using a Poisson distribution of writes to the rows.
   * This means that we can estimate the number of rows not written by using
   * an exponential distribution. Thus it is easy to calculate the percent of
   * data that has been written. Using this information we use the same
   * function (calculate_min_parts) to calculate the parts to checkpoint
   * on a global level, this function returns the number of parts with the
   * maximum number of parts being the BackupFormat::NDB_MAX_LCP_PARTS.
   *
   * calculate_change_rate
   * ---------------------
   * Finally we use the change size, the number of parts and the seconds since
   * the changes we used was started. This gives us a calculated proposed disk
   * write speed. To calculate we will retrieve the time since the start of
   * previous LCP.
   *
   * calculate_redo_parameters
   * -------------------------
   * We got redo_size, redo_usage and redo_written_since_last_call from the
   * call to get_redo_stats. Based on this information we calculate the
   * following variables.
   * redo_percentage:
   * ................
   * Percentage of REDO log currently in use. This is used directly to set the
   * m_redo_alert_factor.
   *
   * max_redo_used_before_cut:
   * mean_redo_used_before_cut:
   * redo_available:
   * ..........................
   * These three variables together are used to calculate if there is a risk
   * that we will run out of REDO log even without a high REDO percentage. If
   * so we will set the m_redo_alert_state based on these variables.
   * The max_redo_used_before_cut is an estimate of how much REDO log will
   * write before the next LCP is completed if maximum REDO write speed is
   * used. Similarly for mean_redo_used_before_cut but based on average REDO
   * write speed. redo_available is the amount of REDO log still available.
   *
   * mean_redo_speed_per_sec:
   * current_redo_speed_per_sec:
   * ...........................
   * These are used to see if we are currently very active in writing the
   * REDO log. If we are we will increase the proposed disk write speed a bit
   * as an effect of this.
   *
   * change_alert_state_redo_percent
   * -------------------------------
   * Based on redo_percentage we will set m_redo_alert_state.
   *
   * change_alert_state_redo_usage
   * -----------------------------
   * The above calculation based on max_redo_before_cut, mean_before_redo_cut,
   * and redo_available is performed here to set m_redo_alert_state
   * appropriately.
   *
   * handle_global_alert_state
   * -------------------------
   * Ensure that we are synchronised in our REDO alert state with other LDMs
   * in the cluster since the LCP protocol is global.
   *
   * set_redo_alert_factor
   * ---------------------
   * Set m_redo_alert_factor based on m_redo_alert_state and redo_percentage.
   *
   * calculate_change_rate
   * ---------------------
   * Calculate proposed disk write speed based on calculated value and on the
   * current activity level as reported in mean_redo_speed_per_sec and
   * current_redo_speed_per_sec. We will also increase to cater for some safety
   * levels and based on the m_redo_alert_state.
   */
  NDB_TICKS current_time = getHighResTimer();
  Uint64 millis_since_last_call =
    NdbTick_Elapsed(m_last_redo_check_time, current_time).milliSec();

  if (millis_since_last_call < 800)
  {
    jam();
    return;
  }
  m_last_redo_check_time = current_time;
  Uint64 redo_usage;
  Uint64 redo_size;
  Uint64 redo_written_since_last_call;
  Uint64 insert_size;
  Uint64 delete_size;
  Uint64 update_size;
  c_lqh->get_redo_stats(redo_usage,
                        redo_size,
                        redo_written_since_last_call,
                        update_size,
                        insert_size,
                        delete_size);

  if (redo_size == 0)
  {
    jam();
    return;
  }
  init_lcp_timers(redo_written_since_last_call);

  m_update_size_lcp_last = update_size;
  m_insert_size_lcp_last = insert_size;
  m_delete_size_lcp_last = delete_size;

  Uint64 redo_percentage;
  Uint64 max_redo_used_before_cut;
  Uint64 mean_redo_used_before_cut;
  Uint64 mean_redo_speed_per_sec;
  Uint64 current_redo_speed_per_sec;
  Uint64 redo_available;
  calculate_redo_parameters(redo_usage,
                            redo_size,
                            redo_written_since_last_call,
                            millis_since_last_call,
                            redo_percentage,
                            max_redo_used_before_cut,
                            mean_redo_used_before_cut,
                            mean_redo_speed_per_sec,
                            current_redo_speed_per_sec,
                            redo_available);

  update_size -= m_update_size_lcp[0];
  insert_size -= m_insert_size_lcp[0];
  delete_size -= m_delete_size_lcp[0];
  Uint64 seconds_since_lcp_cut = 0;
  Uint64 lcp_time_in_secs = 0;
  Uint64 change_rate = calculate_checkpoint_rate(update_size,
                                                 insert_size,
                                                 delete_size,
                                                 get_total_memory(),
                                                 seconds_since_lcp_cut,
                                                 lcp_time_in_secs);
  m_proposed_disk_write_speed = change_rate;

  RedoStateRep::RedoAlertState save_redo_alert_state =
    m_local_redo_alert_state;
  change_alert_state_redo_percent(redo_percentage);
  change_alert_state_redo_usage(max_redo_used_before_cut,
                                mean_redo_used_before_cut,
                                redo_available);
  handle_global_alert_state(signal, save_redo_alert_state);
  set_redo_alert_factor(redo_percentage);
  set_lcp_timing_factors(seconds_since_lcp_cut,
                         lcp_time_in_secs);
  set_proposed_disk_write_speed(current_redo_speed_per_sec,
                                mean_redo_speed_per_sec,
                                seconds_since_lcp_cut);

#ifdef DEBUG_REDO_CONTROL
  Int64 current_lag = m_lcp_lag[0] + m_lcp_lag[1];
  DEB_REDO_CONTROL(("(%u)Proposed speed is %llu kB/sec"
                    ", current_redo_speed is %llu kB/sec and"
                    ", mean_redo_speed is %llu kB/sec"
                    ", %s is %llu kB",
                    instance(),
                    (m_proposed_disk_write_speed / Uint64(1024)),
                    (current_redo_speed_per_sec / Uint64(1024)),
                    (mean_redo_speed_per_sec / Uint64(1024)),
                    (current_lag >= 0) ? "lag" : "ahead",
                    (current_lag >= 0) ? current_lag : -current_lag
                     ));
  DEB_REDO_CONTROL(("(%u)state: %u, redo_size: %llu MByte, "
                    "redo_percent: %llu, last LCP time in ms: %llu",
                    instance(),
                    m_redo_alert_state,
                    redo_size,
                    redo_percentage,
                    m_last_lcp_exec_time_in_ms));
#endif
}

Uint64
Backup::calculate_proposed_disk_write_speed()
{
  if (m_enable_partial_lcp == 0 && false)
  {
    jam();
    return 0;
  }
  Uint64 proposed_speed = m_proposed_disk_write_speed;
  proposed_speed /= CURR_DISK_SPEED_CONVERSION_FACTOR_TO_SECONDS;
  return proposed_speed;
}

/**
 * Calculate the current max and min write speeds, based on the
 * current disk-write demands on this LDM thread
 */
void
Backup::calculate_current_speed_bounds(Uint64& max_speed,
                                       Uint64& max_backup_speed,
                                       Uint64& min_speed)
{
  jam();

  max_speed = c_defaults.m_disk_write_speed_max;
  max_backup_speed = c_defaults.m_disk_write_speed_max;
  min_speed = c_defaults.m_disk_write_speed_min;

  if (m_is_any_node_restarting && m_is_lcp_running)
  {
    jam();
    max_speed = c_defaults.m_disk_write_speed_max_other_node_restart;
  }

  /**
   * Thread balance
   *
   * As Backup is currently run on one LDM instance, we need to take
   * some steps to give it some extra DiskWriteSpeed allowance during
   * a Backup.  This becomes more acute with more LDM threads.
   * The correct way to handle this is to parallelise backup and
   * the backup log.
   *
   * Until then, we will skew the per-LDM disk write speed bounds
   * temporarily during a Backup so that LDM 1 has a large fixed
   * portion as well as its usual 1/n share for LCP.
   *
   * When the Backup completes, balance is restored.
   */

  const Uint32 num_ldm_threads = globalData.ndbMtLqhThreads;

  if (m_is_backup_running && 
      num_ldm_threads > 1)
  {
    jam();

    const Uint64 node_max_speed = 
      max_backup_speed *
      num_ldm_threads;
  
    /* Backup will get a percentage of the node total allowance */
    Uint64 node_backup_max_speed = 
      (node_max_speed * c_defaults.m_backup_disk_write_pct) /
      100;

    /* LCP gets the rest */
    Uint64 node_lcp_max_speed = 
      node_max_speed - node_backup_max_speed;
    
    /* LDM threads get a fair share of the LCP allowance */
    Uint64 ldm_thread_lcp_max_speed =
      node_lcp_max_speed / num_ldm_threads;
    
    /* Backup LDM must perform both node Backup + thread LCP */
    Uint64 backup_ldm_max_speed = 
      node_backup_max_speed + 
      ldm_thread_lcp_max_speed;
    
    /* Other LDMs just do thread LCP */
    Uint64 other_ldm_max_speed = 
      ldm_thread_lcp_max_speed;
    
    ndbrequire(backup_ldm_max_speed + 
               ((num_ldm_threads - 1) * 
                other_ldm_max_speed) <=
               node_max_speed);
    
    if (is_backup_worker())
    {
      jam();
      /**
       * Min is set to node backup speed, 
       * this should quickly increase the thread's
       * allowance.
       */
      max_backup_speed = backup_ldm_max_speed;
      min_speed = node_backup_max_speed;
    }
    else
    {
      jam();
      /**
       * Trim write bandwidth available
       * to other LDM threads
       */
      max_backup_speed = other_ldm_max_speed;
      min_speed = MIN(min_speed, max_speed);
    }
  }
  if (m_is_backup_running)
  {
    /**
     * Make sure that the total can be the sum while running both a backup
     * and an LCP at the same time. The minimum is the same for total and
     * for backup. The minimum is always based on the configured value.
     */
    max_speed = max_backup_speed;
    //max_speed += max_backup_speed;
  }
  ndbrequire(min_speed <= max_speed);
}

void
Backup::adjust_disk_write_speed_down(Uint64& curr_disk_write_speed,
                                     Uint64& loc_disk_write_speed_set_to_min,
                                     Uint64 min_speed,
                                     int adjust_speed)
{
  if ((Int64)curr_disk_write_speed < (Int64)adjust_speed)
  {
    loc_disk_write_speed_set_to_min++;
    curr_disk_write_speed = min_speed;
  }
  else
  {
    curr_disk_write_speed -= adjust_speed;
    if (curr_disk_write_speed < min_speed)
    {
      loc_disk_write_speed_set_to_min++;
      curr_disk_write_speed = min_speed;
    }
  }
}

void
Backup::adjust_disk_write_speed_up(Uint64& curr_disk_write_speed,
                                   Uint64 max_speed,
                                   int adjust_speed)
{
  curr_disk_write_speed += adjust_speed;
  if (curr_disk_write_speed > max_speed)
  {
    curr_disk_write_speed = max_speed;
  }
}

/**
 * Calculate new disk checkpoint write speed based on the new
 * multiplication factor, we decrease in steps of 10% per second
 */
void
Backup::calculate_disk_write_speed(Signal *signal)
{
  if (!m_our_node_started)
  {
    /* No adaptiveness while we're still starting. */
    jam();
    return;
  }
  Uint64 max_disk_write_speed;
  Uint64 max_backup_disk_write_speed;
  Uint64 min_disk_write_speed;
  jamEntry();
  calculate_current_speed_bounds(max_disk_write_speed,
                                 max_backup_disk_write_speed,
                                 min_disk_write_speed);

  /**
   * It is possible that the limits (max + min) have moved so that
   * the current speed is now outside them, if so we immediately
   * track to the relevant limit.
   * In these cases, the data collected for the last period regarding
   * redo log etc will not be relevant here.
   */
  bool ret_flag = false;
  if (m_curr_disk_write_speed < min_disk_write_speed)
  {
    jam();
    m_curr_disk_write_speed = min_disk_write_speed;
    ret_flag = true;
  }
  else if (m_curr_disk_write_speed > max_disk_write_speed)
  {
    jam();
    m_curr_disk_write_speed = max_disk_write_speed;
    ret_flag = true;
  }
  if (m_curr_backup_disk_write_speed > max_backup_disk_write_speed)
  {
    jam();
    m_curr_backup_disk_write_speed = max_backup_disk_write_speed;
  }
  if (ret_flag)
  {
    jam();
    return;
  }


  /**
   * Current speed is within bounds, now consider whether to adjust
   * based on feedback.
   * 
   * Calculate the max - min and divide by 12 to get the adjustment parameter
   * which is 8% of max - min. We will never adjust faster than this to avoid
   * too quick adaptiveness. For adjustments down we will adapt faster for IO
   * lags, for CPU speed we will adapt a bit slower dependent on how high
   * the CPU load is.
   */
  int diff_disk_write_speed =
    max_disk_write_speed - min_disk_write_speed;

  int adjust_speed_up = diff_disk_write_speed / 12;
  int adjust_speed_down_high = diff_disk_write_speed / 7;
  int adjust_speed_down_medium = diff_disk_write_speed / 10;
  int adjust_speed_down_low = diff_disk_write_speed / 14;
  
  jam();
  if (diff_disk_write_speed <= 0 ||
      adjust_speed_up == 0)
  {
    jam();
    /**
     * The min == max which gives no room to adapt the LCP speed.
     * or the difference is too small to adapt it.
     */
    return;
  }
  if (c_lqh->is_ldm_instance_io_lagging())
  {
    /**
     * With IO lagging behind we will decrease the LCP speed to accomodate
     * for more REDO logging bandwidth. The definition of REDO log IO lagging
     * is kept in DBLQH, but will be a number of seconds of outstanding REDO
     * IO requests that LQH is still waiting for completion of.
     * This is a harder condition, so here we will immediately slow down fast.
     */
    jam();
    slowdowns_due_to_io_lag++;
    adjust_disk_write_speed_down(m_curr_disk_write_speed,
                                 disk_write_speed_set_to_min,
                                 min_disk_write_speed,
                                 adjust_speed_down_high);
    adjust_disk_write_speed_down(m_curr_backup_disk_write_speed,
                                 backup_disk_write_speed_set_to_min,
                                 min_disk_write_speed,
                                 adjust_speed_down_high);
  }
  else
  {
    /**
     * Get CPU usage of this LDM thread during last second.
     * If CPU usage is over or equal to 95% we will decrease the LCP speed
     * If CPU usage is below 90% we will increase the LCP speed
     * one more step. Otherwise we will keep it where it currently is.
     *
     * The speed of writing backups and LCPs are fairly linear to the
     * amount of bytes written. So e.g. writing 10 MByte/second gives
     * roughly about 10% CPU usage in one CPU. So by writing less we have a
     * more or less linear decrease of CPU usage. Naturally the speed of
     * writing is very much coupled to the CPU speed. CPUs today have all
     * sorts of power save magic, but this algorithm doesn't kick in until
     * we're at very high CPU loads where we won't be in power save mode.
     * Obviously it also works in the opposite direction that we can easily
     * speed up things when the CPU is less used.
     * 
     * One complication of this algorithm is that we only measure the thread
     * CPU usage, so we don't really know here the level of CPU usage in total
     * of the system. Getting this information is quite complex and can
     * quickly change if the user is also using the machine for many other
     * things. In this case the algorithm will simply go up to the current
     * maximum value. So it will work much the same as before this algorithm
     * was put in place with the maximum value as the new DiskCheckpointSpeed
     * parameter.
     *
     * The algorithm will work best in cases where the user has locked the
     * thread to one or more CPUs and ensures that the thread can always run
     * by not allocating more than one thread per CPU.
     *
     * The reason we put the CPU usage limits fairly high is that the LDM
     * threads become more and more efficient as loads goes up. The reason
     * for this is that as more and more signals are executed in each loop
     * before checking for new signals. This means that as load goes up we
     * spend more and more time doing useful work. At low loads we spend a
     * significant time simply waiting for new signals to arrive and going to
     * sleep and waking up. So being at 95% load still means that we have
     * a bit more than 5% capacity left and even being at 90% means we
     * might have as much as 20% more capacity to use.
     */
    jam();
    EXECUTE_DIRECT_MT(THRMAN, GSN_GET_CPU_USAGE_REQ, signal,
                      1,
                      getThrmanInstance());
    Uint32 cpu_usage = signal->theData[0];
    if (cpu_usage < 90)
    {
      jamEntry();
      adjust_disk_write_speed_up(m_curr_disk_write_speed,
                                 max_disk_write_speed,
                                 adjust_speed_up);
      adjust_disk_write_speed_up(m_curr_backup_disk_write_speed,
                                 max_backup_disk_write_speed,
                                 adjust_speed_up);
    }
    else if (cpu_usage < 95)
    {
      jamEntry();
    }
    else if (cpu_usage < 97)
    {
      jamEntry();
      /* 95-96% load, slightly slow down */
      slowdowns_due_to_high_cpu++;
      adjust_disk_write_speed_up(m_curr_disk_write_speed,
                                 max_disk_write_speed,
                                 adjust_speed_down_low);
      adjust_disk_write_speed_up(m_curr_backup_disk_write_speed,
                                 max_backup_disk_write_speed,
                                 adjust_speed_down_low);
    }
    else if (cpu_usage < 99)
    {
      jamEntry();
      /* 97-98% load, slow down */
      slowdowns_due_to_high_cpu++;
      adjust_disk_write_speed_up(m_curr_disk_write_speed,
                                 max_disk_write_speed,
                                 adjust_speed_down_medium);
      adjust_disk_write_speed_up(m_curr_backup_disk_write_speed,
                                 max_backup_disk_write_speed,
                                 adjust_speed_down_medium);
    }
    else
    {
      jamEntry();
      /* 99-100% load, slow down a bit faster */
      slowdowns_due_to_high_cpu++;
      adjust_disk_write_speed_up(m_curr_disk_write_speed,
                                 max_disk_write_speed,
                                 adjust_speed_down_high);
      adjust_disk_write_speed_up(m_curr_backup_disk_write_speed,
                                 max_backup_disk_write_speed,
                                 adjust_speed_down_high);
    }
  }
}

void
Backup::send_next_reset_disk_speed_counter(Signal *signal)
{
  signal->theData[0] = BackupContinueB::RESET_DISK_SPEED_COUNTER;
  sendSignalWithDelay(reference(),
                      GSN_CONTINUEB,
                      signal,
                      m_reset_delay_used,
                      1);
  return;
}

void
Backup::execCHECK_NODE_RESTARTCONF(Signal *signal)
{
  bool old_is_backup_running = m_is_backup_running;
  bool old_is_any_node_restarting = m_is_any_node_restarting;
  if (!m_is_lcp_running)
  {
    if (signal->theData[0] == 1)
    {
      jam();
      lcp_start_point();
    }
  }
  else
  {
    if (signal->theData[0] == 0)
    {
      jam();
      lcp_end_point();
    }
  }
  m_is_lcp_running = (signal->theData[0] == 1);
  m_is_backup_running = g_is_backup_running;  /* Global from backup instance */
  m_is_any_node_restarting = (signal->theData[1] == 1);
  const char* backup_text=NULL;
  const char* restart_text=NULL;
  
  /* No logging of LCP start/stop w.r.t. Disk Speed */
  if (old_is_backup_running != m_is_backup_running)
  {
    if (old_is_backup_running)
    {
      backup_text=" Backup completed";
    }
    else
    {
      backup_text=" Backup started";
    }
  }
  if (old_is_any_node_restarting != m_is_any_node_restarting)
  {
    if (old_is_any_node_restarting)
    {
      restart_text=" Node restart finished";
    }
    else
    {
      restart_text=" Node restart ongoing";
    }
  }

  if (is_backup_worker())
  {
    /* Just have one LDM log the transition */
    if (backup_text || restart_text)
    {
      g_eventLogger->info("Adjusting disk write speed bounds due to :%s%s",
                          (backup_text ? backup_text : ""),
                          (restart_text ? restart_text : ""));
    }
  }
}

void
Backup::execCONTINUEB(Signal* signal)
{
  jamEntry();
  const Uint32 Tdata0 = signal->theData[0];
  const Uint32 Tdata1 = signal->theData[1];
  const Uint32 Tdata2 = signal->theData[2];
  const Uint32 Tdata3 = signal->theData[3];
  
  switch(Tdata0) {
  case BackupContinueB::RESET_DISK_SPEED_COUNTER:
  {
    jam();
    const NDB_TICKS curr_time = NdbTick_getCurrentTicks();
    const Uint64 millisPassed = 
      NdbTick_Elapsed(m_monitor_snapshot_start,curr_time).milliSec();
    if (millisPassed >= 800 && !m_node_restart_check_sent)
    {
      /**
       * Check for node restart ongoing, we will check for it and use
       * the cached copy of the node restart state when deciding on the
       * disk checkpoint speed. We will start this check a few intervals
       * before calculating the new disk checkpoint speed. We will send
       * such a check once per interval we are changing disk checkpoint
       * speed.
       *
       * So we call DIH asynchronously here after 800ms have passed such
       * that when 1000 ms have passed and we will check disk speeds we
       * have information about if there is a node restart ongoing or not.
       * This information will only affect disk write speed, so it's not
       * a problem to rely on up to 200ms old information.
       */
      jam();
      m_node_restart_check_sent = true;
      signal->theData[0] = reference();
      sendSignal(DBDIH_REF, GSN_CHECK_NODE_RESTARTREQ, signal, 1, JBB);
    }
    /**
     * We check for millis passed larger than 989 to handle the situation
     * when we wake up slightly too early. Since we only wake up once every
     * 100 millisecond, this should be better than occasionally get intervals
     * of 1100 milliseconds. All the calculations takes the real interval into
     * account, so it should not corrupt any data.
     */
    if (millisPassed > 989)
    {
      jam();
      m_node_restart_check_sent = false;
      monitor_disk_write_speed(curr_time, millisPassed);
      measure_change_speed(signal);
      calculate_disk_write_speed(signal);
    }
    handle_overflow(m_overflow_disk_write,
                    m_words_written_this_period,
                    m_curr_disk_write_speed);
    handle_overflow(m_backup_overflow_disk_write,
                    m_backup_words_written_this_period,
                    m_curr_backup_disk_write_speed);
    calculate_next_delay(curr_time);
    send_next_reset_disk_speed_counter(signal);
    break;
  }
  case BackupContinueB::BACKUP_FRAGMENT_INFO:
  {
    jam();
    const Uint32 ptr_I = Tdata1;
    Uint32 tabPtr_I = Tdata2;
    Uint32 fragPtr_I = signal->theData[3];

    BackupRecordPtr ptr;
    c_backupPool.getPtr(ptr, ptr_I);
    TablePtr tabPtr;
    ptr.p->tables.getPtr(tabPtr, tabPtr_I);

    if (fragPtr_I != tabPtr.p->fragments.getSize())
    {
      jam();
      FragmentPtr fragPtr;
      tabPtr.p->fragments.getPtr(fragPtr, fragPtr_I);
      
      BackupFilePtr filePtr;
      ptr.p->files.getPtr(filePtr, ptr.p->ctlFilePtr);
      
      const Uint32 sz = sizeof(BackupFormat::CtlFile::FragmentInfo) >> 2;
      Uint32 * dst;
      if (!filePtr.p->operation.dataBuffer.getWritePtr(&dst, sz))
      {
	sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 
                            WaitDiskBufferCapacityMillis, 4);
	return;
      }
      
      BackupFormat::CtlFile::FragmentInfo * fragInfo = 
	(BackupFormat::CtlFile::FragmentInfo*)dst;
      fragInfo->SectionType = htonl(BackupFormat::FRAGMENT_INFO);
      fragInfo->SectionLength = htonl(sz);
      fragInfo->TableId = htonl(fragPtr.p->tableId);
      fragInfo->FragmentNo = htonl(fragPtr_I);
      fragInfo->NoOfRecordsLow = htonl((Uint32)(fragPtr.p->noOfRecords & 0xFFFFFFFF));
      fragInfo->NoOfRecordsHigh = htonl((Uint32)(fragPtr.p->noOfRecords >> 32));
      fragInfo->FilePosLow = htonl(0);
      fragInfo->FilePosHigh = htonl(0);
      
      filePtr.p->operation.dataBuffer.updateWritePtr(sz);
      
      fragPtr_I++;
    }
    
    if (fragPtr_I == tabPtr.p->fragments.getSize())
    {
      BackupLockTab *req = (BackupLockTab *)signal->getDataPtrSend();
      req->m_senderRef = reference();
      req->m_tableId = tabPtr.p->tableId;
      req->m_lock_unlock = BackupLockTab::UNLOCK_TABLE;
      req->m_backup_state = BackupLockTab::BACKUP_FRAGMENT_INFO;
      req->m_backupRecordPtr_I = ptr_I;
      req->m_tablePtr_I = tabPtr_I;
      sendSignal(DBDICT_REF, GSN_BACKUP_LOCK_TAB_REQ, signal,
                 BackupLockTab::SignalLength, JBB);
      return;
    }
    
    signal->theData[0] = BackupContinueB::BACKUP_FRAGMENT_INFO;
    signal->theData[1] = ptr_I;
    signal->theData[2] = tabPtr_I;
    signal->theData[3] = fragPtr_I;
    sendSignal(reference(), GSN_CONTINUEB, signal, 4, JBB);
    return;
  }
  case BackupContinueB::START_FILE_THREAD:
  case BackupContinueB::BUFFER_UNDERFLOW:
  {
    jam();
    BackupFilePtr filePtr;
    c_backupFilePool.getPtr(filePtr, Tdata1);
    checkFile(signal, filePtr);
    return;
  }
  case BackupContinueB::BUFFER_FULL_SCAN:
  {
    jam();
    BackupFilePtr filePtr;
    BackupRecordPtr ptr;
    c_backupFilePool.getPtr(filePtr, Tdata1);
    c_backupPool.getPtr(ptr, filePtr.p->backupPtr);
    /**
     * Given that we've been waiting a few milliseconds for buffers to become
     * free, we need to initialise the priority mode algorithm to ensure that
     * we select the correct priority mode.
     *
     * We get the number of jobs waiting at B-level to assess the current
     * activity level to get a new starting point of the algorithm.
     * Any load level below 16 signals in the buffer we ignore, if we have
     * a higher level we provide a value that will ensure that we most likely
     * will start at A-level.
     */
    init_scan_prio_level(signal, ptr);
    checkScan(signal, ptr, filePtr);
    return;
  }
  break;
  case BackupContinueB::BUFFER_FULL_FRAG_COMPLETE:
  {
    jam();
    BackupFilePtr filePtr;
    c_backupFilePool.getPtr(filePtr, Tdata1);
    fragmentCompleted(signal, filePtr, Tdata2);
    return;
  }
  break;
  case BackupContinueB::BUFFER_FULL_META:
  {
    jam();
    BackupRecordPtr ptr;
    c_backupPool.getPtr(ptr, Tdata1);
    
    BackupFilePtr filePtr;

    if (ptr.p->is_lcp())
    {
      jam();
      ptr.p->files.getPtr(filePtr, Tdata3);
    }
    else
    {
      jam();
      ptr.p->files.getPtr(filePtr, ptr.p->ctlFilePtr);
    }
    FsBuffer & buf = filePtr.p->operation.dataBuffer;
    
    if(buf.getFreeSize() < buf.getMaxWrite()) {
      jam();
      TablePtr tabPtr;
      c_tablePool.getPtr(tabPtr, Tdata2);
      
      DEBUG_OUT("Backup - Buffer full - " 
                << buf.getFreeSize()
		<< " < " << buf.getMaxWrite()
                << " (sz: " << buf.getUsableSize()
                << " getMinRead: " << buf.getMinRead()
		<< ") - tableId = " << tabPtr.p->tableId);
      
      signal->theData[0] = BackupContinueB::BUFFER_FULL_META;
      signal->theData[1] = Tdata1;
      signal->theData[2] = Tdata2;
      signal->theData[3] = Tdata3;
      sendSignalWithDelay(reference(), GSN_CONTINUEB, signal,
                          WaitDiskBufferCapacityMillis, 4);
      return;
    }//if
    
    TablePtr tabPtr;
    c_tablePool.getPtr(tabPtr, Tdata2);
    GetTabInfoReq * req = (GetTabInfoReq *)signal->getDataPtrSend();
    req->senderRef = reference();
    req->senderData = filePtr.i;
    req->requestType = GetTabInfoReq::RequestById |
      GetTabInfoReq::LongSignalConf;
    req->tableId = tabPtr.p->tableId;
    req->schemaTransId = 0;
    sendSignal(DBDICT_REF, GSN_GET_TABINFOREQ, signal, 
	       GetTabInfoReq::SignalLength, JBB);
    return;
  }
  case BackupContinueB::ZDELAY_SCAN_NEXT:
  {
    if (ERROR_INSERTED(10039))
    {
      jam();
      sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 300, 
			  signal->getLength());
      return;
    }
    else
    {
      jam();
      CLEAR_ERROR_INSERT_VALUE;
      ndbout_c("Resuming backup");

      Uint32 filePtr_I = Tdata1;
      BackupFilePtr filePtr;
      c_backupFilePool.getPtr(filePtr, filePtr_I);
      BackupRecordPtr ptr;
      c_backupPool.getPtr(ptr, filePtr.p->backupPtr);
      TablePtr tabPtr;
      ndbrequire(findTable(ptr, tabPtr, filePtr.p->tableId));
      FragmentPtr fragPtr;
      tabPtr.p->fragments.getPtr(fragPtr, filePtr.p->fragmentNo);

      BlockReference lqhRef = 0;
      if (ptr.p->is_lcp()) {
        lqhRef = calcInstanceBlockRef(DBLQH);
      } else {
        const Uint32 instanceKey = fragPtr.p->lqhInstanceKey;
        ndbrequire(instanceKey != 0);
        lqhRef = numberToRef(DBLQH, instanceKey, getOwnNodeId());
      }

      memmove(signal->theData, signal->theData + 2, 
	      4*ScanFragNextReq::SignalLength);

      sendSignal(lqhRef, GSN_SCAN_NEXTREQ, signal, 
		 ScanFragNextReq::SignalLength, JBB);
      return ;
    }
  }
  case BackupContinueB::ZGET_NEXT_FRAGMENT:
  {
    BackupRecordPtr backupPtr;
    TablePtr tabPtr;
    Uint32 fragNo = signal->theData[3];
    c_backupPool.getPtr(backupPtr, signal->theData[1]);
    ndbrequire(findTable(backupPtr, tabPtr, signal->theData[2]));
    getFragmentInfo(signal, backupPtr, tabPtr, fragNo);
    return;
  }
  case BackupContinueB::ZDELETE_LCP_FILE:
  {
    jam();
    delete_lcp_file_processing(signal, signal->theData[1]);
    return;
  }
  default:
    ndbrequire(0);
  }//switch
}

void
Backup::execBACKUP_LOCK_TAB_CONF(Signal *signal)
{
  jamEntry();
  const BackupLockTab *conf = (const BackupLockTab *)signal->getDataPtr();
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, conf->m_backupRecordPtr_I);
  TablePtr tabPtr;
  ptr.p->tables.getPtr(tabPtr, conf->m_tablePtr_I);

  switch(conf->m_backup_state) {
  case BackupLockTab::BACKUP_FRAGMENT_INFO:
  {
    jam();
    ptr.p->tables.next(tabPtr);
    if (tabPtr.i == RNIL)
    {
      jam();
      closeFiles(signal, ptr);
      return;
    }

    signal->theData[0] = BackupContinueB::BACKUP_FRAGMENT_INFO;
    signal->theData[1] = ptr.i;
    signal->theData[2] = tabPtr.i;
    signal->theData[3] = 0;       // Start from first fragment of next table
    sendSignal(reference(), GSN_CONTINUEB, signal, 4, JBB);
    return;
  }
  case BackupLockTab::GET_TABINFO_CONF:
  {
    jam();
    if (conf->errorCode)
    {
      jam();
      defineBackupRef(signal, ptr, conf->errorCode);
      return;
    }

    ptr.p->tables.next(tabPtr);
    afterGetTabinfoLockTab(signal, ptr, tabPtr);
    return;
  }
  case BackupLockTab::CLEANUP:
  {
    jam();
    ptr.p->tables.next(tabPtr);
    cleanupNextTable(signal, ptr, tabPtr);
    return;
  }
  default:
    ndbrequire(false);
  }
}

void
Backup::execBACKUP_LOCK_TAB_REF(Signal *signal)
{
  jamEntry();
  ndbrequire(false /* Not currently possible. */);
}

Uint64 Backup::get_new_speed_val64(Signal *signal)
{
  if (signal->length() == 3)
  {
    jam();
    Uint64 val = Uint64(signal->theData[1]);
    val <<= 32;
    val += Uint64(signal->theData[2]);
    return val;
  }
  else
  {
    jam();
    return 0;
  }
}

Uint64 Backup::get_new_speed_val32(Signal *signal)
{
  if (signal->length() == 2)
  {
    jam();
    return Uint64(signal->theData[1]);
  }
  else
  {
    jam();
    return 0;
  }
}

void
Backup::execDUMP_STATE_ORD(Signal* signal)
{
  jamEntry();
  
  /* Dump commands used in public interfaces */
  switch (signal->theData[0]) {
  case DumpStateOrd::BackupStatus:
  {
    /* See code in BackupProxy.cpp as well */
    BlockReference result_ref = CMVMI_REF;
    if (signal->length() == 2)
      result_ref = signal->theData[1];

    BackupRecordPtr ptr;
    int reported = 0;
    for(c_backups.first(ptr); ptr.i != RNIL; c_backups.next(ptr))
    {
      if (!ptr.p->is_lcp())
      {
        reportStatus(signal, ptr, result_ref);
        reported++;
      }
    }
    if (!reported)
      reportStatus(signal, ptr, result_ref);
    return;
  }
  case DumpStateOrd::BackupMinWriteSpeed32:
  {
    jam();
    Uint64 new_val = get_new_speed_val32(signal);
    if (new_val < Uint64(1024*1024))
    {
      jam();
      g_eventLogger->info("Use: DUMP 100001 MinDiskWriteSpeed");
      return;
    }
    restore_disk_write_speed_numbers();
    c_defaults.m_disk_write_speed_min = new_val;
    calculate_real_disk_write_speed_parameters();
    return;
  }
  case DumpStateOrd::BackupMaxWriteSpeed32:
  {
    jam();
    Uint64 new_val = get_new_speed_val32(signal);
    if (new_val < Uint64(1024*1024))
    {
      jam();
      g_eventLogger->info("Use: DUMP 100002 MaxDiskWriteSpeed");
      return;
    }
    restore_disk_write_speed_numbers();
    c_defaults.m_disk_write_speed_max = new_val;
    calculate_real_disk_write_speed_parameters();
    return;
  }
  case DumpStateOrd::BackupMaxWriteSpeedOtherNodeRestart32:
  {
    jam();
    Uint64 new_val = get_new_speed_val32(signal);
    if (new_val < Uint64(1024*1024))
    {
      jam();
      g_eventLogger->info("Use: DUMP 100003 MaxDiskWriteSpeedOtherNodeRestart");
      return;
    }
    restore_disk_write_speed_numbers();
    c_defaults.m_disk_write_speed_max_other_node_restart = new_val;
    calculate_real_disk_write_speed_parameters();
    return;
  }
  case DumpStateOrd::BackupMinWriteSpeed64:
  {
    jam();
    Uint64 new_val = get_new_speed_val64(signal);
    if (new_val < Uint64(1024*1024))
    {
      jam();
      g_eventLogger->info("Use: DUMP 100004 MinDiskWriteSpeed(MSB) "
                          "MinDiskWriteSpeed(LSB)");
      return;
    }
    restore_disk_write_speed_numbers();
    c_defaults.m_disk_write_speed_min = new_val;
    calculate_real_disk_write_speed_parameters();
    return;
  }
  case DumpStateOrd::BackupMaxWriteSpeed64:
  {
    jam();
    Uint64 new_val = get_new_speed_val64(signal);
    if (new_val < Uint64(1024*1024))
    {
      jam();
      g_eventLogger->info("Use: DUMP 100005 MaxDiskWriteSpeed(MSB) "
                          "MaxDiskWriteSpeed(LSB)");
      return;
    }
    restore_disk_write_speed_numbers();
    c_defaults.m_disk_write_speed_max = new_val;
    calculate_real_disk_write_speed_parameters();
    return;
  }
  case DumpStateOrd::BackupMaxWriteSpeedOtherNodeRestart64:
  {
    jam();
    Uint64 new_val = get_new_speed_val64(signal);
    if (new_val < Uint64(1024*1024))
    {
      jam();
      g_eventLogger->info("Use: DUMP 100006"
                          " MaxDiskWriteSpeedOtherNodeRestart(MSB)"
                          " MaxDiskWriteSpeedOtherNodeRestart(LSB)");
      return;
    }
    restore_disk_write_speed_numbers();
    c_defaults.m_disk_write_speed_max_other_node_restart = new_val;
    calculate_real_disk_write_speed_parameters();
    return;
  }
  default:
    /* continue to debug section */
    break;
  }

  /* Debugging or unclassified section */

  if(signal->theData[0] == 20){
    if(signal->length() > 1){
      c_defaults.m_dataBufferSize = (signal->theData[1] * 1024 * 1024);
    }
    if(signal->length() > 2){
      c_defaults.m_logBufferSize = (signal->theData[2] * 1024 * 1024);
    }
    if(signal->length() > 3){
      c_defaults.m_minWriteSize = signal->theData[3] * 1024;
    }
    if(signal->length() > 4){
      c_defaults.m_maxWriteSize = signal->theData[4] * 1024;
    }
    
    infoEvent("Backup: data: %d log: %d min: %d max: %d",
	      c_defaults.m_dataBufferSize,
	      c_defaults.m_logBufferSize,
	      c_defaults.m_minWriteSize,
	      c_defaults.m_maxWriteSize);
    return;
  }
  if(signal->theData[0] == 21){
    BackupReq * req = (BackupReq*)signal->getDataPtrSend();
    req->senderData = 23;
    req->backupDataLen = 0;
    sendSignal(reference(), GSN_BACKUP_REQ,signal,BackupReq::SignalLength, JBB);
    startTime = NdbTick_getCurrentTicks();
    return;
  }

  if(signal->theData[0] == 22){
    const Uint32 seq = signal->theData[1];
    FsRemoveReq * req = (FsRemoveReq *)signal->getDataPtrSend();
    req->userReference = reference();
    req->userPointer = 23;
    req->directory = 1;
    req->ownDirectory = 1;
    FsOpenReq::setVersion(req->fileNumber, 2);
    FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_CTL);
    FsOpenReq::v2_setSequence(req->fileNumber, seq);
    FsOpenReq::v2_setNodeId(req->fileNumber, getOwnNodeId());
    sendSignal(NDBFS_REF, GSN_FSREMOVEREQ, signal, 
	       FsRemoveReq::SignalLength, JBA);
    return;
  }

  if(signal->theData[0] == 23){
    /**
     * Print records
     */
    BackupRecordPtr ptr;
    for(c_backups.first(ptr); ptr.i != RNIL; c_backups.next(ptr)){
      infoEvent("BackupRecord %d: BackupId: %u MasterRef: %x ClientRef: %x",
		ptr.i, ptr.p->backupId, ptr.p->masterRef, ptr.p->clientRef);
      infoEvent(" State: %d", ptr.p->slaveState.getState());
      BackupFilePtr filePtr;
      for(ptr.p->files.first(filePtr); filePtr.i != RNIL; 
	  ptr.p->files.next(filePtr)){
	jam();
	infoEvent(" file %d: type: %d flags: H'%x",
		  filePtr.i, filePtr.p->fileType, 
		  filePtr.p->m_flags);
      }
    }

    const NDB_TICKS now = NdbTick_getCurrentTicks();
    const Uint64 resetElapsed = NdbTick_Elapsed(m_reset_disk_speed_time,now).milliSec();
    const Uint64 millisPassed = NdbTick_Elapsed(m_monitor_snapshot_start,now).milliSec();
    /* Dump measured disk write speed since last RESET_DISK_SPEED */
    ndbout_c("m_curr_disk_write_speed: %ukb  m_words_written_this_period:"
             " %u kwords  m_overflow_disk_write: %u kb",
              Uint32(4 * m_curr_disk_write_speed / 1024),
              Uint32(m_words_written_this_period / 1024),
              Uint32(m_overflow_disk_write / 1024));
    ndbout_c("m_backup_curr_disk_write_speed: %ukb  "
             "m_backup_words_written_this_period:"
             " %u kwords  m_backup_overflow_disk_write: %u kb",
              Uint32(4 * m_curr_backup_disk_write_speed / 1024),
              Uint32(m_backup_words_written_this_period / 1024),
              Uint32(m_backup_overflow_disk_write / 1024));
    ndbout_c("m_reset_delay_used: %u  time since last RESET_DISK_SPEED: %llu millis",
             m_reset_delay_used, resetElapsed);
    /* Dump measured rate since last snapshot start */
    Uint64 byteRate = (4000 * m_monitor_words_written) / (millisPassed + 1);
    ndbout_c("m_monitor_words_written : %llu, duration : %llu millis, rate :"
             " %llu bytes/s : (%u pct of config)",
             m_monitor_words_written, millisPassed, 
             byteRate,
             (Uint32) ((100 * byteRate / (4 * 10)) /
                       (m_curr_disk_write_speed + 1)));
    byteRate = (4000 * m_backup_monitor_words_written) / (millisPassed + 1);
    ndbout_c("m_backup_monitor_words_written : %llu, duration : %llu"
             " millis, rate :"
             " %llu bytes/s : (%u pct of config)",
             m_backup_monitor_words_written, millisPassed, 
             byteRate,
             (Uint32) ((100 * byteRate / (4 * 10)) /
                       (m_curr_backup_disk_write_speed + 1)));

    for(c_backups.first(ptr); ptr.i != RNIL; c_backups.next(ptr))
    {
      ndbout_c("BackupRecord %u:  BackupId: %u  MasterRef: %x  ClientRef: %x",
               ptr.i, ptr.p->backupId, ptr.p->masterRef, ptr.p->clientRef);
      ndbout_c(" State: %u", ptr.p->slaveState.getState());
      ndbout_c(" noOfByte: %llu  noOfRecords: %llu",
               ptr.p->noOfBytes, ptr.p->noOfRecords);
      ndbout_c(" noOfLogBytes: %llu  noOfLogRecords: %llu",
               ptr.p->noOfLogBytes, ptr.p->noOfLogRecords);
      ndbout_c(" errorCode: %u", ptr.p->errorCode);
      BackupFilePtr filePtr;
      for(ptr.p->files.first(filePtr); filePtr.i != RNIL; 
	  ptr.p->files.next(filePtr))
      {
	ndbout_c(" file %u:  type: %u  flags: H'%x  tableId: %u  fragmentId: %u",
                 filePtr.i, filePtr.p->fileType, filePtr.p->m_flags,
                 filePtr.p->tableId, filePtr.p->fragmentNo);
      }
      if (ptr.p->slaveState.getState() == SCANNING && ptr.p->dataFilePtr[0] != RNIL)
      {
        c_backupFilePool.getPtr(filePtr, ptr.p->dataFilePtr[0]);
        OperationRecord & op = filePtr.p->operation;
        Uint32 *tmp = NULL;
        Uint32 sz = 0;
        bool eof = FALSE;
        bool ready = op.dataBuffer.getReadPtr(&tmp, &sz, &eof);
        ndbout_c("ready: %s  eof: %s", ready ? "TRUE" : "FALSE", eof ? "TRUE" : "FALSE");
      }
    }
    return;
  }
  if(signal->theData[0] == 24){
    /**
     * Print size of records etc.
     */
    infoEvent("Backup - dump pool sizes");
    infoEvent("BackupPool: %d BackupFilePool: %d TablePool: %d",
	      c_backupPool.getSize(), c_backupFilePool.getSize(), 
	      c_tablePool.getSize());
    infoEvent("AttrPool: %d TriggerPool: %d FragmentPool: %d",
	      c_backupPool.getSize(), c_backupFilePool.getSize(), 
	      c_tablePool.getSize());
    infoEvent("PagePool: %d",
	      c_pagePool.getSize());


    if(signal->getLength() == 2 && signal->theData[1] == 2424)
    {
      /**
       * Handle LCP
       */
      BackupRecordPtr lcp;
      ndbrequire(c_backups.first(lcp));
      
      ndbrequire(c_backupPool.getSize() == c_backupPool.getNoOfFree() + 1);
      ndbrequire(c_tablePool.getSize() == c_tablePool.getNoOfFree() + 2);
      ndbrequire(c_fragmentPool.getSize() == c_fragmentPool.getNoOfFree() + 2);
      ndbrequire(c_triggerPool.getSize() == c_triggerPool.getNoOfFree());

      ndbrequire(c_backupFilePool.getSize() == (c_backupFilePool.getNoOfFree() + 
                           (4 + 2 * BackupFormat::NDB_MAX_FILES_PER_LCP)));

      Uint32 file_pages = 0;
      BackupFilePtr lcp_file;

      c_backupFilePool.getPtr(lcp_file, lcp.p->prepareCtlFilePtr[0]);
      file_pages += lcp_file.p->pages.getSize();

      c_backupFilePool.getPtr(lcp_file, lcp.p->prepareCtlFilePtr[1]);
      file_pages += lcp_file.p->pages.getSize();

      for (Uint32 i = 0; i < BackupFormat::NDB_MAX_FILES_PER_LCP; i++)
      {
        c_backupFilePool.getPtr(lcp_file, lcp.p->dataFilePtr[i]);
        file_pages += lcp_file.p->pages.getSize();

        c_backupFilePool.getPtr(lcp_file, lcp.p->prepareDataFilePtr[i]);
        file_pages += lcp_file.p->pages.getSize();
      }

      c_backupFilePool.getPtr(lcp_file, lcp.p->ctlFilePtr);
      file_pages += lcp_file.p->pages.getSize();

      c_backupFilePool.getPtr(lcp_file, lcp.p->deleteFilePtr);
      file_pages += lcp_file.p->pages.getSize();

      ndbrequire(c_pagePool.getSize() == 
		 c_pagePool.getNoOfFree() + 
                 file_pages);
    }
  }

  if(signal->theData[0] == DumpStateOrd::DumpBackup)
  {
    /* Display a bunch of stuff about Backup defaults */
    infoEvent("Compressed Backup: %d", c_defaults.m_compressed_backup);
    infoEvent("Compressed LCP: %d", c_defaults.m_compressed_lcp);
  }

  if(signal->theData[0] == DumpStateOrd::DumpBackupSetCompressed)
  {
    c_defaults.m_compressed_backup= signal->theData[1];
    infoEvent("Compressed Backup: %d", c_defaults.m_compressed_backup);
  }

  if(signal->theData[0] == DumpStateOrd::DumpBackupSetCompressedLCP)
  {
    c_defaults.m_compressed_lcp= signal->theData[1];
    infoEvent("Compressed LCP: %d", c_defaults.m_compressed_lcp);
  }

  if (signal->theData[0] == DumpStateOrd::BackupErrorInsert)
  {
    if (signal->getLength() == 1)
      ndbout_c("BACKUP: setting error %u", signal->theData[1]);
    else
      ndbout_c("BACKUP: setting error %u, %u",
               signal->theData[1], signal->theData[2]);
    SET_ERROR_INSERT_VALUE2(signal->theData[1], signal->theData[2]);
  }
}

/**
 * We are using a round buffer of measurements, to simplify the code we
 * use this routing to quickly derive the disk write record from an index
 * (how many seconds back we want to check).
 */
Uint32
Backup::get_disk_write_speed_record(Uint32 start_index)
{
  ndbassert(start_index < DISK_WRITE_SPEED_REPORT_SIZE);
  if (next_disk_write_speed_report == last_disk_write_speed_report)
  {
    /* No speed reports generated yet */
    return DISK_WRITE_SPEED_REPORT_SIZE;
  }
  if (start_index < next_disk_write_speed_report)
  {
    return (next_disk_write_speed_report - (start_index + 1));
  }
  else if (last_disk_write_speed_report == 0)
  {
    /**
     * We might still be in inital phase when not all records have
     * been written yet.
     */
    return DISK_WRITE_SPEED_REPORT_SIZE;
  }
  else
  {
    return (DISK_WRITE_SPEED_REPORT_SIZE -
            ((start_index + 1) - next_disk_write_speed_report));
  }
  ndbassert(false);
  return 0;
}

/**
 * Calculates the average speed for a number of seconds back.
 * reports the numbers in number of milliseconds that actually
 * passed and the number of bytes written in this period.
 */
void
Backup::calculate_disk_write_speed_seconds_back(Uint32 seconds_back,
                                         Uint64 & millis_passed,
                                         Uint64 & backup_lcp_bytes_written,
                                         Uint64 & backup_bytes_written,
                                         Uint64 & redo_bytes_written,
                                         bool at_least_one)
{
  Uint64 millis_back = (MILLIS_IN_A_SECOND * seconds_back) -
    MILLIS_ADJUST_FOR_EARLY_REPORT;
  Uint32 start_index = 0;

  ndbassert(seconds_back > 0);

  millis_passed = 0;
  backup_lcp_bytes_written = 0;
  backup_bytes_written = 0;
  redo_bytes_written = 0;
  jam();
  while (at_least_one ||
         (millis_passed < millis_back &&
          start_index < DISK_WRITE_SPEED_REPORT_SIZE))
  {
    jam();
    at_least_one = false;
    Uint32 disk_write_speed_record = get_disk_write_speed_record(start_index);
    if (disk_write_speed_record == DISK_WRITE_SPEED_REPORT_SIZE)
      break;
    millis_passed +=
      disk_write_speed_rep[disk_write_speed_record].millis_passed;
    backup_lcp_bytes_written +=
      disk_write_speed_rep[disk_write_speed_record].backup_lcp_bytes_written;
    backup_bytes_written +=
      disk_write_speed_rep[disk_write_speed_record].backup_bytes_written;
    redo_bytes_written +=
      disk_write_speed_rep[disk_write_speed_record].redo_bytes_written;
    start_index++;
  }
  /**
   * Always report at least one millisecond to avoid risk of division
   * by zero later on in the code.
   */
  jam();
  if (millis_passed == 0)
  {
    jam();
    millis_passed = 1;
  }
  return;
}

void
Backup::calculate_std_disk_write_speed_seconds_back(Uint32 seconds_back,
                             Uint64 millis_passed_total,
                             Uint64 backup_lcp_bytes_written,
                             Uint64 backup_bytes_written,
                             Uint64 redo_bytes_written,
                             Uint64 & std_dev_backup_lcp_in_bytes_per_sec,
                             Uint64 & std_dev_backup_in_bytes_per_sec,
                             Uint64 & std_dev_redo_in_bytes_per_sec)
{
  Uint32 start_index = 0;
  Uint64 millis_passed = 0;
  Uint64 millis_back = (MILLIS_IN_A_SECOND * seconds_back) -
    MILLIS_ADJUST_FOR_EARLY_REPORT;
  Uint64 millis_passed_this_period;

  Uint64 avg_backup_lcp_bytes_per_milli;
  Uint64 backup_lcp_bytes_written_this_period;
  Uint64 avg_backup_lcp_bytes_per_milli_this_period;
  long double backup_lcp_temp_sum;
  long double backup_lcp_square_sum;

  Uint64 avg_backup_bytes_per_milli;
  Uint64 backup_bytes_written_this_period;
  Uint64 avg_backup_bytes_per_milli_this_period;
  long double backup_temp_sum;
  long double backup_square_sum;

  Uint64 avg_redo_bytes_per_milli;
  Uint64 redo_bytes_written_this_period;
  Uint64 avg_redo_bytes_per_milli_this_period;
  long double redo_temp_sum;
  long double redo_square_sum;

  ndbassert(seconds_back > 0);
  if (millis_passed_total == 0)
  {
    jam();
    std_dev_backup_lcp_in_bytes_per_sec = 0;
    std_dev_backup_in_bytes_per_sec = 0;
    std_dev_redo_in_bytes_per_sec = 0;
    return;
  }
  avg_backup_lcp_bytes_per_milli = backup_lcp_bytes_written /
                                   millis_passed_total;
  avg_backup_bytes_per_milli = backup_bytes_written /
                               millis_passed_total;
  avg_redo_bytes_per_milli = redo_bytes_written / millis_passed_total;
  backup_lcp_square_sum = 0;
  backup_square_sum = 0;
  redo_square_sum = 0;
  jam();
  while (millis_passed < millis_back &&
         start_index < DISK_WRITE_SPEED_REPORT_SIZE)
  {
    jam();
    Uint32 disk_write_speed_record = get_disk_write_speed_record(start_index);
    if (disk_write_speed_record == DISK_WRITE_SPEED_REPORT_SIZE)
      break;
    millis_passed_this_period =
      disk_write_speed_rep[disk_write_speed_record].millis_passed;
    backup_lcp_bytes_written_this_period =
      disk_write_speed_rep[disk_write_speed_record].backup_lcp_bytes_written;
    backup_bytes_written_this_period =
      disk_write_speed_rep[disk_write_speed_record].backup_bytes_written;
    redo_bytes_written_this_period =
      disk_write_speed_rep[disk_write_speed_record].redo_bytes_written;
    millis_passed += millis_passed_this_period;

    if (millis_passed_this_period != 0)
    {
      /**
       * We use here a calculation of standard deviation that firsts
       * calculates the variance. The variance is calculated as the square
       * mean of the difference. To get standard intervals we compute the
       * average per millisecond and then sum over all milliseconds. To
       * simplify the calculation we then multiply the square of the diffs
       * per milli to the number of millis passed in a particular measurement.
       * We divide by the total number of millis passed. We do this first to
       * avoid too big numbers. We use long double in all calculations to
       * ensure that we don't overflow.
       *
       * We also try to avoid divisions by zero in the code in multiple
       * places when we query this table before the first measurement have
       * been logged.
       *
       * Calculating standard deviation as:
       * Sum of X(i) - E(X) squared where X(i) is the average per millisecond
       * in this time period and E(X) is the average over the entire period.
       * We divide by number of periods, but to get it more real, we divide
       * by total_millis / millis_in_this_period since the periods aren't
       * exactly the same. Finally we take square root of the sum of those
       * (X(i) - E(X))^2 / #periods. Actually the standard deviation should
       * be calculated using #periods - 1 as divisor. Finally we also need
       * to convert it from standard deviation per millisecond to standard
       * deviation per second. We make that simple by multiplying the
       * result from this function by 1000.
       */
      jam();
      avg_backup_lcp_bytes_per_milli_this_period =
        backup_lcp_bytes_written_this_period / millis_passed_this_period;
      backup_lcp_temp_sum = (long double)avg_backup_lcp_bytes_per_milli;
      backup_lcp_temp_sum -=
        (long double)avg_backup_lcp_bytes_per_milli_this_period;
      backup_lcp_temp_sum *= backup_lcp_temp_sum;
      backup_lcp_temp_sum /= (long double)millis_passed_total;
      backup_lcp_temp_sum *= (long double)millis_passed_this_period;
      backup_lcp_square_sum += backup_lcp_temp_sum;

      avg_backup_bytes_per_milli_this_period =
        backup_bytes_written_this_period / millis_passed_this_period;
      backup_temp_sum = (long double)avg_backup_bytes_per_milli;
      backup_temp_sum -=
        (long double)avg_backup_bytes_per_milli_this_period;
      backup_temp_sum *= backup_temp_sum;
      backup_temp_sum /= (long double)millis_passed_total;
      backup_temp_sum *= (long double)millis_passed_this_period;
      backup_square_sum += backup_temp_sum;

      avg_redo_bytes_per_milli_this_period =
        redo_bytes_written_this_period / millis_passed_this_period;
      redo_temp_sum = (long double)avg_redo_bytes_per_milli;
      redo_temp_sum -= (long double)avg_redo_bytes_per_milli_this_period;
      redo_temp_sum *= redo_temp_sum;
      redo_temp_sum /= (long double)millis_passed_total;
      redo_temp_sum *= (long double)millis_passed_this_period;
      redo_square_sum += redo_temp_sum;
    }
    start_index++;
  }
  if (millis_passed == 0)
  {
    jam();
    std_dev_backup_lcp_in_bytes_per_sec = 0;
    std_dev_backup_in_bytes_per_sec = 0;
    std_dev_redo_in_bytes_per_sec = 0;
    return;
  }
  /**
   * Calculate standard deviation per millisecond
   * We use long double for the calculation, but we want to report it to
   * it in bytes per second, so this is easiest to do with an unsigned
   * integer number. Conversion from long double to Uint64 is a real
   * conversion that we leave to the compiler to generate code to make.
   */
  std_dev_backup_lcp_in_bytes_per_sec = (Uint64)sqrtl(backup_lcp_square_sum);
  std_dev_backup_in_bytes_per_sec = (Uint64)sqrtl(backup_square_sum);
  std_dev_redo_in_bytes_per_sec = (Uint64)sqrtl(redo_square_sum);

  /**
   * Convert to standard deviation per second
   * We calculated it in bytes per millisecond, so simple multiplication of
   * 1000 is sufficient here.
   */
  std_dev_backup_lcp_in_bytes_per_sec*= (Uint64)1000;
  std_dev_backup_in_bytes_per_sec*= (Uint64)1000;
  std_dev_redo_in_bytes_per_sec*= (Uint64)1000;
}

Uint64
Backup::calculate_millis_since_finished(Uint32 start_index)
{
  Uint64 millis_passed = 0;
  jam();
  if (start_index == 0)
  {
    jam();
    return 0;
  }
  for (Uint32 i = 0; i < start_index; i++)
  {
    Uint32 disk_write_speed_record = get_disk_write_speed_record(i);
    millis_passed +=
      disk_write_speed_rep[disk_write_speed_record].millis_passed;
  }
  return millis_passed;
}

void Backup::execDBINFO_SCANREQ(Signal *signal)
{
  jamEntry();
  DbinfoScanReq req= *(DbinfoScanReq*)signal->theData;
  const Ndbinfo::ScanCursor* cursor =
    CAST_CONSTPTR(Ndbinfo::ScanCursor, DbinfoScan::getCursorPtr(&req));

  Ndbinfo::Ratelimit rl;

  switch(req.tableId){
  case Ndbinfo::POOLS_TABLEID:
  {
    Ndbinfo::pool_entry pools[] =
    {
      { "Backup Record",
        c_backupPool.getUsed(),
        c_backupPool.getSize(),
        c_backupPool.getEntrySize(),
        c_backupPool.getUsedHi(),
        { CFG_DB_PARALLEL_BACKUPS,0,0,0 }},
      { "Backup File",
        c_backupFilePool.getUsed(),
        c_backupFilePool.getSize(),
        c_backupFilePool.getEntrySize(),
        c_backupFilePool.getUsedHi(),
        { CFG_DB_PARALLEL_BACKUPS,0,0,0 }},
      { "Table",
        c_tablePool.getUsed(),
        c_tablePool.getSize(),
        c_tablePool.getEntrySize(),
        c_tablePool.getUsedHi(),
        { CFG_DB_PARALLEL_BACKUPS,
          CFG_DB_NO_TABLES,
          CFG_DB_NO_ORDERED_INDEXES,
          CFG_DB_NO_UNIQUE_HASH_INDEXES }},
      { "Trigger",
        c_triggerPool.getUsed(),
        c_triggerPool.getSize(),
        c_triggerPool.getEntrySize(),
        c_triggerPool.getUsedHi(),
        { CFG_DB_PARALLEL_BACKUPS,
          CFG_DB_NO_TABLES,
          CFG_DB_NO_ORDERED_INDEXES,
          CFG_DB_NO_UNIQUE_HASH_INDEXES }},
      { "Fragment",
        c_fragmentPool.getUsed(),
        c_fragmentPool.getSize(),
        c_fragmentPool.getEntrySize(),
        c_fragmentPool.getUsedHi(),
        { CFG_DB_NO_TABLES,
          CFG_DB_NO_ORDERED_INDEXES,
          CFG_DB_NO_UNIQUE_HASH_INDEXES,0 }},
      { "Page",
        c_pagePool.getUsed(),
        c_pagePool.getSize(),
        c_pagePool.getEntrySize(),
        c_pagePool.getUsedHi(),
        { CFG_DB_BACKUP_MEM,
          CFG_DB_BACKUP_DATA_BUFFER_MEM,0,0 }},
      { NULL, 0,0,0,0, { 0,0,0,0 }}
    };

    const size_t num_config_params =
      sizeof(pools[0].config_params) / sizeof(pools[0].config_params[0]);
    Uint32 pool = cursor->data[0];
    BlockNumber bn = blockToMain(number());
    while(pools[pool].poolname)
    {
      jam();
      Ndbinfo::Row row(signal, req);
      row.write_uint32(getOwnNodeId());
      row.write_uint32(bn);           // block number
      row.write_uint32(instance());   // block instance
      row.write_string(pools[pool].poolname);

      row.write_uint64(pools[pool].used);
      row.write_uint64(pools[pool].total);
      row.write_uint64(pools[pool].used_hi);
      row.write_uint64(pools[pool].entry_size);
      for (size_t i = 0; i < num_config_params; i++)
        row.write_uint32(pools[pool].config_params[i]);
      ndbinfo_send_row(signal, req, row, rl);
      pool++;
      if (rl.need_break(req))
      {
        jam();
        ndbinfo_send_scan_break(signal, req, rl, pool);
        return;
      }
    }
    break;
  }
  case Ndbinfo::DISK_WRITE_SPEED_AGGREGATE_TABLEID:
  {

    jam();
    Uint64 backup_lcp_bytes_written;
    Uint64 backup_bytes_written;
    Uint64 redo_bytes_written;
    Uint64 std_dev_backup;
    Uint64 std_dev_backup_lcp;
    Uint64 std_dev_redo;
    Uint64 millis_passed;
    Ndbinfo::Row row(signal, req);
    Uint32 ldm_instance = instance();
 
    if (ldm_instance > 0)
    {
      /* Always start counting instances from 0 */
      ldm_instance--;
    }
    row.write_uint32(getOwnNodeId());
    row.write_uint32(ldm_instance);

    /* Report last second */
    calculate_disk_write_speed_seconds_back(1,
                                            millis_passed,
                                            backup_lcp_bytes_written,
                                            backup_bytes_written,
                                            redo_bytes_written);

    row.write_uint64((backup_lcp_bytes_written / millis_passed ) * 1000);
    row.write_uint64((redo_bytes_written / millis_passed) * 1000);

    /* Report average and std_dev of last 10 seconds */
    calculate_disk_write_speed_seconds_back(10,
                                            millis_passed,
                                            backup_lcp_bytes_written,
                                            backup_bytes_written,
                                            redo_bytes_written);

    row.write_uint64((backup_lcp_bytes_written * 1000) / millis_passed);
    row.write_uint64((redo_bytes_written * 1000) / millis_passed);

    calculate_std_disk_write_speed_seconds_back(10,
                                                millis_passed,
                                                backup_lcp_bytes_written,
                                                backup_bytes_written,
                                                redo_bytes_written,
                                                std_dev_backup_lcp,
                                                std_dev_backup,
                                                std_dev_redo);

    row.write_uint64(std_dev_backup_lcp);
    row.write_uint64(std_dev_redo);
 
    /* Report average and std_dev of last 60 seconds */
    calculate_disk_write_speed_seconds_back(60,
                                            millis_passed,
                                            backup_lcp_bytes_written,
                                            backup_bytes_written,
                                            redo_bytes_written);

    row.write_uint64((backup_lcp_bytes_written / millis_passed ) * 1000);
    row.write_uint64((redo_bytes_written / millis_passed) * 1000);

    calculate_std_disk_write_speed_seconds_back(60,
                                                millis_passed,
                                                backup_lcp_bytes_written,
                                                backup_bytes_written,
                                                redo_bytes_written,
                                                std_dev_backup_lcp,
                                                std_dev_backup,
                                                std_dev_redo);

    row.write_uint64(std_dev_backup_lcp);
    row.write_uint64(std_dev_redo);

    row.write_uint64(slowdowns_due_to_io_lag);
    row.write_uint64(slowdowns_due_to_high_cpu);
    row.write_uint64(disk_write_speed_set_to_min);
    row.write_uint64(m_curr_disk_write_speed *
                     CURR_DISK_SPEED_CONVERSION_FACTOR_TO_SECONDS);

    ndbinfo_send_row(signal, req, row, rl);
    break;
  }
  case Ndbinfo::DISK_WRITE_SPEED_BASE_TABLEID:
  {
    jam();
    Uint32 ldm_instance = instance();
 
    if (ldm_instance > 0)
    {
      /* Always start counting instances from 0 */
      ldm_instance--;
    }
    Uint32 start_index = cursor->data[0];
    for ( ; start_index < DISK_WRITE_SPEED_REPORT_SIZE;)
    {
      jam();
      Ndbinfo::Row row(signal, req);
      row.write_uint32(getOwnNodeId());
      row.write_uint32(ldm_instance);
      Uint32 disk_write_speed_record = get_disk_write_speed_record(start_index);
      if (disk_write_speed_record != DISK_WRITE_SPEED_REPORT_SIZE)
      {
        jam();
        Uint64 backup_lcp_bytes_written_this_period =
          disk_write_speed_rep[disk_write_speed_record].
            backup_lcp_bytes_written;
        Uint64 redo_bytes_written_this_period =
          disk_write_speed_rep[disk_write_speed_record].
            redo_bytes_written;
        Uint64 millis_passed_this_period =
          disk_write_speed_rep[disk_write_speed_record].millis_passed;
        Uint64 millis_since_finished =
          calculate_millis_since_finished(start_index);
        Uint64 target_disk_write_speed =
          disk_write_speed_rep[disk_write_speed_record].target_disk_write_speed;

        row.write_uint64(millis_since_finished);
        row.write_uint64(millis_passed_this_period);
        row.write_uint64(backup_lcp_bytes_written_this_period);
        row.write_uint64(redo_bytes_written_this_period);
        row.write_uint64(target_disk_write_speed);
      }
      else
      {
        jam();
        row.write_uint64((Uint64)0);
        row.write_uint64((Uint64)0);
        row.write_uint64((Uint64)0);
        row.write_uint64((Uint64)0);
        row.write_uint64((Uint64)0);
      }
      ndbinfo_send_row(signal, req, row, rl);
      start_index++;
      if (rl.need_break(req))
      {
        jam();
        ndbinfo_send_scan_break(signal, req, rl, start_index);
        return;
      }
    }
    break;
  }
  case Ndbinfo::LOGBUFFERS_TABLEID:
  {
    jam();
    BackupRecordPtr ptr;
    ndbrequire(c_backups.first(ptr));

    jam();

    if (isNdbMtLqh() && instance() != UserBackupInstanceKey)
    {
      // only LDM1 participates in backup, so other threads
      // always have buffer usage = 0
      break;
    }
    Uint32 files[2] = { ptr.p->dataFilePtr[0], ptr.p->logFilePtr };
    for (Uint32 i=0; i<NDB_ARRAY_SIZE(files); i++)
    {
      jam();
      Uint32 usableBytes, freeLwmBytes, freeSizeBytes;
      usableBytes = freeLwmBytes = freeSizeBytes = 0;
      Uint32 logtype = Ndbinfo::BACKUP_DATA_BUFFER;

      switch(i){
      case 0:
        logtype = Ndbinfo::BACKUP_DATA_BUFFER;
        usableBytes = c_defaults.m_dataBufferSize;
        break;
      case 1:
        logtype = Ndbinfo::BACKUP_LOG_BUFFER;
        usableBytes = c_defaults.m_logBufferSize;
        break;
      default:
        ndbrequire(false);
        break;
      };

      BackupFilePtr filePtr;
      ptr.p->files.getPtr(filePtr, files[i]);
      if (ptr.p->logFilePtr != RNIL)
      {
        freeSizeBytes = filePtr.p->operation.dataBuffer.getFreeSize() << 2;
        freeLwmBytes = filePtr.p->operation.dataBuffer.getFreeLwm() << 2;
      }
      else
      {
        freeSizeBytes = usableBytes;
        freeLwmBytes = usableBytes;
      }

      Ndbinfo::Row data_row(signal, req);
      data_row.write_uint32(getOwnNodeId());
      data_row.write_uint32(logtype);
      data_row.write_uint32(0);   // log id, always 0
      data_row.write_uint32(instance());     // log part, instance for ndbmtd

      data_row.write_uint64(usableBytes);        // total allocated
      data_row.write_uint64(usableBytes - freeSizeBytes); // currently in use
      data_row.write_uint64(usableBytes - freeLwmBytes);  // high water mark
      // only 2 rows to send in total, so ignore ratelimit
      ndbinfo_send_row(signal, req, data_row, rl);
    }
    break;
  }
  default:
    break;
  }

  ndbinfo_send_scan_conf(signal, req, rl);
}

static const Uint32 MAX_TABLE_MAPS = 2;
bool
Backup::findTable(const BackupRecordPtr & ptr, 
		  TablePtr & tabPtr, Uint32 tableId)
{
  Uint32 loopCount = 0;
  tabPtr.i = c_tableMap[tableId];
  while (loopCount++ < MAX_TABLE_MAPS)
  {
    if (tabPtr.i == RNIL)
    {
      jam();
      return false;
    }
    c_tablePool.getPtr(tabPtr);
    if (tabPtr.p->backupPtrI == ptr.i)
    {
      jam();
      return true;
    }
    jam();
    tabPtr.i = tabPtr.p->nextMapTable;
  }
  return false;
}

void
Backup::insertTableMap(TablePtr & tabPtr,
                       Uint32 backupPtrI,
                       Uint32 tableId)
{
  tabPtr.p->backupPtrI = backupPtrI;
  tabPtr.p->tableId = tableId;
  tabPtr.p->nextMapTable = c_tableMap[tableId];
  c_tableMap[tableId] = tabPtr.i;
} 

void
Backup::removeTableMap(TablePtr &tabPtr,
                       Uint32 backupPtr,
                       Uint32 tableId)
{
  TablePtr prevTabPtr;
  TablePtr locTabPtr;
  Uint32 loopCount = 0;

  prevTabPtr.i = RNIL;
  prevTabPtr.p = 0;
  locTabPtr.i = c_tableMap[tableId];

  while (loopCount++ < MAX_TABLE_MAPS)
  {
    jam();
    c_tablePool.getPtr(locTabPtr);
    ndbrequire(locTabPtr.p->tableId == tableId);
    if (locTabPtr.p->backupPtrI == backupPtr)
    {
      ndbrequire(tabPtr.i == locTabPtr.i);
      if (prevTabPtr.i == RNIL)
      {
        jam();
        c_tableMap[tableId] = locTabPtr.p->nextMapTable;
      }
      else
      {
        jam();
        prevTabPtr.p->nextMapTable = locTabPtr.p->nextMapTable;
      }
      locTabPtr.p->nextMapTable = RNIL;
      locTabPtr.p->tableId = RNIL;
      locTabPtr.p->backupPtrI = RNIL;
      return;
    }
    prevTabPtr = locTabPtr;
    locTabPtr.i = locTabPtr.p->nextMapTable;
  }
  ndbrequire(false);
}

static Uint32 xps(Uint64 x, Uint64 ms)
{
  float fx = float(x);
  float fs = float(ms);
  
  if(ms == 0 || x == 0) {
    jamNoBlock();
    return 0;
  }//if
  jamNoBlock();
  return ((Uint32)(1000.0f * (fx + fs/2.1f))) / ((Uint32)fs);
}

struct Number {
  Number(Uint64 r) { val = r;}
  Number & operator=(Uint64 r) { val = r; return * this; }
  Uint64 val;
};

NdbOut &
operator<< (NdbOut & out, const Number & val){
  char p = 0;
  Uint32 loop = 1;
  while(val.val > loop){
    loop *= 1000;
    p += 3;
  }
  if(loop != 1){
    p -= 3;
    loop /= 1000;
  }

  switch(p){
  case 0:
    break;
  case 3:
    p = 'k';
    break;
  case 6:
    p = 'M';
    break;
  case 9:
    p = 'G';
    break;
  default:
    p = 0;
  }
  char str[2];
  str[0] = p;
  str[1] = 0;
  Uint32 tmp = (Uint32)((val.val + (loop >> 1)) / loop);
#if 1
  if(p > 0)
    out << tmp << str;
  else
    out << tmp;
#else
  out << val.val;
#endif

  return out;
}

void
Backup::execBACKUP_CONF(Signal* signal)
{
  jamEntry();
  BackupConf * conf = (BackupConf*)signal->getDataPtr();
  
  ndbout_c("Backup %u has started", conf->backupId);
}

void
Backup::execBACKUP_REF(Signal* signal)
{
  jamEntry();
  BackupRef * ref = (BackupRef*)signal->getDataPtr();

  ndbout_c("Backup (%u) has NOT started %d", ref->senderData, ref->errorCode);
}

void
Backup::execBACKUP_COMPLETE_REP(Signal* signal)
{
  jamEntry();
  BackupCompleteRep* rep = (BackupCompleteRep*)signal->getDataPtr();
 
  const NDB_TICKS now = NdbTick_getCurrentTicks();
  const Uint64 elapsed = NdbTick_Elapsed(startTime,now).milliSec();
  
  ndbout_c("Backup %u has completed", rep->backupId);
  const Uint64 bytes =
    rep->noOfBytesLow + (((Uint64)rep->noOfBytesHigh) << 32);
  const Uint64 records =
    rep->noOfRecordsLow + (((Uint64)rep->noOfRecordsHigh) << 32);

  Number rps = xps(records, elapsed);
  Number bps = xps(bytes, elapsed);

  ndbout << " Data [ "
	 << Number(records) << " rows " 
	 << Number(bytes) << " bytes " << elapsed << " ms ] " 
	 << " => "
	 << rps << " row/s & " << bps << "b/s" << endl;

  bps = xps(rep->noOfLogBytes, elapsed);
  rps = xps(rep->noOfLogRecords, elapsed);

  ndbout << " Log [ "
	 << Number(rep->noOfLogRecords) << " log records " 
	 << Number(rep->noOfLogBytes) << " bytes " << elapsed << " ms ] " 
	 << " => "
	 << rps << " records/s & " << bps << "b/s" << endl;

}

void
Backup::execBACKUP_ABORT_REP(Signal* signal)
{
  jamEntry();
  BackupAbortRep* rep = (BackupAbortRep*)signal->getDataPtr();
  
  ndbout_c("Backup %u has been aborted %d", rep->backupId, rep->reason);
}

const TriggerEvent::Value triggerEventValues[] = {
  TriggerEvent::TE_INSERT,
  TriggerEvent::TE_UPDATE,
  TriggerEvent::TE_DELETE
};

const Backup::State 
Backup::validSlaveTransitions[] = {
  INITIAL,  DEFINING,
  DEFINING, DEFINED,
  DEFINED,  STARTED,
  STARTED,  STARTED, // Several START_BACKUP_REQ is sent
  STARTED,  SCANNING,
  SCANNING, STARTED,
  STARTED,  STOPPING,
  STOPPING, CLEANING,
  CLEANING, INITIAL,
  
  INITIAL,  ABORTING, // Node fail
  DEFINING, ABORTING,
  DEFINED,  ABORTING,
  STARTED,  ABORTING,
  SCANNING, ABORTING,
  STOPPING, ABORTING,
  CLEANING, ABORTING, // Node fail w/ master takeover
  ABORTING, ABORTING, // Slave who initiates ABORT should have this transition
  
  ABORTING, INITIAL,
  INITIAL,  INITIAL
};

const Uint32
Backup::validSlaveTransitionsCount = 
sizeof(Backup::validSlaveTransitions) / sizeof(Backup::State);

void
Backup::CompoundState::setState(State newState){
  bool found = false;
  const State currState = state;
  for(unsigned i = 0; i<noOfValidTransitions; i+= 2) {
    jam();
    if(validTransitions[i]   == currState &&
       validTransitions[i+1] == newState){
      jam();
      found = true;
      break;
    }
  }

  //ndbrequire(found);
  
  if (newState == INITIAL)
    abortState = INITIAL;
  if(newState == ABORTING && currState != ABORTING) {
    jam();
    abortState = currState;
  }
  state = newState;
#ifdef DEBUG_ABORT
  if (newState != currState) {
    ndbout_c("%u: Old state = %u, new state = %u, abort state = %u",
	     id, currState, newState, abortState);
  }
#endif
}

void
Backup::CompoundState::forceState(State newState)
{
  const State currState = state;
  if (newState == INITIAL)
    abortState = INITIAL;
  if(newState == ABORTING && currState != ABORTING) {
    jam();
    abortState = currState;
  }
  state = newState;
#ifdef DEBUG_ABORT
  if (newState != currState) {
    ndbout_c("%u: FORCE: Old state = %u, new state = %u, abort state = %u",
	     id, currState, newState, abortState);
  }
#endif
}

Backup::Table::Table(Fragment_pool & fh)
  : fragments(fh)
{
  triggerIds[0] = ILLEGAL_TRIGGER_ID;
  triggerIds[1] = ILLEGAL_TRIGGER_ID;
  triggerIds[2] = ILLEGAL_TRIGGER_ID;
  triggerAllocated[0] = false;
  triggerAllocated[1] = false;
  triggerAllocated[2] = false;
}

/*****************************************************************************
 * 
 * Node state handling
 *
 *****************************************************************************/
void
Backup::execNODE_FAILREP(Signal* signal)
{
  jamEntry();

  NodeFailRep * rep = (NodeFailRep*)signal->getDataPtr();
  
  bool doStuff = false;
  /*
  Start by saving important signal data which will be destroyed before the
  process is completed.
  */
  NodeId new_master_node_id = rep->masterNodeId;
  Uint32 theFailedNodes[NdbNodeBitmask::Size];
  for (Uint32 i = 0; i < NdbNodeBitmask::Size; i++)
    theFailedNodes[i] = rep->theNodes[i];
  
  c_masterNodeId = new_master_node_id;

  NodePtr nodePtr;
  for(c_nodes.first(nodePtr); nodePtr.i != RNIL; c_nodes.next(nodePtr)) {
    jam();
    if(NdbNodeBitmask::get(theFailedNodes, nodePtr.p->nodeId)){
      if(nodePtr.p->alive){
	jam();
	ndbrequire(c_aliveNodes.get(nodePtr.p->nodeId));
	doStuff = true;
      } else {
        jam();
	ndbrequire(!c_aliveNodes.get(nodePtr.p->nodeId));
      }//if
      nodePtr.p->alive = 0;
      c_aliveNodes.clear(nodePtr.p->nodeId);
    }//if
  }//for

  if(!doStuff){
    jam();
    return;
  }//if
  
#ifdef DEBUG_ABORT
  ndbout_c("****************** Node fail rep ******************");
#endif

  NodeId newCoordinator = c_masterNodeId;
  BackupRecordPtr ptr;
  for(c_backups.first(ptr); ptr.i != RNIL; c_backups.next(ptr)) {
    jam();
    checkNodeFail(signal, ptr, newCoordinator, theFailedNodes);
  }

  /* Block level cleanup */
  for(unsigned i = 1; i < MAX_NDB_NODES; i++) {
    jam();
    if(NdbNodeBitmask::get(theFailedNodes, i))
    {
      jam();
      Uint32 elementsCleaned = simBlockNodeFailure(signal, i); // No callback
      ndbassert(elementsCleaned == 0); // Backup should have no distributed frag signals
      (void) elementsCleaned; // Remove compiler warning
    }//if
  }//for
}

bool
Backup::verifyNodesAlive(BackupRecordPtr ptr,
			 const NdbNodeBitmask& aNodeBitMask)
{
  Uint32 version = getNodeInfo(getOwnNodeId()).m_version;
  for (Uint32 i = 0; i < MAX_NDB_NODES; i++) {
    jam();
    if(aNodeBitMask.get(i)) {
      if(!c_aliveNodes.get(i)){
        jam();
	ptr.p->setErrorCode(AbortBackupOrd::BackupFailureDueToNodeFail);
        return false;
      }//if
      if(getNodeInfo(i).m_version != version)
      {
	jam();
	ptr.p->setErrorCode(AbortBackupOrd::IncompatibleVersions);
	return false;
      }
    }//if
  }//for
  return true;
}

void
Backup::checkNodeFail(Signal* signal,
		      BackupRecordPtr ptr,
		      NodeId newCoord,
		      Uint32 theFailedNodes[NdbNodeBitmask::Size])
{
  NdbNodeBitmask mask;
  mask.assign(2, theFailedNodes);

  /* Update ptr.p->nodes to be up to date with current alive nodes
   */
  NodePtr nodePtr;
  bool found = false;
  for(c_nodes.first(nodePtr); nodePtr.i != RNIL; c_nodes.next(nodePtr)) {
    jam();
    if(NdbNodeBitmask::get(theFailedNodes, nodePtr.p->nodeId)) {
      jam();
      if (ptr.p->nodes.get(nodePtr.p->nodeId)) {
	jam();
	ptr.p->nodes.clear(nodePtr.p->nodeId); 
	found = true;
      }
    }//if
  }//for

  if(!found) {
    jam();
    return; // failed node is not part of backup process, safe to continue
  }

  if(mask.get(refToNode(ptr.p->masterRef)))
  {
    /**
     * Master died...abort
     */
    ptr.p->masterRef = reference();
    ptr.p->nodes.clear();
    ptr.p->nodes.set(getOwnNodeId());
    ptr.p->setErrorCode(AbortBackupOrd::BackupFailureDueToNodeFail);
    switch(ptr.p->m_gsn){
    case GSN_DEFINE_BACKUP_REQ:
    case GSN_START_BACKUP_REQ:
    case GSN_BACKUP_FRAGMENT_REQ:
    case GSN_STOP_BACKUP_REQ:
      // I'm currently processing...reply to self and abort...
      ptr.p->masterData.gsn = ptr.p->m_gsn;
      ptr.p->masterData.sendCounter = ptr.p->nodes;
      return;
    case GSN_DEFINE_BACKUP_REF:
    case GSN_DEFINE_BACKUP_CONF:
    case GSN_START_BACKUP_REF:
    case GSN_START_BACKUP_CONF:
    case GSN_BACKUP_FRAGMENT_REF:
    case GSN_BACKUP_FRAGMENT_CONF:
    case GSN_STOP_BACKUP_REF:
    case GSN_STOP_BACKUP_CONF:
      ptr.p->masterData.gsn = GSN_DEFINE_BACKUP_REQ;
      masterAbort(signal, ptr);
      return;
    case GSN_ABORT_BACKUP_ORD:
      // Already aborting
      return;
    }
  }
  else if (newCoord == getOwnNodeId())
  {
    /**
     * I'm master for this backup
     */
    jam();
    CRASH_INSERTION((10001));
#ifdef DEBUG_ABORT
    ndbout_c("**** Master: Node failed: Master id = %u", 
	     refToNode(ptr.p->masterRef));
#endif

    Uint32 gsn, len, pos;
    ptr.p->nodes.bitANDC(mask);
    switch(ptr.p->masterData.gsn){
    case GSN_DEFINE_BACKUP_REQ:
    {
      DefineBackupRef * ref = (DefineBackupRef*)signal->getDataPtrSend();
      ref->backupPtr = ptr.i;
      ref->backupId = ptr.p->backupId;
      ref->errorCode = AbortBackupOrd::BackupFailureDueToNodeFail;
      gsn= GSN_DEFINE_BACKUP_REF;
      len= DefineBackupRef::SignalLength;
      pos= Uint32(&ref->nodeId - signal->getDataPtrSend());
      break;
    }
    case GSN_START_BACKUP_REQ:
    {
      StartBackupRef * ref = (StartBackupRef*)signal->getDataPtrSend();
      ref->backupPtr = ptr.i;
      ref->backupId = ptr.p->backupId;
      ref->errorCode = AbortBackupOrd::BackupFailureDueToNodeFail;
      gsn= GSN_START_BACKUP_REF;
      len= StartBackupRef::SignalLength;
      pos= Uint32(&ref->nodeId - signal->getDataPtrSend());
      break;
    }
    case GSN_BACKUP_FRAGMENT_REQ:
    {
      BackupFragmentRef * ref = (BackupFragmentRef*)signal->getDataPtrSend();
      ref->backupPtr = ptr.i;
      ref->backupId = ptr.p->backupId;
      ref->errorCode = AbortBackupOrd::BackupFailureDueToNodeFail;
      gsn= GSN_BACKUP_FRAGMENT_REF;
      len= BackupFragmentRef::SignalLength;
      pos= Uint32(&ref->nodeId - signal->getDataPtrSend());
      break;
    }
    case GSN_STOP_BACKUP_REQ:
    {
      StopBackupRef * ref = (StopBackupRef*)signal->getDataPtrSend();
      ref->backupPtr = ptr.i;
      ref->backupId = ptr.p->backupId;
      ref->errorCode = AbortBackupOrd::BackupFailureDueToNodeFail;
      ref->nodeId = getOwnNodeId();
      gsn= GSN_STOP_BACKUP_REF;
      len= StopBackupRef::SignalLength;
      pos= Uint32(&ref->nodeId - signal->getDataPtrSend());
      break;
    }
    case GSN_WAIT_GCP_REQ:
    case GSN_DROP_TRIG_IMPL_REQ:
    case GSN_CREATE_TRIG_IMPL_REQ:
    case GSN_ALTER_TRIG_IMPL_REQ:
      ptr.p->setErrorCode(AbortBackupOrd::BackupFailureDueToNodeFail);
      return;
    case GSN_UTIL_SEQUENCE_REQ:
    case GSN_UTIL_LOCK_REQ:
      return;
    default:
      ndbrequire(false);
    }
    
    for(Uint32 i = 0; (i = mask.find(i+1)) != NdbNodeBitmask::NotFound; )
    {
      signal->theData[pos] = i;
      sendSignal(reference(), gsn, signal, len, JBB);
#ifdef DEBUG_ABORT
      ndbout_c("sending %d to self from %d", gsn, i);
#endif
    }
    return;
  }//if
  
  /**
   * I abort myself as slave if not master
   */
  CRASH_INSERTION((10021));
} 

void
Backup::execINCL_NODEREQ(Signal* signal)
{
  jamEntry();
  
  const Uint32 senderRef = signal->theData[0];
  const Uint32 inclNode  = signal->theData[1];

  NodePtr node;
  for(c_nodes.first(node); node.i != RNIL; c_nodes.next(node)) {
    jam();
    const Uint32 nodeId = node.p->nodeId;
    if(inclNode == nodeId){
      jam();
      
      ndbrequire(node.p->alive == 0);
      ndbrequire(!c_aliveNodes.get(nodeId));
      
      node.p->alive = 1;
      c_aliveNodes.set(nodeId);
      
      break;
    }//if
  }//for
  signal->theData[0] = inclNode;
  signal->theData[1] = reference();
  sendSignal(senderRef, GSN_INCL_NODECONF, signal, 2, JBB);
}

/*****************************************************************************
 * 
 * Master functionallity - Define backup
 *
 *****************************************************************************/

void
Backup::execBACKUP_REQ(Signal* signal)
{
  jamEntry();
  BackupReq * req = (BackupReq*)signal->getDataPtr();
  
  const Uint32 senderData = req->senderData;
  const BlockReference senderRef = signal->senderBlockRef();
  const Uint32 dataLen32 = req->backupDataLen; // In 32 bit words
  const Uint32 flags = signal->getLength() > 2 ? req->flags : 2;
  const Uint32 input_backupId = signal->getLength() > 3 ? req->inputBackupId : 0;

  if (getOwnNodeId() != getMasterNodeId())
  {
    jam();
    sendBackupRef(senderRef, flags, signal, senderData,
                  BackupRef::IAmNotMaster);
    return;
  }//if

  if (c_defaults.m_diskless)
  {
    jam();
    sendBackupRef(senderRef, flags, signal, senderData, 
		  BackupRef::CannotBackupDiskless);
    return;
  }
  
  if (dataLen32 != 0)
  {
    jam();
    sendBackupRef(senderRef, flags, signal, senderData, 
		  BackupRef::BackupDefinitionNotImplemented);
    return;
  }//if
  
#ifdef DEBUG_ABORT
  dumpUsedResources();
#endif
  /**
   * Seize a backup record
   */
  BackupRecordPtr ptr;
  c_backups.seizeFirst(ptr);
  if (ptr.i == RNIL)
  {
    jam();
    sendBackupRef(senderRef, flags, signal, senderData,
                  BackupRef::OutOfBackupRecord);
    return;
  }//if

  ndbrequire(ptr.p->tables.isEmpty());
  
  ptr.p->m_gsn = 0;
  ptr.p->errorCode = 0;
  ptr.p->clientRef = senderRef;
  ptr.p->clientData = senderData;
  ptr.p->flags = flags;
  ptr.p->masterRef = reference();
  ptr.p->nodes = c_aliveNodes;
  if (input_backupId)
  {
    jam();
    ptr.p->backupId = input_backupId;
  }
  else
  {
    jam();
    ptr.p->backupId = 0;
  }
  ptr.p->backupKey[0] = 0;
  ptr.p->backupKey[1] = 0;
  ptr.p->backupDataLen = 0;
  ptr.p->masterData.errorCode = 0;

  ptr.p->masterData.sequence.retriesLeft = 3;
  sendUtilSequenceReq(signal, ptr);
}

void
Backup::sendUtilSequenceReq(Signal* signal, BackupRecordPtr ptr, Uint32 delay)
{
  jam();

  UtilSequenceReq * utilReq = (UtilSequenceReq*)signal->getDataPtrSend();
  ptr.p->masterData.gsn = GSN_UTIL_SEQUENCE_REQ;
  utilReq->senderData  = ptr.i;
  utilReq->sequenceId  = NDB_BACKUP_SEQUENCE;

  if (ptr.p->backupId) 
  {
    jam();
    utilReq->requestType = UtilSequenceReq::SetVal;
    utilReq->value = ptr.p->backupId;
  }
  else
  {
    jam();
    utilReq->requestType = UtilSequenceReq::NextVal;
  }

  if (delay == 0)
  {
    jam();
    sendSignal(DBUTIL_REF, GSN_UTIL_SEQUENCE_REQ,
               signal, UtilSequenceReq::SignalLength, JBB);
  }
  else
  {
    jam();
    sendSignalWithDelay(DBUTIL_REF, GSN_UTIL_SEQUENCE_REQ,
                        signal, delay, UtilSequenceReq::SignalLength);
  }
}

void
Backup::execUTIL_SEQUENCE_REF(Signal* signal)
{
  jamEntry();
  BackupRecordPtr ptr;
  UtilSequenceRef * utilRef = (UtilSequenceRef*)signal->getDataPtr();
  ptr.i = utilRef->senderData;
  c_backupPool.getPtr(ptr);
  ndbrequire(ptr.p->masterData.gsn == GSN_UTIL_SEQUENCE_REQ);

  if (utilRef->errorCode == UtilSequenceRef::TCError)
  {
    jam();
    if (ptr.p->masterData.sequence.retriesLeft > 0)
    {
      jam();
      infoEvent("BACKUP: retrying sequence on error %u",
                utilRef->TCErrorCode);
      ptr.p->masterData.sequence.retriesLeft--;
      sendUtilSequenceReq(signal, ptr, 300);
      return;
    }
  }
  warningEvent("BACKUP: aborting due to sequence error (%u, %u)",
               utilRef->errorCode,
               utilRef->TCErrorCode);

  sendBackupRef(signal, ptr, BackupRef::SequenceFailure);
}//execUTIL_SEQUENCE_REF()

void
Backup::sendBackupRef(Signal* signal, BackupRecordPtr ptr, Uint32 errorCode)
{
  jam();
  sendBackupRef(ptr.p->clientRef, ptr.p->flags, signal,
                ptr.p->clientData, errorCode);
  cleanup(signal, ptr);
}

void
Backup::sendBackupRef(BlockReference senderRef, Uint32 flags, Signal *signal,
		      Uint32 senderData, Uint32 errorCode)
{
  jam();
  if (SEND_BACKUP_STARTED_FLAG(flags))
  {
    jam();
    BackupRef* ref = (BackupRef*)signal->getDataPtrSend();
    ref->senderData = senderData;
    ref->errorCode = errorCode;
    ref->masterRef = numberToRef(BACKUP, getMasterNodeId());
    sendSignal(senderRef, GSN_BACKUP_REF, signal, BackupRef::SignalLength, JBB);
  }

  if (errorCode != BackupRef::IAmNotMaster)
  {
    jam();
    signal->theData[0] = NDB_LE_BackupFailedToStart;
    signal->theData[1] = senderRef;
    signal->theData[2] = errorCode;
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 3, JBB);
  }
}

void
Backup::execUTIL_SEQUENCE_CONF(Signal* signal)
{
  jamEntry();

  UtilSequenceConf * conf = (UtilSequenceConf*)signal->getDataPtr();
  
  if(conf->requestType == UtilSequenceReq::Create) 
  {
    jam();
    sendSTTORRY(signal); // At startup in NDB
    return;
  }

  BackupRecordPtr ptr;
  ptr.i = conf->senderData;
  c_backupPool.getPtr(ptr);

  ndbrequire(ptr.p->masterData.gsn == GSN_UTIL_SEQUENCE_REQ);

  if (ptr.p->checkError())
  {
    jam();
    sendBackupRef(signal, ptr, ptr.p->errorCode);
    return;
  }//if

  if (ERROR_INSERTED(10023)) 
  {
    sendBackupRef(signal, ptr, 323);
    return;
  }//if


  if(!ptr.p->backupId && conf->requestType != UtilSequenceReq::SetVal)
  {
    Uint64 backupId;
    memcpy(&backupId,conf->sequenceValue,8);
    ptr.p->backupId= (Uint32)backupId;
  }

  ptr.p->backupKey[0] = (getOwnNodeId() << 16) | (ptr.p->backupId & 0xFFFF);
  ptr.p->backupKey[1] = Uint32(NdbTick_CurrentMillisecond());

  ptr.p->masterData.gsn = GSN_UTIL_LOCK_REQ;
  Mutex mutex(signal, c_mutexMgr, ptr.p->masterData.m_defineBackupMutex);
  Callback c = { safe_cast(&Backup::defineBackupMutex_locked), ptr.i };
  ndbrequire(mutex.lock(c));

  return;
}

void
Backup::defineBackupMutex_locked(Signal* signal, Uint32 ptrI, Uint32 retVal){
  jamEntry();
  ndbrequire(retVal == 0);
  
  BackupRecordPtr ptr;
  ptr.i = ptrI;
  c_backupPool.getPtr(ptr);
  
  ndbrequire(ptr.p->masterData.gsn == GSN_UTIL_LOCK_REQ);

  ptr.p->masterData.gsn = GSN_UTIL_LOCK_REQ;
  Mutex mutex(signal, c_mutexMgr, ptr.p->masterData.m_dictCommitTableMutex);
  Callback c = { safe_cast(&Backup::dictCommitTableMutex_locked), ptr.i };
  ndbrequire(mutex.lock(c));
}

void
Backup::dictCommitTableMutex_locked(Signal* signal, Uint32 ptrI,Uint32 retVal)
{
  jamEntry();
  ndbrequire(retVal == 0);
  
  /**
   * We now have both the mutexes
   */
  BackupRecordPtr ptr;
  ptr.i = ptrI;
  c_backupPool.getPtr(ptr);

  ndbrequire(ptr.p->masterData.gsn == GSN_UTIL_LOCK_REQ);

  if (ERROR_INSERTED(10031)) {
    ptr.p->setErrorCode(331);
  }//if

  if (ptr.p->checkError())
  {
    jam();
    
    /**
     * Unlock mutexes
     */
    jam();
    Mutex mutex1(signal, c_mutexMgr, ptr.p->masterData.m_dictCommitTableMutex);
    jam();
    mutex1.unlock(); // ignore response
    
    jam();
    Mutex mutex2(signal, c_mutexMgr, ptr.p->masterData.m_defineBackupMutex);
    jam();
    mutex2.unlock(); // ignore response
    
    sendBackupRef(signal, ptr, ptr.p->errorCode);
    return;
  }//if
  
  sendDefineBackupReq(signal, ptr);
}

/*****************************************************************************
 * 
 * Master functionallity - Define backup cont'd (from now on all slaves are in)
 *
 *****************************************************************************/

bool
Backup::haveAllSignals(BackupRecordPtr ptr, Uint32 gsn, Uint32 nodeId)
{ 
  ndbrequire(ptr.p->masterRef == reference());
  ndbrequire(ptr.p->masterData.gsn == gsn);
  ndbrequire(!ptr.p->masterData.sendCounter.done());
  ndbrequire(ptr.p->masterData.sendCounter.isWaitingFor(nodeId));
  
  ptr.p->masterData.sendCounter.clearWaitingFor(nodeId);
  return ptr.p->masterData.sendCounter.done();
}

void
Backup::sendDefineBackupReq(Signal *signal, BackupRecordPtr ptr)
{
  /**
   * Sending define backup to all participants
   */
  DefineBackupReq * req = (DefineBackupReq*)signal->getDataPtrSend();
  req->backupId = ptr.p->backupId;
  req->clientRef = ptr.p->clientRef;
  req->clientData = ptr.p->clientData;
  req->senderRef = reference();
  req->backupPtr = ptr.i;
  req->backupKey[0] = ptr.p->backupKey[0];
  req->backupKey[1] = ptr.p->backupKey[1];
  req->nodes = ptr.p->nodes;
  req->backupDataLen = ptr.p->backupDataLen;
  req->flags = ptr.p->flags;
  
  ptr.p->masterData.gsn = GSN_DEFINE_BACKUP_REQ;
  ptr.p->masterData.sendCounter = ptr.p->nodes;
  BlockNumber backupBlockNo = numberToBlock(BACKUP, instanceKey(ptr));
  NodeReceiverGroup rg(backupBlockNo, ptr.p->nodes);
  sendSignal(rg, GSN_DEFINE_BACKUP_REQ, signal, 
	     DefineBackupReq::SignalLength, JBB);
  
  /**
   * Now send backup data
   */
  const Uint32 len = ptr.p->backupDataLen;
  if(len == 0){
    /**
     * No data to send
     */
    jam();
    return;
  }//if
  
  /**
   * Not implemented
   */
  ndbrequire(0);
}

void
Backup::execDEFINE_BACKUP_REF(Signal* signal)
{
  jamEntry();

  DefineBackupRef* ref = (DefineBackupRef*)signal->getDataPtr();
  
  const Uint32 ptrI = ref->backupPtr;
  //const Uint32 backupId = ref->backupId;
  const Uint32 nodeId = ref->nodeId;
  
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);
  
  ptr.p->setErrorCode(ref->errorCode);
  defineBackupReply(signal, ptr, nodeId);
}

void
Backup::execDEFINE_BACKUP_CONF(Signal* signal)
{
  jamEntry();

  DefineBackupConf* conf = (DefineBackupConf*)signal->getDataPtr();
  const Uint32 ptrI = conf->backupPtr;
  //const Uint32 backupId = conf->backupId;
  const Uint32 nodeId = refToNode(signal->senderBlockRef());

  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);

  if (ERROR_INSERTED(10024))
  {
    ptr.p->setErrorCode(324);
  }

  defineBackupReply(signal, ptr, nodeId);
}

void
Backup::defineBackupReply(Signal* signal, BackupRecordPtr ptr, Uint32 nodeId)
{
  if (!haveAllSignals(ptr, GSN_DEFINE_BACKUP_REQ, nodeId)) {
    jam();
    return;
  }

  /**
   * Unlock mutexes
   */
  jam();
  Mutex mutex1(signal, c_mutexMgr, ptr.p->masterData.m_dictCommitTableMutex);
  jam();
  mutex1.unlock(); // ignore response

  jam();
  Mutex mutex2(signal, c_mutexMgr, ptr.p->masterData.m_defineBackupMutex);
  jam();
  mutex2.unlock(); // ignore response

  if(ptr.p->checkError())
  {
    jam();
    masterAbort(signal, ptr);
    return;
  }
  
  CRASH_INSERTION((10034));

  /**
   * We've received GSN_DEFINE_BACKUP_CONF from all participants.
   *
   * Our next step is to send START_BACKUP_REQ to all participants,
   * who will then send CREATE_TRIG_REQ for all tables to their local
   * DBTUP.
   */
  TablePtr tabPtr;
  ptr.p->tables.first(tabPtr);

  sendStartBackup(signal, ptr, tabPtr);
}

/*****************************************************************************
 * 
 * Master functionallity - Prepare triggers
 *
 *****************************************************************************/
void
Backup::createAttributeMask(TablePtr tabPtr, 
			    Bitmask<MAXNROFATTRIBUTESINWORDS> & mask)
{
  mask.clear();
  for (Uint32 i = 0; i<tabPtr.p->noOfAttributes; i++)
    mask.set(i);
}

void
Backup::sendCreateTrig(Signal* signal, 
			   BackupRecordPtr ptr, TablePtr tabPtr)
{
  CreateTrigImplReq* req = (CreateTrigImplReq*)signal->getDataPtr();

  /*
   * First, setup the structures
   */
  for(Uint32 j=0; j<3; j++) {
    jam();

    TriggerPtr trigPtr;
    if (!ptr.p->triggers.seizeFirst(trigPtr)) {
      jam();
      ptr.p->m_gsn = GSN_START_BACKUP_REF;
      StartBackupRef* ref = (StartBackupRef*)signal->getDataPtrSend();
      ref->backupPtr = ptr.i;
      ref->backupId = ptr.p->backupId;
      ref->errorCode = StartBackupRef::FailedToAllocateTriggerRecord;
      ref->nodeId = getOwnNodeId();
      sendSignal(ptr.p->masterRef, GSN_START_BACKUP_REF, signal,
		 StartBackupRef::SignalLength, JBB);
      return;
    } // if

    const Uint32 triggerId= trigPtr.i;
    tabPtr.p->triggerIds[j] = triggerId;
    tabPtr.p->triggerAllocated[j] = true;
    trigPtr.p->backupPtr = ptr.i;
    trigPtr.p->tableId = tabPtr.p->tableId;
    trigPtr.p->tab_ptr_i = tabPtr.i;
    trigPtr.p->logEntry = 0;
    trigPtr.p->event = j;
    trigPtr.p->maxRecordSize = 4096;
    trigPtr.p->operation =
      &ptr.p->files.getPtr(ptr.p->logFilePtr)->operation;
    trigPtr.p->operation->noOfBytes = 0;
    trigPtr.p->operation->noOfRecords = 0;
    trigPtr.p->errorCode = 0;
  } // for

  /*
   * now ask DBTUP to create
   */
  ptr.p->slaveData.gsn = GSN_CREATE_TRIG_IMPL_REQ;
  ptr.p->slaveData.trigSendCounter = 3;
  ptr.p->slaveData.createTrig.tableId = tabPtr.p->tableId;

  req->senderRef = reference();
  req->receiverRef = reference();
  req->senderData = ptr.i;
  req->requestType = 0;

  Bitmask<MAXNROFATTRIBUTESINWORDS> attrMask;
  createAttributeMask(tabPtr, attrMask);

  req->tableId = tabPtr.p->tableId;
  req->tableVersion = 0;
  req->indexId = RNIL;
  req->indexVersion = 0;

  Uint32 ti = 0;
  /*
   * We always send PK for any operations and any triggertypes.
   * For SUBSCRIPTION_BEFORE
   *   We send after image for INSERT.
   *   We send before image for DELETE.
   *   We send before+after image for UPDATE.
   * For SUBSCRIPTION
   *   We send after image for INSERT.
   *   We send only PK for DELETE.
   *   We send after image for UPDATE.
   */
  if(ptr.p->flags & BackupReq::USE_UNDO_LOG)
    TriggerInfo::setTriggerType(ti, TriggerType::SUBSCRIPTION_BEFORE);
  else
    TriggerInfo::setTriggerType(ti, TriggerType::SUBSCRIPTION);
  TriggerInfo::setTriggerActionTime(ti, TriggerActionTime::TA_DETACHED);
  TriggerInfo::setMonitorReplicas(ti, true);
  TriggerInfo::setMonitorAllAttributes(ti, false);

  for (int i=0; i < 3; i++) {
    req->triggerId = tabPtr.p->triggerIds[i];

    Uint32 ti2 = ti;
    TriggerInfo::setTriggerEvent(ti2, triggerEventValues[i]);
    req->triggerInfo = ti2;

    LinearSectionPtr ptr[3];
    ptr[0].p = attrMask.rep.data;
    ptr[0].sz = attrMask.getSizeInWords();

    sendSignal(DBTUP_REF, GSN_CREATE_TRIG_IMPL_REQ,
	       signal, CreateTrigImplReq::SignalLength, JBB, ptr ,1);
  }
}

void
Backup::execCREATE_TRIG_IMPL_CONF(Signal* signal)
{
  jamEntry();
  const CreateTrigImplConf* conf =
    (const CreateTrigImplConf*)signal->getDataPtr();
  
  const Uint32 ptrI = conf->senderData;
  const Uint32 tableId = conf->tableId;
  const TriggerEvent::Value type =
    TriggerInfo::getTriggerEvent(conf->triggerInfo);

  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);

  /**
   * Verify that I'm waiting for this conf
   *
   * ptr.p->masterRef != reference()
   * as slaves and masters have triggers now.
   */
  ndbrequire(ptr.p->slaveData.gsn == GSN_CREATE_TRIG_IMPL_REQ);
  ndbrequire(ptr.p->slaveData.trigSendCounter.done() == false);
  ndbrequire(ptr.p->slaveData.createTrig.tableId == tableId);

  TablePtr tabPtr;
  ndbrequire(findTable(ptr, tabPtr, tableId));
  ndbrequire(type < 3); // if some decides to change the enums

  createTrigReply(signal, ptr);
}

void
Backup::execCREATE_TRIG_IMPL_REF(Signal* signal)
{
  jamEntry();
  const CreateTrigImplRef* ref =
    (const CreateTrigImplRef*)signal->getDataPtr();

  const Uint32 ptrI = ref->senderData;
  const Uint32 tableId = ref->tableId;

  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);

  /**
   * Verify that I'm waiting for this ref
   *
   * ptr.p->masterRef != reference()
   * as slaves and masters have triggers now
   */
  ndbrequire(ptr.p->slaveData.gsn == GSN_CREATE_TRIG_IMPL_REQ);
  ndbrequire(ptr.p->slaveData.trigSendCounter.done() == false);
  ndbrequire(ptr.p->slaveData.createTrig.tableId == tableId);

  ptr.p->setErrorCode(ref->errorCode);

  createTrigReply(signal, ptr);
}

void
Backup::createTrigReply(Signal* signal, BackupRecordPtr ptr)
{
  CRASH_INSERTION(10003);

  /**
   * Check finished with table
   */
  ptr.p->slaveData.trigSendCounter--;
  if(ptr.p->slaveData.trigSendCounter.done() == false){
    jam();
    return;
  }//if

  if (ERROR_INSERTED(10025))
  {
    ptr.p->errorCode = 325;
  }

  if(ptr.p->checkError()) {
    jam();
    ptr.p->m_gsn = GSN_START_BACKUP_REF;
    StartBackupRef* ref = (StartBackupRef*)signal->getDataPtrSend();
    ref->backupPtr = ptr.i;
    ref->backupId = ptr.p->backupId;
    ref->errorCode = ptr.p->errorCode;
    ref->nodeId = getOwnNodeId();
    ndbout_c("Backup::createTrigReply : CREATE_TRIG_IMPL error %d, backup id %u node %d",
             ref->errorCode, ref->backupId, ref->nodeId);
    sendSignal(ptr.p->masterRef, GSN_START_BACKUP_REF, signal,
               StartBackupRef::SignalLength, JBB);
    return;
  }//if

  TablePtr tabPtr;
  ndbrequire(findTable(ptr, tabPtr, ptr.p->slaveData.createTrig.tableId));

  /**
   * Next table
   */
  ptr.p->tables.next(tabPtr);
  if(tabPtr.i != RNIL){
    jam();
    sendCreateTrig(signal, ptr, tabPtr);
    return;
  }//if

  /**
   * We've finished creating triggers.
   *
   * send conf and wait
   */
  ptr.p->m_gsn = GSN_START_BACKUP_CONF;
  StartBackupConf* conf = (StartBackupConf*)signal->getDataPtrSend();
  conf->backupPtr = ptr.i;
  conf->backupId = ptr.p->backupId;
  sendSignal(ptr.p->masterRef, GSN_START_BACKUP_CONF, signal,
	     StartBackupConf::SignalLength, JBB);
}

/*****************************************************************************
 * 
 * Master functionallity - Start backup
 *
 *****************************************************************************/
void
Backup::sendStartBackup(Signal* signal, BackupRecordPtr ptr, TablePtr tabPtr)
{

  ptr.p->masterData.startBackup.tablePtr = tabPtr.i;

  StartBackupReq* req = (StartBackupReq*)signal->getDataPtrSend();
  req->backupId = ptr.p->backupId;
  req->backupPtr = ptr.i;

  /**
   * We use trigger Ids that are unique to BACKUP.
   * These don't interfere with other triggers (e.g. from DBDICT)
   * as there is a special case in DBTUP.
   *
   * Consequently, backups during online upgrade won't work
   */
  ptr.p->masterData.gsn = GSN_START_BACKUP_REQ;
  ptr.p->masterData.sendCounter = ptr.p->nodes;
  BlockNumber backupBlockNo = numberToBlock(BACKUP, instanceKey(ptr));
  NodeReceiverGroup rg(backupBlockNo, ptr.p->nodes);
  sendSignal(rg, GSN_START_BACKUP_REQ, signal,
	     StartBackupReq::SignalLength, JBB);
}

void
Backup::execSTART_BACKUP_REF(Signal* signal)
{
  jamEntry();

  StartBackupRef* ref = (StartBackupRef*)signal->getDataPtr();
  const Uint32 ptrI = ref->backupPtr;
  //const Uint32 backupId = ref->backupId;
  const Uint32 nodeId = ref->nodeId;

  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);

  ptr.p->setErrorCode(ref->errorCode);
  startBackupReply(signal, ptr, nodeId);
}

void
Backup::execSTART_BACKUP_CONF(Signal* signal)
{
  jamEntry();
  
  StartBackupConf* conf = (StartBackupConf*)signal->getDataPtr();
  const Uint32 ptrI = conf->backupPtr;
  //const Uint32 backupId = conf->backupId;
  const Uint32 nodeId = refToNode(signal->senderBlockRef());
  
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);

  startBackupReply(signal, ptr, nodeId);
}

void
Backup::startBackupReply(Signal* signal, BackupRecordPtr ptr, Uint32 nodeId)
{

  CRASH_INSERTION((10004));

  if (!haveAllSignals(ptr, GSN_START_BACKUP_REQ, nodeId)) {
    jam();
    return;
  }

  if (ERROR_INSERTED(10026))
  {
    ptr.p->errorCode = 326;
  }

  if(ptr.p->checkError()){
    jam();
    masterAbort(signal, ptr);
    return;
  }

  /* 
   * We reply to client after create trigger
   */
  if (SEND_BACKUP_STARTED_FLAG(ptr.p->flags))
  {
    BackupConf * conf = (BackupConf*)signal->getDataPtrSend();
    conf->backupId = ptr.p->backupId;
    conf->senderData = ptr.p->clientData;
    conf->nodes = ptr.p->nodes;
    sendSignal(ptr.p->clientRef, GSN_BACKUP_CONF, signal,
             BackupConf::SignalLength, JBB);
  }

  signal->theData[0] = NDB_LE_BackupStarted;
  signal->theData[1] = ptr.p->clientRef;
  signal->theData[2] = ptr.p->backupId;
  ptr.p->nodes.copyto(NdbNodeBitmask::Size, signal->theData+3);
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 3+NdbNodeBitmask::Size, JBB);

  /**
   * Wait for GCP
   */
  ptr.p->masterData.gsn = GSN_WAIT_GCP_REQ;
  ptr.p->masterData.waitGCP.startBackup = true;

  WaitGCPReq * waitGCPReq = (WaitGCPReq*)signal->getDataPtrSend();
  waitGCPReq->senderRef = reference();
  waitGCPReq->senderData = ptr.i;
  waitGCPReq->requestType = WaitGCPReq::CompleteForceStart;
  //we delay 10 seconds for testcases to generate events to be recorded in the UNDO log
  if (ERROR_INSERTED(10041))
  {
    sendSignalWithDelay(DBDIH_REF, GSN_WAIT_GCP_REQ, signal, 10*1000, WaitGCPReq::SignalLength);
  }
  else
    sendSignal(DBDIH_REF, GSN_WAIT_GCP_REQ, signal,
    	       WaitGCPReq::SignalLength,JBB);
}

void
Backup::execWAIT_GCP_REF(Signal* signal)
{
  jamEntry();
  
  CRASH_INSERTION((10006));

  WaitGCPRef * ref = (WaitGCPRef*)signal->getDataPtr();
  const Uint32 ptrI = ref->senderData;
  
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);

  ndbrequire(ptr.p->masterRef == reference());
  ndbrequire(ptr.p->masterData.gsn == GSN_WAIT_GCP_REQ);

  WaitGCPReq * req = (WaitGCPReq*)signal->getDataPtrSend();
  req->senderRef = reference();
  req->senderData = ptr.i;
  req->requestType = WaitGCPReq::CompleteForceStart;
  sendSignal(DBDIH_REF, GSN_WAIT_GCP_REQ, signal, 
	     WaitGCPReq::SignalLength,JBB);
}

void
Backup::execWAIT_GCP_CONF(Signal* signal){
  jamEntry();

  CRASH_INSERTION((10007));

  WaitGCPConf * conf = (WaitGCPConf*)signal->getDataPtr();
  const Uint32 ptrI = conf->senderData;
  const Uint32 gcp = conf->gci_hi;
  
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);
  
  ndbrequire(ptr.p->masterRef == reference());
  ndbrequire(ptr.p->masterData.gsn == GSN_WAIT_GCP_REQ);
  
  if(ptr.p->checkError()) {
    jam();
    masterAbort(signal, ptr);
    return;
  }//if
  
  if(ptr.p->masterData.waitGCP.startBackup) {
    jam();
    CRASH_INSERTION((10008));
    ptr.p->startGCP = gcp;
    ptr.p->masterData.sendCounter= 0;
    ptr.p->masterData.gsn = GSN_BACKUP_FRAGMENT_REQ;
    nextFragment(signal, ptr);
    return;
  } else {
    jam();
    if(gcp >= ptr.p->startGCP + 3)
    {
      CRASH_INSERTION((10009));
      ptr.p->stopGCP = gcp;
      /**
       * Backup is complete - begin cleanup
       * STOP_BACKUP_REQ is sent to participants.
       * They then drop the local triggers
       */
      sendStopBackup(signal, ptr);
      return;
    }//if
    
    /**
     * Make sure that we got entire stopGCP 
     */
    WaitGCPReq * req = (WaitGCPReq*)signal->getDataPtrSend();
    req->senderRef = reference();
    req->senderData = ptr.i;
    req->requestType = WaitGCPReq::CompleteForceStart;
    sendSignal(DBDIH_REF, GSN_WAIT_GCP_REQ, signal, 
	       WaitGCPReq::SignalLength,JBB);
    return;
  }
}

/*****************************************************************************
 * 
 * Master functionallity - Backup fragment
 *
 *****************************************************************************/
void
Backup::nextFragment(Signal* signal, BackupRecordPtr ptr)
{
  jam();

  BackupFragmentReq* req = (BackupFragmentReq*)signal->getDataPtrSend();
  req->backupPtr = ptr.i;
  req->backupId = ptr.p->backupId;

  NdbNodeBitmask nodes = ptr.p->nodes;
  Uint32 idleNodes = nodes.count();
  Uint32 saveIdleNodes = idleNodes;
  ndbrequire(idleNodes > 0);

  TablePtr tabPtr;
  ptr.p->tables.first(tabPtr);
  for(; tabPtr.i != RNIL && idleNodes > 0; ptr.p->tables.next(tabPtr))
  {
    jam();
    FragmentPtr fragPtr;
    Array<Fragment> & frags = tabPtr.p->fragments;
    const Uint32 fragCount = frags.getSize();
    
    for(Uint32 i = 0; i<fragCount && idleNodes > 0; i++)
    {
      jam();
      tabPtr.p->fragments.getPtr(fragPtr, i);
      const Uint32 nodeId = fragPtr.p->node;
      if(fragPtr.p->scanning != 0) {
        jam();
	ndbrequire(nodes.get(nodeId));
	nodes.clear(nodeId);
	idleNodes--;
      } else if(fragPtr.p->scanned == 0 && nodes.get(nodeId)){
	jam();
	fragPtr.p->scanning = 1;
	nodes.clear(nodeId);
	idleNodes--;
	
	req->tableId = tabPtr.p->tableId;
	req->fragmentNo = i;
	req->count = 0;

	ptr.p->masterData.sendCounter++;
	BlockReference ref = numberToRef(BACKUP, instanceKey(ptr), nodeId);
	sendSignal(ref, GSN_BACKUP_FRAGMENT_REQ, signal,
		   BackupFragmentReq::SignalLength, JBB);
      }//if
    }//for
  }//for
  
  if(idleNodes != saveIdleNodes){
    jam();
    return;
  }//if

  /**
   * Finished with all tables
   */
  {
    ptr.p->masterData.gsn = GSN_WAIT_GCP_REQ;
    ptr.p->masterData.waitGCP.startBackup = false;
    
    WaitGCPReq * req = (WaitGCPReq*)signal->getDataPtrSend();
    req->senderRef = reference();
    req->senderData = ptr.i;
    req->requestType = WaitGCPReq::CompleteForceStart;
    sendSignal(DBDIH_REF, GSN_WAIT_GCP_REQ, signal, 
	       WaitGCPReq::SignalLength, JBB);
  }
}

void
Backup::execBACKUP_FRAGMENT_CONF(Signal* signal)
{
  jamEntry();

  CRASH_INSERTION((10010));
  
  BackupFragmentConf * conf = (BackupFragmentConf*)signal->getDataPtr();
  const Uint32 ptrI = conf->backupPtr;
  //const Uint32 backupId = conf->backupId;
  const Uint32 tableId = conf->tableId;
  const Uint32 fragmentNo = conf->fragmentNo;
  const Uint32 nodeId = refToNode(signal->senderBlockRef());
  const Uint64 noOfBytes =
    conf->noOfBytesLow + (((Uint64)conf->noOfBytesHigh) << 32);
  const Uint64 noOfRecords =
    conf->noOfRecordsLow + (((Uint64)conf->noOfRecordsHigh) << 32);

  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);

  ptr.p->noOfBytes += noOfBytes;
  ptr.p->noOfRecords += noOfRecords;
  ptr.p->masterData.sendCounter--;

  TablePtr tabPtr;
  ndbrequire(findTable(ptr, tabPtr, tableId));

  tabPtr.p->noOfRecords += noOfRecords;

  FragmentPtr fragPtr;
  tabPtr.p->fragments.getPtr(fragPtr, fragmentNo);

  fragPtr.p->noOfRecords = noOfRecords;

  ndbrequire(fragPtr.p->scanned == 0);
  ndbrequire(fragPtr.p->scanning == 1);
  ndbrequire(fragPtr.p->node == nodeId);

  fragPtr.p->scanned = 1;
  fragPtr.p->scanning = 0;

  if (ERROR_INSERTED(10028)) 
  {
    ptr.p->errorCode = 328;
  }

  if(ptr.p->checkError()) 
  {
    jam();
    if(ptr.p->masterData.sendCounter.done())
    {
      jam();
      masterAbort(signal, ptr);
      return;
    }//if
  }
  else
  {
    jam();
    NdbNodeBitmask nodes = ptr.p->nodes;
    nodes.clear(getOwnNodeId());
    if (!nodes.isclear())
    {
      jam();
      BackupFragmentCompleteRep *rep =
        (BackupFragmentCompleteRep*)signal->getDataPtrSend();
      rep->backupId = ptr.p->backupId;
      rep->backupPtr = ptr.i;
      rep->tableId = tableId;
      rep->fragmentNo = fragmentNo;
      rep->noOfTableRowsLow = (Uint32)(tabPtr.p->noOfRecords & 0xFFFFFFFF);
      rep->noOfTableRowsHigh = (Uint32)(tabPtr.p->noOfRecords >> 32);
      rep->noOfFragmentRowsLow = (Uint32)(noOfRecords & 0xFFFFFFFF);
      rep->noOfFragmentRowsHigh = (Uint32)(noOfRecords >> 32);
      BlockNumber backupBlockNo = numberToBlock(BACKUP, instanceKey(ptr));
      NodeReceiverGroup rg(backupBlockNo, ptr.p->nodes);
      sendSignal(rg, GSN_BACKUP_FRAGMENT_COMPLETE_REP, signal,
                 BackupFragmentCompleteRep::SignalLength, JBA);
    }
    nextFragment(signal, ptr);
  }
}

void
Backup::execBACKUP_FRAGMENT_REF(Signal* signal)
{
  jamEntry();

  CRASH_INSERTION((10011));

  BackupFragmentRef * ref = (BackupFragmentRef*)signal->getDataPtr();
  const Uint32 ptrI = ref->backupPtr;
  //const Uint32 backupId = ref->backupId;
  const Uint32 nodeId = ref->nodeId;
  
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);

  TablePtr tabPtr;
  ptr.p->tables.first(tabPtr);
  for(; tabPtr.i != RNIL; ptr.p->tables.next(tabPtr)) {
    jam();
    FragmentPtr fragPtr;
    Array<Fragment> & frags = tabPtr.p->fragments;
    const Uint32 fragCount = frags.getSize();
    
    for(Uint32 i = 0; i<fragCount; i++) {
      jam();
      tabPtr.p->fragments.getPtr(fragPtr, i);
        if(fragPtr.p->scanning != 0 && nodeId == fragPtr.p->node) 
      {
        jam();
	ndbrequire(fragPtr.p->scanned == 0);
	fragPtr.p->scanned = 1;
	fragPtr.p->scanning = 0;
	goto done;
      }
    }
  }
  goto err;

done:
  ptr.p->masterData.sendCounter--;
  ptr.p->setErrorCode(ref->errorCode);
  
  if(ptr.p->masterData.sendCounter.done())
  {
    jam();
    masterAbort(signal, ptr);
    return;
  }//if

err:
  AbortBackupOrd *ord = (AbortBackupOrd*)signal->getDataPtrSend();
  ord->backupId = ptr.p->backupId;
  ord->backupPtr = ptr.i;
  ord->requestType = AbortBackupOrd::LogBufferFull;
  ord->senderData= ptr.i;
  execABORT_BACKUP_ORD(signal);
}

void
Backup::execBACKUP_FRAGMENT_COMPLETE_REP(Signal* signal)
{
  jamEntry();
  BackupFragmentCompleteRep * rep =
    (BackupFragmentCompleteRep*)signal->getDataPtr();

  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, rep->backupPtr);

  TablePtr tabPtr;
  ndbrequire(findTable(ptr, tabPtr, rep->tableId));

  tabPtr.p->noOfRecords =
    rep->noOfTableRowsLow + (((Uint64)rep->noOfTableRowsHigh) << 32);

  FragmentPtr fragPtr;
  tabPtr.p->fragments.getPtr(fragPtr, rep->fragmentNo);

  fragPtr.p->noOfRecords =
    rep->noOfFragmentRowsLow + (((Uint64)rep->noOfFragmentRowsHigh) << 32);
}

/*****************************************************************************
 *
 * Slave functionallity - Drop triggers
 *
 *****************************************************************************/

void
Backup::sendDropTrig(Signal* signal, BackupRecordPtr ptr)
{
  TablePtr tabPtr;
  ptr.p->slaveData.gsn = GSN_DROP_TRIG_IMPL_REQ;

  if (ptr.p->slaveData.dropTrig.tableId == RNIL) {
    jam();
    if(ptr.p->tables.getCount())
      ptr.p->tables.first(tabPtr);
    else
    {
      // Early abort, go to close files
      jam();
      closeFiles(signal, ptr);
      return;
    }
  } else {
    jam();
    ndbrequire(findTable(ptr, tabPtr, ptr.p->slaveData.dropTrig.tableId));
    ptr.p->tables.next(tabPtr);
  }//if
  if (tabPtr.i != RNIL) {
    jam();
    sendDropTrig(signal, ptr, tabPtr);
  } else {
    /**
     * Insert footers
     */
    //if backup error, we needn't insert footers
    if(ptr.p->checkError())
    {
      jam();
      closeFiles(signal, ptr);
      ptr.p->errorCode = 0;
      return;
    }

    {
      BackupFilePtr filePtr;
      ptr.p->files.getPtr(filePtr, ptr.p->logFilePtr);
      Uint32 * dst;
      ndbrequire(filePtr.p->operation.dataBuffer.getWritePtr(&dst, 1));
      * dst = 0;
      filePtr.p->operation.dataBuffer.updateWritePtr(1);
    }

    {
      BackupFilePtr filePtr;
      ptr.p->files.getPtr(filePtr, ptr.p->ctlFilePtr);

      const Uint32 gcpSz = sizeof(BackupFormat::CtlFile::GCPEntry) >> 2;

      Uint32 * dst;
      ndbrequire(filePtr.p->operation.dataBuffer.getWritePtr(&dst, gcpSz));

      BackupFormat::CtlFile::GCPEntry * gcp = 
	(BackupFormat::CtlFile::GCPEntry*)dst;

      gcp->SectionType   = htonl(BackupFormat::GCP_ENTRY);
      gcp->SectionLength = htonl(gcpSz);
      gcp->StartGCP      = htonl(ptr.p->startGCP);
      gcp->StopGCP       = htonl(ptr.p->stopGCP - 1);
      filePtr.p->operation.dataBuffer.updateWritePtr(gcpSz);

      {
        TablePtr tabPtr;
        if (ptr.p->tables.first(tabPtr))
	{
	  jam();
	  signal->theData[0] = BackupContinueB::BACKUP_FRAGMENT_INFO;
	  signal->theData[1] = ptr.i;
	  signal->theData[2] = tabPtr.i;
	  signal->theData[3] = 0;
	  sendSignal(reference(), GSN_CONTINUEB, signal, 4, JBB);
	}
	else
	{
	  jam();
	  closeFiles(signal, ptr);
	}
      }
    }
  }
}

void
Backup::sendDropTrig(Signal* signal, BackupRecordPtr ptr, TablePtr tabPtr)
{
  jam();
  DropTrigImplReq* req = (DropTrigImplReq*)signal->getDataPtrSend();

  ptr.p->slaveData.gsn = GSN_DROP_TRIG_IMPL_REQ;
  ptr.p->slaveData.trigSendCounter = 0;
  req->senderRef = reference(); // Sending to myself
  req->senderData = ptr.i;
  req->requestType = 0;
  req->tableId = tabPtr.p->tableId;
  req->tableVersion = 0;
  req->indexId = RNIL;
  req->indexVersion = 0;
  req->receiverRef = reference();

  // TUP needs some triggerInfo to find right list
  Uint32 ti = 0;
  if(ptr.p->flags & BackupReq::USE_UNDO_LOG)
    TriggerInfo::setTriggerType(ti, TriggerType::SUBSCRIPTION_BEFORE);
  else
    TriggerInfo::setTriggerType(ti, TriggerType::SUBSCRIPTION);
  TriggerInfo::setTriggerActionTime(ti, TriggerActionTime::TA_DETACHED);
  TriggerInfo::setMonitorReplicas(ti, true);
  TriggerInfo::setMonitorAllAttributes(ti, false);

  ptr.p->slaveData.dropTrig.tableId = tabPtr.p->tableId;
  req->tableId = tabPtr.p->tableId;

  for (int i = 0; i < 3; i++) {
    Uint32 id = tabPtr.p->triggerIds[i];
    req->triggerId = id;

    Uint32 ti2 = ti;
    TriggerInfo::setTriggerEvent(ti2, triggerEventValues[i]);
    req->triggerInfo = ti2;

    sendSignal(DBTUP_REF, GSN_DROP_TRIG_IMPL_REQ,
	       signal, DropTrigImplReq::SignalLength, JBB);
    ptr.p->slaveData.trigSendCounter ++;
  }
}

void
Backup::execDROP_TRIG_IMPL_REF(Signal* signal)
{
  jamEntry();

  const DropTrigImplRef* ref = (const DropTrigImplRef*)signal->getDataPtr();
  const Uint32 ptrI = ref->senderData;

  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);

  if(ref->triggerId != ~(Uint32) 0)
  {
    ndbout << "ERROR DROPPING TRIGGER: " << ref->triggerId;
    ndbout << " Err: " << ref->errorCode << endl << endl;
  }

  dropTrigReply(signal, ptr);
}

void
Backup::execDROP_TRIG_IMPL_CONF(Signal* signal)
{
  jamEntry();
  
  const DropTrigImplConf* conf = (const DropTrigImplConf*)signal->getDataPtr();
  const Uint32 ptrI = conf->senderData;

  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);

  dropTrigReply(signal, ptr);
}

void
Backup::dropTrigReply(Signal* signal, BackupRecordPtr ptr)
{
  CRASH_INSERTION((10012));

  ndbrequire(ptr.p->slaveData.gsn == GSN_DROP_TRIG_IMPL_REQ);
  ndbrequire(ptr.p->slaveData.trigSendCounter.done() == false);

  // move from .masterData to .slaveData
  ptr.p->slaveData.trigSendCounter--;
  if(ptr.p->slaveData.trigSendCounter.done() == false){
    jam();
    return;
  }//if

  sendDropTrig(signal, ptr); // recursive next
}

/*****************************************************************************
 * 
 * Master functionallity - Stop backup
 *
 *****************************************************************************/
void
Backup::execSTOP_BACKUP_REF(Signal* signal)
{
  jamEntry();

  StopBackupRef* ref = (StopBackupRef*)signal->getDataPtr();
  const Uint32 ptrI = ref->backupPtr;
  //const Uint32 backupId = ref->backupId;
  const Uint32 nodeId = ref->nodeId;
  
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);

  ptr.p->setErrorCode(ref->errorCode);
  stopBackupReply(signal, ptr, nodeId);
}

void
Backup::sendStopBackup(Signal* signal, BackupRecordPtr ptr)
{
  jam();

  StopBackupReq* stop = (StopBackupReq*)signal->getDataPtrSend();
  stop->backupPtr = ptr.i;
  stop->backupId = ptr.p->backupId;
  stop->startGCP = ptr.p->startGCP;
  stop->stopGCP = ptr.p->stopGCP;

  ptr.p->masterData.gsn = GSN_STOP_BACKUP_REQ;
  ptr.p->masterData.sendCounter = ptr.p->nodes;
  BlockNumber backupBlockNo = numberToBlock(BACKUP, instanceKey(ptr));
  NodeReceiverGroup rg(backupBlockNo, ptr.p->nodes);
  sendSignal(rg, GSN_STOP_BACKUP_REQ, signal, 
	     StopBackupReq::SignalLength, JBB);
}

void
Backup::execSTOP_BACKUP_CONF(Signal* signal)
{
  jamEntry();
  
  StopBackupConf* conf = (StopBackupConf*)signal->getDataPtr();
  const Uint32 ptrI = conf->backupPtr;
  //const Uint32 backupId = conf->backupId;
  const Uint32 nodeId = refToNode(signal->senderBlockRef());
  
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);

  ptr.p->noOfLogBytes += conf->noOfLogBytes;
  ptr.p->noOfLogRecords += conf->noOfLogRecords;
  
  stopBackupReply(signal, ptr, nodeId);
}

void
Backup::stopBackupReply(Signal* signal, BackupRecordPtr ptr, Uint32 nodeId)
{
  CRASH_INSERTION((10013));

  if (!haveAllSignals(ptr, GSN_STOP_BACKUP_REQ, nodeId)) {
    jam();
    return;
  }

  sendAbortBackupOrd(signal, ptr, AbortBackupOrd::BackupComplete);
  
  if(!ptr.p->checkError() &&  ptr.p->masterData.errorCode == 0)
  {
    if (SEND_BACKUP_COMPLETED_FLAG(ptr.p->flags))
    {
      BackupCompleteRep * rep = (BackupCompleteRep*)signal->getDataPtrSend();
      rep->backupId = ptr.p->backupId;
      rep->senderData = ptr.p->clientData;
      rep->startGCP = ptr.p->startGCP;
      rep->stopGCP = ptr.p->stopGCP;
      rep->noOfBytesLow = (Uint32)(ptr.p->noOfBytes & 0xFFFFFFFF);
      rep->noOfRecordsLow = (Uint32)(ptr.p->noOfRecords & 0xFFFFFFFF);
      rep->noOfBytesHigh = (Uint32)(ptr.p->noOfBytes >> 32);
      rep->noOfRecordsHigh = (Uint32)(ptr.p->noOfRecords >> 32);
      rep->noOfLogBytes = Uint32(ptr.p->noOfLogBytes); // TODO 64-bit log-bytes
      rep->noOfLogRecords = Uint32(ptr.p->noOfLogRecords); // TODO ^^
      rep->nodes = ptr.p->nodes;
      sendSignal(ptr.p->clientRef, GSN_BACKUP_COMPLETE_REP, signal,
		 BackupCompleteRep::SignalLength, JBB);
    }

    signal->theData[0] = NDB_LE_BackupCompleted;
    signal->theData[1] = ptr.p->clientRef;
    signal->theData[2] = ptr.p->backupId;
    signal->theData[3] = ptr.p->startGCP;
    signal->theData[4] = ptr.p->stopGCP;
    signal->theData[5] = (Uint32)(ptr.p->noOfBytes & 0xFFFFFFFF);
    signal->theData[6] = (Uint32)(ptr.p->noOfRecords & 0xFFFFFFFF);
    signal->theData[7] = (Uint32)(ptr.p->noOfLogBytes & 0xFFFFFFFF);
    signal->theData[8] = (Uint32)(ptr.p->noOfLogRecords & 0xFFFFFFFF);
    ptr.p->nodes.copyto(NdbNodeBitmask::Size, signal->theData+9);
    signal->theData[9+NdbNodeBitmask::Size] = (Uint32)(ptr.p->noOfBytes >> 32);
    signal->theData[10+NdbNodeBitmask::Size] = (Uint32)(ptr.p->noOfRecords >> 32);
    signal->theData[11+NdbNodeBitmask::Size] = (Uint32)(ptr.p->noOfLogBytes >> 32);
    signal->theData[12+NdbNodeBitmask::Size] = (Uint32)(ptr.p->noOfLogRecords >> 32);
    sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 13+NdbNodeBitmask::Size, JBB);
  }
  else
  {
    masterAbort(signal, ptr);
  }
}

void
Backup::initReportStatus(Signal *signal, BackupRecordPtr ptr)
{
  ptr.p->m_prev_report = NdbTick_getCurrentTicks();
}

void
Backup::checkReportStatus(Signal *signal, BackupRecordPtr ptr)
{
  if (m_backup_report_frequency == 0)
    return;

  const NDB_TICKS now = NdbTick_getCurrentTicks();
  const Uint64 elapsed = NdbTick_Elapsed(ptr.p->m_prev_report, now).seconds();
  if (elapsed > m_backup_report_frequency)
  {
    reportStatus(signal, ptr);
    ptr.p->m_prev_report = now;
  }
}

void
Backup::reportStatus(Signal* signal, BackupRecordPtr ptr,
                     BlockReference ref)
{
  const int signal_length = 11;

  signal->theData[0] = NDB_LE_BackupStatus;
  for (int i= 1; i < signal_length; i++)
    signal->theData[i] = 0;

  if (ptr.i == RNIL ||
      (ptr.p->m_gsn == 0 &&
       ptr.p->masterData.gsn == 0))
  {
    sendSignal(ref, GSN_EVENT_REP, signal, signal_length, JBB);
    return;
  }
  signal->theData[1] = ptr.p->clientRef;
  signal->theData[2] = ptr.p->backupId;

  if (ptr.p->dataFilePtr[0] == RNIL)
  {
    sendSignal(ref, GSN_EVENT_REP, signal, signal_length, JBB);
    return;
  }

  BackupFilePtr dataFilePtr;
  ptr.p->files.getPtr(dataFilePtr, ptr.p->dataFilePtr[0]);
  signal->theData[3] = (Uint32)(dataFilePtr.p->operation.m_bytes_total & 0xFFFFFFFF);
  signal->theData[4] = (Uint32)(dataFilePtr.p->operation.m_bytes_total >> 32);
  signal->theData[5] = (Uint32)(dataFilePtr.p->operation.m_records_total & 0xFFFFFFFF);
  signal->theData[6] = (Uint32)(dataFilePtr.p->operation.m_records_total >> 32);
 
  if (ptr.p->logFilePtr == RNIL)
  {
    sendSignal(ref, GSN_EVENT_REP, signal, signal_length, JBB);
    return;
  }

  BackupFilePtr logFilePtr;
  ptr.p->files.getPtr(logFilePtr, ptr.p->logFilePtr);
  signal->theData[7] = (Uint32)(logFilePtr.p->operation.m_bytes_total & 0xFFFFFFFF);
  signal->theData[8] = (Uint32)(logFilePtr.p->operation.m_bytes_total >> 32);
  signal->theData[9] = (Uint32)(logFilePtr.p->operation.m_records_total & 0xFFFFFFFF);
  signal->theData[10]= (Uint32)(logFilePtr.p->operation.m_records_total >> 32);

  sendSignal(ref, GSN_EVENT_REP, signal, signal_length, JBB);
}

/*****************************************************************************
 * 
 * Master functionallity - Abort backup
 *
 *****************************************************************************/
void
Backup::masterAbort(Signal* signal, BackupRecordPtr ptr)
{
  jam();
#ifdef DEBUG_ABORT
  ndbout_c("************ masterAbort");
#endif

  ndbassert(ptr.p->masterRef == reference());

  if(ptr.p->masterData.errorCode != 0)
  {
    jam();
    return;
  }

  if (SEND_BACKUP_STARTED_FLAG(ptr.p->flags))
  {
    BackupAbortRep* rep = (BackupAbortRep*)signal->getDataPtrSend();
    rep->backupId = ptr.p->backupId;
    rep->senderData = ptr.p->clientData;
    rep->reason = ptr.p->errorCode;
    sendSignal(ptr.p->clientRef, GSN_BACKUP_ABORT_REP, signal, 
	       BackupAbortRep::SignalLength, JBB);
  }
  signal->theData[0] = NDB_LE_BackupAborted;
  signal->theData[1] = ptr.p->clientRef;
  signal->theData[2] = ptr.p->backupId;
  signal->theData[3] = ptr.p->errorCode;
  sendSignal(CMVMI_REF, GSN_EVENT_REP, signal, 4, JBB);

  ndbrequire(ptr.p->errorCode);
  ptr.p->masterData.errorCode = ptr.p->errorCode;

  AbortBackupOrd *ord = (AbortBackupOrd*)signal->getDataPtrSend();
  ord->backupId = ptr.p->backupId;
  ord->backupPtr = ptr.i;
  ord->senderData= ptr.i;
  BlockNumber backupBlockNo = numberToBlock(BACKUP, instanceKey(ptr));
  NodeReceiverGroup rg(backupBlockNo, ptr.p->nodes);
  
  switch(ptr.p->masterData.gsn){
  case GSN_DEFINE_BACKUP_REQ:
    ord->requestType = AbortBackupOrd::BackupFailure;
    sendSignal(rg, GSN_ABORT_BACKUP_ORD, signal, 
	       AbortBackupOrd::SignalLength, JBB);
    return;
  case GSN_CREATE_TRIG_IMPL_REQ:
  case GSN_START_BACKUP_REQ:
  case GSN_ALTER_TRIG_REQ:
  case GSN_WAIT_GCP_REQ:
  case GSN_BACKUP_FRAGMENT_REQ:
    jam();
    ptr.p->stopGCP= ptr.p->startGCP + 1;
    sendStopBackup(signal, ptr); // dropping due to error
    return;
  case GSN_UTIL_SEQUENCE_REQ:
  case GSN_UTIL_LOCK_REQ:
    ndbrequire(false);
    return;
  case GSN_DROP_TRIG_IMPL_REQ:
  case GSN_STOP_BACKUP_REQ:
    return;
  }
}

void
Backup::abort_scan(Signal * signal, BackupRecordPtr ptr)
{
  AbortBackupOrd *ord = (AbortBackupOrd*)signal->getDataPtrSend();
  ord->backupId = ptr.p->backupId;
  ord->backupPtr = ptr.i;
  ord->senderData= ptr.i;
  ord->requestType = AbortBackupOrd::AbortScan;

  TablePtr tabPtr;
  ptr.p->tables.first(tabPtr);
  for(; tabPtr.i != RNIL; ptr.p->tables.next(tabPtr)) {
    jam();
    FragmentPtr fragPtr;
    Array<Fragment> & frags = tabPtr.p->fragments;
    const Uint32 fragCount = frags.getSize();
    
    for(Uint32 i = 0; i<fragCount; i++) {
      jam();
      tabPtr.p->fragments.getPtr(fragPtr, i);
      const Uint32 nodeId = fragPtr.p->node;
      if(fragPtr.p->scanning != 0 && ptr.p->nodes.get(nodeId)) {
        jam();
	
	BlockReference ref = numberToRef(BACKUP, instanceKey(ptr), nodeId);
	sendSignal(ref, GSN_ABORT_BACKUP_ORD, signal,
		   AbortBackupOrd::SignalLength, JBB);
	
      }
    }
  }
}

/*****************************************************************************
 * 
 * Slave functionallity: Define Backup 
 *
 *****************************************************************************/
void
Backup::defineBackupRef(Signal* signal, BackupRecordPtr ptr, Uint32 errCode)
{
  jam();
  if(ptr.p->is_lcp()) 
  {
    jam();
    ptr.p->setPrepareErrorCode(errCode);
    ptr.p->prepareState = PREPARE_ABORTING;
    ndbrequire(ptr.p->ctlFilePtr != RNIL);

    /**
     * This normally happens when a table has been deleted before we got to
     * start the LCP. This is a normal behaviour.
     *
     * At this point we have both the data file and the control file to use
     * open. At this point it is ok to remove both of them since they will
     * no longer be needed. This will happen in closeFile since we have set
     * the error code here.
     */
    BackupFilePtr filePtr;
    ptr.p->files.getPtr(filePtr, ptr.p->prepareDataFilePtr[0]);
    if (filePtr.p->m_flags & BackupFile::BF_OPEN &&
        !(filePtr.p->m_flags & BackupFile::BF_CLOSING))
    {
      jam();
      ndbrequire(! (filePtr.p->m_flags & BackupFile::BF_FILE_THREAD));
      filePtr.p->m_flags &= ~(Uint32)BackupFile::BF_LCP_META;
      closeFile(signal, ptr, filePtr, true);
    }
    else if (filePtr.p->m_flags & BackupFile::BF_CLOSING)
    {
      /* Wait for the data file closing */
      jam();
      return;
    }
    else
    {
      jam();
      ndbrequire(filePtr.p->m_flags == 0);
    }
    ptr.p->files.getPtr(filePtr,
          ptr.p->prepareCtlFilePtr[ptr.p->prepareNextLcpCtlFileNumber]);
    if (filePtr.p->m_flags & BackupFile::BF_OPEN &&
        !(filePtr.p->m_flags & BackupFile::BF_CLOSING))
    {
      jam();
      closeFile(signal, ptr, filePtr, true);
      return;
    }
    else if (filePtr.p->m_flags & BackupFile::BF_CLOSING)
    {
      /* Wait for the control file to close as well. */
      jam();
      return;
    }
    else
    {
      jam();
      ndbrequire(filePtr.p->m_flags == 0);
    }
    
    TablePtr tabPtr;
    FragmentPtr fragPtr;
    
    ndbrequire(ptr.p->prepare_table.first(tabPtr));
    tabPtr.p->fragments.getPtr(fragPtr, 0);
    DEB_LCP(("(%u)LCP_PREPARE_REF", instance()));
    LcpPrepareRef* ref= (LcpPrepareRef*)signal->getDataPtrSend();
    ref->senderData = ptr.p->clientData;
    ref->senderRef = reference();
    ref->tableId = tabPtr.p->tableId;
    ref->fragmentId = fragPtr.p->fragmentId;
    ref->errorCode = ptr.p->prepareErrorCode;
    sendSignal(ptr.p->masterRef, GSN_LCP_PREPARE_REF, 
	       signal, LcpPrepareRef::SignalLength, JBA);
    ptr.p->prepareState = NOT_ACTIVE;
    return;
  }
  ptr.p->setErrorCode(errCode);

  ptr.p->m_gsn = GSN_DEFINE_BACKUP_REF;
  ndbrequire(ptr.p->errorCode != 0);
  
  DefineBackupRef* ref = (DefineBackupRef*)signal->getDataPtrSend();
  ref->backupId = ptr.p->backupId;
  ref->backupPtr = ptr.i;
  ref->errorCode = ptr.p->errorCode;
  ref->nodeId = getOwnNodeId();
  sendSignal(ptr.p->masterRef, GSN_DEFINE_BACKUP_REF, signal, 
	     DefineBackupRef::SignalLength, JBB);
}

void
Backup::init_file(BackupFilePtr filePtr, Uint32 backupPtrI)
{
  filePtr.p->tableId = RNIL;
  filePtr.p->backupPtr = backupPtrI;
  filePtr.p->filePointer = RNIL;
  filePtr.p->m_flags = 0;
  filePtr.p->errorCode = 0;
  filePtr.p->m_sent_words_in_scan_batch = 0;
  filePtr.p->m_num_scan_req_on_prioa = 0;
}

void
Backup::execDEFINE_BACKUP_REQ(Signal* signal)
{
  jamEntry();

  DefineBackupReq* req = (DefineBackupReq*)signal->getDataPtr();
  
  BackupRecordPtr ptr;
  const Uint32 ptrI = req->backupPtr;
  const Uint32 backupId = req->backupId;
  const BlockReference senderRef = req->senderRef;

  if(senderRef == reference()){
    /**
     * Signal sent from myself -> record already seized
     */
    jam();
    c_backupPool.getPtr(ptr, ptrI);
  } else { // from other node
    jam();
#ifdef DEBUG_ABORT
    dumpUsedResources();
#endif
    if (!c_backups.getPool().seizeId(ptr, ptrI)) {
      jam();
      ndbrequire(false); // If master has succeeded slave should succed
    }//if
    c_backups.addFirst(ptr);
  }//if

  CRASH_INSERTION((10014));
  
  ptr.p->m_gsn = GSN_DEFINE_BACKUP_REQ;
  ptr.p->slaveState.forceState(INITIAL);
  ptr.p->slaveState.setState(DEFINING);
  ptr.p->prepareState = NOT_ACTIVE;
  ptr.p->slaveData.dropTrig.tableId = RNIL;
  ptr.p->errorCode = 0;
  ptr.p->clientRef = req->clientRef;
  ptr.p->clientData = req->clientData;
  if(senderRef == reference())
    ptr.p->flags = req->flags;
  else
    ptr.p->flags = req->flags & ~((Uint32)BackupReq::WAITCOMPLETED); /* remove waitCompleted flags
						 * as non master should never
						 * reply
						 */
  ptr.p->masterRef = senderRef;
  ptr.p->nodes = req->nodes;
  ptr.p->backupId = backupId;
  ptr.p->backupKey[0] = req->backupKey[0];
  ptr.p->backupKey[1] = req->backupKey[1];
  ptr.p->backupDataLen = req->backupDataLen;
  ptr.p->masterData.errorCode = 0;
  ptr.p->noOfBytes = 0;
  ptr.p->noOfRecords = 0;
  ptr.p->noOfLogBytes = 0;
  ptr.p->noOfLogRecords = 0;
  ptr.p->currGCP = 0;
  ptr.p->startGCP = 0;
  ptr.p->stopGCP = 0;
  ptr.p->m_prioA_scan_batches_to_execute = 0;
  ptr.p->m_lastSignalId = 0;

  /**
   * Allocate files
   */
  BackupFilePtr files[4 + (2*BackupFormat::NDB_MAX_FILES_PER_LCP)];
  Uint32 noOfPages[] = {
    NO_OF_PAGES_META_FILE,
    2,   // 32k
    0    // 3M
  };
  const Uint32 maxInsert[] = {
    MAX_WORDS_META_FILE,
    4096,    // 16k
    BACKUP_MIN_BUFF_WORDS
  };
  Uint32 minWrite[] = {
    8192,
    8192,
    32768
  };
  Uint32 maxWrite[] = {
    8192,
    8192,
    32768
  };
  
  minWrite[1] = c_defaults.m_minWriteSize;
  maxWrite[1] = c_defaults.m_maxWriteSize;
  noOfPages[1] = (c_defaults.m_logBufferSize + sizeof(Page32) - 1) / 
    sizeof(Page32);
  minWrite[2] = c_defaults.m_minWriteSize;
  maxWrite[2] = c_defaults.m_maxWriteSize;
  noOfPages[2] = (c_defaults.m_dataBufferSize + sizeof(Page32) - 1) / 
    sizeof(Page32);

  ptr.p->ctlFilePtr = ptr.p->logFilePtr = RNIL;
  for (Uint32 i = 0; i < BackupFormat::NDB_MAX_FILES_PER_LCP; i++)
  {
    ptr.p->dataFilePtr[i] = RNIL;
    ptr.p->prepareDataFilePtr[i] = RNIL;
  }

  if (ptr.p->is_lcp())
  {
    /**
     * Allocate table and fragment object LCP prepare and execute
     * phase once and for all. This means we don't risk getting out
     * of resource issues for LCPs.
     */
    jam();
    TablePtr tabPtr;
    m_lcp_ptr_i = ptr.i;
    ndbrequire(ptr.p->prepare_table.seizeLast(tabPtr));
    ndbrequire(tabPtr.p->fragments.seize(1));
    ndbrequire(ptr.p->tables.seizeLast(tabPtr));
    ndbrequire(tabPtr.p->fragments.seize(1));

    noOfPages[2] = (c_defaults.m_lcp_buffer_size + sizeof(Page32) - 1) / 
      sizeof(Page32);
    for (Uint32 i = 0; i < (4 + (2*BackupFormat::NDB_MAX_FILES_PER_LCP)); i++)
    {
      Uint32 minWriteLcp;
      Uint32 maxWriteLcp;
      Uint32 maxInsertLcp;
      Uint32 noOfPagesLcp;
      ndbrequire(ptr.p->files.seizeFirst(files[i]));
      init_file(files[i], ptr.i);
      switch (i)
      {
        case 0:
        {
          jam();
          minWriteLcp = 1024;
          maxWriteLcp = 32768;
          maxInsertLcp = 8192;
          noOfPagesLcp = 2;
          ptr.p->ctlFilePtr = files[i].i;
          files[i].p->fileType = BackupFormat::CTL_FILE;
          break;
        }
        case 1:
        {
          jam();
          minWriteLcp = 1024;
          maxWriteLcp = 32768;
          maxInsertLcp = 8192;
          noOfPagesLcp = 2;
          ptr.p->prepareCtlFilePtr[0] = files[i].i;
          files[i].p->fileType = BackupFormat::CTL_FILE;
          break;
        }
        case 2:
        {
          jam();
          minWriteLcp = 1024;
          maxWriteLcp = 32768;
          maxInsertLcp = 8192;
          noOfPagesLcp = 2;
          ptr.p->prepareCtlFilePtr[1] = files[i].i;
          files[i].p->fileType = BackupFormat::CTL_FILE;
          break;
        }
        case 3:
        {
          jam();
          minWriteLcp = 1024;
          maxWriteLcp = 32768;
          maxInsertLcp = 8192;
          noOfPagesLcp = 2;
          ptr.p->deleteFilePtr = files[i].i;
          files[i].p->fileType = BackupFormat::DATA_FILE;
          break;
        }
        default:
        {
          if (i < 4 + BackupFormat::NDB_MAX_FILES_PER_LCP)
          {
            jam();
            minWriteLcp = minWrite[2];
            maxWriteLcp = maxWrite[2];
            maxInsertLcp = maxInsert[2];
            noOfPagesLcp = noOfPages[2];
            jam();
            ptr.p->prepareDataFilePtr[i - 4] = files[i].i;
            jam();
            files[i].p->fileType = BackupFormat::DATA_FILE;
            jam();
          }
          else
          {
            jam();
            minWriteLcp = minWrite[2];
            maxWriteLcp = maxWrite[2];
            maxInsertLcp = maxInsert[2];
            noOfPagesLcp = noOfPages[2];
            jam();
            ptr.p->dataFilePtr[i - (4 + BackupFormat::NDB_MAX_FILES_PER_LCP)] =
              files[i].i;
            jam();
            files[i].p->fileType = BackupFormat::DATA_FILE;
            jam();
          }
          break;
        }
      }
      Page32Ptr pagePtr;
      DEB_LCP(("LCP: instance: %u, i: %u, seize %u pages",
               instance(),
               i,
               noOfPagesLcp));
      ndbrequire(files[i].p->pages.seize(noOfPagesLcp));
      files[i].p->pages.getPtr(pagePtr, 0);
      const char * msg = files[i].p->
        operation.dataBuffer.setup((Uint32*)pagePtr.p, 
                                   noOfPagesLcp * (sizeof(Page32) >> 2),
                                   128,
                                   minWriteLcp >> 2,
                                   maxWriteLcp >> 2,
                                   maxInsertLcp);
      if (msg != 0)
      {
        ndbout_c("setup msg = %s, i = %u", msg, i);
        ndbrequire(false);
      }
      files[i].p->operation.m_bytes_total = 0;
      files[i].p->operation.m_records_total = 0;
    }
  }
  else
  {
    for (Uint32 i = 0; i < 3; i++)
    {
      jam();
      if (!ptr.p->files.seizeFirst(files[i]))
      {
        jam();
        defineBackupRef(signal, ptr, 
                        DefineBackupRef::FailedToAllocateFileRecord);
        return;
      }//if
      init_file(files[i], ptr.i);

      if(ERROR_INSERTED(10035) || files[i].p->pages.seize(noOfPages[i]) == false)
      {
        jam();
        DEBUG_OUT("Failed to seize " << noOfPages[i] << " pages");
        defineBackupRef(signal, ptr, DefineBackupRef::FailedToAllocateBuffers);
        return;
      }//if

      Page32Ptr pagePtr;
      files[i].p->pages.getPtr(pagePtr, 0);
    
      const char * msg = files[i].p->
        operation.dataBuffer.setup((Uint32*)pagePtr.p, 
                                   noOfPages[i] * (sizeof(Page32) >> 2),
                                   128,
                                   minWrite[i] >> 2,
                                   maxWrite[i] >> 2,
                                   maxInsert[i]);
      if(msg != 0) {
        jam();
        defineBackupRef(signal, ptr, DefineBackupRef::FailedToSetupFsBuffers);
        return;
      }//if

      switch(i)
      {
        case 0:
          files[i].p->fileType = BackupFormat::CTL_FILE;
          ptr.p->ctlFilePtr = files[i].i;
          break;
        case 1:
          if(ptr.p->flags & BackupReq::USE_UNDO_LOG)
            files[i].p->fileType = BackupFormat::UNDO_FILE;
          else
            files[i].p->fileType = BackupFormat::LOG_FILE;
          ptr.p->logFilePtr = files[i].i;
          break;
        case 2:
          files[i].p->fileType = BackupFormat::DATA_FILE;
          ptr.p->dataFilePtr[0] = files[i].i;
      }
      files[i].p->operation.m_bytes_total = 0;
      files[i].p->operation.m_records_total = 0;
    }//for
  }

  initReportStatus(signal, ptr);

  if (!verifyNodesAlive(ptr, ptr.p->nodes)) {
    jam();
    defineBackupRef(signal, ptr, DefineBackupRef::Undefined);
    return;
  }//if
  if (ERROR_INSERTED(10027)) {
    jam();
    defineBackupRef(signal, ptr, 327);
    return;
  }//if

  if(ptr.p->is_lcp())
  {
    jam();
    getFragmentInfoDone(signal, ptr);
    return;
  }
  
  if (ptr.p->backupDataLen == 0)
  {
    jam();
    backupAllData(signal, ptr);
    return;
  }//if
  
  /**
   * Not implemented
   */
  ndbrequire(0);
}

void
Backup::backupAllData(Signal* signal, BackupRecordPtr ptr)
{
  /**
   * Get all tables from dict
   */
  ListTablesReq * req = (ListTablesReq*)signal->getDataPtrSend();
  req->init();
  req->senderRef = reference();
  req->senderData = ptr.i;
  req->setTableId(0);
  req->setTableType(0);
  sendSignal(DBDICT_REF, GSN_LIST_TABLES_REQ, signal, 
	     ListTablesReq::SignalLength, JBB);
}

void
Backup::execLIST_TABLES_CONF(Signal* signal)
{
  jamEntry();
  Uint32 fragInfo = signal->header.m_fragmentInfo;
  ListTablesConf* conf = (ListTablesConf*)signal->getDataPtr();
  Uint32 noOfTables = conf->noOfTables;

  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, conf->senderData);

  SectionHandle handle (this, signal);
  signal->header.m_fragmentInfo = 0;
  if (noOfTables > 0)
  {
    ListTablesData ltd;
    const Uint32 listTablesDataSizeInWords = (sizeof(ListTablesData) + 3) / 4;
    SegmentedSectionPtr tableDataPtr;
    handle.getSection(tableDataPtr, ListTablesConf::TABLE_DATA);
    SimplePropertiesSectionReader
      tableDataReader(tableDataPtr, getSectionSegmentPool());

    tableDataReader.reset();
    for(unsigned int i = 0; i<noOfTables; i++) {
      jam();
      tableDataReader.getWords((Uint32 *)&ltd, listTablesDataSizeInWords);
      Uint32 tableId = ltd.getTableId();
      Uint32 tableType = ltd.getTableType();
      Uint32 state= ltd.getTableState();
      jamLine(tableId);

      if (! (DictTabInfo::isTable(tableType) ||
             DictTabInfo::isIndex(tableType) ||
             DictTabInfo::isFilegroup(tableType) ||
             DictTabInfo::isFile(tableType)
             || DictTabInfo::isHashMap(tableType)
             || DictTabInfo::isForeignKey(tableType)
             ))
      {
        jam();
        continue;
      }

      if (state != DictTabInfo::StateOnline)
      {
        jam();
        continue;
      }

      TablePtr tabPtr;
      ptr.p->tables.seizeLast(tabPtr);
      if(tabPtr.i == RNIL) {
        jam();
        defineBackupRef(signal, ptr, DefineBackupRef::FailedToAllocateTables);
        releaseSections(handle);
        return;
      }//if
      tabPtr.p->tableType = tableType;
      tabPtr.p->tableId = tableId;
#ifdef VM_TRACE
      TablePtr locTabPtr;
      ndbassert(findTable(ptr, locTabPtr, tabPtr.p->tableId) == false);
#endif
      insertTableMap(tabPtr, ptr.i, tabPtr.p->tableId);
    }//for
  }

  releaseSections(handle);

  /*
    If first or not last signal
    then keep accumulating table data
   */
  if ((fragInfo == 1) || (fragInfo == 2))
  {
    jam();
    return;
  }
  openFiles(signal, ptr);
}

void
Backup::openFiles(Signal* signal, BackupRecordPtr ptr)
{
  jam();

  BackupFilePtr filePtr;

  FsOpenReq * req = (FsOpenReq *)signal->getDataPtrSend();
  req->userReference = reference();
  req->fileFlags = 
    FsOpenReq::OM_WRITEONLY | 
    FsOpenReq::OM_CREATE_IF_NONE |
    FsOpenReq::OM_APPEND |
    FsOpenReq::OM_AUTOSYNC;

  if (c_defaults.m_compressed_backup)
    req->fileFlags |= FsOpenReq::OM_GZ;

  FsOpenReq::v2_setCount(req->fileNumber, 0xFFFFFFFF);
  req->auto_sync_size = c_defaults.m_disk_synch_size;
  /**
   * Ctl file
   */
  c_backupFilePool.getPtr(filePtr, ptr.p->ctlFilePtr);
  filePtr.p->m_flags |= BackupFile::BF_OPENING;

  req->userPointer = filePtr.i;
  FsOpenReq::setVersion(req->fileNumber, 2);
  FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_CTL);
  FsOpenReq::v2_setSequence(req->fileNumber, ptr.p->backupId);
  FsOpenReq::v2_setNodeId(req->fileNumber, getOwnNodeId());
  sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, FsOpenReq::SignalLength, JBA);

  /**
   * Log file
   */
  c_backupFilePool.getPtr(filePtr, ptr.p->logFilePtr);
  filePtr.p->m_flags |= BackupFile::BF_OPENING;
  
  //write uncompressed log file when enable undo log,since log file is read from back to front.
  if(ptr.p->flags & BackupReq::USE_UNDO_LOG)
    req->fileFlags &= ~FsOpenReq::OM_GZ;
 
  req->userPointer = filePtr.i;
  FsOpenReq::setVersion(req->fileNumber, 2);
  FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_LOG);
  FsOpenReq::v2_setSequence(req->fileNumber, ptr.p->backupId);
  FsOpenReq::v2_setNodeId(req->fileNumber, getOwnNodeId());
  sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, FsOpenReq::SignalLength, JBA);

  /**
   * Data file
   */
  c_backupFilePool.getPtr(filePtr, ptr.p->dataFilePtr[0]);
  filePtr.p->m_flags |= BackupFile::BF_OPENING;

  if (c_defaults.m_o_direct)
    req->fileFlags |= FsOpenReq::OM_DIRECT;
  if (c_defaults.m_compressed_backup)
    req->fileFlags |= FsOpenReq::OM_GZ;
  req->userPointer = filePtr.i;
  FsOpenReq::setVersion(req->fileNumber, 2);
  FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_DATA);
  FsOpenReq::v2_setSequence(req->fileNumber, ptr.p->backupId);
  FsOpenReq::v2_setNodeId(req->fileNumber, getOwnNodeId());
  FsOpenReq::v2_setCount(req->fileNumber, 0);
  sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, FsOpenReq::SignalLength, JBA);
}

void
Backup::execFSOPENREF(Signal* signal)
{
  jamEntry();

  FsRef * ref = (FsRef *)signal->getDataPtr();
  
  const Uint32 userPtr = ref->userPointer;
  
  BackupFilePtr filePtr;
  c_backupFilePool.getPtr(filePtr, userPtr);
  
  ndbrequire(! (filePtr.p->m_flags & BackupFile::BF_OPEN));
  ndbrequire(filePtr.p->m_flags & BackupFile::BF_OPENING);
  filePtr.p->m_flags &= ~(Uint32)BackupFile::BF_OPENING;

  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, filePtr.p->backupPtr);

  ptr.p->setErrorCode(ref->errorCode);
  if (ptr.p->is_lcp())
  {
    jam();
    openFilesReplyLCP(signal, ptr, filePtr);
    return;
  }
  openFilesReply(signal, ptr, filePtr);
}

void
Backup::execFSOPENCONF(Signal* signal)
{
  jamEntry();
  
  FsConf * conf = (FsConf *)signal->getDataPtr();
  
  const Uint32 userPtr = conf->userPointer;
  const Uint32 filePointer = conf->filePointer;
  
  BackupFilePtr filePtr;
  c_backupFilePool.getPtr(filePtr, userPtr);
  filePtr.p->filePointer = filePointer; 
  
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, filePtr.p->backupPtr);

  /**
   * Mark files as "opened"
   */
  ndbrequire(! (filePtr.p->m_flags & BackupFile::BF_OPEN));
  ndbrequire(filePtr.p->m_flags & BackupFile::BF_OPENING);
  filePtr.p->m_flags &= ~(Uint32)BackupFile::BF_OPENING;
  filePtr.p->m_flags |= BackupFile::BF_OPEN;

  if (ptr.p->is_lcp())
  {
    jam();
    openFilesReplyLCP(signal, ptr, filePtr);
    return;
  }
  openFilesReply(signal, ptr, filePtr);
}

void
Backup::openFilesReply(Signal* signal, 
		       BackupRecordPtr ptr, BackupFilePtr filePtr)
{
  jam();
  /**
   * Check if all files have recived open_reply
   */
  for(ptr.p->files.first(filePtr); filePtr.i!=RNIL;ptr.p->files.next(filePtr)) 
  {
    jam();
    if(filePtr.p->m_flags & BackupFile::BF_OPENING) {
      jam();
      return;
    }//if
  }//for

  if (ERROR_INSERTED(10037)) {
    jam();
    /**
     * Dont return FailedForBackupFilesAleadyExist
     * cause this will make NdbBackup auto-retry with higher number :-)
     */
    ptr.p->errorCode = DefineBackupRef::FailedInsertFileHeader;
    defineBackupRef(signal, ptr);
    return;
  }
  /**
   * Did open succeed for all files
   */
  if(ptr.p->checkError()) 
  {
    jam();
    if(ptr.p->errorCode == FsRef::fsErrFileExists)
    {
      jam();
      ptr.p->errorCode = DefineBackupRef::FailedForBackupFilesAleadyExist;
    }
    defineBackupRef(signal, ptr);
    return;
  }//if

  /**
   * Insert file headers
   */
  ptr.p->files.getPtr(filePtr, ptr.p->ctlFilePtr);
  if(!insertFileHeader(BackupFormat::CTL_FILE, ptr.p, filePtr.p)) {
    jam();
    defineBackupRef(signal, ptr, DefineBackupRef::FailedInsertFileHeader);
    return;
  }//if
    
  BackupFormat::FileType logfiletype;
  if(ptr.p->flags & BackupReq::USE_UNDO_LOG)
    logfiletype = BackupFormat::UNDO_FILE;
  else
    logfiletype = BackupFormat::LOG_FILE;

  ptr.p->files.getPtr(filePtr, ptr.p->logFilePtr);
  if(!insertFileHeader(logfiletype, ptr.p, filePtr.p)) {
    jam();
    defineBackupRef(signal, ptr, DefineBackupRef::FailedInsertFileHeader);
    return;
  }//if
  
  ptr.p->files.getPtr(filePtr, ptr.p->dataFilePtr[0]);
  if(!insertFileHeader(BackupFormat::DATA_FILE, ptr.p, filePtr.p)) {
    jam();
    defineBackupRef(signal, ptr, DefineBackupRef::FailedInsertFileHeader);
    return;
  }//if
  
  /**
   * Start CTL file thread
   */
  ptr.p->files.getPtr(filePtr, ptr.p->ctlFilePtr);
  filePtr.p->m_flags |= BackupFile::BF_FILE_THREAD;
   
  signal->theData[0] = BackupContinueB::START_FILE_THREAD;
  signal->theData[1] = filePtr.i;
  sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
  
  /**
   * Insert table list in ctl file
   */
  FsBuffer & buf = filePtr.p->operation.dataBuffer;
  
  const Uint32 sz = 
    (sizeof(BackupFormat::CtlFile::TableList) >> 2) +
    ptr.p->tables.getCount() - 1;
  
  Uint32 * dst;
  ndbrequire(sz < buf.getMaxWrite());
  if(!buf.getWritePtr(&dst, sz)) {
    jam();
    defineBackupRef(signal, ptr, DefineBackupRef::FailedInsertTableList);
    return;
  }//if
  
  BackupFormat::CtlFile::TableList* tl = 
    (BackupFormat::CtlFile::TableList*)dst;
  tl->SectionType   = htonl(BackupFormat::TABLE_LIST);
  tl->SectionLength = htonl(sz);

  TablePtr tabPtr;
  Uint32 count = 0;
  for(ptr.p->tables.first(tabPtr); 
      tabPtr.i != RNIL;
      ptr.p->tables.next(tabPtr)){
    jam();
    tl->TableIds[count] = htonl(tabPtr.p->tableId);
    count++;
  }//for
  
  buf.updateWritePtr(sz);
  
  /**
   * Start getting table definition data
   */
  ndbrequire(ptr.p->tables.first(tabPtr));
  
  signal->theData[0] = BackupContinueB::BUFFER_FULL_META;
  signal->theData[1] = ptr.i;
  signal->theData[2] = tabPtr.i;
  signal->theData[3] = 0;
  sendSignal(reference(), GSN_CONTINUEB, signal, 4, JBB);
  return;
}

bool
Backup::insertFileHeader(BackupFormat::FileType ft, 
			 BackupRecord * ptrP,
			 BackupFile * filePtrP){
  FsBuffer & buf = filePtrP->operation.dataBuffer;

  const Uint32 sz = sizeof(BackupFormat::FileHeader) >> 2;

  Uint32 * dst;
  ndbrequire(sz < buf.getMaxWrite());
  if(!buf.getWritePtr(&dst, sz)) {
    jam();
    return false;
  }//if
  
  BackupFormat::FileHeader* header = (BackupFormat::FileHeader*)dst;
  ndbrequire(sizeof(header->Magic) == sizeof(BACKUP_MAGIC));
  memcpy(header->Magic, BACKUP_MAGIC, sizeof(BACKUP_MAGIC));
  if (ft == BackupFormat::LCP_FILE)
  {
    jam();
    header->BackupVersion = htonl(NDBD_USE_PARTIAL_LCP_v2);
  }
  else
  {
    jam();
    header->BackupVersion = htonl(NDB_BACKUP_VERSION);
  }
  header->SectionType   = htonl(BackupFormat::FILE_HEADER);
  header->SectionLength = htonl(sz - 3);
  header->FileType      = htonl(ft);
  header->BackupId      = htonl(ptrP->backupId);
  header->BackupKey_0   = htonl(ptrP->backupKey[0]);
  header->BackupKey_1   = htonl(ptrP->backupKey[1]);
  header->ByteOrder     = 0x12345678;
  header->NdbVersion    = htonl(NDB_VERSION_D);
  header->MySQLVersion  = htonl(NDB_MYSQL_VERSION_D);
  
  buf.updateWritePtr(sz);
  return true;
}

void
Backup::execGET_TABINFOREF(Signal* signal)
{
  jamEntry();
  GetTabInfoRef * ref = (GetTabInfoRef*)signal->getDataPtr();
  BackupFilePtr filePtr;
  
  const Uint32 senderData = ref->senderData;
  BackupRecordPtr ptr;
  c_backupFilePool.getPtr(filePtr, senderData);
  c_backupPool.getPtr(ptr, filePtr.p->backupPtr);

  ndbrequire(filePtr.i == ptr.p->prepareDataFilePtr[0] ||
             !ptr.p->is_lcp());
  defineBackupRef(signal, ptr, ref->errorCode);
}

void
Backup::execGET_TABINFO_CONF(Signal* signal)
{
  jamEntry();

  if(!assembleFragments(signal)) {
    jam();
    return;
  }//if

  BackupFilePtr filePtr;
  GetTabInfoConf * const conf = (GetTabInfoConf*)signal->getDataPtr();
  //const Uint32 senderRef = info->senderRef;
  const Uint32 len = conf->totalLen;
  const Uint32 senderData = conf->senderData;
  const Uint32 tableType = conf->tableType;
  const Uint32 tableId = conf->tableId;

  BackupRecordPtr ptr;
  c_backupFilePool.getPtr(filePtr, senderData);
  c_backupPool.getPtr(ptr, filePtr.p->backupPtr);

  SectionHandle handle(this, signal);
  SegmentedSectionPtr dictTabInfoPtr;
  handle.getSection(dictTabInfoPtr, GetTabInfoConf::DICT_TAB_INFO);
  ndbrequire(dictTabInfoPtr.sz == len);

  TablePtr tabPtr ;
  if (ptr.p->is_lcp())
  {
    jam();
    ndbrequire(filePtr.i == ptr.p->prepareDataFilePtr[0])
    ptr.p->prepare_table.first(tabPtr);
    ndbrequire(tabPtr.p->tableId == tableId);
  }
  else
  {
    jam();
    ptr.p->files.getPtr(filePtr, ptr.p->ctlFilePtr);
    ndbrequire(findTable(ptr, tabPtr, tableId));
  }

  FsBuffer & buf = filePtr.p->operation.dataBuffer;
  Uint32* dst = 0;
  {
    /**
     * Write into ctl file for Backups
     *
     * We don't write TABLE_DESCRIPTION into data LCP files. It is not
     * used in the restore process, so it only uses up space on
     * disk for no purpose.
     *
     * An LCP file only has the following sections:
     * 1) File header section
     * 2) Fragment Header section
     * 3) LCP data section that contains records of type:
     *    - INSERT_TYPE (normal records in ALL parts)
     *    - WRITE_TYPE (normal records in CHANGE parts)
     *    - DELETE_BY_ROWID_TYPE (record deleted in CHANGE parts)
     *    - DELETE_BY_PAGEID_TYPE (all records in page deleted in CHANGE part)
     * 4) Fragment Footer section
     *
     * We still need to copy the table description into a linear array,
     * we solve this by using the FsBuffer also for LCPs. We skip the
     * call to updateWritePtr. This means that we write into the
     * buffer, but the next time we write into the buffer we will
     * overwrite this area.
     */
    Uint32 dstLen = len + 3;
    if(!buf.getWritePtr(&dst, dstLen)) {
      jam();
      ndbrequire(false);
      ptr.p->setErrorCode(DefineBackupRef::FailedAllocateTableMem);
      releaseSections(handle);
      defineBackupRef(signal, ptr);
      return;
    }//if
    if(dst != 0) {
      jam();

      BackupFormat::CtlFile::TableDescription * desc = 
        (BackupFormat::CtlFile::TableDescription*)dst;
      desc->SectionType = htonl(BackupFormat::TABLE_DESCRIPTION);
      desc->SectionLength = htonl(len + 3);
      desc->TableType = htonl(tableType);
      dst += 3;
      
      copy(dst, dictTabInfoPtr);
      if (!ptr.p->is_lcp())
      {
        jam();
        buf.updateWritePtr(dstLen);
      }
    }//if
  }

  releaseSections(handle);

  if(ptr.p->checkError())
  {
    jam();
    ndbrequire(!ptr.p->is_lcp());
    defineBackupRef(signal, ptr);
    return;
  }//if

  if (!DictTabInfo::isTable(tabPtr.p->tableType))
  {
    jam();
    ndbrequire(!ptr.p->is_lcp());
    TablePtr tmp = tabPtr;
    removeTableMap(tmp, ptr.i, tmp.p->tableId);
    ptr.p->tables.next(tabPtr);
    ptr.p->tables.release(tmp);
    jamLine(tmp.p->tableId);
    afterGetTabinfoLockTab(signal, ptr, tabPtr);
    return;
  }
  
  if (!parseTableDescription(signal, ptr, tabPtr, dst, len))
  {
    jam();
    ndbrequire(!ptr.p->is_lcp());
    defineBackupRef(signal, ptr);
    return;
  }
  
  if(!ptr.p->is_lcp())
  {
    jam();
    BackupLockTab *req = (BackupLockTab *)signal->getDataPtrSend();
    req->m_senderRef = reference();
    req->m_tableId = tabPtr.p->tableId;
    req->m_lock_unlock = BackupLockTab::LOCK_TABLE;
    req->m_backup_state = BackupLockTab::GET_TABINFO_CONF;
    req->m_backupRecordPtr_I = ptr.i;
    req->m_tablePtr_I = tabPtr.i;
    sendSignal(DBDICT_REF, GSN_BACKUP_LOCK_TAB_REQ, signal,
               BackupLockTab::SignalLength, JBB);
    if (ERROR_INSERTED(10038))
    {
      /* Test */
      AbortBackupOrd *ord = (AbortBackupOrd*)signal->getDataPtrSend();
      ord->backupId = ptr.p->backupId;
      ord->backupPtr = ptr.i;
      ord->requestType = AbortBackupOrd::ClientAbort;
      ord->senderData= ptr.p->clientData;
      sendSignal(ptr.p->masterRef, GSN_ABORT_BACKUP_ORD, signal, 
                 AbortBackupOrd::SignalLength, JBB);
    }
    return;
  }
  else
  {
    jam();
    ndbrequire(filePtr.i == ptr.p->prepareDataFilePtr[0]);
    lcp_open_data_file_done(signal,
                            ptr);
    return;
  }
}

void
Backup::afterGetTabinfoLockTab(Signal *signal,
                               BackupRecordPtr ptr, TablePtr tabPtr)
{
  if(tabPtr.i == RNIL) 
  {
    /**
     * Done with all tables...
     */
    jam();
    
    ndbrequire(ptr.p->tables.first(tabPtr));
    ndbrequire(!ptr.p->is_lcp());
    DihScanTabReq * req = (DihScanTabReq*)signal->getDataPtrSend();
    req->senderRef = reference();
    req->senderData = ptr.i;
    req->tableId = tabPtr.p->tableId;
    req->schemaTransId = 0;
    req->jamBufferPtr = jamBuffer();
    EXECUTE_DIRECT_MT(DBDIH, GSN_DIH_SCAN_TAB_REQ, signal,
               DihScanTabReq::SignalLength, 0);
    DihScanTabConf * conf = (DihScanTabConf*)signal->getDataPtr();
    ndbrequire(conf->senderData == 0);
    conf->senderData = ptr.i;
    execDIH_SCAN_TAB_CONF(signal);
    return;
  }//if

  /**
   * Fetch next table...
   */
  signal->theData[0] = BackupContinueB::BUFFER_FULL_META;
  signal->theData[1] = ptr.i;
  signal->theData[2] = tabPtr.i;
  signal->theData[3] = 0;
  sendSignal(reference(), GSN_CONTINUEB, signal, 4, JBB);
  return;
}

bool
Backup::parseTableDescription(Signal* signal, 
			      BackupRecordPtr ptr, 
			      TablePtr tabPtr, 
			      const Uint32 * tabdescptr,
			      Uint32 len)
{
  SimplePropertiesLinearReader it(tabdescptr, len);
  
  it.first();
  
  DictTabInfo::Table tmpTab; tmpTab.init();
  SimpleProperties::UnpackStatus stat;
  stat = SimpleProperties::unpack(it, &tmpTab, 
				  DictTabInfo::TableMapping, 
				  DictTabInfo::TableMappingSize, 
				  true, true);
  ndbrequire(stat == SimpleProperties::Break);

  bool lcp = ptr.p->is_lcp();

  ndbrequire(tabPtr.p->tableId == tmpTab.TableId);
  ndbrequire(lcp || (tabPtr.p->tableType == tmpTab.TableType));
  
  /**
   * LCP should not save disk attributes but only mem attributes
   */
  
  /**
   * Initialize table object
   */
  tabPtr.p->noOfRecords = 0;
  tabPtr.p->schemaVersion = tmpTab.TableVersion;
  tabPtr.p->triggerIds[0] = ILLEGAL_TRIGGER_ID;
  tabPtr.p->triggerIds[1] = ILLEGAL_TRIGGER_ID;
  tabPtr.p->triggerIds[2] = ILLEGAL_TRIGGER_ID;
  tabPtr.p->triggerAllocated[0] = false;
  tabPtr.p->triggerAllocated[1] = false;
  tabPtr.p->triggerAllocated[2] = false;

  tabPtr.p->noOfAttributes = tmpTab.NoOfAttributes;
  tabPtr.p->maxRecordSize = 1; // LEN word
  bzero(tabPtr.p->attrInfo, sizeof(tabPtr.p->attrInfo));

  if (lcp)
  {
    jam();
    AttributeHeader::init(tabPtr.p->attrInfo, AttributeHeader::READ_LCP, 0);
  }
  else
  {
    jam();
    AttributeHeader::init(tabPtr.p->attrInfo, AttributeHeader::READ_ALL,
                          tmpTab.NoOfAttributes);
  }

  Uint32 varsize = 0;
  Uint32 disk = 0;
  Uint32 null = 0;
  for(Uint32 i = 0; i<tmpTab.NoOfAttributes; i++) {
    jam();
    DictTabInfo::Attribute tmp; tmp.init();
    stat = SimpleProperties::unpack(it, &tmp, 
				    DictTabInfo::AttributeMapping, 
				    DictTabInfo::AttributeMappingSize,
				    true, true);
    
    ndbrequire(stat == SimpleProperties::Break);
    it.next(); // Move Past EndOfAttribute

    if(lcp && tmp.AttributeStorageType == NDB_STORAGETYPE_DISK)
    {
      disk++;
      continue;
    }

    if (tmp.AttributeArrayType != NDB_ARRAYTYPE_FIXED)
      varsize++;

    if (tmp.AttributeNullableFlag)
      null++;

    if (tmp.AttributeSize == 0)
    {
      tabPtr.p->maxRecordSize += (tmp.AttributeArraySize + 31) >> 5;
    }
    else
    {
      const Uint32 arr = tmp.AttributeArraySize;
      const Uint32 sz = 1 << tmp.AttributeSize;
      const Uint32 sz32 = (sz * arr + 31) >> 5;

      tabPtr.p->maxRecordSize += sz32;
    }
  }

  tabPtr.p->attrInfoLen = 1;

  if (lcp)
  {
    jam();
    c_lqh->handleLCPSurfacing(signal);
    Dbtup* tup = (Dbtup*)globalData.getBlock(DBTUP, instance());
    tabPtr.p->maxRecordSize = 1 + tup->get_max_lcp_record_size(tmpTab.TableId);
  }
  else
  {
    // mask
    tabPtr.p->maxRecordSize += 1 + ((tmpTab.NoOfAttributes + null + 31) >> 5);
    tabPtr.p->maxRecordSize += (2 * varsize + 3) / 4;
  }

  return true;
}

void
Backup::execDIH_SCAN_TAB_CONF(Signal* signal)
{
  jamEntry();
  DihScanTabConf * conf = (DihScanTabConf*)signal->getDataPtr();
  const Uint32 fragCount = conf->fragmentCount;
  const Uint32 tableId = conf->tableId;
  const Uint32 senderData = conf->senderData;
  const Uint32 scanCookie = conf->scanCookie;
  ndbrequire(conf->reorgFlag == 0); // no backup during table reorg

  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, senderData);

  TablePtr tabPtr;
  ndbrequire(findTable(ptr, tabPtr, tableId));
  
  tabPtr.p->m_scan_cookie = scanCookie;
  ndbrequire(tabPtr.p->fragments.seize(fragCount) != false);
  for(Uint32 i = 0; i<fragCount; i++) {
    jam();
    FragmentPtr fragPtr;
    tabPtr.p->fragments.getPtr(fragPtr, i);
    fragPtr.p->scanned = 0;
    fragPtr.p->scanning = 0;
    fragPtr.p->tableId = tableId;
    fragPtr.p->fragmentId = i;
    fragPtr.p->lqhInstanceKey = 0;
    fragPtr.p->node = 0;
  }//for
  
  /**
   * Next table
   */
  if(ptr.p->tables.next(tabPtr))
  {
    jam();
    DihScanTabReq * req = (DihScanTabReq*)signal->getDataPtrSend();
    req->senderRef = reference();
    req->senderData = ptr.i;
    req->tableId = tabPtr.p->tableId;
    req->schemaTransId = 0;
    req->jamBufferPtr = jamBuffer();
    EXECUTE_DIRECT_MT(DBDIH, GSN_DIH_SCAN_TAB_REQ, signal,
                      DihScanTabReq::SignalLength, 0);
    jamEntry();
    DihScanTabConf * conf = (DihScanTabConf*)signal->getDataPtr();
    ndbrequire(conf->senderData == 0);
    conf->senderData = ptr.i;
    /* conf is already set up properly to be sent as signal */
    /* Real-time break to ensure we don't run for too long in one signal. */
    sendSignal(reference(), GSN_DIH_SCAN_TAB_CONF, signal,
               DihScanTabConf::SignalLength, JBB);
    return;
  }//if
  
  ptr.p->tables.first(tabPtr);
  getFragmentInfo(signal, ptr, tabPtr, 0);
}

void
Backup::getFragmentInfo(Signal* signal, 
			BackupRecordPtr ptr, TablePtr tabPtr, Uint32 fragNo)
{
  Uint32 loopCount = 0;
  jam();
  
  for(; tabPtr.i != RNIL; ptr.p->tables.next(tabPtr)) {
    jam();
    const Uint32 fragCount = tabPtr.p->fragments.getSize();
    for(; fragNo < fragCount; fragNo ++) {
      jam();
      FragmentPtr fragPtr;
      tabPtr.p->fragments.getPtr(fragPtr, fragNo);
      
      if(fragPtr.p->scanned == 0 && fragPtr.p->scanning == 0) {
        jam();
        DiGetNodesReq * const req = (DiGetNodesReq *)&signal->theData[0];
        req->tableId = tabPtr.p->tableId;
        req->hashValue = fragNo;
        req->distr_key_indicator = ZTRUE;
        req->anyNode = 0;
        req->scan_indicator = ZTRUE;
        req->jamBufferPtr = jamBuffer();
        req->get_next_fragid_indicator = 0;
        EXECUTE_DIRECT_MT(DBDIH, GSN_DIGETNODESREQ, signal,
                          DiGetNodesReq::SignalLength, 0);
        jamEntry();
        DiGetNodesConf * conf = (DiGetNodesConf *)&signal->theData[0];
        Uint32 reqinfo = conf->reqinfo;
        Uint32 nodeId = conf->nodes[0];
        /* Require successful read of table fragmentation */
        ndbrequire(conf->zero == 0);
        Uint32 instanceKey = (reqinfo >> 24) & 127;
        fragPtr.p->lqhInstanceKey = instanceKey;
        fragPtr.p->node = nodeId;
        if (++loopCount >= DiGetNodesReq::MAX_DIGETNODESREQS ||
            ERROR_INSERTED(10046))
        {
          jam();
          if (ERROR_INSERTED(10046))
          {
            CLEAR_ERROR_INSERT_VALUE;
          }
          signal->theData[0] = BackupContinueB::ZGET_NEXT_FRAGMENT;
          signal->theData[1] = ptr.i;
          signal->theData[2] = tabPtr.p->tableId;
          signal->theData[3] = fragNo + 1;
          sendSignal(reference(), GSN_CONTINUEB, signal, 4, JBB);
          return;
        }
      }//if
    }//for

    DihScanTabCompleteRep*rep= (DihScanTabCompleteRep*)signal->getDataPtrSend();
    rep->tableId = tabPtr.p->tableId;
    rep->scanCookie = tabPtr.p->m_scan_cookie;
    rep->jamBufferPtr = jamBuffer();
    EXECUTE_DIRECT_MT(DBDIH, GSN_DIH_SCAN_TAB_COMPLETE_REP, signal,
                      DihScanTabCompleteRep::SignalLength, 0);

    fragNo = 0;
  }//for
  

  getFragmentInfoDone(signal, ptr);
}

void
Backup::getFragmentInfoDone(Signal* signal, BackupRecordPtr ptr)
{
  ptr.p->m_gsn = GSN_DEFINE_BACKUP_CONF;
  ptr.p->slaveState.setState(DEFINED);
  DefineBackupConf * conf = (DefineBackupConf*)signal->getDataPtrSend();
  conf->backupPtr = ptr.i;
  conf->backupId = ptr.p->backupId;
  sendSignal(ptr.p->masterRef, GSN_DEFINE_BACKUP_CONF, signal,
	     DefineBackupConf::SignalLength, JBB);
}


/*****************************************************************************
 * 
 * Slave functionallity: Start backup
 *
 *****************************************************************************/
void
Backup::execSTART_BACKUP_REQ(Signal* signal)
{
  jamEntry();

  CRASH_INSERTION((10015));

  StartBackupReq* req = (StartBackupReq*)signal->getDataPtr();
  const Uint32 ptrI = req->backupPtr;

  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);

  ptr.p->slaveState.setState(STARTED);
  ptr.p->m_gsn = GSN_START_BACKUP_REQ;

  /* At this point, we are effectively starting
   * bulk file writes for this backup, so lets
   * record the fact
   */
  ndbrequire(is_backup_worker());
  ndbassert(!Backup::g_is_backup_running);
  Backup::g_is_backup_running = true;

  /**
   * Start file threads...
   */
  BackupFilePtr filePtr;
  for(ptr.p->files.first(filePtr); filePtr.i!=RNIL;ptr.p->files.next(filePtr))
  {
    jam();
    if(! (filePtr.p->m_flags & BackupFile::BF_FILE_THREAD))
    {
      jam();
      filePtr.p->m_flags |= BackupFile::BF_FILE_THREAD;
      signal->theData[0] = BackupContinueB::START_FILE_THREAD;
      signal->theData[1] = filePtr.i;
      sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
    }//if
  }//for

  /**
   * Tell DBTUP to create triggers
   */
  TablePtr tabPtr;
  ndbrequire(ptr.p->tables.first(tabPtr));
  sendCreateTrig(signal, ptr, tabPtr);
}

/*****************************************************************************
 * 
 * Slave functionallity: Backup fragment
 *
 *****************************************************************************/
void
Backup::execBACKUP_FRAGMENT_REQ(Signal* signal)
{
  jamEntry();
  BackupFragmentReq* req = (BackupFragmentReq*)signal->getDataPtr();

  CRASH_INSERTION((10016));

  const Uint32 ptrI = req->backupPtr;
  //const Uint32 backupId = req->backupId;
  const Uint32 tableId = req->tableId;
  const Uint32 fragNo = req->fragmentNo;
  const Uint32 count = req->count;

  /**
   * Get backup record
   */
  BackupRecordPtr ptr;
  BackupFilePtr filePtr;
  TablePtr tabPtr;

  c_backupPool.getPtr(ptr, ptrI);

  if (ptr.p->is_lcp())
  {
    jam();
    start_execute_lcp(signal, ptr, tabPtr, tableId);
    if (ptr.p->m_empty_lcp)
    {
      /**
       * No need to start LCP processing in this case, we only
       * update LCP control file and this process has already
       * been started when we come here.
       */
      jam();
    }
    else
    {
      jam();
      start_lcp_scan(signal, ptr, tabPtr, ptrI, fragNo);
    }
    return;
  }
  else
  {
    jam();
    /* Backup path */
    /* Get Table */
    ndbrequire(findTable(ptr, tabPtr, tableId));
  }
  c_backupFilePool.getPtr(filePtr, ptr.p->dataFilePtr[0]);

  ptr.p->slaveState.setState(SCANNING);
  ptr.p->m_gsn = GSN_BACKUP_FRAGMENT_REQ;

  ndbrequire(filePtr.p->backupPtr == ptrI);
  
  /* Get fragment */
  FragmentPtr fragPtr;
  tabPtr.p->fragments.getPtr(fragPtr, fragNo);

  ndbrequire(fragPtr.p->scanned == 0);
  ndbrequire(fragPtr.p->scanning == 0 || 
	     refToNode(ptr.p->masterRef) == getOwnNodeId());

  /**
   * Init operation
   */
  if (filePtr.p->tableId != tableId)
  {
    jam();
    DEB_EXTRA_LCP(("(%u)Init new tab(%u): maxRecordSize: %u",
                   instance(),
                   tableId,
                   tabPtr.p->maxRecordSize));
    filePtr.p->operation.init(tabPtr);
    filePtr.p->tableId = tableId;
  }//if
  
  /**
   * Check for space in buffer
   */
  if(!filePtr.p->operation.newFragment(tableId, fragPtr.p->fragmentId)) {
    jam();
    ndbrequire(!ptr.p->is_lcp());
    req->count = count + 1;
    sendSignalWithDelay(reference(), GSN_BACKUP_FRAGMENT_REQ, signal,
                        WaitDiskBufferCapacityMillis,
			signal->length());
    ptr.p->slaveState.setState(STARTED);
    return;
  }//if
  
  /**
   * Mark things as "in use"
   */
  fragPtr.p->scanning = 1;
  filePtr.p->fragmentNo = fragPtr.p->fragmentId;
  filePtr.p->m_retry_count = 0;

  ndbrequire(filePtr.p->m_flags == 
	     (BackupFile::BF_OPEN | BackupFile::BF_FILE_THREAD));
  sendScanFragReq(signal, ptr, filePtr, tabPtr, fragPtr, 0);
}

void
Backup::start_lcp_scan(Signal *signal,
                       BackupRecordPtr ptr,
                       TablePtr tabPtr,
                       Uint32 ptrI,
                       Uint32 fragNo)
{
  BackupFilePtr filePtr;
  FragmentPtr fragPtr;

  DEB_EXTRA_LCP(("(%u)Start lcp scan",
                 instance()));

  ptr.p->slaveState.setState(SCANNING);
  ptr.p->m_gsn = GSN_BACKUP_FRAGMENT_REQ;

  /* Get fragment */
  tabPtr.p->fragments.getPtr(fragPtr, fragNo);

  c_tup->start_lcp_scan(tabPtr.p->tableId,
                        fragPtr.p->fragmentId,
                        ptr.p->m_lcp_max_page_cnt);
  ptr.p->m_is_lcp_scan_active = true;
  ptr.p->m_lcp_current_page_scanned = 0;

  /**
   * Now the LCP have started for this fragment. The following
   * things have to be done in the same real-time break.
   *
   * 1) Write an LCP entry into the UNDO log.
   * 2) Get number of pages to checkpoint.
   * 3) Inform TUP that LCP scan have started
   *
   * It is not absolutely necessary to start the actual LCP scan
   * in the same real-time break. We use this opportunity to open
   * any extra LCP files that this LCP needs. If only one is needed
   * it has already been opened and we can proceed immediately.
   * However large fragments that have seen large number of writes
   * since the last LCP can require multiple LCP files. These
   * extra LCP files are opened before we actually start the
   * LCP scan.
   */

  ndbrequire(fragPtr.p->scanned == 0);
  ndbrequire(fragPtr.p->scanning == 0 || 
	     refToNode(ptr.p->masterRef) == getOwnNodeId());

  ptr.p->m_last_data_file_number =
    get_file_add(ptr.p->m_first_data_file_number,
                 ptr.p->m_num_lcp_files - 1);

  init_file_for_lcp(signal, 0, ptr, ptrI);
  if (ptr.p->m_num_lcp_files > 1)
  {
    jam();
    for (Uint32 i = 1; i < ptr.p->m_num_lcp_files; i++)
    {
      jam();
      lcp_open_data_file_late(signal, ptr, i);
    }
    return;
  }
  c_backupFilePool.getPtr(filePtr, ptr.p->dataFilePtr[0]);
  Uint32 delay = 0;
  if (ERROR_INSERTED(10047))
  {
    g_eventLogger->info("(%u)Start LCP on tab(%u,%u) 3 seconds delay, max_page: %u",
                        instance(),
                        tabPtr.p->tableId,
                        fragPtr.p->fragmentId,
                        ptr.p->m_lcp_max_page_cnt);

    if (ptr.p->m_lcp_max_page_cnt > 20)
    {
      delay = 9000;
    }
  }
  sendScanFragReq(signal, ptr, filePtr, tabPtr, fragPtr, delay);
}

void
Backup::init_file_for_lcp(Signal *signal,
                          Uint32 index,
                          BackupRecordPtr ptr,
                          Uint32 ptrI)
{
  TablePtr tabPtr;
  FragmentPtr fragPtr;
  BackupFilePtr filePtr;
  ptr.p->tables.first(tabPtr);
  tabPtr.p->fragments.getPtr(fragPtr, 0);

  c_backupFilePool.getPtr(filePtr, ptr.p->dataFilePtr[index]);
  ndbrequire(filePtr.p->backupPtr == ptrI);

  /**
   * Init operation
   */
  DEB_EXTRA_LCP(("(%u)Init new tab(%u): maxRecordSize: %u",
                 instance(),
                 tabPtr.p->tableId,
                 tabPtr.p->maxRecordSize));
  filePtr.p->operation.init(tabPtr);
  filePtr.p->tableId = tabPtr.p->tableId;

  /**
   * Mark things as "in use"
   */
  fragPtr.p->scanning = 1;
  filePtr.p->m_retry_count = 0;
  filePtr.p->m_lcp_inserts = 0;
  filePtr.p->m_lcp_writes = 0;
  filePtr.p->m_lcp_delete_by_rowids = 0;
  filePtr.p->m_lcp_delete_by_pageids = 0;

  filePtr.p->fragmentNo = 0;

  ndbrequire(filePtr.p->operation.newFragment(tabPtr.p->tableId,
                                              fragPtr.p->fragmentId));

  /**
   * Start file thread now that we will start writing also
   * fragment checkpoint data.
   */
  ndbrequire(filePtr.p->m_flags == BackupFile::BF_OPEN);
  filePtr.p->m_flags |= BackupFile::BF_FILE_THREAD;

  signal->theData[0] = BackupContinueB::START_FILE_THREAD;
  signal->theData[1] = filePtr.i;
  signal->theData[2] = __LINE__;
  sendSignal(reference(), GSN_CONTINUEB, signal, 3, JBB);
}

/**
 * Backups and LCPs are actions that operate on a long time-scale compared to
 * other activities in the cluster. We also have a number of similar
 * activities that operate on a longer time scale. These operations have to
 * continue to operate at some decent level even if user transactions are
 * arriving at extreme rates.
 *
 * Not providing sufficient activity for LCPs might mean that we run out of
 * REDO log, this means that no writing user transactions are allowed until
 * we have completed an LCP. Clearly this is not a desirable user experience.
 * So we need to find a balance between long-term needs and short-term needs
 * in scheduling LCPs and Backups versus normal user transactions.
 *
 * When designing those scheduling algorithms we need to remember the design
 * aim for the NDB storage engine. We want to ensure that NDB can be used in
 * soft real-time applications such as financial applications, telecom
 * applications. We do not aim for hard real-time applications such as
 * controlling power plants where missing a deadline can lead to major
 * catastrophies.
 *
 * Using NDB for a soft real-time application can still be done at different
 * levels of real-time requirements. If the aim is to provide that more or
 * less 100% of the transactions complete in say 100 microseconds then a
 * certain level of control is needed also from the application.
 *
 * Things that will affect scheduling in NDB are:
 * 1) Use of large rows
 *   NDB will schedule at least one row at a time. There are currently very
 *   few places where execution of one row operation contains breaks for
 *   scheduling. Executing a row operation on the maximum row size of
 *   around 14 kBytes means that signals can execute for up to about 20
 *   microseconds as of 2015. Clearly using smaller rows can give a better
 *   response time experience.
 *
 * 2) Using complex conditions per row
 *   NDB supports pushing down conditions on rows in both key operations and
 *   scan operations and even on join operations. Clearly if these pushed
 *   conditions are very complex the time to execute them per row can extend
 *   the time spent in executing one particular signal. Normal conditions
 *   involving one or a number of columns doesn't present a problem but
 *   SQL have no specific limits on conditions, so extremely complex
 *   conditions are possible to construct.
 *
 * 3) Metadata operations
 *   Creating tables, indexes can contain some operations that take a bit
 *   longer to execute. However using the multi-threaded data nodes (ndbmtd)
 *   means that most of these signals are executed in threads that are not
 *   used for normal user transactions. So using ndbmtd is here a method to
 *   decrease impact of response time of metadata operations.
 *
 * 4) Use of ndbd vs ndbmtd
 *   ndbd is a single threaded data node, ndbd does receive data, operate on
 *   the data and send the data all in one thread. In low load cases with
 *   very high requirements on response time and strict control of the
 *   application layer the use of ndbd for real-time operation can be
 *   beneficial.
 *
 *   Important here is to understand that the single-threaded nature of ndbd
 *   means that it is limited in throughput. One data node using ndbd is
 *   limited to handling on the order of 100.000 row operations per second
 *   with maintained responsiveness as of 2015. ndbmtd can achieve a few
 *   million row operations in very large configurations with maintained
 *   responsiveness.
 *
 * When looking at maintaining a balance between various operations long-term
 * it is important to consider what types of operations that can go in parallel
 * in an NDB data node. These are the activities currently possible.
 *
 * 1) Normal user transactions
 *   These consist of primary key row operations, unique key row operations
 *   (these are implemented as two primary key row operations), scan operations
 *   and finally a bit more complex operations that can have both key
 *   operations and scan operations as part of them. The last category is
 *   created as part of executing SPJ operation trees that currently is used
 *   for executing complex SQL queries.
 *
 * 2) Local checkpoints (LCPs)
 *   These can operate continously without user interaction. The LCPs are
 *   needed to ensure that we can cut the REDO log. If LCPs execute too slow
 *   the we won't have sufficient REDO log to store all user transactions that
 *   are writing on logging tables.
 *
 * 3) Backups
 *   These are started by a user, only one backup at a time is allowed. These
 *   can be stored offsite and used by the user to restore NDB to a former
 *   state, either as an emergency fix, it can also be used to start up a
 *   new cluster or as part of setting up a slave cluster. A backup consists
 *   of a data file per data node and one log file of changes since the backup
 *   started and a control file. It is important that the backup maintains a
 *   level of speed such that the system doesn't run out of disk space for the
 *   log file.
 *
 * 4) Metadata operations
 *   There are many different types of metadata operations. One can define
 *   new tables, indexes, foreign keys, tablespaces. One can also rearrange
 *   the tables for a new number of nodes as part of adding nodes to the
 *   cluster. There are also operations to analyse tables, optimise tables
 *   and so forth. Most of these are fairly short in duration and usage of
 *   resources. But there are a few of them such as rearranging tables for
 *   a new set of nodes that require shuffling data around in the cluster.
 *   This can be a fairly long-running operation.
 *
 * 5) Event operations
 *   To support replication from one MySQL Cluster to another MySQL Cluster
 *   or a different MySQL storage engine we use event operations.
 *   These operate always as part of the normal user transactions, so they
 *   do not constitute anything to consider in the balance between long-term
 *   and short-term needs. In addition in ndbmtd much of the processing happens
 *   in a special thread for event operations.
 *
 * 6) Node synchronisation during node recovery
 *   Recovery as such normally happens when no user transactions are happening
 *   so thus have no special requirements on maintaining a balance between
 *   short-term needs and long-term needs since recovery is always a long-term
 *   operation that has no competing short-term operations. There is however
 *   one exception to this and this is during node recovery when the starting
 *   node needs to synchronize its data with a live node. In this case the
 *   starting node has recovered an old version of the data node using LCPs
 *   and REDO logs and have rebuilt the indexes. At this point it needs to
 *   synchronize the data in each table with a live node within the same node
 *   group.
 *
 *   This synchronization happens row by row controlled by the live node. The
 *   live scans its own data and checks each row to the global checkpoint id
 *   (GCI) that the starting node has restored. If the row has been updated
 *   with a more recent GCI then the row needs to be sent over to the starting
 *   node.
 *
 *   Only one node recovery per node group at a time is possible when using
 *   two replicas.
 *
 * So there can be as many as 4 long-term operations running in parallel to
 * the user transactions. These are 1 LCP scan, 1 Backup scan, 1 node recovery
 * scan and finally 1 metadata scan. All of these long-running operations
 * perform scans of table partitions (fragments). LCPs scan a partition and
 * write rows into a LCP file. Backups scan a partition and write its result
 * into a backup file. Node recovery scans searches for rows that have been
 * updated since the GCI recovered in the starting node and for each row
 * found it is sent over to the starting node. Metadata scans for either
 * all rows or using some condition and then can use this information to
 * send the row to another node, to build an index, to build a foreign key
 * index or other online operation which is performed in parallel to user
 * transactions.
 *
 * From this analysis it's clear that we don't want any long-running operation
 * to consume any major part of the resources. It's desirable that user
 * transactions can use at least about half of the resources even when running
 * in parallel with all four of those activities. Node recovery is slightly
 * more important than the other activities, this means that our aim should
 * be to ensure that LCPs, Backups and metadata operations can at least use
 * about 10% of the CPU resources and that node recovery operations can use
 * at least about 20% of the CPU resources. Obviously they should be able to
 * use more resources when there is less user transactions competing for the
 * resources. But we should try to maintain this level of CPU usage for LCPs
 * and Backups even when the user load is at extreme levels.
 *
 * There is no absolute way of ensuring 10% CPU usage for a certain activity.
 * We use a number of magic numbers controlling the algorithms to ensure this.
 * 
 * At first we use the coding rule that one signal should never execute for
 * more than 10 microseconds in the normal case. There are exceptions to this
 * rule as explained above, but it should be outliers that won't affect the
 * long-term rates very much.
 *
 * Second we use the scheduling classes we have access to. The first is B-level
 * signals, these can have an arbitrary long queue of other jobs waiting before
 * they are executed, so these have no bound on when they execute. We also
 * have special signals that execute with a bounded delay, in one signal they
 * can be delayed more than a B-level signal, but the scheduler ensures that
 * at most 100 B-level signals execute before they are executed. Normally it
 * would even operate with at most 75 B-level signals executed even in high
 * load scenarios and mostly even better than that. We achieve this by calling
 * sendSignalWithDelay with timeout BOUNDED_DELAY.
 *
 * So how fast can an LCP run that is using about 10% of the CPU. In a fairly
 * standard CPU of 2015, not a high-end, but also not at the very low-end,
 * the CPU can produce about 150 MBytes of data for LCPs per second. This is
 * using 100 byte rows. So this constitutes about 1.5M rows per second plus
 * transporting 150 MBytes of data to the write buffers in the Backup block.
 * So we use a formula here where we assume that the fixed cost of scanning
 * a row is about 550 ns and cost per word of data is 4 ns. The reason we
 * a different formula for LCP scans compared to the formula we assume in
 * DBLQH for generic scans is that the copy of data is per row for LCPs
 * whereas it is per column for generic scans. Similarly we never use any
 * scan filters for LCPs, we only check for LCP_SKIP bits and FREE bits.
 * This is much more efficient compared to generic scan filters.
 *
 * At very high load we will assume that we have to wait about 50 signals
 * when sending BOUNDED_DELAY signals. Worst case can be up to about 100
 * signals, but the worst case won't happen very often and more common
 * will be much less than that.
 * The mean execution time of signals are about 5 microseconds. This means
 * that by constantly using bounded delay signals we ensure that we get at
 * least around 4000 executions per second. So this means that
 * in extreme overload situations we can allow for execution to go on
 * for up to about 25 microseconds without giving B-level signals access.
 * 25 microseconds times 4000 is 100 milliseconds so about 10% of the
 * CPU usage.
 *
 * LCPs and Backups also operate using conditions on how fast they can write
 * to the disk subsystem. The user can configure these numbers, the LCPs
 * and Backups gets a quota per 100 millisecond. So if the LCPs and Backups
 * runs too fast they will pause a part of those 100 milliseconds. However
 * it is a good idea to set the minimum disk write speed to at least 20%
 * of the possible CPU speed. So this means setting it to 30 MByte per
 * second. In high-load scenarios we might not be able to process more
 * than 15 MByte per second, but as soon as user load and other load
 * goes down we will get back to the higher write speed.
 *
 * Scans operate in the following fashion which is an important input to
 * the construction of the magic numbers. We start a scan with SCAN_FRAGREQ
 * and here we don't really know the row sizes other than the maximum row
 * size. This SCAN_FRAGREQ will return 16 rows and then it will return
 * SCAN_FRAGCONF. For each row it will return a TRANSID_AI signal.
 * If we haven't used our quota for writing LCPs and Backups AND there is
 * still room in the backup write buffer then we will continue with another
 * set of 16 rows. These will be retrieved using the SCAN_NEXTREQ signal
 * and the response to this signal will be SCAN_FRAGCONF when done with the
 * 16 rows (or all rows scanned).
 * 
 * Processing 16 rows takes about 8800 ns on standard HW of 2015 and so even
 * for minimal rows we will use at least 10000 ns if we execute an entire batch
 * of 16 rows without providing access for other B-level signals. So the
 * absolute maximum amount of rows that we will ever execute without
 * giving access for B-level signals are 32 rows so that we don't go beyond
 * the allowed quota of 25 microsecond without giving B-level priority signal
 * access, this means two SCAN_FRAGREQ/SCAN_NEXTREQ executions.
 *
 * Using the formula we derive that we should never start another set of
 * 16 rows if we have passed 1500 words in the previous batch of 16 rows.
 * Even when deciding in the Backup block to send an entire batch of 16
 * rows at A-level we will never allow to continue gathering when we have
 * already gathered more than 4000 words. When we reach this limit we will
 * send another bounded delay signal. The reason is that we've already
 * reached sufficient CPU usage and going further would go beyond 15%.
 *
 * The boundary 1500 and 4000 is actually based on using 15% of the CPU
 * resources which is better if not all four activities happen at the
 * same time. When we support rate control on all activities we need to
 * adaptively decrease this limit to ensure that the total rate controlled
 * efforts doesn't go beyond 50%.
 *
 * The limit 4000 is ZMAX_WORDS_PER_SCAN_BATCH_HIGH_PRIO set in DblqhMain.cpp.
 * This constant limit the impact of wide rows on responsiveness.
 *
 * The limit 1500 is MAX_LCP_WORDS_PER_BATCH set in this block.
 * This constant limit the impact of row writes on LCP writes.
 *
 * When operating in normal mode, we will not continue gathering when we
 * already gathered at least 500 words. However we will only operate in
 * this mode when we are in low load scenario in which case this speed will
 * be quite sufficient. This limit is to ensure that we don't go beyond
 * normal real-time break limits in normal operations. This limits LCP
 * execution during normal load to around 3-4 microseconds.
 *
 * In the following paragraph a high priority of LCPs means that we need to
 * raise LCP priority to maintain LCP write rate at the expense of user
 * traffic responsiveness. Low priority means that we can get sufficient
 * LCP write rates even with normal responsiveness to user requests.
 *
 * Finally we have to make a decision when we should execute at high priority
 * and when operating at normal priority. Obviously we should avoid entering
 * high priority mode as much as possible since it will affect response times.
 * At the same time once we have entered this mode we need to have some
 * memory of it. The reason is that we will have lost some ground while
 * executing at normal priority when the job buffers were long. We will limit
 * the memory to at most 16 executions of 16 rows at high priority. Each
 * time we start a new execution we will see if we need to add to this
 * "memory". We will add one per 48 signals that we had to wait for between
 * executing a set of 16 rows (normally this means execution of 3 bounded
 * delay signals). When the load level is even higher than we will add to
 * the memory such that we operate in high priority mode a bit longer since
 * we are likely to have missed a bit more opportunity to perform LCP scans
 * in this overload situation.
 *
 * The following "magic" constants control these algorithms:
 * 1) ZMAX_SCAN_DIRECT_COUNT set to 5
 * Means that at most 6 rows will be scanned per execute direct, set in
 * Dblqh.hpp. This applies to all scan types, not only to LCP scans.
 *
 * 2) ZMAX_WORDS_PER_SCAN_BATCH_LOW_PRIO set to 500
 * This controls the maximum number of words that is allowed to be gathered
 * before we decide to do a real-time break when executing at normal
 * priority level. This is defined in DblqhMain.cpp
 *
 * 3) ZMAX_WORDS_PER_SCAN_BATCH_HIGH_PRIO set to 4000
 * This controls the maximum words gathered before we decide to send the
 * next row to be scanned in another bounded delay signal. This is defined in
 * DblqhMain.cpp
 *
 * 4) MAX_LCP_WORDS_PER_BATCH set to 1500
 * This defines the maximum size gathered at A-level to allow for execution
 * of one more batch at A-level. This is defined here in Backup.cpp.
 *
 * 5) HIGH_LOAD_LEVEL set to 32
 * Limit of how many signals have been executed in this LDM thread since
 * starting last 16 rowsin order to enter high priority mode.
 * Defined in this block Backup.cpp.
 *
 * 6) VERY_HIGH_LOAD_LEVEL set to 48
 * For each additional of this we increase the memory. So e.g. with 80 signals
 * executed since last we will increase the memory by two, with 128 we will
 * increase it by three. Thus if #signals >= (32 + 48) => 2, #signals >=
 * (32 + 48 * 2) => 3 and so forth. Memory here means that we will remember
 * the high load until we have compensated for it in a sufficient manner, so
 * we will retain executing on high priority for a bit longer to compensate
 * for what we lost during execution at low priority when load suddenly
 * increased.
 * Defined in this block Backup.cpp.
 *
 * 7) MAX_RAISE_PRIO_MEMORY set to 16
 * Max memory of priority raising, so after load disappears we will at most
 * an additional set of 16*16 rows at high priority mode before going back to
 * normal priority mode.
 * Defined in this block Backup.cpp.
 *
 * 8) NUMBER_OF_SIGNALS_PER_SCAN_BATCH set to 3
 * When starting up the algorithm we check how many signals are in the
 * B-level job buffer. Based on this number we set the initial value to
 * high priority or not. This is based on that we expect a set of 16
 * rows to be executed in 3 signals with 6 rows, 6 rows and last signal
 * 4 rows.
 * Defined in this block Backup.cpp.
 */

 /**
 * These routines are more or less our scheduling logic for LCPs. This is
 * how we try to achieve a balanced output from LCPs while still
 * processing normal transactions at a high rate.
 */
void Backup::init_scan_prio_level(Signal *signal, BackupRecordPtr ptr)
{
  Uint32 level = getSignalsInJBB();
  if ((level * NUMBER_OF_SIGNALS_PER_SCAN_BATCH) > HIGH_LOAD_LEVEL)
  {
    /* Ensure we use prio A and only 1 signal at prio A */
    jam();
    level = VERY_HIGH_LOAD_LEVEL;
  }
  ptr.p->m_lastSignalId = signal->getSignalId() - level;
  ptr.p->m_prioA_scan_batches_to_execute = 0;
}

bool
Backup::check_scan_if_raise_prio(Signal *signal, BackupRecordPtr ptr)
{
  bool flag = false;
  const Uint32 current_signal_id = signal->getSignalId();
  const Uint32 lastSignalId = ptr.p->m_lastSignalId;
  Uint32 prioA_scan_batches_to_execute =
    ptr.p->m_prioA_scan_batches_to_execute;
  const Uint32 num_signals_executed = current_signal_id - lastSignalId;
  
  if (num_signals_executed > HIGH_LOAD_LEVEL)
  {
    jam();
    prioA_scan_batches_to_execute+= 
      ((num_signals_executed + (VERY_HIGH_LOAD_LEVEL - 1)) / 
        VERY_HIGH_LOAD_LEVEL);
    if (prioA_scan_batches_to_execute > MAX_RAISE_PRIO_MEMORY)
    {
      jam();
      prioA_scan_batches_to_execute = MAX_RAISE_PRIO_MEMORY;
    }
  }
  if (prioA_scan_batches_to_execute > 0)
  {
    jam();
    prioA_scan_batches_to_execute--;
    flag = true;
  }
  ptr.p->m_lastSignalId = current_signal_id;
  ptr.p->m_prioA_scan_batches_to_execute = prioA_scan_batches_to_execute;
  return flag;
}

void
Backup::sendScanFragReq(Signal* signal,
                        Ptr<BackupRecord> ptr,
                        Ptr<BackupFile> filePtr,
                        Ptr<Table> tabPtr,
                        Ptr<Fragment> fragPtr,
                        Uint32 delay)
{
  /**
   * Start scan
   */
  {
    if (!(ptr.p->is_lcp() &&
          ptr.p->m_num_lcp_files > 1))
    {
      jam();
      filePtr.p->m_flags |= BackupFile::BF_SCAN_THREAD;
    }
    else
    {
      jam();
      for (Uint32 i = 0; i < ptr.p->m_num_lcp_files; i++)
      {
        BackupFilePtr loopFilePtr;
        c_backupFilePool.getPtr(loopFilePtr, ptr.p->dataFilePtr[i]);
        loopFilePtr.p->m_flags |= BackupFile::BF_SCAN_THREAD;
      }
    }
    
    Table & table = * tabPtr.p;
    ScanFragReq * req = (ScanFragReq *)signal->getDataPtrSend();
    const Uint32 parallelism = ZRESERVED_SCAN_BATCH_SIZE;

    req->senderData = filePtr.i;
    req->resultRef = reference();
    req->schemaVersion = table.schemaVersion;
    req->fragmentNoKeyLen = fragPtr.p->fragmentId;
    req->requestInfo = 0;
    req->savePointId = 0;
    req->tableId = table.tableId;
    ScanFragReq::setReadCommittedFlag(req->requestInfo, 1);
    ScanFragReq::setLockMode(req->requestInfo, 0);
    ScanFragReq::setHoldLockFlag(req->requestInfo, 0);
    ScanFragReq::setKeyinfoFlag(req->requestInfo, 0);
    ScanFragReq::setTupScanFlag(req->requestInfo, 1);
    ScanFragReq::setNotInterpretedFlag(req->requestInfo, 1);
    if (ptr.p->is_lcp())
    {
      ScanFragReq::setScanPrio(req->requestInfo, 1);
      ScanFragReq::setNoDiskFlag(req->requestInfo, 1);
      ScanFragReq::setLcpScanFlag(req->requestInfo, 1);
    }
    filePtr.p->m_sent_words_in_scan_batch = 0;
    filePtr.p->m_num_scan_req_on_prioa = 0;
    init_scan_prio_level(signal, ptr);
    if (check_scan_if_raise_prio(signal, ptr))
    {
      jam();
      ScanFragReq::setPrioAFlag(req->requestInfo, 1);
      filePtr.p->m_num_scan_req_on_prioa = 1;
    }

    req->transId1 = 0;
    req->transId2 = (BACKUP << 20) + (getOwnNodeId() << 8);
    req->clientOpPtr= filePtr.i;
    req->batch_size_rows= parallelism;
    req->batch_size_bytes= 0;
    BlockReference lqhRef = 0;
    bool delay_possible = true;
    if (ptr.p->is_lcp()) {
      lqhRef = calcInstanceBlockRef(DBLQH);
    } else {
      const Uint32 instanceKey = fragPtr.p->lqhInstanceKey;
      ndbrequire(instanceKey != 0);
      lqhRef = numberToRef(DBLQH, instanceKey, getOwnNodeId());
      if (lqhRef != calcInstanceBlockRef(DBLQH))
      {
        /* We can't send delayed signals to other threads. */
        delay_possible = false;
      }
    }

    Uint32 attrInfo[25];
    memcpy(attrInfo, table.attrInfo, 4*table.attrInfoLen);
    LinearSectionPtr ptr[3];
    ptr[0].p = attrInfo;
    ptr[0].sz = table.attrInfoLen;
    if (delay_possible)
    {
      SectionHandle handle(this);
      ndbrequire(import(handle.m_ptr[0], ptr[0].p, ptr[0].sz));
      handle.m_cnt = 1;
      if (delay == 0)
      {
        jam();
        sendSignalWithDelay(lqhRef, GSN_SCAN_FRAGREQ, signal,
                            BOUNDED_DELAY, ScanFragReq::SignalLength, &handle);
      }
      else
      {
        jam();
        sendSignalWithDelay(lqhRef, GSN_SCAN_FRAGREQ, signal,
                            delay, ScanFragReq::SignalLength, &handle);
      }
    }
    else
    {
      /**
       * There is no way to send signals over to another thread at a rate
       * level at the moment. So we send at priority B, but the response
       * back to us will arrive at Priority A if necessary.
       */
      jam();
      sendSignal(lqhRef,
                 GSN_SCAN_FRAGREQ,
                 signal,
                 ScanFragReq::SignalLength,
                 JBB,
                 ptr,
                 1);
    }
  }
}

void
Backup::execSCAN_HBREP(Signal* signal)
{
  jamEntry();
}

void
Backup::record_deleted_pageid(Uint32 pageNo, Uint32 record_size)
{
  BackupRecordPtr ptr;
  BackupFilePtr zeroFilePtr;
  BackupFilePtr currentFilePtr;
  c_backupPool.getPtr(ptr, m_lcp_ptr_i);
  c_backupFilePool.getPtr(zeroFilePtr, ptr.p->dataFilePtr[0]);
  c_backupFilePool.getPtr(currentFilePtr, ptr.p->m_working_data_file_ptr);
  OperationRecord & current_op = currentFilePtr.p->operation;
  OperationRecord & zero_op = zeroFilePtr.p->operation;
  ndbrequire(ptr.p->m_num_parts_in_this_lcp != BackupFormat::NDB_MAX_LCP_PARTS);
  Uint32 * dst = current_op.dst;
  Uint32 dataLen = 2;
  Uint32 copy_array[2];
  copy_array[0] = pageNo;
  copy_array[1] = record_size;
  DEB_LCP_DEL(("(%u) DELETE_BY_PAGEID: page(%u)",
                instance(),
                pageNo));
  *dst = htonl(Uint32(dataLen + (BackupFormat::DELETE_BY_PAGEID_TYPE << 16)));
  memcpy(dst + 1, copy_array, dataLen*sizeof(Uint32));
  ndbrequire(dataLen < zero_op.maxRecordSize);
  zeroFilePtr.p->m_sent_words_in_scan_batch += dataLen;
  zeroFilePtr.p->m_lcp_delete_by_pageids++;
  zero_op.finished(dataLen);
  current_op.newRecord(dst + dataLen + 1);
  ptr.p->noOfRecords++;
  ptr.p->noOfBytes += (4*(dataLen + 1));
  /**
   * LCP keep pages are handled out of order, so here we have prepared before
   * calling NEXT_SCANCONF by temporarily changing the current data file used.
   * Since scans use deep call chaining we restore the current data file
   * immediately after each row written into the LCP data file. Same happens
   * also for TRANSID_AI and record_deleted_rowid.
   */
  restore_current_page(ptr);
}

void
Backup::record_deleted_rowid(Uint32 pageNo, Uint32 pageIndex, Uint32 gci)
{
  BackupRecordPtr ptr;
  BackupFilePtr zeroFilePtr;
  BackupFilePtr currentFilePtr;
  c_backupPool.getPtr(ptr, m_lcp_ptr_i);
  c_backupFilePool.getPtr(zeroFilePtr, ptr.p->dataFilePtr[0]);
  c_backupFilePool.getPtr(currentFilePtr, ptr.p->m_working_data_file_ptr);
  OperationRecord & current_op = currentFilePtr.p->operation;
  OperationRecord & zero_op = zeroFilePtr.p->operation;
  ndbrequire(ptr.p->m_num_parts_in_this_lcp != BackupFormat::NDB_MAX_LCP_PARTS);
  Uint32 * dst = current_op.dst;
  Uint32 dataLen = 3;
  Uint32 copy_array[3];
  copy_array[0] = pageNo;
  copy_array[1] = pageIndex;
  copy_array[2] = gci;
  DEB_LCP_DEL(("(%u) DELETE_BY_ROWID: row(%u,%u)",
                instance(),
                pageNo,
                pageIndex));
  *dst = htonl(Uint32(dataLen + (BackupFormat::DELETE_BY_ROWID_TYPE << 16)));
  memcpy(dst + 1, copy_array, dataLen*sizeof(Uint32));
  ndbrequire(dataLen < zero_op.maxRecordSize);
  zeroFilePtr.p->m_sent_words_in_scan_batch += dataLen;
  zeroFilePtr.p->m_lcp_delete_by_rowids++;
  zero_op.finished(dataLen);
  current_op.newRecord(dst + dataLen + 1);
  ptr.p->noOfRecords++;
  ptr.p->noOfBytes += (4*(dataLen + 1));
  restore_current_page(ptr);
}

void
Backup::execTRANSID_AI(Signal* signal)
{
  jamEntryDebug();

  const Uint32 filePtrI = signal->theData[0];
  //const Uint32 transId1 = signal->theData[1];
  //const Uint32 transId2 = signal->theData[2];
  Uint32 dataLen  = signal->length() - 3;
  
  BackupFilePtr filePtr;
  c_backupFilePool.getPtr(filePtr, filePtrI);

  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, filePtr.p->backupPtr);

  OperationRecord & op = filePtr.p->operation;
  if (ptr.p->is_lcp())
  {
    BackupFilePtr currentFilePtr;
    c_backupFilePool.getPtr(currentFilePtr, ptr.p->m_working_data_file_ptr);
    OperationRecord & current_op = currentFilePtr.p->operation;
    Uint32 * dst = current_op.dst;
    Uint32 header;
    if (ptr.p->m_working_changed_row_page_flag)
    {
      /* LCP for CHANGED ROWS pages */
      jam();
      header = dataLen + (BackupFormat::WRITE_TYPE << 16);
      filePtr.p->m_lcp_writes++;
    }
    else
    {
      /* LCP for ALL ROWS pages */
      jam();
      header = dataLen + (BackupFormat::INSERT_TYPE << 16);
      filePtr.p->m_lcp_inserts++;
    }
    ptr.p->noOfRecords++;
    ptr.p->noOfBytes += (4*(dataLen + 1));
#ifdef VM_TRACE
    Uint32 th = signal->theData[4];
    ndbassert(! (th & 0x00400000)); /* Is MM_GROWN set */
#endif
    ndbrequire(signal->getNoOfSections() == 0);
    const Uint32 * src = &signal->theData[3];
    * dst = htonl(header);
    memcpy(dst + 1, src, 4*dataLen);
#ifdef DEBUG_LCP_ROW
    TablePtr debTabPtr;
    FragmentPtr fragPtr;
    ptr.p->tables.first(debTabPtr);
    debTabPtr.p->fragments.getPtr(fragPtr, 0);
    g_eventLogger->info("(%u) tab(%u,%u) Write row(%u,%u) into LCP, bits: %x",
                 instance(),
                 debTabPtr.p->tableId,
                 fragPtr.p->fragmentId,
                 src[0],
                 src[1],
                 src[3]);
#endif
    if (unlikely(dataLen >= op.maxRecordSize))
    {
      g_eventLogger->info("dataLen: %u, op.maxRecordSize = %u, header: %u",
                          dataLen, op.maxRecordSize, header);
      jamLine(dataLen);
      jamLine(op.maxRecordSize);
      ndbrequire(false);
    }
    filePtr.p->m_sent_words_in_scan_batch += dataLen;
    op.finished(dataLen);
    current_op.newRecord(dst + dataLen + 1);
    restore_current_page(ptr);
  }
  else
  {
    /* Backup handling */
    Uint32 * dst = op.dst;
    Uint32 header = dataLen;
    if (signal->getNoOfSections() == 0)
    {
      jam();
      const Uint32 * src = &signal->theData[3];
      * dst = htonl(header);
      memcpy(dst + 1, src, 4*dataLen);
    }
    else
    {
      jam();
      SectionHandle handle(this, signal);
      SegmentedSectionPtr dataPtr;
      handle.getSection(dataPtr, 0);
      dataLen = dataPtr.sz;

      * dst = htonl(dataLen);
      copy(dst + 1, dataPtr);
      releaseSections(handle);
    }
    filePtr.p->m_sent_words_in_scan_batch += dataLen;
    op.finished(dataLen);
    op.newRecord(dst + dataLen + 1);
  }
}

bool
Backup::is_all_rows_page(BackupRecordPtr ptr,
                         Uint32 part_id)
{
  if (check_if_in_page_range(part_id,
         ptr.p->m_scan_info[ptr.p->m_num_lcp_files-1].m_start_change_part,
         ptr.p->m_scan_info[ptr.p->m_num_lcp_files-1].m_num_change_parts))
  {
    jam();
    return false;
  }
  jam();
  return true;
}

void
Backup::set_working_file(BackupRecordPtr ptr,
                         Uint32 part_id,
                         bool is_all_rows_page)
{
  Uint32 index = ptr.p->m_num_lcp_files - 1; //Change pages index
  if (is_all_rows_page)
  {
    bool found = false;
    for (Uint32 i = 0; i < ptr.p->m_num_lcp_files; i++)
    {
      if (check_if_in_page_range(part_id,
            ptr.p->m_scan_info[i].m_start_all_part,
            ptr.p->m_scan_info[i].m_num_all_parts))
      {
        jam();
        found = true;
        index = i;
        break;
      }
    }
    ndbrequire(found);
  }
  ptr.p->m_working_data_file_ptr = ptr.p->dataFilePtr[index];
}

bool
Backup::check_if_in_page_range(Uint32 part_id,
                               Uint32 start_part,
                               Uint32 num_parts)
{
  Uint32 end_part;
  if (part_id >= start_part)
  {
    if ((start_part + num_parts) > part_id)
    {
      return true;
    }
  }
  else
  {
    end_part = start_part + num_parts;
    if ((part_id + BackupFormat::NDB_MAX_LCP_PARTS) < end_part)
    {
      return true;
    }
  }
  jam();
  return false;
}

Uint32
Backup::hash_lcp_part(Uint32 page_id) const
{
  /**
   * To ensure proper operation also with small number of pages
   * we make a complete bit reorder of the 11 least significant
   * bits of the page id and returns this as the part id to use.
   * This means that for e.g. 8 pages we get the following parts
   * used:
   * 0: 0, 1: 1024, 2: 512, 3: 1536, 4: 256, 5: 1280, 6: 768, 7: 1792
   *
   * This provides a fairly good spread also of small number of
   * pages into the various parts.
   *
   * We implement this bit reorder by handling 4 sets of 3 bits,
   * except for the highest bits where we only use 2 bits.
   * Each 3 bit set is reversed using a simple static lookup
   * table and then the result of those 4 lookups is put back
   * into the hash value in reverse order.
   *
   * As a final step we remove bit 0 which is always 0 since we
   * only use 11 bits and not 12 bits.
   */
  static Uint32 reverse_3bits_array[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };
  const Uint32 lowest_3bits_page_id = page_id & 7;
  const Uint32 low_3bits_page_id = (page_id >> 3) & 7;
  const Uint32 high_3bits_page_id = (page_id >> 6) & 7;
  const Uint32 highest_3bits_page_id = (page_id >> 9) & 3;
  Uint32 part_id =
    reverse_3bits_array[highest_3bits_page_id] +
    (reverse_3bits_array[high_3bits_page_id] << 3) +
    (reverse_3bits_array[low_3bits_page_id] << 6) +
    (reverse_3bits_array[lowest_3bits_page_id] << 9);
  part_id >>= 1;
  return part_id;
}

bool
Backup::is_change_part_state(Uint32 page_id)
{
  BackupRecordPtr ptr;
  jamEntry();
  c_backupPool.getPtr(ptr, m_lcp_ptr_i);
  Uint32 part_id = hash_lcp_part(page_id);
  bool is_all_part = is_all_rows_page(ptr, part_id);
  return !is_all_part;
}

void
Backup::get_page_info(BackupRecordPtr ptr,
                      Uint32 part_id,
                      Uint32 & scanGCI,
                      bool & changed_row_page_flag)
{
  if (is_all_rows_page(ptr, part_id))
  {
    /**
     * We are  within range for all parts to be changed.
     * return scanGCI = 0 such that all rows in this page becomes part
     * of this LCP.
     */
    jam();
    scanGCI = 0;
    changed_row_page_flag = false;
  }
  else
  {
    /**
     * Not all rows to be recorded, only changed rows on this page.
     */
    jam();
    ndbassert(is_partial_lcp_enabled());
    scanGCI = ptr.p->m_scan_change_gci;
    ndbrequire(scanGCI != 0);
    changed_row_page_flag = true;
  }
}

void
Backup::change_current_page_temp(Uint32 page_no)
{
  BackupRecordPtr ptr;
  jamEntry();
  c_backupPool.getPtr(ptr, m_lcp_ptr_i);
  Uint32 part_id = hash_lcp_part(page_no);
  ptr.p->m_working_changed_row_page_flag = !(is_all_rows_page(ptr, part_id));
  set_working_file(ptr,
                   part_id,
                   !ptr.p->m_working_changed_row_page_flag);
}

/**
 * After each operation, whether it is INSERT, WRITE or any DELETE variant,
 * we restore the working data file and current page flag. We can change
 * those for one operation (when retrieving a record from LCP keep list).
 * Since we don't know when we retrieved a record from LCP keep list here,
 * we simply always restore. The current values always have the current
 * setting and the working is the one we're currently using.
 */
void
Backup::restore_current_page(BackupRecordPtr ptr)
{
  ptr.p->m_working_data_file_ptr = ptr.p->m_current_data_file_ptr;
  ptr.p->m_working_changed_row_page_flag =
    ptr.p->m_current_changed_row_page_flag;
}

void
Backup::init_lcp_scan(Uint32 & scanGCI,
                      bool & changed_row_page_flag)
{
  /**
   * Here we come to get what to do with page 0.
   *
   * The number of pages seen at start of LCP scan was set in the method
   * start_lcp_scan. It is of vital importance that this happens
   * synchronised with the insertion of the LCP record in the UNDO log
   * record. There cannot be any signal breaks between setting the
   * max page count, initialising the LCP scan variable in TUP and
   * initialising the variables in this block and finally to insert a
   * start LCP record in UNDO log to allow for proper
   * handling of commits after start of LCP scan (to ensure that we
   * set LCP_SKIP and LCP_DELETE bits when necessary). It is important
   * that we retain exactly the set of rows committed before the start
   * of the LCP scan (the commit point is when the signal TUP_COMMITREQ
   * returns to DBLQH) and that rows inserted after this point is not
   * part of the LCP, this will guarantee that we get synchronisation
   * between the LCP main memory data and the disk data parts after
   * executing the UNDO log.
   *
   * The number of pages will be stored in the LCP to ensure that we can
   * remove rowid's that have been deleted before the next LCP starts.
   * The next LCP will never see any deleted rowid's, so those need to be
   * deleted before applying the rest of the LCP. The actual LCP contains
   * DELETE by ROWID for all rowid's in the range of pages still existing,
   * but for those removed we need to delete all those rows in one go at
   * start of restore by using the number of pages that is part of LCP.
   */
  BackupRecordPtr ptr;
  jamEntry();
  c_backupPool.getPtr(ptr, m_lcp_ptr_i);
  Uint32 part_id = hash_lcp_part(0);
  get_page_info(ptr,
                part_id,
                scanGCI,
                changed_row_page_flag);
  set_working_file(ptr, part_id, !changed_row_page_flag);
  ptr.p->m_current_data_file_ptr = ptr.p->m_working_data_file_ptr;
  ptr.p->m_working_changed_row_page_flag = changed_row_page_flag;
  ptr.p->m_current_changed_row_page_flag = changed_row_page_flag;

#ifdef DEBUG_EXTRA_LCP
  TablePtr debTabPtr;
  FragmentPtr fragPtr;
  ptr.p->tables.first(debTabPtr);
  debTabPtr.p->fragments.getPtr(fragPtr, 0);
  DEB_EXTRA_LCP(("(%u)LCP scan page tab(%u,%u): %u, part_id: %u,"
                 " round: %u, %s",
          instance(),
          debTabPtr.p->tableId,
          fragPtr.p->fragmentId,
          0,
          part_id,
          0,
          changed_row_page_flag ? "CHANGED ROWS page" : " ALL ROWS page"));
#endif
}

void
Backup::alloc_page_after_lcp_start(Uint32 page_no)
{
  BackupRecordPtr ptr;
  jamEntry();
  c_backupPool.getPtr(ptr, m_lcp_ptr_i);
  ptr.p->m_any_lcp_page_ops = true;
  if (is_change_part_state(page_no))
    ptr.p->m_change_page_alloc_after_start++;
  else
    ptr.p->m_all_page_alloc_after_start++;
}

void
Backup::alloc_dropped_page_after_lcp_start(bool is_change_page)
{
  BackupRecordPtr ptr;
  jamEntry();
  c_backupPool.getPtr(ptr, m_lcp_ptr_i);
  ptr.p->m_any_lcp_page_ops = true;
  if (is_change_page)
  {
    ptr.p->m_change_page_alloc_dropped_after_start++;
  }
  else
  {
    ptr.p->m_all_page_alloc_dropped_after_start++;
  }
}

void
Backup::dropped_page_after_lcp_start(bool is_change_page,
                                     bool is_last_lcp_state_A)
{
  BackupRecordPtr ptr;
  jamEntry();
  c_backupPool.getPtr(ptr, m_lcp_ptr_i);
  ptr.p->m_any_lcp_page_ops = true;
  if (is_last_lcp_state_A)
  {
    if (is_change_page)
      ptr.p->m_change_page_dropped_A_after_start++;
    else
      ptr.p->m_all_page_dropped_A_after_start++;
  }
  else
  {
    if (is_change_page)
      ptr.p->m_change_page_dropped_D_after_start++;
    else
      ptr.p->m_all_page_dropped_D_after_start++;
  }
}

void
Backup::skip_page_lcp_scanned_bit()
{
  BackupRecordPtr ptr;
  jamEntry();
  c_backupPool.getPtr(ptr, m_lcp_ptr_i);
  ptr.p->m_any_lcp_page_ops = true;
  if (ptr.p->m_working_changed_row_page_flag)
    ptr.p->m_skip_change_page_lcp_scanned_bit++;
  else
    ptr.p->m_skip_all_page_lcp_scanned_bit++;
}

void
Backup::skip_no_change_page()
{
  BackupRecordPtr ptr;
  jamEntryDebug();
  c_backupPool.getPtr(ptr, m_lcp_ptr_i);
  ptr.p->m_any_lcp_page_ops = true;
  ptr.p->m_skip_change_page_no_change++;
}

void
Backup::skip_empty_page_lcp()
{
  BackupRecordPtr ptr;
  jamEntry();
  c_backupPool.getPtr(ptr, m_lcp_ptr_i);
  ptr.p->m_any_lcp_page_ops = true;
  if (ptr.p->m_working_changed_row_page_flag)
    ptr.p->m_skip_empty_change_page++;
  else
    ptr.p->m_skip_empty_all_page++;
}

void
Backup::record_dropped_empty_page_lcp()
{
  BackupRecordPtr ptr;
  jamEntry();
  c_backupPool.getPtr(ptr, m_lcp_ptr_i);
  ndbrequire(ptr.p->m_working_changed_row_page_flag)
  ptr.p->m_any_lcp_page_ops = true;
  ptr.p->m_record_empty_change_page_A++;
}

void
Backup::record_late_alloc_page_lcp()
{
  BackupRecordPtr ptr;
  jamEntry();
  c_backupPool.getPtr(ptr, m_lcp_ptr_i);
  ndbrequire(ptr.p->m_working_changed_row_page_flag)
  ptr.p->m_any_lcp_page_ops = true;
  ptr.p->m_record_late_alloc_change_page_A++;
}

void
Backup::page_to_skip_lcp(bool is_last_lcp_state_A)
{
  BackupRecordPtr ptr;
  jamEntry();
  c_backupPool.getPtr(ptr, m_lcp_ptr_i);
  ptr.p->m_any_lcp_page_ops = true;
  if (ptr.p->m_working_changed_row_page_flag)
  {
    ndbrequire(!is_last_lcp_state_A);
    ptr.p->m_skip_late_alloc_change_page_D++;
  }
  else
  {
    if (is_last_lcp_state_A)
      ptr.p->m_skip_late_alloc_all_page_A++;
    else
      ptr.p->m_skip_late_alloc_all_page_D++;
  }
}

void
Backup::lcp_keep_delete_by_page_id()
{
  BackupRecordPtr ptr;
  jamEntry();
  c_backupPool.getPtr(ptr, m_lcp_ptr_i);
  ptr.p->m_any_lcp_page_ops = true;
  if (ptr.p->m_working_changed_row_page_flag)
    ptr.p->m_lcp_keep_delete_change_pages++;
  else
    ptr.p->m_lcp_keep_delete_all_pages++;
}

void
Backup::lcp_keep_delete_row()
{
  BackupRecordPtr ptr;
  jamEntry();
  c_backupPool.getPtr(ptr, m_lcp_ptr_i);
  ptr.p->m_any_lcp_page_ops = true;
  if (ptr.p->m_working_changed_row_page_flag)
    ptr.p->m_lcp_keep_delete_row_change_pages++;
  else
    ptr.p->m_lcp_keep_delete_row_all_pages++;
}

void
Backup::lcp_keep_row()
{
  BackupRecordPtr ptr;
  jamEntry();
  c_backupPool.getPtr(ptr, m_lcp_ptr_i);
  ptr.p->m_any_lcp_page_ops = true;
  if (ptr.p->m_working_changed_row_page_flag)
    ptr.p->m_lcp_keep_row_change_pages++;
  else
    ptr.p->m_lcp_keep_row_all_pages++;
}

void
Backup::print_extended_lcp_stat()
{
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, m_lcp_ptr_i);
  if (!ptr.p->m_any_lcp_page_ops)
    return;
  g_eventLogger->info("(%u)change_page_alloc_after_start: %u, "
                      "all_page_alloc_after_start: %u, "
                      "change_page_alloc_dropped_after_start: %u, "
                      "all_page_alloc_dropped_after_start: %u",
                      instance(),
                      ptr.p->m_change_page_alloc_after_start,
                      ptr.p->m_all_page_alloc_after_start,
                      ptr.p->m_change_page_alloc_dropped_after_start,
                      ptr.p->m_all_page_alloc_dropped_after_start);
  g_eventLogger->info("(%u)change_page_dropped_A_after_start: %u, "
                      "all_page_dropped_A_after_start: %u, "
                      "change_page_dropped_D_after_start: %u, "
                      "all_page_dropped_D_after_start: %u",
                      instance(),
                      ptr.p->m_change_page_dropped_A_after_start,
                      ptr.p->m_all_page_dropped_A_after_start,
                      ptr.p->m_change_page_dropped_D_after_start,
                      ptr.p->m_all_page_dropped_D_after_start);
  g_eventLogger->info("(%u)skip_change_page_lcp_scanned_bit: %u, "
                      "skip_all_page_lcp_scanned_bit: %u, "
                      "skip_change_page_no_change: %u, "
                      "skip_empty_change_page: %u, "
                      "skip_empty_all_page: %u",
                      instance(),
                      ptr.p->m_skip_change_page_lcp_scanned_bit,
                      ptr.p->m_skip_all_page_lcp_scanned_bit,
                      ptr.p->m_skip_change_page_no_change,
                      ptr.p->m_skip_empty_change_page,
                      ptr.p->m_skip_empty_all_page);
  g_eventLogger->info("(%u)record_empty_change_page_A: %u, "
                      "record_late_alloc_change_page_A: %u, "
                      "skip_late_alloc_change_page_D: %u, "
                      "skip_late_alloc_all_page_A: %u, "
                      "skip_late_alloc_all_page_D: %u",
                      instance(),
                      ptr.p->m_record_empty_change_page_A,
                      ptr.p->m_record_late_alloc_change_page_A,
                      ptr.p->m_skip_late_alloc_change_page_D,
                      ptr.p->m_skip_late_alloc_all_page_A,
                      ptr.p->m_skip_late_alloc_all_page_D);
  g_eventLogger->info("(%u)lcp_keep_row_change_pages: %llu, "
                      "lcp_keep_row_all_pages: %llu, "
                      "lcp_keep_delete_row_change_pages: %llu, "
                      "lcp_keep_delete_row_all_pages: %llu, "
                      "lcp_keep_delete_change_pages: %u, "
                      "lcp_keep_delete_all_pages: %u",
                      instance(),
                      ptr.p->m_lcp_keep_row_change_pages,
                      ptr.p->m_lcp_keep_row_all_pages,
                      ptr.p->m_lcp_keep_delete_row_change_pages,
                      ptr.p->m_lcp_keep_delete_row_all_pages,
                      ptr.p->m_lcp_keep_delete_change_pages,
                      ptr.p->m_lcp_keep_delete_all_pages);
}

void
Backup::init_extended_lcp_stat()
{
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, m_lcp_ptr_i);
  ptr.p->m_change_page_alloc_after_start = 0;
  ptr.p->m_all_page_alloc_after_start = 0;
  ptr.p->m_change_page_alloc_dropped_after_start = 0;
  ptr.p->m_all_page_alloc_dropped_after_start = 0;
  ptr.p->m_change_page_dropped_A_after_start = 0;
  ptr.p->m_all_page_dropped_A_after_start = 0;
  ptr.p->m_change_page_dropped_D_after_start = 0;
  ptr.p->m_all_page_dropped_D_after_start = 0;
  ptr.p->m_skip_change_page_lcp_scanned_bit = 0;
  ptr.p->m_skip_all_page_lcp_scanned_bit = 0;
  ptr.p->m_skip_change_page_no_change = 0;
  ptr.p->m_skip_empty_change_page = 0;
  ptr.p->m_skip_empty_all_page = 0;
  ptr.p->m_record_empty_change_page_A = 0;
  ptr.p->m_record_late_alloc_change_page_A = 0;
  ptr.p->m_skip_late_alloc_change_page_D = 0;
  ptr.p->m_skip_late_alloc_all_page_A = 0;
  ptr.p->m_skip_late_alloc_all_page_D = 0;
  ptr.p->m_lcp_keep_delete_row_change_pages = 0;
  ptr.p->m_lcp_keep_delete_row_all_pages = 0;
  ptr.p->m_lcp_keep_delete_change_pages = 0;
  ptr.p->m_lcp_keep_delete_all_pages = 0;
  ptr.p->m_lcp_keep_row_change_pages = 0;
  ptr.p->m_lcp_keep_row_all_pages = 0;
  ptr.p->m_any_lcp_page_ops = false;
}

/**
 * Return values:
 * +1 Page have been scanned
 * -1 Page have not been scanned
 * 0 Page is scanned, so need to check the page index as well.
 */
int
Backup::is_page_lcp_scanned(Uint32 page_id, bool & all_part)
{
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, m_lcp_ptr_i);
  all_part = false;

  if (page_id >= ptr.p->m_lcp_max_page_cnt)
  {
    jam();
    return +1; /* Page will never be scanned */
  }
  Uint32 part_id = hash_lcp_part(page_id);
  if (is_all_rows_page(ptr, part_id))
  {
    jam();
    all_part = true;
  }
  if (!ptr.p->m_is_lcp_scan_active)
  {
    /**
     * LCP scan is already completed.
     */
    jam();
    return +1;
  }
  if (page_id < ptr.p->m_lcp_current_page_scanned)
  {
    jam();
    return +1; /* Page have been scanned in this LCP scan round */
  }
  else if (page_id > ptr.p->m_lcp_current_page_scanned)
  {
    jam();
    return -1; /* Page to be scanned this LCP scan round, not done yet */
  }
  else
  {
    jam();
    return 0; /* Page is currently being scanned. Need more info */
  }
}

void
Backup::update_lcp_pages_scanned(Signal *signal,
                                 Uint32 filePtrI,
                                 Uint32 scanned_pages,
                                 Uint32 & scanGCI,
                                 bool & changed_row_page_flag)
{
  BackupFilePtr filePtr;
  BackupRecordPtr ptr;
  jamEntry();

  c_backupFilePool.getPtr(filePtr, filePtrI);

  OperationRecord & op = filePtr.p->operation;
 
  op.set_scanned_pages(scanned_pages);

  /**
   * scanned_pages also contains the Page number which can be used
   * to deduce the part_id for the page.
   */
  c_backupPool.getPtr(ptr, m_lcp_ptr_i);
  Uint32 part_id = hash_lcp_part(scanned_pages);
  ptr.p->m_lcp_current_page_scanned = scanned_pages;
  get_page_info(ptr,
                part_id,
                scanGCI,
                changed_row_page_flag);
  set_working_file(ptr, part_id, !changed_row_page_flag);
  ptr.p->m_current_data_file_ptr = ptr.p->m_working_data_file_ptr;
  ptr.p->m_working_changed_row_page_flag = changed_row_page_flag;
  ptr.p->m_current_changed_row_page_flag = changed_row_page_flag;
#ifdef DEBUG_EXTRA_LCP
  TablePtr debTabPtr;
  FragmentPtr fragPtr;
  ptr.p->tables.first(debTabPtr);
  debTabPtr.p->fragments.getPtr(fragPtr, 0);
  DEB_EXTRA_LCP(("(%u)LCP scan page tab(%u,%u):%u, part_id: %u, round: %u, %s",
                 instance(),
                 debTabPtr.p->tableId,
                 fragPtr.p->fragmentId,
                 scanned_pages,
                 part_id,
                 0,
                 changed_row_page_flag ?
                     "CHANGED ROWS page" : " ALL ROWS page"));
#endif
}

void 
Backup::OperationRecord::init(const TablePtr & tabPtr)
{
  tablePtr = tabPtr.i;
  maxRecordSize = tabPtr.p->maxRecordSize;
  lcpScannedPages = 0;
}

bool
Backup::OperationRecord::newFragment(Uint32 tableId, Uint32 fragNo)
{
  Uint32 * tmp;
  const Uint32 headSz = (sizeof(BackupFormat::DataFile::FragmentHeader) >> 2);
  const Uint32 sz = headSz + ZRESERVED_SCAN_BATCH_SIZE * maxRecordSize;
  
  ndbrequire(sz < dataBuffer.getMaxWrite());
  if(dataBuffer.getWritePtr(&tmp, sz)) {
    jam();
    BackupFormat::DataFile::FragmentHeader * head = 
      (BackupFormat::DataFile::FragmentHeader*)tmp;

    head->SectionType   = htonl(BackupFormat::FRAGMENT_HEADER);
    head->SectionLength = htonl(headSz);
    head->TableId       = htonl(tableId);
    head->FragmentNo    = htonl(fragNo);
    head->ChecksumType  = htonl(0);

    opNoDone = opNoConf = opLen = 0;
    newRecord(tmp + headSz);
    scanStart = tmp;
    scanStop  = (tmp + headSz);
    
    noOfRecords = 0;
    noOfBytes = 0;
    return true;
  }//if
  return false;
}

bool
Backup::OperationRecord::fragComplete(Uint32 tableId, Uint32 fragNo, bool fill_record)
{
  Uint32 * tmp;
  const Uint32 footSz = sizeof(BackupFormat::DataFile::FragmentFooter) >> 2;
  Uint32 sz = footSz + 1;

  if (fill_record)
  {
    Uint32 * new_tmp;
    if (!dataBuffer.getWritePtr(&tmp, sz))
      return false;
    new_tmp = tmp + sz;

    if ((UintPtr)new_tmp & (sizeof(Page32)-1))
    {
      /* padding is needed to get full write */
      new_tmp += 2 /* to fit empty header minimum 2 words*/;
      new_tmp = (Uint32 *)(((UintPtr)new_tmp + sizeof(Page32)-1) &
                            ~(UintPtr)(sizeof(Page32)-1));
      /* new write sz */
      sz = Uint32(new_tmp - tmp);
    }
  }

  if(dataBuffer.getWritePtr(&tmp, sz)) {
    jam();
    * tmp = 0; // Finish record stream
    tmp++;
    BackupFormat::DataFile::FragmentFooter * foot = 
      (BackupFormat::DataFile::FragmentFooter*)tmp;
    foot->SectionType   = htonl(BackupFormat::FRAGMENT_FOOTER);
    foot->SectionLength = htonl(footSz);
    foot->TableId       = htonl(tableId);
    foot->FragmentNo    = htonl(fragNo);
    foot->NoOfRecords   = htonl(Uint32(noOfRecords)); // TODO
    foot->Checksum      = htonl(0);

    if (sz != footSz + 1)
    {
      tmp += footSz;
      memset(tmp, 0, (sz - footSz - 1) * 4);
      *tmp = htonl(BackupFormat::EMPTY_ENTRY);
      tmp++;
      *tmp = htonl(sz - footSz - 1);
    }

    dataBuffer.updateWritePtr(sz);
    return true;
  }//if
  return false;
}

bool
Backup::OperationRecord::newScan()
{
  Uint32 * tmp;
  ndbrequire(ZRESERVED_SCAN_BATCH_SIZE * maxRecordSize < dataBuffer.getMaxWrite());
  if(dataBuffer.getWritePtr(&tmp, ZRESERVED_SCAN_BATCH_SIZE * maxRecordSize))
  {
    jam();
    opNoDone = opNoConf = opLen = 0;
    newRecord(tmp);
    scanStart = tmp;
    scanStop = tmp;
    return true;
  }//if
  return false;
}

bool
Backup::check_new_scan(BackupRecordPtr ptr, OperationRecord & op)
{
  if (ptr.p->is_lcp() && ptr.p->m_num_lcp_files > 1)
  {
    for (Uint32 i = 0; i < ptr.p->m_num_lcp_files; i++)
    {
      jam();
      BackupFilePtr loopFilePtr;
      c_backupFilePool.getPtr(loopFilePtr, ptr.p->dataFilePtr[i]);
      OperationRecord & loop_op = loopFilePtr.p->operation;
      if (!loop_op.newScan())
      {
        jam();
        return false;
      }
    }
    return true;
  }
  else
  {
    jam();
    return op.newScan();
  }
}

bool
Backup::check_frag_complete(BackupRecordPtr ptr, BackupFilePtr filePtr)
{
  if (ptr.p->is_lcp() && ptr.p->m_num_lcp_files > 1)
  {
    for (Uint32 i = 0; i < ptr.p->m_num_lcp_files; i++)
    {
      jam();
      BackupFilePtr loopFilePtr;
      c_backupFilePool.getPtr(loopFilePtr, ptr.p->dataFilePtr[i]);
      OperationRecord & op = loopFilePtr.p->operation;
      if (((loopFilePtr.p->m_flags &
            Uint32(BackupFile::BF_SCAN_THREAD)) == 0) ||
            op.fragComplete(filePtr.p->tableId,
                            filePtr.p->fragmentNo,
                            c_defaults.m_o_direct))
      {
        jam();
        loopFilePtr.p->m_flags &= ~(Uint32)BackupFile::BF_SCAN_THREAD;
      }
      else
      {
        jam();
        return false;
      }
    }
    return true;
  }
  else
  {
    OperationRecord & op = filePtr.p->operation;
    if (op.fragComplete(filePtr.p->tableId,
                        filePtr.p->fragmentNo,
                        c_defaults.m_o_direct))
    {
      jam();
      filePtr.p->m_flags &= ~(Uint32)BackupFile::BF_SCAN_THREAD;
      return true;
    }
    return false;
  }
}

bool
Backup::check_min_buf_size(BackupRecordPtr ptr, OperationRecord &op)
{
  if (ptr.p->is_lcp() && ptr.p->m_num_lcp_files > 1)
  {
    for (Uint32 i = 0; i < ptr.p->m_num_lcp_files; i++)
    {
      jam();
      Uint32 *tmp = NULL;
      Uint32 sz = 0;
      bool eof = FALSE;
      BackupFilePtr loopFilePtr;
      c_backupFilePool.getPtr(loopFilePtr, ptr.p->dataFilePtr[i]);
      OperationRecord & loop_op = loopFilePtr.p->operation;
      if (!loop_op.dataBuffer.getReadPtr(&tmp, &sz, &eof))
      {
        return false;
      }
    }
    return true;
  }
  else
  {
    jam();
    Uint32 *tmp = NULL;
    Uint32 sz = 0;
    bool eof = FALSE;
    return op.dataBuffer.getReadPtr(&tmp, &sz, &eof);
  }
}

bool
Backup::check_error(BackupRecordPtr ptr, BackupFilePtr filePtr)
{
  if (ptr.p->checkError())
  {
    jam();
    return true;
  }
  if (ptr.p->is_lcp() && ptr.p->m_num_lcp_files > 1)
  {
    for (Uint32 i = 0; i < ptr.p->m_num_lcp_files; i++)
    {
      jam();
      BackupFilePtr loopFilePtr;
      c_backupFilePool.getPtr(loopFilePtr, ptr.p->dataFilePtr[i]);
      if (loopFilePtr.p->errorCode != 0)
      {
        jam();
        return true;
      }
    }
    return false;
  }
  else
  {
    return (filePtr.p->errorCode != 0);
  }
}

void
Backup::OperationRecord::closeScan()
{
  opNoDone = opNoConf = opLen = 0;
}

void 
Backup::OperationRecord::scanConfExtra()
{
  const Uint32 len = Uint32(scanStop - scanStart);
  ndbrequire(len < dataBuffer.getMaxWrite());
  dataBuffer.updateWritePtr(len);
}

void 
Backup::OperationRecord::scanConf(Uint32 noOfOps, Uint32 total_len)
{
  const Uint32 done = Uint32(opNoDone-opNoConf);
  
  ndbrequire(noOfOps == done);
  ndbrequire(opLen == total_len);
  opNoConf = opNoDone;
  
  const Uint32 len = Uint32(scanStop - scanStart);
  ndbrequire(len < dataBuffer.getMaxWrite());
  dataBuffer.updateWritePtr(len);
  noOfBytes += (len << 2);
  m_bytes_total += (len << 2);
  m_records_total += noOfOps;
}

void
Backup::execSCAN_FRAGREF(Signal* signal)
{
  jamEntry();

  ScanFragRef * ref = (ScanFragRef*)signal->getDataPtr();
  
  const Uint32 filePtrI = ref->senderData;
  BackupFilePtr filePtr;
  c_backupFilePool.getPtr(filePtr, filePtrI);

  Uint32 errCode = ref->errorCode;
  if (filePtr.p->errorCode == 0)
  {
    // check for transient errors
    switch(errCode){
    case ScanFragRef::ZSCAN_BOOK_ACC_OP_ERROR:
    case ScanFragRef::NO_TC_CONNECT_ERROR:
    case ScanFragRef::ZTOO_MANY_ACTIVE_SCAN_ERROR:
      jam();
      DEB_LCP(("(%u)execSCAN_FRAGREF(temp error: %u)",
               instance(),
               errCode));
      break;
    case ScanFragRef::TABLE_NOT_DEFINED_ERROR:
    case ScanFragRef::DROP_TABLE_IN_PROGRESS_ERROR:
      jam();
      /**
       * The table was dropped either at start of LCP scan or in the
       * middle of it. We will complete in the same manner as if we
       * got a SCAN_FRAGCONF with close flag set. The idea is that
       * the content of the LCP file in this case is not going to
       * be used anyways, so we just ensure that we complete things
       * in an ordered manner and then the higher layers will ensure
       * that the files are dropped and taken care of.
       *
       * This handling will ensure that drop table can complete
       * much faster.
       */
      DEB_LCP(("(%u)execSCAN_FRAGREF(DROP_TABLE_IN_PROGRESS)", instance()));
      fragmentCompleted(signal, filePtr, errCode);
      return;
    default:
      jam();
      filePtr.p->errorCode = errCode;
    }
  }

  if (filePtr.p->errorCode == 0)
  {
    jam();
    filePtr.p->m_retry_count++;
    if (filePtr.p->m_retry_count == 10)
    {
      jam();
      filePtr.p->errorCode = errCode;
    }
  }

  if (filePtr.p->errorCode != 0)
  {
    jam();
    filePtr.p->m_flags &= ~(Uint32)BackupFile::BF_SCAN_THREAD;
    DEB_LCP(("(%u)execSCAN_FRAGREF(backupFragmentRef)", instance()));
    backupFragmentRef(signal, filePtr);
  }
  else
  {
    jam();

    // retry

    BackupRecordPtr ptr;
    c_backupPool.getPtr(ptr, filePtr.p->backupPtr);
    TablePtr tabPtr;
    FragmentPtr fragPtr;
    if (ptr.p->is_lcp())
    {
      ptr.p->tables.first(tabPtr);
      ndbrequire(filePtr.p->fragmentNo == 0);
      ndbrequire(filePtr.p->tableId == tabPtr.p->tableId);
      tabPtr.p->fragments.getPtr(fragPtr, 0);
      DEB_LCP(("(%u)execSCAN_FRAGREF", instance()));
    }
    else
    {
      ndbrequire(findTable(ptr, tabPtr, filePtr.p->tableId));
      tabPtr.p->fragments.getPtr(fragPtr, filePtr.p->fragmentNo);
    }
    sendScanFragReq(signal, ptr, filePtr, tabPtr, fragPtr,
                    WaitScanTempErrorRetryMillis);
  }
}

void
Backup::execSCAN_FRAGCONF(Signal* signal)
{
  jamEntry();

  CRASH_INSERTION((10017));

  ScanFragConf conf = *(ScanFragConf*)signal->getDataPtr();
  
  const Uint32 filePtrI = conf.senderData;
  BackupFilePtr filePtr;
  c_backupFilePool.getPtr(filePtr, filePtrI);

  OperationRecord & op = filePtr.p->operation;
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, filePtr.p->backupPtr);

  if (c_lqh->handleLCPSurfacing(signal))
  {
    jam();
    TablePtr tabPtr;
    ptr.p->tables.first(tabPtr);
    Dbtup* tup = (Dbtup*)globalData.getBlock(DBTUP, instance());
    op.maxRecordSize = tabPtr.p->maxRecordSize =
      1 + tup->get_max_lcp_record_size(tabPtr.p->tableId);
  }
  op.scanConf(conf.completedOps, conf.total_len);
  if (ptr.p->is_lcp() && ptr.p->m_num_lcp_files > 1)
  {
    jam();
    BackupFilePtr loopFilePtr;
    for (Uint32 i = 1; i < ptr.p->m_num_lcp_files; i++)
    {
      c_backupFilePool.getPtr(loopFilePtr, ptr.p->dataFilePtr[i]);
      OperationRecord & loop_op = loopFilePtr.p->operation;
      loop_op.scanConfExtra();
    }
  }
  const Uint32 completed = conf.fragmentCompleted;
  if(completed != 2) {
    jam();
    checkScan(signal, ptr, filePtr);
    return;
  }//if

  fragmentCompleted(signal, filePtr);
}

void
Backup::fragmentCompleted(Signal* signal,
                          BackupFilePtr filePtr,
                          Uint32 errCode)
{
  jam();

  if(filePtr.p->errorCode != 0)
  {
    jam();
    filePtr.p->m_flags &= ~(Uint32)BackupFile::BF_SCAN_THREAD;
    DEB_LCP(("(%u)fragmentCompleted(backupFragmentRef)", instance()));
    backupFragmentRef(signal, filePtr); // Scan completed
    return;
  }//if
    
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, filePtr.p->backupPtr);

  if (!check_frag_complete(ptr, filePtr))
  {
    jam();
    signal->theData[0] = BackupContinueB::BUFFER_FULL_FRAG_COMPLETE;
    signal->theData[1] = filePtr.i;
    signal->theData[2] = errCode;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal,
                        WaitDiskBufferCapacityMillis, 2);
    return;
  }//if
  OperationRecord & op = filePtr.p->operation;
  if (ptr.p->is_lcp())
  {
    jam();
    ptr.p->m_is_lcp_scan_active = false;
    for (Uint32 i = 0; i < ptr.p->m_num_lcp_files; i++)
    {
      BackupFilePtr loopFilePtr;
      c_backupFilePool.getPtr(loopFilePtr,
                              ptr.p->dataFilePtr[i]);
      loopFilePtr.p->operation.dataBuffer.eof();
    }
    {
      jam();
      TablePtr tabPtr;
      FragmentPtr fragPtr;
      ptr.p->tables.first(tabPtr);
      tabPtr.p->fragments.getPtr(fragPtr, 0);
      DEB_LCP_STAT(("(%u)LCP tab(%u,%u): inserts: %llu, writes: %llu"
                    ", delete_by_row: %llu, delete_by_page: %llu"
                    ", bytes written: %llu, num_files: %u"
                    ", first data file: %u",
               instance(),
               tabPtr.p->tableId,
               fragPtr.p->fragmentId,
               filePtr.p->m_lcp_inserts,
               filePtr.p->m_lcp_writes,
               filePtr.p->m_lcp_delete_by_rowids,
               filePtr.p->m_lcp_delete_by_pageids,
               ptr.p->noOfBytes,
               ptr.p->m_num_lcp_files,
               ptr.p->m_first_data_file_number));
#ifdef DEBUG_LCP_EXTENDED_STAT
      print_extended_lcp_stat();
#endif
      c_tup->stop_lcp_scan(tabPtr.p->tableId, fragPtr.p->fragmentId);
    }
    /* Save errCode for later checks */
    ptr.p->m_save_error_code = errCode;
    ptr.p->slaveState.setState(STOPPING);

    /**
     * Scan is completed, we get the newest GCI involved in the
     * LCP. We update both LQH and ourselves with this value.
     */
    c_lqh->lcp_complete_scan(ptr.p->newestGci);

    /**
     * The actual complete processing is started from checkFile which is
     * called regularly from a CONTINUEB loop. We cannot start the complete
     * processing until all data of the fragment have been sent properly to
     * the disk. checkFile is called from CONTINUEB(START_FILE_THREAD).
     *
     * lcp_start_complete_processing will start by sync:ing UNDO log, sync
     * the page cache and sync:ing the extent pages. When all this is done
     * AND the fragment LCP data files are sync:ed and closed then the
     * LCP is done.
     */
    lcp_start_complete_processing(signal, ptr);
  }
  else
  {
    jam();
    BackupFragmentConf * conf = (BackupFragmentConf*)signal->getDataPtrSend();
    conf->backupId = ptr.p->backupId;
    conf->backupPtr = ptr.i;
    conf->tableId = filePtr.p->tableId;
    conf->fragmentNo = filePtr.p->fragmentNo;
    conf->noOfRecordsLow = (Uint32)(op.noOfRecords & 0xFFFFFFFF);
    conf->noOfRecordsHigh = (Uint32)(op.noOfRecords >> 32);
    conf->noOfBytesLow = (Uint32)(op.noOfBytes & 0xFFFFFFFF);
    conf->noOfBytesHigh = (Uint32)(op.noOfBytes >> 32);
    sendSignal(ptr.p->masterRef, GSN_BACKUP_FRAGMENT_CONF, signal,
	       BackupFragmentConf::SignalLength, JBA);

    ptr.p->m_gsn = GSN_BACKUP_FRAGMENT_CONF;
    ptr.p->slaveState.setState(STARTED);
  }
  return;
}

void
Backup::backupFragmentRef(Signal * signal, BackupFilePtr filePtr)
{
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, filePtr.p->backupPtr);

  ptr.p->m_gsn = GSN_BACKUP_FRAGMENT_REF;

  CRASH_INSERTION((10044));
  CRASH_INSERTION((10045));
  
  BackupFragmentRef * ref = (BackupFragmentRef*)signal->getDataPtrSend();
  ref->backupId = ptr.p->backupId;
  ref->backupPtr = ptr.i;
  ref->nodeId = getOwnNodeId();
  ref->errorCode = filePtr.p->errorCode;
  sendSignal(ptr.p->masterRef, GSN_BACKUP_FRAGMENT_REF, signal,
	     BackupFragmentRef::SignalLength, JBB);
}

void
Backup::checkScan(Signal* signal,
                  BackupRecordPtr ptr,
                  BackupFilePtr filePtr)
{  
  OperationRecord & op = filePtr.p->operation;
  BlockReference lqhRef = 0;
  if (ptr.p->is_lcp())
  {
    lqhRef = calcInstanceBlockRef(DBLQH);
  }
  else
  {
    TablePtr tabPtr;
    ndbrequire(findTable(ptr, tabPtr, filePtr.p->tableId));
    FragmentPtr fragPtr;
    tabPtr.p->fragments.getPtr(fragPtr, filePtr.p->fragmentNo);
    const Uint32 instanceKey = fragPtr.p->lqhInstanceKey;
    lqhRef = numberToRef(DBLQH, instanceKey, getOwnNodeId());
  }
  if (check_error(ptr, filePtr))
  {
    jam();
    /**
     * Close scan
     */
    if (ptr.p->is_lcp())
    {
      DEB_LCP(("(%u) Close LCP scan after receiving error: %u",
              instance(),
              filePtr.p->errorCode));
    }
    op.closeScan();
    ScanFragNextReq * req = (ScanFragNextReq *)signal->getDataPtrSend();
    req->senderData = filePtr.i;
    req->requestInfo = 0;
    ScanFragNextReq::setCloseFlag(req->requestInfo, 1);
    req->transId1 = 0;
    req->transId2 = (BACKUP << 20) + (getOwnNodeId() << 8);
    sendSignal(lqhRef, GSN_SCAN_NEXTREQ, signal, 
	       ScanFragNextReq::SignalLength, JBB);
    return;
  }//if
  if (check_new_scan(ptr, op))
  {
    jam();
    
    ScanFragNextReq * req = (ScanFragNextReq *)signal->getDataPtrSend();
    req->senderData = filePtr.i;
    req->requestInfo = 0;
    req->transId1 = 0;
    req->transId2 = (BACKUP << 20) + (getOwnNodeId() << 8);
    req->batch_size_rows= ZRESERVED_SCAN_BATCH_SIZE;
    req->batch_size_bytes= 0;

    if (ERROR_INSERTED(10039) && 
	filePtr.p->tableId >= 2 &&
	filePtr.p->operation.noOfRecords > 0 &&
        !ptr.p->is_lcp())
    {
      ndbout_c("halting backup for table %d fragment: %d after %llu records",
	       filePtr.p->tableId,
	       filePtr.p->fragmentNo,
	       filePtr.p->operation.noOfRecords);
      memmove(signal->theData+2, signal->theData, 
	      4*ScanFragNextReq::SignalLength);
      signal->theData[0] = BackupContinueB::ZDELAY_SCAN_NEXT;
      signal->theData[1] = filePtr.i;
      sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 
			  300, 2+ScanFragNextReq::SignalLength);
      return;
    }
    if(ERROR_INSERTED(10032))
      sendSignalWithDelay(lqhRef, GSN_SCAN_NEXTREQ, signal, 
			  100, ScanFragNextReq::SignalLength);
    else if(ERROR_INSERTED(10033))
    {
      SET_ERROR_INSERT_VALUE(10032);
      sendSignalWithDelay(lqhRef, GSN_SCAN_NEXTREQ, signal, 
			  10000, ScanFragNextReq::SignalLength);
      
      BackupRecordPtr ptr;
      c_backupPool.getPtr(ptr, filePtr.p->backupPtr);
      AbortBackupOrd *ord = (AbortBackupOrd*)signal->getDataPtrSend();
      ord->backupId = ptr.p->backupId;
      ord->backupPtr = ptr.i;
      ord->requestType = AbortBackupOrd::FileOrScanError;
      ord->senderData= ptr.i;
      sendSignal(ptr.p->masterRef, GSN_ABORT_BACKUP_ORD, signal, 
		 AbortBackupOrd::SignalLength, JBB);
    }
#ifdef ERROR_INSERT
    else if (ERROR_INSERTED(10042) && filePtr.p->tableId ==c_error_insert_extra)
    {
      sendSignalWithDelay(lqhRef, GSN_SCAN_NEXTREQ, signal,
			  10, ScanFragNextReq::SignalLength);
    }
#endif
    else
    {
      /**
       * We send all interactions with bounded delay, this means that we will
       * wait for at most 128 signals before the signal is put into the A-level
       * job buffer. After this we will execute at A-level until we arrive
       * back with a SCAN_FRAGCONF. After SCAN_FRAGCONF we get back to here
       * again, so this means we will execute at least 16 rows before any
       * B-level signals are allowed again. So this means that the LCP will
       * scan at least 16 rows per 128 signals even at complete overload.
       *
       * We will even send yet one more row of 16 rows at A-priority level
       * per 100 B-level signals if we have difficulties in even meeting the
       * minimum desired checkpoint level.
       */
      JobBufferLevel prio_level = JBB;
      if (check_scan_if_raise_prio(signal, ptr))
      {
        OperationRecord & op = filePtr.p->operation;
        bool file_buf_contains_min_write_size =
          check_min_buf_size(ptr, op);

        ScanFragNextReq::setPrioAFlag(req->requestInfo, 1);
        if (file_buf_contains_min_write_size ||
            filePtr.p->m_num_scan_req_on_prioa >= 2 ||
            (filePtr.p->m_num_scan_req_on_prioa == 1 &&
             filePtr.p->m_sent_words_in_scan_batch > MAX_LCP_WORDS_PER_BATCH))
        {
          jam();
          /**
           * There are three reasons why we won't continue executing at
           * prio A level.
           *
           * 1) Last two executions was on prio A, this means that we have now
           *    executed 2 sets of 16 rows at prio A level. So it is time to
           *    give up the prio A level and allow back in some B-level jobs.
           *
           * 2) The last execution at prio A generated more than the max words
           *    per A-level batch, so we get back to a bounded delay signal.
           *
           * 3) We already have a buffer ready to be sent to the file
           *    system. No reason to execute at a very high priority simply
           *    to fill buffers not waiting to be filled.
           */
          filePtr.p->m_sent_words_in_scan_batch = 0;
          filePtr.p->m_num_scan_req_on_prioa = 0;
        }
        else
        {
          jam();
          /* Continue at prio A level 16 more rows */
          filePtr.p->m_num_scan_req_on_prioa++;
          prio_level = JBA;
        }
      }
      else
      {
        jam();
        filePtr.p->m_sent_words_in_scan_batch = 0;
        filePtr.p->m_num_scan_req_on_prioa = 0;
      }
      if (lqhRef == calcInstanceBlockRef(DBLQH) && (prio_level == JBB))
      {
        sendSignalWithDelay(lqhRef, GSN_SCAN_NEXTREQ, signal,
                            BOUNDED_DELAY, ScanFragNextReq::SignalLength);
      }
      else
      {
        /* Cannot send delayed signals to other threads. */
        sendSignal(lqhRef,
                   GSN_SCAN_NEXTREQ,
                   signal,
                   ScanFragNextReq::SignalLength,
                   prio_level);
      }
      /*
        check if it is time to report backup status
      */
      BackupRecordPtr ptr;
      c_backupPool.getPtr(ptr, filePtr.p->backupPtr);
      if (!ptr.p->is_lcp())
      {
        jam();
        checkReportStatus(signal, ptr);
      }
      else
      {
        jam();
      }
    }
    return;
  }//if
  
  filePtr.p->m_sent_words_in_scan_batch = 0; 
  filePtr.p->m_num_scan_req_on_prioa = 0;

  if (ptr.p->is_lcp())
  {
    DEB_EXTRA_LCP(("(%u)newScan false in checkScan", instance()));
  }
  signal->theData[0] = BackupContinueB::BUFFER_FULL_SCAN;
  signal->theData[1] = filePtr.i;
  sendSignalWithDelay(reference(), GSN_CONTINUEB, signal,
                      WaitDiskBufferCapacityMillis, 2);
}

void
Backup::execFSAPPENDREF(Signal* signal)
{
  jamEntry();
  
  FsRef * ref = (FsRef *)signal->getDataPtr();

  const Uint32 filePtrI = ref->userPointer;
  const Uint32 errCode = ref->errorCode;
  
  BackupFilePtr filePtr;
  c_backupFilePool.getPtr(filePtr, filePtrI);

  filePtr.p->m_flags &= ~(Uint32)BackupFile::BF_FILE_THREAD;
  filePtr.p->errorCode = errCode;

  CRASH_INSERTION(10044);
  CRASH_INSERTION(10045);
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, filePtr.p->backupPtr);
  if (ptr.p->is_lcp())
  {
    /**
     * Log in this case for LCPs, Backups should be able to
     * handle out of disk space. LCPs could potentially survive for
     * a while, but will eventually crash or they will hit the
     * infamous 410 condition.
     */
    g_eventLogger->info("LCP got FSAPPENDREF, serious error: error code: %u",
                        errCode);
  }
  checkFile(signal, filePtr);
}

void
Backup::execFSAPPENDCONF(Signal* signal)
{
  jamEntry();

  CRASH_INSERTION((10018));

  //FsConf * conf = (FsConf*)signal->getDataPtr();
  const Uint32 filePtrI = signal->theData[0]; //conf->userPointer;
  const Uint32 bytes = signal->theData[1]; //conf->bytes;
  
  BackupFilePtr filePtr;
  c_backupFilePool.getPtr(filePtr, filePtrI);
  
  OperationRecord & op = filePtr.p->operation;
  
  op.dataBuffer.updateReadPtr(bytes >> 2);

  checkFile(signal, filePtr);
}

/*
  This routine handles two problems with writing to disk during local
  checkpoints and backups. The first problem is that we need to limit
  the writing to ensure that we don't use too much CPU and disk resources
  for backups and checkpoints. For LCPs we use an adaptive algorithm that
  changes the current disk write speed based on how much checkpointing we
  need to do in order to not run out of REDO log.
  Backup writes are added to the total disk write speed we control, but
  backup writes are also separately controlled to avoid that backups take
  up resources that are needed by the REDO log.

  The second problem is that in Linux we can get severe problems if we
  write very much to the disk without synching. In the worst case we
  can have Gigabytes of data in the Linux page cache before we reach
  the limit of how much we can write. If this happens the performance
  will drop significantly when we reach this limit since the Linux flush
  daemon will spend a few minutes on writing out the page cache to disk.
  To avoid this we ensure that a file never have more than a certain
  amount of data outstanding before synch. This variable is also
  configurable.
*/
bool
Backup::ready_to_write(bool ready,
                       Uint32 sz,
                       bool eof,
                       BackupFile *fileP,
                       BackupRecord *ptrP)
{
#if 0
  ndbout << "ready_to_write: ready = " << ready << " eof = " << eof;
  ndbout << " sz = " << sz << endl;
  ndbout << "words this period = " << m_words_written_this_period;
  ndbout << "backup words this period = "
         << m_backup_words_written_this_period;
  ndbout << endl << "overflow disk write = " << m_overflow_disk_write;
  ndbout << endl << "backup overflow disk write = "
         << m_backup_overflow_disk_write;
  ndbout << endl << "Current Millisecond is = ";
  ndbout << NdbTick_CurrentMillisecond() << endl;
#endif

  if (ERROR_INSERTED(10043) && eof)
  {
    /* Block indefinitely without closing the file */
    jam();
    return false;
  }

  if ((ready || eof) &&
      m_words_written_this_period <= m_curr_disk_write_speed &&
      (ptrP->is_lcp() ||
       m_backup_words_written_this_period <= m_curr_backup_disk_write_speed))
  {
    /*
      We have a buffer ready to write or we have reached end of
      file and thus we must write the last before closing the
      file.
      We have already checked that we are allowed to write at this
      moment. We only worry about history of last 100 milliseconds.
      What happened before that is of no interest since a disk
      write that was issued more than 100 milliseconds should be
      completed by now.
    */
    jam();
    int overflow;
    m_monitor_words_written+= sz;
    m_words_written_this_period += sz;
    overflow = m_words_written_this_period - m_curr_disk_write_speed;
    if (overflow > 0)
      m_overflow_disk_write = overflow;
    if (!ptrP->is_lcp())
    {
      m_backup_monitor_words_written += sz;
      m_backup_words_written_this_period += sz;
      overflow = m_backup_words_written_this_period -
                 m_curr_backup_disk_write_speed;
      if (overflow > 0)
        m_backup_overflow_disk_write = overflow;
    }
#if 0
    ndbout << "Will write with " << endl;
    ndbout << endl;
#endif
    return true;
  }
  else
  {
#if 0
    ndbout << "Will not write now" << endl << endl;
#endif
    jam();
    return false;
  }
}

void
Backup::checkFile(Signal* signal, BackupFilePtr filePtr)
{

#ifdef DEBUG_ABORT
  //  ndbout_c("---- check file filePtr.i = %u", filePtr.i);
#endif

  OperationRecord & op = filePtr.p->operation;
  Uint32 *tmp = NULL;
  Uint32 sz = 0;
  bool eof = FALSE;
  bool ready = op.dataBuffer.getReadPtr(&tmp, &sz, &eof);
#if 0
  ndbout << "Ptr to data = " << hex << tmp << endl;
#endif
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, filePtr.p->backupPtr);

  if (ERROR_INSERTED(10036))
  {
    jam();
    filePtr.p->m_flags &= ~(Uint32)BackupFile::BF_FILE_THREAD;
    filePtr.p->errorCode = 2810;
    ptr.p->setErrorCode(2810);

    if(ptr.p->m_gsn == GSN_STOP_BACKUP_REQ)
    {
      jam();
      closeFile(signal, ptr, filePtr);
    }
    return;
  }

  if(filePtr.p->errorCode != 0)
  {
    jam();
    ptr.p->setErrorCode(filePtr.p->errorCode);

    if(ptr.p->m_gsn == GSN_STOP_BACKUP_REQ)
    {
      jam();
      closeFile(signal, ptr, filePtr);
    }

    if (ptr.p->is_lcp())
    {
      jam();
      /* Close file with error - will delete it */
      closeFile(signal, ptr, filePtr);
    }
   
    return;
  }

  if (!ready_to_write(ready,
                      sz,
                      eof,
                      filePtr.p,
                      ptr.p))
  {
    jam();
    signal->theData[0] = BackupContinueB::BUFFER_UNDERFLOW;
    signal->theData[1] = filePtr.i;
    sendSignalWithDelay(reference(), GSN_CONTINUEB, signal,
                        WaitDiskBufferCapacityMillis, 2);
    return;
  }
  else if (sz > 0)
  {
    jam();
#ifdef ERROR_INSERT
    /* Test APPENDREF handling */
    if (filePtr.p->fileType == BackupFormat::DATA_FILE)
    {
      if (ERROR_INSERTED(10045))
      {
        ndbout_c("BF_SCAN_THREAD = %u",
                 (filePtr.p->m_flags & BackupFile::BF_SCAN_THREAD));
      }

      if ((ERROR_INSERTED(10044) &&
           !(filePtr.p->m_flags & BackupFile::BF_SCAN_THREAD)) ||
          (ERROR_INSERTED(10045) && 
           (filePtr.p->m_flags & BackupFile::BF_SCAN_THREAD)))
      { 
        jam();
        ndbout_c("REFing on append to data file for table %u, fragment %u, "
                 "BF_SCAN_THREAD running : %u",
                 filePtr.p->tableId,
                 filePtr.p->fragmentNo,
                 filePtr.p->m_flags & BackupFile::BF_SCAN_THREAD);
        FsRef* ref = (FsRef *)signal->getDataPtrSend();
        ref->userPointer = filePtr.i;
        ref->errorCode = FsRef::fsErrInvalidParameters;
        ref->osErrorCode = ~0;
        /* EXEC DIRECT to avoid change in BF_SCAN_THREAD state */
        EXECUTE_DIRECT(BACKUP, GSN_FSAPPENDREF, signal,
                       3);
        return;
      }
    }
#endif

    if (!eof ||
        !c_defaults.m_o_direct ||
        (sz % 128 == 0) ||
        (filePtr.i != ptr.p->dataFilePtr[0]) ||
        (ptr.p->slaveState.getState() != STOPPING) ||
        ptr.p->is_lcp())
    {
      /**
       * We always perform the writes for LCPs, for backups we ignore
       * the writes when we have reached end of file and we are in the
       * process of stopping a backup (this means we are about to abort
       * the backup and will not be interested in its results.). We avoid
       * writing in this case since we don't want to handle errors for
       * e.g. O_DIRECT calls in this case. However we only avoid this write
       * for data files since CTL files and LOG files never use O_DIRECT.
       * Also no need to avoid write if we don't use O_DIRECT at all.
       */
      jam();
      ndbassert((Uint64(tmp - c_startOfPages) >> 32) == 0); // 4Gb buffers!
      FsAppendReq * req = (FsAppendReq *)signal->getDataPtrSend();
      req->filePointer   = filePtr.p->filePointer;
      req->userPointer   = filePtr.i;
      req->userReference = reference();
      req->varIndex      = 0;
      req->offset        = Uint32(tmp - c_startOfPages); // 4Gb buffers!
      req->size          = sz;
      req->synch_flag    = 0;
    
      sendSignal(NDBFS_REF, GSN_FSAPPENDREQ, signal, 
	         FsAppendReq::SignalLength, JBA);
      return;
    }
  }

  Uint32 flags = filePtr.p->m_flags;
  filePtr.p->m_flags &= ~(Uint32)BackupFile::BF_FILE_THREAD;
  
  ndbrequire(flags & BackupFile::BF_OPEN);
  ndbrequire(flags & BackupFile::BF_FILE_THREAD);
  
  if (ptr.p->is_lcp())
  {
    jam();
    closeFile(signal, ptr, filePtr, false, false);
  }
  else
  {
    jam();
    closeFile(signal, ptr, filePtr);
  }
  return;
}


/****************************************************************************
 * 
 * Slave functionallity: Perform logging
 *
 ****************************************************************************/
void
Backup::execBACKUP_TRIG_REQ(Signal* signal)
{
  /*
  TUP asks if this trigger is to be fired on this node.
  */
  TriggerPtr trigPtr;
  TablePtr tabPtr;
  FragmentPtr fragPtr;
  Uint32 trigger_id = signal->theData[0];
  Uint32 frag_id = signal->theData[1];
  Uint32 result;

  jamEntry();

  c_triggerPool.getPtr(trigPtr, trigger_id);

  c_tablePool.getPtr(tabPtr, trigPtr.p->tab_ptr_i);
  tabPtr.p->fragments.getPtr(fragPtr, frag_id);
  if (fragPtr.p->node != getOwnNodeId()) {

    jam();
    result = ZFALSE;
  } else {
    jam();
    result = ZTRUE;
  }//if
  signal->theData[0] = result;
}

BackupFormat::LogFile::LogEntry *
Backup::get_log_buffer(Signal* signal,
                       TriggerPtr trigPtr, Uint32 sz)
{
  Uint32 * dst;
  if(ERROR_INSERTED(10030))
  {
    jam();
    dst = 0;
  }
  else
  {
    jam();
    FsBuffer & buf = trigPtr.p->operation->dataBuffer;
    ndbrequire(sz <= buf.getMaxWrite());
    if (unlikely(!buf.getWritePtr(&dst, sz)))
    {
      jam();
      dst = 0;
    }
  }

  if (unlikely(dst == 0))
  {
    Uint32 save[TrigAttrInfo::StaticLength];
    memcpy(save, signal->getDataPtr(), 4*TrigAttrInfo::StaticLength);
    BackupRecordPtr ptr;
    c_backupPool.getPtr(ptr, trigPtr.p->backupPtr);
    trigPtr.p->errorCode = AbortBackupOrd::LogBufferFull;
    AbortBackupOrd *ord = (AbortBackupOrd*)signal->getDataPtrSend();
    ord->backupId = ptr.p->backupId;
    ord->backupPtr = ptr.i;
    ord->requestType = AbortBackupOrd::LogBufferFull;
    ord->senderData= ptr.i;
    sendSignal(ptr.p->masterRef, GSN_ABORT_BACKUP_ORD, signal,
               AbortBackupOrd::SignalLength, JBB);

    memcpy(signal->getDataPtrSend(), save, 4*TrigAttrInfo::StaticLength);
    return 0;
  }//if

  BackupFormat::LogFile::LogEntry * logEntry =
    (BackupFormat::LogFile::LogEntry *)dst;
  logEntry->Length       = 0;
  logEntry->TableId      = htonl(trigPtr.p->tableId);

  if(trigPtr.p->event==0)
    logEntry->TriggerEvent= htonl(TriggerEvent::TE_INSERT);
  else if(trigPtr.p->event==1)
    logEntry->TriggerEvent= htonl(TriggerEvent::TE_UPDATE);
  else if(trigPtr.p->event==2)
    logEntry->TriggerEvent= htonl(TriggerEvent::TE_DELETE);
  else {
    ndbout << "Bad Event: " << trigPtr.p->event << endl;
    ndbrequire(false);
  }

  return logEntry;
}

void
Backup::execTRIG_ATTRINFO(Signal* signal) {
  jamEntry();

  CRASH_INSERTION((10019));

  TrigAttrInfo * trg = (TrigAttrInfo*)signal->getDataPtr();

  TriggerPtr trigPtr;
  c_triggerPool.getPtr(trigPtr, trg->getTriggerId());
  ndbrequire(trigPtr.p->event != ILLEGAL_TRIGGER_ID); // Online...

  if(trigPtr.p->errorCode != 0) {
    jam();
    return;
  }//if

  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, trigPtr.p->backupPtr);

  if(ptr.p->flags & BackupReq::USE_UNDO_LOG) {
    if(trg->getAttrInfoType() == TrigAttrInfo::AFTER_VALUES) {
      jam();
      /**
       * Backup is doing UNDO logging and don't need after values
       */
      return;
    }//if
  }
  else {
    if(trg->getAttrInfoType() == TrigAttrInfo::BEFORE_VALUES) {
      jam();
      /**
       * Backup is doing REDO logging and don't need before values
       */
      return;
    }//if
  }

  BackupFormat::LogFile::LogEntry * logEntry = trigPtr.p->logEntry;
  if(logEntry == 0) 
  {
    jam();
    Uint32 sz = trigPtr.p->maxRecordSize;
    logEntry = trigPtr.p->logEntry = get_log_buffer(signal, trigPtr, sz);
    if (unlikely(logEntry == 0))
    {
      jam();
      return;
    }
  } else {
    ndbrequire(logEntry->TableId == htonl(trigPtr.p->tableId));
//    ndbrequire(logEntry->TriggerEvent == htonl(trigPtr.p->event));
  }//if

  const Uint32 pos = logEntry->Length;
  const Uint32 dataLen = signal->length() - TrigAttrInfo::StaticLength;
  memcpy(&logEntry->Data[pos], trg->getData(), dataLen << 2);

  logEntry->Length = pos + dataLen;
}

void
Backup::execFIRE_TRIG_ORD(Signal* signal)
{
  jamEntry();
  FireTrigOrd* trg = (FireTrigOrd*)signal->getDataPtr();

  const Uint32 gci = trg->getGCI();
  const Uint32 trI = trg->getTriggerId();
  const Uint32 fragId = trg->fragId;

  TriggerPtr trigPtr;
  c_triggerPool.getPtr(trigPtr, trI);
  
  ndbrequire(trigPtr.p->event != ILLEGAL_TRIGGER_ID);

  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, trigPtr.p->backupPtr);

  if(trigPtr.p->errorCode != 0) {
    jam();
    SectionHandle handle(this, signal);
    releaseSections(handle);
    return;
  }//if

  if (isNdbMtLqh())
  {
    jam();
    /* This is the decision point for including
     * this row change in the log file on ndbmtd
     */
    TablePtr tabPtr;
    c_tablePool.getPtr(tabPtr, trigPtr.p->tab_ptr_i);
    FragmentPtr fragPtr;
    tabPtr.p->fragments.getPtr(fragPtr, fragId);
    if (fragPtr.p->node != getOwnNodeId()) 
    {
      jam();
      trigPtr.p->logEntry = 0;
      SectionHandle handle(this,signal);
      releaseSections(handle);
      return;
    }
  }

  if (signal->getNoOfSections())
  {
    jam();
    SectionHandle handle(this,signal);
    SegmentedSectionPtr dataPtr[3];
    handle.getSection(dataPtr[0], 0);
    handle.getSection(dataPtr[1], 1);
    handle.getSection(dataPtr[2], 2);
    /**
     * dataPtr[0] : Primary key info
     * dataPtr[1] : Before values
     * dataPtr[2] : After values
     */

    /* Backup is doing UNDO logging and need before values
     * Add 2 extra words to get_log_buffer for potential gci and logEntry length info stored at end.
     */
    if(ptr.p->flags & BackupReq::USE_UNDO_LOG) {
      trigPtr.p->logEntry = get_log_buffer(signal,
                                           trigPtr, dataPtr[0].sz + dataPtr[1].sz + 2);
      if (unlikely(trigPtr.p->logEntry == 0))
      {
        jam();
        releaseSections(handle);
        return;
      }
      copy(trigPtr.p->logEntry->Data, dataPtr[0]);
      copy(trigPtr.p->logEntry->Data+dataPtr[0].sz, dataPtr[1]);
      trigPtr.p->logEntry->Length = dataPtr[0].sz + dataPtr[1].sz;
    }
    //  Backup is doing REDO logging and need after values
    else {
      trigPtr.p->logEntry = get_log_buffer(signal,
                                           trigPtr, dataPtr[0].sz + dataPtr[2].sz + 1);
      if (unlikely(trigPtr.p->logEntry == 0))
      {
        jam();
        releaseSections(handle);
        return;
      }
      copy(trigPtr.p->logEntry->Data, dataPtr[0]);
      copy(trigPtr.p->logEntry->Data+dataPtr[0].sz, dataPtr[2]);
      trigPtr.p->logEntry->Length = dataPtr[0].sz + dataPtr[2].sz;
    }

    releaseSections(handle);
  }

  ndbrequire(trigPtr.p->logEntry != 0);
  Uint32 len = trigPtr.p->logEntry->Length;
  trigPtr.p->logEntry->FragId = htonl(fragId);

  if(gci != ptr.p->currGCP)
  {
    jam();
    trigPtr.p->logEntry->TriggerEvent|= htonl(0x10000);
    trigPtr.p->logEntry->Data[len] = htonl(gci);
    len++;
    ptr.p->currGCP = gci;
  }

  Uint32 datalen = len;
  len += (sizeof(BackupFormat::LogFile::LogEntry) >> 2) - 2;
  trigPtr.p->logEntry->Length = htonl(len);

  if(ptr.p->flags & BackupReq::USE_UNDO_LOG)
  {
    /* keep the length at both the end of logEntry and ->logEntry variable
       The total length of logEntry is len + 2
    */
    trigPtr.p->logEntry->Data[datalen] = htonl(len);
  }

  Uint32 entryLength = len +1;
  if(ptr.p->flags & BackupReq::USE_UNDO_LOG)
    entryLength ++;

  ndbrequire(entryLength <= trigPtr.p->operation->dataBuffer.getMaxWrite());
  trigPtr.p->operation->dataBuffer.updateWritePtr(entryLength);
  trigPtr.p->logEntry = 0;
  
  {
    const Uint32 entryByteLength = entryLength << 2;
    trigPtr.p->operation->noOfBytes     += entryByteLength;
    trigPtr.p->operation->m_bytes_total += entryByteLength;
    trigPtr.p->operation->noOfRecords     += 1;
    trigPtr.p->operation->m_records_total += 1;
  }
}

void
Backup::sendAbortBackupOrd(Signal* signal, BackupRecordPtr ptr, 
			   Uint32 requestType)
{
  jam();
  AbortBackupOrd *ord = (AbortBackupOrd*)signal->getDataPtrSend();
  ord->backupId = ptr.p->backupId;
  ord->backupPtr = ptr.i;
  ord->requestType = requestType;
  ord->senderData= ptr.i;
  NodePtr node;
  for(c_nodes.first(node); node.i != RNIL; c_nodes.next(node)) {
    jam();
    const Uint32 nodeId = node.p->nodeId;
    if(node.p->alive && ptr.p->nodes.get(nodeId)) {
      jam();
      BlockReference ref = numberToRef(BACKUP, instanceKey(ptr), nodeId);
      sendSignal(ref, GSN_ABORT_BACKUP_ORD, signal, 
		 AbortBackupOrd::SignalLength, JBB);
    }//if
  }//for
}

/*****************************************************************************
 * 
 * Slave functionallity: Stop backup
 *
 *****************************************************************************/
void
Backup::execSTOP_BACKUP_REQ(Signal* signal)
{
  jamEntry();
  StopBackupReq * req = (StopBackupReq*)signal->getDataPtr();
  
  CRASH_INSERTION((10020));

  const Uint32 ptrI = req->backupPtr;
  //const Uint32 backupId = req->backupId;
  const Uint32 startGCP = req->startGCP;
  const Uint32 stopGCP = req->stopGCP;

  /**
   * At least one GCP must have passed
   */
  ndbrequire(stopGCP > startGCP);

  /**
   * Get backup record
   */
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);

  ptr.p->slaveState.setState(STOPPING);
  ptr.p->m_gsn = GSN_STOP_BACKUP_REQ;
  ptr.p->startGCP= startGCP;
  ptr.p->stopGCP= stopGCP;

  /**
   * Ensure that any in-flight changes are
   * included in the backup log before
   * dropping the triggers
   *
   * This is necessary as the trigger-drop
   * signals are routed :
   *
   *   Backup Worker 1 <-> Proxy <-> TUP Worker 1..n
   * 
   * While the trigger firing signals are
   * routed :
   *
   *   TUP Worker 1..n   -> Backup Worker 1
   *
   * So the arrival of signal-drop acks
   * does not imply that all fired 
   * triggers have been seen.
   *
   *  Backup Worker 1
   *
   *        |             SYNC_PATH_REQ
   *        V
   *     TUP Proxy
   *    |  | ... |
   *    V  V     V
   *    1  2 ... n        (Workers)
   *    |  |     |
   *    |  |     |
   *   
   *   Backup Worker 1
   */

  Uint32 path[] = { DBTUP, 0 };
  Callback cb = { safe_cast(&Backup::startDropTrig_synced), ptrI };
  synchronize_path(signal,
                   path,
                   cb);
}

void
Backup::startDropTrig_synced(Signal* signal, Uint32 ptrI, Uint32 retVal)
{
  jamEntry();
  /**
   * Get backup record
   */
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);
  
  ndbrequire(ptr.p->m_gsn == GSN_STOP_BACKUP_REQ);
  
  /**
   * Now drop the triggers
   */
  sendDropTrig(signal, ptr);
}

void
Backup::closeFiles(Signal* sig, BackupRecordPtr ptr)
{
  /**
   * Close all files
   */
  BackupFilePtr filePtr;
  int openCount = 0;
  for(ptr.p->files.first(filePtr); filePtr.i!=RNIL; ptr.p->files.next(filePtr))
  {
    if(! (filePtr.p->m_flags & BackupFile::BF_OPEN))
    {
      jam();
      continue;
    }
    
    jam();
    openCount++;
    
    if(filePtr.p->m_flags & BackupFile::BF_CLOSING)
    {
      jam();
      continue;
    }//if

    filePtr.p->operation.dataBuffer.eof();
    if(filePtr.p->m_flags & BackupFile::BF_FILE_THREAD)
    {
      jam();
#ifdef DEBUG_ABORT
      ndbout_c("Close files fileRunning == 1, filePtr.i=%u", filePtr.i);
#endif
    } 
    else 
    {
      jam();
      closeFile(sig, ptr, filePtr);
    }
  }
  
  if(openCount == 0){
    jam();
    closeFilesDone(sig, ptr);
  }//if
}

void
Backup::closeFile(Signal* signal,
                  BackupRecordPtr ptr,
                  BackupFilePtr filePtr,
                  bool prepare_phase,
                  bool remove_flag)
{
  ndbrequire(filePtr.p->m_flags & BackupFile::BF_OPEN);
  ndbrequire(! (filePtr.p->m_flags & BackupFile::BF_OPENING));
  ndbrequire(! (filePtr.p->m_flags & BackupFile::BF_CLOSING));
  filePtr.p->m_flags |= BackupFile::BF_CLOSING;
  
  FsCloseReq * req = (FsCloseReq *)signal->getDataPtrSend();
  req->filePointer = filePtr.p->filePointer;
  req->userPointer = filePtr.i;
  req->userReference = reference();
  req->fileFlag = 0;

  if (prepare_phase)
  {
    jam();
    if (ptr.p->prepareErrorCode)
    {
      jam();
      FsCloseReq::setRemoveFileFlag(req->fileFlag, 1);
    }
  }
  else
  {
    jam();
    if (ptr.p->errorCode)
    {
      jam();
      FsCloseReq::setRemoveFileFlag(req->fileFlag, 1);
    }
  }
  if (remove_flag)
  {
    jam();
    FsCloseReq::setRemoveFileFlag(req->fileFlag, 1);
  }

#ifdef DEBUG_ABORT
  ndbout_c("***** a FSCLOSEREQ filePtr.i = %u flags: %x", 
	   filePtr.i, filePtr.p->m_flags);
#endif
  sendSignal(NDBFS_REF, GSN_FSCLOSEREQ, signal, FsCloseReq::SignalLength, JBA);

}

void
Backup::execFSCLOSEREF(Signal* signal)
{
  jamEntry();
  
  FsRef * ref = (FsRef*)signal->getDataPtr();
  const Uint32 filePtrI = ref->userPointer;
  
  BackupFilePtr filePtr;
  c_backupFilePool.getPtr(filePtr, filePtrI);

  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, filePtr.p->backupPtr);
  
  FsConf * conf = (FsConf*)signal->getDataPtr();
  conf->userPointer = filePtrI;

  const char *file_type_str;
  const char *op_type_str;

  if (ptr.p->errorCode == 0)
  {
    ptr.p->errorCode = ref->errorCode;
  }
  if (filePtr.p->errorCode == 0)
  {
    filePtr.p->errorCode = ref->errorCode;
  }
  if (ptr.p->is_lcp())
  {
    op_type_str = "LCP";
    if (ptr.p->prepareCtlFilePtr[0] == filePtrI ||
        ptr.p->prepareCtlFilePtr[1] == filePtrI)
      file_type_str = "prepare ctl";
    else if (ptr.p->prepareDataFilePtr[0] == filePtrI)
      file_type_str = "prepare data";
    else if (ptr.p->deleteFilePtr == filePtrI)
      file_type_str = "delete file";
    else if (ptr.p->dataFilePtr[0] == filePtrI)
      file_type_str = "data";
    else if (ptr.p->ctlFilePtr == filePtrI)
      file_type_str = "ctl";
    else
    {
      ndbrequire(false);
      file_type_str = NULL;
    }
  }
  else
  {
    op_type_str = "backup";
    if (ptr.p->ctlFilePtr == filePtrI)
      file_type_str = "ctl";
    else if (ptr.p->dataFilePtr[0] == filePtrI)
      file_type_str = "data";
    else if (ptr.p->logFilePtr == filePtrI)
      file_type_str = "log";
    else
    {
      ndbrequire(false);
      file_type_str = NULL;
    }
  }
  g_eventLogger->warning("FSCLOSEREF: errCode: %d, performing %s"
                         " for file type %s, ignoring error",
                         ref->errorCode,
                         op_type_str,
                         file_type_str);
  execFSCLOSECONF(signal);
}

void
Backup::execFSCLOSECONF(Signal* signal)
{
  jamEntry();

  FsConf * conf = (FsConf*)signal->getDataPtr();
  const Uint32 filePtrI = conf->userPointer;
  
  BackupFilePtr filePtr;
  c_backupFilePool.getPtr(filePtr, filePtrI);

#ifdef DEBUG_ABORT
  ndbout_c("***** FSCLOSECONF filePtrI = %u", filePtrI);
#endif

  ndbrequire(filePtr.p->m_flags == (BackupFile::BF_OPEN |
				    BackupFile::BF_CLOSING));

  
  const Uint32 usableBytes = 
    filePtr.p->operation.dataBuffer.getUsableSize() << 2;
  const Uint32 freeLwmBytes = 
    filePtr.p->operation.dataBuffer.getFreeLwm() << 2;

  const BackupFormat::FileType ft = filePtr.p->fileType;

  if (ft == BackupFormat::LOG_FILE ||
      ft == BackupFormat::UNDO_FILE)
  {
    g_eventLogger->info("Backup log buffer report : size %u bytes, "
                        "hwm %u bytes (%u pct)",
                        usableBytes,
                        (usableBytes - freeLwmBytes),
                        ((usableBytes - freeLwmBytes) * 100) / 
                        usableBytes);
  }

  filePtr.p->m_flags &= ~(Uint32)(BackupFile::BF_OPEN |BackupFile::BF_CLOSING);
  filePtr.p->operation.dataBuffer.reset();

  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, filePtr.p->backupPtr);

  if (ptr.p->is_lcp())
  {
    if (ptr.p->prepareDataFilePtr[0] == filePtrI)
    {
      /* Close of prepare data file, error condition */
      jam();
      ndbrequire(ptr.p->prepareState == PREPARE_ABORTING);
      defineBackupRef(signal, ptr, ptr.p->errorCode);
      return;
    }
    else if (ptr.p->prepareCtlFilePtr[0] == filePtrI ||
             ptr.p->prepareCtlFilePtr[1] == filePtrI)
    {
      jam();
      if (ptr.p->prepareState == PREPARE_DROP_CLOSE)
      {
        jam();
        lcp_close_ctl_file_drop_case(signal, ptr);
        return;
      }
      if (ptr.p->prepareState == PREPARE_ABORTING)
      {
        jam();
        defineBackupRef(signal, ptr, ptr.p->errorCode);
        return;
      }
      ndbrequire(ptr.p->prepareState == PREPARE_READ_CTL_FILES);
      lcp_close_prepare_ctl_file_done(signal, ptr);
      return;
    }
    else if (ptr.p->ctlFilePtr == filePtrI)
    {
      jam();
      finalize_lcp_processing(signal, ptr);
      return;
    }
    else if (ptr.p->deleteFilePtr == filePtrI)
    {
      jam();
      lcp_close_ctl_file_for_rewrite_done(signal, ptr, filePtr);
      return;
    }
    else if ((ptr.p->dataFilePtr[0] == filePtrI) ||
             (ptr.p->dataFilePtr[1] == filePtrI) ||
             (ptr.p->dataFilePtr[2] == filePtrI) ||
             (ptr.p->dataFilePtr[3] == filePtrI) ||
             (ptr.p->dataFilePtr[4] == filePtrI) ||
             (ptr.p->dataFilePtr[5] == filePtrI) ||
             (ptr.p->dataFilePtr[6] == filePtrI) ||
             (ptr.p->dataFilePtr[7] == filePtrI))
    {
      jam();
      ndbrequire(filePtr.p->m_flags == 0);
      ndbrequire(ptr.p->m_num_lcp_data_files_open > 0);
      ptr.p->m_num_lcp_data_files_open--;
      if (ptr.p->m_num_lcp_data_files_open > 0)
      {
        jam();
        DEB_EXTRA_LCP(("(%u) Closed LCP data file, still waiting for %u files",
                       instance(),
                       ptr.p->m_num_lcp_data_files_open));
        return;
      }
      lcp_close_data_file_conf(signal, ptr);
      return;
    }
    else
    {
      ndbrequire(false);
    }
  }
  /* Backup closing files */
  closeFiles(signal, ptr);
}

void
Backup::closeFilesDone(Signal* signal, BackupRecordPtr ptr)
{
  jam();
  /* Record end-of-backup */
  ndbrequire(is_backup_worker());
  //ndbassert(Backup::g_is_backup_running); /* !set on error paths */
  Backup::g_is_backup_running = false;

  //error when do insert footer or close file
  if(ptr.p->checkError())
  {
    StopBackupRef * ref = (StopBackupRef*)signal->getDataPtrSend();
    ref->backupPtr = ptr.i;
    ref->backupId = ptr.p->backupId;
    ref->errorCode = ptr.p->errorCode;
    ref->nodeId = getOwnNodeId();
    sendSignal(ptr.p->masterRef, GSN_STOP_BACKUP_REF, signal,
             StopBackupConf::SignalLength, JBB);

    ptr.p->m_gsn = GSN_STOP_BACKUP_REF;
    ptr.p->slaveState.setState(CLEANING);
    return;
  }

  StopBackupConf* conf = (StopBackupConf*)signal->getDataPtrSend();
  conf->backupId = ptr.p->backupId;
  conf->backupPtr = ptr.i;

  BackupFilePtr filePtr;
  if(ptr.p->logFilePtr != RNIL)
  {
    ptr.p->files.getPtr(filePtr, ptr.p->logFilePtr);
    conf->noOfLogBytes= Uint32(filePtr.p->operation.noOfBytes);     // TODO
    conf->noOfLogRecords= Uint32(filePtr.p->operation.noOfRecords); // TODO
  }
  else
  {
    conf->noOfLogBytes= 0;
    conf->noOfLogRecords= 0;
  }

  sendSignal(ptr.p->masterRef, GSN_STOP_BACKUP_CONF, signal,
	     StopBackupConf::SignalLength, JBB);
  
  ptr.p->m_gsn = GSN_STOP_BACKUP_CONF;
  ptr.p->slaveState.setState(CLEANING);
}

/*****************************************************************************
 * 
 * Slave functionallity: Abort backup
 *
 *****************************************************************************/
/*****************************************************************************
 * 
 * Slave functionallity: Abort backup
 *
 *****************************************************************************/
void
Backup::execABORT_BACKUP_ORD(Signal* signal)
{
  jamEntry();
  AbortBackupOrd* ord = (AbortBackupOrd*)signal->getDataPtr();

  const Uint32 backupId = ord->backupId;
  const AbortBackupOrd::RequestType requestType = 
    (AbortBackupOrd::RequestType)ord->requestType;
  const Uint32 senderData = ord->senderData;
  
#ifdef DEBUG_ABORT
  ndbout_c("******** ABORT_BACKUP_ORD ********* nodeId = %u", 
	   refToNode(signal->getSendersBlockRef()));
  ndbout_c("backupId = %u, requestType = %u, senderData = %u, ",
	   backupId, requestType, senderData);
  dumpUsedResources();
#endif

  BackupRecordPtr ptr;
  if(requestType == AbortBackupOrd::ClientAbort) {
    if (getOwnNodeId() != getMasterNodeId()) {
      jam();
      // forward to master
#ifdef DEBUG_ABORT
      ndbout_c("---- Forward to master nodeId = %u", getMasterNodeId());
#endif
      BlockReference ref = numberToRef(BACKUP, UserBackupInstanceKey, 
                                       getMasterNodeId());
      sendSignal(ref, GSN_ABORT_BACKUP_ORD, 
		 signal, AbortBackupOrd::SignalLength, JBB);
      return;
    }
    jam();
    for(c_backups.first(ptr); ptr.i != RNIL; c_backups.next(ptr)) {
      jam();
      if(ptr.p->backupId == backupId && ptr.p->clientData == senderData) {
        jam();
	break;
      }//if
    }//for
    if(ptr.i == RNIL) {
      jam();
      return;
    }//if
  } else {
    if (c_backupPool.findId(senderData)) {
      jam();
      c_backupPool.getPtr(ptr, senderData);
    } else { 
      jam();
#ifdef DEBUG_ABORT
      ndbout_c("Backup: abort request type=%u on id=%u,%u not found",
	       requestType, backupId, senderData);
#endif
      return;
    }
  }//if
  
  ptr.p->m_gsn = GSN_ABORT_BACKUP_ORD;
  const bool isCoordinator = (ptr.p->masterRef == reference());
  
  bool ok = false;
  switch(requestType){

    /**
     * Requests sent to master
     */
  case AbortBackupOrd::ClientAbort:
    jam();
    // fall through
  case AbortBackupOrd::LogBufferFull:
    jam();
    // fall through
  case AbortBackupOrd::FileOrScanError:
    jam();
    ndbrequire(isCoordinator);
    ptr.p->setErrorCode(requestType);
    if(ptr.p->masterData.gsn == GSN_BACKUP_FRAGMENT_REQ)
    {
      /**
       * Only scans are actively aborted
       */
      abort_scan(signal, ptr);
    }
    return;
    
    /**
     * Requests sent to slave
     */
  case AbortBackupOrd::AbortScan:
    jam();
    ptr.p->setErrorCode(requestType);
    return;
    
  case AbortBackupOrd::BackupComplete:
    jam();
    cleanup(signal, ptr);
    return;
  case AbortBackupOrd::BackupFailure:
  case AbortBackupOrd::BackupFailureDueToNodeFail:
  case AbortBackupOrd::OkToClean:
  case AbortBackupOrd::IncompatibleVersions:
#ifndef VM_TRACE
  default:
#endif
    ptr.p->setErrorCode(requestType);
    ptr.p->masterData.errorCode = requestType;
    ok= true;
  }
  ndbrequire(ok);
  
  ptr.p->masterRef = reference();
  ptr.p->nodes.clear();
  ptr.p->nodes.set(getOwnNodeId());


  ptr.p->stopGCP= ptr.p->startGCP + 1;
  sendStopBackup(signal, ptr);
}


void
Backup::dumpUsedResources()
{
  jam();
  BackupRecordPtr ptr;

  for(c_backups.first(ptr); ptr.i != RNIL; c_backups.next(ptr)) {
    ndbout_c("Backup id=%u, slaveState.getState = %u, errorCode=%u",
	     ptr.p->backupId,
	     ptr.p->slaveState.getState(),
	     ptr.p->errorCode);

    TablePtr tabPtr;
    for(ptr.p->tables.first(tabPtr);
	tabPtr.i != RNIL;
	ptr.p->tables.next(tabPtr)) {
      jam();
      for(Uint32 j = 0; j<3; j++) {
	jam();
	TriggerPtr trigPtr;
	if(tabPtr.p->triggerAllocated[j]) {
	  jam();
	  c_triggerPool.getPtr(trigPtr, tabPtr.p->triggerIds[j]);
	  ndbout_c("Allocated[%u] Triggerid = %u, event = %u",
		 j,
		 tabPtr.p->triggerIds[j],
		 trigPtr.p->event);
	}//if
      }//for
    }//for
    
    BackupFilePtr filePtr;
    for(ptr.p->files.first(filePtr);
	filePtr.i != RNIL;
	ptr.p->files.next(filePtr)) {
      jam();
      ndbout_c("filePtr.i = %u, flags: H'%x ",
	       filePtr.i, filePtr.p->m_flags);
    }//for
  }
}

void
Backup::cleanup(Signal* signal, BackupRecordPtr ptr)
{
  TablePtr tabPtr;
  ptr.p->tables.first(tabPtr);
  cleanupNextTable(signal, ptr, tabPtr);
}

void
Backup::release_tables(BackupRecordPtr ptr)
{
  TablePtr tabPtr;
  /* Clear backupPtr before releasing */
  for (ptr.p->tables.first(tabPtr);
       tabPtr.i != RNIL;
       ptr.p->tables.next(tabPtr))
  {
    jam();
    tabPtr.p->fragments.release();
    jamLine(tabPtr.p->tableId);
    removeTableMap(tabPtr, ptr.i, tabPtr.p->tableId);
  }
  while (ptr.p->tables.releaseFirst());
}

void
Backup::cleanupNextTable(Signal *signal, BackupRecordPtr ptr, TablePtr tabPtr)
{
  if (tabPtr.i != RNIL)
  {
    jam();
    tabPtr.p->fragments.release();
    for(Uint32 j = 0; j<3; j++) {
      jam();
      TriggerPtr trigPtr;
      if(tabPtr.p->triggerAllocated[j]) {
        jam();
	c_triggerPool.getPtr(trigPtr, tabPtr.p->triggerIds[j]);
	trigPtr.p->event = ILLEGAL_TRIGGER_ID;
        tabPtr.p->triggerAllocated[j] = false;
      }//if
      tabPtr.p->triggerIds[j] = ILLEGAL_TRIGGER_ID;
    }//for
    {
      BackupLockTab *req = (BackupLockTab *)signal->getDataPtrSend();
      req->m_senderRef = reference();
      req->m_tableId = tabPtr.p->tableId;
      req->m_lock_unlock = BackupLockTab::UNLOCK_TABLE;
      req->m_backup_state = BackupLockTab::CLEANUP;
      req->m_backupRecordPtr_I = ptr.i;
      req->m_tablePtr_I = tabPtr.i;
      sendSignal(DBDICT_REF, GSN_BACKUP_LOCK_TAB_REQ, signal,
                 BackupLockTab::SignalLength, JBB);
      return;
    }
  }

  BackupFilePtr filePtr;
  for(ptr.p->files.first(filePtr);filePtr.i != RNIL;ptr.p->files.next(filePtr))
  {
    jam();
    ndbrequire(filePtr.p->m_flags == 0);
    filePtr.p->pages.release();
  }//for

  while (ptr.p->files.releaseFirst());
  release_tables(ptr);
  while (ptr.p->triggers.releaseFirst());
  ptr.p->backupId = ~0;
  
  /*
    report of backup status uses these variables to keep track
    if files are used
  */
  ptr.p->ctlFilePtr = ptr.p->logFilePtr = ptr.p->dataFilePtr[0] = RNIL;

  if(ptr.p->checkError())
    removeBackup(signal, ptr);
  else
  {
    /*
      report of backup status uses these variables to keep track
      if backup ia running and current state
    */
    ptr.p->m_gsn = 0;
    ptr.p->masterData.gsn = 0;
    c_backups.release(ptr);
  }
}


void
Backup::removeBackup(Signal* signal, BackupRecordPtr ptr)
{
  jam();
  
  FsRemoveReq * req = (FsRemoveReq *)signal->getDataPtrSend();
  req->userReference = reference();
  req->userPointer = ptr.i;
  req->directory = 1;
  req->ownDirectory = 1;
  FsOpenReq::setVersion(req->fileNumber, 2);
  FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_CTL);
  FsOpenReq::v2_setSequence(req->fileNumber, ptr.p->backupId);
  FsOpenReq::v2_setNodeId(req->fileNumber, getOwnNodeId());
  sendSignal(NDBFS_REF, GSN_FSREMOVEREQ, signal, 
	     FsRemoveReq::SignalLength, JBA);
}

void
Backup::execFSREMOVEREF(Signal* signal)
{
  jamEntry();
  FsRef * ref = (FsRef*)signal->getDataPtr();
  const Uint32 ptrI = ref->userPointer;

  FsConf * conf = (FsConf*)signal->getDataPtr();
  conf->userPointer = ptrI;
  execFSREMOVECONF(signal);
}

void
Backup::execFSREMOVECONF(Signal* signal)
{
  jamEntry();

  FsConf * conf = (FsConf*)signal->getDataPtr();
  const Uint32 ptrI = conf->userPointer;
  
  /**
   * Get backup record
   */
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, ptrI);

  if (ptr.p->is_lcp())
  {
    jam();
    lcp_remove_file_conf(signal, ptr);
    return;
  }
  /*
    report of backup status uses these variables to keep track
    if backup ia running and current state
  */
  ptr.p->m_gsn = 0;
  ptr.p->masterData.gsn = 0;
  c_backups.release(ptr);
}

/**
 * Description of how LCP works and its principles
 * -----------------------------------------------
 *
 * Introduction of Partial LCP
 * ...........................
 * In MySQL Cluster 7.6 partial LCPs was introduced. This means that each
 * LCP doesn't record every single row in the system. It only records a subset
 * of all rows + all changed rows since the last partial LCP.
 *
 * This allows partial LCPs to complete more quickly than a full LCP, and
 * the REDO log to be trimmed more frequently.
 *
 * We keep track of changed rows by using the GCI stored on each row. We
 * know which GCI that was fully part of the previous LCP. Thus if the
 * previous LCP contained all changes up to and including GCI = 77 this
 * means that the new LCP will only need to record changes from GCI = 78
 * and onwards.
 * 
 * There is some complication that comes from deletions here.
 * The restore of the system uses a number of partial LCPs back in time.
 * For a specific rowid this means that there is a first partial LCP file
 * where it is recorded. It can either be restored with an inserted value as
 * part of this LCP, if it isn't then the rowid will be empty after executing
 * this first partial LCP, further partial LCPs might add it.
 *
 * In the following LCPs this rowid will only be part of the LCP if it has
 * changed since the last LCP. This is absolutely no problem if the row
 * has been inserted or updated since then the row exists and its value will
 * be recorded in the LCP as a changed row.
 *
 * At start of a partial LCP we decide the number of parts to checkpoint
 * fully, currently we have divided the page id range into 2048 different
 * parts. We can checkpoint anything between 1 to 2048 parts in one
 * partial LCP, this is driven by data size of fragment and change percentage.
 *
 * Definition: The set of rows where we record all rows are called ALL ROWS.
 * The set of rows where we only record the changed rows. We call this
 * CHANGE ROWS.
 *
 * The ALL ROWS parts are the same as used in earlier versions of MySQL
 * Cluster, and are a 'state dump' containing INSERT BY ROWID operations.
 * Each row existing at start of LCP will be recorded in pages belonging
 * to this part.
 *
 * The CHANGED ROWS parts are a kind of operation log with WRITE BY ROWID
 * and DELETE BY ROWID and DELETE BY PAGEID (DELETE by ROWID for all rows in a
 * page) operations which must be applied.
 *
 * For partial LCP we divide up the range of pages into 2048 parts using a hash
 * function on page id. For a specific LCP we will have one set of parts that
 * are checkpointed in the ALL ROWS part and the rest is checkpointed in the
 * CHANGE ROWS part.
 *
 * To restore we need to perform the following for each of the 2048 parts.
 * 1) Find the last LCP where this part belonged to the ALL ROWS part.
 * 2) Restore this part from this LCP.
 * 3) For each of the LCP after that up to the LCP we are restoring we will
 *    restore the CHANGE ROWS part for this part.
 *
 * This means that at restore we will never need to go further back than the
 * oldest ALL ROWS part we have remaining which is restorable. This is
 * important understanding for knowing when LCP files can be deleted.
 *
 * More definitions
 * ----------------
 * Rowid: Each row has a rowid (page id and page index) which is a local key
 * to the fixed size part of the row. The fixed part of the row has references
 * to the variable sized part and it also has a reference to the disk part of
 * the row.
 *
 * Page Map: The page map takes a rowid as input and gives back the page
 * pointer to this page. The page map also knows if the page id is empty
 * and it is also used to keep some page state after page has been deleted
 * as discussed further below.
 *
 * Disk reference: This is the reference in the main memory part of the row
 * that refers to the disk part of the row. Currently this reference is
 * located in the fixed size part of the row and the disk data part is a
 * fixed size part.
 *
 * Row content: This is the actual values of the attributes in this row.
 * 
 * Row structure:
 * -------------------------------------------
 * | Fixed size part in main memory          |
 * | Tuple header + Fixed size attributes +  |
 * | disk reference + variable size reference|
 * | + NULL bits                             |
 * ------------------------------------------
 * 
 * ------------------------------------------
 * | Var part in main memory                |
 * | Header part + variable sized attributes|
 * | + dynamic attributes                   |
 * |-----------------------------------------
 *
 * ------------------------------------------
 * | Fixed size part on disk page           |
 * | Header part + Fix size disk attributes |
 * ------------------------------------------
 *
 * The Fixed main memory part header contains also GCI, Checksum. Also the
 * disk part contains a GCI and a reference to the main memory part.
 *
 * Purpose of LCP
 * ..............
 * The purpose of the LCP (Local checkpoint) is to ensure that we can cut the
 * REDO log tail which otherwise grow to infinity. We do this by performing
 * a regular LCP of each fragment.
 *
 * NDB contains both main memory data and disk data. The disk data part is
 * recovered by using a No Steal approach. This means that only committed
 * data is ever sent to the pages written to disk. To support this we use an
 * UNDO log to ensure that the disk data is possible to restore to the
 * exact state it had at the starting point of the LCP.
 *
 * The main memory part of the row content is stored in the LCP files
 * generated by the LCP. The disk part is stored in its position in the
 * disk pages by flushing the pages in memory to disk for the disk parts.
 *
 * Observation 1:
 * Only committed rows are written into any LCP for both main memory data and
 * disk data. Thus after restoring an LCP we only need to roll forward using
 * a REDO log.
 *
 * Observation 2:
 * Given that the LCP maintains the exact row structure at the start of the
 * LCP the REDO log can be a logical log (only logging actions (INSERT, DELETE,
 * UPDATE) and the values changed).
 *
 * The REDO log is mainly operating with primary keys, but to ensure that
 * we synchronize the rowids on different nodes all INSERTs must also log
 * the rowid they are inserted into.
 *
 * Observation 3:
 * Given that the REDO log is a logical log it is location and replication
 * independent. This means that we can restore the LCP stored locally
 * and then apply a mix of the local REDO log and REDO logs from other
 * nodes in the same node group. Using remote REDO logs is a principle we
 * have decided to abandon and instead fully rely on the ability to
 * synchronize data nodes at node restarts.
 *
 * An LCP is performed per fragment. A table consists of multiple fragments
 * that can be checkpointed in parallel in different LDMs.
 *
 * Only one LCP per fragment per LDM instance is currently executed. However
 * we allow for the prepare phase of the next LCP (opening files and preparing
 * the LCP execution) to proceed in parallel to the currently running
 * LCP. In addition the deletion of old LCP files is a background process
 * going on in the background to both of these processes.
 *
 *     Need of LCP_SKIP bit for inserts
 *     ................................
 * Performing a checkpoint for disk pages means simply writing any pages that
 * got dirty since the last checkpoint. It is a bit more involved to perform
 * checkpoints (LCPs) for main memory data. For main memory data we only
 * checkpoint the rows and not pages. This gives us the opportunity to write
 * less data in the main memory checkpoints since we don't have to save the
 * entire page where the changes were done.
 *
 * The idea for LCPs is that we need a LCP to contain exactly the rows present
 * at the start of the LCP. This means that we set an LCP_SKIP bit on rows
 * that are inserted during LCPs to avoid putting those rows into the LCP when
 * we pass by them in the LCP scan.
 *
 * The requirement to have exactly the correct set of rows that existed at
 * start of LCP comes from that we need the reference from main-memory rows
 * to disk rows to be correct. The content of the main memory row and
 * disk data row must not be exactly synchronized but if the row exists
 * in main memory the referred disk row must exist in disk pages and
 * vice versa.
 *
 * Tables that don't have disk data don't need this requirement, but we
 * treat them the same way.
 *
 * The row content in both the disk data and the main memory data can be
 * newer than the data at the start of the LCP, but not older.
 *
 * The reason is that the REDO log or other synchronisation efforts will
 * ensure that all updates from before the LCP and until the restoration
 * point is reapplied, so we will eventually have the correct data in
 * each row at the restoration point.
 *
 *     Need of LCP keep list for deletes
 *     .................................
 * Similarly we use an LCP keep list to record deleted rows such that we
 * record them in the LCP. We use this list to give those recordings a
 * higher priority since we will release the rowid immediately after
 * committing the row.
 *
 * These two principles ensure that the LCP will contain exactly the same
 * set of rows as we had at the start of the LCP. The row data might
 * differ from what it looked at the start of the LCP. This is however
 * of no significance since the REDO log will ensure that we will
 * after recovery have the correct state of the data.
 *
 * As an example a row with a certain rowid can be deleted before LCP scans
 * it and then the row will be sent to the LCP keep list. Later a new row
 * will be inserted while the LCP scan still hasn't arrived at this rowid
 * and then the INSERT will set the LCP_SKIP to ensure that the new row
 * is ignored in this rowid.
 *
 * This leads to the following observations.
 *
 * Observation 1:
 * ..............
 * In an LCP there will only be one row existing for a specific rowid. There
 * will never be two rows with the same rowid in an LCP.
 *
 * Proof:
 * ------
 * If two rows existed there must have been a delete followed by an insert
 * in the LCP scan. The delete will ensure that the first row with this
 * rowid will exist in LCP and the LCP_SKIP bit will ensure that the
 * second row with this rowid will not exist in the LCP.
 *
 * Observation 2:
 * ..............
 * It isn't allowed for any updates to change the disk reference. The disk
 * reference must be stable over a set of LCPs.
 *
 * Proof:
 * ------
 * If an update did change the disk reference the restored main memory row
 * would refer to the wrong disk data part which would not work.
 *
 * The above is the essential requirement on any LCP that is used in a
 * restore of NDB tables. We formulate it here as a theorem.
 *
 * Theorem 1:
 * ..........
 * An LCP used in the recovery of NDB must meet the following requirements.
 * 1) All committed rows that are present at start of LCP (defined as the
 *    the time when we write the marker in the UNDO log of disk data) must
 *    all be part of LCP and no other rows may be present in the LCP.
 * 2) All links from main memory to disk data and vice versa must be
 *    consistent in a checkpoint.
 * 3) The row data must be the same as at the time of the start of the LCP
 *    OR at a time after the start of the LCP.
 *
 * Proof:
 * ------
 * A proof for this is presented in the Ph.D thesis of Mikael Ronström,
 * Design and Modelling of a Parallel Data Server for Telecom Applications,
 * 1998 in chapter 9.2.1. The bearing principle is that the logical REDO
 * log will replay all transactions from a point which is certain to be
 * before the start of the LCP, thus all updates, inserts and deletes
 * happening after the start of the LCP is certain to be part of the
 * REDO log execution.
 *
 * A paper at VLDB 2005 also presents some of the proof behind this in
 * the paper called "Recovery principles in MySQL Cluster 5.1". This paper
 * also takes into account the use of disk data parts.
 *
 * While applying the REDO log the following events can happen to a row that
 * existed in LCP. Note that the start of LCP is not known when executing
 * the REDO log, so this is a theoretical proof of the validity of the
 * algorithm, not how it works.
 *
 * 1) Delete of row before start of LCP, no problems to execute. There are
 *    two variants, the row is not inserted again, in this case the row
 *    won't be in the LCP and no REDO log record will reinsert it. In case
 *    the row is later reinserted the REDO log record will be executed as
 *    part of recovery and the row is thus certain to be part of the
 *    restorable state.
 *
 *    This operation can discover that the row doesn't exist, but this is
 *    ok and can only occur before start of LCP.
 *
 * 2) Delete of row after start of LCP, this is ok since the row will exist
 *    before the delete as it existed at start of LCP.
 *
 * 3) Update before start of LCP. This is ok, it will restore a value to
 *    the record that might not be the end state, but if not so there
 *    will be more updates recorded in the REDO log. The important principle
 *    here is that the REDO log application must be idempotent. Since the
 *    REDO log simply restores the values of the attributes it is
 *    idempotent. It is possible to construct a REDO log that contains
 *    operations also (like add one to column a). This would not work in
 *    this algorithm since we don't have exact control how exactly we
 *    restore a row state. Our algorithm requires an idempotent REDO log.
 *
 *    This update might discover that the row doesn't exist, this can only
 *    occur before start of LCP so it is safe to ignore the REDO log record.
 *
 * 4) Update after start of LCP. The value this REDO log entry restores
 *    could already be in the LCP since we don't care if the LCP records a
 *    newer record than at the start of the LCP.
 *
 * 5) Insert before start of LCP. The REDO log execution will perform this if
 *    the row doesn't exist. If it existed already we are certain that this
 *    insert is before start of LCP and it can be safely ignored.
 *
 * 6) Insert after start of LCP, the row won't be in LCP, so will always work
 *    fine.
 *
 * So what we see here is that the REDO log can sometimes bring us backwards
 * in the row history, but it will eventually bring us forward in row history
 * to the desired state at a particular GCP (global checkpoint).
 *
 *     Handling deletes for partial LCPs
 *     .................................
 * The problematic part is deletes of a row. This could result in 4 different
 * scenarios.
 *
 *     Special handling with reuse of rowids for partial LCPs
 *     ......................................................
 * 1) A first partial LCP has inserted row A into rowid X, after the LCP the
 *    row is deleted and then the delete is followed by a new insert of row B
 *    into rowid X. In this case the LCP will attempt to restore a row where
 *    a row already exists in this rowid. Here we need to remove the old row
 *    first before inserting the new row to ensure that the primary key hash
 *    index is correct.
 *
 *    To handle this case properly we always need to drop the row in the
 *    row id position if the primary key has changed from the previous
 *    LCP to this LCP. One manner is to always drop it first and then
 *    reinsert it even if it is the same row.
 *
 *     Special case of handling deleted rowids with GCI > 0
 *     ....................................................
 * 2) A first partial LCP has inserted row A into rowid X, after that the
 *    row is deleted. At the next LCP this will be recorded as a DELETE
 *    by ROWID. So when applying this partial LCP the rowid X will be
 *    set to an empty rowid and the record A will be deleted as part of
 *    executing that partial LCP. So after executing that partial LCP the
 *    row will not exist.
 *
 *     Special case of empty rowids (GCI = 0) for newly allocated pages
 *     ...............................................................
 * 3) The first partial LCP records the rows within page Y, after the LCP
 *    but before the new LCP the page is dropped, after the drop it is
 *    allocated again. When the LCP starts the page has at least 1 row in
 *    it which has been reinserted.
 *
 *    The remainder of the rows in the page can have GCI = 0, these rows
 *    need to have a DELETE by ROWID in the LCP. This DELETE by ROWID might
 *    encounter a row that actually didn't exist, so DELETE by ROWID at
 *    restore must be able to handle that the row didn't exist when we
 *    try to delete it.
 *
 *    Special case of empty page slot at start of LCP
 *    ...............................................
 * 4) At start of the LCP the page slot is free, in this case we record
 *    the entire page as deleted, we call this DELETE by PAGEID. In this
 *    case restore will delete all rows in this position. This only needs
 *    to happen if the page exists when restoring, if the page slot is
 *    empty when this is reached, then we can ignore the DELETE by PAGEID
 *    since it is already handled.
 *
 *    We only record DELETE by PAGEID for pages that are part of CHANGE
 *    ROWS.
 *
 *    We record this information by setting a flag on the page that says
 *    LCP_EMPTY_PAGE_FLAG. This says that the page is now allocated, but
 *    at start of the LCP scan it was empty, so when we reach this
 *    page we will see this state and record a DELETE by PAGEID.
 *    Similarly if we come by an empty page slot that haven't got the
 *    LCP_SCANNED bit set in the page map as described in 5) we will
 *    also record this as DELETE by PAGEID.
 *
 *    Problematic case of Drop page during LCP scan
 *    .............................................
 * 5) In this case the page exists at start of LCP, for ALL ROWS this is not
 *    a problem, those rows that was deleted since the start of LCP is put
 *    into the LCP through LCP keep lists. However for CHANGE ROWS we need to
 *    record DELETE by ROWID for each row that has GCI = 0 or GCI > scanGCI
 *    for LCP. We cannot drop the page without recording this information
 *    since there is no way to recreate this information.
 *
 *    To solve this issue we use the LCP keep list to enter the information
 *    about rowids that we need to issue DELETE by ROWID for. This means that
 *    we are able to drop the page immediately and store its state information
 *    needed for LCP elsewhere.
 *
 *    When dropping the page we will immediately scan the page and each
 *    rowid that has GCI = 0 or GCI >= lcpScanGCI will be recorded into the
 *    LCP keep list. However for efficiency reasons we will record multiple
 *    rowids in each row in the LCP keep list. So each record in the
 *    LCP keep list will either contain a full row as usual OR it will
 *    contain an indicator of containing dropped rowids, the number of
 *    dropped rowids in this row and the rowids in an array (each rowid
 *    consumes 2 words).
 *
 *    However there is one more problem related to this. Once the page has
 *    been dropped before LCP scan has reached it, it can be reinserted
 *    again. Now if this page as mentioned above belongs to the CHANGE ROWS
 *    category then as explained in 4) we want to record it as a
 *    DELETE by PAGEID. However in this case this is not correct, the page
 *    has already been scanned by the LCP.
 *
 *    We can avoid problems with future updates on the page by setting the
 *    LCP_SKIP bit on the page when it is reinserted, but we also need some
 *    information to avoid inserting the DELETE by PAEGID into the LCP.
 *
 *    The place where we retain information about dropped pages is the page
 *    map. We have 2 32-bit words in memory for each page in the current
 *    set of pages. These 2 words are handled by the DynArr256 data structure.
 *    We need to temporarily use this place to store information about pages
 *    dropped during LCP scan in the CHANGE ROW part.
 *
 *    To describe how this happens requires a description of the Page Map and
 *    its workings and how we make use of it in this case.
 *
 *    Description of Fragment Page Map
 *    ................................
 *
 *    ------------------
 *    | Page Map Head  |
 *    ------------------
 *    The page map head is a normal head of a doubly linked list that contains
 *    the logical page id of the first free logical page id slot.
 *
 *    The entries in the page map is different dependent on if the slot is
 *    free or not. First we'll show the non-empty variant (actually the
 *    second slot can be uninitialised in which case the DynArr256 will return
 *    RNIL (RNIL cannot be set in any manner since we cannot use page ids
 *    higher than or equal to RNIL & 0x3fffffff).
 *    ------------------------------------------
 *    | Physical page id  | Bit 31 set any rest|
 *    ------------------------------------------
 *    Now the empty variant
 *
 *     Next reference              Previous reference
 *    -----------------------------------------------------------
 *    | Bit 31 set, logicalPageId | Bit 31 set logicalPageId    |
 *    -----------------------------------------------------------
 *    So the first position uses bit 31 to indicate that the logical
 *    page id position is empty, the other 31 bits in this position is used
 *    to point to the next free logical page id. If all 30 lowest bits
 *    are set in the logical page id it is FREE_PAGE_RNIL. FREE_PAGE_RNIL
 *    means that there is no next logical page id.
 *
 *    The previous position also contains a reference to a logical page id,
 *    in this case the previous free logical page id. If there is no free
 *    previous logical page id then this is set to FREE_PAGE_RNIL as
 *    well. Bit 31 is set in both words when the entry is free.
 *
 *    The reason that Bit 31 is set in both words is to ensure that when
 *    we scan the fragment page map at drop fragment to release pages
 *    that we don't release any pages from the second position. The
 *    iterator delivers each word back and we don't keep track of which
 *    position is which, so we need bit 31 to be set at all times for
 *    the second position.
 *
 *    The page map is only growing, the only manner to get rid of it is to
 *    either drop the table or restart the node. At restart the page map
 *    starts from scratch again.
 *
 *    The conclusion is that the page map is a place where we can store
 *    the special information about that a logical page id has been dropped
 *    as part of the CHANGE ROWS category and it needs no more LCP scanning
 *    even if reinserted. So by setting a bit here we can use this information
 *    to avoid inserting a DELETE by PAGEID into the LCP and we can set some
 *    some proper information on the page to ensure that we skip this page
 *    later in the LCP scan (obviously also need the LCP scan to reset this
 *    bit then).
 *
 *    We also use bit 30 in the second word to indicate what the page state
 *    was at the start of the previous LCP. This enables us to decide what
 *    to do in those situations when we find that the page or row is not
 *    used at start of this LCP.
 *
 *    Solution:
 *    ---------
 *    We will use bit 30 in the first word of the page map to indicate this
 *    special page state. This has the effect that we can at most have
 *    2^30 pages in one page map. This limits the size of the main memory
 *    fixed part to 32 TBytes. If this becomes a problem then we need to
 *    use 64-bit page id as well and that means that the page map will
 *    contain 2 64-bit words instead and thus the problem will be resolved.
 *    We call this bit the LCP_SCANNED_BIT. Bit 31 in the first word is
 *    already used to store the FREE_PAGE_BIT which indicates if the page
 *    entry is free or in use, if FREE_PAGE_BIT the two words are used
 *    as next and prev of a linked list of free page ids for the fragment.
 *
 *    Obviously we need to ensure that during all page map operations that
 *    we take care in handling this special page state.
 *
 *    Note: For the pages in the ALL ROWS catoegory where re we record all
 *    rows we write all the rowids existing at start of LCP, this means that
 *    a rowid in these parts that isn't recorded as an empty rowid by
 *    definition. For parts where only record changes we have to ensure that
 *    we get the same set of rows after executing all changes, so we need to
 *    record all changes, both new rowids and deleted rowids and updates of
 *    row content of rows.
 *
 *    We will also use the 1 free bit in the second word in the page map.
 *    This bit will be used to store the LCP state at the previous LCP.
 *    When we reach a page in the LCP scan we will set the state of the last
 *    LCP based on the current state and of oter flags as described below.
 *
 *    The state that no page map entry exists is also a valid state, this
 *    state indicates that the previous LCP state was that the page was
 *    released and that the current state is empty state as well and that
 *    that the state of the LCP_SCANNED_BIT is 0.
 *
 *    So we have three bits in the page map:
 *    LCP_SCANNED_BIT: Indicates that we have taken care of everything
 *    related to LCP scans for this page in this LCP.
 *    FREE_PAGE_BIT: Indicates that the current state of the page is free.
 *    LAST_LCP_FREE_BIT: Set to 1 indicates that the last LCP state is D
 *    and set to 0 indicates that the last LCP state is A. This is bit 30
 *    of the second word in the page map.
 *
 *     Detailed discussion of each case of release/allocation of page
 *     ..............................................................
 *
 * A stands for an allocation event, D stands for an release event (drop page)
 * [AD].. stands for a A followed by D but possibly several ones and possibly
 * also no events.
 * E stands for empty set of events (no A or D events happened in the period).
 *
 * Case 1: Dropped before start of last LCP and dropped at start of this LCP
 * Desired action for ALL ROWS pages: Ignore page
 * Desired action for CHANGED ROWS pages: Ignore page, technically acceptable
 * to record it as DELETE by PAGEID as well.
 *
 * D  LCP_Start(n)   [AD]..    LCP_Start(n+1)  E           LCP_End(n+1) (1)
 * D  LCP_Start(n)   [AD]..    LCP_Start(n+1)  A           LCP_End(n+1) (2)
 * D  LCP_Start(n)   [AD]..    LCP_Start(n+1)  [AD]..A     LCP_End(n+1) (3)
 *
 * (1) is found by the empty page when the LCP scan finds it and the
 *     LCP_SCANNED_BIT is not set. Thus ALL ROWS pages knows to ignore the
 *     the page. CHANGED ROWS pages can ignore it by looking at the state
 *     of the last LCP and notice that the page was dropped also then and
 *     thus the page can be ignored.
 *     
 *     In this case we set the state of last LCP to D in the LCP scan.
 *
 * (2) is found by discovering that page->is_page_to_skip_lcp() is true.
 *     The LCP_SCANNED_BIT isn't set in this case when the LCP scan reaches
 *     it. Thus ALL ROWS pages can ignore it. CHANGED ROWS pages will ignore
 *     it after checking the state of the last LCP.
 *
 *     In this case we need to keep the keep the state of last LCP until the
 *     LCP scan has reached the page. When LCP scan reaches the page we will
 *     set the state of last LCP to D when page->is_page_to_skip_lcp() is
 *     true.
 *
 * (3) is found by discovering that LCP_SCANNED_BIT is set since the first
 *     D event after LCP start handled the page and handled any needed
 *     DELETE by PAGEID. After discovering this one needs to reset the
 *     LCP_SCANNED_BIT again. At the first A the page_to_skip_lcp bit
 *     was set, but the first D issued a DELETE BY PAGEID and dropped
 *     the page and to flag that the LCP scan was handled the
 *     LCP_SCANNED_BIT was set.
 *
 *     We read the old last LCP state and set the new last LCP state when
 *     reaching the first D event after start of LCP. The
 *     page->is_page_to_skip_lcp() flag will assist in determining what
 *     the state at start of LCP was.
 *
 * Case 2: Dropped before start of last LCP and allocated at start of LCP.
 *
 * Desired action for ALL ROWS pages: Any rows with committed data at start
 * of LCP should be recorded as INSERTs into the LCP.
 *
 * Desired action for CHANGED ROWS pages: Any rows with committed data at
 * start of LCP should be recorded as WRITEs into the LCP. All other rows
 * should be ignored, technically acceptable behaviour is to issue
 * DELETE by ROWID for those rows that should be ignored as well.
 * 
 * D  LCP_Start(n)   [AD].. A  LCP_Start(n+1)  E           LCP_End(n+1) (1)
 * D  LCP_Start(n)   [AD].. A  LCP_Start(n+1)  D           LCP_End(n+1) (2)
 * D  LCP_Start(n)   [AD].. A  LCP_Start(n+1)  [DA].. D    LCP_End(n+1) (3)
 *
 * (1) is found by that the page exists when being scanned, no LCP_SCANNED_BIT
 *     is set and also not the page to skip lcp flag is set. Individual rows
 *     can have their LCP_SKIP flag set. All rows with committed data AND not
 *     LCP_SKIP flag set will be recorded. All rows with LCP_SKIP flag set
 *     will be ignored for ALL ROWS pages and will be ignored for CHANGED ROWS
 *     pages based on the last LCP state. Rows without committed data will be
 *     ignored for ALL ROWS pages and will be ignored based on the last LCP
 *     state for CHANGED ROWS pages.
 *
 *     When we are done executing a page for the LCP scan we can set the
 *     last LCP state to A.
 *
 * (2) is found when releasing the page. Before page is released it will have
 *     its rows deleted, for each row that is deleted and wasn't already
 *     deleted since start of LCP we will record the row using the LCP keep
 *     list and also setting LCP_SKIP flag on the row. When releasing the
 *     page we can ignore it based on knowledge of the last LCP state.
 *
 *     In this we set the last LCP state and also read it when reaching the
 *     D event. This event can even occur while we're in the middle of
 *     scanning the page for the LCP.
 *
 * (3) is found by discovering that the LCP_SCANNED_BIT is set. This is set
 *     by the first D event after start of LCP after handling the page as
 *     in (2).
 *
 *     Last LCP state already set in the first D event after start of LCP.
 * 
 * Case 3: Allocated before start of last LCP and dropped at start of this LCP
 *
 * Desired action for ALL ROWS pages: Page ignored
 *
 * Desired action for CHANGED ROWS pages: DELETE by PAGEID recorded in LCP
 *
 * A  LCP_Start(n) D [AD]..    LCP_Start(n+1)  E           LCP_End(n+1) (1)
 * A  LCP_Start(n) D [AD]..    LCP_Start(n+1)  A           LCP_End(n+1) (2)
 * A  LCP_Start(n) D [AD]..    LCP_Start(n+1)  [AD].. A    LCP_End(n+1) (3)
 *
 * Here we will take the same action for all cases independent of if we know
 * state of the last LCP or not since the state was allocated before and thus
 * we need to record the change in state.
 *
 * (1) is found by empty page slot and no LCP_SCANNED_BIT set and not skip
 *     flag set on page. For ALL ROWS pages we will simply ignore those
 *     pages. For CHANGED ROWS pages we will record DELETE by PAGEID based
 *     on the state of the last LCP.
 * (2) is found by discovering page->is_page_to_skip_lcp() is true when LCP
 *     scan reaches it. For ALL ROWS pages this means we can ignore it, for
 *     CHANGED ROWS pages we record it as DELETE by PAGEID based on the state
 *     of the last LCP.
 * (3) is found by discovering the LCP_SCANNED_BIT set which was set when the
 *     first D event after start of LCP was found. When this first D event
 *     occurred we handled the page as in (1) followed by setting the
 *     LCP_SCANNED_BIT.
 *
 * The same principles for handling last LCP state exists here as for Case 1.
 *
 * Case 4: Allocated before start of last LCP and allocated before start
 *         of this LCP
 *
 * Desired action for ALL ROWS pages: Record all rows with committed data at
 * start of LCP. Ignore all rows without committed data at start of LCP.
 *
 * Desired action for CHANGED ROWS pages: Record all rows with committed data
 * at start of LCP. Record all rows without committed data at start of LCP as
 * DELETE by ROWID.
 *
 * A  LCP_Start(n)   [DA]..    LCP_Start(n+1)  E           LCP_End(n+1) (1)
 * A  LCP_Start(n)   [DA]..    LCP_Start(n+1)  D           LCP_End(n+1) (2)
 * A  LCP_Start(n)   [DA]..    LCP_Start(n+1)  [DA].. D    LCP_End(n+1) (3)
 *
 * (1) is found by an existing page without LCP_SCANNED_BIT set and without
 *     the page to skip flag set on the page. We will check row by row if the
 *     row is to be copied to LCP.
 *
 *     If a row exists at start of LCP then it will be recorded in the LCP,
 *     either at LCP scan time or at first delete after the start of the LCP.
 *     When the first delete have occurred then we set the LCP_SKIP flag on
 *     the row to indicate that the row has already been processed for this
 *     LCP. The handling here is the same for ALL ROWS pages and for CHANGED
 *     ROWS pages.
 *
 *     If a row didn't exist at start of LCP then we will ignore it for ALL
 *     ROWS pages and we will record a DELETE by ROWID for CHANGED ROWS
 *     pages. We discover this as part of LCP scan for rows not inserted
 *     again before the LCP scan reaches them. For rows that are inserted
 *     after start of LCP we will mark them with LCP_SKIP flag for ALL ROWS
 *     pages. For CHANGED ROWS pages we could record the DELETE by ROWID
 *     immediately, but there is no safe space to record this information.
 *     So instead we mark the row with LCP_DELETE to flag to the LCP scan
 *     that this row needs to generate a DELETE by ROWID.
 *
 * (2) is found when releasing a page, at this point the page has already
 *     recorded everything for ALL ROWS pages. We indicate this by setting
 *     LCP_SCANNED_BIT on the page.
 *
 *     However for CHANGED ROWS pages we can still have a set of rowids that
 *     was empty at start of LCP that we need to record before moving on.
 *     We scan the page before moving on, we ignore rows that have the
 *     LCP_SKIP flag set and rows that have rowGCI < scanGCI which indicates
 *     that they were empty also at last LCP. All other rows we generate a
 *     DELETE by ROWID for. Also here we set the LCP_SCANNED_BIT after
 *     doing this.
 *
 * (3) is found by LCP_SCANNED_BIT set when LCP scan reaches it. Any A or D
 *     event after the first D event will be ignored since LCP_SCANNED_BIT
 *     is set.
 *
 * The same principles for handling last LCP state exists here as for Case 2.
 *
 *     Requirement to record number of pages at start of LCP
 *     .....................................................
 * For partial LCPs we record the number of pages existing in the whole
 * fragment at the start of the partial LCP, this has the effect that during
 * restore we can safely ignore all LCP records on rowids with higher page id
 * than the recorded number of pages. They could never be part of the LCP even
 * if they are part of earlier LCPs.
 *
 * Let's look at an example here. Each page can be sparse or full, it doesn't
 * matter for the description, we need to ensure that the restore can recover
 * the correct set of rows.
 * 
 * LCP 1: Contains 17 pages (rowids from page 0 to 16 included)
 * LCP 2: Contains 13 pages
 * LCP 3: Contains 14 pages
 *
 * When restoring LCP 3 we make use also of parts from LCP 1 and LCP 2.
 * We start by applying the LCP 1 for rowids in page 0 to 13. Next when
 * start applying LCP 2 we need to perform DELETE by ROWID for all rows
 * page id 13. We know that all rowids from page id 13 have either
 * GCI = 0 or a GCI > lcpScanGci which makes them recorded as changes
 * in LCP 3.
 *
 * If we had not recorded the number of pages in LCPs we would not be
 * able to know that rows in page id 14 through 16 was deleted since
 * the LCP scan would not see them since they were not part of the
 * pages scanned during LCP (simply because the pages no longer existed).
 *
 *
 *     Multiple LCP files to save disk space
 *     .....................................
 * Using partial LCP it is essential to be able to drop files as early as
 * possible. If an LCP file contain too many parts fully written then the
 * file needs to be retained although most of its data is no longer useful.
 *
 * To avoid this we cap the number of parts we use for large fragments
 * in each file and use a multi-file implementation of each partial LCP.
 *
 * What we do here is that we divide the LCP of each fragment into several
 * files. We will write each of those files in sequential order. Assume that
 * we have 2048 parts and that this LCP is to record 256 of those parts starting
 * at part 100. Assume that we divide this LCP into 4 files.
 *
 * The first file will record all rows from part 100-163, the second will
 * contain all rows from part 164-228, the third file will contain all
 * rows from part 229-292 and the fourth and last file will contain
 * all rows from part 293-356.
 *
 * The rows from the LCP keep list is written into the file currently
 * used.
 *
 * Changed rows are written to any of the files. But we choose to write
 * them to the first file. The reason is that this means that the biggest
 * file in the LCP will be removed first and thus it is the most efficient
 * algorithm to save disk space.
 *
 * It is a bit complicated to understand to prove that this brings about
 * an LCP that can be correctly restored. We prove it in a number of
 * steps before proving the theorem for Partial LCPs.
 *
 * Corollary 1:
 * ............
 * For each LCP part we always start by applying an LCP where all rows
 * of the part is recorded. Then we will execute the change parts of
 * all LCPs thereafter until the last.
 *
 * Proof:
 * This is the intended recovery algorithm, so proof is not really
 * needed. Proof is only required to prove that this recovers a
 * proper LCP according to Theorem 1 above.
 *
 * Case 1:
 * Assume that the row existed at the time of the first LCP used in
 * restore and is kept all the way until the last LCP, updates can
 * occur.
 *
 * Case 2:
 * Assume that the row was inserted after initial LCP and is kept
 * until the last LCP.
 *
 * Case 3:
 * Assume that the row existed at the time of the first LCP but has
 * been deleted before the final LCP.
 *
 * Case 4:
 * Assume that the row didn't exist at the first LCP and did not
 * exist at the time of the last LCP.
 *
 * Case 4 is obviously ok, no LCP has recorded anything regarding
 * this row, so it cannot be a problem.
 *
 * Case 1 means that the row is restored in first LCP, if any changes
 * has occurred before the last LCP they will be recorded in any of
 * the LCP preceding the last LCP or in the last LCP itself. It
 * could contain a newer value if the last LCP had changes that
 * occurred after start of the LCP. Thus the row is present with
 * same or newer data as it should be according to Theorem 1.
 *
 * Case 2 means that the row was not present in the first LCP.
 * It must have been inserted in either of the following LCPs
 * or the last LCP and since it will be marked with a higher GCI
 * when inserted it will be part of the next LCP after being
 * inserted, similary any updates will be recorded in some LCP if
 * it happens before or during the last LCP. Thus the row exists
 * after applying rows according to Corollary 1 such that Theorem 1
 * holds true.
 *
 * Finally Case 3 have inserted the row as part of the first LCP. The
 * row could have been written by the LCP keep list in this first LCP.
 * However when the row is deleted the GCI of the row will be set to
 * to a GCI higher than the GCI of the first LCP and this ensures that
 * the rowid is recorded in LCP as DELETE by ROWID. Finally if the
 * entire page have been removed before the last LCP we will record
 * this in the last LCP and this means that we will ignore the row
 * that exists in the first LCP restored since we know that not any
 * rows with that rowid is present in the LCP.
 *
 * This means that we have proven that the LCP also in case 3 fits
 * with Theorem 1 in that the row is certain to not be part of the
 * LCP restored.
 *
 * Thus all cases have been proven and Corollary 1 is proven to be
 * a correct restore method for LCPs with Partial LCPs.
 *
 * Corollary 2:
 * ............
 * The LCP keep list can be recorded in any LCP file in the case where
 * multiple files are used to record an LCP.
 *
 * Proof:
 * The record in the LCP from a LCP keep list will always be overwritten
 * or ignored by the following LCPs. The reason is simply that the GCI of
 * the delete is higher than LCP scan GCI of the current LCP. Thus the
 * next LCP will either overwrite this record with a DELETE by ROWID or
 * the record will be ignored by the next LCP since the entire page has
 * been dropped or the rowid will be overwritten by another row that
 * reused the rowid of the deleted row.
 *
 * So thus it is safe to store these LCP keep list items as they come
 * and record them in any list. Obviously all the files of the last
 * LCP will be kept and applied as part of restore.
 *
 * Corollary 3:
 * ............
 * When we remove a file from an LCP we could not be interested in any
 * of the change rows from this LCP. We are only interested in the
 * parts where we have recorded all rows.
 *
 * Proof:
 * We will only remove the oldest LCP files at any time. Thus when we
 * remove a file from an LCP we are sure that all the files from the
 * previous LCP is already deleted. This means that the LCP from where
 * we delete files can only be used to restore the all rows part as
 * described in Corollary 1. Thus we will always ignore all parts
 * with changed rows for an LCP where we are about to delete a file.
 *
 * Theorem 2:
 * ----------
 * The following algorithm will be applied using multiple files.
 * If we want to divide the parts where we record all rows into multiple
 * files we do so in the following manner:
 * 1) In the first file we will record up to 1/8th of the parts. We will
 * also record all changed rows for parts where we are not recording
 * all rows. In addition LCP keep rows are recorded as they arrive.
 * 2) In the following files we will record also all rows for up to 1/8th
 * of the parts. Also LCP keep rows for those as they arrive.
 *
 * Proof:
 * ------
 * Corollary 2 shows that it is correct to record LCP keep rows as they
 * arrive in any of the files.
 * Corollary 3 shows that the any algorithm to select where to record
 * changed rows is correct, in particular this shows that the selected
 * variant to record all in the first file is correct.
 * Corollary 1 shows that the restore algorithm for this type of LCP
 * works as desired.
 *
 * Observation 2:
 * --------------
 * Given that we need two different mechanisms to deduce if a page should
 * be skipped when LCP scanned (is_page_to_skip_lcp() through state on
 * page and lcp_scanned_bit set in page map) this means that both of
 * those need to be checked to see if a row is in remaining LCP set
 * that is used to decide whether to set LCP_SKIP bit on the row.
 *
 * The is_page_to_skip_lcp() flag on page is set when a page as first
 * alloc/release page event after start of LCP scan is allocated. After
 * this the page can be released and if so the last LCP state of the
 * page will be updated and the lcp scanned bit will be set.
 *
 * Similarly if the page is released as the first page event after
 * start of LCP scan we will also update the last LCP state and
 * next set the lcp scanned bit. So when we see a lcp scanned bit we
 * need never do anything more during the LCP scan, we only need to
 * reset the bit.
 *
 * Lemma 1:
 * --------
 * Based on theorem 2 we deduce that each LCP requires a LCP control
 * file that contains at least the following information.
 *
 * MaxGciCompleted:
 * This is the GCI where which we have all changes for in the LCP. The
 * LCP can also contain changes for MaxGciCompleted + 1 and
 * MaxGciCompleted + 2 and beyond.
 *
 * MaxPageCount:
 * This is the number of pages existing (with rowids) in the LCP which
 * is recorded at the start of the partial LCP.
 * 
 * A list of part ranges (one part range per file) and the file numbers.
 * This is recorded using the following variables in the LCP control file.
 *
 * MaxPartPairs:
 * This is the maximum number of LCPs that can constitute a recoverable
 * checkpoints. Thus an LCP control file can write at most this many
 * parts. Currently this number is set to 2048.
 *
 * NumPartPairs:
 * This is the number of files used in the restore of this LCP, there is
 * one part range per file.
 *
 * MaxNumberDataFiles:
 * This is the maximum number of files used, it is used to calculate the
 * file numbers based on a number of files (NumPartPairs) and the
 * parameter LastDataFileNumber.
 *
 * LastDataFileNumber:
 * The last LCP file, this will be the final file restored in a restore
 * situation.
 *
 * An array of pairs (startPart, numParts) where the last records the
 * last LCP file and the first records the first file to start restoring
 * from.
 *
 * In addition we record the following information in the LCP control
 * file.
 *
 * Checksum:
 * To verify the content of the LCP control file.
 *
 * TableId:
 * Table id of the checkpointed fragment.
 *
 * FragmentId:
 * Fragment id of the checkpointed fragment.
 *
 * LcpId:
 * The global LcpId this LCP belongs to.
 *
 * LocalLcpId:
 * If part of global LCP it is 0, otherwise it is 1, 2, 3 and so forth
 * for a local LCP executed without control of DIH.
 *
 * In addition the LCP control file contains a file header as all LCP
 * files and backup files. The most important information here is the
 * version number of the partial LCP changes as such and the version
 * number that wrote this file. This is important for any upgrade
 * scenarios.
 *
 * LCPs and Restarts:
 * ------------------
 * Partial LCP is developed to store less information in LCPs and also
 * that LCPs can run faster. When LCPs complete faster that means that
 * we can cut the REDO log much sooner.
 *
 * However we still need to make a full checkpoint as part of a restart.
 * We will describe the implications this has for various types of
 * restarts.
 *
 * System Restart:
 * ...............
 * No real implication, we have ensured that doing a full checkpoint is
 * still divided into separate files to ensure that we save disk space.
 * There is no updates ongoing during this LCP so this LCP will simply
 * write the changed contents while executing the REDO log.
 *
 * Node restart:
 * .............
 * This restart depends to a great extent on how long time the node
 * was dead, if it was dead for a long time it will have a lot more
 * to write in a LCP than otherwise.
 *
 * Initial node restart:
 * .....................
 * This is the trickiest problem to solve. Using partial LCP we aim for
 * LCPs to complete in 5-10 minutes, but writing the initial LCP after
 * synching the data with the live node might take many hours if the
 * node contains terabytes of data.
 *
 * We solve this by running local LCPs before we become part of the
 * global LCP protocol. DIH won't know about these LCPs but it doesn't
 * really matter, we can make use of it if the node crashes during
 * restart although DIH didn't know about it. But more importantly
 * as soon as we participate in the first global LCP we can run that
 * LCP much faster since we already have logged all rows, so we only
 * need to record the changes since the last local LCP in the first
 * global LCP.
 *
 * The protocol used to tell the starting node about state of fragments
 * is called COPY_ACTIVEREQ. This is received 2 times per fragment
 * per node restart. The first one says that we have completed the
 * synchronisation. We will use this first signal to put the fragment
 * in queue for running an LCP.
 *
 * When all fragments have been synchronised then DIH will start the
 * second phase. In this phase each fragment will start using the
 * REDO log as preparation for the first LCP.
 *
 * Note that a local LCP cannot be used to restore the database on
 * its own. It requires either a node synchronization as part of node
 * restart which works fine as the rowids are synchronized one by one
 * and there might be unneeded work done if the live node uses a GCI
 * from DIH, but it will still be correct.
 *
 * It can also be restored in a system restart by using REDO logs from
 * other nodes, we can avoid applying REDO logs we don't need since we
 * know what GCP we have completely recorded in the LCP. The proof of
 * why applying REDO logs will restore a consistent database still
 * holds.
 *
 * Obviously if as part of recovery we are told to execute the REDO log
 * from GCI 77 to 119 and we know that the LCP is completed for GCI
 * GCI 144 then we can completely skip the part where we execute the
 * REDO log for that fragment as part of the recovery. Later it will
 * be synched up in this case using a live node.
 *
 * Local LCPs during restart
 * .........................
 * When we receive the first COPY_ACTIVEREQ in DBLQH we will start a
 * new local LCP. This will insert an UNDO_LOCAL_LCP_FIRST into the
 * UNDO log. This means that we can move the UNDO log forward, we
 * still need to retain all UNDO log records from the previous LCP,
 * and the one before that since we cannot be certain that the previous
 * LCP actually completed.
 *
 * During Local LCP we cannot insert one more UNDO_LOCAL_LCP_FIRST again
 * until we have completed a Local LCP of each and every fragment to be
 * restored.
 *
 * So what this means is that we will start running a Local LCP as part
 * of the synchronisation with the live node. It is possible to run an
 * LCP for an individual fragment several times during this round of
 * LCP. But we need to complete the Local LCP before allowing the
 * first COPY_ACTIVEREQ in the second phase to continue. If we didn't
 * do this we would run a much bigger chance of running out of UNDO
 * log. In some cases we might still run out of UNDO log and in this
 * case we will ensure that the LCP gets higher priority and that the
 * synchronisation process is blocked temporarily. We will do this
 * when certain thresholds in UNDO log usage is reached.
 *
 * We will allow for two choices in how we perform Local LCPs. We will
 * perform 1 Local LCP for all node restarts before we allow the
 * REDO logging to be activated (activated by COPY_ACTIVEREQ in second
 * phase). After completing this first Local LCP we will measure how
 * much impact introducing the node into the distributed LCP would mean.
 * If we consider the impact too high we will execute one more round of
 * Local LCP.
 *
 * We will not for the moment consider executing a third Local LCP to
 * ensure that we don't get stuck in this state for too long.
 *
 * Executing 2 Local LCPs should in most cases be sufficient to catch
 * up with LCP times at other nodes.
 *
 * Dropped tables during a node failure
 * ....................................
 * This is a tricky problem that requires us to avoid reusing a table id
 * for a new table until we're sure that all nodes have restarted and
 * heard that the table have been dropped. We also need to tell starting
 * nodes that the table is dropped and that it requires all LCP files
 * to be removed.
 *
 * Various implementation details about LCPs
 * .........................................
 * When we commit a delete we need to know if the fragment is currently
 * performing a LCP and if so we need to know if the row has been
 * scanned yet during LCP.
 *
 * With Partial LCP this is a bit more intricate where we need to check
 * the scan order in the Backup block. However only DBTUP knows if a
 * page has been deleted and then followed by a new page allocation.
 *
 * For parts where we record all rows of the part these pages can be
 * skipped since all rows inserted into this page occurs after start of
 * LCP.
 *
 * However for parts where we record changed rows we need to scan these
 * pages and record DELETE by ROWID for those entries that are free.
 *
 * LCP signal flow
 * ---------------
 *
 * Description of local LCP handling when checkpointing one fragment locally in
 * this data node. DBLQH, BACKUP are executing always in the same thread. DICT
 * and NDBFS mostly execute in different threads.
 *
 * The LCP_PREPARE_REQ for the next fragment to checkpoint can execute in
 * parallel with BACKUP_FRAGMENT_REQ processing. This makes LCP processing
 * faster when there is many small fragments.
 *

 DBLQH                        BACKUP             DICT              NDBFS
  |                             |
  |   LCP_PREPARE_REQ           |
  |---------------------------->|
  |                             |    2 * FSOPENREQ (control files)
  |                             |----------------------------------->|
  |                             |    2 * FSOPENCONF                  |
  |                             |<-----------------------------------|
  |                             |    2 * FSREADREQ (control files)
  |                             |----------------------------------->|
  |                             |    2 * FSREADCONF                  |
  |                             |<-----------------------------------|
  |                             |    FSCLOSEREQ (most recent control file)
  |                             |----------------------------------->|
  |                             |    FSCLOSECONF                     |
  |                             |<-----------------------------------|
  |                             |    FSOPENREQ (checkpoint data file)
  |                             |----------------------------------->|
  |                             |    FSOPENCONF                      |
  |                             |<-----------------------------------|
  |                             | CONTINUEB(ZBUFFER_FULL_META) to oneself
  |                             |--------------------------------------->
  |                             |  GET_TABINFOREQ  |
  |                             |----------------->|
  |                             | GET_TABINFO_CONF |
  |                             |<-----------------|
  |   LCP_PREPARE_CONF          |
  |<----------------------------|
  ...
  |   BACKUP_FRAGMENT_REQ       |-------> CONTINUEB(START_FILE_THREAD)|
  |---------------------------->|
  |   SCAN_FRAGREQ              |
  |<----------------------------|
  |
  | Potential CONTINUEB(ZTUP_SCAN) while scanning for tuples to record in LCP
  |
  |  TRANSID_AI                 |
  |---------------------------->|
  |.... More TRANSID_AI         | (Up to 16 TRANSID_AI, 1 per record)
  |  SCAN_FRAGCONF(close_flag)  |
  |---------------------------->|
  |  SCAN_NEXTREQ               |
  |<----------------------------|
  |
  | Potential CONTINUEB(ZTUP_SCAN) while scanning for tuples to record in LCP
  |
  |  TRANSID_AI                 |
  |---------------------------->|
  |.... More TRANSID_AI         | (Up to 16 TRANSID_AI, 1 per record)
  |  SCAN_FRAGCONF(close_flag)  |
  |---------------------------->|
  
  After each SCAN_FRAGCONF we check of there is enough space in the Backup
  buffer used for the LCP. We will not check it until here, so the buffer
  must be big enough to be able to store the maximum size of 16 records
  in the buffer. Given that maximum record size is about 16kB, this means
  that we must have at least 256 kB of buffer space for LCPs. The default
  is 2MB, so should not set it lower than this unless trying to achieve
  a really memory optimised setup.

  If there is currently no space in the LCP buffer, then the buffer is either
  waiting to be written to disk, or it is being written to disk. In this case
  we will send a CONTINUEB(BUFFER_FULL_SCAN) delayed signal until the buffer
  is available again.

  When the buffer is available again we send a new SCAN_NEXTREQ for the next
  set of rows to be recorded in LCP.

  CONTINUEB(START_FILE_THREAD) will either send a FSAPPENDREQ to the opened
  file or it will send a delayed CONTINUEB(BUFFER_UNDERFLOW).

  When FSAPPENDCONF arrives it will make the same check again and either
  send one more file write through FSAPPENDREQ or another
  CONTINUEB(BUFFER_UNDERFLOW). It will continue like this until the
  SCAN_FRAGCONF has been sent with close_flag set to true AND all the buffers
  have been written to disk.

  After the LCP file write have been completed the close of the fragment LCP
  is started.

  An important consideration when executing LCPs is that they conflict with
  the normal processing of user commands such as key lookups, scans and so
  forth. If we execute on normal JBB-level everything we are going to get
  problems in that we could have job buffers of thousands of signals. This
  means that we will run the LCP extremely slow which will be a significant
  problem.

  The other approach is to use JBA-level. This will obviously give the
  LCP too high priority, we will run LCPs until we have filled up the
  buffer or even until we have filled up our quota for the 100ms timeslot
  where we check for those things. This could end up in producing 10
  MByte of LCP data before allowing user level transactions again. This
  is also obviously not a good idea.

  So most of the startup and shutdown logic for LCPs, both for the entire
  LCP and messages per fragment LCP is ok to raise to JBA level. They are
  short and concise messages and won't bother the user transactions at any
  noticable level. We will avoid fixing GET_TABINFO for that since it
  is only one signal per fragment LCP and also the code path is also used
  many other activitites which are not suitable to run at JBA-level.

  So the major problem to handle is the actual scanning towards LQH. Here
  we need to use a mechanism that keeps the rate at appropriate levels.
  We will use a mix of keeping track of how many jobs were executed since
  last time we executed together with sending JBA-level signals to speed
  up LCP processing for a short time and using signals sent with delay 0
  to avoid being delayed for more than 128 signals (the maximum amount
  of signals executed before we check timed signals).

  The first step to handle this is to ensure that we can send SCAN_FRAGREQ
  on priority A and that this also causes the resulting signals that these
  messages generate also to be sent on priority A level. Then each time
  we can continue the scan immediately after receiving SCAN_FRAGCONF we
  need to make a decision at which level to send the signal. We can
  either send it as delayed signal with 0 delay or we could send them
  at priority A level to get another chunk of data for the LCP at a high
  priority.

  We send the information about Priority A-level as a flag in the
  SCAN_FRAGREQ signal. This will ensure that all resulting signals
  will be sent on Priority A except the CONTINUEB(ZTUP_SCAN) which
  will get special treatment where it increases the length of the
  loop counter and sends the signal with delay 0. We cannot send
  this signal on priority level A since there is no bound on how
  long it will execute.

 DBLQH      PGMAN   LGMAN     BACKUP             DICT              NDBFS
  |         SYNC_PAGE_CACHE_REQ
  |          <------------------|
  |           sync_log_lcp_lsn  |
  |                  <----------|
  |           Flush UNDO log
  |                  ---------->|
  |         Flush fragment page cache
  |         SYNC_PAGE_CACHE_CONF
  |          ------------------>|
  |         If first fragment in LCP then also:
  |         SYNC_EXTENT_PAGES_REQ
  |          <------------------|
  |         Flush all extent pages
  |         SYNC_EXTENT_PAGES_CONF
  |          ------------------>|
  |
  | After all file writes to LCP data file completed:
  |
  |                             |     FSCLOSEREQ
  |                             |------------------------------------>|
  |                             |     FSCLOSECONF
  |                             |<------------------------------------|

  When all those activities are completed:
  1) Sync UNDO log
  2) Sync page cache
  3) Sync extent pages (done immediately following sync of page cache)
  4) Write and close of LCP data file
  then we are ready to write the LCP control file. After this file
  is written and closed the LCP of this fragment is completed.

  With this scheme the LCP of a fragment is immediately usable when the
  LCP of a fragment is completed and the signal of this completion is
  that a written LCP control file exists. At restart one needs to verify
  the GCI of this file to ensure that the LCP is restorable. Otherwise
  the older LCP will be used.

  |                             |     FSWRITEREQ (LCP control file)
  |                             |----------------------------------->|
  |                             |     FSWRITECONF
  |                             |<-----------------------------------|
  |                             |     FSCLOSEREQ (LCP control file)
  |                             |----------------------------------->|
  |                             |     FSCLOSECONF
  |                             |<-----------------------------------|
  |                             |
  | BACKUP_FRAGMENT_CONF        |
  |<----------------------------|
  |
  |                     DIH (local)
  |  LCP_FRAG_REP        |
  |--------------------->|

  LCP_FRAG_REP is distributed to all DIHs from the local DIH instance.

  Finally after completing all fragments we have a number of signals sent to
  complete the LCP processing. The only one needed here is the END_LCPREQ
  to TSMAN to make the dropped pages from any dropped tables available again
  after completing the LCP. This signal needs no wait for it to complete.
  DBLQH knows when the last fragment is completed since it will receive a
  special LCP_FRAG_ORD with lastFragmentFlag set from LQH proxy which in
  turn received this from DIH.

                             LQH Proxy   PGMAN(extra)     LGMAN  TSMAN
  |   LCP_FRAG_ORD(last)        |
  |<----------------------------|
  ......
  | LCP_COMPLETE_REP            |
  |---------------------------->|

  Here the LQH Proxy block will wait for all DBLQH instances to complete.
  After all have completed the following signals will be sent.
                             LQH Proxy   PGMAN(extra)     LGMAN  TSMAN

                                | END_LCPREQ                        |
                                |---------------------------------->|
                                | END_LCPCONF                       |
                                |<----------------------------------|
                                |
                                | LCP_COMPLETE_REP(DBLQH) sent to DIH(local)


  As preparation for this DBLQH sent DEFINE_BACKUP_REQ to setup a backup
  record in restart phase 4. It must get the response DEFINE_BACKUP_CONF for
  the restart to successfully complete. This signal allocates memory for the
  LCP buffers.

  Background deletion process
  ---------------------------
  To save file space we try to delete old checkpoint files no longer needed
  as soon as possible. This is a background process fully handled by the
  BACKUP block, it is handled outside the normal LCP processing protocol.
  
  It could interfere with LCP processing in the exceptional case that we
  haven't managed to delete the old LCP files for a fragment before starting
  to prepare the next local checkpoint.

  From DIH's point of view we always have a LCP instance 0 and a LCP instance
  1 for each fragment. When we complete writing a checkpoint file we need to
  keep the old checkpoint file until the new checkpoint file is usable in a
  restore case. At the time when it completes we cannot use it since it can
  contain rows from a GCI that haven't been fully completed yet. As soon as
  we get an indication of that the checkpoint is useful for restore we can
  delete the old checkpoint file.

  To handle this we maintain a list of fragments to handle deletes of fragment
  checkpoint files.

  We also need a way to handle deletion of old files after crashes. This is
  actually fairly easy to handle as part of the recovery as we use the
  checkpoint files to restore, we can as part of that remove any old
  checkpoint files.

  Local LCP execution
  -------------------
  Normally an LCP is executed as a distributed checkpoint where all nodes
  perform the checkpoint in an synchronised manner. During restarts we might
  execute extra local LCPs that can be used to cut the logs (REDO and UNDO
  logs). We don't generate REDO logs until very late in the recovery process,
  UNDO logs however we generate all the time, so it is mainly the UNDO log
  we have to protect from being exhausted during a restart.

  Such a local checkpoint can be used to recover a system, but it can normally
  not be used to recover a node on its own. If the local LCP happens during a
  system restart there are two options. If we have seen the GCP that we are
  attempting to restore we have all checkpoints and REDO logs required and
  a local LCP during restart should not be necessary normally. If our node is
  behind and we rely on some other node to bring us the latest GCIs then we
  might have to perform a checkpoint. In this case this local LCP will not
  be recoverable on its own.

  The reason why these local LCPs are not recoverable on their own is two
  things. First the synchronisation of data with the other node might not
  be completed yet when the local LCP starts. This means that the local LCP
  isn't seeing a united view, some rows will see a very new version whereas
  other rows will be seeing a very old view. To make a consistent state one
  more node is required. Second even if the local LCP started after the
  synchronisation was complete we don't have local REDO log records that
  can bring the local LCP to a consistent state since we don't write to
  the REDO log during the synchronisation phase. Even if we did write to
  the REDO log during synchronisation the various fragments would still be
  able to recover to different GCIs, thus a consistent restore of the node
  is still not possible.

  So when a node crashes the first time it is always recoverable on its
  own from a certain GCI. The node with the highest such GCI per node
  group is selected as the primary recovery node. Other nodes might have
  to rely on this node for its further recovery. Obviously each node group
  need to be restored from the same GCI to restore a consistent database.
  As soon as we start executing a local LCP the node is no longer able to
  be restored independent of other nodes. So before starting to execute a
  local LCP we must first write something to the file system indicating that
  this node is now not recoverable unless another node gives us assistance.

  So independent of what GCI this can restore according to the system file
  it cannot be used to recover data to other nodes without first recovering
  its own data using another node as aid.

  When a node is started we know of the GCI to restore for our node, it
  is stored in DBLQH in the variable crestartNewestGci during recovery
  and DBLQH gets it from DBDIH that got it from the system file stored
  in the DIH blocks.

  For distributed LCPs we use this GCI to restore to check if a fragment
  LCP can be used for recovery. However for local LCPs this information
  is normally not sufficient. For local LCPs we either have a fixed
  new GCI that we need to handle (during system restart) or a moving
  set of GCPs (during node start).

  So for a restore we need to know the crestartNewestGci from DBLQH, but
  we also need to know the GCIs that we can use from other nodes. This
  information must be written into the local system file of this node.

  The local system file is stored in NDBCNTR. It contains the following
  information:
  1) Flag whether node is restorable on its own
  2) Flag whether node have already removed old LCP files
  3) Last GCI of partial GCPs

  When a node is starting up and we are recovering the data (executing
  RESTORE_LCP_REQ from restore) we want to delete any files that isn't
  usable for recovery since they have a MaxGCIWritten that is larger
  than the above Last GCP of partial GCPs. Once we have completed
  the RESTORE_LCP_REQ phase we know that we have deleted all old
  LCP files that can no longer be used and we should only have one
  copy of each fragment LCP stored at this point. At this point we
  can set the flag above to indicate that we have already removed the
  old LCP files.

  The important parameters in the LCP metadata files stored here are
  the parameters MaxGCIWritten and MaxGCICompleted.

  When we write a local LCP the following holds for MaxGCIWritten.
  During system restart the MaxGCIWritten will be set to the
  GCI that the system restart is trying to restore. If the fragment
  has been fully synchronised before the local LCP started it will
  have the MaxGCICompleted set to the same GCI, otherwise it will
  have its value set to the crestartNewestGci (the GCP that was
  the last GCP we were part of the distributed protocol).

  So for system restarts there are only two GCI values that can be
  used during a local LCP. It is the GCI we are attempting to
  restore in the cluster or it is the GCI we were last involved in
  a distributed protocol for, crestartNewestGci).

  For node restarts the MaxGCIWritten is set according to what
  was set during the writing of the local LCP of the fragment.
  It will never be set smaller than crestartNewestGci.

  MaxGCICompleted is set dependent on the state at the start
  of the local LCP. If the fragment was fully synchronized
  before the start of the fragment LCP we set MaxGCICompleted
  to the GCI that was recoverable in the cluster at the time
  of the start of the local fragment LCP. If the fragment
  wasn't fully synchronised before the start of the local LCP
  we set it to crestartNewestGci or the maximum completed GCI
  in the fragment LCP restored.

  MaxGCIWritten is important during recovery to know whether
  a local LCP is valid, if MaxGCIWritten is larger than the
  GCP we have seen complete, the local LCP files cannot be
  trusted and must be deleted.

  MaxGCICompleted setting can ensure that we don't have to
  re-execute the local REDO log any more. It also takes
  into account that we don't have to synchronize more
  than necessary with the starting node.

  Information needed during restore for local LCP
  ...............................................
  We need to know about the crestartNewestGci. We also need
  to know the maximum GCI that is allowed when we encounter
  a local fragment LCP to understand which local fragment
  LCPs to remove.
  crestartNewestGci is sent as part of RESTORE_LCP_REQ for
  each restored fragment. We also need to add the max
  GCI restorable. Actually it is sufficient to send the
  maximum of those two values. Thus if the local system
  file says that we can recover on our own we will
  continue sending crestartNewestGci. Otherwise we will
  send the maximum of crestartNewestGci and the max GCI
  found in local system file.

  If any of the MaxGciWritten and MaxGciCompleted is set
  higher than the max GCI restorable we are sending to
  the restore block we need to remove that fragment LCP.

  Information needed during write of local LCP
  ............................................
  We need to know the state of the synchronisation of the fragment.
  If m_copy_started_state == AC_NORMAL &&
     fragStatus == ACTIVE_CREATION in DBLQH then we have completed
  the synchronisation of the fragment. Otherwise we haven't.
  We'll get this information from DBLQH at start of write of LCP
  in the Backup block.

  The backup block is informed about the GCI that is currently
  completed in the cluster through the signal RESTORABLE_GCI_REP
  sent from DBLQH. This information DBLQH collects from
  the GCP_SAVEREQ signal. This information is stored in the
  Backup block in m_newestRestorableGci.

  MaxGciCompleted is set by DBLQH and retrieved by Backup block
  in the method lcp_max_completed_gci. For normal distributed
  LCPs this method will simply set the MaxGciCompleted to the
  last completed GCI that DBLQH knows of. DBLQH gets to know
  of completion of a GCI through GCP_SAVEREQ. However for
  local LCP the procedure is a bit more complicated.

  It will first check if the fragment is fully synchronised.
  If not it will set MaxGciCompleted to crestartNewestGci.
  If it is synchronised we will use the same method as for
  a distributed LCP given that we have completed the
  GCI fully since the fragment contains the same data as the
  live node although the data isn't yet recoverable.

  Writing of local system file
  ............................
  Before we start a local LCP during recovery we write
  the local system file to indicate that the node can
  no longer be restored on its own until recovered again.
  This sets the following information in the local system
  file.
  1) Node restorable on its own flag is set to 0 (false).
  2) Flag indicating whether local LCPs removed is set to 0 (false).
  3) max GCP recoverable value is set to
  System Restart case: GCI cluster is restored to
  Node Restart case: GCI recoverable at the moment in cluster

  For node restarts we also write the local system file and update
  the max GCI recoverable value each time a GCI have been made
  recoverable.

  During recovery we read the local system file to discover
  whether we can be master in the system restart and also to
  discover if we can recover on our own.

  We propagate the max GCI recoverable value to DBLQH to ensure
  that we drop old LCP files that are not of any value in
  recovery any more.

  After completing the restart we finally write the local system
  file during phase 50. In this phase all recovery of data is
  completed and only initialisation of SUMA clients remains, so
  it is safe to write the local system file here again. This time
  we set the values to:
  1) Node restorable on its own flag is set to 1 (true)
  2) Flag indicating whether local LCPs removed is set to 0 (ignorable)
  3) max GCP recoverable value is set to 0 (ignorable)
*/
void
Backup::execLCP_PREPARE_REQ(Signal* signal)
{
  jamEntry();
  LcpPrepareReq req = *(LcpPrepareReq*)signal->getDataPtr();

  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, req.backupPtr);

  TablePtr tabPtr;
  FragmentPtr fragPtr;

  jamLine(req.tableId);

  ndbrequire(ptr.p->prepareState == NOT_ACTIVE);
  ptr.p->prepareState = PREPARE_READ_CTL_FILES;
  ptr.p->prepareErrorCode = 0;

  ptr.p->prepare_table.first(tabPtr);
  tabPtr.p->fragments.getPtr(fragPtr, 0);

  tabPtr.p->tableId = req.tableId;
  tabPtr.p->tableType = DictTabInfo::UserTable;

  fragPtr.p->fragmentId = req.fragmentId;
  fragPtr.p->scanned = 0;
  fragPtr.p->scanning = 0;
  fragPtr.p->tableId = req.tableId;
  fragPtr.p->createGci = req.createGci;

  if (req.backupId != ptr.p->backupId ||
      req.localLcpId != ptr.p->localLcpId ||
      !ptr.p->m_initial_lcp_started)
  {
    jam();
    /**
     * These variables are only set at the very first LCP_PREPARE_REQ in
     * an LCP. At this point there is no parallelism, so no need to
     * care for concurrency on the ptr object here.
     *
     * New LCP, reset per-LCP counters. noOfBytes and noOfRecords is other
     * than here handled by the LCP execution phase.
     */
    ptr.p->noOfBytes = 0;
    ptr.p->noOfRecords = 0;
    ptr.p->backupId = req.backupId;
    ptr.p->localLcpId = req.localLcpId;
    ptr.p->m_initial_lcp_started = true;
    ndbrequire(ptr.p->m_first_fragment == false);
    ptr.p->m_first_fragment = true;
    ptr.p->m_is_lcp_scan_active = false;
    ptr.p->m_current_lcp_lsn = Uint64(0);
    DEB_LCP_STAT(("(%u)TAGS Start new LCP, id: %u", instance(), req.backupId));
    LocalDeleteLcpFile_list queue(c_deleteLcpFilePool,
                                  m_delete_lcp_file_head);
    ndbrequire(queue.isEmpty());
  }

  /**
   * We need to open both header files. One of them contains the latest
   * information from the last local checkpoint. We need however to
   * keep the old information around since this new LCP isn't immediately
   * useful for recovery. This also has the added benefit that we have the
   * files replicated. If we crash while we are still writing the new
   * header file we can always recover using the old header file. We
   * retain the old header file. This means that we need to open both
   * files to discover which of them is the most recent one. We should
   * use the older one to write the new header information into, but
   * we should use the newer header file to get the information about
   * which parts to perform the LCP on.
   */
  lcp_open_ctl_file(signal, ptr, 0);
  lcp_open_ctl_file(signal, ptr, 1);
}

/**
 * File processing for an LCP
 * --------------------------
 * At LCP_PREPARE_REQ we prepare the files for an LCP. There are two control
 * files for each fragment. These two files are both opened at prepare time.
 * One contains the description of the previous LCP and one contains the
 * description of the LCP before that one. Usually only one control file
 * exist per fragment since as soon as the LCP is fully completed we delete
 * the now oldest control file.
 *
 * So the steps are:
 * 1) Open both control files
 * 2) Find out which is the most recent control file.
 * 3) Use data from most recent control file to prepare which parts we will
 *    use for the this LCP. Calculate number of next data file to use.
 * 4) Open the new data file for this LCP.
 *    The old data file(s) will still exist
 * 5) Prepare phase is completed
 * 6) Execute phase of LCP fills the data file with data from this LCP.
 * 7) Flush and close the new data file.
 * 8) Write new control file, flush and close it.
 * 9) Report LCP processing as completed.
 *
 * Step 10) and onwards is handled as a background process.
 *
 * 10)Calculate data files to delete after this LCP is completed.
 * 11)Delete old data files no longer needed.
 * 12)Delete the LCP control no longer needed.
 */
void Backup::lcp_open_ctl_file(Signal *signal,
                               BackupRecordPtr ptr,
                               Uint32 lcpNo)
{
  FsOpenReq * req = (FsOpenReq *)signal->getDataPtrSend();
  req->userReference = reference();
  req->fileFlags =
    FsOpenReq::OM_READWRITE | FsOpenReq::OM_CREATE;

  /**
   * Compressed files do not support OM_READWRITE, so we will never
   * use compression for the LCP control files. The files will not
   * take up very much space. If it is necessary to support
   * compressed LCP control files then it is easy to do so by first
   * opening the LCP control files for read in this phase and then
   * when deciding which file to use for the next LCP we will close
   * both files and open the file to use with OM_CREATE and also
   * with OM_TRUNCATE to ensure we overwrite the old file
   * content.
   *
   * O_DIRECT requires very special write semantics which we don't
   * follow for CTL files. So we never set this option for CTL files.
   */
 
  FsOpenReq::v2_setCount(req->fileNumber, 0xFFFFFFFF);

  /**
   * Lcp header file
   */
  BackupFilePtr filePtr;
  TablePtr tabPtr;
  FragmentPtr fragPtr;

  c_backupFilePool.getPtr(filePtr, ptr.p->prepareCtlFilePtr[lcpNo]);
  ptr.p->prepare_table.first(tabPtr);
  tabPtr.p->fragments.getPtr(fragPtr, 0);

  ndbrequire(filePtr.p->m_flags == 0);
  filePtr.p->m_flags |= BackupFile::BF_OPENING;
  filePtr.p->m_flags |= BackupFile::BF_HEADER_FILE;
  filePtr.p->tableId = RNIL; // Will force init
  req->userPointer = filePtr.i;
  FsOpenReq::setVersion(req->fileNumber, 5);
  FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_CTL);
  FsOpenReq::v5_setLcpNo(req->fileNumber, lcpNo);
  FsOpenReq::v5_setTableId(req->fileNumber, tabPtr.p->tableId);
  FsOpenReq::v5_setFragmentId(req->fileNumber, fragPtr.p->fragmentId);
  sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, FsOpenReq::SignalLength, JBA);
}

void
Backup::lcp_open_ctl_file_done(Signal* signal,
                               BackupRecordPtr ptr,
                               BackupFilePtr filePtr)
{
  /**
   * Header file has been opened, now time to read it.
   * Header file is never bigger than one page. Get page from list of
   * pages in the file record. Page comes from global page pool.
   */
  Page32Ptr pagePtr;
  FsReadWriteReq* req = (FsReadWriteReq*)signal->getDataPtrSend();

  filePtr.p->pages.getPtr(pagePtr, 0);
  filePtr.p->m_flags |= BackupFile::BF_READING;

  req->userPointer = filePtr.i;
  req->filePointer = filePtr.p->filePointer;
  req->userReference = reference();
  req->varIndex = 0;
  req->numberOfPages = 1;
  req->operationFlag = 0;
  FsReadWriteReq::setFormatFlag(req->operationFlag,
                                FsReadWriteReq::fsFormatMemAddress);
  FsReadWriteReq::setPartialReadFlag(req->operationFlag, 1);

  Uint32 mem_offset = Uint32((char*)pagePtr.p - (char*)c_startOfPages);
  req->data.memoryAddress.memoryOffset = mem_offset;
  req->data.memoryAddress.fileOffset = 0;
  req->data.memoryAddress.size = BackupFormat::NDB_LCP_CTL_FILE_SIZE_BIG;

  sendSignal(NDBFS_REF, GSN_FSREADREQ, signal,
             FsReadWriteReq::FixedLength + 3, JBA);
}

void
Backup::execFSREADREF(Signal *signal)
{
  jamEntry();

  FsRef * ref = (FsRef *)signal->getDataPtr();
  const Uint32 userPtr = ref->userPointer;
  
  BackupFilePtr filePtr;
  c_backupFilePool.getPtr(filePtr, userPtr);
  /**
   * Since we create the file if it doesn't exist, this should not occur
   * unless something is completely wrong with the file system.
   */
  ndbrequire(false);
}

void
Backup::execFSREADCONF(Signal *signal)
{
  jamEntry();

  FsConf * conf = (FsConf *)signal->getDataPtr();
  const Uint32 userPtr = conf->userPointer;
  
  BackupFilePtr filePtr;
  c_backupFilePool.getPtr(filePtr, userPtr);

  /**
   * If we created the file in the open call, then bytes_read will be 0.
   * This will distinguish a non-existing file from an existing file.
   */
  filePtr.p->bytesRead = conf->bytes_read;
  filePtr.p->m_flags &= ~(Uint32)BackupFile::BF_READING;

  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, filePtr.p->backupPtr);

  if (ptr.p->deleteFilePtr == filePtr.i)
  {
    jam();
    ndbrequire(filePtr.p->bytesRead ==
                 BackupFormat::NDB_LCP_CTL_FILE_SIZE_SMALL ||
               filePtr.p->bytesRead ==
                 BackupFormat::NDB_LCP_CTL_FILE_SIZE_BIG);
    lcp_read_ctl_file_for_rewrite_done(signal, filePtr);
    return;
  }
  for (Uint32 i = 0; i < 2; i++)
  {
    jam();
    c_backupFilePool.getPtr(filePtr, ptr.p->prepareCtlFilePtr[i]);
    if ((filePtr.p->m_flags & BackupFile::BF_READING) ||
        (filePtr.p->m_flags & BackupFile::BF_OPENING))
    {
      jam();
      return;
    }
  }
  lcp_read_ctl_file_done(signal, ptr);
}

void
Backup::lcp_read_ctl_file_done(Signal* signal, BackupRecordPtr ptr)
{
  BackupFilePtr filePtr[2];
  for (Uint32 i = 0; i < 2; i++)
  {
    jam();
    c_backupFilePool.getPtr(filePtr[i], ptr.p->prepareCtlFilePtr[i]);
    DEB_EXTRA_LCP(("(%u)ctl: %u, bytesRead: %u",
                   instance(), i, filePtr[i].p->bytesRead));
    if (filePtr[i].p->bytesRead != 0)
    {
      Page32Ptr pagePtr;
      jam();
      filePtr[i].p->pages.getPtr(pagePtr, 0);
      lcp_read_ctl_file(pagePtr, filePtr[i].p->bytesRead, ptr);
    }
    else
    {
      Page32Ptr pagePtr;
      jam();
      filePtr[i].p->pages.getPtr(pagePtr, 0);
      lcp_init_ctl_file(pagePtr);
    }
  }
  Page32Ptr pagePtr0, pagePtr1;
  filePtr[0].p->pages.getPtr(pagePtr0, 0);
  filePtr[1].p->pages.getPtr(pagePtr1, 0);
  struct BackupFormat::LCPCtlFile *lcpCtlFilePtr0 =
    (struct BackupFormat::LCPCtlFile*)pagePtr0.p;
  struct BackupFormat::LCPCtlFile *lcpCtlFilePtr1 =
    (struct BackupFormat::LCPCtlFile*)pagePtr1.p;
  struct BackupFormat::LCPCtlFile *lcpCtlFilePtr;
  Uint32 closeLcpNumber;
  Uint32 dataFileNumber;
  Uint32 maxGciCompleted;
  Uint32 maxGciWritten;
  Uint32 createGci;
  Uint32 createTableVersion;
  Uint32 lqhCreateTableVersion;

  /**
   * Ignore LCP files that are not valid, a file that have
   * CreateTableVersion equal to 0 is also not valid. This kind of
   * file can be created during Drop Table processing.
   */
  if (lcpCtlFilePtr0->ValidFlag == 0 ||
      lcpCtlFilePtr0->CreateTableVersion == 0)
  {
    jam();
    lcpCtlFilePtr0->ValidFlag = 0;
    lcpCtlFilePtr0->LcpId = 0;
    lcpCtlFilePtr0->LocalLcpId = 0;
  }
  if (lcpCtlFilePtr1->ValidFlag == 0 ||
      lcpCtlFilePtr1->CreateTableVersion == 0)
  {
    jam();
    lcpCtlFilePtr1->ValidFlag = 0;
    lcpCtlFilePtr1->LcpId = 0;
    lcpCtlFilePtr1->LocalLcpId = 0;
  }
  if (lcpCtlFilePtr0->LcpId > lcpCtlFilePtr1->LcpId ||
      (lcpCtlFilePtr0->LcpId == lcpCtlFilePtr1->LcpId &&
       lcpCtlFilePtr0->LcpId != 0 &&
       lcpCtlFilePtr0->LocalLcpId > lcpCtlFilePtr1->LocalLcpId))
  {
    jam();
    dataFileNumber = lcpCtlFilePtr0->LastDataFileNumber;
    lcpCtlFilePtr = lcpCtlFilePtr1;
    ptr.p->prepareNextLcpCtlFileNumber = 1;
    closeLcpNumber = 0;
    createGci = lcpCtlFilePtr0->CreateGci;
    createTableVersion = lcpCtlFilePtr0->CreateTableVersion;
    maxGciCompleted = lcpCtlFilePtr0->MaxGciCompleted;
    maxGciWritten = lcpCtlFilePtr0->MaxGciWritten;
    ptr.p->prepareDeleteCtlFileNumber = closeLcpNumber;
    copy_prev_lcp_info(ptr, lcpCtlFilePtr0);
  }
  else
  {
    /**
     * Both can have the same LCP id. This should only happen when none of the
     * files existed and in this case the LCP id should be 0.
     * This will happen after a new table is created. If upgrading from 7.4 or
     * earlier than this is handled as part of node or cluster restart. So this
     * will not be the reason.
     */
    jam();
    ndbrequire(lcpCtlFilePtr0->LcpId < lcpCtlFilePtr1->LcpId ||
               (lcpCtlFilePtr0->LcpId == lcpCtlFilePtr1->LcpId &&
                (lcpCtlFilePtr0->LcpId == 0 ||
                 lcpCtlFilePtr0->LocalLcpId < lcpCtlFilePtr1->LocalLcpId)));
    dataFileNumber = lcpCtlFilePtr1->LastDataFileNumber;
    lcpCtlFilePtr = lcpCtlFilePtr0;
    ptr.p->prepareNextLcpCtlFileNumber = 0;
    createGci = lcpCtlFilePtr1->CreateGci;
    createTableVersion = lcpCtlFilePtr1->CreateTableVersion;
    maxGciCompleted = lcpCtlFilePtr1->MaxGciCompleted;
    maxGciWritten = lcpCtlFilePtr1->MaxGciWritten;
    closeLcpNumber = 1;
    ptr.p->prepareDeleteCtlFileNumber = closeLcpNumber;
    if (lcpCtlFilePtr1->LcpId == 0)
    {
      jam();
      /**
       * None of the files existed before, ensure that we don't delete
       * any data file since no one exists at this moment. Also ensure
       * that the other control file is removed.
       *
       * lcpCtlFilePtr1->LcpId == 0 => lcpCtlFilePtr0->LcpId == 0 since
       * lcpCtlFilePtr1->LcpId >= lcpCtlFilePtr0->LcpId when we come
       * here.
       *
       * We set m_num_parts_in_lcp to 0 to indicate this is first LCP for
       * this fragment and thus needs to always be a full LCP.
       */
      ptr.p->prepareDeleteCtlFileNumber = RNIL;
      ptr.p->m_prepare_num_parts_in_lcp = 0;
      ptr.p->m_prepare_max_parts_in_lcp = 0;
      ptr.p->m_prepare_scan_change_gci = 0;
      ptr.p->m_prepare_first_start_part_in_lcp = 0;
      ptr.p->preparePrevLcpId = 0;
      ptr.p->preparePrevLocalLcpId = 0;
      maxGciCompleted = 0;
      maxGciWritten = 0;
      TablePtr tabPtr;
      FragmentPtr fragPtr;
      ndbrequire(ptr.p->prepare_table.first(tabPtr));
      tabPtr.p->fragments.getPtr(fragPtr, 0);
      createGci = fragPtr.p->createGci;
      createTableVersion = c_lqh->getCreateSchemaVersion(tabPtr.p->tableId);
    }
    else
    {
      jam();
      copy_prev_lcp_info(ptr, lcpCtlFilePtr1);
    }
  }
  /**
   * prepareNextLcpCtlFileNumber is the number of the prepareCtlFilePtr's
   * which will be kept for this LCP. We have written the data in its page
   * with i-value of 0. This is what lcpCtlFilePtr points to at the moment.
   * This is the page we will later write after completing the LCP of this
   * fragment.
   *
   * We will always get the last data file number by getting the last
   * data file number from the control file to close which is the most
   * recent, then we will add one modulo the max number to get the
   * new last data file number.
   */
  dataFileNumber = get_file_add(dataFileNumber, 1);
  ptr.p->prepareFirstDataFileNumber = dataFileNumber;
  TablePtr tabPtr;
  FragmentPtr fragPtr;
  ndbrequire(ptr.p->prepare_table.first(tabPtr));
  tabPtr.p->fragments.getPtr(fragPtr, 0);
  ptr.p->prepareMaxGciWritten = maxGciWritten;
  lqhCreateTableVersion = c_lqh->getCreateSchemaVersion(tabPtr.p->tableId);

  Uint32 maxGci = MAX(maxGciCompleted, maxGciWritten);
  if ((maxGci < fragPtr.p->createGci &&
       maxGci != 0 &&
       createTableVersion < lqhCreateTableVersion) ||
       (c_initial_start_lcp_not_done_yet &&
        (ptr.p->preparePrevLocalLcpId != 0 ||
         ptr.p->preparePrevLcpId != 0)))
  {
    jam();
    /**
     * This case is somewhat obscure. Due to the fact that we support the
     * config variable __at_restart_skip_indexes we can actually come here
     * for a table (should be a unique index table) that have an LCP file
     * remaining from the previous use of this table id. It is potentially
     * possible also when dropping a table while this node is down and then
     * creating it again before this node has started. In this case we could
     * come here and find an old LCP file. So what we do here is that we
     * perform the drop of the old LCP fragments and then we restart the
     * LCP handling again with an empty set of LCP files as it should be.
     *
     * This means first closing the CTL files (deleting the older one and
     * keeping the newer one to ensure we keep one CTL file until all data
     * files have been deleted and to integrate easily into the drop file
     * handling in this block.
     *
     * We can only discover this case in a cluster where the master is
     * on 7.6 version. So in upgrade cases we won't discover this case
     * since we don't get the createGci from the DICT master in that case
     * when the fragment is created.
     *
     * We can also get here when doing an initial node restart and there
     * is old LCP files to clean up.
     */
    DEB_LCP(("(%u)TAGT Drop case: tab(%u,%u).%u (now %u),"
             " maxGciCompleted: %u,"
             " maxGciWritten: %u, createGci: %u",
            instance(),
            tabPtr.p->tableId,
            fragPtr.p->fragmentId,
            createTableVersion,
            c_lqh->getCreateSchemaVersion(tabPtr.p->tableId),
            maxGciCompleted,
            maxGciWritten,
            fragPtr.p->createGci));

    ptr.p->prepareState = PREPARE_DROP_CLOSE;
    closeFile(signal, ptr, filePtr[closeLcpNumber]);
    closeFile(signal,
              ptr,
              filePtr[ptr.p->prepareNextLcpCtlFileNumber],
              true,
              true);
    return;
  }
  /* Initialise page to write to next CTL file with new LCP id */
  lcp_set_lcp_id(ptr, lcpCtlFilePtr);

  DEB_LCP(("(%u)TAGC Use ctl file: %u, prev Lcp(%u,%u), curr Lcp(%u,%u)"
           ", next data file: %u, tab(%u,%u).%u"
           ", prevMaxGciCompleted: %u, createGci: %u",
           instance(),
           ptr.p->prepareNextLcpCtlFileNumber,
           ptr.p->preparePrevLcpId,
           ptr.p->preparePrevLocalLcpId,
           lcpCtlFilePtr->LcpId,
           lcpCtlFilePtr->LocalLcpId,
           dataFileNumber,
           tabPtr.p->tableId,
           fragPtr.p->fragmentId,
           c_lqh->getCreateSchemaVersion(tabPtr.p->tableId),
           maxGciCompleted,
           fragPtr.p->createGci));

  /**
   * lqhCreateTableVersion == 0 means that the table is no longer active.
   * We will continue as if things were ok, the table is being dropped so
   * no need to abort here, the file will be dropped anyways.
   */
  ndbrequire(createTableVersion == lqhCreateTableVersion ||
             lqhCreateTableVersion == 0);


  /**
   * We close the file which was the previous LCP control file. We will
   * retain the oldest one and use this for this LCP, it will then
   * become the most recent one when we are done. We keep the one to
   * use open for now, it will be closed later in the LCP processing.
   */
  ndbrequire(ptr.p->prepareErrorCode == 0);
  closeFile(signal,
            ptr,
            filePtr[closeLcpNumber],
            true,
            (ptr.p->prepareDeleteCtlFileNumber == RNIL));
  return;
}

void
Backup::copy_prev_lcp_info(BackupRecordPtr ptr,
                           struct BackupFormat::LCPCtlFile *lcpCtlFilePtr)
{
  Uint32 next_start_part = 0;
  ndbrequire(lcpCtlFilePtr->NumPartPairs > 0);
  ptr.p->m_prepare_max_parts_in_lcp = lcpCtlFilePtr->MaxPartPairs;
  ptr.p->m_prepare_num_parts_in_lcp = lcpCtlFilePtr->NumPartPairs;
  jam();
  Uint32 total_parts = 0;
  for (Uint32 i = 0; i < ptr.p->m_prepare_num_parts_in_lcp; i++)
  {
    Uint32 start_part = lcpCtlFilePtr->partPairs[i].startPart;
    Uint32 num_parts = lcpCtlFilePtr->partPairs[i].numParts;
    next_start_part = get_part_add(start_part, num_parts);
    ptr.p->m_prepare_part_info[i].startPart = start_part;
    ptr.p->m_prepare_part_info[i].numParts = num_parts;
    total_parts += num_parts;
  }
  ndbrequire(total_parts == BackupFormat::NDB_MAX_LCP_PARTS);
  ptr.p->m_prepare_first_start_part_in_lcp = next_start_part;
  ptr.p->m_prepare_scan_change_gci = lcpCtlFilePtr->MaxGciCompleted;
  ptr.p->preparePrevLcpId = lcpCtlFilePtr->LcpId;
  ptr.p->preparePrevLocalLcpId = lcpCtlFilePtr->LocalLcpId;
}

Uint32
Backup::get_part_add(Uint32 start_part, Uint32 num_parts)
{
  return (start_part + num_parts) % BackupFormat::NDB_MAX_LCP_PARTS;
}

Uint32
Backup::get_file_add(Uint32 start_file, Uint32 num_files)
{
  return (start_file + num_files) % BackupFormat::NDB_MAX_LCP_FILES;
}

Uint32
Backup::get_file_sub(Uint32 start_file, Uint32 num_files)
{
  if (start_file >= num_files)
  {
    jam();
    return (start_file - num_files);
  }
  else
  {
    jam();
    return (start_file + BackupFormat::NDB_MAX_LCP_FILES - num_files);
  }
}

void
Backup::lcp_read_ctl_file(Page32Ptr pagePtr,
                          Uint32 bytesRead,
                          BackupRecordPtr ptr)
{
  struct BackupFormat::LCPCtlFile *lcpCtlFilePtr =
    (struct BackupFormat::LCPCtlFile*)pagePtr.p;
  /**
   * This function reads the LCP Control file data and retrieves information
   * about:
   * 1) next starting part
   * 2) LCP id this file is a header for
   *
   * This information is used to decide which header file to close (the most
   * recent one) and which header file to use for the next LCP.
   */
  ndbrequire(BackupFormat::NDB_LCP_CTL_FILE_SIZE_SMALL == bytesRead ||
             BackupFormat::NDB_LCP_CTL_FILE_SIZE_BIG == bytesRead);
  if (!convert_ctl_page_to_host(lcpCtlFilePtr))
  {
    jam();
    lcp_init_ctl_file(pagePtr);
  }
  {
    TablePtr tabPtr;
    FragmentPtr fragPtr;
    ptr.p->prepare_table.first(tabPtr);
    tabPtr.p->fragments.getPtr(fragPtr, 0);
    ndbrequire(lcpCtlFilePtr->TableId == tabPtr.p->tableId)
    ndbrequire(lcpCtlFilePtr->FragmentId == fragPtr.p->fragmentId);
  }
}

/**
 * We compress before writing LCP control and after reading it we will
 * decompress the part information. In compressed format we use 3 bytes
 * to store two numbers that can at most be 2048. In uncompressed
 * format each part is a 16-bit unsigned integer.
 */
#define BYTES_PER_PART 3
/**
 * Define the LCP Control file header size, remove the one part pair
 * defined in the common header.
 */
#define LCP_CTL_FILE_HEADER_SIZE (sizeof(BackupFormat::LCPCtlFile) - \
                                  sizeof(BackupFormat::PartPair))

bool
Backup::convert_ctl_page_to_host(
  struct BackupFormat::LCPCtlFile *lcpCtlFilePtr)
{
  Uint32 *pageData = (Uint32*)lcpCtlFilePtr;
  Uint32 numPartPairs = ntohl(lcpCtlFilePtr->NumPartPairs);
  Uint32 real_bytes_read = LCP_CTL_FILE_HEADER_SIZE +
                           (BYTES_PER_PART * numPartPairs);

  /* Checksum is calculated on compressed network byte order */
  if (numPartPairs > BackupFormat::NDB_MAX_LCP_PARTS)
  {
    DEB_LCP(("(%u)numPartPairs: %x", instance(), numPartPairs));
    ndbassert(false);
    return false;
  }
  /**
   * Add 3 to ensure that we get also the last word with anything not
   * equal to 0 when changing to word count.
   */
  Uint32 words = (real_bytes_read + 3) / sizeof(Uint32);
  Uint32 chksum = 0;
  for (Uint32 i = 0; i < words; i++)
  {
    chksum ^= pageData[i];
  }
  ndbassert(chksum == 0);

  if (chksum != 0)
  {
    jam();
    ndbassert(false);
    return false;
  }
  /* Magic is written/read as is */
  lcpCtlFilePtr->fileHeader.BackupVersion =
    ntohl(lcpCtlFilePtr->fileHeader.BackupVersion);
  lcpCtlFilePtr->fileHeader.SectionType =
    ntohl(lcpCtlFilePtr->fileHeader.SectionType);
  lcpCtlFilePtr->fileHeader.SectionLength =
    ntohl(lcpCtlFilePtr->fileHeader.SectionLength);
  lcpCtlFilePtr->fileHeader.FileType =
    ntohl(lcpCtlFilePtr->fileHeader.FileType);
  lcpCtlFilePtr->fileHeader.BackupId =
    ntohl(lcpCtlFilePtr->fileHeader.BackupId);
  ndbrequire(lcpCtlFilePtr->fileHeader.BackupKey_0 == 0);
  ndbrequire(lcpCtlFilePtr->fileHeader.BackupKey_1 == 0);
  /* ByteOrder as is */
  lcpCtlFilePtr->fileHeader.NdbVersion =
    ntohl(lcpCtlFilePtr->fileHeader.NdbVersion);
  lcpCtlFilePtr->fileHeader.MySQLVersion =
    ntohl(lcpCtlFilePtr->fileHeader.MySQLVersion);

  lcpCtlFilePtr->ValidFlag = ntohl(lcpCtlFilePtr->ValidFlag);
  lcpCtlFilePtr->TableId = ntohl(lcpCtlFilePtr->TableId);
  lcpCtlFilePtr->FragmentId = ntohl(lcpCtlFilePtr->FragmentId);
  lcpCtlFilePtr->CreateTableVersion = ntohl(lcpCtlFilePtr->CreateTableVersion);
  lcpCtlFilePtr->CreateGci = ntohl(lcpCtlFilePtr->CreateGci);
  lcpCtlFilePtr->MaxGciCompleted = ntohl(lcpCtlFilePtr->MaxGciCompleted);
  lcpCtlFilePtr->MaxGciWritten = ntohl(lcpCtlFilePtr->MaxGciWritten);
  lcpCtlFilePtr->LcpId = ntohl(lcpCtlFilePtr->LcpId);
  lcpCtlFilePtr->LocalLcpId = ntohl(lcpCtlFilePtr->LocalLcpId);
  lcpCtlFilePtr->MaxPageCount = ntohl(lcpCtlFilePtr->MaxPageCount);
  lcpCtlFilePtr->MaxNumberDataFiles = ntohl(lcpCtlFilePtr->MaxNumberDataFiles);
  lcpCtlFilePtr->LastDataFileNumber = ntohl(lcpCtlFilePtr->LastDataFileNumber);
  lcpCtlFilePtr->MaxPartPairs = ntohl(lcpCtlFilePtr->MaxPartPairs);
  lcpCtlFilePtr->NumPartPairs = ntohl(lcpCtlFilePtr->NumPartPairs);

  ndbrequire(BackupFormat::NDB_LCP_CTL_FILE_SIZE_BIG >= real_bytes_read);
  ndbrequire(lcpCtlFilePtr->fileHeader.FileType ==
             BackupFormat::LCP_CTL_FILE);
  ndbrequire(memcmp(BACKUP_MAGIC, lcpCtlFilePtr->fileHeader.Magic, 8) == 0);
  ndbrequire(lcpCtlFilePtr->NumPartPairs <= lcpCtlFilePtr->MaxPartPairs);
  ndbrequire(lcpCtlFilePtr->NumPartPairs > 0);
  Uint32 total_parts;
  ndbrequire(lcpCtlFilePtr->fileHeader.BackupVersion >= NDBD_USE_PARTIAL_LCP_v2)
  lcpCtlFilePtr->RowCountLow = ntohl(lcpCtlFilePtr->RowCountLow);
  lcpCtlFilePtr->RowCountHigh = ntohl(lcpCtlFilePtr->RowCountHigh);
  total_parts = decompress_part_pairs(lcpCtlFilePtr,
                                      lcpCtlFilePtr->NumPartPairs,
                                      &lcpCtlFilePtr->partPairs[0]);
  ndbrequire(total_parts <= lcpCtlFilePtr->MaxPartPairs);
  return true;
}

void
Backup::convert_ctl_page_to_network(Uint32 *page, Uint32 file_size)
{
  struct BackupFormat::LCPCtlFile *lcpCtlFilePtr =
    (struct BackupFormat::LCPCtlFile*)page;
  Uint32 numPartPairs = lcpCtlFilePtr->NumPartPairs;
  Uint32 compressed_bytes_written = LCP_CTL_FILE_HEADER_SIZE +
                                    (BYTES_PER_PART * numPartPairs);

  /**
   * Add 3 to ensure that we take into account the last word that might
   * filled with only 1 byte of information.
   */
  ndbrequire(file_size >= (compressed_bytes_written + 3));

  ndbrequire(memcmp(BACKUP_MAGIC, lcpCtlFilePtr->fileHeader.Magic, 8) == 0);
  ndbrequire(lcpCtlFilePtr->fileHeader.FileType ==
             BackupFormat::LCP_CTL_FILE);
  ndbrequire(lcpCtlFilePtr->NumPartPairs <= lcpCtlFilePtr->MaxPartPairs);
  ndbrequire(lcpCtlFilePtr->NumPartPairs > 0);
  ndbrequire(lcpCtlFilePtr->fileHeader.NdbVersion >= NDBD_USE_PARTIAL_LCP_v2);
  ndbrequire(lcpCtlFilePtr->fileHeader.BackupVersion == NDBD_USE_PARTIAL_LCP_v2);

  /* Magic is written/read as is */
  lcpCtlFilePtr->fileHeader.BackupVersion =
    htonl(lcpCtlFilePtr->fileHeader.BackupVersion);
  lcpCtlFilePtr->fileHeader.SectionType =
    htonl(lcpCtlFilePtr->fileHeader.SectionType);
  lcpCtlFilePtr->fileHeader.SectionLength =
    htonl(lcpCtlFilePtr->fileHeader.SectionLength);
  lcpCtlFilePtr->fileHeader.FileType =
    htonl(lcpCtlFilePtr->fileHeader.FileType);
  lcpCtlFilePtr->fileHeader.BackupId =
    htonl(lcpCtlFilePtr->fileHeader.BackupId);
  ndbrequire(lcpCtlFilePtr->fileHeader.BackupKey_0 == 0);
  ndbrequire(lcpCtlFilePtr->fileHeader.BackupKey_1 == 0);
  /* ByteOrder as is */
  lcpCtlFilePtr->fileHeader.NdbVersion =
    htonl(lcpCtlFilePtr->fileHeader.NdbVersion);
  lcpCtlFilePtr->fileHeader.MySQLVersion =
    htonl(lcpCtlFilePtr->fileHeader.MySQLVersion);

  lcpCtlFilePtr->ValidFlag = htonl(lcpCtlFilePtr->ValidFlag);
  lcpCtlFilePtr->TableId = htonl(lcpCtlFilePtr->TableId);
  lcpCtlFilePtr->FragmentId = htonl(lcpCtlFilePtr->FragmentId);
  lcpCtlFilePtr->CreateTableVersion = htonl(lcpCtlFilePtr->CreateTableVersion);
  lcpCtlFilePtr->CreateGci = htonl(lcpCtlFilePtr->CreateGci);
  lcpCtlFilePtr->MaxGciCompleted = htonl(lcpCtlFilePtr->MaxGciCompleted);
  lcpCtlFilePtr->MaxGciWritten = htonl(lcpCtlFilePtr->MaxGciWritten);
  lcpCtlFilePtr->LcpId = htonl(lcpCtlFilePtr->LcpId);
  lcpCtlFilePtr->LocalLcpId = htonl(lcpCtlFilePtr->LocalLcpId);
  lcpCtlFilePtr->MaxPageCount = htonl(lcpCtlFilePtr->MaxPageCount);
  lcpCtlFilePtr->MaxNumberDataFiles = htonl(lcpCtlFilePtr->MaxNumberDataFiles);
  lcpCtlFilePtr->LastDataFileNumber = htonl(lcpCtlFilePtr->LastDataFileNumber);

  Uint32 maxPartPairs = lcpCtlFilePtr->MaxPartPairs;
  lcpCtlFilePtr->MaxPartPairs = htonl(lcpCtlFilePtr->MaxPartPairs);
  lcpCtlFilePtr->NumPartPairs = htonl(lcpCtlFilePtr->NumPartPairs);

  lcpCtlFilePtr->RowCountLow = htonl(lcpCtlFilePtr->RowCountLow);
  lcpCtlFilePtr->RowCountHigh = htonl(lcpCtlFilePtr->RowCountHigh);

  Uint32 total_parts = compress_part_pairs(lcpCtlFilePtr,
                                           numPartPairs,
                                           file_size);
  ndbrequire(total_parts <= maxPartPairs);

  /**
   * Checksum is calculated on compressed network byte order.
   * The checksum is calculated without regard to size decreasing due to
   * compression. This is not a problem since we fill the remainder with
   * zeroes and XOR doesn't change the checksum with extra zeroes.
   *
   * Add 3 to ensure that we move to word count in a correct manner.
   */
  lcpCtlFilePtr->Checksum = 0;
  Uint32 words = (compressed_bytes_written + 3) / sizeof(Uint32);
  Uint32 chksum = 0;
  for (Uint32 i = 0; i < words; i++)
  {
    chksum ^= page[i];
  }
  lcpCtlFilePtr->Checksum = chksum;
}

Uint32
Backup::compress_part_pairs(struct BackupFormat::LCPCtlFile *lcpCtlFilePtr,
                            Uint32 num_parts,
                            Uint32 file_size)
{
  Uint32 total_parts = 0;
  unsigned char *part_array =
    (unsigned char*)&lcpCtlFilePtr->partPairs[0].startPart;
  for (Uint32 part = 0; part < num_parts; part++)
  {
    /**
     * Compress the 32 bit by only using 12 bits word. This means that we
     * can fit up to 2048 parts in 8 kBytes.
     * The start part uses the first byte to store the upper 8 bits of
     * 12 bits and bits 0-3 of the second byte is bit 0-3 of the start
     * part. The number of parts has bit 0-3 stored in bit 4-7 of the
     * second byte and bit 4-11 stored in the third byte.
     */
    Uint32 startPart = lcpCtlFilePtr->partPairs[part].startPart;
    Uint32 numParts = lcpCtlFilePtr->partPairs[part].numParts;
    ndbrequire(numParts <= BackupFormat::NDB_MAX_LCP_PARTS);
    Uint32 startPart_bit0_3 = (startPart & 0xF);
    Uint32 startPart_bit4_11 = (startPart >> 4) & 0xFF;
    Uint32 numParts_bit0_3 = (numParts & 0xF);
    Uint32 numParts_bit4_11 = (numParts >> 4) & 0xFF;
    part_array[0] = (unsigned char)startPart_bit4_11;
    part_array[1] = (unsigned char)(startPart_bit0_3 + (numParts_bit0_3 << 4));
    part_array[2] = (unsigned char)numParts_bit4_11;
    part_array += 3;
    total_parts += numParts;
    DEB_EXTRA_LCP(("(%u)compress:tab(%u,%u) Part(%u), start:%u, num_parts: %u",
                   instance(),
                   ntohl(lcpCtlFilePtr->TableId),
                   ntohl(lcpCtlFilePtr->FragmentId),
                   part,
                   startPart,
                   numParts));
  }
  ndbrequire(total_parts == BackupFormat::NDB_MAX_LCP_PARTS);
  unsigned char *start_pos = (unsigned char*)lcpCtlFilePtr;
  unsigned char *end_pos = start_pos + file_size;
  Uint64 remaining_size_64 = end_pos - part_array;
  ndbrequire(remaining_size_64 < file_size);
  Uint32 remaining_size = Uint32(remaining_size_64);
  memset(part_array, 0, remaining_size);
  return total_parts;
}

Uint32 Backup::decompress_part_pairs(
  struct BackupFormat::LCPCtlFile *lcpCtlFilePtr,
  Uint32 num_parts,
  struct BackupFormat::PartPair *partPairs)
{
  Uint32 total_parts = 0;
  unsigned char *part_array = (unsigned char*)&partPairs[0].startPart;
  ndbrequire(num_parts <= BackupFormat::NDB_MAX_LCP_PARTS);
  memcpy(c_part_array, part_array, 3 * num_parts);
  Uint32 j = 0;
  for (Uint32 part = 0; part < num_parts; part++)
  {
    Uint32 part_0 = c_part_array[j+0];
    Uint32 part_1 = c_part_array[j+1];
    Uint32 part_2 = c_part_array[j+2];
    Uint32 startPart = ((part_1 & 0xF) + (part_0 << 4));
    Uint32 numParts = (((part_1 >> 4) & 0xF)) + (part_2 << 4);
    ndbrequire(numParts <= BackupFormat::NDB_MAX_LCP_PARTS);
    partPairs[part].startPart = startPart;
    partPairs[part].numParts = numParts;
    total_parts += numParts;
    DEB_EXTRA_LCP(("(%u)decompress:tab(%u,%u) Part(%u), start:%u, num_parts: %u",
                   instance(),
                   lcpCtlFilePtr->TableId,
                   lcpCtlFilePtr->FragmentId,
                   part,
                   startPart,
                   numParts));
    j += 3;
  }
  ndbassert(total_parts == BackupFormat::NDB_MAX_LCP_PARTS);
  return total_parts;
}

void
Backup::lcp_init_ctl_file(Page32Ptr pagePtr)
{
  const Uint32 sz = sizeof(BackupFormat::FileHeader) >> 2;
  struct BackupFormat::LCPCtlFile *lcpCtlFilePtr =
    (struct BackupFormat::LCPCtlFile*)pagePtr.p;

  memcpy(lcpCtlFilePtr->fileHeader.Magic, BACKUP_MAGIC, 8);
  lcpCtlFilePtr->fileHeader.BackupVersion = NDBD_USE_PARTIAL_LCP_v2;
  lcpCtlFilePtr->fileHeader.SectionType = BackupFormat::FILE_HEADER;
  lcpCtlFilePtr->fileHeader.SectionLength = sz - 3;
  lcpCtlFilePtr->fileHeader.FileType = BackupFormat::LCP_CTL_FILE;
  lcpCtlFilePtr->fileHeader.BackupId = 0;
  lcpCtlFilePtr->fileHeader.BackupKey_0 = 0;
  lcpCtlFilePtr->fileHeader.BackupKey_1 = 0;
  lcpCtlFilePtr->fileHeader.ByteOrder = 0x12345678;
  lcpCtlFilePtr->fileHeader.NdbVersion = NDB_VERSION_D;
  lcpCtlFilePtr->fileHeader.MySQLVersion = NDB_MYSQL_VERSION_D;

  /* Checksum needs to calculated again before write to disk */
  lcpCtlFilePtr->Checksum = 0;
  lcpCtlFilePtr->ValidFlag = 0;
  lcpCtlFilePtr->TableId = 0;
  lcpCtlFilePtr->FragmentId = 0;
  lcpCtlFilePtr->CreateTableVersion = 0;
  lcpCtlFilePtr->CreateGci = 0;
  lcpCtlFilePtr->MaxGciWritten = 0;
  lcpCtlFilePtr->MaxGciCompleted = 0;
  lcpCtlFilePtr->LcpId = 0;
  lcpCtlFilePtr->LocalLcpId = 0;
  lcpCtlFilePtr->MaxPageCount = 0;
  lcpCtlFilePtr->MaxNumberDataFiles = BackupFormat::NDB_MAX_LCP_FILES;
  lcpCtlFilePtr->LastDataFileNumber = BackupFormat::NDB_MAX_LCP_FILES - 1;
  lcpCtlFilePtr->MaxPartPairs = BackupFormat::NDB_MAX_LCP_PARTS;
  lcpCtlFilePtr->NumPartPairs = 1;
  lcpCtlFilePtr->RowCountLow = 0;
  lcpCtlFilePtr->RowCountHigh = 0;
  lcpCtlFilePtr->partPairs[0].startPart = 0;
  lcpCtlFilePtr->partPairs[0].numParts = BackupFormat::NDB_MAX_LCP_PARTS;
}

void
Backup::lcp_close_prepare_ctl_file_done(Signal* signal,
                                        BackupRecordPtr ptr)
{
  /**
   * We have closed the old LCP control file now. We have calculated the
   * number of the data file to be used in this LCP. We will now open this
   * data file to be used by this LCP.
   */
  lcp_open_data_file(signal, ptr);
}

void
Backup::lcp_open_data_file(Signal* signal,
                           BackupRecordPtr ptr)
{
  FsOpenReq * req = (FsOpenReq *)signal->getDataPtrSend();
  req->userReference = reference();
  req->fileFlags = 
    FsOpenReq::OM_WRITEONLY | 
    FsOpenReq::OM_TRUNCATE |
    FsOpenReq::OM_CREATE | 
    FsOpenReq::OM_APPEND |
    FsOpenReq::OM_AUTOSYNC;

  if (c_defaults.m_compressed_lcp)
  {
    req->fileFlags |= FsOpenReq::OM_GZ;
  }

  if (c_defaults.m_o_direct)
  {
    req->fileFlags |= FsOpenReq::OM_DIRECT;
  }

  FsOpenReq::v2_setCount(req->fileNumber, 0xFFFFFFFF);
  req->auto_sync_size = c_defaults.m_disk_synch_size;
  
  TablePtr tabPtr;
  FragmentPtr fragPtr;
  BackupFilePtr filePtr;
  Uint32 dataFileNumber;

  ndbrequire(ptr.p->prepare_table.first(tabPtr));
  tabPtr.p->fragments.getPtr(fragPtr, 0);

  c_backupFilePool.getPtr(filePtr, ptr.p->prepareDataFilePtr[0]);
  dataFileNumber = ptr.p->prepareFirstDataFileNumber;
  ndbrequire(ptr.p->prepareState == PREPARE_READ_CTL_FILES);
  ptr.p->prepareState = PREPARE_OPEN_DATA_FILE;

  ndbrequire(filePtr.p->m_flags == 0);
  filePtr.p->m_flags |= BackupFile::BF_OPENING;
  filePtr.p->tableId = RNIL; // Will force init
  req->userPointer = filePtr.i;
  FsOpenReq::setVersion(req->fileNumber, 5);
  FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_DATA);
  FsOpenReq::v5_setLcpNo(req->fileNumber, dataFileNumber);
  FsOpenReq::v5_setTableId(req->fileNumber, tabPtr.p->tableId);
  FsOpenReq::v5_setFragmentId(req->fileNumber, fragPtr.p->fragmentId);
  sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, FsOpenReq::SignalLength, JBA);
}

void
Backup::lcp_open_data_file_late(Signal* signal,
                                BackupRecordPtr ptr,
                                Uint32 index)
{
  FsOpenReq * req = (FsOpenReq *)signal->getDataPtrSend();
  req->userReference = reference();
  req->fileFlags = 
    FsOpenReq::OM_WRITEONLY | 
    FsOpenReq::OM_TRUNCATE |
    FsOpenReq::OM_CREATE | 
    FsOpenReq::OM_APPEND |
    FsOpenReq::OM_AUTOSYNC;

  if (c_defaults.m_compressed_lcp)
  {
    req->fileFlags |= FsOpenReq::OM_GZ;
  }

  if (c_defaults.m_o_direct)
  {
    req->fileFlags |= FsOpenReq::OM_DIRECT;
  }

  FsOpenReq::v2_setCount(req->fileNumber, 0xFFFFFFFF);
  req->auto_sync_size = c_defaults.m_disk_synch_size;
  
  TablePtr tabPtr;
  FragmentPtr fragPtr;
  BackupFilePtr filePtr;
  ndbrequire(ptr.p->tables.first(tabPtr));
  tabPtr.p->fragments.getPtr(fragPtr, 0);

  ndbrequire(index != 0);
  c_backupFilePool.getPtr(filePtr, ptr.p->dataFilePtr[index]);

  Uint32 dataFileNumber = get_file_add(ptr.p->m_first_data_file_number,
                                       index);

  ndbrequire(filePtr.p->m_flags == 0);
  filePtr.p->m_flags |= BackupFile::BF_OPENING;
  req->userPointer = filePtr.i;
  FsOpenReq::setVersion(req->fileNumber, 5);
  FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_DATA);
  FsOpenReq::v5_setLcpNo(req->fileNumber, dataFileNumber);
  FsOpenReq::v5_setTableId(req->fileNumber, tabPtr.p->tableId);
  FsOpenReq::v5_setFragmentId(req->fileNumber, fragPtr.p->fragmentId);
  sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, FsOpenReq::SignalLength, JBA);
}

void
Backup::lcp_open_data_file_done(Signal* signal,
                                BackupRecordPtr ptr)
{
  TablePtr tabPtr;
  FragmentPtr fragPtr;

  ndbrequire(ptr.p->prepare_table.first(tabPtr));
  tabPtr.p->fragments.getPtr(fragPtr, 0);
  
  BackupFilePtr filePtr;
  c_backupFilePool.getPtr(filePtr, ptr.p->prepareDataFilePtr[0]);  
  ndbrequire(filePtr.p->m_flags == 
             (BackupFile::BF_OPEN | BackupFile::BF_LCP_META));
  filePtr.p->m_flags &= ~(Uint32)BackupFile::BF_LCP_META;

  ndbrequire(ptr.p->prepareState == PREPARE_READ_TABLE_DESC);
  ptr.p->prepareState = PREPARED;
 
  LcpPrepareConf* conf= (LcpPrepareConf*)signal->getDataPtrSend();
  conf->senderData = ptr.p->clientData;
  conf->senderRef = reference();
  conf->tableId = tabPtr.p->tableId;
  conf->fragmentId = fragPtr.p->fragmentId;
  sendSignal(ptr.p->masterRef, GSN_LCP_PREPARE_CONF, 
	     signal, LcpPrepareConf::SignalLength, JBA);
}

void
Backup::lcp_set_lcp_id(BackupRecordPtr ptr,
                       struct BackupFormat::LCPCtlFile *lcpCtlFilePtr)
{
  jam();
  lcpCtlFilePtr->fileHeader.BackupId = ptr.p->backupId;
  lcpCtlFilePtr->LcpId = ptr.p->backupId;
  lcpCtlFilePtr->LocalLcpId = ptr.p->localLcpId;
  if (ptr.p->backupId == ptr.p->preparePrevLcpId)
  {
    jam();
    ndbrequire(ptr.p->localLcpId > ptr.p->preparePrevLocalLcpId);
  }
  else
  {
    jam();
    ndbrequire(ptr.p->backupId > ptr.p->preparePrevLcpId);
  }
}

void
Backup::lcp_copy_ctl_page(BackupRecordPtr ptr)
{
  Page32Ptr page_ptr, recent_page_ptr;
  BackupFilePtr file_ptr, recent_file_ptr;
  Uint32 oldest = ptr.p->prepareNextLcpCtlFileNumber;
  ndbrequire(oldest <= 1);
  Uint32 recent = oldest == 0 ? 1 : 0;
  c_backupFilePool.getPtr(file_ptr, ptr.p->ctlFilePtr);
  c_backupFilePool.getPtr(recent_file_ptr, ptr.p->prepareCtlFilePtr[recent]);
  file_ptr.p->pages.getPtr(page_ptr, 0);
  recent_file_ptr.p->pages.getPtr(recent_page_ptr, 0);
  /**
   * Important to consider here that the page is currently in expanded
   * format. So before we copy it we calculate how much to copy.
   */
  {
    struct BackupFormat::LCPCtlFile *lcpCtlFilePtr =
      (struct BackupFormat::LCPCtlFile*)recent_page_ptr.p;
    Uint32 num_parts = lcpCtlFilePtr->NumPartPairs;
    Uint32 size_to_copy = LCP_CTL_FILE_HEADER_SIZE;
    size_to_copy += (num_parts * sizeof(struct BackupFormat::PartPair));
    memcpy(page_ptr.p,
           recent_page_ptr.p,
           size_to_copy);
  }
#ifdef VM_TRACE
  {
    struct BackupFormat::LCPCtlFile *lcpCtlFilePtr =
      (struct BackupFormat::LCPCtlFile*)page_ptr.p;
    jam();
    Uint32 total_parts = 0;
    Uint32 num_parts = lcpCtlFilePtr->NumPartPairs;
    jamLine(num_parts);
    for (Uint32 i = 0; i < num_parts; i++)
    {
      Uint32 parts = lcpCtlFilePtr->partPairs[i].numParts;
      total_parts += parts;
      jamLine(parts);
    }
    jam();
    ndbassert(total_parts == BackupFormat::NDB_MAX_LCP_PARTS);
  }
#endif
}

void
Backup::setRestorableGci(Uint32 restorableGci)
{
  jam();
  if (restorableGci > m_newestRestorableGci)
  {
    jam();
    m_newestRestorableGci = restorableGci;
  }
}

void
Backup::lcp_update_ctl_page(BackupRecordPtr ptr,
                            Page32Ptr & page_ptr,
                            BackupFilePtr & file_ptr)
{
  Uint32 maxCompletedGci;
  c_backupFilePool.getPtr(file_ptr, ptr.p->ctlFilePtr);
  file_ptr.p->pages.getPtr(page_ptr, 0);
  struct BackupFormat::LCPCtlFile *lcpCtlFilePtr =
    (struct BackupFormat::LCPCtlFile*)page_ptr.p;

  /**
   * An idle LCP cannot have written anything since last LCP. The
   * last LCP was definitely restorable on disk, so there is no
   * need to set MaxGciCompleted to an unrestorable GCI since we
   * haven't written this anyways.
   *
   * Thus for idle LCPs we need not wait for a GCI to be restorable
   * ever. We reflect this by sending max_gci_written equal to the
   * restorable gci in the lcp_max_completed_gci call.
   */
  c_lqh->lcp_max_completed_gci(maxCompletedGci,
                               m_newestRestorableGci,
                               m_newestRestorableGci);
  lcpCtlFilePtr->MaxGciCompleted = maxCompletedGci;
  ptr.p->slaveState.setState(STOPPING);
  c_lqh->lcp_complete_scan(ptr.p->newestGci);
  if (ptr.p->newestGci != lcpCtlFilePtr->MaxGciWritten)
  {
    /**
     * Can happen when performing a LCP as part of restart
     * We will set the newestGci as part of the restore to
     * the GCI we restore.
     */
    DEB_LCP(("(%u)newestGci = %u, MaxGciWritten: %u, MaxGciCompleted: %u",
            instance(),
            ptr.p->newestGci,
            lcpCtlFilePtr->MaxGciWritten,
            lcpCtlFilePtr->MaxGciCompleted));
  }
  ndbassert(ptr.p->newestGci == 
            lcpCtlFilePtr->MaxGciWritten ||
            !m_our_node_started);
  /* Check that schema version is ok, 0 means we're currently deleting table */
  Uint32 lqhCreateTableVersion = c_lqh->getCreateSchemaVersion(lcpCtlFilePtr->TableId);
  ndbrequire(lcpCtlFilePtr->CreateTableVersion == lqhCreateTableVersion ||
             lqhCreateTableVersion == 0);

  lcpCtlFilePtr->MaxGciWritten = ptr.p->newestGci;

  ptr.p->m_wait_gci_to_delete = MAX(maxCompletedGci, ptr.p->newestGci);

  lcp_set_lcp_id(ptr, lcpCtlFilePtr);

  ndbrequire(lcpCtlFilePtr->MaxGciWritten <= m_newestRestorableGci);
  ndbrequire(m_newestRestorableGci != 0);
  /**
   * Also idle LCPs have to be careful to ensure that the LCP is valid before
   * we write it as valid. The reason is that otherwise we won't find the
   * LCP record in the UNDO log and apply too many UNDO log records.
   */
  TablePtr tabPtr;
  ptr.p->tables.first(tabPtr);
  Uint32 tableId = tabPtr.p->tableId;
  ptr.p->m_disk_data_exist = c_lqh->is_disk_columns_in_table(tableId);
  Uint32 valid_flag = lcp_pre_sync_lsn(ptr);
  ptr.p->m_lcp_lsn_synced = valid_flag;
  lcpCtlFilePtr->ValidFlag = valid_flag;

  DEB_LCP(("(%u)TAGY Handle idle LCP, tab(%u,%u).%u, maxGciCompleted = %u"
           ", validFlag = %u",
            instance(),
            lcpCtlFilePtr->TableId,
            lcpCtlFilePtr->FragmentId,
            lcpCtlFilePtr->CreateTableVersion,
            lcpCtlFilePtr->MaxGciCompleted,
            valid_flag));
}

void
Backup::handle_idle_lcp(Signal *signal, BackupRecordPtr ptr)
{
  /**
   * In the prepare phase we opened the data file, we need to
   * close this file before returning to DBLQH as completed.
   * 
   * We also need to write the new LCP control file. The
   * contents we will take from the most recent LCP control
   * file updated with a new MaxGciCompleted.
   *
   * We need to move data files and control files to the
   * execution part since we will start preparing a new
   * LCP immediately after completing this signal execution.
   * A LCP_PREPARE_REQ is most likely waiting to be executed
   * as the next signal.
   */
  Page32Ptr page_ptr;
  BackupFilePtr file_ptr;
  ptr.p->m_empty_lcp = true;
  lcp_copy_ctl_page(ptr);
  lcp_update_ctl_page(ptr, page_ptr, file_ptr);
  ptr.p->deleteDataFileNumber = RNIL;
  lcp_write_ctl_file_to_disk(signal, file_ptr, page_ptr);
  lcp_close_data_file(signal, ptr, true);
  ptr.p->m_wait_disk_data_sync = false;
  ptr.p->m_wait_sync_extent = false;
  ptr.p->m_wait_data_file_close = false;
  ptr.p->m_outstanding_operations = 2;
}

void
Backup::prepare_parts_for_lcp(Signal *signal, BackupRecordPtr ptr)
{
  /**
   * We need to switch in prepared data file and ctl file.
   * We make the previous execute data file and ctl file
   * record to be the new prepare data and ctl file record.
   */
  ptr.p->m_empty_lcp = false;
  calculate_number_of_parts(ptr);
}

void
Backup::prepare_ranges_for_parts(BackupRecordPtr ptr,
                                 Uint32 in_parts)
{
#ifdef DEBUG_LCP
  TablePtr debTabPtr;
  FragmentPtr fragPtr;
  ptr.p->tables.first(debTabPtr);
  debTabPtr.p->fragments.getPtr(fragPtr, 0);
#endif
  Uint64 parts = Uint64(in_parts);
  ndbrequire(parts > 0);
  Uint32 start_part = ptr.p->m_first_start_part_in_lcp;
  Uint64 parts_per_file = parts / Uint64(ptr.p->m_num_lcp_files);
  Uint64 parts_extra_in_first_file =
    parts - (parts_per_file * Uint64(ptr.p->m_num_lcp_files));
  for (Uint32 i = 0; i < ptr.p->m_num_lcp_files; i++)
  {
    ptr.p->m_scan_info[i].m_start_all_part = start_part;
    Uint32 num_parts;
    if (i == 0)
    {
      num_parts = Uint32(parts_extra_in_first_file) + Uint32(parts_per_file);
    }
    else
    {
      num_parts = Uint32(parts_per_file);
    }
    ptr.p->m_scan_info[i].m_num_all_parts = num_parts;
    start_part = get_part_add(start_part, num_parts);
    DEB_LCP(("(%u)tab(%u,%u),m_scan_info[%u].start_all_part = %u,"
             " num_all_parts: %u",
             instance(),
             debTabPtr.p->tableId,
             fragPtr.p->fragmentId,
             i,
             ptr.p->m_scan_info[i].m_start_all_part,
             ptr.p->m_scan_info[i].m_num_all_parts));
  }
  Uint32 num_change_parts = BackupFormat::NDB_MAX_LCP_PARTS - parts;
  ptr.p->m_scan_info[ptr.p->m_num_lcp_files-1].m_start_change_part =
    start_part;
  ptr.p->m_scan_info[ptr.p->m_num_lcp_files-1].m_num_change_parts =
    num_change_parts;
  start_part = get_part_add(start_part, num_change_parts);
  ndbassert(start_part == ptr.p->m_first_start_part_in_lcp);
  ndbassert(is_partial_lcp_enabled() || num_change_parts == 0);
  DEB_LCP(("(%u)tab(%u,%u),m_scan_info[%u].start_change_part = %u,"
           " num_all_parts: %u",
           instance(),
           debTabPtr.p->tableId,
           fragPtr.p->fragmentId,
           ptr.p->m_num_lcp_files - 1,
           ptr.p->m_scan_info[ptr.p->m_num_lcp_files-1].m_start_change_part,
           ptr.p->m_scan_info[ptr.p->m_num_lcp_files-1].m_num_change_parts));
}

void
Backup::prepare_new_part_info(BackupRecordPtr ptr, Uint32 new_parts)
{
  Uint32 remove_files = 0;
  ptr.p->m_num_parts_in_this_lcp = new_parts;
  Uint32 old_num_parts = ptr.p->m_num_parts_in_lcp;
  if (old_num_parts != 0)
  {
    Uint32 new_start_part = ptr.p->m_first_start_part_in_lcp;
    Uint32 new_end_part = new_start_part + new_parts;
    Uint32 old_start_part = ptr.p->m_part_info[0].startPart;
    Uint32 old_end_part = old_start_part;
    ndbrequire(new_start_part == old_start_part);
    jam();
    do
    {
      jam();
      Uint32 old_parts = ptr.p->m_part_info[remove_files].numParts;
      old_end_part += old_parts;
      if (old_end_part > new_end_part)
      {
        jam();
        /* This file has to be kept */
        break;
      }
      old_num_parts--;
      remove_files++;
    } while (old_num_parts > 0);
  }
  Uint32 remaining_files = ptr.p->m_num_parts_in_lcp - remove_files;
  /* First remove all files no longer used */
  for (Uint32 i = 0; i < remaining_files; i++)
  {
    ptr.p->m_part_info[i] = ptr.p->m_part_info[i + remove_files];
    DEB_EXTRA_LCP(("(%u)Parts(%u,%u)",
                   instance(),
                   ptr.p->m_part_info[i].startPart,
                   ptr.p->m_part_info[i].numParts));
  }

  /**
   * The first set of parts is now likely too many parts. The new set of
   * parts have eaten into this from the start. So it needs to be moved
   * ahead as many parts as we have eaten into it.
   */
  if (remaining_files >= 1)
  {
    jam();
    Uint32 new_first_part = get_part_add(
             ptr.p->m_scan_info[0].m_start_all_part, new_parts);
    Uint32 old_first_part = ptr.p->m_part_info[0].startPart;
    Uint32 decrement_parts;
    if (old_first_part > new_first_part)
    {
      jam();
      decrement_parts = (new_first_part +
                         BackupFormat::NDB_MAX_LCP_PARTS) - old_first_part;
    }
    else
    {
      jam();
      decrement_parts = new_first_part - old_first_part;
    }
    ndbrequire(decrement_parts < ptr.p->m_part_info[0].numParts);
    ptr.p->m_part_info[0].numParts -= decrement_parts;
    ptr.p->m_part_info[0].startPart = new_first_part;
    DEB_EXTRA_LCP(("(%u)New first data file span is (%u,%u)",
                   instance(),
                   ptr.p->m_part_info[0].startPart,
                   ptr.p->m_part_info[0].numParts));
  }

  /**
   * Calculate file numbers of files to delete after LCP is
   * completed.
   */
  ptr.p->m_lcp_remove_files = remove_files;
  if (remove_files == 0)
  {
    jam();
    ptr.p->deleteDataFileNumber = RNIL;
  }
  else
  {
    Uint32 move_back_files = remove_files + remaining_files;
    ptr.p->deleteDataFileNumber = get_file_sub(
      ptr.p->m_first_data_file_number,
      move_back_files);

    DEB_LCP(("(%u)m_first_data_file_number = %u, deleteDataFileNumber: %u,"
             " remove_files: %u",
             instance(),
             ptr.p->m_first_data_file_number,
             ptr.p->deleteDataFileNumber,
             remove_files));
  }

  /* Insert the new parts at the end */
  jamLineDebug(ptr.p->m_num_lcp_files);
  for (Uint32 i = 0; i < ptr.p->m_num_lcp_files; i++)
  {
    jamDebug();
    ptr.p->m_part_info[old_num_parts + i].startPart =
      ptr.p->m_scan_info[i].m_start_all_part;
    ptr.p->m_part_info[old_num_parts + i].numParts =
      ptr.p->m_scan_info[i].m_num_all_parts;
    ndbrequire(ptr.p->m_part_info[old_num_parts + i].startPart <
               BackupFormat::NDB_MAX_LCP_PARTS);
    ndbrequire(ptr.p->m_part_info[old_num_parts + i].numParts <=
               BackupFormat::NDB_MAX_LCP_PARTS);
  }
  jamLineDebug(remaining_files);
  ptr.p->m_num_parts_in_lcp = ptr.p->m_num_lcp_files + remaining_files;
  ptr.p->m_max_parts_in_lcp = BackupFormat::NDB_MAX_LCP_PARTS;
#ifdef VM_TRACE
  Uint32 total_parts = 0;
  jam();
  for (Uint32 i = 0; i < ptr.p->m_num_parts_in_lcp; i++)
  {
    Uint32 numParts = ptr.p->m_part_info[i].numParts;
    total_parts += numParts;
  }
  ndbassert(total_parts == BackupFormat::NDB_MAX_LCP_PARTS);
#endif
}

Uint32
Backup::calculate_min_parts(Uint64 row_count,
                            Uint64 row_change_count,
                            Uint64 mem_used,
                            Uint64 total_mem)
{
  /**
   * Calculates
   *   min_parts = 1 + (2048 * k) / (k + p)
   * let y = row_change_count / row_count
   * let z = y * (mem_used / total_mem)
   * let k = y + z * 0.5
   * where k = (row_change_count / row_count) +
   *           0.5 * (mem_used / total_mem)
   * let p = RecoveryWork configuration parameter
   *
   * as explained below.
   *
   * Broken down to:
   * memory_used = memory_used / (1024 * 1024)
   * total_memory = total_memory / (1024 * 1024)
   * This means we are ignoring anything not in the range of MBytes to ensure
   * we don't overflow the 64 bits.
   */

  Uint32 recovery_work = get_recovery_work();

  if (!is_partial_lcp_enabled() || row_count == 0)
  {
    jam();
    /**
     * We have configured the defaults to be that we always execute a full LCP.
     * The LCP can still be a multi-file one, but we will never have to handle
     * anything related to CHANGE ROWS pages.
     *
     * If no rows exists in table we might as well run a full LCP.
     */
    return BackupFormat::NDB_MAX_LCP_PARTS;
  }
  if (row_count < row_change_count)
  {
    jam();
    row_change_count = row_count;
  }
  mem_used /= Uint64(1024 * 1024);
  total_mem /= Uint64(1024 * 1024);
  if (total_mem == Uint64(0))
  {
    jam();
    total_mem = 1;
  }

  double y = double(row_change_count);
  y = y / double(row_count);

  double z = double(mem_used);
  z = z / double(total_mem);
  z = z * y;

  double k = y + (z / double(2));

  double parts = double(2048) * k;

  double p = double(recovery_work) / double(100);
  double parts_divisor = p + k;

  parts = parts / parts_divisor;
  parts = parts + double(1);

  Uint32 min_parts = Uint32(parts);
  ndbrequire(min_parts < Uint32(BackupFormat::NDB_MAX_LCP_PARTS));
  return min_parts;
}

/**
 * This function is closely related to the simulations performed by the
 * lcp_simulator.cc program. These simulations shows that is sufficient
 * to count as little as 70% of the inserts and still maintain the
 * same LCP size and recovery time. Even decreasing it to 50% means
 * that we only temporarily can increase the LCP by 3.3% and decreasing
 * it to 40% we can increase it by 6.7%. Even decreasing it to 0 and
 * thus only write the changed rows after insert and no extra speed of
 * LCPs due to inserts would still only increase the maximum LCP size
 * by 30%. The default setting is now 40% and it can be set between 0
 * and 70%. There are no particular reason to set it higher than 70%.
 *
 * If faster restarts are desired one should instead set RecoveryWork
 * lower.
 *
 * Deletes were shown to need a bit more parts, so we set a delete to
 * mean the same as 1.2 updates. There are no common use cases for
 * massive deletes, so we do not make this configurable, this is
 * hard coded.
 *
 * The idea of how to apply this is to split up row_change_count in
 * an update part, an insert part and a delete part. We multiply
 * the update part by 1, the delete part by 1.2 and the insert part
 * by the configured InsertRecoveryWork (defaults to 0.4).
 */
Uint64
Backup::calculate_row_change_count(BackupRecordPtr ptr)
{
  Uint64 insert_recovery_work = (Uint64)get_insert_recovery_work();
  Uint64 delete_recovery_work = (Uint64)DELETE_RECOVERY_WORK;
  Uint64 row_count = ptr.p->m_row_count;
  Uint64 prev_row_count = ptr.p->m_prev_row_count;
  Uint64 row_change_count = ptr.p->m_row_change_count;
  Uint64 decrease_row_change_count = 0;
  Uint64 new_rows, dropped_rows;
  if (row_count > prev_row_count)
  {
    jam();
    new_rows = row_count - prev_row_count;
    dropped_rows = 0;
    decrease_row_change_count = new_rows;
  }
  else
  {
    jam();
    new_rows = 0;
    dropped_rows = prev_row_count - row_count;
    decrease_row_change_count = dropped_rows;
  }
  ndbrequire(decrease_row_change_count <= row_change_count);

  row_change_count -= decrease_row_change_count;

  new_rows *= insert_recovery_work;
  new_rows /= (Uint64)100;

  dropped_rows *= delete_recovery_work;
  dropped_rows /= Uint64(100);

  row_change_count += new_rows;
  row_change_count += dropped_rows;

  return row_change_count;
}

Uint64
Backup::get_total_memory()
{
  Resource_limit res_limit;
  m_ctx.m_mm.get_resource_limit(RG_DATAMEM, res_limit);
  const Uint32 pages_used = res_limit.m_curr;
  const Uint64 dm_used = Uint64(pages_used) * Uint64(sizeof(GlobalPage));
  const Uint64 num_ldms = getLqhWorkers() != 0 ?
                         (Uint64)getLqhWorkers() : (Uint64)1;
  const Uint64 total_memory = dm_used / num_ldms;
  return total_memory;
}

void
Backup::calculate_number_of_parts(BackupRecordPtr ptr)
{
  /**
   * Here we decide on how many parts we need to use for this LCP.
   * As input we have:
   * 1) Row count
   * 2) Row change count since last LCP
   * => Percentage of rows changed since last LCP
   *
   *   The percentage of rows changed since last LCP is the most
   *   important to this algorithm. This gives us a minimum number of
   *   parts that we need to write as part of this LCP.
   *
   *   There is an overhead in not writing full LCPs. The overhead is
   *   dependent on the amount of changed rows in comparison with the
   *   percentage of parts written.
   *
   *   The overhead formula can be written as:
   *   (1 - x) * (y + 0.5 * z) / x
   *   where:
   *   x = percentage of parts fully written in this LCP
   *   y = percentage of rows changed since last LCP
   *   z = percentage of rows changed during LCP
   *
   *   The (1 - x) comes from that only the parts not written have
   *   overhead for writing changed rows.
   *
   *   The y comes from that writing changed rows is an overhead.
   *
   *   The 0.5 * z comes from that writing changed rows during the LCP
   *   is also an overhead, however only half of those rows will
   *   actually be written since the LCP scan will not see rows
   *   changed before the scan pointer.
   *
   *   The division comes from that the first part of the formula is
   *   the overhead cost for one LCP. However a full LCP consists of
   *   1/x LCPs.
   *
   *   We want to select an x such that the overhead becomes smaller
   *   than some select value.
   *
   *   We can also have overhead in that we have written more parts
   *   than are actually needed. To avoid that this overhead is
   *   unnecessary big we will ensure that we never write any files
   *   that contains more than 1/8th of the parts. This means that at
   *   most we can get 12.5% overhead due to extra parts being written.
   *
   *   We will try to ensure that x is chosen such that overhead is
   *   smaller than p where p is the overhead percentage. p is
   *   configurable in the RecoveryWork parameter and can be set between
   *   25 and 100%. It defaults to 50%.
   *
   *   This means that we should at most require
   *   60% overhead compared to the data memory size. This number
   *   is based on that we don't have an extreme amount of small
   *   fragments with very small memory sizes. In this case the
   *   overhead of writing table meta data as well will make the
   *   overhead. So with most applications we can guarantee that the
   *   overhead stays below 60% and actually in most cases we will
   *   probably even have an overhead of around 40%.
   *
   *   So we want to select an x such that:
   *   (1 - x) (y + z*0.5) / x < p
   *
   *   Now at start of an LCP for a fragment we can treat both y and z
   *   as constants, so let us call (y + 0.5*z) k.
   *   =>
   *   (1 - x) * k < p * x
   *   =>
   *   k - k * x < p * x
   *   =>
   *   k < (k + p) * x
   *   =>
   *   x > k / (k + p)
   *   where k = y + 0.5 * z
   *
   *   Now x is the percentage of parts we should use, when x = 1 we have
   *   2048 parts. So replacing x by parts we get.
   *
   *   parts > 2048 * k / (k + p)
   *   We will select min_parts = 1 + (2048 * k) / (k + p)
   *
   *   Now we know the following:
   *   row_count, row_change_count, memory_used_in_fragment, total_memory_used
   *   This gives:
   *   y = row_change_count / row_count
   *   z = (row_change_count / row_count) *
   *       (memory_used_in_fragment / total_memory_used)
   *
   *   The calculation of z is a prediction based on history, so a sort of
   *   Bayesian average.
   *
   *   Now if we assume that the LCP have entered a steady state with a steady
   *   flow of writes going on.
   *
   *   When the k-value above is large we certainly benefits most from writing
   *   entire set. If for example 70% of the data set was changed the execution
   *   overhead of writing everything is only 50% and this certainly pays off
   *   in order to make restart faster by writing the entire data set in this
   *   case.
   *
   *   At the other end of the spectrum we have small k-values (around 1% or
   *   even smaller), in this the above equation can be simplified to
   *   parts = k / p
   *   Thus p = 25% => parts = 4 * k
   *   p = 50% => parts = 2 * k
   *   p = 100% => parts = k
   *
   *   Now k is more or less the percentage of data changing between LCPs.
   *   So if we have a 1 TByte database and k is 1% we will write 10 GByte
   *   per LCP to the database. This means 10 GByte will be written to the
   *   REDO log (can be smaller or larger since REDO log have a 4 byte overhead
   *   per column, but the REDO log only writes changed columns), almost
   *   10 GByte will be written to the CHANGE pages in the partial LCP
   *
   *   Thus with p = 25% we will write 60 GByte to disk, with p = 50% we will
   *   write 40 GByte to disk and with p = 100% we will write 30 GByte to
   *   disk to handle 10 Gbytes of writes.
   *
   *   The other side of the picture is that increasing p means that more
   *   storage space is needed for LCP files. We need (1 + p) * DataMemory
   *   of storage space for LCP files (unless we use compression when
   *   this should be divided by at least 2). Actually the storage space
   *   should in the worst case be increased by 12.5% of the DataMemory
   *   size since we might need to keep LCP data no longer needed since
   *   we only delete LCP files and not parts of a file.
   *
   *   The third side of the picture is that higher p means longer time to
   *   read in the LCP at restart. If we assume in the above example that
   *   we use p = 25%, thus x = 40GByte of parts, thus 25 LCPs are needed
   *   to restore data. In each such LCP there will be 10 GByte of updated
   *   rows extra, but only half of those need to be applied (mean value).
   *   Thus the extra processing during restart is p/2%. So with p = 25%
   *   we will execute 12.5% more rows compared to if all rows fitted in
   *   one LCP. We will have to read all LCP files from disk though, so
   *   we need to read 25% more from disk during restart.
   *
   *   So thus it becomes natural to think of the p value as the
   *   work we are willing to put into recovery during normal operation.
   *   The more work we do during normal operation, the less work we need
   *   to do during recovery.
   *
   *   Thus we call the config parameter RecoveryWork where small values
   *   means lots of work done and higher values means smaller amount of
   *   work done.
   *
   *   Given that decreasing p beyond 25% increases the load of LCPs
   *   exponentially we set the minimum p to be 25%. Increasing
   *   p beyond 100% means exponentially smaller benefits with
   *   linearly increasing recovery, we set the upper limit at 100%
   *   for p.
   *
   *   It is still possible to use the old algorithm where we always
   *   write everything in each LCP. This is kept for better backwards
   *   compatability and for risk averse users. It also works very well
   *   still for smaller database sizes that updates most of the data
   *   all the time.
   *
   *   Independent of all these settings we will never write any new LCP
   *   data files (only LCP control files will be updated) when no changes
   *   have been made to a table. This will be a great benefit to all
   *   database tables that are read-only most of the time.
   *
   * 3) Total memory size used for memory part of rows
   * => Memory size needed to log changed rows
   * => Memory sized needed to write each part of the LCP
   *
   *   Total memory used gives us an indication if we need to bother about
   *   splitting it into parts at all. We don't care about parts smaller
   *   than 64 kBytes. Also we will never split it into parts smaller than
   *   64 kBytes.
   *
   * 4) Total memory space
   * 5) Number of LDMs in the node
   * => Approximate memory space used by this LDM
   *
   *   This gives us a good understanding how large this fragment is
   *   compared to the rest of the memory in this LDM.
   *
   * 6) Current disk write speed
   *
   *   This gives a good approximation of how long time this particular
   *   fragment LCP will take, it will also give us an indication of how
   *   long time the entire LCP will take.
   *
   * 7) Total REDO log size for our log part
   * 8) Total free REDO log size for our log part
   * 9) => Percentage used of REDO log for our log part
   * 10) We also keep free REDO log size from last LCP we executed and the
   *     timestamp for when we last was here. This helps us calculating the
   *     speed we are writing REDO log at.
   *
   *   We mainly use this to see if we are close to running out of REDO
   *   log, if we are we need to speed up LCP processing by raising the
   *   speed of disk writes for LCP.
   *
   * 11) Time used for last distributed LCP
   * 12) Time used for last LCP locally
   */

  const Uint64 total_memory = get_total_memory();

  /**
   * There are four rules that apply for choosing the number of parts to
   * write all rows in.
   * 1) Make sure that overhead doesn't exceed p% for partial LCPs
   *    So we call this rule 1, rule 1 says that we will select the number
   *    of parts that gives p% overhead.
   *
   * 2) Avoid overhead when it doesn't provide any value, if e.g. we
   *    have 80% of the rows that have been changed then the calculation
   *    means that we're going to use actually less than 80% (about 78%)
   *    since that brings about p% overhead. Obviously there is no sense
   *    in creating overhead in this case since we will write 78% of the
   *    rows + 80% of the remaining 22%. Thus we get an overhead of 25%
   *    to save 4.4% of the row writes which doesn't make a lot of sense.
   *
   *    Rule 2 says that we will select all parts if we have changed
   *    more than 70% of the rows. Otherwise rule 2 selects 0 parts.
   *
   *    An observation here is that during heavy deletes patterns we will
   *    very often fall back to full LCPs since the number of rows is
   *    getting smaller whereas the number of changed rows is increasing.
   *
   *    In a sense this is positive since it means that we will quickly
   *    remove LCP files that contain deleted rows, this space might be
   *    needed by other tables that at the same time gets many inserts.
   *
   * 3) The number of pages sets a limit on how small the number of parts
   *    can be. So with 1 page we can only perform full LCPs, with 2 pages
   *    we can never checkpoint with less than 1024 parts, so the rule
   *    here is that we never go below 2048 divided by number of pages.
   *    This ensures that most of the time there is at least one page
   *    that will write ALL rows in the page.
   *
   *  4) First LCP on  fragment must always be a full LCP.
   *     Rule 4 is 2048 parts when first LCP, otherwise it is 0.
   *
   *  5) This rules says that the minimum number of parts is 1, we will
   *     never run an LCP with 0 parts.
   *
   * In conclusion we will select the rule that returns the highest number
   * of parts.
   */    
  Uint64 row_count = ptr.p->m_row_count;
  Uint64 memory_used = ptr.p->m_memory_used_in_bytes;
  Uint64 row_change_count = calculate_row_change_count(ptr);
  Uint32 min_parts_rule1 = calculate_min_parts(row_count,
                                               row_change_count,
                                               memory_used,
                                               total_memory);

  Uint32 min_parts_rule2 = 0;
  if ((Uint64(10) * row_change_count) >
      (Uint64(7) * row_count))
  {
    jam();
    min_parts_rule2 = BackupFormat::NDB_MAX_LCP_PARTS;
  }

  Uint32 min_parts_rule3 = BackupFormat::NDB_MAX_LCP_PARTS;
  if (ptr.p->m_lcp_max_page_cnt > 1)
  {
    jam();
    min_parts_rule3 = BackupFormat::NDB_MAX_LCP_PARTS /
                        ptr.p->m_lcp_max_page_cnt;
  }
  Uint32 min_parts_rule4 = 0;
  if (ptr.p->preparePrevLcpId == 0)
  {
    jam();
    min_parts_rule4 = BackupFormat::NDB_MAX_LCP_PARTS;
  }
  /**
   * We can never go below 1 part, this is the absolute minimum even if
   * all rules say 0.
   */
  Uint32 min_parts_rule5 = 1;
  Uint32 parts = MAX(MAX(min_parts_rule1, min_parts_rule2),
                     MAX(min_parts_rule3,
                     MAX(min_parts_rule4, min_parts_rule5)));

  if (ERROR_INSERTED(10048) && min_parts_rule4 == 0)
  {
    /**
     * We need this in test cases to ensure that we can create a situation
     * with 1 part per LCP and having more than 980 parts and even close to
     * 2048 LCPs to restore a LCP.
     */
    jam();
    g_eventLogger->info("Set to 1 part by ERROR 10048 injection");
    parts = 1;
  }
#ifdef DEBUG_LCP_STAT
  TablePtr debTabPtr;
  FragmentPtr fragPtr;
  ptr.p->tables.first(debTabPtr);
  debTabPtr.p->fragments.getPtr(fragPtr, 0);
  DEB_LCP_STAT(("(%u)tab(%u,%u), row_count: %llu, calc_row_change_count: %llu"
                ", prev_row_count: %llu, "
                "memory_used: %llu kB, total_dm_memory: %llu MB, "
                "parts: %u, min_parts_rule1: %u, "
                "min_parts_rule3: %u",
                instance(),
                debTabPtr.p->tableId,
                fragPtr.p->fragmentId,
                row_count,
                row_change_count,
                ptr.p->m_prev_row_count,
                memory_used / 1024,
                total_memory / (1024 * 1024),
                parts,
                min_parts_rule1,
                min_parts_rule3));
#endif
  /**
   * We have now calculated the parts to use in this LCP.
   * Now we need to calculate how many LCP files to use for this
   * LCP.
   *
   * The calculation of this is to use 1 file per 12.5% of the
   * parts. Each file must still be at least one fixed page
   * since this is what makes use choose which part something
   * goes into.
   */
  Uint32 min_file_rule_1 =
    (BackupFormat::NDB_MAX_FILES_PER_LCP * parts +
    ((BackupFormat::NDB_MAX_LCP_PARTS / BackupFormat::NDB_MAX_FILES_PER_LCP) -
      1)) /
    BackupFormat::NDB_MAX_LCP_PARTS;
  Uint32 min_file_rule = MAX(1, min_file_rule_1);
  Uint32 max_file_rule_1 = ptr.p->m_lcp_max_page_cnt;
  Uint32 max_file_rule_2 = BackupFormat::NDB_MAX_FILES_PER_LCP;
  Uint32 max_file_rule = MIN(max_file_rule_1, max_file_rule_2);
  max_file_rule = MAX(1, max_file_rule);
  Uint32 num_lcp_files = MIN(min_file_rule, max_file_rule);
  if (!is_partial_lcp_enabled())
  {
    /**
     * To not set EnablePartialLcp to true is mostly there to be able to
     * use NDB as close to the 7.5 manner as possible, this means also not
     * using 8 files when partial LCP isn't enabled. So we use only one
     * file here, it will always be full writes in this case.
     */
    jam();
    num_lcp_files = 1;
  }
  ptr.p->m_num_lcp_files = num_lcp_files;
  DEB_EXTRA_LCP(("(%u) min_file_rules1 = %u, max_file_rule1 = %u",
                 instance(),
                 min_file_rule_1,
                 max_file_rule_1));
  DEB_LCP(("(%u) LCP using %u files",
           instance(),
           ptr.p->m_num_lcp_files));

  /**
   * We will now prepare the BackupRecord such that it has all the
   * information set up to execute this LCP.
   */
  prepare_ranges_for_parts(ptr, parts);
  prepare_new_part_info(ptr, parts);
}

void
Backup::lcp_swap_tables(BackupRecordPtr ptr,
                        TablePtr & tabPtr,
                        Uint32 tableId)
{
  ptr.p->prepare_table.first(tabPtr);
  ndbrequire(tabPtr.p->tableId == tableId);
  ptr.p->prepare_table.removeFirst(tabPtr);

  TablePtr newPrepareTablePtr;
  ptr.p->tables.removeFirst(newPrepareTablePtr);
  ptr.p->tables.addFirst(tabPtr);
  ptr.p->prepare_table.addFirst(newPrepareTablePtr);
}

void
Backup::lcp_swap_data_file(BackupRecordPtr ptr)
{
  Uint32 newPrepareDataFilePtr = ptr.p->dataFilePtr[0];
  ptr.p->dataFilePtr[0] = ptr.p->prepareDataFilePtr[0];
  ptr.p->prepareDataFilePtr[0] = newPrepareDataFilePtr;
}

void
Backup::lcp_swap_ctl_file(BackupRecordPtr ptr)
{
  Uint32 newPrepareCtlFilePtr = ptr.p->ctlFilePtr;
  ptr.p->ctlFilePtr =
    ptr.p->prepareCtlFilePtr[ptr.p->prepareNextLcpCtlFileNumber];
  ptr.p->prepareCtlFilePtr[ptr.p->prepareNextLcpCtlFileNumber] =
    newPrepareCtlFilePtr;
}

void
Backup::copy_lcp_info_from_prepare(BackupRecordPtr ptr)
{
  ptr.p->m_scan_change_gci = ptr.p->m_prepare_scan_change_gci;
  Uint32 total_parts = 0;
  for (Uint32 i = 0; i < ptr.p->m_prepare_num_parts_in_lcp; i++)
  {
    Uint32 num_parts = ptr.p->m_prepare_part_info[i].numParts;
    total_parts += num_parts;
    ptr.p->m_part_info[i] = ptr.p->m_prepare_part_info[i];
  }
  ndbrequire(total_parts == 0 || /* First LCP */
             total_parts == BackupFormat::NDB_MAX_LCP_PARTS);

  ptr.p->m_num_parts_in_lcp = ptr.p->m_prepare_num_parts_in_lcp;
  ptr.p->m_max_parts_in_lcp = ptr.p->m_prepare_max_parts_in_lcp;
  ptr.p->m_first_start_part_in_lcp =
    ptr.p->m_prepare_first_start_part_in_lcp;
  ptr.p->m_first_data_file_number = ptr.p->prepareFirstDataFileNumber;
  ptr.p->deleteCtlFileNumber = ptr.p->prepareDeleteCtlFileNumber;
}

/**
 * An important part of starting an LCP is to insert a record in the
 * UNDO log record indicating start of the LCP. This is used to ensure
 * that main memory rows restored and the disk data restored is in
 * perfect synch with each other. This UNDO log record must be
 * completely synchronised with start of LCP scanning.
 */
void
Backup::lcp_write_undo_log(Signal *signal,
                           BackupRecordPtr ptr)
{
  TablePtr tabPtr;
  ptr.p->tables.first(tabPtr);
  if (c_lqh->is_disk_columns_in_table(tabPtr.p->tableId))
  {
    jam();
    LcpFragOrd *ord = (LcpFragOrd*)signal->getDataPtr();
    FragmentPtr fragPtr;
    tabPtr.p->fragments.getPtr(fragPtr, 0);
    ord->tableId = tabPtr.p->tableId;
    ord->fragmentId = fragPtr.p->fragmentId;
    ord->lcpId = ptr.p->backupId;
    {
      Logfile_client lgman(this, c_lgman, 0);
      ptr.p->m_current_lcp_lsn = lgman.exec_lcp_frag_ord(signal,
                               c_lqh->get_current_local_lcp_id());
      ndbrequire(ptr.p->m_current_lcp_lsn > Uint64(0));
    }
  }
  else
  {
    jam();
    ptr.p->m_current_lcp_lsn = Uint64(0);
  }
}

/**
 * Start execution of LCP after receiving BACKUP_FRAGMENT_REQ
 *
 * When executing this method we know that there is no
 * LCP_PREPARE processing ongoing and there is no LCP
 * execution processing going on. So this is a safe place to
 * move data from prepare part of BackupRecord to execution
 * part of the BackupRecord.
 */
void
Backup::start_execute_lcp(Signal *signal,
                          BackupRecordPtr ptr,
                          TablePtr & tabPtr,
                          Uint32 tableId)
{
  init_extended_lcp_stat();
  ptr.p->slaveState.setState(STARTED);
  ndbrequire(ptr.p->prepareState == PREPARED);
  ptr.p->prepareState = NOT_ACTIVE;
  ptr.p->m_lcp_lsn_synced = 1;
  ptr.p->m_num_lcp_data_files_open = 1;

  copy_lcp_info_from_prepare(ptr);

  /**
   * We need to switch places on prepare table
   * execute table.
   */
  lcp_swap_tables(ptr, tabPtr, tableId);
  lcp_swap_data_file(ptr);
  lcp_swap_ctl_file(ptr);

  lcp_write_undo_log(signal, ptr);
  /**
   * With the introduction of Partial LCPs we need to calculate how
   * many parts that should be part of this LCP.
   *
   * We tell LDM that we are about to start a new LCP. This means that
   * we want to know the number of rows changed since last LCP. We
   * want also to know the current number of rows to calculate the
   * proportion between updated rows and the number of rows in total
   * in the fragment.
   *
   * We treat 0 updated rows as a special case. This means that not a
   * single commit has changed any rows since the last LCP started.
   * In this special case we can actually still use the data files
   * from the old LCP. We do however still need to write a new LCP
   * control file. This is the case since we need to update the
   * MaxGciCompleted in the LCP control file which is very
   * important. It is this value which makes it possible for us to
   * use the LCP to cut the REDO log tail (which in principle is
   * the main reason for doing LCPs, to cut the REDO log tail).
   *
   * The 0 updated rows is most likely a very common case and will
   * save us radical amounts of REDO log processing in idle nodes.
   * If this is the very first LCP we are performing, then we
   * will still go ahead and perform the LCP to simplify the code.
   */
  c_lqh->get_lcp_frag_stats(ptr.p->m_row_count,
                            ptr.p->m_prev_row_count,
                            ptr.p->m_row_change_count,
                            ptr.p->m_memory_used_in_bytes,
                            ptr.p->m_lcp_max_page_cnt);
  Uint32 newestGci = c_lqh->get_lcp_newest_gci();

#ifdef DEBUG_LCP
  TablePtr debTabPtr;
  FragmentPtr fragPtr;
  ptr.p->tables.first(debTabPtr);
  debTabPtr.p->fragments.getPtr(fragPtr, 0);
  DEB_LCP(("(%u)TAGY LCP_Start: tab(%u,%u).%u, row_count: %llu,"
           " row_change_count: %llu,"
           " prev_row_count: %llu,"
           " memory_used_in_bytes: %llu, max_page_cnt: %u, LCP lsn: %llu",
           instance(),
           debTabPtr.p->tableId,
           fragPtr.p->fragmentId,
           c_lqh->getCreateSchemaVersion(debTabPtr.p->tableId),
           ptr.p->m_row_count,
           ptr.p->m_row_change_count,
           ptr.p->m_prev_row_count,
           ptr.p->m_memory_used_in_bytes,
           ptr.p->m_lcp_max_page_cnt,
           ptr.p->m_current_lcp_lsn));
#endif

  if (ptr.p->m_row_change_count == 0 &&
      ptr.p->preparePrevLcpId != 0 &&
      (ptr.p->prepareMaxGciWritten == newestGci &&
       m_our_node_started))
  {
    /**
     * We don't handle it as an idle LCP when it is the first LCP
     * executed on the fragment. In this case we need to run a normal
     * LCP even if it produces an empty LCP data file.
     *
     * Also if someone has committed a transaction on the fragment
     * we will not treat it as an idle LCP even if row change count
     * hasn't changed.
     */
    jam();
    handle_idle_lcp(signal, ptr);
    return;
  }
  else
  {
    jam();
    prepare_parts_for_lcp(signal, ptr);
  }
}

/**
 * We have finished writing of a fragment, the file is written to
 * disk and we can start the complete processing of the LCP for
 * this fragment.
 */
void
Backup::lcp_close_data_file(Signal* signal,
                            BackupRecordPtr ptr,
                            bool delete_flag)
{
  BackupFilePtr filePtr;
  c_backupFilePool.getPtr(filePtr, ptr.p->dataFilePtr[0]);
  closeFile(signal, ptr, filePtr, false, delete_flag);
}

void
Backup::lcp_start_complete_processing(Signal *signal, BackupRecordPtr ptr)
{
  /**
   * We start wait here for 2 parallel events.
   * 1) Sync:ing page cache and extent pages
   * 2) Finalising write of LCP data file and closing it
   *
   * After these events are ready we will check if the LSN have been synched
   * yet. If it hasn't we will still write the LCP control file, but we will
   * write with an invalid flag set in it. We will later rewrite it before
   * deleting the data files.
   *
   * When all of those are done we will write the control file and when this
   * write is completed and the file closed then we will report the LCP back
   * as completed.
   *
   * The only reason for syncing the UNDO log is to ensure that if no
   * pages at all was written as part of LCP for the fragment, then we
   * still need to ensure that the UNDO_LCP log record is flushed to
   * disk. We get the LSN of the UNDO_LCP record from DBLQH.
   *
   * When we sync the pages we will ensure that any writes will also
   * sync the UNDO log to the proper point. So we need not worry about
   * losing any UNDO log records as long as we sync the page cache for
   * a fragment as part of LCP processing. This is called the
   * WAL rule.
   * 
   * Sync:ing the extent pages will write all dirty extent pages, so no
   * special phase is needed to write those at the end of all fragment
   * LCPs.
   *
   *
   * Sync:ing happens in two stages
   * The first stage is syncing all data pages in the PGMAN which executes
   * in the same thread as we do. This goes through the list of dirty pages
   * on the fragment and sync's them one by one with potential throttling of
   * write speed here.
   *
   * The second stage is synching the extent pages. This always happens in
   * the PGMAN proxy block that takes care of the extent pages. Here we
   * sync all extent pages that are dirty for each fragment checkpoint. The
   * reason is that one extent page is shared by many fragments, also the
   * extent pages are only updated when we allocate a new page, allocate a
   * new extent or free an extent (only happens at drop table). So normally
   * we should only dirty a page when adding another page to a fragment.
   * Also many of those writes will usually occur on the same fragment and
   * thus the number of writes on those pages will only be high when there
   * is high insert activity into the database. Also each extent page covers
   * about 1.3 GByte of disk space. So even with 10 TByte of disk space we
   * only have a total of 7000 extent pages. So the activity on writing those
   * to disk cannot be very high.
   *
   * By sync:ing data pages and extent pages after writing the main memory
   * part of the fragment to disk we are sure that we can recover using this
   * fragment LCP. After this we are ready to write the control files for
   * this LCP. The LCP is still not 100% ready to use, it still will have
   * to wait until the global checkpoint is completed of its highest GCI
   * that was written as part of the checkpoint.
   *
   * As explained in another place it is actually only necessary to sync
   * the extent pages for the first fragment containing disk data and
   * also at the end of the local checkpoint.
   *
   * We don't need to wait for this however since the restart will check
   * that we don't recover an LCP which has more recent GCI's than we are
   * to restore. We must however wait with deleting the old LCP control
   * file and data files until we have seen the GCI being completed that
   * we wait for.
   *
   * The localisation of LCP handling and immediate removal of old LCPs
   * means that we can no longer restore any older GCPs than the last
   * completed one. If a requirement comes up for this it is fairly
   * straightforward to add this feature. What is needed is that we wait
   * for yet some more time before deleting an old LCP. If we e.g. want
   * to support restoring up to 100 GCI's back from the last completed
   * than we have to wait for 100 GCI's after completing the one we waited
   * for before we can remove the old LCP files. This might require us to
   * maintain many LCP control files. One could handle this by ensuring
   * that new LCPs aren't started so fast in this case.
   * 
   * However most likely there are better options to restore old versions
   * of the database by using backups.
   */

  ptr.p->m_wait_data_file_close = true;
  ptr.p->m_wait_disk_data_sync = true;
  ptr.p->m_wait_sync_extent = true;
  ptr.p->m_disk_data_exist = false;

  if (ptr.p->m_current_lcp_lsn == Uint64(0))
  {
    /**
     * No entry in log file group created, thus table isn't a disk data
     * table. So we can safely ignore going to PGMAN to sync data pages.
     */
    jam();
    ptr.p->m_wait_disk_data_sync = false;
    ptr.p->m_wait_sync_extent = false;
    lcp_write_ctl_file(signal, ptr);
    return;
  }
  BlockReference ref = numberToRef(PGMAN, instance(), getOwnNodeId());
  TablePtr tabPtr;
  FragmentPtr fragPtr;
  ptr.p->tables.first(tabPtr);
  tabPtr.p->fragments.getPtr(fragPtr, 0);
  ptr.p->m_num_sync_pages_waiting = Uint32(~0);

  SyncPageCacheReq *sync_req = (SyncPageCacheReq*)signal->getDataPtrSend();
  sync_req->senderData = ptr.i;
  sync_req->senderRef = reference();
  sync_req->tableId = tabPtr.p->tableId;
  sync_req->fragmentId = fragPtr.p->fragmentId;
  sendSignal(ref, GSN_SYNC_PAGE_CACHE_REQ, signal,
             SyncPageCacheReq::SignalLength, JBB);
}

void
Backup::execSYNC_PAGE_WAIT_REP(Signal *signal)
{
  jamEntry();
  BackupRecordPtr ptr;
  c_backupPool.getPtr(ptr, signal->theData[0]);
  if (ptr.p->m_wait_disk_data_sync)
  {
    jam();
    ptr.p->m_num_sync_pages_waiting = signal->theData[1];
  }
  else if (ptr.p->m_wait_sync_extent ||
           ptr.p->m_wait_final_sync_extent)
  {
    jam();
    ptr.p->m_num_sync_extent_pages_written = signal->theData[1];
  }
  else
  {
    ndbrequire(false);
  }
}

void
Backup::execSYNC_PAGE_CACHE_CONF(Signal *signal)
{
  SyncPageCacheConf *conf = (SyncPageCacheConf*)signal->getDataPtr();
  BackupRecordPtr ptr;
  TablePtr tabPtr;
  FragmentPtr fragPtr;
  jamEntry();

  c_backupPool.getPtr(ptr, conf->senderData);
  ptr.p->m_num_sync_pages_waiting = 0;
  ptr.p->tables.first(tabPtr);
  tabPtr.p->fragments.getPtr(fragPtr, 0);
  ndbrequire(conf->tableId == tabPtr.p->tableId);
  ndbrequire(conf->fragmentId == fragPtr.p->fragmentId);

  DEB_LCP(("(%u)Completed SYNC_PAGE_CACHE_CONF for tab(%u,%u)"
                      ", diskDataExistFlag: %u",
                      instance(),
                      tabPtr.p->tableId,
                      fragPtr.p->fragmentId,
                      conf->diskDataExistFlag));

  ptr.p->m_wait_disk_data_sync = false;
  if (!conf->diskDataExistFlag)
  {
    jam();
    ptr.p->m_wait_sync_extent = false;
    lcp_write_ctl_file(signal, ptr);
    return;
  }
  ptr.p->m_disk_data_exist = true;
  if (!ptr.p->m_first_fragment)
  {
    jam();
    ptr.p->m_wait_sync_extent = false;
    lcp_write_ctl_file(signal, ptr);
    return;
  }
  ptr.p->m_num_sync_extent_pages_written = Uint32(~0);
  /**
   * Sync extent pages, this is sent to Proxy block that routes the signal to
   * the "extra" PGMAN worker that handles the extent pages.
   */
  SyncExtentPagesReq *req = (SyncExtentPagesReq*)signal->getDataPtrSend();
  req->senderData = ptr.i;
  req->senderRef = reference();
  req->lcpOrder = SyncExtentPagesReq::FIRST_LCP;
  ptr.p->m_first_fragment = false;
  sendSignal(PGMAN_REF, GSN_SYNC_EXTENT_PAGES_REQ, signal,
             SyncExtentPagesReq::SignalLength, JBB);
  return;
}

void
Backup::execSYNC_EXTENT_PAGES_CONF(Signal *signal)
{
  SyncExtentPagesConf *conf = (SyncExtentPagesConf*)signal->getDataPtr();
  BackupRecordPtr ptr;
  jamEntry();

  c_backupPool.getPtr(ptr, conf->senderData);
  ptr.p->m_num_sync_extent_pages_written = 0;
  if (ptr.p->slaveState.getState() == DEFINED)
  {
    jam();
    finish_end_lcp(signal, ptr);
    return;
  }
  ndbrequire(ptr.p->slaveState.getState() == STOPPING);
  ptr.p->m_wait_sync_extent = false;
  lcp_write_ctl_file(signal, ptr);
}

/**
 * A file has been closed as part of LCP completion processing
 * for a fragment.
 */
void
Backup::lcp_close_data_file_conf(Signal* signal, BackupRecordPtr ptr)
{
  jam();
  /**
   * We could have completed only 1 part of this fragment LCP.
   * Check for this and start up next part.
   */
  if (ptr.p->m_empty_lcp)
  {
    jam();
    finalize_lcp_processing(signal, ptr);
    return;
  }
  ndbrequire(ptr.p->m_wait_data_file_close);
  ptr.p->m_wait_data_file_close = false;
  lcp_write_ctl_file(signal, ptr);
}

Uint32
Backup::lcp_pre_sync_lsn(BackupRecordPtr ptr)
{
  Uint32 valid_flag = 1;
  if (ptr.p->m_disk_data_exist)
  {
    jam();
    Uint64 sync_lsn;
    {
      Logfile_client lgman(this, c_lgman, 0);
      sync_lsn = lgman.pre_sync_lsn(ptr.p->m_current_lcp_lsn);
    }
    if (sync_lsn < ptr.p->m_current_lcp_lsn)
    {
      jam();
      /**
       * LSN for UNDO log record of this LCP haven't been sync:ed to disk
       * yet. We will still write the LCP control file, but we will write
       * it with an invalid indicator. Later before deleting the LCP data
       * files we will ensure that the LSN is sync:ed by calling sync_lsn.
       * We will actually call it with LSN = 0 then since the LSN we called
       * with here has been recorded already in LGMAN. So there is no need
       * to remember the individual LSNs for individual fragments. When we
       * call sync_lsn we will ensure that all fragment LCPs already handled
       * before will be sync:ed to disk.
       */
      valid_flag = 0;
    }
  }
  else
  {
    jam();
  }
  DEB_LCP(("(%u)Writing first with ValidFlag = %u", instance(), valid_flag));
  return valid_flag;
}

void
Backup::lcp_write_ctl_file(Signal *signal, BackupRecordPtr ptr)
{
  if (ptr.p->m_wait_data_file_close ||
      ptr.p->m_wait_sync_extent ||
      ptr.p->m_wait_disk_data_sync)
  {
    jam();
    return;
  }

  /**
   * Ensure that we didn't find more rows in LCP than what was
   * in fragment at start of LCP.
   *
   * If we run a full LCP we should always find as many rows as was
   * present in the row count at the start of the LCP.
   * If we run a partial LCP we should never find more rows in this
   * LCP file than was present at the start of the LCP, this is the
   * sum of rows from ALL pages and changed rows in CHANGE pages.
   *
   * This check is important such that we find inconsistencies as
   * soon as they occur, rather than at the time when we recover
   * when it is very difficult to trace back the source of the
   * problem.
   *
   * Error means that the table was dropped during LCP and in this
   * case these numbers are not consistent, we're simply closing
   * the LCP scan in an orderly manner with no rows read. So we
   * should not crash in this case.
   *
   * We wait until we come here to check the numbers, this means
   * that the data file exists when we crash and can be used for
   * analysis.
   */
  {
    BackupFilePtr dataFilePtr;
    c_backupFilePool.getPtr(dataFilePtr,
                            ptr.p->dataFilePtr[0]);
    if (!(ptr.p->m_save_error_code != 0 ||
          ptr.p->m_row_count == dataFilePtr.p->m_lcp_inserts ||
          ((ptr.p->m_num_parts_in_this_lcp !=
             BackupFormat::NDB_MAX_LCP_PARTS) &&
           (ptr.p->m_row_count >=
            (dataFilePtr.p->m_lcp_inserts +
             dataFilePtr.p->m_lcp_writes)))))
    {
      g_eventLogger->info("errCode = %u, row_count = %llu, inserts: %llu"
                          ", writes: %llu, parts: %u",
                          ptr.p->m_save_error_code,
                          ptr.p->m_row_count,
                          dataFilePtr.p->m_lcp_inserts,
                          dataFilePtr.p->m_lcp_writes,
                          ptr.p->m_num_parts_in_this_lcp);
      print_extended_lcp_stat();
      ndbrequire(ptr.p->m_save_error_code != 0 ||
                 ptr.p->m_row_count == dataFilePtr.p->m_lcp_inserts ||
        ((ptr.p->m_num_parts_in_this_lcp != BackupFormat::NDB_MAX_LCP_PARTS) &&
         (ptr.p->m_row_count >=
          (dataFilePtr.p->m_lcp_inserts + dataFilePtr.p->m_lcp_writes))));
    }
  }

  Uint32 valid_flag = lcp_pre_sync_lsn(ptr);

  /**
   * This function prepares the page for the LCP Control file data
   * and ensures checksum is correct, values are written in network
   * byte order when appropriate.
   *
   * As soon as this file is properly written to disk, it can be used
   * in restarts. The restart code will ensure that the GCI is restored
   * which this LCP cannot roll back from.
   */

  BackupFilePtr filePtr;
  Page32Ptr pagePtr;

  jam();
  ptr.p->m_lcp_lsn_synced = valid_flag;
  c_backupFilePool.getPtr(filePtr, ptr.p->ctlFilePtr);
  filePtr.p->pages.getPtr(pagePtr, 0);
  struct BackupFormat::LCPCtlFile *lcpCtlFilePtr =
    (struct BackupFormat::LCPCtlFile*)pagePtr.p;

  memcpy(lcpCtlFilePtr->fileHeader.Magic, BACKUP_MAGIC, 8);
  lcpCtlFilePtr->fileHeader.BackupVersion = NDBD_USE_PARTIAL_LCP_v2;

  const Uint32 sz = sizeof(BackupFormat::FileHeader) >> 2;
  lcpCtlFilePtr->fileHeader.SectionType = BackupFormat::FILE_HEADER;
  lcpCtlFilePtr->fileHeader.SectionLength = sz - 3;
  lcpCtlFilePtr->fileHeader.FileType = BackupFormat::LCP_CTL_FILE;
  lcpCtlFilePtr->fileHeader.BackupId = 0;
  lcpCtlFilePtr->fileHeader.BackupKey_0 = 0;
  lcpCtlFilePtr->fileHeader.BackupKey_1 = 0;
  lcpCtlFilePtr->fileHeader.ByteOrder = 0x12345678;
  lcpCtlFilePtr->fileHeader.NdbVersion = NDB_VERSION_D;
  lcpCtlFilePtr->fileHeader.MySQLVersion = NDB_MYSQL_VERSION_D;

  lcpCtlFilePtr->ValidFlag = valid_flag;

  TablePtr tabPtr;
  FragmentPtr fragPtr;
  ptr.p->tables.first(tabPtr);
  tabPtr.p->fragments.getPtr(fragPtr, 0);

  lcpCtlFilePtr->TableId = tabPtr.p->tableId;
  lcpCtlFilePtr->FragmentId = fragPtr.p->fragmentId;
  lcpCtlFilePtr->CreateTableVersion =
    c_lqh->getCreateSchemaVersion(tabPtr.p->tableId);

  Uint32 maxCompletedGci;
  c_lqh->lcp_max_completed_gci(maxCompletedGci,
                               ptr.p->newestGci,
                               m_newestRestorableGci);
  lcpCtlFilePtr->CreateGci = fragPtr.p->createGci;
  lcpCtlFilePtr->MaxGciCompleted = maxCompletedGci;
  lcpCtlFilePtr->MaxGciWritten = ptr.p->newestGci;

  ptr.p->m_wait_gci_to_delete = MAX(maxCompletedGci, ptr.p->newestGci);

  ndbrequire(m_newestRestorableGci != 0);
  DEB_LCP(("(%u)tab(%u,%u).%u, use ctl file %u, GCI completed: %u,"
           " GCI written: %u, createGci: %u",
           instance(),
           lcpCtlFilePtr->TableId,
           lcpCtlFilePtr->FragmentId,
           lcpCtlFilePtr->CreateTableVersion,
           (ptr.p->deleteCtlFileNumber == 0 ? 1 : 0),
           lcpCtlFilePtr->MaxGciCompleted,
           lcpCtlFilePtr->MaxGciWritten,
           lcpCtlFilePtr->CreateGci));
  ndbrequire((lcpCtlFilePtr->MaxGciWritten + 1) >= fragPtr.p->createGci);
  /**
   * LcpId and LocalLcpId was set in prepare phase.
   */
  if (lcpCtlFilePtr->LocalLcpId != c_lqh->get_current_local_lcp_id())
  {
    g_eventLogger->info("(%u)LocalLcpId: %u, local_lcp_id: %u",
     instance(),
     lcpCtlFilePtr->LocalLcpId,
     c_lqh->get_current_local_lcp_id());
  }
  ndbrequire(lcpCtlFilePtr->LocalLcpId == c_lqh->get_current_local_lcp_id());
  lcpCtlFilePtr->MaxPageCount = ptr.p->m_lcp_max_page_cnt;
  lcpCtlFilePtr->LastDataFileNumber = ptr.p->m_last_data_file_number;
  lcpCtlFilePtr->MaxNumberDataFiles =
    BackupFormat::NDB_MAX_LCP_FILES;
  lcpCtlFilePtr->NumPartPairs = ptr.p->m_num_parts_in_lcp;
  lcpCtlFilePtr->MaxPartPairs = BackupFormat::NDB_MAX_LCP_PARTS;
  lcpCtlFilePtr->RowCountLow = Uint32(ptr.p->m_row_count & 0xFFFFFFFF);
  lcpCtlFilePtr->RowCountHigh = Uint32(ptr.p->m_row_count >> 32);

  for (Uint32 i = 0; i < ptr.p->m_num_parts_in_lcp; i++)
  {
    jam();
    lcpCtlFilePtr->partPairs[i] = ptr.p->m_part_info[i];
  }

  /**
   * Since we calculated checksum with bytes in network order we will write it
   * without setting it in network order, this will ensure that the XOR will
   * be over the same bits as here.
   */
  lcp_write_ctl_file_to_disk(signal, filePtr, pagePtr);
}

void
Backup::lcp_write_ctl_file_to_disk(Signal *signal,
                                   BackupFilePtr filePtr,
                                   Page32Ptr pagePtr)
{
  /**
   * If file size becomes bigger than 4096 bytes we need to write
   * 8192 bytes instead. Currently the header parts are 108 bytes,
   * each part consumes 3 bytes, this means that we can fit
   * (4096 - 108) / 3 parts in 4096 bytes == 1329 parts.
   * Maximum number of parts is currently 2048, thus we can
   * always fit in 8192 bytes. We use multiples of 4096 bytes
   * to fit well with disk devices, no need to complicate
   * file management with lots of different file sizes.
   */
  struct BackupFormat::LCPCtlFile *lcpCtlFilePtr =
    (struct BackupFormat::LCPCtlFile*)pagePtr.p;
  Uint32 num_parts = lcpCtlFilePtr->NumPartPairs;
  Uint32 file_size = LCP_CTL_FILE_HEADER_SIZE +
                     (3 * num_parts + 3);
  if (file_size > BackupFormat::NDB_LCP_CTL_FILE_SIZE_SMALL)
  {
    jam();
    DEB_LCP(("(%u)Writing 8192 byte control file", instance()));
    file_size = BackupFormat::NDB_LCP_CTL_FILE_SIZE_BIG;
  }
  else
  {
    jam();
    file_size = BackupFormat::NDB_LCP_CTL_FILE_SIZE_SMALL;
  }
  convert_ctl_page_to_network((Uint32*)pagePtr.p, file_size);
  filePtr.p->m_flags |= BackupFile::BF_WRITING;
  FsReadWriteReq* req = (FsReadWriteReq*)signal->getDataPtrSend();
  req->userPointer = filePtr.i;
  req->filePointer = filePtr.p->filePointer;
  req->userReference = reference();
  req->varIndex = 0;
  req->numberOfPages = 1;
  req->operationFlag = 0;
  FsReadWriteReq::setFormatFlag(req->operationFlag,
                                FsReadWriteReq::fsFormatMemAddress);
  FsReadWriteReq::setSyncFlag(req->operationFlag, 1);

  Uint32 mem_offset = Uint32((char*)pagePtr.p - (char*)c_startOfPages);
  req->data.memoryAddress.memoryOffset = mem_offset;
  req->data.memoryAddress.fileOffset = 0;
  req->data.memoryAddress.size = file_size;

  sendSignal(NDBFS_REF, GSN_FSWRITEREQ, signal,
             FsReadWriteReq::FixedLength + 3, JBA);
}

void
Backup::execFSWRITEREF(Signal *signal)
{
  ndbrequire(false);
}

void
Backup::execFSWRITECONF(Signal *signal)
{
  BackupRecordPtr ptr;
  BackupFilePtr filePtr;
  FsConf * conf = (FsConf *)signal->getDataPtr();
  const Uint32 userPtr = conf->userPointer;
  jamEntry();
  
  c_backupFilePool.getPtr(filePtr, userPtr);
  ndbrequire((filePtr.p->m_flags & BackupFile::BF_WRITING) != 0);
  filePtr.p->m_flags &= ~(Uint32)BackupFile::BF_WRITING;
  c_backupPool.getPtr(ptr, filePtr.p->backupPtr);

  if (ptr.p->ctlFilePtr == filePtr.i)
  {
    jam();
    closeFile(signal, ptr, filePtr);
    return;
  }
  else if (ptr.p->deleteFilePtr == filePtr.i)
  {
    jam();
    lcp_update_ctl_file_for_rewrite_done(signal, ptr, filePtr);
    return;
  }
  ndbrequire(false);
}

void
Backup::finalize_lcp_processing(Signal *signal, BackupRecordPtr ptr)
{
  TablePtr tabPtr;
  FragmentPtr fragPtr;
  BackupFilePtr filePtr;

  if (ptr.p->m_empty_lcp)
  {
    jam();
    ndbrequire(ptr.p->m_outstanding_operations > 0);
    ptr.p->m_outstanding_operations--;
    if (ptr.p->m_outstanding_operations > 0)
    {
      jam();
      return;
    }
  }
  c_backupFilePool.getPtr(filePtr, ptr.p->dataFilePtr[0]);
  ndbrequire(ptr.p->tables.first(tabPtr));
  Uint32 tableId = tabPtr.p->tableId;

  tabPtr.p->fragments.getPtr(fragPtr, 0);
  Uint32 fragmentId = fragPtr.p->fragmentId;
 
  if (ptr.p->errorCode != 0)
  {
    jam();
    ndbout_c("Fatal : LCP Frag scan failed with error %u"
             " file error is: %d",
             ptr.p->errorCode,
             filePtr.p->errorCode);
    ndbrequire(filePtr.p->errorCode == ptr.p->errorCode);
    
    if ((filePtr.p->m_flags & BackupFile::BF_SCAN_THREAD) == 0)
    {
      jam();
      /* No active scan thread to 'find' the file error.
       * Scan is closed, so let's send backupFragmentRef 
       * back to LQH now...
       */
      backupFragmentRef(signal, filePtr);
      return;
    }
    ndbrequire(false);
    return;
  }

  /**
   * We're fully done with everything related to the LCP of this fragment.
   * Report this back to LQH such that LQH can order the start of a new
   * LCP on a new fragment when it is ready to do so.
   */
  if (ptr.p->deleteDataFileNumber != RNIL ||
      ptr.p->deleteCtlFileNumber != RNIL ||
      !ptr.p->m_lcp_lsn_synced)
  {
    /**
     * We insert a record into the list for files to delete that will ensure
     * that we will delete old LCP files as soon as possible.
     * If deleteDataFileNumber is RNIL it means that this was the very first
     * LCP on this fragment, so no need to delete any old files. It could
     * also be an LCP that retains all files from the old LCP, but we might
     * still need to delete a control file.
     *
     * We wait an extra GCP before we delete the old LCP files. The reason is
     * to avoid calling sync_lsn unnecessarily often. Calling sync_lsn will
     * remove log space (up to one log page) each time it is called and it
     * needs to sync the LSN on the current page.
     */
    jam();
    DeleteLcpFilePtr deleteLcpFilePtr;
    ndbrequire(c_deleteLcpFilePool.seize(deleteLcpFilePtr));
    LocalDeleteLcpFile_list queue(c_deleteLcpFilePool,
                                  m_delete_lcp_file_head);

    Uint32 wait_for_gci = ptr.p->m_wait_gci_to_delete;
    if (m_our_node_started)
    {
      jam();
      wait_for_gci++;
    }
    bool ready_for_delete = (wait_for_gci <= m_newestRestorableGci);
    Uint32 lastDeleteFileNumber= get_file_add(ptr.p->deleteDataFileNumber,
                          (ptr.p->m_lcp_remove_files - 1));
    deleteLcpFilePtr.p->tableId = tableId;
    deleteLcpFilePtr.p->fragmentId = fragmentId;
    deleteLcpFilePtr.p->firstFileId = ptr.p->deleteDataFileNumber;
    deleteLcpFilePtr.p->lastFileId = lastDeleteFileNumber;
    deleteLcpFilePtr.p->waitCompletedGci = wait_for_gci;
    deleteLcpFilePtr.p->lcpCtlFileNumber = ptr.p->deleteCtlFileNumber;
    deleteLcpFilePtr.p->validFlag = ptr.p->m_lcp_lsn_synced;
    deleteLcpFilePtr.p->lcpLsn = ptr.p->m_current_lcp_lsn;
#ifdef DEBUG_LCP
    if (deleteLcpFilePtr.p->firstFileId != RNIL)
    {
      DEB_LCP(("(%u)TAGI Insert delete file in queue:"
        " tab(%u,%u).%u, file(%u-%u,%u) GCI: %u, validFlag: %u",
        instance(),
        tableId,
        fragmentId,
        c_lqh->getCreateSchemaVersion(tableId),
        deleteLcpFilePtr.p->firstFileId,
        deleteLcpFilePtr.p->lastFileId,
        ptr.p->deleteCtlFileNumber,
        ptr.p->m_wait_gci_to_delete,
        ptr.p->m_lcp_lsn_synced));
    }
    else
    {
      DEB_LCP(("(%u)TAGI Insert delete file in queue:"
        " tab(%u,%u).%u, file(RNIL,%u) GCI: %u, validFlag: %u",
        instance(),
        tableId,
        fragmentId,
        c_lqh->getCreateSchemaVersion(tableId),
        ptr.p->deleteCtlFileNumber,
        ptr.p->m_wait_gci_to_delete,
        ptr.p->m_lcp_lsn_synced));
    }
#endif

    if (ready_for_delete)
    {
      /**
       * Add first to delete processing queue since it is already ready for
       * deletion.
       */
      jam();
      queue.addFirst(deleteLcpFilePtr);
    }
    else
    {
      jam();
      queue.addLast(deleteLcpFilePtr);
    }
    if (!m_delete_lcp_files_ongoing && ready_for_delete)
    {
      jam();
      m_delete_lcp_files_ongoing = true;
      signal->theData[0] = BackupContinueB::ZDELETE_LCP_FILE;
      signal->theData[1] = ptr.i;
      sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
    }
  }
 
  ptr.p->errorCode = 0;
  ptr.p->slaveState.forceState(DEFINED);

  BackupFragmentConf * conf = (BackupFragmentConf*)signal->getDataPtrSend();
  conf->backupId = ptr.p->backupId;
  conf->backupPtr = ptr.i;
  conf->tableId = tableId;
  conf->fragmentNo = fragmentId;
  conf->noOfRecordsLow = (ptr.p->noOfRecords & 0xFFFFFFFF);
  conf->noOfRecordsHigh = (ptr.p->noOfRecords >> 32);
  conf->noOfBytesLow = (ptr.p->noOfBytes & 0xFFFFFFFF);
  conf->noOfBytesHigh = (ptr.p->noOfBytes >> 32);
  if (ptr.p->m_empty_lcp)
  {
    jam();
    /**
     * Slow down things a bit for empty LCPs to avoid that we use too much
     * CPU for idle LCP processing. This tends to get a bit bursty and can
     * affect traffic performance for short times.
     */
    sendSignalWithDelay(ptr.p->masterRef, GSN_BACKUP_FRAGMENT_CONF, signal,
	                1, BackupFragmentConf::SignalLength);
  }
  else
  {
    jam();
    sendSignal(ptr.p->masterRef, GSN_BACKUP_FRAGMENT_CONF, signal,
	       BackupFragmentConf::SignalLength, JBA);
  }
}

void
Backup::execRESTORABLE_GCI_REP(Signal *signal)
{
  Uint32 restorable_gci = signal->theData[0];
  /**
   * LQH has a more up-to-date view of the node state so use LQHs version
   * of the node state rather than our own.
   */
  if (c_lqh->getNodeState().startLevel >= NodeState::SL_STOPPING_4)
  {
    jam();
    DEB_LCP(("(%u)Ignore RESTORABLE_GCI_REP: %u in SL_STOPPING_4",
             instance(),
             restorable_gci));
    return;
  }
  if (restorable_gci > m_newestRestorableGci)
  {
    jam();
    m_newestRestorableGci = restorable_gci;
  }
  else
  {
    jam();
    DEB_LCP(("(%u)Already received this restorable gci: %u",
             instance(),
             restorable_gci));
    return;
  }
#ifdef DEBUG_LCP_DEL_FILES
  DeleteLcpFilePtr deleteLcpFilePtr;
  LocalDeleteLcpFile_list queue(c_deleteLcpFilePool,
                                m_delete_lcp_file_head);
  queue.first(deleteLcpFilePtr);
  Uint32 waitGCI = (deleteLcpFilePtr.i != RNIL) ? 
           deleteLcpFilePtr.p->waitCompletedGci : 0;
#endif
  if (m_delete_lcp_files_ongoing)
  {
    jam();
    DEB_LCP_DEL_FILES(("(%u)TAGX Completed GCI: %u (delete files ongoing)"
                       ", waitGCI: %u",
                       instance(),
                       m_newestRestorableGci,
                       waitGCI));
    return;
  }
  jam();
  DEB_LCP_DEL_FILES(("(%u)TAGX Completed GCI: %u (delete files not ongoing)"
                     ", waitGCI: %u, m_lcp_ptr_i = %u",
                     instance(),
                     m_newestRestorableGci,
                     waitGCI,
                     m_lcp_ptr_i));
  if (m_lcp_ptr_i != RNIL)
  {
    jam();
    m_delete_lcp_files_ongoing = true;
    delete_lcp_file_processing(signal, m_lcp_ptr_i);
  }
  return;
}

void
Backup::delete_lcp_file_processing(Signal *signal, Uint32 ptrI)
{
  BackupRecordPtr ptr;
  DeleteLcpFilePtr deleteLcpFilePtr;
  c_backupPool.getPtr(ptr, ptrI);

  ndbrequire(m_delete_lcp_files_ongoing);

  LocalDeleteLcpFile_list queue(c_deleteLcpFilePool,
                                m_delete_lcp_file_head);
  if (queue.isEmpty())
  {
    jam();
    ndbrequire(!ptr.p->m_wait_end_lcp);
    m_delete_lcp_files_ongoing = false;
    if (ptr.p->prepareState == PREPARE_DROP)
    {
      jam();
      /**
       * We use this route when we find the obscure case of
       * finding LCP files belonging to an already dropped table.
       * We keep the code simple here and even wait until the
       * queue is completely empty also for this special case to
       * avoid any unnecessary checks. We then proceed with normal
       * LCP_PREPARE_REQ handling for this case.
       */
      ptr.p->prepareState = PREPARE_READ_CTL_FILES;
      DEB_LCP(("(%u)TAGT Completed wait delete files for drop case",
               instance()));
      lcp_open_ctl_file(signal, ptr, 0);
      lcp_open_ctl_file(signal, ptr, 1);
      return;
    }
    DEB_LCP_DEL_FILES(("(%u)TAGB Completed delete files,"
                       " queue empty, no LCP wait",
                       instance()));
    return;
  }
  queue.first(deleteLcpFilePtr);
  if (deleteLcpFilePtr.p->waitCompletedGci > m_newestRestorableGci)
  {
    jam();
    DEB_LCP(("(%u)TAGW Wait for completed GCI: %u",
             instance(),
             deleteLcpFilePtr.p->waitCompletedGci));
    m_delete_lcp_files_ongoing = false;
    return;
  }
  /* The delete record is ready for deletion process to start. */
  ptr.p->currentDeleteLcpFile = deleteLcpFilePtr.i;
  if (deleteLcpFilePtr.p->validFlag == 0)
  {
    jam();
    sync_log_lcp_lsn(signal, deleteLcpFilePtr, ptr.i);
    return;
  }
  BackupFilePtr filePtr;
  c_backupFilePool.getPtr(filePtr, ptr.p->deleteFilePtr);
  lcp_close_ctl_file_for_rewrite_done(signal, ptr, filePtr);
}

/**
 * This segment of code does a rewrite of the LCP control file.
 * The LCP control file was written with the valid flag set to
 * to 0. This indicates to the restore block that the LCP control
 * file isn't safe to use.
 *
 * Before the old LCP control file is deleted we must ensure that
 * the new LCP control file is ready to use by setting the validFlag
 * to 1.
 *
 * The validFlag can however only be set to 1 if we are sure that
 * the LSN of our UNDO log record for this fragment LCP has been
 * flushed to disk. This is done by calling sync_lsn.
 *
 * Calling sync_lsn for each fragment is not a good solution since
 * each such call can cause one page of UNDO log space to be wasted.
 * So to ensure that we minimize the amount of wasted log space we
 * instead wait for the GCI to be completed before we call sync_lsn.
 * To ensure that we pack as many sync_lsn into one sync_lsn as
 * possible we call pre_sync_lsn earlier in the LCP process.
 *
 * So the idea is that as much as possible we will wait for the
 * LSN to be flushed by someone else, if no one has done that job
 * after almost 2 GCPs we will do it ourselves. If we do it ourselves
 * we will also ensure that all LSNs of calls to pre_sync_lsn will
 * be flushed to disk in the same go.
 *
 * If we find that pre_sync_lsn call indicates that our LSN has already
 * been flushed to disk we can avoid this extra round of read and write
 * of the LCP control file. We also don't need it for tables without
 * disk data columns.
 *
 * After sync:ing the UNDO LSN we will read the LCP control file,
 * set the ValidFlag in the LCP control file and write it again
 * and finally close it.
 *
 * Then we will continue deleting the old data files and old
 * LCP control file.
 */
void
Backup::sync_log_lcp_lsn(Signal *signal,
                         DeleteLcpFilePtr deleteLcpFilePtr,
                         Uint32 ptrI)
{
  Logfile_client::Request req;
  int ret;
  req.m_callback.m_callbackData = ptrI;
  req.m_callback.m_callbackIndex = SYNC_LOG_LCP_LSN;
  {
    Logfile_client lgman(this, c_lgman, 0);
    ret = lgman.sync_lsn(signal, deleteLcpFilePtr.p->lcpLsn, &req, 1);
    jamEntry();
  }
  switch (ret)
  {
    case 0:
    {
      jam();
      return;
    }
    case -1:
    {
      g_eventLogger->info("(%u)Failed to Sync LCP lsn", instance());
      ndbrequire(false);
     return; //Will never reach here
    }
    default:
    {
      jam();
      execute(signal, req.m_callback, 0);
      return;
    }
  }
}

void
Backup::sync_log_lcp_lsn_callback(Signal *signal, Uint32 ptrI, Uint32 res)
{
  BackupRecordPtr ptr;
  DeleteLcpFilePtr deleteLcpFilePtr;
  jamEntry();
  c_backupPool.getPtr(ptr, ptrI);
  ndbrequire(res == 0);
  c_deleteLcpFilePool.getPtr(deleteLcpFilePtr, ptr.p->currentDeleteLcpFile);
  ndbrequire(deleteLcpFilePtr.p->validFlag == 0);
  /**
   * The LSN have now been sync:ed, now time to read the LCP control file
   * again to update the validFlag.
   */
  lcp_open_ctl_file_for_rewrite(signal, deleteLcpFilePtr, ptr);
}

void
Backup::lcp_open_ctl_file_for_rewrite(Signal *signal,
                                      DeleteLcpFilePtr deleteLcpFilePtr,
                                      BackupRecordPtr ptr)
{
  BackupFilePtr filePtr;
  c_backupFilePool.getPtr(filePtr, ptr.p->deleteFilePtr);
  FsOpenReq *req = (FsOpenReq*)signal->getDataPtrSend();

  req->userReference = reference();
  req->fileFlags = FsOpenReq::OM_READWRITE;
  req->userPointer = filePtr.i;

  ndbrequire(filePtr.p->m_flags == 0);
  filePtr.p->m_flags = BackupFile::BF_OPENING;

  /**
   * We use same table id and fragment id as the one we are about to
   * delete. If we are about to delete LCP control file 0, then we should
   * rewrite LCP control file 1 and vice versa if we are to delete LCP
   * control file 1.
   */
  Uint32 tableId = deleteLcpFilePtr.p->tableId;
  Uint32 fragmentId = deleteLcpFilePtr.p->fragmentId;
  Uint32 lcpNo = (deleteLcpFilePtr.p->lcpCtlFileNumber == 0) ? 1 : 0;

  FsOpenReq::v2_setCount(req->fileNumber, 0xFFFFFFFF);
  FsOpenReq::setVersion(req->fileNumber, 5);
  FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_CTL);
  FsOpenReq::v5_setLcpNo(req->fileNumber, lcpNo);
  FsOpenReq::v5_setTableId(req->fileNumber, tableId);
  FsOpenReq::v5_setFragmentId(req->fileNumber, fragmentId);

  sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, FsOpenReq::SignalLength, JBA);
}

void
Backup::lcp_open_ctl_file_for_rewrite_done(Signal *signal,
                                           BackupFilePtr filePtr)
{
  lcp_read_ctl_file_for_rewrite(signal, filePtr);
}

void
Backup::lcp_read_ctl_file_for_rewrite(Signal *signal,
                                      BackupFilePtr filePtr)
{
  FsReadWriteReq *req = (FsReadWriteReq*)signal->getDataPtrSend();
  Page32Ptr pagePtr;

  filePtr.p->pages.getPtr(pagePtr, 0);
  ndbrequire(filePtr.p->m_flags == BackupFile::BF_OPEN);
  filePtr.p->m_flags |= BackupFile::BF_READING;

  req->userPointer = filePtr.i;
  req->filePointer = filePtr.p->filePointer;
  req->userReference = reference();
  req->varIndex = 0;
  req->numberOfPages = 1;
  req->operationFlag = 0;
  FsReadWriteReq::setFormatFlag(req->operationFlag,
                                FsReadWriteReq::fsFormatMemAddress);
  FsReadWriteReq::setPartialReadFlag(req->operationFlag, 1);

  Uint32 mem_offset = Uint32(((char*)pagePtr.p) - ((char*)c_startOfPages));
  req->data.memoryAddress.memoryOffset = mem_offset;
  req->data.memoryAddress.fileOffset = 0;
  req->data.memoryAddress.size = BackupFormat::NDB_LCP_CTL_FILE_SIZE_BIG;

  sendSignal(NDBFS_REF, GSN_FSREADREQ, signal,
             FsReadWriteReq::FixedLength + 3, JBA);
}

void
Backup::lcp_read_ctl_file_for_rewrite_done(Signal *signal,
                                           BackupFilePtr filePtr)
{
  Page32Ptr pagePtr;

  filePtr.p->pages.getPtr(pagePtr, 0);
  struct BackupFormat::LCPCtlFile *lcpCtlFilePtr =
    (struct BackupFormat::LCPCtlFile*)pagePtr.p;
  ndbrequire(convert_ctl_page_to_host(lcpCtlFilePtr));
  lcpCtlFilePtr->ValidFlag = 1;
  lcp_update_ctl_file_for_rewrite(signal, filePtr, pagePtr);
}

void
Backup::lcp_update_ctl_file_for_rewrite(Signal *signal,
                                        BackupFilePtr filePtr,
                                        Page32Ptr pagePtr)
{
  ndbrequire(filePtr.p->m_flags == BackupFile::BF_OPEN);
  lcp_write_ctl_file_to_disk(signal, filePtr, pagePtr);
}

void
Backup::lcp_update_ctl_file_for_rewrite_done(Signal *signal,
                                             BackupRecordPtr ptr,
                                             BackupFilePtr filePtr)
{
  lcp_close_ctl_file_for_rewrite(signal, ptr, filePtr);
}

void
Backup::lcp_close_ctl_file_for_rewrite(Signal *signal,
                                       BackupRecordPtr ptr,
                                       BackupFilePtr filePtr)
{
  ndbrequire(ptr.p->errorCode == 0);
  closeFile(signal, ptr, filePtr, false, false);
#ifdef DEBUG_LCP
  DeleteLcpFilePtr deleteLcpFilePtr;
  c_deleteLcpFilePool.getPtr(deleteLcpFilePtr, ptr.p->currentDeleteLcpFile);
  DEB_LCP(("(%u)Completed writing with ValidFlag = 1 for tab(%u,%u).%u",
           instance(),
           deleteLcpFilePtr.p->tableId,
           deleteLcpFilePtr.p->fragmentId,
           c_lqh->getCreateSchemaVersion(deleteLcpFilePtr.p->tableId)));
#endif
}

void
Backup::lcp_close_ctl_file_for_rewrite_done(Signal *signal,
                                            BackupRecordPtr ptr,
                                            BackupFilePtr filePtr)
{
  ndbrequire(filePtr.p->m_flags == 0);
  ndbrequire(ptr.p->errorCode == 0);
  DeleteLcpFilePtr deleteLcpFilePtr;
  c_deleteLcpFilePool.getPtr(deleteLcpFilePtr, ptr.p->currentDeleteLcpFile);

  if (deleteLcpFilePtr.p->firstFileId != RNIL)
  {
    jam();
    ptr.p->m_delete_data_file_ongoing = true;
    lcp_remove_file(signal, ptr, deleteLcpFilePtr);
  }
  else if (deleteLcpFilePtr.p->lcpCtlFileNumber != RNIL)
  {
    jam();
    ptr.p->m_delete_data_file_ongoing = false;
    lcp_remove_file(signal, ptr, deleteLcpFilePtr);
  }
  else
  {
    jam();
    finished_removing_files(signal, ptr);
  }
}

void
Backup::lcp_remove_file(Signal* signal,
                        BackupRecordPtr ptr,
                        DeleteLcpFilePtr deleteLcpFilePtr)
{
  BackupFilePtr filePtr;
  c_backupFilePool.getPtr(filePtr, ptr.p->deleteFilePtr);
  FsRemoveReq * req = (FsRemoveReq *)signal->getDataPtrSend();
  req->userReference = reference();
  req->userPointer = ptr.i;
  req->directory = 0;
  req->ownDirectory = 0;

  filePtr.p->m_flags |= BackupFile::BF_REMOVING;

  FsOpenReq::setVersion(req->fileNumber, 5);
  if (ptr.p->m_delete_data_file_ongoing)
  {
    jam();
    FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_DATA);
    FsOpenReq::v5_setLcpNo(req->fileNumber, deleteLcpFilePtr.p->firstFileId);
    DEB_LCP_DEL_FILES(("(%u)TAGD Remove data file: %u for tab(%u,%u)",
                       instance(),
                       deleteLcpFilePtr.p->firstFileId,
                       deleteLcpFilePtr.p->tableId,
                       deleteLcpFilePtr.p->fragmentId));
  }
  else
  {
    jam();
    FsOpenReq::setSuffix(req->fileNumber, FsOpenReq::S_CTL);
    FsOpenReq::v5_setLcpNo(req->fileNumber,
                           deleteLcpFilePtr.p->lcpCtlFileNumber);
    DEB_LCP_DEL_FILES(("(%u)TAGD Remove control file: %u for tab(%u,%u)",
                       instance(),
                       deleteLcpFilePtr.p->lcpCtlFileNumber,
                       deleteLcpFilePtr.p->tableId,
                       deleteLcpFilePtr.p->fragmentId));
  }
  FsOpenReq::v5_setTableId(req->fileNumber, deleteLcpFilePtr.p->tableId);
  FsOpenReq::v5_setFragmentId(req->fileNumber, deleteLcpFilePtr.p->fragmentId);
  sendSignal(NDBFS_REF, GSN_FSREMOVEREQ, signal, FsOpenReq::SignalLength, JBA);
}

void
Backup::lcp_remove_file_conf(Signal *signal, BackupRecordPtr ptr)
{
  BackupFilePtr filePtr;

  c_backupFilePool.getPtr(filePtr, ptr.p->deleteFilePtr);
  filePtr.p->m_flags &= (~(BackupFile::BF_REMOVING));
  ndbrequire(filePtr.p->m_flags == 0);

  if (ptr.p->m_delete_data_file_ongoing)
  {
    jam();
    DeleteLcpFilePtr deleteLcpFilePtr;
    c_deleteLcpFilePool.getPtr(deleteLcpFilePtr, ptr.p->currentDeleteLcpFile);
    if (deleteLcpFilePtr.p->firstFileId == deleteLcpFilePtr.p->lastFileId)
    {
      jam();
      /**
       * We're done with deleting the data files belonging to this LCP which
       * we no longer need. We continue with deletion of the control LCP
       * file for this LCP.
       */
      ptr.p->m_delete_data_file_ongoing = false;
      lcp_remove_file(signal, ptr, deleteLcpFilePtr);
      return;
    }
    /* Continue with deleting the next data file. */
    deleteLcpFilePtr.p->firstFileId =
      get_file_add(deleteLcpFilePtr.p->firstFileId, 1);
    lcp_remove_file(signal, ptr, deleteLcpFilePtr);
  }
  else
  {
    /**
     * We are done deleting files for this fragment LCP, send CONTINUEB
     * to see if more fragment LCPs are ready to be deleted.
     *
     * We remove it from queue here to ensure that the next LCP can now
     * start up again.
     * It is important to not remove it from queue until we actually deleted
     * all the files, the logic depends on that only one LCP is allowed to
     * execute at a time and that this LCP will remove all the files
     * of the old LCP before the next one is allowed to start.
     */
    jam();
    finished_removing_files(signal, ptr);
  }
}

void
Backup::finished_removing_files(Signal *signal,
                                BackupRecordPtr ptr)
{
  DeleteLcpFilePtr deleteLcpFilePtr;
  jam();
  {
    LocalDeleteLcpFile_list queue(c_deleteLcpFilePool,
                                  m_delete_lcp_file_head);
    c_deleteLcpFilePool.getPtr(deleteLcpFilePtr, ptr.p->currentDeleteLcpFile);
    queue.remove(deleteLcpFilePtr);
    c_deleteLcpFilePool.release(deleteLcpFilePtr);
    ptr.p->currentDeleteLcpFile = RNIL;
  }
  if (ptr.p->m_informDropTabTableId != Uint32(~0))
  {
    jam();
    sendINFORM_BACKUP_DROP_TAB_CONF(signal, ptr);
  }
  else
  {
    jam();
    check_wait_end_lcp(signal, ptr);
  }
  {
    LocalDeleteLcpFile_list queue(c_deleteLcpFilePool,
                                  m_delete_lcp_file_head);
    if (!queue.isEmpty())
    {
      jam();
      signal->theData[0] = BackupContinueB::ZDELETE_LCP_FILE;
      signal->theData[1] = ptr.i;
      sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
    }
    else
    {
      jam();
      delete_lcp_file_processing(signal, ptr.i);
    }
  }
}

void
Backup::execINFORM_BACKUP_DROP_TAB_REQ(Signal *signal)
{
  BackupRecordPtr ptr;
  ndbrequire(c_backups.first(ptr));
  ptr.p->m_informDropTabTableId = signal->theData[0];
  ptr.p->m_informDropTabReference = signal->theData[1];
  if (ptr.p->currentDeleteLcpFile != RNIL)
  {
    DeleteLcpFilePtr deleteLcpFilePtr;
    jam();
    c_deleteLcpFilePool.getPtr(deleteLcpFilePtr, ptr.p->currentDeleteLcpFile);
    if (deleteLcpFilePtr.p->tableId == ptr.p->m_informDropTabTableId)
    {
      jam();
      /**
       * The current delete record is deleting files and writing files
       * from the dropped table. Wait until this is completed before
       * we continue.
       */
      return;
    }
  }
  sendINFORM_BACKUP_DROP_TAB_CONF(signal, ptr);
}

void
Backup::check_wait_end_lcp(Signal *signal, BackupRecordPtr ptr)
{
  LocalDeleteLcpFile_list queue(c_deleteLcpFilePool,
                                m_delete_lcp_file_head);
  if (queue.isEmpty() && ptr.p->m_wait_end_lcp)
  {
    jam();
    ndbrequire(ptr.p->prepareState != PREPARE_DROP);
    ptr.p->m_wait_end_lcp = false;
    sendEND_LCPCONF(signal, ptr);
  }
}

void
Backup::sendINFORM_BACKUP_DROP_TAB_CONF(Signal *signal,
                                        BackupRecordPtr ptr)
{
  /**
   * Before we send the confirm we have to remove all entries from
   * drop delete queue that refer to the dropped table. We have already
   * ensured that the dropped table isn't currently involved in drops.
   * It would create complex code if we could remove the LCP files
   * while we were writing them.
   */

  DEB_LCP(("(%u)Remove all delete file requests for table %u",
           instance(),
           ptr.p->m_informDropTabTableId));
  {
    DeleteLcpFilePtr deleteLcpFilePtr;
    DeleteLcpFilePtr nextDeleteLcpFilePtr;
    LocalDeleteLcpFile_list queue(c_deleteLcpFilePool,
                                  m_delete_lcp_file_head);
    bool is_next_available = queue.first(deleteLcpFilePtr);
    while (is_next_available)
    {
      nextDeleteLcpFilePtr = deleteLcpFilePtr;
      is_next_available = queue.next(nextDeleteLcpFilePtr);
      if (deleteLcpFilePtr.p->tableId == ptr.p->m_informDropTabTableId)
      {
        jam();
        /**
         * We found an entry that is from the dropped table, we can
         * ignore this since the table will be dropped and all
         * LCP files with it.
         */
        queue.remove(deleteLcpFilePtr);
        c_deleteLcpFilePool.release(deleteLcpFilePtr);
      }
      deleteLcpFilePtr = nextDeleteLcpFilePtr;
    }
  }
  check_wait_end_lcp(signal, ptr);

  /**
   * Now we have removed all entries from queue and we are ready to inform
   * LQH that he can continue dropping the table.
   * At this point LQH have already ensured that no more LCPs are started
   * on this table.
   */
  BlockReference ref = ptr.p->m_informDropTabReference;
  Uint32 tableId = ptr.p->m_informDropTabTableId;
  signal->theData[0] = tableId;
  sendSignal(ref, GSN_INFORM_BACKUP_DROP_TAB_CONF, signal, 1, JBB);
  ptr.p->m_informDropTabReference = Uint32(~0);
  ptr.p->m_informDropTabTableId = Uint32(~0);
}

void
Backup::openFilesReplyLCP(Signal* signal, 
		          BackupRecordPtr ptr,
                          BackupFilePtr filePtr)
{
  /**
   * Did open succeed
   */
  if(ptr.p->checkError()) 
  {
    jam();
    if(ptr.p->errorCode == FsRef::fsErrFileExists)
    {
      jam();
      ptr.p->errorCode = DefineBackupRef::FailedForBackupFilesAleadyExist;
    }
    for (Uint32 i = 0; i < ptr.p->m_num_lcp_files; i++)
    {
      jam();
      if (ptr.p->dataFilePtr[i] == filePtr.i)
      {
        jam();
        /* Currently we can't handle failures to open data file */
        g_eventLogger->critical("Fatal: Open file of LCP data file %u failed,"
                                " errCode: %u",
                                i,
                                ptr.p->errorCode);
        ndbrequire(false);
        return;
      }
    }
    if (ptr.p->deleteFilePtr == filePtr.i)
    {
      jam();
      g_eventLogger->critical("Fatal: Reopen LCP control file failed,"
                              " errCode: %u",
                              ptr.p->errorCode);
      ndbrequire(false);
      return;
    }
    defineBackupRef(signal, ptr);
    return;
  }//if

  if (ptr.p->deleteFilePtr == filePtr.i)
  {
    jam();
    lcp_open_ctl_file_for_rewrite_done(signal, filePtr);
    return;
  }
  if (filePtr.p->m_flags & BackupFile::BF_HEADER_FILE)
  {
    jam();
    filePtr.p->m_flags &= ~(Uint32)BackupFile::BF_HEADER_FILE;
    ndbrequire(filePtr.i == ptr.p->prepareCtlFilePtr[0] ||
               filePtr.i == ptr.p->prepareCtlFilePtr[1]);
    lcp_open_ctl_file_done(signal, ptr, filePtr);
    return;
  }
  TablePtr tabPtr;
  bool prepare_phase;
  Uint32 index;
  if (filePtr.i == ptr.p->prepareDataFilePtr[0])
  {
    jam();
    filePtr.p->m_flags |= BackupFile::BF_LCP_META;
    ndbrequire(ptr.p->prepareState == PREPARE_OPEN_DATA_FILE);
    ptr.p->prepareState = PREPARE_READ_TABLE_DESC;
    ptr.p->prepare_table.first(tabPtr);
    prepare_phase = true;
  }
  else
  {
    prepare_phase = true;
    for (index = 0 ; index < ptr.p->m_num_lcp_files; index++)
    {
      if (filePtr.i == ptr.p->dataFilePtr[index])
      {
        prepare_phase = false;
        break;
      }
    }
    ndbrequire(!prepare_phase);
    ptr.p->tables.first(tabPtr);
  }
  ndbrequire(insertFileHeader(BackupFormat::LCP_FILE, ptr.p, filePtr.p));
  /**
   * Insert table list in ctl file
   */
  FsBuffer & buf = filePtr.p->operation.dataBuffer;
  const Uint32 sz = (sizeof(BackupFormat::CtlFile::TableList) >> 2);
  Uint32 * dst;
  ndbrequire(sz < buf.getMaxWrite());
  ndbrequire(buf.getWritePtr(&dst, sz))
  
  BackupFormat::CtlFile::TableList* tl = 
    (BackupFormat::CtlFile::TableList*)dst;

  tl->SectionType   = htonl(BackupFormat::TABLE_LIST);
  tl->SectionLength = htonl(sz);
  tl->TableIds[0] = htonl(tabPtr.p->tableId);
  buf.updateWritePtr(sz);

  if (prepare_phase)
  {
    jam();
    /**
     * Start getting table definition data
     */
    signal->theData[0] = BackupContinueB::BUFFER_FULL_META;
    signal->theData[1] = ptr.i;
    signal->theData[2] = tabPtr.i;
    signal->theData[3] = filePtr.i;
    sendSignal(reference(), GSN_CONTINUEB, signal, 4, JBB);
    return;
  }
  else
  {
    jam();
    FragmentPtr fragPtr;
    tabPtr.p->fragments.getPtr(fragPtr, 0);
    init_file_for_lcp(signal, index, ptr, ptr.i);
    ptr.p->m_num_lcp_data_files_open++;
    ndbrequire(ptr.p->m_num_lcp_data_files_open <= ptr.p->m_num_lcp_files);
    if (ptr.p->m_num_lcp_data_files_open < ptr.p->m_num_lcp_files)
    {
      jam();
      return;
    }
    /**
     * Now all files are open and we can start the actual scanning.
     * We always use the first file record to track number of scanned
     * pages.
     */
    BackupFilePtr zeroFilePtr;
    c_backupFilePool.getPtr(zeroFilePtr, ptr.p->dataFilePtr[0]);
    Uint32 delay = 0;
    if (ERROR_INSERTED(10047))
    {
      g_eventLogger->info("(%u)Start LCP on tab(%u,%u) 3 seconds delay, max_page: %u",
                          instance(),
                          tabPtr.p->tableId,
                          fragPtr.p->fragmentId,
                          ptr.p->m_lcp_max_page_cnt);

      if (ptr.p->m_lcp_max_page_cnt > 20)
      {
        delay = 3000;
      }
    }
    sendScanFragReq(signal, ptr, zeroFilePtr, tabPtr, fragPtr, delay);
  }
}

void
Backup::execEND_LCPREQ(Signal* signal)
{
  BackupRecordPtr ptr;
  {
    EndLcpReq* req= (EndLcpReq*)signal->getDataPtr();
    c_backupPool.getPtr(ptr, req->backupPtr);
    ptr.p->senderData = req->senderData;
  }
  jamEntry();

  BackupFilePtr filePtr;
  ptr.p->files.getPtr(filePtr, ptr.p->prepareCtlFilePtr[0]);
  ndbrequire(filePtr.p->m_flags == 0);
  ptr.p->files.getPtr(filePtr, ptr.p->prepareCtlFilePtr[1]);
  ndbrequire(filePtr.p->m_flags == 0);
  ptr.p->files.getPtr(filePtr, ptr.p->prepareDataFilePtr[0]);
  ndbrequire(filePtr.p->m_flags == 0);
  ptr.p->files.getPtr(filePtr, ptr.p->ctlFilePtr);
  ndbrequire(filePtr.p->m_flags == 0);
  ptr.p->files.getPtr(filePtr, ptr.p->dataFilePtr[0]);
  ndbrequire(filePtr.p->m_flags == 0);

  ptr.p->errorCode = 0;
  ptr.p->slaveState.setState(CLEANING);
  ptr.p->slaveState.setState(INITIAL);
  ptr.p->slaveState.setState(DEFINING);
  ptr.p->slaveState.setState(DEFINED);

  DEB_LCP(("(%u)TAGE Send SYNC_EXTENT_PAGES_REQ", instance()));
  /**
   * As part of ending the LCP we need to ensure that the extent pages
   * are synchronised. This is to ensure that the case with dropped
   * tables after completing a fragment LCP is handled properly. These
   * extent pages need to be synchronised at end of LCP since after the
   * end of the LCP here we will inform TSMAN that it is free to start
   * sharing those pages again and then we need to ensure that the
   * free status is up-to-date in preparation for a potential restart.
   */
  ptr.p->m_wait_final_sync_extent = true;
  ptr.p->m_num_sync_extent_pages_written = Uint32(~0);
  {
    SyncExtentPagesReq *req = (SyncExtentPagesReq*)signal->getDataPtrSend();
    req->senderData = ptr.i;
    req->senderRef = reference();
    req->lcpOrder = SyncExtentPagesReq::END_LCP;
    ptr.p->m_first_fragment = false;
    sendSignal(PGMAN_REF, GSN_SYNC_EXTENT_PAGES_REQ, signal,
               SyncExtentPagesReq::SignalLength, JBB);
  }
  return;
}

void
Backup::finish_end_lcp(Signal *signal, BackupRecordPtr ptr)
{
  DEB_LCP(("(%u)TAGE SYNC_EXTENT_PAGES_CONF: lcpId: %u",
          instance(),
          ptr.p->backupId));
  ptr.p->m_wait_final_sync_extent = false;
  LocalDeleteLcpFile_list queue(c_deleteLcpFilePool,
                                m_delete_lcp_file_head);
  if (!queue.isEmpty())
  {
    jam();
    ptr.p->m_wait_end_lcp = true;
    return;
  }
  /**
   * The delete LCP file queue is empty, this means that we are sure
   * that all reported LCP_FRAG_REP's are actually completed. DIH
   * will not think that any LCP_FRAG_REP is ok to use until we have
   * received LCP_COMPLETE_REP and so we need to wait with sending
   * this signal until we have emptied the queue and thus completed
   * the full LCP.
   */
  sendEND_LCPCONF(signal, ptr);
}

void
Backup::sendEND_LCPCONF(Signal *signal, BackupRecordPtr ptr)
{
  DEB_LCP(("(%u)TAGE END_LCPREQ: lcpId: %u",
          instance(),
          ptr.p->backupId));
  ndbrequire(!ptr.p->m_wait_end_lcp);
  ptr.p->backupId = 0; /* Ensure next LCP_PREPARE_REQ sees a new LCP id */
  EndLcpConf* conf= (EndLcpConf*)signal->getDataPtrSend();
  conf->senderData = ptr.p->senderData;
  conf->senderRef = reference();
  sendSignal(ptr.p->masterRef, GSN_END_LCPCONF,
	     signal, EndLcpConf::SignalLength, JBA);
}

void
Backup::lcp_close_ctl_file_drop_case(Signal *signal, BackupRecordPtr ptr)
{
  BackupFilePtr filePtr;
  for (Uint32 i = 0; i < 2; i++)
  {
    c_backupFilePool.getPtr(filePtr, ptr.p->prepareCtlFilePtr[i]);
    if ((filePtr.p->m_flags & BackupFile::BF_OPEN) != 0)
    {
      jam();
      /* Still waiting for second file to close */
      return;
    }
  }
  /* Now time to start removing data files. */
  DeleteLcpFilePtr deleteLcpFilePtr;
  TablePtr tabPtr;
  FragmentPtr fragPtr;
  ndbrequire(c_deleteLcpFilePool.seize(deleteLcpFilePtr));
  LocalDeleteLcpFile_list queue(c_deleteLcpFilePool,
                                m_delete_lcp_file_head);

  /**
   * We avoid all complexity for this code since it is an obscure case that
   * should be extremely rare. So we simply delete all potential files.
   */
  ptr.p->prepare_table.first(tabPtr);
  tabPtr.p->fragments.getPtr(fragPtr, 0);
  deleteLcpFilePtr.p->tableId = fragPtr.p->tableId;
  deleteLcpFilePtr.p->fragmentId = fragPtr.p->fragmentId;
  deleteLcpFilePtr.p->firstFileId = 0;
  deleteLcpFilePtr.p->lastFileId = BackupFormat::NDB_MAX_LCP_FILES - 1;
  deleteLcpFilePtr.p->waitCompletedGci = 0;
  deleteLcpFilePtr.p->validFlag = 1;
  deleteLcpFilePtr.p->lcpCtlFileNumber =
    ptr.p->prepareNextLcpCtlFileNumber == 0 ? 1 : 0;
  queue.addFirst(deleteLcpFilePtr);
  if (!m_delete_lcp_files_ongoing)
  {
    jam();
    m_delete_lcp_files_ongoing = true;
    signal->theData[0] = BackupContinueB::ZDELETE_LCP_FILE;
    signal->theData[1] = ptr.i;
    sendSignal(reference(), GSN_CONTINUEB, signal, 2, JBB);
  }
  /**
   * We have now closed the files and as soon as the queue of
   * deleted files are empty we can proceed with starting of
   * the LCP.
   */
  ptr.p->prepareState = PREPARE_DROP;
  DEB_LCP(("(%u)TAGT Insert delete files in queue (drop case):"
    " tab(%u,%u), createGci: %u, waitCompletedGCI: 0",
    instance(),
    fragPtr.p->tableId,
    fragPtr.p->fragmentId,
    fragPtr.p->createGci));
}

inline
static 
void setWords(const Uint64 src, Uint32& hi, Uint32& lo)
{
  hi = (Uint32) (src >> 32);
  lo = (Uint32) (src & 0xffffffff);
}

void
Backup::execLCP_STATUS_REQ(Signal* signal)
{
  jamEntry();
  const LcpStatusReq* req = (const LcpStatusReq*) signal->getDataPtr();
  
  const Uint32 senderRef = req->senderRef;
  const Uint32 senderData = req->senderData;
  Uint32 failCode = LcpStatusRef::NoLCPRecord;

  /* Find LCP backup, if there is one */
  BackupRecordPtr ptr;
  bool found_lcp = false;
  for (c_backups.first(ptr); ptr.i != RNIL; c_backups.next(ptr))
  {
    jam();
    if (ptr.p->is_lcp())
    {
      jam();
      ndbrequire(found_lcp == false); /* Just one LCP */
      found_lcp = true;
      
      LcpStatusConf::LcpState state = LcpStatusConf::LCP_IDLE;
      if (ptr.p->m_wait_end_lcp)
      {
        jam();
        state = LcpStatusConf::LCP_WAIT_END_LCP;
      }
      else if (ptr.p->m_wait_final_sync_extent)
      {
        jam();
        state = LcpStatusConf::LCP_WAIT_FINAL_SYNC_EXTENT;
      }
      else
      {
        jam();
        switch (ptr.p->slaveState.getState())
        {
        case STARTED:
          jam();
          state = LcpStatusConf::LCP_PREPARED;
          break;
        case SCANNING:
          jam();
          state = LcpStatusConf::LCP_SCANNING;
          break;
        case STOPPING:
          jam();
          if (ptr.p->m_wait_disk_data_sync)
          {
            jam();
            state = LcpStatusConf::LCP_WAIT_SYNC_DISK;
          }
          else if (ptr.p->m_wait_sync_extent)
          {
            jam();
            state = LcpStatusConf::LCP_WAIT_SYNC_EXTENT;
          }
          else if (ptr.p->m_wait_data_file_close)
          {
            jam();
            state = LcpStatusConf::LCP_SCANNED;
          }
          else if (ptr.p->m_empty_lcp)
          {
            jam();
            state = LcpStatusConf::LCP_WAIT_CLOSE_EMPTY;
          }
          else
          {
            jam();
            state = LcpStatusConf::LCP_WAIT_WRITE_CTL_FILE;
          }
          break;
        case DEFINED:
          jam();
          if (ptr.p->prepareState == NOT_ACTIVE ||
              ptr.p->prepareState == PREPARED)
          {
            jam();
            state = LcpStatusConf::LCP_IDLE;
          }
          else if (ptr.p->prepareState == PREPARE_READ_CTL_FILES)
          {
            jam();
            state = LcpStatusConf::LCP_PREPARE_READ_CTL_FILES;
          }
          else if (ptr.p->prepareState == PREPARE_OPEN_DATA_FILE)
          {
            jam();
            state = LcpStatusConf::LCP_PREPARE_OPEN_DATA_FILE;
          }
          else if (ptr.p->prepareState == PREPARE_READ_TABLE_DESC)
          {
            jam();
            state = LcpStatusConf::LCP_PREPARE_READ_TABLE_DESC;
          }
          else if (ptr.p->prepareState == PREPARE_ABORTING)
          {
            jam();
            state = LcpStatusConf::LCP_PREPARE_ABORTING;
          }
          else if (ptr.p->prepareState == PREPARE_DROP ||
                   ptr.p->prepareState == PREPARE_DROP_CLOSE)
          {
            jam();
            state = LcpStatusConf::LCP_PREPARE_WAIT_DROP_CASE;
          }
          else
          {
            jam();
            ndbout_c("Unusual LCP prepare state in LCP_STATUS_REQ() : %u",
                     ptr.p->prepareState);
            state = LcpStatusConf::LCP_IDLE;
          }
          break;
        default:
          jam();
          ndbout_c("Unusual LCP state in LCP_STATUS_REQ() : %u",
                   ptr.p->slaveState.getState());
          state = LcpStatusConf::LCP_IDLE;
        };
      }
        
      /* Not all values are set here */
      const Uint32 UnsetConst = ~0;
      
      LcpStatusConf* conf = (LcpStatusConf*) signal->getDataPtrSend();
      conf->senderRef = reference();
      conf->senderData = senderData;
      conf->lcpState = state;
      conf->tableId = UnsetConst;
      conf->fragId = UnsetConst;
      conf->completionStateHi = UnsetConst;
      conf->completionStateLo = UnsetConst;
      setWords(ptr.p->noOfRecords,
               conf->lcpDoneRowsHi,
               conf->lcpDoneRowsLo);
      setWords(ptr.p->noOfBytes,
               conf->lcpDoneBytesHi,
               conf->lcpDoneBytesLo);
      conf->lcpScannedPages = 0;
      
      if (state == LcpStatusConf::LCP_SCANNING ||
          state == LcpStatusConf::LCP_WAIT_SYNC_DISK ||
          state == LcpStatusConf::LCP_WAIT_SYNC_EXTENT ||
          state == LcpStatusConf::LCP_WAIT_WRITE_CTL_FILE ||
          state == LcpStatusConf::LCP_WAIT_CLOSE_EMPTY ||
          state == LcpStatusConf::LCP_SCANNED)
      {
        jam();
        /* Actually scanning/closing a fragment, let's grab the details */
        TablePtr tabPtr;
        FragmentPtr fragPtr;
        BackupFilePtr filePtr;
        
        if (ptr.p->dataFilePtr[0] == RNIL)
        {
          jam();
          failCode = LcpStatusRef::NoFileRecord;
          break;
        }
        c_backupFilePool.getPtr(filePtr, ptr.p->dataFilePtr[0]);
        ndbrequire(filePtr.p->backupPtr == ptr.i);

        ptr.p->tables.first(tabPtr);
        if (tabPtr.i != RNIL)
        {
          jam();
          tabPtr.p->fragments.getPtr(fragPtr, 0);
          ndbrequire(fragPtr.p->tableId == tabPtr.p->tableId);
          conf->tableId = tabPtr.p->tableId;
          conf->fragId = fragPtr.p->fragmentId;
        }
        
        if (state == LcpStatusConf::LCP_SCANNING)
        {
          jam();
          setWords(filePtr.p->operation.noOfRecords,
                   conf->completionStateHi,
                   conf->completionStateLo);
          conf->lcpScannedPages = filePtr.p->operation.lcpScannedPages;
        }
        else if (state == LcpStatusConf::LCP_SCANNED)
        {
          jam();
          BackupFilePtr tmp_filePtr;
          Uint64 flushBacklog = 0;
          for (Uint32 i = 0; i < ptr.p->m_num_lcp_files; i++)
          {
            c_backupFilePool.getPtr(tmp_filePtr, ptr.p->dataFilePtr[i]);
            /* May take some time to drain the FS buffer, depending on
             * size of buff, achieved rate.
             * We provide the buffer fill level so that requestors
             * can observe whether there's progress in this phase.
             */
            flushBacklog +=
              tmp_filePtr.p->operation.dataBuffer.getUsableSize() -
              tmp_filePtr.p->operation.dataBuffer.getFreeSize();
          }
          setWords(flushBacklog,
                   conf->completionStateHi,
                   conf->completionStateLo);
        }
        else if (state == LcpStatusConf::LCP_WAIT_SYNC_DISK)
        {
          jam();
          conf->completionStateHi = 0;
          conf->completionStateLo = ptr.p->m_num_sync_pages_waiting;
        }
        else if (state == LcpStatusConf::LCP_WAIT_SYNC_EXTENT)
        {
          jam();
          conf->completionStateHi = 0;
          conf->completionStateLo = ptr.p->m_num_sync_extent_pages_written;
        }
        else if (state == LcpStatusConf::LCP_WAIT_WRITE_CTL_FILE)
        {
          jam();
          conf->completionStateHi = 0;
          conf->completionStateLo = 0;
        }
        else if (state == LcpStatusConf::LCP_WAIT_CLOSE_EMPTY)
        {
          jam();
          conf->completionStateHi = 0;
          conf->completionStateLo = ptr.p->m_outstanding_operations;
        }
        else
        {
          ndbrequire(false); // Impossible state
        }
      }
      else if (state == LcpStatusConf::LCP_WAIT_END_LCP)
      {
        jam();
        DeleteLcpFilePtr deleteLcpFilePtr;
        LocalDeleteLcpFile_list queue(c_deleteLcpFilePool,
                                      m_delete_lcp_file_head);
        ndbrequire(!queue.isEmpty());
        conf->completionStateHi = 0;
        conf->completionStateLo = m_newestRestorableGci;
      }
      else if (state == LcpStatusConf::LCP_WAIT_FINAL_SYNC_EXTENT)
      {
        jam();
        conf->completionStateHi = 0;
        conf->completionStateLo = ptr.p->m_num_sync_extent_pages_written;
      }
      else if (state == LcpStatusConf::LCP_PREPARED)
      {
        /**
         * We are in state of closing LCP control files with a
         * idle fragment LCP.
         */
        jam();
        TablePtr tabPtr;
        FragmentPtr fragPtr;
        ptr.p->tables.first(tabPtr);
        ndbrequire(tabPtr.i != RNIL);
        tabPtr.p->fragments.getPtr(fragPtr, 0);
        ndbrequire(fragPtr.p->tableId == tabPtr.p->tableId);
        conf->tableId = tabPtr.p->tableId;
        conf->fragId = fragPtr.p->fragmentId;
      }
      
      failCode = 0;
    }
  }

  if (failCode == 0)
  {
    jam();
    sendSignal(senderRef, GSN_LCP_STATUS_CONF, 
               signal, LcpStatusConf::SignalLength, JBB);
    return;
  }

  jam();
  LcpStatusRef* ref = (LcpStatusRef*) signal->getDataPtrSend();
  
  ref->senderRef = reference();
  ref->senderData = senderData;
  ref->error = failCode;
  
  sendSignal(senderRef, GSN_LCP_STATUS_REF, 
             signal, LcpStatusRef::SignalLength, JBB);
  return;
}

bool Backup::g_is_backup_running = false;
