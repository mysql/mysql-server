/*
   Copyright (c) 2003, 2024, Oracle and/or its affiliates.

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

#include "TransporterFacade.hpp"
#include <NdbEnv.h>
#include <NdbGetRUsage.h>
#include <NdbLockCpuUtil.h>
#include <NdbSleep.h>
#include <ndb_global.h>
#include <ndb_limits.h>
#include <IPCConfig.hpp>
#include <NdbOut.hpp>
#include <TransporterCallback.hpp>
#include <TransporterRegistry.hpp>
#include "ClusterMgr.hpp"
#include "NdbApiSignal.hpp"
#include "NdbWaiter.hpp"
#include "my_config.h"
#include "my_thread.h"
#include "trp_client.hpp"
#include "util/require.h"

#include <NdbConfig.h>
#include <kernel/GlobalSignalNumbers.h>
#include <kernel/ndb_limits.h>
#include <mgmapi_config_parameters.h>
#include <ndb_version.h>
#include <EventLogger.hpp>
#include <SignalLoggerManager.hpp>
#include <mgmapi_configuration.hpp>
#include <signaldata/AllocNodeId.hpp>
#include <signaldata/AlterTable.hpp>
#include <signaldata/CloseComReqConf.hpp>
#include <signaldata/SumaImpl.hpp>
#include "kernel/signaldata/DumpStateOrd.hpp"
#include "kernel/signaldata/TestOrd.hpp"

// #define REPORT_TRANSPORTER
// #define API_TRACE

static int numberToIndex(int number) { return number - MIN_API_BLOCK_NO; }

static int indexToNumber(int index) { return index + MIN_API_BLOCK_NO; }

#if 0
#define DEBUG_FPRINTF(arglist) \
  do {                         \
    fprintf arglist;           \
  } while (0)
#else
#define DEBUG_FPRINTF(a)
#endif

#if defined DEBUG_TRANSPORTER
#define TRP_DEBUG(t) ndbout << __FILE__ << ":" << __LINE__ << ":" << t << endl;
#else
#define TRP_DEBUG(t)
#endif

#define DBG_POLL 0
#define dbg(x, y) \
  if (DBG_POLL) printf("%llu : " x "\n", NdbTick_CurrentMillisecond(), y)
#define dbg2(x, y, z) \
  if (DBG_POLL) printf("%llu : " x "\n", NdbTick_CurrentMillisecond(), y, z)

/*****************************************************************************
 * Call back functions
 *****************************************************************************/
void TransporterFacade::reportError(NodeId nodeId, TransporterError errorCode,
                                    const char *info) {
#ifdef REPORT_TRANSPORTER
  g_eventLogger->info("REPORT_TRANSP: reportError (nodeId=%d, errorCode=%d) %s",
                      (int)nodeId, (int)errorCode, info ? info : "");
#endif
  if (errorCode & TE_DO_DISCONNECT) {
    g_eventLogger->error(
        "Node %u disconnecting from node %u due to transporter error %u %s",
        ownId(), nodeId, errorCode, info ? info : "");
    if (nodeId == ownId()) {
      g_eventLogger->info("Error on loopback transporter is fatal.");
      abort();
    }
    DEBUG_FPRINTF((stderr, "(%u)FAC:reportError(%u, %d, %s)\n", ownId(), nodeId,
                   (int)errorCode, info));
    startDisconnecting(nodeId);
  }
}

/**
 * Report average send length in bytes (4096 last sends)
 */
void TransporterFacade::reportSendLen(NodeId nodeId [[maybe_unused]],
                                      Uint32 count [[maybe_unused]],
                                      Uint64 bytes [[maybe_unused]]) {
#ifdef REPORT_TRANSPORTER
  g_eventLogger->info(
      "REPORT_TRANSP: reportSendLen (nodeId=%d, bytes/count=%d)", (int)nodeId,
      (Uint32)(bytes / count));
#endif
}

/**
 * Report average receive length in bytes (4096 last receives)
 */
void TransporterFacade::reportReceiveLen(NodeId nodeId [[maybe_unused]],
                                         Uint32 count [[maybe_unused]],
                                         Uint64 bytes [[maybe_unused]]) {
#ifdef REPORT_TRANSPORTER
  g_eventLogger->info(
      "REPORT_TRANSP: reportReceiveLen (nodeId=%d, bytes/count=%d)",
      (int)nodeId, (Uint32)(bytes / count));
#endif
}

/**
 * Report connection established
 */
void TransporterFacade::reportConnect(NodeId nodeId) {
#ifdef REPORT_TRANSPORTER
  g_eventLogger->info("REPORT_TRANSP: API reportConnect (nodeId=%d)",
                      (int)nodeId);
#endif
  DEBUG_FPRINTF((stderr, "(%u)FAC:reportConnect(%u)\n", ownId(), nodeId));
  reportConnected(nodeId);
}

/**
 * Report connection broken
 */
void TransporterFacade::reportDisconnect(NodeId nodeId,
                                         Uint32 error [[maybe_unused]]) {
  DEBUG_FPRINTF(
      (stderr, "(%u)FAC:reportDisconnect(%u, %u)\n", ownId(), nodeId, error));
#ifdef REPORT_TRANSPORTER
  g_eventLogger->info("REPORT_TRANSP: API reportDisconnect (nodeId=%d)",
                      (int)nodeId);
#endif
  reportDisconnected(nodeId);
}

void TransporterFacade::transporter_recv_from(NodeId nodeId) {
  hb_received(nodeId);
}

/****************************************************************************
 *
 *****************************************************************************/

/**
 * Report connection broken
 */
int TransporterFacade::checkJobBuffer() { return 0; }

#ifdef API_TRACE
static const char *API_SIGNAL_LOG = "API_SIGNAL_LOG";
static const char *apiSignalLog = nullptr;
static SignalLoggerManager signalLogger;
static inline bool setSignalLog() {
  signalLogger.flushSignalLog();

  const char *tmp = NdbEnv_GetEnv(API_SIGNAL_LOG, (char *)nullptr, 0);
  if (tmp != nullptr && apiSignalLog != nullptr &&
      strcmp(tmp, apiSignalLog) == 0) {
    return true;
  } else if (tmp == nullptr && apiSignalLog == nullptr) {
    return false;
  } else if (tmp == nullptr && apiSignalLog != nullptr) {
    signalLogger.setOutputStream(nullptr);
    apiSignalLog = tmp;
    return false;
  } else if (tmp != nullptr) {
    if (strcmp(tmp, "-") == 0) signalLogger.setOutputStream(stdout);
#ifndef NDEBUG
    else if (strcmp(tmp, "+") == 0)
      signalLogger.setOutputStream(DBUG_FILE);
#endif
    else
      signalLogger.setOutputStream(fopen(tmp, "w"));
    apiSignalLog = tmp;
    return true;
  }
  return false;
}
inline bool TRACE_GSN(Uint32 gsn) {
  switch (gsn) {
#ifndef TRACE_APIREGREQ
    case GSN_API_REGREQ:
    case GSN_API_REGCONF:
      return false;
#endif
#if 1
    case GSN_SUB_GCP_COMPLETE_REP:
    case GSN_SUB_GCP_COMPLETE_ACK:
      return false;
#endif
    default:
      return true;
  }
}
#endif

/**
 * The execute function : Handle received signal
 */
bool TransporterFacade::deliver_signal(SignalHeader *const header,
                                       Uint8 prio [[maybe_unused]],
                                       TransporterError & /*error_code*/,
                                       Uint32 *const theData,
                                       LinearSectionPtr ptr[3]) {
  Uint32 tRecBlockNo = header->theReceiversBlockNumber;

#ifdef API_TRACE
  if (setSignalLog() && TRACE_GSN(header->theVerId_signalNumber)) {
    signalLogger.executeSignal(*header, prio, theData, ownId(), ptr,
                               header->m_noOfSections);
    signalLogger.flushSignalLog();
  }
#endif

  if (tRecBlockNo >= MIN_API_BLOCK_NO) {
    trp_client *clnt = m_threads.get(tRecBlockNo);
    if (clnt != nullptr) {
      const bool client_locked = clnt->is_locked_for_poll();
      /**
       * Handle received signal immediately to avoid any unnecessary
       * copying of data, allocation of memory and other things. Copying
       * of data could be interesting to support several priority levels
       * and to support a special memory structure when executing the
       * signals. Neither of those are interesting when receiving data
       * in the NDBAPI. The NDBAPI will thus read signal data directly as
       * it was written by the sender (Shared memory sender is other
       * process and TCP/IP sender is the OS that writes the TCP/IP
       * message into a message buffer).
       */
      NdbApiSignal tmpSignal(*header);
      NdbApiSignal *tSignal = &tmpSignal;
      tSignal->setDataPtr(theData);
      if (!client_locked) {
        lock_client(clnt);
      }
      assert(clnt->check_if_locked());
      clnt->trp_deliver_signal(tSignal, ptr);
    } else {
      handleMissingClnt(header, theData);
    }
  } else if (tRecBlockNo == API_PACKED) {
    /**
     * Block number == 2047 is used to signal a signal that consists of
     * multiple instances of the same signal. This is an effort to
     * package the signals so as to avoid unnecessary communication
     * overhead since TCP/IP has a great performance impact.
     */
    Uint32 Tlength = header->theLength;
    Uint32 Tsent = 0;
    /**
     * Since it contains at least two data packets we will first
     * copy the signal data to safe place.
     */
    while (Tsent < Tlength) {
      Uint32 Theader = theData[Tsent];
      Tsent++;
      Uint32 TpacketLen = (Theader & 0x1F) + 3;
      tRecBlockNo = Theader >> 16;
      if (TpacketLen <= 25) {
        if ((TpacketLen + Tsent) <= Tlength) {
          /**
           * Set the data length of the signal and the receivers block
           * reference and then call the API.
           */
          header->theLength = TpacketLen;
          header->theReceiversBlockNumber = tRecBlockNo;
          Uint32 *tDataPtr = &theData[Tsent];
          Tsent += TpacketLen;
          if (tRecBlockNo >= MIN_API_BLOCK_NO) {
            trp_client *clnt = m_threads.get(tRecBlockNo);
            if (clnt != nullptr) {
              const bool client_locked = clnt->is_locked_for_poll();
              NdbApiSignal tmpSignal(*header);
              NdbApiSignal *tSignal = &tmpSignal;
              tSignal->setDataPtr(tDataPtr);
              if (!client_locked) {
                lock_client(clnt);
              }
              assert(clnt->check_if_locked());
              clnt->trp_deliver_signal(tSignal, nullptr);
            } else {
              handleMissingClnt(header, tDataPtr);
            }
          }
        }
      }
    }
  } else if (tRecBlockNo >= MIN_API_FIXED_BLOCK_NO &&
             tRecBlockNo <= MAX_API_FIXED_BLOCK_NO) {
    Uint32 dynamic = m_fixed2dynamic[tRecBlockNo - MIN_API_FIXED_BLOCK_NO];
    trp_client *clnt = m_threads.get(dynamic);
    if (clnt != nullptr) {
      const bool client_locked = clnt->is_locked_for_poll();
      NdbApiSignal tmpSignal(*header);
      NdbApiSignal *tSignal = &tmpSignal;
      tSignal->setDataPtr(theData);
      if (!client_locked) {
        lock_client(clnt);
      }
      assert(clnt->check_if_locked());
      clnt->trp_deliver_signal(tSignal, ptr);
    } else {
      handleMissingClnt(header, theData);
    }
  } else {
    // Ignore all other block numbers.
    if (header->theVerId_signalNumber == GSN_DUMP_STATE_ORD) {
      trp_client *clnt = get_poll_owner(false);
      require(clnt != nullptr);
      NdbApiSignal sig(*header);
      sig.setDataPtr(theData);
      assert(clnt->check_if_locked());
      theClusterMgr->execDUMP_STATE_ORD(&sig, ptr);
    } else if (header->theVerId_signalNumber != GSN_API_REGREQ) {
      TRP_DEBUG("TransporterFacade received signal to unknown block no.");
      fprintf(stderr,
              "%s NDBAPI FATAL ERROR : TransporterFacade received signal to "
              "unknown block number: %u sig %u",
              Logger::Timestamp().c_str(), tRecBlockNo,
              header->theVerId_signalNumber);
      ndberr << *header << "-- Signal Data --" << endl;
      ndberr.hexdump(theData, MAX(header->theLength, 25)) << flush;
      abort();
    }
  }

  /**
   * API_PACKED contains a number of messages,
   * We need to have space for all of them, a
   * maximum of six signals can be carried in
   * one packed signal to the NDB API.
   */
  const Uint32 MAX_MESSAGES_IN_LOCKED_CLIENTS = MAX_LOCKED_CLIENTS - 6;
  return m_locked_cnt >= MAX_MESSAGES_IN_LOCKED_CLIENTS;
}

#include <signaldata/TcCommit.hpp>
#include <signaldata/TcKeyConf.hpp>
#include <signaldata/TcKeyFailConf.hpp>

void TransporterFacade::handleMissingClnt(const SignalHeader *header,
                                          const Uint32 *theData) {
  Uint32 gsn = header->theVerId_signalNumber;
  Uint32 transId[2];
  if (gsn == GSN_TCKEYCONF || gsn == GSN_TCINDXCONF) {
    const TcKeyConf *conf = CAST_CONSTPTR(TcKeyConf, theData);
    if (TcKeyConf::getMarkerFlag(conf->confInfo) == false) {
      return;
    }
    transId[0] = conf->transId1;
    transId[1] = conf->transId2;
  } else if (gsn == GSN_TC_COMMITCONF) {
    const TcCommitConf *conf = CAST_CONSTPTR(TcCommitConf, theData);
    if ((conf->apiConnectPtr & 1) == 0) {
      return;
    }
    transId[0] = conf->transId1;
    transId[1] = conf->transId2;
  } else if (gsn == GSN_TCKEY_FAILCONF) {
    const TcKeyFailConf *conf = CAST_CONSTPTR(TcKeyFailConf, theData);
    if ((conf->apiConnectPtr & 1) == 0) {
      return;
    }
    transId[0] = conf->transId1;
    transId[1] = conf->transId2;
  } else {
    return;
  }

  // g_eventLogger->info(
  //     "KESO KESO KESO: sending commit ack marker 0x%.8x 0x%.8x (gsn: %u)",
  //     transId[0], transId[1], gsn);

  Uint32 ownBlockNo = header->theReceiversBlockNumber;
  Uint32 aTCRef = header->theSendersBlockRef;

  NdbApiSignal tSignal(numberToRef(ownBlockNo, ownId()));
  tSignal.theReceiversBlockNumber = refToBlock(aTCRef);
  tSignal.theVerId_signalNumber = GSN_TC_COMMIT_ACK;
  tSignal.theLength = 2;

  Uint32 *dataPtr = tSignal.getDataPtrSend();
  dataPtr[0] = transId[0];
  dataPtr[1] = transId[1];

  m_poll_owner->safe_sendSignal(&tSignal, refToNode(aTCRef));
}

// These symbols are needed, but not used in the API
void SignalLoggerManager::printSegmentedSection(
    FILE *, const SignalHeader &, const SegmentedSectionPtr /*ptr*/[3],
    unsigned /*i*/) {
  abort();
}

void copy(Uint32 *& /*insertPtr*/, class SectionSegmentPool & /*thePool*/,
          const SegmentedSectionPtr & /*_ptr*/) {
  abort();
}

/**
 * Note that this function needs no locking since it is
 * only called from the constructor of Ndb (the NdbObject)
 *
 * Which is protected by a mutex
 */

int TransporterFacade::start_instance(NodeId nodeId,
                                      const ndb_mgm_configuration *conf,
                                      bool tls_required) {
  DBUG_ENTER("TransporterFacade::start_instance");

  assert(theOwnTrpId == 0);
  assert(theOwnId == 0);
  theOwnId = nodeId;
  DEBUG_FPRINTF((stderr, "(%u)FAC:start_instance\n", ownId()));

#if defined SIGPIPE && !defined _WIN32
  (void)signal(SIGPIPE, SIG_IGN);
#endif

  theTransporterRegistry = new TransporterRegistry(this, this);
  if (theTransporterRegistry == nullptr) {
    DBUG_RETURN(-1);
  }

  if (!theTransporterRegistry->init(nodeId)) {
    DBUG_RETURN(-1);
  }

  theTransporterRegistry->init_tls(m_tls_search_path, m_tls_node_type,
                                   m_mgm_tls_level);

  if (tls_required && !theTransporterRegistry->getTlsKeyManager()->ctx()) {
    delete theTransporterRegistry;
    theTransporterRegistry = nullptr;
    DBUG_RETURN(-2);
  }

  if (theClusterMgr == nullptr) {
    theClusterMgr = new ClusterMgr(*this);
  }

  if (theClusterMgr == nullptr) {
    DBUG_RETURN(-1);
  }

  if (!configure(nodeId, conf)) {
    DBUG_RETURN(-1);
  }

  theReceiveThread = NdbThread_Create(runReceiveResponse_C, (void **)this,
                                      0,  // Use default stack size
                                      "ndb_receive", NDB_THREAD_PRIO_LOW);
  if (theReceiveThread == nullptr) {
    g_eventLogger->info(
        "TransporterFacade::start_instance:"
        " Failed to create thread for receive.");
    assert(theReceiveThread != nullptr);
    DBUG_RETURN(-1);
  }
  theSendThread = NdbThread_Create(runSendRequest_C, (void **)this,
                                   0,  // Use default stack size
                                   "ndb_send", NDB_THREAD_PRIO_LOW);
  if (theSendThread == nullptr) {
    g_eventLogger->info(
        "TransporterFacade::start_instance:"
        " Failed to create thread for send.");
    assert(theSendThread != nullptr);
    DBUG_RETURN(-1);
  }

  theWakeupThread = NdbThread_Create(runWakeupThread_C, (void **)this,
                                     0,  // Use default stack size
                                     "ndb_wakeup_thread", NDB_THREAD_PRIO_LOW);

  theClusterMgr->startThread();

  DBUG_RETURN(0);
}

