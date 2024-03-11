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

#include "Cmvmi.hpp"
#include "util/require.h"

#include <NdbTick.h>
#include <kernel_types.h>
#include <portlib/NdbMem.h>
#include <signal.h>
#include <Configuration.hpp>
#include <NdbOut.hpp>
#include <cstring>
#include <util/ConfigValues.hpp>

#include <FastScheduler.hpp>
#include <SignalLoggerManager.hpp>
#include <TransporterRegistry.hpp>

#define DEBUG(x) \
  { ndbout << "CMVMI::" << x << endl; }

#include <signaldata/AllocMem.hpp>
#include <signaldata/DbinfoScan.hpp>
#include <signaldata/DumpStateOrd.hpp>
#include <signaldata/EventReport.hpp>
#include <signaldata/EventSubscribeReq.hpp>
#include <signaldata/GetConfig.hpp>
#include <signaldata/NodeStateSignalData.hpp>
#include <signaldata/SetLogLevelOrd.hpp>
#include <signaldata/StartOrd.hpp>
#include <signaldata/Sync.hpp>
#include <signaldata/TamperOrd.hpp>
#include <signaldata/TestOrd.hpp>

#ifdef ERROR_INSERT
#include <signaldata/FsOpenReq.hpp>
#endif

#include <EventLogger.hpp>
#include <TimeQueue.hpp>

#include <NdbSleep.h>
#include <SafeCounter.hpp>
#include <SectionReader.hpp>
#include <vm/WatchDog.hpp>

#include <DebuggerNames.hpp>

#define JAM_FILE_ID 380

#define ZREPORT_MEMORY_USAGE 1000

extern int simulate_error_during_shutdown;

#ifdef ERROR_INSERT
extern int simulate_error_during_error_reporting;
#endif

// Index pages used by ACC instances
Uint32 g_acc_pages_used[1 + MAX_NDBMT_LQH_WORKERS];

extern void mt_init_receiver_cache();
extern void mt_set_section_chunk_size();

Cmvmi::Cmvmi(Block_context &ctx)
    : SimulatedBlock(CMVMI, ctx), subscribers(subscriberPool) {
  BLOCK_CONSTRUCTOR(Cmvmi);

  Uint32 long_sig_buffer_size;
  const ndb_mgm_configuration_iterator *p =
      m_ctx.m_config.getOwnConfigIterator();
  ndbrequire(p != 0);

  ndb_mgm_get_int_parameter(p, CFG_DB_LONG_SIGNAL_BUFFER,
                            &long_sig_buffer_size);

  /* Ensure that aligned allocation will result in 64-bit
   * aligned offset for theData
   */
  static_assert((sizeof(SectionSegment) % 8) == 0);
  static_assert((offsetof(SectionSegment, theData) % 8) == 0);

  long_sig_buffer_size = long_sig_buffer_size / sizeof(SectionSegment);
  g_sectionSegmentPool.setSize(long_sig_buffer_size, true, true, true,
                               CFG_DB_LONG_SIGNAL_BUFFER);

  mt_init_receiver_cache();
  mt_set_section_chunk_size();

  // Add received signals
  addRecSignal(GSN_NDB_TAMPER, &Cmvmi::execNDB_TAMPER, true);
  addRecSignal(GSN_SET_LOGLEVELORD, &Cmvmi::execSET_LOGLEVELORD);
  addRecSignal(GSN_EVENT_REP, &Cmvmi::execEVENT_REP);
  addRecSignal(GSN_STTOR, &Cmvmi::execSTTOR);
  addRecSignal(GSN_READ_CONFIG_REQ, &Cmvmi::execREAD_CONFIG_REQ);
  addRecSignal(GSN_TEST_ORD, &Cmvmi::execTEST_ORD);

  addRecSignal(GSN_TAMPER_ORD, &Cmvmi::execTAMPER_ORD);
  addRecSignal(GSN_STOP_ORD, &Cmvmi::execSTOP_ORD);
  addRecSignal(GSN_START_ORD, &Cmvmi::execSTART_ORD);
  addRecSignal(GSN_EVENT_SUBSCRIBE_REQ, &Cmvmi::execEVENT_SUBSCRIBE_REQ);
  addRecSignal(GSN_CANCEL_SUBSCRIPTION_REQ,
               &Cmvmi::execCANCEL_SUBSCRIPTION_REQ);

  addRecSignal(GSN_DUMP_STATE_ORD, &Cmvmi::execDUMP_STATE_ORD);
  addRecSignal(GSN_TC_COMMIT_ACK, &Cmvmi::execTC_COMMIT_ACK);

  addRecSignal(GSN_TESTSIG, &Cmvmi::execTESTSIG);

  addRecSignal(GSN_CONTINUEB, &Cmvmi::execCONTINUEB);
  addRecSignal(GSN_DBINFO_SCANREQ, &Cmvmi::execDBINFO_SCANREQ);

  addRecSignal(GSN_SYNC_REQ, &Cmvmi::execSYNC_REQ, true);
  addRecSignal(GSN_SYNC_REF, &Cmvmi::execSYNC_REF);
  addRecSignal(GSN_SYNC_CONF, &Cmvmi::execSYNC_CONF);

  addRecSignal(GSN_ALLOC_MEM_REF, &Cmvmi::execALLOC_MEM_REF);
  addRecSignal(GSN_ALLOC_MEM_CONF, &Cmvmi::execALLOC_MEM_CONF);

  addRecSignal(GSN_GET_CONFIG_REQ, &Cmvmi::execGET_CONFIG_REQ);

#ifdef ERROR_INSERT
  addRecSignal(GSN_FSOPENCONF, &Cmvmi::execFSOPENCONF);
  addRecSignal(GSN_FSCLOSECONF, &Cmvmi::execFSCLOSECONF);
#endif

  subscriberPool.setSize(5);
  c_syncReqPool.setSize(5);

  const ndb_mgm_configuration_iterator *db =
      m_ctx.m_config.getOwnConfigIterator();
  for (unsigned j = 0; j < LogLevel::LOGLEVEL_CATEGORIES; j++) {
    Uint32 logLevel;
    if (!ndb_mgm_get_int_parameter(db, CFG_MIN_LOGLEVEL + j, &logLevel)) {
      clogLevel.setLogLevel((LogLevel::EventCategory)j, logLevel);
    }
  }

  ndb_mgm_configuration_iterator *iter =
      m_ctx.m_config.getClusterConfigIterator();
  for (ndb_mgm_first(iter); ndb_mgm_valid(iter); ndb_mgm_next(iter)) {
    jam();
    Uint32 nodeId;
    Uint32 nodeType;

    ndbrequire(!ndb_mgm_get_int_parameter(iter, CFG_NODE_ID, &nodeId));
    ndbrequire(
        !ndb_mgm_get_int_parameter(iter, CFG_TYPE_OF_SECTION, &nodeType));

    switch (nodeType) {
      case NodeInfo::DB:
        c_dbNodes.set(nodeId);
        break;
      case NodeInfo::API:
      case NodeInfo::MGM:
        break;
      default:
        ndbabort();
    }
    setNodeInfo(nodeId).m_type = nodeType;
  }

  setNodeInfo(getOwnNodeId()).m_connected = true;
  setNodeInfo(getOwnNodeId()).m_version = ndbGetOwnVersion();
  setNodeInfo(getOwnNodeId()).m_mysql_version = NDB_MYSQL_VERSION_D;

  c_memusage_report_frequency = 0;

  m_start_time = NdbTick_getCurrentTicks();

  std::memset(g_acc_pages_used, 0, sizeof(g_acc_pages_used));
}

Cmvmi::~Cmvmi() { m_shared_page_pool.clear(); }

void Cmvmi::execNDB_TAMPER(Signal *signal) {
  jamEntry();

  SimulatedBlock::execNDB_TAMPER(signal);

  if (ERROR_INSERTED(9999)) {
    CRASH_INSERTION(9999);
  }

  if (ERROR_INSERTED(9998)) {
    while (true) NdbSleep_SecSleep(1);
  }

  if (ERROR_INSERTED(9997)) {
    ndbabort();
  }

#ifndef _WIN32
  if (ERROR_INSERTED(9996)) {
    simulate_error_during_shutdown = SIGSEGV;
    ndbabort();
  }

  if (ERROR_INSERTED(9995)) {
    simulate_error_during_shutdown = SIGSEGV;
    kill(getpid(), SIGABRT);
  }

#endif

}  // execNDB_TAMPER()

static Uint32 blocks[] = {
    QMGR_REF,  NDBCNTR_REF, DBTC_REF,  DBDIH_REF,  DBDICT_REF, DBLQH_REF,
    DBTUP_REF, DBACC_REF,   NDBFS_REF, BACKUP_REF, DBUTIL_REF, SUMA_REF,
    TRIX_REF,  DBTUX_REF,   LGMAN_REF, TSMAN_REF,  PGMAN_REF,  DBINFO_REF,
    DBSPJ_REF, TRPMAN_REF,  0};

void Cmvmi::execSYNC_REQ(Signal *signal) {
  jamEntry();
  SyncReq req = *CAST_CONSTPTR(SyncReq, signal->getDataPtr());
  Ptr<SyncRecord> ptr;
  if (!c_syncReqPool.seize(ptr)) {
    jam();
    SyncRecord tmp;
    ptr.p = &tmp;
    tmp.m_senderRef = req.senderRef;
    tmp.m_senderData = req.senderData;
    tmp.m_prio = req.prio;
    tmp.m_error = SyncRef::SR_OUT_OF_MEMORY;
    sendSYNC_REP(signal, ptr);
    return;
  }

  ptr.p->m_senderRef = req.senderRef;
  ptr.p->m_senderData = req.senderData;
  ptr.p->m_prio = req.prio;
  ptr.p->m_error = 0;

  SyncReq *out = CAST_PTR(SyncReq, signal->getDataPtrSend());
  out->senderRef = reference();
  out->senderData = ptr.i;
  out->prio = ptr.p->m_prio;
  Uint32 i = 0;
  for (i = 0; blocks[i] != 0; i++) {
    sendSignal(blocks[i], GSN_SYNC_REQ, signal, SyncReq::SignalLength,
               JobBufferLevel(ptr.p->m_prio));
  }
  ptr.p->m_cnt = i;
}

void Cmvmi::execSYNC_CONF(Signal *signal) {
  jamEntry();
  SyncConf conf = *CAST_CONSTPTR(SyncConf, signal->getDataPtr());

  Ptr<SyncRecord> ptr;
  ndbrequire(c_syncReqPool.getPtr(ptr, conf.senderData));
  ndbrequire(ptr.p->m_cnt > 0);
  ptr.p->m_cnt--;
  if (ptr.p->m_cnt == 0) {
    jam();

    sendSYNC_REP(signal, ptr);
    c_syncReqPool.release(ptr);
  }
}

void Cmvmi::execSYNC_REF(Signal *signal) {
  jamEntry();
  SyncRef ref = *CAST_CONSTPTR(SyncRef, signal->getDataPtr());

  Ptr<SyncRecord> ptr;
  ndbrequire(c_syncReqPool.getPtr(ptr, ref.senderData));
  ndbrequire(ptr.p->m_cnt > 0);
  ptr.p->m_cnt--;

  if (ptr.p->m_error == 0) {
    jam();
    ptr.p->m_error = ref.errorCode;
  }

  if (ptr.p->m_cnt == 0) {
    jam();

    sendSYNC_REP(signal, ptr);
    c_syncReqPool.release(ptr);
  }
}

void Cmvmi::sendSYNC_REP(Signal *signal, Ptr<SyncRecord> ptr) {
  if (ptr.p->m_error == 0) {
    jam();
    SyncConf *conf = CAST_PTR(SyncConf, signal->getDataPtrSend());
    conf->senderRef = reference();
    conf->senderData = ptr.p->m_senderData;
    sendSignal(ptr.p->m_senderRef, GSN_SYNC_CONF, signal,
               SyncConf::SignalLength, JobBufferLevel(ptr.p->m_prio));
  } else {
    jam();
    SyncRef *ref = CAST_PTR(SyncRef, signal->getDataPtrSend());
    ref->senderRef = reference();
    ref->senderData = ptr.p->m_senderData;
    ref->errorCode = ptr.p->m_error;
    sendSignal(ptr.p->m_senderRef, GSN_SYNC_REF, signal, SyncRef::SignalLength,
               JobBufferLevel(ptr.p->m_prio));
  }
}

void Cmvmi::execSET_LOGLEVELORD(Signal *signal) {
  SetLogLevelOrd *const llOrd = (SetLogLevelOrd *)&signal->theData[0];
  LogLevel::EventCategory category;
  Uint32 level;
  jamEntry();

  ndbrequire(llOrd->noOfEntries <= LogLevel::LOGLEVEL_CATEGORIES);

  for (unsigned int i = 0; i < llOrd->noOfEntries; i++) {
    category = (LogLevel::EventCategory)(llOrd->theData[i] >> 16);
    level = llOrd->theData[i] & 0xFFFF;

    clogLevel.setLogLevel(category, level);
  }
}  // execSET_LOGLEVELORD()

struct SavedEvent {
  Uint32 m_len;
  Uint32 m_seq;
  Uint32 m_time;
  Uint32 m_data[MAX_EVENT_REP_SIZE_WORDS];

  static constexpr Uint32 HeaderLength = 3;
};

#define SAVE_BUFFER_CNT (CFG_MAX_LOGLEVEL - CFG_MIN_LOGLEVEL + 1)

Uint32 m_saved_event_sequence = 0;

static struct SavedEventBuffer {
  SavedEventBuffer() {
    m_read_pos = m_write_pos = 0;
    m_buffer_len = 0;
    m_data = 0;
  }

  void init(Uint32 bytes) {
    if (bytes < 128) {
      return;  // min size...unless set to 0
    }
    Uint32 words = bytes / 4;
    m_data = new Uint32[words];
    if (m_data) {
      m_buffer_len = words;
    }
  }

  Uint16 m_write_pos;
  Uint16 m_read_pos;
  Uint32 m_buffer_len;
  Uint32 *m_data;

  void alloc(Uint32 len);
  void purge();
  void save(const Uint32 *theData, Uint32 len);

  Uint32 num_free() const;

  Uint32 m_scan_pos;
  int startScan();
  int scan(SavedEvent *dst, Uint32 filter[]);

  /**
   * Get sequence number of entry located at current scan pos
   */
  Uint32 getScanPosSeq() const;
} m_saved_event_buffer[SAVE_BUFFER_CNT + /* add unknown here */ 1];

void SavedEventBuffer::alloc(Uint32 len) {
  assert(m_buffer_len > 0);

  while (num_free() <= len) purge();
}

Uint32 SavedEventBuffer::num_free() const {
  if (m_write_pos == m_read_pos)
    return m_buffer_len;
  else if (m_write_pos > m_read_pos)
    return (m_buffer_len - m_write_pos) + m_read_pos;
  else
    return m_read_pos - m_write_pos;
}

void SavedEventBuffer::purge() {
  const Uint32 *ptr = m_data + m_read_pos;
  /* First word of SavedEvent is m_len.
   * One can not safely cast ptr to SavedEvent pointer since it may wrap if at
   * end of buffer.
   */
  constexpr Uint32 len_off = 0;
  static_assert(offsetof(SavedEvent, m_len) == len_off * sizeof(Uint32));
  const Uint32 data_len = ptr[len_off];
  Uint32 len = SavedEvent::HeaderLength + data_len;
  m_read_pos = (m_read_pos + len) % m_buffer_len;
}

void SavedEventBuffer::save(const Uint32 *theData, Uint32 len) {
  if (m_buffer_len == 0) return;

  Uint32 total = len + SavedEvent::HeaderLength;
  alloc(total);

  SavedEvent s;
  s.m_len = len;  // size of SavedEvent
  s.m_seq = m_saved_event_sequence++;
  s.m_time = (Uint32)time(0);
  const Uint32 *src = (const Uint32 *)&s;
  Uint32 *dst = m_data + m_write_pos;

  Uint32 remain = m_buffer_len - m_write_pos;
  if (remain >= total) {
    memcpy(dst, src, 4 * SavedEvent::HeaderLength);
    memcpy(dst + SavedEvent::HeaderLength, theData, 4 * len);
  } else {
    memcpy(s.m_data, theData, 4 * len);
    memcpy(dst, src, 4 * remain);
    memcpy(m_data, src + remain, 4 * (total - remain));
  }
  m_write_pos = (m_write_pos + total) % m_buffer_len;
}

int SavedEventBuffer::startScan() {
  if (m_read_pos == m_write_pos) {
    return 1;
  }
  m_scan_pos = m_read_pos;
  return 0;
}

int SavedEventBuffer::scan(SavedEvent *_dst, Uint32 filter[]) {
  assert(m_scan_pos != m_write_pos);
  Uint32 *dst = (Uint32 *)_dst;
  const Uint32 *ptr = m_data + m_scan_pos;
  /* First word of SavedEvent is m_len.
   * One can not safely cast ptr to SavedEvent pointer since it may wrap if at
   * end of buffer.
   */
  constexpr Uint32 len_off = 0;
  static_assert(offsetof(SavedEvent, m_len) == len_off * sizeof(Uint32));
  const Uint32 data_len = ptr[len_off];
  require(data_len <= MAX_EVENT_REP_SIZE_WORDS);
  Uint32 total = data_len + SavedEvent::HeaderLength;
  if (m_scan_pos + total <= m_buffer_len) {
    memcpy(dst, ptr, 4 * total);
  } else {
    Uint32 remain = m_buffer_len - m_scan_pos;
    memcpy(dst, ptr, 4 * remain);
    memcpy(dst + remain, m_data, 4 * (total - remain));
  }
  m_scan_pos = (m_scan_pos + total) % m_buffer_len;

  if (m_scan_pos == m_write_pos) {
    return 1;
  }
  return 0;
}

Uint32 SavedEventBuffer::getScanPosSeq() const {
  assert(m_scan_pos != m_write_pos);
  const Uint32 *ptr = m_data + m_scan_pos;
  /* First word of SavedEvent is m_len.
   * Second word of SavedEvent is m_seq.
   * One can not safely cast ptr to SavedEvent pointer since it may wrap if at
   * end of buffer.
   */
  static_assert(offsetof(SavedEvent, m_seq) % sizeof(Uint32) == 0);
  constexpr Uint32 seq_off = offsetof(SavedEvent, m_seq) / sizeof(Uint32);
  if (m_scan_pos + seq_off < m_buffer_len) {
    return ptr[seq_off];
  }
  const Uint32 wrap_seq_off = m_scan_pos + seq_off - m_buffer_len;
  return m_data[wrap_seq_off];
}

void Cmvmi::execEVENT_REP(Signal *signal) {
  //-----------------------------------------------------------------------
  // This message is sent to report any types of events in NDB.
  // Based on the log level they will be either ignored or
  // reported. Currently they are printed, but they will be
  // transferred to the management server for further distribution
  // to the graphical management interface.
  //-----------------------------------------------------------------------
  EventReport *const eventReport = (EventReport *)&signal->theData[0];
  Ndb_logevent_type eventType = eventReport->getEventType();
  Uint32 nodeId = eventReport->getNodeId();
  if (nodeId == 0) {
    nodeId = refToNode(signal->getSendersBlockRef());

    if (nodeId == 0) {
      /* Event reporter supplied no node id,
       * assume it was local
       */
      nodeId = getOwnNodeId();
    }

    eventReport->setNodeId(nodeId);
  }

  jamEntry();

  Uint32 num_sections = signal->getNoOfSections();
  SectionHandle handle(this, signal);
  SegmentedSectionPtr segptr;
  if (num_sections > 0) {
    ndbrequire(num_sections == 1);
    ndbrequire(handle.getSection(segptr, 0));
  }
  /**
   * If entry is not found
   */
  Uint32 threshold;
  LogLevel::EventCategory eventCategory;
  Logger::LoggerLevel severity;
  EventLoggerBase::EventTextFunction textF;
  if (EventLoggerBase::event_lookup(eventType, eventCategory, threshold,
                                    severity, textF)) {
    if (num_sections > 0) {
      releaseSections(handle);
    }
    return;
  }

  Uint32 sig_length = signal->length();
  SubscriberPtr subptr;
  for (subscribers.first(subptr); subptr.i != RNIL; subscribers.next(subptr)) {
    jam();
    if (subptr.p->logLevel.getLogLevel(eventCategory) < threshold) {
      jam();
      continue;
    }
    if (num_sections > 0) {
      /**
       * Send only to nodes that are upgraded to a version that can handle
       * signal sections in EVENT_REP.
       * Not possible to send the signal to older versions that don't support
       * sections in EVENT_REP since signal is too small for that.
       */
      Uint32 version = getNodeInfo(refToNode(subptr.p->blockRef)).m_version;
      if (ndbd_send_node_bitmask_in_section(version)) {
        sendSignalNoRelease(subptr.p->blockRef, GSN_EVENT_REP, signal,
                            sig_length, JBB, &handle);
      } else {
        /**
         * MGM server isn't ready to receive a long signal, we need to handle
         * it for at least infoEvent's and WarningEvent's, other reports we
         * will simply throw away. The upgrade order is supposed to start
         * with upgrades of MGM server, so should normally not happen. But
         * still good to not mismanage it completely.
         */
        if (eventType == NDB_LE_WarningEvent || eventType == NDB_LE_InfoEvent) {
          copy(&signal->theData[1], segptr);
          Uint32 sz = segptr.sz > 24 ? 24 : segptr.sz;
          sendSignal(subptr.p->blockRef, GSN_EVENT_REP, signal, sz, JBB);
        }
      }
    } else {
      sendSignal(subptr.p->blockRef, GSN_EVENT_REP, signal, sig_length, JBB);
    }
  }

  Uint32 buf[MAX_EVENT_REP_SIZE_WORDS];
  Uint32 *data = signal->theData;
  const Uint32 sz = (num_sections > 0) ? segptr.sz : signal->getLength();
  ndbrequire(sz <= MAX_EVENT_REP_SIZE_WORDS);
  if (num_sections > 0) {
    copy(&buf[0], segptr);
    data = &buf[0];
  }

  Uint32 saveBuf = Uint32(eventCategory);
  if (saveBuf >= NDB_ARRAY_SIZE(m_saved_event_buffer) - 1)
    saveBuf = NDB_ARRAY_SIZE(m_saved_event_buffer) - 1;
  m_saved_event_buffer[saveBuf].save(data, sz);

  if (clogLevel.getLogLevel(eventCategory) < threshold) {
    if (num_sections > 0) {
      releaseSections(handle);
    }
    return;
  }

  // Print the event info
  g_eventLogger->log(eventReport->getEventType(), data, sz, 0, 0);

  if (num_sections > 0) {
    releaseSections(handle);
  }
  return;
}  // execEVENT_REP()

void Cmvmi::execEVENT_SUBSCRIBE_REQ(Signal *signal) {
  EventSubscribeReq *subReq = (EventSubscribeReq *)&signal->theData[0];
  Uint32 senderRef = signal->getSendersBlockRef();
  SubscriberPtr ptr;
  jamEntry();
  DBUG_ENTER("Cmvmi::execEVENT_SUBSCRIBE_REQ");

  /**
   * Search for subcription
   */
  for (subscribers.first(ptr); ptr.i != RNIL; subscribers.next(ptr)) {
    if (ptr.p->blockRef == subReq->blockRef) break;
  }

  if (ptr.i == RNIL) {
    /**
     * Create a new one
     */
    if (subscribers.seizeFirst(ptr) == false) {
      sendSignal(senderRef, GSN_EVENT_SUBSCRIBE_REF, signal, 1, JBB);
      return;
    }
    ptr.p->logLevel.clear();
    ptr.p->blockRef = subReq->blockRef;
  }

  if (subReq->noOfEntries == 0) {
    /**
     * Cancel subscription
     */
    subscribers.release(ptr.i);
  } else {
    /**
     * Update subscription
     */
    LogLevel::EventCategory category;
    Uint32 level = 0;
    ndbrequire(subReq->noOfEntries <= LogLevel::LOGLEVEL_CATEGORIES);
    for (Uint32 i = 0; i < subReq->noOfEntries; i++) {
      category = (LogLevel::EventCategory)(subReq->theData[i] >> 16);
      level = subReq->theData[i] & 0xFFFF;
      ptr.p->logLevel.setLogLevel(category, level);
      DBUG_PRINT("info",
                 ("entry %d: level=%d, category= %d", i, level, category));
    }
  }

  signal->theData[0] = ptr.i;
  sendSignal(senderRef, GSN_EVENT_SUBSCRIBE_CONF, signal, 1, JBB);
  DBUG_VOID_RETURN;
}

void Cmvmi::execCANCEL_SUBSCRIPTION_REQ(Signal *signal) {
  SubscriberPtr ptr;
  NodeId nodeId = signal->theData[0];

  subscribers.first(ptr);
  while (ptr.i != RNIL) {
    Uint32 i = ptr.i;
    BlockReference blockRef = ptr.p->blockRef;

    subscribers.next(ptr);

    if (refToNode(blockRef) == nodeId) {
      subscribers.release(i);
    }
  }
}

void Cmvmi::sendSTTORRY(Signal *signal) {
  jam();
  signal->theData[3] = 1;
  signal->theData[4] = 3;
  signal->theData[5] = 8;
  signal->theData[6] = 255;
  sendSignal(NDBCNTR_REF, GSN_STTORRY, signal, 7, JBB);
}  // Cmvmi::sendSTTORRY

static Uint32 f_read_config_ref = 0;
static Uint32 f_read_config_data = 0;

void Cmvmi::execREAD_CONFIG_REQ(Signal *signal) {
  jamEntry();

  const ReadConfigReq *req = (ReadConfigReq *)signal->getDataPtr();

  Uint32 ref = req->senderRef;
  Uint32 senderData = req->senderData;

  const ndb_mgm_configuration_iterator *p =
      m_ctx.m_config.getOwnConfigIterator();
  ndbrequire(p != 0);

  {
    void *ptr = m_ctx.m_mm.get_memroot();
    m_shared_page_pool.set((GlobalPage *)ptr, ~0);
  }

  Uint32 min_eventlog = (2 * MAX_EVENT_REP_SIZE_WORDS * 4) + 8;
  Uint32 eventlog = 8192;
  ndb_mgm_get_int_parameter(p, CFG_DB_EVENTLOG_BUFFER_SIZE, &eventlog);
  {
    Uint32 cnt = NDB_ARRAY_SIZE(m_saved_event_buffer);
    Uint32 split = (eventlog + (cnt / 2)) / cnt;
    for (Uint32 i = 0; i < cnt; i++) {
      if (split < min_eventlog) split = min_eventlog;
      m_saved_event_buffer[i].init(split);
    }
  }
  c_memusage_report_frequency = 0;
  ndb_mgm_get_int_parameter(p, CFG_DB_MEMREPORT_FREQUENCY,
                            &c_memusage_report_frequency);

  Uint32 late_alloc = 1;
  ndb_mgm_get_int_parameter(p, CFG_DB_LATE_ALLOC, &late_alloc);
  if (late_alloc) {
    jam();
    f_read_config_ref = ref;
    f_read_config_data = senderData;

    AllocMemReq *req = CAST_PTR(AllocMemReq, signal->getDataPtrSend());
    req->senderData = 0;
    req->senderRef = reference();
    req->requestInfo = AllocMemReq::RT_MAP;
    if (m_ctx.m_config.lockPagesInMainMemory()) {
      req->requestInfo |= AllocMemReq::RT_MEMLOCK;
    }

    req->bytes_hi = 0;
    req->bytes_lo = 0;
    sendSignal(NDBFS_REF, GSN_ALLOC_MEM_REQ, signal, AllocMemReq::SignalLength,
               JBB);

    /**
     * Assume this takes time...
     *   Set sp0 complete (even though it hasn't) but it makes
     *   ndb_mgm -e "show" show starting instead of not-started
     */
    {
      NodeStateRep *rep = CAST_PTR(NodeStateRep, signal->getDataPtrSend());
      NodeState newState(NodeState::SL_STARTING, 0, NodeState::ST_ILLEGAL_TYPE);
      rep->nodeState = newState;
      rep->nodeState.masterNodeId = 0;
      rep->nodeState.setNodeGroup(0);
      sendSignal(QMGR_REF, GSN_NODE_STATE_REP, signal,
                 NodeStateRep::SignalLength, JBB);
    }
    return;
  }

  init_global_page_pool();

  ReadConfigConf *conf = (ReadConfigConf *)signal->getDataPtrSend();
  conf->senderRef = reference();
  conf->senderData = senderData;
  sendSignal(ref, GSN_READ_CONFIG_CONF, signal, ReadConfigConf::SignalLength,
             JBB);
}