void TransporterFacade::stop_instance() {
  DBUG_ENTER("TransporterFacade::stop_instance");

  DEBUG_FPRINTF((stderr, "(%u)FAC:stop_instance\n", ownId()));
  // Stop the send, wakeup and receive thread
  void *status;
  NdbMutex_Lock(m_wakeup_thread_mutex);
  theStopWakeup = 1;
  NdbMutex_Unlock(m_wakeup_thread_mutex);
  if (theWakeupThread) {
    NdbThread_WaitFor(theWakeupThread, &status);
    NdbThread_Destroy(&theWakeupThread);
  }
  theStopReceive = 1;
  if (theReceiveThread) {
    NdbThread_WaitFor(theReceiveThread, &status);
    NdbThread_Destroy(&theReceiveThread);
  }
  theStopSend = 1;
  if (theSendThread) {
    NdbThread_WaitFor(theSendThread, &status);
    NdbThread_Destroy(&theSendThread);
  }

  // Stop clustmgr last as (currently) recv thread accesses clusterMgr
  if (theClusterMgr) {
    theClusterMgr->doStop();
  }

  DBUG_VOID_RETURN;
}

void TransporterFacade::setSendThreadInterval(Uint32 ms) {
  if (ms > 0 && ms <= 10) {
    sendThreadWaitMillisec = ms;
  }
}

Uint32 TransporterFacade::getSendThreadInterval(void) const {
  return sendThreadWaitMillisec;
}

extern "C" void *runSendRequest_C(void *me) {
  ((TransporterFacade *)me)->threadMainSend();
  return nullptr;
}

static inline void link_buffer(TFBuffer *dst, const TFBuffer *src) {
  assert(dst);
  assert(src);
  assert(src->m_head);
  assert(src->m_tail);
  TFBufferGuard g0(*dst);
  TFBufferGuard g1(*src);
  if (dst->m_head == nullptr) {
    dst->m_head = src->m_head;
  } else {
    dst->m_tail->m_next = src->m_head;
  }
  dst->m_tail = src->m_tail;
  dst->m_bytes_in_buffer += src->m_bytes_in_buffer;
}

static const Uint32 SEND_THREAD_NO = 0;

extern "C" void *runWakeupThread_C(void *me) {
  ((TransporterFacade *)me)->threadMainWakeup();
  return nullptr;
}

void TransporterFacade::init_cpu_usage(NDB_TICKS currTime) {
  struct ndb_rusage curr_rusage;
  Ndb_GetRUsage(&curr_rusage, false);
  Uint64 cpu_time = curr_rusage.ru_utime + curr_rusage.ru_stime;
  m_last_recv_thread_cpu_usage_in_micros = cpu_time;
  m_recv_thread_cpu_usage_in_percent = 0;
  m_last_cpu_usage_check = currTime;
  calc_recv_thread_wakeup();
}

void TransporterFacade::check_cpu_usage(NDB_TICKS currTime) {
  struct ndb_rusage curr_rusage;
  Uint64 expired_time_in_micros =
      NdbTick_Elapsed(m_last_cpu_usage_check, currTime).microSec();
  if (expired_time_in_micros < Uint64(1000000)) return;

  m_last_cpu_usage_check = currTime;
  int res = Ndb_GetRUsage(&curr_rusage, false);
  Uint64 cpu_time = curr_rusage.ru_utime + curr_rusage.ru_stime;
  /**
   * Initialise when Ndb_GetRUsage isn't working,
   * when called the first time,
   * when executed time has gone backwards
   */
  if (res != 0 || m_last_recv_thread_cpu_usage_in_micros > cpu_time) {
    m_last_recv_thread_cpu_usage_in_micros = cpu_time;
    m_recv_thread_cpu_usage_in_percent = 0;
    return;
  }
  /**
   * Calculate amount of time used in executing in receive thread since last
   * time we called this routine. It is called once per second.
   * Measure CPU usage in percentage since last call to this method.
   * This will be used to calculate the boundary when we send wakeup calls
   * to the wakeup thread.
   *
   * We will report 92% when we are in the range 91.5% to 92.5%.
   * The measurement isn't exact since the get tick calls are not synchronized
   * exactly with the getRUsage calls, but it should be exact enough.
   */
  Uint64 executed_cpu_time = cpu_time - m_last_recv_thread_cpu_usage_in_micros;
  executed_cpu_time += (expired_time_in_micros / Uint64(200));
  Uint64 percentage =
      (Uint64(100) * executed_cpu_time) / expired_time_in_micros;
  m_last_recv_thread_cpu_usage_in_micros = cpu_time;
  m_recv_thread_cpu_usage_in_percent = percentage;

  Uint64 spin_cpu_time = (Uint64)theTransporterRegistry->get_total_spintime();
  theTransporterRegistry->reset_total_spintime();
  spin_cpu_time += Uint32(expired_time_in_micros / Uint64(200));
  Uint64 spin_percentage =
      (Uint64(100) * spin_cpu_time) / expired_time_in_micros;
#ifdef DEBUG_SHM_TRP
  fprintf(stderr, "recv_thread_cpu_usage: %u percent, spintime: %u percent\n",
          m_recv_thread_cpu_usage_in_percent, Uint32(spin_percentage));
#endif
  m_recv_thread_cpu_usage_in_percent -= (spin_percentage / Uint64(2));
  calc_recv_thread_wakeup();
}

void TransporterFacade::calc_recv_thread_wakeup() {
  /**
   * This function is an adaptive function that sets the number of threads
   * to wakeup from the receive thread based on the CPU usage of the receive
   * thread.
   *
   * The problem with the receive thread is that it is most efficient at
   * executing queries when it also wakes up the waiting threads.
   * The problem with this approach is that when we get close to the
   * limit of what one thread can handle the wakeup of threads consumes
   * CPU resources needed to execute signals and receive messages on the
   * socket.
   *
   * So this method decides once per second how many threads to wakeup in
   * the receive thread. The rest will be woken up by the wakeup thread
   * that is specifically only waking up threads when assigned to do so.
   *
   * Now CPU usage can go up and down quickly, thus as with every adaptive
   * algorithm there must be a degree of inertia in changing this value.
   *
   * The aim of this algorithm is to either to keep m_recv_thread_wakeup
   * at fairly high values or go towards 0. 48 and up to max of 128 more
   * or less means that almost all wakeup work is done by the receive thread.
   *
   * When we have reached below 48 and we are in the 88-89% range we will
   * consider the algorithm stable. If it drops back to 87% and lower we will
   * start increasing m_recv_thread_wakeup again.
   *
   * When we are at 48 and higher values we will consider the range 90-94%
   * to be a stable area. But if we go above 94% we will start decreasing
   * the amount of wakeups handled by the receive thread. We decrease faster
   * the higher the CPU usage is here, similarly we increase the wakeups
   * faster when the receive CPU usage is lowering.
   *
   * When we get close to 0 we will set it to 0 if we were to decrease its
   * value.
   *
   * The algorithm will in a stable algorithm stabilise around 88-94%
   * CPU usage.
   *
   * Sometimes the adaptive algorithm will
   * The general idea is to do most all wakeups from receive thread when
   * we are below 90% in load. At close to 100% load we will do no wakeups
   * instead.
   *
   * We also set some minimum numbers and maximum numbers to ensure that
   * we can adapt to quick changes in the environment as well.
   */
  NdbMutex_Lock(m_wakeup_thread_mutex);
  Uint32 factor = 4;
  Uint32 min_number = 32;
  Uint32 max_number = MAX_NUM_WAKEUPS;
  if (m_recv_thread_cpu_usage_in_percent < 80) {
    factor = 2;
    min_number = 4;
  } else if (m_recv_thread_cpu_usage_in_percent <= 83) {
    factor = 4;
    min_number = 4;
  } else if (m_recv_thread_cpu_usage_in_percent <= 85) {
    factor = 5;
    min_number = 4;
  } else if (m_recv_thread_cpu_usage_in_percent <= 87) {
    factor = 6;
    min_number = 2;
  } else if (m_recv_thread_cpu_usage_in_percent <= 89) {
    if (m_recv_thread_wakeup < 48) {
      factor = 8;
      min_number = 0;
    } else {
      factor = 7;
      min_number = 1;
    }
  } else if (m_recv_thread_cpu_usage_in_percent <= 94) {
    if (m_recv_thread_wakeup < 48) {
      factor = 9;
    } else {
      factor = 8;
    }
    min_number = 0;
  } else if (m_recv_thread_cpu_usage_in_percent <= 95) {
    factor = 12;
    min_number = 0;
  } else if (m_recv_thread_cpu_usage_in_percent <= 96) {
    factor = 16;
    min_number = 0;
  } else if (m_recv_thread_cpu_usage_in_percent <= 97) {
    factor = 20;
    min_number = 0;
  } else if (m_recv_thread_cpu_usage_in_percent <= 98) {
    factor = 24;
    min_number = 0;
  } else {
    factor = 28;
    min_number = 0;
  }
  Uint32 before = m_recv_thread_wakeup;
  m_recv_thread_wakeup = (m_recv_thread_wakeup * 8) / factor;

  if (factor != 8 && m_recv_thread_wakeup == before &&
      m_recv_thread_wakeup != 0) {
    if (factor < 8)
      m_recv_thread_wakeup++;
    else
      m_recv_thread_wakeup--;
  }
  if (factor > 8 && m_recv_thread_wakeup < 8) {
    m_recv_thread_wakeup = 0;
  }

  if (m_recv_thread_wakeup > max_number)
    m_recv_thread_wakeup = max_number;
  else if (m_recv_thread_wakeup < min_number)
    m_recv_thread_wakeup = min_number;

// #define DEBUG_THIS_FUNCTION 1
#ifdef DEBUG_THIS_FUNCTION
  fprintf(stderr, "m_recv_thread_wakeup = %u, cpu_usage = %u, factor = %u\n",
          m_recv_thread_wakeup, m_recv_thread_cpu_usage_in_percent, factor);
#endif
  NdbMutex_Unlock(m_wakeup_thread_mutex);
}

void TransporterFacade::threadMainWakeup() {
  while (theWakeupThread == nullptr) {
    /* Wait until theWakeupThread have been set */
    NdbSleep_MilliSleep(10);
  }
  NdbThread_SetThreadPrio(theWakeupThread, 9);
  NdbMutex_Lock(m_wakeup_thread_mutex);
  while (!theStopWakeup) {
    NdbCondition_WaitTimeout(m_wakeup_thread_cond, m_wakeup_thread_mutex, 100);
    wakeup_and_unlock_calls();
  }
  wakeup_and_unlock_calls();
  NdbMutex_Unlock(m_wakeup_thread_mutex);
}

bool TransporterFacade::transfer_responsibility(trp_client *const *arr,
                                                Uint32 cnt_woken, Uint32 cnt) {
  trp_client *tmp;
  /**
   * This is a deliberate data race, we will avoid grabbing
   * the mutex if the value is smaller than the wakeup count.
   * If this has changed a few microseconds ago doesn't really
   * make any huge difference, so we accept reading it
   * unprotected here.
   */
  if (cnt_woken <= m_recv_thread_wakeup) {
    return false;
  }
  NdbMutex_Lock(m_wakeup_thread_mutex);
  if (theStopWakeup) {
    NdbMutex_Unlock(m_wakeup_thread_mutex);
    return false;
  }
  Uint32 inx = m_wakeup_clients_cnt;
  bool wake_wakeup_thread = false;
  for (Uint32 i = 0; i < cnt_woken; i++) {
    tmp = arr[i];
    if (inx + i < m_recv_thread_wakeup || inx >= MAX_NO_THREADS) {
      NdbCondition_Signal(tmp->m_poll.m_condition);
    } else {
      wake_wakeup_thread = true;
      m_wakeup_clients[inx++] = tmp;
    }
    NdbMutex_Unlock(tmp->m_mutex);
  }
  m_wakeup_clients_cnt = inx;
  for (Uint32 i = cnt_woken; i < cnt; i++) {
    tmp = arr[i];
    NdbMutex_Unlock(tmp->m_mutex);
  }
  if (wake_wakeup_thread) NdbCondition_Signal(m_wakeup_thread_cond);
  NdbMutex_Unlock(m_wakeup_thread_mutex);
  return true;
}

void TransporterFacade::wakeup_and_unlock_calls() {
  trp_client *tmp;
  Uint32 count_wakeup = 0;
  while (m_wakeup_clients_cnt > 0) {
    Uint32 inx = m_wakeup_clients_cnt - 1;
    tmp = m_wakeup_clients[inx];
    m_wakeup_clients[inx] = nullptr;
    count_wakeup++;
    m_wakeup_clients_cnt = inx;
    if (count_wakeup == 4 && inx > 0) {
      count_wakeup = 0;
      NdbMutex_Unlock(m_wakeup_thread_mutex);
    }
    int ret_val = NdbMutex_Trylock(tmp->m_mutex);
    if (ret_val == 0 || ret_val == EBUSY)
      NdbCondition_Signal(tmp->m_poll.m_condition);
    if (ret_val == 0) NdbMutex_Unlock(tmp->m_mutex);
    if (count_wakeup == 0) {
      NdbMutex_Lock(m_wakeup_thread_mutex);
    }
  }
}

void TransporterFacade::remove_trp_client_from_wakeup_list(trp_client *clnt) {
  trp_client *tmp;
  Uint32 inx = 0;
  NdbMutex_Lock(m_wakeup_thread_mutex);
  for (Uint32 i = 0; i < m_wakeup_clients_cnt; i++) {
    tmp = m_wakeup_clients[i];
    if (tmp != clnt) m_wakeup_clients[inx++] = tmp;
  }
  m_wakeup_clients_cnt = inx;
  NdbMutex_Unlock(m_wakeup_thread_mutex);
}

/**
 * Signal the send thread to wake up.
 * require the m_send_thread_mutex to be held by callee.
 */
void TransporterFacade::wakeup_send_thread(void) {
  if (m_send_thread_nodes.get(SEND_THREAD_NO) == false) {
    NdbCondition_Signal(m_send_thread_cond);
  }
  m_send_thread_nodes.set(SEND_THREAD_NO);
}

/**
 * Adaptive send algorithm:
 *
 * The adaptive send algorithm takes advantage of that
 * sending a larger message has less total cost than sending
 * the same payload in multiple smaller messages. Thus it
 * tries to collect flushed send buffers from multiple
 * clients, before actually sending them all together
 *
 * We use two different mechanisms for sending buffered
 * messages:
 *
 * 1) If a 'sufficient' amount of data is available in the
 *    send buffers, we try to send immediately.
 *    ('Sufficient' == 4096 byes)
 *
 * 2) If a 'sufficient' number of the active clients has
 *    flushed their buffers since last send, we try to send.
 *    ('Sufficient' == 1/8 of active clients)
 *
 * 3) Else the send is deferred, waiting for possibly more
 *    data to arrive. To ensure that the data is eventually
 *    sent, we envoke the send thread which will send at
 *    latest after a 200us grace period.
 *
 * The above values has been determined by performance test
 * experiments. They are by intention set conservative. Such
 * that for few client threads the adaptive send is similar
 * in performance to the 'forceSend'. For larger number of
 * client threads, adaptive send always perform better, both
 * wrt. throughput and latency.
 *
 * Requiring a higher 'sufficient' value in 1) and 2) above
 * would have improved the 'high load' performance even more,
 * but at the cost of some performance regression with few clients.
 *
 * Note:
 *    Even if 'forceSend' is used, the send is done
 *    by calling try_send_buffer(), which need to grab the
 *    try_send_lock() before it can do_send_buffer(). If
 *    if fails to get the send_lock, it will leave the
 *    sending to the send thread. So even if 'forceSend'
 *    is used, it gives no guarantee of the send did complete
 *    before return - Just that it will complete as soon as
 *    possible, which is more or less the same as 'adaptive' does.
 */
void TransporterFacade::do_send_adaptive(const TrpBitmask &trps) {
  assert(m_active_trps.contains(trps));

  for (Uint32 trp = trps.find_first(); trp != TrpBitmask::NotFound;
       trp = trps.find_next(trp + 1)) {
    struct TFSendBuffer *b = &m_send_buffers[trp];
    Guard g(&b->m_mutex);

    if (b->m_flushed_cnt > 0 && b->m_current_send_buffer_size > 0) {
      /**
       * Note: We read 'm_poll_waiters' without the poll mutex, which
       *   should be OK - messages will me sent anyway, somehow,
       *   even if we see a value being slightly off.
       */
      if (b->m_current_send_buffer_size > 4 * 1024 ||  // 1)
          b->m_flushed_cnt >= m_poll_waiters / 8)      // 2)
      {
        try_send_buffer(trp, b);
      } else  // 3)
      {
        Guard g(m_send_thread_mutex);
        if (m_has_data_trps.isclear())  // Awake Send thread from 'idle sleep'
        {
          wakeup_send_thread();
        }
        m_has_data_trps.set(trp);
      }
    }
  }
}

/**
 * The send thread mainly serve two purposes:
 *
 * 1) If a regular try_send_buffer(trp) failed to grab the
 *    try_lock_send(), the 'trp' will be added to 'm_has_data_trps',
 *    and the send taken over by the send thread.
 *
 * 2) Handle deferred sends from the adaptive send algorithm,
 *    which were not sent within the 200us grace period.
 *
 * 3) In addition, we infrequently send to all connected nodes
 *    just in case...., see further comments below.
 *
 * If the send thread has been activated for either 1) or 2),
 * it only take a 'micro-nap' (200us) between each round of send.
 * This 'micro-nap' allows more data to be flushed to the buffers
 * and piggy backed on the next send.
 * (See also 'Adaptive send algorithm' comment above.)
 *
 * If the send thread is not activated due to either 1) or 2),
 * it is in an 'idle sleep' state where it only wakes up every
 * 'sendThreadWaitMillisec'.
 *
 * From both of these sleep states the send thread can be signaled
 * to wake up immediately if required. This is used if a state of
 * high send buffer usage is detected.
 */
void TransporterFacade::threadMainSend(void) {
  while (theSendThread == nullptr) {
    /* Wait until theSendThread have been set */
    NdbSleep_MilliSleep(10);
  }
  theTransporterRegistry->startSending();
  if (theTransporterRegistry->start_clients() == nullptr) {
    g_eventLogger->info(
        "Unable to start theTransporterRegistry->start_clients");
    exit(0);
  }

  raise_thread_prio(theSendThread);

  NDB_TICKS lastActivityCheck = NdbTick_getCurrentTicks();
  while (!theStopSend) {
    NdbMutex_Lock(m_send_thread_mutex);
    /**
     * Note: It is intentional that we set 'send_trps' before we
     * wait: If more 'm_has_data_trps' are added while we wait, we
     * do not want these to take effect until after the 200us
     * grace period. The exception is if we are woken up from the
     * micro-nap (!= ETIMEDOUT), which only happens if we have to
     * take immediate send action.
     */
    TrpBitmask send_trps(m_has_data_trps);

    if (m_send_thread_nodes.get(SEND_THREAD_NO) == false) {
      if (!m_has_data_trps.isclear()) {
        /**
         * Take a 200us micro-nap to allow more buffered data
         * to arrive such that larger messages can be sent.
         * Possibly also woken up earlier if buffers fills up
         * too much.
         */
        struct timespec wait_end;  // Calculate 200us wait
        NdbCondition_ComputeAbsTime_ns(&wait_end, 200 * 1000);
        int ret = NdbCondition_WaitTimeoutAbs(m_send_thread_cond,
                                              m_send_thread_mutex, &wait_end);

        /* If we were woken by a signal: take the new send_trps set */
        if (ret != ETIMEDOUT) send_trps.assign(m_has_data_trps);
      } else {
        NdbCondition_WaitTimeout(m_send_thread_cond, m_send_thread_mutex,
                                 sendThreadWaitMillisec);
      }
    }
    m_send_thread_nodes.clear(SEND_THREAD_NO);
    NdbMutex_Unlock(m_send_thread_mutex);

    /**
     * try_send on all transporters requesting assist of the send thread.
     * 'm_has_data_trps' is maintained by try_send to reflect nodes
     * still in need of more send after this try_send
     */
    try_send_all(send_trps);

    /**
     * Safeguard against messages being stuck: (probably an old bug...)
     *
     *  There seems to be cases where messages somehow are put into
     *  the send buffers without ever being registered in the set of
     *  transporters having messages to be sent. Thus we try to send
     *  to all 'active' transporters every 'sendThreadWaitMillisec'.
     */
    const NDB_TICKS now = NdbTick_getCurrentTicks();
    const Uint32 elapsed_ms =
        NdbTick_Elapsed(lastActivityCheck, now).milliSec();
    if (elapsed_ms >= sendThreadWaitMillisec) {
      lastActivityCheck = now;

      Guard g(m_send_thread_mutex);
      m_has_data_trps.bitOR(m_active_trps);
    }
  }
  theTransporterRegistry->stopSending();

  theTransporterRegistry->stop_clients();
}

extern "C" void *runReceiveResponse_C(void *me) {
  ((TransporterFacade *)me)->threadMainReceive();
  return nullptr;
}

class ReceiveThreadClient : public trp_client {
 public:
  explicit ReceiveThreadClient(TransporterFacade *facade);
  ~ReceiveThreadClient() override;
  void trp_deliver_signal(const NdbApiSignal *,
                          const LinearSectionPtr ptr[3]) override;
  enum {
    ACTIVE,      // Is the preferred receiver
    DEACTIVATE,  // Intermediate state going from ACTIVE -> SNOOZE
    SNOOZE       // Prefer any other trp_client as receiver
  } m_state;
};

ReceiveThreadClient::ReceiveThreadClient(TransporterFacade *facade)
    : m_state(SNOOZE) {
  DBUG_ENTER("ReceiveThreadClient::ReceiveThreadClient");
  m_is_receiver_thread = true;
  Uint32 ret = this->open(facade, -1);
  if (unlikely(ret == 0)) {
    g_eventLogger->info("Failed to register receive thread, ret = %d", ret);
    abort();
  }
  DBUG_VOID_RETURN;
}

ReceiveThreadClient::~ReceiveThreadClient() {
  DBUG_ENTER("ReceiveThreadClient::~ReceiveThreadClient");
  this->close();
  DBUG_VOID_RETURN;
}

void ReceiveThreadClient::trp_deliver_signal(
    const NdbApiSignal *signal, const LinearSectionPtr /*ptr*/[3]) {
  DBUG_ENTER("ReceiveThreadClient::trp_deliver_signal");
  switch (signal->theVerId_signalNumber) {
    case GSN_API_REGCONF:
    case GSN_CONNECT_REP:
    case GSN_NODE_FAILREP:
    case GSN_NF_COMPLETEREP:
    case GSN_TAKE_OVERTCCONF:
    case GSN_ALLOC_NODEID_CONF:
    case GSN_SUB_GCP_COMPLETE_REP:
      break;
    case GSN_CLOSE_COMREQ: {
      m_facade->perform_close_clnt(this);
      break;
    }
    default: {
      g_eventLogger->info(
          "Receive thread block should not receive signals, gsn: %d",
          signal->theVerId_signalNumber);
      abort();
    }
  }
  DBUG_VOID_RETURN;
}

int TransporterFacade::unset_recv_thread_cpu(Uint32 recv_thread_id) {
  if (recv_thread_id != 0) {
    return -1;
  }
  int ret;
  if ((ret = unlock_recv_thread_cpu())) {
    return ret;
  }
  recv_thread_cpu_id = NO_RECV_THREAD_CPU_ID;
  return 0;
}

int TransporterFacade::set_recv_thread_cpu(Uint16 *cpuid_array,
                                           Uint32 array_len,
                                           Uint32 recv_thread_id) {
  if (array_len > 1 || array_len == 0) {
    return -1;
  }
  if (recv_thread_id != 0) {
    return -1;
  }
  recv_thread_cpu_id = cpuid_array[0];
  if (theTransporterRegistry) {
    /* Receiver thread already started, lock cpu now */
    int ret;
    if ((ret = lock_recv_thread_cpu())) {
      return ret;
    }
  }
  return 0;
}

int TransporterFacade::set_recv_thread_activation_threshold(Uint32 threshold) {
  if (threshold >= 16) {
    threshold = 256;
  }
  min_active_clients_recv_thread = threshold;
  return 0;
}

int TransporterFacade::unlock_recv_thread_cpu() {
  if (theReceiveThread) {
    int ret_code = Ndb_UnlockCPU(theReceiveThread);
    if (ret_code) {
      g_eventLogger->info(
          "TransporterFacade (%u) : Failed to unlock thread %d, ret_code: %d",
          theOwnId, NdbThread_GetTid(theReceiveThread), ret_code);
      return ret_code;
    }
  }
  return 0;
}

int TransporterFacade::lock_recv_thread_cpu() {
  Uint32 cpu_id = recv_thread_cpu_id;
  if (cpu_id != NO_RECV_THREAD_CPU_ID && theReceiveThread) {
    int ret_code = Ndb_LockCPU(theReceiveThread, cpu_id);
    if (ret_code) {
      g_eventLogger->info(
          "TransporterFacade (%u) : Failed to lock thread %d to CPU %u, "
          "ret_code: %d",
          theOwnId, NdbThread_GetTid(theReceiveThread), cpu_id, ret_code);
      return ret_code;
    }
  }
  return 0;
}

int TransporterFacade::get_recv_thread_activation_threshold() const {
  return min_active_clients_recv_thread;
}

/**
 * At very high loads the normal OS scheduler handling isn't sufficient to
 * maintain high throughput through the NDB API. If the poll ownership isn't
 * getting sufficient attention from the OS scheduler then the performance of
 * the NDB API will suffer.
 *
 * What will happen at high load is the following. The OS scheduler tries to
 * maintain fairness between the various user threads AND the thread handling
 * the poll ownership (either a user thread or a receive thread).
 * This means that when the poll owner has executed its share of time it will
 * be descheduled to handle user threads. These threads will work on the
 * received data and will possibly also start new transactions. After all of
 * those user activities have completed, then the OS will reschedule the
 * poll owner again. At this time it will continue receiving and again start
 * up new threads. The problem is that in a machine with many CPUs available
 * this means that the CPUs won't have anything to do for a short time while
 * waiting for the rescheduled poll owner to receive data from the data nodes
 * to resume the transaction processing in the user part of the API process.
 *
 * Simply put the receiver handling of the NDB API is of such importance that
 * it must execute at a higher thread priority than normal user threads. We
 * handle this by raising thread priority of the receiver thread. If the
 * API goes into a mode where the receiver thread becomes active and we were
 * successful in raising the thread prio, then we will also retain the poll
 * ownership until the receiver thread is stopped or until the activity slows
 * down such that the receiver thread no longer needs to be active.
 *
 * On Linux raising thread prio means that we decrease the nice level of the
 * thread. This means that it will get a higher quota compared to the other
 * threads in the machine. In order to set this the binary needs CAP_NICE
 * capability or the user must have the permission to set niceness which can
 * be set using RLIMIT_NICE and ulimit -e.
 *
 * On Solaris it means setting a high fixed thread priority that will ensure
 * that it stays active unless the OS or some other higher prio threads becomes
 * executable.
 *
 * On Windows it sets the thread priority to THREAD_PRIORITY_HIGHEST.
 */
bool TransporterFacade::raise_thread_prio(NdbThread *thread) {
  int ret_code = NdbThread_SetThreadPrio(thread, 9);
  return (ret_code == 0) ? true : false;
}

static const int DEFAULT_MIN_ACTIVE_CLIENTS_RECV_THREAD = 8;
/*
  ::threadMainReceive() serves two purposes:

  1) Ensure that update_connection() is called regularly (100ms)
  2) If there are sufficient 'do_poll' activity from clients,
     it start acting as a receive thread, offloading the
     transporter polling from the clients.

  Both of these tasks need the poll rights.
  ::update_connection() has to be synced with ::performReceive(),
  and both takes place from within the 'poll-loop'.

  Updates of the connections is triggered by setting the
  flag 'm_check_connections', which will trigger a single
  ::update_connection. Either in the do_poll called from
  threadMainReceive(), if we get the poll right, or in the
  do_poll from the thread already having the poll rights.
*/
void TransporterFacade::threadMainReceive(void) {
  NDB_TICKS lastCheck = NdbTick_getCurrentTicks();
  NDB_TICKS receive_activation_time;
  init_cpu_usage(lastCheck);

  while (theReceiveThread == nullptr) {
    /* Wait until theReceiveThread have been set */
    NdbSleep_MilliSleep(10);
  }
  theTransporterRegistry->startReceiving();
  recv_client = new ReceiveThreadClient(this);
  lock_recv_thread_cpu();
  const bool raised_thread_prio = raise_thread_prio(theReceiveThread);
  while (!theStopReceive) {
    const NDB_TICKS currTime = NdbTick_getCurrentTicks();

    /**
     * Ensure that 'update_connections()' is checked every 100ms.
     * As connections has to be updated by the poll owner, we only
     * flag connections to require a check now. We will later
     * either update them ourself if we get the poll right, or leave
     * it to the thread holding the poll right, either one is fine.
     *
     * NOTE: We set this flag without mutex, which could result in
     * a 'check' to be missed now and then.
     */
    if (unlikely(NdbTick_Elapsed(lastCheck, currTime).milliSec() >= 100)) {
      m_check_connections = true;
      lastCheck = currTime;
      check_cpu_usage(currTime);
    }

    if (recv_client->m_state != ReceiveThreadClient::ACTIVE) {
      /*
         We only activate as receiver thread if
         we are sufficiently active, at least e.g. 8 threads active.
         We check this condition without mutex, there is no issue with
         what we select here, both paths will work.
      */
      if (m_num_active_clients > min_active_clients_recv_thread) {
        recv_client->m_state = ReceiveThreadClient::ACTIVE;
        m_num_active_clients = 0;
        receive_activation_time = currTime;
      } else {
        // The recv_client will 'SNOOZE' in the poll queue.
        recv_client->m_state = ReceiveThreadClient::SNOOZE;
      }
    } else {
      /**
       * We are acting as a receiver thread.
       * Check every 1000ms if activity is below the 50% threshold for
       * keeping the receiver thread still active.
       */
      if (NdbTick_Elapsed(receive_activation_time, currTime).milliSec() >
          1000) {
        /* Reset timer for next activation check time */
        receive_activation_time = currTime;
        lock_poll_mutex();
        if (m_num_active_clients < (min_active_clients_recv_thread / 2)) {
          /* Go back to not have an active receive thread */
          recv_client->m_state = ReceiveThreadClient::DEACTIVATE;
        }
        m_num_active_clients = 0; /* Reset active clients for next timeslot */
        unlock_poll_mutex();
      }
    }

    /**
     * If receiver thread is not requested to 'stay_poll_owner',
     * it might be raced by client threads grabbing the poll-right
     * in front of it. This most likely happens if the receive
     * thread is suspended during execution, and is desired behaviour.
     *
     * However, it could also happen due to another client thread
     * concurrently executing in the window where the receiver
     * thread does not hold the poll-right - That is not something
     * we want to happen.
     *
     * If we managed to raise thread prio we will stay as poll owner
     * as this means that we have a better chance of handling the
     * offered load than any other thread has.
     */
    const bool stay_poll_owner =
        recv_client->m_state == ReceiveThreadClient::ACTIVE &&
        raised_thread_prio;

    /* Don't poll for 10ms if receive thread is deactivating */
    const Uint32 max_wait =
        (recv_client->m_state == ReceiveThreadClient::DEACTIVATE) ? 0 : 10;

    recv_client->prepare_poll();
    do_poll(recv_client, max_wait, stay_poll_owner);
    recv_client->complete_poll();
  }

  if (recv_client->m_poll.m_poll_owner == true) {
    /*
      Ensure to release poll ownership before proceeding to delete the
      transporter client and thus close it. That code expects not to be
      called when being the poll owner.
    */
    recv_client->prepare_poll();
    do_poll(recv_client, 0, false);
    recv_client->complete_poll();
  }
  delete recv_client;
  theTransporterRegistry->stopReceiving();
}

/*
  This method is called by client thread or send thread that owns
  the poll "rights".
  It waits for events and if something arrives it takes care of it
  and returns to caller. It will quickly come back here if not all
  data was received for the worker thread.

  It is also responsible for doing ::update_connections status
  of transporters, as this also require the poll rights in order
  to not interfere with the polling itself.

  In order to not block awaiting 'update_connections' requests,
  we never wait longer than 10ms inside ::pollReceive().
  Longer timeouts are done in multiple 10ms periods

  NOTE: This 10ms max-wait is only a requirement for the thread
  having the poll right. Threads calling do_poll() could specify
  longer max-waits as they will either:
   - Get the poll right and end up here in ::external_poll,
     where the poll wait is done in multiple 10ms chunks.
   - Not get the poll right and put to sleep in the poll queue
     waiting to be woken up by the poller.
*/
void TransporterFacade::external_poll(Uint32 wait_time) {
  do {
    /* Long waits are done in short 10ms chunks */
    const Uint32 wait = (wait_time > 10) ? 10 : wait_time;
    const int res = theTransporterRegistry->pollReceive(wait);

    if (m_check_connections) {
      m_check_connections = false;
      theTransporterRegistry->update_connections();

      // ::reportDisconnect() may have WOKEN the poll owner
      if (m_poll_owner->m_poll.m_waiting == trp_client::PollQueue::PQ_WOKEN)
        break;
    }

    if (res > 0) {
      theTransporterRegistry->performReceive();
      break;
    }

    wait_time -= wait;
  } while (wait_time > 0);
}

TransporterFacade::TransporterFacade(GlobalDictCache *cache)
    : min_active_clients_recv_thread(DEFAULT_MIN_ACTIVE_CLIENTS_RECV_THREAD),
      recv_thread_cpu_id(NO_RECV_THREAD_CPU_ID),
      m_poll_owner_tid(),
      m_poll_owner(nullptr),
      m_poll_queue_head(nullptr),
      m_poll_queue_tail(nullptr),
      m_poll_waiters(0),
#ifdef NDB_MUTEX_DEADLOCK_DETECTOR
      m_poll_owner_region(nullptr),