void Cmvmi::init_global_page_pool() {
  /**
   * This subroutine takes page from m_shared_page_pool and
   *   moves them into m_global_page_pool
   *   (that is currently used by pgman(dbtup) and restore
   */
  void *ptr = m_ctx.m_mm.get_memroot();
  m_global_page_pool.set((GlobalPage *)ptr, ~0);

  Resource_limit rl;
  ndbrequire(m_ctx.m_mm.get_resource_limit(RG_DISK_PAGE_BUFFER, rl));
  while (rl.m_max) {
    Uint32 ptrI;
    Uint32 cnt = rl.m_max;
    m_ctx.m_mm.alloc_pages(RG_DISK_PAGE_BUFFER, &ptrI, &cnt, 1,
                           Ndbd_mem_manager::NDB_ZONE_LE_30);
    ndbrequire(cnt);
    for (Uint32 i = 0; i < cnt; i++) {
      Ptr<GlobalPage> pagePtr;
      ndbrequire(m_shared_page_pool.getPtr(pagePtr, ptrI + i));
      m_global_page_pool.release(pagePtr);
    }
    rl.m_max -= cnt;
  }
}

void Cmvmi::execSTTOR(Signal *signal) {
  Uint32 theStartPhase = signal->theData[1];

  jamEntry();
  if (theStartPhase == 1) {
    jam();

    if (m_ctx.m_config.lockPagesInMainMemory()) {
      jam();
      /**
       * Notify watchdog that we're locking memory...
       *   which can be equally "heavy" as allocating it
       */
      refresh_watch_dog(9);
      int res = NdbMem_MemLockAll(1);
      if (res != 0) {
        char buf[100];
        BaseString::snprintf(buf, sizeof(buf),
                             "Failed to memlock pages, error: %d (%s)", errno,
                             strerror(errno));
        g_eventLogger->warning("%s", buf);
        warningEvent("%s", buf);
      } else {
        g_eventLogger->info("Using locked memory");
      }
    }

    /**
     * Install "normal" watchdog value
     */
    {
      Uint32 db_watchdog_interval = 0;
      const ndb_mgm_configuration_iterator *p =
          m_ctx.m_config.getOwnConfigIterator();
      ndb_mgm_get_int_parameter(p, CFG_DB_WATCHDOG_INTERVAL,
                                &db_watchdog_interval);
      ndbrequire(db_watchdog_interval);
      update_watch_dog_timer(db_watchdog_interval);
      Uint32 kill_val = 0;
      ndb_mgm_get_int_parameter(p, CFG_DB_WATCHDOG_IMMEDIATE_KILL, &kill_val);
      globalEmulatorData.theWatchDog->setKillSwitch((bool)kill_val);
    }

    /**
     * Start auto-mem reporting
     */
    signal->theData[0] = ZREPORT_MEMORY_USAGE;
    signal->theData[1] = 0;
    signal->theData[2] = 0;
    signal->theData[3] = 0;
    signal->theData[4] = 0;
    execCONTINUEB(signal);

    sendSTTORRY(signal);
    return;
  } else if (theStartPhase == 3) {
    jam();
    globalData.activateSendPacked = 1;
    sendSTTORRY(signal);
  } else if (theStartPhase == 8) {
    const ndb_mgm_configuration_iterator *p =
        m_ctx.m_config.getOwnConfigIterator();
    ndbrequire(p != 0);

    Uint32 free_pct = 5;
    ndb_mgm_get_int_parameter(p, CFG_DB_FREE_PCT, &free_pct);
    m_ctx.m_mm.init_resource_spare(RG_DATAMEM, free_pct);

    globalData.theStartLevel = NodeState::SL_STARTED;
    sendSTTORRY(signal);
  }
}

#ifdef VM_TRACE
void modifySignalLogger(bool allBlocks, BlockNumber bno, TestOrd::Command cmd,
                        TestOrd::SignalLoggerSpecification spec) {
  SignalLoggerManager::LogMode logMode;

  /**
   * Mapping between SignalLoggerManager::LogMode and
   *                 TestOrd::SignalLoggerSpecification
   */
  switch (spec) {
    case TestOrd::InputSignals:
      logMode = SignalLoggerManager::LogIn;
      break;
    case TestOrd::OutputSignals:
      logMode = SignalLoggerManager::LogOut;
      break;
    case TestOrd::InputOutputSignals:
      logMode = SignalLoggerManager::LogInOut;
      break;
    default:
      return;
      break;
  }

  switch (cmd) {
    case TestOrd::On:
      globalSignalLoggers.logOn(allBlocks, bno, logMode);
      break;
    case TestOrd::Off:
      globalSignalLoggers.logOff(allBlocks, bno, logMode);
      break;
    case TestOrd::Toggle:
      globalSignalLoggers.logToggle(allBlocks, bno, logMode);
      break;
    case TestOrd::KeepUnchanged:
      // Do nothing
      break;
  }
  globalSignalLoggers.flushSignalLog();
}
#endif

void Cmvmi::execTEST_ORD(Signal *signal) {
  jamEntry();

#ifdef VM_TRACE
  TestOrd *const testOrd = (TestOrd *)&signal->theData[0];

  TestOrd::Command cmd;

  {
    /**
     * Process Trace command
     */
    TestOrd::TraceSpecification traceSpec;

    testOrd->getTraceCommand(cmd, traceSpec);
    unsigned long traceVal = traceSpec;
    unsigned long currentTraceVal = globalSignalLoggers.getTrace();
    switch (cmd) {
      case TestOrd::On:
        currentTraceVal |= traceVal;
        break;
      case TestOrd::Off:
        currentTraceVal &= (~traceVal);
        break;
      case TestOrd::Toggle:
        currentTraceVal ^= traceVal;
        break;
      case TestOrd::KeepUnchanged:
        // Do nothing
        break;
    }
    globalSignalLoggers.setTrace(currentTraceVal);
  }

  {
    /**
     * Process Log command
     */
    TestOrd::SignalLoggerSpecification logSpec;
    BlockNumber bno;
    unsigned int loggers = testOrd->getNoOfSignalLoggerCommands();

    if (loggers == (unsigned)~0) {  // Apply command to all blocks
      testOrd->getSignalLoggerCommand(0, bno, cmd, logSpec);
      modifySignalLogger(true, bno, cmd, logSpec);
    } else {
      for (unsigned int i = 0; i < loggers; i++) {
        testOrd->getSignalLoggerCommand(i, bno, cmd, logSpec);
        modifySignalLogger(false, bno, cmd, logSpec);
      }
    }
  }

  {
    /**
     * Process test command
     */
    testOrd->getTestCommand(cmd);
    switch (cmd) {
      case TestOrd::On: {
        SET_GLOBAL_TEST_ON;
      } break;
      case TestOrd::Off: {
        SET_GLOBAL_TEST_OFF;
      } break;
      case TestOrd::Toggle: {
        TOGGLE_GLOBAL_TEST_FLAG;
      } break;
      case TestOrd::KeepUnchanged:
        // Do nothing
        break;
    }
    globalSignalLoggers.flushSignalLog();
  }

#endif
}

void Cmvmi::execSTOP_ORD(Signal *signal) {
  jamEntry();
  globalData.theRestartFlag = perform_stop;
}  // execSTOP_ORD()

void Cmvmi::execSTART_ORD(Signal *signal) {
  StartOrd *const startOrd = (StartOrd *)&signal->theData[0];
  jamEntry();

  Uint32 tmp = startOrd->restartInfo;
  if (StopReq::getPerformRestart(tmp)) {
    jam();
    /**
     *
     */
    NdbRestartType type = NRT_Default;
    if (StopReq::getNoStart(tmp) && StopReq::getInitialStart(tmp))
      type = NRT_NoStart_InitialStart;
    if (StopReq::getNoStart(tmp) && !StopReq::getInitialStart(tmp))
      type = NRT_NoStart_Restart;
    if (!StopReq::getNoStart(tmp) && StopReq::getInitialStart(tmp))
      type = NRT_DoStart_InitialStart;
    if (!StopReq::getNoStart(tmp) && !StopReq::getInitialStart(tmp))
      type = NRT_DoStart_Restart;
    NdbShutdown(0, NST_Restart, type);
  }

  if (globalData.theRestartFlag == system_started) {
    jam();
    /**
     * START_ORD received when already started(ignored)
     */
    // ndbout << "START_ORD received when already started(ignored)" << endl;
    return;
  }

  if (globalData.theRestartFlag == perform_stop) {
    jam();
    /**
     * START_ORD received when stopping(ignored)
     */
    // ndbout << "START_ORD received when stopping(ignored)" << endl;
    return;
  }

  if (globalData.theStartLevel == NodeState::SL_NOTHING) {
    jam();

    for (unsigned int i = 1; i < MAX_NODES; i++) {
      if (getNodeInfo(i).m_type == NodeInfo::MGM) {
        jam();
        const TrpId trpId = globalTransporterRegistry.get_the_only_base_trp(i);
        if (trpId != 0) {
          globalTransporterRegistry.start_connecting(trpId);
        }
      }
    }
    g_eventLogger->info("First START_ORD executed to connect MGM servers");

    globalData.theStartLevel = NodeState::SL_CMVMI;
    sendSignal(QMGR_REF, GSN_START_ORD, signal, 1, JBA);
    return;
  }

  if (globalData.theStartLevel == NodeState::SL_CMVMI) {
    jam();
    globalData.theStartLevel = NodeState::SL_STARTING;
    globalData.theRestartFlag = system_started;
    /**
     * StartLevel 1
     *
     * Do Restart
     */
    if (signal->getSendersBlockRef() == 0) {
      jam();
      g_eventLogger->info("Received second START_ORD as part of normal start");
    } else {
      jam();
      g_eventLogger->info("Received second START_ORD from node %u",
                          refToNode(signal->getSendersBlockRef()));
    }
    // Disconnect all transporters as part of the system restart.
    // We need to ensure that we are starting up
    // without any connected transporters.
    for (unsigned int i = 1; i < MAX_NODES; i++) {
      if (i != getOwnNodeId() && getNodeInfo(i).m_type != NodeInfo::MGM) {
        const TrpId trpId = globalTransporterRegistry.get_the_only_base_trp(i);
        if (trpId != 0) {
          globalTransporterRegistry.start_disconnecting(trpId);
          globalTransporterRegistry.setIOState(trpId, HaltIO);
        }
      }
    }
    g_eventLogger->info("Disconnect all non-MGM servers");

    CRASH_INSERTION(9994);

    /**
     * Start running startphases
     */
    g_eventLogger->info("Start excuting the start phases");
    sendSignal(NDBCNTR_REF, GSN_START_ORD, signal, 1, JBA);
    return;
  }
}  // execSTART_ORD()

void Cmvmi::execTAMPER_ORD(Signal *signal) {
  jamEntry();
  // TODO We should maybe introduce a CONF and REF signal
  // to be able to indicate if we really introduced an error.
#ifdef ERROR_INSERT
  TamperOrd *const tamperOrd = (TamperOrd *)&signal->theData[0];
  Uint32 errNo = tamperOrd->errorNo;

  if (errNo <= 1) {
    jam();
    signal->theData[0] = errNo;
    for (Uint32 i = 0; blocks[i] != 0; i++) {
      sendSignal(blocks[i], GSN_NDB_TAMPER, signal, 1, JBB);
    }
    sendSignal(CMVMI_REF, GSN_NDB_TAMPER, signal, 1, JBB);
    return;
  }

  Uint32 tuserblockref = 0;
  if (errNo < 1000) {
    /*--------------------------------------------------------------------*/
    // Insert errors into QMGR.
    /*--------------------------------------------------------------------*/
    jam();
    tuserblockref = QMGR_REF;
  } else if (errNo < 2000) {
    /*--------------------------------------------------------------------*/
    // Insert errors into NDBCNTR.
    /*--------------------------------------------------------------------*/
    jam();
    tuserblockref = NDBCNTR_REF;
  } else if (errNo < 3000) {
    /*--------------------------------------------------------------------*/
    // Insert errors into NDBFS.
    /*--------------------------------------------------------------------*/
    jam();
    tuserblockref = NDBFS_REF;
  } else if (errNo < 4000) {
    /*--------------------------------------------------------------------*/
    // Insert errors into DBACC.
    /*--------------------------------------------------------------------*/
    jam();
    tuserblockref = DBACC_REF;
  } else if (errNo < 5000) {
    /*--------------------------------------------------------------------*/
    // Insert errors into DBTUP.
    /*--------------------------------------------------------------------*/
    jam();
    tuserblockref = DBTUP_REF;
  } else if (errNo < 6000) {
    /*---------------------------------------------------------------------*/
    // Insert errors into DBLQH.
    /*---------------------------------------------------------------------*/
    jam();
    tuserblockref = DBLQH_REF;
  } else if (errNo < 7000) {
    /*---------------------------------------------------------------------*/
    // Insert errors into DBDICT.
    /*---------------------------------------------------------------------*/
    jam();
    tuserblockref = DBDICT_REF;
  } else if (errNo < 8000) {
    /*---------------------------------------------------------------------*/
    // Insert errors into DBDIH.
    /*--------------------------------------------------------------------*/
    jam();
    tuserblockref = DBDIH_REF;
  } else if (errNo < 9000) {
    /*--------------------------------------------------------------------*/
    // Insert errors into DBTC.
    /*--------------------------------------------------------------------*/
    jam();
    tuserblockref = DBTC_REF;
  } else if (errNo < 9600) {
    /*--------------------------------------------------------------------*/
    // Insert errors into TRPMAN.
    /*--------------------------------------------------------------------*/
    jam();
    tuserblockref = TRPMAN_REF;
  } else if (errNo < 10000) {
    /*--------------------------------------------------------------------*/
    // Insert errors into CMVMI.
    /*--------------------------------------------------------------------*/
    jam();
    tuserblockref = CMVMI_REF;
  } else if (errNo < 11000) {
    jam();
    tuserblockref = BACKUP_REF;
  } else if (errNo < 12000) {
    jam();
    tuserblockref = PGMAN_REF;
  } else if (errNo < 13000) {
    jam();
    tuserblockref = DBTUX_REF;
  } else if (errNo < 14000) {
    jam();
    tuserblockref = SUMA_REF;
  } else if (errNo < 15000) {
    jam();
    tuserblockref = DBDICT_REF;
  } else if (errNo < 16000) {
    jam();
    tuserblockref = LGMAN_REF;
  } else if (errNo < 17000) {
    jam();
    tuserblockref = TSMAN_REF;
  } else if (errNo < 18000) {
    jam();
    tuserblockref = DBSPJ_REF;
  } else if (errNo < 19000) {
    jam();
    tuserblockref = TRIX_REF;
  } else if (errNo < 20000) {
    jam();
    tuserblockref = DBUTIL_REF;
  } else if (errNo < 30000) {
    /*--------------------------------------------------------------------*/
    // Ignore errors in the 20000-range.
    /*--------------------------------------------------------------------*/
    jam();
    return;
  } else if (errNo < 40000) {
    jam();
    /*--------------------------------------------------------------------*/
    // Redirect errors to master DIH in the 30000-range.
    /*--------------------------------------------------------------------*/

    /**
     * since CMVMI doesn't keep track of master,
     * send to local DIH
     */
    signal->theData[0] = 5;
    signal->theData[1] = errNo;
    signal->theData[2] = 0;
    sendSignal(DBDIH_REF, GSN_DIHNDBTAMPER, signal, 3, JBB);
    return;
  } else if (errNo < 50000) {
    jam();

    /**
     * since CMVMI doesn't keep track of master,
     * send to local DIH
     */
    signal->theData[0] = 5;
    signal->theData[1] = errNo;
    signal->theData[2] = 0;
    sendSignal(DBDIH_REF, GSN_DIHNDBTAMPER, signal, 3, JBB);
    return;
  }

  ndbassert(tuserblockref != 0);  // mapping missing ??
  if (tuserblockref != 0) {
    signal->theData[0] = errNo;
    sendSignal(tuserblockref, GSN_NDB_TAMPER, signal, signal->getLength(), JBB);
  }
#endif
}  // execTAMPER_ORD()