#endif
      m_locked_cnt(0),
      m_locked_clients(),
      m_num_active_clients(0),
      m_check_connections(true),
      theTransporterRegistry(nullptr),
      theOwnTrpId(0),
      theOwnId(0),
      theStartNodeId(1),
      theClusterMgr(nullptr),
      dozer(nullptr),
      theStopReceive(0),
      theStopSend(0),
      theStopWakeup(0),
      sendThreadWaitMillisec(10),
      theSendThread(nullptr),
      theReceiveThread(nullptr),
      theWakeupThread(nullptr),
      m_last_recv_thread_cpu_usage_in_micros(0),
      m_recv_thread_cpu_usage_in_percent(0),
      m_recv_thread_wakeup(MAX_NO_THREADS),
      m_wakeup_clients_cnt(0),
      m_wakeup_thread_mutex(nullptr),
      m_wakeup_thread_cond(nullptr),
      recv_client(nullptr),
      m_enabled_trps_mask(),
      m_fragmented_signal_id(0),
      m_open_close_mutex(nullptr),
      thePollMutex(nullptr),
      m_globalDictCache(cache),
      m_send_buffer("sendbufferpool"),
      m_active_trps(),
      m_send_thread_mutex(nullptr),
      m_send_thread_cond(nullptr),
      m_send_thread_nodes(),
      m_has_data_trps(),
      m_tls_search_path(NDB_TLS_SEARCH_PATH),
      m_tls_node_type(NODE_TYPE_API) {
  DBUG_ENTER("TransporterFacade::TransporterFacade");
  thePollMutex = NdbMutex_CreateWithName("PollMutex");
  sendPerformedLastInterval = 0;
  m_open_close_mutex = NdbMutex_Create();
  for (Uint32 i = 0; i < NDB_ARRAY_SIZE(m_send_buffers); i++) {
    char name_buf[32];
    BaseString::snprintf(name_buf, sizeof(name_buf), "sendbuffer:%u", i);
    NdbMutex_InitWithName(&m_send_buffers[i].m_mutex, name_buf);
  }

  m_send_thread_cond = NdbCondition_Create();
  m_send_thread_mutex = NdbMutex_CreateWithName("SendThreadMutex");

  m_wakeup_thread_cond = NdbCondition_Create();
  m_wakeup_thread_mutex = NdbMutex_CreateWithName("WakeupThreadMutex");

  for (int i = 0; i < NO_API_FIXED_BLOCKS; i++) m_fixed2dynamic[i] = RNIL;

#ifdef API_TRACE
  apiSignalLog = nullptr;
#endif

#ifdef NDB_MUTEX_DEADLOCK_DETECTOR
  m_poll_owner_region = NdbMutex_CreateSerializedRegion();
#endif

  theClusterMgr = new ClusterMgr(*this);
  DBUG_VOID_RETURN;
}

/* Return true if node with "nodeId" is a MGM node */
static bool is_mgmd(NodeId nodeId, const ndb_mgm_configuration *conf) {
  ndb_mgm_configuration_iterator iter(conf, CFG_SECTION_NODE);
  if (iter.find(CFG_NODE_ID, nodeId)) abort();
  Uint32 type;
  if (iter.get(CFG_TYPE_OF_SECTION, &type)) abort();

  return (type == NODE_TYPE_MGM);
}

bool TransporterFacade::do_connect_mgm(NodeId nodeId,
                                       const ndb_mgm_configuration *conf) {
  // Allow other MGM nodes to connect
  DBUG_ENTER("TransporterFacade::do_connect_mgm");
  ndb_mgm_configuration_iterator iter(conf, CFG_SECTION_CONNECTION);
  for (iter.first(); iter.valid(); iter.next()) {
    Uint32 nodeId1, nodeId2;
    if (iter.get(CFG_CONNECTION_NODE_1, &nodeId1) ||
        iter.get(CFG_CONNECTION_NODE_2, &nodeId2))
      DBUG_RETURN(false);

    // Skip connections where this node is not involved
    if (nodeId1 != nodeId && nodeId2 != nodeId) continue;

    // If both sides are MGM, open connection
    if (is_mgmd(nodeId1, conf) && is_mgmd(nodeId2, conf)) {
      Uint32 remoteNodeId = (nodeId == nodeId1 ? nodeId2 : nodeId1);
      DBUG_PRINT("info", ("opening connection to node %d", remoteNodeId));
      startConnecting(remoteNodeId);
    }
  }

  DBUG_RETURN(true);
}

void TransporterFacade::set_up_node_active_in_send_buffers(
    NodeId nodeId, const ndb_mgm_configuration *conf) {
  DBUG_ENTER("TransporterFacade::set_up_node_active_in_send_buffers");
  ndb_mgm_configuration_iterator iter(conf, CFG_SECTION_CONNECTION);
  Uint32 nodeId1, nodeId2, remoteNodeId;
  struct TFSendBuffer *b;
  TrpId trp_ids[MAX_NODE_GROUP_TRANSPORTERS];
  Uint32 num_ids;

  {
    theTransporterRegistry->get_trps_for_node(theOwnId, trp_ids, num_ids,
                                              MAX_NODE_GROUP_TRANSPORTERS);
    assert(num_ids == 1);
    assert(trp_ids[0] > 0);
    theOwnTrpId = trp_ids[0];
  }

  /* Need to also communicate with myself, not found in config */
  b = m_send_buffers + theOwnTrpId;
  b->m_node_active = true;
  m_active_trps.set(theOwnTrpId);

  for (iter.first(); iter.valid(); iter.next()) {
    if (iter.get(CFG_CONNECTION_NODE_1, &nodeId1)) continue;
    if (iter.get(CFG_CONNECTION_NODE_2, &nodeId2)) continue;
    if (nodeId1 != nodeId && nodeId2 != nodeId) continue;
    remoteNodeId = (nodeId == nodeId1 ? nodeId2 : nodeId1);
    theTransporterRegistry->get_trps_for_node(remoteNodeId, trp_ids, num_ids,
                                              MAX_NODE_GROUP_TRANSPORTERS);
    assert(num_ids == 1);
    b = m_send_buffers + trp_ids[0];
    b->m_node_active = true;
    m_active_trps.set(trp_ids[0]);
  }
  DBUG_VOID_RETURN;
}

void TransporterFacade::configure_tls(const char *searchPath, int nodeType,
                                      int mgmTlsRequirement) {
  assert(searchPath);
  m_tls_search_path = searchPath;
  m_tls_node_type = nodeType;
  m_mgm_tls_level = mgmTlsRequirement;
}

const char *TransporterFacade::get_tls_certificate_path() const {
  return theTransporterRegistry->getTlsKeyManager()->cert_path();
}

bool TransporterFacade::configure(NodeId nodeId,
                                  const ndb_mgm_configuration *conf) {
  DBUG_ENTER("TransporterFacade::configure");

  assert(theOwnId == nodeId);
  assert(theTransporterRegistry);
  assert(theClusterMgr);

  // Configure transporters
  if (!IPCConfig::configureTransporters(theOwnId, conf, *theTransporterRegistry,
                                        true))
    DBUG_RETURN(false);

  /* Set up active communication with all configured nodes / transporters */
  set_up_node_active_in_send_buffers(theOwnId, conf);

  // Configure cluster manager
  theClusterMgr->configure(nodeId, conf);

  ndb_mgm_configuration_iterator iter(conf, CFG_SECTION_NODE);
  if (iter.find(CFG_NODE_ID, nodeId)) DBUG_RETURN(false);

  // Configure send buffers
  if (!m_send_buffer.inited()) {
    Uint32 total_send_buffer = 0;
    Uint64 total_send_buffer64;
    size_t total_send_buffer_size_t;
    size_t reserved_send_buffer_size_t;
    iter.get(CFG_TOTAL_SEND_BUFFER_MEMORY, &total_send_buffer);

    total_send_buffer64 = total_send_buffer;
    if (total_send_buffer64 == 0) {
      total_send_buffer64 = theTransporterRegistry->get_total_max_send_buffer();
    }

    Uint64 extra_send_buffer = 0;
    iter.get(CFG_EXTRA_SEND_BUFFER_MEMORY, &extra_send_buffer);

    total_send_buffer64 += extra_send_buffer;

    /**
     * Reserved area for send-to-self
     * We will grab 16 pages, (@32kB/page : 512kB)
     * Gives space for at least 16 signals @ 1/page.
     * If signals are better packed then less pages
     * are needed.
     */
    const Uint32 pagesize = m_send_buffer.get_page_size();
    const Uint64 reserved_send_buffer = 16 * pagesize;

    total_send_buffer64 += reserved_send_buffer;

#if SIZEOF_CHARP == 4
    /* init method can only handle 32-bit sizes on 32-bit platforms */
    if (total_send_buffer64 > 0xFFFFFFFF) {
      total_send_buffer64 = 0xFFFFFFFF;
    }
#endif
    total_send_buffer_size_t = (size_t)total_send_buffer64;
    reserved_send_buffer_size_t = (size_t)reserved_send_buffer;
    if (!m_send_buffer.init(total_send_buffer_size_t,
                            reserved_send_buffer_size_t)) {
      g_eventLogger->info(
          "TransporterFacade : Unable to allocate %zu bytes of memory for send "
          "buffers!!",
          total_send_buffer_size_t);
      DBUG_RETURN(false);
    }
  }

  Uint32 auto_reconnect = 1;
  iter.get(CFG_AUTO_RECONNECT, &auto_reconnect);

  const char *priospec = nullptr;
  if (iter.get(CFG_HB_THREAD_PRIO, &priospec) == 0) {
    NdbThread_SetHighPrioProperties(priospec);
  }

  /**
   * Keep value it set before connect (overriding config)
   */
  if (theClusterMgr->m_auto_reconnect == -1) {
    theClusterMgr->m_auto_reconnect = auto_reconnect;
  }

#ifdef API_TRACE
  signalLogger.logOn(true, 0, SignalLoggerManager::LogInOut);
#endif

#ifdef ERROR_INSERT
  Uint32 mixologyLevel = 0;

  iter.get(CFG_MIXOLOGY_LEVEL, &mixologyLevel);
  if (mixologyLevel) {
    g_eventLogger->info("TransporterFacade Mixology level set to 0x%x",
                        mixologyLevel);
    theTransporterRegistry->setMixologyLevel(mixologyLevel);
  }
#endif

  // Open connection between MGM servers
  if (!do_connect_mgm(nodeId, conf)) DBUG_RETURN(false);

  /**
   * Also setup Loopback Transporter
   */
  startConnecting(nodeId);

  DBUG_RETURN(true);
}

void TransporterFacade::for_each(trp_client *sender,
                                 const NdbApiSignal *aSignal,
                                 const LinearSectionPtr ptr[3]) {
  /**
   * ::for_each() is required to be called by the thread being
   * the poll owner while it trp_deliver_signal() to
   * a client. That client may then use ::for_each() to further
   * distribute a signal to 'each' of the clients known by this
   * TransporterFacade.
   */
  assert(is_poll_owner_thread());

  /*
    Allow up to 16 threads to receive signals here before we start
    waking them up.
  */
  trp_client *woken[16];
  Uint32 cnt_woken = 0;
  Uint32 sz = m_threads.m_clients.size();
  for (Uint32 i = 0; i < sz; i++) {
    trp_client *clnt = m_threads.m_clients[i].m_clnt;
    if (clnt != nullptr && clnt != sender && !clnt->is_receiver_thread()) {
      /**
       * We skip sending signal to receive thread. The receive thread
       * have no interest in signals sent as for_each.
       */
      const bool client_locked = clnt->is_locked_for_poll();
      assert(clnt->check_if_locked() == client_locked);
      if (client_locked) {
        clnt->trp_deliver_signal(aSignal, ptr);
      } else {
        NdbMutex_Lock(clnt->m_mutex);
        int save = clnt->m_poll.m_waiting;
        clnt->trp_deliver_signal(aSignal, ptr);
        if (save != clnt->m_poll.m_waiting &&
            clnt->m_poll.m_waiting == trp_client::PollQueue::PQ_WOKEN) {
          woken[cnt_woken++] = clnt;
          if (cnt_woken == NDB_ARRAY_SIZE(woken)) {
            lock_poll_mutex();
            remove_from_poll_queue(woken, cnt_woken);
            unlock_poll_mutex();
            unlock_and_signal(woken, cnt_woken);
            cnt_woken = 0;
          }
        } else {
          NdbMutex_Unlock(clnt->m_mutex);
        }
      }
    }
  }

  if (cnt_woken != 0) {
    lock_poll_mutex();
    remove_from_poll_queue(woken, cnt_woken);
    unlock_poll_mutex();
    unlock_and_signal(woken, cnt_woken);
  }
}

void TransporterFacade::connected() {
  DBUG_ENTER("TransporterFacade::connected");
  NdbApiSignal signal(numberToRef(API_CLUSTERMGR, theOwnId));
  signal.theVerId_signalNumber = GSN_ALLOC_NODEID_CONF;
  signal.theReceiversBlockNumber = 0;
  signal.theTrace = 0;
  signal.theLength = AllocNodeIdConf::SignalLength;

  AllocNodeIdConf *rep = CAST_PTR(AllocNodeIdConf, signal.getDataPtrSend());
  rep->senderRef = 0;
  rep->senderData = 0;
  rep->nodeId = theOwnId;
  rep->secret_lo = 0;
  rep->secret_hi = 0;

  Uint32 sz = m_threads.m_clients.size();
  for (Uint32 i = 0; i < sz; i++) {
    trp_client *clnt = m_threads.m_clients[i].m_clnt;
    if (clnt != nullptr && !clnt->is_receiver_thread()) {
      /**
       * The receiver thread have no interest in the
       * ALLOC_NODEID_CONF signal, so will skip sending
       * it to receive thread client.
       */
      NdbMutex_Lock(clnt->m_mutex);
      clnt->trp_deliver_signal(&signal, nullptr);
      NdbMutex_Unlock(clnt->m_mutex);
    }
  }
  DBUG_VOID_RETURN;
}

/**
 * perform_close_clnt()
 *
 * Invoked from close_clnt via sending a CLOSE_COMREQ signal.
 * Reason is that the poll-right is needed to guard against
 * that clients are taken out of the m_threads[] array while
 * being looked up by trp_deliver_signal, or iterated by e.g.
 * for_each(), or enable_ / disable_send_buffer().
 */
void TransporterFacade::perform_close_clnt(trp_client *clnt) {
  assert(is_poll_owner_thread());
  Guard g(m_open_close_mutex);
  m_threads.close(clnt->m_blockNo);
  dbg("perform_close_clnt: poll_owner: %p", m_poll_owner);
  dbg("perform_close_clnt: clnt: %p", clnt);
  clnt->wakeup();
}

int TransporterFacade::close_clnt(trp_client *clnt) {
  bool first = true;
  NdbApiSignal signal(numberToRef(clnt->m_blockNo, theOwnId));
  signal.theVerId_signalNumber = GSN_CLOSE_COMREQ;
  signal.theTrace = 0;
  signal.theLength = 1;
  CloseComReqConf *req = CAST_PTR(CloseComReqConf, signal.getDataPtrSend());
  req->xxxBlockRef = numberToRef(clnt->m_blockNo, theOwnId);

  if (clnt) {
    NdbMutex_Lock(m_open_close_mutex);
    signal.theReceiversBlockNumber = clnt->m_blockNo;
    signal.theData[0] = clnt->m_blockNo;
    dbg("close(%p)", clnt);
    if (m_threads.get(clnt->m_blockNo) != clnt) {
      abort();
    }

    /**
     * We close a client through the following procedure
     * 1) Ensure we close something which is open
     * 2) Send a signal to ourselves, this signal will be executed
     *    by the poll owner. When this signal is executed we're
     *    in the correct thread to write NULL into the mapping
     *    array.
     * 3) When this thread receives the signal sent to ourselves it
     *    it will call close (perform_close_clnt) on the client
     *    mapping.
     * 4) We will wait on a condition in this thread for the poll
     *    owner to set this entry to NULL.
     */
    if (!theTransporterRegistry) {
      /*
        We haven't even setup transporter registry, so no need to
        send signal to poll waiter to close.
      */
      m_threads.close(clnt->m_blockNo);
      NdbMutex_Unlock(m_open_close_mutex);
      return 0;
    }
    bool not_finished;
    do {
      /**
       * Obey lock order of trp_client::m_mutex vs. open_close_mutex:
       * deliver_signal(CLOSE_COMREQ) will lock the client, then
       * perform_close_clnt() which takes the m_open_close_mutex.
       * That would deadlock if we didn't release open_close_mutex now.
       */
      NdbMutex_Unlock(m_open_close_mutex);

      clnt->prepare_poll();
      if (first) {
        clnt->raw_sendSignal(&signal, theOwnId);
        clnt->do_forceSend(1);
        first = false;
      }

      // perform_close_clnt() will 'wakeup()', so wait how long it takes
      clnt->do_poll(3000);

      NdbMutex_Lock(m_open_close_mutex);
      not_finished = (m_threads.get(clnt->m_blockNo) == clnt);
      clnt->complete_poll();
    } while (not_finished);
    NdbMutex_Unlock(m_open_close_mutex);
  }
  remove_trp_client_from_wakeup_list(clnt);
  return 0;
}

/**
 * expand_clnt()
 *
 * Invoked from open_clnt() with the EXPAND_CLNT signal iff
 * the m_threads[] array has to be expanded (and relocated).
 * The poll-right is needed to guard against relocation of
 * m_threads[] array while being accessed by trp_deliver_signal,
 * or iterated by e.g. for_each(), or enable_ / disable_send_buffer().
 */
void TransporterFacade::expand_clnt()  // Handle EXPAND_CLNT signal
{
  assert(is_poll_owner_thread());
  Guard g(m_open_close_mutex);
  m_threads.expand(64);
}

Uint32 TransporterFacade::open_clnt(trp_client *clnt, int blockNo) {
  DBUG_ENTER("TransporterFacade::open");
  dbg("open(%p)", clnt);

  /**
   * Need 'm_open_close_mutex' as m_threads[] will be updated.
   */
  NdbMutex_Lock(m_open_close_mutex);

  while (unlikely(m_threads.freeCnt() == 0)) {
    // First ::open_clnt seeing 'freeCnt() == 0' will expand
    const bool do_expand = !m_threads.m_expanding;
    m_threads.m_expanding = true;

    /**
     * Obey lock order of trp_client::m_mutex vs. open_close_mutex:
     * deliver_signal(EXPAND_CLNT) will lock the client, then call
     * expand_clnt() which takes the m_open_close_mutex.
     * That would deadlock if we didn't release open_close_mutex now.
     */
    NdbMutex_Unlock(m_open_close_mutex);

    /**
     * Ask ClusterMgr to do m_thread.expand() (Need poll rights)
     * There is no wakeup() of the client(s) waiting for expand,
     * do_poll waits in short 10ms naps before checking if expand
     * completed. Only the client requesting the expand do_poll,
     * the other simply sleeps.
     */
    if (do_expand) {
      if (unlikely(theOwnTrpId == 0)) {
        assert(!clnt->isSendEnabled(theOwnTrpId));
        DBUG_RETURN(0);
      }

      NdbApiSignal signal(numberToRef(0, theOwnId));
      signal.theVerId_signalNumber = GSN_EXPAND_CLNT;
      signal.theTrace = 0;
      signal.theLength = 1;
      signal.theReceiversBlockNumber = theClusterMgr->m_blockNo;
      signal.theData[0] = 0;  // Unused

      clnt->prepare_poll();
      // This client should be allowed to send to itself
      assert(clnt->isSendEnabled(theOwnTrpId));

      const int res = clnt->raw_sendSignal(&signal, theOwnId);
      if (res != 0) {
        // 'open' failed if expand request could not be sent.
        clnt->complete_poll();
        DBUG_RETURN(0);
      }
      clnt->do_forceSend(1);
      clnt->do_poll(10);
      clnt->complete_poll();
    } else {
      NdbSleep_MilliSleep(10);
    }
    NdbMutex_Lock(m_open_close_mutex);
  }
  const int r = m_threads.open(clnt);
  NdbMutex_Unlock(m_open_close_mutex);
  if (r < 0) {
    DBUG_RETURN(0);
  }

  /**
   * A successful m_threads.open() above also included this client in
   * the list of clients receiving enable_send()/disable_send() callbacks
   * as we (dis)connects to other nodes. First we have to set the initial
   * known set of enabled transporters:
   *
   * As the lock order requires client lock to be taken before
   * open_close_mutex, we have to release it above, before relocking
   * below in correct order. This create a possible race in between
   * here, where a Transporter (dis)connect may enable/disable
   * a send buffer for the client now being in m_client[], without
   * its enabled_nodes_mask yet being set. This should not really
   * matter, as we 'set' the updated enabled mask to the latest
   * value below anyway, overwriting what any races did in between here.
   * The same race could also result in enable/disable notifications
   * arriving after set_enabled_send(), appearing as duplicates which
   * should be ignored.
   *
   * (Also see disable_/enable_send_buffer comments)
   */
  clnt->lock();
  NdbMutex_Lock(m_open_close_mutex);
  clnt->set_enabled_send(m_enabled_trps_mask);
  NdbMutex_Unlock(m_open_close_mutex);
  clnt->unlock();

  if (unlikely(blockNo != -1)) {
    // Using fixed block number, add fixed->dynamic mapping
    Uint32 fixed_index = blockNo - MIN_API_FIXED_BLOCK_NO;

    assert(blockNo >= MIN_API_FIXED_BLOCK_NO &&
           fixed_index <= NO_API_FIXED_BLOCKS);

    m_fixed2dynamic[fixed_index] = r;
  }

  DBUG_RETURN(numberToRef(r, theOwnId));
}

TransporterFacade::~TransporterFacade() {
  DBUG_ENTER("TransporterFacade::~TransporterFacade");

  delete theClusterMgr;
  NdbMutex_Lock(thePollMutex);
  delete theTransporterRegistry;
  NdbMutex_Unlock(thePollMutex);
  for (Uint32 i = 0; i < NDB_ARRAY_SIZE(m_send_buffers); i++) {
    NdbMutex_Deinit(&m_send_buffers[i].m_mutex);
  }
  NdbMutex_Destroy(thePollMutex);
  NdbMutex_Destroy(m_open_close_mutex);
  NdbMutex_Destroy(m_send_thread_mutex);
  NdbCondition_Destroy(m_send_thread_cond);
  NdbMutex_Destroy(m_wakeup_thread_mutex);
  NdbCondition_Destroy(m_wakeup_thread_cond);
#ifdef API_TRACE
  signalLogger.setOutputStream(nullptr);
#endif
#ifdef NDB_MUTEX_DEADLOCK_DETECTOR
  NdbMutex_DestroySerializedRegion(m_poll_owner_region);
#endif
  DBUG_VOID_RETURN;
}

/**
 * ::is_poll_owner_thread() is used in assert's to verify and
 * enforce correct usage of raw_sendSignal vs. safe_sendSignal():
 *
 * - (raw_)sendSignal() is the 'normal' way to send signals from
 *   a client. The sent signals are aggregated in client-local
 *   send buffers which are flushed to the global Transporter
 *   buffers before the client enter poll-wait (do_poll()).
 *   -- prepare_poll() asserts 'has_unflushed_sends() == false'
 *      to catch any violations of this.
 *
 * - safe_sendSignal():
 *   If signals are sent by a client while receiving (processing
 *   trp_deliver_signal()), we can't buffer the signals in this
 *   clients send buffers: There are no guarantee when this client
 *   will wake up and eventually flush its local send buffers.
 *   Instead such signals should be sent with safe_sendSignal().
 *   Such signals will be buffered in the *poll_owners* send buffers,
 *   and later (rather soon) be flushed when the poll owner
 *   finish_poll().
 *   -- This require TransporterFacade::m_poll_owner to be accessed.
 *      Thus, safe_sendSignal() should only be called by the
 *      'poll_owner_thread' (Asserted in safe_sendSignal()).
 *   -- The 'poll_owner_thread' must not use (raw_)sendSignal()
 *      to send from any other clients than itself:
 *      There are no guarantee when such sends would have been
 *      flushed, and the signal eventually sent.
 *      (Asserted in ::sendSignal()++)
 */
bool TransporterFacade::is_poll_owner_thread() const {
  Guard g(thePollMutex);
  return m_poll_owner != nullptr &&
         my_thread_equal(my_thread_self(), m_poll_owner_tid);
}

/******************************************************************************
 * SEND SIGNAL METHODS
 *****************************************************************************/
int TransporterFacade::sendSignal(trp_client *clnt, const NdbApiSignal *aSignal,
                                  NodeId aNode) {
  assert(clnt == m_poll_owner || is_poll_owner_thread() == false);

  const Uint32 *tDataPtr = aSignal->getConstDataPtrSend();
  Uint32 Tlen = aSignal->theLength;
  Uint32 TBno = aSignal->theReceiversBlockNumber;
#ifdef API_TRACE
  if (setSignalLog() && TRACE_GSN(aSignal->theVerId_signalNumber)) {
    SignalHeader tmp = *aSignal;
    tmp.theSendersBlockRef = numberToRef(aSignal->theSendersBlockRef, theOwnId);
    LinearSectionPtr ptr[3];
    signalLogger.sendSignal(tmp, 1, tDataPtr, aNode, ptr, 0);
    signalLogger.flushSignalLog();
  }
#endif
  if ((Tlen != 0) && (Tlen <= 25) && (TBno != 0)) {
    TrpId trp_id = 0;
    SendStatus ss = theTransporterRegistry->prepareSend(
        clnt, aSignal,
        1,  // JBB
        tDataPtr, aNode, trp_id, (LinearSectionPtr *)nullptr);
    // if (ss != SEND_OK) ndbout << ss << endl;
    if (ss == SEND_OK) {
      assert(theClusterMgr->getNodeInfo(aNode).is_confirmed() ||
             aSignal->readSignalNumber() == GSN_API_REGREQ ||
             (aSignal->readSignalNumber() == GSN_CLOSE_COMREQ &&
              aNode == ownId()));
    }
    return (ss == SEND_OK ? 0 : -1);
  } else {
    g_eventLogger->info("ERR: SigLen = %u BlockRec = %u SignalNo = %d", Tlen,
                        TBno, aSignal->theVerId_signalNumber);
    assert(0);
  }           // if
  return -1;  // Node Dead
}

/* Max fragmented signal chunk size (words) is max round number
 * of NDB_SECTION_SEGMENT_SZ words with some slack left for 'main'
 * part of signal etc.
 */
#define CHUNK_SZ                                                       \
  ((((MAX_SEND_MESSAGE_BYTESIZE >> 2) / NDB_SECTION_SEGMENT_SZ) - 2) * \
   NDB_SECTION_SEGMENT_SZ)

/**
 * sendFragmentedSignal (GenericSectionPtr variant)
 * ------------------------------------------------
 * This method will send a signal with attached long sections.  If
 * the signal is longer than CHUNK_SZ, the signal will be split into
 * multiple CHUNK_SZ fragments.
 *
 * This is done by sending two or more long signals(fragments), with the
 * original GSN, but different signal data and with as much of the long
 * sections as will fit in each.
 *
 * Non-final fragment signals contain a fraginfo value in the header
 * (1= first fragment, 2= intermediate fragment, 3= final fragment)
 *
 * Fragment signals contain additional words in their signals :
 *   1..n words Mapping section numbers in fragment signal to original
 *              signal section numbers
 *   1 word     Fragmented signal unique id.
 *
 * Non final fragments (fraginfo=1/2) only have this data in them.  Final
 * fragments have this data in addition to the normal signal data.
 *
 * Each fragment signal can transport one or more long sections, starting
 * with section 0.  Sections are always split on NDB_SECTION_SEGMENT_SZ word
 * boundaries to simplify reassembly in the kernel.
 */
int TransporterFacade::sendFragmentedSignal(trp_client *clnt,
                                            const NdbApiSignal *inputSignal,
                                            NodeId aNode,
                                            const GenericSectionPtr ptr[3],
                                            Uint32 secs) {
  assert(clnt == m_poll_owner || is_poll_owner_thread() == false);

  NdbApiSignal copySignal(*inputSignal);
  NdbApiSignal *aSignal = &copySignal;

  unsigned i;
  Uint32 totalSectionLength = 0;
  for (i = 0; i < secs; i++) totalSectionLength += ptr[i].sz;

  /* If there's no need to fragment, send normally */
  if (totalSectionLength <= CHUNK_SZ)
    return sendSignal(clnt, aSignal, aNode, ptr, secs);

    // TODO : Consider tracing fragment signals?
#ifdef API_TRACE
  if (setSignalLog() && TRACE_GSN(aSignal->theVerId_signalNumber)) {
    SignalHeader tmp = *aSignal;
    tmp.theSendersBlockRef = numberToRef(aSignal->theSendersBlockRef, theOwnId);
    signalLogger.sendSignal(tmp, 1, aSignal->getConstDataPtrSend(), aNode, ptr,
                            0);
    signalLogger.flushSignalLog();
    for (Uint32 i = 0; i < secs; i++) ptr[i].sectionIter->reset();
  }
#endif

  NdbApiSignal tmp_signal(*(SignalHeader *)aSignal);
  GenericSectionPtr tmp_ptr[3];
  GenericSectionPtr empty = {0, nullptr};
  Uint32 unique_id = m_fragmented_signal_id++;  // next unique id

  /* Init tmp_ptr array from ptr[] array, make sure we have
   * 0 length for missing sections
   */
  for (i = 0; i < 3; i++) tmp_ptr[i] = (i < secs) ? ptr[i] : empty;

  /* Create our section iterator adapters */
  FragmentedSectionIterator sec0(tmp_ptr[0]);
  FragmentedSectionIterator sec1(tmp_ptr[1]);
  FragmentedSectionIterator sec2(tmp_ptr[2]);

  /* Replace caller's iterators with ours */
  tmp_ptr[0].sectionIter = &sec0;
  tmp_ptr[1].sectionIter = &sec1;
  tmp_ptr[2].sectionIter = &sec2;

  unsigned start_i = 0;
  unsigned this_chunk_sz = 0;
  unsigned fragment_info = 0;
  Uint32 *tmp_signal_data = tmp_signal.getDataPtrSend();
  for (i = 0; i < secs;) {
    unsigned remaining_sec_sz = tmp_ptr[i].sz;
    tmp_signal_data[i - start_i] = i;
    if (this_chunk_sz + remaining_sec_sz <= CHUNK_SZ) {
      /* This section fits whole, move onto next */
      this_chunk_sz += remaining_sec_sz;
      i++;
      continue;
    } else {
      assert(this_chunk_sz <= CHUNK_SZ);
      /* This section doesn't fit, truncate it */
      unsigned send_sz = CHUNK_SZ - this_chunk_sz;
      if (i != start_i) {
        /* We ensure that the first piece of a new section which is
         * being truncated is a multiple of NDB_SECTION_SEGMENT_SZ
         * (to simplify reassembly).  Subsequent non-truncated pieces
         * will be CHUNK_SZ which is a multiple of NDB_SECTION_SEGMENT_SZ
         * The final piece does not need to be a multiple of
         * NDB_SECTION_SEGMENT_SZ
         *
         * We round down the available send space to the nearest whole
         * number of segments.
         * If there's not enough space for one segment, then we round up
         * to one segment.  This can make us send more than CHUNK_SZ, which
         * is ok as it's defined as less than the maximum message length.
         */
        send_sz = (send_sz / NDB_SECTION_SEGMENT_SZ) *
                  NDB_SECTION_SEGMENT_SZ;               /* Round down */
        send_sz = MAX(send_sz, NDB_SECTION_SEGMENT_SZ); /* At least one */
        send_sz = MIN(send_sz, remaining_sec_sz);       /* Only actual data */

        /* If we've squeezed the last bit of data in, jump out of
         * here to send the last fragment.
         * Otherwise, send what we've collected so far.
         */
        if ((send_sz == remaining_sec_sz) && /* All sent */
            (i == secs - 1))                 /* No more sections */
        {
          this_chunk_sz += remaining_sec_sz;
          i++;
          continue;
        }
      }

      /* At this point, there must be data to send in a further signal */
      assert((send_sz < remaining_sec_sz) || (i < secs - 1));

      /* Modify tmp generic section ptr to describe truncated
       * section
       */
      tmp_ptr[i].sz = send_sz;
      FragmentedSectionIterator *fragIter =
          (FragmentedSectionIterator *)tmp_ptr[i].sectionIter;
      const Uint32 total_sec_sz = ptr[i].sz;
      const Uint32 start = (total_sec_sz - remaining_sec_sz);
      bool ok = fragIter->setRange(start, send_sz);
      assert(ok);
      if (!ok) return -1;

      if (fragment_info < 2)  // 1 = first fragment signal
                              // 2 = middle fragments
        fragment_info++;

      // send tmp_signal
      tmp_signal_data[i - start_i + 1] = unique_id;
      tmp_signal.setLength(i - start_i + 2);
      tmp_signal.m_fragmentInfo = fragment_info;
      tmp_signal.m_noOfSections = i - start_i + 1;
      // do prepare send
      {
        TrpId trp_id = 0;
        SendStatus ss = theTransporterRegistry->prepareSend(
            clnt, &tmp_signal, 1, /*JBB*/
            tmp_signal_data, aNode, trp_id, &tmp_ptr[start_i]);
        if (likely(ss == SEND_OK)) {
          assert(theClusterMgr->getNodeInfo(aNode).is_confirmed() ||
                 tmp_signal.readSignalNumber() == GSN_API_REGREQ);
        } else {
          if (unlikely(ss == SEND_MESSAGE_TOO_BIG)) {
            handle_message_too_big(aNode, aSignal, &tmp_ptr[start_i], __LINE__);
          }
          return -1;
        }
      }
      assert(remaining_sec_sz >= send_sz);
      Uint32 remaining = remaining_sec_sz - send_sz;
      tmp_ptr[i].sz = remaining;
      /* Set sub-range iterator to cover remaining words */
      ok = fragIter->setRange(start + send_sz, remaining);
      assert(ok);
      if (!ok) return -1;

      if (remaining == 0) /* This section's done, move onto the next */
        i++;

      // setup variables for next signal
      start_i = i;
      this_chunk_sz = 0;
    }
  }

  unsigned a_sz = aSignal->getLength();

  if (fragment_info > 0) {
    // update the original signal to include section info
    Uint32 *a_data = aSignal->getDataPtrSend();
    unsigned tmp_sz = i - start_i;
    memcpy(a_data + a_sz, tmp_signal_data, tmp_sz * sizeof(Uint32));
    a_data[a_sz + tmp_sz] = unique_id;
    aSignal->setLength(a_sz + tmp_sz + 1);

    // send last fragment
    aSignal->m_fragmentInfo = 3;  // 3 = last fragment
    aSignal->m_noOfSections = i - start_i;
  } else {
    aSignal->m_noOfSections = secs;
  }

  // send aSignal
  int ret;
  {
    TrpId trp_id = 0;
    SendStatus ss = theTransporterRegistry->prepareSend(
        clnt, aSignal, 1 /*JBB*/, aSignal->getConstDataPtrSend(), aNode, trp_id,
        &tmp_ptr[start_i]);
    if (likely(ss == SEND_OK)) {
      assert(theClusterMgr->getNodeInfo(aNode).is_confirmed() ||
             aSignal->readSignalNumber() == GSN_API_REGREQ);
      ret = 0;
    } else {
      if (unlikely(ss == SEND_MESSAGE_TOO_BIG)) {
        handle_message_too_big(aNode, aSignal, &tmp_ptr[start_i], __LINE__);
      }
      ret = -1;
    }
  }
  aSignal->m_noOfSections = 0;
  aSignal->m_fragmentInfo = 0;
  aSignal->setLength(a_sz);
  return ret;
}