#ifdef VM_TRACE
class RefSignalTest {
 public:
  enum ErrorCode { OK = 0, NF_FakeErrorREF = 7 };
  Uint32 senderRef;
  Uint32 senderData;
  Uint32 errorCode;
};
#endif

#define check_block(block, val)              \
  (((val) >= DumpStateOrd::_##block##Min) && \
   ((val) <= DumpStateOrd::_##block##Max))

int cmp_event_buf(const void *ptr0, const void *ptr1) {
  Uint32 pos0 = *((Uint32 *)ptr0);
  Uint32 pos1 = *((Uint32 *)ptr1);

  Uint32 time0 = m_saved_event_buffer[pos0].getScanPosSeq();
  Uint32 time1 = m_saved_event_buffer[pos1].getScanPosSeq();
  return time0 - time1;
}

#if defined VM_TRACE || defined ERROR_INSERT
static Uint32 f_free_segments[256];
static Uint32 f_free_segment_pos = 0;
#endif

/**
 * TC_COMMIT_ACK is routed through CMVMI to ensure correct signal order
 * when sending DUMP_STATE_ORD to DBTC while TC_COMMIT_ACK is also
 * being in transit.
 */
void Cmvmi::execTC_COMMIT_ACK(Signal *signal) {
  jamEntry();
  BlockReference ref = signal->theData[2];
  sendSignal(ref, GSN_TC_COMMIT_ACK, signal, 2, JBB);
}

void Cmvmi::execDUMP_STATE_ORD(Signal *signal) {
  jamEntry();
  Uint32 val = signal->theData[0];
  if (val >= DumpStateOrd::OneBlockOnly) {
    if (check_block(Backup, val)) {
      sendSignal(BACKUP_REF, GSN_DUMP_STATE_ORD, signal, signal->length(), JBB);
    } else if (check_block(TC, val)) {
      sendSignal(DBTC_REF, GSN_DUMP_STATE_ORD, signal, signal->length(), JBB);
    } else if (check_block(LQH, val)) {
      sendSignal(DBLQH_REF, GSN_DUMP_STATE_ORD, signal, signal->length(), JBB);
    } else if (check_block(CMVMI, val)) {
      /**
       * Handle here since we are already in CMVMI, mostly used for
       * online config changes.
       */
      DumpStateOrd *const &dumpState = (DumpStateOrd *)&signal->theData[0];
      Uint32 arg = dumpState->args[0];
      Uint32 first_val = dumpState->args[1];
      if (signal->length() > 0) {
        if (val == DumpStateOrd::SetSchedulerResponsiveness) {
          if (signal->length() != 2) {
            g_eventLogger->info(
                "dump 103000 X, where X is between 0 and 10"
                " to set transactional priority");
          } else if (arg == DumpStateOrd::SetSchedulerResponsiveness) {
            if (first_val > 10) {
              g_eventLogger->info(
                  "Trying to set SchedulerResponsiveness outside 0-10");
            } else {
              g_eventLogger->info("Setting SchedulerResponsiveness to %u",
                                  first_val);
              Configuration *conf = globalEmulatorData.theConfiguration;
              conf->setSchedulerResponsiveness(first_val);
            }
          }
        } else if (val == DumpStateOrd::EnableEventLoggerDebug) {
          g_eventLogger->info("Enable Debug level in node log");
          g_eventLogger->enable(Logger::LL_DEBUG);
        } else if (val == DumpStateOrd::DisableEventLoggerDebug) {
          g_eventLogger->info("Disable Debug level in node log");
          g_eventLogger->disable(Logger::LL_DEBUG);
        } else if (val == DumpStateOrd::CmvmiRelayDumpStateOrd) {
          /* MGMD have no transporter to API nodes.  To be able to send a
           * dump command to an API node MGMD send the dump signal via a
           * data node using CmvmiRelay command.  The first argument is the
           * destination node, the rest is the dump command that should be
           * sent.
           *
           * args: dest-node dump-state-ord-code dump-state-ord-arg#1 ...
           */
          jam();
          const Uint32 length = signal->length();
          if (length < 3) {
            // Not enough words for sending DUMP_STATE_ORD
            jam();
            return;
          }
          const Uint32 node_id = signal->theData[1];
          const Uint32 ref = numberToRef(CMVMI, node_id);
          std::memmove(&signal->theData[0], &signal->theData[2],
                       (length - 2) * sizeof(Uint32));
          sendSignal(ref, GSN_DUMP_STATE_ORD, signal, length - 2, JBB);
        } else if (val == DumpStateOrd::CmvmiDummySignal) {
          /* Log in event logger that signal sent by dump command
           * CmvmiSendDummySignal is received.  Include information about
           * signal size and its sections and which node sent it.
           */
          jam();
          const Uint32 node_id = signal->theData[2];
          const Uint32 num_secs = signal->getNoOfSections();
          SectionHandle handle(this, signal);
          SegmentedSectionPtr ptr[3];
          for (Uint32 i = 0; i < num_secs; i++) {
            ndbrequire(handle.getSection(ptr[i], i));
          }
          char msg[24 * 4];
          snprintf(msg, sizeof(msg),
                   "Receiving CmvmiDummySignal"
                   " (size %u+%u+%u+%u+%u) from %u to %u.",
                   signal->getLength(), num_secs,
                   (num_secs > 0) ? ptr[0].sz : 0,
                   (num_secs > 1) ? ptr[1].sz : 0,
                   (num_secs > 2) ? ptr[2].sz : 0, node_id, getOwnNodeId());
          g_eventLogger->info("%s", msg);
          infoEvent("%s", msg);
          releaseSections(handle);
        } else if (val == DumpStateOrd::CmvmiSendDummySignal) {
          /* Send a CmvmiDummySignal to specified node with specified size and
           * sections.  This is used to verify that messages with certain
           * signal sizes and sections can be sent and received.
           *
           * The sending is also logged in event logger.  This log entry should
           * be matched with corresponding log when receiving the
           * CmvmiDummySignal dump command.  See preceding dump command above.
           *
           * args: rep-node dest-node padding frag-size
           *       #secs sec#1-len sec#2-len sec#3-len
           */
          jam();
          if (signal->length() < 5) {
            // Not enough words to send a dummy signal
            jam();
            return;
          }
          const Uint32 node_id = signal->theData[2];
          const Uint32 ref = (getNodeInfo(node_id).m_type == NodeInfo::DB)
                                 ? numberToRef(CMVMI, node_id)
                                 : numberToRef(API_CLUSTERMGR, node_id);
          const Uint32 fill_word = signal->theData[3];
          const Uint32 frag_size = signal->theData[4];
          if (frag_size != 0) {
            // Fragmented signals not supported yet.
            jam();
            return;
          }
          const Uint32 num_secs =
              (signal->length() > 5) ? signal->theData[5] : 0;
          if (num_secs > 3) {
            jam();
            return;
          }
          Uint32 tot_len = signal->length();
          LinearSectionPtr ptr[3];
          for (Uint32 i = 0; i < num_secs; i++) {
            const Uint32 sec_len = signal->theData[6 + i];
            ptr[i].sz = sec_len;
            tot_len += sec_len;
          }
          Uint32 *sec_alloc = NULL;
          Uint32 *sec_data = &signal->theData[signal->length()];
          if (tot_len > NDB_ARRAY_SIZE(signal->theData)) {
            sec_data = sec_alloc = new Uint32[tot_len];
          }
          signal->theData[0] = DumpStateOrd::CmvmiDummySignal;
          signal->theData[2] = getOwnNodeId();
          for (Uint32 i = 0; i < tot_len; i++) {
            sec_data[i] = fill_word;
          }
          for (Uint32 i = 0; i < num_secs; i++) {
            const Uint32 sec_len = signal->theData[6 + i];
            ptr[i].p = sec_data;
            sec_data += sec_len;
          }
          char msg[24 * 4];
          snprintf(msg, sizeof(msg),
                   "Sending CmvmiDummySignal"
                   " (size %u+%u+%u+%u+%u) from %u to %u.",
                   signal->getLength(), num_secs,
                   (num_secs > 0) ? ptr[0].sz : 0,
                   (num_secs > 1) ? ptr[1].sz : 0,
                   (num_secs > 2) ? ptr[2].sz : 0, getOwnNodeId(), node_id);
          infoEvent("%s", msg);
          sendSignal(ref, GSN_DUMP_STATE_ORD, signal, signal->length(), JBB,
                     ptr, num_secs);
          delete[] sec_alloc;
        }
      }
    } else if (check_block(THRMAN, val)) {
      sendSignal(THRMAN_REF, GSN_DUMP_STATE_ORD, signal, signal->length(), JBB);
    }
    return;
  }

  for (Uint32 i = 0; blocks[i] != 0; i++) {
    sendSignal(blocks[i], GSN_DUMP_STATE_ORD, signal, signal->length(), JBB);
  }

  /**
   *
   * Here I can dump CMVMI state if needed
   */
  DumpStateOrd *const &dumpState = (DumpStateOrd *)&signal->theData[0];
  Uint32 arg = dumpState->args[0];
  if (arg == DumpStateOrd::CmvmiDumpConnections) {
    for (TrpId trpId = 1; trpId <= globalTransporterRegistry.get_num_trps();
         trpId++) {
      const char *nodeTypeStr = "";
      const NodeId nodeId =
          globalTransporterRegistry.get_transporter_node_id(trpId);
      if (nodeId == 0) continue;

      switch (getNodeInfo(nodeId).m_type) {
        case NodeInfo::DB:
          nodeTypeStr = "DB";
          break;
        case NodeInfo::API:
          nodeTypeStr = "API";
          break;
        case NodeInfo::MGM:
          nodeTypeStr = "MGM";
          break;
        case NodeInfo::INVALID:
          nodeTypeStr = 0;
          break;
        default:
          nodeTypeStr = "<UNKNOWN>";
      }
      infoEvent("Connection to %u (%s), transporter %u is %s", nodeId,
                nodeTypeStr, trpId,
                globalTransporterRegistry.getPerformStateString(trpId));
    }
  }

  if (arg == DumpStateOrd::CmvmiDumpSubscriptions) {
    SubscriberPtr ptr;
    subscribers.first(ptr);
    g_eventLogger->info("List subscriptions:");
    while (ptr.i != RNIL) {
      g_eventLogger->info("Subscription: %u, nodeId: %u, ref: 0x%x", ptr.i,
                          refToNode(ptr.p->blockRef), ptr.p->blockRef);
      for (Uint32 i = 0; i < LogLevel::LOGLEVEL_CATEGORIES; i++) {
        Uint32 level = ptr.p->logLevel.getLogLevel((LogLevel::EventCategory)i);
        g_eventLogger->info("Category %u Level %u", i, level);
      }
      subscribers.next(ptr);
    }
  }

  if (arg == DumpStateOrd::CmvmiDumpLongSignalMemory) {
    infoEvent("Cmvmi: g_sectionSegmentPool size: %d free: %d",
              g_sectionSegmentPool.getSize(),
              g_sectionSegmentPool.getNoOfFree());
  }

  if (arg == DumpStateOrd::CmvmiLongSignalMemorySnapshotStart) {
#if defined VM_TRACE || defined ERROR_INSERT
    f_free_segment_pos = 0;
    std::memset(f_free_segments, 0, sizeof(f_free_segments));
#endif
  }

  if (arg == DumpStateOrd::CmvmiLongSignalMemorySnapshot) {
#if defined VM_TRACE || defined ERROR_INSERT
    if (f_free_segment_pos < NDB_ARRAY_SIZE(f_free_segments)) {
      f_free_segments[f_free_segment_pos++] =
          g_sectionSegmentPool.getNoOfFree();
    } else {
      g_eventLogger->warning(
          "CmvmiLongSignalMemorySnapshot IGNORED"
          ", exceeded the max %zu snapshots",
          NDB_ARRAY_SIZE(f_free_segments));
    }
#endif
  }

  if (arg == DumpStateOrd::CmvmiLongSignalMemorySnapshotCheck) {
#if defined VM_TRACE || defined ERROR_INSERT
    Uint32 start = 1;
    Uint32 stop = f_free_segment_pos;
    Uint32 cnt_dec = 0;
    Uint32 cnt_inc = 0;
    Uint32 cnt_same = 0;
    for (Uint32 i = start; i < stop; i++) {
      Uint32 prev = (i - 1);
      if (f_free_segments[prev] == f_free_segments[i])
        cnt_same++;
      else if (f_free_segments[prev] > f_free_segments[i])
        cnt_dec++;
      else
        cnt_inc++;
    }

    printf("snapshots: ");
    for (Uint32 i = 0; i < stop; i++) {
      printf("%u ", f_free_segments[i]);
    }
    printf("\n");
    printf("summary: #same: %u #increase: %u #decrease: %u\n", cnt_same,
           cnt_inc, cnt_dec);

    if (cnt_dec > 1) {
      /**
       * If memory decreased more than once...
       *   it must also increase at least once
       */
      ndbrequire(cnt_inc > 0);
    }

    if (cnt_dec == 1) {
      // it decreased once...this is ok
      return;
    }
    if (cnt_same >= (cnt_inc + cnt_dec)) {
      // it was frequently the same...this is ok
      return;
    }
    if (cnt_same + cnt_dec >= cnt_inc) {
      // it was frequently the same or less...this is ok
      return;
    }

    ndbabort();
#endif
  }

  if (arg == DumpStateOrd::CmvmiLongSignalMemorySnapshotCheck2) {
    g_eventLogger->info("CmvmiLongSignalMemorySnapshotCheck2");

#if defined VM_TRACE || defined ERROR_INSERT
    Uint32 orig_idx =
        (f_free_segment_pos - 1) % NDB_ARRAY_SIZE(f_free_segments);

    Uint32 poolsize = g_sectionSegmentPool.getSize();
    Uint32 orig_level = f_free_segments[orig_idx];
    Uint32 orig_used = poolsize - orig_level;
    Uint32 curr_level = g_sectionSegmentPool.getNoOfFree();
    Uint32 curr_used = poolsize - curr_level;

    g_eventLogger->info("  Total : %u", poolsize);
    g_eventLogger->info("  Orig free level : %u (%u pct)", orig_level,
                        orig_level * 100 / poolsize);
    g_eventLogger->info("  Curr free level : %u (%u pct)", curr_level,
                        curr_level * 100 / poolsize);
    g_eventLogger->info("  Orig in-use : %u (%u pct)", orig_used,
                        orig_used * 100 / poolsize);
    g_eventLogger->info("  Curr in-use : %u (%u pct)", curr_used,
                        curr_used * 100 / poolsize);

    if (curr_used > 2 * orig_used) {
      g_eventLogger->info(
          "  ERROR : in-use has grown by more than a factor of 2");
      ndbabort();
    } else {
      g_eventLogger->info("  Snapshot ok");
    }
#endif
  }

  if (arg == DumpStateOrd::CmvmiShowLongSignalOwnership) {
#ifdef NDB_DEBUG_RES_OWNERSHIP
    g_eventLogger->info("CMVMI dump LSB usage");
    Uint32 buffs = g_sectionSegmentPool.getSize();
    Uint32 *buffOwners = (Uint32 *)malloc(buffs * sizeof(Uint32));
    Uint64 *buffOwnersCount = (Uint64 *)malloc(buffs * sizeof(Uint64));

    std::memset(buffOwnersCount, 0, buffs * sizeof(Uint64));

    g_eventLogger->info("  Filling owners list");
    Uint32 zeroOwners = 0;
    lock_global_ssp();
    {
      /* Fill owners list */
      Ptr<SectionSegment> tmp;
      for (tmp.i = 0; tmp.i < buffs; tmp.i++) {
        g_sectionSegmentPool.getPtrIgnoreAlloc(tmp);
        buffOwners[tmp.i] = tmp.p->m_ownerRef;
        if (buffOwners[tmp.i] == 0) zeroOwners++;

        /* Expensive, ideally find a hacky way to iterate the freelist */
        if (!g_sectionSegmentPool.findId(tmp.i)) {
          buffOwners[tmp.i] = 0;
        }
      }
    }
    unlock_global_ssp();

    g_eventLogger->info("  Summing by owner");
    /* Use a linear hash to find items */

    Uint32 free = 0;
    Uint32 numOwners = 0;
    for (Uint32 i = 0; i < buffs; i++) {
      Uint32 owner = buffOwners[i];
      if (owner == 0) {
        free++;
      } else {
        Uint32 ownerHash = 17 + 37 * owner;
        Uint32 start = ownerHash % buffs;

        Uint32 y = 0;
        for (; y < buffs; y++) {
          Uint32 pos = (start + y) % buffs;
          if (buffOwnersCount[pos] == 0) {
            numOwners++;
            buffOwnersCount[pos] = (Uint64(owner) << 32 | 1);
            break;
          } else if ((buffOwnersCount[pos] >> 32) == owner) {
            buffOwnersCount[pos]++;
            break;
          }
        }
        ndbrequire(y != buffs);
      }
    }

    g_eventLogger->info("  Summary");
    g_eventLogger->info(
        "    Warning, free buffers in thread caches considered used here");
    g_eventLogger->info("    ndbd avoids this problem");
    g_eventLogger->info("    Zero owners : %u", zeroOwners);
    g_eventLogger->info("    Num free : %u", free);
    g_eventLogger->info("    Num owners : %u", numOwners);

    for (Uint32 i = 0; i < buffs; i++) {
      Uint64 entry = buffOwnersCount[i];
      if (entry != 0) {
        /* Breakdown assuming Block ref + GSN format */
        Uint32 count = Uint32(entry & 0xffffffff);
        Uint32 ownerId = Uint32(entry >> 32);
        Uint32 block = (ownerId >> 16) & 0x1ff;
        Uint32 instance = ownerId >> 25;
        Uint32 gsn = ownerId & 0xffff;
        g_eventLogger->info(
            "      Count : %u : OwnerId : 0x%x (0x%x:%u/%u) %s %s", count,
            ownerId, block, instance, gsn,
            block == 1 ? "RECV" : getBlockName(block, "Unknown"),
            getSignalName(gsn, "Unknown"));
      }
    }

    g_eventLogger->info("Done");

    ::free(buffOwners);
    ::free(buffOwnersCount);
#else
    g_eventLogger->info(
        "CMVMI :: ShowLongSignalOwnership.  Not compiled "
        "with NDB_DEBUG_RES_OWNERSHIP");
#endif
  }

  if (dumpState->args[0] == DumpStateOrd::DumpPageMemory) {
    const Uint32 len = signal->getLength();
    if (len == 1)  // DUMP 1000
    {
      // Start dumping resource limits
      signal->theData[1] = 0;
      signal->theData[2] = ~0;
      sendSignal(reference(), GSN_DUMP_STATE_ORD, signal, 3, JBB);

      // Dump data and index memory
      reportDMUsage(signal, 0);
      reportIMUsage(signal, 0);
      return;
    }

    if (len == 2)  // DUMP 1000 node-ref
    {
      // Dump data and index memory to specific ref
      Uint32 result_ref = signal->theData[1];
      /* Validate ref */
      {
        Uint32 node = refToNode(result_ref);
        if (node == 0 || node >= MAX_NODES) {
          g_eventLogger->info("Bad node in ref to DUMP %u : %u %u",
                              DumpStateOrd::DumpPageMemory, node, result_ref);
          return;
        }
      }
      reportDMUsage(signal, 0, result_ref);
      reportIMUsage(signal, 0, result_ref);
      return;
    }

    // DUMP 1000 0 0
    Uint32 id = signal->theData[1];
    if (id == 0) {
      infoEvent("Resource global total: %u used: %u",
                m_ctx.m_mm.get_allocated(), m_ctx.m_mm.get_in_use());
      infoEvent("Resource reserved total: %u used: %u",
                m_ctx.m_mm.get_reserved(), m_ctx.m_mm.get_reserved_in_use());
      infoEvent("Resource shared total: %u used: %u spare: %u",
                m_ctx.m_mm.get_shared(), m_ctx.m_mm.get_shared_in_use(),
                m_ctx.m_mm.get_spare());
      id++;
    }
    Resource_limit rl;
    for (; id <= RG_COUNT; id++) {
      if (!m_ctx.m_mm.get_resource_limit(id, rl)) {
        continue;
      }
      if (rl.m_min || rl.m_curr || rl.m_max || rl.m_spare) {
        infoEvent("Resource %u min: %u max: %u curr: %u spare: %u", id,
                  rl.m_min, rl.m_max, rl.m_curr, rl.m_spare);
      }
    }
    m_ctx.m_mm.dump(false);  // To data node log
    return;
  }
  if (dumpState->args[0] == DumpStateOrd::DumpPageMemoryOnFail) {
    const Uint32 len = signal->getLength();
    const bool dump_on_fail = (len >= 2) ? (signal->theData[1] != 0) : true;
    m_ctx.m_mm.dump_on_alloc_fail(dump_on_fail);
    return;
  }
  if (arg == DumpStateOrd::CmvmiSchedulerExecutionTimer) {
    Uint32 exec_time = signal->theData[1];
    globalEmulatorData.theConfiguration->schedulerExecutionTimer(exec_time);
  }
  if (arg == DumpStateOrd::CmvmiSchedulerSpinTimer) {
    Uint32 spin_time = signal->theData[1];
    globalEmulatorData.theConfiguration->schedulerSpinTimer(spin_time);
  }
  if (arg == DumpStateOrd::CmvmiRealtimeScheduler) {
    bool realtime_on = signal->theData[1];
    globalEmulatorData.theConfiguration->realtimeScheduler(realtime_on);
  }
  if (arg == DumpStateOrd::CmvmiExecuteLockCPU) {
  }
  if (arg == DumpStateOrd::CmvmiMaintLockCPU) {
  }
  if (arg == DumpStateOrd::CmvmiSetRestartOnErrorInsert) {
    if (signal->getLength() == 1) {
      Uint32 val = (Uint32)NRT_NoStart_Restart;
      const ndb_mgm_configuration_iterator *p =
          m_ctx.m_config.getOwnConfigIterator();
      ndbrequire(p != 0);

      if (!ndb_mgm_get_int_parameter(p, CFG_DB_STOP_ON_ERROR_INSERT, &val)) {
        m_ctx.m_config.setRestartOnErrorInsert(val);
      }
    } else {
      m_ctx.m_config.setRestartOnErrorInsert(signal->theData[1]);
    }
  }

  if (arg == DumpStateOrd::CmvmiTestLongSigWithDelay) {
    unsigned i;
    Uint32 testType = dumpState->args[1];
    Uint32 loopCount = dumpState->args[2];
    Uint32 print = dumpState->args[3];
    const unsigned len0 = 11;
    const unsigned len1 = 123;
    Uint32 sec0[len0];
    Uint32 sec1[len1];
    for (i = 0; i < len0; i++) sec0[i] = i;
    for (i = 0; i < len1; i++) sec1[i] = 16 * i;
    Uint32 *sig = signal->getDataPtrSend();
    sig[0] = reference();
    sig[1] = testType;
    sig[2] = 0;
    sig[3] = print;
    sig[4] = loopCount;
    sig[5] = len0;
    sig[6] = len1;
    sig[7] = 0;
    LinearSectionPtr ptr[3];
    ptr[0].p = sec0;
    ptr[0].sz = len0;
    ptr[1].p = sec1;
    ptr[1].sz = len1;
    sendSignal(reference(), GSN_TESTSIG, signal, 8, JBB, ptr, 2);
  }

  if (arg == DumpStateOrd::DumpEventLog) {
    /**
     * Array with m_saved_event_buffer indexes sorted by time and
     */
    Uint32 cnt = 0;
    Uint32 sorted[NDB_ARRAY_SIZE(m_saved_event_buffer)];

    /**
     * insert
     */
    for (Uint32 i = 0; i < NDB_ARRAY_SIZE(m_saved_event_buffer); i++) {
      if (m_saved_event_buffer[i].startScan()) continue;

      sorted[cnt] = i;
      cnt++;
    }

    /*
     * qsort
     */
    qsort(sorted, cnt, sizeof(Uint32), cmp_event_buf);

    Uint32 result_ref = signal->theData[1];
    SavedEvent s;
    EventReport *rep = CAST_PTR(EventReport, signal->getDataPtrSend());
    rep->setEventType(NDB_LE_SavedEvent);
    rep->setNodeId(getOwnNodeId());
    while (cnt > 0) {
      jam();

      bool done = m_saved_event_buffer[sorted[0]].scan(&s, 0);
      signal->theData[1] = s.m_len;
      signal->theData[2] = s.m_seq;
      signal->theData[3] = s.m_time;
      if (s.m_len <= 21) {
        jam();
        memcpy(signal->theData + 4, s.m_data, 4 * s.m_len);
        sendSignal(result_ref, GSN_EVENT_REP, signal, 4 + s.m_len, JBB);
      } else {
        jam();
        LinearSectionPtr ptr[3];
        ptr[0].p = s.m_data;
        ptr[0].sz = s.m_len;
        sendSignal(result_ref, GSN_EVENT_REP, signal, 4, JBB, ptr, 1);
      }

      if (done) {
        jam();
        memmove(sorted, sorted + 1, (cnt - 1) * sizeof(Uint32));
        cnt--;
      } else {
        jam();
        /**
         * sloppy...use qsort to re-sort
         */
        qsort(sorted, cnt, sizeof(Uint32), cmp_event_buf);
      }
    }
    signal->theData[1] = 0;  // end of stream
    sendSignal(result_ref, GSN_EVENT_REP, signal, 2, JBB);
    return;
  }

  if (arg == DumpStateOrd::CmvmiTestLongSig) {
    /* Forward as GSN_TESTSIG to self */
    Uint32 numArgs = signal->length() - 1;
    memmove(signal->getDataPtrSend(), signal->getDataPtrSend() + 1,
            numArgs << 2);
    sendSignal(reference(), GSN_TESTSIG, signal, numArgs, JBB);
  }

  if (arg == DumpStateOrd::CmvmiSetKillerWatchdog) {
    bool val = true;
    if (signal->length() >= 2) {
      val = (signal->theData[1] != 0);
    }
    globalEmulatorData.theWatchDog->setKillSwitch(val);
    return;
  }

  if (arg == DumpStateOrd::CmvmiSetWatchdogInterval) {
    Uint32 val = 6000;
    const ndb_mgm_configuration_iterator *p =
        m_ctx.m_config.getOwnConfigIterator();
    ndb_mgm_get_int_parameter(p, CFG_DB_WATCHDOG_INTERVAL, &val);

    if (signal->length() >= 2) {
      val = signal->theData[1];
    }
    g_eventLogger->info("Cmvmi : Setting watchdog interval to %u", val);
    update_watch_dog_timer(val);
  }

#ifdef ERROR_INSERT
  if (arg == DumpStateOrd::CmvmiSetErrorHandlingError) {
    Uint32 val = 0;
    if (signal->length() >= 2) {
      val = signal->theData[1];
    }
    g_eventLogger->info("Cmvmi : Setting ErrorHandlingError to %u", val);
    simulate_error_during_error_reporting = val;
  }
#endif

#ifdef VM_TRACE
#if 0
  {
    SafeCounterManager mgr(* this); mgr.setSize(1);
    SafeCounterHandle handle;

    {
      SafeCounter tmp(mgr, handle);
      tmp.init<RefSignalTest>(CMVMI, GSN_TESTSIG, /* senderData */ 13);
      tmp.setWaitingFor(3);
      ndbrequire(!tmp.done());
      g_eventLogger->info("Allocated");
    }
    ndbrequire(!handle.done());
    {
      SafeCounter tmp(mgr, handle);
      tmp.clearWaitingFor(3);
      ndbrequire(tmp.done());
      g_eventLogger->info("Deallocated");
    }
    ndbrequire(handle.done());
  }
#endif
#endif

  if (arg == 9999) {
    Uint32 delay = 1000;
    switch (signal->getLength()) {
      case 1:
        break;
      case 2:
        delay = signal->theData[1];
        break;
      default: {
        Uint32 dmin = signal->theData[1];
        Uint32 dmax = signal->theData[2];
        delay = dmin + (rand() % (dmax - dmin));
        break;
      }
    }
    signal->theData[0] = arg;
    if (delay == 0) {
      execNDB_TAMPER(signal);
    } else if (delay < 10) {
      sendSignal(reference(), GSN_NDB_TAMPER, signal, 1, JBB);
    } else {
      sendSignalWithDelay(reference(), GSN_NDB_TAMPER, signal, delay, 1);
    }
  }

  if (signal->theData[0] == 666) {
    jam();
    Uint32 mb = 100;
    if (signal->getLength() > 1) mb = signal->theData[1];

    Uint64 bytes = Uint64(mb) * 1024 * 1024;
    AllocMemReq *req = CAST_PTR(AllocMemReq, signal->getDataPtrSend());
    req->senderData = 666;
    req->senderRef = reference();
    req->requestInfo = AllocMemReq::RT_EXTEND;
    req->bytes_hi = Uint32(bytes >> 32);
    req->bytes_lo = Uint32(bytes);
    sendSignal(NDBFS_REF, GSN_ALLOC_MEM_REQ, signal, AllocMemReq::SignalLength,
               JBB);
  }

#ifdef ERROR_INSERT
  if (signal->theData[0] == 667) {
    jam();
    Uint32 numFiles = 100;
    if (signal->getLength() == 2) {
      jam();
      numFiles = signal->theData[1];
    }

    /* Send a number of concurrent file open requests
     * for 'bound' files to NdbFS to test that it
     * copes
     * None are closed before all are open
     */
    g_remaining_responses = numFiles;

    g_eventLogger->info("CMVMI : Bulk open %u files", numFiles);
    FsOpenReq *openReq = (FsOpenReq *)&signal->theData[0];
    openReq->userReference = reference();
    openReq->userPointer = 0;
    openReq->fileNumber[0] = ~Uint32(0);
    openReq->fileNumber[1] = ~Uint32(0);
    openReq->fileNumber[2] = 0;
    openReq->fileNumber[3] = ~Uint32(0);
    FsOpenReq::setVersion(openReq->fileNumber, FsOpenReq::V_BLOCK);
    FsOpenReq::setSuffix(openReq->fileNumber, FsOpenReq::S_FRAGLOG);
    openReq->fileFlags = FsOpenReq::OM_WRITEONLY | FsOpenReq::OM_CREATE |
                         FsOpenReq::OM_TRUNCATE |
                         FsOpenReq::OM_ZEROS_ARE_SPARSE;

    openReq->page_size = 0;
    openReq->file_size_hi = UINT32_MAX;
    openReq->file_size_lo = UINT32_MAX;
    openReq->auto_sync_size = 0;

    for (Uint32 i = 0; i < numFiles; i++) {
      jam();
      openReq->fileNumber[2] = i;
      sendSignal(NDBFS_REF, GSN_FSOPENREQ, signal, FsOpenReq::SignalLength,
                 JBB);
    }
    g_eventLogger->info("CMVMI : %u requests sent", numFiles);
  }

  if (signal->theData[0] == 668) {
    jam();

    g_eventLogger->info("CMVMI : missing responses %u", g_remaining_responses);
    /* Check that all files were opened */
    ndbrequire(g_remaining_responses == 0);
  }
#endif  // ERROR_INSERT

}  // Cmvmi::execDUMP_STATE_ORD()

#ifdef ERROR_INSERT
void Cmvmi::execFSOPENCONF(Signal *signal) {
  jam();
  if (signal->header.theSendersBlockRef != reference()) {
    jam();
    g_remaining_responses--;
    g_eventLogger->info("Waiting for %u responses", g_remaining_responses);
  }

  if (g_remaining_responses > 0) {
    // We don't close any files until all are open
    jam();
    g_eventLogger->info("CMVMI delaying CONF");
    sendSignalWithDelay(reference(), GSN_FSOPENCONF, signal, 300,
                        signal->getLength());
  } else {
    signal->theData[0] = signal->theData[1];
    signal->theData[1] = reference();
    signal->theData[2] = 0;
    signal->theData[3] = 1;  // Remove the file on close"
    signal->theData[4] = 0;
    sendSignal(NDBFS_REF, GSN_FSCLOSEREQ, signal, 5, JBB);
  }
}

void Cmvmi::execFSCLOSECONF(Signal *signal) { jam(); }

#endif  // ERROR_INSERT

void Cmvmi::execALLOC_MEM_REF(Signal *signal) {
  jamEntry();
  const AllocMemRef *ref = CAST_CONSTPTR(AllocMemRef, signal->getDataPtr());

  if (ref->senderData == 0) {
    jam();
    ndbabort();
  }
}

void Cmvmi::execALLOC_MEM_CONF(Signal *signal) {
  jamEntry();
  const AllocMemConf *conf = CAST_CONSTPTR(AllocMemConf, signal->getDataPtr());

  if (conf->senderData == 0) {
    jam();

    init_global_page_pool();

    ReadConfigConf *conf = (ReadConfigConf *)signal->getDataPtrSend();
    conf->senderRef = reference();
    conf->senderData = f_read_config_data;
    sendSignal(f_read_config_ref, GSN_READ_CONFIG_CONF, signal,
               ReadConfigConf::SignalLength, JBB);
    return;
  }
}

void Cmvmi::execDBINFO_SCANREQ(Signal *signal) {
  DbinfoScanReq req = *(DbinfoScanReq *)signal->theData;
  const Ndbinfo::ScanCursor *cursor =
      CAST_CONSTPTR(Ndbinfo::ScanCursor, DbinfoScan::getCursorPtr(&req));
  Ndbinfo::Ratelimit rl;

  jamEntry();

  switch (req.tableId) {
    case Ndbinfo::RESOURCES_TABLEID: {
      jam();
      Uint32 resource_id = cursor->data[0];
      Resource_limit resource_limit;

      if (resource_id == 0) {
        resource_id++;
      }
      while (m_ctx.m_mm.get_resource_limit(resource_id, resource_limit)) {
        jam();
        Ndbinfo::Row row(signal, req);
        row.write_uint32(getOwnNodeId());  // Node id
        row.write_uint32(resource_id);

        row.write_uint32(resource_limit.m_min);
        row.write_uint32(resource_limit.m_curr);
        row.write_uint32(resource_limit.m_max);
        row.write_uint32(0);  // TODO
        row.write_uint32(resource_limit.m_spare);
        ndbinfo_send_row(signal, req, row, rl);
        resource_id++;

        if (rl.need_break(req)) {
          jam();
          ndbinfo_send_scan_break(signal, req, rl, resource_id);
          return;
        }
      }
      break;
    }

    case Ndbinfo::NODES_TABLEID: {
      jam();
      const NodeState &nodeState = getNodeState();
      const Uint32 start_level = nodeState.startLevel;
      const NDB_TICKS now = NdbTick_getCurrentTicks();
      const Uint64 uptime = NdbTick_Elapsed(m_start_time, now).seconds();
      Uint32 generation = m_ctx.m_config.get_config_generation();

      Ndbinfo::Row row(signal, req);
      row.write_uint32(getOwnNodeId());  // Node id

      row.write_uint64(uptime);  // seconds
      row.write_uint32(start_level);
      row.write_uint32(start_level == NodeState::SL_STARTING
                           ? nodeState.starting.startPhase
                           : 0);
      row.write_uint32(generation);
      ndbinfo_send_row(signal, req, row, rl);
      break;
    }

    case Ndbinfo::POOLS_TABLEID: {
      jam();

      Resource_limit res_limit;
      m_ctx.m_mm.get_resource_limit(RG_DATAMEM, res_limit);

      const Uint32 dm_pages_used = res_limit.m_curr;
      const Uint32 dm_pages_total =
          res_limit.m_max > 0 ? res_limit.m_max : res_limit.m_min;

      Ndbinfo::pool_entry pools[] = {{"Data memory",
                                      dm_pages_used,
                                      dm_pages_total,
                                      sizeof(GlobalPage),
                                      0,
                                      {CFG_DB_DATA_MEM, 0, 0, 0},
                                      0},
                                     {"Long message buffer",
                                      g_sectionSegmentPool.getUsed(),
                                      g_sectionSegmentPool.getSize(),
                                      sizeof(SectionSegment),
                                      g_sectionSegmentPool.getUsedHi(),
                                      {CFG_DB_LONG_SIGNAL_BUFFER, 0, 0, 0},
                                      0},
                                     {NULL, 0, 0, 0, 0, {0, 0, 0, 0}, 0}};

      static const size_t num_config_params =
          sizeof(pools[0].config_params) / sizeof(pools[0].config_params[0]);
      const Uint32 numPools = NDB_ARRAY_SIZE(pools);
      Uint32 pool = cursor->data[0];
      ndbrequire(pool < numPools);
      BlockNumber bn = blockToMain(number());
      while (pools[pool].poolname) {
        jam();
        Ndbinfo::Row row(signal, req);
        row.write_uint32(getOwnNodeId());
        row.write_uint32(bn);          // block number
        row.write_uint32(instance());  // block instance
        row.write_string(pools[pool].poolname);

        row.write_uint64(pools[pool].used);
        row.write_uint64(pools[pool].total);
        row.write_uint64(pools[pool].used_hi);
        row.write_uint64(pools[pool].entry_size);
        for (size_t i = 0; i < num_config_params; i++)
          row.write_uint32(pools[pool].config_params[i]);
        row.write_uint32(GET_RG(pools[pool].record_type));
        row.write_uint32(GET_TID(pools[pool].record_type));
        ndbinfo_send_row(signal, req, row, rl);
        pool++;
        if (rl.need_break(req)) {
          jam();
          ndbinfo_send_scan_break(signal, req, rl, pool);
          return;
        }
      }
      break;
    }

    case Ndbinfo::CONFIG_VALUES_TABLEID: {
      jam();
      Uint32 index = cursor->data[0];

      char buf[512];
      const ConfigValues *const values = m_ctx.m_config.get_own_config_values();
      ConfigSection::Entry entry;
      while (true) {
        /*
          Iterate own configuration by index and
          return the configured values
        */
        index = values->getNextEntry(index, &entry);
        if (index == 0) {
          // No more config values
          break;
        }

        if (entry.m_key > PRIVATE_BASE) {
          // Skip private configuration values which are calculated
          // and only to be known within one data node
          index++;
          continue;
        }

        Ndbinfo::Row row(signal, req);
        row.write_uint32(getOwnNodeId());  // Node id
        row.write_uint32(entry.m_key);     // config_param

        switch (entry.m_type) {
          case ConfigSection::IntTypeId:
            BaseString::snprintf(buf, sizeof(buf), "%u", entry.m_int);
            break;

          case ConfigSection::Int64TypeId:
            BaseString::snprintf(buf, sizeof(buf), "%llu", entry.m_int64);
            break;

          case ConfigSection::StringTypeId:
            BaseString::snprintf(buf, sizeof(buf), "%s", entry.m_string);
            break;

          default:
            assert(false);
            break;
        }
        row.write_string(buf);  // config_values

        ndbinfo_send_row(signal, req, row, rl);

        if (rl.need_break(req)) {
          jam();
          ndbinfo_send_scan_break(signal, req, rl, index);
          return;
        }
      }
      break;
    }

    case Ndbinfo::CONFIG_NODES_TABLEID: {
      jam();
      ndb_mgm_configuration_iterator *iter =
          m_ctx.m_config.getClusterConfigIterator();
      Uint32 row_num, sent_row_num = cursor->data[0];

      for (row_num = 1, ndb_mgm_first(iter); ndb_mgm_valid(iter);
           row_num++, ndb_mgm_next(iter)) {
        if (row_num > sent_row_num) {
          Uint32 row_node_id, row_node_type;
          const char *hostname;
          Ndbinfo::Row row(signal, req);
          row.write_uint32(getOwnNodeId());
          ndb_mgm_get_int_parameter(iter, CFG_NODE_ID, &row_node_id);
          row.write_uint32(row_node_id);
          ndb_mgm_get_int_parameter(iter, CFG_TYPE_OF_SECTION, &row_node_type);
          row.write_uint32(row_node_type);
          ndb_mgm_get_string_parameter(iter, CFG_NODE_HOST, &hostname);
          row.write_string(hostname);
          ndbinfo_send_row(signal, req, row, rl);

          if (rl.need_break(req)) {
            jam();
            ndbinfo_send_scan_break(signal, req, rl, row_num);
            return;
          }
        }
      }
      break;
    }

    default:
      break;
  }

  ndbinfo_send_scan_conf(signal, req, rl);
}

BLOCK_FUNCTIONS(Cmvmi)

void Cmvmi::startFragmentedSend(Signal *signal, Uint32 variant, Uint32 numSigs,
                                NodeReceiverGroup rg) {
  Uint32 *sigData = signal->getDataPtrSend();
  const Uint32 sigLength = 6;
  const Uint32 sectionWords = 240;
  Uint32 sectionData[sectionWords];

  for (Uint32 i = 0; i < sectionWords; i++) sectionData[i] = i;

  const Uint32 secCount = 1;
  LinearSectionPtr ptr[3];
  ptr[0].sz = sectionWords;
  ptr[0].p = &sectionData[0];

  for (Uint32 i = 0; i < numSigs; i++) {
    sigData[0] = variant;
    sigData[1] = 31;
    sigData[2] = 0;
    sigData[3] = 1;  // print
    sigData[4] = 0;
    sigData[5] = sectionWords;

    if ((i & 1) == 0) {
      DEBUG("Starting linear fragmented send (" << i + 1 << "/" << numSigs
                                                << ")");

      /* Linear send */
      /* Todo : Avoid reading from invalid stackptr in CONTINUEB */
      sendFragmentedSignal(rg, GSN_TESTSIG, signal, sigLength, JBB, ptr,
                           secCount, TheEmptyCallback,
                           90);  // messageSize
    } else {
      /* Segmented send */
      DEBUG("Starting segmented fragmented send (" << i + 1 << "/" << numSigs
                                                   << ")");
      Ptr<SectionSegment> segPtr;
      ndbrequire(import(segPtr, sectionData, sectionWords));
      SectionHandle handle(this, segPtr.i);

      sendFragmentedSignal(rg, GSN_TESTSIG, signal, sigLength, JBB, &handle,
                           TheEmptyCallback,
                           90);  // messageSize
    }
  }
}

void Cmvmi::testNodeFailureCleanupCallback(Signal *signal, Uint32 data,
                                           Uint32 elementsCleaned) {
  DEBUG("testNodeFailureCleanupCallback");
  DEBUG("Data : " << data << " elementsCleaned : " << elementsCleaned);

  debugPrintFragmentCounts();

  Uint32 variant = data & 0xffff;
  Uint32 testType = (data >> 16) & 0xffff;

  DEBUG("Sending trigger(" << testType << ") variant " << variant
                           << " to self to cleanup any fragments that arrived "
                           << "before send was cancelled");

  Uint32 *sigData = signal->getDataPtrSend();
  sigData[0] = variant;
  sigData[1] = testType;
  sendSignal(reference(), GSN_TESTSIG, signal, 2, JBB);

  return;
}

void Cmvmi::testFragmentedCleanup(Signal *signal, SectionHandle *handle,
                                  Uint32 testType, Uint32 variant) {
  DEBUG("TestType " << testType << " variant " << variant);
  debugPrintFragmentCounts();

  /* Variants :
   *     Local fragmented send   Multicast fragmented send
   * 0 : Immediate cleanup       Immediate cleanup
   * 1 : Continued cleanup       Immediate cleanup
   * 2 : Immediate cleanup       Continued cleanup
   * 3 : Continued cleanup       Continued cleanup
   */
  const Uint32 NUM_VARIANTS = 4;
  if (variant >= NUM_VARIANTS) {
    DEBUG("Unsupported variant");
    releaseSections(*handle);
    return;
  }

  /* Test from ndb_mgm with
   * <node(s)> DUMP 2605 0 30
   *
   * Use
   * <node(s)> DUMP 2605 0 39 to get fragment resource usage counts
   * Use
   * <node(s)> DUMP 2601 to get segment usage counts in clusterlog
   */
  if (testType == 30) {
    /* Send the first fragment of a fragmented signal to self
     * Receiver will allocate assembly hash entries
     * which must be freed when node failure cleanup
     * executes later
     */
    const Uint32 sectionWords = 240;
    Uint32 sectionData[sectionWords];

    for (Uint32 i = 0; i < sectionWords; i++) sectionData[i] = i;

    const Uint32 secCount = 1;
    LinearSectionPtr ptr[3];
    ptr[0].sz = sectionWords;
    ptr[0].p = &sectionData[0];

    /* Send signal with testType == 31 */
    NodeReceiverGroup me(reference());
    Uint32 *sigData = signal->getDataPtrSend();
    const Uint32 sigLength = 6;
    const Uint32 numPartialSigs = 4;
    /* Not too many as CMVMI's fragInfo hash is limited size */
    // TODO : Consider making it debug-larger to get
    // more coverage on CONTINUEB path

    for (Uint32 i = 0; i < numPartialSigs; i++) {
      /* Fill in messy TESTSIG format */
      sigData[0] = variant;
      sigData[1] = 31;
      sigData[2] = 0;
      sigData[3] = 0;  // print
      sigData[4] = 0;
      sigData[5] = sectionWords;

      FragmentSendInfo fsi;

      DEBUG("Sending first fragment to self");
      sendFirstFragment(fsi, me, GSN_TESTSIG, signal, sigLength, JBB, ptr,
                        secCount,
                        90);  // FragmentLength

      DEBUG("Cancelling remainder to free internal section");
      fsi.m_status = FragmentSendInfo::SendCancelled;
      sendNextLinearFragment(signal, fsi);
    };

    /* Ok, now send short signal with testType == 32
     * to trigger 'remote-side' actions in middle of
     * multiple fragment assembly
     */
    sigData[0] = variant;
    sigData[1] = 32;

    DEBUG("Sending node fail trigger to self");
    sendSignal(me, GSN_TESTSIG, signal, 2, JBB);
    return;
  }

  if (testType == 31) {
    /* Just release sections - execTESTSIG() has shown sections received */
    releaseSections(*handle);
    return;
  }

  if (testType == 32) {
    /* 'Remote side' trigger to clean up fragmented signal resources */
    BlockReference senderRef = signal->getSendersBlockRef();
    Uint32 sendingNode = refToNode(senderRef);

    /* Start sending some linear and fragmented responses to the
     * sender, to exercise frag-send cleanup code when we execute
     * node-failure later
     */
    DEBUG("Starting fragmented send using continueB back to self");

    NodeReceiverGroup sender(senderRef);
    startFragmentedSend(signal, variant, 6, sender);

    debugPrintFragmentCounts();

    Uint32 cbData = (((Uint32)33) << 16) | variant;
    Callback cb = {safe_cast(&Cmvmi::testNodeFailureCleanupCallback), cbData};

    Callback *cbPtr = NULL;

    bool passCallback = variant & 1;

    if (passCallback) {
      DEBUG("Running simBlock failure code WITH CALLBACK for node "
            << sendingNode);
      cbPtr = &cb;
    } else {
      DEBUG("Running simBlock failure code IMMEDIATELY (no callback) for node "
            << sendingNode);
      cbPtr = &TheEmptyCallback;
    }

    Uint32 elementsCleaned = simBlockNodeFailure(signal, sendingNode, *cbPtr);

    DEBUG("Elements cleaned by call : " << elementsCleaned);

    debugPrintFragmentCounts();

    if (!passCallback) {
      DEBUG("Variant " << variant << " manually executing callback");
      /* We call the callback inline here to continue processing */
      testNodeFailureCleanupCallback(signal, cbData, elementsCleaned);
    }

    return;
  }

  if (testType == 33) {
    /* Original side - receive cleanup trigger from 'remote' side
     * after node failure cleanup performed there.  We may have
     * fragments it managed to send before the cleanup completed
     * so we'll get rid of them.
     * This would not be necessary in reality as this node would
     * be failed
     */
    Uint32 sendingNode = refToNode(signal->getSendersBlockRef());
    DEBUG("Running simBlock failure code for node " << sendingNode);

    Uint32 elementsCleaned = simBlockNodeFailure(signal, sendingNode);

    DEBUG("Elements cleaned : " << elementsCleaned);

    /* Should have no fragment resources in use now */
    ndbrequire(debugPrintFragmentCounts() == 0);

    /* Now use ReceiverGroup to multicast a fragmented signal to
     * all database nodes
     */
    DEBUG("Starting to send fragmented continueB to all nodes inc. self : ");
    NodeReceiverGroup allNodes(CMVMI, c_dbNodes);

    unsigned nodeId = 0;
    while ((nodeId = c_dbNodes.find(nodeId + 1)) != BitmaskImpl::NotFound) {
      DEBUG("Node " << nodeId);
    }

    startFragmentedSend(signal, variant, 8, allNodes);

    debugPrintFragmentCounts();

    Uint32 cbData = (((Uint32)34) << 16) | variant;
    Callback cb = {safe_cast(&Cmvmi::testNodeFailureCleanupCallback), cbData};

    Callback *cbPtr = NULL;

    bool passCallback = variant & 2;

    if (passCallback) {
      DEBUG("Running simBlock failure code for self WITH CALLBACK ("
            << getOwnNodeId() << ")");
      cbPtr = &cb;
    } else {
      DEBUG("Running simBlock failure code for self IMMEDIATELY (no callback) ("
            << getOwnNodeId() << ")");
      cbPtr = &TheEmptyCallback;
    }

    /* Fragmented signals being sent will have this node removed
     * from their receiver group, but will keep sending to the
     * other node(s).
     * Other node(s) should therefore receive the complete signals.
     * We will then receive only the first fragment of each of
     * the signals which must be removed later.
     */
    elementsCleaned = simBlockNodeFailure(signal, getOwnNodeId(), *cbPtr);

    DEBUG("Elements cleaned : " << elementsCleaned);

    debugPrintFragmentCounts();

    /* Callback will send a signal to self to clean up fragments that
     * were sent to self before the send was cancelled.
     * (Again, unnecessary in a 'real' situation
     */
    if (!passCallback) {
      DEBUG("Variant " << variant << " manually executing callback");

      testNodeFailureCleanupCallback(signal, cbData, elementsCleaned);
    }

    return;
  }

  if (testType == 34) {
    /* Cleanup fragments which were sent before send was cancelled. */
    Uint32 elementsCleaned = simBlockNodeFailure(signal, getOwnNodeId());

    DEBUG("Elements cleaned " << elementsCleaned);

    /* All FragInfo should be clear, may still be sending some
     * to other node(s)
     */
    debugPrintFragmentCounts();

    DEBUG("Variant " << variant << " completed.");

    if (++variant < NUM_VARIANTS) {
      DEBUG("Re-executing with variant " << variant);
      Uint32 *sigData = signal->getDataPtrSend();
      sigData[0] = variant;
      sigData[1] = 30;
      sendSignal(reference(), GSN_TESTSIG, signal, 2, JBB);
    }
    //    else
    //    {
    //      // Infinite loop to test for leaks
    //       DEBUG("Back to zero");
    //       Uint32* sigData = signal->getDataPtrSend();
    //       sigData[0] = 0;
    //       sigData[1] = 30;
    //       sendSignal(reference(), GSN_TESTSIG, signal, 2, JBB);
    //    }
  }
}

static Uint32 g_print;
static LinearSectionPtr g_test[3];

/* See above for how to generate TESTSIG using DUMP 2603
 * (e.g. : <All/NodeId> DUMP 2603 <TestId> <LoopCount> <Print>
 *   LoopCount : How many times test should loop (0-n)
 *   Print : Whether signals should be printed : 0=no 1=yes
 *
 * TestIds
 *   20 : Test sendDelayed with 1 milli delay, LoopCount times
 *   1-16 : See vm/testLongSig.cpp
 */
void Cmvmi::execTESTSIG(Signal *signal) {
  Uint32 i;
  /**
   * Test of SafeCounter
   */
  jamEntry();

  if (!assembleFragments(signal)) {
    jam();
    return;
  }

  Uint32 ref = signal->theData[0];
  Uint32 testType = signal->theData[1];
  Uint32 fragmentLength = signal->theData[2];
  g_print = signal->theData[3];
  //  Uint32 returnCount = signal->theData[4];
  Uint32 *secSizes = &signal->theData[5];

  SectionHandle handle(this, signal);

  if (g_print) {
    SignalLoggerManager::printSignalHeader(stdout, signal->header, 0,
                                           getOwnNodeId(), true);
    ndbout_c("-- Fixed section --");
    for (i = 0; i < signal->length(); i++) {
      fprintf(stdout, "H'0x%.8x ", signal->theData[i]);
      if (((i + 1) % 6) == 0) fprintf(stdout, "\n");
    }
    fprintf(stdout, "\n");

    for (i = 0; i < handle.m_cnt; i++) {
      SegmentedSectionPtr ptr(0, 0, 0);
      ndbout_c("-- Section %d --", i);
      ndbrequire(handle.getSection(ptr, i));
      ndbrequire(ptr.p != 0);
      print(ptr, stdout);
      ndbrequire(ptr.sz == secSizes[i]);
    }
  }

  /**
   * Validate length:s
   */
  for (i = 0; i < handle.m_cnt; i++) {
    SegmentedSectionPtr ptr;
    ndbrequire(handle.getSection(ptr, i));
    ndbrequire(ptr.p != 0);
    ndbrequire(ptr.sz == secSizes[i]);
  }

  /**
   * Testing send with delay.
   */
  if (testType == 20) {
    if (signal->theData[4] == 0) {
      releaseSections(handle);
      return;
    }
    signal->theData[4]--;
    sendSignalWithDelay(reference(), GSN_TESTSIG, signal, 100, 8, &handle);
    return;
  }

  if (g_print)
    ndbout_c("TestType=%u signal->theData[4]=%u, sendersBlockRef=%u ref=%u\n",
             testType, signal->theData[4], signal->getSendersBlockRef(), ref);

  NodeReceiverGroup rg(CMVMI, c_dbNodes);

  /**
   * Testing SimulatedBlock fragment assembly cleanup
   */
  if ((testType >= 30) && (testType < 40)) {
    testFragmentedCleanup(signal, &handle, testType, ref);
    return;
  }

  /**
   * Testing Api fragmented signal send/receive
   */
  if (testType == 40) {
    /* Fragmented signal sent from Api, we'll check it and return it */
    Uint32 expectedVal = 0;
    for (Uint32 s = 0; s < handle.m_cnt; s++) {
      SectionReader sr(handle.m_ptr[s].i, getSectionSegmentPool());
      Uint32 received;
      while (sr.getWord(&received)) {
        ndbrequire(received == expectedVal++);
      }
    }

    /* Now return it back to the Api, no callback, so framework
     * can time-slice the send
     */
    sendFragmentedSignal(ref, GSN_TESTSIG, signal, signal->length(), JBB,
                         &handle);

    return;
  }

  if (signal->getSendersBlockRef() == ref) {
    /**
     * Signal from API (not via NodeReceiverGroup)
     */
    if ((testType % 2) == 1) {
      signal->theData[4] = 1;  // No further signals after this
    } else {
      // Change testType to UniCast, and set loopCount to the
      // number of nodes.
      signal->theData[1]--;
      signal->theData[4] = rg.m_nodes.count();
    }
  }

  switch (testType) {
    case 1:
      /* Unicast to self */
      sendSignal(ref, GSN_TESTSIG, signal, signal->length(), JBB, &handle);
      break;
    case 2:
      /* Multicast to all nodes */
      sendSignal(rg, GSN_TESTSIG, signal, signal->length(), JBB, &handle);
      break;
    case 3:
    case 4: {
      LinearSectionPtr ptr[3];
      const Uint32 secs = handle.m_cnt;
      for (i = 0; i < secs; i++) {
        SegmentedSectionPtr sptr(0, 0, 0);
        ndbrequire(handle.getSection(sptr, i));
        Uint32 *p = new Uint32[sptr.sz];
        copy(p, sptr);
        ptr[i].p = p;
        ptr[i].sz = sptr.sz;
      }

      if (testType == 3) {
        /* Unicast linear sections to self */
        sendSignal(ref, GSN_TESTSIG, signal, signal->length(), JBB, ptr, secs);
      } else {
        /* Broadcast linear sections to all nodes */
        sendSignal(rg, GSN_TESTSIG, signal, signal->length(), JBB, ptr, secs);
      }
      for (Uint32 i = 0; i < secs; i++) {
        delete[] ptr[i].p;
      }
      releaseSections(handle);
      break;
    }
    /* Send fragmented segmented sections direct send */
    case 5:
    case 6: {
      NodeReceiverGroup tmp;
      if (testType == 5) {
        /* Unicast */
        tmp = ref;
      } else {
        /* Multicast */
        tmp = rg;
      }

      FragmentSendInfo fragSend;
      sendFirstFragment(fragSend, tmp, GSN_TESTSIG, signal, signal->length(),
                        JBB, &handle,
                        false,  // Release sections on send
                        fragmentLength);

      int count = 1;
      while (fragSend.m_status != FragmentSendInfo::SendComplete) {
        count++;
        if (g_print) ndbout_c("Sending fragment %d", count);
        sendNextSegmentedFragment(signal, fragSend);
      }
      break;
    }
    /* Send fragmented linear sections direct send */
    case 7:
    case 8: {
      LinearSectionPtr ptr[3];
      const Uint32 secs = handle.m_cnt;
      for (i = 0; i < secs; i++) {
        SegmentedSectionPtr sptr(0, 0, 0);
        ndbrequire(handle.getSection(sptr, i));
        Uint32 *p = new Uint32[sptr.sz];
        copy(p, sptr);
        ptr[i].p = p;
        ptr[i].sz = sptr.sz;
      }

      NodeReceiverGroup tmp;
      if (testType == 7) {
        /* Unicast */
        tmp = ref;
      } else {
        /* Multicast */
        tmp = rg;
      }

      FragmentSendInfo fragSend;
      sendFirstFragment(fragSend, tmp, GSN_TESTSIG, signal, signal->length(),
                        JBB, ptr, secs, fragmentLength);

      int count = 1;
      while (fragSend.m_status != FragmentSendInfo::SendComplete) {
        count++;
        if (g_print) ndbout_c("Sending fragment %d", count);
        sendNextLinearFragment(signal, fragSend);
      }

      for (i = 0; i < secs; i++) {
        delete[] ptr[i].p;
      }
      releaseSections(handle);
      break;
    }
    /* Test fragmented segmented send with callback */
    case 9:
    case 10: {
      Callback m_callBack;
      m_callBack.m_callbackFunction = safe_cast(&Cmvmi::sendFragmentedComplete);

      if (testType == 9) {
        /* Unicast */
        m_callBack.m_callbackData = 9;
        sendFragmentedSignal(ref, GSN_TESTSIG, signal, signal->length(), JBB,
                             &handle, m_callBack, fragmentLength);
      } else {
        /* Multicast */
        m_callBack.m_callbackData = 10;
        sendFragmentedSignal(rg, GSN_TESTSIG, signal, signal->length(), JBB,
                             &handle, m_callBack, fragmentLength);
      }
      break;
    }
    /* Test fragmented linear send with callback */
    case 11:
    case 12: {
      const Uint32 secs = handle.m_cnt;
      std::memset(g_test, 0, sizeof(g_test));
      for (i = 0; i < secs; i++) {
        SegmentedSectionPtr sptr(0, 0, 0);
        ndbrequire(handle.getSection(sptr, i));
        Uint32 *p = new Uint32[sptr.sz];
        copy(p, sptr);
        g_test[i].p = p;
        g_test[i].sz = sptr.sz;
      }

      releaseSections(handle);

      Callback m_callBack;
      m_callBack.m_callbackFunction = safe_cast(&Cmvmi::sendFragmentedComplete);

      if (testType == 11) {
        /* Unicast */
        m_callBack.m_callbackData = 11;
        sendFragmentedSignal(ref, GSN_TESTSIG, signal, signal->length(), JBB,
                             g_test, secs, m_callBack, fragmentLength);
      } else {
        /* Multicast */
        m_callBack.m_callbackData = 12;
        sendFragmentedSignal(rg, GSN_TESTSIG, signal, signal->length(), JBB,
                             g_test, secs, m_callBack, fragmentLength);
      }
      break;
    }
    /* Send fragmented segmented sections direct send no-release */
    case 13:
    case 14: {
      NodeReceiverGroup tmp;
      if (testType == 13) {
        /* Unicast */
        tmp = ref;
      } else {
        /* Multicast */
        tmp = rg;
      }

      FragmentSendInfo fragSend;
      sendFirstFragment(fragSend, tmp, GSN_TESTSIG, signal, signal->length(),
                        JBB, &handle,
                        true,  // Don't release sections
                        fragmentLength);

      int count = 1;
      while (fragSend.m_status != FragmentSendInfo::SendComplete) {
        count++;
        if (g_print) ndbout_c("Sending fragment %d", count);
        sendNextSegmentedFragment(signal, fragSend);
      }

      if (g_print)
        ndbout_c("Free sections : %u\n", g_sectionSegmentPool.getNoOfFree());
      releaseSections(handle);
      // handle.clear(); // Use instead of releaseSections to Leak sections
      break;
    }
    /* Loop decrementing signal->theData[9] */
    case 15: {
      releaseSections(handle);
      ndbrequire(signal->getNoOfSections() == 0);
      Uint32 loop = signal->theData[9];
      if (loop > 0) {
        signal->theData[9]--;
        sendSignal(CMVMI_REF, GSN_TESTSIG, signal, signal->length(), JBB);
        return;
      }
      sendSignal(ref, GSN_TESTSIG, signal, signal->length(), JBB);
      return;
    }
    case 16: {
      releaseSections(handle);
      Uint32 count = signal->theData[8];
      signal->theData[10] = count * rg.m_nodes.count();
      for (i = 0; i < count; i++) {
        sendSignal(rg, GSN_TESTSIG, signal, signal->length(), JBB);
      }
      return;
    }

    default:
      ndbabort();
  }
  return;
}

void Cmvmi::sendFragmentedComplete(Signal *signal, Uint32 data,
                                   Uint32 returnCode) {
  if (g_print) ndbout_c("sendFragmentedComplete: %d", data);
  if (data == 11 || data == 12) {
    for (Uint32 i = 0; i < 3; i++) {
      if (g_test[i].p != 0) delete[] g_test[i].p;
    }
  }
}

static Uint32 calc_percent(Uint32 used, Uint32 total) {
  return (total ? (used * 100) / total : 0);
}

static Uint32 sum_array(const Uint32 array[], unsigned sz) {
  Uint32 sum = 0;
  for (unsigned i = 0; i < sz; i++) sum += array[i];
  return sum;
}

/*
  Check if any of the given thresholds has been
  passed since last

  Returns:
   -1       no threshold passed
   0 - 100  threshold passed
*/

static int check_threshold(Uint32 last, Uint32 now) {
  assert(last <= 100 && now <= 100);

  static const Uint32 thresholds[] = {100, 99, 90, 80, 0};

  Uint32 passed = 0; /* Initialised to silence compiler warning */
  for (size_t i = 0; i < NDB_ARRAY_SIZE(thresholds); i++) {
    if (now >= thresholds[i]) {
      passed = thresholds[i];
      break;
    }
  }
  assert(passed <= 100);

  if (passed == last) return -1;  // Already reported this level

  return passed;
}

void Cmvmi::execCONTINUEB(Signal *signal) {
  switch (signal->theData[0]) {
    case ZREPORT_MEMORY_USAGE: {
      jam();
      Uint32 cnt = signal->theData[1];
      Uint32 dm_percent_last = signal->theData[2];
      Uint32 tup_percent_last = signal->theData[3];
      Uint32 acc_percent_last = signal->theData[4];

      // Data memory threshold
      Resource_limit rl;
      m_ctx.m_mm.get_resource_limit(RG_DATAMEM, rl);
      {
        const Uint32 dm_pages_used = rl.m_curr;
        const Uint32 dm_pages_total =
            (rl.m_max < Resource_limit::HIGHEST_LIMIT) ? rl.m_max : rl.m_min;
        const Uint32 dm_percent_now =
            calc_percent(dm_pages_used, dm_pages_total);

        const Uint32 acc_pages_used =
            sum_array(g_acc_pages_used, NDB_ARRAY_SIZE(g_acc_pages_used));

        const Uint32 tup_pages_used = dm_pages_used - acc_pages_used;

        /**
         * If for example both acc and tup uses 50% each of data memory
         * we want it to show 100% usage so that thresholds warning
         * starting at 80% trigger.
         *
         * Therefore acc and tup percentage are calculated against free
         * data memory plus its own usage.
         */
        const Uint32 acc_pages_total = dm_pages_total - tup_pages_used;
        const Uint32 acc_percent_now =
            calc_percent(acc_pages_used, acc_pages_total);

        const Uint32 tup_pages_total = dm_pages_total - acc_pages_used;
        const Uint32 tup_percent_now =
            calc_percent(tup_pages_used, tup_pages_total);

        int passed;
        if ((passed = check_threshold(tup_percent_last, tup_percent_now)) !=
            -1) {
          jam();
          reportDMUsage(signal, tup_percent_now >= tup_percent_last ? 1 : -1);
          tup_percent_last = passed;
        }
        if ((passed = check_threshold(acc_percent_last, acc_percent_now)) !=
            -1) {
          jam();
          reportIMUsage(signal, acc_percent_now >= acc_percent_last ? 1 : -1);
          acc_percent_last = passed;
        }
        if ((passed = check_threshold(dm_percent_last, dm_percent_now)) != -1) {
          jam();
          /* no separate report, see dbtup and dbacc report above */
          dm_percent_last = passed;
        }
      }

      // Index and data memory report frequency
      if (c_memusage_report_frequency &&
          cnt + 1 == c_memusage_report_frequency) {
        jam();
        reportDMUsage(signal, 0);
        reportIMUsage(signal, 0);
        cnt = 0;
      } else {
        jam();
        cnt++;
      }
      signal->theData[0] = ZREPORT_MEMORY_USAGE;
      signal->theData[1] = cnt;  // seconds since last report
      signal->theData[2] =
          dm_percent_last;  // last reported threshold for data memory
      signal->theData[3] = tup_percent_last;  // last reported threshold for TUP
      signal->theData[4] = acc_percent_last;  // last reported threshold for ACC
      sendSignalWithDelay(reference(), GSN_CONTINUEB, signal, 1000, 5);
      return;
    }
  }
}

void Cmvmi::reportDMUsage(Signal *signal, int incDec, BlockReference ref) {
  Resource_limit rl;
  m_ctx.m_mm.get_resource_limit(RG_DATAMEM, rl);

  const Uint32 dm_pages_used = rl.m_curr;
  const Uint32 dm_pages_total =
      (rl.m_max < Resource_limit::HIGHEST_LIMIT) ? rl.m_max : rl.m_min;

  const Uint32 acc_pages_used =
      sum_array(g_acc_pages_used, NDB_ARRAY_SIZE(g_acc_pages_used));

  const Uint32 tup_pages_used = dm_pages_used - acc_pages_used;

  const Uint32 tup_pages_total = dm_pages_total - acc_pages_used;

  signal->theData[0] = NDB_LE_MemoryUsage;
  signal->theData[1] = incDec;
  signal->theData[2] = sizeof(GlobalPage);
  signal->theData[3] = tup_pages_used;
  signal->theData[4] = tup_pages_total;
  signal->theData[5] = DBTUP;
  sendSignal(ref, GSN_EVENT_REP, signal, 6, JBB);
}

void Cmvmi::reportIMUsage(Signal *signal, int incDec, BlockReference ref) {
  Resource_limit rl;
  m_ctx.m_mm.get_resource_limit(RG_DATAMEM, rl);

  const Uint32 dm_pages_used = rl.m_curr;
  const Uint32 dm_pages_total =
      (rl.m_max < Resource_limit::HIGHEST_LIMIT) ? rl.m_max : rl.m_min;

  const Uint32 acc_pages_used =
      sum_array(g_acc_pages_used, NDB_ARRAY_SIZE(g_acc_pages_used));

  const Uint32 tup_pages_used = dm_pages_used - acc_pages_used;

  const Uint32 acc_pages_total = dm_pages_total - tup_pages_used;

  signal->theData[0] = NDB_LE_MemoryUsage;
  signal->theData[1] = incDec;
  signal->theData[2] = sizeof(GlobalPage);
  signal->theData[3] = acc_pages_used;
  signal->theData[4] = acc_pages_total;
  signal->theData[5] = DBACC;
  sendSignal(ref, GSN_EVENT_REP, signal, 6, JBB);
}

void Cmvmi::execGET_CONFIG_REQ(Signal *signal) {
  jamEntry();
  const GetConfigReq *const req = (const GetConfigReq *)signal->getDataPtr();

  Uint32 error = 0;
  Uint32 retRef = req->senderRef;  // mgm servers ref

  if (retRef != signal->header.theSendersBlockRef) {
    error = GetConfigRef::WrongSender;
  }

  if (req->nodeId != getOwnNodeId()) {
    error = GetConfigRef::WrongNodeId;
  }
  Uint32 mgm_nodeid = refToNode(retRef);

  const Uint32 version = getNodeInfo(mgm_nodeid).m_version;

  bool v2 = ndb_config_version_v2(version);

  const Uint32 config_length =
      v2 ? m_ctx.m_config.m_clusterConfigPacked_v2.length()
         : m_ctx.m_config.m_clusterConfigPacked_v1.length();
  if (config_length == 0) {
    error = GetConfigRef::NoConfig;
  }

  if (error) {
    warningEvent("execGET_CONFIG_REQ: failed %u", error);
    GetConfigRef *ref = (GetConfigRef *)signal->getDataPtrSend();
    ref->error = error;
    sendSignal(retRef, GSN_GET_CONFIG_REF, signal, GetConfigRef::SignalLength,
               JBB);
    return;
  }

  const Uint32 nSections = 1;
  LinearSectionPtr ptr[3];
  ptr[0].p =
      v2 ? (Uint32 *)(m_ctx.m_config.m_clusterConfigPacked_v2.get_data())
         : (Uint32 *)(m_ctx.m_config.m_clusterConfigPacked_v1.get_data());
  ptr[0].sz = (config_length + 3) / 4;

  GetConfigConf *conf = (GetConfigConf *)signal->getDataPtrSend();

  conf->configLength = config_length;

  sendFragmentedSignal(retRef, GSN_GET_CONFIG_CONF, signal,
                       GetConfigConf::SignalLength, JBB, ptr, nSections,
                       TheEmptyCallback);
}