int TransporterFacade::sendFragmentedSignal(trp_client *clnt,
                                            const NdbApiSignal *aSignal,
                                            NodeId aNode,
                                            const LinearSectionPtr ptr[3],
                                            Uint32 secs) {
  assert(clnt == m_poll_owner || is_poll_owner_thread() == false);

  /* Use the GenericSection variant of sendFragmentedSignal */
  GenericSectionPtr tmpPtr[3];
  LinearSectionPtr linCopy[3];
  const LinearSectionPtr empty = {0, nullptr};

  /* Make sure all of linCopy is initialised */
  for (Uint32 j = 0; j < 3; j++) linCopy[j] = (j < secs) ? ptr[j] : empty;

  LinearSectionIterator zero(linCopy[0].p, linCopy[0].sz);
  LinearSectionIterator one(linCopy[1].p, linCopy[1].sz);
  LinearSectionIterator two(linCopy[2].p, linCopy[2].sz);

  /* Build GenericSectionPtr array using iterators */
  tmpPtr[0].sz = linCopy[0].sz;
  tmpPtr[0].sectionIter = &zero;
  tmpPtr[1].sz = linCopy[1].sz;
  tmpPtr[1].sectionIter = &one;
  tmpPtr[2].sz = linCopy[2].sz;
  tmpPtr[2].sectionIter = &two;

  return sendFragmentedSignal(clnt, aSignal, aNode, tmpPtr, secs);
}

template <typename SectionPtr>
void TransporterFacade::handle_message_too_big(NodeId aNode,
                                               const NdbApiSignal *aSignal,
                                               const SectionPtr ptr[3],
                                               Uint32 /* line */) const {
  /* If message is too big when sending CmvmiDummySignal log a convenient
   * message about it to.
   * Note that CmvmiDummySignal is not intended for production usage but for
   * use by test cases.
   * In production this function do nothing and the message too big failure
   * handling is left to caller.
   */
  if (aSignal->theVerId_signalNumber == GSN_DUMP_STATE_ORD &&
      aSignal->theData[0] == DumpStateOrd::CmvmiDummySignal) {
    const Uint32 rep_node_id = aSignal->theData[1];
    const Uint32 num_secs = aSignal->m_noOfSections;
    char msg[24 * sizeof(Uint32)];
    snprintf(msg, sizeof(msg),
             "Failed sending CmvmiDummySignal"
             " (size %u+%u+%u+%u+%u) from %u to %u.",
             aSignal->getLength(), num_secs, (num_secs > 0) ? ptr[0].sz : 0,
             (num_secs > 1) ? ptr[1].sz : 0, (num_secs > 2) ? ptr[2].sz : 0,
             ownId(), aNode);
    const Uint32 len = strlen(msg) + 1;
    NdbApiSignal bSignal(numberToRef(API_CLUSTERMGR, ownId()));
    bSignal.theTrace = TestOrd::TraceAPI;
    bSignal.theReceiversBlockNumber = CMVMI;
    bSignal.theVerId_signalNumber = GSN_EVENT_REP;
    bSignal.theLength = ((len + 3) / 4) + 1;
    Uint32 *data = bSignal.theData;
    data[0] = NDB_LE_InfoEvent;
    memcpy(&data[1], msg, len);
    LinearSectionPtr ptr[3];
    TrpId trp_id = 0;
    theTransporterRegistry->prepareSend(m_poll_owner, &bSignal,
                                        1,  // JBB
                                        bSignal.getConstDataPtrSend(),
                                        rep_node_id, trp_id, ptr);
  } else {
    assert(false);  // SEND_MESSAGE_TOO_BIG
  }
}

int TransporterFacade::sendSignal(trp_client *clnt, const NdbApiSignal *aSignal,
                                  NodeId aNode, const LinearSectionPtr ptr[3],
                                  Uint32 secs) {
  assert(clnt == m_poll_owner || is_poll_owner_thread() == false);

  Uint32 save = aSignal->m_noOfSections;
  const_cast<NdbApiSignal *>(aSignal)->m_noOfSections = secs;
#ifdef API_TRACE
  if (setSignalLog() && TRACE_GSN(aSignal->theVerId_signalNumber)) {
    SignalHeader tmp = *aSignal;
    tmp.theSendersBlockRef = numberToRef(aSignal->theSendersBlockRef, theOwnId);
    signalLogger.sendSignal(tmp, 1, aSignal->getConstDataPtrSend(), aNode, ptr,
                            secs);
    signalLogger.flushSignalLog();
  }
#endif
  TrpId trp_id = 0;
  SendStatus ss = theTransporterRegistry->prepareSend(
      clnt, aSignal,
      1,  // JBB
      aSignal->getConstDataPtrSend(), aNode, trp_id, ptr);
  const_cast<NdbApiSignal *>(aSignal)->m_noOfSections = save;
  if (likely(ss == SEND_OK)) {
    assert(theClusterMgr->getNodeInfo(aNode).is_confirmed() ||
           aSignal->readSignalNumber() == GSN_API_REGREQ);
    return 0;
  }
  if (unlikely(ss == SEND_MESSAGE_TOO_BIG)) {
    handle_message_too_big(aNode, aSignal, ptr, __LINE__);
  }
  return -1;
}

int TransporterFacade::sendSignal(trp_client *clnt, const NdbApiSignal *aSignal,
                                  NodeId aNode, const GenericSectionPtr ptr[3],
                                  Uint32 secs) {
  assert(clnt == m_poll_owner || is_poll_owner_thread() == false);

  Uint32 save = aSignal->m_noOfSections;
  const_cast<NdbApiSignal *>(aSignal)->m_noOfSections = secs;
#ifdef API_TRACE
  if (setSignalLog() && TRACE_GSN(aSignal->theVerId_signalNumber)) {
    SignalHeader tmp = *aSignal;
    tmp.theSendersBlockRef = numberToRef(aSignal->theSendersBlockRef, theOwnId);
    signalLogger.sendSignal(tmp, 1, aSignal->getConstDataPtrSend(), aNode, ptr,
                            secs);
    signalLogger.flushSignalLog();
    for (Uint32 i = 0; i < secs; i++) ptr[i].sectionIter->reset();
  }
#endif
  TrpId trp_id = 0;
  SendStatus ss = theTransporterRegistry->prepareSend(
      clnt, aSignal,
      1,  // JBB
      aSignal->getConstDataPtrSend(), aNode, trp_id, ptr);
  int ret;
  if (ss == SEND_OK) {
    assert(theClusterMgr->getNodeInfo(aNode).is_confirmed() ||
           aSignal->readSignalNumber() == GSN_API_REGREQ);
    ret = 0;
  } else {
    if (unlikely(ss == SEND_MESSAGE_TOO_BIG)) {
      handle_message_too_big(aNode, aSignal, ptr, __LINE__);
    }
    ret = -1;
  }
  const_cast<NdbApiSignal *>(aSignal)->m_noOfSections = save;
  return ret;
}

/******************************************************************************
 * CONNECTION METHODS  Etc
 ******************************************************************************/
void TransporterFacade::startConnecting(NodeId aNodeId) {
  const TrpId trpId = theTransporterRegistry->get_the_only_base_trp(aNodeId);
  if (trpId != 0) {
    theTransporterRegistry->setIOState(trpId, NoHalt);
    theTransporterRegistry->start_connecting(trpId);
  }
}

void TransporterFacade::startDisconnecting(NodeId aNodeId) {
  const TrpId trpId = theTransporterRegistry->get_the_only_base_trp(aNodeId);
  if (trpId != 0) {
    theTransporterRegistry->start_disconnecting(trpId);
  }
}

/**
 * ClusterMgr maintains the shared global data.
 * Notify it about the changed connection state.
 */
void TransporterFacade::reportConnected(NodeId aNodeId) {
  theClusterMgr->reportConnected(aNodeId);
}

void TransporterFacade::reportDisconnected(NodeId aNodeId) {
  theClusterMgr->reportDisconnected(aNodeId);
}

NodeId TransporterFacade::ownId() const { return theOwnId; }

NodeId TransporterFacade::get_an_alive_node() {
  DBUG_ENTER("TransporterFacade::get_an_alive_node");
  DBUG_PRINT("enter", ("theStartNodeId: %d", theStartNodeId));
#ifdef VM_TRACE
#ifdef NDB_USE_GET_ENV
  const char *p = NdbEnv_GetEnv("NDB_ALIVE_NODE_ID", (char *)0, 0);
  if (p != 0 && *p != 0) DBUG_RETURN(atoi(p));
#endif
#endif
  NodeId i;
  for (i = theStartNodeId; i < MAX_NDB_NODES; i++) {
    if (get_node_alive(i)) {
      DBUG_PRINT("info", ("Node %d is alive", i));
      theStartNodeId = ((i + 1) % MAX_NDB_NODES);
      DBUG_RETURN(i);
    }
  }
  for (i = 1; i < theStartNodeId; i++) {
    if (get_node_alive(i)) {
      DBUG_PRINT("info", ("Node %d is alive", i));
      theStartNodeId = ((i + 1) % MAX_NDB_NODES);
      DBUG_RETURN(i);
    }
  }
  DBUG_RETURN((NodeId)0);
}

TransporterFacade::ThreadData::ThreadData(Uint32 size)
    : m_use_cnt(0), m_firstFree(END_OF_LIST), m_expanding(false) {
  expand(size);
}

/**
 * Expand the Vectors used to register open Clients.
 * As a Vector::expand() reallocates and moves the Vector
 * items, it has to be protected from concurrent get'ers.
 * This is achieved by requiring that both get, expand and close
 * is only called from ::deliver_signal while having the poll right.
 *
 * In addition to having the poll right,
 * m_open_close_mutex should be held.
 */
void TransporterFacade::ThreadData::expand(Uint32 size) {
  const Uint32 sz = m_clients.size();
  m_clients.expand(sz + size);
  for (Uint32 i = 0; i < size; i++) {
    m_clients.push_back(Client(nullptr, sz + i + 1));
  }

  m_clients.back().m_next = m_firstFree;
  m_firstFree = m_clients.size() - size;
  m_expanding = false;
}

/**
 * Register 'clnt' in the TransporterFacade.
 * Should be called with m_open_close_mutex locked.
 *
 * Poll right *not* required as ::open do not ::expand!:
 *
 * As there are no mutex protection between threads ::open'ing a
 * trp_client and threads get'ing the trp_client* for *other clients*,
 * we can't let ::open do an ::expand of the Vectors. (See above.)
 * Concurrent open and get/close of the same clients can't happen as
 * there could be no such operation until after a successful open.
 * Thus, this needs no concurrency control.
 */
int TransporterFacade::ThreadData::open(trp_client *clnt) {
  const Uint32 nextFree = m_firstFree;

  if (m_clients.size() >= MAX_NO_THREADS && nextFree == END_OF_LIST) {
    return -1;
  }

  require(nextFree != END_OF_LIST);  //::expand before ::open if required
  m_use_cnt++;
  m_firstFree = m_clients[nextFree].m_next;
  m_clients[nextFree] = Client(clnt, INACTIVE);
  ;

  return indexToNumber(nextFree);
}

/**
 * Should be called with m_open_close_mutex locked and
 * only when having the poll right (Protects ::get())
 */
int TransporterFacade::ThreadData::close(int number) {
  const Uint32 nextFree = m_firstFree;
  const int index = numberToIndex(number);

  /**
   * Guard against race between close from multiple threads.
   * Couldn't detect this for sure until we now have the poll right.
   */
  if (m_clients[index].m_clnt == nullptr) return 0;

  assert(m_use_cnt);
  m_use_cnt--;
  m_firstFree = index;
  m_clients[index] = Client(nullptr, nextFree);
  return 0;
}

Uint32 TransporterFacade::get_active_ndb_objects() const {
  return m_threads.m_use_cnt;
}

Uint32 TransporterFacade::mapRefToIdx(Uint32 reference) const {
  assert(reference >= MIN_API_BLOCK_NO);
  return reference - MIN_API_BLOCK_NO;
}

/**
 * Propose a client to become new poll owner if
 * no one is currently assigned.
 *
 * Prefer the receiver thread if it is waiting in the poll_queue and is
 * in ACTIVE state, else pick the 'last' in the poll_queue.
 *
 * The suggested poll owner will race with any other clients
 * not yet 'WAITING' to become poll owner. (If any such arrives.)
 */
void TransporterFacade::propose_poll_owner() {
  int retries = 0;

  do {
    lock_poll_mutex();

    if (m_poll_owner != nullptr || m_poll_queue_tail == nullptr) {
      /**
       * New poll owner already appointed or none waiting
       * ...no need to do anything
       */
      unlock_poll_mutex();
      break;
    }

    /**
     * Prefer an ACTIVE receiver thread as the new poll owner *candidate*.
     * Else pick the last client in the poll queue, not being the recv_client
     */
    trp_client *const new_owner =
        // Prefer recv_client if in ACTIVE state
        (recv_client && recv_client->m_poll.m_poll_queue &&
         recv_client->m_state == ReceiveThreadClient::ACTIVE)
            ? recv_client
        // Avoid the recv_client as it is not ACTIVE
        : (m_poll_queue_tail == recv_client &&
           m_poll_queue_tail->m_poll.m_prev != nullptr)
            // 'tail' is the recv_client, prefer another
            ? m_poll_queue_tail->m_poll.m_prev
            : m_poll_queue_tail;

    /**
     * Note: we can only try lock here, to prevent potential deadlock
     *   given that we acquire mutex in different order when starting to poll.
     *   Only lock if not already locked (can happen when signals received
     *   and trp_client isn't ready).
     */
    if (NdbMutex_Trylock(new_owner->m_mutex) == 0) {
      assert(new_owner->m_poll.m_poll_queue == true);
      /**
       * It can happen that new_owner is in state PQ_WOKEN if it is currently
       * closing down its client and another thread comes in between as poll
       * owner before the closing thread has the chance to become poll owner.
       */
      assert(new_owner->m_poll.m_waiting == trp_client::PollQueue::PQ_WAITING ||
             new_owner->m_poll.m_waiting == trp_client::PollQueue::PQ_WOKEN);
      unlock_poll_mutex();

      /**
       * Signal the proposed poll owner
       */
      NdbCondition_Signal(new_owner->m_poll.m_condition);
      NdbMutex_Unlock(new_owner->m_mutex);
      break;
    }
    unlock_poll_mutex();

    /**
     * Failed to lock new owner. Retry, but start to
     * back off the CPU if many retries are needed.
     */
    retries++;
    if (retries > 100)
      NdbSleep_MicroSleep(10);
    else if (retries > 10)
      my_thread_yield();

  } while (true);
}

/**
 * Try to acquire the poll-right to 'clnt' within the specified 'wait_time'.
 *
 * By design, we allow existing clnt's 'WAITING' in poll_queue, and new
 * not_yet_waiting clients, to race for becoming the poll-owner.
 * Getting a new poll owner ASAP is critical for API throughput and
 * latency, so its better to give the poll right to the first thread
 * being able to take it, rather than trying to implement some 'fairness' in
 * whose turn it is to be the poll owner. The later would have implied
 * waiting for that thread to eventually be scheduled by the OS.
 *
 * Assumed to be called with 'clnt' locked and 'poll_mutex' not held.
 */
bool TransporterFacade::try_become_poll_owner(trp_client *clnt,
                                              Uint32 wait_time) {
  assert(clnt->m_poll.m_locked == true);
  assert(clnt->m_poll.m_poll_owner == false);

  lock_poll_mutex();
  assert(m_poll_owner != clnt);

  if (m_poll_owner != nullptr) {
    /*
      Dont wait for the poll right to become available if
      no wait_time is allowed. Return without poll right,
      and without waiting in poll queue.
    */
    if (wait_time == 0) {
      unlock_poll_mutex();

      assert(clnt->m_poll.m_waiting == trp_client::PollQueue::PQ_WAITING);
      clnt->m_poll.m_waiting = trp_client::PollQueue::PQ_IDLE;
      dbg("%p - poll_owner == false && wait_time == 0 => return", clnt);
      return false;
    }

    /* All poll "right" waiters are in the poll_queue */
    add_to_poll_queue(clnt);

    /**
     * We will sleep on a conditional mutex while the poll right
     * can't be acquired. While sleeping the thread owning the poll "right"
     * could wake us up if it has delivered all data to us. That will
     * terminate our wait. (PQ_WOKEN state)
     *
     * We could also be woken up by the current poll owner which will
     * set 'm_poll_owner = NULL' and signal *one of* the waiting clients
     * when it retire as poll owner. The poll owner right could then be
     * acquired if we find it free, *or* another client could have raced us
     * and already grabbed it.
     *
     * We could also terminate the poll-right wait due to the max
     * wait_time being exceeded.
     */
    struct timespec wait_end;
    NdbCondition_ComputeAbsTime(&wait_end, wait_time);

    while (true)  //(m_poll_owner != nullptr)
    {
      unlock_poll_mutex();  // Release while waiting
      dbg("cond_wait(%p)", clnt);
      const int ret = NdbCondition_WaitTimeoutAbs(clnt->m_poll.m_condition,
                                                  clnt->m_mutex, &wait_end);

      switch (clnt->m_poll.m_waiting) {
        case trp_client::PollQueue::PQ_WOKEN:
          dbg("%p - PQ_WOKEN", clnt);
          /**
           *  We have already been taken out of poll queue
           * in the normal case, but when we sent CLOSE_COMREQ the
           * state PQ_WOKEN signals that the receiver has executed
           * perform_close_clnt. So in this case we might still be left
           * in the poll queue.
           */
          if (clnt->m_poll.m_poll_queue) {
            lock_poll_mutex();
            if (clnt->m_poll.m_poll_queue) {
              remove_from_poll_queue(clnt);
            }
            unlock_poll_mutex();
          }
          assert(clnt->m_poll.m_poll_queue == false);
          assert(clnt->m_poll.m_poll_owner == false);
          clnt->m_poll.m_waiting = trp_client::PollQueue::PQ_IDLE;
          return false;
        case trp_client::PollQueue::PQ_WAITING:
          dbg("%p - PQ_WAITING", clnt);
          break;
        case trp_client::PollQueue::PQ_IDLE:
          dbg("%p - PQ_IDLE", clnt);
          [[fallthrough]];
        default:
          require(false);  // should not happen!!
          break;
      }

      lock_poll_mutex();
      if (m_poll_owner == nullptr) {
        assert(clnt->m_poll.m_poll_owner == false);
        break;
      }
      if (ret == ETIMEDOUT) {
        /**
         * We got timeout...hopefully rare...
         */
        assert(m_poll_owner != clnt);
        assert(clnt->m_poll.m_poll_owner == false);
        remove_from_poll_queue(clnt);
        unlock_poll_mutex();

        clnt->m_poll.m_waiting = trp_client::PollQueue::PQ_IDLE;
        dbg("%p - PQ_WAITING poll_owner == false => return", clnt);
        return false;
      }
    }
    remove_from_poll_queue(clnt);

    /**
     * We were proposed as new poll owner, and was first to wakeup
     */
    dbg("%p - PQ_WAITING => new poll_owner", clnt);
  }

  /* We found the poll-right available, grab it */
  assert(m_poll_owner == nullptr);
  m_poll_owner = clnt;
  m_poll_owner_tid = my_thread_self();

#ifdef NDB_MUTEX_DEADLOCK_DETECTOR
  // We 'own' the poll lock even if we do not hold the poll_mutex itself
  NdbMutex_EnterSerializedRegion(m_poll_owner_region);
#endif

  unlock_poll_mutex();

  assert(clnt->m_poll.m_poll_owner == false);
  clnt->m_poll.m_poll_owner = true;
  return true;
}

/**
 * When a m_poll_owner has been assigned, any actual receiver polling
 * (::external_poll()) has to be enclosed in a pair of
 * start_poll() - finish_poll() calls.
 */
void TransporterFacade::start_poll() {
  assert(m_poll_owner != nullptr);
  assert(m_poll_owner->m_poll.m_waiting == trp_client::PollQueue::PQ_WAITING);
  assert(m_poll_owner->m_poll.m_locked);

  assert(m_locked_cnt == 0);
  m_locked_cnt = 1;
  m_locked_clients[0] = m_poll_owner;
  assert(m_poll_owner->is_locked_for_poll() == false);
  m_poll_owner->set_locked_for_poll(true);
  dbg("%p becomes poll owner", m_poll_owner);
}

int TransporterFacade::finish_poll(trp_client *arr[]) {
  assert(m_poll_owner != nullptr);
  assert(m_locked_cnt > 0);
  assert(m_locked_cnt <= MAX_LOCKED_CLIENTS);
  assert(m_locked_clients[0] == m_poll_owner);

  const Uint32 lock_cnt = m_locked_cnt;
  int cnt_woken = 0;

#ifndef NDEBUG
  {
    // no duplicates
    if (DBG_POLL) printf("after external_poll: cnt: %u ", lock_cnt);
    for (Uint32 i = 0; i < lock_cnt; i++) {
      trp_client *tmp = m_locked_clients[i];
      if (DBG_POLL) printf("%p(%u) ", tmp, tmp->m_poll.m_waiting);
      for (Uint32 j = i + 1; j < lock_cnt; j++) {
        assert(tmp != m_locked_clients[j]);
      }
    }
    if (DBG_POLL) printf("\n");

    for (Uint32 i = 1; i < lock_cnt; i++) {
      trp_client *tmp = m_locked_clients[i];
      if (tmp->m_poll.m_locked == true) {
        assert(tmp->m_poll.m_waiting != trp_client::PollQueue::PQ_IDLE);
      } else {
        assert(tmp->m_poll.m_poll_owner == false);
        assert(tmp->m_poll.m_poll_queue == false);
        assert(tmp->m_poll.m_waiting == trp_client::PollQueue::PQ_IDLE);
      }
    }
  }
#endif

  /**
   * we're finished polling:
   *  - Any signals sent by receivers has been 'safe-sent' by the poll_owner.
   *    Thus we have to flush the poll_owner (== clnt) send buffer to 'global'
   *    TransporterFacade queues.
   *    (At latest sent by send thread after 'sendThreadWaitMillisec')
   *  - Assert: There should *not* be any 'unflushed_send' for the other clients
   *  - Unlock the clients.
   */
  trp_client *clnt = m_poll_owner;

  assert(clnt->is_locked_for_poll() == true);
  clnt->flush_send_buffers();
  clnt->set_locked_for_poll(false);
  dbg("%p->set_locked_for_poll false", clnt);

  /**
   * count woken clients
   *   skip m_poll_owner in m_locked_clients[0]
   *   and put the woken clients to the left in array,
   *   non woken are filled in from right in array
   */
  int cnt_not_woken = 0;
  for (Uint32 i = 1; i < lock_cnt; i++) {
    trp_client *tmp = m_locked_clients[i];
    bool woken = (tmp->m_poll.m_waiting == trp_client::PollQueue::PQ_WOKEN);
    assert(tmp->is_locked_for_poll() == true);
    assert(tmp->has_unflushed_sends() == false);
    tmp->set_locked_for_poll(false);
    dbg("%p->set_locked_for_poll false", tmp);
    if (woken) {
      arr[cnt_woken++] = tmp;
    } else {
      arr[lock_cnt - 2 - cnt_not_woken++] = tmp;
    }
  }
  assert(cnt_woken + cnt_not_woken == (int)(lock_cnt - 1));

  if (DBG_POLL) {
    printf("after sort: cnt: %u ", lock_cnt - 1);
    for (Uint32 i = 0; i < lock_cnt - 1; i++) {
      trp_client *tmp = arr[i];
      printf("%p(%u) ", tmp, tmp->m_poll.m_waiting);
    }
    printf("\n");
  }
  return cnt_woken;
}

/**
 * Poll the Transporters for incoming messages.
 * Also 'update_connections' status in regular intervals
 * controlled by the flag 'm_check_connections'.
 * (::threadMainReceive() is responsible for requesting
 * this in regular intervals)
 *
 * Both of these operations require the poll right to
 * have been acquired. If we are not already 'poll_owner',
 * we will try to set it within the timeout 'wait_time'.
 *
 * If we get the poll rights within the specified wait_time,
 * we will repeatedly poll the receiver until either
 * 'clnt' is woken up, or the max 'wait_time' expires.
 *
 * Poll ownership is released on return if not
 * 'stay_poll_owner' is requested.
 *
 * 'clnt->m_poll.m_poll_owner' will maintain whether
 * poll right is being owned or not,
 */
void TransporterFacade::do_poll(trp_client *clnt, Uint32 wait_time,
                                bool stay_poll_owner) {
  dbg("do_poll(%p)", clnt);
  const NDB_TICKS start = NdbTick_getCurrentTicks();
  clnt->m_poll.m_waiting = trp_client::PollQueue::PQ_WAITING;
  assert(clnt->m_poll.m_locked == true);

  Uint32 elapsed_ms = 0;  //'wait_time' used so far
  do {
    if (clnt->m_poll.m_poll_owner == false) {
      assert(wait_time >= elapsed_ms);
      const Uint32 rem_wait_time = wait_time - elapsed_ms;
      /**
       * If we fail to become poll owner, we either was PQ_WOKEN
       * up as we received what we were PQ_WAITING for, or other
       * clients held the poll right until 'wait_time' expired.
       *
       * In either case the clients which held the poll right
       * will be responsible for signaling the new client
       * preferred to be next poll owner. (see further below)
       * So we can just return here.
       */
      if (!try_become_poll_owner(clnt, rem_wait_time)) return;
    }
    assert(clnt->m_poll.m_poll_owner == true);

    /**
     * We have the poll "right" and we poll until data is received. After
     * receiving data we will check if all data is received,
     * if not we poll again.
     */
    start_poll();
    dbg("%p->external_poll", clnt);
    external_poll(wait_time);

    /**
     * As m_locked_clients[] are protected by owning the poll right,
     * which we are soon about to release, its contents is now copied
     * out to locked[] by finish_poll().
     * NOTE: 'clnt' being the first 'locked' is not copied out
     */
    const Uint32 locked_cnt = m_locked_cnt;
    trp_client *locked[MAX_LOCKED_CLIENTS - 1];  // locked_cnt-1
    const int cnt_woken = finish_poll(locked);
    m_locked_cnt = 0;

    lock_poll_mutex();

    if (locked_cnt > m_num_active_clients) {
      m_num_active_clients = locked_cnt;
    }

    /**
     * Now remove all woken from poll queue
     * note: poll mutex held
     */
    remove_from_poll_queue(locked, cnt_woken);

    /**
     * Release poll right temporarily in case we get
     * suspended when unlocking other trp_client::m_mutex'es below.
     * If still available, the poll right will be re-acquired in our
     * next round in this poll-loop
     *
     * We can't (reasonably) control whether we are yielded by
     * the OS scheduler, so we can just prepare for being
     * suspended here.
     */
    if (!stay_poll_owner) {
#ifdef NDB_MUTEX_DEADLOCK_DETECTOR
      NdbMutex_LeaveSerializedRegion(m_poll_owner_region);
#endif
      clnt->m_poll.m_poll_owner = false;
      m_poll_owner = nullptr;
      /**
       * Note, there is no platform independent 'NULL' defined for
       * thread id, so can't clear it as one might have expected here.
       * Instead we define that 'owner_tid' is only valid iff
       * 'm_poll_owner != NULL'
       */
      // m_poll_owner_tid = 0;
    }
    unlock_poll_mutex();

    if (!transfer_responsibility(locked, cnt_woken, locked_cnt - 1)) {
      /**
       * Now wake all the woken clients
       */
      unlock_and_signal(locked, cnt_woken);

      /**
       * And unlock the rest that we delivered messages to
       */
      for (Uint32 i = cnt_woken; i < locked_cnt - 1; i++) {
        dbg("unlock (%p)", locked[i]);
        NdbMutex_Unlock(locked[i]->m_mutex);
      }
    }

    // Terminate polling if we are PQ_WOKEN
    assert(clnt->m_poll.m_waiting != trp_client::PollQueue::PQ_IDLE);
    if (clnt->m_poll.m_waiting == trp_client::PollQueue::PQ_WOKEN) break;

    // Check for poll-timeout
    const NDB_TICKS now = NdbTick_getCurrentTicks();
    elapsed_ms = NdbTick_Elapsed(start, now).milliSec();

  } while (elapsed_ms < wait_time);

  clnt->m_poll.m_waiting = trp_client::PollQueue::PQ_IDLE;

  // Might have to find a new poll owner
  propose_poll_owner();

  dbg("%p->do_poll return", clnt);
  return;
}

void TransporterFacade::wakeup(trp_client *clnt) {
  switch (clnt->m_poll.m_waiting) {
    case trp_client::PollQueue::PQ_WAITING:
      dbg2("wakeup(%p) PQ_WAITING => PQ_WOKEN on %p", clnt, this);
      clnt->m_poll.m_waiting = trp_client::PollQueue::PQ_WOKEN;
      break;
    case trp_client::PollQueue::PQ_WOKEN:
      dbg2("wakeup(%p) PQ_WOKEN on %p", clnt, this);
      break;
    case trp_client::PollQueue::PQ_IDLE:
      dbg2("wakeup(%p) PQ_IDLE on %p", clnt, this);
      break;
  }
}

// static
void TransporterFacade::unlock_and_signal(trp_client *const arr[], Uint32 cnt) {
  for (Uint32 i = 0; i < cnt; i++) {
    NdbCondition_Signal(arr[i]->m_poll.m_condition);
    NdbMutex_Unlock(arr[i]->m_mutex);
  }
}

bool TransporterFacade::check_if_locked(const trp_client *clnt,
                                        const Uint32 start) const {
  for (Uint32 i = start; i < m_locked_cnt; i++) {
    if (m_locked_clients[i] == clnt)  // already locked
      return true;
  }
  return false;
}

/**
 * Note that it is a requirement that we check that 'clnt' is not yet
 * 'is_locked_for_poll()' before we ::lock_client(), else it may deadlock.
 */
void TransporterFacade::lock_client(trp_client *clnt) {
  assert(m_locked_cnt <= MAX_LOCKED_CLIENTS);
  assert(check_if_locked(clnt, 0) == false);

  NdbMutex_Lock(clnt->m_mutex);
  assert(!clnt->is_locked_for_poll());

  Uint32 locked_cnt = m_locked_cnt;
  clnt->set_locked_for_poll(true);
  dbg("lock_client(%p)", clnt);

  assert(m_locked_cnt < MAX_LOCKED_CLIENTS);
  m_locked_clients[locked_cnt] = clnt;
  m_locked_cnt = locked_cnt + 1;
}

void TransporterFacade::add_to_poll_queue(
    trp_client *clnt)  // Need thePollMutex
{
  assert(clnt != nullptr);
  assert(clnt->m_poll.m_prev == nullptr);
  assert(clnt->m_poll.m_next == nullptr);
  assert(clnt->m_poll.m_locked == true);
  assert(clnt->m_poll.m_poll_owner == false);
  assert(clnt->m_poll.m_poll_queue == false);

  clnt->m_poll.m_poll_queue = true;
  if (m_poll_queue_head == nullptr) {
    assert(m_poll_queue_tail == nullptr);
    m_poll_queue_head = clnt;
  } else {
    assert(m_poll_queue_tail->m_poll.m_next == nullptr);
    m_poll_queue_tail->m_poll.m_next = clnt;
    clnt->m_poll.m_prev = m_poll_queue_tail;
  }
  m_poll_queue_tail = clnt;
  m_poll_waiters++;
}

void TransporterFacade::remove_from_poll_queue(trp_client *const arr[],
                                               Uint32 cnt) {
  for (Uint32 i = 0; i < cnt; i++) {
    if (arr[i]->m_poll.m_poll_queue) {
      remove_from_poll_queue(arr[i]);
    }
  }
}

void TransporterFacade::remove_from_poll_queue(
    trp_client *clnt)  // Need thePollMutex
{
  assert(clnt != nullptr);
  assert(clnt->m_poll.m_locked == true);
  assert(clnt->m_poll.m_poll_owner == false);
  assert(clnt->m_poll.m_poll_queue == true);
  assert(m_poll_waiters > 0);
  m_poll_waiters--;

  if (clnt->m_poll.m_prev != nullptr) {
    clnt->m_poll.m_prev->m_poll.m_next = clnt->m_poll.m_next;
  } else {
    assert(m_poll_queue_head == clnt);
    m_poll_queue_head = clnt->m_poll.m_next;
  }

  if (clnt->m_poll.m_next != nullptr) {
    clnt->m_poll.m_next->m_poll.m_prev = clnt->m_poll.m_prev;
  } else {
    assert(m_poll_queue_tail == clnt);
    m_poll_queue_tail = clnt->m_poll.m_prev;
  }

  if (m_poll_queue_head == nullptr)
    assert(m_poll_queue_tail == nullptr);
  else if (m_poll_queue_tail == nullptr)
    assert(m_poll_queue_head == nullptr);

  clnt->m_poll.m_prev = nullptr;
  clnt->m_poll.m_next = nullptr;
  clnt->m_poll.m_poll_queue = false;
}

template class Vector<TransporterFacade::ThreadData::Client>;

#include "SignalSender.hpp"

/**
 * ::flush_send_buffer()
 *
 * Append a set of send buffer pages (TFPage) to the
 * TransporterFacade 'global' list of send buffers to
 * the specified node.
 *
 * The send buffers to be appended has been produced
 * thread-locally by the client thread. The send buffers,
 * both in the TransporterFacade, and trp_client is enabled/disabled
 * synchronously when a Transporter connect or disconnect.
 * Furthermore, the client is not allowed to allocate
 * from a disabled send buffer. This guards us from ever
 * flushing any data into a disabled send buffer. (Asserted below)
 */
void TransporterFacade::flush_send_buffer(TrpId trp, const TFBuffer *sb) {
  if (unlikely(sb->m_head == nullptr))  // Cleared by ::disable_send_buffer()
    return;

  assert(trp < NDB_ARRAY_SIZE(m_send_buffers));
  assert(m_active_trps.get(trp));
  struct TFSendBuffer *b = m_send_buffers + trp;
  Guard g(&b->m_mutex);
  assert(b->m_node_enabled);
  b->m_current_send_buffer_size += sb->m_bytes_in_buffer;
  b->m_flushed_cnt++;
  link_buffer(&b->m_buffer, sb);
}

/**
 * Try to send the prepared send buffers to 'node'.
 * Could fail to send if 'try_lock_send' not granted.
 * The thread holding the lock will then take over the
 * send for us.
 * If there are more data than could be sent in a single
 * try, the send-thread will be requested to be started.
 *
 * Requires the 'b->m_mutex' to be held by callee
 */
void TransporterFacade::try_send_buffer(TrpId trp_id, struct TFSendBuffer *b) {
  if (!b->try_lock_send()) {
    /**
     * Did not get lock, held by other sender.
     * Holder of send lock will check if here is data, and wake send-thread
     * if needed
     */
  } else {
    assert(b->m_current_send_buffer_size ==
           b->m_buffer.m_bytes_in_buffer + b->m_out_buffer.m_bytes_in_buffer);

    do_send_buffer(trp_id, b);
    const Uint32 out_buffer_bytes = b->m_out_buffer.m_bytes_in_buffer;
    const Uint32 send_buffer_bytes = b->m_current_send_buffer_size;
    b->unlock_send();

    /**
     * NOTE: There are two different variants of 'more_data' being
     *   available for sending immediately after a do_send_buffer():
     *
     *  1) 'send_buffer_bytes > 0 && out_buffer_bytes == 0'
     *     More data was flushed to the 'm_buffer' by other threads
     *     while this thread waited for OS to send 'out_buffer_bytes'.
     *     (Also see 'try_lock_send' comment above)
     *
     *  2) 'out_buffer_bytes > 0'
     *     do_send_buffer() didn't send all m_out_buffer'ed data
     *     being available when it was called. This was likely due
     *     to too much send data, or the adaptive send being too lazy
     *     (Also implies 'send_buffer_bytes > 0')
     *
     * In both cases we append the transporter to the 'm_has_data_trps'
     * which is to be handled by the send thread.
     *
     * We need to wakeup the send thread if it was in either
     * 'deep sleep', or in case of 2), where the send is lagging
     * behind and immediate retry of the send is required.
     *
     * We do *not* want it to wake up immediately if taking a
     * 'micro-nap. That would have prevented it from collecting
     * a larger message to be sent after the 'nap'
     */
    Guard g(m_send_thread_mutex);
    if (send_buffer_bytes > 0) {
      if (m_has_data_trps.isclear() ||  // In 'deep sleep'
          out_buffer_bytes > 0)         // Lagging behind, immediate retry
      {
        wakeup_send_thread();
      }
      m_has_data_trps.set(trp_id);
    } else {
      m_has_data_trps.clear(trp_id);
    }
  }
}

/**
 * Try to send the prepared send buffers on all transporters
 * having pending data. Called regularly from the
 * send thread when woken up by the regular timer,
 * or needed to off load a client thread having
 * 'more_data' to send.
 *
 * Also see ::try_send_buffer() for comments.
 */
void TransporterFacade::try_send_all(const TrpBitmask &trps) {
  for (Uint32 trp = trps.find_first(); trp != TrpBitmask::NotFound;
       trp = trps.find_next(trp + 1)) {
    struct TFSendBuffer *b = &m_send_buffers[trp];
    NdbMutex_Lock(&b->m_mutex);
    if (likely(b->m_current_send_buffer_size > 0)) {
      try_send_buffer(trp, b);
    } else {
      Guard g(m_send_thread_mutex);
      m_has_data_trps.clear(trp);
    }
    NdbMutex_Unlock(&b->m_mutex);
  }
}

/**
 * Precondition: Called with 'm_mutex' lock held
 *               and m_sending flag set.
 *
 * Do actual send of data from m_out_buffer.
 * Any pending data in 'm_buffer' is appended to
 * 'm_out_buffer' before sending.
 *
 * Will take care of any deferred buffer reset
 * before return.
 */
void TransporterFacade::do_send_buffer(TrpId trp_id, struct TFSendBuffer *b) {
  assert(!b->try_lock_send());  // Sending already locked
  assert(b->m_node_enabled);

  /**
   * Copy all data from m_buffer to m_out_buffer
   */
  TFBuffer copy = b->m_buffer;
  b->m_buffer.clear();
  b->m_flushed_cnt = 0;

  /**
   * Note that we still hold the 'b->m_sending' right
   * even if we now unlock 'm_mutex'.
   */
  NdbMutex_Unlock(&b->m_mutex);
  if (copy.m_bytes_in_buffer > 0) {
    link_buffer(&b->m_out_buffer, &copy);
  }
  theTransporterRegistry->performSend(trp_id);

  NdbMutex_Lock(&b->m_mutex);
  /**
   * Sending to node could possible have been disabled
   * wo/ 'out_buffer' being cleared as we held the send_lock.
   * Thus, discard any out_buffer'ed send data now.
   */
  if (unlikely(!b->m_node_enabled && b->m_out_buffer.m_head != nullptr)) {
    m_send_buffer.release_list(b->m_out_buffer.m_head);
    b->m_out_buffer.clear();
  }

  /* Update pending bytes to be sent. */
  b->m_current_send_buffer_size =
      b->m_buffer.m_bytes_in_buffer + b->m_out_buffer.m_bytes_in_buffer;

  /**
   * Maintaining send_buffer_usage in API despite :
   *  - There is no ndbinfo to report it
   *  - We do not use the slowdown / overload states.
   * Note that allocBytes is only used by ndbinfo, thus unused by the API,
   * so not calculated currently.
   */
  constexpr Uint64 allocBytes = 0;
  theTransporterRegistry->update_send_buffer_usage(
      trp_id, allocBytes, b->m_current_send_buffer_size);
}

/**
 * Precondition: ::get_bytes_to_send_iovec() & ::bytes_sent()
 *
 * Required to be called with m_send_buffers[trp_id].m_sending==true.
 * 'm_sending==true' is a 'lock' which signals to other threads
 * to back of from the 'm_out_buffer' for this transporter.
 */
Uint32 TransporterFacade::get_bytes_to_send_iovec(TrpId trp_id,
                                                  struct iovec *dst,
                                                  Uint32 max) {
  if (max == 0) {
    return 0;
  }

  Uint32 count = 0;
  TFBuffer *b = &m_send_buffers[trp_id].m_out_buffer;
  TFBufferGuard g0(*b);
  TFPage *page = b->m_head;
  while (page != nullptr && count < max) {
    dst[count].iov_base = page->m_data + page->m_start;
    dst[count].iov_len = page->m_bytes;
    assert(Uint32{page->m_start} + page->m_bytes <= page->max_data_bytes());
    page = page->m_next;
    count++;
  }

  return count;
}

Uint32 TransporterFacade::bytes_sent(TrpId trp_id, Uint32 bytes) {
  TFBuffer *b = &m_send_buffers[trp_id].m_out_buffer;
  TFBufferGuard g0(*b);
  Uint32 used_bytes = b->m_bytes_in_buffer;
  Uint32 page_count = 0;

  if (bytes == 0) {
    return used_bytes;
  }

  assert(used_bytes >= bytes);
  used_bytes -= bytes;
  b->m_bytes_in_buffer = used_bytes;

  TFPage *page = b->m_head;
  TFPage *prev = nullptr;
  while (bytes && bytes >= page->m_bytes) {
    prev = page;
    bytes -= page->m_bytes;
    page = page->m_next;
    page_count++;
  }

  if (used_bytes == 0) {
    m_send_buffer.release(b->m_head, b->m_tail, page_count);
    b->m_head = nullptr;
    b->m_tail = nullptr;
  } else {
    if (prev) {
      m_send_buffer.release(b->m_head, prev, page_count);
    }

    page->m_start += bytes;
    page->m_bytes -= bytes;
    assert(Uint32{page->m_start} + page->m_bytes <= page->max_data_bytes());
    b->m_head = page;
  }

  return used_bytes;
}

/**
 * ::enable_send_buffer(), ::disable_send_buffer()
 *
 * Enable / disable send on the specified 'transporter'.
 * Any pending data in the TransporterFacade and trp_client's
 * send buffers are discarded when 'disabled'.
 *
 * Require the 'poll-right' to be held, and takes the required locks
 * to update the global and local send buffer structures as needed.
 *
 * Handle both the 'global' send buffers in the TransporterFacade,
 * and the clients thread-local send buffers. Note, that enable/disable is
 * *not* an atomic operation across the global TF-buffers and thread-local
 * client buffers: The consistency requirement is such that the global TF buffer
 * should be enabled if any of the thread-local client buffers are enabled.
 * Thus, the 'global' (TF) buffers has to be enabled prior to enabling
 * any thread-local client buffers. While disabling, all thread-local buffers
 * must be disabled before disabling the 'global' TF buffers.
 *
 * The above 'protocol' also ensure that trp_client::isSendEnabled()
 * correctly reflect the current status of the local trp_client
 * buffers and its related 'global' TF buffers.
 *
 * The poll-right guarantee that a enable/disable sequence is fully
 * executed across both the global and local send buffers.
 * It also protects against races between enable, disable (and close)
 * of clients, which otherwise could result in an enabling being overtaken
 * by a disable of the same node, or a client being closed while being
 * notified about enable/disable.
 *
 * A 'global' TransporterFacade::m_enabled_trps_mask holding the
 * current set of enabled transporters is also maintained. 'Open' of new
 * clients will  use this to init their current set of enabled transporters.
 * (Must be set prior to client enable/disable callback to handle a
 * a race in ::open_clnt())
 *
 * Also see comments for these methods in TransporterCallback.hpp,
 * and how ::open_clnt() synchronize its set of enabled nodes.
 */
void TransporterFacade::enable_send_buffer(TrpId trp_id) {
  assert(is_poll_owner_thread());

  // Always set the 'outcome' first
  NdbMutex_Lock(m_open_close_mutex);
  assert(!m_enabled_trps_mask.get(trp_id));
  m_enabled_trps_mask.set(trp_id);
  NdbMutex_Unlock(m_open_close_mutex);

  // Enable global buffers
  {
    struct TFSendBuffer *b = &m_send_buffers[trp_id];
    Guard g(&b->m_mutex);

    // There should be no pending buffered send data
    assert(b->m_buffer.m_bytes_in_buffer == 0);
    assert(b->m_out_buffer.m_bytes_in_buffer == 0);

    assert(b->m_node_enabled == false);
    b->m_node_enabled = true;
  }

  // Enable thread-local buffers
  const Uint32 sz = m_threads.m_clients.size();
  for (Uint32 i = 0; i < sz; i++) {
    trp_client *const clnt = m_threads.m_clients[i].m_clnt;
    if (clnt != nullptr) {
      if (clnt->is_locked_for_poll()) {
        clnt->enable_send(trp_id);
      } else {
        Guard g(clnt->m_mutex);
        clnt->enable_send(trp_id);
      }
    }
  }
}

void TransporterFacade::disable_send_buffer(TrpId trp_id) {
  assert(is_poll_owner_thread());

  // Always set the 'outcome' first.
  NdbMutex_Lock(m_open_close_mutex);
  m_enabled_trps_mask.clear(trp_id);
  NdbMutex_Unlock(m_open_close_mutex);

  /**
   * Disable thread local buffers:
   * disable and discard all clients send buffer to 'node'.
   * Avoids these later being flushed to the TransporterFacade send buffer,
   * creating a non-empty transporter send buffer when expecting
   * to be empty after 'disable'.
   */
  const Uint32 sz = m_threads.m_clients.size();
  for (Uint32 i = 0; i < sz; i++) {
    trp_client *clnt = m_threads.m_clients[i].m_clnt;
    if (clnt != nullptr) {
      if (clnt->is_locked_for_poll()) {
        clnt->disable_send(trp_id);
      } else {
        Guard g(clnt->m_mutex);
        clnt->disable_send(trp_id);
      }
    }
  }

  // Disable global buffers when all thread-locals are disabled.
  {
    struct TFSendBuffer *b = &m_send_buffers[trp_id];
    Guard g(&b->m_mutex);
    b->m_node_enabled = false;
    discard_send_buffer(b);
    m_has_data_trps.set(trp_id);
  }
}

/**
 * Precondition: Called with 'm_mutex' lock held.
 *
 * Release all data in our two levels of send buffers.
 * We do not wait for the 'sending lock' to become
 * available. Instead the sender holding it will check
 * for 'disabled' send buffers and clear any remaining
 * data in the m_out_buffer.
 */
void TransporterFacade::discard_send_buffer(struct TFSendBuffer *b) {
  /**
   * Clear the TransporterFacade two levels of send buffers.
   */
  {
    TFBuffer *buffer = &b->m_buffer;
    if (buffer->m_head != nullptr) {
      m_send_buffer.release_list(buffer->m_head);
      buffer->clear();
    }
  }

  if (b->try_lock_send()) {
    TFBuffer *out_buffer = &b->m_out_buffer;
    if (out_buffer->m_head != nullptr) {
      m_send_buffer.release_list(out_buffer->m_head);
      out_buffer->clear();
    }
    b->unlock_send();
  } else {
    /**
     * Current do_send_buffer() hold the send lock.
     * It will detect the disabled node when completed,
     * and clear any remaining out_buffer. Thus,
     * no further action is required now.
     */
    assert(!b->m_node_enabled);  //-> do_send_buffer() will clear
  }

  b->m_current_send_buffer_size = 0;
  b->m_flushed_cnt = 0;
}

void TransporterFacade::set_auto_reconnect(int val) {
  theClusterMgr->m_auto_reconnect = val;
}

int TransporterFacade::get_auto_reconnect() const {
  return theClusterMgr->m_auto_reconnect;
}

void TransporterFacade::ext_set_max_api_reg_req_interval(Uint32 interval) {
  theClusterMgr->set_max_api_reg_req_interval(interval);
}

ndb_sockaddr TransporterFacade::ext_get_connect_address(NodeId nodeId) {
  return theTransporterRegistry->get_connect_address_node(nodeId);
}

bool TransporterFacade::ext_isConnected(NodeId aNodeId) {
  bool val;
  theClusterMgr->lock();
  val = theClusterMgr->theNodes[aNodeId].is_connected();
  theClusterMgr->unlock();
  return val;
}

void TransporterFacade::ext_doConnect(NodeId aNodeId) {
  theClusterMgr->lock();
  assert(theClusterMgr->theNodes[aNodeId].is_connected() == false);
  startConnecting(aNodeId);
  theClusterMgr->unlock();
}

bool TransporterFacade::setupWakeup() {
  /* Ask TransporterRegistry to setup wakeup sockets */
  bool rc;
  lock_poll_mutex();
  {
    dbg("setupWakeup on %p", this);
    rc = theTransporterRegistry->setup_wakeup_socket();
  }
  unlock_poll_mutex();
  return rc;
}

bool TransporterFacade::registerForWakeup(trp_client *_dozer) {
  /* Called with Transporter lock */
  /* In future use a DLList for dozers.
   * Ideally with some way to wake one rather than all
   * For now, we just have one/TransporterFacade
   */
  dbg2("register dozer = %p on  %p", _dozer, this);
  if (dozer != nullptr) return false;

  dozer = _dozer;
  return true;
}

bool TransporterFacade::unregisterForWakeup(trp_client *_dozer) {
  /* Called with Transporter lock */
  if (dozer != _dozer) return false;

  dbg2("unregister dozer = %p on %p", _dozer, this);
  dozer = nullptr;
  return true;
}

void TransporterFacade::requestWakeup() {
  /* Forward to TransporterRegistry
   * No need for locks, assuming only one client at a time will use
   */
  theTransporterRegistry->wakeup();
}

void TransporterFacade::reportWakeup() {
  /* Explicit wakeup callback
   * Called with Transporter Mutex held
   */
  /* Notify interested parties */
  if (dozer != nullptr) {
    dozer->trp_wakeup();
  };
}

#ifdef ERROR_INSERT

/* Test methods to consume sendbuffer */
static TFPage *consumed_sendbuff = nullptr;

void TransporterFacade::consume_sendbuffer(Uint32 bytes_remain) {
  if (consumed_sendbuff) {
    g_eventLogger->info("SendBuff already consumed, release first");
    return;
  }

  Uint64 tot_size = m_send_buffer.get_total_send_buffer_size();
  Uint64 used = m_send_buffer.get_total_used_send_buffer_size();
  Uint32 page_count = 0;

  while (tot_size - used > bytes_remain) {
    TFPage *p = m_send_buffer.try_alloc(1);

    if (p) {
      p->init();
      p->m_next = consumed_sendbuff;
      consumed_sendbuff = p;
      page_count++;
    } else {
      break;
    }
    used = m_send_buffer.get_total_used_send_buffer_size();
  }

  g_eventLogger->info("Consumed %u pages, remaining bytes : %llu", page_count,
                      m_send_buffer.get_total_send_buffer_size() -
                          m_send_buffer.get_total_used_send_buffer_size());
}

void TransporterFacade::release_consumed_sendbuffer() {
  if (!consumed_sendbuff) {
    g_eventLogger->info("No sendbuffer consumed");
    return;
  }

  m_send_buffer.release_list(consumed_sendbuff);

  consumed_sendbuff = nullptr;

  g_eventLogger->info("Remaining bytes : %llu",
                      m_send_buffer.get_total_send_buffer_size() -
                          m_send_buffer.get_total_used_send_buffer_size());
}

#endif
